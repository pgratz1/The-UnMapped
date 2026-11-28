// Spec V3D-01 9.1, U6: the fly camera's step rules of 8.2.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include <cmath>

#include "terrain/fly_state.hpp"

using namespace unmapped::terrain;

namespace {

// |a - b| within a relative 1e-5 (at least 1e-5 absolute), in float.
bool near(float a, float b) {
    const float scale = std::max(1.0f, std::max(std::abs(a), std::abs(b)));
    return std::abs(a - b) <= 1e-5f * scale;
}

}  // namespace

namespace {

float flat(float, float) {
    return 0.0f;
}

constexpr float BOUND = 16.0f * 1024.0f;

FlyState at_altitude(float y) {
    FlyState s;
    s.y = y;
    s.pitch_deg = 0.0f;
    return s;
}

}  // namespace

TEST_CASE("U6 forward at heading 0 and 90, boost") {
    FlyState s = at_altitude(1000.0f);
    FlyInput in;
    in.forward = 1;
    step(s, in, 1.0f, flat, BOUND);
    CHECK(std::abs(s.z + 200.0f) <= 0.01f);
    CHECK(near(s.x, 0.0f));
    CHECK(near(s.y, 1000.0f));

    s = at_altitude(1000.0f);
    s.heading_deg = 90.0f;
    step(s, in, 1.0f, flat, BOUND);
    CHECK(std::abs(s.x - 200.0f) <= 0.01f);
    CHECK(std::abs(s.z) <= 0.01f);

    s = at_altitude(1000.0f);
    in.boost = true;
    step(s, in, 1.0f, flat, BOUND);
    CHECK(near(s.z, -800.0f));
}

TEST_CASE("U6 strafe and climb") {
    FlyState s = at_altitude(1000.0f);
    FlyInput in;
    in.right = 1;
    step(s, in, 1.0f, flat, BOUND);
    CHECK(near(s.x, 200.0f));
    in = {};
    in.up = 1;
    step(s, in, 0.5f, flat, BOUND);
    CHECK(near(s.y, 1100.0f));
}

TEST_CASE("U6 pitch clamps, heading wraps") {
    FlyState s = at_altitude(1000.0f);
    FlyInput in;
    in.pitch_deg = 200.0f;
    step(s, in, 0.0f, flat, BOUND);
    CHECK(s.pitch_deg == 89.0f);
    in.pitch_deg = -500.0f;
    step(s, in, 0.0f, flat, BOUND);
    CHECK(s.pitch_deg == -89.0f);
    in = {};
    in.yaw_deg = 370.0f;
    step(s, in, 0.0f, flat, BOUND);
    CHECK(near(s.heading_deg, 10.0f));
    in.yaw_deg = -20.0f;
    step(s, in, 0.0f, flat, BOUND);
    CHECK(near(s.heading_deg, 350.0f));
    CHECK(s.heading_deg >= 0.0f);
    CHECK(s.heading_deg < 360.0f);
}

TEST_CASE("U6 a dive onto a 1000 m plateau stays at 1002") {
    FlyState s = at_altitude(5000.0f);
    s.pitch_deg = -89.0f;
    FlyInput in;
    in.forward = 1;
    in.boost = true;
    s.speed_index = 7;
    const auto plateau = [](float, float) { return 1000.0f; };
    for (int i = 0; i < 100; ++i) {
        step(s, in, 0.1f, plateau, BOUND);
        CHECK(s.y >= 1002.0f);
    }
    CHECK(s.y == 1002.0f);
    // Above the ceiling.
    s = at_altitude(29990.0f);
    in = {};
    in.up = 1;
    step(s, in, 1.0f, plateau, BOUND);
    CHECK(s.y == 30000.0f);
}

TEST_CASE("U6 speed steps saturate") {
    FlyState s;
    CHECK(s.speed_index == 4);
    CHECK(speed_of(s, false) == 200.0f);
    for (int i = 0; i < 10; ++i) {
        speed_step(s, 1);
    }
    CHECK(s.speed_index == 7);
    CHECK(speed_of(s, false) == 2000.0f);
    CHECK(speed_of(s, true) == 8000.0f);
    for (int i = 0; i < 10; ++i) {
        speed_step(s, -1);
    }
    CHECK(s.speed_index == 0);
    CHECK(speed_of(s, false) == 10.0f);
}

TEST_CASE("U6 the bound clamps x and z") {
    FlyState s = at_altitude(1000.0f);
    s.x = BOUND - 10.0f;
    s.z = -BOUND + 10.0f;
    s.heading_deg = 45.0f;
    FlyInput in;
    in.forward = 1;
    s.speed_index = 7;
    step(s, in, 10.0f, flat, BOUND);
    CHECK(s.x == BOUND);
    CHECK(s.z == -BOUND);
}

TEST_CASE("camera rotation") {
    FlyState s;
    s.heading_deg = 90.0f;
    s.pitch_deg = -25.0f;
    const Rotation r = camera_rotation(s);
    CHECK(near(r.x, -0.4363323f));
    CHECK(near(r.y, -1.5707963f));
    CHECK(r.z == 0.0f);
}
