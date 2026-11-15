#pragma once
// Loader errors and results (spec P0-03 section 9).
//
// Loading is all-or-nothing (P0-03 section 3): a LoadResult holds either a
// value and no errors, or no value and a non-empty error list sorted by
// (path bytes, line, column, message bytes) with duplicates removed.
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace worldgen::data {

// One loader error (P0-03 section 9).
struct LoadError {
    // Relative to the data root with '/' separators for data files and kind
    // directories; the path as given for the settings file and the data root.
    std::string path;
    // 1-based line and column as reported by the parser for the offending
    // value (for a missing field: the definition's table header). Both 0
    // when the error has no position. Columns count Unicode code points
    // (toml++ convention), not bytes.
    std::uint32_t line = 0;
    std::uint32_t column = 0;
    std::string message;

    // "<path>:<line>:<column>: error: <message>", or
    // "<path>: error: <message>" when there is no position (P0-03 9).
    // No trailing newline.
    [[nodiscard]] std::string to_string() const;

    friend bool operator==(const LoadError&, const LoadError&) = default;
};

// The order of P0-03 section 9: path bytes, then line, then column, then
// message bytes (an error without a position, line 0, sorts first in its
// file).
[[nodiscard]] bool error_less(const LoadError& a, const LoadError& b) noexcept;

// Sorts with error_less and removes exact duplicates (P0-03 9).
void sort_errors(std::vector<LoadError>& errors);

// Result of a load (P0-03 section 3): value set exactly when errors is empty.
template <typename T>
struct LoadResult {
    std::optional<T> value;
    std::vector<LoadError> errors;  // sorted and deduplicated (P0-03 9)

    [[nodiscard]] bool ok() const noexcept { return value.has_value(); }
};

}  // namespace worldgen::data
