// Spec P5-12 7.3 and 13 items 5 and 7: the readers of table region_name on the generated world geo-seed-42-512
// (shipped data; its rows are those of worldgen/tests/golden/p5-12-renaming.txt):
//   - P4-01 r3 16: Timeline::region_name(region, year) is region.name before a row's from_year and the row's
//     name from it (half-open years), and at E the present name; it changes only at a row's from_year;
//   - P3-04 r21 23.1: HistoryView::region_name_at_year and region_name_at_event (N(r, e)): the renaming event
//     itself and every later event carry the new name, the event before it the old one;
//   - P3-07 r21 26.1, 26.2, 26.3, 26.4: report --history v3 at both levels: the header, the renamed regions of
//     [present] (one line per row with its cause and former name), the hidden refuges' count line, the [present]
//     region lines (full) and the four [stats] keys of 26.5;
//   - P3-04 r21 23.4: [history] region_names of the world report and hash --detail's table line.
// GUESSES (the spec names them, not their exact signatures): HistoryView::region_name_at_event(r, e) and
// region_name_at_year(r, y) return a std::string_view; Timeline::region_name is as P4-01 r3 16 item 2 gives it.
// Written from the specs before the implementation (test-writer rule).

#include "p5_12_renaming_support.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../history_report_checks.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/report.hpp"

namespace {

using namespace renaming_test;
namespace wh = worldgen::history;
using worldgen::inspect::HistoryLevel;

std::string geo42() {
    const CCase& c = conflict_case("geo-seed-42-512");
    return chrono_test::world(chrono_test::WorldKey{c.seed, "", c.size, c.span, c.races, "", 1});
}

std::string mid(const std::string& n) { return n.rfind("The ", 0) == 0 ? "the " + n.substr(4) : n; }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P4-01 r3 16, P3-04 r21 23.1: the name of a region by year and by event on geo-seed-42-512") {
    const RCase& g = renaming_case("geo-seed-42-512");
    const db::WorldReader r = open_world(geo42());
    auto loaded = wh::load_history(r);
    REQUIRE(loaded.ok());
    const wh::LoadedHistory& h = loaded.value();
    std::map<std::int64_t, std::string> geo = geo_names_of(r);
    const std::int64_t E = std::stoll(r.meta("history_end_year").value());
    std::map<std::int64_t, std::string> current = geo;
    for (const RenameRow& w : g.renames) {
        CAPTURE(w.name);
        const std::string before = current.at(w.region);
        CHECK(std::string(h.timeline.region_name(w.region, w.from_year - 1)) == before);
        CHECK(std::string(h.timeline.region_name(w.region, w.from_year)) == w.name);
        CHECK(std::string(h.view.region_name_at_year(w.region, w.from_year - 1)) == before);
        CHECK(std::string(h.view.region_name_at_year(w.region, w.from_year)) == w.name);
        CHECK(std::string(h.view.region_name_at_event(w.region, w.event)) == w.name);
        CHECK(std::string(h.view.region_name_at_event(w.region, w.event - 1)) == before);
        current[w.region] = w.name;
    }
    for (const auto& [rid, name] : current) {
        CAPTURE(rid);
        CHECK(std::string(h.timeline.region_name(rid, E)) == name);   // the present name (invariant 3 of r3 16)
        CHECK(std::string(h.timeline.region_name(rid, 0)) == geo.at(rid));
    }
}

