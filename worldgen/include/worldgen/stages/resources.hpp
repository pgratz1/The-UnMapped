#pragma once

// Stage P1-05 resources and habitability (spec
// docs/specs/p1-05-resources-habitability.md; P1-00 sections 3, 4, 7.7 and
// 7.8).
//
// Seven score and area layers per cell from the stored geography, the biome
// definitions' fertility, timber and forage values (data kind `biome`) and
// the tuning `worldgen.resources`. Steps R1 to R7: preconditions (R1),
// height above drainage with k = 1 and the cell slope (R2, G7 and G8), the
// two chamfer distances and fresh water (R3), fertility, timber, forage and
// ore (R4), the walk areas (R5), habitability (R6) and the seven layers
// written in one stage transaction (R7).
//
// Every step is a library function on plain arrays and any W x H grid, so
// that the tests of P1-05 section 10 can call it alone. Cell (x, y) has
// index y * width + x.
//
// Every function raises FatalError (P0-01 4.1) on a contract violation: an
// array of the wrong length, threads outside [1, 256], a violated
// precondition, tuning outside its schema bounds or not ordered as the
// named checks of 4.2 require. `threads` only schedules the G6 row blocks of
// R3 to R4 and R6 (P1-00 D10): no result depends on it.

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/value.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/noise.hpp"
#include "worldgen/stages/biomes.hpp"
#include "worldgen/stages/stage.hpp"

namespace worldgen::stages {

// The stage name recorded in meta.stages (P1-00 3).
inline constexpr std::string_view RESOURCES_STAGE = "resources";

// The layers the stage writes, in writing order (P1-05 3; layer ids 12 to
// 18 in a full run).
inline constexpr std::string_view FERTILITY_LAYER = "fertility";
inline constexpr std::string_view ORE_LAYER = "ore";
inline constexpr std::string_view TIMBER_LAYER = "timber";
inline constexpr std::string_view FRESH_WATER_LAYER = "fresh_water";
inline constexpr std::string_view FORAGE_LAYER = "forage";
inline constexpr std::string_view HABITABILITY_LAYER = "habitability";
inline constexpr std::string_view WALK_AREA_LAYER = "walk_area";

// The largest walk-area id (u16 layer, P1-05 R5).
inline constexpr std::int64_t WALK_AREA_MAX_ID = 65535;

// "No source" of a chamfer saturates to this many metres (P1-05 R3).
inline constexpr std::int64_t RESOURCE_DISTANCE_MAX_M = 32767;

// --- Tuning (P1-05 4.2) --------------------------------------------------------------------

// The fields of definition `worldgen.resources`, in schema declaration order.
struct ResourceTuning {
    std::int64_t fresh_reach_m = 0;
    Fx fresh_rain_weight;
    std::int64_t fresh_rain_lo_mm = 0;
    std::int64_t fresh_rain_hi_mm = 0;
    Fx fert_slope_lo;
    Fx fert_slope_hi;
    Fx alluvial_bonus;
    Fx alluvial_hand_m;
    Fx timber_slope_lo;
    Fx timber_slope_hi;
    std::int64_t fish_river_class_min = 0;
    Fx fish_weight;
    std::int64_t fish_reach_m = 0;
    geo::FbmM ore_noise;
    Fx ore_vein_lo;
    Fx ore_vein_hi;
    std::int64_t ore_elev_lo_m = 0;
    std::int64_t ore_elev_hi_m = 0;
    Fx ore_lowland;
    Fx ore_slope_base;
    Fx ore_slope_lo;
    Fx ore_slope_hi;
    Fx walk_slope_max;
    std::int64_t walk_min_area_m2 = 0;
    Fx hab_slope_lo;
    Fx hab_slope_hi;
    std::int64_t hab_weight_fertility = 0;
    std::int64_t hab_weight_forage = 0;
    std::int64_t hab_weight_timber = 0;
    std::int64_t hab_weight_ore = 0;
    Fx hab_water_min;
    Fx hab_flood_hand_m;
    Fx hab_flood_factor;
    Fx hab_pocket_factor;

