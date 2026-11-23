#include "viewer/layer_map_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>

namespace unmapped {

namespace {

constexpr double MIN_ZOOM = 1.0 / 64.0;
constexpr double MAX_ZOOM = 64.0;
constexpr double WHEEL_FACTOR = 1.25;
// P4-04 8.4: glyph cell size steps and the zoom below which small markers
// are left out.
constexpr double GLYPH_STEP_2 = 4.0;
constexpr double GLYPH_STEP_3 = 10.0;
constexpr double SMALL_HIDDEN_BELOW = 0.5;

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

void LayerMapView::centre_on(godot::Vector2 cell, double zoom) {
    auto_fit_ = false;
    zoom_ = std::clamp(zoom, MIN_ZOOM, MAX_ZOOM);
    offset_ = get_size() * 0.5F - (cell + godot::Vector2(0.5F, 0.5F)) * static_cast<float>(zoom_);
    queue_redraw();
    if (mouse_inside_) {
        update_hover(mouse_position_);
    }
}

godot::Vector2 LayerMapView::cell_centre_position(int64_t x, int64_t y) const {
    return {static_cast<float>(static_cast<double>(offset_.x) + (static_cast<double>(x) + 0.5) * zoom_),
            static_cast<float>(static_cast<double>(offset_.y) + (static_cast<double>(y) + 0.5) * zoom_)};
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
    mouse_position_ = position;
    mouse_inside_ = true;
    const godot::Vector2i cell = cell_at(position);
    if (cell != hovered_) {
        hovered_ = cell;
        emit_signal("cell_hovered", cell.x, cell.y);
    }
    update_marker_hover();
}

void LayerMapView::update_marker_hover() {
    const int64_t hit = mouse_inside_ ? marker_at(mouse_position_) : -1;
    if (hit != hovered_marker_) {
        hovered_marker_ = hit;
        emit_signal("marker_hovered", hit);
    }
}

void LayerMapView::_draw() {
    if (texture_.is_null()) {
        return;
    }
    draw_texture_rect(texture_, get_shown_rect(), false);
    if (markers_.empty()) {
        return;
    }
    const auto start = static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec());
    const int32_t g = glyph_cell_size();
    const int32_t half = g / 2;
    for (const Marker& marker : markers_) {
        if (!marker_drawn(marker)) {
            continue;
        }
        const godot::Ref<godot::ImageTexture> texture = glyph_texture(marker, g);
        if (texture.is_null()) {
            continue;
        }
        const godot::Vector2i c = marker_centre(marker);
        const int32_t r = marker.cells->radius;
        draw_texture(texture, godot::Vector2(static_cast<float>(c.x - r * g - half),
                                             static_cast<float>(c.y - r * g - half)));
    }
    last_marker_draw_usec_ = static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec()) - start;
    ++marker_draws_;
}

// --- Markers (P4-04 8.4, 8.5) ------------------------------------------------

void LayerMapView::set_markers(const godot::Array& markers, const godot::Dictionary& glyphs) {
    const godot::Array names = glyphs.keys();
    for (int64_t i = 0; i < names.size(); ++i) {
        const godot::String name = names[i];
        const godot::CharString utf8 = name.utf8();
        const std::string key(utf8.get_data(), static_cast<std::size_t>(utf8.length()));
        if (glyphs_.count(key) != 0) {
            continue;
        }
        const godot::Dictionary glyph = glyphs[name];
        const godot::PackedByteArray cells = glyph.get("cells", godot::PackedByteArray());
        const int64_t radius = glyph.get("radius", int64_t{-1});
        const int64_t side = 2 * radius + 1;
        if (radius < 0 || radius > 64 || cells.size() != side * side) {
            continue;
        }
        GlyphCells entry;
        entry.radius = static_cast<int32_t>(radius);
        entry.cells.assign(cells.ptr(), cells.ptr() + cells.size());
        glyphs_.emplace(key, std::move(entry));
    }
    markers_.clear();
    markers_.reserve(static_cast<std::size_t>(markers.size()));
    for (int64_t i = 0; i < markers.size(); ++i) {
        const godot::Dictionary entry = markers[i];
        Marker marker;
        marker.x = entry.get("x", int64_t{0});
        marker.y = entry.get("y", int64_t{0});
        const godot::CharString glyph = godot::String(entry.get("glyph", godot::String())).utf8();
        marker.glyph.assign(glyph.get_data(), static_cast<std::size_t>(glyph.length()));
        marker.fill = entry.get("fill", int64_t{-1});
        marker.small = entry.get("small", false);
        const auto it = glyphs_.find(marker.glyph);
        marker.cells = it != glyphs_.end() ? &it->second : nullptr;
        markers_.push_back(std::move(marker));
    }
    queue_redraw();
    update_marker_hover();
}

void LayerMapView::clear_markers() {
    markers_.clear();
    textures_.clear();
    glyphs_.clear();
    queue_redraw();
    update_marker_hover();
}

int32_t LayerMapView::glyph_cell_size() const {
    return zoom_ < GLYPH_STEP_2 ? 1 : zoom_ < GLYPH_STEP_3 ? 2 : 3;
}

bool LayerMapView::marker_drawn(const Marker& marker) const {
    return texture_.is_valid() && marker.cells != nullptr && !(marker.small && zoom_ < SMALL_HIDDEN_BELOW);
}

