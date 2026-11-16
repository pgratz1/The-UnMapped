// lint-selftest-path: worldgen/src/core/parallel.cpp
// Self-test snippet: scanned as if it were the one parallel facility, so
// threads and atomics are allowed. Thread identity, thread-local state, the
// machine's thread count and every other rule still apply. Not compiled.
#include <atomic>
#include <thread>
#include <vector>

void run(std::uint32_t workers) {
    std::atomic<std::uint64_t> next{0};
    std::vector<std::thread> pool(workers);
    for (std::thread& slot : pool) {
        slot = std::thread([&next] { next.fetch_add(1U); });
    }
    thread_local int scratch = 0;  // expect: thread-identity
    auto id = std::this_thread::get_id();  // expect: thread-identity
    auto n = std::thread::hardware_concurrency();  // expect: hardware-concurrency
    auto s = std::reduce(v.begin(), v.end());  // expect: unordered-reduction
    double d = 0;  // expect: float-type
}
