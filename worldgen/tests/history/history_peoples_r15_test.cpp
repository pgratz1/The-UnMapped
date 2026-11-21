// Spec P3-05 r15 22.7a and 22.8 item 4a (P3-10 second round; P3-06 r17 22.3): one definition of lying low, held by
// peoples. The hook lie_low(civ, settlement, until_year) records the latest (refuge, until) per civilization, and
// lying_low(state, civ, year) is true while year < until and the refuge stands and is the civilization's. Through
// the engine's public seam on a hand-made geography (four regions in a row, humans with found_rate 1000):
//   - lying low: no colony while the refuge stands and is the civilization's, before until;
//   - the refuge ruined, or passed to another civilization, before until while the civilization stands elsewhere:
//     foundings resume in the next step; the new owner does not lie low;
//   - lying_low agrees with what peoples does in every step of the run (a probe reads it after peoples).
// The orc's 100 years (P3-06 r17 22.2) is the flight's until; peoples reads only the year it is given (the
// reference's check_units_r12 and the conflict golden worlds cover the flight itself).
//
// Written from the spec before the implementation (test-writer rule). The hook and the predicate are the round's
// new API: the file compiles to nothing until UNMAPPED_P3_10_R3_API is 1 (p3_10_r3_switch.hpp).

#include "../p3_10_r3_switch.hpp"
#include "p3_05_peoples_support.hpp"

#if UNMAPPED_HAVE_PEOPLES && UNMAPPED_P3_10_R3_API

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
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

// Four regions of 32 x 128 cells in a row, walkable borders, grassland.
Geo four_regions() {
    std::vector<db::RegionRow> rs;
    for (std::int64_t i = 0; i < 4; ++i) {
        db::RegionRow r = plain_region(i + 1, 32 * i, 0, 32 * i + 31, 127);
        r.cells = 4096;
        r.walk_cells = 4000;
        rs.push_back(r);
    }
    Geo g = hand_made(128, rs);
    for (std::int64_t i = 1; i < 4; ++i) g.borders.push_back(db::RegionBorderRow{i, i + 1, 2048, 0, 0, 0, 10, 0, 0, 0, 0});
    for (std::int64_t i = 1; i <= 4; ++i) g.region_biomes.push_back({i, 4, 4096});
    return g;
}

Settings humans(std::int64_t civs) {
    Settings s;
    s.span = 6000;
    s.races = {"human"};
    s.civs = civs;
    return s;
}

enum class Release { none, ruined, passed };

struct LowRun {
    Tables t;
    std::int64_t h = 0;
    std::map<std::int64_t, bool> low1, low2;  // year -> lying_low(state, c, year) after peoples, at T6
};

// Civilization 1 founds a refuge h in a free region at year 1 (T6), lie_low(1, h, 301); its capital is crowded at
// years 1 and 50. At year 50 (T6) h is ruined, passed to civilization 2, or left as it is.
LowRun low_run(Release how) {
    const Geo g = four_regions();
    const wd::ContentSet& c = stress_content();
    const eng::HistoryInputs in = inputs_of(g, 0, humans(how == Release::passed ? 2 : 1), c);
    PeoplesConfig cfg(c, true);
    PeoplesConfig* p = &cfg;
    LowRun r;
    cfg.add_after({Step::t6}, [&, p](eng::StepContext& ctx) {
        auto& st = ctx.state();
        if (ctx.year() == 1) {
            std::int64_t free = 0;
            for (std::int64_t rg = 4; rg >= 1 && free == 0; --rg) {
                if (st.region(rg).owner == 0) free = rg;
            }
            REQUIRE(free != 0);
            const db::RegionRow& rr = in.regions[static_cast<std::size_t>(free - 1)];
            const std::int64_t e = test_event(ctx, free, 0);
            r.h = st.found_settlement(rr.seed_x + 4, rr.seed_y + 4, free, 1, eng::SettlementKind::village, 500,
                                      "Test Refuge", e);
            st.set_region_owner(free, 1, e);
            p->peoples.sub->lie_low(1, r.h, 301);
        }
        if (ctx.year() == 1 || ctx.year() == 50) st.set_population(st.civ(1).capital, 20000);
        if (ctx.year() == 50 && how != Release::none) {
            const std::int64_t e = test_event(ctx, st.settlement(r.h).region, r.h);
            if (how == Release::ruined) {
                st.set_settlement_status(r.h, eng::SettlementStatus::ruined, e);
                st.set_region_owner(st.settlement(r.h).region, 0, e);
            } else {
                st.set_settlement_owner(r.h, 2, e);
                st.set_region_owner(st.settlement(r.h).region, 2, e);
            }
        }
        if (ctx.year() >= 1 && ctx.year() <= 320) {
            r.low1[ctx.year()] = p->peoples.sub->lying_low(st, 1, ctx.year());
            if (how == Release::passed) r.low2[ctx.year()] = p->peoples.sub->lying_low(st, 2, ctx.year());
        }
    });
    r.t = eng::run_engine(c, in, cfg.get());
    return r;
}

