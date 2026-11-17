#pragma once

// The [geography] section of worldgen-cli report, format v2 (spec P1-00
// section 10.3, P0-04 amendment 16c).
//
// Every statistic is an exact integer function of stored layers (P1-00 5.1),
// declared here as a function on the cell arrays so that tests can check
// each one on hand-made grids. Grids are W x H (the statistics do not need
// a square map), cells in index order i = y W + x (P0-02 5.6). "Land" is
// elevation > 0 (lakes included), "ocean" elevation < 0; a cell with
// elevation exactly 0 (which no finished world has, P1-00 4 rule 4) is
// neither, so it is counted in neither and never forms a coastline pair.
//
// Single-threaded throughout. Neighbour orders are those of P1-00 2.3.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"

namespace worldgen::inspect {

// --- Number formats (P1-00 10.3) ---------------------------------------------

// ratio4(a, b) (P1-00 10.3): RHA(a * 10^4 / b) printed as its integer part,
// '.', and four digits, e.g. "0.4213", "1.0000"; "n/a" if b = 0. Exact on
// 128-bit integers for every a and b; FatalError if a * 10^4 / b >= 2^64.
[[nodiscard]] std::string ratio4(std::uint64_t a, std::uint64_t b);

// metres1(v), v in decimetres (P1-00 10.3): "-" if v < 0, then |v| div 10,
// '.', |v| mod 10, e.g. "-12.5", "0.0", "2400.0". Any int64.
[[nodiscard]] std::string metres1(std::int64_t v);

// The rank of percentile p (integer percent) of n values (P1-00 10.3):
// min(n - 1, floor(p n / 100)). FatalError if n = 0 or p > 100.
[[nodiscard]] std::uint64_t percentile_rank(std::uint64_t n, std::uint32_t p);

// The text n/a printed for a value that is undefined on this world
// (reading chosen: a percentile of no values, a ratio over 0).
inline constexpr std::string_view NOT_AVAILABLE = "n/a";

// --- Elevation statistics (P1-00 10.3) -----------------------------------------
// e is an elevation grid in decimetres, w x h cells, e.size() = w h; each
// function raises FatalError otherwise (or if w or h is 0).

// Counts of land (e > 0) and ocean (e < 0) cells.
struct LandOceanCounts {
    std::uint64_t cells = 0;  // w h
    std::uint64_t land = 0;
    std::uint64_t ocean = 0;
};
[[nodiscard]] LandOceanCounts count_land_ocean(std::span<const std::int16_t> e, std::uint32_t w,
                                               std::uint32_t h);

// The 8-connected components of land (P1-00 2.3, labels as G2 7.2): how many
// and the cell count of the largest (0 and 0 when there is no land).
// Computed with G2 (geo::label_components, 8-connectivity). FatalError if
// w h exceeds geo::MAX_GRID_CELLS (2^24, 4096 x 4096), the limit of the
// shared algorithms; geography_layers_problem reports such a layer first.
struct Landmasses {
    std::uint64_t count = 0;
    std::uint64_t largest_cells = 0;
};
[[nodiscard]] Landmasses count_landmasses(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h);

// coastline_m (P1-00 10.3): 16 x the number of unordered 4-adjacent
// (land, ocean) cell pairs.
[[nodiscard]] std::uint64_t coastline_m(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h);

// The perimeter, as Fx raw, of a circle whose area is that of land_cells
// cells (P1-00 10.3): Pq = isqrt_u128(4 PI.raw A 2^32), A = 256 land_cells.
// Exact on U128 for land_cells < 2^40.  FatalError above.
[[nodiscard]] std::uint64_t circle_perimeter_raw(std::uint64_t land_cells);

// coast_ratio (P1-00 10.3): the Fx of RHA(coastline_m 2^64 / Pq), which is
// from_ratio(coastline_m 2^32, Pq) evaluated on U128 so that it is exact for
// any grid size (the int64 form overflows once coastline_m >= 2^31, beyond
// any map of N <= 2048). std::nullopt when land_cells = 0 (Pq = 0; printed
// as n/a). FatalError if the ratio exceeds Fx::MAX.
[[nodiscard]] std::optional<Fx> coast_ratio(std::uint64_t coastline_m, std::uint64_t land_cells);

// An integer point of the hull construction (cell-corner coordinates).
struct HullPoint {
    std::int64_t x = 0;
    std::int64_t y = 0;
    friend constexpr bool operator==(const HullPoint&, const HullPoint&) noexcept = default;
};

// Twice the area of the convex hull of the points (P1-00 10.3): Andrew's
// monotone chain on integers (points sorted by (x, y), duplicates removed,
// collinear points dropped), then the shoelace formula. 0 for fewer than
// three distinct points or all collinear. Coordinates must lie in
// [-2^29, 2^29] (FatalError otherwise), so every cross product and the
// area sum fit int64.
[[nodiscard]] std::uint64_t convex_hull_area2(std::span<const HullPoint> points);

// The hull points of the land (P1-00 10.3, land_solidity): for each row y
// with land, in ascending y, the corners (x_l, y), (x_l, y+1), (x_r+1, y),
// (x_r+1, y+1) of its westmost x_l and eastmost x_r land cells.
[[nodiscard]] std::vector<HullPoint> land_hull_points(std::span<const std::int16_t> e, std::uint32_t w,
                                                      std::uint32_t h);

// 2H for land_solidity: convex_hull_area2(land_hull_points(e, w, h)); 0 if
// there is no land. land_solidity = ratio4(2 land_cells, 2H).
[[nodiscard]] std::uint64_t land_hull_area2(std::span<const std::int16_t> e, std::uint32_t w,
                                            std::uint32_t h);

// The land percentiles listed in ps (each 0..100) of e over land cells, in
// decimetres, in the order of ps (P1-00 10.3 elevation_m: kth(values,
// percentile_rank(n, p)), G1). Empty if there is no land.
// The values are those of G1 (geo::kth); the implementation counts an exact
// histogram over the i16 range instead, which gives the same values and was
// measured faster on large layers than geo::kth_many on a copy (report at
// 2048: 0.31 s against 0.37 s).
[[nodiscard]] std::vector<std::int64_t> land_percentiles(std::span<const std::int16_t> e,
                                                         std::span<const std::uint32_t> ps);

// The same for the depths -e of ocean cells (P1-00 10.3 depth_m), each > 0.
// Empty if there is no ocean.
[[nodiscard]] std::vector<std::int64_t> ocean_depth_percentiles(std::span<const std::int16_t> e,
                                                                std::span<const std::uint32_t> ps);

// The number of land cells with e >= threshold_dm (P1-00 10.3 land_above_m).
[[nodiscard]] std::uint64_t land_at_or_above(std::span<const std::int16_t> e, std::int64_t threshold_dm);

// max_step_m in decimetres (P1-00 10.3): the largest |e(a) - e(b)| over
// unordered 4-adjacent pairs of land cells; 0 if there is no such pair
// (reading chosen).
[[nodiscard]] std::uint64_t max_land_step(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h);

// --- Step, slope and edge statistics (P1-00 10.3, added 2026-11-15) ----------------

// The rank of the per-mille p of n values, the percentile rank rule of
// P1-00 10.3 in thousandths (the "Cell step" paragraph): min(n - 1,
// floor(p n / 1000)). FatalError if n = 0 or p > 1000.
[[nodiscard]] std::uint64_t permille_rank(std::uint64_t n, std::uint32_t p);

// The cell step m(c) of every land cell, in index order (P1-00 10.3 "Cell
// step"): the largest |e(c) - e(n)| over the 4-neighbours n of c that exist
// and are land, 0 if there are none. In decimetres, at most 32766.
[[nodiscard]] std::vector<std::int64_t> land_cell_steps(std::span<const std::int16_t> e, std::uint32_t w,
                                                        std::uint32_t h);

// step_p999_m in decimetres (P1-00 10.3): kth(m over land,
// permille_rank(land_cells, 999)); std::nullopt if there is no land (n/a).
// Equals max_land_step when there are fewer than 1000 land cells. Computed
// on an exact histogram of m, which gives the value of G1.
[[nodiscard]] std::optional<std::int64_t> land_step_p999(std::span<const std::int16_t> e, std::uint32_t w,
                                                         std::uint32_t h);

// The slope thresholds of [geography] as exact decimal tangents t / 10^4,
// constants of the report (P1-00 10.3 "Cell slope"): 35 degrees is 7002,
// 45 degrees 10000, 0.5 degrees 87.
inline constexpr std::int64_t SLOPE_TAN4_35_DEG = 7002;
inline constexpr std::int64_t SLOPE_TAN4_45_DEG = 10000;
inline constexpr std::int64_t SLOPE_TAN4_HALF_DEG = 87;
// Elevation bounds of slope_p50_above_500m (e >= 5000 dm) and
// lowland_flat_share (e < 1000 dm), P1-00 10.3.
inline constexpr std::int64_t SLOPE_HIGH_DM = 5000;
inline constexpr std::int64_t SLOPE_LOWLAND_DM = 1000;

// S2 = gx^2 + gy^2 of cell (x, y) by central differences (P1-00 10.3 "Cell
// slope"): with e'(p) = max(e(p), 0), xa = max(x - 1, 0), xb = min(x + 1,
// w - 1) (ya, yb likewise), gx = (e'(xb, y) - e'(xa, y)) (2 div (xb - xa)),
// 0 if xb = xa, and gy likewise; so a one-sided difference at a grid edge is
// doubled and both are in decimetres per 32 m. The slope's tangent is
// sqrt(S2) / 320. Defined for any cell (the statistics use land cells);
// S2 < 2^33. FatalError if (x, y) is outside the grid.
[[nodiscard]] std::int64_t cell_slope_s2(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h,
                                         std::uint32_t x, std::uint32_t y);

// Exact tangent comparisons of P1-00 10.3, for S2 as above and the
// threshold t / 10^4 given as tan4_value = t (0 <= t <= 10^5, FatalError
// otherwise):
// steeper: 10^8 S2 > (320 t)^2; flatter: 10^8 S2 < (320 t)^2.
// (Steeper than 35 degrees is S2 >= 50205, than 45 degrees S2 > 102400,
// flatter than 0.5 degrees S2 <= 7.) FatalError if S2 < 0 or S2 >= 2^34.
[[nodiscard]] bool slope_steeper_than(std::int64_t s2, std::int64_t tan4_value);
[[nodiscard]] bool slope_flatter_than(std::int64_t s2, std::int64_t tan4_value);

// tan4(S2) (P1-00 10.3): r = isqrt_u64(S2 10^8), T = (r + 160) div 320,
// printed as T div 10^4, '.', and T mod 10^4 in four digits, e.g. "0.5039".
// FatalError if S2 < 0 or S2 >= 2^34.
[[nodiscard]] std::string tan4(std::int64_t s2);

// The counts behind the three slope keys of [geography] (P1-00 10.3), from
// one pass over the land cells:
//   slope_over            = 35: ratio4(steeper_35, land) 45: ratio4(steeper_45, land)
//   slope_p50_above_500m  = tan4(median_high_s2), n/a when it is nullopt
//   lowland_flat_share    = ratio4(lowland_flat, lowland)
// median_high_s2 is kth over the S2 of land cells with e >= SLOPE_HIGH_DM at
// percentile_rank(n, 50) (G1, value only), nullopt if there are none.
struct SlopeStats {
    std::uint64_t land = 0;          // land cells (e > 0)
    std::uint64_t steeper_35 = 0;    // land cells steeper than 35 degrees
    std::uint64_t steeper_45 = 0;    // land cells steeper than 45 degrees
    std::uint64_t high = 0;          // land cells with e >= SLOPE_HIGH_DM
    std::optional<std::int64_t> median_high_s2;
    std::uint64_t lowland = 0;       // land cells with e < SLOPE_LOWLAND_DM
    std::uint64_t lowland_flat = 0;  // of those, flatter than 0.5 degrees
};
[[nodiscard]] SlopeStats slope_stats(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h);

// The value of each slope key as printed in [geography] (P1-00 10.3); each
// computes slope_stats.
[[nodiscard]] std::string slope_over(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h);
[[nodiscard]] std::string slope_p50_above_500m(std::span<const std::int16_t> e, std::uint32_t w,
                                               std::uint32_t h);
[[nodiscard]] std::string lowland_flat_share(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h);

// The sides of the map for edge_parallel_run_m (P1-00 10.3 "Edge-parallel
// run").
enum class MapSide : std::uint8_t { west, east, north, south };

// The tolerance of an edge-parallel run, in cells: a constant of the report
// (P1-00 10.3).
inline constexpr std::int64_t EDGE_RUN_TOLERANCE = 8;

// A line with no land in edge_distances.
inline constexpr std::int64_t EDGE_UNDEFINED = -1;

// The sequence t of P1-00 10.3 for one side, one entry per line parallel to
// it: west, for each row a = 0..h-1, the smallest x with land in the row;
// east, w - 1 - the largest such x; north, for each column a = 0..w-1, the
// smallest y with land in the column; south, h - 1 - the largest such y.
// EDGE_UNDEFINED for a line with no land.
[[nodiscard]] std::vector<std::int64_t> edge_distances(std::span<const std::int16_t> e, std::uint32_t w,
                                                       std::uint32_t h, MapSide side);

// The largest run of P1-00 10.3 over all starts of t: the most consecutive
// entries, all defined (>= 0), whose max - min is <= tolerance; 0 if no entry
// is defined. FatalError if tolerance or an entry is outside [0, 2^32]
// (EDGE_UNDEFINED apart).
[[nodiscard]] std::uint64_t longest_edge_run(std::span<const std::int64_t> t, std::int64_t tolerance);

// longest_edge_run(edge_distances(side), EDGE_RUN_TOLERANCE) for each side.
struct EdgeRuns {
    std::uint64_t west = 0;
    std::uint64_t east = 0;
    std::uint64_t north = 0;
    std::uint64_t south = 0;
};
[[nodiscard]] EdgeRuns edge_parallel_runs(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h);

// edge_parallel_run_m (P1-00 10.3): 16 x the largest side value of
// edge_parallel_runs, in metres; 0 if there is no land.
[[nodiscard]] std::uint64_t edge_parallel_run_m(std::span<const std::int16_t> e, std::uint32_t w,
                                                std::uint32_t h);

// --- Hydrology statistics (P1-00 10.3, P1-02 3 and H6) ---------------------------

// Path lengths to the ocean along flow_dir (P1-00 10.3 longest_river_m): for
// every cell, D = 0 on ocean (e < 0); otherwise D(c) = step(c) + D(down(c)),
// step 16 x ONE.raw orthogonal (even code) and 16 x SQRT2.raw diagonal (odd
// code), in raw int64. When some non-ocean cell does not reach the ocean
// (flow_dir not a D8 code 0..7, a step leaving the map, or a cycle), d is
// empty and bad_cell is the smallest such index (reading chosen: the
// statistics are then undefined, and the report fails with an input error).
struct FlowPaths {
    std::vector<std::int64_t> d;
    std::optional<std::uint64_t> bad_cell;
};
[[nodiscard]] FlowPaths flow_path_lengths(std::span<const std::int16_t> e, std::span<const std::uint8_t> flow_dir,
                                          std::uint32_t w, std::uint32_t h);

// Raw Fx lengths of one D8 step of 16 m (P1-00 10.3): 16 ONE.raw and
// 16 SQRT2.raw, SQRT2.raw = 6074001000 (P1-00 amendment A1). SQRT2_RAW is
// Fx::SQRT2.raw(), kept as a name for callers.
inline constexpr std::int64_t SQRT2_RAW = Fx::SQRT2.raw();
inline constexpr std::int64_t ORTHOGONAL_STEP_RAW = 16 * Fx::ONE.raw();
inline constexpr std::int64_t DIAGONAL_STEP_RAW = 16 * Fx::SQRT2.raw();

// The hydrology lines of [geography] (P1-00 10.3). Definitions, with "ocean"
// e < 0 as for the elevation lines and down(c) the cell flow_dir[c] points
// at (when it is a code 0..7 inside the map):
//   lakes                largest lake value
//   lake_cells           cells with water = 2
//   largest_lake_cells   the most cells sharing one non-zero lake value
//                        (0 if none)
//   river_cells          cells with water = 1
//   river_sources        river cells c with no cell u, water[u] 1 or 2,
//                        down(u) = c
//   river_mouths         river cells whose down(c) is ocean
//   major_rivers         river mouths with river >= 4
//   longest_river_raw    largest D (flow_path_lengths) over all river cells
//                        (water = 1), 0 if none, so a river that begins at
//                        a lake outlet is measured from there (P1-00
//                        appendix C item 8); longest_river_m = round_int
//                        of it
//   largest_basin_cells  largest flow_accum over non-ocean cells whose
//                        down(c) is ocean, 0 if none
struct HydrologyStats {
    std::uint64_t lakes = 0;
    std::uint64_t lake_cells = 0;
    std::uint64_t largest_lake_cells = 0;
    std::uint64_t river_cells = 0;
    std::uint64_t river_sources = 0;
    std::uint64_t river_mouths = 0;
    std::uint64_t major_rivers = 0;
    std::int64_t longest_river_raw = 0;
    std::uint64_t largest_basin_cells = 0;
};

// The hydrology layers as their catalogue types (P1-00 4), each w h cells.
struct HydrologyCells {
    std::span<const std::uint8_t> water;
    std::span<const std::uint8_t> flow_dir;
    std::span<const std::uint32_t> flow_accum;
    std::span<const std::uint8_t> river;
    std::span<const std::uint32_t> lake;
};

// The statistics above. Fails (std::nullopt, with bad_cell set as in
// FlowPaths) when flow_path_lengths does. FatalError if a size is wrong.
struct HydrologyResult {
    std::optional<HydrologyStats> stats;
    std::optional<std::uint64_t> bad_cell;
};
[[nodiscard]] HydrologyResult hydrology_stats(std::span<const std::int16_t> e, const HydrologyCells& hydro,
                                              std::uint32_t w, std::uint32_t h);

// --- Pass statistics (P1-00 10.3, P1-01 9.1; added with P1-01's fourth version) -----

// One row of table `pass` as the report reads it: kind (1 lowered pass, 0
// natural gap), capped, the col cell and the path cells (decoded from the
// u32 little-endian blob), in id order.
struct PassRecord {
    std::int64_t kind = 0;
    std::int64_t capped = 0;
    std::int64_t col_x = 0;
    std::int64_t col_y = 0;
    std::vector<std::uint32_t> path;
};

// The constants of the walkability measure (P1-00 10.3, "Walkable end to
// end"): band radius 19 cells around the path, foot discs of 37 cells at
// its two ends, walkable = not steeper than 35 degrees (S2 <= 50204).
inline constexpr std::int64_t PASS_BAND_R2 = 361;
inline constexpr std::int64_t PASS_FOOT_R2 = 1369;

// True if the pass with path `path` (cell indices of the w x h layer e,
// non-empty, each < w h) is walkable end to end (P1-00 10.3): some cell of
// F_a and some cell of F_b are joined by an 8-path of walkable cells of R.
// FatalError if the path is empty or a cell is outside the grid.
[[nodiscard]] bool pass_walkable(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h,
                                 std::span<const std::uint32_t> path);

// The pass keys of P1-00 10.3 in their order, each "<key> = <value>\n":
// passes, natural_gaps, passes_capped, pass_col_m, passes_walkable.
// FatalError if a col or path cell is outside the grid.
[[nodiscard]] std::string pass_lines(std::span<const std::int16_t> e, std::uint32_t w, std::uint32_t h,
                                     std::span<const PassRecord> passes);

// --- Climate statistics (P1-00 10.3, P1-03 9) ----------------------------------------

// The land percentiles listed in ps (each 0..100) of a climate layer over
// the land cells (e > 0) of e, in the order of ps (kth(values,
// percentile_rank(n, p)), G1 values; computed on an exact histogram). Empty
// if there is no land. FatalError if the sizes differ.
[[nodiscard]] std::vector<std::int64_t> land_layer_percentiles(std::span<const std::int16_t> e,
                                                               std::span<const std::int16_t> values,
                                                               std::span<const std::uint32_t> ps);
[[nodiscard]] std::vector<std::int64_t> land_layer_percentiles(std::span<const std::int16_t> e,
                                                               std::span<const std::uint16_t> values,
                                                               std::span<const std::uint32_t> ps);

// The climate keys of P1-03 9 in their order, each "<key> = <value>\n":
//   temperature_c   land percentiles 0, 10, 50, 90, 100 of temperature, each
//                   metres1 of the value in tenths (e.g. "-14.5"); a single
//                   n/a (for both keys) when there is no land
//   rainfall_mm     land percentiles 0, 10, 50, 90, 100 of rainfall
//   land_below_0c   ratio4(land cells with temperature < 0, land cells)
//   rain_contrast   ratio4(rainfall p90, rainfall p10); n/a when p10 = 0
//                   or there is no land
// FatalError if a size differs from e.
[[nodiscard]] std::string climate_lines(std::span<const std::int16_t> e, std::span<const std::int16_t> temperature,
                                        std::span<const std::uint16_t> rainfall);

// --- Tables of schema 3: climate and biome (P1-03 3, P1-04 3) ----------------------

// The names of the wind codes for wind_toward (P1-00 2.3): "E", "SE", "S",
// "SW", "W", "NW", "N", "NE" for 0..7. FatalError for any other code.
[[nodiscard]] std::string_view wind_code_name(std::int64_t code);

// The rows of table `climate` as report reads them (P1-03 9): std::nullopt
// for an empty table; the wind_toward code of its row otherwise. Returns the
// reason when the table is malformed: more than one row, a row with other
// than 2 integer columns, an id other than 1, a wind_toward outside 0..7.
struct ClimateTableResult {
    std::optional<std::int64_t> wind_toward{};
    std::optional<std::string> problem;
};
[[nodiscard]] ClimateTableResult read_climate_rows(std::span<const std::vector<db::ColumnValue>> rows);

// One row of table `biome` (P1-04 3, B6) as report and render read it.
struct BiomeRecord {
    std::int64_t id = 0;
    std::string key;
    std::string name;
    std::int64_t colour = 0;  // 0xRRGGBB
    std::int64_t water = 0;   // 0 or 1
};

// The rows of table `biome`, checked (reading chosen, as for `pass`): five
// columns, id, colour and water integers, key and name text, id in 1..254,
// colour in 0..0xFFFFFF, water 0 or 1. Returns the records in table order,
// or the reason the first bad row fails ("row <id>: ...").
struct BiomeTableResult {
    std::vector<BiomeRecord> records;
    std::optional<std::string> problem;
};
[[nodiscard]] BiomeTableResult read_biome_rows(std::span<const std::vector<db::ColumnValue>> rows);

// The biome keys of P1-04 9 in their order, each "<key> = <value>\n", for a
// biome layer of w x h cells (u8) and the biome table:
//   biome_share    for each record with water = 0, ascending id:
//                  <key>:<ratio4(cells with that code, cells whose biome
//                  record has water = 0)>, separated by single spaces; n/a
//                  when the table has no record with water = 0
//   biome_patches  the number of 8-connected components (G2, equivalence:
//                  equal biome value) of the cells whose biome record has
//                  water = 0
// Keys are escaped as report text. FatalError if a size is wrong.
[[nodiscard]] std::string biome_lines(std::span<const std::uint8_t> biome, std::uint32_t w, std::uint32_t h,
                                      std::span<const BiomeRecord> records);

// --- The section ------------------------------------------------------------------

// The layers [geography] reads, from a world (null when absent).
struct GeographyLayers {
    const db::LayerGrid* elevation = nullptr;
    const db::LayerGrid* water = nullptr;
    const db::LayerGrid* flow_dir = nullptr;
    const db::LayerGrid* flow_accum = nullptr;
    const db::LayerGrid* river = nullptr;
    const db::LayerGrid* lake = nullptr;
    // The rows of table `pass` (P1-01 9.1), or null when the world has no
    // such table; the pass keys are printed only when it is not null.
    const std::vector<PassRecord>* passes = nullptr;
    // The climate layers (P1-03 3), null when absent.
    const db::LayerGrid* temperature = nullptr;
    const db::LayerGrid* rainfall = nullptr;
    // The wind_toward code of table `climate` (P1-03 9), std::nullopt when
    // the table is empty or absent; printed first of the climate group.
    std::optional<std::int64_t> wind_toward{};
    // The biome layer (P1-04 3) and the rows of table `biome`, null when
    // absent; the biome keys are printed only when both are given.
    const db::LayerGrid* biome = nullptr;
    const std::vector<BiomeRecord>* biomes = nullptr;
    // True when temperature and rainfall are both present.
    [[nodiscard]] bool has_climate() const noexcept { return temperature != nullptr && rainfall != nullptr; }
    // True when the biome layer and the biome table are both present.
    [[nodiscard]] bool has_biomes() const noexcept { return biome != nullptr && biomes != nullptr; }
    // True when water, flow_dir, flow_accum, river and lake are all present.
    [[nodiscard]] bool has_hydrology() const noexcept {
        return water != nullptr && flow_dir != nullptr && flow_accum != nullptr && river != nullptr &&
               lake != nullptr;
    }
};

// Why the layers cannot give a [geography] section, or std::nullopt
// (reading chosen): elevation must be present, valid and i16; when
// has_hydrology(), water, flow_dir and river must be u8 and flow_accum and
// lake u32, each the size of elevation. When any of the five is absent the
// hydrology layers are not used, so they are not checked. When
// has_climate(), temperature must be i16 and rainfall u16, each the size of
// elevation (unchecked otherwise); when has_biomes(), biome must be u8 and
// the size of elevation. Text names the layer.
[[nodiscard]] std::optional<std::string> geography_layers_problem(const GeographyLayers& layers);

// The [geography] section (P1-00 10.3): the line "[geography]" then one
// "<key> = <value>" line per key, in the order of 10.3, each ending in a
// line feed; the hydrology lines only when has_hydrology(), then
// wind_toward when it is set, the climate lines (climate_lines) only when
// has_climate(), then the biome lines (biome_lines) only when has_biomes().
// Values that are
// undefined on the world print as n/a (percentiles with no land or no ocean,
// ratio4 over 0, coast_ratio with no land). Returns std::nullopt with
// *problem set when geography_layers_problem reports one, or when the
// hydrology statistics fail (problem then names the cell, e.g. "layer
// 'flow_dir' does not lead to the ocean from cell (3, 4)").
[[nodiscard]] std::optional<std::string> geography_section(const GeographyLayers& layers, std::string* problem);

}  // namespace worldgen::inspect
