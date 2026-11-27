// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1) 16 items 2, 4, 14 and 15, part c: I0 (14: act_permille 0 and
// flare_permille 0 give no act, no cult and no faction row), resume (P3-04 16.3) of a pantheon-small world whose
// history holds an act, a cult and a flare, the activity levels (4.2: distant <= normal <= meddling in acts on one
// geography), the god rows of 6.1 on seeds 0, 1, 42 and `--races elf,orc` (absent races keep their creator, the
// pantheon is the same; creator and age names as 0.29.0's), and the slow suite pantheon-slow: the shipped data at
// 1024 on seeds 0 to 19 and `--divine-activity meddling` and `distant` on seeds 0 to 4.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p3_04_engine_api.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_08_property_common.hpp"
#include "worldgen/db/testing.hpp"

namespace {

namespace wd = worldgen::data;

std::int64_t acts_of(const std::string& path) {
    return static_cast<std::int64_t>(p508_check::acts_of(pantheon_prop::rows_of(path)).size());
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-08 14 I0: act_permille 0 and flare_permille 0: no act, no cult, no flare by a god") {
    P508_REQUIRE_INTEGRATED();
    for (const std::uint64_t seed : {0U, 42U}) {
        CAPTURE(seed);
        const p508_check::World w = pantheon_prop::rows_of(p508api::generated(seed, 512, p508api::zero(), "zero"));
        for (const auto& [id, key] : p508api::new_kinds()) {
            CAPTURE(key);
            if (id <= p508api::K_SUPPRESSED) CHECK(w.of(key).empty());
        }
        CHECK(w.rows("faction").empty());
        for (const auto& [k, v] : p508_check::stats(w)) {
            if (k == "divine_flares" || k == "cults_founded" || k == "divine_acts") CHECK(v == 0);
        }
        // the catalogue rows 100 to 110 and the 15 god rows are written whatever the tuning (14)
        CHECK(w.rows("god").size() == 15U);
    }
    // the other half of I0 (the history tables equal those of the same build without the subsystem) needs a build
    // without it: the reference's I0 replay (p5_08_pantheon_reference.py fixtures) checks it
}

TEST_CASE("P5-08 16 item 15: resume (P3-04 16.3) gives the same world with an act, a cult and a flare") {
    P508_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string full = p508api::generated(seed, 512, p508api::pantheon_small(), "small");
        const p508_check::World w = pantheon_prop::rows_of(full);
        const auto st = p508_check::stats(w);
        std::int64_t acts = 0, cults = 0, flares = 0;
        for (const auto& [k, v] : st) {
            acts += k == "divine_acts" ? v : 0;
            cults += k == "cults_founded" ? v : 0;
            flares += k == "divine_flares" ? v : 0;
        }
        if (acts == 0 || cults == 0 || flares == 0) continue;
        CAPTURE(seed);
        const std::string part = p508api::generated(seed, 512, p508api::pantheon_small(), "small", 1, "chronology");
        const eng::Tables resumed = eng::history_stage(
            p508api::pantheon_small(), eng::inputs_from_world(chrono_test::open_world(part), p508api::pantheon_small()));
        const eng::Tables stored = engine_test::tables_of_world(chrono_test::open_world(full));
        CHECK(resumed.event == stored.event);
        CHECK(resumed.site == stored.site);
        CHECK(resumed.relation == stored.relation);
        CHECK(resumed == stored);
        // the faction table is not among eng::Tables' (P3-04's): its rows follow from the events checked here
        return;
    }
    MESSAGE("no pantheon-small seed 0 to 9 at 512 holds an act, a cult and a flare: resume not run");
}

TEST_CASE("P5-08 4.2, PN2: the activity levels order the acts on one geography") {
    P508_REQUIRE_INTEGRATED();
    const wd::ContentSet c = p508api::pantheon_small_with({{"act_permille", p503api::V(100)}});
    const std::int64_t d = acts_of(p508api::generated(7, 512, c, "act-d", 1, "", "distant"));
    const std::int64_t m = acts_of(p508api::generated(7, 512, c, "act-m", 1, "", "meddling"));
    const std::int64_t nn = acts_of(p508api::generated(7, 512, c, "act-n", 1, "", "normal"));
    CHECK(d <= nn);
    CHECK(nn <= m);
    CHECK(d < m);
}

