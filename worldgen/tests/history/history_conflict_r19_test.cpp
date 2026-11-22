// Spec P3-06 r19 section 23 (P3-11, generator 0.20.0), tests 23.7, with P3-07 r14 23 items 3 to 5:
//   23.7 item 2  the foothold cession is one settlements_ceded (from L, to W, the passing settlements in
//                ascending id, a hidden one included), no region_ceded, no region_control row; the war line
//                gains -foothold;
//   23.7 item 3  invariants (13) and (17) read kind 40 as the cession of their text, (15) reads kind 39 as a
//                claim; every settlement participant of kinds 39 and 40 lies in the event's region and is owned
//                at the event by the holder (39) or the from side (40);
//   P3-07 23 items 3, 4  the declaration's note names the claim's region when it differs (same year and
//                earlier, with and without an earlier war between the pair); the flare-up note of a kind 39
//                claim with one and three settlements, one named "The ...".
// The rows are hand-made (the public seam cannot set conflict's strain or claims, so 23.7 item 1's hand-made
// claim worlds are the reference's check_units_r19; the C++ meets kind 39 in the golden worlds, where
// check_r17 also compares the r19 line: T11, the kind 39 claims by holder, kind 40 and the war goals). The
// replay is the support's invariants_r17 and cession_violations. Written from the specs before the
// implementation (test-writer rule).

#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "worldgen/db/testing.hpp"

#if __has_include("worldgen/history/digest.hpp")
#include "worldgen/history/digest.hpp"
#define UNMAPPED_P3_11_DIGEST 1
#else
#define UNMAPPED_P3_11_DIGEST 0
#endif

