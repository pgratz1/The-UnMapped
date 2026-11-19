// World database writer (spec P0-02 sections 4.2, 5, 6).

#include "worldgen/db/writer.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <tuple>
#include <utility>

#include "db/file_header.hpp"
#include "db/runtime_settings.hpp"
#include "db/sqlite_util.hpp"
#include "db/writer_state.hpp"
#include "worldgen/core/decimal.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/db/utf8_path.hpp"
#include "worldgen/version.hpp"

namespace worldgen::db {

namespace detail {

WriterState& live_state(WriterState* state, const char* operation) {
    if (state == nullptr) {
        throw FatalError(std::string("WorldWriter::") + operation + ": writer was moved from");
    }
    if (state->finalized) {
        throw FatalError(std::string("WorldWriter::") + operation + ": writer is already finalized");
    }
    return *state;
}

void insert_layer(WriterState& state, std::int64_t id, std::string_view name, std::int64_t width,
                  std::int64_t height, std::string_view format, std::span<const std::uint8_t> data) {
    Statement stmt;
    int rc = stmt.prepare(state.db.get(),
                          "INSERT INTO layer (id, name, width, height, format, data) VALUES (?1, ?2, ?3, ?4, ?5, ?6)");
    if (rc == SQLITE_OK) rc = stmt.bind_int64(1, id);
    if (rc == SQLITE_OK) rc = stmt.bind_text(2, name);
    if (rc == SQLITE_OK) rc = stmt.bind_int64(3, width);
    if (rc == SQLITE_OK) rc = stmt.bind_int64(4, height);
    if (rc == SQLITE_OK) rc = stmt.bind_text(5, format);
    if (rc == SQLITE_OK) rc = stmt.bind_blob(6, data);
    if (rc == SQLITE_OK) rc = stmt.step() == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    if (rc != SQLITE_OK) {
        std::string m = "world database: inserting layer '";
        m.append(name);
        m += "' (id ";
        m += format_i64(id);
        m += ") failed: ";
        m += error_message(state.db.get());
        throw FatalError(m);
    }
}

}  // namespace detail

namespace {

using detail::Connection;
using detail::Statement;
using detail::WriterState;

[[noreturn]] void fatal_text(const std::string& message) {
    throw FatalError(message);
}

// --- steps 1-2: working database (P0-02 4.2, 5.4, 6) -------------------------

std::string query_single_text(sqlite3* db, std::string_view sql) {
    Statement stmt;
    if (stmt.prepare(db, sql) != SQLITE_OK || stmt.step() != SQLITE_ROW) {
        fatal_text("world database: '" + std::string(sql) + "' failed: " + detail::error_message(db));
    }
    return stmt.column_text(0);
}

std::int64_t query_single_int(sqlite3* db, std::string_view sql) {
    Statement stmt;
    if (stmt.prepare(db, sql) != SQLITE_OK || stmt.step() != SQLITE_ROW ||
        stmt.column_type(0) != SQLITE_INTEGER) {
        fatal_text("world database: '" + std::string(sql) + "' failed: " + detail::error_message(db));
    }
    return stmt.column_int64(0);
}

void expect_setting(bool ok, const char* what) {
    if (!ok) {
        fatal_text(std::string("world database: working database setting not applied: ") + what);
    }
}

Connection open_working_database() {
    sqlite3* raw = nullptr;
    const int rc = sqlite3_open_v2(detail::WORKING_DB_NAME, &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    Connection db(raw);
    if (rc != SQLITE_OK) {
        fatal_text("world database: cannot open the in-memory working database: " + detail::error_message(raw));
    }
    // Pragmas of 4.2, in order.
    for (const std::string_view pragma : detail::WORKING_DB_PRAGMAS) {
        detail::exec_or_fatal(db.get(), pragma, pragma);
    }
    const std::string user_version = std::string(detail::USER_VERSION_PRAGMA_PREFIX) + format_i64(SCHEMA_VERSION);
    detail::exec_or_fatal(db.get(), user_version, user_version);

    // Read the settings back: a pragma that SQLite silently ignores would
    // otherwise go unnoticed.
    expect_setting(query_single_int(db.get(), "PRAGMA page_size") == static_cast<std::int64_t>(PAGE_SIZE),
                   "page_size");
    expect_setting(query_single_text(db.get(), "PRAGMA encoding") == "UTF-8", "encoding");
    expect_setting(query_single_int(db.get(), "PRAGMA auto_vacuum") == 0, "auto_vacuum");
    expect_setting(query_single_text(db.get(), "PRAGMA journal_mode") == "memory", "journal_mode");
    expect_setting(query_single_int(db.get(), "PRAGMA temp_store") == 2, "temp_store");
    expect_setting(query_single_int(db.get(), "PRAGMA foreign_keys") == 0, "foreign_keys");
    expect_setting(query_single_int(db.get(), "PRAGMA application_id") == static_cast<std::int64_t>(APPLICATION_ID),
                   "application_id");
    expect_setting(query_single_int(db.get(), "PRAGMA user_version") == SCHEMA_VERSION, "user_version");

    // Step 2: the DDL of 5.4 in one transaction.
    detail::exec_or_fatal(db.get(), "BEGIN", "BEGIN (schema)");
    for (const std::string_view ddl : DDL_STATEMENTS) {
        detail::exec_or_fatal(db.get(), ddl, ddl);
    }
    detail::exec_or_fatal(db.get(), "COMMIT", "COMMIT (schema)");
    return db;
}

// --- meta (P0-02 5.5) ---------------------------------------------------------

std::string join_stages(const std::vector<std::string>& stages) {
    std::string out;
    for (std::size_t i = 0; i < stages.size(); ++i) {
        if (i > 0) {
            out += ',';
        }
        out += stages[i];
    }
    return out;
}

// The complete meta table as it will be written, keyed and therefore
// ordered by byte order (std::string compares as unsigned char).
std::map<std::string, std::string> build_meta(const WriterState& s) {
    std::map<std::string, std::string> meta = s.params;
    meta[std::string(META_CONTENT_HASH)] = *s.content_hash;
    meta[std::string(META_FORMAT)] = std::string(WORLD_FORMAT);
    meta[std::string(META_GENERATOR_VERSION)] = std::string(GENERATOR_VERSION);
    // P3-04 9.4: the last year of history + 1, = param.history_span, for
    // every world (finalize requires param.history_span).
    if (const auto span = s.params.find(std::string(META_PARAM_HISTORY_SPAN)); span != s.params.end()) {
        meta[std::string(META_HISTORY_END_YEAR)] = span->second;
    }
    meta[std::string(META_SCHEMA_VERSION)] = format_i64(SCHEMA_VERSION);
    meta[std::string(META_SEED)] = format_u64(*s.seed);
    meta[std::string(META_SEED_PHRASE)] = s.seed_phrase;
    meta[std::string(META_STAGES)] = join_stages(s.stages);
    return meta;
}

// Step 4.
void write_meta(WriterState& s, const std::map<std::string, std::string>& meta) {
    std::vector<const std::pair<const std::string, std::string>*> rows;
    rows.reserve(meta.size());
    for (const auto& row : meta) {
        rows.push_back(&row);
    }
    if (s.meta_order == testing::MetaOrder::descending) {
        std::reverse(rows.begin(), rows.end());
    }
    sqlite3* db = s.db.get();
    detail::exec_or_fatal(db, "BEGIN", "BEGIN (meta)");
    for (const auto* row : rows) {
        Statement stmt;
        int rc = stmt.prepare(db, "INSERT INTO meta (key, value) VALUES (?1, ?2)");
        if (rc == SQLITE_OK) rc = stmt.bind_text(1, row->first);
        if (rc == SQLITE_OK) rc = stmt.bind_text(2, row->second);
        if (rc == SQLITE_OK) rc = stmt.step() == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
        if (rc != SQLITE_OK) {
            fatal_text("world database: inserting meta key '" + row->first + "' failed: " + detail::error_message(db));
        }
    }
    detail::exec_or_fatal(db, "COMMIT", "COMMIT (meta)");
}

// Step 5.
void integrity_check(sqlite3* db) {
    Statement stmt;
    if (stmt.prepare(db, "PRAGMA integrity_check") != SQLITE_OK) {
        fatal_text("world database: integrity_check failed: " + detail::error_message(db));
    }
    std::vector<std::string> rows;
    int rc = SQLITE_ROW;
    while ((rc = stmt.step()) == SQLITE_ROW) {
        rows.push_back(stmt.column_text(0));
    }
    if (rc != SQLITE_DONE) {
        fatal_text("world database: integrity_check failed: " + detail::error_message(db));
    }
    if (rows.size() != 1 || rows[0] != "ok") {
        std::string m = "world database: integrity_check reported:";
        for (const std::string& r : rows) {
            m += ' ';
            m += r;
        }
        fatal_text(m);
    }
}

// Step 8: verify the header fields of 6.1 and the meta rows of <out>.partial.
void verify_partial(const std::filesystem::path& partial, const std::string& partial_utf8,
                    const std::map<std::string, std::string>& meta) {
    const detail::HeaderRead h = detail::read_header(partial);
    if (!h.readable || !h.complete) {
        fatal_text("world database: cannot read back the header of '" + partial_utf8 + "'");
    }
    if (const std::optional<std::string> problem = detail::check_world_header(h.header, h.file_size)) {
        fatal_text("world database: '" + partial_utf8 + "' failed verification: " + *problem);
    }
    sqlite3* raw = nullptr;
    const int rc = sqlite3_open_v2(partial_utf8.c_str(), &raw, SQLITE_OPEN_READONLY, nullptr);
    Connection db(raw);
    if (rc != SQLITE_OK) {
        fatal_text("world database: cannot reopen '" + partial_utf8 + "': " + detail::error_message(raw));
    }
    if (query_single_int(db.get(), "PRAGMA application_id") != static_cast<std::int64_t>(APPLICATION_ID) ||
        query_single_int(db.get(), "PRAGMA user_version") != SCHEMA_VERSION) {
        fatal_text("world database: '" + partial_utf8 + "' lost application_id or user_version");
    }
    {
        Statement stmt;
        if (stmt.prepare(db.get(), "SELECT key, value FROM meta ORDER BY key") != SQLITE_OK) {
            fatal_text("world database: cannot read meta of '" + partial_utf8 + "': " + detail::error_message(db.get()));
        }
        auto expected = meta.begin();
        int step = SQLITE_ROW;
        while ((step = stmt.step()) == SQLITE_ROW) {
            if (expected == meta.end() || stmt.column_type(0) != SQLITE_TEXT || stmt.column_type(1) != SQLITE_TEXT ||
                stmt.column_text(0) != expected->first || stmt.column_text(1) != expected->second) {
                fatal_text("world database: meta of '" + partial_utf8 + "' differs from what was written");
            }
            ++expected;
        }
        if (step != SQLITE_DONE || expected != meta.end()) {
            fatal_text("world database: meta of '" + partial_utf8 + "' differs from what was written");
        }
    }
    if (db.close() != SQLITE_OK) {
        fatal_text("world database: closing '" + partial_utf8 + "' failed");
    }
}

// Test hook (worldgen/db/testing.hpp): fail on purpose at a given point.
std::optional<Error> forced_failure(const WriterState& s, testing::FailurePoint point) {
    if (s.failure_point != point || point == testing::FailurePoint::none) {
        return std::nullopt;
    }
    const std::string m = "test hook: forced failure in finalize";
    if (s.failure_mode == testing::FailureMode::throw_fatal) {
        throw FatalError(m);
    }
    return Error{ErrorCode::output_write_failed, m};
}

// Deletes <out>.partial and <out>.partial-journal and closes the working
// database unless dismissed: the cleanup of P0-02 6 "On any failure", for
// returned errors and for exceptions alike. SQLite journals the VACUUM INTO
// target, so <out>.partial-journal exists while the vacuum runs and could
// survive a failure.
class FinalizeCleanup {
public:
    FinalizeCleanup(WriterState& state, std::filesystem::path partial, std::filesystem::path journal) noexcept
        : state_(state), partial_(std::move(partial)), journal_(std::move(journal)) {}
    FinalizeCleanup(const FinalizeCleanup&) = delete;
    FinalizeCleanup& operator=(const FinalizeCleanup&) = delete;
    ~FinalizeCleanup() {
        if (dismissed_) {
            return;
        }
        state_.db.close();
        std::error_code ec;
        std::filesystem::remove(partial_, ec);  // nothing more can be done if these fail
        std::filesystem::remove(journal_, ec);
    }
    void dismiss() noexcept { dismissed_ = true; }

private:
    WriterState& state_;
    std::filesystem::path partial_;
    std::filesystem::path journal_;
    bool dismissed_ = false;
};

// Removes path if it exists; an error value if it exists and cannot be removed.
std::optional<Error> remove_stale(const std::filesystem::path& path, const std::string& shown) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        return Error{ErrorCode::output_write_failed, "cannot remove old '" + shown + "': " + ec.message()};
    }
    return std::nullopt;
}

// rename(from, to), or a simulated failure while the test hook asks for one.
void rename_file(WriterState& s, const std::filesystem::path& from, const std::filesystem::path& to,
                 std::error_code& ec) {
    if (s.fs_faults.failing_renames > 0) {
        --s.fs_faults.failing_renames;
        ec = std::make_error_code(std::errc::permission_denied);
        return;
    }
    std::filesystem::rename(from, to, ec);
}

// Step 9: moves <out>.partial to <out>. Returns std::nullopt only when a
// rename call has succeeded, i.e. the new file is at <out>; every other path
// returns an error (and the caller's cleanup removes <out>.partial).
std::optional<Error> rename_into_place(WriterState& s, const std::filesystem::path& partial,
                                       const std::string& partial_utf8, const std::filesystem::path& out,
                                       const std::string& shown, Overwrite overwrite) {
    if (overwrite == Overwrite::refuse) {
        // Checked again here (also at the start of finalize); the window
        // between this check and the rename is accepted (P0-02 14.7).
        std::error_code status_ec;
        // symlink_status: anything at the path, a dangling link included,
        // counts as existing (a link is not followed).
        const std::filesystem::file_status status = std::filesystem::symlink_status(out, status_ec);
        // status() reports "not found" as an error code on some
        // implementations; only a missing file counts as "does not exist".
        if (s.fs_faults.output_status_error ||
            (status_ec && status.type() != std::filesystem::file_type::not_found)) {
            if (s.fs_faults.output_status_error) {
                status_ec = std::make_error_code(std::errc::permission_denied);  // simulated
            }
            return Error{ErrorCode::output_write_failed,
                         "cannot check whether output '" + shown + "' exists: " + status_ec.message()};
        }
        if (std::filesystem::exists(status)) {
            return Error{ErrorCode::output_exists,
                         "output '" + shown + "' already exists; allow overwriting to replace it"};
        }
    }
    // While the test hook simulates faults, the fallback is used on every
    // platform (captured before the first rename consumes a fault).
    const bool simulating = s.fs_faults.failing_renames > 0 || s.fs_faults.remove_output_error;
    std::error_code rename_ec;
    rename_file(s, partial, out, rename_ec);
    if (!rename_ec) {
        return std::nullopt;
    }
    // Replace fallback. std::filesystem::rename replaces an existing regular
    // file on POSIX (rename(2), atomic) and is expected to on MSVC (MoveFileExW
    // with MOVEFILE_REPLACE_EXISTING). Should it fail on Windows with an
    // existing target anyway, remove the target and rename once more. Not
    // atomic: if the second rename fails, the old <out> is gone. Used on
    // every platform while the test hook simulates rename failures.
#ifdef _WIN32
    const bool fallback_enabled = true;
    static_cast<void>(simulating);
#else
    const bool fallback_enabled = simulating;
#endif
    const std::string first = "cannot rename '" + partial_utf8 + "' to '" + shown + "': " + rename_ec.message();
    if (overwrite != Overwrite::replace || !fallback_enabled) {
        return Error{ErrorCode::output_rename_failed, first};
    }
    std::error_code type_ec;
    if (!std::filesystem::is_regular_file(out, type_ec) || type_ec) {
        return Error{ErrorCode::output_rename_failed, first};  // nothing to remove, or cannot tell
    }
    std::error_code remove_ec;
    const bool removed = !s.fs_faults.remove_output_error && std::filesystem::remove(out, remove_ec) && !remove_ec;
    if (!removed) {
        return Error{ErrorCode::output_rename_failed, first + "; removing the existing file also failed"};
    }
    std::error_code retry_ec;
    rename_file(s, partial, out, retry_ec);
    if (retry_ec) {
        return Error{ErrorCode::output_rename_failed,
                     first + "; the existing file was removed but the retry failed: " + retry_ec.message()};
    }
    return std::nullopt;
}

Error output_error(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}

void require_param_name(std::string_view name, const char* operation) {
    const std::string key = std::string(META_PARAM_PREFIX) + std::string(name);
    if (!is_valid_meta_key(key)) {
        throw FatalError(std::string("WorldWriter::") + operation + ": '" + key + "' is not a valid meta key");
    }
}

// The eight history keys take only their canonical text (P3-03 4.1, 4.2).
void require_canonical_history_value(const std::string& key, std::string_view value, const char* operation) {
    if (is_history_meta_key(key) && !is_canonical_history_meta_value(key, value)) {
        throw FatalError(std::string("WorldWriter::") + operation + ": '" + std::string(value) +
                         "' is not the canonical form of a value of meta key '" + key + "'");
    }
}

}  // namespace

