#include "viewer/viewer_screen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
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
    wired_ = open_button_ != nullptr && open_dialog_ != nullptr && file_label_ != nullptr && layer_list_ != nullptr &&
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
        set_process_unhandled_key_input(true);
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
        get_tree()->create_timer(1.0)->connect("timeout", callable_mp(this, &ViewerScreen::take_screenshot));
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
    const godot::String text = x < 0 || y < 0 ? godot::String() : model_->describe_cell(x, y);
    cell_label_->set_text(text.is_empty() ? godot::String("Point at the map to inspect a cell.") : text);
}

void ViewerScreen::on_world_opened(const godot::String& path) {
    // P4-03 4.6: a history that could not be read is named under the path.
    const godot::String file_text =
        model_->get_history_status() == "failed"
            ? path + godot::String("\nhistory: ") + model_->get_history_error_code() + godot::String(": ") + model_->get_history_error_message()
            : path;
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
    layer_list_->clear();
    item_view_.clear();
    view_item_.clear();
    legend_list_->clear();
    legend_title_->set_text("");
    cell_label_->set_text(" ");
    meta_tree_->clear();
    map_view_->clear_image();
    hover_x_ = -1;
    hover_y_ = -1;
    update_year_bar();
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
    show_status(view_tooltip(info).replace("\n", "   "), false);
    show_legend();
}

void ViewerScreen::show_legend() {
    legend_list_->clear();
    legend_title_->set_text(model_->get_legend_title());
    const godot::Array legend = model_->get_legend();
    for (int64_t i = 0; i < legend.size(); ++i) {
        const godot::Dictionary entry = legend[i];
        const godot::PackedInt32Array colours = entry["colours"];
        // A row without colours is a heading: no swatch (P4-03 6.4).
        const godot::Ref<godot::Texture2D> icon =
            colours.is_empty() ? godot::Ref<godot::Texture2D>() : godot::Ref<godot::Texture2D>(legend_icon(colours));
        const int32_t item = legend_list_->add_item(entry["label"], icon, false);
        legend_list_->set_item_tooltip_enabled(item, false);
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

void ViewerScreen::update_year_bar() {
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
    year_marks_->set_marks(model_->get_year_marks(), history ? end : 0);
}

void ViewerScreen::on_year_changed(int64_t /*year*/) {
    update_year_bar();
    if (model_->get_selected_view() >= 0 && model_->get_selected_view() == model_->get_territory_view()) {
        show_legend();  // rebuilt by the model for the new year
    }
    if (hover_x_ >= 0 && hover_y_ >= 0) {
        on_cell_hovered(hover_x_, hover_y_);  // its text depends on the year
    }
}

void ViewerScreen::on_view_image_changed(int64_t index) {
    if (index == model_->get_selected_view()) {
        map_view_->set_image(model_->get_view_image(index));  // same size: texture updated, place kept
    }
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
    if (!wired_ || key.is_null() || !key->is_pressed() || !model_->has_history() || key->is_ctrl_pressed() ||
        key->is_alt_pressed() || key->is_meta_pressed()) {
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
    const int64_t redraws_before = timings.redraws;
    const int64_t end = model_->get_end_year();
    int64_t changes = 0;
    int64_t total = 0;
    int64_t slowest = 0;
    int64_t slowest_year = -1;
    const auto change = [&](int64_t year) {
        const int64_t t = usec();
        model_->set_year(year);
        const int64_t took = usec() - t;
        ++changes;
        total += took;
        if (took > slowest) {
            slowest = took;
            slowest_year = year;
        }
    };
    for (int64_t y = 0; y <= end; ++y) {
        change(y);
    }
    for (int64_t y = end; y >= 0; --y) {
        change(y);
    }
    const auto change_years =
        static_cast<int64_t>(model_->history_state().history().timeline.holder_change_years().size());
    godot::UtilityFunctions::print("UNMAPPED-BENCH: year changes 0..", end, " and back: ", changes, ", mean ",
                                   godot::String::num(static_cast<double>(total) / static_cast<double>(changes) / 1000.0,
                                                      3),
                                   " ms, slowest ", ms(slowest), " (to year ", slowest_year, ")");
    godot::UtilityFunctions::print("UNMAPPED-BENCH: territory redraws ", timings.redraws - redraws_before,
                                   " (holder change years: ", change_years, " per pass), slowest redraw ",
                                   ms(timings.slowest_redraw_usec));
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
