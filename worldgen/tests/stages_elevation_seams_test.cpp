// The elevation stage's small seams (worldgen/stages/elevation.hpp) against
// the worked examples of spec P1-01 10.1. The examples file checks the same
// numbers on the test's own arithmetic; here the stage's own functions are
// held to them: derived integers, E1 coordinates, E3 gradient and width, the
// coast factor and mask (case F1), the floor's bisection mids, E5.3, E6,
// the parts of E8 on the 4 x 3 example, E10.0 and E10.3.

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/geo/flow.hpp"
#include "worldgen/stages/elevation.hpp"

using namespace elev_test;
namespace ws = worldgen::stages;
using worldgen::Fx;
using worldgen::geo::Grid;

namespace {

const ws::ElevationTuning& shipped() {
    static const ws::ElevationTuning t = [] {
        const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml());
        REQUIRE(l.ok());
        return elev_api::tuning_from(*l.content);
    }();
    return t;
}

ws::ElevationTuning with(const Overrides& o) {
    const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml(o));
    REQUIRE(l.ok());
    return elev_api::tuning_from(*l.content);
}

Fx m(std::int64_t metres) { return Fx::from_int(metres); }

}  // namespace

TEST_CASE("P1-01 10.1 (stage): derived integers") {
    const ws::ElevationTuning& t = shipped();
    CHECK(ws::elevation_margin(t.edge_margin_frac, 512) == 15);
    CHECK(ws::elevation_margin(t.edge_margin_frac, 1024) == 30);
    CHECK(ws::elevation_margin(t.edge_margin_frac, 2048) == 61);
    CHECK(ws::elevation_margin(Fx(), 512) == 1);
    CHECK(ws::island_min_cells(50000) == 196);
    CHECK(ws::island_min_cells(0) == 0);
    CHECK(ws::island_min_cells(256) == 1);
    CHECK(ws::island_min_cells(257) == 2);
    CHECK(t.land_floor_m.raw() == 429496730);
    const std::vector<std::pair<std::int32_t, std::int64_t>> lmc = {
        {128, 184}, {256, 733}, {512, 2930}, {1024, 11719}, {2048, 46875}};
    for (const auto& [n, want] : lmc) {
        CHECK(ws::lake_max_cells(t.lake_max_area_m2, t.lake_area_ref_n, n) == want);
    }
    CHECK(ws::lake_min_cells(t.lake_min_area_m2) == 391);
    CHECK(ws::lake_min_depth_dm(t.lake_min_depth_m) == 20);
    CHECK(ws::fill_fine_slope(t.fill_slope_dm) == 65536);
    CHECK(ws::mountain_floor_target(6517, t.segment_on_share) == 521);
    CHECK(ws::lake_max_share_cells(6517, t.lake_max_share) == 196);
    CHECK(ws::mountain_floor_target(26261, t.segment_on_share) == 2101);
    CHECK(ws::lake_max_share_cells(26261, t.lake_max_share) == 788);
    CHECK(ws::lake_max_share_cells(0, t.lake_max_share) == 1);  // the cap is never 0
    CHECK(ws::belt_halfwidth(t.belt_halfwidth_m, t.belt_halfwidth_frac, 512).raw() == 5277655810048);
    CHECK(ws::belt_halfwidth(t.belt_halfwidth_m, t.belt_halfwidth_frac, 1024) == m(2000));
    CHECK(ws::belt_halfwidth(t.belt_halfwidth_m, t.belt_halfwidth_frac, 2048) == m(2000));
    // The small-map fixture at N = 128.
    const ws::ElevationTuning s = with(small_overrides());
    CHECK(ws::lake_min_cells(s.lake_min_area_m2) == 40);
    CHECK(ws::lake_max_cells(s.lake_max_area_m2, s.lake_area_ref_n, 128) == 625);
    CHECK(ws::lake_min_depth_dm(s.lake_min_depth_m) == 10);
    // ErosionParams of the tuning (D_o, D_d, K_erosion).
    const ws::ErosionParams ep = ws::erosion_params(t, 0);
    CHECK(ep.talus_drop_o.raw() == 206158430208);
    CHECK(ep.talus_drop_d.raw() == 291552048000);
    CHECK(ep.key == 0x7d5f47a9629481b8ULL);
    CHECK(ep.iterations == 24);
    CHECK(ep.route_interval == 1);
    CHECK(ep.k.raw() == 34359738);
    CHECK(ep.diffusion.raw() == 143022411);
    CHECK(ep.talus_rate.raw() == 268435456);
    CHECK(ep.floor.raw() == 429496730);
}

