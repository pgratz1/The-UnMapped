#include "db/file_header.hpp"

#include <sqlite3.h>

#include <fstream>
#include <system_error>

#include "worldgen/db/schema.hpp"

// The SQLite version is written into every file header (offset 96) and
// fixes the B-tree layout, so it is part of the output (P0-02 4.1). Changing
// the vendored SQLite means a generator version bump, new golden hashes and
// a proposals entry; update this assertion only together with those.
static_assert(SQLITE_VERSION_NUMBER == 3054001, "vendored SQLite must be 3.54.1 (P0-02 4.1, version pinning)");

namespace worldgen::db::detail {

namespace {

constexpr char SQLITE_MAGIC[16] = {'S', 'Q', 'L', 'i', 't', 'e', ' ', 'f',
                                   'o', 'r', 'm', 'a', 't', ' ', '3', '\0'};

std::string field_message(const char* field, std::uint64_t got, std::uint64_t expected) {
    std::string m = "header field ";
    m += field;
    m += " is ";
    m += format_u64(got);
    m += ", expected ";
    m += format_u64(expected);
    return m;
}

}  // namespace

std::uint32_t header_u16(const SqliteHeader& h, std::size_t offset) noexcept {
    return (static_cast<std::uint32_t>(h[offset]) << 8U) | static_cast<std::uint32_t>(h[offset + 1]);
}

std::uint32_t header_u32(const SqliteHeader& h, std::size_t offset) noexcept {
    return (static_cast<std::uint32_t>(h[offset]) << 24U) | (static_cast<std::uint32_t>(h[offset + 1]) << 16U) |
           (static_cast<std::uint32_t>(h[offset + 2]) << 8U) | static_cast<std::uint32_t>(h[offset + 3]);
}

bool has_sqlite_magic(const SqliteHeader& h) noexcept {
    for (std::size_t i = 0; i < sizeof SQLITE_MAGIC; ++i) {
        if (h[i] != static_cast<std::uint8_t>(SQLITE_MAGIC[i])) {
            return false;
        }
    }
    return true;
}

HeaderRead read_header(const std::filesystem::path& path) {
    HeaderRead r;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || ec) {
        return r;
    }
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec) {
        return r;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return r;
    }
    r.readable = true;
    r.file_size = static_cast<std::uint64_t>(size);
    char buffer[SQLITE_HEADER_SIZE] = {};
    in.read(buffer, static_cast<std::streamsize>(SQLITE_HEADER_SIZE));
    const std::streamsize got = in.gcount();
    if (in.bad()) {
        r.readable = false;
        return r;
    }
    r.complete = got == static_cast<std::streamsize>(SQLITE_HEADER_SIZE);
    for (std::size_t i = 0; i < SQLITE_HEADER_SIZE; ++i) {
        r.header[i] = static_cast<std::uint8_t>(buffer[i]);
    }
    return r;
}

std::optional<std::string> check_world_header(const SqliteHeader& h, std::uint64_t file_size) {
    if (!has_sqlite_magic(h)) {
        return std::string("header magic is not \"SQLite format 3\"");
    }
    struct Fixed {
        const char* name;
        std::uint64_t got;
        std::uint64_t expected;
    };
    const Fixed fixed[] = {
        {"16 (page size)", header_u16(h, 16), PAGE_SIZE},
        {"18 (write format version)", h[18], 1},
        {"19 (read format version)", h[19], 1},
        {"20 (reserved bytes per page)", h[20], 0},
        {"21 (max embedded payload fraction)", h[21], 64},
        {"22 (min embedded payload fraction)", h[22], 32},
        {"23 (leaf payload fraction)", h[23], 32},
        {"28 (database size in pages) * 4096", static_cast<std::uint64_t>(header_u32(h, 28)) * PAGE_SIZE,
         file_size},
        {"32 (first freelist trunk page)", header_u32(h, 32), 0},
        {"36 (freelist page count)", header_u32(h, 36), 0},
        {"44 (schema format number)", header_u32(h, 44), 4},
        {"48 (default page cache size)", header_u32(h, 48), 0},
        {"52 (largest root page)", header_u32(h, 52), 0},
        {"56 (text encoding)", header_u32(h, 56), 1},
        {"60 (user version)", header_u32(h, 60), static_cast<std::uint64_t>(SCHEMA_VERSION)},
        {"64 (incremental vacuum)", header_u32(h, 64), 0},
        {"68 (application id)", header_u32(h, 68), APPLICATION_ID},
        {"92 (version-valid-for) vs 24 (change counter)", header_u32(h, 92), header_u32(h, 24)},
        {"96 (SQLite version number)", header_u32(h, 96),
         static_cast<std::uint64_t>(sqlite3_libversion_number())},
    };
    for (const Fixed& f : fixed) {
        if (f.got != f.expected) {
            return field_message(f.name, f.got, f.expected);
        }
    }
    for (std::size_t i = 72; i < 92; ++i) {
        if (h[i] != 0U) {
            return field_message("72-91 (reserved)", h[i], 0);
        }
    }
    return std::nullopt;
}

}  // namespace worldgen::db::detail
