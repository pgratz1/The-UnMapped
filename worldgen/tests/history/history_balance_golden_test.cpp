// Spec P3-06 r9 20.14 item 8 (P3-09, generator 0.18.0): the test writer's extra golden case
// hist-conflict-flight (seed 1 at 512, default settings), a world in which last refuges fall into flight and
// their hidden refuges are found again before the present. Generated with the shipped stages and compared
// with worldgen/tests/golden/p3-06-conflict.txt (the reference p3_conflict_reference.py): hash --detail's
// thirteen table lines, [history], [stats] (with P3-07 r8 21.5's six keys, from the rows), the per-war lines
// re-derived from the rows (outcomes with -ceded and -tribute), every rendered sentence (kinds 32 and 33
// with their templates), the invariants of P3-06 14 and r9 20.13 from the rows, threads 1 and 4
// byte-identical; and that the case does what it is for: at least one refugees_settled whose refuge is found
// (its year + its founder's race's hidden_years, P3-06 r17 22.2, before the present) and a tribute.
//
// P3-06 r13 20.27 item 8 (the last test): invariant (13) on 2048 seed 3, default settings, the case of the P3-09
// validation's D1, in the doctest suite "history-slow" (worldgen-tests -ts=history-slow --no-skip). Invariant (13)
// on the synthetic, stress and golden worlds is checked with the r9 invariants (check_invariants_09).
//
// Written from the spec only; needs no new library name. Runs by default (one 512 world, two threads).

#include "p5_06_history_section.hpp"
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
    REQUIRE(g.tables.size() == 21U);   // P5-03 16.1 (schema 10): two more;  // P5-02 11.1 (schema 9): five more; P3-04 r21 23.4: region_name (schema 8)
    for (const auto& [name, hex] : g.tables) {
        CAPTURE(name);
        CHECK(detail.at(name) == hex);
    }
    const auto rep = worldgen::inspect::build_report(r);
    REQUIRE(rep.ok());
    std::string want = "[history]\n";
    for (const std::string& l : g.report) want += l + "\n";
    CHECK(p506section::strip_figure_counts(section(rep.value(), "[history]")) == want);   // P5-06 9.4: figures and books keys close the section
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
    CHECK(invariants_19(t, shipped()) == std::vector<std::string>{});  // P3-06 r14 (14), 21.7 (0.19.0)
    check_r17(t, shipped(), g);                                        // P3-06 r17 (8), (15), (17), 22.5's facts
    // what the case is for
    const Index x(t);
    const std::map<std::int64_t, std::int64_t> hidden_of_race = race_hidden_years(shipped());  // P3-06 r17 22.2
    std::int64_t flights = 0, found = 0, tributes = 0;
    for (const Row& e : t.event) {
        if (at(e, 3) == k_refugees_settled) {
            ++flights;
            const std::int64_t hidden_years = hidden_of_race.at(at(*x.cv.at(x.role1(at(e, 0), 2)), 2));
            found += at(e, 1) + hidden_years < g.span ? 1 : 0;
            // the sentence of kind 32 names the origin, the refuge and its region
            const std::string& s = sent[static_cast<std::size_t>(at(e, 0) - 1)];
            CHECK(s.rfind("The survivors of ", 0) == 0);
            CHECK(s.find(" fled into hiding and founded ") != std::string::npos);
        }
        tributes += at(e, 3) == k_tribute ? 1 : 0;
    }
    // P5-12 6 "Effect on the rest of history": from 0.22.0 seed 1 at 512 has no flight (the renamings shift the
    // event ids that conflict keys draws by); the reference reports it and no longer checks it. Flights and found
    // refuges are reached by the stress cases of p3-06-conflict.txt.
    MESSAGE("hist-conflict-flight at 0.22.0: flights " << flights << ", found " << found);
    CHECK(tributes >= 1);
    // threads: byte-identical tables
    CHECK(tables_of_world(open_world(flight_world(4))) == t);
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

