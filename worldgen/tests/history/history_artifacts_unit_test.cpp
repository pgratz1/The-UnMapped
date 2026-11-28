// Spec P5-11 (docs/specs/p5-11-artifacts.md r1) test 16 items 2, 3, 7 and 8 by hand: the formulas of p5_11_checks.hpp
// (p511_f, ported from the independent reference) on hand-checked values: the kind form of 7.6 (a/an, of deep iron),
// the making chance and its forge factor (6.3), the class weights with relic and staff zeroed (6.3), the forge of 6.4
// (own mine, partner by depth then id, none), the eligibility of 6.3 (ended, hidden, rest, world cap, Q4 budget), the
// ArtifactView sums with the deep bonus and the caps (7.1), the win chance, strain and hunt factors of 7.2; the spec's
// listing of 3 and 8.1 as the adapter holds it. These cases run before integration.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p5_11_checks.hpp"

TEST_SUITE("history") {

TEST_CASE("P5-11 7.6: the form kind") {
    CHECK(p511_f::kind_form("axe", 0) == "an axe");
    CHECK(p511_f::kind_form("sword", 1) == "a sword of deep iron");
    CHECK(p511_f::kind_form("orb", 0) == "an orb");
    CHECK(p511_f::kind_form("idol", 1) == "an idol of deep iron");
    CHECK(p511_f::kind_form("helm", 0) == "a helm");
    CHECK(p511_f::kind_form("ugly mace", 0) == "an ugly mace");
    CHECK(p511_f::kind_form("ring", 0) == "a ring");
    CHECK(p511_f::kind_form("reliquary", 0) == "a reliquary");
}

TEST_CASE("P5-11 6.3: the making chance and the forge factor") {
    CHECK(p511_f::make_permille(60, 0, 2000) == 60);
    CHECK(p511_f::make_permille(60, 7, 2000) == 120);
    CHECK(p511_f::make_permille(600, 7, 2000) == 1000);
    CHECK(p511_f::make_permille(600, 7, 0) == 0);
    CHECK(p511_f::make_permille(1000, 1, 4000) == 1000);
    CHECK(p511_f::make_permille(7, 3, 1500) == 10);
}

TEST_CASE("P5-11 6.3: class weights, relic and staff zeroed") {
    const std::vector<std::int64_t> w = {1, 2, 3, 4, 5};
    CHECK(p511_f::class_weights(w, false, true) == std::vector<std::int64_t>{1, 2, 3, 0, 5});
    CHECK(p511_f::class_weights(w, true, false) == std::vector<std::int64_t>{1, 2, 3, 4, 0});
    CHECK(p511_f::class_weights(w, false, false) == std::vector<std::int64_t>{1, 2, 3, 0, 0});
    CHECK(p511_f::class_weights({0, 0, 0, 1000, 0}, true, true) == std::vector<std::int64_t>{0, 0, 0, 1000, 0});
    // 4.4's cult_founded occasion without a god participant would have no weight left: no making (6.3 "all W = 0")
    const std::vector<std::int64_t> z = p511_f::class_weights({0, 0, 0, 1000, 0}, false, true);
    std::int64_t s = 0;
    for (const std::int64_t x : z) s += x;
    CHECK(s == 0);
}

TEST_CASE("P5-11 6.4: the forge (own mine, a partner by depth then id, none)") {
    const std::map<std::int64_t, std::int64_t> depth = {{1, 0}, {2, 1}, {3, 3}, {4, 3}, {5, 0}, {6, 2}};
    CHECK(p511_f::forge_of(2, depth, {3, 4}) == 2);
    CHECK(p511_f::forge_of(1, depth, {6, 4, 3}) == 3);
    CHECK(p511_f::forge_of(1, depth, {2, 6}) == 6);
    CHECK(p511_f::forge_of(1, depth, {5}) == 0);
    CHECK(p511_f::forge_of(5, depth, {}) == 0);
    CHECK(p511_f::forge_of(7, depth, {1, 5}) == 0);   // unknown realm: depth 0
}

TEST_CASE("P5-11 6.3: eligibility (no draw when it fails)") {
    constexpr std::int64_t never = -(std::int64_t{1} << 62);
    CHECK(p511_f::eligible(true, true, 100, 50, 50, 3, 40, false, 0));
    CHECK_FALSE(p511_f::eligible(true, true, 100, 51, 50, 3, 40, false, 0));
    CHECK(p511_f::eligible(true, true, 0, never, 10000, 0, 40, false, 0));
    CHECK_FALSE(p511_f::eligible(false, true, 100, never, 50, 0, 40, false, 0));
    CHECK_FALSE(p511_f::eligible(true, false, 100, never, 50, 0, 40, false, 0));
    CHECK_FALSE(p511_f::eligible(true, true, 100, never, 50, 40, 40, false, 0));
    CHECK_FALSE(p511_f::eligible(true, true, 100, never, 50, 0, 0, false, 0));
    CHECK_FALSE(p511_f::eligible(true, true, 100, never, 50, 0, 40, true, 0));
    CHECK(p511_f::eligible(true, true, 100, never, 50, 0, 40, true, 1));
    CHECK(p511_f::eligible(true, true, 100, 100, 0, 0, 40, false, 0));   // rest 0: twice in a year
}

TEST_CASE("P5-11 7.1: the borne sums, the deep bonus and the caps") {
    const p511_f::Borne sword{50, 60, false}, deep_sword{50, 60, true}, axe{40, 60, true}, helm{30, 30, false},
        deep_helm{30, 30, true}, staff{0, 40, true};
    CHECK(p511_f::battle_sum({sword, helm}, 30, 150) == 80);
    CHECK(p511_f::battle_sum({deep_sword, helm}, 30, 150) == 110);
    CHECK(p511_f::battle_sum({deep_sword, axe, deep_helm}, 30, 150) == 150);
    CHECK(p511_f::battle_sum({staff}, 30, 150) == 0);   // no bonus on a field of 0
    CHECK(p511_f::hunt_sum({staff}, 30, 200) == 70);
    CHECK(p511_f::hunt_sum({deep_sword, axe, deep_helm}, 30, 200) == 200);
    CHECK(p511_f::hunt_sum({}, 30, 200) == 0);
    CHECK(p511_f::battle_sum({deep_sword}, 30, 0) == 0);   // force_max_permille 0
}

TEST_CASE("P5-11 7.2: the win chance, the strain factor and the hunt") {
    // no arms: P5-06 7.6's chance
    CHECK(p511_f::win_chance(1000, 1000, 1500, 100, 200, 0, 0) == 1100000 / (1100 + 1500 * 1200 / 1000));
    CHECK(p511_f::win_chance(1000, 1000, 1000, 0, 0, 150, 0) == 1150 * 1000 / 2150);
    CHECK(p511_f::win_chance(2000, 1000, 1200, 100, 50, 80, 30) == 2360 * 1000 / (2360 + 1296));
    CHECK(p511_f::win_chance(0, 0, 1000, 0, 0, 100, 100) == 500);
    CHECK(p511_f::strain_rate(1234, 500) == 1851);
    CHECK(p511_f::strain_rate(1234, 0) == 1234);
    CHECK(p511_f::strain_rate(7, 1000) == 14);
    CHECK(p511_f::hunt_values(20, 200, 5, 3, 1000, 1200, 30, 500, 60, 0) == std::pair<std::int64_t, std::int64_t>{60, 1872});
    CHECK(p511_f::hunt_values(20, 200, 100, 9, 1000, 1000, 30, 0, 0, 50).first == 280);
    CHECK(p511_f::hunt_values(20, 200, 5, 1, 1000, 1000, 0, 0, 0, 50).first == 70);
    CHECK(p511_f::hunt_values(20, 200, 5, 1, 1000, 1000, 0, 0, 0, 0) == std::pair<std::int64_t, std::int64_t>{20, 1000});
}

TEST_CASE("P5-11 3, 8.1, 10 item 6: the spec's listing in the adapter") {
    CHECK(p511api::streams().size() == 7U);
    CHECK(p511api::streams().front() == "hist.artifacts.make");
    CHECK(p511api::streams().back() == "hist.names.artifact");
    CHECK(p511api::new_kinds().size() == 6U);
    for (std::size_t i = 0; i < p511api::new_kinds().size(); ++i) {
        CHECK(p511api::new_kinds()[i].first == 111 + static_cast<std::int64_t>(i));
    }
    CHECK(p511api::stats_keys().size() == 25U);
    CHECK(p511api::stats_keys().back() == "cult_returns_barred");
    CHECK(p511_check::is_quiet_move_key("figure_died"));
    CHECK_FALSE(p511_check::is_quiet_move_key("sacked"));
    CHECK(p511_check::move_states_ok("hero_slain", p511api::BORNE, p511api::HOARDED));
    CHECK_FALSE(p511_check::move_states_ok("hero_slain", p511api::KEPT, p511api::HOARDED));
    CHECK_FALSE(p511_check::move_states_ok("battle", p511api::KEPT, p511api::KEPT));
}

}  // TEST_SUITE
