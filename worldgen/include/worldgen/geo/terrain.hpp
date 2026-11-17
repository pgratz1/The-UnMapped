#pragma once

// Terrain measures shared by the second-pass stages (spec P1-00 sections 7.7
// and 7.8): G7, height above drainage with a minimum river class, and G8,
// the central-difference cell slope.
//
// Both are exact integer functions of stored layers on any W x H grid (cell
// (x, y) has index y * width + x). G7 is used by P1-04 (marsh, k =
// drain_river_class_min) and P1-05 (floodplain and flooding, k = 1).

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/geo/grid.hpp"

namespace worldgen::geo {

// Water categories of the `water` layer (P1-00 4).
inline constexpr std::uint8_t WATER_CATEGORY_RIVER = 1;
inline constexpr std::uint8_t WATER_CATEGORY_LAKE = 2;
inline constexpr std::uint8_t WATER_CATEGORY_OCEAN = 3;

// The river class cap (P1-02 H6; P1-00 6.5).
inline constexpr std::int64_t RIVER_CLASS_MAX = 15;

// The surface S = elevation + water_depth (decimetres; 0 on ocean, the
// water surface on lakes, the ground elsewhere) of every cell (P1-00 7.7).
// Fatal if a size is wrong.
[[nodiscard]] std::vector<std::int64_t> water_surface(Grid grid, std::span<const std::int16_t> elevation,
                                                      std::span<const std::uint16_t> water_depth);

// True if cell c is a drain cell of G7 for minimum river class k: a lake or
// ocean cell (water 2 or 3), or a river cell (water 1) with river >= k.
[[nodiscard]] constexpr bool is_drain_cell(std::uint8_t water, std::uint8_t river, std::int64_t k) noexcept {
    return water == WATER_CATEGORY_LAKE || water == WATER_CATEGORY_OCEAN ||
           (water == WATER_CATEGORY_RIVER && std::int64_t{river} >= k);
}

// G7 (P1-00 7.7): hand[c] = S[c] - S[drain(c)] in decimetres for every cell,
// where drain(c) = c on a drain cell (is_drain_cell with class k) and
// drain(down(c)) otherwise, down(c) the neighbour flow_dir[c] points to.
// Computed by the reference order of 7.7 (D5): a FIFO queue starts with
// every drain cell in index order; popping c, each neighbour m in the
// 8-neighbour order that is not a drain cell and whose flow_dir points to c
// gets drain(m) = drain(c) and is pushed. The result does not depend on the
// order. Fatal, with the message beginning `message_prefix` (default "G7:
// "), if k is outside [1, 15], a size is wrong, a non-drain cell is not
// reached (its flow_dir does not lead to a drain cell: a code above 7, a
// step off the map or a cycle), or some hand is negative. On a hydrology
// output 0 <= hand <= 30000.
[[nodiscard]] std::vector<std::int64_t> height_above_drainage(Grid grid, std::span<const std::int64_t> surface,
                                                              std::span<const std::uint8_t> water,
                                                              std::span<const std::uint8_t> river,
                                                              std::span<const std::uint8_t> flow_dir, std::int64_t k);
[[nodiscard]] std::vector<std::int64_t> height_above_drainage(Grid grid, std::span<const std::int64_t> surface,
                                                              std::span<const std::uint8_t> water,
                                                              std::span<const std::uint8_t> river,
                                                              std::span<const std::uint8_t> flow_dir, std::int64_t k,
                                                              std::string_view message_prefix);

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

// "Not steeper than t" (P1-00 7.8), exactly: S2 x 2^64 <= (320 t.raw)^2.
// Fatal if S2 < 0 or as slope_s2_limit.
[[nodiscard]] bool slope_not_steeper(std::int64_t s2, Fx t);

}  // namespace worldgen::geo
