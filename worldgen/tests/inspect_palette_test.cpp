// The P1 render palettes, spec P1-00 section 10.2 with appendix C items 1
// to 3, and P0-04 amendment 16: the interpolation rule, every palette's
// colours, which element types each palette suits, the registered palettes
// and auto, render_rgb with the palettes, the render_rgb overload that fills
// a caller's buffer, and codec_name.
//
// Expected colours are computed here from the spec's tables, copied below
// from the spec text, by a reference implementation of the interpolation
// rule written for this file; the library's own stop tables are only
// compared against the spec's.

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "p0_04_reference.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/inspect/render.hpp"

namespace {

using worldgen::FatalError;
using worldgen::db::Codec;
using worldgen::db::ElementType;
using worldgen::db::LayerGrid;
using worldgen::inspect::Palette;
using worldgen::inspect::RampStop;
using worldgen::inspect::Rgb;
using Pixels = std::vector<std::uint8_t>;

constexpr std::uint64_t U64_MAX = std::numeric_limits<std::uint64_t>::max();
constexpr std::int64_t I64_MIN = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t I64_MAX = std::numeric_limits<std::int64_t>::max();
constexpr Rgb MAGENTA{255, 0, 255};

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

// --- Reference implementation of the interpolation rule (P1-00 10.2) -------

// floor(n / d) for d > 0, n of any sign.
std::int64_t floor_div(std::int64_t n, std::int64_t d) {
    std::int64_t q = n / d;
    if (n % d != 0 && n < 0) {
        --q;
    }
    return q;
}

struct Stop {
    std::int64_t t;
    int r;
    int g;
    int b;
};

int ref_channel(int c0, int c1, std::int64_t t0, std::int64_t t1, std::int64_t t) {
    const std::int64_t dt = t1 - t0;
    return c0 + static_cast<int>(floor_div(2 * (c1 - c0) * (t - t0) + dt, 2 * dt));
}

Rgb rgb(int r, int g, int b) {
    return Rgb{static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b)};
}

Rgb ref_ramp(const std::vector<Stop>& stops, std::int64_t t) {
    if (t < stops.front().t) {
        return rgb(stops.front().r, stops.front().g, stops.front().b);
    }
    if (t >= stops.back().t) {
        return rgb(stops.back().r, stops.back().g, stops.back().b);
    }
    for (std::size_t k = 0; k + 1 < stops.size(); ++k) {
        const Stop& a = stops[k];
        const Stop& b = stops[k + 1];
        if (a.t <= t && t < b.t) {
            return rgb(ref_channel(a.r, b.r, a.t, b.t, t), ref_channel(a.g, b.g, a.t, b.t, t),
                       ref_channel(a.b, b.b, a.t, b.t, t));
        }
    }
    FAIL("unreachable");
    return {};
}

std::vector<RampStop> to_ramp(const std::vector<Stop>& stops) {
    std::vector<RampStop> out;
    for (const Stop& s : stops) {
        out.push_back(RampStop{s.t, rgb(s.r, s.g, s.b)});
    }
    return out;
}

// The hypsometric tables of P1-00 10.2, copied from the spec text.
const std::vector<Stop> LAND = {
    {0, 70, 120, 64},       {500, 104, 146, 78},    {2000, 150, 168, 96},   {5000, 178, 160, 108},
    {10000, 150, 120, 86},  {15000, 128, 106, 92},  {20000, 196, 192, 188}, {30000, 250, 250, 250},
};
const std::vector<Stop> OCEAN = {
    {0, 120, 170, 205}, {200, 88, 140, 190}, {1000, 56, 104, 168}, {5000, 32, 66, 130}, {30000, 16, 34, 82},
};

Rgb ref_hypsometric(std::int64_t e) {
    // Land e > 0 on t = e; ocean (and e = 0, appendix C item 3) on t = -e.
    return e > 0 ? ref_ramp(LAND, e) : ref_ramp(OCEAN, -e);
}

// The categorical tables of P1-00 10.2, copied from the spec text.
const std::array<Rgb, 4> WATER = {{{214, 200, 160}, {52, 100, 200}, {64, 128, 204}, {24, 56, 120}}};
const std::array<Rgb, 16> RIVER = {{{16, 16, 16},
                                    {120, 170, 230},
                                    {100, 150, 225},
                                    {80, 130, 220},
                                    {60, 110, 210},
                                    {45, 90, 200},
                                    {30, 70, 190},
                                    {30, 70, 190},
                                    {30, 70, 190},
                                    {30, 70, 190},
                                    {30, 70, 190},
                                    {30, 70, 190},
                                    {30, 70, 190},
                                    {30, 70, 190},
                                    {30, 70, 190},
                                    {30, 70, 190}}};
const std::array<Rgb, 9> DIRECTION = {{{230, 25, 75},
                                       {245, 130, 48},
                                       {255, 225, 25},
                                       {210, 245, 60},
                                       {60, 180, 75},
                                       {70, 240, 240},
                                       {0, 130, 200},
                                       {145, 30, 180},
                                       {0, 0, 0}}};

// bit_length (P1-00 amendment A1): 0 for 0, else floor(log2 v) + 1.
unsigned bit_length(std::uint64_t v) {
    unsigned n = 0;
    while (v != 0) {
        ++n;
        v >>= 1U;
    }
    return n;
}

Rgb ref_log2(std::uint64_t v) {
    const unsigned g = std::min(255U, 11U * bit_length(v));
    return rgb(static_cast<int>(g), static_cast<int>(g), static_cast<int>(g));
}

Rgb ref_ids(std::uint64_t v) {
    if (v == 0) {
        return Rgb{0, 0, 0};
    }
    const std::uint64_t h = worldgen::mix64(v);
    auto ch = [&](unsigned j) { return 64 + static_cast<int>(((h >> (8U * j)) & 0xFFU) * 191U / 255U); };
    return rgb(ch(0), ch(1), ch(2));
}

