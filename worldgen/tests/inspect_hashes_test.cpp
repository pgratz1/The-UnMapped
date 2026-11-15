// File and logical hashes (spec P0-04 sections 8 and 12.5) and raw row
// access of the reader (P0-02 amendment 10), with the amendments of P0-04
// section 14. Every expected logical hash is built here by hand from the
// encodings of section 8 (p0_04_reference.hpp).

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <ostream>
#include <string>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

#include "db_test_support.hpp"
#include "p0_04_reference.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/generate.hpp"
#include "worldgen/inspect/hashes.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using worldgen::FatalError;
using worldgen::to_hex;
using worldgen::db::Codec;
using worldgen::db::ColumnValue;
using worldgen::db::ErrorCode;
using worldgen::db::Overwrite;
using worldgen::db::ValueType;
using worldgen::db::WorldReader;
using worldgen::inspect::detail_hashes;
using worldgen::inspect::layer_hash;

WorldReader open_world(const std::string& path) {
    auto r = WorldReader::open(path);
    INFO("open: ", r.ok() ? std::string() : r.error().message);
    REQUIRE(r.ok());
    return std::move(r).value();
}

worldgen::WorldParams params_for(std::uint64_t seed, bool pattern) {
    worldgen::WorldParams p;
    p.seed = seed;
    p.content_hash = worldgen::sha256(std::string_view{});
    p.test_pattern = pattern;
    return p;
}

ColumnValue text(std::string_view s) { return {ValueType::text, 0, std::string(s)}; }
ColumnValue integer(std::int64_t v) { return {ValueType::integer, v, {}}; }
ColumnValue null_value() { return {ValueType::null, 0, {}}; }
ColumnValue blob(const std::vector<std::uint8_t>& b) { return {ValueType::blob, 0, std::string(b.begin(), b.end())}; }

// layer_hash by hand for any element type.
template <class T>
worldgen::Sha256Digest hand_layer_hash(std::uint32_t w, std::uint32_t h, std::uint8_t code, const std::vector<T>& v) {
    p004::Encoder e;
    e.u32(w);
    e.u32(h);
    e.u8(code);
    for (const T x : v) {
        auto u = static_cast<std::uint64_t>(static_cast<std::make_unsigned_t<T>>(x));
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            e.u8(static_cast<std::uint8_t>(u >> (8 * i)));
        }
    }
    return e.digest();
}

}  // namespace

TEST_CASE("hashes 8: hash_file is SHA-256 of the bytes") {
    dbtest::TempDir dir("p004-hash-file");
    const std::string world = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(0, false), world, Overwrite::refuse).ok());
    const auto h = worldgen::inspect::hash_file(world);
    REQUIRE(h.ok());
    CHECK(to_hex(h.value()) == dbtest::file_sha256(world));
    dbtest::write_file(dir.file("abc"), {'a', 'b', 'c'});
    CHECK(to_hex(worldgen::inspect::hash_file(dir.file("abc")).value()) ==
          "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    dbtest::write_file(dir.file("empty"), {});
    CHECK(to_hex(worldgen::inspect::hash_file(dir.file("empty")).value()) == p004::EMPTY_CONTENT_HASH);
    // Larger than any plausible read buffer, not a multiple of 64.
    std::vector<std::uint8_t> big(1000003);
    for (std::size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<std::uint8_t>(i * 31 + 7);
    }
    dbtest::write_file(dir.file("big"), big);
    CHECK(to_hex(worldgen::inspect::hash_file(dir.file("big")).value()) == worldgen::sha256_hex(big));

    const auto missing = worldgen::inspect::hash_file(dir.file("missing"));
    REQUIRE(!missing.ok());
    CHECK(missing.code() == ErrorCode::file_unreadable);
    const auto directory = worldgen::inspect::hash_file(worldgen::db::path_to_utf8(dir.path()));
    REQUIRE(!directory.ok());
    CHECK(directory.code() == ErrorCode::file_unreadable);
}

