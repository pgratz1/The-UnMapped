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
    // error_less compares every field, so it is a strict total order on
    // distinct errors (P0-01 section 4, sorting); equal errors are removed.
    std::sort(errors.begin(), errors.end(), error_less);
    errors.erase(std::unique(errors.begin(), errors.end()), errors.end());
}

}  // namespace worldgen::data
