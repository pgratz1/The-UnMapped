// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) test 24.1 item 10: the reader's checks of 16.4, each on a
// corrupt copy of the generated 512 world of seed 42 (rows changed by SQL), refused by HistoryView::load and
// read_history_tables alike with `corrupt table '<table>': row <key>: <reason>` (code corrupt_layer); composite
// keys print as `(<a>, <b>, <c>)` (P3-07 26.6, as region_name's). Also schema 10's refusal of a schema 9 file
// (16.1) and the two new tables in hash --detail (16.3).
//
// "not above the previous row" cannot be made by SQL (WITHOUT ROWID tables are read in key order) and is left to
// the engineer's unit test. Written from the spec before the implementation.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/history/digest.hpp"
#include "worldgen/inspect/hashes.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

namespace h = worldgen::history;
std::string n(std::int64_t v) { return std::to_string(v); }
std::string key3(std::int64_t a, std::int64_t b, std::int64_t c) { return "(" + n(a) + ", " + n(b) + ", " + n(c) + ")"; }

const std::string& base_path() {
    static const std::string p = p503run::world_path(42, 512);
    return p;
}

struct Base {
    std::int64_t E = 0;
    std::vector<std::vector<std::int64_t>> rr, sr;   // the rows, key order
    std::int64_t met_event = 0, met_a = 0, met_b = 0;
    std::int64_t mixed_event = 0;   // the first settlement_mixed
    std::map<std::int64_t, std::int64_t> founded;
};

const Base& base() {
    static const Base b = [] {
        Base out;
        const db::WorldReader r = chrono_test::open_world(base_path());
        out.E = std::stoll(r.meta("history_end_year").value_or("0"));
        for (const auto& x : chrono_test::rows_of(r, "race_relation")) {
            out.rr.push_back({x[0].integer, x[1].integer, x[2].integer, x[3].integer, x[4].integer});
        }
        for (const auto& x : chrono_test::rows_of(r, "settlement_race")) {
            out.sr.push_back({x[0].integer, x[1].integer, x[2].integer, x[3].integer, x[4].integer});
        }
        std::int64_t k57 = 0, k60 = 0;
        for (const auto& k : chrono_test::rows_of(r, "event_kind")) {
            if (k[1].bytes == "peoples_met") k57 = k[0].integer;
            if (k[1].bytes == "settlement_mixed") k60 = k[0].integer;
        }
        for (const auto& e : chrono_test::rows_of(r, "event")) {
            if (e[3].integer == k57 && out.met_event == 0) out.met_event = e[0].integer;
            if (e[3].integer == k60 && out.mixed_event == 0) out.mixed_event = e[0].integer;
        }
        for (const auto& p : chrono_test::rows_of(r, "event_participant")) {
            if (p[0].integer != out.met_event) continue;
            if (p[1].integer == 1) out.met_a = p[4].integer;
            if (p[1].integer == 2) out.met_b = p[4].integer;
        }
        for (const auto& s : chrono_test::rows_of(r, "settlement")) out.founded[s[0].integer] = s[6].integer;
        REQUIRE_MESSAGE(!out.rr.empty(), "the world has no race_relation row");
        REQUIRE_MESSAGE(!out.sr.empty(), "the world has no settlement_race row");
        REQUIRE(out.met_event != 0);
        return out;
    }();
    return b;
}

std::string corrupt(const std::string& name, const std::string& sql) {
    static dbtest::TempDir dir("relations-corrupt");
    const std::string path = dir.file(name + ".db");
    dbtest::write_file(path, dbtest::read_file(base_path()));
    REQUIRE_MESSAGE(worldgen::db::testing::execute_on_file(path, sql).ok(), "SQL rejected: " << sql);
    return path;
}

void refused(const std::string& name, const std::string& sql, const std::string& message, bool prefix = false) {
    CAPTURE(name);
    const db::WorldReader r = chrono_test::open_world(corrupt(name, sql));
    const auto e = eng::view_error(r);
    REQUIRE_MESSAGE(e.has_value(), "HistoryView::load accepted " << name);
    CHECK(e->code == db::ErrorCode::corrupt_layer);
    if (prefix) {
        CHECK(e->message.rfind(message, 0) == 0U);
    } else {
        CHECK(e->message == message);
    }
    const auto t = h::read_history_tables(r);
    REQUIRE_FALSE(t.ok());
    CHECK(t.code() == db::ErrorCode::corrupt_layer);
    CHECK(t.error().message == e->message);
}

