// The composite map of worldgen-cli render --composite map, spec P1-00
// section 10.2 with appendix C items 3 and 4: the hillshade factor on
// hand-made slopes, the shaded colour, the ocean, lake and river overlays,
// and the problems that stop the composite.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "p0_04_reference.hpp"
#include "p1_00_composite_api.hpp"
#include "p1_00_composite_reference.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/inspect/render.hpp"

namespace {

using worldgen::FatalError;
using worldgen::db::LayerGrid;
using worldgen::inspect::hillshade_factor;
using worldgen::inspect::Rgb;
using worldgen::inspect::shade_colour;
using Pixels = std::vector<std::uint8_t>;
using Cells = std::vector<std::int16_t>;

}  // namespace

namespace doctest {
template <>
struct StringMaker<Rgb> {
    static String convert(const Rgb& c) {
        return (std::to_string(int{c.r}) + "," + std::to_string(int{c.g}) + "," + std::to_string(int{c.b})).c_str();
    }
};
}  // namespace doctest

namespace {

// Reference hillshade (P1-00 10.2 step 2), written from the spec text.
std::uint32_t ref_factor(const Cells& e, std::uint32_t w, std::uint32_t h, std::uint32_t x, std::uint32_t y) {
    auto at = [&](std::int64_t cx, std::int64_t cy) -> std::int64_t {
        cx = std::clamp<std::int64_t>(cx, 0, std::int64_t{w} - 1);
        cy = std::clamp<std::int64_t>(cy, 0, std::int64_t{h} - 1);
        return std::max<std::int64_t>(e[static_cast<std::size_t>(cy * std::int64_t{w} + cx)], 0);
    };
    const std::int64_t gx = at(std::int64_t{x} + 1, y) - at(std::int64_t{x} - 1, y);
    const std::int64_t gy = at(x, std::int64_t{y} + 1) - at(x, std::int64_t{y} - 1);
    const std::int64_t s = gx + gy;
    const std::int64_t half = s >= 0 ? s / 2 : -((-s + 1) / 2);  // floor(s / 2)
    return static_cast<std::uint32_t>(std::clamp<std::int64_t>(256 + half, 160, 352));
}

Rgb ref_shade(Rgb c, std::uint32_t f) {
    auto ch = [&](std::uint8_t v) { return static_cast<std::uint8_t>(std::min<std::uint32_t>(255, (v * f + 128) >> 8U)); };
    return Rgb{ch(c.r), ch(c.g), ch(c.b)};
}

Rgb pixel_at(const Pixels& px, std::size_t image_width, std::size_t x, std::size_t y) {
    const std::size_t o = (y * image_width + x) * 3;
    return Rgb{px.at(o), px.at(o + 1), px.at(o + 2)};
}

constexpr Rgb LAKE{64, 128, 204};
constexpr Rgb RIVER_NO_CLASS{52, 100, 200};

// The base colour of a cell (P1-00 10.2 steps 1 and 2): ocean e < 0 is the
// hypsometric ocean colour of -e; otherwise (e = 0 included, see the
// ambiguity noted in the composite tests) the shaded land colour.
Rgb ref_base(const Cells& e, std::uint32_t w, std::uint32_t h, std::uint32_t x, std::uint32_t y) {
    const std::int16_t v = e[y * w + x];
    if (v < 0) {
        return worldgen::inspect::hypsometric_ocean_colour(-std::int64_t{v});
    }
    return ref_shade(worldgen::inspect::hypsometric_land_colour(v), ref_factor(e, w, h, x, y));
}

}  // namespace

