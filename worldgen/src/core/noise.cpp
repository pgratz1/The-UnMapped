// Integer gradient noise (P0-01 8).

#include "worldgen/core/noise.hpp"

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"

namespace worldgen {

namespace {

constexpr std::int64_t kOneRaw = 4294967296;  // 2^32
constexpr std::int64_t kFracMask = 0xFFFFFFFF;

// dot(g, u, v) for the improved-Perlin 2D gradient table (P0-01 8.1).
// |u|, |v| < 2^32, so every sum is exact.
std::int64_t gradient_dot(std::uint64_t g, std::int64_t u, std::int64_t v) noexcept {
    switch (g) {
        case 0: return u + v;
        case 1: return -u + v;
        case 2: return u - v;
        case 3: return -u - v;
        case 4: return u;
        case 5: return -u;
        case 6: return v;
        default: return -v;  // g = 7 (g is the top 3 bits, so 0..7)
    }
}

Fx corner(std::uint64_t key, std::int64_t xi, std::int64_t yi, std::int64_t fx, std::int64_t fy,
          std::int64_t dx, std::int64_t dy) noexcept {
    const std::int64_t u = fx - dx * kOneRaw;
    const std::int64_t v = fy - dy * kOneRaw;
    const std::uint64_t g = hash_at(key, xi + dx, yi + dy) >> 61U;
    return Fx::from_raw(gradient_dot(g, u, v));
}

}  // namespace

Fx noise2(std::uint64_t key, Fx x, Fx y) {
    const std::int64_t xi = x.raw() >> 32;
    const std::int64_t yi = y.raw() >> 32;
    const std::int64_t fx = x.raw() & kFracMask;
    const std::int64_t fy = y.raw() & kFracMask;

    const Fx d00 = corner(key, xi, yi, fx, fy, 0, 0);
    const Fx d10 = corner(key, xi, yi, fx, fy, 1, 0);
    const Fx d01 = corner(key, xi, yi, fx, fy, 0, 1);
    const Fx d11 = corner(key, xi, yi, fx, fy, 1, 1);

    const Fx su = fade(Fx::from_raw(fx));
    const Fx sv = fade(Fx::from_raw(fy));
    const Fx nx0 = lerp(d00, d10, su);
    const Fx nx1 = lerp(d01, d11, su);
    const Fx n = lerp(nx0, nx1, sv);
    return clamp(n, -Fx::ONE, Fx::ONE);
}

Fx fbm2(std::uint64_t key, Fx x, Fx y, std::int32_t octaves, Fx base_freq, Fx lacunarity, Fx gain) {
    if (octaves < 1 || octaves > 16) {
        fatal("fbm2: octaves outside [1, 16]");
    }
    if (base_freq.raw() <= 0) {
        fatal("fbm2: base_freq must be > 0");
    }
    if (lacunarity.raw() <= 0) {
        fatal("fbm2: lacunarity must be > 0");
    }
    if (gain.raw() <= 0 || gain.raw() > Fx::ONE.raw()) {
        fatal("fbm2: gain outside (0, ONE]");
    }
    Fx sum;
    Fx norm;
    Fx amp = Fx::ONE;
    Fx f = base_freq;
    for (std::int32_t o = 0; o < octaves; ++o) {
        const std::uint64_t ko = substream(key, static_cast<std::uint64_t>(o));
        const Fx n = noise2(ko, mul(x, f), mul(y, f));
        sum = sum + mul(amp, n);
        norm = norm + amp;
        amp = mul(amp, gain);
        f = mul(f, lacunarity);
    }
    return clamp(div(sum, norm), -Fx::ONE, Fx::ONE);
}

}  // namespace worldgen
