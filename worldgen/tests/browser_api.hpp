#pragma once

// The P4-05 Part A library API (docs/specs/p4-05-browser.md r1 section 4) as the browser tests use it: one
// adapter, so that the integration with the engineer's branch fixes names here only. Everything below is the
// spec's signature; where the spec leaves a name or namespace open, the reading taken is noted.
//
// Assumed (spec 4.1 to 4.6):
//   worldgen/history/event_catalogue.hpp   worldgen::history::TextLink, LinkedText, render_event_linked(view, id)
//   worldgen/inspect/history_report.hpp    worldgen::inspect::SegmentKind, Segment, BlockLine,
//                                          civilization_block(view, tables, digest, civ, level),
//                                          block_line_report_text(view, line), block_line_display(view, line),
//                                          history_report_digest(view, tables)
//   worldgen/inspect/history_browser.hpp   worldgen::inspect::EventGroup, ALL_GROUPS, event_group,
//                                          event_group_word, CivilizationEntry, EventFilter, EventEntry,
//                                          Participant, EventDetail, NameMatch, BrowserIndex
//   worldgen/inspect/markers.hpp           MarkerIndex::event_place(history, event)
// Reading (namespaces): the 4.2 and 4.5 declarations name history:: types unqualified-by-worldgen, so they live
// in worldgen::inspect beside HistoryLevel; TextLink and LinkedText in worldgen::history (4.1's header).
//
// Written from the spec only.

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/history_report.hpp"
#include "worldgen/inspect/markers.hpp"

namespace brapi {

namespace wh = worldgen::history;
namespace wi = worldgen::inspect;

// 4.1
using wh::LinkedText;
using wh::TextLink;
inline LinkedText render_event_linked(const wh::HistoryView& view, std::int64_t event) {
    return wh::render_event_linked(view, event);
}

// 4.2
using wi::BlockLine;
using wi::HistoryLevel;
using wi::Segment;
using wi::SegmentKind;
inline std::vector<BlockLine> civilization_block(const wh::LoadedHistory& h, const wh::HistoryDigest& digest,
                                                 std::int64_t civ, HistoryLevel level) {
    return wi::civilization_block(h.view, h.tables, digest, civ, level);
}
inline std::string block_line_report_text(const wh::LoadedHistory& h, const BlockLine& line) {
    return wi::block_line_report_text(h.view, line);
}
inline LinkedText block_line_display(const wh::LoadedHistory& h, const BlockLine& line) {
    return wi::block_line_display(h.view, line);
}
inline wh::HistoryDigest history_report_digest(const wh::LoadedHistory& h) {
    return wi::history_report_digest(h.view, h.tables);
}

// 4.4
using wi::ALL_GROUPS;
using wi::EventGroup;
inline EventGroup event_group(std::string_view key) noexcept { return wi::event_group(key); }
inline std::string_view event_group_word(EventGroup g) noexcept { return wi::event_group_word(g); }

// 4.5
using wi::BrowserIndex;
using wi::CivilizationEntry;
using wi::EventDetail;
using wi::EventEntry;
using wi::EventFilter;
using wi::NameMatch;
using wi::Participant;

// 4.6
inline std::optional<std::array<std::int64_t, 2>> event_place(const wi::MarkerIndex& ix, const wh::LoadedHistory& h,
                                                               std::int64_t event) {
    return ix.event_place(h, event);
}

}  // namespace brapi