Rgb pixel_at(const Pixels& px, std::size_t image_width, std::size_t x, std::size_t y) {
    const std::size_t o = (y * image_width + x) * 3;
    return Rgb{px.at(o), px.at(o + 1), px.at(o + 2)};
}

// A 2 x 2 zero grid of each element type.
LayerGrid zero_grid(ElementType t) { return worldgen::db::make_layer_grid(t, 2, 2); }

constexpr std::array<ElementType, 8> ALL_TYPES = {ElementType::u8,  ElementType::i8,  ElementType::u16,
                                                  ElementType::i16, ElementType::u32, ElementType::i32,
                                                  ElementType::u64, ElementType::i64};

bool is_unsigned(ElementType t) {
    return t == ElementType::u8 || t == ElementType::u16 || t == ElementType::u32 || t == ElementType::u64;
}

}  // namespace

// --- Interpolation rule ---------------------------------------------------------

TEST_CASE("palette 10.2: the spec's worked example e = 250") {
    // R = 70 + floor((2*34*250 + 500)/1000) = 87; G = 120 + 13 = 133;
    // B = 64 + 7 = 71.
    CHECK(worldgen::inspect::hypsometric_land_colour(250) == Rgb{87, 133, 71});
    CHECK(worldgen::inspect::hypsometric_colour(250) == Rgb{87, 133, 71});
    CHECK(ref_hypsometric(250) == Rgb{87, 133, 71});
}

TEST_CASE("palette 10.2: rounding half up with floor division, rising and falling channels") {
    // One channel per case; the other two are constant at 100.
    auto one = [](int c0, int c1, std::int64_t t1, std::int64_t t) {
        const std::vector<RampStop> stops = {{0, rgb(c0, 100, 100)}, {t1, rgb(c1, 100, 100)}};
        return int{worldgen::inspect::ramp_colour(stops, t).r};
    };
    // Rising: +0.5 -> +1 (half up), +0.4 -> 0, +0.6 -> +1.
    CHECK(one(10, 11, 2, 1) == 11);  // floor((2 + 2) / 4) = 1
    CHECK(one(10, 11, 5, 2) == 10);  // 0.4: floor((4 + 5) / 10) = 0
    CHECK(one(10, 11, 5, 3) == 11);  // 0.6: floor((6 + 5) / 10) = 1
    CHECK(one(10, 13, 2, 1) == 12);  // 1.5 -> 2
    // Falling: -0.5 -> 0 (half up, towards +inf), -1.5 -> -1, -0.4 -> 0,
    // -0.6 -> -1. Truncating division would give 0 for -0.1 and -0.6.
    CHECK(one(10, 9, 2, 1) == 10);  // floor((-2 + 2) / 4) = 0
    CHECK(one(10, 7, 2, 1) == 9);   // floor((-6 + 2) / 4) = -1
    CHECK(one(10, 9, 5, 2) == 10);  // -0.4: floor((-4 + 5) / 10) = 0
    CHECK(one(10, 9, 5, 3) == 9);   // -0.6: floor((-6 + 5) / 10) = floor(-0.1) = -1
    CHECK(one(255, 0, 255, 1) == 254);
    CHECK(one(255, 0, 510, 1) == 255);  // -0.5 -> 0
    CHECK(one(255, 0, 510, 3) == 254);  // -1.5 -> -1
    CHECK(one(0, 255, 510, 1) == 1);    // +0.5 -> 1

    // A falling channel in the land table: G 168 -> 160 between 2000 and
    // 5000. t = 2000 + 187: -8 * 187 / 3000 = -0.4987 -> 0 -> 168;
    // t = 2000 + 188: -0.5013 -> -1 -> 167 (floor((-3008 + 3000) / 6000) = -1).
    CHECK(int{worldgen::inspect::hypsometric_land_colour(2187).g} == 168);
    CHECK(int{worldgen::inspect::hypsometric_land_colour(2188).g} == 167);
    // R 178 -> 150 between 5000 and 10000 at t = 7500: -14 exactly -> 164.
    CHECK(int{worldgen::inspect::hypsometric_land_colour(7500).r} == 164);
    // Ocean R 120 -> 88 over 0..200, t = 1: -0.16 -> 0 -> 120; t = 3: -0.48
    // -> 0; t = 4: -0.64 -> -1 -> 119.
    CHECK(int{worldgen::inspect::hypsometric_ocean_colour(1).r} == 120);
    CHECK(int{worldgen::inspect::hypsometric_ocean_colour(3).r} == 120);
    CHECK(int{worldgen::inspect::hypsometric_ocean_colour(4).r} == 119);
}

TEST_CASE("palette 10.2: ramp_colour on general stops") {
    const std::vector<Stop> s = {{-100, 0, 255, 10}, {-50, 255, 0, 10}, {0, 0, 0, 200}, {7, 30, 30, 30}};
    const std::vector<RampStop> stops = to_ramp(s);
    for (std::int64_t t = -130; t <= 20; ++t) {
        INFO("t = ", t);
        CHECK(worldgen::inspect::ramp_colour(stops, t) == ref_ramp(s, t));
    }
    // Below the first stop and at or above the last.
    CHECK(worldgen::inspect::ramp_colour(stops, I64_MIN) == rgb(0, 255, 10));
    CHECK(worldgen::inspect::ramp_colour(stops, -101) == rgb(0, 255, 10));
    CHECK(worldgen::inspect::ramp_colour(stops, -100) == rgb(0, 255, 10));
    CHECK(worldgen::inspect::ramp_colour(stops, 7) == rgb(30, 30, 30));
    CHECK(worldgen::inspect::ramp_colour(stops, I64_MAX) == rgb(30, 30, 30));
    // One stop: its colour everywhere.
    const std::vector<RampStop> single = {{5, rgb(1, 2, 3)}};
    CHECK(worldgen::inspect::ramp_colour(single, 4) == rgb(1, 2, 3));
    CHECK(worldgen::inspect::ramp_colour(single, 5) == rgb(1, 2, 3));
    CHECK(worldgen::inspect::ramp_colour(single, 6) == rgb(1, 2, 3));
    // A gap just under 2^40 is allowed (the numerator 2 * 255 * (t - t_k)
    // stays in int64); checked against the reference at its ends and middle.
    const std::int64_t gap = (std::int64_t{1} << 40) - 1;
    const std::vector<Stop> wide = {{0, 255, 0, 128}, {gap, 0, 255, 128}};
    for (const std::int64_t t : {std::int64_t{0}, std::int64_t{1}, gap / 2, gap / 2 + 1, gap - 1, gap}) {
        INFO("t = ", t);
        CHECK(worldgen::inspect::ramp_colour(to_ramp(wide), t) == ref_ramp(wide, t));
    }
}

