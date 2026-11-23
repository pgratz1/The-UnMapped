#pragma once

// LayerMapView: a Control that shows one layer image with pan and zoom.
//
// Presentation only. It is given an Image (from ViewerModel) and draws it
// one texel per cell with nearest filtering, as tiles of 128 x 128 cells,
// each its own texture; the same image redrawn in place uploads only the
// tiles whose pixels changed. A new image is fitted to the
// view and kept fitted as the view resizes, until the user zooms (mouse
// wheel, about the cursor) or pans (drag with the left or middle button);
// double click fits again. Zoom and offset are floats on the Godot side only.
// Emits cell_hovered(x, y) with the cell under the mouse, (-1, -1) outside.
//
// Markers (spec P4-04 8.4, 8.5): set_markers hands it the model's marker
// list (get_markers()) and the glyphs it names; they are drawn after the
// image, in list order, at screen size: each glyph cell is a g x g square
// of screen pixels (g = 1 below zoom 0.5, 2 below zoom 10, else 3; P4-05
// 12.1 F1), centred on the marker's cell; below zoom 0.5 the entries marked
// small are neither drawn nor hit. Each distinct (glyph, fill, g) is one
// cached ImageTexture (dropped by clear_markers). Hover hit-tests the drawn
// glyphs in reverse draw order (a glyph cell of any class but 0; for small
// entries and passes also a glyph cell within one cell of one, F6) and emits
// marker_hovered(index), -1 when none, after cell_hovered.

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

namespace unmapped {

class LayerMapView : public godot::Control {
    GDCLASS(LayerMapView, godot::Control)

public:
    LayerMapView() = default;

    void _draw() override;
    void _gui_input(const godot::Ref<godot::InputEvent>& event) override;
    void _notification(int what);

    // --- Bound to Godot ------------------------------------------------------
    // Shows image. A new size fits the view; the same size keeps zoom and
    // pan, so switching layers of one world keeps the place.
    void set_image(const godot::Ref<godot::Image>& image);
    [[nodiscard]] godot::Ref<godot::Image> get_image() const;
    void clear_image();
    void fit_to_view();
    void set_zoom(double zoom);
    [[nodiscard]] double get_zoom() const;
    // Where the image is drawn, in the view's coordinates; empty without one.
    [[nodiscard]] godot::Rect2 get_shown_rect() const;

    // Markers: an Array of Dictionary {x, y, glyph, fill, small} (the
    // model's get_markers()) and a Dictionary glyph name -> {radius, cells}.
    void set_markers(const godot::Array& markers, const godot::Dictionary& glyphs);
    // No markers, and the texture cache dropped (the world closed).
    void clear_markers();
    // The centre (cx, cy) of entry index on screen, (-1, -1) when not drawn.
    [[nodiscard]] godot::Vector2i get_marker_screen_position(int64_t index) const;
    [[nodiscard]] int64_t get_hovered_marker() const { return hovered_marker_; }
    // The marker hit at a position of the view, -1 when none.
    [[nodiscard]] int64_t marker_at(godot::Vector2 position) const;

    // Shows cell (x, y) at the centre of the view at the given zoom (for
    // screenshots); stops fitting.
    void centre_on(godot::Vector2 cell, double zoom);
    // The screen position of the centre of cell (x, y) in the view.
    [[nodiscard]] godot::Vector2 cell_centre_position(int64_t x, int64_t y) const;

    // --- C++ only (--bench) ----------------------------------------------------
    // Time of the marker part of the last _draw, microseconds (-1: never
    // drawn), and the number of _draw calls that drew markers.
    [[nodiscard]] int64_t last_marker_draw_usec() const noexcept { return last_marker_draw_usec_; }
    [[nodiscard]] int64_t marker_draws() const noexcept { return marker_draws_; }
    [[nodiscard]] std::size_t marker_texture_count() const noexcept { return textures_.size(); }
    // Tile textures created or updated since the view was made (P4-05 12.3).
    [[nodiscard]] int64_t tiles_uploaded() const noexcept { return tiles_uploaded_; }
    [[nodiscard]] std::size_t tile_count() const noexcept { return tiles_.size(); }
    // The glyph cell size g at a zoom (P4-04 8.4, P4-05 12.1 F1), and at the
    // current zoom.
    [[nodiscard]] static int32_t glyph_cell_size_at(double zoom) noexcept;
    [[nodiscard]] int32_t glyph_cell_size() const;

protected:
    static void _bind_methods();

private:
    [[nodiscard]] godot::Vector2i cell_at(godot::Vector2 position) const;
    void zoom_about(godot::Vector2 anchor, double factor);
    void update_hover(godot::Vector2 position);
    void apply_fit();

    struct GlyphCells {
        int32_t radius = 0;
        std::vector<uint8_t> cells;
    };
    struct Marker {
        int64_t x = 0, y = 0;
        std::string glyph;
        int64_t fill = -1;
        bool small = false;
        bool margin = false;  // hit one glyph cell beyond the glyph (F6)
        const GlyphCells* cells = nullptr;  // into glyphs_; null: glyph unknown
    };
    [[nodiscard]] bool marker_drawn(const Marker& marker) const;
    [[nodiscard]] godot::Vector2i marker_centre(const Marker& marker) const;
    godot::Ref<godot::ImageTexture> glyph_texture(const Marker& marker, int32_t g);
    void update_marker_hover();

    std::map<std::string, GlyphCells> glyphs_;
    std::vector<Marker> markers_;
    std::map<std::tuple<std::string, int64_t, int32_t>, godot::Ref<godot::ImageTexture>> textures_;
    int64_t hovered_marker_ = -1;
    godot::Vector2 mouse_position_;
    bool mouse_inside_ = false;
    int64_t last_marker_draw_usec_ = -1;
    int64_t marker_draws_ = 0;

    godot::Ref<godot::Image> image_;
    // The image as tiles (P4-05 12.3): each a TILE x TILE cell rectangle
    // (smaller at the right and bottom edges) with its RGBA8 texture, and
    // the RGB8 bytes the tiles show, to find the tiles a redraw changed.
    struct Tile {
        godot::Rect2i rect;
        godot::Ref<godot::ImageTexture> texture;
    };
    void upload_tiles(bool changed_only);
    [[nodiscard]] godot::Ref<godot::Image> tile_image(const godot::Rect2i& rect) const;
    std::vector<Tile> tiles_;
    std::vector<uint8_t> shown_;
    int64_t tiles_uploaded_ = 0;
    godot::Vector2i image_size_;
    godot::Vector2 offset_;
    double zoom_ = 1.0;
    bool dragging_ = false;
    bool auto_fit_ = true;
    godot::Vector2i hovered_{-1, -1};
};

}  // namespace unmapped