// --- StageWriter --------------------------------------------------------------

StageWriter::StageWriter(detail::WriterState& state, std::string stage_name)
    : state_(&state), name_(std::move(stage_name)) {}

StageWriter::StageWriter(StageWriter&& other) noexcept : state_(other.state_), name_(std::move(other.name_)) {
    other.state_ = nullptr;
}

StageWriter::~StageWriter() {
    if (state_ == nullptr) {
        return;
    }
    // Not committed: roll back and leave no trace (P0-02 14.6). The rows go
    // with the transaction (so their layer names are free again), the stage
    // name was never recorded (that happens in commit), and the layer id
    // counter returns to its value at begin_stage, so ids stay gap-free.
    if (state_->db.is_open()) {
        std::string ignored;
        detail::exec(state_->db.get(), "ROLLBACK", ignored);
    }
    state_->next_layer_id = state_->stage_start_layer_id;
    state_->next_pass_id = state_->stage_start_pass_id;
    state_->last_biome_id = state_->stage_start_biome_id;
    state_->climate_written = state_->stage_start_climate;
    state_->last_region_id = state_->stage_start_region_id;
    state_->last_border_key = state_->stage_start_border_key;
    state_->last_region_biome_key = state_->stage_start_region_biome_key;
    state_->last_race_id = state_->stage_start_race_id;
    state_->last_race_biome_key = state_->stage_start_race_biome_key;
    state_->last_culture_id = state_->stage_start_culture_id;
    state_->last_god_id = state_->stage_start_god_id;
    state_->last_age_id = state_->stage_start_age_id;
    state_->history_keys = state_->stage_start_history_keys;
    state_->stage_open = false;
}

