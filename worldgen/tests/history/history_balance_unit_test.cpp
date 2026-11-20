// Spec P3-09 (generator 0.18.0): units that need no world.
//
//   - P3-07 r8 21.5's six [stats] keys on a hand-made fixture with hand-counted values (the test-support
//     derivation from rows that the golden tests compare with the reference's [stats] lines; P3-07 is not
//     implemented, so these keys are checked from rows until it is).
//   - P3-07 r8 21.4 history_note on the golden worlds, and ordinal() (guarded: needs
//     worldgen/history/digest.hpp, P3-07 5.1).
//   - The seams P3-06 r9 and P3-05 r8 21 name (guarded by UNMAPPED_P3_09_API, p3_09_switch.hpp): the r9
//     tuning fields and the conflict_enmity_hops message through check_conflict_tuning, the race fields of
//     ConflictRace, and the PeoplesView readers' signatures.
//
// The rules themselves (flight, goals, costs, weariness, revenge, muster, levies, odds, truce, concealment,
// home defence, enmity hops) are checked through the shipped stage against the reference's golden worlds,
// synthetic and stress cases (history_conflict_property_test.cpp, history_balance_golden_test.cpp) and by
// the reference's own hand-made units (p3_conflict_reference.py check_units_r9, numbers in its source).
//
// Written from the specs only.

#include "../p3_09_switch.hpp"
#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#if UNMAPPED_P3_09_DIGEST
#include "worldgen/history/digest.hpp"
#endif
#if UNMAPPED_P3_09_API
#include "worldgen/history/conflict.hpp"
#include "worldgen/history/peoples.hpp"
#endif

namespace {

using namespace conflict_test;
using eng::I;
using eng::N;
using eng::T;

Row ev(std::int64_t id, std::int64_t year, std::int64_t kind, std::int64_t region) {
    return {I(id), I(year), I(year), I(kind), I(1), I(region), I(0), I(50)};
}
Row part(std::int64_t e, std::int64_t seq, std::int64_t civ) { return {I(e), I(seq), I(seq), I(3), I(civ)}; }
Row cause(std::int64_t e, std::int64_t c) { return {I(e), I(c)}; }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-07 r8 21.5 (P3-09): the six [stats] keys on a hand-made log with hand-counted values") {
    // civilizations c1, c3 goblins (race 2), c2 elves (race 1). Four wars on c2:
    //   w1 e10 (c1, R5) 100-110, ended by the truce e11: hollow;
    //   w2 e20 (c3, R5) 200-210, the peace e21 with the tribute e22: not hollow;
    //   w3 e30 (c1, R5) 300-320, battle e31, siege e32, sack e33, peace e34: not hollow, R5 changes in 305;
    //   w4 e40 (c1, R6) 400-open: hollow (E = 1000).
    // (c2, R5): three wars, runs 1, 2, then 0 (w3 moved R5): same_goal_max 3, unchanged_max 2.
    Tables t;
    t.civilization = {{I(1), T("A"), I(2)}, {I(2), T("B"), I(1)}, {I(3), T("C"), I(2)}};
    auto pad = [](Row r) {
        while (r.size() < 10) r.push_back(N());
        return r;
    };
    for (Row& c : t.civilization) c = pad(c);
    t.event = {ev(10, 100, k_war_declared, 5), ev(11, 110, k_truce, 0),  ev(20, 200, k_war_declared, 5),
               ev(21, 210, k_peace, 0),        ev(22, 210, k_tribute, 0), ev(30, 300, k_war_declared, 5),
               ev(31, 305, k_battle, 5),       ev(32, 305, k_siege, 5),   ev(33, 305, k_sacked, 5),
               ev(34, 320, k_peace, 0),        ev(40, 400, k_war_declared, 6)};
    for (const auto& [e, a] : std::vector<std::pair<std::int64_t, std::int64_t>>{{10, 1}, {20, 3}, {30, 1}, {40, 1}}) {
        t.event_participant.push_back(part(e, 1, a));
        t.event_participant.push_back(part(e, 2, 2));
    }
    t.event_cause = {cause(11, 10), cause(21, 20), cause(22, 21), cause(31, 30), cause(32, 31), cause(33, 32),
                     cause(34, 33)};
    t.relation = {{I(3), I(1), I(3), I(2), T("war"), I(100), I(110), I(10), I(11)},
                  {I(3), I(3), I(3), I(2), T("war"), I(200), I(210), I(20), I(21)},
                  {I(3), I(1), I(3), I(2), T("war"), I(300), I(320), I(30), I(34)},
                  {I(3), I(1), I(3), I(2), T("war"), I(400), N(), I(40), N()}};
    t.region_control = {{I(5), I(0), I(305), I(2), I(1)}, {I(5), I(305), N(), I(1), I(33)}, {I(7), I(0), N(), I(2), I(1)}};
    // standing settlements: c1 600, c2 300, c3 100 (race 2: 700 of 1000); a ruin of c2 does not count
    t.settlement = {pad({I(1)}), pad({I(2)}), pad({I(3)}), pad({I(4)})};
    const std::vector<std::pair<std::int64_t, std::int64_t>> sp = {{1, 600}, {2, 300}, {3, 100}, {2, 5000}};
    for (std::size_t i = 0; i < t.settlement.size(); ++i) {
        Row& s = t.settlement[i];
        while (s.size() < 14) s.push_back(N());
        s[8] = I(sp[i].first);
        s[10] = I(i == 3 ? 3 : 1);
        s[11] = I(sp[i].second);
    }
    eng::HistoryInputs in;
    in.params.span = 1000;
    db::RaceRow elf, gob;
    elf.id = 1;
    elf.key = "elf";
    gob.id = 2;
    gob.key = "goblin";
    in.races = {elf, gob};
    CHECK(stats_lines_21(t, in) == std::vector<std::string>{
                                       "wars_hollow = 2", "wars_hollow_permille = 500", "wars_same_goal_max = 3",
                                       "wars_same_goal_unchanged_max = 2", "race_population_share_max_permille = 700",
                                       // held: R5 by c1 (goblin), R7 by c2 (elf)
                                       "race_regions_share_max_permille = 500"});
    // no war: 0 for every war key; no people, no regions: 0 for the shares
    Tables empty;
    CHECK(stats_lines_21(empty, in) == std::vector<std::string>{
                                           "wars_hollow = 0", "wars_hollow_permille = 0", "wars_same_goal_max = 0",
                                           "wars_same_goal_unchanged_max = 0", "race_population_share_max_permille = 0",
                                           "race_regions_share_max_permille = 0"});
}

