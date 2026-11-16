// River drawing of the composite map (spec P1-00 10.2 step 4, amended
// 2026-11-16 after the critic's S1; X7 "the river drawing example of 10.2
// composite step 4, pixel by pixel"). The library is reached through
// p1_00_composite_api.hpp; the expected images come from the test's own
// painter, p1_00_composite_reference.hpp, and the spec's hand values.

#include <doctest/doctest.h>

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
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/inspect/render.hpp"

namespace {

using worldgen::db::LayerGrid;
using worldgen::inspect::Rgb;
using Pixels = std::vector<std::uint8_t>;
using U8 = std::vector<std::uint8_t>;

Rgb pixel_at(const Pixels& px, std::size_t image_width, std::size_t x, std::size_t y) {
    const std::size_t o = (y * image_width + x) * 3;
    return Rgb{px.at(o), px.at(o + 1), px.at(o + 2)};
}

bool same(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

// The composite of elevation, water and river (no flow_dir) as the per-cell
// base colours of steps 1 and 2: rendered with no water layer at all.
std::vector<Rgb> base_of(const LayerGrid& elev, std::uint32_t w, std::uint32_t h) {
    const Pixels px = comp_api::render_map(elev, nullptr, nullptr, nullptr, 1);
    std::vector<Rgb> out;
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            out.push_back(pixel_at(px, w, x, y));
        }
    }
    return out;
}

}  // namespace

TEST_CASE("P1-00 10.2 step 4, X7: the 4 x 3 river drawing example, pixel by pixel") {
    const std::uint32_t w = 4;
    const std::uint32_t h = 3;
    // Dry land everywhere (a gentle constant height, so the base is one
    // shaded colour); river cells (1, 1) class 4 flow_dir 1 (SE) and (2, 2)
    // class 1 flow_dir 0.
    const std::vector<std::int16_t> e(12, 100);
    U8 water(12, 0), river(12, 0), dir(12, 0);
    water[1 * 4 + 1] = 1;
    river[1 * 4 + 1] = 4;
    dir[1 * 4 + 1] = 1;
    water[2 * 4 + 2] = 1;
    river[2 * 4 + 2] = 1;
    dir[2 * 4 + 2] = 0;
    // The paint list of the spec: (1,1,4), (2,1,4), (2,1,4), (1,2,4), (2,2,1).
    const std::vector<comp_ref::Entry> list = comp_ref::paint_list(w, h, water, river, &dir);
    REQUIRE(list.size() == 5U);
    const std::int64_t want_list[5][3] = {{1, 1, 4}, {2, 1, 4}, {2, 1, 4}, {1, 2, 4}, {2, 2, 1}};
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK(list[i].x == want_list[i][0]);
        CHECK(list[i].y == want_list[i][1]);
        CHECK(std::int64_t{list[i].k} == want_list[i][2]);
    }
    const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, e);
    const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
    const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
    const LayerGrid dg = p004::grid_of<std::uint8_t>(w, h, dir);
    REQUIRE(!comp_api::problem(eg, &wg, &rg, &dg).has_value());
    const Pixels px = comp_api::render_map(eg, &wg, &rg, &dg, 1);
    const std::vector<Rgb> base = base_of(eg, w, h);
    CHECK(same(pixel_at(px, w, 2, 2), Rgb{70, 125, 215}));
    CHECK(same(pixel_at(px, w, 1, 1), Rgb{30, 80, 190}));
    CHECK(same(pixel_at(px, w, 2, 1), Rgb{30, 80, 190}));
    CHECK(same(pixel_at(px, w, 1, 2), Rgb{30, 80, 190}));
    // Every other cell keeps its shaded land colour.
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const bool painted = (x == 2 && y == 2) || (x == 1 && y == 1) || (x == 2 && y == 1) || (x == 1 && y == 2);
            if (!painted) {
                INFO("cell ", x, ",", y);
                CHECK(same(pixel_at(px, w, x, y), base[y * w + x]));
            }
        }
    }
    // If (1, 2) were a lake cell it would stay (64, 128, 204).
    U8 water_lake = water;
    water_lake[2 * 4 + 1] = 2;
    const LayerGrid wl = p004::grid_of<std::uint8_t>(w, h, water_lake);
    const Pixels pl = comp_api::render_map(eg, &wl, &rg, &dg, 1);
    CHECK(same(pixel_at(pl, w, 1, 2), Rgb{64, 128, 204}));
    CHECK(same(pixel_at(pl, w, 2, 1), Rgb{30, 80, 190}));
    // Without flow_dir there is no corner closing: (2, 1) is still painted
    // by the thickening of (1, 1), since class 4 thickens east.
    const Pixels pn = comp_api::render_map(eg, &wg, &rg, nullptr, 1);
    CHECK(same(pixel_at(pn, w, 2, 1), Rgb{30, 80, 190}));
    // With river absent: every river cell (52, 100, 200), nothing else.
    const Pixels pr = comp_api::render_map(eg, &wg, nullptr, &dg, 1);
    CHECK(same(pixel_at(pr, w, 1, 1), Rgb{52, 100, 200}));
    CHECK(same(pixel_at(pr, w, 2, 2), Rgb{52, 100, 200}));
    CHECK(same(pixel_at(pr, w, 2, 1), base[1 * 4 + 2]));
    CHECK(same(pixel_at(pr, w, 1, 2), base[2 * 4 + 1]));
}

