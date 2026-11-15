// Layer blob format, version 1 (spec P0-02 section 7).
//
// All multi-byte fields are serialized little-endian by arithmetic
// (P0-01 section 4), never by memcpy.

#include "worldgen/db/layer_blob.hpp"

#include <limits>

#include "db/layer_blob_detail.hpp"
#include "worldgen/core/error.hpp"

namespace worldgen::db {

namespace {

// --- little-endian helpers ---------------------------------------------------

void put_le(std::vector<std::uint8_t>& out, std::uint64_t value, std::uint32_t bytes) {
    for (std::uint32_t i = 0; i < bytes; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8U * i)) & 0xFFU));
    }
}

std::uint64_t get_le(std::span<const std::uint8_t> in, std::size_t offset, std::uint32_t bytes) noexcept {
    std::uint64_t value = 0;
    for (std::uint32_t i = 0; i < bytes; ++i) {
        value |= static_cast<std::uint64_t>(in[offset + i]) << (8U * i);
    }
    return value;
}

// --- per-type widening and range checks (7.3) --------------------------------

// The element widened to 64 bits: sign-extended for signed types,
// zero-extended for unsigned, as a uint64 bit pattern.
template <class T>
std::uint64_t widen(T v) noexcept {
    if constexpr (std::is_signed_v<T>) {
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
    } else {
        return static_cast<std::uint64_t>(v);
    }
}

// True if the 64-bit pattern w is the widening of some T.
template <class T>
bool fits(std::uint64_t w) noexcept {
    if constexpr (std::is_same_v<T, std::uint64_t> || std::is_same_v<T, std::int64_t>) {
        return true;
    } else if constexpr (std::is_signed_v<T>) {
        const auto s = static_cast<std::int64_t>(w);
        return s >= static_cast<std::int64_t>(std::numeric_limits<T>::min()) &&
               s <= static_cast<std::int64_t>(std::numeric_limits<T>::max());
    } else {
        return w <= static_cast<std::uint64_t>(std::numeric_limits<T>::max());
    }
}

// The T whose widening is w (precondition: fits<T>(w)).
template <class T>
T narrow(std::uint64_t w) noexcept {
    if constexpr (std::is_signed_v<T>) {
        return static_cast<T>(static_cast<std::int64_t>(w));
    } else {
        return static_cast<T>(w);
    }
}

// --- LEB128 (7.3) ------------------------------------------------------------

void put_leb128(std::vector<std::uint8_t>& out, std::uint64_t n) {
    while (n >= 0x80U) {
        out.push_back(static_cast<std::uint8_t>((n & 0x7FU) | 0x80U));
        n >>= 7U;
    }
    out.push_back(static_cast<std::uint8_t>(n));
}

// Reads one shortest-form LEB128 from in[pos..end); advances pos.
BlobError get_leb128(std::span<const std::uint8_t> in, std::size_t& pos, std::uint64_t& value) noexcept {
    value = 0;
    std::uint32_t shift = 0;
    std::size_t count = 0;
    for (;;) {
        if (pos >= in.size()) {
            return BlobError::truncated;
        }
        const std::uint8_t byte = in[pos];
        ++pos;
        ++count;
        const auto group = static_cast<std::uint64_t>(byte & 0x7FU);
        if (shift == 63U && group > 1U) {
            return BlobError::leb128_overflow;  // the 10th group holds only bit 63
        }
        value |= group << shift;
        if ((byte & 0x80U) == 0U) {
            // The last group of a shortest form is non-zero, unless the whole
            // value is the single byte 00.
            if (byte == 0U && count > 1U) {
                return BlobError::non_shortest_leb128;
            }
            return BlobError::none;
        }
        if (shift == 63U) {
            return BlobError::leb128_overflow;  // an 11th group would follow
        }
        shift += 7U;
    }
}

std::uint64_t zigzag(std::uint64_t d) noexcept {
    return (d << 1U) ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(d) >> 63);
}

std::uint64_t unzigzag(std::uint64_t z) noexcept {
    return (z >> 1U) ^ (0U - (z & 1U));
}

