// The hydrology stage's step functions (spec P1-02 sections 2, 4, H1 to H7):
// check_hydrology_preconditions, hydrology_network, river_threshold_cells,
// river_class and coast_distance, each against the spec's values and against
// the test's own arithmetic and the stage's whole result. Also the full
// i16 range of elevation (the lead's decision of 2026-11-16: the stage handles
// heights beyond the 30000 of P1-02 section 7).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "p1_02_hydrology_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "worldgen/core/error.hpp"

using namespace hydro_test;
using worldgen::geo::Grid;

namespace {

template <typename T>
std::vector<T> flat_rows(const std::vector<std::vector<T>>& rows) {
    return flat(rows);
}

void expect_precondition(const Fixture& f) {
    try {
        hydro_api::check_preconditions(f.grid, f.e);
        FAIL("no fatal error");
    } catch (const worldgen::FatalError& ex) {
        CHECK(std::string(ex.what()).rfind(std::string(PRECONDITION), 0) == 0);
    }
}

}  // namespace

TEST_CASE("P1-02 4: river_threshold_cells, including the shipped 125000 (T = 489)") {
    CHECK(hydro_api::threshold_cells(256) == 1);
    CHECK(hydro_api::threshold_cells(257) == 2);
    CHECK(hydro_api::threshold_cells(512) == 2);
    CHECK(hydro_api::threshold_cells(513) == 3);
    CHECK(hydro_api::threshold_cells(768) == 3);
    CHECK(hydro_api::threshold_cells(769) == 4);
    CHECK(hydro_api::threshold_cells(124928) == 488);
    CHECK(hydro_api::threshold_cells(124929) == 489);
    CHECK(hydro_api::threshold_cells(125000) == 489);
    CHECK(hydro_api::threshold_cells(125184) == 489);
    CHECK(hydro_api::threshold_cells(125185) == 490);
    CHECK(hydro_api::threshold_cells(500000) == 1954);
    CHECK(hydro_api::threshold_cells(1000000000) == 3906250);
    CHECK(hydro_api::threshold_cells(SHIPPED_AREA) == threshold(SHIPPED_AREA));
    CHECK_THROWS_AS(static_cast<void>(hydro_api::threshold_cells(255)), worldgen::FatalError);
    CHECK_THROWS_AS(static_cast<void>(hydro_api::threshold_cells(1000000001)), worldgen::FatalError);
}

TEST_CASE("P1-02 H6, 8.1 item 7: river_class, the spec's list and the test's formula") {
    CHECK(hydro_api::stage_river_class(3, 3) == 1);
    CHECK(hydro_api::stage_river_class(5, 3) == 1);
    CHECK(hydro_api::stage_river_class(6, 3) == 2);
    CHECK(hydro_api::stage_river_class(11, 3) == 2);
    CHECK(hydro_api::stage_river_class(12, 3) == 3);
    CHECK(hydro_api::stage_river_class(3 * (std::int64_t{1} << 14), 3) == 15);
    CHECK(hydro_api::stage_river_class(3 * (std::int64_t{1} << 20), 3) == 15);
    CHECK(hydro_api::stage_river_class(2, 3) == 0);
    CHECK(hydro_api::stage_river_class(0, 1) == 0);
    for (const std::int64_t t : {std::int64_t{1}, std::int64_t{2}, std::int64_t{3}, std::int64_t{489},
                                 std::int64_t{1954}}) {
        for (std::int64_t a = 0; a < 70000; a += 1 + a / 64) {
            CAPTURE(t);
            CAPTURE(a);
            REQUIRE(hydro_api::stage_river_class(a, t) ==
                    river_class(static_cast<std::uint64_t>(a), static_cast<std::uint64_t>(t)));
        }
    }
    CHECK_THROWS_AS(static_cast<void>(hydro_api::stage_river_class(5, 0)), worldgen::FatalError);
    CHECK_THROWS_AS(static_cast<void>(hydro_api::stage_river_class(-1, 3)), worldgen::FatalError);
}

