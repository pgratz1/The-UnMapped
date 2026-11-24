// Spec P3-05 r6 16.1a (generator 0.17.0, docs/specs/p3-08-followups.md): unit tests of the r6 rules through
// the engine's public seam, on hand-made geography: the cell term of 6.2 (17a), the fortress cell rule of
// 8.4 (17b), the last settlement of 8.5 (a) (17c), the destination of 8.5 item 1 with kind 31 (17d), the
// decline start among the causes (17e), the cap drain of 9.1 (17f); the saliences of 10.2 and 10.3 (17g,
// 17h) on the stress cases of the golden file (with invariant 13 recomputed from rows); 17i (kind 31 of
// the catalogue) is in history_peoples_unit_test.cpp.
//
// Written from the spec only, before the implementation (test-writer rule). The layers elevation, ore and
// walk_area reach HistoryInputs only with UNMAPPED_P3_08_API (p3_08_switch.hpp); every case compiles
// against the 0.16.0 library and fails there, as it must, until the P3-08 implementation is merged.

#include "p3_05_peoples_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

using namespace peoples_test;
using eng::Step;

std::int64_t test_event(eng::StepContext& ctx, std::int64_t region, std::int64_t settlement) {
    eng::EventSpec e;
    e.kind = "test_change";
    e.year = ctx.year();
    e.end_year = ctx.year();
    e.region = region;
    e.settlement = settlement;
    return ctx.append(e);
}

// Two regions of 32 x 64 cells side by side, walkable border, grassland, not upland.
Geo two_regions() {
    db::RegionRow a = plain_region(1, 0, 0, 31, 63);
    db::RegionRow b = plain_region(2, 32, 0, 63, 63);
    for (db::RegionRow* r : {&a, &b}) {
        r->cells = 2048;
        r->walk_cells = 2000;
    }
    Geo g = hand_made(64, {a, b});
    g.borders = {db::RegionBorderRow{1, 2, 1024, 0, 0, 0, 10, 0, 0, 0, 0}};
    g.region_biomes = {{1, 4, 2048}, {2, 4, 2048}};
    return g;
}

// One region of 64 x 64 cells (not upland), grassland.
Geo one_region(std::int64_t elev_max_dm) {
    db::RegionRow a = plain_region(1, 0, 0, 63, 63);
    a.cells = 4096;
    a.walk_cells = 4000;
    a.elev_max_dm = elev_max_dm;
    Geo g = hand_made(64, {a});
    g.region_biomes = {{1, 4, 4096}};
    return g;
}

std::size_t cell(std::int64_t x, std::int64_t y) { return static_cast<std::size_t>(y * 64 + x); }

Settings one_race(const char* race) {
    Settings s;
    s.span = 2000;
    s.races = {race};
    s.civs = 1;
    return s;
}

const wd::ContentSet& quiet_content() {
    static const wd::ContentSet c = with_peoples({{"found_rate_permille", I64(0)},
                                                  {"famine_ppm", I64(0)},
                                                  {"plague_ppm", by_kind(0, 0, 0, 0, 0)},
                                                  {"crisis_per_generation_permille", I64(0)},
                                                  // P5-03 11.1 (0.24.0): P3-05 r6's reach and no road loss, so
                                                  // that the next region's settlement is a destination as before
                                                  {"abandon_reach_hops", I64(2)},
                                                  {"abandon_loss_min_permille", I64(0)},
                                                  {"abandon_loss_max_permille", I64(0)}});
    return c;
}

// Plague every year with a fixed toll of 300 per mille, no spread, certain chance abandonment.
const wd::ContentSet& plague_content() {
    static const wd::ContentSet c = with_peoples({{"found_rate_permille", I64(0)},
                                                  {"famine_ppm", I64(0)},
                                                  {"plague_ppm", by_kind(1000000, 1000000, 1000000, 1000000, 1000000)},
                                                  {"plague_toll_min", I64(300)},
                                                  {"plague_toll_max", I64(300)},
                                                  {"plague_spread_permille", I64(0)},
                                                  {"abandon_permille", by_kind(1000, 1000, 1000, 1000, 1000)},
                                                  {"crisis_per_generation_permille", I64(0)},
                                                  {"abandon_reach_hops", I64(2)},   // P5-03 11.1, as above
                                                  {"abandon_loss_min_permille", I64(0)},
                                                  {"abandon_loss_max_permille", I64(0)}});
    return c;
}

