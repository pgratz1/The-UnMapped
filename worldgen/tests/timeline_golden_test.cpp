// Spec P4-01 13.3 and 13.4: the fixture T1 written through the writer API against the reference's copy
// (table hashes first), and the golden values of worldgen/tests/golden/p4-01-timeline.txt (written by
// reference/p4_timeline_reference.py golden): per case the SHA-256 of the static dump S, of D(Y) for the
// years of item 2, of D(0) ... D(E), and for the world cases the territory RGB buffer at scale 1 for the
// years of item 4. Cases: the P3-07 fixtures and T1, the 512 world cases of tools/ci/world-hashes.sh with
// history (default suite), and the 1024 cases with sample-42-1024 (suite "timeline-slow").
//
// P4-03 9.6: the colours and the territory image are those of P4-03 7 and 8, render_territory with the
// holders of territory_holders (the CLI's call, 9.2); the file was re-recorded at generator 0.21.0.
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "timeline_support.hpp"

namespace {

using namespace tltest;

void check_fixture(const std::string& name) {
    CAPTURE(name);
    const GoldenCase& g = golden_case(name);
    CHECK(g.territory.empty());
    // 13.3: the C++ copy and the reference's copy have the same tables
    const std::map<std::string, std::string> got = hrtest::table_hashes(fixture_world(name));
    REQUIRE_FALSE(g.tables.empty());
    for (const auto& [table, hex] : g.tables) {
        CAPTURE(table);
        const auto it = got.find(table);
        REQUIRE(it != got.end());
        CHECK(it->second == hex);
    }
    CHECK(got.size() == g.tables.size());
    const db::WorldReader r = open_reader(fixture_world(name));
    const wh::LoadedHistory h = load(r);
    check_dumps(name, h);
}

void check_world(const WorldSpec& c) {
    CAPTURE(c.name);
    const GoldenCase& g = golden_case(c.name);
    CHECK(g.tables.empty());
    const db::WorldReader r = open_reader(world(c));
    const wh::LoadedHistory h = load(r);
    check_dumps(c.name, h);
    const WorldLayers layers = read_layers(r);
    const wi::TerritoryBase base = territory_base(layers);
    const std::vector<wi::Rgb> colours = wi::civilization_colours(h.tables);
    std::vector<std::pair<std::int64_t, std::string>> got;
    for (const std::int64_t y : territory_years(h)) {
        const std::vector<std::uint8_t> img = wi::render_territory(base, wi::territory_fill(h.timeline, colours, y),
                                                                   wi::territory_holders(h.timeline, y), 1);
        got.emplace_back(y, worldgen::sha256_hex(img));
    }
    CHECK(got == g.territory);
    // invariant 9 on a real world: an empty fill is the regions composite
    CHECK(wi::render_territory(base, {}, {}, 1) == regions_composite(layers, 1));
}

}  // namespace

TEST_SUITE("timeline") {

TEST_CASE("P4-01 13.3: T1 through the writer API has the reference's table hashes") {
    const GoldenCase& g = golden_case(T1);
    const std::map<std::string, std::string> got = hrtest::table_hashes(t1_world());
    for (const char* table : {"layer", "biome", "region", "event", "event_participant", "event_cause",
                              "settlement_census", "region_control", "relation", "meta"}) {
        CAPTURE(std::string(table));
        REQUIRE(g.tables.count(table) == 1U);
        CHECK(got.at(table) == g.tables.at(table));
    }
}

TEST_CASE("P4-01 13.4: the fixtures (P3-07 and T1) against the golden file") {
    for (const std::string& name : hrtest::fixture_names()) check_fixture(name);
    check_fixture(T1);
}

TEST_CASE("P4-01 13.4: the 512 world cases against the golden file") {
    for (const WorldSpec& c : world_cases_512()) check_world(c);
}

TEST_CASE("P4-01 13.4: every case of the golden file is checked by a test") {
    std::set<std::string> known;
    for (const std::string& n : hrtest::fixture_names()) known.insert(n);
    known.insert(T1);
    for (const WorldSpec& c : world_cases_512()) known.insert(c.name);
    for (const WorldSpec& c : world_cases_1024()) known.insert(c.name);
    std::set<std::string> in_file;
    for (const auto& [name, g] : golden()) {
        in_file.insert(name);
        CHECK_FALSE(g.static_hash.empty());
        CHECK_FALSE(g.years_hash.empty());
        CHECK_FALSE(g.years.empty());
    }
    CHECK(in_file == known);
}

}  // TEST_SUITE

TEST_SUITE("timeline-slow" * doctest::skip()) {

TEST_CASE("P4-01 13.4 (slow): the 1024 world cases and sample-42-1024 against the golden file") {
    for (const WorldSpec& c : world_cases_1024()) check_world(c);
}

}  // TEST_SUITE
