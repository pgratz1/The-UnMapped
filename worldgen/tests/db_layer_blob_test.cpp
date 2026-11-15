// Layer blob codec tests, spec P0-02 section 7 and 12.1 (with the decoder
// amendments of 14.4).

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include "worldgen/core/error.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/db/layer_blob.hpp"

namespace {

using worldgen::FatalError;
using worldgen::db::BlobError;
using worldgen::db::Codec;
using worldgen::db::ElementType;
using worldgen::db::LayerGrid;
using Bytes = std::vector<std::uint8_t>;

template <class T>
LayerGrid grid_of(std::uint32_t width, std::uint32_t height, std::vector<T> values) {
    LayerGrid grid;
    grid.width = width;
    grid.height = height;
    grid.values = std::move(values);
    return grid;
}

void put_le(Bytes& out, std::uint64_t value, int size) {
    for (int i = 0; i < size; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFFU));
    }
}

// A blob built by hand from the 7.1 header fields and a payload; the payload
// length field is the real payload length unless overridden.
Bytes make_blob(std::uint8_t type, std::uint8_t codec, std::uint32_t width, std::uint32_t height,
                const Bytes& payload, std::uint16_t version = 1,
                std::uint64_t payload_length = std::numeric_limits<std::uint64_t>::max()) {
    Bytes b = {0x55, 0x4D, 0x4C, 0x59};
    put_le(b, version, 2);
    b.push_back(type);
    b.push_back(codec);
    put_le(b, width, 4);
    put_le(b, height, 4);
    put_le(b, payload_length == std::numeric_limits<std::uint64_t>::max() ? payload.size() : payload_length, 8);
    b.insert(b.end(), payload.begin(), payload.end());
    return b;
}

Bytes payload_of(const Bytes& blob) {
    REQUIRE(blob.size() >= 24);
    return Bytes(blob.begin() + 24, blob.end());
}

BlobError decode_error(const Bytes& blob) {
    return worldgen::db::decode_layer_blob(std::span<const std::uint8_t>(blob)).error;
}

bool grids_equal(const LayerGrid& a, const LayerGrid& b) {
    return a.width == b.width && a.height == b.height && a.values == b.values;
}

// Round trip through both codecs: decode gives the grid back, re-encoding the
// decoded grid gives the same bytes (canonical), the header fields are right.
void check_round_trip(const LayerGrid& grid) {
    for (const Codec codec : {Codec::raw, Codec::delta_varint}) {
        CAPTURE(static_cast<int>(codec));
        const Bytes blob = worldgen::db::encode_layer_blob(grid, codec);
        REQUIRE(blob.size() >= 24);
        const auto header = worldgen::db::parse_blob_header(blob);
        REQUIRE(header.ok());
        CHECK(header.header.type == grid.type());
        CHECK(header.header.codec == codec);
        CHECK(header.header.width == grid.width);
        CHECK(header.header.height == grid.height);
        CHECK(header.header.payload_length == blob.size() - 24);
        if (codec == Codec::raw) {
            CHECK(blob.size() - 24 == grid.cell_count() * worldgen::db::element_size(grid.type()));
        }
        const auto decoded = worldgen::db::decode_layer_blob(blob);
        REQUIRE_MESSAGE(decoded.ok(), worldgen::db::blob_error_name(decoded.error));
        CHECK(decoded.codec == codec);
        CHECK(grids_equal(decoded.grid, grid));
        CHECK(worldgen::db::encode_layer_blob(decoded.grid, codec) == blob);
    }
}