// P5-03a 10.4: the value checks "in the existing form" `value <k> <v> outside <a> to <b>` (reading D18: the table
// and row prefix and the upper bound of `years` are not given, so the message is matched as a part).
void refused_with(const std::string& name, const std::string& sql, const std::string& part) {
    CAPTURE(name);
    const db::WorldReader r = chrono_test::open_world(corrupt(name, sql));
    const auto e = eng::view_error(r);
    REQUIRE_MESSAGE(e.has_value(), "HistoryView::load accepted " << name);
    CHECK(e->code == db::ErrorCode::corrupt_layer);
    CHECK_MESSAGE(e->message.find(part) != std::string::npos, e->message);
    const auto t = h::read_history_tables(r);
    REQUIRE_FALSE(t.ok());
    CHECK(t.error().message == e->message);
}

// The event_value row of value `key` of the first event of kind `kind`, as SQL selecting it.
std::string value_of(const std::string& kind, int seq) {
    return "event_value WHERE event = (SELECT min(id) FROM event WHERE kind = (SELECT id FROM event_kind WHERE key = '" +
           kind + "')) AND seq = " + std::to_string(seq);
}

std::string where_rr(const std::vector<std::int64_t>& x) {
    return " WHERE race_a = " + n(x[0]) + " AND race_b = " + n(x[1]) + " AND year = " + n(x[2]);
}
std::string where_sr(const std::vector<std::int64_t>& x) {
    return " WHERE settlement = " + n(x[0]) + " AND race = " + n(x[1]) + " AND year = " + n(x[2]);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 16.4: the generated world loads; each damaged race_relation row is refused with its message") {
    REQUIRE_FALSE(eng::view_error(chrono_test::open_world(base_path())).has_value());
    const Base& b = base();
    const std::string head = "corrupt table 'race_relation': row ";
    const std::vector<std::int64_t>& f = b.rr.front();
    refused("rr-unknown-race", "INSERT INTO race_relation VALUES (0, 1, 0, 0, 1);",
            head + key3(0, 1, 0) + ": unknown race 0");
    refused("rr-order", "INSERT INTO race_relation VALUES (1, 1, 0, 0, 1);",
            head + key3(1, 1, 0) + ": race_a 1 is not below race_b 1");
    refused("rr-year", "INSERT INTO race_relation VALUES (" + n(f[0]) + ", " + n(f[1]) + ", " + n(b.E) + ", " +
                           n(f[3] + 1) + ", " + n(f[4]) + ");",
            head + key3(f[0], f[1], b.E) + ": year " + n(b.E) + " outside 0 to " + n(b.E - 1));
    refused("rr-disposition", "UPDATE race_relation SET disposition = 4001" + where_rr(f) + ";",
            head + key3(f[0], f[1], f[2]) + ": disposition 4001 outside -4000 to 4000");
    refused("rr-disposition-low", "UPDATE race_relation SET disposition = -4001" + where_rr(f) + ";",
            head + key3(f[0], f[1], f[2]) + ": disposition -4001 outside -4000 to 4000");
    refused("rr-band", "UPDATE race_relation SET band = 3" + where_rr(f) + ";",
            head + key3(f[0], f[1], f[2]) + ": band 3 outside 0 to 2");
    // the last row of f's pair repeated one year later
    std::vector<std::int64_t> last = f;
    for (const auto& x : b.rr) {
        if (x[0] == f[0] && x[1] == f[1]) last = x;
    }
    if (last[2] + 1 <= b.E - 1) {
        refused("rr-equal", "INSERT INTO race_relation VALUES (" + n(last[0]) + ", " + n(last[1]) + ", " +
                                n(last[2] + 1) + ", " + n(last[3]) + ", " + n(last[4]) + ");",
                head + key3(last[0], last[1], last[2] + 1) + ": equals the previous row of this pair");
    }
    // a pair's first row moved off its meeting (the pair's first row deleted, when it has a second)
    bool moved = false;
    for (std::size_t i = 0; i + 1 < b.rr.size() && !moved; ++i) {
        const auto& x = b.rr[i];
        const auto& y = b.rr[i + 1];
        if ((i == 0 || b.rr[i - 1][0] != x[0] || b.rr[i - 1][1] != x[1]) && y[0] == x[0] && y[1] == x[1]) {
            refused("rr-meeting", "DELETE FROM race_relation" + where_rr(x) + ";",
                    head + key3(y[0], y[1], y[2]) + ": first row is not at its meeting");
            moved = true;
        }
    }
    if (!moved) MESSAGE("no pair with two rows: `first row is not at its meeting` not exercised");
}

