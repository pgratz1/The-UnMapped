// Spec V3D-02 11.1, U7, U8, U10 to U12 (and hash32 of U9): ground types,
// textures, control map, river paint and the textured colour map.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <string>
#include <vector>

#include "terrain/ground.hpp"
#include "worldgen/inspect/render.hpp"

using namespace unmapped::terrain;

namespace {

Layout layout_of(int n) {
    std::string problem;
    const auto layout = layout_for(n, n, problem);
    REQUIRE(layout.has_value());
    return *layout;
}

std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t h = 0xCBF29CE484222325ull;
    for (const std::uint8_t b : bytes) {
        h ^= b;
        h *= 0x100000001B3ull;
    }
    return h;
}

// The test's own copy of 3.2 for one pixel, i and j taken mod 256.
std::uint32_t own_hash(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = a * 0x8DA6B343u + b * 0xD8163841u + c * 0xCB1AB31Fu;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

int own_n(int id, int i, int j) {
    const std::uint32_t P0[15] = {1, 8, 6, 6, 4, 4, 4, 8, 6, 16, 4, 4, 2, 8, 16};
    std::uint32_t q[4];
    for (std::uint32_t o = 0; o < 4; ++o) {
        const std::uint32_t P = P0[id] << o;
        const auto ii = static_cast<std::uint32_t>(i);
        const auto jj = static_cast<std::uint32_t>(j);
        // The lattice index taken mod P (3.2: pixel 256 maps to lattice P = 0),
        // which changes nothing inside 0..255 and extends the formula past it.
        const std::uint32_t ux = ii * P, xl = (ux >> 8) % P, fx = ux & 255, uy = jj * P, yl = (uy >> 8) % P, fy = uy & 255;
        const std::uint32_t x1 = (xl + 1) % P, y1 = (yl + 1) % P;
        auto v = [&](std::uint32_t a, std::uint32_t b) { return own_hash(a, b, 16u * static_cast<std::uint32_t>(id) + o) >> 24; };
        auto sm = [](std::uint32_t f) { return (f * f * (768 - 2 * f)) >> 16; };
        const std::uint32_t sx = sm(fx), sy = sm(fy);
        const std::uint32_t top = v(xl, yl) * (256 - sx) + v(x1, yl) * sx;
        const std::uint32_t bot = v(xl, y1) * (256 - sx) + v(x1, y1) * sx;
        q[o] = (top * (256 - sy) + bot * sy) >> 8;
    }
    return static_cast<int>((8 * q[0] + 4 * q[1] + 2 * q[2] + q[3] + 1920) / 3840);
}

int own_n_wrapped(int id, int i, int j) { return own_n(id, (i + 256) % 256, (j + 256) % 256); }

// A hand-made world of n x n cells: type per cell from a function.
struct Grid {
    Layout layout;
    std::vector<std::int16_t> elevation;
    std::vector<std::uint8_t> biome;
    std::vector<std::int16_t> temperature;
    std::vector<worldgen::inspect::BiomeRecord> records;
};

std::vector<worldgen::inspect::BiomeRecord> shipped_records() {
    return {
        {1, "ocean", "Ocean", 0x183878, 1},           {2, "lake", "Lake", 0x4080CC, 1},
        {3, "grassland", "Grassland", 0x96AA5A, 0},   {4, "temperate_forest", "Forest", 0x2E6034, 0},
        {5, "alpine", "Alpine", 0xBEBEB4, 0},         {6, "heath", "Heath", 0x94664A, 0},
    };
}

}  // namespace

