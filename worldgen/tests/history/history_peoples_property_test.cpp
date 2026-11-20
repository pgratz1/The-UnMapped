// Spec P3-05 14 and 16.2 item 17: the peoples history on the synthetic
// geographies of the golden file (make_synthetic in the reference and in
// p3_05_peoples_support.hpp: 2 to 60 regions, islets, regions with no
// walkable cell, walkable, pass, barrier and river borders, river mouths;
// race subsets all, elf, dwarf/faerie, goblin/human/orc; civs auto, 1, 4;
// intensities normal, calm, turbulent; spans 2000 and 6000) and on the
// stress cases (pressures strong enough to reach abandonment, capital
// moves, endings, secessions and fading): the thirteen tables equal the
// reference's (table hashes), the homes, the [history] lines of 15.4 and
// the [stats] keys, and invariants 1-8 re-derived from the rows. Invariant
// 9 (a quiet step of L years = L tick years without pressures) on a
// hand-made world; invariant 10 is in history_peoples_unit_test.cpp.
//
// Written from the spec only. No world file is generated (inputs are built
// in memory), so the whole set runs by default. Since generator 0.16.0 the
// shipped stage adds conflict (P3-06): these cases run the peoples-only
// configuration (PeoplesConfig through run_engine), as the golden file does.

#include "p3_05_peoples_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using namespace peoples_test;

std::int64_t peoples_int(const wd::ContentSet& c, std::string_view f) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == "peoples") return d.record.get_int(f);
        }
    }
    FAIL("no history.peoples");
    return 0;
}

void check_case(const GCase& g) {
    CAPTURE(g.name);
    const wd::ContentSet& c = content_of(g);
    const eng::HistoryInputs in = synthetic_inputs(g);
    PeoplesConfig cfg(c);  // the peoples-only configuration (the golden file's since 0.16.0)
    const Tables t = eng::run_engine(c, in, cfg.get());
    const std::map<std::string, std::string> h = table_hashes(t);
    for (const auto& [name, hex] : g.tables) {
        CAPTURE(name);
        CHECK(h.at(name) == hex);
    }
    CHECK(homes_from_rows(t) == homes_without_n(g));
    CHECK(report_lines_05(t, in) == g.report);
    CHECK(stats_lines(t) == g.stats);
    CHECK(t.event.size() == g.sentences.size());
    check_invariants(t, in, g, peoples_int(c, "ruin_fade_years"), peoples_int(c, "abandon_min"));
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-05 14, 16.2 item 17: the sixteen synthetic geographies equal the reference, invariants 1-8 hold") {
    std::size_t n = 0;
    for (const GCase& g : peoples_golden()) {
        if (g.kind == "synthetic") {
            check_case(g);
            ++n;
        }
    }
    CHECK(n == 16U);
}

TEST_CASE("P3-05 8.5, 8.6, 10.3, 14: the stress cases equal the reference and reach every rare event kind") {
    std::size_t n = 0;
    std::map<std::int64_t, std::int64_t> kinds;
    for (const GCase& g : peoples_golden()) {
        if (g.kind != "stress") continue;
        check_case(g);
        ++n;
        PeoplesConfig cfg(content_of(g));
        const Tables t = eng::run_engine(content_of(g), synthetic_inputs(g), cfg.get());
        for (const Row& e : t.event) {
            kinds[at(e, 3)] += 1;
        }
    }
    CHECK(n == 10U);
    for (const std::int64_t k : {std::int64_t{k_settlement_abandoned}, std::int64_t{k_capital_moved},
                                 std::int64_t{k_civ_ended}, std::int64_t{k_ruin_faded}, std::int64_t{k_civ_seceded},
                                 std::int64_t{k_settlement_dwindled}, std::int64_t{k_plague_spread}}) {
        CAPTURE(k);
        CHECK(kinds[k] > 0);
    }
}

