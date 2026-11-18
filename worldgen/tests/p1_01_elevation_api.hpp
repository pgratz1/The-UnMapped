#pragma once

// The one place where the P1-01 elevation tests touch the stage's API
// (worldgen/stages/elevation.hpp).
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays and raw Fx values. This file maps them onto
// the stage's seams (P1-01 section 10). Tests that check one of the stage's
// small helpers (land_profile, mountain_mask, ocean_depth, fill_cost, ...)
// call worldgen::stages directly.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p1_04_biomes_data.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/stages/elevation.hpp"

namespace elev_api {

using Tuning = worldgen::stages::ElevationTuning;
using worldgen::geo::Grid;

// The hydrology fixture of P1-02 section 4 (needed beside elevation.toml
// when the `worldgen` kind's id set includes `hydrology`, P1-00 6.1).
inline constexpr std::string_view HYDROLOGY_TOML = "[worldgen.hydrology]\nriver_min_area_m2 = 125000\nflat_noise = { wavelength_m = 400, octaves = 1, gain = \"0.5\" }\n";

// The climate fixture of P1-03 section 4 (needed beside the others once the
// id set includes `climate`).
inline constexpr std::string_view CLIMATE_TOML =
    "[worldgen.climate]\n"
    "temp_north_c = 5\n"
    "temp_south_c = 9\n"
    "lapse_c_per_km = \"6.5\"\n"
    "temp_noise = { wavelength_m = 6000, octaves = 3, gain = \"0.5\" }\n"
    "temp_noise_amp_c = 1\n"
    "wind_weights = [1, 1, 1, 1, 1, 1, 1, 1]\n"
    "smooth_radius_m = 320\n"
    "rain_base_mm = 800\n"
    "rain_oro_mm = 2500\n"
    "moisture_path_mm_km = 32000\n"
    "loss_max = \"0.25\"\n"
    "ocean_recharge_m = 500\n"
    "coast_rain_mm = 40\n"
    "coast_rain_reach_m = 1500\n"
    "rain_noise = { wavelength_m = 5000, octaves = 3, gain = \"0.5\" }\n"
    "rain_noise_amp = \"0.4\"\n"
    "rain_min_mm = 250\n"
    "temp_offset_max_c = 2\n";

// The worldgen.biomes fixture (P1-04 4.2, the shipped values; required once
// the `worldgen` kind's id set includes `biomes`).
inline constexpr std::string_view BIOMES_TOML =
    "[worldgen.biomes]\n"
    "jitter_noise = { wavelength_m = 500, octaves = 4, gain = \"0.5\" }\n"
    "temp_jitter_c = 1\n"
    "rain_jitter = \"0.2\"\n"
    "rain_scale_512 = \"1.125\"\n"
    "rain_scale_1024 = 1\n"
    "rain_scale_2048 = \"0.875\"\n"
    "drain_river_class_min = 3\n"
    "river_dist_class_min = 4\n"
    "wet_window_m = 480\n"
    "wet_hand_m = 5\n"
    "wet_scale_512 = 1\n"
    "wet_scale_1024 = 1\n"
    "wet_scale_2048 = \"1.5\"\n";

// The biome definitions of P1-04 4.1 as one file (file names carry no
// meaning, P0-03 7.4; required once the kind `biome` is registered).
inline constexpr std::string_view BIOME_DEFINITIONS_TOML =
    "[biome.ocean]\n"
    "code = 1\n"
    "name = \"Ocean\"\n"
    "colour = 0x183878\n"
    "water = true\n"
    "water_body = \"ocean\"\n"
    "priority = 0\n"
    "fallback = false\n"
    "rules = []\n"
    "fertility = 0\n"
    "timber = 0\n"
    "forage = 0\n"
    "region_noun = \"Sea\"\n"
    "\n"
    "[biome.lake]\n"
    "code = 2\n"
    "name = \"Lake\"\n"
    "colour = 0x4080CC\n"
    "water = true\n"
    "water_body = \"lake\"\n"
    "priority = 0\n"
    "fallback = false\n"
    "rules = []\n"
    "fertility = 0\n"
    "timber = 0\n"
    "forage = 0\n"
    "region_noun = \"Mere\"\n"
    "\n"
    "[biome.temperate_forest]\n"
    "code = 3\n"
    "name = \"Temperate forest\"\n"
    "colour = 0x2E6034\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 20\n"
    "fallback = false\n"
    "fertility = \"0.7\"\n"
    "timber = 1\n"
    "forage = \"0.5\"\n"
    "region_noun = \"Wood\"\n"
    "\n"
    "[[biome.temperate_forest.rules]]\n"
    "temp_min_c = -60\n"
    "temp_max_c = 50\n"
    "rain_min_mm = 720\n"
    "rain_max_mm = 10000\n"
    "elev_min_m = 0\n"
    "elev_max_m = 3000\n"
    "slope_max = 100\n"
    "hand_max_m = 3000\n"
    "coast_max_m = 32767\n"
    "river_dist_max_m = 32767\n"
    "wet_frac_min = 0\n"
    "\n"
    "[biome.grassland]\n"
    "code = 4\n"
    "name = \"Grassland\"\n"
    "colour = 0x96AA5A\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 10\n"
    "fallback = true\n"
    "rules = []\n"
    "fertility = 1\n"
    "timber = \"0.15\"\n"
    "forage = \"0.3\"\n"
    "region_noun = \"Downs\"\n"
    "\n"
    "[biome.marsh]\n"
    "code = 5\n"
    "name = \"Marsh\"\n"
    "colour = 0x6AA396\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 40\n"
    "fallback = false\n"
    "fertility = \"0.45\"\n"
    "timber = \"0.3\"\n"
    "forage = \"0.8\"\n"
    "region_noun = \"Marsh\"\n"
    "\n"
    "[[biome.marsh.rules]]\n"
    "temp_min_c = -60\n"
    "temp_max_c = 50\n"
    "rain_min_mm = 500\n"
    "rain_max_mm = 10000\n"
    "elev_min_m = 0\n"
    "elev_max_m = 200\n"
    "slope_max = \"0.05\"\n"
    "hand_max_m = 1\n"
    "coast_max_m = 32767\n"
    "river_dist_max_m = 32767\n"
    "wet_frac_min = 0\n"
    "\n"
    "[biome.highland_moor]\n"
    "code = 6\n"
    "name = \"Highland moor\"\n"
    "colour = 0x7E6680\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 30\n"
    "fallback = false\n"
    "fertility = \"0.3\"\n"
    "timber = \"0.1\"\n"
    "forage = \"0.25\"\n"
    "region_noun = \"Moor\"\n"
    "\n"
    "[[biome.highland_moor.rules]]\n"
    "temp_min_c = -60\n"
    "temp_max_c = 4\n"
    "rain_min_mm = 720\n"
    "rain_max_mm = 10000\n"
    "elev_min_m = 300\n"
    "elev_max_m = 3000\n"
    "slope_max = 100\n"
    "hand_max_m = 3000\n"
    "coast_max_m = 32767\n"
    "river_dist_max_m = 32767\n"
    "wet_frac_min = 0\n"
    "\n"
    "[biome.alpine]\n"
    "code = 7\n"
    "name = \"Alpine\"\n"
    "colour = 0xBEBEB4\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 50\n"
    "fallback = false\n"
    "fertility = 0\n"
    "timber = 0\n"
    "forage = \"0.05\"\n"
    "region_noun = \"Peaks\"\n"
    "\n"
    "[[biome.alpine.rules]]\n"
    "temp_min_c = -60\n"
    "temp_max_c = \"0.5\"\n"
    "rain_min_mm = 0\n"
    "rain_max_mm = 10000\n"
    "elev_min_m = 600\n"
    "elev_max_m = 3000\n"
    "slope_max = 100\n"
    "hand_max_m = 3000\n"
    "coast_max_m = 32767\n"
    "river_dist_max_m = 32767\n"
    "wet_frac_min = 0\n"
    "\n"
    "[biome.blighted_waste]\n"
    "code = 8\n"
    "name = \"Blighted waste\"\n"
    "colour = 0x605446\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 0\n"
    "fallback = false\n"
    "rules = []\n"
    "fertility = 0\n"
    "timber = 0\n"
    "forage = 0\n"
    "region_noun = \"Waste\"\n"
    "\n"
    "[biome.heath]\n"
    "code = 9\n"
    "name = \"Heath\"\n"
    "colour = 0x94664A\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 25\n"
    "fallback = false\n"
    "fertility = \"0.2\"\n"
    "timber = \"0.05\"\n"
    "forage = \"0.2\"\n"
    "region_noun = \"Heath\"\n"
    "\n"
    "[[biome.heath.rules]]\n"
    "temp_min_c = -60\n"
    "temp_max_c = 4\n"
    "rain_min_mm = 0\n"
    "rain_max_mm = 10000\n"
    "elev_min_m = 300\n"
    "elev_max_m = 3000\n"
    "slope_max = 100\n"
    "hand_max_m = 3000\n"
    "coast_max_m = 32767\n"
    "river_dist_max_m = 32767\n"
    "wet_frac_min = 0\n"
    "\n"
    "[biome.steppe]\n"
    "code = 10\n"
    "name = \"Steppe\"\n"
    "colour = 0xE0D296\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 15\n"
    "fallback = false\n"
    "fertility = \"0.7\"\n"
    "timber = \"0.05\"\n"
    "forage = \"0.35\"\n"
    "region_noun = \"Steppe\"\n"
    "\n"
    "[[biome.steppe.rules]]\n"
    "temp_min_c = -60\n"
    "temp_max_c = 50\n"
    "rain_min_mm = 0\n"
    "rain_max_mm = 600\n"
    "elev_min_m = 0\n"
    "elev_max_m = 3000\n"
    "slope_max = 100\n"
    "hand_max_m = 3000\n"
    "coast_max_m = 32767\n"
    "river_dist_max_m = 32767\n"
    "wet_frac_min = 0\n"
    "\n"
    "[biome.wet_woodland]\n"
    "code = 11\n"
    "name = \"Wet woodland\"\n"
    "colour = 0x23505E\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 18\n"
    "fallback = false\n"
    "fertility = \"0.5\"\n"
    "timber = \"0.7\"\n"
    "forage = \"0.7\"\n"
    "region_noun = \"Carr\"\n"
    "\n"
    "[[biome.wet_woodland.rules]]\n"
    "temp_min_c = -60\n"
    "temp_max_c = 50\n"
    "rain_min_mm = 450\n"
    "rain_max_mm = 10000\n"
    "elev_min_m = 0\n"
    "elev_max_m = 300\n"
    "slope_max = 100\n"
    "hand_max_m = 5\n"
    "coast_max_m = 32767\n"
    "river_dist_max_m = 112\n"
    "wet_frac_min = 0\n"
    "\n"
    "[biome.fen]\n"
    "code = 12\n"
    "name = \"Fen\"\n"
    "colour = 0x8A8070\n"
    "water = false\n"
    "water_body = \"none\"\n"
    "priority = 45\n"
    "fallback = false\n"
    "fertility = \"0.25\"\n"
    "timber = \"0.2\"\n"
    "forage = \"0.6\"\n"
    "region_noun = \"Fen\"\n"
    "\n"
    "[[biome.fen.rules]]\n"
    "temp_min_c = -60\n"
    "temp_max_c = 50\n"
    "rain_min_mm = 500\n"
    "rain_max_mm = 10000\n"
    "elev_min_m = 0\n"
    "elev_max_m = 200\n"
    "slope_max = \"0.05\"\n"
    "hand_max_m = 3\n"
    "coast_max_m = 32767\n"
    "river_dist_max_m = 32767\n"
    "wet_frac_min = \"0.45\"\n";

// worldgen/resources.toml: the shipped values (P1-05 4.2).
inline constexpr std::string_view RESOURCES_TOML =
    "[worldgen.resources]\n"
    "fresh_reach_m = 400\n"
    "fresh_rain_weight = \"0.4\"\n"
    "fresh_rain_lo_mm = 300\n"
    "fresh_rain_hi_mm = 1500\n"
    "fert_slope_lo = \"0.18\"\n"
    "fert_slope_hi = \"0.47\"\n"
    "alluvial_bonus = \"0.15\"\n"
    "alluvial_hand_m = 5\n"
    "timber_slope_lo = \"0.7\"\n"
    "timber_slope_hi = \"1.4\"\n"
    "fish_river_class_min = 4\n"
    "fish_weight = \"0.3\"\n"
    "fish_reach_m = 300\n"
    "ore_noise = { wavelength_m = 1500, octaves = 3, gain = \"0.5\" }\n"
    "ore_vein_lo = 0\n"
    "ore_vein_hi = \"0.2\"\n"
    "ore_elev_lo_m = 100\n"
    "ore_elev_hi_m = 600\n"
    "ore_lowland = \"0.15\"\n"
    "ore_slope_base = \"0.65\"\n"
    "ore_slope_lo = \"0.3\"\n"
    "ore_slope_hi = \"1.0\"\n"
    "walk_slope_max = \"0.7002\"\n"
    "walk_min_area_m2 = 250000\n"
    "hab_slope_lo = \"0.18\"\n"
    "hab_slope_hi = \"0.47\"\n"
    "hab_weight_fertility = 70\n"
    "hab_weight_forage = 15\n"
    "hab_weight_timber = 10\n"
    "hab_weight_ore = 5\n"
    "hab_water_min = \"0.25\"\n"
    "hab_flood_hand_m = \"0.5\"\n"
    "hab_flood_factor = \"0.75\"\n"
    "hab_pocket_factor = \"0.25\"\n";

// Adds what the production registry requires besides the stage tunings
// given by a test: worldgen/biomes.toml when the id `biomes` is registered,
// worldgen/resources.toml when the id `resources` is, and the directory `biomes` with the twelve definitions when the kind
// `biome` is (P1-04 4). The `worldgen` directory must already be in root.
inline void add_biome_fixtures(worldgen::data::MemoryDataRoot& root) {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    const wd::Kind* kind = registry.find("worldgen");
    if (kind != nullptr && kind->schema_for("biomes") != nullptr) {
        for (wd::MemoryDirectory& d : root.directories) {
            if (d.name == "worldgen") {
                d.entries.push_back(wd::MemoryEntry{"biomes.toml", std::string(BIOMES_TOML), false});
            }
        }
    }
    if (kind != nullptr && kind->schema_for("resources") != nullptr) {
        for (wd::MemoryDirectory& d : root.directories) {
            if (d.name == "worldgen") {
                d.entries.push_back(wd::MemoryEntry{"resources.toml", std::string(RESOURCES_TOML), false});
            }
        }
    }
    if (registry.find("biome") != nullptr) {
        root.directories.push_back(
            wd::MemoryDirectory{"biomes", {wd::MemoryEntry{"biomes.toml", std::string(BIOME_DEFINITIONS_TOML), false}}});
    }
}

// True when the production registry's `worldgen` kind has the id (P1-00
// 6.1: every id of the set must then be defined).
inline bool worldgen_id_registered(std::string_view id) {
    const worldgen::data::KindRegistry registry = worldgen::data::builtin_kinds();
    const worldgen::data::Kind* kind = registry.find("worldgen");
    return kind != nullptr && kind->schema_for(id) != nullptr;
}

// The result of loading a data root that holds the given elevation.toml
// text, through the production registry (P0-03, P1-00 6.1).
struct TuningLoad {
    std::optional<worldgen::data::ContentSet> content;
    std::vector<worldgen::data::LoadError> errors;
    [[nodiscard]] bool ok() const noexcept { return content.has_value(); }
};

inline TuningLoad load_tuning_text(std::string_view elevation_toml) {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", std::string(elevation_toml), false});
    const wd::Kind* kind = registry.find("worldgen");
    if (kind != nullptr && kind->schema_for("hydrology") != nullptr) {
        dir.entries.push_back(wd::MemoryEntry{"hydrology.toml", std::string(HYDROLOGY_TOML), false});
    }
    if (kind != nullptr && kind->schema_for("climate") != nullptr) {
        dir.entries.push_back(wd::MemoryEntry{"climate.toml", std::string(CLIMATE_TOML), false});
    }
    wd::MemoryDataRoot root;
    root.directories.push_back(dir);
    add_biome_fixtures(root);
    wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(registry, root);
    TuningLoad out;
    out.errors = r.errors;
    if (r.ok()) {
        out.content = std::move(*r.value);
    }
    return out;
}

// The stage's tuning from loaded content.
inline Tuning tuning_from(const worldgen::data::ContentSet& content) {
    return worldgen::stages::elevation_tuning_from(content);
}

// E1 to E3 (seam elevation_fields).
struct Fields {
    std::int32_t n = 0;
    std::vector<std::int64_t> c, nb, ns;                         // raw Fx per cell
    std::int64_t c_sea = 0, w_b = 0, s_land = 0, cx = 0, cy = 0;  // raw
    worldgen::stages::ElevationFields engine;                    // the stage's own result, passed on
};

inline Fields fields(std::uint64_t seed, std::int32_t n, const Tuning& t, std::uint32_t threads) {
    worldgen::stages::ElevationFields f = worldgen::stages::elevation_fields(seed, n, t, threads);
    Fields out;
    out.n = n;
    out.c = f.c;
    out.nb = f.nb;
    out.ns = f.ns;
    out.c_sea = f.c_sea.raw();
    out.w_b = f.w_b.raw();
    out.s_land = f.s_land.raw();
    out.cx = f.centre.cx.raw();
    out.cy = f.centre.cy.raw();
    out.engine = std::move(f);
    return out;
}

// E4 steps 2 and 3 on any W x H grid (seam elevation_ocean_from_provisional):
// returns the ocean mask, 1 = ocean.
inline std::vector<std::uint8_t> ocean_from_provisional(Grid grid, std::span<const std::uint8_t> provisional,
                                                        std::int64_t i_min) {
    return worldgen::stages::elevation_ocean_from_provisional(grid, provisional, i_min);
}

// E4 on the map (seam elevation_classify).
struct Ocean {
    std::vector<std::uint8_t> mask;  // 1 = ocean
    std::int64_t land_cells = 0;     // L_O
};

inline Ocean classify(const Fields& f, const Tuning& t) {
    Ocean out;
    out.mask = worldgen::stages::elevation_classify(f.engine, t);
    for (const std::uint8_t o : out.mask) {
        out.land_cells += o != 0U ? 0 : 1;
    }
    return out;
}

// E5.1 (the floor seam) on lists of the land cells' nb, ns and gm (raw).
struct Floor {
    bool triggered = false;
    std::int64_t target = 0;
    std::int64_t o_star = 0;          // raw
    std::int64_t beta_star = 0;       // raw
    std::vector<std::int64_t> mids;   // bisection mids, raw (empty if none ran)
};

inline Floor floor_of(const worldgen::stages::MountainFloor& m) {
    Floor f{m.triggered, m.target, m.offset.raw(), m.bias.raw(), {}};
    for (const worldgen::Fx x : m.bisection) {
        f.mids.push_back(x.raw());
    }
    return f;
}

inline Floor floor_search(std::span<const std::int64_t> nb, std::span<const std::int64_t> ns,
                          std::span<const std::int64_t> gm, std::int64_t w_b, const Tuning& t) {
    std::vector<worldgen::stages::FloorCell> cells;
    for (std::size_t i = 0; i < nb.size(); ++i) {
        cells.push_back(worldgen::stages::FloorCell{worldgen::Fx::from_raw(nb[i]), worldgen::Fx::from_raw(ns[i]),
                                                   worldgen::Fx::from_raw(gm[i])});
    }
    return floor_of(worldgen::stages::mountain_floor(cells, worldgen::Fx::from_raw(w_b), t));
}

// E5 to E7 (seam elevation_heights; fourth version: mask, profile and the
// pass layout added).
struct Heights {
    std::vector<std::int64_t> h7;  // raw Fx per cell, land and ocean
    std::vector<std::int64_t> unit, mask, profile;  // raw, per cell (0 on ocean)
    std::int64_t scale = 0;        // raw (E6)
    Floor floor;                   // (o*, beta*) used (E5.1)
    worldgen::stages::PassLayout passes;  // E6.5
};

inline Heights heights(std::uint64_t seed, const Fields& f, const Ocean& ocean, const Tuning& t,
                       std::uint32_t threads) {
    auto h = worldgen::stages::elevation_heights(seed, f.engine, ocean.mask, t, threads);
    Heights out;
    out.h7 = h.h;
    out.unit = h.unit;
    out.mask = h.mask;
    out.profile = h.profile;
    out.scale = h.scale.raw();
    out.floor = floor_of(h.floor);
    out.passes = std::move(h.passes);
    return out;
}

// --- E6.5 (fourth version) -------------------------------------------------------------
//
// Grids are raw Fx values per cell, as the seams take them (P1-01 10).

using PassLayout = worldgen::stages::PassLayout;
using PassReject = worldgen::stages::PassReject;

// The test's names of PassReject, in the declaration order of P1-01 10;
// "site" stands for a try without a reject (a pass or a natural gap).
inline std::string reject_name(const std::optional<PassReject>& r) {
    if (!r.has_value()) {
        return "site";
    }
    switch (*r) {
        case PassReject::coast: return "coast";
        case PassReject::barrier: return "barrier";
        case PassReject::sea: return "sea";
        case PassReject::side: return "side";
        case PassReject::along_divide: return "along_divide";
        case PassReject::along_crest: return "along_crest";
        case PassReject::chord: return "chord";
        case PassReject::tarn_crossing: return "tarn_crossing";
        case PassReject::col_tarn: return "col_tarn";
        case PassReject::summit_top: return "summit_top";
        case PassReject::zone_p90: return "zone_p90";
        case PassReject::feet: return "feet";
        case PassReject::shoulders: return "shoulders";
    }
    return "?";
}

// pass_layout on given grids (any W x H grid with an ocean cell).
inline PassLayout pass_layout(std::uint64_t seed, Grid grid, std::span<const std::int64_t> h0,
                              std::span<const std::int64_t> unit, std::span<const std::int64_t> mask,
                              std::span<const std::int64_t> profile, std::span<const std::int64_t> nb,
                              std::span<const std::uint8_t> ocean, std::int64_t scale, std::int64_t offset,
                              std::int64_t w_b, const Tuning& t) {
    worldgen::stages::PassInputs in;
    in.grid = grid;
    in.h0 = h0;
    in.unit = unit;
    in.mask = mask;
    in.profile = profile;
    in.nb = nb;
    in.ocean = ocean;
    in.scale = worldgen::Fx::from_raw(scale);
    in.offset = worldgen::Fx::from_raw(offset);
    in.w_b = worldgen::Fx::from_raw(w_b);
    return worldgen::stages::pass_layout(seed, in, t);
}

// E6.5.3 step 2 (seam divide_flood).
inline worldgen::stages::DivideFlood divide_flood(Grid grid, std::span<const std::uint8_t> support,
                                                  std::span<const std::int64_t> key,
                                                  std::span<const std::int64_t> nb, std::int64_t offset) {
    return worldgen::stages::divide_flood(grid, support, key, nb, worldgen::Fx::from_raw(offset));
}

// m'(c, p) of E6.5.0 (seam pass_lowered), raw.
inline std::int64_t pass_lowered(std::int64_t scale, std::int64_t mask, std::int64_t profile, std::int64_t p,
                                 std::int64_t foothill_fraction, std::int64_t texture_keep) {
    using worldgen::Fx;
    return worldgen::stages::pass_lowered(Fx::from_raw(scale), Fx::from_raw(mask), Fx::from_raw(profile),
                                          Fx::from_raw(p), Fx::from_raw(foothill_fraction),
                                          Fx::from_raw(texture_keep))
        .raw();
}

// E6.5.6 (o) (seam pass_depth): (lo raw, capped).
inline std::pair<std::int64_t, bool> pass_depth(std::span<const std::int64_t> mask,
                                                std::span<const std::int64_t> profile, std::int64_t scale,
                                                std::int64_t saddle, bool capped, std::int64_t maxm,
                                                const Tuning& t) {
    const worldgen::stages::PassDepth d = worldgen::stages::pass_depth(
        mask, profile, worldgen::Fx::from_raw(scale), worldgen::Fx::from_raw(saddle), capped, maxm, t);
    return {d.lo.raw(), d.capped};
}

// E6.5.7 bump (seam pass_bump), raw.
inline std::int64_t pass_bump(std::int64_t dist, std::int64_t dend, std::int64_t halfwidth, std::int64_t width_var,
                              std::int64_t nz) {
    using worldgen::Fx;
    return worldgen::stages::pass_bump(Fx::from_raw(dist), Fx::from_raw(dend), Fx::from_raw(halfwidth),
                                       Fx::from_raw(width_var), Fx::from_raw(nz))
        .raw();
}

// E6.5.8 (seam pass_drainage): Delta per axis cell, raw.
inline std::vector<std::int64_t> pass_drainage(std::span<const std::int64_t> s, std::span<const std::int64_t> nat,
                                               std::int32_t col_index, std::int64_t eps) {
    return worldgen::stages::pass_drainage(s, nat, col_index, worldgen::Fx::from_raw(eps));
}

// E8.7 (seam final_talus) on a copy of h.
inline std::vector<std::int64_t> final_talus(Grid grid, std::span<const std::int64_t> h,
                                             std::span<const std::uint8_t> ocean, std::int64_t drop_o,
                                             std::int64_t drop_d, std::int64_t rate, std::int64_t floor,
                                             std::int32_t iterations, std::uint32_t threads) {
    using worldgen::Fx;
    std::vector<std::int64_t> out(h.begin(), h.end());
    worldgen::stages::final_talus(grid, out, ocean, Fx::from_raw(drop_o), Fx::from_raw(drop_d), Fx::from_raw(rate),
                                  Fx::from_raw(floor), iterations, threads);
    return out;
}

// The whole stage with its pass sites and E7 grid (seam elevation_run).
inline worldgen::stages::ElevationResult elevation_run(std::uint64_t seed, std::int32_t n, const Tuning& t,
                                                       std::uint32_t threads) {
    return worldgen::stages::elevation_run(seed, n, t, threads);
}

// E8 on any W x H grid (seam erode). Values raw; talus_slope is the
// tuning value (D_o = mul_int(talus_slope, 16), D_d = mul(D_o, SQRT2)).
struct ErosionParams {
    std::int64_t iterations = 0;
    std::int64_t route_interval = 1;
    std::int64_t k = 0;
    std::int64_t diffusion = 0;
    std::int64_t talus_slope = 0;
    std::int64_t talus_rate = 0;
    std::int64_t floor = 0;
    std::uint64_t key = 0;  // K_erosion
};

inline std::vector<std::int64_t> erode(Grid grid, std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean,
                                       const ErosionParams& p, std::uint32_t threads) {
    worldgen::stages::ErosionParams ep;
    ep.iterations = static_cast<std::int32_t>(p.iterations);
    ep.route_interval = static_cast<std::int32_t>(p.route_interval);
    ep.k = worldgen::Fx::from_raw(p.k);
    ep.diffusion = worldgen::Fx::from_raw(p.diffusion);
    ep.talus_drop_o = worldgen::mul_int(worldgen::Fx::from_raw(p.talus_slope), 16);
    ep.talus_drop_d = worldgen::mul(ep.talus_drop_o, worldgen::Fx::SQRT2);
    ep.talus_rate = worldgen::Fx::from_raw(p.talus_rate);
    ep.floor = worldgen::Fx::from_raw(p.floor);
    ep.key = p.key;
    std::vector<std::int64_t> out(h.begin(), h.end());
    worldgen::stages::erode(grid, out, ocean, ep, threads);
    return out;
}

// E9 (seam quantize_elevation), on a 1-row grid of h.size() cells.
inline std::vector<std::int16_t> quantize(std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean) {
    const Grid g{static_cast<std::int32_t>(h.size()), 1};
    return worldgen::stages::quantize_elevation(g, h, ocean, 1);
}

// E10.1 to E10.5 on any W x H grid (the conditioning seam).
struct ConditionParams {
    std::int64_t lake_min_cells = 0;
    std::int64_t lake_max_cells = 0;
    std::int64_t lake_min_depth_dm = 0;
    std::int64_t max_share_cells = 1;
    std::int64_t fine_slope = 65536;
};

inline worldgen::stages::ConditionParams engine_params(const ConditionParams& p) {
    worldgen::stages::ConditionParams cp;
    cp.lake_min_cells = p.lake_min_cells;
    cp.lake_max_cells = p.lake_max_cells;
    cp.lake_min_depth_dm = p.lake_min_depth_dm;
    cp.max_share_cells = p.max_share_cells;
    cp.fine_slope = p.fine_slope;
    return cp;
}

// The full result: e', the E10.2 components and the kept labels.
inline worldgen::stages::Conditioned condition_full(Grid grid, std::span<const std::int16_t> e,
                                                   std::span<const std::uint8_t> ocean,
                                                   std::span<const std::int64_t> cost,
                                                   std::span<const std::int64_t> bump, const ConditionParams& p) {
    return worldgen::stages::condition_depressions(grid, e, ocean, cost, bump, engine_params(p));
}

inline std::vector<std::int16_t> condition(Grid grid, std::span<const std::int16_t> e,
                                           std::span<const std::uint8_t> ocean, std::span<const std::int64_t> cost,
                                           std::span<const std::int64_t> bump, const ConditionParams& p) {
    return condition_full(grid, e, ocean, cost, bump, p).e;
}

// The whole stage (seam elevation_grid): the elevation layer's values.
inline std::vector<std::int16_t> elevation_grid(std::uint64_t seed, std::int32_t n, const Tuning& t,
                                                std::uint32_t threads) {
    return worldgen::stages::elevation_grid(seed, n, t, threads);
}

}  // namespace elev_api
