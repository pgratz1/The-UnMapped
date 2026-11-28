#pragma once

// The look pass of the 3D terrain preview (spec V3D-04 sections 5, 8, 9).
//
// Plain C++, no Godot types: which items are drawn with V3D-04's look
// (`Look`, the option --look-v3d03), the light's constants for both looks,
// the sun's direction and the start of --at-peak. TerrainPreview applies
// them to the scene; the unit tests (gdext/tests/look_test.cpp) use this
// file without Godot.
//
// sun_direction uses floating point and std::sin and std::cos: rendering
// only, compared with tolerances and never hashed (spec 13).

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "terrain/terrain_data.hpp"

namespace unmapped::terrain {

struct GroundCells;

// true = the V3D-04 look of that item; false = V3D-03's (section 9).
struct Look {
    bool rock = true;
    bool light = true;
    bool snow = true;
    bool shrubs = true;
    bool sea = true;
};

// `all`, or a comma list of rock, light, snow, shrubs, sea (repeats
// allowed): those items false. Anything else: std::nullopt.
[[nodiscard]] std::optional<Look> parse_look_v3d03(std::string_view list);

// The items drawn as in V3D-03, in the order rock, light, snow, shrubs,
// sea, joined by ", "; empty when none is.
[[nodiscard]] std::string restored_items(const Look& look);

// Section 5: the light of V3D-04 ...
inline constexpr float SUN_AZIMUTH_DEG = 210.0f;  // the sun's position, clockwise from north
inline constexpr float SUN_ELEVATION_DEG = 45.0f;
inline constexpr float SUN_ENERGY = 0.75f;
inline constexpr float AMBIENT_SKY_CONTRIBUTION = 0.4f;
inline constexpr std::array<float, 3> AMBIENT_RGB = {0.70f, 0.74f, 0.82f};  // sRGB
inline constexpr float AMBIENT_ENERGY = 0.8f;
// ... and V3D-03's (the scene file's values).
inline constexpr float V3D03_SUN_AZIMUTH_DEG = 315.0f;
inline constexpr float V3D03_SUN_ELEVATION_DEG = 40.0f;
inline constexpr float V3D03_SUN_ENERGY = 1.0f;
inline constexpr float V3D03_AMBIENT_SKY_CONTRIBUTION = 1.0f;
inline constexpr std::array<float, 3> V3D03_AMBIENT_RGB = {0.0f, 0.0f, 0.0f};
inline constexpr float V3D03_AMBIENT_ENERGY = 1.0f;

// The direction the light travels (the light's -Z axis), X east, Y up,
// Z south: (-cos e sin A, -sin e, cos e cos A).
[[nodiscard]] std::array<float, 3> sun_direction(float azimuth_deg, float elevation_deg);

// Section 8: the start of --at-peak. The land cell (types 1 to 10, not
// painted) of largest elevation, ties to the smaller row then column; the
// camera cell (x, min(n - 1, y + 125)), 2,000 m south. No land (or no
// ground cells): the centre for both, found false.
struct PeakView {
    int x = 0;
    int y = 0;
    int cam_x = 0;
    int cam_y = 0;
    bool found = false;
};

inline constexpr int PEAK_VIEW_ROWS = 125;

[[nodiscard]] PeakView peak_view(const Layout& layout, std::span<const std::int16_t> elevation, const GroundCells& cells);

}  // namespace unmapped::terrain
