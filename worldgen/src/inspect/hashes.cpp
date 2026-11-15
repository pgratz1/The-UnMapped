// File and logical hashes (spec P0-04 section 8).

#include "worldgen/inspect/hashes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <new>
#include <system_error>
#include <type_traits>
#include <variant>

#include "worldgen/core/error.hpp"
#include "worldgen/db/utf8_path.hpp"
#include "worldgen/inspect/report.hpp"

namespace worldgen::inspect {

namespace {

// Little-endian encoders feeding a Sha256 (P0-04 8). Bytes are produced by
// arithmetic, never by copying an integer's memory (P0-01 4).
class Encoder {
public:
    explicit Encoder(Sha256& sha) noexcept : sha_(sha) {}

    void u8(std::uint8_t v) noexcept { put(v, 1); }
    void u32(std::uint32_t v) noexcept { put(v, 4); }
    void u64(std::uint64_t v) noexcept { put(v, 8); }
    void i64(std::int64_t v) noexcept { put(static_cast<std::uint64_t>(v), 8); }
    void str(std::string_view s) noexcept {
        u64(static_cast<std::uint64_t>(s.size()));
        sha_.update(s);
    }
    void value(const db::ColumnValue& v) noexcept {
        u8(static_cast<std::uint8_t>(v.type));
        switch (v.type) {
            case db::ValueType::null: break;
            case db::ValueType::integer: i64(v.integer); break;
            case db::ValueType::text:
            case db::ValueType::blob: str(v.bytes); break;
        }
    }

    // Raw bytes, already serialized.
    void bytes(std::span<const std::uint8_t> b) noexcept { sha_.update(b); }

    // v's low `size` bytes, little-endian.
    void put(std::uint64_t v, std::size_t size) noexcept {
        std::array<std::uint8_t, 8> b{};
        for (std::size_t i = 0; i < size; ++i) {
            b[i] = static_cast<std::uint8_t>((v >> (8U * i)) & 0xFFU);
        }
        sha_.update(std::span<const std::uint8_t>(b.data(), size));
    }

private:
    Sha256& sha_;
};

// Natural size and bit pattern of a layer element.
template <class T>
std::uint64_t element_bits(T v) noexcept {
    if constexpr (std::is_signed_v<T>) {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));  // sign-extended; low bytes taken
    } else {
        return static_cast<std::uint64_t>(v);
    }
}

}  // namespace

db::Result<Sha256Digest> hash_file(std::string_view path_utf8) {
    try {
        const std::filesystem::path path = db::path_from_utf8(path_utf8);
        const std::string shown(path_utf8);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            return db::Error{db::ErrorCode::file_unreadable,
                             "cannot read '" + shown + "': the file is missing or not a regular file"};
        }
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return db::Error{db::ErrorCode::file_unreadable, "cannot open '" + shown + "'"};
        }
        Sha256 sha;
        std::vector<char> buffer(1U << 16U);
        for (;;) {
            in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize got = in.gcount();
            if (got > 0) {
                sha.update(std::string_view(buffer.data(), static_cast<std::size_t>(got)));
            }
            if (in.eof()) {
                break;
            }
            if (!in) {
                return db::Error{db::ErrorCode::file_unreadable, "reading '" + shown + "' failed"};
            }
        }
        if (in.bad()) {
            return db::Error{db::ErrorCode::file_unreadable, "reading '" + shown + "' failed"};
        }
        return sha.final();
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while hashing a file"};
    }
}

Sha256Digest layer_hash(const db::LayerGrid& grid) {
    if (!db::is_valid_layer_grid(grid)) {
        fatal("layer_hash: invalid layer grid");
    }
    Sha256 sha;
    Encoder enc(sha);
    enc.u32(grid.width);
    enc.u32(grid.height);
    enc.u8(static_cast<std::uint8_t>(grid.type()));
    std::visit(
        [&enc](const auto& cells) {
            using T = typename std::decay_t<decltype(cells)>::value_type;
            constexpr std::size_t size = sizeof(T);
            // Elements are serialized into a block, then fed in one update.
            std::array<std::uint8_t, 4096> block{};
            std::size_t used = 0;
            for (const T v : cells) {
                const std::uint64_t bits = element_bits(v);
                for (std::size_t i = 0; i < size; ++i) {
                    block[used + i] = static_cast<std::uint8_t>((bits >> (8U * i)) & 0xFFU);
                }
                used += size;
                if (used == block.size()) {  // 4096 is a multiple of every element size
                    enc.bytes(std::span<const std::uint8_t>(block.data(), used));
                    used = 0;
                }
            }
            enc.bytes(std::span<const std::uint8_t>(block.data(), used));
        },
        grid.values);
    return sha.final();
}

db::Result<DetailHashes> detail_hashes(const db::WorldReader& world) {
    try {
        DetailHashes out;
        {
            Sha256 sha;
            Encoder enc(sha);
            db::Status status = world.visit_schema_rows([&enc](std::span<const db::ColumnValue> row) {
                for (const db::ColumnValue& v : row) {
                    enc.value(v);
                }
            });
            if (!status.ok()) {
                return status.error();
            }
            out.schema = sha.final();
        }

        db::Result<std::vector<db::TableSummary>> tables = world.tables();
        if (!tables.ok()) {
            return tables.error();
        }
        for (const db::TableSummary& t : tables.value()) {
            db::Result<std::vector<std::string>> columns = world.table_columns(t.name);
            if (!columns.ok()) {
                return columns.error();
            }
            Sha256 sha;
            Encoder enc(sha);
            enc.u64(static_cast<std::uint64_t>(columns.value().size()));
            db::Status status = world.visit_table_rows(t.name, [&enc](std::span<const db::ColumnValue> row) {
                for (const db::ColumnValue& v : row) {
                    enc.value(v);
                }
            });
            if (!status.ok()) {
                return status.error();
            }
            out.tables.push_back(NamedHash{t.name, sha.final()});
        }

        db::Result<std::vector<db::LayerInfo>> layers = world.layers();
        if (!layers.ok()) {
            return layers.error();
        }
        for (const db::LayerInfo& info : layers.value()) {
            db::Result<db::Layer> layer = world.read_layer(info.name);
            if (!layer.ok()) {
                return layer.error();
            }
            out.layers.push_back(NamedHash{info.name, layer_hash(layer.value().grid)});
        }
        return out;
    } catch (const std::bad_alloc&) {
        return db::Error{db::ErrorCode::out_of_memory, "out of memory while hashing the world"};
    }
}

std::string format_detail(const Sha256Digest& file, const DetailHashes& detail) {
    std::string out = "file " + to_hex(file) + "\n";
    out += "schema " + to_hex(detail.schema) + "\n";
    for (const NamedHash& t : detail.tables) {
        out += "table " + escape_report_text(t.name) + " " + to_hex(t.digest) + "\n";
    }
    for (const NamedHash& l : detail.layers) {
        out += "layer " + escape_report_text(l.name) + " " + to_hex(l.digest) + "\n";
    }
    return out;
}

bool is_sha256_hex(std::string_view text) noexcept {
    if (text.size() != 64) {
        return false;
    }
    for (const char c : text) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) {
            return false;
        }
    }
    return true;
}

}  // namespace worldgen::inspect