TEST_CASE("composite 10.2: hillshade on a flat map and on each cardinal slope") {
    const std::uint32_t n = 5;
    auto grid = [&](auto f) {
        Cells c(n * n);
        for (std::uint32_t y = 0; y < n; ++y) {
            for (std::uint32_t x = 0; x < n; ++x) {
                c[y * n + x] = static_cast<std::int16_t>(f(static_cast<int>(x), static_cast<int>(y)));
            }
        }
        return c;
    };
    const Cells flat = grid([](int, int) { return 100; });
    const Cells east = grid([](int x, int) { return 100 + 10 * x; });   // rising to the east
    const Cells south = grid([](int, int y) { return 100 + 10 * y; });  // rising to the south
    const Cells west = grid([](int x, int) { return 140 - 10 * x; });   // rising to the west
    const Cells north = grid([](int, int y) { return 140 - 10 * y; });  // rising to the north
    for (std::uint32_t y = 0; y < n; ++y) {
        for (std::uint32_t x = 0; x < n; ++x) {
            INFO("cell ", x, ",", y);
            CHECK(hillshade_factor(flat, n, n, x, y) == 256U);
            // Interior: g = 20, f = 256 + 10. Edge (clamped): g = 10, f = 256 + 5.
            const bool x_edge = x == 0 || x == n - 1;
            const bool y_edge = y == 0 || y == n - 1;
            CHECK(hillshade_factor(east, n, n, x, y) == (x_edge ? 261U : 266U));
            CHECK(hillshade_factor(south, n, n, x, y) == (y_edge ? 261U : 266U));
            CHECK(hillshade_factor(west, n, n, x, y) == (x_edge ? 251U : 246U));
            CHECK(hillshade_factor(north, n, n, x, y) == (y_edge ? 251U : 246U));
            CHECK(hillshade_factor(east, n, n, x, y) == ref_factor(east, n, n, x, y));
        }
    }
    // A diagonal slope rising to the south-east adds both gradients:
    // interior 256 + (20 + 20) / 2 = 276; the corner (0, 0): 256 + 10 = 266.
    const Cells se = grid([](int x, int y) { return 100 + 10 * x + 10 * y; });
    CHECK(hillshade_factor(se, n, n, 2, 2) == 276U);
    CHECK(hillshade_factor(se, n, n, 0, 0) == 266U);
    CHECK(hillshade_factor(se, n, n, 4, 4) == 266U);
    CHECK(hillshade_factor(se, n, n, 4, 0) == 266U);
    // One cell: both differences are 0.
    CHECK(hillshade_factor(Cells{1234}, 1, 1, 0, 0) == 256U);
}

TEST_CASE("composite 10.2: hillshade rounding, clamps and ocean neighbours") {
    // Columns 101, 50, 100 on 3 rows: at (1, 1) gx = 100 - 101 = -1, gy = 0;
    // floor(-1 / 2) = -1, so f = 255 (truncation would give 256).
    const Cells odd_neg = {101, 50, 100, 101, 50, 100, 101, 50, 100};
    CHECK(hillshade_factor(odd_neg, 3, 3, 1, 1) == 255U);
    // gx = +1: floor(1 / 2) = 0, f = 256.
    const Cells odd_pos = {100, 50, 101, 100, 50, 101, 100, 50, 101};
    CHECK(hillshade_factor(odd_pos, 3, 3, 1, 1) == 256U);
    // gx = -3: floor(-1.5) = -2, f = 254.
    const Cells neg3 = {103, 50, 100, 103, 50, 100, 103, 50, 100};
    CHECK(hillshade_factor(neg3, 3, 3, 1, 1) == 254U);

    // Clamps. A 3 x 1 row a, b, c: at the middle, gx = c - a, gy = 0.
    auto mid = [](std::int16_t a, std::int16_t c) { return hillshade_factor(Cells{a, 1, c}, 3, 1, 1, 0); };
    CHECK(mid(1, 193) == 352U);   // 256 + 96 = 352, at the clamp
    CHECK(mid(1, 195) == 352U);   // 256 + 97 clamped to 352
    CHECK(mid(1, 30001) == 352U);
    CHECK(mid(1, 191) == 351U);   // 256 + 95
    CHECK(mid(193, 1) == 160U);   // 256 - 96 = 160, at the clamp
    CHECK(mid(194, 1) == 160U);   // floor(-193 / 2) = -97: 159 clamped to 160
    CHECK(mid(192, 1) == 160U);   // floor(-191 / 2) = -96: 160
    CHECK(mid(191, 1) == 161U);   // -190 / 2 = -95: 161
    CHECK(mid(30000, 1) == 160U);

    // Ocean neighbours count as e' = max(e, 0) = 0, not as their depth.
    // West neighbour -500: gx = 100 - 0 = 100, f = 306 (with -500 it would
    // be 600 / 2 and clamp at 352).
    CHECK(hillshade_factor(Cells{-500, 100, 100}, 3, 1, 1, 0) == 306U);
    // East neighbour -500: gx = 0 - 100 = -100, f = 206.
    CHECK(hillshade_factor(Cells{100, 100, -500}, 3, 1, 1, 0) == 206U);
    // North and south: a column of -30000, 100, -30000: gy = 0, f = 256.
    CHECK(hillshade_factor(Cells{-30000, 100, -30000}, 1, 3, 0, 1) == 256U);
    // The cell's own value does not enter, only its neighbours'.
    // Here gx = 250 - 50 = 200: 256 + 100 = 356, clamped to 352.
    CHECK(hillshade_factor(Cells{50, -30000, 250}, 3, 1, 1, 0) == 352U);
    CHECK(hillshade_factor(Cells{50, -30000, 150}, 3, 1, 1, 0) == 306U);

    // Edge cells clamp the coordinates to the map: in a 3 x 1 row 10, 50,
    // 200, cell 0 has gx = e(1) - e(0) = 40 (f = 276) and cell 2 has gx =
    // e(2) - e(1) = 150 (f = 331).
    CHECK(hillshade_factor(Cells{10, 50, 200}, 3, 1, 0, 0) == 276U);
    CHECK(hillshade_factor(Cells{10, 50, 200}, 3, 1, 2, 0) == 331U);

    // Random-ish grid against the reference.
    Cells r(7 * 6);
    for (std::size_t i = 0; i < r.size(); ++i) {
        r[i] = static_cast<std::int16_t>(static_cast<int>((i * 7919U) % 900U) - 300);
    }
    for (std::uint32_t y = 0; y < 6; ++y) {
        for (std::uint32_t x = 0; x < 7; ++x) {
            INFO("cell ", x, ",", y);
            CHECK(hillshade_factor(r, 7, 6, x, y) == ref_factor(r, 7, 6, x, y));
        }
    }

    // Contract violations.
    CHECK_THROWS_AS((void)hillshade_factor(Cells{1, 2, 3}, 2, 2, 0, 0), FatalError);
    CHECK_THROWS_AS((void)hillshade_factor(Cells{1, 2, 3, 4}, 2, 2, 2, 0), FatalError);
    CHECK_THROWS_AS((void)hillshade_factor(Cells{1, 2, 3, 4}, 2, 2, 0, 2), FatalError);
}

