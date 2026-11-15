#pragma once

// Umbrella header for the deterministic core (spec P0-01).

#include "worldgen/core/checked.hpp"   // checked_add/sub/mul, floor_div, floor_mod, mul_div (4, 5.4)
#include "worldgen/core/decimal.hpp"   // parse_decimal, canonical_decimal (5.6)
#include "worldgen/core/error.hpp"     // FatalError (4.1)
#include "worldgen/core/fx.hpp"        // Fx, arithmetic, fade, to_decimal (5)
#include "worldgen/core/hash.hpp"      // mix64, hash_words, stream_key, hash_at, phrase_seed (7)
#include "worldgen/core/math.hpp"      // isqrt_u64, isqrt_u128, sqrt, hypot (9.1)
#include "worldgen/core/noise.hpp"     // noise2, fbm2 (8)
#include "worldgen/core/rng.hpp"       // splitmix64_next, Rng (6)
#include "worldgen/core/sha256.hpp"    // Sha256, sha256, to_hex (10)
#include "worldgen/core/streams.hpp"   // stream-name registry (7.3)
#include "worldgen/core/wide.hpp"      // U128, mul_u64_wide, divmod_u128_u64 (5.3)
