// Spec P3-09 (generator 0.18.0): units that need no world.
//
//   - P3-07 r8 21.5's six [stats] keys and 21.8's two on a hand-made fixture with hand-counted values (the test-support
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

#include <algorithm>
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
                                       "race_regions_share_max_permille = 500", "wars_unfound = 0",
                                       "refuges_held_no_destination = 0"});
    // P3-07 r8 21.8: a fifth war w5 e50 (c1, R8) given up (search_abandoned e51) is counted apart: hollow stays 2
    // of the 4 that found their enemy (500); two refuge_held events, one of reason 1
    t.event.push_back(ev(50, 500, k_war_declared, 8));
    t.event.push_back(ev(51, 503, k_search_abandoned, 8));
    t.event.push_back(ev(52, 600, k_refuge_held, 7));
    t.event.push_back(ev(53, 610, k_refuge_held, 7));
    t.event_participant.push_back(part(50, 1, 1));
    t.event_participant.push_back(part(50, 2, 2));
    t.event_value.push_back({I(52), I(1), I(1)});
    t.event_value.push_back({I(53), I(1), I(2)});
    t.event_cause.push_back(cause(51, 50));
    t.relation.push_back({I(3), I(1), I(3), I(2), T("war"), I(500), I(503), I(50), I(51)});
    const std::vector<std::string> l = stats_lines_21(t, in);
    REQUIRE(l.size() == 8U);
    CHECK(l[0] == "wars_hollow = 2");
    CHECK(l[1] == "wars_hollow_permille = 500");
    CHECK(l[2] == "wars_same_goal_max = 3");
    CHECK(l[6] == "wars_unfound = 1");
    CHECK(l[7] == "refuges_held_no_destination = 1");
    // the lead's ruling on wars_same_goal_unchanged_max: a change of R5's owner between w1 (ends 110) and w2
    // (from 200), in 150, breaks the run: w1 1, w2 0, w3 0 (305 in its span): the longest run is 1
    t.region_control = {{I(5), I(0), I(150), I(2), I(1)}, {I(5), I(150), I(305), I(2), I(1)},
                        {I(5), I(305), N(), I(1), I(33)}, {I(7), I(0), N(), I(2), I(1)}};
    CHECK(stats_lines_21(t, in)[3] == "wars_same_goal_unchanged_max = 1");
    // no war: 0 for every war key; no people, no regions: 0 for the shares
    Tables empty;
    CHECK(stats_lines_21(empty, in) == std::vector<std::string>{
                                           "wars_hollow = 0", "wars_hollow_permille = 0", "wars_same_goal_max = 0",
                                           "wars_same_goal_unchanged_max = 0", "race_population_share_max_permille = 0",
                                           "race_regions_share_max_permille = 0", "wars_unfound = 0",
                                           "refuges_held_no_destination = 0"});
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

TEST_CASE("P3-06 r11 20.23, 20.13 item 12 from rows: wars in progress count toward weariness") {
    // two truces of c2's R5 (count 2), then c1 declares (e30, open) and c3 declares while it is open (e40)
    Tables t;
    t.event = {ev(10, 100, k_war_declared, 5), ev(11, 110, k_truce, 0), ev(20, 200, k_war_declared, 5),
               ev(21, 210, k_truce, 0), ev(30, 300, k_war_declared, 5), ev(40, 301, k_war_declared, 5),
               ev(41, 320, k_truce, 0)};
    for (const auto& [e, a] : std::vector<std::pair<std::int64_t, std::int64_t>>{{10, 1}, {20, 3}, {30, 1}, {40, 3}}) {
        t.event_participant.push_back(part(e, 1, a));
        t.event_participant.push_back(part(e, 2, 2));
    }
    t.relation = {{I(3), I(1), I(3), I(2), T("war"), I(100), I(110), I(10), I(11)},
                  {I(3), I(3), I(3), I(2), T("war"), I(200), I(210), I(20), I(21)},
                  {I(3), I(1), I(3), I(2), T("war"), I(300), N(), I(30), N()},
                  {I(3), I(3), I(3), I(2), T("war"), I(301), I(320), I(40), I(41)}};
    t.region_control = {{I(5), I(0), N(), I(2), I(1)}};
    CHECK(weary_violations(t, 3) == std::vector<std::int64_t>{40});  // 2 futile + 1 open = 3
    CHECK(weary_violations(t, 4).empty());
    CHECK(weary_violations(t, 0).empty());
    // the first war's goal changed hands meanwhile (the count resets): only the open war counts, 1 < 3
    t.region_control = {{I(5), I(0), I(250), I(2), I(1)}, {I(5), I(250), N(), I(2), I(1)}};
    CHECK(weary_violations(t, 3).empty());
    // weary_wars 1: e20 follows one futile war (no change by 200); e40 meets the open e30
    CHECK(weary_violations(t, 1) == std::vector<std::int64_t>{20, 40});
}

