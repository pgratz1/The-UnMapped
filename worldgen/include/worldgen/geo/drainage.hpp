#pragma once

// G7, height above drainage with a minimum river class (spec P1-00 section
// 7.7), shared by the second-pass stages: P1-04 (marsh, k =
// drain_river_class_min) and P1-05 (floodplain and flooding, k = 1). An exact
// integer function of the stored layers on any W x H grid (cell (x, y) has
// index y * width + x).

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

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

}  // namespace worldgen::geo
