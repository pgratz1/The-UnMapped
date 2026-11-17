// World database writer and reader tests, spec P0-02 sections 5, 6, 6.1, 8,
// 9, 10 and 12.2, with the amendments of section 14.

#include <doctest/doctest.h>
#include <sqlite3.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "db_test_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/db.hpp"
#include "worldgen/db/testing.hpp"

namespace {

namespace fs = std::filesystem;
using worldgen::FatalError;
using worldgen::db::Codec;
using worldgen::db::ElementType;
using worldgen::db::ErrorCode;
using worldgen::db::LayerGrid;
using worldgen::db::Overwrite;
using worldgen::db::WorldReader;
using worldgen::db::WorldWriter;
using worldgen::db::testing::WriterAccess;
using Bytes = std::vector<std::uint8_t>;
using Rows = std::vector<std::vector<std::string>>;

// The DDL of P0-02 5.4, copied from the spec text.
constexpr const char* SPEC_DDL_META =
    "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) STRICT, WITHOUT ROWID";
constexpr const char* SPEC_DDL_LAYER =
    "CREATE TABLE layer (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, width INTEGER NOT NULL, height INTEGER "
    "NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL) STRICT";
// The DDL of table pass, schema version 2 (P1-01 9.1), copied from the spec text.
constexpr const char* SPEC_DDL_PASS =
    "CREATE TABLE pass (id INTEGER PRIMARY KEY, range_id INTEGER NOT NULL, kind INTEGER NOT NULL, capped "
    "INTEGER NOT NULL, col_x INTEGER NOT NULL, col_y INTEGER NOT NULL, saddle_dm INTEGER NOT NULL, col_dm INTEGER "
    "NOT NULL, path BLOB NOT NULL) STRICT";
constexpr const char* EMPTY_SHA256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

template <class T>
LayerGrid grid_of(std::uint32_t width, std::uint32_t height, std::vector<T> values) {
    LayerGrid grid;
    grid.width = width;
    grid.height = height;
    grid.values = std::move(values);
    return grid;
}

LayerGrid ramp_u8(std::uint32_t w, std::uint32_t h) {
    std::vector<std::uint8_t> v(static_cast<std::size_t>(w) * h);
    for (std::uint32_t y = 0; y < h; ++y) {
        for (std::uint32_t x = 0; x < w; ++x) {
            v[static_cast<std::size_t>(y) * w + x] = static_cast<std::uint8_t>((x + 2U * y) % 256U);
        }
    }
    return grid_of<std::uint8_t>(w, h, std::move(v));
}

LayerGrid random_i16(std::uint32_t w, std::uint32_t h, std::uint64_t key) {
    worldgen::Rng rng(key);
    std::vector<std::int16_t> v(static_cast<std::size_t>(w) * h);
    for (auto& e : v) {
        const std::uint64_t r = rng.next();
        e = static_cast<std::int16_t>(static_cast<std::uint16_t>(r >> 48U));
    }
    return grid_of<std::int16_t>(w, h, std::move(v));
}

// The required meta rows of an empty P0 world (P0-02 section 8).
Rows expected_empty_meta(const std::string& seed_text, const std::string& phrase) {
    return {
        {"content_hash", EMPTY_SHA256},
        {"format", "the-unmapped-world"},
        {"generator_version", "0.6.0"},
        {"param.map_size", "1024"},
        {"schema_version", "2"},
        {"seed", seed_text},
        {"seed_phrase", phrase},
        {"stages", ""},
    };
}

// The header fields of P0-02 6.1, read from the file bytes, and the
// invariants of section 10 that concern them.
void check_header(const std::string& path) {
    const Bytes b = dbtest::read_file(path);
    REQUIRE(b.size() >= 100);
    const char magic[16] = {'S', 'Q', 'L', 'i', 't', 'e', ' ', 'f', 'o', 'r', 'm', 'a', 't', ' ', '3', '\0'};
    for (std::size_t i = 0; i < 16; ++i) {
        CHECK(b[i] == static_cast<std::uint8_t>(magic[i]));
    }
    CHECK(dbtest::be16(b, 16) == 4096);  // page size
    CHECK(b[18] == 1);                   // write format version (never WAL)
    CHECK(b[19] == 1);                   // read format version
    CHECK(b[20] == 0);                   // reserved bytes per page
    CHECK(b[21] == 64);                  // payload fractions
    CHECK(b[22] == 32);
    CHECK(b[23] == 32);
    CHECK(static_cast<std::uint64_t>(dbtest::be32(b, 28)) * 4096U == b.size());  // size in pages
    CHECK(b.size() % 4096U == 0);
    CHECK(dbtest::be32(b, 32) == 0);  // first freelist trunk page
    CHECK(dbtest::be32(b, 36) == 0);  // freelist page count
    CHECK(dbtest::be32(b, 44) == 4);  // schema format number
    CHECK(dbtest::be32(b, 48) == 0);  // default page cache size
    CHECK(dbtest::be32(b, 52) == 0);  // largest root page (auto-vacuum)
    CHECK(dbtest::be32(b, 56) == 1);  // text encoding UTF-8
    CHECK(dbtest::be32(b, 60) == static_cast<std::uint32_t>(worldgen::db::SCHEMA_VERSION));  // user version
    CHECK(dbtest::be32(b, 60) == 2);
    CHECK(dbtest::be32(b, 64) == 0);           // incremental vacuum
    CHECK(dbtest::be32(b, 68) == 0x554E4D50U);  // application id "UNMP"
    CHECK(b[68] == 'U');
    CHECK(b[69] == 'N');
    CHECK(b[70] == 'M');
    CHECK(b[71] == 'P');
    for (std::size_t i = 72; i < 92; ++i) {
        CAPTURE(i);
        CHECK(b[i] == 0);  // reserved
    }
    CHECK(dbtest::be32(b, 92) == dbtest::be32(b, 24));  // version-valid-for = change counter
    CHECK(dbtest::be32(b, 96) == static_cast<std::uint32_t>(sqlite3_libversion_number()));
}

// typeof() of every value in every table is never 'real' (section 10, 14.1).
void check_no_real_values(const std::string& path) {
    dbtest::SqliteFile db(path);
    REQUIRE(db.ok());
    const Rows tables = db.rows("SELECT name FROM sqlite_schema WHERE type = 'table' ORDER BY name");
    REQUIRE_FALSE(tables.empty());
    for (const auto& t : tables) {
        const Rows cols = db.rows("SELECT name FROM pragma_table_info('" + t[0] + "')");
        REQUIRE_FALSE(cols.empty());
        for (const auto& c : cols) {
            CAPTURE(t[0]);
            CAPTURE(c[0]);
            const Rows n = db.rows("SELECT count(*) FROM \"" + t[0] + "\" WHERE typeof(\"" + c[0] + "\") = 'real'");
            REQUIRE(n.size() == 1);
            CHECK(n[0][0] == "0");
        }
    }
}

// Only `keep` is in the directory: no .partial, -journal, -wal or -shm.
void check_only(const fs::path& dir, const std::set<std::string>& keep) {
    CHECK(dbtest::dir_entries(dir) == keep);
}

// A world with two stages and three layers, for reader tests.
void write_layered_world(const std::string& path, std::uint64_t seed) {
    WorldWriter writer;
    dbtest::set_standard_meta(writer, seed);
    {
        auto stage = writer.begin_stage("test_pattern");
        stage.write_layer(1, "test.ramp", ramp_u8(16, 9));
        stage.write_layer(2, "test.noise", random_i16(10, 12, seed), Codec::raw);
        stage.commit();
    }
    {
        auto stage = writer.begin_stage("second");
        stage.write_layer(3, "elevation", grid_of<std::uint64_t>(2, 1, {0, std::numeric_limits<std::uint64_t>::max()}));
        stage.commit();
    }
    REQUIRE(writer.finalize(path, Overwrite::refuse).ok());
}

}  // namespace