namespace {

// P3-06 r13 20.28: one defender c2, region R5, attacker c1; each war (declaration id, since, until or -1 for open,
// ending kind); region_control rows of R5 as (from, to or -1). Returns wars_same_goal_unchanged_max.
struct HandWar {
    std::int64_t d, since, until, end_kind;
};
std::string same_goal_unchanged(const std::vector<HandWar>& wars,
                                const std::vector<std::pair<std::int64_t, std::int64_t>>& rows, std::int64_t E = 1000) {
    Tables t;
    std::int64_t nid = 1000;
    for (const HandWar& w : wars) {
        t.event.push_back(ev(w.d, w.since, k_war_declared, 5));
        t.event_participant.push_back(part(w.d, 1, 1));
        t.event_participant.push_back(part(w.d, 2, 2));
        if (w.until < 0) {
            t.relation.push_back({I(3), I(1), I(3), I(2), T("war"), I(w.since), N(), I(w.d), N()});
        } else {
            ++nid;
            t.event.push_back(ev(nid, w.until, w.end_kind, 0));
            t.relation.push_back({I(3), I(1), I(3), I(2), T("war"), I(w.since), I(w.until), I(w.d), I(nid)});
        }
    }
    std::sort(t.event.begin(), t.event.end(), [](const Row& a, const Row& b) { return at(a, 0) < at(b, 0); });
    for (const auto& [a, b] : rows) t.region_control.push_back({I(5), I(a), b < 0 ? N() : I(b), I(2), I(1)});
    eng::HistoryInputs in;
    in.params.span = E;
    return stats_lines_21(t, in)[3];
}
std::string unchanged_key(std::int64_t n) { return "wars_same_goal_unchanged_max = " + std::to_string(n); }

// P3-06 r13 20.27 item 6 row fixtures: events (id, year, kind, region, settlement); settlements (id, region,
// founder, founding event, owner, status); census (settlement, year, owner, status); region_control rows.
Row ev_s(std::int64_t id, std::int64_t year, std::int64_t kind, std::int64_t region, std::int64_t settlement) {
    return {I(id), I(year), I(year), I(kind), I(1), I(region), I(settlement), I(50)};
}
Row stl(std::int64_t id, std::int64_t region, std::int64_t founder, std::int64_t founding, std::int64_t owner,
        std::int64_t status) {
    return {I(id), T("S"), I(0),  I(0),     I(region), I(founder), I(0), I(founding), I(owner), I(1), I(status),
            I(100), N(),  I(0)};
}
Row cen(std::int64_t s, std::int64_t year, std::int64_t owner, std::int64_t status) {
    return {I(s), I(year), I(owner), I(1), I(status), I(100)};
}
Row rc(std::int64_t region, std::int64_t from, std::int64_t to, std::int64_t civ, std::int64_t event) {
    return {I(region), I(from), to < 0 ? N() : I(to), I(civ), I(event)};
}

}  // namespace

