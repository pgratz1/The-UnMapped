// Spec P3-06 r17 section 22 (P3-10 second round, docs/specs/p3-10-followups.md section 12, items 18, 19, 21, 24,
// 25, 26; generator stays 0.19.0), with P3-01 r13 3.6:
//   22.1  the awakening-quarrel claim is held back while its civilization has fewer than quarrel_min_settlements
//         (2) standing settlements: on a hand-made geography of elves and orcs whose orcs keep one settlement, the
//         orcs claim the elves but never declare on them, and no flare-up is triggered by their claim, until the
//         first war between the pair (whoever declares it);
//   22.2  hidden_years is a race field (orc 100, others 300); history.conflict has no hidden_years;
//   22.3  lying low by the one definition, from the rows (invariant (15) restated: the refuge standing and the
//         civilization's, its race's hidden years);
//   22.5  aim needs the defender out of the goal, the foothold cession (a region_ceded with no region_control
//         row), invariant (8) restated and the new (17), the quick-aim-win count: the row checks of
//         invariants_r17 (p3_06_conflict_support.hpp) fire on hand-made logs that break them and stay silent on
//         the same logs repaired; on the golden worlds they hold and their facts equal the reference's r17 lines
//         (history_conflict_golden_test.cpp, history_conflict_property_test.cpp, history_balance_golden_test.cpp);
//   22.6  the search's weariness bound is exclusive (history_conflict_r14_test.cpp, the 1599 and 1600 claims);
//   22.7  the named check conflict_quarrel_strain_max: strain_max 500 with quarrel_strain 600 fails, 600 and 600
//         passes (exact texts of 16.4's form).
// The hand-made state units of 22.10 items 1 to 5 (the declaration and its draw, the tension, a colony visible
// the next year, the aim endings and the foothold cession step by step, lying low ended early) are the
// reference's (reference/p3_conflict_reference.py check_units_r17); the public seam cannot set conflict's strain
// or claims.
//
// Written from the specs before the implementation (test-writer rule). The tuning and data cases need no new API
// and fail until the round's code and data land; the seam cases compile only with UNMAPPED_P3_10_R3_API 1.

#include "../p3_10_r3_switch.hpp"
#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#if UNMAPPED_HAVE_CONFLICT
#include "worldgen/history/conflict.hpp"
#endif

