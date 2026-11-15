// Fixed-point type Fx, Q32.32 (P0-01 5.1-5.5, 5.7).

#include "worldgen/core/fx.hpp"

#include <array>
#include <charconv>
#include <limits>
#include <string>
#include <system_error>

#include "core/int_detail.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen {

namespace {

constexpr std::int64_t kRawMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kRawMin = -kRawMax;  // -(2^63 - 1)
constexpr std::int64_t kIntLimit = 2147483647;  // 2^31 - 1
constexpr std::uint64_t kLow32 = 0xFFFFFFFFULL;
constexpr std::uint64_t kHalfRaw = 0x80000000ULL;  // 2^31

// Builds an Fx from a raw value already known to be valid.
Fx make(std::int64_t raw) {
    return Fx::from_raw(raw);
}

// |v| >> 32 rounded half away from zero, as a uint64 magnitude.
std::uint64_t rha_shift32(std::uint64_t m) noexcept {
    std::uint64_t q = m >> 32;
    if ((m & kLow32) >= kHalfRaw) {
        q += 1U;
    }
    return q;
}

}  // namespace

// --- Conversions (5.2) -------------------------------------------------------

Fx Fx::from_raw(std::int64_t r) {
    if (r == std::numeric_limits<std::int64_t>::min()) {
        fatal("Fx::from_raw: INT64_MIN is not a valid Fx");
    }
    return Fx(r, RawTag{});
}

Fx Fx::from_int(std::int64_t n) {
    if (n < -kIntLimit || n > kIntLimit) {
        fatal("Fx::from_int: out of range");
    }
    // |n| <= 2^31 - 1, so n * 2^32 fits; shift the unsigned pattern.
    const auto bits = static_cast<std::uint64_t>(n) << 32;
    return Fx(static_cast<std::int64_t>(bits), RawTag{});
}

Fx Fx::from_ratio(std::int64_t num, std::int64_t den) {
    if (den == 0) {
        fatal("Fx::from_ratio: division by zero");
    }
    const bool negative = (num < 0) != (den < 0);
    const U128 n = U128{0, detail::magnitude(num)} << 32U;
    const U128 q = detail::rha_divide(n, detail::magnitude(den));
    return Fx(detail::signed_from_magnitude(negative, q, "Fx::from_ratio: result out of range"), RawTag{});
}

std::int64_t Fx::floor_int() const noexcept {
    return raw_ >> 32;
}

std::int64_t Fx::ceil_int() const noexcept {
    return -((-raw_) >> 32);
}

std::int64_t Fx::round_int() const noexcept {
    const std::uint64_t q = rha_shift32(detail::magnitude(raw_));
    return detail::apply_sign(raw_ < 0, q);
}

std::int64_t Fx::trunc_int() const noexcept {
    const std::uint64_t q = detail::magnitude(raw_) >> 32;
    return detail::apply_sign(raw_ < 0, q);
}

std::string Fx::to_decimal(int d) const {
    return worldgen::to_decimal(*this, d);
}

// --- Arithmetic (5.4) --------------------------------------------------------

Fx operator+(Fx a, Fx b) {
    const std::int64_t x = a.raw();
    const std::int64_t y = b.raw();
    if ((y > 0 && x > kRawMax - y) || (y < 0 && x < kRawMin - y)) {
        fatal("Fx +: overflow");
    }
    return make(x + y);
}

Fx operator-(Fx a, Fx b) {
    const std::int64_t x = a.raw();
    const std::int64_t y = b.raw();
    if ((y < 0 && x > kRawMax + y) || (y > 0 && x < kRawMin + y)) {
        fatal("Fx -: overflow");
    }
    return make(x - y);
}

Fx operator-(Fx a) noexcept {
    // raw is never INT64_MIN, so -raw is defined and valid.
    return Fx::from_raw(-a.raw());
}

Fx abs(Fx a) noexcept {
    return a.raw() < 0 ? -a : a;
}

Fx mul(Fx a, Fx b) {
    const bool negative = (a.raw() < 0) != (b.raw() < 0);
    const U128 p = mul_u64_wide(detail::magnitude(a.raw()), detail::magnitude(b.raw()));
    U128 q = p >> 32U;
    if ((p.lo & kLow32) >= kHalfRaw) {
        q = q + U128{0, 1};
    }
    return make(detail::signed_from_magnitude(negative, q, "Fx mul: overflow"));
}

Fx operator*(Fx a, Fx b) {
    return mul(a, b);
}

Fx div(Fx a, Fx b) {
    if (b.raw() == 0) {
        fatal("Fx div: division by zero");
    }
    const bool negative = (a.raw() < 0) != (b.raw() < 0);
    const U128 n = U128{0, detail::magnitude(a.raw())} << 32U;
    const U128 q = detail::rha_divide(n, detail::magnitude(b.raw()));
    return make(detail::signed_from_magnitude(negative, q, "Fx div: overflow"));
}

