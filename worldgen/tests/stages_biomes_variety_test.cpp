// The biome variety revision (spec P1-04 third version, generator 0.9.0):
// B2b river distance, B2c wet share, the per-size wet scale, the two new
// rule comparisons and their sentinels, the worked examples of 5.1 (V, W, H
// and P lookups with their sole failures, the second version's cases and
// their variants, exactness at 0.45 and 0.675) and the entry guard of the
// B2c seam (10.1 tests 2 to 5, 8; section 7).
//
// Written from the spec only. Every stage call goes through
// p1_04_biomes_api.hpp; the test's own B2b, B2c and B4 are in
// p1_04_biomes_support.hpp. The same numbers are reproduced by
// reference/p1_biomes_reference.py check.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "p1_04_biomes_api.hpp"
#include "p1_04_biomes_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"

using namespace biome_test;

namespace {

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
    FAIL("no biome " << std::string(key));
    return 0;
}

const Def& def_of(std::string_view key) {
    for (const Def& d : shipped_defs()) {
        if (d.key == key) {
            return d;
        }
    }
    FAIL("no biome " << std::string(key));
    return shipped_defs().front();
}

std::uint32_t bit(std::string_view cond) {
    for (std::size_t k = 0; k < CONDITION_NAMES.size(); ++k) {
        if (CONDITION_NAMES[k] == cond) {
            return 1U << k;
        }
    }
    FAIL("no condition " << std::string(cond));
    return 0;
}

// --- B2b example of 5.1 ---------------------------------------------------------------------------

struct B2bGrid {
    Grid g{8, 5};
    std::vector<std::uint8_t> water, river;
};

B2bGrid b2b_example() {
    B2bGrid b;
    b.water.assign(40, WATER_DRY);
    b.river.assign(40, 0);
    for (std::int32_t y = 0; y < 5; ++y) {
        b.water[b.g.index(7, y)] = WATER_OCEAN;
    }
    const std::array<std::array<std::int32_t, 3>, 4> rivers = {{{0, 1, 2}, {1, 1, 2}, {2, 2, 4}, {3, 3, 5}}};
    for (const auto& r : rivers) {
        b.water[b.g.index(r[0], r[1])] = WATER_RIVER;
        b.river[b.g.index(r[0], r[1])] = static_cast<std::uint8_t>(r[2]);
    }
    return b;
}

constexpr std::int64_t F = FAR;

}  // namespace

TEST_CASE("P1-04 5.1 B2b, 10.1 test 4: the 8 x 5 example after both passes; k_r 6 gives FAR; 1 x 1") {
    const B2bGrid b = b2b_example();
    const std::vector<std::int64_t> want = {
        8, 7, 6, 7, 8, 11, 12, 15,  //
        7, 4, 3, 4, 7, 8,  11, 14,  //
        6, 3, 0, 3, 4, 7,  10, 13,  //
        7, 4, 3, 0, 3, 6,  9,  12,  //
        8, 7, 4, 3, 4, 7,  10, 13,
    };
    CHECK(biome_api::river_distance(b.g, b.water, b.river, 4) == want);
    CHECK(own_d34(b.g, b.water, b.river, 4) == want);
    CHECK(closed_d34(b.g, b.water, b.river, 4) == want);
    // By hand: (0, 0) two diagonal steps from (2, 2); (7, 0) from (3, 3): 3 x 4 + 3.
    CHECK(want[0] == 8);
    CHECK(want[7] == 15);
    CHECK(biome_api::river_distance(b.g, b.water, b.river, 6) == std::vector<std::int64_t>(40, F));
    CHECK(biome_api::river_distance(b.g, b.water, b.river, 5) == closed_d34(b.g, b.water, b.river, 5));
    const std::vector<std::uint8_t> w1 = {WATER_RIVER}, r1 = {4}, w0 = {WATER_DRY}, r0 = {0};
    CHECK(biome_api::river_distance(Grid{1, 1}, w1, r1, 4) == std::vector<std::int64_t>{0});
    CHECK(biome_api::river_distance(Grid{1, 1}, w0, r0, 4) == std::vector<std::int64_t>{F});
}

