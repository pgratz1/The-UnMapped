#pragma once

// Portable 128-bit unsigned intermediates (spec P0-01 section 5.3).
//
// MSVC has no __int128, so the core uses its own U128. All functions here are
// the portable reference methods; there is no compiler-specific fast path in
// this implementation.

#include <compare>
#include <cstdint>

namespace worldgen {

// Unsigned 128-bit value hi * 2^64 + lo (P0-01 5.3). Aggregate:
// U128{hi, lo}. Comparison is numeric (hi first, then lo).
struct U128 {
    std::uint64_t hi = 0;
    std::uint64_t lo = 0;

    friend constexpr bool operator==(const U128&, const U128&) = default;
    friend constexpr std::strong_ordering operator<=>(const U128&, const U128&) = default;
};

// The value v as a U128 (P0-01 5.3).
[[nodiscard]] constexpr U128 u128_from_u64(std::uint64_t v) noexcept {
    return U128{0, v};
}

// Exact 64 x 64 -> 128 product (P0-01 5.3). Reference method: four 32 x 32
// partial products summed with carries.
[[nodiscard]] U128 mul_u64_wide(std::uint64_t a, std::uint64_t b) noexcept;

// Result of divmod_u128_u64.
struct U128DivMod {
    U128 quotient;
    std::uint64_t remainder = 0;
};

// Exact floor division n / d and remainder n mod d (P0-01 5.3).
// Fatal (FatalError) if d = 0.
[[nodiscard]] U128DivMod divmod_u128_u64(U128 n, std::uint64_t d);

// Addition and subtraction modulo 2^128 (P0-01 5.3, "as needed"). Callers
// that need exactness check for wrap themselves (for example a + b < a).
[[nodiscard]] U128 operator+(U128 a, U128 b) noexcept;
[[nodiscard]] U128 operator-(U128 a, U128 b) noexcept;

// Logical shifts (P0-01 5.3). The shift count must be in [0, 127];
// fatal (FatalError) otherwise.
[[nodiscard]] U128 operator<<(U128 a, unsigned int count);
[[nodiscard]] U128 operator>>(U128 a, unsigned int count);

}  // namespace worldgen
