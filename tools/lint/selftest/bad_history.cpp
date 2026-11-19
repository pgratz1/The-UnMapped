// lint-selftest-path: worldgen/src/history/selftest_history.cpp
// Self-test snippet: scanned as history library code (P3-00 H4). Every
// random decision of the history is a keyed draw; the sequential generator
// is banned here, whatever it is used for. Not compiled.
#include "worldgen/core/rng.hpp"  // expect: history-sequential-rng
#include "worldgen/history/draw.hpp"

namespace worldgen::history {

std::uint64_t bad_draws(std::uint64_t key, std::uint64_t x) {
    Rng rng(key);  // expect: history-sequential-rng
    auto other = worldgen::Rng::from_state(1, 2, 3, 4);  // expect: history-sequential-rng
    const std::uint64_t a = rng.next();
    const std::uint64_t b = splitmix64_next(x);  // expect: history-sequential-rng
    return a ^ b ^ other.next();
}

// Allowed: keyed draws, and names that merely contain the letters.
std::uint64_t good_draws(std::uint64_t key, std::uint64_t year, std::uint64_t id) {
    const std::uint64_t u = draw(key, year, id, 0, 0);  // Rng is mentioned only in this comment
    const char* text = "Rng rng(key); splitmix64_next(x)";
    std::uint64_t RngCount = 0;
    std::uint64_t my_splitmix64_next = 0;
    return u + pick(u, 6) + RngCount + my_splitmix64_next + (text != nullptr ? 1U : 0U);
}

}  // namespace worldgen::history
