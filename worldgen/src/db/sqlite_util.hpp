#pragma once

// Private SQLite helpers for the world database layer. Not part of the
// public API; sqlite3.h never appears in a public header.

#include <sqlite3.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace worldgen::db::detail {

// Owns a connection; closes it on destruction.
class Connection {
public:
    Connection() noexcept = default;
    explicit Connection(sqlite3* db) noexcept : db_(db) {}
    Connection(Connection&& other) noexcept : db_(other.db_) { other.db_ = nullptr; }
    Connection& operator=(Connection&& other) noexcept;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    ~Connection() { close(); }

    [[nodiscard]] sqlite3* get() const noexcept { return db_; }
    [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }
    // Closes the connection; returns the sqlite3_close result (SQLITE_OK if
    // it was not open).
    int close() noexcept;

private:
    sqlite3* db_ = nullptr;
};

// Owns a prepared statement; finalizes it on destruction.
class Statement {
public:
    Statement() noexcept = default;
    Statement(Statement&& other) noexcept : stmt_(other.stmt_) { other.stmt_ = nullptr; }
    Statement& operator=(Statement&&) = delete;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    ~Statement() { sqlite3_finalize(stmt_); }

    // Prepares one statement; returns the SQLite result code.
    int prepare(sqlite3* db, std::string_view sql) noexcept;

    [[nodiscard]] sqlite3_stmt* get() const noexcept { return stmt_; }

    int bind_int64(int index, std::int64_t value) noexcept;
    // Text as UTF-8 with an explicit byte length (P0-02 5.3); copied by SQLite.
    int bind_text(int index, std::string_view text) noexcept;
    // Blob with an explicit byte length (P0-02 5.3); copied by SQLite.
    int bind_blob(int index, std::span<const std::uint8_t> bytes) noexcept;

    // sqlite3_step result.
    int step() noexcept { return sqlite3_step(stmt_); }

    [[nodiscard]] int column_type(int column) const noexcept { return sqlite3_column_type(stmt_, column); }
    [[nodiscard]] std::int64_t column_int64(int column) const noexcept {
        return static_cast<std::int64_t>(sqlite3_column_int64(stmt_, column));
    }
    // TEXT column as bytes (empty for NULL).
    [[nodiscard]] std::string column_text(int column) const;
    // BLOB column as a view valid until the next step/finalize.
    [[nodiscard]] std::span<const std::uint8_t> column_blob(int column) const noexcept;

private:
    sqlite3_stmt* stmt_ = nullptr;
};

// sqlite3_errmsg of db as a std::string ("out of memory" for a null db).
[[nodiscard]] std::string error_message(sqlite3* db);

// Runs sql (one or more statements, no parameters) and returns the SQLite
// result code; on failure message receives SQLite's message.
int exec(sqlite3* db, std::string_view sql, std::string& message);

// Runs sql; FatalError naming `what` and SQLite's message on failure.
void exec_or_fatal(sqlite3* db, std::string_view sql, std::string_view what);

}  // namespace worldgen::db::detail
