// The engineer's checks of P3-05 internals (the independent tests are the
// test writer's, history_peoples_*). Hand-made inputs only.

#include <doctest/doctest.h>

#include <cstdint>

#include "worldgen/history/peoples.hpp"

namespace h = worldgen::history;

namespace {

h::PeoplesTuning tuning() {
    h::PeoplesTuning t;
    t.score_site = t.score_biome = t.score_upland = t.score_coast = 1000;
    t.ore_reach_full = 200;
    t.coast_full = 200;
    t.rough_base = 150;
    t.rough_upland = 600;
    t.people_per_kcell = 2500;
    t.capacity_floor = 400;
    t.village_min = 250;
    t.town_min = 1500;
    t.city_min = 8000;
    t.city_max = 25000;
    t.fortress_max = 6000;
    t.pass_cost = 3;
    t.river_cost = 2;
    t.river_border_share_permille = 500;
    return t;
}

h::PeoplesRace race(std::initializer_list<h::SettlementKind> kinds) {
    h::PeoplesRace r;
    r.id = 1;
    r.w_fertility = 1;
    r.density_permille = 1000;
    for (const h::SettlementKind k : kinds) {
        r.builds[static_cast<std::size_t>(k)] = 1;
    }
    return r;
}

}  // namespace

TEST_CASE("P3-05 engineer: growth rule of 9.1") {
    // P < C: logistic step, at least 1 with g > 0, capped.
    CHECK(h::grow_population(300, 1000, 15, 24999, 100) == 300 + (300 * 15 * 700 + 500 * 1000) / 1000000);
    CHECK(h::grow_population(10, 100000, 1, 24999, 100) == 11);
    CHECK(h::grow_population(10, 100000, 0, 24999, 100) == 10);
    CHECK(h::grow_population(1499, 5000, 20, 1499, 100) == 1499);
    CHECK(h::grow_population(1600, 5000, 20, 1499, 100) == 1600);  // above the cap: kept, not cut
    // P > C: lose a tenth of the excess, at least 1.
    CHECK(h::grow_population(1100, 1000, 15, 24999, 100) == 1090);
    CHECK(h::grow_population(1005, 1000, 15, 24999, 100) == 1004);
    CHECK(h::grow_population(1000, 1000, 15, 24999, 100) == 1000);
}

TEST_CASE("P3-05 engineer: ladder kinds and caps of 5.2, 8.1") {
    const h::PeoplesTuning t = tuning();
    using K = h::SettlementKind;
    const h::PeoplesRace faerie = race({K::hamlet, K::village});
    CHECK(h::ladder_kind(faerie, 100, t) == K::hamlet);
    CHECK(h::ladder_kind(faerie, 300, t) == K::village);
    CHECK(h::ladder_kind(faerie, 9000, t) == K::village);
    CHECK(h::kind_cap(faerie, K::village, t) == 1499);
    const h::PeoplesRace townsfolk = race({K::town, K::city});
    CHECK(h::ladder_kind(townsfolk, 10, t) == K::town);  // none at or below: the lowest
    CHECK(h::kind_cap(townsfolk, K::town, t) == 25000);
    const h::PeoplesRace forts = race({K::fortress});
    CHECK(h::ladder_kind(forts, 10, t) == K::fortress);
    CHECK(h::kind_cap(forts, K::fortress, t) == 6000);
}

TEST_CASE("P3-05 engineer: border costs of 4.4") {
    const h::PeoplesTuning t = tuning();
    worldgen::db::RegionBorderRow b;
    b.length_m = 1600;
    CHECK_FALSE(h::border_cost(b, t).has_value());
    b.pass_id = 2;
    CHECK(*h::border_cost(b, t) == 3);
    b.walk_pairs = 1;
    CHECK(*h::border_cost(b, t) == 1);
    b.river_pairs = 50;  // 50 x 16 x 1000 = 800000 = 500 x 1600: exactly the share
    CHECK(*h::border_cost(b, t) == 3);
    b.river_pairs = 49;
    CHECK(*h::border_cost(b, t) == 1);
    b.river_pairs = 50;
    b.length_m = 0;
    CHECK(*h::border_cost(b, t) == 1);
}

TEST_CASE("P3-05 engineer: score terms use floor division, capacity has its floor") {
    const h::PeoplesTuning t = tuning();
    h::PeoplesRegion g;
    g.id = 1;
    g.dry = 2000;
    g.walk_cells = 2000;
    g.shares = {{3, 1}};  // one per mille of biome 3
    h::PeoplesRace r = race({h::SettlementKind::hamlet});
    r.biomes = {worldgen::db::RaceBiomeRow{1, 3, -1}};
    // S = 0, B = floor(-1 / 1000) = -1, sigma = floor(-1000 / 1000) = -1.
    CHECK(h::region_score(g, r, t) == -1);
    CHECK(h::region_fit(-1) == 0);
    CHECK(h::region_capacity(g, r, t) == 400);
    g.fertility_mean = 255;
    r.biomes.clear();
    CHECK(h::region_score(g, r, t) == 1000);
    CHECK(h::region_capacity(g, r, t) == std::int64_t{2000} * 1000 * 1000 * 2500 / 1000000000);
}

// Timing of the history stage (P3-05 16.4) on stored worlds run up to
// chronology: UNMAPPED_PEOPLES_BENCH holds their paths separated by ':'.
// Skipped by default: worldgen-tests -ts=peoples-bench --no-skip.
#include <chrono>
#include <cstdlib>
#include <string>

#include "p3_04_engine_support.hpp"
#include "worldgen/stages/history.hpp"

TEST_SUITE("peoples-bench" * doctest::skip()) {
TEST_CASE("P3-05 engineer: history stage time") {
    const char* list = std::getenv("UNMAPPED_PEOPLES_BENCH");
    REQUIRE(list != nullptr);
    std::string paths = list;
    std::size_t at = 0;
    while (at <= paths.size()) {
        const std::size_t end = std::min(paths.find(':', at), paths.size());
        const std::string path = paths.substr(at, end - at);
        at = end + 1;
        if (path.empty()) {
            continue;
        }
        const worldgen::db::WorldReader world = engine_test::open_world(path);
        const h::HistoryInputs in = h::history_inputs_from_world(world, engine_test::shipped());
        const auto t0 = std::chrono::steady_clock::now();
        const worldgen::stages::HistoryResult r = worldgen::stages::history_stage(engine_test::shipped(), in);
        const auto t1 = std::chrono::steady_clock::now();
        MESSAGE(path << ": history " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                     << " ms, " << r.events.size() << " events");
    }
}
}
