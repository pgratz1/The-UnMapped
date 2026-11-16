#pragma once

// Stage P1-01 elevation (spec docs/specs/p1-01-elevation.md, design v16;
// P1-00 sections 3, 4, 7 and 8).
//
// The stage builds a continent field from warped map-scale noise, an edge
// falloff and metre-scale coast noise (E1, E2), sets the coast by a quantile
// and derives the belt width and the land scale (E3), fixes the ocean (E4),
// lays out mountain belts along a level set of a second noise and lengthens
// or moves them if the world has too little mountain core (E5.1), composes
// heights in metres (E5.2, E5.3, E6, E7), erodes with an implicit
// stream-power solve along random receivers, diffusion and a talus step
// (E8), quantizes to decimetres (E9), conditions depressions so that every
// remaining one is a kept lake (E10) and writes the `elevation` layer (E11:
// i16, decimetres, codec 1).
//
// Every step is also a library function on plain arrays, so the tests of
// P1-01 section 10 can call it alone (the "seams" of 10, interim names).
// Grids of Fx values are std::int64_t raw values; masks are one byte per
// cell, non-zero meaning "in the set". Cell (x, y) has index y * width + x
// (P1-00 2.1). Steps defined only on the N x N map take N (16 <= N <= 2048,
// P1-00 2.1); the steps the spec's hand examples run on small W x H grids
// take a geo::Grid (E4 on a given provisional mask, E5.3's chamfer, E8 and
// its parts, E9, E10).
//
// Every function raises FatalError (P0-01 4.1) on a contract violation:
// N outside [16, 2048], an array of the wrong length, threads outside
// [1, 256], or an overflow that the spec's bounds rule out for valid tuning.
// `threads` only schedules the G6 row blocks of the parallel steps (P1-00
// D10): no result depends on it.

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/noise.hpp"
#include "worldgen/stages/stage.hpp"

namespace worldgen::stages {

// The stage name recorded in meta.stages (P1-00 3).
inline constexpr std::string_view ELEVATION_STAGE = "elevation";
// The layer the stage writes (P1-00 4, P1-01 3).
inline constexpr std::string_view ELEVATION_LAYER = "elevation";

// Smallest and largest map side the library accepts (P1-00 2.1).
inline constexpr std::int32_t ELEVATION_MIN_N = 16;
inline constexpr std::int32_t ELEVATION_MAX_N = 2048;

// Constants of the code (P1-01 4.5).
inline constexpr std::int64_t ELEVATION_H_MAX_M = 2990;  // H_MAX = from_int(2990): largest land height before E9
inline constexpr std::int64_t ELEVATION_RATIO_CAP = 1024;  // RATIO_CAP of rc()
inline constexpr std::int64_t ELEVATION_KEY_MAX = 29990;   // KEY_MAX: largest land key E10 writes, decimetres
inline constexpr std::uint32_t ELEVATION_FINE_SHIFT = 16;  // E10.4 fine units: 2^16 per decimetre
inline constexpr std::int32_t ELEVATION_BISECTION_STEPS = 32;  // E5.1

// --- Tuning (P1-01 section 4.2) ---------------------------------------------------

// The fields of definition `worldgen.elevation`, in the order of P1-01 4.2.
struct ElevationTuning {
    Fx land_fraction;
    Fx edge_margin_frac;
    geo::FbmMap continent_warp;
    Fx continent_warp_amp_frac;
    geo::FbmMap continent_noise;
    Fx continent_noise_amp;
    Fx continent_center_jitter;
    Fx edge_falloff_frac;
    Fx edge_falloff_depth;
    Fx edge_noise_amp;
    Fx edge_radial_start;
    Fx edge_radial_end;
    geo::FbmM coast_noise;
    Fx coast_noise_amp;
    std::int64_t island_min_area_m2 = 0;
    geo::FbmMap belt_noise;
    Fx belt_halfwidth_m;
    Fx belt_halfwidth_frac;
    geo::FbmMap segment_noise;
    Fx segment_contrast;
    Fx segment_bias;
    Fx segment_on_share;
    Fx segment_floor_mask;
    std::int32_t belt_offset_steps = 0;
    Fx floor_inland_gm;
    Fx mountain_coast_t;
    geo::FbmM relief_warp;
    Fx relief_warp_amp_m;
    geo::FbmM ridge_noise;
    Fx ridge_weight;
    Fx foothill_fraction;
    Fx mountain_height_m;
    Fx mountain_peak_rank;
    Fx mountain_cover_height_m;
    Fx mountain_cover_rank;
    Fx lowland_height_m;
    geo::FbmM basin_noise;
    Fx basin_amp_m;
    geo::FbmM hill_noise;
    geo::FbmM roughness_noise;
    Fx roughness_min;
    Fx hill_height_m;
    Fx hill_coast_min;
    geo::FbmM detail_noise;
    Fx detail_amp_m;
    Fx ocean_depth_m;
    Fx ocean_shelf_m;
    std::int32_t erosion_iterations = 0;
    std::int32_t erosion_route_interval = 1;
    Fx erosion_k;
    Fx erosion_diffusion;
    Fx talus_slope;
    Fx talus_rate;
    Fx land_floor_m;
    std::int64_t lake_min_area_m2 = 0;
    std::int64_t lake_max_area_m2 = 0;
    std::int32_t lake_area_ref_n = 0;
    Fx lake_min_depth_m;
    Fx lake_max_share;
    Fx fill_slope_dm;
    geo::FbmM fill_noise;
    Fx fill_cost_amp;
    Fx fill_detail_m;