TEST_CASE("P1-01 10.1 (stage): coordinates and continent centre") {
    CHECK(ws::elevation_map_fraction(0, 1024).raw() == 2097152);  // 1/2048 (the spec prints the 512 value)
    CHECK(ws::elevation_map_fraction(1023, 1024).raw() == 4292870144);
    CHECK(ws::elevation_map_fraction(0, 512).raw() == 4194304);
    CHECK(ws::elevation_map_fraction(3, 16).raw() == 939524096);
    CHECK(ws::elevation_metres(2047) == m(32760));
    const Fx jit = shipped().continent_center_jitter;
    CHECK(ws::continent_centre_coordinate(std::uint64_t{1} << 31U, jit) == Fx::HALF);
    CHECK(ws::continent_centre_coordinate(0, jit).raw() == 1889785610);
    CHECK(ws::continent_centre_coordinate((std::uint64_t{1} << 32U) - 1U, jit).raw() == 2405181686);
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{7}}) {
        const ws::ContinentCentre c = ws::continent_centre(seed, jit);
        CHECK(c.cx.raw() == centre(seed, 0, shipped_values()));
        CHECK(c.cy.raw() == centre(seed, 1, shipped_values()));
    }
}

TEST_CASE("P1-01 10.1 (stage): E3 belt gradient and belt width") {
    const std::int64_t u = 67108864;
    const std::vector<std::int64_t> rows = {0, 4, 8, 12, 2, 6, 10, 14, 4, 8, 12, 16, 6, 10, 14, 30};
    std::vector<std::int64_t> nb;
    for (const std::int64_t r : rows) {
        nb.push_back(r * u);
    }
    for (const std::uint32_t threads : {1U, 3U}) {
        const std::vector<std::int64_t> g = ws::belt_gradient(Grid{4, 4}, nb, threads);
        CHECK(g == belt_gradient(Grid{4, 4}, nb));
        CHECK(g[15] == 1426754019);
        CHECK(g[14] == 684378814);
        CHECK(g[11] == 600239927);
        CHECK(g[0] == 300119964);
    }
    CHECK(ws::belt_width(R(300119964), m(2000)).raw() == 37514995500);
    CHECK(ws::belt_width(R(dec("0.01")), R(5277655810048)).raw() == 3298534884);
    CHECK(ws::belt_width(R(dec("0.01")), m(2000)).raw() == 5368709125);
    CHECK(ws::belt_width(R(1), R(1)).raw() == 1);  // at least raw 1
}

