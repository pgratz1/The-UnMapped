// Spec P3-06 r14 section 21 (P3-10, generator 0.19.0) with P3-07 r12 22.5a/22.5b: units that need no world.
//
//   - P3-06 21.8 / P3-07 22.5b: wars_same_goal_unchanged_max counts by holder: a region emptied and resettled by
//     its holder is one run; a region taken by another civilization and retaken breaks it (hand-made rows,
//     hand-counted values). holder_change_years on the same rows.
//   - The row checks of invariants (14), (15) and 21.7 (invariants_19, p3_06_conflict_support.hpp) fire on
//     hand-made logs that break them, and stay silent on the same logs repaired.
//   - The seams the specs name (the engineer's names; P3-10 integration): history.conflict's
//     search_weary_years, search_repeat_salience and quarrel_strain (r15, with conflict_quarrel_strain) in
//     ConflictTuning without odds_min; the race fields
//     odds_min_permille and awakening_quarrel in ConflictRace; PeoplesSubsystem::lie_low(civ, until_year).
//
// The rules themselves (aim by holding the goal, the lost goal's score ending, searches, the holder, the odds per
// race, lying low, the awakening quarrel) are checked through the shipped stage against the reference's golden
// worlds, synthetic and stress cases (history_conflict_property_test.cpp, history_conflict_golden_test.cpp,
// history_balance_golden_test.cpp: invariants_19 and check_invariants_10's amended (8)) and by the reference's
// own hand-made units (p3_conflict_reference.py check_units_r14, the cases of 21.5, 21.11 and 21.12 item 6).
//
// Written from the specs only.

#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "worldgen/history/conflict.hpp"
#include "worldgen/history/peoples.hpp"

