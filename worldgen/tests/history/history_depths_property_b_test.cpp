// Part b of the P5-07 depths property tests (spec P5-07 r2 16 item 13, 14): the shipped data at 512 and determinism. Split from
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

TEST_CASE("P5-07 13, 16 item 13: the shipped data, seeds 0 to 9 at 512: the invariants") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        check_world(p503run::world_path(seed, 512), conflict_test::shipped(), "seed " + std::to_string(seed) + " at 512");
    }
}

TEST_CASE("P5-07 14: determinism: depths-small twice, and at one and four threads") {
    P507_REQUIRE_INTEGRATED();
    const std::string a = p507api::generated(3, 512, p507api::depths_small(), "det-a");
    const std::string b = p507api::generated(3, 512, p507api::depths_small(), "det-b");
    CHECK(dbtest::read_file(a) == dbtest::read_file(b));
    const std::string four = p507api::generated(3, 512, p507api::depths_small(), "det-4", 4);
    CHECK(dbtest::read_file(a) == dbtest::read_file(four));
}

}  // TEST_SUITE
