#pragma once

// Shared helpers for the P0-01 deterministic-core tests.
//
// Written from docs/specs/p0-01-deterministic-core.md and the public headers
// only. No floating point: statistics use integer formulations.

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <limits>

#include "worldgen/core/core.hpp"

namespace worldgen_test {

inline constexpr std::int64_t I64_MIN = std::numeric_limits<std::int64_t>::min();
inline constexpr std::int64_t I64_MAX = std::numeric_limits<std::int64_t>::max();
inline constexpr std::uint64_t U64_MAX = std::numeric_limits<std::uint64_t>::max();
inline constexpr std::int64_t ONE_RAW = std::int64_t{4294967296};
inline constexpr std::int64_t HALF_RAW = std::int64_t{2147483648};

// Fixed keys for property tests. These seed Rng directly with a constant
// (not a world seed and not a stage stream), so no stream name is needed.
inline constexpr std::uint64_t KEY_FX_PROPS = 0x5445535446585031ULL;     // "1PXFTSET"
inline constexpr std::uint64_t KEY_DECIMAL_PROPS = 0x5445535444454331ULL;
inline constexpr std::uint64_t KEY_SQRT_PROPS = 0x5445535453515231ULL;
inline constexpr std::uint64_t KEY_RNG_PROPS = 0x544553545247E731ULL;
inline constexpr std::uint64_t KEY_HASH_PROPS = 0x5445535448534831ULL;
inline constexpr std::uint64_t KEY_NOISE_PROPS = 0x544553544E4F4931ULL;

[[nodiscard]] inline worldgen::Fx raw(std::int64_t r) { return worldgen::Fx::from_raw(r); }
[[nodiscard]] inline worldgen::Fx fxi(std::int64_t n) { return worldgen::Fx::from_int(n); }

// Advance a generator k times, discarding the outputs.
inline void advance(worldgen::Rng& rng, std::uint64_t k) {
    for (std::uint64_t i = 0; i < k; ++i) {
        const std::uint64_t discarded = rng.next();
        static_cast<void>(discarded);
    }
}

// True if `used` is exactly `draws` next() calls ahead of a generator made
// with from_state(s).
[[nodiscard]] inline bool consumed_exactly(const worldgen::Rng& used, const std::array<std::uint64_t, 4>& s,
                                           std::uint64_t draws) {
    worldgen::Rng twin = worldgen::Rng::from_state(s[0], s[1], s[2], s[3]);
    advance(twin, draws);
    return twin.state() == used.state();
}

}  // namespace worldgen_test

// The call raises worldgen::FatalError (spec 4.1).
#define CHECK_FATAL(...) CHECK_THROWS_AS(static_cast<void>(__VA_ARGS__), worldgen::FatalError)
