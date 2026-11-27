#pragma once

// The terrain check of the 3D preview (spec V3D-01 9.2, checks T1 to T8),
// run by `godot --headless ... res://viewer3d/terrain_preview.tscn --
// <world> --check <dir> --expect-terrain3d present|absent` from
// gdext/cmake/terrain_check.cmake. It prints one line per check and ends
// with UNMAPPED-TERRAIN-CHECK: PASS or FAIL. It has its own copy of the
// formulas of 5.1 to 5.3 and compares the preview's buffers and images with
// them, with worldgen's `render --layer biome` image (<dir>/biome.png), and
// with what Terrain3D reports back.

#include <godot_cpp/variant/string.hpp>

namespace unmapped {

class TerrainPreview;

struct TerrainCheckOptions {
    godot::String reference_dir;
    godot::String expect_terrain3d;  // "present" or "absent"
    godot::String world_path;
};

// Runs the checks on the preview's current world; returns the exit status
// (0 pass, 1 fail).
int run_terrain_check(TerrainPreview& preview, const TerrainCheckOptions& options);

}  // namespace unmapped
