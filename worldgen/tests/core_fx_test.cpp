// Spec P0-01 section 13.1 (Fx unit tests), the plain-integer helpers of 5.4,
// the fatal error type of 4.1, and the Fx invariants of section 11.

#include <type_traits>

#include "core_test_util.hpp"

using namespace worldgen;
using namespace worldgen_test;

TEST_CASE("P0-01 4.1: FatalError is a std::exception carrying a message") {
    static_assert(std::is_base_of_v<std::exception, FatalError>);
    bool caught = false;
    try {
        static_cast<void>(Fx::from_raw(I64_MIN));
    } catch (const std::exception& e) {
        caught = dynamic_cast<const FatalError*>(&e) != nullptr;
        CHECK(std::string(e.what()).size() > 0);
    }
    CHECK(caught);
    CHECK_FATAL(fatal("test message"));
}

TEST_CASE("P0-01 5.1: Fx constants and default construction") {
    CHECK(Fx::ONE.raw() == 4294967296);
    CHECK(Fx::HALF.raw() == 2147483648);
    CHECK(Fx::MAX.raw() == I64_MAX);
    CHECK(Fx::MIN.raw() == -I64_MAX);
    CHECK(Fx::PI.raw() == 13493037705);
    CHECK(Fx{}.raw() == 0);
    static_assert(!std::is_convertible_v<std::int64_t, Fx>);
    static_assert(!std::is_convertible_v<Fx, std::int64_t>);
    static_assert(!std::is_constructible_v<Fx, std::int64_t>);
    static_assert(!std::is_constructible_v<Fx, int>);
    // Comparison compares raw values.
    CHECK(raw(-1) < raw(0));
    CHECK(raw(5) == raw(5));
    CHECK(raw(6) > raw(5));
}

TEST_CASE("P0-01 13.1: conversions") {
    CHECK(Fx::from_int(1).raw() == 4294967296);
    CHECK(Fx::from_int(-3).raw() == -12884901888);
    CHECK(Fx::from_int(2147483647).raw() == std::int64_t{2147483647} * ONE_RAW);
    CHECK(Fx::from_int(-2147483647).raw() == std::int64_t{-2147483647} * ONE_RAW);
    CHECK_FATAL(Fx::from_int(2147483648));
    CHECK_FATAL(Fx::from_int(-2147483648));
    CHECK_FATAL(Fx::from_raw(I64_MIN));
    CHECK(Fx::from_raw(I64_MAX).raw() == I64_MAX);
    CHECK(Fx::from_raw(-I64_MAX).raw() == -I64_MAX);
    CHECK(Fx::from_ratio(1, 3).raw() == 1431655765);
    CHECK(Fx::from_ratio(-1, 3).raw() == -1431655765);
    CHECK(Fx::from_ratio(1, -3).raw() == -1431655765);
    CHECK(Fx::from_ratio(1, 4).raw() == 1073741824);
    CHECK_FATAL(Fx::from_ratio(1, 0));
    CHECK_FATAL(Fx::from_ratio(2147483648, 1));  // 2^63 out of range
}

TEST_CASE("P0-01 13.1: floor, ceil, round, trunc") {
    const Fx neg = raw(-6442450944);  // -1.5
    CHECK(neg.floor_int() == -2);
    CHECK(neg.ceil_int() == -1);
    CHECK(neg.round_int() == -2);
    CHECK(neg.trunc_int() == -1);
    const Fx pos = raw(6442450944);  // 1.5
    CHECK(pos.floor_int() == 1);
    CHECK(pos.ceil_int() == 2);
    CHECK(pos.round_int() == 2);
    CHECK(pos.trunc_int() == 1);
    // Extremes never fail.
    CHECK(Fx::MAX.floor_int() == 2147483647);
    CHECK(Fx::MAX.ceil_int() == 2147483648);
    CHECK(Fx::MAX.round_int() == 2147483648);
    CHECK(Fx::MAX.trunc_int() == 2147483647);
    CHECK(Fx::MIN.floor_int() == -2147483648);
    CHECK(Fx::MIN.ceil_int() == -2147483647);
    CHECK(Fx::MIN.round_int() == -2147483648);
    CHECK(Fx::MIN.trunc_int() == -2147483647);
}