TEST_CASE("P1-01 10.1 (stage): coast factor, mask values of case F1, floor mids") {
    const ws::LandProfile p = ws::land_profile(Fx::HALF, R(dec("0.4")), m(2));
    CHECK(p.tl.raw() == 5368709121);
    CHECK(p.gm.raw() == 3112960001);
    CHECK(p.co.raw() == 2386092942);
    const Fx sl = R(dec("0.4"));
    CHECK(ws::ratio_capped(worldgen::mul_int(sl, 1024), sl) == Fx::from_int(1024));
    CHECK(ws::land_profile(worldgen::mul_int(sl, 1024), sl, m(2)).gm == Fx::ONE);
    CHECK(ws::land_profile(-Fx::HALF, sl, m(2)).gm.raw() == 3112960001);  // abs: pockets
    // F1: b0, sg(0.25) and masks at o = 0, +d, -d with bias 0.25 and 2.
    const std::vector<std::array<const char*, 3>> cells = {
        {"0.01", "-0.6", "0.5"}, {"-0.02", "-0.7", "0.6"}, {"0.03", "-0.5", "0.4"}, {"-0.07", "-0.4", "1"},
        {"-0.08", "-0.5", "0.95"}, {"0.08", "-0.6", "1"}, {"0.2", "0.4", "1"}, {"-0.3", "0.3", "1"},
        {"0.5", "0", "1"}, {"-0.6", "0.2", "0.2"}};
    const std::vector<std::array<std::int64_t, 10>> table = {{
        {700423267, 4283480114, 349274969, 2141740057, 2678025583, 218366206, 1339012792, 1616941713, 131845428, 808470857},
        {338970816, 4212436052, 199474332, 2527461632, 1123559201, 53204659, 674135521, 3171408094, 150177831, 1902844857},
        {1182007296, 4046202788, 445418173, 1618481115, 3594544031, 395698219, 1437817612, 700423267, 77104700, 280169307},
        {1747506766, 2415124793, 982649372, 2415124793, 1512254, 615296, 1512254, 4293455043, 1746891471, 4293455043},
        {1182007296, 1879842488, 491479449, 1785850364, 0, 0, 0, 4293455043, 1122511557, 4078782291},
        {700423267, 1879842488, 306564713, 1879842488, 4293455043, 700176649, 4293455043, 0, 0, 0},
        {ONE, 0, 0, 0, 152444828, 152444828, 152444828, 0, 0, 0},
        {4294321122, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {3850371072, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {4258202376, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    }};
    const Fx w_b = R(dec("0.15"));
    const Fx d = worldgen::div_int(w_b, 2);
    const Fx contrast = R(dec("0.75"));
    const Fx bias = R(dec("0.25"));
    const Fx two = Fx::from_int(2);
    std::vector<ws::FloorCell> fc;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        CAPTURE(i);
        const Fx nb = R(dec(cells[i][0]));
        const Fx ns = R(dec(cells[i][1]));
        const Fx gm = R(dec(cells[i][2]));
        fc.push_back(ws::FloorCell{nb, ns, gm});
        std::array<std::int64_t, 10> got{};
        got[0] = ws::segment_factor(ns, bias, contrast).raw();
        std::size_t k = 1;
        for (const Fx o : {Fx(), d, -d}) {
            got[k++] = ws::belt_profile(nb, o, w_b).raw();
            got[k++] = ws::mountain_mask(nb, ns, gm, o, bias, w_b, contrast).raw();
            got[k++] = ws::mountain_mask(nb, ns, gm, o, two, w_b, contrast).raw();
        }
        CHECK(got == table[i]);
        CHECK(ws::segment_factor(ns, two, contrast) == Fx::ONE);
    }
    const ws::MountainFloor f = ws::mountain_floor(fc, w_b, with({{"segment_on_share", "\"0.2\""}}));
    CHECK(f.target == 2);
    CHECK(f.triggered);
    CHECK(f.offset == Fx());
    CHECK(f.bias.raw() == 1844380220);
    REQUIRE(f.bisection.size() == 32U);
    const std::vector<std::int64_t> first = {4831838208, 2952790016, 2013265920, 1543503872, 1778384896, 1895825408};
    for (std::size_t i = 0; i < first.size(); ++i) {
        CHECK(f.bisection[i].raw() == first[i]);
    }
    CHECK(f.bisection[31].raw() == 1844380218);
    // F2: no bisection.
    fc[3] = ws::FloorCell{R(dec("-0.09")), R(dec("-0.4")), R(dec("1"))};
    fc[4] = ws::FloorCell{R(dec("-0.1")), R(dec("-0.5")), R(dec("0.95"))};
    fc[5] = ws::FloorCell{R(dec("0.08")), R(dec("-0.6")), R(dec("0.5"))};
    const ws::MountainFloor f2 = ws::mountain_floor(fc, w_b, with({{"segment_on_share", "\"0.2\""}}));
    CHECK(f2.offset.raw() == -322122547);
    CHECK(f2.bias == bias);
    CHECK(f2.bisection.empty());
}

TEST_CASE("P1-01 10.1 (stage): E5.3 ocean depth and chamfer") {
    const ws::ElevationTuning& t = shipped();
    const std::vector<std::pair<std::int64_t, std::int64_t>> want = {
        {5, -12373596248}, {7, -16418735648}, {12, -26420454248}, {100, -179870268248}};
    for (const auto& [dl, h] : want) {
        CHECK(ws::ocean_depth(dl, t.ocean_shelf_m, t.ocean_depth_m).raw() == h);
        CHECK(ws::ocean_depth(dl, t.ocean_shelf_m, t.ocean_depth_m).raw() == ocean_height(dl, shipped_values()));
    }
    CHECK(ws::quantize_elevation_cell(-12373596248, true) == -29);
    CHECK(ws::quantize_elevation_cell(-179870268248, true) == -419);
    // ocean_shelf_m = 0 is allowed (dist_m >= 16 m).
    CHECK(ws::ocean_depth(5, Fx(), t.ocean_depth_m) == -(Fx::HALF + t.ocean_depth_m));
    // The chamfer against Dijkstra on P1-02 10.1 item 8 and on random sources.
    std::vector<std::uint8_t> src(81, 0);
    src[4 * 9 + 4] = 1;
    const std::vector<std::int64_t> dl = ws::chamfer_distance(Grid{9, 9}, src);
    CHECK(dl == chamfer_dijkstra(Grid{9, 9}, src));
    CHECK(dl[0] == 28);
    CHECK(dl[4 * 9 + 8] == 20);
    worldgen::Rng rng(0x5345414D43484146ULL);
    const Grid g{37, 23};
    for (int k = 0; k < 20; ++k) {
        std::vector<std::uint8_t> s(cells(g));
        for (auto& x : s) {
            x = rng.uniform_i64(0, 9) == 0 ? 1 : 0;
        }
        s[0] = 1;
        CHECK(ws::chamfer_distance(g, s) == chamfer_dijkstra(g, s));
    }
}

TEST_CASE("P1-01 E6 (stage): mountain_scale is the E6 formula of the stage's units") {
    const ws::ElevationTuning& t = shipped();
    const elev_api::Fields f = elev_api::fields(0, 128, t, THREADS);
    const elev_api::Ocean oc = elev_api::classify(f, t);
    const ws::ElevationHeights h = ws::elevation_heights(0, f.engine, oc.mask, t, THREADS);
    std::vector<std::int64_t> units;
    for (std::size_t i = 0; i < oc.mask.size(); ++i) {
        if (oc.mask[i] == 0U) {
            units.push_back(h.unit[i]);
            REQUIRE(h.unit[i] >= 0);
            REQUIRE(h.unit[i] <= ONE);
        } else {
            REQUIRE(h.unit[i] == 0);
        }
    }
    CHECK(h.land_cells == oc.land_cells);
    const ws::MountainScale ms = ws::mountain_scale(units, t);
    CHECK(ms.scale == h.scale);
    CHECK(ms.q == h.q);
    CHECK(ms.qc == h.qc);
    // E6 by the test.
    const Fx q = R(kth_sorted(units, q_rank(t.mountain_peak_rank.raw(), units.size())));
    const Fx qc = R(kth_sorted(units, q_rank(t.mountain_cover_rank.raw(), units.size())));
    CHECK(q == ms.q);
    CHECK(qc == ms.qc);
    const Fx ps = worldgen::mul_int(q, 3) > Fx::ONE ? worldgen::div(t.mountain_height_m, q)
                                                    : worldgen::mul_int(t.mountain_height_m, 3);
    const Fx scale = (qc > Fx() && worldgen::mul(ps, qc) > t.mountain_cover_height_m)
                         ? worldgen::div(t.mountain_cover_height_m, qc)
                         : ps;
    CHECK(scale == ms.scale);
    // Edge cases: all units 0 gives the cap 3 x mountain_height_m.
    const std::vector<std::int64_t> zeros(50, 0);
    CHECK(ws::mountain_scale(zeros, t).scale == worldgen::mul_int(t.mountain_height_m, 3));
}

TEST_CASE("P1-01 10.1 (stage): the parts of E8 on the 4 x 3 example") {
    const Grid g{4, 3};
    const std::vector<std::int64_t> rows = {-5, 20, 60, 140, -5, 10, 50, 130, -5, 30, 70, 150};
    std::vector<std::int64_t> h;
    std::vector<std::uint8_t> o;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        h.push_back(m(rows[i]).raw());
        o.push_back(i % 4 == 0 ? 1 : 0);
    }
    const ws::ErosionParams p = ws::erosion_params(shipped(), 0);
    const worldgen::geo::RoutingFlood fl = worldgen::geo::routing_flood(g, h, o);
    CHECK(fl.order == std::vector<worldgen::geo::CellIndex>{0, 4, 8, 5, 1, 9, 6, 2, 10, 7, 3, 11});
    std::vector<worldgen::geo::Direction> rcv = worldgen::geo::receivers(g, fl.level, fl.parent, o);
    ws::erosion_random_receivers(g, fl.level, o, p.key, 0, rcv, 2);
    CHECK(rcv == std::vector<worldgen::geo::Direction>{8, 3, 3, 3, 8, 3, 5, 5, 8, 5, 5, 4});
    const std::vector<std::uint32_t> a = worldgen::geo::accumulate(g, rcv, o, fl.order);
    CHECK(a == std::vector<std::uint32_t>{0, 3, 2, 1, 0, 5, 2, 1, 0, 1, 2, 1});
    std::vector<std::int64_t> f(12, -1);
    ws::erosion_factors(g, a, rcv, o, p.k, f, 2);
    CHECK(f == std::vector<std::int64_t>{0, 42081660, 34359405, 24296004, 0, 54327163, 34359405, 24296004, 0,
                                         24296004, 34359405, 34359738});
    std::vector<std::int64_t> hn(12, 0);
    ws::erosion_solve(g, h, o, rcv, f, fl.order, p.floor, hn);
    CHECK(hn == std::vector<std::int64_t>{-21474836480, 85065878946, 255989444282, 599115291631, -21474836480,
                                          42413187352, 213719148628, 556644983913, -21474836480, 128124238736,
                                          298598250103, 641501865506});
    std::vector<std::int64_t> hd(12, 0);
    ws::erosion_diffuse(g, hn, o, p.diffusion, p.floor, hd, 2);
    CHECK(hd == std::vector<std::int64_t>{-21474836480, 86504605273, 260316179429, 586274939663, -21474836480,
                                          50979829367, 223668165360, 549465549000, -21474836480, 126680308153,
                                          301513681839, 627257440952});
    std::vector<std::int64_t> tt(12, 0);
    ws::erosion_talus(g, hd, o, p.talus_drop_o, p.talus_drop_d, p.talus_rate, p.floor, tt, 2);
    CHECK(tt == std::vector<std::int64_t>{-21474836480, 86504605273, 267803700056, 574346498642, -21474836480,
                                          50979829367, 242588847069, 541988114410, -21474836480, 126680308153,
                                          308987764896, 612781031170});
    // The diagonal factor: A = 4, k = HALF, diagonal receiver.
    const Grid g2{2, 2};
    const std::vector<std::uint8_t> o2 = {1, 0, 0, 0};
    const std::vector<std::uint32_t> a2 = {0, 1, 1, 4};
    const std::vector<worldgen::geo::Direction> r2 = {8, 4, 6, 5};
    std::vector<std::int64_t> f2(4, -1);
    ws::erosion_factors(g2, a2, r2, o2, Fx::HALF, f2, 1);
    CHECK(f2[3] == 3037000500);
    CHECK(f2[1] == Fx::HALF.raw());
}

TEST_CASE("P1-01 10.1 (stage): E10.0 cost, bump and eps; fill_grids") {
    const Fx amp = Fx::from_int(3);
    CHECK(ws::fill_cost(R(dec("0.1")), amp).raw() == 12025908430);
    CHECK(ws::fill_cost(R(dec("-0.7")), amp) == Fx::ONE);
    CHECK(ws::fill_cost(R(dec("0.6")), amp) == Fx::from_int(4));
    CHECK(ws::fill_slope_eps(65536, R(12025908430), false) == 183501);
    CHECK(ws::fill_slope_eps(65536, R(12025908430), true) == 259509);
    CHECK(ws::fill_slope_eps(65536, Fx::ONE, false) == 65536);
    CHECK(ws::fill_slope_eps(65536, Fx::ONE, true) == 92682);
    CHECK(ws::fill_slope_eps(65536, Fx::from_int(4), false) == 262144);
    CHECK(ws::fill_slope_eps(65536, Fx::from_int(4), true) == 370728);
    CHECK(ws::fill_slope_eps(0, Fx::ONE, true) == 1);  // at least one fine unit
    CHECK(ws::fill_slope_eps(65536, Fx::ONE + Fx::HALF, true) == 139023);
    CHECK(ws::fill_bump(R(dec("0.3")), amp) == 589824);
    CHECK(ws::fill_bump(R(dec("-0.2")), amp) == 0);
    CHECK(ws::fill_bump(R(dec("0.6667")), amp) == 1310786);
    CHECK(ws::fill_bump(R(dec("0.6667")), Fx()) == 0);
    for (const bool small : {false, true}) {
        const ws::ElevationTuning t = small ? with(small_overrides()) : shipped();
        const elev_api::Fields f = elev_api::fields(3, 64, t, THREADS);
        const elev_api::Ocean oc = elev_api::classify(f, t);
        const CostBump own = cost_bump(3, 64, oc.mask, small ? small_values() : shipped_values());
        for (const std::uint32_t threads : {1U, 5U}) {
            const ws::FillGrids fg = ws::fill_grids(3, 64, oc.mask, t, threads);
            CHECK(fg.cost == own.cost);
            CHECK(fg.bump == own.bump);
        }
    }
}

TEST_CASE("P1-01 10.1 (stage): E10.3 select_lakes on the example's components") {
    std::vector<ws::DepressionComponent> c = {{1, 5, 25, 60},  {2, 14, 40, 300}, {3, 8, 30, 120}, {4, 2, 50, 90},
                                              {5, 6, 15, 50},  {6, 10, 22, 120}, {7, 9, 21, 61},  {8, 3, 20, 25}};
    ws::ConditionParams p;
    p.lake_min_cells = 3;
    p.lake_max_cells = 12;
    p.lake_min_depth_dm = 20;
    p.max_share_cells = 30;
    CHECK(ws::select_lakes(c, p) == std::vector<std::uint32_t>{3, 6, 7, 8});
    p.max_share_cells = 29;
    CHECK(ws::select_lakes(c, p) == std::vector<std::uint32_t>{3, 6, 7});
    p.max_share_cells = 1;
    CHECK(ws::select_lakes(c, p).empty());
    p.max_share_cells = 1000;
    CHECK(ws::select_lakes(c, p) == std::vector<std::uint32_t>{3, 6, 7, 1, 8});
}

TEST_CASE("P1-01 10.1 (stage): the conditioning seam reports components and kept labels") {
    // The fill-flood example: one component, label 1, size 2, depth 4, volume 6.
    const std::vector<std::int16_t> e = {-5, -5, -5, -5, -5, -5, -5, -5, 30, 40, 35, 50, 45, -5,
                                         -5, 45, 10, 14, 60, 1,  -5, -5, 38, 12, 25, 1,  1,  -5,
                                         -5, 20, 28, 70, 1,  1,  -5, -5, -5, -5, -5, -5, -5, -5};
    std::vector<std::uint8_t> o;
    for (const std::int16_t x : e) {
        o.push_back(x < 0 ? 1 : 0);
    }
    const std::vector<std::int64_t> cost(42, ONE);
    const std::vector<std::int64_t> bump(42, 0);
    elev_api::ConditionParams p;
    p.lake_min_cells = 2;
    p.lake_max_cells = 10;
    p.lake_min_depth_dm = 4;
    p.max_share_cells = 100;
    const ws::Conditioned c = elev_api::condition_full(Grid{7, 6}, e, o, cost, bump, p);
    REQUIRE(c.components.size() == 1U);
    CHECK(c.components[0].label == 1U);
    CHECK(c.components[0].size == 2);
    CHECK(c.components[0].depth == 4);
    CHECK(c.components[0].volume == 6);
    CHECK(c.kept == std::vector<std::uint32_t>{1});
    p.lake_min_depth_dm = 5;
    CHECK(elev_api::condition_full(Grid{7, 6}, e, o, cost, bump, p).kept.empty());
}