TEST_SUITE("db.writer_reader") {

TEST_CASE("empty world for seed 0: tables, meta rows and values as in section 8 (12.2)") {
    dbtest::TempDir dir("empty-seed-0");
    const std::string path = dir.file("world.db");
    const auto status = dbtest::write_empty_world(path, 0);
    REQUIRE_MESSAGE(status.ok(), (status.ok() ? std::string() : status.error().message));

    check_only(dir.path(), {"world.db"});
    check_header(path);
    check_no_real_values(path);
    // Amendment 14.9: the empty world is 4 pages; schema version 2 adds the
    // root page of table pass (P1-01 9.1), so 5.
    CHECK(dbtest::read_file(path).size() == 20480);

    dbtest::SqliteFile db(path);
    REQUIRE(db.ok());
    // sqlite_schema: the three tables and the automatic index of layer.name.
    const Rows schema = db.rows("SELECT type, name, tbl_name FROM sqlite_schema ORDER BY type, name");
    CHECK(schema == Rows{{"index", "sqlite_autoindex_layer_1", "layer"}, {"table", "layer", "layer"},
                         {"table", "meta", "meta"}, {"table", "pass", "pass"}});
    // DDL byte for byte (5.4), and equal to the header's constants.
    CHECK(db.rows("SELECT sql FROM sqlite_schema WHERE name = 'meta'") == Rows{{SPEC_DDL_META}});
    CHECK(db.rows("SELECT sql FROM sqlite_schema WHERE name = 'layer'") == Rows{{SPEC_DDL_LAYER}});
    CHECK(worldgen::db::DDL_META == SPEC_DDL_META);
    CHECK(worldgen::db::DDL_LAYER == SPEC_DDL_LAYER);
    CHECK(db.rows("SELECT sql FROM sqlite_schema WHERE name = 'pass'") == Rows{{SPEC_DDL_PASS}});
    CHECK(worldgen::db::DDL_PASS == SPEC_DDL_PASS);
    CHECK(db.rows("SELECT count(*) FROM pass") == Rows{{"0"}});
    CHECK(db.rows("SELECT hex(sql) FROM sqlite_schema WHERE name = 'layer'") ==
          db.rows("SELECT hex(CAST('" + std::string(SPEC_DDL_LAYER) + "' AS BLOB))"));
    // Schema 2: the DDL of table `pass` (P1-01 9.1) byte for byte, and the
    // table empty in a world no stage wrote.
    CHECK(db.rows("SELECT sql FROM sqlite_schema WHERE name = 'pass'") == Rows{{SPEC_DDL_PASS}});
    CHECK(db.rows("SELECT hex(sql) FROM sqlite_schema WHERE name = 'pass'") ==
          db.rows("SELECT hex(CAST('" + std::string(SPEC_DDL_PASS) + "' AS BLOB))"));
    CHECK(db.rows("SELECT count(*) FROM pass") == Rows{{"0"}});
    CHECK(worldgen::db::SCHEMA_VERSION == 2);

    // meta: exactly 8 rows, in key order, all TEXT.
    CHECK(db.rows("SELECT key, value FROM meta ORDER BY key") == expected_empty_meta("0", ""));
    CHECK(db.rows("SELECT count(*) FROM meta WHERE typeof(key) <> 'text' OR typeof(value) <> 'text'") ==
          Rows{{"0"}});
    // layer: no rows.
    CHECK(db.rows("SELECT count(*) FROM layer") == Rows{{"0"}});
    // Pragmas agree with the header.
    CHECK(db.rows("PRAGMA application_id") == Rows{{"1431194960"}});
    CHECK(db.rows("PRAGMA user_version") == Rows{{"2"}});
    CHECK(db.rows("PRAGMA page_size") == Rows{{"4096"}});
    CHECK(db.rows("PRAGMA encoding") == Rows{{"UTF-8"}});
    CHECK(db.rows("PRAGMA auto_vacuum") == Rows{{"0"}});
    CHECK(db.rows("PRAGMA freelist_count") == Rows{{"0"}});
    CHECK(db.rows("PRAGMA journal_mode") == Rows{{"delete"}});
    CHECK(db.rows("PRAGMA integrity_check") == Rows{{"ok"}});
}

TEST_CASE("empty world through the reader (9)") {
    dbtest::TempDir dir("empty-reader");
    const std::string path = dir.file("world.db");
    REQUIRE(dbtest::write_empty_world(path, 0).ok());

    auto opened = WorldReader::open(path);
    REQUIRE_MESSAGE(opened.ok(), (opened.ok() ? std::string() : opened.error().message));
    const WorldReader& reader = opened.value();
    CHECK(reader.seed() == 0);
    CHECK(reader.schema_version() == 2);
    CHECK(reader.generator_version() == "0.6.0");
    CHECK(reader.seed_phrase().empty());
    CHECK(reader.map_size() == 1024);
    CHECK(reader.stages().empty());
    CHECK(reader.content_hash() == EMPTY_SHA256);
    CHECK(reader.meta("format") == std::optional<std::string>("the-unmapped-world"));
    CHECK(reader.meta("seed_phrase") == std::optional<std::string>(""));
    CHECK_FALSE(reader.meta("no_such_key").has_value());

    const auto entries = reader.meta_entries();
    const Rows expected = expected_empty_meta("0", "");
    REQUIRE(entries.size() == expected.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        CHECK(entries[i].key == expected[i][0]);
        CHECK(entries[i].value == expected[i][1]);
    }
    const auto layers = reader.layers();
    REQUIRE(layers.ok());
    CHECK(layers.value().empty());
    const auto missing = reader.read_layer("elevation");
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.code() == ErrorCode::layer_not_found);
}

