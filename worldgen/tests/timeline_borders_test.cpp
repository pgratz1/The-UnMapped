// Spec P4-03 (docs/specs/p4-03-history-view.md) 11.1, the library items: items 1 and 3 (the colour table of
// 7.2, race_civilization_colour, race_base_colour), item 2 (rules C1 to C3 of 7.3 with D in int64, the set B
// built as 7.3 states it, the biome rows from a shipped-data world's table biome), item 4 (territory_holders
// on T1 at every year), items 5 and 6 (the inner borders of 8.2 on hand-made layers of 8 x 6 cells, the
// example of 8.2, and the images without holders or with every region free), item 7 (audit A3: Timeline::build
// refuses region_control runs out of year order or overlapping, with the texts of 9.4) and item 8
// (phase_kind_word; the report's phase words). Items 9 and 10 are in timeline_property_test.cpp and
// timeline_golden_test.cpp; the P4-01 tests whose colours or calls changed are adjusted in place.
//
// Written from the spec only; the expected pixels were worked out by hand and are also checked by
// `p4_timeline_reference.py check` (check_example_82, check_inner_borders, check_control_runs,
// check_distances).

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "timeline_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace {

using namespace tltest;
using wi::Rgb;

template <class F>
std::string fatal_text(F&& f) {
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

const Opened& t1() {
    static const Opened o = [] {
        db::WorldReader r = open_reader(t1_world());
        wh::LoadedHistory h = load(r);
        return Opened{std::move(r), std::move(h)};
    }();
    return o;
}

// --- 7.2 and 7.3 ----------------------------------------------------------------------------------

struct RaceRow {
    const char* key;
    std::array<Rgb, 4> entries;
    int min_to_b;      // the "min to B" column of 7.2
    int min_in_race;   // the "min within race" column
};

const std::array<RaceRow, 6>& table_72() {
    static const std::array<RaceRow, 6> t = {{
        {"elf", {{{40, 190, 70}, {150, 230, 150}, {0, 140, 40}, {130, 205, 40}}}, 109, 126},
        {"goblin", {{{230, 195, 20}, {250, 235, 110}, {170, 125, 0}, {205, 160, 60}}}, 88, 100},
        {"dwarf", {{{220, 105, 25}, {245, 165, 105}, {150, 65, 15}, {195, 80, 55}}}, 93, 78},
        {"faerie", {{{175, 70, 225}, {215, 160, 250}, {115, 35, 165}, {200, 110, 200}}}, 127, 97},
        {"human", {{{0, 190, 195}, {125, 235, 230}, {0, 120, 130}, {50, 215, 175}}}, 98, 94},
        {"orc", {{{205, 30, 30}, {240, 120, 110}, {135, 15, 20}, {230, 50, 80}}}, 165, 93},
    }};
    return t;
}

// D(a, b) of 7.3, in int64.
std::int64_t distance(Rgb a, Rgb b) {
    const std::int64_t r1 = a.r, g1 = a.g, b1 = a.b, r2 = b.r, g2 = b.g, b2 = b.b;
    const std::int64_t rm = (r1 + r2) / 2;
    return (512 + rm) * (r1 - r2) * (r1 - r2) + 1024 * (g1 - g2) * (g1 - g2) + (767 - rm) * (b1 - b2) * (b1 - b2);
}

// floor(sqrt(floor(D / 256))), the unit of the "min" columns.
std::int64_t units(std::int64_t d) {
    const std::int64_t q = d / 256;
    std::int64_t s = 0;
    while ((s + 1) * (s + 1) <= q) ++s;
    return s;
}

constexpr std::int64_t C1_MIN = 256 * 80 * 80;
constexpr std::int64_t C2_MIN = 256 * 70 * 70;
constexpr std::int64_t C3_MIN = 256 * 80 * 80;

Rgb rgb_of(std::int64_t c) {
    return Rgb{static_cast<std::uint8_t>((c >> 16) & 0xFF), static_cast<std::uint8_t>((c >> 8) & 0xFF),
               static_cast<std::uint8_t>(c & 0xFF)};
}

// --- 8.2 on hand-made layers ------------------------------------------------------------------------
//
// 8 x 6 cells, elevation 10 everywhere (hillshade factor 256), biome 1 (0x808000) everywhere, water 0 but a
// lake (water 2) at (0, 1); no river or flow_dir. Regions, row by row:
//     1 1 2 2 4 4 5 6
//     1 1 2 2 4 4 5 6
//     3 3 3 2 2 4 5 6
//     7 7 4 4 4 4 0 0
//     3 3 4 4 4 4 0 0
//     3 3 4 4 4 4 0 0
// Holders: 1, 2, 3 and 7 by civilization 1, 4 by civilization 2, 5 and 6 free. Fill: 1, 2, 3 elf 0
// (40, 190, 70), 4 dwarf 0 (220, 105, 25); 7 none (held, no fill). dim(elf 0) = (30, 142, 52).

constexpr std::uint32_t BW = 8, BH = 6;
const std::vector<std::uint16_t>& border_regions() {
    static const std::vector<std::uint16_t> v = {1, 1, 2, 2, 4, 4, 5, 6,  //
                                                 1, 1, 2, 2, 4, 4, 5, 6,  //
                                                 3, 3, 3, 2, 2, 4, 5, 6,  //
                                                 7, 7, 4, 4, 4, 4, 0, 0,  //
                                                 3, 3, 4, 4, 4, 4, 0, 0,  //
                                                 3, 3, 4, 4, 4, 4, 0, 0};
    return v;
}

struct BorderLayers {
    db::LayerGrid elevation = db::make_layer_grid(db::ElementType::i16, BW, BH);
    db::LayerGrid water = db::make_layer_grid(db::ElementType::u8, BW, BH);
    db::LayerGrid biome = db::make_layer_grid(db::ElementType::u8, BW, BH);
    db::LayerGrid region = db::make_layer_grid(db::ElementType::u16, BW, BH);
    std::vector<wi::BiomeColour> table = {{1, 0x808000}};
    BorderLayers() {
        elevation.values = std::vector<std::int16_t>(BW * BH, 10);
        std::vector<std::uint8_t> w(BW * BH, 0);
        w[8] = 2;  // (0, 1)
        water.values = w;
        biome.values = std::vector<std::uint8_t>(BW * BH, 1);
        region.values = border_regions();
    }
    [[nodiscard]] wi::TerritoryBase base() const {
        return wi::TerritoryBase::build(elevation, &water, nullptr, nullptr, biome, region, table);
    }
    [[nodiscard]] std::vector<std::uint8_t> regions(std::uint32_t k) const {
        return wi::render_composite_regions(elevation, &water, nullptr, nullptr, biome, region, k, table);
    }
};

const Rgb ELF0{40, 190, 70};
const Rgb DWARF0{220, 105, 25};
const Rgb DIM_ELF0{30, 142, 52};
const Rgb BLACK{0, 0, 0};
const Rgb GROUND{128, 128, 0};

std::vector<std::optional<Rgb>> border_fill() {
    std::vector<std::optional<Rgb>> f(8);
    f[1] = ELF0;
    f[2] = ELF0;
    f[3] = ELF0;
    f[4] = DWARF0;
    return f;
}
const std::vector<std::int64_t> BORDER_HOLDERS = {0, 1, 1, 1, 2, 0, 0, 1};

}  // namespace