TEST_CASE("palette 10.2: ramp_colour contract violations") {
    CHECK_THROWS_AS((void)worldgen::inspect::ramp_colour(std::span<const RampStop>{}, 0), FatalError);
    const std::vector<RampStop> equal = {{0, rgb(0, 0, 0)}, {0, rgb(1, 1, 1)}};
    CHECK_THROWS_AS((void)worldgen::inspect::ramp_colour(equal, 0), FatalError);
    const std::vector<RampStop> descending = {{5, rgb(0, 0, 0)}, {1, rgb(1, 1, 1)}};
    CHECK_THROWS_AS((void)worldgen::inspect::ramp_colour(descending, 3), FatalError);
    const std::vector<RampStop> too_wide = {{0, rgb(0, 0, 0)}, {std::int64_t{1} << 40, rgb(1, 1, 1)}};
    CHECK_THROWS_AS((void)worldgen::inspect::ramp_colour(too_wide, 1), FatalError);
}

// --- Hypsometric ------------------------------------------------------------------

TEST_CASE("palette 10.2: the library's hypsometric stops are the spec's tables") {
    REQUIRE(worldgen::inspect::HYPSOMETRIC_LAND_STOPS.size() == LAND.size());
    for (std::size_t k = 0; k < LAND.size(); ++k) {
        CHECK(worldgen::inspect::HYPSOMETRIC_LAND_STOPS[k].t == LAND[k].t);
        CHECK(worldgen::inspect::HYPSOMETRIC_LAND_STOPS[k].colour == rgb(LAND[k].r, LAND[k].g, LAND[k].b));
    }
    REQUIRE(worldgen::inspect::HYPSOMETRIC_OCEAN_STOPS.size() == OCEAN.size());
    for (std::size_t k = 0; k < OCEAN.size(); ++k) {
        CHECK(worldgen::inspect::HYPSOMETRIC_OCEAN_STOPS[k].t == OCEAN[k].t);
        CHECK(worldgen::inspect::HYPSOMETRIC_OCEAN_STOPS[k].colour == rgb(OCEAN[k].r, OCEAN[k].g, OCEAN[k].b));
    }
}

TEST_CASE("palette 10.2: hypsometric at each stop, between stops, below the first, at or above the last") {
    for (const Stop& s : LAND) {
        INFO("land stop ", s.t);
        CHECK(worldgen::inspect::hypsometric_land_colour(s.t) == rgb(s.r, s.g, s.b));
        if (s.t > 0) {
            CHECK(worldgen::inspect::hypsometric_colour(s.t) == rgb(s.r, s.g, s.b));
        }
    }
    for (const Stop& s : OCEAN) {
        INFO("ocean stop ", s.t);
        CHECK(worldgen::inspect::hypsometric_ocean_colour(s.t) == rgb(s.r, s.g, s.b));
        CHECK(worldgen::inspect::hypsometric_colour(-s.t) == rgb(s.r, s.g, s.b));
    }
    // Between stops: midpoints and one step either side of every stop.
    for (std::size_t k = 0; k + 1 < LAND.size(); ++k) {
        for (const std::int64_t t : {LAND[k].t + 1, (LAND[k].t + LAND[k + 1].t) / 2, LAND[k + 1].t - 1}) {
            INFO("land t ", t);
            CHECK(worldgen::inspect::hypsometric_land_colour(t) == ref_ramp(LAND, t));
        }
    }
    for (std::size_t k = 0; k + 1 < OCEAN.size(); ++k) {
        for (const std::int64_t t : {OCEAN[k].t + 1, (OCEAN[k].t + OCEAN[k + 1].t) / 2, OCEAN[k + 1].t - 1}) {
            INFO("ocean t ", t);
            CHECK(worldgen::inspect::hypsometric_ocean_colour(t) == ref_ramp(OCEAN, t));
        }
    }
    // Midpoint values worked by hand: land t = 1250 between 500 and 2000:
    // R 104 + floor((2*46*750 + 1500)/3000) = 104 + 23 = 127; G 146 + 11 =
    // 157; B 78 + 9 = 87.
    CHECK(worldgen::inspect::hypsometric_land_colour(1250) == Rgb{127, 157, 87});
    // Ocean t = 3000 between 1000 and 5000: R 56 - 12 = 44, G 104 - 19 = 85,
    // B 168 - 19 = 149.
    CHECK(worldgen::inspect::hypsometric_ocean_colour(3000) == Rgb{44, 85, 149});
    // Below the first stop: the first colour.
    CHECK(worldgen::inspect::hypsometric_land_colour(-1) == Rgb{70, 120, 64});
    CHECK(worldgen::inspect::hypsometric_land_colour(I64_MIN) == Rgb{70, 120, 64});
    CHECK(worldgen::inspect::hypsometric_ocean_colour(-1) == Rgb{120, 170, 205});
    // At or above the last stop: the last colour.
    CHECK(worldgen::inspect::hypsometric_land_colour(30000) == Rgb{250, 250, 250});
    CHECK(worldgen::inspect::hypsometric_land_colour(32767) == Rgb{250, 250, 250});
    CHECK(worldgen::inspect::hypsometric_land_colour(I64_MAX) == Rgb{250, 250, 250});
    CHECK(worldgen::inspect::hypsometric_ocean_colour(30000) == Rgb{16, 34, 82});
    CHECK(worldgen::inspect::hypsometric_ocean_colour(32768) == Rgb{16, 34, 82});
    CHECK(worldgen::inspect::hypsometric_colour(-32768) == Rgb{16, 34, 82});
    // The header accepts any int64: depth -INT64_MIN = 2^63 is past the
    // last ocean stop.
    CHECK(worldgen::inspect::hypsometric_colour(I64_MIN) == Rgb{16, 34, 82});
    CHECK(worldgen::inspect::hypsometric_colour(I64_MAX) == Rgb{250, 250, 250});
}

