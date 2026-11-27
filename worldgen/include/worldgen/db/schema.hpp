#pragma once

// World database schema, version 9 (spec P0-02 sections 4.2, 5;
// docs/specs/schema.md; table `pass` from P1-01 section 9.1, schema 2;
// tables `biome` and `climate` from P1-04 section 3 and P1-03 section 3,
// schema 3; column climate.temp_offset_dc, schema 4, P0-02 amendment 16;
// tables `region`, `region_border` and `region_biome` from P1-06 section 3,
// schema 5, P0-02 amendment 17; tables `race`, `race_biome`, `culture`,
// `god` and `age` and the eight history `meta` keys from P3-03 sections 4.2
// and 5, schema 6, P0-02 amendment 18; the thirteen history tables and
// meta key history_end_year from P3-04 section 9, schema 7, P0-02
// amendment 19).
//
// The DDL strings below are stored verbatim in sqlite_schema and are
// therefore part of every world file. Each is one line with single spaces,
// exactly as written in P0-02 section 5.4. Never load them from a file.

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace worldgen::db {

// The schema version (P0-02 5.2 rule 1). Written to PRAGMA user_version
// and meta.schema_version; a reader supports exactly this version.
inline constexpr std::int64_t SCHEMA_VERSION = 14;

// PRAGMA application_id of every world file: 0x554E4D50, ASCII "UNMP"
// (P0-02 4.2, header offset 68 in 6.1).
inline constexpr std::uint32_t APPLICATION_ID = 0x554E4D50U;

// Page size of every world file and of the working database (P0-02 4.1, 4.2).
inline constexpr std::uint32_t PAGE_SIZE = 4096U;

// DDL of the meta table (P0-02 5.4), executed first.
inline constexpr std::string_view DDL_META =
    "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) STRICT, WITHOUT ROWID";

// DDL of the layer table (P0-02 5.4), executed second.
inline constexpr std::string_view DDL_LAYER =
    "CREATE TABLE layer (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, width INTEGER NOT NULL, "
    "height INTEGER NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL) STRICT";

// DDL of the pass table (P1-01 9.1, schema version 2), executed third.
inline constexpr std::string_view DDL_PASS =
    "CREATE TABLE pass (id INTEGER PRIMARY KEY, range_id INTEGER NOT NULL, kind INTEGER NOT NULL, "
    "capped INTEGER NOT NULL, col_x INTEGER NOT NULL, col_y INTEGER NOT NULL, saddle_dm INTEGER NOT NULL, "
    "col_dm INTEGER NOT NULL, path BLOB NOT NULL) STRICT";

// The name of the pass table (P1-01 9.1).
inline constexpr std::string_view PASS_TABLE = "pass";

// DDL of the biome table (P1-04 3, schema version 3), executed fourth: one
// row per loaded biome definition, written by the biome stage.
inline constexpr std::string_view DDL_BIOME =
    "CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, "
    "colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT";

// DDL of the climate table (P1-03 3, schema version 4; the column
// temp_offset_dc is new in version 4), executed fifth: one row (id 1)
// written by the climate stage.
inline constexpr std::string_view DDL_CLIMATE =
    "CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL, temp_offset_dc INTEGER NOT NULL) "
    "STRICT";

// The names of the biome and climate tables (P1-04 3, P1-03 3).
inline constexpr std::string_view BIOME_TABLE = "biome";
inline constexpr std::string_view CLIMATE_TABLE = "climate";

// DDL of the region table (P1-06 3.1, schema version 5), executed sixth:
// one row per region, written by the regions stage in id order.
inline constexpr std::string_view DDL_REGION =
    "CREATE TABLE region (id INTEGER PRIMARY KEY, name TEXT NOT NULL, biome INTEGER NOT NULL, "
    "x_min INTEGER NOT NULL, y_min INTEGER NOT NULL, x_max INTEGER NOT NULL, y_max INTEGER NOT NULL, "
    "seed_x INTEGER NOT NULL, seed_y INTEGER NOT NULL, landmass INTEGER NOT NULL, cells INTEGER NOT NULL, "
    "lake_cells INTEGER NOT NULL, river_cells INTEGER NOT NULL, river_class_max INTEGER NOT NULL, "
    "coast_cells INTEGER NOT NULL, walk_cells INTEGER NOT NULL, walk_area INTEGER NOT NULL, "
    "elev_mean_dm INTEGER NOT NULL, elev_max_dm INTEGER NOT NULL, fertility_mean INTEGER NOT NULL, "
    "forage_mean INTEGER NOT NULL, timber_mean INTEGER NOT NULL, ore_mean INTEGER NOT NULL, "
    "fresh_water_mean INTEGER NOT NULL, habitability_mean INTEGER NOT NULL, habitability_p90 INTEGER NOT NULL, "
    "ore_rich_cells INTEGER NOT NULL, ore_reach_cells INTEGER NOT NULL) STRICT";

// DDL of the region_border table (P1-06 3.1, schema version 5), executed
// seventh: one row per pair of bordering regions, in (a, b) order.
inline constexpr std::string_view DDL_REGION_BORDER =
    "CREATE TABLE region_border (a INTEGER NOT NULL, b INTEGER NOT NULL, length_m INTEGER NOT NULL, "
    "low_dm INTEGER NOT NULL, low_x INTEGER NOT NULL, low_y INTEGER NOT NULL, walk_pairs INTEGER NOT NULL, "
    "river_pairs INTEGER NOT NULL, river_class_max INTEGER NOT NULL, pass_id INTEGER NOT NULL, "
    "passes INTEGER NOT NULL, PRIMARY KEY (a, b)) STRICT, WITHOUT ROWID";

// DDL of the region_biome table (P1-06 3.1, schema version 5), executed
// eighth: cells per region and biome code, in (region, biome) order.
inline constexpr std::string_view DDL_REGION_BIOME =
    "CREATE TABLE region_biome (region INTEGER NOT NULL, biome INTEGER NOT NULL, cells INTEGER NOT NULL, "
    "PRIMARY KEY (region, biome)) STRICT, WITHOUT ROWID";

// The names of the three region tables (P1-06 3.1).
inline constexpr std::string_view REGION_TABLE = "region";
inline constexpr std::string_view REGION_BORDER_TABLE = "region_border";
inline constexpr std::string_view REGION_BIOME_TABLE = "region_biome";

// DDL of the chronology tables (P3-03 5.1, schema version 6), executed
// ninth to thirteenth: written by the chronology stage, empty when it did
// not run. race: one row per race definition (present or not); race_biome:
// one row per listed biome preference; culture: one row per culture
// definition; god: one row per race (its creator god, id = race id); age:
// one row per present race. awakening_year is NULL exactly for an absent
// race.
inline constexpr std::string_view DDL_RACE =
    "CREATE TABLE race (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT NULL, "
    "adjective TEXT NOT NULL, camp INTEGER NOT NULL, lifespan_years INTEGER NOT NULL, generation_years INTEGER NOT "
    "NULL, culture INTEGER NOT NULL, present INTEGER NOT NULL, awakening_year INTEGER, creator_god INTEGER NOT NULL) "
    "STRICT";
inline constexpr std::string_view DDL_RACE_BIOME =
    "CREATE TABLE race_biome (race INTEGER NOT NULL, biome INTEGER NOT NULL, weight INTEGER NOT NULL, "
    "PRIMARY KEY (race, biome)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_CULTURE =
    "CREATE TABLE culture (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, race INTEGER NOT "
    "NULL) STRICT";
inline constexpr std::string_view DDL_GOD =
    "CREATE TABLE god (id INTEGER PRIMARY KEY, name TEXT NOT NULL, camp INTEGER NOT NULL, domain INTEGER NOT NULL, "
    "title TEXT NOT NULL, race INTEGER NOT NULL) STRICT";  // schema 14 (P5-08 9): domain, title, race
inline constexpr std::string_view DDL_AGE =
    "CREATE TABLE age (id INTEGER PRIMARY KEY, race INTEGER NOT NULL, start_year INTEGER NOT NULL, end_year INTEGER "
    "NOT NULL, name TEXT NOT NULL) STRICT";

// The names of the five chronology tables (P3-03 5.1).
inline constexpr std::string_view RACE_TABLE = "race";
inline constexpr std::string_view RACE_BIOME_TABLE = "race_biome";
inline constexpr std::string_view CULTURE_TABLE = "culture";
inline constexpr std::string_view GOD_TABLE = "god";
inline constexpr std::string_view AGE_TABLE = "age";

// DDL of the history tables (P3-04 9.1, schema version 7), executed
// fourteenth to twenty-sixth: written by the history stage, empty when it
// did not run. The event catalogue (event_kind, event_kind_role,
// event_kind_value), the phases and the event log (phase, event,
// event_participant, event_cause, event_value), the entities at the end of
// the run (civilization, settlement) and the by-year tables
// (settlement_census, region_control, relation). Nullable columns:
// civilization.ended_year, settlement.ruined_year, region_control.to_year,
// relation.until.
inline constexpr std::string_view DDL_EVENT_KIND =
    "CREATE TABLE event_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, caused INTEGER NOT NULL, region_use "
    "INTEGER NOT NULL, settlement_use INTEGER NOT NULL, salience INTEGER NOT NULL, template TEXT NOT NULL) STRICT";
inline constexpr std::string_view DDL_EVENT_KIND_ROLE =
    "CREATE TABLE event_kind_role (kind INTEGER NOT NULL, seq INTEGER NOT NULL, role TEXT NOT NULL, entity_type "
    "INTEGER NOT NULL, min_count INTEGER NOT NULL, max_count INTEGER NOT NULL, PRIMARY KEY (kind, seq)) STRICT, "
    "WITHOUT ROWID";
inline constexpr std::string_view DDL_EVENT_KIND_VALUE =
    "CREATE TABLE event_kind_value (kind INTEGER NOT NULL, seq INTEGER NOT NULL, key TEXT NOT NULL, PRIMARY KEY "
    "(kind, seq)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_PHASE =
    "CREATE TABLE phase (id INTEGER PRIMARY KEY, age INTEGER NOT NULL, kind INTEGER NOT NULL, start_year INTEGER NOT "
    "NULL, end_year INTEGER NOT NULL, trigger_event INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_EVENT =
    "CREATE TABLE event (id INTEGER PRIMARY KEY, year INTEGER NOT NULL, end_year INTEGER NOT NULL, kind INTEGER NOT "
    "NULL, phase INTEGER NOT NULL, region INTEGER NOT NULL, settlement INTEGER NOT NULL, salience INTEGER NOT NULL) "
    "STRICT";
inline constexpr std::string_view DDL_EVENT_PARTICIPANT =
    "CREATE TABLE event_participant (event INTEGER NOT NULL, seq INTEGER NOT NULL, role INTEGER NOT NULL, "
    "entity_type INTEGER NOT NULL, entity_id INTEGER NOT NULL, PRIMARY KEY (event, seq)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_EVENT_CAUSE =
    "CREATE TABLE event_cause (event INTEGER NOT NULL, cause INTEGER NOT NULL, PRIMARY KEY (event, cause)) STRICT, "
    "WITHOUT ROWID";
inline constexpr std::string_view DDL_EVENT_VALUE =
    "CREATE TABLE event_value (event INTEGER NOT NULL, seq INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY "
    "(event, seq)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_CIVILIZATION =
    "CREATE TABLE civilization (id INTEGER PRIMARY KEY, name TEXT NOT NULL, race INTEGER NOT NULL, culture INTEGER "
    "NOT NULL, home_region INTEGER NOT NULL, capital INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event "
    "INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_SETTLEMENT =
    "CREATE TABLE settlement (id INTEGER PRIMARY KEY, name TEXT NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, "
    "region INTEGER NOT NULL, founder_civ INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event INTEGER NOT "
    "NULL, owner_civ INTEGER NOT NULL, kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT NULL, "
    "ruined_year INTEGER, ruin_event INTEGER NOT NULL, ending INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_SETTLEMENT_CENSUS =
    "CREATE TABLE settlement_census (settlement INTEGER NOT NULL, year INTEGER NOT NULL, owner_civ INTEGER NOT NULL, "
    "kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT NULL, PRIMARY KEY (settlement, year)) "
    "STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_REGION_CONTROL =
    "CREATE TABLE region_control (region INTEGER NOT NULL, from_year INTEGER NOT NULL, to_year INTEGER, civ INTEGER "
    "NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_RELATION =
    "CREATE TABLE relation (a_type INTEGER NOT NULL, a_id INTEGER NOT NULL, b_type INTEGER NOT NULL, b_id INTEGER NOT "
    "NULL, kind TEXT NOT NULL, since INTEGER NOT NULL, until INTEGER, since_event INTEGER NOT NULL, until_event "
    "INTEGER NOT NULL, PRIMARY KEY (a_type, a_id, b_type, b_id, kind, since)) STRICT, WITHOUT ROWID";

// The names of the thirteen history tables (P3-04 9.1), in DDL order.
inline constexpr std::string_view EVENT_KIND_TABLE = "event_kind";
inline constexpr std::string_view EVENT_KIND_ROLE_TABLE = "event_kind_role";
inline constexpr std::string_view EVENT_KIND_VALUE_TABLE = "event_kind_value";
inline constexpr std::string_view PHASE_TABLE = "phase";
inline constexpr std::string_view EVENT_TABLE = "event";
inline constexpr std::string_view EVENT_PARTICIPANT_TABLE = "event_participant";
inline constexpr std::string_view EVENT_CAUSE_TABLE = "event_cause";
inline constexpr std::string_view EVENT_VALUE_TABLE = "event_value";
inline constexpr std::string_view CIVILIZATION_TABLE = "civilization";
inline constexpr std::string_view SETTLEMENT_TABLE = "settlement";
inline constexpr std::string_view SETTLEMENT_CENSUS_TABLE = "settlement_census";
inline constexpr std::string_view REGION_CONTROL_TABLE = "region_control";
inline constexpr std::string_view RELATION_TABLE = "relation";
inline constexpr std::array<std::string_view, 13> HISTORY_TABLES = {
    EVENT_KIND_TABLE,        EVENT_KIND_ROLE_TABLE,   EVENT_KIND_VALUE_TABLE, PHASE_TABLE,
    EVENT_TABLE,             EVENT_PARTICIPANT_TABLE, EVENT_CAUSE_TABLE,      EVENT_VALUE_TABLE,
    CIVILIZATION_TABLE,      SETTLEMENT_TABLE,        SETTLEMENT_CENSUS_TABLE, REGION_CONTROL_TABLE,
    RELATION_TABLE};

// DDL of table region_name (P3-04 r21 23.4, P5-12 7.2, schema version 8),
// executed twenty-seventh, after relation: one row per renaming of a region
// by history, written by the history stage in (region, from_year) order,
// empty when it did not run. region.name is never rewritten.
inline constexpr std::string_view DDL_REGION_NAME =
    "CREATE TABLE region_name (region INTEGER NOT NULL, from_year INTEGER NOT NULL, name TEXT NOT NULL, culture "
    "INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view REGION_NAME_TABLE = "region_name";

// DDL of the tables of P5-02 11.1 (schema version 9), executed after
// region_name, in this order: the beast and monster kinds of the data, the
// beast counts by region and year, the monsters and the sites, written by
// the history stage, empty when it did not run. Nullable columns:
// monster.died, site.ended_year.
inline constexpr std::string_view DDL_BEAST =
    "CREATE TABLE beast (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT NULL, "
    "menace INTEGER NOT NULL, food INTEGER NOT NULL, bold_phrases TEXT NOT NULL) STRICT";
inline constexpr std::string_view DDL_MONSTER_KIND =
    "CREATE TABLE monster_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, noun TEXT NOT "
    "NULL, grouped INTEGER NOT NULL, wake INTEGER NOT NULL, might INTEGER NOT NULL, raid_phrases TEXT NOT NULL) "
    "STRICT";
inline constexpr std::string_view DDL_BEAST_POPULATION =
    "CREATE TABLE beast_population (region INTEGER NOT NULL, beast INTEGER NOT NULL, year INTEGER NOT NULL, count "
    "INTEGER NOT NULL, PRIMARY KEY (region, beast, year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_MONSTER =
    "CREATE TABLE monster (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, lair_site INTEGER NOT "
    "NULL, born INTEGER NOT NULL, born_event INTEGER NOT NULL, died INTEGER, death_event INTEGER NOT NULL, slain_by "
    "INTEGER NOT NULL, status INTEGER NOT NULL, hoard INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_SITE =
    "CREATE TABLE site (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, region INTEGER NOT NULL, x "
    "INTEGER NOT NULL, y INTEGER NOT NULL, settlement INTEGER NOT NULL, founder_civ INTEGER NOT NULL, monster INTEGER "
    "NOT NULL, origin_year INTEGER NOT NULL, origin_event INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER "
    "NOT NULL, ending INTEGER NOT NULL) STRICT";
inline constexpr std::string_view BEAST_TABLE = "beast";
inline constexpr std::string_view MONSTER_KIND_TABLE = "monster_kind";
inline constexpr std::string_view BEAST_POPULATION_TABLE = "beast_population";
inline constexpr std::string_view MONSTER_TABLE = "monster";
inline constexpr std::string_view SITE_TABLE = "site";

// DDL of the tables of P5-03 16.1 (schema version 10), executed after site,
// in this order: the dispositions of the race pairs by year and the shares
// and standings of the races of each settlement by year, written by the
// history stage, empty when it did not run.
inline constexpr std::string_view DDL_RACE_RELATION =
    "CREATE TABLE race_relation (race_a INTEGER NOT NULL, race_b INTEGER NOT NULL, year INTEGER NOT NULL, "
    "disposition INTEGER NOT NULL, band INTEGER NOT NULL, PRIMARY KEY (race_a, race_b, year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_SETTLEMENT_RACE =
    "CREATE TABLE settlement_race (settlement INTEGER NOT NULL, race INTEGER NOT NULL, year INTEGER NOT NULL, share "
    "INTEGER NOT NULL, standing INTEGER NOT NULL, PRIMARY KEY (settlement, race, year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view RACE_RELATION_TABLE = "race_relation";
inline constexpr std::string_view SETTLEMENT_RACE_TABLE = "settlement_race";

// DDL of the tables of P5-06 9.1 and 9.3 (schema version 12), executed after
// settlement_race, in this order: the named figures and the books, written
// by the history stage, empty when it did not run. Nullable column:
// figure.died.
inline constexpr std::string_view DDL_FIGURE =
    "CREATE TABLE figure (id INTEGER PRIMARY KEY, name TEXT NOT NULL, epithet TEXT NOT NULL, role INTEGER NOT NULL, "
    "race INTEGER NOT NULL, culture INTEGER NOT NULL, civ INTEGER NOT NULL, born INTEGER NOT NULL, office_from "
    "INTEGER NOT NULL, start_event INTEGER NOT NULL, died INTEGER, death_event INTEGER NOT NULL, death_region INTEGER "
    "NOT NULL, death_settlement INTEGER NOT NULL, status INTEGER NOT NULL, reign_no INTEGER NOT NULL, regnal INTEGER "
    "NOT NULL, temper INTEGER NOT NULL, skill INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_BOOK =
    "CREATE TABLE book (id INTEGER PRIMARY KEY, title TEXT NOT NULL, author INTEGER NOT NULL, site INTEGER NOT NULL, "
    "year INTEGER NOT NULL, event INTEGER NOT NULL) STRICT";
inline constexpr std::string_view FIGURE_TABLE = "figure";
inline constexpr std::string_view BOOK_TABLE = "book";

// DDL of table faction (P5-08 9, schema version 14), executed after book:
// the cults (kind 1), written by the history stage in ascending id, empty
// when it did not run. Nullable column: ended_year.
inline constexpr std::string_view DDL_FACTION =
    "CREATE TABLE faction (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, civ INTEGER NOT NULL, "
    "god INTEGER NOT NULL, founder INTEGER NOT NULL, seat INTEGER NOT NULL, founded_year INTEGER NOT NULL, "
    "founding_event INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER NOT NULL, ending INTEGER NOT NULL, "
    "status INTEGER NOT NULL) STRICT";
inline constexpr std::string_view FACTION_TABLE = "faction";

// The DDL statements in execution order (P0-02 5.4, section 6 step 2;
// schema 7: meta, layer, pass, biome, climate, region, region_border,
// region_biome, race, race_biome, culture, god, age, then the thirteen
// history tables of P3-04 9.1; schema 8: then region_name; schema 9: then
// beast, monster_kind, beast_population, monster, site; schema 10: then
// race_relation, settlement_race; schema 12: then figure, book; schema 14:
// then faction).
inline constexpr std::array<std::string_view, 37> DDL_STATEMENTS = {
    DDL_META,           DDL_LAYER,           DDL_PASS,
    DDL_BIOME,          DDL_CLIMATE,         DDL_REGION,
    DDL_REGION_BORDER,  DDL_REGION_BIOME,    DDL_RACE,
    DDL_RACE_BIOME,     DDL_CULTURE,         DDL_GOD,
    DDL_AGE,            DDL_EVENT_KIND,      DDL_EVENT_KIND_ROLE,
    DDL_EVENT_KIND_VALUE, DDL_PHASE,         DDL_EVENT,
    DDL_EVENT_PARTICIPANT, DDL_EVENT_CAUSE,  DDL_EVENT_VALUE,
    DDL_CIVILIZATION,   DDL_SETTLEMENT,      DDL_SETTLEMENT_CENSUS,
    DDL_REGION_CONTROL, DDL_RELATION,        DDL_REGION_NAME,
    DDL_BEAST,          DDL_MONSTER_KIND,    DDL_BEAST_POPULATION,
    DDL_MONSTER,        DDL_SITE,            DDL_RACE_RELATION,
    DDL_SETTLEMENT_RACE, DDL_FIGURE,         DDL_BOOK,
    DDL_FACTION};


// --- meta keys (P0-02 5.5) --------------------------------------------------

inline constexpr std::string_view META_CONTENT_HASH = "content_hash";        // 64 lowercase hex digits
inline constexpr std::string_view META_FORMAT = "format";                    // literal WORLD_FORMAT
inline constexpr std::string_view META_GENERATOR_VERSION = "generator_version";  // MAJOR.MINOR.PATCH
inline constexpr std::string_view META_HISTORY_END_YEAR = "history_end_year";  // = param.history_span (P3-04 9.4)
inline constexpr std::string_view META_PARAM_CIVS_PER_RACE = "param.civs_per_race";      // auto, 1 ... 4
inline constexpr std::string_view META_PARAM_DIVINE_ACTIVITY = "param.divine_activity";  // distant, normal, meddling
inline constexpr std::string_view META_PARAM_DIVINE_SCRIPT = "param.divine_script";      // empty or 64 lower-case hex
inline constexpr std::string_view META_PARAM_EVENT_INTENSITY = "param.event_intensity";  // calm, normal, turbulent
inline constexpr std::string_view META_PARAM_HISTORY_DETAIL = "param.history_detail";    // sparse, normal, dense
inline constexpr std::string_view META_PARAM_HISTORY_SPAN = "param.history_span";        // 2000 ... 12000
inline constexpr std::string_view META_PARAM_MAP_SIZE = "param.map_size";    // 512, 1024 or 2048
inline constexpr std::string_view META_PARAM_MONSTER_PRESSURE = "param.monster_pressure";  // low, normal, high
inline constexpr std::string_view META_PARAM_RACES = "param.races";          // race ids joined by ','
inline constexpr std::string_view META_SCHEMA_VERSION = "schema_version";    // decimal, = user_version
inline constexpr std::string_view META_SEED = "seed";                        // unsigned decimal
inline constexpr std::string_view META_SEED_PHRASE = "seed_phrase";          // scrubbed phrase or empty
inline constexpr std::string_view META_STAGES = "stages";                    // comma-separated stage names

// Prefix of every parameter key (P0-02 5.5, "Rules for parameters").
inline constexpr std::string_view META_PARAM_PREFIX = "param.";

// Value of meta.format (P0-02 5.5).
inline constexpr std::string_view WORLD_FORMAT = "the-unmapped-world";

// The map sizes allowed for param.map_size (P0-02 5.5, P0-03 6).
inline constexpr std::array<std::int64_t, 3> ALLOWED_MAP_SIZES = {512, 1024, 2048};

// Every key required at SCHEMA_VERSION, in ascending byte order, which is
// the order the writer inserts them (P0-02 5.5, section 6 step 4).
inline constexpr std::array<std::string_view, 17> REQUIRED_META_KEYS = {
    META_CONTENT_HASH,
    META_FORMAT,
    META_GENERATOR_VERSION,
    META_HISTORY_END_YEAR,
    META_PARAM_CIVS_PER_RACE,
    META_PARAM_DIVINE_ACTIVITY,
    META_PARAM_DIVINE_SCRIPT,
    META_PARAM_EVENT_INTENSITY,
    META_PARAM_HISTORY_DETAIL,
    META_PARAM_HISTORY_SPAN,
    META_PARAM_MAP_SIZE,
    META_PARAM_MONSTER_PRESSURE,
    META_PARAM_RACES,
    META_SCHEMA_VERSION,
    META_SEED,
    META_SEED_PHRASE,
    META_STAGES,
};

// The eight history settings keys of P3-03 4.2, in ascending byte order
// (also the order of history::history_meta_entries).
inline constexpr std::array<std::string_view, 8> HISTORY_META_KEYS = {
    META_PARAM_CIVS_PER_RACE, META_PARAM_DIVINE_ACTIVITY, META_PARAM_DIVINE_SCRIPT,   META_PARAM_EVENT_INTENSITY,
    META_PARAM_HISTORY_DETAIL, META_PARAM_HISTORY_SPAN,  META_PARAM_MONSTER_PRESSURE, META_PARAM_RACES,
};

// True if key is one of HISTORY_META_KEYS.
[[nodiscard]] bool is_history_meta_key(std::string_view key) noexcept;

// True if value is the canonical text of history key `key` (P3-03 4.2):
//   param.civs_per_race     "auto", "1", "2", "3" or "4"
//   param.divine_activity   "distant", "normal" or "meddling"
//   param.divine_script     empty, or 64 lower-case hex digits (the P5 form)
//   param.event_intensity   "calm", "normal" or "turbulent"
//   param.history_detail    "sparse", "normal" or "dense"
//   param.history_span      canonical decimal 2000 ... 12000
//   param.monster_pressure  "low", "normal" or "high"
//   param.races             one or more ids of the P0-03 definition id
//                           grammar joined by ',', none twice
// False for any other key.
[[nodiscard]] bool is_canonical_history_meta_value(std::string_view key, std::string_view value);

// True if key matches [a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)* (P0-02 5.5). No
// length limit is specified for meta keys.
[[nodiscard]] bool is_valid_meta_key(std::string_view key) noexcept;

// True if name is a valid stage name: the stream-name grammar without dots,
// [a-z][a-z0-9_]*, 1 to 64 bytes (P0-02 5.5, key "stages"; P0-01 7.3).
[[nodiscard]] bool is_valid_stage_name(std::string_view name) noexcept;

// True if name is a valid layer name: the stream-name grammar of P0-01 7.3,
// 1 to 64 bytes (P0-02 5.6).
[[nodiscard]] bool is_valid_layer_name(std::string_view name) noexcept;

// --- Canonical forms of meta values (P0-02 5.5) -----------------------------

// Canonical decimal text of an integer: "0", or [1-9][0-9]* with a leading
// "-" for negative values (P0-02 5.5: seed, schema_version, integer param.*).
[[nodiscard]] std::string format_u64(std::uint64_t value);
[[nodiscard]] std::string format_i64(std::int64_t value);

// Parses canonical unsigned decimal text ("0" or [1-9][0-9]*, at most
// 2^64 - 1); std::nullopt for anything else (sign, leading zero, empty,
// overflow, any other character) (P0-02 5.5, key "seed").
[[nodiscard]] std::optional<std::uint64_t> parse_canonical_u64(std::string_view text) noexcept;

// True if text is a canonical generator version: MAJOR.MINOR.PATCH, each
// "0" or [1-9][0-9]* (P0-02 5.5).
[[nodiscard]] bool is_canonical_version(std::string_view text) noexcept;

}  // namespace worldgen::db
