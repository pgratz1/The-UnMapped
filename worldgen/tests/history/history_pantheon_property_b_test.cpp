// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1) 16 item 14, part b: the shipped data at 512, seeds 0 to 9 (the
// invariants of 13), and determinism (14): pantheon-small twice and at one and four threads, byte for byte.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>

#include "p5_03_relations_runs.hpp"
#include "p5_08_property_common.hpp"
#include "worldgen/db/testing.hpp"

TEST_SUITE("history") {

TEST_CASE("P5-08 13, 16 item 14: the shipped data, seeds 0 to 9 at 512: the invariants") {
    P508_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string path = p503run::world_path(seed, 512);
        pantheon_prop::check_world(path, conflict_test::shipped(), "shipped seed " + std::to_string(seed));
        MESSAGE("shipped seed " << seed << ":" << pantheon_prop::stats_line(pantheon_prop::rows_of(path)));
    }
}

TEST_CASE("P5-08 14: determinism: pantheon-small twice, and at one and four threads") {
    P508_REQUIRE_INTEGRATED();
    const std::string a = p508api::generated(4, 512, p508api::pantheon_small(), "det-a");
    const std::string b = p508api::generated(4, 512, p508api::pantheon_small(), "det-b");
    CHECK(dbtest::read_file(a) == dbtest::read_file(b));
    const std::string four = p508api::generated(4, 512, p508api::pantheon_small(), "det-4", 4);
    CHECK(dbtest::read_file(a) == dbtest::read_file(four));
}

}  // TEST_SUITE
