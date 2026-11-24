#include "viewer/layer_map_view.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

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
// P4-04 8.4 as changed by P4-05 12.1 F1: glyph cell size steps and the zoom
// below which small markers are left out.
constexpr double GLYPH_STEP_2 = 0.5;
constexpr double GLYPH_STEP_3 = 10.0;
constexpr double SMALL_HIDDEN_BELOW = 0.5;
// P4-05 12.3: the image is drawn as tiles of TILE x TILE cells.
constexpr int32_t TILE = 128;
// P4-05 5.5, 6.2: a click moves less than this many screen pixels; the
// dimming alpha (0.45 of 255); the frame and place colours.
constexpr float CLICK_SLOP = 4.0F;
constexpr uint8_t DIM_ALPHA = 115;
const godot::Color FRAME_GOLD(255.0F / 255.0F, 215.0F / 255.0F, 0.0F);
const godot::Color FRAME_PLACE(0.0F, 230.0F / 255.0F, 1.0F);
constexpr int32_t EVENT_PLACE_HALF = 12;  // half-size in glyph cells

}  // namespace

void LayerMapView::set_image(const godot::Ref<godot::Image>& image) {
    if (image.is_null()) {
        clear_image();
        return;
    }
    const godot::Vector2i size = image->get_size();
    const bool new_size = size != image_size_ || tiles_.empty();
    image_ = image;
    image_size_ = size;
    upload_tiles(!new_size);
    if (new_size) {
        fit_to_view();
    }
    queue_redraw();
}

