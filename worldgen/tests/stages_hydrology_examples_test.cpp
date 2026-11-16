// Worked example and unit tests of the hydrology stage (spec P1-02 sections
// 2, 3, 5 and 8.1). The stage is reached only through
// p1_02_hydrology_api.hpp. The shared G3, G4 and G5 functions on these grids
// are tested in geo_flow_test.cpp; here the stage's own outputs are checked.
//
// The expected grids of 5.1 are the spec's; those of the designed fixtures
// (8.1 items 5 and 6) were computed by
// worldgen/tests/reference/p1_hydrology_reference.py (command `fixtures`)
// and checked there against the invariants of section 6.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "p1_02_hydrology_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "worldgen/core/error.hpp"

using namespace hydro_test;
using worldgen::geo::Grid;

namespace {

using Rows8 = std::vector<std::vector<std::uint8_t>>;
using Rows16 = std::vector<std::vector<std::uint16_t>>;
using Rows32 = std::vector<std::vector<std::uint32_t>>;
using RowsI16 = std::vector<std::vector<std::int16_t>>;

hydro_api::Layers run(const Fixture& f, std::int64_t area, std::uint32_t threads = THREADS) {
    return hydro_api::layers(f.grid, f.e, tuning(area), threads);
}

template <typename T>
std::vector<std::vector<T>> rows(Grid g, const std::vector<T>& v) {
    std::vector<std::vector<T>> out;
    for (std::int32_t y = 0; y < g.height; ++y) {
        out.emplace_back(v.begin() + y * g.width, v.begin() + (y + 1) * g.width);
    }
    return out;
}

// The cells of a lake id.
std::vector<std::size_t> cells_of(const hydro_api::Layers& l, std::uint32_t id) {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < l.lake.size(); ++i) {
        if (l.lake[i] == id) {
            out.push_back(i);
        }
    }
    return out;
}

// Lake cells whose downstream cell is outside the lake.
std::vector<std::size_t> exits_of(Grid g, std::span<const std::uint8_t> dir, std::span<const std::uint32_t> lake,
                                  std::uint32_t id) {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < lake.size(); ++i) {
        if (lake[i] == id) {
            const std::int64_t m = step_to(g, i, dir[i]);
            if (m < 0 || lake[static_cast<std::size_t>(m)] != id) {
                out.push_back(i);
            }
        }
    }
    return out;
}

void expect_precondition_error(const Fixture& f, std::string_view what) {
    CAPTURE(std::string(what));
    try {
        static_cast<void>(run(f, SHIPPED_AREA));
        FAIL("no fatal error for " << std::string(what));
    } catch (const worldgen::FatalError& ex) {
        const std::string msg = ex.what();
        CAPTURE(msg);
        CHECK(msg.rfind(std::string(PRECONDITION), 0) == 0);
    }
}

}  // namespace

// --- 8.1 item 1: worked example 5.1 ------------------------------------------------

