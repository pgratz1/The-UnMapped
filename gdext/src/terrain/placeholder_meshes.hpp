#pragma once

// Placeholder vegetation meshes of the 3D terrain preview (spec V3D-02 6.2,
// V3D-04 7).
//
// Plain C++, no Godot types: triangles (three vertices each, clockwise seen
// from outside, which is Godot's front face). V3D-02's meshes are flat
// shaded (normal the outward face normal) with one sRGB colour per
// primitive; V3D-04's shrub has smooth normals and a colour per vertex.
// Origin at the ground point, +Y up, metres. TerrainPreview turns each into
// an ArrayMesh.

#include <cstdint>
#include <vector>

#include "terrain/look.hpp"

namespace unmapped::terrain {

struct MeshData {
    std::vector<float> positions;       // x, y, z per vertex
    std::vector<float> normals;         // x, y, z per vertex
    std::vector<std::uint32_t> colours; // 0xRRGGBBAA per vertex (sRGB)
    [[nodiscard]] std::size_t triangles() const noexcept { return colours.size() / 3; }
};

// The LOD count of a mesh id (6.1): 2, 2, 1, 1, 1; 0 for an unknown id.
[[nodiscard]] int lod_count(int kind);

// The mesh of a kind (mesh id 0 to 4) and LOD; empty for an unknown pair.
// Only the shrub (kind 2) depends on the look: look.shrubs false gives
// V3D-02's, true V3D-04 7's three-lobed shrub.
[[nodiscard]] MeshData build_mesh(int kind, int lod, const Look& look);

// V3D-04 7: the shrub's lobes (centre, radii), in the order drawn, and its
// vertex colours (sRGB) at the bottom and the top.
struct ShrubLobe {
    double cx, cy, cz;
    double rx, ry, rz;
};
inline constexpr ShrubLobe SHRUB_LOBES[3] = {
    {0.0, 0.55, 0.0, 0.95, 0.75, 0.95},
    {0.55, 0.45, 0.30, 0.70, 0.60, 0.70},
    {-0.45, 0.50, -0.40, 0.75, 0.65, 0.75},
};
inline constexpr int SHRUB_SIDES = 5;
inline constexpr int SHRUB_RINGS = 3;
inline constexpr double SHRUB_TOP = 1.30;
inline constexpr std::uint32_t SHRUB_DARK = 0x3C6420;
inline constexpr std::uint32_t SHRUB_LIGHT = 0x6E9E38;

}  // namespace unmapped::terrain