TEST_CASE("header fields of 6.1 hold for a world with layers too") {
    dbtest::TempDir dir("header-layers");
    const std::string path = dir.file("world.db");
    write_layered_world(path, 42);
    check_header(path);
    check_no_real_values(path);
    check_only(dir.path(), {"world.db"});
}

TEST_CASE("seed canonical text (5.5, 12.2)") {
    struct Case {
        std::uint64_t seed;
        const char* text;
    };
    const Case cases[] = {
        {0, "0"},
        {1, "1"},
        {42, "42"},
        {9223372036854775808ULL, "9223372036854775808"},
        {18446744073709551615ULL, "18446744073709551615"},
    };
    dbtest::TempDir dir("seed-text");
    int index = 0;
    for (const Case& c : cases) {
        CAPTURE(c.text);
        const std::string path = dir.file("w" + std::to_string(index++) + ".db");
        REQUIRE(dbtest::write_empty_world(path, c.seed).ok());
        dbtest::SqliteFile db(path);
        REQUIRE(db.ok());
        CHECK(db.rows("SELECT value, typeof(value) FROM meta WHERE key = 'seed'") == Rows{{c.text, "text"}});
        CHECK(db.rows("SELECT key, value FROM meta ORDER BY key") == expected_empty_meta(c.text, ""));
        auto reader = WorldReader::open(path);
        REQUIRE(reader.ok());
        CHECK(reader.value().seed() == c.seed);
    }
    CHECK(worldgen::db::format_u64(18446744073709551615ULL) == "18446744073709551615");
    CHECK(worldgen::db::format_u64(0) == "0");
    CHECK(worldgen::db::parse_canonical_u64("18446744073709551615") == 18446744073709551615ULL);
    CHECK_FALSE(worldgen::db::parse_canonical_u64("18446744073709551616").has_value());
    CHECK_FALSE(worldgen::db::parse_canonical_u64("042").has_value());
    CHECK_FALSE(worldgen::db::parse_canonical_u64("").has_value());
    CHECK_FALSE(worldgen::db::parse_canonical_u64("+1").has_value());
}

TEST_CASE("phrase seed: seed_phrase holds the scrubbed phrase, seed holds phrase_seed of it (12.2, amendment 12)") {
    dbtest::TempDir dir("phrase");
    // Scrubbed phrases (P0-01 7.3): "Ünmapped Wörld" scrubs to "nmapped w rld".
    const std::string phrases[] = {"the unmapped", "nmapped w rld", "42", "a", std::string(256, 'z')};
    int index = 0;
    for (const std::string& phrase : phrases) {
        CAPTURE(phrase);
        const std::string path = dir.file("p" + std::to_string(index++) + ".db");
        {
            WorldWriter writer;
            writer.set_seed_phrase(phrase);
            CHECK(writer.seed() == worldgen::phrase_seed(phrase));
            writer.set_content_hash(dbtest::empty_content_hash());
            writer.set_param_integer("map_size", 1024);
            REQUIRE(writer.finalize(path, Overwrite::refuse).ok());
        }
        const std::uint64_t seed = worldgen::phrase_seed(phrase);
        CHECK(worldgen::seed_from_phrase(phrase).seed == seed);
        dbtest::SqliteFile db(path);
        REQUIRE(db.ok());
        CHECK(db.rows("SELECT key, value FROM meta ORDER BY key") ==
              expected_empty_meta(worldgen::db::format_u64(seed), phrase));
        auto reader = WorldReader::open(path);
        REQUIRE(reader.ok());
        CHECK(reader.value().seed() == seed);
        CHECK(reader.value().seed_phrase() == phrase);
    }
    // The phrase as typed scrubs to what the writer takes: same file bytes.
    {
        const worldgen::PhraseSeed ps = worldgen::seed_from_phrase("\xC3\x9Cnmapped W\xC3\xB6rld");
        REQUIRE(ps.ok());
        CHECK(ps.phrase == "nmapped w rld");
        const std::string path = dir.file("typed.db");
        WorldWriter writer;
        writer.set_seed_phrase(ps.phrase);
        CHECK(writer.seed() == ps.seed);
        writer.set_content_hash(dbtest::empty_content_hash());
        writer.set_param_integer("map_size", 1024);
        REQUIRE(writer.finalize(path, Overwrite::refuse).ok());
        CHECK(dbtest::read_file(path) == dbtest::read_file(dir.file("p1.db")));
    }
}

TEST_CASE("phrase seed: the writer takes only an already scrubbed phrase (amendment 12)") {
    const std::string bad[] = {
        "The Unmapped",           // capitals
        "THE UNMAPPED",
        "the-unmapped",           // punctuation
        "the unmapped!",
        "the  unmapped",          // double space
        " the unmapped",          // edge spaces
        "the unmapped ",
        "the\tunmapped",          // whitespace other than a single space
        "the unmapped\n",
        std::string("a\0b", 3),   // NUL
        "\xC3\x9Cnmapped",        // non-ASCII bytes
        "nmapped w\xC3\xB6rld",
        "",                       // empty
        " ",
        std::string(257, 'a'),    // over 256 bytes
        std::string(128, 'a') + " " + std::string(128, 'b'),
    };
    for (const std::string& phrase : bad) {
        CAPTURE(phrase);
        CAPTURE(phrase.size());
        WorldWriter writer;
        CHECK_THROWS_AS(writer.set_seed_phrase(phrase), FatalError);
    }
    // Boundary: 256 bytes with interior spaces is accepted.
    WorldWriter writer;
    const std::string ok256 = std::string(127, 'a') + " " + std::string(128, 'b');
    REQUIRE(ok256.size() == 256);
    writer.set_seed_phrase(ok256);
    CHECK(writer.seed() == worldgen::phrase_seed(ok256));
}

