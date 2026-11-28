// Spec V3D-04 10.1, U37 to U39: the option --look-v3d03, the light's
// constants and the sun's direction, and the start of --at-peak.

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "terrain/ground.hpp"
#include "terrain/look.hpp"

using namespace unmapped::terrain;

namespace {

bool equal(const Look& a, const Look& b) {
    return a.rock == b.rock && a.light == b.light && a.snow == b.snow && a.shrubs == b.shrubs && a.sea == b.sea;
}

}  // namespace

TEST_CASE("U37 parse_look_v3d03") {
    const Look none;
    CHECK(none.rock);
    CHECK(none.light);
    CHECK(none.snow);
    CHECK(none.shrubs);
    CHECK(none.sea);
    const auto all = parse_look_v3d03("all");
    REQUIRE(all.has_value());
    CHECK(equal(*all, Look{false, false, false, false, false}));
    const auto rock = parse_look_v3d03("rock");
    REQUIRE(rock.has_value());
    CHECK(equal(*rock, Look{false, true, true, true, true}));
    const auto two = parse_look_v3d03("sea,rock");
    REQUIRE(two.has_value());
    CHECK(equal(*two, Look{false, true, true, true, false}));
    const auto again = parse_look_v3d03("rock,rock");
    REQUIRE(again.has_value());
    CHECK(equal(*again, *rock));
    const auto every = parse_look_v3d03("shrubs,light,snow");
    REQUIRE(every.has_value());
    CHECK(equal(*every, Look{true, false, false, false, true}));
    for (const char* bad : {"", "Rock", "trees", "rock,", ",rock", "all,rock", "rock sea", "ALL"}) {
        CAPTURE(bad);
        CHECK_FALSE(parse_look_v3d03(bad).has_value());
    }
    CHECK(restored_items(Look{}).empty());
    CHECK(restored_items(*all) == "rock, light, snow, shrubs, sea");
    CHECK(restored_items(*two) == "rock, sea");
}

TEST_CASE("U38 sun_direction and the light constants") {
    const auto a = sun_direction(315.0f, 40.0f);
    CHECK(std::abs(a[0] - 0.541675f) < 1e-5f);
    CHECK(std::abs(a[1] + 0.642788f) < 1e-5f);
    CHECK(std::abs(a[2] - 0.541675f) < 1e-5f);
    const auto b = sun_direction(210.0f, 45.0f);
    CHECK(std::abs(b[0] - 0.353553f) < 1e-5f);
    CHECK(std::abs(b[1] + 0.707107f) < 1e-5f);
    CHECK(std::abs(b[2] + 0.612372f) < 1e-5f);
    for (const auto& d : {a, b, sun_direction(0.0f, 10.0f), sun_direction(123.0f, 77.0f)}) {
        CHECK(std::abs(std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) - 1.0f) < 1e-6f);
    }
    CHECK(SUN_AZIMUTH_DEG == 210.0f);
    CHECK(SUN_ELEVATION_DEG == 45.0f);
    CHECK(SUN_ENERGY == 0.75f);
    CHECK(AMBIENT_SKY_CONTRIBUTION == 0.4f);
    CHECK(AMBIENT_RGB[0] == 0.70f);
    CHECK(AMBIENT_RGB[1] == 0.74f);
    CHECK(AMBIENT_RGB[2] == 0.82f);
    CHECK(AMBIENT_ENERGY == 0.8f);
    CHECK(V3D03_SUN_AZIMUTH_DEG == 315.0f);
    CHECK(V3D03_SUN_ELEVATION_DEG == 40.0f);
    CHECK(V3D03_SUN_ENERGY == 1.0f);
    CHECK(V3D03_AMBIENT_SKY_CONTRIBUTION == 1.0f);
    CHECK(V3D03_AMBIENT_RGB[0] == 0.0f);
    CHECK(V3D03_AMBIENT_ENERGY == 1.0f);
}

TEST_CASE("U39 peak_view") {
    std::string problem;
    const auto layout = layout_for(512, 512, problem);
    REQUIRE(layout.has_value());
    const int n = 512;
    const std::size_t total = 512u * 512u;
    auto at = [](int x, int y) { return static_cast<std::size_t>(y) * 512u + static_cast<std::size_t>(x); };
    std::vector<std::int16_t> e(total, 100);
    GroundCells cells;
    cells.type.assign(total, 1);
    cells.painted.assign(total, 0);
    cells.rock.assign(total, 0);
    cells.snow.assign(total, 0);
    // The highest cell is water, then a painted one: both ignored.
    e[at(5, 5)] = 9000;
    cells.type[at(5, 5)] = 13;
    e[at(6, 6)] = 8000;
    cells.painted[at(6, 6)] = 3;
    e[at(7, 7)] = 7000;
    cells.type[at(7, 7)] = 0;  // unknown: not a land type of 1 to 10
    // Two land cells of the same height: the smaller row wins, then the
    // smaller column.
    e[at(300, 200)] = 5000;
    e[at(100, 200)] = 5000;
    e[at(50, 250)] = 5000;
    PeakView v = peak_view(*layout, e, cells);
    CHECK(v.found);
    CHECK(v.x == 100);
    CHECK(v.y == 200);
    CHECK(v.cam_x == 100);
    CHECK(v.cam_y == 325);
    // Near the south edge the camera row is clamped.
    e[at(40, 500)] = 6000;
    v = peak_view(*layout, e, cells);
    CHECK(v.x == 40);
    CHECK(v.y == 500);
    CHECK(v.cam_y == n - 1);
    // No land: the centre, found false.
    cells.type.assign(total, 14);
    v = peak_view(*layout, e, cells);
    CHECK_FALSE(v.found);
    CHECK(v.x == 256);
    CHECK(v.y == 256);
    CHECK(v.cam_x == 256);
    CHECK(v.cam_y == 256);
    // No ground cells at all.
    v = peak_view(*layout, e, GroundCells{});
    CHECK_FALSE(v.found);
}
