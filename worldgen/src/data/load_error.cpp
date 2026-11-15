// Loader errors (P0-03 section 9).

#include "worldgen/data/load_error.hpp"

#include <algorithm>

#include "data/text.hpp"

namespace worldgen::data {

std::string LoadError::to_string() const {
    std::string out = path;
    if (line != 0) {
        out += ':';
        out += detail::uint_text(line);
        out += ':';
        out += detail::uint_text(column);
    }
    out += ": error: ";
    out += message;
    return out;
}

bool error_less(const LoadError& a, const LoadError& b) noexcept {
    if (a.path != b.path) {
        return detail::bytes_less(a.path, b.path);
    }
    if (a.line != b.line) {
        return a.line < b.line;
    }
    if (a.column != b.column) {
        return a.column < b.column;
    }
    return detail::bytes_less(a.message, b.message);
}

void sort_errors(std::vector<LoadError>& errors) {
    // Identical errors can occur before deduplication, so the order is made
    // independent of the standard library with a stable sort (P0-01 section
    // 4, sorting). error_less compares exactly the fields that operator==
    // compares (path, line, column, message), so two errors are equivalent
    // under error_less exactly when they are equal, every run of equal errors
    // is adjacent after sorting, and std::unique removes all duplicates.
    std::stable_sort(errors.begin(), errors.end(), error_less);
    errors.erase(std::unique(errors.begin(), errors.end()), errors.end());
}

}  // namespace worldgen::data
