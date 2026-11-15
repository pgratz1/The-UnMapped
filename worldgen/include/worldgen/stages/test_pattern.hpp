#pragma once

// The P0 test-pattern stage (spec P0-04 section 5.2).
//
// Writes three 256 x 256 layers, codec 1, in one stage transaction, so that
// layer writing, the codec, location hashing, streams and noise are proved
// identical across platforms in P0. Every cell value is location-keyed, so
// the grids are pure functions of the seed.

#include <cstdint>
#include <string_view>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/writer.hpp"

namespace worldgen::stages {

// The stage name recorded in meta.stages (P0-04 5.2).
inline constexpr std::string_view TEST_PATTERN_STAGE = "test_pattern";

// Side of every test-pattern layer, regardless of param.map_size (P0-04 5.2).
inline constexpr std::uint32_t TEST_PATTERN_SIDE = 256U;

// Layer names, in writing order (ids 1, 2, 3 in a world where this stage
// runs first) (P0-04 5.2).
inline constexpr std::string_view TEST_RAMP_LAYER = "test.ramp";
inline constexpr std::string_view TEST_HASH_LAYER = "test.hash";
inline constexpr std::string_view TEST_NOISE_LAYER = "test.noise";

// test.ramp, u8: cell (x, y) = (x + 2y) mod 256 (P0-04 5.2).
[[nodiscard]] db::LayerGrid test_ramp_grid();

// test.hash, u8: cell (x, y) = hash_at(stream_key(seed, "test.hash"), x, y)
// >> 56 (P0-04 5.2).
[[nodiscard]] db::LayerGrid test_hash_grid(std::uint64_t seed);

// test.noise, i16: cell (x, y) = mul_div(n.raw, 32767, 2^32) with
// n = fbm2(stream_key(seed, "test.noise"), from_int(x) + HALF,
// from_int(y) + HALF, 5 octaves, base_freq 1/32 (raw 134217728),
// lacunarity 2, gain HALF), in [-32767, 32767] (P0-04 5.2).
[[nodiscard]] db::LayerGrid test_noise_grid(std::uint64_t seed);

// The stage entry point (P0-04 5.1 step 3, 5.2): begin_stage("test_pattern")
// on the world, write test.ramp, test.hash, test.noise with codec 1 at ids
// next_layer_id(), +1, +2, and commit. The seed is world.seed(). Raises
// FatalError on any contract violation (P0-01 4.1); the stage is then rolled
// back and not recorded.
void run_test_pattern(db::WorldWriter& world);

}  // namespace worldgen::stages
