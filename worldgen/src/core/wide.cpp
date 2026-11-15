// Portable 128-bit arithmetic (P0-01 5.3). Reference methods only.

#include "worldgen/core/wide.hpp"

#include "worldgen/core/error.hpp"

namespace worldgen {

namespace {
constexpr std::uint64_t kLow32 = 0xFFFFFFFFULL;
}  // namespace

U128 mul_u64_wide(std::uint64_t a, std::uint64_t b) noexcept {
    const std::uint64_t a_lo = a & kLow32;
    const std::uint64_t a_hi = a >> 32;
    const std::uint64_t b_lo = b & kLow32;
    const std::uint64_t b_hi = b >> 32;

    const std::uint64_t p_ll = a_lo * b_lo;
    const std::uint64_t p_lh = a_lo * b_hi;
    const std::uint64_t p_hl = a_hi * b_lo;
    const std::uint64_t p_hh = a_hi * b_hi;

    // Middle column: at most (2^32-1) + 2 (2^32-1) < 2^34, no overflow.
    const std::uint64_t mid = (p_ll >> 32) + (p_lh & kLow32) + (p_hl & kLow32);

    U128 r;
    r.lo = (mid << 32) | (p_ll & kLow32);
    r.hi = p_hh + (p_lh >> 32) + (p_hl >> 32) + (mid >> 32);
    return r;
}

U128DivMod divmod_u128_u64(U128 n, std::uint64_t d) {
    if (d == 0) {
        fatal("divmod_u128_u64: division by zero");
    }
    U128DivMod out;
    // High word: ordinary 64-bit division.
    out.quotient.hi = n.hi / d;
    std::uint64_t rem = n.hi % d;  // rem < d

    // Low word: restoring shift-subtract, one bit at a time. The running
    // remainder is (carry * 2^64 + rem) and is always < 2d before the
    // subtraction, so subtracting d once is enough.
    std::uint64_t q_lo = 0;
    for (int i = 63; i >= 0; --i) {
        const std::uint64_t carry = rem >> 63;
        rem = (rem << 1) | ((n.lo >> static_cast<unsigned int>(i)) & 1U);
        if (carry != 0 || rem >= d) {
            rem -= d;  // exact mod 2^64: the true value minus d is < d
            q_lo |= std::uint64_t{1} << static_cast<unsigned int>(i);
        }
    }
    out.quotient.lo = q_lo;
    out.remainder = rem;
    return out;
}

U128 operator+(U128 a, U128 b) noexcept {
    U128 r;
    r.lo = a.lo + b.lo;
    const std::uint64_t carry = r.lo < a.lo ? 1U : 0U;
    r.hi = a.hi + b.hi + carry;
    return r;
}

U128 operator-(U128 a, U128 b) noexcept {
    U128 r;
    r.lo = a.lo - b.lo;
    const std::uint64_t borrow = a.lo < b.lo ? 1U : 0U;
    r.hi = a.hi - b.hi - borrow;
    return r;
}

U128 operator<<(U128 a, unsigned int count) {
    if (count > 127U) {
        fatal("U128 shift count out of range");
    }
    if (count == 0U) {
        return a;
    }
    U128 r;
    if (count >= 64U) {
        r.hi = a.lo << (count - 64U);
        r.lo = 0;
    } else {
        r.hi = (a.hi << count) | (a.lo >> (64U - count));
        r.lo = a.lo << count;
    }
    return r;
}

U128 operator>>(U128 a, unsigned int count) {
    if (count > 127U) {
        fatal("U128 shift count out of range");
    }
    if (count == 0U) {
        return a;
    }
    U128 r;
    if (count >= 64U) {
        r.lo = a.hi >> (count - 64U);
        r.hi = 0;
    } else {
        r.lo = (a.lo >> count) | (a.hi << (64U - count));
        r.hi = a.hi >> count;
    }
    return r;
}

}  // namespace worldgen