std::vector<std::int64_t> causes_of(const Tables& t, std::int64_t e) {
    std::vector<std::int64_t> cs;
    for (const Row& c : t.event_cause) {
        if (at(c, 0) == e) cs.push_back(at(c, 1));
    }
    return cs;
}
std::map<std::int64_t, std::int64_t> values_of(const Tables& t, std::int64_t e) {
    std::map<std::int64_t, std::int64_t> v;
    for (const Row& r : t.event_value) {
        if (at(r, 0) == e) v[at(r, 1)] = at(r, 2);
    }
    return v;
}
std::int64_t participant(const Tables& t, std::int64_t e, std::int64_t role_seq) {
    for (const Row& p : t.event_participant) {
        if (at(p, 0) == e && at(p, 2) == role_seq) return at(p, 4);
    }
    return 0;
}
std::vector<const Row*> events_of(const Tables& t, std::int64_t kind) {
    std::vector<const Row*> out;
    for (const Row& e : t.event) {
        if (at(e, 3) == kind) out.push_back(&e);
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-05 r6 6.2, 16.1a 17a: a race with a cell term skips the seed cell and takes the high ore cell") {
    Geo g = one_region(3000);
    // the seed cell (1, 1): habitability 0, elevation 1; A (40, 1): habitability 200, low; B (1, 40): ore 200 at
    // the region's elev_max_dm
    g.hab[cell(40, 1)] = 200;
    g.ore[cell(1, 40)] = 200;
    g.elevation[cell(1, 40)] = 3000;
    {
        const Tables t = eng::history_stage(quiet_content(), inputs_of(g, 0, one_race("elf"), quiet_content()));
        REQUIRE_FALSE(t.settlement.empty());
        CHECK(at(t.settlement[0], 2) == 1);  // cell_weights 0, 0: the seed cell
        CHECK(at(t.settlement[0], 3) == 1);
    }
    {
        // dwarves (400, 600): v(B) = 0 + (400 x 200 + 600 x 255) div 1000 = 233 > v(A) = 200 + 600 x 0 div 1000
        const Tables t = eng::history_stage(quiet_content(), inputs_of(g, 0, one_race("dwarf"), quiet_content()));
        REQUIRE_FALSE(t.settlement.empty());
        CHECK(at(t.settlement[0], 2) == 1);
        CHECK(at(t.settlement[0], 3) == 40);
        CHECK(at(t.settlement[0], 9) == village);  // a walkable cell of a non-upland region: the ladder (300)
    }
    {
        // elev_max_dm 0: hgt = min(255, e x 255 div max(1, 0)) = 255 for both cells, so A (200 + 153) beats
        // B (80 + 153)
        Geo g0 = g;
        g0.regions[0].elev_max_dm = 0;
        const Tables t = eng::history_stage(quiet_content(), inputs_of(g0, 0, one_race("dwarf"), quiet_content()));
        REQUIRE_FALSE(t.settlement.empty());
        CHECK(at(t.settlement[0], 2) == 40);
        CHECK(at(t.settlement[0], 3) == 1);
    }
    {
        // every cell of equal value: ties to the smaller cell index, (0, 0) (the seed is not taken)
        Geo ge = one_region(1);
        const Tables t = eng::history_stage(quiet_content(), inputs_of(ge, 0, one_race("dwarf"), quiet_content()));
        REQUIRE_FALSE(t.settlement.empty());
        CHECK(at(t.settlement[0], 2) == 0);
        CHECK(at(t.settlement[0], 3) == 0);
    }
}

TEST_CASE("P3-05 r6 8.4, 16.1a 17b: a dwarven founding off the walkable ground is a fortress; a goblin's is not") {
    Geo g = one_region(3000);
    g.ore[cell(1, 40)] = 200;
    g.elevation[cell(1, 40)] = 3000;
    g.walk_area[cell(1, 40)] = 0;
    g.walk_area[cell(1, 1)] = 0;  // the seed cell, the goblins' site
    const Tables d = eng::history_stage(quiet_content(), inputs_of(g, 0, one_race("dwarf"), quiet_content()));
    REQUIRE_FALSE(d.settlement.empty());
    CHECK(at(d.settlement[0], 2) == 1);
    CHECK(at(d.settlement[0], 3) == 40);
    CHECK(at(d.settlement[0], 9) == fortress);
    const Tables gb = eng::history_stage(quiet_content(), inputs_of(g, 0, one_race("goblin"), quiet_content()));
    REQUIRE_FALSE(gb.settlement.empty());
    CHECK(at(gb.settlement[0], 2) == 1);
    CHECK(at(gb.settlement[0], 3) == 1);
    CHECK(at(gb.settlement[0], 9) != fortress);  // no cell term: the ladder
}

TEST_CASE("P3-05 r6 8.5 (a), r9 21 item 6, 16.1a 17c: a race's last refuge is never abandoned; with a second it is by chance") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, one_race("human"), plague_content());
    {
        // one settlement, struck every year: the chance is not drawn (r6), and from 0.18.0 (P3-05 r9 21 item 6)
        // it is its race's last refuge, so it is not abandoned below abandon_min either: it stands, the
        // civilization does not end (until 0.17.0 it was abandoned as kind 16 and the civilization ended)
        PeoplesConfig cfg(plague_content());
        const Tables t = eng::run_engine(plague_content(), in, cfg.get());
        CHECK(events_of(t, k_settlement_abandoned).empty());
        CHECK(events_of(t, k_settlement_abandoned_to).empty());
        CHECK(events_of(t, k_civ_ended).empty());
        CHECK(is_null(t.civilization[0][8]));
    }
    {
        // a second settlement founded earlier in the same step (T4, before peoples) once the capital is
        // declining: the chance is drawn and decides (abandon_permille 1000): kind 31 to the new settlement
        PeoplesConfig cfg(plague_content(), true);
        std::int64_t s2 = 0, year = -1;
        cfg.add_before({Step::t4}, [&](eng::StepContext& ctx) {
            if (s2 != 0 || ctx.state().settlement(1).status != eng::SettlementStatus::declining) return;
            const std::int64_t other = 3 - ctx.state().civ(1).home_region;
            const db::RegionRow& r = in.regions[static_cast<std::size_t>(other - 1)];
            const std::int64_t e = test_event(ctx, other, 0);
            s2 = ctx.state().found_settlement(r.seed_x, r.seed_y, other, 1, eng::SettlementKind::village, 500, "Test Hold", e);
            ctx.state().set_region_owner(other, 1, e);
            year = ctx.year();
        });
        const Tables t = eng::run_engine(plague_content(), in, cfg.get());
        REQUIRE(s2 != 0);
        const auto ab = events_of(t, k_settlement_abandoned_to);
        REQUIRE_FALSE(ab.empty());
        const Row& e = *ab[0];
        CHECK(at(e, 1) == year);
        CHECK(at(e, 6) == 1);
        CHECK(participant(t, at(e, 0), 3) == s2);
        const auto v = values_of(t, at(e, 0));
        CHECK(v.at(1) + v.at(2) >= 40);  // a chance abandonment
        for (const Row* c : events_of(t, k_civ_ended)) CHECK(at(*c, 1) != year);
    }
}

