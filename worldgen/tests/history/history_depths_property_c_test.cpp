// Part c of the P5-07 depths property tests (spec P5-07 r2 16 item 13, 14): I0, I1 and the aid settings. Split from
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

TEST_CASE("P5-07 14 I0: deep_delve_permille 0: no deep event; forge_depth_permille 0 and 15 give the same history") {
    P507_REQUIRE_INTEGRATED();
    for (const std::uint64_t seed : {0U, 42U}) {
        CAPTURE(seed);
        const std::string z0 = p507api::generated(seed, 512, p507api::zero(0), "zero-0");
        const std::string z15 = p507api::generated(seed, 512, p507api::zero(15), "zero-15");
        const p503check::World w = rows_of(z0, p507api::zero(0));
        const p503check::Index x(w);
        for (const auto& [id, key] : p507api::new_kinds()) {
            CAPTURE(key);
            CHECK(x.events_of(key).empty());
        }
        for (const eng::Row& m : w.rows.t.monster) CHECK(p503check::at(m, 2) <= 6);
        const eng::Tables t0 = engine_test::tables_of_world(chrono_test::open_world(z0));
        const eng::Tables t15 = engine_test::tables_of_world(chrono_test::open_world(z15));
        CHECK(t0.event == t15.event);
        CHECK(t0.event_value == t15.event_value);
        CHECK(t0.settlement == t15.settlement);
        CHECK(t0.relation == t15.relation);
        CHECK(t0.site == t15.site);
        CHECK(t0.monster == t15.monster);
        // the catalogue rows of 93 to 96 and the monster_kind rows 7 to 9 are written whatever the tuning (14)
        CHECK(t0.monster_kind.size() == 9U);
    }
}

TEST_CASE("P5-07 r2 14 I1 (as rows show it): delving_permille 0 and deep_aid_permille 0: no warren, no aid") {
    P507_REQUIRE_INTEGRATED();
    for (const std::uint64_t seed : {0U, 1U, 2U}) {
        CAPTURE(seed);
        const std::string path = p507api::generated(seed, 512, p507api::i1_content(), "i1");
        check_world(path, p507api::i1_content(), "i1 seed " + std::to_string(seed));
        const p503check::World w = rows_of(path, p507api::i1_content());
        const p503check::Index x(w);
        CHECK(x.events_of("warren_delved").empty());
        CHECK(x.events_of("warren_sealed").empty());
        CHECK(x.events_of("ally_aided_hold").empty());
        for (const std::int64_t e : x.events_of("deep_woken")) CHECK(x.part(e, 3) != 0);   // every waking at a mine
        for (const std::int64_t e : x.events_of("hold_lost")) CHECK(x.val(e, 2) <= 2);     // the dwarven wordings
    }
    // the other half of I1 (the tables equal r1's on the same data) needs a build of r1: the reference's I1 replay
    // (p5_07_depths_reference.py fixtures) checks it against its own r1 rules
}

TEST_CASE("P5-07 r2 6.11, 13 item 14: deep_aid_max 0 gives no aid; deep_aid_hops 0 only allies in the hold's region") {
    P507_REQUIRE_INTEGRATED();
    static const wd::ContentSet none = p503api::with_history(p507api::depths_small(), "monsters", {{"deep_aid_max", V(0)}});
    static const wd::ContentSet near = p503api::with_history(p507api::depths_small(), "monsters", {{"deep_aid_hops", V(0)}});
    for (const std::uint64_t seed : {0U, 3U}) {
        CAPTURE(seed);
        const p503check::World a = rows_of(p507api::generated(seed, 512, none, "aid-none"), none);
        CHECK(p503check::Index(a).events_of("ally_aided_hold").empty());
        CHECK(p507_check::violations(a, p507_check::tuning_of(none)) == std::vector<std::string>{});
        const p503check::World b = rows_of(p507api::generated(seed, 512, near, "aid-near"), near);
        const p503check::Index x(b);
        for (const std::int64_t e : x.events_of("ally_aided_hold")) {
            const std::int64_t q = x.part(e, 4);
            CHECK(p503check::at(*x.stl.at(q), 4) == p503check::at(*x.ev.at(e), 5));   // the origin in the hold's region
        }
        CHECK(p507_check::violations(b, p507_check::tuning_of(near)) == std::vector<std::string>{});
    }
}

}  // TEST_SUITE
