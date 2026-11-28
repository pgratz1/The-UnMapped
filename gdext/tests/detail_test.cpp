// Spec V3D-06 15.1, U53 to U60 and U63 to U66: the ground detail of the 3D
// terrain preview (detail.hpp) on hand-made coarse worlds of 512 cells.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "terrain/detail.hpp"
#include "terrain/markers3d.hpp"
#include "terrain/vegetation.hpp"

using namespace unmapped::terrain;

namespace {

constexpr int N = 512;

Layout layout512() {
    std::string problem;
    const auto layout = layout_for(N, N, problem);
    REQUIRE(layout.has_value());
    return *layout;
}

std::int64_t abs64(std::int64_t v) { return v < 0 ? -v : v; }

std::size_t at(int x, int y) { return static_cast<std::size_t>(y) * N + static_cast<std::size_t>(x); }

// A coarse world: elevation (dm), water depth, water layer and ground cells.
struct World {
    Layout layout = layout512();
    std::vector<std::int16_t> e = std::vector<std::int16_t>(static_cast<std::size_t>(N) * N, 500);
    std::vector<std::uint16_t> depth = std::vector<std::uint16_t>(static_cast<std::size_t>(N) * N, 0);
    std::vector<std::uint8_t> water = std::vector<std::uint8_t>(static_cast<std::size_t>(N) * N, 0);
    GroundCells cells;
    World() {
        const std::size_t t = static_cast<std::size_t>(N) * N;
        cells.type.assign(t, 1);
        cells.painted.assign(t, 0);
        cells.rock.assign(t, 0);
        cells.snow.assign(t, 0);
        cells.rock_variant.assign(t, 11);
        cells.snow_line.assign(t, 0);
    }
    DetailPlan plan(int s, std::uint64_t seed = 42, std::vector<PadPlace> places = {}) const {
        control = build_control(layout, cells);
        colour.assign(4 * cells.type.size(), 0);
        for (std::size_t k = 0; k < cells.type.size(); ++k) {
            colour[4 * k] = static_cast<std::uint8_t>(cells.type[k] * 20);
            colour[4 * k + 1] = 100;
            colour[4 * k + 2] = 200;
            colour[4 * k + 3] = 128;
        }
        return make_detail_plan(layout, s, seed, e, depth, cells, water, places, control, colour, true, Look{});
    }
    mutable std::vector<std::uint32_t> control;
    mutable std::vector<std::uint8_t> colour;
};

// Flat grassland, an alpine ridge with heath around it, a lake, a coast and
// a painted river.
World mixed_world() {
    World w;
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            const std::size_t k = at(x, y);
            // A ridge along x = 384: up to 1,200 m, alpine within 24 cells.
            const int d = std::abs(x - 384);
            if (d < 60) {
                w.e[k] = static_cast<std::int16_t>(500 + (60 - d) * 190);
                w.cells.type[k] = d < 24 ? 9 : 7;
            }
            // A coast: sea below y < 32.
            if (y < 32) {
                w.e[k] = -500;
                w.cells.type[k] = 14;
                w.water[k] = 1;
            }
        }
    }
    // A lake of one surface (bed 40 m, surface 52 m) at (100..120, 200..220).
    for (int y = 200; y < 220; ++y) {
        for (int x = 100; x < 120; ++x) {
            w.e[at(x, y)] = 400;
            w.depth[at(x, y)] = 120;
            w.water[at(x, y)] = 2;
            w.cells.type[at(x, y)] = 13;
        }
    }
    // A river along y = 300 from x 40 to 200.
    for (int x = 40; x < 200; ++x) {
        w.cells.painted[at(x, 300)] = 5;
        w.water[at(x, 300)] = 4;
    }
    return w;
}

