#include "viewer/viewer_screen.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <string_view>
#include <utility>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "viewer/viewer_check.hpp"
#include "worldgen/inspect/layer_catalogue.hpp"

namespace unmapped {

namespace {

int64_t now_nsec() {
    return static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

bool is_composite(const godot::Dictionary& info) {
    return info.get("kind", "") == godot::Variant("composite");
}

// Under its group heading the size is the same for every view of a world,
// so the list shows the name only (the tooltip has the rest).
godot::String view_label(const godot::Dictionary& info) {
    const godot::String name = info["name"];
    if (is_composite(info)) {
        return "   " + name;
    }
    return "   " + name + "   " + godot::String(info["element_type"]);
}

// Legend icons: a strip of LEGEND_ICON_W x LEGEND_ICON_H pixels, one colour
// or a linear blend of several, evenly spaced (display only).
constexpr int32_t LEGEND_ICON_W = 40;
constexpr int32_t LEGEND_ICON_H = 14;

godot::Ref<godot::ImageTexture> legend_icon(const godot::PackedInt32Array& colours) {
    godot::PackedByteArray data;
    data.resize(static_cast<int64_t>(LEGEND_ICON_W) * LEGEND_ICON_H * 3);
    uint8_t* out = data.ptrw();
    const int64_t n = colours.size();
    const auto channel = [](int32_t c, int shift) { return static_cast<double>((c >> shift) & 0xFF); };
    for (int32_t x = 0; x < LEGEND_ICON_W; ++x) {
        std::array<double, 3> rgb{0.0, 0.0, 0.0};
        if (n == 1) {
            rgb = {channel(colours[0], 16), channel(colours[0], 8), channel(colours[0], 0)};
        } else if (n > 1) {
            const double t = static_cast<double>(x) * static_cast<double>(n - 1) / (LEGEND_ICON_W - 1);
            const auto k = std::min(static_cast<int64_t>(t), n - 2);
            const double f = t - static_cast<double>(k);
            for (std::size_t c = 0; c < 3; ++c) {
                const int shift = 16 - 8 * static_cast<int>(c);
                rgb[c] = channel(colours[k], shift) * (1.0 - f) + channel(colours[k + 1], shift) * f;
            }
        }
        for (int32_t y = 0; y < LEGEND_ICON_H; ++y) {
            uint8_t* px = out + (static_cast<int64_t>(y) * LEGEND_ICON_W + x) * 3;
            for (std::size_t c = 0; c < 3; ++c) {
                px[c] = static_cast<uint8_t>(std::lround(std::clamp(rgb[c], 0.0, 255.0)));
            }
        }
    }
    const godot::Ref<godot::Image> image =
        godot::Image::create_from_data(LEGEND_ICON_W, LEGEND_ICON_H, false, godot::Image::FORMAT_RGB8, data);
    return godot::ImageTexture::create_from_image(image);
}

// The key list's icons (P4-04 8.6, P4-05 12.1 F7): the glyph at g screen
// pixels per glyph cell, g = 2 except the ring rows (g = 1: a ring at 2
// would be 46 px), centred on a MARKER_ICON x MARKER_ICON canvas (the ring's
// size at 1; the largest other glyph, a city of 11 cells, is 22 px at 2).
constexpr int32_t MARKER_ICON = 23;
constexpr int64_t MARKER_ICON_G = 2;
constexpr int64_t MARKER_ICON_RING_G = 1;

godot::Ref<godot::ImageTexture> marker_icon(const godot::Dictionary& glyph, int64_t fill, int64_t g) {
    const int64_t radius = glyph.get("radius", int64_t{-1});
    const godot::PackedByteArray cells = glyph.get("cells", godot::PackedByteArray());
    const int64_t side = 2 * radius + 1;
    if (radius < 0 || g < 1 || side * g > MARKER_ICON || cells.size() != side * side) {
        return {};
    }
    godot::PackedByteArray data;
    data.resize(static_cast<int64_t>(MARKER_ICON) * MARKER_ICON * 4);
    data.fill(0);
    uint8_t* out = data.ptrw();
    const int64_t pad = (MARKER_ICON - side * g) / 2;
    for (int64_t row = 0; row < side; ++row) {
        for (int64_t col = 0; col < side; ++col) {
            const uint8_t cls = cells[row * side + col];
            int64_t rgb = -1;
            if (cls == 1) {
                rgb = 0xF0F0F0;
            } else if (cls == 2) {
                rgb = fill;
            } else if (cls == 3) {
                rgb = 0x000000;
            }
            if (rgb < 0) {
                continue;
            }
            for (int64_t sy = 0; sy < g; ++sy) {
                for (int64_t sx = 0; sx < g; ++sx) {
                    uint8_t* px = out + ((row * g + sy + pad) * MARKER_ICON + col * g + sx + pad) * 4;
                    px[0] = static_cast<uint8_t>((rgb >> 16) & 0xFF);
                    px[1] = static_cast<uint8_t>((rgb >> 8) & 0xFF);
                    px[2] = static_cast<uint8_t>(rgb & 0xFF);
                    px[3] = 255;
                }
            }
        }
    }
    const godot::Ref<godot::Image> image =
        godot::Image::create_from_data(MARKER_ICON, MARKER_ICON, false, godot::Image::FORMAT_RGBA8, data);
    return godot::ImageTexture::create_from_image(image);
}

godot::String view_tooltip(const godot::Dictionary& info) {
    if (is_composite(info)) {
        return godot::String("{0} (composite)\n{1} x {2} cells, built by worldgen from elevation and the layers "
                             "it uses")
            .format(godot::Array::make(info["name"], info["width"], info["height"]));
    }
    const godot::String palette = info.get("palette", "");
    return godot::String("{0}\n{1} x {2} cells, {3}\ncodec {4}, {5} bytes encoded{6}")
        .format(godot::Array::make(info["name"], info["width"], info["height"], info["element_type"],
                                   info["codec"], info["encoded_size"],
                                   palette.is_empty() ? godot::String() : "\npalette " + palette));
}

}  // namespace

template <class T>
T* ViewerScreen::require(const char* unique_name) {
    const godot::NodePath path(godot::String("%") + unique_name);
    T* node = get_node<T>(path);
    if (node == nullptr) {
        godot::UtilityFunctions::push_error(godot::String("ViewerScreen: the scene has no node %") + unique_name +
                                            " of the expected type");
    }
    return node;
}

void ViewerScreen::_ready() {
    if (godot::Engine::get_singleton()->is_editor_hint()) {
        return;
    }
    open_button_ = require<godot::Button>("OpenButton");
    open_dialog_ = require<godot::FileDialog>("OpenDialog");
    file_label_ = require<godot::Label>("FileLabel");
    layer_list_ = require<godot::ItemList>("LayerList");
    meta_tree_ = require<godot::Tree>("MetaTree");
    map_view_ = require<LayerMapView>("MapView");
    status_label_ = require<godot::Label>("StatusLabel");
    cell_label_ = require<godot::Label>("CellLabel");
    legend_title_ = require<godot::Label>("LegendTitle");
    legend_list_ = require<godot::ItemList>("LegendList");
    year_back25_ = require<godot::Button>("YearBack25");
    year_back1_ = require<godot::Button>("YearBack1");
    year_prev_event_ = require<godot::Button>("YearPrevEvent");
    year_prev_change_ = require<godot::Button>("YearPrevChange");
    year_slider_ = require<godot::HSlider>("YearSlider");
    year_marks_ = require<YearMarks>("YearMarks");
    year_next_change_ = require<godot::Button>("YearNextChange");
    year_next_event_ = require<godot::Button>("YearNextEvent");
    year_forward1_ = require<godot::Button>("YearForward1");
    year_forward25_ = require<godot::Button>("YearForward25");
    year_field_ = require<godot::SpinBox>("YearField");
    year_label_ = require<godot::Label>("YearLabel");
    year_counts_ = require<godot::Label>("YearCounts");
    markers_toggle_ = require<godot::CheckButton>("MarkersToggle");
    mark_faded_ = require<godot::CheckBox>("MarkFaded");
    mark_events_ = require<godot::CheckBox>("MarkEvents");
    mark_passes_ = require<godot::CheckBox>("MarkPasses");
    marker_legend_ = require<godot::ItemList>("MarkerLegend");
    right_pane_ = require<godot::TabContainer>("RightPane");
    browser_back_ = require<godot::Button>("BrowserBack");
    browser_forward_ = require<godot::Button>("BrowserForward");
    browser_search_ = require<godot::LineEdit>("BrowserSearch");
    entity_tabs_ = require<godot::TabBar>("EntityTabs");
    civ_list_ = require<godot::Tree>("CivList");
    search_results_ = require<godot::ItemList>("SearchResults");
    event_scope_ = require<godot::OptionButton>("EventScope");
    event_group_ = require<godot::OptionButton>("EventGroup");
    event_all_ = require<godot::CheckBox>("EventAll");
    event_from_ = require<godot::SpinBox>("EventFrom");
    event_to_ = require<godot::SpinBox>("EventTo");
    event_this_year_ = require<godot::CheckBox>("EventThisYear");
    follow_toggle_ = require<godot::CheckBox>("FollowToggle");
    follow_first_ = require<godot::Button>("FollowFirst");
    event_list_ = require<godot::ItemList>("EventList");
    browser_detail_ = require<godot::RichTextLabel>("BrowserDetail");
    wired_ = right_pane_ != nullptr && browser_back_ != nullptr && browser_forward_ != nullptr &&
             browser_search_ != nullptr && entity_tabs_ != nullptr && civ_list_ != nullptr &&
             search_results_ != nullptr && event_scope_ != nullptr && event_group_ != nullptr &&
             event_all_ != nullptr && event_from_ != nullptr && event_to_ != nullptr &&
             event_this_year_ != nullptr && follow_toggle_ != nullptr && follow_first_ != nullptr &&
             event_list_ != nullptr && browser_detail_ != nullptr &&
             markers_toggle_ != nullptr && mark_faded_ != nullptr && mark_events_ != nullptr &&
             mark_passes_ != nullptr && marker_legend_ != nullptr && open_button_ != nullptr && open_dialog_ != nullptr && file_label_ != nullptr && layer_list_ != nullptr &&
             meta_tree_ != nullptr && map_view_ != nullptr && status_label_ != nullptr && cell_label_ != nullptr &&
             legend_title_ != nullptr && legend_list_ != nullptr && year_slider_ != nullptr &&
             year_marks_ != nullptr && year_field_ != nullptr && year_label_ != nullptr && year_counts_ != nullptr;
    for (const godot::Button* button : year_buttons()) {
        wired_ = wired_ && button != nullptr;
    }

    model_.instantiate();
    if (wired_) {
        open_button_->connect("pressed", callable_mp(this, &ViewerScreen::on_open_pressed));
        open_dialog_->connect("file_selected", callable_mp(this, &ViewerScreen::on_file_selected));
        layer_list_->connect("item_selected", callable_mp(this, &ViewerScreen::on_layer_list_selected));
        map_view_->connect("cell_hovered", callable_mp(this, &ViewerScreen::on_cell_hovered));
        meta_tree_->set_column_title(0, "Key");
        meta_tree_->set_column_title(1, "Value");
        model_->connect("world_opened", callable_mp(this, &ViewerScreen::on_world_opened));
        model_->connect("world_open_failed", callable_mp(this, &ViewerScreen::on_world_open_failed));
        model_->connect("world_closed", callable_mp(this, &ViewerScreen::on_world_closed));
        model_->connect("view_selected", callable_mp(this, &ViewerScreen::on_view_selected));
        model_->connect("view_failed", callable_mp(this, &ViewerScreen::on_view_failed));
        model_->connect("year_changed", callable_mp(this, &ViewerScreen::on_year_changed));
        model_->connect("view_image_changed", callable_mp(this, &ViewerScreen::on_view_image_changed));
        model_->connect("markers_changed", callable_mp(this, &ViewerScreen::on_markers_changed));
        map_view_->connect("marker_hovered", callable_mp(this, &ViewerScreen::on_marker_hovered));
        markers_toggle_->connect("toggled", callable_mp(this, &ViewerScreen::on_markers_toggled));
        mark_faded_->connect("toggled",
                             callable_mp(this, &ViewerScreen::on_marker_option_toggled).bind(godot::String("faded")));
        mark_events_->connect("toggled",
                              callable_mp(this, &ViewerScreen::on_marker_option_toggled).bind(godot::String("events")));
        mark_passes_->connect("toggled",
                              callable_mp(this, &ViewerScreen::on_marker_option_toggled).bind(godot::String("passes")));
        update_marker_controls();
        year_slider_->connect("value_changed", callable_mp(this, &ViewerScreen::on_year_slider_changed));
        year_field_->connect("value_changed", callable_mp(this, &ViewerScreen::on_year_field_changed));
        year_back25_->connect("pressed", callable_mp(this, &ViewerScreen::on_step_year).bind(int64_t{-25}));
        year_back1_->connect("pressed", callable_mp(this, &ViewerScreen::on_step_year).bind(int64_t{-1}));
        year_prev_event_->connect("pressed", callable_mp(this, &ViewerScreen::on_step_event).bind(int64_t{-1}));
        year_prev_change_->connect("pressed", callable_mp(this, &ViewerScreen::on_step_change).bind(int64_t{-1}));
        year_next_change_->connect("pressed", callable_mp(this, &ViewerScreen::on_step_change).bind(int64_t{1}));
        year_next_event_->connect("pressed", callable_mp(this, &ViewerScreen::on_step_event).bind(int64_t{1}));
        year_forward1_->connect("pressed", callable_mp(this, &ViewerScreen::on_step_year).bind(int64_t{1}));
        year_forward25_->connect("pressed", callable_mp(this, &ViewerScreen::on_step_year).bind(int64_t{25}));
        update_year_bar();
        wire_browser();
        set_process_unhandled_key_input(true);
        set_process_input(true);
    }

    // Arguments after `--` on the Godot command line.
    const godot::PackedStringArray args = godot::OS::get_singleton()->get_cmdline_user_args();
    godot::String world_path;
    godot::String start_view;
    godot::String start_year;
    bool has_start_year = false;
    for (int64_t i = 0; i < args.size(); ++i) {
        const godot::String arg = args[i];
        if (arg == "--view" && i + 1 < args.size()) {
            start_view = args[++i];
        } else if (arg == "--check" && i + 1 < args.size()) {
            check_reference_dir_ = args[++i];
        } else if (arg == "--expect-layers" && i + 1 < args.size()) {
            check_expect_layers_ = args[++i].to_int();
        } else if (arg == "--expect-composites" && i + 1 < args.size()) {
            check_expect_composites_ = args[++i].to_int();
        } else if (arg == "--expect-history" && i + 1 < args.size()) {
            check_expect_history_ = args[++i];
        } else if (arg == "--expect-end-year" && i + 1 < args.size()) {
            check_expect_end_year_ = args[++i].to_int();
        } else if (arg == "--expect-years" && i + 1 < args.size()) {
            check_expect_years_ = args[++i];
        } else if (arg == "--history-report" && i + 1 < args.size()) {
            check_history_report_ = args[++i];
        } else if (arg == "--year" && i + 1 < args.size()) {
            start_year = args[++i];
            has_start_year = true;
        } else if (arg == "--bench") {
            bench_ = true;
        } else if (arg == "--hover" && i + 2 < args.size()) {
            start_hover_x_ = args[++i].to_int();
            start_hover_y_ = args[++i].to_int();
        } else if (arg == "--screenshot" && i + 1 < args.size()) {
            screenshot_path_ = args[++i];
        } else if (arg == "--zoom" && i + 1 < args.size()) {
            start_zoom_ = args[++i].to_float();
        } else if (arg == "--centre" && i + 2 < args.size()) {
            start_centre_x_ = args[++i].to_int();
            start_centre_y_ = args[++i].to_int();
        } else if (arg == "--mouse" && i + 2 < args.size()) {
            start_mouse_x_ = args[++i].to_int();
            start_mouse_y_ = args[++i].to_int();
        } else if (arg == "--faded") {
            start_faded_ = true;
        } else if (arg == "--expect-marker-years" && i + 1 < args.size()) {
            check_expect_marker_years_ = args[++i];
        } else if (arg == "--expect-passes" && i + 1 < args.size()) {
            check_expect_passes_ = args[++i].to_int();
        } else if (arg == "--history-summary" && i + 1 < args.size()) {
            check_history_summary_ = args[++i];
        } else if (arg == "--select" && i + 1 < args.size()) {
            start_select_ = args[++i];
        } else if (arg == "--browser-group" && i + 1 < args.size()) {
            start_group_ = args[++i];
        } else if (arg == "--browser-full") {
            start_full_ = true;
        } else if (arg == "--search" && i + 1 < args.size()) {
            start_search_ = args[++i];
        } else if (arg == "--tab" && i + 1 < args.size()) {
            start_tab_ = args[++i];
        } else if (arg == "--size" && i + 2 < args.size()) {
            start_width_ = args[++i].to_int();
            start_height_ = args[++i].to_int();
        } else if (!arg.begins_with("--") && world_path.is_empty()) {
            world_path = arg;
        } else {
            godot::UtilityFunctions::push_warning("Viewer: ignoring command-line argument ", arg);
        }
    }

    if (bench_) {
        // The bench opens the world itself, timed.
        bench_world_ = world_path;
        callable_mp(this, &ViewerScreen::run_bench).call_deferred();
        return;
    }
    if (!world_path.is_empty() && wired_) {
        const bool opened = open_world(world_path);
        if (opened && !start_view.is_empty()) {
            bool found = false;
            for (int64_t i = 0; i < model_->get_view_count() && !found; ++i) {
                if (model_->get_view_info(i)["name"] == godot::Variant(start_view)) {
                    choose_view(i);
                    found = true;
                }
            }
            if (!found) {
                godot::UtilityFunctions::push_warning("Viewer: no layer or composite called ", start_view);
            }
        }
        if (opened && has_start_year) {
            apply_start_year(start_year);
        }
        if (opened && start_faded_) {
            model_->set_marker_option("faded", true);
        }
        if (opened) {
            apply_browser_options();
        }
        if (start_hover_x_ >= 0 && start_hover_y_ >= 0) {
            hover_cell(start_hover_x_, start_hover_y_);
        }
    } else if (wired_) {
        show_status("Open a world file (world.db) to view its layers.", false);
    }

    if (!check_reference_dir_.is_empty()) {
        // The headless display server has no window: the root viewport has
        // no size and the screen would shrink to its minimum. Lay it out at
        // the project's window size, as on a desktop.
        if (godot::DisplayServer::get_singleton()->get_name() == "headless") {
            godot::ProjectSettings* settings = godot::ProjectSettings::get_singleton();
            const int64_t width = settings->get_setting("display/window/size/viewport_width");
            const int64_t height = settings->get_setting("display/window/size/viewport_height");
            const godot::Vector2 size(static_cast<float>(width), static_cast<float>(height));
            set_anchors_preset(PRESET_TOP_LEFT);
            set_size(size);
            godot::UtilityFunctions::print("Viewer: headless, screen laid out at ", size);
        }
        // After a few frames, once containers have settled the layout.
        get_tree()->create_timer(0.25)->connect("timeout", callable_mp(this, &ViewerScreen::run_check_deferred));
    } else if (!screenshot_path_.is_empty()) {
        get_tree()->create_timer(0.6)->connect("timeout", callable_mp(this, &ViewerScreen::apply_view_options));
        get_tree()->create_timer(1.2)->connect("timeout", callable_mp(this, &ViewerScreen::take_screenshot));
    }
}

void ViewerScreen::apply_view_options() {
    if (!wired_) {
        return;
    }
    if (start_zoom_ > 0.0 && start_centre_x_ >= 0 && start_centre_y_ >= 0) {
        map_view_->centre_on(godot::Vector2(static_cast<float>(start_centre_x_), static_cast<float>(start_centre_y_)),
                             start_zoom_);
    }
    if (start_mouse_x_ >= 0 && start_mouse_y_ >= 0) {
        godot::Ref<godot::InputEventMouseMotion> motion;
        motion.instantiate();
        motion->set_position(map_view_->cell_centre_position(start_mouse_x_, start_mouse_y_));
        map_view_->_gui_input(motion);
    }
}

void ViewerScreen::take_screenshot() {
    const godot::Ref<godot::Image> image = get_viewport()->get_texture()->get_image();
    const bool saved = image.is_valid() && image->save_png(screenshot_path_) == godot::OK;
    if (saved) {
        godot::UtilityFunctions::print("Viewer: screenshot saved to ", screenshot_path_);
    } else {
        godot::UtilityFunctions::printerr("Viewer: cannot save a screenshot to ", screenshot_path_,
                                          " (no display, or the path is not writable)");
    }
    get_tree()->quit(saved ? 0 : 1);
}

void ViewerScreen::run_check_deferred() {
    ViewerCheckOptions options;
    options.reference_dir = check_reference_dir_;
    options.expect_layers = check_expect_layers_;
    options.expect_composites = check_expect_composites_;
    options.expect_history = check_expect_history_;
    options.expect_end_year = check_expect_end_year_;
    const godot::PackedStringArray years = check_expect_years_.split(",", false);
    for (int64_t i = 0; i < years.size(); ++i) {
        options.expect_years.push_back(years[i].strip_edges().to_int());
    }
    options.history_report = check_history_report_;
    const godot::PackedStringArray marker_years = check_expect_marker_years_.split(",", false);
    for (int64_t i = 0; i < marker_years.size(); ++i) {
        options.marker_years.push_back(marker_years[i].strip_edges().to_int());
    }
    options.expect_passes = check_expect_passes_;
    options.history_summary = check_history_summary_;
    options.wired = wired_;
    const int exit_code = run_viewer_check(*this, options);
    get_tree()->quit(exit_code);
}

godot::Ref<ViewerModel> ViewerScreen::get_model() const {
    return model_;
}

bool ViewerScreen::open_world(const godot::String& path) {
    const auto start = static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec());
    if (!wired_ || !model_->open_world(path)) {
        return false;
    }
    // The model selects its first view (the first layer); show the first
    // entry of the list instead, a composite when there is one.
    for (std::size_t item = 0; item < item_view_.size(); ++item) {
        if (item_view_[item] >= 0) {
            choose_view(item_view_[item]);
            break;
        }
    }
    last_open_usec_ = static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec()) - start;
    return true;
}

