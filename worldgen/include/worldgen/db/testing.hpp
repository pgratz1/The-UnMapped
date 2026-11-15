#pragma once

// TEST SUPPORT ONLY. Hooks into the world database writer that the tests of
// spec P0-02 section 12 need. Never call these from generator code or the
// CLI: they bypass the writer's ordering rules or make it fail on purpose.

#include <cstdint>
#include <span>
#include <string_view>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/result.hpp"
#include "worldgen/db/writer.hpp"

namespace worldgen::db::testing {

// A point inside WorldWriter::finalize() after which it fails on purpose
// (P0-02 12.2 "a run forced to fail after step 6"). Step numbers are those
// of P0-02 section 6.
enum class FailurePoint : std::uint8_t {
    none = 0,               // normal operation
    after_meta,             // after step 4 (meta written)
    after_integrity_check,  // after step 5
    after_vacuum,           // after step 6: <out>.partial exists, working database still open
    after_close,            // after step 7: working database closed
    after_verify,           // after step 8: <out>.partial verified, not yet renamed
};

// How finalize() fails at the failure point.
enum class FailureMode : std::uint8_t {
    throw_fatal,   // raises FatalError("test hook: forced failure ...")
    return_error,  // returns an output_write_failed error
};

// Order in which finalize() inserts the meta rows (P0-02 12.3 layout
// independence). The file must not depend on it.
enum class MetaOrder : std::uint8_t {
    ascending = 0,  // the normal order of P0-02 6 step 4
    descending,     // reverse key order
};

// Simulated filesystem faults in step 9 of WorldWriter::finalize (the
// rename into place). A simulated call does not touch the filesystem and
// reports an error, exactly as a failing real call would.
struct FsFaults {
    // The existence check of <out> made just before the rename (refuse
    // mode) reports an error, e.g. permission denied.
    bool output_status_error = false;
    // The first failing_renames rename attempts fail. While this is
    // non-zero, the replace fallback (remove <out>, rename again), which is
    // otherwise used on Windows only, is used on every platform so that its
    // paths can be tested anywhere.
    std::uint32_t failing_renames = 0;
    // The fallback's removal of the existing <out> fails.
    bool remove_output_error = false;
};

struct WriterAccess {
    // Installs simulated faults for the rename step (P0-02 6 step 9). In
    // every case finalize() must either return Ok with the new file at <out>
    // or return an error with <out>.partial removed.
    static void set_fs_faults(WorldWriter& writer, const FsFaults& faults);

    // Makes finalize() fail at point in the given mode. In both modes the
    // writer's cleanup runs: no <out>.partial remains.
    static void set_failure_point(WorldWriter& writer, FailurePoint point,
                                  FailureMode mode = FailureMode::throw_fatal);

    // Sets the order of the meta inserts in finalize().
    static void set_meta_order(WorldWriter& writer, MetaOrder order);

    // Runs sql (one or more statements) on the working database, inside
    // whatever transaction is open. Never throws: an SQLite failure is a
    // sql_rejected error carrying SQLite's message. Used for "writing a
    // REAL fails (STRICT)" (12.2) and "inserts extra rows and deletes them"
    // (12.3). FatalError only if the writer is already finalized.
    static Status execute(WorldWriter& writer, std::string_view sql);

    // Like StageWriter::write_layer, but accepts any id (P0-02 12.3 "inserts
    // in reverse id order"). Still validates name and grid. Afterwards
    // next_layer_id() is 1 + the largest id written so far.
    static void write_layer_any_id(StageWriter& stage, std::int64_t id, std::string_view name,
                                   const LayerGrid& grid, Codec codec = Codec::delta_varint);

    // Inserts a raw layer row with no validation at all, e.g. a blob whose
    // header disagrees with the columns (P0-02 12.2 "rejected by the reader
    // as corrupt"). Does not change next_layer_id(). FatalError if SQLite
    // rejects the row (STRICT types, NOT NULL, UNIQUE).
    static void insert_layer_row(StageWriter& stage, std::int64_t id, std::string_view name,
                                 std::int64_t width, std::int64_t height, std::string_view format,
                                 std::span<const std::uint8_t> data);
};

// Opens an existing database file read-write (never creating one), runs sql
// (one or more statements) and closes it. For building reader-error inputs
// from a finished world (P0-02 12.2): a different application_id,
// user_version 2, a missing seed key. Never throws: failures are
// sql_rejected or file_unreadable errors.
[[nodiscard]] Status execute_on_file(std::string_view path_utf8, std::string_view sql);

}  // namespace worldgen::db::testing
