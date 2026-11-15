// Spec P0-01 section 13.3: wide arithmetic (5.3) and square roots (9.1).

#include "core_test_util.hpp"

using namespace worldgen;
using namespace worldgen_test;

namespace {

// Test-side schoolbook 64 x 64 -> 128 product in 32-bit digits, written
// independently of the library, to cross-check mul_u64_wide.
U128 schoolbook_mul(std::uint64_t a, std::uint64_t b) {
    const std::uint64_t mask = 0xFFFFFFFFULL;
    const std::uint64_t a0 = a & mask;
    const std::uint64_t a1 = a >> 32;
    const std::uint64_t b0 = b & mask;
    const std::uint64_t b1 = b >> 32;
    // Digits of the result, base 2^32, accumulated column by column.
    const std::uint64_t p00 = a0 * b0;
    const std::uint64_t p01 = a0 * b1;
    const std::uint64_t p10 = a1 * b0;
    const std::uint64_t p11 = a1 * b1;
    const std::uint64_t d0 = p00 & mask;
    const std::uint64_t col1 = (p00 >> 32) + (p01 & mask) + (p10 & mask);  // < 3 * 2^32
    const std::uint64_t d1 = col1 & mask;
    const std::uint64_t col2 = (col1 >> 32) + (p01 >> 32) + (p10 >> 32) + (p11 & mask);
    const std::uint64_t d2 = col2 & mask;
    const std::uint64_t col3 = (col2 >> 32) + (p11 >> 32);
    return U128{(col3 << 32) | d2, (d1 << 32) | d0};
}

// Square of a value below 2^64, as U128.
U128 sq(std::uint64_t r) { return mul_u64_wide(r, r); }

}  // namespace

TEST_CASE("P0-01 13.3: mul_u64_wide vectors") {
    CHECK(mul_u64_wide(U64_MAX, U64_MAX) == U128{0xFFFFFFFFFFFFFFFEULL, 1});
    CHECK(mul_u64_wide(0x100000000ULL, 0x100000000ULL) == U128{1, 0});
    CHECK(mul_u64_wide(0, U64_MAX) == U128{0, 0});
    CHECK(mul_u64_wide(1, U64_MAX) == U128{0, U64_MAX});
    CHECK(mul_u64_wide(U64_MAX, 2) == U128{1, 0xFFFFFFFFFFFFFFFEULL});
}

TEST_CASE("P0-01 13.3: divmod_u128_u64 vectors") {
    const U128DivMod r = divmod_u128_u64(U128{1, 0}, 3);
    CHECK(r.quotient == U128{0, 6148914691236517205ULL});
    CHECK(r.quotient.lo == 0x5555555555555555ULL);
    CHECK(r.remainder == 1);
    const U128DivMod m = divmod_u128_u64(U128{U64_MAX, U64_MAX}, 1);
    CHECK(m.quotient == U128{U64_MAX, U64_MAX});
    CHECK(m.remainder == 0);
    const U128DivMod k = divmod_u128_u64(U128{U64_MAX, U64_MAX}, U64_MAX);
    CHECK(k.quotient == U128{1, 1});
    CHECK(k.remainder == 0);
    CHECK_FATAL(divmod_u128_u64(U128{1, 0}, 0));
}

TEST_CASE("P0-01 5.3: U128 add, sub, shifts, comparison") {
    CHECK(U128{0, U64_MAX} + U128{0, 1} == U128{1, 0});
    CHECK(U128{1, 0} - U128{0, 1} == U128{0, U64_MAX});
    CHECK(U128{U64_MAX, U64_MAX} + U128{0, 1} == U128{0, 0});  // mod 2^128
    CHECK((U128{0, 1} << 64U) == U128{1, 0});
    CHECK((U128{0, 1} << 127U) == U128{0x8000000000000000ULL, 0});
    CHECK((U128{1, 0} >> 1U) == U128{0, 0x8000000000000000ULL});
    CHECK((U128{0x8000000000000000ULL, 0} >> 127U) == U128{0, 1});
    CHECK((U128{3, 5} << 0U) == U128{3, 5});
    CHECK_FATAL(U128{0, 1} << 128U);
    CHECK_FATAL(U128{0, 1} >> 128U);
    CHECK(U128{1, 0} > U128{0, U64_MAX});
    CHECK(U128{1, 2} < U128{1, 3});
    CHECK(u128_from_u64(7) == U128{0, 7});
}

