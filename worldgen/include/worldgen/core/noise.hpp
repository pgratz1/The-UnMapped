#pragma once

// Integer gradient noise (spec P0-01 section 8).

#include <cstdint>

#include "worldgen/core/fx.hpp"

namespace worldgen {

// 2D gradient noise on the integer lattice, in [-ONE, ONE] (P0-01 8.1).
// Lattice gradients come from hash_at(key, xi, yi) >> 61 and the improved
// Perlin 2D table; the fade is fade() (5.5). Exactly 0 at lattice points.
[[nodiscard]] Fx noise2(std::uint64_t key, Fx x, Fx y);

// Fractal sum of noise2 octaves, in [-ONE, ONE] (P0-01 8.2). Octave o uses
// key substream(key, o) and frequency base_freq * lacunarity^o (with RHA at
// each mul). Fatal unless 1 <= octaves <= 16, base_freq > 0,
// lacunarity > 0 and 0 < gain <= ONE; fatal also if mul(x, f) or any other
// step overflows.
[[nodiscard]] Fx fbm2(std::uint64_t key, Fx x, Fx y, std::int32_t octaves, Fx base_freq, Fx lacunarity,
                      Fx gain);

}  // namespace worldgen
