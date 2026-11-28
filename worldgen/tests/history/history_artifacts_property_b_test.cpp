// Spec P5-11 (docs/specs/p5-11-artifacts.md r1) 16 items 13 and 14, part b: the invariants of 13 on the shipped data
// at 512 (seeds 0 to 9); determinism (the same world twice, and with 1 and 4 threads); I0 (14: the `zero` tuning,
// occasions empty, gives no artifact, no smith and no artifact event, while artifact_kind and kinds 111 to 116 are
// written); resume (P3-04 16.3) of an artifacts-small world whose history holds an artifact made, taken and hoarded
// before the resume point; and the slow suite artifacts-slow: the shipped data at 1024 on seeds 0 to 19 and
// `--event-intensity turbulent` on seeds 0 to 4. Every case starts with P511_REQUIRE_INTEGRATED().
//
// The other half of I0 (the history tables equal those of the same build without ArtifactsSubsystem installed) needs a
// build without it: the reference's I0 replay (p5_11_artifacts_reference.py fixtures) checks it.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p3_04_engine_api.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_11_property_common.hpp"

TEST_SUITE("history") {

TEST_CASE("P5-11 13, 16 item 13: the shipped data, seeds 0 to 9 at 512: the invariants") {
    P511_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string path = p503run::world_path(seed, 512);
        artifacts_prop::check_world(path, conflict_test::shipped(), "shipped seed " + std::to_string(seed));
        MESSAGE("shipped seed " << seed << ":" << artifacts_prop::stats_line(artifacts_prop::rows_of(path)));
    }
}

TEST_CASE("P5-11 14: determinism: artifacts-small twice and with 1 and 4 threads gives the same tables") {
    P511_REQUIRE_INTEGRATED();
    const std::string a = p511api::generated(4, 512, p511api::artifacts_small(), "small");
    const std::string b = p511api::generated(4, 512, p511api::artifacts_small(), "small-again");
    const std::string c = p511api::generated(4, 512, p511api::artifacts_small(), "small", 4);
    for (const char* t : {"artifact_kind", "artifact", "artifact_holding", "figure", "event", "event_cause",
                          "event_participant", "event_value"}) {
        CAPTURE(t);
        const auto ra = chrono_test::rows_of(chrono_test::open_world(a), t);
        CHECK(ra == chrono_test::rows_of(chrono_test::open_world(b), t));
        CHECK(ra == chrono_test::rows_of(chrono_test::open_world(c), t));
    }
}

TEST_CASE("P5-11 14 I0: occasions empty: no artifact, no smith, no artifact event; the catalogue and kinds written") {
    P511_REQUIRE_INTEGRATED();
    for (const std::uint64_t seed : {0U, 42U}) {
        CAPTURE(seed);
        const p511_check::World w = artifacts_prop::rows_of(p511api::generated(seed, 512, p511api::zero(), "zero"));
        CHECK(w.art.empty());
        CHECK(w.hold.empty());
        CHECK(p511_check::stat(w, "smiths") == 0);
        for (const auto& [id, key] : p511api::new_kinds()) {
            CAPTURE(key);
            CHECK(w.of(key).empty());
            CHECK(w.base.key.count(id) == 1U);
        }
        CHECK(w.kind.size() == conflict_test::shipped().find_kind("artifact")->definitions.size());
    }
}

TEST_CASE("P5-11 16 item 14: resume (P3-04 16.3) gives the same world with an artifact made, taken and hoarded") {
    P511_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string full = p511api::generated(seed, 512, p511api::artifacts_small(), "small");
        const p511_check::World w = artifacts_prop::rows_of(full);
        if (w.of("artifact_taken").empty() || w.of("artifact_hoarded").empty()) continue;
        CAPTURE(seed);
        const std::string part = p511api::generated(seed, 512, p511api::artifacts_small(), "small", 1, "chronology");
        const eng::Tables resumed = eng::history_stage(
            p511api::artifacts_small(), eng::inputs_from_world(chrono_test::open_world(part), p511api::artifacts_small()));
        const eng::Tables stored = engine_test::tables_of_world(chrono_test::open_world(full));
        CHECK(resumed.event == stored.event);
        CHECK(resumed == stored);
        // the artifact tables are not among eng::Tables' (P3-04's): their rows follow from the events checked here,
        // and the reference's golden lines hash them
        return;
    }
    MESSAGE("no artifacts-small seed 0 to 9 at 512 holds a taking and a hoard: resume not run");
}

}  // TEST_SUITE

TEST_SUITE("artifacts-slow" * doctest::skip()) {

TEST_CASE("P5-11 13, 16 item 13 (slow): the shipped data at 1024, seeds 0 to 19") {
    P511_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        const std::string path = p503run::world_path(seed, 1024);
        artifacts_prop::check_world(path, conflict_test::shipped(), "shipped 1024 seed " + std::to_string(seed));
        MESSAGE("shipped 1024 seed " << seed << ":" << artifacts_prop::stats_line(artifacts_prop::rows_of(path)));
    }
}

TEST_CASE("P5-11 13, 16 item 13 (slow): --event-intensity turbulent at 1024, seeds 0 to 4") {
    P511_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 5; ++seed) {
        const std::string path = p511api::generated(seed, 1024, conflict_test::shipped(), "turbulent", 1, "", "turbulent");
        artifacts_prop::check_world(path, conflict_test::shipped(), "turbulent 1024 seed " + std::to_string(seed));
    }
}

}  // TEST_SUITE
