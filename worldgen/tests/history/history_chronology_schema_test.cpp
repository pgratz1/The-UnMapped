// Spec P3-03 sections 4 and 5, tests 14.1 items 7, 8 and 11 and 14.3 item
// 20 (library side): schema version 6, the five statements of 5.1 byte for
// byte, the empty world, the refusal of version 5, the round trip of every
// row type (NULL awakening_year included), no REAL; the eight meta keys of
// 4.2: canonical text for every value, the writer's refusals, finalize
// without a key, the reader's refusals with its message, and
// history_params_from_meta as the inverse of history_meta_entries; the
// five table lines of hash --detail, with the NULL tag hand-computed.

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p3_03_chronology_support.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/inspect/hashes.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace chrono_test;
using chrono_api::Params;

const char* const DDL_RACE =
    "CREATE TABLE race (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT NULL, "
    "adjective TEXT NOT NULL, camp INTEGER NOT NULL, lifespan_years INTEGER NOT NULL, generation_years INTEGER NOT "
    "NULL, culture INTEGER NOT NULL, present INTEGER NOT NULL, awakening_year INTEGER, creator_god INTEGER NOT NULL) "
    "STRICT";
const char* const DDL_RACE_BIOME =
    "CREATE TABLE race_biome (race INTEGER NOT NULL, biome INTEGER NOT NULL, weight INTEGER NOT NULL, PRIMARY KEY "
    "(race, biome)) STRICT, WITHOUT ROWID";
const char* const DDL_CULTURE =
    "CREATE TABLE culture (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, race INTEGER NOT "
    "NULL) STRICT";
const char* const DDL_GOD = "CREATE TABLE god (id INTEGER PRIMARY KEY, name TEXT NOT NULL, camp INTEGER NOT NULL) STRICT";
const char* const DDL_AGE =
    "CREATE TABLE age (id INTEGER PRIMARY KEY, race INTEGER NOT NULL, start_year INTEGER NOT NULL, end_year INTEGER "
    "NOT NULL, name TEXT NOT NULL) STRICT";

const std::vector<std::string>& required_keys() {
    static const std::vector<std::string> v = {
        "content_hash",          "format",               "generator_version",    "history_end_year",
        "param.civs_per_race",
        "param.divine_activity", "param.divine_script",  "param.event_intensity", "param.history_detail",
        "param.history_span",    "param.map_size",       "param.monster_pressure", "param.races",
        "schema_version",        "seed",                 "seed_phrase",          "stages"};
    return v;
}

std::string empty_world(const dbtest::TempDir& dir, const std::string& name, const Params& h = Params{}) {
    const std::string path = dir.file(name);
    REQUIRE(worldgen::generate_world(world_params(1, 512, h, shipped(), std::string("none")), shipped(), path,
                                     worldgen::db::Overwrite::refuse)
                .ok());
    return path;
}

// A chronology world through the pipeline (synthetic region rows).
std::string chronology_world(const dbtest::TempDir& dir, const std::string& name, std::uint64_t seed, const Params& h) {
    const std::string path = dir.file(name);
    REQUIRE(run_pipeline(path, shipped(), seed, h, {"North Downs", "East Wood"}).empty());
    return path;
}

std::string open_error(const std::string& path) {
    const auto r = worldgen::db::WorldReader::open(path);
    return r.ok() ? std::string() : r.error().message;
}

std::string fatal_of(const std::function<void()>& f) {
    try {
        f();
    } catch (const worldgen::FatalError& e) {
        return e.what();
    }
    return {};
}

}  // namespace

