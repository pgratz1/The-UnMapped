// Spec P3-05 16.1 (unit tests through the engine's public seam): the
// catalogue of kinds 6-19 (11), the streams (3.3), the shipped
// configuration (13, P3-04 6.4), PeoplesView on hand-made and synthetic
// inputs against hand-checked values and the reference's sigma and C
// (4, 5), crossing costs (4.4), homes (7.1, 7.2), the fatal messages, the
// ruin hook (8.5 r1, 13) including that it does not end the civilization,
// note_reduction (13, 8.5 (b)), and draw independence (14 item 10, Y3).
//
// Written from the spec only. Hand-made states are built with test
// subsystems that act through State as P3-06 will (13). Empty until the
// implementation's header exists (p3_05_peoples_api.hpp).

#include "p3_05_peoples_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "worldgen/core/streams.hpp"
#include "worldgen/history/draw.hpp"

namespace {

using namespace peoples_test;
using eng::Step;

Tables run_with(PeoplesConfig& cfg, const wd::ContentSet& c, const eng::HistoryInputs& in) {
    return eng::run_engine(c, in, cfg.get());
}

// A tick-step probe that runs `fn` once, in the step and year given.
void once_at(PeoplesConfig& cfg, Step step, std::int64_t year, std::function<void(eng::StepContext&)> fn) {
    cfg.add_after({step}, [year, fn](eng::StepContext& ctx) {
        if (ctx.year() == year) {
            fn(ctx);
        }
    });
}

std::int64_t test_event(eng::StepContext& ctx, std::int64_t region, std::int64_t settlement) {
    eng::EventSpec e;
    e.kind = "test_change";
    e.year = ctx.year();
    e.end_year = ctx.year();
    e.region = region;
    e.settlement = settlement;
    return ctx.append(e);
}

// Two regions of 32 x 64 cells side by side (x 0-31, 32-63), walkable border, grassland.
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

// History.peoples without random pressures or foundings.
const wd::ContentSet& quiet_content() {
    static const wd::ContentSet c = with_peoples({{"found_rate_permille", I64(0)},
                                                  {"famine_ppm", I64(0)},
                                                  {"plague_ppm", by_kind(0, 0, 0, 0, 0)},
                                                  {"crisis_per_generation_permille", I64(0)}});
    return c;
}

Settings humans_2000() {
    Settings s;
    s.span = 2000;
    s.races = {"human"};
    s.civs = 1;
    return s;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-05 11, 16.1 item 14: the compiled catalogue appends kinds 6-19 exactly as section 11") {
    using ET = eng::EntityType;
    struct K {
        const char* key;
        bool caused;
        std::vector<std::pair<const char*, ET>> roles;
        std::vector<const char*> values;
        std::int64_t salience;
        const char* templ;
    };
    const ET S = ET::settlement, C = ET::civilization;
    const std::vector<K> want = {
        {"colony_founded", false, {{"settlement", S}, {"founder", C}, {"origin", S}}, {"population"}, 15,
         "{founder} sent settlers from {origin} to found {settlement} in {@region}."},
        {"became_village", true, {{"settlement", S}, {"civilization", C}}, {"population"}, 10,
         "{settlement} grew into a village of {population}."},
        {"became_town", true, {{"settlement", S}, {"civilization", C}}, {"population"}, 18,
         "{settlement} grew into a town of {population}."},
        {"became_city", true, {{"settlement", S}, {"civilization", C}}, {"population"}, 45,
         "{settlement} grew into a city of {population}."},
        {"settlement_dwindled", false, {{"settlement", S}, {"civilization", C}}, {"population"}, 15,
         "{settlement} dwindled to {population} souls."},
        {"famine", false, {{"settlement", S}, {"civilization", C}}, {"dead", "population"}, 25,
         "Famine struck {settlement}; {dead} died."},
        {"plague", false, {{"settlement", S}, {"civilization", C}}, {"dead", "population"}, 45,
         "Plague broke out in {settlement}; {dead} died."},
        {"plague_spread", true, {{"settlement", S}, {"civilization", C}}, {"dead", "population"}, 25,
         "The plague spread to {settlement}; {dead} died."},
        {"succession_crisis", false, {{"civilization", C}}, {}, 40, "A succession crisis shook {civilization}."},
        {"civ_seceded", true, {{"civilization", C}, {"from", C}}, {"settlements"}, 70,
         "{civilization} broke away from {from}, taking {settlements} {settlements?settlement|settlements} with it."},
        {"settlement_abandoned", true, {{"settlement", S}, {"civilization", C}}, {"survivors"}, 55,
         "{settlement} was abandoned; {survivors} {survivors?survivor|survivors} left."},
        {"capital_moved", true, {{"civilization", C}, {"settlement", S}}, {}, 50,
         "{civilization} moved its seat to {settlement}."},
        {"civ_ended", true, {{"civilization", C}}, {}, 85, "{civilization} came to an end."},
        {"ruin_faded", true, {{"settlement", S}}, {}, 10, "The ruins of {settlement} faded from memory."},
    };
    const eng::EventCatalogue& compiled = eng::compiled();
    REQUIRE(compiled.kinds.size() == 19U);  // no kind 20-30 before 0.16.0 (P3-04 8.2 r4)
    const eng::EventCatalogue cat = eng::catalogue_of(shipped());
    REQUIRE(cat.kinds.size() == 19U);
    for (std::size_t i = 0; i < want.size(); ++i) {
        const K& w = want[i];
        CAPTURE(w.key);
        for (const eng::KindDef* k : {&compiled.kinds[i + 5], &cat.kinds[i + 5]}) {
            CHECK(k->id == static_cast<std::int64_t>(i) + 6);
            CHECK(k->key == w.key);
            CHECK(k->caused == w.caused);
            CHECK(k->region_use == 1);
            CHECK(k->settlement_use == 1);
            REQUIRE(k->roles.size() == w.roles.size());
            for (std::size_t r = 0; r < w.roles.size(); ++r) {
                CHECK(k->roles[r].seq == static_cast<std::int64_t>(r) + 1);
                CHECK(k->roles[r].role == w.roles[r].first);
                CHECK(k->roles[r].type == w.roles[r].second);
                CHECK(k->roles[r].min_count == 1);
                CHECK(k->roles[r].max_count == 1);
            }
            REQUIRE(k->values.size() == w.values.size());
            for (std::size_t v = 0; v < w.values.size(); ++v) {
                CHECK(k->values[v] == w.values[v]);
            }
        }
        CHECK(cat.kinds[i + 5].salience == w.salience);
        CHECK(cat.kinds[i + 5].templ == w.templ);
        CHECK(eng::template_problem(cat.kinds[i + 5], w.templ) == std::nullopt);
    }
    // P3-04's five kinds are unchanged (a shipped kind is never edited, P3-04 8.2).
    CHECK(compiled.kinds[0].key == "awakening");
    CHECK(compiled.kinds[4].key == "flare_up");
}

TEST_CASE("P3-05 3.3: the twelve hist.peoples streams follow hist.chronology.awakening, 51 names in all (P3-06 adds five)") {
    const auto names = ppl::registered_streams();
    REQUIRE(names.size() == 51U);
    std::size_t at = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == "hist.chronology.awakening") at = i;
    }
    REQUIRE(at + 12 < names.size() + 1);
    for (std::size_t i = 0; i < 12; ++i) {
        CHECK(names[at + 1 + i] == ppl::stream_names()[i]);
    }
}