TEST_CASE("P1-04 B2b, 10.1 test 4: fatal for k outside 1..15 and arrays of the wrong length") {
    const B2bGrid b = b2b_example();
    for (const std::int64_t k : {std::int64_t{0}, std::int64_t{16}}) {
        CAPTURE(k);
        CHECK_THROWS_AS(static_cast<void>(biome_api::river_distance(b.g, b.water, b.river, k)), worldgen::FatalError);
    }
    CHECK_NOTHROW(static_cast<void>(biome_api::river_distance(b.g, b.water, b.river, 1)));
    CHECK_NOTHROW(static_cast<void>(biome_api::river_distance(b.g, b.water, b.river, 15)));
    const std::vector<std::uint8_t> short_water(b.water.begin(), b.water.end() - 1);
    CHECK_THROWS_AS(static_cast<void>(biome_api::river_distance(b.g, short_water, b.river, 4)), worldgen::FatalError);
}

TEST_CASE("P1-04 6 invariant 8, 10.1 test 4: on 200 random grids up to 12 x 12, d34 is the closed form") {
    for (std::uint64_t f = 0; f < 200; ++f) {
        CAPTURE(f);
        worldgen::Rng rng(worldgen::substream(worldgen::stream_key(0, "test.biomes.b2b"), f));
        const auto w = static_cast<std::int32_t>(1 + rng.uniform_i64(0, 11));
        const auto h = static_cast<std::int32_t>(1 + rng.uniform_i64(0, 11));
        const Grid g{w, h};
        std::vector<std::uint8_t> water, river;
        for (std::size_t i = 0; i < g.cells(); ++i) {
            const std::array<std::uint8_t, 3> kinds = {WATER_DRY, WATER_RIVER, WATER_OCEAN};
            water.push_back(kinds[static_cast<std::size_t>(rng.uniform_i64(0, 2))]);
            river.push_back(static_cast<std::uint8_t>(water.back() == WATER_RIVER ? rng.uniform_i64(1, 15) : 0));
        }
        const std::int64_t k = 1 + rng.uniform_i64(0, 14);
        REQUIRE(biome_api::river_distance(g, water, river, k) == closed_d34(g, water, river, k));
    }
}

TEST_CASE("P1-04 5.1 B2c, 10.1 test 5: the 6 x 4 example exactly; r 2 and 3 agree; the 0.45 cells") {
    const Grid g{6, 4};
    std::vector<std::uint8_t> water(24, WATER_DRY);
    for (std::int32_t y = 0; y < 4; ++y) {
        water[g.index(0, y)] = WATER_OCEAN;
    }
    water[g.index(1, 1)] = WATER_RIVER;
    water[g.index(2, 2)] = WATER_LAKE;
    const std::vector<std::int64_t> hand = {0, 10, 40, 80, 90, 120, 0, 0,  20, 51, 60,  70,
                                            0, 5,  0,  30, 200, 300, 0, 30, 60, 10, 250, 400};
    const std::vector<std::int64_t> wet = {0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0};
    CHECK(own_wet(water, hand, 5 * ONE) == wet);
    const std::vector<std::int64_t> count = {15, 26, 27, 18, 7,  1, 23, 41, 43, 31, 13, 3,
                                             22, 41, 44, 35, 16, 5, 13, 25, 27, 23, 11, 4};
    const std::vector<std::int64_t> total = {25, 40, 45, 45, 40, 25, 40, 64, 72, 72, 64, 40,
                                             40, 64, 72, 72, 64, 40, 25, 40, 45, 45, 40, 25};
    for (const std::int64_t r : {std::int64_t{2}, std::int64_t{3}}) {
        CAPTURE(r);
        const biome_api::WetShare s = biome_api::wet_share(g, water, hand, 5 * ONE, r);
        CHECK(s.count == count);
        CHECK(s.total == total);
        const WetShare o = own_wet_share(g, water, hand, 5 * ONE, r);
        CHECK(o.count == count);
        CHECK(o.total == total);
    }
    // wet_window_m 32 and 47 give r = 2 (div 16).
    CHECK(32 / 16 == 2);
    CHECK(47 / 16 == 2);
    // The cells passing the shipped 0.45 at 512 and 1024 (need raw 1932735283).
    const std::vector<int> pass = {1, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0};
    for (std::size_t i = 0; i < 24; ++i) {
        CAPTURE(i);
        CHECK((need_of(1932735283, ONE) * total[i] <= count[i] * ONE) == (pass[i] == 1));
    }
    CHECK(1932735283LL * 72 == 139156940376LL);
    CHECK(35 * ONE == 150323855360LL);
}