TEST_SUITE("chronology") {

// --- 14.1 item 7 ------------------------------------------------------------------------------

TEST_CASE("P3-03 5.1, 14.1 item 7: SCHEMA_VERSION 6 and the five statements after region_biome's, byte for byte") {
    CHECK(chrono_api::schema_version() == 8);
    const std::vector<std::string> ddl = chrono_api::ddl_statements();
    REQUIRE(ddl.size() == 26U);  // P3-04 9.1: thirteen more after age's
    CHECK(ddl[7].rfind("CREATE TABLE region_biome ", 0) == 0U);
    CHECK(ddl[8] == DDL_RACE);
    CHECK(ddl[9] == DDL_RACE_BIOME);
    CHECK(ddl[10] == DDL_CULTURE);
    CHECK(ddl[11] == DDL_GOD);
    CHECK(ddl[12] == DDL_AGE);
    CHECK(chrono_api::required_meta_keys() == required_keys());
}

TEST_CASE("P3-03 5.1, 4.3, 14.1 item 7: the empty world: pages, the five empty tables, 17 meta keys (P3-04 9.4)") {
    dbtest::TempDir dir("chronology-empty");
    const std::string path = empty_world(dir, "e.db");
    CHECK(static_cast<std::int64_t>(dbtest::read_file(path).size()) == EMPTY_WORLD_BYTES);
    const dbtest::SqliteFile db(path);
    REQUIRE(db.ok());
    CHECK(db.rows("PRAGMA page_count") ==
          std::vector<std::vector<std::string>>{{std::to_string(EMPTY_WORLD_PAGES)}});
    CHECK(db.rows("PRAGMA user_version") == std::vector<std::vector<std::string>>{{"7"}});
    CHECK(db.rows("SELECT value FROM meta WHERE key = 'schema_version'") == std::vector<std::vector<std::string>>{{"8"}});
    for (const std::string& t : five_tables()) {
        CAPTURE(t);
        CHECK(db.rows("SELECT count(*) FROM " + t) == std::vector<std::vector<std::string>>{{"0"}});
    }
    std::vector<std::string> keys;
    for (const auto& row : db.rows("SELECT key FROM meta ORDER BY key")) {
        keys.push_back(row[0]);
    }
    CHECK(keys == required_keys());
    // The statements are stored in execution order, ninth to thirteenth.
    const auto sql = db.rows("SELECT name, sql FROM sqlite_schema WHERE type = 'table' ORDER BY rowid");
    REQUIRE(sql.size() == 26U);
    CHECK(sql[8] == std::vector<std::string>{"race", DDL_RACE});
    CHECK(sql[9] == std::vector<std::string>{"race_biome", DDL_RACE_BIOME});
    CHECK(sql[10] == std::vector<std::string>{"culture", DDL_CULTURE});
    CHECK(sql[11] == std::vector<std::string>{"god", DDL_GOD});
    CHECK(sql[12] == std::vector<std::string>{"age", DDL_AGE});
    CHECK(db.rows("SELECT name FROM sqlite_schema WHERE type = 'index' AND tbl_name IN ('race', 'culture') ORDER BY name") ==
          std::vector<std::vector<std::string>>{{"sqlite_autoindex_culture_1"}, {"sqlite_autoindex_race_1"}});
}

TEST_CASE("P3-03 4.4, 14.1 item 7: a version 5 file is refused: world schema version 5, this build reads 8") {
    dbtest::TempDir dir("chronology-v5");
    for (const char* sql : {"PRAGMA user_version = 5", "UPDATE meta SET value = '5' WHERE key = 'schema_version'"}) {
        CAPTURE(std::string_view(sql));
        const std::string path = empty_world(dir, std::string("w") + (sql[0] == 'P' ? "1" : "2") + ".db");
        REQUIRE(worldgen::db::testing::execute_on_file(path, sql).ok());
        const auto r = worldgen::db::WorldReader::open(path);
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == worldgen::db::ErrorCode::unsupported_schema_version);
        CHECK(r.error().message == "world schema version 5, this build reads 8");
    }
}