TEST_CASE("P3-06 r13 20.28, P3-07 r11 21.8 item 5: wars_same_goal_unchanged_max counts the wars weariness counts") {
    // 1024 seed 6 (20.28's table): e341 war_won, e445 peace, e453 closed by civ_conquered, e538 truce; no change of
    // region 40's owner in 4892-5597: e453 is skipped, key 3 (21.8 item 5 as written before r11 gave 4)
    const std::vector<HandWar> seed6 = {{341, 4892, 4922, k_war_won}, {445, 5297, 5309, k_peace},
                                        {453, 5310, 5338, k_civ_conquered}, {538, 5591, 5597, k_truce}};
    CHECK(same_goal_unchanged(seed6, {{0, -1}}, 6000) == unchanged_key(3));
    std::vector<HandWar> counted = seed6;
    counted[2].end_kind = k_peace;  // had e453 ended by a peace, it would extend the run
    CHECK(same_goal_unchanged(counted, {{0, -1}}, 6000) == unchanged_key(4));
    // 1024 calm seed 17: e285 (5577-5587), e297 (5588-5598), changes at 5039 and 5857 only: key 2 (D3: not 1)
    CHECK(same_goal_unchanged({{285, 5577, 5587, k_peace}, {297, 5588, 5598, k_truce}}, {{0, 5039}, {5039, 5857}, {5857, -1}},
                              6000) == unchanged_key(2));
    // a war closed by civ_conquered (or civ_ended) between two peace wars neither extends nor breaks the run
    CHECK(same_goal_unchanged({{10, 100, 110, k_peace}, {20, 200, 210, k_civ_conquered}, {30, 300, 310, k_peace}}, {{0, -1}}) ==
          unchanged_key(2));
    CHECK(same_goal_unchanged({{10, 100, 110, k_peace}, {20, 200, 210, k_civ_ended}, {30, 300, 310, k_peace}}, {{0, -1}}) ==
          unchanged_key(2));
    // ... but a change of owner in its span (205) breaks the run, being in the window [100, 310]
    CHECK(same_goal_unchanged({{10, 100, 110, k_peace}, {20, 200, 210, k_civ_conquered}, {30, 300, 310, k_peace}},
                              {{0, 205}, {205, -1}}) == unchanged_key(1));
    // a change between two wars: the second starts a run of 1; a third extends it
    CHECK(same_goal_unchanged({{10, 100, 110, k_peace}, {20, 200, 210, k_peace}}, {{0, 150}, {150, -1}}) == unchanged_key(1));
    CHECK(same_goal_unchanged({{10, 100, 110, k_peace}, {20, 200, 210, k_peace}, {30, 300, 310, k_truce}},
                              {{0, 150}, {150, -1}}) == unchanged_key(2));
    // a war whose own span holds the change is in no run; the next starts again at 1
    CHECK(same_goal_unchanged({{10, 100, 110, k_peace}, {20, 200, 210, k_peace}}, {{0, 105}, {105, -1}}) == unchanged_key(1));
    // three overlapping wars, the middle ending last: one run of 3 with the change after them (450), 1 with it at 300
    const std::vector<HandWar> overlap = {{10, 100, 150, k_peace}, {20, 120, 400, k_peace}, {30, 200, 250, k_peace}};
    CHECK(same_goal_unchanged(overlap, {{0, 450}, {450, -1}}) == unchanged_key(3));
    CHECK(same_goal_unchanged(overlap, {{0, 300}, {300, -1}}) == unchanged_key(1));
    // the window ends at the latest end in the run, not the last war's: w1 100-400, w2 200-250, a change at 300
    CHECK(same_goal_unchanged({{10, 100, 400, k_peace}, {20, 200, 250, k_peace}}, {{0, 300}, {300, -1}}) == unchanged_key(1));
    // an open war counts with until = E
    CHECK(same_goal_unchanged({{10, 100, 110, k_peace}, {20, 200, -1, 0}}, {{0, -1}}) == unchanged_key(2));
    CHECK(same_goal_unchanged({{10, 100, 110, k_peace}, {20, 200, -1, 0}}, {{0, 900}, {900, -1}}) == unchanged_key(1));
    CHECK(same_goal_unchanged({}, {{0, -1}}) == unchanged_key(0));
}