void LayerMapView::upload_tiles(bool changed_only) {
    // P4-05 12.3: the image is shown as tiles of TILE x TILE cells, each its
    // own RGBA8 texture. Given the same size again (an image redrawn in
    // place), only the tiles whose bytes differ from those shown are
    // uploaded: a year change touches a few regions, while a whole-image
    // upload (4 MiB at 1024, converted from RGB8 by the engine) cost about
    // 3 ms each time and stalled for 170 ms once the renderer's staging
    // space ran out between frames.
    godot::Ref<godot::Image> source = image_;
    if (source->get_format() != godot::Image::FORMAT_RGB8 || source->has_mipmaps()) {
        source = image_->duplicate();
        source->clear_mipmaps();
        source->convert(godot::Image::FORMAT_RGB8);
        changed_only = false;
    }
    const auto width = static_cast<std::size_t>(image_size_.x);
    const auto height = static_cast<std::size_t>(image_size_.y);
    const uint8_t* bytes = source->ptr();
    if (!changed_only || shown_.size() != width * height * 3U) {
        tiles_.clear();
        shown_.assign(bytes, bytes + width * height * 3U);
        for (int32_t ty = 0; ty < image_size_.y; ty += TILE) {
            for (int32_t tx = 0; tx < image_size_.x; tx += TILE) {
                Tile tile;
                tile.rect = godot::Rect2i(tx, ty, std::min(TILE, image_size_.x - tx), std::min(TILE, image_size_.y - ty));
                tile.texture = godot::ImageTexture::create_from_image(tile_image(tile.rect));
                tiles_.push_back(std::move(tile));
            }
        }
        tiles_uploaded_ += static_cast<int64_t>(tiles_.size());
        return;
    }
    const auto row_at = [width](int32_t x, int32_t y) {
        return (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 3U;
    };
    for (Tile& tile : tiles_) {
        const auto row_bytes = static_cast<std::size_t>(tile.rect.size.x) * 3U;
        const int32_t y_end = tile.rect.position.y + tile.rect.size.y;
        bool differs = false;
        for (int32_t y = tile.rect.position.y; y < y_end && !differs; ++y) {
            const std::size_t at = row_at(tile.rect.position.x, y);
            differs = std::memcmp(bytes + at, shown_.data() + at, row_bytes) != 0;
        }
        if (!differs) {
            continue;
        }
        for (int32_t y = tile.rect.position.y; y < y_end; ++y) {
            const std::size_t at = row_at(tile.rect.position.x, y);
            std::memcpy(shown_.data() + at, bytes + at, row_bytes);
        }
        tile.texture->update(tile_image(tile.rect));
        ++tiles_uploaded_;
    }
}

godot::Ref<godot::Image> LayerMapView::tile_image(const godot::Rect2i& rect) const {
    // The RGBA8 pixels of a tile, from the bytes shown (RGB8).
    const auto width = static_cast<std::size_t>(image_size_.x);
    godot::PackedByteArray data;
    data.resize(static_cast<int64_t>(rect.size.x) * rect.size.y * 4);
    uint8_t* out = data.ptrw();
    for (int32_t y = 0; y < rect.size.y; ++y) {
        const uint8_t* in = shown_.data() + (static_cast<std::size_t>(rect.position.y + y) * width +
                                             static_cast<std::size_t>(rect.position.x)) *
                                                3U;
        for (int32_t x = 0; x < rect.size.x; ++x) {
            out[0] = in[0];
            out[1] = in[1];
            out[2] = in[2];
            out[3] = 255;
            out += 4;
            in += 3;
        }
    }
    return godot::Image::create_from_data(rect.size.x, rect.size.y, false, godot::Image::FORMAT_RGBA8, data);
}

godot::Ref<godot::Image> LayerMapView::get_image() const {
    return image_;
}

void LayerMapView::clear_image() {
    image_.unref();
    tiles_.clear();
    shown_.clear();
    mask_tiles_.clear();
    mask_shown_.clear();
    mask_size_ = godot::Vector2i();
    gold_frames_.clear();
    place_frame_ = -1;
    event_place_ = godot::Vector2i(-1, -1);
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
    if (tiles_.empty()) {
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
    if (tiles_.empty()) {
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
    if (tiles_.empty()) {
        return;
    }
    const auto z = static_cast<float>(zoom_);
    for (const Tile& tile : tiles_) {
        // Tile edges at offset + cell * zoom, so neighbours meet exactly.
        const godot::Vector2 from = offset_ + godot::Vector2(tile.rect.position) * z;
        const godot::Vector2 to = offset_ + godot::Vector2(tile.rect.position + tile.rect.size) * z;
        draw_texture_rect(tile.texture, godot::Rect2(from, to - from), false);
    }
    // P4-05 6.2 item 1: the dimming mask, when it fits the image.
    if (mask_size_ == image_size_) {
        for (const Tile& tile : mask_tiles_) {
            const godot::Vector2 from = offset_ + godot::Vector2(tile.rect.position) * z;
            const godot::Vector2 to = offset_ + godot::Vector2(tile.rect.position + tile.rect.size) * z;
            draw_texture_rect(tile.texture, godot::Rect2(from, to - from), false);
        }
    }
    const int32_t g = glyph_cell_size();
    const int32_t half = g / 2;
    if (!markers_.empty()) {
        const auto start = static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec());
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
    // 6.2 item 3: frames; item 4: the event place.
    for (const int64_t index : gold_frames_) {
        draw_marker_frame(index, FRAME_GOLD);
    }
    if (place_frame_ >= 0) {
        draw_marker_frame(place_frame_, FRAME_PLACE);
    }
    if (event_place_.x >= 0 && event_place_.y >= 0 && event_place_.x < image_size_.x && event_place_.y < image_size_.y) {
        const double cx = std::floor(static_cast<double>(offset_.x) + (static_cast<double>(event_place_.x) + 0.5) * zoom_);
        const double cy = std::floor(static_cast<double>(offset_.y) + (static_cast<double>(event_place_.y) + 0.5) * zoom_);
        const auto r = static_cast<float>(EVENT_PLACE_HALF * g);
        const godot::Vector2 c(static_cast<float>(cx), static_cast<float>(cy));
        // A 2 px line centred on the square's edge: white at half-size r,
        // black just outside it.
        draw_rect(godot::Rect2(c - godot::Vector2(r, r), godot::Vector2(2.0F * r, 2.0F * r)), godot::Color(1, 1, 1),
                  false, 2.0F);
        draw_rect(godot::Rect2(c - godot::Vector2(r + 2.0F, r + 2.0F), godot::Vector2(2.0F * r + 4.0F, 2.0F * r + 4.0F)),
                  godot::Color(0, 0, 0), false, 2.0F);
    }
}

void LayerMapView::draw_marker_frame(int64_t index, godot::Color colour) {
    if (index < 0 || static_cast<std::size_t>(index) >= markers_.size()) {
        return;
    }
    const Marker& marker = markers_[static_cast<std::size_t>(index)];
    if (!marker_drawn(marker)) {
        return;
    }
    const int32_t g = glyph_cell_size();
    const int32_t half = g / 2;
    const godot::Vector2i c = marker_centre(marker);
    const int32_t r = marker.cells->radius;
    // The glyph covers [c - r g - half, c - r g - half + (2r + 1) g); the
    // frame is a 2 px line just outside it.
    const auto x0 = static_cast<float>(c.x - r * g - half);
    const auto y0 = static_cast<float>(c.y - r * g - half);
    const auto side = static_cast<float>((2 * r + 1) * g);
    draw_rect(godot::Rect2(x0 - 1.0F, y0 - 1.0F, side + 2.0F, side + 2.0F), colour, false, 2.0F);
}

// --- Highlight (P4-05 6.2) -----------------------------------------------------

void LayerMapView::set_mask(const std::vector<uint8_t>& mask, uint32_t width, uint32_t height) {
    const int64_t start = static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec());
    if (mask.empty() || width == 0 || height == 0 ||
        mask.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        mask_tiles_.clear();
        mask_shown_.clear();
        mask_size_ = godot::Vector2i();
        queue_redraw();
        last_mask_nsec_ = (static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec()) - start) * 1000;
        return;
    }
    const godot::Vector2i size(static_cast<int32_t>(width), static_cast<int32_t>(height));
    if (size != mask_size_ || mask_tiles_.empty()) {
        mask_size_ = size;
        mask_shown_ = mask;
        mask_tiles_.clear();
        for (int32_t ty = 0; ty < size.y; ty += TILE) {
            for (int32_t tx = 0; tx < size.x; tx += TILE) {
                Tile tile;
                tile.rect = godot::Rect2i(tx, ty, std::min(TILE, size.x - tx), std::min(TILE, size.y - ty));
                tile.texture = godot::ImageTexture::create_from_image(mask_tile_image(tile.rect));
                mask_tiles_.push_back(std::move(tile));
            }
        }
        mask_tiles_uploaded_ += static_cast<int64_t>(mask_tiles_.size());
    } else {
        for (Tile& tile : mask_tiles_) {
            bool differs = false;
            const int32_t y_end = tile.rect.position.y + tile.rect.size.y;
            const auto row_bytes = static_cast<std::size_t>(tile.rect.size.x);
            for (int32_t y = tile.rect.position.y; y < y_end && !differs; ++y) {
                const std::size_t at = static_cast<std::size_t>(y) * width + static_cast<std::size_t>(tile.rect.position.x);
                differs = std::memcmp(mask.data() + at, mask_shown_.data() + at, row_bytes) != 0;
            }
            if (!differs) {
                continue;
            }
            for (int32_t y = tile.rect.position.y; y < y_end; ++y) {
                const std::size_t at = static_cast<std::size_t>(y) * width + static_cast<std::size_t>(tile.rect.position.x);
                std::memcpy(mask_shown_.data() + at, mask.data() + at, row_bytes);
            }
            tile.texture->update(mask_tile_image(tile.rect));
            ++mask_tiles_uploaded_;
        }
    }
    queue_redraw();
    last_mask_nsec_ = (static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec()) - start) * 1000;
}

