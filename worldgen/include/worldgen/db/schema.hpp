#pragma once

// World database schema, version 6 (spec P0-02 sections 4.2, 5;
// docs/specs/schema.md; table `pass` from P1-01 section 9.1, schema 2;
// tables `biome` and `climate` from P1-04 section 3 and P1-03 section 3,
// schema 3; column climate.temp_offset_dc, schema 4, P0-02 amendment 16;
// tables `region`, `region_border` and `region_biome` from P1-06 section 3,
// schema 5, P0-02 amendment 17; tables `race`, `race_biome`, `culture`,
// `god` and `age` and the eight history `meta` keys from P3-03 sections 4.2
// and 5, schema 6, P0-02 amendment 18).
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
inline constexpr std::int64_t SCHEMA_VERSION = 6;

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
    "CREATE TABLE god (id INTEGER PRIMARY KEY, name TEXT NOT NULL, camp INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_AGE =
    "CREATE TABLE age (id INTEGER PRIMARY KEY, race INTEGER NOT NULL, start_year INTEGER NOT NULL, end_year INTEGER "
    "NOT NULL, name TEXT NOT NULL) STRICT";

// The names of the five chronology tables (P3-03 5.1).
inline constexpr std::string_view RACE_TABLE = "race";
inline constexpr std::string_view RACE_BIOME_TABLE = "race_biome";
inline constexpr std::string_view CULTURE_TABLE = "culture";
inline constexpr std::string_view GOD_TABLE = "god";
inline constexpr std::string_view AGE_TABLE = "age";

// The DDL statements in execution order (P0-02 5.4, section 6 step 2;
// schema 6: meta, layer, pass, biome, climate, region, region_border,
// region_biome, race, race_biome, culture, god, age).
inline constexpr std::array<std::string_view, 13> DDL_STATEMENTS = {
    DDL_META,       DDL_LAYER,         DDL_PASS,         DDL_BIOME, DDL_CLIMATE,
    DDL_REGION,     DDL_REGION_BORDER, DDL_REGION_BIOME, DDL_RACE,  DDL_RACE_BIOME,
    DDL_CULTURE,    DDL_GOD,           DDL_AGE};

// --- meta keys (P0-02 5.5) --------------------------------------------------

inline constexpr std::string_view META_CONTENT_HASH = "content_hash";        // 64 lowercase hex digits
inline constexpr std::string_view META_FORMAT = "format";                    // literal WORLD_FORMAT
inline constexpr std::string_view META_GENERATOR_VERSION = "generator_version";  // MAJOR.MINOR.PATCH
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
inline constexpr std::array<std::string_view, 16> REQUIRED_META_KEYS = {
    META_CONTENT_HASH,
    META_FORMAT,
    META_GENERATOR_VERSION,
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
