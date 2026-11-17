#pragma once

// Reference worlds for the [geography] section of report v2 (spec P1-00
// section 10.3 with appendix C items 6 to 10), shared by worldgen-tests and
// worldgen-cli-tests. Every layer is copied from a spec's text, and every
// expected value is derived by hand in the comments, never from the code
// under test.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/stages/testing.hpp"

namespace p100 {

template <class T>
worldgen::db::LayerGrid grid(std::uint32_t width, std::uint32_t height, std::vector<T> values) {
    worldgen::db::LayerGrid g;
    g.width = width;
    g.height = height;
    g.values = std::move(values);
    return g;
}

// --- P1-02 section 5.1, the worked example (8 wide, 7 tall) -------------------

inline constexpr std::uint32_t WX = 8;
inline constexpr std::uint32_t WY = 7;

// Elevation e (decimetres), copied from P1-02 5.1.
inline const std::vector<std::int16_t> WORKED_ELEVATION = {
    -5, -5, -5, -5, -5, -5, -5, -5,  //
    -5, 60, 70, 80, 70, 60, 50, -5,  //
    -5, 50, 20, 22, 60, 40, 30, -5,  //
    -5, 45, 21, 23, 55, 35, 25, -5,  //
    -5, 40, 30, 28, 40, 30, 20, -5,  //
    -5, 35, 33, 31, 25, 20, 10, -5,  //
    -5, -5, -5, -5, -5, -5, -5, -5,  //
};

// Final flow_dir (after H4 step 2), copied from P1-02 5.1.
inline const std::vector<std::uint8_t> WORKED_FLOW_DIR = {
    8, 8, 8, 8, 8, 8, 8, 8,  //
    8, 4, 6, 6, 6, 6, 0, 8,  //
    8, 4, 2, 3, 4, 1, 0, 8,  //
    8, 4, 1, 4, 4, 1, 0, 8,  //
    8, 4, 0, 1, 2, 1, 0, 8,  //
    8, 2, 2, 2, 2, 2, 0, 8,  //
    8, 8, 8, 8, 8, 8, 8, 8,  //
};

// flow_accum (rows 0 and 6 are 0), copied from P1-02 5.1.
inline const std::vector<std::uint32_t> WORKED_FLOW_ACCUM = {
    0, 0, 0, 0,  0, 0, 0, 0,  //
    0, 1, 1, 1,  1, 1, 1, 0,  //
    0, 1, 1, 2,  1, 1, 1, 0,  //
    0, 1, 6, 2,  1, 1, 2, 0,  //
    0, 1, 1, 8,  1, 1, 2, 0,  //
    0, 1, 1, 1, 10, 1, 2, 0,  //
    0, 0, 0, 0,  0, 0, 0, 0,  //
};

// water, copied from P1-02 5.1.
inline const std::vector<std::uint8_t> WORKED_WATER = {
    3, 3, 3, 3, 3, 3, 3, 3,  //
    3, 0, 0, 0, 0, 0, 0, 3,  //
    3, 0, 2, 2, 0, 0, 0, 3,  //
    3, 0, 2, 2, 0, 0, 0, 3,  //
    3, 0, 0, 1, 0, 0, 0, 3,  //
    3, 0, 0, 0, 1, 0, 0, 3,  //
    3, 3, 3, 3, 3, 3, 3, 3,  //
};

// water_depth: 5 on the ocean, 8, 6 / 7, 5 on the lake (P1-02 5.1).
inline const std::vector<std::uint16_t> WORKED_WATER_DEPTH = {
    5, 5, 5, 5, 5, 5, 5, 5,  //
    5, 0, 0, 0, 0, 0, 0, 5,  //
    5, 0, 8, 6, 0, 0, 0, 5,  //
    5, 0, 7, 5, 0, 0, 0, 5,  //
    5, 0, 0, 0, 0, 0, 0, 5,  //
    5, 0, 0, 0, 0, 0, 0, 5,  //
    5, 5, 5, 5, 5, 5, 5, 5,  //
};

// river: class 2 at (3, 4) (A 8) and (4, 5) (A 10) (P1-02 5.1 H6).
inline const std::vector<std::uint8_t> WORKED_RIVER = {
    0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 2, 0, 0, 0, 0,  //
    0, 0, 0, 0, 2, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0,  //
};

// lake: 1 on the four lake cells (P1-02 5.1 H3).
inline const std::vector<std::uint32_t> WORKED_LAKE = {
    0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 1, 1, 0, 0, 0, 0,  //
    0, 0, 1, 1, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0,  //
    0, 0, 0, 0, 0, 0, 0, 0,  //
};

// coast_dist (metres), copied from P1-02 5.1 H7.
inline const std::vector<std::int16_t> WORKED_COAST_DIST = {
    -22, -16, -16, -16, -16, -16, -16, -22,  //
    -16, 16,  16,  16,  16,  16,  16,  -16,  //
    -16, 16,  32,  32,  32,  32,  16,  -16,  //
    -16, 16,  32,  48,  48,  32,  16,  -16,  //
    -16, 16,  32,  32,  32,  32,  16,  -16,  //
    -16, 16,  16,  16,  16,  16,  16,  -16,  //
    -22, -16, -16, -16, -16, -16, -16, -22,  //
};

// The worked example as a fixture world: elevation, then the seven
// hydrology layers in the order of P1-02 H9.
inline std::vector<worldgen::stages::testing::FixtureLayer> worked_example_layers(
    worldgen::db::Codec codec = worldgen::db::Codec::delta_varint) {
    std::vector<worldgen::stages::testing::FixtureLayer> v;
    v.push_back({"elevation", grid(WX, WY, WORKED_ELEVATION), codec});
    v.push_back({"water", grid(WX, WY, WORKED_WATER), codec});
    v.push_back({"water_depth", grid(WX, WY, WORKED_WATER_DEPTH), codec});
    v.push_back({"flow_dir", grid(WX, WY, WORKED_FLOW_DIR), codec});
    v.push_back({"flow_accum", grid(WX, WY, WORKED_FLOW_ACCUM), codec});
    v.push_back({"river", grid(WX, WY, WORKED_RIVER), codec});
    v.push_back({"lake", grid(WX, WY, WORKED_LAKE), codec});
    v.push_back({"coast_dist", grid(WX, WY, WORKED_COAST_DIST), codec});
    return v;
}

// The elevation lines of [geography] for the worked example, derived by hand:
//   cells 8 x 7 = 56; land = the 6 x 5 interior = 30 (every interior e > 0);
//   ocean = 56 - 30 = 26; land_fraction = RHA(300000 / 56) = 5357.14 -> 0.5357.
//   landmasses 1 (one rectangle), largest share 30/30 = 1.0000.
//   coastline: the 6 x 5 rectangle has 2 (6 + 5) = 22 edges against ocean,
//   16 x 22 = 352 m.
//   coast_ratio: A = 256 x 30 = 7680 m^2, Pq = isqrt(4 x 13493037705 x 7680
//   x 2^32) = 1334275167302 (2 sqrt(pi A) = 310.66 m as Fx raw), value
//   RHA(352 x 2^64 / Pq) = 4866502857 (1.13307), to_decimal(4) = 1.1331.
//   land_solidity: every land row y = 1..5 spans x 1..6, corners x = 1 and
//   7, y = 1..6: hull is the 6 x 5 rectangle, 2H = 60; ratio4(60, 60) = 1.0000.
//   elevation_m: the 30 land values sorted are 10 20 20 20 21 22 23 25 25 28
//   30 30 30 31 33 35 35 40 40 40 45 50 50 55 60 60 60 70 70 80; ranks
//   min(29, floor(p 30 / 100)) for p = 0 1 10 25 50 75 90 99 100 are 0 0 3 7
//   15 22 27 29 29, values 10 10 20 25 35 50 70 80 80 dm.
//   depth_m: every ocean cell is -5, depth 5 dm = 0.5 m at every percentile.
//   land_above_m: the highest land is 80 dm < 500 dm, so every share is 0.
//   max_step_m: largest |e(a) - e(b)| over 4-adjacent land pairs is (3, 1) 80
//   against (3, 2) 22 = 58 dm (next: (2, 1) 70 / (2, 2) 20 = 50) -> 5.8.
//   The five keys of 2026-11-15 (10.3 "Cell step", "Cell slope", "Edge-
//   parallel run"):
//   step_p999_m: n_L = 30 < 1000, rank min(29, floor(999 x 30 / 1000) = 29)
//   = 29, the largest cell step, which is the largest pair step: 5.8.
//   Slopes: every land cell is inside the 8 x 7 grid, so all differences are
//   central; ocean neighbours count as e' = 0. Per cell (gx, gy) S2, row by
//   row (x = 1..6):
//     y 1: (70,50) 7400, (20,20) 800, (0,22) 484, (-20,60) 4000,
//          (-20,40) 2000, (-60,30) 4500
//     y 2: (20,-15) 625, (-28,-49) 3185, (40,-57) 4849, (18,-15) 549,
//          (-30,-25) 1525, (-40,-25) 2225
//     y 3: (21,-10) 541, (-22,10) 584, (34,6) 1192, (12,-20) 544,
//          (-30,-10) 1000, (-35,-10) 1325
//     y 4: (30,-10) 1000, (-12,12) 288, (10,8) 164, (2,-30) 904,
//          (-20,-15) 625, (-30,-15) 1125
//     y 5: (33,-40) 2689, (-4,-30) 916, (-8,-28) 848, (-11,-40) 1721,
//          (-15,-30) 1125, (-20,-20) 800
//   so every S2 lies in [164, 7400]: none is steeper than 35 degrees (S2 >=
//   50205) or 45 degrees: slope_over 35:0.0000 45:0.0000; none is flatter
//   than 0.5 degrees (S2 <= 7). Every land cell is below 1000 dm, so the
//   lowland is all 30 cells, 0 flat: lowland_flat_share 0.0000. No cell is
//   >= 5000 dm: slope_p50_above_500m n/a.
//   edge_parallel_run_m: land is the rectangle x 1..6, y 1..5. West: t = 1
//   on rows 1..5 (rows 0 and 6 undefined): 5. East: 7 - 6 = 1 on rows 1..5:
//   5. North: t = 1 on columns 1..6: 6. South: 6 - 5 = 1 on columns 1..6: 6.
//   Largest 6, 16 x 6 = 96.
inline constexpr std::string_view WORKED_GEOGRAPHY_ELEVATION_LINES =
    "[geography]\n"
    "cells = 56\n"
    "land_cells = 30\n"
    "ocean_cells = 26\n"
    "land_fraction = 0.5357\n"
    "landmasses = 1\n"
    "largest_landmass_share = 1.0000\n"
    "coastline_m = 352\n"
    "coast_ratio = 1.1331\n"
    "land_solidity = 1.0000\n"
    "elevation_m = 1.0 1.0 2.0 2.5 3.5 5.0 7.0 8.0 8.0\n"
    "depth_m = 0.5 0.5 0.5\n"
    "land_above_m = 50:0.0000 500:0.0000 1000:0.0000 1500:0.0000 2000:0.0000\n"
    "max_step_m = 5.8\n"
    "step_p999_m = 5.8\n"
    "slope_over = 35:0.0000 45:0.0000\n"
    "slope_p50_above_500m = n/a\n"
    "lowland_flat_share = 0.0000\n"
    "edge_parallel_run_m = 96\n";

// The hydrology lines for the worked example, derived by hand:
//   lakes: the largest lake value is 1.
//   lake_cells: water = 2 at (2,2) (3,2) (2,3) (3,3) = 4; share RHA(40000/30)
//   = 1333.3 -> 0.1333; largest_lake_cells 4.
//   river_cells: water = 1 at (3,4) and (4,5) = 2; share RHA(20000/30) =
//   666.7 -> 0.0667.
//   river_sources: (3,4) is fed by the lake outlet (2,3) (flow_dir 1, SE),
//   a lake cell, so it is not a source; (4,5) is fed by (3,4) (flow_dir 1),
//   a river cell. 0 sources (appendix C item 8: possible with rivers).
//   river_mouths: (4,5) has flow_dir 2 (S) to (4,6), ocean; (3,4) drains to
//   (4,5), a river. 1 mouth. major_rivers: its class is 2 < 4, so 0.
//   longest_river_m (appendix C item 8, over all river cells): D(4,5) = one
//   orthogonal step = 16 m; D(3,4) = one diagonal step to (4,5) plus D(4,5)
//   = 16 x 6074001000 + 16 x 2^32 = 97184016000 + 68719476736 =
//   165903492736 raw = 38.627 m, round_int 39. (The lake outlet (2,3) is
//   further, D = 16 sqrt2 + D(3,4) = 61.25 m, but it is a lake cell, not a
//   river cell.)
//   largest_basin_share: cells whose downstream cell is ocean are the
//   rim of the interior; their flow_accum values are 1 except (6,3) 2,
//   (6,4) 2, (6,5) 2 and (4,5) 10; the largest is 10; RHA(100000/30) =
//   3333.3 -> 0.3333. (Check: these outlet values sum to 30, the land.)
inline constexpr std::string_view WORKED_GEOGRAPHY_HYDROLOGY_LINES =
    "lakes = 1\n"
    "lake_cells = 4\n"
    "lake_share_of_land = 0.1333\n"
    "largest_lake_cells = 4\n"
    "river_cells = 2\n"
    "river_share_of_land = 0.0667\n"
    "river_sources = 0\n"
    "river_mouths = 1\n"
    "major_rivers = 0\n"
    "longest_river_m = 39\n"
    "largest_basin_share = 0.3333\n";

// The pass keys of [geography] for a world whose table pass has no rows
// (P1-00 10.3; every schema version 2 world has the table, P1-01 9.1),
// inserted after the edge_parallel_run_m line of a section.
inline constexpr std::string_view EMPTY_PASS_LINES =
    "passes = 0\n"
    "natural_gaps = 0\n"
    "passes_capped = 0\n"
    "pass_col_m = n/a n/a n/a\n"
    "passes_walkable = 0\n";

inline std::string with_empty_pass_lines(std::string_view section) {
    std::string s(section);
    const std::size_t at = s.find("edge_parallel_run_m = ");
    const std::size_t end = s.find('\n', at);
    s.insert(end + 1U, EMPTY_PASS_LINES);
    return s;
}

inline std::string worked_geography() {
    return std::string(WORKED_GEOGRAPHY_ELEVATION_LINES) + std::string(WORKED_GEOGRAPHY_HYDROLOGY_LINES);
}

// --- A square island with elevation only (5 x 5) ------------------------------

inline constexpr std::uint32_t SQ = 5;

// Ocean -100 except (0, 0) -300 and (4, 4) -1; a 3 x 3 island in the middle.
inline const std::vector<std::int16_t> SQUARE_ELEVATION = {
    -300, -100, -100,  -100, -100,  //
    -100, 100,  600,   5000, -100,  //
    -100, 15000, 20000, 9999, -100,  //
    -100, 1,    499,   500,  -100,  //
    -100, -100, -100,  -100, -1,    //
};

// Derived by hand:
//   cells 25, land 9, ocean 16, land_fraction RHA(90000/25) = 0.3600.
//   landmasses 1, share 1.0000. coastline: the 3 x 3 square has 12 edges
//   against ocean: 192 m.
//   coast_ratio: 2 sqrt(pi x 2304) = 170.15 m; 192 / 170.15 = 1.12838 ->
//   1.1284 (Pq = 730812607050, raw 4846351620).
//   land_solidity: hull of rows 1..3 spanning x 1..3 is the square [1,4]^2,
//   2H = 18, ratio4(18, 18) = 1.0000.
//   elevation_m: sorted land 1 100 499 500 600 5000 9999 15000 20000 (n 9);
//   ranks min(8, floor(9p/100)) for p 0 1 10 25 50 75 90 99 100 = 0 0 0 2 4 6
//   8 8 8: 1 1 1 499 600 9999 20000 20000 20000 dm.
//   depth_m: depths sorted 1, 100 (14 times), 300 (n 16); ranks 0, 8, 15:
//   0.1 10.0 30.0.
//   land_above_m: e >= 500: 500 600 5000 9999 15000 20000 = 6 -> 6/9 =
//   0.6667; >= 5000: 4 -> 0.4444; >= 10000: 15000 20000 = 2 -> 0.2222;
//   >= 15000: 2 -> 0.2222; >= 20000: 1 -> 0.1111.
//   max_step_m: 4-adjacent land pairs; the largest is (2,2) 20000 against
//   (2,3) 499 = 19501 dm -> 1950.1 (next: (2,1) 600 / (2,2) 20000 = 19400).
//   step_p999_m: cell steps m(c) over land 4-neighbours: (1,1) 14900,
//   (2,1) 19400, (3,1) 4999, (1,2) 14999, (2,2) 19501, (3,2) 10001,
//   (1,3) 14999, (2,3) 19501, (3,3) 9499; n_L = 9, rank min(8,
//   floor(8991 / 1000) = 8) = 8, the largest: 19501 -> 1950.1.
//   Slopes (all central; e' = 0 on the ocean ring), (gx, gy) S2:
//     (1,1) (600, 15000) 225360000      (2,1) (4900, 20000) 424010000
//     (3,1) (-600, 9999) 100340001      (1,2) (20000, -99) 400009801
//     (2,2) (-5001, -101) 25020202      (3,2) (-20000, -4500) 420250000
//     (1,3) (499, -15000) 225249001     (2,3) (499, -20000) 400249001
//     (3,3) (-499, -9999) 100229002
//   all > 102400: slope_over 35:1.0000 45:1.0000.
//   slope_p50_above_500m: cells with e >= 5000 are (3,1) 5000 (the
//   threshold itself is included), (1,2), (2,2), (3,2): S2 sorted 25020202,
//   100340001, 400009801, 420250000; rank min(3, floor(200 / 100)) = 2:
//   400009801; r = isqrt(400009801 x 10^8) = 200002450 (200002450^2 =
//   40000980006002500 <= 40000980100000000 < 200002451^2), T = (200002450 +
//   160) div 320 = 625008: 62.5008.
//   lowland_flat_share: e < 1000 are 100, 600, 1, 499, 500 (5 cells), none
//   flat: 0.0000.
//   edge_parallel_run_m: land x 1..3, y 1..3; each side's t is 1 on three
//   lines: 3 on every side, 48.
inline constexpr std::string_view SQUARE_GEOGRAPHY =
    "[geography]\n"
    "cells = 25\n"
    "land_cells = 9\n"
    "ocean_cells = 16\n"
    "land_fraction = 0.3600\n"
    "landmasses = 1\n"
    "largest_landmass_share = 1.0000\n"
    "coastline_m = 192\n"
    "coast_ratio = 1.1284\n"
    "land_solidity = 1.0000\n"
    "elevation_m = 0.1 0.1 0.1 49.9 60.0 999.9 2000.0 2000.0 2000.0\n"
    "depth_m = 0.1 10.0 30.0\n"
    "land_above_m = 50:0.6667 500:0.4444 1000:0.2222 1500:0.2222 2000:0.1111\n"
    "max_step_m = 1950.1\n"
    "step_p999_m = 1950.1\n"
    "slope_over = 35:1.0000 45:1.0000\n"
    "slope_p50_above_500m = 62.5008\n"
    "lowland_flat_share = 0.0000\n"
    "edge_parallel_run_m = 48\n";

// --- A world built for the slope statistics (7 x 6) ---------------------------
//
// Ocean -40 on the west column and the north row only; the land touches the
// east and south grid edges (test layers only), so one-sided differences are
// doubled there. A coastal plain in the west, a highland in the south-east.

inline constexpr std::uint32_t SLW = 7;
inline constexpr std::uint32_t SLH = 6;

inline const std::vector<std::int16_t> SLOPE_ELEVATION = {
    -40, -40, -40, -40,  -40,  -40,  -40,   //
    -40, 2,   3,   3,    40,   400,  700,   //
    -40, 2,   3,   4,    999,  3000, 5000,  //
    -40, 3,   3,   600,  4800, 5000, 5100,  //
    -40, 2,   4,   1500, 5150, 5300, 5250,  //
    -40, 2,   10,  3000, 5200, 5350, 5400,  //
};

// Derived by hand from 10.3:
//   cells 42, land 30 (x 1..6, y 1..5), ocean 12; land_fraction RHA(300000 /
//   42) = 7142.86 -> 0.7143. One landmass, share 1.0000.
//   coastline: row 1 against row 0 (6 pairs) and column 1 against column 0
//   (5 pairs): 11 pairs, 176 m. coast_ratio: A = 7680 m^2 as in the worked
//   example, Pq = 1334275167302; RHA(176 x 2^64 / Pq) = 2433251429 raw
//   (0.56653), to_decimal(4) 0.5665.
//   land_solidity: every land row spans x 1..6: hull [1,7] x [1,6], 2H = 60,
//   ratio4(60, 60) = 1.0000.
//   elevation_m: land sorted 2 2 2 2 3 3 3 3 3 4 4 10 40 400 600 700 999 1500
//   3000 3000 4800 5000 5000 5100 5150 5200 5250 5300 5350 5400; ranks
//   min(29, floor(30 p / 100)) for p 0 1 10 25 50 75 90 99 100 = 0 0 3 7 15
//   22 27 29 29: 2 2 2 3 700 5000 5300 5400 5400 dm.
//   depth_m: 12 ocean cells, all 40 dm: 4.0 4.0 4.0.
//   land_above_m: >= 500: 600 700 999 1500 3000 3000 4800 and the nine
//   >= 5000 = 16 -> 0.5333; >= 5000: 9 -> 0.3000; >= 10000: 0.
//   max_step_m: the largest pair step is (6,1) 700 / (6,2) 5000 = 4300 dm
//   (next (3,3) 600 / (4,3) 4800 = 4200): 430.0.
//   step_p999_m: n_L = 30, rank 29, the largest cell step: 430.0.
//   Slopes, (gx, gy) S2 per land cell (x = 1..6; at x = 6 and y = 5 the
//   one-sided difference is doubled, e.g. (6,1): gx = (700 - 400) x 2 = 600):
//     y 1: (3,2) 13, (1,3) 10, (37,4) 1385, (397,999) 1155610,
//          (660,3000) 9435600, (600,5000) 25360000
//     y 2: (3,1) 10, (2,0) 4, (996,597) 1348425, (2996,4760) 31633616,
//          (4001,4600) 37168001, (4000,4400) 35360000
//     y 3: (3,0) 9, (597,1) 356410, (4797,1496) 25249225,
//          (4400,4151) 36590801, (300,2300) 5380000, (200,250) 102500
//     y 4: (4,-1) 17, (1498,7) 2244053, (5146,2400) 32241316,
//          (3800,400) 14600000, (100,350) 132500, (-100,300) 100000
//     y 5: (10,0) 100, (2998,12) 8988148, (5190,3000) 35936100,
//          (2350,100) 5532500, (200,100) 50000, (100,300) 100000
//   Steeper than 35 degrees (S2 >= 50205): every S2 from 102500 up, plus
//   the two 100000 = 21 cells; (5,5) at 50000 (34.94 degrees) is not:
//   ratio4(21, 30) = 0.7000. Steeper than 45 degrees (S2 > 102400): the same
//   less (6,4) and (6,5) at 100000 = 19 -> 0.6333. (6,3) at 102500 is
//   45.01 degrees and counts in both.
//   slope_p50_above_500m: e >= 5000 are (6,2) 5000, (5,3) 5000, (6,3),
//   (4,4), (5,4), (6,4), (4,5), (5,5), (6,5): S2 sorted 50000 100000 100000
//   102500 132500 5380000 5532500 14600000 35360000 (n 9); rank floor(450 /
//   100) = 4: 132500; r = isqrt(132500 x 10^8) = 3640054, T = (3640054 +
//   160) div 320 = 11375: 1.1375 (48.7 degrees). (Without the two cells at
//   exactly 5000, n 7, rank 3: 102500, 1.0005.)
//   lowland_flat_share: e < 1000 are the 17 cells of columns 1 and 2, (3,1)
//   (3,2) (3,3), (4,1) and (4,2) 999, (5,1) and (6,1); flat (S2 <= 7)
//   only (2,2), S2 4: ratio4(1, 17) = 0.0588.
//   edge_parallel_run_m: west t = 1 on rows 1..5: 5; east t = 6 - 6 = 0 on
//   rows 1..5: 5; north t = 1 on columns 1..6: 6; south t = 5 - 5 = 0 on
//   columns 1..6: 6. Largest 6: 96.
inline constexpr std::string_view SLOPE_GEOGRAPHY =
    "[geography]\n"
    "cells = 42\n"
    "land_cells = 30\n"
    "ocean_cells = 12\n"
    "land_fraction = 0.7143\n"
    "landmasses = 1\n"
    "largest_landmass_share = 1.0000\n"
    "coastline_m = 176\n"
    "coast_ratio = 0.5665\n"
    "land_solidity = 1.0000\n"
    "elevation_m = 0.2 0.2 0.2 0.3 70.0 500.0 530.0 540.0 540.0\n"
    "depth_m = 4.0 4.0 4.0\n"
    "land_above_m = 50:0.5333 500:0.3000 1000:0.0000 1500:0.0000 2000:0.0000\n"
    "max_step_m = 430.0\n"
    "step_p999_m = 430.0\n"
    "slope_over = 35:0.7000 45:0.6333\n"
    "slope_p50_above_500m = 1.1375\n"
    "lowland_flat_share = 0.0588\n"
    "edge_parallel_run_m = 96\n";

}  // namespace p100
