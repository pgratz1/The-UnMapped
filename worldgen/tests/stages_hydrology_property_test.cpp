// Property tests of the hydrology stage (spec P1-02 sections 6 and 8.2, 8.3):
// invariants 1 to 10 checked with code written independently of the stage
// (p1_02_hydrology_support.hpp: minimax flood levels, accumulation by
// walking upstream, union-find lakes, Dijkstra chamfer), over random
// fixtures and over worlds from the elevation stage; queue independence of
// the flood.
//
// 8.2 asks for seeds 0 to 63 at N = 128 (both fixtures) and 0 to 3 at
// N = 512. The default run covers seeds 0 to 5 at N = 128 with each
// elevation fixture and the 200 random fixtures; the full sweep is in the
// opt-in suite "hydrology-slow":
//   worldgen-tests -ts=hydrology-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "p1_02_hydrology_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/geo/noise.hpp"

using namespace hydro_test;
using worldgen::geo::Grid;

namespace {

struct WorldStats {
    std::int64_t lakes = 0, lake_cells = 0, river_cells = 0, mouths = 0, outlets_into_rivers = 0;
};

WorldStats stats_of(Grid g, std::span<const std::int16_t> e, const hydro_api::Layers& l) {
    WorldStats s;
    for (std::size_t i = 0; i < e.size(); ++i) {
        s.lakes = std::max<std::int64_t>(s.lakes, l.lake[i]);
        s.lake_cells += l.water[i] == 2U ? 1 : 0;
        s.river_cells += l.water[i] == 1U ? 1 : 0;
        if (e[i] > 0) {
            const std::size_t m = static_cast<std::size_t>(step_to(g, i, l.flow_dir[i]));
            s.mouths += (l.water[i] == 1U && e[m] < 0) ? 1 : 0;
            s.outlets_into_rivers += (l.lake[i] > 0U && l.lake[m] != l.lake[i] && l.water[m] == 1U) ? 1 : 0;
        }
    }
    return s;
}

// Invariants of one world from the elevation stage, run with flat_tie of
// its seed (as the stage runs) and with t = 0; invariant 11 between the two.
// Returns the statistics of the flat_tie run.
WorldStats check_world(std::uint64_t seed, std::int32_t n, bool small, std::int64_t area) {
    CAPTURE(seed);
    CAPTURE(n);
    CAPTURE(small);
    CAPTURE(area);
    const std::vector<std::int16_t> e =
        hydro_api::elevation_grid(seed, n, elevation_tuning(small), THREADS);
    const Grid g{n, n};
    const hydro_api::Layers l = hydro_api::world_layers(seed, g, e, tuning(area), THREADS);
    const std::string why = invariant_violation(g, e, l, threshold(area));
    CHECK_MESSAGE(why.empty(), why);
    const hydro_api::Layers z = hydro_api::layers(g, e, tuning(area), THREADS);
    const std::string why0 = invariant_violation(g, e, z, threshold(area));
    CHECK_MESSAGE(why0.empty(), why0);
    const std::string why11 = tie_invariance_violation(z, l);
    CHECK_MESSAGE(why11.empty(), why11);
    return stats_of(g, e, l);
}

}  // namespace

TEST_CASE("P1-02 8.2: the random fixture stream is registered as a test-only stream name") {
    const auto& names = worldgen::streams::all_stream_names;
    CHECK(std::find(names.begin(), names.end(), RANDOM_STREAM) != names.end());
}

TEST_CASE("P1-02 2, P1-00 3: the stream geo.hydrology.flat is registered") {
    const auto& names = worldgen::streams::all_stream_names;
    CHECK(std::find(names.begin(), names.end(), std::string_view("geo.hydrology.flat")) != names.end());
}

