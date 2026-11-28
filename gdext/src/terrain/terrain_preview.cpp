#include "terrain/terrain_preview.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/class_db_singleton.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/performance.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/scene_tree_timer.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "terrain/placeholder_meshes.hpp"
#include "terrain/terrain_check.hpp"
#include "world_layer.hpp"

namespace unmapped {

namespace {

std::string to_utf8(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data(), static_cast<std::size_t>(utf8.length())};
}

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.c_str(), static_cast<int64_t>(text.size()));
}

std::uint64_t now_usec() {
    return godot::Time::get_singleton()->get_ticks_usec();
}

godot::String ms(std::uint64_t usec) {
    return to_godot(terrain::one_decimal(static_cast<double>(usec) / 1000.0));
}

bool is_headless() {
    return godot::DisplayServer::get_singleton()->get_name() == "headless";
}

// A flat 64 x 64 RGBA8 image with mipmaps (5.3: texture asset 0).
godot::Ref<godot::ImageTexture> flat_texture(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    const godot::Ref<godot::Image> image = godot::Image::create_empty(64, 64, false, godot::Image::FORMAT_RGBA8);
    image->fill(godot::Color(static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
                             static_cast<float>(b) / 255.0f, static_cast<float>(a) / 255.0f));
    image->generate_mipmaps();
    return godot::ImageTexture::create_from_image(image);
}

godot::Color rgb_colour(std::uint32_t rgb) {
    return godot::Color(static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f,
                        static_cast<float>(rgb & 0xFF) / 255.0f);
}

// An RGBA8 256 x 256 image with mipmaps from bytes (V3D-02 3.2).
godot::Ref<godot::ImageTexture> rgba_texture(const std::vector<std::uint8_t>& bytes, int size) {
    godot::PackedByteArray data;
    data.resize(static_cast<int64_t>(bytes.size()));
    std::memcpy(data.ptrw(), bytes.data(), bytes.size());
    const godot::Ref<godot::Image> image = godot::Image::create_from_data(size, size, false, godot::Image::FORMAT_RGBA8, data);
    image->generate_mipmaps();
    return godot::ImageTexture::create_from_image(image);
}

godot::Color srgb_byte_colour(std::uint32_t rgba) {
    return godot::Color(static_cast<float>((rgba >> 24) & 0xFF) / 255.0f, static_cast<float>((rgba >> 16) & 0xFF) / 255.0f,
                        static_cast<float>((rgba >> 8) & 0xFF) / 255.0f, static_cast<float>(rgba & 0xFF) / 255.0f);
}

// A PackedScene of one vegetation kind built in code (V3D-02 9.1 step 3): a
// Node3D root with one MeshInstance3D child per LOD, MeshLOD0 and MeshLOD1,
// owned by the root.
godot::Ref<godot::PackedScene> vegetation_scene(int kind, const godot::Ref<godot::StandardMaterial3D>& material,
                                                 std::array<godot::Ref<godot::ArrayMesh>, 2>& meshes) {
    godot::Node3D* root = memnew(godot::Node3D);
    root->set_name(terrain::VEGETATION_KINDS_INFO[static_cast<std::size_t>(kind)].name);
    for (int lod = 0; lod < terrain::lod_count(kind); ++lod) {
        const terrain::MeshData data = terrain::build_mesh(kind, lod);
        godot::PackedVector3Array vertices;
        godot::PackedVector3Array normals;
        godot::PackedColorArray colours;
        const std::size_t count = data.colours.size();
        vertices.resize(static_cast<int64_t>(count));
        normals.resize(static_cast<int64_t>(count));
        colours.resize(static_cast<int64_t>(count));
        for (std::size_t v = 0; v < count; ++v) {
            const auto i = static_cast<int64_t>(v);
            vertices.set(i, godot::Vector3(data.positions[3 * v], data.positions[3 * v + 1], data.positions[3 * v + 2]));
            normals.set(i, godot::Vector3(data.normals[3 * v], data.normals[3 * v + 1], data.normals[3 * v + 2]));
            colours.set(i, srgb_byte_colour(data.colours[v]));
        }
        godot::Array arrays;
        arrays.resize(godot::Mesh::ARRAY_MAX);
        arrays[godot::Mesh::ARRAY_VERTEX] = vertices;
        arrays[godot::Mesh::ARRAY_NORMAL] = normals;
        arrays[godot::Mesh::ARRAY_COLOR] = colours;
        godot::Ref<godot::ArrayMesh> mesh;
        mesh.instantiate();
        mesh->add_surface_from_arrays(godot::Mesh::PRIMITIVE_TRIANGLES, arrays);
        mesh->surface_set_material(0, material);
        meshes[static_cast<std::size_t>(lod)] = mesh;
        godot::MeshInstance3D* instance = memnew(godot::MeshInstance3D);
        instance->set_name(godot::String("MeshLOD") + godot::String::num_int64(lod));
        instance->set_mesh(mesh);
        root->add_child(instance);
        instance->set_owner(root);
    }
    godot::Ref<godot::PackedScene> scene;
    scene.instantiate();
    scene->pack(root);
    memdelete(root);
    return scene;
}

constexpr float MOUSE_DEG_PER_PIXEL = 0.15f;
constexpr float ARROW_YAW_DEG_PER_S = 90.0f;
constexpr float ARROW_PITCH_DEG_PER_S = 60.0f;

template <class T>
T* find_part(godot::Node* owner, const char* name) {
    godot::Node* node = owner->get_node_or_null(godot::NodePath(godot::String("%") + name));
    T* part = godot::Object::cast_to<T>(node);
    if (part == nullptr) {
        godot::UtilityFunctions::printerr("TerrainPreview: the scene has no node %", name, " of the expected type");
    }
    return part;
}

}  // namespace

void TerrainPreview::_bind_methods() {
    godot::ClassDB::bind_method(godot::D_METHOD("open_world", "path"), &TerrainPreview::open_world);
    godot::ClassDB::bind_method(godot::D_METHOD("get_status_text"), &TerrainPreview::status_text);
}

godot::String TerrainPreview::status_text() const {
    godot::String text;
    for (std::size_t i = 0; i < status_lines_.size(); ++i) {
        if (i > 0) {
            text += "\n";
        }
        text += status_lines_[i];
    }
    return text;
}

TerrainPreview::~TerrainPreview() {
    // _exit_tree has waited for every task; this is a last guard for a
    // preview freed outside the tree.
    for (NearSlot& slot : near_slots_) {
        if (slot.busy) {
            godot::WorkerThreadPool::get_singleton()->wait_for_task_completion(slot.task);
            slot.busy = false;
        }
    }
}

void TerrainPreview::_exit_tree() {
    // V3D-03 7.1: every worker task is finished before the scene exits.
    check_run_.reset();
    exiting_ = true;
    near_close();
}