TEST_CASE("P1-00 10.2 step 4: corner closing on each diagonal code, and none on orthogonal codes") {
    // A 5 x 5 dry map with one class-1 river cell at the centre.
    const std::uint32_t w = 5;
    const std::uint32_t h = 5;
    const std::vector<std::int16_t> e(25, 100);
    const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, e);
    const std::vector<Rgb> base = base_of(eg, w, h);
    static constexpr std::int64_t DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    for (std::uint8_t d = 0; d < 8; ++d) {
        CAPTURE(int{d});
        U8 water(25, 0), river(25, 0), dir(25, 0);
        water[12] = 1;
        river[12] = 1;
        dir[12] = d;
        const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
        const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
        const LayerGrid dg = p004::grid_of<std::uint8_t>(w, h, dir);
        const Pixels px = comp_api::render_map(eg, &wg, &rg, &dg, 1);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                const bool centre = x == 2 && y == 2;
                const bool corner = (d & 1U) != 0U && y == 2 && std::int64_t{x} == 2 + DX[d];
                INFO("cell ", x, ",", y);
                CHECK(same(pixel_at(px, w, x, y), centre || corner ? Rgb{70, 125, 215} : base[y * w + x]));
            }
        }
    }
}

TEST_CASE("P1-00 10.2 step 4: the class colour ramp and thickening from class 4") {
    const std::uint32_t w = 3;
    const std::uint32_t h = 3;
    const std::vector<std::int16_t> e(9, 100);
    const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, e);
    const std::vector<Rgb> base = base_of(eg, w, h);
    const Rgb ramp[16] = {{}, {70, 125, 215}, {55, 110, 210}, {40, 95, 200}, {30, 80, 190}, {25, 65, 175},
                          {20, 50, 160}, {20, 50, 160}, {20, 50, 160}, {20, 50, 160}, {20, 50, 160},
                          {20, 50, 160}, {20, 50, 160}, {20, 50, 160}, {20, 50, 160}, {20, 50, 160}};
    for (std::uint8_t k = 1; k <= 15; ++k) {
        CAPTURE(int{k});
        U8 water(9, 0), river(9, 0);
        water[4] = 1;  // centre
        river[4] = k;
        const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
        const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
        const Pixels px = comp_api::render_map(eg, &wg, &rg, nullptr, 1);
        CHECK(same(pixel_at(px, w, 1, 1), ramp[k]));
        CHECK(same(comp_ref::river_class_colour(k), ramp[k]));
        // East and south of the cell only from class 4; never west or north.
        CHECK(same(pixel_at(px, w, 2, 1), k >= 4 ? ramp[k] : base[1 * 3 + 2]));
        CHECK(same(pixel_at(px, w, 1, 2), k >= 4 ? ramp[k] : base[2 * 3 + 1]));
        CHECK(same(pixel_at(px, w, 0, 1), base[1 * 3 + 0]));
        CHECK(same(pixel_at(px, w, 1, 0), base[0 * 3 + 1]));
        CHECK(same(pixel_at(px, w, 2, 2), base[2 * 3 + 2]));
    }
}

