#pragma once

// Private helpers shared by the core sources: sign/magnitude handling and
// RHA rounding of U128 quotients. Not part of the public API.

#include <cstdint>

#include "worldgen/core/error.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen::detail {

inline constexpr std::uint64_t kInt64MaxU = 0x7FFFFFFFFFFFFFFFULL;  // 2^63 - 1

// |v| as uint64; exact for every int64, including INT64_MIN (2^63).
[[nodiscard]] constexpr std::uint64_t magnitude(std::int64_t v) noexcept {
    const auto u = static_cast<std::uint64_t>(v);
    return v < 0 ? std::uint64_t{0} - u : u;
}

// sign * m as int64 where m <= 2^63 - 1 (so the result is never INT64_MIN).
[[nodiscard]] constexpr std::int64_t apply_sign(bool negative, std::uint64_t m) noexcept {
    const auto s = static_cast<std::int64_t>(m);
    return negative ? -s : s;
}

// RHA(n / d) for unsigned n, d with d != 0: floor quotient, plus one if
// 2r >= d. Returns the rounded magnitude as U128. d <= 2^63 is required so
// that 2r fits in uint64 (r < d).
[[nodiscard]] inline U128 rha_divide(U128 n, std::uint64_t d) {
    const U128DivMod qr = divmod_u128_u64(n, d);
    U128 q = qr.quotient;
    // r < d <= 2^63, so 2r < 2^64; compare r >= d - r to avoid any doubt.
    if (qr.remainder >= d - qr.remainder) {
        q = q + U128{0, 1};
    }
    return q;
}

// U128 magnitude to int64 with sign; fatal if it exceeds 2^63 - 1.
[[nodiscard]] inline std::int64_t signed_from_magnitude(bool negative, U128 m, const char* what) {
    if (m.hi != 0 || m.lo > kInt64MaxU) {
        fatal(what);
    }
    return apply_sign(negative, m.lo);
}

}  // namespace worldgen::detail