void StageWriter::write_layer(std::int64_t id, std::string_view layer_name, const LayerGrid& grid, Codec codec) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_layer: stage is not open");
    }
    if (id != state_->next_layer_id) {
        fatal_text("StageWriter::write_layer: layer '" + std::string(layer_name) + "' has id " + format_i64(id) +
                   ", expected " + format_i64(state_->next_layer_id) + " (ids are 1, 2, 3, ... in writing order)");
    }
    if (!is_valid_layer_name(layer_name)) {
        fatal_text("StageWriter::write_layer: '" + std::string(layer_name) + "' is not a valid layer name");
    }
    const std::vector<std::uint8_t> blob = encode_layer_blob(grid, codec);
    detail::insert_layer(*state_, id, layer_name, static_cast<std::int64_t>(grid.width),
                         static_cast<std::int64_t>(grid.height), element_type_name(grid.type()), blob);
    state_->next_layer_id = id + 1;
}

void StageWriter::write_pass(const PassRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_pass: stage is not open");
    }
    if (row.id != state_->next_pass_id) {
        fatal_text("StageWriter::write_pass: pass row has id " + format_i64(row.id) + ", expected " +
                   format_i64(state_->next_pass_id) + " (ids are 1, 2, 3, ... in writing order)");
    }
    if ((row.kind != 0 && row.kind != 1) || (row.capped != 0 && row.capped != 1)) {
        fatal("StageWriter::write_pass: kind and capped must be 0 or 1");
    }
    detail::Statement stmt;
    sqlite3* db = state_->db.get();
    int rc = stmt.prepare(db,
                          "INSERT INTO pass (id, range_id, kind, capped, col_x, col_y, saddle_dm, col_dm, path) "
                          "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)");
    const std::int64_t ints[8] = {row.id,    row.range_id, row.kind,      row.capped,
                                  row.col_x, row.col_y,    row.saddle_dm, row.col_dm};
    for (int k = 0; k < 8 && rc == SQLITE_OK; ++k) {
        rc = stmt.bind_int64(k + 1, ints[k]);
    }
    if (rc == SQLITE_OK) rc = stmt.bind_blob(9, row.path);
    if (rc == SQLITE_OK) rc = stmt.step() == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    if (rc != SQLITE_OK) {
        fatal_text("world database: inserting pass row " + format_i64(row.id) + " failed: " +
                   detail::error_message(db));
    }
    state_->next_pass_id = row.id + 1;
}

