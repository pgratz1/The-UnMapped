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

// An instance's offset from its cell's vertex, one axis (Paul, 2026-11-27;
// replaces 6.4's jx, jz in [-6, 6) from bits 0 to 19 of h, which left a
// 4 m empty lane between cells, so trees stood in rows 16 m apart). Each
// axis has its own keyed draw, mix64(h ^ JITTER_KEY_X) and
// mix64(h ^ JITTER_KEY_Z); its top 10 bits k give (2k + 1) / 128 - 8 m:
// 1024 steps of 1/64 m covering the whole cell, [-7.992, 7.992], so the
// nearest vertex is still the instance's own cell.
inline constexpr std::uint64_t JITTER_KEY_X = 0x6A09E667F3BCC909ull;
inline constexpr std::uint64_t JITTER_KEY_Z = 0xBB67AE8584CAA73Bull;
[[nodiscard]] float cell_jitter(std::uint64_t draw) noexcept;

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
// on a size mismatch. Built on place_candidates over the whole map.
[[nodiscard]] Placement place_vegetation(const Layout& layout, std::span<const float> heights,
                                         std::span<const std::int16_t> elevation, std::span<const std::uint8_t> timber,
                                         std::span<const std::uint8_t> water, const GroundCells& cells,
                                         std::uint64_t seed, std::uint64_t cap, const GroundField* ground = nullptr);

// --- Tiers (spec V3D-03 4, 5) -------------------------------------------------------

// One candidate of 6.4 before any cap: the instance, its keyed thin draw and
// its mesh id.
struct Candidate {
    Instance instance;
    std::uint32_t thin = 0;
    std::uint8_t kind = 0;
};

// The inputs of a placement, as place_vegetation's arguments. timber and
// water empty when absent. The pointers and spans must outlive every use;
// the scene keeps them unchanged while a worker task reads them.
struct VegetationInputs {
    const Layout* layout = nullptr;
    std::span<const float> heights;
    std::span<const std::int16_t> elevation;
    std::span<const std::uint8_t> timber;
    std::span<const std::uint8_t> water;
    const GroundCells* cells = nullptr;
    std::uint64_t seed = 0;
    // V3D-06 10: when set, Y = ground_height on this field (heights above
    // then only sizes the inputs); null: ground_low on heights, as before.
    const GroundField* ground = nullptr;
};

// 6.4's candidates of terrain cells x0 <= x < x1, y0 <= y < y1 (clamped to
// the map), in placement order (y, x, kind, k): the whole map's list
// filtered by cell. Throws std::invalid_argument on a size mismatch.
[[nodiscard]] std::vector<Candidate> place_candidates(const VegetationInputs& in, int x0, int y0, int x1, int y1);

// 4.1: the kinds of the far tier (trees and snags); shrubs and reeds are
// near-only.
inline constexpr std::array<bool, VEGETATION_KINDS> FAR_KIND = {true, true, false, false, true};
inline constexpr std::uint64_t DEFAULT_FAR_BUDGET = 2000000;  // Paul, 2026-11-27 (spec r1: 1,000,000)
inline constexpr std::uint64_t THIN_ALL = std::uint64_t{1} << 32;  // T when the budget holds every far candidate

// 4.2: the far tier. threshold T, far_candidates S_far, budget B;
// placement.kinds the far instances (thin < T) in placement order, ids 2
// and 3 empty; placement.candidates S (every kind), placement.cap B.
struct FarTier {
    std::uint64_t threshold = THIN_ALL;
    std::uint64_t far_candidates = 0;
    std::uint64_t budget = 0;
    Placement placement;
};
[[nodiscard]] FarTier place_far_tier(const VegetationInputs& in, std::uint64_t budget);

// Whether a candidate belongs to the far tier of threshold T (4.2).
[[nodiscard]] inline bool is_far(const Candidate& c, std::uint64_t threshold) noexcept {
    return FAR_KIND[c.kind] && static_cast<std::uint64_t>(c.thin) < threshold;
}

// 4.2: Extra of near cell (i, j), terrain cells 16i to 16i + 15 and 16j to
// 16j + 15, per kind, in placement order.
[[nodiscard]] std::array<std::vector<Instance>, VEGETATION_KINDS> near_instances(const VegetationInputs& in, int i, int j,
                                                                                  std::uint64_t threshold);

// 7.2: 16 floats per instance, the 3 x 4 transform rows (basis row, origin
// component) then RGBA. tint_mode 0 grey (v, v, v), 1 the tier colours
// (v, 0.45 v, 0.45 v).
[[nodiscard]] std::vector<float> instance_buffer(std::span<const Instance> instances, std::uint8_t tint_mode);

}  // namespace unmapped::terrain
