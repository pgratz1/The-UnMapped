#pragma once

// Terrain data of the 3D terrain preview (spec V3D-01 sections 5 and 6).
//
// Plain C++, no Godot types: the layout of a world's map on Terrain3D's
// region grid, the height and colour buffers built from the world's layers,
// the conversions between cells and world metres, and the HUD line of 8.4.
// TerrainPreview (terrain_preview.hpp) turns the buffers into Godot images;
// the unit tests (gdext/tests/) use this file without Godot.
//
// Floating point is allowed here: nothing computed in this file reaches a
// world file (the preview opens world.db read-only).

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "worldgen/inspect/geography.hpp"

namespace unmapped::terrain {

inline constexpr float CELL_M = 16.0f;         // metres per cell and per vertex (5.1)
inline constexpr int REGION_SIZE = 256;        // vertices per region side (5.1)
inline constexpr std::uint8_t A_NEUTRAL = 128; // colour-map alpha: roughness unchanged (5.3, V7)

struct Layout {
    int n = 0;              // map side in cells (512, 1024 or 2048)
    float spacing = CELL_M; // vertex spacing in metres
    int region_size = REGION_SIZE;
    int region_min = 0;     // lowest region location on each axis: -n/512
    int region_count = 0;   // regions per axis: n/256
    float origin = 0.0f;    // X and Z of vertex (0, 0): -8n
};

struct Vec2 {
    float x = 0.0f;
    float z = 0.0f;
};

struct CellHit {
    int x = 0;
    int y = 0;
    bool inside = false;
};

// The layout for a width x height elevation layer; nullopt with problem set
// to 5.1's refusal text unless width = height is 512, 1024 or 2048.
[[nodiscard]] std::optional<Layout> layout_for(int width, int height, std::string& problem);

// n*n heights in metres, row-major (5.2). water_depth empty means absent.
// Throws std::invalid_argument when a non-empty span is not n*n long.
[[nodiscard]] std::vector<float> build_heights(const Layout& layout, std::span<const std::int16_t> elevation,
                                               std::span<const std::uint16_t> water_depth);

// 4*n*n bytes RGBA (5.3). biome empty (or no biome records) means grey.
// Throws std::invalid_argument when a non-empty biome span is not n*n long.
[[nodiscard]] std::vector<std::uint8_t> build_colours(const Layout& layout, std::span<const std::uint8_t> biome,
                                                      std::span<const worldgen::inspect::BiomeRecord> records);

// The number of cells whose code has no biome record (drawn magenta).
[[nodiscard]] std::size_t unknown_biome_cells(std::span<const std::uint8_t> biome,
                                              std::span<const worldgen::inspect::BiomeRecord> records);

// 0xRRGGBB of the record keyed `ocean`, else 0x183878 (5.4).
[[nodiscard]] std::uint32_t sea_colour(std::span<const worldgen::inspect::BiomeRecord> records);

// (X, Z) of cell (x, y): 16 (x - n/2), 16 (y - n/2).
[[nodiscard]] Vec2 cell_to_world(const Layout& layout, int x, int y);

// The cell nearest to (X, Z); inside iff it is a cell of the map.
[[nodiscard]] CellHit world_to_cell(const Layout& layout, float X, float Z);

// The largest of the four heights around (X, Z), clamped to the map (6).
[[nodiscard]] float ground_clamp(const Layout& layout, std::span<const float> heights, float X, float Z);

// Bilinear height at (X, Z), clamped to the map (6; HUD only).
[[nodiscard]] float sample_bilinear(const Layout& layout, std::span<const float> heights, float X, float Z);

// The lower of the two triangulations of the cell square at (X, Z), with
// the grid point of sample_bilinear (spec V3D-02 6.5): never above the
// surface Terrain3D draws.
[[nodiscard]] float ground_low(const Layout& layout, std::span<const float> heights, float X, float Z);

// --- HUD (8.4) ------------------------------------------------------------------

// What the HUD reads of a world. Spans empty when the layer is absent.
struct HudWorld {
    const Layout* layout = nullptr;
    std::span<const float> heights;
    std::span<const std::int16_t> elevation;
    std::span<const std::uint16_t> water_depth;
    std::span<const std::uint8_t> biome;
    std::span<const std::uint16_t> region;
    // Name of a biome code ("" when unknown) and of a region id ("" when none).
    std::function<std::string(std::int64_t)> biome_name;
    std::function<std::string(std::int64_t)> region_name;
};

// The first HUD line for a camera at (X, Y, Z) metres flying at speed m/s
// with the given frame rate.
[[nodiscard]] std::string hud_line(const HudWorld& world, float X, float Y, float Z, float speed, int fps);

// The key line under it.
inline constexpr const char* HUD_KEYS =
    "WASD move, E/Q up/down, right mouse look, wheel speed, Shift fast, R start, T trees, G ground, H hide";

// One decimal, without a negative zero ("12.5", "-0.1", "0.0").
[[nodiscard]] std::string one_decimal(double value);

}  // namespace unmapped::terrain
