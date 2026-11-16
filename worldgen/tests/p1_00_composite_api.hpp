#pragma once

// The one place where the composite-map tests call the library's composite
// functions (P1-00 10.2, amended 2026-11-16: river drawing, step 4, which
// reads `flow_dir`). The amendment names no signature; the assumption is
// that `flow_dir` is passed as one more optional layer after `river`:
//
//   composite_map_problem(elevation, water, river, flow_dir)
//   render_composite_map(elevation, water, river, flow_dir, scale)
//
// with flow_dir null when the world has no such layer. Integration adapts
// this file only.

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

inline std::vector<std::uint8_t> render_map(const LayerGrid& elevation, const LayerGrid* water, const LayerGrid* river,
                                            const LayerGrid* flow_dir, std::uint32_t scale) {
    return worldgen::inspect::render_composite_map(elevation, water, river, flow_dir, scale);
}

}  // namespace comp_api