TEST_CASE("P0-01 13.1: mul rounding (RHA) and overflow") {
    CHECK(mul(raw(6442450944), raw(10737418240)).raw() == 16106127360);
    CHECK((raw(6442450944) * raw(10737418240)).raw() == 16106127360);
    CHECK(mul(raw(1), raw(1)).raw() == 0);
    CHECK(mul(raw(1), Fx::HALF).raw() == 1);
    CHECK(mul(raw(-1), Fx::HALF).raw() == -1);
    CHECK(mul(raw(3), Fx::HALF).raw() == 2);
    CHECK(mul(raw(-3), Fx::HALF).raw() == -2);
    CHECK(mul(fxi(65536), fxi(32767)) == fxi(2147418112));
    CHECK_FATAL(mul(fxi(65536), fxi(32768)));
    CHECK_FATAL(fxi(65536) * fxi(32768));
    CHECK_FATAL(mul(fxi(-65536), fxi(32768)));  // -2^63 = INT64_MIN is not valid
}

TEST_CASE("P0-01 13.1: div rounding (RHA) and errors") {
    CHECK(div(Fx::ONE, fxi(3)).raw() == 1431655765);
    CHECK((Fx::ONE / fxi(3)).raw() == 1431655765);
    CHECK(div(fxi(2), fxi(3)).raw() == 2863311531);
    CHECK(div(fxi(-2), fxi(3)).raw() == -2863311531);
    CHECK(div(raw(1), fxi(2)).raw() == 1);
    CHECK(div(raw(-1), fxi(2)).raw() == -1);
    CHECK_FATAL(div(Fx::ONE, raw(1)));
    CHECK_FATAL(div(Fx::ONE, Fx{}));
    CHECK_FATAL(div(Fx{}, Fx{}));
    CHECK_FATAL(Fx::ONE / raw(0));
}

TEST_CASE("P0-01 13.1 / 5.4: mul_div") {
    CHECK(mul_div(7, 3, 2) == 11);
    CHECK(mul_div(-7, 3, 2) == -11);
    CHECK(mul_div(7, -3, 2) == -11);
    CHECK(mul_div(7, 3, -2) == -11);
    CHECK(mul_div(-7, -3, 2) == 11);
    CHECK_FATAL(mul_div(1, 1, 0));
    CHECK(mul_div(I64_MAX, I64_MAX, I64_MAX) == I64_MAX);
    // INT64_MIN is accepted as an argument; only the result is checked.
    CHECK(mul_div(I64_MIN, 1, 2) == -4611686018427387904);
    CHECK_FATAL(mul_div(I64_MIN, 1, 1));   // result = INT64_MIN
    CHECK_FATAL(mul_div(I64_MIN, -1, 1));  // result = 2^63
    CHECK_FATAL(mul_div(I64_MAX, 2, 1));
}

TEST_CASE("P0-01 5.4: checked helpers, floor_div, floor_mod") {
    CHECK(checked_add(1, 2) == 3);
    CHECK_FATAL(checked_add(I64_MAX, 1));
    CHECK_FATAL(checked_add(I64_MIN, -1));
    CHECK(checked_add(I64_MIN + 1, -1) == I64_MIN);  // INT64_MIN allowed for plain ints
    CHECK(checked_sub(-1, I64_MAX) == I64_MIN);
    CHECK_FATAL(checked_sub(I64_MIN, 1));
    CHECK_FATAL(checked_sub(0, I64_MIN));
    CHECK(checked_mul(-3, 4) == -12);
    CHECK(checked_mul(I64_MIN, 1) == I64_MIN);
    CHECK_FATAL(checked_mul(I64_MIN, -1));
    CHECK_FATAL(checked_mul(std::int64_t{1} << 32, std::int64_t{1} << 31));
    CHECK(checked_mul(-(std::int64_t{1} << 32), std::int64_t{1} << 31) == I64_MIN);

    CHECK(floor_div(7, 2) == 3);
    CHECK(floor_div(-7, 2) == -4);
    CHECK(floor_div(7, -2) == -4);
    CHECK(floor_div(-7, -2) == 3);
    CHECK(floor_mod(7, 2) == 1);
    CHECK(floor_mod(-7, 2) == 1);
    CHECK(floor_mod(7, -2) == -1);
    CHECK(floor_mod(-7, -2) == -1);
    CHECK(floor_mod(6, -3) == 0);
    CHECK_FATAL(floor_div(1, 0));
    CHECK_FATAL(floor_mod(1, 0));
    CHECK_FATAL(floor_div(I64_MIN, -1));
    CHECK(floor_mod(I64_MIN, -1) == 0);
    CHECK(floor_div(I64_MIN, 1) == I64_MIN);
}

