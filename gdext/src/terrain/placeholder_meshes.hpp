#pragma once

// Placeholder vegetation meshes of the 3D terrain preview (spec V3D-02 6.2).
//
// Plain C++, no Godot types: flat-shaded triangles (three vertices each,
// normal the outward face normal, clockwise seen from outside, which is
// Godot's front face) with one sRGB colour per primitive. Origin at the
// ground point, +Y up, metres. TerrainPreview turns each into an ArrayMesh.

#include <cstdint>
#include <vector>

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
[[nodiscard]] MeshData build_mesh(int kind, int lod);

}  // namespace unmapped::terrain
