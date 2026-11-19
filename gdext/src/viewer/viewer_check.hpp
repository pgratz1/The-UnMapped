#pragma once

// Headless check of the viewer (gdext/README.md, "Headless check").
//
// Run by the real viewer scene when it is started with `-- <world.db> --check
// <reference dir>`. It drives the screen as a user would (the world is
// opened through the screen, each view chosen through the layer list) and
// checks:
//   - the scene has every part the screen needs and a world opened;
//   - the layer list shows every view: every layer of the file (and, with
//     --expect-layers, that many) and every composite (with
//     --expect-composites, that many); the meta panel every meta row;
//   - a world with a `biome` layer has its table `biome` read;
//   - for each layer, the image handed to the map view equals, pixel for
//     pixel, <reference dir>/<layer name>.png written by
//     `worldgen-cli render --palette auto --scale 1`; for each composite,
//     <reference dir>/composite.<name>.png written by `worldgen-cli render
//     --composite <name>` (map, biomes, regions);
//   - the layer list groups the views under non-selectable headings,
//     composites first, and the world opens on the first entry;
//   - every view has a legend; for `biome`, `biomes` and `regions` it names
//     every (land) row of table `biome` with its colour;
//   - hovering a cell of a region describes it: coordinates, elevation in
//     metres, biome name, region id and name from table `region`;
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
    int64_t expect_layers = -1;      // -1: not checked
    int64_t expect_composites = -1;  // -1: not checked
    bool wired = false;
};

[[nodiscard]] int run_viewer_check(ViewerScreen& screen, const ViewerCheckOptions& options);

}  // namespace unmapped
