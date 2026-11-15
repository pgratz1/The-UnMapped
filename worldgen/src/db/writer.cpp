// World database writer (spec P0-02 sections 4.2, 5, 6).

#include "worldgen/db/writer.hpp"

#include <algorithm>
#include <filesystem>
#include <system_error>
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
        const std::filesystem::file_status status = std::filesystem::status(out, status_ec);
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
    const PhraseSeed r = seed_from_phrase(phrase);
    if (!r.ok() || r.phrase != phrase) {
        fatal("WorldWriter::set_seed_phrase: phrase must be valid UTF-8, already trimmed, 1 to 256 bytes");
    }
    s.seed = r.seed;
    s.seed_phrase = r.phrase;
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
    s.params[std::string(META_PARAM_PREFIX) + std::string(name)] = format_i64(value);
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
    s.params[std::string(META_PARAM_PREFIX) + std::string(name)] = std::move(*canonical);
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
    const std::filesystem::file_status status = std::filesystem::status(out, ec);
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
    if (s.params.find(std::string(META_PARAM_MAP_SIZE)) == s.params.end()) {
        fatal("WorldWriter::finalize: param.map_size not set");
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
