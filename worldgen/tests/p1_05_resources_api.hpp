#pragma once

// The one place where the P1-05 resources tests touch the stage's API.
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays (Fx values as raw int64). Integration adapts
// this file only.
//
// Seams (P1-05 section 10, "interim; the engineer may add helpers"). The
// spec gives names and arguments only; the types below are the test
// writer's assumptions, to be adapted here:
//
//   worldgen/stages/resources.hpp, namespace worldgen::stages:
//     resource_tuning_from(const data::ContentSet&) -> Tuning          (opaque to the tests)
//     resource_ramp(Fx x, Fx a, Fx b) -> Fx
//     resource_q255(Fx s) -> std::uint8_t
//     chamfer_metres(Grid, std::span<const std::uint8_t> source) -> std::vector<std::int64_t>
//                                                    (source: 1 on a source cell, 0 elsewhere; metres)
//     fresh_water_cell(std::int64_t df, std::int64_t rainfall, const Tuning&) -> std::uint8_t
//     fertility_cell(Fx fb, std::int64_t s2, std::int64_t hand, const Tuning&) -> std::uint8_t
//     timber_cell(Fx tb, std::int64_t s2, const Tuning&) -> std::uint8_t
//     forage_cell(Fx gb, std::int64_t dg, const Tuning&) -> std::uint8_t
//     ore_cell(std::int64_t elevation, std::int64_t s2, Fx noise, const Tuning&) -> std::uint8_t
//     walk_areas(Grid, std::span<const std::int64_t> S, std::span<const std::uint8_t> water,
//                std::span<const std::int64_t> s2, Fx walk_slope_max, std::int64_t walk_min_cells)
//         -> {ids (std::vector<std::uint16_t>), count}
//     habitability_cell(std::uint8_t F, W, G, T, O, std::int64_t s2, std::int64_t hand,
//                       std::uint16_t walk_area, const Tuning&) -> std::uint8_t
//     ResourceInputs{grid, elevation, water, water_depth, flow_dir, river, rainfall, biome}:
//         a Grid and seven spans in the order of section 2
//     resources_layers(const ResourceInputs&, const BiomeDefinitions&, const Tuning&, std::uint64_t seed,
//                      std::uint32_t threads) -> {fertility, ore, timber, fresh_water, forage,
//                                                 habitability (u8), walk_area (u16)}
//     resource_ore_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const Tuning&) -> Fx
//                                                    (assumed helper: R4's fbm_m(K_ore, X, Y, ore_noise))
//     run_resources(StageContext&), reached through stages::implemented_geography_stages()
//   worldgen/geo/terrain.hpp, namespace worldgen::geo (G8's tangent, "shared with P1-04"):
//     slope_tangent(std::int64_t s2) -> Fx
//   BiomeDef (worldgen/stages/biomes.hpp) gains `Fx forage` after `timber` (P1-05 4.1).
//
// A precondition violation throws worldgen::FatalError: "resources: input
// precondition: " for the stage (P1-05 section 2).

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "p1_01_elevation_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "p1_03_climate_support.hpp"
#include "p1_04_biomes_api.hpp"
#include "p1_04_biomes_data.hpp"
#include "p1_05_resources_data.hpp"
#include "p3_01_history_fixture.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/terrain.hpp"
#include "worldgen/stages/biomes.hpp"
#include "worldgen/stages/resources.hpp"

