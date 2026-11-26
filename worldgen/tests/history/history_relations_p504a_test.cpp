// Spec P5-04a (docs/specs/p5-04a-relations-alliances-followups.md r2) test 16 items 6, 7 and 13 (relations): every
// disposition_turned's change against its band move (7.1), the cool-down (7.2: a turn inside turn_cooldown_years of
// the pair's last is not made on its direction's fallback causes alone; rows cannot show a contribution itself, so
// the full rule is the reference's replay), 8's rule on every expulsion (a people held expel_settled_years or more is
// expelled only within expel_fresh_years of a turn into hatred), on the shipped data's worlds (seeds 0 to 9 at 512; in
// "relations-slow" seeds 0 to 19 at 1024) and on relations-small at 0.28.0 (turn_cooldown_years 0, 14.2: every change
// 0 to 3 happens). p5_04a_checks.hpp: relation_violations, as the reference's p5_03_relations_reference.py
// violations_028. Every case starts with P504A_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"
#include "p5_04a_api.hpp"
#include "p5_04a_checks.hpp"
#include "p5_06_figures_runs.hpp"

namespace {

namespace db = worldgen::db;
namespace wd = worldgen::data;
using p503api::V;

const wd::ContentSet& relations_small_028() {
    static const wd::ContentSet c =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"turn_cooldown_years", V(0)}});
    return c;
}

std::vector<std::string> violations(const std::string& path, const wd::ContentSet& content) {
    const db::WorldReader r = chrono_test::open_world(path);
    return p504a_check::relation_violations(p503check::world_of_file(r, content), p504a_check::tuning_of(content));
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04a 7, 8: change, cool-down and long-held expulsions on the shipped worlds, seeds 0 to 9 at 512") {
    P504A_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        CAPTURE(seed);
        CHECK(violations(p503run::world_path(seed, 512), conflict_test::shipped()) == std::vector<std::string>{});
    }
}

TEST_CASE("P5-04a 7.1, 14.2: relations-small at 0.28.0: every change 0 to 3 happens; the invariants hold") {
    P504A_REQUIRE_INTEGRATED();
    std::set<std::int64_t> changes;
    for (const std::uint64_t seed : {1U, 2U, 3U, 4U}) {
        CAPTURE(seed);
        const std::string path = p506run::generated(seed, 512, relations_small_028(), "rel-small-028");
        CHECK(violations(path, relations_small_028()) == std::vector<std::string>{});
        const db::WorldReader r = chrono_test::open_world(path);
        const auto cov = p504a_check::value_coverage(p503check::world_of_file(r, relations_small_028()));
        if (const auto it = cov.find("disposition_turned.change"); it != cov.end()) {
            changes.insert(it->second.begin(), it->second.end());
        }
    }
    CHECK(changes == std::set<std::int64_t>{0, 1, 2, 3});
}

}  // TEST_SUITE

TEST_SUITE("relations-slow" * doctest::skip()) {

TEST_CASE("P5-04a 7, 8: the relations invariants at 1024, seeds 0 to 19") {
    P504A_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        CAPTURE(seed);
        CHECK(violations(p503run::world_path(seed, 1024), conflict_test::shipped()) == std::vector<std::string>{});
    }
}

}  // TEST_SUITE
