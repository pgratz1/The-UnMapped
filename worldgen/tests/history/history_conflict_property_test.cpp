// Spec P3-06 14 and 16.2 item 15: the shipped history stage (peoples and
// conflict, generator 0.16.0) on the synthetic geographies of the golden
// file (P3-05's make_synthetic: 2 to 60 regions, islets, barriers, passes,
// river borders; races all, elf,goblin, elf,orc, dwarf,faerie; civs auto
// and 4; intensities normal, calm, turbulent; spans 2000 and 6000) and on
// the stress cases (a conflict tuning in which sieges, sacks, captures,
// conquests, cessions, truces, flare-ups, battles at passes, last_refuge and
// the shelter all happen): the thirteen tables equal the reference's (table
// hashes), the [history] lines and the [stats] keys of 15.4, the per-war
// lines of the golden file re-derived from the rows (declaration, sides,
// goal, casus belli, battles, falls, outcome), and invariants 1-5, 7, 8 of
// 14 (with P3-05's, through the P3-05 helper where they still hold)
// re-derived from the rows: relations symmetric for enmity and peace and
// asymmetric for war, a war always leaves a war row and never ends in its
// declaration year (7.3, N5), a pair's war and peace rows never overlap,
// falls caused by fallen sieges after won battles, conquests, territory,
// every race alive (last_refuge), flare-ups triggered by claims, and no
// colony founded earlier in a quiet step is ceded in it (8.3 item 4, N2).
//
// Written from the spec only. The cases need no P3-06 name of the library
// (the shipped stage through eng::history_stage), so they compile before
// the implementation exists; no world file is generated, so the whole set
// runs by default.

#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

using namespace conflict_test;

void check_case(const CCase& g, std::map<std::int64_t, std::int64_t>* kinds = nullptr) {
    CAPTURE(g.name);
    const wd::ContentSet& c = content_of(g);
    const eng::HistoryInputs in = synthetic_inputs(g);
    const Tables t = eng::history_stage(c, in);
    const std::map<std::string, std::string> h = table_hashes(t);
    CHECK(g.tables.size() == 13U);
    for (const auto& [name, hex] : g.tables) {
        CAPTURE(name);
        CHECK(h.at(name) == hex);
    }
    CHECK(report_lines_05(t, in) == g.report);
    CHECK(stats_lines_06(t, in) == g.stats);
    CHECK(war_lines_from_rows(t) == golden_wars_without_how(g));
    if (g.kind == "synthetic") {
        CHECK(t.event.size() == g.sentences.size());
    }
    check_invariants_06(t, in, 25, conflict_int(c, "last_refuge") == 1);
    if (kinds != nullptr) {
        for (const Row& e : t.event) (*kinds)[at(e, 3)] += 1;
    }
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 14, 16.2 item 15: the sixteen synthetic geographies equal the reference; invariants hold") {
    std::size_t n = 0;
    for (const CCase& g : conflict_golden()) {
        if (g.kind == "synthetic") {
            check_case(g);
            ++n;
        }
    }
    CHECK(n == 16U);
}

TEST_CASE("P3-06 7, 14: the stress cases equal the reference and reach every conflict event kind") {
    std::size_t n = 0;
    std::map<std::int64_t, std::int64_t> kinds;
    for (const CCase& g : conflict_golden()) {
        if (g.kind != "stress") continue;
        check_case(g, &kinds);
        ++n;
    }
    CHECK(n == 12U);
    for (std::int64_t k = k_claim; k <= k_civ_conquered; ++k) {
        CAPTURE(k);
        CHECK(kinds[k] > 0);
    }
    CHECK(kinds[k_flare_up] > 0);
    CHECK(kinds[k_capital_moved] > 0);
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