TEST_CASE("P3-03 5.2, 5.3, 14.1 item 7: round trip of every row type, NULL awakening_year included; no REAL") {
    dbtest::TempDir dir("chronology-rows");
    for (const auto& races : {six(), std::vector<std::string>{"elf", "orc"}, std::vector<std::string>{"dwarf"}}) {
        CAPTURE(joined(races));
        const Params h = params_with(6000, races);
        const std::string path = chronology_world(dir, "w-" + joined(races, '-') + ".db", 3, h);
        const Rows want = chrono_api::stage(shipped(), h, 3, {"North Downs", "East Wood"});
        const worldgen::db::WorldReader r = open_world(path);
        CHECK(rows_of_world(r) == want);
        const dbtest::SqliteFile db(path);
        for (const std::string& t : five_tables()) {
            CAPTURE(t);
            const auto info = db.rows("SELECT name FROM pragma_table_info('" + t + "')");
            for (const auto& col : info) {
                CHECK(db.rows("SELECT count(*) FROM " + t + " WHERE typeof(\"" + col[0] + "\") NOT IN ('integer', 'text', 'null')") ==
                      std::vector<std::vector<std::string>>{{"0"}});
                if (col[0] != "awakening_year") {
                    CHECK(db.rows("SELECT count(*) FROM " + t + " WHERE typeof(\"" + col[0] + "\") = 'null'") ==
                          std::vector<std::vector<std::string>>{{"0"}});
                }
            }
        }
        // awakening_year is NULL exactly for the absent races (5.2).
        CHECK(db.rows("SELECT count(*) FROM race WHERE (awakening_year IS NULL) != (present = 0)") ==
              std::vector<std::vector<std::string>>{{"0"}});
    }
}

// --- 14.1 item 8 ------------------------------------------------------------------------------

TEST_CASE("P3-03 4.1, 4.2, 14.1 item 8: HistoryParams defaults and history_meta_entries' canonical text") {
    CHECK(chrono_api::library_defaults() == Params{});
    const std::vector<std::pair<std::string, std::string>> want = {
        {"param.civs_per_race", "auto"},  {"param.divine_activity", "normal"}, {"param.divine_script", ""},
        {"param.event_intensity", "normal"}, {"param.history_detail", "normal"}, {"param.history_span", "6000"},
        {"param.monster_pressure", "normal"}, {"param.races", "elf,goblin,dwarf,faerie,human,orc"}};
    CHECK(chrono_api::meta_entries(Params{}) == want);
    auto value = [](const Params& p, const std::string& key) {
        for (const auto& [k, v] : chrono_api::meta_entries(p)) {
            if (k == key) {
                return v;
            }
        }
        return std::string("<absent>");
    };
    for (const std::int64_t c : {1, 2, 3, 4}) {
        Params p;
        p.civs = c;
        CHECK(value(p, "param.civs_per_race") == std::to_string(c));
    }
    for (const char* v : {"distant", "normal", "meddling"}) {
        Params p;
        p.activity = v;
        CHECK(value(p, "param.divine_activity") == v);
    }
    for (const char* v : {"calm", "normal", "turbulent"}) {
        Params p;
        p.intensity = v;
        CHECK(value(p, "param.event_intensity") == v);
    }
    for (const char* v : {"sparse", "normal", "dense"}) {
        Params p;
        p.detail = v;
        CHECK(value(p, "param.history_detail") == v);
    }
    for (const char* v : {"low", "normal", "high"}) {
        Params p;
        p.pressure = v;
        CHECK(value(p, "param.monster_pressure") == v);
    }
    for (const std::int64_t s : {2000, 2001, 6000, 11999, 12000}) {
        CHECK(value(params_with(s, six()), "param.history_span") == std::to_string(s));
    }
    CHECK(value(params_with(6000, {"elf", "orc"}), "param.races") == "elf,orc");
    CHECK(value(params_with(6000, {"orc"}), "param.races") == "orc");
}

