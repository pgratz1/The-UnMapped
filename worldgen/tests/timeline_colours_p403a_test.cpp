// Change spec P4-03a (docs/specs/p4-03a-territory-colour-followups.md r1) with P4-03 r4
// (docs/specs/p4-03-history-view.md 7.2, 7.3, 7.5, 8.2, 9.2, 11.1), section 7 "Tests required":
//   item 1  the four changed entries of 7.2 (the whole table is in timeline_borders_test.cpp);
//   item 2  the nearest-colour figures of P4-03a 2.2 for the changed entries and their effects (rules C1 to
//           C3 and the "min" columns are in timeline_borders_test.cpp);
//   item 3  L of 7.5 for every entry, the n = 10 set of 8.2, the r3 pale entries of 2.1, and on a fixture an
//           inner border at n = 10 (goblin 1) and at n = 12 (orc 0), and a hashed key (troll) by the same test;
//   item 4  free-land borders on hand-made 8 x 6 layers: free | free half(U) (values written here), free |
//           held, realm | realm, inside a realm, a free-free lake cell and river-painted cell, a cell whose E
//           neighbour is free and whose S neighbour is held; scales 1 and 2; all-zero and empty holders;
//           TerritoryBase::Cell::half;
//   item 5  render_territory(base, timeline, colours, year, scale[, out]) equal to the two-step path on T1 at
//           every year and on the 512 world cases, scales 1 and 3, both forms, and the same FatalError texts
//           in the same order.
// Item 6 (the property) is in timeline_property_test.cpp, item 7 (golden) in timeline_golden_test.cpp.
//
// Written from the spec only; the expected pixels were worked out by hand and are also checked by
// `p4_timeline_reference.py check` (check_luma, check_free_borders).

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "timeline_support.hpp"
#include "worldgen/core/error.hpp"

