// report on worlds with tables biome and climate (spec P1-04 9 and 10.1
// test 10; P1-03 9 `wind_toward`; P1-00 10.3).
//
// Hand-made worlds: layers written by stages::testing::write_fixture_world,
// table rows inserted with raw SQL. The expected values are recomputed by
// the test (p1_04_biomes_support.hpp) and were checked by
// reference/p1_biomes_reference.py (check_tables).

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "p1_04_biomes_support.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/report.hpp"
#include "worldgen/stages/testing.hpp"

using namespace biome_test;

namespace {

// The 6 x 6 world of 10.1 test 10: an ocean ring; inside, the biome codes
// below (every value has a row: a value without one is a corrupt layer,
// P1-04 9 as amended).
const std::vector<std::uint8_t> BIOMES = {1, 1, 1, 1, 1, 1,  //
                                          1, 3, 3, 4, 2, 1,  //
                                          1, 3, 4, 4, 2, 1,  //
                                          1, 5, 5, 3, 3, 1,  //
                                          1, 8, 6, 6, 7, 1,  //
                                          1, 1, 1, 1, 1, 1};

constexpr std::string_view ROWS_SQL =
    "INSERT INTO biome VALUES (1, 'ocean', 'Ocean', 1587320, 1);"
    "INSERT INTO biome VALUES (2, 'lake', 'Lake', 4227276, 1);"
    "INSERT INTO biome VALUES (3, 'temperate_forest', 'Temperate forest', 3039284, 0);"
    "INSERT INTO biome VALUES (4, 'grassland', 'Grassland', 9874010, 0);"
    "INSERT INTO biome VALUES (5, 'marsh', 'Marsh', 6988694, 0);"
    "INSERT INTO biome VALUES (6, 'highland_moor', 'Highland moor', 8283776, 0);"
    "INSERT INTO biome VALUES (7, 'alpine', 'Alpine', 12500660, 0);"
    "INSERT INTO biome VALUES (8, 'blighted_waste', 'Blighted waste', 6313030, 0);"
    "INSERT INTO biome VALUES (9, 'heath', 'Heath', 9725514, 0);"
    "INSERT INTO biome VALUES (10, 'steppe', 'Steppe', 14733974, 0);"
    "INSERT INTO biome VALUES (11, 'wet_woodland', 'Wet woodland', 2314334, 0);"
    "INSERT INTO biome VALUES (12, 'fen', 'Fen', 9076848, 0);";

// The same 6 x 6 ring with every natural code of the third version inside
// (P1-04 10.1 test 12: ten keys in code order).
const std::vector<std::uint8_t> BIOMES12 = {1, 1,  1,  1,  1,  1,  //
                                            1, 3,  9,  10, 2,  1,  //
                                            1, 11, 12, 12, 2,  1,  //
                                            1, 5,  4,  3,  9,  1,  //
                                            1, 6,  7,  10, 10, 1,  //
                                            1, 1,  1,  1,  1,  1};

std::string world(const dbtest::TempDir& dir, const std::string& name, const std::string& sql, bool with_biome = true,
                  const worldgen::db::LayerGrid* biome = nullptr) {
    std::vector<std::int16_t> e(36, 50);
    for (std::size_t i = 0; i < 36; ++i) {
        if (BIOMES[i] == OCEAN) {
            e[i] = -20;
        }
    }
    std::vector<worldgen::stages::testing::FixtureLayer> layers;
    layers.push_back({"elevation", p004::grid_of(6, 6, e), worldgen::db::Codec::delta_varint});
    if (with_biome) {
        layers.push_back({"biome", biome != nullptr ? *biome : p004::grid_of(6, 6, BIOMES),
                          worldgen::db::Codec::delta_varint});
    }
    const std::string out = dir.file(name);
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 3, layers).ok());
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

bool ends_with(std::string_view s, std::string_view tail) {
    return s.size() >= tail.size() && s.substr(s.size() - tail.size()) == tail;
}

}  // namespace