void TerrainPreview::_ready() {
    if (godot::Engine::get_singleton()->is_editor_hint()) {
        set_process(false);
        set_process_unhandled_input(false);
        return;
    }
    camera_ = find_part<godot::Camera3D>(this, "Camera");
    sea_ = find_part<godot::MeshInstance3D>(this, "Sea");
    hud_ = find_part<godot::Label>(this, "Hud");
    status_ = find_part<godot::Label>(this, "Status");
    open_button_ = find_part<godot::Button>(this, "OpenButton");
    open_dialog_ = find_part<godot::FileDialog>(this, "OpenDialog");
    if (camera_ == nullptr || sea_ == nullptr || hud_ == nullptr || status_ == nullptr || open_button_ == nullptr ||
        open_dialog_ == nullptr) {
        get_tree()->quit(1);
        return;
    }
    open_button_->connect("pressed", callable_mp(this, &TerrainPreview::on_open_pressed));
    open_dialog_->connect("file_selected", callable_mp(this, &TerrainPreview::on_file_selected));
    camera_->set_near(1.0f);

    parse_options(godot::OS::get_singleton()->get_cmdline_user_args());
    if (refused_) {
        show_status();
        if (scripted_) {
            get_tree()->quit(1);
        }
        return;
    }
    if (world_path_.is_empty()) {
        status_lines_ = {NO_WORLD_TEXT};
        show_status();
        if (scripted_) {
            godot::UtilityFunctions::printerr(NO_WORLD_TEXT);
            get_tree()->quit(1);
        }
        return;
    }
    const bool shown = open_world(world_path_);
    if (refused_ && scripted_) {
        get_tree()->quit(1);
        return;
    }
    if (!check_dir_.is_empty()) {
        // The check runs even when nothing could be shown (T5); after a few
        // frames, once Terrain3D has processed its first frame.
        get_tree()->create_timer(0.25)->connect("timeout", callable_mp(this, &TerrainPreview::run_check_deferred));
        return;
    }
    if (!shown && scripted_) {
        godot::UtilityFunctions::printerr(status_text());
        get_tree()->quit(1);
    }
}

void TerrainPreview::refuse(const godot::String& text) {
    refused_ = true;
    status_lines_.push_back(text);
    godot::UtilityFunctions::printerr(text);
}

void TerrainPreview::parse_options(const godot::PackedStringArray& args) {
    auto number = [](const godot::String& text, float& out) {
        if (!text.is_valid_float()) {
            return false;
        }
        out = static_cast<float>(text.to_float());
        return std::isfinite(out);
    };
    godot::String size_w;
    godot::String size_h;
    for (int64_t i = 0; i < args.size(); ++i) {
        const godot::String arg = args[i];
        const bool has_one = i + 1 < args.size();
        if (arg == "--at") {
            if (i + 2 >= args.size() || !args[i + 1].is_valid_int() || !args[i + 2].is_valid_int()) {
                refuse("--at needs two cells");
                i = std::min<int64_t>(i + 2, args.size());
                continue;
            }
            opt_at_ = std::make_pair(static_cast<int>(args[i + 1].to_int()), static_cast<int>(args[i + 2].to_int()));
            i += 2;
        } else if (arg == "--alt" && has_one) {
            float v = 0.0f;
            const godot::String text = args[++i];
            if (number(text, v)) {
                opt_alt_ = v;
            } else {
                refuse("--alt " + text + ": not a number");
            }
        } else if (arg == "--heading" && has_one) {
            float v = 0.0f;
            const godot::String text = args[++i];
            if (number(text, v)) {
                opt_heading_ = v;
            } else {
                refuse("--heading " + text + ": not a number");
            }
        } else if (arg == "--pitch" && has_one) {
            float v = 0.0f;
            const godot::String text = args[++i];
            if (number(text, v) && v >= -89.0f && v <= 89.0f) {
                opt_pitch_ = v;
            } else {
                refuse("--pitch " + text + ": not between -89 and 89");
            }
        } else if (arg == "--speed" && has_one) {
            const godot::String text = args[++i];
            float v = 0.0f;
            std::optional<int> index;
            if (number(text, v)) {
                for (std::size_t k = 0; k < terrain::SPEEDS.size(); ++k) {
                    if (terrain::SPEEDS[k] == v) {
                        index = static_cast<int>(k);
                    }
                }
            }
            if (index) {
                opt_speed_index_ = index;
            } else {
                refuse("--speed " + text + ": not one of 10, 20, 50, 100, 200, 500, 1000, 2000");
            }
        } else if (arg == "--size" && i + 2 < args.size()) {
            size_w = args[i + 1];
            size_h = args[i + 2];
            i += 2;
        } else if (arg == "--screenshot" && has_one) {
            screenshot_path_ = args[++i];
        } else if (arg == "--bench") {
            bench_ = true;
        } else if (arg == "--check" && has_one) {
            check_dir_ = args[++i];
        } else if (arg == "--expect-terrain3d" && has_one) {
            expect_terrain3d_ = args[++i];
            if (expect_terrain3d_ != "present" && expect_terrain3d_ != "absent") {
                refuse("--expect-terrain3d " + expect_terrain3d_ + ": present or absent");
            }
        } else if (arg == "--no-terrain3d") {
            no_terrain3d_ = true;
        } else if (arg == "--flat-colours") {
            flat_colours_ = true;
        } else if (arg == "--no-trees") {
            no_trees_ = true;
        } else if (arg == "--instance-cap" && has_one) {
            const godot::String text = args[++i];
            const int64_t v = text.is_valid_int() ? text.to_int() : -1;
            if (v >= 0 && v <= static_cast<int64_t>(terrain::MAX_INSTANCE_CAP)) {
                instance_cap_ = static_cast<std::uint64_t>(v);
                instance_cap_given_ = true;
            } else {
                refuse("--instance-cap " + text + ": not a whole number from 0 to 4000000");
            }
        } else if (arg == "--above" && has_one) {
            float v = 0.0f;
            const godot::String text = args[++i];
            if (number(text, v)) {
                opt_above_ = v;
            } else {
                refuse("--above " + text + ": not a number");
            }
        } else if (arg == "--veg-radius" && has_one) {
            // V3D-03 9.1: 0 or a whole number from 1536 to 8192.
            const godot::String text = args[++i];
            const int64_t v = text.is_valid_int() ? text.to_int() : -1;
            if (v == 0 || (v >= static_cast<int64_t>(terrain::NEAR_RADIUS_MIN) &&
                           v <= static_cast<int64_t>(terrain::NEAR_RADIUS_MAX))) {
                veg_radius_ = static_cast<double>(v);
                veg_radius_given_ = true;
            } else {
                refuse("--veg-radius " + text + ": 0 or a whole number from 1536 to 8192");
            }
        } else if (arg == "--veg-threads" && has_one) {
            const godot::String text = args[++i];
            const int64_t v = text.is_valid_int() ? text.to_int() : -1;
            if (v >= 0 && v <= 8) {
                veg_threads_ = static_cast<int>(v);
            } else {
                refuse("--veg-threads " + text + ": a whole number from 0 to 8");
            }
        } else if (arg == "--veg-whole-map") {
            veg_whole_map_ = true;
        } else if (arg == "--tier-colours") {
            tier_colours_ = true;
        } else if (arg == "--at-forest") {
            at_forest_ = true;
        } else if (!arg.begins_with("--") && world_path_.is_empty()) {
            world_path_ = arg;
        }
    }
    scripted_ = !screenshot_path_.is_empty() || bench_ || !check_dir_.is_empty();
    if (opt_above_ && opt_alt_) {
        refuse("--above cannot be combined with --alt");
    }
    if (veg_whole_map_ && veg_radius_given_) {
        refuse("--veg-whole-map cannot be combined with --veg-radius");
    }
    if (at_forest_ && opt_at_) {
        refuse("--at-forest cannot be combined with --at");
    }
    if (!instance_cap_given_) {
        // V3D-03 9.1: the far budget, or V3D-02's cap with --veg-whole-map.
        instance_cap_ = veg_whole_map_ ? terrain::DEFAULT_INSTANCE_CAP : terrain::DEFAULT_FAR_BUDGET;
    }
    if (!screenshot_path_.is_empty()) {
        if (bench_ || !check_dir_.is_empty()) {
            refuse("--screenshot cannot be combined with --bench or --check");
        } else if (is_headless()) {
            refuse("--screenshot needs a display");
        }
    }
    if (size_w.is_valid_int() && size_h.is_valid_int() && !is_headless()) {
        godot::DisplayServer::get_singleton()->window_set_size(
            godot::Vector2i(static_cast<int32_t>(size_w.to_int()), static_cast<int32_t>(size_h.to_int())));
    }
}

