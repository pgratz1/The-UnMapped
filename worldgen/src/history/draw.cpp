// Keyed draws of the history stages (spec P3-00 4.6, P3-02 4.1).

#include "worldgen/history/draw.hpp"

#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen::history {

namespace {

constexpr std::uint64_t TWO_POW_32 = std::uint64_t{1} << 32U;

}  // namespace

std::uint64_t draw(std::uint64_t stream_key, std::uint64_t w0, std::uint64_t a, std::uint64_t b,
                   std::uint64_t k) noexcept {
    return hash_words(TAG_HIST, {stream_key, w0, a, b, k});
}

std::uint64_t pick(std::uint64_t u, std::uint64_t n) {
    if (n == 0 || n > TWO_POW_32) {
        fatal("history::pick: n must be in [1, 2^32]");
    }
    // (u >> 32) < 2^32 and n <= 2^32, so the product is below 2^64.
    return ((u >> 32U) * n) >> 32U;
}

bool chance(std::uint64_t u, Fx p) {
    if (p.raw() < 0 || p.raw() > Fx::ONE.raw()) {
        fatal("history::chance: p must be in [0, ONE]");
    }
    return (u >> 32U) < static_cast<std::uint64_t>(p.raw());
}

std::size_t weighted(std::uint64_t u, std::span<const std::int64_t> w) {
    if (w.empty()) {
        fatal("history::weighted: no weights");
    }
    std::int64_t total = 0;
    for (const std::int64_t wi : w) {
        if (wi < 0) {
            fatal("history::weighted: a weight is negative");
        }
        total = checked_add(total, wi);
    }
    if (total == 0) {
        fatal("history::weighted: the weights sum to 0");
    }
    // lhs < 2^32 * W, so the last index with a positive weight always
    // satisfies the condition.
    const U128 lhs = mul_u64_wide(u >> 32U, static_cast<std::uint64_t>(total));
    std::int64_t prefix = 0;
    for (std::size_t i = 0; i < w.size(); ++i) {
        prefix += w[i];  // no overflow: prefix <= total
        if (lhs < mul_u64_wide(TWO_POW_32, static_cast<std::uint64_t>(prefix))) {
            return i;
        }
    }
    fatal("history::weighted: unreachable");
}

}  // namespace worldgen::history