TEST_CASE("P1-04 9, 10.1 test 10: biome_share and biome_patches on a hand-made 6 x 6 world") {
    dbtest::TempDir dir("biomes-report");
    const std::string text = report_of(world(dir, "w.db", std::string(ROWS_SQL)));
    const std::string share = biome_share(BIOMES, shipped_defs());
    CHECK(share ==
          "temperate_forest:0.3571 grassland:0.2143 marsh:0.1429 highland_moor:0.1429 alpine:0.0714 "
          "blighted_waste:0.0714 heath:0.0000 steppe:0.0000 wet_woodland:0.0000 fen:0.0000");
    CHECK(biome_patches(Grid{6, 6}, BIOMES, shipped_defs()) == 7);
    // The two keys close [geography] (no climate or hydrology layers here).
    CHECK(ends_with(text, "\nbiome_share = " + share + "\nbiome_patches = 7\n"));
    // [tables], ascending name: biome, climate, layer, meta, pass and the
    // three region tables of schema 5.
    CHECK(text.find("\n[tables]\nage = 0\nbiome = 12\nclimate = 0\nculture = 0\ngod = 0\nlayer = 2\nmeta = 16\npass = 0\nrace = 0\nrace_biome = 0\nregion = 0\nregion_biome = "
                    "0\nregion_border = 0\n[layers]\n") != std::string::npos);
    // No climate row: no wind_toward, no temp_offset_c.
    CHECK(text.find("wind_toward") == std::string::npos);
    CHECK(text.find("temp_offset_c") == std::string::npos);
}

TEST_CASE("P1-04 9, 10.1 test 12: biome_share lists ten keys in code order on a world with every natural code") {
    dbtest::TempDir dir("biomes-report-12");
    const worldgen::db::LayerGrid b = p004::grid_of(6, 6, BIOMES12);
    const std::string text = report_of(world(dir, "w.db", std::string(ROWS_SQL), true, &b));
    const std::string share = biome_share(BIOMES12, shipped_defs());
    CHECK(share ==
          "temperate_forest:0.1429 grassland:0.0714 marsh:0.0714 highland_moor:0.0714 alpine:0.0714 "
          "blighted_waste:0.0000 heath:0.1429 steppe:0.2143 wet_woodland:0.0714 fen:0.1429");
    CHECK(biome_patches(Grid{6, 6}, BIOMES12, shipped_defs()) == 12);
    CHECK(ends_with(text, "\nbiome_share = " + share + "\nbiome_patches = 12\n"));
}

TEST_CASE("P1-04 9: the biome keys need the biome layer; without it they are absent") {
    dbtest::TempDir dir("biomes-report-nolayer");
    const std::string text = report_of(world(dir, "w.db", std::string(ROWS_SQL), false));
    CHECK(text.find("biome_share") == std::string::npos);
    CHECK(text.find("biome_patches") == std::string::npos);
    CHECK(text.find("\n[tables]\nage = 0\nbiome = 12\nclimate = 0\n") != std::string::npos);
}

TEST_CASE("P1-03 9: wind_toward from table climate, first of the climate group, without climate layers") {
    dbtest::TempDir dir("biomes-report-wind");
    for (std::int64_t k = 0; k < 8; ++k) {
        CAPTURE(k);
        // temp_offset_dc (schema 4) over the formatter's cases: -19, 20, 0, -5, ...
        const std::array<std::int64_t, 8> offs = {-19, 20, 0, -5, 50, -50, 9, -10};
        const std::array<const char*, 8> texts = {"-1.9", "2.0", "0.0", "-0.5", "5.0", "-5.0", "0.9", "-1.0"};
        const std::size_t ki = static_cast<std::size_t>(k);
        const std::string sql = std::string(ROWS_SQL) + "INSERT INTO climate VALUES (1, " + std::to_string(k) + ", " +
                                std::to_string(offs[ki]) + ");";
        const std::string text = report_of(world(dir, "w" + std::to_string(k) + ".db", sql));
        const std::string line = "\nwind_toward = " + std::string(WIND_NAMES[ki]) + "\n";
        const std::size_t at = text.find(line);
        REQUIRE(at != std::string::npos);
        // temp_offset_c right after wind_toward (P1-03 9), then the biome group.
        const std::string off_line = std::string("temp_offset_c = ") + texts[ki] + "\n";
        CHECK(text.substr(at + line.size(), off_line.size()) == off_line);
        CHECK(text.substr(at + line.size() + off_line.size(), 14) == "biome_share = ");
        CHECK(text.find("\npasses_walkable = 0" + line) != std::string::npos);
        CHECK(text.find("\nclimate = 1\n") != std::string::npos);
    }
}

