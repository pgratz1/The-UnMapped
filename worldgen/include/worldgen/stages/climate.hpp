#pragma once

// Stage P1-03 climate (spec docs/specs/p1-03-climate.md; P1-00 sections 3,
// 4, 7 and 8).
//
// Annual mean temperature and annual precipitation for every cell, from the
// stored `elevation` (P1-01) and the hydrology layers `water`, `water_depth`
// and `coast_dist` (P1-02). Steps C1 to C7: preconditions and the surface
// S = elevation + water_depth (C1), one prevailing wind per world drawn from
// geo.climate.wind (C2), temperature from latitude, lapse rate and noise
// (C3), the box-smoothed surface the air sees (C4), the moisture sweep along
// the wind (C5), rainfall with noise and the coastal bonus (C6), and the two
// layers written in one stage transaction (C7).
//
// Every step is a library function on plain arrays and any W x H grid, so
// the tests of P1-03 section 10 can call it alone (the interim seams of 10).
// On a W x H grid the latitude V uses the grid height H in place of N
// (P1-03 5). Cell (x, y) has index y * width + x.
//
// Every function raises FatalError (P0-01 4.1) on a contract violation: an
// invalid grid or an array of the wrong length, threads outside [1, 256], a
// violated precondition, or an arithmetic overflow (which the bounds of
// section 4 exclude). `threads` only schedules the G6 row blocks of the
// parallel steps (P1-00 D10): no result depends on it.

#include <array>
#include <cstddef>
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
inline constexpr std::string_view CLIMATE_STAGE = "climate";

// The layers the stage writes, in writing order (P1-03 3, C7; P1-00 4).
inline constexpr std::string_view TEMPERATURE_LAYER = "temperature";
inline constexpr std::string_view RAINFALL_LAYER = "rainfall";

// Catalogue ranges (P1-03 3): temperature in tenths of a degree Celsius,
// rainfall in millimetres per year.
inline constexpr std::int64_t TEMPERATURE_MIN = -600;
inline constexpr std::int64_t TEMPERATURE_MAX = 500;
inline constexpr std::int64_t RAINFALL_MAX = 10000;

// The largest surface S, decimetres (P1-03 section 2 precondition 3; the
// bound behind C4's overflow argument, section 7).
inline constexpr std::int64_t SURFACE_MAX_DM = 32767;

// The bound of temp_offset_max_c (P1-03 4), degrees C, and so of the
// offset off_dc of C2b and of climate.temp_offset_dc, tenths (P1-03 3, 7).
inline constexpr std::int64_t TEMP_OFFSET_MAX_C = 5;
inline constexpr std::int64_t TEMP_OFFSET_MAX_DC = 50;

// Number of wind weights: one per D8 code (P1-03 4).
inline constexpr std::size_t WIND_CODES = 8;

// --- Tuning (P1-03 section 4) ---------------------------------------------------------

// The fields of definition `worldgen.climate`, in schema declaration order.
struct ClimateTuning {
    Fx temp_north_c;     // sea-level annual mean at the north edge (v = 0), degrees C
    Fx temp_south_c;     // the same at the south edge (v = 1)
    Fx lapse_c_per_km;   // cooling per km of height
    geo::FbmM temp_noise;
    Fx temp_noise_amp_c;  // degrees C per unit of temp_noise
    std::array<std::int64_t, WIND_CODES> wind_weights{};  // chance the wind blows toward code k
    std::int64_t smooth_radius_m = 0;      // half-width of the C4 window, metres
    std::int64_t rain_base_mm = 0;         // rain from saturated air over level ground
    std::int64_t rain_oro_mm = 0;          // extra rain per unit of upslope gradient
    std::int64_t moisture_path_mm_km = 0;  // rain along the path that uses up saturated air
    Fx loss_max;                           // cap on the moisture share lost in one step
    std::int64_t ocean_recharge_m = 0;     // open water over which air recharges
    std::int64_t coast_rain_mm = 0;        // rain added at the coast
    std::int64_t coast_rain_reach_m = 0;   // distance inland over which that falls to 0
    geo::FbmM rain_noise;
    Fx rain_noise_amp;            // rainfall x (1 + amp x noise)
    std::int64_t rain_min_mm = 0;  // floor of the stored rainfall
    Fx temp_offset_max_c;          // half-width of the per-world offset of C2b, degrees C, [0, 5]

