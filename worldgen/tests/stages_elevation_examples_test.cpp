// Worked examples of the elevation stage (spec P1-01 section 10.1), through
// the library seams of section 10.
//
// Written from docs/specs/p1-01-elevation.md and P1-00 only, independently
// of the implementation. Every expected number is from the spec text and was
// recomputed by worldgen/tests/reference/p1_elevation_reference.py. Where
// the spec gives a number for a step that has no seam (the 4 x 4 gradient,
// the coast factor, the cost and bump), the example checks the test's own
// helper in p1_01_elevation_support.hpp, which the map-level tests then use
// to check the stage.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"

using namespace elev_test;
using worldgen::Fx;
using worldgen::geo::Grid;

namespace {

const elev_api::Tuning& shipped_tuning() {
    static const elev_api::Tuning t = [] {
        const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml());
        REQUIRE_MESSAGE(l.ok(), "shipped elevation.toml does not load");
        return elev_api::tuning_from(*l.content);
    }();
    return t;
}

elev_api::Tuning tuning_with(const Overrides& o) {
    const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml(o));
    REQUIRE_MESSAGE(l.ok(), "elevation.toml with overrides does not load");
    return elev_api::tuning_from(*l.content);
}

std::vector<std::int64_t> raws(std::initializer_list<const char*> texts) {
    std::vector<std::int64_t> out;
    for (const char* t : texts) {
        out.push_back(dec(t));
    }
    return out;
}

Fx m(std::int64_t metres) { return Fx::from_int(metres); }

// The shipped erosion values (4.4) as seam parameters.
elev_api::ErosionParams shipped_erosion(std::uint64_t key_erosion) {
    const Values& v = shipped_values();
    elev_api::ErosionParams p;
    p.iterations = v.i("erosion_iterations");
    p.route_interval = v.i("erosion_route_interval");
    p.k = v.raw("erosion_k");
    p.diffusion = v.raw("erosion_diffusion");
    p.talus_slope = v.raw("talus_slope");
    p.talus_rate = v.raw("talus_rate");
    p.floor = v.raw("land_floor_m");
    p.key = key_erosion;
    return p;
}

}  // namespace

// --- Derived integers and coordinates (10.1) --------------------------------------

TEST_CASE("P1-01 10.1: derived integers of the shipped values") {
    const Values& v = shipped_values();
    CHECK(v.raw("edge_margin_frac") == 128849019);
    CHECK(v.margin(512) == 15);
    CHECK(v.margin(1024) == 30);
    CHECK(v.margin(2048) == 61);
    CHECK(Values({{"edge_margin_frac", "\"0\""}}).margin(512) == 1);
    CHECK(v.i_min() == 196);
    CHECK(Values({{"island_min_area_m2", "0"}}).i_min() == 0);
    CHECK(Values({{"island_min_area_m2", "256"}}).i_min() == 1);
    CHECK(Values({{"island_min_area_m2", "257"}}).i_min() == 2);
    CHECK(v.raw("land_floor_m") == 429496730);
    CHECK(v.lake_max_cells(128) == 184);
    CHECK(v.lake_max_cells(256) == 733);
    CHECK(v.lake_max_cells(512) == 2930);
    CHECK(v.lake_max_cells(1024) == 11719);
    CHECK(v.lake_max_cells(2048) == 46875);
    CHECK(v.lake_min_cells() == 391);
    CHECK(v.lake_min_depth_dm() == 20);
    CHECK(v.fine_slope() == 65536);
    CHECK(worldgen::mul_int(v.fx("talus_slope"), 16).raw() == 206158430208);
    CHECK(worldgen::mul(worldgen::mul_int(v.fx("talus_slope"), 16), Fx::SQRT2).raw() == 291552048000);
    CHECK(v.target(6517) == 521);
    CHECK(v.max_share_cells(6517) == 196);
    CHECK(v.target(26261) == 2101);
    CHECK(v.max_share_cells(26261) == 788);
    // 4.4: no lake fits below N = 187 with the shipped values.
    CHECK(v.lake_max_cells(186) < v.lake_min_cells());
    CHECK(v.lake_max_cells(187) >= v.lake_min_cells());
    // The small-map fixture at N = 128.
    const Values& s = small_values();
    CHECK(s.lake_min_cells() == 40);
    CHECK(s.lake_max_cells(128) == 625);
    CHECK(s.lake_min_depth_dm() == 10);
    // Belt half-width cap, 512 against 1024.
    CHECK(v.raw("belt_halfwidth_frac") == 644245094);
    CHECK(v.hw(512) == 5277655810048);
    CHECK(v.hw(1024) == m(2000).raw());
    CHECK(v.hw(2048) == m(2000).raw());
}

