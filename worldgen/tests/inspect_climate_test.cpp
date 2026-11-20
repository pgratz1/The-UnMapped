// The climate parts of report and render: spec P1-03 section 9 (report keys
// temperature_c, rainfall_mm, land_below_0c, rain_contrast; the
// temperature and rainfall palettes), with the P1-00 10.2 interpolation
// rule and the 10.3 number formats, percentile rule and appendix C 6.
//
// Written from the specs only, on fixture worlds (write_fixture_world) and
// hand-made grids; the climate stage is not involved. Expected colours come
// from the spec's stop tables, copied below, by this file's own
// implementation of the interpolation rule.
//
// As revised 2026-11-17 (P1-03 9, P1-00 10.2, 10.3): the palettes are named
// "temperature" (i16 only) and "rainfall" (u16 only); the climate lines are
// printed whenever both layers exist, with or without the hydrology lines,
// and are the last [geography] lines of a world with no later stage; with
// no land each key prints a single n/a.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "db_test_support.hpp"
#include "worldgen/core/error.hpp"
#include "p1_00_geography_reference.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using worldgen::db::Codec;
using worldgen::db::LayerGrid;
using worldgen::inspect::Palette;
using worldgen::inspect::Rgb;
using worldgen::stages::testing::FixtureLayer;

struct Stop {
    std::int64_t t;
    int r, g, b;
};

// P1-03 9, temperature (t in tenths of a degree).
const std::vector<Stop>& temperature_stops() {
    static const std::vector<Stop> s = {{-300, 49, 54, 149}, {-100, 69, 117, 180}, {0, 171, 217, 233},
                                        {50, 255, 255, 191},  {100, 253, 174, 97},  {200, 215, 48, 39},
                                        {300, 165, 0, 38}};
    return s;
}

// P1-03 9, rainfall (t in mm).
const std::vector<Stop>& rainfall_stops() {
    static const std::vector<Stop> s = {{0, 140, 81, 10},     {250, 191, 129, 45},  {500, 223, 194, 125},
                                        {750, 246, 232, 195}, {1000, 199, 234, 229}, {1500, 128, 205, 193},
                                        {2500, 53, 151, 143}, {4000, 1, 102, 94},   {10000, 0, 60, 48}};
    return s;
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) {
        --q;
    }
    return q;
}

// The interpolation rule of P1-00 10.2.
Rgb ramp(const std::vector<Stop>& s, std::int64_t t) {
    auto rgb = [](int r, int g, int b) {
        return Rgb{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b)};
    };
    if (t < s.front().t) {
        return rgb(s.front().r, s.front().g, s.front().b);
    }
    if (t >= s.back().t) {
        return rgb(s.back().r, s.back().g, s.back().b);
    }
    for (std::size_t k = 0; k + 1 < s.size(); ++k) {
        if (s[k].t <= t && t < s[k + 1].t) {
            const std::int64_t d = s[k + 1].t - s[k].t;
            auto ch = [&](int a, int b) {
                return static_cast<int>(a + floor_div(2 * (b - a) * (t - s[k].t) + d, 2 * d));
            };
            return rgb(ch(s[k].r, s[k + 1].r), ch(s[k].g, s[k + 1].g), ch(s[k].b, s[k + 1].b));
        }
    }
    FAIL("unreachable");
    return {};
}

// The palette of a --palette name; fails the test if the name is unknown.
Palette palette_named(std::string_view name) {
    const std::optional<Palette> p = worldgen::inspect::palette_from_name(name);
    REQUIRE_MESSAGE(p.has_value(), "no palette named " << std::string(name));
    return *p;
}

template <typename T>
std::vector<Rgb> pixels(const LayerGrid& g, Palette p) {
    const std::vector<std::uint8_t> buf = worldgen::inspect::render_rgb(g, p, 1);
    std::vector<Rgb> out;
    for (std::size_t i = 0; i + 2 < buf.size(); i += 3) {
        out.push_back(Rgb{buf[i], buf[i + 1], buf[i + 2]});
    }
    return out;
}

std::string report_of(const std::string& path) {
    auto w = worldgen::db::WorldReader::open(path);
    REQUIRE(w.ok());
    auto r = worldgen::inspect::build_report(w.value());
    INFO("report: ", r.ok() ? std::string() : r.error().message);
    REQUIRE(r.ok());
    return r.value();
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t nl = text.find('\n', at);
        out.push_back(text.substr(at, nl - at));
        at = nl + 1;
    }
    return out;
}

// A 6 x 2 world: ocean at cells 0 and 11, ten land cells.
const std::vector<std::int16_t>& six_by_two_elevation() {
    static const std::vector<std::int16_t> e = {-5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, -5};
    return e;
}

}  // namespace