namespace resource_api {

using worldgen::Fx;
using worldgen::geo::Grid;
using Tuning = decltype(worldgen::stages::resource_tuning_from(std::declval<const worldgen::data::ContentSet&>()));
using Definitions = biome_api::Definitions;

// The result of loading a data root through the production registry.
struct Load {
    std::optional<worldgen::data::ContentSet> content;
    std::vector<worldgen::data::LoadError> errors;
    [[nodiscard]] bool ok() const noexcept { return content.has_value(); }
};

// A data root with the fixture tuning of the earlier stages, the given
// worldgen/resources.toml (std::nullopt: none) and the given biomes/
// directory (std::nullopt: none).
inline Load load(std::optional<std::string> resources_tuning, std::optional<worldgen::data::MemoryDirectory> biomes_dir) {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", elev_test::elevation_toml(), false});
    dir.entries.push_back(wd::MemoryEntry{"hydrology.toml", hydro_test::hydrology_toml(hydro_test::SHIPPED_AREA), false});
    dir.entries.push_back(wd::MemoryEntry{"climate.toml", std::string(climate_test::SHIPPED_TOML), false});
    dir.entries.push_back(wd::MemoryEntry{"biomes.toml", std::string(biome_data::TUNING_TOML), false});
    if (resources_tuning.has_value()) {
        dir.entries.push_back(wd::MemoryEntry{"resources.toml", *resources_tuning, false});
    }
    if (registry.find("worldgen")->schema_for("regions") != nullptr) {  // P1-06 4
        dir.entries.push_back(wd::MemoryEntry{"regions.toml", std::string(elev_api::REGIONS_TOML), false});
    }
    wd::MemoryDataRoot root;
    root.directories.push_back(dir);
    if (biomes_dir.has_value()) {
        root.directories.push_back(*biomes_dir);
    }
    history_fixture::add_to(root);  // P3-01: the required history kinds
    wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(registry, root);
    Load out;
    out.errors = r.errors;
    if (r.ok()) {
        out.content = std::move(*r.value);
    }
    return out;
}

inline Tuning tuning_from(const worldgen::data::ContentSet& c) { return worldgen::stages::resource_tuning_from(c); }

// The (fertility, timber, forage) of a loaded definition by code, raw;
// forage through BiomeDef::forage (P1-05 4.1).
struct BiomeValues {
    std::int64_t fertility = 0, timber = 0, forage = 0;
};
inline std::optional<BiomeValues> biome_values(const Definitions& d, std::int64_t code) {
    for (const auto& b : d.biomes) {
        if (b.code == code) {
            return BiomeValues{b.fertility.raw(), b.timber.raw(), b.forage.raw()};
        }
    }
    return std::nullopt;
}

// --- Helpers (5) ------------------------------------------------------------------------------

inline std::int64_t ramp(std::int64_t x, std::int64_t a, std::int64_t b) {
    return worldgen::stages::resource_ramp(Fx::from_raw(x), Fx::from_raw(a), Fx::from_raw(b)).raw();
}
inline std::uint8_t q255(std::int64_t s) { return worldgen::stages::resource_q255(Fx::from_raw(s)); }
inline std::int64_t tangent(std::int64_t s2) { return worldgen::geo::slope_tangent(s2).raw(); }

// --- R3 ---------------------------------------------------------------------------------------

inline std::vector<std::int64_t> chamfer_metres(Grid g, std::span<const std::uint8_t> source) {
    return worldgen::stages::chamfer_metres(g, source);
}
inline std::uint8_t fresh_water(std::int64_t df, std::int64_t rainfall, const Tuning& t) {
    return worldgen::stages::fresh_water_cell(df, rainfall, t);
}

// --- R4 ---------------------------------------------------------------------------------------

inline std::uint8_t fertility(std::int64_t fb_raw, std::int64_t s2, std::int64_t hand, const Tuning& t) {
    return worldgen::stages::fertility_cell(Fx::from_raw(fb_raw), s2, hand, t);
}
inline std::uint8_t timber(std::int64_t tb_raw, std::int64_t s2, const Tuning& t) {
    return worldgen::stages::timber_cell(Fx::from_raw(tb_raw), s2, t);
}
inline std::uint8_t forage(std::int64_t gb_raw, std::int64_t dg, const Tuning& t) {
    return worldgen::stages::forage_cell(Fx::from_raw(gb_raw), dg, t);
}
inline std::uint8_t ore(std::int64_t elevation, std::int64_t s2, std::int64_t noise_raw, const Tuning& t) {
    return worldgen::stages::ore_cell(elevation, s2, Fx::from_raw(noise_raw), t);
}
inline std::int64_t ore_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const Tuning& t) {
    return worldgen::stages::resource_ore_noise(seed, x, y, t).raw();
}

// --- R5 ---------------------------------------------------------------------------------------

struct Walk {
    std::vector<std::uint16_t> ids;
    std::int64_t count = 0;
};
inline Walk walk_areas(Grid g, std::span<const std::int64_t> surface, std::span<const std::uint8_t> water,
                       std::span<const std::int64_t> s2, std::int64_t walk_slope_max_raw, std::int64_t min_cells) {
    auto r = worldgen::stages::walk_areas(g, surface, water, s2, Fx::from_raw(walk_slope_max_raw), min_cells);
    return Walk{std::move(r.ids), static_cast<std::int64_t>(r.count)};
}

// --- R6 ---------------------------------------------------------------------------------------

inline std::uint8_t habitability(std::uint8_t f, std::uint8_t w, std::uint8_t g, std::uint8_t t8, std::uint8_t o,
                                 std::int64_t s2, std::int64_t hand, std::uint16_t walk, const Tuning& t) {
    return worldgen::stages::habitability_cell(f, w, g, t8, o, s2, hand, walk, t);
}

// --- The whole stage ------------------------------------------------------------------------

// The seven input layers of section 2, as the tests hold them.
struct Inputs {
    Grid grid;
    std::vector<std::int16_t> elevation;
    std::vector<std::uint8_t> water;
    std::vector<std::uint16_t> water_depth;
    std::vector<std::uint8_t> flow_dir;
    std::vector<std::uint8_t> river;
    std::vector<std::uint16_t> rainfall;
    std::vector<std::uint8_t> biome;
};

// The seven output layers of section 3.
struct Layers {
    std::vector<std::uint8_t> fertility, ore, timber, fresh_water, forage, habitability;
    std::vector<std::uint16_t> walk_area;
    friend bool operator==(const Layers&, const Layers&) = default;
};

inline Layers layers(const Inputs& in, const Definitions& d, const Tuning& t, std::uint64_t seed,
                     std::uint32_t threads) {
    const worldgen::stages::ResourceInputs ri{in.grid,  in.elevation, in.water,    in.water_depth,
                                              in.flow_dir, in.river,  in.rainfall, in.biome};
    auto r = worldgen::stages::resources_layers(ri, d, t, seed, threads);
    Layers out;
    out.fertility = std::move(r.fertility);
    out.ore = std::move(r.ore);
    out.timber = std::move(r.timber);
    out.fresh_water = std::move(r.fresh_water);
    out.forage = std::move(r.forage);
    out.habitability = std::move(r.habitability);
    out.walk_area = std::move(r.walk_area);
    return out;
}

}  // namespace resource_api