TEST_CASE("P1-01 10.1: coordinates") {
    // Spec 10.1 prints 4194304 and 4290772992 for N = 1024; those are the
    // N = 512 values. For N = 1024, U = from_ratio(2x + 1, 2048): 1/2048 at
    // x = 0 (the spec's own parenthesis) and 2047/2048 at x = 1023.
    CHECK(Fx::from_ratio(1, 2048).raw() == 2097152);
    CHECK(Fx::from_ratio(2047, 2048).raw() == 4292870144);
    CHECK(Fx::from_ratio(1, 1024).raw() == 4194304);
    CHECK(Fx::from_ratio(1023, 1024).raw() == 4290772992);
    CHECK(Fx::from_ratio(7, 32).raw() == 939524096);
    CHECK(Fx::from_int(16 * 2047 + 8) == Fx::from_int(32760));
}

TEST_CASE("P1-01 10.1: continent centre") {
    const Values& v = shipped_values();
    const Fx jit = v.fx("continent_center_jitter");
    CHECK(jit.raw() == 257698038);
    auto j = [&](std::int64_t hv) {
        return (Fx::HALF + worldgen::mul(jit, Fx::from_raw((hv - (std::int64_t{1} << 31)) * 2))).raw();
    };
    CHECK(j(std::int64_t{1} << 31) == Fx::HALF.raw());
    CHECK(j(0) == 1889785610);
    CHECK(j((std::int64_t{1} << 32) - 1) == 2405181686);
    // Seed 0: cx, cy returned by the seam equal the test's E1.
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}}) {
        CAPTURE(seed);
        const elev_api::Fields f = elev_api::fields(seed, 64, shipped_tuning(), THREADS);
        CHECK(f.cx == centre(seed, 0, v));
        CHECK(f.cy == centre(seed, 1, v));
        CHECK(f.cx >= ONE / 4);
        CHECK(f.cx < 3 * ONE / 4);
        CHECK(f.cy >= ONE / 4);
        CHECK(f.cy < 3 * ONE / 4);
    }
}

TEST_CASE("P1-01 10.1: E2 structural, 32 cells at N = 64, seed 0") {
    const Values& v = shipped_values();
    const std::int32_t n = 64;
    const elev_api::Fields f = elev_api::fields(0, n, shipped_tuning(), THREADS);
    REQUIRE(f.c.size() == 4096U);
    std::vector<std::pair<std::int32_t, std::int32_t>> pts = {{0, 0}, {63, 0}, {0, 63}, {63, 63}};
    for (std::int32_t k = 0; k < 8; ++k) {
        pts.emplace_back(8 * k + 3, n / 2);   // row N/2
        pts.emplace_back(n / 2, 8 * k + 5);   // column N/2
    }
    for (std::int32_t k = 0; k < 12; ++k) {
        pts.emplace_back((17 * k + 11) % n, (29 * k + 7) % n);
    }
    REQUIRE(pts.size() == 32U);
    for (const auto& [x, y] : pts) {
        CAPTURE(x);
        CAPTURE(y);
        const CellFields want = e2_cell(0, n, x, y, v);
        const std::size_t i = static_cast<std::size_t>(y * n + x);
        CHECK(f.c[i] == want.c);
        CHECK(f.nb[i] == want.nb);
        CHECK(f.ns[i] == want.ns);
    }
}

// --- E3 ------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1: E3 coast threshold and land scale (the test's arithmetic)") {
    const std::vector<std::int64_t> c = {5, -3, 2, 0, 8, -1, 4, -7, 1, 3};
    const std::int64_t q = Fx::ONE.raw() - dec("0.4");
    CHECK(dec("0.4") == 1717986918);
    CHECK(q == 2576980378);
    CHECK(q_rank(q, 10) == 6U);
    const std::int64_t c_sea = kth_sorted(c, q_rank(q, 10));
    CHECK(c_sea == 3);
    std::vector<std::int64_t> land;
    for (const std::int64_t x : c) {
        if (x > c_sea) {
            land.push_back(x - c_sea);
        }
    }
    CHECK(std::max<std::int64_t>(1, kth_sorted(land, q_rank(Fx::HALF.raw(), land.size()))) == 2);
}

TEST_CASE("P1-01 10.1: E3 belt gradient on a 4 x 4 grid (the test's arithmetic)") {
    const std::int64_t u = 67108864;  // 1/64
    const std::vector<std::int64_t> rows = {0, 4, 8, 12, 2, 6, 10, 14, 4, 8, 12, 16, 6, 10, 14, 30};
    std::vector<std::int64_t> nb;
    for (const std::int64_t r : rows) {
        nb.push_back(r * u);
    }
    const Grid g{4, 4};
    const std::vector<std::int64_t> gr = belt_gradient(g, nb);
    CHECK(gr[0] == 300119964);
    CHECK(gr[12] == 300119964);
    CHECK(gr[13] == 300119964);
    CHECK(gr[14] == 684378814);
    CHECK(gr[15] == 1426754019);
    CHECK(gr[11] == 600239927);
    for (std::size_t i = 0; i < 11; ++i) {
        CHECK(gr[i] == 300119964);
    }
    const std::int64_t g_med = std::max<std::int64_t>(1, kth_sorted(gr, q_rank(Fx::HALF.raw(), 16)));
    CHECK(g_med == 300119964);
    const std::int64_t w_b =
        std::max(Fx::from_raw(1), worldgen::div_int(worldgen::mul(R(g_med), m(2000)), 16)).raw();
    CHECK(w_b == 37514995500);
    // Half-width cap: g_med = 0.01.
    const Values& v = shipped_values();
    CHECK(worldgen::div_int(worldgen::mul(R(dec("0.01")), R(v.hw(512))), 16).raw() == 3298534884);
    CHECK(worldgen::div_int(worldgen::mul(R(dec("0.01")), R(v.hw(1024))), 16).raw() == 5368709125);
}

