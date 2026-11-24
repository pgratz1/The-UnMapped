// Spec P3-04 11.1, test 16.1 item 13 (library side): the [history]
// section of `report` on a hand-made world (an empty world with
// chronology and history rows written by SQL), its keys recomputed here
// from the stored rows and written out; no section with an empty `phase`;
// [tables] lists 26 tables; each corrupt case of 11.1 with its message, in
// the stated order. Row-shape corruption (a NULL in a NOT NULL column, a
// wrong type) cannot be written into these STRICT tables and is not tested.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/report.hpp"

namespace {

using namespace engine_test;

// Three ages (elf 0, dwarf 3000, orc 5000), span 6000 (the fixture world's
// default HistoryParams); a schedule with one flare-up; four events.
const char* const BASE_ROWS =
    "INSERT INTO race VALUES "
    "(1, 'elf', 'Elf', 'Elves', 'Elven', 0, 700, 150, 2, 1, 0, 1),"
    "(2, 'goblin', 'Goblin', 'Goblins', 'Goblin', 2, 40, 15, 4, 0, NULL, 2),"
    "(3, 'dwarf', 'Dwarf', 'Dwarves', 'Dwarven', 1, 300, 70, 1, 1, 3000, 3),"
    "(4, 'faerie', 'Faerie', 'Faeries', 'Faerie', 0, 400, 100, 3, 0, NULL, 4),"
    "(5, 'human', 'Human', 'Humans', 'Human', 1, 70, 25, 5, 0, NULL, 5),"
    "(6, 'orc', 'Orc', 'Orcs', 'Orcish', 2, 50, 18, 6, 1, 5000, 6);"
    "INSERT INTO god VALUES (1, 'Aa', 0), (2, 'Bb', 2), (3, 'Cc', 1), (4, 'Dd', 0), (5, 'Ee', 1), (6, 'Ff', 2);"
    "INSERT INTO age VALUES (1, 1, 0, 2999, 'The Age of Elves'), (2, 3, 3000, 4999, 'The Age of Dwarves'),"
    "(3, 6, 5000, 5999, 'The Age of Orcs');"
    "INSERT INTO event_kind VALUES (1, 'awakening', 0, 1, 0, 100, 'a'), (2, 'civ_founded', 1, 1, 0, 80, 'b'),"
    "(3, 'settlement_founded', 0, 1, 1, 40, 'c'), (4, 'quiet_era', 0, 1, 0, 20, 'd'), (5, 'flare_up', 1, 0, 0, 50, 'e');"
    "INSERT INTO phase VALUES (1, 1, 1, 0, 149, 1), (2, 1, 2, 150, 2999, 0), (3, 2, 1, 3000, 3149, 2),"
    "(4, 2, 3, 3150, 3199, 1), (5, 2, 2, 3200, 4999, 0), (6, 3, 1, 5000, 5149, 4), (7, 3, 4, 5150, 5999, 0);"
    "INSERT INTO event VALUES (1, 0, 0, 1, 1, 1, 0, 100), (2, 3000, 3000, 1, 3, 1, 0, 100),"
    "(3, 3150, 3150, 5, 4, 0, 0, 50), (4, 5000, 5000, 1, 6, 1, 0, 100);"
    "INSERT INTO event_cause VALUES (3, 1);"
    "INSERT INTO civilization VALUES (1, 'A', 1, 2, 1, 1, 0, 1, NULL, 0), (2, 'B', 3, 1, 1, 2, 3000, 2, 4000, 3);"
    "INSERT INTO settlement VALUES (1, 'S1', 0, 0, 1, 1, 0, 1, 1, 1, 1, 200, NULL, 0, 0),"   // P5-02 11.1: ending
    "(2, 'S2', 1, 0, 1, 2, 3000, 2, 0, 1, 3, 0, 4000, 3, 5);"
    "INSERT INTO settlement_census VALUES (1, 0, 1, 1, 1, 200), (2, 3000, 2, 1, 1, 200), (2, 4000, 0, 1, 3, 0);"
    "INSERT INTO region_control VALUES (1, 0, 3000, 1, 1), (1, 3000, NULL, 2, 2);"
    "INSERT INTO relation VALUES (3, 1, 3, 2, 'war', 3150, NULL, 3, 0);";

class Worlds {
public:
    Worlds() : dir_("engine-report") {
        REQUIRE(worldgen::stages::testing::write_fixture_world(dir_.file("base.db"), 9, {}).ok());
        bytes_ = dbtest::read_file(dir_.file("base.db"));
    }
    std::string make(const std::string& sql) {
        const std::string path = dir_.file("w" + std::to_string(n_++) + ".db");
        dbtest::write_file(path, bytes_);
        if (!sql.empty()) {
            const auto s = worldgen::db::testing::execute_on_file(path, sql);
            REQUIRE_MESSAGE(s.ok(), "SQL rejected: " << sql);
        }
        return path;
    }

private:
    dbtest::TempDir dir_;
    std::vector<std::uint8_t> bytes_;
    int n_ = 0;
};

worldgen::db::Result<std::string> report_of(const std::string& path) {
    const db::WorldReader r = open_world(path);
    return worldgen::inspect::build_report(r);
}
std::string report_error(const std::string& path) {
    const auto r = report_of(path);
    return r.ok() ? std::string("<no error>") : r.error().message;
}
std::string section(const std::string& text, const std::string& header) {
    const std::size_t at = text.find(header + "\n");
    if (at == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find("\n[", at + 1);
    return text.substr(at, end == std::string::npos ? std::string::npos : end + 1 - at);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 11.1, 16.1 item 13: [history] of a hand-made world, after [chronology]") {
    Worlds w;
    const std::string path = w.make(BASE_ROWS);
    const auto r = report_of(path);
    REQUIRE_MESSAGE(r.ok(), r.error().message);
    const std::string& text = r.value();
    const std::string want =
        "[history]\nend_year = 6000\nphases_opening = 3\nphases_quiet = 2\nphases_flare = 1\nphases_final = 1\n"
        "simulated_years = 1350\nsimulated_permille = 225\nevents = 4\nevents_awakening = 3\nevents_civ_founded = 0\n"
        "events_settlement_founded = 0\nevents_quiet_era = 0\nevents_flare_up = 1\ncivilizations = 2\n"
        "civilizations_alive = 1\nsettlements = 2\nsettlements_standing = 1\nregions_held = 1\ncensus_rows = 3\n"
        "relations = 1\nregion_names = 0\n"   // P3-04 r21 23.4: region_names after relations
        "beasts = 0\nmonster_kinds = 0\nbeast_populations = 0\nmonsters = 0\nsites = 0\n"   // P5-02 11.3
        "race_relations = 0\nsettlement_races = 0\n";   // P5-03 16.3
    // P3-05 15.4 appends the population keys after relations.
    const std::string p305 = "population = 200\n""population_elf = 200\n""population_goblin = 0\n""population_dwarf = 0\n""population_faerie = 0\n""population_human = 0\n""population_orc = 0\n""settlements_standing_elf = 1\n""settlements_standing_goblin = 0\n""settlements_standing_dwarf = 0\n""settlements_standing_faerie = 0\n""settlements_standing_human = 0\n""settlements_standing_orc = 0\n""regions_held_elf = 0\n""regions_held_goblin = 0\n""regions_held_dwarf = 1\n""regions_held_faerie = 0\n""regions_held_human = 0\n""regions_held_orc = 0\n";
    CHECK(section(text, "[history]") == want + p305);
    // the keys recomputed from the stored rows
    std::string again = "[history]\n";
    for (const std::string& l : report_lines(tables_of_world(open_world(path)), 6000)) {
        again += l + "\n";
    }
    CHECK(again == want);
    const std::size_t c = text.find("[chronology]\n");
    const std::size_t h = text.find("[history]\n");
    REQUIRE(c != std::string::npos);
    REQUIRE(h != std::string::npos);
    CHECK(c < h);
    CHECK(text.find("\n[", c + 1) == h - 1);
    // [tables]: 32 tables (schema 9, P5-02 11.1; schema 8, P3-04 r21 23.4), ascending name
    const std::string tables = section(text, "[tables]");
    std::size_t n = 0;
    for (const char ch : tables) {
        n += ch == '\n' ? 1 : 0;
    }
    CHECK(n == 35U);  // header + 34 (P5-03 16.3: race_relations, settlement_races)
    for (const std::string& t : hash_tables()) {
        CHECK_MESSAGE(tables.find("\n" + t + " = ") != std::string::npos, t);
    }
}

TEST_CASE("P3-04 11.1: no [history] while `phase` is empty; the event-kind lines include zeros in id order") {
    Worlds w;
    const std::string path = w.make("");
    const auto r = report_of(path);
    REQUIRE(r.ok());
    CHECK(section(r.value(), "[history]").empty());
    // A kind with no event still has its line; kinds in id order, also one this build does not compile.
    const std::string p2 = w.make(std::string(BASE_ROWS) + "INSERT INTO event_kind VALUES (6, 'omen', 0, 2, 2, 1, 'f');");
    const auto r2 = report_of(p2);
    REQUIRE(r2.ok());
    const std::string s = section(r2.value(), "[history]");
    CHECK(s.find("events_flare_up = 1\nevents_omen = 0\ncivilizations = 2\n") != std::string::npos);
}

TEST_CASE("P3-04 11.1, 16.1 item 13: each corrupt case with its message, in order") {
    Worlds w;
    struct Case {
        std::string sql, message;
    };
    const std::vector<Case> cases = {
        {"UPDATE phase SET id = 8 WHERE id = 7;", "corrupt table 'phase': row 8: id not consecutive"},
        {"UPDATE phase SET start_year = 151 WHERE id = 2;", "corrupt table 'phase': row 2: start_year 151, expected 150"},
        {"UPDATE phase SET end_year = 3149 WHERE id = 4;", "corrupt table 'phase': row 4: end_year below start_year"},
        {"UPDATE phase SET kind = 5 WHERE id = 2;", "corrupt table 'phase': row 2: kind 5"},
        {"UPDATE phase SET kind = 0 WHERE id = 2;", "corrupt table 'phase': row 2: kind 0"},
        {"UPDATE phase SET end_year = 5998 WHERE id = 7;",
         "corrupt table 'phase': the last phase ends in 5998, expected 5999"},
        {"UPDATE event SET id = 5 WHERE id = 4;", "corrupt table 'event': row 5: id not consecutive"},
        {"UPDATE event SET year = 2999, end_year = 2999 WHERE id = 3;",
         "corrupt table 'event': row 3: year 2999 below the previous event's 3000"},
        {"UPDATE event SET end_year = 2999 WHERE id = 2;", "corrupt table 'event': row 2: end_year below year"},
        {"UPDATE event SET kind = 9 WHERE id = 2;", "corrupt table 'event': row 2: kind 9 is not an event kind"},
        {"INSERT INTO event_cause VALUES (2, 2);", "corrupt table 'event_cause': row (2, 2): cause not below event"},
        {"INSERT INTO event_cause VALUES (1, 4);", "corrupt table 'event_cause': row (1, 4): cause not below event"},
        // first failure in the stated order: phase before event before event_cause
        {"UPDATE phase SET kind = 7 WHERE id = 3; UPDATE event SET kind = 9 WHERE id = 1;",
         "corrupt table 'phase': row 3: kind 7"},
        {"UPDATE event SET kind = 9 WHERE id = 1; INSERT INTO event_cause VALUES (2, 3);",
         "corrupt table 'event': row 1: kind 9 is not an event kind"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.sql);
        CHECK(report_error(w.make(std::string(BASE_ROWS) + c.sql)) == c.message);
    }
}

}  // TEST_SUITE
