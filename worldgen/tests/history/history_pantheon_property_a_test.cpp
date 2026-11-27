// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1) 16 item 14, part a: the invariants of 13 (items 1 to 9, 12, 13
// from rows: p5_08_checks.hpp) on pantheon-small (4.1) at 512, seeds 0 to 9, and the paths of 6 over them (4.1:
// "every path of 6 occurs in a 512 world"; the required ones fail, the others are reported). Items 10 and 11 (the
// force and growth factors, the stir) are the reference's replay. Every case starts with P508_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <string>

#include "p5_08_property_common.hpp"

TEST_SUITE("history") {

TEST_CASE("P5-08 13, 16 item 14: pantheon-small, seeds 0 to 9 at 512: the invariants and the paths of 6") {
    P508_REQUIRE_INTEGRATED();
    std::set<std::string> seen;
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string path = p508api::generated(seed, 512, p508api::pantheon_small(), "small");
        pantheon_prop::check_world(path, p508api::pantheon_small(), "pantheon-small seed " + std::to_string(seed));
        const p508_check::World w = pantheon_prop::rows_of(path);
        for (const std::string& p : p508_check::paths_of(w)) seen.insert(p);
        MESSAGE("pantheon-small seed " << seed << ":" << pantheon_prop::stats_line(w));
    }
    for (const std::string& p : pantheon_prop::required_paths()) {
        CAPTURE(p);
        CHECK(seen.count(p) == 1U);
    }
    for (const char* p : {"hidden", "ended suppressed", "ended fell", "sacred mark", "profane mark", "tower_taken_up",
                          "tower_abandoned"}) {
        if (seen.count(p) == 0U) MESSAGE("pantheon-small seeds 0 to 9 show no '" << p << "' (reported, not failed)");
    }
}

}  // TEST_SUITE