    friend bool operator==(const ClimateTuning&, const ClimateTuning&) = default;
};

// The tuning held by a validated `worldgen.climate` record. Fatal if a field
// is missing or of another kind.
[[nodiscard]] ClimateTuning climate_tuning_from(const data::Record& record);
// The tuning of definition worldgen.climate in loaded content. Fatal if the
// content has no such definition.
[[nodiscard]] ClimateTuning climate_tuning_from(const data::ContentSet& content);

// --- C1: preconditions and the surface -------------------------------------------------

// P1-03 section 2 on a W x H grid: returns S[i] = elevation[i] +
// water_depth[i] (decimetres). Fatal ("climate: input precondition: ...")
// unless every array has grid.cells() values, then (precondition 2) every
// water value is in 0..3, then (precondition 3) S = 0 on every ocean cell
// (water = 3), S >= 1 on every other cell and S <= 32767 everywhere.
[[nodiscard]] std::vector<std::int64_t> climate_surface(geo::Grid grid, std::span<const std::int16_t> elevation,
                                                        std::span<const std::uint8_t> water,
                                                        std::span<const std::uint16_t> water_depth);

// --- C2: prevailing wind ------------------------------------------------------------------

// C2 (D9, sequential): Rng(stream_key(seed, "geo.climate.wind")).
// weighted_index(weights), one call: the D8 code (P1-00 2.3) the wind blows
// toward (0 = toward the east, a westerly). Fatal unless weights has 8
// entries, none negative, with a non-zero sum.
[[nodiscard]] geo::Direction climate_wind(std::uint64_t seed, std::span<const std::int64_t> weights);

// --- C2b: per-world temperature offset -------------------------------------------------

// C2b (D9, sequential; generator 0.9.0): the world's temperature offset in
// tenths of a degree, off_dc. M = round_int(mul_int(temp_offset_max_c, 10));
// 0 if M = 0 (no draw), otherwise the first value of
// Rng(stream_key(seed, "geo.climate.temp_offset")).uniform_i64(-M, M).
// Fatal unless temp_offset_max_c is in [0, 5].
[[nodiscard]] std::int64_t climate_temp_offset_dc(std::uint64_t seed, const ClimateTuning& tuning);

// --- C3: temperature ----------------------------------------------------------------------

// C3 for the cell of row y of a grid of `height` rows: v = V =
// from_ratio(2y + 1, 2 height), s_dm = S, noise = the fbm_m value of
// geo.climate.temperature at the cell (the stage passes fbm_m(K_temp, X, Y,
// temp_noise); tn = mul(temp_noise_amp_c, noise)):
//   t_sea = temp_north_c + mul(temp_south_c - temp_north_c, v)
//   lap   = div_int(mul_int(lapse_c_per_km, s_dm), 10000)
//   off   = from_ratio(offset_dc, 10)       (offset_dc = off_dc of C2b)
//   T     = ((t_sea + off) - lap) + tn
//   temperature = clamp(round_int(mul_int(T, 10)), -600, 500)
// Fatal unless 0 <= y < height <= INT32_MAX and -50 <= offset_dc <= 50.
[[nodiscard]] std::int16_t climate_temperature_cell(std::int64_t y, std::int64_t height, std::int64_t s_dm, Fx noise,
                                                    std::int64_t offset_dc, const ClimateTuning& tuning);

// C3 on a W x H grid (D1, G6 row blocks), the surface S as climate_surface
// returns it, with the offset off_dc of C2b. Fatal unless -50 <= offset_dc <= 50.
[[nodiscard]] std::vector<std::int16_t> climate_temperature(std::uint64_t seed, geo::Grid grid,
                                                            std::span<const std::int64_t> surface,
                                                            std::int64_t offset_dc, const ClimateTuning& tuning,
                                                            std::uint32_t threads = 1);

// --- C4: smoothed surface -----------------------------------------------------------------

// One box pass (pass 1 of C4; D2, G6 row blocks): hs1[i] = from_ratio(sum,
// c_w), the mean of S over the square window of half-width r_cells cells
// around the cell, clipped to the grid (c_w its cell count). r_cells = 0
// gives from_int(S). Fatal if r_cells < 0 or some S is outside [0, 32767].
[[nodiscard]] std::vector<Fx> climate_box_mean(geo::Grid grid, std::span<const std::int64_t> surface,
                                               std::int64_t r_cells, std::uint32_t threads = 1);

