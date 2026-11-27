// Golden vectors of spec P3-03 (14.4 item 22) and 14.3 item 21: every line
// of worldgen/tests/golden/p3-03-chronology.txt, written by the test
// writer's independent reference
// worldgen/tests/reference/p3_chronology_reference.py (never regenerated
// from this C++). Seam cases through chronology_stage (years, the C3 trace
// recomputed here with the P3-02 draw, ages, gods with their attempts
// replayed with choose_name, the five table hashes, the eight meta values,
// the [chronology] keys); the schedule lines through awakening_years; the
// tuned lines through awakening_years with hand-made tuning; world cases
// through generate_world with the shipped data (hash --detail's table
// lines, meta, the input region table), the 512 cases here and the 1024
// cases in the suite "chronology-slow". Where the worked examples of 7.1
// sit in the file is stated in its header.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "p3_03_chronology_support.hpp"

namespace {

using namespace chrono_test;
using chrono_api::Params;

std::vector<std::string> regions_of_case(const GoldenCase& c) {
    if (c.regions == "collision") {
        return collision_regions();
    }
    REQUIRE(c.regions == "none");
    return {};
}

std::string years_text(const Rows& r) {
    std::string s;
    for (const AgeRow& a : r.ages) {
        s += (s.empty() ? "" : " ") + std::to_string(a.start_year);
    }
    return s;
}

// The items every case shares: years, ages, gods, tables, meta, report.
void check_rows(const GoldenCase& c, const Rows& rows, const Params& p, const std::vector<std::string>& regions) {
    CHECK(years_text(rows) == item(c, "years"));
    CHECK(ages_text(rows.ages) == item(c, "ages"));
    // The gods' names in the rows, and their attempts replayed through the
    // registry of 8.1 with the P3-02 library (choose_name).
    std::string names;
    for (const GodRow& g : rows.gods) {
        names += (names.empty() ? "" : "|") + std::to_string(g.id) + ":" + g.name;
    }
    const std::string want = item(c, "gods");
    std::string want_names;
    std::size_t from = 0;
    while (from <= want.size()) {
        const std::size_t bar = std::min(want.find('|', from), want.size());
        const std::string entry = want.substr(from, bar - from);
        want_names += (want_names.empty() ? "" : "|") + entry.substr(0, entry.rfind(':'));
        from = bar + 1;
    }
    CHECK(names == want_names);
    // P5-08 (0.30.0): the creators' attempts replayed by the test; the nine pantheon gods follow them in the record.
    const std::string creators = replay_gods(rows, c.seed, regions, shipped());
    CHECK(want.substr(0, creators.size()) == creators);
    // The stage's own record of each choice (ChronologyResult::god_names).
    CHECK(chrono_api::god_choices(shipped(), p, c.seed, regions) == want);
    const std::map<std::string, std::string> h = table_hashes(rows);
    for (const std::string& t : five_tables()) {
        CAPTURE(t);
        CHECK(h.at(t) == item(c, "table " + t));
    }
    for (const auto& [k, v] : chrono_api::meta_entries(p)) {
        CAPTURE(k);
        CHECK(v == item(c, "meta " + k));
    }
    CHECK(report_lines(rows) == c.report);
}

void check_world_case(const GoldenCase& c) {
    CAPTURE(c.name);
    const std::string path = world_of(c);
    const worldgen::db::WorldReader r = open_world(path);
    const std::map<std::string, std::string> detail = detail_tables(r);
    CHECK(detail.at("region") == item(c, "input_region"));
    const std::vector<std::string> regions = region_names_of(r);
    CHECK(std::to_string(regions.size()) == item(c, "regions"));
    for (const std::string& t : five_tables()) {
        CAPTURE(t);
        CHECK(detail.at(t) == item(c, "table " + t));
    }
    for (const std::string& k : eight_keys()) {
        CAPTURE(k);
        CHECK(meta_or(r, k) == item(c, "meta " + k));
    }
    const Params p = params_with(c.span, races_of_text(c.races));
    const Rows rows = rows_of_world(r);
    check_rows(c, rows, p, regions);
    CHECK(trace_text(own_shipped_schedule(c.seed, c.span, p.races)) == item(c, "trace"));
    // 14.3 item 21: on shipped data the rows do not depend on the region names.
    CHECK(chrono_api::stage(shipped(), p, c.seed, {}) == rows);
    CHECK(chrono_api::stage(shipped(), p, c.seed, regions) == rows);
    CHECK(meta_or(r, "stages") == "elevation,hydrology,climate,biomes,resources,regions,chronology,history");  // P3-04 9.5
}

}  // namespace

