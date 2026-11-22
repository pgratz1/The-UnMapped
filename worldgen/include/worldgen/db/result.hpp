#pragma once

// Error values of the world database layer (spec P0-02 section 9, "Errors").
//
// The read API and the file-writing step of the writer return errors as
// values, never as exceptions, so that the GDExtension can bind them
// without exception translation. Contract violations by the generator itself
// (a bug: wrong layer id, invalid layer name, SQLite rejecting a statement
// the writer built) still raise worldgen::FatalError (P0-01 4.1).

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "worldgen/core/error.hpp"

namespace worldgen::db {

// What went wrong. Each reader condition of P0-02 section 9 has its own
// code. The open checks are listed in the order the reader applies them.
enum class ErrorCode : std::uint8_t {
    // --- WorldReader::open, in validation order (P0-02 9, "Open") ---------
    file_unreadable = 1,         // invalid path (is_valid_utf8_path), file missing,
                                 // not a regular file, or unreadable
    not_sqlite,                  // shorter than 100 bytes or no "SQLite format 3\0" magic
    not_world,                   // application_id != APPLICATION_ID, the file is in WAL mode,
                                 // meta table missing, meta.format missing or wrong, or the
                                 // SQLite magic followed by garbage (SQLITE_NOTADB) (14.2)
    unsupported_schema_version,  // user_version or meta.schema_version != SCHEMA_VERSION
    meta_key_missing,            // a key of REQUIRED_META_KEYS is absent
    meta_value_invalid,          // a required key's value is not in its canonical form (5.5);
                                 // not listed in the spec, see the implementation report
    // --- WorldReader layer access (P0-02 9, "Layer list", "Layer read") ----
    layer_not_found,  // no layer with that name
    corrupt_layer,    // blob header or payload invalid (7), or header disagrees with the row
    // --- WorldWriter::finalize file output (P0-02 6; P0-04 4.4, exit 4) -----
    output_exists,          // <out> is a directory, or exists and overwrite was not allowed
    output_parent_missing,  // the parent directory of <out> does not exist
    output_write_failed,    // invalid output path (is_valid_utf8_path); an old <out>.partial
                            // or <out>.partial-journal cannot be removed; VACUUM INTO failed;
                            // the existence check of <out> before the rename failed (refuse);
                            // or a testing::FailureMode::return_error forced failure
    output_rename_failed,   // renaming <out>.partial to <out> did not succeed, including the
                            // replace fallback (see WorldWriter::finalize)
    // --- Any operation ----------------------------------------------------
    sqlite_error,   // an unexpected SQLite failure while reading (I/O error, corruption)
    out_of_memory,  // an allocation failed while reading
    sql_rejected,   // testing::execute only: SQLite rejected the statement
    // --- WorldReader raw row access (P0-02 14.10; P0-04 sections 7, 8) ------
    table_not_found,  // no table of that name in sqlite_schema
    no_primary_key,   // the table has no primary key, so it has no defined row order (P0-02 5.3)
    real_value,       // a REAL value was found (P0-02 5.3 forbids them; P0-04 8, exit 3)
    // --- The history report (P3-07 5.2; P0-02 amendment 20) ----------------
    history_missing,  // the world has no history (meta.stages does not name history)
};

// Short stable name of a code, e.g. "not_world" (for logs and tests).
[[nodiscard]] const char* error_code_name(ErrorCode code) noexcept;

// An error value: a code and a plain-English message saying what was
// expected (P0-02 9, "Errors"). Messages name the file or layer involved.
struct Error {
    ErrorCode code = ErrorCode::sqlite_error;
    std::string message;
};

// Either a value of type T or an Error (P0-02 9, "Errors"). Accessing
// value() of an error result, or error() of a success, is a contract
// violation and raises FatalError.
template <class T>
class [[nodiscard]] Result {
public:
    Result(T value) : data_(std::in_place_index<0>, std::move(value)) {}      // NOLINT: implicit by design
    Result(Error error) : data_(std::in_place_index<1>, std::move(error)) {}  // NOLINT: implicit by design

    [[nodiscard]] bool ok() const noexcept { return data_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    [[nodiscard]] T& value() & {
        require_ok();
        return std::get<0>(data_);
    }
    [[nodiscard]] const T& value() const& {
        require_ok();
        return std::get<0>(data_);
    }
    [[nodiscard]] T&& value() && {
        require_ok();
        return std::get<0>(std::move(data_));
    }
    [[nodiscard]] const Error& error() const {
        if (ok()) {
            fatal("worldgen::db::Result::error() called on a success");
        }
        return std::get<1>(data_);
    }
    // Shorthand for error().code.
    [[nodiscard]] ErrorCode code() const { return error().code; }

private:
    void require_ok() const {
        if (!ok()) {
            fatal("worldgen::db::Result::value() called on an error");
        }
    }

    std::variant<T, Error> data_;
};

// Success without a value.
struct Ok {};

// Result of an operation that returns nothing on success.
using Status = Result<Ok>;

}  // namespace worldgen::db