TEST_CASE("P3-03 4.1, 14.1 item 8: history_params_from(Settings) after resolve_races") {
    wd::Settings s;
    s.history_span = 2000;
    s.history_detail = wd::HistoryDetail::dense;
    s.civs_per_race = 3;
    s.races = std::vector<std::string>{"orc", "elf"};
    s.event_intensity = wd::EventIntensity::calm;
    s.monster_pressure = wd::MonsterPressure::high;
    s.divine_activity = wd::DivineActivity::meddling;
    wd::resolve_races(s, shipped());
    Params want = params_with(2000, {"elf", "orc"});
    want.detail = "dense";
    want.civs = 3;
    want.intensity = "calm";
    want.pressure = "high";
    want.activity = "meddling";
    CHECK(chrono_api::params_from_settings(s) == want);
    wd::Settings all;
    wd::resolve_races(all, shipped());
    CHECK(chrono_api::params_from_settings(all) == Params{});
    // Unresolved races (all, std::nullopt) are a FatalError (4.1).
    wd::Settings unresolved;
    CHECK(fatal_of([&] { (void)chrono_api::params_from_settings(unresolved); }) ==
          "history_params_from: the races are not resolved");
}

TEST_CASE("P3-03 4.1: generate_world checks params.history before any work, naming the field") {
    dbtest::TempDir dir("chronology-params");
    struct Case {
        Params p;
        std::string field;
    };
    std::vector<Case> cases;
    cases.push_back({params_with(1999, six()), "span"});
    cases.push_back({params_with(12001, six()), "span"});
    {
        Params p;
        p.civs = 0;
        cases.push_back({p, "civs_per_race"});
        p.civs = 5;
        cases.push_back({p, "civs_per_race"});
    }
    cases.push_back({params_with(6000, {}), "races"});
    cases.push_back({params_with(6000, {"elf", "elf"}), "races"});
    cases.push_back({params_with(6000, {"Elf"}), "races"});
    cases.push_back({params_with(6000, {"elf", ""}), "races"});
    {
        Params p;
        p.script = std::string(63, 'a');
        cases.push_back({p, "divine_script"});
        p.script = std::string(64, 'A');
        cases.push_back({p, "divine_script"});
    }
    int n = 0;
    for (const Case& c : cases) {
        CAPTURE(c.field);
        const std::string path = dir.file("p" + std::to_string(n++) + ".db");
        const std::string msg = fatal_of([&] {
            (void)worldgen::generate_world(world_params(1, 512, c.p, shipped(), std::string("none")), shipped(), path,
                                           worldgen::db::Overwrite::refuse);
        });
        CHECK(msg == "generate_world: params.history." + c.field + " is invalid");
        CHECK_FALSE(dbtest::exists(path));
    }
    // Race ids that name no loaded race are the stage's check, not generate_world's (4.1 step 2).
    CHECK(fatal_of([&] {
              (void)worldgen::generate_world(world_params(1, 512, params_with(6000, {"troll"}), shipped(),
                                                          std::string("none")),
                                             shipped(), dir.file("troll.db"), worldgen::db::Overwrite::refuse);
          }).empty());
}

TEST_CASE("P3-03 4.1, 4.3, 14.3 item 20: every kind of world records the eight keys") {
    dbtest::TempDir dir("chronology-meta");
    Params h = params_with(2000, {"orc"});
    h.detail = "sparse";
    h.civs = 4;
    h.intensity = "turbulent";
    h.pressure = "low";
    h.activity = "distant";
    const std::vector<std::pair<std::string, std::string>> want = chrono_api::meta_entries(h);
    std::vector<std::string> worlds;
    worlds.push_back(empty_world(dir, "none.db", h));
    {
        worldgen::WorldParams p = world_params(1, 512, h, shipped());
        p.test_pattern = true;
        REQUIRE(worldgen::generate_world(p, shipped(), dir.file("tp.db"), worldgen::db::Overwrite::refuse).ok());
        worlds.push_back(dir.file("tp.db"));
    }
    worlds.push_back(chronology_world(dir, "chrono.db", 5, h));
    {
        // write_fixture_world records the defaults of HistoryParams (4.1).
        REQUIRE(worldgen::stages::testing::write_fixture_world(dir.file("fixture.db"), 5, {}).ok());
        const worldgen::db::WorldReader r = open_world(dir.file("fixture.db"));
        for (const auto& [k, v] : chrono_api::meta_entries(Params{})) {
            CAPTURE(k);
            CHECK(meta_or(r, k) == v);
        }
    }
    for (const std::string& path : worlds) {
        CAPTURE(path);
        const worldgen::db::WorldReader r = open_world(path);
        for (const auto& [k, v] : want) {
            CAPTURE(k);
            CHECK(meta_or(r, k) == v);
        }
        CHECK(chrono_api::params_from_meta(r) == h);
    }
}