void TerrainPreview::clear_world() {
    // Every worker task finishes before the inputs it reads are freed.
    near_close();
    if (terrain_ != nullptr) {
        remove_child(terrain_);
        terrain_->queue_free();
        terrain_ = nullptr;
    }
    scene_ = TerrainScene{};
    height_image_.unref();
    control_image_.unref();
    colour_image_.unref();
    world_.unref();
    timings_ = TerrainTimings{};
}

void TerrainPreview::read_optional_layer(const char* name, const char* type, std::vector<godot::String>& notes) {
    if (world_->find_layer(name) < 0) {
        return;
    }
    const godot::Ref<WorldLayer> layer = world_->read_layer(name);
    if (layer.is_null()) {
        notes.push_back(godot::String(name) + ": " + world_->get_error_code() + ": " + world_->get_error_message() +
                        "; " + name + " not used.");
        return;
    }
    const int n = scene_.layout->n;
    const auto& grid = layer->layer().grid;
    if (static_cast<int>(grid.width) != n || static_cast<int>(grid.height) != n) {
        notes.push_back(godot::String(name) + " is " + godot::String::num_int64(grid.width) + "x" +
                        godot::String::num_int64(grid.height) + ", expected " + godot::String::num_int64(n) + "x" +
                        godot::String::num_int64(n) + "; " + name + " not used.");
        return;
    }
    if (layer->get_element_type() != type) {
        notes.push_back(godot::String(name) + " is " + layer->get_element_type() + ", expected " + type + "; " + name +
                        " not used.");
        return;
    }
    const std::string key = name;
    if (key == "water_depth") {
        scene_.water_depth = *grid.get_if<std::uint16_t>();
    } else if (key == "biome") {
        scene_.biome = *grid.get_if<std::uint8_t>();
    } else if (key == "region") {
        scene_.region = *grid.get_if<std::uint16_t>();
    } else if (key == "water") {
        scene_.water = *grid.get_if<std::uint8_t>();
    } else if (key == "river") {
        scene_.river = *grid.get_if<std::uint8_t>();
    } else if (key == "flow_dir") {
        scene_.flow_dir = *grid.get_if<std::uint8_t>();
    } else if (key == "timber") {
        scene_.timber = *grid.get_if<std::uint8_t>();
    } else if (key == "temperature") {
        scene_.temperature = *grid.get_if<std::int16_t>();
    }
}