TEST_CASE("hashes 8: layer_hash by hand, every element type") {
    using p004::grid_of;
    const std::vector<std::uint8_t> u8v = {0, 1, 255};
    CHECK(layer_hash(grid_of(3, 1, u8v)) == hand_layer_hash<std::uint8_t>(3, 1, 1, u8v));
    const std::vector<std::int8_t> i8v = {-128, 0, 127, -1};
    CHECK(layer_hash(grid_of(2, 2, i8v)) == hand_layer_hash<std::int8_t>(2, 2, 2, i8v));
    const std::vector<std::uint16_t> u16v = {0x1234, 0xFFFF};
    CHECK(layer_hash(grid_of(1, 2, u16v)) == hand_layer_hash<std::uint16_t>(1, 2, 3, u16v));
    const std::vector<std::int16_t> i16v = {-32767, 0, 32767};
    CHECK(layer_hash(grid_of(3, 1, i16v)) == hand_layer_hash<std::int16_t>(3, 1, 4, i16v));
    CHECK(layer_hash(grid_of(3, 1, i16v)) == p004::layer_hash_i16(3, 1, i16v));
    const std::vector<std::uint32_t> u32v = {0xDEADBEEF};
    CHECK(layer_hash(grid_of(1, 1, u32v)) == hand_layer_hash<std::uint32_t>(1, 1, 5, u32v));
    const std::vector<std::int32_t> i32v = {std::numeric_limits<std::int32_t>::min(), 5};
    CHECK(layer_hash(grid_of(2, 1, i32v)) == hand_layer_hash<std::int32_t>(2, 1, 6, i32v));
    const std::vector<std::uint64_t> u64v = {0x0102030405060708ULL};
    CHECK(layer_hash(grid_of(1, 1, u64v)) == hand_layer_hash<std::uint64_t>(1, 1, 7, u64v));
    const std::vector<std::int64_t> i64v = {std::numeric_limits<std::int64_t>::min(), -2};
    CHECK(layer_hash(grid_of(1, 2, i64v)) == hand_layer_hash<std::int64_t>(1, 2, 8, i64v));
    // The exact bytes of a tiny layer, written out: width 2, height 1, u16,
    // values 0x1234, 0x00FF.
    const std::vector<std::uint8_t> bytes = {2, 0, 0, 0, 1, 0, 0, 0, 3, 0x34, 0x12, 0xFF, 0x00};
    CHECK(layer_hash(grid_of<std::uint16_t>(2, 1, {0x1234, 0x00FF})) == worldgen::sha256(bytes));
    // Width and height are part of the hash.
    CHECK(layer_hash(grid_of<std::uint8_t>(2, 1, {1, 2})) != layer_hash(grid_of<std::uint8_t>(1, 2, {1, 2})));
    CHECK_THROWS_AS((void)layer_hash(grid_of<std::uint8_t>(2, 2, {1})), FatalError);
}

TEST_CASE("hashes 8, 12.5: detail hashes of the empty world") {
    dbtest::TempDir dir("p004-detail-empty");
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(0, false), out, Overwrite::refuse).ok());
    const WorldReader w = open_world(out);
    const auto d = detail_hashes(w);
    REQUIRE(d.ok());
    CHECK(d.value().schema == p004::p0_schema_hash());
    REQUIRE(d.value().tables.size() == 2);
    CHECK(d.value().tables[0].name == "layer");
    CHECK(d.value().tables[0].digest == p004::layer_table_hash({}));
    CHECK(d.value().tables[1].name == "meta");
    // The table meta hash built by hand over the eight known rows (12.5).
    CHECK(d.value().tables[1].digest == p004::meta_table_hash(p004::meta_rows(0)));
    CHECK(d.value().layers.empty());

    // The --detail text.
    const auto file = worldgen::inspect::hash_file(out);
    REQUIRE(file.ok());
    const std::string expected = "file " + to_hex(file.value()) + "\nschema " + to_hex(p004::p0_schema_hash()) +
                                 "\ntable layer " + to_hex(p004::layer_table_hash({})) + "\ntable meta " +
                                 to_hex(p004::meta_table_hash(p004::meta_rows(0))) + "\n";
    CHECK(worldgen::inspect::format_detail(file.value(), d.value()) == expected);
}

TEST_CASE("hashes 8: table meta by hand for other meta contents") {
    dbtest::TempDir dir("p004-detail-meta");
    const worldgen::PhraseSeed ps = worldgen::seed_from_phrase("\xC3\x9Cnmapped W\xC3\xB6rld");
    REQUIRE(ps.ok());
    worldgen::WorldParams p = params_for(ps.seed, true);
    p.seed_phrase = ps.phrase;
    p.map_size = 512;
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(p, out, Overwrite::refuse).ok());
    const auto d = detail_hashes(open_world(out));
    REQUIRE(d.ok());
    REQUIRE(d.value().tables.size() == 2);
    CHECK(d.value().tables[1].digest ==
          p004::meta_table_hash(p004::meta_rows(ps.seed, ps.phrase, "test_pattern", "512")));
}

