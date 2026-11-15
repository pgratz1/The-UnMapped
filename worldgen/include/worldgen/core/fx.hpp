#pragma once

// Fixed-point type Fx, Q32.32 (spec P0-01 section 5).
//
// value = raw / 2^32, raw an int64 in [-(2^63 - 1), 2^63 - 1]. raw = INT64_MIN
// is not a valid value. Every operation is exact before rounding and checked
// after; contract violations raise worldgen::FatalError (section 4.1). The
// only rounding mode is RHA, round half away from zero (section 5.2).

#include <compare>
#include <cstdint>
#include <string>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"

namespace worldgen {

// Q32.32 fixed-point value (P0-01 5.1). No implicit conversions from or to
// any arithmetic type; construct with the named functions. Comparison
// compares raw values. Default construction gives raw 0 (an implementation
// choice so that Fx can be held in arrays; it is the only unnamed
// construction).
class Fx {
public:
    // Raw value 0 (implementation choice, see above).
    constexpr Fx() noexcept = default;

    // Constants (P0-01 5.1). Defined after the class.
    static const Fx ONE;   // raw 2^32
    static const Fx HALF;  // raw 2^31
    static const Fx MAX;   // raw 2^63 - 1
    static const Fx MIN;   // raw -(2^63 - 1)
    static const Fx PI;    // raw 13493037705 = round(pi * 2^32)
    // P0-01 section 15 (P1-00 appendix A1 (a), section 12): sqrt(2) and
    // 1/sqrt(2); mul(SQRT2, INV_SQRT2) is exactly ONE.
    static const Fx SQRT2;      // raw 6074001000 = round(sqrt(2) * 2^32)
    static const Fx INV_SQRT2;  // raw 3037000500 = round(2^32 / sqrt(2))

    // raw = r (P0-01 5.2). Fatal if r = INT64_MIN.
    [[nodiscard]] static Fx from_raw(std::int64_t r);

    // raw = n * 2^32 (P0-01 5.2). Fatal if n < -(2^31 - 1) or n > 2^31 - 1.
    [[nodiscard]] static Fx from_int(std::int64_t n);

    // raw = RHA(num * 2^32 / den), exact (P0-01 5.2). Fatal if den = 0 or
    // the result is out of range.
    [[nodiscard]] static Fx from_ratio(std::int64_t num, std::int64_t den);

    // The raw value (P0-01 5.2).
    [[nodiscard]] constexpr std::int64_t raw() const noexcept { return raw_; }

    // raw >> 32, i.e. floor (P0-01 5.2).
    [[nodiscard]] std::int64_t floor_int() const noexcept;
    // -((-raw) >> 32), i.e. ceiling (P0-01 5.2).
    [[nodiscard]] std::int64_t ceil_int() const noexcept;
    // RHA(raw / 2^32) (P0-01 5.2).
    [[nodiscard]] std::int64_t round_int() const noexcept;
    // Toward zero: sign(raw) * (|raw| >> 32) (P0-01 5.2).
    [[nodiscard]] std::int64_t trunc_int() const noexcept;

    // Decimal text with d fractional digits, 0 <= d <= 18 (P0-01 5.7).
    // Fatal if d is out of range. Same as the free to_decimal(x, d).
    [[nodiscard]] std::string to_decimal(int d) const;

    friend constexpr bool operator==(const Fx&, const Fx&) noexcept = default;
    friend constexpr std::strong_ordering operator<=>(const Fx&, const Fx&) noexcept = default;

private:
    struct RawTag {};
    constexpr Fx(std::int64_t r, RawTag) noexcept : raw_(r) {}

    std::int64_t raw_ = 0;
};

inline constexpr Fx Fx::ONE{std::int64_t{4294967296}, Fx::RawTag{}};
inline constexpr Fx Fx::HALF{std::int64_t{2147483648}, Fx::RawTag{}};
inline constexpr Fx Fx::MAX{std::int64_t{9223372036854775807}, Fx::RawTag{}};
inline constexpr Fx Fx::MIN{std::int64_t{-9223372036854775807}, Fx::RawTag{}};
inline constexpr Fx Fx::PI{std::int64_t{13493037705}, Fx::RawTag{}};
inline constexpr Fx Fx::SQRT2{std::int64_t{6074001000}, Fx::RawTag{}};
inline constexpr Fx Fx::INV_SQRT2{std::int64_t{3037000500}, Fx::RawTag{}};

// --- Arithmetic (P0-01 5.4) -------------------------------------------------

// Raw sum (P0-01 5.4). Fatal if the result is outside the valid range.
[[nodiscard]] Fx operator+(Fx a, Fx b);
// Raw difference (P0-01 5.4). Fatal if the result is outside the valid range.
[[nodiscard]] Fx operator-(Fx a, Fx b);
// -raw (P0-01 5.4). Never fatal.
[[nodiscard]] Fx operator-(Fx a) noexcept;
// |raw| (P0-01 5.4). Never fatal.
[[nodiscard]] Fx abs(Fx a) noexcept;

// RHA(a.raw * b.raw / 2^32) (P0-01 5.4). Fatal if out of range.
[[nodiscard]] Fx mul(Fx a, Fx b);
// Same as mul(a, b) (P0-01 5.4).
[[nodiscard]] Fx operator*(Fx a, Fx b);

// RHA(a.raw * 2^32 / b.raw) (P0-01 5.4). Fatal if b = 0 or out of range.
[[nodiscard]] Fx div(Fx a, Fx b);
// Same as div(a, b) (P0-01 5.4).
[[nodiscard]] Fx operator/(Fx a, Fx b);

// raw * n, exact (P0-01 5.4). Fatal on overflow (including a result of
// INT64_MIN).
[[nodiscard]] Fx mul_int(Fx a, std::int64_t n);
// RHA(raw / n) (P0-01 5.4). Fatal if n = 0.
[[nodiscard]] Fx div_int(Fx a, std::int64_t n);

// Smaller / larger raw value (P0-01 5.4).
[[nodiscard]] Fx min(Fx a, Fx b) noexcept;
[[nodiscard]] Fx max(Fx a, Fx b) noexcept;
// x limited to [lo, hi] on raw values (P0-01 5.4). Fatal if lo > hi.
[[nodiscard]] Fx clamp(Fx x, Fx lo, Fx hi);

// a + mul(b - a, t) (P0-01 5.4). Fatal if b - a or the sum overflows.
// t is not range-checked.
[[nodiscard]] Fx lerp(Fx a, Fx b, Fx t);

// As + and -, but clamped to Fx::MIN / Fx::MAX instead of failing
// (P0-01 5.4). Never fatal.
[[nodiscard]] Fx sat_add(Fx a, Fx b) noexcept;
[[nodiscard]] Fx sat_sub(Fx a, Fx b) noexcept;

// Quintic smoothstep 6t^5 - 15t^4 + 10t^3, evaluated in the exact order of
// P0-01 5.5. Fatal unless 0 <= t.raw <= ONE.raw.
[[nodiscard]] Fx fade(Fx t);

// Decimal text of x with d fractional digits, 0 <= d <= 18 (P0-01 5.7).
// Fatal if d is out of range. Never produces "-0".
[[nodiscard]] std::string to_decimal(Fx x, int d);

}  // namespace worldgen