bool TerrainPreview::open_world(const godot::String& path) {
    clear_world();
    status_lines_.clear();
    refused_ = false;
    // Option refusals of parse_options stay (they were printed already).

    std::uint64_t t0 = now_usec();
    current_path_ = path;
    world_.instantiate();
    if (!world_->open(path)) {
        status_lines_.push_back(path + godot::String(": ") + world_->get_error_code() + godot::String(": ") + world_->get_error_message());
        world_.unref();
        show_status();
        return false;
    }
    timings_.open = now_usec() - t0;

    t0 = now_usec();
    if (world_->find_layer("elevation") < 0) {
        status_lines_.push_back(NO_ELEVATION_TEXT);
        show_status();
        return false;
    }
    const godot::Ref<WorldLayer> elevation = world_->read_layer("elevation");
    if (elevation.is_null()) {
        status_lines_.push_back(path + godot::String(": ") + world_->get_error_code() + godot::String(": ") + world_->get_error_message());
        show_status();
        return false;
    }
    if (elevation->get_element_type() != "i16") {
        status_lines_.push_back("elevation is " + elevation->get_element_type() + ", expected i16.");
        show_status();
        return false;
    }
    std::string problem;
    scene_.layout = terrain::layout_for(static_cast<int>(elevation->get_width()),
                                        static_cast<int>(elevation->get_height()), problem);
    if (!scene_.layout) {
        status_lines_.push_back(to_godot(problem));
        show_status();
        return false;
    }
    const terrain::Layout layout = *scene_.layout;
    const int n = layout.n;
    scene_.elevation = *elevation->layer().grid.get_if<std::int16_t>();
    std::vector<godot::String> notes;
    read_optional_layer("water_depth", "u16", notes);
    read_optional_layer("biome", "u8", notes);
    read_optional_layer("region", "u16", notes);
    read_optional_layer("water", "u8", notes);
    read_optional_layer("river", "u8", notes);
    read_optional_layer("flow_dir", "u8", notes);
    read_optional_layer("timber", "u8", notes);
    read_optional_layer("temperature", "i16", notes);
    {
        // The placement key (V3D-02 2): meta seed, an unsigned decimal.
        const godot::String seed = world_->get_meta_value("seed");
        const std::string text = to_utf8(seed);
        bool ok = !text.empty() && text.size() <= 20;
        std::uint64_t value = 0;
        for (const char ch : text) {
            if (ch < '0' || ch > '9') {
                ok = false;
                break;
            }
            const auto digit = static_cast<std::uint64_t>(ch - '0');
            if (value > (UINT64_MAX - digit) / 10) {
                ok = false;
                break;
            }
            value = value * 10 + digit;
        }
        scene_.seed = ok ? value : 0;
        scene_.seed_known = ok;
    }
    timings_.layers = now_usec() - t0;

    t0 = now_usec();
    scene_.heights = terrain::build_heights(layout, scene_.elevation, scene_.water_depth);
    const auto& records = world_->biome_records();
    const bool have_biome = !scene_.biome.empty();
    scene_.textured = have_biome && !flat_colours_;
    const auto [lo, hi] = std::minmax_element(scene_.heights.begin(), scene_.heights.end());
    scene_.h_min = *lo;
    scene_.h_max = *hi;
    scene_.sea_rgb = terrain::sea_colour(records);
    timings_.buffers = now_usec() - t0;

    // Ground (V3D-02 4, 5): cells whenever there is a biome layer (the
    // placement reads them), control and colours of the textured mode.
    const std::size_t cells = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    t0 = now_usec();
    if (have_biome) {
        terrain::GroundInputs in;
        in.layout = &layout;
        in.elevation = scene_.elevation;
        in.biome = scene_.biome;
        in.water = scene_.water;
        in.river = scene_.river;
        in.flow_dir = scene_.flow_dir;
        in.temperature = scene_.temperature;
        in.records = records;
        scene_.ground = terrain::build_ground_cells(in);
    }
    timings_.cells = now_usec() - t0;
    t0 = now_usec();
    if (scene_.textured) {
        scene_.control = terrain::build_control(layout, scene_.ground);
        scene_.colours = terrain::build_textured_colours(layout, scene_.ground, records);
    } else {
        scene_.control.assign(cells, 0u);
        scene_.colours = terrain::build_colours(layout, scene_.biome, records);
    }
    timings_.control = now_usec() - t0;
    t0 = now_usec();
    scene_.vegetation = have_biome && !no_trees_;
    scene_.tiers = scene_.vegetation && !veg_whole_map_;
    veg_inputs_ = terrain::VegetationInputs{};
    veg_inputs_.layout = &*scene_.layout;
    veg_inputs_.heights = scene_.heights;
    veg_inputs_.elevation = scene_.elevation;
    veg_inputs_.timber = scene_.timber;
    veg_inputs_.water = scene_.water;
    veg_inputs_.cells = &scene_.ground;
    veg_inputs_.seed = scene_.seed;
    if (scene_.tiers) {
        // V3D-03 7.1: the far tier; the near tier follows the camera.
        terrain::FarTier far = terrain::place_far_tier(veg_inputs_, instance_cap_);
        scene_.far_threshold = far.threshold;
        scene_.far_candidates = far.far_candidates;
        scene_.placement = std::move(far.placement);
    } else if (scene_.vegetation) {
        scene_.placement = terrain::place_vegetation(layout, scene_.heights, scene_.elevation, scene_.timber, scene_.water,
                                                     scene_.ground, scene_.seed, instance_cap_);
    }
    scene_.forest_column = have_biome ? terrain::forest_column(layout, scene_.ground) : n / 2;
    timings_.placement = now_usec() - t0;

    t0 = now_usec();
    {
        // Little-endian floats for FORMAT_RF: the host's own byte order on
        // x86_64, the only target of the extension. Control values are bit
        // patterns, copied with memcpy, never through float arithmetic.
        godot::PackedByteArray bytes;
        bytes.resize(static_cast<int64_t>(cells * sizeof(float)));
        std::memcpy(bytes.ptrw(), scene_.heights.data(), cells * sizeof(float));
        height_image_ = godot::Image::create_from_data(n, n, false, godot::Image::FORMAT_RF, bytes);
        godot::PackedByteArray control;
        control.resize(static_cast<int64_t>(cells * sizeof(std::uint32_t)));
        std::memcpy(control.ptrw(), scene_.control.data(), cells * sizeof(std::uint32_t));
        control_image_ = godot::Image::create_from_data(n, n, false, godot::Image::FORMAT_RF, control);
        godot::PackedByteArray rgba;
        rgba.resize(static_cast<int64_t>(scene_.colours.size()));
        std::memcpy(rgba.ptrw(), scene_.colours.data(), scene_.colours.size());
        colour_image_ = godot::Image::create_from_data(n, n, false, godot::Image::FORMAT_RGBA8, rgba);
    }
    timings_.images = now_usec() - t0;

    const bool have_class = godot::ClassDBSingleton::get_singleton()->class_exists("Terrain3D");
    if (have_class && !no_terrain3d_) {
        if (!make_terrain()) {
            status_lines_.push_back("Terrain3D could not be created; see the log.");
        }
    } else {
        status_lines_.push_back(NO_TERRAIN3D_TEXT);
    }

    place_sea();
    place_start();
    near_open();

    if (opt_at_) {
        const auto [x, y] = *opt_at_;
        if (x < 0 || y < 0 || x >= n || y >= n) {
            refuse("--at " + godot::String::num_int64(x) + " " + godot::String::num_int64(y) +
                   ": not a cell of this map (0 to " + godot::String::num_int64(n - 1) + ")");
        }
    }

    // Status lines (V3D-01 7, V3D-02 10.3).
    if (!have_biome) {
        status_lines_.push_back("no biome layer: ground drawn grey, no vegetation");
    } else {
        status_lines_.push_back(scene_.textured ? "ground: textured, 15 ground types" : "ground: flat colours");
        if (scene_.tiers) {
            // V3D-03 9.2.
            godot::String line = "vegetation: far tier " + godot::String::num_int64(static_cast<int64_t>(scene_.placement.kept())) +
                                 " trees and snags of " + godot::String::num_int64(static_cast<int64_t>(scene_.far_candidates)) +
                                 " (budget " + godot::String::num_int64(static_cast<int64_t>(instance_cap_)) + "); ";
            if (veg_radius_ > 0.0) {
                line += "near tier within " + godot::String::num_int64(static_cast<int64_t>(veg_radius_)) + " m in cells of 256 m";
            } else {
                line += "no near tier: no shrubs or reeds";
            }
            status_lines_.push_back(line);
        }
        if (scene_.vegetation && !scene_.tiers) {
            godot::String line = "vegetation: " + godot::String::num_int64(static_cast<int64_t>(scene_.placement.kept())) + " instances";
            if (scene_.placement.candidates > scene_.placement.cap) {
                line += ", thinned from " + godot::String::num_int64(static_cast<int64_t>(scene_.placement.candidates)) + " to the cap";
            }
            status_lines_.push_back(line);
        }
        if (scene_.vegetation) {
            if (!scene_.seed_known) {
                status_lines_.push_back("no seed in meta: vegetation placed with seed 0");
            }
            if (scene_.timber.empty()) {
                status_lines_.push_back("no timber layer: tree density from the biome alone");
            }
        } else {
            status_lines_.push_back("vegetation: off");
        }
        if (scene_.textured) {
            if (scene_.water.empty()) {
                status_lines_.push_back("no water layer: rivers not drawn");
            }
            if (scene_.temperature.empty()) {
                status_lines_.push_back("no temperature layer: no snow");
            }
        }
    }
    if (scene_.water_depth.empty()) {
        status_lines_.push_back("no water_depth layer: lakes drawn at their beds");
    }
    const std::size_t unknown = terrain::unknown_biome_cells(scene_.biome, records);
    if (unknown > 0) {
        status_lines_.push_back(godot::String::num_int64(static_cast<int64_t>(unknown)) + " cells of unknown biome drawn magenta");
    }
    for (const auto& note : notes) {
        status_lines_.push_back(note);
    }
    show_status();
    after_load();
    return true;
}