TEST_CASE("P3-06 r9 20.13 item 5 from rows: weariness violations found on a hand-made log, none after a change") {
    // three truces of c2's R5 (c1, c3, c1) with no change of owner, then a fourth declaration over R5
    Tables t;
    t.event = {ev(10, 100, k_war_declared, 5), ev(11, 110, k_truce, 0), ev(20, 200, k_war_declared, 5),
               ev(21, 210, k_truce, 0),        ev(30, 300, k_war_declared, 5), ev(31, 310, k_truce, 0),
               ev(40, 400, k_war_declared, 5)};
    for (const auto& [e, a] : std::vector<std::pair<std::int64_t, std::int64_t>>{{10, 1}, {20, 3}, {30, 1}, {40, 3}}) {
        t.event_participant.push_back(part(e, 1, a));
        t.event_participant.push_back(part(e, 2, 2));
    }
    t.relation = {{I(3), I(1), I(3), I(2), T("war"), I(100), I(110), I(10), I(11)},
                  {I(3), I(3), I(3), I(2), T("war"), I(200), I(210), I(20), I(21)},
                  {I(3), I(1), I(3), I(2), T("war"), I(300), I(310), I(30), I(31)},
                  {I(3), I(3), I(3), I(2), T("war"), I(400), N(), I(40), N()}};
    t.region_control = {{I(5), I(0), N(), I(2), I(1)}};
    CHECK(weary_violations(t, 3) == std::vector<std::int64_t>{40});
    CHECK(weary_violations(t, 4).empty());
    CHECK(weary_violations(t, 0).empty());  // weary_wars 0: never weary
    // the region changed hands between the third truce and the fourth declaration: no violation
    t.region_control = {{I(5), I(0), I(350), I(2), I(1)}, {I(5), I(350), N(), I(2), I(1)}};
    CHECK(weary_violations(t, 3).empty());
}