// The test's own copy of 6 (U55).
std::uint32_t own_hash(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    std::uint32_t h = a * 0x8DA6B343u + b * 0xD8163841u + c * 0xCB1AB31Fu;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

std::int64_t own_floor(std::int64_t a, std::int64_t b) { return (a % b != 0 && a < 0) ? a / b - 1 : a / b; }

std::int64_t own_noise(std::uint64_t seed, std::int64_t xm, std::int64_t zm, int octaves, std::int64_t ridge,
                       std::int64_t billow) {
    const std::int64_t lambdas[6] = {128, 64, 32, 16, 8, 4};
    const std::int64_t weights[6] = {361, 252, 177, 124, 86, 60};
    const std::uint32_t base = static_cast<std::uint32_t>(mix64(seed ^ 0x64657461696C3036ull) >> 32);
    std::int64_t sum = 0;
    for (int o = 0; o < octaves; ++o) {
        const std::uint32_t key = base ^ (0x9E3779B9u * static_cast<std::uint32_t>(o + 1));
        const std::int64_t l = lambdas[o];
        auto val = [key](std::int64_t p, std::int64_t q) {
            return static_cast<std::int64_t>(own_hash(static_cast<std::uint32_t>(p), static_cast<std::uint32_t>(q), key) >> 16) - 32768;
        };
        auto sm = [](std::int64_t t) { return (t * t * (196608 - 2 * t)) >> 32; };
        const std::int64_t L = xm / l, K = zm / l;
        const std::int64_t sx = sm((xm % l) * 65536 / l), sy = sm((zm % l) * 65536 / l);
        const std::int64_t top = val(L, K) * (65536 - sx) + val(L + 1, K) * sx;
        const std::int64_t bot = val(L, K + 1) * (65536 - sx) + val(L + 1, K + 1) * sx;
        const std::int64_t n = (top * (65536 - sy) + bot * sy) >> 32;
        const std::int64_t r = std::max<std::int64_t>(-32768, 32767 - 2 * abs64(n));
        const std::int64_t b = 2 * abs64(n) - 32768;
        sum += weights[o] * (n + own_floor(ridge * (r - n) + billow * (b - n), 1000));
    }
    return own_floor(sum, 1000);
}

}  // namespace

TEST_CASE("U53 Catmull-Rom weights") {
    for (const int f : {2, 4, 8}) {
        for (int a = 0; a < f; ++a) {
            const auto w = catmull_rom_weights(a, f);
            CHECK(w[0] + w[1] + w[2] + w[3] == 2 * f * f * f);
        }
    }
    CHECK(catmull_rom_weights(0, 4) == std::array<std::int64_t, 4>{0, 128, 0, 0});
    CHECK(catmull_rom_weights(1, 4) == std::array<std::int64_t, 4>{-9, 111, 29, -3});
    CHECK(catmull_rom_weights(2, 4) == std::array<std::int64_t, 4>{-8, 72, 72, -8});
    CHECK(catmull_rom_weights(3, 4) == std::array<std::int64_t, 4>{-3, 29, 111, -9});
}

TEST_CASE("U54 upsampling") {
    const World w = mixed_world();
    const std::vector<std::int32_t> hd = coarse_decimetres(w.e, w.depth);
    for (const int f : {2, 4}) {
        for (int y = 0; y < N; y += 7) {
            for (int x = 0; x < N; x += 5) {
                CHECK(upsample_mm(hd, N, f, x * f, y * f) == 100 * hd[at(x, y)]);
            }
        }
        for (int yf = 190 * f; yf < 230 * f; ++yf) {
            for (int xf = 90 * f; xf < 130 * f; ++xf) {
                const int i = std::min(xf / f, N - 1), j = std::min(yf / f, N - 1);
                const int i1 = std::min(i + 1, N - 1), j1 = std::min(j + 1, N - 1);
                const std::int32_t c[4] = {hd[at(i, j)], hd[at(i1, j)], hd[at(i, j1)], hd[at(i1, j1)]};
                const std::int64_t u = upsample_mm(hd, N, f, xf, yf);
                CHECK(u >= 100 * *std::min_element(c, c + 4));
                CHECK(u <= 100 * *std::max_element(c, c + 4));
                if (i >= 100 && i1 < 120 && j >= 200 && j1 < 220) {
                    CHECK(u == 52000);  // a lake quad: exactly its surface
                }
            }
        }
    }
    // Flat stays flat; a ramp is reproduced away from the edges; edges clamp.
    std::vector<std::int32_t> flat(static_cast<std::size_t>(N) * N, 700);
    std::vector<std::int32_t> ramp(flat.size());
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            ramp[at(x, y)] = 10 * x;
        }
    }
    for (int k = 0; k < 4000; k += 3) {
        const int xf = (k * 37) % (N * 4), yf = (k * 61) % (N * 4);
        CHECK(upsample_mm(flat, N, 4, xf, yf) == 70000);
        if (xf >= 8 && xf < (N - 2) * 4) {
            CHECK(upsample_mm(ramp, N, 4, xf, yf) == 250 * xf);
        }
    }
    CHECK(upsample_mm(ramp, N, 4, N * 4 - 1, 0) == 100 * 10 * (N - 1));
}

