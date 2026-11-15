#include "viewer/viewer_model.hpp"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>

namespace unmapped {

ViewerModel::ViewerModel() {
    world_.instantiate();
}

bool ViewerModel::open_world(const godot::String& path) {
    const bool had_world = has_world();
    layers_.clear();
    images_.clear();
    selected_ = -1;
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
    const auto count = static_cast<std::size_t>(world_->get_layer_count());
    layers_.resize(count);
    images_.resize(count);
    emit_signal("world_opened", path);
    if (count > 0) {
        select_layer(0);
    }
    return true;
}

void ViewerModel::close_world() {
    if (!has_world()) {
        return;
    }
    world_->close();
    layers_.clear();
    images_.clear();
    selected_ = -1;
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

godot::Dictionary ViewerModel::get_layer_info(int64_t index) const {
    return world_->get_layer_info(index);
}

bool ViewerModel::decode(int64_t index) {
    const auto i = static_cast<std::size_t>(index);
    if (layers_[i].is_valid()) {
        return true;
    }
    const godot::String name = world_->get_layer_info(index)["name"];
    godot::Ref<WorldLayer> layer = world_->read_layer(name);
    if (layer.is_null()) {
        error_code_ = world_->get_error_code();
        error_message_ = world_->get_error_message();
        return false;
    }
    godot::Ref<godot::Image> image = layer->to_image();
    if (image.is_null()) {
        error_code_ = "not_displayable";
        error_message_ = "Layer " + name + " cannot be shown as an image";
        return false;
    }
    layers_[i] = layer;
    images_[i] = image;
    return true;
}

bool ViewerModel::select_layer(int64_t index) {
    if (index < 0 || index >= get_layer_count()) {
        return false;
    }
    if (!decode(index)) {
        emit_signal("layer_failed", index, error_code_, error_message_);
        return false;
    }
    error_code_ = godot::String();
    error_message_ = godot::String();
    selected_ = index;
    emit_signal("layer_selected", index);
    return true;
}

int64_t ViewerModel::get_selected_layer() const {
    return selected_;
}

godot::Ref<WorldLayer> ViewerModel::get_layer(int64_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= layers_.size()) {
        return {};
    }
    return layers_[static_cast<std::size_t>(index)];
}

godot::Ref<godot::Image> ViewerModel::get_layer_image(int64_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= images_.size()) {
        return {};
    }
    return images_[static_cast<std::size_t>(index)];
}

godot::String ViewerModel::get_cell_text(int64_t x, int64_t y) const {
    const godot::Ref<WorldLayer> layer = get_layer(selected_);
    return layer.is_valid() ? layer->get_cell_text(x, y) : godot::String();
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
    ClassDB::bind_method(D_METHOD("get_layer_info", "index"), &ViewerModel::get_layer_info);
    ClassDB::bind_method(D_METHOD("select_layer", "index"), &ViewerModel::select_layer);
    ClassDB::bind_method(D_METHOD("get_selected_layer"), &ViewerModel::get_selected_layer);
    ClassDB::bind_method(D_METHOD("get_layer", "index"), &ViewerModel::get_layer);
    ClassDB::bind_method(D_METHOD("get_layer_image", "index"), &ViewerModel::get_layer_image);
    ClassDB::bind_method(D_METHOD("get_cell_text", "x", "y"), &ViewerModel::get_cell_text);
    ClassDB::bind_method(D_METHOD("get_world"), &ViewerModel::get_world);

    ADD_SIGNAL(MethodInfo("world_opened", PropertyInfo(Variant::STRING, "path")));
    ADD_SIGNAL(MethodInfo("world_open_failed", PropertyInfo(Variant::STRING, "path"),
                          PropertyInfo(Variant::STRING, "code"), PropertyInfo(Variant::STRING, "message")));
    ADD_SIGNAL(MethodInfo("world_closed"));
    ADD_SIGNAL(MethodInfo("layer_selected", PropertyInfo(Variant::INT, "index")));
    ADD_SIGNAL(MethodInfo("layer_failed", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::STRING, "code"),
                          PropertyInfo(Variant::STRING, "message")));
}

}  // namespace unmapped