void ViewerScreen::choose_view(int64_t index) {
    const int64_t item = item_of_view(index);
    if (!wired_ || item < 0) {
        return;
    }
    layer_list_->select(static_cast<int32_t>(item));
    on_layer_list_selected(item);
}

void ViewerScreen::hover_cell(int64_t x, int64_t y) {
    if (wired_) {
        on_cell_hovered(x, y);
    }
}

int64_t ViewerScreen::item_of_view(int64_t view) const {
    return view >= 0 && static_cast<std::size_t>(view) < view_item_.size() ? view_item_[static_cast<std::size_t>(view)]
                                                                           : -1;
}

int64_t ViewerScreen::view_of_item(int64_t item) const {
    return item >= 0 && static_cast<std::size_t>(item) < item_view_.size() ? item_view_[static_cast<std::size_t>(item)]
                                                                           : -1;
}

void ViewerScreen::on_open_pressed() {
    open_dialog_->popup_centered_ratio(0.7F);
}

void ViewerScreen::on_file_selected(const godot::String& path) {
    open_world(path);
}

void ViewerScreen::on_layer_list_selected(int64_t item) {
    const int64_t view = view_of_item(item);
    if (view >= 0) {
        model_->select_view(view);
    }
}

void ViewerScreen::on_cell_hovered(int64_t x, int64_t y) {
    hover_x_ = x;
    hover_y_ = y;
    show_hover_text();
}