TEST_CASE("P1-00 10.2 step 4: rivers are drawn in ascending class, list order within a class") {
    // (0, 0) class 6 thickens onto (1, 0) and (0, 1); (1, 0) is a class-2
    // river cell and (0, 1) a class-9 river cell. Ascending class: (1, 0)
    // ends class 6 (painted after its own class 2), (0, 1) ends class 9.
    const std::uint32_t w = 3;
    const std::uint32_t h = 2;
    const std::vector<std::int16_t> e(6, 100);
    U8 water = {1, 1, 0, 1, 0, 0};
    U8 river = {6, 2, 0, 9, 0, 0};
    const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, e);
    const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
    const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
    const Pixels px = comp_api::render_map(eg, &wg, &rg, nullptr, 1);
    CHECK(same(pixel_at(px, w, 0, 0), Rgb{20, 50, 160}));
    CHECK(same(pixel_at(px, w, 1, 0), Rgb{20, 50, 160}));  // class 6 over class 2
    CHECK(same(pixel_at(px, w, 0, 1), Rgb{20, 50, 160}));  // class 9 (also the class-6 colour)
    // A class-5 cell (0, 0) whose east thickening lands on a class-4 river
    // cell (1, 0): the class-5 colour wins although (1, 0) comes later in
    // index order.
    const U8 water2 = {1, 1, 0, 0, 0, 0};
    const U8 river2 = {5, 4, 0, 0, 0, 0};
    const LayerGrid wg2 = p004::grid_of<std::uint8_t>(w, h, water2);
    const LayerGrid rg2 = p004::grid_of<std::uint8_t>(w, h, river2);
    const Pixels p2 = comp_api::render_map(eg, &wg2, &rg2, nullptr, 1);
    CHECK(same(pixel_at(p2, w, 1, 0), Rgb{25, 65, 175}));  // class 5 painted after class 4
    CHECK(same(pixel_at(p2, w, 1, 1), Rgb{30, 80, 190}));
    CHECK(same(pixel_at(p2, w, 2, 0), Rgb{30, 80, 190}));
    CHECK(same(pixel_at(p2, w, 0, 1), Rgb{25, 65, 175}));
}

TEST_CASE("P1-00 10.2 step 4: never over a lake or ocean cell, never off the map (edges, all codes)") {
    // Rivers of class 4 to 15 along the east and south edges with diagonal
    // codes pointing off the map, next to lake and ocean cells.
    const std::uint32_t w = 4;
    const std::uint32_t h = 4;
    const std::vector<std::int16_t> e = {100, 100, 100, 100, 100, -5, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100};
    U8 water = {0, 1, 2, 1, 0, 3, 1, 1, 0, 2, 0, 1, 1, 1, 1, 1};
    U8 river = {0, 4, 0, 9, 0, 0, 15, 6, 0, 0, 0, 7, 5, 4, 8, 15};
    U8 dir = {0, 3, 0, 1, 0, 8, 5, 7, 0, 0, 0, 1, 3, 1, 1, 1};
    const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, e);
    const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
    const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
    const LayerGrid dg = p004::grid_of<std::uint8_t>(w, h, dir);
    const Pixels plain = comp_api::render_map(eg, &wg, nullptr, nullptr, 1);
    const Pixels px = comp_api::render_map(eg, &wg, &rg, &dg, 1);
    REQUIRE(px.size() == 3U * w * h);
    const std::vector<Rgb> want = comp_ref::overlay(w, h, base_of(eg, w, h), &water, &river, &dir);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            const std::size_t i = y * w + x;
            INFO("cell ", x, ",", y, " water ", int{water[i]});
            CHECK(same(pixel_at(px, w, x, y), want[i]));
            if (water[i] >= 2U) {
                // Lake and ocean cells are exactly as without rivers.
                CHECK(same(pixel_at(px, w, x, y), pixel_at(plain, w, x, y)));
            }
        }
    }
}