TEST_SUITE("timeline") {

TEST_CASE("P4-03 11.1 item 1: race_civilization_colour and race_base_colour, the table of 7.2") {
    for (const RaceRow& row : table_72()) {
        CAPTURE(std::string(row.key));
        for (std::int64_t k = 0; k < 8; ++k) {
            CAPTURE(k);
            CHECK(wi::race_civilization_colour(row.key, k) == row.entries[static_cast<std::size_t>(k % 4)]);
        }
        CHECK(wi::race_civilization_colour(row.key, 4003) == row.entries[3]);
        CHECK(wi::race_base_colour(row.key) == row.entries[0]);
        // the four entries of a race are distinct (P4-01 invariant 8 still holds)
        for (std::size_t a = 0; a < 4; ++a) {
            for (std::size_t b = a + 1; b < 4; ++b) CHECK_FALSE(row.entries[a] == row.entries[b]);
        }
    }
    // keys without a row keep P4-01 5.5: the hash base and civilization_shade
    CHECK(wi::race_base_colour("troll") == Rgb{194, 204, 68});
    CHECK(wi::race_base_colour("") == Rgb{50, 157, 62});
    for (const char* key : {"troll", "", "Elf", "elf ", "orcs"}) {
        CAPTURE(std::string(key));
        for (std::int64_t k = 0; k < 8; ++k) {
            CAPTURE(k);
            CHECK(wi::race_civilization_colour(key, k) == wi::civilization_shade(wi::race_base_colour(key), k));
        }
    }
    CHECK(wi::race_civilization_colour("troll", 1) == Rgb{224, 229, 161});  // 194 + 61 * 8 / 16, ...
    CHECK(wi::race_civilization_colour("troll", 2) == Rgb{121, 127, 42});   // floor(c * 10 / 16)
    // keys are compared as bytes
    CHECK_FALSE(wi::race_base_colour("Elf") == Rgb{40, 190, 70});
    // a key holding a NUL byte is not "elf"
    CHECK_FALSE(wi::race_base_colour(std::string_view("elf\0", 4)) == Rgb{40, 190, 70});
    // FatalError for k < 0, for a shipped key and for one without a row
    CHECK_FALSE(fatal_text([] { (void)wi::race_civilization_colour("elf", -1); }).empty());
    CHECK_FALSE(fatal_text([] { (void)wi::race_civilization_colour("troll", -1); }).empty());
    CHECK(fatal_text([] { (void)wi::race_civilization_colour("elf", 0); }).empty());
}

TEST_CASE("P4-03 11.1 item 3: civilization_colours on T1 (two races interleaved by id)") {
    const std::vector<Rgb> c = wi::civilization_colours(t1().h.tables);
    REQUIRE(c.size() == 4U);
    CHECK(c[0] == Rgb{40, 190, 70});    // c1 elf, ordinal 0
    CHECK(c[1] == Rgb{220, 105, 25});   // c2 dwarf, ordinal 0
    CHECK(c[2] == Rgb{150, 230, 150});  // c3 elf, ordinal 1
    CHECK(c[3] == Rgb{245, 165, 105});  // c4 dwarf, ordinal 1
    // five civilizations of one race: the fifth reuses entry 0
    wh::HistoryTables tb = t1().h.tables;
    for (db::CivilizationRow& cv : tb.civilizations) cv.race = 1;  // every one elf
    db::CivilizationRow fifth = tb.civilizations.back();
    fifth.id = 5;
    tb.civilizations.push_back(fifth);
    const std::vector<Rgb> e = wi::civilization_colours(tb);
    REQUIRE(e.size() == 5U);
    for (std::size_t i = 0; i < 5; ++i) CHECK(e[i] == table_72()[0].entries[i % 4]);
}

TEST_CASE("P4-03 11.1 item 2: rules C1, C2 and C3 of 7.3 against the background set B") {
    // B: the ocean ramp for t = 0 ... 30000, the lake colour, the six river colours, and every row of table
    // biome of a world generated from the shipped data (twelve rows, water rows included)
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
        CHECK(rows.value().size() == 12U);
        for (const db::BiomeRow& b : rows.value()) bg.emplace_back("biome " + b.key, rgb_of(b.colour));
    }
    // 7.3's checks on D itself
    CHECK(distance(Rgb{0, 0, 0}, Rgb{255, 255, 255}) == (512 + 127 + 1024 + 767 - 127) * 65025LL);
    CHECK(distance(Rgb{1, 2, 3}, Rgb{200, 100, 50}) == distance(Rgb{200, 100, 50}, Rgb{1, 2, 3}));
    // the measurements quoted in 7.1 on P4-01's colours
    CHECK(units(distance(Rgb{50, 105, 200}, wi::composite_river_colour(2))) == 20);
    CHECK(units(distance(Rgb{32, 93, 43}, Rgb{46, 96, 52})) == 26);
    CHECK(units(distance(Rgb{222, 207, 152}, Rgb{224, 210, 150})) == 7);
    CHECK(units(distance(Rgb{70, 35, 19}, Rgb{87, 17, 15})) == 44);
    std::int64_t worst_b = -1;
    std::string worst_b_what;
    std::int64_t closest_across = -1;
    std::string closest_across_what;
    for (std::size_t ri = 0; ri < table_72().size(); ++ri) {
        const RaceRow& row = table_72()[ri];
        std::int64_t race_min_b = -1, race_min_in = -1;
        for (std::size_t k = 0; k < 4; ++k) {
            const Rgb c = row.entries[k];
            const std::string who = std::string(row.key) + " " + std::to_string(k);
            CAPTURE(who);
            for (const auto& [what, b] : bg) {
                const std::int64_t d = distance(c, b);
                if (d < C1_MIN) FAIL_CHECK("C1: " << who << " against " << what << ": " << units(d));
                if (race_min_b < 0 || d < race_min_b) race_min_b = d;
                if (worst_b < 0 || d < worst_b) {
                    worst_b = d;
                    worst_b_what = who + " / " + what;
                }
            }
            for (std::size_t j = 0; j < 4; ++j) {
                if (j == k) continue;
                const std::int64_t d = distance(c, row.entries[j]);
                if (d < C2_MIN) FAIL_CHECK("C2: " << who << " against entry " << j << ": " << units(d));
                if (race_min_in < 0 || d < race_min_in) race_min_in = d;
            }
            for (std::size_t rj = ri + 1; rj < table_72().size(); ++rj) {
                for (std::size_t j = 0; j < 4; ++j) {
                    const std::int64_t d = distance(c, table_72()[rj].entries[j]);
                    if (d < C3_MIN) {
                        FAIL_CHECK("C3: " << who << " against " << table_72()[rj].key << " " << j << ": " << units(d));
                    }
                    if (closest_across < 0 || d < closest_across) {
                        closest_across = d;
                        closest_across_what = who + " / " + table_72()[rj].key + " " + std::to_string(j);
                    }
                }
            }
        }
        // the "min" columns of 7.2
        CHECK(units(race_min_b) == row.min_to_b);
        CHECK(units(race_min_in) == row.min_in_race);
    }
    CHECK(units(worst_b) == 88);
    CHECK(worst_b_what == "goblin 1 / biome steppe");
    CHECK(units(closest_across) == 90);
    CHECK(closest_across_what == "dwarf 1 / orc 1");
}