void StageWriter::write_biome(const BiomeRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_biome: stage is not open");
    }
    if (row.id < 1 || row.id <= state_->last_biome_id) {
        fatal_text("StageWriter::write_biome: biome row has id " + format_i64(row.id) +
                   ", expected an id above " + format_i64(state_->last_biome_id) + " (ascending ids)");
    }
    if (row.water != 0 && row.water != 1) {
        fatal("StageWriter::write_biome: water must be 0 or 1");
    }
    detail::Statement stmt;
    sqlite3* db = state_->db.get();
    int rc = stmt.prepare(db, "INSERT INTO biome (id, key, name, colour, water) VALUES (?1, ?2, ?3, ?4, ?5)");
    if (rc == SQLITE_OK) rc = stmt.bind_int64(1, row.id);
    if (rc == SQLITE_OK) rc = stmt.bind_text(2, row.key);
    if (rc == SQLITE_OK) rc = stmt.bind_text(3, row.name);
    if (rc == SQLITE_OK) rc = stmt.bind_int64(4, row.colour);
    if (rc == SQLITE_OK) rc = stmt.bind_int64(5, row.water);
    if (rc == SQLITE_OK) rc = stmt.step() == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    if (rc != SQLITE_OK) {
        fatal_text("world database: inserting biome row " + format_i64(row.id) + " failed: " +
                   detail::error_message(db));
    }
    state_->last_biome_id = row.id;
}

void StageWriter::write_climate(const ClimateRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_climate: stage is not open");
    }
    if (row.id != 1 || state_->climate_written) {
        fatal("StageWriter::write_climate: table climate has exactly one row, id 1");
    }
    if (row.wind_toward < 0 || row.wind_toward > 7) {
        fatal("StageWriter::write_climate: wind_toward must be a D8 code 0..7");
    }
    if (row.temp_offset_dc < -50 || row.temp_offset_dc > 50) {
        fatal("StageWriter::write_climate: temp_offset_dc must be in -50..50");
    }
    detail::Statement stmt;
    sqlite3* db = state_->db.get();
    int rc = stmt.prepare(db, "INSERT INTO climate (id, wind_toward, temp_offset_dc) VALUES (?1, ?2, ?3)");
    if (rc == SQLITE_OK) rc = stmt.bind_int64(1, row.id);
    if (rc == SQLITE_OK) rc = stmt.bind_int64(2, row.wind_toward);
    if (rc == SQLITE_OK) rc = stmt.bind_int64(3, row.temp_offset_dc);
    if (rc == SQLITE_OK) rc = stmt.step() == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    if (rc != SQLITE_OK) {
        fatal_text("world database: inserting the climate row failed: " + detail::error_message(db));
    }
    state_->climate_written = true;
}

namespace {

// Binds integers ?1 .. ?n of `stmt` from `values`, starting at parameter
// `first`; the first SQLite error code, or SQLITE_OK.
int bind_integers(detail::Statement& stmt, int first, std::span<const std::int64_t> values) {
    int rc = SQLITE_OK;
    for (std::size_t k = 0; k < values.size() && rc == SQLITE_OK; ++k) {
        rc = stmt.bind_int64(first + static_cast<int>(k), values[k]);
    }
    return rc;
}

// Prepares `sql`, binds `values` from ?first on (and whatever `bind_rest`
// binds), steps once; FatalError naming `what` if any step fails.
template <class BindRest>
void insert_row(sqlite3* db, std::string_view sql, int first, std::span<const std::int64_t> values,
                const BindRest& bind_rest, const std::string& what) {
    detail::Statement stmt;
    int rc = stmt.prepare(db, sql);
    if (rc == SQLITE_OK) rc = bind_rest(stmt);
    if (rc == SQLITE_OK) rc = bind_integers(stmt, first, values);
    if (rc == SQLITE_OK) rc = stmt.step() == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
    if (rc != SQLITE_OK) {
        fatal_text("world database: inserting " + what + " failed: " + detail::error_message(db));
    }
}

std::string key_text(std::pair<std::int64_t, std::int64_t> k) {
    return "(" + format_i64(k.first) + ", " + format_i64(k.second) + ")";
}

}  // namespace

void StageWriter::write_region(const RegionRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_region: stage is not open");
    }
    if (row.id < 1 || row.id <= state_->last_region_id) {
        fatal_text("StageWriter::write_region: region row has id " + format_i64(row.id) +
                   ", expected an id above " + format_i64(state_->last_region_id) + " (ascending ids)");
    }
    // Every column but `name` (?2), in declared order from ?3.
    const std::int64_t ints[26] = {
        row.biome,          row.x_min,          row.y_min,          row.x_max,         row.y_max,
        row.seed_x,         row.seed_y,         row.landmass,       row.cells,         row.lake_cells,
        row.river_cells,    row.river_class_max, row.coast_cells,   row.walk_cells,    row.walk_area,
        row.elev_mean_dm,   row.elev_max_dm,    row.fertility_mean, row.forage_mean,   row.timber_mean,
        row.ore_mean,       row.fresh_water_mean, row.habitability_mean, row.habitability_p90,
        row.ore_rich_cells, row.ore_reach_cells,
    };
    insert_row(state_->db.get(),
               "INSERT INTO region (id, name, biome, x_min, y_min, x_max, y_max, seed_x, seed_y, landmass, cells, "
               "lake_cells, river_cells, river_class_max, coast_cells, walk_cells, walk_area, elev_mean_dm, "
               "elev_max_dm, fertility_mean, forage_mean, timber_mean, ore_mean, fresh_water_mean, "
               "habitability_mean, habitability_p90, ore_rich_cells, ore_reach_cells) VALUES (?1, ?2, ?3, ?4, ?5, "
               "?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24, ?25, ?26, "
               "?27, ?28)",
               3, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.id);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(2, row.name);
                   return rc;
               },
               "region row " + format_i64(row.id));
    state_->last_region_id = row.id;
}

