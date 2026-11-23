#pragma once

// ViewerScreen: the flat-screen view of the world viewer.
//
// The root Control of game/viewer/viewer.tscn. The scene holds the layout;
// this class finds its parts by unique name (%OpenButton, %OpenDialog,
// %FileLabel, %LayerList, %LegendTitle, %LegendList, %MetaTree, %MapView,
// %CellLabel, %StatusLabel), wires their signals to a ViewerModel, and
// mirrors the model into them. No logic lives in the scene. The layer list
// shows the model's views grouped under headings (Composites first, then
// Terrain, Water, Climate, Biomes, Resources, Regions, Other), the legend
// the selected view's colours, and %CellLabel the cell under the mouse.
//
// The year bar (spec P4-03 4): %YearBack25, %YearBack1, %YearPrevEvent,
// %YearPrevChange, %YearSlider over %YearMarks, %YearNextChange,
// %YearNextEvent, %YearForward1, %YearForward25, %YearField, then %YearLabel
// and %YearCounts. Every control calls the model; the screen mirrors the
// model's year on year_changed. Keys (when no text field has them): Left /
// Right one year, with Shift 25, Page Up / Page Down the previous / next
// event, [ and ] the previous / next change of holder, Home year 0, End the
// present. Without history the year bar is disabled.
//
// Command line (arguments after `--`):
//   <world.db>                 open this world at start
//   --view <name>              then show this layer or composite ("biome",
//                              "map", "biomes", "regions") instead of the
//                              first entry of the list
//   --year <Y>                 after opening, show year Y (a world with
//                              history; P4-03 4.7)
//   --hover <x> <y>            show the cell description of (x, y) as if the
//                              mouse were there (for screenshots)
//   --bench                    time opening the world and every year change
//                              0 .. E and back, print, quit (P4-03 11.2)
//   --check <reference dir>    headless check (viewer_check.hpp), then quit
//   --expect-layers <n>        with --check: the number of layers expected
//   --expect-composites <n>    with --check: the number of composites expected
//   --expect-history <s>       with --check: none or loaded (P4-03 10)
//   --expect-end-year <E>      with --check: the history's end year
//   --expect-years <a,b,...>   with --check: the years compared with
//                              <dir>/territory-<Y>.png
//   --history-report <file>    with --check: `report --history --level full`
//   --screenshot <file.png>    save the window's contents after one second,
//                              then quit (needs a display; for review images)

#include <cstdint>
#include <vector>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/spin_box.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/variant/string.hpp>

#include "viewer/layer_map_view.hpp"
#include "viewer/viewer_model.hpp"
#include "viewer/year_marks.hpp"

namespace unmapped {

class ViewerScreen : public godot::Control {
    GDCLASS(ViewerScreen, godot::Control)

public:
    ViewerScreen() = default;

    void _ready() override;
    void _unhandled_key_input(const godot::Ref<godot::InputEvent>& event) override;

    // --- Bound to Godot ------------------------------------------------------
    [[nodiscard]] godot::Ref<ViewerModel> get_model() const;
    // Opens a world as if chosen in the file dialog.
    bool open_world(const godot::String& path);
    // Selects a view (layer or composite; a model view index) as if clicked
    // in the layer list.
    void choose_view(int64_t index);
    // Shows the description of cell (x, y) as if the mouse were over it;
    // (-1, -1) as if it left the map.
    void hover_cell(int64_t x, int64_t y);

