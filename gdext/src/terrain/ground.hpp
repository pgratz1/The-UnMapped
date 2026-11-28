#pragma once

// Ground of the 3D terrain preview (spec V3D-02 sections 3 to 5, V3D-04 4).
//
// Plain C++, no Godot types: the seventeen ground types, their generated
// textures and colours, the per-cell ground (type, river paint, rock,
// snow), Terrain3D's control map and the colour map of the textured mode.
// TerrainPreview turns the buffers into Godot images and texture assets;
// the unit tests (gdext/tests/ground_test.cpp) use this file without Godot.
//
// Every buffer here is integer arithmetic (spec 15); only albedo_colour,
// which is compared with a tolerance and never hashed, uses floating point.

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "terrain/look.hpp"
#include "terrain/terrain_data.hpp"
#include "worldgen/inspect/geography.hpp"

namespace unmapped::terrain {

// The ids of 3.1 (and V3D-04 4.1). Ids 0 to 10 are land, 11 to 16 are not.
enum class GroundType : std::uint8_t {
    unknown = 0,
    grassland = 1,
    forest_floor = 2,
    carr = 3,
    marsh = 4,
    fen = 5,
    steppe = 6,
    heath = 7,
    moor = 8,
    scree = 9,
    waste = 10,
    rock = 11,
    snow = 12,
    water = 13,
    shore = 14,
    pale_rock = 15,   // V3D-04 4.1: rock band overlays, never a base
    brown_rock = 16,
};

inline constexpr int GROUND_TYPE_COUNT = 17;

// V3D-04 4.2 and 4.3.
inline constexpr std::int32_t ROCK_BAND_DM = 450;
inline constexpr std::array<std::uint8_t, 5> ROCK_SEQ = {11, 15, 11, 16, 15};
inline constexpr std::int64_t SNOW_T0 = -10;           // 0.1 C: no snow line at or above
inline constexpr std::int64_t SNOW_T1 = SNOW_T0 - 20;  // full snow line at or below
// The slope hold of 4.3, in g (dm over 32 m): full up to SNOW_HOLD_FULL_G,
// none from SNOW_HOLD_NONE_G. Tuned (V3D-04 10.3, S1): the spec's 320 (45
// degrees) and 457 (55 degrees) left 57 to 60 per cent of alpine cells and
// most summits past the hold, so no cap could form; 457 (55 degrees) and
// 627 (about 63 degrees) leave the steepest faces bare.
inline constexpr std::int64_t SNOW_HOLD_FULL_G = 457;
inline constexpr std::int64_t SNOW_HOLD_NONE_G = 627;
inline constexpr std::uint32_t ROCK_JITTER_KEY = 0x524B4A54u;
inline constexpr std::uint32_t SNOW_JITTER_KEY = 0x534E4F57u;
inline constexpr int GROUND_TEXTURE_SIZE = 256;

struct GroundTypeInfo {
    const char* key;             // biome key mapped to this type; "" for none (unknown, rock, snow)
    std::uint32_t fallback_rgb;  // the colour of 3.1 (the shipped record's colour for ids 1 to 10)
    int p0;                      // base lattice period
    int grain;                   // grain depth G
    int normal_strength;         // normal strength S
    std::uint8_t roughness;      // normal alpha
    float uv_scale;              // 1/m
};

// Table 3.1 with V3D-04 4.1's rows 15 and 16, indexed by id.
extern const std::array<GroundTypeInfo, GROUND_TYPE_COUNT> GROUND_TYPES;

// 3.1: the type of a biome key; unknown for any key not in the table.
[[nodiscard]] GroundType ground_type_of_key(std::string_view key);

// The sRGB colour c of 3.3: the record with the type's key for ids 1 to
// 10 (else the shipped value), the table's colour for the others.
[[nodiscard]] std::uint32_t ground_type_colour(GroundType type,
                                               std::span<const worldgen::inspect::BiomeRecord> records);

// 3.2's hash.
[[nodiscard]] std::uint32_t hash32(std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept;

// floor(sqrt(v)) for v >= 0.
[[nodiscard]] std::int64_t isqrt64(std::int64_t v) noexcept;

struct GroundTexture {
    std::vector<std::uint8_t> albedo;  // 4 * 256 * 256 bytes, row-major RGBA
    std::vector<std::uint8_t> normal;  // likewise
    double mean_grey = 0.0;            // mean of the albedo's grey g (3.3)
};

// 3.2: the two images of a type, identical in every world.
[[nodiscard]] GroundTexture build_ground_texture(GroundType type);

// 3.3: albedo_color (linear, channels may exceed 1).
[[nodiscard]] std::array<float, 3> albedo_colour(GroundType type, const GroundTexture& texture,
                                                 std::span<const worldgen::inspect::BiomeRecord> records);

// The value to give Terrain3D's albedo_color so that the shader multiplies
// by the linear value u: Godot hands the colour array to the shader through
// an sRGB-to-linear conversion, so u is sRGB-encoded first (the standard
// curve, extended above 1). Found while building V3D-02: 3.3's assumption
// that albedo_color reaches the shader unchanged is false.
[[nodiscard]] float srgb_encode(float u);

// The layers read for the ground. A span is empty when its layer is absent.
struct GroundInputs {
    const Layout* layout = nullptr;
    std::span<const std::int16_t> elevation;
    std::span<const std::uint8_t> biome;
    std::span<const std::uint8_t> water;
    std::span<const std::uint8_t> river;
    std::span<const std::uint8_t> flow_dir;
    std::span<const std::int16_t> temperature;
    std::span<const worldgen::inspect::BiomeRecord> records;
};

// Per cell, row-major, n * n each (4.1, 5.2).
struct GroundCells {
    std::vector<std::uint8_t> type;     // GroundType id
    std::vector<std::uint8_t> painted;  // river paint class, 0 none, 16 no class
    std::vector<std::uint8_t> rock;     // 0 to 255
    std::vector<std::uint8_t> snow;     // 0 to 255
    // V3D-04 (control only; placement reads rock and snow above).
    std::vector<std::uint8_t> rock_variant;  // 11, 15 or 16 (4.2)
    std::vector<std::uint8_t> snow_line;     // 0 to 255 (4.3)
};

// V3D-04 4.2 for a land cell at (x, y) of elevation e (dm).
[[nodiscard]] std::uint8_t rock_variant(int x, int y, std::int32_t e) noexcept;

// V3D-04 4.3 for a land cell at (x, y) of temperature t (0.1 C) and slope
// g (V3D-02 4.1's isqrt of the central differences, dm over 32 m).
[[nodiscard]] std::uint8_t snow_line(int x, int y, std::int64_t t, std::int64_t g) noexcept;

// 4.1 and 5.2. Throws std::invalid_argument when a non-empty span is not
// n * n long or elevation is empty.
[[nodiscard]] GroundCells build_ground_cells(const GroundInputs& inputs);

// 5.2: the river paint class of each cell (0 none, 16 where the river
// layer is absent). Empty water gives all 0.
[[nodiscard]] std::vector<std::uint8_t> river_paint(const Layout& layout, std::span<const std::uint8_t> water,
                                                    std::span<const std::uint8_t> river,
                                                    std::span<const std::uint8_t> flow_dir);

// 4.2: control values (bit patterns), n * n: V3D-02's rule.
[[nodiscard]] std::vector<std::uint32_t> build_control(const Layout& layout, const GroundCells& cells);

// V3D-04 4.4: with look.rock the rock overlay is the cell's rock_variant,
// with look.snow the snow line replaces snow and wins at 128 or more. With
// both restored it is the two-argument form bit for bit. An empty
// rock_variant or snow_line reads as 11 or 0.
[[nodiscard]] std::vector<std::uint32_t> build_control(const Layout& layout, const GroundCells& cells, const Look& look);

// V3D-04 4.4: over the land cells of type 9 (alpine) not painted, how many
// have a snow overlay (12) and a rock overlay (11, 15 or 16) of blend 128 or
// more in `control`.
struct LookCounts {
    std::uint64_t alpine = 0;
    std::uint64_t snow = 0;
    std::uint64_t rock = 0;
};
[[nodiscard]] LookCounts look_counts(const Layout& layout, const GroundCells& cells, std::span<const std::uint32_t> control);

// 9.2's percentage of a to one decimal, in tenths: (1000 v + a/2) / a, 0
// when a = 0.
[[nodiscard]] std::uint64_t tenths_of(std::uint64_t v, std::uint64_t a) noexcept;

// 5.1: RGBA colour map of the textured mode, 4 * n * n bytes.
[[nodiscard]] std::vector<std::uint8_t> build_textured_colours(const Layout& layout, const GroundCells& cells,
                                                               std::span<const worldgen::inspect::BiomeRecord> records);

// Land types are ids 0 to 10 (4.1).
[[nodiscard]] constexpr bool is_land_type(std::uint8_t type) noexcept { return type <= 10; }

}  // namespace unmapped::terrain
