#pragma once

// The one place where the P1-02 hydrology tests touch the stage's API.
//
// The tests were written from the spec before the implementation existed
// (test-writer rule), against the test-owned functions below, which take and
// return plain integer arrays. Integration adapts this file only.
//
// Seams used (P1-02 section 8: "hydrology_layers(elevation grid, tuning,
// threads) returning the seven grids; and the shared G3, G4, G5 functions on
// any W x H grid"). Where the spec names a seam but not its signature, the
// assumption is written beside the call:
//
//   header        worldgen/stages/hydrology.hpp                   (assumed)
//   tuning        worldgen::stages::HydrologyTuning,
//                 hydrology_tuning_from(const data::ContentSet&)  (assumed, as elevation_tuning_from)
//   stage         worldgen::stages::hydrology_layers(geo::Grid, std::span<const std::int16_t> elevation,
//                     const HydrologyTuning&, std::uint32_t threads) -> HydrologyLayers   (assumed)
//                 HydrologyLayers has the seven vectors named and typed as the layers of
//                 P1-02 section 3: water (u8), water_depth (u16), flow_dir (u8),
//                 flow_accum (u32), river (u8), lake (u32), coast_dist (i16)          (assumed)
//                 A precondition violation throws worldgen::FatalError whose message
//                 starts "hydrology: elevation precondition: " (P1-02 section 2).
//   H7 alone      worldgen::stages::chamfer_distance(geo::Grid, std::span<const std::uint8_t> source)
//                 (exists on main for P1-01 E5.3, declared there as "P1-02 H7"; 8.1 item 8
//                 tests "the H7 function alone")
//   G3 to G5      worldgen::geo::routing_flood, receivers, accumulate_topological (main)
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

// The whole stage on any W x H grid (seam hydrology_layers).
inline Layers layers(Grid grid, std::span<const std::int16_t> elevation, const Tuning& t, std::uint32_t threads) {
    worldgen::stages::HydrologyLayers r = worldgen::stages::hydrology_layers(grid, elevation, t, threads);
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
