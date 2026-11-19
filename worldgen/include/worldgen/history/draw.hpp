#pragma once

// Keyed draws of the history stages (spec P3-00 4.6, P3-02 4.1).
//
// Every random decision of the history is a keyed draw: a pure hash of the
// stream key, the year (or attempt number), up to two entity ids and a draw
// counter, never a value taken from a shared sequential generator (rule H4).
// The helpers turn a draw into an index, a yes/no or a weighted choice with
// integer arithmetic only.

#include <cstddef>
#include <cstdint>
#include <span>

#include "worldgen/core/fx.hpp"

namespace worldgen::history {

// hash_words(TAG_HIST, [stream_key, w0, a, b, k]) (P3-02 4.1), with
// stream_key = worldgen::stream_key(seed, name), computed once per stream by
// the caller. w0 is the year in the simulation's draws and the attempt
// number in the name generator's; a and b are entity ids or other integers
// (0 if unused); k counts the draws of one decision. Signed values are
// passed as their two's-complement bit pattern (encode_i64).
[[nodiscard]] std::uint64_t draw(std::uint64_t stream_key, std::uint64_t w0, std::uint64_t a, std::uint64_t b,
                                 std::uint64_t k) noexcept;

// ((u >> 32) * n) >> 32: an index in [0, n) (P3-02 4.1). Fatal if n = 0 or
// n > 2^32.
[[nodiscard]] std::uint64_t pick(std::uint64_t u, std::uint64_t n);

// (u >> 32) < p.raw: true with probability p (P3-02 4.1). Fatal if p is
// outside [0, ONE].
[[nodiscard]] bool chance(std::uint64_t u, Fx p);

// The smallest i with (u >> 32) * W < 2^32 * (w[0] + ... + w[i]), where
// W = w[0] + ... + w[m-1], compared exactly in 128 bits (P3-02 4.1). A
// zero weight is never chosen. Fatal if w is empty, a weight is negative,
// the sum is 0 or the sum exceeds 2^63 - 1 (P3-00 H6).
[[nodiscard]] std::size_t weighted(std::uint64_t u, std::span<const std::int64_t> w);

}  // namespace worldgen::history
