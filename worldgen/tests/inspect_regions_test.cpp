// report and render for the regions layer and tables (spec P1-06 section 9
// and 10.1 tests 7 and 8; P0-04 amendment 18; P1-00 10.2 and 10.3).
//
// Hand-made worlds: layers written by stages::testing::write_fixture_world,
// table rows inserted with SQL. The expected report values are recomputed
// by the test (p1_06_regions_support.hpp report_values) from the rows, as
// the reference's report_items does; `report` does not recompute the tables
// from the layer (9).

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "p1_06_regions_api.hpp"
#include "p1_06_regions_support.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/render.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/testing.hpp"

using namespace region_test;
using worldgen::db::Codec;
using worldgen::db::LayerGrid;

namespace {

constexpr std::uint32_t W = 8;

// The 8 x 8 world of 10.1 test 7: an ocean border (e -20); inside, four
// regions in quadrants with a lake cell in region 2; biome 4 on land.
struct World {
    std::vector<std::int16_t> elevation;
    std::vector<std::uint8_t> water, biome;
    std::vector<std::uint16_t> region;
    std::vector<region_api::RegionRow> rows;
    std::vector<region_api::BorderRow> borders;
};

region_api::RegionRow row_of(std::int64_t id, std::string name, std::int64_t cells, std::int64_t walk_cells) {
    region_api::RegionRow r;
    r.id = id;
    r.name = std::move(name);
    r.biome = 4;
    r.cells = cells;
    r.walk_cells = walk_cells;
    r.landmass = 1;
    return r;
}

World make_world() {
    World w;
    for (std::uint32_t i = 0; i < W * W; ++i) {
        const std::uint32_t x = i % W, y = i / W;
        const bool border = x == 0 || y == 0 || x == W - 1 || y == W - 1;
        const bool lake = x == 5 && y == 2;
        w.elevation.push_back(static_cast<std::int16_t>(border ? -20 : 40 + 7 * static_cast<int>(x + y)));
        w.water.push_back(border ? WATER_OCEAN : lake ? WATER_LAKE : WATER_DRY);
        w.biome.push_back(border ? std::uint8_t{1} : lake ? std::uint8_t{2} : std::uint8_t{4});
        std::uint16_t r = 0;
        if (!border) {
            r = static_cast<std::uint16_t>(1 + (x >= 4 ? 1 : 0) + (y >= 4 ? 2 : 0));
        }
        w.region.push_back(r);
    }
    // The rows need not match the layer (report reads the tables only); the
    // cells give areas 0.02 km^2 and more, two upland regions, an isolated
    // region 4.
    w.rows = {row_of(1, "North Wood", 9, 9), row_of(2, "East Wood", 2930, 100), row_of(3, "South Downs", 11719, 2930),
              row_of(4, "Central Isle", 1, 1)};
    w.borders = {region_api::BorderRow{1, 2, 48, 50, 3, 1, 3, 0, 0, 0, 0},
                 region_api::BorderRow{1, 3, 64, 60, 1, 3, 0, 4, 6, 2, 1},
                 region_api::BorderRow{2, 3, 16, 70, 4, 3, 1, 1, 7, 0, 0}};
    return w;
}

std::string region_sql(const region_api::RegionRow& r) {
    std::string s = "INSERT INTO region VALUES (" + std::to_string(r.id) + ", '" + r.name + "'";
    for (const std::int64_t v : {r.biome, r.x_min, r.y_min, r.x_max, r.y_max, r.seed_x, r.seed_y, r.landmass, r.cells,
                                 r.lake_cells, r.river_cells, r.river_class_max, r.coast_cells, r.walk_cells,
                                 r.walk_area, r.elev_mean_dm, r.elev_max_dm, r.fertility_mean, r.forage_mean,
                                 r.timber_mean, r.ore_mean, r.fresh_water_mean, r.habitability_mean,
                                 r.habitability_p90, r.ore_rich_cells, r.ore_reach_cells}) {
        s += ", " + std::to_string(v);
    }
    return s + ");";
}

std::string border_sql(const region_api::BorderRow& b) {
    std::string s = "INSERT INTO region_border VALUES (" + std::to_string(b.a);
    for (const std::int64_t v : {b.b, b.length_m, b.low_dm, b.low_x, b.low_y, b.walk_pairs, b.river_pairs,
                                 b.river_class_max, b.pass_id, b.passes}) {
        s += ", " + std::to_string(v);
    }
    return s + ");";
}

// Writes the world; with_region false leaves the layer out, `region`
// replaces its grid; `rows` (default: the world's) and `extra` SQL.
std::string write(const dbtest::TempDir& dir, const std::string& file, const World& w, bool with_region = true,
                  const LayerGrid* region = nullptr, std::optional<std::string> rows_sql = std::nullopt,
                  const std::string& extra = {}) {
    std::vector<worldgen::stages::testing::FixtureLayer> layers;
    layers.push_back({"elevation", p004::grid_of(W, W, w.elevation), Codec::delta_varint});
    layers.push_back({"water", p004::grid_of(W, W, w.water), Codec::delta_varint});
    // No biome layer: a biome layer would need table biome (P1-04 9), and the
    // region keys do not read it.
    if (with_region) {
        layers.push_back({"region", region != nullptr ? *region : p004::grid_of(W, W, w.region), Codec::delta_varint});
    }
    const std::string out = dir.file(file);
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 5, layers).ok());
    std::string sql;
    if (rows_sql.has_value()) {
        sql = *rows_sql;
    } else {
        for (const auto& r : w.rows) {
            sql += region_sql(r);
        }
        for (const auto& b : w.borders) {
            sql += border_sql(b);
        }
    }
    sql += extra;
    if (!sql.empty()) {
        REQUIRE(worldgen::db::testing::execute_on_file(out, sql).ok());
    }
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

}  // namespace