void TerrainPreview::build_ground_assets() {
    // V3D-02 9.1 step 3: once per process; the textures and meshes do not
    // depend on the world.
    if (albedo_textures_.empty()) {
        const std::uint64_t t0 = now_usec();
        for (int id = 0; id < terrain::GROUND_TYPE_COUNT; ++id) {
            const terrain::GroundTexture tex = terrain::build_ground_texture(static_cast<terrain::GroundType>(id));
            albedo_textures_.push_back(rgba_texture(tex.albedo, terrain::GROUND_TEXTURE_SIZE));
            normal_textures_.push_back(rgba_texture(tex.normal, terrain::GROUND_TEXTURE_SIZE));
            mean_grey_.push_back(tex.mean_grey);
        }
        timings_.ground_textures = now_usec() - t0;
    }
    if (mesh_scenes_[0].is_null()) {
        godot::Ref<godot::StandardMaterial3D> material;
        material.instantiate();
        material->set_flag(godot::BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
        material->set_flag(godot::BaseMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
        material->set_roughness(0.9f);
        material->set_cull_mode(godot::BaseMaterial3D::CULL_BACK);
        for (int kind = 0; kind < terrain::VEGETATION_KINDS; ++kind) {
            mesh_scenes_[static_cast<std::size_t>(kind)] =
                vegetation_scene(kind, material, veg_meshes_[static_cast<std::size_t>(kind)]);
        }
    }
}

bool TerrainPreview::make_terrain() {
    godot::ClassDBSingleton* classes = godot::ClassDBSingleton::get_singleton();
    godot::Object* object = classes->instantiate("Terrain3D");
    godot::Node3D* node = godot::Object::cast_to<godot::Node3D>(object);
    if (node == nullptr) {
        godot::UtilityFunctions::printerr("TerrainPreview: Terrain3D is not a Node3D");
        if (object != nullptr) {
            memdelete(object);
        }
        return false;
    }
    node->set_name("Terrain3D");
    node->set("vertex_spacing", scene_.layout->spacing);  // V1
    node->set("free_editor_textures", false);             // no saved Assets file (5.3)

    const godot::Variant assets = classes->instantiate("Terrain3DAssets");
    godot::Object* assets_object = assets;
    if (assets_object != nullptr) {
        if (scene_.textured || scene_.vegetation) {
            build_ground_assets();
        }
        if (scene_.textured) {
            // The fifteen ground types (V3D-02 3).
            const auto& records = world_->biome_records();
            for (int id = 0; id < terrain::GROUND_TYPE_COUNT; ++id) {
                const auto type = static_cast<terrain::GroundType>(id);
                const terrain::GroundTypeInfo& info = terrain::GROUND_TYPES[static_cast<std::size_t>(id)];
                const godot::Variant asset = classes->instantiate("Terrain3DTextureAsset");
                godot::Object* a = asset;
                if (a == nullptr) {
                    continue;
                }
                terrain::GroundTexture mean;
                mean.mean_grey = mean_grey_[static_cast<std::size_t>(id)];
                const auto c = terrain::albedo_colour(type, mean, records);
                a->set("name", godot::String("ground ") + godot::String::num_int64(id));
                a->set("albedo_texture", albedo_textures_[static_cast<std::size_t>(id)]);
                a->set("normal_texture", normal_textures_[static_cast<std::size_t>(id)]);
                // albedo_color passes through an sRGB-to-linear conversion on its
                // way to the shader, so 3.3's linear value is encoded first.
                a->set("albedo_color", godot::Color(terrain::srgb_encode(c[0]), terrain::srgb_encode(c[1]),
                                                    terrain::srgb_encode(c[2]), 1.0f));
                a->set("uv_scale", info.uv_scale);
                const float detile = (id == 0 || id == 13) ? 0.0f : 0.2f;
                a->set("detiling_rotation", detile);
                a->set("detiling_shift", detile);
                assets_object->call("set_texture_asset", id, asset);
            }
        } else {
            // Texture asset 0 (V3D-01 5.3, V7): white albedo (alpha =
            // height), flat normal (alpha = roughness), both built in code.
            const godot::Variant asset = classes->instantiate("Terrain3DTextureAsset");
            if (godot::Object* a = asset; a != nullptr) {
                a->set("name", "white");
                a->set("albedo_texture", flat_texture(255, 255, 255, 0));
                a->set("normal_texture", flat_texture(128, 128, 255, 230));
                assets_object->call("set_texture_asset", 0, asset);
            }
        }
        if (scene_.vegetation) {
            // Mesh assets 0 to 4 (V3D-02 6.1): scene_file first, since
            // set_scene_file resets name, height offset, density and LOD
            // ranges.
            for (int kind = 0; kind < terrain::VEGETATION_KINDS; ++kind) {
                const terrain::VegetationKindInfo& info = terrain::VEGETATION_KINDS_INFO[static_cast<std::size_t>(kind)];
                const godot::Variant asset = classes->instantiate("Terrain3DMeshAsset");
                godot::Object* a = asset;
                if (a == nullptr) {
                    continue;
                }
                a->set("scene_file", mesh_scenes_[static_cast<std::size_t>(kind)]);
                a->set("name", info.name);
                a->set("enabled", trees_visible_);
                a->set("height_offset", 0.0f);
                a->set("cast_shadows", 0);
                a->set("visibility_layers", 1);
                a->set("lod0_range", info.lod0_range);
                if (info.lods > 1) {
                    a->set("lod1_range", info.lod1_range);
                }
                a->set("fade_margin", 0.0f);
                assets_object->call("set_mesh_asset", kind, asset);
            }
        }
        node->set("assets", assets);
    }

    add_child(node);
    terrain_ = node;
    // After entering the tree, Terrain3D's subsystems exist.
    node->set("collision_mode", 0);  // V9: Disabled
    const godot::Variant material = node->get("material");
    if (godot::Object* m = material; m != nullptr) {
        m->set("world_background", 0);  // V8: None
    }
    node->call("set_camera", camera_);  // V10

    godot::Object* data = node->call("get_data");
    if (data == nullptr) {
        godot::UtilityFunctions::printerr("TerrainPreview: Terrain3D has no data object");
        return false;
    }
    godot::TypedArray<godot::Image> images;
    images.push_back(height_image_);
    images.push_back(control_image_);
    images.push_back(colour_image_);
    const float origin = scene_.layout->origin;
    const std::uint64_t t0 = now_usec();
    data->call("import_images", images, godot::Vector3(origin, 0.0f, origin), 0.0f, 1.0f);  // V3, V4
    import_end_usec_ = now_usec();
    timings_.import = import_end_usec_ - t0;
    add_vegetation();
    import_end_usec_ = now_usec();  // the first frame starts after the transfer (V3D-02 12)
    return true;
}

void TerrainPreview::add_vegetation() {
    // V3D-02 9.1 step 5. add_transforms copies each region's and each
    // instancer cell's colour array once per instance it appends (Terrain3D
    // writes a PackedColorArray back for every push), so one call with every
    // instance of a kind is quadratic in a region's count. The instances
    // are therefore handed over one instancer cell (32 x 32 vertices, 512 m)
    // at a time, in placement order within each cell, which gives the same
    // stored instances.
    if (!scene_.vegetation || terrain_ == nullptr) {
        return;
    }
    godot::Object* instancer = terrain_->call("get_instancer");
    if (instancer == nullptr) {
        godot::UtilityFunctions::printerr("TerrainPreview: Terrain3D has no instancer");
        return;
    }
    const std::uint64_t t0 = now_usec();
    const terrain::Layout& layout = *scene_.layout;
    const float region_m = static_cast<float>(layout.region_size) * layout.spacing;
    const int cells_per_region = layout.region_size / 32;
    const int cells_per_axis = layout.region_count * cells_per_region;
    const float lo = -8.0f * static_cast<float>(layout.n);
    for (int kind = 0; kind < terrain::VEGETATION_KINDS; ++kind) {
        const auto& list = scene_.placement.kinds[static_cast<std::size_t>(kind)];
        if (list.empty()) {
            continue;
        }
        // Group the instances by instancer cell, keeping placement order.
        std::vector<std::uint32_t> cell_of(list.size());
        std::vector<std::uint32_t> count(static_cast<std::size_t>(cells_per_axis) * static_cast<std::size_t>(cells_per_axis) + 1, 0);
        for (std::size_t i = 0; i < list.size(); ++i) {
            const int cx = std::clamp(static_cast<int>(std::floor((list[i].x - lo) / (region_m / static_cast<float>(cells_per_region)))), 0, cells_per_axis - 1);
            const int cz = std::clamp(static_cast<int>(std::floor((list[i].z - lo) / (region_m / static_cast<float>(cells_per_region)))), 0, cells_per_axis - 1);
            cell_of[i] = static_cast<std::uint32_t>(cz * cells_per_axis + cx);
            ++count[cell_of[i] + 1];
        }
        for (std::size_t c = 1; c < count.size(); ++c) {
            count[c] += count[c - 1];
        }
        std::vector<std::uint32_t> order(list.size());
        {
            std::vector<std::uint32_t> next(count.begin(), count.end() - 1);
            for (std::size_t i = 0; i < list.size(); ++i) {
                order[next[cell_of[i]]++] = static_cast<std::uint32_t>(i);
            }
        }
        for (std::size_t c = 0; c + 1 < count.size(); ++c) {
            if (count[c] == count[c + 1]) {
                continue;
            }
            godot::TypedArray<godot::Transform3D> transforms;
            godot::PackedColorArray colours;
            colours.resize(static_cast<int64_t>(count[c + 1] - count[c]));
            int64_t slot = 0;
            for (std::uint32_t k = count[c]; k < count[c + 1]; ++k) {
                const terrain::Instance& in = list[order[k]];
                const float s = static_cast<float>(in.scale_pm) / 1000.0f;
                const float yaw = static_cast<float>(in.yaw1024) * 6.28318530717958647692f / 1024.0f;
                const godot::Basis basis = godot::Basis(godot::Vector3(0.0f, 1.0f, 0.0f), yaw).scaled(godot::Vector3(s, s, s));
                transforms.push_back(godot::Transform3D(basis, godot::Vector3(in.x, in.y, in.z)));
                const float v = static_cast<float>(200 + static_cast<int>(in.tint) * 55 / 255) / 255.0f;
                colours.set(slot++, godot::Color(v, v, v, 1.0f));
            }
            instancer->call("add_transforms", kind, transforms, colours, false);
        }
    }
    instancer->call("update_mmis", -1, godot::Vector2i(0, 0), true);
    timings_.transfer = now_usec() - t0;
}

void TerrainPreview::place_sea() {
    // V3D-02 7: a plane of 800 km faded by depth fog to the sky's ground
    // colour, so it reaches the horizon with no visible edge.
    const float side = SEA_SIDE;
    godot::Ref<godot::PlaneMesh> plane;
    plane.instantiate();
    plane->set_size(godot::Vector2(side, side));
    godot::Ref<godot::StandardMaterial3D> material;
    material.instantiate();
    material->set_albedo(rgb_colour(scene_.sea_rgb));
    material->set_roughness(0.2f);
    plane->set_material(material);
    sea_->set_mesh(plane);
    sea_->set_position(godot::Vector3(0.0f, 0.0f, 0.0f));
    camera_->set_far(CAMERA_FAR);
    // Beyond the plane the sky's ground shows below the horizon; give it the
    // sea's colour so the sea reads as reaching the horizon (9.3 G1).
    if (auto* env_node = godot::Object::cast_to<godot::WorldEnvironment>(get_node_or_null("WorldEnvironment"))) {
        const godot::Ref<godot::Environment> env = env_node->get_environment();
        if (env.is_valid()) {
            env->set_fog_enabled(true);
            env->set_fog_mode(godot::Environment::FOG_MODE_DEPTH);
            env->set_fog_light_color(rgb_colour(scene_.sea_rgb));
            env->set_fog_light_energy(1.0f);
            env->set_fog_density(1.0f);
            env->set_fog_sun_scatter(0.0f);
            env->set_fog_aerial_perspective(0.0f);
            env->set_fog_sky_affect(0.0f);
            env->set_fog_depth_begin(FOG_BEGIN);
            env->set_fog_depth_end(FOG_END);
            env->set_fog_depth_curve(1.0f);
        }
        if (env.is_valid() && env->get_sky().is_valid()) {
            const godot::Ref<godot::ProceduralSkyMaterial> sky = env->get_sky()->get_material();
            if (sky.is_valid()) {
                sky->set_ground_bottom_color(rgb_colour(scene_.sea_rgb));
                sky->set_ground_horizon_color(rgb_colour(scene_.sea_rgb));
            }
        }
    }
}

void TerrainPreview::place_start() {
    const terrain::Layout& layout = *scene_.layout;
    const int n = layout.n;
    int cx = n / 2;
    int cy = 7 * n / 8;
    if (opt_at_ && opt_at_->first >= 0 && opt_at_->second >= 0 && opt_at_->first < n && opt_at_->second < n) {
        cx = opt_at_->first;
        cy = opt_at_->second;
    } else if (at_forest_) {
        cx = scene_.forest_column;  // V3D-03 9.1: the forest path's start (12.1)
    }
    const terrain::Vec2 w = terrain::cell_to_world(layout, cx, cy);
    start_ = terrain::FlyState{};
    start_.x = w.x;
    start_.z = w.z;
    start_.y = opt_alt_.value_or(scene_.h_max + 300.0f);
    if (opt_above_) {
        start_.y = terrain::ground_clamp(layout, scene_.heights, w.x, w.z) + *opt_above_;
    } else if (at_forest_ && !opt_alt_) {
        start_.y = terrain::ground_clamp(layout, scene_.heights, w.x, w.z) + 60.0f;
    }
    start_.heading_deg = opt_heading_.value_or(0.0f);
    start_.pitch_deg = opt_pitch_.value_or(-25.0f);
    start_.speed_index = opt_speed_index_.value_or(terrain::DEFAULT_SPEED_INDEX);
    // The heading wrap and the floor of 8.2, without moving.
    terrain::step(start_, terrain::FlyInput{}, 0.0f,
                  [this](float X, float Z) { return terrain::ground_clamp(*scene_.layout, scene_.heights, X, Z); },
                  16.0f * static_cast<float>(n));
    fly_ = start_;
    apply_camera();
}

void TerrainPreview::apply_camera() {
    const terrain::Rotation r = terrain::camera_rotation(fly_);
    camera_->set_position(godot::Vector3(fly_.x, fly_.y, fly_.z));
    camera_->set_rotation(godot::Vector3(r.x, r.y, r.z));
}

void TerrainPreview::show_status() {
    if (status_ != nullptr) {
        status_->set_text(status_text());
    }
}

std::string TerrainPreview::hud_at(float X, float Y, float Z, int fps) const {
    if (!scene_.layout) {
        return {};
    }
    terrain::HudWorld hud;
    hud.layout = &*scene_.layout;
    hud.heights = scene_.heights;
    hud.elevation = scene_.elevation;
    hud.water_depth = scene_.water_depth;
    hud.biome = scene_.biome;
    hud.region = scene_.region;
    const WorldFile* world = world_.ptr();
    hud.biome_name = [world](std::int64_t code) {
        const worldgen::inspect::BiomeRecord* record = world->find_biome(code);
        return record != nullptr ? record->name : std::string();
    };
    hud.region_name = [world](std::int64_t id) { return to_utf8(world->get_region_name(id)); };
    return terrain::hud_line(hud, X, Y, Z, terrain::speed_of(fly_, boost_), fps);
}

void TerrainPreview::update_hud() {
    if (hud_ == nullptr || !hud_->is_visible() || !scene_.layout) {
        return;
    }
    const int fps = static_cast<int>(std::lround(godot::Engine::get_singleton()->get_frames_per_second()));
    hud_->set_text(to_godot(hud_at(fly_.x, fly_.y, fly_.z, fps)) + "\n" + to_godot(vegetation_hud_line()) + "\n" +
                   terrain::HUD_KEYS);
}

void TerrainPreview::after_load() {
    update_hud();
    if (!screenshot_path_.is_empty() && !refused_) {
        // V3D-03 9.1: after the first drawn frame after the terrain is added
        // (the instancer builds its multimeshes in that frame), then once
        // the near tier has settled (at most 30 s), 2.0 s.
        godot::RenderingServer::get_singleton()->connect(
            "frame_post_draw", callable_mp(this, &TerrainPreview::on_first_drawn_frame), godot::Object::CONNECT_ONE_SHOT);
    }
    if (bench_ && !refused_) {
        // 10: the first frame runs from the end of the import to the next
        // frame_post_draw. Headless, no frame is drawn, so the next
        // process frame stands in for it.
        if (import_end_usec_ == 0) {
            import_end_usec_ = now_usec();
        }
        if (is_headless()) {
            get_tree()->connect("process_frame", callable_mp(this, &TerrainPreview::bench_first_frame),
                                godot::Object::CONNECT_ONE_SHOT);
        } else {
            godot::RenderingServer::get_singleton()->connect(
                "frame_post_draw", callable_mp(this, &TerrainPreview::bench_first_frame),
                godot::Object::CONNECT_ONE_SHOT);
        }
    }
}

namespace {

// Video memory in use: the rendering server's total, or where the driver
// reports none (seen with the NVIDIA Vulkan driver), its texture and buffer
// counts.
std::uint64_t video_memory_bytes() {
    godot::RenderingServer* rs = godot::RenderingServer::get_singleton();
    const std::uint64_t total = rs->get_rendering_info(godot::RenderingServer::RENDERING_INFO_VIDEO_MEM_USED);
    const std::uint64_t parts = rs->get_rendering_info(godot::RenderingServer::RENDERING_INFO_TEXTURE_MEM_USED) +
                                rs->get_rendering_info(godot::RenderingServer::RENDERING_INFO_BUFFER_MEM_USED);
    return std::max(total, parts);
}

}  // namespace

void TerrainPreview::on_first_drawn_frame() {
    phase_ = Phase::screenshot_settle;
    phase_frames_ = 0;
    phase_start_usec_ = now_usec();
}

void TerrainPreview::bench_first_frame() {
    first_frame_usec_ = now_usec() - import_end_usec_;
    const int n = scene_.layout->n;
    int64_t regions = 0;
    if (terrain_ != nullptr) {
        godot::Object* data = terrain_->call("get_data");
        if (data != nullptr) {
            regions = data->call("get_region_count");
        }
    }
    godot::UtilityFunctions::print("UNMAPPED-TERRAIN-BENCH: map ", n, ", open ", ms(timings_.open), " ms, layers ",
                                   ms(timings_.layers), " ms, buffers ", ms(timings_.buffers), " ms, images ",
                                   ms(timings_.images), " ms, import ", ms(timings_.import), " ms, first frame ",
                                   ms(first_frame_usec_), " ms");
    godot::Performance* perf = godot::Performance::get_singleton();
    const double mb = 1024.0 * 1024.0;
    godot::UtilityFunctions::print(
        "UNMAPPED-TERRAIN-BENCH: memory static ",
        to_godot(terrain::one_decimal(perf->get_monitor(godot::Performance::MEMORY_STATIC) / mb)), " MB, video ",
        to_godot(terrain::one_decimal(static_cast<double>(video_memory_bytes()) / mb)),
        " MB, regions ", regions);
    godot::UtilityFunctions::print("UNMAPPED-TERRAIN-BENCH: ground textures ", ms(timings_.ground_textures), " ms, cells ",
                                   ms(timings_.cells), " ms, control ", ms(timings_.control),
                                   " ms; vegetation placement ", ms(timings_.placement), " ms, transfer ",
                                   ms(timings_.transfer), " ms, instances ",
                                   static_cast<int64_t>(scene_.placement.kept()), " of ",
                                   static_cast<int64_t>(scene_.placement.candidates), " (cap ",
                                   static_cast<int64_t>(instance_cap_), ")");
    // V3D-02 12: the view at the centre cell, 150 m above the ground,
    // heading 0, pitch -10, vsync off: 60 warm-up frames, then 300. V3D-03
    // 12.2: the near tier's settling is timed from here.
    const terrain::Vec2 c = terrain::cell_to_world(*scene_.layout, n / 2, n / 2);
    fly_ = start_;
    fly_.x = c.x;
    fly_.z = c.z;
    fly_.y = terrain::ground_clamp(*scene_.layout, scene_.heights, c.x, c.z) + 150.0f;
    fly_.heading_deg = 0.0f;
    fly_.pitch_deg = -10.0f;
    apply_camera();
    view_start_usec_ = now_usec();
    settled_usec_ = 0;
    settled_seen_ = false;
    if (is_headless()) {
        // Headless: the V3D-02 lines and the tiers line only.
        if (scene_.tiers) {
            phase_ = Phase::headless_settle;
            phase_frames_ = 0;
            phase_start_usec_ = now_usec();
        } else {
            get_tree()->quit(0);
        }
        return;
    }
    godot::DisplayServer::get_singleton()->window_set_vsync_mode(godot::DisplayServer::VSYNC_DISABLED);
    godot::OS::get_singleton()->set_low_processor_usage_mode(false);
    view_frames_ = 0;
    frame_usec_.clear();
    last_frame_usec_ = now_usec();
    godot::RenderingServer::get_singleton()->connect("frame_post_draw",
                                                     callable_mp(this, &TerrainPreview::bench_view_frame));
}

void TerrainPreview::bench_view_frame() {
    const std::uint64_t t = now_usec();
    ++view_frames_;
    if (view_frames_ > 60) {
        frame_usec_.push_back(t - last_frame_usec_);
    }
    last_frame_usec_ = t;
    if (frame_usec_.size() < 300) {
        return;
    }
    godot::RenderingServer::get_singleton()->disconnect("frame_post_draw",
                                                        callable_mp(this, &TerrainPreview::bench_view_frame));
    std::uint64_t sum = 0;
    std::uint64_t max = 0;
    int over16 = 0;
    for (const std::uint64_t f : frame_usec_) {
        sum += f;
        max = std::max(max, f);
        over16 += f > 16700 ? 1 : 0;
    }
    const int n = scene_.layout->n;
    godot::UtilityFunctions::print("UNMAPPED-TERRAIN-BENCH: view at cell (", n / 2, ", ", n / 2,
                                   "), 150 m above ground: frames 300, mean ", ms(sum / frame_usec_.size()), ", max ",
                                   ms(max), ", over 16.7 ms ", over16);
    // V3D-03 12.2: the tiers line once the near tier has settled, then the
    // forest flights, then V3D-01's flight.
    phase_ = Phase::view_settle;
    phase_frames_ = 0;
    phase_start_usec_ = now_usec();
}

void TerrainPreview::start_flight() {
    // The flight: from the start, heading north at pitch -10 and 1000 m/s,
    // for 16 N / 1000 s, vsync off.
    const int n = scene_.layout->n;
    fly_ = start_;
    fly_.heading_deg = 0.0f;
    fly_.pitch_deg = -10.0f;
    fly_.speed_index = 6;
    flight_length_ = 16.0f * static_cast<float>(n) / 1000.0f;
    flight_time_ = 0.0f;
    flying_ = true;
    frame_usec_.clear();
    last_frame_usec_ = now_usec();
    godot::RenderingServer::get_singleton()->connect("frame_post_draw",
                                                     callable_mp(this, &TerrainPreview::bench_flight_frame));
}

void TerrainPreview::bench_flight_frame() {
    const std::uint64_t t = now_usec();
    frame_usec_.push_back(t - last_frame_usec_);
    last_frame_usec_ = t;
}

void TerrainPreview::finish_bench() {
    flying_ = false;
    godot::RenderingServer::get_singleton()->disconnect("frame_post_draw",
                                                        callable_mp(this, &TerrainPreview::bench_flight_frame));
    std::uint64_t sum = 0;
    std::uint64_t max = 0;
    int over11 = 0;
    int over16 = 0;
    int over33 = 0;
    for (const std::uint64_t f : frame_usec_) {
        sum += f;
        max = std::max(max, f);
        over11 += f > 11100 ? 1 : 0;
        over16 += f > 16700 ? 1 : 0;
        over33 += f > 33300 ? 1 : 0;
    }
    const std::uint64_t mean = frame_usec_.empty() ? 0 : sum / frame_usec_.size();
    godot::UtilityFunctions::print("UNMAPPED-TERRAIN-BENCH: flight ",
                                   to_godot(terrain::one_decimal(static_cast<double>(flight_length_))),
                                   " s at 1000 m/s: frames ", static_cast<int64_t>(frame_usec_.size()), ", mean ",
                                   ms(mean), ", max ", ms(max), ", over 11.1 ms ", over11, ", over 16.7 ms ", over16,
                                   ", over 33.3 ms ", over33);
    get_tree()->quit(0);
}

void TerrainPreview::_process(double delta) {
    if (!scene_.layout) {
        return;
    }
    const auto dt = static_cast<float>(delta);
    const float bound = 16.0f * static_cast<float>(scene_.layout->n);
    const auto ground = [this](float X, float Z) {
        return terrain::ground_clamp(*scene_.layout, scene_.heights, X, Z);
    };
    if (check_run_) {
        // The check's path: the check places the camera, the near tier
        // runs its frame, the check looks at the result.
        check_run_->before_frame(*this);
        near_frame();
        if (!check_run_->after_frame(*this)) {
            const int code = check_run_->exit_code();
            check_run_.reset();
            get_tree()->quit(code);
        }
        return;
    }
    if (flying_) {
        terrain::FlyInput in;
        in.forward = 1;
        terrain::step(fly_, in, dt, ground, bound);
        apply_camera();
        near_frame();
        flight_time_ += dt;
        if (flight_time_ >= flight_length_) {
            finish_bench();
        }
        return;
    }
    if (scripted_) {
        const Phase before = phase_;
        bench_phases(dt);
        if (phase_ != before) {
            phase_frames_ = 0;
        }
        near_frame();
        ++phase_frames_;
        update_hud();
        return;
    }
    terrain::FlyInput in;
    const bool dialog_open = open_dialog_ != nullptr && open_dialog_->is_visible();
    if (!dialog_open) {
        godot::Input* input = godot::Input::get_singleton();
        auto key = [input](godot::Key k) { return input->is_physical_key_pressed(k) ? 1 : 0; };
        in.forward = key(godot::KEY_W) - key(godot::KEY_S);
        in.right = key(godot::KEY_D) - key(godot::KEY_A);
        in.up = key(godot::KEY_E) - key(godot::KEY_Q);
        in.boost = input->is_key_pressed(godot::KEY_SHIFT);
        in.yaw_deg = static_cast<float>(key(godot::KEY_RIGHT) - key(godot::KEY_LEFT)) * ARROW_YAW_DEG_PER_S * dt;
        in.pitch_deg = static_cast<float>(key(godot::KEY_UP) - key(godot::KEY_DOWN)) * ARROW_PITCH_DEG_PER_S * dt;
    }
    in.yaw_deg += pending_yaw_;
    in.pitch_deg += pending_pitch_;
    pending_yaw_ = 0.0f;
    pending_pitch_ = 0.0f;
    boost_ = in.boost;
    terrain::step(fly_, in, dt, ground, bound);
    apply_camera();
    near_frame();
    update_hud();
}

void TerrainPreview::_unhandled_input(const godot::Ref<godot::InputEvent>& event) {
    if (event.is_null()) {
        return;
    }
    godot::Input* input = godot::Input::get_singleton();
    if (const auto* button = godot::Object::cast_to<godot::InputEventMouseButton>(event.ptr())) {
        if (button->get_button_index() == godot::MOUSE_BUTTON_RIGHT) {
            mouse_look_ = button->is_pressed();
            input->set_mouse_mode(mouse_look_ ? godot::Input::MOUSE_MODE_CAPTURED : godot::Input::MOUSE_MODE_VISIBLE);
            get_viewport()->set_input_as_handled();
        } else if (button->is_pressed() && button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_UP) {
            terrain::speed_step(fly_, 1);
            get_viewport()->set_input_as_handled();
        } else if (button->is_pressed() && button->get_button_index() == godot::MOUSE_BUTTON_WHEEL_DOWN) {
            terrain::speed_step(fly_, -1);
            get_viewport()->set_input_as_handled();
        }
        return;
    }
    if (const auto* motion = godot::Object::cast_to<godot::InputEventMouseMotion>(event.ptr())) {
        if (mouse_look_) {
            const godot::Vector2 rel = motion->get_relative();
            pending_yaw_ += rel.x * MOUSE_DEG_PER_PIXEL;
            pending_pitch_ -= rel.y * MOUSE_DEG_PER_PIXEL;
            get_viewport()->set_input_as_handled();
        }
        return;
    }
    if (const auto* key = godot::Object::cast_to<godot::InputEventKey>(event.ptr())) {
        if (!key->is_pressed() || key->is_echo()) {
            return;
        }
        switch (key->get_physical_keycode()) {
            case godot::KEY_MINUS:
                terrain::speed_step(fly_, -1);
                break;
            case godot::KEY_EQUAL:
                terrain::speed_step(fly_, 1);
                break;
            case godot::KEY_R:
                if (scene_.layout) {
                    const int speed = fly_.speed_index;
                    fly_ = start_;
                    fly_.speed_index = speed;
                    apply_camera();
                }
                break;
            case godot::KEY_T:
                toggle_trees();
                break;
            case godot::KEY_G:
                toggle_ground();
                break;
            case godot::KEY_H:
                if (hud_ != nullptr) {
                    hud_->set_visible(!hud_->is_visible());
                }
                break;
            case godot::KEY_ESCAPE:
                mouse_look_ = false;
                input->set_mouse_mode(godot::Input::MOUSE_MODE_VISIBLE);
                break;
            default:
                return;
        }
        get_viewport()->set_input_as_handled();
    }
}

bool TerrainPreview::toggle_trees() {
    if (no_trees_ || !scene_.vegetation || terrain_ == nullptr) {
        return false;
    }
    trees_visible_ = !trees_visible_;
    if (near_root_ != nullptr) {
        near_root_->set_visible(trees_visible_);  // V3D-03 7.2: the bookkeeping keeps running
    }
    godot::Object* assets = terrain_->call("get_assets");
    if (assets == nullptr) {
        return false;
    }
    const int64_t count = assets->call("get_mesh_count");
    for (int64_t id = 0; id < count; ++id) {
        godot::Object* asset = assets->call("get_mesh_asset", id);
        if (asset != nullptr) {
            asset->set("enabled", trees_visible_);
        }
    }
    return true;
}

bool TerrainPreview::toggle_ground() {
    if (!scene_.layout || scene_.biome.empty() || current_path_.is_empty()) {
        return false;
    }
    const terrain::FlyState keep = fly_;
    const terrain::FlyState keep_start = start_;
    flat_colours_ = !flat_colours_;
    const godot::String path = current_path_;
    if (!open_world(path)) {
        return false;
    }
    fly_ = keep;
    start_ = keep_start;
    apply_camera();
    return true;
}

void TerrainPreview::on_open_pressed() {
    open_dialog_->popup_centered_ratio(0.6f);
}

void TerrainPreview::on_file_selected(const godot::String& path) {
    open_world(path);
}

void TerrainPreview::take_screenshot() {
    const godot::Ref<godot::Image> image = get_viewport()->get_texture()->get_image();
    const bool saved = image.is_valid() && image->save_png(screenshot_path_) == godot::OK;
    if (saved) {
        godot::UtilityFunctions::print("TerrainPreview: screenshot saved to ", screenshot_path_);
    } else {
        godot::UtilityFunctions::printerr("TerrainPreview: cannot save a screenshot to ", screenshot_path_);
    }
    get_tree()->quit(saved ? 0 : 1);
}

void TerrainPreview::run_check_deferred() {
    TerrainCheckOptions options;
    options.reference_dir = check_dir_;
    options.expect_terrain3d = expect_terrain3d_.is_empty() ? godot::String("absent") : expect_terrain3d_;
    options.world_path = world_path_;
    const int code = run_terrain_check(*this, options, &check_run_);
    if (check_run_) {
        return;  // the path runs in _process (V3D-03 11.2)
    }
    get_tree()->quit(code);
}

}  // namespace unmapped