TEST_CASE("palette 10.2, appendix C 3: hypsometric on land, on ocean and at elevation 0") {
    // e = 0 is ocean at depth 0 (appendix C item 3), not the land colour of
    // t = 0 (which differs: (70, 120, 64)).
    CHECK(worldgen::inspect::hypsometric_colour(0) == Rgb{120, 170, 205});
    // e = 1: land at t = 1: R 70 + floor((68 + 500)/1000) = 70, G 120, B 64.
    CHECK(worldgen::inspect::hypsometric_colour(1) == Rgb{70, 120, 64});
    // e = -1: ocean at depth 1: R 120 + floor((-64 + 200)/400) = 120,
    // G 170 + floor((-60 + 200)/400) = 170, B 205 + floor((-30+200)/400) = 205.
    CHECK(worldgen::inspect::hypsometric_colour(-1) == Rgb{120, 170, 205});
    // Every i16 value against the reference.
    for (std::int64_t e = -32768; e <= 32767; ++e) {
        if (worldgen::inspect::hypsometric_colour(e) != ref_hypsometric(e)) {
            INFO("e = ", e);
            CHECK(worldgen::inspect::hypsometric_colour(e) == ref_hypsometric(e));
        }
    }
}

// --- Categorical palettes -----------------------------------------------------------

TEST_CASE("palette 10.2, appendix C 2: water, river and direction for every value") {
    for (std::uint64_t v = 0; v < 4; ++v) {
        CHECK(worldgen::inspect::water_colour(v) == WATER[v]);
    }
    for (const std::uint64_t v : {std::uint64_t{4}, std::uint64_t{5}, std::uint64_t{255}, std::uint64_t{256},
                                  std::uint64_t{1} << 32, U64_MAX}) {
        INFO("water ", v);
        CHECK(worldgen::inspect::water_colour(v) == MAGENTA);
    }
    for (std::uint64_t v = 0; v < 16; ++v) {
        INFO("river ", v);
        CHECK(worldgen::inspect::river_colour(v) == RIVER[v]);
    }
    for (const std::uint64_t v : {std::uint64_t{16}, std::uint64_t{17}, std::uint64_t{255}, U64_MAX}) {
        INFO("river ", v);
        CHECK(worldgen::inspect::river_colour(v) == MAGENTA);
    }
    for (std::uint64_t v = 0; v < 9; ++v) {
        INFO("direction ", v);
        CHECK(worldgen::inspect::direction_colour(v) == DIRECTION[v]);
    }
    for (const std::uint64_t v : {std::uint64_t{9}, std::uint64_t{10}, std::uint64_t{255}, U64_MAX}) {
        INFO("direction ", v);
        CHECK(worldgen::inspect::direction_colour(v) == Rgb{255, 255, 255});
    }
}

TEST_CASE("palette 10.2: log2 at every bit-length boundary") {
    CHECK(worldgen::inspect::log2_colour(0) == Rgb{0, 0, 0});
    CHECK(worldgen::inspect::log2_colour(1) == Rgb{11, 11, 11});
    CHECK(worldgen::inspect::log2_colour(2) == Rgb{22, 22, 22});
    CHECK(worldgen::inspect::log2_colour(3) == Rgb{22, 22, 22});
    // bit_length 23 (2^22 .. 2^23 - 1): 253; 24 and above: min(255, 264+) = 255.
    CHECK(worldgen::inspect::log2_colour((std::uint64_t{1} << 23) - 1) == Rgb{253, 253, 253});
    CHECK(worldgen::inspect::log2_colour(std::uint64_t{1} << 23) == Rgb{255, 255, 255});
    CHECK(worldgen::inspect::log2_colour(U64_MAX) == Rgb{255, 255, 255});
    for (unsigned k = 0; k < 64; ++k) {
        const std::uint64_t p = std::uint64_t{1} << k;
        INFO("2^", k);
        CHECK(worldgen::inspect::log2_colour(p) == ref_log2(p));
        CHECK(worldgen::inspect::log2_colour(p - 1) == ref_log2(p - 1));
        CHECK(worldgen::inspect::log2_colour(p + 1) == ref_log2(p + 1));
    }
}

TEST_CASE("palette 10.2: ids") {
    CHECK(worldgen::inspect::ids_colour(0) == Rgb{0, 0, 0});
    for (std::uint64_t v = 1; v <= 2000; ++v) {
        if (worldgen::inspect::ids_colour(v) != ref_ids(v)) {
            INFO("v = ", v);
            CHECK(worldgen::inspect::ids_colour(v) == ref_ids(v));
        }
    }
    for (const std::uint64_t v : {std::uint64_t{0xFFFFFFFFU}, std::uint64_t{1} << 32, U64_MAX - 1, U64_MAX}) {
        INFO("v = ", v);
        CHECK(worldgen::inspect::ids_colour(v) == ref_ids(v));
    }
    // Every channel lies in 64..255, and the extremes of the byte map to
    // them: byte 0 -> 64, byte 255 -> 64 + 191 = 255. Find ids whose hash has
    // a 0x00 and a 0xFF byte in some channel and check them.
    bool seen_low = false;
    bool seen_high = false;
    for (std::uint64_t v = 1; v <= 200000 && !(seen_low && seen_high); ++v) {
        const std::uint64_t h = worldgen::mix64(v);
        const Rgb c = worldgen::inspect::ids_colour(v);
        const std::array<std::uint8_t, 3> ch = {c.r, c.g, c.b};
        for (unsigned j = 0; j < 3; ++j) {
            const auto byte = static_cast<unsigned>((h >> (8U * j)) & 0xFFU);
            if (byte == 0U) {
                seen_low = true;
                CHECK(int{ch[j]} == 64);
            }
            if (byte == 255U) {
                seen_high = true;
                CHECK(int{ch[j]} == 255);
            }
        }
    }
    CHECK(seen_low);
    CHECK(seen_high);
}