godot::Ref<godot::Image> LayerMapView::mask_tile_image(const godot::Rect2i& rect) const {
    const auto width = static_cast<std::size_t>(mask_size_.x);
    godot::PackedByteArray data;
    data.resize(static_cast<int64_t>(rect.size.x) * rect.size.y * 2);
    uint8_t* out = data.ptrw();
    for (int32_t y = 0; y < rect.size.y; ++y) {
        const uint8_t* in = mask_shown_.data() + static_cast<std::size_t>(rect.position.y + y) * width +
                            static_cast<std::size_t>(rect.position.x);
        for (int32_t x = 0; x < rect.size.x; ++x) {
            out[0] = 0;
            out[1] = in[x] != 0 ? DIM_ALPHA : 0;
            out += 2;
        }
    }
    return godot::Image::create_from_data(rect.size.x, rect.size.y, false, godot::Image::FORMAT_LA8, data);
}

void LayerMapView::set_frames(const std::vector<int64_t>& gold, int64_t place) {
    if (gold == gold_frames_ && place == place_frame_) {
        return;
    }
    gold_frames_ = gold;
    place_frame_ = place;
    queue_redraw();
}

void LayerMapView::set_event_place(godot::Vector2i cell) {
    if (cell == event_place_) {
        return;
    }
    event_place_ = cell;
    queue_redraw();
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
        // P4-05 12.1 F6: small entries and passes are hit one glyph cell
        // beyond their glyph too.
        marker.margin = marker.small || marker.glyph == "pass";
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

int32_t LayerMapView::glyph_cell_size_at(double zoom) noexcept {
    return zoom < GLYPH_STEP_2 ? 1 : zoom < GLYPH_STEP_3 ? 2 : 3;
}

int32_t LayerMapView::glyph_cell_size() const {
    return glyph_cell_size_at(zoom_);
}

bool LayerMapView::marker_drawn(const Marker& marker) const {
    return !tiles_.empty() && marker.cells != nullptr && !(marker.small && zoom_ < SMALL_HIDDEN_BELOW);
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
    if (markers_.empty() || tiles_.empty()) {
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
        const int64_t reach = marker.margin ? 1 : 0;  // F6: Chebyshev margin in glyph cells
        if (dx < -r - reach || dx > r + reach || dy < -r - reach || dy > r + reach) {
            continue;
        }
        const auto side = static_cast<std::size_t>(2 * r + 1);
        const auto cell_set = [&](int64_t cx, int64_t cy) {
            return cx >= -r && cx <= r && cy >= -r && cy <= r &&
                   marker.cells->cells[static_cast<std::size_t>(cy + r) * side + static_cast<std::size_t>(cx + r)] != 0;
        };
        bool hit = cell_set(dx, dy);
        for (int64_t ny = dy - reach; ny <= dy + reach && !hit; ++ny) {
            for (int64_t nx = dx - reach; nx <= dx + reach && !hit; ++nx) {
                hit = cell_set(nx, ny);
            }
        }
        if (hit) {
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
            // P4-05 5.5: a left press and release without a drag is a click.
            if (index == godot::MOUSE_BUTTON_LEFT) {
                if (button->is_pressed()) {
                    pressing_ = true;
                    press_cancelled_ = button->is_double_click();
                    press_position_ = button->get_position();
                } else if (pressing_) {
                    pressing_ = false;
                    const bool moved = (button->get_position() - press_position_).length() >= CLICK_SLOP;
                    if (!press_cancelled_ && !moved) {
                        update_hover(button->get_position());
                        const godot::Vector2i cell = cell_at(button->get_position());
                        emit_signal("cell_clicked", cell.x, cell.y, marker_at(button->get_position()));
                        return;
                    }
                }
            }
        }
        update_hover(button->get_position());
        return;
    }
    const godot::Ref<godot::InputEventMouseMotion> motion = event;
    if (motion.is_valid()) {
        if (pressing_ && (motion->get_position() - press_position_).length() >= CLICK_SLOP) {
            press_cancelled_ = true;
        }
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
    ADD_SIGNAL(MethodInfo("cell_clicked", PropertyInfo(Variant::INT, "x"), PropertyInfo(Variant::INT, "y"),
                          PropertyInfo(Variant::INT, "marker_index")));
}

}  // namespace unmapped
