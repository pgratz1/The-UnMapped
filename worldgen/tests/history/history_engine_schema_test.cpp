// P5-02 11.1 (schema 9): the settlement statement's last column ending and the five tables after region_name;
// SCHEMA_VERSION 9 (P5-02 19.1 item 14 and 17 step 1).
// Spec P3-04 section 9 (schema 7), test 16.1 item 12: SCHEMA_VERSION 7,
// the thirteen statements of 9.1 byte for byte and their place in
// sqlite_schema, the autoindex of event_kind, the 17 required meta keys,
// the empty world's size (one named constant, EMPTY_WORLD_PAGES, spec 9.6
// unconfirmed), refusal of a version 6 file, round trip of every new row
// type with NULLs, no REAL, the writers' ascending-key refusals, and
// meta.history_end_year in every world kind of 9.5 and refused when it
// differs from param.history_span.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "p3_04_engine_support.hpp"
#include "worldgen/db/testing.hpp"

namespace {

using namespace engine_test;

const std::vector<std::string>& new_ddl() {
    static const std::vector<std::string> v = {
        "CREATE TABLE event_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, caused INTEGER NOT NULL, "
        "region_use INTEGER NOT NULL, settlement_use INTEGER NOT NULL, salience INTEGER NOT NULL, template TEXT NOT "
        "NULL) STRICT",
        "CREATE TABLE event_kind_role (kind INTEGER NOT NULL, seq INTEGER NOT NULL, role TEXT NOT NULL, entity_type "
        "INTEGER NOT NULL, min_count INTEGER NOT NULL, max_count INTEGER NOT NULL, PRIMARY KEY (kind, seq)) STRICT, "
        "WITHOUT ROWID",
        "CREATE TABLE event_kind_value (kind INTEGER NOT NULL, seq INTEGER NOT NULL, key TEXT NOT NULL, PRIMARY KEY "
        "(kind, seq)) STRICT, WITHOUT ROWID",
        "CREATE TABLE phase (id INTEGER PRIMARY KEY, age INTEGER NOT NULL, kind INTEGER NOT NULL, start_year INTEGER "
        "NOT NULL, end_year INTEGER NOT NULL, trigger_event INTEGER NOT NULL) STRICT",
        "CREATE TABLE event (id INTEGER PRIMARY KEY, year INTEGER NOT NULL, end_year INTEGER NOT NULL, kind INTEGER "
        "NOT NULL, phase INTEGER NOT NULL, region INTEGER NOT NULL, settlement INTEGER NOT NULL, salience INTEGER NOT "
        "NULL) STRICT",
        "CREATE TABLE event_participant (event INTEGER NOT NULL, seq INTEGER NOT NULL, role INTEGER NOT NULL, "
        "entity_type INTEGER NOT NULL, entity_id INTEGER NOT NULL, PRIMARY KEY (event, seq)) STRICT, WITHOUT ROWID",
        "CREATE TABLE event_cause (event INTEGER NOT NULL, cause INTEGER NOT NULL, PRIMARY KEY (event, cause)) "
        "STRICT, WITHOUT ROWID",
        "CREATE TABLE event_value (event INTEGER NOT NULL, seq INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY "
        "(event, seq)) STRICT, WITHOUT ROWID",
        "CREATE TABLE civilization (id INTEGER PRIMARY KEY, name TEXT NOT NULL, race INTEGER NOT NULL, culture "
        "INTEGER NOT NULL, home_region INTEGER NOT NULL, capital INTEGER NOT NULL, founded_year INTEGER NOT NULL, "
        "founding_event INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER NOT NULL) STRICT",
        "CREATE TABLE settlement (id INTEGER PRIMARY KEY, name TEXT NOT NULL, x INTEGER NOT NULL, y INTEGER NOT "
        "NULL, region INTEGER NOT NULL, founder_civ INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event "
        "INTEGER NOT NULL, owner_civ INTEGER NOT NULL, kind INTEGER NOT NULL, status INTEGER NOT NULL, population "
        "INTEGER NOT NULL, ruined_year INTEGER, ruin_event INTEGER NOT NULL, ending INTEGER NOT NULL) STRICT",   // P5-02 11.1
        "CREATE TABLE settlement_census (settlement INTEGER NOT NULL, year INTEGER NOT NULL, owner_civ INTEGER NOT "
        "NULL, kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT NULL, PRIMARY KEY (settlement, "
        "year)) STRICT, WITHOUT ROWID",
        "CREATE TABLE region_control (region INTEGER NOT NULL, from_year INTEGER NOT NULL, to_year INTEGER, civ "
        "INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID",
        "CREATE TABLE relation (a_type INTEGER NOT NULL, a_id INTEGER NOT NULL, b_type INTEGER NOT NULL, b_id "
        "INTEGER NOT NULL, kind TEXT NOT NULL, since INTEGER NOT NULL, until INTEGER, since_event INTEGER NOT NULL, "
        "until_event INTEGER NOT NULL, PRIMARY KEY (a_type, a_id, b_type, b_id, kind, since)) STRICT, WITHOUT ROWID",
        // P3-04 r21 23.4 (P5-12, schema 8): appended after relation, byte for byte
        "CREATE TABLE region_name (region INTEGER NOT NULL, from_year INTEGER NOT NULL, name TEXT NOT NULL, culture "
        "INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID",
        // P5-02 11.1 (schema 9): five statements after region_name's, in this order, byte for byte
        // P5-03 16.1 (schema 10): beast gains bold_phrases and monster_kind raid_phrases, last
        "CREATE TABLE beast (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT "
        "NULL, menace INTEGER NOT NULL, food INTEGER NOT NULL, bold_phrases TEXT NOT NULL) STRICT",
        "CREATE TABLE monster_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, noun TEXT "
        "NOT NULL, grouped INTEGER NOT NULL, wake INTEGER NOT NULL, might INTEGER NOT NULL, raid_phrases TEXT NOT "
        "NULL) STRICT",
        "CREATE TABLE beast_population (region INTEGER NOT NULL, beast INTEGER NOT NULL, year INTEGER NOT NULL, count "
        "INTEGER NOT NULL, PRIMARY KEY (region, beast, year)) STRICT, WITHOUT ROWID",
        "CREATE TABLE monster (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, lair_site INTEGER "
        "NOT NULL, born INTEGER NOT NULL, born_event INTEGER NOT NULL, died INTEGER, death_event INTEGER NOT NULL, "
        "slain_by INTEGER NOT NULL, status INTEGER NOT NULL, hoard INTEGER NOT NULL) STRICT",
        "CREATE TABLE site (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, region INTEGER NOT "
        "NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, settlement INTEGER NOT NULL, founder_civ INTEGER NOT NULL, "
        "monster INTEGER NOT NULL, origin_year INTEGER NOT NULL, origin_event INTEGER NOT NULL, ended_year INTEGER, "
        "ending_event INTEGER NOT NULL, ending INTEGER NOT NULL) STRICT",
        // P5-03 16.1 (schema 10): two statements after site's, in this order
        "CREATE TABLE race_relation (race_a INTEGER NOT NULL, race_b INTEGER NOT NULL, year INTEGER NOT NULL, "
        "disposition INTEGER NOT NULL, band INTEGER NOT NULL, PRIMARY KEY (race_a, race_b, year)) STRICT, WITHOUT ROWID",
        "CREATE TABLE settlement_race (settlement INTEGER NOT NULL, race INTEGER NOT NULL, year INTEGER NOT NULL, share "
        "INTEGER NOT NULL, standing INTEGER NOT NULL, PRIMARY KEY (settlement, race, year)) STRICT, WITHOUT ROWID",
    };
    return v;
}

std::string generated(const dbtest::TempDir& dir, const std::string& name, const std::string& until,
                      std::int64_t span = 6000) {
    const std::string path = dir.file(name);
    chrono_api::Params p;
    p.span = span;
    REQUIRE(worldgen::generate_world(chrono_test::world_params(1, 512, p, shipped(),
                                                               until.empty() ? std::nullopt : std::optional(until)),
                                     shipped(), path, worldgen::db::Overwrite::refuse)
                .ok());
    return path;
}

// Rows of every new table, small and valid, including NULLs, in key order.
std::map<std::string, std::vector<Row>> sample_rows() {
    using eng::I;
    using eng::N;
    using eng::T;
    return {
        {"event_kind", {{I(1), T("awakening"), I(0), I(1), I(0), I(100), T("{race}")},
                        {I(2), T("x_kind"), I(1), I(2), I(2), I(0), T("t")}}},
        {"event_kind_role", {{I(1), I(1), T("race"), I(1), I(1), I(1)}, {I(1), I(2), T("creator"), I(2), I(1), I(1)},
                             {I(2), I(1), T("actor"), I(3), I(0), I(0)}}},
        {"event_kind_value", {{I(2), I(1), T("years")}, {I(2), I(2), T("dead")}}},
        {"phase", {{I(1), I(1), I(1), I(0), I(149), I(1)}, {I(2), I(1), I(2), I(150), I(5999), I(0)}}},
        {"event", {{I(1), I(0), I(0), I(1), I(1), I(1), I(0), I(100)}, {I(2), I(5), I(9), I(2), I(1), I(0), I(1), I(7)}}},
        {"event_participant", {{I(1), I(1), I(1), I(1), I(1)}, {I(1), I(2), I(2), I(2), I(1)}, {I(2), I(1), I(1), I(3), I(1)}}},
        {"event_cause", {{I(2), I(1)}}},
        {"event_value", {{I(2), I(1), I(-3)}, {I(2), I(2), I(2147483647)}}},
        {"civilization", {{I(1), T("Finen"), I(1), I(2), I(12), I(1), I(0), I(2), N(), I(0)},
                          {I(2), T("The Other"), I(2), I(4), I(10), I(0), I(5), I(2), I(9), I(2)}}},
        {"settlement", {{I(1), T("Voeledell"), I(755), I(293), I(12), I(1), I(0), I(3), I(1), I(1), I(1), I(200), N(), I(0),
                         I(0)},
                        {I(2), T("Ruin"), I(0), I(0), I(10), I(2), I(5), I(2), I(0), I(5), I(3), I(0), I(9), I(2),
                         I(1)}}},   // P5-02 11.1: ending
        {"settlement_census", {{I(1), I(0), I(1), I(1), I(1), I(200)}, {I(1), I(150), I(1), I(2), I(1), I(250)},
                               {I(2), I(5), I(2), I(5), I(1), I(10)}}},
        {"region_control", {{I(10), I(5), I(9), I(2), I(2)}, {I(12), I(0), N(), I(1), I(1)}}},
        {"relation", {{I(3), I(1), I(3), I(2), T("peace"), I(5), I(9), I(2), I(2)},
                      {I(3), I(1), I(3), I(2), T("war"), I(5), N(), I(2), I(0)}}},
        // schema 8 (P3-04 r21 23.4): (region, from_year) keys ascending, one region twice
        {"region_name", {{I(10), I(5), T("The Ashes of Ruin"), I(1), I(3)}, {I(10), I(9), T("Ruin's Grave"), I(0), I(4)},
                         {I(12), I(0), T("The Heart of Voeledell"), I(2), I(1)}}},
        // schema 9 (P5-02 11.1): NULL in monster.died and site.ended_year
        {"beast", {{I(1), T("bear"), T("Bear"), T("bears"), I(30), I(30), T("broke into the stores of")},
                   {I(2), T("wolf"), T("Wolf"), T("wolves"), I(10), I(0), T("hunted the roads from|grew bold")}}},
        {"monster_kind", {{I(1), T("dragon"), T("Dragon"), T("a dragon of the old world"), I(0), I(1), I(6000),
                           T("fell in fire upon")},
                          {I(2), T("undead"), T("Undead"), T("a host of the restless dead"), I(1), I(2), I(1500),
                           T("crept into|walked by night in")}}},
        {"beast_population", {{I(10), I(1), I(0), I(7)}, {I(10), I(1), I(149), I(6)}, {I(12), I(2), I(0), I(40)}}},
        {"monster", {{I(1), T("Old Grakh"), I(1), I(1), I(5), I(2), N(), I(0), I(0), I(1), I(9)},
                     {I(2), T("The Host of Edric"), I(2), I(2), I(5), I(2), I(9), I(2), I(1), I(3), I(0)}}},
        {"site", {{I(1), T("Grakh's Hollow"), I(1), I(10), I(4), I(5), I(0), I(0), I(1), I(5), I(2), N(), I(0), I(0)},
                  {I(2), T("Snat Watch"), I(2), I(12), I(6), I(7), I(0), I(1), I(0), I(5), I(2), I(9), I(2), I(6)}}},
        // schema 10 (P5-03 16.2): keys ascending, a negative disposition, a share 0 with standing 0
        {"race_relation", {{I(1), I(2), I(5), I(-250), I(1)}, {I(1), I(2), I(9), I(-600), I(0)}}},
        {"settlement_race", {{I(1), I(1), I(0), I(1000), I(3)}, {I(1), I(1), I(5), I(700), I(3)},
                             {I(1), I(2), I(5), I(300), I(0)}, {I(1), I(2), I(9), I(0), I(0)}}},
    };
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 9.1, r21 23.4, P5-02 11.1: schema version 9 and the nineteen history statements, fourteenth to "
          "thirty-second (settlement with ending; region_name; beast, monster_kind, beast_population, monster, site)") {
    CHECK(eng::schema_version() == 12);
    const std::vector<std::string> ddl = eng::ddl_statements();
    REQUIRE(ddl.size() == 36U);   // P5-03 16.1
    REQUIRE(new_ddl().size() == 21U);   // P5-03 16.1
    for (std::size_t i = 0; i < 21; ++i) {
        CAPTURE(i);
        CHECK(ddl[13 + i] == new_ddl()[i]);
    }
    CHECK(ddl[12].rfind("CREATE TABLE age ", 0) == 0);
    dbtest::TempDir dir("engine-schema");
    const std::string path = generated(dir, "none.db", "none");
    const dbtest::SqliteFile db(path);
    const auto sql = db.rows("SELECT name, sql FROM sqlite_schema WHERE type = 'table' ORDER BY rowid");
    REQUIRE(sql.size() == 36U);   // P5-03 16.1
    for (std::size_t i = 0; i < 21; ++i) {
        CHECK(sql[13 + i] == std::vector<std::string>{ddl_tables()[i], new_ddl()[i]});
    }
    CHECK(db.rows("SELECT name FROM sqlite_schema WHERE type = 'index' AND tbl_name = 'event_kind'") ==
          std::vector<std::vector<std::string>>{{"sqlite_autoindex_event_kind_1"}});
    // P5-02 11.1: the automatic indexes of the two UNIQUE keys
    CHECK(db.rows("SELECT name FROM sqlite_schema WHERE type = 'index' AND tbl_name IN ('beast', 'monster_kind') "
                  "ORDER BY name") ==
          std::vector<std::vector<std::string>>{{"sqlite_autoindex_beast_1"}, {"sqlite_autoindex_monster_kind_1"}});
    CHECK(db.rows("SELECT count(*) FROM sqlite_schema WHERE type = 'index' AND tbl_name IN ('phase', 'event', "
                  "'event_participant', 'event_cause', 'event_value', 'civilization', 'settlement', "
                  "'settlement_census', 'region_control', 'relation', 'event_kind_role', 'event_kind_value', "
                  "'region_name', 'beast_population', 'monster', 'site', 'race_relation', 'settlement_race')") ==
          std::vector<std::vector<std::string>>{{"0"}});
    CHECK(db.rows("PRAGMA user_version") == std::vector<std::vector<std::string>>{{"12"}});
}

TEST_CASE("P3-04 9.4, 16.1 item 12: 17 required meta keys, history_end_year fourth") {
    const std::vector<std::string> keys = eng::required_meta_keys();
    REQUIRE(keys.size() == 17U);
    CHECK(keys[0] == "content_hash");
    CHECK(keys[1] == "format");
    CHECK(keys[2] == "generator_version");
    CHECK(keys[3] == "history_end_year");
    CHECK(keys[4] == "param.civs_per_race");
    CHECK(keys.back() == "stages");
    CHECK(std::is_sorted(keys.begin(), keys.end()));
}

TEST_CASE("P3-04 9.6, 16.5: the empty world's size (34 pages, unconfirmed; EMPTY_WORLD_PAGES)") {
    dbtest::TempDir dir("engine-empty");
    const std::string path = generated(dir, "none.db", "none");
    const auto bytes = dbtest::read_file(path);
    CHECK(static_cast<std::int64_t>(bytes.size()) == EMPTY_WORLD_BYTES);
    CHECK(static_cast<std::int64_t>(dbtest::be32(bytes, 28)) == EMPTY_WORLD_PAGES);  // header page count
}

TEST_CASE("P3-04 9.5, P5-02 11.1: version 6 and 8 files are refused: world schema version <v>, this build reads 12") {
    dbtest::TempDir dir("engine-v6");
    int k = 0;
    for (const char* v : {"6", "8"}) {
        for (const std::string& sql : std::vector<std::string>{
                 std::string("PRAGMA user_version = ") + v,
                 std::string("UPDATE meta SET value = '") + v + "' WHERE key = 'schema_version'"}) {
            CAPTURE(sql);
            const std::string path = generated(dir, "w" + std::to_string(k++) + ".db", "none");
            REQUIRE(worldgen::db::testing::execute_on_file(path, sql).ok());
            const auto r = db::WorldReader::open(path);
            REQUIRE_FALSE(r.ok());
            CHECK(r.code() == db::ErrorCode::unsupported_schema_version);
            CHECK(r.error().message == std::string("world schema version ") + v + ", this build reads 12");
        }
    }
}

TEST_CASE("P3-04 9.4, 9.5: meta.history_end_year in every world kind; refused when it differs from the span") {
    dbtest::TempDir dir("engine-end-year");
    // Empty worlds (cheap) at three spans.
    for (const std::int64_t span : {2000, 2500, 12000}) {
        CAPTURE(span);
        const std::string path = generated(dir, "none-" + std::to_string(span) + ".db", "none", span);
        const db::WorldReader r = open_world(path);
        CHECK(r.meta("history_end_year").value_or("<absent>") == std::to_string(span));
        CHECK(r.meta("schema_version").value_or("") == "12");   // P5-03 16.1 (9 at P5-02, 8 at P5-12)
        for (const std::string& t : ddl_tables()) {
            CHECK_MESSAGE(rows_of(r, t).empty(), t);
        }
    }
    // The geography, chronology and full worlds: history_engine_golden_resume_test.cpp (resume refusals) and
    // cli_history_test.cpp (--until chronology, the default run).
    // test-pattern world
    {
        const std::string path = dir.file("pattern.db");
        worldgen::WorldParams p = chrono_test::world_params(1, 512, chrono_api::Params{}, shipped());
        p.test_pattern = true;
        REQUIRE(worldgen::generate_world(p, shipped(), path, db::Overwrite::refuse).ok());
        CHECK(open_world(path).meta("history_end_year").value_or("<absent>") == "6000");
    }
    int nbad = 0;
    for (const char* bad : {"5999", "06000", "6000 ", ""}) {
        CAPTURE(std::string_view(bad));
        const std::string path = generated(dir, "bad" + std::to_string(nbad++) + ".db", "none");
        REQUIRE(worldgen::db::testing::execute_on_file(
                    path, std::string("UPDATE meta SET value = '") + bad + "' WHERE key = 'history_end_year'")
                    .ok());
        const auto r = db::WorldReader::open(path);
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == db::ErrorCode::meta_value_invalid);
        CHECK(r.error().message == "world file '" + path + "' has an invalid value '" + bad +
                                       "' for meta key 'history_end_year'");
    }
    {
        const std::string path = generated(dir, "missing.db", "none");
        REQUIRE(worldgen::db::testing::execute_on_file(path, "DELETE FROM meta WHERE key = 'history_end_year'").ok());
        const auto r = db::WorldReader::open(path);
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == db::ErrorCode::meta_key_missing);
    }
}

