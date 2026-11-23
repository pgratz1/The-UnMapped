// Spec P3-07 r21 section 26 (the history report's format v3, P5-12, generator 0.22.0), the tests of 26.8 items 2 to
// 5, 7 and 8 on the fixture H12 (worldgen/tests/data/history-report/h12-v3.toml), against lines written here from
// the fixture rows and the spec; the whole text is also compared with the reference's (p3-07-report.txt, in
// history_report_fixtures_test.cpp).
//
// The H12 rows (E = 2000): region 2 "The Holt of Yews" is renamed in 800 after the sack e8 of Belford (row e9
// "Belford's Fall", namer c1) and in 1300 after the battle e15 (row e16 "The Red Downs", salience 95); region 4
// "Cor Wold" in 1400 after the battle e17 with no namer (row e18 "The Bloody Wold", culture 0). The war e7 of 500
// names region 2 before both renamings, the claim e19 of 1500 after both; c2's home is region 2. The refuges: s4
// (kind 42 to 1999 = E - 1), s5 (kind 42 to 1200) and s6 (none), all founded by flight from Belford. 1000 notes of
// salience (37 i) mod 101 (as H4) make the summary threshold 78, above the renaming's 60 and below the sack's 90.
//
// Written by the test writer from the spec (phase 2 of P5-12's tests).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "history_report_checks.hpp"
#include "history_report_fixture.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace {

using hrtest::block;
using hrtest::contains_line;
using hrtest::HistoryLevel;

std::string text(HistoryLevel level) {
    static std::map<int, std::string> cache;
    const auto it = cache.find(static_cast<int>(level));
    if (it != cache.end()) return it->second;
    return cache[static_cast<int>(level)] = hrtest::report_of(hrtest::fixture_world("h12-v3"), level);
}

// The chronicle line of event `id` (the line whose second field is e<id>), or "".
std::string chronicle_line(const std::string& report, std::int64_t id) {
    const std::string tag = " e" + std::to_string(id) + " ";
    for (const std::string& l : hrtest::lines_of(report)) {
        const std::size_t sp = l.find(' ');
        if (sp != std::string::npos && l.compare(sp, tag.size(), tag) == 0) return l;
    }
    return "";
}

// The lines of [present] from the line equal to `head` (inclusive), `n` of them.
std::vector<std::string> from(const std::vector<std::string>& lines, const std::string& head, std::size_t n) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (lines[i] == head) {
            const std::size_t end = std::min(lines.size(), i + n);
            return std::vector<std::string>(lines.begin() + static_cast<std::ptrdiff_t>(i),
                                            lines.begin() + static_cast<std::ptrdiff_t>(end));
        }
    }
    return {};
}

struct Corruption {
    const char* sql;
    const char* message;
};

void check_corruptions(const std::vector<Corruption>& cases) {
    for (const Corruption& c : cases) {
        CAPTURE(std::string(c.sql));
        try {
            const std::string path = hrtest::fixture_copy("h12-v3", "corrupt-v3");
            const worldgen::db::Status s = worldgen::db::testing::execute_on_file(path, c.sql);
            REQUIRE_MESSAGE(s.ok(), "execute_on_file: " << (s.ok() ? "" : s.error().message));
            const worldgen::db::WorldReader r = hrtest::open_world(path);
            for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
                const auto rep = worldgen::inspect::build_history_report(r, level);
                REQUIRE_FALSE(rep.ok());
                CHECK(rep.error().code == worldgen::db::ErrorCode::corrupt_layer);
                CHECK(rep.error().message == c.message);
            }
        } catch (const worldgen::FatalError& e) {
            FAIL_CHECK("FatalError: " << e.what());
        }
    }
}

}  // namespace

TEST_SUITE("history-report") {

TEST_CASE("P3-07 r21 26.1: H12 is v3 at both levels") {
    CHECK(hrtest::lines_of(text(HistoryLevel::summary)).front() == "# worldgen history v4");   // P5-02 12.1
    CHECK(hrtest::lines_of(text(HistoryLevel::full)).front() == "# worldgen history v4");
}

TEST_CASE("P3-07 r21 26.2, 26.8 item 2: renamed regions, a region renamed twice and one once") {
    for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
        CAPTURE(static_cast<int>(level));
        const std::vector<std::string> present = block(text(level), "[present]");
        CHECK(from(present, "renamed regions: 3", 4) ==
              std::vector<std::string>{
                  "renamed regions: 3",
                  "  rg2: Belford's Fall since 800, e9 after e8 sacked; formerly The Holt of Yews",
                  "  rg2: The Red Downs since 1300, e16 after e15 battle; formerly Belford's Fall",
                  "  rg4: The Bloody Wold since 1400, e18 after e17 battle; formerly Cor Wold"});
        // 26.2: appended after `standing claims` (its one claim line), the last lines of [present]
        CHECK(from(present, "standing claims: 1", 2).size() == 2);
        CHECK(present.size() >= 4);
        CHECK(present[present.size() - 4] == "renamed regions: 3");
    }
    // n = 0 prints the count line: H2 has no region_name row
    CHECK(contains_line(block(hrtest::report_of(hrtest::fixture_world("h2-minimal"), HistoryLevel::summary),
                              "[present]"),
                        "renamed regions: 0"));
}