namespace doctest {
template <>
struct StringMaker<Rgb> {
    static String convert(const Rgb& c) {
        return (std::to_string(int{c.r}) + "," + std::to_string(int{c.g}) + "," + std::to_string(int{c.b})).c_str();
    }
};
}  // namespace doctest

// --- Palettes (P1-03 9) ---------------------------------------------------------------

TEST_CASE("P1-03 9: the temperature and rainfall palettes are registered under auto") {
    const std::optional<Palette> temp = worldgen::inspect::palette_from_name("temperature");
    const std::optional<Palette> rain = worldgen::inspect::palette_from_name("rainfall");
    REQUIRE(temp.has_value());
    REQUIRE(rain.has_value());
    CHECK(*temp != *rain);
    CHECK(*temp != Palette::gray);
    CHECK(*rain != Palette::gray);
    CHECK(worldgen::inspect::registered_palette("temperature") == *temp);
    CHECK(worldgen::inspect::registered_palette("rainfall") == *rain);
    CHECK(worldgen::inspect::palette_name(*temp) == "temperature");
    CHECK(worldgen::inspect::palette_name(*rain) == "rainfall");
    const LayerGrid t = p100::grid<std::int16_t>(2, 1, {-8, 50});
    const LayerGrid r = p100::grid<std::uint16_t>(2, 1, {250, 1000});
    CHECK(worldgen::inspect::palette_suits(*temp, t));
    CHECK(worldgen::inspect::palette_suits(*rain, r));
    CHECK(worldgen::inspect::auto_palette("temperature", t) == *temp);
    CHECK(worldgen::inspect::auto_palette("rainfall", r) == *rain);
    // P1-03 9: temperature suits i16 layers only, rainfall u16 layers only;
    // under auto an unsuitable registered palette falls back to gray.
    const LayerGrid u16 = p100::grid<std::uint16_t>(2, 1, {1, 2});
    const LayerGrid i16 = p100::grid<std::int16_t>(2, 1, {1, 2});
    const LayerGrid u8 = p100::grid<std::uint8_t>(2, 1, {1, 2});
    const LayerGrid i32 = p100::grid<std::int32_t>(2, 1, {1, 2});
    const LayerGrid u32 = p100::grid<std::uint32_t>(2, 1, {1, 2});
    CHECK_FALSE(worldgen::inspect::palette_suits(*temp, u16));
    CHECK_FALSE(worldgen::inspect::palette_suits(*temp, u8));
    CHECK_FALSE(worldgen::inspect::palette_suits(*temp, i32));
    CHECK_FALSE(worldgen::inspect::palette_suits(*rain, i16));
    CHECK_FALSE(worldgen::inspect::palette_suits(*rain, u8));
    CHECK_FALSE(worldgen::inspect::palette_suits(*rain, u32));
    CHECK(worldgen::inspect::auto_palette("temperature", u16) == Palette::gray);
    CHECK(worldgen::inspect::auto_palette("rainfall", i16) == Palette::gray);
    CHECK_THROWS_AS(static_cast<void>(worldgen::inspect::render_rgb(u16, *temp, 1)), worldgen::FatalError);
    CHECK_THROWS_AS(static_cast<void>(worldgen::inspect::render_rgb(i16, *rain, 1)), worldgen::FatalError);
}

TEST_CASE("P1-03 9: the temperature ramp, the spec's example 25 -> (213, 236, 212), every stop and between") {
    CHECK(ramp(temperature_stops(), 25) == Rgb{213, 236, 212});
    const Palette p = palette_named("temperature");
    std::vector<std::int16_t> v = {-600, -301, -300, -299, -200, -101, -100, -1, 0, 1, 25, 49, 50,
                                   51,   75,   99,   100,  150,  199,  200,  250, 299, 300, 301, 500};
    for (std::int16_t t = -600; t <= 500; t += 37) {
        v.push_back(t);
    }
    const LayerGrid g = p100::grid<std::int16_t>(static_cast<std::uint32_t>(v.size()), 1, v);
    const std::vector<Rgb> px = pixels<std::int16_t>(g, p);
    REQUIRE(px.size() == v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        CAPTURE(v[i]);
        CHECK(px[i] == ramp(temperature_stops(), v[i]));
    }
    CHECK(px[10] == Rgb{213, 236, 212});  // 25: the spec's worked example
    CHECK(px[0] == Rgb{49, 54, 149});     // below the first stop
    CHECK(px[24] == Rgb{165, 0, 38});     // at or above the last
}

