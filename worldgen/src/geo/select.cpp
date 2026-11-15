// G1: k-th value and quantile (P1-00 section 7.1).

#include "worldgen/geo/select.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

#include "worldgen/core/error.hpp"

namespace worldgen::geo {

namespace {

// Places the k-th smallest of v[first, end) at index k, with every element
// before it no greater and every element after it no smaller. The value at
// k is the k-th order statistic, which does not depend on how equal
// elements are arranged.
void select_at(std::span<std::int64_t> v, std::size_t first, std::size_t k) {
    const auto begin = v.begin() + static_cast<std::ptrdiff_t>(first);
    const auto nth = v.begin() + static_cast<std::ptrdiff_t>(k);
    // determinism-lint: allow(unstable-sort) -- only the value at nth is used (G1 is a value, P1-00 7.1); equal int64 elements are indistinguishable, so their arrangement cannot reach a result
    std::nth_element(begin, nth, v.end());
}

}  // namespace

std::int64_t kth_in_place(std::span<std::int64_t> values, std::uint64_t k) {
    if (k >= static_cast<std::uint64_t>(values.size())) {
        fatal("kth: k out of range (k must be below the number of values)");
    }
    const auto at = static_cast<std::size_t>(k);
    select_at(values, 0, at);
    return values[at];
}

std::int64_t kth(std::span<const std::int64_t> values, std::uint64_t k) {
    if (k >= static_cast<std::uint64_t>(values.size())) {
        fatal("kth: k out of range (k must be below the number of values)");
    }
    std::vector<std::int64_t> copy(values.begin(), values.end());
    return kth_in_place(copy, k);
}

std::vector<std::int64_t> kth_many(std::span<const std::int64_t> values, std::span<const std::uint64_t> ks) {
    const auto n = static_cast<std::uint64_t>(values.size());
    for (const std::uint64_t k : ks) {
        if (k >= n) {
            fatal("kth_many: k out of range (k must be below the number of values)");
        }
    }
    // Process the ranks in ascending order; each selection only needs the
    // part of the array above the previous rank.
    std::vector<std::size_t> by_rank(ks.size());
    std::iota(by_rank.begin(), by_rank.end(), std::size_t{0});
    std::stable_sort(by_rank.begin(), by_rank.end(), [&ks](std::size_t a, std::size_t b) { return ks[a] < ks[b]; });

    std::vector<std::int64_t> copy(values.begin(), values.end());
    std::vector<std::int64_t> out(ks.size());
    std::size_t first = 0;      // copy[first, end) holds the values not yet placed
    bool have_previous = false;
    std::uint64_t previous = 0;
    for (const std::size_t j : by_rank) {
        const std::uint64_t k = ks[j];
        if (!have_previous || k != previous) {
            select_at(copy, first, static_cast<std::size_t>(k));
            first = static_cast<std::size_t>(k) + 1U;
            previous = k;
            have_previous = true;
        }
        out[j] = copy[static_cast<std::size_t>(k)];
    }
    return out;
}

std::uint64_t quantile_rank(Fx q, std::uint64_t n) {
    if (n == 0U) {
        fatal("quantile: no values");
    }
    if (n > (std::uint64_t{1} << 31U)) {
        fatal("quantile: more than 2^31 values");
    }
    if (q.raw() < 0 || q.raw() > Fx::ONE.raw()) {
        fatal("quantile: q outside [0, ONE]");
    }
    // q.raw <= 2^32 and n <= 2^31: the product is at most 2^63.
    const std::uint64_t k = (static_cast<std::uint64_t>(q.raw()) * n) >> 32U;
    return std::min(n - 1U, k);
}

std::int64_t quantile(std::span<const std::int64_t> values, Fx q) {
    const std::uint64_t k = quantile_rank(q, static_cast<std::uint64_t>(values.size()));
    return kth(values, k);
}

}  // namespace worldgen::geo
