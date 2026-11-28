// Spec P5-11 (docs/specs/p5-11-artifacts.md r1) 16 item 13, part a: the invariants of 13 (items 1 to 8 from rows:
// p5_11_checks.hpp; item 9 through P5-08's checks) on artifacts-small (4.6) at 512, seeds 0 to 9, and the paths of 6
// over them (4.6: "every path of 6 occurs in a 512 world"; the required ones fail, the others are reported). The win
// chances and hunts of item 7 are the reference's replay. Every case starts with P511_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <string>

#include "p5_11_property_common.hpp"

TEST_SUITE("history") {

TEST_CASE("P5-11 13, 16 item 13: artifacts-small, seeds 0 to 9 at 512: the invariants and the paths of 6") {
    P511_REQUIRE_INTEGRATED();
    std::set<std::string> seen;
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string path = p511api::generated(seed, 512, p511api::artifacts_small(), "small");
        artifacts_prop::check_world(path, p511api::artifacts_small(), "artifacts-small seed " + std::to_string(seed));
        const p511_check::World w = artifacts_prop::rows_of(path);
        for (const std::string& p : p511_check::paths_of(w)) seen.insert(p);
        MESSAGE("artifacts-small seed " << seed << ":" << artifacts_prop::stats_line(w));
    }
    for (const std::string& p : artifacts_prop::required_paths()) {
        CAPTURE(p);
        CHECK(seen.count(p) == 1U);
    }
    for (const std::string& p : artifacts_prop::reported_paths()) {
        if (seen.count(p) == 0U) MESSAGE("artifacts-small seeds 0 to 9 show no '" << p << "' (reported, not failed)");
    }
}

TEST_CASE("P5-11 6.9, 11.1: every artifact lies somewhere at E; the timeline reads equal the rows (artifacts-small)") {
    P511_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 3; ++seed) {
        CAPTURE(seed);
        const std::string path = p511api::generated(seed, 512, p511api::artifacts_small(), "small");
        const worldgen::db::WorldReader r = chrono_test::open_world(path);
        const p511_check::World w = p511_check::world_of_file(r);
        auto lh = worldgen::history::load_history(r);
        REQUIRE_MESSAGE(lh.ok(), (lh.ok() ? "" : lh.error().message));
        const worldgen::history::Timeline& t = lh.value().timeline;
        const std::int64_t E = w.base.end_year;
        for (const auto& [aid, ap] : w.art) {
            CAPTURE(aid);
            const std::int64_t made = p511_check::I(*ap, 9);
            CHECK(p511api::artifact_at(t, aid, made - 1).state == 0);
            for (const std::int64_t y : {made, (made + E) / 2, E - 1}) {
                CAPTURE(y);
                const p511api::Place want = p511_check::artifact_at(w, aid, y);
                const p511api::Place got = p511api::artifact_at(t, aid, y);
                CHECK(got.state == want.state);
                CHECK(got.holder_type == want.holder_type);
                CHECK(got.holder == want.holder);
                CHECK(got.since == want.since);
                CHECK(got.event == want.event);
                if (want.state >= p511api::LOST) CHECK(got == want);
                if (want.state == p511api::HOARDED) CHECK(got.site == want.site);   // reading A-R16: the lair
                if (want.state <= p511api::BORNE) CHECK((got.region == 0 && got.site == 0 && got.x == 0 && got.y == 0));
            }
            const p511api::Place end = p511_check::artifact_at(w, aid, E - 1);
            CHECK(end.state == p511_check::I(*ap, 13));
        }
        for (const std::int64_t y : {E / 3, E - 1}) {
            CHECK(p511api::artifacts_placed(t, y) == p511_check::artifacts_placed(w, y));
            for (const auto& [c, row] : w.base.civ) {
                CHECK(p511api::artifacts_held(t, p511api::ENTITY_CIV, c, y) ==
                      p511_check::artifacts_held(w, p511api::ENTITY_CIV, c, y));
            }
        }
    }
}

}  // TEST_SUITE