TEST_CASE("P1-02 H1: check_hydrology_preconditions") {
    hydro_api::check_preconditions(example_51().grid, example_51().e);  // valid: no throw
    Fixture zero = example_51();
    zero.e[zero.grid.index(3, 3)] = 0;
    expect_precondition(zero);
    Fixture dry = example_51();
    for (auto& v : dry.e) {
        v = static_cast<std::int16_t>(v < 0 ? -v : v);
    }
    expect_precondition(dry);
    Fixture pocket = example_51();
    pocket.e[pocket.grid.index(3, 3)] = -4;
    expect_precondition(pocket);
    expect_precondition(fixture({{-1, 5, 5, 5}, {5, -1, 5, 5}, {5, 5, 5, 5}, {5, 5, 5, 5}}));
}

TEST_CASE("P1-02 5.1, 8.1 item 1: hydrology_network gives L, parent, order, G4, lakes, the outlet, rcv and A") {
    const Fixture f = example_51();
    const Grid g = f.grid;
    const hydro_api::Network net = hydro_api::network(g, f.e, {}, 1);
    CHECK(net.level_dm == flat_rows<std::int64_t>({
                           {0, 0, 0, 0, 0, 0, 0, 0},
                           {0, 60, 70, 80, 70, 60, 50, 0},
                           {0, 50, 28, 28, 60, 40, 30, 0},
                           {0, 45, 28, 28, 55, 35, 25, 0},
                           {0, 40, 30, 28, 40, 30, 20, 0},
                           {0, 35, 33, 31, 25, 20, 10, 0},
                           {0, 0, 0, 0, 0, 0, 0, 0},
                       }));
    CHECK(net.parent == flat_rows<std::uint8_t>({
                            {8, 8, 8, 8, 8, 8, 8, 8},
                            {8, 5, 5, 5, 5, 5, 5, 8},
                            {8, 5, 2, 3, 3, 1, 7, 8},
                            {8, 5, 1, 2, 3, 1, 7, 8},
                            {8, 5, 0, 1, 1, 1, 7, 8},
                            {8, 5, 3, 3, 3, 3, 7, 8},
                            {8, 8, 8, 8, 8, 8, 8, 8},
                        }));
    CHECK(net.order == std::vector<std::uint32_t>{0,  1,  2,  3,  4,  5,  6,  7,  8,  15, 16, 23, 24, 31,
                                                  32, 39, 40, 47, 48, 49, 50, 51, 52, 53, 54, 55, 46, 38,
                                                  45, 30, 44, 35, 26, 27, 18, 19, 22, 37, 34, 43, 42, 41,
                                                  29, 33, 36, 21, 25, 14, 17, 28, 9,  13, 20, 10, 12, 11});
    CHECK(net.g4[g.index(2, 2)] == 2);
    CHECK(net.g4[g.index(3, 2)] == 3);
    CHECK(net.g4[g.index(2, 3)] == 1);
    CHECK(net.g4[g.index(3, 3)] == 2);
    CHECK(net.lake_level == std::vector<std::int64_t>{28});
    CHECK(net.outlet == std::vector<std::uint32_t>{g.index(2, 3)});
    const hydro_api::Layers l = hydro_api::layers(g, f.e, tuning(EXAMPLE_AREA), 1);
    CHECK(net.rcv == l.flow_dir);
    CHECK(net.accum == l.flow_accum);
    CHECK(net.lake == l.lake);
}

