#pragma once

// ViewerModel: the model of the world viewer screen (design doc section 8,
// "each screen has one model and two views").
//
// Holds the open world, the list of views, the selected view and the decoded
// images, and reports changes by signals. It knows nothing about Controls:
// the flat view (ViewerScreen) reads it now, and a VR view can read the same
// model later.
//
// A view is one map the screen can show: first every layer of the file, in
// id order (drawn with worldgen's palette for it; the `biome` layer with the
// world's table `biome`), then every composite worldgen can build from those
// layers ("map", "biomes", "regions"; WorldFile::get_composite_names). It
// lists whatever the file holds, so layers added by later phases appear
// without changes here. Each view has a group (layer_catalogue.hpp:
// "Composites", "Terrain", "Water", ...) for the list to be grouped by; the
// order of views is not the order a view shows them in.
//
// The model also gives the legend of the selected view and a description of
// any cell (value in display units, biome and region names), both from the
// world file's tables and worldgen's palettes.

#include <cstdint>
#include <string_view>
#include <vector>

#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

#include "world_file.hpp"
#include "world_layer.hpp"

namespace unmapped {

class ViewerModel : public godot::RefCounted {
    GDCLASS(ViewerModel, godot::RefCounted)

public:
    ViewerModel();

    // --- Bound to Godot ------------------------------------------------------
    // Signals:
    //   world_opened(path: String)
    //   world_open_failed(path: String, code: String, message: String)
    //   world_closed()
    //   view_selected(index: int)
    //   view_failed(index: int, code: String, message: String)

    // Opens a world file (read-only). Emits world_opened, then selects the
    // first view if there is one; or world_open_failed.
    bool open_world(const godot::String& path);
    void close_world();
    [[nodiscard]] bool has_world() const;
    [[nodiscard]] godot::String get_world_path() const;
    [[nodiscard]] godot::String get_error_code() const;
    [[nodiscard]] godot::String get_error_message() const;

    [[nodiscard]] godot::Array get_meta_entries() const;

    // The layers of the file (views 0 .. layer_count - 1).
    [[nodiscard]] int64_t get_layer_count() const;
    // The composites (views layer_count .. view_count - 1).
    [[nodiscard]] int64_t get_composite_count() const;
    [[nodiscard]] int64_t get_view_count() const;
    // Dictionary {kind ("layer" or "composite"), name, group, unit, width,
    // height, element_type, palette, codec, encoded_size}. group is the
    // title of its group in the layer list; unit the catalogue's unit of a
    // layer ("" when unknown or for a composite). For a layer, the
    // WorldFile layer info plus kind and palette ("" until decoded); for a
    // composite, name is the composite's ("map"), width and height those of
    // `elevation`, element_type and palette "composite", codec "" and
    // encoded_size 0. Empty for an index out of range.
    [[nodiscard]] godot::Dictionary get_view_info(int64_t index) const;

    // Selects a view, decoding it on first use. Emits view_selected, or
    // view_failed and keeps the previous selection.
    bool select_view(int64_t index);
    [[nodiscard]] int64_t get_selected_view() const;

    // The image of a view (worldgen render_rgb or composite); null if it has
    // not been decoded or failed.
    [[nodiscard]] godot::Ref<godot::Image> get_view_image(int64_t index) const;
    // The decoded layer of a layer view; null otherwise.
    [[nodiscard]] godot::Ref<WorldLayer> get_layer(int64_t index) const;

    // What the selected view holds at a cell, as text; empty outside. A
    // layer: its exact value in decimal. A composite: "elevation <value>".
    [[nodiscard]] godot::String get_cell_text(int64_t x, int64_t y) const;

    // A description of cell (x, y) for the selected view, one line: the
    // cell, the view's value in display units (a composite: the elevation),
    // then, when the world has them, the cell's biome name (table `biome`)
    // and region id and name (table `region`). Empty outside the map or
    // without a selection. Decodes `biome` and `region` on first use.
    godot::String describe_cell(int64_t x, int64_t y);

    // The legend of the selected view: an Array of Dictionary {label,
    // colours (PackedInt32Array of 0xRRGGBB: one is a swatch, several a
    // gradient strip)}. Empty without a selection.
    [[nodiscard]] godot::Array get_legend() const;
    // A heading for the legend: the view's name and what its colours show.
    [[nodiscard]] godot::String get_legend_title() const;

    // The underlying data-access object.
    [[nodiscard]] godot::Ref<WorldFile> get_world() const;

protected:
    static void _bind_methods();

private:
    void reset_views();
    bool decode_layer(int64_t layer_index);
    bool build_image(int64_t view_index);
    // The grid of a layer by name, decoded on first use; null if absent or
    // unreadable.
    const worldgen::db::LayerGrid* layer_grid(std::string_view name);

    godot::Ref<WorldFile> world_;
    std::vector<worldgen::inspect::Composite> composites_;
    std::vector<godot::Ref<WorldLayer>> layers_;   // per layer
    std::vector<godot::Ref<godot::Image>> images_;  // per view
    godot::Array legend_;                           // of the selected view
    int64_t selected_ = -1;
    godot::String error_code_;
    godot::String error_message_;
};

}  // namespace unmapped
