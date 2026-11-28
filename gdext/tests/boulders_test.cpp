// Spec V3D-06 15.1, U61 and U62: boulder placement and meshes (boulders.hpp).

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "terrain/boulders.hpp"
#include "terrain/vegetation.hpp"

using namespace unmapped::terrain;

namespace {

constexpr int N = 512;

std::size_t at(int x, int y) { return static_cast<std::size_t>(y) * N + static_cast<std::size_t>(x); }

struct Fixture {
    Layout layout;
    std::vector<std::int16_t> e;
    std::vector<std::uint16_t> depth;
    std::vector<std::uint8_t> water;
    std::vector<std::uint8_t> timber;
    GroundCells cells;
    std::vector<std::uint32_t> control;
    std::vector<std::uint8_t> colour;
    DetailPlan plan;
    GroundField field;
    BoulderInputs in;

    Fixture() {
        std::string problem;
        layout = *layout_for(N, N, problem);
        const std::size_t t = static_cast<std::size_t>(N) * N;
        e.assign(t, 500);
        depth.assign(t, 0);
        water.assign(t, 0);
        timber.assign(t, 128);
        cells.type.assign(t, 1);
        cells.painted.assign(t, 0);
        cells.rock.assign(t, 0);
        cells.snow.assign(t, 0);
        cells.rock_variant.assign(t, 11);
        cells.snow_line.assign(t, 0);
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                const int d = std::abs(x - 384);
                if (d < 60) {
                    e[at(x, y)] = static_cast<std::int16_t>(500 + (60 - d) * 190);
                    cells.type[at(x, y)] = d < 24 ? 9 : 7;
                    cells.rock[at(x, y)] = d < 24 ? 255 : 0;
                    cells.rock_variant[at(x, y)] = d < 12 ? 15 : 11;
                }
                if (y < 20) {
                    water[at(x, y)] = 3;
                    e[at(x, y)] = -100;
                    cells.type[at(x, y)] = 14;
                }
            }
        }
        control = build_control(layout, cells);
        colour.assign(4 * t, 128);
        plan = make_detail_plan(layout, 4, 42, e, depth, cells, water, std::vector<PadPlace>{{200, 300, 64}}, control,
                                colour, true, Look{});
        field = build_ground_field(plan);
        in.layout = &layout;
        in.elevation = e;
        in.water = water;
        in.cells = &cells;
        in.plan = &plan;
        in.field = &field;
        in.seed = 42;
    }
};

const Fixture& fixture() {
    static const Fixture f;
    return f;
}

}  // namespace

TEST_CASE("U61 boulder placement") {
    const Fixture& f = fixture();
    const BoulderInputs& in = f.in;
    // Counts within 4 standard deviations of the expected sum of q / 256.
    std::map<int, std::pair<double, double>> expect;  // type: (expected, variance)
    std::map<int, double> got;
    for (int y = 20; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const double q = static_cast<double>(boulder_density(in, x, y)) / 256.0;
            const double frac = q - std::floor(q);
            expect[f.cells.type[at(x, y)]].first += q;
            expect[f.cells.type[at(x, y)]].second += frac * (1.0 - frac);
        }
    }
    const std::vector<Boulder> all = place_boulders(in, 0, 0, N, N);
    for (const Boulder& b : all) {
        const CellHit c = world_to_cell(f.layout, b.x, b.z);
        got[f.cells.type[at(c.x, c.y)]] += 1.0;
    }
    for (const auto& [type, ev] : expect) {
        CHECK(std::abs(got[type] - ev.first) <= 4.0 * std::sqrt(ev.second) + 1.0);
    }
    for (int y = 20 + 4; y < N; y += 3) {
        for (int x = 0; x < 250; x += 3) {
            CHECK(boulder_density(in, x, y) == 0u);  // plains far from the ridge: none
        }
    }
    CHECK(expect[9].first > 1000.0);
    // Exclusions: none on the sea, none within the pad's flat part.
    for (const Boulder& b : all) {
        const CellHit c = world_to_cell(f.layout, b.x, b.z);
        CHECK(f.water[at(c.x, c.y)] == 0);
        CHECK(std::abs(c.x - 200) + std::abs(c.y - 300) > 4);
        // Jitter < 8 m: the boulder's nearest vertex is its own cell.
        CHECK(c.inside);
    }
    // Diameters: median 0.84 m, 5 % above 5 m.
    std::vector<float> d;
    for (const Boulder& b : all) {
        d.push_back(b.d);
    }
    REQUIRE(d.size() > 1000);
    std::sort(d.begin(), d.end());
    CHECK(std::abs(d[d.size() / 2] - 0.84f) <= 0.05f);
    const auto big = static_cast<double>(std::count_if(d.begin(), d.end(), [](float v) { return v > 5.0f; }));
    CHECK(std::abs(big / static_cast<double>(d.size()) - 0.05) <= 0.01);
    // Every boulder's lowest point at or below the ground at its five points.
    for (std::size_t k = 0; k < all.size(); k += 7) {
        const Boulder& b = all[k];
        const float r = 0.5f * b.d;
        for (const auto& p : {std::make_pair(0.0f, 0.0f), std::make_pair(r, 0.0f), std::make_pair(-r, 0.0f),
                              std::make_pair(0.0f, r), std::make_pair(0.0f, -r)}) {
            CHECK(b.y <= ground_height(f.field, b.x + p.first, b.z + p.second));
        }
    }
    // Two calls identical; vegetation candidates unchanged by boulders.
    const std::vector<Boulder> again = place_boulders(in, 0, 0, N, N);
    REQUIRE(again.size() == all.size());
    for (std::size_t k = 0; k < all.size(); ++k) {
        CHECK(again[k].x == all[k].x);
        CHECK(again[k].y == all[k].y);
        CHECK(again[k].d == all[k].d);
    }
    VegetationInputs vi;
    vi.layout = &f.layout;
    std::vector<float> h(static_cast<std::size_t>(N) * N, 50.0f);
    vi.heights = h;
    vi.elevation = f.e;
    vi.timber = f.timber;
    vi.water = f.water;
    vi.cells = &f.cells;
    vi.seed = 42;
    vi.ground = &f.field;
    const auto c1 = place_candidates(vi, 300, 100, 420, 200);
    (void)near_boulders(in, 20, 8);
    const auto c2 = place_candidates(vi, 300, 100, 420, 200);
    REQUIRE(c1.size() == c2.size());
    for (std::size_t k = 0; k < c1.size(); ++k) {
        CHECK(c1[k].instance.x == c2[k].instance.x);
        CHECK(c1[k].thin == c2[k].thin);
    }
    CHECK(BOULDER_SLOT == 5);
}