TEST_CASE("P1-02 H2 step 1, 8.2: flat_tie: 0 on ocean, [0, 65535] on land, the test's own fbm_m at three cells") {
    const std::vector<std::int16_t> e = hydro_api::elevation_grid(0, 64, elevation_tuning(false), THREADS);
    const Grid g{64, 64};
    const std::vector<std::uint8_t> ocean = ocean_of(e);
    const std::vector<std::uint16_t> t = hydro_api::flat_tie(0, g, ocean, shipped_tuning(), THREADS);
    REQUIRE(t.size() == e.size());
    std::vector<std::size_t> land;
    for (std::size_t i = 0; i < e.size(); ++i) {
        if (ocean[i] != 0U) {
            CHECK(t[i] == 0U);
        } else {
            land.push_back(i);
        }
    }
    REQUIRE(land.size() > 100U);
    // Not constant on land.
    CHECK(std::any_of(land.begin(), land.end(), [&](std::size_t i) { return t[i] != t[land[0]]; }));
    // The first, middle and last land cell (index order): the test's own
    // fbm_m (P1-01 4.1) with the stream key, and the reference values.
    const std::uint64_t key = worldgen::stream_key(0, "geo.hydrology.flat");
    const worldgen::geo::FbmM rec{400, 1, worldgen::Fx::HALF};
    const std::vector<std::size_t> probe = {land[0], land[land.size() / 2], land.back()};
    const std::vector<std::uint16_t> reference = {28044, 30479, 36154};  // p1_hydrology_reference.py check
    for (std::size_t j = 0; j < probe.size(); ++j) {
        const std::size_t i = probe[j];
        const std::int32_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(i));
        const std::int32_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(i));
        CAPTURE(x);
        CAPTURE(y);
        const worldgen::Fx nz =
            worldgen::geo::fbm_m(key, worldgen::Fx::from_int(16 * x + 8), worldgen::Fx::from_int(16 * y + 8), rec);
        const std::int64_t own = std::min<std::int64_t>(65535, (nz.raw() + (std::int64_t{1} << 32)) >> 17);
        CHECK(std::int64_t{t[i]} == own);
        CHECK(t[i] == reference[j]);
    }
    CHECK(g.x_of(static_cast<worldgen::geo::CellIndex>(probe[0])) == 34);
    CHECK(g.y_of(static_cast<worldgen::geo::CellIndex>(probe[0])) == 5);
    // A pure function of seed, grid, ocean and tuning; another seed differs.
    CHECK(hydro_api::flat_tie(0, g, ocean, shipped_tuning(), 1) == t);
    CHECK(hydro_api::flat_tie(1, g, ocean, shipped_tuning(), THREADS) != t);
    // Another flat_noise gives other values.
    CHECK(hydro_api::flat_tie(0, g, ocean, tuning_noise("{ wavelength_m = 64, octaves = 3, gain = \"0.5\" }"),
                              THREADS) != t);
}

TEST_CASE("P1-02 H2 step 1, D10: flat_tie at 1, 2, 5 and 256 threads (N = 128 and a ragged 37 x 200 grid)") {
    const std::vector<std::int16_t> e = hydro_api::elevation_grid(42, 128, elevation_tuning(false), THREADS);
    const std::vector<std::uint8_t> ocean = ocean_of(e);
    const std::vector<std::uint16_t> a = hydro_api::flat_tie(42, Grid{128, 128}, ocean, shipped_tuning(), 1);
    for (const std::uint32_t threads : {2U, 5U, 256U}) {
        CHECK(hydro_api::flat_tie(42, Grid{128, 128}, ocean, shipped_tuning(), threads) == a);
    }
    const Grid r{37, 200};
    std::vector<std::uint8_t> ro(elev_test::cells(r), 0);
    for (std::size_t i = 0; i < ro.size(); i += 7) {
        ro[i] = 1;
    }
    const std::vector<std::uint16_t> b = hydro_api::flat_tie(3, r, ro, shipped_tuning(), 1);
    for (const std::uint32_t threads : {2U, 3U, 5U}) {
        CHECK(hydro_api::flat_tie(3, r, ro, shipped_tuning(), threads) == b);
    }
}

