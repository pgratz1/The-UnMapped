// report on worlds with tables biome and climate (spec P1-04 9 and 10.1
// test 10; P1-03 9 `wind_toward`; P1-00 10.3).
//
// Hand-made worlds: layers written by stages::testing::write_fixture_world,
// table rows inserted with raw SQL. The expected values are recomputed by
// the test (p1_04_biomes_support.hpp) and were checked by
// reference/p1_biomes_reference.py (check_tables).

#include <doctest/doctest.h>

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
// below (9 has no table row, so it counts in neither share nor patches).
const std::vector<std::uint8_t> BIOMES = {1, 1, 1, 1, 1, 1,  //
                                          1, 3, 3, 4, 2, 1,  //
                                          1, 3, 4, 4, 2, 1,  //
                                          1, 5, 5, 3, 3, 1,  //
                                          1, 9, 6, 6, 7, 1,  //
                                          1, 1, 1, 1, 1, 1};

constexpr std::string_view ROWS_SQL =
    "INSERT INTO biome VALUES (1, 'ocean', 'Ocean', 1587320, 1);"
    "INSERT INTO biome VALUES (2, 'lake', 'Lake', 4227276, 1);"
    "INSERT INTO biome VALUES (3, 'temperate_forest', 'Temperate forest', 3039284, 0);"
    "INSERT INTO biome VALUES (4, 'grassland', 'Grassland', 9874010, 0);"
    "INSERT INTO biome VALUES (5, 'marsh', 'Marsh', 6057556, 0);"
    "INSERT INTO biome VALUES (6, 'highland_moor', 'Highland moor', 7890024, 0);"
    "INSERT INTO biome VALUES (7, 'alpine', 'Alpine', 12500660, 0);"
    "INSERT INTO biome VALUES (8, 'blighted_waste', 'Blighted waste', 6313030, 0);";

std::string world(const dbtest::TempDir& dir, const std::string& name, const std::string& sql, bool with_biome = true) {
    std::vector<std::int16_t> e(36, 50);
    for (std::size_t i = 0; i < 36; ++i) {
        if (BIOMES[i] == OCEAN) {
            e[i] = -20;
        }
    }
    std::vector<worldgen::stages::testing::FixtureLayer> layers;
    layers.push_back({"elevation", p004::grid_of(6, 6, e), worldgen::db::Codec::delta_varint});
    if (with_biome) {
        layers.push_back({"biome", p004::grid_of(6, 6, BIOMES), worldgen::db::Codec::delta_varint});
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
          "temperate_forest:0.3846 grassland:0.2308 marsh:0.1538 highland_moor:0.1538 alpine:0.0769 "
          "blighted_waste:0.0000");
    CHECK(biome_patches(Grid{6, 6}, BIOMES, shipped_defs()) == 6);
    // The two keys close [geography] (no climate or hydrology layers here).
    CHECK(ends_with(text, "\nbiome_share = " + share + "\nbiome_patches = 6\n"));
    // [tables], ascending name: biome, climate, layer, meta, pass.
    CHECK(text.find("\n[tables]\nbiome = 8\nclimate = 0\nlayer = 2\nmeta = 8\npass = 0\n[layers]\n") != std::string::npos);
    // No climate row: no wind_toward.
    CHECK(text.find("wind_toward") == std::string::npos);
}

TEST_CASE("P1-04 9: the biome keys need the biome layer; without it they are absent") {
    dbtest::TempDir dir("biomes-report-nolayer");
    const std::string text = report_of(world(dir, "w.db", std::string(ROWS_SQL), false));
    CHECK(text.find("biome_share") == std::string::npos);
    CHECK(text.find("biome_patches") == std::string::npos);
    CHECK(text.find("\n[tables]\nbiome = 8\nclimate = 0\n") != std::string::npos);
}

TEST_CASE("P1-03 9: wind_toward from table climate, first of the climate group, without climate layers") {
    dbtest::TempDir dir("biomes-report-wind");
    for (std::int64_t k = 0; k < 8; ++k) {
        CAPTURE(k);
        const std::string sql = std::string(ROWS_SQL) + "INSERT INTO climate VALUES (1, " + std::to_string(k) + ");";
        const std::string text = report_of(world(dir, "w" + std::to_string(k) + ".db", sql));
        const std::string line = "\nwind_toward = " + std::string(WIND_NAMES[static_cast<std::size_t>(k)]) + "\n";
        const std::size_t at = text.find(line);
        REQUIRE(at != std::string::npos);
        // Before the biome group, after the pass keys.
        CHECK(text.substr(at + line.size(), 14) == "biome_share = ");
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
    const std::vector<Bad> bads = {
        {"two rows", "INSERT INTO climate VALUES (1, 1); INSERT INTO climate VALUES (2, 1);"},
        {"id 2", "INSERT INTO climate VALUES (2, 1);"},
        {"wind_toward 8", "INSERT INTO climate VALUES (1, 8);"},
        {"wind_toward -1", "INSERT INTO climate VALUES (1, -1);"},
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
