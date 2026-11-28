#pragma once

// Shared by the terrain check's files: terrain_check.cpp (V3D-01 T1 to
// T8), terrain_check_ground.cpp (V3D-02 T3 default mode, T9 to T16) and
// terrain_check_near.cpp (V3D-03 T17 to T24).

#include <memory>

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace unmapped {

class TerrainPreview;
struct TerrainCheckOptions;
class TerrainCheckRun;

struct TerrainChecker {
    bool all = true;
    void check(bool ok, const godot::String& what) {
        all = all && ok;
        godot::UtilityFunctions::print(godot::String(ok ? "  ok    " : "  FAIL  ") + what);
    }
    static void info(const godot::String& text) { godot::UtilityFunctions::print("UNMAPPED-TERRAIN-CHECK: " + text); }
};

// V3D-02: the default mode's colours (T3), textures (T9), control (T10),
// mesh assets (T11), instances (T12), placement (T13), sea (T14) and
// status (T15). Run before T16, which reopens the world.
void run_ground_checks(TerrainPreview& preview, const TerrainCheckOptions& options, TerrainChecker& c);

// V3D-02 T16: keys G and T. Reopens the world (twice, ending in the mode it
// started in).
void run_key_checks(TerrainPreview& preview, const TerrainCheckOptions& options, TerrainChecker& c);

// V3D-03: T17 at once, and for a run with vegetation in a Terrain3D node
// the path of 11.2 (T18 to T24) as a run over frames that takes over c's
// result; null when there is no path to fly.
std::unique_ptr<TerrainCheckRun> start_near_check(TerrainPreview& preview, const TerrainCheckOptions& options,
                                                  TerrainChecker& c);

}  // namespace unmapped
