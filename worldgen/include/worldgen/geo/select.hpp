#pragma once

// G1: k-th value and quantile (spec P1-00 section 7.1, rule D7).
//
// Results are values, independent of how ties are ordered.

#include <cstdint>
#include <span>
#include <vector>

#include "worldgen/core/fx.hpp"

namespace worldgen::geo {

// kth(values, k): the k-th smallest element (0-based) of the multiset
// `values` (P1-00 7.1). Fatal (FatalError) unless k < values.size(). The
// input is not modified (the function works on a copy).
[[nodiscard]] std::int64_t kth(std::span<const std::int64_t> values, std::uint64_t k);

// As kth, but works in the caller's array, saving the copy. After the call
// `values` holds the same multiset in an unspecified arrangement that
// differs between standard libraries (and so between platforms): it may be
// used only as a multiset, never by position (P1-00 appendix C item 11). A
// caller that needs the array afterwards in a defined order sorts it with
// a strict total order, or uses kth.
[[nodiscard]] std::int64_t kth_in_place(std::span<std::int64_t> values, std::uint64_t k);

// kth(values, ks[j]) for every j, returned in the order of ks (any order,
// repeats allowed); one copy of values is made. Fatal unless every
// ks[j] < values.size().
[[nodiscard]] std::vector<std::int64_t> kth_many(std::span<const std::int64_t> values,
                                                 std::span<const std::uint64_t> ks);

// The rank of quantile q in a multiset of n values (P1-00 7.1):
// k = min(n - 1, (q.raw * n) >> 32), in uint64. Fatal if n = 0, if
// n > 2^31, or unless 0 <= q <= ONE.
[[nodiscard]] std::uint64_t quantile_rank(Fx q, std::uint64_t n);

// quantile(values, q) = kth(values, quantile_rank(q, n)) (P1-00 7.1). Fatal
// if values is empty or q is outside [0, ONE].
[[nodiscard]] std::int64_t quantile(std::span<const std::int64_t> values, Fx q);

}  // namespace worldgen::geo