TEST_CASE("P3-07 r21 26.3, 26.8 item 3: hidden refuges, only those hidden at E - 1 listed") {
    for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
        CAPTURE(static_cast<int>(level));
        const std::vector<std::string> present = block(text(level), "[present]");
        CHECK(from(present, "hidden refuges: 1 of 3 founded by flight", 3) ==
              std::vector<std::string>{"hidden refuges: 1 of 3 founded by flight",
                                       "  s4 Hidden Hole in Bel Downs (rg3): The Kingdom of Bel (c2), population 150, "
                                       "founded 801 by flight from Belford (s2), e10",
                                       "open wars: 1"});
        for (const std::string& l : present) {
            CHECK(l.find("Found Hole in") == std::string::npos);
            CHECK(l.find("Plain Hole in") == std::string::npos);
            CHECK(l.rfind("refuges:", 0) != 0);   // 24.7's line is replaced
        }
    }
    const auto st = hrtest::stats_of(text(HistoryLevel::full));
    CHECK(st.at("refuges") == 3);
    CHECK(st.at("refuges_hidden") == 1);
}

TEST_CASE("P3-07 r21 26.4, 26.8 item 4: references by the present name, sentences by the name at the event") {
    const std::string full = text(HistoryLevel::full);
    // sentences: before both renamings, between them, the renamings themselves, after both
    CHECK(chronicle_line(full, 7).find("over the Holt of Yews.") != std::string::npos);
    CHECK(chronicle_line(full, 15).find("in Belford's Fall;") != std::string::npos);
    CHECK(chronicle_line(full, 9).find("After the sack, the Holt of Yews became known as Belford's Fall.") !=
          std::string::npos);
    CHECK(chronicle_line(full, 16).find("After the slaughter, Belford's Fall became known as the Red Downs.") !=
          std::string::npos);
    CHECK(chronicle_line(full, 18).find("After the slaughter, Cor Wold became known as the Bloody Wold.") !=
          std::string::npos);
    CHECK(chronicle_line(full, 19).find("laid claim to the Red Downs, held by") != std::string::npos);
    // references outside sentences: the present name, with its stored capital
    CHECK(contains_line(hrtest::civ_block(full, 2), "  home: The Red Downs (rg2), grassland"));
    const std::vector<std::string> present = block(full, "[present]");
    CHECK(contains_line(present, "  rg2 The Red Downs: The Kingdom of Arn (c1) since 800"));
    CHECK(contains_line(present, "  rg4 The Bloody Wold: The Kingdom of Arn (c1) since 20"));
    CHECK(contains_line(present, "  e7 The Kingdom of Arn (c1) against The Kingdom of Bel (c2) over The Red Downs "
                                 "(rg2), since 500"));
    CHECK(contains_line(present, "  e19 1500 The Kingdom of Bel (c2) on The Kingdom of Arn (c1): The Red Downs (rg2)"));
    for (const std::string& l : present) {
        if (l.rfind("  rg", 0) == 0 && l.find(" since ") != std::string::npos && l.find("formerly") != std::string::npos) {
            continue;   // the renamed-regions lines name the former names on purpose
        }
        CHECK(l.find("The Holt of Yews") == std::string::npos);
        CHECK(l.find("Belford's Fall") == std::string::npos);
        const bool rg4_ok = l.find("(rg4)") == std::string::npos || l.find("The Bloody Wold (rg4)") != std::string::npos;
        CHECK(rg4_ok);
    }
}

TEST_CASE("P3-07 r21 26.5, 26.8 item 5: a renaming and its deed are shown together at the summary") {
    const std::string s = text(HistoryLevel::summary);
    const auto st = hrtest::stats_of(s);
    REQUIRE(st.at("summary_threshold") == 78);   // above the renaming's 60, below the sack's 90
    // the sack (90) is shown and brings the renaming of salience 60 (part 2)
    CHECK_FALSE(chronicle_line(s, 8).empty());
    CHECK_FALSE(chronicle_line(s, 9).empty());
    // the renaming of salience 95 is shown and brings its cause, a battle, which is no candidate (part 4)
    CHECK_FALSE(chronicle_line(s, 16).empty());
    CHECK_FALSE(chronicle_line(s, 15).empty());
    // the renaming e18 (60) is below the threshold: its battle e17 is shown only as the war's last battle (the
    // frame of e7), and companions are not followed further (24.2), so e18 stays out
    CHECK_FALSE(chronicle_line(s, 17).empty());
    CHECK(chronicle_line(s, 18).empty());
}

