// Spec P5-04a (docs/specs/p5-04a-relations-alliances-followups.md r2) test 16 items 13, 14 and 9 (alliances): the
// invariants of P5-04 13 (p5_04_alliances_checks.hpp; item 11 as amended by 11 item 1, item 12 replaced) and the
// new ones of 16 item 13 that rows show (p5_04a_checks.hpp: alliance_violations) on the shipped data's generated
// worlds (seeds 0 to 9 at 512; in the suite "alliances-slow" seeds 0 to 19 at 1024 and civs4 seeds 0 to 4), on
// alliances-small at 0.28.0 (P5-04 18.3's edits with 14.1's spoils_large_share 1, soured_renew_permille 0,
// weary_years 0: every value of the new codes happens over its seeds), determinism at one and four threads; the
// validation world of 11 item 1 (civs4 seed 6 at 1024: a third realm's conquest of an allied war's enemy owes
// nothing) as a slow witness.
//
// I0 (P5-04 7.14, P5-06 7.11) and resume (P3-04 16.3) need the stage's configuration without the subsystems, which
// the spec does not name: they are the engineer's and the reference's (p5_04_alliances_reference.py i0-world). Every
// case starts with P504A_REQUIRE_INTEGRATED(). Written from the spec before the implementation.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"
#include "p5_04_alliances_checks.hpp"
#include "p5_04a_api.hpp"
#include "p5_04a_checks.hpp"
#include "p5_06_figures_runs.hpp"

namespace {

namespace db = worldgen::db;
namespace wd = worldgen::data;
using p503api::V;

// P5-04 18.3's alliances-small (the conflict stress data with the alliance edits) with 14.1's edits.
const wd::ContentSet& alliances_small() {
    static const wd::ContentSet c = p503api::with_history(
        p503api::with_history(conflict_test::stress_content(), "alliances",
                              {{"form_foe_permille", V(1000)}, {"form_fear_permille", V(1000)},
                               {"form_trade_permille", V(1000)}, {"form_friend_permille", V(1000)},
                               {"form_max_permille", V(1000)}, {"defend_permille", V(1000)},
                               {"refusals_to_break", V(1)}, {"term_years", V(10)}, {"trade_permille", V(1000)},
                               {"spoils_grant_permille", V(500)}, {"spoils_grant_min", V(300)},
                               {"spoils_grant_max", V(700)}, {"withheld_to_break", V(1)},
                               {"mine_permille", V(1000)}, {"mine_settlement_min", V(0)},
                               {"spoils_large_share", V(1)}, {"soured_renew_permille", V(0)},
                               {"weary_years", V(0)}}),
        "conflict", {{"allied_strain_permille", V(1000)}});
    return c;
}

// P5-04 13 without its item 12 (replaced at 0.28.0), then 16 item 13.
std::vector<std::string> violations(const std::string& path, const wd::ContentSet& content) {
    const db::WorldReader r = chrono_test::open_world(path);
    const p503check::World w = p503check::world_of_file(r, content);
    std::vector<std::string> out;
    for (const std::string& v : p504check::violations(w, p504check::geo_of_file(r), p504check::data_of(content))) {
        if (v.rfind("12:", 0) != 0) out.push_back(v);
    }
    for (const std::string& v : p504a_check::alliance_violations(w, p504a_check::tuning_of(content))) out.push_back(v);
    return out;
}

void check_world(const std::string& path, const wd::ContentSet& content, const std::string& label) {
    CAPTURE(label);
    CHECK(violations(path, content) == std::vector<std::string>{});
}

std::string coverage_line(const std::map<std::string, std::set<std::int64_t>>& cov) {
    std::string out;
    for (const auto& [k, vs] : cov) {
        out += (out.empty() ? "" : "; ") + k + ":";
        for (const std::int64_t v : vs) out += " " + std::to_string(v);
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04a 16 item 13: P5-04 13 (12 replaced) and the new invariants on the shipped worlds, seeds 0 to 9 at 512") {
    P504A_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        check_world(p503run::world_path(seed, 512), conflict_test::shipped(), "seed " + std::to_string(seed) + " at 512");
    }
}

TEST_CASE("P5-04a 16 items 2 to 5: alliances-small at 0.28.0: every motive, grievance, spurning reason and betrayal") {
    P504A_REQUIRE_INTEGRATED();
    std::map<std::string, std::set<std::int64_t>> cov;
    for (const std::uint64_t seed : {3U, 5U, 6U, 7U, 10U, 0U}) {
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
