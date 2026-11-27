// Spec V3D-01 9.1, U1 to U5: layout, heights, colours, cell and world
// coordinates, ground and bilinear samples.

#include <doctest/doctest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "terrain/terrain_data.hpp"

using namespace unmapped::terrain;

namespace {

Layout layout_of(int n) {
    std::string problem;
    const auto layout = layout_for(n, n, problem);
    REQUIRE(layout.has_value());
    return *layout;
}

std::vector<worldgen::inspect::BiomeRecord> records() {
    std::vector<worldgen::inspect::BiomeRecord> out;
    out.push_back({1, "ocean", "Ocean", 0x183878, 1});
    out.push_back({5, "grassland", "Grassland", 0x8CB45A, 0});
    return out;
}

}  // namespace

TEST_CASE("U1 layout_for") {
    struct Row {
        int n, region_min, region_count;
        float origin;
    };
    for (const Row row : {Row{512, -1, 2, -4096.0f}, Row{1024, -2, 4, -8192.0f}, Row{2048, -4, 8, -16384.0f}}) {
        const Layout layout = layout_of(row.n);
        CHECK(layout.n == row.n);
        CHECK(layout.spacing == 16.0f);
        CHECK(layout.region_size == 256);
        CHECK(layout.region_min == row.region_min);
        CHECK(layout.region_count == row.region_count);
        CHECK(layout.origin == row.origin);
        // The regions cover the map exactly (5.1).
        CHECK(static_cast<float>(layout.region_min * layout.region_size) * layout.spacing == layout.origin);
        CHECK(layout.region_count * layout.region_size == layout.n);
    }
    struct Bad {
        int w, h;
    };
    for (const Bad bad : {Bad{256, 256}, Bad{4096, 4096}, Bad{512, 1024}, Bad{0, 0}}) {
        std::string problem;
        CHECK_FALSE(layout_for(bad.w, bad.h, problem).has_value());
        CHECK(problem == "elevation is " + std::to_string(bad.w) + "x" + std::to_string(bad.h) +
                             "; the 3D preview needs a square map of 512, 1024 or 2048 cells.");
    }
}

TEST_CASE("U2 build_heights") {
    const Layout layout = layout_of(512);
    const std::size_t cells = 512 * 512;
    std::vector<std::int16_t> e(cells, 50);
    std::vector<std::uint16_t> d(cells, 0);
    e[0] = -3005;
    d[0] = 3005;  // ocean: water_depth is -e
    e[1] = 1;
    e[2] = 29990;
    e[3] = 120;
    d[3] = 35;  // lake
    e[4] = 700;  // land, d = 0
    const std::vector<float> h = build_heights(layout, e, d);
    REQUIRE(h.size() == cells);
    CHECK(h[0] == -300.5f);
    CHECK(h[1] == 0.1f);
    CHECK(h[2] == 2999.0f);
    CHECK(h[3] == 15.5f);
    CHECK(h[4] == 70.0f);
    CHECK(h[5] == 5.0f);
    const std::vector<float> dry = build_heights(layout, e, {});
    CHECK(dry[3] == 12.0f);
    CHECK(dry[0] == -300.5f);
    std::vector<std::int16_t> short_e(cells - 1, 1);
    CHECK_THROWS_AS((void)build_heights(layout, short_e, {}), std::invalid_argument);
    std::vector<std::uint16_t> short_d(10, 0);
    CHECK_THROWS_AS((void)build_heights(layout, e, short_d), std::invalid_argument);
}

TEST_CASE("U3 build_colours") {
    const Layout layout = layout_of(512);
    const std::size_t cells = 512 * 512;
    std::vector<std::uint8_t> biome(cells, 5);
    biome[1] = 1;
    biome[2] = 77;  // no record
    const auto recs = records();
    const std::vector<std::uint8_t> c = build_colours(layout, biome, recs);
    REQUIRE(c.size() == 4 * cells);
    CHECK(c[0] == 0x8C);
    CHECK(c[1] == 0xB4);
    CHECK(c[2] == 0x5A);
    CHECK(c[3] == 128);
    CHECK(c[4] == 0x18);
    CHECK(c[5] == 0x38);
    CHECK(c[6] == 0x78);
    CHECK(c[7] == 128);
    CHECK(c[8] == 255);
    CHECK(c[9] == 0);
    CHECK(c[10] == 255);
    CHECK(c[11] == 128);
    CHECK(unknown_biome_cells(biome, recs) == 1);
    const std::vector<std::uint8_t> grey = build_colours(layout, {}, recs);
    REQUIRE(grey.size() == 4 * cells);
    for (std::size_t k = 0; k < cells; k += 4099) {
        CHECK(grey[4 * k] == 128);
        CHECK(grey[4 * k + 1] == 128);
        CHECK(grey[4 * k + 2] == 128);
        CHECK(grey[4 * k + 3] == 128);
    }
    CHECK(sea_colour(recs) == 0x183878u);
    CHECK(sea_colour({}) == 0x183878u);
    std::vector<worldgen::inspect::BiomeRecord> other = {{1, "ocean", "Ocean", 0x102030, 1}};
    CHECK(sea_colour(other) == 0x102030u);
}

