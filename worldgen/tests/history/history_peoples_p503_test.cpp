// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) test 24.1 item 8, the peoples' part (11): the road loss
// formula; on the rows, every kind-31 abandonment loses at least P x abandon_loss_min_permille / 1000 (or P - 1)
// of its P = settled + lost, its destination lies within abandon_reach_hops (open borders, P3-05 4.4), and with
// abandon_reach_hops 0 the destination shares the region; fading (11.2): with ghost_fade_years 0 no ruin of
// endings 2 to 6 fades and no ruin at an open site fades; with ghost_fade_years 1 such ruins may fade; ending 6.
//
// In the -peoples shard (24.1). Written from the spec before the implementation. Library names: the adapter's.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "p5_03_relations_formulas.hpp"
#include "p5_03_relations_runs.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

void check_abandonments(const p503check::World& w) {
    const p503check::Index x(w);
    const wd::ContentSet& c = *w.content;
    const std::int64_t reach = p503api::tuning_int(c, "peoples", "abandon_reach_hops");
    const std::int64_t lmin = p503api::tuning_int(c, "peoples", "abandon_loss_min_permille");
    for (const std::int64_t e : x.events_of("settlement_abandoned_to")) {
        CAPTURE(e);
        const std::int64_t s = x.part(e, 1), t = x.part(e, 3), settled = x.val(e, 0), lost = x.val(e, 1);
        const std::int64_t p = settled + lost;
        CHECK(lost >= std::min(p * lmin / 1000, std::max<std::int64_t>(0, p - 1)));
        const auto hp = x.hops(p503check::at(*x.stl.at(s), 4));
        const std::int64_t rt = p503check::at(*x.stl.at(t), 4);
        CHECK(hp.count(rt) == 1U);
        if (hp.count(rt) == 1U) CHECK(hp.at(rt) <= reach);
    }
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 11.1: the dead of the road") {
    CHECK(rel_f::road_loss(1000, 50, 0) == 50);
    CHECK(rel_f::road_loss(1000, 50, 150) == 200);
    CHECK(rel_f::road_loss(39, 50, 0) == 1);
    CHECK(rel_f::road_loss(19, 50, 0) == 0);
    CHECK(rel_f::road_loss(1, 1000, 0) == 0);   // min(road, P - 1)
    CHECK(rel_f::road_loss(0, 50, 0) == 0);
}

TEST_CASE("P5-03 11.1: road losses and the reach of a destination, shipped data (512 worlds, seeds 42 and 0)") {
    for (const std::uint64_t seed : {42U, 0U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::world_rows(p503run::world_path(seed, 512));
        check_abandonments(w);
    }
}

TEST_CASE("P5-03 11.1: abandon_reach_hops 0 and the stress data: destinations in the region, losses") {
    const wd::ContentSet c =
        p503api::with_history(conflict_test::stress_content(), "peoples", {{"abandon_reach_hops", p503api::V(0)}});
    std::int64_t without = 0;
    // stress 13 added at integration: with the calibrated data it is the stress map with an abandonment of kind 16
    // (p3-06-conflict.txt, stress-13 events_settlement_abandoned = 1); at 0.25.0 (P5-03a) stress 5 is one
    // (stress-5 events_settlement_abandoned = 1; stress-13 has none)
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 5U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, c);
        check_abandonments(w);
        without += static_cast<std::int64_t>(p503check::Index(w).events_of("settlement_abandoned").size());
    }
    CHECK(without > 0);   // places with no destination in reach are abandoned without one (kind 16)
}

TEST_CASE("P5-03 11.2: fading by ending and open site (ghost_fade_years 0); ghost_fade_years 1 lets ghosts fade"
          * doctest::skip()) {
    // Superseded by P5-06 25.1 (b) (0.27.0): ghost_fade_years is removed and no ruin fades; P5-06 18.1 item 20 and
    // invariant 14 test the rule that replaces this one. Kept, skipped, for the record.
    const wd::ContentSet fade_fast =
        p503api::with_history(conflict_test::stress_content(), "peoples", {{"ghost_fade_years", p503api::V(1)}});
    std::int64_t faded_ghosts = 0;
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, conflict_test::stress_content());
        CHECK(p503check::violations(w) == std::vector<std::string>{});
        const p503check::World f = p503run::stage_fixture(seed, fade_fast);
        p503check::Options o;
        o.ghost_fade_zero = false;
        CHECK(p503check::violations(f, o) == std::vector<std::string>{});
        for (const eng::Row& s : f.rows.t.settlement) {
            faded_ghosts += p503check::at(s, 10) == 4 && p503check::at(s, 14) >= 2 ? 1 : 0;
        }
    }
    MESSAGE("ghost places faded with ghost_fade_years 1: " << faded_ghosts);
}

}  // TEST_SUITE
