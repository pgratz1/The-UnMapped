// Spec P3-05 r12 22.8 (generator 0.19.0, P3-10, docs/specs/p3-10-followups.md): unit tests through the
// engine's public seam on hand-made geography, and properties on the golden worlds:
//   22.1  kind 37 plague_outbreak with dead, population, total and reached, its salience and sentence;
//   22.2  the cause memory: a dwindling 49, 50 and 51 years after a noted reduction cites it, it, nothing;
//         every dwindling that cites a colony cites one it sent within 50 years (golden worlds);
//   22.3, 22.4  the templates of kinds 10, 15, 16, 17 and 31 (shipped data);
//   22.5  the site value: a faerie in a coastal region settles inland (not the seed cell); a dwarf prefers a
//         hill cell of habitability 60 and height 255 to a shore cell of habitability 200 (183 against 100);
//   22.7a lie_low: no colony while lying low (PeoplesSubsystem::lie_low);
// (P3-10 4 item 2, the dwarven forts and ladder kinds of hist-peoples-42-1024, is read from the golden file by
// history_peoples_golden_test.cpp; the sentence of kind 37 is rendered in history_engine_r12_test.cpp.)
// Written from the spec only, before the implementation (test-writer rule). Every case but the lie_low one
// compiles against the 0.18.0 library and fails there until the P3-10 implementation is merged. The
// reference (reference/p3_peoples_reference.py, check_units_r12) checks the same cases on its own.

#include "p3_05_peoples_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
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

std::size_t cell(std::int64_t x, std::int64_t y) { return static_cast<std::size_t>(y * 64 + x); }

Settings race_span(const char* race, std::int64_t span) {
    Settings s;
    s.span = span;
    s.races = {race};
    s.civs = 1;
    return s;
}

const wd::ContentSet& quiet_content() {
    static const wd::ContentSet c = with_peoples({{"found_rate_permille", I64(0)},
                                                  {"famine_ppm", I64(0)},
                                                  {"plague_ppm", by_kind(0, 0, 0, 0, 0)},
                                                  {"crisis_per_generation_permille", I64(0)}});
    return c;
}

// Plague at every settlement every year, toll 300 per mille, spread certain (or never), no famine or crisis.
const wd::ContentSet& plague_content(bool spread) {
    static const wd::ContentSet with = with_peoples({{"found_rate_permille", I64(0)},
                                                     {"famine_ppm", I64(0)},
                                                     {"plague_ppm", by_kind(1000000, 1000000, 1000000, 1000000, 1000000)},
                                                     {"plague_toll_min", I64(300)},
                                                     {"plague_toll_max", I64(300)},
                                                     {"plague_spread_permille", I64(1000)},
                                                     {"crisis_per_generation_permille", I64(0)}});
    static const wd::ContentSet without = with_peoples({{"found_rate_permille", I64(0)},
                                                        {"famine_ppm", I64(0)},
                                                        {"plague_ppm", by_kind(1000000, 1000000, 1000000, 1000000, 1000000)},
                                                        {"plague_toll_min", I64(300)},
                                                        {"plague_toll_max", I64(300)},
                                                        {"plague_spread_permille", I64(0)},
                                                        {"crisis_per_generation_permille", I64(0)}});
    return spread ? with : without;
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
std::vector<const Row*> events_of(const Tables& t, std::int64_t kind) {
    std::vector<const Row*> out;
    for (const Row& e : t.event) {
        if (at(e, 3) == kind) out.push_back(&e);
    }
    return out;
}
// The peoples-only world of a golden case (as history_peoples_golden_test.cpp's world_path).
std::string world_of_case(const GCase& g) {
    chrono_api::Params p = chrono_test::params_with(g.span, chrono_test::races_of_text(g.races));
    p.detail = g.detail;
    return peoples_world(g.name, g.seed, g.name == "geo-phrase-512" ? "the unmapped" : "", g.size, p);
}
std::string template_of(const char* key) {
    const eng::EventCatalogue c = eng::catalogue_of(shipped());
    const eng::KindDef* k = c.find(key);
    REQUIRE(k != nullptr);
    return k->templ;
}

const std::string M =
    "famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,battle=war,"
    "battle_at_pass=war,tribute=tribute";

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-05 r12 22.3, 22.4, 22.1: the shipped templates of kinds 10, 15, 16, 17, 31 and 37") {
    CHECK(template_of("settlement_dwindled") ==
          "{@causes:" + M + ",colony_founded=emigration|Weakened by |, }{settlement} dwindled to {population} souls.");
    CHECK(template_of("settlement_abandoned") ==
          "{@causes:" + M +
              "|Weakened by |, }{settlement} was abandoned; {survivors?no one survived|its one survivor was "
              "scattered|its # survivors were scattered}.");
    CHECK(template_of("settlement_abandoned_to") ==
          "{@causes:" + M +
              "|Weakened by |, }{settlement} was abandoned; {settled} {settled?survivor|survivors} went to "
              "{destination}{lost?|, and one was lost|, and # were lost}.");
    CHECK(template_of("civ_seceded") ==
          "{civilization} broke away from {from} with {settlements} {settlements?settlement|settlements}.");
    CHECK(template_of("capital_moved") == "The seat of {civilization} moved to {settlement}.");
    CHECK(template_of("plague_outbreak") ==
          "Plague broke out in {settlement}{reached?| and spread to one other settlement| and spread to # other "
          "settlements}; {total} died{reached?| in all| in all}.");
}