// --- E4 ------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1: E4 on a given mask (8 x 8, I_min 2)") {
    const std::vector<std::string_view> pic = {
        "........", ".###..#.", ".#.##...", ".#####..", "..####..", "...##...", ".....#..", "........",
    };
    std::vector<std::uint8_t> p;
    for (const auto row : pic) {
        for (const char ch : row) {
            p.push_back(ch == '.' ? 1 : 0);
        }
    }
    const Grid g{8, 8};
    const std::vector<std::uint8_t> o = elev_api::ocean_from_provisional(g, p, 2);
    REQUIRE(o.size() == 64U);
    std::vector<std::pair<int, int>> land;
    for (int i = 0; i < 64; ++i) {
        if (o[static_cast<std::size_t>(i)] == 0U) {
            land.emplace_back(i % 8, i / 8);
        }
    }
    std::vector<std::pair<int, int>> want;
    for (int x = 1; x <= 3; ++x) {
        want.emplace_back(x, 1);
    }
    for (int x = 1; x <= 4; ++x) {
        want.emplace_back(x, 2);
    }
    for (int x = 1; x <= 5; ++x) {
        want.emplace_back(x, 3);
    }
    for (int x = 2; x <= 5; ++x) {
        want.emplace_back(x, 4);
    }
    want.emplace_back(3, 5);
    want.emplace_back(4, 5);
    want.emplace_back(5, 6);
    CHECK(land == want);
    CHECK(land.size() == 19U);
    CHECK(o[2 * 8 + 2] == 0U);  // the pocket becomes land
    CHECK(o[1 * 8 + 6] == 1U);  // the single cell is sunk
    CHECK(o[6 * 8 + 5] == 0U);  // joined diagonally, it stays
}

TEST_CASE("P1-01 10.1: E4 pockets and several edge components") {
    // 5 x 5: border and (2, 2) provisional; O is the border only.
    const Grid g5{5, 5};
    std::vector<std::uint8_t> p(25, 0);
    for (int i = 0; i < 25; ++i) {
        const int x = i % 5;
        const int y = i / 5;
        p[static_cast<std::size_t>(i)] = (x == 0 || y == 0 || x == 4 || y == 4 || (x == 2 && y == 2)) ? 1 : 0;
    }
    const std::vector<std::uint8_t> o = elev_api::ocean_from_provisional(g5, p, 1);
    for (int i = 0; i < 25; ++i) {
        const int x = i % 5;
        const int y = i / 5;
        CAPTURE(i);
        CHECK(o[static_cast<std::size_t>(i)] == ((x == 0 || y == 0 || x == 4 || y == 4) ? 1 : 0));
    }
    // A 6 x 3 test grid whose P has two edge-touching components that are
    // not 4-connected to each other: both are ocean.
    //   P . # # # # .     (x = 0 column and x = 5 column provisional)
    const Grid g{6, 3};
    std::vector<std::uint8_t> p2(18, 0);
    for (int y = 0; y < 3; ++y) {
        p2[static_cast<std::size_t>(y * 6)] = 1;
        p2[static_cast<std::size_t>(y * 6 + 5)] = 1;
    }
    const std::vector<std::uint8_t> o2 = elev_api::ocean_from_provisional(g, p2, 1);
    for (int i = 0; i < 18; ++i) {
        CAPTURE(i);
        CHECK(o2[static_cast<std::size_t>(i)] == p2[static_cast<std::size_t>(i)]);
    }
    // No component of at least I_min cells: fatal "elevation: no land".
    CHECK_THROWS_AS(static_cast<void>(elev_api::ocean_from_provisional(g, p2, 13)), worldgen::FatalError);
    CHECK_NOTHROW(static_cast<void>(elev_api::ocean_from_provisional(g, p2, 12)));
}

// --- E5 ------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1: E5 coast factor (the test's arithmetic)") {
    const Fx s_land = R(dec("0.4"));
    const Fx tl = rc(Fx::HALF, s_land);
    CHECK(tl.raw() == 5368709121);
    CHECK(worldgen::div(tl, Fx::from_int(2)).raw() == 2684354561);
    CHECK(gm_of(Fx::HALF.raw(), 0, dec("0.4"), shipped_values()) == 3112960001);  // s = HALF, t = 2
    CHECK(worldgen::div(tl, Fx::ONE + tl).raw() == 2386092942);
    CHECK(rc(worldgen::mul_int(s_land, 1024), s_land) == Fx::from_int(1024));
    CHECK(gm_of(worldgen::mul_int(s_land, 1024).raw(), 0, s_land.raw(), shipped_values()) == Fx::ONE.raw());
}

