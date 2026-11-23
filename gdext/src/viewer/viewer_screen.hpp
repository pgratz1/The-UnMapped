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
// Markers (spec P4-04 8.2): %MarkersToggle, %MarkFaded, %MarkEvents,
// %MarkPasses call the model; %MarkerLegend is the key list. On
// markers_changed and view_selected the screen hands the model's markers and
// their glyphs to %MapView. Key M toggles the markers. While the mouse is on
// a marker, %CellLabel shows its description instead of the cell's.
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
//   --zoom <z> --centre <x> <y>  with --screenshot: show cell (x, y) at the
//                              centre at z screen pixels per cell
//   --mouse <x> <y>            with --screenshot: a mouse motion over the
//                              centre of cell (x, y) (markers hover too)
//   --faded                    start with the faded ruins shown
//   --expect-marker-years <a,b,...>  with --check: the years compared with
//                              <dir>/markers-<Y>.png (P4-04 9)
//   --expect-passes <n>        with --check: the world report's passes

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/check_box.hpp>
#include <godot_cpp/classes/check_button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/h_slider.hpp>
#include <godot_cpp/classes/image_texture.hpp>
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
    [[nodiscard]] godot::CheckButton* markers_toggle() const noexcept { return markers_toggle_; }
    [[nodiscard]] godot::CheckBox* mark_faded() const noexcept { return mark_faded_; }
    [[nodiscard]] godot::CheckBox* mark_events() const noexcept { return mark_events_; }
    [[nodiscard]] godot::CheckBox* mark_passes() const noexcept { return mark_passes_; }
    [[nodiscard]] godot::ItemList* marker_legend() const noexcept { return marker_legend_; }
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

    void on_markers_changed();
    void on_marker_hovered(int64_t index);
    void on_markers_toggled(bool on);
    void on_marker_option_toggled(bool on, const godot::String& name);
    void push_markers();
    void update_marker_controls();
    void show_hover_text();
    void apply_view_options();

    void show_status(const godot::String& text, bool is_error);
    void show_legend();
    void update_year_bar(bool marks = true);
    void apply_start_year(const godot::String& text);
    void run_check_deferred();
    void run_bench();
    void bench_frame_step();
    void finish_bench();
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
    godot::CheckButton* markers_toggle_ = nullptr;
    godot::CheckBox* mark_faded_ = nullptr;
    godot::CheckBox* mark_events_ = nullptr;
    godot::CheckBox* mark_passes_ = nullptr;
    godot::ItemList* marker_legend_ = nullptr;
    int64_t hovered_marker_ = -1;
    godot::Dictionary marker_glyphs_;  // glyph name -> {radius, cells}, filled on first use
    godot::Array marker_legend_rows_;  // the key list's rows as last shown
    godot::Array legend_rows_;         // the legend's rows as last shown
    std::map<std::vector<int32_t>, godot::Ref<godot::ImageTexture>> legend_icons_;  // by colour list
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
    godot::String check_expect_marker_years_;
    int64_t check_expect_passes_ = -1;
    double start_zoom_ = 0.0;
    int64_t start_centre_x_ = -1;
    int64_t start_centre_y_ = -1;
    int64_t start_mouse_x_ = -1;
    int64_t start_mouse_y_ = -1;
    bool start_faded_ = false;
    godot::String screenshot_path_;
    godot::String bench_world_;
    bool bench_ = false;
    // --bench (spec P4-05 12.3): the screen's steps of the last year change,
    // nanoseconds: (c) view_image_changed (texture update), (d) the
    // markers_changed handler, (f) the legend rebuild, (g) the year bar,
    // (h) the other year_changed work (the hover text).
    struct YearSteps {
        int64_t image = 0;
        int64_t markers = 0;
        int64_t legend = 0;
        int64_t year_bar = 0;
        int64_t hover = 0;
    };
    YearSteps year_steps_;
    // The windowed pass: one year change per frame, timed from set_year to
    // the next frame_post_draw.
    std::vector<int64_t> bench_frame_years_;
    std::size_t bench_frame_next_ = 0;
    int64_t bench_frame_start_ = -1;
    int64_t bench_frame_year_ = -1;
    int64_t bench_frame_idle_ = 0;  // frames drawn without a year change, timed too
    std::vector<std::pair<int64_t, int64_t>> bench_frame_times_;  // (year, nsec); year -1: idle frame
};

}  // namespace unmapped
