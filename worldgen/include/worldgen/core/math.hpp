#pragma once

// Math functions provided in P0 (spec P0-01 section 9.1). fade, lerp, clamp,
// min, max, abs and the rounding conversions are in fx.hpp; mul_div is in
// checked.hpp.

#include <bit>
#include <cstdint>

#include "worldgen/core/fx.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen {

// The number of significant bits of v: 0 for v = 0, otherwise
// floor(log2 v) + 1, so 1 -> 1, 2 and 3 -> 2, 2^63 and 2^64 - 1 -> 64
// (P0-01 section 15, P1-00 appendix A1 (b) and section 12). Exact; uses
// std::bit_width (<bit>, C++20), which has no compiler-specific form.
[[nodiscard]] constexpr std::int32_t bit_length(std::uint64_t v) noexcept {
    return static_cast<std::int32_t>(std::bit_width(v));
}

// floor(sqrt(n)), exact (P0-01 9.1). Bitwise digit-by-digit method.
[[nodiscard]] std::uint64_t isqrt_u64(std::uint64_t n) noexcept;

// floor(sqrt(n)), exact, result < 2^64 (P0-01 9.1).
[[nodiscard]] std::uint64_t isqrt_u128(U128 n) noexcept;

// Square root rounded to nearest (P0-01 9.1): N = x.raw * 2^32,
// r = isqrt_u128(N), r + 1 if N - r^2 > r. Fatal if x < 0.
[[nodiscard]] Fx sqrt(Fx x);

// sqrt(a^2 + b^2) rounded to nearest with no intermediate rounding
// (P0-01 9.1). Fatal if the result exceeds Fx::MAX.
[[nodiscard]] Fx hypot(Fx a, Fx b);

}  // namespace worldgen
