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

#endif  // UNMAPPED_HAVE_PEOPLES
