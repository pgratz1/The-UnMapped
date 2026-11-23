#pragma once

// YearMarks: the marks under the year slider (spec P4-03 4.5).
//
// Presentation only. Given the model's get_year_marks() and E, it draws one
// vertical line per mark at x = (width - 1) year / E: an awakening and the
// present the full height, a phase start half the height. Its tooltip at the
// mouse is the text of the nearest mark. Floats are presentation only.

#include <cstdint>

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace unmapped {

class YearMarks : public godot::Control {
    GDCLASS(YearMarks, godot::Control)

public:
    YearMarks() = default;

    void _draw() override;
    [[nodiscard]] godot::String _get_tooltip(const godot::Vector2& at_position) const override;

    // --- Bound to Godot ------------------------------------------------------
    // marks: Array of Dictionary {year, kind, label}; end_year E (> 0 to
    // draw anything). An empty array clears the marks.
    void set_marks(const godot::Array& marks, int64_t end_year);
    [[nodiscard]] godot::Array get_marks() const;

protected:
    static void _bind_methods();

private:
    [[nodiscard]] float mark_x(int64_t year) const;

    godot::Array marks_;
    int64_t end_year_ = 0;
};

}  // namespace unmapped