void ViewerScreen::show_hover_text() {
    // P4-04 8.5: a marker under the mouse is described instead of the cell.
    if (hovered_marker_ >= 0) {
        const godot::String text = model_->describe_marker(hovered_marker_);
        if (!text.is_empty()) {
            cell_label_->set_text(text);
            return;
        }
    }
    const godot::String text =
        hover_x_ < 0 || hover_y_ < 0 ? godot::String() : model_->describe_cell(hover_x_, hover_y_);
    cell_label_->set_text(text.is_empty() ? godot::String("Point at the map to inspect a cell.") : text);
}

void ViewerScreen::on_marker_hovered(int64_t index) {
    hovered_marker_ = index;
    show_hover_text();
}

void ViewerScreen::push_markers() {
    const godot::Array markers = model_->get_markers();
    for (int64_t i = 0; i < markers.size(); ++i) {
        const godot::String name = godot::Dictionary(markers[i])["glyph"];
        if (!marker_glyphs_.has(name)) {
            marker_glyphs_[name] = model_->get_marker_glyph(name);
        }
    }
    map_view_->set_markers(markers, marker_glyphs_);
}

void ViewerScreen::on_markers_changed() {
    const int64_t start = now_nsec();
    push_markers();
    push_highlight_frames();  // the frames name entries of the new list
    update_marker_controls();
    show_hover_text();
    year_steps_.markers = now_nsec() - start;
}

