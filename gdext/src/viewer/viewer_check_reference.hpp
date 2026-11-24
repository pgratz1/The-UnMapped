#pragma once

// The headless check's own references (spec P4-05 8.1), shared by the
// browser checks (B0 to B12) and the playback checks (P0 to P10): the
// history reports as the check reads them, and the check's own read of the
// world through worldgen. Never the viewer's model.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <godot_cpp/variant/string.hpp>

#include "worldgen/db/reader.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/markers.hpp"

namespace unmapped::check_ref {

struct ChronicleLine {
    int64_t id = 0, start = 0, end = 0, salience = 0, region = 0, settlement = 0;
    std::string kind, text;          // text: between "<key>/<salience>: " and " {"
    std::vector<std::string> refs;   // the tokens inside {...}
    std::vector<int64_t> causes;
};

struct BlockText {
    int64_t id = 0, race = 0, founded = -1, ended = -1, events = -1;
    std::string name;
    std::vector<std::string> lines;  // as printed, the first "c<id> <name>"
};

struct BrowserReport {
    std::string problem;
    std::vector<ChronicleLine> chronicle;  // ascending id
    std::vector<BlockText> blocks;         // report order
    std::map<int64_t, std::pair<int64_t, int64_t>> settlement_cells;  // [present]
};

// `report --history` at either level.
[[nodiscard]] BrowserReport read_browser_report(const godot::String& path);

// The check's own read of the world.
struct OwnWorld {
    std::optional<worldgen::db::WorldReader> reader;
    std::optional<worldgen::history::LoadedHistory> history;
    std::optional<worldgen::inspect::MarkerIndex> markers;
    std::optional<worldgen::inspect::BrowserIndex> browser;
    std::map<int64_t, std::pair<int64_t, int64_t>> seeds;
    std::string problem;
};

[[nodiscard]] OwnWorld open_own_world(const godot::String& path);

}  // namespace unmapped::check_ref