TEST_CASE("hashes 8, 12.5: detail hashes of a test-pattern world") {
    dbtest::TempDir dir("p004-detail-pattern");
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(0, true), out, Overwrite::refuse).ok());
    const auto d = detail_hashes(open_world(out));
    REQUIRE(d.ok());
    const auto ramp = p004::ramp_cells();
    const auto hash = p004::hash_cells(0);
    const auto noise = p004::noise_cells(0);
    CHECK(d.value().schema == p004::p0_schema_hash());
    const std::vector<p004::LayerRow> rows = {
        {1, "test.ramp", 256, 256, "u8", worldgen::db::encode_layer_blob(p004::grid_of(256, 256, ramp))},
        {2, "test.hash", 256, 256, "u8", worldgen::db::encode_layer_blob(p004::grid_of(256, 256, hash))},
        {3, "test.noise", 256, 256, "i16", worldgen::db::encode_layer_blob(p004::grid_of(256, 256, noise))},
    };
    REQUIRE(d.value().tables.size() == 2);
    CHECK(d.value().tables[0].digest == p004::layer_table_hash(rows));
    CHECK(d.value().tables[1].digest == p004::meta_table_hash(p004::meta_rows(0, "", "test_pattern")));
    REQUIRE(d.value().layers.size() == 3);
    CHECK(d.value().layers[0].name == "test.ramp");
    CHECK(d.value().layers[0].digest == p004::layer_hash_u8(256, 256, ramp));
    CHECK(d.value().layers[1].name == "test.hash");
    CHECK(d.value().layers[1].digest == p004::layer_hash_u8(256, 256, hash));
    CHECK(d.value().layers[2].name == "test.noise");
    CHECK(d.value().layers[2].digest == p004::layer_hash_i16(256, 256, noise));
    std::string listing;
    for (const auto& l : d.value().layers) {
        listing += "layer " + l.name + " " + to_hex(l.digest) + "\n";
    }
    MESSAGE("seed-0 test-pattern layer hashes:\n", listing);
}

TEST_CASE("hashes 8, 12.5: the layer hash is codec-independent") {
    dbtest::TempDir dir("p004-detail-codec");
    using worldgen::stages::testing::FixtureLayer;
    std::vector<std::int32_t> cells(11 * 3);
    for (std::size_t i = 0; i < cells.size(); ++i) {
        cells[i] = static_cast<std::int32_t>(i * i * 7919) - 50000;
    }
    std::vector<FixtureLayer> layers;
    layers.push_back({"test.raw", p004::grid_of(11, 3, cells), Codec::raw});
    layers.push_back({"test.varint", p004::grid_of(11, 3, cells), Codec::delta_varint});
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(out, 0, layers).ok());
    const WorldReader w = open_world(out);
    const auto list = w.layers();
    REQUIRE(list.ok());
    REQUIRE(list.value().size() == 2);
    CHECK(list.value()[0].codec == Codec::raw);
    CHECK(list.value()[1].codec == Codec::delta_varint);
    CHECK(list.value()[0].encoded_size != list.value()[1].encoded_size);
    const auto d = detail_hashes(w);
    REQUIRE(d.ok());
    REQUIRE(d.value().layers.size() == 2);
    CHECK(d.value().layers[0].digest == d.value().layers[1].digest);
    CHECK(d.value().layers[0].digest == hand_layer_hash<std::int32_t>(11, 3, 6, cells));
}