void ViewerScreen::on_markers_toggled(bool on) {
    model_->set_markers_enabled(on);
    update_marker_controls();
}

void ViewerScreen::on_marker_option_toggled(bool on, const godot::String& name) {
    model_->set_marker_option(name, on);
    update_marker_controls();
}

void ViewerScreen::update_marker_controls() {
    if (!wired_) {
        return;
    }
    const bool available = model_->get_markers_available();
    const bool enabled = available && model_->get_markers_enabled();
    markers_toggle_->set_pressed_no_signal(enabled);
    markers_toggle_->set_disabled(!available);
    const std::array<std::pair<godot::CheckBox*, const char*>, 3> boxes{
        {{mark_faded_, "faded"}, {mark_events_, "events"}, {mark_passes_, "passes"}}};
    for (const auto& [box, name] : boxes) {
        // Without markers the boxes show the defaults (8.2: faded off).
        box->set_pressed_no_signal(available ? model_->get_marker_option(name) : godot::String(name) != "faded");
        box->set_disabled(!enabled);
    }
    const godot::Array legend = model_->get_marker_legend();
    // The key list changes only with the options and the flag: rebuilt
    // (icons and all) only when its rows differ.
    if (legend == marker_legend_rows_ && marker_legend_->get_item_count() == legend.size()) {
        return;
    }
    marker_legend_rows_ = legend.duplicate(true);
    marker_legend_->clear();
    for (int64_t i = 0; i < legend.size(); ++i) {
        const godot::Dictionary row = legend[i];
        const godot::PackedInt32Array colours = row["colours"];
        const int64_t fill = colours.is_empty() ? ViewerModel::marker_legend_grey() : int64_t{colours[0]} & 0xFFFFFF;
        const godot::String glyph = row["glyph"];
        const godot::Ref<godot::Texture2D> icon = marker_icon(model_->get_marker_glyph(glyph), fill,
                                                              glyph == "ring" ? MARKER_ICON_RING_G : MARKER_ICON_G);
        const int32_t item = marker_legend_->add_item(row["label"], icon, false);
        marker_legend_->set_item_tooltip_enabled(item, false);
    }
}