TEST_CASE("U55 noise") {
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{42}}) {
        std::array<std::uint32_t, 6> keys{};
        for (int o = 0; o < 6; ++o) {
            keys[static_cast<std::size_t>(o)] = octave_key(seed, o);
        }
        for (const auto rb : {std::array<int, 2>{0, 0}, std::array<int, 2>{1000, 0}, std::array<int, 2>{0, 600},
                              std::array<int, 2>{300, 300}}) {
            for (int k = 0; k < 64; ++k) {
                const std::int64_t xm = (k * 977) % 8192, zm = (k * 613 + 5) % 8192;
                const std::int32_t n = detail_noise(keys, 5, xm, zm, rb[0], rb[1]);
                CHECK(n == own_noise(seed, xm, zm, 5, rb[0], rb[1]));
                CHECK(n >= -32768);
                CHECK(n <= 32767);
            }
        }
        // Shapes in metres: s = 8 and s = 4 agree at shared positions on the
        // octaves both use.
        for (int k = 0; k < 64; ++k) {
            const std::int64_t xm = 8 * ((k * 131) % 1024), zm = 8 * ((k * 71) % 1024);
            CHECK(detail_noise(keys, octave_count(8), xm, zm, 0, 0) == own_noise(seed, xm, zm, 4, 0, 0));
        }
    }
    CHECK(octave_count(8) == 4);
    CHECK(octave_count(4) == 5);
    CHECK(octave_count(2) == 6);
}

TEST_CASE("U56 factor fields") {
    const World w = mixed_world();
    // Chamfer against brute force on a small source set.
    std::vector<std::uint8_t> src(static_cast<std::size_t>(N) * N, 0);
    src[at(10, 10)] = 1;
    src[at(300, 40)] = 1;
    const std::vector<std::int32_t> d = chamfer_distance(src, N);
    for (int y = 0; y < N; y += 13) {
        for (int x = 0; x < N; x += 11) {
            std::int32_t best = 6 * N;
            for (const auto& p : {std::array<int, 2>{10, 10}, std::array<int, 2>{300, 40}}) {
                const int dx = std::abs(x - p[0]), dy = std::abs(y - p[1]);
                best = std::min(best, 3 * std::max(dx, dy) + std::min(dx, dy));
            }
            CHECK(d[at(x, y)] == best);
        }
    }
    CHECK(detail_ramp(3) == 0);
    CHECK(detail_ramp(4) == 0);
    CHECK(detail_ramp(6) == 500);
    CHECK(detail_ramp(9) == 1000);
    const DetailFactors f = build_detail_factors(w.layout, w.e, w.cells, w.water);
    // Plain grassland far from anything: M 1000, A = AMP_CM.
    CHECK(f.m[at(200, 100)] == 1000);
    CHECK(f.a_cell[at(200, 100)] == DETAIL_AMPLITUDE[1].amp_cm);
    CHECK(f.billow[at(200, 100)] == 600);
    // Alpine at the ridge: capped.
    // 9 x 9 relief 76 m: m_rel (760 - 50) 1000 / 950 = 747; prox 1000.
    CHECK(f.m[at(384, 200)] == 1000 + 747 + 500);
    CHECK(f.a_cell[at(384, 200)] == A_MAX_CM);
    CHECK(f.ridge[at(384, 200)] == 1000);
    // Zero on wet cells and their neighbours (diagonal included).
    for (int x = 99; x <= 120; ++x) {
        CHECK(f.a_cell[at(x, 199)] == 0);
        CHECK(f.w_noise[at(x, 210)] == 0);
    }
    CHECK(f.a_cell[at(120, 220)] == 0);
    CHECK(f.a_cell[at(80, 301)] == 0);
    CHECK(f.a_cell[at(80, 299)] == 0);
    CHECK(f.a_cell[at(80, 302)] > 0);
    // Ab at a border: mean of the 5 x 5 window.
    CHECK(f.a_cell[at(326, 100)] > DETAIL_AMPLITUDE[1].amp_cm);
    CHECK(DETAIL_AMPLITUDE[9].amp_cm == 600);
    CHECK(DETAIL_AMPLITUDE[4].amp_cm == 40);
}

