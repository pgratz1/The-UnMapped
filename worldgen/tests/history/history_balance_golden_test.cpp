// Spec P3-06 r9 20.14 item 8 (P3-09, generator 0.18.0): the test writer's extra golden case
// hist-conflict-flight (seed 1 at 512, default settings), a world in which last refuges fall into flight and
// their hidden refuges are found again before the present. Generated with the shipped stages and compared
// with worldgen/tests/golden/p3-06-conflict.txt (the reference p3_conflict_reference.py): hash --detail's
// thirteen table lines, [history], [stats] (with P3-07 r8 21.5's six keys, from the rows), the per-war lines
// re-derived from the rows (outcomes with -ceded and -tribute), every rendered sentence (kinds 32 and 33
// with their templates), the invariants of P3-06 14 and r9 20.13 from the rows, threads 1 and 4
// byte-identical; and that the case does what it is for: at least one refugees_settled whose refuge is found
// (its year + hidden_years before the present) and a tribute.
//
// P3-06 r13 20.27 item 8 (the last test): invariant (13) on 2048 seed 3, default settings, the case of the P3-09
// validation's D1, in the doctest suite "history-slow" (worldgen-tests -ts=history-slow --no-skip). Invariant (13)
// on the synthetic, stress and golden worlds is checked with the r9 invariants (check_invariants_09).
//
// Written from the spec only; needs no new library name. Runs by default (one 512 world, two threads).

#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "worldgen/inspect/report.hpp"

namespace {

using namespace conflict_test;

std::string flight_world(std::uint32_t threads, const std::string& until = "") {
    const CCase& g = conflict_case("hist-conflict-flight");
    return chrono_test::world(chrono_test::WorldKey{g.seed, "", g.size, g.span, g.races, until, threads});
}

std::string section(const std::string& text, const std::string& header) {
    const std::size_t at = text.find(header + "\n");
    if (at == std::string::npos) return {};
    const std::size_t end = text.find("\n[", at + 1);
    return text.substr(at, end == std::string::npos ? std::string::npos : end + 1 - at);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 r9 20.14 item 8: hist-conflict-flight equals the reference; flights, found refuges, tributes") {
    const CCase& g = conflict_case("hist-conflict-flight");
    REQUIRE(g.kind == "world");
    CHECK(g.size == 512);
    const db::WorldReader part = open_world(flight_world(1, "chronology"));
    const eng::HistoryInputs in = eng::inputs_from_world(part, shipped());
    const db::WorldReader r = open_world(flight_world(1));
    const Tables t = tables_of_world(r);
    CHECK(eng::history_stage(shipped(), in) == t);  // P3-04 Y2: resumed equals complete
    const std::map<std::string, std::string> detail = chrono_test::detail_tables(r);
    REQUIRE(g.tables.size() == 13U);
    for (const auto& [name, hex] : g.tables) {
        CAPTURE(name);
        CHECK(detail.at(name) == hex);
    }
    const auto rep = worldgen::inspect::build_report(r);
    REQUIRE(rep.ok());
    std::string want = "[history]\n";
    for (const std::string& l : g.report) want += l + "\n";
    CHECK(section(rep.value(), "[history]") == want);
    CHECK(stats_lines_06(t, in) == g.stats);
    CHECK(war_lines_from_rows(t) == golden_wars_without_how(g));
    const std::vector<std::string> sent = eng::render_all(r, static_cast<std::int64_t>(t.event.size()));
    CHECK(g.sentences.size() == sent.size());
    for (const auto& [id, text] : g.sentences) {
        CAPTURE(id);
        REQUIRE(static_cast<std::size_t>(id) <= sent.size());
        CHECK(sent[static_cast<std::size_t>(id - 1)] == text);
    }
    check_invariants_06(t, in, 25, true);
    check_invariants_09(t, in, shipped());
    check_invariants_10(t, shipped());
    CHECK(invariants_19(t, shipped()) == std::vector<std::string>{});  // P3-06 r14 (14), (15), 21.7 (0.19.0)
    // what the case is for
    const Index x(t);
    const std::int64_t hidden_years = conflict_int(shipped(), "hidden_years");
    std::int64_t flights = 0, found = 0, tributes = 0;
    for (const Row& e : t.event) {
        if (at(e, 3) == k_refugees_settled) {
            ++flights;
            found += at(e, 1) + hidden_years < g.span ? 1 : 0;
            // the sentence of kind 32 names the origin, the refuge and its region
            const std::string& s = sent[static_cast<std::size_t>(at(e, 0) - 1)];
            CHECK(s.rfind("The survivors of ", 0) == 0);
            CHECK(s.find(" fled into hiding and founded ") != std::string::npos);
        }
        tributes += at(e, 3) == k_tribute ? 1 : 0;
    }
    CHECK(flights >= 1);
    CHECK(found >= 1);
    CHECK(tributes >= 1);
    // threads: byte-identical tables
    CHECK(tables_of_world(open_world(flight_world(4))) == t);
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-06 r13 20.27 items 7, 8: 2048 seed 3, the colony s240 passes to c10 with the region_ceded e1142") {
    // 20.27 item 7: the log is unchanged up to and including e1142 (the transfer writes no event): the colony s240
    // of c15 in region 158 (e1138), c15's war_won lost to c10 (e1141) and the cession of 158 from c15 to c10 (e1142),
    // all in the quiet step of 5234. Under r13 s240 passes to c10 with e1142: its census row of 5234 has owner c10.
    const std::string path = chrono_test::world(chrono_test::WorldKey{3, "", 2048, 6000, "all", "", 1});
    const db::WorldReader r = open_world(path);
    const Tables t = tables_of_world(r);
    const Index x(t);
    REQUIRE(x.ev.count(1142) == 1U);
    CHECK(x.kind(1138) == k_colony_founded);
    CHECK(at(*x.ev.at(1138), 6) == 240);
    CHECK(at(*x.ev.at(1138), 5) == 158);
    CHECK(x.role1(1138, 2) == 15);
    CHECK(x.kind(1141) == k_war_won);
    CHECK(x.role1(1141, 1) == 10);
    CHECK(x.role1(1141, 2) == 15);
    CHECK(x.kind(1142) == k_region_ceded);
    CHECK(at(*x.ev.at(1142), 1) == 5234);
    CHECK(at(*x.ev.at(1142), 5) == 158);
    CHECK(x.role1(1142, 1) == 15);
    CHECK(x.role1(1142, 2) == 10);
    REQUIRE(x.causes.count(1142) == 1U);
    CHECK(x.causes.at(1142) == std::vector<std::int64_t>{1141});
    // s240 keeps c15 as founder and e1138 as founding event; its census row of 5234 is c10's
    const Row& s = *x.st.at(240);
    CHECK(at(s, 5) == 15);
    CHECK(at(s, 7) == 1138);
    std::int64_t owner_5234 = -1;
    for (const Row& c : t.settlement_census) {
        if (at(c, 0) == 240 && at(c, 1) == 5234) owner_5234 = at(c, 2);
    }
    CHECK(owner_5234 == 10);
    // c10 holds 158 from 5234 with e1142, as before
    bool row = false;
    for (const Row& rc : t.region_control) row = row || (at(rc, 0) == 158 && at(rc, 1) == 5234 && at(rc, 3) == 10 && at(rc, 4) == 1142);
    CHECK(row);
    // invariant (13), with 14 item 5 (a region's owner has a standing settlement in it) through the replay
    CHECK(cession_violations(t) == std::vector<std::string>{});
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
