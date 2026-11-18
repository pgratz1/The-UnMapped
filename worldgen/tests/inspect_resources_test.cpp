// report and render for the resources layers (spec P1-05 section 9 and
// 10.1 tests 3, 7 and 8; P1-00 10.2 and 10.3).
//
// Hand-made worlds: layers written by stages::testing::write_fixture_world.
// The expected report values are recomputed by the test
// (p1_05_resources_support.hpp report_values) and were checked by
// reference/p1_resources_reference.py (report_items) on the same arrays.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "p1_05_resources_support.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/testing.hpp"

using namespace resource_test;
using worldgen::db::Codec;
using worldgen::db::LayerGrid;

namespace {

constexpr std::uint32_t W = 8;

// The 8 x 8 world of 10.1 test 7: an ocean border (e -20); lake cells at
// (5, 2) and (6, 2); river cells on row 4, x 1 to 4; elevation 50 + 10 x
// inside. Scores (i * a + b) mod 256 on dry and river cells, fresh water
// 255 on lake and river cells, 0 on the ocean; walk areas 1 on x <= 3, 0
// on x = 4, 2 on x >= 5.
struct World {
    std::vector<std::int16_t> elevation;
    std::vector<std::uint8_t> water;
    resource_api::Layers l;
};

World make_world() {
    World w;
    for (std::uint32_t i = 0; i < W * W; ++i) {
        const std::uint32_t x = i % W, y = i / W;
        const bool border = x == 0 || y == 0 || x == W - 1 || y == W - 1;
        const bool lake = y == 2 && (x == 5 || x == 6);
        const bool river = y == 4 && x >= 1 && x <= 4;
        const bool land = !border && !lake;
        w.elevation.push_back(static_cast<std::int16_t>(border ? -20 : 50 + 10 * static_cast<int>(x)));
        w.water.push_back(border ? WATER_OCEAN : lake ? WATER_LAKE : river ? WATER_RIVER : WATER_DRY);
        auto f = [&](std::uint32_t a, std::uint32_t b) { return static_cast<std::uint8_t>(land ? (i * a + b) % 256 : 0); };
        w.l.fertility.push_back(f(37, 11));
        w.l.ore.push_back(f(53, 7));
        w.l.timber.push_back(f(29, 3));
        w.l.fresh_water.push_back((lake || river) ? std::uint8_t{255} : f(41, 5));
        w.l.forage.push_back(f(17, 1));
        w.l.habitability.push_back(f(61, 13));
        w.l.walk_area.push_back(static_cast<std::uint16_t>(!land ? 0 : x <= 3 ? 1 : x == 4 ? 0 : 2));
    }
    return w;
}

const std::vector<std::string_view> NAMES = {"fertility", "ore",          "timber",   "fresh_water",
                                             "forage",    "habitability", "walk_area"};

// Writes the world; `skip` leaves one layer (or "water") out; `replace`
// substitutes one layer's grid.
std::string write(const dbtest::TempDir& dir, const std::string& file, const World& w, std::string_view skip = {},
                  std::string_view replace = {}, const LayerGrid* by = nullptr) {
    std::vector<worldgen::stages::testing::FixtureLayer> layers;
    auto add = [&](std::string_view name, LayerGrid g) {
        if (name == skip) {
            return;
        }
        if (name == replace && by != nullptr) {
            g = *by;
        }
        layers.push_back({std::string(name), std::move(g), Codec::delta_varint});
    };
    add("elevation", p004::grid_of(W, W, w.elevation));
    add("water", p004::grid_of(W, W, w.water));
    add("fertility", p004::grid_of(W, W, w.l.fertility));
    add("ore", p004::grid_of(W, W, w.l.ore));
    add("timber", p004::grid_of(W, W, w.l.timber));
    add("fresh_water", p004::grid_of(W, W, w.l.fresh_water));
    add("forage", p004::grid_of(W, W, w.l.forage));
    add("habitability", p004::grid_of(W, W, w.l.habitability));
    add("walk_area", p004::grid_of(W, W, w.l.walk_area));
    const std::string out = dir.file(file);
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 4, layers).ok());
    return out;
}

std::string report_of(const std::string& path) {
    auto w = worldgen::db::WorldReader::open(path);
    REQUIRE(w.ok());
    const auto r = worldgen::inspect::build_report(w.value());
    INFO("report error: ", r.ok() ? std::string() : r.error().message);
    REQUIRE(r.ok());
    return r.value();
}

std::string report_error(const std::string& path) {
    auto w = worldgen::db::WorldReader::open(path);
    REQUIRE(w.ok());
    const auto r = worldgen::inspect::build_report(w.value());
    if (r.ok()) {
        return {};
    }
    CHECK(r.code() == worldgen::db::ErrorCode::corrupt_layer);
    return r.error().message;
}

