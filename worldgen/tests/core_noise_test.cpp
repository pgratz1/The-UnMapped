// Spec P0-01 section 13.6: integer gradient noise noise2 (8.1) and fbm2 (8.2).

#include <array>
#include <vector>

#include "core_test_util.hpp"

using namespace worldgen;
using namespace worldgen_test;

namespace {

// Ten noise keys from a fixed test stream.
std::array<std::uint64_t, 10> noise_keys() {
    Rng rng(KEY_NOISE_PROPS);
    std::array<std::uint64_t, 10> keys{};
    for (std::uint64_t& k : keys) {
        k = rng.next();
    }
    keys[0] = stream_key(0, streams::test_noise);  // include the P0-04 test-pattern key
    return keys;
}

// dot(g, u, v) from the gradient table of 8.1, on raw values.
std::int64_t dot(std::uint64_t g, std::int64_t u, std::int64_t v) {
    switch (g) {
        case 0: return u + v;
        case 1: return -u + v;
        case 2: return u - v;
        case 3: return -u - v;
        case 4: return u;
        case 5: return -u;
        case 6: return v;
        case 7: return -v;
        default: FAIL("gradient index out of range"); return 0;
    }
}

// Test pattern fbm2 parameters (golden file, P0-04): 5 octaves, 1/32, 2, 1/2.
const Fx BASE_1_32 = Fx::from_ratio(1, 32);
const Fx LAC_2 = Fx::from_int(2);
const Fx GAIN_HALF = Fx::HALF;

std::int64_t iabs(std::int64_t v) { return v < 0 ? -v : v; }

}  // namespace

TEST_CASE("P0-01 13.6: noise2 is 0 at integer lattice points") {
    Rng rng(KEY_NOISE_PROPS ^ 1U);
    const auto keys = noise_keys();
    for (const std::uint64_t k : keys) {
        for (int n = 0; n < 1000; ++n) {
            std::int64_t i = 0;
            std::int64_t j = 0;
            if (n < 900) {
                i = rng.uniform_i64(-1000, 1000);
                j = rng.uniform_i64(-1000, 1000);
            } else {
                i = rng.uniform_i64(-2147483647, 2147483647);
                j = rng.uniform_i64(-2147483647, 2147483647);
            }
            INFO("key " << k << " point " << i << ", " << j);
            REQUIRE(noise2(k, Fx::from_int(i), Fx::from_int(j)) == Fx{});
        }
    }
}

TEST_CASE("P0-01 13.6: noise2 at cell centres is the mean of the four corner dots") {
    Rng rng(KEY_NOISE_PROPS ^ 2U);
    const auto keys = noise_keys();
    for (const std::uint64_t k : keys) {
        for (int n = 0; n < 1000; ++n) {
            const std::int64_t i = rng.uniform_i64(-100000, 100000);
            const std::int64_t j = rng.uniform_i64(-100000, 100000);
            std::int64_t sum = 0;
            for (std::int64_t dy = 0; dy <= 1; ++dy) {
                for (std::int64_t dx = 0; dx <= 1; ++dx) {
                    const std::uint64_t g = hash_at(k, i + dx, j + dy) >> 61;
                    const std::int64_t u = HALF_RAW - dx * ONE_RAW;
                    const std::int64_t v = HALF_RAW - dy * ONE_RAW;
                    sum += dot(g, u, v);
                }
            }
            REQUIRE(sum % 4 == 0);  // each d is a multiple of 1/2: exact
            const Fx x = Fx::from_raw(i * ONE_RAW + HALF_RAW);
            const Fx y = Fx::from_raw(j * ONE_RAW + HALF_RAW);
            INFO("key " << k << " cell " << i << ", " << j);
            REQUIRE(noise2(k, x, y).raw() == sum / 4);
        }
    }
}