// --- report (9, 10.1 test 7) ------------------------------------------------------------------------

TEST_CASE("P1-06 9, 10.1 test 7: the eight keys on a hand-made 8 x 8 world, last in [geography]") {
    dbtest::TempDir dir("regions-report");
    const World w = make_world();
    const std::string text = report_of(write(dir, "w.db", w));
    const auto want = report_values(w.rows, w.borders);
    CHECK(keys_text(want) ==
          "\nregions = 4\nregion_area_km2 = 0.00 0.75 3.00\nregion_borders = 3\nregion_borders_walkable = 2\n"
          "region_borders_river = 2\nregion_borders_pass = 1\nregions_isolated = 1\nregions_upland = 1\n");
    CHECK(ends_with(text, keys_text(want)));
    // [tables] lists the three tables in byte order (P0-04 amendment 18 (a)).
    CHECK(text.find("\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nrace_relation = 0\nregion = 4\nregion_biome = 0\nregion_border = 3\n") != std::string::npos);   // race_relation: schema 10 (P5-03)
}

TEST_CASE("P1-06 9: the area percentiles use the rank rule; km^2 with two decimals, rounded half away") {
    // 2930 cells = 750080 m^2 -> 0.75; 11719 cells -> 3.00004 -> 3.00; 9 cells
    // -> 0.002304 -> 0.00; 20 cells -> 0.00512 -> 0.01; 1954 -> 0.500224 -> 0.50.
    CHECK(km2(2930) == "0.75");
    CHECK(km2(11719) == "3.00");
    CHECK(km2(9) == "0.00");
    CHECK(km2(20) == "0.01");
    CHECK(km2(1954) == "0.50");
    CHECK(km2(39063) == "10.00");
    // Two rows: p50 rank min(1, 1) = 1, the larger.
    std::vector<region_api::RegionRow> two = {row_of(1, "A B", 100, 100), row_of(2, "C D", 4000, 4000)};
    CHECK(report_values(two, {})[1].second == "0.03 1.02 1.02");
}