TEST_CASE("P0-01 5.4: add, sub, neg, abs, saturating") {
    CHECK_FATAL(Fx::MAX + raw(1));
    CHECK(sat_add(Fx::MAX, raw(1)) == Fx::MAX);
    CHECK_FATAL(Fx::MIN - raw(1));
    CHECK(sat_sub(Fx::MIN, raw(1)) == Fx::MIN);
    CHECK(sat_add(Fx::MIN, raw(-5)) == Fx::MIN);
    CHECK(sat_sub(Fx::MAX, raw(-5)) == Fx::MAX);
    CHECK_FATAL(Fx::MIN + Fx::MIN);
    CHECK((Fx::MAX + Fx::MIN).raw() == 0);
    CHECK((-Fx::MIN) == Fx::MAX);
    CHECK(abs(Fx::MIN) == Fx::MAX);
    CHECK(abs(raw(-7)).raw() == 7);
    CHECK((raw(5) - raw(7)).raw() == -2);
}

TEST_CASE("P0-01 5.4: mul_int, div_int, min, max, clamp") {
    CHECK(mul_int(Fx::ONE, 3).raw() == 12884901888);
    CHECK_FATAL(mul_int(Fx::MAX, 2));
    CHECK_FATAL(mul_int(raw(std::int64_t{1} << 62), -2));  // INT64_MIN result
    CHECK(div_int(Fx::ONE, 3).raw() == 1431655765);
    CHECK(div_int(raw(3), 2).raw() == 2);
    CHECK(div_int(raw(-3), 2).raw() == -2);
    CHECK(div_int(raw(3), -2).raw() == -2);
    CHECK_FATAL(div_int(Fx::ONE, 0));
    CHECK(min(raw(1), raw(2)).raw() == 1);
    CHECK(max(raw(1), raw(2)).raw() == 2);
    CHECK(clamp(raw(5), raw(0), raw(10)).raw() == 5);
    CHECK(clamp(raw(-5), raw(0), raw(10)).raw() == 0);
    CHECK(clamp(raw(15), raw(0), raw(10)).raw() == 10);
    CHECK(clamp(raw(15), raw(3), raw(3)).raw() == 3);
    CHECK_FATAL(clamp(raw(0), raw(1), raw(0)));
}

TEST_CASE("P0-01 13.1 / 5.5: fade") {
    CHECK(fade(Fx{}).raw() == 0);
    CHECK(fade(Fx::HALF).raw() == 2147483648);
    CHECK(fade(Fx::ONE).raw() == 4294967296);
    CHECK_FATAL(fade(raw(-1)));
    CHECK_FATAL(fade(raw(ONE_RAW + 1)));
    // The final clamp keeps every result in [0, ONE] (5.5).
    for (std::int64_t i = 0; i <= 4096; ++i) {
        const std::int64_t f = fade(raw(i * (ONE_RAW / 4096))).raw();
        REQUIRE(f >= 0);
        REQUIRE(f <= ONE_RAW);
    }
    // Odd symmetry about HALF is not specified, so it is not tested.
}

TEST_CASE("P0-01 13.1 / 5.4: lerp") {
    CHECK(lerp(fxi(2), fxi(4), Fx::HALF) == fxi(3));
    CHECK(lerp(fxi(2), fxi(4), Fx{}) == fxi(2));
    CHECK(lerp(fxi(2), fxi(4), Fx::ONE) == fxi(4));
    CHECK(lerp(fxi(2), fxi(4), fxi(2)) == fxi(6));  // t is not range-checked
    CHECK_FATAL(lerp(Fx::MIN, Fx::MAX, Fx{}));     // b - a overflows
}

TEST_CASE("P0-01 13.1: a + b overflow is fatal, sat_add saturates") {
    CHECK_FATAL(Fx::MAX + raw(1));
    CHECK(sat_add(Fx::MAX, raw(1)) == Fx::MAX);
}

TEST_CASE("P0-01 11: Fx algebraic invariants over a fixed stream") {
    Rng rng(KEY_FX_PROPS);
    const std::int64_t span47 = std::int64_t{1} << 47;
    for (int i = 0; i < 20000; ++i) {
        const std::int64_t ar = rng.uniform_i64(-span47, span47);
        const std::int64_t br = rng.uniform_i64(-span47, span47);
        const Fx a = raw(ar);
        const Fx b = raw(br);
        REQUIRE(mul(a, b) == mul(b, a));
        REQUIRE(mul(-a, b) == -mul(a, b));
        REQUIRE(mul(a, Fx::ONE) == a);
        REQUIRE(div(a, Fx::ONE) == a);
        if (br != 0) {
            REQUIRE(div(-a, b) == -div(a, b));
        }
        const std::int64_t small = rng.uniform_i64(-(std::int64_t{1} << 40), std::int64_t{1} << 40);
        std::int64_t n = rng.uniform_i64(-(std::int64_t{1} << 20), std::int64_t{1} << 20);
        if (n == 0) {
            n = 1;
        }
        REQUIRE(div(mul_int(raw(small), n), Fx::from_int(n)) == raw(small));
    }
}