TEST_CASE("P3-05 13, P3-04 6.4: the shipped stage is the peoples configuration (handler, then the subsystem)") {
    const GCase& g = golden_case("syn-0");
    const eng::HistoryInputs in = synthetic_inputs(g);
    PeoplesConfig cfg(shipped());
    const Tables a = run_with(cfg, shipped(), in);
    const Tables b = eng::history_stage(shipped(), in);
    CHECK(str(a.event) == str(b.event));
    CHECK(a == b);
}

TEST_CASE("P3-05 4, 5, 16.1 item 1: PeoplesView score and capacity equal the reference on synthetic geography") {
    for (const char* name : {"syn-0", "syn-3", "syn-8", "syn-11"}) {
        const GCase& g = golden_case(name);
        CAPTURE(g.name);
        const eng::HistoryInputs in = synthetic_inputs(g);
        PeoplesConfig cfg(shipped());
        bool probed = false;
        const PeoplesConfig* p = &cfg;
        cfg.add_after({Step::t2}, [&](eng::StepContext&) {
            if (probed) return;
            probed = true;
            for (const auto& [key, sc] : g.score) {
                CAPTURE(key.first);
                CAPTURE(key.second);
                CHECK(p->view().score(key.first, key.second) == sc.first);
                CHECK(p->view().capacity(key.first, key.second) == sc.second);
            }
        });
        (void)run_with(cfg, shipped(), in);
        CHECK(probed);
    }
}