TEST_CASE("P1-06 9, 10.1 test 7: with the region layer and no rows, region_area_km2 is n/a n/a n/a") {
    dbtest::TempDir dir("regions-report-empty");
    const World w = make_world();
    const std::string text = report_of(write(dir, "w.db", w, true, nullptr, std::string()));
    CHECK(ends_with(text, "\nregions = 0\nregion_area_km2 = n/a n/a n/a\nregion_borders = 0\nregion_borders_walkable = "
                          "0\nregion_borders_river = 0\nregion_borders_pass = 0\nregions_isolated = 0\nregions_upland = 0\n"));
    // Without the layer the group is not printed, rows or not.
    const std::string none = report_of(write(dir, "n.db", w, false));
    CHECK(none.find("\nregions = ") == std::string::npos);
    CHECK(none.find("region_area_km2") == std::string::npos);
    CHECK(none.find("\nregion = 4\n") != std::string::npos);
}

TEST_CASE("P1-06 9, 10.1 test 7: each corrupt case is an input error with its message") {
    dbtest::TempDir dir("regions-report-corrupt");
    const World w = make_world();
    // A region layer that is not u16, or not the size of elevation.
    const LayerGrid i16 = p004::grid_of(W, W, std::vector<std::int16_t>(W * W, 1));
    std::string e = report_error(write(dir, "a.db", w, true, &i16));
    CHECK(e.find("corrupt layer 'region': type i16, expected u16") != std::string::npos);
    const LayerGrid u8 = p004::grid_of(W, W, std::vector<std::uint8_t>(W * W, 1));
    e = report_error(write(dir, "a2.db", w, true, &u8));
    CHECK(e.find("corrupt layer 'region': type u8, expected u16") != std::string::npos);
    const LayerGrid small = p004::grid_of(4, W, std::vector<std::uint16_t>(4 * W, 1));
    e = report_error(write(dir, "b.db", w, true, &small));
    CHECK(e.find("corrupt layer 'region': size 4x8, expected 8x8") != std::string::npos);
    // A border row with a not below b, or naming an unknown region (first
    // failing row in key order).
    std::string rows;
    for (const auto& r : w.rows) {
        rows += region_sql(r);
    }
    e = report_error(write(dir, "c.db", w, true, nullptr, rows + border_sql({2, 1, 16, 0, 0, 0, 0, 0, 0, 0, 0})));
    CHECK(e.find("corrupt table 'region_border': row (2, 1): a not below b") != std::string::npos);
    e = report_error(write(dir, "c2.db", w, true, nullptr, rows + border_sql({3, 3, 16, 0, 0, 0, 0, 0, 0, 0, 0})));
    CHECK(e.find("corrupt table 'region_border': row (3, 3): a not below b") != std::string::npos);
    e = report_error(write(dir, "d.db", w, true, nullptr,
                           rows + border_sql({1, 9, 16, 0, 0, 0, 0, 0, 0, 0, 0}) +
                               border_sql({7, 8, 16, 0, 0, 0, 0, 0, 0, 0, 0})));
    CHECK(e.find("corrupt table 'region_border': row (1, 9): unknown region 9") != std::string::npos);
    e = report_error(write(dir, "d2.db", w, true, nullptr, rows + border_sql({0, 2, 16, 0, 0, 0, 0, 0, 0, 0, 0})));
    CHECK(e.find("corrupt table 'region_border': row (0, 2): unknown region 0") != std::string::npos);
    // A region row with cells below 1.
    region_api::RegionRow zero = row_of(2, "East Wood", 0, 0);
    e = report_error(write(dir, "f.db", w, true, nullptr, region_sql(w.rows[0]) + region_sql(zero)));
    CHECK(e.find("corrupt table 'region': row 2: cells below 1") != std::string::npos);
    // The corrupt layer is refused whenever it exists, tables or not.
    e = report_error(write(dir, "g.db", w, true, &i16, std::string()));
    CHECK(e.find("corrupt layer 'region'") != std::string::npos);
}

// --- render (9, 10.1 test 8) ------------------------------------------------------------------------

