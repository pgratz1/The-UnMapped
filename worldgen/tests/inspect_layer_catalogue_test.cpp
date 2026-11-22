// Spec P4-02 (layer catalogue and region names in the library), section 9
// tests 1 to 8: the catalogue table of 4.2, the group titles of 4.1, the
// palette mapping (invariant 3), the display forms of 4.3, cell_value, the
// legends of 4.4, the available composites of 4.5 and the region names of
// 4.6, on hand-made inputs. The generated-world cases (test 9, invariant 2)
// are in inspect_layer_catalogue_worlds_test.cpp.
//
// Written from the spec only: every expected string and colour below is a
// literal taken from the tables of P4-02 4.2 to 4.4 and the stops and colour
// comments of render.hpp (which 4.4 names as the source of the colours),
// never a value computed by the code under test. Where the spec reads two
// ways the test asserts only what both readings share; the places are marked
// "AMBIGUITY" and listed in the test writer's report.

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/layer_catalogue.hpp"
#include "worldgen/inspect/render.hpp"

namespace {

namespace wi = worldgen::inspect;
namespace wdb = worldgen::db;
using wdb::ElementType;
using wdb::LayerGrid;
using wi::LayerGroup;
using wi::Palette;

// The degree sign in UTF-8, kept apart from the next literal so that a
// following hex digit ("C") is not read into the escape.
#define DEG "\xC2\xB0"

constexpr std::int64_t I64_MIN = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t I64_MAX = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t U32_MAX = 4294967295;

std::uint32_t pack(wi::Rgb c) {
    return (static_cast<std::uint32_t>(c.r) << 16U) | (static_cast<std::uint32_t>(c.g) << 8U) |
           static_cast<std::uint32_t>(c.b);
}

template <class T>
LayerGrid grid_of(std::uint32_t w, std::uint32_t h, std::vector<T> cells) {
    LayerGrid g;
    g.width = w;
    g.height = h;
    g.values = std::move(cells);
    return g;
}

// One expected legend row.
struct Row {
    std::string label;
    std::vector<std::uint32_t> colours;
};

std::string hex(std::uint32_t c) {
    static constexpr char DIGITS[] = "0123456789ABCDEF";
    std::string s = "0x";
    for (int shift = 20; shift >= 0; shift -= 4) {
        s += DIGITS[(c >> static_cast<unsigned>(shift)) & 0xFU];
    }
    return s;
}

std::string colours_text(const std::vector<std::uint32_t>& cs) {
    std::string s = "{";
    for (std::size_t i = 0; i < cs.size(); ++i) {
        s += (i == 0 ? "" : ", ") + hex(cs[i]);
    }
    return s + "}";
}

void check_legend(const std::vector<wi::LegendEntry>& got, const std::vector<Row>& want) {
    CHECK(got.size() == want.size());
    const std::size_t n = got.size() < want.size() ? got.size() : want.size();
    for (std::size_t i = 0; i < n; ++i) {
        CAPTURE(i);
        CHECK(got[i].label == want[i].label);
        const std::string g = colours_text(got[i].colours);
        const std::string w = colours_text(want[i].colours);
        CHECK(g == w);
    }
}

// --- 4.2: the expected table, literally -------------------------------------

struct Special {
    std::int64_t value;
    std::string_view label;
};

struct Expected {
    std::string_view name;
    LayerGroup group;
    ElementType type;
    std::int64_t min;
    std::int64_t max;
    bool max_is_cell_count;
    std::int64_t scale;
    std::string_view display_unit;
    std::string_view unit;
    std::vector<Special> special;
    Palette palette;
};

const std::vector<Expected>& expected_table() {
    // AMBIGUITY (special labels): 4.2 writes the special column as
    // "<value> <label>"; the label is read as the text after the value.
    static const std::vector<Expected> t = {
        {"elevation", LayerGroup::terrain, ElementType::i16, -30000, 30000, false, 10, "m",
         "decimetres above sea level", {}, Palette::hypsometric},
        {"water", LayerGroup::water, ElementType::u8, 0, 3, false, 1, "",
         "category: 0 dry land, 1 river, 2 lake, 3 ocean",
         {{0, "dry land"}, {1, "river"}, {2, "lake"}, {3, "ocean"}}, Palette::water},
        {"water_depth", LayerGroup::water, ElementType::u16, 0, 32768, false, 10, "m", "decimetres of standing water",
         {{0, "no standing water"}}, Palette::gray},
        {"flow_dir", LayerGroup::water, ElementType::u8, 0, 8, false, 1, "", "D8 code of the downstream neighbour",
         {{8, "none (ocean)"}}, Palette::direction},
        {"flow_accum", LayerGroup::water, ElementType::u32, 0, U32_MAX, true, 1, "",
         "cells draining through the cell (x 256 m2)", {{0, "ocean"}}, Palette::log2},
        {"river", LayerGroup::water, ElementType::u8, 0, 15, false, 1, "", "river size class, 0 none",
         {{0, "not a river"}}, Palette::river},
        {"lake", LayerGroup::water, ElementType::u32, 0, U32_MAX, false, 1, "", "lake id, 0 none", {{0, "none"}},
         Palette::ids},
        {"coast_dist", LayerGroup::water, ElementType::i16, -32767, 32767, false, 1, "m",
         "metres to the coast, < 0 at sea", {}, Palette::signed_distance},
        {"temperature", LayerGroup::climate, ElementType::i16, -600, 500, false, 10, DEG "C",
         "annual mean, tenths of a degree Celsius", {}, Palette::temperature},
        {"rainfall", LayerGroup::climate, ElementType::u16, 0, 10000, false, 1, "mm/yr",
         "annual precipitation, millimetres", {}, Palette::rainfall},
        {"biome", LayerGroup::biomes, ElementType::u8, 1, 254, false, 1, "", "biome code of table biome", {},
         Palette::biome},
        {"fertility", LayerGroup::resources, ElementType::u8, 0, 255, false, 1, "/ 255", "score 0 to 255",
         {{0, "none"}}, Palette::score},
        {"ore", LayerGroup::resources, ElementType::u8, 0, 255, false, 1, "/ 255", "score 0 to 255", {{0, "none"}},
         Palette::score},
        {"timber", LayerGroup::resources, ElementType::u8, 0, 255, false, 1, "/ 255", "score 0 to 255",
         {{0, "none"}}, Palette::score},
        {"fresh_water", LayerGroup::resources, ElementType::u8, 0, 255, false, 1, "/ 255", "score 0 to 255",
         {{0, "none"}}, Palette::score},
        {"forage", LayerGroup::resources, ElementType::u8, 0, 255, false, 1, "/ 255", "score 0 to 255",
         {{0, "none"}}, Palette::score},
        {"habitability", LayerGroup::resources, ElementType::u8, 0, 255, false, 1, "/ 255", "score 0 to 255",
         {{0, "none"}}, Palette::score},
        {"walk_area", LayerGroup::resources, ElementType::u16, 0, 65535, false, 1, "", "walkable area id, 0 none",
         {{0, "none"}}, Palette::ids},
        {"region", LayerGroup::regions, ElementType::u16, 0, 65535, false, 1, "", "region id of table region, 0 ocean",
         {{0, "none (ocean)"}}, Palette::ids},
    };
    return t;
}

// The mapping of registered_palette documented in render.hpp before this
// stage (invariant 3), written out again independently of expected_table.
const std::vector<std::pair<std::string_view, Palette>>& documented_palettes() {
    static const std::vector<std::pair<std::string_view, Palette>> m = {
        {"elevation", Palette::hypsometric}, {"water", Palette::water},
        {"water_depth", Palette::gray},      {"flow_dir", Palette::direction},
        {"flow_accum", Palette::log2},       {"river", Palette::river},
        {"lake", Palette::ids},              {"coast_dist", Palette::signed_distance},
        {"temperature", Palette::temperature}, {"rainfall", Palette::rainfall},
        {"biome", Palette::biome},           {"fertility", Palette::score},
        {"ore", Palette::score},             {"timber", Palette::score},
        {"fresh_water", Palette::score},     {"forage", Palette::score},
        {"habitability", Palette::score},    {"walk_area", Palette::ids},
        {"region", Palette::ids},
    };
    return m;
}

const std::array<std::string_view, 9> UNKNOWN_NAMES = {
    "", "Elevation", "water_level", "test.ramp", "elevation ", " elevation", "regions", "score", "map"};

bool ascii_plus_degree(std::string_view s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c >= 0x20U && c <= 0x7EU) {
            continue;
        }
        if (c == 0xC2U && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0xB0U) {
            ++i;
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

// --- noexcept as declared in section 4 --------------------------------------

static_assert(noexcept(wi::layer_group_title(LayerGroup::other)));
static_assert(noexcept(wi::layer_catalogue()));
static_assert(noexcept(wi::find_layer_spec(std::string_view{})));
static_assert(noexcept(wi::layer_group(std::string_view{})));
static_assert(noexcept(wi::layer_unit(std::string_view{})));
static_assert(noexcept(wi::composite_description(wi::Composite::map)));
static_assert(noexcept(wi::region_name(std::span<const wi::RegionRecord>{}, std::int64_t{0})));
static_assert(wi::LAYER_GROUP_COUNT == 8U);

TEST_SUITE("layer-catalogue") {

// --- Test 1: the table (4.1, 4.2, invariant 1) ------------------------------

TEST_CASE("P4-02 4.1: group enumerators and titles, out-of-range gives Other") {
    CHECK(static_cast<int>(LayerGroup::composites) == 0);
    CHECK(static_cast<int>(LayerGroup::terrain) == 1);
    CHECK(static_cast<int>(LayerGroup::water) == 2);
    CHECK(static_cast<int>(LayerGroup::climate) == 3);
    CHECK(static_cast<int>(LayerGroup::biomes) == 4);
    CHECK(static_cast<int>(LayerGroup::resources) == 5);
    CHECK(static_cast<int>(LayerGroup::regions) == 6);
    CHECK(static_cast<int>(LayerGroup::other) == 7);
    const std::array<std::string_view, 8> titles = {"Composites", "Terrain", "Water",   "Climate",
                                                    "Biomes",     "Resources", "Regions", "Other"};
    for (std::size_t i = 0; i < wi::LAYER_GROUP_COUNT; ++i) {
        CAPTURE(i);
        CHECK(wi::layer_group_title(static_cast<LayerGroup>(static_cast<std::uint8_t>(i))) == titles[i]);
    }
    for (const std::uint8_t bad : {std::uint8_t{8}, std::uint8_t{9}, std::uint8_t{127}, std::uint8_t{255}}) {
        CAPTURE(static_cast<int>(bad));
        CHECK(wi::layer_group_title(static_cast<LayerGroup>(bad)) == "Other");
    }
}

TEST_CASE("P4-02 4.2, invariant 1: 19 entries in layer id order, every field as the table") {
    const auto cat = wi::layer_catalogue();
    const auto& want = expected_table();
    REQUIRE(cat.size() == 19U);
    REQUIRE(want.size() == 19U);
    std::set<std::string_view> names;
    for (std::size_t i = 0; i < cat.size(); ++i) {
        const wi::LayerSpec& s = cat[i];
        const Expected& e = want[i];
        CAPTURE(i);
        CAPTURE(e.name);
        CHECK(s.name == e.name);
        names.insert(s.name);
        CHECK(s.group == e.group);
        CHECK(s.type == e.type);
        CHECK(s.min == e.min);
        CHECK(s.max == e.max);
        CHECK(s.max_is_cell_count == e.max_is_cell_count);
        CHECK(s.scale == e.scale);
        CHECK(s.display_unit == e.display_unit);
        CHECK(s.unit == e.unit);
        CHECK(s.palette == e.palette);
        REQUIRE(s.special.size() == e.special.size());
        for (std::size_t k = 0; k < e.special.size(); ++k) {
            CAPTURE(k);
            CHECK(s.special[k].value == e.special[k].value);
            CHECK(s.special[k].label == e.special[k].label);
            if (k > 0) {
                CHECK(s.special[k - 1].value < s.special[k].value);  // ascending value
            }
        }
        // description: non-empty, at most 160 bytes, ASCII plus the degree sign
        CHECK_FALSE(s.description.empty());
        CHECK(s.description.size() <= 160U);
        CHECK(ascii_plus_degree(s.description));
        // the range lies inside the catalogue type
        CHECK(s.min <= s.max);
    }
    CHECK(names.size() == 19U);
}

TEST_CASE("P4-02 4.2: the degree sign is the two UTF-8 bytes C2 B0") {
    const wi::LayerSpec* t = wi::find_layer_spec("temperature");
    REQUIRE(t != nullptr);
    REQUIRE(t->display_unit.size() == 3U);
    CHECK(static_cast<unsigned char>(t->display_unit[0]) == 0xC2U);
    CHECK(static_cast<unsigned char>(t->display_unit[1]) == 0xB0U);
    CHECK(t->display_unit[2] == 'C');
}

TEST_CASE("P4-02 4.2: find_layer_spec, layer_group and layer_unit for every catalogue name") {
    for (const Expected& e : expected_table()) {
        CAPTURE(e.name);
        const wi::LayerSpec* s = wi::find_layer_spec(e.name);
        REQUIRE(s != nullptr);
        CHECK(s->name == e.name);
        CHECK(wi::layer_group(e.name) == e.group);
        CHECK(wi::layer_unit(e.name) == e.unit);
        // a lookup by a copy of the name, not by the same pointer
        const std::string copy(e.name);
        CHECK(wi::find_layer_spec(copy) == s);
    }
}

// --- Test 2: palettes and unknown names (4.2, 5, invariant 3) ---------------

TEST_CASE("P4-02 4.2, invariant 3: registered_palette and the entry palette equal the documented mapping") {
    for (const auto& [name, palette] : documented_palettes()) {
        CAPTURE(name);
        CHECK(wi::registered_palette(name) == palette);
        const wi::LayerSpec* s = wi::find_layer_spec(name);
        REQUIRE(s != nullptr);
        CHECK(s->palette == palette);
    }
}

TEST_CASE("P4-02 5: an unknown name gives null, Other, an empty unit, gray and the plain decimal") {
    for (const std::string_view name : UNKNOWN_NAMES) {
        CAPTURE(name);
        CHECK(wi::find_layer_spec(name) == nullptr);
        CHECK(wi::layer_group(name) == LayerGroup::other);
        CHECK(wi::layer_unit(name).empty());
        CHECK(wi::registered_palette(name) == Palette::gray);
        CHECK(wi::format_layer_value(name, 0) == "0");
        CHECK(wi::format_layer_value(name, -7) == "-7");
        CHECK(wi::format_layer_value(name, I64_MIN) == "-9223372036854775808");
        CHECK(wi::format_layer_value(name, I64_MAX) == "9223372036854775807");
    }
}

// --- Test 3: format_layer_value (4.3) ---------------------------------------

TEST_CASE("P4-02 4.3: format_layer_value, a literal table") {
    struct Case {
        std::string_view name;
        std::int64_t v;
        std::string_view text;
    };
    const std::vector<Case> cases = {
        // elevation: T(v) m, plus " (sea)" when v < 0
        {"elevation", 0, "0.0 m"},
        {"elevation", 1234, "123.4 m"},
        {"elevation", 1, "0.1 m"},
        {"elevation", 10, "1.0 m"},
        {"elevation", -5, "-0.5 m (sea)"},
        {"elevation", -1, "-0.1 m (sea)"},
        {"elevation", -15, "-1.5 m (sea)"},
        {"elevation", -10, "-1.0 m (sea)"},
        {"elevation", 30000, "3000.0 m"},
        {"elevation", -30000, "-3000.0 m (sea)"},
        {"elevation", 32767, "3276.7 m"},
        {"elevation", -32768, "-3276.8 m (sea)"},
        {"elevation", I64_MAX, "922337203685477580.7 m"},
        {"elevation", I64_MIN, "-922337203685477580.8 m (sea)"},
        // water: categories, else "category <v>"
        {"water", 0, "dry land"},
        {"water", 1, "river"},
        {"water", 2, "lake"},
        {"water", 3, "ocean"},
        {"water", 4, "category 4"},
        {"water", 255, "category 255"},
        {"water", -1, "category -1"},
        // water_depth: 0 / T(v) m deep
        {"water_depth", 0, "0 m (no standing water)"},
        {"water_depth", 1, "0.1 m deep"},
        {"water_depth", 15, "1.5 m deep"},
        {"water_depth", 1731, "173.1 m deep"},
        {"water_depth", 32768, "3276.8 m deep"},
        {"water_depth", 65535, "6553.5 m deep"},
        {"water_depth", -5, "-0.5 m deep"},
        // flow_dir: D8 code / none (ocean) / code <v>
        {"flow_dir", 0, "E"},
        {"flow_dir", 1, "SE"},
        {"flow_dir", 2, "S"},
        {"flow_dir", 3, "SW"},
        {"flow_dir", 4, "W"},
        {"flow_dir", 5, "NW"},
        {"flow_dir", 6, "N"},
        {"flow_dir", 7, "NE"},
        {"flow_dir", 8, "none (ocean)"},
        {"flow_dir", 9, "code 9"},
        {"flow_dir", 255, "code 255"},
        {"flow_dir", -1, "code -1"},
        // flow_accum: <= 0 / <v> cells, A(v) km2 upstream; A(n) = H((256 n + 5000) / 10000)
        {"flow_accum", 0, "0 (ocean)"},
        {"flow_accum", -1, "0 (ocean)"},
        {"flow_accum", I64_MIN, "0 (ocean)"},
        {"flow_accum", 1, "1 cells, 0.00 km2 upstream"},
        {"flow_accum", 19, "19 cells, 0.00 km2 upstream"},    // 9864 / 10000 = 0
        {"flow_accum", 20, "20 cells, 0.01 km2 upstream"},    // 10120 / 10000 = 1
        {"flow_accum", 58, "58 cells, 0.01 km2 upstream"},    // 19848 -> 1
        {"flow_accum", 59, "59 cells, 0.02 km2 upstream"},    // 20104 -> 2 (round half up boundary)
        {"flow_accum", 4096, "4096 cells, 1.05 km2 upstream"},
        {"flow_accum", 16559, "16559 cells, 4.24 km2 upstream"},  // 4244104 -> 424
        {"flow_accum", 262144, "262144 cells, 67.11 km2 upstream"},  // 67113864 -> 6711
        {"flow_accum", U32_MAX, "4294967295 cells, 1099511.63 km2 upstream"},
        // river: not a river / class <v>
        {"river", 0, "not a river"},
        {"river", 1, "class 1"},
        {"river", 15, "class 15"},
        {"river", 255, "class 255"},
        // lake and walk_area: none / id <v>
        {"lake", 0, "none"},
        {"lake", 1, "id 1"},
        {"lake", U32_MAX, "id 4294967295"},
        {"walk_area", 0, "none"},
        {"walk_area", 3, "id 3"},
        {"walk_area", 65535, "id 65535"},
        // coast_dist: < 0 offshore by magnitude / >= 0 from the sea
        {"coast_dist", -48, "48 m offshore"},
        {"coast_dist", -1, "1 m offshore"},
        {"coast_dist", 0, "0 m from the sea"},
        {"coast_dist", 1, "1 m from the sea"},
        {"coast_dist", 32767, "32767 m from the sea"},
        {"coast_dist", -32767, "32767 m offshore"},
        {"coast_dist", -32768, "32768 m offshore"},
        {"coast_dist", I64_MIN, "9223372036854775808 m offshore"},
        {"coast_dist", I64_MAX, "9223372036854775807 m from the sea"},
        // temperature: T(v) degrees C
        {"temperature", -15, "-1.5 " DEG "C"},
        {"temperature", -5, "-0.5 " DEG "C"},
        {"temperature", 0, "0.0 " DEG "C"},
        {"temperature", 102, "10.2 " DEG "C"},
        {"temperature", -600, "-60.0 " DEG "C"},
        {"temperature", 500, "50.0 " DEG "C"},
        {"temperature", I64_MIN, "-922337203685477580.8 " DEG "C"},
        // rainfall: <v> mm/yr
        {"rainfall", 0, "0 mm/yr"},
        {"rainfall", 646, "646 mm/yr"},
        {"rainfall", 10000, "10000 mm/yr"},
        {"rainfall", 65535, "65535 mm/yr"},
        // the six score layers: <v> / 255 (no "(none)" or "(best)": those are legend labels)
        {"fertility", 0, "0 / 255"},
        {"fertility", 255, "255 / 255"},
        {"ore", 128, "128 / 255"},
        {"timber", 0, "0 / 255"},
        {"fresh_water", 200, "200 / 255"},
        {"forage", 1, "1 / 255"},
        {"habitability", 255, "255 / 255"},
        // biome: code <v>
        {"biome", 1, "code 1"},
        {"biome", 12, "code 12"},
        {"biome", 254, "code 254"},
        {"biome", 0, "code 0"},
        // region: none (ocean) / id <v>
        {"region", 0, "none (ocean)"},
        {"region", 1, "id 1"},
        {"region", 65535, "id 65535"},
        // any other name: the plain decimal
        {"test.ramp", 12, "12"},
        {"water_level", -3, "-3"},
        {"", I64_MAX, "9223372036854775807"},
        {"Elevation", I64_MIN, "-9223372036854775808"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.name);
        CAPTURE(c.v);
        CHECK(wi::format_layer_value(c.name, c.v) == c.text);
    }
}

TEST_CASE("P4-02 4.3, 5, invariant 4: format_layer_value never throws for any name and edge value") {
    const std::vector<std::int64_t> values = {I64_MIN, I64_MIN + 1, -4294967296, -32769, -32768, -1, 0, 1, 7, 8,
                                              9, 255, 256, 65535, 65536, U32_MAX, U32_MAX + 1, I64_MAX - 1, I64_MAX};
    for (const Expected& e : expected_table()) {
        for (const std::int64_t v : values) {
            CAPTURE(e.name);
            CAPTURE(v);
            CHECK_NOTHROW((void)wi::format_layer_value(e.name, v));
        }
    }
    // Lead ruling (phase 2): A(n) is exact for any value, not only up to
    // u32 max (256 n + 5000 computed without overflow).
    CHECK(wi::format_layer_value("flow_accum", U32_MAX + 1) == "4294967296 cells, 1099511.63 km2 upstream");
    CHECK(wi::format_layer_value("flow_accum", I64_MAX) ==
          "9223372036854775807 cells, 2361183241434822.61 km2 upstream");
}

// --- Test 4: cell_value (4.3) -----------------------------------------------

TEST_CASE("P4-02 4.3: cell_value inside, outside and for every element type") {
    std::int64_t v = 99;
    {
        const LayerGrid g = grid_of<std::uint8_t>(2, 2, {0, 1, 254, 255});
        CHECK(wi::cell_value(g, 0, 0, v));
        CHECK(v == 0);
        CHECK(wi::cell_value(g, 1, 0, v));
        CHECK(v == 1);
        CHECK(wi::cell_value(g, 0, 1, v));
        CHECK(v == 254);
        CHECK(wi::cell_value(g, 1, 1, v));
        CHECK(v == 255);
        CHECK_FALSE(wi::cell_value(g, 2, 0, v));
        CHECK_FALSE(wi::cell_value(g, 0, 2, v));
        CHECK_FALSE(wi::cell_value(g, 2, 2, v));
        CHECK_FALSE(wi::cell_value(g, 0xFFFFFFFFU, 0, v));
        CHECK_FALSE(wi::cell_value(g, 0, 0xFFFFFFFFU, v));
        CHECK_FALSE(wi::cell_value(g, 0xFFFFFFFFU, 0xFFFFFFFFU, v));
    }
    {
        const LayerGrid g = grid_of<std::int8_t>(2, 1, {-128, 127});
        CHECK(wi::cell_value(g, 0, 0, v));
        CHECK(v == -128);
        CHECK(wi::cell_value(g, 1, 0, v));
        CHECK(v == 127);
    }
    {
        const LayerGrid g = grid_of<std::uint16_t>(1, 2, {0, 65535});
        CHECK(wi::cell_value(g, 0, 1, v));
        CHECK(v == 65535);
        CHECK_FALSE(wi::cell_value(g, 1, 0, v));
    }
    {
        const LayerGrid g = grid_of<std::int16_t>(2, 1, {-32768, 32767});
        CHECK(wi::cell_value(g, 0, 0, v));
        CHECK(v == -32768);
        CHECK(wi::cell_value(g, 1, 0, v));
        CHECK(v == 32767);
    }
    {
        const LayerGrid g = grid_of<std::uint32_t>(2, 1, {0, 4294967295U});
        CHECK(wi::cell_value(g, 1, 0, v));
        CHECK(v == U32_MAX);
    }
    {
        const LayerGrid g = grid_of<std::int32_t>(2, 1, {std::numeric_limits<std::int32_t>::min(), 2147483647});
        CHECK(wi::cell_value(g, 0, 0, v));
        CHECK(v == -2147483648LL);
        CHECK(wi::cell_value(g, 1, 0, v));
        CHECK(v == 2147483647);
    }
    {
        const LayerGrid g = grid_of<std::int64_t>(2, 1, {I64_MIN, I64_MAX});
        CHECK(wi::cell_value(g, 0, 0, v));
        CHECK(v == I64_MIN);
        CHECK(wi::cell_value(g, 1, 0, v));
        CHECK(v == I64_MAX);
    }
    {
        // u64: up to INT64_MAX as int64; above it false (the caller shows the raw decimal)
        const std::uint64_t at = static_cast<std::uint64_t>(I64_MAX);
        const LayerGrid g = grid_of<std::uint64_t>(4, 1, {0, at, at + 1U, std::numeric_limits<std::uint64_t>::max()});
        CHECK(wi::cell_value(g, 0, 0, v));
        CHECK(v == 0);
        CHECK(wi::cell_value(g, 1, 0, v));
        CHECK(v == I64_MAX);
        CHECK_FALSE(wi::cell_value(g, 2, 0, v));
        CHECK_FALSE(wi::cell_value(g, 3, 0, v));
    }
}

TEST_CASE("P4-02 4.3, 5: cell_value is false for an index past the value vector, never undefined") {
    std::int64_t v = 0;
    // 2 x 2 declared, three cells held: index 3 is past the vector
    const LayerGrid short_grid = grid_of<std::uint16_t>(2, 2, {10, 11, 12});
    CHECK(wi::cell_value(short_grid, 0, 1, v));
    CHECK(v == 12);
    CHECK_FALSE(wi::cell_value(short_grid, 1, 1, v));
    // no cells at all
    const LayerGrid empty_cells = grid_of<std::int32_t>(3, 3, {});
    CHECK_FALSE(wi::cell_value(empty_cells, 0, 0, v));
    // a zero-size grid
    const LayerGrid zero = grid_of<std::uint8_t>(0, 0, {});
    CHECK_FALSE(wi::cell_value(zero, 0, 0, v));
    // a default grid (u8, 0 x 0)
    CHECK_FALSE(wi::cell_value(LayerGrid{}, 0, 0, v));
}

// --- Test 5: layer_legend (4.4) ---------------------------------------------

namespace {

// The literal colours of the stops and per-value palettes of render.hpp.
const std::vector<std::uint32_t> LAND = {0x467840, 0x68924E, 0x96A860, 0xB2A06C,
                                         0x967856, 0x806A5C, 0xC4C0BC, 0xFAFAFA};
const std::vector<std::uint32_t> OCEAN = {0x78AACD, 0x588CBE, 0x3868A8, 0x204282, 0x102252};
const std::vector<std::uint32_t> TEMP = {0x313695, 0x4575B4, 0xABD9E9, 0xFFFFBF, 0xFDAE61, 0xD73027, 0xA50026};
const std::vector<std::uint32_t> RAIN = {0x8C510A, 0xBF812D, 0xDFC27D, 0xF6E8C3, 0xC7EAE5,
                                         0x80CDC1, 0x35978F, 0x01665E, 0x003C30};
const std::vector<std::uint32_t> SCORE = {0x440154, 0x3B528B, 0x21918C, 0x5EC962, 0xFDE725};
const std::vector<std::uint32_t> WATER = {0xD6C8A0, 0x3464C8, 0x4080CC, 0x183878};
const std::vector<std::uint32_t> RIVER = {0x101010, 0x78AAE6, 0x6496E1, 0x5082DC, 0x3C6ED2, 0x2D5AC8, 0x1E46BE};
const std::vector<std::uint32_t> DIRECTION = {0xE6194B, 0xF58230, 0xFFE119, 0xD2F53C, 0x3CB44B,
                                              0x46F0F0, 0x0082C8, 0x911EB4, 0x000000};
const std::vector<std::uint32_t> COMPOSITE_RIVER = {0x467DD7, 0x376ED2, 0x285FC8, 0x1E50BE, 0x1941AF, 0x1432A0};

std::vector<Row> hypsometric_land_rows() {
    const std::array<std::string_view, 8> labels = {"0.0 m",    "50.0 m",   "200.0 m",  "500.0 m",
                                                    "1000.0 m", "1500.0 m", "2000.0 m", "3000.0 m"};
    std::vector<Row> rows;
    for (std::size_t i = 0; i < labels.size(); ++i) {
        rows.push_back({std::string(labels[i]), {LAND[i]}});
    }
    return rows;
}

std::vector<Row> water_rows_w() {
    return {{"ocean, 0 to 3000 m deep", OCEAN}, {"lake", {0x4080CC}}, {"river, class 1 to 6+", COMPOSITE_RIVER}};
}

const LayerGrid& any_u8_grid() {
    static const LayerGrid g = grid_of<std::uint8_t>(2, 2, {0, 1, 2, 3});
    return g;
}

}  // namespace

TEST_CASE("P4-02 4.4: the literal stop colours match render.hpp (guards the transcription above)") {
    REQUIRE(wi::HYPSOMETRIC_LAND_STOPS.size() == LAND.size());
    for (std::size_t i = 0; i < LAND.size(); ++i) CHECK(pack(wi::HYPSOMETRIC_LAND_STOPS[i].colour) == LAND[i]);
    REQUIRE(wi::HYPSOMETRIC_OCEAN_STOPS.size() == OCEAN.size());
    for (std::size_t i = 0; i < OCEAN.size(); ++i) CHECK(pack(wi::HYPSOMETRIC_OCEAN_STOPS[i].colour) == OCEAN[i]);
    REQUIRE(wi::TEMPERATURE_STOPS.size() == TEMP.size());
    for (std::size_t i = 0; i < TEMP.size(); ++i) CHECK(pack(wi::TEMPERATURE_STOPS[i].colour) == TEMP[i]);
    REQUIRE(wi::RAINFALL_STOPS.size() == RAIN.size());
    for (std::size_t i = 0; i < RAIN.size(); ++i) CHECK(pack(wi::RAINFALL_STOPS[i].colour) == RAIN[i]);
    REQUIRE(wi::SCORE_STOPS.size() == SCORE.size());
    for (std::size_t i = 0; i < SCORE.size(); ++i) CHECK(pack(wi::SCORE_STOPS[i].colour) == SCORE[i]);
    for (std::uint64_t v = 0; v < 4; ++v) CHECK(pack(wi::water_colour(v)) == WATER[v]);
    for (std::uint64_t v = 0; v < 7; ++v) CHECK(pack(wi::river_colour(v)) == RIVER[v]);
    for (std::uint64_t v = 0; v < 9; ++v) CHECK(pack(wi::direction_colour(v)) == DIRECTION[v]);
    for (std::uint64_t k = 1; k <= 6; ++k) CHECK(pack(wi::composite_river_colour(k)) == COMPOSITE_RIVER[k - 1]);
}

TEST_CASE("P4-02 4.4: hypsometric legend, land stops then ocean stops") {
    const LayerGrid g = grid_of<std::int16_t>(2, 1, {-1731, 21715});
    std::vector<Row> want = hypsometric_land_rows();
    const std::array<std::string_view, 5> sea = {"sea, 0.0 m deep", "sea, 20.0 m deep", "sea, 100.0 m deep",
                                                 "sea, 500.0 m deep", "sea, 3000.0 m deep"};
    for (std::size_t i = 0; i < sea.size(); ++i) {
        want.push_back({std::string(sea[i]), {OCEAN[i]}});
    }
    check_legend(wi::layer_legend("elevation", Palette::hypsometric, g, {}), want);
}

TEST_CASE("P4-02 4.4: temperature, rainfall and score legends") {
    const LayerGrid t = grid_of<std::int16_t>(1, 1, {0});
    check_legend(wi::layer_legend("temperature", Palette::temperature, t, {}),
                 {{"-30.0 " DEG "C", {TEMP[0]}},
                  {"-10.0 " DEG "C", {TEMP[1]}},
                  {"0.0 " DEG "C", {TEMP[2]}},
                  {"5.0 " DEG "C", {TEMP[3]}},
                  {"10.0 " DEG "C", {TEMP[4]}},
                  {"20.0 " DEG "C", {TEMP[5]}},
                  {"30.0 " DEG "C", {TEMP[6]}}});
    const LayerGrid r = grid_of<std::uint16_t>(1, 1, {0});
    check_legend(wi::layer_legend("rainfall", Palette::rainfall, r, {}),
                 {{"0 mm/yr", {RAIN[0]}},
                  {"250 mm/yr", {RAIN[1]}},
                  {"500 mm/yr", {RAIN[2]}},
                  {"750 mm/yr", {RAIN[3]}},
                  {"1000 mm/yr", {RAIN[4]}},
                  {"1500 mm/yr", {RAIN[5]}},
                  {"2500 mm/yr", {RAIN[6]}},
                  {"4000 mm/yr", {RAIN[7]}},
                  {"10000 mm/yr", {RAIN[8]}}});
    for (const std::string_view name : {"fertility", "ore", "timber", "fresh_water", "forage", "habitability"}) {
        CAPTURE(name);
        check_legend(wi::layer_legend(name, Palette::score, any_u8_grid(), {}),
                     {{"0 / 255 (none)", {SCORE[0]}},
                      {"64 / 255", {SCORE[1]}},
                      {"128 / 255", {SCORE[2]}},
                      {"192 / 255", {SCORE[3]}},
                      {"255 / 255 (best)", {SCORE[4]}}});
    }
}

TEST_CASE("P4-02 4.4: water, river and direction legends") {
    check_legend(wi::layer_legend("water", Palette::water, any_u8_grid(), {}),
                 {{"dry land", {WATER[0]}}, {"river", {WATER[1]}}, {"lake", {WATER[2]}}, {"ocean", {WATER[3]}}});
    check_legend(wi::layer_legend("river", Palette::river, any_u8_grid(), {}),
                 {{"0 not a river", {RIVER[0]}},
                  {"class 1", {RIVER[1]}},
                  {"class 2", {RIVER[2]}},
                  {"class 3", {RIVER[3]}},
                  {"class 4", {RIVER[4]}},
                  {"class 5", {RIVER[5]}},
                  {"class 6 to 15", {RIVER[6]}}});
    check_legend(wi::layer_legend("flow_dir", Palette::direction, any_u8_grid(), {}),
                 {{"0 E", {DIRECTION[0]}},
                  {"1 SE", {DIRECTION[1]}},
                  {"2 S", {DIRECTION[2]}},
                  {"3 SW", {DIRECTION[3]}},
                  {"4 W", {DIRECTION[4]}},
                  {"5 NW", {DIRECTION[5]}},
                  {"6 N", {DIRECTION[6]}},
                  {"7 NE", {DIRECTION[7]}},
                  {"8 none (ocean)", {DIRECTION[8]}}});
}

TEST_CASE("P4-02 4.4: log2 legend, with cell areas for flow_accum only") {
    const LayerGrid g = grid_of<std::uint32_t>(1, 1, {0});
    const std::vector<std::uint32_t> grays = {0x000000, 0x0B0B0B, 0x373737, 0x636363, 0x8F8F8F, 0xBBBBBB, 0xE7E7E7};
    check_legend(wi::layer_legend("flow_accum", Palette::log2, g, {}),
                 {{"0", {grays[0]}},
                  {"1 cells (0.00 km2)", {grays[1]}},
                  {"16 cells (0.00 km2)", {grays[2]}},
                  {"256 cells (0.07 km2)", {grays[3]}},
                  {"4096 cells (1.05 km2)", {grays[4]}},
                  {"65536 cells (16.78 km2)", {grays[5]}},
                  {"1048576 cells (268.44 km2)", {grays[6]}}});
    // another layer drawn with log2 (a named --palette log2): plain values
    for (const std::string_view name : {"lake", "walk_area", "unknown.layer"}) {
        CAPTURE(name);
        check_legend(wi::layer_legend(name, Palette::log2, g, {}),
                     {{"0", {grays[0]}},
                      {"1", {grays[1]}},
                      {"16", {grays[2]}},
                      {"256", {grays[3]}},
                      {"4096", {grays[4]}},
                      {"65536", {grays[5]}},
                      {"1048576", {grays[6]}}});
    }
}

TEST_CASE("P4-02 4.4: ids legend") {
    std::vector<std::uint32_t> strip;
    for (std::uint64_t v = 1; v <= 5; ++v) strip.push_back(pack(wi::ids_colour(v)));
    for (const std::string_view name : {"lake", "walk_area", "region", "other.ids"}) {
        CAPTURE(name);
        check_legend(wi::layer_legend(name, Palette::ids, any_u8_grid(), {}),
                     {{"0: none", {0x000000}}, {"each id its own colour", strip}});
    }
}

TEST_CASE("P4-02 4.4: signed legend, six rows when none coincide") {
    // M = 2710, P = 2205: -M, -M/2, -1, 0, P/2, P
    const LayerGrid g = grid_of<std::int16_t>(3, 1, {-2710, 0, 2205});
    const wi::SignedScale sc{2205, 2710};
    const auto c = [&sc](std::int64_t v) { return pack(wi::signed_colour(v, sc)); };
    check_legend(wi::layer_legend("coast_dist", Palette::signed_distance, g, {}),
                 {{"2710 m offshore", {c(-2710)}},
                  {"1355 m offshore", {c(-1355)}},
                  {"1 m offshore", {c(-1)}},
                  {"0 m from the sea", {c(0)}},
                  {"1102 m from the sea", {c(1102)}},
                  {"2205 m from the sea", {c(2205)}}});
    // odd M and P: C++ truncation of -M/2 and P/2
    const LayerGrid h = grid_of<std::int32_t>(2, 1, {-7, 9});
    const wi::SignedScale sh{9, 7};
    const auto d = [&sh](std::int64_t v) { return pack(wi::signed_colour(v, sh)); };
    check_legend(wi::layer_legend("unknown.signed", Palette::signed_distance, h, {}),
                 {{"-7", {d(-7)}}, {"-3", {d(-3)}}, {"-1", {d(-1)}}, {"0", {d(0)}}, {"4", {d(4)}}, {"9", {d(9)}}});
    // the full i64 range: M = 2^63 as u64, -M is INT64_MIN
    const LayerGrid big = grid_of<std::int64_t>(2, 1, {I64_MIN, I64_MAX});
    const wi::SignedScale sb{static_cast<std::uint64_t>(I64_MAX), std::uint64_t{1} << 63U};
    const auto b = [&sb](std::int64_t v) { return pack(wi::signed_colour(v, sb)); };
    check_legend(wi::layer_legend("unknown.signed", Palette::signed_distance, big, {}),
                 {{"-9223372036854775808", {b(I64_MIN)}},
                  {"-4611686018427387904", {b(I64_MIN / 2)}},
                  {"-1", {b(-1)}},
                  {"0", {b(0)}},
                  {"4611686018427387903", {b(I64_MAX / 2)}},
                  {"9223372036854775807", {b(I64_MAX)}}});
}

TEST_CASE("P4-02 4.4: signed legend with small M or P keeps repeated rows") {
    // Lead ruling (phase 2): the skip of 4.4 never drops a row (M, P >= 1);
    // repeated values for small M or P are kept, as the viewer repeats them.
    struct Small {
        std::vector<std::int16_t> cells;
        wi::SignedScale scale;
        std::vector<std::int64_t> list;  // -M, -M/2, -1, 0, P/2, P
    };
    const std::vector<Small> cases = {
        {{0, 1}, {1, 1}, {-1, 0, -1, 0, 0, 1}},    // no negative value: M = 1
        {{-1, 0}, {1, 1}, {-1, 0, -1, 0, 0, 1}},   // no positive value: P = 1
        {{-2, 3}, {3, 2}, {-2, -1, -1, 0, 1, 3}},  // -M/2 = -1
        {{5, 5}, {5, 1}, {-1, 0, -1, 0, 2, 5}},    // constant positive grid
    };
    for (const Small& s : cases) {
        CAPTURE(s.scale.p);
        CAPTURE(s.scale.m);
        const LayerGrid g = grid_of<std::int16_t>(static_cast<std::uint32_t>(s.cells.size()), 1, s.cells);
        std::vector<Row> want;
        for (const std::int64_t v : s.list) {
            want.push_back({wi::format_layer_value("coast_dist", v), {pack(wi::signed_colour(v, s.scale))}});
        }
        check_legend(wi::layer_legend("coast_dist", Palette::signed_distance, g, {}), want);
    }
}

TEST_CASE("P4-02 4.4, 5: signed on an unsigned or invalid grid gives no rows") {
    for (const LayerGrid& g : {grid_of<std::uint8_t>(1, 1, {0}), grid_of<std::uint16_t>(1, 1, {3}),
                               grid_of<std::uint32_t>(1, 1, {3}), grid_of<std::uint64_t>(1, 1, {3}),
                               grid_of<std::int16_t>(2, 2, {1}), grid_of<std::int16_t>(0, 0, {}), LayerGrid{}}) {
        CHECK_NOTHROW((void)wi::layer_legend("coast_dist", Palette::signed_distance, g, {}));
        CHECK(wi::layer_legend("coast_dist", Palette::signed_distance, g, {}).empty());
    }
}

TEST_CASE("P4-02 4.4: gray legend on unsigned, signed, constant and invalid grids") {
    const std::vector<std::uint32_t> strip = {0x000000, 0x808080, 0xFFFFFF};
    // unsigned, a catalogue name with display forms
    check_legend(wi::layer_legend("water_depth", Palette::gray, grid_of<std::uint16_t>(3, 1, {15, 0, 1731}), {}),
                 {{"0 m (no standing water) (min)", {0x000000}},
                  {"", strip},
                  {"173.1 m deep (max)", {0xFFFFFF}}});
    // signed
    check_legend(wi::layer_legend("elevation", Palette::gray, grid_of<std::int16_t>(2, 1, {1234, -5}), {}),
                 {{"-0.5 m (sea) (min)", {0x000000}}, {"", strip}, {"123.4 m (max)", {0xFFFFFF}}});
    // constant: r = 0, so the max row is gray 0 too
    check_legend(wi::layer_legend("unknown.const", Palette::gray, grid_of<std::uint8_t>(2, 2, {7, 7, 7, 7}), {}),
                 {{"7 (min)", {0x000000}}, {"", strip}, {"7 (max)", {0x000000}}});
    // an unknown name: plain decimals
    check_legend(wi::layer_legend("test.noise", Palette::gray, grid_of<std::int16_t>(2, 1, {-32767, 32767}), {}),
                 {{"-32767 (min)", {0x000000}}, {"", strip}, {"32767 (max)", {0xFFFFFF}}});
    // u64 above INT64_MAX: the plain decimal, even for a catalogue name with a display form
    const std::uint64_t top = std::numeric_limits<std::uint64_t>::max();
    check_legend(wi::layer_legend("lake", Palette::gray, grid_of<std::uint64_t>(2, 1, {1, top}), {}),
                 {{"id 1 (min)", {0x000000}}, {"", strip}, {"18446744073709551615 (max)", {0xFFFFFF}}});
    check_legend(wi::layer_legend("unknown.u64", Palette::gray, grid_of<std::uint64_t>(1, 1, {top}), {}),
                 {{"18446744073709551615 (min)", {0x000000}}, {"", strip}, {"18446744073709551615 (max)", {0x000000}}});
    // i64 extremes
    check_legend(wi::layer_legend("unknown.i64", Palette::gray, grid_of<std::int64_t>(2, 1, {I64_MAX, I64_MIN}), {}),
                 {{"-9223372036854775808 (min)", {0x000000}},
                  {"", strip},
                  {"9223372036854775807 (max)", {0xFFFFFF}}});
    // invalid grids: no rows
    for (const LayerGrid& g : {LayerGrid{}, grid_of<std::uint8_t>(2, 2, {1, 2, 3}), grid_of<std::int16_t>(0, 1, {})}) {
        CHECK(wi::layer_legend("water_depth", Palette::gray, g, {}).empty());
    }
}

TEST_CASE("P4-02 4.4: biome legend, every record in the given order, key when the name is empty") {
    std::vector<wi::BiomeRecord> recs(3);
    recs[0].id = 4;
    recs[0].key = "temperate_forest";
    recs[0].name = "Temperate forest";
    recs[0].colour = 0x2E7D32;
    recs[0].water = 0;
    recs[1].id = 1;
    recs[1].key = "ocean";
    recs[1].name = "";
    recs[1].colour = 0x183878;
    recs[1].water = 1;
    recs[2].id = 9;
    recs[2].key = "odd";
    recs[2].name = "Odd colour";
    recs[2].colour = 0x7F123456;  // above 0xFFFFFF: masked
    recs[2].water = 0;
    const LayerGrid g = grid_of<std::uint8_t>(1, 1, {4});
    check_legend(wi::layer_legend("biome", Palette::biome, g, recs),
                 {{"Temperate forest", {0x2E7D32}}, {"ocean", {0x183878}}, {"Odd colour", {0x123456}}});
    CHECK(wi::layer_legend("biome", Palette::biome, g, {}).empty());
}

TEST_CASE("P4-02 4.4: the legend follows the palette it is given, not the layer's registered one") {
    // elevation drawn gray (auto_palette's fallback for an unexpected type)
    const LayerGrid u = grid_of<std::uint8_t>(2, 1, {0, 10});
    check_legend(wi::layer_legend("elevation", Palette::gray, u, {}),
                 {{"0.0 m (min)", {0x000000}}, {"", {0x000000, 0x808080, 0xFFFFFF}}, {"1.0 m (max)", {0xFFFFFF}}});
    // an unknown layer given a ramp palette gets that palette's rows
    const auto rows = wi::layer_legend("unknown", Palette::score, u, {});
    REQUIRE(rows.size() == 5U);
    CHECK(rows[0].label == "0 / 255 (none)");
    CHECK(rows[4].label == "255 / 255 (best)");
}

TEST_CASE("P4-02 5, invariant 4: layer_legend never throws for any palette, name and grid") {
    const std::vector<LayerGrid> grids = {
        grid_of<std::uint8_t>(2, 1, {0, 255}),          grid_of<std::int8_t>(2, 1, {-128, 127}),
        grid_of<std::uint16_t>(2, 1, {0, 65535}),       grid_of<std::int16_t>(2, 1, {-32768, 32767}),
        grid_of<std::uint32_t>(2, 1, {0, 4294967295U}), grid_of<std::int32_t>(1, 1, {0}),
        grid_of<std::uint64_t>(2, 1, {0, std::numeric_limits<std::uint64_t>::max()}),
        grid_of<std::int64_t>(2, 1, {I64_MIN, I64_MAX}), grid_of<std::int16_t>(1, 1, {5}),
        grid_of<std::int16_t>(1, 1, {-5}),               LayerGrid{},
        grid_of<std::uint8_t>(3, 3, {1}),
    };
    std::vector<wi::BiomeRecord> recs(1);
    recs[0].id = 1;
    recs[0].key = "k";
    std::vector<std::string_view> names;
    for (const Expected& e : expected_table()) names.push_back(e.name);
    names.push_back("");
    names.push_back("unknown");
    for (const Palette p : wi::ALL_PALETTES) {
        for (const std::string_view n : names) {
            for (const LayerGrid& g : grids) {
                CAPTURE(wi::palette_name(p));
                CAPTURE(n);
                CHECK_NOTHROW((void)wi::layer_legend(n, p, g, recs));
            }
        }
    }
}

// --- Test 6: composites (4.4) -----------------------------------------------

TEST_CASE("P4-02 4.4: composite_description and composite_legend with a two-record biome table") {
    CHECK(wi::composite_description(wi::Composite::map) == "relief shaded by slope");
    CHECK(wi::composite_description(wi::Composite::biomes) == "biome colours shaded by slope");
    CHECK(wi::composite_description(wi::Composite::regions) == "biome colours shaded by slope, region borders");

    std::vector<wi::BiomeRecord> recs(2);
    recs[0].id = 1;
    recs[0].key = "ocean";
    recs[0].name = "Ocean";
    recs[0].colour = 0x183878;
    recs[0].water = 1;
    recs[1].id = 4;
    recs[1].key = "grassland";
    recs[1].name = "Grassland";
    recs[1].colour = 0xA4C639;
    recs[1].water = 0;

    std::vector<Row> map_rows = hypsometric_land_rows();
    for (Row& r : water_rows_w()) map_rows.push_back(std::move(r));
    check_legend(wi::composite_legend(wi::Composite::map, recs), map_rows);
    check_legend(wi::composite_legend(wi::Composite::map, {}), map_rows);

    std::vector<Row> biomes = {{"Grassland", {0xA4C639}}};
    for (Row& r : water_rows_w()) biomes.push_back(std::move(r));
    check_legend(wi::composite_legend(wi::Composite::biomes, recs), biomes);

    std::vector<Row> regions = {{"region border", {0x000000}}, {"Grassland", {0xA4C639}}};
    for (Row& r : water_rows_w()) regions.push_back(std::move(r));
    check_legend(wi::composite_legend(wi::Composite::regions, recs), regions);

    // a land record without a name shows its key; records keep their order
    recs[0].water = 0;
    recs[1].name = "";
    std::vector<Row> both = {{"Ocean", {0x183878}}, {"grassland", {0xA4C639}}};
    for (Row& r : water_rows_w()) both.push_back(std::move(r));
    check_legend(wi::composite_legend(wi::Composite::biomes, recs), both);
}

// --- Test 7: available_composites (4.5, invariant 7) ------------------------

TEST_CASE("P4-02 4.5, invariant 7: available_composites for every combination") {
    using wi::Composite;
    for (int mask = 0; mask < 16; ++mask) {
        const bool elevation = (mask & 1) != 0;
        const bool biome = (mask & 2) != 0;
        const bool region = (mask & 4) != 0;
        const bool usable = (mask & 8) != 0;
        CAPTURE(elevation);
        CAPTURE(biome);
        CAPTURE(region);
        CAPTURE(usable);
        // layers in an order unlike the catalogue's, with one the rule ignores
        std::vector<wdb::LayerInfo> layers;
        const auto add = [&layers](std::string name, ElementType t) {
            wdb::LayerInfo info;
            info.id = static_cast<std::int64_t>(layers.size()) + 1;
            info.name = std::move(name);
            info.width = 4;
            info.height = 4;
            info.type = t;
            layers.push_back(std::move(info));
        };
        add("water", ElementType::u8);
        if (region) add("region", ElementType::u16);
        if (biome) add("biome", ElementType::u8);
        if (elevation) add("elevation", ElementType::i16);
        std::vector<Composite> want;
        if (elevation) {
            want.push_back(Composite::map);
            if (biome && usable) {
                want.push_back(Composite::biomes);
                if (region) want.push_back(Composite::regions);
            }
        }
        CHECK(wi::available_composites(layers, usable) == want);
    }
    CHECK(wi::available_composites({}, true).empty());
    CHECK(wi::available_composites({}, false).empty());
}

TEST_CASE("P4-02 4.5: names only, no decoding: a wrong type still lists the composite") {
    std::vector<wdb::LayerInfo> layers(3);
    layers[0].name = "elevation";
    layers[0].type = ElementType::u8;  // not i16: drawing would fail, listing does not
    layers[1].name = "biome";
    layers[1].type = ElementType::u32;
    layers[2].name = "region";
    layers[2].type = ElementType::i64;
    CHECK(wi::available_composites(layers, true) ==
          std::vector<wi::Composite>{wi::Composite::map, wi::Composite::biomes, wi::Composite::regions});
    // names compare exactly
    layers[0].name = "Elevation";
    CHECK(wi::available_composites(layers, true).empty());
}

// --- Test 8: read_region_rows and region_name (4.6, invariant 6) -----------

namespace {

// A row of table `region` (P1-06 3.2, 28 columns): id, name, biome, then 25
// integers, cells at column 11 and walk_cells at column 16 (1-based).
std::vector<wdb::ColumnValue> region_row(std::int64_t id, std::string name, std::int64_t biome, std::int64_t cells,
                                         std::int64_t walk_cells) {
    std::vector<wdb::ColumnValue> row(28);
    for (std::size_t k = 0; k < row.size(); ++k) {
        row[k].type = wdb::ValueType::integer;
        row[k].integer = static_cast<std::int64_t>(k) * 3;
    }
    row[0].integer = id;
    row[1].type = wdb::ValueType::text;
    row[1].integer = 0;
    row[1].bytes = std::move(name);
    row[2].integer = biome;
    row[10].integer = cells;
    row[15].integer = walk_cells;
    return row;
}

std::vector<wi::RegionRecord> records_of(std::vector<std::pair<std::int64_t, std::string>> list) {
    std::vector<wi::RegionRecord> out;
    for (auto& [id, name] : list) {
        wi::RegionRecord r;
        r.id = id;
        r.name = std::move(name);
        out.push_back(std::move(r));
    }
    return out;
}

}  // namespace

TEST_CASE("P4-02 4.6: read_region_rows fills name and biome beside id, cells and walk_cells") {
    const std::vector<std::vector<wdb::ColumnValue>> rows = {region_row(1, "North Wood", 4, 9, 7),
                                                             region_row(2, "the Grey Fells", 7, 2930, 100),
                                                             region_row(5, "", 12, 1, 0)};
    const wi::RegionTableResult r = wi::read_region_rows(rows);
    REQUIRE_FALSE(r.problem.has_value());
    REQUIRE(r.records.size() == 3U);
    CHECK(r.records[0].id == 1);
    CHECK(r.records[0].name == "North Wood");
    CHECK(r.records[0].biome == 4);
    CHECK(r.records[0].cells == 9);
    CHECK(r.records[0].walk_cells == 7);
    CHECK(r.records[1].id == 2);
    CHECK(r.records[1].name == "the Grey Fells");
    CHECK(r.records[1].biome == 7);
    CHECK(r.records[1].cells == 2930);
    CHECK(r.records[1].walk_cells == 100);
    CHECK(r.records[2].id == 5);
    CHECK(r.records[2].name.empty());
    CHECK(r.records[2].biome == 12);
    CHECK(r.records[2].cells == 1);
    CHECK(r.records[2].walk_cells == 0);
}

TEST_CASE("P4-02 4.6: read_region_rows keeps every problem message") {
    const auto problem_of = [](std::vector<wdb::ColumnValue> bad) {
        const std::vector<std::vector<wdb::ColumnValue>> rows = {region_row(1, "A", 1, 5, 5), std::move(bad)};
        const wi::RegionTableResult r = wi::read_region_rows(rows);
        return r.problem.value_or("<none>");
    };
    {
        auto row = region_row(5, "B", 1, 5, 5);
        row.pop_back();
        CHECK(problem_of(row) == "row 5: has 27 columns, expected 28");
    }
    {
        auto row = region_row(5, "B", 1, 5, 5);
        row.push_back(row.back());
        CHECK(problem_of(row) == "row 5: has 29 columns, expected 28");
    }
    {
        auto row = region_row(5, "B", 1, 5, 5);
        row[1].type = wdb::ValueType::integer;
        row[1].bytes.clear();
        CHECK(problem_of(row) == "row 5: name is not text");
    }
    {
        auto row = region_row(5, "B", 1, 5, 5);
        row[2].type = wdb::ValueType::text;
        row[2].bytes = "4";
        CHECK(problem_of(row) == "row 5: column 3 is not an integer");
    }
    {
        auto row = region_row(5, "B", 1, 5, 5);
        row[27].type = wdb::ValueType::null;
        CHECK(problem_of(row) == "row 5: column 28 is not an integer");
    }
    {
        auto row = region_row(5, "B", 1, 0, 0);
        CHECK(problem_of(row) == "row 5: cells below 1");
    }
}

TEST_CASE("P4-02 4.6, invariant 6: region_name for present, absent, 0 and empty-span cases") {
    const auto recs = records_of({{1, "North Wood"}, {3, "East Wood"}, {7, "the Grey Fells"}, {65535, "Last Reach"}});
    CHECK(wi::region_name(recs, 1) == "North Wood");
    CHECK(wi::region_name(recs, 3) == "East Wood");
    CHECK(wi::region_name(recs, 7) == "the Grey Fells");
    CHECK(wi::region_name(recs, 65535) == "Last Reach");
    for (const std::int64_t absent : {std::int64_t{0}, std::int64_t{2}, std::int64_t{4}, std::int64_t{6},
                                      std::int64_t{8}, std::int64_t{65534}, std::int64_t{65536}, std::int64_t{-1},
                                      I64_MIN, I64_MAX}) {
        CAPTURE(absent);
        CHECK(wi::region_name(recs, absent).empty());
    }
    CHECK(wi::region_name({}, 0).empty());
    CHECK(wi::region_name({}, 1).empty());
    // one record
    const auto one = records_of({{2, "Solo"}});
    CHECK(wi::region_name(one, 2) == "Solo");
    CHECK(wi::region_name(one, 1).empty());
    CHECK(wi::region_name(one, 3).empty());
    // a longer run, every id found by the search
    std::vector<std::pair<std::int64_t, std::string>> many;
    for (std::int64_t id = 1; id <= 40; ++id) many.emplace_back(id * 2, "R" + wdb::format_i64(id * 2));
    const auto lots = records_of(many);
    for (std::int64_t id = 0; id <= 82; ++id) {
        CAPTURE(id);
        if (id > 0 && id <= 80 && id % 2 == 0) {
            CHECK(wi::region_name(lots, id) == "R" + wdb::format_i64(id));
        } else {
            CHECK(wi::region_name(lots, id).empty());
        }
    }
}

TEST_CASE("P4-02 4.6, invariant 4: region_name on unordered records gives a record's name or empty") {
    // 4.6: "Unordered records give an unspecified record or empty, never
    // undefined behaviour" (the implementation's binary search follows it).
    const auto recs = records_of({{9, "Nine"}, {2, "Two"}, {5, "Five"}, {1, "One"}, {9, "Nine again"}});
    const std::set<std::string> allowed = {"", "Nine", "Two", "Five", "One", "Nine again"};
    for (std::int64_t id = -1; id <= 10; ++id) {
        CAPTURE(id);
        const std::string got(wi::region_name(recs, id));
        CHECK(allowed.count(got) == 1U);
    }
    // id 0 is empty even when a record claims it
    const auto zero = records_of({{0, "Sea"}, {1, "Land"}});
    CHECK(wi::region_name(zero, 0).empty());
    CHECK(wi::region_name(zero, 1) == "Land");
}

TEST_CASE("P4-02 4.4: composite_description of an out-of-range value is empty") {
    CHECK(wi::composite_description(static_cast<wi::Composite>(std::uint8_t{3})).empty());
    CHECK(wi::composite_description(static_cast<wi::Composite>(std::uint8_t{255})).empty());
}

}  // TEST_SUITE