void ViewerScreen::on_world_opened(const godot::String& path) {
    // P4-03 4.6: a history that could not be read is named under the path.
    godot::String file_text =
        model_->get_history_status() == "failed"
            ? path + godot::String("\nhistory: ") + model_->get_history_error_code() + godot::String(": ") + model_->get_history_error_message()
            : path;
    // P4-04 8.1: markers that could not be built likewise.
    if (model_->has_history() && !model_->get_markers_available()) {
        file_text += godot::String("\nmarkers: ") + model_->get_markers_error_code() + godot::String(": ") +
                     model_->get_markers_error_message();
    }
    // P4-05 5.1: a browser that could not be built likewise.
    if (model_->get_markers_available() && !model_->get_browser_available()) {
        file_text += godot::String("\nbrowser: ") + model_->get_browser_error_code() + godot::String(": ") +
                     model_->get_browser_error_message();
    }
    file_label_->set_text(file_text);
    file_label_->set_tooltip_text(file_text);
    hover_x_ = -1;
    hover_y_ = -1;

    layer_list_->clear();
    item_view_.clear();
    const int64_t count = model_->get_view_count();
    view_item_.assign(static_cast<std::size_t>(count), -1);
    // Group by the model's group titles, in the catalogue's group order;
    // within a group, in view order (layer id order).
    for (std::size_t g = 0; g < worldgen::inspect::LAYER_GROUP_COUNT; ++g) {
        const std::string_view group_title =
            worldgen::inspect::layer_group_title(static_cast<worldgen::inspect::LayerGroup>(g));
        const godot::String title =
            godot::String::utf8(group_title.data(), static_cast<int64_t>(group_title.size()));
        bool heading = false;
        for (int64_t i = 0; i < count; ++i) {
            const godot::Dictionary info = model_->get_view_info(i);
            if (info["group"] != godot::Variant(title)) {
                continue;
            }
            if (!heading) {
                const int32_t head = layer_list_->add_item(title, godot::Ref<godot::Texture2D>(), false);
                layer_list_->set_item_disabled(head, true);
                item_view_.push_back(-1);
                heading = true;
            }
            const int32_t item = layer_list_->add_item(view_label(info));
            layer_list_->set_item_tooltip(item, view_tooltip(info));
            item_view_.push_back(i);
            view_item_[static_cast<std::size_t>(i)] = item;
        }
    }

    meta_tree_->clear();
    godot::TreeItem* root = meta_tree_->create_item();
    const godot::Array entries = model_->get_meta_entries();
    for (int64_t i = 0; i < entries.size(); ++i) {
        const godot::Dictionary entry = entries[i];
        godot::TreeItem* row = meta_tree_->create_item(root);
        row->set_text(0, entry["key"]);
        row->set_text(1, entry["value"]);
        row->set_tooltip_text(1, entry["value"]);
    }

    cell_label_->set_text("Point at the map to inspect a cell.");
    hovered_marker_ = -1;
    update_marker_controls();
    browser_world_opened();
    if (count == 0) {
        map_view_->clear_image();
        show_status("This world has no layers.", false);
    } else {
        const int64_t layers = model_->get_layer_count();
        const int64_t composites = model_->get_composite_count();
        godot::String text = godot::String::num_int64(layers) + (layers == 1 ? " layer" : " layers");
        if (composites > 0) {
            text += godot::String(", ") + godot::String::num_int64(composites) +
                    (composites == 1 ? " composite" : " composites");
        }
        show_status(text, false);
    }
    update_year_bar();
}

void ViewerScreen::on_world_open_failed(const godot::String& path, const godot::String& code,
                                        const godot::String& message) {
    file_label_->set_text("No world open");
    file_label_->set_tooltip_text("");
    godot::String text = godot::String("Cannot open {0}: {1} ({2})").format(godot::Array::make(path, message, code));
    if (code == "file_unreadable" && path.is_relative_path()) {
        text += ". Give an absolute path: Godot runs from the project folder.";
    }
    show_status(text, true);
    godot::UtilityFunctions::printerr(text);
}

void ViewerScreen::on_world_closed() {
    legend_rows_.clear();
    legend_icons_.clear();
    layer_list_->clear();
    item_view_.clear();
    view_item_.clear();
    legend_list_->clear();
    legend_title_->set_text("");
    cell_label_->set_text(" ");
    meta_tree_->clear();
    map_view_->clear_image();
    map_view_->clear_markers();
    hover_x_ = -1;
    hover_y_ = -1;
    hovered_marker_ = -1;
    update_year_bar();
    update_marker_controls();
    browser_world_closed();
}

void ViewerScreen::on_view_selected(int64_t index) {
    const auto item = static_cast<int32_t>(item_of_view(index));
    if (item >= 0) {
        if (!layer_list_->is_selected(item)) {
            layer_list_->select(item);
        }
        // After layout: on opening, the list has no final size yet and
        // would scroll as if it were one row high.
        layer_list_->call_deferred("ensure_current_is_visible");
    }
    const godot::Dictionary info = model_->get_view_info(index);
    if (item >= 0) {
        layer_list_->set_item_tooltip(item, view_tooltip(info));  // palette now known
    }
    map_view_->set_image(model_->get_view_image(index));
    push_markers();  // P4-04 8.4: over every view
    show_status(view_tooltip(info).replace("\n", "   "), false);
    show_legend();
    on_highlight_changed();  // the mask fits the map of the world
}

void ViewerScreen::show_legend() {
    legend_title_->set_text(model_->get_legend_title());
    const godot::Array legend = model_->get_legend();
    // P4-05 12.3: rebuilt only when its rows differ from those shown (an
    // answer compared, not cached); most year changes leave them alone.
    if (legend == legend_rows_ && legend_list_->get_item_count() == legend.size()) {
        return;
    }
    legend_rows_ = legend.duplicate(true);
    legend_list_->clear();
    for (int64_t i = 0; i < legend.size(); ++i) {
        const godot::Dictionary entry = legend[i];
        const godot::PackedInt32Array colours = entry["colours"];
        // A row without colours is a heading: no swatch (P4-03 6.4). One
        // icon texture per distinct colour list, kept while the world is open.
        godot::Ref<godot::Texture2D> icon;
        if (!colours.is_empty()) {
            std::vector<int32_t> key(colours.ptr(), colours.ptr() + colours.size());
            auto found = legend_icons_.find(key);
            if (found == legend_icons_.end()) {
                found = legend_icons_.emplace(std::move(key), legend_icon(colours)).first;
            }
            icon = found->second;
        }
        const int32_t item = legend_list_->add_item(entry["label"], icon, false);
        const godot::String tooltip = entry.get("tooltip", godot::String());
        if (tooltip.is_empty()) {
            legend_list_->set_item_tooltip_enabled(item, false);
        } else {
            legend_list_->set_item_tooltip(item, tooltip);
        }
    }
}

void ViewerScreen::on_view_failed(int64_t index, const godot::String& code, const godot::String& message) {
    const godot::Dictionary info = model_->get_view_info(index);
    const godot::String text = godot::String("Cannot show {0} {1}: {2} ({3})")
                                   .format(godot::Array::make(info["kind"], info["name"], message, code));
    show_status(text, true);
    godot::UtilityFunctions::printerr(text);
}

void ViewerScreen::show_status(const godot::String& text, bool is_error) {
    status_label_->set_text(text);
    if (is_error) {
        status_label_->add_theme_color_override("font_color", godot::Color(1.0F, 0.45F, 0.4F));
    } else {
        status_label_->remove_theme_color_override("font_color");
    }
}

std::vector<godot::Button*> ViewerScreen::year_buttons() const {
    return {year_back25_,      year_back1_,      year_prev_event_, year_prev_change_,
            year_next_change_, year_next_event_, year_forward1_,   year_forward25_};
}