TEST_CASE("U7 ground types") {
    const char* keys[12] = {"grassland", "temperate_forest", "wet_woodland", "marsh", "fen", "steppe",
                            "heath", "highland_moor", "alpine", "blighted_waste", "lake", "ocean"};
    const int ids[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 13, 14};
    for (int k = 0; k < 12; ++k) {
        CHECK(static_cast<int>(ground_type_of_key(keys[k])) == ids[k]);
    }
    CHECK(ground_type_of_key("other") == GroundType::unknown);
    CHECK(ground_type_of_key("") == GroundType::unknown);
    REQUIRE(GROUND_TYPES.size() == 15);
    for (const auto& info : GROUND_TYPES) {
        CHECK(info.p0 * 8 <= 128);
        CHECK(info.p0 >= 1);
    }
    CHECK(GROUND_TYPES[11].fallback_rgb == 0x6E6A64);
    CHECK(GROUND_TYPES[12].roughness == 115);
    CHECK(GROUND_TYPES[9].uv_scale == doctest::Approx(1.0 / 6.0));
    // Colour by key from the records, fallback otherwise.
    const auto records = shipped_records();
    CHECK(ground_type_colour(GroundType::grassland, records) == 0x96AA5A);
    CHECK(ground_type_colour(GroundType::fen, records) == 0x8A8070);
    CHECK(ground_type_colour(GroundType::water, records) == 0xFFFFFF);
    CHECK(ground_type_colour(GroundType::shore, records) == 0xB8A878);
}

TEST_CASE("U8 ground textures") {
    // FNV-1a 64 of each albedo and normal buffer (recorded; the same on GCC,
    // Clang and MSVC).
    const std::uint64_t albedo_fnv[15] = {
0x017CD960DCA4A0BCull, 0x8562F2791FBB0BABull, 0x336B40EAAAF86236ull, 0x1CDBA0FFD2AB3A51ull, 0x6856F4610D9723F1ull,
        0x269962D7CEB7B496ull, 0x0E357AAC9EC2BFA2ull, 0x10808D2777FE0986ull, 0x3F3C478EF2600927ull, 0x0AE0A71A7BDDE648ull,
        0xCF77E793A5FEA07Full, 0x02F68E971E6E14B1ull, 0xD23A013283D90E45ull, 0xD1B65597078C9866ull, 0x18B155EA7B5EF6CEull,
    };
    const std::uint64_t normal_fnv[15] = {
0x4EE2ADD566BC2325ull, 0x447BEC8183AA7622ull, 0xF5452B193D1705D8ull, 0x3CD4AB95F7AE849Aull, 0x44BD4331C6BC7838ull,
        0x6EE2B68E78A13A31ull, 0x26F52156027FDFABull, 0xFBE18A972BDDEC99ull, 0x5FE39943BC522B2Aull, 0x071A23C459599C8Full,
        0x6B8232F9223F3F0Dull, 0xE791EF1F6E44A3F9ull, 0x9699D0CA076EFD1Eull, 0xBA6FF962C9BC1D52ull, 0x3647FF4DD85049FDull,
    };
    for (int id = 0; id < 15; ++id) {
        CAPTURE(id);
        const GroundTexture tex = build_ground_texture(static_cast<GroundType>(id));
        REQUIRE(tex.albedo.size() == 4u * 256u * 256u);
        REQUIRE(tex.normal.size() == 4u * 256u * 256u);
        const auto& info = GROUND_TYPES[static_cast<std::size_t>(id)];
        for (const auto [i, j] : {std::pair{0, 0}, std::pair{255, 255}, std::pair{17, 93}, std::pair{128, 1}}) {
            const std::size_t k = 4 * (static_cast<std::size_t>(j) * 256 + static_cast<std::size_t>(i));
            const int n = own_n(id, i, j);
            const int g = 255 - (info.grain * (255 - n) + 127) / 255;
            CHECK(tex.albedo[k] == g);
            CHECK(tex.albedo[k + 1] == g);
            CHECK(tex.albedo[k + 2] == g);
            CHECK(tex.albedo[k + 3] == n);
            const std::int64_t gx = own_n_wrapped(id, i + 1, j) - own_n_wrapped(id, i - 1, j);
            const std::int64_t gy = own_n_wrapped(id, i, j + 1) - own_n_wrapped(id, i, j - 1);
            const std::int64_t X = -info.normal_strength * gx, Y = info.normal_strength * gy, Z = 512;
            std::int64_t L = 0;
            while ((L + 1) * (L + 1) <= X * X + Y * Y + Z * Z) {
                ++L;
            }
            CHECK(tex.normal[k] == 128 + 127 * X / L);
            CHECK(tex.normal[k + 1] == 128 + 127 * Y / L);
            CHECK(tex.normal[k + 2] == 128 + 127 * Z / L);
            CHECK(tex.normal[k + 3] == info.roughness);
        }
        // The noise tiles: pixel 256 + i is pixel i.
        for (int i = 0; i < 256; i += 15) {
            CHECK(own_n(id, 256 + i, 40) == own_n(id, i, 40));
            CHECK(own_n(id, 40, 256 + i) == own_n(id, 40, i));
        }
        CHECK(fnv1a(tex.albedo) == albedo_fnv[id]);
        CHECK(fnv1a(tex.normal) == normal_fnv[id]);
    }
    // Id 0 is flat.
    const GroundTexture flat = build_ground_texture(GroundType::unknown);
    bool all_flat = true;
    for (std::size_t k = 0; k < flat.normal.size(); k += 4) {
        all_flat = all_flat && flat.normal[k] == 128 && flat.normal[k + 1] == 128 && flat.normal[k + 2] == 255;
    }
    CHECK(all_flat);
    CHECK(flat.mean_grey == 255.0);
}