TEST_CASE("palette 10.2: signed at its boundaries") {
    using worldgen::inspect::signed_colour;
    using worldgen::inspect::signed_scale;
    using worldgen::inspect::SignedScale;
    // Scale: P = max(1, largest positive), M = max(1, -smallest negative).
    {
        const auto s = signed_scale(p004::grid_of<std::int16_t>(3, 1, {-48, 0, 160}));
        CHECK(s.p == 160U);
        CHECK(s.m == 48U);
    }
    {
        const auto s = signed_scale(p004::grid_of<std::int16_t>(2, 1, {0, 0}));
        CHECK(s.p == 1U);
        CHECK(s.m == 1U);
    }
    {
        const auto s = signed_scale(p004::grid_of<std::int16_t>(2, 1, {-5, -1}));
        CHECK(s.p == 1U);
        CHECK(s.m == 5U);
    }
    {
        const auto s = signed_scale(p004::grid_of<std::int16_t>(2, 1, {3, 9}));
        CHECK(s.p == 9U);
        CHECK(s.m == 1U);
    }
    {
        const auto s = signed_scale(p004::grid_of<std::int8_t>(2, 1, {-128, 127}));
        CHECK(s.p == 127U);
        CHECK(s.m == 128U);
    }
    {
        const auto s = signed_scale(p004::grid_of<std::int64_t>(2, 1, {I64_MIN, I64_MAX}));
        CHECK(s.p == static_cast<std::uint64_t>(I64_MAX));
        CHECK(s.m == std::uint64_t{1} << 63);
    }
    CHECK_THROWS_AS((void)signed_scale(p004::grid_of<std::uint16_t>(1, 1, {1})), FatalError);
    CHECK_THROWS_AS((void)signed_scale(p004::grid_of<std::int16_t>(2, 2, {1})), FatalError);

    // Colours with P = 160, M = 48.
    const SignedScale s{160, 48};
    CHECK(signed_colour(0, s) == Rgb{0, 0, 0});
    CHECK(signed_colour(160, s) == Rgb{255, 255, 255});
    // v = 80: gray floor((510*80 + 160)/320) = floor(128) = 128.
    CHECK(signed_colour(80, s) == Rgb{128, 128, 128});
    // v = 1: floor((510 + 160)/320) = 2.
    CHECK(signed_colour(1, s) == Rgb{2, 2, 2});
    // v = -48: gray 255, blue 255 - floor(765/4) = 255 - 191 = 64.
    CHECK(signed_colour(-48, s) == Rgb{0, 0, 64});
    // v = -24: gray floor((510*24 + 48)/96) = 128, blue 255 - 96 = 159.
    CHECK(signed_colour(-24, s) == Rgb{0, 0, 159});
    // v = -1: gray floor((510 + 48)/96) = 5, blue 255 - floor(15/4) = 252.
    CHECK(signed_colour(-1, s) == Rgb{0, 0, 252});
    for (std::int64_t v = -48; v <= 160; ++v) {
        INFO("v = ", v);
        const Rgb c = signed_colour(v, s);
        if (v >= 0) {
            const std::uint8_t g = worldgen::inspect::gray_level(static_cast<std::uint64_t>(v), 160);
            CHECK(c == Rgb{g, g, g});
        } else {
            const unsigned g = worldgen::inspect::gray_level(static_cast<std::uint64_t>(-v), 48);
            CHECK(c == rgb(0, 0, 255 - static_cast<int>(3U * g / 4U)));
        }
    }
    // Out of the scale is a contract violation.
    CHECK_THROWS_AS((void)signed_colour(161, s), FatalError);
    CHECK_THROWS_AS((void)signed_colour(-49, s), FatalError);
    // Widest type: P = 2^63 - 1, M = 2^63.
    const SignedScale w{static_cast<std::uint64_t>(I64_MAX), std::uint64_t{1} << 63};
    CHECK(signed_colour(I64_MAX, w) == Rgb{255, 255, 255});
    CHECK(signed_colour(I64_MIN, w) == Rgb{0, 0, 64});
    CHECK(signed_colour(0, w) == Rgb{0, 0, 0});
    // -1 over M = 2^63: gray floor((510 + 2^63) / 2^64) = 0, blue 255.
    CHECK(signed_colour(-1, w) == Rgb{0, 0, 255});
    // -2^62 over M = 2^63: gray floor((510 * 2^62 + 2^63) / 2^64) =
    // floor(127.5 + 0.5) = 128, blue 255 - 96 = 159.
    CHECK(signed_colour(-(std::int64_t{1} << 62), w) == Rgb{0, 0, 159});
}

// --- Which element types a palette suits (appendix C item 1) ---------------------------