TEST_CASE("P4-03 11.1 item 4: territory_holders equals the year state's holders at every year of T1") {
    const wh::Timeline& t = t1().h.timeline;
    const std::vector<Rgb> colours = wi::civilization_colours(t1().h.tables);
    wh::YearState st;
    std::int64_t bad = 0;
    for (std::int64_t y = 0; y <= t.end_year(); ++y) {
        const std::vector<std::int64_t> holders = wi::territory_holders(t, y);
        const auto fill = wi::territory_fill(t, colours, y);
        t.year_state(y, st);
        std::vector<std::int64_t> want(7, 0);  // largest region id 6, plus 1; region 5 has no row
        for (const wh::RegionHolding& r : st.regions) want[static_cast<std::size_t>(r.region)] = r.civ;
        if (holders != want || holders.size() != fill.size()) {
            if (++bad <= 5) FAIL_CHECK("territory_holders at " << y);
        }
    }
    CHECK(bad == 0);
    CHECK(wi::territory_holders(t, 0) == std::vector<std::int64_t>{0, 1, 0, 0, 0, 0, 0});
    CHECK(wi::territory_holders(t, 1600) == std::vector<std::int64_t>{0, 1, 3, 1, 4, 0, 0});
    CHECK(wi::territory_holders(t, 2000) == std::vector<std::int64_t>{0, 1, 3, 1, 0, 0, 0});
    // a year outside 0 to E: the timeline's text
    CHECK(fatal_text([&] { (void)wi::territory_holders(t, -1); }) == "timeline: year -1 outside 0 to 2000");
    CHECK(fatal_text([&] { (void)wi::territory_holders(t, 2001); }) == "timeline: year 2001 outside 0 to 2000");
    // a world whose timeline has no region: size 1
    const db::WorldReader r = open_reader(hrtest::fixture_world("h2-minimal"));
    const wh::LoadedHistory h2 = load(r);
    if (h2.tables.regions.empty()) {
        CHECK(wi::territory_holders(h2.timeline, 0) == std::vector<std::int64_t>{0});
    } else {
        std::int64_t largest = 0;
        for (const wh::RegionInfo& ri : h2.tables.regions) largest = ri.id > largest ? ri.id : largest;
        CHECK(wi::territory_holders(h2.timeline, 0).size() == static_cast<std::size_t>(largest + 1));
    }
}