    friend bool operator==(const ResourceTuning&, const ResourceTuning&) = default;
};

// The tuning held by a validated `worldgen.resources` record. Fatal if a
// field is missing or of another kind.
[[nodiscard]] ResourceTuning resource_tuning_from(const data::Record& record);
// The tuning of definition worldgen.resources in loaded content. Fatal if
// the content has no such definition.
[[nodiscard]] ResourceTuning resource_tuning_from(const data::ContentSet& content);

// Fatal ("resources: tuning: <field> outside its schema range", or the
// named check's message of 4.2 after "resources: tuning: ") unless every
// field lies in its schema range of 4.2 and the named checks hold. The
// loader guarantees both for data; this guards hand-built tuning, and every
// seam that takes a tuning calls it first.
void check_resource_tuning(const ResourceTuning& tuning);

// walk_min_cells of R5: (walk_min_area_m2 + 255) div 256.
[[nodiscard]] std::int64_t resource_walk_min_cells(const ResourceTuning& tuning);

// --- Helpers (P1-05 5) ---------------------------------------------------------------------

// ramp(x, a, b) = x <= a ? 0 : x >= b ? ONE : div(x - a, b - a). Fatal
// unless a < b.
[[nodiscard]] Fx resource_ramp(Fx x, Fx a, Fx b);

// q255(s) = clamp(round_int(mul_int(clamp(s, 0, ONE), 255)), 0, 255).
[[nodiscard]] std::uint8_t resource_q255(Fx s);

// The R3 distance in metres: min(32767, (32 d + 5) div 10) of the P1-02 H7
// chamfer d (weights 5 and 7, INF = 2^40) to the cells with source != 0;
// 32767 on every cell when there is no source. Fatal if a size is wrong.
[[nodiscard]] std::vector<std::int64_t> chamfer_metres(geo::Grid grid, std::span<const std::uint8_t> source);

// --- Per-cell scores (R3, R4, R6) ----------------------------------------------------------
// Each checks the tuning (check_resource_tuning) and its arguments: fatal
// if df, dg, s2 or hand is negative or s2 >= 2^34, or a biome value is
// outside [0, ONE].

// R3: fs = df >= fresh_reach_m ? 0 : from_ratio(fresh_reach_m - df,
// fresh_reach_m); fr = mul(fresh_rain_weight, ramp(from_int(rainfall),
// from_int(fresh_rain_lo_mm), from_int(fresh_rain_hi_mm))); q255(max(fs,
// fr)). df in metres, rainfall in mm (0..65535).
[[nodiscard]] std::uint8_t fresh_water_cell(std::int64_t df, std::int64_t rainfall, const ResourceTuning& tuning);

// R4 fertility: fb the biome's fertility, S2 of G8, hand in decimetres.
[[nodiscard]] std::uint8_t fertility_cell(Fx fb, std::int64_t s2, std::int64_t hand, const ResourceTuning& tuning);

// R4 timber: tb the biome's timber.
[[nodiscard]] std::uint8_t timber_cell(Fx tb, std::int64_t s2, const ResourceTuning& tuning);

// R4 forage: gb the biome's forage, dg the distance to fish water in metres.
[[nodiscard]] std::uint8_t forage_cell(Fx gb, std::int64_t dg, const ResourceTuning& tuning);

// R4 ore: elevation in decimetres (i16 range), noise the ore noise value.
[[nodiscard]] std::uint8_t ore_cell(std::int64_t elevation, std::int64_t s2, Fx noise, const ResourceTuning& tuning);

// The ore noise of R4 at cell (x, y): fbm_m(K_ore, from_int(16 x + 8),
// from_int(16 y + 8), ore_noise), K_ore = stream_key(seed,
// "geo.resources.ore").
[[nodiscard]] Fx resource_ore_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const ResourceTuning& tuning);

