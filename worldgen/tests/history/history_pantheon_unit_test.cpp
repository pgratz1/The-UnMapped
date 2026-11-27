// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1) test 16 items 3 to 6 and 12 on hand-checked values: kin and
// foe (4.3), the add of 6.2 (clamp, memory, contributions_max), the cumulative fade over steps of different lengths,
// 6.3's chance, causes and telling, 6.4's pestilence and the effects in force for exactly `years` with their sums and
// clamps, 7.1's growth and force terms, 6.7 to 6.9's chances by fear, affinity, temper and the Q4 budget, 12.2's stir
// chance and 12.3's tellings mod 9. The values are the reference's units (p5_08_pantheon_reference.py units), each
// derived from the spec's text. These cases need no engine and run before integration; the library's names (11.1)
// are checked in the report test.

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "p5_08_checks.hpp"

using p508_f::Contribution;
using p508_f::Effect;

TEST_SUITE("history") {

TEST_CASE("P5-08 4.3: kin and foe of creators and pantheon gods") {
    CHECK(p508_f::kin(3, 2, 3, 2));
    CHECK_FALSE(p508_f::kin(3, 2, 4, 2));    // a creator is kin to its race only
    CHECK(p508_f::kin(0, 0, 9, 0));
    CHECK_FALSE(p508_f::kin(0, 0, 9, 1));    // a pantheon god to its camp
    CHECK(p508_f::foe(0, 2));
    CHECK(p508_f::foe(2, 0));
    CHECK_FALSE(p508_f::foe(1, 2));          // neutral has no foe
    CHECK_FALSE(p508_f::foe(1, 0));
    CHECK_FALSE(p508_f::foe(1, 1));
}

TEST_CASE("P5-08 6.2: add clamps, forgets by memory and keeps contributions_max") {
    CHECK(p508_f::add(390, {}, 30, 5, 1000, 400, 300, 8).first == 400);
    CHECK(p508_f::add(-390, {}, -30, 5, 1000, 400, 300, 8).first == -400);
    std::vector<Contribution> eight;
    for (std::int64_t i = 1; i <= 8; ++i) eight.push_back({i, 1000, 10});
    const auto full = p508_f::add(80, eight, 10, 9, 1000, 400, 300, 8).second;
    REQUIRE(full.size() == 8U);
    CHECK(full.front().event == 2);
    CHECK(full.back().event == 9);
    const auto mem = p508_f::add(0, {{1, 699, 10}, {2, 700, 10}}, 10, 3, 1000, 400, 300, 8).second;
    REQUIRE(mem.size() == 2U);   // year < 1000 - 300 dropped; 700 kept
    CHECK(mem[0].event == 2);
}

TEST_CASE("P5-08 6.2: the fade is cumulative, so steps of different lengths give the same R") {
    CHECK(p508_f::fade_F(20, 4) == 0);
    CHECK(p508_f::fade_F(20, 5) == 1);
    CHECK(p508_f::fade_F(20, 100) == 20);
    std::int64_t ones = 0;
    for (std::int64_t y = 1000; y < 1100; ++y) ones += p508_f::fade_amount(20, y, 1);
    CHECK(ones == 20);
    CHECK(p508_f::fade_amount(20, 1000, 37) + p508_f::fade_amount(20, 1037, 63) == 20);
    std::int64_t R1 = 130, R2 = 130;
    for (std::int64_t y = 1000; y < 1250; ++y) R1 = p508_f::faded(R1, p508_f::fade_amount(20, y, 1));
    for (const auto& [y, L] : std::vector<std::pair<std::int64_t, std::int64_t>>{{1000, 40}, {1040, 1}, {1041, 209}}) {
        R2 = p508_f::faded(R2, p508_f::fade_amount(20, y, L));
    }
    CHECK(R1 == R2);
    CHECK(R1 == 80);
    CHECK(p508_f::faded(5, 7) == 0);     // never past 0
    CHECK(p508_f::faded(-30, 7) == -23);
}

TEST_CASE("P5-08 6.3: the act's chance by activity, its causes and its telling") {
    CHECK(p508_f::act_permille(150, 300) == 45);
    CHECK(p508_f::act_permille(150, 1000) == 150);
    CHECK(p508_f::act_permille(150, 2500) == 375);
    CHECK(p508_f::act_permille(150, 10000) == 1000);
    CHECK(p508_f::act_permille(0, 2500) == 0);   // act_permille 0: no god ever acts
    const auto c = p508_f::causes({{3, 1, 25}, {7, 2, 40}, {9, 3, 25}, {11, 4, -60}, {12, 5, 10}}, 1);
    CHECK(c.first == 7);
    CHECK(c.second == std::vector<std::int64_t>{3, 7, 9});
    const auto twice = p508_f::causes({{4, 1, 10}, {4, 1, 10}, {6, 2, 15}}, 1);
    CHECK(twice.second == std::vector<std::int64_t>{4, 6});
    CHECK(p508_f::causes({{1, 1, 10}}, -1).first == 0);
    CHECK(p508_f::act_telling(4, 0) == 1);
    CHECK(p508_f::act_telling(4, 2) == 0);
    CHECK(p508_f::cult_telling(5) == 2);
}

TEST_CASE("P5-08 6.4, 7.1: pestilence, effects in force for exactly their years, sums and clamps") {
    CHECK(p508_f::pestilence_dead(3016, 120) == 361);
    CHECK(p508_f::pestilence_dead(8, 120) == 0);
    const std::vector<Effect> plenty{{"plenty", 1000, 50}};
    CHECK(p508_f::growth(plenty, 999, 300, 500) == 0);
    CHECK(p508_f::growth(plenty, 1000, 300, 500) == 300);
    CHECK(p508_f::growth(plenty, 1049, 300, 500) == 300);
    CHECK(p508_f::growth(plenty, 1050, 300, 500) == 0);
    CHECK(p508_f::growth({{"plenty", 1000, 50}, {"withering", 1020, 50}}, 1030, 300, 500) == -200);
    CHECK(p508_f::growth({{"withering", 0, 9}, {"withering", 0, 9}}, 1, 300, 500) == -900);
    CHECK(p508_f::growth({{"plenty", 0, 9}, {"plenty", 0, 9}, {"plenty", 0, 9}}, 1, 300, 500, 200) == 1000);
    CHECK(p508_f::growth({}, 5, 300, 500, 200) == 200);   // a sacred shrine
    const std::vector<Effect> f{{"valour", 10, 50}, {"defeat", 20, 50}};
    CHECK(p508_f::force(f, 9, 50, 60) == 0);
    CHECK(p508_f::force(f, 10, 50, 60) == 50);
    CHECK(p508_f::force(f, 30, 50, 60) == -10);
    CHECK(p508_f::force(f, 65, 50, 60) == -60);
    CHECK(p508_f::force(f, 70, 50, 60) == 0);
    std::vector<Effect> many;
    for (int i = 0; i < 10; ++i) many.push_back({"defeat", 0, 9});
    CHECK(p508_f::force(many, 1, 50, 60) == -500);
    CHECK(p508_f::growth_term(10, 300) == 13);
    CHECK(p508_f::growth_term(1, -500) == 0);   // reading P-R2: after the floor of 1
    CHECK(p508_f::growth_term(-5, 300) == -5);  // a decline is unchanged
    CHECK(p508_f::force_term(10000, 50) == 10500);
    CHECK(p508_f::force_term(10000, -60) == 9400);
}

TEST_CASE("P5-08 6.7 to 6.9: founding, spread, faith and suppression chances") {
    CHECK(p508_f::found_permille(250, 500, true) == 250);
    CHECK(p508_f::found_permille(250, 500, false) == 125);   // a cult of appeasement
    CHECK(p508_f::spread_permille(20, 1000) == 20);
    CHECK(p508_f::spread_permille(20, 500) == 10);
    CHECK(p508_f::spread_permille(20, 100) == 2);
    CHECK(p508_f::spread_permille(20, 1000, 30) == 600);    // Q4: p x L
    CHECK(p508_f::spread_permille(20, 1000, 60) == 1000);   // and its cap
    CHECK(p508_f::temper_permille(15, 500) == 7);
    CHECK(p508_f::temper_permille(15, 1500) == 22);
    CHECK(p508_f::temper_permille(15, 1500, 40) == 880);
    CHECK(p508_f::temper_permille(10, 1300) == 13);
}

TEST_CASE("P5-08 12.2, 12.3: the stir's chance and the tellings mod 9") {
    CHECK(p508_f::stir_ppm({0, 150, 600}, 2, 1000, 30, 1000) == 4500);
    CHECK(p508_f::stir_ppm({0, 150, 600}, 3, 1000, 50, 1000, 500) == 15000);
    CHECK(p508_f::stir_ppm({0, 150, 600000}, 3, 1600, 25, 1000) == 1000000);
    CHECK(p508_f::stir_ppm({0, 150, 600}, 3, 1000, 50, 0) == 0);   // deep_stir_permille 0 (the code default)
    CHECK(p508_f::telling_delve(7, 1) == 2);
    CHECK(p508_f::telling_delve(7, 2) == 6);
    CHECK(p508_f::telling_delve(7, 3) == 1);
    CHECK(p508_f::telling_warren(5, 3) == 8);
}

}  // TEST_SUITE