TEST_CASE("P4-03 8.2: the 4 x 2 example") {
    db::LayerGrid elevation = db::make_layer_grid(db::ElementType::i16, 4, 2);
    elevation.values = std::vector<std::int16_t>(8, 10);
    db::LayerGrid biome = db::make_layer_grid(db::ElementType::u8, 4, 2);
    biome.values = std::vector<std::uint8_t>(8, 1);
    db::LayerGrid region = db::make_layer_grid(db::ElementType::u16, 4, 2);
    region.values = std::vector<std::uint16_t>{1, 2, 3, 3, 1, 2, 3, 3};
    const std::vector<wi::BiomeColour> table = {{1, 0x808000}};
    const wi::TerritoryBase base = wi::TerritoryBase::build(elevation, nullptr, nullptr, nullptr, biome, region, table);
    std::vector<std::optional<Rgb>> fill(4);
    fill[1] = wi::race_civilization_colour("elf", 0);
    fill[2] = wi::race_civilization_colour("elf", 0);
    fill[3] = wi::race_civilization_colour("elf", 1);
    const std::vector<std::int64_t> holders = {0, 1, 1, 2};
    const std::vector<std::uint8_t> img = wi::render_territory(base, fill, holders, 1);
    REQUIRE(img.size() == 24U);
    for (std::uint32_t y = 0; y < 2; ++y) {
        CAPTURE(y);
        CHECK(pixel(img, 4, 0, y) == Rgb{30, 142, 52});    // E neighbour 2, same civilization: dim
        CHECK(pixel(img, 4, 1, y) == BLACK);               // E neighbour 3, another civilization
        CHECK(pixel(img, 4, 2, y) == Rgb{150, 230, 150});  // E neighbour 3 = v: not a border
        CHECK(pixel(img, 4, 3, y) == Rgb{150, 230, 150});
    }
    // the cells of 9.2
    REQUIRE(base.cells().size() == 8U);
    const std::uint8_t kinds[] = {2, 2, 1, 1, 2, 2, 1, 1};
    const std::uint16_t easts[] = {2, 3, 0, 0, 2, 3, 0, 0};
    const std::uint16_t regions[] = {1, 2, 3, 3, 1, 2, 3, 3};
    for (std::size_t i = 0; i < 8; ++i) {
        CAPTURE(i);
        const wi::TerritoryBase::Cell& c = base.cells()[i];
        CHECK(int{c.kind} == int{kinds[i]});
        CHECK(c.east == easts[i]);
        CHECK(c.south == 0);
        CHECK(c.region == regions[i]);
        CHECK(int{c.factor} == 256);
    }
    // without holders P4-01's image: the inner border black
    const std::vector<std::uint8_t> p401 = wi::render_territory(base, fill, {}, 1);
    CHECK(pixel(p401, 4, 0, 0) == BLACK);
    CHECK(pixel(p401, 4, 2, 0) == Rgb{150, 230, 150});
}