TEST_CASE("P0-01 13.6: noise2 and fbm2 stay in [-ONE, ONE] on a 512 x 512 grid") {
    // Grid step 1/7 cell, centred on the origin so negative coordinates are
    // covered: x = (i - 256) / 7 for i in 0..511.
    const auto keys = noise_keys();
    std::vector<Fx> coord(512);
    for (std::size_t i = 0; i < coord.size(); ++i) {
        coord[i] = Fx::from_ratio(static_cast<std::int64_t>(i) - 256, 7);
    }
    // Track the extremes and check them once: millions of doctest assertions
    // would dominate the run time.
    std::int64_t n_lo = 0;
    std::int64_t n_hi = 0;
    std::int64_t f_lo = 0;
    std::int64_t f_hi = 0;
    std::uint64_t samples = 0;
    for (const std::uint64_t k : keys) {
        for (const Fx y : coord) {
            for (const Fx x : coord) {
                const std::int64_t n = noise2(k, x, y).raw();
                const std::int64_t f = fbm2(k, x, y, 5, BASE_1_32, LAC_2, GAIN_HALF).raw();
                n_lo = n < n_lo ? n : n_lo;
                n_hi = n > n_hi ? n : n_hi;
                f_lo = f < f_lo ? f : f_lo;
                f_hi = f > f_hi ? f : f_hi;
                ++samples;
            }
        }
    }
    CHECK(samples == 10U * 512U * 512U);
    CHECK(n_lo >= -ONE_RAW);
    CHECK(n_hi <= ONE_RAW);
    CHECK(f_lo >= -ONE_RAW);
    CHECK(f_hi <= ONE_RAW);
    // Not degenerate: the noise takes values well away from zero.
    CHECK(n_lo < -ONE_RAW / 4);
    CHECK(n_hi > ONE_RAW / 4);
    CHECK(f_lo < 0);
    CHECK(f_hi > 0);
}

TEST_CASE("P0-01 8.1: noise2 accepts extreme coordinates") {
    const auto keys = noise_keys();
    const std::array<Fx, 6> extremes = {Fx::MAX, Fx::MIN, Fx::from_raw(1), Fx::from_raw(-1),
                                        Fx::from_raw(Fx::MAX.raw() - ONE_RAW), Fx::from_raw(Fx::MIN.raw() + 1)};
    for (const std::uint64_t k : keys) {
        for (const Fx x : extremes) {
            for (const Fx y : extremes) {
                const Fx n = noise2(k, x, y);
                REQUIRE(n >= -Fx::ONE);
                REQUIRE(n <= Fx::ONE);
            }
        }
    }
}

TEST_CASE("P0-01 13.6: noise2 continuity, |delta| <= 2^-9 for steps of 2^-12") {
    const auto keys = noise_keys();
    const std::int64_t step = std::int64_t{1} << 20;   // 2^-12 in raw
    const std::int64_t bound = std::int64_t{1} << 23;  // 2^-9 in raw
    const std::int64_t steps = 2 * 4096 + 7;           // crosses two lattice lines
    for (const std::uint64_t k : keys) {
        for (const std::int64_t base : {std::int64_t{-1} * ONE_RAW - 12345, std::int64_t{37} * ONE_RAW + 999}) {
            const std::int64_t other = base / 3;
            Fx prev_x = noise2(k, Fx::from_raw(base), Fx::from_raw(other));
            Fx prev_y = noise2(k, Fx::from_raw(other), Fx::from_raw(base));
            for (std::int64_t s = 1; s <= steps; ++s) {
                const std::int64_t t = base + s * step;
                const Fx nx = noise2(k, Fx::from_raw(t), Fx::from_raw(other));
                const Fx ny = noise2(k, Fx::from_raw(other), Fx::from_raw(t));
                REQUIRE(iabs(nx.raw() - prev_x.raw()) <= bound);
                REQUIRE(iabs(ny.raw() - prev_y.raw()) <= bound);
                prev_x = nx;
                prev_y = ny;
            }
        }
    }
}

TEST_CASE("P0-01 13.6: fbm2 with one octave at base_freq ONE is noise2 of substream(k, 0)") {
    Rng rng(KEY_NOISE_PROPS ^ 3U);
    const auto keys = noise_keys();
    for (const std::uint64_t k : keys) {
        for (int n = 0; n < 100; ++n) {
            const Fx x = Fx::from_raw(rng.uniform_i64(-1000 * ONE_RAW, 1000 * ONE_RAW));
            const Fx y = Fx::from_raw(rng.uniform_i64(-1000 * ONE_RAW, 1000 * ONE_RAW));
            const Fx expected = noise2(substream(k, 0), x, y);
            REQUIRE(fbm2(k, x, y, 1, Fx::ONE, LAC_2, GAIN_HALF) == expected);
            REQUIRE(fbm2(k, x, y, 1, Fx::ONE, Fx::ONE, Fx::ONE) == expected);
        }
    }
}