TEST_CASE("P1-00 10.2 step 4: random layers against the test's own painter, scale 1 and 3") {
    worldgen::Rng rng(worldgen::stream_key(5, "test.hydro"));
    for (int k = 0; k < 60; ++k) {
        const std::uint32_t w = 2 + static_cast<std::uint32_t>(rng.uniform_u64(30));
        const std::uint32_t h = 2 + static_cast<std::uint32_t>(rng.uniform_u64(30));
        const std::size_t n = std::size_t{w} * h;
        std::vector<std::int16_t> e(n);
        U8 water(n), river(n), dir(n);
        for (std::size_t i = 0; i < n; ++i) {
            e[i] = static_cast<std::int16_t>(rng.uniform_i64(-300, 3000));
            if (e[i] == 0) {
                e[i] = 1;
            }
            const std::uint64_t r = rng.uniform_u64(10);
            water[i] = e[i] < 0 ? 3 : r < 4 ? 1 : r < 5 ? 2 : 0;
            river[i] = water[i] == 1U ? static_cast<std::uint8_t>(1 + rng.uniform_u64(15)) : 0;
            dir[i] = static_cast<std::uint8_t>(rng.uniform_u64(8));
        }
        CAPTURE(k);
        const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, e);
        const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
        const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
        const LayerGrid dg = p004::grid_of<std::uint8_t>(w, h, dir);
        const std::vector<Rgb> want = comp_ref::overlay(w, h, base_of(eg, w, h), &water, &river, &dir);
        const Pixels px = comp_api::render_map(eg, &wg, &rg, &dg, 1);
        REQUIRE(px.size() == 3U * n);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                REQUIRE(same(pixel_at(px, w, x, y), want[y * w + x]));
            }
        }
        const Pixels p3 = comp_api::render_map(eg, &wg, &rg, &dg, 3);
        REQUIRE(p3.size() == 27U * n);
        for (std::uint32_t y = 0; y < 3 * h; ++y) {
            for (std::uint32_t x = 0; x < 3 * w; ++x) {
                REQUIRE(same(pixel_at(p3, 3 * w, x, y), want[(y / 3) * w + x / 3]));
            }
        }
    }
}

TEST_CASE("P1-00 10.2: composite_river_colour is the step 4 ramp; class 0 is (255, 0, 255)") {
    CHECK(same(comp_api::river_colour(0), Rgb{255, 0, 255}));
    for (std::uint64_t k = 1; k <= 20; ++k) {
        CAPTURE(k);
        CHECK(same(comp_api::river_colour(k), comp_ref::river_class_colour(static_cast<std::uint8_t>(k))));
    }
    CHECK(same(comp_api::river_colour(1), Rgb{70, 125, 215}));
    CHECK(same(comp_api::river_colour(5), Rgb{25, 65, 175}));
    CHECK(same(comp_api::river_colour(6), Rgb{20, 50, 160}));
    CHECK(same(comp_api::river_colour(255), Rgb{20, 50, 160}));
    CHECK(same(comp_api::river_colour(std::uint64_t{1} << 40), Rgb{20, 50, 160}));
}

TEST_CASE("P1-00 10.2 step 4: a flow_dir code above 7 is not diagonal; a class-0 river cell is not painted") {
    const std::uint32_t w = 5;
    const std::uint32_t h = 3;
    const std::vector<std::int16_t> e(15, 100);
    const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, e);
    const std::vector<Rgb> base = base_of(eg, w, h);
    for (const std::uint8_t d : {std::uint8_t{8}, std::uint8_t{9}, std::uint8_t{11}, std::uint8_t{255}}) {
        CAPTURE(int{d});
        U8 water(15, 0), river(15, 0), dir(15, 0);
        water[7] = 1;  // (2, 1)
        river[7] = 2;
        dir[7] = d;
        const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
        const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
        const LayerGrid dg = p004::grid_of<std::uint8_t>(w, h, dir);
        REQUIRE(!comp_api::problem(eg, &wg, &rg, &dg).has_value());
        const Pixels px = comp_api::render_map(eg, &wg, &rg, &dg, 1);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                INFO("cell ", x, ",", y);
                CHECK(same(pixel_at(px, w, x, y), x == 2 && y == 1 ? Rgb{55, 110, 210} : base[y * w + x]));
            }
        }
    }
    // Class 0 on a water-1 cell: no entry, so the cell keeps its land colour
    // and neither corner closing nor thickening happens.
    U8 water(15, 0), river(15, 0), dir(15, 0);
    water[7] = 1;
    dir[7] = 1;
    const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
    const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
    const LayerGrid dg = p004::grid_of<std::uint8_t>(w, h, dir);
    CHECK(comp_api::render_map(eg, &wg, &rg, &dg, 1) == comp_api::render_map(eg, nullptr, nullptr, nullptr, 1));
    CHECK(comp_ref::paint_list(w, h, water, river, &dir).empty());
}

