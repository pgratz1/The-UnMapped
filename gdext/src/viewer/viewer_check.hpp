#pragma once

// Headless check of the viewer (gdext/README.md, "Headless check").
//
// Run by the real viewer scene when it is started with `-- <world.db> --check
// <reference dir>`. It drives the screen as a user would (the world is
// opened through the screen, each layer chosen through the layer list) and
// checks:
//   - the scene has every part the screen needs and a world opened;
//   - the layer list shows every layer of the file (and, with
//     --expect-layers, that many) and the meta panel every meta row;
//   - for each layer, the image handed to the map view equals, pixel for
//     pixel, <reference dir>/<layer name>.png written by
//     `worldgen-cli render --palette auto --scale 1`;
//   - opening a missing file and a non-SQLite file give the reader's
//     distinct errors (file_unreadable, not_sqlite) with a message.
// Prints one line per check and "UNMAPPED-CHECK: PASS" or "UNMAPPED-CHECK:
// FAIL", and returns the process exit code (0 or 1).

#include <cstdint>

#include <godot_cpp/variant/string.hpp>

namespace unmapped {

class ViewerScreen;

struct ViewerCheckOptions {
    godot::String reference_dir;
    int64_t expect_layers = -1;  // -1: not checked
    bool wired = false;
};

[[nodiscard]] int run_viewer_check(ViewerScreen& screen, const ViewerCheckOptions& options);

}  // namespace unmapped