namespace {

using namespace conflict_test;
using eng::I;
using eng::N;
using eng::T;

Row ev(std::int64_t id, std::int64_t year, std::int64_t kind, std::int64_t region, std::int64_t settlement = 0) {
    return {I(id), I(year), I(year), I(kind), I(1), I(region), I(settlement), I(50)};
}
Row part(std::int64_t e, std::int64_t seq, std::int64_t type, std::int64_t id) { return {I(e), I(seq), I(seq), I(type), I(id)}; }
Row cause(std::int64_t e, std::int64_t c) { return {I(e), I(c)}; }
// civilization: id, name, race, culture, home_region, capital, founded_year, founding_event, ended_year, ending_event
Row civ_row(std::int64_t id, std::int64_t race, std::int64_t capital) {
    return {I(id), T("C" + std::to_string(id)), I(race), I(race), I(1), I(capital), I(0), I(1), N(), I(0)};
}
// settlement: id, name, x, y, region, founder, founded_year, founding_event, owner, kind, status, population,
// ruined_year, ruin_event
Row stl_row(std::int64_t id, std::int64_t region, std::int64_t founder, std::int64_t event, std::int64_t owner,
            std::int64_t status) {
    return {I(id), T("S" + std::to_string(id)), I(id), I(1), I(region), I(founder), I(0), I(event), I(owner), I(2),
            I(status), I(100), N(), I(0)};
}
constexpr std::int64_t CIV = 3, SET = 4;  // entity types of event_participant (P3-04 9.2)

std::int64_t orc_hidden_years() { return race_hidden_years(shipped()).at(6); }

// (15): c1 elves (s1), c2 orcs (s2 founded in 900; the refuge s3 of e3, a flight of 1000); a claim by c2 in
// `claim_year`; with `captured`, c1 takes s3 in 1040 first.
Tables flight_log(std::int64_t claim_year, bool captured) {
    Tables t;
    t.civilization = {civ_row(1, 1, 1), civ_row(2, 6, 2)};
    t.event = {ev(1, 0, k_settlement_founded, 1, 1), ev(2, 900, k_settlement_founded, 2, 2),
               ev(3, 1000, k_refugees_settled, 3, 3)};
    t.event_participant = {part(3, 1, SET, 3), part(3, 2, CIV, 2), part(3, 3, SET, 2)};
    std::int64_t next = 4;
    if (captured) {
        t.event.push_back(ev(next, 1040, k_settlement_captured, 3, 3));
        t.event_participant.push_back(part(next, 1, CIV, 1));
        t.event_participant.push_back(part(next, 2, CIV, 2));
        ++next;
    }
    t.event.push_back(ev(next, claim_year, k_claim, 1));
    t.event_participant.push_back(part(next, 1, CIV, 2));
    t.event_participant.push_back(part(next, 2, CIV, 1));
    t.settlement = {stl_row(1, 1, 1, 1, 1, 1), stl_row(2, 2, 2, 2, 2, 1), stl_row(3, 3, 2, 3, captured ? 1 : 2, 1)};
    t.region_control = {{I(1), I(0), N(), I(1), I(1)}, {I(2), I(900), N(), I(2), I(2)}};
    return t;
}

// (8), (17) and the facts: c1 goblins (s1 in R1) attack c2 elves (capital s2 in R4) over R5, which c1 holds (s3,
// from 1100); c2 keeps the foothold s4 in R5. War e10 (1200): `battle_by` 0 no battle, else the winner of battle
// e11 (1205); `foothold_lost` c1 captures s4 in 1206 (e12) [with a battle] or s4 is abandoned in 1200 (e12)
// [without]; the war_won e13 by c1 in `end_year`; `cession` a region_ceded e14 of R5 from c2 to c1 caused by e13
// (no region_control row: a foothold cession).
Tables war_log(std::int64_t battle_by, bool foothold_lost, bool cession, std::int64_t end_year = 1210) {
    Tables t;
    t.civilization = {civ_row(1, 2, 1), civ_row(2, 1, 2)};
    t.event = {ev(1, 0, k_settlement_founded, 1, 1), ev(2, 0, k_settlement_founded, 4, 2),
               ev(3, 1100, k_settlement_founded, 5, 3), ev(4, 1000, k_settlement_founded, 5, 4)};
    std::sort(t.event.begin(), t.event.end(), [](const Row& a, const Row& b) { return at(a, 0) < at(b, 0); });
    t.event.push_back(ev(10, 1200, k_war_declared, 5));
    t.event_participant = {part(10, 1, CIV, 1), part(10, 2, CIV, 2)};
    if (battle_by != 0) {
        t.event.push_back(ev(11, 1205, k_battle, 5));
        t.event_participant.push_back(part(11, 1, CIV, battle_by));
        t.event_participant.push_back(part(11, 2, CIV, 3 - battle_by));
        t.event_cause.push_back(cause(11, 10));
    }
    bool s4_c1 = cession, s4_ruined = false;
    if (foothold_lost) {
        if (battle_by != 0) {
            t.event.push_back(ev(12, 1206, k_settlement_captured, 5, 4));
            t.event_participant.push_back(part(12, 1, CIV, 1));
            t.event_participant.push_back(part(12, 2, CIV, 2));
            s4_c1 = true;
        } else {
            t.event.push_back(ev(12, 1200, k_settlement_abandoned, 5, 4));
            s4_ruined = true;
        }
    }
    t.event.push_back(ev(13, end_year, k_war_won, 0));
    t.event_participant.push_back(part(13, 1, CIV, 1));
    t.event_participant.push_back(part(13, 2, CIV, 2));
    t.event_cause.push_back(cause(13, battle_by != 0 ? 11 : 10));
    t.event_value.push_back({I(13), I(1), I(end_year - 1200)});
    if (cession) {
        t.event.push_back(ev(14, end_year, k_region_ceded, 5));
        t.event_participant.push_back(part(14, 1, CIV, 2));
        t.event_participant.push_back(part(14, 2, CIV, 1));
        t.event_cause.push_back(cause(14, 13));
    }
    t.relation = {{I(3), I(1), I(3), I(2), T("war"), I(1200), I(end_year), I(10), I(13)}};
    t.region_control = {{I(1), I(0), N(), I(1), I(1)}, {I(4), I(0), N(), I(2), I(2)}, {I(5), I(1100), N(), I(1), I(3)}};
    t.settlement = {stl_row(1, 1, 1, 1, 1, 1), stl_row(2, 4, 2, 2, 2, 1), stl_row(3, 5, 1, 3, 1, 1),
                    stl_row(4, 5, 2, 4, s4_ruined ? 0 : (s4_c1 ? 1 : 2), s4_ruined ? 3 : 1)};
    return t;
}

std::size_t count_of(const std::vector<std::string>& v, const char* prefix) {
    return static_cast<std::size_t>(
        std::count_if(v.begin(), v.end(), [&](const std::string& s) { return s.rfind(prefix, 0) == 0; }));
}

std::string configure_error(const wd::ContentSet& c) {
#if UNMAPPED_HAVE_CONFLICT
    return fatal_of([&] { (void)eng::history_stage(c, synthetic_inputs(conflict_case("syn-0"))); });
#else
    (void)c;
    return "no conflict subsystem";
#endif
}

// history.conflict with fields replaced, and appended when absent (the first round's records have no
// quarrel_min_settlements).
wd::ContentSet with_conflict_set(const std::map<std::string, wd::Value>& edits) {
    wd::ContentSet c = with_conflict(edits);
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (wd::Definition& d : k.definitions) {
            if (d.id != "conflict") continue;
            for (const auto& [name, v] : edits) {
                bool has = false;
                for (std::size_t i = 0; i < d.record.size(); ++i) has = has || d.record.name_at(i) == name;
                if (!has) d.record.add(name, v);
            }
        }
    }
    return c;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 r17 22.2, 22.9, P3-01 r13 3.6: hidden_years per race (orc 100, others 300); none in history.conflict") {
    const wd::KindContent* race = shipped().find_kind("race");
    REQUIRE(race != nullptr);
    for (const wd::Definition& d : race->definitions) {
        CAPTURE(d.id);
        bool has = false;
        for (std::size_t i = 0; i < d.record.size(); ++i) has = has || d.record.name_at(i) == "hidden_years";
        REQUIRE(has);
        CHECK(d.record.get_int("hidden_years") == (d.id == "orc" ? 100 : 300));
    }
    const std::map<std::int64_t, std::int64_t> hy = race_hidden_years(shipped());
    CHECK(hy == std::map<std::int64_t, std::int64_t>{{1, 300}, {2, 300}, {3, 300}, {4, 300}, {5, 300}, {6, 100}});
    for (const wd::KindContent& k : shipped().kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id != "conflict") continue;
            for (std::size_t i = 0; i < d.record.size(); ++i) CHECK(d.record.name_at(i) != "hidden_years");
            CHECK(d.record.get_int("quarrel_min_settlements") == 2);  // 22.1 item 4
        }
    }
}

