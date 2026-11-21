// The engineer's checks of the P3-09 rules (spec P3-06 r9 section 20,
// P3-07 r8 21.4); the independent tests are the test writer's. Pure seams
// on hand-made numbers only.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/digest.hpp"

namespace h = worldgen::history;

namespace {

h::ConflictTuning tuning_09() {
    h::ConflictTuning t;
    t.camp = h::ConflictCamp{400, -150, -700, 100, -250, 300};
    t.enmity_max = -500;
    t.hunger_full = 300;
    t.fear_share = 400;
    t.strain_max = 5000;
    t.claim_strain = 300;
    t.declare_min = 300;
    t.declare_span = 600;
    t.declare_permille = 150;
    t.declare_ratio = 1000;
    t.march_hops = 2;
    t.levy_permille = h::ConflictByKind{100, 100, 100, 80, 250};
    t.garrison_permille = 300;
    t.walls_permille = h::ConflictByKind{1000, 1500, 2500, 4000, 6000};
    t.capital_walls = 2000;
    t.exhaustion_permille = 80;
    t.war_max_years = 40;
    t.truce_years = 200;
    t.last_refuge = 1;
    // r9 20.9, shipped.
    t.refuge_flight = 1;
    t.flight_hops = 3;
    t.weary_wars = 3;
    t.tribute_permille = 100;
    t.revenge_aggression = 400;
    t.levy_hops = 3;
    t.odds_floor = 500;
    t.truce_break_aggression = 600;
    t.enmity_hops = 3;
    t.kill_max_permille = 500;  // r10 20.19
    t.find_permille = 250;
    t.search_years = 3;
    t.search_weary_years = 500;  // r14 21.7 (generator 0.19.0)
    t.search_repeat_salience = 15;
    return t;
}

std::string fatal_of(const h::ConflictTuning& t) {
    try {
        h::check_conflict_tuning(t);
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return "";
}

}  // namespace

TEST_CASE("P3-09 engineer: muster scales force after prowess (20.6 item 1)") {
    const h::ConflictTuning t = tuning_09();
    const std::vector<std::pair<std::int64_t, h::SettlementKind>> horde = {{5000, h::SettlementKind::town}};
    CHECK(h::force_of(t, horde, 1300) == 650);
    CHECK(h::force_of(t, horde, 1300, 1000) == 650);
    CHECK(h::force_of(t, horde, 1300, 4000) == 2600);
    // levy 99 x 1300 div 1000 = 128, x 4000 div 1000 = 512 (left to right).
    const std::vector<std::pair<std::int64_t, h::SettlementKind>> small = {{990, h::SettlementKind::village}};
    CHECK(h::force_of(t, small, 1300, 4000) == 512);
}

TEST_CASE("P3-09 engineer: odds with the floor, the minimum and the chance (20.6 item 3)") {
    const h::ConflictTuning t = tuning_09();
    // goblins (600): m = max(400, 500) = 500; need = 1000 x 1000 div 1000 x 500 div 1000 = 500.
    CHECK(h::declare_odds(t, 400, 1000, 600) == 800);
    CHECK(h::declare_odds(t, 500, 1000, 600) == 1000);
    // elves (150): m = 850, need 850.
    CHECK(h::declare_odds(t, 595, 1000, 150) == 700);
    CHECK(h::declare_odds(t, 594, 1000, 150) == 698);
    // orcs (900): the floor 500 rules, not 100.
    CHECK(h::declare_odds(t, 350, 1000, 900) == 700);
    CHECK(h::declare_odds(t, 0, 0, 900) == 0);
    CHECK(h::declare_chance(t, 299, 1000) == 0);
    CHECK(h::declare_chance(t, 900, 1000) == 150);
    CHECK(h::declare_chance(t, 600, 800) == 60);
}

TEST_CASE("P3-09 engineer: the directional truce (20.6 item 4)") {
    const h::ConflictTuning t = tuning_09();
    CHECK(h::truce_length(t, 900) == 50);
    CHECK(h::truce_length(t, 600) == 200);
    CHECK(h::truce_length(t, 150) == 200);
    CHECK(h::truce_length(t, 700) == 150);
    CHECK(h::truce_length(t, 1000) == 0);
}

TEST_CASE("P3-09 engineer: home defence in def and in the garrison (20.7 item 2)") {
    const h::ConflictTuning t = tuning_09();
    h::DefenceTerms d;
    CHECK(h::defence_of(t, d) == 1000);
    d.home_defense = 1500;
    d.biome_term = 400;
    CHECK(h::defence_of(t, d) == 1600);
    d.biome_term = -300;
    CHECK(h::defence_of(t, d) == 1000);  // max(0, B)
    // G = 480 x 300 div 1000 = 144, x 1600 div 1000 = 230, x 1500 div 1000 = 345; x 1800 div 1000 = 621.
    CHECK(h::garrison_of(t, 480, 1600, h::SettlementKind::village, false) == 345);
    CHECK(h::garrison_of(t, 480, 1600, h::SettlementKind::village, false, 2000, 400) == 621);
    CHECK(h::garrison_of(t, 480, 1600, h::SettlementKind::village, false, 2000, -1000) == 345);
    CHECK(h::garrison_of(t, 480, 1600, h::SettlementKind::village, false, 0, 1000) == 345);
}

TEST_CASE("P3-09 engineer: the tuning seam's new ranges and named check (20.9)") {
    CHECK(fatal_of(tuning_09()).empty());
    h::ConflictTuning t = tuning_09();
    t.enmity_hops = 1;
    CHECK(fatal_of(t) == "history: tuning: enmity_hops must not be below march_hops");
    t = tuning_09();
    t.truce_break_aggression = 1000;
    CHECK(fatal_of(t) == "history: tuning: conflict.truce_break_aggression outside its schema range");
    t = tuning_09();
    t.flight_hops = 0;
    CHECK(fatal_of(t) == "history: tuning: conflict.flight_hops outside its schema range");
    t = tuning_09();
    t.levy_hops = 17;
    CHECK(fatal_of(t) == "history: tuning: conflict.levy_hops outside its schema range");
}

TEST_CASE("P3-09 engineer: the ordinals of the war note (P3-07 r8 21.4 item 3)") {
    CHECK(h::history_ordinal(2) == "second");
    CHECK(h::history_ordinal(10) == "tenth");
    CHECK(h::history_ordinal(11) == "11th");
    CHECK(h::history_ordinal(12) == "12th");
    CHECK(h::history_ordinal(13) == "13th");
    CHECK(h::history_ordinal(21) == "21st");
    CHECK(h::history_ordinal(22) == "22nd");
    CHECK(h::history_ordinal(23) == "23rd");
    CHECK(h::history_ordinal(24) == "24th");
    CHECK(h::history_ordinal(101) == "101st");
    CHECK(h::history_ordinal(111) == "111th");
    CHECK(h::history_ordinal(112) == "112th");
}

TEST_CASE("P3-09 r3 engineer: the war score of dead and people taken, and the kill bound (20.15, 20.16)") {
    // s = (dead(b) - dead(a)) + (taken(a) - taken(b)): Dulimin killed 991 for 44.
    CHECK(h::war_score_people(991, 44, 0, 0) == -947);
    CHECK(h::war_score_people(44, 991, 0, 0) == 947);
    CHECK(h::war_score_people(100, 50, 50, 0) == 0);
    CHECK(h::war_score_people(10, 0, 0, 300) == -310);
    h::ConflictTuning t = tuning_09();
    t.loss_loser_permille = 250;
    t.kill_max_permille = 500;
    // F_W = F_L / 20: 2000 defenders bound by 100 victors to 50 dead (not 500).
    CHECK(h::loser_dead_of(t, 100, 2000) == 50);
    CHECK(h::loser_dead_of(t, 2000, 100) == 25);
    CHECK(h::loser_dead_of(t, 0, 100) == 0);
    t.kill_max_permille = 1;
    CHECK(fatal_of(t).empty());
    t.kill_max_permille = 10001;
    CHECK(fatal_of(t) == "history: tuning: conflict.kill_max_permille outside its schema range");
    t = tuning_09();
    t.search_years = 0;
    CHECK(fatal_of(t) == "history: tuning: conflict.search_years outside its schema range");
    t = tuning_09();
    t.find_permille = 1001;
    CHECK(fatal_of(t) == "history: tuning: conflict.find_permille outside its schema range");
}