template <class T>
void round_trips_for_type() {
    using L = std::numeric_limits<T>;
    CAPTURE(static_cast<int>(worldgen::db::element_type_of<T>));
    const std::uint32_t w = 3;
    const std::uint32_t h = 7;
    const std::size_t n = 21;

    // All zeros, all maximum values, all minimum values.
    check_round_trip(grid_of<T>(w, h, std::vector<T>(n, T{0})));
    check_round_trip(grid_of<T>(w, h, std::vector<T>(n, L::max())));
    check_round_trip(grid_of<T>(w, h, std::vector<T>(n, L::min())));

    // Alternating minimum and maximum (the delta wraps mod 2^64).
    std::vector<T> alt(n);
    for (std::size_t i = 0; i < n; ++i) {
        alt[i] = (i % 2 == 0) ? L::min() : L::max();
    }
    check_round_trip(grid_of<T>(w, h, alt));
    std::vector<T> alt2(n);
    for (std::size_t i = 0; i < n; ++i) {
        alt2[i] = (i % 2 == 0) ? L::max() : L::min();
    }
    check_round_trip(grid_of<T>(w, h, alt2));

    // A ramp (wrapping for the small types).
    std::vector<T> ramp(1000);
    for (std::size_t i = 0; i < ramp.size(); ++i) {
        ramp[i] = static_cast<T>(i * 7U);
    }
    check_round_trip(grid_of<T>(40, 25, ramp));

    // 10^5 values from a fixed Rng stream.
    worldgen::Rng rng(0x5EEDBA5EULL + static_cast<std::uint64_t>(worldgen::db::element_type_of<T>));
    std::vector<T> random(100000);
    for (auto& v : random) {
        const std::uint64_t r = rng.next();
        v = static_cast<T>(r);
    }
    check_round_trip(grid_of<T>(400, 250, random));

    // Dimensions 1 x 1, 1 x 65536, 65536 x 1, 3 x 7 with mixed values.
    check_round_trip(grid_of<T>(1, 1, std::vector<T>{L::max()}));
    check_round_trip(grid_of<T>(1, 1, std::vector<T>{L::min()}));
    std::vector<T> tall(65536);
    for (std::size_t i = 0; i < tall.size(); ++i) {
        tall[i] = static_cast<T>((i / 300U) % 2U == 0 ? 0U : i);
    }
    check_round_trip(grid_of<T>(1, 65536, tall));
    check_round_trip(grid_of<T>(65536, 1, tall));
    std::vector<T> mixed(n);
    for (std::size_t i = 0; i < n; ++i) {
        mixed[i] = static_cast<T>(i < 5 ? 0U : (i < 9 ? 3U : i * 1000003U));
    }
    check_round_trip(grid_of<T>(w, h, mixed));
}

}  // namespace