TEST_CASE("P3-06 r17 22.7, 22.1 item 4: conflict_quarrel_strain_max and the range of quarrel_min_settlements, exact texts") {
    CHECK(configure_error(with_conflict_set({{"strain_max", I64(500)},
                                             {"quarrel_strain", I64(600)},
                                             {"claim_strain", I64(100)},
                                             {"declare_min", I64(100)},
                                             {"declare_span", I64(400)}})) ==
          "history: tuning: quarrel_strain must not be above strain_max");
    CHECK(configure_error(with_conflict_set({{"strain_max", I64(600)},
                                             {"quarrel_strain", I64(600)},
                                             {"claim_strain", I64(100)},
                                             {"declare_min", I64(100)},
                                             {"declare_span", I64(500)}})) == "<no error>");
    CHECK(configure_error(with_conflict_set({{"quarrel_min_settlements", I64(1001)}})) ==
          "history: tuning: conflict.quarrel_min_settlements outside its schema range");
    CHECK(configure_error(with_conflict_set({{"quarrel_min_settlements", I64(-1)}})) ==
          "history: tuning: conflict.quarrel_min_settlements outside its schema range");
    CHECK(configure_error(with_conflict_set({{"quarrel_min_settlements", I64(1000)}})) == "<no error>");
}

TEST_CASE("P3-06 r17 22.3, (15) restated: lying low from the rows (the refuge standing and its own, the race's years)") {
    const wd::ContentSet& c = shipped();
    const std::int64_t orc = orc_hidden_years();
    // a claim by the orcs 50 years after their flight: they lie low (100 years for orcs)
    CHECK(count_of(invariants_r17(flight_log(1050, false), c).problems, "(15)") == 1U);
    // ... at 1000 + 99 still, at 1000 + 100 no longer (r17: orc 100)
    CHECK(count_of(invariants_r17(flight_log(1000 + orc - 1, false), c).problems, "(15)") == 1U);
    CHECK(count_of(invariants_r17(flight_log(1000 + orc, false), c).problems, "(15)") == 0U);
    CHECK(orc == 100);
    // a claim at 1150 is allowed for orcs (it would not be under 300 hidden years)
    CHECK(count_of(invariants_r17(flight_log(1150, false), c).problems, "(15)") == 0U);
    // the refuge taken by the elves in 1040: from then the orcs no longer lie low
    CHECK(count_of(invariants_r17(flight_log(1050, true), c).problems, "(15)") == 0U);
    // the replay agrees with the tables (no replay problem on these logs)
    CHECK(count_of(invariants_r17(flight_log(1050, true), c).problems, "replay") == 0U);
    CHECK(count_of(invariants_r17(flight_log(1050, false), c).problems, "replay") == 0U);
}