TEST_CASE("P3-05 r12 22.1, 22.8 item 1: an outbreak with no spread is kind 37, total = dead, reached 0, salience 25") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, race_span("human", 2000), plague_content(false));
    PeoplesConfig cfg(plague_content(false));
    const Tables t = eng::run_engine(plague_content(false), in, cfg.get());
    CHECK(events_of(t, k_plague).empty());
    const auto ob = events_of(t, k_plague_outbreak);
    REQUIRE_FALSE(ob.empty());
    const Row& e = *ob[0];
    const auto v = values_of(t, at(e, 0));
    REQUIRE(v.size() == 4U);
    // the toll is 300 permille of the population at the strike (which has grown from the founding 300)
    CHECK(v.at(1) == (v.at(1) + v.at(2)) * 300 / 1000);
    CHECK(v.at(3) == v.at(1));  // total
    CHECK(v.at(4) == 0);     // reached
    CHECK(at(e, 7) == 25);
}

TEST_CASE("P3-05 r12 22.1, 22.8 item 1: an outbreak that spreads carries its total and reached; invariant 13") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, race_span("human", 2000), plague_content(true));
    PeoplesConfig cfg(plague_content(true), true);
    std::int64_t s2 = 0;
    cfg.add_after({Step::t6}, [&](eng::StepContext& ctx) {
        if (s2 != 0) return;
        const std::int64_t other = 3 - ctx.state().civ(1).home_region;
        const db::RegionRow& r = in.regions[static_cast<std::size_t>(other - 1)];
        const std::int64_t e = test_event(ctx, other, 0);
        s2 = ctx.state().found_settlement(r.seed_x, r.seed_y, other, 1, eng::SettlementKind::town, 6000, "Second", e);
        ctx.state().set_region_owner(other, 1, e);
    });
    const Tables t = eng::run_engine(plague_content(true), in, cfg.get());
    REQUIRE(s2 != 0);
    std::map<std::int64_t, std::int64_t> spread_dead, spread_n;
    for (const Row* e : events_of(t, k_plague_spread)) {
        const auto cs = causes_of(t, at(*e, 0));
        REQUIRE(cs.size() == 1U);
        spread_dead[cs[0]] += values_of(t, at(*e, 0)).at(1);
        spread_n[cs[0]] += 1;
    }
    std::int64_t with_spread = 0;
    for (const Row* e : events_of(t, k_plague_outbreak)) {
        const std::int64_t id = at(*e, 0);
        const auto v = values_of(t, id);
        CAPTURE(id);
        CHECK(v.at(3) == v.at(1) + spread_dead[id]);
        CHECK(v.at(4) == spread_n[id]);
        CHECK(at(*e, 7) == (v.at(3) >= 1000 ? 45 : 25));
        if (v.at(4) >= 1) ++with_spread;
    }
    // Integration (P3-10): this hand-made fixture produces no spread with the shipped stage (the settlement the
    // test founds through State is outside peoples' own lists), so the spreading case is checked on the golden
    // and stress worlds (invariant 13 with total, p3_05_peoples_support.hpp); here only the rows' agreement.
    MESSAGE("outbreaks with a spread in the fixture: " << with_spread);
}

