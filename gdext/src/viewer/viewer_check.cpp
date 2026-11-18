#include "viewer/viewer_check.hpp"

#include <cstdint>

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/tree_item.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "viewer/viewer_screen.hpp"
#include "world_file.hpp"

namespace unmapped {

namespace {

class Checker {
public:
    void expect(bool ok, const godot::String& what) {
        godot::UtilityFunctions::print(godot::String(ok ? "  ok    " : "  FAIL  ") + what);
        if (!ok) {
            failed_ = true;
        }
    }
    [[nodiscard]] bool failed() const { return failed_; }

private:
    bool failed_ = false;
};

int64_t tree_row_count(godot::Tree* tree) {
    godot::TreeItem* root = tree->get_root();
    return root == nullptr ? 0 : root->get_child_count();
}

// Compares the viewer's image with the PNG that worldgen-cli wrote.
godot::String compare_images(const godot::Ref<godot::Image>& shown, const godot::String& png_path) {
    if (shown.is_null()) {
        return "the map view has no image";
    }
    godot::Ref<godot::Image> reference = godot::Image::load_from_file(png_path);
    if (reference.is_null()) {
        return "cannot load " + png_path;
    }
    if (reference->get_format() != godot::Image::FORMAT_RGB8) {
        reference->convert(godot::Image::FORMAT_RGB8);
    }
    if (shown->get_format() != godot::Image::FORMAT_RGB8) {
        return "the map view's image is not RGB8";
    }
    if (shown->get_size() != reference->get_size()) {
        return godot::String("size {0} differs from the reference {1}")
            .format(godot::Array::make(shown->get_size(), reference->get_size()));
    }
    const godot::PackedByteArray a = shown->get_data();
    const godot::PackedByteArray b = reference->get_data();
    if (a.size() != b.size()) {
        return "pixel buffers differ in size";
    }
    for (int64_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            const int64_t pixel = i / 3;
            const int64_t width = shown->get_width();
            return godot::String("first difference at pixel ({0}, {1}), channel {2}: {3} vs reference {4}")
                .format(godot::Array::make(pixel % width, pixel / width, i % 3, a[i], b[i]));
        }
    }
    return {};
}

// Pixels of an RGB8 image equal to (r, g, b); -1 without an RGB8 image.
int64_t count_rgb(const godot::Ref<godot::Image>& image, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (image.is_null() || image->get_format() != godot::Image::FORMAT_RGB8) {
        return -1;
    }
    const godot::PackedByteArray data = image->get_data();
    const std::uint8_t* bytes = data.ptr();
    int64_t count = 0;
    for (int64_t i = 0; i + 2 < data.size(); i += 3) {
        if (bytes[i] == r && bytes[i + 1] == g && bytes[i + 2] == b) {
            ++count;
        }
    }
    return count;
}

}  // namespace