TEST_SUITE("db.blob") {

TEST_CASE("element type codes, names and sizes (7.1, 5.6)") {
    struct Row {
        ElementType type;
        std::uint8_t code;
        const char* name;
        std::uint32_t size;
    };
    const Row rows[] = {
        {ElementType::u8, 1, "u8", 1},   {ElementType::i8, 2, "i8", 1},   {ElementType::u16, 3, "u16", 2},
        {ElementType::i16, 4, "i16", 2}, {ElementType::u32, 5, "u32", 4}, {ElementType::i32, 6, "i32", 4},
        {ElementType::u64, 7, "u64", 8}, {ElementType::i64, 8, "i64", 8},
    };
    for (const Row& r : rows) {
        CAPTURE(r.name);
        CHECK(static_cast<std::uint8_t>(r.type) == r.code);
        CHECK(worldgen::db::element_type_from_code(r.code) == r.type);
        CHECK(worldgen::db::element_type_name(r.type) == r.name);
        CHECK(worldgen::db::element_type_from_name(r.name) == r.type);
        CHECK(worldgen::db::element_size(r.type) == r.size);
    }
    CHECK_FALSE(worldgen::db::element_type_from_code(0).has_value());
    CHECK_FALSE(worldgen::db::element_type_from_code(9).has_value());
    CHECK_FALSE(worldgen::db::element_type_from_name("U8").has_value());
    CHECK_FALSE(worldgen::db::element_type_from_name("f32").has_value());
    CHECK_FALSE(worldgen::db::element_type_from_name("").has_value());
}

TEST_CASE("worked example 1: u8 5,5,5,7,0 and the full 29-byte blob (7.3)") {
    const LayerGrid grid = grid_of<std::uint8_t>(5, 1, {5, 5, 5, 7, 0});
    const Bytes blob = worldgen::db::encode_layer_blob(grid, Codec::delta_varint);
    const Bytes expected = {0x55, 0x4D, 0x4C, 0x59, 0x01, 0x00, 0x01, 0x01, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                            0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x01, 0x04, 0x0D};
    CHECK(blob.size() == 29);
    CHECK(blob == expected);
    // Default codec is delta-varint.
    CHECK(worldgen::db::encode_layer_blob(grid) == expected);

    const auto decoded = worldgen::db::decode_layer_blob(expected);
    REQUIRE(decoded.ok());
    CHECK(decoded.codec == Codec::delta_varint);
    CHECK(grids_equal(decoded.grid, grid));
}

TEST_CASE("worked example 2: i16 -1,300,300 (7.3)") {
    const LayerGrid grid = grid_of<std::int16_t>(3, 1, {-1, 300, 300});
    const Bytes blob = worldgen::db::encode_layer_blob(grid);
    CHECK(payload_of(blob) == Bytes{0x01, 0xDA, 0x04, 0x00, 0x00});
    CHECK(blob == make_blob(4, 1, 3, 1, {0x01, 0xDA, 0x04, 0x00, 0x00}));
    const auto decoded = worldgen::db::decode_layer_blob(blob);
    REQUIRE(decoded.ok());
    CHECK(grids_equal(decoded.grid, grid));
}

TEST_CASE("worked example 3: u8 0,0,0 (7.3)") {
    const LayerGrid grid = grid_of<std::uint8_t>(3, 1, {0, 0, 0});
    const Bytes blob = worldgen::db::encode_layer_blob(grid);
    CHECK(payload_of(blob) == Bytes{0x00, 0x02});
    const auto decoded = worldgen::db::decode_layer_blob(make_blob(1, 1, 3, 1, {0x00, 0x02}));
    REQUIRE(decoded.ok());
    CHECK(grids_equal(decoded.grid, grid));
}

TEST_CASE("hand-checked codec 1 encodings (7.3)") {
    // u8 255: delta 255, zigzag 510 = FE 03. i8 -1: zigzag 1 (sign extension).
    CHECK(payload_of(worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(1, 1, {255}))) == Bytes{0xFE, 0x03});
    CHECK(payload_of(worldgen::db::encode_layer_blob(grid_of<std::int8_t>(1, 1, {-1}))) == Bytes{0x01});
    // u64 max: delta 2^64-1 is int64 -1, zigzag 1.
    CHECK(payload_of(worldgen::db::encode_layer_blob(
              grid_of<std::uint64_t>(1, 1, {std::numeric_limits<std::uint64_t>::max()}))) == Bytes{0x01});
    // i64 min: zigzag 2^64-1, a 10-byte LEB128.
    CHECK(payload_of(worldgen::db::encode_layer_blob(
              grid_of<std::int64_t>(1, 1, {std::numeric_limits<std::int64_t>::min()}))) ==
          Bytes{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01});
    // i64 max: zigzag 2^64-2.
    CHECK(payload_of(worldgen::db::encode_layer_blob(
              grid_of<std::int64_t>(1, 1, {std::numeric_limits<std::int64_t>::max()}))) ==
          Bytes{0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01});
    // Alternating u8 0,255,0,255: deltas 0,255,-255,255 -> zigzag 0,510,509,510.
    CHECK(payload_of(worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(4, 1, {0, 255, 0, 255}))) ==
          Bytes{0x00, 0x00, 0xFE, 0x03, 0xFD, 0x03, 0xFE, 0x03});
    // A run of 21 zeros: 00 then LEB128(20).
    CHECK(payload_of(worldgen::db::encode_layer_blob(grid_of<std::uint16_t>(3, 7, std::vector<std::uint16_t>(21, 0)))) ==
          Bytes{0x00, 0x14});
    // A run of 65536 zeros: 00 then LEB128(65535) = FF FF 03.
    CHECK(payload_of(worldgen::db::encode_layer_blob(
              grid_of<std::uint32_t>(1, 65536, std::vector<std::uint32_t>(65536, 0)))) ==
          Bytes{0x00, 0xFF, 0xFF, 0x03});
    // A run of zero deltas after a non-zero value: 9,9,9,9 -> 12, run of 3.
    CHECK(payload_of(worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(2, 2, {9, 9, 9, 9}))) ==
          Bytes{0x12, 0x00, 0x02});
    // Width 65536 in the header: 00 00 01 00.
    const Bytes wide = worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(65536, 1, std::vector<std::uint8_t>(65536, 1)));
    CHECK(Bytes(wide.begin() + 8, wide.begin() + 16) == Bytes{0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00});
}

