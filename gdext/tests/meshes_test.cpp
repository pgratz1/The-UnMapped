// Spec V3D-02 11.1, U16: the placeholder meshes (V3D-02's shrub); spec
// V3D-04 10.1, U36: V3D-04's shrub.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "terrain/placeholder_meshes.hpp"

using namespace unmapped::terrain;

namespace {

Look v3d02_shrub() {
    Look look;
    look.shrubs = false;
    return look;
}

}  // namespace

TEST_CASE("U16 build_mesh") {
    const int lods[5] = {2, 2, 1, 1, 1};
    const std::size_t tris[5][2] = {{92, 14}, {60, 8}, {36, 0}, {12, 0}, {26, 0}};
    for (int kind = 0; kind < 5; ++kind) {
        CAPTURE(kind);
        CHECK(lod_count(kind) == lods[kind]);
        for (int lod = 0; lod < lods[kind]; ++lod) {
            CAPTURE(lod);
            const MeshData m = build_mesh(kind, lod, v3d02_shrub());
            CHECK(m.triangles() == tris[kind][lod]);
            REQUIRE(m.positions.size() == 9 * m.triangles());
            REQUIRE(m.normals.size() == 9 * m.triangles());
            float top = -1e9f;
            int bad_normal = 0;
            int bad_winding = 0;
            for (std::size_t t = 0; t < m.triangles(); ++t) {
                const float* p = &m.positions[9 * t];
                const float* nn = &m.normals[9 * t];
                for (int v = 0; v < 3; ++v) {
                    top = std::max(top, p[3 * v + 1]);
                }
                const float len = std::sqrt(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
                bad_normal += std::abs(len - 1.0f) < 1e-4f ? 0 : 1;
                // Clockwise seen from the side the normal points to: the
                // counter-clockwise cross product points against it.
                const float ax = p[3] - p[0], ay = p[4] - p[1], az = p[5] - p[2];
                const float bx = p[6] - p[0], by = p[7] - p[1], bz = p[8] - p[2];
                const float cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
                bad_winding += (cx * nn[0] + cy * nn[1] + cz * nn[2]) < 0.0f ? 0 : 1;
                // Away from the vertical axis (trunks, crowns, reeds) or from
                // below (caps), for the kinds whose axis is x = z = 0 near the
                // triangle: the centroid's outward direction.
                if (kind == 2 || (kind == 0 && p[1] > 5.0f && p[4] > 5.0f && p[7] > 5.0f)) {
                    const float centre_y = kind == 2 ? 0.5f : 8.5f;
                    const float ox = (p[0] + p[3] + p[6]) / 3.0f;
                    const float oy = (p[1] + p[4] + p[7]) / 3.0f - centre_y;
                    const float oz = (p[2] + p[5] + p[8]) / 3.0f;
                    bad_normal += (ox * nn[0] + oy * nn[1] + oz * nn[2]) > 0.0f ? 0 : 1;
                }
            }
            CHECK(bad_normal == 0);
            CHECK(bad_winding == 0);
            if (kind == 0) {
                CHECK(static_cast<double>(top) == doctest::Approx(11.5));
            }
            if (kind == 1) {
                CHECK(static_cast<double>(top) == doctest::Approx(15.5));
            }
            // One colour per primitive, all opaque.
            bool opaque = true;
            for (const std::uint32_t c : m.colours) {
                opaque = opaque && (c & 0xFFu) == 0xFFu;
            }
            CHECK(opaque);
        }
        CHECK(build_mesh(kind, lods[kind], v3d02_shrub()).triangles() == 0);
    }
    // Broadleaf colours: trunk then crown.
    const MeshData b = build_mesh(0, 0, v3d02_shrub());
    CHECK(b.colours.front() == 0x5A4632FFu);
    CHECK(b.colours.back() == 0x3F6A2EFFu);
    CHECK(lod_count(5) == 0);
}

TEST_CASE("U36 V3D-04's shrub") {
    const MeshData m = build_mesh(2, 0, Look{});
    REQUIRE(m.triangles() == 60);
    REQUIRE(m.positions.size() == 180 * 3);
    REQUIRE(m.normals.size() == 180 * 3);
    REQUIRE(m.colours.size() == 180);
    const double lobes[3][6] = {{0.0, 0.55, 0.0, 0.95, 0.75, 0.95},
                                {0.55, 0.45, 0.30, 0.70, 0.60, 0.70},
                                {-0.45, 0.50, -0.40, 0.75, 0.65, 0.75}};
    int bad_unit = 0, bad_dir = 0, bad_out = 0, bad_winding = 0, bad_colour = 0;
    double top = -1e9;
    const int dark[3] = {0x3C, 0x64, 0x20};
    const int light[3] = {0x6E, 0x9E, 0x38};
    for (std::size_t v = 0; v < 180; ++v) {
        const double* L = lobes[v / 60];
        const double px = static_cast<double>(m.positions[3 * v]);
        const double py = static_cast<double>(m.positions[3 * v + 1]);
        const double pz = static_cast<double>(m.positions[3 * v + 2]);
        const double nx = static_cast<double>(m.normals[3 * v]);
        const double ny = static_cast<double>(m.normals[3 * v + 1]);
        const double nz = static_cast<double>(m.normals[3 * v + 2]);
        top = std::max(top, py);
        bad_unit += std::abs(std::sqrt(nx * nx + ny * ny + nz * nz) - 1.0) < 1e-5 ? 0 : 1;
        // The gradient direction of the lobe at the vertex.
        double gx = (px - L[0]) / (L[3] * L[3]);
        double gy = (py - L[1]) / (L[4] * L[4]);
        double gz = (pz - L[2]) / (L[5] * L[5]);
        const double gl = std::sqrt(gx * gx + gy * gy + gz * gz);
        gx /= gl;
        gy /= gl;
        gz /= gl;
        bad_dir += (std::abs(gx - nx) < 1e-5 && std::abs(gy - ny) < 1e-5 && std::abs(gz - nz) < 1e-5) ? 0 : 1;
        bad_out += ((px - L[0]) * nx + (py - L[1]) * ny + (pz - L[2]) * nz) > 0.0 ? 0 : 1;
        // Colour: between dark and light, as the formula of 7.
        const std::uint32_t c = m.colours[v];
        const double t = std::clamp(py / 1.30, 0.0, 1.0);
        for (int ch = 0; ch < 3; ++ch) {
            const int got = static_cast<int>((c >> (24 - 8 * ch)) & 0xFFu);
            const int want = static_cast<int>(std::lround(dark[ch] + (light[ch] - dark[ch]) * t));
            bad_colour += (got == want && got >= dark[ch] && got <= light[ch]) ? 0 : 1;
        }
        bad_colour += (c & 0xFFu) == 0xFFu ? 0 : 1;
    }
    for (std::size_t t = 0; t < 60; ++t) {
        // Clockwise seen from outside: the counter-clockwise cross product
        // points inwards (against the lobe's outward direction at the centroid).
        double p[9];
        for (int i = 0; i < 9; ++i) {
            p[i] = static_cast<double>(m.positions[9 * t + static_cast<std::size_t>(i)]);
        }
        const double* L = lobes[t / 20];
        const double ax = p[3] - p[0], ay = p[4] - p[1], az = p[5] - p[2];
        const double bx = p[6] - p[0], by = p[7] - p[1], bz = p[8] - p[2];
        const double cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
        const double ox = (p[0] + p[3] + p[6]) / 3.0 - L[0];
        const double oy = (p[1] + p[4] + p[7]) / 3.0 - L[1];
        const double oz = (p[2] + p[5] + p[8]) / 3.0 - L[2];
        bad_winding += (cx * ox + cy * oy + cz * oz) < 0.0 ? 0 : 1;
    }
    CHECK(bad_unit == 0);
    CHECK(bad_dir == 0);
    CHECK(bad_out == 0);
    CHECK(bad_winding == 0);
    CHECK(bad_colour == 0);
    CHECK(top == doctest::Approx(1.30).epsilon(0.01 / 1.30));
    // Colours non-decreasing in y.
    std::vector<std::pair<float, std::uint32_t>> by_y;
    for (std::size_t v = 0; v < 180; ++v) {
        by_y.emplace_back(m.positions[3 * v + 1], m.colours[v]);
    }
    std::sort(by_y.begin(), by_y.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    int decreasing = 0;
    for (std::size_t k = 1; k < by_y.size(); ++k) {
        for (int ch = 0; ch < 3; ++ch) {
            const auto a = (by_y[k - 1].second >> (24 - 8 * ch)) & 0xFFu;
            const auto b = (by_y[k].second >> (24 - 8 * ch)) & 0xFFu;
            decreasing += b >= a ? 0 : 1;
        }
    }
    CHECK(decreasing == 0);
    // Restored: V3D-02's 36-triangle shrub, byte for byte as before.
    Look old;
    old.shrubs = false;
    const MeshData s = build_mesh(2, 0, old);
    CHECK(s.triangles() == 36);
    CHECK(s.colours.front() == 0x55663AFFu);
    // Other kinds ignore the look.
    for (const int kind : {0, 1, 3, 4}) {
        for (int lod = 0; lod < lod_count(kind); ++lod) {
            const MeshData a = build_mesh(kind, lod, Look{});
            const MeshData b = build_mesh(kind, lod, Look{false, false, false, false, false});
            CHECK(a.positions == b.positions);
            CHECK(a.normals == b.normals);
            CHECK(a.colours == b.colours);
        }
    }
}