TEST_CASE("P1-04 B2c, 10.1 test 5: r 0 and 1 give count = wet, total = 1; a window as large as the grid") {
    const Grid g{6, 4};
    std::vector<std::uint8_t> water(24, WATER_DRY);
    water[0] = WATER_OCEAN;
    std::vector<std::int64_t> hand(24);
    for (std::size_t i = 0; i < 24; ++i) {
        hand[i] = static_cast<std::int64_t>(i * 7 % 90);
    }
    const std::vector<std::int64_t> wet = own_wet(water, hand, 5 * ONE);
    for (const std::int64_t r : {std::int64_t{0}, std::int64_t{1}}) {
        const biome_api::WetShare s = biome_api::wet_share(g, water, hand, 5 * ONE, r);
        CHECK(s.count == wet);
        CHECK(s.total == std::vector<std::int64_t>(24, 1));
    }
    std::int64_t nwet = 0;
    for (const std::int64_t v : wet) {
        nwet += v;
    }
    // h >= max(W, H) - 1 = 5: every window of both passes is the whole grid.
    for (const std::int64_t r : {std::int64_t{10}, std::int64_t{11}, std::int64_t{125}}) {
        CAPTURE(r);
        const biome_api::WetShare s = biome_api::wet_share(g, water, hand, 5 * ONE, r);
        CHECK(s.total == std::vector<std::int64_t>(24, 24 * 24));
        CHECK(s.count == std::vector<std::int64_t>(24, nwet * 24));
    }
}

TEST_CASE("P1-04 7, 10.1 test 5: the B2c entry guard, in order, before any allocation; r 125 at 2048 is exact") {
    const Grid g{6, 4};
    const std::vector<std::uint8_t> water(24, WATER_DRY);
    const std::vector<std::int64_t> hand(24, 0);
    for (const std::int64_t r : {std::int64_t{-1}, std::int64_t{126}}) {
        CAPTURE(r);
        CHECK(starts(fatal_message([&] { (void)biome_api::wet_share(g, water, hand, 5 * ONE, r); }), PRECONDITION));
    }
    // W x H = 2^31 + 65536 with empty arrays: the size guard comes before the
    // length check and before any allocation.
    const std::vector<std::uint8_t> no_water;
    const std::vector<std::int64_t> no_hand;
    CHECK(65536LL * 32769LL == (std::int64_t{1} << 31) + 65536);
    CHECK(starts(fatal_message([&] { (void)biome_api::wet_share(Grid{65536, 32769}, no_water, no_hand, 5 * ONE, 125); }),
                 PRECONDITION));
    // r out of range is reported before the grid size (order of section 7).
    const std::string m = fatal_message([&] { (void)biome_api::wet_share(Grid{65536, 32769}, no_water, no_hand, 5 * ONE, 126); });
    CHECK(starts(m, PRECONDITION));
    // Arrays of the wrong length.
    const std::vector<std::int64_t> short_hand(23, 0);
    CHECK(starts(fatal_message([&] { (void)biome_api::wet_share(g, water, short_hand, 5 * ONE, 2); }), PRECONDITION));
    // The stage's extreme: 2048 x 2048, every cell wet, r_cells 125 (h = 62).
    const Grid big{2048, 2048};
    const std::vector<std::uint8_t> bw(big.cells(), WATER_DRY);
    const std::vector<std::int64_t> bh(big.cells(), 0);
    const biome_api::WetShare s = biome_api::wet_share(big, bw, bh, 5 * ONE, 125);
    CHECK(s.total[big.index(1024, 1024)] == 244140625);  // 125^4
    CHECK(s.count == s.total);
    // Corner: each pass at the corner sums first-pass windows clipped to 63 x 63.
    std::int64_t corner = 0;
    for (std::int64_t x = 0; x <= 62; ++x) {
        corner += std::min<std::int64_t>(2048, x + 63) - std::max<std::int64_t>(0, x - 62);
    }
    CHECK(s.total[0] == corner * corner);
}