namespace {

struct FloorCase {
    std::vector<std::int64_t> nb, ns, gm;
};

FloorCase case_f1() {
    FloorCase c;
    c.nb = raws({"0.01", "-0.02", "0.03", "-0.07", "-0.08", "0.08", "0.2", "-0.3", "0.5", "-0.6"});
    c.ns = raws({"-0.6", "-0.7", "-0.5", "-0.4", "-0.5", "-0.6", "0.4", "0.3", "0", "0.2"});
    c.gm = raws({"0.5", "0.6", "0.4", "1", "0.95", "1", "1", "1", "1", "0.2"});
    return c;
}

FloorCase case_f2() {
    FloorCase c = case_f1();
    c.nb[3] = dec("-0.09");
    c.ns[3] = dec("-0.4");
    c.gm[3] = dec("1");
    c.nb[4] = dec("-0.1");
    c.ns[4] = dec("-0.5");
    c.gm[4] = dec("0.95");
    c.nb[5] = dec("0.08");
    c.ns[5] = dec("-0.6");
    c.gm[5] = dec("0.5");
    return c;
}

const Overrides& floor_overrides() {
    static const Overrides o = {{"segment_on_share", "\"0.2\""}};
    return o;
}

}  // namespace

TEST_CASE("P1-01 10.1: E5 mask values of case F1 (the test's arithmetic)") {
    const Values v(floor_overrides());
    const std::int64_t w_b = dec("0.15");
    CHECK(w_b == 644245094);
    const std::int64_t d = worldgen::div_int(R(w_b), 2).raw();
    CHECK(d == 322122547);
    CHECK(v.raw("segment_floor_mask") == 1073741824);
    CHECK(v.raw("floor_inland_gm") == 3865470566);
    CHECK(v.target(10) == 2);
    const FloorCase c = case_f1();
    // Columns: m0(.25), m0(2), m+(.25), m+(2), m-(.25), m-(2).
    const std::vector<std::vector<std::int64_t>> want = {
        {349274969, 2141740057, 218366206, 1339012792, 131845428, 808470857},
        {199474332, 2527461632, 53204659, 674135521, 150177831, 1902844857},
        {445418173, 1618481115, 395698219, 1437817612, 77104700, 280169307},
        {982649372, 2415124793, 615296, 1512254, 1746891471, 4293455043},
        {491479449, 1785850364, 0, 0, 1122511557, 4078782291},
        {306564713, 1879842488, 700176649, 4293455043, 0, 0},
        {0, 0, 152444828, 152444828, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
    };
    const std::int64_t bias = v.raw("segment_bias");
    const std::int64_t two = Fx::from_int(2).raw();
    for (std::size_t i = 0; i < 10; ++i) {
        CAPTURE(i);
        std::vector<std::int64_t> got;
        for (const std::int64_t o : {std::int64_t{0}, d, -d}) {
            got.push_back(mask_of(c.nb[i], c.ns[i], c.gm[i], o, bias, w_b, v));
            got.push_back(mask_of(c.nb[i], c.ns[i], c.gm[i], o, two, w_b, v));
        }
        CHECK(got == want[i]);
    }
}

TEST_CASE("P1-01 10.1: E5.1 floor case F1, the bisection runs (required by 10.4)") {
    const elev_api::Tuning t = tuning_with(floor_overrides());
    const FloorCase c = case_f1();
    const std::int64_t w_b = dec("0.15");
    const elev_api::Floor f = elev_api::floor_search(c.nb, c.ns, c.gm, w_b, t);
    CHECK(f.triggered);
    CHECK(f.target == 2);
    CHECK(f.o_star == 0);
    CHECK(f.beta_star == 1844380220);
    CHECK(f.beta_star != shipped_values().raw("segment_bias"));  // not the trivial result
    // The test's own E5.1 agrees, with the mid sequence of the spec.
    const FloorOwn own = floor_own(c.nb, c.ns, c.gm, w_b, Values(floor_overrides()));
    CHECK(own.beta_star == 1844380220);
    REQUIRE(own.mids.size() == 32U);
    CHECK(own.mids[0] == 4831838208);
    CHECK(own.mids[1] == 2952790016);
    CHECK(own.mids[2] == 2013265920);
    CHECK(own.mids[3] == 1543503872);
    CHECK(own.mids[4] == 1778384896);
    CHECK(own.mids[5] == 1895825408);
    CHECK(own.mids[31] == 1844380218);
    // With floor_inland_gm 0 (the v15 rule) the result is the same here.
    const Overrides v15 = concat(floor_overrides(), {{"floor_inland_gm", "\"0\""}});
    const elev_api::Floor f15 = elev_api::floor_search(c.nb, c.ns, c.gm, w_b, tuning_with(v15));
    CHECK(f15.o_star == 0);
    CHECK(f15.beta_star == 1844380220);
}

TEST_CASE("P1-01 10.1: E5.1 floor case F2, the offset search") {
    const elev_api::Tuning t = tuning_with(floor_overrides());
    const FloorCase c = case_f2();
    const std::int64_t w_b = dec("0.15");
    const Values v(floor_overrides());
    const std::int64_t d = worldgen::div_int(R(w_b), 2).raw();
    const std::int64_t two = Fx::from_int(2).raw();
    CHECK(mask_of(c.nb[3], c.ns[3], c.gm[3], 0, two, w_b, v) == 1363394412);
    CHECK(mask_of(c.nb[4], c.ns[4], c.gm[4], 0, two, w_b, v) == 856342242);
    CHECK(mask_of(c.nb[3], c.ns[3], c.gm[3], -d, two, w_b, v) == 4258202375);
    CHECK(mask_of(c.nb[4], c.ns[4], c.gm[4], -d, two, w_b, v) == 3935396343);
    CHECK(mask_of(c.nb[3], c.ns[3], c.gm[3], -d, v.raw("segment_bias"), w_b, v) == 1732548108);
    CHECK(mask_of(c.nb[4], c.ns[4], c.gm[4], -d, v.raw("segment_bias"), w_b, v) == 1083050666);
    const elev_api::Floor f = elev_api::floor_search(c.nb, c.ns, c.gm, w_b, t);
    CHECK(f.triggered);
    CHECK(f.target == 2);
    CHECK(f.o_star == -322122547);
    CHECK(f.beta_star == 1073741824);
    // v15 (floor_inland_gm 0): offset 0 with a raised bias, a coastal rim.
    const Overrides v15 = concat(floor_overrides(), {{"floor_inland_gm", "\"0\""}});
    const elev_api::Floor f15 = elev_api::floor_search(c.nb, c.ns, c.gm, w_b, tuning_with(v15));
    CHECK(f15.o_star == 0);
    CHECK(f15.beta_star > v.raw("segment_bias"));
    CHECK(f15.beta_star == floor_own(c.nb, c.ns, c.gm, w_b, Values(v15)).beta_star);
}

TEST_CASE("P1-01 E5.1: floor not triggered, and nothing reaches the target") {
    const FloorCase c = case_f1();
    const std::int64_t w_b = dec("0.15");
    // segment_on_share 0: target 0, never triggers.
    const elev_api::Floor f0 = elev_api::floor_search(c.nb, c.ns, c.gm, w_b, tuning_with({{"segment_on_share", "\"0\""}}));
    CHECK_FALSE(f0.triggered);
    CHECK(f0.target == 0);
    CHECK(f0.o_star == 0);
    CHECK(f0.beta_star == shipped_values().raw("segment_bias"));
    // segment_on_share 1: target 10, more than any offset or bias reaches.
    const Overrides all = {{"segment_on_share", "\"1\""}};
    const elev_api::Floor f1 = elev_api::floor_search(c.nb, c.ns, c.gm, w_b, tuning_with(all));
    const FloorOwn own = floor_own(c.nb, c.ns, c.gm, w_b, Values(all));
    CHECK(f1.triggered);
    CHECK(f1.target == 10);
    CHECK(f1.o_star == own.o_star);
    CHECK(f1.beta_star == Fx::from_int(2).raw());  // hi never moves
    CHECK(own.beta_star == Fx::from_int(2).raw());
}

// --- E5.3, E10.0 (the test's arithmetic) -------------------------------------------

TEST_CASE("P1-01 10.1: E5.3 ocean depth and E10.0 cost and bump (the test's arithmetic)") {
    const Values& v = shipped_values();
    CHECK(ocean_height(5, v) == -12373596248);
    CHECK(quantize_cell(ocean_height(5, v), true) == -29);
    CHECK(ocean_height(7, v) == -16418735648);
    CHECK(quantize_cell(ocean_height(7, v), true) == -38);
    CHECK(ocean_height(12, v) == -26420454248);
    CHECK(quantize_cell(ocean_height(12, v), true) == -62);
    CHECK(ocean_height(100, v) == -179870268248);
    CHECK(quantize_cell(ocean_height(100, v), true) == -419);
    auto cost = [](const char* nz) {
        return (Fx::ONE + worldgen::mul(Fx::from_int(3), worldgen::clamp(Fx::HALF + R(dec(nz)), Fx(), Fx::ONE))).raw();
    };
    auto eps = [](std::int64_t c, bool diag) {
        const std::int64_t ce = diag ? worldgen::mul(R(c), Fx::SQRT2).raw() : c;
        return std::max<std::int64_t>(1, worldgen::mul_div(65536, ce, std::int64_t{1} << 32));
    };
    CHECK(cost("0.1") == 12025908430);
    CHECK(eps(cost("0.1"), false) == 183501);
    CHECK(eps(cost("0.1"), true) == 259509);
    CHECK(cost("-0.7") == ONE);
    CHECK(eps(cost("-0.7"), false) == 65536);
    CHECK(eps(cost("-0.7"), true) == 92682);
    CHECK(cost("0.6") == Fx::from_int(4).raw());
    CHECK(eps(cost("0.6"), false) == 262144);
    CHECK(eps(cost("0.6"), true) == 370728);
    auto bump = [](const char* dn) {
        const Fx bv = worldgen::mul(Fx::from_int(3), worldgen::max(Fx(), R(dec(dn))));
        return worldgen::mul_div(bv.raw(), 10 * FINE, std::int64_t{1} << 32);
    };
    CHECK(dec("0.3") == 1288490189);
    CHECK(worldgen::mul(Fx::from_int(3), R(dec("0.3"))).raw() == 3865470567);
    CHECK(bump("0.3") == 589824);
    CHECK(bump("-0.2") == 0);
    CHECK(bump("0.6667") == 1310786);
    // Chamfer by Dijkstra on P1-02 10.1 item 8's 9 x 9 grid.
    std::vector<std::uint8_t> src(81, 0);
    src[4 * 9 + 4] = 1;
    const std::vector<std::int64_t> dl = chamfer_dijkstra(Grid{9, 9}, src);
    CHECK(dl[4 * 9 + 5] == 5);
    CHECK(dl[5 * 9 + 5] == 7);
    CHECK(dl[5 * 9 + 6] == 12);
    CHECK(dl[8 * 9 + 8] == 28);
    CHECK(dl[4 * 9 + 8] == 20);
    CHECK(dl[0] == 28);
}

// --- E8 ------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1: E8 solve by hand, 5 x 1") {
    elev_api::ErosionParams p;
    p.iterations = 1;
    p.route_interval = 1;
    p.k = Fx::HALF.raw();
    p.diffusion = 0;
    p.talus_slope = Fx::from_int(3).raw();
    p.talus_rate = 0;
    p.floor = dec("0.1");
    p.key = 12345;
    const std::vector<std::int64_t> h = {m(-5).raw(), m(10).raw(), m(20).raw(), m(40).raw(), m(80).raw()};
    const std::vector<std::uint8_t> o = {1, 0, 0, 0, 0};
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        const std::vector<std::int64_t> out = elev_api::erode(Grid{5, 1}, h, o, p, threads);
        CHECK(out[0] == h[0]);
        CHECK(out[1] == 21474836480);
        CHECK(out[2] == 55999923266);
        CHECK(out[3] == 123833544243);
        CHECK(out[4] == 270342770534);
    }
    // The factors of the example.
    CHECK(static_cast<std::int64_t>(worldgen::isqrt_u64(std::uint64_t{4} << 32U) << 16U) == 8589934592);
    CHECK(static_cast<std::int64_t>(worldgen::isqrt_u64(std::uint64_t{3} << 32U) << 16U) == 7439056896);
    CHECK(static_cast<std::int64_t>(worldgen::isqrt_u64(std::uint64_t{2} << 32U) << 16U) == 6073942016);
    CHECK(worldgen::mul(R(4294967296), Fx::INV_SQRT2).raw() == 3037000500);  // diagonal factor
}

