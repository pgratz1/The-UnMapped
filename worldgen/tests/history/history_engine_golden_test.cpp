// Spec P3-04 16.3 and 16.4: the world cases of
// worldgen/tests/golden/p3-04-engine.txt (written by the independent
// reference reference/p3_engine_reference.py): hash --detail's thirteen
// table lines, the [history] section, the phase table with triggers and
// the rendered sentence of every event, byte for byte (render_event is a
// cross-platform contract); the worked example of 17 with both
// ForcedTension variants through run_engine; R1 (history_inputs_from in the
// pipeline equals history_inputs_from_world, member by member), R2 (the
// member counts of HistoryInputs and HistoryParams), Y2 resume with each
// refusal of 3.2, and threads 1, 2, 5. The 512 cases run by default, the
// other 512 cases are in the doctest suite "history-slow"
// (worldgen-tests -ts=history-slow --no-skip). R1, Y2 and the refusals on
// geo-seed-42-512 and every 1024 case are in
// history_engine_golden_resume_test.cpp, so that each of the two CTest
// entries generates about half of the worlds (P3-08 4 item 1).


#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p3_04_engine_golden_support.hpp"

using namespace engine_golden;


TEST_SUITE("history") {

TEST_CASE("P3-04 16.4: the 512 world case geo-seed-42-512: tables, [history], phases and every sentence") {
    check_world_case(golden().world("geo-seed-42-512"));
}

TEST_CASE("P3-04 16.3 item 18, 14.1: threads 1 and 5 give identical history tables (2 in history-slow)") {
    const GoldenWorld& g = golden().world("geo-seed-42-512");
    const Tables one = tables_of_world(open_world(world_of(g, 1)));
    CHECK(tables_of_world(open_world(world_of(g, 5))) == one);
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-04 16.4, 16.3 items 19, 21: the other 512 world cases: tables, sentences, R1 and Y2") {
    for (const GoldenWorld& g : golden().worlds) {
        if (g.map_size == 512 && g.tension == "none" && !default_case(g)) {
            check_world_case(g);
            check_r1(g);
            check_resume(g);
        }
    }
}

TEST_CASE("P3-04 16.3 item 18: threads 2 gives the tables of threads 1") {
    const GoldenWorld& g = golden().world("geo-seed-42-512");
    CHECK(tables_of_world(open_world(world_of(g, 2))) == tables_of_world(open_world(world_of(g, 1))));
}

}  // TEST_SUITE