TEST_CASE("P1-03 9: a malformed climate table is a corrupt-data error naming table climate") {
    dbtest::TempDir dir("biomes-report-badclimate");
    struct Bad {
        const char* what;
        const char* sql;
    };
    // Each reason of P1-03 9 (0.9.0), in the order checked; the message names
    // the table and gives the reason.
    struct Reason {
        const char* sql;
        const char* reason;
    };
    const std::vector<Reason> reasons = {
        {"INSERT INTO climate VALUES (1, 1, 0); INSERT INTO climate VALUES (2, 1, 0);", "2 rows, expected at most 1"},
        {"DROP TABLE climate; CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL) STRICT;"
         "INSERT INTO climate VALUES (1, 1);",
         "the row does not have three integer columns"},
        {"INSERT INTO climate VALUES (2, 1, 0);", "id 2, expected 1"},
        {"INSERT INTO climate VALUES (1, 8, 0);", "wind_toward 8 outside 0..7"},
        {"INSERT INTO climate VALUES (1, 1, 51);", "temp_offset_dc 51 outside -50..50"},
        {"INSERT INTO climate VALUES (1, 1, -51);", "temp_offset_dc -51 outside -50..50"},
        {"INSERT INTO climate VALUES (1, 8, 51);", "wind_toward 8 outside 0..7"},  // order: wind first
    };
    int kr = 0;
    for (const Reason& rr : reasons) {
        CAPTURE(rr.reason);
        const std::string out = world(dir, "r" + std::to_string(kr++) + ".db", std::string(ROWS_SQL) + rr.sql);
        auto w = worldgen::db::WorldReader::open(out);
        REQUIRE(w.ok());
        const auto r = worldgen::inspect::build_report(w.value());
        REQUIRE_FALSE(r.ok());
        CHECK(r.error().message.find("corrupt table 'climate': " + std::string(rr.reason)) != std::string::npos);
    }
    // The bounds themselves are accepted.
    for (const char* ok : {"INSERT INTO climate VALUES (1, 7, 50);", "INSERT INTO climate VALUES (1, 0, -50);"}) {
        CAPTURE(ok);
        const std::string text = report_of(world(dir, "ok" + std::to_string(kr++) + ".db", std::string(ROWS_SQL) + ok));
        CHECK(text.find("temp_offset_c = ") != std::string::npos);
    }
    const std::vector<Bad> bads = {
        {"two rows", "INSERT INTO climate VALUES (1, 1, 0); INSERT INTO climate VALUES (2, 1, 0);"},
        {"id 2", "INSERT INTO climate VALUES (2, 1, 0);"},
        {"wind_toward 8", "INSERT INTO climate VALUES (1, 8, 0);"},
        {"wind_toward -1", "INSERT INTO climate VALUES (1, -1, 0);"},
    };
    int k = 0;
    for (const Bad& b : bads) {
        CAPTURE(b.what);
        const std::string out = world(dir, "b" + std::to_string(k++) + ".db", std::string(ROWS_SQL) + b.sql);
        auto w = worldgen::db::WorldReader::open(out);
        REQUIRE(w.ok());
        const auto r = worldgen::inspect::build_report(w.value());
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == worldgen::db::ErrorCode::corrupt_layer);
        CHECK(r.error().message.find("climate") != std::string::npos);
    }
}

// --- Malformed biome rows and layer (P1-04 9, amended 2026-11-17; 10.1 test 10) ------------------

namespace {

// The report error of a world; "" if it reports.
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

}  // namespace