TEST_CASE("P1-02 5.1: every output grid of the worked example") {
    const Fixture f = example_51();
    REQUIRE(threshold(EXAMPLE_AREA) == 3);
    const hydro_api::Layers l = run(f, EXAMPLE_AREA);
    const Grid g = f.grid;

    CHECK(rows(g, l.flow_dir) == Rows8{
                                     {8, 8, 8, 8, 8, 8, 8, 8},
                                     {8, 4, 6, 6, 6, 6, 0, 8},
                                     {8, 4, 2, 3, 4, 1, 0, 8},
                                     {8, 4, 1, 4, 4, 1, 0, 8},
                                     {8, 4, 0, 1, 2, 1, 0, 8},
                                     {8, 2, 2, 2, 2, 2, 0, 8},
                                     {8, 8, 8, 8, 8, 8, 8, 8},
                                 });
    CHECK(rows(g, l.flow_accum) == Rows32{
                                       {0, 0, 0, 0, 0, 0, 0, 0},
                                       {0, 1, 1, 1, 1, 1, 1, 0},
                                       {0, 1, 1, 2, 1, 1, 1, 0},
                                       {0, 1, 6, 2, 1, 1, 2, 0},
                                       {0, 1, 1, 8, 1, 1, 2, 0},
                                       {0, 1, 1, 1, 10, 1, 2, 0},
                                       {0, 0, 0, 0, 0, 0, 0, 0},
                                   });
    CHECK(rows(g, l.water) == Rows8{
                                  {3, 3, 3, 3, 3, 3, 3, 3},
                                  {3, 0, 0, 0, 0, 0, 0, 3},
                                  {3, 0, 2, 2, 0, 0, 0, 3},
                                  {3, 0, 2, 2, 0, 0, 0, 3},
                                  {3, 0, 0, 1, 0, 0, 0, 3},
                                  {3, 0, 0, 0, 1, 0, 0, 3},
                                  {3, 3, 3, 3, 3, 3, 3, 3},
                              });
    // River cells (3, 4) with A 8 and (4, 5) with A 10: class 2.
    std::vector<std::uint8_t> river(56, 0);
    river[g.index(3, 4)] = 2;
    river[g.index(4, 5)] = 2;
    CHECK(l.river == river);
    // water_depth: 5 on the ocean, 8 6 / 7 5 on the lake, 0 elsewhere.
    CHECK(rows(g, l.water_depth) == Rows16{
                                        {5, 5, 5, 5, 5, 5, 5, 5},
                                        {5, 0, 0, 0, 0, 0, 0, 5},
                                        {5, 0, 8, 6, 0, 0, 0, 5},
                                        {5, 0, 7, 5, 0, 0, 0, 5},
                                        {5, 0, 0, 0, 0, 0, 0, 5},
                                        {5, 0, 0, 0, 0, 0, 0, 5},
                                        {5, 5, 5, 5, 5, 5, 5, 5},
                                    });
    std::vector<std::uint32_t> lake(56, 0);
    for (const auto [x, y] : {std::pair{2, 2}, std::pair{3, 2}, std::pair{2, 3}, std::pair{3, 3}}) {
        lake[g.index(x, y)] = 1;
    }
    CHECK(l.lake == lake);
    CHECK(rows(g, l.coast_dist) == RowsI16{
                                       {-22, -16, -16, -16, -16, -16, -16, -22},
                                       {-16, 16, 16, 16, 16, 16, 16, -16},
                                       {-16, 16, 32, 32, 32, 32, 16, -16},
                                       {-16, 16, 32, 48, 48, 32, 16, -16},
                                       {-16, 16, 32, 32, 32, 32, 16, -16},
                                       {-16, 16, 16, 16, 16, 16, 16, -16},
                                       {-22, -16, -16, -16, -16, -16, -16, -22},
                                   });
    CHECK(invariant_violation(g, f.e, l, 3).empty());
}

TEST_CASE("P1-02 5.1 H4: G4 alone gives the lake two exits; the stage keeps the outlet (2, 3), first popped") {
    const Fixture f = example_51();
    const Grid g = f.grid;
    const std::vector<std::uint8_t> ocean = ocean_of(f.e);
    const hydro_api::Flood fl = hydro_api::flood(g, keys_of(f.e), ocean);
    const std::vector<std::uint8_t> g4 = hydro_api::receivers(g, fl, ocean, 1);
    // Pre-rerouting receivers: the lake cells keep their flood parents.
    CHECK(g4[g.index(2, 2)] == 2);
    CHECK(g4[g.index(3, 2)] == 3);
    CHECK(g4[g.index(2, 3)] == 1);
    CHECK(g4[g.index(3, 3)] == 2);
    const hydro_api::Layers l = run(f, EXAMPLE_AREA);
    std::vector<std::uint32_t> lake = l.lake;
    CHECK(exits_of(g, g4, lake, 1) == std::vector<std::size_t>{g.index(2, 3), g.index(3, 3)});
    CHECK(exits_of(g, l.flow_dir, lake, 1) == std::vector<std::size_t>{g.index(2, 3)});
    // The outlet is the lake cell first in `order`, and its receiver is its
    // flood parent; every other cell of the stage's flow_dir equals G4 except
    // the lake cells (H4 step 2 changes nothing else).
    const auto& order = fl.order;
    const auto first = std::find_if(order.begin(), order.end(), [&](std::uint32_t c) { return lake[c] == 1U; });
    REQUIRE(first != order.end());
    CHECK(*first == g.index(2, 3));
    CHECK(l.flow_dir[*first] == fl.parent[*first]);
    for (std::size_t i = 0; i < g4.size(); ++i) {
        if (lake[i] == 0U) {
            CAPTURE(i);
            CHECK(l.flow_dir[i] == g4[i]);
        }
    }
    // BFS from the outlet: code 0 reaches (3, 3) (W), 6 reaches (2, 2) (S),
    // 7 reaches (3, 2) (SW).
    CHECK(l.flow_dir[g.index(3, 3)] == 4);
    CHECK(l.flow_dir[g.index(2, 2)] == 2);
    CHECK(l.flow_dir[g.index(3, 2)] == 3);
}