TEST_CASE("U57 field: wet quads, sea guard, recorded hashes") {
    const World w = mixed_world();
    for (const int s : {8, 4}) {
        const DetailPlan p = w.plan(s);
        const int f = 16 / s;
        // Every vertex of a quad touching a wet cell is up_mm.
        for (int y = 195; y < 225; ++y) {
            for (int x = 95; x < 125; ++x) {
                bool wet = false;
                for (int q = 0; q < 4; ++q) {
                    const int cx = x + (q & 1), cy = y + (q >> 1);
                    wet = wet || w.water[at(cx, cy)] != 0;
                }
                if (!wet) {
                    continue;
                }
                for (int b = 0; b <= f; ++b) {
                    for (int a = 0; a <= f; ++a) {
                        const int xf = x * f + a, yf = y * f + b;
                        CHECK(field_mm(p, xf, yf) == upsample_mm(p.hd, N, f, xf, yf));
                    }
                }
            }
        }
        // The sea guard near the coast.
        for (int yf = 28 * f; yf < 40 * f; ++yf) {
            for (int xf = 0; xf < 64 * f; xf += 3) {
                const std::int64_t up = upsample_mm(p.hd, N, f, xf, yf);
                CHECK(field_mm(p, xf, yf) >= std::min<std::int64_t>(up, 200));
            }
        }
    }
    // Recorded FNV-1a 64 of the field's float bits, seed 42.
    const GroundField f8 = build_ground_field(w.plan(8));
    const GroundField f4 = build_ground_field(w.plan(4));
    const std::uint64_t h8 = fnv1a64(f8.heights.data(), f8.heights.size() * sizeof(float));
    const std::uint64_t h4 = fnv1a64(f4.heights.data(), f4.heights.size() * sizeof(float));
    // Recorded with GCC 2026-11-28; the same on Clang and MSVC (integer only).
    CHECK(h8 == 14621603434702135909ull);
    CHECK(h4 == 13664493670390306587ull);
}

TEST_CASE("U58 pads") {
    World w = mixed_world();
    // A pad on flat grass and one on the ridge's flank; a pad on the lake is dropped.
    const std::vector<PadPlace> places = {{200, 400, 64}, {200 + 5, 400, 40}, {110, 210, 32}, {340, 100, 40}};
    const DetailPlan p = w.plan(4, 42, places);
    REQUIRE(p.pads.size() == 3);
    const Pad& a = p.pads[0];
    CHECK(a.level_pm == 1000);
    CHECK(field_mm(p, 4 * 200, 4 * 400) != 0);
    // Within r0 of the first pad and outside the second's reach: exactly P.
    for (int dz = -64; dz <= 64; dz += 4) {
        const int xf = (200 * 16 - 64) / 4, yf = (400 * 16 + dz) / 4;
        CHECK(field_mm(p, xf, yf) == a.p_mm);
    }
    // The flank pad: rel > 6 m, LEVEL 1000 * 60 / rel.
    const Pad& c = p.pads[2];
    CHECK(c.rel_dm > PAD_FLAT_DM);
    CHECK(c.level_pm == 1000 * PAD_FLAT_DM / c.rel_dm);
    // Adjacent vertices across the overlap stay close.
    std::int64_t worst = 0;
    for (int xf = 4 * 180; xf < 4 * 230; ++xf) {
        worst = std::max(worst, abs64(field_mm(p, xf + 1, 1600) - field_mm(p, xf, 1600)));
    }
    CHECK(worst < 400);
    // r0 per kind.
    MarkerRecord r;
    r.kind = MarkerKind::lair;
    CHECK(pad_radius(r) == 16);
    r.kind = MarkerKind::outpost;
    CHECK(pad_radius(r) == 20);
    r.kind = MarkerKind::mine;
    CHECK(pad_radius(r) == 24);
    r.kind = MarkerKind::tower;
    CHECK(pad_radius(r) == 8);
    r.library = true;
    CHECK(pad_radius(r) == 20);
    r.kind = MarkerKind::settlement;
    r.settlement_kind = 4;
    r.population = 0;
    CHECK(pad_radius(r) == 16 * marker_extent(r) + 24);
    r.settlement_kind = 2;
    CHECK(pad_radius(r) == 16 * marker_extent(r) + 16);
    CHECK(pad_smooth(0) == 0);
    CHECK(pad_smooth(1000) == 1000);
    CHECK(pad_rho(a, 64) == 1000);
    CHECK(pad_rho(a, 64 + PAD_FALL) == 0);
}

