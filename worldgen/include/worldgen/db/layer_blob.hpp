#pragma once

// Layer blob format, version 1 (spec P0-02 section 7), element types and
// the in-memory layer grid (P0-02 5.6).
//
// Encoding is generator-side: an invalid grid is a contract violation and
// raises FatalError. Decoding takes data from a file, so errors are
// returned as values and decoding never throws FatalError.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace worldgen::db {

// --- Element types (P0-02 5.6 column `format`, 7.1 offset 6) ----------------

// Element type of a layer; the enumerator value is the type code of the blob
// header (P0-02 7.1 offset 6).
enum class ElementType : std::uint8_t {
    u8 = 1,
    i8 = 2,
    u16 = 3,
    i16 = 4,
    u32 = 5,
    i32 = 6,
    u64 = 7,
    i64 = 8,
};

// Element type for a header code 1..8; std::nullopt for any other code (7.1).
[[nodiscard]] std::optional<ElementType> element_type_from_code(std::uint8_t code) noexcept;

// The `format` column text: "u8", "i8", ..., "i64" (5.6).
[[nodiscard]] std::string_view element_type_name(ElementType type) noexcept;

// Element type for a `format` text; std::nullopt for anything else (5.6).
[[nodiscard]] std::optional<ElementType> element_type_from_name(std::string_view name) noexcept;

// Size of one element in bytes: 1, 2, 4 or 8 (7.2).
[[nodiscard]] std::uint32_t element_size(ElementType type) noexcept;

// The ElementType of a C++ element type, e.g. element_type_of<std::int16_t>.
template <class T>
inline constexpr ElementType element_type_of = [] {
    if constexpr (std::is_same_v<T, std::uint8_t>) return ElementType::u8;
    else if constexpr (std::is_same_v<T, std::int8_t>) return ElementType::i8;
    else if constexpr (std::is_same_v<T, std::uint16_t>) return ElementType::u16;
    else if constexpr (std::is_same_v<T, std::int16_t>) return ElementType::i16;
    else if constexpr (std::is_same_v<T, std::uint32_t>) return ElementType::u32;
    else if constexpr (std::is_same_v<T, std::int32_t>) return ElementType::i32;
    else if constexpr (std::is_same_v<T, std::uint64_t>) return ElementType::u64;
    else {
        static_assert(std::is_same_v<T, std::int64_t>, "not a layer element type");
        return ElementType::i64;
    }
}();

// --- Layer grid (P0-02 5.6) --------------------------------------------------

// The cells of a layer, as a contiguous row-major buffer of its element
// type. The variant index is the type code minus 1 (u8 first, i64 last).
using LayerValues = std::variant<std::vector<std::uint8_t>, std::vector<std::int8_t>,
                                 std::vector<std::uint16_t>, std::vector<std::int16_t>,
                                 std::vector<std::uint32_t>, std::vector<std::int32_t>,
                                 std::vector<std::uint64_t>, std::vector<std::int64_t>>;

// Largest allowed width and height (5.6, 7.1).
inline constexpr std::uint32_t MAX_LAYER_SIDE = 65536U;

// A layer's dimensions and cells. Cell (x, y): x is the column (0 west,
// increasing east), y the row (0 north, increasing south); element index =
// y * width + x (P0-02 5.6). Valid when 1 <= width, height <= 65536 and
// the buffer holds exactly width * height elements.
struct LayerGrid {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    LayerValues values;

    // The element type, from the variant alternative held.
    [[nodiscard]] ElementType type() const noexcept {
        return static_cast<ElementType>(static_cast<std::uint8_t>(values.index() + 1U));
    }
    // width * height as uint64.
    [[nodiscard]] std::uint64_t cell_count() const noexcept {
        return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    }
    // The cells as T, or nullptr if the grid holds another element type.
    template <class T>
    [[nodiscard]] const std::vector<T>* get_if() const noexcept {
        return std::get_if<std::vector<T>>(&values);
    }
};

// A zero-filled grid of the given type (dimensions not validated).
[[nodiscard]] LayerGrid make_layer_grid(ElementType type, std::uint32_t width, std::uint32_t height);

// True if 1 <= width, height <= 65536 and the buffer size is width * height.
[[nodiscard]] bool is_valid_layer_grid(const LayerGrid& grid) noexcept;

// --- Blob header (P0-02 7.1) -------------------------------------------------