TEST_CASE("P1-02 5.1 H7: the chamfer values quoted in the example, through the H7 function alone") {
    const Fixture f = example_51();
    const std::vector<std::uint8_t> ocean = ocean_of(f.e);
    std::vector<std::uint8_t> land(ocean.size());
    for (std::size_t i = 0; i < ocean.size(); ++i) {
        land[i] = ocean[i] != 0U ? 0 : 1;
    }
    const std::vector<std::int64_t> dl = hydro_api::chamfer(f.grid, ocean);
    const std::vector<std::int64_t> dw = hydro_api::chamfer(f.grid, land);
    CHECK(dl[f.grid.index(3, 3)] == 15);
    CHECK(coast_metres(15) == 48);
    CHECK(dw[f.grid.index(0, 0)] == 7);
    CHECK(coast_metres(7) == 22);
    CHECK(dl == elev_test::chamfer_dijkstra(f.grid, ocean));
    CHECK(dw == elev_test::chamfer_dijkstra(f.grid, land));
}

TEST_CASE("P1-02 5.1: the result does not depend on the thread count") {
    const Fixture f = example_51();
    const hydro_api::Layers a = run(f, EXAMPLE_AREA, 1);
    CHECK(run(f, EXAMPLE_AREA, 2) == a);
    CHECK(run(f, EXAMPLE_AREA, 5) == a);
    CHECK(run(f, EXAMPLE_AREA, 256) == a);
}

TEST_CASE("P1-02 4: T = 3 for every river_min_area_m2 from 513 to 768 on the worked example") {
    const Fixture f = example_51();
    const hydro_api::Layers want = run(f, EXAMPLE_AREA);
    for (const std::int64_t area : {std::int64_t{513}, std::int64_t{768}}) {
        CAPTURE(area);
        CHECK(run(f, area) == want);
    }
    // 512 gives T = 2: every land cell with A >= 2 that is not a lake cell
    // is a river; (3, 4) (A 8) and (4, 5) (A 10) have class 3.
    const hydro_api::Layers t2 = run(f, 512);
    for (std::size_t i = 0; i < t2.river.size(); ++i) {
        CAPTURE(i);
        const bool is_r = f.e[i] > 0 && want.lake[i] == 0U && want.flow_accum[i] >= 2U;
        CHECK((t2.water[i] == 1U) == is_r);
        CHECK(t2.river[i] == (is_r ? river_class(want.flow_accum[i], 2) : 0));
    }
    CHECK(t2.river[f.grid.index(3, 4)] == 3);
    CHECK(t2.river[f.grid.index(4, 5)] == 3);
    // 769 gives T = 4: class bit_length(8 div 4) = 2 and bit_length(10 div 4) = 2.
    const hydro_api::Layers t4 = run(f, 769);
    CHECK(t4.river[f.grid.index(3, 4)] == 2);
    CHECK(t4.river[f.grid.index(4, 5)] == 2);
    CHECK(t4.flow_dir == want.flow_dir);
    CHECK(t4.flow_accum == want.flow_accum);
}

// --- 8.1 items 3 and 4 ---------------------------------------------------------------

TEST_CASE("P1-02 8.1 item 3: score ties go to the lowest code (stage output)") {
    const Fixture f = ties_fixture();
    const hydro_api::Layers l = run(f, 256);
    CHECK(rows(f.grid, l.flow_dir)[1] == std::vector<std::uint8_t>{8, 2, 2, 2, 2, 2});
    CHECK(rows(f.grid, l.flow_accum)[1] == std::vector<std::uint32_t>{0, 1, 1, 1, 1, 1});
    CHECK(invariant_violation(f.grid, f.e, l, 1).empty());
}

