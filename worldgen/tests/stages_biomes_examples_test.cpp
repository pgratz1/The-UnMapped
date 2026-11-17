// Worked examples and unit tests of the biome stage (spec P1-04 5.1 and
// 10.1 tests 1 to 5, 8 and 9 at the seams; P1-00 G7 and G8).
//
// Written from the spec only. Every stage call goes through
// p1_04_biomes_api.hpp; the test's own B2 to B5 are in
// p1_04_biomes_support.hpp. The values here are those of the spec's tables,
// reproduced independently by reference/p1_biomes_reference.py check.

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "p1_02_hydrology_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "p1_04_biomes_api.hpp"
#include "p1_04_biomes_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/noise.hpp"

using namespace biome_test;

namespace {

// P1-02 5.1 through the hydrology stage (T = 3): the two river cells (3, 4)
// and (4, 5) are class 2, the lake (2..3, 2..3) has level 28.
struct Example {
    Grid g;
    std::vector<std::int16_t> e;
    hydro_api::Layers h;
    std::vector<std::int64_t> s;
};

const Example& example() {
    static const Example ex = [] {
        Example x;
        const hydro_test::Fixture f = hydro_test::example_51();
        x.g = f.grid;
        x.e = f.e;
        x.h = hydro_api::layers(f.grid, f.e, hydro_test::tuning(hydro_test::EXAMPLE_AREA), 1);
        x.s = surface(x.e, x.h.water_depth);
        return x;
    }();
    return ex;
}

const std::vector<std::vector<std::int64_t>> HAND_K1 = {
    {0, 0, 0, 0, 0, 0, 0, 0},     {0, 60, 70, 80, 70, 60, 50, 0}, {0, 50, 0, 0, 32, 40, 30, 0},
    {0, 45, 0, 0, 27, 35, 25, 0}, {0, 40, 2, 0, 15, 30, 20, 0},   {0, 35, 33, 31, 0, 20, 10, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
};

std::vector<std::int64_t> hand_k3() {
    std::vector<std::vector<std::int64_t>> rows = HAND_K1;
    rows[4] = {0, 40, 30, 28, 40, 30, 20, 0};
    rows[5] = {0, 35, 33, 31, 25, 20, 10, 0};
    return hydro_test::flat(rows);
}

std::string fatal_message(auto&& fn) {
    try {
        fn();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

bool starts(std::string_view s, std::string_view p) { return s.substr(0, p.size()) == p; }

std::uint8_t code_of(std::string_view key) {
    for (const Def& d : shipped_defs()) {
        if (d.key == key) {
            return d.code;
        }
    }
    FAIL("no definition " << std::string(key));
    return 0;
}

}  // namespace

// --- G7 (10.1 test 1) ----------------------------------------------------------------------

TEST_CASE("P1-04 5.1, P1-00 G7: drainage height on P1-02's worked example with k = 1 and k = 3") {
    const Example& ex = example();
    REQUIRE(ex.h.river[ex.g.index(3, 4)] == 2);
    REQUIRE(ex.h.river[ex.g.index(4, 5)] == 2);
    REQUIRE(ex.h.water[ex.g.index(3, 4)] == WATER_RIVER);
    REQUIRE(ex.s[ex.g.index(2, 2)] == 28);  // lake level
    const std::vector<std::int64_t> k1 = hydro_test::flat(HAND_K1);
    const std::vector<std::int64_t> k3 = hand_k3();
    CHECK(biome_api::hand(ex.g, ex.s, ex.h.water, ex.h.river, ex.h.flow_dir, 1) == k1);
    CHECK(biome_api::hand(ex.g, ex.s, ex.h.water, ex.h.river, ex.h.flow_dir, 3) == k3);
    // Class 2 rivers are drains for k <= 2 only.
    CHECK(biome_api::hand(ex.g, ex.s, ex.h.water, ex.h.river, ex.h.flow_dir, 2) == k1);
    CHECK(biome_api::hand(ex.g, ex.s, ex.h.water, ex.h.river, ex.h.flow_dir, 15) == k3);
    // The test's own G7 (recursion along flow_dir) agrees.
    CHECK(own_hand(ex.g, ex.s, ex.h.water, ex.h.river, ex.h.flow_dir, 1).value() == k1);
    CHECK(own_hand(ex.g, ex.s, ex.h.water, ex.h.river, ex.h.flow_dir, 3).value() == k3);
    // The hand-checked cells of 5.1.
    CHECK(k3[ex.g.index(3, 4)] == 28);
    CHECK(k3[ex.g.index(2, 4)] == 30);
    CHECK(k3[ex.g.index(4, 4)] == 40);
    CHECK(k1[ex.g.index(4, 2)] == 32);
    CHECK(k1[ex.g.index(2, 4)] == 2);
}

TEST_CASE("P1-04 10.1 test 1, P1-00 G7: a flow_dir cycle, a negative height and an unreachable drain are fatal") {
    // 4 x 1: (0) -> (1) -> (0) is a cycle; (3) is ocean.
    {
        const Grid g{4, 1};
        const std::vector<std::int64_t> s = {10, 10, 10, 0};
        const std::vector<std::uint8_t> water = {0, 0, 0, 3}, river = {0, 0, 0, 0}, fd = {0, 4, 0, 8};
        const std::string m = fatal_message([&] { (void)biome_api::hand(g, s, water, river, fd, 1); });
        CHECK(starts(m, G7_PREFIX));
        CHECK_FALSE(own_hand(g, s, water, river, fd, 1).has_value());
    }
    // 3 x 1: a dry cell (S 5) draining into a lake whose surface is 15.
    {
        const Grid g{3, 1};
        const std::vector<std::int64_t> s = {5, 15, 0};
        const std::vector<std::uint8_t> water = {0, 2, 3}, river = {0, 0, 0}, fd = {0, 0, 8};
        const std::string m = fatal_message([&] { (void)biome_api::hand(g, s, water, river, fd, 1); });
        CHECK(starts(m, G7_PREFIX));
    }
    // The only water is class-2 river cells, no lake or ocean: with k = 3 no
    // drain is reached; with k = 2 every cell is fine.
    {
        const Grid g{3, 1};
        const std::vector<std::int64_t> s = {9, 8, 7};
        const std::vector<std::uint8_t> water = {0, 1, 1}, river = {0, 2, 2}, fd = {0, 0, 4};
        CHECK(biome_api::hand(g, s, water, river, fd, 2) == std::vector<std::int64_t>{1, 0, 0});
        CHECK(starts(fatal_message([&] { (void)biome_api::hand(g, s, water, river, fd, 3); }), G7_PREFIX));
    }
    // k outside 1..15 is fatal with the G7 prefix (amended 7.7), also on
    // the worked example where every other check passes.
    {
        const Example& ex = example();
        for (const std::int64_t k : {0, 16, -1}) {
            CAPTURE(k);
            CHECK(starts(fatal_message([&] { (void)biome_api::hand(ex.g, ex.s, ex.h.water, ex.h.river, ex.h.flow_dir, k); }),
                         G7_PREFIX));
        }
        // Arrays of the wrong length: the seam's argument check, G7 prefix.
        const std::vector<std::uint8_t> short_fd(ex.h.flow_dir.begin(), ex.h.flow_dir.end() - 1);
        CHECK(starts(fatal_message([&] { (void)biome_api::hand(ex.g, ex.s, ex.h.water, ex.h.river, short_fd, 1); }),
                     G7_PREFIX));
    }
    // Not fatal: a cell whose path passes a higher non-drain cell before a
    // drain lower than itself (only the drain's S counts). 4 x 1: S 10, 20,
    // lake at 6, ocean.
    {
        const Grid g{4, 1};
        const std::vector<std::int64_t> s = {10, 20, 6, 0};
        const std::vector<std::uint8_t> water = {0, 0, 2, 3}, river = {0, 0, 0, 0}, fd = {0, 0, 0, 8};
        CHECK(biome_api::hand(g, s, water, river, fd, 1) == std::vector<std::int64_t>{4, 14, 0, 0});
        CHECK(own_hand(g, s, water, river, fd, 1).value() == std::vector<std::int64_t>{4, 14, 0, 0});
    }
}

// --- G8 thresholds ---------------------------------------------------------------------------

TEST_CASE("P1-04 5.1, P1-00 G8: slope_max 0.05 is raw 214748365; S2 256 is not steeper, 257 is") {
    CHECK(fx_of("0.05") == 214748365);
    CHECK(not_steeper(256, 214748365));
    CHECK_FALSE(not_steeper(257, 214748365));
    CHECK(not_steeper(0, 0));
    CHECK_FALSE(not_steeper(1, 0));
    // P1-00 7.8: t = 0.7002, the largest S2 not steeper is 50204.
    CHECK(not_steeper(50204, fx_of("0.7002")));
    CHECK_FALSE(not_steeper(50205, fx_of("0.7002")));
    // Through the stage's lookup: B3 and B4 of the table differ only in S2.
    const std::int64_t sc = biome_api::rain_scale(tuning(), 1024);
    const Jittered j = own_jitter(80, 900, 0, 0, values());
    for (std::int64_t s2 = 250; s2 <= 262; ++s2) {
        CAPTURE(s2);
        CHECK(biome_api::lookup(shipped_definitions(), sc, j.tq, j.rq, 30, s2, 8, 200) ==
              (s2 <= 256 ? MARSH : FOREST));
    }
}

// --- B4 lookups (10.1 test 2) ------------------------------------------------------------------

namespace {

struct Lookup {
    const char* name;
    std::int64_t n, temp, rain, elev, s2, hand, coast, nt, nr;
    std::int64_t tq, rq;  // raw, or INT64_MIN when the table gives no raw value
    const char* biome;
};

constexpr std::int64_t NONE = INT64_MIN;

const std::vector<Lookup>& lookups() {
    static const std::vector<Lookup> t = {
        {"B1", 1024, 3, 1200, 8000, 200000, 500, 3000, 0, 0, NONE, NONE, "alpine"},
        {"B2", 1024, 3, 1200, 5000, 200000, 500, 3000, 0, 0, NONE, NONE, "highland_moor"},
        {"B3", 1024, 80, 900, 30, 100, 8, 200, 0, 0, NONE, NONE, "marsh"},
        {"B4", 1024, 80, 900, 30, 257, 8, 200, 0, 0, NONE, NONE, "temperate_forest"},
        {"B5", 1024, 80, 900, 30, 256, 10, 200, 0, 0, NONE, NONE, "marsh"},
        {"B6", 1024, 80, 900, 30, 100, 11, 200, 0, 0, NONE, NONE, "temperate_forest"},
        {"B7", 1024, 80, 900, 2001, 100, 5, 200, 0, 0, NONE, NONE, "temperate_forest"},
        {"B8", 1024, 80, 719, 400, 100, 100, 2000, 0, 0, NONE, NONE, "grassland"},
        {"B9", 1024, 80, 720, 400, 100, 100, 2000, 0, 0, NONE, NONE, "temperate_forest"},
        {"B10", 1024, 40, 650, 3000, 100, 100, 2000, 0, 0, NONE, NONE, "highland_moor"},
        {"B11", 1024, 41, 650, 3000, 100, 100, 2000, 0, 0, NONE, NONE, "grassland"},
        {"B12", 1024, 80, 650, 400, 100, 100, 2000, 0, HALF, NONE, 3070901616900, "grassland"},
        {"B13", 1024, 80, 650, 400, 100, 100, 2000, 0, 2576980378, NONE, 3126736191150, "temperate_forest"},
        {"B14", 1024, 10, 650, 6500, 100, 100, 2000, -HALF, 0, 2147483648, NONE, "alpine"},
        {"B15", 1024, 10, 650, 6500, 100, 100, 2000, -1717986918, 0, 2576980378, NONE, "highland_moor"},
        {"B16", 512, 80, 800, 400, 100, 100, 2000, 0, 0, NONE, NONE, "grassland"},
        {"B17", 512, 80, 810, 400, 100, 100, 2000, 0, 0, NONE, NONE, "temperate_forest"},
        {"B18", 2048, 80, 630, 400, 100, 100, 2000, 0, 0, NONE, NONE, "temperate_forest"},
        {"B19", 2048, 80, 438, 30, 100, 8, 200, 0, 0, NONE, NONE, "marsh"},
        {"B20", 2048, 80, 437, 30, 100, 8, 200, 0, 0, NONE, NONE, "grassland"},
        {"B21", 1024, 80, 499, 30, 100, 8, 200, 0, 0, NONE, NONE, "grassland"},
        // Sentinels (5.1, amended 2026-11-17): inputs outside the field ranges.
        {"S1", 1024, -600, 650, 6500, 100, 100, 2000, -ONE, 0, -261993005056, NONE, "alpine"},
        {"S2", 1024, 500, 900, 400, 100, 100, 2000, 2576980378, 0, 217325345178, NONE, "temperate_forest"},
        {"S3", 1024, 80, 9583, 400, 100, 100, 2000, 0, HALF, NONE, 45274538761158, "temperate_forest"},
        {"S4", 1024, 3, 1200, 8000, 8589934591, 500, 3000, 0, 0, NONE, NONE, "alpine"},
        {"S5", 1024, -100, 1200, 32767, 200000, 500, 3000, 0, 0, NONE, NONE, "alpine"},
        {"S6", 1024, 3, 1200, 8000, 200000, 32000, 3000, 0, 0, NONE, NONE, "alpine"},
    };
    return t;
}

}  // namespace

TEST_CASE("P1-04 5.1, 10.1 test 2: the lookups B1 to B21 and S1 to S6 with the shipped definitions") {
    const Values v = values();
    const biome_api::Tuning t = tuning();
    REQUIRE(lookups().size() == 27U);
    for (const Lookup& c : lookups()) {
        CAPTURE(c.name);
        const Jittered j = own_jitter(c.temp, c.rain, c.nt, c.nr, v);
        if (c.tq != NONE) {
            CHECK(j.tq == c.tq);
        }
        if (c.rq != NONE) {
            CHECK(j.rq == c.rq);
        }
        const std::int64_t sc = biome_api::rain_scale(t, c.n);
        CHECK(sc == scale_for(v, c.n));
        const std::uint8_t want = code_of(c.biome);
        CHECK(biome_api::lookup(shipped_definitions(), sc, j.tq, j.rq, c.elev, c.s2, c.hand, c.coast) == want);
        CHECK(own_lookup(shipped_defs(), sc, Cell{j.tq, j.rq, c.elev, c.s2, c.hand, c.coast}) == want);
    }
    // The decimal values of the table and the prose after it.
    CHECK(own_jitter(80, 650, 0, HALF, v).rq == 715 * ONE);
    CHECK(fx_of("0.6") == 2576980378);
    CHECK(fx_of("-0.4") == -1717986918);
    CHECK((Fx::from_int(1) + worldgen::mul(Fx::from_raw(v.rain_jitter), Fx::from_raw(HALF))).raw() == 4724464026);
}

TEST_CASE("P1-04 4.1, B4: each bound is inclusive and each condition decides on its own") {
    const Values v = values();
    const std::int64_t sc = biome_api::rain_scale(tuning(), 1024);
    auto at = [&](std::int64_t tq, std::int64_t rq, std::int64_t elev, std::int64_t s2, std::int64_t hand,
                  std::int64_t coast) {
        const std::uint8_t got = biome_api::lookup(shipped_definitions(), sc, tq, rq, elev, s2, hand, coast);
        CHECK(got == own_lookup(shipped_defs(), sc, Cell{tq, rq, elev, s2, hand, coast}));
        return got;
    };
    // Alpine: tq <= 0.5 and elevation >= 600 m (6000 dm).
    CHECK(at(HALF, 800 * ONE, 6000, 0, 500, 1000) == ALPINE);
    CHECK(at(HALF + 1, 800 * ONE, 6000, 0, 500, 1000) == MOOR);
    CHECK(at(HALF, 800 * ONE, 5999, 0, 500, 1000) == MOOR);
    // Moor: tq <= 4, elevation >= 300 m.
    CHECK(at(4 * ONE, 600 * ONE, 3000, 0, 500, 1000) == MOOR);
    CHECK(at(4 * ONE + 1, 600 * ONE, 3000, 0, 500, 1000) == GRASSLAND);
    CHECK(at(4 * ONE, 600 * ONE, 2999, 0, 500, 1000) == GRASSLAND);
    // Marsh at 1024: rq >= 500, elevation <= 200 m, S2 <= 256, hand <= 10 dm.
    CHECK(at(8 * ONE, 500 * ONE, 2000, 256, 10, 100) == MARSH);
    CHECK(at(8 * ONE, 500 * ONE - 1, 2000, 256, 10, 100) == GRASSLAND);
    CHECK(at(8 * ONE, 500 * ONE, 2001, 256, 10, 100) == GRASSLAND);
    CHECK(at(8 * ONE, 500 * ONE, 2000, 257, 10, 100) == GRASSLAND);
    CHECK(at(8 * ONE, 500 * ONE, 2000, 256, 11, 100) == GRASSLAND);
    // Forest: rq >= 720 at 1024, one raw step below is grassland.
    CHECK(at(8 * ONE, 720 * ONE, 4000, 1000, 500, 100) == FOREST);
    CHECK(at(8 * ONE, 720 * ONE - 1, 4000, 1000, 500, 100) == GRASSLAND);
    // A cell that matches alpine and every lower rule takes alpine (priority).
    CHECK(at(0, 900 * ONE, 6000, 0, 0, 100) == ALPINE);
    // Marsh before moor and forest: a cold, wet, flat lowland cell.
    CHECK(at(2 * ONE, 900 * ONE, 100, 0, 0, 100) == MARSH);
    static_cast<void>(v);
}

// --- Size rule (10.1 test 3) ---------------------------------------------------------------------

TEST_CASE("P1-04 4.2, B4, 10.1 test 3: the rain scale by map size; 10000 is never scaled") {
    const biome_api::Tuning t = tuning();
    for (const std::int64_t n : {1, 16, 128, 512}) {
        CAPTURE(n);
        CHECK(biome_api::rain_scale(t, n) == 4831838208);
    }
    for (const std::int64_t n : {513, 1024}) {
        CAPTURE(n);
        CHECK(biome_api::rain_scale(t, n) == ONE);
    }
    for (const std::int64_t n : {1025, 2048, 4096}) {
        CAPTURE(n);
        CHECK(biome_api::rain_scale(t, n) == 3758096384);
    }
    // Distinct values in each entry show which one applies.
    const biome_api::Tuning d =
        tuning({{"rain_scale_512", "0.5"}, {"rain_scale_1024", "0.75"}, {"rain_scale_2048", "1.5"}});
    CHECK(biome_api::rain_scale(d, 512) == HALF);
    CHECK(biome_api::rain_scale(d, 513) == 3 * ONE / 4);
    CHECK(biome_api::rain_scale(d, 1025) == 3 * ONE / 2);
    // rq = 9999 at scale 0.875: the forest rule (rain_max_mm 10000) matches.
    const std::int64_t s875 = biome_api::rain_scale(t, 2048);
    CHECK(biome_api::lookup(shipped_definitions(), s875, 8 * ONE, 9999 * ONE, 4000, 1000, 500, 100) == FOREST);
    CHECK(biome_api::lookup(shipped_definitions(), s875, 8 * ONE, 10000 * ONE, 4000, 1000, 500, 100) == FOREST);
    // rain_max_mm 10000 is the sentinel: rq 15000 matches at any scale.
    for (const std::int64_t n : {512, 1024, 2048}) {
        CHECK(biome_api::lookup(shipped_definitions(), biome_api::rain_scale(t, n), 8 * ONE, 15000 * ONE, 4000, 1000,
                                500, 100) == FOREST);
    }
    // A W x H grid uses N = max(W, H): a 600 x 100 grid takes the 1024
    // entry. Checked through biome_layer on such a grid below
    // ("rain scale of a 600 x 100 grid").
    CHECK(scale_for(values(), std::max(600, 100)) == ONE);
    // A bound other than 10000 is scaled: rain_max_mm 9999 becomes 8749.125.
    std::string forest(biome_data::shipped_files()[2].text);
    const std::size_t at = forest.find("rain_max_mm = 10000");
    REQUIRE(at != std::string::npos);
    forest.replace(at, 19, "rain_max_mm = 9999");
    const biome_api::Load l = biome_api::load(std::string(biome_data::TUNING_TOML),
                                              biome_data::directory({{"temperate_forest.toml", forest}}));
    REQUIRE_MESSAGE(l.ok(), all_errors(l));
    const biome_api::Definitions defs = biome_api::definitions_from(*l.content);
    CHECK(biome_api::lookup(defs, s875, 8 * ONE, 8749 * ONE, 4000, 1000, 500, 100) == FOREST);
    CHECK(biome_api::lookup(defs, s875, 8 * ONE, 8750 * ONE, 4000, 1000, 500, 100) == GRASSLAND);
    CHECK(biome_api::lookup(defs, ONE, 8 * ONE, 9999 * ONE, 4000, 1000, 500, 100) == FOREST);
    // The scaled bounds of the shipped rules are exact (binary fractions).
    CHECK(rb(4831838208, 720) == 810 * ONE);
    CHECK(rb(3758096384, 720) == 630 * ONE);
    CHECK(rb(4831838208, 500) == fx_of("562.5"));
    CHECK(rb(3758096384, 500) == fx_of("437.5"));
}

// --- B5 (10.1 test 4) -------------------------------------------------------------------------

namespace {

// A 3 x 3 grid with a river cell at the centre (B4 code 9, a value no
// definition has, so a kept B4 code is visible) and the given neighbours in
// D8 order 0..7 (E, SE, S, SW, W, NW, N, NE).
std::uint8_t centre_after_b5(const std::array<std::uint8_t, 8>& codes, const std::array<std::uint8_t, 8>& waters) {
    const Grid g{3, 3};
    std::vector<std::uint8_t> b4(9, 0), water(9, 0);
    b4[4] = 9;
    water[4] = WATER_RIVER;
    for (std::size_t k = 0; k < 8; ++k) {
        const std::size_t j = g.index(1 + DX[k], 1 + DY[k]);
        b4[j] = codes[k];
        water[j] = waters[k];
    }
    const std::vector<std::uint8_t> got = biome_api::river_cells(g, water, b4);
    CHECK(got == own_river_cells(g, water, b4));
    // B5 changes river cells only.
    for (std::size_t j = 0; j < 9; ++j) {
        if (j != 4) {
            CHECK(got[j] == b4[j]);
        }
    }
    return got[4];
}

}  // namespace

TEST_CASE("P1-04 5.1 B5, 10.1 test 4: a river cell takes the most common biome of its dry neighbours") {
    // 3, 3, 4, 4, 5 -> 3 (tie of 3 and 4 to the smaller code).
    CHECK(centre_after_b5({3, 3, 4, 4, 5, 2, 2, 2}, {0, 0, 0, 0, 0, 2, 2, 2}) == 3);
    // The same in another order of directions.
    CHECK(centre_after_b5({4, 5, 3, 2, 4, 2, 3, 2}, {0, 0, 0, 2, 0, 2, 0, 2}) == 3);
    // 4, 4, 4, 3 -> 4.
    CHECK(centre_after_b5({4, 4, 4, 3, 2, 2, 2, 2}, {0, 0, 0, 0, 2, 2, 2, 2}) == 4);
    // No dry neighbour (a lake and other river cells): keeps its B4 code.
    CHECK(centre_after_b5({2, 2, 2, 2, 5, 5, 5, 5}, {2, 2, 2, 2, 1, 1, 1, 1}) == 9);
    // River and ocean neighbours do not count, whatever their B4 code.
    CHECK(centre_after_b5({5, 5, 5, 5, 1, 1, 4, 3}, {1, 1, 1, 1, 3, 3, 0, 0}) == 3);
    // The real cell (571, 380) of 5.1: dry neighbours 4, 4, 4, 4, 4, 3, 3.
    CHECK(centre_after_b5({4, 4, 4, 4, 4, 3, 3, 1}, {0, 0, 0, 0, 0, 0, 0, 1}) == 4);
}

TEST_CASE("P1-04 B5: at the grid edge only existing neighbours count; river cells read B4 values of dry cells only") {
    // A 4 x 2 strip: river cells at (0, 0) and (1, 0) beside each other.
    const Grid g{4, 2};
    const std::vector<std::uint8_t> water = {1, 1, 0, 0, 0, 0, 0, 0};
    const std::vector<std::uint8_t> b4 = {7, 7, 6, 6, 3, 5, 5, 6};
    // (0, 0): dry neighbours (0, 1) 3, (1, 1) 5 -> tie, 3. (1, 0): (2, 0) 6,
    // (0, 1) 3, (1, 1) 5, (2, 1) 5 -> 5.
    const std::vector<std::uint8_t> want = {3, 5, 6, 6, 3, 5, 5, 6};
    CHECK(biome_api::river_cells(g, water, b4) == want);
    CHECK(own_river_cells(g, water, b4) == want);
}

// --- Two rules (10.1 test 5) -------------------------------------------------------------------

TEST_CASE("P1-04 10.1 test 5: a definition with two rules applies where either matches") {
    std::string moor(biome_data::shipped_files()[5].text);
    moor +=
        "\n[[biome.highland_moor.rules]]\ntemp_min_c = 20\ntemp_max_c = 50\nrain_min_mm = 0\nrain_max_mm = 10000\n"
        "elev_min_m = 0\nelev_max_m = 100\nslope_max = 100\nhand_max_m = 3000\ncoast_max_m = 32767\n";
    const biome_api::Load l =
        biome_api::load(std::string(biome_data::TUNING_TOML), biome_data::directory({{"highland_moor.toml", moor}}));
    REQUIRE_MESSAGE(l.ok(), all_errors(l));
    const biome_api::Definitions d = biome_api::definitions_from(*l.content);
    // Only the second rule matches (25 C, 50 m): moor, not forest or grassland.
    CHECK(biome_api::lookup(d, ONE, 25 * ONE, 600 * ONE, 500, 0, 100, 100) == MOOR);
    CHECK(biome_api::lookup(d, ONE, 25 * ONE, 900 * ONE, 500, 0, 100, 100) == MOOR);
    // Only the first (2 C, 400 m).
    CHECK(biome_api::lookup(d, ONE, 2 * ONE, 600 * ONE, 4000, 0, 100, 100) == MOOR);
    // Neither (15 C, 50 m): grassland, and forest with rain.
    CHECK(biome_api::lookup(d, ONE, 15 * ONE, 600 * ONE, 500, 0, 100, 100) == GRASSLAND);
    CHECK(biome_api::lookup(d, ONE, 15 * ONE, 900 * ONE, 500, 0, 100, 100) == FOREST);
}

// --- Real cells (5.1) --------------------------------------------------------------------------

TEST_CASE("P1-04 5.1: the real cells of 1024 seed 3 (inputs as tabulated)") {
    struct Real {
        std::int64_t temp, rain, elev, s2, hand, coast, nt, nr, tq, rq;
        std::uint8_t biome;
    };
    const std::vector<Real> cells = {
        {58, 716, 1401, 9250, 262, 1936, 23557280, 120289931, 24934367597, 3092422101912, FOREST},
        {68, 665, 1297, 17, 718, 2819, -773053270, 126360917, 28432724343, 2872959253535, GRASSLAND},
        {64, 662, 1222, 20, 0, 4470, 371464994, -710571975, 27859255688, 2749188620462, MARSH},
        {25, 590, 8659, 139634, 3695, 1866, -1090468039, -442498318, 9646950201, 2481815902880, MOOR},
        {-13, 748, 12129, 174941, 9549, 992, 98162261, 904479069, -5485295224, 3347945606280, ALPINE},
        {59, 725, 1140, 1, 67, 2352, 1293082526, -319437946, 26633389572, 3067532787575, GRASSLAND},
    };
    const Values v = values();
    const std::int64_t sc = biome_api::rain_scale(tuning(), 1024);
    for (const Real& c : cells) {
        CAPTURE(c.temp);
        CAPTURE(c.rain);
        const Jittered j = own_jitter(c.temp, c.rain, c.nt, c.nr, v);
        CHECK(j.tq == c.tq);
        CHECK(j.rq == c.rq);
        CHECK(biome_api::lookup(shipped_definitions(), sc, c.tq, c.rq, c.elev, c.s2, c.hand, c.coast) == c.biome);
    }
    // nt and nr of (375, 469) are the jitter fields of seed 3 (B3).
    const std::uint64_t ktj = worldgen::stream_key(3, STREAM_TJ);
    const std::uint64_t krj = worldgen::stream_key(3, STREAM_RJ);
    const Fx X = Fx::from_int(16 * 375 + 8), Y = Fx::from_int(16 * 469 + 8);
    CHECK(worldgen::geo::fbm_m(ktj, X, Y, v.jitter).raw() == 23557280);
    CHECK(worldgen::geo::fbm_m(krj, X, Y, v.jitter).raw() == 120289931);
}

// --- B3 structural (10.1 test 8) -------------------------------------------------------------

TEST_CASE("P1-04 B3, 10.1 test 8: the jitter fields are fbm2 of the stream keys at the cell centres (seed 0, N = 64)") {
    const Values v = values();
    const std::uint64_t ktj = worldgen::stream_key(0, STREAM_TJ);
    const std::uint64_t krj = worldgen::stream_key(0, STREAM_RJ);
    const Fx base = Fx::from_ratio(1, 500);
    const biome_api::Inputs in = world_inputs(0, 64, false);
    const std::vector<std::int64_t> nt = noise_grid(0, STREAM_TJ, in.grid, in.water, v.jitter);
    const std::vector<std::int64_t> nr = noise_grid(0, STREAM_RJ, in.grid, in.water, v.jitter);
    const Own own = own_biomes(in, shipped_defs(), v, 0);
    const std::vector<std::uint8_t> stage = biome_api::layer(in, shipped_definitions(), tuning(), 0, THREADS);
    int land = 0;
    for (std::int32_t k = 0; k < 16; ++k) {
        const std::int32_t x = (k * 37 + 5) % 64, y = (k * 23 + 11) % 64;
        CAPTURE(x);
        CAPTURE(y);
        const std::size_t i = in.grid.index(x, y);
        if (in.water[i] > WATER_RIVER) {
            continue;
        }
        ++land;
        const Fx X = Fx::from_int(16 * x + 8), Y = Fx::from_int(16 * y + 8);
        const Fx a = worldgen::fbm2(ktj, X, Y, 4, base, Fx::from_int(2), Fx::from_raw(HALF));
        const Fx b = worldgen::fbm2(krj, X, Y, 4, base, Fx::from_int(2), Fx::from_raw(HALF));
        CHECK(a.raw() == nt[i]);
        CHECK(b.raw() == nr[i]);
        const Jittered j = own_jitter(in.temperature[i], in.rainfall[i], a.raw(), b.raw(), v);
        CHECK(j.tq == own.tq[i]);
        CHECK(j.rq == own.rq[i]);
        if (in.water[i] == WATER_DRY) {
            CHECK(stage[i] == own_lookup(shipped_defs(), own.scale,
                                         Cell{j.tq, j.rq, in.elevation[i], own.s2[i], own.hand[i], in.coast_dist[i]}));
        }
    }
    CHECK(land >= 4);
}

// --- Scale entry of a W x H grid (B4, amended 2026-11-17) ---------------------------------------

TEST_CASE("P1-04 B4, 10.1 test 3: rain scale of a 600 x 100 grid is the 1024 entry (N = max(W, H))") {
    biome_api::Inputs in;
    in.grid = Grid{600, 100};
    for (std::int32_t y = 0; y < 100; ++y) {
        for (std::int32_t x = 0; x < 600; ++x) {
            const bool sea = x < 2 || y < 2 || x >= 598 || y >= 98;
            in.elevation.push_back(static_cast<std::int16_t>(sea ? -30 : 1 + x / 8));
            in.water.push_back(sea ? WATER_OCEAN : WATER_DRY);
            in.water_depth.push_back(static_cast<std::uint16_t>(sea ? 30 : 0));
            in.flow_dir.push_back(static_cast<std::uint8_t>(sea ? 8 : 4));
            in.river.push_back(0);
            in.temperature.push_back(80);
            in.rainfall.push_back(static_cast<std::uint16_t>(400 + 6 * y));
        }
    }
    std::vector<std::uint8_t> ocean(in.grid.cells());
    for (std::size_t i = 0; i < ocean.size(); ++i) {
        ocean[i] = in.water[i] == WATER_OCEAN ? 1 : 0;
    }
    in.coast_dist = hydro_api::coast_distance(in.grid, ocean);
    const auto at = [&](const Overrides& o) { return biome_api::layer(in, shipped_definitions(), tuning(o), 5, 2); };
    const Overrides all_one = {{"rain_scale_512", "1"}, {"rain_scale_1024", "1"}, {"rain_scale_2048", "1"}};
    const std::vector<std::uint8_t> ones = at(all_one);
    CHECK(at({{"rain_scale_512", "0.5"}, {"rain_scale_2048", "2"}}) == ones);
    CHECK(at({{"rain_scale_1024", "0.5"}}) != ones);
    CHECK(ones == own_biomes(in, shipped_defs(), values(all_one), 5).biome);
}
