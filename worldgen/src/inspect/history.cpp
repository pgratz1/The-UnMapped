// The [history] section of the report (spec P3-04 11.1).

#include "worldgen/inspect/history.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "worldgen/db/schema.hpp"

namespace worldgen::inspect {

namespace {

using Rows = std::vector<std::vector<db::ColumnValue>>;

// The shape of one table's rows: column count, text columns, the nullable
// integer column (or `columns` for none).
struct Shape {
    std::string_view table;
    std::size_t columns;
    std::array<std::size_t, 2> text;  // unused slots hold `columns`
    std::size_t nullable;
};

constexpr std::array<Shape, 13> SHAPES = {{
    {db::EVENT_KIND_TABLE, 7, {1, 6}, 7},
    {db::EVENT_KIND_ROLE_TABLE, 6, {2, 6}, 6},
    {db::EVENT_KIND_VALUE_TABLE, 3, {2, 3}, 3},
    {db::PHASE_TABLE, 6, {6, 6}, 6},
    {db::EVENT_TABLE, 8, {8, 8}, 8},
    {db::EVENT_PARTICIPANT_TABLE, 5, {5, 5}, 5},
    {db::EVENT_CAUSE_TABLE, 2, {2, 2}, 2},
    {db::EVENT_VALUE_TABLE, 3, {3, 3}, 3},
    {db::CIVILIZATION_TABLE, 10, {1, 10}, 8},
    {db::SETTLEMENT_TABLE, 14, {1, 14}, 12},
    {db::SETTLEMENT_CENSUS_TABLE, 6, {6, 6}, 6},
    {db::REGION_CONTROL_TABLE, 5, {5, 5}, 2},
    {db::RELATION_TABLE, 9, {4, 9}, 6},
}};

std::string row_at(const std::vector<db::ColumnValue>& row) {
    const std::string id =
        (!row.empty() && row[0].type == db::ValueType::integer) ? db::format_i64(row[0].integer) : std::string("?");
    return "row " + id + ": ";
}

std::optional<std::string> shape_problem(const Shape& s, const std::vector<db::ColumnValue>& row) {
    if (row.size() != s.columns) {
        return "has " + db::format_u64(row.size()) + " columns, expected " + db::format_u64(s.columns);
    }
    for (std::size_t k = 0; k < s.columns; ++k) {
        const bool is_text = s.text[0] == k || s.text[1] == k;
        if (is_text) {
            if (row[k].type != db::ValueType::text) {
                return "column " + db::format_u64(k + 1U) + " is not text";
            }
        } else if (row[k].type != db::ValueType::integer &&
                   !(k == s.nullable && row[k].type == db::ValueType::null)) {
            return "column " + db::format_u64(k + 1U) + " is not an integer";
        }
    }
    return std::nullopt;
}

std::string corrupt(std::string_view table, const std::string& why) {
    return "corrupt table '" + std::string(table) + "': " + why;
}

}  // namespace

db::Result<HistoryTables> read_history_tables(const db::WorldReader& world) {
    HistoryTables t;
    for (std::size_t k = 0; k < SHAPES.size(); ++k) {
        db::Result<Rows> r = world.table_rows(SHAPES[k].table);
        if (!r.ok()) {
            return r.error();
        }
        t.rows[k] = std::move(r.value());
    }
    db::Result<Rows> races = world.table_rows(db::RACE_TABLE);
    if (!races.ok()) {
        return races.error();
    }
    t.races = std::move(races.value());
    return t;
}

HistoryStatsResult history_stats(const HistoryTables& tables, std::int64_t end_year) {
    HistoryStatsResult result;
    const auto& rows = tables.rows;
    const Rows& kinds = rows[0];
    const Rows& phases = rows[3];
    const Rows& events = rows[4];
    if (phases.empty() && events.empty()) {
        return result;
    }
    // Row shapes, tables in DDL order.
    for (std::size_t t = 0; t < SHAPES.size(); ++t) {
        for (const std::vector<db::ColumnValue>& row : rows[t]) {
            if (std::optional<std::string> why = shape_problem(SHAPES[t], row)) {
                result.problem = corrupt(SHAPES[t].table, row_at(row) + *why);
                return result;
            }
        }
    }
    HistoryStats& st = result.stats;
    st.end_year = end_year;
    std::int64_t counts[5] = {0, 0, 0, 0, 0};  // by kind 1 ... 4
    std::int64_t expected_id = 1;
    std::int64_t expected_start = 0;
    for (const std::vector<db::ColumnValue>& row : phases) {
        const std::string at = row_at(row);
        const std::int64_t id = row[0].integer;
        const std::int64_t kind = row[2].integer;
        const std::int64_t start = row[3].integer;
        const std::int64_t end = row[4].integer;
        if (id != expected_id) {
            result.problem = corrupt(db::PHASE_TABLE, at + "id not consecutive");
            return result;
        }
        if (start != expected_start) {
            result.problem = corrupt(db::PHASE_TABLE, at + "start_year " + db::format_i64(start) + ", expected " +
                                                          db::format_i64(expected_start));
            return result;
        }
        if (end < start) {
            result.problem = corrupt(db::PHASE_TABLE, at + "end_year below start_year");
            return result;
        }
        if (kind < 1 || kind > 4) {
            result.problem = corrupt(db::PHASE_TABLE, at + "kind " + db::format_i64(kind));
            return result;
        }
        // Audit A3: bound the stored year before any arithmetic on it (start
        // is bounded already: it equals expected_start, at most end_year).
        // On the last row this is the check of 11.1 made early.
        if (end > end_year - 1) {
            result.problem = &row == &phases.back()
                                 ? corrupt(db::PHASE_TABLE, "the last phase ends in " + db::format_i64(end) +
                                                                ", expected " + db::format_i64(end_year - 1))
                                 : corrupt(db::PHASE_TABLE, at + "end_year " + db::format_i64(end) + " above " +
                                                                db::format_i64(end_year - 1));
            return result;
        }
        ++counts[kind];
        if (kind != 2) {
            st.simulated_years += end - start + 1;
        }
        ++expected_id;
        expected_start = end + 1;
    }
    if (!phases.empty() && expected_start - 1 != end_year - 1) {
        result.problem = corrupt(db::PHASE_TABLE, "the last phase ends in " + db::format_i64(expected_start - 1) +
                                                      ", expected " + db::format_i64(end_year - 1));
        return result;
    }
    std::vector<std::int64_t> per_kind(kinds.size(), 0);
    expected_id = 1;
    std::int64_t previous_year = 0;
    for (const std::vector<db::ColumnValue>& row : events) {
        const std::string at = row_at(row);
        const std::int64_t id = row[0].integer;
        const std::int64_t year = row[1].integer;
        const std::int64_t end = row[2].integer;
        const std::int64_t kind = row[3].integer;
        if (id != expected_id) {
            result.problem = corrupt(db::EVENT_TABLE, at + "id not consecutive");
            return result;
        }
        if (id > 1 && year < previous_year) {
            result.problem = corrupt(db::EVENT_TABLE, at + "year " + db::format_i64(year) +
                                                          " below the previous event's " + db::format_i64(previous_year));
            return result;
        }
        if (end < year) {
            result.problem = corrupt(db::EVENT_TABLE, at + "end_year below year");
            return result;
        }
        std::size_t k = 0;
        while (k < kinds.size() && kinds[k][0].integer != kind) {
            ++k;
        }
        if (k == kinds.size()) {
            result.problem = corrupt(db::EVENT_TABLE, at + "kind " + db::format_i64(kind) + " is not an event kind");
            return result;
        }
        ++per_kind[k];
        previous_year = year;
        ++expected_id;
    }
    for (const std::vector<db::ColumnValue>& row : rows[6]) {
        if (row[1].integer >= row[0].integer) {
            result.problem = corrupt(db::EVENT_CAUSE_TABLE, "row (" + db::format_i64(row[0].integer) + ", " +
                                                                db::format_i64(row[1].integer) +
                                                                "): cause not below event");
            return result;
        }
    }
    result.has_phases = !phases.empty();
    const auto n = [](std::size_t v) { return static_cast<std::int64_t>(v); };
    st.phases_opening = counts[1];
    st.phases_quiet = counts[2];
    st.phases_flare = counts[3];
    st.phases_final = counts[4];
    st.simulated_permille = end_year > 0 ? st.simulated_years * 1000 / end_year : 0;
    st.events = n(events.size());
    for (std::size_t k = 0; k < kinds.size(); ++k) {
        st.events_by_kind.emplace_back(kinds[k][1].bytes, per_kind[k]);
    }
    st.civilizations = n(rows[8].size());
    for (const std::vector<db::ColumnValue>& row : rows[8]) {
        st.civilizations_alive += row[8].type == db::ValueType::null ? 1 : 0;
    }
    st.settlements = n(rows[9].size());
    for (const std::vector<db::ColumnValue>& row : rows[9]) {
        st.settlements_standing += (row[10].integer == 1 || row[10].integer == 2) ? 1 : 0;
    }
    for (const std::vector<db::ColumnValue>& row : rows[11]) {
        st.regions_held += row[2].type == db::ValueType::null ? 1 : 0;
    }
    st.census_rows = n(rows[10].size());
    st.relations = n(rows[12].size());
    // P3-05 15.4: per race. Race rows: column 1 id, 2 key.
    std::vector<std::int64_t> race_ids;
    for (const std::vector<db::ColumnValue>& row : tables.races) {
        if (row.size() < 2 || row[0].type != db::ValueType::integer || row[1].type != db::ValueType::text) {
            result.problem = corrupt(db::RACE_TABLE, row_at(row) + "id or key of the wrong type");
            return result;
        }
        race_ids.push_back(row[0].integer);
        st.races.push_back(HistoryStats::RaceStats{row[1].bytes, 0, 0, 0});
    }
    // The race of each civilization (column 3), by civilization id.
    const auto race_slot = [&](std::int64_t civ) -> HistoryStats::RaceStats* {
        for (const std::vector<db::ColumnValue>& c : rows[8]) {
            if (c[0].integer == civ) {
                for (std::size_t k = 0; k < race_ids.size(); ++k) {
                    if (race_ids[k] == c[2].integer) {
                        return &st.races[k];
                    }
                }
                return nullptr;
            }
        }
        return nullptr;
    };
    for (const std::vector<db::ColumnValue>& row : rows[9]) {
        if (row[10].integer != 1 && row[10].integer != 2) {
            continue;
        }
        st.population += row[11].integer;
        if (HistoryStats::RaceStats* r = race_slot(row[8].integer)) {
            r->population += row[11].integer;
            ++r->settlements_standing;
        }
    }
    for (const std::vector<db::ColumnValue>& row : rows[11]) {
        if (row[2].type != db::ValueType::null) {
            continue;
        }
        if (HistoryStats::RaceStats* r = race_slot(row[3].integer)) {
            ++r->regions_held;
        }
    }
    return result;
}

std::string history_section_text(const HistoryStats& st) {
    std::string out = "[history]\n";
    const auto line = [&out](std::string_view key, std::int64_t value) {
        out += std::string(key) + " = " + db::format_i64(value) + "\n";
    };
    line("end_year", st.end_year);
    line("phases_opening", st.phases_opening);
    line("phases_quiet", st.phases_quiet);
    line("phases_flare", st.phases_flare);
    line("phases_final", st.phases_final);
    line("simulated_years", st.simulated_years);
    line("simulated_permille", st.simulated_permille);
    line("events", st.events);
    for (const auto& [key, count] : st.events_by_kind) {
        line("events_" + key, count);
    }
    line("civilizations", st.civilizations);
    line("civilizations_alive", st.civilizations_alive);
    line("settlements", st.settlements);
    line("settlements_standing", st.settlements_standing);
    line("regions_held", st.regions_held);
    line("census_rows", st.census_rows);
    line("relations", st.relations);
    line("population", st.population);
    for (const HistoryStats::RaceStats& r : st.races) {
        line("population_" + r.key, r.population);
    }
    for (const HistoryStats::RaceStats& r : st.races) {
        line("settlements_standing_" + r.key, r.settlements_standing);
    }
    for (const HistoryStats::RaceStats& r : st.races) {
        line("regions_held_" + r.key, r.regions_held);
    }
    return out;
}

db::Result<HistorySectionResult> history_section(const db::WorldReader& world, std::int64_t end_year) {
    db::Result<HistoryTables> tables = read_history_tables(world);
    if (!tables.ok()) {
        return tables.error();
    }
    const HistoryStatsResult stats = history_stats(tables.value(), end_year);
    HistorySectionResult result;
    result.problem = stats.problem;
    if (!stats.problem && stats.has_phases) {
        result.section = history_section_text(stats.stats);
    }
    return result;
}

}  // namespace worldgen::inspect