TEST_CASE("P1-02 H2 to H5: hydrology_network agrees with the test's floods and the stage's layers") {
    for (std::uint64_t k = 0; k < 40; ++k) {
        CAPTURE(k);
        const Fixture f = random_fixture(k);
        const hydro_api::Network net = hydro_api::network(f.grid, f.e, {}, 1 + static_cast<std::uint32_t>(k % 5));
        const OwnFlood own = own_flood(f.grid, keys_of(f.e), ocean_of(f.e), true);
        REQUIRE(net.level_dm == own.level);
        REQUIRE(net.parent == own.parent);
        REQUIRE(net.order == own.order);
        const hydro_api::Layers l = hydro_api::layers(f.grid, f.e, tuning(RANDOM_AREA), THREADS);
        REQUIRE(net.rcv == l.flow_dir);
        REQUIRE(net.accum == l.flow_accum);
        REQUIRE(net.lake == l.lake);
        const std::uint32_t lakes = *std::max_element(l.lake.begin(), l.lake.end());
        REQUIRE(net.lake_level.size() == lakes);
        REQUIRE(net.outlet.size() == lakes);
        // Each outlet is the lake's first cell in pop order, and its level the
        // surface of its cells.
        for (std::uint32_t id = 1; id <= lakes; ++id) {
            const auto first = std::find_if(own.order.begin(), own.order.end(),
                                            [&](std::uint32_t c) { return l.lake[c] == id; });
            REQUIRE(first != own.order.end());
            CHECK(net.outlet[id - 1U] == *first);
            CHECK(net.lake_level[id - 1U] == own.level[*first]);
        }
    }
}

TEST_CASE("P1-02 8.1 items 5 and 6: outlets and levels of the designed fixtures from hydrology_network") {
    const Fixture t = terrace_fixture();
    const hydro_api::Network nt = hydro_api::network(t.grid, t.e, {}, 1);
    CHECK(nt.lake_level == std::vector<std::int64_t>{150, 80});
    CHECK(nt.outlet == std::vector<std::uint32_t>{t.grid.index(5, 3), t.grid.index(2, 8)});
    const Fixture r = rims_fixture();
    const hydro_api::Network nr = hydro_api::network(r.grid, r.e, {}, 1);
    CHECK(nr.lake_level == std::vector<std::int64_t>{40});
    CHECK(nr.outlet == std::vector<std::uint32_t>{r.grid.index(5, 3)});
}

TEST_CASE("P1-02 H7, 8.1 item 8: coast_distance") {
    const Fixture f = example_51();
    CHECK(hydro_api::coast_distance(f.grid, ocean_of(f.e)) ==
          flat_rows<std::int16_t>({
              {-22, -16, -16, -16, -16, -16, -16, -22},
              {-16, 16, 16, 16, 16, 16, 16, -16},
              {-16, 16, 32, 32, 32, 32, 16, -16},
              {-16, 16, 32, 48, 48, 32, 16, -16},
              {-16, 16, 32, 32, 32, 32, 16, -16},
              {-16, 16, 16, 16, 16, 16, 16, -16},
              {-22, -16, -16, -16, -16, -16, -16, -22},
          }));
    const Grid g{9, 9};
    std::vector<std::uint8_t> ocean(81, 0);
    ocean[g.index(4, 4)] = 1;
    const std::vector<std::int16_t> cd = hydro_api::coast_distance(g, ocean);
    CHECK(cd[g.index(4, 4)] == -16);
    CHECK(cd[g.index(5, 4)] == 16);
    CHECK(cd[g.index(5, 5)] == 22);
    CHECK(cd[g.index(6, 5)] == 38);
    CHECK(cd[g.index(8, 8)] == 90);
    CHECK(cd[g.index(8, 4)] == 64);
    CHECK(cd[g.index(0, 0)] == 90);
    // No land, no ocean: the INF distance saturates (section 7).
    const Grid s{4, 3};
    for (const std::int16_t v : hydro_api::coast_distance(s, std::vector<std::uint8_t>(12, 1))) {
        CHECK(v == -32767);
    }
    for (const std::int16_t v : hydro_api::coast_distance(s, std::vector<std::uint8_t>(12, 0))) {
        CHECK(v == 32767);
    }
    // Random masks: the test's Dijkstra chamfer.
    for (std::uint64_t k = 0; k < 10; ++k) {
        const Fixture r = random_fixture(k);
        std::vector<std::uint8_t> m(r.e.size());
        for (std::size_t i = 0; i < m.size(); ++i) {
            m[i] = r.e[i] % 3 == 0 ? 1 : 0;
        }
        std::vector<std::uint8_t> land(m.size());
        for (std::size_t i = 0; i < m.size(); ++i) {
            land[i] = m[i] != 0U ? 0 : 1;
        }
        const std::vector<std::int64_t> dl = elev_test::chamfer_dijkstra(r.grid, m);
        const std::vector<std::int64_t> dw = elev_test::chamfer_dijkstra(r.grid, land);
        const std::vector<std::int16_t> got = hydro_api::coast_distance(r.grid, m);
        for (std::size_t i = 0; i < m.size(); ++i) {
            REQUIRE(got[i] == (m[i] != 0U ? static_cast<std::int16_t>(-coast_metres(dw[i])) : coast_metres(dl[i])));
        }
    }
}

