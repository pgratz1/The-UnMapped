// P0-01 section 15 / P1-00 appendix A1 (a), (b): the constants SQRT2 and
// INV_SQRT2 and the integer helper bit_length.
//
// Written from docs/specs/p1-00-geography-overview.md (appendix A1, section
// 12) and docs/specs/p0-01-deterministic-core.md section 15, and the public
// headers only.

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>

#include "core_test_util.hpp"
#include "worldgen/core/core.hpp"

using worldgen::Fx;
using worldgen_test::U64_MAX;

TEST_CASE("P0-01 15 / P1-00 A1 (b): bit_length of the listed values") {
    // The spec's list: 0, 1, 2, 3, 2^63, 2^64 - 1 -> 0, 1, 2, 2, 64, 64.
    CHECK(worldgen::bit_length(0U) == 0);
    CHECK(worldgen::bit_length(1U) == 1);
    CHECK(worldgen::bit_length(2U) == 2);
    CHECK(worldgen::bit_length(3U) == 2);
    CHECK(worldgen::bit_length(std::uint64_t{1} << 63U) == 64);
    CHECK(worldgen::bit_length(U64_MAX) == 64);
}

TEST_CASE("P0-01 15 / P1-00 A1 (b): bit_length is floor(log2 v) + 1 at every power of two and its neighbours") {
    for (unsigned k = 0; k < 64; ++k) {
        CAPTURE(k);
        const std::uint64_t p = std::uint64_t{1} << k;
        CHECK(worldgen::bit_length(p) == static_cast<std::int32_t>(k) + 1);
        // 2^(k+1) - 1 has k + 1 significant bits as well.
        const std::uint64_t below_next = (k == 63U) ? U64_MAX : (std::uint64_t{1} << (k + 1U)) - 1U;
        CHECK(worldgen::bit_length(below_next) == static_cast<std::int32_t>(k) + 1);
        if (k >= 1U) {
            CHECK(worldgen::bit_length(p - 1U) == static_cast<std::int32_t>(k));
        }
    }
}

TEST_CASE("P0-01 15 / P1-00 A1 (b): bit_length agrees with a shift-count reference on seeded values") {
    worldgen::Rng rng(worldgen_test::KEY_HASH_PROPS ^ 0xB17B17ULL);
    for (int i = 0; i < 2000; ++i) {
        const std::uint64_t r = rng.next();
        const std::uint64_t shift_draw = rng.next();
        const std::uint64_t v = r >> (shift_draw % 64U);  // spread over every bit length
        std::int32_t expected = 0;
        for (std::uint64_t t = v; t != 0U; t >>= 1U) {
            ++expected;
        }
        CAPTURE(v);
        CHECK(worldgen::bit_length(v) == expected);
    }
}

TEST_CASE("P1-02 H6: river classes from bit_length (worked example list, T = 3)") {
    // class = min(15, bit_length(A div T)), P1-02 5.1 last paragraph.
    const auto river_class = [](std::uint64_t a, std::uint64_t t) {
        const std::int32_t b = worldgen::bit_length(a / t);
        return b < 15 ? b : 15;
    };
    CHECK(river_class(3, 3) == 1);
    CHECK(river_class(5, 3) == 1);
    CHECK(river_class(6, 3) == 2);
    CHECK(river_class(11, 3) == 2);
    CHECK(river_class(12, 3) == 3);
    CHECK(river_class(3U * (1U << 14U), 3) == 15);
    CHECK(river_class(3U * (1U << 20U), 3) == 15);
    CHECK(river_class(8, 3) == 2);   // 5.1: (3, 4), A 8
    CHECK(river_class(10, 3) == 2);  // 5.1: (4, 5), A 10
}

TEST_CASE("P0-01 15 / P1-00 A1 (a): SQRT2 and INV_SQRT2 raw values") {
    CHECK(Fx::SQRT2.raw() == std::int64_t{6074001000});
    CHECK(Fx::INV_SQRT2.raw() == std::int64_t{3037000500});
}

TEST_CASE("P0-01 15 / P1-00 A1 (a): the raw values are round(sqrt(2) * 2^32) and round(2^32 / sqrt(2))") {
    // round(sqrt(2) * 2^32) = r with (r - 1/2)^2 < 2^65 < (r + 1/2)^2, i.e.
    // (2r - 1)^2 < 2^67 < (2r + 1)^2, checked exactly in U128.
    const auto sq = [](std::uint64_t v) { return worldgen::mul_u64_wide(v, v); };
    const worldgen::U128 two_67{std::uint64_t{1} << 3U, 0};  // 2^67
    const auto r = static_cast<std::uint64_t>(Fx::SQRT2.raw());
    CHECK(sq(2U * r - 1U) < two_67);
    CHECK(two_67 < sq(2U * r + 1U));
    // round(2^32 / sqrt(2)) = round(sqrt(2^63)): (2s - 1)^2 < 2^65 < (2s + 1)^2.
    const worldgen::U128 two_65{2, 0};
    const auto s = static_cast<std::uint64_t>(Fx::INV_SQRT2.raw());
    CHECK(sq(2U * s - 1U) < two_65);
    CHECK(two_65 < sq(2U * s + 1U));
    // And they are the rounded square roots P0-01 sqrt gives.
    CHECK(worldgen::sqrt(Fx::from_int(2)) == Fx::SQRT2);
    CHECK(worldgen::sqrt(Fx::HALF) == Fx::INV_SQRT2);
}

TEST_CASE("P0-01 15 / P1-00 A1 (a): mul(SQRT2, INV_SQRT2) is exactly ONE") {
    CHECK(worldgen::mul(Fx::SQRT2, Fx::INV_SQRT2) == Fx::ONE);
    CHECK(worldgen::mul(Fx::INV_SQRT2, Fx::SQRT2) == Fx::ONE);
    CHECK((Fx::SQRT2 * Fx::INV_SQRT2).raw() == worldgen_test::ONE_RAW);
}

TEST_CASE("P0-01 15 / P1-00 A1 (a): the constants are usable in constant expressions") {
    constexpr std::int64_t sqrt2_raw = Fx::SQRT2.raw();
    constexpr std::int64_t inv_raw = Fx::INV_SQRT2.raw();
    static_assert(sqrt2_raw == 6074001000);
    static_assert(inv_raw == 3037000500);
    constexpr std::int32_t bl = worldgen::bit_length(255U);
    static_assert(bl == 8);
    CHECK(sqrt2_raw == 2 * inv_raw);
}
