// Layer rendering (render_rgb and the gray palette), spec P0-04 sections 6,
// 10, 12.3 and 12.6, with the amendments of section 14.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "p0_04_reference.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/stages/test_pattern.hpp"

namespace {

using worldgen::FatalError;
using worldgen::db::LayerGrid;
using worldgen::inspect::gray_level;
using worldgen::inspect::Palette;
using worldgen::inspect::render_rgb;
using worldgen::inspect::value_range;
using Pixels = std::vector<std::uint8_t>;

constexpr std::uint64_t U64_MAX = std::numeric_limits<std::uint64_t>::max();
constexpr std::int64_t I64_MIN = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t I64_MAX = std::numeric_limits<std::int64_t>::max();

// Golden SHA-256 of render_rgb(<layer of the seed-0 test-pattern world>,
// gray, 1) (P0-04 12.3, 12.6). PLACEHOLDERS: the validation specialist
// records the values. While any is empty the golden test is skipped.
constexpr std::string_view GOLDEN_RGB_RAMP = "bb739f8740afff2f8fef18d9bfa18b267ad15ace01dd1e28e4ee951b2018aaa0";
constexpr std::string_view GOLDEN_RGB_HASH = "e9ba1cadd82dfafb1fa7ecfc3cbeaf01438e6a03ab6b4661c4f0ab8fe7ad71f7";
constexpr std::string_view GOLDEN_RGB_NOISE = "626e58cc782e85658fdb28b8494b7010e2fc9661cbb4e8f3b315d900e24a10d5";
constexpr bool RGB_GOLDEN_RECORDED = !GOLDEN_RGB_RAMP.empty() && !GOLDEN_RGB_HASH.empty() && !GOLDEN_RGB_NOISE.empty();

// The gray buffer of a grid computed here, independently: values widened to
// int64 (enough for the element types used with it), g = floor((510 d + r) /
// (2 r)) with d = v - min, r = max - min (exact in uint64 while r < 2^54).
template <class T>
Pixels expected_gray(const std::vector<T>& cells, std::uint32_t width, std::uint32_t height, std::uint32_t scale) {
    std::int64_t lo = static_cast<std::int64_t>(cells[0]);
    std::int64_t hi = lo;
    for (const T v : cells) {
        lo = std::min(lo, static_cast<std::int64_t>(v));
        hi = std::max(hi, static_cast<std::int64_t>(v));
    }
    const auto r = static_cast<std::uint64_t>(hi - lo);
    const std::size_t w = static_cast<std::size_t>(width) * scale;
    const std::size_t h = static_cast<std::size_t>(height) * scale;
    Pixels px(w * h * 3);
    for (std::size_t py = 0; py < h; ++py) {
        for (std::size_t pxl = 0; pxl < w; ++pxl) {
            const T v = cells[(py / scale) * width + (pxl / scale)];
            const auto d = static_cast<std::uint64_t>(static_cast<std::int64_t>(v) - lo);
            const auto g = static_cast<std::uint8_t>(r == 0 ? 0 : (510 * d + r) / (2 * r));
            for (std::size_t c = 0; c < 3; ++c) {
                px[(py * w + pxl) * 3 + c] = g;
            }
        }
    }
    return px;
}

std::uint8_t pixel(const Pixels& px, std::size_t image_width, std::size_t x, std::size_t y, std::size_t channel) {
    return px.at((y * image_width + x) * 3 + channel);
}

}  // namespace