TEST_CASE("P3-07 r21 26.5 item 2: the four [stats] keys at full, 17 summary keys") {
    const auto full = hrtest::stats_list(text(HistoryLevel::full));
    std::vector<std::string> tail;
    for (std::size_t i = full.size() - 4; i < full.size(); ++i) tail.push_back(full[i].first);
    CHECK(tail == std::vector<std::string>{"region_names", "regions_renamed", "refuges", "refuges_hidden"});
    const auto st = hrtest::stats_of(text(HistoryLevel::full));
    CHECK(st.at("region_names") == 3);
    CHECK(st.at("regions_renamed") == 2);
    const auto summary = hrtest::stats_list(text(HistoryLevel::summary));
    CHECK(summary.size() == 17);
    CHECK(summary[summary.size() - 2].first == "regions_renamed");
    CHECK(summary.back().first == "refuges_hidden");
}

TEST_CASE("P3-07 r21 26.6, 26.8 item 7: each corrupt region_name message") {
    check_corruptions({
        {"UPDATE region_name SET region = 9 WHERE region = 4",
         "corrupt table 'region_name': row (9, 1400): unknown region 9"},
        {"UPDATE region_name SET from_year = 2000 WHERE region = 4",
         "corrupt table 'region_name': row (4, 2000): from_year 2000 outside 0 to 1999"},
        {"UPDATE region_name SET from_year = -1 WHERE region = 4",
         "corrupt table 'region_name': row (4, -1): from_year -1 outside 0 to 1999"},
        {"UPDATE region_name SET name = '' WHERE region = 4",
         "corrupt table 'region_name': row (4, 1400): name is not a name"},
        {"UPDATE region_name SET name = 'The_Bloody Wold' WHERE region = 4",
         "corrupt table 'region_name': row (4, 1400): name is not a name"},
        {"UPDATE region_name SET name = 'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' WHERE region = 4",
         "corrupt table 'region_name': row (4, 1400): name is not a name"},   // 65 bytes
        {"UPDATE region_name SET culture = 7 WHERE region = 4",
         "corrupt table 'region_name': row (4, 1400): unknown culture 7"},
        {"UPDATE region_name SET event = 17 WHERE region = 4",   // another kind
         "corrupt table 'region_name': row (4, 1400): event 17 is not a region_renamed of this region and year"},
        {"UPDATE region_name SET event = 5000 WHERE region = 4",   // no such event
         "corrupt table 'region_name': row (4, 1400): event 5000 is not a region_renamed of this region and year"},
        {"UPDATE region_name SET event = 16 WHERE region = 4",   // another region's renaming
         "corrupt table 'region_name': row (4, 1400): event 16 is not a region_renamed of this region and year"},
        {"UPDATE region_name SET from_year = 1401 WHERE region = 4",   // the event's year differs
         "corrupt table 'region_name': row (4, 1401): event 18 is not a region_renamed of this region and year"},
        {"DELETE FROM region_name WHERE region = 4",
         "corrupt table 'event': row 18: region_renamed without a region_name row"},
        // the first failing row in key order, and its first reason in the listed order
        {"UPDATE region_name SET culture = 7 WHERE region = 4; UPDATE region_name SET name = '' WHERE from_year = 1300",
         "corrupt table 'region_name': row (2, 1300): name is not a name"},
        {"UPDATE region_name SET name = '', culture = 7, event = 17 WHERE region = 4",
         "corrupt table 'region_name': row (4, 1400): name is not a name"},
        // after the relation checks
        {"UPDATE region_name SET region = 9 WHERE region = 4; UPDATE relation SET since_event = 0",
         "corrupt table 'relation': row (3, 1, 3, 2, war, 500): since_event 0 is not an event"},
    });
}

TEST_CASE("P3-07 r21 26.6: a name with a hyphen, and one of 64 bytes, is a name") {
    for (const char* sql : {"UPDATE region_name SET name = 'The Bloody-Wold' WHERE region = 4",
                            "UPDATE region_name SET name = "
                            "'AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA' WHERE region = 4"}) {
        CAPTURE(std::string(sql));
        const std::string path = hrtest::fixture_copy("h12-v3", "hyphen-v3");
        REQUIRE(worldgen::db::testing::execute_on_file(path, sql).ok());
        const worldgen::db::WorldReader r = hrtest::open_world(path);
        const auto rep = worldgen::inspect::build_history_report(r, HistoryLevel::full);
        CHECK(rep.ok());
    }
}

