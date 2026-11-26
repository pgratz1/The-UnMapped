// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r1) test 16 items 2 to 7 on hand-checked values: the formulas of
// 6.2 (the worked years at deep_years[d] - 1 and at it, the chance with mining_permille 500, Q4's p x L and its cap),
// 6.3 (the forge factor at depth 0 to 3, forge_depth_permille 0 gives P5-04's), 6.4 (the wake chance, the weights),
// 6.5 (the odds at S = 0, S = might and the cap, sealed at u = odds - 1 and fallen at u = odds; the spec's samples),
// 6.6 (the toll of the spec's two sample sentences), 6.7 (the take chance, the conditions of a later take, the hunt
// sample), the tellings of 8.1 and mine_depth_word of 11.1. The formulas are p5_07_checks.hpp's p507_f, ported from
// the reference (p5_07_depths_reference.py units gives the same values). Run before integration; one case compares
// the library's mine_depth_word and waits for it.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p5_07_api.hpp"
#include "p5_07_checks.hpp"
#include "worldgen/history/draw.hpp"

namespace {

using namespace p507_f;
const std::vector<std::int64_t> YEARS = {150, 500, 1000};   // 4.2 starts
const std::vector<std::int64_t> PPM = {0, 150, 600};

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-07 6.2: the worked years at deep_years[d] - 1 and at it; depth 3 is the last") {
    CHECK_FALSE(can_delve(1149, 1000, 0, YEARS));
    CHECK(can_delve(1150, 1000, 0, YEARS));
    CHECK_FALSE(can_delve(1499, 1000, 1, YEARS));
    CHECK(can_delve(1500, 1000, 1, YEARS));
    CHECK_FALSE(can_delve(1999, 1000, 2, YEARS));
    CHECK(can_delve(2000, 1000, 2, YEARS));
    CHECK_FALSE(can_delve(9000, 1000, 3, YEARS));
}

TEST_CASE("P5-07 6.2: the chance: mining 1000 and 500, Q4's p x L and its cap at 1000") {
    CHECK(delve_permille(10, 1000) == 10);
    CHECK(delve_permille(10, 500) == 5);
    CHECK(delve_permille(10, 0) == 0);
    CHECK(delve_permille(0, 1000) == 0);       // deep_delve_permille 0: no mine deepens (I0)
    CHECK(delve_permille(10, 1000, 50) == 500);
    CHECK(delve_permille(10, 500, 30) == 150);
    CHECK(delve_permille(10, 1000, 200) == 1000);
    CHECK(delve_permille(1000, 1000, 5) == 1000);   // depths-small
}

TEST_CASE("P5-07 6.3: the forge factor at depth 0 to 3 (+5% to +9.5%); not forged; forge_depth_permille 0") {
    CHECK(forge_factor(10000, 50, 15, 0, true) == 10500);
    CHECK(forge_factor(10000, 50, 15, 1, true) == 10650);
    CHECK(forge_factor(10000, 50, 15, 2, true) == 10800);
    CHECK(forge_factor(10000, 50, 15, 3, true) == 10950);   // 6.3: "+9.5% instead of +5%"
    CHECK(forge_factor(10000, 50, 15, 3, false) == 10000);
    CHECK(forge_factor(10000, 50, 0, 3, true) == 10500);    // P5-04's force
    CHECK(forge_factor(1999, 50, 15, 3, true) == 2188);     // floor: 1999 x 1095 div 1000
}

TEST_CASE("P5-07 6.4: the wake chance by depth and pressure; the kind's weights") {
    CHECK(wake_ppm(PPM, 1, 1000) == 0);
    CHECK(wake_ppm(PPM, 2, 1000) == 150);
    CHECK(wake_ppm(PPM, 3, 1000) == 600);
    CHECK(wake_ppm(PPM, 3, 500) == 300);       // pressure low
    CHECK(wake_ppm(PPM, 3, 1600) == 960);      // pressure high
    CHECK(wake_ppm({0, 200000, 600000}, 3, 1600) == 960000);
    CHECK(wake_ppm({0, 0, 1000000}, 3, 1600) == 1000000);
    // P3-02 4.1's weighted: a weight of 0 is never chosen; 500/350/150 at the boundaries of the high word
    namespace dr = worldgen::history;
    CHECK(dr::weighted(0, std::vector<std::int64_t>{0, 350, 150}) == 1U);
    CHECK(dr::weighted(0x7FFFFFFFFFFFFFFFULL, std::vector<std::int64_t>{500, 350, 150}) == 0U);
    CHECK(dr::weighted(0x8000000000000000ULL, std::vector<std::int64_t>{500, 350, 150}) == 1U);
    CHECK(dr::weighted(0xDFFFFFFF00000000ULL, std::vector<std::int64_t>{500, 350, 150}) == 2U);
}

