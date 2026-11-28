#pragma once

// TerrainPreview: the 3D terrain preview (spec V3D-01 sections 4, 7, 8, 10).
//
// The root Node3D of game/viewer3d/terrain_preview.tscn. The scene holds the
// layout (environment, sun, camera, sea plane, HUD, status, Open button and
// dialog); this class finds them by unique name (%Camera, %Sea, %Hud,
// %Status, %OpenButton, %OpenDialog) and does the rest in code:
//
//   - opens world.db read-only through WorldFile and reads the layers
//     elevation, water_depth, biome and region (no SQL here);
//   - builds the height, control and colour buffers with terrain_data.hpp
//     (plain C++, no Godot types) and copies them into Godot Images;
//   - creates the Terrain3D node through Godot's ClassDB (this library does
//     not link Terrain3D; the class exists only when the plug-in is in the
//     run copy) and imports the images into it;
//   - flies the camera with fly_state.hpp (plain C++), and writes the HUD.
//
// The data (TerrainScene below) is kept apart from the Godot nodes, so a
// later view (VR, the in-game map) can reuse it.
//
// Command line (arguments after `--`, 4):
//   <world.db>              absolute path; the first argument not starting
//                           with --
//   --at <x> <y>            start above cell (x, y)
//   --alt <m>               start altitude in metres
//   --heading <deg>         0 north, 90 east
//   --pitch <deg>           -89 to 89
//   --speed <v>             10, 20, 50, 100, 200, 500, 1000 or 2000
//   --size <w> <h>          window size
//   --screenshot <file.png> save the window 2.0 s after the terrain is added,
//                           then quit
//   --bench                 print the timings of 10, then quit
//   --check <dir>           the terrain check (terrain_check.hpp), then quit;
//                           --expect-terrain3d present|absent
//   --no-terrain3d          behave as if class Terrain3D did not exist
//   --flat-colours          V3D-01's ground: one white texture, control 0,
//                           biome colours (spec V3D-02 10.1)
//   --no-trees              no vegetation
//   --instance-cap <n>      the vegetation cap, 0 to 4000000 (default 2000000)
//   --above <m>             start m metres above the ground at the start cell
//
// Keys of V3D-02 10.2: T vegetation on or off, G ground textured or flat.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "terrain/fly_state.hpp"
#include "terrain/ground.hpp"
#include "terrain/terrain_data.hpp"
#include "terrain/vegetation.hpp"
#include "world_file.hpp"

namespace unmapped {

// What the preview holds of one opened world (no Godot nodes).
struct TerrainScene {
    std::optional<terrain::Layout> layout;
    std::vector<std::int16_t> elevation;
    std::vector<std::uint16_t> water_depth;  // empty when absent
    std::vector<std::uint8_t> biome;         // empty when absent
    std::vector<std::uint16_t> region;       // empty when absent
    std::vector<std::uint8_t> water;         // empty when absent (V3D-02 2)
    std::vector<std::uint8_t> river;         // empty when absent
    std::vector<std::uint8_t> flow_dir;      // empty when absent
    std::vector<std::uint8_t> timber;        // empty when absent
    std::vector<std::int16_t> temperature;   // empty when absent
    std::uint64_t seed = 0;                  // meta seed, 0 when absent
    bool seed_known = false;
    std::vector<float> heights;
    std::vector<std::uint8_t> colours;       // RGBA, 4 n n bytes, as imported
    bool textured = false;                   // the ground of V3D-02 (else V3D-01's flat colours)
    terrain::GroundCells ground;             // empty without a biome layer
    std::vector<std::uint32_t> control;      // control bits, as imported
    bool vegetation = false;                 // a placement was made
    terrain::Placement placement;
    float h_min = 0.0f;
    float h_max = 0.0f;
    std::uint32_t sea_rgb = 0x183878;
};

// Times of the last open, in microseconds (10).
struct TerrainTimings {
    std::uint64_t open = 0;
    std::uint64_t layers = 0;
    std::uint64_t buffers = 0;
    std::uint64_t images = 0;
    std::uint64_t import = 0;
    std::uint64_t ground_textures = 0;  // V3D-02 12
    std::uint64_t cells = 0;
    std::uint64_t control = 0;
    std::uint64_t placement = 0;
    std::uint64_t transfer = 0;
};

class TerrainPreview : public godot::Node3D {
    GDCLASS(TerrainPreview, godot::Node3D)

public:
    TerrainPreview() = default;

    void _ready() override;
    void _process(double delta) override;
    void _unhandled_input(const godot::Ref<godot::InputEvent>& event) override;

    // Opens a world and replaces the one shown. False when nothing could be
    // shown (the status says why).
    bool open_world(const godot::String& path);