Fx operator/(Fx a, Fx b) {
    return div(a, b);
}

Fx mul_int(Fx a, std::int64_t n) {
    const std::int64_t r = checked_mul(a.raw(), n);
    if (r == std::numeric_limits<std::int64_t>::min()) {
        fatal("Fx mul_int: overflow");
    }
    return make(r);
}

Fx div_int(Fx a, std::int64_t n) {
    if (n == 0) {
        fatal("Fx div_int: division by zero");
    }
    const bool negative = (a.raw() < 0) != (n < 0);
    const U128 q = detail::rha_divide(U128{0, detail::magnitude(a.raw())}, detail::magnitude(n));
    // |q| <= |raw| <= 2^63 - 1, so this never fails.
    return make(detail::signed_from_magnitude(negative, q, "Fx div_int: overflow"));
}

Fx min(Fx a, Fx b) noexcept {
    return b.raw() < a.raw() ? b : a;
}

Fx max(Fx a, Fx b) noexcept {
    return b.raw() > a.raw() ? b : a;
}

Fx clamp(Fx x, Fx lo, Fx hi) {
    if (lo.raw() > hi.raw()) {
        fatal("Fx clamp: lo > hi");
    }
    if (x.raw() < lo.raw()) {
        return lo;
    }
    if (x.raw() > hi.raw()) {
        return hi;
    }
    return x;
}

Fx lerp(Fx a, Fx b, Fx t) {
    const Fx diff = b - a;
    const Fx step = mul(diff, t);
    return a + step;
}

Fx sat_add(Fx a, Fx b) noexcept {
    const std::int64_t x = a.raw();
    const std::int64_t y = b.raw();
    if (y > 0 && x > kRawMax - y) {
        return Fx::MAX;
    }
    if (y < 0 && x < kRawMin - y) {
        return Fx::MIN;
    }
    return Fx::from_raw(x + y);  // in range by the checks above
}

Fx sat_sub(Fx a, Fx b) noexcept {
    const std::int64_t x = a.raw();
    const std::int64_t y = b.raw();
    if (y < 0 && x > kRawMax + y) {
        return Fx::MAX;
    }
    if (y > 0 && x < kRawMin + y) {
        return Fx::MIN;
    }
    return Fx::from_raw(x - y);  // in range by the checks above
}

// --- fade (5.5) --------------------------------------------------------------

Fx fade(Fx t) {
    const std::int64_t one = Fx::ONE.raw();
    if (t.raw() < 0 || t.raw() > one) {
        fatal("fade: t outside [0, ONE]");
    }
    const Fx t2 = mul(t, t);
    const Fx t3 = mul(t2, t);
    const Fx a = Fx::from_raw(6 * t.raw() - 15 * one);  // |6t| <= 6 * 2^32, exact
    const Fx b = mul(t, a) + Fx::from_raw(10 * one);
    const Fx f = mul(t3, b);
    return clamp(f, Fx(), Fx::ONE);
}

// --- to_decimal (5.7) --------------------------------------------------------

std::string to_decimal(Fx x, int d) {
    if (d < 0 || d > 18) {
        fatal("to_decimal: digit count outside [0, 18]");
    }
    std::uint64_t pow10 = 1;
    for (int i = 0; i < d; ++i) {
        pow10 *= 10U;
    }
    // scaled = RHA(raw * 10^d / 2^32); |raw| * 10^d < 2^123.
    const U128 p = mul_u64_wide(detail::magnitude(x.raw()), pow10);
    U128 scaled = p >> 32U;
    if ((p.lo & kLow32) >= kHalfRaw) {
        scaled = scaled + U128{0, 1};
    }
    const bool is_zero = scaled.hi == 0 && scaled.lo == 0;
    const U128DivMod parts = divmod_u128_u64(scaled, pow10);
    // Integer part: |x| < 2^31 + 1, so it fits in the low word.
    std::string out;
    if (x.raw() < 0 && !is_zero) {
        out.push_back('-');
    }
    std::array<char, 24> buf{};
    const auto int_res = std::to_chars(buf.data(), buf.data() + buf.size(), parts.quotient.lo);
    out.append(buf.data(), int_res.ptr);
    if (d > 0) {
        const auto frac_res = std::to_chars(buf.data(), buf.data() + buf.size(), parts.remainder);
        const auto len = static_cast<std::size_t>(frac_res.ptr - buf.data());
        out.push_back('.');
        out.append(static_cast<std::size_t>(d) - len, '0');
        out.append(buf.data(), len);
    }
    return out;
}

}  // namespace worldgen
