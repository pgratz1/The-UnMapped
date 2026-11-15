#include "viewer/layer_map_view.hpp"

#include <algorithm>
#include <cmath>

#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>

namespace unmapped {

namespace {

constexpr double MIN_ZOOM = 1.0 / 64.0;
constexpr double MAX_ZOOM = 64.0;
constexpr double WHEEL_FACTOR = 1.25;

}  // namespace

void LayerMapView::set_image(const godot::Ref<godot::Image>& image) {
    if (image.is_null()) {
        clear_image();
        return;
    }
    const godot::Vector2i size = image->get_size();
    const bool new_size = size != image_size_ || texture_.is_null();
    image_ = image;
    if (texture_.is_valid() && !new_size) {
        texture_->update(image);
    } else {
        texture_ = godot::ImageTexture::create_from_image(image);
    }
    image_size_ = size;
    if (new_size) {
        fit_to_view();
    }
    queue_redraw();
}

godot::Ref<godot::Image> LayerMapView::get_image() const {
    return image_;
}

void LayerMapView::clear_image() {
    image_.unref();
    texture_.unref();
    image_size_ = godot::Vector2i();
    hovered_ = godot::Vector2i(-1, -1);
    queue_redraw();
}

void LayerMapView::fit_to_view() {
    // Stays fitted through later resizes (the first layout passes give
    // temporary sizes) until the user pans or zooms.
    auto_fit_ = true;
    apply_fit();
}

void LayerMapView::apply_fit() {
    const godot::Vector2 view = get_size();
    if (image_size_.x <= 0 || image_size_.y <= 0 || view.x <= 0.0F || view.y <= 0.0F) {
        return;
    }
    const double zx = static_cast<double>(view.x) / static_cast<double>(image_size_.x);
    const double zy = static_cast<double>(view.y) / static_cast<double>(image_size_.y);
    zoom_ = std::clamp(std::min(zx, zy), MIN_ZOOM, MAX_ZOOM);
    const godot::Vector2 shown = godot::Vector2(image_size_) * static_cast<float>(zoom_);
    offset_ = (view - shown) * 0.5F;
    queue_redraw();
}

godot::Rect2 LayerMapView::get_shown_rect() const {
    if (texture_.is_null()) {
        return {};
    }
    return {offset_, godot::Vector2(image_size_) * static_cast<float>(zoom_)};
}

void LayerMapView::set_zoom(double zoom) {
    zoom_about(get_size() * 0.5F, zoom / zoom_);
}

double LayerMapView::get_zoom() const {
    return zoom_;
}

void LayerMapView::zoom_about(godot::Vector2 anchor, double factor) {
    auto_fit_ = false;
    const double zoom = std::clamp(zoom_ * factor, MIN_ZOOM, MAX_ZOOM);
    const auto applied = static_cast<float>(zoom / zoom_);
    offset_ = anchor - (anchor - offset_) * applied;
    zoom_ = zoom;
    queue_redraw();
}

godot::Vector2i LayerMapView::cell_at(godot::Vector2 position) const {
    if (texture_.is_null()) {
        return {-1, -1};
    }
    const godot::Vector2 local = (position - offset_) / static_cast<float>(zoom_);
    const auto x = static_cast<int32_t>(std::floor(local.x));
    const auto y = static_cast<int32_t>(std::floor(local.y));
    if (x < 0 || y < 0 || x >= image_size_.x || y >= image_size_.y) {
        return {-1, -1};
    }
    return {x, y};
}

void LayerMapView::update_hover(godot::Vector2 position) {
    const godot::Vector2i cell = cell_at(position);
    if (cell != hovered_) {
        hovered_ = cell;
        emit_signal("cell_hovered", cell.x, cell.y);
    }
}

void LayerMapView::_draw() {
    if (texture_.is_null()) {
        return;
    }
    draw_texture_rect(texture_, get_shown_rect(), false);
}

void LayerMapView::_gui_input(const godot::Ref<godot::InputEvent>& event) {
    const godot::Ref<godot::InputEventMouseButton> button = event;
    if (button.is_valid()) {
        const auto index = button->get_button_index();
        if (button->is_pressed() && index == godot::MOUSE_BUTTON_WHEEL_UP) {
            zoom_about(button->get_position(), WHEEL_FACTOR);
            accept_event();
        } else if (button->is_pressed() && index == godot::MOUSE_BUTTON_WHEEL_DOWN) {
            zoom_about(button->get_position(), 1.0 / WHEEL_FACTOR);
            accept_event();
        } else if (index == godot::MOUSE_BUTTON_LEFT || index == godot::MOUSE_BUTTON_MIDDLE) {
            if (button->is_pressed() && button->is_double_click()) {
                fit_to_view();
            }
            dragging_ = button->is_pressed();
            accept_event();
        }
        update_hover(button->get_position());
        return;
    }
    const godot::Ref<godot::InputEventMouseMotion> motion = event;
    if (motion.is_valid()) {
        if (dragging_) {
            auto_fit_ = false;
            offset_ += motion->get_relative();
            queue_redraw();
            accept_event();
        }
        update_hover(motion->get_position());
    }
}

void LayerMapView::_notification(int what) {
    if (what == NOTIFICATION_RESIZED && auto_fit_) {
        apply_fit();
    } else if (what == NOTIFICATION_MOUSE_EXIT && hovered_ != godot::Vector2i(-1, -1)) {
        hovered_ = godot::Vector2i(-1, -1);
        emit_signal("cell_hovered", -1, -1);
    }
}

void LayerMapView::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    using godot::MethodInfo;
    using godot::PropertyInfo;
    using godot::Variant;

    ClassDB::bind_method(D_METHOD("set_image", "image"), &LayerMapView::set_image);
    ClassDB::bind_method(D_METHOD("get_image"), &LayerMapView::get_image);
    ClassDB::bind_method(D_METHOD("clear_image"), &LayerMapView::clear_image);
    ClassDB::bind_method(D_METHOD("fit_to_view"), &LayerMapView::fit_to_view);
    ClassDB::bind_method(D_METHOD("set_zoom", "zoom"), &LayerMapView::set_zoom);
    ClassDB::bind_method(D_METHOD("get_zoom"), &LayerMapView::get_zoom);
    ClassDB::bind_method(D_METHOD("get_shown_rect"), &LayerMapView::get_shown_rect);
    ADD_SIGNAL(MethodInfo("cell_hovered", PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y")));
}

}  // namespace unmapped