TEST_CASE("P1-06 9, 10.1 test 8: layer region uses ids under auto; the palette of P1-00 10.2") {
    CHECK(worldgen::inspect::registered_palette("region") == worldgen::inspect::Palette::ids);
    const Grid g{5, 3};
    std::vector<std::uint16_t> v = {0, 1, 2, 3, 65535, 7, 7, 0, 300, 1, 2, 2, 2, 40000, 9};
    const LayerGrid grid{5, 3, v};
    CHECK(worldgen::inspect::auto_palette("region", grid) == worldgen::inspect::Palette::ids);
    const std::vector<std::uint8_t> px = worldgen::inspect::render_rgb(grid, worldgen::inspect::Palette::ids, 1);
    REQUIRE(px.size() == 45U);
    for (std::size_t i = 0; i < v.size(); ++i) {
        CAPTURE(i);
        const Rgb want = ids_colour(v[i]);
        CHECK(Rgb{px[3 * i], px[3 * i + 1], px[3 * i + 2]} == want);
    }
    CHECK(ids_colour(0) == Rgb{0, 0, 0});
    static_cast<void>(g);
}

TEST_CASE("P0-04 amendment 18 (e), 10.1 test 8: composite regions = composite biomes with borders in black") {
    const World w = make_world();
    const LayerGrid e = p004::grid_of(W, W, w.elevation);
    const LayerGrid water = p004::grid_of(W, W, w.water);
    const LayerGrid biome = p004::grid_of(W, W, w.biome);
    const LayerGrid region = p004::grid_of(W, W, w.region);
    const std::vector<std::uint8_t> zeros(W * W, 0);
    const LayerGrid river = p004::grid_of(W, W, zeros);
    const LayerGrid flow = p004::grid_of(W, W, std::vector<std::uint8_t>(W * W, 4));
    const std::vector<worldgen::inspect::BiomeColour> table = {
        {1, 0x183878}, {2, 0x4080CC}, {3, 0x2E6034}, {4, 0x96AA5A}};
    CHECK_FALSE(region_api::composite_problem(e, &water, &river, &flow, biome, region).has_value());
    for (const std::uint32_t scale : {1U, 3U}) {
        CAPTURE(scale);
        const std::vector<std::uint8_t> base =
            worldgen::inspect::render_composite_biomes(e, biome, table, &water, &river, &flow, scale);
        const std::vector<std::uint8_t> got = region_api::composite(e, biome, region, table, &water, &river, &flow, scale);
        REQUIRE(got.size() == base.size());
        const std::vector<bool> black = border_cells(Grid{static_cast<std::int32_t>(W), static_cast<std::int32_t>(W)}, w.region);
        std::size_t blacks = 0;
        bool ok = true;
        for (std::uint32_t py = 0; py < W * scale && ok; ++py) {
            for (std::uint32_t px = 0; px < W * scale; ++px) {
                const std::size_t cell = (py / scale) * W + px / scale;
                const std::size_t at = 3U * (static_cast<std::size_t>(py) * W * scale + px);
                const bool is_black = black[cell];
                for (std::size_t c = 0; c < 3; ++c) {
                    const std::uint8_t want = is_black ? std::uint8_t{0} : base[at + c];
                    if (got[at + c] != want) {
                        CAPTURE(px);
                        CAPTURE(py);
                        CHECK(int{got[at + c]} == int{want});
                        ok = false;
                        break;
                    }
                }
                blacks += is_black ? 1U : 0U;
            }
        }
        // The quadrant borders: column x = 3 (y 1..6) and row y = 3 (x 1..6), 11 cells.
        CHECK(blacks == 11U * scale * scale);
    }
    // The 3 x 2 example of the amendment.
    const LayerGrid e2 = p004::grid_of(3, 2, std::vector<std::int16_t>(6, 100));
    const LayerGrid b2 = p004::grid_of(3, 2, std::vector<std::uint8_t>(6, 4));
    const LayerGrid r2 = p004::grid_of(3, 2, std::vector<std::uint16_t>{1, 1, 2, 1, 2, 2});
    const std::vector<std::uint8_t> got = region_api::composite(e2, b2, r2, table, nullptr, nullptr, nullptr, 1);
    const std::vector<std::uint8_t> base = worldgen::inspect::render_composite_biomes(e2, b2, table, nullptr, nullptr, nullptr, 1);
    for (std::size_t i = 0; i < 6; ++i) {
        CAPTURE(i);
        const bool blk = i == 1 || i == 3;
        CHECK((got[3 * i] == 0 && got[3 * i + 1] == 0 && got[3 * i + 2] == 0) == blk);
        if (!blk) {
            CHECK(got[3 * i] == base[3 * i]);
        }
    }
}