    // --- Read by the check (terrain_check.cpp) --------------------------------
    [[nodiscard]] const TerrainScene& scene() const noexcept { return scene_; }
    [[nodiscard]] const godot::Ref<WorldFile>& world() const noexcept { return world_; }
    [[nodiscard]] godot::Ref<godot::Image> height_image() const { return height_image_; }
    [[nodiscard]] godot::Ref<godot::Image> control_image() const { return control_image_; }
    [[nodiscard]] godot::Ref<godot::Image> colour_image() const { return colour_image_; }
    // The Terrain3D node, or null.
    [[nodiscard]] godot::Node3D* terrain_node() const noexcept { return terrain_; }
    [[nodiscard]] godot::Camera3D* camera() const noexcept { return camera_; }
    [[nodiscard]] godot::MeshInstance3D* sea() const noexcept { return sea_; }
    [[nodiscard]] godot::String status_text() const;
    [[nodiscard]] const terrain::FlyState& start_state() const noexcept { return start_; }
    [[nodiscard]] const terrain::FlyState& fly_state() const noexcept { return fly_; }
    // The first HUD line for a camera at (X, Y, Z), at the current speed and
    // the given frame rate.
    [[nodiscard]] std::string hud_at(float X, float Y, float Z, int fps) const;
    [[nodiscard]] bool terrain3d_wanted() const noexcept { return !no_terrain3d_; }
    [[nodiscard]] bool flat_colours_option() const noexcept { return flat_colours_; }
    [[nodiscard]] bool no_trees_option() const noexcept { return no_trees_; }
    [[nodiscard]] std::uint64_t instance_cap() const noexcept { return instance_cap_; }
    [[nodiscard]] bool trees_visible() const noexcept { return trees_visible_; }
    // The world shown, as opened.
    [[nodiscard]] const godot::String& current_path() const noexcept { return current_path_; }

    // Key T: vegetation on or off (enabled on every mesh asset). Key G:
    // reopen the world with the other ground, keeping the camera. Both do
    // nothing when they do not apply (V3D-02 10.2); they return whether
    // something changed.
    bool toggle_trees();
    bool toggle_ground();

    // Sea of V3D-02 7.
    static constexpr float SEA_SIDE = 800000.0f;
    static constexpr float CAMERA_FAR = 450000.0f;
    static constexpr float FOG_BEGIN = 200000.0f;
    static constexpr float FOG_END = 400000.0f;

    // Texts of section 7.
    static constexpr const char* NO_TERRAIN3D_TEXT =
        "Terrain3D is not in this build (configure with -DUNMAPPED_BUILD_TERRAIN3D=ON): showing the sea plane only.";
    static constexpr const char* NO_ELEVATION_TEXT =
        "This world has no elevation layer, so there is no terrain to show.";
    static constexpr const char* NO_WORLD_TEXT =
        "No world given: pass an absolute path after --, or use Open world...";

protected:
    static void _bind_methods();

private:
    void parse_options(const godot::PackedStringArray& args);
    void refuse(const godot::String& text);
    void clear_world();
    void read_optional_layer(const char* name, const char* type, std::vector<godot::String>& notes);
    void build_ground_assets();
    bool make_terrain();
    void add_vegetation();
    void place_sea();
    void place_start();
    void apply_camera();
    void show_status();
    void update_hud();
    void after_load();

    void on_open_pressed();
    void on_file_selected(const godot::String& path);
    void take_screenshot();
    void run_check_deferred();
    void bench_first_frame();
    void bench_view_frame();
    void bench_flight_frame();
    void start_flight();
    void on_first_drawn_frame();
    void finish_bench();

    godot::Camera3D* camera_ = nullptr;
    godot::MeshInstance3D* sea_ = nullptr;
    godot::Label* hud_ = nullptr;
    godot::Label* status_ = nullptr;
    godot::Button* open_button_ = nullptr;
    godot::FileDialog* open_dialog_ = nullptr;
    godot::Node3D* terrain_ = nullptr;

    godot::Ref<WorldFile> world_;
    TerrainScene scene_;
    godot::Ref<godot::Image> height_image_;
    godot::Ref<godot::Image> control_image_;
    godot::Ref<godot::Image> colour_image_;
    // Built once per process (they do not depend on the world).
    std::vector<godot::Ref<godot::ImageTexture>> albedo_textures_;
    std::vector<godot::Ref<godot::ImageTexture>> normal_textures_;
    std::vector<double> mean_grey_;
    std::array<godot::Ref<godot::PackedScene>, terrain::VEGETATION_KINDS> mesh_scenes_;
    std::vector<godot::String> status_lines_;
    TerrainTimings timings_;

    terrain::FlyState fly_;
    terrain::FlyState start_;
    float pending_yaw_ = 0.0f;
    float pending_pitch_ = 0.0f;
    bool mouse_look_ = false;
    bool boost_ = false;

    // Options.
    godot::String world_path_;
    std::optional<std::pair<int, int>> opt_at_;
    std::optional<float> opt_alt_;
    std::optional<float> opt_heading_;
    std::optional<float> opt_pitch_;
    std::optional<int> opt_speed_index_;
    godot::String screenshot_path_;
    bool bench_ = false;
    godot::String check_dir_;
    godot::String expect_terrain3d_;
    bool no_terrain3d_ = false;
    bool flat_colours_ = false;
    bool no_trees_ = false;
    std::uint64_t instance_cap_ = terrain::DEFAULT_INSTANCE_CAP;
    std::optional<float> opt_above_;
    bool trees_visible_ = true;
    godot::String current_path_;
    bool refused_ = false;
    bool scripted_ = false;  // --screenshot, --bench or --check

    // Bench.
    std::uint64_t import_end_usec_ = 0;
    std::uint64_t first_frame_usec_ = 0;
    bool flying_ = false;
    float flight_time_ = 0.0f;
    float flight_length_ = 0.0f;
    std::uint64_t last_frame_usec_ = 0;
    std::vector<std::uint64_t> frame_usec_;
    int view_frames_ = 0;
};

}  // namespace unmapped