TEST_CASE("P3-05 r6 8.5 item 1, 16.1a 17d: the destination, settled and lost; kind 16 when no survivor arrives") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, one_race("human"), quiet_content());
    // room: 2 C - P_t after the year's growth; `excess` above 2 C at the probe leaves no room after T1's decline
    for (const std::int64_t excess : {std::int64_t{-200}, std::int64_t{-5}, std::int64_t{1000}}) {
        CAPTURE(excess);
        PeoplesConfig cfg(quiet_content(), true);
        const PeoplesConfig* p = &cfg;
        std::int64_t s2 = 0;
        cfg.add_after({Step::t6}, [&](eng::StepContext& ctx) {
            const std::int64_t other = 3 - ctx.state().civ(1).home_region;
            if (ctx.year() == 1) {
                const db::RegionRow& r = in.regions[static_cast<std::size_t>(other - 1)];
                const std::int64_t e = test_event(ctx, other, 0);
                s2 = ctx.state().found_settlement(r.seed_x, r.seed_y, other, 1, eng::SettlementKind::village, 500,
                                                  "Test Hold", e);
                ctx.state().set_region_owner(other, 1, e);
            }
            if (ctx.year() == 3) {
                ctx.state().set_population(1, 10);
                ctx.state().set_population(s2, 2 * p->view().capacity(other, 5) + excess);
            }
        });
        const Tables t = eng::run_engine(quiet_content(), in, cfg.get());
        const auto to = events_of(t, k_settlement_abandoned_to);
        const auto plain = events_of(t, k_settlement_abandoned);
        if (excess >= 1000) {
            REQUIRE(plain.size() == 1U);
            CHECK(to.empty());
            CHECK(values_of(t, at(*plain[0], 0)).at(1) == 11);  // 10 + max(1, d)
        } else {
            REQUIRE(to.size() == 1U);
            CHECK(plain.empty());
            const Row& e = *to[0];
            CHECK(at(e, 1) == 4);  // T1 of the next year (8.5 (b))
            CHECK(participant(t, at(e, 0), 1) == 1);
            CHECK(participant(t, at(e, 0), 2) == 1);
            CHECK(participant(t, at(e, 0), 3) == s2);
            const auto v = values_of(t, at(e, 0));
            CHECK(v.at(1) >= 1);
            CHECK(v.at(1) + v.at(2) == 11);
            if (excess == -200) CHECK(v.at(2) == 0);  // ample room: nobody lost
            // the census of the destination records its population after it took in `settled`: settled =
            // min(11, 2 C - P_t) with P_t its population before
            const std::int64_t region_t = at(t.settlement.at(static_cast<std::size_t>(s2 - 1)), 4);
            for (const Row& c : t.settlement_census) {
                if (at(c, 0) == s2 && at(c, 1) == 4) {
                    const std::int64_t p_t = at(c, 5) - v.at(1);
                    CHECK(v.at(1) == std::min<std::int64_t>(
                                         11, std::max<std::int64_t>(0, 2 * cfg.view().capacity(region_t, 5) - p_t)));
                }
            }
        }
    }
}