TEST_CASE("P3-07 r21 26.1 to 26.5: report --history v3 on geo-seed-42-512") {
    const RCase& g = renaming_case("geo-seed-42-512");
    const db::WorldReader r = open_world(geo42());
    const Tables t = tables_of_world(r);
    std::map<std::int64_t, std::string> geo = geo_names_of(r);
    std::map<std::int64_t, std::string> key_of;
    for (const Row& k : t.event_kind) key_of[at(k, 0)] = k[1].bytes;
    std::map<std::int64_t, std::int64_t> kind_of;
    for (const Row& e : t.event) kind_of[at(e, 0)] = at(e, 3);
    for (const HistoryLevel level : {HistoryLevel::summary, HistoryLevel::full}) {
        const std::string text = hrtest::report_of(r, level);
        const std::vector<std::string> lines = hrtest::lines_of(text);
        REQUIRE_FALSE(lines.empty());
        CHECK(lines[0] == "# worldgen history v3");
        // 26.2: renamed regions after standing claims
        std::size_t at_line = lines.size();
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].rfind("renamed regions: ", 0) == 0) at_line = i;
        }
        REQUIRE(at_line < lines.size());
        CHECK(lines[at_line] == "renamed regions: " + std::to_string(g.rows));
        std::map<std::int64_t, std::string> former = geo;
        for (std::size_t i = 0; i < g.renames.size(); ++i) {
            const RenameRow& w = g.renames[i];
            REQUIRE(at_line + 1 + i < lines.size());
            CHECK(lines[at_line + 1 + i] ==
                  "  rg" + std::to_string(w.region) + ": " + w.name + " since " + std::to_string(w.from_year) + ", e" +
                      std::to_string(w.event) + " after e" + std::to_string(w.cause) + " " +
                      key_of[kind_of[w.cause]] + "; formerly " + former[w.region]);
            former[w.region] = w.name;
        }
        bool standing_before = false;
        for (std::size_t i = 0; i < at_line; ++i) {
            standing_before = standing_before || lines[i].rfind("standing claims: ", 0) == 0;
        }
        CHECK(standing_before);
        // 26.3: the hidden refuges' line replaces refuges:
        bool hidden_line = false, old_line = false;
        for (const std::string& l : lines) {
            hidden_line = hidden_line || (l.rfind("hidden refuges: ", 0) == 0 && l.find(" founded by flight") != std::string::npos);
            old_line = old_line || l.rfind("refuges: ", 0) == 0;
        }
        CHECK(hidden_line);
        CHECK_FALSE(old_line);
        // 26.5 item 2: the summary's [stats] has 17 keys, among them regions_renamed and refuges_hidden
        const std::vector<std::string> stats = hrtest::block(text, "[stats]");
        std::map<std::string, std::string> kv;
        for (const std::string& s : stats) {
            const std::size_t eq = s.find(" = ");
            if (eq != std::string::npos) kv[s.substr(0, eq)] = s.substr(eq + 3);
        }
        std::set<std::int64_t> renamed;
        for (const RenameRow& w : g.renames) renamed.insert(w.region);
        CHECK(kv.at("regions_renamed") == std::to_string(renamed.size()));
        CHECK(kv.count("refuges_hidden") == 1U);
        if (level == HistoryLevel::summary) {
            CHECK(stats.size() == 17U);
        } else {
            CHECK(kv.at("region_names") == std::to_string(g.rows));
            CHECK(kv.count("refuges") == 1U);
            // 26.4: the [present] region lines print the present name
            for (const auto& [rid, name] : former) {
                bool found = false;
                for (const std::string& l : lines) {
                    found = found || l.rfind("  rg" + std::to_string(rid) + " " + name + ": ", 0) == 0;
                }
                CAPTURE(rid);
                CHECK(found);
            }
        }
    }
}

TEST_CASE("P3-04 r21 23.4: the world report's [history] region_names and hash --detail's table line") {
    const RCase& g = renaming_case("geo-seed-42-512");
    const db::WorldReader r = open_world(geo42());
    const auto rep = worldgen::inspect::build_report(r);
    REQUIRE(rep.ok());
    CHECK(rep.value().find("\nregion_names = " + std::to_string(g.rows) + "\n") != std::string::npos);
    CHECK(rep.value().find("\nregion_name = " + std::to_string(g.rows) + "\n") != std::string::npos);   // [tables]
    const std::map<std::string, std::string> detail = chrono_test::detail_tables(r);
    REQUIRE(detail.count("region_name") == 1U);
    CHECK(detail.at("region_name") == g.hash);
    // the sentence of every renaming: "... <former> became known as <new>." with the name at the event
    const std::vector<std::string> sent = eng::render_all(r, static_cast<std::int64_t>(rows_of(r, "event").size()));
    std::map<std::int64_t, std::string> former = geo_names_of(r);
    for (const RenameRow& w : g.renames) {
        REQUIRE(static_cast<std::size_t>(w.event) <= sent.size());
        const std::string& s = sent[static_cast<std::size_t>(w.event - 1)];
        CAPTURE(s);
        const std::string tail = " became known as " + mid(w.name) + ".";
        CHECK(s.size() > tail.size());
        CHECK(s.compare(s.size() - tail.size(), tail.size(), tail) == 0);
        CHECK((s.find(mid(former[w.region]) + " became") != std::string::npos ||
               s.find(former[w.region] + " became") != std::string::npos));
        former[w.region] = w.name;
    }
}

}  // TEST_SUITE