TEST_CASE("P5-03 16.4: each damaged settlement_race row is refused with its message") {
    const Base& b = base();
    const std::string head = "corrupt table 'settlement_race': row ";
    const std::vector<std::int64_t>& f = b.sr.front();
    refused("sr-unknown-settlement", "INSERT INTO settlement_race VALUES (0, 1, 0, 1000, 3);",
            head + key3(0, 1, 0) + ": unknown settlement 0");
    refused("sr-unknown-race", "INSERT INTO settlement_race VALUES (" + n(f[0]) + ", 0, " + n(f[2]) + ", 1000, 3);",
            head + key3(f[0], 0, f[2]) + ": unknown race 0");
    const std::int64_t fy = b.founded.at(f[0]);
    refused("sr-year", "INSERT INTO settlement_race VALUES (" + n(f[0]) + ", " + n(f[1]) + ", " + n(fy - 1) +
                           ", 1000, 3);",
            head + key3(f[0], f[1], fy - 1) + ": year " + n(fy - 1) + " outside " + n(fy) + " to " + n(b.E - 1));
    refused("sr-share", "UPDATE settlement_race SET share = 1001" + where_sr(f) + ";",
            head + key3(f[0], f[1], f[2]) + ": share 1001 outside 0 to 1000");
    refused("sr-standing", "UPDATE settlement_race SET standing = 4" + where_sr(f) + ";",
            head + key3(f[0], f[1], f[2]) + ": standing 4 outside 0 to 3");
    refused("sr-standing-share", "UPDATE settlement_race SET share = 0, standing = 3" + where_sr(f) + ";",
            head + key3(f[0], f[1], f[2]) + ": standing 3 with share 0");
    std::vector<std::int64_t> last = f;
    for (const auto& x : b.sr) {
        if (x[0] == f[0] && x[1] == f[1]) last = x;
    }
    if (last[2] + 1 <= b.E - 1 && last[3] > 0) {
        refused("sr-equal", "INSERT INTO settlement_race VALUES (" + n(last[0]) + ", " + n(last[1]) + ", " +
                                n(last[2] + 1) + ", " + n(last[3]) + ", " + n(last[4]) + ");",
                head + key3(last[0], last[1], last[2] + 1) + ": equals the previous row of this settlement and race");
    }
    // a settlement whose first year has one row (its founder's people at 1000, ruling): the sum and the ruling race
    for (std::size_t i = 0; i < b.sr.size(); ++i) {
        const auto& x = b.sr[i];
        const bool first = i == 0 || b.sr[i - 1][0] != x[0];
        const bool alone_at_year = [&] {
            for (const auto& y : b.sr) {
                if (y[0] == x[0] && y[2] <= x[2] && y[1] != x[1]) return false;
            }
            return true;
        }();
        if (!first || !alone_at_year || x[3] != 1000) continue;
        refused("sr-sum", "UPDATE settlement_race SET share = 999" + where_sr(x) + ";",
                head + key3(x[0], x[1], x[2]) + ": shares in force at " + n(x[2]) + " sum to 999");
        refused("sr-ruling", "UPDATE settlement_race SET standing = 2" + where_sr(x) + ";",
                head + key3(x[0], x[1], x[2]) + ": 0 ruling races at " + n(x[2]));
        break;
    }
}

TEST_CASE("P5-03 16.4: phrase lists, the events' checks and settlement.ending 6") {
    const Base& b = base();
    refused("bold-phrases", "UPDATE beast SET bold_phrases = 'Hunted the roads' WHERE id = 5;",
            "corrupt table 'beast': row 5: bold_phrases is not a phrase list");
    refused("bold-empty", "UPDATE beast SET bold_phrases = '' WHERE id = 5;",
            "corrupt table 'beast': row 5: bold_phrases is not a phrase list");
    refused("bold-five", "UPDATE beast SET bold_phrases = 'a|b|c|d|e' WHERE id = 5;",
            "corrupt table 'beast': row 5: bold_phrases is not a phrase list");
    refused("bold-gap", "UPDATE beast SET bold_phrases = 'a||b' WHERE id = 5;",
            "corrupt table 'beast': row 5: bold_phrases is not a phrase list");
    refused("raid-phrases", "UPDATE monster_kind SET raid_phrases = 'fell  upon' WHERE id = 4;",
            "corrupt table 'monster_kind': row 4: raid_phrases is not a phrase list");
    // every peoples_met names a pair with rows
    refused("met-no-rows", "DELETE FROM race_relation WHERE race_a = " + n(b.met_a) + " AND race_b = " + n(b.met_b) + ";",
            "corrupt table 'event': row " + n(b.met_event) + ": peoples_met without its rows");
    // a race participant naming no race row: P5-03a 12 item 2, event_catalogue.cpp's race_participant_problem in
    // P5-02 11.4's order, the first such row in key order (the settlement_mixed's role race, seq 3, before ruling)
    // (On a settlement_mixed event: a peoples_met participant changed would first break its pair's race_relation
    // rows, "first row is not at its meeting", which the history reader checks before the events.)
    REQUIRE(b.mixed_event != 0);
    refused("race-participant",
            "UPDATE event_participant SET entity_id = 999 WHERE entity_type = 1 AND event = (SELECT min(id) FROM event "
            "WHERE kind = (SELECT id FROM event_kind WHERE key = 'settlement_mixed'));",
            "corrupt table 'event_participant': row (" + n(b.mixed_event) + ", 3): race 999 does not exist");
}

