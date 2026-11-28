// Spec V3D-02 11.1, U16: the placeholder meshes.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>

#include "terrain/placeholder_meshes.hpp"

using namespace unmapped::terrain;

TEST_CASE("U16 build_mesh") {
    const int lods[5] = {2, 2, 1, 1, 1};
    const std::size_t tris[5][2] = {{92, 14}, {60, 8}, {36, 0}, {12, 0}, {26, 0}};
    for (int kind = 0; kind < 5; ++kind) {
        CAPTURE(kind);
        CHECK(lod_count(kind) == lods[kind]);
        for (int lod = 0; lod < lods[kind]; ++lod) {
            CAPTURE(lod);
            const MeshData m = build_mesh(kind, lod);
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
        CHECK(build_mesh(kind, lods[kind]).triangles() == 0);
    }
    // Broadleaf colours: trunk then crown.
    const MeshData b = build_mesh(0, 0);
    CHECK(b.colours.front() == 0x5A4632FFu);
    CHECK(b.colours.back() == 0x3F6A2EFFu);
    CHECK(lod_count(5) == 0);
}