// --- codecs ------------------------------------------------------------------

template <class T>
void encode_raw(std::vector<std::uint8_t>& out, const std::vector<T>& values) {
    for (const T v : values) {
        put_le(out, widen(v), static_cast<std::uint32_t>(sizeof(T)));
    }
}

template <class T>
void encode_delta_varint(std::vector<std::uint8_t>& out, const std::vector<T>& values) {
    std::uint64_t prev = 0;
    std::uint64_t run = 0;  // pending zero run length
    for (const T v : values) {
        const std::uint64_t e = widen(v);
        const std::uint64_t z = zigzag(e - prev);
        prev = e;
        if (z == 0U) {
            ++run;
            continue;
        }
        if (run > 0U) {
            put_leb128(out, 0U);
            put_leb128(out, run - 1U);
            run = 0;
        }
        put_leb128(out, z);
    }
    if (run > 0U) {
        put_leb128(out, 0U);
        put_leb128(out, run - 1U);
    }
}

template <class T>
BlobError decode_raw(std::span<const std::uint8_t> payload, std::uint64_t count, std::vector<T>& out) {
    const std::uint64_t size = static_cast<std::uint64_t>(sizeof(T));
    if (static_cast<std::uint64_t>(payload.size()) != count * size) {
        return BlobError::raw_size_mismatch;
    }
    out.resize(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = narrow<T>(get_le(payload, i * sizeof(T), static_cast<std::uint32_t>(sizeof(T))));
    }
    return BlobError::none;
}

template <class T>
BlobError decode_delta_varint(std::span<const std::uint8_t> payload, std::uint64_t count, std::vector<T>& out) {
    out.clear();
    out.reserve(static_cast<std::size_t>(count));
    std::uint64_t prev = 0;
    std::uint64_t filled = 0;
    bool last_was_run = false;
    std::size_t pos = 0;
    while (pos < payload.size()) {
        if (filled == count) {
            return BlobError::trailing_bytes;
        }
        std::uint64_t z = 0;
        if (const BlobError e = get_leb128(payload, pos, z); e != BlobError::none) {
            return e;
        }
        if (z == 0U) {
            if (last_was_run) {
                return BlobError::consecutive_zero_runs;
            }
            std::uint64_t k_minus_1 = 0;
            if (const BlobError e = get_leb128(payload, pos, k_minus_1); e != BlobError::none) {
                return e;
            }
            if (k_minus_1 >= count - filled) {
                return BlobError::run_overrun;  // k = k_minus_1 + 1 > elements left
            }
            // A zero delta repeats prev, which already fits the type (or is
            // the initial 0, which fits every type).
            const T value = narrow<T>(prev);
            out.insert(out.end(), static_cast<std::size_t>(k_minus_1 + 1U), value);
            filled += k_minus_1 + 1U;
            last_was_run = true;
            continue;
        }
        const std::uint64_t e = prev + unzigzag(z);  // mod 2^64
        if (!fits<T>(e)) {
            return BlobError::value_out_of_range;
        }
        out.push_back(narrow<T>(e));
        prev = e;
        ++filled;
        last_was_run = false;
    }
    if (filled != count) {
        return BlobError::truncated;
    }
    return BlobError::none;
}

bool valid_side(std::uint64_t side) noexcept {
    return side >= 1U && side <= MAX_LAYER_SIDE;
}

}  // namespace

// --- element types -----------------------------------------------------------

std::optional<ElementType> element_type_from_code(std::uint8_t code) noexcept {
    if (code >= 1U && code <= 8U) {
        return static_cast<ElementType>(code);
    }
    return std::nullopt;
}

std::string_view element_type_name(ElementType type) noexcept {
    switch (type) {
        case ElementType::u8: return "u8";
        case ElementType::i8: return "i8";
        case ElementType::u16: return "u16";
        case ElementType::i16: return "i16";
        case ElementType::u32: return "u32";
        case ElementType::i32: return "i32";
        case ElementType::u64: return "u64";
        case ElementType::i64: return "i64";
    }
    return "";
}

