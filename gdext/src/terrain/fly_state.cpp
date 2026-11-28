#include "terrain/fly_state.hpp"

#include <algorithm>
#include <cmath>

namespace unmapped::terrain {

namespace {

constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

}  // namespace

float speed_of(const FlyState& state, bool boost) {
    const int i = std::clamp(state.speed_index, 0, static_cast<int>(SPEEDS.size()) - 1);
    return SPEEDS[static_cast<std::size_t>(i)] * (boost ? BOOST : 1.0f);
}

void speed_step(FlyState& state, int delta) {
    state.speed_index = std::clamp(state.speed_index + (delta > 0 ? 1 : (delta < 0 ? -1 : 0)), 0,
                                   static_cast<int>(SPEEDS.size()) - 1);
}

void step(FlyState& state, const FlyInput& input, float dt, const std::function<float(float, float)>& ground,
          float bound) {
    float heading = std::fmod(state.heading_deg + input.yaw_deg, 360.0f);
    if (heading < 0.0f) {
        heading += 360.0f;
    }
    if (heading >= 360.0f) {
        heading -= 360.0f;
    }
    state.heading_deg = heading;
    state.pitch_deg = std::clamp(state.pitch_deg + input.pitch_deg, -89.0f, 89.0f);

    const float h = state.heading_deg * DEG_TO_RAD;
    const float p = state.pitch_deg * DEG_TO_RAD;
    const float fx = std::sin(h) * std::cos(p);
    const float fy = std::sin(p);
    const float fz = -std::cos(h) * std::cos(p);
    const float rx = std::cos(h);
    const float rz = std::sin(h);
    const float speed = speed_of(state, input.boost);
    const auto forward = static_cast<float>(input.forward);
    const auto right = static_cast<float>(input.right);
    const auto up = static_cast<float>(input.up);

    state.x += speed * (forward * fx + right * rx) * dt;
    state.y += speed * (forward * fy + up) * dt;
    state.z += speed * (forward * fz + right * rz) * dt;

    state.x = std::clamp(state.x, -bound, bound);
    state.z = std::clamp(state.z, -bound, bound);
    const float floor = ground ? ground(state.x, state.z) + GROUND_MARGIN : GROUND_MARGIN;
    state.y = std::clamp(state.y, floor, std::max(floor, MAX_ALTITUDE));
}

Rotation camera_rotation(const FlyState& state) {
    return {state.pitch_deg * DEG_TO_RAD, -state.heading_deg * DEG_TO_RAD, 0.0f};
}

}  // namespace unmapped::terrain