TEST_CASE("P1-01 10.1: E8 diffusion and the edge rule") {
    elev_api::ErosionParams p;
    p.iterations = 1;
    p.route_interval = 1;
    p.k = 0;
    p.diffusion = dec("0.1");
    p.talus_slope = Fx::from_int(3).raw();
    p.talus_rate = 0;
    p.floor = dec("0.1");
    p.key = 1;
    CHECK(p.diffusion == 429496730);
    // Centre (1, 1) at 10 m; E ocean, S 20 m, W 30 m, N 40 m; corners ocean.
    const std::vector<std::int64_t> h = {m(-1).raw(), m(40).raw(), m(-1).raw(), m(30).raw(), m(10).raw(),
                                         m(-1).raw(), m(-1).raw(), m(20).raw(), m(-1).raw()};
    const std::vector<std::uint8_t> o = {1, 0, 1, 0, 0, 1, 1, 0, 1};
    const std::vector<std::int64_t> out = elev_api::erode(Grid{3, 3}, h, o, p, 1);
    CHECK(out[4] == 64424509460);
    for (std::size_t i : {std::size_t{0}, std::size_t{2}, std::size_t{5}, std::size_t{6}, std::size_t{8}}) {
        CHECK(out[i] == h[i]);  // ocean unchanged
    }
    // Edge rule: 3 x 1, ocean, 10 m, 20 m at the east edge.
    const std::vector<std::int64_t> h2 = {m(-1).raw(), m(10).raw(), m(20).raw()};
    const std::vector<std::uint8_t> o2 = {1, 0, 0};
    const std::vector<std::int64_t> out2 = elev_api::erode(Grid{3, 1}, h2, o2, p, 1);
    CHECK(out2[2] == 81604378620);
}

