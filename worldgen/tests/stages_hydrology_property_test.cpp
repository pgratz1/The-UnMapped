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
#include <vector>

#include "p1_02_hydrology_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "worldgen/core/streams.hpp"

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

// Invariants of one world from the elevation stage; returns its statistics.
WorldStats check_world(std::uint64_t seed, std::int32_t n, bool small, std::int64_t area) {
    CAPTURE(seed);
    CAPTURE(n);
    CAPTURE(small);
    CAPTURE(area);
    const std::vector<std::int16_t> e =
        hydro_api::elevation_grid(seed, n, elevation_tuning(small), THREADS);
    const Grid g{n, n};
    const hydro_api::Layers l = hydro_api::layers(g, e, tuning(area), THREADS);
    const std::string why = invariant_violation(g, e, l, threshold(area));
    CHECK_MESSAGE(why.empty(), why);
    return stats_of(g, e, l);
}

}  // namespace

TEST_CASE("P1-02 8.2: the random fixture stream is registered as a test-only stream name") {
    const auto& names = worldgen::streams::all_stream_names;
    CHECK(std::find(names.begin(), names.end(), RANDOM_STREAM) != names.end());
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
