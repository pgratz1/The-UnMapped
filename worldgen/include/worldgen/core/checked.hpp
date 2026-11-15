#pragma once

// Checked plain-integer helpers (spec P0-01 sections 4 and 5.4).
//
// Every function here raises worldgen::FatalError instead of overflowing or
// dividing by zero; none ever returns a wrapped or saturated value.

#include <cstdint>

namespace worldgen {

// a + b on int64 (P0-01 4, 5.4). Fatal on overflow. INT64_MIN is a valid
// result for these plain-integer helpers.
[[nodiscard]] std::int64_t checked_add(std::int64_t a, std::int64_t b);

// a - b on int64 (P0-01 4, 5.4). Fatal on overflow.
[[nodiscard]] std::int64_t checked_sub(std::int64_t a, std::int64_t b);

// a * b on int64 (P0-01 4, 5.4). Fatal on overflow.
[[nodiscard]] std::int64_t checked_mul(std::int64_t a, std::int64_t b);

// floor(a / b) (P0-01 5.4). Fatal if b = 0 or the result overflows
// (INT64_MIN / -1).
[[nodiscard]] std::int64_t floor_div(std::int64_t a, std::int64_t b);

// a - b * floor(a / b) (P0-01 5.4): the result has the sign of the divisor
// (or is 0). Fatal if b = 0. floor_mod(INT64_MIN, -1) = 0.
[[nodiscard]] std::int64_t floor_mod(std::int64_t a, std::int64_t b);

// RHA(a * b / c) with an exact 128-bit product (P0-01 5.4): round half away
// from zero. Fatal if c = 0, or if the result is outside int64 or equals
// INT64_MIN. Any int64 is accepted for a, b and c.
[[nodiscard]] std::int64_t mul_div(std::int64_t a, std::int64_t b, std::int64_t c);

}  // namespace worldgen
