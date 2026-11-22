// The tables of the history report and their checks (spec P3-07 5.2 items
// 1 to 3, 5.3). Rows in primary-key order; the first failure is reported.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/db/schema.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/chronology.hpp"
#include "worldgen/inspect/history.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace worldgen::history {

namespace {

using Row = std::vector<db::ColumnValue>;
using Rows = std::vector<Row>;

constexpr std::int64_t POPULATION_MAX = 2147483647;

std::string num(std::int64_t v) {
    return db::format_i64(v);
}

// A check failure: the whole message.
struct Corrupt {
    std::string message;
};

[[noreturn]] void corrupt(std::string_view table, const std::string& detail) {
    throw Corrupt{"corrupt table '" + std::string(table) + "': " + detail};
}

// --- Row shapes (as P3-03 9.1) ----------------------------------------------

std::string id_key(const Row& row) {
    return (!row.empty() && row[0].type == db::ValueType::integer) ? num(row[0].integer) : std::string("?");
}

// Column count, the text columns, every other column an integer (no
// nullable column in the tables checked here).
void check_shape(std::string_view table, const Row& row, std::size_t columns, std::span<const std::size_t> text) {
    const std::string at = "row " + id_key(row) + ": ";
    if (row.size() != columns) {
        corrupt(table, at + "has " + db::format_u64(row.size()) + " columns, expected " + db::format_u64(columns));
    }
    for (std::size_t k = 0; k < columns; ++k) {
        const bool is_text = std::find(text.begin(), text.end(), k) != text.end();
        if (is_text && row[k].type != db::ValueType::text) {
            corrupt(table, at + "column " + db::format_u64(k + 1U) + " is not text");
        }
        if (!is_text && row[k].type != db::ValueType::integer) {
            corrupt(table, at + "column " + db::format_u64(k + 1U) + " is not an integer");
        }
    }
}

std::int64_t i64(const Row& r, std::size_t k) {
    return r[k].integer;
}
std::optional<std::int64_t> opt(const Row& r, std::size_t k) {
    if (r[k].type == db::ValueType::null) {
        return std::nullopt;
    }
    return r[k].integer;
}

// --- Lookups ----------------------------------------------------------------

template <class T, class F>
bool has_id(const std::vector<T>& rows, std::int64_t id, F key) {
    const auto it = std::lower_bound(rows.begin(), rows.end(), id,
                                     [&key](const T& r, std::int64_t v) { return key(r) < v; });
    return it != rows.end() && key(*it) == id;
}

bool contains_item(std::string_view list, std::string_view item) {
    std::size_t start = 0;
    while (start <= list.size()) {
        const std::size_t comma = list.find(',', start);
        const std::size_t end = comma == std::string_view::npos ? list.size() : comma;
        if (list.substr(start, end - start) == item) {
            return true;
        }
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    return false;
}

// --- The tables -------------------------------------------------------------

struct Reader {
    const db::WorldReader& world;

    Rows rows(std::string_view table) const {
        db::Result<Rows> r = world.table_rows(table);
        if (!r.ok()) {
            throw r.error();
        }
        return std::move(r.value());
    }
};

void read_regions(HistoryTables& t, const Rows& rows) {
    static constexpr std::size_t TEXT[] = {1};
    for (const Row& r : rows) {
        check_shape(db::REGION_TABLE, r, 28, TEXT);
        t.regions.push_back(RegionInfo{i64(r, 0), r[1].bytes, i64(r, 2), i64(r, 10), i64(r, 11), i64(r, 15)});
    }
}

void read_biomes(HistoryTables& t, const Rows& rows) {
    static constexpr std::size_t TEXT[] = {1, 2};
    for (const Row& r : rows) {
        check_shape(db::BIOME_TABLE, r, 5, TEXT);
        t.biome_keys.emplace_back(i64(r, 0), r[1].bytes);
    }
}

void read_gods(HistoryTables& t, const Rows& rows) {
    static constexpr std::size_t TEXT[] = {1};
    for (const Row& r : rows) {
        check_shape(db::GOD_TABLE, r, 3, TEXT);
        t.gods.push_back(db::GodRow{i64(r, 0), r[1].bytes, i64(r, 2)});
    }
}

// Race and age rows were checked by P3-03 9.1 (chronology_section).
void read_races_ages(HistoryTables& t, const Rows& races, const Rows& ages) {
    for (const Row& r : races) {
        t.races.push_back(db::RaceRow{i64(r, 0), r[1].bytes, r[2].bytes, r[3].bytes, r[4].bytes, i64(r, 5), i64(r, 6),
                                      i64(r, 7), i64(r, 8), i64(r, 9), opt(r, 10), i64(r, 11)});
    }
    for (const Row& r : ages) {
        t.ages.push_back(db::AgeRow{i64(r, 0), i64(r, 1), i64(r, 2), i64(r, 3), r[4].bytes});
    }
}

// The history tables were shape-checked by P3-04 11.1 (history_stats).
void read_history_rows(HistoryTables& t, const inspect::HistoryTables& h) {
    for (const Row& r : h.rows[3]) {
        t.phases.push_back(db::PhaseRow{i64(r, 0), i64(r, 1), i64(r, 2), i64(r, 3), i64(r, 4), i64(r, 5)});
    }
    t.event_rows = static_cast<std::int64_t>(h.rows[4].size());
    t.cause_rows = static_cast<std::int64_t>(h.rows[6].size());
    for (const Row& r : h.rows[8]) {
        t.civilizations.push_back(db::CivilizationRow{i64(r, 0), r[1].bytes, i64(r, 2), i64(r, 3), i64(r, 4), i64(r, 5),
                                                      i64(r, 6), i64(r, 7), opt(r, 8), i64(r, 9)});
    }
    for (const Row& r : h.rows[9]) {
        t.settlements.push_back(db::SettlementRow{i64(r, 0), r[1].bytes, i64(r, 2), i64(r, 3), i64(r, 4), i64(r, 5),
                                                  i64(r, 6), i64(r, 7), i64(r, 8), i64(r, 9), i64(r, 10), i64(r, 11),
                                                  opt(r, 12), i64(r, 13)});
    }
    for (const Row& r : h.rows[10]) {
        t.census.push_back(db::SettlementCensusRow{i64(r, 0), i64(r, 1), i64(r, 2), i64(r, 3), i64(r, 4), i64(r, 5)});
    }
    for (const Row& r : h.rows[11]) {
        t.region_control.push_back(db::RegionControlRow{i64(r, 0), i64(r, 1), opt(r, 2), i64(r, 3), i64(r, 4)});
    }
    for (const Row& r : h.rows[12]) {
        t.relations.push_back(db::RelationRow{i64(r, 0), i64(r, 1), i64(r, 2), i64(r, 3), r[4].bytes, i64(r, 5),
                                              opt(r, 6), i64(r, 7), i64(r, 8)});
    }
}

// --- The checks of 5.3 ------------------------------------------------------

class Checks {
public:
    explicit Checks(const HistoryTables& t) : t_(t), e_(t.end_year) {}

    void run(const Rows& event_rows) const {
        regions();
        phases();
        events(event_rows);
        civilizations();
        settlements();
        census();
        region_control();
        relations();
        gods_and_races();
    }

private:
    // The event rows' phases.
    void events(const Rows& events) const {
        std::size_t p = 0;
        for (const Row& r : events) {
            const std::int64_t year = i64(r, 1);
            while (p < t_.phases.size() && t_.phases[p].end_year < year) {
                ++p;
            }
            const std::int64_t x =
                (p < t_.phases.size() && t_.phases[p].start_year <= year) ? t_.phases[p].id : 0;
            if (x == 0 || i64(r, 4) != x) {
                corrupt(db::EVENT_TABLE, "row " + num(i64(r, 0)) + ": phase " + num(i64(r, 4)) + ", expected " + num(x));
            }
        }
    }

    bool is_event(std::int64_t id) const { return id >= 1 && id <= t_.event_rows; }
    bool is_region(std::int64_t id) const {
        return has_id(t_.regions, id, [](const RegionInfo& r) { return r.id; });
    }
    bool is_civ(std::int64_t id) const {
        return has_id(t_.civilizations, id, [](const db::CivilizationRow& r) { return r.id; });
    }
    bool is_settlement(std::int64_t id) const {
        return has_id(t_.settlements, id, [](const db::SettlementRow& r) { return r.id; });
    }
    bool is_present_race(std::int64_t id) const {
        for (const db::RaceRow& r : t_.races) {
            if (r.id == id) {
                return r.present == 1;
            }
        }
        return false;
    }
    bool exists(std::int64_t type, std::int64_t id) const {
        switch (type) {
            case 1:
                return has_id(t_.races, id, [](const db::RaceRow& r) { return r.id; });
            case 2:
                return has_id(t_.gods, id, [](const db::GodRow& r) { return r.id; });
            case 3: return is_civ(id);
            case 4: return is_settlement(id);
            case 5: return is_region(id);
            default: return true;  // types 6 to 9: nothing to look up
        }
    }
    std::string range(std::int64_t lo) const { return " outside " + num(lo) + " to " + num(e_ - 1); }

    void regions() const {
        for (const RegionInfo& r : t_.regions) {
            const std::string at = "row " + num(r.id) + ": ";
            if (r.cells < 1) {
                corrupt(db::REGION_TABLE, at + "cells below 1");
            }
            if (r.lake_cells < 0 || r.lake_cells > r.cells) {
                corrupt(db::REGION_TABLE, at + "lake_cells outside 0 to cells");
            }
            if (r.walk_cells < 0 || r.walk_cells > r.cells) {
                corrupt(db::REGION_TABLE, at + "walk_cells outside 0 to cells");
            }
            if (!has_id(t_.biome_keys, r.biome, [](const std::pair<std::int64_t, std::string>& b) { return b.first; })) {
                corrupt(db::REGION_TABLE, at + "biome " + num(r.biome) + " is not a biome");
            }
        }
    }

    void phases() const {
        for (const db::PhaseRow& p : t_.phases) {
            const db::AgeRow* age = nullptr;
            for (const db::AgeRow& a : t_.ages) {
                if (a.start_year <= p.start_year && p.start_year <= a.end_year) {
                    age = &a;
                    break;
                }
            }
            const std::int64_t x = age != nullptr ? age->id : 0;
            const std::string at = "row " + num(p.id) + ": ";
            if (age == nullptr || p.age != x) {
                corrupt(db::PHASE_TABLE, at + "age " + num(p.age) + ", expected " + num(x));
            }
            if (p.end_year > age->end_year) {
                corrupt(db::PHASE_TABLE, at + "end_year " + num(p.end_year) + " beyond the end of age " + num(x) + ", " +
                                             num(age->end_year));
            }
        }
    }

    void civilizations() const {
        std::int64_t expected = 1;
        for (const db::CivilizationRow& c : t_.civilizations) {
            const std::string at = "row " + num(c.id) + ": ";
            const auto fail = [&at](const std::string& why) { corrupt(db::CIVILIZATION_TABLE, at + why); };
            if (c.id != expected) {
                fail("id not consecutive");
            }
            ++expected;
            if (!is_present_race(c.race)) {
                fail("race " + num(c.race) + " is not a present race");
            }
            if (!is_region(c.home_region)) {
                fail("home_region " + num(c.home_region) + " is not a region");
            }
            if (c.founded_year < 0 || c.founded_year > e_ - 1) {
                fail("founded_year " + num(c.founded_year) + range(0));
            }
            if (!is_event(c.founding_event)) {
                fail("founding_event " + num(c.founding_event) + " is not an event");
            }
            if (c.capital != 0 && !is_settlement(c.capital)) {
                fail("capital " + num(c.capital) + " is not a settlement");
            }
            if (c.ended_year.has_value() != (c.ending_event != 0)) {
                fail("ended_year and ending_event disagree");
            }
            if (c.ended_year) {
                if (*c.ended_year < c.founded_year || *c.ended_year > e_ - 1) {
                    fail("ended_year " + num(*c.ended_year) + range(c.founded_year));
                }
                if (!is_event(c.ending_event)) {
                    fail("ending_event " + num(c.ending_event) + " is not an event");
                }
            }
        }
    }

    void settlements() const {
        std::int64_t expected = 1;
        for (const db::SettlementRow& s : t_.settlements) {
            const std::string at = "row " + num(s.id) + ": ";
            const auto fail = [&at](const std::string& why) { corrupt(db::SETTLEMENT_TABLE, at + why); };
            if (s.id != expected) {
                fail("id not consecutive");
            }
            ++expected;
            if (!is_region(s.region)) {
                fail("region " + num(s.region) + " is not a region");
            }
            if (!is_civ(s.founder_civ)) {
                fail("founder_civ " + num(s.founder_civ) + " is not a civilization");
            }
            if (s.founded_year < 0 || s.founded_year > e_ - 1) {
                fail("founded_year " + num(s.founded_year) + range(0));
            }
            if (!is_event(s.founding_event)) {
                fail("founding_event " + num(s.founding_event) + " is not an event");
            }
            if (s.owner_civ != 0 && !is_civ(s.owner_civ)) {
                fail("owner_civ " + num(s.owner_civ) + " is not a civilization");
            }
            if (s.kind < 1 || s.kind > 5) {
                fail("kind " + num(s.kind));
            }
            if (s.status < 1 || s.status > 4) {
                fail("status " + num(s.status));
            }
            if (s.population < 0) {
                fail("population " + num(s.population) + " is negative");
            }
            if (s.population > POPULATION_MAX) {
                fail("population " + num(s.population) + " above " + num(POPULATION_MAX));
            }
            if (s.ruined_year.has_value() != (s.ruin_event != 0)) {
                fail("ruined_year and ruin_event disagree");
            }
            if (s.ruined_year) {
                if (*s.ruined_year < s.founded_year || *s.ruined_year > e_ - 1) {
                    fail("ruined_year " + num(*s.ruined_year) + range(s.founded_year));
                }
                if (!is_event(s.ruin_event)) {
                    fail("ruin_event " + num(s.ruin_event) + " is not an event");
                }
            }
        }
    }

    // The settlement row of id s (ids are 1, 2, ... by now).
    const db::SettlementRow& settlement(std::int64_t s) const { return t_.settlements[static_cast<std::size_t>(s - 1)]; }

    void census_end(std::int64_t s, const db::SettlementCensusRow& last) const {
        const db::SettlementRow& row = settlement(s);
        if (last.owner_civ != row.owner_civ || last.kind != row.kind || last.status != row.status ||
            last.population != row.population) {
            corrupt(db::SETTLEMENT_CENSUS_TABLE,
                    "settlement " + num(s) + ": the last row differs from the settlement's end state");
        }
    }

    // Settlements with ids in (after, before) have no census row.
    void census_gap(std::int64_t after, std::int64_t before) const {
        if (after + 1 < before) {
            corrupt(db::SETTLEMENT_CENSUS_TABLE, "settlement " + num(after + 1) + ": no rows");
        }
    }

    void census() const {
        const auto n = static_cast<std::int64_t>(t_.settlements.size());
        std::int64_t current = 0;  // the settlement of the previous row, 0 before the first
        const db::SettlementCensusRow* last = nullptr;
        for (const db::SettlementCensusRow& c : t_.census) {
            const std::string at = "row (" + num(c.settlement) + ", " + num(c.year) + "): ";
            const auto fail = [&at](const std::string& why) { corrupt(db::SETTLEMENT_CENSUS_TABLE, at + why); };
            const bool first = c.settlement != current;
            if (first && last != nullptr) {
                census_end(current, *last);
            }
            if (c.settlement < 1 || c.settlement > n) {
                fail("settlement " + num(c.settlement) + " is not a settlement");
            }
            if (first) {
                census_gap(current, c.settlement);
                if (c.year != settlement(c.settlement).founded_year) {
                    fail("first row in " + num(c.year) + ", expected " + num(settlement(c.settlement).founded_year));
                }
            }
            if (c.year > e_ - 1) {
                fail("year " + num(c.year) + " beyond " + num(e_ - 1));
            }
            if (c.owner_civ != 0 && !is_civ(c.owner_civ)) {
                fail("owner_civ " + num(c.owner_civ) + " is not a civilization");
            }
            if (c.kind < 1 || c.kind > 5) {
                fail("kind " + num(c.kind));
            }
            if (c.status < 1 || c.status > 4) {
                fail("status " + num(c.status));
            }
            if (c.population < 0) {
                fail("population " + num(c.population) + " is negative");
            }
            if (c.population > POPULATION_MAX) {
                fail("population " + num(c.population) + " above " + num(POPULATION_MAX));
            }
            current = c.settlement;
            last = &c;
        }
        if (last != nullptr) {
            census_end(current, *last);
        }
        census_gap(current, n + 1);
    }

    void region_control() const {
        const db::RegionControlRow* previous = nullptr;
        for (const db::RegionControlRow& r : t_.region_control) {
            const std::string at = "row (" + num(r.region) + ", " + num(r.from_year) + "): ";
            const auto fail = [&at](const std::string& why) { corrupt(db::REGION_CONTROL_TABLE, at + why); };
            if (!is_region(r.region)) {
                fail("region " + num(r.region) + " is not a region");
            }
            if (!is_civ(r.civ)) {
                fail("civ " + num(r.civ) + " is not a civilization");
            }
            if (r.from_year < 0 || r.from_year > e_ - 1) {
                fail("from_year " + num(r.from_year) + range(0));
            }
            if (r.to_year && (*r.to_year < r.from_year + 1 || *r.to_year > e_ - 1)) {
                fail("to_year " + num(*r.to_year) + range(r.from_year + 1));
            }
            if (previous != nullptr && previous->region == r.region &&
                (!previous->to_year || *previous->to_year > r.from_year)) {
                fail("overlaps the previous interval");
            }
            if (!is_event(r.event)) {
                fail("event " + num(r.event) + " is not an event");
            }
            previous = &r;
        }
    }

    void relations() const {
        static constexpr std::array<std::string_view, 9> PREFIX = {"ra", "g", "c", "s", "rg", "f", "fa", "m", "si"};
        for (const db::RelationRow& r : t_.relations) {
            const std::string at = "row (" + num(r.a_type) + ", " + num(r.a_id) + ", " + num(r.b_type) + ", " +
                                   num(r.b_id) + ", " + inspect::escape_history_text(r.kind) + ", " + num(r.since) +
                                   "): ";
            const auto fail = [&at](const std::string& why) { corrupt(db::RELATION_TABLE, at + why); };
            if (r.a_type < 1 || r.a_type > 9) {
                fail("a_type " + num(r.a_type));
            }
            if (r.b_type < 1 || r.b_type > 9) {
                fail("b_type " + num(r.b_type));
            }
            if (!exists(r.a_type, r.a_id)) {
                fail("a " + std::string(PREFIX[static_cast<std::size_t>(r.a_type - 1)]) + num(r.a_id) +
                     " does not exist");
            }
            if (!exists(r.b_type, r.b_id)) {
                fail("b " + std::string(PREFIX[static_cast<std::size_t>(r.b_type - 1)]) + num(r.b_id) +
                     " does not exist");
            }
            if (r.since < 0 || r.since > e_ - 1) {
                fail("since " + num(r.since) + range(0));
            }
            if (r.until.has_value() != (r.until_event != 0)) {
                fail("until and until_event disagree");
            }
            // P3-04 6.7 r9 (P3-07 r6): a relation opened and closed within
            // one year has until = since, so the range starts at since.
            if (r.until && (*r.until < r.since || *r.until > e_ - 1)) {
                fail("until " + num(*r.until) + range(r.since));
            }
            if (!is_event(r.since_event)) {
                fail("since_event " + num(r.since_event) + " is not an event");
            }
            if (r.until_event != 0 && !is_event(r.until_event)) {
                fail("until_event " + num(r.until_event) + " is not an event");
            }
        }
    }

    void gods_and_races() const {
        for (const db::GodRow& g : t_.gods) {
            if (g.camp < 0 || g.camp > 2) {
                corrupt(db::GOD_TABLE, "row " + num(g.id) + ": camp " + num(g.camp));
            }
        }
        for (const db::RaceRow& r : t_.races) {
            if (!has_id(t_.gods, r.creator_god, [](const db::GodRow& g) { return g.id; })) {
                corrupt(db::RACE_TABLE, "row " + num(r.id) + ": creator_god " + num(r.creator_god) + " is not a god");
            }
        }
    }

    const HistoryTables& t_;
    std::int64_t e_;
};

std::int64_t meta_integer(const db::WorldReader& world, std::string_view key) {
    if (const std::optional<std::string> text = world.meta(key)) {
        if (const std::optional<std::uint64_t> v = db::parse_canonical_u64(*text)) {
            if (*v <= 1000000000U) {
                return static_cast<std::int64_t>(*v);  // validated by the reader: 2000 ... 12000
            }
        }
    }
    return 0;
}

// The world report's P3-04 11.1 checks end with a bound on the population
// of a standing settlement (audit A2 of P3-04), "row <id>: population <p>
// outside 0 to 2147483647". The history report prints 5.3's texts for the
// same row instead (the lead's ruling; r16): "population <p> is negative"
// for a negative value and "population <p> above 2147483647" for one above
// the bound, as for a settlement that is not standing. Every other problem
// is passed on unchanged.
std::string history_stats_message(const inspect::HistoryTables& h, const std::string& problem) {
    for (const Row& r : h.rows[9]) {  // settlement rows, shapes checked by then
        const std::int64_t status = i64(r, 10);
        const std::int64_t population = i64(r, 11);
        if ((status != 1 && status != 2) || (population >= 0 && population <= POPULATION_MAX)) {
            continue;
        }
        const std::string at = "corrupt table 'settlement': row " + num(i64(r, 0)) + ": population " + num(population);
        if (problem == at + " outside 0 to " + num(POPULATION_MAX)) {
            return population < 0 ? at + " is negative" : at + " above " + num(POPULATION_MAX);
        }
        break;  // the first standing row out of bounds is the one history_stats reported
    }
    return problem;
}

HistoryTables read_and_check(const db::WorldReader& world) {
    const Reader read{world};
    HistoryTables t;
    t.stages = world.meta(db::META_STAGES).value_or(std::string());
    t.end_year = meta_integer(world, db::META_HISTORY_END_YEAR);

    // 5.2 item 1: history present.
    db::Result<inspect::HistoryTables> raw = inspect::read_history_tables(world);
    if (!raw.ok()) {
        throw raw.error();
    }
    const inspect::HistoryTables& h = raw.value();
    const bool named = contains_item(t.stages, "history");
    const std::size_t phases = h.rows[3].size();
    const std::size_t events = h.rows[4].size();
    if (!named && phases == 0 && events == 0) {
        const std::string stages = t.stages.empty() ? std::string("none") : inspect::escape_history_text(t.stages);
        throw db::Error{db::ErrorCode::history_missing, "world file '" + world.path() + "' has no history (stages: " +
                                                            stages + "); generate it with the history stage"};
    }
    if (!named) {
        const bool phase_first = phases != 0;
        corrupt(phase_first ? db::PHASE_TABLE : db::EVENT_TABLE,
                db::format_u64(phase_first ? phases : events) + " rows, but meta.stages does not name history");
    }
    if (phases == 0) {
        corrupt(db::PHASE_TABLE, "no rows, but meta.stages names history");
    }

    // 5.2 item 2: the world report's checks, P3-03 9.1 then P3-04 11.1.
    const Rows ages = read.rows(db::AGE_TABLE);
    const Rows gods = read.rows(db::GOD_TABLE);
    const inspect::ChronologySectionResult chronology = inspect::chronology_section(
        h.races, ages, static_cast<std::uint64_t>(gods.size()), meta_integer(world, db::META_PARAM_HISTORY_SPAN));
    if (chronology.problem) {
        throw db::Error{db::ErrorCode::corrupt_layer, *chronology.problem};
    }
    const inspect::HistoryStatsResult stats = inspect::history_stats(h, t.end_year);
    if (stats.problem) {
        throw db::Error{db::ErrorCode::corrupt_layer, history_stats_message(h, *stats.problem)};
    }

    // 5.2 item 3: the tables of 3.1 and the checks of 5.3.
    static constexpr std::array<std::pair<std::string_view, std::string_view>, 12> SETTINGS = {{
        {"seed", db::META_SEED},
        {"seed_phrase", db::META_SEED_PHRASE},
        {"map_size", db::META_PARAM_MAP_SIZE},
        {"history_span", db::META_PARAM_HISTORY_SPAN},
        {"history_detail", db::META_PARAM_HISTORY_DETAIL},
        {"races", db::META_PARAM_RACES},
        {"civs_per_race", db::META_PARAM_CIVS_PER_RACE},
        {"event_intensity", db::META_PARAM_EVENT_INTENSITY},
        {"monster_pressure", db::META_PARAM_MONSTER_PRESSURE},
        {"divine_activity", db::META_PARAM_DIVINE_ACTIVITY},
        {"divine_script", db::META_PARAM_DIVINE_SCRIPT},
        {"generator_version", db::META_GENERATOR_VERSION},
    }};
    for (const auto& [key, meta_key] : SETTINGS) {
        t.settings.emplace_back(std::string(key), world.meta(meta_key).value_or(std::string()));
    }
    read_races_ages(t, h.races, ages);
    read_history_rows(t, h);
    read_biomes(t, read.rows(db::BIOME_TABLE));
    read_gods(t, gods);
    read_regions(t, read.rows(db::REGION_TABLE));
    Checks(t).run(h.rows[4]);
    return t;
}

}  // namespace

// The two reads of the file (read_history_tables, then HistoryView::load)
// are separate snapshots. Everything the digest and the report index in one
// by a value of the other is checked here, so that a file changed between
// the reads is a corrupt-table error and never an out-of-range index.
std::optional<std::string> history_reads_disagree(const HistoryView& view, const HistoryTables& t) {
    if (static_cast<std::int64_t>(view.events().size()) != t.event_rows) {
        return std::string("corrupt table 'event': the file changed while it was read");
    }
    for (const HistoryView::Event& e : view.events()) {
        if (e.row.phase < 1 || static_cast<std::size_t>(e.row.phase) > t.phases.size()) {
            return std::string("corrupt table 'event': the file changed while it was read");
        }
    }
    return std::nullopt;
}

db::Result<HistoryTables> read_history_tables(const db::WorldReader& world) {
    try {
        return read_and_check(world);
    } catch (const Corrupt& c) {
        return db::Error{db::ErrorCode::corrupt_layer, c.message};
    } catch (const db::Error& e) {
        return e;
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while reading the history tables"};
    }
}

}  // namespace worldgen::history
