// Roman numerals (P1-06 RG11), shared by the regions stage and the history
// library.

#include "worldgen/core/roman.hpp"

#include <array>
#include <utility>

#include "worldgen/core/error.hpp"

namespace worldgen {

std::string roman_numeral(std::int64_t k) {
    if (k < 1 || k > 100000) {
        fatal("roman: k outside [1, 100000]");
    }
    static constexpr std::array<std::pair<std::int64_t, const char*>, 13> PARTS = {{
        {1000, "M"},
        {900, "CM"},
        {500, "D"},
        {400, "CD"},
        {100, "C"},
        {90, "XC"},
        {50, "L"},
        {40, "XL"},
        {10, "X"},
        {9, "IX"},
        {5, "V"},
        {4, "IV"},
        {1, "I"},
    }};
    std::string out;
    for (const auto& [value, text] : PARTS) {
        while (k >= value) {
            out += text;
            k -= value;
        }
    }
    return out;
}

}  // namespace worldgen
