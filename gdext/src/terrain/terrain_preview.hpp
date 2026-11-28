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
//   --instance-cap <n>      the far budget, 0 to 4000000 (default 1000000;
//                           with --veg-whole-map V3D-02's cap, default 2000000)
//   --above <m>             start m metres above the ground at the start cell
//   --veg-radius <m>        near tier radius R: 0 or 1536 to 8192 (default 2048)
//   --veg-whole-map         V3D-02's whole-map placement, no tiers
//   --veg-threads <n>       worker tasks in flight, 0 to 8 (default 4)
//   --tier-colours          near-tier instances tinted red
//   --at-forest             start at the bench's forest path, 60 m up (at least 60 m above the sea)
//   --look-v3d03 <list>     the named items of V3D-04 (rock, light, snow,
//                           shrubs, sea, or all) drawn as in V3D-03
//   --at-peak               start 2,000 m south of the highest land cell,
//                           200 m above the peak, looking north (V3D-04 9.1)
//   --year <Y>              the markers of year Y (default the history's end)
//   --no-markers            the history is not read; no markers (V3D-05 8.1)
//   --at-marker <v>         start at the jump view of a marker: settlement:<id>,
//                           site:<id>, rank:<k> or a name
//
// Keys of V3D-02 10.2: T vegetation on or off, G ground textured or flat.
// V3D-05 8.2: M markers on or off, N and Shift+N the next and previous
// settlement of the jump order (terrain_preview_markers.cpp).
//
// Vegetation in tiers (spec V3D-03): the far tier (trees and snags thinned
// to the budget) goes to Terrain3D's instancer once at load; the near tier
// fills 256 m cells around the camera with the instances the far tier left
// out, computed on WorkerThreadPool tasks (terrain_preview_near.cpp) and
// drawn with the preview's own MultiMeshInstance3D nodes under the child
// NearVegetation.

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/file_dialog.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include "terrain/fly_state.hpp"
#include "terrain/ground.hpp"
#include "terrain/look.hpp"
#include "terrain/markers3d.hpp"
#include "terrain/near_cells.hpp"
#include "terrain/terrain_check.hpp"
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
    bool tiers = false;                      // V3D-03 tiers (else V3D-02's whole-map placement)
    terrain::Placement placement;            // given to Terrain3D: the far tier in tier mode
    std::uint64_t far_threshold = 0;         // T (tiers)
    std::uint64_t far_candidates = 0;        // S_far (tiers)
    int forest_column = 0;                   // x* of V3D-03 12.1
    terrain::ForestStart forest;             // the forest path's start cell and direction (12.1, lead's ruling)
    terrain::PeakView peak;                  // V3D-04 8: the start of --at-peak
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

// The near tier's work in the last frame (V3D-03 7.1), read by the check
// and the bench.
struct NearFrameStats {
    std::vector<int> attached;
    std::vector<int> dropped;
    std::uint64_t in_flight = 0;    // tasks running after step 6
    std::uint64_t attach_usec = 0;  // step 5
};

// One worker task's slot: what it reads (set by the main thread before the
// task starts, unchanged while it runs) and what it writes (read by the main
// thread only after the task has completed).
struct NearSlot {
    bool busy = false;
    std::int64_t task = -1;
    int cell = -1;
    std::uint64_t generation = 0;
    std::uint64_t threshold = 0;
    const terrain::VegetationInputs* inputs = nullptr;
    int m = 0;
    std::uint8_t tint_mode = 0;
    std::array<std::vector<float>, terrain::VEGETATION_KINDS> buffers;
    std::array<std::uint32_t, terrain::VEGETATION_KINDS> counts{};
    std::uint64_t usec = 0;
};

// A computed cell's result, held until it is attached or released.
struct NearResult {
    std::array<std::vector<float>, terrain::VEGETATION_KINDS> buffers;
    std::array<std::uint32_t, terrain::VEGETATION_KINDS> counts{};
};

class TerrainPreview : public godot::Node3D {
    GDCLASS(TerrainPreview, godot::Node3D)

public:
    TerrainPreview() = default;

    ~TerrainPreview() override;