TEST_CASE("P4-03 11.1 item 5: inner borders on hand-made layers") {
    const BorderLayers L;
    const wi::TerritoryBase base = L.base();
    REQUIRE(base.width() == BW);
    REQUIRE(base.height() == BH);
    const auto fill = border_fill();
    const std::vector<std::uint8_t> img = wi::render_territory(base, fill, BORDER_HOLDERS, 1);
    REQUIRE(img.size() == 3U * BW * BH);
    const Rgb E = ELF0, D = DWARF0, d = DIM_ELF0, K = BLACK, G = GROUND;
    const Rgb want[BH][BW] = {{E, d, E, K, D, K, K, G},  //
                              {K, d, d, K, K, K, K, G},  //
                              {d, d, K, K, K, K, K, G},  //
                              {K, K, D, D, D, D, G, G},  //
                              {E, K, D, D, D, D, G, G},  //
                              {E, K, D, D, D, D, G, G}};
    for (std::uint32_t y = 0; y < BH; ++y) {
        for (std::uint32_t x = 0; x < BW; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            CHECK(pixel(img, BW, x, y) == want[y][x]);
        }
    }
    // the named cases of 11.1 item 5, one by one
    CHECK(pixel(img, BW, 1, 0) == DIM_ELF0);  // regions 1 | 2 of one realm: dimmed
    CHECK(pixel(img, BW, 1, 1) == DIM_ELF0);  // E 2 and S 3, both of the realm: dimmed
    CHECK(pixel(img, BW, 0, 2) == DIM_ELF0);  // S 7 of the realm (7 has no fill; the cell's own region has)
    CHECK(pixel(img, BW, 3, 0) == BLACK);     // regions 2 | 4: two realms
    CHECK(pixel(img, BW, 5, 0) == BLACK);     // region 4 | 5: a realm and free land
    CHECK(pixel(img, BW, 6, 0) == BLACK);     // regions 5 | 6: two free regions
    CHECK(pixel(img, BW, 0, 1) == BLACK);     // a lake cell on a border inside the realm
    CHECK(pixel(img, BW, 2, 2) == BLACK);     // E 2 of the realm, S 4 of another
    CHECK(pixel(img, BW, 0, 3) == BLACK);     // region 7 held by the realm, S 3 of the realm, no fill for 7
    // the cells of 9.2: kind, region, east, south, factor
    const std::uint8_t kinds[BH][BW] = {{1, 2, 1, 2, 1, 2, 2, 1},  //
                                        {0, 2, 2, 2, 2, 2, 2, 1},  //
                                        {2, 2, 2, 2, 2, 2, 2, 1},  //
                                        {2, 2, 1, 1, 1, 1, 0, 0},  //
                                        {1, 2, 1, 1, 1, 1, 0, 0},  //
                                        {1, 2, 1, 1, 1, 1, 0, 0}};
    for (std::uint32_t y = 0; y < BH; ++y) {
        for (std::uint32_t x = 0; x < BW; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            const std::size_t i = std::size_t{y} * BW + x;
            const wi::TerritoryBase::Cell& c = base.cells()[i];
            CHECK(int{c.kind} == int{kinds[y][x]});
            CHECK(int{c.region} == (kinds[y][x] != 0 ? int{border_regions()[i]} : 0));
            CHECK(int{c.factor} == (kinds[y][x] != 0 ? 256 : 0));
            if (kinds[y][x] != 2) {
                CHECK(c.east == 0);
                CHECK(c.south == 0);
            }
        }
    }
    struct NS {
        std::uint32_t x, y;
        std::uint16_t east, south;
    };
    for (const NS& n : {NS{1, 0, 2, 0}, NS{1, 1, 2, 3}, NS{0, 2, 0, 7}, NS{2, 2, 2, 4}, NS{4, 2, 4, 4},
                        NS{0, 3, 0, 3}, NS{1, 3, 4, 3}, NS{1, 4, 4, 0}, NS{5, 2, 5, 0}, NS{4, 1, 0, 2}}) {
        CAPTURE(n.x);
        CAPTURE(n.y);
        const wi::TerritoryBase::Cell& c = base.cells()[std::size_t{n.y} * BW + n.x];
        CHECK(c.east == n.east);
        CHECK(c.south == n.south);
    }
    // the lake cell's base is black (the regions composite draws the border over the lake)
    CHECK(base.cells()[8].base == BLACK);
    // holders shorter than a region id: that region counts as free
    const std::vector<std::int64_t> short_holders(BORDER_HOLDERS.begin(), BORDER_HOLDERS.begin() + 2);
    const std::vector<std::uint8_t> s = wi::render_territory(base, fill, short_holders, 1);
    CHECK(pixel(s, BW, 1, 0) == BLACK);  // holders[2] past the end: free
    CHECK(pixel(s, BW, 0, 0) == ELF0);   // fillable cells do not depend on holders
    // scale 2: each cell's colour fills its 2 x 2 block; the caller-buffer overload writes the same bytes
    const std::vector<std::uint8_t> img2 = wi::render_territory(base, fill, BORDER_HOLDERS, 2);
    REQUIRE(img2.size() == 3U * 4U * BW * BH);
    for (std::uint32_t y = 0; y < BH; ++y) {
        for (std::uint32_t x = 0; x < BW; ++x) {
            for (std::uint32_t dy = 0; dy < 2; ++dy) {
                for (std::uint32_t dx = 0; dx < 2; ++dx) {
                    CAPTURE(x);
                    CAPTURE(y);
                    CHECK(pixel(img2, 2 * BW, 2 * x + dx, 2 * y + dy) == want[y][x]);
                }
            }
        }
    }
    std::vector<std::uint8_t> out(img2.size(), 0xCD);
    wi::render_territory(base, fill, BORDER_HOLDERS, 2, out);
    CHECK(out == img2);
}

