#pragma once

// Stage P1-04 biomes (spec docs/specs/p1-04-biomes.md; P1-00 sections 3,
// 4, 6.6, 7.7 and 7.8).
//
// One biome code per cell from the biome definitions (data kind `biome`)
// and the tuning `worldgen.biomes`. Steps B1 to B6: preconditions (B1),
// height above drainage with the minimum river class and the cell slope
// (B2, G7 and G8), the 3-4 chamfer distance to the larger rivers (B2b), the
// share of wet ground in a triangle window (B2c), the jittered temperature and rainfall the rules see
// (B3), the priority-ordered rule lookup with the per-size rainfall scale
// (B4), river cells taking the most common biome of their dry neighbours
// (B5), and the layer `biome` and table `biome` written in one stage
// transaction (B6).
//
// Every step is a library function on plain arrays and any W x H grid, so
// that the tests of P1-04 section 10 can call it alone (the interim seams
// of 10). Cell (x, y) has index y * width + x.
//
// Every function raises FatalError (P0-01 4.1) on a contract violation: an
// array of the wrong length, threads outside [1, 256], a violated
// precondition. `threads` only schedules the G6 row blocks of B2c to B5
// (P1-00 D10): no result depends on it.

#include <cstdint>
#include <span>
#include <string>
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
inline constexpr std::string_view BIOMES_STAGE = "biomes";

// The layer the stage writes (P1-04 3; P1-00 4, layer id 11 in a full run).
inline constexpr std::string_view BIOME_LAYER = "biome";

// "No upper limit" of a rule's rainfall bound: never scaled (P1-04 4.1, B4).
inline constexpr std::int64_t BIOME_RAIN_UNBOUNDED_MM = 10000;

// The schema extremes of the biome_rule fields (P1-04 4.1). A bound equal to
// the extreme of its field's range means "unbounded on that side": B4 skips
// that comparison (temp_min_c -60, temp_max_c 50, rain_min_mm 0, rain_max_mm
// 10000, elev_min_m 0, elev_max_m 3000, slope_max 100, hand_max_m 3000,
// coast_max_m 32767, river_dist_max_m 32767, wet_frac_min 0). Every seam that takes rules (biome_rule_matches,
// biome_lookup, biome_layer) is fatal, "biomes: rule: <field> outside its
// schema range", for a field outside these ranges (checked in B4's order:
// land definitions in descending priority, rules in array order, fields in
// declaration order; the first failure is reported), and "biomes: rule: the
// rain scale is outside [0.25, 4]" or "biomes: rule: the wet scale is
// outside [0.25, 4]" for such a scale.
inline constexpr std::int64_t BIOME_TEMP_MIN_C = -60;
inline constexpr std::int64_t BIOME_TEMP_MAX_C = 50;
inline constexpr std::int64_t BIOME_ELEV_MAX_M = 3000;
inline constexpr std::int64_t BIOME_SLOPE_MAX = 100;
inline constexpr std::int64_t BIOME_HAND_MAX_M = 3000;
inline constexpr std::int64_t BIOME_COAST_MAX_M = 32767;
inline constexpr std::int64_t BIOME_RIVER_DIST_MAX_M = 32767;

// B2b: the distance of a cell with no target river (P1-04 B2b), 2^40.
inline constexpr std::int64_t BIOME_D34_FAR = std::int64_t{1} << 40;

// B2c: the bound of wet_window_m (P1-04 4.2) and so of the seam's r_cells
// (2000 div 16 = 125), and of the grid of biome_wet_share (W x H <= 2^31),
// which keep every sum and product of B2c inside int64 (P1-04 7).
inline constexpr std::int64_t BIOME_WET_WINDOW_MAX_M = 2000;
inline constexpr std::int64_t BIOME_WET_R_CELLS_MAX = 125;
inline constexpr std::int64_t BIOME_WET_GRID_CELLS_MAX = std::int64_t{1} << 31;

// --- Definitions (P1-04 4.1) -------------------------------------------------------------

// What hydrology water cells a biome is given to (P1-04 4.1 water_body).
enum class WaterBody : std::uint8_t { none = 0, lake = 1, ocean = 2 };