TEST_CASE("P1-04 5.1, 10.1 test 2: the lookups V1 to P6 through biome_lookup, each named sole failure exact") {
    struct Case {
        const char* name;
        std::int64_t n, temp, rain, elev, s2, hand, coast, d34, count, total;
        const char* biome;
        std::vector<std::pair<const char*, const char*>> soles;
    };
    const std::vector<Case> cases = {
        {"V1", 1024, 80, 900, 30, 100, 20, 200, F, 9, 10, "fen", {}},
        {"V2", 1024, 80, 900, 30, 100, 20, 200, F, 8, 20, "temperate_forest", {{"fen", "wet"}}},
        {"V3", 1024, 80, 900, 30, 100, 10, 200, F, 8, 20, "marsh", {{"fen", "wet"}}},
        {"V4", 1024, 80, 900, 30, 100, 10, 200, F, 9, 20, "fen", {}},
        {"V5", 1024, 80, 900, 30, 100, 31, 200, F, 20, 20, "temperate_forest", {{"fen", "hand"}}},
        {"V6", 1024, 80, 900, 30, 257, 20, 200, F, 20, 20, "temperate_forest", {{"fen", "slope"}}},
        {"V7", 1024, 80, 499, 30, 100, 20, 200, F, 20, 20, "steppe", {{"fen", "rain"}, {"wet_woodland", "rdist"}}},
        {"V8", 1024, 80, 900, 2001, 100, 20, 200, F, 20, 20, "temperate_forest", {{"fen", "elev"}}},
        {"V9", 2048, 80, 900, 30, 100, 20, 200, F, 26, 40, "temperate_forest", {{"fen", "wet"}}},
        {"V10", 2048, 80, 900, 30, 100, 20, 200, F, 27, 40, "temperate_forest", {{"fen", "wet"}}},
        {"V11", 2048, 80, 900, 30, 100, 20, 200, F, 28, 40, "fen", {}},
        {"V12", 512, 80, 900, 30, 100, 20, 200, F, 9, 20, "fen", {}},
        {"W1", 1024, 80, 650, 1000, 100, 40, 2000, 21, 0, 100, "wet_woodland", {}},
        {"W2", 1024, 80, 650, 1000, 100, 40, 2000, 22, 0, 100, "grassland", {{"wet_woodland", "rdist"}, {"steppe", "rain"}}},
        {"W3", 1024, 80, 449, 1000, 100, 40, 2000, 21, 0, 100, "steppe", {{"wet_woodland", "rain"}}},
        {"W4", 1024, 80, 650, 3001, 100, 40, 2000, 21, 0, 100, "grassland", {{"wet_woodland", "elev"}, {"heath", "temp"}}},
        {"W5", 1024, 80, 650, 1000, 100, 51, 2000, 21, 0, 100, "grassland", {{"wet_woodland", "hand"}}},
        {"W6", 1024, 80, 600, 1000, 100, 40, 2000, 22, 0, 100, "steppe", {}},
        {"W7", 1024, 80, 650, 1000, 100, 40, 2000, F, 0, 100, "grassland", {{"wet_woodland", "rdist"}}},
        {"W8", 2048, 80, 394, 1000, 100, 40, 2000, 21, 0, 100, "wet_woodland", {}},
        {"W9", 2048, 80, 393, 1000, 100, 40, 2000, 21, 0, 100, "steppe", {{"wet_woodland", "rain"}}},
        {"H1", 1024, 30, 720, 5000, 10000, 500, 3000, F, 0, 100, "highland_moor", {}},
        {"H2", 1024, 30, 719, 5000, 10000, 500, 3000, F, 0, 100, "heath", {{"highland_moor", "rain"}}},
        {"H3", 512, 30, 809, 5000, 10000, 500, 3000, F, 0, 100, "heath", {{"highland_moor", "rain"}}},
        {"H4", 512, 30, 810, 5000, 10000, 500, 3000, F, 0, 100, "highland_moor", {}},
        {"H5", 1024, 41, 650, 5000, 10000, 500, 3000, F, 0, 100, "grassland", {{"heath", "temp"}}},
        {"H6", 1024, 41, 590, 5000, 10000, 500, 3000, F, 0, 100, "steppe", {{"heath", "temp"}}},
        {"H7", 1024, 40, 300, 2999, 10000, 500, 3000, F, 0, 100, "steppe", {{"heath", "elev"}}},
        {"P1", 1024, 80, 600, 400, 100, 100, 2000, F, 0, 100, "steppe", {}},
        {"P2", 1024, 80, 601, 400, 100, 100, 2000, F, 0, 100, "grassland", {{"steppe", "rain"}}},
        {"P3", 512, 80, 675, 400, 100, 100, 2000, F, 0, 100, "steppe", {}},
        {"P4", 512, 80, 676, 400, 100, 100, 2000, F, 0, 100, "grassland", {{"steppe", "rain"}}},
        {"P5", 2048, 80, 525, 400, 100, 100, 2000, F, 0, 100, "steppe", {}},
        {"P6", 2048, 80, 526, 400, 100, 100, 2000, F, 0, 100, "grassland", {{"steppe", "rain"}}},
    };
    const Values v = values();
    const biome_api::Tuning t = tuning();
    for (const Case& c : cases) {
        CAPTURE(std::string_view(c.name));
        const Jittered j = own_jitter(c.temp, c.rain, 0, 0, v);
        const std::int64_t sc = biome_api::rain_scale(t, c.n);
        const std::int64_t ws = biome_api::wet_scale(t, c.n);
        CHECK(sc == scale_for(v, c.n));
        CHECK(ws == wet_scale_for(v, c.n));
        CHECK(biome_api::lookup(shipped_definitions(), sc, j.tq, j.rq, c.elev, c.s2, c.hand, c.coast, c.d34, c.count,
                                c.total, ws) == code_of(c.biome));
        const Cell cell{j.tq, j.rq, c.elev, c.s2, c.hand, c.coast, c.d34, c.count, c.total};
        CHECK(own_lookup(shipped_defs(), sc, cell, nullptr, ws) == code_of(c.biome));
        for (const auto& [key, cond] : c.soles) {
            CAPTURE(std::string_view(key));
            CHECK(failures(def_of(key).rules.front(), sc, cell, ws) == bit(cond));
            // Through the seam: the rule alone does not match.
            const biome_api::Rule r = biome_api::rule_of(shipped_definitions(), key);
            CHECK_FALSE(biome_api::rule_matches(r, sc, j.tq, j.rq, c.elev, c.s2, c.hand, c.coast, c.d34, c.count,
                                                c.total, ws));
        }
    }
}

