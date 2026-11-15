// World database read API, P0 part (spec P0-02 section 9).
//
// Nothing here throws to the caller: SQLite and filesystem calls report
// through return codes and std::error_code, FatalError is never raised, and
// std::bad_alloc is caught at each public entry point.

#include "worldgen/db/reader.hpp"

#include <array>
#include <map>
#include <new>
#include <utility>

#include "db/file_header.hpp"
#include "db/layer_blob_detail.hpp"
#include "db/sqlite_util.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/db/utf8_path.hpp"

namespace worldgen::db {

namespace detail {

struct ReaderState {
    Connection db;
    std::string path;                          // UTF-8, for messages
    std::map<std::string, std::string> meta;   // every meta row, byte order
    std::uint64_t seed = 0;
    std::uint32_t map_size = 0;
    std::vector<std::string> stages;
};

}  // namespace detail

namespace {

using detail::ReaderState;
using detail::Statement;

Error make_error(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}

Error out_of_memory_error() {
    return make_error(ErrorCode::out_of_memory, "out of memory while reading the world file");
}

bool is_lower_hex(std::string_view s) noexcept {
    for (const char c : s) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

// meta.stages: empty, or stage names separated by single commas (5.5).
bool parse_stages(std::string_view text, std::vector<std::string>& out) {
    out.clear();
    if (text.empty()) {
        return true;
    }
    std::size_t start = 0;
    for (;;) {
        const std::size_t comma = text.find(',', start);
        const std::string_view name =
            text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        if (!is_valid_stage_name(name)) {
            return false;
        }
        out.emplace_back(name);
        if (comma == std::string_view::npos) {
            return true;
        }
        start = comma + 1;
    }
}

// Checks the canonical forms of the required keys (5.5). Returns the key
// whose value is invalid, or an empty view.
std::string_view validate_meta_values(ReaderState& s) {
    const auto& m = s.meta;
    const std::string& hash = m.at(std::string(META_CONTENT_HASH));
    if (hash.size() != 64 || !is_lower_hex(hash)) {
        return META_CONTENT_HASH;
    }
    if (!is_canonical_version(m.at(std::string(META_GENERATOR_VERSION)))) {
        return META_GENERATOR_VERSION;
    }
    const std::optional<std::uint64_t> map_size = parse_canonical_u64(m.at(std::string(META_PARAM_MAP_SIZE)));
    bool map_size_ok = false;
    for (const std::int64_t allowed : ALLOWED_MAP_SIZES) {
        map_size_ok = map_size_ok || (map_size && *map_size == static_cast<std::uint64_t>(allowed));
    }
    if (!map_size_ok) {
        return META_PARAM_MAP_SIZE;
    }
    s.map_size = static_cast<std::uint32_t>(*map_size);
    const std::optional<std::uint64_t> seed = parse_canonical_u64(m.at(std::string(META_SEED)));
    if (!seed) {
        return META_SEED;
    }
    s.seed = *seed;
    const std::string& phrase = m.at(std::string(META_SEED_PHRASE));
    if (!phrase.empty()) {
        // Must be a phrase as seed_from_phrase leaves it: valid UTF-8,
        // trimmed, 1 to 256 bytes.
        const PhraseSeed p = seed_from_phrase(phrase);
        if (!p.ok() || p.phrase != phrase) {
            return META_SEED_PHRASE;
        }
    }
    if (!parse_stages(m.at(std::string(META_STAGES)), s.stages)) {
        return META_STAGES;
    }
    return {};
}

// Reads the 24-byte header of layer row `id` with incremental blob I/O, so
// that the payload is never read (9, "Layer list").
bool read_blob_header(sqlite3* db, std::int64_t id, std::array<std::uint8_t, BLOB_HEADER_SIZE>& header) {
    sqlite3_blob* blob = nullptr;
    if (sqlite3_blob_open(db, "main", "layer", "data", static_cast<sqlite3_int64>(id), 0, &blob) != SQLITE_OK) {
        sqlite3_blob_close(blob);
        return false;
    }
    const bool ok = sqlite3_blob_bytes(blob) >= static_cast<int>(BLOB_HEADER_SIZE) &&
                    sqlite3_blob_read(blob, header.data(), static_cast<int>(BLOB_HEADER_SIZE), 0) == SQLITE_OK;
    sqlite3_blob_close(blob);
    return ok;
}

std::string corrupt_message(std::string_view name, std::string_view why) {
    std::string m = "layer '";
    m.append(name);
    m += "' is corrupt: ";
    m.append(why);
    return m;
}

// The id, width, height and format columns of a layer row, validated
// (5.6). Returns an empty string on success, else what is wrong.
struct LayerColumns {
    std::int64_t id = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    ElementType type = ElementType::u8;
};

std::string read_columns(const Statement& stmt, int id_col, int width_col, int height_col, int format_col,
                         LayerColumns& out) {
    if (stmt.column_type(id_col) != SQLITE_INTEGER || stmt.column_type(width_col) != SQLITE_INTEGER ||
        stmt.column_type(height_col) != SQLITE_INTEGER || stmt.column_type(format_col) != SQLITE_TEXT) {
        return "a column has the wrong type";
    }
    out.id = stmt.column_int64(id_col);
    const std::int64_t width = stmt.column_int64(width_col);
    const std::int64_t height = stmt.column_int64(height_col);
    if (width < 1 || width > static_cast<std::int64_t>(MAX_LAYER_SIDE) || height < 1 ||
        height > static_cast<std::int64_t>(MAX_LAYER_SIDE)) {
        return "width and height must be 1 to 65536";
    }
    out.width = static_cast<std::uint32_t>(width);
    out.height = static_cast<std::uint32_t>(height);
    const std::optional<ElementType> type = element_type_from_name(stmt.column_text(format_col));
    if (!type) {
        return "format '" + stmt.column_text(format_col) + "' is not one of u8, i8, u16, i16, u32, i32, u64, i64";
    }
    out.type = *type;
    return {};
}

// "" if the blob header agrees with the row, else what differs.
std::string compare_header(const BlobHeader& h, const LayerColumns& c) {
    if (h.width != c.width || h.height != c.height) {
        return "blob header says " + format_u64(h.width) + " x " + format_u64(h.height) + ", the row says " +
               format_u64(c.width) + " x " + format_u64(c.height);
    }
    if (h.type != c.type) {
        return "blob header element type " + std::string(element_type_name(h.type)) + " differs from format " +
               std::string(element_type_name(c.type));
    }
    return {};
}

Error sqlite_failure(const ReaderState& s, std::string_view what) {
    std::string m = "reading '" + s.path + "' failed (";
    m.append(what);
    m += "): ";
    m += detail::error_message(s.db.get());
    return make_error(ErrorCode::sqlite_error, m);
}

}  // namespace

// --- open -------------------------------------------------------------------

namespace {

}  // namespace

WorldReader::WorldReader(std::unique_ptr<detail::ReaderState> state) noexcept : state_(std::move(state)) {}
WorldReader::WorldReader(WorldReader&& other) noexcept = default;
WorldReader& WorldReader::operator=(WorldReader&& other) noexcept = default;
WorldReader::~WorldReader() = default;

void WorldReader::close() noexcept {
    if (state_ != nullptr) {
        state_->db.close();
    }
}

Result<WorldReader> WorldReader::open(std::string_view path_utf8) {
    try {
        auto state = std::make_unique<detail::ReaderState>();
        state->path = std::string(path_utf8);
        const std::string& shown = state->path;

        // 1. File missing or unreadable (a path that cannot be named is
        // unreadable; converting it on MSVC would throw).
        if (!is_valid_utf8_path(path_utf8)) {
            return make_error(ErrorCode::file_unreadable,
                              "cannot read '" + shown + "': the path is empty, not valid UTF-8, or contains a NUL byte");
        }
        const detail::HeaderRead h = detail::read_header(path_from_utf8(path_utf8));
        if (!h.readable) {
            return make_error(ErrorCode::file_unreadable,
                              "cannot read '" + shown + "': the file is missing or unreadable");
        }
        // 2. Not an SQLite file.
        if (!h.complete || !detail::has_sqlite_magic(h.header)) {
            return make_error(ErrorCode::not_sqlite, "'" + shown + "' is not an SQLite database");
        }
        // 3a. application_id (header offset 68, the value PRAGMA
        // application_id reports). A world file is never in WAL mode
        // (6.1 offsets 18, 19); refusing WAL here also guarantees the
        // read-only open below never creates -wal or -shm files.
        const std::uint32_t application_id = detail::header_u32(h.header, 68);
        if (application_id != APPLICATION_ID) {
            return make_error(ErrorCode::not_world, "'" + shown + "' is not a world file (application_id " +
                                                        format_u64(application_id) + ", expected " +
                                                        format_u64(APPLICATION_ID) + ")");
        }
        if (h.header[18] != 1U || h.header[19] != 1U) {
            return make_error(ErrorCode::not_world, "'" + shown + "' is not a world file (it is in WAL mode)");
        }

        sqlite3* raw = nullptr;
        const int rc = sqlite3_open_v2(shown.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
        state->db = detail::Connection(raw);
        if (rc != SQLITE_OK) {
            return make_error(ErrorCode::file_unreadable,
                              "cannot open '" + shown + "': " + detail::error_message(raw));
        }

        // Read all of meta.
        {
            Statement stmt;
            if (stmt.prepare(state->db.get(), "SELECT key, value FROM meta ORDER BY key") != SQLITE_OK) {
                const int code = sqlite3_errcode(state->db.get());
                if (code == SQLITE_NOTADB) {
                    return make_error(ErrorCode::not_world,
                                      "'" + shown + "' is not a world file (SQLite header followed by garbage)");
                }
                if (code == SQLITE_ERROR) {  // no meta table, or not the expected columns
                    return make_error(ErrorCode::not_world, "'" + shown + "' is not a world file (no meta table)");
                }
                return sqlite_failure(*state, "meta");
            }
            int step = SQLITE_ROW;
            while ((step = stmt.step()) == SQLITE_ROW) {
                state->meta.emplace(stmt.column_text(0), stmt.column_text(1));
            }
            if (step != SQLITE_DONE) {
                if (sqlite3_errcode(state->db.get()) == SQLITE_NOTADB) {
                    return make_error(ErrorCode::not_world,
                                      "'" + shown + "' is not a world file (SQLite header followed by garbage)");
                }
                return sqlite_failure(*state, "meta");
            }
        }
        const auto& rows = state->meta;

        // 3b. meta.format.
        const auto format = rows.find(std::string(META_FORMAT));
        if (format == rows.end() || format->second != WORLD_FORMAT) {
            return make_error(ErrorCode::not_world, "'" + shown + "' is not a world file (meta.format is not '" +
                                                        std::string(WORLD_FORMAT) + "')");
        }
        // 4. Schema version: user_version (header offset 60), then
        // meta.schema_version when present.
        const std::string supported = format_i64(SCHEMA_VERSION);
        const std::uint32_t user_version = detail::header_u32(h.header, 60);
        if (static_cast<std::int64_t>(user_version) != SCHEMA_VERSION) {
            return make_error(ErrorCode::unsupported_schema_version,
                              "world schema version " + format_u64(user_version) + ", this build reads " + supported);
        }
        const auto schema = rows.find(std::string(META_SCHEMA_VERSION));
        if (schema != rows.end() && schema->second != supported) {
            return make_error(ErrorCode::unsupported_schema_version,
                              "world schema version " + schema->second + ", this build reads " + supported);
        }
        // 5. Required keys.
        for (const std::string_view key : REQUIRED_META_KEYS) {
            if (rows.find(std::string(key)) == rows.end()) {
                return make_error(ErrorCode::meta_key_missing,
                                  "world file '" + shown + "' has no meta key '" + std::string(key) + "'");
            }
        }
        // 6. Canonical values.
        if (const std::string_view bad = validate_meta_values(*state); !bad.empty()) {
            return make_error(ErrorCode::meta_value_invalid, "world file '" + shown + "' has an invalid value '" +
                                                                 rows.at(std::string(bad)) + "' for meta key '" +
                                                                 std::string(bad) + "'");
        }
        return WorldReader(std::move(state));
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

// --- meta -------------------------------------------------------------------

std::optional<std::string> WorldReader::meta(std::string_view key) const {
    if (state_ == nullptr) {
        return std::nullopt;
    }
    const auto it = state_->meta.find(std::string(key));
    if (it == state_->meta.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<MetaEntry> WorldReader::meta_entries() const {
    std::vector<MetaEntry> out;
    if (state_ != nullptr) {
        for (const auto& [key, value] : state_->meta) {
            out.push_back(MetaEntry{key, value});
        }
    }
    return out;
}

std::uint64_t WorldReader::seed() const noexcept {
    return state_ != nullptr ? state_->seed : 0U;
}

std::int64_t WorldReader::schema_version() const noexcept {
    return state_ != nullptr ? SCHEMA_VERSION : 0;  // open() refuses any other version
}

std::string WorldReader::generator_version() const {
    return meta(META_GENERATOR_VERSION).value_or(std::string());
}

std::string WorldReader::seed_phrase() const {
    return meta(META_SEED_PHRASE).value_or(std::string());
}

std::uint32_t WorldReader::map_size() const noexcept {
    return state_ != nullptr ? state_->map_size : 0U;
}

std::vector<std::string> WorldReader::stages() const {
    return state_ != nullptr ? state_->stages : std::vector<std::string>();
}

std::string WorldReader::content_hash() const {
    return meta(META_CONTENT_HASH).value_or(std::string());
}

// --- layers -----------------------------------------------------------------

Result<std::vector<LayerInfo>> WorldReader::layers() const {
    try {
        if (state_ == nullptr || !state_->db.is_open()) {
            return make_error(ErrorCode::sqlite_error, "the world reader is closed");
        }
        const ReaderState& s = *state_;
        Statement stmt;
        if (stmt.prepare(s.db.get(), "SELECT id, name, width, height, format, length(data) FROM layer ORDER BY id") !=
            SQLITE_OK) {
            return sqlite_failure(s, "layer list");
        }
        std::vector<LayerInfo> out;
        int step = SQLITE_ROW;
        while ((step = stmt.step()) == SQLITE_ROW) {
            LayerInfo info;
            info.name = stmt.column_text(1);
            LayerColumns cols;
            if (std::string why = read_columns(stmt, 0, 2, 3, 4, cols); !why.empty()) {
                return make_error(ErrorCode::corrupt_layer, corrupt_message(info.name, why));
            }
            if (stmt.column_type(5) != SQLITE_INTEGER || stmt.column_int64(5) < 0) {
                return make_error(ErrorCode::corrupt_layer, corrupt_message(info.name, "data is not a blob"));
            }
            const auto size = static_cast<std::uint64_t>(stmt.column_int64(5));
            std::array<std::uint8_t, BLOB_HEADER_SIZE> header{};
            if (size < BLOB_HEADER_SIZE || !read_blob_header(s.db.get(), cols.id, header)) {
                return make_error(ErrorCode::corrupt_layer,
                                  corrupt_message(info.name, blob_error_name(BlobError::too_short)));
            }
            const BlobHeaderParse parsed = detail::parse_blob_header_fields(header, size);
            if (!parsed.ok()) {
                return make_error(ErrorCode::corrupt_layer, corrupt_message(info.name, blob_error_name(parsed.error)));
            }
            if (std::string why = compare_header(parsed.header, cols); !why.empty()) {
                return make_error(ErrorCode::corrupt_layer, corrupt_message(info.name, why));
            }
            info.id = cols.id;
            info.width = cols.width;
            info.height = cols.height;
            info.type = cols.type;
            info.codec = parsed.header.codec;
            info.encoded_size = size;
            out.push_back(std::move(info));
        }
        if (step != SQLITE_DONE) {
            return sqlite_failure(s, "layer list");
        }
        return out;
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

Result<Layer> WorldReader::read_layer(std::string_view name) const {
    try {
        if (state_ == nullptr || !state_->db.is_open()) {
            return make_error(ErrorCode::sqlite_error, "the world reader is closed");
        }
        const ReaderState& s = *state_;
        Statement stmt;
        if (stmt.prepare(s.db.get(), "SELECT id, width, height, format, data FROM layer WHERE name = ?1") !=
                SQLITE_OK ||
            stmt.bind_text(1, name) != SQLITE_OK) {
            return sqlite_failure(s, "layer read");
        }
        const int step = stmt.step();
        if (step == SQLITE_DONE) {
            return make_error(ErrorCode::layer_not_found,
                              "world file '" + s.path + "' has no layer '" + std::string(name) + "'");
        }
        if (step != SQLITE_ROW) {
            return sqlite_failure(s, "layer read");
        }
        LayerColumns cols;
        if (std::string why = read_columns(stmt, 0, 1, 2, 3, cols); !why.empty()) {
            return make_error(ErrorCode::corrupt_layer, corrupt_message(name, why));
        }
        if (stmt.column_type(4) != SQLITE_BLOB) {
            return make_error(ErrorCode::corrupt_layer, corrupt_message(name, "data is not a blob"));
        }
        BlobDecode decoded = decode_layer_blob(stmt.column_blob(4));
        if (!decoded.ok()) {
            return make_error(ErrorCode::corrupt_layer, corrupt_message(name, blob_error_name(decoded.error)));
        }
        BlobHeader header;
        header.width = decoded.grid.width;
        header.height = decoded.grid.height;
        header.type = decoded.grid.type();
        if (std::string why = compare_header(header, cols); !why.empty()) {
            return make_error(ErrorCode::corrupt_layer, corrupt_message(name, why));
        }
        Layer layer;
        layer.id = cols.id;
        layer.name = std::string(name);
        layer.codec = decoded.codec;
        layer.grid = std::move(decoded.grid);
        return layer;
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

// --- tables and raw rows (P0-02 14.10; P0-04 sections 7, 8) ----------------

const char* sqlite_library_version() noexcept {
    return sqlite3_libversion();
}

namespace {

// An SQL identifier in double quotes, inner quotes doubled.
std::string quote_identifier(std::string_view name) {
    std::string out = "\"";
    for (const char c : name) {
        out += c;
        if (c == '"') {
            out += '"';
        }
    }
    out += '"';
    return out;
}

Error closed_error() {
    return make_error(ErrorCode::sqlite_error, "the world reader is closed");
}

// True if a table of exactly that name exists; std::nullopt on an SQLite error.
std::optional<bool> table_exists(const ReaderState& s, std::string_view table) {
    Statement stmt;
    if (stmt.prepare(s.db.get(), "SELECT count(*) FROM sqlite_schema WHERE type = 'table' AND name = ?1") !=
            SQLITE_OK ||
        stmt.bind_text(1, table) != SQLITE_OK || stmt.step() != SQLITE_ROW) {
        return std::nullopt;
    }
    return stmt.column_int64(0) > 0;
}

Error table_not_found_error(const ReaderState& s, std::string_view table) {
    return make_error(ErrorCode::table_not_found,
                      "world file '" + s.path + "' has no table '" + std::string(table) + "'");
}

// Column names in declared order and primary-key columns in key order, from
// pragma_table_info. The table must exist.
struct TableShape {
    std::vector<std::string> columns;
    std::vector<std::string> key;  // empty: no primary key
};

Result<TableShape> table_shape(const ReaderState& s, std::string_view table) {
    const std::optional<bool> exists = table_exists(s, table);
    if (!exists) {
        return sqlite_failure(s, "table list");
    }
    if (!*exists) {
        return table_not_found_error(s, table);
    }
    Statement stmt;
    if (stmt.prepare(s.db.get(), "SELECT name, pk FROM pragma_table_info(?1) ORDER BY cid") != SQLITE_OK ||
        stmt.bind_text(1, table) != SQLITE_OK) {
        return sqlite_failure(s, "table columns");
    }
    TableShape shape;
    std::vector<std::pair<std::int64_t, std::string>> key;  // (position in key, name)
    int step = SQLITE_ROW;
    while ((step = stmt.step()) == SQLITE_ROW) {
        std::string name = stmt.column_text(0);
        const std::int64_t pk = stmt.column_int64(1);
        if (pk > 0) {
            key.emplace_back(pk, name);
        }
        shape.columns.push_back(std::move(name));
    }
    if (step != SQLITE_DONE) {
        return sqlite_failure(s, "table columns");
    }
    // pk positions are 1..n, each once: place each name at its position.
    shape.key.resize(key.size());
    for (auto& [position, name] : key) {
        if (position < 1 || static_cast<std::uint64_t>(position) > key.size()) {
            return sqlite_failure(s, "table columns");
        }
        shape.key[static_cast<std::size_t>(position - 1)] = std::move(name);
    }
    return shape;
}

Error no_primary_key_error(const ReaderState& s, std::string_view table) {
    return make_error(ErrorCode::no_primary_key, "table '" + std::string(table) + "' of world file '" + s.path +
                                                     "' has no primary key, so its rows have no defined order");
}

// Runs a prepared SELECT and passes each row, converted to column values,
// to visit. `what` names the table for messages, `columns` the column names.
Status visit_statement(const ReaderState& s, Statement& stmt, std::string_view what,
                       const std::vector<std::string>& columns, const RowVisitor& visit) {
    const int count = sqlite3_column_count(stmt.get());
    std::vector<ColumnValue> row(static_cast<std::size_t>(count));
    std::uint64_t row_number = 0;
    int step = SQLITE_ROW;
    while ((step = stmt.step()) == SQLITE_ROW) {
        ++row_number;
        for (int c = 0; c < count; ++c) {
            ColumnValue& v = row[static_cast<std::size_t>(c)];
            v.integer = 0;
            v.bytes.clear();
            switch (stmt.column_type(c)) {
                case SQLITE_NULL:
                    v.type = ValueType::null;
                    break;
                case SQLITE_INTEGER:
                    v.type = ValueType::integer;
                    v.integer = stmt.column_int64(c);
                    break;
                case SQLITE_TEXT:
                    v.type = ValueType::text;
                    v.bytes = stmt.column_text(c);
                    break;
                case SQLITE_BLOB: {
                    v.type = ValueType::blob;
                    const std::span<const std::uint8_t> blob = stmt.column_blob(c);
                    v.bytes.assign(reinterpret_cast<const char*>(blob.data()), blob.size());
                    break;
                }
                default: {  // SQLITE_FLOAT
                    const std::string column =
                        static_cast<std::size_t>(c) < columns.size() ? columns[static_cast<std::size_t>(c)] : "?";
                    return make_error(ErrorCode::real_value,
                                      "world file '" + s.path + "' holds a REAL value in " + std::string(what) +
                                          ", column '" + column + "', row " + format_u64(row_number) +
                                          "; world files store integers, text and blobs only");
                }
            }
        }
        visit(std::span<const ColumnValue>(row));
    }
    if (step != SQLITE_DONE) {
        return sqlite_failure(s, what);
    }
    return Ok{};
}

}  // namespace

Result<std::vector<TableSummary>> WorldReader::tables() const {
    try {
        if (state_ == nullptr || !state_->db.is_open()) {
            return closed_error();
        }
        const ReaderState& s = *state_;
        std::vector<TableSummary> out;
        {
            // SQLite reserves every name starting with "sqlite_" in any case.
            Statement stmt;
            if (stmt.prepare(s.db.get(),
                             "SELECT name FROM sqlite_schema WHERE type = 'table' AND "
                             "lower(substr(name, 1, 7)) <> 'sqlite_' ORDER BY name COLLATE BINARY") != SQLITE_OK) {
                return sqlite_failure(s, "table list");
            }
            int step = SQLITE_ROW;
            while ((step = stmt.step()) == SQLITE_ROW) {
                out.push_back(TableSummary{stmt.column_text(0), 0});
            }
            if (step != SQLITE_DONE) {
                return sqlite_failure(s, "table list");
            }
        }
        for (TableSummary& t : out) {
            Statement stmt;
            if (stmt.prepare(s.db.get(), "SELECT count(*) FROM " + quote_identifier(t.name)) != SQLITE_OK ||
                stmt.step() != SQLITE_ROW) {
                return sqlite_failure(s, "row count");
            }
            t.row_count = static_cast<std::uint64_t>(stmt.column_int64(0));
        }
        return out;
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

Result<std::vector<std::string>> WorldReader::table_columns(std::string_view table) const {
    try {
        if (state_ == nullptr || !state_->db.is_open()) {
            return closed_error();
        }
        Result<TableShape> shape = table_shape(*state_, table);
        if (!shape.ok()) {
            return shape.error();
        }
        return std::move(shape.value().columns);
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

Result<std::vector<std::string>> WorldReader::primary_key_columns(std::string_view table) const {
    try {
        if (state_ == nullptr || !state_->db.is_open()) {
            return closed_error();
        }
        Result<TableShape> shape = table_shape(*state_, table);
        if (!shape.ok()) {
            return shape.error();
        }
        if (shape.value().key.empty()) {
            return no_primary_key_error(*state_, table);
        }
        return std::move(shape.value().key);
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

Status WorldReader::visit_table_rows(std::string_view table, const RowVisitor& visit) const {
    try {
        if (state_ == nullptr || !state_->db.is_open()) {
            return closed_error();
        }
        const ReaderState& s = *state_;
        Result<TableShape> shape = table_shape(s, table);
        if (!shape.ok()) {
            return shape.error();
        }
        const TableShape& t = shape.value();
        if (t.key.empty()) {
            return no_primary_key_error(s, table);
        }
        std::string sql = "SELECT ";
        for (std::size_t i = 0; i < t.columns.size(); ++i) {
            if (i > 0) {
                sql += ", ";
            }
            sql += quote_identifier(t.columns[i]);
        }
        sql += " FROM " + quote_identifier(table) + " ORDER BY ";
        for (std::size_t i = 0; i < t.key.size(); ++i) {
            if (i > 0) {
                sql += ", ";
            }
            sql += quote_identifier(t.key[i]) + " COLLATE BINARY";
        }
        Statement stmt;
        if (stmt.prepare(s.db.get(), sql) != SQLITE_OK) {
            return sqlite_failure(s, "table rows");
        }
        return visit_statement(s, stmt, "table '" + std::string(table) + "'", t.columns, visit);
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

Result<std::vector<std::vector<ColumnValue>>> WorldReader::table_rows(std::string_view table) const {
    try {
        std::vector<std::vector<ColumnValue>> rows;
        Status status = visit_table_rows(table, [&rows](std::span<const ColumnValue> row) {
            rows.emplace_back(row.begin(), row.end());
        });
        if (!status.ok()) {
            return status.error();
        }
        return rows;
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

Status WorldReader::visit_schema_rows(const RowVisitor& visit) const {
    try {
        if (state_ == nullptr || !state_->db.is_open()) {
            return closed_error();
        }
        const ReaderState& s = *state_;
        Statement stmt;
        if (stmt.prepare(s.db.get(),
                         "SELECT type, name, tbl_name, sql FROM sqlite_schema "
                         "ORDER BY type COLLATE BINARY, name COLLATE BINARY") != SQLITE_OK) {
            return sqlite_failure(s, "schema rows");
        }
        const std::vector<std::string> columns(SCHEMA_ROW_COLUMNS.begin(), SCHEMA_ROW_COLUMNS.end());
        return visit_statement(s, stmt, "sqlite_schema", columns, visit);
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

Result<std::vector<std::vector<ColumnValue>>> WorldReader::schema_rows() const {
    try {
        std::vector<std::vector<ColumnValue>> rows;
        Status status = visit_schema_rows([&rows](std::span<const ColumnValue> row) {
            rows.emplace_back(row.begin(), row.end());
        });
        if (!status.ok()) {
            return status.error();
        }
        return rows;
    } catch (const std::bad_alloc&) {
        return out_of_memory_error();
    }
}

}  // namespace worldgen::db