    void _ready() override;
    void _exit_tree() override;
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
    // V3D-03.
    [[nodiscard]] bool whole_map_option() const noexcept { return veg_whole_map_; }
    [[nodiscard]] double veg_radius() const noexcept { return veg_radius_; }
    [[nodiscard]] int veg_threads() const noexcept { return veg_threads_; }
    [[nodiscard]] bool tier_colours_option() const noexcept { return tier_colours_; }
    [[nodiscard]] const terrain::NearCells* near_cells() const noexcept { return near_.get(); }
    [[nodiscard]] godot::Node3D* near_root() const noexcept { return near_root_; }
    [[nodiscard]] const NearFrameStats& near_stats() const noexcept { return near_stats_; }
    [[nodiscard]] const terrain::VegetationInputs& vegetation_inputs() const noexcept { return veg_inputs_; }
    // V3D-04.
    [[nodiscard]] const terrain::Look& look() const noexcept { return look_; }
    [[nodiscard]] bool at_peak_option() const noexcept { return at_peak_; }
    [[nodiscard]] godot::DirectionalLight3D* sun() const noexcept { return sun_; }
    // The mesh of a vegetation kind and LOD as built (shared by Terrain3D's
    // mesh asset and the near tier's nodes); null before it is built.
    [[nodiscard]] godot::Ref<godot::ArrayMesh> vegetation_mesh(int kind, int lod) const;
    // V3D-04 9.2: the bench's look line (without the UNMAPPED-TERRAIN-BENCH
    // prefix); empty without ground cells.
    [[nodiscard]] std::string look_bench_line() const;
    // The HUD's vegetation line (9.2).
    [[nodiscard]] std::string vegetation_hud_line() const;
    // Places the camera (the check's path).
    void set_fly_state(const terrain::FlyState& state);
    // The start of the forest path of 12.1 (60 m above the ground and at
    // least 60 m above the sea, heading 0 or 180).
    [[nodiscard]] terrain::FlyState forest_start() const;
    // The world shown, as opened.
    [[nodiscard]] const godot::String& current_path() const noexcept { return current_path_; }

    // V3D-05 (terrain_preview_markers.cpp).
    [[nodiscard]] const terrain::MarkerLoad& marker_load() const noexcept { return marker_load_; }
    [[nodiscard]] const std::vector<terrain::Marker3D>& markers() const noexcept { return markers_; }
    [[nodiscard]] const std::vector<std::size_t>& marker_jump_order() const noexcept { return jump_order_; }
    [[nodiscard]] const std::vector<std::size_t>& shown_labels() const noexcept { return shown_labels_; }
    // Per shown label: drawn, or hidden by the declutter (markers3d.hpp).
    [[nodiscard]] const std::vector<bool>& labels_drawn() const noexcept { return labels_drawn_; }
    [[nodiscard]] godot::Node3D* markers_root() const noexcept { return markers_root_; }
    [[nodiscard]] bool no_markers_option() const noexcept { return no_markers_; }
    [[nodiscard]] std::optional<std::int64_t> year_option() const noexcept { return opt_year_; }
    [[nodiscard]] const godot::String& at_marker_option() const noexcept { return at_marker_; }
    [[nodiscard]] int jump_position() const noexcept { return jump_pos_; }
    // The viewport height the beacons and labels are computed for.
    [[nodiscard]] int marker_viewport_height() const;
    // Beacons and labels for a camera at (X, Y, Z) (10), and the HUD's
    // marker line; nothing while Markers is hidden.
    void markers_frame(float X, float Y, float Z);
    // HUD line 3 (8.2).
    [[nodiscard]] std::string markers_hud_text() const;
    // Key M: Markers visible or not; false when nothing is loaded.
    bool toggle_markers();
    // Keys N (+1) and Shift+N (-1): the camera to the next or previous entry
    // of the jump order; false when there is none.
    bool jump_marker(int direction);
    // The label pixel size (W26) and the beacon quad's draw box.
    static constexpr float LABEL_PIXEL_SIZE = 0.0017f;
    static constexpr int LABEL_FONT_SIZE = 20;
    static constexpr int LABEL_OUTLINE_SIZE = 6;
    static constexpr float LABEL_OFFSET_PX = 24.0f;
    static constexpr float BEACON_TOP = 30000.0f;

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
    void apply_light();
    void place_start();
    void apply_camera();
    void show_status();
    void update_hud();
    void after_load();

    // Markers (terrain_preview_markers.cpp).
    void markers_open();
    void markers_close();
    void markers_build_nodes();
    void print_markers_bench_line();
    void print_markers_frame_line();

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

