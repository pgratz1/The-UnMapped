// lint-selftest-path: worldgen/src/geo/selftest_lib.cpp
// Self-test snippet: scanned as library code outside the database and
// inspection directories. The library reads no environment variable, and
// threads are confined to parallel.cpp; SQL text is not read here. Not
// compiled.
#include <thread>  // expect: thread

constexpr const char* not_sql_here = "CREATE TABLE t (id INTEGER PRIMARY KEY AUTOINCREMENT, x REAL)";

const char* data_dir() {
    return std::getenv("UNMAPPED_DATA_DIR");  // expect: environment
}

void stage(std::uint32_t threads) {
    run_blocks(8, threads, [](std::uint32_t b) { work(b); });
    std::atomic<int> counter{0};  // expect: shared-state
}