TEST_CASE("palette appendix C 1: palette_suits for every palette and element type") {
    using worldgen::inspect::palette_suits;
    for (const ElementType t : ALL_TYPES) {
        const LayerGrid g = zero_grid(t);
        INFO("type ", worldgen::db::element_type_name(t));
        CHECK(palette_suits(Palette::gray, g));
        CHECK(palette_suits(Palette::hypsometric, g) == (t == ElementType::i16));
        CHECK(palette_suits(Palette::water, g) == (t == ElementType::u8));
        CHECK(palette_suits(Palette::direction, g) == (t == ElementType::u8));
        CHECK(palette_suits(Palette::river, g) == (t == ElementType::u8));
        CHECK(palette_suits(Palette::log2, g) == is_unsigned(t));
        CHECK(palette_suits(Palette::ids, g) == is_unsigned(t));
        CHECK(palette_suits(Palette::signed_distance, g) == !is_unsigned(t));
    }
    // river: every value at most 15.
    CHECK(palette_suits(Palette::river, p004::grid_of<std::uint8_t>(2, 1, {0, 15})));
    CHECK(!palette_suits(Palette::river, p004::grid_of<std::uint8_t>(2, 1, {0, 16})));
    CHECK(!palette_suits(Palette::river, p004::grid_of<std::uint8_t>(2, 1, {255, 1})));
    // water and direction: values outside the range still suit (they have
    // their own colour, appendix C item 2 and P1-00 10.2).
    CHECK(palette_suits(Palette::water, p004::grid_of<std::uint8_t>(2, 1, {4, 255})));
    CHECK(palette_suits(Palette::direction, p004::grid_of<std::uint8_t>(2, 1, {9, 255})));
    // An invalid grid suits nothing.
    for (const Palette p : worldgen::inspect::ALL_PALETTES) {
        INFO("palette ", worldgen::inspect::palette_name(p));
        CHECK(!palette_suits(p, p004::grid_of<std::uint8_t>(2, 2, {0, 0, 0})));
        CHECK(!palette_suits(p, p004::grid_of<std::int16_t>(0, 1, {})));
    }
}

TEST_CASE("palette appendix C 1: palette_requirement says what the palette needs") {
    using worldgen::inspect::palette_requirement;
    auto has = [](std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; };
    for (const Palette p : worldgen::inspect::ALL_PALETTES) {
        CHECK(!palette_requirement(p).empty());
    }
    CHECK(has(palette_requirement(Palette::hypsometric), "i16"));
    CHECK(has(palette_requirement(Palette::water), "u8"));
    CHECK(has(palette_requirement(Palette::direction), "u8"));
    CHECK(has(palette_requirement(Palette::river), "u8"));
    CHECK(has(palette_requirement(Palette::river), "15"));
    CHECK(has(palette_requirement(Palette::log2), "unsigned"));
    CHECK(has(palette_requirement(Palette::ids), "unsigned"));
    CHECK(has(palette_requirement(Palette::signed_distance), "signed"));
}

TEST_CASE("palette 10.2: names, registered palettes and auto") {
    using worldgen::inspect::auto_palette;
    using worldgen::inspect::palette_from_name;
    using worldgen::inspect::palette_name;
    using worldgen::inspect::registered_palette;
    const std::array<std::string_view, 11> names = {"gray", "hypsometric", "water",  "river",       "log2",    "direction",
                                                    "ids",  "signed",      "temperature", "rainfall", "biome"};
    for (std::size_t k = 0; k < names.size(); ++k) {
        const Palette p = worldgen::inspect::ALL_PALETTES[k];
        CHECK(palette_name(p) == names[k]);
        CHECK(palette_from_name(names[k]) == std::optional<Palette>(p));
    }
    for (const char* bad : {"auto", "biomes", "Hypsometric", "signed_distance", " gray", ""}) {
        INFO("name '", bad, "'");
        CHECK(!palette_from_name(bad).has_value());
    }

    // The seven registered palettes (P1-00 10.2) and gray for anything else.
    CHECK(registered_palette("elevation") == Palette::hypsometric);
    CHECK(registered_palette("water") == Palette::water);
    CHECK(registered_palette("river") == Palette::river);
    CHECK(registered_palette("flow_accum") == Palette::log2);
    CHECK(registered_palette("flow_dir") == Palette::direction);
    CHECK(registered_palette("lake") == Palette::ids);
    CHECK(registered_palette("coast_dist") == Palette::signed_distance);
    CHECK(registered_palette("biome") == Palette::biome);  // P1-04 9
    for (const char* other : {"water_depth", "test.ramp", "test.noise", "Elevation", "elevation ", "region",
                              "biomes", "Temperature", ""}) {
        INFO("layer '", other, "'");
        CHECK(registered_palette(other) == Palette::gray);
    }

    // auto: the registered palette when it suits, otherwise gray.
    using worldgen::db::make_layer_grid;
    CHECK(auto_palette("elevation", make_layer_grid(ElementType::i16, 2, 2)) == Palette::hypsometric);
    CHECK(auto_palette("elevation", make_layer_grid(ElementType::u16, 2, 2)) == Palette::gray);
    CHECK(auto_palette("elevation", make_layer_grid(ElementType::i32, 2, 2)) == Palette::gray);
    CHECK(auto_palette("water", make_layer_grid(ElementType::u8, 2, 2)) == Palette::water);
    CHECK(auto_palette("water", make_layer_grid(ElementType::u16, 2, 2)) == Palette::gray);
    CHECK(auto_palette("river", p004::grid_of<std::uint8_t>(2, 1, {15, 3})) == Palette::river);
    CHECK(auto_palette("river", p004::grid_of<std::uint8_t>(2, 1, {16, 3})) == Palette::gray);
    CHECK(auto_palette("flow_accum", make_layer_grid(ElementType::u32, 2, 2)) == Palette::log2);
    CHECK(auto_palette("flow_accum", make_layer_grid(ElementType::i32, 2, 2)) == Palette::gray);
    CHECK(auto_palette("flow_dir", make_layer_grid(ElementType::u8, 2, 2)) == Palette::direction);
    CHECK(auto_palette("flow_dir", make_layer_grid(ElementType::i8, 2, 2)) == Palette::gray);
    CHECK(auto_palette("lake", make_layer_grid(ElementType::u32, 2, 2)) == Palette::ids);
    CHECK(auto_palette("lake", make_layer_grid(ElementType::i64, 2, 2)) == Palette::gray);
    CHECK(auto_palette("coast_dist", make_layer_grid(ElementType::i16, 2, 2)) == Palette::signed_distance);
    CHECK(auto_palette("coast_dist", make_layer_grid(ElementType::u16, 2, 2)) == Palette::gray);
    CHECK(auto_palette("water_depth", make_layer_grid(ElementType::u16, 2, 2)) == Palette::gray);
    CHECK(auto_palette("test.ramp", make_layer_grid(ElementType::u8, 2, 2)) == Palette::gray);
}