TEST_CASE("P1-03 9: the rainfall ramp at every stop, between stops and above the last") {
    const Palette p = palette_named("rainfall");
    std::vector<std::uint16_t> v = {0, 1, 124, 125, 249, 250, 251, 499, 500, 625, 750, 999, 1000, 1250, 1500,
                                    2000, 2500, 3999, 4000, 7000, 9999, 10000, 10001, 65535};
    for (std::uint32_t t = 0; t <= 12000; t += 333) {
        v.push_back(static_cast<std::uint16_t>(t));
    }
    const LayerGrid g = p100::grid<std::uint16_t>(static_cast<std::uint32_t>(v.size()), 1, v);
    const std::vector<Rgb> px = pixels<std::uint16_t>(g, p);
    REQUIRE(px.size() == v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        CAPTURE(v[i]);
        CHECK(px[i] == ramp(rainfall_stops(), v[i]));
    }
    CHECK(px[0] == Rgb{140, 81, 10});
    CHECK(px.back().r == 0);  // 65535: the last colour (0, 60, 48)
}

// --- Report keys (P1-03 9) -----------------------------------------------------------------

TEST_CASE("P1-03 9: the four climate keys end [geography], computed over land (elevation > 0)") {
    dbtest::TempDir dir("climate-report");
    const std::string out = dir.file("w.db");
    // Land temperatures, sorted: -145 -8 -1 0 3 12 50 70 90 125 (n = 10):
    // p0 = -145, p10 = rank 1 -> -8, p50 = rank 5 -> 12, p90 = rank 9 -> 125,
    // p100 = rank min(9, 10) -> 125. Below 0: 3 of 10.
    // Land rainfall, sorted: 250 251 300 400 640 800 950 1200 2500 10000:
    // p10 = 251, p50 = 800, p90 = 10000; rain_contrast = RHA(10^8 / 251) =
    // 398406 -> 39.8406. The ocean cells' values (-600 and 0) are not counted.
    const std::vector<std::int16_t> temp = {-600, -145, -8, 0, 12, 50, 70, 90, 125, 3, -1, -600};
    const std::vector<std::uint16_t> rain = {0, 250, 300, 800, 1200, 950, 10000, 400, 640, 2500, 251, 0};
    std::vector<FixtureLayer> layers;
    layers.push_back({"elevation", p100::grid(6, 2, six_by_two_elevation()), Codec::delta_varint});
    layers.push_back({"temperature", p100::grid(6, 2, temp), Codec::delta_varint});
    layers.push_back({"rainfall", p100::grid(6, 2, rain), Codec::delta_varint});
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 1, layers).ok());
    const std::vector<std::string> l = lines_of(report_of(out));
    REQUIRE(l.size() >= 4U);
    const std::vector<std::string> tail(l.end() - 4, l.end());
    CHECK(tail == std::vector<std::string>{"temperature_c = -14.5 -0.8 1.2 12.5 12.5",
                                           "rainfall_mm = 250 251 800 10000 10000", "land_below_0c = 0.3000",
                                           "rain_contrast = 39.8406"});
    // They follow the pass keys (no hydrology layers here).
    CHECK(l[l.size() - 5].rfind("passes_walkable = ", 0) == 0);
}

TEST_CASE("P1-03 9, P1-00 10.3: the climate lines come after the hydrology lines") {
    dbtest::TempDir dir("climate-report-hydro");
    const std::string out = dir.file("w.db");
    std::vector<FixtureLayer> layers = p100::worked_example_layers();
    const std::size_t n = p100::WX * p100::WY;
    std::vector<std::int16_t> temp(n);
    std::vector<std::uint16_t> rain(n);
    for (std::size_t i = 0; i < n; ++i) {
        temp[i] = static_cast<std::int16_t>(90 - static_cast<std::int16_t>(i));
        rain[i] = static_cast<std::uint16_t>(250 + 37 * i);
    }
    layers.push_back({"temperature", p100::grid(p100::WX, p100::WY, temp), Codec::delta_varint});
    layers.push_back({"rainfall", p100::grid(p100::WX, p100::WY, rain), Codec::delta_varint});
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 7, layers).ok());
    const std::vector<std::string> l = lines_of(report_of(out));
    REQUIRE(l.size() >= 5U);
    CHECK(l[l.size() - 5].rfind("largest_basin_share = ", 0) == 0);
    CHECK(l[l.size() - 4].rfind("temperature_c = ", 0) == 0);
    CHECK(l[l.size() - 3].rfind("rainfall_mm = ", 0) == 0);
    CHECK(l[l.size() - 2].rfind("land_below_0c = ", 0) == 0);
    CHECK(l[l.size() - 1].rfind("rain_contrast = ", 0) == 0);
    // Recomputed here: land is elevation > 0.
    std::vector<std::int64_t> lt, lr;
    for (std::size_t i = 0; i < n; ++i) {
        if (p100::WORKED_ELEVATION[i] > 0) {
            lt.push_back(temp[i]);
            lr.push_back(rain[i]);
        }
    }
    std::sort(lt.begin(), lt.end());
    std::sort(lr.begin(), lr.end());
    auto pct = [](const std::vector<std::int64_t>& v, std::size_t p) {
        return v[std::min(v.size() - 1, (p * v.size()) / 100)];
    };
    std::string want_r = "rainfall_mm =";
    for (const std::size_t p : {0U, 10U, 50U, 90U, 100U}) {
        want_r += " " + std::to_string(pct(lr, p));
    }
    CHECK(l[l.size() - 3] == want_r);
    std::string want_t = "temperature_c =";
    for (const std::size_t p : {0U, 10U, 50U, 90U, 100U}) {
        want_t += " " + worldgen::inspect::metres1(pct(lt, p));
    }
    CHECK(l[l.size() - 4] == want_t);
}

