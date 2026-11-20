// Spec P3-04 16.3 and 16.4: the 1024 world cases of
// worldgen/tests/golden/p3-04-engine.txt (the reference
// reference/p3_engine_reference.py): tables, [history], phases and every
// sentence; the worked example of 17 with both ForcedTension variants
// through run_engine; R1 and Y2. In the doctest suite "history-slow"
// (worldgen-tests -ts=history-slow --no-skip). Split from
// history_engine_golden_test.cpp (P3-08 4 item 1); the helpers are in
// p3_04_engine_golden_support.hpp.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p3_04_engine_golden_support.hpp"

using namespace engine_golden;

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-04 16.4: the 1024 world cases: tables, [history], phases and every sentence") {
    for (const GoldenWorld& g : golden().worlds) {
        if (g.map_size == 1024 && g.tension == "none") {
            check_world_case(g);
        }
    }
}

TEST_CASE("P3-04 17, 16.4: example-42-1024 with ForcedTension(500, 1000, 1) and (1, 5000, 1) through run_engine") {
    const GoldenWorld& base = golden().world("example-42-1024");
    const db::WorldReader part = open_world(world_of(base, 1, "chronology"));
    const eng::HistoryInputs in = eng::inputs_from_world(part, shipped());
    for (const char* name : {"example-42-1024-ft500", "example-42-1024-ft1"}) {
        const GoldenWorld& g = golden().world(name);
        CAPTURE(g.name);
        Config cfg;
        cfg.handler = eng::make_stub();
        cfg.catalogue = eng::catalogue_of(shipped());
        if (g.tension == "ForcedTension(500,1000,1)") {
            cfg.add_tension(eng::forced_tension(500, 1000, 1));
        } else {
            cfg.add_tension(eng::forced_tension(1, 5000, 1));
        }
        const Tables t = eng::run_engine(shipped(), in, cfg.get());
        CHECK(table_hashes(t) == g.tables);
        CHECK(phases_text(t, true) == g.phases);
        std::vector<std::string> lines = report_lines(t, 6000);
        CHECK(lines == g.report);
    }
}

TEST_CASE("P3-04 16.3 items 19, 21: R1 and Y2 on the 1024 world cases") {
    for (const GoldenWorld& g : golden().worlds) {
        if (g.map_size == 1024 && g.tension == "none") {
            check_r1(g);
            check_resume(g);
        }
    }
}

}  // TEST_SUITE