TEST_CASE("map size must be 512, 1024 or 2048 (5.5, 14.5)") {
    dbtest::TempDir dir("map-size");
    for (const std::int64_t size : {512, 2048}) {
        const std::string path = dir.file("m" + std::to_string(size) + ".db");
        WorldWriter writer;
        writer.set_seed(7);
        writer.set_content_hash(dbtest::empty_content_hash());
        writer.set_param_integer("map_size", size);
        REQUIRE(writer.finalize(path, Overwrite::refuse).ok());
        auto reader = WorldReader::open(path);
        REQUIRE(reader.ok());
        CHECK(reader.value().map_size() == static_cast<std::uint32_t>(size));
        dbtest::SqliteFile db(path);
        CHECK(db.rows("SELECT value FROM meta WHERE key = 'param.map_size'") == Rows{{std::to_string(size)}});
    }
    WorldWriter writer;
    CHECK_THROWS_AS(writer.set_param_integer("map_size", 1000), FatalError);
    CHECK_THROWS_AS(writer.set_param_integer("map_size", 0), FatalError);
    CHECK_THROWS_AS(writer.set_param_integer("Bad", 1), FatalError);
}

TEST_CASE("finalize without a required value is a fatal error and writes nothing") {
    dbtest::TempDir dir("missing-required");
    const std::string path = dir.file("world.db");
    {
        WorldWriter writer;
        writer.set_content_hash(dbtest::empty_content_hash());
        writer.set_param_integer("map_size", 1024);
        CHECK_THROWS_AS((void)writer.finalize(path, Overwrite::refuse), FatalError);
    }
    {
        WorldWriter writer;
        writer.set_seed(1);
        writer.set_param_integer("map_size", 1024);
        CHECK_THROWS_AS((void)writer.finalize(path, Overwrite::refuse), FatalError);
    }
    {
        WorldWriter writer;
        writer.set_seed(1);
        writer.set_content_hash(dbtest::empty_content_hash());
        CHECK_THROWS_AS((void)writer.finalize(path, Overwrite::refuse), FatalError);
    }
    check_only(dir.path(), {});
}

TEST_CASE("layers, stages and the reader's layer list and read (5.6, 9, 14.3)") {
    dbtest::TempDir dir("layers");
    const std::string path = dir.file("world.db");
    write_layered_world(path, 42);

    dbtest::SqliteFile db(path);
    REQUIRE(db.ok());
    CHECK(db.rows("SELECT value FROM meta WHERE key = 'stages'") == Rows{{"test_pattern,second"}});
    CHECK(db.rows("SELECT id, name, width, height, format, typeof(data) FROM layer ORDER BY id") ==
          Rows{{"1", "test.ramp", "16", "9", "u8", "blob"},
               {"2", "test.noise", "10", "12", "i16", "blob"},
               {"3", "elevation", "2", "1", "u64", "blob"}});
    // The stored blobs are exactly the encoder's output.
    const Bytes ramp_blob = worldgen::db::encode_layer_blob(ramp_u8(16, 9));
    CHECK(db.rows("SELECT length(data) FROM layer WHERE id = 1") == Rows{{std::to_string(ramp_blob.size())}});

    auto opened = WorldReader::open(path);
    REQUIRE(opened.ok());
    const WorldReader& reader = opened.value();
    CHECK(reader.stages() == std::vector<std::string>{"test_pattern", "second"});

    const auto list = reader.layers();
    REQUIRE(list.ok());
    REQUIRE(list.value().size() == 3);
    const auto& l = list.value();
    CHECK(l[0].id == 1);
    CHECK(l[0].name == "test.ramp");
    CHECK(l[0].width == 16);
    CHECK(l[0].height == 9);
    CHECK(l[0].type == ElementType::u8);
    CHECK(l[0].codec == Codec::delta_varint);
    CHECK(l[0].encoded_size == ramp_blob.size());
    CHECK(l[1].id == 2);
    CHECK(l[1].name == "test.noise");
    CHECK(l[1].type == ElementType::i16);
    CHECK(l[1].codec == Codec::raw);
    CHECK(l[1].encoded_size == 24U + 10U * 12U * 2U);
    CHECK(l[2].id == 3);
    CHECK(l[2].name == "elevation");
    CHECK(l[2].type == ElementType::u64);

    const auto ramp = reader.read_layer("test.ramp");
    REQUIRE(ramp.ok());
    CHECK(ramp.value().id == 1);
    CHECK(ramp.value().name == "test.ramp");
    CHECK(ramp.value().codec == Codec::delta_varint);
    CHECK(ramp.value().grid.width == 16);
    CHECK(ramp.value().grid.height == 9);
    CHECK(ramp.value().grid.values == ramp_u8(16, 9).values);
    // Row-major, y * width + x, row 0 north (5.6).
    REQUIRE(ramp.value().grid.get_if<std::uint8_t>() != nullptr);
    CHECK(ramp.value().grid.get_if<std::uint8_t>()->at(3U * 16U + 5U) == 11);

    const auto noise = reader.read_layer("test.noise");
    REQUIRE(noise.ok());
    CHECK(noise.value().codec == Codec::raw);
    CHECK(noise.value().grid.values == random_i16(10, 12, 42).values);

    const auto elevation = reader.read_layer("elevation");
    REQUIRE(elevation.ok());
    CHECK(elevation.value().grid.values ==
          grid_of<std::uint64_t>(2, 1, {0, std::numeric_limits<std::uint64_t>::max()}).values);

    const auto missing = reader.read_layer("test.missing");
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.code() == ErrorCode::layer_not_found);
}

TEST_CASE("layer order: writing b then a gives ids 1 and 2, list returns b, a (12.2)") {
    dbtest::TempDir dir("layer-order");
    const std::string path = dir.file("world.db");
    {
        WorldWriter writer;
        dbtest::set_standard_meta(writer, 3);
        CHECK(writer.next_layer_id() == 1);
        auto stage = writer.begin_stage("order");
        stage.write_layer(1, "b", ramp_u8(2, 2));
        CHECK(writer.next_layer_id() == 2);
        stage.write_layer(2, "a", ramp_u8(3, 1));
        CHECK(writer.next_layer_id() == 3);
        stage.commit();
        REQUIRE(writer.finalize(path, Overwrite::refuse).ok());
    }
    dbtest::SqliteFile db(path);
    CHECK(db.rows("SELECT id, name FROM layer ORDER BY id") == Rows{{"1", "b"}, {"2", "a"}});
    auto reader = WorldReader::open(path);
    REQUIRE(reader.ok());
    const auto list = reader.value().layers();
    REQUIRE(list.ok());
    REQUIRE(list.value().size() == 2);
    CHECK(list.value()[0].name == "b");
    CHECK(list.value()[0].id == 1);
    CHECK(list.value()[1].name == "a");
    CHECK(list.value()[1].id == 2);
}

