#pragma once

// The history report of worldgen-cli report --history, format
// "# worldgen history v3" (spec P3-07 sections 4 to 6, amendments 21 to 26).
// The text is part of the cross-platform contract: ASCII and LF only,
// integers only, no locale.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/db/reader.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/history/event_catalogue.hpp"

namespace worldgen::inspect {

// First line of format v1, without its line feed (P3-07 6.2); no longer
// written from generator 0.21.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V1 = "# worldgen history v1";
// First line of format v2 (P3-07 r19 25.1); no longer written from
// generator 0.22.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V2 = "# worldgen history v2";
// First line of format v3 (P3-07 r21 26.1); no longer written from
// generator 0.23.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V3 = "# worldgen history v3";
// First line of format v4 (P5-02 12.1); no longer written from generator
// 0.24.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V4 = "# worldgen history v4";
// First line of format v5 (P5-03 17.1); no longer written from generator
// 0.25.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V5 = "# worldgen history v5";
// First line of format v6 (P5-03a 9.1); no longer written from generator
// 0.26.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V6 = "# worldgen history v6";
// First line of format v7 (P5-04 10.1); no longer written from generator
// 0.27.0.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V7 = "# worldgen history v7";
// First line of format v8 (P5-06 10.1), the one written.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V8 = "# worldgen history v8";
// P5-04a 12.1 (generator 0.28.0): format v9.
inline constexpr std::string_view HISTORY_REPORT_HEADER_V9 = "# worldgen history v9";

enum class HistoryLevel : std::uint8_t { summary = 1, full = 2 };

// P3-07 r21 26.7 (audit A1): the row of settlement `id` in a vector indexed
// by settlement id - 1 (the digest's last_held_by and refuge_hidden_end), or
// null when id is below 1 or above the span's size.
template <class T>
[[nodiscard]] const T* settlement_row(std::span<const T> rows, std::int64_t id) noexcept {
    if (id < 1 || static_cast<std::uint64_t>(id) > rows.size()) {
        return nullptr;
    }
    return &rows[static_cast<std::size_t>(id - 1)];
}

// "summary" or "full"; std::nullopt for anything else.
[[nodiscard]] std::optional<HistoryLevel> parse_history_level(std::string_view text);

// Text from the world escaped for the history report (P3-07 6.1): a
// backslash becomes "\\", every byte below 0x20, 0x7F and every byte from
// 0x80 becomes "\xHH" with upper-case hex, every other byte is copied.
[[nodiscard]] std::string escape_history_text(std::string_view text);

// --- Block lines (P4-05 4.2) ------------------------------------------------

// One piece of a block line: literal bytes as printed; text from the world
// (escaped in the report); a reference to an entity (type code, id, its name
// or empty when it has none); an event reference "e<id>"; the sentence of an
// event (id).
enum class SegmentKind : std::uint8_t { literal = 1, text = 2, entity = 3, event = 4, sentence = 5 };
struct Segment {
    SegmentKind kind = SegmentKind::literal;
    std::string text;
    std::int64_t type = 0;
    std::int64_t id = 0;
    friend bool operator==(const Segment&, const Segment&) = default;
};
using BlockLine = std::vector<Segment>;

// The lines of civilization `civ`'s block in [civilizations] at `level`, in
// print order, the first being "c<id> <name>". `digest` is the report's
// (history_report_digest). FatalError "civilization_block: no civilization
// <c>" for an unknown civ, "civilization_block: bad level" for a level that
// is neither summary nor full.
[[nodiscard]] std::vector<BlockLine> civilization_block(const history::HistoryView& view,
                                                        const history::HistoryTables& tables,
                                                        const history::HistoryDigest& digest, std::int64_t civ,
                                                        HistoryLevel level);

// P5-06 11.3: the lines of figure `figure` for the browser, in the form of
// civilization_block: its [figures] line of the report (without the two
// leading spaces), and for a ruler its realm's "rulers:" line of
// [civilizations] at `level` (that line alone at summary, with the reign
// lines at full). FatalError "figure_block: no figure <f>", "figure_block:
// bad level".
[[nodiscard]] std::vector<BlockLine> figure_block(const history::HistoryView& view,
                                                  const history::HistoryTables& tables,
                                                  const history::HistoryDigest& digest, std::int64_t figure,
                                                  HistoryLevel level);

// The report form of a line (before the report's trailing-space rule):
// literal as is; text escape_history_text(text); entity
// escape_history_text(name) + " (" + tag + ")", or the tag alone when the
// name is empty; event "e<id>"; sentence escape_history_text(render_event).
// The tag is the report's prefix of the type ("ra", "g", "c", "s", "rg",
// "f", "fa", "m", "si"; "?" otherwise) and the id.
[[nodiscard]] std::string block_line_report_text(const history::HistoryView& view, const BlockLine& line);

// The display form: the same without escapes and without " (<tag>)". An
// entity with a name is a tagged link over the name; one without a name
// shows its tag, untagged (so that B4's transform gives the report form); an
// event is a link (type 0) over "e<id>"; a sentence carries its own links
// (render_event_linked) shifted.
[[nodiscard]] history::LinkedText block_line_display(const history::HistoryView& view, const BlockLine& line);

// The report's tag of an entity: prefix and id ("c3", "rg4"; "?<id>" for a
// type code outside 1 to 9).
[[nodiscard]] std::string history_entity_tag(std::int64_t type, std::int64_t id);

// --- The digest the report prints with (P4-05 4.3) ---------------------------

// digest_history, then, as P5-02 12.9, the summary set chosen again with
// the budget of the summary's line count outside [chronicle]. The report
// and the browser both use this digest. Pure on a checked history.
[[nodiscard]] history::HistoryDigest history_report_digest(const history::HistoryView& view,
                                                           const history::HistoryTables& tables);

// The whole history report of an open world (P3-07 section 6), or the first
// error of P3-07 5.2: history_missing for a world without history,
// corrupt_layer for a corrupt table, reader errors as they come,
// out_of_memory.
[[nodiscard]] db::Result<std::string> build_history_report(const db::WorldReader& world, HistoryLevel level);

}  // namespace worldgen::inspect
