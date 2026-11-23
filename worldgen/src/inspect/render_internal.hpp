#pragma once

// Steps of the composites of render.cpp shared with territory.cpp (spec
// P4-01 5.6). Internal to worldgen; not part of the public interface.

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/inspect/render.hpp"

namespace worldgen::inspect::detail {

// The byte size of a composite's RGB buffer, 3 (w scale) (h scale).
// FatalError `what` when scale is outside 1..16, "render: image too large"
// when the size does not fit in std::size_t.
[[nodiscard]] std::size_t composite_buffer_size(std::uint32_t w, std::uint32_t h, std::uint32_t scale,
                                                const char* what);

// The composite regions at scale 1, per cell in index order: its colour (the
// pixel of render_composite_regions), whether step 3 or step 4 of the
// composite map drew it (a lake cell, a river cell without layer river, or a
// cell an entry of the river paint list was painted on), and whether it is a
// border cell drawn black. The layers must pass composite_regions_problem.
struct RegionsCells {
    std::vector<Rgb> colour;
    std::vector<std::uint8_t> drawn;   // 1: drawn by step 3 or 4
    std::vector<std::uint8_t> border;  // 1: a border cell (black)
};
[[nodiscard]] RegionsCells regions_composite_cells(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                                   const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                                   const db::LayerGrid& biome, const db::LayerGrid& region,
                                                   std::span<const BiomeColour> biome_table);

}  // namespace worldgen::inspect::detail