TEST_CASE("P4-03 11.1 item 6: no holders, or every region free, is the regions composite") {
    const BorderLayers L;
    const wi::TerritoryBase base = L.base();
    const std::vector<std::optional<Rgb>> none(8);
    const std::vector<std::int64_t> zeros(8, 0);
    for (const std::uint32_t k : {1U, 3U}) {
        CAPTURE(k);
        const std::vector<std::uint8_t> want = L.regions(k);
        CHECK(wi::render_territory(base, {}, {}, k) == want);
        CHECK(wi::render_territory(base, none, zeros, k) == want);
        // every border black without holders (P4-01's image): fillable cells filled, the rest B
        const std::vector<std::uint8_t> p401 = wi::render_territory(base, border_fill(), {}, k);
        for (std::uint32_t y = 0; y < BH; ++y) {
            for (std::uint32_t x = 0; x < BW; ++x) {
                CAPTURE(x);
                CAPTURE(y);
                const std::size_t i = std::size_t{y} * BW + x;
                const wi::TerritoryBase::Cell& c = base.cells()[i];
                const std::uint16_t v = border_regions()[i];
                Rgb expect = pixel(want, k * BW, k * x, k * y);
                if (c.kind == 1 && v < 8 && border_fill()[v].has_value()) expect = *border_fill()[v];
                if (c.kind == 2) CHECK(expect == BLACK);
                CHECK(pixel(p401, k * BW, k * x, k * y) == expect);
            }
        }
        // holders that hold but a fill that has nothing: every border black, every cell B
        CHECK(wi::render_territory(base, none, BORDER_HOLDERS, k) == want);
    }
}

