#pragma once

// The one place where the P1-04 biome tests touch the stage's API.
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays (Fx values as raw int64). Integration adapts
// this file only.
//
// Seams (P1-04 section 10, "interim; the engineer may add helpers"). The
// spec gives names and arguments only; the types below are the test
// writer's assumptions, to be adapted here:
//
//   worldgen/stages/biomes.hpp, namespace worldgen::stages:
//     biome_definitions_from(const data::ContentSet&) -> Definitions   (opaque to the tests)
//     biome_tuning_from(const data::ContentSet&) -> Tuning              (opaque to the tests)
//     biome_rain_scale(const Tuning&, std::int64_t n) -> Fx             (B4 size rule)
//     biome_lookup(const Definitions&, Fx scale, Fx tq, Fx rq, std::int64_t elevation_dm,
//                  std::int64_t s2, std::int64_t hand_dm, std::int64_t coast_dist) -> std::uint8_t   (B4)
//     biome_river_cells(Grid, std::span<const std::uint8_t> water,
//                       std::span<const std::uint8_t> b4) -> std::vector<std::uint8_t>          (B5)
//     BiomeInputs{grid, elevation, water, water_depth, flow_dir, river, coast_dist, temperature,
//                 rainfall}: an aggregate of a Grid and eight spans in the order of section 2
//     biome_layer(const BiomeInputs&, const Definitions&, const Tuning&, std::uint64_t seed,
//                 std::uint32_t threads) -> std::vector<std::uint8_t>
//     run_biomes(StageContext&), reached through stages::implemented_geography_stages()
//   worldgen/geo/drainage.hpp, namespace worldgen::geo (G7 is a shared algorithm, P1-00 7.7,
//   used by P1-05 too):
//     height_above_drainage(Grid, std::span<const std::int64_t> S, std::span<const std::uint8_t> water,
//                           std::span<const std::uint8_t> river, std::span<const std::uint8_t> flow_dir,
//                           std::int64_t k) -> std::vector<std::int64_t>
//
// A precondition violation throws worldgen::FatalError: "biomes: input
// precondition: " for the stage (P1-04 section 2), "G7: " for G7 alone
// (P1-00 7.7).

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p1_01_elevation_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "p1_03_climate_support.hpp"
#include "p1_04_biomes_data.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/geo/drainage.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/stages/biomes.hpp"

namespace biome_api {

using worldgen::Fx;
using worldgen::geo::Grid;
using Definitions = decltype(worldgen::stages::biome_definitions_from(std::declval<const worldgen::data::ContentSet&>()));
using Tuning = decltype(worldgen::stages::biome_tuning_from(std::declval<const worldgen::data::ContentSet&>()));

// The result of loading a data root through the production registry.
struct Load {
    std::optional<worldgen::data::ContentSet> content;
    std::vector<worldgen::data::LoadError> errors;
    [[nodiscard]] bool ok() const noexcept { return content.has_value(); }
};

// A data root with the fixture tuning of the earlier stages, the given
// worldgen/biomes.toml (std::nullopt: none) and the given biomes/ directory
// (std::nullopt: none).
inline Load load(std::optional<std::string> biomes_tuning, std::optional<worldgen::data::MemoryDirectory> biomes_dir) {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", elev_test::elevation_toml(), false});
    dir.entries.push_back(wd::MemoryEntry{"hydrology.toml", hydro_test::hydrology_toml(hydro_test::SHIPPED_AREA), false});
    dir.entries.push_back(wd::MemoryEntry{"climate.toml", std::string(climate_test::SHIPPED_TOML), false});
    if (biomes_tuning.has_value()) {
        dir.entries.push_back(wd::MemoryEntry{"biomes.toml", *biomes_tuning, false});
    }
    wd::MemoryDataRoot root;
    root.directories.push_back(dir);
    if (biomes_dir.has_value()) {
        root.directories.push_back(*biomes_dir);
    }
    wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(registry, root);
    Load out;
    out.errors = r.errors;
    if (r.ok()) {
        out.content = std::move(*r.value);
    }
    return out;
}

inline Definitions definitions_from(const worldgen::data::ContentSet& c) {
    return worldgen::stages::biome_definitions_from(c);
}
inline Tuning tuning_from(const worldgen::data::ContentSet& c) { return worldgen::stages::biome_tuning_from(c); }

// B4's size rule (seam biome_rain_scale), raw.
inline std::int64_t rain_scale(const Tuning& t, std::int64_t n) { return worldgen::stages::biome_rain_scale(t, n).raw(); }

// B4 for one cell (seam biome_lookup).
inline std::uint8_t lookup(const Definitions& d, std::int64_t scale_raw, std::int64_t tq_raw, std::int64_t rq_raw,
                           std::int64_t elevation, std::int64_t s2, std::int64_t hand, std::int64_t coast_dist) {
    return worldgen::stages::biome_lookup(d, Fx::from_raw(scale_raw), Fx::from_raw(tq_raw), Fx::from_raw(rq_raw),
                                          elevation, s2, hand, coast_dist);
}

// B5 (seam biome_river_cells): the biome grid from the B4 grid.
inline std::vector<std::uint8_t> river_cells(Grid g, std::span<const std::uint8_t> water,
                                             std::span<const std::uint8_t> b4) {
    return worldgen::stages::biome_river_cells(g, water, b4);
}

// G7 (seam height_above_drainage).
inline std::vector<std::int64_t> hand(Grid g, std::span<const std::int64_t> s, std::span<const std::uint8_t> water,
                                      std::span<const std::uint8_t> river, std::span<const std::uint8_t> flow_dir,
                                      std::int64_t k) {
    return worldgen::geo::height_above_drainage(g, s, water, river, flow_dir, k);
}

// The eight input layers of section 2, as the tests hold them.
struct Inputs {
    Grid grid;
    std::vector<std::int16_t> elevation;
    std::vector<std::uint8_t> water;
    std::vector<std::uint16_t> water_depth;
    std::vector<std::uint8_t> flow_dir;
    std::vector<std::uint8_t> river;
    std::vector<std::int16_t> coast_dist;
    std::vector<std::int16_t> temperature;
    std::vector<std::uint16_t> rainfall;
};

// The stage on any grid (seam biome_layer).
inline std::vector<std::uint8_t> layer(const Inputs& in, const Definitions& d, const Tuning& t, std::uint64_t seed,
                                       std::uint32_t threads) {
    const worldgen::stages::BiomeInputs bi{in.grid,     in.elevation,  in.water,       in.water_depth, in.flow_dir,
                                           in.river,    in.coast_dist, in.temperature, in.rainfall};
    return worldgen::stages::biome_layer(bi, d, t, seed, threads);
}

}  // namespace biome_api
