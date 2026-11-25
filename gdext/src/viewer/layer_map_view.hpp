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
//
// The browser's highlight (spec P4-05 6.2), drawn after the image in this
// order: the dimming mask (black at alpha 0.45 over the cells it marks, as
// tiles like the image, a new mask uploading only the tiles that changed),
// the markers, the frames (a 2 px square in (255, 215, 0) around the glyph
// of each framed marker, one in (0, 230, 255) around the place marker), and
// the event place (two concentric 2 px squares, white inside, black
// outside, of half-size 12 g). Nothing is drawn into the image.
// A left press and release with less than CLICK_SLOP screen pixels of
// movement between them emits cell_clicked(x, y, marker_index) (marker -1
// when none); a drag still pans and a double click still fits (P4-05 5.5).

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

    // --- Highlight (P4-05 6.2), C++ only --------------------------------------
    // The dimming mask: one byte per cell, non-zero = dimmed; empty for none.
    void set_mask(const std::vector<uint8_t>& mask, uint32_t width, uint32_t height);
    // Frames around markers (indexes into the list of set_markers): gold
    // ones and the one place marker (-1 none).
    void set_frames(const std::vector<int64_t>& gold, int64_t place);
    // The selected event's place cell, (-1, -1) for none.
    void set_event_place(godot::Vector2i cell);
    [[nodiscard]] bool has_mask() const noexcept { return !mask_tiles_.empty(); }
    [[nodiscard]] godot::Vector2i get_event_place() const noexcept { return event_place_; }
    [[nodiscard]] const std::vector<int64_t>& gold_frames() const noexcept { return gold_frames_; }
    [[nodiscard]] int64_t place_frame() const noexcept { return place_frame_; }
    // Mask tile textures created or updated, and the time of the last
    // set_mask (nanoseconds).
    [[nodiscard]] int64_t mask_tiles_uploaded() const noexcept { return mask_tiles_uploaded_; }

    // --- Flashes (P4-06 7), C++ only -------------------------------------------
    // The model's get_flashes(): {x, y, colour, age, life}. Drawn last, above
    // the markers and the highlight: a ring centred on the cell's screen
    // centre, radius 6g + 14g age / life, width 2g, in the flash colour with
    // alpha 1 - age / life. Redraws when the list changes.
    void set_flashes(const godot::Array& flashes);
    [[nodiscard]] std::size_t flash_count() const noexcept { return flashes_.size(); }
    [[nodiscard]] int64_t last_mask_nsec() const noexcept { return last_mask_nsec_; }

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
    void draw_marker_frame(int64_t index, godot::Color colour);

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
        // P5-04 11.3: the glyph's centre is this many glyph cells from the
        // cell's (a mine: 7 right, 7 up, so its town does not hide it).
        int32_t offset_x = 0, offset_y = 0;
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
    // The dimming mask as tiles (the same rectangles as the image's), LA8,
    // and the mask bytes they show.
    [[nodiscard]] godot::Ref<godot::Image> mask_tile_image(const godot::Rect2i& rect) const;
    std::vector<Tile> mask_tiles_;
    std::vector<uint8_t> mask_shown_;
    godot::Vector2i mask_size_;
    int64_t mask_tiles_uploaded_ = 0;
    int64_t last_mask_nsec_ = 0;
    struct FlashRing {
        int64_t x = 0, y = 0;
        int64_t colour = 0xFFFFFF;
        int64_t age = 0, life = 1;
    };
    std::vector<FlashRing> flashes_;
    std::vector<int64_t> gold_frames_;
    int64_t place_frame_ = -1;
    godot::Vector2i event_place_{-1, -1};
    // A left press not yet released: where, and whether it moved too far or
    // was a double click (no click then).
    bool pressing_ = false;
    bool press_cancelled_ = false;
    godot::Vector2 press_position_;
    int64_t tiles_uploaded_ = 0;
    godot::Vector2i image_size_;
    godot::Vector2 offset_;
    double zoom_ = 1.0;
    bool dragging_ = false;
    bool auto_fit_ = true;
    godot::Vector2i hovered_{-1, -1};
};

}  // namespace unmapped
