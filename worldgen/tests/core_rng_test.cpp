// Spec P0-01 section 13.4: SplitMix64, xoshiro256** (6.1, 6.2) and the
// sampling functions (6.3), with draw counts; property tests on fixed
// streams; the Rng invariants of section 11.

#include <array>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

#include "core_test_util.hpp"

using namespace worldgen;
using namespace worldgen_test;

namespace {

constexpr std::array<std::uint64_t, 4> S1234 = {1, 2, 3, 4};

Rng rng1234() { return Rng::from_state(1, 2, 3, 4); }

}  // namespace

TEST_CASE("P0-01 13.4: SplitMix64 reference outputs") {
    std::uint64_t x = 0;
    const std::array<std::uint64_t, 5> from0 = {0xE220A8397B1DCDAFULL, 0x6E789E6AA1B965F4ULL, 0x06C45D188009454FULL,
                                                0xF88BB8A8724C81ECULL, 0x1B39896A51A8749BULL};
    for (const std::uint64_t expected : from0) {
        CHECK(splitmix64_next(x) == expected);
    }
    CHECK(x == 5 * GOLDEN);  // x advances by GOLDEN per step, mod 2^64

    std::uint64_t y = 1234567;
    const std::array<std::uint64_t, 5> from1234567 = {6457827717110365317ULL, 3203168211198807973ULL,
                                                      9817491932198370423ULL, 4593380528125082431ULL,
                                                      16408922859458223821ULL};
    for (const std::uint64_t expected : from1234567) {
        CHECK(splitmix64_next(y) == expected);
    }
}

TEST_CASE("P0-01 13.4: xoshiro256** from_state(1, 2, 3, 4)") {
    Rng rng = rng1234();
    CHECK(rng.state() == S1234);
    CHECK(rng.next() == 11520);
    CHECK(rng.state() == std::array<std::uint64_t, 4>{7, 0, 262146, 211106232532992ULL});
    CHECK(rng.next() == 0);
    CHECK(rng.next() == 1509978240);
}

TEST_CASE("P0-01 13.4: Rng(0) is seeded from SplitMix64(0)") {
    Rng rng(0);
    CHECK(rng.state() == std::array<std::uint64_t, 4>{0xE220A8397B1DCDAFULL, 0x6E789E6AA1B965F4ULL,
                                                      0x06C45D188009454FULL, 0xF88BB8A8724C81ECULL});
    CHECK(rng.next() == 0x99EC5F36CB75F2B4ULL);
}

TEST_CASE("P0-01 13.4: from_state(0, 0, 0, 0) is fatal") {
    CHECK_FATAL(Rng::from_state(0, 0, 0, 0));
    static_cast<void>(Rng::from_state(0, 0, 0, 1));
}

TEST_CASE("P0-01 13.4: uniform_u64 values and draw counts") {
    Rng rng = rng1234();
    CHECK(rng.uniform_u64(10) == 0);
    CHECK(consumed_exactly(rng, S1234, 1));
    CHECK(rng.uniform_u64(3) == 0);  // draw 0 rejected because t = 1
    CHECK(consumed_exactly(rng, S1234, 3));
    // Compare the following next() against a twin advanced three times.
    Rng twin = rng1234();
    advance(twin, 3);
    CHECK(rng.next() == twin.next());

    Rng big = rng1234();
    CHECK(big.uniform_u64(std::uint64_t{1} << 63) == 5760);
    CHECK(consumed_exactly(big, S1234, 1));
}

TEST_CASE("P0-01 13.4: shuffle and weighted_index from from_state(1, 2, 3, 4)") {
    Rng rng = rng1234();
    std::array<char, 3> abc = {'a', 'b', 'c'};
    rng.shuffle(abc);
    CHECK(abc == std::array<char, 3>{'b', 'c', 'a'});
    CHECK(consumed_exactly(rng, S1234, 2));  // n - 1 calls, one draw each here

    // Same result through a span and a vector.
    Rng rng_span = rng1234();
    std::vector<char> v = {'a', 'b', 'c'};
    rng_span.shuffle(std::span<char>(v));
    CHECK(v == std::vector<char>{'b', 'c', 'a'});

    Rng w = rng1234();
    const std::array<std::uint64_t, 3> weights = {0, 5, 3};
    CHECK(w.weighted_index(weights) == 1);
    CHECK(consumed_exactly(w, S1234, 1));
}

TEST_CASE("P0-01 13.4: shuffle of n <= 1 makes no draws") {
    Rng rng = rng1234();
    std::vector<int> empty;
    rng.shuffle(empty);
    std::array<int, 1> one = {42};
    rng.shuffle(one);
    CHECK(one[0] == 42);
    CHECK(consumed_exactly(rng, S1234, 0));
}

