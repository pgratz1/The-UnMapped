// lint-selftest-path: tools/worldgen-cli/selftest_cli.cpp
// Self-test snippet: scanned as command-line code. The CLI may read the
// environment (it passes values to the library), and SQL text is not read
// outside the database and inspection code. The thread rules, number
// formatting and discarded writes still apply. Not compiled.
#include <cstdlib>
#include <thread>  // determinism-lint: allow(thread) -- only for std::thread::hardware_concurrency, the --threads default

const char* data_dir() {
    return std::getenv("UNMAPPED_DATA_DIR");
}

std::uint32_t default_threads() {
    // determinism-lint: allow(hardware-concurrency) -- only the --threads default; the count never reaches a result
    const unsigned int hw = std::thread::hardware_concurrency();
    return hw == 0U ? 1U : hw;
}

constexpr const char* sql = "SELECT a FROM t";  // not in a SQL directory

void out(std::string_view t) {
    std::fwrite(t.data(), 1, t.size(), stdout);  // expect: unchecked-write
    std::thread worker([] {});  // expect: thread
    auto s = std::to_string(1);  // expect: number-text
}