TEST_CASE("U59 slope bound and walkability") {
    const World w = mixed_world();
    const DetailPlan p = w.plan(4);
    const int v = p.v;
    std::int64_t worst_flat = 0;
    std::uint64_t steep = 0, total = 0;
    for (int yf = 4 * 60; yf < 4 * 500; yf += 3) {
        for (int xf = 4 * 140; xf < 4 * 300; xf += 3) {
            if (xf + 1 >= v || yf + 1 >= v) {
                continue;
            }
            const auto d = [&](int x, int y) { return field_mm(p, x, y) - upsample_mm(p.hd, N, 4, x, y); };
            const std::int64_t gx = d(xf + 1, yf) - d(xf - 1, yf);
            const std::int64_t gz = d(xf, yf + 1) - d(xf, yf - 1);
            worst_flat = std::max(worst_flat, std::max(abs64(gx), abs64(gz)));
            const std::int64_t hx = field_mm(p, xf + 1, yf) - field_mm(p, xf - 1, yf);
            const std::int64_t hz = field_mm(p, xf, yf + 1) - field_mm(p, xf, yf - 1);
            steep += fine_rock(isqrt64(hx * hx + hz * hz), 4) > 0 ? 1u : 0u;
            ++total;
        }
    }
    // Grassland: |grad detail| <= 0.186 A over 8 m (the central difference spans 2 s).
    CHECK(static_cast<double>(worst_flat) <= 0.186 * 1200.0 * 8.0 * 1.6);
    CHECK(steep * 1000 <= 10 * total);
}

TEST_CASE("U60 fine control and colour") {
    World w = mixed_world();
    const DetailPlan p = w.plan(4);
    const DetailBuffers b = build_detail_buffers(p, 1, true);
    for (std::size_t k = 0; k < b.control.size(); k += 97) {
        const std::uint32_t c = b.control[k];
        const std::uint32_t base = c >> 27, overlay = (c >> 22) & 0x1F;
        const bool base_ok = base <= 10 || base == 13 || base == 14;
        CHECK(base_ok);
        CHECK(overlay <= 16);
        CHECK((c & 0x3FFFu) == 0u);
    }
    // A uniform area: colour equals the coarse colour, control the coarse control.
    const std::size_t k = static_cast<std::size_t>(4 * 400) * static_cast<std::size_t>(p.v) + 4 * 200;
    CHECK(b.colour[4 * k] == 20);
    CHECK(b.colour[4 * k + 1] == 100);
    CHECK((b.control[k] >> 27) == 1u);
    CHECK(fine_rock(static_cast<std::int64_t>(0.69 * 8000), 4) == 0);
    CHECK(std::abs(static_cast<int>(fine_rock(static_cast<std::int64_t>(0.85 * 8000), 4)) - 128) <= 3);
    CHECK(fine_rock(8000, 4) == 255);
    for (int y = 0; y < 2048; y += 37) {
        for (int x = 0; x < 2048; x += 41) {
            const auto wp = fine_warp(p, x, y);
            CHECK(std::abs(wp[0]) <= 64);
            CHECK(std::abs(wp[1]) <= 64);
        }
    }
}

TEST_CASE("U63 ground_height and ground_top") {
    const World w = mixed_world();
    const GroundField f = build_ground_field(w.plan(4));
    for (int k = 0; k < 1000; ++k) {
        const int xf = (k * 7919) % f.layout.n, yf = (k * 104729) % f.layout.n;
        const Vec2 p = cell_to_world(f.layout, xf, yf);
        const float h = f.heights[static_cast<std::size_t>(yf) * static_cast<std::size_t>(f.layout.n) + static_cast<std::size_t>(xf)];
        CHECK(ground_height(f, p.x, p.z) == h);
        const float X = p.x + 1.3f, Z = p.z + 2.1f;
        CHECK(ground_top(f, X, Z) >= ground_height(f, X, Z));
        CHECK(ground_height(f, X, Z) <= sample_bilinear(f.layout, f.heights, X, Z) + 1e-3f);
    }
    std::vector<float> coarse(static_cast<std::size_t>(N) * N);
    for (std::size_t k = 0; k < coarse.size(); ++k) {
        coarse[k] = static_cast<float>(w.e[k]) / 10.0f;
    }
    const GroundField off = coarse_field(w.layout, coarse);
    CHECK(ground_height(off, 100.5f, -37.2f) == ground_low(w.layout, coarse, 100.5f, -37.2f));
    CHECK(ground_top(off, 100.5f, -37.2f) == ground_clamp(w.layout, coarse, 100.5f, -37.2f));
}