godot::String ViewerScreen::year_label_text() const {
    const godot::String status = model_->get_history_status();
    if (status == "failed") {
        return "History not read: " + model_->get_history_error_code() + ": " + model_->get_history_error_message();
    }
    if (status != "loaded") {
        return "No history in this world.";
    }
    const godot::Dictionary info = model_->get_year_info();
    const int64_t year = info["year"];
    const int64_t end = info["end_year"];
    godot::String text = "Year " + godot::String::num_int64(year) + " of " + godot::String::num_int64(end);
    if (year == end) {
        text += " (the present)";
    }
    if (int64_t(info["age"]) != 0) {
        text += "   |   " + godot::String(info["age_name"]);
    }
    if (int64_t(info["phase"]) == 0) {
        text += "   |   no phase";
    } else {
        text += "   |   phase p" + godot::String::num_int64(info["phase"]) + " " + godot::String(info["phase_kind"]) +
                ", " + godot::String::num_int64(info["phase_start"]) + "-" + godot::String::num_int64(info["phase_end"]);
    }
    return text;
}

godot::String ViewerScreen::year_counts_text() const {
    if (!model_->has_history()) {
        return {};
    }
    const godot::Dictionary info = model_->get_year_info();
    const auto count = [&info](const char* key, const char* one, const char* many) {
        const int64_t n = info[key];
        return godot::String::num_int64(n) + " " + (n == 1 ? one : many);
    };
    return count("realms", "realm", "realms") + " alive, " + count("regions_held", "region", "regions") + " held, " +
           count("settlements", "settlement", "settlements") + " standing, " + count("wars", "war", "wars") + ", " +
           count("events", "event", "events") + " this year";
}

void ViewerScreen::update_year_bar(bool marks) {
    if (!wired_) {
        return;
    }
    const bool history = model_->has_history();
    const int64_t year = history ? model_->get_year() : 0;
    const int64_t end = history ? model_->get_end_year() : 0;
    updating_year_bar_ = true;
    year_slider_->set_min(0.0);
    year_slider_->set_max(static_cast<double>(end));
    year_slider_->set_value_no_signal(static_cast<double>(year));
    year_slider_->set_editable(history);
    year_field_->set_min(0.0);
    year_field_->set_max(static_cast<double>(end));
    year_field_->set_value_no_signal(static_cast<double>(year));
    year_field_->set_editable(history);
    updating_year_bar_ = false;
    year_back25_->set_disabled(!history || year == 0);
    year_back1_->set_disabled(!history || year == 0);
    year_forward1_->set_disabled(!history || year == end);
    year_forward25_->set_disabled(!history || year == end);
    year_prev_event_->set_disabled(!model_->has_previous_event());
    year_next_event_->set_disabled(!model_->has_next_event());
    year_prev_change_->set_disabled(!model_->has_previous_change());
    year_next_change_->set_disabled(!model_->has_next_change());
    year_label_->set_text(year_label_text());
    year_counts_->set_text(year_counts_text());
    if (marks) {
        // The marks do not depend on the year: set when a world opens or closes.
        year_marks_->set_marks(model_->get_year_marks(), history ? end : 0);
    }
}

void ViewerScreen::on_year_changed(int64_t /*year*/) {
    const int64_t start = now_nsec();
    update_year_bar(false);
    scroll_event_list();  // P4-05 6.3: to the events of the year, no rebuild
    const int64_t bar = now_nsec();
    year_steps_.year_bar = bar - start;
    if (model_->get_selected_view() >= 0 && model_->get_selected_view() == model_->get_territory_view()) {
        show_legend();  // rebuilt by the model for the new year
    }
    const int64_t legend = now_nsec();
    year_steps_.legend = legend - bar;
    if (hover_x_ >= 0 && hover_y_ >= 0) {
        on_cell_hovered(hover_x_, hover_y_);  // its text depends on the year
    }
    year_steps_.hover = now_nsec() - legend;
}

void ViewerScreen::on_view_image_changed(int64_t index) {
    const int64_t start = now_nsec();
    if (index == model_->get_selected_view()) {
        map_view_->set_image(model_->get_view_image(index));  // same size: texture updated, place kept
    }
    year_steps_.image = now_nsec() - start;
}

void ViewerScreen::on_year_slider_changed(double value) {
    if (!updating_year_bar_) {
        model_->set_year(static_cast<int64_t>(std::llround(value)));
    }
}

void ViewerScreen::on_year_field_changed(double value) {
    if (!updating_year_bar_) {
        model_->set_year(static_cast<int64_t>(std::llround(value)));
    }
}

void ViewerScreen::on_step_year(int64_t delta) {
    model_->step_year(delta);
}

void ViewerScreen::on_step_event(int64_t direction) {
    model_->step_event(direction);
}

void ViewerScreen::on_step_change(int64_t direction) {
    model_->step_holder_change(direction);
}

void ViewerScreen::_unhandled_key_input(const godot::Ref<godot::InputEvent>& event) {
    const godot::Ref<godot::InputEventKey> key = event;
    if (!wired_ || key.is_null() || !key->is_pressed() || key->is_ctrl_pressed() || key->is_alt_pressed() ||
        key->is_meta_pressed()) {
        return;
    }
    // P4-05 6.5: B hides or shows the right pane; F follow; Backspace and
    // Shift+Backspace the path.
    if (key->get_keycode() == godot::KEY_B && !key->is_shift_pressed() && !key->is_echo()) {
        right_pane_->set_visible(!right_pane_->is_visible());
        get_viewport()->set_input_as_handled();
        return;
    }
    if (key->get_keycode() == godot::KEY_F && !key->is_shift_pressed() && !key->is_echo()) {
        if (model_->get_browser_available()) {
            model_->set_follow(!model_->get_follow());
            get_viewport()->set_input_as_handled();
        }
        return;
    }
    if (key->get_keycode() == godot::KEY_BACKSPACE) {
        if (model_->get_browser_available()) {
            if (key->is_shift_pressed()) {
                model_->browser_forward();
            } else {
                model_->browser_back();
            }
            get_viewport()->set_input_as_handled();
        }
        return;
    }
    // P4-04 8.2: M toggles the markers (nothing happens when there are none).
    if (key->get_keycode() == godot::KEY_M && !key->is_shift_pressed() && !key->is_echo()) {
        if (model_->get_markers_available()) {
            model_->set_markers_enabled(!model_->get_markers_enabled());
            update_marker_controls();
            get_viewport()->set_input_as_handled();
        }
        return;
    }
    if (!model_->has_history()) {
        return;
    }
    const godot::Key code = key->get_keycode();
    const char32_t unicode = key->get_unicode();
    const bool shift = key->is_shift_pressed();
    bool handled = true;
    if (code == godot::KEY_LEFT) {
        model_->step_year(shift ? -25 : -1);
    } else if (code == godot::KEY_RIGHT) {
        model_->step_year(shift ? 25 : 1);
    } else if (code == godot::KEY_PAGEUP) {
        model_->step_event(-1);
    } else if (code == godot::KEY_PAGEDOWN) {
        model_->step_event(1);
    } else if (code == godot::KEY_BRACKETLEFT || unicode == U'[') {
        model_->step_holder_change(-1);
    } else if (code == godot::KEY_BRACKETRIGHT || unicode == U']') {
        model_->step_holder_change(1);
    } else if (code == godot::KEY_HOME) {
        model_->set_year(0);
    } else if (code == godot::KEY_END) {
        model_->set_year(model_->get_end_year());
    } else {
        handled = false;
    }
    if (handled) {
        get_viewport()->set_input_as_handled();
    }
}

