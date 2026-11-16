#pragma once

// The one place where the P1-02 hydrology tests touch the stage's API.
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays. Integration adapts this file only.
//
// Seams used (P1-02 section 8: "hydrology_layers(elevation grid, tuning,
// threads) returning the seven grids; and the shared G3, G4, G5 functions on
// any W x H grid"), as declared in worldgen/stages/hydrology.hpp:
//
//   tuning        HydrologyTuning, hydrology_tuning_from(const data::ContentSet&)
//   stage         hydrology_layers(geo::Grid, std::span<const std::int16_t> e, std::span<const std::uint16_t> tie,
//                     const HydrologyTuning&, std::uint32_t threads) -> HydrologyLayers (the seven layers
//                     of section 3; an empty tie span means t = 0, P1-02 8 as amended 2026-11-16)
//   tie           flat_tie(seed, geo::Grid, std::span<const std::uint8_t> ocean, const HydrologyTuning&,
//                     std::uint32_t threads) -> std::vector<std::uint16_t> (H2 step 1)
//                 A precondition violation throws worldgen::FatalError whose message
//                 starts "hydrology: elevation precondition: " (P1-02 section 2).
//   steps         check_hydrology_preconditions (H1), hydrology_network(grid, e, tie, threads) (H2 to
//                 H5; HydrologyNetwork has level_dm, parent and order of the H2 flood),
//                 river_threshold_cells (section 4), river_class (H6), coast_distance (H7)
//   H7 chamfer    stages::chamfer_distance (elevation.hpp, shared with P1-01 E5.3)
//   G3 to G5      geo::routing_flood, receivers, accumulate_topological
//   entry         worldgen::generate_world with WorldParams::until = "hydrology" (P1-00 10.1)

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/load_error.hpp"
#include "worldgen/data/schema.hpp"
#include "worldgen/geo/flow.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/stages/elevation.hpp"
#include "worldgen/stages/hydrology.hpp"

namespace hydro_api {

using worldgen::geo::Grid;
using Tuning = worldgen::stages::HydrologyTuning;

// The seven layers of P1-02 section 3, as the tests read them.
struct Layers {
    std::vector<std::uint8_t> water;
    std::vector<std::uint16_t> water_depth;
    std::vector<std::uint8_t> flow_dir;
    std::vector<std::uint32_t> flow_accum;
    std::vector<std::uint8_t> river;
    std::vector<std::uint32_t> lake;
    std::vector<std::int16_t> coast_dist;

