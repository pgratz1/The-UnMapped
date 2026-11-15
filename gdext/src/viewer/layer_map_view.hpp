#pragma once

// LayerMapView: a Control that shows one layer image with pan and zoom.
//
// Presentation only. It is given an Image (from ViewerModel) and draws it
// one texel per cell with nearest filtering. A new image is fitted to the
// view and kept fitted as the view resizes, until the user zooms (mouse
// wheel, about the cursor) or pans (drag with the left or middle button);
// double click fits again. Zoom and offset are floats on the Godot side only.
// Emits cell_hovered(x, y) with the cell under the mouse, (-1, -1) outside.

#include <cstdint>

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/rect2.hpp>
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

protected:
    static void _bind_methods();

private:
    [[nodiscard]] godot::Vector2i cell_at(godot::Vector2 position) const;
    void zoom_about(godot::Vector2 anchor, double factor);
    void update_hover(godot::Vector2 position);
    void apply_fit();

    godot::Ref<godot::Image> image_;
    godot::Ref<godot::ImageTexture> texture_;
    godot::Vector2i image_size_;
    godot::Vector2 offset_;
    double zoom_ = 1.0;
    bool dragging_ = false;
    bool auto_fit_ = true;
    godot::Vector2i hovered_{-1, -1};
};

}  // namespace unmapped