TEST_CASE("P3-03 4.1, 14.1 item 8: history_params_from_meta inverts history_meta_entries for every value") {
    dbtest::TempDir dir("chronology-inverse");
    // Every value of each setting at least once, combined round-robin, written
    // into empty worlds and read back.
    const std::vector<std::optional<std::int64_t>> civs = {std::nullopt, 1, 2, 3, 4};
    const std::vector<std::string> details = {"sparse", "normal", "dense"};
    const std::vector<std::string> intensities = {"calm", "normal", "turbulent"};
    const std::vector<std::string> pressures = {"low", "normal", "high"};
    const std::vector<std::string> activities = {"distant", "normal", "meddling"};
    const std::vector<std::int64_t> spans = {2000, 6000, 12000, 4321, 11999};
    const std::vector<std::vector<std::string>> races = {six(), {"elf", "orc"}, {"orc"}, {"goblin", "dwarf", "human"},
                                                         {"faerie"}};
    for (std::size_t i = 0; i < 15; ++i) {
        Params p = params_with(spans[i % spans.size()], races[i % races.size()]);
        p.civs = civs[i % civs.size()];
        p.detail = details[i % 3];
        p.intensity = intensities[(i / 3) % 3];
        p.pressure = pressures[(i + 1) % 3];
        p.activity = activities[(i / 5) % 3];
        CAPTURE(i);
        const std::string path = empty_world(dir, "w" + std::to_string(i) + ".db", p);
        CHECK(chrono_api::params_from_meta(open_world(path)) == p);
    }
}

TEST_CASE("P3-03 4.1, 14.1 item 8: set_param_text refuses a non-canonical value for each key") {
    const std::vector<std::pair<std::string, std::vector<std::string>>> bad = {
        {"civs_per_race", {"0", "5", "Auto", "01", "auto,1", ""}},
        {"divine_activity", {"Normal", "", "meddle"}},
        {"divine_script", {std::string(63, 'a'), std::string(64, 'A'), std::string(65, 'a'), "x"}},
        {"event_intensity", {"Calm", "", "wild"}},
        {"history_detail", {"Normal", "", "normal "}},
        {"history_span", {"2000 ", "02000", "1999", "12001", "+6000", "-6000", ""}},
        {"monster_pressure", {"LOW", "", "medium"}},
        {"races", {"", "elf,,orc", "elf,elf", "Elf", "elf, orc", ",elf", "elf,"}},
    };
    for (const auto& [name, values] : bad) {
        for (const std::string& v : values) {
            CAPTURE(name);
            CAPTURE(v);
            worldgen::db::WorldWriter w;
            CHECK(fatal_of([&] { chrono_api::set_param_text(w, name, v); }) ==
                  "WorldWriter::set_param_text: '" + v + "' is not the canonical form of a value of meta key 'param." +
                      name + "'");
        }
    }
    // map_size stays integer-only.
    {
        worldgen::db::WorldWriter w;
        CHECK(fatal_of([&] { chrono_api::set_param_text(w, "map_size", "1024"); }) ==
              "WorldWriter::set_param_text: map_size is an integer parameter");
    }
    // The same canonical check through set_param_integer and set_param_decimal
    // for the eight keys (the message names the canonical form and the key).
    const std::vector<std::pair<std::string, std::int64_t>> bad_int = {
        {"history_span", 1999}, {"history_span", 12001}, {"civs_per_race", 0}, {"civs_per_race", 5}};
    for (const auto& [name, v] : bad_int) {
        CAPTURE(name);
        CAPTURE(v);
        worldgen::db::WorldWriter w;
        CHECK(fatal_of([&] { chrono_api::set_param_integer(w, name, v); }) ==
              "WorldWriter::set_param_integer: '" + std::to_string(v) +
                  "' is not the canonical form of a value of meta key 'param." + name + "'");
    }
    for (const char* name : {"history_detail", "races", "history_span"}) {
        CAPTURE(std::string_view(name));
        worldgen::db::WorldWriter w;
        CHECK(fatal_of([&] { chrono_api::set_param_decimal(w, name, "0.5"); }) ==
              "WorldWriter::set_param_decimal: '0.5' is not the canonical form of a value of meta key 'param." +
                  std::string(name) + "'");
    }
    {
        worldgen::db::WorldWriter w;
        CHECK_NOTHROW(chrono_api::set_param_integer(w, "history_span", 6000));
        CHECK_NOTHROW(chrono_api::set_param_integer(w, "civs_per_race", 2));
    }
    const std::vector<std::pair<std::string, std::string>> good = {
        {"civs_per_race", "auto"}, {"civs_per_race", "4"}, {"divine_script", ""},
        {"divine_script", std::string(64, 'f')}, {"history_span", "2000"}, {"history_span", "12000"},
        {"races", "elf,goblin,dwarf,faerie,human,orc"}, {"races", "orc"}};
    for (const auto& [name, v] : good) {
        CAPTURE(name);
        CAPTURE(v);
        worldgen::db::WorldWriter w;
        CHECK_NOTHROW(chrono_api::set_param_text(w, name, v));
    }
}