TEST_CASE("P1-00 10.2: flow_dir validation: a u8 grid of the elevation's size, else a problem naming it") {
    const std::uint32_t w = 3;
    const std::uint32_t h = 2;
    const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, std::vector<std::int16_t>(6, 100));
    const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, U8(6, 0));
    const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, U8(6, 0));
    const LayerGrid dir_ok = p004::grid_of<std::uint8_t>(w, h, U8(6, 0));
    const LayerGrid dir_i16 = p004::grid_of<std::int16_t>(w, h, std::vector<std::int16_t>(6, 0));
    const LayerGrid dir_u32 = p004::grid_of<std::uint32_t>(w, h, std::vector<std::uint32_t>(6, 0));
    const LayerGrid dir_wide = p004::grid_of<std::uint8_t>(w + 1, h, U8(8, 0));
    const LayerGrid dir_tall = p004::grid_of<std::uint8_t>(w, h + 1, U8(9, 0));
    auto names_flow_dir = [](const std::optional<std::string>& p) {
        return p.has_value() && p->find("flow_dir") != std::string::npos;
    };
    CHECK(!comp_api::problem(eg, &wg, &rg, &dir_ok).has_value());
    CHECK(!comp_api::problem(eg, &wg, nullptr, &dir_ok).has_value());
    for (const LayerGrid* bad : {&dir_i16, &dir_u32, &dir_wide, &dir_tall}) {
        CHECK(names_flow_dir(comp_api::problem(eg, &wg, &rg, bad)));
        CHECK(names_flow_dir(comp_api::problem(eg, &wg, nullptr, bad)));
        // Given without water it is unused, but still validated ("each
        // optional layer that is given").
        CHECK(names_flow_dir(comp_api::problem(eg, nullptr, nullptr, bad)));
        CHECK_THROWS_AS(static_cast<void>(comp_api::render_map(eg, &wg, &rg, bad, 1)), worldgen::FatalError);
    }
    const std::optional<std::string> p16 = comp_api::problem(eg, &wg, &rg, &dir_i16);
    REQUIRE(p16.has_value());
    CHECK(p16->find("i16") != std::string::npos);
    CHECK(p16->find("u8") != std::string::npos);
}

TEST_CASE("P1-00 10.2: the forms without flow_dir equal the long forms with flow_dir null") {
    worldgen::Rng rng(worldgen::stream_key(6, "test.hydro"));
    for (int k = 0; k < 10; ++k) {
        const std::uint32_t w = 3 + static_cast<std::uint32_t>(rng.uniform_u64(12));
        const std::uint32_t h = 3 + static_cast<std::uint32_t>(rng.uniform_u64(12));
        const std::size_t n = std::size_t{w} * h;
        std::vector<std::int16_t> e(n);
        U8 water(n), river(n);
        for (std::size_t i = 0; i < n; ++i) {
            e[i] = static_cast<std::int16_t>(rng.uniform_i64(1, 2000));
            water[i] = rng.uniform_u64(3) == 0 ? 1 : 0;
            river[i] = water[i] == 1U ? static_cast<std::uint8_t>(1 + rng.uniform_u64(9)) : 0;
        }
        const LayerGrid eg = p004::grid_of<std::int16_t>(w, h, e);
        const LayerGrid wg = p004::grid_of<std::uint8_t>(w, h, water);
        const LayerGrid rg = p004::grid_of<std::uint8_t>(w, h, river);
        CHECK(comp_api::render_map3(eg, &wg, &rg, 2) == comp_api::render_map(eg, &wg, &rg, nullptr, 2));
        CHECK(comp_api::render_map3(eg, &wg, nullptr, 1) == comp_api::render_map(eg, &wg, nullptr, nullptr, 1));
        CHECK(comp_api::problem3(eg, &wg, &rg) == comp_api::problem(eg, &wg, &rg, nullptr));
    }
}