TEST_CASE("P1-02 8.1 item 4: a plateau (stage output)") {
    const Fixture f = plateau_fixture();
    const hydro_api::Layers l = run(f, 256);
    const Rows8 dir = rows(f.grid, l.flow_dir);
    CHECK(dir[1] == std::vector<std::uint8_t>{8, 4, 6, 6, 6, 0, 8});
    CHECK(dir[2] == std::vector<std::uint8_t>{8, 4, 5, 5, 5, 0, 8});
    CHECK(dir[3] == std::vector<std::uint8_t>{8, 2, 2, 2, 2, 0, 8});
    const Rows32 acc = rows(f.grid, l.flow_accum);
    CHECK(acc[1] == std::vector<std::uint32_t>{0, 2, 2, 2, 1, 1, 0});
    CHECK(acc[2] == std::vector<std::uint32_t>{0, 1, 1, 1, 1, 1, 0});
    CHECK(acc[3] == std::vector<std::uint32_t>{0, 1, 1, 1, 1, 1, 0});
    // A flat is not a depression: no lake.
    CHECK(std::all_of(l.lake.begin(), l.lake.end(), [](std::uint32_t v) { return v == 0U; }));
    CHECK(invariant_violation(f.grid, f.e, l, 1).empty());
}

// --- 8.1 item 5: two lakes at different levels -------------------------------------