std::optional<ElementType> element_type_from_name(std::string_view name) noexcept {
    for (std::uint8_t code = 1; code <= 8U; ++code) {
        const auto type = static_cast<ElementType>(code);
        if (element_type_name(type) == name) {
            return type;
        }
    }
    return std::nullopt;
}

std::string_view codec_name(Codec codec) noexcept {
    switch (codec) {
        case Codec::raw: return "raw";
        case Codec::delta_varint: return "delta_varint";
    }
    return "";
}

std::uint32_t element_size(ElementType type) noexcept {
    switch (type) {
        case ElementType::u8:
        case ElementType::i8: return 1;
        case ElementType::u16:
        case ElementType::i16: return 2;
        case ElementType::u32:
        case ElementType::i32: return 4;
        case ElementType::u64:
        case ElementType::i64: return 8;
    }
    return 0;
}

LayerGrid make_layer_grid(ElementType type, std::uint32_t width, std::uint32_t height) {
    LayerGrid grid;
    grid.width = width;
    grid.height = height;
    const auto count = static_cast<std::size_t>(static_cast<std::uint64_t>(width) * height);
    switch (type) {
        case ElementType::u8: grid.values = std::vector<std::uint8_t>(count); break;
        case ElementType::i8: grid.values = std::vector<std::int8_t>(count); break;
        case ElementType::u16: grid.values = std::vector<std::uint16_t>(count); break;
        case ElementType::i16: grid.values = std::vector<std::int16_t>(count); break;
        case ElementType::u32: grid.values = std::vector<std::uint32_t>(count); break;
        case ElementType::i32: grid.values = std::vector<std::int32_t>(count); break;
        case ElementType::u64: grid.values = std::vector<std::uint64_t>(count); break;
        case ElementType::i64: grid.values = std::vector<std::int64_t>(count); break;
    }
    return grid;
}

bool is_valid_layer_grid(const LayerGrid& grid) noexcept {
    if (!valid_side(grid.width) || !valid_side(grid.height)) {
        return false;
    }
    const std::uint64_t count = grid.cell_count();
    return std::visit([count](const auto& v) { return static_cast<std::uint64_t>(v.size()) == count; },
                      grid.values);
}

// --- errors ------------------------------------------------------------------

const char* blob_error_name(BlobError error) noexcept {
    switch (error) {
        case BlobError::none: return "none";
        case BlobError::too_short: return "too_short";
        case BlobError::bad_magic: return "bad_magic";
        case BlobError::bad_version: return "bad_version";
        case BlobError::bad_element_type: return "bad_element_type";
        case BlobError::bad_codec: return "bad_codec";
        case BlobError::bad_dimensions: return "bad_dimensions";
        case BlobError::payload_length_mismatch: return "payload_length_mismatch";
        case BlobError::raw_size_mismatch: return "raw_size_mismatch";
        case BlobError::truncated: return "truncated";
        case BlobError::leb128_overflow: return "leb128_overflow";
        case BlobError::non_shortest_leb128: return "non_shortest_leb128";
        case BlobError::consecutive_zero_runs: return "consecutive_zero_runs";
        case BlobError::run_overrun: return "run_overrun";
        case BlobError::trailing_bytes: return "trailing_bytes";
        case BlobError::value_out_of_range: return "value_out_of_range";
    }
    return "unknown";
}

// --- header ------------------------------------------------------------------