TEST_CASE("P5-03a 10.4: the new values' ranges: sealed, standing and majority of kind 60, standing and years of 62") {
    refused_with("sealed", "UPDATE " + value_of("peoples_met", 3).replace("event_value WHERE", "event_value SET value = 2 WHERE") + ";",
                 "value sealed 2 outside 0 to 1");
    refused_with("mixed-standing",
                 "UPDATE " + value_of("settlement_mixed", 2).replace("event_value WHERE", "event_value SET value = 3 WHERE") + ";",
                 "value standing 3 outside 0 to 2");
    refused_with("mixed-majority",
                 "UPDATE " + value_of("settlement_mixed", 3).replace("event_value WHERE", "event_value SET value = 2 WHERE") + ";",
                 "value majority 2 outside 0 to 1");
    // kind 62 is rare: checked when the world holds a people_expelled
    const db::WorldReader r = chrono_test::open_world(base_path());
    bool expelled = false;
    std::int64_t k62 = 0;
    for (const auto& k : chrono_test::rows_of(r, "event_kind")) {
        if (k[1].bytes == "people_expelled") k62 = k[0].integer;
    }
    for (const auto& e : chrono_test::rows_of(r, "event")) expelled = expelled || e[3].integer == k62;
    if (!expelled) {
        MESSAGE("seed 42 at 512 has no people_expelled: kind 62's value checks are left to the slow worlds");
        return;
    }
    refused_with("expel-standing",
                 "UPDATE " + value_of("people_expelled", 2).replace("event_value WHERE", "event_value SET value = 2 WHERE") + ";",
                 "value standing 2 outside 0 to 1");
    refused_with("expel-years",
                 "UPDATE " + value_of("people_expelled", 3).replace("event_value WHERE", "event_value SET value = -1 WHERE") + ";",
                 "value years -1 outside 0 to");
}

TEST_CASE("P5-03 16.4: two peoples_met of one pair are refused") {
    const Base& b = base();
    const db::WorldReader r0 = chrono_test::open_world(base_path());
    const std::int64_t next = static_cast<std::int64_t>(chrono_test::rows_of(r0, "event").size()) + 1;
    std::string sql;
    for (const auto& e : chrono_test::rows_of(r0, "event")) {
        if (e[0].integer != b.met_event) continue;
        // dated in the last year and phase, so that the log stays in time order
        const std::int64_t phases = static_cast<std::int64_t>(chrono_test::rows_of(r0, "phase").size());
        sql = "INSERT INTO event VALUES (" + n(next) + ", " + n(b.E - 1) + ", " + n(b.E - 1) + ", " +
              n(e[3].integer) + ", " + n(phases) + ", " + n(e[5].integer) + ", 0, " + n(e[7].integer) + ");";
    }
    sql += "INSERT INTO event_participant VALUES (" + n(next) + ", 1, 1, 1, " + n(b.met_a) + ");";
    sql += "INSERT INTO event_participant VALUES (" + n(next) + ", 2, 2, 1, " + n(b.met_b) + ");";
    for (const auto& v : chrono_test::rows_of(r0, "event_value")) {
        if (v[0].integer == b.met_event) {
            sql += "INSERT INTO event_value VALUES (" + n(next) + ", " + n(v[1].integer) + ", " + n(v[2].integer) + ");";
        }
    }
    for (const auto& c : chrono_test::rows_of(r0, "event_cause")) {
        if (c[0].integer == b.met_event) sql += "INSERT INTO event_cause VALUES (" + n(next) + ", " + n(c[1].integer) + ");";
    }
    refused("met-twice", sql, "corrupt table 'event': row " + n(next) + ": " + n(b.met_a) + "-" + n(b.met_b) +
                                  " met twice");
}

TEST_CASE("P5-03 16.1, 16.3: a schema 9 file is refused; hash --detail lists the two tables") {
    const std::string path = corrupt("schema9", "PRAGMA user_version = 9; UPDATE meta SET value = '9' WHERE key = "
                                                "'schema_version';");
    auto r = db::WorldReader::open(path);
    REQUIRE_FALSE(r.ok());
    CHECK(r.error().message.find("world schema version 9, this build reads 10") != std::string::npos);
    const db::WorldReader w = chrono_test::open_world(base_path());
    auto d = worldgen::inspect::detail_hashes(w);
    REQUIRE(d.ok());
    std::vector<std::string> names;
    for (const auto& t : d.value().tables) names.push_back(t.name);
    CHECK(std::find(names.begin(), names.end(), "race_relation") != names.end());
    CHECK(std::find(names.begin(), names.end(), "settlement_race") != names.end());
}

}  // TEST_SUITE
