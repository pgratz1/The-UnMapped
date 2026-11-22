// Spec P3-07 14.1 items 5 and 6: each check of 5.2 item 1 and of 5.3 once, with its exact message, on a
// copy of fixture H1 (E = 3000) corrupted with db::testing::execute_on_file, the corruption chosen so that
// the earlier checks of 5.2 pass; and HistoryView::load's failures (5.2 item 4) arriving as corrupt_layer
// errors, never as a FatalError. The messages of P3-03 9.1 and P3-04 11.1 (5.2 item 2) are those specs'
// tests.
//
// Written from the spec only. Readings: in "lake_cells outside 0 to cells" and "walk_cells outside 0 to
// cells" the word "cells" is literal (it is not a <placeholder> in 5.3); a relation row with until = since
// is not used here (5.3 refuses it, 6.6 r6 prints it: the test writer's report, R5).

#include "p3_07_switch.hpp"

#if UNMAPPED_P3_07_REPORT

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "history_report_checks.hpp"
#include "history_report_fixture.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/testing.hpp"

namespace {

struct Corruption {
    const char* sql;
    const char* message;
};

// The result of build_history_report on a copy of H1 changed by `sql`.
worldgen::db::Result<std::string> report_after(const std::string& sql, std::string* path_out = nullptr) {
    const std::string path = hrtest::fixture_copy("h1-war-and-ruin", "corrupt");
    const worldgen::db::Status s = worldgen::db::testing::execute_on_file(path, sql);
    REQUIRE_MESSAGE(s.ok(), "execute_on_file: " << (s.ok() ? "" : s.error().message));
    if (path_out != nullptr) *path_out = path;
    const worldgen::db::WorldReader r = hrtest::open_world(path);
    return worldgen::inspect::build_history_report(r, hrtest::HistoryLevel::summary);
}

void check_cases(const std::vector<Corruption>& cases) {
    for (const Corruption& c : cases) {
        CAPTURE(std::string(c.sql));
        bool threw = false;
        try {
            const auto rep = report_after(c.sql);
            REQUIRE_FALSE(rep.ok());
            CHECK(rep.error().code == worldgen::db::ErrorCode::corrupt_layer);
            CHECK(rep.error().message == c.message);
            // both levels report the same first failure
        } catch (const worldgen::FatalError& e) {
            threw = true;
            FAIL_CHECK("FatalError: " << e.what());
        }
        CHECK_FALSE(threw);
    }
}

}  // namespace