TEST_CASE("composite 10.2: the shaded colour") {
    CHECK(shade_colour(Rgb{70, 120, 64}, 256) == Rgb{70, 120, 64});
    // (200 * 352 + 128) >> 8 = 275 -> 255; (100 * 352 + 128) >> 8 = 138;
    // (1 * 352 + 128) >> 8 = 1.
    CHECK(shade_colour(Rgb{200, 100, 1}, 352) == Rgb{255, 138, 1});
    // (255 * 160 + 128) >> 8 = 159; (1 * 160 + 128) >> 8 = 1; 0 stays 0.
    CHECK(shade_colour(Rgb{255, 1, 0}, 160) == Rgb{159, 1, 0});
    // Half rounding: (3 * 128 + 128) >> 8 = 2 (1.5 rounds up).
    CHECK(shade_colour(Rgb{3, 3, 3}, 128) == Rgb{2, 2, 2});
    for (std::uint32_t f = 160; f <= 352; ++f) {
        for (unsigned c = 0; c < 256; ++c) {
            const auto v = static_cast<std::uint8_t>(c);
            const Rgb got = shade_colour(Rgb{v, v, v}, f);
            const Rgb want = ref_shade(Rgb{v, v, v}, f);
            if (got != want) {
                INFO("c ", c, " f ", f);
                CHECK(got == want);
            }
        }
    }
}