std::vector<std::int64_t> colony_years_of(const Tables& t, std::int64_t civ) {
    std::map<std::int64_t, std::int64_t> founder;
    for (const Row& p : t.event_participant) {
        if (at(p, 2) == 2) founder[at(p, 0)] = at(p, 4);
    }
    std::vector<std::int64_t> out;
    for (const Row& e : t.event) {
        if (at(e, 3) == k_colony_founded && founder[at(e, 0)] == civ) out.push_back(at(e, 1));
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-05 r15 22.7a (seam): lie_low(civ, settlement, until_year) and lying_low(const State&, civ, year) const") {
    namespace h = worldgen::history;
    static_assert(std::is_same_v<decltype(&h::PeoplesSubsystem::lie_low),
                                 void (h::PeoplesSubsystem::*)(std::int64_t, std::int64_t, std::int64_t)>);
    CHECK(true);
}

TEST_CASE("P3-05 r15 22.8 item 4a: with the refuge standing and its own, no colony before until; lying_low true") {
    const LowRun r = low_run(Release::none);
    REQUIRE(r.h != 0);
    for (const std::int64_t y : colony_years_of(r.t, 1)) {
        CHECK_MESSAGE((y < 2 || y >= 301), "colony of c1 in " << y << " while lying low");
    }
    for (const auto& [y, low] : r.low1) {
        CAPTURE(y);
        CHECK(low == (y < 301));
    }
}

TEST_CASE("P3-05 r15 22.8 item 4a: the refuge ruined at 50: foundings resume in the next step; lying_low false") {
    const LowRun r = low_run(Release::ruined);
    REQUIRE(r.h != 0);
    bool resumed = false;
    for (const std::int64_t y : colony_years_of(r.t, 1)) {
        CHECK_MESSAGE((y < 2 || y > 50), "colony of c1 in " << y << " while lying low");
        resumed = resumed || (y > 50 && y < 301);
    }
    CHECK(resumed);
    for (const auto& [y, low] : r.low1) {
        CAPTURE(y);
        CHECK(low == (y < 50));
    }
}

TEST_CASE("P3-05 r15 22.8 item 4a: the refuge passed to c2 at 50: c1 founds again; c2 does not lie low") {
    const LowRun r = low_run(Release::passed);
    REQUIRE(r.h != 0);
    bool resumed = false;
    for (const std::int64_t y : colony_years_of(r.t, 1)) {
        CHECK_MESSAGE((y < 2 || y > 50), "colony of c1 in " << y << " while lying low");
        resumed = resumed || (y > 50 && y < 301);
    }
    CHECK(resumed);
    for (const auto& [y, low] : r.low1) {
        CAPTURE(y);
        CHECK(low == (y < 50));
    }
    for (const auto& [y, low] : r.low2) CHECK_FALSE(low);
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES && UNMAPPED_P3_10_R3_API
