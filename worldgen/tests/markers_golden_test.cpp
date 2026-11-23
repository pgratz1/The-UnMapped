// Spec P4-04 10.3: the golden file worldgen/tests/golden/p4-04-markers.txt, written by
// reference/p4_markers_reference.py golden: per case the static dump, M(Y) at the golden years, M(0) ... M(E),
// and for the world cases the overlay at scale 1 (first ruined year, E / 2, E) and at scale 3 (E); once, the
// glyph dump (markers_glyph_test.cpp). Fixtures: the P3-07 fixtures, T1 and T2 (each written through the writer
// API); world cases: the 512 cases here, the 1024 cases in the skipped suite "markers-slow" (CTest
// worldgen-tests-slow-markers). Never regenerate the golden file to make this test pass.
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "markers_support.hpp"

namespace {

using namespace mktest;

void check_world(const tltest::WorldSpec& spec) {
    const std::string path = tltest::world(spec);
    {
        const Indexed o = open_indexed(path);
        check_dumps(spec.name, o.h, o.index);
    }
    check_overlays(spec.name, path);
}

}  // namespace

TEST_SUITE("markers") {

TEST_CASE("P4-04 10.3: every case of the golden file is one this test knows") {
    std::vector<std::string> known = fixture_names();
    for (const tltest::WorldSpec& s : tltest::world_cases_512()) known.push_back(s.name);
    for (const tltest::WorldSpec& s : tltest::world_cases_1024()) known.push_back(s.name);
    for (const auto& [name, c] : golden().cases) {
        CAPTURE(name);
        CHECK(std::find(known.begin(), known.end(), name) != known.end());
        CHECK_FALSE(c.static_hash.empty());
        CHECK_FALSE(c.years_hash.empty());
        CHECK_FALSE(c.markers.empty());
    }
    CHECK(golden().cases.size() == known.size());
    CHECK_FALSE(golden().glyphs.empty());
}

TEST_CASE("P4-04 10.3: fixtures (P3-07, T1, T2)") {
    for (const std::string& name : fixture_names()) {
        CAPTURE(name);
        const Indexed o = open_indexed(fixture_world(name));
        check_dumps(name, o.h, o.index);
        CHECK(golden_case(name).overlay.empty());
        CHECK_FALSE(golden_case(name).overlay3.has_value());
    }
}

TEST_CASE("P4-04 10.3: the 512 world cases") {
    for (const tltest::WorldSpec& s : tltest::world_cases_512()) {
        CAPTURE(s.name);
        check_world(s);
    }
}

}  // TEST_SUITE

TEST_SUITE("markers-slow" * doctest::skip()) {

TEST_CASE("P4-04 10.3: the 1024 world cases") {
    for (const tltest::WorldSpec& s : tltest::world_cases_1024()) {
        CAPTURE(s.name);
        check_world(s);
    }
}

}  // TEST_SUITE
