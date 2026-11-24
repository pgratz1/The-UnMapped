// Spec P4-05 10.1: the golden dumps of every case (the P3-07 fixtures, T1, T2, T3 and the world cases) against
// worldgen/tests/golden/p4-05-browser.txt (reference/p4_browser_reference.py), the groups line, T3 written
// through the writer API against the reference's SQL copy (its table hashes), and determinism: the index built
// twice, and on a second process-independent copy of a world, gives the same answers. The 1024 world cases are
// in the skipped suite "browser-slow" (CTest entry worldgen-tests-slow-browser).
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "browser_support.hpp"

namespace {

using namespace brtest;

void check_world(const tltest::WorldSpec& spec) {
    const std::string path = tltest::world(spec);
    check_dumps(spec.name, opened(path));
}

}  // namespace

TEST_SUITE("browser") {

TEST_CASE("P4-05 10.1: T3 through the writer API has the table hashes of the reference's SQL copy") {
    const std::map<std::string, std::string> got = hrtest::table_hashes(t3_world());
    const GoldenCase& g = golden_case(T3);
    REQUIRE_FALSE(g.tables.empty());
    CHECK(got == g.tables);
}

TEST_CASE("P4-05 10.1: the groups line") { CHECK(sha(groups_dump()) == golden().groups); }

TEST_CASE("P4-05 10.1: the dumps of every fixture case") {
    for (const std::string& name : fixture_names()) {
        check_dumps(name, opened(fixture_world(name)));
    }
}

TEST_CASE("P4-05 10.1: the dumps of the 512 world cases") {
    for (const tltest::WorldSpec& spec : tltest::world_cases_512()) check_world(spec);
}

TEST_CASE("P4-05 10.1: every case of the golden file is one this suite computes") {
    std::vector<std::string> want = fixture_names();
    for (const tltest::WorldSpec& s : tltest::world_cases_512()) want.push_back(s.name);
    for (const tltest::WorldSpec& s : tltest::world_cases_1024()) want.push_back(s.name);
    std::sort(want.begin(), want.end());
    std::vector<std::string> got;
    for (const auto& [name, c] : golden().cases) got.push_back(name);
    CHECK(got == want);
    for (const auto& [name, c] : golden().cases) {
        CAPTURE(name);
        CHECK(c.dumps.size() == dump_names().size());
    }
}

TEST_CASE("P4-05 11 item 3: the index and its answers are a function of the world") {
    for (const std::string& path : {t3_world(), tltest::world(tltest::world_cases_512()[2])}) {
        CAPTURE(path);
        const Opened& a = opened(path);
        const std::unique_ptr<Opened> b = open_fresh(path);
        CHECK(std::vector<brapi::CivilizationEntry>(a.index.civilizations().begin(), a.index.civilizations().end()) ==
              std::vector<brapi::CivilizationEntry>(b->index.civilizations().begin(), b->index.civilizations().end()));
        CHECK(a.index.events().size() == b->index.events().size());
        CHECK(a.index.digest().summary == b->index.digest().summary);
        for (const std::string& d : dump_names()) {
            CAPTURE(d);
            CHECK(dump(a, d) == dump(*b, d));
        }
        // select reuses its output vector: a dirty vector gives the same ids
        std::vector<std::int64_t> dirty = {99, 98, 97};
        a.index.select(a.h, full_filter(), dirty);
        CHECK(dirty == select(a, full_filter()));
    }
}

}  // TEST_SUITE("browser")

TEST_SUITE("browser-slow" * doctest::skip()) {

TEST_CASE("P4-05 10.1: the dumps of the 1024 world cases") {
    for (const tltest::WorldSpec& spec : tltest::world_cases_1024()) check_world(spec);
}

}  // TEST_SUITE("browser-slow")
