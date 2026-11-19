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
// Command line (arguments after `--`):
//   <world.db>                 open this world at start
//   --view <name>              then show this layer or composite ("biome",
//                              "map", "biomes", "regions") instead of the
//                              first entry of the list
//   --hover <x> <y>            show the cell description of (x, y) as if the
//                              mouse were there (for screenshots)
//   --check <reference dir>    headless check (viewer_check.hpp), then quit
//   --expect-layers <n>        with --check: the number of layers expected
//   --expect-composites <n>    with --check: the number of composites expected
//   --screenshot <file.png>    save the window's contents after one second,
//                              then quit (needs a display; for review images)

#include <cstdint>
#include <vector>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/item_list.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/tree.hpp>
#include <godot_cpp/variant/string.hpp>

#include "viewer/layer_map_view.hpp"
#include "viewer/viewer_model.hpp"

namespace unmapped {

class ViewerScreen : public godot::Control {
    GDCLASS(ViewerScreen, godot::Control)

public:
    ViewerScreen() = default;

    void _ready() override;

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

    void show_status(const godot::String& text, bool is_error);
    void show_legend();
    void run_check_deferred();
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
    bool wired_ = false;
    std::vector<int64_t> item_view_;  // per list item: view index, -1 for a heading
    std::vector<int64_t> view_item_;  // per view: list item
    int64_t start_hover_x_ = -1;
    int64_t start_hover_y_ = -1;

    godot::String check_reference_dir_;
    int64_t check_expect_layers_ = -1;
    int64_t check_expect_composites_ = -1;
    godot::String screenshot_path_;
};

}  // namespace unmapped