TEST_CASE("P3-05 r6 8.5 causes, 16.1a 17e: an abandonment by chance names the blow and the decline start") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, one_race("human"), plague_content());
    PeoplesConfig cfg(plague_content(), true);
    std::int64_t s2 = 0;
    cfg.add_after({Step::t6}, [&](eng::StepContext& ctx) {
        if (s2 != 0) return;
        const std::int64_t other = 3 - ctx.state().civ(1).home_region;
        const db::RegionRow& r = in.regions[static_cast<std::size_t>(other - 1)];
        const std::int64_t e = test_event(ctx, other, 0);
        s2 = ctx.state().found_settlement(r.seed_x, r.seed_y, other, 1, eng::SettlementKind::town, 3000, "Test Hold", e);
        ctx.state().set_region_owner(other, 1, e);
    });
    const Tables t = eng::run_engine(plague_content(), in, cfg.get());
    // settlement 1: struck while thriving (a, its decline start; 300 -> 210, below 900 per mille of 300, no
    // recovery), then struck while declining (b): the chance abandons it, causes (a, b)
    std::vector<std::int64_t> strikes;
    for (const Row* e : events_of(t, k_plague_outbreak)) {  // P3-05 r12 22.1 (0.19.0): every outbreak is kind 37
        if (at(*e, 6) == 1) strikes.push_back(at(*e, 0));
    }
    REQUIRE(strikes.size() >= 2U);
    const Row* ab = nullptr;
    for (const Row& e : t.event) {
        if ((at(e, 3) == k_settlement_abandoned || at(e, 3) == k_settlement_abandoned_to) && at(e, 6) == 1) ab = &e;
    }
    REQUIRE(ab != nullptr);
    CHECK(causes_of(t, at(*ab, 0)) == std::vector<std::int64_t>{strikes[0], strikes[1]});
}