TEST_CASE("P5-08 6.1, 16 item 2: the fifteen god rows on seeds 0, 1, 42 and with --races elf,orc") {
    P508_REQUIRE_INTEGRATED();
    static const char* const ids[9] = {"conquest", "craft", "death", "harvest", "lore", "oaths", "ruin", "war", "wild"};
    static const std::int64_t camps[9] = {2, 1, 2, 0, 0, 0, 2, 1, 1};
    static const std::int64_t domains[9] = {2, 5, 3, 4, 8, 7, 9, 2, 6};
    static const char* const titles[9] = {"the Red Banner", "the Hammer Under the Mountain", "the Hollow Keeper",
                                          "the Giver of Harvests", "the Lamp of the Wise", "the Keeper of Oaths",
                                          "the Burner of Halls", "the Shield of Battles", "the Hunter in the Green"};
    static const char* const plurals[6] = {"Elves", "Goblins", "Dwarves", "Faeries", "Humans", "Orcs"};
    for (const std::uint64_t seed : {0U, 1U, 42U}) {
        CAPTURE(seed);
        const p508_check::World w = pantheon_prop::rows_of(p503run::world_path(seed, 512));
        REQUIRE(w.rows("god").size() == 15U);
        for (std::size_t i = 0; i < 6U; ++i) {
            const auto& g = w.rows("god")[i];
            CHECK(p508_check::I(g, 3) == 1);
            CHECK(p508_check::I(g, 5) == static_cast<std::int64_t>(i + 1U));
            CHECK(p508_check::S(g, 4) == std::string("the Maker of ") + plurals[i]);
        }
        for (std::size_t i = 0; i < 9U; ++i) {
            CAPTURE(ids[i]);
            const auto& g = w.rows("god")[6U + i];
            CHECK(p508_check::I(g, 0) == static_cast<std::int64_t>(7U + i));
            CHECK(p508_check::I(g, 2) == camps[i]);
            CHECK(p508_check::I(g, 3) == domains[i]);
            CHECK(p508_check::S(g, 4) == titles[i]);
            CHECK(p508_check::I(g, 5) == 0);
        }
    }
    // --races elf,orc: the god table is that of all six races (absent races keep their creator), names included
    chrono_api::Params p = chrono_test::params_with(6000, chrono_test::races_of_text("elf,orc"));
    const std::string path = chrono_test::world_dir().file("p508-gods-elf-orc.db");
    REQUIRE(worldgen::generate_world(chrono_test::world_params(1, 512, p, conflict_test::shipped(), std::string("chronology")),
                                     conflict_test::shipped(), path, worldgen::db::Overwrite::refuse)
                .ok());
    const auto a = chrono_test::rows_of(chrono_test::open_world(path), "god");
    const auto b = chrono_test::rows_of(chrono_test::open_world(p503run::world_path(1, 512)), "god");
    CHECK(a == b);
    // the creator names are p3-03-chronology.txt's (spec 17: they do not move); p3_03 tests read that golden file
}

}  // TEST_SUITE

TEST_SUITE("pantheon-slow" * doctest::skip()) {

TEST_CASE("P5-08 13, 16 item 14 (slow): the shipped data at 1024, seeds 0 to 19") {
    P508_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        pantheon_prop::check_world(p503run::world_path(seed, 1024), conflict_test::shipped(),
                                   "shipped seed " + std::to_string(seed) + " at 1024");
    }
}

TEST_CASE("P5-08 13, 16 item 14 (slow): --divine-activity meddling and distant at 1024, seeds 0 to 4") {
    P508_REQUIRE_INTEGRATED();
    for (const char* activity : {"meddling", "distant"}) {
        for (std::uint64_t seed = 0; seed < 5; ++seed) {
            const std::string path = p508api::generated(seed, 1024, conflict_test::shipped(), std::string("act-") + activity,
                                                        1, "", activity);
            pantheon_prop::check_world(path, conflict_test::shipped(),
                                       std::string(activity) + " seed " + std::to_string(seed));
        }
    }
}

}  // TEST_SUITE