namespace {

using namespace tltest;
using wi::Rgb;

std::string fatal_text(const std::function<void()>& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

Rgb pixel(const std::vector<std::uint8_t>& img, std::uint32_t width, std::uint32_t x, std::uint32_t y) {
    const std::size_t i = 3U * (std::size_t{y} * width + x);
    REQUIRE(i + 2U < img.size());
    return Rgb{img[i], img[i + 1U], img[i + 2U]};
}

// P4-03 r4 7.5.
int luma(Rgb c) { return (77 * c.r + 150 * c.g + 29 * c.b) / 256; }

// D of 7.3 in int64 and its unit floor(sqrt(floor(D / 256))).
std::int64_t distance(Rgb a, Rgb b) {
    const std::int64_t r1 = a.r, g1 = a.g, b1 = a.b, r2 = b.r, g2 = b.g, b2 = b.b;
    const std::int64_t rm = (r1 + r2) / 2;
    return (512 + rm) * (r1 - r2) * (r1 - r2) + 1024 * (g1 - g2) * (g1 - g2) + (767 - rm) * (b1 - b2) * (b1 - b2);
}
std::int64_t units(std::int64_t d) {
    const std::int64_t q = d / 256;
    std::int64_t s = 0;
    while ((s + 1) * (s + 1) <= q) ++s;
    return s;
}

const std::array<const char*, 6> KEYS = {"elf", "goblin", "dwarf", "faerie", "human", "orc"};

// L of every entry, the table of 7.5 (r4).
const std::array<std::array<int, 4>, 6> LUMA_75 = {{{131, 179, 86, 163},
                                                    {185, 206, 124, 136},
                                                    {130, 182, 84, 111},
                                                    {119, 186, 73, 147},
                                                    {133, 180, 85, 160},
                                                    {82, 154, 51, 107}}};

// --- The free-border fixture (item 4) ---------------------------------------------------------------
//
// 8 x 6 cells. Regions, row by row:
//     1 1 2 2 3 3 4 4
//     1 1 2 2 3 3 4 4
//     1 1 2 2 3 3 4 4
//     5 6 6 6 7 7 8 8
//     5 5 6 6 7 7 8 9
//     5 5 6 6 7 7 8 9
// Elevation 10 everywhere but 30 at (2, 0): hillshade factor 266 at (1, 0) (gx = 30 - 10), 246 at (2, 0),
// (3, 0) and (2, 1), 256 elsewhere. Biome 1 (250, 99, 37) on region 1, biome 2 (224, 210, 150) (steppe)
// elsewhere. Water 0 but a lake (water 2) at (1, 1), and a river cell (water 1, river class 1) at (6, 5); no
// flow_dir. Holders: regions 3 and 4 by civilization 1 (goblin, ordinal 1: (235, 215, 90), L 206), 6 and 7
// by civilization 2 (orc, ordinal 0: (205, 30, 30), L 82); 1, 2, 5, 8 and 9 free.
//
// Border cells and their pixels:
//     (1, 0)  1 | 2, both free: half(shade((250, 99, 37), 266)) = half((255, 103, 38)) = (127, 51, 19)
//             (shading first: 250 * 266 overflows to 255, so halving first would give 130)
//     (0, 2)  1 | 5 (S), both free, factor 256: half((250, 99, 37)) = (125, 49, 18)
//     (7, 3)  8 | 9 (S) and (6, 4) 8 | 9 (E), both free: half((224, 210, 150)) = (112, 105, 75)
//     (1, 1)  lake on the 1 | 2 free border, kind 0: black
//     (6, 5)  river cell on the 8 | 9 free border, kind 0: black
//     (1, 2)  E neighbour 2 free, S neighbour 6 held: black
//     (3, 0 ... 2) 2 free | 3 held, (0, 3), (1, 4), (1, 5) 5 free | 6 held, (5, 3 ... 5) 7 held | 8 free,
//             (6, 2), (7, 2) 4 held | 8 free, (1, 3) 6 held | 5 free (S), (2, 2) 2 free | 6 held (S): black
//     (4, 2)  3 | 7 (S), two realms: black; (5, 2) E 4 of the realm, S 7 of another: black
//     (5, 0), (5, 1)  3 | 4 inside the goblin realm, n = 10: dim((235, 215, 90)) = (146, 134, 56)
//     (3, 3 ... 5)    6 | 7 inside the orc realm, n = 12: dim((205, 30, 30)) = (153, 22, 22)

constexpr std::uint32_t FW = 8, FH = 6;

const std::vector<std::uint16_t>& free_regions() {
    static const std::vector<std::uint16_t> v = {1, 1, 2, 2, 3, 3, 4, 4,  //
                                                 1, 1, 2, 2, 3, 3, 4, 4,  //
                                                 1, 1, 2, 2, 3, 3, 4, 4,  //
                                                 5, 6, 6, 6, 7, 7, 8, 8,  //
                                                 5, 5, 6, 6, 7, 7, 8, 9,  //
                                                 5, 5, 6, 6, 7, 7, 8, 9};
    return v;
}

struct FreeLayers {
    db::LayerGrid elevation = db::make_layer_grid(db::ElementType::i16, FW, FH);
    db::LayerGrid water = db::make_layer_grid(db::ElementType::u8, FW, FH);
    db::LayerGrid river = db::make_layer_grid(db::ElementType::u8, FW, FH);
    db::LayerGrid biome = db::make_layer_grid(db::ElementType::u8, FW, FH);
    db::LayerGrid region = db::make_layer_grid(db::ElementType::u16, FW, FH);
    std::vector<wi::BiomeColour> table = {{1, 0xFA6325}, {2, 0xE0D296}};
    FreeLayers() {
        std::vector<std::int16_t> e(FW * FH, 10);
        e[2] = 30;  // (2, 0)
        elevation.values = e;
        std::vector<std::uint8_t> w(FW * FH, 0), r(FW * FH, 0), b(FW * FH, 2);
        w[9] = 2;   // (1, 1) lake
        w[46] = 1;  // (6, 5) river
        r[46] = 1;
        water.values = w;
        river.values = r;
        for (std::size_t i = 0; i < b.size(); ++i) {
            if (free_regions()[i] == 1) b[i] = 1;
        }
        biome.values = b;
        region.values = free_regions();
    }
    [[nodiscard]] wi::TerritoryBase base() const {
        return wi::TerritoryBase::build(elevation, &water, &river, nullptr, biome, region, table);
    }
    [[nodiscard]] std::vector<std::uint8_t> regions(std::uint32_t k) const {
        return wi::render_composite_regions(elevation, &water, &river, nullptr, biome, region, k, table);
    }
};

const std::vector<std::int64_t> FREE_HOLDERS = {0, 0, 0, 1, 1, 0, 2, 2, 0, 0};

std::vector<std::optional<Rgb>> free_fill(Rgb realm1, Rgb realm2) {
    std::vector<std::optional<Rgb>> f(10);
    f[3] = realm1;
    f[4] = realm1;
    f[6] = realm2;
    f[7] = realm2;
    return f;
}

const Rgb K{0, 0, 0};
const Rgb GOBLIN1{235, 215, 90};
const Rgb ORC0{205, 30, 30};
const Rgb DIM_GOBLIN1{146, 134, 56};  // n = 10
const Rgb DIM_ORC0{153, 22, 22};      // n = 12

const Opened& t1() {
    static const Opened o = [] {
        db::WorldReader r = open_reader(t1_world());
        wh::LoadedHistory h = load(r);
        return Opened{std::move(r), std::move(h)};
    }();
    return o;
}

wi::TerritoryBase t1_base() {
    return wi::TerritoryBase::build(t1_layer("elevation"), &t1_layer("water"), &t1_layer("river"),
                                    &t1_layer("flow_dir"), t1_layer("biome"), t1_layer("region"),
                                    t1_biome_colours());
}

// The two-step path of P4-03 r4 9.2, sequenced as the spec orders the errors: the fill (year, then missing
// colour), then the holders, then the image (scale and size).
std::vector<std::uint8_t> two_step(const wi::TerritoryBase& base, const wh::Timeline& t, std::span<const Rgb> colours,
                                   std::int64_t year, std::uint32_t scale) {
    const std::vector<std::optional<Rgb>> fill = wi::territory_fill(t, colours, year);
    const std::vector<std::int64_t> holders = wi::territory_holders(t, year);
    return wi::render_territory(base, fill, holders, scale);
}
void two_step(const wi::TerritoryBase& base, const wh::Timeline& t, std::span<const Rgb> colours, std::int64_t year,
              std::uint32_t scale, std::span<std::uint8_t> out) {
    const std::vector<std::optional<Rgb>> fill = wi::territory_fill(t, colours, year);
    const std::vector<std::int64_t> holders = wi::territory_holders(t, year);
    wi::render_territory(base, fill, holders, scale, out);
}

// Both forms of the timeline overload against both forms of the two-step path.
void check_forms(const wi::TerritoryBase& base, const wh::Timeline& t, const std::vector<Rgb>& colours,
                 std::int64_t year, std::uint32_t scale, std::int64_t* bad) {
    const std::vector<std::uint8_t> want = two_step(base, t, colours, year, scale);
    const std::vector<std::uint8_t> got = wi::render_territory(base, t, colours, year, scale);
    std::vector<std::uint8_t> out(want.size(), 0xA5);
    wi::render_territory(base, t, colours, year, scale, out);
    std::vector<std::uint8_t> out2(want.size(), 0x5A);
    two_step(base, t, colours, year, scale, out2);
    if (!(got == want && out == want && out2 == want) && ++*bad <= 5) {
        FAIL_CHECK("timeline form differs at year " << year << ", scale " << scale);
    }
}

}  // namespace

TEST_SUITE("timeline") {

TEST_CASE("P4-03a 7 item 1: the four entries of 7.2 that r4 changes") {
    CHECK(wi::race_civilization_colour("goblin", 3) == Rgb{140, 155, 30});
    CHECK(wi::race_civilization_colour("goblin", 7) == Rgb{140, 155, 30});
    CHECK(wi::race_civilization_colour("goblin", 1) == Rgb{235, 215, 90});
    CHECK(wi::race_civilization_colour("elf", 1) == Rgb{130, 215, 130});
    CHECK(wi::race_civilization_colour("human", 1) == Rgb{100, 215, 215});
    CHECK(wi::race_civilization_colour("human", 5) == Rgb{100, 215, 215});
    // the r3 values are gone
    CHECK_FALSE(wi::race_civilization_colour("goblin", 3) == Rgb{205, 160, 60});
    CHECK_FALSE(wi::race_civilization_colour("goblin", 1) == Rgb{250, 235, 110});
    CHECK_FALSE(wi::race_civilization_colour("elf", 1) == Rgb{150, 230, 150});
    CHECK_FALSE(wi::race_civilization_colour("human", 1) == Rgb{125, 235, 230});
    // dwarf, faerie and orc entry 1 unchanged
    CHECK(wi::race_civilization_colour("dwarf", 1) == Rgb{245, 165, 105});
    CHECK(wi::race_civilization_colour("faerie", 1) == Rgb{215, 160, 250});
    CHECK(wi::race_civilization_colour("orc", 1) == Rgb{240, 120, 110});
    // race_base_colour unchanged (entry 0)
    CHECK(wi::race_base_colour("goblin") == Rgb{230, 195, 20});
    CHECK(wi::race_base_colour("elf") == Rgb{40, 190, 70});
    CHECK(wi::race_base_colour("human") == Rgb{0, 190, 195});
}

TEST_CASE("P4-03a 7 item 2: the nearest-colour figures of P4-03a 2.2") {
    std::vector<std::pair<std::string, Rgb>> bg;
    for (std::int64_t t = 0; t <= 30000; ++t) {
        bg.emplace_back("ocean " + std::to_string(t), wi::ramp_colour(wi::HYPSOMETRIC_OCEAN_STOPS, t));
    }
    bg.emplace_back("lake", Rgb{64, 128, 204});
    for (std::uint64_t k = 1; k <= 6; ++k) bg.emplace_back("river " + std::to_string(k), wi::composite_river_colour(k));
    {
        const db::WorldReader r = open_reader(world(property_world(0, 512)));
        auto rows = db::read_biome_rows(r);
        REQUIRE(rows.ok());
        REQUIRE(rows.value().size() == 12U);
        for (const db::BiomeRow& b : rows.value()) {
            bg.emplace_back("biome " + b.key, Rgb{static_cast<std::uint8_t>((b.colour >> 16) & 0xFF),
                                                  static_cast<std::uint8_t>((b.colour >> 8) & 0xFF),
                                                  static_cast<std::uint8_t>(b.colour & 0xFF)});
        }
    }
    struct Near {
        std::int64_t to_b = -1, in_race = -1, other_race = -1;
        std::string b_what;
    };
    const auto nearest = [&](std::size_t race, std::int64_t k) {
        const Rgb c = wi::race_civilization_colour(KEYS[race], k);
        Near n;
        for (const auto& [what, b] : bg) {
            const std::int64_t d = distance(c, b);
            if (n.to_b < 0 || d < n.to_b) {
                n.to_b = d;
                n.b_what = what;
            }
        }
        for (std::size_t r = 0; r < KEYS.size(); ++r) {
            for (std::int64_t j = 0; j < 4; ++j) {
                if (r == race && j == k) continue;
                const std::int64_t d = distance(c, wi::race_civilization_colour(KEYS[r], j));
                std::int64_t& slot = r == race ? n.in_race : n.other_race;
                if (slot < 0 || d < slot) slot = d;
            }
        }
        return n;
    };
    struct Row {
        std::size_t race;
        std::int64_t k;
        std::int64_t to_b, in_race, other_race;
        const char* b_what;
    };
    // the table of 2.2 (goblin 1, 3 = race 1; elf = 0; human = 4)
    for (const Row& row : {Row{1, 3, 99, 89, 102, "biome grassland"}, Row{1, 1, 89, 109, 103, "biome steppe"},
                           Row{0, 1, 114, 143, 142, "biome grassland"}, Row{4, 1, 96, 100, 143, "ocean 0"}}) {
        CAPTURE(std::string(KEYS[row.race]));
        CAPTURE(row.k);
        const Near n = nearest(row.race, row.k);
        CHECK(units(n.to_b) == row.to_b);
        CHECK(units(n.in_race) == row.in_race);
        CHECK(units(n.other_race) == row.other_race);
        CHECK(n.b_what == row.b_what);
    }
    // the effects on the rest of the table that 2.2 names
    CHECK(units(nearest(1, 0).in_race) == 109);     // goblin 0
    CHECK(units(nearest(1, 2).in_race) == 89);      // goblin 2, to the new goblin 3
    CHECK(units(nearest(0, 3).other_race) == 102);  // elf 3, to goblin 3
    CHECK(units(nearest(0, 0).other_race) == 180);  // elf 0
    // the table's minima: C1 89, C2 78, C3 90
    std::int64_t c1 = -1, c2 = -1, c3 = -1;
    for (std::size_t r = 0; r < KEYS.size(); ++r) {
        for (std::int64_t k = 0; k < 4; ++k) {
            const Near n = nearest(r, k);
            if (c1 < 0 || n.to_b < c1) c1 = n.to_b;
            if (c2 < 0 || n.in_race < c2) c2 = n.in_race;
            if (c3 < 0 || n.other_race < c3) c3 = n.other_race;
        }
    }
    CHECK(units(c1) == 89);
    CHECK(units(c2) == 78);
    CHECK(units(c3) == 90);
}

TEST_CASE("P4-03a 7 item 3: luma of 7.5 and the split of 8.2 at 175") {
    std::vector<std::string> high;
    int largest_low = -1, smallest_low = 256, n_low = 0;
    for (std::size_t r = 0; r < KEYS.size(); ++r) {
        for (std::int64_t k = 0; k < 4; ++k) {
            const Rgb c = wi::race_civilization_colour(KEYS[r], k);
            CAPTURE(std::string(KEYS[r]));
            CAPTURE(k);
            CHECK(luma(c) == LUMA_75[r][static_cast<std::size_t>(k)]);
            if (luma(c) > 175) {
                high.push_back(std::string(KEYS[r]) + " " + std::to_string(k));
            } else {
                ++n_low;
                largest_low = luma(c) > largest_low ? luma(c) : largest_low;
                smallest_low = luma(c) < smallest_low ? luma(c) : smallest_low;
            }
        }
    }
    // in table order (elf, goblin, dwarf, faerie, human, orc)
    CHECK(high == std::vector<std::string>{"elf 1", "goblin 0", "goblin 1", "dwarf 1", "faerie 1", "human 1"});
    CHECK(n_low == 18);
    CHECK(largest_low == 163);  // elf 3
    CHECK(smallest_low == 51);  // orc 2
    // 2.1: the r3 pale entries (L > 190) were goblin 1 225, human 1 201, elf 1 196
    CHECK(luma(Rgb{250, 235, 110}) == 225);
    CHECK(luma(Rgb{125, 235, 230}) == 201);
    CHECK(luma(Rgb{150, 230, 150}) == 196);
    // a realm of each side on a fixture: goblin 1 (L 206) at n = 10, orc 0 (L 82) at n = 12
    const FreeLayers L;
    const wi::TerritoryBase base = L.base();
    const std::vector<std::uint8_t> img = wi::render_territory(base, free_fill(GOBLIN1, ORC0), FREE_HOLDERS, 1);
    CHECK(pixel(img, FW, 5, 0) == DIM_GOBLIN1);
    CHECK(pixel(img, FW, 5, 1) == DIM_GOBLIN1);
    CHECK_FALSE(DIM_GOBLIN1 == (Rgb{176, 161, 67}));  // what n = 12 would give
    for (std::uint32_t y = 3; y < 6; ++y) {
        CAPTURE(y);
        CHECK(pixel(img, FW, 3, y) == DIM_ORC0);
    }
    CHECK_FALSE(DIM_ORC0 == (Rgb{128, 18, 18}));  // what n = 10 would give
    // the bound itself: 175 is not above 175. Hand-made fills: L(176, 176, 176) = 176, n = 10;
    // L(175, 175, 175) = 175, n = 12 (77 + 150 + 29 = 256, so L of a grey is its value)
    {
        const std::vector<std::uint8_t> a =
            wi::render_territory(base, free_fill(Rgb{176, 176, 176}, Rgb{175, 175, 175}), FREE_HOLDERS, 1);
        CHECK(pixel(a, FW, 5, 0) == Rgb{110, 110, 110});  // 176 * 10 / 16
        CHECK(pixel(a, FW, 3, 3) == Rgb{131, 131, 131});  // 175 * 12 / 16 = 131.25
    }
    // a hashed key follows the same test: troll 0 (194, 204, 68), L 185, n = 10: (121, 127, 42); troll 2
    // (121, 127, 42), L 115, n = 12: (90, 95, 31)
    const Rgb t0 = wi::race_civilization_colour("troll", 0), t2 = wi::race_civilization_colour("troll", 2);
    REQUIRE(t0 == Rgb{194, 204, 68});
    REQUIRE(t2 == Rgb{121, 127, 42});
    CHECK(luma(t0) == 185);
    CHECK(luma(t2) == 115);
    const std::vector<std::uint8_t> imt = wi::render_territory(base, free_fill(t0, t2), FREE_HOLDERS, 1);
    CHECK(pixel(imt, FW, 5, 0) == Rgb{121, 127, 42});
    CHECK(pixel(imt, FW, 3, 3) == Rgb{90, 95, 31});
}

TEST_CASE("P4-03a 7 item 4: free-land borders at half strength on hand-made layers") {
    const FreeLayers L;
    const wi::TerritoryBase base = L.base();
    REQUIRE(base.width() == FW);
    REQUIRE(base.height() == FH);
    REQUIRE(base.cells().size() == std::size_t{FW} * FH);
    const auto fill = free_fill(GOBLIN1, ORC0);
    const std::vector<std::uint8_t> reg1 = L.regions(1);
    // the kinds of 9.2
    const std::uint8_t kinds[FH][FW] = {{1, 2, 1, 2, 1, 2, 1, 1},  //
                                        {1, 0, 1, 2, 1, 2, 1, 1},  //
                                        {2, 2, 2, 2, 2, 2, 2, 2},  //
                                        {2, 2, 1, 2, 1, 2, 1, 2},  //
                                        {1, 2, 1, 2, 1, 2, 2, 1},  //
                                        {1, 2, 1, 2, 1, 2, 0, 1}};
    // Cell::half: half(U) on kind-2 cells, black elsewhere. U is steppe (224, 210, 150) at factor 256 on
    // every kind-2 cell but (1, 0) (biome 1 at factor 266), (0, 2) and (1, 2) (biome 1 at 256) and (3, 0)
    // (steppe at factor 246: U = (215, 202, 144), half (107, 101, 72))
    for (std::uint32_t y = 0; y < FH; ++y) {
        for (std::uint32_t x = 0; x < FW; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            const wi::TerritoryBase::Cell& c = base.cells()[std::size_t{y} * FW + x];
            REQUIRE(int{c.kind} == int{kinds[y][x]});
            Rgb want_half = K;
            if (c.kind == 2) {
                want_half = Rgb{112, 105, 75};
                if (x == 1 && y == 0) want_half = Rgb{127, 51, 19};
                if ((x == 0 || x == 1) && y == 2) want_half = Rgb{125, 49, 18};
                if (x == 3 && y == 0) want_half = Rgb{107, 101, 72};
            }
            CHECK(c.half == want_half);
        }
    }
    CHECK(int{base.cells()[1].factor} == 266);
    CHECK(int{base.cells()[3].factor} == 246);
    // the pixels with the holders
    struct P {
        std::uint32_t x, y;
        Rgb rgb;
        const char* what;
    };
    const P named[] = {
        {1, 0, {127, 51, 19}, "1 | 2 free: half(shade((250, 99, 37), 266)) = half((255, 103, 38))"},
        {0, 2, {125, 49, 18}, "1 | 5 (S) free: half((250, 99, 37))"},
        {6, 4, {112, 105, 75}, "8 | 9 free: half of steppe"},
        {7, 3, {112, 105, 75}, "8 | 9 (S) free: half of steppe"},
        {1, 1, K, "a lake cell on the 1 | 2 free border"},
        {6, 5, K, "a river-painted cell on the 8 | 9 free border"},
        {1, 2, K, "E neighbour 2 free, S neighbour 6 held"},
        {3, 0, K, "2 free | 3 held"},
        {0, 3, K, "5 free | 6 held"},
        {1, 3, K, "6 held | 5 free (S)"},
        {2, 2, K, "2 free | 6 held (S)"},
        {5, 3, K, "7 held | 8 free"},
        {6, 2, K, "4 held | 8 free (S)"},
        {4, 2, K, "3 | 7 (S): two realms"},
        {5, 2, K, "E 4 of the realm, S 7 of another"},
        {5, 0, DIM_GOBLIN1, "3 | 4 inside the goblin realm, n = 10"},
        {5, 1, DIM_GOBLIN1, "3 | 4 inside the goblin realm, n = 10"},
        {3, 3, DIM_ORC0, "6 | 7 inside the orc realm, n = 12"},
        {3, 4, DIM_ORC0, "6 | 7 inside the orc realm, n = 12"},
        {3, 5, DIM_ORC0, "6 | 7 inside the orc realm, n = 12"},
    };
    for (const std::uint32_t k : {1U, 2U}) {
        CAPTURE(k);
        const std::vector<std::uint8_t> img = wi::render_territory(base, fill, FREE_HOLDERS, k);
        REQUIRE(img.size() == 3U * k * k * FW * FH);
        for (const P& p : named) {
            CAPTURE(std::string(p.what));
            CAPTURE(p.x);
            CAPTURE(p.y);
            for (std::uint32_t dy = 0; dy < k; ++dy) {
                for (std::uint32_t dx = 0; dx < k; ++dx) CHECK(pixel(img, k * FW, k * p.x + dx, k * p.y + dy) == p.rgb);
            }
        }
        // every other cell: a fillable cell of a held region its shaded fill, every other cell B
        for (std::uint32_t y = 0; y < FH; ++y) {
            for (std::uint32_t x = 0; x < FW; ++x) {
                bool listed = false;
                for (const P& p : named) listed = listed || (p.x == x && p.y == y);
                const std::size_t i = std::size_t{y} * FW + x;
                const wi::TerritoryBase::Cell& c = base.cells()[i];
                if (listed) continue;
                CAPTURE(x);
                CAPTURE(y);
                Rgb want = pixel(reg1, FW, x, y);
                const std::uint16_t v = free_regions()[i];
                if (c.kind == 1 && fill[v].has_value()) want = wi::shade_colour(*fill[v], c.factor);
                if (c.kind == 2) CHECK(want == K);  // every unlisted kind-2 cell is a black border
                for (std::uint32_t dy = 0; dy < k; ++dy) {
                    for (std::uint32_t dx = 0; dx < k; ++dx) CHECK(pixel(img, k * FW, k * x + dx, k * y + dy) == want);
                }
            }
        }
        // the caller-buffer overload writes the same bytes
        std::vector<std::uint8_t> out(img.size(), 0x33);
        wi::render_territory(base, fill, FREE_HOLDERS, k, out);
        CHECK(out == img);
        // all-zero holders (every region free): the same free-free cells half, and every other kind-2 cell
        // too; the lake and river cells stay black; the rest is the regions composite but the kind-1 cells
        // of a region with a fill entry (fill is not tied to holders in the two-vector form)
        const std::vector<std::optional<Rgb>> none(10);
        const std::vector<std::int64_t> zeros(10, 0);
        const std::vector<std::uint8_t> regk = L.regions(k);
        for (const auto* f : {&none, &fill}) {
            const std::vector<std::uint8_t> z = wi::render_territory(base, *f, zeros, k);
            for (std::uint32_t y = 0; y < FH; ++y) {
                for (std::uint32_t x = 0; x < FW; ++x) {
                    CAPTURE(x);
                    CAPTURE(y);
                    const wi::TerritoryBase::Cell& c = base.cells()[std::size_t{y} * FW + x];
                    const std::uint16_t v = free_regions()[std::size_t{y} * FW + x];
                    Rgb want = c.kind == 2 ? c.half : pixel(regk, k * FW, k * x, k * y);
                    if (c.kind == 1 && (*f)[v].has_value()) want = wi::shade_colour(*(*f)[v], c.factor);
                    CHECK(pixel(z, k * FW, k * x, k * y) == want);
                    CHECK(pixel(z, k * FW, k * x + k - 1, k * y + k - 1) == want);
                }
            }
            CHECK(pixel(z, k * FW, k * 1, 0) == Rgb{127, 51, 19});
            CHECK(pixel(z, k * FW, k * 3, 0) == Rgb{107, 101, 72});
            CHECK(pixel(z, k * FW, k * 1, k * 1) == K);
            CHECK(pixel(z, k * FW, k * 6, k * 5) == K);
        }
        // a shorter all-zero vector, and holders past the end, count as free: the same image
        const std::vector<std::int64_t> one_zero = {0};
        CHECK(wi::render_territory(base, none, one_zero, k) == wi::render_territory(base, none, zeros, k));
        // an empty holder vector: every border black; with an empty fill the regions composite
        CHECK(wi::render_territory(base, {}, {}, k) == regk);
        CHECK(wi::render_territory(base, none, {}, k) == regk);
        const std::vector<std::uint8_t> nh = wi::render_territory(base, fill, {}, k);
        for (std::uint32_t y = 0; y < FH; ++y) {
            for (std::uint32_t x = 0; x < FW; ++x) {
                CAPTURE(x);
                CAPTURE(y);
                if (base.cells()[std::size_t{y} * FW + x].kind == 2) CHECK(pixel(nh, k * FW, k * x, k * y) == K);
            }
        }
    }
    // the 8.2 examples on steppe: every region of the 4 x 2 layers free gives (112, 105, 75) on the border
    // cells, and black with an empty holder vector
    {
        db::LayerGrid elevation = db::make_layer_grid(db::ElementType::i16, 4, 2);
        elevation.values = std::vector<std::int16_t>(8, 10);
        db::LayerGrid biome = db::make_layer_grid(db::ElementType::u8, 4, 2);
        biome.values = std::vector<std::uint8_t>(8, 1);
        db::LayerGrid region = db::make_layer_grid(db::ElementType::u16, 4, 2);
        region.values = std::vector<std::uint16_t>{1, 2, 3, 3, 1, 2, 3, 3};
        const std::vector<wi::BiomeColour> table = {{1, 0xE0D296}};
        const wi::TerritoryBase b4 = wi::TerritoryBase::build(elevation, nullptr, nullptr, nullptr, biome, region, table);
        const std::vector<std::optional<Rgb>> none(4);
        const std::vector<std::uint8_t> z = wi::render_territory(b4, none, std::vector<std::int64_t>{0, 0, 0, 0}, 1);
        const std::vector<std::uint8_t> e = wi::render_territory(b4, none, {}, 1);
        for (std::uint32_t y = 0; y < 2; ++y) {
            CAPTURE(y);
            CHECK(pixel(z, 4, 0, y) == Rgb{112, 105, 75});
            CHECK(pixel(z, 4, 1, y) == Rgb{112, 105, 75});
            CHECK(pixel(z, 4, 2, y) == Rgb{224, 210, 150});
            CHECK(pixel(e, 4, 0, y) == K);
            CHECK(pixel(e, 4, 1, y) == K);
        }
        // regions 1 and 2 held by an elf realm, 3 free: (1, .) black (one side held)
        std::vector<std::optional<Rgb>> f(4);
        f[1] = wi::race_civilization_colour("elf", 0);
        f[2] = f[1];
        const std::vector<std::uint8_t> h = wi::render_territory(b4, f, std::vector<std::int64_t>{0, 1, 1, 0}, 1);
        CHECK(pixel(h, 4, 1, 0) == K);
        CHECK(pixel(h, 4, 0, 0) == Rgb{30, 142, 52});
    }
}

TEST_CASE("P4-03a 7 item 5: the timeline form equals the two-step path on T1 at every year") {
    const wh::Timeline& t = t1().h.timeline;
    const std::vector<Rgb> colours = wi::civilization_colours(t1().h.tables);
    const wi::TerritoryBase base = t1_base();
    std::int64_t bad = 0;
    for (std::int64_t y = 0; y <= t.end_year(); ++y) {
        for (const std::uint32_t k : {1U, 3U}) check_forms(base, t, colours, y, k, &bad);
    }
    CHECK(bad == 0);
}

TEST_CASE("P4-03a 7 item 5: the timeline form gives the two-step path's FatalErrors, in its order") {
    const wh::Timeline& t = t1().h.timeline;
    const std::vector<Rgb> colours = wi::civilization_colours(t1().h.tables);
    const std::vector<Rgb> short_colours(colours.begin(), colours.begin() + 1);  // c1 only
    const wi::TerritoryBase base = t1_base();
    const std::size_t size1 = 3U * 8U * 6U;
    struct Case {
        const char* what;
        const std::vector<Rgb>* colours;
        std::int64_t year;
        std::uint32_t scale;
        std::size_t out_size;  // for the caller-buffer form
    };
    // at 1500 c2, c3 and c4 hold regions, so c1 alone is short; at 0 only c1 holds
    const Case cases[] = {
        {"year -1", &colours, -1, 1, size1},
        {"year E + 1", &colours, 2001, 1, size1},
        {"short colours", &short_colours, 1500, 1, size1},
        {"empty colours", nullptr, 1500, 1, size1},
        {"scale 0", &colours, 1500, 0, size1},
        {"scale 17", &colours, 1500, 17, size1},
        {"wrong out size", &colours, 1500, 1, size1 - 1},
        {"year before colours", &short_colours, -1, 1, size1},
        {"year before scale", &colours, 2001, 0, size1},
        {"year before size", &colours, -1, 1, 7},
        {"colours before scale", &short_colours, 1500, 17, size1},
        {"colours before size", &short_colours, 1500, 1, 7},
        {"scale and size", &colours, 1500, 0, 7},
    };
    const std::vector<Rgb> empty;
    for (const Case& c : cases) {
        CAPTURE(std::string(c.what));
        const std::vector<Rgb>& cs = c.colours != nullptr ? *c.colours : empty;
        const std::string want = fatal_text([&] { (void)two_step(base, t, cs, c.year, c.scale); });
        // the returning form has no buffer: a fault of the buffer size alone is no fault there
        CHECK(want.empty() == (std::string_view(c.what) == "wrong out size"));
        CHECK(fatal_text([&] { (void)wi::render_territory(base, t, cs, c.year, c.scale); }) == want);
        std::vector<std::uint8_t> out(c.out_size);
        const std::string want_out = fatal_text([&] { two_step(base, t, cs, c.year, c.scale, out); });
        CHECK_FALSE(want_out.empty());
        CHECK(fatal_text([&] { wi::render_territory(base, t, cs, c.year, c.scale, out); }) == want_out);
    }
    // the order of 9.2 (year, missing colour, then scale and size): a combined fault reports the earlier one
    const auto text = [&](const std::vector<Rgb>& cs, std::int64_t year, std::uint32_t scale) {
        return fatal_text([&] { (void)wi::render_territory(base, t, cs, year, scale); });
    };
    CHECK(text(short_colours, -1, 1) == text(colours, -1, 1));
    CHECK(text(empty, 2001, 0) == text(colours, 2001, 1));
    CHECK(text(short_colours, 1500, 0) == text(short_colours, 1500, 1));
    CHECK_FALSE(text(short_colours, 1500, 1) == text(colours, 1500, 0));
    CHECK_FALSE(text(colours, -1, 1) == text(short_colours, 1500, 1));
    {
        std::vector<std::uint8_t> out(7);
        const std::string size_text = fatal_text([&] { wi::render_territory(base, t, colours, 1500, 1, out); });
        CHECK_FALSE(size_text.empty());
        CHECK(fatal_text([&] { wi::render_territory(base, t, colours, -1, 1, out); }) == text(colours, -1, 1));
        CHECK(fatal_text([&] { wi::render_territory(base, t, short_colours, 1500, 1, out); }) ==
              text(short_colours, 1500, 1));
    }
    // no error where there is none: year 0 needs c1 only; scale 16 is allowed
    CHECK(fatal_text([&] { (void)wi::render_territory(base, t, short_colours, 0, 1); }).empty());
    CHECK(fatal_text([&] { (void)wi::render_territory(base, t, colours, 2000, 16); }).empty());
    {
        std::vector<std::uint8_t> out(size1);
        CHECK(fatal_text([&] { wi::render_territory(base, t, colours, 0, 1, out); }).empty());
    }
}

TEST_CASE("P4-03a 7 item 5: the timeline form equals the two-step path on the 512 world cases") {
    for (const WorldSpec& c : world_cases_512()) {
        CAPTURE(c.name);
        const db::WorldReader r = open_reader(world(c));
        const wh::LoadedHistory h = load(r);
        const WorldLayers layers = read_layers(r);
        const wi::TerritoryBase base = territory_base(layers);
        const std::vector<Rgb> colours = wi::civilization_colours(h.tables);
        std::int64_t bad = 0;
        // the years of the golden file's territory lines at scales 1 and 3; its year lines at scale 1
        for (const std::int64_t y : territory_years(h)) {
            for (const std::uint32_t k : {1U, 3U}) check_forms(base, h.timeline, colours, y, k, &bad);
        }
        for (const std::int64_t y : golden_years(h)) check_forms(base, h.timeline, colours, y, 1, &bad);
        CHECK(bad == 0);
    }
}

}  // TEST_SUITE
