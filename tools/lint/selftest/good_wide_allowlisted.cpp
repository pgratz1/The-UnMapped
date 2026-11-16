// lint-selftest-path: worldgen/src/core/wide.cpp
// Self-test snippet: scanned as if it were the allow-listed file
// worldgen/src/core/wide.cpp (spec P0-01 5.3 fast path), so the 128-bit
// types below are not reported. Other rules still apply there. Not compiled.
#include <cstdint>

std::uint64_t fast_hi(std::uint64_t a, std::uint64_t b) {
    const unsigned __int128 p = static_cast<unsigned __int128>(a) * b;
    return static_cast<std::uint64_t>(p >> 64);
}

double still_banned = 0;  // expect: float-type