TEST_CASE("render 6: palette names") {
    CHECK(worldgen::inspect::palette_name(Palette::gray) == "gray");
    CHECK(worldgen::inspect::palette_from_name("gray") == std::optional<Palette>(Palette::gray));
    CHECK(!worldgen::inspect::palette_from_name("Gray").has_value());
    CHECK(!worldgen::inspect::palette_from_name("").has_value());
    CHECK(!worldgen::inspect::palette_from_name("rainbow").has_value());
    // Layers without a registered palette (P1-00 10.2) render gray under
    // auto; elevation is now hypsometric (P0-04 amendment 16).
    for (const char* layer : {"test.ramp", "test.hash", "test.noise", "anything"}) {
        CHECK(worldgen::inspect::registered_palette(layer) == Palette::gray);
    }
    CHECK(worldgen::inspect::registered_palette("elevation") == Palette::hypsometric);
    CHECK(worldgen::inspect::MIN_RENDER_SCALE == 1U);
    CHECK(worldgen::inspect::MAX_RENDER_SCALE == 16U);
}

TEST_CASE("render 6, 12.3: gray formula and the worked numbers") {
    // 12.3: i16 values -32767, 0, 32767: r = 65534.
    CHECK(gray_level(0, 65534) == 0);
    CHECK(gray_level(32767, 65534) == 128);  // 16776704 / 131068 = 128 exactly
    CHECK(gray_level(65534, 65534) == 255);  // 33487874 / 131068 = 255.5, floor 255
    // r = 0: black.
    CHECK(gray_level(0, 0) == 0);
    // Full u8 range: identity.
    for (std::uint64_t v = 0; v < 256; ++v) {
        CHECK(gray_level(v, 255) == v);
    }
    // Rounding half up: 1 * 255 / 2 = 127.5 -> 128; 1 * 255 / 4 = 63.75 -> 64.
    CHECK(gray_level(1, 2) == 128);
    CHECK(gray_level(1, 4) == 64);
    CHECK(gray_level(1, 510) == 1);  // 0.5 -> 1
    CHECK(gray_level(1, 511) == 0);  // 0.499... -> 0
    // Full 64-bit range, which needs the 128-bit intermediate.
    CHECK(gray_level(0, U64_MAX) == 0);
    CHECK(gray_level(1, U64_MAX) == 0);
    CHECK(gray_level(std::uint64_t{1} << 63, U64_MAX) == 128);  // floor((2^72 - 1) / (2^65 - 2)) = 128
    CHECK(gray_level(U64_MAX - 1, U64_MAX) == 255);
    CHECK(gray_level(U64_MAX, U64_MAX) == 255);
    // d > r is a contract violation.
    CHECK_THROWS_AS((void)gray_level(2, 1), FatalError);
    CHECK_THROWS_AS((void)gray_level(1, 0), FatalError);
}

TEST_CASE("render 6: value_range per element type") {
    {
        const auto r = value_range(p004::grid_of<std::int16_t>(3, 1, {-32767, 0, 32767}));
        CHECK(r.is_signed);
        CHECK(r.min_signed == -32767);
        CHECK(r.max_signed == 32767);
    }
    {
        const auto r = value_range(p004::grid_of<std::int8_t>(2, 2, {5, -128, 127, 0}));
        CHECK(r.is_signed);
        CHECK(r.min_signed == -128);
        CHECK(r.max_signed == 127);
    }
    {
        const auto r = value_range(p004::grid_of<std::uint64_t>(3, 1, {7, U64_MAX, 0}));
        CHECK(!r.is_signed);
        CHECK(r.min_unsigned == 0U);
        CHECK(r.max_unsigned == U64_MAX);
    }
    {
        const auto r = value_range(p004::grid_of<std::int64_t>(3, 1, {I64_MAX, 0, I64_MIN}));
        CHECK(r.is_signed);
        CHECK(r.min_signed == I64_MIN);
        CHECK(r.max_signed == I64_MAX);
    }
    {
        const auto r = value_range(p004::grid_of<std::uint32_t>(1, 1, {4000000000U}));
        CHECK(!r.is_signed);
        CHECK(r.min_unsigned == 4000000000U);
        CHECK(r.max_unsigned == 4000000000U);
    }
    {
        // Compared as values of the type: 0xFFFF is the largest u16, not -1.
        const auto r = value_range(p004::grid_of<std::uint16_t>(2, 1, {0xFFFF, 1}));
        CHECK(r.min_unsigned == 1U);
        CHECK(r.max_unsigned == 0xFFFFU);
    }
    CHECK_THROWS_AS((void)value_range(p004::grid_of<std::uint8_t>(2, 2, {1, 2, 3})), FatalError);
}