namespace {

using namespace conflict_test;
using eng::I;
using eng::N;
using eng::T;

Row ev(std::int64_t id, std::int64_t year, std::int64_t kind, std::int64_t region, std::int64_t settlement = 0) {
    return {I(id), I(year), I(year), I(kind), I(1), I(region), I(settlement), I(50)};
}
Row part(std::int64_t e, std::int64_t seq, std::int64_t role, std::int64_t type, std::int64_t id) {
    return {I(e), I(seq), I(role), I(type), I(id)};
}
Row cause(std::int64_t e, std::int64_t c) { return {I(e), I(c)}; }
Row civ_row(std::int64_t id, std::int64_t race, std::int64_t capital) {
    return {I(id), T("C" + std::to_string(id)), I(race), I(race), I(1), I(capital), I(0), I(1), N(), I(0)};
}
Row stl_row(std::int64_t id, std::int64_t region, std::int64_t founder, std::int64_t event, std::int64_t owner,
            std::int64_t status) {
    return {I(id), T("S" + std::to_string(id)), I(id), I(1), I(region), I(founder), I(0), I(event), I(owner), I(2),
            I(status), I(100), N(), I(0)};
}
Row cen(std::int64_t s, std::int64_t y, std::int64_t owner) { return {I(s), I(y), I(owner), I(2), I(1), I(100)}; }
constexpr std::int64_t CIV = 3, SET = 4;

std::size_t count_of(const std::vector<std::string>& v, const char* prefix) {
    return static_cast<std::size_t>(
        std::count_if(v.begin(), v.end(), [&](const std::string& s) { return s.rfind(prefix, 0) == 0; }));
}

// c1 goblins (s1 in R1) attack c2 elves (capital s2 in R4) over R5, which c1 holds (s3, from 1100); c2 keeps the
// footholds s4 and s5 in R5. War e10 (1200), battle e11 won by c1 (1205), war_won e13 by c1 in 1210, and the
// foothold cession e14 of kind 40 listing `listed` (from c2, to c1), caused by e13; every listed settlement passes.
// The war cites c1's kind 39 claim e6 (1150) on s4 and s5.
Tables foothold_log(const std::vector<std::int64_t>& listed, bool region_row = false) {
    Tables t;
    t.civilization = {civ_row(1, 2, 1), civ_row(2, 1, 2)};
    t.event = {ev(1, 0, k_settlement_founded, 1, 1), ev(2, 0, k_settlement_founded, 4, 2),
               ev(3, 1000, k_settlement_founded, 5, 4), ev(4, 1001, k_settlement_founded, 5, 5),
               ev(5, 1100, k_settlement_founded, 5, 3)};
    // the casus belli: c1's kind 39 claim on c2's s4 and s5 in R5, which c1 holds (P3-06 r19 23.2)
    t.event.push_back(ev(6, 1150, k_claim_settlements, 5));
    t.event_participant = {part(6, 1, 1, CIV, 1), part(6, 2, 2, CIV, 2), part(6, 3, 3, SET, 4), part(6, 4, 3, SET, 5)};
    t.event_cause.push_back(cause(6, 3));
    t.event.push_back(ev(10, 1200, k_war_declared, 5));
    t.event_participant.push_back(part(10, 1, 1, CIV, 1));
    t.event_participant.push_back(part(10, 2, 2, CIV, 2));
    t.event_cause.push_back(cause(10, 6));
    t.event.push_back(ev(11, 1205, k_battle, 5));
    t.event_participant.push_back(part(11, 1, 1, CIV, 1));
    t.event_participant.push_back(part(11, 2, 2, CIV, 2));
    t.event_cause.push_back(cause(11, 10));
    t.event.push_back(ev(13, 1210, k_war_won, 0));
    t.event_participant.push_back(part(13, 1, 1, CIV, 1));
    t.event_participant.push_back(part(13, 2, 2, CIV, 2));
    t.event_cause.push_back(cause(13, 11));
    t.event_value.push_back({I(13), I(1), I(10)});
    t.event.push_back(ev(14, 1210, k_settlements_ceded, 5));
    t.event_participant.push_back(part(14, 1, 1, CIV, 2));
    t.event_participant.push_back(part(14, 2, 2, CIV, 1));
    for (std::size_t i = 0; i < listed.size(); ++i) {
        t.event_participant.push_back(part(14, static_cast<std::int64_t>(3 + i), 3, SET, listed[i]));
    }
    t.event_cause.push_back(cause(14, 13));
    t.relation = {{I(3), I(1), I(3), I(2), T("war"), I(1200), I(1210), I(10), I(13)}};
    t.region_control = {{I(1), I(0), N(), I(1), I(1)}, {I(4), I(0), N(), I(2), I(2)}, {I(5), I(1100), N(), I(1), I(5)}};
    if (region_row) t.region_control.push_back({I(5), I(1210), N(), I(1), I(14)});
    t.settlement = {stl_row(1, 1, 1, 1, 1, 1), stl_row(2, 4, 2, 2, 2, 1), stl_row(3, 5, 1, 5, 1, 1),
                    stl_row(4, 5, 2, 3, 1, 1), stl_row(5, 5, 2, 4, 1, 1)};
    t.settlement_census = {cen(1, 0, 1), cen(2, 0, 2), cen(3, 1100, 1), cen(4, 1000, 2), cen(5, 1001, 2),
                           cen(4, 1210, 1), cen(5, 1210, 1)};
    return t;
}

// A claim of kind 39 by c1 on c2's settlements in R5 (held by c1), listing `listed`, in 1150 (before the war).
Tables claim_log(const std::vector<std::int64_t>& listed, std::int64_t holder_of_r5) {
    Tables t;
    t.civilization = {civ_row(1, 2, 1), civ_row(2, 1, 2), civ_row(3, 5, 6)};
    t.event = {ev(1, 0, k_settlement_founded, 1, 1), ev(2, 0, k_settlement_founded, 4, 2),
               ev(3, 1000, k_settlement_founded, 5, 4), ev(4, 1001, k_settlement_founded, 5, 5),
               ev(5, 1100, k_settlement_founded, 5, 3), ev(6, 0, k_settlement_founded, 6, 6)};
    t.event.push_back(ev(7, 1150, k_claim_settlements, 5));
    t.event_participant = {part(7, 1, 1, CIV, 1), part(7, 2, 2, CIV, 2)};
    for (std::size_t i = 0; i < listed.size(); ++i) {
        t.event_participant.push_back(part(7, static_cast<std::int64_t>(3 + i), 3, SET, listed[i]));
    }
    t.event_cause.push_back(cause(7, 3));
    t.region_control = {{I(1), I(0), N(), I(1), I(1)}, {I(4), I(0), N(), I(2), I(2)}, {I(6), I(0), N(), I(3), I(6)}};
    if (holder_of_r5 != 0) t.region_control.push_back({I(5), I(1100), N(), I(holder_of_r5), I(5)});
    t.settlement = {stl_row(1, 1, 1, 1, 1, 1), stl_row(2, 4, 2, 2, 2, 1), stl_row(3, 5, 1, 5, 1, 1),
                    stl_row(4, 5, 2, 3, 2, 1), stl_row(5, 5, 2, 4, 2, 1), stl_row(6, 6, 3, 6, 3, 1)};
    return t;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 r19 23.3, 23.7 items 2, 3: a foothold cession of kind 40 passes its listed settlements; (13), (17) hold") {
    const wd::ContentSet& c = shipped();
    const R17 r = invariants_r17(foothold_log({4, 5}), c);
    CHECK(r.problems == std::vector<std::string>{});
    CHECK(r.foothold_cessions == 1);
    CHECK(cession_violations(foothold_log({4, 5})) == std::vector<std::string>{});
    // a listing that leaves out a passing settlement, or names one in the wrong order, is found
    CHECK(count_of(invariants_r17(foothold_log({4}), c).problems, "(23.3)") == 1U);
    CHECK(count_of(invariants_r17(foothold_log({5, 4}), c).problems, "(23.3)") == 1U);
    // a kind 40 event that opened a region_control row is no foothold cession
    CHECK(count_of(invariants_r17(foothold_log({4, 5}, true), c).problems, "(23.3)") >= 1U);
}

TEST_CASE("P3-06 r19 23.3: the war line of a foothold cession ends in -foothold, a kind 29 cession in -ceded") {
    const std::vector<std::string> lines = war_lines_from_rows(foothold_log({4, 5}));
    REQUIRE(lines.size() == 1U);
    CHECK(lines[0].find("won-c1-foothold") != std::string::npos);
    CHECK(lines[0].find("-ceded") == std::string::npos);
}

TEST_CASE("P3-06 r19 23.2, 23.7 item 3: the settlements of a kind 39 claim are the holder's, in its region, ascending") {
    const wd::ContentSet& c = shipped();
    // R5 held by the claimant c1, a third civilization c3 or no one: c2's s4 and s5 there
    for (const std::int64_t holder : {1, 3, 0}) {
        CAPTURE(holder);
        CHECK(invariants_r17(claim_log({4, 5}, holder), c).problems == std::vector<std::string>{});
    }
    // s3 is c1's (not the holder c2's); s6 is in R6; out of order; held by c2 itself (then kind 20 is the claim)
    CHECK(count_of(invariants_r17(claim_log({3, 4}, 1), c).problems, "(23.7.3)") == 1U);
    CHECK(count_of(invariants_r17(claim_log({4, 6}, 1), c).problems, "(23.7.3)") == 1U);
    CHECK(count_of(invariants_r17(claim_log({5, 4}, 1), c).problems, "(23.7.3)") == 1U);
    CHECK(count_of(invariants_r17(claim_log({4, 5}, 2), c).problems, "(23.2)") == 1U);
}

TEST_CASE("P3-06 r19 23.1-23.4: the r19 line from hand-made rows (claims by holder, kind 40, no orcs)") {
    CHECK(r19_line(claim_log({4, 5}, 1), 6000) == "t11 - first_orc_war - claims39 1/0/0 cessions40 0 goal_differs 0");
    CHECK(r19_line(claim_log({4, 5}, 3), 6000) == "t11 - first_orc_war - claims39 0/1/0 cessions40 0 goal_differs 0");
    CHECK(r19_line(claim_log({4, 5}, 0), 6000) == "t11 - first_orc_war - claims39 0/0/1 cessions40 0 goal_differs 0");
    CHECK(r19_line(foothold_log({4, 5}), 6000) == "t11 - first_orc_war - claims39 1/0/0 cessions40 1 goal_differs 0");
}

}  // TEST_SUITE

