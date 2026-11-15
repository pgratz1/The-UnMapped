// G1: k-th value and quantile (spec P1-00 section 7.1, rule D7).
//
// Written from docs/specs/p1-00-geography-overview.md and the public header
// worldgen/geo/select.hpp only. The reference is a sorted copy
// (std::sort of int64 values: a value-only result, so the sort's treatment
// of ties cannot matter).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

#include "core_test_util.hpp"
#include "worldgen/geo/select.hpp"

namespace geo = worldgen::geo;
using worldgen::Fx;
using worldgen_test::I64_MAX;
using worldgen_test::I64_MIN;
using worldgen_test::ONE_RAW;

namespace {

constexpr std::uint64_t KEY_G1 = 0x5445535447314B31ULL;

std::vector<std::int64_t> worked_example() { return {5, 3, 9, 1, 7, 3, 8, 2}; }

// A seeded multiset with heavy ties: values drawn from a small alphabet that
// includes the int64 extremes.
std::vector<std::int64_t> tied_multiset(worldgen::Rng& rng, std::size_t n, std::uint64_t alphabet) {
    std::vector<std::int64_t> v(n);
    for (auto& e : v) {
        const std::uint64_t r = rng.uniform_u64(alphabet + 3U);
        if (r == alphabet) {
            e = I64_MIN;
        } else if (r == alphabet + 1U) {
            e = I64_MAX;
        } else if (r == alphabet + 2U) {
            e = -static_cast<std::int64_t>(alphabet);
        } else {
            e = static_cast<std::int64_t>(r) - static_cast<std::int64_t>(alphabet / 2U);
        }
    }
    return v;
}

}  // namespace

TEST_CASE("G1 worked example (P1-00 7.1)") {
    const std::vector<std::int64_t> values = worked_example();
    CHECK(geo::quantile(values, Fx::from_raw(0)) == 1);
    CHECK(geo::quantile(values, Fx::HALF) == 5);
    CHECK(geo::quantile(values, Fx::ONE) == 9);
    CHECK(geo::quantile(values, Fx::from_raw(1803886264)) == 3);  // 0.42
    CHECK(worldgen::parse_decimal("0.42").value.raw() == 1803886264);
    CHECK(geo::quantile_rank(Fx::from_raw(0), 8) == 0U);
    CHECK(geo::quantile_rank(Fx::HALF, 8) == 4U);
    CHECK(geo::quantile_rank(Fx::ONE, 8) == 7U);  // min(7, 8)
    CHECK(geo::quantile_rank(Fx::from_raw(1803886264), 8) == 3U);
    // Sorted 1, 2, 3, 3, 5, 7, 8, 9.
    const std::int64_t sorted[] = {1, 2, 3, 3, 5, 7, 8, 9};
    for (std::uint64_t k = 0; k < 8; ++k) {
        CAPTURE(k);
        CHECK(geo::kth(values, k) == sorted[k]);
    }
    // The input is not modified.
    CHECK(values == worked_example());
}

TEST_CASE("G1: kth equals a sorted copy over seeded multisets with heavy ties") {
    worldgen::Rng rng(KEY_G1);
    for (int trial = 0; trial < 300; ++trial) {
        const std::size_t n = 1U + static_cast<std::size_t>(rng.uniform_u64(trial < 200 ? 40U : 3000U));
        const std::uint64_t alphabet = 1U + rng.uniform_u64(trial % 3 == 0 ? 2U : 12U);
        const std::vector<std::int64_t> values = tied_multiset(rng, n, alphabet);
        std::vector<std::int64_t> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        CAPTURE(trial);
        CAPTURE(n);
        // Every k for small sets; a spread of k (including both ends) for large.
        std::vector<std::uint64_t> ks;
        if (n <= 40U) {
            for (std::uint64_t k = 0; k < n; ++k) {
                ks.push_back(k);
            }
        } else {
            ks = {0U, 1U, n / 2U, n - 2U, n - 1U};
            for (int j = 0; j < 20; ++j) {
                ks.push_back(rng.uniform_u64(n));
            }
        }
        for (const std::uint64_t k : ks) {
            CAPTURE(k);
            CHECK(geo::kth(values, k) == sorted[k]);
            std::vector<std::int64_t> scratch = values;
            CHECK(geo::kth_in_place(scratch, k) == sorted[k]);
            // kth_in_place may reorder but keeps the multiset.
            std::sort(scratch.begin(), scratch.end());
            CHECK(scratch == sorted);
        }
        const std::vector<std::int64_t> many = geo::kth_many(values, ks);
        REQUIRE(many.size() == ks.size());
        for (std::size_t j = 0; j < ks.size(); ++j) {
            CHECK(many[j] == sorted[ks[j]]);
        }
    }
}

TEST_CASE("G1: kth_many accepts any order and repeats") {
    const std::vector<std::int64_t> values = worked_example();
    const std::vector<std::uint64_t> ks = {7, 0, 3, 3, 2, 7, 4};
    CHECK(geo::kth_many(values, ks) == std::vector<std::int64_t>{9, 1, 3, 3, 3, 9, 5});
    CHECK(geo::kth_many(values, std::vector<std::uint64_t>{}).empty());
    CHECK(values == worked_example());
}

