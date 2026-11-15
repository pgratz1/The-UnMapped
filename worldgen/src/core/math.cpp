// Integer and fixed-point square roots (P0-01 9.1).

#include "worldgen/core/math.hpp"

#include "core/int_detail.hpp"
#include "worldgen/core/error.hpp"

namespace worldgen {

namespace {

// r rounded to nearest given N and r = floor(sqrt(N)): r + 1 if N - r^2 > r.
// Ties cannot occur because (r + 1/2)^2 is never an integer.
U128 round_sqrt(U128 n, std::uint64_t r) noexcept {
    const U128 excess = n - mul_u64_wide(r, r);  // N - r^2 >= 0
    U128 out{0, r};
    if (excess > U128{0, r}) {
        out = out + U128{0, 1};
    }
    return out;
}

}  // namespace

std::uint64_t isqrt_u64(std::uint64_t n) noexcept {
    std::uint64_t res = 0;
    std::uint64_t bit = std::uint64_t{1} << 62U;
    while (bit > n) {
        bit >>= 2U;
    }
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1U) + bit;
        } else {
            res >>= 1U;
        }
        bit >>= 2U;
    }
    return res;
}

std::uint64_t isqrt_u128(U128 n) noexcept {
    U128 res{};
    U128 bit{std::uint64_t{1} << 62U, 0};  // 2^126
    while (bit > n) {
        bit = bit >> 2U;
    }
    const U128 zero{};
    while (bit != zero) {
        const U128 trial = res + bit;
        if (n >= trial) {
            n = n - trial;
            res = (res >> 1U) + bit;
        } else {
            res = res >> 1U;
        }
        bit = bit >> 2U;
    }
    return res.lo;  // floor(sqrt(n)) < 2^64, so hi is 0
}

Fx sqrt(Fx x) {
    if (x.raw() < 0) {
        fatal("sqrt: negative argument");
    }
    const U128 n = U128{0, static_cast<std::uint64_t>(x.raw())} << 32U;
    const std::uint64_t r = isqrt_u128(n);
    const U128 rounded = round_sqrt(n, r);
    // r <= sqrt(2^95) < 2^48: always in range.
    return Fx::from_raw(static_cast<std::int64_t>(rounded.lo));
}

Fx hypot(Fx a, Fx b) {
    const std::uint64_t ma = detail::magnitude(a.raw());
    const std::uint64_t mb = detail::magnitude(b.raw());
    const U128 n = mul_u64_wide(ma, ma) + mul_u64_wide(mb, mb);  // < 2^127
    const std::uint64_t r = isqrt_u128(n);
    const U128 rounded = round_sqrt(n, r);
    return Fx::from_raw(detail::signed_from_magnitude(false, rounded, "hypot: result out of range"));
}

}  // namespace worldgen
