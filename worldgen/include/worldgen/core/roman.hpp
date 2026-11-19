#pragma once

// Roman numerals (P1-06 RG11: subtractive form, thousands as repeated M).
// Shared by the region names of the regions stage (stages::roman, P1-06)
// and the name generator of the history library (history::roman, P3-02
// 4.3), so that the history library does not include a stage header.

#include <cstdint>
#include <string>

namespace worldgen {

// The Roman numeral of k: standard subtractive form (IV, IX, XL, XC, CD,
// CM), thousands as repeated M. Fatal ("roman: k outside [1, 100000]") if
// k < 1 or k > 100000.
[[nodiscard]] std::string roman_numeral(std::int64_t k);

}  // namespace worldgen
