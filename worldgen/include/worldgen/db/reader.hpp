#pragma once

// World database read API, P0 part (spec P0-02 section 9).
//
// Used by worldgen-cli render, report and hash --detail, and later by the
// GDExtension. Every operation returns errors as values; no exception
// escapes the API (std::bad_alloc is caught and reported as
// out_of_memory). A reader is used by one thread at a time; open one
// reader per thread. Every result is a copy owned by the caller, and only
// integers are returned.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/result.hpp"

namespace worldgen::db {

namespace detail {
struct ReaderState;  // the read-only connection and the loaded meta rows
}

// One meta row (P0-02 5.5).
struct MetaEntry {
    std::string key;
    std::string value;
};

// One row of the layer list, read without reading or decoding the payload
// (P0-02 9, "Layer list"). type comes from the `format` column; codec from
// the blob header, of which only the 24 header bytes are read.
struct LayerInfo {
    std::int64_t id = 0;
    std::string name;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    ElementType type = ElementType::u8;
    Codec codec = Codec::delta_varint;
    std::uint64_t encoded_size = 0;  // bytes of the whole blob, header included
};

// A decoded layer (P0-02 9, "Layer read").
struct Layer {
    std::int64_t id = 0;
    std::string name;
    Codec codec = Codec::delta_varint;
    LayerGrid grid;  // contiguous row-major buffer of the element type
};

class WorldReader {
public:
    // Opens a world file read-only (SQLITE_OPEN_READONLY): never writes,
    // never creates a file, never leaves a journal (P0-02 9, "Open").
    // Validates in this order, returning the first failure:
    //   1. file_unreadable: missing, not a regular file, or unreadable;
    //   2. not_sqlite: no SQLite header;
    //   3. not_world: application_id != APPLICATION_ID, the file is in WAL
    //      mode, the SQLite header is followed by garbage, no meta table,
    //      or meta.format missing or wrong (P0-02 14.2);
    //   4. unsupported_schema_version: user_version or meta.schema_version
    //      != SCHEMA_VERSION ("world schema version X, this build reads Y");
    //   5. meta_key_missing: a key of REQUIRED_META_KEYS absent;
    //   6. meta_value_invalid: a required value not in canonical form.
    // The meta table is read once here; the meta accessors below do no I/O.
    [[nodiscard]] static Result<WorldReader> open(std::string_view path_utf8);

    WorldReader(WorldReader&& other) noexcept;
    WorldReader& operator=(WorldReader&& other) noexcept;
    WorldReader(const WorldReader&) = delete;
    WorldReader& operator=(const WorldReader&) = delete;
    ~WorldReader();

    // Closes the file now; the destructor does the same. Afterwards the
    // meta accessors still work and layer calls return sqlite_error.
    void close() noexcept;

    // --- Meta (P0-02 9, "Meta") ---------------------------------------------

    // The value of key, or std::nullopt if absent (a normal result).
    [[nodiscard]] std::optional<std::string> meta(std::string_view key) const;
    // Every meta row, in ascending key byte order.
    [[nodiscard]] std::vector<MetaEntry> meta_entries() const;

    // Typed accessors; the values were validated by open().
    [[nodiscard]] std::uint64_t seed() const noexcept;           // meta.seed
    [[nodiscard]] std::int64_t schema_version() const noexcept;  // meta.schema_version
    [[nodiscard]] std::string generator_version() const;         // meta.generator_version
    [[nodiscard]] std::string seed_phrase() const;               // meta.seed_phrase, may be empty
    [[nodiscard]] std::uint32_t map_size() const noexcept;       // meta.param.map_size
    [[nodiscard]] std::vector<std::string> stages() const;       // meta.stages split at ','
    [[nodiscard]] std::string content_hash() const;              // meta.content_hash

    // --- Layers (P0-02 9, "Layer list", "Layer read") -----------------------

    // Every layer in id order. Reads the columns and the 24-byte blob header
    // only (SQLite incremental blob I/O); payloads are neither read nor
    // decoded. corrupt_layer, naming the layer, if a column is invalid or
    // the header fails parse_blob_header.
    [[nodiscard]] Result<std::vector<LayerInfo>> layers() const;

    // Decodes the layer called name. layer_not_found if there is none;
    // corrupt_layer, naming the layer, if a column is invalid, the blob
    // fails decode_layer_blob, or its header's width, height or element type
    // disagrees with the row.
    [[nodiscard]] Result<Layer> read_layer(std::string_view name) const;

private:
    explicit WorldReader(std::unique_ptr<detail::ReaderState> state) noexcept;
    std::unique_ptr<detail::ReaderState> state_;
};

}  // namespace worldgen::db