// --- render_rgb with the palettes ----------------------------------------------------------

TEST_CASE("palette 10.2: render_rgb draws each cell with its palette colour") {
    using worldgen::inspect::render_rgb;
    SUBCASE("hypsometric, scale 1 and 3") {
        const std::vector<std::int16_t> cells = {-32768, -1000, -1, 0, 1, 250, 5000, 30000, 32767};
        const LayerGrid g = p004::grid_of<std::int16_t>(3, 3, cells);
        for (const std::uint32_t scale : {1U, 3U}) {
            const Pixels px = render_rgb(g, Palette::hypsometric, scale);
            REQUIRE(px.size() == 9U * 3U * scale * scale);
            for (std::size_t py = 0; py < 3U * scale; ++py) {
                for (std::size_t pxl = 0; pxl < 3U * scale; ++pxl) {
                    const std::int16_t e = cells[(py / scale) * 3 + pxl / scale];
                    INFO("scale ", scale, " pixel ", pxl, ",", py);
                    CHECK(pixel_at(px, 3U * scale, pxl, py) == ref_hypsometric(e));
                }
            }
        }
    }
    SUBCASE("water, river, direction, with out-of-range values") {
        const std::vector<std::uint8_t> cells = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 14, 15, 16, 200, 255, 0};
        const LayerGrid g = p004::grid_of<std::uint8_t>(4, 4, cells);
        const Pixels w = render_rgb(g, Palette::water, 2);
        const Pixels d = render_rgb(g, Palette::direction, 1);
        for (std::size_t i = 0; i < cells.size(); ++i) {
            const std::size_t x = i % 4;
            const std::size_t y = i / 4;
            INFO("cell ", i, " value ", int{cells[i]});
            const Rgb ew = cells[i] < 4 ? WATER[cells[i]] : MAGENTA;
            for (std::size_t j = 0; j < 4; ++j) {
                CHECK(pixel_at(w, 8, 2 * x + j % 2, 2 * y + j / 2) == ew);
            }
            CHECK(pixel_at(d, 4, x, y) == (cells[i] < 9 ? DIRECTION[cells[i]] : Rgb{255, 255, 255}));
        }
        // river needs every value <= 15 (appendix C item 1).
        CHECK_THROWS_AS((void)render_rgb(g, Palette::river, 1), FatalError);
        const std::vector<std::uint8_t> classes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        const Pixels r = render_rgb(p004::grid_of<std::uint8_t>(16, 1, classes), Palette::river, 1);
        for (std::size_t x = 0; x < 16; ++x) {
            CHECK(pixel_at(r, 16, x, 0) == RIVER[x]);
        }
    }
    SUBCASE("log2 and ids on u32 and on the widest type u64") {
        const std::vector<std::uint32_t> c32 = {0, 1, 2, 3, 4194304, 8388607, 8388608, 0xFFFFFFFFU};
        const LayerGrid g32 = p004::grid_of<std::uint32_t>(4, 2, c32);
        const Pixels l32 = render_rgb(g32, Palette::log2, 1);
        const Pixels i32 = render_rgb(g32, Palette::ids, 1);
        for (std::size_t i = 0; i < c32.size(); ++i) {
            INFO("u32 cell ", i);
            CHECK(pixel_at(l32, 4, i % 4, i / 4) == ref_log2(c32[i]));
            CHECK(pixel_at(i32, 4, i % 4, i / 4) == ref_ids(c32[i]));
        }
        const std::vector<std::uint64_t> c64 = {0, 1, std::uint64_t{1} << 40, U64_MAX};
        const LayerGrid g64 = p004::grid_of<std::uint64_t>(2, 2, c64);
        const Pixels l64 = render_rgb(g64, Palette::log2, 1);
        const Pixels i64 = render_rgb(g64, Palette::ids, 1);
        for (std::size_t i = 0; i < c64.size(); ++i) {
            INFO("u64 cell ", i);
            CHECK(pixel_at(l64, 2, i % 2, i / 2) == ref_log2(c64[i]));
            CHECK(pixel_at(i64, 2, i % 2, i / 2) == ref_ids(c64[i]));
        }
        // u8 and u16 suit too.
        CHECK(pixel_at(render_rgb(p004::grid_of<std::uint8_t>(1, 1, {255}), Palette::log2, 1), 1, 0, 0) ==
              Rgb{88, 88, 88});
        CHECK(pixel_at(render_rgb(p004::grid_of<std::uint16_t>(1, 1, {65535}), Palette::ids, 1), 1, 0, 0) ==
              ref_ids(65535));
    }
    SUBCASE("signed on i16 and on the widest type i64; the scale comes from the layer") {
        // P = 160, M = 48 from the layer itself.
        const std::vector<std::int16_t> c = {-48, -24, -1, 0, 1, 80, 160, 16};
        const Pixels px = render_rgb(p004::grid_of<std::int16_t>(4, 2, c), Palette::signed_distance, 1);
        const worldgen::inspect::SignedScale s{160, 48};
        for (std::size_t i = 0; i < c.size(); ++i) {
            INFO("i16 cell ", i);
            CHECK(pixel_at(px, 4, i % 4, i / 4) == worldgen::inspect::signed_colour(c[i], s));
        }
        CHECK(pixel_at(px, 4, 0, 0) == Rgb{0, 0, 64});
        CHECK(pixel_at(px, 4, 2, 1) == Rgb{255, 255, 255});
        // Only positive values: M = 1. Only negative: P = 1.
        const Pixels pos = render_rgb(p004::grid_of<std::int16_t>(2, 1, {0, 7}), Palette::signed_distance, 1);
        CHECK(pixel_at(pos, 2, 0, 0) == Rgb{0, 0, 0});
        CHECK(pixel_at(pos, 2, 1, 0) == Rgb{255, 255, 255});
        const Pixels neg = render_rgb(p004::grid_of<std::int16_t>(2, 1, {-7, -1}), Palette::signed_distance, 1);
        CHECK(pixel_at(neg, 2, 0, 0) == Rgb{0, 0, 64});
        // -1 over M = 7: gray floor((510 + 7)/14) = 36, blue 255 - 27 = 228.
        CHECK(pixel_at(neg, 2, 1, 0) == Rgb{0, 0, 228});
        const Pixels wide = render_rgb(p004::grid_of<std::int64_t>(3, 1, {I64_MIN, 0, I64_MAX}),
                                       Palette::signed_distance, 1);
        CHECK(pixel_at(wide, 3, 0, 0) == Rgb{0, 0, 64});
        CHECK(pixel_at(wide, 3, 1, 0) == Rgb{0, 0, 0});
        CHECK(pixel_at(wide, 3, 2, 0) == Rgb{255, 255, 255});
        const Pixels i8 = render_rgb(p004::grid_of<std::int8_t>(2, 1, {-128, 127}), Palette::signed_distance, 1);
        CHECK(pixel_at(i8, 2, 0, 0) == Rgb{0, 0, 64});
        CHECK(pixel_at(i8, 2, 1, 0) == Rgb{255, 255, 255});
    }
    SUBCASE("a palette that does not suit the grid is a contract violation") {
        const LayerGrid u8 = p004::grid_of<std::uint8_t>(1, 1, {0});
        const LayerGrid i16 = p004::grid_of<std::int16_t>(1, 1, {0});
        CHECK_THROWS_AS((void)render_rgb(u8, Palette::hypsometric, 1), FatalError);
        CHECK_THROWS_AS((void)render_rgb(u8, Palette::signed_distance, 1), FatalError);
        CHECK_THROWS_AS((void)render_rgb(i16, Palette::water, 1), FatalError);
        CHECK_THROWS_AS((void)render_rgb(i16, Palette::log2, 1), FatalError);
        CHECK_THROWS_AS((void)render_rgb(i16, Palette::ids, 1), FatalError);
        CHECK_THROWS_AS((void)render_rgb(i16, Palette::direction, 1), FatalError);
        CHECK_THROWS_AS((void)render_rgb(i16, Palette::river, 1), FatalError);
        CHECK_THROWS_AS((void)render_rgb(i16, Palette::hypsometric, 0), FatalError);
        CHECK_THROWS_AS((void)render_rgb(i16, Palette::hypsometric, 17), FatalError);
    }
}

