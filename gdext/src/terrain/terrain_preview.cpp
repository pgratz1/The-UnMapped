#include "terrain/terrain_preview.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

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
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/vector3.hpp>

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
        } else if (!arg.begins_with("--") && world_path_.is_empty()) {
            world_path_ = arg;
        }
    }
    scripted_ = !screenshot_path_.is_empty() || bench_ || !check_dir_.is_empty();
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
    }
}

bool TerrainPreview::open_world(const godot::String& path) {
    clear_world();
    status_lines_.clear();
    refused_ = false;
    // Option refusals of parse_options stay (they were printed already).

    std::uint64_t t0 = now_usec();
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
    timings_.layers = now_usec() - t0;

    t0 = now_usec();
    scene_.heights = terrain::build_heights(layout, scene_.elevation, scene_.water_depth);
    const auto& records = world_->biome_records();
    scene_.colours = terrain::build_colours(layout, scene_.biome, records);
    const auto [lo, hi] = std::minmax_element(scene_.heights.begin(), scene_.heights.end());
    scene_.h_min = *lo;
    scene_.h_max = *hi;
    scene_.sea_rgb = terrain::sea_colour(records);
    timings_.buffers = now_usec() - t0;

    t0 = now_usec();
    const std::size_t cells = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    {
        // Little-endian floats for FORMAT_RF: the host's own byte order on
        // x86_64, the only target of the extension.
        godot::PackedByteArray bytes;
        bytes.resize(static_cast<int64_t>(cells * sizeof(float)));
        std::memcpy(bytes.ptrw(), scene_.heights.data(), cells * sizeof(float));
        height_image_ = godot::Image::create_from_data(n, n, false, godot::Image::FORMAT_RF, bytes);
        godot::PackedByteArray zeros;
        zeros.resize(static_cast<int64_t>(cells * sizeof(float)));
        zeros.fill(0);
        control_image_ = godot::Image::create_from_data(n, n, false, godot::Image::FORMAT_RF, zeros);
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

    if (opt_at_) {
        const auto [x, y] = *opt_at_;
        if (x < 0 || y < 0 || x >= n || y >= n) {
            refuse("--at " + godot::String::num_int64(x) + " " + godot::String::num_int64(y) +
                   ": not a cell of this map (0 to " + godot::String::num_int64(n - 1) + ")");
        }
    }

    if (scene_.biome.empty()) {
        status_lines_.push_back("no biome layer: terrain drawn grey");
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

    // Texture asset 0 (5.3, V7): white albedo (alpha = height), flat normal
    // (alpha = roughness), both built in code.
    const godot::Variant assets = classes->instantiate("Terrain3DAssets");
    const godot::Variant asset = classes->instantiate("Terrain3DTextureAsset");
    godot::Object* asset_object = asset;
    godot::Object* assets_object = assets;
    if (asset_object != nullptr && assets_object != nullptr) {
        asset_object->set("name", "white");
        asset_object->set("albedo_texture", flat_texture(255, 255, 255, 0));
        asset_object->set("normal_texture", flat_texture(128, 128, 255, 230));
        assets_object->call("set_texture_asset", 0, asset);
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
    return true;
}

void TerrainPreview::place_sea() {
    const float side = 48.0f * static_cast<float>(scene_.layout->n);
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
    camera_->set_far(side);
    // Beyond the plane the sky's ground shows below the horizon; give it the
    // sea's colour so the sea reads as reaching the horizon (9.3 G1).
    if (auto* env_node = godot::Object::cast_to<godot::WorldEnvironment>(get_node_or_null("WorldEnvironment"))) {
        const godot::Ref<godot::Environment> env = env_node->get_environment();
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
    }
    const terrain::Vec2 w = terrain::cell_to_world(layout, cx, cy);
    start_ = terrain::FlyState{};
    start_.x = w.x;
    start_.z = w.z;
    start_.y = opt_alt_.value_or(scene_.h_max + 300.0f);
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
    hud_->set_text(to_godot(hud_at(fly_.x, fly_.y, fly_.z, fps)) + "\n" + terrain::HUD_KEYS);
}

void TerrainPreview::after_load() {
    update_hud();
    if (!screenshot_path_.is_empty() && !refused_) {
        get_tree()->create_timer(2.0)->connect("timeout", callable_mp(this, &TerrainPreview::take_screenshot));
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
    if (is_headless()) {
        get_tree()->quit(0);
        return;
    }
    // The flight: from the start, heading north at pitch -10 and 1000 m/s,
    // for 16 N / 1000 s, vsync off.
    godot::DisplayServer::get_singleton()->window_set_vsync_mode(godot::DisplayServer::VSYNC_DISABLED);
    godot::OS::get_singleton()->set_low_processor_usage_mode(false);
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
    if (flying_) {
        terrain::FlyInput in;
        in.forward = 1;
        terrain::step(fly_, in, dt, ground, bound);
        apply_camera();
        flight_time_ += dt;
        if (flight_time_ >= flight_length_) {
            finish_bench();
        }
        return;
    }
    if (scripted_) {
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
    const int code = run_terrain_check(*this, options);
    get_tree()->quit(code);
}

}  // namespace unmapped
