#pragma once

// The browser of civilizations and events (spec P4-05 section 4): kind
// groups, the browser index over a loaded history, event lists by filter,
// one event's detail, a civilization's block as display lines, row texts and
// name search. The library answers, the viewer draws (P4-00 V3): everything
// the browser shows as text comes from here. Integers only; every list in
// ascending id. No SQL: everything comes from LoadedHistory.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/db/result.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace worldgen::inspect {

// --- Kind groups (4.4) ------------------------------------------------------

// Codes are never reused; a later group is appended (section 9).
enum class EventGroup : std::uint8_t { realms = 0, settlements = 1, war = 2, calamity = 3, other = 4, monsters = 5 };
inline constexpr std::uint32_t ALL_GROUPS = 0x3F;

// The group of a kind key, compared as bytes; other for a key 4.4 does not
// name. P5-02's kinds 44 to 55 are in monsters (section 9).
[[nodiscard]] EventGroup event_group(std::string_view kind_key) noexcept;
// "realms", "settlements", "war", "calamity", "other", "monsters".
[[nodiscard]] std::string_view event_group_word(EventGroup group) noexcept;

// --- The browser index (4.5) ------------------------------------------------

struct CivilizationEntry {
    std::int64_t id = 0, race = 0, founded_year = 0;
    std::int64_t ended_year = -1, ending_event = 0;  // -1 / 0 while alive
    std::int64_t events = 0;                         // CivilizationDigest::events
    std::string name, race_name;
    friend bool operator==(const CivilizationEntry&, const CivilizationEntry&) = default;
};

struct EventFilter {
    std::int64_t civilization = 0, settlement = 0, region = 0;  // 0: any; several: all must hold
    std::int64_t from_year = -1, to_year = -1;  // -1: open; kept when year <= to and end_year >= from
    std::uint32_t groups = ALL_GROUPS;          // bit (1 << group)
    bool summary_only = true;                   // the summary set of the report's digest
};

struct EventEntry {
    std::int64_t id = 0, year = 0, end_year = 0, kind = 0, salience = 0, region = 0, settlement = 0;
    EventGroup group = EventGroup::other;
    bool summary = false;
    friend bool operator==(const EventEntry&, const EventEntry&) = default;
};

struct Participant {
    std::string role;
    std::int64_t type = 0, id = 0;
    std::string name;
    friend bool operator==(const Participant&, const Participant&) = default;
};

struct EventDetail {
    EventEntry entry;
    std::string kind_key;
    history::LinkedText sentence;           // render_event_linked
    std::string note;                       // history_note(view, id, 0), unescaped
    std::vector<Participant> participants;  // seq order; name: a region's at the event, else HistoryView::name, else empty
    std::vector<std::pair<std::string, std::int64_t>> values;  // seq order
    std::vector<std::int64_t> causes;                          // ascending (HistoryView)
    std::vector<std::int64_t> effects;                         // ascending (Timeline::effects)
    friend bool operator==(const EventDetail&, const EventDetail&) = default;
};

struct NameMatch {
    std::int64_t type = 0, id = 0;
    std::string name;
    friend bool operator==(const NameMatch&, const NameMatch&) = default;
};

class BrowserIndex {
public:
    // The report's digest (history_report_digest), the civilization and
    // event entries, and per event its group and summary flag. std::bad_alloc:
    // out_of_memory "out of memory while building the browser". Holds no
    // reference into `history`.
    [[nodiscard]] static db::Result<BrowserIndex> build(const history::LoadedHistory& history);

    [[nodiscard]] std::span<const CivilizationEntry> civilizations() const noexcept { return civilizations_; }
    [[nodiscard]] std::span<const EventEntry> events() const noexcept { return events_; }
    [[nodiscard]] const history::HistoryDigest& digest() const noexcept { return digest_; }

    // The ids of the events the filter keeps, ascending; reuses out. The
    // candidates are the timeline's list of the first non-zero entity field
    // (civilization, settlement, region), else every event; each is kept when
    // it is in every other non-zero entity's list, in the year span, in
    // `groups` and, with summary_only, in the summary set. FatalError
    // "browser: no civilization <c>" / "no settlement <s>" / "no region <r>"
    // for an id the world lacks.
    void select(const history::LoadedHistory& history, const EventFilter& filter,
                std::vector<std::int64_t>& out) const;
    // FatalError "browser: no event <e>".
    [[nodiscard]] EventDetail event_detail(const history::LoadedHistory& history, std::int64_t event) const;
    // block_line_display of each line of civilization_block at `level`.
    // FatalError "browser: no civilization <c>", "browser: bad level".
    [[nodiscard]] std::vector<history::LinkedText> block(const history::LoadedHistory& history, std::int64_t civ,
                                                         HistoryLevel level) const;
    // "<span>  <sentence>[ (<note>)]", unescaped; span "<year>" or
    // "<year>-<end_year>". FatalError "browser: no event <e>".
    [[nodiscard]] std::string row_text(const history::LoadedHistory& history, std::int64_t event) const;
    // Names containing `query` as a substring, ASCII case-insensitive (A-Z
    // folded only): civilizations by id, settlements by id, then regions by
    // id (name at E, geographic name, then its region_name rows by ascending
    // from_year; the first that matches). One match per (type, id), at most
    // `limit`; none for an empty query.
    [[nodiscard]] std::vector<NameMatch> search(const history::LoadedHistory& history, std::string_view query,
                                                std::size_t limit) const;

private:
    void check_history(const history::LoadedHistory& history) const;
    [[nodiscard]] const EventEntry& entry(std::int64_t event) const;

    history::HistoryDigest digest_;
    std::vector<CivilizationEntry> civilizations_;
    std::vector<EventEntry> events_;
};

}  // namespace worldgen::inspect