TEST_CASE("P3-05 14 item 9, 9.2: a quiet step of 25 years equals 25 tick years without pressures") {
    // Span 2000, one race: normal has A = scale(150) = 50, so years 0-49 are ticks and [50, 75) is the
    // first quiet step; with active_years 225 the opening is 75 years of ticks. No pressures, no
    // foundings, and a population that stays a village: no event of peoples in either run.
    db::RegionRow a = plain_region(1, 0, 0, 31, 63);
    a.cells = 2048;
    a.walk_cells = 2000;
    Geo g = hand_made(64, {a});
    g.region_biomes = {{1, 4, 2048}};
    const wd::ContentSet quiet = with_peoples({{"found_rate_permille", I64(0)},
                                               {"famine_ppm", I64(0)},
                                               {"plague_ppm", by_kind(0, 0, 0, 0, 0)},
                                               {"crisis_per_generation_permille", I64(0)}});
    EngineTuning et;
    et.normal.active_years = 225;
    const wd::ContentSet ticks = with_engine(et, quiet);
    Settings s;
    s.span = 2000;
    s.races = {"human"};
    s.civs = 1;
    std::int64_t after_quiet = -1, after_ticks = -1;
    {
        PeoplesConfig cfg(quiet);
        cfg.add_after({eng::Step::q3}, [&](eng::StepContext& ctx) {
            if (ctx.year() == 50) after_quiet = ctx.state().settlement(1).population;
        });
        const Tables t = eng::run_engine(quiet, inputs_of(g, 0, s, quiet), cfg.get());
        REQUIRE(t.phase.size() >= 2U);
        CHECK(at(t.phase[1], 2) == 2);
        CHECK(at(t.phase[1], 3) == 50);
    }
    {
        PeoplesConfig cfg(ticks);
        cfg.add_after({eng::Step::t2}, [&](eng::StepContext& ctx) {
            if (ctx.year() == 74) after_ticks = ctx.state().settlement(1).population;
        });
        const Tables t = eng::run_engine(ticks, inputs_of(g, 0, s, ticks), cfg.get());
        CHECK(at(t.phase[0], 4) == 74);
    }
    CHECK(after_quiet > 300);
    CHECK(after_quiet < 1500);
    CHECK(after_quiet == after_ticks);
}

TEST_CASE("P3-05 14 item 9, 8.3 (lead's ruling 4): recovery is applied each year inside Q1, as in T1") {
    // A capital pushed far above its share in year 48 declines toward it; a famine of exactly 5% in T4 of
    // year 49 (the last tick year) makes it declining with before = P. In the quiet step [50, 75) its
    // population falls toward the share: per year it recovers in the first year (P >= 0.9 before), but
    // not by the end of the step. The test recomputes both readings and requires the per-year one.
    db::RegionRow a = plain_region(1, 0, 0, 31, 63);
    a.cells = 2048;
    a.walk_cells = 2000;
    Geo g = hand_made(64, {a});
    g.region_biomes = {{1, 4, 2048}};
    const wd::ContentSet c = with_peoples({{"found_rate_permille", I64(0)},
                                           {"famine_ppm", I64(1000000)},
                                           {"famine_crowding_permille", I64(0)},
                                           {"famine_toll_min", I64(50)},
                                           {"famine_toll_max", I64(50)},
                                           {"abandon_permille", by_kind(0, 0, 0, 0, 0)},
                                           {"plague_ppm", by_kind(0, 0, 0, 0, 0)},
                                           {"crisis_per_generation_permille", I64(0)}});
    Settings s;
    s.span = 2000;
    s.races = {"human"};
    s.civs = 1;
    PeoplesConfig cfg(c);
    const PeoplesConfig* p = &cfg;
    std::int64_t before = 0, p0 = 0, share = 0;
    eng::SettlementStatus after_q1 = eng::SettlementStatus::thriving;
    cfg.add_after({eng::Step::t2}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 48) share = p->view().share(ctx, 1);
        if (ctx.year() == 49) before = ctx.state().settlement(1).population;  // after T1, before T4
    });
    cfg.add_after({eng::Step::t6}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 48) ctx.state().set_population(1, 2 * share);
        if (ctx.year() == 49) {
            p0 = ctx.state().settlement(1).population;
            CHECK(ctx.state().settlement(1).status == eng::SettlementStatus::declining);
            CHECK(p->view().share(ctx, 1) == share);
        }
    });
    cfg.add_after({eng::Step::q3}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 50) after_q1 = ctx.state().settlement(1).status;
    });
    (void)eng::run_engine(c, inputs_of(g, 0, s, c), cfg.get());
    REQUIRE(share > 0);
    REQUIRE(p0 == before - before * 50 / 1000);
    // 9.1 decline over 25 years, 8.3 each year or once at the end
    std::int64_t q = p0;
    bool each_year = false;
    for (int y = 0; y < 25; ++y) {
        q = q > share ? q - std::max<std::int64_t>(1, (q - share) * 100 / 1000) : q;
        each_year = each_year || q * 1000 >= 900 * before;
    }
    const bool at_end = q * 1000 >= 900 * before;
    REQUIRE(each_year);
    REQUIRE_FALSE(at_end);  // the case tells the two readings apart
    CHECK(after_q1 == eng::SettlementStatus::thriving);
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