TEST_SUITE("chronology") {

TEST_CASE("P3-03 14.4 item 22: the golden file holds every case the spec lists") {
    const Golden& g = golden();
    std::size_t seam = 0;
    std::size_t worlds = 0;
    for (const GoldenCase& c : g.cases) {
        (c.kind == "seam" ? seam : worlds) += 1;
    }
    CHECK(seam == 5U * 3U * 4U + 1U);  // seeds x spans x race lists, plus the collision case
    CHECK(worlds == 6U);
    for (const char* name : {"geo-seed-0-512", "geo-seed-42-512", "geo-phrase-512", "geo-seed-1-1024",
                             "hist-span-2000-512", "hist-races-elf-orc-1024", "collision-42-6000-all",
                             "seam-42-6000-all", "seam-18446744073709551615-12000-orc"}) {
        CAPTURE(std::string_view(name));
        CHECK(g.find(name).name == name);
    }
    CHECK(g.sched.size() == 34U * 5U * 10U);
    CHECK(!g.tuned.empty());
    // 7.1's seed 42 example, as the file states it.
    const GoldenCase& c = g.find("seam-42-6000-all");
    CHECK(item(c, "years") == "0 2074 3242 4243 4969 5596");
    // r3 (0.17.0): P3-02 11.6's seed-42 gods (7.1 lists r4's: Krok, Kruu); unchanged by the short word list
    // of 0.18.0 ("mold" stays on it).
    CHECK(item(c, "gods") == "1:Fielie:0|2:Krokzit:0|3:Bronhak:1|4:Syfiash:0|5:Wealstuld:0|6:Ogh'krak:1|7:Muug'murk:0|8:Drulihai:0|9:Bagit:0|"
                        "10:Ouwi:0|11:Naecend:0|12:Maeglith:0|13:Sneegib:2|14:Edearbuck:1|15:Ashedsock:0");  // 0.30.0: the nine of the pantheon follow the six creators (golden, from the reference)
}

TEST_CASE("P3-03 14.4 item 22: seam cases through chronology_stage") {
    for (const GoldenCase& c : golden().cases) {
        if (c.kind != "seam") {
            continue;
        }
        CAPTURE(c.name);
        const Params p = params_with(c.span, races_of_text(c.races));
        const std::vector<std::string> regions = regions_of_case(c);
        const Rows rows = chrono_api::stage(shipped(), p, c.seed, regions);
        check_rows(c, rows, p, regions);
        CHECK(trace_text(own_shipped_schedule(c.seed, c.span, p.races)) == item(c, "trace"));
        // The same rows from both fixture data roots.
        CHECK(chrono_api::stage(fixture_geo(), p, c.seed, regions) == rows);
    }
}

TEST_CASE("P3-03 14.4 item 22: sched lines through awakening_years") {
    const Tuning t = chrono_api::tuning_from(shipped());
    for (const GoldenSched& s : golden().sched) {
        CAPTURE(s.seed);
        CAPTURE(s.span);
        CAPTURE(s.races);
        const auto [ids, shares] = present_of(races_of_text(s.races));
        CHECK(chrono_api::awakening_years(s.seed, s.span, ids, shares, t) == s.years);
    }
}

TEST_CASE("P3-03 14.4 item 22: tuned lines through awakening_years and awakening_bounds") {
    for (const GoldenTuned& x : golden().tuned) {
        CAPTURE(x.name);
        const Tuning t{x.jitter_raw, x.gap, "The Age of {plural}"};
        CHECK(chrono_api::bounds(x.span, static_cast<std::int64_t>(x.ids.size()), t) == chrono_api::Bounds{x.g, x.jmax});
        CHECK(chrono_api::awakening_years(x.seed, x.span, x.ids, x.shares, t) == x.years);
        CHECK(years_of(own_schedule(x.seed, x.span, x.ids, x.shares, x.jitter_raw, x.gap)) == x.years);
    }
}

TEST_CASE("P3-03 14.4 item 22, 14.3 item 21: world cases at 512 through generate_world") {
    for (const GoldenCase& c : golden().cases) {
        if (c.kind == "world" && c.map_size == 512) {
            check_world_case(c);
        }
    }
}

}  // TEST_SUITE

TEST_SUITE("chronology-slow" * doctest::skip()) {

TEST_CASE("P3-03 14.4 item 22, 14.3 item 21: world cases at 1024 through generate_world") {
    for (const GoldenCase& c : golden().cases) {
        if (c.kind == "world" && c.map_size == 1024) {
            check_world_case(c);
        }
    }
}

}  // TEST_SUITE
