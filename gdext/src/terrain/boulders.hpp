#pragma once

// Boulders of the 3D terrain preview (spec V3D-06 section 11).
//
// Plain C++, no Godot types: the density per 16 m cell, the keyed placement
// of the near tier (slot 5 of V3D-02 6.4's keys, so no draw is shared with
// the vegetation), the three placeholder meshes and the instance buffer.
// TerrainPreview (terrain_preview_near.cpp) computes a near cell's boulders
// on the same worker task as its vegetation; the unit tests
// (gdext/tests/boulders_test.cpp) use this file without Godot.

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "terrain/detail.hpp"
#include "terrain/ground.hpp"
#include "terrain/placeholder_meshes.hpp"
#include "terrain/terrain_data.hpp"

namespace unmapped::terrain {

inline constexpr int BOULDER_SHAPES = 3;
inline constexpr std::uint64_t BOULDER_SLOT = 5;
inline constexpr std::uint32_t ROCK_MESH_KEY = 0x524F434Bu;
// q's amplitude threshold (cm): spec 150; tuned to 200 with grassland's
// AMP_CM 200, so that plains stay free of boulders.
inline constexpr std::int64_t BOULDER_A0_CM = 200;
inline constexpr float BOULDER_LOD0_END = 250.0f;
inline constexpr float BOULDER_LOD1_END = 1200.0f;
inline constexpr float BOULDER_FADE = 100.0f;

// What placement reads. Pointers and spans must outlive every use; the
// scene keeps them unchanged while a worker task reads them.
struct BoulderInputs {
    const Layout* layout = nullptr;          // coarse
    std::span<const std::int16_t> elevation;
    std::span<const std::uint8_t> water;     // empty when absent
    const GroundCells* cells = nullptr;
    const DetailPlan* plan = nullptr;        // factors (A_cell) and pads
    const GroundField* field = nullptr;      // the ground boulders stand on
    std::uint64_t seed = 0;
    bool rock_look = true;                   // look.rock: the cell's rock variant, else 11
};

// q(c) of 11 (Q8 per cell); 0 on an excluded cell.
[[nodiscard]] std::uint32_t boulder_density(const BoulderInputs& in, int x, int y);

struct Boulder {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float d = 0.0f;  // diameter, m
    std::uint8_t shape = 0;
    std::uint16_t yaw1024 = 0;
    float tilt_x = 0.0f;  // degrees
    float tilt_z = 0.0f;
    std::uint8_t tint = 0;
    std::uint8_t rock_id = 11;
};

// 11's boulders of terrain cells x0 <= x < x1, y0 <= y < y1 (clamped), in
// placement order (y, x, k).
[[nodiscard]] std::vector<Boulder> place_boulders(const BoulderInputs& in, int x0, int y0, int x1, int y1);
// Those of near cell (i, j) (terrain cells 16 i to 16 i + 15), per shape.
[[nodiscard]] std::array<std::vector<Boulder>, BOULDER_SHAPES> near_boulders(const BoulderInputs& in, int i, int j);

// 11's mesh table.
[[nodiscard]] float boulder_height(int shape) noexcept;
[[nodiscard]] MeshData build_boulder_mesh(int shape, int lod);
// The instance colour (linear factors, 0 to 1) of a boulder.
[[nodiscard]] std::array<float, 3> boulder_colour(const Boulder& b) noexcept;
// V3D-03 7.2's buffer layout, 16 floats per boulder.
[[nodiscard]] std::vector<float> boulder_buffer(std::span<const Boulder> boulders);

}  // namespace unmapped::terrain
