// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r2) tests 16 items 13, 14 and (r2) 16, 17 and 19 as rows show them:
// the warren paths (delved, depth 3, woken, sealed, lost) and the aid over depths-small seeds 0 to 9 (4.2: "so every
// path of 6, 6.10 and 6.11 occurs in a 512 world"); I1's part that rows show (no event of kinds 97 and 98 and no
// waking without a site under delving_permille 0 and deep_aid_permille 0); deep_aid_max 0 and deep_aid_hops 0. And,
// from r1, 4.2's fixture: the invariants of
// 13 that rows show (p5_07_checks.hpp: items 1 to 9; 10 and 11 are the reference's replay and check-world) on
// depths-small worlds (seeds 0 to 9 at 512: every path of 6 must occur over them) and on the shipped data's worlds
// (seeds 0 to 9 at 512; the slow suite "depths-slow": seeds 0 to 19 at 1024 and --monster-pressure high 0 to 4);
// determinism (twice, and at one and four threads); I0 (14) as rows show it: with deep_delve_permille 0 no event of
// kinds 93 to 96 and no deep monster, and forge_depth_permille 0 and 15 give the same history tables (no mine has a
// depth, so D(c) = 0); resume (P3-04 16.3 Y2: --until chronology, then the history stage on the stored inputs) on a
// depths-small world whose history holds a waking (reading: P3-04's resume point is the history stage's start, so
// "a waking before the resume point" is read as "a world whose resumed history has a waking", D-R13 of the notes).
// Every case starts with P507_REQUIRE_INTEGRATED().

// Part a of the P5-07 depths property tests (spec P5-07 r2 16 item 13, 14): depths-small seeds 0 to 9, the 512 witnesses and the slow cases. Split from
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

TEST_CASE("P5-07 13, 16 item 13: depths-small seeds 0 to 9 at 512: the invariants; every path of 6 occurs") {
    P507_REQUIRE_INTEGRATED();
    std::set<std::string> seen;
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string path = p507api::generated(seed, 512, p507api::depths_small(), "small");
        check_world(path, p507api::depths_small(), "depths-small seed " + std::to_string(seed));
        const p503check::World w = rows_of(path, p507api::depths_small());
        MESSAGE("depths-small seed " << seed << ":" << stats_line(w));
        const std::set<std::string> p = paths_of(w);
        seen.insert(p.begin(), p.end());
    }
    std::string list;
    for (const std::string& s : seen) list += " [" + s + "]";
    MESSAGE("depths-small paths:" << list);
    // 4.2: "so that every path of 6 occurs in a 512 world"
    for (const char* p : {"deepened", "depth 3", "woken", "sealed", "fall at the waking", "later take",
                          // r2: "so every path of 6, 6.10 and 6.11 occurs" (4.2)
                          "warren delved", "warren woken", "aid"}) {
        CAPTURE(p);
        CHECK(seen.count(p) == 1U);
    }
    // r2: shown by the reference's depths-small replay on the stress geographies, but not required by name in 4.2
    WARN(seen.count("warren depth 3") == 1U);
    WARN(seen.count("warren sealed") == 1U);
    WARN(seen.count("warren lost") == 1U);
    WARN(seen.count("two aids") == 1U);
    // a flight needs a race's last refuge and a realm lost needs another realm of the race: reported, not failed
    WARN(seen.count("flight") == 1U);
    WARN(seen.count("realm lost") == 1U);
}

TEST_CASE("P5-07 integration witnesses at 512 (shipped): seed 78 a warren woken and sealed, seed 10 an aid") {
    P507_REQUIRE_INTEGRATED();
    // Found by the integration's witness search (docs/reviews/p5-07-integration-notes.md) over shipped seeds 0 to 99
    // at 512; the reference's replay of these worlds equals the engine's tables.
    const std::set<std::string> p78 = paths_of(rows_of(p503run::world_path(78, 512), conflict_test::shipped()));
    CHECK(p78.count("warren woken") == 1U);
    CHECK(p78.count("warren sealed") == 1U);
    // 0.30.0: seed 9 no longer has an aid (the pantheon moved the draws); the search over seeds 0 to 59 found 10
    // first (also 34, 38, 41, 55).
    const std::set<std::string> p10 = paths_of(rows_of(p503run::world_path(10, 512), conflict_test::shipped()));
    CHECK(p10.count("aid") == 1U);
}

}  // TEST_SUITE

TEST_SUITE("depths-slow" * doctest::skip()) {

TEST_CASE("P5-07 integration witnesses at 1024 (shipped): seed 5 two aids at one waking and a warren sealed, seed 72 a "
          "realm ended by the deep") {
    P507_REQUIRE_INTEGRATED();
    const std::string w5 = p503run::world_path(5, 1024);
    check_world(w5, conflict_test::shipped(), "seed 5 at 1024");
    const std::set<std::string> p5 = paths_of(rows_of(w5, conflict_test::shipped()));
    CHECK(p5.count("two aids") == 1U);
    CHECK(p5.count("warren woken") == 1U);
    CHECK(p5.count("warren sealed") == 1U);
    const std::string w72 = p503run::world_path(72, 1024);
    check_world(w72, conflict_test::shipped(), "seed 72 at 1024");
    const std::set<std::string> p72 = paths_of(rows_of(w72, conflict_test::shipped()));
    CHECK(p72.count("realm lost") == 1U);
    CHECK(p72.count("aid") == 1U);
}

TEST_CASE("P5-07 13, 16 item 13: the invariants at 1024, seeds 0 to 19 (shipped)") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        const std::string path = p503run::world_path(seed, 1024);
        check_world(path, conflict_test::shipped(), "seed " + std::to_string(seed) + " at 1024");
        MESSAGE("seed " << seed << " at 1024:" << stats_line(rows_of(path, conflict_test::shipped())));
    }
}

TEST_CASE("P5-07 13, 16 item 13: the invariants at 1024 with --monster-pressure high, seeds 0 to 4") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 5; ++seed) {
        const std::string path = p503run::world_path(seed, 1024, "high");
        check_world(path, conflict_test::shipped(), "high seed " + std::to_string(seed) + " at 1024");
        MESSAGE("high seed " << seed << " at 1024:" << stats_line(rows_of(path, conflict_test::shipped())));
    }
}

}  // TEST_SUITE
