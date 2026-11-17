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

// The eight meta rows of a P0 world in key order (P0-02 8).
inline MetaRows meta_rows(std::uint64_t seed, std::string_view phrase = {}, std::string_view stages = {},
                          std::string_view map_size = "1024") {
    return {
        {"content_hash", std::string(EMPTY_CONTENT_HASH)},
        {"format", "the-unmapped-world"},
        {"generator_version", "0.6.0"},
        {"param.map_size", std::string(map_size)},
        {"schema_version", "2"},
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

// schema of every world at schema version 2 (the name is the P0 one):
// sqlite_schema rows ordered by type, then name: the automatic index of
// layer.name (sql NULL, amendment 10), then the tables layer, meta and pass
// with their DDL. rootpage is excluded.
inline worldgen::Sha256Digest p0_schema_hash() {
    Encoder e;
    e.col_text("index");
    e.col_text("sqlite_autoindex_layer_1");
    e.col_text("layer");
    e.col_null();
    e.col_text("table");
    e.col_text("layer");
    e.col_text("layer");
    e.col_text(DDL_LAYER);
    e.col_text("table");
    e.col_text("meta");
    e.col_text("meta");
    e.col_text(DDL_META);
    e.col_text("table");
    e.col_text("pass");
    e.col_text("pass");
    e.col_text(DDL_PASS);
    return e.digest();
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
    "generator_version = 0.6.0\n"
    "param.map_size = 1024\n"
    "schema_version = 2\n"
    "seed = 0\n"
    "seed_phrase =\n"
    "stages =\n"
    "[tables]\n"
    "layer = 0\n"
    "meta = 8\n"
    "pass = 0\n"
    "[layers]\n"
    "(none)\n";

}  // namespace p004
