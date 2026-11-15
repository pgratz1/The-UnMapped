#pragma once

// Noise records and noise compositions shared by the geography stages
// (spec P1-01 section 4.1; P1-00 section 6.2; P0-01 section 15 / P1-00
// appendix A1 (e)).
//
// fbm_map and fbm_m are fbm2 (P0-01 8.2) with the base frequency taken from
// a tuning record and the lacunarity fixed at 2; ridged_m is the
// Musgrave-style ridged multifractal of P1-01 4.1, composed from noise2.
// The TOML schemas of the two record types are in
// worldgen/geo/noise_schema.hpp.

#include <cstdint>

#include "worldgen/core/fx.hpp"

namespace worldgen::geo {

// LAC = from_int(2): the lacunarity of every geography noise, a constant of
// the code, not tuning (P1-01 4.1, P1-00 6.5).
inline constexpr std::int64_t LACUNARITY = 2;
// LAC as Fx (raw 2^33).
[[nodiscard]] Fx lacunarity();

// Record `fbm_map` (P1-01 4.1): noise specified in cycles per map side,
// evaluated at map-fraction coordinates. Schema bounds: cycles fx(0.125, 64),
// octaves int(1, 10), gain fx(0.0625, 1).
struct FbmMap {
    Fx cycles;                 // base frequency, cycles per map side
    std::int32_t octaves = 0;  // number of octaves
    Fx gain;                   // amplitude factor per octave

    friend bool operator==(const FbmMap&, const FbmMap&) = default;
};

// Record `fbm_m` (P1-01 4.1): noise specified by a base wavelength in
// metres, evaluated at metre coordinates. Schema bounds: wavelength_m
// int(64, 65536), octaves int(1, 10), gain fx(0.0625, 1).
struct FbmM {
    std::int64_t wavelength_m = 0;  // base wavelength, metres
    std::int32_t octaves = 0;
    Fx gain;

    friend bool operator==(const FbmM&, const FbmM&) = default;
};

// fbm_map(K, a, b, rec) = fbm2(K, a, b, rec.octaves, rec.cycles, LAC,
// rec.gain) (P1-01 4.1). Fatal (FatalError) where fbm2 is.
[[nodiscard]] Fx fbm_map(std::uint64_t key, Fx a, Fx b, const FbmMap& rec);

// fbm_m(K, a, b, rec) = fbm2(K, a, b, rec.octaves,
// from_ratio(1, rec.wavelength_m), LAC, rec.gain) (P1-01 4.1). Fatal where
// from_ratio or fbm2 is (for example wavelength_m = 0).
[[nodiscard]] Fx fbm_m(std::uint64_t key, Fx a, Fx b, const FbmM& rec);

// ridged_m(K, a, b, rec, weight) of P1-01 4.1, result in [0, ONE]:
//
//     f = from_ratio(1, rec.wavelength_m); amp = ONE; wgt = ONE; sum = 0; norm = 0
//     for o in 0 .. rec.octaves - 1:
//         n    = noise2(substream(K, o), mul(a, f), mul(b, f))
//         s    = ONE - abs(n)
//         s    = mul(s, s)
//         s    = mul(s, wgt)
//         wgt  = clamp(mul(s, weight), 0, ONE)
//         sum  = sum + mul(s, amp)
//         norm = norm + amp
//         amp  = mul(amp, rec.gain)
//         f    = mul(f, LAC)
//     return clamp(div(sum, norm), 0, ONE)
//
// Every statement evaluated as written, in this order, with P0-01 rounding;
// amp and f are updated after the last octave too (as fbm2 does), so an
// overflow there is fatal as well. Fatal if rec.octaves is outside [1, 16]
// (the range fbm2 accepts) or any step is fatal under P0-01.
[[nodiscard]] Fx ridged_m(std::uint64_t key, Fx a, Fx b, const FbmM& rec, Fx weight);

}  // namespace worldgen::geo
