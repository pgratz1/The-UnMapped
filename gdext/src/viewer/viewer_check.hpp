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
//     distinct errors (file_unreadable, not_sqlite) with a message;
//   - with --expect-history none, the checks N1 to N3 of spec P4-03 10.2
//     (no territory view, the year bar disabled, no holding in the hover
//     text); with --expect-history loaded, H1 to H8 (the year bar, the
//     territory image at each year of --expect-years against
//     territory-<Y>.png, the readouts and the legend against the history
//     report, steps, keys, hover, colours, year marks);
//   - markers (spec P4-04 9.2): M0 without history; M1 to M9 with it (the
//     marker set against the history report, the pixels of the model's set
//     drawn by worldgen against <dir>/markers-<Y>.png for --expect-marker-years,
//     hover texts and mouse motions, toggles, markers_changed per year).
// Prints one line per check and "UNMAPPED-CHECK: PASS" or "UNMAPPED-CHECK:
// FAIL", and returns the process exit code (0 or 1).

#include <cstdint>
#include <vector>

#include <godot_cpp/variant/string.hpp>

namespace unmapped {

class ViewerScreen;

struct ViewerCheckOptions {
    godot::String reference_dir;
    int64_t expect_layers = -1;      // -1: not checked
    int64_t expect_composites = -1;  // -1: not checked
    // P4-03 10.2: "none" (checks N1 to N3), "loaded" (H1 to H8) or empty.
    godot::String expect_history;
    int64_t expect_end_year = -1;
    std::vector<int64_t> expect_years;  // compared with <reference dir>/territory-<Y>.png
    godot::String history_report;       // `report --history --level full` of the world
    std::vector<int64_t> marker_years;  // P4-04 9: compared with <reference dir>/markers-<Y>.png
    int64_t expect_passes = -1;         // the world report's passes; -1: not checked
    bool wired = false;
};

[[nodiscard]] int run_viewer_check(ViewerScreen& screen, const ViewerCheckOptions& options);

}  // namespace unmapped
