#pragma once

// The 100-byte SQLite file header (spec P0-02 section 6.1). Private.

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace worldgen::db::detail {

inline constexpr std::size_t SQLITE_HEADER_SIZE = 100;

using SqliteHeader = std::array<std::uint8_t, SQLITE_HEADER_SIZE>;

// Big-endian 16- and 32-bit fields of the SQLite header.
[[nodiscard]] std::uint32_t header_u16(const SqliteHeader& h, std::size_t offset) noexcept;
[[nodiscard]] std::uint32_t header_u32(const SqliteHeader& h, std::size_t offset) noexcept;

// True if the header starts with "SQLite format 3\0".
[[nodiscard]] bool has_sqlite_magic(const SqliteHeader& h) noexcept;

// Result of reading the start of a file.
struct HeaderRead {
    bool readable = false;    // the file exists, is a regular file and could be read
    std::uint64_t file_size = 0;
    bool complete = false;    // at least 100 bytes were read
    SqliteHeader header{};
};

// Reads the first 100 bytes and the size of a file. Never throws except
// std::bad_alloc.
[[nodiscard]] HeaderRead read_header(const std::filesystem::path& path);

// Checks every field of P0-02 6.1 that has a fixed value in a world file,
// plus database size * 4096 == file size and version-valid-for == change
// counter. Returns a description of the first mismatch, or std::nullopt.
[[nodiscard]] std::optional<std::string> check_world_header(const SqliteHeader& h, std::uint64_t file_size);

}  // namespace worldgen::db::detail
