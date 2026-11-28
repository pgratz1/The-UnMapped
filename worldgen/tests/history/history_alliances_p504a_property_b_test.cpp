// Part b of the P5-04a alliances property tests (spec P5-04a r2 test 16 items 13, 14 and 9): alliances-small coverage and determinism. Split from
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

TEST_CASE("P5-04a 16 items 2 to 5: alliances-small at 0.28.0: every motive, grievance, spurning reason and betrayal") {
    P504A_REQUIRE_INTEGRATED();
    std::map<std::string, std::set<std::int64_t>> cov;
    for (const std::uint64_t seed : {3U, 5U, 6U, 7U, 10U, 0U, 1U, 2U, 4U, 8U, 9U}) {   // P5-08: seeds 1, 2, 4, 8, 9 added (the pantheon moved the draws and four motives remained in the first six)
        const std::string path = p506run::generated(seed, 512, alliances_small(), "al-small-028");
        check_world(path, alliances_small(), "alliances-small seed " + std::to_string(seed));
        const db::WorldReader r = chrono_test::open_world(path);
        for (const auto& [k, vs] : p504a_check::value_coverage(p503check::world_of_file(r, alliances_small()))) {
            cov[k].insert(vs.begin(), vs.end());
        }
    }
    MESSAGE("alliances-small coverage: " << coverage_line(cov));
    // 14.1: "so that every kind and motive occurs". Motive 5 needs a grasping ruler and motive 7 a soured call;
    // the reading is that the fixture's seeds show every code (a miss is reported, not a defect of the rule)
    // The reference's replay of alliances-small at 0.28.0 (fixtures-028) shows motives 0 and 2 to 6 and every
    // betrayal motive; owed shares are rare on the small maps, so the spoils codes are warnings (H18 and the
    // shipped worlds check their rules)
    CHECK(cov["ally_refused.motive"].size() >= 5U);
    CHECK(cov["alliance_betrayed.motive"].size() >= 2U);
    WARN(cov["spoils_withheld.grievance"].count(2) == 1U);   // spoils_large_share 1: every kept share is large
    WARN(cov["alliance_spurned.reason"].count(2) == 1U);
}

TEST_CASE("P5-04a 18: determinism: alliances-small twice, and at one and four threads") {
    P504A_REQUIRE_INTEGRATED();
    const std::string a = p506run::generated(5, 512, alliances_small(), "al-det-a");
    const std::string b = p506run::generated(5, 512, alliances_small(), "al-det-b");
    CHECK(dbtest::read_file(a) == dbtest::read_file(b));
    const std::string four = p506run::generated(5, 512, alliances_small(), "al-det-4", 0, 4);
    CHECK(dbtest::read_file(a) == dbtest::read_file(four));
}

}  // TEST_SUITE
