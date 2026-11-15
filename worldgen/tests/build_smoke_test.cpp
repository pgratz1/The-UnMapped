// Build scaffold check: proves that the test runner, the worldgen library
// and the vendored SQLite link together. Replace or extend freely.

#include <doctest/doctest.h>
#include <sqlite3.h>

#include "worldgen/version.hpp"

TEST_CASE("worldgen reports a generator version") {
    CHECK_FALSE(worldgen::generator_version().empty());
}

TEST_CASE("vendored SQLite is the pinned version and opens an in-memory database") {
    // The library linked must match the header compiled against.
    CHECK(sqlite3_libversion_number() == SQLITE_VERSION_NUMBER);
    CHECK(SQLITE_VERSION_NUMBER == 3054001);

    sqlite3* db = nullptr;
    REQUIRE(sqlite3_open(":memory:", &db) == SQLITE_OK);
    CHECK(sqlite3_exec(db, "CREATE TABLE t(x INTEGER); INSERT INTO t VALUES (1);", nullptr, nullptr, nullptr) ==
          SQLITE_OK);
    CHECK(sqlite3_close(db) == SQLITE_OK);
}
