// Internal text helpers of the loader (P0-03).

#include "data/text.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>

#include "worldgen/core/decimal.hpp"

namespace worldgen::data::detail {

bool bytes_less(std::string_view a, std::string_view b) noexcept {
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto x = static_cast<std::uint8_t>(a[i]);
        const auto y = static_cast<std::uint8_t>(b[i]);
        if (x != y) {
            return x < y;
        }
    }
    return a.size() < b.size();
}

bool ends_with(std::string_view text, std::string_view suffix) noexcept {
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

bool is_valid_data_file_name(std::string_view name) noexcept {
    constexpr std::string_view ext = ".toml";
    if (!ends_with(name, ext)) {
        return false;
    }
    const std::string_view stem = name.substr(0, name.size() - ext.size());
    if (stem.empty() || !(is_lower(stem[0]) || is_digit(stem[0]))) {
        return false;
    }
    for (const char c : stem) {
        if (!(is_lower(c) || is_digit(c) || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

std::string int_text(std::int64_t v) {
    std::array<char, 24> buf{};
    const auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    return std::string(buf.data(), res.ptr);
}

std::string uint_text(std::uint64_t v) {
    std::array<char, 24> buf{};
    const auto res = std::to_chars(buf.data(), buf.data() + buf.size(), v);
    return std::string(buf.data(), res.ptr);
}

std::string fx_text(Fx x) {
    for (int d = 0; d < 18; ++d) {
        std::string s = to_decimal(x, d);
        const DecimalParse back = parse_decimal(s);
        if (back.ok() && back.value == x) {
            return s;
        }
    }
    return to_decimal(x, 18);
}

std::int32_t first_forbidden_control(std::string_view s) noexcept {
    for (const char ch : s) {
        const auto b = static_cast<std::uint8_t>(ch);
        if ((b < 0x20 && b != 0x09 && b != 0x0A) || b == 0x7F) {
            return static_cast<std::int32_t>(b);
        }
    }
    return -1;
}

std::string code_point_text(std::uint32_t cp) {
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::string out = "U+";
    for (int shift = 12; shift >= 0; shift -= 4) {
        out.push_back(hex[(cp >> static_cast<std::uint32_t>(shift)) & 0xFU]);
    }
    return out;
}

std::string in_quotes(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('\'');
    out.append(text);
    out.push_back('\'');
    return out;
}

}  // namespace worldgen::data::detail