TEST_CASE("P3-03 4.1, 14.1 item 8: finalize without one of the eight keys names the first missing key") {
    dbtest::TempDir dir("chronology-finalize");
    const std::vector<std::pair<std::string, std::string>> all = chrono_api::meta_entries(Params{});
    for (std::size_t skip = 0; skip < all.size(); ++skip) {
        CAPTURE(all[skip].first);
        worldgen::db::WorldWriter w;
        w.set_seed(1);
        w.set_content_hash(wd::content_hash(shipped()));
        w.set_param_integer("map_size", 512);
        for (std::size_t i = 0; i < all.size(); ++i) {
            if (i != skip) {
                chrono_api::set_param_text(w, all[i].first.substr(6), all[i].second);
            }
        }
        const std::string msg = fatal_of([&] { (void)w.finalize(dir.file("f.db"), worldgen::db::Overwrite::refuse); });
        CHECK(msg == "WorldWriter::finalize: meta key '" + all[skip].first + "' was not set");
        CHECK_FALSE(dbtest::exists(dir.file("f.db")));
    }
    // Two missing: the first in ascending order.
    worldgen::db::WorldWriter w;
    w.set_seed(1);
    w.set_content_hash(wd::content_hash(shipped()));
    w.set_param_integer("map_size", 512);
    chrono_api::set_param_text(w, "races", "elf");
    const std::string msg = fatal_of([&] { (void)w.finalize(dir.file("g.db"), worldgen::db::Overwrite::refuse); });
    CHECK(msg == "WorldWriter::finalize: meta key 'param.civs_per_race' was not set");
}

