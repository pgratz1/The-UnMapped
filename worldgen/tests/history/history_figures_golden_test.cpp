// Spec P5-06 (docs/specs/p5-06-figures.md r3) 17: generated worlds against worldgen/tests/golden/p5-06-figures.txt,
// written by reference/p5_06_figures_reference.py golden from the reference chain's history tables. Per case: the
// figure and book rows, the ruled, served, rival and parent rows of table relation, the event rows of kinds 79 to 92
// with their participants, causes and values, and the kind 4 site rows, each as a P0-04 8 table hash (the counts and
// [stats] lines are the reference's own comparison). The 512 cases run by default; the 1024 cases in the suite
// "figures-slow". The golden file is recorded after calibration: until then (and until integration) every case
// returns at its start.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_06_figures_api.hpp"

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

namespace db = worldgen::db;
using Rows = std::vector<std::vector<db::ColumnValue>>;
using Golden = std::map<std::string, std::map<std::string, std::pair<std::string, std::string>>>;

std::filesystem::path golden_path() {
    return std::filesystem::path(UNMAPPED_TEST_SOURCE_ROOT) / "worldgen" / "tests" / "golden" / "p5-06-figures.txt";
}

const Golden& golden() {
    static const Golden g = [] {
        Golden out;
        std::ifstream in(golden_path(), std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            std::string name, what, a, b, c;
            ls >> name >> what;
            if (what == "rows" || what == "sites") {
                ls >> a >> b >> c;   // <kind> <n> <hash>
                out[name][what + " " + a] = {b, c};
            } else if (what == "figure" || what == "book" || what == "events") {
                ls >> b >> c;
                out[name][what] = {b, c};
            } else if (what == "participants" || what == "causes" || what == "values") {
                ls >> c;
                out[name][what] = {"", c};
            }
        }
        return out;
    }();
    return g;
}

bool ready() {
    if (!p506api::INTEGRATED) {
        MESSAGE("P5-06 not integrated: skipped");
        return false;
    }
    if (!std::filesystem::exists(golden_path())) {
        MESSAGE("p5-06-figures.txt absent (recorded after calibration): skipped");
        return false;
    }
    return true;
}

void check_case(const std::string& name, const std::string& path) {
    CAPTURE(name);
    const auto it = golden().find(name);
    REQUIRE_MESSAGE(it != golden().end(), "no golden lines for " << name);
    const auto& g = it->second;
    const db::WorldReader r = chrono_test::open_world(path);
    std::map<std::int64_t, std::string> kind;
    for (const auto& k : chrono_test::rows_of(r, "event_kind")) kind[k.at(0).integer] = k.at(1).bytes;
    const auto check = [&](const std::string& what, const Rows& rows, std::size_t columns, bool counted) {
        CAPTURE(what);
        const auto gi = g.find(what);
        REQUIRE_MESSAGE(gi != g.end(), "no golden line " << what);
        if (counted) CHECK(std::to_string(rows.size()) == gi->second.first);
        CHECK(engine_test::table_hash(columns, rows) == gi->second.second);
    };
    check("figure", chrono_test::rows_of(r, "figure"), 19, true);
    check("book", chrono_test::rows_of(r, "book"), 6, true);
    std::map<std::string, Rows> rel;
    for (const auto& x : chrono_test::rows_of(r, "relation")) rel[x.at(4).bytes].push_back(x);
    for (const char* k : {"ruled", "served", "rival", "parent"}) check(std::string("rows ") + k, rel[k], 9, true);
    Rows ev;
    std::set<std::int64_t> ids;
    for (const auto& e : chrono_test::rows_of(r, "event")) {
        if (e.at(3).integer >= 79 && e.at(3).integer <= 92) {
            ev.push_back(e);
            ids.insert(e.at(0).integer);
        }
    }
    check("events", ev, 8, true);
    const auto of_ids = [&](const char* table) {
        Rows out;
        for (const auto& x : chrono_test::rows_of(r, table)) {
            if (ids.count(x.at(0).integer) != 0U) out.push_back(x);
        }
        return out;
    };
    check("participants", of_ids("event_participant"), 5, false);
    check("causes", of_ids("event_cause"), 2, false);
    check("values", of_ids("event_value"), 3, false);
    Rows towers;
    for (const auto& s : chrono_test::rows_of(r, "site")) {
        if (s.at(2).integer == 4) towers.push_back(s);
    }
    check("sites tower", towers, 14, true);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-06 17: geo-seed-0-512, geo-seed-42-512 and hist-span-2000-512 against p5-06-figures.txt") {
    if (!ready()) return;
    check_case("geo-seed-0-512", p503run::world_path(0, 512));
    check_case("geo-seed-42-512", p503run::world_path(42, 512));
    check_case("hist-span-2000-512", p503run::world_path(7, 512, "normal", 2000));
}

}  // TEST_SUITE

TEST_SUITE("figures-slow" * doctest::skip()) {

TEST_CASE("P5-06 17: the 1024 world cases against the golden") {
    if (!ready()) return;
    check_case("geo-seed-1-1024", p503run::world_path(1, 1024));
    check_case("seed-0-1024", p503run::world_path(0, 1024));
    check_case("sample-42-1024", p503run::world_path(42, 1024));
    check_case("hist-races-elf-orc-1024", p503run::world_path(3, 1024, "normal", 6000, "elf,orc"));
}

}  // TEST_SUITE
