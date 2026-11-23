// Spec P4-01 13.2 item 7 and 5.6: the territory image on T1's hand-made layers (8 x 6 cells) with the
// expected pixels written here: a held cell, a border cell, a lake cell (step 3), a river cell and dry
// cells painted by the river widening and the diagonal closing (step 4, unfilled), a water-1 cell of
// class 0 (no paint entry, so fillable), an ocean cell, a held region's cell with e < 0, a free cell, a
// cell of a region value with no region row, elevation 0 (land), scale 2; the equality with
// render_composite_regions for an empty fill and an all-empty fill (k = 1, 3; with and without water,
// river and flow_dir); territory_fill; 5.6's 4 x 1 example; and the FatalErrors of render_territory.
//
// Written from the spec only. Shading: shade_colour(c, f) = min(255, (c f + 128) >> 8) per channel
// (P1-00 10.2 step 2), so factor 256 leaves a colour unchanged; T1's factor at (0, 4) and (5, 1) is 251
// (gy = 0 - 10 and gx = 0 - 10 respectively, floor_div(-10, 2) = -5).

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "timeline_support.hpp"
#include "worldgen/core/error.hpp"

namespace {

using namespace tltest;
using wi::Rgb;

struct T1Layers {
    const db::LayerGrid& elevation = t1_layer("elevation");
    const db::LayerGrid& water = t1_layer("water");
    const db::LayerGrid& river = t1_layer("river");
    const db::LayerGrid& flow_dir = t1_layer("flow_dir");
    const db::LayerGrid& biome = t1_layer("biome");
    const db::LayerGrid& region = t1_layer("region");
    std::vector<wi::BiomeColour> table = t1_biome_colours();
};

Rgb pixel(const std::vector<std::uint8_t>& img, std::uint32_t width, std::uint32_t x, std::uint32_t y) {
    const std::size_t i = 3U * (std::size_t{y} * width + x);
    REQUIRE(i + 2U < img.size());
    return Rgb{img[i], img[i + 1U], img[i + 2U]};
}

const wh::LoadedHistory& t1_history() {
    static const wh::LoadedHistory h = [] {
        const db::WorldReader r = open_reader(t1_world());
        return load(r);
    }();
    return h;
}

std::string fatal_text(const std::function<void()>& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

}  // namespace

TEST_SUITE("timeline") {

TEST_CASE("P4-01 5.6: the 4 x 1 example") {
    db::LayerGrid elevation = db::make_layer_grid(db::ElementType::i16, 4, 1);
    elevation.values = std::vector<std::int16_t>{10, 10, 10, 10};
    db::LayerGrid biome = db::make_layer_grid(db::ElementType::u8, 4, 1);
    biome.values = std::vector<std::uint8_t>{1, 1, 1, 1};
    db::LayerGrid region = db::make_layer_grid(db::ElementType::u16, 4, 1);
    region.values = std::vector<std::uint16_t>{1, 1, 2, 0};
    const std::vector<wi::BiomeColour> table = {{1, 0x808000}};
    const wi::TerritoryBase base = wi::TerritoryBase::build(elevation, nullptr, nullptr, nullptr, biome, region, table);
    CHECK(base.width() == 4U);
    CHECK(base.height() == 1U);
    const Rgb elf0 = wi::civilization_shade(wi::race_base_colour("elf"), 0);
    std::vector<std::optional<Rgb>> fill(3);
    fill[1] = elf0;
    const std::vector<std::uint8_t> img = wi::render_territory(base, fill, 1);
    REQUIRE(img.size() == 12U);
    CHECK(pixel(img, 4, 0, 0) == Rgb{52, 150, 70});   // fillable, factor 256
    CHECK(pixel(img, 4, 1, 0) == Rgb{0, 0, 0});       // border: E neighbour 2
    CHECK(pixel(img, 4, 2, 0) == Rgb{128, 128, 0});   // free: B
    CHECK(pixel(img, 4, 3, 0) == Rgb{128, 128, 0});   // v = 0: never fillable, B
    // v = 0 stays B even if a fill entry [0] were set
    std::vector<std::optional<Rgb>> f0(3);
    f0[0] = Rgb{1, 2, 3};
    CHECK(wi::render_territory(base, f0, 1) == wi::render_composite_regions(elevation, nullptr, nullptr, nullptr,
                                                                            biome, region, 1, table));
}

TEST_CASE("P4-01 13.2 item 7: territory pixels on T1's layers at year 1500") {
    const T1Layers L;
    const wh::LoadedHistory& h = t1_history();
    const wi::TerritoryBase base =
        wi::TerritoryBase::build(L.elevation, &L.water, &L.river, &L.flow_dir, L.biome, L.region, L.table);
    REQUIRE(base.width() == 8U);
    REQUIRE(base.height() == 6U);
    const std::vector<Rgb> colours = wi::civilization_colours(h.tables);
    // at 1500: r1 c1, r2 c3, r3 c2, r4 c4, r6 free
    const std::vector<std::optional<Rgb>> fill = wi::territory_fill(h.timeline, colours, 1500);
    REQUIRE(fill.size() == 7U);  // largest region id 6, plus 1
    CHECK_FALSE(fill[0].has_value());
    CHECK(fill[1] == std::optional<Rgb>(Rgb{52, 150, 70}));
    CHECK(fill[2] == std::optional<Rgb>(Rgb{153, 202, 162}));
    CHECK(fill[3] == std::optional<Rgb>(Rgb{160, 82, 45}));
    CHECK(fill[4] == std::optional<Rgb>(Rgb{207, 168, 150}));
    CHECK_FALSE(fill[5].has_value());  // no region row 5
    CHECK_FALSE(fill[6].has_value());  // never held
    const std::vector<std::uint8_t> img = wi::render_territory(base, fill, 1);
    REQUIRE(img.size() == 3U * 8U * 6U);
    struct P {
        std::uint32_t x, y;
        Rgb rgb;
        const char* what;
    };
    const P expected[] = {
        {0, 0, {52, 150, 70}, "r1 held by c1 (elf s0), factor 256"},
        {1, 0, {0, 0, 0}, "border cell (E neighbour region 2)"},
        {0, 1, {64, 128, 204}, "lake cell (step 3) of a held region"},
        {2, 1, {153, 202, 162}, "water 1 of river class 0: no paint entry, so filled with c3 (elf s1)"},
        {4, 0, {70, 125, 215}, "river cell, class 1"},
        {5, 0, {70, 125, 215}, "dry cell painted by the diagonal closing of (4, 0), unfilled"},
        {1, 3, {30, 80, 190}, "river cell, class 4"},
        {1, 4, {30, 80, 190}, "dry cell painted by the widening (x, y + 1), unfilled"},
        {2, 3, {0, 0, 0}, "painted by the widening (x + 1, y) but a border cell"},
        {0, 4, {203, 165, 147}, "c4 (dwarf s1) shaded by factor 251"},
        {0, 5, {207, 168, 150}, "elevation 0 is land: filled, factor 256"},
        {5, 1, {157, 80, 44}, "c2 (dwarf s0) shaded by factor 251"},
        {6, 1, {119, 169, 205}, "region 3 but elevation -5: the ocean colour of depth 5, unfilled"},
        {7, 0, {112, 163, 201}, "ocean of depth 50, region 0"},
        {6, 3, {96, 160, 64}, "r6 never held: B, the biome colour at factor 256"},
        {3, 3, {255, 0, 255}, "region value 5 with no region row, biome 9 with no biome row: B"},
        {6, 0, {117, 167, 204}, "ocean of depth 20, region 0"},
    };
    for (const P& p : expected) {
        CAPTURE(std::string(p.what));
        CAPTURE(p.x);
        CAPTURE(p.y);
        CHECK(pixel(img, 8, p.x, p.y) == p.rgb);
    }
    // scale 2: each cell's colour fills its 2 x 2 block
    const std::vector<std::uint8_t> img2 = wi::render_territory(base, fill, 2);
    REQUIRE(img2.size() == 3U * 16U * 12U);
    for (const P& p : expected) {
        CAPTURE(std::string(p.what));
        for (std::uint32_t dy = 0; dy < 2; ++dy) {
            for (std::uint32_t dx = 0; dx < 2; ++dx) CHECK(pixel(img2, 16, 2 * p.x + dx, 2 * p.y + dy) == p.rgb);
        }
    }
    // the caller-buffer overload writes the same bytes
    std::vector<std::uint8_t> out(img2.size(), 0xAB);
    wi::render_territory(base, fill, 2, out);
    CHECK(out == img2);
}

TEST_CASE("P4-01 5.6 and invariant 9: free land is the regions composite, at several years") {
    const T1Layers L;
    const wh::LoadedHistory& h = t1_history();
    const wi::TerritoryBase base =
        wi::TerritoryBase::build(L.elevation, &L.water, &L.river, &L.flow_dir, L.biome, L.region, L.table);
    const std::vector<std::uint8_t> regions1 =
        wi::render_composite_regions(L.elevation, &L.water, &L.river, &L.flow_dir, L.biome, L.region, 1, L.table);
    const std::vector<Rgb> colours = wi::civilization_colours(h.tables);
    // at 1250 r2 is free: (2, 1) shows B, the shaded grassland colour
    const std::vector<std::uint8_t> y1250 = wi::render_territory(base, wi::territory_fill(h.timeline, colours, 1250), 1);
    CHECK(pixel(y1250, 8, 2, 1) == Rgb{96, 160, 64});
    CHECK(pixel(y1250, 8, 2, 1) == pixel(regions1, 8, 2, 1));
    // at 299 only r1 is held
    const std::vector<std::uint8_t> y299 = wi::render_territory(base, wi::territory_fill(h.timeline, colours, 299), 1);
    for (std::uint32_t y = 0; y < 6; ++y) {
        for (std::uint32_t x = 0; x < 8; ++x) {
            const std::uint16_t v = std::get<std::vector<std::uint16_t>>(L.region.values)[y * 8U + x];
            if (v != 1) CHECK(pixel(y299, 8, x, y) == pixel(regions1, 8, x, y));
        }
    }
    // every pixel of every year is B or a shade of its holder's colour (invariant 9), here checked against
    // the expected B of the unfillable cells and the holder of each filled cell
    for (const std::int64_t year : {0, 300, 1000, 1200, 1300, 1600, 1800, 2000}) {
        CAPTURE(year);
        const auto fill = wi::territory_fill(h.timeline, colours, year);
        const std::vector<std::uint8_t> img = wi::render_territory(base, fill, 1);
        for (std::uint32_t y = 0; y < 6; ++y) {
            for (std::uint32_t x = 0; x < 8; ++x) {
                const Rgb b = pixel(regions1, 8, x, y);
                const Rgb got = pixel(img, 8, x, y);
                const std::uint16_t v = std::get<std::vector<std::uint16_t>>(L.region.values)[y * 8U + x];
                const wh::RegionHolding held =
                    (v == 0 || v == 5) ? wh::RegionHolding{} : h.timeline.region_holder(v, year);
                if (held.civ == 0) {
                    CHECK(got == b);
                } else {
                    const std::uint32_t f = wi::hillshade_factor(
                        std::get<std::vector<std::int16_t>>(L.elevation.values), 8, 6, x, y);
                    const Rgb c = wi::shade_colour(colours[static_cast<std::size_t>(held.civ - 1)], f);
                    CHECK((got == b || got == c));
                }
            }
        }
    }
}

TEST_CASE("P4-01 13.2 item 7, invariant 9: an empty fill equals render_composite_regions, byte for byte") {
    const T1Layers L;
    struct Variant {
        const char* what;
        const db::LayerGrid* water;
        const db::LayerGrid* river;
        const db::LayerGrid* flow_dir;
    };
    const Variant variants[] = {
        {"water, river, flow_dir", &L.water, &L.river, &L.flow_dir},
        {"water, river", &L.water, &L.river, nullptr},
        {"water only", &L.water, nullptr, nullptr},
        {"no water", nullptr, nullptr, nullptr},
    };
    for (const Variant& v : variants) {
        CAPTURE(std::string(v.what));
        const wi::TerritoryBase base =
            wi::TerritoryBase::build(L.elevation, v.water, v.river, v.flow_dir, L.biome, L.region, L.table);
        for (const std::uint32_t k : {1U, 3U}) {
            CAPTURE(k);
            const std::vector<std::uint8_t> want =
                wi::render_composite_regions(L.elevation, v.water, v.river, v.flow_dir, L.biome, L.region, k, L.table);
            CHECK(wi::render_territory(base, {}, k) == want);
            const std::vector<std::optional<Rgb>> none(7);
            CHECK(wi::render_territory(base, none, k) == want);
            // a short fill: values at or above its size are unfilled
            std::vector<std::optional<Rgb>> short_fill(2);
            short_fill[1] = Rgb{1, 2, 3};
            const std::vector<std::uint8_t> s = wi::render_territory(base, short_fill, k);
            CHECK(s.size() == want.size());
        }
    }
    // without river every water-1 cell is (52, 100, 200) and unfilled; (5, 0) is then plain held land
    {
        const wh::LoadedHistory& h = t1_history();
        const std::vector<Rgb> colours = wi::civilization_colours(h.tables);
        const auto fill = wi::territory_fill(h.timeline, colours, 1500);
        const wi::TerritoryBase nr =
            wi::TerritoryBase::build(L.elevation, &L.water, nullptr, &L.flow_dir, L.biome, L.region, L.table);
        const std::vector<std::uint8_t> img = wi::render_territory(nr, fill, 1);
        CHECK(pixel(img, 8, 2, 1) == Rgb{52, 100, 200});
        CHECK(pixel(img, 8, 4, 0) == Rgb{52, 100, 200});
        CHECK(pixel(img, 8, 5, 0) == Rgb{157, 80, 44});   // c2 at factor 251
        CHECK(pixel(img, 8, 1, 4) == Rgb{207, 168, 150});  // c4 at factor 256
        const wi::TerritoryBase nw =
            wi::TerritoryBase::build(L.elevation, nullptr, nullptr, nullptr, L.biome, L.region, L.table);
        const std::vector<std::uint8_t> img2 = wi::render_territory(nw, fill, 1);
        CHECK(pixel(img2, 8, 0, 1) == Rgb{52, 150, 70});    // the lake cell is plain held land
        CHECK(pixel(img2, 8, 1, 3) == Rgb{207, 168, 150});  // the river cell too, factor 256
    }
}

TEST_CASE("P4-01 5.3: render_territory and TerritoryBase::build refuse what they must") {
    const T1Layers L;
    const wi::TerritoryBase base =
        wi::TerritoryBase::build(L.elevation, &L.water, &L.river, &L.flow_dir, L.biome, L.region, L.table);
    CHECK_FALSE(fatal_text([&] { (void)wi::render_territory(base, {}, 0); }).empty());
    CHECK_FALSE(fatal_text([&] { (void)wi::render_territory(base, {}, 17); }).empty());
    CHECK(fatal_text([&] { (void)wi::render_territory(base, {}, 16); }).empty());
    std::vector<std::uint8_t> wrong(3U * 8U * 6U - 1U);
    CHECK_FALSE(fatal_text([&] { wi::render_territory(base, {}, 1, wrong); }).empty());
    std::vector<std::uint8_t> right(3U * 8U * 6U);
    CHECK(fatal_text([&] { wi::render_territory(base, {}, 1, right); }).empty());
    // composite_regions_problem reports a problem: region of type u8
    db::LayerGrid bad = db::make_layer_grid(db::ElementType::u8, 8, 6);
    REQUIRE(wi::composite_regions_problem(L.elevation, &L.water, &L.river, &L.flow_dir, L.biome, bad).has_value());
    CHECK_FALSE(fatal_text([&] {
                    (void)wi::TerritoryBase::build(L.elevation, &L.water, &L.river, &L.flow_dir, L.biome, bad, L.table);
                }).empty());
    // a region layer of another size
    db::LayerGrid small = db::make_layer_grid(db::ElementType::u16, 4, 6);
    CHECK_FALSE(fatal_text([&] {
                    (void)wi::TerritoryBase::build(L.elevation, &L.water, &L.river, &L.flow_dir, L.biome, small,
                                                   L.table);
                }).empty());
}

TEST_CASE("P4-01 13.2 item 7: the T1 world's stored layers are the fixture's, and render as the in-memory ones") {
    const db::WorldReader r = open_reader(t1_world());
    const WorldLayers stored = read_layers(r);
    const T1Layers L;
    CHECK(stored.elevation == L.elevation);
    CHECK(stored.region == L.region);
    REQUIRE(stored.water.has_value());
    CHECK(*stored.water == L.water);
    const wi::TerritoryBase a = territory_base(stored);
    const wi::TerritoryBase b =
        wi::TerritoryBase::build(L.elevation, &L.water, &L.river, &L.flow_dir, L.biome, L.region, L.table);
    const wh::LoadedHistory& h = t1_history();
    const auto fill = wi::territory_fill(h.timeline, wi::civilization_colours(h.tables), 1600);
    CHECK(wi::render_territory(a, fill, 1) == wi::render_territory(b, fill, 1));
}

}  // TEST_SUITE
