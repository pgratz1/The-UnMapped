#include "viewer/viewer_screen.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
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

#include "viewer/layer_catalogue.hpp"
#include "viewer/viewer_check.hpp"

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
    wired_ = open_button_ != nullptr && open_dialog_ != nullptr && file_label_ != nullptr && layer_list_ != nullptr &&
             meta_tree_ != nullptr && map_view_ != nullptr && status_label_ != nullptr && cell_label_ != nullptr &&
             legend_title_ != nullptr && legend_list_ != nullptr;

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
    }

    // Arguments after `--` on the Godot command line.
    const godot::PackedStringArray args = godot::OS::get_singleton()->get_cmdline_user_args();
    godot::String world_path;
    godot::String start_view;
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

    if (!world_path.is_empty() && wired_) {
        if (open_world(world_path) && !start_view.is_empty()) {
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
    options.wired = wired_;
    const int exit_code = run_viewer_check(*this, options);
    get_tree()->quit(exit_code);
}

godot::Ref<ViewerModel> ViewerScreen::get_model() const {
    return model_;
}

bool ViewerScreen::open_world(const godot::String& path) {
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
    const godot::String text = x < 0 || y < 0 ? godot::String() : model_->describe_cell(x, y);
    cell_label_->set_text(text.is_empty() ? godot::String("Point at the map to inspect a cell.") : text);
}

void ViewerScreen::on_world_opened(const godot::String& path) {
    file_label_->set_text(path);
    file_label_->set_tooltip_text(path);

    layer_list_->clear();
    item_view_.clear();
    const int64_t count = model_->get_view_count();
    view_item_.assign(static_cast<std::size_t>(count), -1);
    // Group by the model's group titles, in the catalogue's group order;
    // within a group, in view order (layer id order).
    for (std::size_t g = 0; g < unmapped::catalogue::GROUP_COUNT; ++g) {
        const godot::String title = godot::String::utf8(
            unmapped::catalogue::group_title(static_cast<unmapped::catalogue::Group>(g)).data());
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
        const int32_t item = legend_list_->add_item(entry["label"], legend_icon(entry["colours"]), false);
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

void ViewerScreen::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("get_model"), &ViewerScreen::get_model);
    ClassDB::bind_method(D_METHOD("open_world", "path"), &ViewerScreen::open_world);
    ClassDB::bind_method(D_METHOD("choose_view", "index"), &ViewerScreen::choose_view);
    ClassDB::bind_method(D_METHOD("hover_cell", "x", "y"), &ViewerScreen::hover_cell);
}

}  // namespace unmapped
