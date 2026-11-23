#pragma once

// Steps of the composites of render.cpp shared with territory.cpp (spec
// P4-01 5.6, P4-03 9.3). Internal to worldgen; not part of the public interface.

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

// Fills rgb (of composite_buffer_size bytes) with colour_of(index) for each cell,
// each cell a scale x scale block, in index order. Every byte is written.
template <class ColourOf>
void fill_buffer(std::span<std::uint8_t> rgb, std::uint32_t width, std::uint32_t height, std::uint32_t scale,
                 const ColourOf& colour_of) {
    const std::size_t w = width;
    const std::size_t h = height;
    const std::size_t s = scale;
    const std::size_t row_bytes = w * s * 3U;
    for (std::size_t y = 0; y < h; ++y) {
        // Fill the first pixel row of this cell row, then copy it to the
        // other scale - 1 rows.
        std::uint8_t* row = rgb.data() + (y * s) * row_bytes;
        for (std::size_t x = 0; x < w; ++x) {
            const Rgb c = colour_of(y * w + x);
            std::uint8_t* px = row + x * s * 3U;
            for (std::size_t i = 0; i < s; ++i) {
                px[3U * i] = c.r;
                px[3U * i + 1U] = c.g;
                px[3U * i + 2U] = c.b;
            }
        }
        for (std::size_t j = 1; j < s; ++j) {
            std::uint8_t* dst = row + j * row_bytes;
            for (std::size_t b = 0; b < row_bytes; ++b) {
                dst[b] = row[b];
            }
        }
    }
}

// The composite regions at scale 1, per cell in index order: its colour (the
// pixel of render_composite_regions), whether step 3 or step 4 of the
// composite map drew it (a lake cell, a river cell without layer river, or a
// cell an entry of the river paint list was painted on), and whether it is a
// border cell drawn black. The layers must pass composite_regions_problem.
struct RegionsCells {
    std::vector<Rgb> colour;
    std::vector<std::uint8_t> drawn;   // 1: drawn by step 3 or 4
    std::vector<std::uint8_t> border;  // 1: a border cell (black)
    // On a border cell, the E (S) neighbour's region value when that
    // neighbour makes it a border cell, else 0 (P4-03 8.2); 0 elsewhere.
    std::vector<std::uint16_t> east;
    std::vector<std::uint16_t> south;
    // On a border cell, the colour it had before the border pass blackened
    // it (P4-03a: the unbordered colour U of 8.2 rule 3); (0, 0, 0) elsewhere.
    std::vector<Rgb> unbordered;
};
[[nodiscard]] RegionsCells regions_composite_cells(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                                   const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                                   const db::LayerGrid& biome, const db::LayerGrid& region,
                                                   std::span<const BiomeColour> biome_table);

}  // namespace worldgen::inspect::detail