    friend bool operator==(const ElevationTuning&, const ElevationTuning&) = default;
};

// The tuning held by a validated `worldgen.elevation` record (P1-01 4.2).
// Fatal if a field is missing or of another kind.
[[nodiscard]] ElevationTuning elevation_tuning_from(const data::Record& record);
// The tuning of definition worldgen.elevation in loaded content. Fatal if
// the content has no such definition.
[[nodiscard]] ElevationTuning elevation_tuning_from(const data::ContentSet& content);

// --- Derived integers (P1-01 4.5; worked examples in 10.1) ----------------------------

// margin M = max(1, (edge_margin_frac.raw * N) >> 32). Fatal unless
// 1 <= N <= 2048 and 0 <= edge_margin_frac <= ONE.
[[nodiscard]] std::int32_t elevation_margin(Fx edge_margin_frac, std::int32_t n);
// I_min = (island_min_area_m2 + 255) div 256. Fatal unless 0 <= a <= 10^9.
[[nodiscard]] std::int64_t island_min_cells(std::int64_t island_min_area_m2);
// lake_min_cells = (lake_min_area_m2 + 255) div 256. Fatal as above.
[[nodiscard]] std::int64_t lake_min_cells(std::int64_t lake_min_area_m2);
// lake_max_cells: a = (lake_max_area_m2 * N^2) div lake_area_ref_n^2, then
// (a + 255) div 256, all int64. Fatal unless 0 <= area <= 10^9,
// 16 <= ref <= 2048 and 1 <= N <= 2048.
[[nodiscard]] std::int64_t lake_max_cells(std::int64_t lake_max_area_m2, std::int32_t lake_area_ref_n,
                                          std::int32_t n);
// lake_min_depth_dm = mul_div(lake_min_depth_m.raw, 10, 2^32) (RHA). Fatal
// outside [0, 100] m.
[[nodiscard]] std::int64_t lake_min_depth_dm(Fx lake_min_depth_m);
// fine_slope = mul_div(fill_slope_dm.raw, 2^16, 2^32): decimetres per cell
// in fine units. Fatal outside [0, 10].
[[nodiscard]] std::int64_t fill_fine_slope(Fx fill_slope_dm);
// hw = min(belt_halfwidth_m, mul(belt_halfwidth_frac, from_int(16 N))).
[[nodiscard]] Fx belt_halfwidth(Fx belt_halfwidth_m, Fx belt_halfwidth_frac, std::int32_t n);
// target = round_int(mul(from_int(L_O), segment_on_share)). Fatal unless
// 0 <= L_O <= 2^24.
[[nodiscard]] std::int64_t mountain_floor_target(std::int64_t land_cells, Fx segment_on_share);
// max_share_cells = max(1, round_int(mul(from_int(L_O), lake_max_share))).
// Fatal unless 0 <= L_O <= 2^24.
[[nodiscard]] std::int64_t lake_max_share_cells(std::int64_t land_cells, Fx lake_max_share);

// --- E1: coordinates and continent centre --------------------------------------------

// U = from_ratio(2k + 1, 2N), the map-fraction coordinate of row or column k.
[[nodiscard]] Fx elevation_map_fraction(std::int32_t k, std::int32_t n);
// X = from_int(16k + 8), the metre coordinate of row or column k.
[[nodiscard]] Fx elevation_metres(std::int32_t k);
// J = HALF + mul(jitter, from_raw((hv - 2^31) * 2)) for hv in [0, 2^32).
// Fatal if hv >= 2^32.
[[nodiscard]] Fx continent_centre_coordinate(std::uint64_t hv, Fx continent_center_jitter);
// (cx, cy): J for hv_k = hash_at(K_continent, k, 0) >> 32, k = 0 and 1.
struct ContinentCentre {
    Fx cx;
    Fx cy;
};
[[nodiscard]] ContinentCentre continent_centre(std::uint64_t seed, Fx continent_center_jitter);

// --- E2, E3: pass A and thresholds ---------------------------------------------------

// The results of E1 to E3 (P1-01 10, `elevation_fields`).
struct ElevationFields {
    std::int32_t n = 0;
    std::vector<std::int64_t> c;   // continent field, raw, per cell
    std::vector<std::int64_t> nb;  // belt noise (signed), raw, per cell
    std::vector<std::int64_t> ns;  // segment noise, raw, per cell
    ContinentCentre centre;        // E1
    Fx c_sea;                      // E3.1: quantile(c, ONE - land_fraction)
    Fx g_med;                      // E3.3: median belt-noise gradient, >= raw 1
    Fx hw;                         // E3.4: belt half-width in metres
    Fx w_b;                        // E3.5: belt half-width in units of nb, >= raw 1
    Fx s_land;                     // E3.6: median of c - c_sea over c > c_sea, >= raw 1
};

[[nodiscard]] ElevationFields elevation_fields(std::uint64_t seed, std::int32_t n, const ElevationTuning& tuning,
                                               std::uint32_t threads = 1);

// E3.2 on any grid: g[x, y] = hypot(gx, gy).raw with one-sided differences
// at the grid edge (D2, G6 row blocks of the grid).
[[nodiscard]] std::vector<std::int64_t> belt_gradient(geo::Grid grid, std::span<const std::int64_t> nb,
                                                      std::uint32_t threads = 1);
// E3.5: w_b = max(from_raw(1), div_int(mul(g_med, hw), 16)).
[[nodiscard]] Fx belt_width(Fx g_med, Fx hw);

// --- E4: coast and ocean --------------------------------------------------------------

// E4 steps 2 and 3 on any W x H grid and provisional ocean P: O = the
// 4-connected components of P that touch the grid edge; then every
// 8-connected land component with fewer than island_min cells joins O.
// Returns O (1 = ocean). Fatal "elevation: no land" if no land component
// has at least island_min cells.
[[nodiscard]] std::vector<std::uint8_t> elevation_ocean_from_provisional(geo::Grid grid,
                                                                         std::span<const std::uint8_t> provisional,
                                                                         std::int64_t island_min);

// E4 on the map: P = c <= c_sea or within the margin M, then the above with
// I_min of the tuning.
[[nodiscard]] std::vector<std::uint8_t> elevation_classify(const ElevationFields& fields,
                                                           const ElevationTuning& tuning);

// --- E5: coast factor, mask, mountain floor ------------------------------------------

// The coast and lowland profile of a land cell, for s = c - c_sea:
// tl = rc(abs(s), s_land); gm = fade(clamp(div(tl, mountain_coast_t), 0,
// ONE)); co = div(tl, ONE + tl).
struct LandProfile {
    Fx tl;
    Fx gm;
    Fx co;
};
[[nodiscard]] LandProfile land_profile(Fx s, Fx s_land, Fx mountain_coast_t);
// rc(a, b) = from_int(1024) if a >= mul_int(b, 1024), else div(a, b)
// (a >= 0, b > 0).
[[nodiscard]] Fx ratio_capped(Fx a, Fx b);
// b_o = 0 if a = abs(nb - o) >= w_b, else fade(ONE - div(a, w_b)).
[[nodiscard]] Fx belt_profile(Fx nb, Fx offset, Fx w_b);
// sg = fade(clamp((HALF + bias) + mul(segment_contrast, ns), 0, ONE)).
[[nodiscard]] Fx segment_factor(Fx ns, Fx bias, Fx segment_contrast);
// mask(o, beta) = mul(mul(b_o, sg_beta), gm).
[[nodiscard]] Fx mountain_mask(Fx nb, Fx ns, Fx gm, Fx offset, Fx bias, Fx w_b, Fx segment_contrast);

// One land cell as seen by the mountain floor (E5.1).
struct FloorCell {
    Fx nb;
    Fx ns;
    Fx gm;
};

// The result of E5.1.
struct MountainFloor {
    std::int64_t target = 0;       // round_int(mul(from_int(L_O), segment_on_share))
    bool triggered = false;        // core(0, segment_bias) < target
    Fx offset;                     // o*
    Fx bias;                       // beta*
    std::vector<Fx> bisection;     // the mid of every bisection step, in order (empty if none ran)
};

// E5.1 (P1-01 10, the floor seam) over the given land cells (L_O =
// cells.size()) with the belt width w_b and the tuning's segment_contrast,
// segment_bias, segment_on_share, segment_floor_mask, belt_offset_steps and
// floor_inland_gm. Counts are exact; the search order and the 32-step
// bisection are those of the spec.
[[nodiscard]] MountainFloor mountain_floor(std::span<const FloorCell> cells, Fx w_b, const ElevationTuning& tuning);

// --- E5.2 to E7: heights before erosion ------------------------------------------------

// The results of E5 to E7 (P1-01 10, `elevation_heights`).
struct ElevationHeights {
    std::vector<std::int64_t> h;     // E7: Fx raw metres per cell (land and ocean)
    std::vector<std::int64_t> unit;  // E5.2: mask x profile per cell (0 on ocean)
    MountainFloor floor;             // E5.1
    std::int64_t land_cells = 0;     // L_O
    Fx q;                            // E6: quantile of unit at mountain_peak_rank
    Fx qc;                           // E6: quantile of unit at mountain_cover_rank
    Fx scale;                        // E6
};

[[nodiscard]] ElevationHeights elevation_heights(std::uint64_t seed, const ElevationFields& fields,
                                                 std::span<const std::uint8_t> ocean, const ElevationTuning& tuning,
                                                 std::uint32_t threads = 1);

// E5.3's chamfer (P1-02 H7) on any grid: distance in fifths of a cell from
// the cells with source != 0; INF = 2^40 where no source exists (D4).
[[nodiscard]] std::vector<std::int64_t> chamfer_distance(geo::Grid grid, std::span<const std::uint8_t> source);
// E5.3 for one ocean cell at chamfer distance dl (>= 1) from land: h =
// -(HALF + mul(ocean_depth_m, div(dist_m, dist_m + ocean_shelf_m))) with
// dist_m = div_int(from_int(16 min(dl, 2^20)), 5).
[[nodiscard]] Fx ocean_depth(std::int64_t dl, Fx ocean_shelf_m, Fx ocean_depth_m);
// E6 on the multiset of land units: returns {q, qc, scale}. Fatal if empty.
struct MountainScale {
    Fx q;
    Fx qc;
    Fx scale;
};
[[nodiscard]] MountainScale mountain_scale(std::span<const std::int64_t> land_units, const ElevationTuning& tuning);

// --- E8: erosion ----------------------------------------------------------------------------

// The erosion tuning of E8 (so that the hand examples of 10.1 can set it).
struct ErosionParams {
    std::int32_t iterations = 0;      // erosion_iterations, 0 to 200
    std::int32_t route_interval = 1;  // erosion_route_interval, 1 to 64
    Fx k;                             // erosion_k
    Fx diffusion;                     // erosion_diffusion
    Fx floor;                         // land_floor_m, > 0
    Fx talus_drop_o;                  // D_o = mul_int(talus_slope, 16)
    Fx talus_drop_d;                  // D_d = mul(D_o, SQRT2)
    Fx talus_rate;                    // talus_rate
    std::uint64_t key = 0;            // K_erosion, the key of the random receivers
};
// The parameters of the tuning, with K_erosion = stream_key(seed,
// "geo.elevation.erosion").
[[nodiscard]] ErosionParams erosion_params(const ElevationTuning& tuning, std::uint64_t seed);

// E8.2: random receivers (D1, G6 row blocks; reads level, writes rcv). For
// each land cell with a lower neighbour: weights q^2 (diagonal) or 2 q^2
// (orthogonal), q = (L[c] - L[m]) >> 16; r = hash_at(key, c, it) mod W; the
// first code whose running sum exceeds r. W = 0 keeps rcv[c].
void erosion_random_receivers(geo::Grid grid, std::span<const std::int64_t> level,
                              std::span<const std::uint8_t> ocean, std::uint64_t key, std::int32_t it,
                              std::span<geo::Direction> rcv, std::uint32_t threads = 1);
// E8.3 factors (D1): F = mul(k, from_raw(isqrt_u64(A << 32) << 16)), times
// INV_SQRT2 for a diagonal receiver; 0 on ocean. Writes f.
void erosion_factors(geo::Grid grid, std::span<const std::uint32_t> accum, std::span<const geo::Direction> rcv,
                     std::span<const std::uint8_t> ocean, Fx k, std::span<std::int64_t> f, std::uint32_t threads = 1);
// E8.4 implicit solve (D4, in `order`, every receiver before its donors).
// Writes hn.
void erosion_solve(geo::Grid grid, std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean,
                   std::span<const geo::Direction> rcv, std::span<const std::int64_t> f,
                   std::span<const geo::CellIndex> order, Fx floor, std::span<std::int64_t> hn);
// E8.5 diffusion (D2; reads hn, writes h).
void erosion_diffuse(geo::Grid grid, std::span<const std::int64_t> hn, std::span<const std::uint8_t> ocean,
                     Fx diffusion, Fx floor, std::span<std::int64_t> h, std::uint32_t threads = 1);
// E8.6 talus step (D2; reads h, writes t).
void erosion_talus(geo::Grid grid, std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean,
                   Fx drop_o, Fx drop_d, Fx rate, Fx floor, std::span<std::int64_t> t, std::uint32_t threads = 1);
// E8 in full (P1-01 10, `erode`) on any grid with at least one ocean cell.
// h (Fx raw metres) is updated in place; every land height must be >= 1 raw
// (G3).
void erode(geo::Grid grid, std::span<std::int64_t> h, std::span<const std::uint8_t> ocean,
           const ErosionParams& params, std::uint32_t threads = 1);

// --- E9: quantization ------------------------------------------------------------------

// q = mul_div(h_raw, 10, 2^32) (RHA); ocean: min(-1, q); land: max(1, q).
// Fatal if the result does not fit i16.
[[nodiscard]] std::int16_t quantize_elevation_cell(std::int64_t h_raw, bool ocean);
[[nodiscard]] std::vector<std::int16_t> quantize_elevation(geo::Grid grid, std::span<const std::int64_t> h,
                                                           std::span<const std::uint8_t> ocean,
                                                           std::uint32_t threads = 1);

// --- E10: depression conditioning --------------------------------------------------------

// E10.0 for one land cell: cost = ONE + mul(fill_cost_amp, clamp(HALF + nz,
// 0, ONE)) (Fx); bump = mul_div(mul(fill_detail_m, max(0, dn)).raw,
// 10 * 2^16, 2^32) (fine units).
[[nodiscard]] Fx fill_cost(Fx nz, Fx fill_cost_amp);
[[nodiscard]] std::int64_t fill_bump(Fx dn, Fx fill_detail_m);
// E10.4's eps on a depressed cell before the bump: max(1, mul_div(fine_slope,
// ce.raw, 2^32)) with ce = cost or mul(cost, SQRT2) for a diagonal step.
[[nodiscard]] std::int64_t fill_slope_eps(std::int64_t fine_slope, Fx cost, bool diagonal);

// The cost and bump grids of E10.0 on the map (D1, G6 row blocks): land
// cells as above, ocean cells cost = ONE and bump = 0.
struct FillGrids {
    std::vector<std::int64_t> cost;  // Fx raw
    std::vector<std::int64_t> bump;  // fine units
};
[[nodiscard]] FillGrids fill_grids(std::uint64_t seed, std::int32_t n, std::span<const std::uint8_t> ocean,
                                   const ElevationTuning& tuning, std::uint32_t threads = 1);

// One depression component of E10.2.
struct DepressionComponent {
    std::uint32_t label = 0;  // G2 label, 1, 2, ...
    std::int64_t size = 0;
    std::int64_t depth = 0;   // decimetres
    std::int64_t volume = 0;  // decimetre-cells
};

// The lake parameters of E10.3 and E10.4.
struct ConditionParams {
    std::int64_t lake_min_cells = 0;
    std::int64_t lake_max_cells = 0;
    std::int64_t lake_min_depth_dm = 0;
    std::int64_t max_share_cells = 1;  // >= 1
    std::int64_t fine_slope = 0;       // >= 0
};

// E10.3 on given components: the candidates (lake_min_cells <= size <=
// lake_max_cells and depth >= lake_min_depth_dm) in (volume descending,
// label ascending) order, each kept if the kept total stays within
// max_share_cells. Returns the kept labels in that order.
[[nodiscard]] std::vector<std::uint32_t> select_lakes(std::span<const DepressionComponent> components,
                                                      const ConditionParams& params);

// The result of E10.1 to E10.5.
struct Conditioned {
    std::vector<std::int16_t> e;                     // e' per cell
    std::vector<DepressionComponent> components;     // E10.2, by label
    std::vector<std::uint32_t> kept;                 // E10.3, in selection order
};

// E10.1 to E10.5 on any W x H grid with at least one ocean cell (P1-01 10,
// the conditioning seam): e the E9 grid (every non-ocean e >= 1), ocean the
// mask O, cost (Fx raw) and bump (fine units, >= 0) per cell.
[[nodiscard]] Conditioned condition_depressions(geo::Grid grid, std::span<const std::int16_t> e,
                                                std::span<const std::uint8_t> ocean,
                                                std::span<const std::int64_t> cost,
                                                std::span<const std::int64_t> bump, const ConditionParams& params);

// --- The whole stage ------------------------------------------------------------------------

// E1 to E10 on the N x N map (P1-01 10, `elevation_grid`): the cells of the
// `elevation` layer (decimetres, i16), index y * N + x.
[[nodiscard]] std::vector<std::int16_t> elevation_grid(std::uint64_t seed, std::int32_t n,
                                                       const ElevationTuning& tuning, std::uint32_t threads = 1);

// The stage entry point (P1-01 E11, P1-00 3): reads worldgen.elevation from
// the context's content, runs elevation_grid with the context's seed, map
// size and thread count, and writes layer `elevation` (i16, N x N, codec 1).
void run_elevation(StageContext& context);

}  // namespace worldgen::stages
