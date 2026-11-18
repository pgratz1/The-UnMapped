// Worked examples and unit tests of the resources stage (spec P1-05
// sections 2, 5.1 and 10.1 tests 1 to 3 and 5; P1-00 G7 with k = 1, G8).
//
// Written from the spec only. Every example of 5.1 goes through the stage's
// seams (p1_05_resources_api.hpp) and through the test's own functions
// (p1_05_resources_support.hpp); both must give the spec's values. The
// values were also reproduced by reference/p1_resources_reference.py check.

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "p1_05_resources_api.hpp"
#include "p1_05_resources_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"

using namespace resource_test;

namespace {

// fx text as raw (the spec's decimals are parse_decimal values).
std::int64_t D(std::string_view s) { return fx_of(s); }

const resource_api::Tuning& shipped() {
    static const resource_api::Tuning t = tuning();
    return t;
}

std::string fatal_message(const auto& fn) {
    try {
        fn();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

bool starts_with(std::string_view s, std::string_view p) { return s.substr(0, p.size()) == p; }

// A grid of land (water 0, depth 0) with the given elevations; S = elevation.
struct LandGrid {
    Grid g;
    std::vector<std::int16_t> e;
    std::vector<std::uint8_t> water;
    std::vector<std::int64_t> s, s2;
};
LandGrid land(std::int32_t w, std::int32_t h, const std::vector<std::int16_t>& e) {
    LandGrid l{Grid{w, h}, e, std::vector<std::uint8_t>(e.size(), WATER_DRY), {}, {}};
    for (const std::int16_t v : e) {
        l.s.push_back(v);
    }
    l.s2 = biome_test::own_s2(l.g, l.e);
    return l;
}

std::vector<std::uint16_t> ids_of(const LandGrid& l, std::int64_t min_cells) {
    const resource_api::Walk w = resource_api::walk_areas(l.g, l.s, l.water, l.s2, D("0.7002"), min_cells);
    const OwnWalk o = own_walk(l.g, l.s, l.water, l.s2, D("0.7002"), min_cells);
    CHECK(w.ids == o.ids);
    CHECK(w.count == o.kept);
    return w.ids;
}

}  // namespace

// --- Helpers and tangents (5.1 "Helpers") -------------------------------------------------------

TEST_CASE("P1-05 5.1 helpers: ramp, q255 and the G8 tangent") {
    CHECK(resource_api::ramp(D("0.3"), D("0.18"), D("0.47")) == 1777227848);
    CHECK(own_ramp(D("0.3"), D("0.18"), D("0.47")) == 1777227848);
    // ramp's ends: x <= a gives 0, x >= b gives ONE.
    CHECK(resource_api::ramp(D("0.18"), D("0.18"), D("0.47")) == 0);
    CHECK(resource_api::ramp(-ONE, D("0.18"), D("0.47")) == 0);
    CHECK(resource_api::ramp(D("0.47"), D("0.18"), D("0.47")) == ONE);
    CHECK(resource_api::ramp(5 * ONE, D("0.18"), D("0.47")) == ONE);
    struct Q {
        std::int64_t s;
        int want;
    };
    for (const Q q : {Q{HALF, 128}, Q{D("0.762"), 194}, Q{D("0.001"), 0}, Q{D("0.002"), 1}, Q{ONE, 255},
                      Q{-ONE, 0}, Q{2 * ONE, 255}, Q{0, 0}}) {
        CAPTURE(q.s);
        CHECK(int{resource_api::q255(q.s)} == q.want);
        CHECK(int{own_q255(q.s)} == q.want);
    }
    struct T {
        std::int64_t s2, raw;
    };
    for (const T t : {T{0, 0}, T{3318, 773122075}, T{9216, 1288490189}, T{22118, 1996102368}, T{50205, 3007345802},
                      T{102400, ONE}, T{250000, 6710886400}}) {
        CAPTURE(t.s2);
        CHECK(resource_api::tangent(t.s2) == t.raw);
        CHECK(own_tangent(t.s2) == t.raw);
    }
    // walk_slope_max "0.7002" is raw 3007336101.
    CHECK(D("0.7002") == 3007336101);
    CHECK(D("0.1") == 429496730);
    CHECK(D("-0.04") == -171798692);
}

// --- R3 -------------------------------------------------------------------------------------------

TEST_CASE("P1-05 5.1 chamfer to metres: one river cell at (4, 1) of a 9 x 3 grid") {
    const Grid g{9, 3};
    std::vector<std::uint8_t> src(27, 0);
    src[1 * 9 + 4] = 1;
    const std::vector<std::int64_t> m = resource_api::chamfer_metres(g, src);
    const std::vector<std::int64_t> want = {70, 54, 38, 22, 16, 22, 38, 54, 70,  //
                                            64, 48, 32, 16, 0,  16, 32, 48, 64,  //
                                            70, 54, 38, 22, 16, 22, 38, 54, 70};
    CHECK(m == want);
    CHECK(metres_of(own_chamfer(g, src)) == want);
    const std::vector<std::int64_t> raw = own_chamfer(g, src);
    CHECK(std::vector<std::int64_t>(raw.begin(), raw.begin() + 9) ==
          std::vector<std::int64_t>{22, 17, 12, 7, 5, 7, 12, 17, 22});
    // No source: INF saturates to 32767 m.
    CHECK(resource_api::chamfer_metres(g, std::vector<std::uint8_t>(27, 0)) == std::vector<std::int64_t>(27, 32767));
    CHECK(own_metres(INF) == 32767);
    // The seam is fatal for an array of the wrong length.
    CHECK_THROWS_AS((void)resource_api::chamfer_metres(g, std::vector<std::uint8_t>(26, 0)), worldgen::FatalError);
}

TEST_CASE("P1-05 5.1 R3: fresh_water (df, rainfall) with the shipped tuning, every row") {
    struct Row {
        std::int64_t df, rain;
        int want;
    };
    const Values v = values();
    for (const Row r : {Row{0, 800, 255}, Row{16, 800, 245}, Row{38, 800, 231}, Row{64, 800, 214},
                        Row{200, 800, 128}, Row{400, 800, 43}, Row{399, 200, 1}, Row{1000, 1500, 102},
                        Row{1000, 900, 51}, Row{1000, 2500, 102}, Row{1000, 300, 0}}) {
        CAPTURE(r.df);
        CAPTURE(r.rain);
        CHECK(int{resource_api::fresh_water(r.df, r.rain, shipped())} == r.want);
        CHECK(int{own_fresh(r.df, r.rain, v)} == r.want);
    }
    CHECK(own_fresh_parts(16, 800, v).fs == 4123168604);
    CHECK(own_fresh_parts(200, 800, v).fs == HALF);
    CHECK(own_fresh_parts(400, 800, v).fr == 715827883);
    CHECK(own_fresh_parts(399, 200, v).fs == 10737418);
    CHECK(own_fresh_parts(399, 200, v).fr == 0);
}

// --- R4 -------------------------------------------------------------------------------------------

TEST_CASE("P1-05 5.1 R4: fertility (fb, S2, hand) with sf and al") {
    struct Row {
        const char* fb;
        std::int64_t s2, hand;
        int want;
    };
    const Values v = values();
    for (const Row r : {Row{"1", 9216, 20, 176}, Row{"1", 0, 0, 255}, Row{"0.7", 0, 0, 232}, Row{"0.7", 0, 60, 178},
                        Row{"0.7", 22118, 100, 3}, Row{"0.45", 0, 5, 146}, Row{"1", 30000, 0, 0},
                        Row{"0.3", 3318, 49, 77}}) {
        CAPTURE(r.fb);
        CAPTURE(r.s2);
        CAPTURE(r.hand);
        CHECK(int{resource_api::fertility(D(r.fb), r.s2, r.hand, shipped())} == r.want);
        CHECK(int{own_fertility(D(r.fb), r.s2, r.hand, v)} == r.want);
    }
    CHECK(own_fert_parts(9216, 20, v).sf == 2517739448);
    CHECK(own_fert_parts(9216, 20, v).al == 5068061410);
    CHECK(own_fert_parts(0, 0, v).al == 5583457485);
    CHECK(own_fert_parts(0, 60, v).al == ONE);
    CHECK(own_fert_parts(22118, 100, v).sf == 77697452);
    CHECK(own_fert_parts(3318, 49, v).sf == 4294870875);
    CHECK(own_fert_parts(3318, 49, v).al == 4320737100);
}

TEST_CASE("P1-05 5.1 R4: timber (tb, S2)") {
    struct Row {
        const char* tb;
        std::int64_t s2;
        int want;
    };
    const Values v = values();
    for (const Row r : {Row{"1", 9216, 255}, Row{"1", 50205, 255}, Row{"1", 102400, 146}, Row{"0.15", 0, 38},
                        Row{"0.7", 250000, 0}}) {
        CAPTURE(r.tb);
        CAPTURE(r.s2);
        CHECK(int{resource_api::timber(D(r.tb), r.s2, shipped())} == r.want);
        CHECK(int{own_timber(D(r.tb), r.s2, v)} == r.want);
    }
    CHECK(own_timber_factor(50205, v) == 4293726303);
    CHECK(own_timber_factor(102400, v) == 2454267026);
}

TEST_CASE("P1-05 5.1 R4: forage (gb, dg) and the fish term") {
    struct Row {
        const char* gb;
        std::int64_t dg;
        int want;
    };
    const Values v = values();
    for (const Row r : {Row{"0.8", 0, 255}, Row{"0.5", 0, 204}, Row{"0.3", 16, 149}, Row{"0.3", 150, 115},
                        Row{"0.05", 299, 13}, Row{"0.3", 300, 77}, Row{"0.2", 5000, 51}}) {
        CAPTURE(r.gb);
        CAPTURE(r.dg);
        CHECK(int{resource_api::forage(D(r.gb), r.dg, shipped())} == r.want);
        CHECK(int{own_forage(D(r.gb), r.dg, v)} == r.want);
    }
    CHECK(own_fish(0, v) == 1288490189);
    CHECK(own_fish(16, v) == 1219770712);
    CHECK(own_fish(150, v) == 644245095);
    CHECK(own_fish(299, v) == 4294967);
    CHECK(own_fish(300, v) == 0);
}

TEST_CASE("P1-05 5.1 R4: ore (elevation, S2, noise) with eu, su, vn") {
    struct Row {
        std::int64_t e, s2;
        const char* noise;
        int want;
    };
    const Values v = values();
    for (const Row r : {Row{8000, 102400, "0.2", 255}, Row{8000, 102400, "0.1", 128}, Row{500, 0, "0.3", 19},
                        Row{15000, 20000, "0.05", 38}, Row{4500, 9216, "0.15", 48}, Row{8000, 102400, "-0.04", 0}}) {
        CAPTURE(r.e);
        CAPTURE(r.s2);
        CAPTURE(r.noise);
        CHECK(int{resource_api::ore(r.e, r.s2, D(r.noise), shipped())} == r.want);
        CHECK(int{own_ore(r.e, r.s2, D(r.noise), v)} == r.want);
    }
    CHECK(own_ore_parts(8000, 102400, D("0.1"), v).vn == 2147483651);
    const OreParts low = own_ore_parts(500, 0, D("0.3"), v);
    CHECK(low.eu == 644245094);
    CHECK(low.su == HALF);
    CHECK(low.vn == ONE);
    const OreParts hi = own_ore_parts(15000, 20000, D("0.05"), v);
    CHECK(hi.su == 2582937308);
    CHECK(hi.vn == 1073741825);
    const OreParts mid = own_ore_parts(4500, 9216, D("0.15"), v);
    CHECK(mid.eu == HALF);
    CHECK(mid.su == HALF);
    CHECK(mid.vn == 3221225471);
}

TEST_CASE("P1-05 10.1 test 2: the ore noise at 16 cells of seed 0, N = 64, is fbm2 of K_ore at the cell centres") {
    const std::uint64_t key = worldgen::stream_key(0, STREAM_ORE);
    const Values v = values();
    for (const std::int32_t x : {0, 9, 31, 63}) {
        for (const std::int32_t y : {0, 17, 40, 63}) {
            CAPTURE(x);
            CAPTURE(y);
            // fbm2 with base frequency from_ratio(1, 1500), lacunarity 2, gain 0.5, 3 octaves.
            const std::int64_t want =
                worldgen::fbm2(key, Fx::from_int(16 * x + 8), Fx::from_int(16 * y + 8), 3, Fx::from_ratio(1, 1500),
                               Fx::from_int(2), Fx::from_raw(HALF))
                    .raw();
            CHECK(own_noise(key, x, y, v.ore_noise) == want);
            CHECK(resource_api::ore_noise(0, x, y, shipped()) == want);
        }
    }
    // The stream name of P1-00 3.
    CHECK(worldgen::stream_key(0, "geo.resources.ore") == key);
}

// --- R6 -------------------------------------------------------------------------------------------

TEST_CASE("P1-05 5.1 R6: the eight rows of the habitability table, every intermediate") {
    struct Row {
        int f, w, g, t, o;
        std::int64_t s2, hand;
        std::uint16_t walk;
        std::int64_t sc, b, wat, slope;
        bool flood, pocket;
        int want;
    };
    const std::vector<Row> rows = {
        {232, 255, 204, 255, 3, 0, 20, 1, 20695, 3485660713, ONE, ONE, false, false, 207},
        {232, 255, 204, 255, 3, 0, 0, 1, 20695, 3485660713, ONE, ONE, true, false, 103},
        {255, 255, 153, 38, 0, 0, 5, 1, 17655, 2973633240, ONE, ONE, true, false, 88},
        {255, 255, 153, 38, 0, 0, 5, 0, 17655, 2973633240, ONE, ONE, true, true, 22},
        {179, 102, 128, 255, 0, 3318, 300, 1, 16230, 2733620361, 2362232013, 4294870875, false, false, 89},
        {255, 40, 90, 38, 0, 0, 300, 1, 16395, 2761411326, 1579032094, ONE, false, false, 60},
        {170, 13, 89, 13, 0, 0, 300, 2, 11325, 1907470770, 1237961162, ONE, false, false, 33},
        {200, 200, 200, 200, 200, 30000, 300, 1, 20000, 3368601801, 3600193175, 0, false, false, 0},
    };
    const Values v = values();
    for (const Row& r : rows) {
        CAPTURE(r.f);
        CAPTURE(r.w);
        CAPTURE(r.hand);
        CAPTURE(r.walk);
        const HabParts p = own_hab_parts(r.f, r.w, r.g, r.t, r.o, r.s2, r.hand, r.walk, v);
        CHECK(p.sc == r.sc);
        CHECK(p.b == r.b);
        CHECK(p.wat == r.wat);
        CHECK(p.slope == r.slope);
        CHECK(p.flood == r.flood);
        CHECK(p.pocket == r.pocket);
        CHECK(int{own_habitability(r.f, r.w, r.g, r.t, r.o, r.s2, r.hand, r.walk, v)} == r.want);
        CHECK(int{resource_api::habitability(static_cast<std::uint8_t>(r.f), static_cast<std::uint8_t>(r.w),
                                             static_cast<std::uint8_t>(r.g), static_cast<std::uint8_t>(r.t),
                                             static_cast<std::uint8_t>(r.o), r.s2, r.hand, r.walk, shipped())} ==
              r.want);
    }
}

// --- R5 -------------------------------------------------------------------------------------------

TEST_CASE("P1-05 5.1 R5 (a): a 300 dm spike in a 5 x 5 grid is a one-cell component, dropped below 2 cells") {
    std::vector<std::int16_t> e(25, 20);
    e[12] = 300;
    const LandGrid l = land(5, 5, e);
    for (const std::size_t i : {7U, 11U, 13U, 17U}) {
        CHECK(l.s2[i] == 78400);
    }
    CHECK(l.s2[12] == 0);
    std::vector<std::uint16_t> want(25, 1);
    for (const std::size_t i : {7U, 11U, 12U, 13U, 17U}) {
        want[i] = 0;
    }
    CHECK(ids_of(l, 2) == want);
    // With walk_min_cells 1 the centre is its own area, id 2 (its index 12
    // is above the outer area's smallest, 0).
    want[12] = 2;
    CHECK(ids_of(l, 1) == want);
}

TEST_CASE("P1-05 5.1 R5 (b): a 37.7 m step separates two lowlands, ids 1 and 2") {
    const std::vector<std::int16_t> e = {20, 21, 22, 23, 400, 401, 402, 403, 404,  //
                                         20, 21, 22, 23, 400, 401, 402, 403, 404,  //
                                         21, 22, 23, 24, 401, 402, 403, 404, 405,  //
                                         21, 22, 23, 24, 401, 402, 403, 404, 405,  //
                                         22, 23, 24, 25, 402, 403, 404, 405, 406};
    const LandGrid l = land(9, 5, e);
    std::int64_t least = INF;
    for (std::int32_t y = 0; y < 5; ++y) {
        for (const std::int32_t x : {3, 4}) {
            least = std::min(least, l.s2[l.g.index(x, y)]);
        }
    }
    CHECK(least == 142884);
    std::vector<std::uint16_t> want;
    for (std::int32_t y = 0; y < 5; ++y) {
        for (std::int32_t x = 0; x < 9; ++x) {
            want.push_back(static_cast<std::uint16_t>(x <= 2 ? 1 : x <= 4 ? 0 : 2));
        }
    }
    CHECK(ids_of(l, 2) == want);
}

TEST_CASE("P1-05 5.1 R5 (c): the orthogonal step limit, 112 dm linked and 113 not") {
    for (const std::int16_t h : {std::int16_t{112}, std::int16_t{113}, std::int16_t{224}}) {
        CAPTURE(h);
        const LandGrid l = land(4, 1, {1000, 1000, static_cast<std::int16_t>(1000 + h), static_cast<std::int16_t>(1000 + h)});
        CHECK(l.s2 == std::vector<std::int64_t>{0, std::int64_t{h} * h, std::int64_t{h} * h, 0});
        const std::vector<std::uint16_t> want =
            h == 112 ? std::vector<std::uint16_t>{1, 1, 1, 1} : std::vector<std::uint16_t>{1, 1, 2, 2};
        CHECK(ids_of(l, 1) == want);
    }
    // 225 dm: S2 50625 at the two middle cells, not walkable (the step's
    // cells are steeper than 35 deg).
    const LandGrid l = land(4, 1, {1000, 1000, 1225, 1225});
    CHECK(ids_of(l, 1) == std::vector<std::uint16_t>{1, 0, 0, 2});
}

TEST_CASE("P1-05 R5: the diagonal step limit, 158 dm linked and 159 not") {
    // A 2 x 2 grid: the anti-diagonal cells are ocean, so (0, 0) and (1, 1)
    // can only link diagonally. S2 on a 2 x 2 grid doubles the one-sided
    // difference: e' of the ocean cells is 0 by max(e, 0), so give the two
    // land cells the slope test by S2 directly (the seam takes S2).
    for (const std::int64_t d : {158, 159}) {
        CAPTURE(d);
        const Grid g{2, 2};
        const std::vector<std::int64_t> s = {1000, -5, -5, 1000 + d};
        const std::vector<std::uint8_t> water = {WATER_DRY, WATER_OCEAN, WATER_OCEAN, WATER_DRY};
        const std::vector<std::int64_t> s2 = {0, 0, 0, 0};
        const resource_api::Walk w = resource_api::walk_areas(g, s, water, s2, D("0.7002"), 1);
        const std::vector<std::uint16_t> want =
            d == 158 ? std::vector<std::uint16_t>{1, 0, 0, 1} : std::vector<std::uint16_t>{1, 0, 0, 2};
        CHECK(w.ids == want);
        CHECK(w.count == (d == 158 ? 1 : 2));
        CHECK(own_walk(g, s, water, s2, D("0.7002"), 1).ids == want);
        // The same along the other diagonal.
        const std::vector<std::int64_t> s_b = {-5, 1000, 1000 + d, -5};
        const std::vector<std::uint8_t> water_b = {WATER_OCEAN, WATER_DRY, WATER_DRY, WATER_OCEAN};
        const std::vector<std::uint16_t> want_b =
            d == 158 ? std::vector<std::uint16_t>{0, 1, 1, 0} : std::vector<std::uint16_t>{0, 1, 2, 0};
        CHECK(resource_api::walk_areas(g, s_b, water_b, s2, D("0.7002"), 1).ids == want_b);
    }
}

TEST_CASE("P1-05 R5: walkability is S2 <= 50204 at the shipped limit; river cells walk, lakes and ocean do not") {
    const Grid g{6, 1};
    const std::vector<std::int64_t> s(6, 100);
    const std::vector<std::uint8_t> water = {WATER_DRY, WATER_DRY, WATER_RIVER, WATER_LAKE, WATER_OCEAN, WATER_DRY};
    const std::vector<std::int64_t> s2 = {50204, 50205, 0, 0, 0, 0};
    const resource_api::Walk w = resource_api::walk_areas(g, s, water, s2, D("0.7002"), 1);
    CHECK(w.ids == std::vector<std::uint16_t>{1, 0, 2, 0, 0, 3});
    CHECK(w.count == 3);
}

TEST_CASE("P1-05 R5, 10.1 test 3: dropped components leave dense ids ordered by smallest index") {
    // A 12 x 3 strip of land with cliff columns (ocean) at x = 2, 5 and 9:
    // components of 6, 6, 9 and 6 cells (smallest indices 0, 3, 6, 10).
    // With walk_min_cells 7 only the third is kept and gets id 1; with 6
    // all four in index order.
    const Grid g{12, 3};
    std::vector<std::int64_t> s(36, 50);
    std::vector<std::uint8_t> water(36, WATER_DRY);
    for (std::int32_t y = 0; y < 3; ++y) {
        for (const std::int32_t x : {2, 5, 9}) {
            water[g.index(x, y)] = WATER_OCEAN;
        }
    }
    const std::vector<std::int64_t> s2(36, 0);
    auto at = [&](const std::vector<std::uint16_t>& ids, std::int32_t x) { return int{ids[g.index(x, 1)]}; };
    const resource_api::Walk all = resource_api::walk_areas(g, s, water, s2, D("0.7002"), 6);
    CHECK(all.count == 4);
    CHECK(at(all.ids, 0) == 1);
    CHECK(at(all.ids, 3) == 2);
    CHECK(at(all.ids, 6) == 3);
    CHECK(at(all.ids, 11) == 4);
    const resource_api::Walk one = resource_api::walk_areas(g, s, water, s2, D("0.7002"), 7);
    CHECK(one.count == 1);
    CHECK(at(one.ids, 0) == 0);
    CHECK(at(one.ids, 3) == 0);
    CHECK(at(one.ids, 7) == 1);
    CHECK(at(one.ids, 10) == 0);
    CHECK(one.ids == own_walk(g, s, water, s2, D("0.7002"), 7).ids);
    // Nothing kept: every id 0, count 0.
    const resource_api::Walk none = resource_api::walk_areas(g, s, water, s2, D("0.7002"), 10);
    CHECK(none.count == 0);
    CHECK(none.ids == std::vector<std::uint16_t>(36, 0));
}

TEST_CASE("P1-05 R5: more than 65535 walk areas is fatal; 65535 is not") {
    // Isolated walkable cells at even (x, y) of a 512 x 512 grid of ocean:
    // 65536 one-cell components with walk_min_cells 1.
    const Grid g{512, 512};
    std::vector<std::uint8_t> water(g.cells(), WATER_OCEAN);
    for (std::int32_t y = 0; y < 512; y += 2) {
        for (std::int32_t x = 0; x < 512; x += 2) {
            water[g.index(x, y)] = WATER_DRY;
        }
    }
    const std::vector<std::int64_t> s(g.cells(), 10), s2(g.cells(), 0);
    const std::string err =
        fatal_message([&] { (void)resource_api::walk_areas(g, s, water, s2, D("0.7002"), 1); });
    CHECK(err == "resources: more than 65535 walk areas");
    water[g.index(510, 510)] = WATER_OCEAN;
    const resource_api::Walk w = resource_api::walk_areas(g, s, water, s2, D("0.7002"), 1);
    CHECK(w.count == 65535);
    CHECK(w.ids[g.index(508, 510)] == 65535);
}

TEST_CASE("P1-05 R5: the walk_areas seam is fatal for arrays of the wrong length") {
    const Grid g{4, 4};
    const std::vector<std::int64_t> s(16, 0), s2(16, 0), short_s(15, 0);
    const std::vector<std::uint8_t> water(16, 0), short_w(15, 0);
    CHECK_THROWS_AS((void)resource_api::walk_areas(g, short_s, water, s2, D("0.7002"), 1), worldgen::FatalError);
    CHECK_THROWS_AS((void)resource_api::walk_areas(g, s, short_w, s2, D("0.7002"), 1), worldgen::FatalError);
    CHECK_THROWS_AS((void)resource_api::walk_areas(g, s, water, short_s, D("0.7002"), 1), worldgen::FatalError);
}

// --- Preconditions (section 2, 10.1 test 5) through resources_layers -----------------------------

namespace {

// A 6 x 5 world: an ocean border, land inside draining west to the sea, a
// river of class 4 along row 2 (which drains into the sea).
resource_api::Inputs small_world() {
    resource_api::Inputs in;
    in.grid = Grid{6, 5};
    for (std::int32_t y = 0; y < 5; ++y) {
        for (std::int32_t x = 0; x < 6; ++x) {
            const bool sea = x == 0 || y == 0 || y == 4 || x == 5;
            in.elevation.push_back(static_cast<std::int16_t>(sea ? -10 : 10 * x));
            in.water.push_back(sea ? WATER_OCEAN : (y == 2 ? WATER_RIVER : WATER_DRY));
            in.water_depth.push_back(static_cast<std::uint16_t>(sea ? 10 : 0));
            in.flow_dir.push_back(static_cast<std::uint8_t>(sea ? 8 : 4));
            in.river.push_back(static_cast<std::uint8_t>((!sea && y == 2) ? 4 : 0));
            in.rainfall.push_back(800);
            in.biome.push_back(static_cast<std::uint8_t>(sea ? 1 : 4));
        }
    }
    return in;
}

std::string layers_error(const resource_api::Inputs& in) {
    return fatal_message([&] { (void)resource_api::layers(in, definitions(), tuning(), 0, THREADS); });
}

}  // namespace

TEST_CASE("P1-05 2, 10.1 test 5: each precondition is fatal with its message through resources_layers") {
    {
        const resource_api::Inputs in = small_world();
        CHECK(resource_api::layers(in, definitions(), tuning(), 0, THREADS) ==
              own_resources(in, values(), 0).layers);
    }
    {
        resource_api::Inputs in = small_world();
        in.water[1 * 6 + 3] = 4;
        in.water[3 * 6 + 1] = 5;
        CHECK(layers_error(in) == std::string(PRECONDITION) + "water value 4 at (3, 1)");
    }
    {
        resource_api::Inputs in = small_world();
        in.river[2 * 6 + 2] = 16;
        CHECK(layers_error(in) == std::string(PRECONDITION) + "river value 16 at (2, 2)");
    }
    {
        resource_api::Inputs in = small_world();
        in.biome[3 * 6 + 4] = 13;
        in.biome[3 * 6 + 2] = 0;
        CHECK(layers_error(in) == std::string(PRECONDITION) + "biome value 0 at (2, 3) has no definition");
    }
    {  // The order of section 2: water before river before biome.
        resource_api::Inputs in = small_world();
        in.biome[6 + 1] = 99;
        in.river[6 + 2] = 20;
        in.water[3 * 6 + 3] = 9;
        CHECK(layers_error(in) == std::string(PRECONDITION) + "water value 9 at (3, 3)");
        in.water[3 * 6 + 3] = WATER_DRY;
        CHECK(layers_error(in) == std::string(PRECONDITION) + "river value 20 at (2, 1)");
    }
    {  // G7 with k = 1, through the stage's prefix: a two-cell flow_dir cycle.
        resource_api::Inputs in = small_world();
        in.flow_dir[1 * 6 + 2] = 0;  // (2, 1) east, (3, 1) west
        const std::string err = layers_error(in);
        CHECK(starts_with(err, std::string(PRECONDITION) + "G7: "));
    }
    {  // An array of the wrong length.
        resource_api::Inputs in = small_world();
        in.rainfall.pop_back();
        CHECK(layers_error(in) == std::string(PRECONDITION) + "rainfall length is not width * height");
    }
    {  // Threads outside [1, 256] (D10).
        const resource_api::Inputs in = small_world();
        CHECK_THROWS_AS((void)resource_api::layers(in, definitions(), tuning(), 0, 0), worldgen::FatalError);
        CHECK_THROWS_AS((void)resource_api::layers(in, definitions(), tuning(), 0, 257), worldgen::FatalError);
    }
}

TEST_CASE("P1-05 10 (as implemented, lead's ruling 2): resources_layers checks threads, tuning, biome values, then section 2") {
    resource_api::Inputs in = small_world();
    in.water[1 * 6 + 3] = 4;  // a section-2 failure, reported last
    resource_api::Tuning bad = tuning();
    bad.fert_slope_lo = Fx::from_raw(fx_of("0.5"));  // fert_slope_ordered fails
    resource_api::Definitions defs = definitions();
    for (auto& b : defs.biomes) {
        if (b.key == "marsh") {
            b.forage = Fx::from_raw(fx_of("1.5"));
        }
    }
    auto err = [&](const resource_api::Definitions& d, const resource_api::Tuning& t, std::uint32_t threads) {
        return fatal_message([&] { (void)resource_api::layers(in, d, t, 0, threads); });
    };
    CHECK(err(defs, bad, 0) == "resources: thread count outside [1, 256]");
    CHECK(err(defs, bad, 1) == "resources: tuning: fert_slope_lo must be below fert_slope_hi");
    CHECK(err(defs, tuning(), 1) == "resources: biome 'marsh' has a value outside [0, 1]");
    CHECK(err(definitions(), tuning(), 1) == std::string(PRECONDITION) + "water value 4 at (3, 1)");
    // A tuning field outside its schema range.
    resource_api::Tuning range = tuning();
    range.fresh_reach_m = 15;
    CHECK(err(definitions(), range, 1) == "resources: tuning: fresh_reach_m outside its schema range");
    resource_api::Tuning zero = tuning();
    zero.hab_weight_fertility = zero.hab_weight_forage = zero.hab_weight_timber = zero.hab_weight_ore = 0;
    CHECK(err(definitions(), zero, 1) == "resources: tuning: the hab_weight fields must not all be 0");
}

TEST_CASE("P1-05 3: on the small world, ocean 0, lakes 0 but fresh water 255, river cells fresh water 255") {
    resource_api::Inputs in = small_world();
    // A lake cell at (4, 1): water 2, depth 3, draining into the river.
    in.water[1 * 6 + 4] = WATER_LAKE;
    in.water_depth[1 * 6 + 4] = 3;
    in.flow_dir[1 * 6 + 4] = 3;  // SW to (3, 2), a river cell
    in.biome[1 * 6 + 4] = 2;
    const resource_api::Layers l = resource_api::layers(in, definitions(), tuning(), 0, THREADS);
    CHECK(l == own_resources(in, values(), 0).layers);
    for (std::size_t i = 0; i < in.grid.cells(); ++i) {
        CAPTURE(i);
        if (in.water[i] == WATER_OCEAN || in.water[i] == WATER_LAKE) {
            CHECK(l.fertility[i] == 0);
            CHECK(l.ore[i] == 0);
            CHECK(l.timber[i] == 0);
            CHECK(l.forage[i] == 0);
            CHECK(l.habitability[i] == 0);
            CHECK(l.walk_area[i] == 0);
            CHECK(int{l.fresh_water[i]} == (in.water[i] == WATER_LAKE ? 255 : 0));
        }
        if (in.water[i] == WATER_RIVER) {
            CHECK(l.fresh_water[i] == 255);
            // A class-4 river is fish water: grassland forage 0.3 + 0.3.
            CHECK(int{l.forage[i]} == int{own_q255(D("0.3") + own_fish(0, values()))});
        }
    }
}