// R6 for one land cell from the stored u8 scores (F fertility, W
// fresh_water, G forage, T timber, O ore), S2, hand (dm) and the cell's
// walk_area id. Fatal if a score is outside 0..255 or walk_area < 0.
[[nodiscard]] std::uint8_t habitability_cell(std::int64_t fertility, std::int64_t fresh_water, std::int64_t forage,
                                             std::int64_t timber, std::int64_t ore, std::int64_t s2,
                                             std::int64_t hand, std::int64_t walk_area,
                                             const ResourceTuning& tuning);

// --- R5: walk areas --------------------------------------------------------------------------

// The walk areas of R5 (P1-05 R5): per cell the area id 1..count, 0 where
// the cell is not walkable or in a component below walk_min_cells.
struct WalkAreas {
    std::vector<std::uint16_t> ids;
    std::int64_t count = 0;
};

// R5 on a W x H grid: walkable(c) = water(c) in {0, 1} and S2(c) x 2^64 <=
// (320 walk_slope_max.raw)^2; linked(a, b) = both walkable and (S(a) -
// S(b))^2 x 2^64 <= w (160 walk_slope_max.raw)^2, w = 1 orthogonal, 2
// diagonal (U128); components by G2's FIFO BFS (8-neighbour order); kept if
// at least walk_min_cells cells, numbered 1, 2, ... in order of smallest
// index. surface is S = elevation + water_depth (dm), s2 the G8 values.
// Fatal if a size is wrong, a water value is outside 0..3, an s2 is
// negative, walk_slope_max is outside [0.01, 10], walk_min_cells < 1, or
// more than 65535 areas are kept ("resources: more than 65535 walk areas").
[[nodiscard]] WalkAreas walk_areas(geo::Grid grid, std::span<const std::int64_t> surface,
                                   std::span<const std::uint8_t> water, std::span<const std::int64_t> s2,
                                   Fx walk_slope_max, std::int64_t walk_min_cells);

// --- The whole stage -------------------------------------------------------------------------

// The seven input layers of P1-05 section 2 on a W x H grid, each
// grid.cells() values, catalogue types.
struct ResourceInputs {
    geo::Grid grid;
    std::span<const std::int16_t> elevation;
    std::span<const std::uint8_t> water;
    std::span<const std::uint16_t> water_depth;
    std::span<const std::uint8_t> flow_dir;
    std::span<const std::uint8_t> river;
    std::span<const std::uint16_t> rainfall;
    std::span<const std::uint8_t> biome;
};

// The seven layers of P1-05 section 3, each grid.cells() values.
struct ResourceLayers {
    std::vector<std::uint8_t> fertility;
    std::vector<std::uint8_t> ore;
    std::vector<std::uint8_t> timber;
    std::vector<std::uint8_t> fresh_water;
    std::vector<std::uint8_t> forage;
    std::vector<std::uint8_t> habitability;
    std::vector<std::uint16_t> walk_area;
};

// R1 to R6 on a W x H grid. Fatal, in this order: threads outside [1, 256]
// ("resources: thread count outside [1, 256]"); the tuning
// (check_resource_tuning); a definition's fertility, timber or forage
// outside [0, 1] ("resources: biome '<key>' has a value outside [0, 1]");
// then with the prefix "resources: input precondition: ": the grid (P1-00
// grid checks); "<name> length is not width * height" for the first input
// of the wrong length (order of ResourceInputs); "water value <v> at (<x>,
// <y>)"; "river value <v> at (<x>, <y>)"; "biome value <v> at (<x>, <y>)
// has no definition" (each the lowest such index); G7 ("G7: ...").
[[nodiscard]] ResourceLayers resources_layers(const ResourceInputs& inputs, const BiomeDefinitions& definitions,
                                              const ResourceTuning& tuning, std::uint64_t seed,
                                              std::uint32_t threads = 1);

// The stage entry point (P1-05 R7, P1-00 3): reads the biome definitions,
// worldgen.resources and the seven stored input layers, each N x N with its
// catalogue type (fatal otherwise: "resources: input precondition: missing
// layer '<name>'", "... layer '<name>' does not have its catalogue type",
// "... layer '<name>' is not N x N"), runs resources_layers with meta.seed
// and writes the seven layers (codec 1) in the order of section 3.
void run_resources(StageContext& context);

}  // namespace worldgen::stages