    // --- C++ only (used by the headless check) ---------------------------------
    [[nodiscard]] LayerMapView* map_view() const noexcept { return map_view_; }
    [[nodiscard]] godot::ItemList* layer_list() const noexcept { return layer_list_; }
    [[nodiscard]] godot::Tree* meta_tree() const noexcept { return meta_tree_; }
    [[nodiscard]] godot::Label* status_label() const noexcept { return status_label_; }
    [[nodiscard]] godot::Label* cell_label() const noexcept { return cell_label_; }
    [[nodiscard]] godot::Label* legend_title() const noexcept { return legend_title_; }
    [[nodiscard]] godot::ItemList* legend_list() const noexcept { return legend_list_; }
    [[nodiscard]] godot::Label* file_label() const noexcept { return file_label_; }
    [[nodiscard]] godot::HSlider* year_slider() const noexcept { return year_slider_; }
    [[nodiscard]] godot::SpinBox* year_field() const noexcept { return year_field_; }
    [[nodiscard]] godot::Label* year_label() const noexcept { return year_label_; }
    [[nodiscard]] godot::Label* year_counts() const noexcept { return year_counts_; }
    [[nodiscard]] YearMarks* year_marks() const noexcept { return year_marks_; }
    // The eight buttons of the year row, in their order in the row.
    [[nodiscard]] std::vector<godot::Button*> year_buttons() const;
    // The texts of %YearLabel and %YearCounts for the model's year (4.4).
    [[nodiscard]] godot::String year_label_text() const;
    [[nodiscard]] godot::String year_counts_text() const;
    // How long the last open_world took (the model's open and the first view
    // shown), in microseconds; -1 before any.
    [[nodiscard]] int64_t last_open_usec() const noexcept { return last_open_usec_; }
    // The list item showing a view, or -1; the view an item shows, or -1
    // (a group heading).
    [[nodiscard]] int64_t item_of_view(int64_t view) const;
    [[nodiscard]] int64_t view_of_item(int64_t item) const;

protected:
    static void _bind_methods();

private:
    template <class T>
    T* require(const char* unique_name);

    void on_open_pressed();
    void on_file_selected(const godot::String& path);
    void on_layer_list_selected(int64_t index);
    void on_cell_hovered(int64_t x, int64_t y);

    void on_world_opened(const godot::String& path);
    void on_world_open_failed(const godot::String& path, const godot::String& code, const godot::String& message);
    void on_world_closed();
    void on_view_selected(int64_t index);
    void on_view_failed(int64_t index, const godot::String& code, const godot::String& message);

    void on_year_changed(int64_t year);
    void on_view_image_changed(int64_t index);
    void on_year_slider_changed(double value);
    void on_year_field_changed(double value);
    void on_step_year(int64_t delta);
    void on_step_event(int64_t direction);
    void on_step_change(int64_t direction);

    void show_status(const godot::String& text, bool is_error);
    void show_legend();
    void update_year_bar();
    void apply_start_year(const godot::String& text);
    void run_check_deferred();
    void run_bench();
    void take_screenshot();

    godot::Ref<ViewerModel> model_;
    godot::Button* open_button_ = nullptr;
    godot::FileDialog* open_dialog_ = nullptr;
    godot::Label* file_label_ = nullptr;
    godot::ItemList* layer_list_ = nullptr;
    godot::Tree* meta_tree_ = nullptr;
    LayerMapView* map_view_ = nullptr;
    godot::Label* status_label_ = nullptr;
    godot::Label* cell_label_ = nullptr;
    godot::Label* legend_title_ = nullptr;
    godot::ItemList* legend_list_ = nullptr;
    godot::Button* year_back25_ = nullptr;
    godot::Button* year_back1_ = nullptr;
    godot::Button* year_prev_event_ = nullptr;
    godot::Button* year_prev_change_ = nullptr;
    godot::HSlider* year_slider_ = nullptr;
    YearMarks* year_marks_ = nullptr;
    godot::Button* year_next_change_ = nullptr;
    godot::Button* year_next_event_ = nullptr;
    godot::Button* year_forward1_ = nullptr;
    godot::Button* year_forward25_ = nullptr;
    godot::SpinBox* year_field_ = nullptr;
    godot::Label* year_label_ = nullptr;
    godot::Label* year_counts_ = nullptr;
    bool wired_ = false;
    bool updating_year_bar_ = false;  // value_changed from our own updates is ignored
    int64_t hover_x_ = -1;            // the cell last hovered, (-1, -1) for none
    int64_t hover_y_ = -1;
    int64_t last_open_usec_ = -1;
    std::vector<int64_t> item_view_;  // per list item: view index, -1 for a heading
    std::vector<int64_t> view_item_;  // per view: list item
    int64_t start_hover_x_ = -1;
    int64_t start_hover_y_ = -1;

    godot::String check_reference_dir_;
    int64_t check_expect_layers_ = -1;
    int64_t check_expect_composites_ = -1;
    godot::String check_expect_history_;
    int64_t check_expect_end_year_ = -1;
    godot::String check_expect_years_;
    godot::String check_history_report_;
    godot::String screenshot_path_;
    godot::String bench_world_;
    bool bench_ = false;
};

}  // namespace unmapped