// One record `biome_rule` (P1-04 4.1). Each condition holds when the value
// lies in the closed interval.
struct BiomeRule {
    Fx temp_min_c;
    Fx temp_max_c;
    std::int64_t rain_min_mm = 0;
    std::int64_t rain_max_mm = 0;
    std::int64_t elev_min_m = 0;
    std::int64_t elev_max_m = 0;
    Fx slope_max;
    Fx hand_max_m;
    std::int64_t coast_max_m = 0;
    std::int64_t river_dist_max_m = 0;  // B2b distance bound, metres: 16 d34 <= 3 bound
    Fx wet_frac_min;                    // B2c share minimum, scaled by the wet scale

    friend bool operator==(const BiomeRule&, const BiomeRule&) = default;
};

// One biome definition (P1-04 4.1), as loaded.
struct BiomeDef {
    std::string key;  // the definition id, e.g. "temperate_forest"
    std::int64_t code = 0;
    std::string name;
    std::int64_t colour = 0;  // 0xRRGGBB
    bool water = false;
    WaterBody water_body = WaterBody::none;
    std::int64_t priority = 0;
    bool fallback = false;
    std::vector<BiomeRule> rules;
    Fx fertility;
    Fx timber;
    Fx forage;  // P1-05 4.1 (generator 0.10.0); read by the resources stage only
    std::string region_noun;

    friend bool operator==(const BiomeDef&, const BiomeDef&) = default;
};

// The loaded biome definitions: every definition in ascending code, and the
// lookup order of B4.
struct BiomeDefinitions {
    std::vector<BiomeDef> biomes;          // ascending code (the order of table biome, B6)
    std::vector<std::size_t> land_order;   // indices of land definitions with priority > 0, descending priority
    std::uint8_t ocean_code = 0;           // the code of the definition with water_body "ocean"
    std::uint8_t lake_code = 0;            // ... "lake"
    std::uint8_t fallback_code = 0;        // the code of the fallback definition

    friend bool operator==(const BiomeDefinitions&, const BiomeDefinitions&) = default;
};

// The definitions of a list of validated `biome` definitions (any order),
// sorted by code. Fatal if the named checks of P1-04 4.1 would fail (codes
// not unique, no single ocean, lake or fallback, priorities of land biomes
// not distinct): the loader rejects such data, so this guards hand-built
// input only.
[[nodiscard]] BiomeDefinitions biome_definitions_from(std::span<const data::Definition> definitions);
// The definitions of kind `biome` in loaded content. Fatal if the content
// has no such kind or the definitions fail as above.
[[nodiscard]] BiomeDefinitions biome_definitions_from(const data::ContentSet& content);

// --- Tuning (P1-04 4.2) ------------------------------------------------------------------

// The fields of definition `worldgen.biomes`, in schema declaration order.
struct BiomeTuning {
    geo::FbmM jitter_noise;
    Fx temp_jitter_c;
    Fx rain_jitter;
    Fx rain_scale_512;
    Fx rain_scale_1024;
    Fx rain_scale_2048;
    std::int64_t drain_river_class_min = 0;
    std::int64_t river_dist_class_min = 0;  // B2b targets: river cells of at least this class
    std::int64_t wet_window_m = 0;          // B2c: r = wet_window_m div 16, h = r div 2
    Fx wet_hand_m;                          // B2c: wet when not ocean and hand <= this (metres)
    Fx wet_scale_512;
    Fx wet_scale_1024;
    Fx wet_scale_2048;

    friend bool operator==(const BiomeTuning&, const BiomeTuning&) = default;
};

// The tuning held by a validated `worldgen.biomes` record. Fatal if a field
// is missing or of another kind.
[[nodiscard]] BiomeTuning biome_tuning_from(const data::Record& record);
// The tuning of definition worldgen.biomes in loaded content. Fatal if the
// content has no such definition.
[[nodiscard]] BiomeTuning biome_tuning_from(const data::ContentSet& content);

// B4's size rule: rain_scale_512 if n <= 512, rain_scale_1024 if 512 < n <=
// 1024, otherwise rain_scale_2048. Test grids (n < 512) use the 512 entry.
// For a W x H grid n = max(W, H) (the side N of a square map).
// Fatal if n < 1.
[[nodiscard]] Fx biome_rain_scale(const BiomeTuning& tuning, std::int64_t n);

