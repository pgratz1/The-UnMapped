#pragma once

// Vegetation of the 3D terrain preview (spec V3D-02 section 6).
//
// Plain C++, no Godot types: the five placeholder kinds, their densities
// per ground type, and the deterministic placement from the world seed and
// cell coordinates, with the instance cap. TerrainPreview hands the result
// to Terrain3D's instancer; the unit tests (gdext/tests/vegetation_test.cpp)
// use this file without Godot.

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "terrain/ground.hpp"
#include "terrain/terrain_data.hpp"

namespace unmapped::terrain {

inline constexpr int VEGETATION_KINDS = 5;  // mesh ids 0 broadleaf, 1 conifer, 2 shrub, 3 reed, 4 snag
inline constexpr std::uint64_t DEFAULT_INSTANCE_CAP = 2000000;
inline constexpr std::uint64_t MAX_INSTANCE_CAP = 4000000;

struct VegetationKindInfo {
    const char* name;
    int lods;
    float lod0_range;  // m
    float lod1_range;  // m; 0 with one LOD
    int scale_min_pm;  // per mille
    int scale_max_pm;
};

// Table 6.1, indexed by mesh id.
extern const std::array<VegetationKindInfo, VEGETATION_KINDS> VEGETATION_KINDS_INFO;

// One row of 6.3 per land type 0 to 10.
struct VegetationDensity {
    int fallback_timber;  // t when the timber layer is absent
    int conifer_c0;       // %
    int tree_q8;          // trees (Q8 per cell) at t = 255: 393 (60 per ha), 983 (150) on forest types
    int shrub_q8;
    int reed_q8;
    int snag_q8;
};

extern const std::array<VegetationDensity, 11> VEGETATION_DENSITY;

// 6.3: the expected counts (Q8 per cell) of the five kinds on a land type
// with timber t at elevation E (decimetres).
[[nodiscard]] std::array<std::uint32_t, VEGETATION_KINDS> expected_counts(std::uint8_t type, int timber,
                                                                          std::int64_t elevation_dm);

// 6.4: SplitMix64's mix and the per-world key.
[[nodiscard]] std::uint64_t mix64(std::uint64_t z) noexcept;
[[nodiscard]] std::uint64_t scatter_key(std::uint64_t seed) noexcept;

struct Instance {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    std::uint16_t yaw1024 = 0;
    std::uint16_t scale_pm = 0;
    std::uint8_t tint = 0;
};

struct Placement {
    std::array<std::vector<Instance>, VEGETATION_KINDS> kinds;  // kept, in placement order
    std::uint64_t candidates = 0;                               // S
    std::uint64_t cap = 0;                                      // B
    [[nodiscard]] std::uint64_t kept() const noexcept {
        std::uint64_t n = 0;
        for (const auto& k : kinds) {
            n += k.size();
        }
        return n;
    }
};

// 6.3, 6.4. timber and water empty when absent. Throws std::invalid_argument
// on a size mismatch.
[[nodiscard]] Placement place_vegetation(const Layout& layout, std::span<const float> heights,
                                         std::span<const std::int16_t> elevation, std::span<const std::uint8_t> timber,
                                         std::span<const std::uint8_t> water, const GroundCells& cells,
                                         std::uint64_t seed, std::uint64_t cap);

}  // namespace unmapped::terrain