TEST_CASE("P3-06 r13 20.27 item 6, 20.13 (13) from rows: a cession passes every settlement of the loser in the region") {
    // c1 (s1 in R1), c2 (s2 in R3); in the step of 500 c2 founds the colony s3 in the free R2, then loses a war to c1
    // and cedes R2 (e5). c2 has no region_control row of R2 (P3-04 6.7 item 1's net rule): its hold is read from the
    // log.
    Tables good;
    good.event = {ev_s(1, 0, k_civ_founded, 1, 1), ev_s(2, 0, k_civ_founded, 3, 2), ev_s(3, 500, k_colony_founded, 2, 3),
                  ev_s(4, 500, k_peace, 0, 0), ev_s(5, 500, k_region_ceded, 2, 0)};
    good.event_participant = {part(5, 1, 2), part(5, 2, 1)};
    good.settlement = {stl(1, 1, 1, 1, 1, thriving), stl(2, 3, 2, 2, 2, thriving), stl(3, 2, 2, 3, 1, thriving)};
    good.settlement_census = {cen(1, 0, 1, thriving), cen(2, 0, 2, thriving), cen(3, 500, 1, thriving)};
    good.region_control = {rc(1, 0, -1, 1, 1), rc(2, 500, -1, 1, 5), rc(3, 0, -1, 2, 2)};
    CHECK(cession_violations(good).empty());
    // the defect D1 (2048 seed 3 at 0.18.0 before r13): the colony stays c2's
    Tables bad = good;
    bad.settlement[2] = stl(3, 2, 2, 3, 2, thriving);
    bad.settlement_census[2] = cen(3, 500, 2, thriving);
    CHECK(cession_violations(bad) == std::vector<std::string>{"e5: s3 of c2 in rg2 did not pass to c1 (census of 500: c2)",
                                                              "s3: replayed owner c1 standing True, stored c2 status 1"});
    // an older settlement of c2 in R2 held by a row passes; a cession of a region c2 never held is found
    Tables held;
    held.event = {ev_s(1, 0, k_civ_founded, 1, 1), ev_s(2, 0, k_civ_founded, 3, 2), ev_s(3, 100, k_colony_founded, 2, 3),
                  ev_s(4, 500, k_peace, 0, 0),     ev_s(5, 500, k_region_ceded, 2, 0), ev_s(6, 600, k_peace, 0, 0),
                  ev_s(7, 600, k_region_ceded, 4, 0)};
    held.event_participant = {part(5, 1, 2), part(5, 2, 1), part(7, 1, 2), part(7, 2, 1)};
    held.settlement = good.settlement;
    held.settlement_census = {cen(1, 0, 1, thriving), cen(2, 0, 2, thriving), cen(3, 100, 2, thriving),
                              cen(3, 500, 1, thriving)};
    held.region_control = {rc(1, 0, -1, 1, 1), rc(2, 100, 500, 2, 3), rc(2, 500, -1, 1, 5), rc(3, 0, -1, 2, 2),
                           rc(4, 600, -1, 1, 7)};
    CHECK(cession_violations(held) == std::vector<std::string>{"e7: c2 ceded rg4, which it did not hold"});
    // captures and ruins are replayed: s3 captured by c1 (e6) and s4 sacked (e7) before the cession e9
    Tables cap;
    cap.event = {ev_s(1, 0, k_civ_founded, 1, 1),         ev_s(2, 0, k_civ_founded, 3, 2),
                 ev_s(3, 100, k_colony_founded, 2, 3),    ev_s(4, 100, k_settlement_founded, 2, 4),
                 ev_s(6, 400, k_settlement_captured, 2, 3), ev_s(7, 450, k_sacked, 2, 4),
                 ev_s(8, 500, k_peace, 0, 0),             ev_s(9, 500, k_region_ceded, 2, 0)};
    cap.event_participant = {part(6, 1, 1), part(6, 2, 2), part(7, 1, 1), part(7, 2, 2), part(9, 1, 2), part(9, 2, 1)};
    cap.settlement = {stl(1, 1, 1, 1, 1, thriving), stl(2, 3, 2, 2, 2, thriving), stl(3, 2, 2, 3, 1, thriving),
                      stl(4, 2, 2, 4, 0, ruined)};
    cap.settlement_census = {cen(1, 0, 1, thriving),   cen(2, 0, 2, thriving), cen(3, 100, 2, thriving),
                             cen(3, 400, 1, thriving), cen(4, 100, 2, thriving), cen(4, 450, 0, ruined)};
    cap.region_control = {rc(1, 0, -1, 1, 1), rc(2, 100, -1, 2, 3), rc(3, 0, -1, 2, 2)};
    CHECK(cession_violations(cap).empty());
}

#if UNMAPPED_P3_09_DIGEST

