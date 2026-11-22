// Spec P3-06 r22 section 24 (P5-01, generator 0.21.0), tests 24.8, from the rows:
//   24.7  invariants (18) to (21) (the support's invariants_22) on hand-made logs, each once satisfied and once
//         broken: a declaration citing a claim on its own region; a fresh claim (caused by a claim) followed by the
//         war_declared citing it; a claim_dropped caused by one claim, in a tick, never cited afterwards; a
//         refugees_settled of a race with hidden_years >= 1 followed by its refuge_hidden with end_year
//         min(Y + hidden_years, E) - 1 (orc 100: 5990; goblin 300: 5999 at E 6000; hidden_years 0: none);
//         winner_dead <= loser_dead;
//   25.9 / T19  the open claims at E and the refuges still hidden at E from the rows (open_claims_line,
//         hidden_refuges_line);
//   24.5  the toll factors v = 1000 - s + pick(draw(hist.conflict.toll, Y, w, 0, k), 2s + 1) for hand-picked keys
//         (the reference's values, r22_toll_vectors), and the bounds 800 and 1200 at u's high word 0 and 2^32 - 1.
// The rules themselves (the fresh claim at a re-aimed declaration, the strength gate, the drop, the flight's record,
// the factors in a battle) are exercised on hand-made worlds by the reference (check_units_r22: the public seam
// cannot set conflict's strain or claims) and met by the C++ in the golden worlds, where check_r17 also compares the
// r22 lines (counts, open claims and hidden refuges at E) and invariants (18) to (21). Written from the specs before
// the implementation (test-writer rule).

#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p3_02_names_api.hpp"
#include "worldgen/core/hash.hpp"

