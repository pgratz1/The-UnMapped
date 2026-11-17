#pragma once

// Stage P1-04 biomes (spec docs/specs/p1-04-biomes.md; P1-00 sections 3,
// 4, 6.6, 7.7 and 7.8).
//
// One biome code per cell from the biome definitions (data kind `biome`)
// and the tuning `worldgen.biomes`. Steps B1 to B6: preconditions (B1),
// height above drainage with the minimum river class and the cell slope
// (B2, G7 and G8), the jittered temperature and rainfall the rules see
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
// precondition. `threads` only schedules the G6 row blocks of B3 to B5
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
// For a W x H grid the stage passes n = H (the grid's side for a square map).
// Fatal if n < 1.
[[nodiscard]] Fx biome_rain_scale(const BiomeTuning& tuning, std::int64_t n);

// rb(b) of B4: from_int(10000) if b = 10000, otherwise mul_int(scale, b).
[[nodiscard]] Fx biome_rain_bound(Fx scale, std::int64_t bound_mm);

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

// True if a rule matches (all of, B4): r.temp_min_c <= tq <= r.temp_max_c;
// rb(r.rain_min_mm) <= rq <= rb(r.rain_max_mm); 10 r.elev_min_m <=
// elevation_dm <= 10 r.elev_max_m; S2 x 2^64 <= (320 r.slope_max.raw)^2;
// from_int(hand_dm) <= mul_int(r.hand_max_m, 10); coast_dist <= r.coast_max_m.
[[nodiscard]] bool biome_rule_matches(const BiomeRule& rule, Fx scale, Fx tq, Fx rq, std::int64_t elevation_dm,
                                      std::int64_t s2, std::int64_t hand_dm, std::int64_t coast_dist);

// B4 for a land or river cell (water 0 or 1): the code of the first land
// definition with priority > 0, in descending priority, with any rule that
// matches; the fallback's code if none does. `scale` is
// biome_rain_scale(tuning, N).
[[nodiscard]] std::uint8_t biome_lookup(const BiomeDefinitions& definitions, Fx scale, Fx tq, Fx rq,
                                        std::int64_t elevation_dm, std::int64_t s2, std::int64_t hand_dm,
                                        std::int64_t coast_dist);

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

// Section 2 preconditions 2 and 3 and B2 to B5 on a W x H grid: the `biome`
// layer. Fatal ("biomes: input precondition: ...") if an array has the
// wrong length, a water value is outside 0..3, a river value outside 0..15,
// or G7's preconditions fail. The rainfall scale uses n = grid.height (the
// side N of a square map).
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