TEST_CASE("P4-03 11.1 item 7 (audit A3): Timeline::build refuses overlapping or unordered control runs") {
    const wh::HistoryView& view = t1().h.view;
    const wh::HistoryTables& base = t1().h.tables;
    // T1's rows: (1, 0, -), (2, 300, 1200), (2, 1300, -), (3, 1000, 1600), (3, 1600, -), (4, 1010, 1800)
    REQUIRE(base.region_control.size() == 6U);
    const auto row = [](std::int64_t region, std::int64_t from, std::optional<std::int64_t> to, std::int64_t civ,
                        std::int64_t event) {
        db::RegionControlRow r;
        r.region = region;
        r.from_year = from;
        r.to_year = to;
        r.civ = civ;
        r.event = event;
        return r;
    };
    const auto build_text = [&](const std::vector<db::RegionControlRow>& rows) {
        wh::HistoryTables tb = base;
        tb.region_control = rows;
        return fatal_text([&] { (void)wh::Timeline::build(view, tb); });
    };
    const std::vector<db::RegionControlRow>& rc = base.region_control;
    // T1 as stored: accepted (it has an equal boundary, region 3 changing hands in 1600)
    CHECK(build_text(rc).empty());
    // (a) one region's rows with from_year descending
    CHECK(build_text({rc[0], row(2, 1300, 1400, 3, 14), row(2, 300, 1200, 3, 4), rc[3], rc[4], rc[5]}) ==
          "timeline: region_control rows of region 2 overlap at year 300");
    // (b) p.to_year > r.from_year
    CHECK(build_text({rc[0], row(2, 300, 1350, 3, 4), rc[2], rc[3], rc[4], rc[5]}) ==
          "timeline: region_control rows of region 2 overlap at year 1300");
    // (c) p open, followed by another row
    CHECK(build_text({rc[0], row(2, 300, std::nullopt, 3, 4), rc[2], rc[3], rc[4], rc[5]}) ==
          "timeline: region_control rows of region 2 overlap at year 1300");
    // (d) to_year = from_year, and to_year before from_year
    CHECK(build_text({rc[0], rc[1], rc[2], rc[3], rc[4], row(4, 1010, 1010, 4, 9)}) ==
          "timeline: region_control row (4, 1010) ends at 1010, not after it starts");
    CHECK(build_text({rc[0], rc[1], rc[2], rc[3], rc[4], row(4, 1010, 900, 4, 9)}) ==
          "timeline: region_control row (4, 1010) ends at 900, not after it starts");
    // p.to_year = r.from_year is accepted, and the holder changes in that year
    {
        wh::HistoryTables tb = base;
        tb.region_control = {rc[0], row(2, 300, 1300, 3, 4), rc[2], rc[3], rc[4], rc[5]};
        std::optional<wh::Timeline> t;
        CHECK(fatal_text([&] { t = wh::Timeline::build(view, tb); }).empty());
        REQUIRE(t.has_value());
        const wh::RegionHolding a = t->region_holder(2, 1299);
        const wh::RegionHolding b = t->region_holder(2, 1300);
        CHECK(a.civ == 3);
        CHECK(a.since == 300);
        CHECK(b.civ == 3);
        CHECK(b.since == 1300);
        CHECK(b.event == 14);
    }
}