namespace detail {

BlobHeaderParse parse_blob_header_fields(std::span<const std::uint8_t> blob, std::uint64_t blob_size) noexcept {
    BlobHeaderParse r;
    if (blob.size() < BLOB_HEADER_SIZE || blob_size < BLOB_HEADER_SIZE) {
        r.error = BlobError::too_short;
        return r;
    }
    for (std::size_t i = 0; i < BLOB_MAGIC.size(); ++i) {
        if (blob[i] != BLOB_MAGIC[i]) {
            r.error = BlobError::bad_magic;
            return r;
        }
    }
    if (get_le(blob, 4, 2) != BLOB_FORMAT_VERSION) {
        r.error = BlobError::bad_version;
        return r;
    }
    const std::optional<ElementType> type = element_type_from_code(blob[6]);
    if (!type) {
        r.error = BlobError::bad_element_type;
        return r;
    }
    if (blob[7] > 1U) {
        r.error = BlobError::bad_codec;
        return r;
    }
    const std::uint64_t width = get_le(blob, 8, 4);
    const std::uint64_t height = get_le(blob, 12, 4);
    if (!valid_side(width) || !valid_side(height)) {
        r.error = BlobError::bad_dimensions;
        return r;
    }
    const std::uint64_t payload_length = get_le(blob, 16, 8);
    if (payload_length != blob_size - BLOB_HEADER_SIZE) {
        r.error = BlobError::payload_length_mismatch;
        return r;
    }
    r.header.type = *type;
    r.header.codec = static_cast<Codec>(blob[7]);
    r.header.width = static_cast<std::uint32_t>(width);
    r.header.height = static_cast<std::uint32_t>(height);
    r.header.payload_length = payload_length;
    return r;
}

}  // namespace detail

BlobHeaderParse parse_blob_header(std::span<const std::uint8_t> blob) noexcept {
    return detail::parse_blob_header_fields(blob, static_cast<std::uint64_t>(blob.size()));
}

// --- decode ------------------------------------------------------------------

BlobDecode decode_layer_blob(std::span<const std::uint8_t> blob) {
    BlobDecode r;
    const BlobHeaderParse h = parse_blob_header(blob);
    if (!h.ok()) {
        r.error = h.error;
        return r;
    }
    LayerGrid grid = make_layer_grid(h.header.type, 0, 0);  // empty buffer of the right type
    grid.width = h.header.width;
    grid.height = h.header.height;
    const std::uint64_t count = grid.cell_count();
    const std::span<const std::uint8_t> payload = blob.subspan(BLOB_HEADER_SIZE);
    const Codec codec = h.header.codec;
    const BlobError error = std::visit(
        [&](auto& values) {
            return codec == Codec::raw ? decode_raw(payload, count, values)
                                       : decode_delta_varint(payload, count, values);
        },
        grid.values);
    if (error != BlobError::none) {
        r.error = error;
        return r;
    }
    r.grid = std::move(grid);
    r.codec = codec;
    return r;
}

// --- encode ------------------------------------------------------------------

std::vector<std::uint8_t> encode_layer_blob(const LayerGrid& grid, Codec codec) {
    if (!is_valid_layer_grid(grid)) {
        fatal("encode_layer_blob: invalid layer grid (width and height must be 1 to 65536 and the "
              "buffer must hold width * height elements)");
    }
    if (codec != Codec::raw && codec != Codec::delta_varint) {
        fatal("encode_layer_blob: unknown codec");
    }
    std::vector<std::uint8_t> out;
    out.reserve(BLOB_HEADER_SIZE);
    out.insert(out.end(), BLOB_MAGIC.begin(), BLOB_MAGIC.end());
    put_le(out, BLOB_FORMAT_VERSION, 2);
    out.push_back(static_cast<std::uint8_t>(grid.type()));
    out.push_back(static_cast<std::uint8_t>(codec));
    put_le(out, grid.width, 4);
    put_le(out, grid.height, 4);
    put_le(out, 0, 8);  // payload length, patched below
    std::visit(
        [&](const auto& values) {
            if (codec == Codec::raw) {
                encode_raw(out, values);
            } else {
                encode_delta_varint(out, values);
            }
        },
        grid.values);
    const auto payload_length = static_cast<std::uint64_t>(out.size() - BLOB_HEADER_SIZE);
    for (std::uint32_t i = 0; i < 8U; ++i) {
        out[16U + i] = static_cast<std::uint8_t>((payload_length >> (8U * i)) & 0xFFU);
    }
    return out;
}

}  // namespace worldgen::db
