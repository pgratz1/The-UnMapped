// Canonical encoding and content hash (P0-03 section 8).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/core/sha256.hpp"
#include "worldgen/data/content.hpp"

namespace worldgen::data {

namespace {

class Encoder {
public:
    void u8(std::uint8_t b) { out_.push_back(b); }

    // 8 bytes little-endian, by arithmetic (P0-01 section 4, byte order).
    void u64(std::uint64_t v) {
        for (std::uint32_t i = 0; i < 8; ++i) {
            out_.push_back(static_cast<std::uint8_t>((v >> (8U * i)) & 0xFFU));
        }
    }

    // Two's complement bit pattern, little-endian.
    void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }

    // u64(length in bytes), then the bytes.
    void str(std::string_view s) {
        u64(static_cast<std::uint64_t>(s.size()));
        for (const char c : s) {
            out_.push_back(static_cast<std::uint8_t>(c));
        }
    }

    void record(const Record& r) {
        for (std::size_t i = 0; i < r.size(); ++i) {
            str(r.name_at(i));
            value(r.value_at(i));
        }
    }

    void value(const Value& v) {
        switch (v.kind()) {
            case ValueKind::integer:
                u8(0x01);
                i64(v.as_int());
                return;
            case ValueKind::fixed:
                u8(0x02);
                i64(v.as_fx().raw());
                return;
            case ValueKind::boolean:
                u8(0x03);
                u8(v.as_bool() ? std::uint8_t{0x01} : std::uint8_t{0x00});
                return;
            case ValueKind::string:
                u8(0x04);
                str(v.as_string());
                return;
            case ValueKind::array:
                u8(0x05);
                u64(static_cast<std::uint64_t>(v.as_array().size()));
                for (const Value& e : v.as_array()) {
                    value(e);
                }
                return;
            case ValueKind::record:
                u8(0x06);
                record(v.as_record());
                return;
        }
    }

    std::vector<std::uint8_t> take() { return std::move(out_); }

private:
    std::vector<std::uint8_t> out_;
};

}  // namespace

std::vector<std::uint8_t> canonical_encoding(const ContentSet& content) {
    Encoder e;
    for (const KindContent& kind : content.kinds) {
        e.str(kind.kind);
        e.u64(static_cast<std::uint64_t>(kind.definitions.size()));
        for (const Definition& d : kind.definitions) {
            e.str(d.id);
            e.record(d.record);
        }
    }
    return e.take();
}

Sha256Digest content_hash(const ContentSet& content) {
    const std::vector<std::uint8_t> bytes = canonical_encoding(content);
    return sha256(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
}

std::string content_hash_hex(const ContentSet& content) {
    return to_hex(content_hash(content));
}

}  // namespace worldgen::data