TEST_CASE("P3-05 4.2, 5.1, 16.1 item 1: each term alone, floor division, the fit clamp and the capacity floor") {
    // fen-only region for faeries: B = 800; S = (0 + 60 x 100 + 20 x 100 + 0 + 20 x 100) x 1000 div (255 x 100) = 392
    // (the reading of 4.2 S: one division of the sum); U = 200 x 250 div 1000 = 50; sigma = 1242, fit 1000.
    struct Case {
        const char* what;
        db::RegionRow r;
        std::vector<db::RegionBiomeRow> biomes;
        std::string race;
        std::int64_t race_id, sigma, capacity;
    };
    db::RegionRow fen = plain_region(1, 0, 0, 63, 63);
    db::RegionRow bare = plain_region(1, 0, 0, 63, 63);
    bare.walk_cells = 0;
    bare.fertility_mean = bare.forage_mean = bare.timber_mean = bare.fresh_water_mean = 0;
    db::RegionRow neg = plain_region(1, 0, 0, 63, 63);
    neg.walk_cells = 4000;
    neg.fertility_mean = neg.forage_mean = neg.timber_mean = neg.fresh_water_mean = 0;
    db::RegionRow full = plain_region(1, 0, 0, 63, 63);
    full.fertility_mean = full.forage_mean = full.timber_mean = full.fresh_water_mean = 255;
    full.ore_reach_cells = 200;
    full.walk_cells = 4000;  // rough 0: no upland term for humans (upland -300)
    const std::vector<Case> cases = {
        // area = 3000 + 1000 x 270 div 1000 = 3270; C = 3270 x 1000 x 250 x 2500 div 10^9 = 2043
        {"faeries on fen", fen, {{1, 12, 4000}}, "faerie", 4, 1242, 2043},
        // dwarves: S = 0, B = 0, U = 800 x 1000 div 1000 = 800; area 4000 x 630 div 1000 = 2520; C = 2520 x 800 x 700 x 2500 div 10^9 = 3528
        {"dwarves on bare rough upland", bare, {}, "dwarf", 3, 800, 3528},
        // elves: heath 5 permille (-200 x 5 = -1000 div 1000 = -1), grassland 0: sigma -1, fit 0, C the floor 400
        {"elves: B = -1 by floor division", neg, {{1, 9, 5}, {1, 4, 995}}, "elf", 1, -1, 400},
        // humans: S 1000, B 300, U 0, C 0: sigma 1300, fit 1000; area 4000; C = 4000 x 1000 x 1000 x 2500 div 10^9
        {"humans on full farmland", full, {{1, 4, 4000}}, "human", 5, 1300, 10000},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        Geo g = hand_made(64, {c.r});
        g.region_biomes = c.biomes;
        Settings s;
        s.span = 2000;
        s.races = {c.race};
        s.civs = 1;
        PeoplesConfig cfg(shipped());
        const PeoplesConfig* p = &cfg;
        std::int64_t sigma = 0, cap = 0;
        once_at(cfg, Step::t2, 0, [&](eng::StepContext&) {
            sigma = p->view().score(1, c.race_id);
            cap = p->view().capacity(1, c.race_id);
        });
        (void)run_with(cfg, shipped(), inputs_of(g, 0, s));
        CHECK(sigma == c.sigma);
        CHECK(cap == c.capacity);
    }
    // coast_m at coast_cells 0, 100, 200, 400 (coast_full 200): humans' C term 200 x coast_m div 1000
    for (const auto& [cells, term] : std::vector<std::pair<std::int64_t, std::int64_t>>{{0, 0}, {100, 100}, {200, 200}, {400, 200}}) {
        CAPTURE(cells);
        db::RegionRow r = full;
        r.coast_cells = cells;
        Geo g = hand_made(64, {r});
        g.region_biomes = {{1, 4, 4000}};
        PeoplesConfig cfg(shipped());
        const PeoplesConfig* p = &cfg;
        std::int64_t sigma = 0;
        once_at(cfg, Step::t2, 0, [&](eng::StepContext&) { sigma = p->view().score(1, 5); });
        (void)run_with(cfg, shipped(), inputs_of(g, 0, humans_2000()));
        CHECK(sigma == 1300 + term);
    }
}

