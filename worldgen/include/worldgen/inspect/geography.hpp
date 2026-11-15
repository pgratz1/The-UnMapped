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
// MERGE (p1/foundation): G2 connected components of worldgen/geo/ can
// replace the BFS in the implementation.
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
// MERGE (p1/foundation): no shared hull exists in P1-00 section 7; keep.
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
// MERGE (p1/foundation): G1 kth of worldgen/geo/ gives the same values; the
// implementation uses an exact histogram over the i16 range instead.
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
// 16 SQRT2.raw, SQRT2.raw = 6074001000 (P1-00 amendment A1).
// MERGE (p1/foundation): use Fx::SQRT2 from the core once amendment A1 is
// merged.
inline constexpr std::int64_t SQRT2_RAW = 6074001000;
inline constexpr std::int64_t ORTHOGONAL_STEP_RAW = std::int64_t{16} << 32;
inline constexpr std::int64_t DIAGONAL_STEP_RAW = 16 * SQRT2_RAW;

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
//   longest_river_raw    largest D (flow_path_lengths) over river sources, 0
//                        if none; longest_river_m = round_int of it
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

// --- The section ------------------------------------------------------------------

// The layers [geography] reads, from a world (null when absent).
struct GeographyLayers {
    const db::LayerGrid* elevation = nullptr;
    const db::LayerGrid* water = nullptr;
    const db::LayerGrid* flow_dir = nullptr;
    const db::LayerGrid* flow_accum = nullptr;
    const db::LayerGrid* river = nullptr;
    const db::LayerGrid* lake = nullptr;
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
// hydrology layers are not used, so they are not checked. Text names the
// layer.
[[nodiscard]] std::optional<std::string> geography_layers_problem(const GeographyLayers& layers);

// The [geography] section (P1-00 10.3): the line "[geography]" then one
// "<key> = <value>" line per key, in the order of 10.3, each ending in a
// line feed; the hydrology lines only when has_hydrology(). Values that are
// undefined on the world print as n/a (percentiles with no land or no ocean,
// ratio4 over 0, coast_ratio with no land). Returns std::nullopt with
// *problem set when geography_layers_problem reports one, or when the
// hydrology statistics fail (problem then names the cell, e.g. "layer
// 'flow_dir' does not lead to the ocean from cell (3, 4)").
[[nodiscard]] std::optional<std::string> geography_section(const GeographyLayers& layers, std::string* problem);

}  // namespace worldgen::inspect