TEST_CASE("P1-04 5.1, 10.1 test 2: the second version's B1 to B21 with count / total 1 / 1 and with d34 0") {
    // Inputs of the second version's table (nt = nr = 0 where these are used).
    struct B {
        const char* name;
        std::int64_t n, temp, rain, elev, s2, hand, coast;
        const char* plain;   // d34 FAR, 0 / 1 (third version's result)
        const char* wet1;    // count / total 1 / 1
        const char* d34_0;   // d34 0
    };
    const std::vector<B> cases = {
        {"B3", 1024, 80, 900, 30, 100, 8, 200, "marsh", "fen", "marsh"},
        {"B5", 1024, 80, 900, 30, 256, 10, 200, "marsh", "fen", "marsh"},
        {"B6", 1024, 80, 900, 30, 100, 11, 200, "temperate_forest", "fen", "temperate_forest"},
        {"B19", 2048, 80, 438, 30, 100, 8, 200, "marsh", "fen", "marsh"},
        {"B20", 2048, 80, 437, 30, 100, 8, 200, "steppe", "steppe", "wet_woodland"},
        {"B21", 1024, 80, 499, 30, 100, 8, 200, "steppe", "steppe", "wet_woodland"},
        {"B10", 1024, 40, 650, 3000, 100, 100, 2000, "heath", "heath", "heath"},
        {"B8", 1024, 80, 719, 400, 100, 100, 2000, "grassland", "grassland", "grassland"},
        {"B4", 1024, 80, 900, 30, 257, 8, 200, "temperate_forest", "temperate_forest", "temperate_forest"},
    };
    const Values v = values();
    const biome_api::Tuning t = tuning();
    for (const B& c : cases) {
        CAPTURE(std::string_view(c.name));
        const Jittered j = own_jitter(c.temp, c.rain, 0, 0, v);
        const std::int64_t sc = biome_api::rain_scale(t, c.n);
        const std::int64_t ws = biome_api::wet_scale(t, c.n);
        auto at = [&](std::int64_t d34, std::int64_t cnt, std::int64_t tot) {
            return biome_api::lookup(shipped_definitions(), sc, j.tq, j.rq, c.elev, c.s2, c.hand, c.coast, d34, cnt,
                                     tot, ws);
        };
        CHECK(at(F, 0, 1) == code_of(c.plain));
        CHECK(at(F, 1, 1) == code_of(c.wet1));
        CHECK(at(0, 0, 1) == code_of(c.d34_0));
    }
}

