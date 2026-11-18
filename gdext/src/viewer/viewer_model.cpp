#include "viewer/viewer_model.hpp"

#include <string_view>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>

namespace unmapped {

namespace {

godot::String to_godot(std::string_view text) {
    return godot::String::utf8(text.data(), static_cast<int64_t>(text.size()));
}

}  // namespace

ViewerModel::ViewerModel() {
    world_.instantiate();
}

void ViewerModel::reset_views() {
    composites_.clear();
    layers_.clear();
    images_.clear();
    selected_ = -1;
}

bool ViewerModel::open_world(const godot::String& path) {
    const bool had_world = has_world();
    reset_views();
    if (had_world) {
        world_->close();
        emit_signal("world_closed");
    }
    if (!world_->open(path)) {
        error_code_ = world_->get_error_code();
        error_message_ = world_->get_error_message();
        emit_signal("world_open_failed", path, error_code_, error_message_);
        return false;
    }
    error_code_ = godot::String();
    error_message_ = godot::String();
    composites_ = world_->composites();
    layers_.resize(static_cast<std::size_t>(get_layer_count()));
    images_.resize(static_cast<std::size_t>(get_view_count()));
    emit_signal("world_opened", path);
    if (get_view_count() > 0) {
        select_view(0);
    }
    return true;
}

void ViewerModel::close_world() {
    if (!has_world()) {
        return;
    }
    world_->close();
    reset_views();
    emit_signal("world_closed");
}

bool ViewerModel::has_world() const {
    return world_->is_open();
}

godot::String ViewerModel::get_world_path() const {
    return world_->get_path();
}

godot::String ViewerModel::get_error_code() const {
    return error_code_;
}

godot::String ViewerModel::get_error_message() const {
    return error_message_;
}

godot::Array ViewerModel::get_meta_entries() const {
    return world_->get_meta_entries();
}

int64_t ViewerModel::get_layer_count() const {
    return world_->get_layer_count();
}

int64_t ViewerModel::get_composite_count() const {
    return static_cast<int64_t>(composites_.size());
}

int64_t ViewerModel::get_view_count() const {
    return get_layer_count() + get_composite_count();
}

godot::Dictionary ViewerModel::get_view_info(int64_t index) const {
    if (index < 0 || index >= get_view_count()) {
        return {};
    }
    if (index < get_layer_count()) {
        godot::Dictionary info = world_->get_layer_info(index);
        info["kind"] = "layer";
        const godot::Ref<WorldLayer> layer = get_layer(index);
        info["palette"] = layer.is_valid() ? layer->get_palette() : godot::String();
        return info;
    }
    const auto composite = composites_[static_cast<std::size_t>(index - get_layer_count())];
    godot::Dictionary info;
    info["kind"] = "composite";
    info["name"] = to_godot(worldgen::inspect::composite_name(composite));
    const godot::Dictionary elevation = world_->get_layer_info(world_->find_layer("elevation"));
    info["width"] = elevation.get("width", 0);
    info["height"] = elevation.get("height", 0);
    info["element_type"] = "composite";
    info["palette"] = "composite";
    info["codec"] = "";
    info["encoded_size"] = 0;
    return info;
}

bool ViewerModel::decode_layer(int64_t layer_index) {
    const auto i = static_cast<std::size_t>(layer_index);
    if (layers_[i].is_valid()) {
        return true;
    }
    const godot::String name = world_->get_layer_info(layer_index)["name"];
    godot::Ref<WorldLayer> layer = world_->read_layer(name);
    if (layer.is_null()) {
        error_code_ = world_->get_error_code();
        error_message_ = world_->get_error_message();
        return false;
    }
    layers_[i] = layer;
    return true;
}

bool ViewerModel::build_image(int64_t view_index) {
    const auto v = static_cast<std::size_t>(view_index);
    if (images_[v].is_valid()) {
        return true;
    }
    if (view_index < get_layer_count()) {
        if (!decode_layer(view_index)) {
            return false;
        }
        godot::Ref<godot::Image> image = layers_[v]->to_image();
        if (image.is_null()) {
            error_code_ = "not_displayable";
            error_message_ = "Layer " + layers_[v]->get_layer_name() + " cannot be shown as an image";
            return false;
        }
        images_[v] = image;
        return true;
    }

    // A composite: decode the layers it is built from (kept for reuse).
    const auto composite = composites_[static_cast<std::size_t>(view_index - get_layer_count())];
    const godot::String name = to_godot(worldgen::inspect::composite_name(composite));
    CompositeLayers parts;
    bool failed = false;
    const auto grid = [&](std::string_view layer_name) -> const worldgen::db::LayerGrid* {
        const int64_t index = world_->find_layer(layer_name);
        if (index < 0 || failed) {
            return nullptr;
        }
        if (!decode_layer(index)) {
            failed = true;
            return nullptr;
        }
        return &layers_[static_cast<std::size_t>(index)]->layer().grid;
    };
    parts.elevation = grid("elevation");
    if (composite == worldgen::inspect::Composite::biomes) {
        parts.biome = grid("biome");
    }
    parts.water = grid("water");
    parts.river = grid("river");
    parts.flow_dir = grid("flow_dir");
    if (failed) {
        return false;  // error_* set by decode_layer
    }
    godot::String problem;
    godot::Ref<godot::Image> image = composite_image(composite, parts, world_->biome_colours(), problem);
    if (image.is_null()) {
        error_code_ = "not_displayable";
        error_message_ = "Composite " + name + " cannot be shown" + (problem.is_empty() ? "" : ": " + problem);
        return false;
    }
    images_[v] = image;
    return true;
}

bool ViewerModel::select_view(int64_t index) {
    if (index < 0 || index >= get_view_count()) {
        return false;
    }
    if (!build_image(index)) {
        emit_signal("view_failed", index, error_code_, error_message_);
        return false;
    }
    error_code_ = godot::String();
    error_message_ = godot::String();
    selected_ = index;
    emit_signal("view_selected", index);
    return true;
}

int64_t ViewerModel::get_selected_view() const {
    return selected_;
}

godot::Ref<WorldLayer> ViewerModel::get_layer(int64_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= layers_.size()) {
        return {};
    }
    return layers_[static_cast<std::size_t>(index)];
}