TEST_CASE("P5-07 6.5: the gate: S, the odds at S = 0, S = might and the cap; u = odds - 1 seals, u = odds falls") {
    const std::int64_t town = gate_strength(3000, 1000, 1400);   // dwarves' prowess_permille 1400
    const std::int64_t village = gate_strength(800, 1000, 1400);
    CHECK(town == 4200);
    CHECK(gate_odds(town, 4000, 800) == 512);     // 6.5: "a dwarven town of 3,000 seals an Ymrach at 512"
    CHECK(gate_odds(town, 7000, 800) == 375);     // "and a Zhakul at 375"
    CHECK(gate_odds(village, 4000, 800) == 218);  // the spec prints 219 (reading D-R2: the formula floors)
    CHECK(gate_odds(village, 7000, 800) == 137);  // the spec prints 138
    CHECK(gate_odds(0, 4000, 800) == 0);
    CHECK(gate_odds(4000, 4000, 800) == 500);
    CHECK(gate_odds(1000000000, 2500, 800) == 800);
    CHECK(gate_strength(100000, 4000, 1400) == 560000);
    CHECK(sealed_at(511, 512));
    CHECK_FALSE(sealed_at(512, 512));
    CHECK_FALSE(sealed_at(0, 0));   // S = 0 never seals
}

TEST_CASE("P5-07 6.6: the toll of a take (the sample sentences of 8.2)") {
    CHECK(take_toll(3016, 400) == std::make_pair<std::int64_t, std::int64_t>(1206, 1810));
    CHECK(take_toll(535, 400) == std::make_pair<std::int64_t, std::int64_t>(214, 321));
    CHECK(take_toll(900, 0) == std::make_pair<std::int64_t, std::int64_t>(0, 900));
    CHECK(take_toll(1, 400) == std::make_pair<std::int64_t, std::int64_t>(0, 1));
}

TEST_CASE("P5-07 6.7: the take chance; a later take only of the waking realm, in reach, at most deep_takes_max") {
    CHECK(take_permille(300, 1000, 3000, 1500) == 300);    // f_p capped at 1000
    CHECK(take_permille(300, 1000, 3000, 6000) == 150);    // a city: f_p 500
    CHECK(take_permille(400, 1600, 3000, 12000) == 160);
    CHECK(take_permille(800, 1600, 3000, 3000) == 1000);   // base capped
    CHECK(take_permille(1000, 1000, 3000, 0) == 1000);     // max(1, P)
    // (takes, takes_max, hops, reach, owner, waking realm, realm alive, sealed)
    CHECK(may_take_later(1, 2, 1, 1, 4, 4, true, false));
    CHECK(may_take_later(2, 2, 0, 1, 4, 4, true, false));
    CHECK_FALSE(may_take_later(3, 2, 0, 1, 4, 4, true, false));   // 1 + deep_takes_max holds at most
    CHECK_FALSE(may_take_later(1, 2, 2, 1, 4, 4, true, false));   // two borders away
    CHECK_FALSE(may_take_later(1, 2, 0, 1, 5, 4, true, false));   // another realm's settlement
    CHECK_FALSE(may_take_later(1, 2, 0, 1, 4, 4, false, false));  // the waking realm ended
    CHECK_FALSE(may_take_later(1, 2, 0, 1, 4, 4, true, true));    // sealed
    CHECK_FALSE(may_take_later(1, 2, -1, 1, 4, 4, true, false));  // not reachable
    // the hunt sample of 6.7: S 840 against an Ymrach (17%) and a Zhakul (11%), the slay cap above
    CHECK(gate_odds(840, 4000, 1000) == 173);
    CHECK(gate_odds(840, 7000, 1000) == 107);
}

TEST_CASE("P5-07 8.1: the tellings are functions of ids; 11.1 the depth words") {
    CHECK(telling_delve(7, 1) == 2);
    CHECK(telling_delve(7, 2) == 0);
    CHECK(telling_delve(7, 3) == 1);
    CHECK(telling_woken(14) == 2);
    CHECK(telling_sealed(14) == 0);
    CHECK(telling_lost(14, 40) == 0);
    CHECK(depth_word(0).empty());
    CHECK(depth_word(1) == "below daylight");
    CHECK(depth_word(2) == "to the mountain's roots");
    CHECK(depth_word(3) == "to the old deep");
    CHECK(depth_word(4).empty());
}

TEST_CASE("P5-07 11.1: inspect::mine_depth_word gives the spec's words") {
    P507_REQUIRE_INTEGRATED();
    for (std::int64_t d = -1; d <= 4; ++d) {
        CAPTURE(d);
        CHECK(p507api::mine_depth_word(d) == depth_word(d));
    }
}

}  // TEST_SUITE