TEST_SUITE("history-report") {

TEST_CASE("P3-07 5.2 item 1: a world without history, and the inconsistent cases") {
    const std::string no_log = "DELETE FROM phase; DELETE FROM event_participant; DELETE FROM event_value; "
                               "DELETE FROM event_cause; DELETE FROM event; ";
    {
        std::string path;
        const auto rep = report_after(no_log + "UPDATE meta SET value = 'biomes,regions,chronology' WHERE key = 'stages'",
                                      &path);
        REQUIRE_FALSE(rep.ok());
        CHECK(rep.error().code == worldgen::db::ErrorCode::history_missing);
        CHECK(rep.error().message == "world file '" + path +
                                         "' has no history (stages: biomes,regions,chronology); generate it with the "
                                         "history stage");
    }
    {
        std::string path;
        const auto rep = report_after(no_log + "UPDATE meta SET value = '' WHERE key = 'stages'", &path);
        REQUIRE_FALSE(rep.ok());
        CHECK(rep.error().code == worldgen::db::ErrorCode::history_missing);
        CHECK(rep.error().message ==
              "world file '" + path + "' has no history (stages: none); generate it with the history stage");
    }
    CHECK(std::string(worldgen::db::error_code_name(worldgen::db::ErrorCode::history_missing)) == "history_missing");
    check_cases({
        {"UPDATE meta SET value = 'biomes,regions,chronology' WHERE key = 'stages'",
         "corrupt table 'phase': 9 rows, but meta.stages does not name history"},
        {"DELETE FROM phase; UPDATE meta SET value = 'biomes,regions,chronology' WHERE key = 'stages'",
         "corrupt table 'event': 35 rows, but meta.stages does not name history"},
        {"DELETE FROM phase", "corrupt table 'phase': no rows, but meta.stages names history"},
    });
}

TEST_CASE("P3-07 5.3: region, phase and event") {
    check_cases({
        {"UPDATE region SET cells = 0 WHERE id = 6", "corrupt table 'region': row 6: cells below 1"},
        {"UPDATE region SET lake_cells = 2001 WHERE id = 1",
         "corrupt table 'region': row 1: lake_cells outside 0 to cells"},
        {"UPDATE region SET lake_cells = -1 WHERE id = 2",
         "corrupt table 'region': row 2: lake_cells outside 0 to cells"},
        {"UPDATE region SET walk_cells = 2001 WHERE id = 3",
         "corrupt table 'region': row 3: walk_cells outside 0 to cells"},
        {"UPDATE region SET biome = 9 WHERE id = 4", "corrupt table 'region': row 4: biome 9 is not a biome"},
        {"UPDATE phase SET age = 2 WHERE id = 2", "corrupt table 'phase': row 2: age 2, expected 1"},
        {"UPDATE phase SET end_year = 1500 WHERE id = 2; UPDATE phase SET start_year = 1501 WHERE id = 3",
         "corrupt table 'phase': row 2: end_year 1500 beyond the end of age 1, 1499"},
        {"UPDATE event SET phase = 2 WHERE id = 4", "corrupt table 'event': row 4: phase 2, expected 1"},
        {"UPDATE event SET phase = 4 WHERE id = 21", "corrupt table 'event': row 21: phase 4, expected 5"},
    });
}

TEST_CASE("P3-07 5.3: civilization") {
    check_cases({
        {"UPDATE civilization SET id = 4 WHERE id = 3", "corrupt table 'civilization': row 4: id not consecutive"},
        {"UPDATE civilization SET race = 3 WHERE id = 1",
         "corrupt table 'civilization': row 1: race 3 is not a present race"},
        {"UPDATE civilization SET race = 7 WHERE id = 2",
         "corrupt table 'civilization': row 2: race 7 is not a present race"},
        {"UPDATE civilization SET home_region = 7 WHERE id = 1",
         "corrupt table 'civilization': row 1: home_region 7 is not a region"},
        {"UPDATE civilization SET founded_year = 3000 WHERE id = 3",
         "corrupt table 'civilization': row 3: founded_year 3000 outside 0 to 2999"},
        {"UPDATE civilization SET founding_event = 36 WHERE id = 1",
         "corrupt table 'civilization': row 1: founding_event 36 is not an event"},
        {"UPDATE civilization SET founding_event = 0 WHERE id = 2",
         "corrupt table 'civilization': row 2: founding_event 0 is not an event"},
        {"UPDATE civilization SET capital = 8 WHERE id = 1",
         "corrupt table 'civilization': row 1: capital 8 is not a settlement"},
        {"UPDATE civilization SET ending_event = 30 WHERE id = 1",
         "corrupt table 'civilization': row 1: ended_year and ending_event disagree"},
        {"UPDATE civilization SET ending_event = 0 WHERE id = 2",
         "corrupt table 'civilization': row 2: ended_year and ending_event disagree"},
        {"UPDATE civilization SET ended_year = 1499 WHERE id = 2",
         "corrupt table 'civilization': row 2: ended_year 1499 outside 1500 to 2999"},
        {"UPDATE civilization SET ending_event = 99 WHERE id = 2",
         "corrupt table 'civilization': row 2: ending_event 99 is not an event"},
    });
}

TEST_CASE("P3-07 5.3: settlement") {
    check_cases({
        {"UPDATE settlement SET id = 8 WHERE id = 7", "corrupt table 'settlement': row 8: id not consecutive"},
        {"UPDATE settlement SET region = 9 WHERE id = 1", "corrupt table 'settlement': row 1: region 9 is not a region"},
        {"UPDATE settlement SET founder_civ = 4 WHERE id = 1",
         "corrupt table 'settlement': row 1: founder_civ 4 is not a civilization"},
        {"UPDATE settlement SET founded_year = -1 WHERE id = 2",
         "corrupt table 'settlement': row 2: founded_year -1 outside 0 to 2999"},
        {"UPDATE settlement SET founding_event = 0 WHERE id = 2",
         "corrupt table 'settlement': row 2: founding_event 0 is not an event"},
        {"UPDATE settlement SET owner_civ = 5 WHERE id = 1",
         "corrupt table 'settlement': row 1: owner_civ 5 is not a civilization"},
        {"UPDATE settlement SET kind = 6 WHERE id = 1", "corrupt table 'settlement': row 1: kind 6"},
        {"UPDATE settlement SET kind = 0 WHERE id = 2", "corrupt table 'settlement': row 2: kind 0"},
        {"UPDATE settlement SET status = 0 WHERE id = 1", "corrupt table 'settlement': row 1: status 0"},
        {"UPDATE settlement SET status = 5 WHERE id = 4", "corrupt table 'settlement': row 4: status 5"},
        {"UPDATE settlement SET population = -5 WHERE id = 1",
         "corrupt table 'settlement': row 1: population -5 is negative"},
        {"UPDATE settlement SET ruin_event = 29 WHERE id = 1",
         "corrupt table 'settlement': row 1: ruined_year and ruin_event disagree"},
        {"UPDATE settlement SET ruined_year = 1400 WHERE id = 3",
         "corrupt table 'settlement': row 3: ruined_year 1400 outside 1500 to 2999"},
        {"UPDATE settlement SET ruin_event = 40 WHERE id = 3",
         "corrupt table 'settlement': row 3: ruin_event 40 is not an event"},
    });
}

TEST_CASE("P3-07 5.3: settlement_census") {
    check_cases({
        {"INSERT INTO settlement_census VALUES (9, 100, 1, 1, 1, 10)",
         "corrupt table 'settlement_census': row (9, 100): settlement 9 is not a settlement"},
        {"UPDATE settlement_census SET year = 1 WHERE settlement = 1 AND year = 0",
         "corrupt table 'settlement_census': row (1, 1): first row in 1, expected 0"},
        {"UPDATE settlement_census SET year = 3000 WHERE settlement = 1 AND year = 2049",
         "corrupt table 'settlement_census': row (1, 3000): year 3000 beyond 2999"},
        {"UPDATE settlement_census SET owner_civ = 7 WHERE settlement = 1 AND year = 149",
         "corrupt table 'settlement_census': row (1, 149): owner_civ 7 is not a civilization"},
        {"UPDATE settlement_census SET kind = 9 WHERE settlement = 1 AND year = 149",
         "corrupt table 'settlement_census': row (1, 149): kind 9"},
        {"UPDATE settlement_census SET status = 5 WHERE settlement = 1 AND year = 149",
         "corrupt table 'settlement_census': row (1, 149): status 5"},
        {"UPDATE settlement_census SET population = -1 WHERE settlement = 1 AND year = 149",
         "corrupt table 'settlement_census': row (1, 149): population -1 is negative"},
        {"UPDATE settlement_census SET population = 1499 WHERE settlement = 1 AND year = 2049",
         "corrupt table 'settlement_census': settlement 1: the last row differs from the settlement's end state"},
        {"UPDATE settlement_census SET owner_civ = 3 WHERE settlement = 7 AND year = 2005",
         "corrupt table 'settlement_census': settlement 7: the last row differs from the settlement's end state"},
        {"DELETE FROM settlement_census WHERE settlement = 2",
         "corrupt table 'settlement_census': settlement 2: no rows"},
        {"DELETE FROM settlement_census WHERE settlement = 7",
         "corrupt table 'settlement_census': settlement 7: no rows"},
    });
}

TEST_CASE("P3-07 5.3: region_control") {
    check_cases({
        {"INSERT INTO region_control VALUES (9, 0, NULL, 1, 2)",
         "corrupt table 'region_control': row (9, 0): region 9 is not a region"},
        {"UPDATE region_control SET civ = 4 WHERE region = 1",
         "corrupt table 'region_control': row (1, 0): civ 4 is not a civilization"},
        {"UPDATE region_control SET from_year = 3000 WHERE region = 2",
         "corrupt table 'region_control': row (2, 3000): from_year 3000 outside 0 to 2999"},
        {"UPDATE region_control SET to_year = 1500 WHERE region = 3 AND from_year = 1500",
         "corrupt table 'region_control': row (3, 1500): to_year 1500 outside 1501 to 2999"},
        {"UPDATE region_control SET to_year = 3000 WHERE region = 5",
         "corrupt table 'region_control': row (5, 300): to_year 3000 outside 301 to 2999"},
        {"UPDATE region_control SET to_year = 2006 WHERE region = 3 AND from_year = 1500",
         "corrupt table 'region_control': row (3, 2005): overlaps the previous interval"},
        {"UPDATE region_control SET to_year = NULL WHERE region = 3 AND from_year = 1500",
         "corrupt table 'region_control': row (3, 2005): overlaps the previous interval"},
        {"UPDATE region_control SET event = 0 WHERE region = 1",
         "corrupt table 'region_control': row (1, 0): event 0 is not an event"},
        {"UPDATE region_control SET event = 36 WHERE region = 4",
         "corrupt table 'region_control': row (4, 1500): event 36 is not an event"},
    });
}

TEST_CASE("P3-07 5.3: relation") {
    check_cases({
        {"UPDATE relation SET a_type = 10 WHERE kind = 'pact'",
         "corrupt table 'relation': row (10, 1, 6, 4, pact, 100): a_type 10"},
        {"UPDATE relation SET b_type = 0 WHERE kind = 'pact'",
         "corrupt table 'relation': row (3, 1, 0, 4, pact, 100): b_type 0"},
        {"UPDATE relation SET a_id = 9 WHERE kind = 'pact'",
         "corrupt table 'relation': row (3, 9, 6, 4, pact, 100): a c9 does not exist"},
        {"UPDATE relation SET b_id = 7 WHERE kind = 'enmity'",
         "corrupt table 'relation': row (3, 1, 3, 7, enmity, 1600): b c7 does not exist"},
        {"UPDATE relation SET a_type = 4, a_id = 8 WHERE kind = 'pact'",
         "corrupt table 'relation': row (4, 8, 6, 4, pact, 100): a s8 does not exist"},
        {"UPDATE relation SET b_type = 5, b_id = 7 WHERE kind = 'pact'",
         "corrupt table 'relation': row (3, 1, 5, 7, pact, 100): b rg7 does not exist"},
        {"UPDATE relation SET b_type = 1, b_id = 4 WHERE kind = 'pact'",
         "corrupt table 'relation': row (3, 1, 1, 4, pact, 100): b ra4 does not exist"},
        {"UPDATE relation SET b_type = 2, b_id = 4 WHERE kind = 'pact'",
         "corrupt table 'relation': row (3, 1, 2, 4, pact, 100): b g4 does not exist"},
        {"UPDATE relation SET since = 3000 WHERE kind = 'pact'",
         "corrupt table 'relation': row (3, 1, 6, 4, pact, 3000): since 3000 outside 0 to 2999"},
        {"UPDATE relation SET until_event = 5 WHERE kind = 'pact'",
         "corrupt table 'relation': row (3, 1, 6, 4, pact, 100): until and until_event disagree"},
        {"UPDATE relation SET until = 2000 WHERE kind = 'war' AND since = 2001",
         "corrupt table 'relation': row (3, 1, 3, 2, war, 2001): until 2000 outside 2002 to 2999"},
        {"UPDATE relation SET until = 3000 WHERE kind = 'war' AND since = 2001",
         "corrupt table 'relation': row (3, 1, 3, 2, war, 2001): until 3000 outside 2002 to 2999"},
        {"UPDATE relation SET since_event = 0 WHERE kind = 'pact'",
         "corrupt table 'relation': row (3, 1, 6, 4, pact, 100): since_event 0 is not an event"},
        {"UPDATE relation SET until_event = 99 WHERE kind = 'war' AND since = 2001",
         "corrupt table 'relation': row (3, 1, 3, 2, war, 2001): until_event 99 is not an event"},
        // the kind in the key is escaped as in 6.1
        {"UPDATE relation SET kind = 'pa\\ct', a_type = 10 WHERE kind = 'pact'",
         "corrupt table 'relation': row (10, 1, 6, 4, pa\\\\ct, 100): a_type 10"},
    });
}

TEST_CASE("P3-07 5.2 item 4, 14.1 item 6: HistoryView::load's failures are corrupt_layer errors, never fatal") {
    // the one message P3-04 16 spells out for these
    check_cases({{"UPDATE event_participant SET entity_id = 9 WHERE event = 2 AND seq = 1",
                  "corrupt table 'event_participant': row (2, 1): civilization 9 does not exist"}});
    const std::vector<const char*> cases = {
        "UPDATE event_kind SET template = '{nobody} was here.' WHERE key = 'omen'",    // unknown placeholder
        "UPDATE event_kind SET template = 'In {@year tensions' WHERE key = 'flare_up'",  // unclosed
        "INSERT INTO event_kind_role VALUES (99, 1, 'x', 3, 1, 1)",                     // a role of no kind
        "INSERT INTO event_kind_value VALUES (16, 2, 'x')",                             // a value seq not 1, 2, ...
        "UPDATE event_participant SET role = 5 WHERE event = 1 AND seq = 1",            // no such role seq
        "UPDATE event_participant SET entity_type = 4 WHERE event = 1 AND seq = 1",     // the role's type is race
        "UPDATE event_participant SET entity_id = 9 WHERE event = 1 AND seq = 2",       // no god 9
        "DELETE FROM event_participant WHERE event = 31 AND seq = 2",                   // truce: 2 parties needed
        "DELETE FROM event_value WHERE event = 3",                                      // population missing
        "INSERT INTO event_value VALUES (4, 1, 5)",                                     // omen has no value
        "UPDATE event SET region = 9 WHERE id = 1",                                     // no region 9
        "UPDATE event SET settlement = 9 WHERE id = 3",                                 // no settlement 9
    };
    for (const char* sql : cases) {
        CAPTURE(std::string(sql));
        try {
            const auto rep = report_after(sql);
            REQUIRE_FALSE(rep.ok());
            CHECK(rep.error().code == worldgen::db::ErrorCode::corrupt_layer);
            CHECK(rep.error().message.rfind("corrupt table '", 0) == 0);
        } catch (const worldgen::FatalError& e) {
            FAIL_CHECK("FatalError: " << e.what());
        }
    }
}

}  // TEST_SUITE

#endif  // UNMAPPED_P3_07_REPORT