TEST_CASE("round trip for every element type, both codecs, canonical re-encoding (12.1)") {
    SUBCASE("u8") { round_trips_for_type<std::uint8_t>(); }
    SUBCASE("i8") { round_trips_for_type<std::int8_t>(); }
    SUBCASE("u16") { round_trips_for_type<std::uint16_t>(); }
    SUBCASE("i16") { round_trips_for_type<std::int16_t>(); }
    SUBCASE("u32") { round_trips_for_type<std::uint32_t>(); }
    SUBCASE("i32") { round_trips_for_type<std::int32_t>(); }
    SUBCASE("u64") { round_trips_for_type<std::uint64_t>(); }
    SUBCASE("i64") { round_trips_for_type<std::int64_t>(); }
}

TEST_CASE("decoding hand-made payloads gives the expected values (7.3)") {
    // A 10-byte LEB128 of 2^64-1 as a u64 element: zigzag^-1 gives 2^63.
    const auto big = worldgen::db::decode_layer_blob(
        make_blob(7, 1, 1, 1, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01}));
    REQUIRE(big.ok());
    REQUIRE(big.grid.get_if<std::uint64_t>() != nullptr);
    CHECK(big.grid.get_if<std::uint64_t>()->at(0) == 0x8000000000000000ULL);
    // The same payload {zero-run 1, z=1} is 0, -1 for i8 ...
    const auto i8 = worldgen::db::decode_layer_blob(make_blob(2, 1, 2, 1, {0x00, 0x00, 0x01}));
    REQUIRE(i8.ok());
    REQUIRE(i8.grid.get_if<std::int8_t>() != nullptr);
    CHECK(*i8.grid.get_if<std::int8_t>() == std::vector<std::int8_t>{0, -1});
    // ... but out of range for u8 (0 - 1 mod 2^64 is not a u8).
    CHECK(decode_error(make_blob(1, 1, 2, 1, {0x00, 0x00, 0x01})) == BlobError::value_out_of_range);
}

TEST_CASE("codec 0 raw: little-endian at natural size (7.2, 12.1)") {
    const Bytes u16 = worldgen::db::encode_layer_blob(grid_of<std::uint16_t>(1, 1, {0x1234}), Codec::raw);
    CHECK(u16 == make_blob(3, 0, 1, 1, {0x34, 0x12}));
    const Bytes u32 = worldgen::db::encode_layer_blob(grid_of<std::uint32_t>(2, 1, {0x12345678U, 1U}), Codec::raw);
    CHECK(payload_of(u32) == Bytes{0x78, 0x56, 0x34, 0x12, 0x01, 0x00, 0x00, 0x00});
    const Bytes i16 = worldgen::db::encode_layer_blob(grid_of<std::int16_t>(1, 1, {-2}), Codec::raw);
    CHECK(payload_of(i16) == Bytes{0xFE, 0xFF});
    const Bytes u64 =
        worldgen::db::encode_layer_blob(grid_of<std::uint64_t>(1, 1, {0x0102030405060708ULL}), Codec::raw);
    CHECK(payload_of(u64) == Bytes{0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01});
    const Bytes i8 = worldgen::db::encode_layer_blob(grid_of<std::int8_t>(3, 1, {-128, 0, 127}), Codec::raw);
    CHECK(payload_of(i8) == Bytes{0x80, 0x00, 0x7F});

    const auto decoded = worldgen::db::decode_layer_blob(make_blob(3, 0, 2, 1, {0x34, 0x12, 0xFF, 0xFF}));
    REQUIRE(decoded.ok());
    CHECK(decoded.codec == Codec::raw);
    REQUIRE(decoded.grid.get_if<std::uint16_t>() != nullptr);
    CHECK(*decoded.grid.get_if<std::uint16_t>() == std::vector<std::uint16_t>{0x1234, 0xFFFF});
}

