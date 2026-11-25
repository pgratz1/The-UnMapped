// Spec P3-03 9.1, test 14.1 item 10 (library side): the [chronology]
// section of `report` on hand-made worlds (an empty world with rows of
// race, god and age written by SQL), its keys recomputed by the test; no
// section with an empty age table; each corrupt case of 9.1 with its
// message, in the stated order; [tables] with the 13 tables and the 16
// meta keys. The CLI side (exit 3, nothing on stdout) is in
// tools/worldgen-cli/tests/cli_chronology_test.cpp.

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "p3_03_chronology_support.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace chrono_test;

// Three present races (elf 0, dwarf 3000, orc 5000), six gods, three ages
// tiling [0, 6000): a consistent hand-made chronology.
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
    "(3, 6, 5000, 5999, 'The Age of Orcs');";

class Worlds {
public:
    Worlds() : dir_("chronology-report") {
        REQUIRE(worldgen::stages::testing::write_fixture_world(dir_.file("base.db"), 9, {}).ok());
        bytes_ = dbtest::read_file(dir_.file("base.db"));
    }
    // The empty fixture world with `sql` applied.
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
    const worldgen::db::WorldReader r = open_world(path);
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

TEST_SUITE("chronology") {

TEST_CASE("P3-03 9.1, 14.1 item 10: [chronology] of a hand-made world, after [layers] when there is no [geography]") {
    Worlds w;
    const std::string path = w.make(BASE_ROWS);
    const auto r = report_of(path);
    REQUIRE_MESSAGE(r.ok(), r.error().message);
    const std::string& text = r.value();
    CHECK(section(text, "[chronology]") ==
          "[chronology]\nraces_present = 3\nawakening_years = 0 3000 5000\nage_years = 3000 2000 1000\n"
          "age_years_min = 1000\ngods = 6\n");
    CHECK(text.find("[geography]") == std::string::npos);
    const std::size_t at = text.find("[chronology]\n");
    REQUIRE(at != std::string::npos);
    CHECK(text.substr(0, at).ends_with("[layers]\n(none)\n"));
    // The keys recomputed from the stored rows by the test.
    std::vector<std::string> want = {"[chronology]"};
    for (const std::string& l : report_lines(rows_of_world(open_world(path)))) {
        want.push_back(l);
    }
    std::string joined_want;
    for (const std::string& l : want) {
        joined_want += l + "\n";
    }
    CHECK(section(text, "[chronology]") == joined_want);
}

TEST_CASE("P3-03 9.1, 14.1 item 10: the keys of a stage-written world; the minimum is not always the last age") {
    Worlds w;
    // Ages of 100, 5800 and 100 years (only race 1, 3 and 6 present).
    const std::string path = w.make(
        "INSERT INTO race VALUES (1, 'elf', 'Elf', 'Elves', 'Elven', 0, 700, 150, 2, 1, 0, 1),"
        "(3, 'dwarf', 'Dwarf', 'Dwarves', 'Dwarven', 1, 300, 70, 1, 1, 100, 3),"
        "(6, 'orc', 'Orc', 'Orcs', 'Orcish', 2, 50, 18, 6, 1, 5900, 6);"
        "INSERT INTO age VALUES (1, 1, 0, 99, 'A'), (2, 3, 100, 5899, 'B'), (3, 6, 5900, 5999, 'C');");
    const auto r = report_of(path);
    REQUIRE_MESSAGE(r.ok(), r.error().message);
    CHECK(section(r.value(), "[chronology]") ==
          "[chronology]\nraces_present = 3\nawakening_years = 0 100 5900\nage_years = 100 5800 100\n"
          "age_years_min = 100\ngods = 0\n");
}

TEST_CASE("P3-03 9.1, 14.1 item 10: no [chronology] when race and age are both empty") {
    Worlds w;
    const auto r = report_of(w.make(""));
    REQUIRE(r.ok());
    CHECK(r.value().find("[chronology]") == std::string::npos);
}

TEST_CASE("P3-03 9.1, 14.1 item 10: race rows with an empty age table are corrupt (lead's ruling)") {
    // A chronology run writes race and age together and a world stopped
    // before it has both empty, so "whenever table race or age has rows"
    // applies with an empty age table: the count check fails.
    Worlds w;
    CHECK(report_error(w.make(std::string(BASE_ROWS) + "DELETE FROM age;")) ==
          "corrupt table 'age': 0 rows for 3 present races");
}

TEST_CASE("P3-03 9.1, 14.1 item 10: [tables] lists the 32 tables (schema 9, P5-02) and [meta] the 17 keys (P3-04)") {
    Worlds w;
    const auto r = report_of(w.make(""));
    REQUIRE(r.ok());
    const std::string tables = section(r.value(), "[tables]");
    CHECK(tables ==
          "[tables]\nage = 0\nbeast = 0\nbeast_population = 0\nbiome = 0\nbook = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\nfigure = 0\ngod = 0\nlayer = 0\nmeta = 17\nmonster = 0\nmonster_kind = 0\npass = 0\nphase = 0\nrace = 0\n"
          "race_biome = 0\nrace_relation = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\n"
          "relation = 0\n"
          "settlement = 0\nsettlement_census = 0\nsettlement_race = 0\nsite = 0\n");
    const std::string meta = section(r.value(), "[meta]");
    for (const std::string& k : eight_keys()) {
        CAPTURE(k);
        if (k == "param.divine_script") {
            CHECK(meta.find("\nparam.divine_script =\n") != std::string::npos);
        } else {
            CHECK(meta.find("\n" + k + " = ") != std::string::npos);
        }
    }
    CHECK(meta.find("\nparam.races = elf,goblin,dwarf,faerie,human,orc\n") != std::string::npos);
    CHECK(meta.find("\nparam.history_span = 6000\n") != std::string::npos);
}

TEST_CASE("P3-03 9.1, 14.1 item 10: each corrupt case, with its message") {
    Worlds w;
    const std::string base = BASE_ROWS;
    struct Case {
        std::string sql;
        std::string message;
    };
    const std::vector<Case> cases = {
        // (1) race rows in id order
        {"UPDATE race SET present = 2 WHERE id = 4", "corrupt table 'race': row 4: present is 2"},
        {"UPDATE race SET present = -1 WHERE id = 2", "corrupt table 'race': row 2: present is -1"},
        {"UPDATE race SET awakening_year = 10 WHERE id = 2",
         "corrupt table 'race': row 2: awakening_year does not match present"},
        {"UPDATE race SET awakening_year = NULL WHERE id = 3",
         "corrupt table 'race': row 3: awakening_year does not match present"},
        // the first failing race row in id order
        {"UPDATE race SET present = 7 WHERE id = 5; UPDATE race SET awakening_year = 1 WHERE id = 4",
         "corrupt table 'race': row 4: awakening_year does not match present"},
        // a race failure before any age failure
        {"UPDATE race SET present = 3 WHERE id = 6; DELETE FROM age WHERE id = 3",
         "corrupt table 'race': row 6: present is 3"},
        // (2) age rows in id order
        {"UPDATE age SET id = 4 WHERE id = 3", "corrupt table 'age': row 4: id not consecutive"},
        {"UPDATE age SET id = id + 10", "corrupt table 'age': row 11: id not consecutive"},
        {"UPDATE age SET race = 2 WHERE id = 2", "corrupt table 'age': row 2: race 2 is not a present race"},
        {"UPDATE age SET race = 9 WHERE id = 3", "corrupt table 'age': row 3: race 9 is not a present race"},
        {"UPDATE age SET start_year = 5 WHERE id = 1", "corrupt table 'age': row 1: start_year 5, expected 0"},
        {"UPDATE age SET start_year = 3001 WHERE id = 2", "corrupt table 'age': row 2: start_year 3001, expected 3000"},
        {"UPDATE age SET end_year = 2998 WHERE id = 2", "corrupt table 'age': row 2: end_year below start_year"},
        {"UPDATE race SET awakening_year = 3100 WHERE id = 3", "corrupt table 'age': row 2: race 3 awakens in 3100"},
        // after the rows: the last end, then the count
        {"UPDATE age SET end_year = 5998 WHERE id = 3", "corrupt table 'age': the last age ends in 5998, expected 5999"},
        {"DELETE FROM age WHERE id = 3; UPDATE age SET end_year = 5999 WHERE id = 2; "
         "UPDATE race SET present = 1, awakening_year = 5500 WHERE id = 5",
         "corrupt table 'age': 2 rows for 4 present races"},
        {"UPDATE race SET present = 1, awakening_year = 5500 WHERE id = 5",
         "corrupt table 'age': 3 rows for 4 present races"},
        // the span is param.history_span
        // (history_end_year with it: the reader requires them equal, P3-04 9.4)
        {"UPDATE meta SET value = '7000' WHERE key IN ('param.history_span', 'history_end_year')",
         "corrupt table 'age': the last age ends in 5999, expected 6999"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.sql);
        CHECK(report_error(w.make(base + c.sql)) == c.message);
    }
    // The base world is not corrupt.
    CHECK(report_of(w.make(base)).ok());
    // Rows in race without any age row are still checked ("whenever race or age has rows").
    CHECK(report_error(w.make(base + "DELETE FROM age; UPDATE race SET present = 5 WHERE id = 1")) ==
          "corrupt table 'race': row 1: present is 5");
}

}  // TEST_SUITE