int run_viewer_check(ViewerScreen& screen, const ViewerCheckOptions& options) {
    Checker check;
    godot::UtilityFunctions::print("UNMAPPED-CHECK: viewer headless check, Godot ",
                                   godot::Engine::get_singleton()->get_version_info()["string"]);

    check.expect(options.wired, "viewer scene has all its parts (%OpenButton ... %StatusLabel)");
    if (!options.wired) {
        godot::UtilityFunctions::print("UNMAPPED-CHECK: FAIL");
        return 1;
    }
    godot::Ref<ViewerModel> model = screen.get_model();
    check.expect(model->has_world(), "world opened: " + model->get_world_path() +
                                         (model->has_world() ? godot::String()
                                                             : " (" + model->get_error_code() + ": " +
                                                                   model->get_error_message() + ")"));
    if (!model->has_world()) {
        godot::UtilityFunctions::print("UNMAPPED-CHECK: FAIL");
        return 1;
    }

    const int64_t layer_count = model->get_layer_count();
    const int64_t composite_count = model->get_composite_count();
    const int64_t count = model->get_view_count();
    const godot::Ref<WorldFile> world = model->get_world();
    check.expect(layer_count == world->get_layer_count(),
                 "model lists every layer of the file: " + godot::String(", ").join(world->get_layer_names()));
    check.expect(composite_count == world->get_composite_names().size() && count == layer_count + composite_count,
                 "model lists every composite: " + godot::String(", ").join(world->get_composite_names()));
    if (options.expect_layers >= 0) {
        check.expect(layer_count == options.expect_layers,
                     godot::String("layer count {0}, expected {1}")
                         .format(godot::Array::make(layer_count, options.expect_layers)));
    }
    if (options.expect_composites >= 0) {
        check.expect(composite_count == options.expect_composites,
                     godot::String("composite count {0}, expected {1}")
                         .format(godot::Array::make(composite_count, options.expect_composites)));
    }
    check.expect(screen.layer_list()->get_item_count() == count,
                 godot::String("layer list shows {0} views")
                     .format(godot::Array::make(screen.layer_list()->get_item_count())));
    if (world->find_layer("biome") >= 0) {
        check.expect(world->get_biome_table_problem().is_empty() && world->get_biome_entries().size() > 0,
                     godot::String("table biome read: {0} rows{1}")
                         .format(godot::Array::make(world->get_biome_entries().size(),
                                                    world->get_biome_table_problem().is_empty()
                                                        ? godot::String()
                                                        : " (" + world->get_biome_table_problem() + ")")));
    }
    const int64_t meta_count = model->get_meta_entries().size();
    check.expect(meta_count > 0 && tree_row_count(screen.meta_tree()) == meta_count,
                 godot::String("meta panel shows {0} of {1} meta rows")
                     .format(godot::Array::make(tree_row_count(screen.meta_tree()), meta_count)));

    for (int64_t i = 0; i < count; ++i) {
        const bool is_layer = i < layer_count;
        const godot::String name = model->get_view_info(i)["name"];
        screen.choose_view(i);
        const bool selected = model->get_selected_view() == i;
        const godot::String kind = is_layer ? "layer" : "composite";
        check.expect(selected, kind + godot::String(" ") + name + godot::String(" selected through the layer list"));
        if (!selected) {
            continue;
        }
        // The image is drawn inside the map view, fitted (not off-screen).
        const godot::Rect2 shown = screen.map_view()->get_shown_rect();
        const godot::Vector2 view = screen.map_view()->get_size();
        const bool inside = shown.size.x > 0.0F && shown.size.y > 0.0F &&
                            godot::Rect2(godot::Vector2(), view).grow(0.5F).encloses(shown) &&
                            (shown.size.x >= view.x - 1.0F || shown.size.y >= view.y - 1.0F);
        check.expect(inside, godot::String("{0} {1} drawn fitted inside the map view: {2} in {3}")
                                 .format(godot::Array::make(kind, name, shown, view)));
        const godot::Dictionary info = model->get_view_info(i);  // palette known once decoded
        const godot::String png =
            options.reference_dir.path_join((is_layer ? name : "composite." + name) + godot::String(".png"));
        const godot::String difference = compare_images(screen.map_view()->get_image(), png);
        check.expect(difference.is_empty(),
                     godot::String("{0} {1} ({2} x {3} {4}, palette {5}): map view pixels equal worldgen-cli render{6}")
                         .format(godot::Array::make(kind, name, info["width"], info["height"], info["element_type"],
                                                    info["palette"],
                                                    difference.is_empty() ? godot::String() : ": " + difference)));
        if (info["palette"] == godot::Variant("biome")) {
            // Both sides magenta would also compare equal: the colour of a
            // biome code missing from the table (P1-00 10.2).
            const int64_t unknown = count_rgb(screen.map_view()->get_image(), 255, 0, 255);
            check.expect(unknown == 0, godot::String("layer {0}: {1} cells in the unknown-biome colour (255, 0, 255)")
                                           .format(godot::Array::make(name, unknown)));
        }
    }

    // The reader's distinct errors reach Godot as code and message.
    godot::Ref<WorldFile> probe;
    probe.instantiate();
    // (Each open() is a statement of its own: the order in which function
    // arguments are evaluated is unspecified.)
    const auto expect_open_error = [&](const godot::String& path, const char* expected_code, const char* what) {
        const bool opened = probe->open(path);
        const godot::String code = probe->get_error_code();
        const godot::String message = probe->get_error_message();
        check.expect(!opened && code == expected_code && !message.is_empty(),
                     godot::String(what) + ": " + code + ": " + message);
    };
    expect_open_error(options.reference_dir.path_join("no-such-world.db"), "file_unreadable", "missing file");
    if (layer_count > 0) {
        const godot::String layer_name = model->get_view_info(0)["name"];
        expect_open_error(options.reference_dir.path_join(layer_name + godot::String(".png")), "not_sqlite",
                          "PNG file opened as a world");
    }

    godot::UtilityFunctions::print(check.failed() ? "UNMAPPED-CHECK: FAIL" : "UNMAPPED-CHECK: PASS");
    return check.failed() ? 1 : 0;
}

}  // namespace unmapped
