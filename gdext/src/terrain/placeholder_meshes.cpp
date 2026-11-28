#include "terrain/placeholder_meshes.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace unmapped::terrain {

namespace {

struct V3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 normalize(V3 a) {
    const double l = std::sqrt(dot(a, a));
    return l > 0.0 ? a * (1.0 / l) : a;
}

constexpr double TAU = 6.283185307179586476925;

struct Builder {
    MeshData mesh;
    std::uint32_t colour = 0xFFFFFFFF;

    // A triangle, wound clockwise seen from the side `outward` points to,
    // with the outward face normal.
    void triangle(V3 a, V3 b, V3 c, V3 outward) {
        V3 n = cross(b - a, c - a);  // counter-clockwise normal
        if (dot(n, outward) > 0.0) {
            std::swap(b, c);
            n = cross(b - a, c - a);
        }
        const V3 face = normalize(n * -1.0);
        for (const V3& p : {a, b, c}) {
            mesh.positions.push_back(static_cast<float>(p.x));
            mesh.positions.push_back(static_cast<float>(p.y));
            mesh.positions.push_back(static_cast<float>(p.z));
            mesh.normals.push_back(static_cast<float>(face.x));
            mesh.normals.push_back(static_cast<float>(face.y));
            mesh.normals.push_back(static_cast<float>(face.z));
            mesh.colours.push_back(colour);
        }
    }

    // 6.2's cone: a frustum from a to b, radii ra and rb, `sides` sides;
    // cap closes the a end with a fan.
    void cone(V3 a, V3 b, double ra, double rb, int sides, bool cap) {
        const V3 w = normalize(b - a);
        const V3 Z{0.0, 0.0, 1.0};
        const V3 X{1.0, 0.0, 0.0};
        const V3 u = std::abs(dot(w, Z)) < 0.9 ? normalize(cross(w, Z)) : normalize(cross(w, X));
        const V3 v = cross(w, u);
        auto ring = [&](V3 centre, double r, int i) {
            const double t = TAU * static_cast<double>(i) / static_cast<double>(sides);
            return centre + u * (r * std::cos(t)) + v * (r * std::sin(t));
        };
        for (int i = 0; i < sides; ++i) {
            const V3 p0 = ring(a, ra, i);
            const V3 p1 = ring(a, ra, i + 1);
            const double t = TAU * (static_cast<double>(i) + 0.5) / static_cast<double>(sides);
            const V3 radial = u * std::cos(t) + v * std::sin(t);
            if (rb <= 0.0) {
                triangle(p0, p1, b, radial);
            } else {
                const V3 q0 = ring(b, rb, i);
                const V3 q1 = ring(b, rb, i + 1);
                triangle(p0, p1, q1, radial);
                triangle(p0, q1, q0, radial);
            }
            if (cap) {
                triangle(a, p0, p1, w * -1.0);
            }
        }
    }

    // V3D-04 7: a triangle wound as `triangle` does, with smooth normals
    // (the gradient of the ellipsoid of centre c and radii r at each vertex)
    // and a colour per vertex from its height.
    void smooth_triangle(V3 a, V3 b, V3 cc, V3 outward, V3 c, V3 r) {
        const V3 n = cross(b - a, cc - a);
        if (dot(n, outward) > 0.0) {
            std::swap(b, cc);
        }
        for (const V3& p : {a, b, cc}) {
            const V3 d = p - c;
            const V3 g = normalize(V3{d.x / (r.x * r.x), d.y / (r.y * r.y), d.z / (r.z * r.z)});
            mesh.positions.push_back(static_cast<float>(p.x));
            mesh.positions.push_back(static_cast<float>(p.y));
            mesh.positions.push_back(static_cast<float>(p.z));
            mesh.normals.push_back(static_cast<float>(g.x));
            mesh.normals.push_back(static_cast<float>(g.y));
            mesh.normals.push_back(static_cast<float>(g.z));
            mesh.colours.push_back(shrub_colour(p.y));
        }
    }

    static std::uint32_t shrub_colour(double y) {
        const double t = std::clamp(y / SHRUB_TOP, 0.0, 1.0);
        std::uint32_t out = 0xFFu;
        for (int shift = 16; shift >= 0; shift -= 8) {
            const double dark = static_cast<double>((SHRUB_DARK >> shift) & 0xFFu);
            const double light = static_cast<double>((SHRUB_LIGHT >> shift) & 0xFFu);
            const auto v = static_cast<std::uint32_t>(std::lround(dark + (light - dark) * t));
            out |= v << (shift + 8);
        }
        return out;
    }

    // V3D-04 7's lobe: 6.2's ellipsoid with smooth normals and vertex colours.
    void smooth_ellipsoid(V3 c, double rx, double ry, double rz, int sides, int rings) {
        auto point = [&](int k, int i) {
            const double phi = -TAU / 4.0 + TAU / 2.0 * static_cast<double>(k) / static_cast<double>(rings);
            const double t = TAU * static_cast<double>(i) / static_cast<double>(sides);
            return c + V3{rx * std::cos(phi) * std::cos(t), ry * std::sin(phi), rz * std::cos(phi) * std::sin(t)};
        };
        const V3 r{rx, ry, rz};
        for (int k = 0; k < rings; ++k) {
            for (int i = 0; i < sides; ++i) {
                const V3 a = point(k, i);
                const V3 b = point(k, i + 1);
                const V3 d = point(k + 1, i);
                const V3 e = point(k + 1, i + 1);
                if (k == 0) {
                    smooth_triangle(a, d, e, ((a + d + e) * (1.0 / 3.0)) - c, c, r);
                } else if (k == rings - 1) {
                    smooth_triangle(a, b, d, ((a + b + d) * (1.0 / 3.0)) - c, c, r);
                } else {
                    smooth_triangle(a, b, e, ((a + b + e) * (1.0 / 3.0)) - c, c, r);
                    smooth_triangle(a, e, d, ((a + e + d) * (1.0 / 3.0)) - c, c, r);
                }
            }
        }
    }

