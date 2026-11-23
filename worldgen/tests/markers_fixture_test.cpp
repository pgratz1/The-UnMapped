// Spec P4-04 10.2 item 3: fixture T2 built twice, as P4-01 13.3 builds T1: through the writer API
// (markers_fixture.hpp) and by the reference through SQL (reference/p4_markers_reference.py fixture
// t2-markers OUT.db). The two copies must have the same table hashes; the golden file records the reference's
// (`t2-markers table <name> <hex>`). Also: the history of T2 loads and its rows are what the TOML says.
//
// Written from the spec only; needs nothing of P4-04's API.

#include <doctest/doctest.h>

#include <map>
#include <sstream>
#include <string>

#include "markers_fixture.hpp"

#ifndef UNMAPPED_P4_04_GOLDEN_PATH
#error "UNMAPPED_P4_04_GOLDEN_PATH must name worldgen/tests/golden/p4-04-markers.txt"
#endif

namespace {

std::map<std::string, std::string> golden_tables() {
    std::map<std::string, std::string> out;
    std::istringstream in(hrtest::detail::text_of_file(mktest::fs::path(UNMAPPED_P4_04_GOLDEN_PATH)));
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ls(line);
        std::string name, what, table, hex;
        ls >> name >> what >> table >> hex;
        if (name == mktest::T2 && what == "table") out[table] = hex;
    }
    return out;
}

}  // namespace

TEST_SUITE("markers") {

TEST_CASE("P4-04 10.2 item 3: T2 through the writer API has the reference's table hashes") {
    const std::map<std::string, std::string> want = golden_tables();
    REQUIRE_FALSE(want.empty());
    const std::map<std::string, std::string> got = hrtest::table_hashes(mktest::t2_world());
    CHECK(got == want);
    for (const auto& [t, hex] : want) {
        CAPTURE(t);
        const auto it = got.find(t);
        REQUIRE(it != got.end());
        CHECK(it->second == hex);
    }
}

TEST_CASE("P4-04 10.2 item 3: T2's history loads; its passes and region seeds are written") {
    const mktest::db::WorldReader r = tltest::open_reader(mktest::t2_world());
    const worldgen::history::LoadedHistory h = tltest::load(r);
    CHECK(h.tables.end_year == 2000);
    CHECK(h.tables.settlements.size() == 7U);
    CHECK(h.tables.civilizations.size() == 3U);
    auto regions = mktest::db::read_region_rows(r);
    REQUIRE(regions.ok());
    REQUIRE(regions.value().size() == 4U);
    CHECK(regions.value()[0].seed_x == 40);
    CHECK(regions.value()[0].seed_y == 50);
    CHECK(regions.value()[3].seed_x == 450);
    CHECK(regions.value()[3].seed_y == 450);
    REQUIRE(mktest::t2_passes().size() == 2U);
    CHECK(mktest::t2_passes()[0].path.size() == 8U);
    CHECK(mktest::t2_passes()[1].path.size() == 4U);
}

}  // TEST_SUITE