TEST_CASE("P3-05 5.1: the capacity bound guard (C < 2^31) is fatal") {
    db::RegionRow r = plain_region(1, 0, 0, 63, 63);
    r.cells = 100000;
    r.walk_cells = 100000;
    r.fertility_mean = r.forage_mean = r.timber_mean = r.fresh_water_mean = 255;
    r.ore_reach_cells = 200;
    Geo g = hand_made(64, {r});
    g.region_biomes = {{1, 4, 100000}};
    const wd::ContentSet c = with_peoples({{"people_per_kcell", I64(100000000)}});  // past the loader's range
    const std::string m = fatal_of([&] { (void)eng::history_stage(c, inputs_of(g, 0, humans_2000(), c)); });
    // The message is not given by the spec (gap); the implementation uses P3-04 6.1's bound form
    // `history: bound: <what> exceeds <n>`, checked for every region and race.
    CHECK(m.rfind("history: bound: capacity of region 1 for race ", 0) == 0);
    CHECK(m.find(" exceeds 2147483647") != std::string::npos);
}

TEST_CASE("P3-05 4.4, 16.1 item 2: walkable, pass-only, barrier, river share exactly 500 permille, length_m 0") {
    std::vector<db::RegionRow> rs;
    for (std::int64_t i = 0; i < 5; ++i) {
        rs.push_back(plain_region(i + 1, 12 * i, 0, 12 * i + 11, 63));
    }
    Geo g = hand_made(64, rs);
    auto border = [](std::int64_t a, std::int64_t b, std::int64_t len, std::int64_t walk, std::int64_t river,
                     std::int64_t pass) { return db::RegionBorderRow{a, b, len, 0, 0, 0, walk, river, 0, pass, 0}; };
    g.borders = {border(1, 2, 1000, 3, 0, 0), border(1, 5, 0, 1, 5, 0), border(2, 3, 1000, 0, 0, 1),
                 border(3, 4, 1000, 0, 0, 0),
                 // 32 x 16 x 1000 = 512000 >= 500 x 1024: a river crossing (cost 1 + 2)
                 border(4, 5, 1024, 1, 32, 0)};
    Settings s = humans_2000();
    PeoplesConfig cfg(shipped());
    const PeoplesConfig* p = &cfg;
    std::vector<std::vector<ppl::Nb>> got(6);
    once_at(cfg, Step::t2, 0, [&](eng::StepContext&) {
        for (std::int64_t r = 1; r <= 5; ++r) got[static_cast<std::size_t>(r)] = ppl::neighbours(p->view(), r);
    });
    (void)run_with(cfg, shipped(), inputs_of(g, 0, s));
    CHECK(got[1] == std::vector<ppl::Nb>{{2, 1}, {5, 1}});  // length_m 0: no river cost
    CHECK(got[2] == std::vector<ppl::Nb>{{1, 1}, {3, 3}});  // pass only: pass_cost 3
    CHECK(got[3] == std::vector<ppl::Nb>{{2, 3}});          // 3-4 is a barrier
    CHECK(got[4] == std::vector<ppl::Nb>{{5, 3}});
    CHECK(got[5] == std::vector<ppl::Nb>{{1, 1}, {4, 3}});
    // 31 river pairs: 496000 < 512000, no river cost
    g.borders.back().river_pairs = 31;
    PeoplesConfig cfg2(shipped());
    const PeoplesConfig* p2 = &cfg2;
    std::vector<ppl::Nb> four;
    once_at(cfg2, Step::t2, 0, [&](eng::StepContext&) { four = ppl::neighbours(p2->view(), 4); });
    (void)run_with(cfg2, shipped(), inputs_of(g, 0, s));
    CHECK(four == std::vector<ppl::Nb>{{5, 1}});
}

