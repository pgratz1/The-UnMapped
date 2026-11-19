#pragma once

// The [history] section of the report and its corrupt-data checks (spec
// P3-04 11.1).

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/db/reader.hpp"

namespace worldgen::inspect {

// The section text (empty when table phase has no row), or the first
// problem, the whole message of P3-04 11.1 ("corrupt table '<t>': ...").
struct HistorySectionResult {
    std::string section;
    std::optional<std::string> problem;
};

// Checks the history tables when phase or event has rows, then builds
// [history] when phase has a row. Checks, first failure reported: the row
// shapes of the thirteen tables in DDL order (column count, integer or
// text, NULL only in civilization.ended_year, settlement.ruined_year,
// region_control.to_year, relation.until; "row <id>: has <n> columns,
// expected <m>", "row <id>: column <k> is not an integer", "row <id>:
// column <k> is not text"); phase rows ("id not consecutive", "start_year
// <s>, expected <x>", "end_year below start_year", "kind <k>") and "the last
// phase ends in <e>, expected <E - 1>"; event rows ("id not consecutive",
// "year <y> below the previous event's <p>", "end_year below year", "kind
// <k> is not an event kind"); event_cause rows ("row (<e>, <c>): cause not
// below event"). end_year is meta.history_end_year (E). Errors of the
// reader are returned as such.
[[nodiscard]] db::Result<HistorySectionResult> history_section(const db::WorldReader& world, std::int64_t end_year);

// The raw rows of the thirteen history tables, in DDL order (index as
// db::HISTORY_TABLES), each in primary-key order as visited.
struct HistoryTables {
    std::array<std::vector<std::vector<db::ColumnValue>>, 13> rows;
    // The rows of table race (P3-03), id order: the per-race keys of P3-05
    // 15.4.
    std::vector<std::vector<db::ColumnValue>> races;
};
[[nodiscard]] db::Result<HistoryTables> read_history_tables(const db::WorldReader& world);

// The values of [history] (P3-04 11.1), in key order. events_by_kind holds
// (event_kind.key, rows of event of that kind) per event_kind row in id
// order. One computation shared by the world report's [history] and the
// history report's [stats] (P3-07), which prints the same values.
struct HistoryStats {
    std::int64_t end_year = 0;
    std::int64_t phases_opening = 0;
    std::int64_t phases_quiet = 0;
    std::int64_t phases_flare = 0;
    std::int64_t phases_final = 0;
    std::int64_t simulated_years = 0;
    std::int64_t simulated_permille = 0;
    std::int64_t events = 0;
    std::vector<std::pair<std::string, std::int64_t>> events_by_kind;
    std::int64_t civilizations = 0;
    std::int64_t civilizations_alive = 0;
    std::int64_t settlements = 0;
    std::int64_t settlements_standing = 0;
    std::int64_t regions_held = 0;
    std::int64_t census_rows = 0;
    std::int64_t relations = 0;
    // P3-05 15.4, after relations: the population of the standing
    // settlements, then per race row in id order (present or not) its
    // population, its standing settlements and its held regions (open
    // region_control rows), each list in race order.
    struct RaceStats {
        std::string key;
        std::int64_t population = 0;
        std::int64_t settlements_standing = 0;
        std::int64_t regions_held = 0;
    };
    std::int64_t population = 0;
    std::vector<RaceStats> races;
};

// The checks of history_section on tables already read, then the stats.
// problem is set on the first failure (and stats is then unspecified);
// has_phases is false when table phase has no row (no section).
struct HistoryStatsResult {
    HistoryStats stats;
    bool has_phases = false;
    std::optional<std::string> problem;
};
[[nodiscard]] HistoryStatsResult history_stats(const HistoryTables& tables, std::int64_t end_year);

// The [history] block of stats, "[history]\n" and one "key = value" line per key.
[[nodiscard]] std::string history_section_text(const HistoryStats& stats);

}  // namespace worldgen::inspect
