#include "terrain/boulders.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <utility>

#include "terrain/vegetation.hpp"

namespace unmapped::terrain {

namespace {

std::size_t at(int n, int x, int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
}

struct Shape {
    double amp;
    double sx, sy, sz;
    double height;
    bool sphere;  // icosphere (else octahedron) at LOD 0
};

constexpr std::array<Shape, BOULDER_SHAPES> SHAPES = {{
    {0.15, 0.5, 0.4, 0.45, 0.8, true},     // 0 round
    {0.10, 0.5, 0.225, 0.375, 0.45, true},  // 1 slab
    {0.25, 0.5, 0.35, 0.425, 0.7, false},   // 2 angular
}};

struct P3 {
    double x = 0.0, y = 0.0, z = 0.0;
};
P3 operator+(P3 a, P3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
P3 operator-(P3 a, P3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
P3 scale(P3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double dot(P3 a, P3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
P3 cross(P3 a, P3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
P3 unit(P3 a) {
    const double l = std::sqrt(dot(a, a));
    return l > 0.0 ? scale(a, 1.0 / l) : a;
}

struct Poly {
    std::vector<P3> v;                     // in order of first creation
    std::vector<std::array<int, 3>> f;
};

Poly octahedron() {
    Poly p;
    p.v = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    p.f = {{0, 2, 4}, {4, 2, 1}, {1, 2, 5}, {5, 2, 0}, {4, 3, 0}, {1, 3, 4}, {5, 3, 1}, {0, 3, 5}};
    return p;
}

Poly icosahedron() {
    Poly p;
    const double t = (1.0 + std::sqrt(5.0)) / 2.0;
    const P3 raw[12] = {{-1, t, 0}, {1, t, 0}, {-1, -t, 0}, {1, -t, 0}, {0, -1, t}, {0, 1, t},
                        {0, -1, -t}, {0, 1, -t}, {t, 0, -1}, {t, 0, 1}, {-t, 0, -1}, {-t, 0, 1}};
    for (const P3& r : raw) {
        p.v.push_back(unit(r));
    }
    p.f = {{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
           {11, 10, 2}, {10, 7, 6}, {7, 1, 8},  {3, 9, 4},  {3, 4, 2},   {3, 2, 6}, {3, 6, 8},
           {3, 8, 9},  {4, 9, 5},  {2, 4, 11}, {6, 2, 10}, {8, 6, 7},   {9, 8, 1}};
    return p;
}

// One subdivision: each triangle into four, midpoints on the unit sphere,
// new vertices appended in order of first creation (faces in order, edges
// a-b, b-c, c-a).
Poly subdivide(const Poly& in) {
    Poly out;
    out.v = in.v;
    std::map<std::pair<int, int>, int> mid;
    auto midpoint = [&](int a, int b) {
        const std::pair<int, int> key = a < b ? std::make_pair(a, b) : std::make_pair(b, a);
        const auto it = mid.find(key);
        if (it != mid.end()) {
            return it->second;
        }
        out.v.push_back(unit(scale(out.v[static_cast<std::size_t>(a)] + out.v[static_cast<std::size_t>(b)], 0.5)));
        const int id = static_cast<int>(out.v.size()) - 1;
        mid.emplace(key, id);
        return id;
    };
    for (const auto& t : in.f) {
        const int ab = midpoint(t[0], t[1]);
        const int bc = midpoint(t[1], t[2]);
        const int ca = midpoint(t[2], t[0]);
        out.f.push_back({t[0], ab, ca});
        out.f.push_back({t[1], bc, ab});
        out.f.push_back({t[2], ca, bc});
        out.f.push_back({ab, bc, ca});
    }
    return out;
}

}  // namespace

std::uint32_t boulder_density(const BoulderInputs& in, int x, int y) {
    const int n = in.layout->n;
    const std::size_t c = at(n, x, y);
    const GroundCells& cells = *in.cells;
    const std::uint8_t type = cells.type[c];
    if (type < 1 || type > 10 || cells.painted[c] > 0 || (!in.water.empty() && in.water[c] != 0) || in.elevation[c] <= 0) {
        return 0;
    }
    const std::int64_t a = in.plan != nullptr ? in.plan->factors.a_cell[c] : 0;
    const std::int64_t base = std::clamp<std::int64_t>((a - BOULDER_A0_CM) * 192 / (A_MAX_CM - BOULDER_A0_CM), 0, 192) + static_cast<std::int64_t>(cells.rock[c]) * 64 / 255;
    const std::int64_t n_pad =
        in.plan != nullptr ? pad_noise_factor(*in.plan, 16 * static_cast<std::int64_t>(x), 16 * static_cast<std::int64_t>(y)) : 1000;
    return static_cast<std::uint32_t>(base * n_pad / 1000);
}

std::vector<Boulder> place_boulders(const BoulderInputs& in, int x0, int y0, int x1, int y1) {
    if (in.layout == nullptr || in.cells == nullptr || in.field == nullptr || in.cells->type.empty()) {
        return {};
    }
    const int n = in.layout->n;
    x0 = std::clamp(x0, 0, n);
    x1 = std::clamp(x1, 0, n);
    y0 = std::clamp(y0, 0, n);
    y1 = std::clamp(y1, 0, n);
    const std::uint64_t key0 = scatter_key(in.seed);
    const float lo = -8.0f * static_cast<float>(n);
    const float hi = 8.0f * static_cast<float>(n) - 16.0f;
    std::vector<Boulder> out;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::uint32_t q = boulder_density(in, x, y);
            if (q == 0) {
                continue;
            }
            const std::uint64_t hb = mix64(key0 ^ ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
                                                   static_cast<std::uint64_t>(static_cast<std::uint32_t>(y))));
            const std::uint64_t hc = mix64(hb + 64 * BOULDER_SLOT);
            const std::uint32_t count = (q >> 8) + ((hc & 0xFF) < (q & 0xFF) ? 1u : 0u);
            for (std::uint64_t k = 0; k < count; ++k) {
                const std::uint64_t h = mix64(hb + 64 * BOULDER_SLOT + 1 + k);
                const float X = 16.0f * static_cast<float>(x - n / 2) + cell_jitter(mix64(h ^ JITTER_KEY_X));
                const float Z = 16.0f * static_cast<float>(y - n / 2) + cell_jitter(mix64(h ^ JITTER_KEY_Z));
                if (X < lo || X > hi || Z < lo || Z > hi) {
                    continue;
                }
                Boulder b;
                b.x = X;
                b.z = Z;
                b.tint = static_cast<std::uint8_t>(h & 0xFF);
                b.yaw1024 = static_cast<std::uint16_t>((h >> 20) & 0x3FF);
                const std::int64_t u = static_cast<std::int64_t>((h >> 30) & 0x3FF);
                const std::int64_t u4 = u * u * u * u;
                constexpr std::int64_t full = 1023LL * 1023LL * 1023LL * 1023LL;
                const std::int64_t d_mm = 500 + 5500 * u4 / full;
                b.d = static_cast<float>(d_mm) / 1000.0f;
                b.shape = static_cast<std::uint8_t>(((h >> 40) & 0xFF) % 3);
                b.tilt_x = (static_cast<float>((h >> 48) & 0xFF) - 127.5f) * 20.0f / 127.5f;
                b.tilt_z = (static_cast<float>((h >> 56) & 0xFF) - 127.5f) * 20.0f / 127.5f;
                b.rock_id = in.rock_look && !in.cells->rock_variant.empty() ? in.cells->rock_variant[at(n, x, y)] : 11;
                const float r = 0.5f * b.d;
                const GroundField& g = *in.field;
                float ground = ground_height(g, X, Z);
                ground = std::min(ground, ground_height(g, X + r, Z));
                ground = std::min(ground, ground_height(g, X - r, Z));
                ground = std::min(ground, ground_height(g, X, Z + r));
                ground = std::min(ground, ground_height(g, X, Z - r));
                b.y = ground - 0.3f * boulder_height(b.shape) * b.d;
                out.push_back(b);
            }
        }
    }
    return out;
}

std::array<std::vector<Boulder>, BOULDER_SHAPES> near_boulders(const BoulderInputs& in, int i, int j) {
    std::array<std::vector<Boulder>, BOULDER_SHAPES> out;
    for (const Boulder& b : place_boulders(in, 16 * i, 16 * j, 16 * i + 16, 16 * j + 16)) {
        out[b.shape].push_back(b);
    }
    return out;
}

float boulder_height(int shape) noexcept {
    return shape >= 0 && shape < BOULDER_SHAPES ? static_cast<float>(SHAPES[static_cast<std::size_t>(shape)].height) : 0.0f;
}

MeshData build_boulder_mesh(int shape, int lod) {
    MeshData out;
    if (shape < 0 || shape >= BOULDER_SHAPES || lod < 0 || lod > 1) {
        return out;
    }
    const Shape& sh = SHAPES[static_cast<std::size_t>(shape)];
    const Poly poly = lod == 1 ? octahedron() : sh.sphere ? subdivide(icosahedron()) : subdivide(octahedron());
    // Displace each vertex once (index = order of first creation), scale.
    std::vector<P3> v(poly.v.size());
    for (std::size_t p = 0; p < poly.v.size(); ++p) {
        const double hsh = static_cast<double>(hash32(static_cast<std::uint32_t>(shape), static_cast<std::uint32_t>(p), ROCK_MESH_KEY));
        const double k = 1.0 + sh.amp * (2.0 * hsh / 4294967296.0 - 1.0);
        v[p] = P3{poly.v[p].x * k * sh.sx, poly.v[p].y * k * sh.sy, poly.v[p].z * k * sh.sz};
    }
    // Unit size: largest horizontal extent 1, lowest y 0, highest height.
    P3 lo = v[0];
    P3 hi = v[0];
    for (const P3& p : v) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    const double hs = 1.0 / std::max(hi.x - lo.x, hi.z - lo.z);
    const double vs = sh.height / (hi.y - lo.y);
    const double cx = 0.5 * (lo.x + hi.x);
    const double cz = 0.5 * (lo.z + hi.z);
    for (P3& p : v) {
        p = P3{(p.x - cx) * hs, (p.y - lo.y) * vs, (p.z - cz) * hs};
    }
    const P3 centre{0.0, 0.5 * sh.height, 0.0};
    for (std::size_t face = 0; face < poly.f.size(); ++face) {
        P3 a = v[static_cast<std::size_t>(poly.f[face][0])];
        P3 b = v[static_cast<std::size_t>(poly.f[face][1])];
        P3 c = v[static_cast<std::size_t>(poly.f[face][2])];
        const P3 outward = scale(a + b + c, 1.0 / 3.0) - centre;
        P3 nrm = cross(b - a, c - a);
        if (dot(nrm, outward) > 0.0) {
            std::swap(b, c);
            nrm = cross(b - a, c - a);
        }
        const P3 fn = unit(scale(nrm, -1.0));
        const std::uint32_t g =
            224u + (hash32(static_cast<std::uint32_t>(shape), static_cast<std::uint32_t>(face), ROCK_MESH_KEY + 1u) >> 24) * 31u / 255u;
        const std::uint32_t rgba = (g << 24) | (g << 16) | (g << 8) | 0xFFu;
        for (const P3& p : {a, b, c}) {
            out.positions.push_back(static_cast<float>(p.x));
            out.positions.push_back(static_cast<float>(p.y));
            out.positions.push_back(static_cast<float>(p.z));
            out.normals.push_back(static_cast<float>(fn.x));
            out.normals.push_back(static_cast<float>(fn.y));
            out.normals.push_back(static_cast<float>(fn.z));
            out.colours.push_back(rgba);
        }
    }
    return out;
}

std::array<float, 3> boulder_colour(const Boulder& b) noexcept {
    std::uint32_t rgb = 0x6E6A64;
    if (b.rock_id == 15) {
        rgb = 0x8C8880;
    } else if (b.rock_id == 16) {
        rgb = 0x7A6A58;
    }
    const float v = static_cast<float>(216 + static_cast<int>(b.tint) * 39 / 255) / 255.0f;
    return {static_cast<float>((rgb >> 16) & 0xFF) / 255.0f * v, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f * v,
            static_cast<float>(rgb & 0xFF) / 255.0f * v};
}

std::vector<float> boulder_buffer(std::span<const Boulder> boulders) {
    std::vector<float> out(16 * boulders.size());
    float* p = out.data();
    constexpr double PI = 3.14159265358979323846;
    for (const Boulder& b : boulders) {
        const double yaw = static_cast<double>(b.yaw1024) * 2.0 * PI / 1024.0;
        const double tx = static_cast<double>(b.tilt_x) * PI / 180.0;
        const double tz = static_cast<double>(b.tilt_z) * PI / 180.0;
        const double cy = std::cos(yaw), sy = std::sin(yaw);
        const double cx = std::cos(tx), sx = std::sin(tx);
        const double cz = std::cos(tz), sz = std::sin(tz);
        // RotY * RotX * RotZ, rows.
        const double ry[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
        const double rx[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
        const double rz[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
        double m1[3][3] = {};
        double m[3][3] = {};
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                for (int k = 0; k < 3; ++k) {
                    m1[r][c] += ry[r][k] * rx[k][c];
                }
            }
        }
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                for (int k = 0; k < 3; ++k) {
                    m[r][c] += m1[r][k] * rz[k][c];
                }
            }
        }
        const double d = static_cast<double>(b.d);
        const float origin[3] = {b.x, b.y, b.z};
        for (int r = 0; r < 3; ++r) {
            p[4 * r + 0] = static_cast<float>(m[r][0] * d);
            p[4 * r + 1] = static_cast<float>(m[r][1] * d);
            p[4 * r + 2] = static_cast<float>(m[r][2] * d);
            p[4 * r + 3] = origin[r];
        }
        const auto col = boulder_colour(b);
        p[12] = col[0];
        p[13] = col[1];
        p[14] = col[2];
        p[15] = 1.0f;
        p += 16;
    }
    return out;
}

}  // namespace unmapped::terrain