TEST_CASE("P3-05 7.1, 7.2, 16.1 item 5: equal scores order homes by the keyed draw, then id; explicit 4 on separated regions") {
    std::vector<db::RegionRow> rs;
    for (std::int64_t i = 0; i < 8; ++i) {
        db::RegionRow r = plain_region(i + 1, 8 * i, 0, 8 * i + 7, 63);
        r.cells = 20000;
        r.walk_cells = 20000;
        rs.push_back(r);
    }
    const Geo g = hand_made(64, rs);
    Settings s;
    s.span = 2000;
    s.races = {"elf"};
    s.civs = 4;
    const eng::HistoryInputs in = inputs_of(g, 0, s);
    const Tables t = eng::history_stage(shipped(), in);
    const std::uint64_t key = worldgen::stream_key(0, "hist.peoples.home");
    std::vector<std::int64_t> order = {1, 2, 3, 4, 5, 6, 7, 8};
    std::sort(order.begin(), order.end(), [&](std::int64_t a, std::int64_t b) {
        const std::uint64_t da = worldgen::history::draw(key, 0, 1, static_cast<std::uint64_t>(a), 0);
        const std::uint64_t db_ = worldgen::history::draw(key, 0, 1, static_cast<std::uint64_t>(b), 0);
        return da != db_ ? da < db_ : a < b;
    });
    // no border: every other home is unreachable, which counts as far (7.2)
    REQUIRE(t.civilization.size() >= 4U);
    for (std::size_t i = 0; i < 4; ++i) {
        CHECK(at(t.civilization[i], 4) == order[i]);
    }
    // home_min_hops: a chain of walkable borders; two homes at least 3 hops apart
    Geo chain = hand_made(64, std::vector<db::RegionRow>(rs.begin(), rs.begin() + 4));
    for (std::int64_t i = 1; i < 4; ++i) {
        chain.borders.push_back(db::RegionBorderRow{i, i + 1, 1000, 0, 0, 0, 1, 0, 0, 0, 0});
    }
    s.civs = 2;
    const Tables t2 = eng::history_stage(shipped(), inputs_of(chain, 0, s));
    if (t2.civilization.size() == 2U) {
        CHECK(std::abs(at(t2.civilization[0], 4) - at(t2.civilization[1], 4)) >= 3);
    }
}

TEST_CASE("P3-05 7.1: a high-score region below home_min_capacity is skipped; nothing eligible falls back") {
    db::RegionRow small = plain_region(1, 0, 0, 7, 63);
    small.cells = 1200;  // not an islet (1172), but C 400 < 1500
    small.walk_cells = 100;
    small.fertility_mean = small.forage_mean = small.timber_mean = 255;
    db::RegionRow big = plain_region(2, 8, 0, 63, 63);
    big.cells = 20000;
    big.walk_cells = 20000;
    Geo g = hand_made(64, {small, big});
    g.region_biomes = {{1, 3, 1200}, {2, 4, 20000}};
    Settings s;
    s.span = 2000;
    s.races = {"elf"};
    s.civs = 1;
    CHECK(at(eng::history_stage(shipped(), inputs_of(g, 0, s)).civilization.at(0), 4) == 2);
    const wd::ContentSet c = with_peoples({{"home_min_capacity", I64(1000000)}});
    CHECK(at(eng::history_stage(c, inputs_of(g, 0, s, c)).civilization.at(0), 4) == 1);  // the first free non-islet region
}

TEST_CASE("P3-05 6.2 item 4: no settlement site for an awakening is fatal") {
    db::RegionRow r = plain_region(1, 0, 0, 15, 15);
    r.cells = 5000;
    Geo g = hand_made(16, {r});
    std::fill(g.water.begin(), g.water.end(), static_cast<std::uint8_t>(1));  // every cell a river: no candidate
    std::fill(g.river.begin(), g.river.end(), static_cast<std::uint8_t>(3));
    const std::string m = fatal_of([&] { (void)eng::history_stage(shipped(), inputs_of(g, 0, humans_2000())); });
    CHECK(m == "history: peoples: no settlement site in region 1");
}

