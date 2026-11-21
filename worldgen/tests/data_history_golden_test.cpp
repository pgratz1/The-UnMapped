// The content hash of the three data roots against the golden file
// (spec P3-01 8.3 test 9), and the version of the merge (P3-01 2, 7).
//
// worldgen/tests/golden/p3-01-data.txt is written by the test writer's
// independent reference worldgen/tests/reference/p3_data_reference.py (the
// canonical encoding of P0-03 8 over the TOML files, with the schemas
// transcribed from the specs), never by running the C++ under test.

#include <doctest/doctest.h>

#include <map>
#include <ostream>
#include <string>
#include <string_view>

#include "p3_01_history_data.hpp"
#include "p3_01_history_support.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/version.hpp"

using namespace history_test;

TEST_CASE("P3-01 8.3 test 9: the golden file names the three roots") {
    const auto g = golden_hashes();
    REQUIRE(g.size() == 3U);
    for (const char* root : {"game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
        CAPTURE(std::string_view(root));
        REQUIRE(g.count(root) == 1U);
        CHECK(g.at(root).size() == 64U);
    }
    // geo is a copy of game/data; geo-small differs in its elevation tuning
    // (P1-00 6.4) and so in its hash.
    CHECK(g.at("worldgen/tests/data/geo") == g.at("game/data"));
    CHECK(g.at("worldgen/tests/data/geo-small") != g.at("game/data"));
}

TEST_CASE("P3-01 8.3 test 9: content_hash of each data root equals the reference's") {
    const auto g = golden_hashes();
    for (const char* root : {"game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
        CAPTURE(std::string_view(root));
        const Load l = load_disk(source_root() / root);
        INFO(dump(l));
        REQUIRE(l.ok());
        CHECK(worldgen::data::content_hash_hex(*l.content) == g.at(root));
    }
}

TEST_CASE("P3-01 8.3 test 9: the spec listings with the shipped geography data hash to the game/data value") {
    // Independent of the data files on disk: the transcribed listings of
    // P3-01 3.1, 4, 5.1 and P3-02 5 with game/data's biomes/ and worldgen/.
    CHECK(worldgen::data::content_hash_hex(shipped_content()) == golden_hashes().at("game/data"));
}

TEST_CASE("P3-01 2, 6.3, 7: generator version and schema version (0.18.0 since P3-09; schema 7 since P3-04)") {
    CHECK(std::string(worldgen::GENERATOR_VERSION) == "0.18.0");  // P3-09
    CHECK(std::string(worldgen::generator_version()) == "0.18.0");  // P3-09
    CHECK(worldgen::db::SCHEMA_VERSION == 7);  // P3-04: thirteen tables and history_end_year
}