namespace {

using namespace conflict_test;
using eng::I;
using eng::N;
using eng::T;

Row ev(std::int64_t id, std::int64_t year, std::int64_t kind, std::int64_t region, std::int64_t salience = 50) {
    return {I(id), I(year), I(year), I(kind), I(1), I(region), I(0), I(salience)};
}
Row part(std::int64_t e, std::int64_t seq, std::int64_t civ) { return {I(e), I(seq), I(seq), I(3), I(civ)}; }
Row cause(std::int64_t e, std::int64_t c) { return {I(e), I(c)}; }
Row pad(Row r, std::size_t n) {
    while (r.size() < n) r.push_back(N());
    return r;
}

// Three wars of c1 (goblins) on c2 (elves) over R5, each ended by a truce: e10 100-105, e20 200-205, e30 320-330.
Tables three_wars(const std::vector<Row>& control) {
    Tables t;
    t.civilization = {pad({I(1), T("A"), I(2)}, 10), pad({I(2), T("B"), I(1)}, 10), pad({I(3), T("C"), I(5)}, 10)};
    t.event = {ev(10, 100, k_war_declared, 5), ev(11, 105, k_truce, 0), ev(20, 200, k_war_declared, 5),
               ev(21, 205, k_truce, 0),        ev(30, 320, k_war_declared, 5), ev(31, 330, k_truce, 0)};
    for (const std::int64_t e : {10, 20, 30}) {
        t.event_participant.push_back(part(e, 1, 1));
        t.event_participant.push_back(part(e, 2, 2));
    }
    t.event_cause = {cause(11, 10), cause(21, 20), cause(31, 30)};
    t.relation = {{I(3), I(1), I(3), I(2), T("war"), I(100), I(105), I(10), I(11)},
                  {I(3), I(1), I(3), I(2), T("war"), I(200), I(205), I(20), I(21)},
                  {I(3), I(1), I(3), I(2), T("war"), I(320), I(330), I(30), I(31)}};
    t.region_control = control;
    return t;
}

std::string unchanged_line(const Tables& t) {
    eng::HistoryInputs in;
    in.params.span = 1000;
    db::RaceRow elf, gob;
    elf.id = 1;
    elf.key = "elf";
    gob.id = 2;
    gob.key = "goblin";
    in.races = {elf, gob};
    for (const std::string& l : stats_lines_21(t, in)) {
        if (l.rfind("wars_same_goal_unchanged_max = ", 0) == 0) return l;
    }
    return {};
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 r14 21.8, P3-07 r12 22.5b: the run statistic counts by holder (resettled: one run; retaken: broken)") {
    // the wood emptied after each war (rows end) and resettled by its holder c2 (new rows of c2): no change of holder
    const Tables same = three_wars({{I(5), I(0), I(104), I(2), I(1)},
                                    {I(5), I(150), I(204), I(2), I(1)},
                                    {I(5), I(250), N(), I(2), I(1)}});
    CHECK(holder_change_years(same, 5) == std::vector<std::int64_t>{0});
    CHECK(unchanged_line(same) == "wars_same_goal_unchanged_max = 3");
    // taken by c3 in 150 and retaken by c2 in 160: changes at 0, 150, 160; the first war's run breaks there
    const Tables other = three_wars({{I(5), I(0), I(150), I(2), I(1)},
                                     {I(5), I(150), I(160), I(3), I(1)},
                                     {I(5), I(160), N(), I(2), I(1)}});
    CHECK(holder_change_years(other, 5) == std::vector<std::int64_t>{0, 150, 160});
    CHECK(unchanged_line(other) == "wars_same_goal_unchanged_max = 2");
    // the weariness check from the rows reads the same holder: a fourth claim on (c2, R5) after the three wars of
    // `same` is a violation with weary_wars 3, after those of `other` not
    Tables same4 = same;
    same4.event.push_back(ev(40, 400, k_claim, 5));
    same4.event_participant.push_back(part(40, 1, 1));
    same4.event_participant.push_back(part(40, 2, 2));
    CHECK(weary_violations(same4, 3) == std::vector<std::int64_t>{40});
    Tables other4 = other;
    other4.event.push_back(ev(40, 400, k_claim, 5));
    other4.event_participant.push_back(part(40, 1, 1));
    other4.event_participant.push_back(part(40, 2, 2));
    CHECK(weary_violations(other4, 3).empty());
}

TEST_CASE("P3-06 r14 (15), 21.7: the row checks fire on hand-made logs that break them") {
    // c1 elves, c2 goblins, c3 faeries. e1 refugees_settled of c3 in 1000 (hidden_years 300). e5 search_abandoned
    // by c1 of c3 in R4 in 1100 (the war e3, its battle e4 in R2), e7 a second one in 1200 (war e6) at salience 15.
    const wd::ContentSet& c = shipped();
    const std::int64_t hidden = conflict_int(c, "hidden_years");
    const std::int64_t repeat = conflict_int(c, "search_repeat_salience");
    REQUIRE(hidden == 300);
    REQUIRE(repeat == 15);
    Tables t;
    t.event_kind = {pad({I(35), T("search_abandoned"), I(1), I(1), I(0), I(40)}, 7)};
    t.event = {ev(1, 1000, k_refugees_settled, 3), ev(3, 1090, k_war_declared, 4), ev(4, 1095, k_battle, 2),
               ev(5, 1100, k_search_abandoned, 4, 40), ev(6, 1150, k_war_declared, 6),
               ev(7, 1200, k_search_abandoned, 6, 15)};
    t.event_participant = {part(1, 1, 9), part(1, 2, 3), part(3, 1, 1), part(3, 2, 3), part(4, 1, 1), part(4, 2, 3),
                           part(5, 1, 1), part(5, 2, 3), part(6, 1, 2), part(6, 2, 3), part(7, 1, 2), part(7, 2, 3)};
    t.event_cause = {cause(4, 3), cause(5, 4), cause(7, 6)};
    t.relation = {{I(3), I(1), I(3), I(3), T("war"), I(1090), I(1100), I(3), I(5)},
                  {I(3), I(2), I(3), I(3), T("war"), I(1150), I(1200), I(6), I(7)}};
    t.region_control = {{I(4), I(900), N(), I(3), I(1)}};
    CHECK(invariants_19(t, c).empty());
    // (15): a colony of c3 in 1250 (inside [1000, 1300)), a claim by c3 in 1299; none counted from 1300
    Tables bad = t;
    bad.event.push_back(ev(8, 1250, k_colony_founded, 3));
    bad.event_participant.push_back(part(8, 1, 20));
    bad.event_participant.push_back(part(8, 2, 3));
    bad.event.push_back(ev(9, 1299, k_claim, 5));
    bad.event_participant.push_back(part(9, 1, 3));
    bad.event_participant.push_back(part(9, 2, 1));
    bad.event.push_back(ev(10, 1300, k_war_declared, 5));
    bad.event_participant.push_back(part(10, 1, 3));
    bad.event_participant.push_back(part(10, 2, 1));
    CHECK(invariants_19(bad, c).size() == 2U);
    // 21.7 item 2: a battle of the war e3 in the searched region R4
    bad = t;
    bad.event.push_back(ev(11, 1096, k_battle, 4));
    bad.event_participant.push_back(part(11, 1, 1));
    bad.event_participant.push_back(part(11, 2, 3));
    bad.event_cause.push_back(cause(11, 3));
    std::sort(bad.event.begin(), bad.event.end(), [](const Row& a, const Row& b) { return at(a, 0) < at(b, 0); });
    CHECK(invariants_19(bad, c).size() == 1U);
    // 21.7 item 3: the second search at salience 40, or the first at 15
    bad = t;
    bad.event[5] = ev(7, 1200, k_search_abandoned, 6, 40);
    CHECK(invariants_19(bad, c).size() == 1U);
    bad.event[3] = ev(5, 1100, k_search_abandoned, 4, 15);
    CHECK(invariants_19(bad, c).size() == 2U);
    // 21.7 item 1: a claim by c2 on c3 over R4 in 1590 (inside 500 years of 1100) is refused; in 1600 it is not;
    // after a change of R4's holder it is not
    bad = t;
    bad.event.push_back(ev(12, 1590, k_claim, 4));
    bad.event_participant.push_back(part(12, 1, 2));
    bad.event_participant.push_back(part(12, 2, 3));
    CHECK(invariants_19(bad, c).size() == 1U);
    bad.event.back() = ev(12, 1600, k_claim, 4);
    CHECK(invariants_19(bad, c).empty());
    bad.event.back() = ev(12, 1590, k_claim, 4);
    bad.region_control = {{I(4), I(900), I(1400), I(3), I(1)}, {I(4), I(1400), I(1500), I(1), I(1)},
                          {I(4), I(1500), N(), I(3), I(1)}};
    CHECK(invariants_19(bad, c).empty());
}

TEST_CASE("P3-06 r14 (14): a cost after a lost goal's ending only when the attacker won the last battle") {
    // c1 attacks c2 over R2 (e1, 100); battle e2 (110) won by c2; R2 passes to c3 in 115; war_won e3 (120) by c1
    // with a tribute e4: (14) breaks. With the battle won by c1 it holds.
    const wd::ContentSet& c = shipped();
    Tables t;
    t.event = {ev(1, 100, k_war_declared, 2), ev(2, 110, k_battle, 2), ev(3, 120, k_war_won, 0),
               ev(4, 120, k_tribute, 0)};
    t.event_participant = {part(1, 1, 1), part(1, 2, 2), part(2, 1, 2), part(2, 2, 1), part(3, 1, 1), part(3, 2, 2),
                           part(4, 1, 2), part(4, 2, 1)};
    t.event_cause = {cause(2, 1), cause(3, 2), cause(4, 3)};
    t.relation = {{I(3), I(1), I(3), I(2), T("war"), I(100), I(120), I(1), I(3)}};
    t.region_control = {{I(2), I(0), I(115), I(2), I(1)}, {I(2), I(115), N(), I(3), I(1)}};
    CHECK(invariants_19(t, c).size() == 1U);
    t.event_participant[2] = part(2, 1, 1);
    t.event_participant[3] = part(2, 2, 2);
    CHECK(invariants_19(t, c).empty());
}


TEST_CASE("P3-06 r14 21.7, 21.9 (seam): ConflictTuning has search_weary_years 500 and search_repeat_salience 15") {
    namespace h = worldgen::history;
    const h::ConflictTuning t = h::conflict_tuning_from(shipped());
    CHECK(t.search_weary_years == 500);
    CHECK(t.search_repeat_salience == 15);
    CHECK(t.search_years == 3);
    CHECK(t.quarrel_strain == 600);  // r15 21.12 item 2
}

TEST_CASE("P3-06 r15 21.12 item 2 (seam): the named check conflict_quarrel_strain") {
    namespace h = worldgen::history;
    h::ConflictTuning t = h::conflict_tuning_from(shipped());
    t.quarrel_strain = t.claim_strain;
    h::check_conflict_tuning(t);
    t.quarrel_strain = t.claim_strain - 1;
    CHECK(fatal_of([&] { h::check_conflict_tuning(t); }) == "history: tuning: quarrel_strain must not be below claim_strain");
}

TEST_CASE("P3-06 r14 21.9, 21.12, P3-01 r11 3.5 (seam): ConflictRace carries odds_min_permille and awakening_quarrel") {
    worldgen::history::ConflictRace r;
    r.odds_min = 500;  // orc
    r.awakening_quarrel = 1;
    CHECK(r.odds_min + r.awakening_quarrel == 501);
}

TEST_CASE("P3-05 r15 22.7a (seam): PeoplesSubsystem::lie_low(civ, settlement, until_year)") {
    namespace h = worldgen::history;
    static_assert(std::is_same_v<decltype(&h::PeoplesSubsystem::lie_low),
                                 void (h::PeoplesSubsystem::*)(std::int64_t, std::int64_t, std::int64_t)>);
    CHECK(true);
}


}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