TEST_CASE("decoder rejects malformed headers (7.1, 12.1, 14.4)") {
    const Bytes good = make_blob(1, 1, 5, 1, {0x0A, 0x00, 0x01, 0x04, 0x0D});
    REQUIRE(decode_error(good) == BlobError::none);

    SUBCASE("wrong magic") {
        for (std::size_t i = 0; i < 4; ++i) {
            Bytes b = good;
            b[i] = static_cast<std::uint8_t>(b[i] ^ 0x20U);
            CHECK(decode_error(b) == BlobError::bad_magic);
            CHECK(worldgen::db::parse_blob_header(b).error == BlobError::bad_magic);
        }
    }
    SUBCASE("version other than 1") {
        CHECK(decode_error(make_blob(1, 1, 5, 1, {0x0A, 0x00, 0x01, 0x04, 0x0D}, 0)) == BlobError::bad_version);
        CHECK(decode_error(make_blob(1, 1, 5, 1, {0x0A, 0x00, 0x01, 0x04, 0x0D}, 2)) == BlobError::bad_version);
        CHECK(decode_error(make_blob(1, 1, 5, 1, {0x0A, 0x00, 0x01, 0x04, 0x0D}, 0x0100)) == BlobError::bad_version);
    }
    SUBCASE("unknown element type") {
        CHECK(decode_error(make_blob(0, 1, 1, 1, {0x02})) == BlobError::bad_element_type);
        CHECK(decode_error(make_blob(9, 1, 1, 1, {0x02})) == BlobError::bad_element_type);
        CHECK(decode_error(make_blob(255, 1, 1, 1, {0x02})) == BlobError::bad_element_type);
    }
    SUBCASE("unknown codec") {
        CHECK(decode_error(make_blob(1, 2, 1, 1, {0x02})) == BlobError::bad_codec);
        CHECK(decode_error(make_blob(1, 255, 1, 1, {0x02})) == BlobError::bad_codec);
    }
    SUBCASE("payload length mismatch") {
        CHECK(decode_error(make_blob(1, 1, 5, 1, {0x0A, 0x00, 0x01, 0x04, 0x0D}, 1, 4)) ==
              BlobError::payload_length_mismatch);
        CHECK(decode_error(make_blob(1, 1, 5, 1, {0x0A, 0x00, 0x01, 0x04, 0x0D}, 1, 6)) ==
              BlobError::payload_length_mismatch);
        CHECK(decode_error(make_blob(1, 1, 5, 1, {0x0A, 0x00, 0x01, 0x04, 0x0D}, 1, 0x100000005ULL)) ==
              BlobError::payload_length_mismatch);
    }
    SUBCASE("shorter than the 24-byte header") {
        CHECK(decode_error(Bytes{}) == BlobError::too_short);
        CHECK(decode_error(Bytes(good.begin(), good.begin() + 23)) == BlobError::too_short);
        CHECK(worldgen::db::parse_blob_header(Bytes(good.begin(), good.begin() + 23)).error == BlobError::too_short);
    }
    SUBCASE("width or height outside 1..65536") {
        CHECK(decode_error(make_blob(1, 1, 0, 1, {})) == BlobError::bad_dimensions);
        CHECK(decode_error(make_blob(1, 1, 1, 0, {})) == BlobError::bad_dimensions);
        CHECK(decode_error(make_blob(1, 1, 65537, 1, {0x00, 0x80, 0x80, 0x04})) == BlobError::bad_dimensions);
        CHECK(decode_error(make_blob(1, 1, 1, 65537, {0x00, 0x80, 0x80, 0x04})) == BlobError::bad_dimensions);
    }
    SUBCASE("parse_blob_header accepts a good header and reports its fields") {
        const auto parsed = worldgen::db::parse_blob_header(good);
        REQUIRE(parsed.ok());
        CHECK(parsed.header.type == ElementType::u8);
        CHECK(parsed.header.codec == Codec::delta_varint);
        CHECK(parsed.header.width == 5);
        CHECK(parsed.header.height == 1);
        CHECK(parsed.header.payload_length == 5);
        // It does not look at the payload.
        CHECK(worldgen::db::parse_blob_header(make_blob(1, 1, 5, 1, {0x80, 0x00})).ok());
    }
}