TEST_CASE("P3-05 r6 9.1, 16.1a 17f: a settlement above its kind cap drains toward it at decline_permille") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, one_race("faerie"), quiet_content());
    PeoplesConfig cfg(quiet_content());
    const PeoplesConfig* p = &cfg;
    std::int64_t before = 0, share = 0, after = 0, kind = 0;
    cfg.add_before({Step::t1}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 2) {
            ctx.state().set_population(1, 2277);  // a faerie village (cap 1499) after taking in refugees
            before = ctx.state().settlement(1).population;
            share = p->view().share(ctx, 1);
            kind = static_cast<std::int64_t>(ctx.state().settlement(1).kind);
        }
    });
    cfg.add_after({Step::t1}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 2) after = ctx.state().settlement(1).population;
    });
    (void)eng::run_engine(quiet_content(), in, cfg.get());
    REQUIRE(before == 2277);
    REQUIRE(kind == village);
    const std::int64_t target = std::min<std::int64_t>(share, 1499);
    CHECK(after == before - std::max<std::int64_t>(1, (before - target) * 100 / 1000));
    if (share >= 1499) CHECK(after == 2277 - 77);
}

TEST_CASE("P3-05 r6 10.2, 10.3, 14 item 13, 16.1a 17g 17h: saliences by toll and split on the stress cases") {
    std::int64_t major = 0, small = 0, split = 0, unsplit = 0;
    for (const GCase& g : peoples_golden()) {
        if (g.kind != "stress") continue;
        CAPTURE(g.name);
        const wd::ContentSet& c = content_of(g);
        const eng::HistoryInputs in = synthetic_inputs(g);
        PeoplesConfig cfg(c);
        const Tables t = eng::run_engine(c, in, cfg.get());
        check_invariants_r6(t, 40, synthetic_geo(g));
        std::map<std::int64_t, std::string> civ_name, stl_name;
        for (const Row& r : t.civilization) civ_name[at(r, 0)] = r[1].bytes;
        for (const Row& r : t.settlement) stl_name[at(r, 0)] = r[1].bytes;
        std::set<std::int64_t> seceded;
        for (const Row* e : events_of(t, k_civ_seceded)) {
            for (const std::int64_t x : causes_of(t, at(*e, 0))) seceded.insert(x);
        }
        for (const Row* e : events_of(t, k_plague_outbreak)) (at(*e, 7) == 45 ? major : small) += 1;  // r12 22.1: kind 37
        for (const Row* e : events_of(t, k_succession_crisis)) {
            const std::int64_t id = at(*e, 0);
            (seceded.count(id) == 1U ? split : unsplit) += 1;
            // 17h: the sentence names the capital (the event's settlement) and the civilization
            const auto it = g.sentences.find(id);
            REQUIRE(it != g.sentences.end());
            // P3-04 r10 8.5: a leading "The " of a name mid-sentence is written "the ".
            const auto mid = [](std::string n) {
                if (n.rfind("The ", 0) == 0) n[0] = 't';
                return n;
            };
            CHECK(it->second == "A succession crisis at " + mid(stl_name.at(at(*e, 6))) + " shook " +
                                    mid(civ_name.at(participant(t, id, 1))) + ".");
        }
    }
    // the stress cases reach both saliences of each kind
    CHECK(major > 0);
    CHECK(small > 0);
    CHECK(split > 0);
    CHECK(unsplit > 0);
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
