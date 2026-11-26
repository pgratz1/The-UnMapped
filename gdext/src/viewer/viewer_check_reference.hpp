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
    // P5-04a 12.4: one event of a same-day group line, which prints no
    // {...} and no causes; refs, region, settlement and causes are then the
    // check's own read of the event (event_refs), text is the group's.
    bool grouped = false;
};
// A same-day group line of [chronicle] (P5-04a 12.4): "<year>[-<end>]
// e<id>, e<id>[, ...] <key>/<salience>: <sentence>".
struct GroupLine {
    int64_t start = 0, end = 0, salience = 0;
    std::vector<int64_t> ids;
    std::string kind, text;
};
[[nodiscard]] bool parse_group_line(const std::string& line, GroupLine& out);
// The {...} tokens of an event as the report prints them, its region,
// settlement and causes, from the check's own HistoryView of the world.
struct EventRefs {
    int64_t region = 0, settlement = 0;
    std::vector<std::string> refs;
    std::vector<int64_t> causes;
};
// For each id (problem set when the world cannot be read or an id is not
// an event).
[[nodiscard]] std::map<int64_t, EventRefs> event_refs(const godot::String& world_path,
                                                      const std::vector<int64_t>& ids, std::string& problem);

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
// world_path: the world, read for the events of group lines (P5-04a 12.4).
[[nodiscard]] BrowserReport read_browser_report(const godot::String& path, const godot::String& world_path);

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