void StageWriter::write_region_border(const RegionBorderRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_region_border: stage is not open");
    }
    const std::pair<std::int64_t, std::int64_t> key{row.a, row.b};
    if (row.a < 1 || row.a >= row.b || key <= state_->last_border_key) {
        fatal_text("StageWriter::write_region_border: row " + key_text(key) + " is not above " +
                   key_text(state_->last_border_key) + " with 1 <= a < b (ascending keys)");
    }
    const std::int64_t ints[11] = {row.a,           row.b,          row.length_m,    row.low_dm,
                                   row.low_x,       row.low_y,      row.walk_pairs,  row.river_pairs,
                                   row.river_class_max, row.pass_id, row.passes};
    insert_row(state_->db.get(),
               "INSERT INTO region_border (a, b, length_m, low_dm, low_x, low_y, walk_pairs, river_pairs, "
               "river_class_max, pass_id, passes) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)",
               1, ints, [](detail::Statement&) { return SQLITE_OK; }, "region_border row " + key_text(key));
    state_->last_border_key = key;
}

void StageWriter::write_region_biome(const RegionBiomeRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_region_biome: stage is not open");
    }
    const std::pair<std::int64_t, std::int64_t> key{row.region, row.biome};
    if (row.region < 1 || key <= state_->last_region_biome_key) {
        fatal_text("StageWriter::write_region_biome: row " + key_text(key) + " is not above " +
                   key_text(state_->last_region_biome_key) + " with region >= 1 (ascending keys)");
    }
    const std::int64_t ints[3] = {row.region, row.biome, row.cells};
    insert_row(state_->db.get(), "INSERT INTO region_biome (region, biome, cells) VALUES (?1, ?2, ?3)", 1, ints,
               [](detail::Statement&) { return SQLITE_OK; }, "region_biome row " + key_text(key));
    state_->last_region_biome_key = key;
}

namespace {

// FatalError unless id is at least 1 and above `last` (ascending ids).
void require_next_id(const char* operation, const char* table, std::int64_t id, std::int64_t last) {
    if (id < 1 || id <= last) {
        fatal_text(std::string("StageWriter::") + operation + ": " + table + " row has id " + format_i64(id) +
                   ", expected an id above " + format_i64(last) + " (ascending ids)");
    }
}

void require_camp(const char* operation, std::int64_t camp) {
    if (camp < 0 || camp > 2) {
        fatal_text(std::string("StageWriter::") + operation + ": camp must be 0, 1 or 2");
    }
}

}  // namespace

void StageWriter::write_race(const RaceRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_race: stage is not open");
    }
    require_next_id("write_race", "race", row.id, state_->last_race_id);
    require_camp("write_race", row.camp);
    if (row.present != 0 && row.present != 1) {
        fatal("StageWriter::write_race: present must be 0 or 1");
    }
    if (row.awakening_year.has_value() != (row.present == 1)) {
        fatal("StageWriter::write_race: awakening_year must be set exactly when present is 1");
    }
    // Columns camp ... present from ?6, then awakening_year (?11) and
    // creator_god (?12).
    const std::int64_t ints[5] = {row.camp, row.lifespan_years, row.generation_years, row.culture, row.present};
    insert_row(state_->db.get(),
               "INSERT INTO race (id, key, name, plural, adjective, camp, lifespan_years, generation_years, culture, "
               "present, awakening_year, creator_god) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12)",
               6, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.id);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(2, row.key);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(3, row.name);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(4, row.plural);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(5, row.adjective);
                   if (rc == SQLITE_OK) {
                       rc = row.awakening_year ? stmt.bind_int64(11, *row.awakening_year) : stmt.bind_null(11);
                   }
                   if (rc == SQLITE_OK) rc = stmt.bind_int64(12, row.creator_god);
                   return rc;
               },
               "race row " + format_i64(row.id));
    state_->last_race_id = row.id;
}

void StageWriter::write_race_biome(const RaceBiomeRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_race_biome: stage is not open");
    }
    const std::pair<std::int64_t, std::int64_t> key{row.race, row.biome};
    if (row.race < 1 || key <= state_->last_race_biome_key) {
        fatal_text("StageWriter::write_race_biome: row " + key_text(key) + " is not above " +
                   key_text(state_->last_race_biome_key) + " with race >= 1 (ascending keys)");
    }
    const std::int64_t ints[3] = {row.race, row.biome, row.weight};
    insert_row(state_->db.get(), "INSERT INTO race_biome (race, biome, weight) VALUES (?1, ?2, ?3)", 1, ints,
               [](detail::Statement&) { return SQLITE_OK; }, "race_biome row " + key_text(key));
    state_->last_race_biome_key = key;
}

void StageWriter::write_culture(const CultureRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_culture: stage is not open");
    }
    require_next_id("write_culture", "culture", row.id, state_->last_culture_id);
    const std::int64_t ints[1] = {row.race};
    insert_row(state_->db.get(), "INSERT INTO culture (id, key, name, race) VALUES (?1, ?2, ?3, ?4)", 4, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.id);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(2, row.key);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(3, row.name);
                   return rc;
               },
               "culture row " + format_i64(row.id));
    state_->last_culture_id = row.id;
}

void StageWriter::write_god(const GodRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_god: stage is not open");
    }
    require_next_id("write_god", "god", row.id, state_->last_god_id);
    require_camp("write_god", row.camp);
    const std::int64_t ints[1] = {row.camp};
    insert_row(state_->db.get(), "INSERT INTO god (id, name, camp) VALUES (?1, ?2, ?3)", 3, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.id);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(2, row.name);
                   return rc;
               },
               "god row " + format_i64(row.id));
    state_->last_god_id = row.id;
}

void StageWriter::write_age(const AgeRow& row) {
    if (state_ == nullptr) {
        fatal("StageWriter::write_age: stage is not open");
    }
    require_next_id("write_age", "age", row.id, state_->last_age_id);
    if (row.end_year < row.start_year) {
        fatal("StageWriter::write_age: end_year must not be below start_year");
    }
    const std::int64_t ints[4] = {row.id, row.race, row.start_year, row.end_year};
    insert_row(state_->db.get(),
               "INSERT INTO age (id, race, start_year, end_year, name) VALUES (?1, ?2, ?3, ?4, ?5)", 1, ints,
               [&row](detail::Statement& stmt) { return stmt.bind_text(5, row.name); },
               "age row " + format_i64(row.id));
    state_->last_age_id = row.id;
}

// --- The history tables (P3-04 9.3) ------------------------------------------

namespace {

void require_open(const detail::WriterState* state, const char* operation) {
    if (state == nullptr) {
        fatal_text(std::string("StageWriter::") + operation + ": stage is not open");
    }
}

// FatalError unless the pair key is above `last` with a first component >= 1.
void require_next_pair(const char* operation, std::pair<std::int64_t, std::int64_t> key,
                       std::pair<std::int64_t, std::int64_t> last) {
    if (key.first < 1 || key <= last) {
        fatal_text(std::string("StageWriter::") + operation + ": row " + key_text(key) + " is not above " +
                   key_text(last) + " with a first key column >= 1 (ascending keys)");
    }
}

int bind_optional(detail::Statement& stmt, int index, const std::optional<std::int64_t>& value) {
    return value ? stmt.bind_int64(index, *value) : stmt.bind_null(index);
}

constexpr auto NO_TEXT = [](detail::Statement&) { return SQLITE_OK; };

}  // namespace