TEST_CASE("P3-07 r21 26.7, 26.8 item 8: settlement_row with ids 0, 1, size and size + 1") {
    const std::vector<std::int64_t> v = {11, 22, 33};
    const std::span<const std::int64_t> s(v);
    CHECK(worldgen::inspect::settlement_row(s, 0) == nullptr);
    CHECK(worldgen::inspect::settlement_row(s, -1) == nullptr);
    REQUIRE(worldgen::inspect::settlement_row(s, 1) != nullptr);
    CHECK(*worldgen::inspect::settlement_row(s, 1) == 11);
    REQUIRE(worldgen::inspect::settlement_row(s, 3) != nullptr);
    CHECK(*worldgen::inspect::settlement_row(s, 3) == 33);
    CHECK(worldgen::inspect::settlement_row(s, 4) == nullptr);
    CHECK(worldgen::inspect::settlement_row(std::span<const std::int64_t>(), 1) == nullptr);
}

// P3-07 r22 26.4 (P5-12 validation finding A; the lead's ruling): a history note names a region as it was at the
// noted event, like the sentence. A copy of H12 gains, after its notes, the claim e1020 (1700, c1 on region 3
// "Bel Downs", held by c2), the declaration e1021 (1750, over region 1, caused by e1020, with its war row) and the
// renaming e1022 (1800) of region 3 to "The Grey Downs" with no cause (r22 26.2: no `after` part). The note of
// e1021 names the claim's region by its name at e1021, "Bel Downs", not by its present name; references outside
// sentences print the present name. The two SHA-256 values are the reference's text of this world
// (p3_report_reference.py: `fixture h12-v3`, the same SQL, `report --level summary|full`), never the C++ output.
TEST_CASE("P3-07 r22 26.4, 26.2: a claim note names a region renamed after the declaration as at the declaration") {
    const std::string path = hrtest::fixture_copy("h12-v3", "claim-note-v3");
    const worldgen::db::Status s = worldgen::db::testing::execute_on_file(
        path,
        "INSERT INTO event VALUES (1020, 1700, 1700, 10, 2, 3, 0, 35);"
        "INSERT INTO event_participant VALUES (1020, 1, 1, 3, 1), (1020, 2, 2, 3, 2);"
        "INSERT INTO event VALUES (1021, 1750, 1750, 4, 2, 1, 0, 70);"
        "INSERT INTO event_participant VALUES (1021, 1, 1, 3, 1), (1021, 2, 2, 3, 2);"
        "INSERT INTO event_cause VALUES (1021, 1020);"
        "INSERT INTO relation VALUES (3, 1, 3, 2, 'war', 1750, NULL, 1021, 0);"
        "INSERT INTO event VALUES (1022, 1800, 1800, 9, 2, 3, 0, 60);"
        "INSERT INTO region_name VALUES (3, 1800, 'The Grey Downs', 1, 1022);");
    REQUIRE_MESSAGE(s.ok(), "execute_on_file: " << (s.ok() ? "" : s.error().message));
    const std::string note = "(The claim, since 1700, was to Bel Downs. The second war between them.)";
    const std::string full = hrtest::report_of(path, HistoryLevel::full);
    const std::string summary = hrtest::report_of(path, HistoryLevel::summary);
    CHECK(chronicle_line(full, 1021) ==
          "1750 e1021 war_declared/70: The Kingdom of Arn declared war on the Kingdom of Bel over Arn Vale. " + note +
              " {@rg1 attacker=c1 defender=c2} [causes e1020]");
    CHECK(chronicle_line(full, 1020).find("laid claim to Bel Downs, held by") != std::string::npos);
    CHECK(chronicle_line(full, 1022).find("Bel Downs became known as the Grey Downs.") != std::string::npos);
    CHECK(summary.find(note) != std::string::npos);
    for (const std::string* t : {&full, &summary}) {
        CHECK(t->find("was to the Grey Downs") == std::string::npos);
        CHECK(t->find("was to The Grey Downs") == std::string::npos);
        const std::vector<std::string> present = block(*t, "[present]");
        CHECK(contains_line(present, "  rg3: The Grey Downs since 1800, e1022; formerly Bel Downs"));
    }
    CHECK(contains_line(block(full, "[present]"), "  rg3 The Grey Downs: The Kingdom of Bel (c2) since 801"));
    CHECK(hrtest::sha_hex(summary) == "9bcb5bb3ce21ab3f79c2c4eda9fda350092087aab45998d6100d5588cb7baea6");
    CHECK(hrtest::sha_hex(full) == "91d23b8bcfb3e3940d89115613881398be54e712c427125972133f2ae0c6ee33");
}

}  // TEST_SUITE