TEST_CASE("P0-01 13.4: uniform_u64(1), uniform_u64(0), full-range uniform_i64") {
    Rng rng = rng1234();
    CHECK(rng.uniform_u64(1) == 0);
    CHECK(consumed_exactly(rng, S1234, 1));
    CHECK_FATAL(rng.uniform_u64(0));

    // Full int64 range: exactly one draw, int64(next()) of a twin.
    Rng a(0x0123456789ABCDEFULL);
    Rng b(0x0123456789ABCDEFULL);
    for (int i = 0; i < 100; ++i) {
        const std::int64_t got = a.uniform_i64(I64_MIN, I64_MAX);
        const std::uint64_t twin = b.next();
        REQUIRE(got == static_cast<std::int64_t>(twin));
        REQUIRE(a.state() == b.state());
    }
    CHECK_FATAL(a.uniform_i64(1, 0));
}

TEST_CASE("P0-01 13.4: chance(0) and chance(ONE), one draw each") {
    Rng rng(0x00C0FFEE00C0FFEEULL);
    Rng twin(0x00C0FFEE00C0FFEEULL);
    for (int i = 0; i < 10000; ++i) {
        REQUIRE_FALSE(rng.chance(Fx{}));
        advance(twin, 1);
        REQUIRE(rng.state() == twin.state());
    }
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(rng.chance(Fx::ONE));
        advance(twin, 1);
        REQUIRE(rng.state() == twin.state());
    }
    CHECK_FATAL(rng.chance(Fx::from_raw(-1)));
    CHECK_FATAL(rng.chance(Fx::from_raw(ONE_RAW + 1)));
}

TEST_CASE("P0-01 6.3: chance_ratio, uniform_fx and uniform_fx01 contracts") {
    Rng rng = rng1234();
    CHECK_FATAL(rng.chance_ratio(0, 0));
    CHECK_FATAL(rng.chance_ratio(2, 1));
    CHECK_FATAL(rng.uniform_fx(Fx::ONE, Fx::ONE));
    CHECK_FATAL(rng.uniform_fx(Fx::ONE, Fx{}));
    Rng r2 = rng1234();
    CHECK(r2.next_u32() == 0);  // 11520 >> 32
    CHECK(consumed_exactly(r2, S1234, 1));
    Rng r3 = rng1234();
    CHECK(r3.uniform_fx01().raw() == 0);
    CHECK(consumed_exactly(r3, S1234, 1));
}

TEST_CASE("P0-01 13.4: weighted_index errors") {
    Rng rng = rng1234();
    const std::vector<std::uint64_t> none;
    CHECK_FATAL(rng.weighted_index(none));
    const std::array<std::uint64_t, 2> zeros = {0, 0};
    CHECK_FATAL(rng.weighted_index(zeros));
    const std::array<std::uint64_t, 2> too_big = {U64_MAX, 1};
    CHECK_FATAL(rng.weighted_index(too_big));
    const std::array<std::uint64_t, 3> too_big3 = {std::uint64_t{1} << 63, std::uint64_t{1} << 62,
                                                   std::uint64_t{1} << 62};
    CHECK_FATAL(rng.weighted_index(too_big3));
    const std::array<std::uint64_t, 2> exactly_max = {U64_MAX - 1, 1};
    static_cast<void>(rng.weighted_index(exactly_max));  // sum = 2^64 - 1 is fine
}

TEST_CASE("P0-01 13.4: Rng is movable and not copyable") {
    static_assert(!std::is_copy_constructible_v<Rng>);
    static_assert(!std::is_copy_assignable_v<Rng>);
    static_assert(std::is_move_constructible_v<Rng>);
    static_assert(std::is_move_assignable_v<Rng>);
    Rng a(77);
    Rng twin(77);
    advance(a, 5);
    advance(twin, 5);
    Rng b = std::move(a);
    CHECK(b.next() == twin.next());
}

// --- Property tests (fixed streams) -----------------------------------------

TEST_CASE("P0-01 13.4: uniform_u64(s) < s for the listed bounds") {
    Rng rng(KEY_RNG_PROPS);
    const std::array<std::uint64_t, 9> bounds = {1,
                                                 2,
                                                 3,
                                                 7,
                                                 10,
                                                 1000,
                                                 (std::uint64_t{1} << 32) + 1,
                                                 (std::uint64_t{1} << 63) + 1,
                                                 U64_MAX};
    for (const std::uint64_t s : bounds) {
        for (int i = 0; i < 10000; ++i) {
            const std::uint64_t r = rng.uniform_u64(s);
            REQUIRE(r < s);
        }
    }
}