TEST_CASE("P3-03 4.2, 14.1 item 8: the reader refuses each key missing or invalid, with its message") {
    dbtest::TempDir dir("chronology-reader");
    const std::string base = empty_world(dir, "base.db");
    const std::vector<std::uint8_t> bytes = dbtest::read_file(base);
    int n = 0;
    auto variant = [&](const std::string& sql) {
        const std::string path = dir.file("v" + std::to_string(n++) + ".db");
        dbtest::write_file(path, bytes);
        REQUIRE(worldgen::db::testing::execute_on_file(path, sql).ok());
        return path;
    };
    for (const std::string& key : eight_keys()) {
        CAPTURE(key);
        const std::string path = variant("DELETE FROM meta WHERE key = '" + key + "'");
        const auto r = worldgen::db::WorldReader::open(path);
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == worldgen::db::ErrorCode::meta_key_missing);
    }
    const std::vector<std::pair<std::string, std::string>> bad = {
        {"param.history_span", "2000 "},       {"param.history_span", "02000"},  {"param.history_span", "1999"},
        {"param.history_span", "12001"},       {"param.history_detail", "Normal"}, {"param.civs_per_race", "auto,1"},
        {"param.civs_per_race", "0"},          {"param.races", "elf,,orc"},      {"param.races", "elf,elf"},
        {"param.races", ""},                   {"param.divine_script", std::string(63, 'a')},
        {"param.event_intensity", "normal "},  {"param.monster_pressure", "Low"}, {"param.divine_activity", "x"}};
    for (const auto& [key, value] : bad) {
        CAPTURE(key);
        CAPTURE(value);
        const std::string path = variant("UPDATE meta SET value = '" + value + "' WHERE key = '" + key + "'");
        const auto r = worldgen::db::WorldReader::open(path);
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == worldgen::db::ErrorCode::meta_value_invalid);
        CHECK(r.error().message ==
              "world file '" + path + "' has an invalid value '" + value + "' for meta key '" + key + "'");
    }
    // The P5 form of the script and any syntactically valid race list are accepted.
    for (const auto& [key, value] : std::vector<std::pair<std::string, std::string>>{
             {"param.divine_script", std::string(64, 'a')}, {"param.races", "troll,gnome"}, {"param.races", "orc,elf"}}) {
        CAPTURE(value);
        CHECK(open_error(variant("UPDATE meta SET value = '" + value + "' WHERE key = '" + key + "'")).empty());
    }
}

// --- 14.1 item 11 (library side) --------------------------------------------------------------

TEST_CASE("P3-03 9.2, 14.1 item 11: the five table lines of hash --detail; NULL awakening_year has tag 0x00") {
    dbtest::TempDir dir("chronology-detail");
    const Params h = params_with(6000, {"elf", "orc"});
    const std::string path = chronology_world(dir, "w.db", 3, h);
    const worldgen::db::WorldReader r = open_world(path);
    auto d = worldgen::inspect::detail_hashes(r);
    REQUIRE(d.ok());
    std::vector<std::string> names;
    for (const auto& t : d.value().tables) {
        names.push_back(t.name);
    }
    CHECK(names == std::vector<std::string>{"age",
                                            "biome",
                                            "civilization",
                                            "climate",
                                            "culture",
                                            "event",
                                            "event_cause",
                                            "event_kind",
                                            "event_kind_role",
                                            "event_kind_value",
                                            "event_participant",
                                            "event_value",
                                            "god",
                                            "layer",
                                            "meta",
                                            "pass",
                                            "phase",
                                            "race",
                                            "race_biome",
                                            "region",
                                            "region_biome",
                                            "region_border",
                                            "region_control",
                                            "relation",
                                            "settlement",
                                            "settlement_census"});  // schema 7 (P3-04 9.1)
    const Rows rows = chrono_api::stage(shipped(), h, 3, {"North Downs", "East Wood"});
    const std::map<std::string, std::string> want = table_hashes(rows);
    const std::map<std::string, std::string> got = detail_tables(r);
    for (const std::string& t : five_tables()) {
        CAPTURE(t);
        CHECK(got.at(t) == want.at(t));
    }
    // Hand-computed: one race row with awakening_year NULL, encoded with tag 0x00.
    TableHasher one(12);
    one.integer(2);
    one.text("goblin");
    one.text("Goblin");
    one.text("Goblins");
    one.text("Goblin");
    one.integer(2);
    one.integer(40);
    one.integer(15);
    one.integer(4);
    one.integer(0);
    one.null();
    one.integer(2);
    Rows only;
    only.races.push_back(rows.races[1]);
    CHECK(table_hashes(only).at("race") == one.hex());
    // A world with only that row in table race: hash --detail agrees.
    dbtest::write_file(dir.file("one.db"), dbtest::read_file(path));
    REQUIRE(worldgen::db::testing::execute_on_file(dir.file("one.db"), "DELETE FROM race WHERE id != 2").ok());
    CHECK(detail_tables(open_world(dir.file("one.db"))).at("race") == one.hex());
}

}  // TEST_SUITE
