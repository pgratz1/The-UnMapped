// Part a of the P5-04a alliances property tests (spec P5-04a r2 test 16 items 13, 14 and 9): the invariants on the shipped worlds at 512 and the slow cases. Split from
// history_alliances_p504a_property_test.cpp for the CTest shards; the cases are moved unchanged (see the
// header comment of p5_04a_alliances_property_common.hpp for the helpers). Every case starts with
// P504A_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"
#include "p5_04_alliances_checks.hpp"
#include "p5_04a_alliances_property_common.hpp"
#include "p5_04a_api.hpp"
#include "p5_04a_checks.hpp"
#include "p5_06_figures_runs.hpp"

namespace db = worldgen::db;
namespace wd = worldgen::data;
using alp504a::alliances_small;
using alp504a::check_world;
using alp504a::coverage_line;

TEST_SUITE("history") {

TEST_CASE("P5-04a 16 item 13: P5-04 13 (12 replaced) and the new invariants on the shipped worlds, seeds 0 to 9 at 512") {
    P504A_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        check_world(p503run::world_path(seed, 512), conflict_test::shipped(), "seed " + std::to_string(seed) + " at 512");
    }
}

}  // TEST_SUITE

TEST_SUITE("alliances-slow" * doctest::skip()) {

TEST_CASE("P5-04a 16 item 13: the invariants at 1024, seeds 0 to 19") {
    P504A_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        check_world(p503run::world_path(seed, 1024), conflict_test::shipped(), "seed " + std::to_string(seed) + " at 1024");
    }
}

TEST_CASE("P5-04a 16 item 13: the invariants on civs4 worlds at 1024, seeds 0 to 4; 11 item 1's witness civs4 seed 6") {
    P504A_REQUIRE_INTEGRATED();
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 4U, 6U}) {
        // seed 6 (P5-04 validation finding 1): P5-04 13 item 11 as amended checks that every share's end event is a
        // peace or war_won of its principal, or the principal's own conquest of the enemy
        check_world(p506run::generated(seed, 1024, conflict_test::shipped(), "civs4", 4), conflict_test::shipped(),
                    "civs4 seed " + std::to_string(seed));
    }
}

}  // TEST_SUITE
