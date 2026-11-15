#pragma once

// World database read API, P0 part (spec P0-02 section 9).
//
// Used by worldgen-cli render, report and hash --detail, and later by the
// GDExtension. Every operation returns errors as values; no exception
// escapes the API (std::bad_alloc is caught and reported as
// out_of_memory). A reader is used by one thread at a time; open one
// reader per thread. Every result is a copy owned by the caller, and only
// integers are returned.

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
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

// --- Tables and raw rows (P0-02 14.10; P0-04 sections 7, 8) -----------------

// Storage class of a column value. The enumerator value is the tag byte of
// the column-value encoding of P0-04 section 8 (NULL 0x00, INTEGER 0x01,
// TEXT 0x03, BLOB 0x04). There is no REAL alternative: a REAL value is the
// error real_value (P0-02 5.3, 14.10).
enum class ValueType : std::uint8_t {
    null = 0x00,
    integer = 0x01,
    text = 0x03,
    blob = 0x04,
};

// One column value of a row, as stored (P0-02 14.10). `integer` is set for
// INTEGER and 0 otherwise; `bytes` holds the stored bytes of TEXT (UTF-8,
// not validated) or BLOB and is empty otherwise.
struct ColumnValue {
    ValueType type = ValueType::null;
    std::int64_t integer = 0;
    std::string bytes;

    friend bool operator==(const ColumnValue&, const ColumnValue&) = default;
};

// A table name and its row count (P0-04 section 7, "[tables]").
struct TableSummary {
    std::string name;
    std::uint64_t row_count = 0;
};

// Called once per row, in order, with the row's columns in declared order.
// The span is valid only during the call.
using RowVisitor = std::function<void(std::span<const ColumnValue> row)>;

// The sqlite_schema columns visited by visit_schema_rows, in this order
// (P0-04 section 8, "schema": rootpage is excluded).
inline constexpr std::array<std::string_view, 4> SCHEMA_ROW_COLUMNS = {"type", "name", "tbl_name", "sql"};

// The version string of the linked SQLite library, sqlite3_libversion()
// (P0-04 4.1, --version), so that callers need not include SQLite.
[[nodiscard]] const char* sqlite_library_version() noexcept;

class WorldReader {
public:
    // Opens a world file read-only (SQLITE_OPEN_READONLY): never writes,
    // never creates a file, never leaves a journal (P0-02 9, "Open").
    // Validates in this order, returning the first failure:
    //   1. file_unreadable: the path is empty, not strict UTF-8 or holds a
    //      NUL byte (is_valid_utf8_path), or the file is missing, not a
    //      regular file, or unreadable;
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

    // --- Tables and raw rows (P0-02 14.10; P0-04 sections 7, 8) -------------
    //
    // Errors common to the functions below: table_not_found if no table of
    // that exact name (bytes) exists; no_primary_key if the table has none
    // (P0-02 5.3); real_value, naming table, column and row number, at the
    // first REAL value met (rows before it have already been visited);
    // sqlite_error for an SQLite failure. An exception thrown by a visitor
    // propagates to the caller.

    // Every table (sqlite_schema type 'table') except SQLite's own sqlite_*
    // tables, ascending name by bytes, with its row count (P0-04 7, 8).
    [[nodiscard]] Result<std::vector<TableSummary>> tables() const;

    // The column names of a table, in declared order (P0-04 8).
    [[nodiscard]] Result<std::vector<std::string>> table_columns(std::string_view table) const;

    // The primary-key column names of a table, in key order (P0-04 8).
    // no_primary_key if it has none.
    [[nodiscard]] Result<std::vector<std::string>> primary_key_columns(std::string_view table) const;

    // Visits every row of a table in primary-key order (ORDER BY the key
    // columns in key order, BINARY collation), each with every column in
    // declared order (P0-04 8, "table").
    [[nodiscard]] Status visit_table_rows(std::string_view table, const RowVisitor& visit) const;

    // Same, collected into a vector (convenient for small tables and tests).
    [[nodiscard]] Result<std::vector<std::vector<ColumnValue>>> table_rows(std::string_view table) const;

    // Visits every row of sqlite_schema ordered by type, then name (BINARY),
    // with the columns of SCHEMA_ROW_COLUMNS (P0-04 8, "schema").
    [[nodiscard]] Status visit_schema_rows(const RowVisitor& visit) const;

    // Same, collected into a vector.
    [[nodiscard]] Result<std::vector<std::vector<ColumnValue>>> schema_rows() const;

private:
    explicit WorldReader(std::unique_ptr<detail::ReaderState> state) noexcept;
    std::unique_ptr<detail::ReaderState> state_;
};

}  // namespace worldgen::db