bool ends_with(std::string_view s, std::string_view tail) {
    return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

std::string keys_text(const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string s;
    for (const auto& [k, v] : kv) {
        s += "\n" + k + " = " + v;
    }
    return s + "\n";
}

worldgen::inspect::Palette score_palette() {
    const std::optional<worldgen::inspect::Palette> p = worldgen::inspect::palette_from_name("score");
    REQUIRE_MESSAGE(p.has_value(), "no palette named score");
    return *p;
}

}  // namespace

// --- report (9, 10.1 test 7) ------------------------------------------------------------------------

TEST_CASE("P1-05 9, 10.1 test 7: the eight keys on a hand-made 8 x 8 world, last in [geography]") {
    dbtest::TempDir dir("resources-report");
    const World w = make_world();
    const auto values = report_values(w.elevation, w.water, w.l);
    // The reference's values for these arrays.
    const std::vector<std::pair<std::string, std::string>> want = {
        {"scores_p50", "fertility:140 ore:132 timber:105 fresh_water:150 forage:154 habitability:124"},
        {"scores_p90", "fertility:239 ore:229 timber:240 fresh_water:255 forage:238 habitability:234"},
        {"fertile_share", "0.3824"},
        {"habitable_share", "0.6765"},
        {"ore_rich_share", "0.5294"},
        {"walk_areas", "2"},
        {"walk_main_share", "0.5000"},
        {"walk_share", "0.7778"},
    };
    CHECK(values == want);
    const std::string text = report_of(write(dir, "w.db", w));
    INFO(text);
    CHECK(ends_with(text, keys_text(want)));
}

TEST_CASE("P1-05 9, 10.1 test 7: the group is absent when any of the seven layers or water is missing") {
    dbtest::TempDir dir("resources-report-missing");
    const World w = make_world();
    std::vector<std::string_view> skips(NAMES.begin(), NAMES.end());
    skips.push_back("water");
    int k = 0;
    for (const std::string_view skip : skips) {
        CAPTURE(skip);
        const std::string text = report_of(write(dir, "m" + std::to_string(k++) + ".db", w, skip));
        for (const char* key : {"scores_p50", "scores_p90", "fertile_share", "habitable_share", "ore_rich_share",
                                "walk_areas", "walk_main_share", "walk_share"}) {
            CHECK(text.find(std::string("\n") + key + " = ") == std::string::npos);
        }
    }
}

TEST_CASE("P1-05 9, 10.1 test 3: a world whose walkable land is all below the minimum: walk_areas 0, shares 0.0000") {
    dbtest::TempDir dir("resources-report-nowalk");
    World w = make_world();
    std::fill(w.l.walk_area.begin(), w.l.walk_area.end(), std::uint16_t{0});
    const auto values = report_values(w.elevation, w.water, w.l);
    const std::string text = report_of(write(dir, "w.db", w));
    CHECK(ends_with(text, keys_text(values)));
    CHECK(text.find("\nwalk_areas = 0\nwalk_main_share = 0.0000\nwalk_share = 0.0000\n") != std::string::npos);
}

TEST_CASE("P1-05 9: dry land is water 0 or 1 (lakes excluded), land is elevation > 0 (lakes included)") {
    dbtest::TempDir dir("resources-report-dry");
    World w = make_world();
    // Lake cells with the highest scores: no percentile or share moves.
    for (std::size_t i = 0; i < w.water.size(); ++i) {
        if (w.water[i] == WATER_LAKE) {
            w.l.fertility[i] = w.l.ore[i] = w.l.timber[i] = w.l.forage[i] = w.l.habitability[i] = 255;
        }
    }
    const std::string text = report_of(write(dir, "w.db", w));
    CHECK(text.find("\nfertile_share = 0.3824\nhabitable_share = 0.6765\nore_rich_share = 0.5294\n") !=
          std::string::npos);
    // A lake cell in a walk area counts towards walk_share over land.
    w.l.walk_area[2 * W + 5] = 2;
    const std::string t2 = report_of(write(dir, "w2.db", w));
    CHECK(t2.find("\nwalk_share = 0.8056\n") != std::string::npos);  // 29 / 36
}

TEST_CASE("P1-05 9, 10.1 test 7: a score layer not u8, walk_area not u16, or a layer of another size: exit-3 errors") {
    dbtest::TempDir dir("resources-report-corrupt");
    const World w = make_world();
    int k = 0;
    for (const std::string_view name : NAMES) {
        CAPTURE(name);
        const bool walk = name == "walk_area";
        {
            const LayerGrid bad = walk ? p004::grid_of(W, W, std::vector<std::uint8_t>(W * W, 1))
                                       : p004::grid_of(W, W, std::vector<std::int16_t>(W * W, 1));
            const std::string err = report_error(write(dir, "t" + std::to_string(k++) + ".db", w, {}, name, &bad));
            CHECK(err.find("'" + std::string(name) + "'") != std::string::npos);
            CHECK(err.find(walk ? "type u8, expected u16" : "type i16, expected u8") != std::string::npos);
        }
        {
            const LayerGrid bad = walk ? p004::grid_of(7, W, std::vector<std::uint16_t>(7 * W, 1))
                                       : p004::grid_of(7, W, std::vector<std::uint8_t>(7 * W, 1));
            const std::string err = report_error(write(dir, "s" + std::to_string(k++) + ".db", w, {}, name, &bad));
            CHECK(err.find("'" + std::string(name) + "'") != std::string::npos);
            CHECK(err.find("size 7x8, expected 8x8") != std::string::npos);
        }
    }
}