TEST_CASE("P1-04 5.1: exactness of the share comparison at 0.45 (1024) and 0.675 (2048)") {
    const biome_api::Tuning t = tuning();
    const std::int64_t w1024 = biome_api::wet_scale(t, 1024), w2048 = biome_api::wet_scale(t, 2048);
    CHECK(w2048 == 6442450944);
    CHECK(need_of(1932735283, w1024) == 1932735283);
    CHECK(need_of(1932735283, w2048) == 2899102925);  // RHA(2899102924.5)
    CHECK(1932735283LL * 20 == 38654705660LL);
    CHECK(9 * ONE == 38654705664LL);
    CHECK(2899102925LL * 40 == 115964117000LL);
    CHECK(27 * ONE == 115964116992LL);
    const biome_api::Rule fen = biome_api::rule_of(shipped_definitions(), "fen");
    // Every other condition of fen holds (V-row inputs).
    const Jittered j = own_jitter(80, 900, 0, 0, values());
    auto fen_ok = [&](std::int64_t n, std::int64_t cnt, std::int64_t tot) {
        return biome_api::rule_matches(fen, biome_api::rain_scale(t, n), j.tq, j.rq, 30, 100, 20, 200, F, cnt, tot,
                                       biome_api::wet_scale(t, n));
    };
    CHECK(fen_ok(1024, 9, 20));    // exactly 0.45 passes
    CHECK_FALSE(fen_ok(1024, 8, 20));
    CHECK_FALSE(fen_ok(2048, 27, 40));  // exactly 0.675 fails
    CHECK(fen_ok(2048, 28, 40));
    CHECK(fen_ok(512, 9, 20));
    CHECK(fen_ok(1024, 923521, 923521));
}