TEST_CASE("P1-01 10.1: E8 one full iteration with random receivers and talus, 4 x 3") {
    CHECK(worldgen::stream_key(0, "geo.elevation.erosion") == 0x7d5f47a9629481b8ULL);
    const Values& v = shipped_values();
    CHECK(v.raw("erosion_k") == 34359738);
    CHECK(v.raw("erosion_diffusion") == 143022411);
    CHECK(v.raw("talus_rate") == 268435456);
    elev_api::ErosionParams p = shipped_erosion(0x7d5f47a9629481b8ULL);
    p.iterations = 1;
    const std::vector<std::int64_t> rows = {-5, 20, 60, 140, -5, 10, 50, 130, -5, 30, 70, 150};
    std::vector<std::int64_t> h;
    std::vector<std::uint8_t> o;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        h.push_back(m(rows[i]).raw());
        o.push_back(i % 4 == 0 ? 1 : 0);
    }
    const std::vector<std::int64_t> want = {-21474836480, 86504605273, 267803700056, 574346498642,
                                            -21474836480, 50979829367, 242588847069, 541988114410,
                                            -21474836480, 126680308153, 308987764896, 612781031170};
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        CHECK(elev_api::erode(Grid{4, 3}, h, o, p, threads) == want);
    }
    // The draws of the example's table: hash_at(K, c, 0) for cells 1 and 11.
    CHECK(worldgen::hash_at(0x7d5f47a9629481b8ULL, 1, 0) == 16623902295446874679ULL);
    CHECK(worldgen::hash_at(0x7d5f47a9629481b8ULL, 11, 0) == 13145864497773222452ULL);
    // With erosion_iterations = 0 erosion does nothing.
    p.iterations = 0;
    CHECK(elev_api::erode(Grid{4, 3}, h, o, p, 2) == h);
}