TEST_CASE("render 12.3: test.ramp renders as (v, v, v), and --scale 3 repeats each cell") {
    const LayerGrid ramp = worldgen::stages::test_ramp_grid();
    const Pixels px = render_rgb(ramp, Palette::gray, 1);
    REQUIRE(px.size() == 256U * 256U * 3U);
    for (std::size_t y = 0; y < 256; ++y) {
        for (std::size_t x = 0; x < 256; ++x) {
            const auto v = static_cast<std::uint8_t>((x + 2 * y) % 256);
            for (std::size_t c = 0; c < 3; ++c) {
                if (pixel(px, 256, x, y, c) != v) {
                    FAIL("pixel (", x, ", ", y, ") channel ", c, " is ", int{pixel(px, 256, x, y, c)}, ", expected ",
                         int{v});
                }
            }
        }
    }
    const Pixels px3 = render_rgb(ramp, Palette::gray, 3);
    REQUIRE(px3.size() == 768U * 768U * 3U);
    bool blocks_ok = true;
    for (std::size_t y = 0; y < 256 && blocks_ok; ++y) {
        for (std::size_t x = 0; x < 256 && blocks_ok; ++x) {
            for (std::size_t j = 0; j < 3; ++j) {
                for (std::size_t i = 0; i < 3; ++i) {
                    for (std::size_t c = 0; c < 3; ++c) {
                        if (pixel(px3, 768, 3 * x + i, 3 * y + j, c) != pixel(px, 256, x, y, c)) {
                            blocks_ok = false;
                        }
                    }
                }
            }
        }
    }
    CHECK(blocks_ok);
    CHECK(px3 == expected_gray(*ramp.get_if<std::uint8_t>(), 256, 256, 3));
}

TEST_CASE("render 6, 10: every test-pattern layer against an independent gray computation") {
    // The grids themselves are checked against the formulas in
    // stages_test_pattern_test.cpp; here the gray mapping of each.
    const LayerGrid hash = worldgen::stages::test_hash_grid(0);
    const LayerGrid noise = worldgen::stages::test_noise_grid(0);
    const auto& noise_cells = *noise.get_if<std::int16_t>();
    CHECK(render_rgb(hash, Palette::gray, 1) == expected_gray(*hash.get_if<std::uint8_t>(), 256, 256, 1));
    CHECK(render_rgb(noise, Palette::gray, 1) == expected_gray(noise_cells, 256, 256, 1));
    CHECK(render_rgb(noise, Palette::gray, 2) == expected_gray(noise_cells, 256, 256, 2));
}

TEST_CASE("render 12.3: constant layers render all black") {
    const LayerGrid u8c = p004::grid_of<std::uint8_t>(4, 3, std::vector<std::uint8_t>(12, 200));
    const LayerGrid i64c = p004::grid_of<std::int64_t>(2, 2, std::vector<std::int64_t>(4, I64_MIN));
    const LayerGrid u64c = p004::grid_of<std::uint64_t>(1, 1, {U64_MAX});
    const Pixels a = render_rgb(u8c, Palette::gray, 1);
    CHECK(a == Pixels(4 * 3 * 3, std::uint8_t{0}));
    CHECK(render_rgb(i64c, Palette::gray, 5) == Pixels(10 * 10 * 3, std::uint8_t{0}));
    CHECK(render_rgb(u64c, Palette::gray, 16) == Pixels(16 * 16 * 3, std::uint8_t{0}));
}

