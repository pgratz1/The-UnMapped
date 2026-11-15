// The P0 test-pattern stage (spec P0-04 section 5.2).

#include "worldgen/stages/test_pattern.hpp"

#include <cstddef>
#include <vector>

#include "stages/stage_faults.hpp"
#include "worldgen/core/checked.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/noise.hpp"
#include "worldgen/core/streams.hpp"

namespace worldgen::stages {

namespace {

constexpr std::int64_t SIDE = static_cast<std::int64_t>(TEST_PATTERN_SIDE);

// fbm2 parameters of test.noise (P0-04 5.2).
constexpr std::int32_t NOISE_OCTAVES = 5;
constexpr std::int64_t NOISE_BASE_FREQ_RAW = 134217728;  // 1/32
constexpr std::int64_t NOISE_LACUNARITY = 2;
constexpr std::int64_t NOISE_SCALE = 32767;
constexpr std::int64_t FX_ONE_RAW = 4294967296;  // 2^32

std::size_t cell_index(std::int64_t x, std::int64_t y) noexcept {
    return static_cast<std::size_t>(y * SIDE + x);  // 0 <= x, y < 256
}

}  // namespace

db::LayerGrid test_ramp_grid() {
    db::LayerGrid grid = db::make_layer_grid(db::ElementType::u8, TEST_PATTERN_SIDE, TEST_PATTERN_SIDE);
    auto& cells = std::get<std::vector<std::uint8_t>>(grid.values);
    for (std::int64_t y = 0; y < SIDE; ++y) {
        for (std::int64_t x = 0; x < SIDE; ++x) {
            cells[cell_index(x, y)] = static_cast<std::uint8_t>((x + 2 * y) % 256);
        }
    }
    return grid;
}

db::LayerGrid test_hash_grid(std::uint64_t seed) {
    const std::uint64_t key = stream_key(seed, streams::test_hash);
    db::LayerGrid grid = db::make_layer_grid(db::ElementType::u8, TEST_PATTERN_SIDE, TEST_PATTERN_SIDE);
    auto& cells = std::get<std::vector<std::uint8_t>>(grid.values);
    for (std::int64_t y = 0; y < SIDE; ++y) {
        for (std::int64_t x = 0; x < SIDE; ++x) {
            cells[cell_index(x, y)] = static_cast<std::uint8_t>(hash_at(key, x, y) >> 56U);
        }
    }
    return grid;
}

db::LayerGrid test_noise_grid(std::uint64_t seed) {
    const std::uint64_t key = stream_key(seed, streams::test_noise);
    const Fx base_freq = Fx::from_raw(NOISE_BASE_FREQ_RAW);
    const Fx lacunarity = Fx::from_int(NOISE_LACUNARITY);
    db::LayerGrid grid = db::make_layer_grid(db::ElementType::i16, TEST_PATTERN_SIDE, TEST_PATTERN_SIDE);
    auto& cells = std::get<std::vector<std::int16_t>>(grid.values);
    for (std::int64_t y = 0; y < SIDE; ++y) {
        const Fx fy = Fx::from_int(y) + Fx::HALF;
        for (std::int64_t x = 0; x < SIDE; ++x) {
            const Fx fx = Fx::from_int(x) + Fx::HALF;
            const Fx n = fbm2(key, fx, fy, NOISE_OCTAVES, base_freq, lacunarity, Fx::HALF);
            const std::int64_t v = mul_div(n.raw(), NOISE_SCALE, FX_ONE_RAW);
            if (v < -NOISE_SCALE || v > NOISE_SCALE) {
                fatal("test_pattern: test.noise value outside [-32767, 32767]");
            }
            cells[cell_index(x, y)] = static_cast<std::int16_t>(v);
        }
    }
    return grid;
}

namespace detail {

void run_test_pattern(db::WorldWriter& world, StageFault fault) {
    const std::uint64_t seed = world.seed();
    const std::int64_t first_id = world.next_layer_id();
    db::StageWriter stage = world.begin_stage(TEST_PATTERN_STAGE);
    stage.write_layer(first_id, TEST_RAMP_LAYER, test_ramp_grid(), db::Codec::delta_varint);
    if (fault == StageFault::fatal_inside_test_pattern) {
        // Test hook (P0-04 12.2): fail with the stage transaction open and
        // one layer written. The StageWriter destructor rolls it back.
        fatal("test hook: forced fatal error inside stage test_pattern");
    }
    stage.write_layer(first_id + 1, TEST_HASH_LAYER, test_hash_grid(seed), db::Codec::delta_varint);
    stage.write_layer(first_id + 2, TEST_NOISE_LAYER, test_noise_grid(seed), db::Codec::delta_varint);
    stage.commit();
}

}  // namespace detail

void run_test_pattern(db::WorldWriter& world) {
    detail::run_test_pattern(world, StageFault::none);
}

}  // namespace worldgen::stages