TEST_CASE("P1-02 H2, H6 (lead 2026-11-16): the full i16 range, land at 32767 and ocean at -32768") {
    // Ocean -32768 on the border; land 32767 inside with a pit of 1 at the
    // centre, and a notch of 32766 to the sea at (3, 1) so the walls drain.
    const Fixture f = fixture({
        {-32768, -32768, -32768, -32768, -32768, -32768, -32768},
        {-32768, 32767, 32767, 32766, 32767, 32767, -32768},
        {-32768, 32767, 32767, 32767, 32767, 32767, -32768},
        {-32768, 32767, 32767, 1, 32767, 32767, -32768},
        {-32768, 32767, 32767, 32767, 32767, 32767, -32768},
        {-32768, 32767, 32767, 32767, 32767, 32767, -32768},
        {-32768, -32768, -32768, -32768, -32768, -32768, -32768},
    });
    const Grid g = f.grid;
    const hydro_api::Layers l = hydro_api::layers(g, f.e, tuning(256), THREADS);
    CHECK(invariant_violation(g, f.e, l, 1).empty());
    // Ocean depth 32768 fits u16.
    CHECK(l.water_depth[0] == 32768U);
    CHECK(l.water[0] == 3);
    // The pit is a lake at the level 32767 (its rim), depth 32766.
    const std::size_t c = g.index(3, 3);
    CHECK(l.lake[c] == 1U);
    CHECK(l.water[c] == 2);
    CHECK(l.water_depth[c] == 32766U);
    CHECK(*std::max_element(l.lake.begin(), l.lake.end()) == 1U);
    // Drops of 32767 dm to the sea (L = 0 on ocean): every land cell beside
    // the sea drains straight to it.
    const hydro_api::Network net = hydro_api::network(g, f.e, {}, 1);
    CHECK(net.level_dm[c] == 32767);
    CHECK(net.lake_level == std::vector<std::int64_t>{32767});
    CHECK(l.flow_dir[g.index(1, 3)] == 4);
    CHECK(l.flow_dir[g.index(5, 3)] == 0);
    CHECK(l.flow_dir[g.index(3, 5)] == 2);
    // With the largest tie values the flood keys reach 32767 x 2^16 + 65535 <
    // 2^31 (P1-02 H2 step 2); invariant 11 holds.
    std::vector<std::uint16_t> tmax(f.e.size(), 0);
    for (std::size_t i = 0; i < f.e.size(); ++i) {
        tmax[i] = f.e[i] > 0 ? 65535 : 0;
    }
    const hydro_api::Layers lt = hydro_api::layers_tie(g, f.e, tmax, tuning(256), THREADS);
    CHECK(invariant_violation(g, f.e, lt, 1).empty());
    CHECK(tie_invariance_violation(l, lt).empty());
    const hydro_api::Network nt = hydro_api::network(g, f.e, tmax, 1);
    CHECK(nt.level_dm[c] == 32767);
    CHECK(nt.order == own_flood_tie(g, f.e, tmax, false).order);
    // Threads do not matter here either.
    CHECK(hydro_api::layers(g, f.e, tuning(256), 1) == l);
    CHECK(hydro_api::layers(g, f.e, tuning(256), 5) == l);
}
