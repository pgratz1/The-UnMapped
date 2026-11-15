// Decimal parsing and canonical text (P0-01 5.6).

#include "worldgen/core/decimal.hpp"

#include <cstddef>
#include <string>

#include "core/int_detail.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen {

namespace {

constexpr std::size_t kMaxIntDigits = 10;
constexpr std::size_t kMaxFracDigits = 18;

bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

std::uint64_t digit_value(char c) noexcept {
    return static_cast<std::uint64_t>(static_cast<unsigned char>(c) - static_cast<unsigned char>('0'));
}

// Splits text into sign and the int / frac digit runs, checking the grammar.
struct Parts {
    bool negative = false;
    bool has_plus = false;
    std::string_view int_part;
    std::string_view frac_part;  // empty if there is no '.'
    bool has_point = false;
};

DecimalError split(std::string_view text, Parts& out) noexcept {
    std::size_t pos = 0;
    if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) {
        out.negative = text[pos] == '-';
        out.has_plus = text[pos] == '+';
        ++pos;
    }
    const std::size_t int_begin = pos;
    while (pos < text.size() && is_digit(text[pos])) {
        ++pos;
    }
    out.int_part = text.substr(int_begin, pos - int_begin);
    if (out.int_part.empty()) {
        return DecimalError::syntax;  // "", "-", ".5", "x"
    }
    if (out.int_part.size() > 1 && out.int_part[0] == '0') {
        return DecimalError::syntax;  // leading zero
    }
    if (pos < text.size() && text[pos] == '.') {
        out.has_point = true;
        ++pos;
        const std::size_t frac_begin = pos;
        while (pos < text.size() && is_digit(text[pos])) {
            ++pos;
        }
        out.frac_part = text.substr(frac_begin, pos - frac_begin);
        if (out.frac_part.empty()) {
            return DecimalError::syntax;  // trailing '.'
        }
    }
    if (pos != text.size()) {
        return DecimalError::syntax;  // anything else left over
    }
    if (out.int_part.size() > kMaxIntDigits) {
        return DecimalError::int_digits;
    }
    if (out.frac_part.size() > kMaxFracDigits) {
        return DecimalError::frac_digits;
    }
    return DecimalError::none;
}

// N = N * 10 + digit, in U128. N < 10^28 < 2^94 throughout, so no wrap.
U128 append_digit(U128 n, std::uint64_t digit) noexcept {
    const U128 lo_part = mul_u64_wide(n.lo, 10U);
    U128 r{n.hi * 10U + lo_part.hi, lo_part.lo};
    return r + U128{0, digit};
}

}  // namespace

DecimalParse parse_decimal(std::string_view text) noexcept {
    DecimalParse result;
    Parts parts;
    const DecimalError e = split(text, parts);
    if (e != DecimalError::none) {
        result.error = e;
        return result;
    }

    U128 n{};
    for (const char c : parts.int_part) {
        n = append_digit(n, digit_value(c));
    }
    std::uint64_t pow10 = 1;
    for (const char c : parts.frac_part) {
        n = append_digit(n, digit_value(c));
        pow10 *= 10U;
    }
    // raw magnitude = RHA(N * 2^32 / 10^f); N * 2^32 < 2^126, 10^f < 2^60.
    // divmod_u128_u64 cannot fail here (pow10 >= 1).
    const U128 q = detail::rha_divide(n << 32U, pow10);
    if (q.hi != 0 || q.lo > detail::kInt64MaxU) {
        result.error = DecimalError::out_of_range;
        return result;
    }
    result.value = Fx::from_raw(detail::apply_sign(parts.negative, q.lo));
    return result;
}

std::optional<std::string> canonical_decimal(std::string_view text) {
    Parts parts;
    if (split(text, parts) != DecimalError::none || !parse_decimal(text).ok()) {
        return std::nullopt;
    }
    std::string_view frac = parts.frac_part;
    while (!frac.empty() && frac.back() == '0') {
        frac.remove_suffix(1);
    }
    const bool is_zero = parts.int_part == "0" && frac.empty();
    std::string out;
    if (parts.negative && !is_zero) {
        out.push_back('-');
    }
    out.append(parts.int_part);
    if (!frac.empty()) {
        out.push_back('.');
        out.append(frac);
    }
    return out;
}

}  // namespace worldgen