TEST_CASE("P3-06 r17 22.5, (8) restated: a winner that lost the last battle wins only holding G with the loser out of it") {
    const wd::ContentSet& c = shipped();
    // c2 won the last battle, c1 holds R5 but c2's foothold s4 stands: (8) breaks
    R17 r = invariants_r17(war_log(2, false, false), c);
    CHECK(count_of(r.problems, "(8)") == 1U);
    CHECK(count_of(r.problems, "replay") == 0U);
    // the foothold captured before the ending: aim, (8) holds; no quick aim win (a battle was fought)
    r = invariants_r17(war_log(2, true, false), c);
    CHECK(count_of(r.problems, "(8)") == 0U);
    CHECK(r.quick_aim_wins == 0);
    CHECK(count_of(r.problems, "replay") == 0U);
}

TEST_CASE("P3-06 r17 22.5, (17): after an attacker's win over a held goal the loser is out of it, by a foothold cession") {
    const wd::ContentSet& c = shipped();
    // won by the score (c1 won the last battle), no cost in R5: c2 still stands in the region c1 holds: (17) breaks
    R17 r = invariants_r17(war_log(1, false, false), c);
    CHECK(count_of(r.problems, "(17)") == 1U);
    CHECK(r.foothold_cessions == 0);
    // the foothold cession passes s4: (17) holds; one foothold cession (no region_control row of e14)
    r = invariants_r17(war_log(1, false, true), c);
    CHECK(count_of(r.problems, "(17)") == 0U);
    CHECK(count_of(r.problems, "replay") == 0U);
    CHECK(r.foothold_cessions == 1);
    CHECK(r.line() == "quick_aim_wins 0 foothold_cessions 1 held_goal_wars 1/0/0/0");
}

TEST_CASE("P3-06 r17 22.5: a quick aim win: no battle, one year, the loser's last settlement in G lost otherwise") {
    const wd::ContentSet& c = shipped();
    R17 r = invariants_r17(war_log(0, true, false, 1201), c);
    CHECK(r.problems == std::vector<std::string>{});
    CHECK(r.quick_aim_wins == 1);
    // two years: not quick
    r = invariants_r17(war_log(0, true, false, 1202), c);
    CHECK(r.quick_aim_wins == 0);
}