TEST_CASE("U64 options and constants") {
    CHECK(parse_ground_detail("off") == 0);
    CHECK(parse_ground_detail("8") == 8);
    CHECK(parse_ground_detail("4") == 4);
    CHECK(parse_ground_detail("2") == 2);
    CHECK_FALSE(parse_ground_detail("16"));
    CHECK_FALSE(parse_ground_detail("3"));
    CHECK_FALSE(parse_ground_detail("x"));
    CHECK_FALSE(parse_ground_detail(""));
    CHECK(parse_detail_threads("0") == 0);
    CHECK(parse_detail_threads("16") == 16);
    CHECK_FALSE(parse_detail_threads("17"));
    CHECK(ground_detail_refusal("3") == "--ground-detail 3: off, 8, 4 or 2");
    CHECK(detail_threads_refusal("17") == "--detail-threads 17: a whole number from 0 to 16");
    CHECK(A_MAX_CM == 1200);
    CHECK(PAD_FLAT_DM == 60);
    CHECK(PAD_FALL == 48);
    CHECK(MINE_BEACON == 0xD6A840u);
}

TEST_CASE("U65 fine layouts") {
    for (const int n : {512, 1024, 2048}) {
        std::string problem;
        const Layout c = *layout_for(n, n, problem);
        for (const int s : {8, 4, 2}) {
            const int se = effective_spacing(n, s);
            CHECK(se == ((n == 2048 && s == 2) ? 4 : s));
            const Layout f = fine_layout(c, se);
            const int v = n * 16 / se;
            CHECK(f.n == v);
            CHECK(f.region_size == std::max(256, v / 16));
            CHECK(f.region_count == v / f.region_size);
            CHECK(f.region_min == -v / (2 * f.region_size));
            CHECK(v <= 32 * f.region_size);
            CHECK(f.origin == -8.0f * static_cast<float>(n));
            CHECK(f.region_count * f.region_count <= 256);
        }
    }
    CHECK(detail_mesh_lods(8) == 8);
    CHECK(detail_mesh_lods(4) == 9);
    CHECK(detail_mesh_lods(2) == 10);
}

TEST_CASE("U66 bands") {
    const World w = mixed_world();
    const DetailPlan p = w.plan(8, 42, {{200, 400, 64}});
    const DetailBuffers one = build_detail_buffers(p, 1, true);
    for (const int bands : {3, 16}) {
        const DetailBuffers b = build_detail_buffers(p, bands, true);
        CHECK(b.heights == one.heights);
        CHECK(b.control == one.control);
        CHECK(b.colour == one.colour);
    }
    // Any order: bands written backwards.
    DetailBuffers r;
    r.heights.resize(one.heights.size());
    r.control.resize(one.control.size());
    r.colour.resize(one.colour.size());
    for (int band = 6; band >= 0; --band) {
        build_detail_band(p, band * 160, band * 160 + 160, r.heights, r.control, r.colour);
    }
    CHECK(r.heights == one.heights);
    CHECK(r.control == one.control);
}

TEST_CASE("detail stats on plain grassland") {
    const World w = mixed_world();
    const DetailPlan p = w.plan(4);
    const GroundField f = build_ground_field(p);
    const auto stats = detail_stats(p, f, w.cells);
    for (const auto& s : stats) {
        MESSAGE("type ", s.type, " rms ", s.rms, " steep ", s.steep_pm, " windows ", s.windows);
        if (s.type == 1) {
            CHECK(s.rms > 0.08);
            CHECK(s.rms < 0.30);
            CHECK(s.steep_pm < 5.0);
        }
    }
}