// The same size rule for the wet scale ws (B4): wet_scale_512, _1024 or
// _2048. Fatal if n < 1.
[[nodiscard]] Fx biome_wet_scale(const BiomeTuning& tuning, std::int64_t n);

// rb(b) of B4: from_int(10000) if b = 10000, otherwise mul_int(scale, b).
[[nodiscard]] Fx biome_rain_bound(Fx scale, std::int64_t bound_mm);

// --- B2b: river distance -------------------------------------------------------------------

// B2b (sequential, exact): d34 of every cell, the 3-4 chamfer distance to the
// nearest target, a cell with water = 1 and river >= k (two raster passes;
// equal to min over targets t of 3 max(|dx|, |dy|) + min(|dx|, |dy|)), and
// BIOME_D34_FAR (2^40) on every cell when the grid has no target. Fatal if k
// is outside 1..15 or an array does not have grid.cells() values.
[[nodiscard]] std::vector<std::int64_t> biome_river_distance(geo::Grid grid, std::span<const std::uint8_t> water,
                                                             std::span<const std::uint8_t> river, std::int64_t k);

// --- B2c: wet share ----------------------------------------------------------------------------

// The two sums of B2c for every cell.
struct BiomeWetShare {
    std::vector<std::int64_t> count;  // Box(Box(wet, h), h)
    std::vector<std::int64_t> total;  // Box(Box(1, h), h)
};

// B2c (exact; any split): wet[i] = 1 if water[i] != 3 and from_int(hand[i])
// <= mul_int(wet_hand_m, 10) (hand in dm, as B2), else 0; with h = r_cells
// div 2, count = Box(Box(wet, h), h) and total = Box(Box(1, h), h), each box
// clipped to the grid. Entry guard (P1-04 7), checked before any summation
// or allocation, fatal with the prefix "biomes: input precondition: ", in
// this order: r_cells in 0..125; W x H <= 2^31 (and the grid valid); every
// array of length W x H. `threads` schedules the G6 row blocks of the box
// passes only.
[[nodiscard]] BiomeWetShare biome_wet_share(geo::Grid grid, std::span<const std::uint8_t> water,
                                            std::span<const std::int64_t> hand, Fx wet_hand_m, std::int64_t r_cells,
                                            std::uint32_t threads = 1);

// --- B3: jittered climate ----------------------------------------------------------------

// The temperature and rainfall the rules see (B3).
struct BiomeClimate {
    Fx tq;  // degrees C
    Fx rq;  // mm per year
};

// B3 for one cell from its stored temperature (tenths of a degree C) and
// rainfall (mm) and the two noise values nt, nr:
//   tq = from_ratio(temperature, 10) + mul(temp_jitter_c, nt)
//   rq = mul(from_int(rainfall), max(0, ONE + mul(rain_jitter, nr)))
[[nodiscard]] BiomeClimate biome_jitter_cell(std::int64_t temperature, std::int64_t rainfall, Fx nt, Fx nr,
                                             const BiomeTuning& tuning);

// The two noise values of B3 at cell (x, y): nt = fbm_m(K_tj, X, Y,
// jitter_noise), nr = fbm_m(K_rj, X, Y, jitter_noise), K_tj =
// stream_key(seed, "geo.biomes.temp_jitter"), K_rj = stream_key(seed,
// "geo.biomes.rain_jitter"), X = from_int(16 x + 8), Y = from_int(16 y + 8).
struct BiomeNoise {
    Fx nt;
    Fx nr;
};
[[nodiscard]] BiomeNoise biome_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const BiomeTuning& tuning);

// --- B4: classification -------------------------------------------------------------------