TEST_CASE("U9 hash32 vectors") {
    CHECK(hash32(0, 0, 0) == 0u);
    CHECK(hash32(1, 2, 3) == 0x48FE210Au);
    CHECK(hash32(255, 0, 16) == 0x56785DECu);
    CHECK(hash32(0xFFFFFFFFu, 7, 230) == 0x51AAE231u);
}

TEST_CASE("albedo_colour gives the sRGB colour at the mean grey") {
    const auto records = shipped_records();
    const GroundTexture tex = build_ground_texture(GroundType::grassland);
    const auto c = albedo_colour(GroundType::grassland, tex, records);
    // c * lin(mean grey) = lin(record colour).
    auto lin = [](double u) { return u <= 0.04045 ? u / 12.92 : std::pow((u + 0.055) / 1.055, 2.4); };
    const double m = lin(tex.mean_grey / 255.0);
    CHECK(static_cast<double>(c[0]) * m == doctest::Approx(lin(0x96 / 255.0)).epsilon(1e-6));
    CHECK(static_cast<double>(c[2]) * m == doctest::Approx(lin(0x5A / 255.0)).epsilon(1e-6));
}

namespace {

GroundCells cells_of_types(const Layout& layout, const std::vector<std::uint8_t>& types) {
    GroundCells cells;
    const std::size_t total = static_cast<std::size_t>(layout.n) * static_cast<std::size_t>(layout.n);
    cells.type = types;
    cells.painted.assign(total, 0);
    cells.rock.assign(total, 0);
    cells.snow.assign(total, 0);
    return cells;
}

std::uint32_t blend_of(std::uint32_t c) { return (c >> 14) & 0xFF; }
std::uint32_t overlay_of(std::uint32_t c) { return (c >> 22) & 0x1F; }
std::uint32_t base_of(std::uint32_t c) { return c >> 27; }

}  // namespace