namespace {

using namespace conflict_test;
using eng::I;
using eng::N;
using eng::T;

Row ev(std::int64_t id, std::int64_t year, std::int64_t kind, std::int64_t region, std::int64_t settlement = 0,
       std::int64_t end_year = -1, std::int64_t phase = 1) {
    return {I(id), I(year), I(end_year < 0 ? year : end_year), I(kind), I(phase), I(region), I(settlement), I(50)};
}
Row part(std::int64_t e, std::int64_t seq, std::int64_t role, std::int64_t type, std::int64_t id) {
    return {I(e), I(seq), I(role), I(type), I(id)};
}
Row cause(std::int64_t e, std::int64_t c) { return {I(e), I(c)}; }
Row val(std::int64_t e, std::int64_t seq, std::int64_t v) { return {I(e), I(seq), I(v)}; }
Row civ_row(std::int64_t id, std::int64_t race, std::int64_t capital, bool ended = false) {
    return {I(id), T("C" + std::to_string(id)), I(race), I(race), I(1), I(capital), I(0), I(1), ended ? I(5000) : N(),
            I(0)};
}
Row stl_row(std::int64_t id, std::int64_t region, std::int64_t owner, std::int64_t status) {
    return {I(id), T("S" + std::to_string(id)), I(id), I(1), I(region), I(owner), I(0), I(1), I(owner), I(2),
            I(status), I(100), N(), I(0)};
}
constexpr std::int64_t CIV = 3, SET = 4;
// phases: 1 an opening (kind 1) over the whole history, 2 a quiet row (kind 2) for the quiet-step case
Row phase(std::int64_t id, std::int64_t kind, std::int64_t a, std::int64_t b) {
    return {I(id), I(1), I(kind), I(a), I(b), I(0)};
}

// c1 goblins (race 2) and c2 elves (race 1); c1's claim e3 on R2 (1100), re-aimed at R3: the fresh claim e4
// (5200) caused by e3 and R3's owner event e2, then the declaration e5 citing e4. `fresh_cites_old` false: the
// declaration cites the old claim e3 instead (breaks (18)).
Tables fresh_log(bool well_formed) {
    Tables t;
    t.civilization = {civ_row(1, 2, 1), civ_row(2, 1, 2)};
    t.settlement = {stl_row(1, 1, 1, 1), stl_row(2, 3, 2, 1)};
    t.phase = {phase(1, 1, 0, 5999)};
    t.event = {ev(1, 0, 3, 1, 1), ev(2, 0, 3, 3, 2), ev(3, 1100, k_claim, 2), ev(4, 5200, k_claim, 3),
               ev(5, 5200, k_war_declared, 3)};
    t.event_participant = {part(1, 1, 1, SET, 1), part(1, 2, 2, CIV, 1), part(2, 1, 1, SET, 2), part(2, 2, 2, CIV, 2),
                           part(3, 1, 1, CIV, 1), part(3, 2, 2, CIV, 2), part(4, 1, 1, CIV, 1), part(4, 2, 2, CIV, 2),
                           part(5, 1, 1, CIV, 1), part(5, 2, 2, CIV, 2)};
    t.event_cause = {cause(3, 1), cause(4, 2), cause(4, 3), cause(5, well_formed ? 4 : 3)};
    return t;
}

const std::map<std::int64_t, std::int64_t> HIDDEN = {{1, 300}, {2, 300}, {6, 100}, {4, 0}};

bool has_problem(const std::vector<std::string>& v, const std::string& prefix) {
    for (const std::string& s : v) {
        if (s.rfind(prefix, 0) == 0) return true;
    }
    return false;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 r22 24.7 (18): a declaration cites a claim on its own region; a fresh claim precedes the declaration") {
    CHECK(invariants_22(fresh_log(true), HIDDEN, 6000) == std::vector<std::string>{});
    const std::vector<std::string> bad = invariants_22(fresh_log(false), HIDDEN, 6000);
    CHECK(has_problem(bad, "(18) e5: its claim e3 differs"));          // the old claim is on R2, the war on R3
    CHECK(has_problem(bad, "(18) fresh claim e4 is not followed"));    // e5 does not cite e4
    // a declaration with two claim causes
    Tables two = fresh_log(true);
    two.event_cause.push_back(cause(5, 3));
    CHECK(has_problem(invariants_22(two, HIDDEN, 6000), "(18) e5: 2 claim causes"));
    // a fresh claim of kind 39 reads as a claim (23.2): e4 a claim_settlements listing s2
    Tables k39 = fresh_log(true);
    k39.event[3] = ev(4, 5200, k_claim_settlements, 3);
    k39.event_participant.push_back(part(4, 3, 3, SET, 2));
    CHECK(invariants_22(k39, HIDDEN, 6000) == std::vector<std::string>{});
    // the fresh claim in another year than its declaration
    Tables late = fresh_log(true);
    late.event[3] = ev(4, 5199, k_claim, 3);
    CHECK(has_problem(invariants_22(late, HIDDEN, 6000), "(18) fresh claim e4"));
}

TEST_CASE("P3-06 r22 24.7 (19): claim_dropped is caused by its claim, in a tick, and nothing cites the claim after") {
    Tables t;
    t.civilization = {civ_row(1, 2, 1), civ_row(2, 1, 2)};
    t.settlement = {stl_row(1, 1, 1, 1), stl_row(2, 3, 2, 1)};
    t.phase = {phase(1, 1, 0, 5000), phase(2, 2, 5001, 5999)};
    t.event = {ev(1, 0, 3, 1, 1), ev(2, 0, 3, 3, 2), ev(3, 1100, k_claim, 3), ev(4, 1200, k_claim_dropped, 3)};
    t.event_participant = {part(1, 1, 1, SET, 1), part(1, 2, 2, CIV, 1), part(2, 1, 1, SET, 2), part(2, 2, 2, CIV, 2),
                           part(3, 1, 1, CIV, 1), part(3, 2, 2, CIV, 2), part(4, 1, 1, CIV, 1), part(4, 2, 2, CIV, 2)};
    t.event_cause = {cause(3, 2), cause(4, 3)};
    CHECK(invariants_22(t, HIDDEN, 6000) == std::vector<std::string>{});
    CHECK(open_claims_line(t) == "-");   // dropped: no standing claim
    // the same claim not dropped stands at E
    Tables open = t;
    open.event.pop_back();
    open.event_participant.resize(6);
    open.event_cause.pop_back();
    CHECK(open_claims_line(open) == "e3");
    // ... unless a side has ended
    Tables ended = open;
    ended.civilization[1] = civ_row(2, 1, 2, true);
    CHECK(open_claims_line(ended) == "-");
    // broken: dated in a quiet step; another region; a later event citing the claim
    Tables quiet = t;
    quiet.event[3] = ev(4, 5100, k_claim_dropped, 3, 0, -1, 2);
    CHECK(has_problem(invariants_22(quiet, HIDDEN, 6000), "(19) e4: dated in a quiet step"));
    Tables other = t;
    other.event[3] = ev(4, 1200, k_claim_dropped, 1);
    CHECK(has_problem(invariants_22(other, HIDDEN, 6000), "(19) e4: its claim e3 differs"));
    Tables cited = t;
    cited.event.push_back(ev(5, 1300, k_war_declared, 3));
    cited.event_participant.push_back(part(5, 1, 1, CIV, 1));
    cited.event_participant.push_back(part(5, 2, 2, CIV, 2));
    cited.event_cause.push_back(cause(5, 3));
    const std::vector<std::string> bad = invariants_22(cited, HIDDEN, 6000);
    CHECK(has_problem(bad, "(19) e4: its claim is cited by e5"));
    CHECK(has_problem(bad, "(19) e4: its claim is cited afterwards by e5"));
}

TEST_CASE("P3-06 r22 24.4, 24.7 (20): refuge_hidden after every flight of a race with hidden_years; spans cut at E") {
    // a flight in 5891 at E 6000: the refuge s2 of c2 in R4, from s1; refuge_hidden e3 right after
    auto flight = [](std::int64_t race, std::int64_t end_year, bool with42) {
        Tables t;
        t.civilization = {civ_row(1, 1, 1), civ_row(2, race, 2)};
        t.settlement = {stl_row(1, 3, 1, 1), stl_row(2, 4, 2, 1)};
        t.phase = {phase(1, 1, 0, 5999)};
        t.event = {ev(1, 5891, 25, 3, 1), ev(2, 5891, k_refugees_settled, 4, 2)};
        t.event_participant = {part(1, 1, 1, CIV, 1), part(1, 2, 2, CIV, 2), part(2, 1, 1, SET, 2),
                               part(2, 2, 2, CIV, 2), part(2, 3, 3, SET, 1)};
        t.event_cause = {cause(2, 1)};
        if (with42) {
            t.event.push_back(ev(3, 5891, k_refuge_hidden, 4, 2, end_year));
            t.event_participant.push_back(part(3, 1, 1, SET, 2));
            t.event_participant.push_back(part(3, 2, 2, CIV, 2));
            t.event_cause.push_back(cause(3, 2));
        }
        return t;
    };
    CHECK(invariants_22(flight(6, 5990, true), HIDDEN, 6000) == std::vector<std::string>{});   // orc: 100 years
    CHECK(invariants_22(flight(2, 5999, true), HIDDEN, 6000) == std::vector<std::string>{});   // goblin: cut at E
    CHECK(invariants_22(flight(4, 0, false), HIDDEN, 6000) == std::vector<std::string>{});     // hidden_years 0
    CHECK(has_problem(invariants_22(flight(6, 5991, true), HIDDEN, 6000), "(20) e2"));                // wrong end_year
    CHECK(has_problem(invariants_22(flight(2, 0, false), HIDDEN, 6000), "(20) e2"));                  // missing
    CHECK(has_problem(invariants_22(flight(4, 5999, true), HIDDEN, 6000), "(20) e2: a refuge_hidden for a race with"));
    // P3-07 r19 25.9: the goblin refuge is still hidden in E - 1, the orc one was found in 5991
    CHECK(hidden_refuges_line(flight(2, 5999, true), 6000) == "s2");
    CHECK(hidden_refuges_line(flight(6, 5990, true), 6000) == "-");
    // a ruined refuge is not listed
    Tables ruined = flight(2, 5999, true);
    ruined.settlement[1] = stl_row(2, 4, 2, 3);
    CHECK(hidden_refuges_line(ruined, 6000) == "-");
}

TEST_CASE("P3-06 r22 24.7 (21): winner_dead <= loser_dead in every battle") {
    Tables t;
    t.civilization = {civ_row(1, 1, 1), civ_row(2, 2, 2)};
    t.phase = {phase(1, 1, 0, 5999)};
    t.event = {ev(1, 1000, k_battle, 3), ev(2, 1001, k_battle_at_pass, 3)};
    t.event_participant = {part(1, 1, 1, CIV, 1), part(1, 2, 2, CIV, 2), part(2, 1, 1, CIV, 1), part(2, 2, 2, CIV, 2)};
    t.event_value = {val(1, 1, 96), val(1, 2, 200), val(2, 1, 50), val(2, 2, 50), val(2, 3, 1)};
    CHECK(invariants_22(t, HIDDEN, 6000) == std::vector<std::string>{});
    t.event_value[2] = val(2, 1, 51);
    CHECK(has_problem(invariants_22(t, HIDDEN, 6000), "(21) e2"));
}

TEST_CASE("P3-06 r22 24.5, 24.8 item 5: the toll factors of hand-picked keys (s = 200) and the bounds") {
    struct V {
        std::uint64_t seed, year, war, u0, u1;
        std::int64_t vl, vw;
    };
    // the reference's values (p3_conflict_reference.py r22_toll_vectors): u = draw(stream_key(seed,
    // "hist.conflict.toll"), Y, w, 0, k), v = 800 + pick(u, 401)
    const std::vector<V> want = {
        {0, 1051, 3, 6731837152786326516ULL, 17686602317554875411ULL, 946, 1184},
        {42, 5836, 412, 2082948192982989877ULL, 14012500397234035673ULL, 845, 1104},
        {42, 5837, 412, 15636079376857031826ULL, 17403726049988704225ULL, 1139, 1178},
        {7, 2000, 1, 12563321675012032690ULL, 778147891883362258ULL, 1073, 816},
        {1, 1, 18446744073709551615ULL, 13623483423206103183ULL, 12013273116736706943ULL, 1096, 1061},
    };
    for (const V& v : want) {
        CAPTURE(v.seed);
        CAPTURE(v.year);
        const std::uint64_t key = worldgen::stream_key(v.seed, "hist.conflict.toll");
        const std::uint64_t u0 = names_api::draw(key, v.year, v.war, 0, 0);
        const std::uint64_t u1 = names_api::draw(key, v.year, v.war, 0, 1);
        CHECK(u0 == v.u0);
        CHECK(u1 == v.u1);
        CHECK(800 + static_cast<std::int64_t>(names_api::pick(u0, 401)) == v.vl);
        CHECK(800 + static_cast<std::int64_t>(names_api::pick(u1, 401)) == v.vw);
    }
    // the bounds: u's high word 0 gives 1000 - s, 2^32 - 1 gives 1000 + s
    CHECK(names_api::pick(0x00000000FFFFFFFFULL, 401) == 0U);
    CHECK(names_api::pick(0xFFFFFFFF00000000ULL, 401) == 400U);
    // the toll of the shipped rule at the factor bounds keeps the victors below the defeated (conflict_toll_spread):
    // F_W = F_L = 1000: dead_W at most 80 x 1200 / 1000 = 96, dead_L at least 250 x 800 / 1000 = 200
    CHECK(1000 * 80 / 1000 * 1200 / 1000 <= 1000 * 250 / 1000 * 800 / 1000);
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
