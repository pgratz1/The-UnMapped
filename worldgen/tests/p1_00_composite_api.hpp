#pragma once

// The one place where the composite-map tests call the library's composite
// functions (P1-00 10.2, amended 2026-11-16: river drawing, step 4, which
// reads `flow_dir`). Seams as P1-00 10.2 now gives them (namespace
// worldgen::inspect, render.hpp): composite_map_problem and
// render_composite_map with and without a trailing flow_dir layer (null =
// absent; the short forms equal the long ones with flow_dir null), and
// composite_river_colour(k), the step 4 ramp ((255, 0, 255) for class 0).
// Integration adapts this file only.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/inspect/render.hpp"

namespace comp_api {

using worldgen::db::LayerGrid;

inline std::optional<std::string> problem(const LayerGrid& elevation, const LayerGrid* water, const LayerGrid* river,
                                          const LayerGrid* flow_dir) {
    return worldgen::inspect::composite_map_problem(elevation, water, river, flow_dir);
}

inline std::optional<std::string> problem3(const LayerGrid& elevation, const LayerGrid* water,
                                           const LayerGrid* river) {
    return worldgen::inspect::composite_map_problem(elevation, water, river);
}

inline std::vector<std::uint8_t> render_map3(const LayerGrid& elevation, const LayerGrid* water,
                                             const LayerGrid* river, std::uint32_t scale) {
    return worldgen::inspect::render_composite_map(elevation, water, river, scale);
}

inline worldgen::inspect::Rgb river_colour(std::uint64_t k) { return worldgen::inspect::composite_river_colour(k); }

inline std::vector<std::uint8_t> render_map(const LayerGrid& elevation, const LayerGrid* water, const LayerGrid* river,
                                            const LayerGrid* flow_dir, std::uint32_t scale) {
    return worldgen::inspect::render_composite_map(elevation, water, river, flow_dir, scale);
}

}  // namespace comp_api