    friend bool operator==(const Layers&, const Layers&) = default;
};

// The result of loading a data root holding the given worldgen/*.toml
// texts, through the production registry (P0-03, P1-00 6.1).
struct TuningLoad {
    std::optional<worldgen::data::ContentSet> content;
    std::vector<worldgen::data::LoadError> errors;
    [[nodiscard]] bool ok() const noexcept { return content.has_value(); }
};

inline TuningLoad load_tuning_text(std::string_view elevation_toml, std::string_view hydrology_toml) {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", std::string(elevation_toml), false});
    dir.entries.push_back(wd::MemoryEntry{"hydrology.toml", std::string(hydrology_toml), false});
    wd::MemoryDataRoot root;
    root.directories.push_back(dir);
    wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(registry, root);
    TuningLoad out;
    out.errors = r.errors;
    if (r.ok()) {
        out.content = std::move(*r.value);
    }
    return out;
}

// The stage's tuning from loaded content (assumed name, as P1-01's
// elevation_tuning_from).
inline Tuning tuning_from(const worldgen::data::ContentSet& content) {
    return worldgen::stages::hydrology_tuning_from(content);
}

// The whole stage on any W x H grid (seam hydrology_layers) with the given
// tie values (empty: t = 0 everywhere).
inline Layers layers_tie(Grid grid, std::span<const std::int16_t> elevation, std::span<const std::uint16_t> tie,
                         const Tuning& t, std::uint32_t threads) {
    worldgen::stages::HydrologyLayers r = worldgen::stages::hydrology_layers(grid, elevation, tie, t, threads);
    Layers out;
    out.water = std::move(r.water);
    out.water_depth = std::move(r.water_depth);
    out.flow_dir = std::move(r.flow_dir);
    out.flow_accum = std::move(r.flow_accum);
    out.river = std::move(r.river);
    out.lake = std::move(r.lake);
    out.coast_dist = std::move(r.coast_dist);
    return out;
}

// The whole stage with t = 0 (the worked example and the unit fixtures of
// 8.1 are given for t = 0).
inline Layers layers(Grid grid, std::span<const std::int16_t> elevation, const Tuning& t, std::uint32_t threads) {
    return layers_tie(grid, elevation, {}, t, threads);
}

// H2 step 1 (seam flat_tie): the tie values of a world seed, 0 on ocean.
inline std::vector<std::uint16_t> flat_tie(std::uint64_t seed, Grid grid, std::span<const std::uint8_t> ocean,
                                           const Tuning& t, std::uint32_t threads) {
    return worldgen::stages::flat_tie(seed, grid, ocean, t, threads);
}

// The stage as run on a world: the tie values of the seed (what run_hydrology
// passes, P1-02 8).
inline Layers world_layers(std::uint64_t seed, Grid grid, std::span<const std::int16_t> elevation, const Tuning& t,
                           std::uint32_t threads) {
    std::vector<std::uint8_t> ocean(elevation.size());
    for (std::size_t i = 0; i < ocean.size(); ++i) {
        ocean[i] = elevation[i] < 0 ? 1 : 0;
    }
    const std::vector<std::uint16_t> tie = hydro_api::flat_tie(seed, grid, ocean, t, threads);
    return layers_tie(grid, elevation, tie, t, threads);
}

// Section 4: T = ceil(river_min_area_m2 / 256) (the stage's function).
inline std::int64_t threshold_cells(std::int64_t river_min_area_m2) {
    return worldgen::stages::river_threshold_cells(river_min_area_m2);
}

// H6 river class of accumulation a with threshold t (the stage's function).
inline std::uint8_t stage_river_class(std::int64_t a, std::int64_t t) { return worldgen::stages::river_class(a, t); }

// H1 alone.
inline void check_preconditions(Grid grid, std::span<const std::int16_t> e) {
    worldgen::stages::check_hydrology_preconditions(grid, e);
}

// H7 alone: coast_dist in metres from an ocean mask.
inline std::vector<std::int16_t> coast_distance(Grid grid, std::span<const std::uint8_t> ocean) {
    return worldgen::stages::coast_distance(grid, ocean);
}

// H2 to H5 (seam hydrology_network): the intermediates as plain arrays.
struct Network {
    std::vector<std::int64_t> level_dm;   // L, decimetres (H2)
    std::vector<std::uint8_t> parent;     // flood parent, 8 on ocean
    std::vector<std::uint32_t> order;     // pop order
    std::vector<std::uint8_t> g4;         // G4 receivers before rerouting, 8 on ocean
    std::vector<std::uint32_t> lake;      // lake id per cell (H3), 0 elsewhere
    std::vector<std::int64_t> lake_level; // per lake id - 1
    std::vector<std::uint32_t> outlet;    // per lake id - 1 (H4 step 2)
    std::vector<std::uint8_t> rcv;        // final receivers, 8 on ocean
    std::vector<std::uint32_t> accum;     // A (H5), 0 on ocean
};

inline Network network(Grid grid, std::span<const std::int16_t> e, std::span<const std::uint16_t> tie,
                       std::uint32_t threads) {
    worldgen::stages::HydrologyNetwork r = worldgen::stages::hydrology_network(grid, e, tie, threads);
    Network out;
    out.level_dm = std::move(r.level_dm);
    out.parent.assign(r.parent.begin(), r.parent.end());
    out.order.assign(r.order.begin(), r.order.end());
    out.g4.assign(r.g4.begin(), r.g4.end());
    out.lake = std::move(r.lakes.label);
    out.lake_level = std::move(r.lake_level);
    out.outlet.assign(r.lake_outlet.begin(), r.lake_outlet.end());
    out.rcv.assign(r.rcv.begin(), r.rcv.end());
    out.accum = std::move(r.accum);
    return out;
}

// H7's chamfer alone: distance in fifths of a cell to the nearest source
// cell (source[i] != 0), INF = 2^40 where no source is reachable.
inline std::vector<std::int64_t> chamfer(Grid grid, std::span<const std::uint8_t> source) {
    return worldgen::stages::chamfer_distance(grid, source);
}

// G3 on any W x H grid (shared, P1-00 7.3).
struct Flood {
    std::vector<std::int64_t> level;
    std::vector<std::uint8_t> parent;
    std::vector<std::uint32_t> order;
};

inline Flood flood(Grid grid, std::span<const std::int64_t> keys, std::span<const std::uint8_t> ocean) {
    worldgen::geo::RoutingFlood f = worldgen::geo::routing_flood(grid, keys, ocean);
    return Flood{std::move(f.level), std::vector<std::uint8_t>(f.parent.begin(), f.parent.end()),
                 std::vector<std::uint32_t>(f.order.begin(), f.order.end())};
}

// G4 on any W x H grid (shared, P1-00 7.4): 8 on ocean.
inline std::vector<std::uint8_t> receivers(Grid grid, const Flood& f, std::span<const std::uint8_t> ocean,
                                           std::uint32_t threads) {
    const std::vector<worldgen::geo::Direction> parent(f.parent.begin(), f.parent.end());
    const std::vector<worldgen::geo::Direction> r = worldgen::geo::receivers(grid, f.level, parent, ocean, threads);
    std::vector<std::uint8_t> out(r.begin(), r.end());
    for (std::size_t i = 0; i < out.size(); ++i) {
        if (ocean[i] != 0U) {
            out[i] = 8;
        }
    }
    return out;
}

// The elevation stage's grid (P1-01 seam elevation_grid).
inline std::vector<std::int16_t> elevation_grid(std::uint64_t seed, std::int32_t n,
                                                const worldgen::stages::ElevationTuning& t, std::uint32_t threads) {
    return worldgen::stages::elevation_grid(seed, n, t, threads);
}

}  // namespace hydro_api