TEST_CASE("U10 build_control") {
    // The layout must be 512 or more; the hand-made grid lies in its corner.
    const Layout layout = layout_of(512);
    const int n = layout.n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    auto at = [n](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x); };

    SUBCASE("a straight border") {
        std::vector<std::uint8_t> types(total, 1);
        for (int y = 0; y < n; ++y) {
            for (int x = 8; x < n; ++x) {
                types[at(x, y)] = 2;
            }
        }
        const auto control = build_control(layout, cells_of_types(layout, types));
        const int y = 100;
        CHECK(blend_of(control[at(5, y)]) == 0);
        CHECK(blend_of(control[at(6, y)]) == 51);
        CHECK(blend_of(control[at(7, y)]) == 102);
        CHECK(overlay_of(control[at(7, y)]) == 2);
        CHECK(blend_of(control[at(8, y)]) == 102);
        CHECK(overlay_of(control[at(8, y)]) == 1);
        CHECK(blend_of(control[at(9, y)]) == 51);
        CHECK(blend_of(control[at(10, y)]) == 0);
        CHECK(control[at(10, y)] == (2u << 27 | 2u << 22));
    }
    SUBCASE("a lone cell, capped at 127") {
        std::vector<std::uint8_t> types(total, 1);
        types[at(20, 20)] = 7;
        const auto control = build_control(layout, cells_of_types(layout, types));
        CHECK(control[at(20, 20)] == (7u << 27 | 1u << 22 | 127u << 14));
    }
    SUBCASE("rock, snow and ties") {
        std::vector<std::uint8_t> types(total, 9);
        GroundCells cells = cells_of_types(layout, types);
        // Rock from the elevation through build_ground_cells.
        std::vector<std::int16_t> elevation(total, 1000);
        // g = |E(x+1) - E(x-1)|: 224, 272, 320.
        const int gs[3] = {224, 272, 320};
        for (int k = 0; k < 3; ++k) {
            const int y = 50 + 10 * k;
            elevation[at(9, y)] = 1000;
            elevation[at(11, y)] = static_cast<std::int16_t>(1000 + gs[k]);
        }
        std::vector<std::int16_t> temperature(total, 0);
        const int ts[3] = {-20, -40, -60};
        for (int k = 0; k < 3; ++k) {
            temperature[at(300, 50 + 10 * k)] = static_cast<std::int16_t>(ts[k]);
        }
        const auto records = shipped_records();
        std::vector<std::uint8_t> biome(total, 5);  // alpine
        GroundInputs in;
        in.layout = &layout;
        in.elevation = elevation;
        in.biome = biome;
        in.temperature = temperature;
        in.records = records;
        const GroundCells g = build_ground_cells(in);
        const std::uint8_t want[3] = {0, 128, 255};
        for (int k = 0; k < 3; ++k) {
            CHECK(g.rock[at(10, 50 + 10 * k)] == want[k]);
            CHECK(g.snow[at(300, 50 + 10 * k)] == want[k]);
        }
        const auto control = build_control(layout, g);
        CHECK(control[at(10, 60)] == (9u << 27 | 11u << 22 | 128u << 14));
        CHECK(control[at(300, 70)] == (9u << 27 | 12u << 22 | 255u << 14));
        // A tie between rock and snow goes to rock (the earlier candidate).
        cells = cells_of_types(layout, types);
        cells.rock[at(200, 200)] = 90;
        cells.snow[at(200, 200)] = 90;
        // A tie between snow and the border goes to snow.
        types[at(400, 400)] = 1;
        types[at(401, 400)] = 1;
        types[at(402, 400)] = 1;
        types[at(403, 400)] = 1;
        types[at(404, 400)] = 1;
        cells.type = types;
        cells.snow[at(402, 402)] = 51;  // border at (402, 402) is 51: one row of grassland
        const auto tied = build_control(layout, cells);
        CHECK(overlay_of(tied[at(200, 200)]) == 11);
        CHECK(blend_of(tied[at(200, 200)]) == 90);
        CHECK(overlay_of(tied[at(402, 402)]) == 12);
        CHECK(blend_of(tied[at(402, 402)]) == 51);
    }
    SUBCASE("painted, lake and sea cells have blend 0") {
        std::vector<std::uint8_t> types(total, 1);
        types[at(30, 30)] = 13;
        types[at(31, 30)] = 14;
        for (int x = 0; x < n; ++x) {
            types[at(x, 40)] = 2;
        }
        GroundCells cells = cells_of_types(layout, types);
        cells.painted[at(50, 40)] = 3;
        cells.rock[at(50, 40)] = 255;
        const auto control = build_control(layout, cells);
        CHECK(control[at(30, 30)] == (13u << 27 | 13u << 22));
        CHECK(control[at(31, 30)] == (14u << 27 | 14u << 22));
        CHECK(control[at(50, 40)] == (13u << 27 | 13u << 22));
        // Water and shore are not counted in the land window.
        CHECK(blend_of(control[at(29, 30)]) == 0);
        bool bits = true;
        bool ids = true;
        for (const std::uint32_t c : control) {
            bits = bits && (c & 0x3FFFu) == 0;
            ids = ids && base_of(c) < 15 && overlay_of(c) < 15;
        }
        CHECK(bits);
        CHECK(ids);
    }
}

