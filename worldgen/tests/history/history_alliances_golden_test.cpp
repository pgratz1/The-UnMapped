// Spec P5-04 (docs/specs/p5-04-alliances-trade.md r3) 17: generated worlds against
// worldgen/tests/golden/p5-04-alliances.txt, written by reference/p5_04_alliances_reference.py golden from the
// reference chain's history tables (p3_report_reference.py build-inputs) of the report's world cases. Per case: the
// alliance, trade and joined war rows of table relation, the event rows of kinds 63 to 78 with their participants,
// causes and values, and the kind 3 site rows, each as a P0-04 8 table hash. The 512 cases run by default; the 1024
// cases whose settings world_path makes are in the suite "alliances-slow". (geo-phrase-512, hist-dense-1024 and the
// civs4 cases are compared at integration by the references' check-world, not here.)

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

#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

namespace db = worldgen::db;
using Rows = std::vector<std::vector<db::ColumnValue>>;

// "<case> <what...> <hash>" lines: case -> what -> (count or "", hash)
using Golden = std::map<std::string, std::map<std::string, std::pair<std::string, std::string>>>;

const Golden& golden() {
    static const Golden g = [] {
        Golden out;
        const std::filesystem::path p =
            std::filesystem::path(UNMAPPED_TEST_SOURCE_ROOT) / "worldgen" / "tests" / "golden" / "p5-04-alliances.txt";
        std::ifstream in(p, std::ios::binary);
        REQUIRE_MESSAGE(in.good(), "cannot read " << p.string());
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            std::string name, what, a, b, c;
            ls >> name >> what;
            if (what == "rows" || what == "sites") {
                ls >> a >> b >> c;   // <kind> <n> <hash>
                out[name][what + " " + a] = {b, c};
            } else if (what == "events") {
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

std::string n_of(const Rows& r) { return std::to_string(r.size()); }

void check_case(const std::string& name, const std::string& path) {
    CAPTURE(name);
    const auto it = golden().find(name);
    REQUIRE_MESSAGE(it != golden().end(), "no golden lines for " << name);
    const auto& g = it->second;
    const db::WorldReader r = chrono_test::open_world(path);
    // kind ids by key
    std::map<std::int64_t, std::string> kind;
    for (const auto& k : chrono_test::rows_of(r, "event_kind")) kind[k.at(0).integer] = k.at(1).bytes;
    static const std::set<std::string> news = {
        "alliance_formed", "alliance_renewed", "alliance_lapsed", "alliance_broken", "alliance_betrayed",
        "ally_joined_war", "ally_refused", "ally_withdrew", "allied_battle", "trade_opened", "trade_broken",
        "spoils_shared", "spoils_withheld", "alliance_spurned", "mine_opened", "arms_sold"};
    const Rows events = chrono_test::rows_of(r, "event");
    std::map<std::int64_t, std::string> event_kind;
    for (const auto& e : events) event_kind[e.at(0).integer] = kind[e.at(3).integer];
    Rows alliance, trade, joined;
    for (const auto& x : chrono_test::rows_of(r, "relation")) {
        const std::string& k = x.at(4).bytes;
        if (k == "alliance") alliance.push_back(x);
        if (k == "trade") trade.push_back(x);
        if (k == "war" && event_kind[x.at(7).integer] == "ally_joined_war") joined.push_back(x);
    }
    const auto check = [&](const std::string& what, const Rows& rows, std::size_t columns, bool counted) {
        CAPTURE(what);
        const auto gi = g.find(what);
        REQUIRE_MESSAGE(gi != g.end(), "no golden line " << what);
        if (counted) CHECK(n_of(rows) == gi->second.first);
        CHECK(engine_test::table_hash(columns, rows) == gi->second.second);
    };
    check("rows alliance", alliance, 9, true);
    check("rows trade", trade, 9, true);
    check("rows war_joined", joined, 9, true);
    Rows ev;
    std::set<std::int64_t> ids;
    for (const auto& e : events) {
        if (news.count(kind[e.at(3).integer]) != 0U) {
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
    Rows mines;
    for (const auto& s : chrono_test::rows_of(r, "site")) {
        if (s.at(2).integer == 3) mines.push_back(s);
    }
    check("sites mine", mines, 14, true);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04 17: geo-seed-0-512, geo-seed-42-512 and hist-span-2000-512 against p5-04-alliances.txt") {
    check_case("geo-seed-0-512", p503run::world_path(0, 512));
    check_case("geo-seed-42-512", p503run::world_path(42, 512));
    check_case("hist-span-2000-512", p503run::world_path(7, 512, "normal", 2000));
}

}  // TEST_SUITE

TEST_SUITE("alliances-slow" * doctest::skip()) {

TEST_CASE("P5-04 17: geo-seed-1-1024, seed-0-1024, sample-42-1024 and hist-races-elf-orc-1024 against the golden") {
    check_case("geo-seed-1-1024", p503run::world_path(1, 1024));
    check_case("seed-0-1024", p503run::world_path(0, 1024));
    check_case("sample-42-1024", p503run::world_path(42, 1024));
    check_case("hist-races-elf-orc-1024", p503run::world_path(3, 1024, "normal", 6000, "elf,orc"));
}

}  // TEST_SUITE