TEST_CASE("writer contract violations are fatal (5.6, 6, 14.6)") {
    WorldWriter writer;
    dbtest::set_standard_meta(writer, 1);
    {
        auto stage = writer.begin_stage("one");
        CHECK_THROWS_AS(stage.write_layer(2, "x", ramp_u8(1, 1)), FatalError);  // id out of order
        CHECK_THROWS_AS(stage.write_layer(0, "x", ramp_u8(1, 1)), FatalError);
        CHECK_THROWS_AS(stage.write_layer(1, "Bad", ramp_u8(1, 1)), FatalError);  // invalid name
        CHECK_THROWS_AS(stage.write_layer(1, "", ramp_u8(1, 1)), FatalError);
        CHECK_THROWS_AS(stage.write_layer(1, std::string(65, 'a'), ramp_u8(1, 1)), FatalError);
        CHECK_THROWS_AS(stage.write_layer(1, "x", grid_of<std::uint8_t>(2, 2, {1})), FatalError);  // invalid grid
        CHECK_THROWS_AS((void)writer.begin_stage("two"), FatalError);  // another stage open
        stage.write_layer(1, "x", ramp_u8(1, 1));
        CHECK_THROWS_AS(stage.write_layer(2, "x", ramp_u8(1, 1)), FatalError);  // duplicate name
        stage.commit();
        CHECK_THROWS_AS(stage.commit(), FatalError);
    }
    CHECK_THROWS_AS((void)writer.begin_stage("one"), FatalError);  // same stage name twice (14.6)
    CHECK_THROWS_AS((void)writer.begin_stage("Bad"), FatalError);
    CHECK_THROWS_AS((void)writer.begin_stage("a.b"), FatalError);  // stage names have no dots
    CHECK(writer.stages() == std::vector<std::string>{"one"});
}

TEST_CASE("a stage that ends without committing is rolled back and not recorded (14.6)") {
    dbtest::TempDir dir("rollback");
    const std::string path = dir.file("world.db");
    {
        WorldWriter writer;
        dbtest::set_standard_meta(writer, 5);
        {
            auto stage = writer.begin_stage("abandoned");
            stage.write_layer(1, "lost", ramp_u8(4, 4));
        }
        CHECK(writer.stages().empty());
        CHECK(writer.next_layer_id() == 1);
        {
            auto stage = writer.begin_stage("kept");
            stage.write_layer(1, "kept", ramp_u8(2, 2));
            stage.commit();
        }
        REQUIRE(writer.finalize(path, Overwrite::refuse).ok());
    }
    dbtest::SqliteFile db(path);
    CHECK(db.rows("SELECT id, name FROM layer ORDER BY id") == Rows{{"1", "kept"}});
    CHECK(db.rows("SELECT value FROM meta WHERE key = 'stages'") == Rows{{"kept"}});
}

TEST_CASE("STRICT: a REAL is rejected where it is not lossless, and no stored value is real (12.2, 14.1)") {
    dbtest::TempDir dir("strict");
    const std::string path = dir.file("world.db");
    const Bytes blob = worldgen::db::encode_layer_blob(ramp_u8(2, 1));
    std::string hex;
    for (const std::uint8_t byte : blob) {
        const char* digits = "0123456789ABCDEF";
        hex += digits[byte >> 4U];
        hex += digits[byte & 0x0FU];
    }
    {
        WorldWriter writer;
        dbtest::set_standard_meta(writer, 9);
        // 1.5 into layer.width fails.
        auto s1 = WriterAccess::execute(
            writer, "INSERT INTO layer (id, name, width, height, format, data) VALUES (1, 'r', 1.5, 1, 'u8', X'" + hex +
                        "')");
        CHECK_FALSE(s1.ok());
        if (!s1.ok()) {
            CHECK(s1.code() == ErrorCode::sql_rejected);
        }
        // A REAL into layer.data fails.
        auto s2 = WriterAccess::execute(
            writer, "INSERT INTO layer (id, name, width, height, format, data) VALUES (1, 'r', 2, 1, 'u8', 1.5)");
        CHECK_FALSE(s2.ok());
        if (!s2.ok()) {
            CHECK(s2.code() == ErrorCode::sql_rejected);
        }
        // Non-integral REALs into the other INTEGER columns fail too.
        CHECK_FALSE(WriterAccess::execute(writer, "INSERT INTO layer (id, name, width, height, format, data) VALUES "
                                                  "(1, 'r', 2, 0.25, 'u8', X'" + hex + "')")
                        .ok());
        // Lossless coercions are accepted (14.1): 2.0 into INTEGER is stored
        // as integer 2.
        CHECK(WriterAccess::execute(writer, "INSERT INTO layer (id, name, width, height, format, data) VALUES "
                                            "(1, 'r', 2.0, 1.0, 'u8', X'" + hex + "')")
                  .ok());
        REQUIRE(writer.finalize(path, Overwrite::refuse).ok());
    }
    dbtest::SqliteFile db(path);
    CHECK(db.rows("SELECT typeof(width), width, typeof(height), height FROM layer") ==
          Rows{{"integer", "2", "integer", "1"}});
    check_no_real_values(path);
    // The row is valid for the reader as well.
    auto reader = WorldReader::open(path);
    REQUIRE(reader.ok());
    CHECK(reader.value().read_layer("r").ok());
}