TEST_CASE("P1-02 6 invariants 1 to 11, 8.2: the random fixtures with random tie values, T = 8 and T = 1") {
    std::int64_t changed = 0;
    for (std::uint64_t f = 0; f < 200; ++f) {
        CAPTURE(f);
        const Fixture fx = random_fixture(f);
        const std::vector<std::uint16_t> tie = random_tie(f, fx.e);
        for (const std::int64_t area : {RANDOM_AREA, std::int64_t{256}}) {
            CAPTURE(area);
            const hydro_api::Layers z = hydro_api::layers(fx.grid, fx.e, tuning(area), THREADS);
            const hydro_api::Layers l = hydro_api::layers_tie(fx.grid, fx.e, tie, tuning(area), THREADS);
            const std::string why = invariant_violation(fx.grid, fx.e, l, threshold(area));
            REQUIRE_MESSAGE(why.empty(), why);
            const std::string why11 = tie_invariance_violation(z, l);
            REQUIRE_MESSAGE(why11.empty(), why11);
            changed += l.flow_dir != z.flow_dir ? 1 : 0;
        }
        // The decimetre levels and lake levels of the network are those of t = 0.
        const hydro_api::Network n0 = hydro_api::network(fx.grid, fx.e, {}, THREADS);
        const hydro_api::Network nt = hydro_api::network(fx.grid, fx.e, tie, THREADS);
        REQUIRE(nt.level_dm == n0.level_dm);
        REQUIRE(nt.lake_level == n0.lake_level);
        REQUIRE(nt.lake == n0.lake);
        // L = Lk >> 16, and Lk is the minimax level of the fine keys.
        for (std::size_t i = 0; i < nt.level.size(); ++i) {
            REQUIRE(nt.level_dm[i] == (nt.level[i] >> 16));
        }
        REQUIRE(nt.level == elev_test::minimax_levels(fx.grid, fine_keys(fx.e, tie), ocean_of(fx.e)));
    }
    // The tie values do reroute flats.
    CHECK(changed >= 100);
}

TEST_CASE("P1-02 H2: t = 0 given explicitly equals the empty tie span, and equals t = 0 on 5.1") {
    for (std::uint64_t f = 0; f < 10; ++f) {
        const Fixture fx = random_fixture(f);
        const std::vector<std::uint16_t> zeros(fx.e.size(), 0);
        CHECK(hydro_api::layers_tie(fx.grid, fx.e, zeros, tuning(RANDOM_AREA), THREADS) ==
              hydro_api::layers(fx.grid, fx.e, tuning(RANDOM_AREA), THREADS));
    }
}

TEST_CASE("P1-02 H2: a tie value only orders cells of equal height (a crafted flat)") {
    // A 1-row flat of 10 at x = 1 .. 6 between ocean at both ends. The two
    // end cells drain straight to the sea; with t rising to the east the
    // inner cells drain west (to the lower fine level), with t rising to the
    // west they drain east.
    const Fixture f = fixture({{-1, 10, 10, 10, 10, 10, 10, -1}});
    std::vector<std::uint16_t> east(8, 0), west(8, 0);
    for (std::int32_t x = 1; x <= 6; ++x) {
        east[static_cast<std::size_t>(x)] = static_cast<std::uint16_t>(1000 * x);
        west[static_cast<std::size_t>(x)] = static_cast<std::uint16_t>(1000 * (7 - x));
    }
    const hydro_api::Layers le = hydro_api::layers_tie(f.grid, f.e, east, tuning(256), 1);
    const hydro_api::Layers lw = hydro_api::layers_tie(f.grid, f.e, west, tuning(256), 1);
    CHECK(le.flow_dir == std::vector<std::uint8_t>{8, 4, 4, 4, 4, 4, 0, 8});
    CHECK(lw.flow_dir == std::vector<std::uint8_t>{8, 4, 0, 0, 0, 0, 0, 8});
    CHECK(le.flow_accum == std::vector<std::uint32_t>{0, 5, 4, 3, 2, 1, 1, 0});
    CHECK(lw.flow_accum == std::vector<std::uint32_t>{0, 1, 1, 2, 3, 4, 5, 0});
    CHECK(invariant_violation(f.grid, f.e, le, 1).empty());
    CHECK(invariant_violation(f.grid, f.e, lw, 1).empty());
    // A tie never lets a higher decimetre cell drain before a lower one: a
    // step of 1 dm beats the largest tie difference.
    const Fixture s = fixture({{-1, 11, 10, -1}});
    const std::vector<std::uint16_t> t = {0, 0, 65535, 0};
    const hydro_api::Layers ls = hydro_api::layers_tie(s.grid, s.e, t, tuning(256), 1);
    CHECK(ls.flow_dir[2] == 0);  // 10 dm drains to the sea at x = 3, not to 11 dm
    CHECK(invariant_violation(s.grid, s.e, ls, 1).empty());
}

