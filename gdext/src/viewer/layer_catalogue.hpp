#pragma once

// What the viewer knows about the layers of the catalogue (spec P1-00
// section 4): the group a layer is listed under, its value in display units,
// and the legend of a view. Model side: plain C++ on worldgen's public
// types, no Godot Controls, so a VR view or the in-game map can use it too.
//
// P1-00 4 rule 2: "Every layer is an integer type with a stated unit ... A
// reader converts to display units itself." worldgen's public API names the
// palette of a layer but not its unit, so the units are restated here from
// the catalogue table. A layer this file does not know is listed under
// "Other" and shown as its raw value.
//
// Legend colours come from worldgen's own palette functions and ramp stops
// (worldgen/inspect/render.hpp), so a swatch is exactly the colour drawn.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/render.hpp"

namespace unmapped::catalogue {

// The groups of the layer list, in list order.
enum class Group : std::uint8_t {
    composites = 0,
    terrain = 1,
    water = 2,
    climate = 3,
    biomes = 4,
    resources = 5,
    regions = 6,
    other = 7,
};

inline constexpr std::size_t GROUP_COUNT = 8;

[[nodiscard]] std::string_view group_title(Group group) noexcept;

// The group of a layer by name (P1-00 3: the stage that writes it).
[[nodiscard]] Group layer_group(std::string_view layer_name) noexcept;

// The catalogue's unit of a layer in a few words ("decimetres above sea
// level"), empty for a layer not in the catalogue.
[[nodiscard]] std::string_view layer_unit(std::string_view layer_name) noexcept;

// The value v of a cell of the layer in display units: "123.4 m" for
// elevation, "12.3 °C" for temperature, "river" for water 1, "142 / 255"
// for a score; the decimal value for a layer not in the catalogue. Biome
// and region names are not added here (they need the world's tables).
[[nodiscard]] std::string format_value(std::string_view layer_name, std::int64_t v);

// The value of cell (x, y) as int64; false if outside the grid or a u64
// value above INT64_MAX (shown as its decimal text instead).
[[nodiscard]] bool cell_value(const worldgen::db::LayerGrid& grid, std::uint32_t x, std::uint32_t y,
                              std::int64_t& value) noexcept;

// One row of a legend: a label and one colour (a swatch) or several (a
// gradient strip, drawn left to right), each 0xRRGGBB.
struct LegendEntry {
    std::string label;
    std::vector<std::uint32_t> colours;
};

// The legend of a layer drawn with palette (the layer's auto palette).
// grid gives the value range for gray and signed; biomes the world's table
// `biome` for the biome palette.
[[nodiscard]] std::vector<LegendEntry> layer_legend(std::string_view layer_name, worldgen::inspect::Palette palette,
                                                    const worldgen::db::LayerGrid& grid,
                                                    std::span<const worldgen::inspect::BiomeRecord> biomes);

// The legend of a composite (P1-00 10.2, P1-04 9, P0-04 amendment 18 (e)).
[[nodiscard]] std::vector<LegendEntry> composite_legend(worldgen::inspect::Composite composite,
                                                        std::span<const worldgen::inspect::BiomeRecord> biomes);

}  // namespace unmapped::catalogue