// C4, the triangle kernel with r = r_cells: h = r_cells div 2; pass 1 is
// climate_box_mean with half-width h; pass 2 is a box of half-width h over
// the raw values of pass 1, hs = div_int(from_raw(sum2), c_w). r_cells 0 or
// 1 gives from_int(S). Fatal as climate_box_mean.
[[nodiscard]] std::vector<Fx> climate_triangle_mean(geo::Grid grid, std::span<const std::int64_t> surface,
                                                    std::int64_t r_cells, std::uint32_t threads = 1);

// --- C5: moisture sweep ---------------------------------------------------------------------

// The results of C5 for every cell: q the moisture share in [0, ONE], p the
// precipitation in mm/yr.
struct ClimateSweep {
    std::vector<Fx> q;
    std::vector<Fx> p;
};

// C5 (D4; single-threaded) for the wind blowing toward code `wind` (0..7),
// with water (3 = ocean) and the smoothed surface hs of C4. Orthogonal winds
// sweep by lines with the three taps of the previous line; diagonal winds
// sweep in raster order with the taps (x - dx, y), (x - dx, y - dy) and
// (x, y - dy), across the wind (P1-03 C5).
// Off-map taps are saturated air at sea level (q = ONE, hs = 0). Fatal if a
// size is wrong or wind > 7.
[[nodiscard]] ClimateSweep climate_sweep(geo::Grid grid, std::span<const std::uint8_t> water,
                                         std::span<const Fx> hs, geo::Direction wind, const ClimateTuning& tuning);

// --- C6: rainfall ---------------------------------------------------------------------------

// C6 for one cell: p from C5, its coast_dist (metres; ocean negative) and
// rn, the fbm_m value of geo.climate.rainfall at the cell:
//   f  = max(0, ONE + mul(rain_noise_amp, rn)); pr = mul(p, f)
//   d  = max(0, coast_dist)
//   cb = d >= reach ? 0 : from_ratio(coast_rain_mm (reach - d), reach)
//   rainfall = clamp(round_int(pr + cb), rain_min_mm, 10000)
[[nodiscard]] std::uint16_t climate_rainfall_cell(Fx p, std::int64_t coast_dist, Fx rn, const ClimateTuning& tuning);

// C6 on a W x H grid (D1, G6 row blocks).
[[nodiscard]] std::vector<std::uint16_t> climate_rainfall(std::uint64_t seed, geo::Grid grid, std::span<const Fx> p,
                                                          std::span<const std::int16_t> coast_dist,
                                                          const ClimateTuning& tuning, std::uint32_t threads = 1);

// --- The whole stage ------------------------------------------------------------------------

// The two layers of P1-03 section 3, each grid.cells() values.
struct ClimateLayers {
    std::vector<std::int16_t> temperature;
    std::vector<std::uint16_t> rainfall;
};

// C1 to C6 on a W x H grid from the four input layers (P1-03 10): the wind
// is climate_wind(seed, tuning.wind_weights), the offset of C3 is
// climate_temp_offset_dc(seed, tuning) (C2b), C4 is climate_triangle_mean.
[[nodiscard]] ClimateLayers climate_layers(geo::Grid grid, std::span<const std::int16_t> elevation,
                                           std::span<const std::uint8_t> water,
                                           std::span<const std::uint16_t> water_depth,
                                           std::span<const std::int16_t> coast_dist, const ClimateTuning& tuning,
                                           std::uint64_t seed, std::uint32_t threads = 1);

// The stage entry point (P1-03 C7, P1-00 3): reads worldgen.climate and the
// stored elevation (i16), water (u8), water_depth (u16) and coast_dist (i16)
// layers, each N x N with N = param.map_size (fatal otherwise, "climate:
// input precondition: ..."), runs climate_layers with meta.seed and writes
// `temperature` then `rainfall` (codec 1), then the one row of table
// `climate` (id 1, wind_toward = climate_wind(seed, wind_weights),
// temp_offset_dc = climate_temp_offset_dc(seed, tuning); schema 4).
void run_climate(StageContext& context);

}  // namespace worldgen::stages