TEST_CASE("decoder rejects malformed codec 1 payloads (7.3, 12.1, 14.4)") {
    // Non-shortest LEB128: 80 00 encodes 0 with a redundant group.
    CHECK(decode_error(make_blob(1, 1, 1, 1, {0x80, 0x00})) == BlobError::non_shortest_leb128);
    CHECK(decode_error(make_blob(1, 1, 1, 1, {0x82, 0x00})) == BlobError::non_shortest_leb128);
    // ... also inside a zero-run length.
    CHECK(decode_error(make_blob(1, 1, 2, 1, {0x00, 0x81, 0x00})) == BlobError::non_shortest_leb128);
    // Two consecutive zero-run tokens with at least 2 elements.
    CHECK(decode_error(make_blob(1, 1, 2, 1, {0x00, 0x00, 0x00, 0x00})) == BlobError::consecutive_zero_runs);
    CHECK(decode_error(make_blob(4, 1, 5, 1, {0x00, 0x01, 0x00, 0x01, 0x02})) == BlobError::consecutive_zero_runs);
    // With 1 element the same bytes are trailing bytes (first problem met, 14.4).
    CHECK(decode_error(make_blob(1, 1, 1, 1, {0x00, 0x00, 0x00, 0x00})) == BlobError::trailing_bytes);
    // A run past the element count.
    CHECK(decode_error(make_blob(1, 1, 2, 1, {0x00, 0x02})) == BlobError::run_overrun);
    CHECK(decode_error(make_blob(1, 1, 3, 1, {0x02, 0x00, 0x02})) == BlobError::run_overrun);
    // Trailing bytes after the last element.
    CHECK(decode_error(make_blob(1, 1, 1, 1, {0x02, 0x02})) == BlobError::trailing_bytes);
    CHECK(decode_error(make_blob(1, 1, 5, 1, {0x0A, 0x00, 0x01, 0x04, 0x0D, 0x00})) == BlobError::trailing_bytes);
    // Values out of range for the type: u8 256, i8 128, i8 -129, u16 65536,
    // i32 2^31, u32 -1.
    CHECK(decode_error(make_blob(1, 1, 1, 1, {0x80, 0x04})) == BlobError::value_out_of_range);
    CHECK(decode_error(make_blob(2, 1, 1, 1, {0x80, 0x02})) == BlobError::value_out_of_range);
    CHECK(decode_error(make_blob(2, 1, 1, 1, {0x81, 0x02})) == BlobError::value_out_of_range);
    CHECK(decode_error(make_blob(3, 1, 1, 1, {0x80, 0x80, 0x08})) == BlobError::value_out_of_range);
    CHECK(decode_error(make_blob(6, 1, 1, 1, {0x80, 0x80, 0x80, 0x80, 0x10})) == BlobError::value_out_of_range);
    CHECK(decode_error(make_blob(5, 1, 1, 1, {0x01})) == BlobError::value_out_of_range);
    // ... while the largest in-range values decode: u8 255, i8 127, i8 -128.
    CHECK(decode_error(make_blob(1, 1, 1, 1, {0xFE, 0x03})) == BlobError::none);
    CHECK(decode_error(make_blob(2, 1, 1, 1, {0xFE, 0x01})) == BlobError::none);
    CHECK(decode_error(make_blob(2, 1, 1, 1, {0xFF, 0x01})) == BlobError::none);
    // The payload ends inside a token or before all elements.
    CHECK(decode_error(make_blob(1, 1, 2, 1, {0x0A})) == BlobError::truncated);
    CHECK(decode_error(make_blob(4, 1, 1, 1, {0xDA})) == BlobError::truncated);
    CHECK(decode_error(make_blob(1, 1, 2, 1, {0x00})) == BlobError::truncated);
    CHECK(decode_error(make_blob(1, 1, 1, 1, {})) == BlobError::truncated);
    // A LEB128 value that does not fit in 64 bits.
    CHECK(decode_error(make_blob(7, 1, 1, 1, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x02})) ==
          BlobError::leb128_overflow);
    CHECK(decode_error(make_blob(7, 1, 1, 1, {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01})) ==
          BlobError::leb128_overflow);
    // A rejected blob leaves no grid.
    const auto bad = worldgen::db::decode_layer_blob(make_blob(1, 1, 1, 1, {0x80, 0x04}));
    CHECK_FALSE(bad.ok());
    CHECK(bad.grid.cell_count() == 0);
}

