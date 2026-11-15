// Noise compositions shared by the geography stages (P1-01 section 4.1).

#include "worldgen/geo/noise.hpp"

#include "worldgen/core/error.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/noise.hpp"

namespace worldgen::geo {

Fx lacunarity() {
    return Fx::from_int(LACUNARITY);
}

Fx fbm_map(std::uint64_t key, Fx a, Fx b, const FbmMap& rec) {
    return fbm2(key, a, b, rec.octaves, rec.cycles, lacunarity(), rec.gain);
}

Fx fbm_m(std::uint64_t key, Fx a, Fx b, const FbmM& rec) {
    return fbm2(key, a, b, rec.octaves, Fx::from_ratio(1, rec.wavelength_m), lacunarity(), rec.gain);
}

Fx ridged_m(std::uint64_t key, Fx a, Fx b, const FbmM& rec, Fx weight) {
    if (rec.octaves < 1 || rec.octaves > 16) {
        fatal("ridged_m: octaves outside [1, 16]");
    }
    const Fx lac = lacunarity();
    const Fx zero;
    Fx f = Fx::from_ratio(1, rec.wavelength_m);
    Fx amp = Fx::ONE;
    Fx wgt = Fx::ONE;
    Fx sum;
    Fx norm;
    for (std::int32_t o = 0; o < rec.octaves; ++o) {
        const std::uint64_t ko = substream(key, static_cast<std::uint64_t>(o));
        const Fx xs = mul(a, f);
        const Fx ys = mul(b, f);
        const Fx n = noise2(ko, xs, ys);
        Fx s = Fx::ONE - abs(n);
        s = mul(s, s);
        s = mul(s, wgt);
        wgt = clamp(mul(s, weight), zero, Fx::ONE);
        sum = sum + mul(s, amp);
        norm = norm + amp;
        amp = mul(amp, rec.gain);
        f = mul(f, lac);
    }
    return clamp(div(sum, norm), zero, Fx::ONE);
}

}  // namespace worldgen::geo