TEST_CASE("render 12.3: synthetic i16 layer -32767, 0, 32767 gives 0, 128, 255") {
    const Pixels px = render_rgb(p004::grid_of<std::int16_t>(3, 1, {-32767, 0, 32767}), Palette::gray, 1);
    CHECK(px == Pixels{0, 0, 0, 128, 128, 128, 255, 255, 255});
    // Vertical orientation: top row is y = 0 (north).
    const Pixels col = render_rgb(p004::grid_of<std::int16_t>(1, 3, {32767, 0, -32767}), Palette::gray, 1);
    CHECK(col == Pixels{255, 255, 255, 128, 128, 128, 0, 0, 0});
    // Scale 2 on a 2 x 1 layer: rows of 4 pixels, two rows.
    const Pixels s2 = render_rgb(p004::grid_of<std::uint8_t>(2, 1, {0, 1}), Palette::gray, 2);
    CHECK(s2 == Pixels{0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255});
}

TEST_CASE("render 6: full-range 64-bit layers") {
    CHECK(render_rgb(p004::grid_of<std::uint64_t>(3, 1, {0, std::uint64_t{1} << 63, U64_MAX}), Palette::gray, 1) ==
          Pixels{0, 0, 0, 128, 128, 128, 255, 255, 255});
    CHECK(render_rgb(p004::grid_of<std::int64_t>(3, 1, {I64_MIN, 0, I64_MAX}), Palette::gray, 1) ==
          Pixels{0, 0, 0, 128, 128, 128, 255, 255, 255});
    CHECK(render_rgb(p004::grid_of<std::int32_t>(2, 1, {std::numeric_limits<std::int32_t>::min(),
                                                        std::numeric_limits<std::int32_t>::max()}),
                     Palette::gray, 1) == Pixels{0, 0, 0, 255, 255, 255});
}

TEST_CASE("render 6: contract violations") {
    const LayerGrid g = p004::grid_of<std::uint8_t>(1, 1, {1});
    CHECK_THROWS_AS((void)render_rgb(g, Palette::gray, 0), FatalError);
    CHECK_THROWS_AS((void)render_rgb(g, Palette::gray, 17), FatalError);
    CHECK_THROWS_AS((void)render_rgb(p004::grid_of<std::uint8_t>(2, 1, {1}), Palette::gray, 1), FatalError);
    CHECK_THROWS_AS((void)render_rgb(p004::grid_of<std::uint8_t>(0, 0, {}), Palette::gray, 1), FatalError);
    CHECK(render_rgb(g, Palette::gray, 16).size() == 16U * 16U * 3U);
}

TEST_CASE("render 12.3: render_rgb hashes of the seed-0 test pattern are deterministic") {
    const LayerGrid layers[] = {worldgen::stages::test_ramp_grid(), worldgen::stages::test_hash_grid(0),
                                worldgen::stages::test_noise_grid(0)};
    const char* names[] = {"test.ramp", "test.hash", "test.noise"};
    std::string listing;
    for (std::size_t i = 0; i < 3; ++i) {
        const std::string a = worldgen::sha256_hex(render_rgb(layers[i], Palette::gray, 1));
        const std::string b = worldgen::sha256_hex(render_rgb(layers[i], Palette::gray, 1));
        CHECK(a == b);
        listing += std::string(names[i]) + " " + a + "\n";
    }
    MESSAGE("observed render_rgb SHA-256, seed 0, gray, scale 1:\n", listing);
}

TEST_CASE("render 12.3, 12.6: golden render_rgb hashes, seed 0" * doctest::skip(!RGB_GOLDEN_RECORDED)) {
    CHECK(worldgen::sha256_hex(render_rgb(worldgen::stages::test_ramp_grid(), Palette::gray, 1)) == GOLDEN_RGB_RAMP);
    CHECK(worldgen::sha256_hex(render_rgb(worldgen::stages::test_hash_grid(0), Palette::gray, 1)) == GOLDEN_RGB_HASH);
    CHECK(worldgen::sha256_hex(render_rgb(worldgen::stages::test_noise_grid(0), Palette::gray, 1)) == GOLDEN_RGB_NOISE);
}
