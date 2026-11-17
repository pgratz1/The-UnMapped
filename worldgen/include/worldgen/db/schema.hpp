#pragma once

// World database schema, version 3 (spec P0-02 sections 4.2, 5;
// docs/specs/schema.md; table `pass` from P1-01 section 9.1, schema 2;
// tables `biome` and `climate` from P1-04 section 3 and P1-03 section 3,
// schema 3).
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
inline constexpr std::int64_t SCHEMA_VERSION = 3;

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

// DDL of the climate table (P1-03 3, schema version 3), executed fifth: one
// row (id 1) written by the climate stage.
inline constexpr std::string_view DDL_CLIMATE =
    "CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL) STRICT";

// The names of the biome and climate tables (P1-04 3, P1-03 3).
inline constexpr std::string_view BIOME_TABLE = "biome";
inline constexpr std::string_view CLIMATE_TABLE = "climate";

// The DDL statements in execution order (P0-02 5.4, section 6 step 2;
// schema 3: meta, layer, pass, biome, climate).
inline constexpr std::array<std::string_view, 5> DDL_STATEMENTS = {DDL_META, DDL_LAYER, DDL_PASS, DDL_BIOME,
                                                                   DDL_CLIMATE};

// --- meta keys (P0-02 5.5) --------------------------------------------------

inline constexpr std::string_view META_CONTENT_HASH = "content_hash";        // 64 lowercase hex digits
inline constexpr std::string_view META_FORMAT = "format";                    // literal WORLD_FORMAT
inline constexpr std::string_view META_GENERATOR_VERSION = "generator_version";  // MAJOR.MINOR.PATCH
inline constexpr std::string_view META_PARAM_MAP_SIZE = "param.map_size";    // 512, 1024 or 2048
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
inline constexpr std::array<std::string_view, 8> REQUIRED_META_KEYS = {
    META_CONTENT_HASH, META_FORMAT,     META_GENERATOR_VERSION, META_PARAM_MAP_SIZE,
    META_SCHEMA_VERSION, META_SEED, META_SEED_PHRASE, META_STAGES,
};

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