godot::Ref<godot::Image> ViewerModel::get_view_image(int64_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= images_.size()) {
        return {};
    }
    return images_[static_cast<std::size_t>(index)];
}

godot::String ViewerModel::get_cell_text(int64_t x, int64_t y) const {
    if (selected_ < 0) {
        return {};
    }
    if (selected_ < get_layer_count()) {
        const godot::Ref<WorldLayer> layer = get_layer(selected_);
        return layer.is_valid() ? layer->get_cell_text(x, y) : godot::String();
    }
    const godot::Ref<WorldLayer> elevation = get_layer(world_->find_layer("elevation"));
    if (elevation.is_null()) {
        return {};
    }
    const godot::String value = elevation->get_cell_text(x, y);
    return value.is_empty() ? value : "elevation " + value;
}

godot::Ref<WorldFile> ViewerModel::get_world() const {
    return world_;
}

void ViewerModel::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    using godot::MethodInfo;
    using godot::PropertyInfo;
    using godot::Variant;

    ClassDB::bind_method(D_METHOD("open_world", "path"), &ViewerModel::open_world);
    ClassDB::bind_method(D_METHOD("close_world"), &ViewerModel::close_world);
    ClassDB::bind_method(D_METHOD("has_world"), &ViewerModel::has_world);
    ClassDB::bind_method(D_METHOD("get_world_path"), &ViewerModel::get_world_path);
    ClassDB::bind_method(D_METHOD("get_error_code"), &ViewerModel::get_error_code);
    ClassDB::bind_method(D_METHOD("get_error_message"), &ViewerModel::get_error_message);
    ClassDB::bind_method(D_METHOD("get_meta_entries"), &ViewerModel::get_meta_entries);
    ClassDB::bind_method(D_METHOD("get_layer_count"), &ViewerModel::get_layer_count);
    ClassDB::bind_method(D_METHOD("get_composite_count"), &ViewerModel::get_composite_count);
    ClassDB::bind_method(D_METHOD("get_view_count"), &ViewerModel::get_view_count);
    ClassDB::bind_method(D_METHOD("get_view_info", "index"), &ViewerModel::get_view_info);
    ClassDB::bind_method(D_METHOD("select_view", "index"), &ViewerModel::select_view);
    ClassDB::bind_method(D_METHOD("get_selected_view"), &ViewerModel::get_selected_view);
    ClassDB::bind_method(D_METHOD("get_view_image", "index"), &ViewerModel::get_view_image);
    ClassDB::bind_method(D_METHOD("get_layer", "index"), &ViewerModel::get_layer);
    ClassDB::bind_method(D_METHOD("get_cell_text", "x", "y"), &ViewerModel::get_cell_text);
    ClassDB::bind_method(D_METHOD("get_world"), &ViewerModel::get_world);

    ADD_SIGNAL(MethodInfo("world_opened", PropertyInfo(Variant::STRING, "path")));
    ADD_SIGNAL(MethodInfo("world_open_failed", PropertyInfo(Variant::STRING, "path"),
                          PropertyInfo(Variant::STRING, "code"), PropertyInfo(Variant::STRING, "message")));
    ADD_SIGNAL(MethodInfo("world_closed"));
    ADD_SIGNAL(MethodInfo("view_selected", PropertyInfo(Variant::INT, "index")));
    ADD_SIGNAL(MethodInfo("view_failed", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::STRING, "code"),
                          PropertyInfo(Variant::STRING, "message")));
}

}  // namespace unmapped