// Codec byte of the header (7.1 offset 7).
enum class Codec : std::uint8_t {
    raw = 0,           // 7.2
    delta_varint = 1,  // 7.3, the default
};

// Short stable name of a codec: "raw" or "delta_varint" (the enumerator
// names), for messages and the viewer; "" for a value that is not a codec.
// Not used in report text, which prints the codec number (P0-04 7).
[[nodiscard]] std::string_view codec_name(Codec codec) noexcept;

inline constexpr std::size_t BLOB_HEADER_SIZE = 24;                                     // 7.1
inline constexpr std::array<std::uint8_t, 4> BLOB_MAGIC = {0x55U, 0x4DU, 0x4CU, 0x59U};  // "UMLY"
inline constexpr std::uint16_t BLOB_FORMAT_VERSION = 1;                                 // 7.1 offset 4

// The decoded fields of a 24-byte blob header (7.1).
struct BlobHeader {
    ElementType type = ElementType::u8;
    Codec codec = Codec::delta_varint;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint64_t payload_length = 0;
};

// --- Decoder errors (P0-02 7.3, 12.1) ----------------------------------------

// Why a blob was rejected as corrupt. One code per rejection listed in 7.1,
// 7.3 and 12.1, plus a few the spec implies (truncation, LEB128 overflow).
enum class BlobError : std::uint8_t {
    none = 0,                 // success
    too_short,                // fewer than 24 bytes, so no complete header
    bad_magic,                // bytes 0..3 are not "UMLY"
    bad_version,              // blob format version != 1
    bad_element_type,         // type code not 1..8
    bad_codec,                // codec not 0 or 1
    bad_dimensions,           // width or height 0 or above 65536
    payload_length_mismatch,  // header payload length != blob length - 24
    raw_size_mismatch,        // codec 0: payload length != width * height * element size
    truncated,                // codec 1: payload ends inside a LEB128 or before all elements
    leb128_overflow,          // codec 1: a LEB128 value does not fit in 64 bits
    non_shortest_leb128,      // codec 1: a LEB128 with a redundant final 0x00 group, e.g. 80 00
    consecutive_zero_runs,    // codec 1: a zero-run token directly after a zero-run token
    run_overrun,              // codec 1: a zero run longer than the elements left
    trailing_bytes,           // codec 1: bytes left after the last element
    value_out_of_range,       // codec 1: a decoded value does not fit the element type
};

// Short stable name of a blob error, e.g. "bad_magic".
[[nodiscard]] const char* blob_error_name(BlobError error) noexcept;

// Result of parse_blob_header: header when ok(), else error says why.
struct BlobHeaderParse {
    BlobHeader header;
    BlobError error = BlobError::none;
    [[nodiscard]] bool ok() const noexcept { return error == BlobError::none; }
};

// Parses and validates the 24-byte header of a whole blob (7.1): magic,
// version, type, codec, width and height ranges, and payload length ==
// blob.size() - 24. Does not look at the payload. Never throws.
[[nodiscard]] BlobHeaderParse parse_blob_header(std::span<const std::uint8_t> blob) noexcept;

// Result of decode_layer_blob: grid and codec when ok(), else error says
// why (and grid is empty).
struct BlobDecode {
    LayerGrid grid;
    Codec codec = Codec::delta_varint;
    BlobError error = BlobError::none;
    [[nodiscard]] bool ok() const noexcept { return error == BlobError::none; }
};

// Decodes a whole blob (header and payload) with every check of 7.1 to 7.3.
// The checks apply in this order: header fields as parse_blob_header; then
// the payload in a single forward pass, reporting the first problem met.
// Throws std::bad_alloc only if the grid cannot be allocated; never throws
// FatalError.
[[nodiscard]] BlobDecode decode_layer_blob(std::span<const std::uint8_t> blob);

// --- Encoder (P0-02 7.1 to 7.3) ----------------------------------------------

// Encodes a grid as a complete blob (24-byte header and payload) with the
// given codec. The output is a pure function of the grid and codec, and
// codec 1 output is the unique canonical encoding of 7.3. FatalError if the
// grid is invalid (is_valid_layer_grid) or the codec value is unknown.
[[nodiscard]] std::vector<std::uint8_t> encode_layer_blob(const LayerGrid& grid,
                                                          Codec codec = Codec::delta_varint);

}  // namespace worldgen::db