TEST_CASE("P1-02 8.1 item 5: two lakes at different levels, the upper draining into the lower") {
    const Fixture f = terrace_fixture();
    const Grid g = f.grid;
    const hydro_api::Layers l = run(f, TERRACE_AREA);
    REQUIRE(threshold(TERRACE_AREA) == 4);
    // Ids follow the smallest index: the upper lake (level 150) is 1, the
    // lower lake (level 80) is 2, although the lower one is flooded first.
    std::vector<std::size_t> upper, lower;
    for (std::int32_t y = 2; y <= 3; ++y) {
        for (std::int32_t x = 5; x <= 7; ++x) {
            upper.push_back(g.index(x, y));
        }
    }
    for (std::int32_t y = 7; y <= 8; ++y) {
        for (std::int32_t x = 2; x <= 4; ++x) {
            lower.push_back(g.index(x, y));
        }
    }
    CHECK(cells_of(l, 1) == upper);
    CHECK(cells_of(l, 2) == lower);
    CHECK(cells_of(l, 3).empty());
    for (const std::size_t c : upper) {
        CHECK(std::int64_t{f.e[c]} + l.water_depth[c] == 150);
    }
    for (const std::size_t c : lower) {
        CHECK(std::int64_t{f.e[c]} + l.water_depth[c] == 80);
    }
    // One outlet each: (5, 3) spilling SE over the rim (6, 4), and (2, 8)
    // spilling W over the rim (1, 8).
    CHECK(exits_of(g, l.flow_dir, l.lake, 1) == std::vector<std::size_t>{g.index(5, 3)});
    CHECK(l.flow_dir[g.index(5, 3)] == 1);
    CHECK(exits_of(g, l.flow_dir, l.lake, 2) == std::vector<std::size_t>{g.index(2, 8)});
    CHECK(l.flow_dir[g.index(2, 8)] == 4);
    // The path from the upper outlet runs down the channel into the lower lake.
    std::vector<std::size_t> path;
    for (std::size_t c = g.index(5, 3); f.e[c] > 0; c = static_cast<std::size_t>(step_to(g, c, l.flow_dir[c]))) {
        path.push_back(c);
        REQUIRE(path.size() < 200U);
    }
    CHECK(path.size() >= 4U);
    CHECK(std::find(path.begin(), path.end(), g.index(6, 4)) != path.end());
    CHECK(std::find(path.begin(), path.end(), g.index(5, 5)) != path.end());
    CHECK(std::find(path.begin(), path.end(), g.index(5, 6)) != path.end());
    CHECK(std::any_of(path.begin(), path.end(), [&](std::size_t c) { return l.lake[c] == 2U; }));
    CHECK(path.back() == g.index(1, 8));
    // Accumulation and classes along the way (reference values): the rim
    // (6, 4) 14 (class 2), the channel 21 and 26 (class 3), the lower outlet
    // 38 and the lower rim 39 (class 4).
    CHECK(l.flow_accum[g.index(5, 3)] == 11U);
    CHECK(l.flow_accum[g.index(6, 4)] == 14U);
    CHECK(l.river[g.index(6, 4)] == 2);
    CHECK(l.flow_accum[g.index(5, 5)] == 21U);
    CHECK(l.river[g.index(5, 5)] == 3);
    CHECK(l.flow_accum[g.index(5, 6)] == 26U);
    CHECK(l.river[g.index(5, 6)] == 3);
    CHECK(l.flow_accum[g.index(2, 8)] == 38U);
    CHECK(l.flow_accum[g.index(1, 8)] == 39U);
    CHECK(l.river[g.index(1, 8)] == 4);
    CHECK(l.water[g.index(1, 8)] == 1);
    CHECK(rows(g, l.flow_dir) == Rows8{
                                     {8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
                                     {8, 4, 6, 6, 6, 6, 6, 6, 6, 0, 8},
                                     {8, 4, 5, 5, 0, 2, 3, 3, 4, 0, 8},
                                     {8, 4, 5, 1, 0, 1, 4, 4, 4, 0, 8},
                                     {8, 4, 5, 1, 1, 2, 3, 4, 5, 0, 8},
                                     {8, 4, 5, 3, 0, 2, 4, 5, 7, 0, 8},
                                     {8, 4, 2, 2, 2, 3, 4, 4, 7, 0, 8},
                                     {8, 4, 2, 3, 3, 4, 5, 5, 7, 0, 8},
                                     {8, 4, 4, 4, 4, 4, 3, 3, 7, 0, 8},
                                     {8, 2, 2, 2, 2, 2, 2, 2, 2, 0, 8},
                                     {8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8},
                                 });
    CHECK(rows(g, l.flow_accum) == Rows32{
                                       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0},
                                       {0, 2, 1, 1, 1, 2, 1, 2, 1, 1, 0},
                                       {0, 2, 1, 1, 1, 11, 6, 3, 1, 1, 0},
                                       {0, 2, 1, 1, 2, 1, 14, 1, 1, 2, 0},
                                       {0, 1, 1, 1, 2, 21, 1, 1, 1, 2, 0},
                                       {0, 1, 2, 1, 1, 26, 3, 1, 1, 2, 0},
                                       {0, 1, 3, 2, 29, 1, 1, 1, 1, 2, 0},
                                       {0, 39, 38, 32, 2, 1, 1, 1, 1, 1, 0},
                                       {0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 0},
                                       {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                                   });
    CHECK(invariant_violation(g, f.e, l, 4).empty());
}

// --- 8.1 item 6: a lake reached from several rim cells -------------------------------

TEST_CASE("P1-02 8.1 item 6: a lake touched by two rim cells at its level has exactly one exit, the first popped") {
    const Fixture f = rims_fixture();
    const Grid g = f.grid;
    const std::vector<std::uint8_t> ocean = ocean_of(f.e);
    const hydro_api::Flood fl = hydro_api::flood(g, keys_of(f.e), ocean);
    const std::vector<std::uint8_t> g4 = hydro_api::receivers(g, fl, ocean, 1);
    const hydro_api::Layers l = run(f, 256);
    // The lake: (3..5, 2..3), level 40.
    std::vector<std::size_t> want;
    for (std::int32_t y = 2; y <= 3; ++y) {
        for (std::int32_t x = 3; x <= 5; ++x) {
            want.push_back(g.index(x, y));
        }
    }
    REQUIRE(cells_of(l, 1) == want);
    CHECK(cells_of(l, 2).empty());
    // Both rim cells are land at the lake level and touch the lake.
    CHECK(f.e[g.index(2, 3)] == 40);
    CHECK(f.e[g.index(6, 2)] == 40);
    // G4 alone: four cells leave the lake, over both rims: (3, 2) SW and
    // (3, 3) W to the west rim, (5, 2) E and (5, 3) NE to the east rim.
    CHECK(exits_of(g, g4, l.lake, 1) ==
          std::vector<std::size_t>{g.index(3, 2), g.index(5, 2), g.index(3, 3), g.index(5, 3)});
    // The stage: one, the first lake cell in pop order, over its flood parent.
    const auto first = std::find_if(fl.order.begin(), fl.order.end(), [&](std::uint32_t c) { return l.lake[c] == 1U; });
    REQUIRE(first != fl.order.end());
    CHECK(*first == g.index(5, 3));
    CHECK(exits_of(g, l.flow_dir, l.lake, 1) == std::vector<std::size_t>{g.index(5, 3)});
    CHECK(l.flow_dir[g.index(5, 3)] == fl.parent[g.index(5, 3)]);
    CHECK(step_to(g, g.index(5, 3), l.flow_dir[g.index(5, 3)]) == std::int64_t{g.index(6, 2)});
    CHECK(rows(g, l.flow_dir) == Rows8{
                                     {8, 8, 8, 8, 8, 8, 8, 8, 8},
                                     {8, 4, 6, 6, 6, 6, 6, 0, 8},
                                     {8, 4, 0, 1, 1, 2, 0, 0, 8},
                                     {8, 4, 4, 0, 0, 7, 4, 0, 8},
                                     {8, 2, 2, 2, 2, 2, 2, 0, 8},
                                     {8, 8, 8, 8, 8, 8, 8, 8, 8},
                                 });
    CHECK(rows(g, l.flow_accum) == Rows32{
                                       {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                       {0, 1, 1, 1, 1, 1, 1, 1, 0},
                                       {0, 1, 1, 2, 1, 1, 9, 10, 0},
                                       {0, 2, 1, 1, 4, 8, 1, 1, 0},
                                       {0, 1, 1, 1, 1, 1, 1, 1, 0},
                                       {0, 0, 0, 0, 0, 0, 0, 0, 0},
                                   });
    CHECK(invariant_violation(g, f.e, l, 1).empty());
}

// --- 8.1 item 7: river classes -------------------------------------------------------

TEST_CASE("P1-02 8.1 item 7: river classes with T = 3 (the test's own formula, the spec's list)") {
    CHECK(river_class(3, 3) == 1);
    CHECK(river_class(5, 3) == 1);
    CHECK(river_class(6, 3) == 2);
    CHECK(river_class(11, 3) == 2);
    CHECK(river_class(12, 3) == 3);
    CHECK(river_class(3U * (1U << 14U), 3) == 15);
    CHECK(river_class(3U * (1U << 20U), 3) == 15);
    CHECK(river_class(2, 3) == 0);
    CHECK(threshold(256) == 1);
    CHECK(threshold(257) == 2);
    CHECK(threshold(500000) == 1954);
}

TEST_CASE("P1-02 4, 8.1 item 7: the stage derives T = ceil(area / 256) (a corridor with every A from 1 to 3999)") {
    const Fixture f = corridor_fixture(4000);
    for (const std::int64_t area : {std::int64_t{256}, std::int64_t{257}, std::int64_t{511}, std::int64_t{512},
                                    std::int64_t{513}, std::int64_t{499968}, std::int64_t{499969},
                                    std::int64_t{500000}, std::int64_t{500224}, std::int64_t{500225},
                                    std::int64_t{124928}, std::int64_t{124929}, std::int64_t{125000},
                                    std::int64_t{125184}, std::int64_t{125185}}) {
        CAPTURE(area);
        const std::uint64_t t = static_cast<std::uint64_t>(threshold(area));
        const hydro_api::Layers l = run(f, area);
        for (std::int32_t x = 1; x < 4000; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(4000 - x);
            REQUIRE(l.flow_accum[static_cast<std::size_t>(x)] == a);
            REQUIRE(l.river[static_cast<std::size_t>(x)] == river_class(a, t));
            REQUIRE(l.water[static_cast<std::size_t>(x)] == (a >= t ? 1 : 0));
        }
    }
    CHECK(threshold(499968) == 1953);
    CHECK(threshold(499969) == 1954);
    CHECK(threshold(500224) == 1954);
    CHECK(threshold(500225) == 1955);
}

TEST_CASE("P1-02 H6: the class cap 15 (a 32771 x 1 corridor, A up to 32770)") {
    // corridor_fixture: A at x is 32771 - x. With T = 1, A = 32770 has
    // bit_length 16: capped at 15.
    constexpr std::int32_t W = 32771;
    const Fixture f = corridor_fixture(W);
    for (const std::int64_t area : {std::int64_t{256}, std::int64_t{512}}) {
        CAPTURE(area);
        const std::uint64_t t = static_cast<std::uint64_t>(threshold(area));
        const hydro_api::Layers l = run(f, area);
        for (std::int32_t x = 1; x < W; ++x) {
            const std::uint32_t a = static_cast<std::uint32_t>(W - x);
            REQUIRE(l.flow_dir[static_cast<std::size_t>(x)] == 4);
            REQUIRE(l.flow_accum[static_cast<std::size_t>(x)] == a);
            REQUIRE(l.river[static_cast<std::size_t>(x)] == river_class(a, t));
        }
        CHECK(l.river[1] == 15);
        CHECK(bit_length(32770U / t) >= 15U);
    }
}

// --- 8.1 item 8: chamfer ---------------------------------------------------------------

TEST_CASE("P1-02 8.1 item 8: chamfer on a 9 x 9 grid with one ocean cell at (4, 4)") {
    const Grid g{9, 9};
    std::vector<std::uint8_t> ocean(81, 0);
    ocean[g.index(4, 4)] = 1;
    std::vector<std::uint8_t> land(81, 1);
    land[g.index(4, 4)] = 0;
    const std::vector<std::int64_t> dl = hydro_api::chamfer(g, ocean);
    const std::vector<std::int64_t> dw = hydro_api::chamfer(g, land);
    struct Want {
        std::int32_t x, y;
        std::int64_t d;
        std::int16_t m;
    };
    for (const Want w : {Want{5, 4, 5, 16}, Want{5, 5, 7, 22}, Want{6, 5, 12, 38}, Want{8, 8, 28, 90},
                         Want{8, 4, 20, 64}, Want{0, 0, 28, 90}}) {
        CAPTURE(w.x);
        CAPTURE(w.y);
        CHECK(dl[g.index(w.x, w.y)] == w.d);
        CHECK(coast_metres(dl[g.index(w.x, w.y)]) == w.m);
    }
    CHECK(-coast_metres(dw[g.index(4, 4)]) == -16);
    CHECK(dl == elev_test::chamfer_dijkstra(g, ocean));
    CHECK(dw == elev_test::chamfer_dijkstra(g, land));
}

TEST_CASE("P1-02 H7, 7: chamfer with no source leaves INF = 2^40, which saturates to 32767") {
    const Grid g{5, 3};
    const std::vector<std::uint8_t> none(15, 0);
    const std::vector<std::int64_t> d = hydro_api::chamfer(g, none);
    for (const std::int64_t v : d) {
        CHECK(v == INF);
    }
    CHECK(coast_metres(INF) == 32767);
}

TEST_CASE("P1-02 H7: chamfer equals the Dijkstra path minimum on random sources, non-square grids") {
    worldgen::Rng rng(worldgen::stream_key(1, RANDOM_STREAM));
    for (int k = 0; k < 20; ++k) {
        const Grid g{static_cast<std::int32_t>(3 + rng.uniform_u64(40)), static_cast<std::int32_t>(3 + rng.uniform_u64(40))};
        std::vector<std::uint8_t> src(elev_test::cells(g));
        for (auto& s : src) {
            s = rng.chance_ratio(1, 30) ? 1 : 0;
        }
        src[0] = 1;
        CAPTURE(g.width);
        CAPTURE(g.height);
        CHECK(hydro_api::chamfer(g, src) == elev_test::chamfer_dijkstra(g, src));
    }
}

TEST_CASE("P1-02 H7: coast_dist saturates at +-32767 metres far from the coast") {
    // A 2050 x 3 strip: ocean in column x = 0, flat land beyond. Land cell
    // (x, y) has chamfer 5x, so (32 * 5x + 5) div 10 = 16x metres: 32752 at
    // x = 2047, and 32768 at x = 2048, saturated to 32767.
    const Grid g{2050, 3};
    std::vector<std::int16_t> e(elev_test::cells(g), 5);
    for (std::int32_t y = 0; y < 3; ++y) {
        e[g.index(0, y)] = -3;
    }
    const hydro_api::Layers l = hydro_api::layers(g, e, tuning(SHIPPED_AREA), THREADS);
    CHECK(l.coast_dist[g.index(1, 1)] == 16);
    CHECK(l.coast_dist[g.index(2047, 1)] == 32752);
    CHECK(l.coast_dist[g.index(2048, 1)] == 32767);
    CHECK(l.coast_dist[g.index(2049, 1)] == 32767);
    CHECK(invariant_violation(g, e, l, threshold(SHIPPED_AREA)).empty());
}

// --- 8.1 item 9: preconditions ---------------------------------------------------------

TEST_CASE("P1-02 2, 8.1 item 9: precondition violations are the fatal error") {
    SUBCASE("a 0 cell") {
        Fixture f = example_51();
        f.e[f.grid.index(3, 3)] = 0;
        expect_precondition_error(f, "a 0 cell");
    }
    SUBCASE("a 0 cell on the ocean border") {
        Fixture f = example_51();
        f.e[f.grid.index(0, 3)] = 0;
        expect_precondition_error(f, "a 0 cell on the border");
    }
    SUBCASE("no ocean") {
        Fixture f = example_51();
        for (auto& v : f.e) {
            v = static_cast<std::int16_t>(v < 0 ? -v : v);
        }
        expect_precondition_error(f, "no ocean");
    }
    SUBCASE("an ocean pocket not connected to the edge") {
        Fixture f = example_51();
        f.e[f.grid.index(3, 3)] = -4;
        expect_precondition_error(f, "ocean pocket");
    }
    SUBCASE("an ocean pocket joined to the sea only diagonally") {
        // (1, 1) is ocean and touches the edge ocean only through (0, 0)
        // diagonally: the ocean is 4-connected, so this is a pocket.
        Fixture f = fixture({{-1, 5, 5, 5}, {5, -1, 5, 5}, {5, 5, 5, 5}, {5, 5, 5, 5}});
        expect_precondition_error(f, "diagonal pocket");
    }
    SUBCASE("the whole edge is land, ocean inside") {
        Fixture f = fixture({{5, 5, 5}, {5, -1, 5}, {5, 5, 5}});
        expect_precondition_error(f, "inner sea");
    }
    SUBCASE("a layer of the wrong size") {
        const Fixture f = example_51();
        const std::vector<std::int16_t> shorter(f.e.begin(), f.e.end() - 1);
        try {
            static_cast<void>(hydro_api::layers(f.grid, shorter, tuning(SHIPPED_AREA), 1));
            FAIL("no fatal error for a short layer");
        } catch (const worldgen::FatalError& ex) {
            CHECK(std::string(ex.what()).rfind(std::string(PRECONDITION), 0) == 0);
        }
    }
}

TEST_CASE("P1-02 2: a valid fixture whose ocean touches only one edge, and land on the other edges, is accepted") {
    // 8.1 item 3's grid has land on the east edge; also an ocean only along
    // the north edge.
    const Fixture f = fixture({{-2, -2, -2, -2}, {3, 4, 5, 6}, {7, 8, 9, 10}});
    const hydro_api::Layers l = run(f, 256);
    CHECK(invariant_violation(f.grid, f.e, l, 1).empty());
    CHECK(l.flow_dir[f.grid.index(0, 2)] == 6);  // straight north, through (0, 1)
}

TEST_CASE("P1-02 H6: water_depth bounds: depth 30000 on ocean, a lake 30000 deep") {
    // Ocean at -30000 and a lake with level 30000 over a bed of 1 is
    // outside the stage's real range (elevation is in [-30000, 30000]) but
    // fits u16 (H6): depth 29999 on the lake.
    const Fixture f = fixture({
        {-30000, -30000, -30000, -30000, -30000},
        {-30000, 30000, 30000, 30000, -30000},
        {-30000, 30000, 1, 30000, -30000},
        {-30000, 30000, 30000, 30000, -30000},
        {-30000, -30000, -30000, -30000, -30000},
    });
    const hydro_api::Layers l = run(f, 256);
    CHECK(l.water_depth[0] == 30000U);
    CHECK(l.water_depth[f.grid.index(2, 2)] == 29999U);
    CHECK(l.lake[f.grid.index(2, 2)] == 1U);
    CHECK(invariant_violation(f.grid, f.e, l, 1).empty());
}
