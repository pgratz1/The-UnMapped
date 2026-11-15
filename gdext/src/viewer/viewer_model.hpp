#pragma once

// ViewerModel: the model of the world viewer screen (design doc section 8,
// "each screen has one model and two views").
//
// Holds the open world, the selected layer and the decoded layer images, and
// reports changes by signals. It knows nothing about Controls: the flat view
// (ViewerScreen) reads it now, and a VR view can read the same model later.
// It lists whatever layers the file holds, so layers added by later phases
// appear without changes here.

#include <cstdint>
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
    //   layer_selected(index: int)
    //   layer_failed(index: int, code: String, message: String)

    // Opens a world file (read-only). Emits world_opened, then selects the
    // first layer if there is one; or world_open_failed.
    bool open_world(const godot::String& path);
    void close_world();
    [[nodiscard]] bool has_world() const;
    [[nodiscard]] godot::String get_world_path() const;
    [[nodiscard]] godot::String get_error_code() const;
    [[nodiscard]] godot::String get_error_message() const;

    [[nodiscard]] godot::Array get_meta_entries() const;
    [[nodiscard]] int64_t get_layer_count() const;
    [[nodiscard]] godot::Dictionary get_layer_info(int64_t index) const;

    // Selects a layer, decoding it on first use. Emits layer_selected, or
    // layer_failed and keeps the previous selection.
    bool select_layer(int64_t index);
    [[nodiscard]] int64_t get_selected_layer() const;

    // The decoded layer and its image (worldgen render_rgb); null if the
    // layer has not been decoded or failed.
    [[nodiscard]] godot::Ref<WorldLayer> get_layer(int64_t index) const;
    [[nodiscard]] godot::Ref<godot::Image> get_layer_image(int64_t index) const;

    // Exact value of a cell of the selected layer, decimal; empty outside.
    [[nodiscard]] godot::String get_cell_text(int64_t x, int64_t y) const;

    // The underlying data-access object.
    [[nodiscard]] godot::Ref<WorldFile> get_world() const;

protected:
    static void _bind_methods();

private:
    bool decode(int64_t index);

    godot::Ref<WorldFile> world_;
    std::vector<godot::Ref<WorldLayer>> layers_;
    std::vector<godot::Ref<godot::Image>> images_;
    int64_t selected_ = -1;
    godot::String error_code_;
    godot::String error_message_;
};

}  // namespace unmapped
