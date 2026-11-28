#pragma once

// Fly camera of the 3D terrain preview (spec V3D-01 8.2). Plain C++, no
// Godot types; TerrainPreview feeds it the keys and mouse and applies the
// result to its Camera3D.

#include <array>
#include <functional>

namespace unmapped::terrain {

inline constexpr std::array<float, 8> SPEEDS = {10.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f};
inline constexpr int DEFAULT_SPEED_INDEX = 4;  // 200 m/s
inline constexpr float BOOST = 4.0f;            // Shift held
inline constexpr float MAX_ALTITUDE = 30000.0f; // metres
inline constexpr float GROUND_MARGIN = 2.0f;    // metres above ground_clamp

struct FlyState {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float heading_deg = 0.0f;  // 0 north (-Z), 90 east (+X)
    float pitch_deg = 0.0f;    // -89 to 89, negative down
    int speed_index = DEFAULT_SPEED_INDEX;
};

struct FlyInput {
    int forward = 0;  // -1, 0 or 1 on each axis
    int right = 0;
    int up = 0;
    bool boost = false;
    float yaw_deg = 0.0f;    // deltas
    float pitch_deg = 0.0f;
};

// The speed in m/s of the state with or without boost.
[[nodiscard]] float speed_of(const FlyState& state, bool boost);

// One speed step down (delta < 0) or up (delta > 0); saturates at 0 and 7.
void speed_step(FlyState& state, int delta);

// Advances the camera by dt seconds (8.2). ground(X, Z) is the height the
// camera must stay 2 m above; bound is the largest |X| and |Z|.
void step(FlyState& state, const FlyInput& input, float dt, const std::function<float(float, float)>& ground,
          float bound);

// The camera's rotation in radians in Godot's YXZ order: (P, -H, 0).
struct Rotation {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};
[[nodiscard]] Rotation camera_rotation(const FlyState& state);

}  // namespace unmapped::terrain