TEST_CASE("P3-05 r12 22.2, 22.8 item 2: a dwindling 49, 50 and 51 years after a noted reduction") {
    const Geo g = two_regions();
    const eng::HistoryInputs in = inputs_of(g, 0, race_span("human", 6000), quiet_content());
    for (const std::int64_t d : {std::int64_t{49}, std::int64_t{50}, std::int64_t{51}}) {
        CAPTURE(d);
        PeoplesConfig cfg(quiet_content(), true);
        PeoplesConfig* p = &cfg;
        std::int64_t s2 = 0, red = 0;
        cfg.add_after({Step::t6}, [&](eng::StepContext& ctx) {
            const std::int64_t other = 3 - ctx.state().civ(1).home_region;
            if (ctx.year() == 1) {
                const db::RegionRow& r = in.regions[static_cast<std::size_t>(other - 1)];
                const std::int64_t e = test_event(ctx, other, 0);
                s2 = ctx.state().found_settlement(r.seed_x, r.seed_y, other, 1, eng::SettlementKind::town, 3000,
                                                  "Glathenddell", e);
                ctx.state().set_region_owner(other, 1, e);
            }
            if (ctx.year() == 5) {
                red = test_event(ctx, other, s2);
                p->peoples.sub->note_reduction(s2, red);  // P3-05 13: the seam P3-06's battles use
            }
        });
        cfg.add_before({Step::t1}, [&](eng::StepContext& ctx) {
            if (ctx.year() == 5 + d) ctx.state().set_population(s2, 1000);  // below 800 per mille of town_min
        });
        const Tables t = eng::run_engine(quiet_content(), in, cfg.get());
        REQUIRE(red != 0);
        const Row* dw = nullptr;
        for (const Row* e : events_of(t, k_settlement_dwindled)) {
            if (at(*e, 6) == s2 && dw == nullptr) dw = e;
        }
        REQUIRE(dw != nullptr);
        CHECK(at(*dw, 1) == 5 + d);
        CHECK(causes_of(t, at(*dw, 0)) == (d <= 50 ? std::vector<std::int64_t>{red} : std::vector<std::int64_t>{}));
    }
}

TEST_CASE("P3-05 r12 22.5, 22.8 item 4: a faerie in a coastal region settles inland, not at the seed cell") {
    // the sea is column 0; every land cell of region 1 (x >= 1) has habitability 200 - 4 x: the shore is the
    // most habitable. Faerie (inland 400, port 0, inland_cap 20): v = 200 - 4 x + 400 min(20, x) 255 div 20000,
    // largest at x = 20 (120 + 102 = 222), ties to the smaller index: (20, 0). Before 0.19.0 the faerie had no
    // cell term and took the seed cell (2, 1).
    db::RegionRow a = plain_region(1, 1, 0, 63, 63);
    a.cells = 4032;
    a.walk_cells = 4000;
    a.coast_cells = 64;
    Geo g = hand_made(64, {a});
    for (std::int64_t y = 0; y < 64; ++y) {
        g.water[cell(0, y)] = 3;
        for (std::int64_t x = 1; x < 64; ++x) g.hab[cell(x, y)] = static_cast<std::uint8_t>(std::max<std::int64_t>(0, 200 - 4 * x));
    }
    g.region_biomes = {{1, 3, 4032}};  // temperate forest
    const Tables t = eng::history_stage(quiet_content(), inputs_of(g, 0, race_span("faerie", 2000), quiet_content()));
    REQUIRE_FALSE(t.settlement.empty());
    CHECK(at(t.settlement[0], 2) == 20);
    CHECK(at(t.settlement[0], 3) == 0);
}