#if UNMAPPED_HAVE_CONFLICT
TEST_CASE("P3-06 r17 22.1: one-settlement orcs claim the elves but never declare on them before their first war") {
    // Two regions, elves and orcs, one civilization each; peoples founds nothing (found_rate 0), so the orcs keep
    // one settlement: their every claim on the elves is the quarrel claim (the only people they can come to war
    // with is their quarrel neighbour) until the first war between them, and with quarrel_min_settlements 2 it is
    // held back: no war_declared with the orcs as attacker before the elves' first declaration on them, and no
    // flare-up whose trigger is an orc claim on the elves (5.4: a held-back claim reports nothing).
    db::RegionRow a = peoples_test::plain_region(1, 0, 0, 31, 63);
    db::RegionRow b = peoples_test::plain_region(2, 32, 0, 63, 63);
    for (db::RegionRow* r : {&a, &b}) {
        r->cells = 2048;
        r->walk_cells = 2000;
    }
    peoples_test::Geo g = peoples_test::hand_made(64, {a, b});
    g.borders = {db::RegionBorderRow{1, 2, 1024, 0, 0, 0, 10, 0, 0, 0, 0}};
    g.region_biomes = {{1, 4, 2048}, {2, 4, 2048}};
    peoples_test::Settings s;
    s.span = 6000;
    s.races = {"elf", "orc"};
    s.civs = 1;
    const wd::ContentSet content = with_conflict_set({{"quarrel_min_settlements", I64(2)}});  // as shipped
    const wd::ContentSet c = peoples_test::with_peoples({{"found_rate_permille", I64(0)}}, content);
    const eng::HistoryInputs in = peoples_test::inputs_of(g, 0, s, c);
    const Tables t = eng::history_stage(c, in);
    const Index x(t);
    std::int64_t orc = 0, elf = 0;
    for (const Row& cv : t.civilization) (at(cv, 2) == 6 ? orc : elf) = at(cv, 0);
    REQUIRE(orc != 0);
    REQUIRE(elf != 0);
    std::int64_t first_war = 0, orc_claims = 0;
    for (const Row& e : t.event) {
        const std::int64_t id = at(e, 0);
        if (at(e, 3) == k_claim && x.role1(id, 1) == orc && x.role1(id, 2) == elf && first_war == 0) ++orc_claims;
        if (at(e, 3) == k_war_declared && first_war == 0) {
            CHECK_MESSAGE(x.role1(id, 1) != orc, "e" << id << ": the one-settlement orcs declared their quarrel");
            first_war = id;
        }
        if (at(e, 3) == k_flare_up && first_war == 0) {
            const auto cs = x.causes.find(id);
            if (cs == x.causes.end()) continue;
            for (const std::int64_t tr : cs->second) {
                CHECK_MESSAGE(!(x.kind(tr) == k_claim && x.role1(tr, 1) == orc),
                              "flare-up e" << id << " triggered by the held-back orc claim e" << tr);
            }
        }
    }
    for (const Row& st : t.settlement) {
        if (at(st, 8) == orc) CHECK(at(st, 5) == orc);  // the orcs found nothing (found_rate 0)
    }
    CHECK(orc_claims >= 1);  // the rule had something to hold back
}
#endif

#if UNMAPPED_P3_10_R3_API && UNMAPPED_HAVE_CONFLICT
// GUESS (interim names; adapt at integration): ConflictTuning::quarrel_min_settlements, ConflictRace::hidden_years.
TEST_CASE("P3-06 r17 22.1, 22.2 (seam): ConflictTuning has quarrel_min_settlements 2; ConflictRace hidden_years") {
    namespace h = worldgen::history;
    const h::ConflictTuning t = h::conflict_tuning_from(shipped());
    CHECK(t.quarrel_min_settlements == 2);
    CHECK(t.quarrel_strain == 600);
    h::ConflictRace r;
    r.hidden_years = 100;  // orc
    CHECK(r.hidden_years == 100);
}

TEST_CASE("P3-06 r17 22.7 (seam): check_conflict_tuning's conflict_quarrel_strain_max message") {
    namespace h = worldgen::history;
    h::ConflictTuning t = h::conflict_tuning_from(shipped());
    t.strain_max = 500;
    t.quarrel_strain = 600;
    t.claim_strain = 100;
    t.declare_min = 100;
    t.declare_span = 400;
    CHECK(fatal_of([&] { h::check_conflict_tuning(t); }) == "history: tuning: quarrel_strain must not be above strain_max");
    t.strain_max = 600;
    t.declare_span = 500;
    CHECK(fatal_of([&] { h::check_conflict_tuning(t); }) == "<no error>");
}
#endif

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