// --- E9 ------------------------------------------------------------------------------

TEST_CASE("P1-01 10.1: E9 quantization") {
    const std::vector<std::int64_t> h = {216895848448, 171798692, dec("0.05"), -dec("0.05"), -1,
                                         53021371269,  m(2990).raw(), -dec("2900.5")};
    const std::vector<std::uint8_t> o = {0, 0, 0, 1, 1, 0, 0, 1};
    CHECK(dec("50.5") == 216895848448);
    CHECK(dec("0.04") == 171798692);
    CHECK(dec("12.345") == 53021371269);
    const std::vector<std::int16_t> e = elev_api::quantize(h, o);
    const std::vector<std::int16_t> want = {505, 1, 1, -1, -1, 123, 29900, -29005};
    CHECK(e == want);
}

// --- E10 -----------------------------------------------------------------------------

namespace {

// The grid of the E10.3 selection example: eight pits, each in its own row
// of a flat rim at 100 dm, with (size, depth, volume) as in the spec, so
// that their G2 labels are 1 to 8 in order.
struct SelectionGrid {
    Grid g{18, 19};
    std::vector<std::int16_t> e;
    std::vector<std::uint8_t> o;
    std::vector<std::vector<std::size_t>> pits;
};

SelectionGrid selection_grid() {
    const std::vector<std::array<int, 3>> comps = {{5, 25, 60},  {14, 40, 300}, {8, 30, 120}, {2, 50, 90},
                                                   {6, 15, 50},  {10, 22, 120}, {9, 21, 61},  {3, 20, 25}};
    SelectionGrid s;
    const int w = s.g.width;
    const int h = s.g.height;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const bool border = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            s.e.push_back(static_cast<std::int16_t>(border ? -5 : 100));
            s.o.push_back(border ? 1 : 0);
        }
    }
    for (std::size_t k = 0; k < comps.size(); ++k) {
        const int size = comps[k][0];
        const int depth = comps[k][1];
        const int volume = comps[k][2];
        std::vector<int> ds(static_cast<std::size_t>(size), 1);
        ds[0] = depth;
        int rest = volume - depth - (size - 1);
        for (std::size_t j = 1; j < ds.size() && rest > 0; ++j) {
            const int a = std::min(rest, depth - ds[j]);
            ds[j] += a;
            rest -= a;
        }
        REQUIRE(rest == 0);
        const int y = 2 + 2 * static_cast<int>(k);
        std::vector<std::size_t> pit;
        for (int j = 0; j < size; ++j) {
            const std::size_t i = static_cast<std::size_t>(y * w + 2 + j);
            s.e[i] = static_cast<std::int16_t>(100 - ds[static_cast<std::size_t>(j)]);
            pit.push_back(i);
        }
        s.pits.push_back(pit);
    }
    return s;
}

}  // namespace