TEST_CASE("a layer row that disagrees with its blob is corrupt for list and read (9, 12.2, 14.3)") {
    dbtest::TempDir dir("corrupt-layer");
    const std::string path = dir.file("world.db");
    const Bytes ramp22 = worldgen::db::encode_layer_blob(ramp_u8(2, 2));
    Bytes bad_payload = ramp22;
    bad_payload.push_back(0x00);  // trailing byte; header length now wrong too
    // Header intact, payload corrupt: payload length field adjusted.
    bad_payload[16] = static_cast<std::uint8_t>(bad_payload[16] + 1U);

    struct Case {
        const char* label;
        std::int64_t width;
        std::int64_t height;
        const char* format;
        Bytes data;
        bool list_fails;
    };
    const std::vector<Case> cases = {
        {"width differs", 3, 2, "u8", ramp22, true},
        {"height differs", 2, 1, "u8", ramp22, true},
        {"type differs", 2, 2, "u16", ramp22, true},
        {"unknown format text", 2, 2, "f32", ramp22, true},
        {"header garbage", 2, 2, "u8", Bytes(30, 0x41), true},
        {"blob too short", 2, 2, "u8", Bytes(10, 0x00), true},
        {"payload corrupt", 2, 2, "u8", bad_payload, false},
    };
    int index = 0;
    for (const Case& c : cases) {
        CAPTURE(c.label);
        const std::string file = dir.file("c" + std::to_string(index++) + ".db");
        {
            WorldWriter writer;
            dbtest::set_standard_meta(writer, 1);
            auto stage = writer.begin_stage("corrupt");
            stage.write_layer(1, "good", ramp_u8(3, 3));
            WriterAccess::insert_layer_row(stage, 2, "test.bad", c.width, c.height, c.format, c.data);
            stage.commit();
            REQUIRE(writer.finalize(file, Overwrite::refuse).ok());
        }
        auto opened = WorldReader::open(file);
        REQUIRE(opened.ok());
        const auto list = opened.value().layers();
        if (c.list_fails) {
            REQUIRE_FALSE(list.ok());
            CHECK(list.code() == ErrorCode::corrupt_layer);
            CHECK(list.error().message.find("test.bad") != std::string::npos);
        } else {
            // The list does not read or decode the payload (9, 14.3).
            CHECK(list.ok());
        }
        const auto read = opened.value().read_layer("test.bad");
        REQUIRE_FALSE(read.ok());
        CHECK(read.code() == ErrorCode::corrupt_layer);
        CHECK(read.error().message.find("test.bad") != std::string::npos);
        // The good layer still reads.
        CHECK(opened.value().read_layer("good").ok());
    }
}