    // 6.2's ellipsoid: latitudes -90 + 180 k / rings, `sides` longitudes.
    void ellipsoid(V3 c, double rx, double ry, double rz, int sides, int rings) {
        auto point = [&](int k, int i) {
            const double phi = -TAU / 4.0 + TAU / 2.0 * static_cast<double>(k) / static_cast<double>(rings);
            const double t = TAU * static_cast<double>(i) / static_cast<double>(sides);
            return c + V3{rx * std::cos(phi) * std::cos(t), ry * std::sin(phi), rz * std::cos(phi) * std::sin(t)};
        };
        for (int k = 0; k < rings; ++k) {
            for (int i = 0; i < sides; ++i) {
                const V3 a = point(k, i);
                const V3 b = point(k, i + 1);
                const V3 d = point(k + 1, i);
                const V3 e = point(k + 1, i + 1);
                if (k == 0) {
                    triangle(a, d, e, ((a + d + e) * (1.0 / 3.0)) - c);
                } else if (k == rings - 1) {
                    triangle(a, b, d, ((a + b + d) * (1.0 / 3.0)) - c);
                } else {
                    triangle(a, b, e, ((a + b + e) * (1.0 / 3.0)) - c);
                    triangle(a, e, d, ((a + e + d) * (1.0 / 3.0)) - c);
                }
            }
        }
    }
};

constexpr std::uint32_t rgba(std::uint32_t rgb) { return (rgb << 8) | 0xFFu; }

constexpr std::uint32_t TRUNK = rgba(0x5A4632);
constexpr std::uint32_t CROWN = rgba(0x3F6A2E);
constexpr std::uint32_t NEEDLES = rgba(0x2B4A2C);
constexpr std::uint32_t SHRUB = rgba(0x55663A);
constexpr std::uint32_t REED = rgba(0x8F8A52);
constexpr std::uint32_t SNAG = rgba(0x4E453C);

}  // namespace

int lod_count(int kind) {
    switch (kind) {
        case 0:
        case 1:
            return 2;
        case 2:
        case 3:
        case 4:
            return 1;
        default:
            return 0;
    }
}

MeshData build_mesh(int kind, int lod, const Look& look) {
    Builder m;
    if (lod < 0 || lod >= lod_count(kind)) {
        return m.mesh;
    }
    switch (kind) {
        case 0:  // broadleaf
            m.colour = TRUNK;
            m.cone({0, -2, 0}, {0, 6, 0}, 0.35, 0.25, lod == 0 ? 6 : 3, false);
            m.colour = CROWN;
            m.ellipsoid({0, 8.5, 0}, 3.5, 3.0, 3.5, lod == 0 ? 8 : 4, lod == 0 ? 6 : 2);
            break;
        case 1:  // conifer
            if (lod == 0) {
                m.colour = TRUNK;
                m.cone({0, -2, 0}, {0, 4, 0}, 0.30, 0.20, 6, false);
                m.colour = NEEDLES;
                m.cone({0, 2, 0}, {0, 9, 0}, 3.0, 0.0, 8, true);
                m.cone({0, 6, 0}, {0, 12, 0}, 2.3, 0.0, 8, true);
                m.cone({0, 10, 0}, {0, 15.5, 0}, 1.5, 0.0, 8, true);
            } else {
                m.colour = NEEDLES;
                m.cone({0, 1.5, 0}, {0, 15.5, 0}, 2.8, 0.0, 4, true);
            }
            break;
        case 2:  // shrub
            if (look.shrubs) {
                // V3D-04 7: three lobes, smooth, coloured by height.
                for (const ShrubLobe& lobe : SHRUB_LOBES) {
                    m.smooth_ellipsoid({lobe.cx, lobe.cy, lobe.cz}, lobe.rx, lobe.ry, lobe.rz, SHRUB_SIDES, SHRUB_RINGS);
                }
            } else {
                m.colour = SHRUB;
                m.ellipsoid({0, 0.5, 0}, 1.2, 0.9, 1.2, 6, 4);
            }
            break;
        case 3:  // reed clump
            m.colour = REED;
            m.cone({0, -0.5, 0}, {0, 2.2, 0}, 0.12, 0.0, 4, false);
            m.cone({0.4, -0.5, 0.2}, {0.5, 1.9, 0.25}, 0.12, 0.0, 4, false);
            m.cone({-0.3, -0.5, 0.35}, {-0.4, 2.0, 0.45}, 0.12, 0.0, 4, false);
            break;
        case 4:  // snag
            m.colour = SNAG;
            m.cone({0, -2, 0}, {0, 7, 0}, 0.30, 0.08, 5, false);
            m.cone({0, 4, 0}, {1.8, 6, 0}, 0.10, 0.02, 4, false);
            m.cone({0, 5.2, 0}, {-1.4, 6.8, 0.3}, 0.09, 0.02, 4, false);
            break;
        default:
            break;
    }
    return m.mesh;
}

}  // namespace unmapped::terrain