TEST_CASE("P1-04 4.1, 10.1 test 3: the sentinels of the two new fields and one step inside them") {
    const biome_api::Rule full = biome_api::rule_of(shipped_definitions(), "temperate_forest");
    const Jittered j = own_jitter(80, 900, 0, 0, values());
    auto m = [&](const biome_api::Rule& r, std::int64_t d34, std::int64_t cnt, std::int64_t tot, std::int64_t ws) {
        return biome_api::rule_matches(r, ONE, j.tq, j.rq, 400, 100, 100, 2000, d34, cnt, tot, ws);
    };
    // river_dist_max_m 32767 matches d34 = 2^40; 32766 fails it and matches 6143, not 6144.
    CHECK(m(full, F, 0, 1, ONE));
    const biome_api::Rule r66 = biome_api::with_river_dist_max(full, 32766);
    CHECK_FALSE(m(r66, F, 0, 1, ONE));
    CHECK(32766 * 3 / 16 == 6143);
    CHECK(m(r66, 6143, 0, 1, ONE));
    CHECK_FALSE(m(r66, 6144, 0, 1, ONE));
    // The shipped 112 m: d34 21 (seven orthogonal steps) matches, 22 does not; 20 (five diagonal) matches.
    const biome_api::Rule r112 = biome_api::with_river_dist_max(full, 112);
    CHECK(m(r112, 21, 0, 1, ONE));
    CHECK(m(r112, 20, 0, 1, ONE));
    CHECK_FALSE(m(r112, 22, 0, 1, ONE));
    CHECK_FALSE(m(r112, 24, 0, 1, ONE));
    // wet_frac_min 0 matches count 0; raw 1 with ws 0.25 (need 0) too; raw 1 with ws 1 (need 1)
    // fails 0 / 1 and matches 1 / 1000.
    CHECK(m(full, F, 0, 1, ONE));
    const biome_api::Rule w1 = biome_api::with_wet_frac_min_raw(full, 1);
    CHECK(need_of(1, ONE / 4) == 0);
    CHECK(m(w1, F, 0, 1, ONE / 4));
    CHECK_FALSE(m(w1, F, 0, 1, ONE));
    CHECK(m(w1, F, 1, 1000, ONE));
    // At the ends of the field range: wet_frac_min 1 needs every cell wet (ws 1).
    const biome_api::Rule wall = biome_api::with_wet_frac_min_raw(full, ONE);
    CHECK(m(wall, F, 25, 25, ONE));
    CHECK_FALSE(m(wall, F, 24, 25, ONE));
    // ws 4 (its bound): need = 4 for wet_frac_min 1, more than any share.
    CHECK_FALSE(m(wall, F, 25, 25, 4 * ONE));
}

TEST_CASE("P1-04 4.1: B4's order puts fen above marsh, heath below moor, wet woodland between forest and steppe") {
    std::vector<std::string> order;
    for (const Def* d : ordered(shipped_defs())) {
        order.push_back(d->key);
    }
    CHECK(order == std::vector<std::string>{"alpine", "fen", "marsh", "highland_moor", "heath", "temperate_forest",
                                            "wet_woodland", "steppe", "grassland"});
    // The twelve shipped colours are pairwise distinct (P1-04 9).
    for (std::size_t a = 0; a < shipped_defs().size(); ++a) {
        for (std::size_t b = a + 1; b < shipped_defs().size(); ++b) {
            CHECK(shipped_defs()[a].colour != shipped_defs()[b].colour);
        }
    }
    // The bounds by size of the 4.1 summary.
    for (const auto& [n, forest, steppe, ww] : std::vector<std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t>>{
             {512, 810 * ONE, 675 * ONE, 506 * ONE + ONE / 4}, {1024, 720 * ONE, 600 * ONE, 450 * ONE},
             {2048, 630 * ONE, 525 * ONE, 393 * ONE + 3 * ONE / 4}}) {
        CAPTURE(n);
        const std::int64_t sc = scale_for(values(), n);
        CHECK(rb(sc, 720) == forest);
        CHECK(rb(sc, 600) == steppe);
        CHECK(rb(sc, 450) == ww);
    }
}

TEST_CASE("P1-04 6 invariant 6, 10.3: B2b, B2c and the layer are independent of the thread count (terrace fixtures)") {
    for (std::uint64_t f = 0; f < 2; ++f) {
        CAPTURE(f);
        const biome_api::Inputs in = designed_inputs(f, true);
        for (const Overrides& o : {Overrides{}, Overrides{{"wet_window_m", "2000"}}, Overrides{{"river_dist_class_min", "1"}}}) {
            const biome_api::Tuning t = tuning(o);
            const std::vector<std::uint8_t> one = biome_api::layer(in, shipped_definitions(), t, f, 1);
            for (const std::uint32_t threads : {2U, 5U, 256U}) {
                CAPTURE(threads);
                CHECK(biome_api::layer(in, shipped_definitions(), t, f, threads) == one);
            }
            CHECK(one == own_biomes(in, shipped_defs(), values(o), f).biome);
        }
    }
}

// --- Lead's rulings on the integration (2026-11-18) -------------------------------------------

