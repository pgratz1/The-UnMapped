// The engineer's checks of P3-06 internals (the independent tests are the
// test writer's, history_conflict_*): the pure rules on the numbers of
// worked example W1 (spec 16.1) and the tuning seam. Hand-made inputs only.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/history/conflict.hpp"

namespace h = worldgen::history;

namespace {

h::ConflictTuning shipped_tuning() {
    h::ConflictTuning t;
    t.camp = h::ConflictCamp{400, -150, -700, 100, -250, 300};
    t.enmity_max = -500;
    t.border_friction = 200;
    t.want_weight = 500;
    t.hunger_full = 300;
    t.fear_share = 400;
    t.fear_weight = 1000;
    t.tick_strain_permille = 25;
    t.quiet_strain_permille = 6;
    t.strain_max = 5000;
    t.claim_strain = 300;
    t.declare_min = 300;
    t.declare_span = 600;
    t.declare_permille = 150;
    t.declare_ratio = 1000;
    t.shelter_years = 150;
    t.march_hops = 2;
    t.cost_penalty = 100;
    t.levy_permille = h::ConflictByKind{100, 100, 100, 80, 250};
    t.terrain_defense = 1000;
    t.pass_defense = 1000;
    t.river_defense = 500;
    t.fort_defense = 500;
    t.battle_permille = 300;
    t.loss_loser_permille = 250;
    t.loss_winner_permille = 80;
    t.garrison_permille = 300;
    t.walls_permille = h::ConflictByKind{1000, 1500, 2500, 4000, 6000};
    t.capital_walls = 2000;
    t.sack_base = 200;
    t.sack_hostility = 1000;
    t.sack_toll_permille = 300;
    t.capture_toll_permille = 100;
    t.exhaustion_permille = 80;
    t.war_max_years = 40;
    t.truce_years = 200;
    t.war_grudge = 150;
    t.capture_grudge = 50;
    t.secession_grudge = 300;
    t.grudge_decay_per_century = 50;
    t.last_refuge = 1;
    t.capital_sacked_salience = 90;
    t.capital_captured_salience = 80;
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

TEST_CASE("P3-06 engineer: W1 dispositions, fear, drive and the strain rate") {
    const h::ConflictTuning t = shipped_tuning();
    const std::int64_t d = h::disposition(t, 0, 2, 0);
    CHECK(d == -700);
    CHECK(h::hostility(d) == 700);
    CHECK(h::share_of(2, 3) == 666);
    CHECK(h::fear_of(t, 666) == 443);
    CHECK(h::drive_of(t, d, 0, 443) == 1343);
    CHECK(h::drive_of(t, d, 0, 0) == 900);
    CHECK(h::strain_rate(600, 1343, 1) == 483);
    CHECK(h::strain_after(t, 0, 483, 1, 25, 1000) == 12075);
    CHECK(h::strain_after(t, 4999000, 483, 1, 25, 1000) == 5000000);  // capped at strain_max
    CHECK(h::declare_permille(t, 603, 480, 400, 600) == 75);
    CHECK(h::declare_permille(t, 299, 480, 400, 600) == 0);
}

TEST_CASE("P3-06 engineer: W1 front, force, battle, siege and casualties") {
    // R1 - R2 - R3, walkable borders of cost 1.
    const h::ConflictGraph g = {{{2, 1}}, {{1, 1}, {3, 1}}, {{2, 1}}};
    const std::vector<std::int64_t> goblins = {1};
    const std::vector<std::int64_t> elves = {2, 3};
    const std::vector<h::FrontEntry> fr = h::front_of(g, goblins, elves, 2);
    REQUIRE(fr.size() == 2U);
    CHECK(fr[0] == h::FrontEntry{2, 1, 1});
    CHECK(fr[1] == h::FrontEntry{3, 2, 2});
    CHECK(h::front_of(g, goblins, elves, 1).size() == 1U);
    const h::ConflictTuning t = shipped_tuning();
    const std::vector<std::pair<std::int64_t, h::SettlementKind>> gob = {{6000, h::SettlementKind::town}};
    const std::vector<std::pair<std::int64_t, h::SettlementKind>> elf = {{2000, h::SettlementKind::town},
                                                                          {500, h::SettlementKind::village}};
    CHECK(h::force_of(t, gob, 800) == 480);
    CHECK(h::force_of(t, elf, 1600) == 400);
    CHECK(h::battle_permille_of(480, 400, h::defence_of(t, h::DefenceTerms{})) == 545);
    CHECK(h::battle_permille_of(0, 0, 1000) == 500);
    const std::vector<std::int64_t> losses = h::casualty_losses(std::vector<std::int64_t>{2000, 500}, 100);
    CHECK(losses == std::vector<std::int64_t>{80, 20});
    CHECK(h::casualty_losses(std::vector<std::int64_t>{6000}, 38) == std::vector<std::int64_t>{38});
    CHECK(h::casualty_losses(std::vector<std::int64_t>{1, 1, 1}, 2) == std::vector<std::int64_t>{1, 1, 0});
    const std::int64_t garrison = h::garrison_of(t, 480, 1600, h::SettlementKind::village, false);
    CHECK(garrison == 345);
    CHECK(h::siege_permille_of(442, garrison) == 561);
    CHECK(h::siege_permille_of(0, 0) == 0);
    CHECK(h::war_score(1, 0, 0, 1) == -1);
}

TEST_CASE("P3-06 engineer: a pass against a two-border walkable path, and ties of (cost, last region)") {
    // 1 - 4 over a pass (cost 3); 1 - 2 - 4 and 1 - 3 - 4 walkable (cost 2 each).
    const h::ConflictGraph g = {{{2, 1}, {3, 1}, {4, 3}}, {{1, 1}, {4, 1}}, {{1, 1}, {4, 1}}, {{1, 3}, {2, 1}, {3, 1}}};
    const std::vector<std::int64_t> x = {1};
    const std::vector<std::int64_t> y = {4};
    const std::vector<h::FrontEntry> fr = h::front_of(g, x, y, 2);
    REQUIRE(fr.size() == 1U);
    CHECK(fr[0] == h::FrontEntry{4, 2, 2});
    const std::vector<h::FrontEntry> one = h::front_of(g, x, y, 1);
    REQUIRE(one.size() == 1U);
    CHECK(one[0] == h::FrontEntry{4, 3, 1});
}

TEST_CASE("P3-06 engineer: the tuning seam's messages (16.4)") {
    CHECK(fatal_of(shipped_tuning()).empty());
    h::ConflictTuning t = shipped_tuning();
    t.march_hops = 5;
    CHECK(fatal_of(t) == "history: tuning: conflict.march_hops outside its schema range");
    t = shipped_tuning();
    t.camp.good_evil = -1001;
    CHECK(fatal_of(t) == "history: tuning: conflict.camp.good_evil outside its schema range");
    t = shipped_tuning();
    t.walls_permille.city = 0;
    CHECK(fatal_of(t) == "history: tuning: conflict.walls_permille.city outside its schema range");
    t = shipped_tuning();
    t.claim_strain = 301;
    CHECK(fatal_of(t) == "history: tuning: claim_strain must not be above declare_min");
    t = shipped_tuning();
    t.strain_max = 899;
    CHECK(fatal_of(t) == "history: tuning: declare_min + declare_span must not be above strain_max");
}