TEST_CASE("hashes 8: is_sha256_hex") {
    const std::string lower(p004::EMPTY_CONTENT_HASH);
    std::string upper = lower;
    for (char& c : upper) {
        if (c >= 'a' && c <= 'f') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    CHECK(worldgen::inspect::is_sha256_hex(lower));
    CHECK(worldgen::inspect::is_sha256_hex(upper));
    CHECK(worldgen::inspect::is_sha256_hex(std::string(64, '0')));
    CHECK(!worldgen::inspect::is_sha256_hex(""));
    CHECK(!worldgen::inspect::is_sha256_hex(lower.substr(1)));
    CHECK(!worldgen::inspect::is_sha256_hex(lower + "0"));
    CHECK(!worldgen::inspect::is_sha256_hex("g" + lower.substr(1)));
    CHECK(!worldgen::inspect::is_sha256_hex(" " + lower.substr(1)));
    CHECK(!worldgen::inspect::is_sha256_hex(lower.substr(0, 63) + "\n"));
}

TEST_CASE("hashes 8: REAL values and tables without a primary key are errors") {
    dbtest::TempDir dir("p004-detail-errors");
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(0, false), out, Overwrite::refuse).ok());
    SUBCASE("REAL") {
        REQUIRE(worldgen::db::testing::execute_on_file(
                    out, "CREATE TABLE extra (id INTEGER PRIMARY KEY, v ANY) STRICT; INSERT INTO extra VALUES (1, 2), "
                         "(2, 0.5)")
                    .ok());
        const WorldReader w = open_world(out);
        const auto d = detail_hashes(w);
        REQUIRE(!d.ok());
        CHECK(d.code() == ErrorCode::real_value);
        const auto rows = w.table_rows("extra");
        REQUIRE(!rows.ok());
        CHECK(rows.code() == ErrorCode::real_value);
        // Rows before the REAL are visited first (reader.hpp).
        int visited = 0;
        const auto st = w.visit_table_rows("extra", [&](std::span<const ColumnValue>) { ++visited; });
        CHECK(!st.ok());
        CHECK(visited == 1);
    }
    SUBCASE("no primary key") {
        REQUIRE(worldgen::db::testing::execute_on_file(out, "CREATE TABLE extra (v INTEGER NOT NULL) STRICT").ok());
        const WorldReader w = open_world(out);
        const auto d = detail_hashes(w);
        REQUIRE(!d.ok());
        CHECK(d.code() == ErrorCode::no_primary_key);
        const auto pk = w.primary_key_columns("extra");
        REQUIRE(!pk.ok());
        CHECK(pk.code() == ErrorCode::no_primary_key);
    }
    SUBCASE("sqlite_* tables (sqlite_stat1 from ANALYZE) are excluded") {
        REQUIRE(worldgen::db::testing::execute_on_file(out, "ANALYZE").ok());
        const auto d = detail_hashes(open_world(out));
        REQUIRE(d.ok());
        REQUIRE(d.value().tables.size() == 2);
        CHECK(d.value().tables[0].name == "layer");
        CHECK(d.value().tables[1].name == "meta");
    }
    SUBCASE("corrupt layer") {
        worldgen::db::WorldWriter writer;
        dbtest::set_standard_meta(writer, 0);
        {
            worldgen::db::StageWriter s = writer.begin_stage("broken");
            const std::vector<std::uint8_t> junk = {9, 9, 9, 9};
            worldgen::db::testing::WriterAccess::insert_layer_row(s, 1, "test.junk", 1, 1, "u8", junk);
            s.commit();
        }
        const std::string bad = dir.file("bad.db");
        REQUIRE(writer.finalize(bad, Overwrite::refuse).ok());
        const auto d = detail_hashes(open_world(bad));
        REQUIRE(!d.ok());
        CHECK(d.code() == ErrorCode::corrupt_layer);
    }
}