void StageWriter::write_event_kind(const EventKindRow& row) {
    require_open(state_, "write_event_kind");
    require_next_id("write_event_kind", "event_kind", row.id, state_->history_keys.event_kind);
    const std::int64_t ints[4] = {row.caused, row.region_use, row.settlement_use, row.salience};
    insert_row(state_->db.get(),
               "INSERT INTO event_kind (id, key, caused, region_use, settlement_use, salience, template) VALUES (?1, "
               "?2, ?3, ?4, ?5, ?6, ?7)",
               3, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.id);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(2, row.key);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(7, row.template_text);
                   return rc;
               },
               "event_kind row " + format_i64(row.id));
    state_->history_keys.event_kind = row.id;
}

void StageWriter::write_event_kind_role(const EventKindRoleRow& row) {
    require_open(state_, "write_event_kind_role");
    const std::pair<std::int64_t, std::int64_t> key{row.kind, row.seq};
    require_next_pair("write_event_kind_role", key, state_->history_keys.event_kind_role);
    const std::int64_t ints[3] = {row.entity_type, row.min_count, row.max_count};
    insert_row(state_->db.get(),
               "INSERT INTO event_kind_role (kind, seq, role, entity_type, min_count, max_count) VALUES (?1, ?2, ?3, "
               "?4, ?5, ?6)",
               4, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.kind);
                   if (rc == SQLITE_OK) rc = stmt.bind_int64(2, row.seq);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(3, row.role);
                   return rc;
               },
               "event_kind_role row " + key_text(key));
    state_->history_keys.event_kind_role = key;
}

void StageWriter::write_event_kind_value(const EventKindValueRow& row) {
    require_open(state_, "write_event_kind_value");
    const std::pair<std::int64_t, std::int64_t> key{row.kind, row.seq};
    require_next_pair("write_event_kind_value", key, state_->history_keys.event_kind_value);
    const std::int64_t ints[2] = {row.kind, row.seq};
    insert_row(state_->db.get(), "INSERT INTO event_kind_value (kind, seq, key) VALUES (?1, ?2, ?3)", 1, ints,
               [&row](detail::Statement& stmt) { return stmt.bind_text(3, row.key); },
               "event_kind_value row " + key_text(key));
    state_->history_keys.event_kind_value = key;
}

void StageWriter::write_phase(const PhaseRow& row) {
    require_open(state_, "write_phase");
    require_next_id("write_phase", "phase", row.id, state_->history_keys.phase);
    const std::int64_t ints[6] = {row.id, row.age, row.kind, row.start_year, row.end_year, row.trigger_event};
    insert_row(state_->db.get(),
               "INSERT INTO phase (id, age, kind, start_year, end_year, trigger_event) VALUES (?1, ?2, ?3, ?4, ?5, "
               "?6)",
               1, ints, NO_TEXT, "phase row " + format_i64(row.id));
    state_->history_keys.phase = row.id;
}

void StageWriter::write_event(const EventRow& row) {
    require_open(state_, "write_event");
    require_next_id("write_event", "event", row.id, state_->history_keys.event);
    const std::int64_t ints[8] = {row.id,     row.year,   row.end_year,   row.kind,
                                  row.phase,  row.region, row.settlement, row.salience};
    insert_row(state_->db.get(),
               "INSERT INTO event (id, year, end_year, kind, phase, region, settlement, salience) VALUES (?1, ?2, ?3, "
               "?4, ?5, ?6, ?7, ?8)",
               1, ints, NO_TEXT, "event row " + format_i64(row.id));
    state_->history_keys.event = row.id;
}

void StageWriter::write_event_participant(const EventParticipantRow& row) {
    require_open(state_, "write_event_participant");
    const std::pair<std::int64_t, std::int64_t> key{row.event, row.seq};
    require_next_pair("write_event_participant", key, state_->history_keys.event_participant);
    const std::int64_t ints[5] = {row.event, row.seq, row.role, row.entity_type, row.entity_id};
    insert_row(state_->db.get(),
               "INSERT INTO event_participant (event, seq, role, entity_type, entity_id) VALUES (?1, ?2, ?3, ?4, ?5)",
               1, ints, NO_TEXT, "event_participant row " + key_text(key));
    state_->history_keys.event_participant = key;
}

void StageWriter::write_event_cause(const EventCauseRow& row) {
    require_open(state_, "write_event_cause");
    const std::pair<std::int64_t, std::int64_t> key{row.event, row.cause};
    require_next_pair("write_event_cause", key, state_->history_keys.event_cause);
    const std::int64_t ints[2] = {row.event, row.cause};
    insert_row(state_->db.get(), "INSERT INTO event_cause (event, cause) VALUES (?1, ?2)", 1, ints, NO_TEXT,
               "event_cause row " + key_text(key));
    state_->history_keys.event_cause = key;
}

void StageWriter::write_event_value(const EventValueRow& row) {
    require_open(state_, "write_event_value");
    const std::pair<std::int64_t, std::int64_t> key{row.event, row.seq};
    require_next_pair("write_event_value", key, state_->history_keys.event_value);
    const std::int64_t ints[3] = {row.event, row.seq, row.value};
    insert_row(state_->db.get(), "INSERT INTO event_value (event, seq, value) VALUES (?1, ?2, ?3)", 1, ints, NO_TEXT,
               "event_value row " + key_text(key));
    state_->history_keys.event_value = key;
}

void StageWriter::write_civilization(const CivilizationRow& row) {
    require_open(state_, "write_civilization");
    require_next_id("write_civilization", "civilization", row.id, state_->history_keys.civilization);
    const std::int64_t ints[6] = {row.race,         row.culture,        row.home_region,
                                  row.capital,      row.founded_year,   row.founding_event};
    insert_row(state_->db.get(),
               "INSERT INTO civilization (id, name, race, culture, home_region, capital, founded_year, "
               "founding_event, ended_year, ending_event) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
               3, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.id);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(2, row.name);
                   if (rc == SQLITE_OK) rc = bind_optional(stmt, 9, row.ended_year);
                   if (rc == SQLITE_OK) rc = stmt.bind_int64(10, row.ending_event);
                   return rc;
               },
               "civilization row " + format_i64(row.id));
    state_->history_keys.civilization = row.id;
}

void StageWriter::write_settlement(const SettlementRow& row) {
    require_open(state_, "write_settlement");
    require_next_id("write_settlement", "settlement", row.id, state_->history_keys.settlement);
    const std::int64_t ints[10] = {row.x,           row.y,         row.region, row.founder_civ, row.founded_year,
                                   row.founding_event, row.owner_civ, row.kind,   row.status,      row.population};
    insert_row(state_->db.get(),
               "INSERT INTO settlement (id, name, x, y, region, founder_civ, founded_year, founding_event, "
               "owner_civ, kind, status, population, ruined_year, ruin_event) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, "
               "?9, ?10, ?11, ?12, ?13, ?14)",
               3, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.id);
                   if (rc == SQLITE_OK) rc = stmt.bind_text(2, row.name);
                   if (rc == SQLITE_OK) rc = bind_optional(stmt, 13, row.ruined_year);
                   if (rc == SQLITE_OK) rc = stmt.bind_int64(14, row.ruin_event);
                   return rc;
               },
               "settlement row " + format_i64(row.id));
    state_->history_keys.settlement = row.id;
}