TEST_CASE("P3-05 r12 22.5, 22.8 item 4: a dwarf prefers the hill (v 183) to the shore (v 100)") {
    // dwarf (ore 400, height 600, port 0, habitability 500): the hill (30, 30) habitability 60 at the region's
    // elev_max_dm (hgt 255): 30 + 153 = 183; the shore cell (1, 5) habitability 200 next to the sea (port bonus
    // dropped by port 0), elevation 1 (hgt 0): 100. Before 0.19.0: 213 against 200 + 16 = 216, the shore.
    db::RegionRow a = plain_region(1, 1, 0, 63, 63);
    a.cells = 4032;
    a.walk_cells = 4000;
    a.elev_max_dm = 3000;
    a.seed_x = 1;
    a.seed_y = 5;
    Geo g = hand_made(64, {a});
    for (std::int64_t y = 0; y < 64; ++y) g.water[cell(0, y)] = 3;
    g.hab[cell(30, 30)] = 60;
    g.elevation[cell(30, 30)] = 3000;
    g.hab[cell(1, 5)] = 200;
    g.region_biomes = {{1, 4, 4032}};
    const Tables t = eng::history_stage(quiet_content(), inputs_of(g, 0, race_span("dwarf", 2000), quiet_content()));
    REQUIRE_FALSE(t.settlement.empty());
    CHECK(at(t.settlement[0], 2) == 30);
    CHECK(at(t.settlement[0], 3) == 30);
}

TEST_CASE("P3-05 r12 22.7a, 22.8 item 4a: after lie_low(c, Y + 300) c founds no colony before Y + 300") {
    const Geo g = two_regions();
    const wd::ContentSet& c = stress_content();  // found_rate 1000: a colony as soon as the pressure allows
    const eng::HistoryInputs in = inputs_of(g, 0, race_span("human", 6000), c);
    PeoplesConfig cfg(c);
    PeoplesConfig* p = &cfg;
    cfg.add_after({Step::t6}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 1) {
            p->peoples.sub->lie_low(1, 301);
            ctx.state().set_population(1, 20000);  // the pressure far above found_pressure_permille
        }
    });
    const Tables t = eng::run_engine(c, in, cfg.get());
    for (const Row* e : events_of(t, k_colony_founded)) {
        CHECK_MESSAGE(at(*e, 1) >= 301, "colony e" << at(*e, 0) << " in " << at(*e, 1) << " while lying low");
    }
}

TEST_CASE("P3-05 r12 22.2: every dwindling that cites a colony cites one it sent within 50 years (geo-seed-42-512)") {
    const GCase& g = golden_case("geo-seed-42-512");
    const Tables t = tables_of_world(open_world(world_of_case(g)));
    std::map<std::int64_t, const Row*> ev;
    for (const Row& r : t.event) ev[at(r, 0)] = &r;
    std::map<std::int64_t, std::map<std::int64_t, std::int64_t>> part;
    for (const Row& p : t.event_participant) part[at(p, 0)][at(p, 2)] = at(p, 4);
    for (const Row* d : events_of(t, k_settlement_dwindled)) {
        for (const std::int64_t c : causes_of(t, at(*d, 0))) {
            if (at(*ev.at(c), 3) != k_colony_founded) continue;
            CAPTURE(at(*d, 0));
            CHECK(part[c].at(3) == at(*d, 6));                       // its origin is the dwindled settlement
            CHECK(at(*d, 1) - at(*ev.at(c), 1) <= cause_memory_years);
        }
    }
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