TEST_CASE("P0-01 11: uniform_i64, uniform_fx and uniform_fx01 stay in range") {
    Rng rng(KEY_RNG_PROPS ^ 2U);
    const std::array<std::pair<std::int64_t, std::int64_t>, 6> ranges = {{{0, 0},
                                                                          {-5, 5},
                                                                          {I64_MIN, I64_MIN + 3},
                                                                          {I64_MAX - 3, I64_MAX},
                                                                          {I64_MIN, I64_MAX - 1},
                                                                          {-1000000, 1}}};
    for (const auto& [lo, hi] : ranges) {
        for (int i = 0; i < 5000; ++i) {
            const std::int64_t r = rng.uniform_i64(lo, hi);
            REQUIRE(r >= lo);
            REQUIRE(r <= hi);
        }
    }
    const std::array<std::pair<Fx, Fx>, 4> fx_ranges = {{{Fx{}, Fx::ONE},
                                                         {Fx::MIN, Fx::MAX},
                                                         {Fx::from_int(-3), Fx::from_raw(-ONE_RAW * 3 + 1)},
                                                         {Fx::from_int(-7), Fx::from_int(11)}}};
    for (const auto& [lo, hi] : fx_ranges) {
        for (int i = 0; i < 5000; ++i) {
            const Fx r = rng.uniform_fx(lo, hi);
            REQUIRE(r >= lo);
            REQUIRE(r < hi);
        }
    }
    for (int i = 0; i < 5000; ++i) {
        const Fx r = rng.uniform_fx01();
        REQUIRE(r >= Fx{});
        REQUIRE(r < Fx::ONE);
    }
}

namespace {

// Pearson chi-squared for a uniform distribution over k cells, as an exact
// integer test: chi2 = sum (k*O - n)^2 / (k*n), so chi2 < crit is
// sum (k*O - n)^2 * 1000 < crit_milli * k * n.
bool chi_squared_below(Rng& rng, std::uint64_t k, std::uint64_t n, std::uint64_t crit_milli,
                       std::uint64_t& sum_out) {
    std::vector<std::uint64_t> counts(static_cast<std::size_t>(k), 0);
    for (std::uint64_t i = 0; i < n; ++i) {
        const std::uint64_t r = rng.uniform_u64(k);
        ++counts[static_cast<std::size_t>(r)];
    }
    std::uint64_t sum = 0;
    for (const std::uint64_t o : counts) {
        const std::uint64_t ko = k * o;
        const std::uint64_t diff = ko > n ? ko - n : n - ko;
        sum += diff * diff;
    }
    sum_out = sum;
    return sum * 1000 < crit_milli * k * n;
}

}  // namespace

TEST_CASE("P0-01 13.4: chi-squared of uniform_u64(10) and uniform_u64(7) over 10^6 draws") {
    Rng rng(KEY_RNG_PROPS ^ 3U);
    std::uint64_t sum = 0;
    // 0.999 critical values: 27.877 for 9 degrees of freedom, 22.458 for 6.
    CHECK(chi_squared_below(rng, 10, 1000000, 27877, sum));
    MESSAGE("uniform_u64(10): sum (kO - n)^2 = " << sum << ", chi2 * 10^7 = " << sum);
    CHECK(chi_squared_below(rng, 7, 1000000, 22458, sum));
    MESSAGE("uniform_u64(7): sum (kO - n)^2 = " << sum << ", chi2 = sum / 7e6");
}

TEST_CASE("P0-01 13.4: shuffle of 0..n-1 is a permutation for n in 0..64") {
    Rng rng(KEY_RNG_PROPS ^ 4U);
    for (std::size_t n = 0; n <= 64; ++n) {
        std::vector<std::uint64_t> a(n);
        for (std::size_t i = 0; i < n; ++i) {
            a[i] = static_cast<std::uint64_t>(i);
        }
        rng.shuffle(a);
        std::vector<int> seen(n, 0);
        for (const std::uint64_t x : a) {
            REQUIRE(x < static_cast<std::uint64_t>(n));
            ++seen[static_cast<std::size_t>(x)];
        }
        for (const int s : seen) {
            REQUIRE(s == 1);
        }
    }
}

TEST_CASE("P0-01 13.4: weighted_index frequencies with weights [1, 2, 3, 0, 4]") {
    Rng rng(KEY_RNG_PROPS ^ 5U);
    const std::array<std::uint64_t, 5> w = {1, 2, 3, 0, 4};
    const std::uint64_t n = 1000000;
    std::array<std::uint64_t, 5> counts{};
    for (std::uint64_t i = 0; i < n; ++i) {
        const std::size_t idx = rng.weighted_index(w);
        REQUIRE(idx < w.size());
        ++counts[idx];
    }
    CHECK(counts[3] == 0);
    // |O/n - w/10| <= 1/100  <=>  |10*O - w*n| <= n/10.
    for (std::size_t i = 0; i < w.size(); ++i) {
        const std::uint64_t got = 10 * counts[i];
        const std::uint64_t want = w[i] * n;
        const std::uint64_t diff = got > want ? got - want : want - got;
        INFO("index " << i << " count " << counts[i]);
        CHECK(diff <= n / 10);
    }
}

TEST_CASE("P0-01 11: weighted_index never returns a zero weight") {
    Rng rng(KEY_RNG_PROPS ^ 6U);
    const std::array<std::uint64_t, 7> w = {0, 1, 0, U64_MAX / 4, 0, 3, 0};
    for (int i = 0; i < 20000; ++i) {
        const std::size_t idx = rng.weighted_index(w);
        REQUIRE(idx < w.size());
        REQUIRE(w[idx] != 0);
    }
}
