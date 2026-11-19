#include "db/sqlite_util.hpp"

#include <limits>

#include "worldgen/core/error.hpp"

namespace worldgen::db::detail {

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

int Connection::close() noexcept {
    if (db_ == nullptr) {
        return SQLITE_OK;
    }
    // sqlite3_close (not _v2): every statement is finalized by RAII before
    // its connection closes, so SQLITE_BUSY here would be a bug.
    const int rc = sqlite3_close(db_);
    db_ = nullptr;
    return rc;
}

int Statement::prepare(sqlite3* db, std::string_view sql) noexcept {
    sqlite3_finalize(stmt_);
    stmt_ = nullptr;
    if (sql.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return SQLITE_TOOBIG;
    }
    return sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr);
}

int Statement::bind_int64(int index, std::int64_t value) noexcept {
    return sqlite3_bind_int64(stmt_, index, static_cast<sqlite3_int64>(value));
}

int Statement::bind_null(int index) noexcept {
    return sqlite3_bind_null(stmt_, index);
}

int Statement::bind_text(int index, std::string_view text) noexcept {
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return SQLITE_TOOBIG;
    }
    return sqlite3_bind_text(stmt_, index, text.data(), static_cast<int>(text.size()), SQLITE_TRANSIENT);
}

int Statement::bind_blob(int index, std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return SQLITE_TOOBIG;
    }
    // A zero-length blob still needs a non-null pointer, or SQLite binds NULL.
    static constexpr std::uint8_t empty = 0;
    const void* data = bytes.empty() ? static_cast<const void*>(&empty) : static_cast<const void*>(bytes.data());
    return sqlite3_bind_blob(stmt_, index, data, static_cast<int>(bytes.size()), SQLITE_TRANSIENT);
}

std::string Statement::column_text(int column) const {
    const unsigned char* text = sqlite3_column_text(stmt_, column);
    const int size = sqlite3_column_bytes(stmt_, column);
    if (text == nullptr || size <= 0) {
        return std::string();
    }
    return std::string(reinterpret_cast<const char*>(text), static_cast<std::size_t>(size));
}

std::span<const std::uint8_t> Statement::column_blob(int column) const noexcept {
    const void* data = sqlite3_column_blob(stmt_, column);
    const int size = sqlite3_column_bytes(stmt_, column);
    if (data == nullptr || size <= 0) {
        return {};
    }
    return {static_cast<const std::uint8_t*>(data), static_cast<std::size_t>(size)};
}

std::string error_message(sqlite3* db) {
    if (db == nullptr) {
        return "out of memory";
    }
    return std::string(sqlite3_errmsg(db));
}

int exec(sqlite3* db, std::string_view sql, std::string& message) {
    const std::string text(sql);  // sqlite3_exec needs a NUL-terminated string
    char* raw = nullptr;
    const int rc = sqlite3_exec(db, text.c_str(), nullptr, nullptr, &raw);
    if (rc != SQLITE_OK) {
        message = raw != nullptr ? std::string(raw) : std::string(sqlite3_errstr(rc));
    }
    sqlite3_free(raw);
    return rc;
}

void exec_or_fatal(sqlite3* db, std::string_view sql, std::string_view what) {
    std::string message;
    if (exec(db, sql, message) != SQLITE_OK) {
        std::string text = "world database: ";
        text.append(what);
        text.append(" failed: ");
        text.append(message);
        throw FatalError(text);
    }
}

}  // namespace worldgen::db::detail