// --- The render_rgb overload that fills a caller's buffer -------------------------------

TEST_CASE("render: render_rgb into a caller's buffer equals the returning overload") {
    using worldgen::inspect::render_rgb;
    using worldgen::inspect::render_rgb_size;
    const LayerGrid elev = p004::grid_of<std::int16_t>(4, 3, {-300, -1, 0, 1, 250, 900, 5000, 12000, 25000, 30000,
                                                              -5, 7});
    const LayerGrid ramp = p004::grid_of(p004::SIDE, p004::SIDE, p004::ramp_cells());
    const LayerGrid flow = p004::grid_of<std::uint8_t>(3, 2, {0, 1, 2, 8, 9, 255});
    struct Case {
        const LayerGrid* grid;
        Palette palette;
    };
    for (const Case& c : {Case{&elev, Palette::hypsometric}, Case{&elev, Palette::gray},
                          Case{&elev, Palette::signed_distance}, Case{&ramp, Palette::gray},
                          Case{&flow, Palette::direction}}) {
        for (const std::uint32_t scale : {1U, 3U}) {
            INFO("palette ", worldgen::inspect::palette_name(c.palette), " scale ", scale);
            const Pixels expected = render_rgb(*c.grid, c.palette, scale);
            const std::size_t n = render_rgb_size(*c.grid, scale);
            CHECK(n == 3U * c.grid->width * scale * c.grid->height * scale);
            REQUIRE(n == expected.size());
            // Pre-filled with a byte that the palettes do not produce
            // everywhere, so every byte must be overwritten.
            for (const std::uint8_t fill : {std::uint8_t{0xAB}, std::uint8_t{0x00}, std::uint8_t{0xFF}}) {
                Pixels buffer(n, fill);
                render_rgb(*c.grid, c.palette, scale, std::span<std::uint8_t>(buffer));
                CHECK(buffer == expected);
            }
        }
    }
    // A wrong buffer size is fatal, both ways, and so is an empty one.
    const std::size_t n = render_rgb_size(elev, 1);
    for (const std::size_t wrong : {n - 1, n + 1, std::size_t{0}, render_rgb_size(elev, 2)}) {
        INFO("size ", wrong);
        Pixels buffer(wrong, 0);
        CHECK_THROWS_AS(render_rgb(elev, Palette::hypsometric, 1, std::span<std::uint8_t>(buffer)), FatalError);
    }
    // The same contract violations as the returning overload.
    {
        Pixels buffer(n, 0);
        CHECK_THROWS_AS(render_rgb(elev, Palette::water, 1, std::span<std::uint8_t>(buffer)), FatalError);
    }
    CHECK_THROWS_AS((void)render_rgb_size(elev, 0), FatalError);
    CHECK_THROWS_AS((void)render_rgb_size(elev, 17), FatalError);
    CHECK_THROWS_AS((void)render_rgb_size(p004::grid_of<std::int16_t>(2, 2, {1}), 1), FatalError);
}

// --- codec_name (P0-02 7.1) ----------------------------------------------------------

TEST_CASE("layer blob: codec_name") {
    CHECK(worldgen::db::codec_name(Codec::raw) == "raw");
    CHECK(worldgen::db::codec_name(Codec::delta_varint) == "delta_varint");
    CHECK(worldgen::db::codec_name(static_cast<Codec>(2)).empty());
    CHECK(worldgen::db::codec_name(static_cast<Codec>(255)).empty());
}