TEST_CASE("P3-05 8.5 r1, 13: ruin(s, e) applies items 2-5 with e and does not end the civilization") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, humans_2000(), quiet_content());
    PeoplesConfig cfg(quiet_content(), true);
    const PeoplesConfig* p = &cfg;
    std::int64_t home = 0, other = 0, s2 = 0, e_ruin = 0, e_ruin2 = 0;
    std::int64_t p1 = 0, p2 = 0, c2 = 0;
    once_at(cfg, Step::t6, 1, [&](eng::StepContext& ctx) {
        home = ctx.state().civ(1).home_region;
        other = 3 - home;
        const db::RegionRow& r = in.regions[static_cast<std::size_t>(other - 1)];
        const std::int64_t e = test_event(ctx, other, 0);
        s2 = ctx.state().found_settlement(r.seed_x, r.seed_y, other, 1, eng::SettlementKind::village, 500, "Test Hold", e);
        ctx.state().set_region_owner(other, 1, e);
    });
    once_at(cfg, Step::t6, 3, [&](eng::StepContext& ctx) {
        const std::int64_t s1 = ctx.state().civ(1).capital;
        REQUIRE(s1 == 1);
        p1 = ctx.state().settlement(1).population;
        p2 = ctx.state().settlement(s2).population;
        c2 = p->view().capacity(other, 5);
        e_ruin = test_event(ctx, home, 1);
        cfg.peoples.sub->ruin(ctx, 1, e_ruin);
        const eng::Settlement& a = ctx.state().settlement(1);
        CHECK(a.status == eng::SettlementStatus::ruined);
        CHECK(a.ruin_event == e_ruin);
        CHECK(a.ruined_year == std::optional<std::int64_t>(3));
        CHECK(a.owner_civ == 0);
        CHECK(a.population == 0);
        CHECK(ctx.state().settlement(s2).population == p2 + std::min(p1, std::max<std::int64_t>(0, 2 * c2 - p2)));
        CHECK(ctx.state().civ(1).capital == s2);              // item 5
        CHECK(ctx.state().region(home).owner == 0);           // item 4
        CHECK(ctx.state().region(home).owner_event == e_ruin);
        CHECK_FALSE(ctx.state().civ(1).ended_year.has_value());
    });
    once_at(cfg, Step::t6, 5, [&](eng::StepContext& ctx) {
        e_ruin2 = test_event(ctx, other, s2);
        cfg.peoples.sub->ruin(ctx, s2, e_ruin2);
        CHECK(ctx.state().settlement(s2).status == eng::SettlementStatus::ruined);
        CHECK(ctx.state().region(other).owner == 0);
        CHECK_FALSE(ctx.state().civ(1).ended_year.has_value());  // not item 6: the caller's
        CHECK(ctx.state().civ(1).capital == s2);                  // no other settlement: no move
    });
    const Tables t = run_with(cfg, quiet_content(), in);
    // capital_moved after the first ruin, caused by it
    std::vector<const Row*> moved, ended, abandoned;
    for (const Row& e : t.event) {
        if (at(e, 3) == k_capital_moved) moved.push_back(&e);
        if (at(e, 3) == k_civ_ended) ended.push_back(&e);
        if (at(e, 3) == k_settlement_abandoned) abandoned.push_back(&e);
    }
    CHECK(abandoned.empty());  // ruin appends no settlement_abandoned (item 1 is abandonment's)
    REQUIRE(moved.size() == 1U);
    CHECK(at(*moved[0], 1) == 3);
    CHECK(at(*moved[0], 5) == other);
    CHECK(at(*moved[0], 6) == s2);
    // the civilization ends at the next T1 (9.1), caused by the ruin event of its settlement ruined last
    REQUIRE(ended.size() == 1U);
    CHECK(at(*ended[0], 1) == 6);
    CHECK(at(*ended[0], 5) == other);
    std::vector<std::int64_t> moved_causes, ended_causes;
    for (const Row& c : t.event_cause) {
        if (at(c, 0) == at(*moved[0], 0)) moved_causes.push_back(at(c, 1));
        if (at(c, 0) == at(*ended[0], 0)) ended_causes.push_back(at(c, 1));
    }
    CHECK(moved_causes == std::vector<std::int64_t>{e_ruin});
    CHECK(ended_causes == std::vector<std::int64_t>{e_ruin2});
    CHECK(at(t.civilization[0], 8) == 6);
    CHECK(at(t.civilization[0], 9) == at(*ended[0], 0));
    // both ruins fade within the span (1500 years, quiet steps from 50): the rows are cleared (lead's
    // ruling 3) and each ruin_faded is caused by the ruin event the hook was given
    std::set<std::int64_t> fade_causes;
    for (const Row& e : t.event) {
        if (at(e, 3) != k_ruin_faded) continue;
        for (const Row& c : t.event_cause) {
            if (at(c, 0) == at(e, 0)) fade_causes.insert(at(c, 1));
        }
    }
    CHECK(fade_causes == std::set<std::int64_t>{e_ruin, e_ruin2});
    CHECK(at(t.settlement[0], 10) == faded);
    CHECK(at(t.settlement[0], 13) == 0);
}

