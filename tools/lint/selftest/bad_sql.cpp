// lint-selftest-path: worldgen/src/db/selftest_sql.cpp
// Self-test snippet: SQL text in string literals, scanned as a file of the
// database layer. Adjacent literals are joined; a finding is reported on
// the line of the first literal of the group. Not compiled.
#include <string_view>

constexpr std::string_view a = "CREATE TABLE t (id INTEGER PRIMARY KEY AUTOINCREMENT)";  // expect: sql-autoincrement
constexpr std::string_view b = "ANALYZE";  // expect: sql-maintenance
constexpr std::string_view c = "PRAGMA optimize";  // expect: sql-maintenance
constexpr std::string_view d = "VACUUM";  // expect: sql-maintenance
constexpr std::string_view e = "PRAGMA journal_mode = WAL";  // expect: sql-setting
constexpr std::string_view f = "PRAGMA journal_mode=wal";  // expect: sql-setting
constexpr std::string_view g = "PRAGMA auto_vacuum = FULL";  // expect: sql-setting
constexpr std::string_view h = "CREATE TABLE t (x REAL NOT NULL) STRICT";  // expect: sql-real
constexpr std::string_view i = "SELECT CAST(v AS DOUBLE) FROM t ORDER BY k";  // expect: sql-real
constexpr std::string_view j = "SELECT key, value FROM meta";  // expect: sql-unordered-select
constexpr std::string_view k = "SELECT name FROM layer "  // expect: sql-unordered-select
                               "WHERE width > 1";
const char* l = R"sql(SELECT a FROM t)sql";  // expect: sql-unordered-select
const char* l2 = R"sql(SELECT a  -- raw strings are read too  // expect: sql-unordered-select
    FROM t)sql";
constexpr std::string_view m = " FROM t ORDER BY k AUTOINCREMENT";  // expect: sql-autoincrement

// determinism-lint: allow(sql-unordered-select) -- count(*) with no GROUP BY returns exactly one row
constexpr std::string_view n = "SELECT count(*) FROM layer";

// Near misses: not reported.
constexpr std::string_view ok1 = "SELECT key, value FROM meta ORDER BY key";
constexpr std::string_view ok2 = "SELECT name FROM sqlite_schema WHERE type = 'table' "
                                 "ORDER BY name COLLATE BINARY";
constexpr std::string_view ok3 = "VACUUM INTO ?1";
constexpr std::string_view ok4 = "VACUUM main INTO ?1";
constexpr std::string_view ok5 = "PRAGMA journal_mode = MEMORY";
constexpr std::string_view ok6 = "PRAGMA auto_vacuum = NONE";
constexpr std::string_view ok7 = "PRAGMA auto_vacuum";
constexpr std::string_view ok8 = "SELECT typeof(v) = 'real' FROM t ORDER BY k";  // inside an SQL string
constexpr std::string_view ok9 = "CREATE TABLE \"real\" (id INTEGER PRIMARY KEY) STRICT";  // quoted identifier
constexpr std::string_view msg1 = "' holds a REAL value in ";  // a message, not SQL
constexpr std::string_view msg2 = "analyze the world file and select a layer";  // lower-case prose
constexpr std::string_view msg3 = "select a layer: vacuum, analyze";  // SQL is recognised by upper-case keywords
// "SELECT x FROM t" and "ANALYZE" in a comment are ignored.