void ViewerScreen::apply_start_year(const godot::String& text) {
    if (!model_->has_history()) {
        show_status("--year " + text + ": this world has no history", true);
        return;
    }
    const int64_t end = model_->get_end_year();
    const godot::String stripped = text.strip_edges();
    // A decimal integer in [0, E]: digits only, at most 18 of them.
    bool digits = !stripped.is_empty() && stripped.length() <= 18;
    for (int64_t i = 0; digits && i < stripped.length(); ++i) {
        digits = stripped[i] >= U'0' && stripped[i] <= U'9';
    }
    const int64_t year = digits ? stripped.to_int() : -1;
    if (!digits || year > end || !model_->set_year(year)) {
        show_status("--year " + text + ": not a year of this world (0 to " + godot::String::num_int64(end) + ")",
                    true);
    }
}

void ViewerScreen::run_bench() {
    const auto usec = [] { return static_cast<int64_t>(godot::Time::get_singleton()->get_ticks_usec()); };
    const auto ms = [](int64_t microseconds) {
        return godot::String::num(static_cast<double>(microseconds) / 1000.0, 2) + " ms";
    };
    if (!wired_ || bench_world_.is_empty()) {
        godot::UtilityFunctions::printerr("UNMAPPED-BENCH: give a world file after --");
        get_tree()->quit(1);
        return;
    }
    godot::UtilityFunctions::print("UNMAPPED-BENCH: world ", bench_world_);
    const int64_t t0 = usec();
    const bool opened = open_world(bench_world_);
    const int64_t open_time = usec() - t0;
    if (!opened) {
        godot::UtilityFunctions::printerr("UNMAPPED-BENCH: cannot open the world: ", model_->get_error_code(), ": ",
                                          model_->get_error_message());
        get_tree()->quit(1);
        return;
    }
    godot::UtilityFunctions::print("UNMAPPED-BENCH: open_world to the first view shown: ", ms(open_time),
                                   " (history ", model_->get_history_status(), ", selected view ",
                                   model_->get_view_info(model_->get_selected_view())["name"], ")");
    if (!model_->has_history()) {
        godot::UtilityFunctions::print("UNMAPPED-BENCH: no history; done");
        get_tree()->quit(0);
        return;
    }
    const auto& timings = model_->territory_timings();
    godot::UtilityFunctions::print("UNMAPPED-BENCH: first territory image (base and image): ",
                                   ms(timings.first_image_usec));
    // P4-05 7: the browser.
    if (model_->get_browser_available()) {
        godot::UtilityFunctions::print("UNMAPPED-BENCH: browser index and row texts at open: ",
                                       ms(model_->browser_state().load_usec()), " (",
                                       static_cast<int64_t>(model_->browser_state().index().events().size()),
                                       " events, ", static_cast<int64_t>(model_->browser_state().index().civilizations().size()),
                                       " civilizations)");
        bench_browser_filters();
        // The year changes below run with the first civilization selected and
        // follow on, as in the walkthrough.
        model_->select_civilization(model_->get_civilization_list().is_empty()
                                        ? int64_t{0}
                                        : int64_t(godot::Dictionary(model_->get_civilization_list()[0])["id"]));
        model_->set_year(model_->get_end_year());
        model_->reset_browser_timings();
    }
    const int64_t redraws_before = timings.redraws;
    const ViewerModel::MarkerTimings markers_before = model_->marker_timings();
    const int64_t tiles_before = map_view_->tiles_uploaded();
    const int64_t end = model_->get_end_year();
    // P4-05 12.3: each step of a year change, its maximum (and the year of
    // it) and mean over 0..E and back.
    struct Stat {
        const char* name;
        int64_t max = 0;
        int64_t max_year = -1;
        int64_t total = 0;
    };
    std::array<Stat, 14> stats{{{"(a) year_state and holders"},
                                {"(b) render_territory into the image"},
                                {"(c) view_image_changed: texture update"},
                                {"(d) marker step (markers_at, compare, order)"},
                                {"(d) markers_changed handler (map view, key list)"},
                                {"(e) model legend (legend, legend_array)"},
                                {"(f) screen legend rebuild (show_legend)"},
                                {"(g) year bar (labels, buttons)"},
                                {"(h) other year_changed work (hover text)"},
                                {"model part (a + b + d + e)"},
                                {"whole set_year"},
                                {"(i) browser: highlight, This year list, mask"},
                                {"(j) browser: highlight_changed handler (mask upload, frames)"},
                                {"(k) browser: event_list_changed handler (list rebuild)"}}};
    int64_t changes = 0;
    const auto change = [&](int64_t year) {
        year_steps_ = {};
        model_->set_year(year);
        const ViewerModel::YearChangeSteps& m = model_->last_year_change();
        const YearSteps& v = year_steps_;
        const int64_t other = std::max<int64_t>(0, m.year_signal - v.legend - v.year_bar);
        const std::array<int64_t, 14> values{m.state,   m.draw,   v.image,    m.markers,
                                             v.markers, m.legend, v.legend,   v.year_bar,
                                             other,     m.state + m.draw + m.markers + m.legend, m.total,
                                             m.browser, v.highlight, v.list};
        for (std::size_t i = 0; i < stats.size(); ++i) {
            stats[i].total += values[i];
            if (values[i] > stats[i].max) {
                stats[i].max = values[i];
                stats[i].max_year = year;
            }
        }
        ++changes;
    };
    for (int64_t y = 0; y <= end; ++y) {
        change(y);
    }
    for (int64_t y = end; y >= 0; --y) {
        change(y);
    }
    const auto nms = [](int64_t nsec, int digits) {
        return godot::String::num(static_cast<double>(nsec) / 1000000.0, digits) + " ms";
    };
    const auto change_years =
        static_cast<int64_t>(model_->history_state().history().timeline.holder_change_years().size());
    const Stat& whole = stats[10];  // the whole set_year
    godot::UtilityFunctions::print("UNMAPPED-BENCH: year changes 0..", end, " and back: ", changes, ", mean ",
                                   nms(whole.total / std::max<int64_t>(changes, 1), 3), ", slowest ",
                                   nms(whole.max, 2), " (to year ", whole.max_year, ")");
    for (const Stat& stat : stats) {
        godot::UtilityFunctions::print("UNMAPPED-BENCH:   step ", stat.name, ": max ", nms(stat.max, 3),
                                       " (to year ", stat.max_year, "), mean ",
                                       nms(stat.total / std::max<int64_t>(changes, 1), 4));
    }
    godot::UtilityFunctions::print("UNMAPPED-BENCH: territory redraws ", timings.redraws - redraws_before,
                                   " (holder change years: ", change_years, " per pass), slowest redraw ",
                                   ms(timings.slowest_redraw_usec), "; map tiles uploaded ",
                                   map_view_->tiles_uploaded() - tiles_before, " of ",
                                   static_cast<int64_t>(map_view_->tile_count()), " per image");
    // P4-04 8.4: the marker step of each year change, and markers_changed.
    const ViewerModel::MarkerTimings& markers = model_->marker_timings();
    const int64_t steps = markers.steps - markers_before.steps;
    godot::UtilityFunctions::print(
        "UNMAPPED-BENCH: marker steps ", steps, ", mean ",
        godot::String::num(steps > 0 ? static_cast<double>(markers.total_nsec - markers_before.total_nsec) / 1000.0 /
                                           static_cast<double>(steps) / 1000.0
                                     : 0.0,
                           4),
        " ms, slowest ", godot::String::num(static_cast<double>(markers.slowest_nsec) / 1000000.0, 3),
        " ms; markers_changed ", markers.changes - markers_before.changes, " over 0..", end, " and back");
    model_->set_year(end);
    godot::UtilityFunctions::print("UNMAPPED-BENCH: markers at the present: ", model_->get_markers().size());
    if (model_->get_browser_available()) {
        const ViewerModel::BrowserTimings& b = model_->browser_timings();
        godot::UtilityFunctions::print(
            "UNMAPPED-BENCH: browser over 0..", end, " and back with c",
            model_->get_selection().get("entity_id", int64_t{0}), " selected and follow ",
            model_->get_follow() ? "on" : "off", ": event_list_changed ", b.list_changes, ", highlight_changed ",
            b.highlight_changes, ", mask rebuilds ", b.mask_rebuilds, ", slowest mask rebuild (model) ",
            nms(b.slowest_mask_nsec, 3), ", slowest browser part of a year change ", nms(b.slowest_year_nsec, 3));
    }
    if (godot::DisplayServer::get_singleton()->get_name() == "headless") {
        godot::UtilityFunctions::print("UNMAPPED-BENCH: frame times: headless, no frame is drawn");
        get_tree()->create_timer(0.3)->connect("timeout", callable_mp(this, &ViewerScreen::finish_bench));
        return;
    }
    // The windowed pass: one year change per frame, timed from set_year to
    // the next frame_post_draw, with vsync off so that no frame is held back:
    // each step's slowest year (arriving from the year before), every 25th
    // year, and each holder change year and the year after it.
    godot::DisplayServer::get_singleton()->window_set_vsync_mode(godot::DisplayServer::VSYNC_DISABLED);
    godot::Engine::get_singleton()->set_max_fps(0);
    godot::OS::get_singleton()->set_low_processor_usage_mode(false);
    bench_frame_years_.clear();
    for (const Stat& stat : stats) {
        if (stat.max_year >= 0) {
            bench_frame_years_.push_back(stat.max_year > 0 ? stat.max_year - 1 : 1);
            bench_frame_years_.push_back(stat.max_year);
        }
    }
    for (int64_t y = 0; y <= end; y += 25) {
        bench_frame_years_.push_back(y);
    }
    for (const int64_t y : model_->history_state().history().timeline.holder_change_years()) {
        bench_frame_years_.push_back(y);
        if (y + 1 <= end) {
            bench_frame_years_.push_back(y + 1);
        }
    }
    bench_frame_next_ = 0;
    bench_frame_start_ = -1;
    bench_frame_idle_ = 30;
    bench_frame_times_.clear();
    godot::RenderingServer::get_singleton()->connect("frame_post_draw",
                                                     callable_mp(this, &ViewerScreen::bench_frame_step));
}

