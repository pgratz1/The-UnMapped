// Part d of the P5-07 depths property tests (spec P5-07 r2 16 item 13, 14): the warren rules and resume. Split from
// history_depths_property_test.cpp for the CTest shards; the cases are moved unchanged. The original header
// comment (what the cases check) is kept in part a.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p3_04_engine_api.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_07_api.hpp"
#include "p5_07_checks.hpp"
#include "p5_07_depths_property_common.hpp"
#include "worldgen/db/testing.hpp"

using p503api::V;
using depths_prop::check_world;
using depths_prop::paths_of;
using depths_prop::rows_of;
using depths_prop::stats_line;
namespace wd = worldgen::data;

TEST_SUITE("history") {

TEST_CASE("P5-07 r2 6.10, 13 items 12 and 13: a warren opens no site and sells no arms; a goblin realm is no seller") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string path = p507api::generated(seed, 512, p507api::depths_small(), "small");
        const p503check::World w = rows_of(path, p507api::depths_small());
        const p503check::Index x(w);
        std::set<std::int64_t> delvers;
        for (const std::int64_t e : x.events_of("warren_delved")) delvers.insert(x.part(e, 1));
        for (const eng::Row& s : w.rows.t.site) {
            if (p503check::at(s, 2) == p507api::SITE_MINE) {
                CAPTURE(p503check::at(s, 0));
                CHECK(delvers.count(p503check::at(s, 7)) == 0U);   // no warren delver founded a mine
            }
        }
        // every deep_woken without a site is a warren's: its settlement has a warren_delved before it
        for (const std::int64_t e : x.events_of("deep_woken")) {
            if (x.part(e, 3) != 0) continue;
            bool warren = false;
            for (const std::int64_t d : x.events_of("warren_delved")) {
                warren = warren || (d < e && p503check::at(*x.ev.at(d), 6) == p503check::at(*x.ev.at(e), 6));
            }
            CHECK(warren);
        }
    }
}

TEST_CASE("P5-07 14, P3-04 16.3 Y2: resume from --until chronology on a depths-small world with a waking") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string full = p507api::generated(seed, 512, p507api::depths_small(), "small");
        const p503check::World w = rows_of(full, p507api::depths_small());
        if (p503check::Index(w).events_of("deep_woken").empty()) continue;
        CAPTURE(seed);
        const std::string part = p507api::generated(seed, 512, p507api::depths_small(), "small", 1, "chronology");
        const eng::Tables resumed = eng::history_stage(
            p507api::depths_small(), eng::inputs_from_world(chrono_test::open_world(part), p507api::depths_small()));
        const eng::Tables stored = engine_test::tables_of_world(chrono_test::open_world(full));
        CHECK(resumed.event == stored.event);
        CHECK(resumed.site == stored.site);
        CHECK(resumed.monster == stored.monster);
        CHECK(resumed == stored);
        return;
    }
    FAIL("no depths-small world of seeds 0 to 9 at 512 has a waking");
}

}  // TEST_SUITE
