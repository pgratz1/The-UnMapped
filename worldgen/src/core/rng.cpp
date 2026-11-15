// xoshiro256** and sampling functions (P0-01 6).

#include "worldgen/core/rng.hpp"

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen {

namespace {

constexpr std::uint64_t rotl(std::uint64_t x, unsigned int k) noexcept {
    // 0 < k < 64 at every call site.
    return (x << k) | (x >> (64U - k));
}

}  // namespace

std::uint64_t splitmix64_next(std::uint64_t& x) noexcept {
    x += GOLDEN;
    return mix64(x);
}

Rng::Rng(std::uint64_t key) noexcept {
    std::uint64_t x = key;
    s0_ = splitmix64_next(x);
    s1_ = splitmix64_next(x);
    s2_ = splitmix64_next(x);
    s3_ = splitmix64_next(x);
}

Rng::Rng(std::uint64_t s0, std::uint64_t s1, std::uint64_t s2, std::uint64_t s3) noexcept
    : s0_(s0), s1_(s1), s2_(s2), s3_(s3) {}

Rng Rng::from_state(std::uint64_t s0, std::uint64_t s1, std::uint64_t s2, std::uint64_t s3) {
    if ((s0 | s1 | s2 | s3) == 0) {
        fatal("Rng::from_state: all-zero state");
    }
    return Rng(s0, s1, s2, s3);
}

std::array<std::uint64_t, 4> Rng::state() const noexcept {
    return {s0_, s1_, s2_, s3_};
}

std::uint64_t Rng::next() noexcept {
    const std::uint64_t result = rotl(s1_ * 5U, 7U) * 9U;
    const std::uint64_t t = s1_ << 17U;
    s2_ ^= s0_;
    s3_ ^= s1_;
    s1_ ^= s2_;
    s0_ ^= s3_;
    s2_ ^= t;
    s3_ = rotl(s3_, 45U);
    return result;
}

std::uint32_t Rng::next_u32() noexcept {
    return static_cast<std::uint32_t>(next() >> 32U);
}

std::uint64_t Rng::uniform_u64(std::uint64_t s) {
    if (s == 0) {
        fatal("Rng::uniform_u64: empty range");
    }
    std::uint64_t x = next();
    U128 m = mul_u64_wide(x, s);
    if (m.lo < s) {
        const std::uint64_t t = (std::uint64_t{0} - s) % s;
        while (m.lo < t) {
            x = next();
            m = mul_u64_wide(x, s);
        }
    }
    return m.hi;
}

std::int64_t Rng::uniform_i64(std::int64_t lo, std::int64_t hi) {
    if (lo > hi) {
        fatal("Rng::uniform_i64: lo > hi");
    }
    const std::uint64_t span = static_cast<std::uint64_t>(hi) - static_cast<std::uint64_t>(lo);
    if (span == ~std::uint64_t{0}) {
        return static_cast<std::int64_t>(next());
    }
    const std::uint64_t offset = uniform_u64(span + 1U);
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(lo) + offset);
}

Fx Rng::uniform_fx01() noexcept {
    return Fx::from_raw(static_cast<std::int64_t>(next() >> 32U));
}

Fx Rng::uniform_fx(Fx lo, Fx hi) {
    if (lo.raw() >= hi.raw()) {
        fatal("Rng::uniform_fx: lo >= hi");
    }
    const std::uint64_t span = static_cast<std::uint64_t>(hi.raw()) - static_cast<std::uint64_t>(lo.raw());
    const std::uint64_t offset = uniform_u64(span);
    return Fx::from_raw(static_cast<std::int64_t>(static_cast<std::uint64_t>(lo.raw()) + offset));
}

bool Rng::chance(Fx p) {
    if (p.raw() < 0 || p.raw() > Fx::ONE.raw()) {
        fatal("Rng::chance: p outside [0, ONE]");
    }
    return uniform_fx01().raw() < p.raw();
}

bool Rng::chance_ratio(std::uint64_t num, std::uint64_t den) {
    if (den == 0) {
        fatal("Rng::chance_ratio: den = 0");
    }
    if (num > den) {
        fatal("Rng::chance_ratio: num > den");
    }
    return uniform_u64(den) < num;
}

std::size_t Rng::weighted_index(std::span<const std::uint64_t> w) {
    if (w.empty()) {
        fatal("Rng::weighted_index: no weights");
    }
    std::uint64_t total = 0;
    for (const std::uint64_t wi : w) {
        if (wi > ~std::uint64_t{0} - total) {
            fatal("Rng::weighted_index: weight sum overflows uint64");
        }
        total += wi;
    }
    if (total == 0) {
        fatal("Rng::weighted_index: all weights are zero");
    }
    const std::uint64_t r = uniform_u64(total);
    std::uint64_t acc = 0;
    std::size_t i = 0;
    for (; i + 1 < w.size(); ++i) {
        acc += w[i];
        if (r < acc) {
            return i;
        }
    }
    // r < total = acc + w[last], and every earlier prefix was <= r.
    return i;
}

}  // namespace worldgen