godot::Vector2i LayerMapView::marker_centre(const Marker& marker) const {
    const double cx = std::floor(static_cast<double>(offset_.x) + (static_cast<double>(marker.x) + 0.5) * zoom_);
    const double cy = std::floor(static_cast<double>(offset_.y) + (static_cast<double>(marker.y) + 0.5) * zoom_);
    return {static_cast<int32_t>(cx), static_cast<int32_t>(cy)};
}

godot::Ref<godot::ImageTexture> LayerMapView::glyph_texture(const Marker& marker, int32_t g) {
    const auto key = std::make_tuple(marker.glyph, marker.fill, g);
    const auto found = textures_.find(key);
    if (found != textures_.end()) {
        return found->second;
    }
    const GlyphCells& glyph = *marker.cells;
    const int32_t side = 2 * glyph.radius + 1;
    const int32_t size = side * g;
    godot::PackedByteArray data;
    data.resize(static_cast<int64_t>(size) * size * 4);
    uint8_t* out = data.ptrw();
    std::fill(out, out + data.size(), uint8_t{0});
    const auto channel = [](int64_t rgb, int shift) { return static_cast<uint8_t>((rgb >> shift) & 0xFF); };
    for (int32_t row = 0; row < side; ++row) {
        for (int32_t col = 0; col < side; ++col) {
            const uint8_t cls = glyph.cells[static_cast<std::size_t>(row) * static_cast<std::size_t>(side) +
                                            static_cast<std::size_t>(col)];
            std::array<uint8_t, 4> rgba{0, 0, 0, 0};
            if (cls == 1) {
                rgba = {240, 240, 240, 255};  // MARKER_HALO
            } else if (cls == 2) {
                const int64_t fill = marker.fill < 0 ? 0xA0A0A0 : marker.fill;
                rgba = {channel(fill, 16), channel(fill, 8), channel(fill, 0), 255};
            } else if (cls == 3) {
                rgba = {0, 0, 0, 255};  // MARKER_INK
            } else {
                continue;  // 0 none, 4 interior: the map shows
            }
            for (int32_t py = row * g; py < (row + 1) * g; ++py) {
                for (int32_t px = col * g; px < (col + 1) * g; ++px) {
                    uint8_t* p = out + (static_cast<int64_t>(py) * size + px) * 4;
                    std::copy(rgba.begin(), rgba.end(), p);
                }
            }
        }
    }
    const godot::Ref<godot::Image> image =
        godot::Image::create_from_data(size, size, false, godot::Image::FORMAT_RGBA8, data);
    godot::Ref<godot::ImageTexture> texture = godot::ImageTexture::create_from_image(image);
    textures_.emplace(key, texture);
    return texture;
}

godot::Vector2i LayerMapView::get_marker_screen_position(int64_t index) const {
    if (index < 0 || static_cast<std::size_t>(index) >= markers_.size() ||
        !marker_drawn(markers_[static_cast<std::size_t>(index)])) {
        return {-1, -1};
    }
    return marker_centre(markers_[static_cast<std::size_t>(index)]);
}

int64_t LayerMapView::marker_at(godot::Vector2 position) const {
    if (markers_.empty() || texture_.is_null()) {
        return -1;
    }
    const int32_t g = glyph_cell_size();
    const int32_t half = g / 2;
    const auto px = static_cast<int64_t>(std::floor(position.x));
    const auto py = static_cast<int64_t>(std::floor(position.y));
    // floor division for negative offsets
    const auto cell_of = [g](int64_t d) {
        return d >= 0 ? d / g : -((-d + g - 1) / g);
    };
    for (std::size_t i = markers_.size(); i-- > 0;) {
        const Marker& marker = markers_[i];
        if (!marker_drawn(marker)) {
            continue;
        }
        const godot::Vector2i c = marker_centre(marker);
        const int64_t dx = cell_of(px - c.x + half);
        const int64_t dy = cell_of(py - c.y + half);
        const int64_t r = marker.cells->radius;
        if (dx < -r || dx > r || dy < -r || dy > r) {
            continue;
        }
        const auto side = static_cast<std::size_t>(2 * r + 1);
        const uint8_t cls = marker.cells->cells[static_cast<std::size_t>(dy + r) * side + static_cast<std::size_t>(dx + r)];
        if (cls != 0) {
            return static_cast<int64_t>(i);
        }
    }
    return -1;
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
    } else if (what == NOTIFICATION_MOUSE_EXIT) {
        mouse_inside_ = false;
        if (hovered_ != godot::Vector2i(-1, -1)) {
            hovered_ = godot::Vector2i(-1, -1);
            emit_signal("cell_hovered", -1, -1);
        }
        update_marker_hover();
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
    ClassDB::bind_method(D_METHOD("set_markers", "markers", "glyphs"), &LayerMapView::set_markers);
    ClassDB::bind_method(D_METHOD("clear_markers"), &LayerMapView::clear_markers);
    ClassDB::bind_method(D_METHOD("get_marker_screen_position", "index"), &LayerMapView::get_marker_screen_position);
    ClassDB::bind_method(D_METHOD("get_hovered_marker"), &LayerMapView::get_hovered_marker);
    ADD_SIGNAL(MethodInfo("cell_hovered", PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y")));
    ADD_SIGNAL(MethodInfo("marker_hovered", PropertyInfo(Variant::INT, "index")));
}

}  // namespace unmapped