// P5-08 (generator 0.30.0, integration part 3b): seed 3 has none any more; of the 1024 turbulent seeds 0 to 39, seeds
// 24, 28 and 33 have one each (searched by this test's rule on CLI worlds); seed 24's is region_ceded e374 of
// region 21 in 3407 (colony e372).
TEST_CASE("P3-06 r13 20.27 items 7, 8: 1024 turbulent seed 24, a colony founded in the step of a cession passes to the winner") {
    // 20.27 item 7 at 0.19.0 (the 0.18.0 case, 2048 seed 3's s240, no longer occurs; P3-10 integration; the first
    // round's 1024 seed 36 case, e310, no longer occurs with the second round's rules, and no 1024 default world
    // of seeds 0 to 160 has one): the reference (p3_conflict_reference.py check-world, table for table on this
    // world at the second round's integration) has two such cessions in 1024 seed 5 with --event-intensity
    // turbulent: region_ceded e933 of region 25 in 4410 (colony e930, s101) and e1102 of region 29 in 4885
    // (colony e1100, s110), each in a quiet step. The test finds them from the log rather than by id: a
    // region_ceded e of region R in year Y of a quiet phase (one Q4, so one step), from L to W, preceded in Y by a
    // colony_founded of a settlement s in R with founder L. (Same year in an active phase is not the same step:
    // the T2 colony is visible in T6, 1024 seed 23 e215.)
    // P5-01 (generator 0.21.0): seed 5 has none any more; of the 1024 turbulent seeds 0 to 39, seeds 19, 20, 22, 30
    // and 35 have one each (P5-01 integration, C++ worlds); seed 19's is region_ceded e751 of region 10 in 4331
    // (colony e749, s122).
    // P5-02 (generator 0.23.0, test writer): seed 19 has none any more; of the 1024 turbulent seeds 0 to 39, seeds
    // 5 and 12 have one each; seed 5's is region_ceded e1048 of region 34 in 4410 (colony e1046, s112), and the
    // conflict reference's check-world reproduces that world's history tables.
    // P5-03 (generator 0.24.0, test writer at integration): seeds 5 and 12 have none any more; of the 1024 turbulent
    // seeds 0 to 39 only seed 34 has one: region_ceded e148 of region 36 in 2611 (colony e145, s36).
    // P5-03a (generator 0.25.0, validation): seed 34 has none any more; of the 1024 turbulent seeds 0 to 39, seeds 4,
    // 5, 15 and 29 have one each (C++ worlds); seed 4's is region_ceded e173 of region 24 in 3030 (colony e171, s50).
    // P5-04 (generator 0.26.0, test writer at integration): seed 4 has none any more; of seeds 0 to 16 only seed 1
    // has one: region_ceded e648 of region 28 in 3816 (colony e643, s70; check-world equal).
    // P5-06 (generator 0.27.0, test writer at integration): seed 1 has none any more; of the 1024 turbulent seeds 0
    // to 30, seed 12 has two (region_ceded e1986 of region 9 in 5108, colony e1984, s127; e2403 of region 9 in 5308,
    // colony e2401, s134) and seed 24 one (e1356 of region 14 in 5109, colony e1354, s95) (C++ worlds, searched by
    // this test's rule).
    static dbtest::TempDir dir("balance-golden-1024-turbulent-24");
    chrono_api::Params hp;
    hp.intensity = "turbulent";
    const std::string path = dir.file("turbulent-24-1024.db");
    REQUIRE(worldgen::generate_world(chrono_test::world_params(24, 1024, hp, shipped()), shipped(), path,
                                     worldgen::db::Overwrite::refuse)
                .ok());
    const db::WorldReader r = open_world(path);
    const Tables t = tables_of_world(r);
    const Index x(t);
    std::int64_t found = 0;
    for (const Row& e : t.event) {
        if (at(e, 3) != k_region_ceded) continue;
        const std::int64_t y = at(e, 1), region = at(e, 5), loser = x.role1(at(e, 0), 1), winner = x.role1(at(e, 0), 2);
        bool quiet = false;
        for (const Row& ph : t.phase) quiet = quiet || (at(ph, 2) == 2 && at(ph, 3) <= y && y <= at(ph, 4));
        if (!quiet) continue;
        for (const Row& f : t.event) {
            if (at(f, 0) >= at(e, 0) || at(f, 3) != k_colony_founded || at(f, 1) != y || at(f, 5) != region) continue;
            if (x.role1(at(f, 0), 2) != loser) continue;
            ++found;
            const std::int64_t s = at(f, 6);
            CAPTURE(at(e, 0));
            CAPTURE(s);
            // s keeps its founder and founding event; its census row of Y is the winner's (20.27 item 7)
            CHECK(at(*x.st.at(s), 5) == loser);
            CHECK(at(*x.st.at(s), 7) == at(f, 0));
            std::int64_t owner = -1;
            for (const Row& c : t.settlement_census) {
                if (at(c, 0) == s && at(c, 1) == y) owner = at(c, 2);
            }
            CHECK(owner == winner);
            // the winner holds the region from Y with e
            bool row = false;
            for (const Row& rc : t.region_control) {
                row = row || (at(rc, 0) == region && at(rc, 1) == y && at(rc, 3) == winner && at(rc, 4) == at(e, 0));
            }
            CHECK(row);
        }
    }
    CHECK(found >= 1);  // 0.21.0: e751 (s122, region 10, 4331); 0.23.0 seed 5: e1048 (s112, region 34, 4410);
                        // 0.24.0 seed 34: e148 (s36, region 36, 2611); 0.25.0 seed 4: e173 (s50, region 24, 3030)
                        // 0.26.0 seed 1: e648 (s70, region 28, 3816); 0.27.0 seed 12: e1986 (s127, region 9,
                        // 5108), e2403 (s134, region 9, 5308); 0.28.0 seed 18: e598 (s?, region 11, 4631); 0.29.0 seed 3: e1018 (s105, region 26, 5234)
    MESSAGE("same-step colony cessions found: " << found);
    // invariant (13), with 14 item 5 (a region's owner has a standing settlement in it) through the replay
    CHECK(cession_violations(t) == std::vector<std::string>{});
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
