// Spec P3-07 14.3 and 14.4 on the hand-made fixture worlds (worldgen/tests/data/history-report/): the C++
// copy of each fixture, written through the writer API, has the table hashes the reference recorded for
// its own SQL copy (a difference is a fixture-building defect, reported before any report text); then the
// report of each fixture at both levels has the golden hash, and satisfies the invariants of section 13.
// Golden values: worldgen/tests/golden/p3-07-report.txt, written by reference/p3_report_reference.py.
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <string>

#include "history_report_checks.hpp"
#include "history_report_fixture.hpp"

TEST_SUITE("history-report") {

TEST_CASE("P3-07 14.3: each fixture's C++ copy has the reference's table hashes") {
    for (const std::string& name : hrtest::fixture_names()) {
        CAPTURE(name);
        const auto& want = hrtest::golden_case(name).tables;
        REQUIRE(!want.empty());
        const auto got = hrtest::table_hashes(hrtest::fixture_world(name));
        CHECK(got.size() == want.size());
        for (const auto& [table, hex] : want) {
            CAPTURE(table);
            REQUIRE(got.count(table) == 1U);
            CHECK(got.at(table) == hex);
        }
    }
}

#if UNMAPPED_P3_07_REPORT

TEST_CASE("P3-07 14.4: the history report of each fixture has the golden hash at both levels") {
    for (const std::string& name : hrtest::fixture_names()) {
        CAPTURE(name);
        const hrtest::GoldenCase& g = hrtest::golden_case(name);
        const auto r = hrtest::open_world(hrtest::fixture_world(name));
        CHECK(hrtest::sha_hex(hrtest::report_of(r, hrtest::HistoryLevel::summary)) == g.report.at("summary"));
        CHECK(hrtest::sha_hex(hrtest::report_of(r, hrtest::HistoryLevel::full)) == g.report.at("full"));
    }
}

TEST_CASE("P3-07 13 items 1-7 and 14.1 item 8 ([stats] holds [history]): invariants on every fixture") {
    for (const std::string& name : hrtest::fixture_names()) {
        CAPTURE(name);
        const auto r = hrtest::open_world(hrtest::fixture_world(name));
        hrtest::check_invariants(r, hrtest::report_of(r, hrtest::HistoryLevel::summary),
                                 hrtest::report_of(r, hrtest::HistoryLevel::full));
    }
}

TEST_CASE("P3-07 13 item 8 and 12: the report is a function of the file (a second open, a copy)") {
    const std::string path = hrtest::fixture_world("h1-war-and-ruin");
    const std::string copy = hrtest::fixture_copy("h1-war-and-ruin", "same");
    for (const auto level : {hrtest::HistoryLevel::summary, hrtest::HistoryLevel::full}) {
        const std::string a = hrtest::report_of(path, level);
        CHECK(hrtest::report_of(path, level) == a);
        CHECK(hrtest::report_of(copy, level) == a);
    }
}

#endif  // UNMAPPED_P3_07_REPORT

}  // TEST_SUITE
