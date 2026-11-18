#pragma once

// WorldLayer: one decoded layer of a world file, as Godot sees it.
//
// Holds the integer grid returned by worldgen's reader. The cells stay
// integers; the only conversion is to_image(), which builds a Godot Image
// from worldgen's own render_rgb (so the viewer and `worldgen-cli render`
// show the same pixels). Nothing here flows back into worldgen.

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

#include "worldgen/db/reader.hpp"
#include "worldgen/inspect/render.hpp"

namespace unmapped {

class WorldLayer : public godot::RefCounted {
    GDCLASS(WorldLayer, godot::RefCounted)

public:
    WorldLayer() = default;

    // C++ side: takes ownership of a layer read by WorldFile. The palette is
    // worldgen's auto_palette for the layer (as `worldgen-cli render
    // --palette auto`); biome_table is the world's table `biome`, used only
    // when that palette is biome (P1-00 10.2: without it every cell would be
    // magenta).
    void set_layer(worldgen::db::Layer layer, std::vector<worldgen::inspect::BiomeColour> biome_table = {});
    [[nodiscard]] const worldgen::db::Layer& layer() const noexcept { return layer_; }
    [[nodiscard]] worldgen::inspect::Palette palette() const noexcept { return palette_; }

    // --- Bound to Godot ------------------------------------------------------
    [[nodiscard]] godot::String get_layer_name() const;
    [[nodiscard]] int64_t get_width() const;
    [[nodiscard]] int64_t get_height() const;
    // "u8", "i16", ... (the layer row's `format` text).
    [[nodiscard]] godot::String get_element_type() const;
    // "raw" or "delta_varint".
    [[nodiscard]] godot::String get_codec() const;
    // The palette the layer is drawn with: worldgen's auto_palette, i.e. the
    // registered palette of the name ("hypsometric", "biome", ...) when it
    // suits the grid, otherwise "gray".
    [[nodiscard]] godot::String get_palette() const;
    // The exact value of cell (x, y) in decimal; empty when out of range.
    // Text, because a u64 cell does not fit Godot's signed 64-bit int.
    [[nodiscard]] godot::String get_cell_text(int64_t x, int64_t y) const;
    // Minimum and maximum cell value in decimal.
    [[nodiscard]] godot::String get_min_text() const;
    [[nodiscard]] godot::String get_max_text() const;
    // RGB8 image of the layer, one pixel per cell, from worldgen's
    // render_rgb with get_palette() (and the biome table for biome). Null if
    // the grid is empty or too large for a Godot Image.
    [[nodiscard]] godot::Ref<godot::Image> to_image() const;

protected:
    static void _bind_methods();

private:
    worldgen::db::Layer layer_;
    worldgen::inspect::Palette palette_ = worldgen::inspect::Palette::gray;
    std::vector<worldgen::inspect::BiomeColour> biome_table_;
};

// Text names used by the binding (shared with WorldFile).
[[nodiscard]] godot::String codec_text(worldgen::db::Codec codec);
[[nodiscard]] godot::String element_type_text(worldgen::db::ElementType type);

// Whether a width x height RGB8 image is within Godot's Image limits.
[[nodiscard]] bool fits_godot_image(std::uint64_t width, std::uint64_t height) noexcept;

// An RGB8 Image of width x height pixels filled by fill(out), where out is
// the image's 3 width height bytes. Null (and an engine error naming what)
// if the size exceeds Godot's limits or fill throws. Rendering straight into
// the image's buffer avoids a second copy.
[[nodiscard]] godot::Ref<godot::Image> make_rgb_image(std::uint64_t width, std::uint64_t height,
                                                      const godot::String& what,
                                                      const std::function<void(std::span<std::uint8_t>)>& fill);

// The layers a composite image is built from (worldgen render_composite_map
// and render_composite_biomes). elevation is required, biome for the
// composite biomes; null water, river or flow_dir are absent layers.
struct CompositeLayers {
    const worldgen::db::LayerGrid* elevation = nullptr;
    const worldgen::db::LayerGrid* biome = nullptr;
    const worldgen::db::LayerGrid* water = nullptr;
    const worldgen::db::LayerGrid* river = nullptr;
    const worldgen::db::LayerGrid* flow_dir = nullptr;
};

// RGB8 image of a composite at scale 1, the same pixels as `worldgen-cli
// render --composite <name>`. Null with problem set (worldgen's text, e.g.
// "layer 'water' is i16, expected u8") when the layers do not suit it.
[[nodiscard]] godot::Ref<godot::Image> composite_image(worldgen::inspect::Composite composite,
                                                       const CompositeLayers& layers,
                                                       std::span<const worldgen::inspect::BiomeColour> biome_table,
                                                       godot::String& problem);

}  // namespace unmapped