#if UNMAPPED_P3_09_DIGEST

TEST_CASE("P3-07 r8 21.4 (P3-09): history_note equals the golden notes of the world cases; no note elsewhere") {
    for (const char* name : {"geo-seed-42-512", "hist-conflict-flight"}) {
        const CCase& g = conflict_case(name);
        CAPTURE(g.name);
        const std::string path =
            chrono_test::world(chrono_test::WorldKey{g.seed, "", g.size, g.span, g.races, "", 1});
        const db::WorldReader r = open_world(path);
        const auto view = eng::HistoryView::load(r);
        REQUIRE(view.ok());
        const Tables t = tables_of_world(r);
        for (const Row& e : t.event) {
            const auto it = g.notes.find(at(e, 0));
            CHECK(worldgen::history::history_note(view.value(), at(e, 0)) == (it == g.notes.end() ? "" : it->second));
        }
    }
}

#endif  // UNMAPPED_P3_09_DIGEST

#if UNMAPPED_P3_09_API

TEST_CASE("P3-06 r9 20.9 (seam): the r9 tuning fields and the conflict_enmity_hops message of check_conflict_tuning") {
    namespace h = worldgen::history;
    h::ConflictTuning t = h::conflict_tuning_from(shipped());
    CHECK(t.refuge_flight == 1);
    CHECK(t.flight_hops == 3);
    CHECK(t.hidden_years == 300);
    CHECK(t.weary_wars == 3);
    CHECK(t.tribute_permille == 100);
    CHECK(t.revenge_aggression == 400);
    CHECK(t.levy_hops == 3);
    CHECK(t.odds_floor == 500);
    CHECK(t.odds_min == 700);
    CHECK(t.truce_break_aggression == 600);
    CHECK(t.enmity_hops == 3);
    h::check_conflict_tuning(t);
    t.enmity_hops = 1;
    CHECK(fatal_of([&] { h::check_conflict_tuning(t); }) == "history: tuning: enmity_hops must not be below march_hops");
    t.enmity_hops = 3;
    t.flight_hops = 9;
    CHECK(fatal_of([&] { h::check_conflict_tuning(t); }) == "history: tuning: conflict.flight_hops outside its schema range");
    t.flight_hops = 3;
    t.truce_break_aggression = 1000;  // int(0, 999): the truce formula divides by 1000 - this
    CHECK(fatal_of([&] { h::check_conflict_tuning(t); }) ==
          "history: tuning: conflict.truce_break_aggression outside its schema range");
}

TEST_CASE("P3-06 r9 20.12 (seam): ConflictRace carries muster, conceal_min and home defence") {
    worldgen::history::ConflictRace r;
    r.muster = 4000;
    r.conceal_min = 400;
    r.home_defense = 2000;
    CHECK(r.muster + r.conceal_min + r.home_defense == 6400);
}

TEST_CASE("P3-05 r8 21 item 1 (seam): the PeoplesView readers and settlement_name have the stated signatures") {
    namespace h = worldgen::history;
    static_assert(std::is_same_v<decltype(&h::PeoplesView::site),
                                 std::optional<h::SiteCell> (h::PeoplesView::*)(const h::StepContext&, std::int64_t,
                                                                               std::int64_t) const>);
    static_assert(std::is_same_v<decltype(&h::PeoplesView::founding_kind),
                                 h::SettlementKind (h::PeoplesView::*)(std::int64_t, std::int64_t, const h::SiteCell&,
                                                                      std::int64_t) const>);
    static_assert(std::is_same_v<decltype(&h::PeoplesView::hops),
                                 const std::vector<std::int64_t>& (h::PeoplesView::*)(std::int64_t) const>);
    static_assert(std::is_same_v<decltype(&h::PeoplesSubsystem::settlement_name),
                                 std::string (h::PeoplesSubsystem::*)(const h::StepContext&, std::int64_t, std::int64_t)>);
    CHECK(true);
}

#endif  // UNMAPPED_P3_09_API

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