    // Near tier (terrain_preview_near.cpp).
    void near_open();
    void near_close();
    void near_frame();
    void near_attach(int q, NearResult& result);
    void near_free_nodes(int q);
    void compute_near_slot(int slot);
    void finish_slot(NearSlot& slot);
    [[nodiscard]] bool near_settled() const;
    // Bench additions (12).
    void bench_phases(float dt);
    void start_forest_flight(int index);
    void print_tiers_line();
    void finish_forest_flight();
    godot::Camera3D* camera_ = nullptr;
    godot::MeshInstance3D* sea_ = nullptr;
    godot::Label* hud_ = nullptr;
    godot::Label* status_ = nullptr;
    godot::Button* open_button_ = nullptr;
    godot::FileDialog* open_dialog_ = nullptr;
    godot::Node3D* terrain_ = nullptr;
    godot::DirectionalLight3D* sun_ = nullptr;

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
    std::array<std::array<godot::Ref<godot::ArrayMesh>, 2>, terrain::VEGETATION_KINDS> veg_meshes_;
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
    bool instance_cap_given_ = false;
    bool veg_whole_map_ = false;
    bool veg_radius_given_ = false;
    double veg_radius_ = terrain::NEAR_RADIUS_DEFAULT;
    int veg_threads_ = 4;
    bool tier_colours_ = false;
    bool at_forest_ = false;
    bool at_peak_ = false;
    terrain::Look look_;
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

    // Near tier.
    terrain::VegetationInputs veg_inputs_;
    std::unique_ptr<terrain::NearCells> near_;
    godot::Node3D* near_root_ = nullptr;
    std::array<NearSlot, 8> near_slots_;
    std::uint64_t near_generation_ = 0;
    bool exiting_ = false;  // in _exit_tree: the near nodes go with the tree
    std::map<int, NearResult> near_results_;
    std::map<int, std::vector<godot::MultiMeshInstance3D*>> near_nodes_;
    NearFrameStats near_stats_;
    std::uint64_t compute_usec_sum_ = 0;
    std::uint64_t compute_usec_max_ = 0;
    std::uint64_t compute_cells_ = 0;
    std::uint64_t attach_usec_max_ = 0;

    // Markers (V3D-05).
    std::optional<std::int64_t> opt_year_;
    bool no_markers_ = false;
    godot::String at_marker_;
    terrain::MarkerLoad marker_load_;
    std::vector<terrain::Marker3D> markers_;
    std::vector<std::size_t> jump_order_;
    std::optional<std::size_t> at_marker_index_;
    std::vector<godot::String> marker_status_;
    int jump_pos_ = -1;
    bool markers_on_ = true;  // key M; kept across key G
    godot::Node3D* markers_root_ = nullptr;
    godot::MultiMeshInstance3D* beacons_ = nullptr;
    std::array<godot::Label3D*, terrain::LABEL_KEEP> labels_{};
    std::vector<std::size_t> shown_labels_;
    std::vector<bool> labels_drawn_;
    std::vector<std::size_t> beacon_index_;  // record index of each beacon instance
    std::string marker_hud_;                 // the line of the last markers_frame
    godot::Ref<godot::StandardMaterial3D> marker_material_;
    godot::Ref<godot::StandardMaterial3D> beacon_material_;
    std::uint64_t marker_mesh_usec_ = 0;
    std::uint64_t marker_node_usec_ = 0;
    std::uint64_t marker_place_usec_ = 0;
    std::uint64_t marker_triangles_ = 0;
    bool marker_bench_view_ = false;
    std::vector<std::uint64_t> marker_frame_usec_;

    // The check's path (terrain_check_near.cpp), driven frame by frame.
    std::unique_ptr<TerrainCheckRun> check_run_;

    // Screenshot and bench phases of V3D-03.
    enum class Phase { none, screenshot_settle, headless_settle, view_settle, forest_settle, forest_fly };
    Phase phase_ = Phase::none;
    // Near frames run since the phase began: a settle test counts only after
    // the near tier has seen the camera where the phase put it.
    int phase_frames_ = 0;
    std::uint64_t phase_start_usec_ = 0;
    std::uint64_t view_start_usec_ = 0;
    std::uint64_t settled_usec_ = 0;  // from the first frame at the view point; 0 until settled
    bool settled_seen_ = false;
    int forest_index_ = 0;            // 0: 200 m/s, 1: 800 m/s
    float forest_speed_ = 0.0f;
    float forest_length_ = 0.0f;
    float forest_done_ = 0.0f;
    std::uint64_t forest_filled_ = 0;
    std::uint64_t forest_dropped_ = 0;
    std::uint64_t forest_late0_ = 0;
    std::uint64_t forest_pending_max_ = 0;
    std::uint64_t forest_t0_ = 0;
    bool forest_recording_ = false;
};

}  // namespace unmapped