TEST_CASE("P1-04 9: every row of table biome is checked, in ascending id, first failing check named") {
    dbtest::TempDir dir("biomes-report-badrows");
    struct Bad {
        const char* what;
        std::string sql;
        const char* reason;  // "row <id>: <reason>"
    };
    const std::string rows(ROWS_SQL);
    const std::vector<Bad> bads = {
        {"id 0", rows + "INSERT INTO biome VALUES (0, 'z', 'Z', 0, 0);", "row 0: id outside 1..254"},
        {"id 255", rows + "INSERT INTO biome VALUES (255, 'z', 'Z', 0, 0);", "row 255: id outside 1..254"},
        {"empty key", rows + "UPDATE biome SET key = '' WHERE id = 5;", "row 5: empty key"},
        {"colour -1", rows + "UPDATE biome SET colour = -1 WHERE id = 6;", "row 6: colour outside 0..16777215"},
        {"colour 2^24", rows + "UPDATE biome SET colour = 16777216 WHERE id = 6;", "row 6: colour outside 0..16777215"},
        {"water 2", rows + "UPDATE biome SET water = 2 WHERE id = 7;", "row 7: water not 0 or 1"},
        {"water -1", rows + "UPDATE biome SET water = -1 WHERE id = 3;", "row 3: water not 0 or 1"},
        // The first failing check of a row, in the order listed.
        {"empty key and water 2", rows + "UPDATE biome SET key = '', water = 2 WHERE id = 4;", "row 4: empty key"},
        {"id 300 and empty key", rows + "INSERT INTO biome VALUES (300, '', 'Z', -5, 7);", "row 300: id outside 1..254"},
        // Rows in ascending id: the lowest bad row is reported.
        {"two bad rows", rows + "UPDATE biome SET water = 5 WHERE id = 7; UPDATE biome SET colour = -2 WHERE id = 2;",
         "row 2: colour outside 0..16777215"},
    };
    int k = 0;
    for (const Bad& b : bads) {
        CAPTURE(b.what);
        // With the layer and without it: rows are checked either way.
        for (const bool layer : {true, false}) {
            CAPTURE(layer);
            const std::string err = report_error(world(dir, "b" + std::to_string(k++) + ".db", b.sql, layer));
            CHECK(err.find("biome") != std::string::npos);
            CHECK(err.find(b.reason) != std::string::npos);
        }
    }
}

TEST_CASE("P1-04 9: the biome layer must be u8, of the elevation's size, and every value must have a row") {
    dbtest::TempDir dir("biomes-report-badlayer");
    const std::string rows(ROWS_SQL);
    {  // a value with no row: the lowest such cell, (1, 4) = index 25
        std::vector<std::uint8_t> v = BIOMES;
        v[4 * 6 + 1] = 13;  // codes 1 to 12 have rows since 0.9.0
        v[5 * 6 + 5] = 13;
        const worldgen::db::LayerGrid g = p004::grid_of(6, 6, v);
        const std::string err = report_error(world(dir, "v.db", rows, true, &g));
        CHECK(err.find("'biome'") != std::string::npos);
        CHECK(err.find("value 13 at (1, 4) has no row in table 'biome'") != std::string::npos);
    }
    {  // an empty table with the layer: the error at cell 0
        const std::string err = report_error(world(dir, "e.db", ""));
        CHECK(err.find("value 1 at (0, 0) has no row in table 'biome'") != std::string::npos);
    }
    {  // type i16
        const worldgen::db::LayerGrid g = p004::grid_of(6, 6, std::vector<std::int16_t>(36, 3));
        CHECK(report_error(world(dir, "t.db", rows, true, &g)).find("type i16, expected u8") != std::string::npos);
    }
    {  // size 5 x 6 against a 6 x 6 elevation
        const worldgen::db::LayerGrid g = p004::grid_of(5, 6, std::vector<std::uint8_t>(30, 3));
        CHECK(report_error(world(dir, "s.db", rows, true, &g)).find("size 5x6, expected 6x6") != std::string::npos);
    }
    {  // rows and no layer: neither key, no error
        const std::string text = report_of(world(dir, "n.db", rows, false));
        CHECK(text.find("biome_share") == std::string::npos);
        CHECK(text.find("biome_patches") == std::string::npos);
    }
}

TEST_CASE("P1-04 9 (lead's ruling 2026-11-17): biome_share with no water = 0 rows prints n/a") {
    dbtest::TempDir dir("biomes-report-na");
    std::vector<std::uint8_t> v(36, 1);
    v[14] = 2;
    const worldgen::db::LayerGrid g = p004::grid_of(6, 6, v);
    const std::string sql = "INSERT INTO biome VALUES (1, 'ocean', 'Ocean', 1587320, 1);"
                            "INSERT INTO biome VALUES (2, 'lake', 'Lake', 4227276, 1);";
    const std::string text = report_of(world(dir, "na.db", sql, true, &g));
    CHECK(ends_with(text, "\nbiome_share = n/a\nbiome_patches = 0\n"));
}