TEST_CASE("P3-05 13, 8.5 (b): note_reduction makes P3-06's event the cause of a later abandonment in T1") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, humans_2000(), quiet_content());
    PeoplesConfig cfg(quiet_content(), true);
    std::int64_t e_cas = 0;
    once_at(cfg, Step::t6, 2, [&](eng::StepContext& ctx) {
        ctx.state().set_population(1, 10);
        e_cas = test_event(ctx, ctx.state().settlement(1).region, 1);
        cfg.peoples.sub->note_reduction(1, e_cas);
        CHECK(cfg.view().last_reduction(1) == e_cas);
    });
    const Tables t = run_with(cfg, quiet_content(), in);
    const Row* ab = nullptr;
    const Row* end = nullptr;
    for (const Row& e : t.event) {
        if (at(e, 3) == k_settlement_abandoned) ab = &e;
        if (at(e, 3) == k_civ_ended) end = &e;
    }
    REQUIRE(ab != nullptr);
    CHECK(at(*ab, 1) == 3);  // T1 of the next year, after growth: 10 + max(1, d) = 11 < 40
    std::vector<std::int64_t> cs;
    std::int64_t survivors = -1;
    for (const Row& c : t.event_cause) {
        if (at(c, 0) == at(*ab, 0)) cs.push_back(at(c, 1));
    }
    for (const Row& v : t.event_value) {
        if (at(v, 0) == at(*ab, 0)) survivors = at(v, 2);
    }
    CHECK(cs == std::vector<std::int64_t>{e_cas});
    CHECK(survivors == 11);
    // its only settlement: the civilization ends with it (item 6), regions freed
    REQUIRE(end != nullptr);
    CHECK(at(*end, 1) == 3);
    CHECK(at(t.civilization[0], 8) == 3);
    for (const Row& rc : t.region_control) {
        CHECK_FALSE(is_null(rc[2]));
    }
}

TEST_CASE("P3-05 14 item 10, 16.2 item 19 (Y3): extra draws on every hist.peoples stream change nothing") {
    for (const char* name : {"syn-7", "stress-3"}) {
        const GCase& g = golden_case(name);
        CAPTURE(g.name);
        const eng::HistoryInputs in = synthetic_inputs(g);
        const wd::ContentSet& c = content_of(g);
        PeoplesConfig base(c);
        const Tables want = run_with(base, c, in);
        PeoplesConfig extra(c);
        std::uint64_t sink = 0;
        extra.before.push_back(std::make_unique<FnSubsystem>(
            std::vector<Step>{Step::t1, Step::t2, Step::t4, Step::q1, Step::q3, Step::q4}, [&](eng::StepContext& ctx) {
                for (const std::string_view s : ppl::stream_names()) {
                    const std::uint64_t key = worldgen::stream_key(ctx.inputs().seed, s);
                    for (std::uint64_t id = 0; id < 4; ++id) {
                        sink ^= ctx.draw(key, id, id + 1, 0);
                    }
                }
            }));
        const Tables got = run_with(extra, c, in);
        CHECK(sink != 0);
        CHECK(got == want);
    }
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