// True if a rule matches (all of, B4; a comparison against a bound at its
// field's schema extreme is skipped): r.temp_min_c <= tq <= r.temp_max_c;
// rb(r.rain_min_mm) <= rq <= rb(r.rain_max_mm); 10 r.elev_min_m <=
// elevation_dm <= 10 r.elev_max_m; S2 x 2^64 <= (320 r.slope_max.raw)^2;
// from_int(hand_dm) <= mul_int(r.hand_max_m, 10); coast_dist <= r.coast_max_m;
// 16 d34 <= 3 r.river_dist_max_m; need x wet_total <= wet_count x 2^32 with
// need = mul(r.wet_frac_min, wet_scale).raw (exact, 128-bit).
// Fatal if s2 < 0, d34 < 0 or not 0 <= wet_count <= wet_total with
// wet_total >= 1, then as the rule-field and scale checks above.
[[nodiscard]] bool biome_rule_matches(const BiomeRule& rule, Fx scale, Fx wet_scale, Fx tq, Fx rq,
                                      std::int64_t elevation_dm, std::int64_t s2, std::int64_t hand_dm,
                                      std::int64_t coast_dist, std::int64_t d34, std::int64_t wet_count,
                                      std::int64_t wet_total);

// B4 for a land or river cell (water 0 or 1): the code of the first land
// definition with priority > 0, in descending priority, with any rule that
// matches; the fallback's code if none does. `scale` is
// biome_rain_scale(tuning, N), `wet_scale` biome_wet_scale(tuning, N); d34
// is B2b's distance (BIOME_D34_FAR without a target), wet_count and
// wet_total B2c's sums. Fatal as biome_rule_matches; every rule field is
// checked before the cell is evaluated.
[[nodiscard]] std::uint8_t biome_lookup(const BiomeDefinitions& definitions, Fx scale, Fx wet_scale, Fx tq, Fx rq,
                                        std::int64_t elevation_dm, std::int64_t s2, std::int64_t hand_dm,
                                        std::int64_t coast_dist, std::int64_t d34, std::int64_t wet_count,
                                        std::int64_t wet_total);

// --- B5: river cells ----------------------------------------------------------------------

// B5 on the finished B4 grid: for each cell with water 1, among its existing
// 8-neighbours with water 0, the code with the largest count, ties to the
// smallest code; a river cell with no dry neighbour keeps its value. Returns
// the new grid (biome itself is not changed). Fatal if a size is wrong.
[[nodiscard]] std::vector<std::uint8_t> biome_river_cells(geo::Grid grid, std::span<const std::uint8_t> water,
                                                          std::span<const std::uint8_t> biome,
                                                          std::uint32_t threads = 1);

// --- The whole stage ------------------------------------------------------------------------

// The eight input layers of P1-04 section 2 on a W x H grid, each
// grid.cells() values, catalogue types.
struct BiomeInputs {
    geo::Grid grid;
    std::span<const std::int16_t> elevation;
    std::span<const std::uint8_t> water;
    std::span<const std::uint16_t> water_depth;
    std::span<const std::uint8_t> flow_dir;
    std::span<const std::uint8_t> river;
    std::span<const std::int16_t> coast_dist;
    std::span<const std::int16_t> temperature;
    std::span<const std::uint16_t> rainfall;
};

// Section 2 preconditions 2 and 3 and B2 to B5 (with B2b and B2c, each only
// when some land rule uses its result) on a W x H grid: the `biome` layer. Fatal ("biomes: input precondition: ...") if an array has the
// wrong length, a water value is outside 0..3, a river value outside 0..15,
// or G7's preconditions fail, or as biome_wet_share's entry guard. The rain
// and wet scales use n = max(W, H) (the side N of a square map).
[[nodiscard]] std::vector<std::uint8_t> biome_layer(const BiomeInputs& inputs, const BiomeDefinitions& definitions,
                                                    const BiomeTuning& tuning, std::uint64_t seed,
                                                    std::uint32_t threads = 1);

// The rows of table `biome` (B6): one per definition in ascending code, id =
// code, key = definition id, name, colour, water 0 or 1.
[[nodiscard]] std::vector<db::BiomeRow> biome_table_rows(const BiomeDefinitions& definitions);

// The stage entry point (P1-04 B6, P1-00 3): reads the biome definitions,
// worldgen.biomes and the eight stored input layers, each N x N with its
// catalogue type (fatal otherwise, "biomes: input precondition: ..."), runs
// biome_layer with meta.seed and writes layer `biome` (codec 1) then the
// rows of table `biome`.
void run_biomes(StageContext& context);

}  // namespace worldgen::stages