TEST_CASE("decoder rejects a codec 0 payload of the wrong size (14.4)") {
    CHECK(decode_error(make_blob(3, 0, 2, 1, {0x34, 0x12, 0xFF})) == BlobError::raw_size_mismatch);
    CHECK(decode_error(make_blob(3, 0, 2, 1, {0x34, 0x12, 0xFF, 0xFF, 0x00})) == BlobError::raw_size_mismatch);
    CHECK(decode_error(make_blob(1, 0, 3, 7, Bytes(20, 0))) == BlobError::raw_size_mismatch);
    CHECK(decode_error(make_blob(1, 0, 3, 7, Bytes(21, 0))) == BlobError::none);
}

TEST_CASE("blob error names are distinct and stable") {
    std::vector<std::string> names;
    for (int e = 0; e <= static_cast<int>(BlobError::value_out_of_range); ++e) {
        names.emplace_back(worldgen::db::blob_error_name(static_cast<BlobError>(e)));
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        CHECK_FALSE(names[i].empty());
        for (std::size_t j = i + 1; j < names.size(); ++j) {
            CHECK(names[i] != names[j]);
        }
    }
    CHECK(std::string(worldgen::db::blob_error_name(BlobError::bad_magic)) == "bad_magic");
}

TEST_CASE("encoder rejects an invalid grid or codec as a fatal error") {
    CHECK_THROWS_AS((void)worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(0, 1, {})), FatalError);
    CHECK_THROWS_AS((void)worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(1, 0, {})), FatalError);
    CHECK_THROWS_AS((void)worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(2, 2, {1, 2, 3})), FatalError);
    CHECK_THROWS_AS((void)worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(65537, 1, std::vector<std::uint8_t>(65537, 0))),
                    FatalError);
    CHECK_THROWS_AS((void)worldgen::db::encode_layer_blob(grid_of<std::uint8_t>(1, 1, {1}), static_cast<Codec>(2)),
                    FatalError);
    CHECK_FALSE(worldgen::db::is_valid_layer_grid(grid_of<std::uint8_t>(2, 2, {1, 2, 3})));
    CHECK(worldgen::db::is_valid_layer_grid(grid_of<std::uint8_t>(2, 2, {1, 2, 3, 4})));
    const LayerGrid zero = worldgen::db::make_layer_grid(ElementType::i32, 3, 7);
    CHECK(zero.type() == ElementType::i32);
    REQUIRE(zero.get_if<std::int32_t>() != nullptr);
    CHECK(*zero.get_if<std::int32_t>() == std::vector<std::int32_t>(21, 0));
}

}  // TEST_SUITE
