// Checked plain-integer helpers (P0-01 4, 5.4).

#include "worldgen/core/checked.hpp"

#include <limits>

#include "core/int_detail.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/wide.hpp"

namespace worldgen {

namespace {
constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
}  // namespace

std::int64_t checked_add(std::int64_t a, std::int64_t b) {
    if ((b > 0 && a > kMax - b) || (b < 0 && a < kMin - b)) {
        fatal("checked_add: int64 overflow");
    }
    return a + b;
}

std::int64_t checked_sub(std::int64_t a, std::int64_t b) {
    if ((b < 0 && a > kMax + b) || (b > 0 && a < kMin + b)) {
        fatal("checked_sub: int64 overflow");
    }
    return a - b;
}

std::int64_t checked_mul(std::int64_t a, std::int64_t b) {
    const bool negative = (a < 0) != (b < 0);
    const U128 p = mul_u64_wide(detail::magnitude(a), detail::magnitude(b));
    // Largest magnitude allowed: 2^63 for a negative result, 2^63 - 1 else.
    const std::uint64_t limit = negative ? detail::kInt64MaxU + 1U : detail::kInt64MaxU;
    if (p.hi != 0 || p.lo > limit) {
        fatal("checked_mul: int64 overflow");
    }
    // Reinterpret mod 2^64; for a negative result 0 - p.lo is the two's
    // complement pattern, also for p.lo = 2^63 (gives INT64_MIN).
    const std::uint64_t bits = negative ? std::uint64_t{0} - p.lo : p.lo;
    return static_cast<std::int64_t>(bits);
}

std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    if (b == 0) {
        fatal("floor_div: division by zero");
    }
    if (a == kMin && b == -1) {
        fatal("floor_div: int64 overflow");
    }
    std::int64_t q = a / b;
    const std::int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        q -= 1;
    }
    return q;
}

std::int64_t floor_mod(std::int64_t a, std::int64_t b) {
    if (b == 0) {
        fatal("floor_mod: division by zero");
    }
    if (b == -1) {
        return 0;  // avoids INT64_MIN % -1, which is undefined
    }
    std::int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        r += b;  // |r| < |b| and opposite signs: no overflow
    }
    return r;
}

std::int64_t mul_div(std::int64_t a, std::int64_t b, std::int64_t c) {
    if (c == 0) {
        fatal("mul_div: division by zero");
    }
    const bool negative = ((a < 0) != (b < 0)) != (c < 0);
    const U128 p = mul_u64_wide(detail::magnitude(a), detail::magnitude(b));
    const U128 q = detail::rha_divide(p, detail::magnitude(c));
    if (q.hi == 0 && q.lo == 0) {
        return 0;
    }
    return detail::signed_from_magnitude(negative, q, "mul_div: result out of range");
}

}  // namespace worldgen
