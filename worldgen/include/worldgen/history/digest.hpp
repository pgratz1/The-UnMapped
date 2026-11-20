#pragma once

// The history digest of spec P3-07 (section 5.1). From generator 0.18.0
// (P3-09) it holds the note of P3-07 r8 21.4, which the history report and
// the viewer's browser share; the rest of 5.1 (read_history_tables,
// digest_history) comes with the P3-07 implementation.

#include <cstdint>
#include <string>

#include "worldgen/history/event_catalogue.hpp"

namespace worldgen::history {

// The note written after the sentence of event `event_id` (P3-07 r8 21.4),
// without its parentheses, or an empty string when the event has none:
//   war_declared: "Claimed since <year>." or "Claimed the same year.", then,
//                 from the second war between the same two civilizations,
//                 " The <ordinal> war between them.";
//   flare_up:     "<claimant> pressed its claim to <region> against <holder>."
//                 when its first cause is a claim.
// Kinds are found by key in the stored catalogue. Never throws on a loaded
// view; an id not in the log has no note.
[[nodiscard]] std::string history_note(const HistoryView& view, std::int64_t event_id);

// ordinal(n) of 21.4 item 3 for n >= 2: "second" .. "tenth", then "11th",
// "21st", "22nd", "23rd", "101st", "111th".
[[nodiscard]] std::string history_ordinal(std::int64_t n);

}  // namespace worldgen::history
