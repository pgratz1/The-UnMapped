#pragma once

// The [history] section of a report without the two keys P5-06 9.4 appends after settlement_races (figures = <n>, books = <n>):
// the goldens of the older stages (written by their references) do not hold them. Their presence is checked.

#include <doctest/doctest.h>

#include <string>

namespace p506section {

inline std::string strip_figure_counts(const std::string& s) {
    const std::size_t at = s.find("\nfigures = ");
    if (at == std::string::npos) {
        FAIL_CHECK("no 'figures = ' key in [history]");
        return s;
    }
    const std::size_t books = s.find('\n', at + 1) + 1;   // the next line must be books = <n>
    CHECK(s.compare(books, 8, "books = ") == 0);
    const std::size_t end = s.find('\n', books);
    return s.substr(0, at + 1) + (end == std::string::npos ? std::string() : s.substr(end + 1));
}

}  // namespace p506section