TEST_CASE("U11 river_paint against the 2D rule") {
    const Layout layout = layout_of(512);
    const int n = layout.n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    auto at = [n](int x, int y) { return static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x); };
    std::vector<std::uint8_t> water(total, 0), river(total, 0), flow(total, 2);
    // A diagonal step SE (code 1) at (10, 10) closes its corner at (11, 10).
    water[at(10, 10)] = 1;
    river[at(10, 10)] = 2;
    flow[at(10, 10)] = 1;
    // Class 4 at (20, 20) widens east and south.
    water[at(20, 20)] = 1;
    river[at(20, 20)] = 4;
    // A class-5 widening onto a lake cell is refused.
    water[at(30, 30)] = 1;
    river[at(30, 30)] = 5;
    water[at(31, 30)] = 2;
    const auto paint = river_paint(layout, water, river, flow);
    CHECK(paint[at(10, 10)] == 2);
    CHECK(paint[at(11, 10)] == 2);
    CHECK(paint[at(10, 11)] == 0);
    CHECK(paint[at(20, 20)] == 4);
    CHECK(paint[at(21, 20)] == 4);
    CHECK(paint[at(20, 21)] == 4);
    CHECK(paint[at(31, 30)] == 0);
    CHECK(paint[at(30, 31)] == 5);
    // Without river, every water-1 cell has class 16.
    const auto plain = river_paint(layout, water, {}, flow);
    CHECK(plain[at(10, 10)] == 16);
    CHECK(plain[at(11, 10)] == 0);
    CHECK(plain[at(31, 30)] == 0);
    // Without water nothing is painted.
    const auto none = river_paint(layout, {}, river, flow);
    CHECK(std::count(none.begin(), none.end(), std::uint8_t{0}) == static_cast<std::ptrdiff_t>(total));
}

TEST_CASE("U12 build_textured_colours") {
    const Layout layout = layout_of(512);
    const std::size_t total = 512u * 512u;
    std::vector<std::uint8_t> types(total, 1);
    types[1] = 13;
    GroundCells cells = cells_of_types(layout, types);
    cells.painted[2] = 1;
    cells.painted[3] = 16;
    cells.painted[4] = 7;
    const auto records = shipped_records();
    const auto rgba = build_textured_colours(layout, cells, records);
    REQUIRE(rgba.size() == 4 * total);
    auto px = [&rgba](std::size_t k) {
        return (static_cast<std::uint32_t>(rgba[4 * k]) << 16) | (static_cast<std::uint32_t>(rgba[4 * k + 1]) << 8) |
               rgba[4 * k + 2];
    };
    CHECK(px(0) == 0xFFFFFF);
    CHECK(px(1) == 0x4080CC);
    CHECK(px(2) == 0x467DD7);  // (70, 125, 215)
    CHECK(px(3) == 0x3464C8);  // (52, 100, 200)
    CHECK(px(4) == 0x1432A0);  // (20, 50, 160)
    bool alpha = true;
    for (std::size_t k = 0; k < total; ++k) {
        alpha = alpha && rgba[4 * k + 3] == 128;
    }
    CHECK(alpha);
    // Without a lake record, 0x4080CC.
    const auto no_lake = build_textured_colours(layout, cells, {});
    CHECK(no_lake[4] == 0x40);
}