TEST_CASE("reader errors are distinct and in the order of section 9 (12.2, 14.2)") {
    dbtest::TempDir dir("reader-errors");
    const std::string base = dir.file("base.db");
    REQUIRE(dbtest::write_empty_world(base, 42).ok());
    const Bytes world = dbtest::read_file(base);

    auto variant = [&](const std::string& name, const std::string& sql) {
        const std::string path = dir.file(name);
        dbtest::write_file(path, world);
        const auto s = worldgen::db::testing::execute_on_file(path, sql);
        REQUIRE_MESSAGE(s.ok(), (s.ok() ? std::string() : s.error().message));
        return path;
    };
    auto open_code = [](const std::string& path) {
        const auto r = WorldReader::open(path);
        if (r.ok()) {
            return std::string("ok");
        }
        CHECK_FALSE(r.error().message.empty());
        return std::string(worldgen::db::error_code_name(r.code()));
    };
    auto name = [](ErrorCode c) { return std::string(worldgen::db::error_code_name(c)); };

    CHECK(open_code(base) == "ok");

    SUBCASE("missing file") { CHECK(open_code(dir.file("missing.db")) == name(ErrorCode::file_unreadable)); }
    SUBCASE("a directory") { CHECK(open_code(worldgen::db::path_to_utf8(dir.path())) == name(ErrorCode::file_unreadable)); }
    SUBCASE("a text file") {
        const std::string path = dir.file("text.db");
        const std::string text = "This is not a database.\n";
        dbtest::write_file(path, Bytes(text.begin(), text.end()));
        CHECK(open_code(path) == name(ErrorCode::not_sqlite));
    }
    SUBCASE("an empty file") {
        const std::string path = dir.file("empty.db");
        dbtest::write_file(path, {});
        CHECK(open_code(path) == name(ErrorCode::not_sqlite));
    }
    SUBCASE("SQLite magic followed by garbage (14.2)") {
        Bytes garbage(4096);
        worldgen::Rng rng(77);
        for (auto& g : garbage) {
            g = static_cast<std::uint8_t>(rng.next() >> 56U);
        }
        const char magic[] = "SQLite format 3";
        for (std::size_t i = 0; i < 16; ++i) {
            garbage[i] = static_cast<std::uint8_t>(magic[i]);
        }
        const std::string path = dir.file("garbage.db");
        dbtest::write_file(path, garbage);
        CHECK(open_code(path) == name(ErrorCode::not_world));
    }
    SUBCASE("a different application_id") {
        CHECK(open_code(variant("appid.db", "PRAGMA application_id = 1")) == name(ErrorCode::not_world));
    }
    SUBCASE("an SQLite file that is not a world at all") {
        const std::string path = dir.file("plain.db");
        dbtest::write_file(path, world);
        REQUIRE(worldgen::db::testing::execute_on_file(path, "DROP TABLE meta; PRAGMA application_id = 0").ok());
        CHECK(open_code(path) == name(ErrorCode::not_world));
    }
    SUBCASE("no meta table (14.2)") {
        CHECK(open_code(variant("nometa.db", "DROP TABLE meta")) == name(ErrorCode::not_world));
    }
    SUBCASE("meta.format missing or wrong") {
        CHECK(open_code(variant("noformat.db", "DELETE FROM meta WHERE key = 'format'")) == name(ErrorCode::not_world));
        CHECK(open_code(variant("badformat.db", "UPDATE meta SET value = 'the-unmapped-worlds' WHERE key = 'format'")) ==
              name(ErrorCode::not_world));
    }
    SUBCASE("a file in WAL mode, without creating -wal or -shm (14.2)") {
        const std::string path = variant("wal.db", "PRAGMA journal_mode = WAL");
        const auto before = dbtest::dir_entries(dir.path());
        CHECK(open_code(path) == name(ErrorCode::not_world));
        CHECK(dbtest::dir_entries(dir.path()) == before);
    }
    SUBCASE("user_version 3") {
        const auto r = WorldReader::open(variant("uv3.db", "PRAGMA user_version = 3"));
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == ErrorCode::unsupported_schema_version);
        // "world schema version X, this build reads Y"
        CHECK(r.error().message.find('3') != std::string::npos);
        CHECK(r.error().message.find('2') != std::string::npos);
    }
    SUBCASE("meta.schema_version 3") {
        CHECK(open_code(variant("msv3.db", "UPDATE meta SET value = '3' WHERE key = 'schema_version'")) ==
              name(ErrorCode::unsupported_schema_version));
    }
    SUBCASE("a world missing seed") {
        const auto r = WorldReader::open(variant("noseed.db", "DELETE FROM meta WHERE key = 'seed'"));
        REQUIRE_FALSE(r.ok());
        CHECK(r.code() == ErrorCode::meta_key_missing);
        CHECK(r.error().message.find("seed") != std::string::npos);
    }
    SUBCASE("every required key missing in turn") {
        for (const std::string_view key : worldgen::db::REQUIRED_META_KEYS) {
            if (key == "format" || key == "schema_version") {
                continue;  // these give not_world / unsupported_schema_version first
            }
            CAPTURE(key);
            CHECK(open_code(variant("no-" + std::string(key) + ".db",
                                    "DELETE FROM meta WHERE key = '" + std::string(key) + "'")) ==
                  name(ErrorCode::meta_key_missing));
        }
    }
    SUBCASE("a required value not in canonical form (14.2, 14.5)") {
        const std::pair<const char*, const char*> bad[] = {
            {"seed", "042"},
            {"seed", "18446744073709551616"},
            {"seed", "-1"},
            {"seed", ""},
            {"param.map_size", "1000"},
            {"param.map_size", "01024"},
            {"generator_version", "0.1"},
            {"generator_version", "0.01.0"},
            {"content_hash", "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855"},
            {"content_hash", "e3b0"},
            {"seed_phrase", " untrimmed"},
            {"stages", "Bad"},
        };
        int i = 0;
        for (const auto& [key, value] : bad) {
            CAPTURE(key);
            CAPTURE(value);
            CHECK(open_code(variant("inv" + std::to_string(i++) + ".db",
                                    std::string("UPDATE meta SET value = '") + value + "' WHERE key = '" + key + "'")) ==
                  name(ErrorCode::meta_value_invalid));
        }
    }
    SUBCASE("a seed_phrase that is not a scrubbed phrase of at most 256 bytes (amendment 12)") {
        // Each value is an SQL expression, so that control and non-ASCII
        // bytes can be written with char() and X'..'.
        const std::string bad[] = {
            "'The Unmapped'",                         // capitals
            "'the-unmapped'",                         // punctuation
            "'the unmapped!'",
            "'the  unmapped'",                        // double space
            "' the unmapped'",                        // leading space
            "'the unmapped '",                        // trailing space
            "' '",
            "'the' || char(9) || 'unmapped'",         // TAB
            "'the unmapped' || char(10)",             // LF
            "'a' || char(0) || 'b'",                  // NUL
            "CAST(X'C39C6E6D6170706564' AS TEXT)",    // "Ünmapped"
            "'" + std::string(257, 'a') + "'",        // 257 bytes, scrubbed form
            "'" + std::string(128, 'a') + " " + std::string(128, 'b') + "'",
        };
        int i = 0;
        for (const std::string& value : bad) {
            CAPTURE(value);
            CHECK(open_code(variant("phr" + std::to_string(i++) + ".db",
                                    "UPDATE meta SET value = " + value + " WHERE key = 'seed_phrase'")) ==
                  name(ErrorCode::meta_value_invalid));
        }
        // Scrubbed phrases are accepted; the reader does not recompute the
        // seed, so the numeric seed of the base world is kept as it is.
        const std::string good[] = {"the unmapped", "42", "a", "nmapped w rld", std::string(256, 'a'),
                                    std::string(127, 'a') + " " + std::string(128, 'b')};
        for (const std::string& phrase : good) {
            CAPTURE(phrase);
            const std::string path = variant("phr" + std::to_string(i++) + ".db",
                                             "UPDATE meta SET value = '" + phrase + "' WHERE key = 'seed_phrase'");
            auto r = WorldReader::open(path);
            REQUIRE(r.ok());
            CHECK(r.value().seed_phrase() == phrase);
        }
    }
    SUBCASE("unknown meta keys are tolerated (5.5)") {
        const auto path = variant("extra.db", "INSERT INTO meta VALUES ('zz.future', 'x')");
        CHECK(open_code(path) == "ok");
        auto r = WorldReader::open(path);
        REQUIRE(r.ok());
        CHECK(r.value().meta("zz.future") == std::optional<std::string>("x"));
        CHECK(r.value().meta_entries().size() == 9);
    }
    SUBCASE("validation order: the earlier check wins") {
        CHECK(open_code(variant("o1.db", "PRAGMA application_id = 1; PRAGMA user_version = 3")) ==
              name(ErrorCode::not_world));
        CHECK(open_code(variant("o2.db", "PRAGMA user_version = 3; DELETE FROM meta WHERE key = 'seed'")) ==
              name(ErrorCode::unsupported_schema_version));
        CHECK(open_code(variant("o3.db", "DELETE FROM meta WHERE key = 'seed'; UPDATE meta SET value = '7' WHERE key "
                                         "= 'param.map_size'")) == name(ErrorCode::meta_key_missing));
    }
    SUBCASE("the codes are distinct") {
        const ErrorCode codes[] = {ErrorCode::file_unreadable,  ErrorCode::not_sqlite,
                                   ErrorCode::not_world,        ErrorCode::unsupported_schema_version,
                                   ErrorCode::meta_key_missing, ErrorCode::meta_value_invalid,
                                   ErrorCode::layer_not_found,  ErrorCode::corrupt_layer};
        std::set<std::string> names;
        for (const ErrorCode c : codes) {
            names.insert(name(c));
        }
        CHECK(names.size() == 8);
    }
}

TEST_CASE("reader never writes: bytes and modification time unchanged (12.2)") {
    dbtest::TempDir dir("reader-readonly");
    const std::string path = dir.file("world.db");
    write_layered_world(path, 11);
    const fs::path fpath = worldgen::db::path_from_utf8(path);
    // Pin the modification time to a fixed point in the past so that a write
    // would show regardless of the file system's timestamp resolution.
    const auto pinned = fs::last_write_time(fpath) - std::chrono::hours(24 * 30);
    fs::last_write_time(fpath, pinned);
    const Bytes before = dbtest::read_file(path);
    {
        auto opened = WorldReader::open(path);
        REQUIRE(opened.ok());
        WorldReader& reader = opened.value();
        (void)reader.meta_entries();
        (void)reader.meta("seed");
        REQUIRE(reader.layers().ok());
        REQUIRE(reader.read_layer("test.ramp").ok());
        REQUIRE(reader.read_layer("test.noise").ok());
        CHECK_FALSE(reader.read_layer("absent").ok());
        CHECK(dbtest::dir_entries(dir.path()) == std::set<std::string>{"world.db"});
        reader.close();
        // After close the meta accessors still work, layer calls fail.
        CHECK(reader.seed() == 11);
        CHECK_FALSE(reader.layers().ok());
    }
    CHECK(dbtest::read_file(path) == before);
    CHECK(fs::last_write_time(fpath) == pinned);
    check_only(dir.path(), {"world.db"});
}

