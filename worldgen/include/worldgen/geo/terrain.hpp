#pragma once

// G8, the central-difference cell slope (spec P1-00 section 7.8), shared by
// the second-pass stages: an exact integer function of the stored elevation
// on any W x H grid (cell (x, y) has index y * width + x). G7 (height above
// drainage) is in worldgen/geo/drainage.hpp, included here for convenience.

#include <cstdint>
#include <span>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/geo/drainage.hpp"
#include "worldgen/geo/grid.hpp"

namespace worldgen::geo {

// G8 (P1-00 7.8, 10.3 "Cell slope"): S2 = gx^2 + gy^2 of cell (x, y) on the
// stored elevation with e' = max(e, 0): xa = max(x - 1, 0), xb = min(x + 1,
// W - 1), gx = (e'(xb, y) - e'(xa, y)) (2 div (xb - xa)), 0 if xb = xa; gy
// likewise. In (decimetres per 32 m)^2, S2 < 2^33. Fatal if (x, y) is
// outside the grid or a size is wrong.
[[nodiscard]] std::int64_t cell_slope_s2(Grid grid, std::span<const std::int16_t> elevation, std::int32_t x,
                                         std::int32_t y);

// G8 for every cell, in index order (D1). Fatal if a size is wrong.
[[nodiscard]] std::vector<std::int64_t> slope_s2(Grid grid, std::span<const std::int16_t> elevation);

// The largest S2 that is "not steeper than" the tangent t (P1-00 7.8):
// S2 x 2^64 <= (320 t.raw)^2 (U128) holds exactly when S2 <= the high 64
// bits of (320 t.raw)^2, which this returns. Fatal if t < 0 or 320 t.raw
// does not fit in 64 bits (t at most 2^25 or so; every schema bound is far
// below).
[[nodiscard]] std::int64_t slope_s2_limit(Fx t);

// The slope tangent of G8 (P1-00 7.8): tan = div_int(from_raw(isqrt_u128(S2
// x 2^64)), 320), an Fx. Fatal if S2 < 0 or S2 >= 2^34 (S2 < 2^33 on any
// i16 elevation). Examples: 9216 -> raw 1288490189 (0.3); 102400 -> ONE.
[[nodiscard]] Fx slope_tangent(std::int64_t s2);

// "Not steeper than t" (P1-00 7.8), exactly: S2 x 2^64 <= (320 t.raw)^2.
// Fatal if S2 < 0 or as slope_s2_limit.
[[nodiscard]] bool slope_not_steeper(std::int64_t s2, Fx t);

}  // namespace worldgen::geo