TEST_CASE("P1-02 6, 8.2: invariants 1 to 9 on 200 random fixtures (34 x 34, e in [1, 200], ocean border)") {
    std::int64_t lakes = 0, multi_lake = 0;
    for (std::uint64_t f = 0; f < 200; ++f) {
        CAPTURE(f);
        const Fixture fx = random_fixture(f);
        const hydro_api::Layers l = hydro_api::layers(fx.grid, fx.e, tuning(RANDOM_AREA), THREADS);
        const std::string why = invariant_violation(fx.grid, fx.e, l, threshold(RANDOM_AREA));
        REQUIRE_MESSAGE(why.empty(), why);
        const std::uint32_t k = *std::max_element(l.lake.begin(), l.lake.end());
        lakes += k;
        multi_lake += k >= 2U ? 1 : 0;
    }
    // The fixtures do what 8.2 wants them for: many depressions and lakes.
    CHECK(lakes >= 200);
    CHECK(multi_lake >= 100);
}

TEST_CASE("P1-02 6, 8.2: invariants on the random fixtures with T = 1 (every dry cell a river)") {
    for (std::uint64_t f = 0; f < 40; ++f) {
        CAPTURE(f);
        const Fixture fx = random_fixture(f);
        const hydro_api::Layers l = hydro_api::layers(fx.grid, fx.e, tuning(256), THREADS);
        const std::string why = invariant_violation(fx.grid, fx.e, l, 1);
        REQUIRE_MESSAGE(why.empty(), why);
        for (std::size_t i = 0; i < fx.e.size(); ++i) {
            CHECK(l.water[i] == (fx.e[i] < 0 ? 3 : (l.lake[i] > 0U ? 2 : 1)));
        }
    }
}

TEST_CASE("P1-02 6 invariant 10, 8.3: pure function; 1, 2 and 5 threads; random fixtures and odd sizes") {
    for (std::uint64_t f = 0; f < 20; ++f) {
        CAPTURE(f);
        const Fixture fx = random_fixture(f);
        const hydro_api::Layers a = hydro_api::layers(fx.grid, fx.e, tuning(RANDOM_AREA), 1);
        CHECK(hydro_api::layers(fx.grid, fx.e, tuning(RANDOM_AREA), 1) == a);
        CHECK(hydro_api::layers(fx.grid, fx.e, tuning(RANDOM_AREA), 2) == a);
        CHECK(hydro_api::layers(fx.grid, fx.e, tuning(RANDOM_AREA), 5) == a);
        const std::vector<std::uint16_t> tie = random_tie(f, fx.e);
        const hydro_api::Layers b = hydro_api::layers_tie(fx.grid, fx.e, tie, tuning(RANDOM_AREA), 1);
        CHECK(hydro_api::layers_tie(fx.grid, fx.e, tie, tuning(RANDOM_AREA), 2) == b);
        CHECK(hydro_api::layers_tie(fx.grid, fx.e, tie, tuning(RANDOM_AREA), 5) == b);
    }
    // A grid taller than several row blocks with a ragged last block.
    worldgen::Rng rng(worldgen::stream_key(2, RANDOM_STREAM));
    const Grid g{37, 200};
    std::vector<std::int16_t> e(elev_test::cells(g));
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const bool border = x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1;
            e[g.index(x, y)] = static_cast<std::int16_t>(border ? -7 : rng.uniform_i64(1, 60));
        }
    }
    const hydro_api::Layers a = hydro_api::layers(g, e, tuning(2560), 1);
    CHECK(invariant_violation(g, e, a, threshold(2560)).empty());
    for (const std::uint32_t threads : {2U, 3U, 4U, 5U, 256U}) {
        CAPTURE(threads);
        CHECK(hydro_api::layers(g, e, tuning(2560), threads) == a);
    }
}

