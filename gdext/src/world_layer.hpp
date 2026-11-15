#pragma once

// WorldLayer: one decoded layer of a world file, as Godot sees it.
//
// Holds the integer grid returned by worldgen's reader. The cells stay
// integers; the only conversion is to_image(), which builds a Godot Image
// from worldgen's own render_rgb (so the viewer and `worldgen-cli render`
// show the same pixels). Nothing here flows back into worldgen.

#include <cstdint>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>

#include "worldgen/db/reader.hpp"

namespace unmapped {

class WorldLayer : public godot::RefCounted {
    GDCLASS(WorldLayer, godot::RefCounted)

public:
    WorldLayer() = default;

    // C++ side: takes ownership of a layer read by WorldFile.
    void set_layer(worldgen::db::Layer layer);
    [[nodiscard]] const worldgen::db::Layer& layer() const noexcept { return layer_; }

    // --- Bound to Godot ------------------------------------------------------
    [[nodiscard]] godot::String get_layer_name() const;
    [[nodiscard]] int64_t get_width() const;
    [[nodiscard]] int64_t get_height() const;
    // "u8", "i16", ... (the layer row's `format` text).
    [[nodiscard]] godot::String get_element_type() const;
    // "raw" or "delta_varint".
    [[nodiscard]] godot::String get_codec() const;
    // The palette worldgen registers for this layer ("gray" in P0).
    [[nodiscard]] godot::String get_palette() const;
    // The exact value of cell (x, y) in decimal; empty when out of range.
    // Text, because a u64 cell does not fit Godot's signed 64-bit int.
    [[nodiscard]] godot::String get_cell_text(int64_t x, int64_t y) const;
    // Minimum and maximum cell value in decimal.
    [[nodiscard]] godot::String get_min_text() const;
    [[nodiscard]] godot::String get_max_text() const;
    // RGB8 image of the layer, one pixel per cell, from worldgen's
    // render_rgb with the registered palette. Null if the grid is empty or
    // too large for a Godot Image.
    [[nodiscard]] godot::Ref<godot::Image> to_image() const;

protected:
    static void _bind_methods();

private:
    worldgen::db::Layer layer_;
};

// Text names used by the binding (shared with WorldFile).
[[nodiscard]] godot::String codec_text(worldgen::db::Codec codec);
[[nodiscard]] godot::String element_type_text(worldgen::db::ElementType type);

}  // namespace unmapped