TEST_CASE("P0-01 13.3: wide arithmetic agrees with an independent product over test.wide") {
    // There is no fast path (wide.hpp), so the reference-vs-fast comparison
    // of 13.3 does not apply. Instead check mul_u64_wide against a test-side
    // schoolbook product, and divmod against q * d + r == n with r < d.
    Rng rng(stream_key(0, streams::test_wide));
    for (int i = 0; i < 100000; ++i) {
        const std::uint64_t a = rng.next();
        const std::uint64_t b = rng.next();
        const U128 p = mul_u64_wide(a, b);
        REQUIRE(p == schoolbook_mul(a, b));
        const std::uint64_t shift = rng.next() >> 58;  // 0..63: vary divisor size
        std::uint64_t d = rng.next() >> shift;
        if (d == 0) {
            d = 1;
        }
        const std::uint64_t n_hi = rng.next();
        const std::uint64_t n_lo = rng.next();
        const U128 n{n_hi, n_lo};
        const U128DivMod qr = divmod_u128_u64(n, d);
        REQUIRE(qr.remainder < d);
        // q * d + r, computed as qhi * d * 2^64 + qlo * d + r. qhi * d must fit.
        const U128 lo_part = mul_u64_wide(qr.quotient.lo, d);
        const U128 hi_part = mul_u64_wide(qr.quotient.hi, d);
        REQUIRE(hi_part.hi == 0);
        const U128 sum = U128{hi_part.lo, 0} + lo_part + u128_from_u64(qr.remainder);
        REQUIRE(sum == n);
    }
}

TEST_CASE("P0-01 13.3: isqrt_u64 vectors") {
    CHECK(isqrt_u64(0) == 0);
    CHECK(isqrt_u64(1) == 1);
    CHECK(isqrt_u64(2) == 1);
    CHECK(isqrt_u64(3) == 1);
    CHECK(isqrt_u64(4) == 2);
    CHECK(isqrt_u64(15) == 3);
    CHECK(isqrt_u64(16) == 4);
    CHECK(isqrt_u64(U64_MAX) == 4294967295ULL);
    CHECK(isqrt_u128(U128{U64_MAX, U64_MAX}) == U64_MAX);
    CHECK(isqrt_u128(U128{1, 0}) == 0x100000000ULL);
    CHECK(isqrt_u128(U128{0, 15}) == 3);
}

TEST_CASE("P0-01 9.1: isqrt property r^2 <= n < (r+1)^2") {
    Rng rng(KEY_SQRT_PROPS);
    for (int i = 0; i < 20000; ++i) {
        const std::uint64_t s64 = rng.next() >> 58;  // 0..63
        const std::uint64_t n64 = rng.next() >> s64;
        const std::uint64_t r64 = isqrt_u64(n64);
        REQUIRE(sq(r64) <= u128_from_u64(n64));
        REQUIRE(sq(r64 + 1) > u128_from_u64(n64));
        const unsigned int s = static_cast<unsigned int>(rng.next() >> 57);  // 0..127
        const std::uint64_t n_hi = rng.next();
        const std::uint64_t n_lo = rng.next();
        const U128 n = U128{n_hi, n_lo} >> s;
        const std::uint64_t r = isqrt_u128(n);
        REQUIRE(sq(r) <= n);
        if (r != U64_MAX) {
            REQUIRE(sq(r + 1) > n);
        }
    }
}

TEST_CASE("P0-01 13.3: sqrt vectors") {
    CHECK(sqrt(Fx{}).raw() == 0);
    CHECK(sqrt(raw(1)).raw() == 65536);
    CHECK(sqrt(Fx::from_ratio(1, 4)).raw() == 2147483648);
    CHECK(sqrt(fxi(4)).raw() == 8589934592);
    CHECK(sqrt(fxi(2)).raw() == 6074001000);
    CHECK(sqrt(fxi(3)).raw() == 7439101574);
    CHECK(sqrt(fxi(5)).raw() == 9603838835);
    CHECK(sqrt(Fx::ONE) == Fx::ONE);
    CHECK_FATAL(sqrt(raw(-1)));
    CHECK_FATAL(sqrt(Fx::MIN));
    static_cast<void>(sqrt(Fx::MAX));  // never fatal for x >= 0
}

TEST_CASE("P0-01 13.3: hypot") {
    CHECK(hypot(fxi(3), fxi(4)) == fxi(5));
    CHECK(hypot(fxi(-3), fxi(4)) == fxi(5));
    CHECK(hypot(Fx{}, Fx{}) == Fx{});
    CHECK(hypot(Fx::MAX, Fx{}) == Fx::MAX);
    CHECK_FATAL(hypot(Fx::MAX, Fx::MAX));
}

TEST_CASE("P0-01 13.3: sqrt rounds to nearest, (2r-1)^2 <= 4N < (2r+1)^2") {
    Rng rng(KEY_SQRT_PROPS ^ 1U);
    int checked = 0;
    while (checked < 100000) {
        // Positive raw values of every magnitude: shift a 63-bit draw right by 0..63.
        const std::uint64_t shift = rng.next() >> 58;
        const std::int64_t xr = static_cast<std::int64_t>((rng.next() >> 1) >> shift);
        if (xr == 0) {
            continue;  // x = 0 excluded by 13.3
        }
        const std::uint64_t r = static_cast<std::uint64_t>(sqrt(raw(xr)).raw());
        // 4N = x.raw * 2^34 < 2^97.
        const U128 four_n = u128_from_u64(static_cast<std::uint64_t>(xr)) << 34U;
        REQUIRE(r >= 1);
        REQUIRE(sq(2 * r - 1) <= four_n);
        REQUIRE(four_n < sq(2 * r + 1));
        ++checked;
    }
}