TEST_CASE("composite 10.2: elevation only, and each optional layer added") {
    // 4 x 3 elevation with ocean, coast, e = 0 and high ground.
    const std::uint32_t w = 4;
    const std::uint32_t h = 3;
    const Cells e = {-3000, -1, 0, 1, -200, 250, 900, 5000, -30000, 2000, 15000, 30000};
    const LayerGrid elev = p004::grid_of<std::int16_t>(w, h, e);
    // water: 3 ocean on the ocean cells, 1 river, 2 lake, 0 dry; one lake on
    // an ocean cell and one river on an ocean cell to show that step 3
    // replaces the colour whatever e is.
    const std::vector<std::uint8_t> water = {3, 2, 0, 1, 1, 0, 2, 1, 3, 0, 1, 0};
    const std::vector<std::uint8_t> river = {0, 0, 0, 1, 7, 0, 0, 15, 0, 0, 4, 0};
    const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
    const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);

    SUBCASE("elevation only") {
        CHECK(!comp_api::problem(elev, nullptr, nullptr, nullptr).has_value());
        const Pixels px = comp_api::render_map(elev, nullptr, nullptr, nullptr, 1);
        REQUIRE(px.size() == 3U * w * h);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                INFO("cell ", x, ",", y);
                CHECK(pixel_at(px, w, x, y) == ref_base(e, w, h, x, y));
            }
        }
        // Hand-checked cells. (0, 0) ocean depth 3000: R 56 + floor((2*-24*2000
        // + 4000)/8000) = 56 - 12 = 44, G 104 - 19 = 85, B 168 - 19 = 149.
        CHECK(pixel_at(px, w, 0, 0) == Rgb{44, 85, 149});
        // (0, 2) depth 30000: the last ocean colour.
        CHECK(pixel_at(px, w, 0, 2) == Rgb{16, 34, 82});
        // (3, 0) e = 1: land colour (70, 120, 64); neighbours west e' 0 (e =
        // 0), east clamped to itself (1), north clamped (1), south 5000:
        // gx = 1 - 0 = 1, gy = 5000 - 1 = 4999, f = 256 + 2500 -> 352;
        // (70*352+128)>>8 = 96, (120*352+128)>>8 = 165, (64*352+128)>>8 = 88.
        CHECK(pixel_at(px, w, 3, 0) == Rgb{96, 165, 88});
    }
    SUBCASE("with water: lakes and rivers without classes") {
        CHECK(!comp_api::problem(elev, &wg, nullptr, nullptr).has_value());
        const Pixels px = comp_api::render_map(elev, &wg, nullptr, nullptr, 1);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                const std::uint8_t v = water[y * w + x];
                const Rgb want = v == 2 ? LAKE : v == 1 ? RIVER_NO_CLASS : ref_base(e, w, h, x, y);
                INFO("cell ", x, ",", y, " water ", int{v});
                CHECK(pixel_at(px, w, x, y) == want);
            }
        }
    }
    SUBCASE("with water and river: rivers drawn by step 4 (amended 2026-11-16), no flow_dir") {
        CHECK(!comp_api::problem(elev, &wg, &rg, nullptr).has_value());
        const Pixels px = comp_api::render_map(elev, &wg, &rg, nullptr, 1);
        std::vector<Rgb> base;
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                base.push_back(ref_base(e, w, h, x, y));
            }
        }
        const std::vector<Rgb> want = comp_ref::overlay(w, h, base, &water, &river, nullptr);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                INFO("cell ", x, ",", y);
                CHECK(pixel_at(px, w, x, y) == want[y * w + x]);
            }
        }
        // By hand: (3, 0) class 1 (70, 125, 215); (0, 1) class 7 and (3, 1)
        // class 15 (20, 50, 160); (2, 2) class 4 (30, 80, 190); class 7 at
        // (0, 1) thickens east onto dry (1, 1); class 15 at (3, 1) thickens
        // south onto (3, 2), painted after class 4's east step there.
        CHECK(pixel_at(px, w, 3, 0) == Rgb{70, 125, 215});
        CHECK(pixel_at(px, w, 0, 1) == Rgb{20, 50, 160});
        CHECK(pixel_at(px, w, 3, 1) == Rgb{20, 50, 160});
        CHECK(pixel_at(px, w, 2, 2) == Rgb{30, 80, 190});
        CHECK(pixel_at(px, w, 1, 1) == Rgb{20, 50, 160});
        CHECK(pixel_at(px, w, 3, 2) == Rgb{20, 50, 160});
        // A lake cell keeps the lake colour; (0, 2) is ocean (water 3) below
        // the class-7 cell and keeps its ocean colour.
        CHECK(pixel_at(px, w, 1, 0) == LAKE);
        CHECK(pixel_at(px, w, 0, 2) == ref_base(e, w, h, 0, 2));
    }
    SUBCASE("river without water is not used") {
        CHECK(comp_api::render_map(elev, nullptr, &rg, nullptr, 1) == comp_api::render_map(elev, nullptr, nullptr, nullptr, 1));
    }
    SUBCASE("scale 2 repeats each cell's colour in a 2 x 2 block") {
        const Pixels one = comp_api::render_map(elev, &wg, &rg, nullptr, 1);
        const Pixels two = comp_api::render_map(elev, &wg, &rg, nullptr, 2);
        REQUIRE(two.size() == 4U * one.size());
        for (std::uint32_t y = 0; y < 2 * h; ++y) {
            for (std::uint32_t x = 0; x < 2 * w; ++x) {
                CHECK(pixel_at(two, 2 * w, x, y) == pixel_at(one, w, x / 2, y / 2));
            }
        }
    }
}