TEST_CASE("U4 cell_to_world and world_to_cell") {
    for (const int n : {512, 1024, 2048}) {
        const Layout layout = layout_of(n);
        const float fn = static_cast<float>(n);
        const Vec2 first = cell_to_world(layout, 0, 0);
        CHECK(first.x == -8.0f * fn);
        CHECK(first.z == -8.0f * fn);
        const Vec2 last = cell_to_world(layout, n - 1, n - 1);
        CHECK(last.x == 8.0f * fn - 16.0f);
        CHECK(last.z == 8.0f * fn - 16.0f);
        for (int k = 0; k < n * n; k += 97) {
            const int x = k % n;
            const int y = k / n;
            const Vec2 w = cell_to_world(layout, x, y);
            for (const float dx : {-7.9f, 0.0f, 7.9f}) {
                for (const float dz : {-7.9f, 0.0f, 7.9f}) {
                    const CellHit hit = world_to_cell(layout, w.x + dx, w.z + dz);
                    REQUIRE(hit.inside);
                    REQUIRE(hit.x == x);
                    REQUIRE(hit.y == y);
                }
            }
        }
        CHECK_FALSE(world_to_cell(layout, -8.0f * fn - 8.1f, 0.0f).inside);
        CHECK_FALSE(world_to_cell(layout, 0.0f, -8.0f * fn - 8.1f).inside);
        CHECK_FALSE(world_to_cell(layout, 8.0f * fn - 16.0f + 8.1f, 0.0f).inside);
    }
}

TEST_CASE("U5 ground_clamp and sample_bilinear") {
    const Layout layout = layout_of(512);
    const int n = 512;
    std::vector<float> h(static_cast<std::size_t>(n) * n, 0.0f);
    auto at = [&](int x, int y) -> float& { return h[static_cast<std::size_t>(y) * n + static_cast<std::size_t>(x)]; };
    at(10, 20) = 100.0f;
    at(11, 20) = 200.0f;
    at(10, 21) = 300.0f;
    at(11, 21) = 400.0f;
    at(0, 0) = 50.0f;
    at(n - 1, n - 1) = 70.0f;
    const Vec2 v = cell_to_world(layout, 10, 20);
    // At a vertex.
    CHECK(sample_bilinear(layout, h, v.x, v.z) == doctest::Approx(100.0f));
    CHECK(ground_clamp(layout, h, v.x, v.z) == 400.0f);
    // Cell midpoint.
    CHECK(sample_bilinear(layout, h, v.x + 8.0f, v.z + 8.0f) == doctest::Approx(250.0f));
    CHECK(ground_clamp(layout, h, v.x + 8.0f, v.z + 8.0f) == 400.0f);
    // Edge midpoint.
    CHECK(sample_bilinear(layout, h, v.x + 8.0f, v.z) == doctest::Approx(150.0f));
    // Outside the map: clamped to the edge.
    CHECK(sample_bilinear(layout, h, -100000.0f, -100000.0f) == doctest::Approx(50.0f));
    CHECK(ground_clamp(layout, h, -100000.0f, -100000.0f) == 50.0f);
    CHECK(sample_bilinear(layout, h, 100000.0f, 100000.0f) == doctest::Approx(70.0f));
    CHECK(ground_clamp(layout, h, 100000.0f, 100000.0f) == 70.0f);
    // The last vertex itself (i = n - 2 with fu = 1).
    const Vec2 last = cell_to_world(layout, n - 1, n - 1);
    CHECK(sample_bilinear(layout, h, last.x, last.z) == doctest::Approx(70.0f));
}

TEST_CASE("one_decimal") {
    CHECK(one_decimal(12.25) == "12.2");
    CHECK(one_decimal(-0.04) == "0.0");
    CHECK(one_decimal(-0.1) == "-0.1");
}