TEST_CASE("P1-03 9, P1-00 appendix C 6: no land gives n/a; p10 = 0 gives rain_contrast n/a") {
    dbtest::TempDir dir("climate-report-empty");
    {
        const std::string out = dir.file("ocean.db");
        std::vector<FixtureLayer> layers;
        layers.push_back({"elevation", p100::grid<std::int16_t>(3, 1, {-5, -6, -7}), Codec::delta_varint});
        layers.push_back({"temperature", p100::grid<std::int16_t>(3, 1, {10, 20, 30}), Codec::delta_varint});
        layers.push_back({"rainfall", p100::grid<std::uint16_t>(3, 1, {300, 400, 500}), Codec::delta_varint});
        REQUIRE(worldgen::stages::testing::write_fixture_world(out, 1, layers).ok());
        const std::vector<std::string> l = lines_of(report_of(out));
        const std::vector<std::string> tail(l.end() - 4, l.end());
        CHECK(tail == std::vector<std::string>{"temperature_c = n/a", "rainfall_mm = n/a", "land_below_0c = n/a",
                                               "rain_contrast = n/a"});
    }
    {
        const std::string out = dir.file("dry.db");
        std::vector<FixtureLayer> layers;
        layers.push_back({"elevation", p100::grid(6, 2, six_by_two_elevation()), Codec::delta_varint});
        layers.push_back(
            {"temperature", p100::grid<std::int16_t>(6, 2, {0, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0}), Codec::delta_varint});
        layers.push_back({"rainfall", p100::grid<std::uint16_t>(6, 2, {9, 0, 0, 7, 7, 7, 7, 7, 7, 7, 7, 9}),
                          Codec::delta_varint});
        REQUIRE(worldgen::stages::testing::write_fixture_world(out, 1, layers).ok());
        const std::vector<std::string> l = lines_of(report_of(out));
        const std::vector<std::string> tail(l.end() - 4, l.end());
        CHECK(tail == std::vector<std::string>{"temperature_c = 0.5 0.5 0.5 0.5 0.5", "rainfall_mm = 0 0 7 7 7",
                                               "land_below_0c = 0.0000", "rain_contrast = n/a"});
    }
}

TEST_CASE("P1-03 9: without both climate layers there are no climate lines") {
    dbtest::TempDir dir("climate-report-partial");
    for (const char* only : {"temperature", "rainfall", "neither"}) {
        CAPTURE(std::string_view(only));
        const std::string out = dir.file(std::string(only) + ".db");
        std::vector<FixtureLayer> layers;
        layers.push_back({"elevation", p100::grid(6, 2, six_by_two_elevation()), Codec::delta_varint});
        if (std::string_view(only) == "temperature") {
            layers.push_back({"temperature", p100::grid<std::int16_t>(6, 2, std::vector<std::int16_t>(12, 5)),
                              Codec::delta_varint});
        } else if (std::string_view(only) == "rainfall") {
            layers.push_back({"rainfall", p100::grid<std::uint16_t>(6, 2, std::vector<std::uint16_t>(12, 500)),
                              Codec::delta_varint});
        }
        REQUIRE(worldgen::stages::testing::write_fixture_world(out, 1, layers).ok());
        const std::string r = report_of(out);
        for (const char* key : {"temperature_c", "rainfall_mm", "land_below_0c", "rain_contrast"}) {
            CAPTURE(std::string_view(key));
            CHECK(r.find(std::string("\n") + key + " = ") == std::string::npos);
        }
    }
}
