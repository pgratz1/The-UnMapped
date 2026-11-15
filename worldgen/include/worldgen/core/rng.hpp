#pragma once

// The project random generator: xoshiro256** seeded by SplitMix64, and every
// sampling function (spec P0-01 section 6).
//
// Rule (section 4): never make two draws on the same Rng within one full
// expression; each draw is its own statement.

#include <array>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>

#include "worldgen/core/fx.hpp"

namespace worldgen {

// One SplitMix64 step (P0-01 6.2): x += 0x9E3779B97F4A7C15, then the mix64
// finalizer of the new x is returned. x is updated in place.
[[nodiscard]] std::uint64_t splitmix64_next(std::uint64_t& x) noexcept;

// xoshiro256** generator (P0-01 6.1). Movable, not copyable (6.2): two
// generators with the same stream are made by constructing both from the
// same key. Moving transfers the stream: the source's four state words are
// set to zero, and next() (so every sampling function) is fatal on an
// all-zero state. A moved-from Rng may be assigned a new generator and used
// again.
class Rng {
public:
    // Seeds from SplitMix64 started at x = key; its first four outputs are
    // s0, s1, s2, s3 (P0-01 6.2). Keys come from section 7 (stream_key,
    // substream, ...), never the raw world seed.
    explicit Rng(std::uint64_t key) noexcept;

    // Sets the state directly (P0-01 6.2). Fatal if all four words are zero.
    [[nodiscard]] static Rng from_state(std::uint64_t s0, std::uint64_t s1, std::uint64_t s2,
                                        std::uint64_t s3);

    Rng(const Rng&) = delete;
    Rng& operator=(const Rng&) = delete;
    // Takes other's state and sets other's state to all zero (P0-01 6.2).
    Rng(Rng&& other) noexcept;
    // Takes other's state and sets other's state to all zero (P0-01 6.2).
    // Self-move-assignment leaves the object unchanged.
    Rng& operator=(Rng&& other) noexcept;
    ~Rng() = default;

    // The current state {s0, s1, s2, s3} (for tests and vectors).
    [[nodiscard]] std::array<std::uint64_t, 4> state() const noexcept;

    // xoshiro256** step (P0-01 6.1). 1 draw. Fatal if the state is all zero
    // (a moved-from Rng, 6.2).
    [[nodiscard]] std::uint64_t next();

    // next() >> 32 (P0-01 6.3). 1 draw.
    [[nodiscard]] std::uint32_t next_u32();

    // Unbiased integer in [0, s), Lemire's method (P0-01 6.3). At least one
    // draw, also for s = 1. Fatal if s = 0.
    [[nodiscard]] std::uint64_t uniform_u64(std::uint64_t s);

    // Integer in [lo, hi] (P0-01 6.3). For the full int64 range exactly one
    // draw, int64(next()). Fatal if lo > hi.
    [[nodiscard]] std::int64_t uniform_i64(std::int64_t lo, std::int64_t hi);

    // Fx in [0, ONE): raw = next() >> 32 (P0-01 6.3). 1 draw.
    [[nodiscard]] Fx uniform_fx01();

    // Fx in [lo, hi): raw = lo.raw + uniform_u64(hi.raw - lo.raw), the
    // difference taken in uint64 (P0-01 6.3). Fatal if lo >= hi.
    [[nodiscard]] Fx uniform_fx(Fx lo, Fx hi);

    // uniform_fx01().raw < p.raw (P0-01 6.3). Always exactly 1 draw. Fatal
    // unless 0 <= p <= ONE.
    [[nodiscard]] bool chance(Fx p);

    // uniform_u64(den) < num (P0-01 6.3). Fatal if den = 0 or num > den.
    [[nodiscard]] bool chance_ratio(std::uint64_t num, std::uint64_t den);

    // Index i chosen with probability w[i] / sum(w) (P0-01 6.3): one
    // r = uniform_u64(total), then the first i with r < w[0] + ... + w[i].
    // Fatal if w is empty, the sum is 0, or the sum overflows uint64.
    // Callers with Fx weights pass raw values (which must be >= 0).
    [[nodiscard]] std::size_t weighted_index(std::span<const std::uint64_t> w);

    // Fisher-Yates, descending (P0-01 6.3): for i = n-1 down to 1,
    // j = uniform_u64(i + 1), swap a[i] and a[j]. n - 1 draws of uniform_u64;
    // none for n <= 1. Accepts any sized contiguous range: std::span<T>,
    // std::vector<T>, std::array<T, N>, a C array.
    template <class R>
        requires std::ranges::contiguous_range<R> && std::ranges::sized_range<R>
    void shuffle(R&& range) {
        auto* data = std::ranges::data(range);
        const auto n = static_cast<std::uint64_t>(std::ranges::size(range));
        for (std::uint64_t i = n; i > 1; --i) {
            const std::uint64_t hi_index = i - 1;
            const std::uint64_t j = uniform_u64(i);
            using std::swap;
            swap(data[static_cast<std::size_t>(hi_index)], data[static_cast<std::size_t>(j)]);
        }
    }

private:
    Rng(std::uint64_t s0, std::uint64_t s1, std::uint64_t s2, std::uint64_t s3) noexcept;

    std::uint64_t s0_ = 0;
    std::uint64_t s1_ = 0;
    std::uint64_t s2_ = 0;
    std::uint64_t s3_ = 0;
};

}  // namespace worldgen