TEST_CASE("P1-05 9 (lead's ruling 3): a corrupt resources layer is refused even when the group is not printed") {
    dbtest::TempDir dir("resources-report-corrupt-partial");
    const World w = make_world();
    const LayerGrid bad = p004::grid_of(W, W, std::vector<std::int16_t>(W * W, 1));
    // walk_area missing (no group), fertility i16: still a corrupt layer.
    std::vector<worldgen::stages::testing::FixtureLayer> layers;
    layers.push_back({"elevation", p004::grid_of(W, W, w.elevation), Codec::delta_varint});
    layers.push_back({"water", p004::grid_of(W, W, w.water), Codec::delta_varint});
    layers.push_back({"fertility", bad, Codec::delta_varint});
    const std::string out = dir.file("p.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 4, layers).ok());
    const std::string err = report_error(out);
    CHECK(err.find("'fertility'") != std::string::npos);
    CHECK(err.find("type i16, expected u8") != std::string::npos);
    // walk_area of type u8 alone, without water.
    std::vector<worldgen::stages::testing::FixtureLayer> l2;
    l2.push_back({"elevation", p004::grid_of(W, W, w.elevation), Codec::delta_varint});
    l2.push_back({"walk_area", p004::grid_of(W, W, std::vector<std::uint8_t>(W * W, 1)), Codec::delta_varint});
    const std::string out2 = dir.file("q.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(out2, 4, l2).ok());
    CHECK(report_error(out2).find("type u8, expected u16") != std::string::npos);
}

// --- render (9, 10.1 test 8) --------------------------------------------------------------------------

TEST_CASE("P1-05 9, 10.1 test 8: the score palette at its stops and t = 100; registered for the six score layers") {
    const worldgen::inspect::Palette p = score_palette();
    CHECK(worldgen::inspect::palette_name(p) == "score");
    CHECK(score_colour(100) == Rgb{44, 117, 140});
    const std::vector<std::uint8_t> t = {0, 64, 100, 128, 192, 255, 1, 63, 129, 254};
    const LayerGrid g = p004::grid_of(static_cast<std::uint32_t>(t.size()), 1, t);
    CHECK(worldgen::inspect::palette_suits(p, g));
    const std::vector<std::uint8_t> px = worldgen::inspect::render_rgb(g, p, 1);
    REQUIRE(px.size() == 3 * t.size());
    const std::vector<Rgb> stops = {{68, 1, 84}, {59, 82, 139}, {44, 117, 140}, {33, 145, 140}, {94, 201, 98},
                                    {253, 231, 37}};
    for (std::size_t i = 0; i < t.size(); ++i) {
        CAPTURE(int{t[i]});
        const Rgb want = score_colour(t[i]);
        if (i < stops.size()) {
            CHECK(want == stops[i]);
        }
        CHECK(Rgb{px[3 * i], px[3 * i + 1], px[3 * i + 2]} == want);
    }
    for (const char* name : {"fertility", "ore", "timber", "fresh_water", "forage", "habitability"}) {
        CAPTURE(name);
        CHECK(worldgen::inspect::registered_palette(name) == p);
        CHECK(worldgen::inspect::auto_palette(name, g) == p);
    }
    // walk_area takes ids, which accepts u16 (P1-00 10.2).
    const std::optional<worldgen::inspect::Palette> ids = worldgen::inspect::palette_from_name("ids");
    REQUIRE(ids.has_value());
    CHECK(worldgen::inspect::registered_palette("walk_area") == *ids);
    const LayerGrid u16 = p004::grid_of(4, 1, std::vector<std::uint16_t>{0, 1, 2, 65535});
    CHECK(worldgen::inspect::palette_suits(*ids, u16));
    CHECK(worldgen::inspect::auto_palette("walk_area", u16) == *ids);
}

TEST_CASE("P1-05 9: score suits u8 layers only; its requirement names u8") {
    const worldgen::inspect::Palette p = score_palette();
    CHECK_FALSE(worldgen::inspect::palette_suits(p, p004::grid_of(2, 1, std::vector<std::int16_t>{0, 1})));
    CHECK_FALSE(worldgen::inspect::palette_suits(p, p004::grid_of(2, 1, std::vector<std::uint16_t>{0, 1})));
    CHECK_FALSE(worldgen::inspect::palette_suits(p, p004::grid_of(2, 1, std::vector<std::uint32_t>{0, 1})));
    CHECK(std::string(worldgen::inspect::palette_requirement(p)).find("u8") != std::string::npos);
    // Under auto an unsuitable layer of a score name falls back to gray.
    CHECK(worldgen::inspect::auto_palette("fertility", p004::grid_of(2, 1, std::vector<std::int16_t>{0, 1})) ==
          worldgen::inspect::Palette::gray);
}