TEST_CASE("failure cleanup: a run forced to fail leaves no .partial and no output (6, 10, 12.2)") {
    using worldgen::db::testing::FailureMode;
    using worldgen::db::testing::FailurePoint;
    const FailurePoint points[] = {FailurePoint::after_meta, FailurePoint::after_integrity_check,
                                   FailurePoint::after_vacuum, FailurePoint::after_close,
                                   FailurePoint::after_verify};
    dbtest::TempDir dir("failure");
    int index = 0;
    for (const FailurePoint point : points) {
        for (const FailureMode mode : {FailureMode::throw_fatal, FailureMode::return_error}) {
            CAPTURE(static_cast<int>(point));
            CAPTURE(static_cast<int>(mode));
            const std::string sub = "f" + std::to_string(index++);
            fs::create_directory(dir.path() / sub);
            const std::string out = dir.file(sub + "/world.db");
            {
                WorldWriter writer;
                dbtest::set_standard_meta(writer, 1);
                {
                    auto stage = writer.begin_stage("stage");
                    stage.write_layer(1, "test.ramp", ramp_u8(64, 64));
                    stage.commit();
                }
                WriterAccess::set_failure_point(writer, point, mode);
                if (mode == FailureMode::throw_fatal) {
                    CHECK_THROWS_AS((void)writer.finalize(out, Overwrite::refuse), FatalError);
                } else {
                    const auto status = writer.finalize(out, Overwrite::refuse);
                    REQUIRE_FALSE(status.ok());
                    CHECK(status.code() == ErrorCode::output_write_failed);
                }
            }
            CHECK_FALSE(dbtest::exists(out));
            CHECK_FALSE(dbtest::exists(out + ".partial"));
        CHECK_FALSE(dbtest::exists(out + ".partial-journal"));
            CHECK_FALSE(dbtest::exists(out + ".partial-journal"));
            check_only(dir.path() / sub, {});
        }
    }
}

TEST_CASE("failure cleanup: an existing output is untouched by a failed replace") {
    using worldgen::db::testing::FailureMode;
    using worldgen::db::testing::FailurePoint;
    dbtest::TempDir dir("failure-existing");
    {
        const std::string out = dir.file("existing.db");
        REQUIRE(dbtest::write_empty_world(out, 99).ok());
        const Bytes old = dbtest::read_file(out);
        {
            WorldWriter writer;
            dbtest::set_standard_meta(writer, 100);
            WriterAccess::set_failure_point(writer, FailurePoint::after_verify, FailureMode::return_error);
            CHECK_FALSE(writer.finalize(out, Overwrite::replace).ok());
        }
        CHECK(dbtest::read_file(out) == old);
        CHECK_FALSE(dbtest::exists(out + ".partial"));
    }
}

TEST_CASE("output location: exists, replace, missing parent, stale .partial (6 step 9, 14.7)") {
    dbtest::TempDir dir("output");
    const std::string out = dir.file("world.db");
    REQUIRE(dbtest::write_empty_world(out, 1).ok());
    const Bytes first = dbtest::read_file(out);

    SUBCASE("refuse keeps the old file and removes a stale .partial") {
        dbtest::write_file(out + ".partial", Bytes{1, 2, 3});
        CHECK(WorldWriter::check_output(out, Overwrite::refuse).code() == ErrorCode::output_exists);
        const auto status = dbtest::write_empty_world(out, 2);
        REQUIRE_FALSE(status.ok());
        CHECK(status.code() == ErrorCode::output_exists);
        CHECK(dbtest::read_file(out) == first);
        check_only(dir.path(), {"world.db"});
    }
    SUBCASE("replace overwrites, and a stale .partial is not reused") {
        dbtest::write_file(out + ".partial", Bytes(5000, 0xAB));
        CHECK(WorldWriter::check_output(out, Overwrite::replace).ok());
        WorldWriter writer;
        dbtest::set_standard_meta(writer, 2);
        REQUIRE(writer.finalize(out, Overwrite::replace).ok());
        check_only(dir.path(), {"world.db"});
        auto reader = WorldReader::open(out);
        REQUIRE(reader.ok());
        CHECK(reader.value().seed() == 2);
        // Identical to a fresh write of seed 2.
        const std::string fresh = dir.file("fresh.db");
        REQUIRE(dbtest::write_empty_world(fresh, 2).ok());
        CHECK(dbtest::read_file(fresh) == dbtest::read_file(out));
    }
    SUBCASE("missing parent directory") {
        const std::string nowhere = dir.file("no/such/dir/world.db");
        CHECK(WorldWriter::check_output(nowhere, Overwrite::refuse).code() == ErrorCode::output_parent_missing);
        const auto status = dbtest::write_empty_world(nowhere, 3);
        REQUIRE_FALSE(status.ok());
        CHECK(status.code() == ErrorCode::output_parent_missing);
        check_only(dir.path(), {"world.db"});
    }
}

TEST_CASE("a world written to a non-ASCII file name reads back (P0-04 4.1)") {
    dbtest::TempDir dir("non-ascii");
    // "wörld-ünmapped-地図.db" in UTF-8.
    const std::string name = "w\xC3\xB6rld-\xC3\xBCnmapped-\xE5\x9C\xB0\xE5\x9B\xB3.db";
    const std::string path = dir.file(name);
    write_layered_world(path, 42);
    CHECK(fs::exists(dir.path() / worldgen::db::path_from_utf8(name)));
    check_only(dir.path(), {name});
    check_header(path);
    auto reader = WorldReader::open(path);
    REQUIRE(reader.ok());
    CHECK(reader.value().seed() == 42);
    CHECK(reader.value().read_layer("test.ramp").ok());
}

}  // TEST_SUITE