#if UNMAPPED_HAVE_CONFLICT && UNMAPPED_P3_11_DIGEST

namespace {

// A copy of the 512 world geo-seed-42-512 (conflict golden case) with hand-made events appended in 5990-5999:
// civilizations 1 and 2 named Clan A'thuukh and The Kingdom of Sunt, settlements 1-3 named Drutnot Hole, Nutskrank
// and The Grey Fort, regions 1 and 2 named East Wood and Woodward's Shaw. Returns the event ids in order.
struct NoteWorld {
    std::string path;
    std::vector<std::int64_t> ids;
};

struct NEv {
    std::string kind;
    std::int64_t year, region;
    std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t>> parts;  // (role seq, type, id)
    std::vector<std::int64_t> causes;  // indices into the appended events (0-based), or -1: the world's event 1
    std::vector<std::int64_t> vals;
};

NoteWorld note_world(const std::string& file, const std::vector<NEv>& evs) {
    static dbtest::TempDir dir("conflict-r19");
    const CCase& g = conflict_case("geo-seed-42-512");
    const std::string src = chrono_test::world(chrono_test::WorldKey{g.seed, "", g.size, g.span, g.races, "", 1});
    const db::WorldReader base = open_world(src);
    const Tables bt = tables_of_world(base);
    std::int64_t eid = static_cast<std::int64_t>(bt.event.size());
    const std::int64_t phase = static_cast<std::int64_t>(bt.phase.size());
    std::map<std::string, std::int64_t> kid;
    for (const Row& k : bt.event_kind) kid[k.at(1).bytes] = at(k, 0);
    std::string sql =
        "UPDATE civilization SET name = 'Clan A''thuukh' WHERE id = 1;"
        "UPDATE civilization SET name = 'The Kingdom of Sunt' WHERE id = 2;"
        "UPDATE settlement SET name = 'Drutnot Hole' WHERE id = 1;"
        "UPDATE settlement SET name = 'Nutskrank' WHERE id = 2;"
        "UPDATE settlement SET name = 'The Grey Fort' WHERE id = 3;"
        "UPDATE region SET name = 'East Wood' WHERE id = 1;"
        "UPDATE region SET name = 'Woodward''s Shaw' WHERE id = 2;";
    NoteWorld out;
    for (const NEv& e : evs) {
        REQUIRE(kid.count(e.kind) == 1U);
        const std::string id = std::to_string(++eid);
        out.ids.push_back(eid);
        sql += "INSERT INTO event VALUES (" + id + ", " + std::to_string(e.year) + ", " + std::to_string(e.year) + ", " +
               std::to_string(kid.at(e.kind)) + ", " + std::to_string(phase) + ", " + std::to_string(e.region) +
               ", 0, 50);";
        for (std::size_t p = 0; p < e.parts.size(); ++p) {
            sql += "INSERT INTO event_participant VALUES (" + id + ", " + std::to_string(p + 1) + ", " +
                   std::to_string(std::get<0>(e.parts[p])) + ", " + std::to_string(std::get<1>(e.parts[p])) + ", " +
                   std::to_string(std::get<2>(e.parts[p])) + ");";
        }
        for (const std::int64_t c : e.causes) {
            sql += "INSERT INTO event_cause VALUES (" + id + ", " + std::to_string(c < 0 ? 1 : out.ids.at(static_cast<std::size_t>(c))) + ");";
        }
        for (std::size_t v = 0; v < e.vals.size(); ++v) {
            sql += "INSERT INTO event_value VALUES (" + id + ", " + std::to_string(v + 1) + ", " +
                   std::to_string(e.vals[v]) + ");";
        }
    }
    out.path = dir.file(file);
    dbtest::write_file(out.path, dbtest::read_file(src));
    const auto s = worldgen::db::testing::execute_on_file(out.path, sql);
    REQUIRE_MESSAGE(s.ok(), "SQL rejected");
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-07 r14 23 items 3, 5: the declaration's note names the claim's region when it differs") {
    using P = std::tuple<std::int64_t, std::int64_t, std::int64_t>;
    const std::int64_t CV = 3, ST = 4;
    // 0: a kind 39 claim on Woodward's Shaw (5990); 1: a declaration over East Wood citing it (5995);
    // 2: a claim (kind 20) on Woodward's Shaw in 5996 and 3: a declaration over East Wood the same year citing it;
    // 4: a declaration over Woodward's Shaw citing claim 2 (same region): the old note; the pair's third war
    const std::vector<NEv> evs = {
        {"claim_settlements", 5990, 2, {P{1, CV, 1}, P{2, CV, 2}, P{3, ST, 2}}, {-1}, {}},
        {"war_declared", 5995, 1, {P{1, CV, 1}, P{2, CV, 2}}, {0}, {}},
        {"claim", 5996, 2, {P{1, CV, 1}, P{2, CV, 2}}, {-1}, {}},
        {"war_declared", 5996, 1, {P{1, CV, 1}, P{2, CV, 2}}, {2}, {}},
        {"war_declared", 5997, 2, {P{1, CV, 1}, P{2, CV, 2}}, {2}, {}},
    };
    const NoteWorld w = note_world("decl.db", evs);
    const db::WorldReader r = open_world(w.path);
    const auto view = worldgen::history::HistoryView::load(r);
    REQUIRE(view.ok());
    // n counts the declarations between the pair before each one in the whole log
    const Tables t = tables_of_world(r);
    const Index x(t);
    auto nth = [&](std::int64_t id) {
        std::int64_t n = 1;
        for (const Row& e : t.event) {
            if (at(e, 3) != k_war_declared || at(e, 0) >= id) continue;
            std::vector<std::int64_t> a = {x.role1(at(e, 0), 1), x.role1(at(e, 0), 2)}, b = {1, 2};
            std::sort(a.begin(), a.end());
            n += a == b ? 1 : 0;
        }
        return n;
    };
    auto tail = [&](std::int64_t id) {
        const std::int64_t n = nth(id);
        return n >= 2 ? " The " + worldgen::history::history_ordinal(n) + " war between them." : std::string();
    };
    CHECK(worldgen::history::history_note(view.value(), w.ids[1]) ==
          "The claim, since 5990, was to Woodward's Shaw." + tail(w.ids[1]));
    CHECK(worldgen::history::history_note(view.value(), w.ids[3]) ==
          "The claim, made the same year, was to Woodward's Shaw." + tail(w.ids[3]));
    CHECK(worldgen::history::history_note(view.value(), w.ids[4]) == "Claimed since 5996." + tail(w.ids[4]));
    CHECK(nth(w.ids[4]) >= 3);  // the pair's third (or later) war: the ordinal follows the new form too
}

TEST_CASE("P3-07 r14 23 items 4, 5: the flare-up note of a kind 39 claim, with one and three settlements") {
    using P = std::tuple<std::int64_t, std::int64_t, std::int64_t>;
    const std::int64_t CV = 3, ST = 4;
    const std::vector<NEv> evs = {
        {"claim_settlements", 5990, 2, {P{1, CV, 1}, P{2, CV, 2}, P{3, ST, 1}}, {-1}, {}},
        {"flare_up", 5991, 0, {}, {0}, {10}},
        {"claim_settlements", 5992, 2, {P{1, CV, 2}, P{2, CV, 1}, P{3, ST, 1}, P{3, ST, 2}, P{3, ST, 3}}, {-1}, {}},
        {"flare_up", 5993, 0, {}, {2}, {10}},
    };
    const NoteWorld w = note_world("flare.db", evs);
    const db::WorldReader r = open_world(w.path);
    const auto view = worldgen::history::HistoryView::load(r);
    REQUIRE(view.ok());
    CHECK(worldgen::history::history_note(view.value(), w.ids[1]) ==
          "The claim of Clan A'thuukh to Drutnot Hole of the Kingdom of Sunt in Woodward's Shaw came to a head.");
    CHECK(worldgen::history::history_note(view.value(), w.ids[3]) ==
          "The claim of the Kingdom of Sunt to Drutnot Hole, Nutskrank and the Grey Fort of Clan A'thuukh in "
          "Woodward's Shaw came to a head.");
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_CONFLICT && UNMAPPED_P3_11_DIGEST

#endif  // UNMAPPED_HAVE_PEOPLES