TEST_CASE("reader raw rows (P0-02 amendment 10): tables, columns, keys, rows") {
    dbtest::TempDir dir("p004-raw-rows");
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(0, true), out, Overwrite::refuse).ok());
    const WorldReader w = open_world(out);

    const auto tables = w.tables();
    REQUIRE(tables.ok());
    REQUIRE(tables.value().size() == 2);
    CHECK(tables.value()[0].name == "layer");
    CHECK(tables.value()[0].row_count == 3U);
    CHECK(tables.value()[1].name == "meta");
    CHECK(tables.value()[1].row_count == 8U);

    CHECK(w.table_columns("meta").value() == std::vector<std::string>{"key", "value"});
    CHECK(w.table_columns("layer").value() ==
          std::vector<std::string>{"id", "name", "width", "height", "format", "data"});
    CHECK(w.primary_key_columns("meta").value() == std::vector<std::string>{"key"});
    CHECK(w.primary_key_columns("layer").value() == std::vector<std::string>{"id"});
    for (const char* missing : {"nope", "Meta", "META"}) {
        CAPTURE(missing);
        const auto r = w.table_columns(missing);
        REQUIRE(!r.ok());
        CHECK(r.code() == ErrorCode::table_not_found);
        CHECK(w.table_rows(missing).code() == ErrorCode::table_not_found);
    }

    const auto meta = w.table_rows("meta");
    REQUIRE(meta.ok());
    const p004::MetaRows expected = p004::meta_rows(0, "", "test_pattern");
    REQUIRE(meta.value().size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(meta.value()[i] == std::vector<ColumnValue>{text(expected[i].first), text(expected[i].second)});
    }

    const auto layer = w.table_rows("layer");
    REQUIRE(layer.ok());
    REQUIRE(layer.value().size() == 3);
    const std::vector<ColumnValue> first = {integer(1),   text("test.ramp"), integer(256), integer(256), text("u8"),
                                            blob(worldgen::db::encode_layer_blob(
                                                p004::grid_of(256, 256, p004::ramp_cells())))};
    CHECK(layer.value()[0] == first);
    CHECK(layer.value()[2][4] == text("i16"));

    const auto schema = w.schema_rows();
    REQUIRE(schema.ok());
    const std::vector<std::vector<ColumnValue>> expected_schema = {
        {text("index"), text("sqlite_autoindex_layer_1"), text("layer"), null_value()},
        {text("table"), text("layer"), text("layer"), text(p004::DDL_LAYER)},
        {text("table"), text("meta"), text("meta"), text(p004::DDL_META)},
    };
    CHECK(schema.value() == expected_schema);
    CHECK(worldgen::db::SCHEMA_ROW_COLUMNS == std::array<std::string_view, 4>{"type", "name", "tbl_name", "sql"});

    std::size_t visited = 0;
    REQUIRE(w.visit_schema_rows([&](std::span<const ColumnValue> row) {
                 CHECK(row.size() == 4);
                 ++visited;
             })
                .ok());
    CHECK(visited == 3);
    CHECK(std::string(worldgen::db::sqlite_library_version()) == sqlite3_libversion());
}

TEST_CASE("reader raw rows: primary-key order with BINARY collation, composite keys") {
    dbtest::TempDir dir("p004-raw-order");
    const std::string out = dir.file("w.db");
    REQUIRE(worldgen::generate_world(params_for(0, false), out, Overwrite::refuse).ok());
    REQUIRE(worldgen::db::testing::execute_on_file(
                out,
                "CREATE TABLE pair (a INTEGER NOT NULL, b TEXT NOT NULL, c BLOB, PRIMARY KEY (b, a)) STRICT, WITHOUT "
                "ROWID; INSERT INTO pair VALUES (2, 'b', x'00'), (1, 'b', NULL), (5, 'a', x''), (9, 'B', x'FF01'), "
                "(-3, 'b', x'7F')")
                .ok());
    const WorldReader w = open_world(out);
    CHECK(w.table_columns("pair").value() == std::vector<std::string>{"a", "b", "c"});
    CHECK(w.primary_key_columns("pair").value() == std::vector<std::string>{"b", "a"});
    const auto rows = w.table_rows("pair");
    REQUIRE(rows.ok());
    const std::vector<std::vector<ColumnValue>> expected = {
        {integer(9), text("B"), blob({0xFF, 0x01})},  // 'B' (0x42) sorts before 'a' (0x61)
        {integer(5), text("a"), blob({})},
        {integer(-3), text("b"), blob({0x7F})},
        {integer(1), text("b"), null_value()},
        {integer(2), text("b"), blob({0x00})},
    };
    CHECK(rows.value() == expected);

    // The table hash of section 8 over these rows, by hand.
    p004::Encoder e;
    e.u64(3);
    e.col_integer(9);
    e.col_text("B");
    e.col_blob(std::vector<std::uint8_t>{0xFF, 0x01});
    e.col_integer(5);
    e.col_text("a");
    e.col_blob(std::vector<std::uint8_t>{});
    e.col_integer(-3);
    e.col_text("b");
    e.col_blob(std::vector<std::uint8_t>{0x7F});
    e.col_integer(1);
    e.col_text("b");
    e.col_null();
    e.col_integer(2);
    e.col_text("b");
    e.col_blob(std::vector<std::uint8_t>{0x00});
    const auto d = detail_hashes(w);
    REQUIRE(d.ok());
    REQUIRE(d.value().tables.size() == 3);
    CHECK(d.value().tables[2].name == "pair");
    CHECK(d.value().tables[2].digest == e.digest());
    // The schema hash now covers the new table too (and is no longer the P0 one).
    CHECK(d.value().schema != p004::p0_schema_hash());
}