void StageWriter::write_settlement_census(const SettlementCensusRow& row) {
    require_open(state_, "write_settlement_census");
    const std::pair<std::int64_t, std::int64_t> key{row.settlement, row.year};
    require_next_pair("write_settlement_census", key, state_->history_keys.settlement_census);
    const std::int64_t ints[6] = {row.settlement, row.year, row.owner_civ, row.kind, row.status, row.population};
    insert_row(state_->db.get(),
               "INSERT INTO settlement_census (settlement, year, owner_civ, kind, status, population) VALUES (?1, ?2, "
               "?3, ?4, ?5, ?6)",
               1, ints, NO_TEXT, "settlement_census row " + key_text(key));
    state_->history_keys.settlement_census = key;
}

void StageWriter::write_region_control(const RegionControlRow& row) {
    require_open(state_, "write_region_control");
    const std::pair<std::int64_t, std::int64_t> key{row.region, row.from_year};
    require_next_pair("write_region_control", key, state_->history_keys.region_control);
    const std::int64_t ints[2] = {row.civ, row.event};
    insert_row(state_->db.get(),
               "INSERT INTO region_control (region, from_year, to_year, civ, event) VALUES (?1, ?2, ?3, ?4, ?5)", 4,
               ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_int64(1, row.region);
                   if (rc == SQLITE_OK) rc = stmt.bind_int64(2, row.from_year);
                   if (rc == SQLITE_OK) rc = bind_optional(stmt, 3, row.to_year);
                   return rc;
               },
               "region_control row " + key_text(key));
    state_->history_keys.region_control = key;
}

void StageWriter::write_relation(const RelationRow& row) {
    require_open(state_, "write_relation");
    auto key = std::make_tuple(row.a_type, row.a_id, row.b_type, row.b_id, row.kind, row.since);
    if (row.a_type < 1 || key <= state_->history_keys.relation) {
        fatal_text("StageWriter::write_relation: row (" + format_i64(row.a_type) + ", " + format_i64(row.a_id) +
                   ", " + format_i64(row.b_type) + ", " + format_i64(row.b_id) + ", '" + row.kind + "', " +
                   format_i64(row.since) + ") is not above the previous row with a_type >= 1 (ascending keys)");
    }
    const std::int64_t ints[4] = {row.a_type, row.a_id, row.b_type, row.b_id};
    insert_row(state_->db.get(),
               "INSERT INTO relation (a_type, a_id, b_type, b_id, kind, since, until, since_event, until_event) "
               "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)",
               1, ints,
               [&row](detail::Statement& stmt) {
                   int rc = stmt.bind_text(5, row.kind);
                   if (rc == SQLITE_OK) rc = stmt.bind_int64(6, row.since);
                   if (rc == SQLITE_OK) rc = bind_optional(stmt, 7, row.until);
                   if (rc == SQLITE_OK) rc = stmt.bind_int64(8, row.since_event);
                   if (rc == SQLITE_OK) rc = stmt.bind_int64(9, row.until_event);
                   return rc;
               },
               "relation row");
    state_->history_keys.relation = std::move(key);
}

void StageWriter::commit() {
    if (state_ == nullptr) {
        fatal("StageWriter::commit: stage is not open");
    }
    detail::exec_or_fatal(state_->db.get(), "COMMIT", "COMMIT (stage " + name_ + ")");
    state_->stages.push_back(name_);
    state_->stage_open = false;
    state_ = nullptr;
}

// --- WorldWriter --------------------------------------------------------------

WorldWriter::WorldWriter() : state_(std::make_unique<detail::WriterState>()) {
    state_->db = open_working_database();
}

WorldWriter::~WorldWriter() = default;
WorldWriter::WorldWriter(WorldWriter&& other) noexcept = default;
WorldWriter& WorldWriter::operator=(WorldWriter&& other) noexcept = default;

void WorldWriter::set_seed(std::uint64_t value) {
    WriterState& s = detail::live_state(state_.get(), "set_seed");
    s.seed = value;
    s.seed_phrase.clear();
}

void WorldWriter::set_seed_phrase(std::string_view phrase) {
    WriterState& s = detail::live_state(state_.get(), "set_seed_phrase");
    if (phrase.empty() || phrase.size() > PHRASE_MAX_BYTES || scrub_phrase(phrase) != phrase) {
        fatal("WorldWriter::set_seed_phrase: phrase must be already scrubbed (a-z, 0-9 and single interior "
              "spaces), 1 to 256 bytes");
    }
    s.seed = phrase_seed(phrase);
    s.seed_phrase = std::string(phrase);
}

void WorldWriter::set_content_hash(const Sha256Digest& digest) {
    WriterState& s = detail::live_state(state_.get(), "set_content_hash");
    s.content_hash = to_hex(digest);
}

void WorldWriter::set_param_integer(std::string_view name, std::int64_t value) {
    WriterState& s = detail::live_state(state_.get(), "set_param_integer");
    require_param_name(name, "set_param_integer");
    if (std::string(META_PARAM_PREFIX) + std::string(name) == META_PARAM_MAP_SIZE &&
        std::find(ALLOWED_MAP_SIZES.begin(), ALLOWED_MAP_SIZES.end(), value) == ALLOWED_MAP_SIZES.end()) {
        fatal_text("WorldWriter::set_param_integer: map_size " + format_i64(value) + " is not 512, 1024 or 2048");
    }
    const std::string key = std::string(META_PARAM_PREFIX) + std::string(name);
    std::string text = format_i64(value);
    require_canonical_history_value(key, text, "set_param_integer");
    s.params[key] = std::move(text);
}

void WorldWriter::set_param_decimal(std::string_view name, std::string_view decimal_text) {
    WriterState& s = detail::live_state(state_.get(), "set_param_decimal");
    require_param_name(name, "set_param_decimal");
    if (std::string(META_PARAM_PREFIX) + std::string(name) == META_PARAM_MAP_SIZE) {
        fatal("WorldWriter::set_param_decimal: map_size is an integer parameter");
    }
    std::optional<std::string> canonical = canonical_decimal(decimal_text);
    if (!canonical) {
        fatal_text("WorldWriter::set_param_decimal: '" + std::string(decimal_text) + "' is not a valid decimal");
    }
    const std::string key = std::string(META_PARAM_PREFIX) + std::string(name);
    require_canonical_history_value(key, *canonical, "set_param_decimal");
    s.params[key] = std::move(*canonical);
}

void WorldWriter::set_param_text(std::string_view name, std::string_view value) {
    WriterState& s = detail::live_state(state_.get(), "set_param_text");
    require_param_name(name, "set_param_text");
    const std::string key = std::string(META_PARAM_PREFIX) + std::string(name);
    if (key == META_PARAM_MAP_SIZE) {
        fatal("WorldWriter::set_param_text: map_size is an integer parameter");
    }
    require_canonical_history_value(key, value, "set_param_text");
    s.params[key] = std::string(value);
}