TEST_CASE("P1-02 8.3: queue independence: a binary heap and a bucket queue give the same flood as G3") {
    for (std::uint64_t f = 0; f < 200; ++f) {
        CAPTURE(f);
        const Fixture fx = random_fixture(f);
        const std::vector<std::uint8_t> ocean = ocean_of(fx.e);
        const std::vector<std::int64_t> keys = keys_of(fx.e);
        const OwnFlood heap = own_flood(fx.grid, keys, ocean, false);
        const OwnFlood bucket = own_flood(fx.grid, keys, ocean, true);
        REQUIRE(heap.order == bucket.order);
        REQUIRE(heap.level == bucket.level);
        REQUIRE(heap.parent == bucket.parent);
        const hydro_api::Flood g3 = hydro_api::flood(fx.grid, keys, ocean);
        REQUIRE(g3.order == heap.order);
        REQUIRE(g3.level == heap.level);
        REQUIRE(g3.parent == heap.parent);
        // And the flood levels are the minimax spill levels.
        REQUIRE(heap.level == elev_test::minimax_levels(fx.grid, keys, ocean));
    }
}

TEST_CASE("P1-02 6, 8.2: invariants on worlds from the elevation stage, N = 128, seeds 0 to 5, both fixtures") {
    WorldStats total;
    for (const bool small : {false, true}) {
        for (std::uint64_t seed = 0; seed < 6; ++seed) {
            const WorldStats s = check_world(seed, 128, small, SHIPPED_AREA);
            total.lakes += s.lakes;
        }
    }
    // The small-map fixture keeps lakes at 128 (P1-01 4.4).
    CHECK(total.lakes >= 4);
}

TEST_CASE("P1-02 6, 8.2: invariants with a low river threshold (T = 100), rivers and lake outlets") {
    // With the fixture threshold (T = 489) a 128 map has few river cells;
    // T = 100 exercises the river invariants more: rivers reach the sea,
    // lakes feed rivers.
    WorldStats total;
    for (std::uint64_t seed = 0; seed < 3; ++seed) {
        const WorldStats s = check_world(seed, 128, true, 25600);
        total.river_cells += s.river_cells;
        total.mouths += s.mouths;
        total.outlets_into_rivers += s.outlets_into_rivers;
        total.lakes += s.lakes;
    }
    CHECK(total.river_cells > 0);
    CHECK(total.mouths > 0);
    CHECK(total.outlets_into_rivers > 0);
}

TEST_CASE("P1-02 6: invariants on non-power-of-two maps (N = 48, 100)") {
    static_cast<void>(check_world(3, 48, false, SHIPPED_AREA));
    static_cast<void>(check_world(5, 100, true, 25600));
}

// --- Slow sweep (opt-in) -------------------------------------------------------------

TEST_SUITE("hydrology-slow" * doctest::skip()) {

TEST_CASE("P1-02 8.2 (slow): invariants, N = 128 seeds 0 to 63 with both fixtures, N = 512 seeds 0 to 3") {
    for (const bool small : {false, true}) {
        for (std::uint64_t seed = 0; seed < 64; ++seed) {
            static_cast<void>(check_world(seed, 128, small, SHIPPED_AREA));
        }
    }
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        static_cast<void>(check_world(seed, 512, false, SHIPPED_AREA));
    }
}

}  // TEST_SUITE
