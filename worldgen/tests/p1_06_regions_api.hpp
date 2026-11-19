#pragma once

// The one place where the P1-06 regions tests touch the stage's API.
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays (Fx values as raw int64, cells as int64
// indices) and the plain structs of this file. This file alone was adapted
// to the final seams at integration (P1-06 section 10, the engineer's
// signatures; worldgen/stages/regions.hpp):
//
//   region_tuning_from, region_cost_noise, region_step_weight (Fx wt),
//   RegionSite + region_candidates / region_seeds, region_crosses,
//   region_landmasses + region_landmass_seeds(..., landmasses, seeds, seed),
//   region_flood(grid, span<const Fx> wt, ...), region_merge, region_sector
//   (u32 index: central, north, ..., northwest), roman(k), region_names
//   (landmass sizes and land sums; the wrapper below builds them from a
//   centroid and isle flags), region_borders(grid, region, S, water, river,
//   walk_area, tuning), region_pass_crossings, regions_stage -> RegionResult;
//   inspect::composite_regions_problem / render_composite_regions(elevation,
//   water, river, flow_dir, biome, region, scale, table); data::word_grammar().
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
    for (const worldgen::stages::RegionSite& c : worldgen::stages::region_candidates(g, water, habitability, s, seed, t)) {
        out.push_back(Candidate{static_cast<std::int64_t>(c.cell), static_cast<std::int64_t>(c.habitability),
                                static_cast<std::int64_t>(c.surface), static_cast<std::uint64_t>(c.hash)});
    }
    return out;
}

inline std::vector<std::int64_t> seeds(std::span<const Candidate> cands, Grid g, std::span<const std::uint8_t> water,
                                       std::span<const std::uint8_t> river, const Tuning& t) {
    std::vector<worldgen::stages::RegionSite> in;
    for (const Candidate& c : cands) {
        worldgen::stages::RegionSite rc{};
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
    const worldgen::stages::RegionLandmasses lm = worldgen::stages::region_landmasses(g, water);
    std::vector<std::int64_t> out;
    for (const CellIndex c :
         worldgen::stages::region_landmass_seeds(g, water, habitability, s, lm, std::move(in), seed)) {
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
    std::vector<Fx> w;
    w.reserve(wt.size());
    for (const std::int64_t v : wt) {
        w.push_back(Fx::from_raw(v));
    }
    auto r = worldgen::stages::region_flood(g, w, water, river, in, t);
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
inline std::string roman(std::int64_t k) { return worldgen::stages::roman(k); }

// cells, lake_cells and walk_cells decide RG11 3 b (Mere) and c (upland,
// r2); the defaults make neither apply.
struct NameInput {
    std::int64_t seed_x = 0, seed_y = 0, biome = 0;
    bool isle = false;
    std::int64_t cells = 1, lake_cells = 0, walk_cells = 1;
};
// RG11 2 to 4 with the land centroid (cx, cy) and map side n given: the
// seam computes the centroid from the land sums and the isles from the
// landmass sizes, so the regions not marked isle are put on landmass 1 (the
// largest) and each isle on a landmass of its own of one cell.
inline std::vector<std::string> names(std::span<const NameInput> in, std::int64_t cx, std::int64_t cy, std::int64_t n,
                                      const Tuning& t, const Definitions& d) {
    std::vector<worldgen::stages::RegionNameInput> v;
    std::vector<std::uint64_t> lm_cells = {1000000};
    for (const NameInput& e : in) {
        worldgen::stages::RegionNameInput r{};
        r.seed_x = e.seed_x;
        r.seed_y = e.seed_y;
        r.biome = static_cast<std::uint8_t>(e.biome);
        // r2: RegionNameInput.cells, lake_cells, walk_cells (spec section 10).
        r.cells = e.cells;
        r.lake_cells = e.lake_cells;
        r.walk_cells = e.walk_cells;
        if (e.isle) {
            lm_cells.push_back(1);
            r.landmass = static_cast<std::uint32_t>(lm_cells.size());
        } else {
            r.landmass = 1;
        }
        v.push_back(r);
    }
    return worldgen::stages::region_names(v, lm_cells, 1, static_cast<std::uint64_t>(cx), static_cast<std::uint64_t>(cy),
                                          n, d, t);
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
    std::int64_t col_x = 0, col_y = 0;  // the col cell (r2: on the path)
};

inline std::vector<worldgen::db::PassRow> pass_rows(std::span<const Pass> passes) {
    std::vector<worldgen::db::PassRow> out;
    for (const Pass& p : passes) {
        worldgen::db::PassRow r;
        r.id = p.id;
        r.path = p.path;
        r.col_x = p.col_x;
        r.col_y = p.col_y;
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
    std::vector<worldgen::db::RegionBorderRow> rows = worldgen::stages::region_borders(g, region, s, water, river, walk, t);
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
    return worldgen::inspect::render_composite_regions(elevation, water, river, flow_dir, biome, region, scale, table);
}

}  // namespace region_api