TEST_CASE("P1-01 10.1: E10.3 lake selection with the share cap") {
    const SelectionGrid s = selection_grid();
    const std::size_t n = s.e.size();
    const std::vector<std::int64_t> cost(n, ONE);
    const std::vector<std::int64_t> bump(n, 0);
    elev_api::ConditionParams p;
    p.lake_min_cells = 3;
    p.lake_max_cells = 12;
    p.lake_min_depth_dm = 20;
    p.max_share_cells = 30;
    p.fine_slope = 65536;
    const std::vector<std::int16_t> out = elev_api::condition(s.g, s.e, s.o, cost, bump, p);
    REQUIRE(out.size() == n);
    // Kept: labels 3, 6, 7 and 8 (8 + 10 + 9 + 3 = 30 cells); 1 skipped by
    // the cap; 2 too large, 4 too small, 5 too shallow.
    const std::vector<bool> kept = {false, false, true, false, false, true, true, true};
    for (std::size_t k = 0; k < 8; ++k) {
        CAPTURE(k + 1);
        for (const std::size_t i : s.pits[k]) {
            if (kept[k]) {
                CHECK(out[i] == s.e[i]);
            } else {
                CHECK(out[i] >= 100);
            }
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (s.o[i] != 0U) {
            CHECK(out[i] == s.e[i]);
        }
    }
    // With the cap at 29, label 8 no longer fits after 3, 6, 7 (27 + 3 > 29)
    // and label 1 (5 cells) does not either: only 3, 6 and 7 are kept.
    p.max_share_cells = 29;
    const std::vector<std::int16_t> out29 = elev_api::condition(s.g, s.e, s.o, cost, bump, p);
    for (const std::size_t i : s.pits[7]) {
        CHECK(out29[i] >= 100);
    }
    for (const std::size_t i : s.pits[6]) {
        CHECK(out29[i] == s.e[i]);
    }
    // With the cap at 1 nothing fits.
    p.max_share_cells = 1;
    const std::vector<std::int16_t> out1 = elev_api::condition(s.g, s.e, s.o, cost, bump, p);
    for (std::size_t i = 0; i < n; ++i) {
        if (s.o[i] == 0U) {
            CHECK(out1[i] >= 100);
        }
    }
}

namespace {

struct FillCase {
    Grid g{7, 6};
    std::vector<std::int16_t> e;
    std::vector<std::uint8_t> o;
    std::vector<std::int64_t> cost, bump;
    [[nodiscard]] std::size_t at(int x, int y) const { return static_cast<std::size_t>(y * 7 + x); }
};

FillCase fill_case() {
    FillCase f;
    f.e = {-5, -5, -5, -5, -5, -5, -5, -5, 30, 40, 35, 50, 45, -5, -5, 45, 10, 14, 60, 1, -5,
           -5, 38, 12, 25, 1,  1,  -5, -5, 20, 28, 70, 1,  1,  -5, -5, -5, -5, -5, -5, -5, -5};
    for (const std::int16_t v : f.e) {
        f.o.push_back(v < 0 ? 1 : 0);
    }
    f.cost.assign(42, ONE);
    f.cost[f.at(2, 2)] = Fx::from_int(2).raw();
    f.cost[f.at(2, 3)] = ONE + HALF;
    f.bump.assign(42, 0);
    f.bump[f.at(2, 2)] = 30000;
    f.bump[f.at(4, 3)] = 200000;
    f.bump[f.at(5, 2)] = 70000;
    return f;
}

}  // namespace

TEST_CASE("P1-01 10.1: E10 fill flood, case 1 (filled) and case 2 (kept)") {
    const FillCase f = fill_case();
    elev_api::ConditionParams p;
    p.lake_min_cells = 2;
    p.lake_max_cells = 10;
    p.max_share_cells = 100;
    p.fine_slope = 65536;
    // The arithmetic of the example.
    CHECK(worldgen::mul_div(65536, worldgen::mul(R(ONE + HALF), Fx::SQRT2).raw(), std::int64_t{1} << 32) == 139023);
    // Case 1: lake_min_depth_dm 5, the depression (depth 4) is filled.
    p.lake_min_depth_dm = 5;
    const std::vector<std::int16_t> out1 = elev_api::condition(f.g, f.e, f.o, f.cost, f.bump, p);
    std::vector<std::int16_t> want1 = f.e;
    want1[f.at(2, 2)] = 16;
    want1[f.at(2, 3)] = 16;
    want1[f.at(4, 3)] = 4;
    CHECK(out1 == want1);
    // Case 2: lake_min_depth_dm 4, kept.
    p.lake_min_depth_dm = 4;
    const std::vector<std::int16_t> out2 = elev_api::condition(f.g, f.e, f.o, f.cost, f.bump, p);
    std::vector<std::int16_t> want2 = f.e;
    want2[f.at(4, 3)] = 4;
    CHECK(out2 == want2);
    // Case 2 with the cap at 1: not kept, filled as in case 1.
    p.max_share_cells = 1;
    CHECK(elev_api::condition(f.g, f.e, f.o, f.cost, f.bump, p) == want1);
    // With every bump 0 the flat (4, 3) is not raised to 4.
    FillCase f0 = fill_case();
    f0.bump.assign(42, 0);
    p.max_share_cells = 100;
    p.lake_min_depth_dm = 5;
    const std::vector<std::int16_t> out0 = elev_api::condition(f0.g, f0.e, f0.o, f0.cost, f0.bump, p);
    CHECK(out0[f0.at(4, 3)] == 1);
    // (4, 3) gets kf 65537 (eps 1, no bump), so it stays 1 dm; (3, 2) is
    // still reached from it at kf = base 917504; then (2, 3): max(786432,
    // 917504 + 139023) >> 16 = 16; (2, 2): max(655360, 917504 + 131072) >> 16
    // = 16 (checked with the reference).
    CHECK(out0[f0.at(2, 3)] == 16);
    CHECK(out0[f0.at(2, 2)] == 16);
}
