#pragma once

// SQLITE RUNTIME SETTINGS: the single place for the runtime configuration
// of the working database (spec P0-02 section 4.2). The compile-time options
// live in the block marked SQLITE CONFIGURATION in extern/CMakeLists.txt.
//
// Issued in this order immediately after sqlite3_open_v2(":memory:", ...)
// and before any table is created. Never run ANALYZE, PRAGMA optimize or
// VACUUM on the working database, and never enable WAL.

#include <array>
#include <string_view>

#include "worldgen/db/schema.hpp"

namespace worldgen::db::detail {

// The filename of the in-memory working database (P0-02 4.2).
inline constexpr const char* WORKING_DB_NAME = ":memory:";

// Every pragma of P0-02 4.2 except the last, in order. The last,
// PRAGMA user_version = <SCHEMA_VERSION>, is built from SCHEMA_VERSION.
inline constexpr std::array<std::string_view, 7> WORKING_DB_PRAGMAS = {
    "PRAGMA page_size = 4096",
    "PRAGMA encoding = 'UTF-8'",
    "PRAGMA auto_vacuum = NONE",
    "PRAGMA journal_mode = MEMORY",
    "PRAGMA temp_store = MEMORY",
    "PRAGMA foreign_keys = OFF",
    "PRAGMA application_id = 1431194960",
};

// Prefix of the final pragma; SCHEMA_VERSION in decimal follows.
inline constexpr std::string_view USER_VERSION_PRAGMA_PREFIX = "PRAGMA user_version = ";

static_assert(APPLICATION_ID == 1431194960U, "PRAGMA application_id text must match APPLICATION_ID");
static_assert(PAGE_SIZE == 4096U, "PRAGMA page_size text must match PAGE_SIZE");

}  // namespace worldgen::db::detail