TEST_CASE("P1-04 B4 (ruling): a wet scale outside [0.25, 4] is fatal through rule_matches and lookup") {
    const biome_api::Rule fen = biome_api::rule_of(shipped_definitions(), "fen");
    const std::string want = "biomes: rule: the wet scale is outside [0.25, 4]";
    for (const std::int64_t ws : {ONE / 4 - 1, 4 * ONE + 1, std::int64_t{0}, -ONE}) {
        CAPTURE(ws);
        CHECK(fatal_message([&] {
                  (void)biome_api::rule_matches(fen, ONE, 8 * ONE, 900 * ONE, 30, 100, 20, 200, F, 9, 20, ws);
              }) == want);
        CHECK(fatal_message([&] {
                  (void)biome_api::lookup(shipped_definitions(), ONE, 8 * ONE, 900 * ONE, 30, 100, 20, 200, F, 9, 20, ws);
              }) == want);
    }
    for (const std::int64_t ws : {ONE / 4, 4 * ONE}) {
        CHECK_NOTHROW((void)biome_api::lookup(shipped_definitions(), ONE, 8 * ONE, 900 * ONE, 30, 100, 20, 200, F, 9,
                                              20, ws));
    }
}

TEST_CASE("P1-04 B4 (ruling): the lookup seams are fatal for d34 < 0 and counts outside 0 <= count <= total, total >= 1") {
    const biome_api::Rule fen = biome_api::rule_of(shipped_definitions(), "fen");
    struct Bad {
        std::int64_t d34, count, total;
    };
    for (const Bad& b : {Bad{-1, 0, 1}, Bad{F, -1, 1}, Bad{F, 2, 1}, Bad{F, 0, 0}, Bad{F, 0, -1}}) {
        CAPTURE(b.d34);
        CAPTURE(b.count);
        CAPTURE(b.total);
        CHECK_THROWS_AS((void)biome_api::rule_matches(fen, ONE, 8 * ONE, 900 * ONE, 30, 100, 20, 200, b.d34, b.count,
                                                      b.total, ONE),
                        worldgen::FatalError);
        CHECK_THROWS_AS((void)biome_api::lookup(shipped_definitions(), ONE, 8 * ONE, 900 * ONE, 30, 100, 20, 200, b.d34,
                                                b.count, b.total, ONE),
                        worldgen::FatalError);
    }
    // The edges are accepted: d34 0, count = total, count 0.
    CHECK_NOTHROW((void)biome_api::lookup(shipped_definitions(), ONE, 8 * ONE, 900 * ONE, 30, 100, 20, 200, 0, 1, 1, ONE));
    CHECK_NOTHROW((void)biome_api::lookup(shipped_definitions(), ONE, 8 * ONE, 900 * ONE, 30, 100, 20, 200, F, 0, 1, ONE));
}

TEST_CASE("P1-04 7 (ruling): the B2c guard also rejects grids above 2^24 cells, before the array lengths") {
    const std::vector<std::uint8_t> no_water;
    const std::vector<std::int64_t> no_hand;
    CHECK(4097LL * 4096LL > (std::int64_t{1} << 24));
    CHECK(starts(fatal_message([&] { (void)biome_api::wet_share(Grid{4097, 4096}, no_water, no_hand, 5 * ONE, 2); }),
                 PRECONDITION));
    // 4096 x 4096 = 2^24 passes the size check and fails on the lengths, also with the prefix.
    CHECK(starts(fatal_message([&] { (void)biome_api::wet_share(Grid{4096, 4096}, no_water, no_hand, 5 * ONE, 2); }),
                 PRECONDITION));
}

TEST_CASE("P1-04 2, 4.2 (ruling): river_dist_class_min outside 1..15 is fatal in biome_layer") {
    const biome_api::Inputs in = designed_inputs(0);
    for (const std::int64_t k : {std::int64_t{0}, std::int64_t{16}}) {
        CAPTURE(k);
        biome_api::Tuning t = tuning();
        t.river_dist_class_min = k;
        CHECK_THROWS_AS((void)biome_api::layer(in, shipped_definitions(), t, 0, 1), worldgen::FatalError);
    }
}
