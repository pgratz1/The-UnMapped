#pragma once

// The one place where the P1-06 regions tests touch the stage's API.
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays (Fx values as raw int64, cells as int64
// indices) and the plain structs of this file. Integration adapts this file
// only.
//
// Seams (P1-06 section 10, "interim; the engineer records the final
// signatures"). The spec gives names and arguments only; the types below
// are the test writer's assumptions, to be adapted here:
//
//   worldgen/stages/regions.hpp, namespace worldgen::stages:
//     region_tuning_from(const data::ContentSet&) -> RegionTuning            (opaque to the tests)
//     region_step_weight(Fx nz, std::int64_t hand, const RegionTuning&) -> Fx           (RG3)
//     region_cost_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const RegionTuning&) -> Fx
//                                                    (assumed helper: RG3's fbm_m(K_cost, X, Y, cost_noise))
//     RegionCandidate{cell, habitability, surface, hash}                                (RG4's site key)
//     region_candidates(Grid, water, habitability, S, std::uint64_t seed, const RegionTuning&)
//         -> std::vector<RegionCandidate>                                               (tile order)
//     region_seeds(std::span<const RegionCandidate>, Grid, water, river, const RegionTuning&)
//         -> std::vector<CellIndex>                    (RG5: sorts best first, then the greedy selection)
//     region_crosses(Grid, std::span<const std::uint8_t> border_river, CellIndex s, CellIndex c) -> bool
//     region_landmass_seeds(Grid, water, habitability, S, std::uint64_t seed, std::span<const CellIndex> seeds)
//         -> std::vector<CellIndex>                                       (RG6: the full seed list)
//     region_flood(Grid, std::span<const std::int64_t> wt, water, river, std::span<const CellIndex> seeds,
//                  const RegionTuning&) -> RegionFlood{owner (u32 seed numbers, 0 ocean), cost (int64)}
//     region_merge(Grid, std::span<const std::uint32_t> owner, water, river, std::uint32_t count,
//                  std::int64_t min_cells, const RegionTuning&) -> std::vector<std::uint32_t>   (RG8)
//     region_sector(std::int64_t dx, std::int64_t dy, std::int64_t r_c) -> int  (0 central, then the
//                  record order north, northeast, ..., northwest)
//     region_roman(std::int64_t k) -> std::string
//     RegionNameInput{seed_x, seed_y, biome, isle}
//     region_names(std::span<const RegionNameInput>, std::int64_t cx, std::int64_t cy, std::int64_t r_c,
//                  const RegionTuning&, const BiomeDefinitions&) -> std::vector<std::string>   (RG11 2 to 4)
//     region_borders(Grid, region (u16), S, walk_area (u16), water, river, const RegionTuning&)
//         -> std::vector<db::RegionBorderRow>                             (RG12 without passes, (a, b) order)
//     region_pass_crossings(Grid, region, water, std::span<const db::PassRow>, std::vector<db::RegionBorderRow>&)
//     RegionInputs{grid, elevation, water, water_depth, flow_dir, river, biome, fertility, ore, timber,
//                  fresh_water, forage, habitability, walk_area}: a Grid and thirteen spans (section 2 order)
//     regions_stage(const RegionInputs&, const BiomeDefinitions&, std::span<const db::PassRow>,
//                   const RegionTuning&, std::uint64_t seed, std::uint32_t threads)
//         -> RegionsOutput{region (u16), regions (db::RegionRow), borders (db::RegionBorderRow),
//                          biomes (db::RegionBiomeRow)}
//     run_regions(StageContext&), reached through stages::implemented_geography_stages()
//   worldgen/inspect/render.hpp (P0-04 amendment 18 (e)):
//     composite_regions_problem(elevation, water, river, flow_dir, biome, region) -> std::optional<std::string>
//                                                    (LayerGrid&, three LayerGrid* as for biomes, LayerGrid& twice)
//     render_composite_regions(elevation, biome, region, biome_table, water, river, flow_dir, scale)
//         -> std::vector<std::uint8_t>          (the biomes composite's arguments with region after biome)
//   worldgen/db/writer.hpp: RegionRow, RegionBorderRow, RegionBiomeRow with the columns of P1-06 3.2,
//     3.3 in declaration order (int64 fields, name std::string).
//   worldgen/data/schema.hpp (P0-03 amendment 17): word_grammar() beside id_grammar().
//
// A contract violation throws worldgen::FatalError: "regions: input
// precondition: " for the stage (P1-06 section 2).

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
#include "p1_06_regions_data.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/db/writer.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/stages/biomes.hpp"
#include "worldgen/stages/regions.hpp"

namespace region_api {

using worldgen::Fx;
using worldgen::geo::Grid;
using Tuning = decltype(worldgen::stages::region_tuning_from(std::declval<const worldgen::data::ContentSet&>()));
using Definitions = biome_api::Definitions;
using worldgen::geo::CellIndex;

// The result of loading a data root through the production registry.
struct Load {
    std::optional<worldgen::data::ContentSet> content;
    std::vector<worldgen::data::LoadError> errors;
    [[nodiscard]] bool ok() const noexcept { return content.has_value(); }
};

// A data root with the fixture tuning of the earlier stages, the given
// worldgen/regions.toml (std::nullopt: none) and the given biomes/
// directory (std::nullopt: none).
inline Load load(std::optional<std::string> regions_tuning, std::optional<worldgen::data::MemoryDirectory> biomes_dir) {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", elev_test::elevation_toml(), false});
    dir.entries.push_back(wd::MemoryEntry{"hydrology.toml", hydro_test::hydrology_toml(hydro_test::SHIPPED_AREA), false});
    dir.entries.push_back(wd::MemoryEntry{"climate.toml", std::string(climate_test::SHIPPED_TOML), false});
    dir.entries.push_back(wd::MemoryEntry{"biomes.toml", std::string(biome_data::TUNING_TOML), false});
    dir.entries.push_back(wd::MemoryEntry{"resources.toml", std::string(resource_data::TUNING_TOML), false});
    if (regions_tuning.has_value()) {
        dir.entries.push_back(wd::MemoryEntry{"regions.toml", *regions_tuning, false});
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

inline Tuning tuning_from(const worldgen::data::ContentSet& c) { return worldgen::stages::region_tuning_from(c); }

// P0-03 amendment 17: the named grammar `word`.
inline bool word_matches(std::string_view s) { return worldgen::data::word_grammar().matches(s); }

// --- RG3 ------------------------------------------------------------------------------------------

inline std::int64_t step_weight(std::int64_t nz_raw, std::int64_t hand, const Tuning& t) {
    return worldgen::stages::region_step_weight(Fx::from_raw(nz_raw), hand, t).raw();
}
inline std::int64_t cost_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const Tuning& t) {
    return worldgen::stages::region_cost_noise(seed, x, y, t).raw();
}

// --- RG4 to RG6 -----------------------------------------------------------------------------------

struct Candidate {
    std::int64_t cell = 0;
    std::int64_t habitability = 0;
    std::int64_t surface = 0;
    std::uint64_t hash = 0;
    friend bool operator==(const Candidate&, const Candidate&) = default;
};

inline std::vector<Candidate> candidates(Grid g, std::span<const std::uint8_t> water,
                                         std::span<const std::uint8_t> habitability, std::span<const std::int64_t> s,
                                         std::uint64_t seed, const Tuning& t) {
    std::vector<Candidate> out;
    for (const auto& c : worldgen::stages::region_candidates(g, water, habitability, s, seed, t)) {
        out.push_back(Candidate{static_cast<std::int64_t>(c.cell), static_cast<std::int64_t>(c.habitability),
                                static_cast<std::int64_t>(c.surface), static_cast<std::uint64_t>(c.hash)});
    }
    return out;
}

inline std::vector<std::int64_t> seeds(std::span<const Candidate> cands, Grid g, std::span<const std::uint8_t> water,
                                       std::span<const std::uint8_t> river, const Tuning& t) {
    std::vector<worldgen::stages::RegionCandidate> in;
    for (const Candidate& c : cands) {
        worldgen::stages::RegionCandidate rc{};
        rc.cell = static_cast<CellIndex>(c.cell);
        rc.habitability = static_cast<decltype(rc.habitability)>(c.habitability);
        rc.surface = static_cast<decltype(rc.surface)>(c.surface);
        rc.hash = c.hash;
        in.push_back(rc);
    }
    std::vector<std::int64_t> out;
    for (const CellIndex s : worldgen::stages::region_seeds(in, g, water, river, t)) {
        out.push_back(static_cast<std::int64_t>(s));
    }
    return out;
}

inline bool crosses(Grid g, std::span<const std::uint8_t> border_river, std::int64_t s, std::int64_t c) {
    return worldgen::stages::region_crosses(g, border_river, static_cast<CellIndex>(s), static_cast<CellIndex>(c));
}

inline std::vector<std::int64_t> landmass_seeds(Grid g, std::span<const std::uint8_t> water,
                                                std::span<const std::uint8_t> habitability,
                                                std::span<const std::int64_t> s, std::uint64_t seed,
                                                std::span<const std::int64_t> greedy) {
    std::vector<CellIndex> in;
    for (const std::int64_t c : greedy) {
        in.push_back(static_cast<CellIndex>(c));
    }
    std::vector<std::int64_t> out;
    for (const CellIndex c : worldgen::stages::region_landmass_seeds(g, water, habitability, s, seed, in)) {
        out.push_back(static_cast<std::int64_t>(c));
    }
    return out;
}

// --- RG7, RG8 -------------------------------------------------------------------------------------

struct Flood {
    std::vector<std::int64_t> owner;  // seed number, 0 on ocean
    std::vector<std::int64_t> cost;   // best; any value on ocean (not compared)
};

inline Flood flood(Grid g, std::span<const std::int64_t> wt, std::span<const std::uint8_t> water,
                   std::span<const std::uint8_t> river, std::span<const std::int64_t> seeds, const Tuning& t) {
    std::vector<CellIndex> in;
    for (const std::int64_t c : seeds) {
        in.push_back(static_cast<CellIndex>(c));
    }
    auto r = worldgen::stages::region_flood(g, wt, water, river, in, t);
    Flood out;
    out.owner.assign(r.owner.begin(), r.owner.end());
    out.cost.assign(r.cost.begin(), r.cost.end());
    return out;
}

inline std::vector<std::int64_t> merge(Grid g, std::span<const std::int64_t> owner, std::span<const std::uint8_t> water,
                                       std::span<const std::uint8_t> river, std::int64_t count, std::int64_t min_cells,
                                       const Tuning& t) {
    std::vector<std::uint32_t> in;
    for (const std::int64_t o : owner) {
        in.push_back(static_cast<std::uint32_t>(o));
    }
    std::vector<std::int64_t> out;
    for (const auto o : worldgen::stages::region_merge(g, in, water, river, static_cast<std::uint32_t>(count),
                                                       min_cells, t)) {
        out.push_back(static_cast<std::int64_t>(o));
    }
    return out;
}

// --- RG11 -----------------------------------------------------------------------------------------

inline int sector(std::int64_t dx, std::int64_t dy, std::int64_t rc) {
    return static_cast<int>(worldgen::stages::region_sector(dx, dy, rc));
}
inline std::string roman(std::int64_t k) { return worldgen::stages::region_roman(k); }

struct NameInput {
    std::int64_t seed_x = 0, seed_y = 0, biome = 0;
    bool isle = false;
};
inline std::vector<std::string> names(std::span<const NameInput> in, std::int64_t cx, std::int64_t cy, std::int64_t rc,
                                      const Tuning& t, const Definitions& d) {
    std::vector<worldgen::stages::RegionNameInput> v;
    for (const NameInput& n : in) {
        worldgen::stages::RegionNameInput r{};
        r.seed_x = n.seed_x;
        r.seed_y = n.seed_y;
        r.biome = n.biome;
        r.isle = n.isle;
        v.push_back(r);
    }
    return worldgen::stages::region_names(v, cx, cy, rc, t, d);
}

// --- Tables (3.2, 3.3) ----------------------------------------------------------------------------

struct RegionRow {
    std::int64_t id = 0;
    std::string name;
    std::int64_t biome = 0, x_min = 0, y_min = 0, x_max = 0, y_max = 0, seed_x = 0, seed_y = 0, landmass = 0,
                 cells = 0, lake_cells = 0, river_cells = 0, river_class_max = 0, coast_cells = 0, walk_cells = 0,
                 walk_area = 0, elev_mean_dm = 0, elev_max_dm = 0, fertility_mean = 0, forage_mean = 0,
                 timber_mean = 0, ore_mean = 0, fresh_water_mean = 0, habitability_mean = 0, habitability_p90 = 0,
                 ore_rich_cells = 0, ore_reach_cells = 0;
    friend bool operator==(const RegionRow&, const RegionRow&) = default;
};

struct BorderRow {
    std::int64_t a = 0, b = 0, length_m = 0, low_dm = 0, low_x = 0, low_y = 0, walk_pairs = 0, river_pairs = 0,
                 river_class_max = 0, pass_id = 0, passes = 0;
    friend bool operator==(const BorderRow&, const BorderRow&) = default;
};

struct BiomeRow {
    std::int64_t region = 0, biome = 0, cells = 0;
    friend bool operator==(const BiomeRow&, const BiomeRow&) = default;
};

struct Pass {
    std::int64_t id = 0;
    std::vector<std::uint8_t> path;  // u32 LE cell indices
};

inline std::vector<worldgen::db::PassRow> pass_rows(std::span<const Pass> passes) {
    std::vector<worldgen::db::PassRow> out;
    for (const Pass& p : passes) {
        worldgen::db::PassRow r;
        r.id = p.id;
        r.path = p.path;
        out.push_back(std::move(r));
    }
    return out;
}

inline BorderRow border_of(const worldgen::db::RegionBorderRow& r) {
    return BorderRow{r.a,          r.b,           r.length_m,        r.low_dm,  r.low_x, r.low_y,
                     r.walk_pairs, r.river_pairs, r.river_class_max, r.pass_id, r.passes};
}

inline std::vector<BorderRow> borders(Grid g, std::span<const std::uint16_t> region, std::span<const std::int64_t> s,
                                      std::span<const std::uint16_t> walk, std::span<const std::uint8_t> water,
                                      std::span<const std::uint8_t> river, std::span<const Pass> passes,
                                      const Tuning& t) {
    std::vector<worldgen::db::RegionBorderRow> rows = worldgen::stages::region_borders(g, region, s, walk, water, river, t);
    const std::vector<worldgen::db::PassRow> pr = pass_rows(passes);
    worldgen::stages::region_pass_crossings(g, region, water, pr, rows);
    std::vector<BorderRow> out;
    for (const auto& r : rows) {
        out.push_back(border_of(r));
    }
    return out;
}

// --- The whole stage ------------------------------------------------------------------------------

// The thirteen input layers of section 2, as the tests hold them.
struct Inputs {
    Grid grid;
    std::vector<std::int16_t> elevation;
    std::vector<std::uint8_t> water;
    std::vector<std::uint16_t> water_depth;
    std::vector<std::uint8_t> flow_dir;
    std::vector<std::uint8_t> river;
    std::vector<std::uint8_t> biome;
    std::vector<std::uint8_t> fertility, ore, timber, fresh_water, forage, habitability;
    std::vector<std::uint16_t> walk_area;
};

struct Output {
    std::vector<std::uint16_t> region;
    std::vector<RegionRow> regions;
    std::vector<BorderRow> borders;
    std::vector<BiomeRow> biomes;
    friend bool operator==(const Output&, const Output&) = default;
};

inline Output stage(const Inputs& in, const Definitions& d, std::span<const Pass> passes, const Tuning& t,
                    std::uint64_t seed, std::uint32_t threads) {
    const worldgen::stages::RegionInputs ri{in.grid,      in.elevation, in.water,       in.water_depth, in.flow_dir,
                                            in.river,     in.biome,     in.fertility,   in.ore,         in.timber,
                                            in.fresh_water, in.forage,  in.habitability, in.walk_area};
    const std::vector<worldgen::db::PassRow> pr = pass_rows(passes);
    auto r = worldgen::stages::regions_stage(ri, d, pr, t, seed, threads);
    Output out;
    out.region = std::move(r.region);
    for (const auto& x : r.regions) {
        out.regions.push_back(RegionRow{x.id,          x.name,         x.biome,           x.x_min,
                                        x.y_min,       x.x_max,        x.y_max,           x.seed_x,
                                        x.seed_y,      x.landmass,     x.cells,           x.lake_cells,
                                        x.river_cells, x.river_class_max, x.coast_cells,  x.walk_cells,
                                        x.walk_area,   x.elev_mean_dm, x.elev_max_dm,     x.fertility_mean,
                                        x.forage_mean, x.timber_mean,  x.ore_mean,        x.fresh_water_mean,
                                        x.habitability_mean, x.habitability_p90, x.ore_rich_cells, x.ore_reach_cells});
    }
    for (const auto& x : r.borders) {
        out.borders.push_back(border_of(x));
    }
    for (const auto& x : r.biomes) {
        out.biomes.push_back(BiomeRow{x.region, x.biome, x.cells});
    }
    return out;
}

// --- Composite regions (P0-04 amendment 18 (e)) ------------------------------------------------------

inline std::optional<std::string> composite_problem(const worldgen::db::LayerGrid& elevation,
                                                    const worldgen::db::LayerGrid* water,
                                                    const worldgen::db::LayerGrid* river,
                                                    const worldgen::db::LayerGrid* flow_dir,
                                                    const worldgen::db::LayerGrid& biome,
                                                    const worldgen::db::LayerGrid& region) {
    return worldgen::inspect::composite_regions_problem(elevation, water, river, flow_dir, biome, region);
}

inline std::vector<std::uint8_t> composite(const worldgen::db::LayerGrid& elevation, const worldgen::db::LayerGrid& biome,
                                           const worldgen::db::LayerGrid& region,
                                           std::span<const worldgen::inspect::BiomeColour> table,
                                           const worldgen::db::LayerGrid* water, const worldgen::db::LayerGrid* river,
                                           const worldgen::db::LayerGrid* flow_dir, std::uint32_t scale) {
    return worldgen::inspect::render_composite_regions(elevation, biome, region, table, water, river, flow_dir, scale);
}

}  // namespace region_api