TEST_CASE("U62 boulder meshes") {
    const int tris[3] = {80, 80, 32};
    for (int shape = 0; shape < BOULDER_SHAPES; ++shape) {
        for (int lod = 0; lod < 2; ++lod) {
            const MeshData m = build_boulder_mesh(shape, lod);
            CHECK(m.triangles() == static_cast<std::size_t>(lod == 0 ? tris[shape] : 8));
            // Closed: every edge in two triangles (positions compared exactly).
            std::map<std::pair<std::array<float, 3>, std::array<float, 3>>, int> edges;
            const auto vtx = [&m](std::size_t i) {
                return std::array<float, 3>{m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]};
            };
            float lo = 1e9f, hi = -1e9f, x0 = 1e9f, x1 = -1e9f, z0 = 1e9f, z1 = -1e9f;
            for (std::size_t t = 0; t < m.triangles(); ++t) {
                for (std::size_t k = 0; k < 3; ++k) {
                    auto a = vtx(3 * t + k), b = vtx(3 * t + (k + 1) % 3);
                    if (b < a) {
                        std::swap(a, b);
                    }
                    ++edges[{a, b}];
                }
                // Outward unit normal, clockwise seen from outside.
                const auto a = vtx(3 * t), b = vtx(3 * t + 1), c = vtx(3 * t + 2);
                const float nx = m.normals[9 * t], ny = m.normals[9 * t + 1], nz = m.normals[9 * t + 2];
                CHECK(std::abs(nx * nx + ny * ny + nz * nz - 1.0f) < 1e-4f);
                const float cx = (a[0] + b[0] + c[0]) / 3.0f, cy = (a[1] + b[1] + c[1]) / 3.0f - 0.5f * boulder_height(shape),
                            cz = (a[2] + b[2] + c[2]) / 3.0f;
                CHECK(nx * cx + ny * cy + nz * cz > 0.0f);
                const std::uint32_t g = m.colours[3 * t] >> 24;
                CHECK(g >= 224u);
                CHECK(g <= 255u);
            }
            for (std::size_t i = 0; i < m.colours.size(); ++i) {
                lo = std::min(lo, m.positions[3 * i + 1]);
                hi = std::max(hi, m.positions[3 * i + 1]);
                x0 = std::min(x0, m.positions[3 * i]);
                x1 = std::max(x1, m.positions[3 * i]);
                z0 = std::min(z0, m.positions[3 * i + 2]);
                z1 = std::max(z1, m.positions[3 * i + 2]);
            }
            for (const auto& [edge, count] : edges) {
                CHECK(count == 2);
            }
            CHECK(std::abs(lo) < 1e-5f);
            CHECK(std::abs(hi - boulder_height(shape)) < 1e-5f);
            CHECK(std::abs(std::max(x1 - x0, z1 - z0) - 1.0f) < 1e-5f);
        }
    }
}