TEST_CASE("G1: all-equal and single-element multisets") {
    const std::vector<std::int64_t> same(1000, -42);
    for (const std::uint64_t k : {0U, 1U, 500U, 999U}) {
        CHECK(geo::kth(same, k) == -42);
    }
    const std::vector<std::int64_t> one = {I64_MIN};
    CHECK(geo::kth(one, 0) == I64_MIN);
    CHECK(geo::quantile(one, Fx::from_raw(0)) == I64_MIN);
    CHECK(geo::quantile(one, Fx::ONE) == I64_MIN);
}

TEST_CASE("G1: quantile_rank = min(n - 1, (q.raw * n) >> 32)") {
    worldgen::Rng rng(KEY_G1 + 1U);
    for (int trial = 0; trial < 2000; ++trial) {
        const std::uint64_t n = 1U + rng.uniform_u64(trial < 1000 ? 100U : (std::uint64_t{1} << 31U));
        const auto q = static_cast<std::int64_t>(rng.uniform_u64(static_cast<std::uint64_t>(ONE_RAW) + 1U));
        // (q * n) >> 32 without 128-bit types: q <= 2^32, n <= 2^31, so the
        // product is below 2^63 and fits uint64.
        const std::uint64_t prod = static_cast<std::uint64_t>(q) * n;
        const std::uint64_t k = std::min(n - 1U, prod >> 32U);
        CAPTURE(n);
        CAPTURE(q);
        CHECK(geo::quantile_rank(Fx::from_raw(q), n) == k);
    }
}

TEST_CASE("G1: quantile at 0, ONE and just either side of a rank boundary") {
    // n = 3: rank k starts at q.raw = ceil(k * 2^32 / 3).
    //   k = 1 at 1431655766 (1431655765 * 3 = 2^32 - 1 gives 0);
    //   k = 2 at 2863311531 (2863311530 * 3 = 2^33 - 2 gives 1).
    const std::vector<std::int64_t> v3 = {30, 10, 20};
    CHECK(geo::quantile(v3, Fx::from_raw(0)) == 10);
    CHECK(geo::quantile(v3, Fx::from_raw(1431655765)) == 10);
    CHECK(geo::quantile(v3, Fx::from_raw(1431655766)) == 20);
    CHECK(geo::quantile(v3, Fx::from_raw(2863311530)) == 20);
    CHECK(geo::quantile(v3, Fx::from_raw(2863311531)) == 30);
    CHECK(geo::quantile(v3, Fx::ONE) == 30);
    CHECK(geo::quantile(v3, Fx::from_raw(ONE_RAW - 1)) == 30);

    // n = 8, boundaries at multiples of 2^29.
    const std::vector<std::int64_t> v8 = worked_example();  // sorted 1 2 3 3 5 7 8 9
    const std::int64_t step = std::int64_t{1} << 29U;
    const std::int64_t sorted[] = {1, 2, 3, 3, 5, 7, 8, 9};
    for (std::int64_t k = 1; k < 8; ++k) {
        CAPTURE(k);
        CHECK(geo::quantile(v8, Fx::from_raw(k * step - 1)) == sorted[k - 1]);
        CHECK(geo::quantile(v8, Fx::from_raw(k * step)) == sorted[k]);
        CHECK(geo::quantile_rank(Fx::from_raw(k * step - 1), 8) == static_cast<std::uint64_t>(k - 1));
        CHECK(geo::quantile_rank(Fx::from_raw(k * step), 8) == static_cast<std::uint64_t>(k));
    }
    // ONE gives n, clamped to n - 1.
    CHECK(geo::quantile_rank(Fx::ONE, 1) == 0U);
    CHECK(geo::quantile_rank(Fx::ONE, std::uint64_t{1} << 31U) == (std::uint64_t{1} << 31U) - 1U);
    CHECK(geo::quantile_rank(Fx::from_raw(ONE_RAW - 1), std::uint64_t{1} << 31U) == (std::uint64_t{1} << 31U) - 1U);
}

TEST_CASE("G1: fatal preconditions") {
    const std::vector<std::int64_t> empty;
    const std::vector<std::int64_t> values = worked_example();
    CHECK_FATAL(geo::quantile(empty, Fx::HALF));
    CHECK_FATAL(geo::quantile(empty, Fx::from_raw(0)));
    CHECK_FATAL(geo::quantile_rank(Fx::HALF, 0));
    CHECK_FATAL(geo::quantile_rank(Fx::HALF, (std::uint64_t{1} << 31U) + 1U));
    CHECK_FATAL(geo::quantile(values, Fx::from_raw(-1)));
    CHECK_FATAL(geo::quantile(values, Fx::from_raw(ONE_RAW + 1)));
    CHECK_FATAL(geo::quantile_rank(Fx::from_raw(-1), 8));
    CHECK_FATAL(geo::quantile_rank(Fx::from_raw(ONE_RAW + 1), 8));
    CHECK_FATAL(geo::kth(empty, 0));
    CHECK_FATAL(geo::kth(values, 8));
    CHECK_FATAL(geo::kth(values, worldgen_test::U64_MAX));
    std::vector<std::int64_t> scratch = values;
    CHECK_FATAL(geo::kth_in_place(scratch, 8));
    const std::vector<std::uint64_t> bad = {0, 8};
    CHECK_FATAL(geo::kth_many(values, bad));
}