TEST_CASE("P0-04 amendment 18 (e): composite regions refuses a region layer that is not u16 or not the elevation's size") {
    const World w = make_world();
    const LayerGrid e = p004::grid_of(W, W, w.elevation);
    const LayerGrid biome = p004::grid_of(W, W, w.biome);
    const LayerGrid u8 = p004::grid_of(W, W, std::vector<std::uint8_t>(W * W, 1));
    const auto p1 = region_api::composite_problem(e, nullptr, nullptr, nullptr, biome, u8);
    REQUIRE(p1.has_value());
    CHECK(p1->find("layer 'region' is u8, expected u16") != std::string::npos);
    const LayerGrid small = p004::grid_of(4, W, std::vector<std::uint16_t>(4 * W, 1));
    const auto p2 = region_api::composite_problem(e, nullptr, nullptr, nullptr, biome, small);
    REQUIRE(p2.has_value());
    CHECK(p2->find("layer 'region' is not the size of layer 'elevation'") != std::string::npos);
    // The biomes composite's checks come first.
    const LayerGrid bad_biome = p004::grid_of(W, W, std::vector<std::int16_t>(W * W, 1));
    const auto p3 = region_api::composite_problem(e, nullptr, nullptr, nullptr, bad_biome, u8);
    REQUIRE(p3.has_value());
    CHECK(p3->find("'biome'") != std::string::npos);
}

TEST_CASE("P1-06 9 (ruling): check order layer, table region, table region_border; tables checked without the layer") {
    dbtest::TempDir dir("regions-report-order");
    const World w = make_world();
    const LayerGrid i16 = p004::grid_of(W, W, std::vector<std::int16_t>(W * W, 1));
    const std::string bad_region = region_sql(w.rows[0]) + region_sql(row_of(2, "East Wood", 0, 0));
    const std::string bad_border = border_sql({2, 1, 16, 0, 0, 0, 0, 0, 0, 0, 0});
    // Layer first.
    std::string e = report_error(write(dir, "a.db", w, true, &i16, bad_region + bad_border));
    CHECK(e.find("corrupt layer 'region': type i16, expected u16") != std::string::npos);
    // Then table region before table region_border.
    e = report_error(write(dir, "b.db", w, true, nullptr, bad_region + bad_border));
    CHECK(e.find("corrupt table 'region': row 2: cells below 1") != std::string::npos);
    // Within a table, the first failing row in key order.
    std::string rows;
    for (const auto& r : w.rows) {
        rows += region_sql(r);
    }
    e = report_error(write(dir, "c.db", w, true, nullptr,
                           rows + border_sql({3, 2, 16, 0, 0, 0, 0, 0, 0, 0, 0}) + border_sql({1, 7, 16, 0, 0, 0, 0, 0, 0, 0, 0})));
    CHECK(e.find("corrupt table 'region_border': row (1, 7): unknown region 7") != std::string::npos);
    // The table checks run without the layer.
    e = report_error(write(dir, "d.db", w, false, nullptr, bad_region));
    CHECK(e.find("corrupt table 'region': row 2: cells below 1") != std::string::npos);
    e = report_error(write(dir, "e.db", w, false, nullptr, rows + bad_border));
    CHECK(e.find("corrupt table 'region_border': row (2, 1): a not below b") != std::string::npos);
}