TEST_CASE("P4-03 11.1 item 8: phase_kind_word, and the report's phase words") {
    CHECK(wh::phase_kind_word(0).empty());
    CHECK(wh::phase_kind_word(1) == "opening");
    CHECK(wh::phase_kind_word(2) == "quiet");
    CHECK(wh::phase_kind_word(3) == "flare_up");
    CHECK(wh::phase_kind_word(4) == "final");
    CHECK(wh::phase_kind_word(5).empty());
    CHECK(wh::phase_kind_word(-1).empty());
    CHECK(wh::phase_kind_word(9223372036854775807LL).empty());
    static_assert(noexcept(wh::phase_kind_word(1)));
    // the report's [phases] lines of T1 name each phase by the same word
    const auto rep = wi::build_history_report(t1().reader, wi::HistoryLevel::full);
    REQUIRE(rep.ok());
    for (const db::PhaseRow& p : t1().h.tables.phases) {
        const std::string line = "p" + std::to_string(p.id) + " a" + std::to_string(p.age) + " " +
                                 std::string(wh::phase_kind_word(p.kind)) + " " + std::to_string(p.start_year) + "-" +
                                 std::to_string(p.end_year);
        CAPTURE(line);
        CHECK(rep.value().find(line) != std::string::npos);
    }
}

}  // TEST_SUITE
