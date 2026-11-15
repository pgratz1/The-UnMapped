#pragma once

// Shared helpers for the world database tests (spec P0-02 section 12).
//
// Every test that writes a file does so under a fresh directory inside the
// build tree (UNMAPPED_DB_TEST_TMP_ROOT, set by worldgen/tests/CMakeLists.txt),
// never in the repository or the current directory, and removes it again.
// File bytes are read by these helpers directly, independently of the
// reader under test; SQLite is used directly (read-only) to inspect files.

#include <sqlite3.h>

#include <cstddef>
#include <ostream>  // doctest needs the full type to print strings with the MSVC STL
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include "worldgen/core/sha256.hpp"
#include "worldgen/db/utf8_path.hpp"
#include "worldgen/db/writer.hpp"

#ifndef UNMAPPED_DB_TEST_TMP_ROOT
#error "UNMAPPED_DB_TEST_TMP_ROOT must be defined by the build (worldgen/tests/CMakeLists.txt)"
#endif

namespace dbtest {

namespace fs = std::filesystem;

inline long process_id() {
#if defined(_WIN32)
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

// A fresh, empty directory <build tree>/db-test-tmp/<label>-<pid>, removed
// with its contents on destruction. The label is unique per test, the pid
// separates concurrent runs of the test binary.
class TempDir {
public:
    explicit TempDir(std::string_view label) {
        const fs::path root = worldgen::db::path_from_utf8(UNMAPPED_DB_TEST_TMP_ROOT);
        path_ = root / worldgen::db::path_from_utf8(std::string(label) + "-" + std::to_string(process_id()));
        std::error_code ec;
        fs::remove_all(path_, ec);
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }
    // UTF-8 path of a file inside the directory; name is UTF-8.
    [[nodiscard]] std::string file(std::string_view name) const {
        return worldgen::db::path_to_utf8(path_ / worldgen::db::path_from_utf8(name));
    }

private:
    fs::path path_;
};

// Changes the current directory for the lifetime of the object.
class CurrentDir {
public:
    explicit CurrentDir(const fs::path& dir) : saved_(fs::current_path()) { fs::current_path(dir); }
    ~CurrentDir() {
        std::error_code ec;
        fs::current_path(saved_, ec);
    }
    CurrentDir(const CurrentDir&) = delete;
    CurrentDir& operator=(const CurrentDir&) = delete;

private:
    fs::path saved_;
};

inline std::vector<std::uint8_t> read_file(const std::string& path_utf8) {
    std::ifstream in(worldgen::db::path_from_utf8(path_utf8), std::ios::binary);
    std::vector<std::uint8_t> bytes;
    if (!in) {
        return bytes;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(size));
    if (size > 0) {
        in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
    }
    return bytes;
}

inline void write_file(const std::string& path_utf8, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(worldgen::db::path_from_utf8(path_utf8), std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

inline bool exists(const std::string& path_utf8) {
    std::error_code ec;
    return fs::exists(worldgen::db::path_from_utf8(path_utf8), ec);
}

// Names (UTF-8) of every entry in a directory, sorted.
inline std::set<std::string> dir_entries(const fs::path& dir) {
    std::set<std::string> names;
    for (const auto& entry : fs::directory_iterator(dir)) {
        names.insert(worldgen::db::path_to_utf8(entry.path().filename()));
    }
    return names;
}

// Big-endian fields of the SQLite header (P0-02 6.1).
inline std::uint32_t be16(const std::vector<std::uint8_t>& b, std::size_t off) {
    return (static_cast<std::uint32_t>(b.at(off)) << 8U) | static_cast<std::uint32_t>(b.at(off + 1));
}
inline std::uint32_t be32(const std::vector<std::uint8_t>& b, std::size_t off) {
    return (static_cast<std::uint32_t>(b.at(off)) << 24U) | (static_cast<std::uint32_t>(b.at(off + 1)) << 16U) |
           (static_cast<std::uint32_t>(b.at(off + 2)) << 8U) | static_cast<std::uint32_t>(b.at(off + 3));
}

// SHA-256 hex of a file's bytes.
inline std::string file_sha256(const std::string& path_utf8) {
    return worldgen::sha256_hex(read_file(path_utf8));
}

// The digest of the empty encoding: content_hash of a P0 world (P0-02 8).
inline worldgen::Sha256Digest empty_content_hash() {
    return worldgen::sha256(std::string_view{});
}

// Sets the values every world needs, as `worldgen-cli generate` does for an
// empty world (P0-02 8): numeric seed, empty content hash, map size 1024.
inline void set_standard_meta(worldgen::db::WorldWriter& writer, std::uint64_t seed) {
    writer.set_seed(seed);
    writer.set_content_hash(empty_content_hash());
    writer.set_param_integer("map_size", 1024);
}

// Writes an empty world for a numeric seed; REQUIREs nothing, returns status.
inline worldgen::db::Status write_empty_world(const std::string& out_utf8, std::uint64_t seed) {
    worldgen::db::WorldWriter writer;
    set_standard_meta(writer, seed);
    return writer.finalize(out_utf8, worldgen::db::Overwrite::refuse);
}

// A read-only SQLite connection for inspecting a finished file
// independently of WorldReader.
class SqliteFile {
public:
    explicit SqliteFile(const std::string& path_utf8) {
        if (sqlite3_open_v2(path_utf8.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }
    ~SqliteFile() { sqlite3_close(db_); }
    SqliteFile(const SqliteFile&) = delete;
    SqliteFile& operator=(const SqliteFile&) = delete;

    [[nodiscard]] bool ok() const noexcept { return db_ != nullptr; }

    // Every row of a query, each column as text (NULL as "<null>"). For
    // typeof() queries and text columns.
    [[nodiscard]] std::vector<std::vector<std::string>> rows(std::string_view sql) const {
        std::vector<std::vector<std::string>> out;
        sqlite3_stmt* stmt = nullptr;
        if (db_ == nullptr ||
            sqlite3_prepare_v2(db_, sql.data(), static_cast<int>(sql.size()), &stmt, nullptr) != SQLITE_OK) {
            sqlite3_finalize(stmt);
            out.push_back({"<prepare failed>"});
            return out;
        }
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::vector<std::string> row;
            const int n = sqlite3_column_count(stmt);
            for (int i = 0; i < n; ++i) {
                if (sqlite3_column_type(stmt, i) == SQLITE_NULL) {
                    row.emplace_back("<null>");
                } else {
                    const auto* text = sqlite3_column_text(stmt, i);
                    const int len = sqlite3_column_bytes(stmt, i);
                    row.emplace_back(reinterpret_cast<const char*>(text), static_cast<std::size_t>(len));
                }
            }
            out.push_back(std::move(row));
        }
        sqlite3_finalize(stmt);
        return out;
    }

private:
    sqlite3* db_ = nullptr;
};

}  // namespace dbtest
