#pragma once

// Shared by the terrain check's two files: terrain_check.cpp (V3D-01 T1 to
// T8) and terrain_check_ground.cpp (V3D-02 T3 default mode, T9 to T16).

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace unmapped {

class TerrainPreview;
struct TerrainCheckOptions;

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

}  // namespace unmapped