TEST_CASE("composite 10.2, appendix C 3: elevation 0 in the composite") {
    // Step 1 is e < 0, so e = 0 is not ocean: it takes step 2 with the land
    // colour of t = 0 (70, 120, 64). On a flat map of zeros, f = 256.
    // (Appendix C item 3 says the composite counts 0 as neither land nor
    // ocean, without giving a colour; this test follows the step order of
    // 10.2, which is also what the header documents.)
    const Pixels px = comp_api::render_map(p004::grid_of<std::int16_t>(2, 1, {0, 0}), nullptr, nullptr, nullptr, 1);
    CHECK(pixel_at(px, 2, 0, 0) == Rgb{70, 120, 64});
    CHECK(pixel_at(px, 2, 1, 0) == Rgb{70, 120, 64});
    // And it differs from what --layer elevation (hypsometric) draws for 0.
    CHECK(worldgen::inspect::hypsometric_colour(0) == Rgb{120, 170, 205});
}

TEST_CASE("composite appendix C 4: layers that stop the composite") {
    const LayerGrid elev = p004::grid_of<std::int16_t>(2, 2, {1, 2, -1, -2});
    const LayerGrid water = p004::grid_of<std::uint8_t>(2, 2, {0, 0, 3, 3});
    const LayerGrid river = p004::grid_of<std::uint8_t>(2, 2, {0, 0, 0, 0});
    auto names = [](const std::optional<std::string>& p, const char* layer) {
        return p.has_value() && p->find(layer) != std::string::npos;
    };
    // Elevation of the wrong type.
    const LayerGrid elev_u16 = p004::grid_of<std::uint16_t>(2, 2, {1, 2, 3, 4});
    const LayerGrid elev_i32 = p004::grid_of<std::int32_t>(2, 2, {1, 2, -1, -2});
    CHECK(names(comp_api::problem(elev_u16, nullptr, nullptr, nullptr), "elevation"));
    CHECK(names(comp_api::problem(elev_i32, &water, &river, nullptr), "elevation"));
    // Invalid elevation grid.
    CHECK(comp_api::problem(p004::grid_of<std::int16_t>(2, 2, {1}), nullptr, nullptr, nullptr).has_value());
    // Optional layers of the wrong type.
    const LayerGrid water_i16 = p004::grid_of<std::int16_t>(2, 2, {0, 0, 3, 3});
    const LayerGrid river_u16 = p004::grid_of<std::uint16_t>(2, 2, {0, 0, 0, 0});
    CHECK(names(comp_api::problem(elev, &water_i16, nullptr, nullptr), "water"));
    CHECK(names(comp_api::problem(elev, &water, &river_u16, nullptr), "river"));
    // Optional layers of the wrong size.
    const LayerGrid water_small = p004::grid_of<std::uint8_t>(2, 1, {0, 0});
    const LayerGrid river_wide = p004::grid_of<std::uint8_t>(4, 1, {0, 0, 0, 0});
    CHECK(names(comp_api::problem(elev, &water_small, nullptr, nullptr), "water"));
    CHECK(names(comp_api::problem(elev, &water, &river_wide, nullptr), "river"));
    // An optional layer of the wrong type is a failure (appendix C item 4),
    // also river when water is absent and river would not be drawn.
    CHECK(names(comp_api::problem(elev, nullptr, &river_u16, nullptr), "river"));
    // All good.
    CHECK(!comp_api::problem(elev, &water, &river, nullptr).has_value());
    // render_composite_map refuses the same.
    CHECK_THROWS_AS((void)comp_api::render_map(elev_u16, nullptr, nullptr, nullptr, 1), FatalError);
    CHECK_THROWS_AS((void)comp_api::render_map(elev, &water_small, nullptr, nullptr, 1), FatalError);
    CHECK_THROWS_AS((void)comp_api::render_map(elev, &water, &river_u16, nullptr, 1), FatalError);
    CHECK_THROWS_AS((void)comp_api::render_map(elev, nullptr, nullptr, nullptr, 0), FatalError);
    CHECK_THROWS_AS((void)comp_api::render_map(elev, nullptr, nullptr, nullptr, 17), FatalError);
}

TEST_CASE("composite: names") {
    using worldgen::inspect::Composite;
    CHECK(worldgen::inspect::composite_name(Composite::map) == "map");
    CHECK(worldgen::inspect::composite_from_name("map") == std::optional<Composite>(Composite::map));
    CHECK(!worldgen::inspect::composite_from_name("Map").has_value());
    CHECK(!worldgen::inspect::composite_from_name("biomes").has_value());
    CHECK(!worldgen::inspect::composite_from_name("").has_value());
}
