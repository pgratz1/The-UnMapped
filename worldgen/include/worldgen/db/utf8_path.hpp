#pragma once

// UTF-8 paths (spec P0-04 section 4.1): every path is handled as UTF-8
// internally and converted to std::filesystem::path through char8_t, so a
// non-ASCII path survives on Windows, where a narrow std::string would be
// read in the ANSI code page. SQLite takes the same UTF-8 bytes directly.

#include <filesystem>
#include <string>
#include <string_view>

namespace worldgen::db {

// True if utf8 is usable as a path: non-empty, strict UTF-8 (RFC 3629: no
// overlong forms, no surrogates, nothing above U+10FFFF) and no NUL byte
// (SQLite takes paths as NUL-terminated strings). The writer and the reader
// check this before converting, so that a bad path is an error value.
[[nodiscard]] bool is_valid_utf8_path(std::string_view utf8) noexcept;

// UTF-8 bytes to a filesystem path (P0-04 4.1). Precondition:
// is_valid_utf8_path(utf8); on MSVC invalid UTF-8 makes the conversion
// throw std::system_error.
[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view utf8);

// A filesystem path to UTF-8 bytes (P0-04 4.1).
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);

}  // namespace worldgen::db