TEST_CASE("P3-04 9.3, 16.1 item 12: round trip of every new row type with NULLs; no REAL; ascending keys") {
    dbtest::TempDir dir("engine-rows");
    const auto rows = sample_rows();
    const std::string path = dir.file("rows.db");
    {
        db::WorldWriter w;
        dbtest::set_standard_meta(w, 3);
        db::StageWriter s = w.begin_stage("history");
        for (const std::string& t : ddl_tables()) {
            for (const Row& r : rows.at(t)) {
                eng::write_row(s, t, r);
            }
        }
        s.commit();
        REQUIRE(w.finalize(path, db::Overwrite::refuse).ok());
    }
    const db::WorldReader r = open_world(path);
    const dbtest::SqliteFile db(path);
    for (const std::string& t : ddl_tables()) {
        CAPTURE(t);
        CHECK(str(rows_of(r, t)) == str(rows.at(t)));
        for (const auto& col : db.rows("SELECT name FROM pragma_table_info('" + t + "')")) {
            CHECK(db.rows("SELECT count(*) FROM " + t + " WHERE typeof(\"" + col[0] +
                          "\") NOT IN ('integer', 'text', 'null')") == std::vector<std::vector<std::string>>{{"0"}});
        }
    }
    // Each writer refuses a key not above the previous one (9.3: FatalError, as the region writers).
    for (const std::string& t : ddl_tables()) {
        CAPTURE(t);
        const std::vector<Row>& v = rows.at(t);
        db::WorldWriter w;
        dbtest::set_standard_meta(w, 3);
        db::StageWriter s = w.begin_stage("history");
        eng::write_row(s, t, v.back());
        const std::string e = fatal_of([&] { eng::write_row(s, t, v.front()); });
        CAPTURE(e);
        if (v.size() > 1U) {
            CHECK(e.rfind("StageWriter::write_" + t + ": ", 0) == 0);
        }
        const std::string again = fatal_of([&] { eng::write_row(s, t, v.back()); });  // equal key
        CHECK(again.rfind("StageWriter::write_" + t + ": ", 0) == 0);
    }
}

}  // TEST_SUITE