TEST_CASE("P3-07 r8 21.4 (P3-09): history_note equals the golden notes of the world cases; no note elsewhere") {
    for (const char* name : {"geo-seed-42-512", "hist-conflict-flight"}) {
        const CCase& g = conflict_case(name);
        CAPTURE(g.name);
        const std::string path =
            chrono_test::world(chrono_test::WorldKey{g.seed, "", g.size, g.span, g.races, "", 1});
        const db::WorldReader r = open_world(path);
        const auto view = worldgen::history::HistoryView::load(r);
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
    CHECK(t.kill_max_permille == 500);  // r10 20.19
    CHECK(t.find_permille == 250);
    CHECK(t.search_years == 3);
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

TEST_CASE("P3-06 r9 20.12, r10 20.17 (seam): ConflictRace carries muster, conceal_min and home defence") {
    worldgen::history::ConflictRace r;
    r.muster = 4000;
    r.conceal_min = 400;
    r.home_defense = 0;  // faerie, r10 20.17 item 3
    CHECK(r.muster + r.conceal_min + r.home_defense == 4400);
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
    // P3-05 r8 21 item 1 names settlement_name "one non-const call on the subsystem"; the implementation keeps
    // it private (P3-06 reaches it as a friend), so its signature is not asserted here (reported at integration).
    CHECK(true);
}

TEST_CASE("P3-06 r9, r10 (seams): muster, odds, truce, home defence, the score of people and the kill bound") {
    namespace h = worldgen::history;
    const h::ConflictTuning t = h::conflict_tuning_from(shipped());
    // 20.6 item 1: two orc towns of 1000 (levy 100 each) x prowess 1300 x muster 4000
    const std::vector<std::pair<std::int64_t, h::SettlementKind>> orcs = {{1000, h::SettlementKind::town},
                                                                         {1000, h::SettlementKind::town}};
    CHECK(h::force_of(t, orcs, 1300, 4000) == 200 * 1300 / 1000 * 4000 / 1000);
    CHECK(h::force_of(t, orcs, 1300) == h::force_of(t, orcs, 1300, 1000));
    // 20.6 item 3: odds = min(1000, fa x 1000 div (fb x ratio div 1000 x max(1000 - g', floor) div 1000))
    CHECK(h::declare_odds(t, 160, 800, 400) == 160 * 1000 / 480);
    CHECK(h::declare_odds(t, 160, 800, 150) == 160 * 1000 / 680);
    CHECK(h::declare_odds(t, 160, 800, 900) == 160 * 1000 / 400);  // the floor 500
    CHECK(h::declare_odds(t, 100000, 1, 150) == 1000);
    // 20.6 item 4: the orcs (900) keep a truce of 50 years, the goblins (600) and elves 200
    CHECK(h::truce_length(t, 900) == 50);
    CHECK(h::truce_length(t, 600) == 200);
    CHECK(h::truce_length(t, 150) == 200);
    // 20.7 item 2: a capital village of 500 faeries (prowess 1200), home defence 2000 at B 250
    const std::int64_t g0 = 500 * 300 / 1000 * 1200 / 1000 * (1500 + 2000) / 1000;
    CHECK(h::garrison_of(t, 500, 1200, h::SettlementKind::village, true) == g0);
    CHECK(h::garrison_of(t, 500, 1200, h::SettlementKind::village, true, 2000, 250) == g0 * 1500 / 1000);
    CHECK(h::garrison_of(t, 500, 1200, h::SettlementKind::village, true, 2000, -300) == g0);  // max(0, B)
    h::DefenceTerms d;
    d.home_defense = 1500;
    d.biome_term = 500;
    CHECK(h::defence_of(t, d) == 1000 + 1500 * 500 / 1000);
    // 20.15 item 1: s = (dead(b) - dead(a)) + (taken(a) - taken(b))
    CHECK(h::war_score_people(300, 500, 0, 150) == 50);
    CHECK(h::war_score_people(300, 500, 0, 200) == 0);
    CHECK(h::war_score_people(0, 0, 0, 1) == -1);
    // 20.16: dead_L = min(F_L x 250, F_W x 500) div 1000; F_W = F_L / 20
    CHECK(h::loser_dead_of(t, 80, 1600) == 40);
    CHECK(h::loser_dead_of(t, 1600, 80) == 20);
}

#if UNMAPPED_P3_09_DIGEST
TEST_CASE("P3-07 r8 21.4 item 3 (seam): history_ordinal") {
    namespace h = worldgen::history;
    const std::vector<std::pair<std::int64_t, const char*>> want = {
        {2, "second"}, {3, "third"}, {4, "fourth"}, {5, "fifth"}, {6, "sixth"}, {7, "seventh"}, {8, "eighth"},
        {9, "ninth"}, {10, "tenth"}, {11, "11th"}, {12, "12th"}, {13, "13th"}, {21, "21st"}, {22, "22nd"},
        {23, "23rd"}, {24, "24th"}, {101, "101st"}, {111, "111th"}, {112, "112th"}, {113, "113th"}, {121, "121st"}};
    for (const auto& [n, text] : want) {
        CAPTURE(n);
        CHECK(h::history_ordinal(n) == text);
    }
}
#endif


#endif  // UNMAPPED_P3_09_API

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
