#pragma once

// Reference values for the P0-04 tests (spec P0-04 sections 5.2, 7, 8),
// built by the tests from the spec text and the core functions, never from
// the P0-04 code under test. Shared by worldgen-tests (library tests) and
// worldgen-cli-tests (integration tests).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/checked.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/noise.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/layer_blob.hpp"

namespace p004 {

inline constexpr std::uint32_t SIDE = 256;
inline constexpr std::size_t CELLS = static_cast<std::size_t>(SIDE) * SIDE;

// The DDL of P0-02 5.4, copied from the spec text.
inline constexpr std::string_view DDL_META =
    "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_LAYER =
    "CREATE TABLE layer (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, width INTEGER NOT NULL, height INTEGER "
    "NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL) STRICT";
// The DDL of table `pass` (schema version 2), copied from P1-01 9.1.
inline constexpr std::string_view DDL_PASS =
    "CREATE TABLE pass (id INTEGER PRIMARY KEY, range_id INTEGER NOT NULL, kind INTEGER NOT NULL, capped INTEGER NOT "
    "NULL, col_x INTEGER NOT NULL, col_y INTEGER NOT NULL, saddle_dm INTEGER NOT NULL, col_dm INTEGER NOT NULL, path "
    "BLOB NOT NULL) STRICT";
// The DDL of tables `biome` and `climate` (schema version 4), copied from
// P1-04 3 and P1-03 3.
inline constexpr std::string_view DDL_BIOME =
    "CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, colour INTEGER NOT "
    "NULL, water INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_CLIMATE =
    "CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL, temp_offset_dc INTEGER NOT NULL) "
    "STRICT";
// The DDL of tables `region`, `region_border` and `region_biome` (schema
// version 5), copied from P1-06 3.1.
inline constexpr std::string_view DDL_REGION =
    "CREATE TABLE region (id INTEGER PRIMARY KEY, name TEXT NOT NULL, biome INTEGER NOT NULL, x_min INTEGER NOT NULL, "
    "y_min INTEGER NOT NULL, x_max INTEGER NOT NULL, y_max INTEGER NOT NULL, seed_x INTEGER NOT NULL, seed_y INTEGER "
    "NOT NULL, landmass INTEGER NOT NULL, cells INTEGER NOT NULL, lake_cells INTEGER NOT NULL, river_cells INTEGER NOT "
    "NULL, river_class_max INTEGER NOT NULL, coast_cells INTEGER NOT NULL, walk_cells INTEGER NOT NULL, walk_area "
    "INTEGER NOT NULL, elev_mean_dm INTEGER NOT NULL, elev_max_dm INTEGER NOT NULL, fertility_mean INTEGER NOT NULL, "
    "forage_mean INTEGER NOT NULL, timber_mean INTEGER NOT NULL, ore_mean INTEGER NOT NULL, fresh_water_mean INTEGER "
    "NOT NULL, habitability_mean INTEGER NOT NULL, habitability_p90 INTEGER NOT NULL, ore_rich_cells INTEGER NOT NULL, "
    "ore_reach_cells INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_REGION_BORDER =
    "CREATE TABLE region_border (a INTEGER NOT NULL, b INTEGER NOT NULL, length_m INTEGER NOT NULL, low_dm INTEGER NOT "
    "NULL, low_x INTEGER NOT NULL, low_y INTEGER NOT NULL, walk_pairs INTEGER NOT NULL, river_pairs INTEGER NOT NULL, "
    "river_class_max INTEGER NOT NULL, pass_id INTEGER NOT NULL, passes INTEGER NOT NULL, PRIMARY KEY (a, b)) STRICT, "
    "WITHOUT ROWID";
inline constexpr std::string_view DDL_REGION_BIOME =
    "CREATE TABLE region_biome (region INTEGER NOT NULL, biome INTEGER NOT NULL, cells INTEGER NOT NULL, PRIMARY KEY "
    "(region, biome)) STRICT, WITHOUT ROWID";
// The DDL of tables `race`, `race_biome`, `culture`, `god` and `age`
// (schema version 6), copied from P3-03 5.1.
inline constexpr std::string_view DDL_RACE =
    "CREATE TABLE race (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT"
    " NOT NULL, adjective TEXT NOT NULL, camp INTEGER NOT NULL, lifespan_years INTEGER NOT NULL, generati"
    "on_years INTEGER NOT NULL, culture INTEGER NOT NULL, present INTEGER NOT NULL, awakening_year INTEGE"
    "R, creator_god INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_RACE_BIOME =
    "CREATE TABLE race_biome (race INTEGER NOT NULL, biome INTEGER NOT NULL, weight INTEGER NOT NULL, PRI"
    "MARY KEY (race, biome)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_CULTURE =
    "CREATE TABLE culture (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, race INT"
    "EGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_GOD =
    "CREATE TABLE god (id INTEGER PRIMARY KEY, name TEXT NOT NULL, camp INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_AGE =
    "CREATE TABLE age (id INTEGER PRIMARY KEY, race INTEGER NOT NULL, start_year INTEGER NOT NULL, end_ye"
    "ar INTEGER NOT NULL, name TEXT NOT NULL) STRICT";
// The DDL of the thirteen history tables (schema version 7), copied from
// P3-04 9.1.
inline constexpr std::string_view DDL_EVENT_KIND =
    "CREATE TABLE event_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, caused INTEGER NOT NULL, "
    "region_use INTEGER NOT NULL, settlement_use INTEGER NOT NULL, salience INTEGER NOT NULL, template TE"
    "XT NOT NULL) STRICT";
inline constexpr std::string_view DDL_EVENT_KIND_ROLE =
    "CREATE TABLE event_kind_role (kind INTEGER NOT NULL, seq INTEGER NOT NULL, role TEXT NOT NULL, entit"
    "y_type INTEGER NOT NULL, min_count INTEGER NOT NULL, max_count INTEGER NOT NULL, PRIMARY KEY (kind, "
    "seq)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_EVENT_KIND_VALUE =
    "CREATE TABLE event_kind_value (kind INTEGER NOT NULL, seq INTEGER NOT NULL, key TEXT NOT NULL, PRIMA"
    "RY KEY (kind, seq)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_PHASE =
    "CREATE TABLE phase (id INTEGER PRIMARY KEY, age INTEGER NOT NULL, kind INTEGER NOT NULL, start_year "
    "INTEGER NOT NULL, end_year INTEGER NOT NULL, trigger_event INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_EVENT =
    "CREATE TABLE event (id INTEGER PRIMARY KEY, year INTEGER NOT NULL, end_year INTEGER NOT NULL, kind I"
    "NTEGER NOT NULL, phase INTEGER NOT NULL, region INTEGER NOT NULL, settlement INTEGER NOT NULL, salie"
    "nce INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_EVENT_PARTICIPANT =
    "CREATE TABLE event_participant (event INTEGER NOT NULL, seq INTEGER NOT NULL, role INTEGER NOT NULL,"
    " entity_type INTEGER NOT NULL, entity_id INTEGER NOT NULL, PRIMARY KEY (event, seq)) STRICT, WITHOUT"
    " ROWID";
inline constexpr std::string_view DDL_EVENT_CAUSE =
    "CREATE TABLE event_cause (event INTEGER NOT NULL, cause INTEGER NOT NULL, PRIMARY KEY (event, cause)"
    ") STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_EVENT_VALUE =
    "CREATE TABLE event_value (event INTEGER NOT NULL, seq INTEGER NOT NULL, value INTEGER NOT NULL, PRIM"
    "ARY KEY (event, seq)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_CIVILIZATION =
    "CREATE TABLE civilization (id INTEGER PRIMARY KEY, name TEXT NOT NULL, race INTEGER NOT NULL, cultur"
    "e INTEGER NOT NULL, home_region INTEGER NOT NULL, capital INTEGER NOT NULL, founded_year INTEGER NOT"
    " NULL, founding_event INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_SETTLEMENT =
    "CREATE TABLE settlement (id INTEGER PRIMARY KEY, name TEXT NOT NULL, x INTEGER NOT NULL, y INTEGER N"
    "OT NULL, region INTEGER NOT NULL, founder_civ INTEGER NOT NULL, founded_year INTEGER NOT NULL, found"
    "ing_event INTEGER NOT NULL, owner_civ INTEGER NOT NULL, kind INTEGER NOT NULL, status INTEGER NOT NU"
    "LL, population INTEGER NOT NULL, ruined_year INTEGER, ruin_event INTEGER NOT NULL, ending INTEGER NOT NULL) "
    "STRICT";   // P5-02 11.1 (schema 9): the last column ending
inline constexpr std::string_view DDL_SETTLEMENT_CENSUS =
    "CREATE TABLE settlement_census (settlement INTEGER NOT NULL, year INTEGER NOT NULL, owner_civ INTEGE"
    "R NOT NULL, kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT NULL, PRIMARY KEY"
    " (settlement, year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_REGION_CONTROL =
    "CREATE TABLE region_control (region INTEGER NOT NULL, from_year INTEGER NOT NULL, to_year INTEGER, c"
    "iv INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_RELATION =
    "CREATE TABLE relation (a_type INTEGER NOT NULL, a_id INTEGER NOT NULL, b_type INTEGER NOT NULL, b_id"
    " INTEGER NOT NULL, kind TEXT NOT NULL, since INTEGER NOT NULL, until INTEGER, since_event INTEGER NO"
    "T NULL, until_event INTEGER NOT NULL, PRIMARY KEY (a_type, a_id, b_type, b_id, kind, since)) STRICT,"
    " WITHOUT ROWID";
// Table region_name (schema 8, P5-12 7.2; docs/specs/schema.md).
inline constexpr std::string_view DDL_REGION_NAME =
    "CREATE TABLE region_name (region INTEGER NOT NULL, from_year INTEGER NOT NULL, name TEXT NOT NULL, cultur"
    "e INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID";
// The five tables of P5-02 11.1 (schema 9), byte for byte.
inline constexpr std::string_view DDL_BEAST =
    "CREATE TABLE beast (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT NULL, "
    "menace INTEGER NOT NULL, food INTEGER NOT NULL, bold_phrases TEXT NOT NULL) STRICT";   // P5-03 16.1
inline constexpr std::string_view DDL_MONSTER_KIND =
    "CREATE TABLE monster_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, noun TEXT NOT "
    "NULL, grouped INTEGER NOT NULL, wake INTEGER NOT NULL, might INTEGER NOT NULL, raid_phrases TEXT NOT NULL) "
    "STRICT";   // P5-03 16.1
inline constexpr std::string_view DDL_BEAST_POPULATION =
    "CREATE TABLE beast_population (region INTEGER NOT NULL, beast INTEGER NOT NULL, year INTEGER NOT NULL, count "
    "INTEGER NOT NULL, PRIMARY KEY (region, beast, year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_MONSTER =
    "CREATE TABLE monster (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, lair_site INTEGER NOT "
    "NULL, born INTEGER NOT NULL, born_event INTEGER NOT NULL, died INTEGER, death_event INTEGER NOT NULL, slain_by "
    "INTEGER NOT NULL, status INTEGER NOT NULL, hoard INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_SITE =
    "CREATE TABLE site (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, region INTEGER NOT NULL, "
    "x INTEGER NOT NULL, y INTEGER NOT NULL, settlement INTEGER NOT NULL, founder_civ INTEGER NOT NULL, monster "
    "INTEGER NOT NULL, origin_year INTEGER NOT NULL, origin_event INTEGER NOT NULL, ended_year INTEGER, ending_event "
    "INTEGER NOT NULL, ending INTEGER NOT NULL) STRICT";
// The two tables of P5-03 16.1 (schema 10), byte for byte.
inline constexpr std::string_view DDL_RACE_RELATION =
    "CREATE TABLE race_relation (race_a INTEGER NOT NULL, race_b INTEGER NOT NULL, year INTEGER NOT NULL, disposition "
    "INTEGER NOT NULL, band INTEGER NOT NULL, PRIMARY KEY (race_a, race_b, year)) STRICT, WITHOUT ROWID";
inline constexpr std::string_view DDL_SETTLEMENT_RACE =
    "CREATE TABLE settlement_race (settlement INTEGER NOT NULL, race INTEGER NOT NULL, year INTEGER NOT NULL, share "
    "INTEGER NOT NULL, standing INTEGER NOT NULL, PRIMARY KEY (settlement, race, year)) STRICT, WITHOUT ROWID";
// The two tables of P5-06 9.1 and 9.3 (schema 12), byte for byte.
inline constexpr std::string_view DDL_FIGURE =
    "CREATE TABLE figure (id INTEGER PRIMARY KEY, name TEXT NOT NULL, epithet TEXT NOT NULL, role INTEGER NOT NULL, "
    "race INTEGER NOT NULL, culture INTEGER NOT NULL, civ INTEGER NOT NULL, born INTEGER NOT NULL, office_from "
    "INTEGER NOT NULL, start_event INTEGER NOT NULL, died INTEGER, death_event INTEGER NOT NULL, death_region INTEGER "
    "NOT NULL, death_settlement INTEGER NOT NULL, status INTEGER NOT NULL, reign_no INTEGER NOT NULL, regnal INTEGER "
    "NOT NULL, temper INTEGER NOT NULL, skill INTEGER NOT NULL) STRICT";
inline constexpr std::string_view DDL_BOOK =
    "CREATE TABLE book (id INTEGER PRIMARY KEY, title TEXT NOT NULL, author INTEGER NOT NULL, site INTEGER NOT NULL, "
    "year INTEGER NOT NULL, event INTEGER NOT NULL) STRICT";
inline constexpr std::string_view EMPTY_CONTENT_HASH =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

// --- 5.2 test pattern, computed cell by cell from the formulas ---------------

// test.ramp: (x + 2y) mod 256.
inline std::vector<std::uint8_t> ramp_cells() {
    std::vector<std::uint8_t> v(CELLS);
    for (std::uint32_t y = 0; y < SIDE; ++y) {
        for (std::uint32_t x = 0; x < SIDE; ++x) {
            v[y * SIDE + x] = static_cast<std::uint8_t>((x + 2U * y) % 256U);
        }
    }
    return v;
}

// test.hash: hash_at(stream_key(seed, "test.hash"), x, y) >> 56.
inline std::vector<std::uint8_t> hash_cells(std::uint64_t seed) {
    const std::uint64_t key = worldgen::stream_key(seed, "test.hash");
    std::vector<std::uint8_t> v(CELLS);
    for (std::int64_t y = 0; y < SIDE; ++y) {
        for (std::int64_t x = 0; x < SIDE; ++x) {
            v[static_cast<std::size_t>(y * SIDE + x)] = static_cast<std::uint8_t>(worldgen::hash_at(key, x, y) >> 56U);
        }
    }
    return v;
}

// test.noise: mul_div(n.raw, 32767, 2^32), n = fbm2(stream_key(seed,
// "test.noise"), from_int(x) + HALF, from_int(y) + HALF, 5,
// from_raw(134217728), from_int(2), HALF).
inline std::vector<std::int16_t> noise_cells(std::uint64_t seed) {
    using worldgen::Fx;
    const std::uint64_t key = worldgen::stream_key(seed, "test.noise");
    const Fx base_freq = Fx::from_raw(134217728);
    const Fx lacunarity = Fx::from_int(2);
    std::vector<std::int16_t> v(CELLS);
    for (std::int64_t y = 0; y < SIDE; ++y) {
        const Fx fy = Fx::from_int(y) + Fx::HALF;
        for (std::int64_t x = 0; x < SIDE; ++x) {
            const Fx fx = Fx::from_int(x) + Fx::HALF;
            const Fx n = worldgen::fbm2(key, fx, fy, 5, base_freq, lacunarity, Fx::HALF);
            const std::int64_t value = worldgen::mul_div(n.raw(), 32767, std::int64_t{1} << 32);
            v[static_cast<std::size_t>(y * SIDE + x)] = static_cast<std::int16_t>(value);
        }
    }
    return v;
}

template <class T>
worldgen::db::LayerGrid grid_of(std::uint32_t width, std::uint32_t height, std::vector<T> values) {
    worldgen::db::LayerGrid grid;
    grid.width = width;
    grid.height = height;
    grid.values = std::move(values);
    return grid;
}

// --- Section 8 encodings, written out by hand ---------------------------------

class Encoder {
public:
    void u8(std::uint8_t v) { bytes_.push_back(v); }
    void u16(std::uint16_t v) { le(v, 2); }
    void u32(std::uint32_t v) { le(v, 4); }
    void u64(std::uint64_t v) { le(v, 8); }
    void i64(std::int64_t v) { le(static_cast<std::uint64_t>(v), 8); }
    void str(std::span<const std::uint8_t> s) {
        u64(s.size());
        bytes_.insert(bytes_.end(), s.begin(), s.end());
    }
    void str(std::string_view s) {
        u64(s.size());
        for (const char c : s) {
            bytes_.push_back(static_cast<std::uint8_t>(c));
        }
    }
    // Column values: tag byte, then content.
    void col_null() { u8(0x00); }
    void col_integer(std::int64_t v) {
        u8(0x01);
        i64(v);
    }
    void col_text(std::string_view s) {
        u8(0x03);
        str(s);
    }
    void col_blob(std::span<const std::uint8_t> b) {
        u8(0x04);
        str(b);
    }
    [[nodiscard]] worldgen::Sha256Digest digest() const { return worldgen::sha256(bytes_); }
    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }

private:
    void le(std::uint64_t v, int n) {
        for (int i = 0; i < n; ++i) {
            bytes_.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    std::vector<std::uint8_t> bytes_;
};

using MetaRows = std::vector<std::pair<std::string, std::string>>;

// The seventeen meta rows of a P0 world in key order (P0-02 8; the eight
// history keys at their defaults since schema 6, P3-03 4.2; history_end_year
// since schema 7, P3-04 9.4).
inline MetaRows meta_rows(std::uint64_t seed, std::string_view phrase = {}, std::string_view stages = {},
                          std::string_view map_size = "1024") {
    return {
        {"content_hash", std::string(EMPTY_CONTENT_HASH)},
        {"format", "the-unmapped-world"},
        {"generator_version", "0.27.0"},
        {"history_end_year", "6000"},
        {"param.civs_per_race", "auto"},
        {"param.divine_activity", "normal"},
        {"param.divine_script", ""},
        {"param.event_intensity", "normal"},
        {"param.history_detail", "normal"},
        {"param.history_span", "6000"},
        {"param.map_size", std::string(map_size)},
        {"param.monster_pressure", "normal"},
        {"param.races", "elf,goblin,dwarf,faerie,human,orc"},
        {"schema_version", "12"},   // P5-03 16.1; P5-04 9.2
        {"seed", std::to_string(seed)},
        {"seed_phrase", std::string(phrase)},
        {"stages", std::string(stages)},
    };
}

// table meta: u64 column count (2), then each row's key and value as TEXT.
inline worldgen::Sha256Digest meta_table_hash(const MetaRows& rows) {
    Encoder e;
    e.u64(2);
    for (const auto& [key, value] : rows) {
        e.col_text(key);
        e.col_text(value);
    }
    return e.digest();
}

// One layer row of table layer, as stored.
struct LayerRow {
    std::int64_t id;
    std::string name;
    std::int64_t width;
    std::int64_t height;
    std::string format;
    std::vector<std::uint8_t> data;
};

// table layer: u64 column count (6), then each row in id order.
inline worldgen::Sha256Digest layer_table_hash(const std::vector<LayerRow>& rows) {
    Encoder e;
    e.u64(6);
    for (const LayerRow& r : rows) {
        e.col_integer(r.id);
        e.col_text(r.name);
        e.col_integer(r.width);
        e.col_integer(r.height);
        e.col_text(r.format);
        e.col_blob(r.data);
    }
    return e.digest();
}

// One row of table `pass` (P1-01 9.1), as stored.
struct PassTableRow {
    std::int64_t id, range_id, kind, capped, col_x, col_y, saddle_dm, col_dm;
    std::vector<std::uint8_t> path;
};

// table pass: u64 column count (9), then each row in id order.
inline worldgen::Sha256Digest pass_table_hash(const std::vector<PassTableRow>& rows) {
    Encoder e;
    e.u64(9);
    for (const PassTableRow& r : rows) {
        for (const std::int64_t v : {r.id, r.range_id, r.kind, r.capped, r.col_x, r.col_y, r.saddle_dm, r.col_dm}) {
            e.col_integer(v);
        }
        e.col_blob(r.path);
    }
    return e.digest();
}

// table pass with no rows: the column count 9 only.
inline worldgen::Sha256Digest empty_pass_table_hash() { return pass_table_hash({}); }

// tables biome (5 columns) and climate (3 columns) with no rows (schema 4):
// the column count only.
inline worldgen::Sha256Digest empty_table_hash(std::uint64_t columns) {
    Encoder e;
    e.u64(columns);
    return e.digest();
}
inline worldgen::Sha256Digest empty_biome_table_hash() { return empty_table_hash(5); }
inline worldgen::Sha256Digest empty_climate_table_hash() { return empty_table_hash(3); }
// tables region (28 columns), region_border (11) and region_biome (3) with
// no rows (schema 5).
inline worldgen::Sha256Digest empty_region_table_hash() { return empty_table_hash(28); }
inline worldgen::Sha256Digest empty_region_border_table_hash() { return empty_table_hash(11); }
inline worldgen::Sha256Digest empty_region_biome_table_hash() { return empty_table_hash(3); }
// tables race (12 columns), race_biome (3), culture (4), god (3) and age (5)
// with no rows (schema 6).
inline worldgen::Sha256Digest empty_race_table_hash() { return empty_table_hash(12); }
inline worldgen::Sha256Digest empty_race_biome_table_hash() { return empty_table_hash(3); }
inline worldgen::Sha256Digest empty_culture_table_hash() { return empty_table_hash(4); }
inline worldgen::Sha256Digest empty_god_table_hash() { return empty_table_hash(3); }
inline worldgen::Sha256Digest empty_age_table_hash() { return empty_table_hash(5); }
// The thirteen history tables with no rows (schema 7), by column count.
inline worldgen::Sha256Digest empty_event_kind_table_hash() { return empty_table_hash(7); }
inline worldgen::Sha256Digest empty_event_kind_role_table_hash() { return empty_table_hash(6); }
inline worldgen::Sha256Digest empty_event_kind_value_table_hash() { return empty_table_hash(3); }
inline worldgen::Sha256Digest empty_phase_table_hash() { return empty_table_hash(6); }
inline worldgen::Sha256Digest empty_event_table_hash() { return empty_table_hash(8); }
inline worldgen::Sha256Digest empty_event_participant_table_hash() { return empty_table_hash(5); }
inline worldgen::Sha256Digest empty_event_cause_table_hash() { return empty_table_hash(2); }
inline worldgen::Sha256Digest empty_event_value_table_hash() { return empty_table_hash(3); }
inline worldgen::Sha256Digest empty_civilization_table_hash() { return empty_table_hash(10); }
inline worldgen::Sha256Digest empty_settlement_table_hash() { return empty_table_hash(15); }   // P5-02: 15
inline worldgen::Sha256Digest empty_settlement_census_table_hash() { return empty_table_hash(6); }
inline worldgen::Sha256Digest empty_region_control_table_hash() { return empty_table_hash(5); }
inline worldgen::Sha256Digest empty_relation_table_hash() { return empty_table_hash(9); }
// table region_name (5 columns) with no rows (schema 8, P5-12 7.2).
inline worldgen::Sha256Digest empty_region_name_table_hash() { return empty_table_hash(5); }
// the five tables of P5-02 11.1 with no rows (schema 9).
inline worldgen::Sha256Digest empty_beast_table_hash() { return empty_table_hash(7); }   // P5-03 16.1: 7
inline worldgen::Sha256Digest empty_monster_kind_table_hash() { return empty_table_hash(8); }   // P5-03 16.1: 8
inline worldgen::Sha256Digest empty_beast_population_table_hash() { return empty_table_hash(4); }
inline worldgen::Sha256Digest empty_monster_table_hash() { return empty_table_hash(11); }
inline worldgen::Sha256Digest empty_site_table_hash() { return empty_table_hash(14); }
// the two tables of P5-03 16.1 with no rows (schema 10).
inline worldgen::Sha256Digest empty_race_relation_table_hash() { return empty_table_hash(5); }
inline worldgen::Sha256Digest empty_settlement_race_table_hash() { return empty_table_hash(5); }
// the two tables of P5-06 (schema 12) with no rows.
inline worldgen::Sha256Digest empty_figure_table_hash() { return empty_table_hash(19); }
inline worldgen::Sha256Digest empty_book_table_hash() { return empty_table_hash(6); }

// schema of every world at schema version 8 (the name is the P0 one):
// sqlite_schema rows ordered by type, then name: the automatic indexes of
// biome.key, culture.key, event_kind.key, layer.name and race.key (sql NULL,
// amendment 10), then the twenty-seven tables with their DDL. rootpage is excluded.
inline worldgen::Sha256Digest p0_schema_hash() {
    Encoder e;
    const auto index = [&e](std::string_view name, std::string_view table) {
        e.col_text("index");
        e.col_text(name);
        e.col_text(table);
        e.col_null();
    };
    const auto table = [&e](std::string_view name, std::string_view ddl) {
        e.col_text("table");
        e.col_text(name);
        e.col_text(name);
        e.col_text(ddl);
    };
    index("sqlite_autoindex_beast_1", "beast");                 // P5-02 11.1
    index("sqlite_autoindex_biome_1", "biome");
    index("sqlite_autoindex_culture_1", "culture");
    index("sqlite_autoindex_event_kind_1", "event_kind");
    index("sqlite_autoindex_layer_1", "layer");
    index("sqlite_autoindex_monster_kind_1", "monster_kind");   // P5-02 11.1
    index("sqlite_autoindex_race_1", "race");
    table("age", DDL_AGE);
    table("beast", DDL_BEAST);
    table("beast_population", DDL_BEAST_POPULATION);
    table("biome", DDL_BIOME);
    table("book", DDL_BOOK);   // P5-06 9.3
    table("civilization", DDL_CIVILIZATION);
    table("climate", DDL_CLIMATE);
    table("culture", DDL_CULTURE);
    table("event", DDL_EVENT);
    table("event_cause", DDL_EVENT_CAUSE);
    table("event_kind", DDL_EVENT_KIND);
    table("event_kind_role", DDL_EVENT_KIND_ROLE);
    table("event_kind_value", DDL_EVENT_KIND_VALUE);
    table("event_participant", DDL_EVENT_PARTICIPANT);
    table("event_value", DDL_EVENT_VALUE);
    table("figure", DDL_FIGURE);   // P5-06 9.1
    table("god", DDL_GOD);
    table("layer", DDL_LAYER);
    table("meta", DDL_META);
    table("monster", DDL_MONSTER);
    table("monster_kind", DDL_MONSTER_KIND);
    table("pass", DDL_PASS);
    table("phase", DDL_PHASE);
    table("race", DDL_RACE);
    table("race_biome", DDL_RACE_BIOME);
    table("race_relation", DDL_RACE_RELATION);   // P5-03 16.1
    table("region", DDL_REGION);
    table("region_biome", DDL_REGION_BIOME);
    table("region_border", DDL_REGION_BORDER);
    table("region_control", DDL_REGION_CONTROL);
    table("region_name", DDL_REGION_NAME);
    table("relation", DDL_RELATION);
    table("settlement", DDL_SETTLEMENT);
    table("settlement_census", DDL_SETTLEMENT_CENSUS);
    table("settlement_race", DDL_SETTLEMENT_RACE);   // P5-03 16.1
    table("site", DDL_SITE);
    return e.digest();
}

// The `table` lines of hash --detail for a world whose only non-empty
// tables are layer and meta (schema 8): (name, digest) in ascending name.
inline std::vector<std::pair<std::string, worldgen::Sha256Digest>> empty_world_tables(
    const worldgen::Sha256Digest& layer, const worldgen::Sha256Digest& meta) {
    return {
        {"age", empty_age_table_hash()},
        {"beast", empty_beast_table_hash()},                        // P5-02 11.1
        {"beast_population", empty_beast_population_table_hash()},
        {"biome", empty_biome_table_hash()},
        {"book", empty_book_table_hash()},   // P5-06 9.3
        {"civilization", empty_civilization_table_hash()},
        {"climate", empty_climate_table_hash()},
        {"culture", empty_culture_table_hash()},
        {"event", empty_event_table_hash()},
        {"event_cause", empty_event_cause_table_hash()},
        {"event_kind", empty_event_kind_table_hash()},
        {"event_kind_role", empty_event_kind_role_table_hash()},
        {"event_kind_value", empty_event_kind_value_table_hash()},
        {"event_participant", empty_event_participant_table_hash()},
        {"event_value", empty_event_value_table_hash()},
        {"figure", empty_figure_table_hash()},   // P5-06 9.1
        {"god", empty_god_table_hash()},
        {"layer", layer},
        {"meta", meta},
        {"monster", empty_monster_table_hash()},
        {"monster_kind", empty_monster_kind_table_hash()},
        {"pass", empty_pass_table_hash()},
        {"phase", empty_phase_table_hash()},
        {"race", empty_race_table_hash()},
        {"race_biome", empty_race_biome_table_hash()},
        {"race_relation", empty_race_relation_table_hash()},       // P5-03 16.1
        {"region", empty_region_table_hash()},
        {"region_biome", empty_region_biome_table_hash()},
        {"region_border", empty_region_border_table_hash()},
        {"region_control", empty_region_control_table_hash()},
        {"region_name", empty_region_name_table_hash()},
        {"relation", empty_relation_table_hash()},
        {"settlement", empty_settlement_table_hash()},
        {"settlement_census", empty_settlement_census_table_hash()},
        {"settlement_race", empty_settlement_race_table_hash()},   // P5-03 16.1
        {"site", empty_site_table_hash()},
    };
}

// layer <name>: u32 width, u32 height, u8 type code, then elements
// little-endian at their natural size.
inline worldgen::Sha256Digest layer_hash_u8(std::uint32_t w, std::uint32_t h, const std::vector<std::uint8_t>& v) {
    Encoder e;
    e.u32(w);
    e.u32(h);
    e.u8(1);
    for (const std::uint8_t x : v) {
        e.u8(x);
    }
    return e.digest();
}
inline worldgen::Sha256Digest layer_hash_i16(std::uint32_t w, std::uint32_t h, const std::vector<std::int16_t>& v) {
    Encoder e;
    e.u32(w);
    e.u32(h);
    e.u8(4);
    for (const std::int16_t x : v) {
        e.u16(static_cast<std::uint16_t>(x));
    }
    return e.digest();
}

// --- Section 7 report -------------------------------------------------------------

// The exact report of section 7 for generate --seed 0, in format v2 (P0-04
// amendment 16, P1-00 10.3): the header line is "# worldgen report v2"; the
// v1 sections follow unchanged, and an empty world has no [geography].
inline constexpr std::string_view REPORT_SEED_0 =
    "# worldgen report v2\n"
    "[meta]\n"
    "content_hash = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n"
    "format = the-unmapped-world\n"
    "generator_version = 0.27.0\n"
    "history_end_year = 6000\n"
    "param.civs_per_race = auto\n"
    "param.divine_activity = normal\n"
    "param.divine_script =\n"
    "param.event_intensity = normal\n"
    "param.history_detail = normal\n"
    "param.history_span = 6000\n"
    "param.map_size = 1024\n"
    "param.monster_pressure = normal\n"
    "param.races = elf,goblin,dwarf,faerie,human,orc\n"
    "schema_version = 12\n"
    "seed = 0\n"
    "seed_phrase =\n"
    "stages =\n"
    "[tables]\n"
    "age = 0\n"
    "beast = 0\n"
    "beast_population = 0\n"
    "biome = 0\nbook = 0\n"
    "civilization = 0\n"
    "climate = 0\n"
    "culture = 0\n"
    "event = 0\n"
    "event_cause = 0\n"
    "event_kind = 0\n"
    "event_kind_role = 0\n"
    "event_kind_value = 0\n"
    "event_participant = 0\n"
    "event_value = 0\nfigure = 0\n"
    "god = 0\n"
    "layer = 0\n"
    "meta = 17\n"
    "monster = 0\n"
    "monster_kind = 0\n"
    "pass = 0\n"
    "phase = 0\n"
    "race = 0\n"
    "race_biome = 0\n"
    "race_relation = 0\n"
    "region = 0\n"
    "region_biome = 0\n"
    "region_border = 0\n"
    "region_control = 0\n"
    "region_name = 0\n"
    "relation = 0\n"
    "settlement = 0\n"
    "settlement_census = 0\n"
    "settlement_race = 0\n"
    "site = 0\n"
    "[layers]\n"
    "(none)\n";

}  // namespace p004