void ViewerScreen::bench_frame_step() {
    const int64_t now = now_nsec();
    if (bench_frame_start_ >= 0) {
        bench_frame_times_.emplace_back(bench_frame_year_, now - bench_frame_start_);
    }
    // First some frames without a year change, for the frame's own cost.
    if (bench_frame_idle_ > 0) {
        --bench_frame_idle_;
        bench_frame_year_ = -1;
        bench_frame_start_ = now_nsec();
        map_view_->queue_redraw();
        return;
    }
    if (bench_frame_next_ >= bench_frame_years_.size()) {
        godot::RenderingServer::get_singleton()->disconnect("frame_post_draw",
                                                            callable_mp(this, &ViewerScreen::bench_frame_step));
        bench_frame_start_ = -1;
        finish_bench();
        return;
    }
    bench_frame_year_ = bench_frame_years_[bench_frame_next_++];
    if (bench_frame_year_ == model_->get_year()) {
        model_->set_year(bench_frame_year_ == 0 ? 1 : bench_frame_year_ - 1);  // untimed: a change must happen
    }
    bench_frame_start_ = now_nsec();
    model_->set_year(bench_frame_year_);
}

void ViewerScreen::finish_bench() {
    if (!bench_frame_times_.empty()) {
        int64_t idle_max = 0;
        int64_t idle_total = 0;
        int64_t idle_n = 0;
        int64_t max = 0;
        int64_t max_year = -1;
        int64_t total = 0;
        int64_t n = 0;
        int64_t over = 0;
        bool first_idle = true;
        for (const auto& [year, nsec] : bench_frame_times_) {
            if (year < 0) {
                // The first frame follows the synchronous pass: left out.
                if (!first_idle) {
                    ++idle_n;
                    idle_total += nsec;
                    idle_max = std::max(idle_max, nsec);
                }
                first_idle = false;
                continue;
            }
            ++n;
            total += nsec;
            over += nsec > 50000000 ? 1 : 0;
            if (nsec > max) {
                max = nsec;
                max_year = year;
            }
        }
        const auto nms = [](int64_t nsec) {
            return godot::String::num(static_cast<double>(nsec) / 1000000.0, 2) + " ms";
        };
        godot::UtilityFunctions::print("UNMAPPED-BENCH: frame without a year change: mean ",
                                       nms(idle_total / std::max<int64_t>(idle_n, 1)), ", max ", nms(idle_max), " (",
                                       idle_n, " frames)");
        godot::UtilityFunctions::print("UNMAPPED-BENCH: set_year to the next frame_post_draw: ", n,
                                       " year changes, mean ", nms(total / std::max<int64_t>(n, 1)), ", max ",
                                       nms(max), " (to year ", max_year, "), over 50 ms: ", over);
    }
    if (map_view_->marker_draws() > 0) {
        godot::UtilityFunctions::print("UNMAPPED-BENCH: marker draw (", model_->get_markers().size(), " markers, ",
                                       static_cast<int64_t>(map_view_->marker_texture_count()), " textures): ",
                                       godot::String::num(static_cast<double>(map_view_->last_marker_draw_usec()) /
                                                              1000.0,
                                                          3),
                                       " ms (", map_view_->marker_draws(), " draws)");
    } else {
        godot::UtilityFunctions::print("UNMAPPED-BENCH: marker draw: the map view was not drawn (headless)");
    }
    get_tree()->quit(0);
}

void ViewerScreen::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("get_model"), &ViewerScreen::get_model);
    ClassDB::bind_method(D_METHOD("open_world", "path"), &ViewerScreen::open_world);
    ClassDB::bind_method(D_METHOD("choose_view", "index"), &ViewerScreen::choose_view);
    ClassDB::bind_method(D_METHOD("hover_cell", "x", "y"), &ViewerScreen::hover_cell);
}

}  // namespace unmapped
