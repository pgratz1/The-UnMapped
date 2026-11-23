#include "viewer/year_marks.hpp"

#include <cmath>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace unmapped {

void YearMarks::set_marks(const godot::Array& marks, int64_t end_year) {
    marks_ = marks.duplicate();
    end_year_ = end_year;
    queue_redraw();
}

godot::Array YearMarks::get_marks() const {
    return marks_;
}

float YearMarks::mark_x(int64_t year) const {
    const double width = static_cast<double>(get_size().x);
    return static_cast<float>((width - 1.0) * static_cast<double>(year) / static_cast<double>(end_year_));
}

void YearMarks::_draw() {
    if (end_year_ <= 0) {
        return;
    }
    const float height = get_size().y;
    const godot::Color full(0.85F, 0.85F, 0.85F);
    const godot::Color half(0.6F, 0.6F, 0.6F);
    for (int64_t i = 0; i < marks_.size(); ++i) {
        const godot::Dictionary mark = marks_[i];
        const auto year = static_cast<int64_t>(mark.get("year", 0));
        const bool is_phase = mark.get("kind", "") == godot::Variant("phase");
        const float x = std::floor(mark_x(year)) + 0.5F;
        draw_line(godot::Vector2(x, 0.0F), godot::Vector2(x, is_phase ? height * 0.5F : height), is_phase ? half : full,
                  1.0F);
    }
}

godot::String YearMarks::_get_tooltip(const godot::Vector2& at_position) const {
    if (end_year_ <= 0 || marks_.is_empty()) {
        return {};
    }
    float best = 0.0F;
    godot::String label;
    for (int64_t i = 0; i < marks_.size(); ++i) {
        const godot::Dictionary mark = marks_[i];
        const float distance = std::abs(mark_x(static_cast<int64_t>(mark.get("year", 0))) - at_position.x);
        if (label.is_empty() || distance < best) {
            best = distance;
            label = godot::String(mark.get("label", ""));
        }
    }
    return label;
}

void YearMarks::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("set_marks", "marks", "end_year"), &YearMarks::set_marks);
    ClassDB::bind_method(D_METHOD("get_marks"), &YearMarks::get_marks);
}

}  // namespace unmapped