std::uint64_t WorldWriter::seed() const noexcept {
    return state_ != nullptr && state_->seed ? *state_->seed : 0U;
}

StageWriter WorldWriter::begin_stage(std::string_view stage_name) {
    WriterState& s = detail::live_state(state_.get(), "begin_stage");
    if (s.stage_open) {
        fatal("WorldWriter::begin_stage: another stage is still open");
    }
    if (!is_valid_stage_name(stage_name)) {
        fatal_text("WorldWriter::begin_stage: '" + std::string(stage_name) + "' is not a valid stage name");
    }
    if (std::find(s.stages.begin(), s.stages.end(), stage_name) != s.stages.end()) {
        fatal_text("WorldWriter::begin_stage: stage '" + std::string(stage_name) + "' already ran");
    }
    detail::exec_or_fatal(s.db.get(), "BEGIN", "BEGIN (stage " + std::string(stage_name) + ")");
    s.stage_open = true;
    s.stage_start_layer_id = s.next_layer_id;
    s.stage_start_pass_id = s.next_pass_id;
    s.stage_start_biome_id = s.last_biome_id;
    s.stage_start_climate = s.climate_written;
    s.stage_start_region_id = s.last_region_id;
    s.stage_start_border_key = s.last_border_key;
    s.stage_start_region_biome_key = s.last_region_biome_key;
    s.stage_start_race_id = s.last_race_id;
    s.stage_start_race_biome_key = s.last_race_biome_key;
    s.stage_start_culture_id = s.last_culture_id;
    s.stage_start_god_id = s.last_god_id;
    s.stage_start_age_id = s.last_age_id;
    s.stage_start_history_keys = s.history_keys;
    return StageWriter(s, std::string(stage_name));
}

std::int64_t WorldWriter::next_layer_id() const noexcept {
    return state_ != nullptr ? state_->next_layer_id : 0;
}

const std::vector<std::string>& WorldWriter::stages() const noexcept {
    static const std::vector<std::string> none;
    return state_ != nullptr ? state_->stages : none;
}

Status WorldWriter::check_output(std::string_view out_utf8, Overwrite overwrite) {
    if (!is_valid_utf8_path(out_utf8)) {
        return output_error(ErrorCode::output_write_failed,
                            "output path is empty, not valid UTF-8, or contains a NUL byte");
    }
    const std::string shown(out_utf8);
    const std::filesystem::path out = path_from_utf8(out_utf8);
    std::error_code ec;
    // symlink_status: anything at the path, a dangling symbolic link
    // included, counts as existing; a link is never followed.
    const std::filesystem::file_status status = std::filesystem::symlink_status(out, ec);
    if (std::filesystem::exists(status)) {
        if (std::filesystem::is_directory(status)) {
            return output_error(ErrorCode::output_exists, "output '" + shown + "' is a directory");
        }
        if (overwrite == Overwrite::refuse) {
            return output_error(ErrorCode::output_exists,
                                "output '" + shown + "' already exists; allow overwriting to replace it");
        }
    }
    const std::filesystem::path parent = out.parent_path();
    if (!parent.empty() && !std::filesystem::is_directory(parent, ec)) {
        return output_error(ErrorCode::output_parent_missing,
                            "directory '" + path_to_utf8(parent) + "' for output '" + shown + "' does not exist");
    }
    return Ok{};
}

Status WorldWriter::finalize(std::string_view out_utf8, Overwrite overwrite) {
    WriterState& s = detail::live_state(state_.get(), "finalize");
    if (s.stage_open) {
        fatal("WorldWriter::finalize: a stage is still open");
    }
    if (!s.seed) {
        fatal("WorldWriter::finalize: no seed set");
    }
    if (!s.content_hash) {
        fatal("WorldWriter::finalize: no content hash set");
    }
    // Every required param.* key (P3-03 4.1): the first missing one in
    // ascending key order is named.
    for (const std::string_view key : REQUIRED_META_KEYS) {
        if (key.substr(0, META_PARAM_PREFIX.size()) == META_PARAM_PREFIX &&
            s.params.find(std::string(key)) == s.params.end()) {
            fatal_text("WorldWriter::finalize: meta key '" + std::string(key) + "' was not set");
        }
    }
    s.finalized = true;

    if (!is_valid_utf8_path(out_utf8)) {
        // Nothing can have been written under a path that cannot be named.
        s.db.close();
        return output_error(ErrorCode::output_write_failed,
                            "output path is empty, not valid UTF-8, or contains a NUL byte");
    }
    const std::string partial_utf8 = std::string(out_utf8) + std::string(PARTIAL_SUFFIX);
    const std::string journal_utf8 = partial_utf8 + "-journal";
    const std::filesystem::path partial = path_from_utf8(partial_utf8);
    const std::filesystem::path journal = path_from_utf8(journal_utf8);
    const std::filesystem::path out = path_from_utf8(out_utf8);
    FinalizeCleanup cleanup(s, partial, journal);  // from here on, every exit cleans up unless dismissed

    if (Status pre = check_output(out_utf8, overwrite); !pre.ok()) {
        // Nothing was written; the cleanup closes the working database and
        // removes a stale <out>.partial (P0-02 6, "On any failure").
        return pre;
    }

    // Step 4.
    const std::map<std::string, std::string> meta = build_meta(s);
    write_meta(s, meta);
    if (auto e = forced_failure(s, testing::FailurePoint::after_meta)) return *e;

    // Step 5.
    integrity_check(s.db.get());
    if (auto e = forced_failure(s, testing::FailurePoint::after_integrity_check)) return *e;

    // Step 6: remove a stale <out>.partial and its journal, then vacuum.
    if (auto e = remove_stale(partial, partial_utf8)) return *e;
    if (auto e = remove_stale(journal, journal_utf8)) return *e;
    {
        Statement vacuum;
        int rc = vacuum.prepare(s.db.get(), "VACUUM INTO ?1");
        if (rc == SQLITE_OK) rc = vacuum.bind_text(1, partial_utf8);
        if (rc == SQLITE_OK) rc = vacuum.step() == SQLITE_DONE ? SQLITE_OK : SQLITE_ERROR;
        if (rc != SQLITE_OK) {
            return output_error(ErrorCode::output_write_failed,
                                "cannot write '" + partial_utf8 + "': " + detail::error_message(s.db.get()));
        }
    }
    if (auto e = forced_failure(s, testing::FailurePoint::after_vacuum)) return *e;

    // Step 7.
    if (s.db.close() != SQLITE_OK) {
        fatal("world database: closing the working database failed");
    }
    if (auto e = forced_failure(s, testing::FailurePoint::after_close)) return *e;

    // Step 8.
    verify_partial(partial, partial_utf8, meta);
    if (auto e = forced_failure(s, testing::FailurePoint::after_verify)) return *e;

    // Step 9.
    if (auto e = rename_into_place(s, partial, partial_utf8, out, std::string(out_utf8), overwrite)) return *e;
    cleanup.dismiss();
    return Ok{};
}

}  // namespace worldgen::db
