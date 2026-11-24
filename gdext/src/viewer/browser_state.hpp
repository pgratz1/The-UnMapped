#pragma once

// BrowserState: what the viewer model holds of the browser of civilizations
// and events (spec P4-05 5.1). C++ only, not registered with Godot, no Godot
// types; owned by ViewerModel beside HistoryState.
//
// Built once when a world opens, after HistoryState loaded the history and
// the markers: worldgen's BrowserIndex (the report's digest, the entries,
// the groups and summary flags) and the row text of every event. Every list,
// text, link target and place comes from worldgen (P4-00 V3): the browser
// parses no report text, issues no SQL and tests no kind by id.
//
// It holds, across years: the selection (an entity, an event, a place), the
// filter of the event list, the current id list (worldgen's select), follow,
// the path (back and forward stacks of selections with their years, at most
// PATH_LIMIT entries) and the highlight of 5.4. The highlight depends on Y
// and is recomputed on a year change; it is compared, not cached (V6).

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace unmapped {

class HistoryState;

// Entity type codes of worldgen's EntityType used by the browser.
inline constexpr std::int64_t BROWSER_CIVILIZATION = 3;
inline constexpr std::int64_t BROWSER_SETTLEMENT = 4;
inline constexpr std::int64_t BROWSER_REGION = 5;

// 5.1: what is selected; 0 means none. The entity is a civilization (3) now;
// monsters (8) and sites (9) attach later (section 9), so nothing here
// assumes the entity is a civilization except BrowserState::entity_filter.
struct BrowserSelection {
    std::int64_t entity_type = 0, entity_id = 0;
    std::int64_t event = 0;
    std::int64_t place_type = 0, place_id = 0;  // 4 settlement, 5 region
    friend bool operator==(const BrowserSelection&, const BrowserSelection&) = default;
};

enum class BrowserScope : std::uint8_t { all = 0, entity = 1, place = 2 };

// 5.1: the filter of the event list. group -1 is every group, else an
// EventGroup code.
struct BrowserFilter {
    BrowserScope scope = BrowserScope::all;
    int group = -1;
    worldgen::inspect::HistoryLevel level = worldgen::inspect::HistoryLevel::summary;
    std::int64_t from = -1, to = -1;  // -1: open
    bool this_year = false;           // the span is [Y, Y]
};

// 5.4: the highlight at Y.
struct BrowserHighlight {
    std::int64_t civilization = 0;             // the selected civilization, 0 none
    std::vector<std::int64_t> regions;         // held by it at Y (or the selected region), ascending
    std::vector<std::int64_t> settlements;     // its standing marker entries at Y, ascending id
    bool dim = false;                          // the land outside `regions` is dimmed
    std::array<std::int64_t, 2> place{-1, -1};  // the selected event's place while Y is in its span
    std::int64_t place_event = 0;              // the event of `place`, 0 none
    std::int64_t place_settlement = 0;         // the selected place settlement, 0 none
    friend bool operator==(const BrowserHighlight&, const BrowserHighlight&) = default;
};

// One step of the path: the selection, the filter scope and the year.
struct BrowserPathEntry {
    BrowserSelection selection;
    BrowserScope scope = BrowserScope::all;
    std::int64_t year = 0;
};

class BrowserState {
public:
    static constexpr std::size_t PATH_LIMIT = 100;

    // Builds the index and the row texts (5.1). Never throws: a failure
    // leaves the browser unavailable with worldgen's code and message.
    void load(const worldgen::history::LoadedHistory& history);
    void reset();

    [[nodiscard]] bool available() const noexcept { return index_.has_value(); }
    [[nodiscard]] const std::string& error_code() const noexcept { return error_code_; }
    [[nodiscard]] const std::string& error_message() const noexcept { return error_message_; }
    // Valid only while available().
    [[nodiscard]] const worldgen::inspect::BrowserIndex& index() const { return *index_; }
    // The time load took (index and row texts), microseconds.
    [[nodiscard]] std::int64_t load_usec() const noexcept { return load_usec_; }

    // The entry of an event; null for an id the index lacks.
    [[nodiscard]] const worldgen::inspect::EventEntry* event(std::int64_t id) const noexcept;
    [[nodiscard]] const worldgen::inspect::CivilizationEntry* civilization(std::int64_t id) const noexcept;
    // The row text of an event (worldgen's row_text, built at load); empty
    // for an unknown id.
    [[nodiscard]] const std::string& row(std::int64_t id) const noexcept;

    // --- State ---------------------------------------------------------------
    BrowserSelection selection;
    BrowserFilter filter;
    bool follow = false;

    // worldgen's EventFilter for the filter and selection at year Y. Returns
    // false when the list is empty by construction (scope entity or place
    // with nothing selected).
    [[nodiscard]] bool event_filter(std::int64_t year, worldgen::inspect::EventFilter& out) const;
    // Recomputes the id list; true when it differs from the one before.
    // FatalError from worldgen passes through (a caller's bad id).
    bool refresh_list(const worldgen::history::LoadedHistory& history, std::int64_t year);
    [[nodiscard]] const std::vector<std::int64_t>& ids() const noexcept { return ids_; }
    // The time of the last refresh_list, nanoseconds.
    [[nodiscard]] std::int64_t last_select_nsec() const noexcept { return last_select_nsec_; }

    // 5.3: the event a follow step in `direction` (-1, +1) selects at year
    // Y, or nullopt. Only meaningful while follow is on and the list is not
    // empty.
    [[nodiscard]] std::optional<std::int64_t> follow_target(std::int64_t year, int direction) const;
    // The first row of the list whose event starts in a year >= Y (6.3; a
    // binary search over the ids), or the row count.
    [[nodiscard]] std::size_t first_row_at_or_after(std::int64_t year) const;

    // --- Highlight (5.4) -----------------------------------------------------
    // Recomputes the highlight of Y from the history state (holders, the
    // current markers, event places); true when it differs from the one
    // before. `regions_changed` tells whether the dimmed set (regions, dim)
    // changed, which alone needs a new mask.
    bool refresh_highlight(const HistoryState& history, bool& regions_changed);
    [[nodiscard]] const BrowserHighlight& highlight() const noexcept { return highlight_; }

    // --- Path ----------------------------------------------------------------
    // Pushes the current state (selection, scope, year) onto the back stack
    // and clears the forward stack; the oldest entry goes beyond PATH_LIMIT.
    void push_path(std::int64_t year);
    [[nodiscard]] bool can_back() const noexcept { return !back_.empty(); }
    [[nodiscard]] bool can_forward() const noexcept { return !forward_.empty(); }
    // Moves one step back (forward): the current state goes onto the other
    // stack and the entry restored is returned; nullopt when there is none.
    std::optional<BrowserPathEntry> back(std::int64_t year);
    std::optional<BrowserPathEntry> forward(std::int64_t year);

private:
    std::optional<worldgen::inspect::BrowserIndex> index_;
    std::string error_code_;
    std::string error_message_;
    std::int64_t load_usec_ = 0;
    std::vector<std::string> rows_;  // parallel to index_->events()
    std::vector<std::int64_t> ids_;
    std::vector<std::int64_t> scratch_;
    std::int64_t last_select_nsec_ = 0;
    BrowserHighlight highlight_;
    BrowserHighlight scratch_highlight_;
    std::vector<BrowserPathEntry> back_;
    std::vector<BrowserPathEntry> forward_;
};

// The report's tag (c3, s12, rg4, e57, ...) of an entity type code and id,
// type 0 being an event.
[[nodiscard]] std::string browser_target(std::int64_t type, std::int64_t id);
// Parses a target: prefix letters and a decimal id; false otherwise.
[[nodiscard]] bool parse_browser_target(std::string_view target, std::string& prefix, std::int64_t& id);

}  // namespace unmapped
