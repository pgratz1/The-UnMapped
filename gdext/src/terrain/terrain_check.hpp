#pragma once

// The terrain check of the 3D preview (spec V3D-01 9.2, checks T1 to T8),
// run by `godot --headless ... res://viewer3d/terrain_preview.tscn --
// <world> --check <dir> --expect-terrain3d present|absent` from
// gdext/cmake/terrain_check.cmake. It prints one line per check and ends
// with UNMAPPED-TERRAIN-CHECK: PASS or FAIL. It has its own copy of the
// formulas of 5.1 to 5.3 and compares the preview's buffers and images with
// them, with worldgen's `render --layer biome` image (<dir>/biome.png), and
// with what Terrain3D reports back.

#include <memory>

#include <godot_cpp/variant/string.hpp>

namespace unmapped {

class TerrainPreview;

struct TerrainCheckOptions {
    godot::String reference_dir;
    godot::String expect_terrain3d;  // "present" or "absent"
    godot::String world_path;
};

// The part of the check that runs over frames (spec V3D-03 11.2, T17 to
// T24: the camera path). The preview calls before_frame before its near
// tier's frame and after_frame after it, every frame, until after_frame
// returns false; then exit_code() is the exit status.
class TerrainCheckRun {
public:
    TerrainCheckRun() = default;
    TerrainCheckRun(const TerrainCheckRun&) = delete;
    TerrainCheckRun& operator=(const TerrainCheckRun&) = delete;
    virtual ~TerrainCheckRun() = default;
    virtual void before_frame(TerrainPreview& preview) = 0;
    virtual bool after_frame(TerrainPreview& preview) = 0;
    [[nodiscard]] virtual int exit_code() const = 0;
};

// Runs the checks on the preview's current world; returns the exit status
// (0 pass, 1 fail). When path is given and the run has a near-tier path to
// fly (V3D-03), *path receives it and the final PASS or FAIL line is left
// to it; the return value is then -1.
int run_terrain_check(TerrainPreview& preview, const TerrainCheckOptions& options,
                      std::unique_ptr<TerrainCheckRun>* path = nullptr);

}  // namespace unmapped