TEST_CASE("P0-01 8.2: fbm2 follows the octave loop") {
    // Recompute fbm2 from noise2 and the loop of 8.2, for a few parameter sets.
    Rng rng(KEY_NOISE_PROPS ^ 4U);
    const auto keys = noise_keys();
    struct Params {
        std::int32_t octaves;
        Fx base;
        Fx lac;
        Fx gain;
    };
    const std::array<Params, 4> params = {{{5, BASE_1_32, LAC_2, GAIN_HALF},
                                           {16, Fx::from_ratio(1, 1024), Fx::from_ratio(3, 2), Fx::from_ratio(2, 3)},
                                           {3, Fx::from_ratio(1, 3), Fx::from_ratio(1, 2), Fx::ONE},
                                           {2, Fx::ONE, Fx::from_int(3), Fx::from_raw(1)}}};
    for (const std::uint64_t k : keys) {
        for (const Params& p : params) {
            for (int n = 0; n < 20; ++n) {
                const Fx x = Fx::from_raw(rng.uniform_i64(-500 * ONE_RAW, 500 * ONE_RAW));
                const Fx y = Fx::from_raw(rng.uniform_i64(-500 * ONE_RAW, 500 * ONE_RAW));
                Fx sum{};
                Fx norm{};
                Fx amp = Fx::ONE;
                Fx f = p.base;
                for (std::int32_t o = 0; o < p.octaves; ++o) {
                    const std::uint64_t ko = substream(k, static_cast<std::uint64_t>(o));
                    const Fx nv = noise2(ko, mul(x, f), mul(y, f));
                    sum = sum + mul(amp, nv);
                    norm = norm + amp;
                    amp = mul(amp, p.gain);
                    f = mul(f, p.lac);
                }
                const Fx expected = clamp(div(sum, norm), -Fx::ONE, Fx::ONE);
                REQUIRE(fbm2(k, x, y, p.octaves, p.base, p.lac, p.gain) == expected);
            }
        }
    }
}

TEST_CASE("P0-01 13.6: fbm2 argument checks") {
    const std::uint64_t k = stream_key(0, streams::test_noise);
    const Fx x = Fx::from_ratio(7, 3);
    const Fx y = Fx::from_ratio(-5, 2);
    CHECK_FATAL(fbm2(k, x, y, 0, Fx::ONE, LAC_2, GAIN_HALF));
    CHECK_FATAL(fbm2(k, x, y, 17, Fx::ONE, LAC_2, GAIN_HALF));
    CHECK_FATAL(fbm2(k, x, y, -1, Fx::ONE, LAC_2, GAIN_HALF));
    CHECK_FATAL(fbm2(k, x, y, 1, Fx::ONE, LAC_2, Fx{}));
    CHECK_FATAL(fbm2(k, x, y, 1, Fx{}, LAC_2, GAIN_HALF));
    CHECK_FATAL(fbm2(k, x, y, 1, -Fx::ONE, LAC_2, GAIN_HALF));
    CHECK_FATAL(fbm2(k, x, y, 1, Fx::ONE, Fx{}, GAIN_HALF));
    CHECK_FATAL(fbm2(k, x, y, 1, Fx::ONE, -LAC_2, GAIN_HALF));
    CHECK_FATAL(fbm2(k, x, y, 1, Fx::ONE, LAC_2, Fx::from_raw(ONE_RAW + 1)));
    CHECK_FATAL(fbm2(k, x, y, 1, Fx::ONE, LAC_2, -GAIN_HALF));
    // Valid limits.
    static_cast<void>(fbm2(k, x, y, 1, Fx::ONE, LAC_2, Fx::ONE));
    static_cast<void>(fbm2(k, x, y, 16, Fx::from_ratio(1, 65536), LAC_2, GAIN_HALF));
    // Overflow of mul(x, f) is fatal.
    CHECK_FATAL(fbm2(k, Fx::from_int(1 << 20), y, 1, Fx::from_int(1 << 12), LAC_2, GAIN_HALF));
}
