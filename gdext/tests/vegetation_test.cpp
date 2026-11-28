// Spec V3D-02 11.1, U9 (mix64), U13 to U15: placement, the cap and the
// ground under an instance.

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>

#include "terrain/ground.hpp"
#include "terrain/terrain_data.hpp"
#include "terrain/vegetation.hpp"

using namespace unmapped::terrain;

namespace {

Layout layout_of(int n) {
    std::string problem;
    const auto layout = layout_for(n, n, problem);
    REQUIRE(layout.has_value());
    return *layout;
}

// A 512 world: flat at 10 m, every cell of one land type, or a mixed map.
struct World {
    Layout layout;
    std::vector<std::int16_t> elevation;
    std::vector<float> heights;
    std::vector<std::uint8_t> timber;
    std::vector<std::uint8_t> water;
    GroundCells cells;
};

World flat_world(std::uint8_t type, std::uint8_t t) {
    World w;
    w.layout = layout_of(512);
    const std::size_t total = 512u * 512u;
    w.elevation.assign(total, 100);
    w.heights.assign(total, 10.0f);
    w.timber.assign(total, t);
    w.water.assign(total, 0);
    w.cells.type.assign(total, type);
    w.cells.painted.assign(total, 0);
    w.cells.rock.assign(total, 0);
    w.cells.snow.assign(total, 0);
    return w;
}

// A mixed world: bands of types, some water, painted, rock and snow cells,
// rising ground.
World mixed_world() {
    World w = flat_world(2, 200);
    const int n = 512;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const std::size_t k = static_cast<std::size_t>(y) * n + static_cast<std::size_t>(x);
            w.cells.type[k] = static_cast<std::uint8_t>((x / 37 + y / 53) % 15);
            w.elevation[k] = static_cast<std::int16_t>((x * 7 + y * 3) % 9000 - 200);
            w.heights[k] = static_cast<float>(w.elevation[k]) / 10.0f;
            w.timber[k] = static_cast<std::uint8_t>((x * 13 + y) % 256);
            w.water[k] = static_cast<std::uint8_t>((x % 41 == 0) ? 1 : (y % 47 == 0 ? 2 : 0));
            w.cells.painted[k] = static_cast<std::uint8_t>((x % 43 == 0) ? 3 : 0);
            w.cells.rock[k] = static_cast<std::uint8_t>((x * 31 + y * 17) % 256);
            w.cells.snow[k] = static_cast<std::uint8_t>((x * 11 + y * 29) % 256);
        }
    }
    return w;
}

Placement place(const World& w, std::uint64_t seed, std::uint64_t cap) {
    return place_vegetation(w.layout, w.heights, w.elevation, w.timber, w.water, w.cells, seed, cap);
}

bool same(const Instance& a, const Instance& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.yaw1024 == b.yaw1024 && a.scale_pm == b.scale_pm &&
           a.tint == b.tint;
}

bool same(const Placement& a, const Placement& b) {
    if (a.candidates != b.candidates) {
        return false;
    }
    for (std::size_t j = 0; j < static_cast<std::size_t>(VEGETATION_KINDS); ++j) {
        if (a.kinds[j].size() != b.kinds[j].size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.kinds[j].size(); ++i) {
            if (!same(a.kinds[j][i], b.kinds[j][i])) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

TEST_CASE("U9 mix64 vectors") {
    CHECK(mix64(0) == 0xE220A8397B1DCDAFull);
    CHECK(mix64(1) == 0x910A2DEC89025CC1ull);
    CHECK(mix64(0x7633642D73636174ull) == 0x6D85CB5419E96C85ull);
    CHECK(scatter_key(42) == 0x5797AA86629AEA6Cull);
    CHECK(scatter_key(0) == mix64(0x7633642D73636174ull));
}

TEST_CASE("U13 place_vegetation") {
    const World w = mixed_world();
    const Placement a = place(w, 42, DEFAULT_INSTANCE_CAP);
    const Placement b = place(w, 42, DEFAULT_INSTANCE_CAP);
    CHECK(a.candidates > 1000);
    CHECK(same(a, b));
    const Placement c = place(w, 43, DEFAULT_INSTANCE_CAP);
    CHECK_FALSE(same(a, c));
    // No instance on an excluded cell; each less than 8 m on each axis from its cell's vertex
    // and inside the map.
    const int n = 512;
    std::size_t bad = 0;
    for (const auto& kind : a.kinds) {
        for (const Instance& i : kind) {
            const CellHit hit = world_to_cell(w.layout, i.x, i.z);
            if (!hit.inside) {
                ++bad;
                continue;
            }
            const Vec2 v = cell_to_world(w.layout, hit.x, hit.y);
            const std::size_t k = static_cast<std::size_t>(hit.y) * n + static_cast<std::size_t>(hit.x);
            const std::uint8_t t = w.cells.type[k];
            const bool excluded = t < 1 || t > 10 || w.cells.painted[k] > 0 || w.cells.rock[k] >= 128 ||
                                  w.cells.snow[k] >= 128 || w.water[k] != 0 || w.elevation[k] <= 0;
            const bool near = i.x - v.x > -8.0f && i.x - v.x < 8.0f && i.z - v.z > -8.0f && i.z - v.z < 8.0f;
            const bool in_map = i.x >= -8.0f * static_cast<float>(n) && i.x <= 8.0f * static_cast<float>(n) - 16.0f && i.z >= -8.0f * static_cast<float>(n) && i.z <= 8.0f * static_cast<float>(n) - 16.0f;
            bad += (excluded || !near || !in_map) ? 1 : 0;
        }
    }
    CHECK(bad == 0);

    // Counts per kind on one land type with t = 255, for every row of 6.3,
    // over the interior cells (the edge cells lose the instances jittered
    // off the map).
    for (std::uint8_t type = 1; type <= 10; ++type) {
        CAPTURE(static_cast<int>(type));
        const World one = flat_world(type, 255);
        const Placement p = place(one, 7, MAX_INSTANCE_CAP);
        const auto e = expected_counts(type, 255, 100);
        for (int j = 0; j < VEGETATION_KINDS; ++j) {
            CAPTURE(j);
            std::size_t count = 0;
            for (const Instance& i : p.kinds[static_cast<std::size_t>(j)]) {
                const CellHit hit = world_to_cell(one.layout, i.x, i.z);
                count += (hit.x >= 1 && hit.x <= n - 2 && hit.y >= 1 && hit.y <= n - 2) ? 1 : 0;
            }
            const double cells = static_cast<double>(n - 2) * static_cast<double>(n - 2);
            const double mean = cells * static_cast<double>(e[static_cast<std::size_t>(j)]) / 256.0;
            const double q = static_cast<double>(e[static_cast<std::size_t>(j)] & 0xFF) / 256.0;
            const double sigma = std::sqrt(cells * q * (1.0 - q));
            CHECK(std::abs(static_cast<double>(count) - mean) <= 4.0 * sigma + 1.0);
        }
    }
    // The ground under an instance.
    for (const auto& kind : a.kinds) {
        for (std::size_t i = 0; i < kind.size(); i += 97) {
            CHECK(kind[i].y == ground_low(w.layout, w.heights, kind[i].x, kind[i].z));
        }
    }
}

namespace {

// The test's own copy of 6.4's candidates: (kind, X, Z, thin), in order.
struct Cand {
    int kind;
    float x, z;
    std::uint32_t thin;
};

std::uint64_t own_mix(std::uint64_t z) {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

std::vector<Cand> own_candidates(const World& w, std::uint64_t seed) {
    std::vector<Cand> out;
    const int n = w.layout.n;
    const std::uint64_t key0 = own_mix(seed ^ 0x7633642D73636174ull);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const std::size_t c = static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
            const std::uint8_t t = w.cells.type[c];
            if (t < 1 || t > 10 || w.cells.painted[c] > 0 || w.cells.rock[c] >= 128 || w.cells.snow[c] >= 128 ||
                w.water[c] != 0 || w.elevation[c] <= 0) {
                continue;
            }
            const auto e = expected_counts(t, w.timber[c], w.elevation[c]);
            const std::uint64_t hb = own_mix(key0 ^ ((static_cast<std::uint64_t>(x) << 32) | static_cast<std::uint64_t>(y)));
            for (std::uint64_t j = 0; j < 5; ++j) {
                const std::uint64_t hc = own_mix(hb + 64 * j);
                const std::uint64_t count = (e[j] >> 8) + ((hc & 0xFF) < (e[j] & 0xFF) ? 1 : 0);
                for (std::uint64_t k = 0; k < count; ++k) {
                    const std::uint64_t h = own_mix(hb + 64 * j + 1 + k);
                    const float X = 16.0f * static_cast<float>(x - n / 2) + static_cast<float>(2 * (own_mix(h ^ 0x6A09E667F3BCC909ull) >> 54) + 1) / 128.0f - 8.0f;
                    const float Z = 16.0f * static_cast<float>(y - n / 2) + static_cast<float>(2 * (own_mix(h ^ 0xBB67AE8584CAA73Bull) >> 54) + 1) / 128.0f - 8.0f;
                    if (X < -8.0f * static_cast<float>(n) || X > 8.0f * static_cast<float>(n) - 16.0f || Z < -8.0f * static_cast<float>(n) || Z > 8.0f * static_cast<float>(n) - 16.0f) {
                        continue;
                    }
                    out.push_back({static_cast<int>(j), X, Z, static_cast<std::uint32_t>(own_mix(h ^ 0xD1B54A32D192ED03ull) >> 32)});
                }
            }
        }
    }
    return out;
}

}  // namespace

TEST_CASE("U14 the instance cap") {
    const World w = mixed_world();
    const std::vector<Cand> cands = own_candidates(w, 5);
    const Placement all = place(w, 5, MAX_INSTANCE_CAP);
    REQUIRE(all.candidates == cands.size());
    CHECK(all.kept() == cands.size());
    for (const std::uint64_t cap : {cands.size() / 3, std::size_t{1}, cands.size() - 1}) {
        CAPTURE(cap);
        const Placement p = place(w, 5, cap);
        CHECK(p.candidates == cands.size());
        CHECK(p.cap == cap);
        REQUIRE(p.kept() == cap);
        // Brute force: the cap smallest keys, back in placement order.
        std::vector<std::pair<std::uint64_t, std::size_t>> keys;
        for (std::size_t i = 0; i < cands.size(); ++i) {
            keys.emplace_back((static_cast<std::uint64_t>(cands[i].thin) << 32) | i, i);
        }
        std::sort(keys.begin(), keys.end());
        std::vector<std::size_t> kept;
        for (std::size_t i = 0; i < cap; ++i) {
            kept.push_back(keys[i].second);
        }
        std::sort(kept.begin(), kept.end());
        std::array<std::size_t, 5> next{};
        std::size_t bad = 0;
        for (const std::size_t i : kept) {
            const Cand& c = cands[i];
            const auto& list = p.kinds[static_cast<std::size_t>(c.kind)];
            std::size_t& at = next[static_cast<std::size_t>(c.kind)];
            if (at >= list.size() || list[at].x != c.x || list[at].z != c.z) {
                ++bad;
            }
            ++at;
        }
        CHECK(bad == 0);
    }
    const Placement none = place(w, 5, 0);
    CHECK(none.kept() == 0);
    CHECK(none.candidates == cands.size());
    const Placement exact = place(w, 5, cands.size());
    CHECK(exact.kept() == cands.size());
}

TEST_CASE("U15 ground_low") {
    const Layout layout = layout_of(512);
    const int n = 512;
    std::vector<float> h(static_cast<std::size_t>(n) * n);
    for (std::size_t k = 0; k < h.size(); ++k) {
        h[k] = static_cast<float>(static_cast<std::int64_t>(own_mix(k) % 20001) - 10000) / 10.0f;
    }
    auto H = [&](int x, int y) { return h[static_cast<std::size_t>(y) * n + static_cast<std::size_t>(x)]; };
    // At a vertex it equals the vertex height.
    for (const auto [x, y] : {std::pair{0, 0}, std::pair{100, 200}, std::pair{510, 3}, std::pair{37, 510}}) {
        const Vec2 v = cell_to_world(layout, x, y);
        CHECK(ground_low(layout, h, v.x, v.z) == H(x, y));
    }
    // On the last row and column the grid point is the far corner of the
    // last cell square, reached through float sums.
    CHECK(static_cast<double>(ground_low(layout, h, 4080.0f, 4080.0f)) == doctest::Approx(static_cast<double>(H(511, 511))).epsilon(1e-6));
    int bad = 0;
    for (std::uint64_t k = 0; k < 1000; ++k) {
        const std::uint64_t r = own_mix(k + 77);
        const float X = static_cast<float>(static_cast<std::int64_t>(r % 8000) - 4000) + static_cast<float>((r >> 20) % 1000) / 1000.0f;
        const float Z = static_cast<float>(static_cast<std::int64_t>((r >> 32) % 8000) - 4000) + static_cast<float>((r >> 50) % 1000) / 1000.0f;
        const float low = ground_low(layout, h, X, Z);
        // Both triangulations, from the grid point.
        const float u = std::clamp(X / 16.0f + 256.0f, 0.0f, 511.0f);
        const float v = std::clamp(Z / 16.0f + 256.0f, 0.0f, 511.0f);
        const int i = std::min(static_cast<int>(std::floor(u)), n - 2);
        const int j = std::min(static_cast<int>(std::floor(v)), n - 2);
        const float fu = u - static_cast<float>(i), fv = v - static_cast<float>(j);
        const float h00 = H(i, j), h10 = H(i + 1, j), h01 = H(i, j + 1), h11 = H(i + 1, j + 1);
        const float a = fu >= fv ? h00 + fu * (h10 - h00) + fv * (h11 - h10) : h00 + fv * (h01 - h00) + fu * (h11 - h01);
        const float b = fu + fv <= 1.0f ? h00 + fu * (h10 - h00) + fv * (h01 - h00)
                                        : h11 + (1.0f - fu) * (h01 - h11) + (1.0f - fv) * (h10 - h11);
        const float bil = sample_bilinear(layout, h, X, Z);
        const float eps = 1e-3f;
        bad += (low <= a && low <= b && low <= bil + eps) ? 0 : 1;
    }
    CHECK(bad == 0);
}

TEST_CASE("U13 the forest densities of 6.3 (Paul, 2026-11-27)") {
    // Forest floor and carr: 150 trees per hectare at t = 255 (Q8 983) and
    // 100 shrubs per hectare of undergrowth (Q8 655) on top; the mix of kinds
    // is unchanged. Every other type keeps 60 trees per hectare (Q8 393).
    using E = std::array<std::uint32_t, VEGETATION_KINDS>;
    CHECK(expected_counts(2, 255, 100) == E{688, 295, 66 + 655, 0, 0});
    CHECK(expected_counts(3, 255, 100) == E{885, 98, 98 + 655, 66, 0});
    CHECK(expected_counts(1, 255, 100) == E{314, 79, 52, 0, 0});
    CHECK(expected_counts(7, 255, 100) == E{79, 314, 262, 0, 0});
    CHECK(expected_counts(9, 0, 100) == E{0, 0, 26, 0, 0});
    for (std::uint8_t type = 0; type <= 10; ++type) {
        CAPTURE(static_cast<int>(type));
        const bool forest = type == 2 || type == 3;
        CHECK(VEGETATION_DENSITY[type].tree_q8 == (forest ? 983 : 393));
    }
    CHECK(VEGETATION_KINDS_INFO[2].lod0_range == 1000.0f);
    CHECK(DEFAULT_INSTANCE_CAP == 2000000);
}

namespace {

double correlation(const std::vector<double>& a, const std::vector<double>& b) {
    const double n = static_cast<double>(a.size());
    double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sa += a[i];
        sb += b[i];
        saa += a[i] * a[i];
        sbb += b[i] * b[i];
        sab += a[i] * b[i];
    }
    const double cov = sab / n - (sa / n) * (sb / n);
    const double va = saa / n - (sa / n) * (sa / n);
    const double vb = sbb / n - (sb / n) * (sb / n);
    return cov / std::sqrt(va * vb);
}

}  // namespace

TEST_CASE("U13 instances spread evenly and independently over their cell (Paul, 2026-11-27)") {
    // Offsets from the cell's vertex fill the whole 16 m cell: a 4 x 4
    // histogram of (dx, dz) is even, and dx is uncorrelated with dz, with
    // the next slot in the same cell and with the neighbouring cells. The
    // old jitter of [-6, 6) left the outer bins at half their share.
    const World w = flat_world(2, 255);
    const int n = 512;
    const Placement p = place(w, 42, MAX_INSTANCE_CAP);
    std::array<std::size_t, 16> bins{};
    std::size_t total = 0;
    std::vector<double> dxs, dzs;
    constexpr float NONE = 1000.0f;
    std::vector<float> first_dx(static_cast<std::size_t>(n) * n, NONE), first_dz(first_dx.size(), NONE),
        second_dx(first_dx.size(), NONE);
    for (std::size_t j = 0; j < p.kinds.size(); ++j) {
        for (const Instance& i : p.kinds[j]) {
            const CellHit hit = world_to_cell(w.layout, i.x, i.z);
            REQUIRE(hit.inside);
            const Vec2 v = cell_to_world(w.layout, hit.x, hit.y);
            const float dx = i.x - v.x;
            const float dz = i.z - v.z;
            REQUIRE(dx > -8.0f);
            REQUIRE(dx < 8.0f);
            REQUIRE(dz > -8.0f);
            REQUIRE(dz < 8.0f);
            if (hit.x < 1 || hit.x > n - 2 || hit.y < 1 || hit.y > n - 2) {
                continue;  // edge cells lose the instances jittered off the map
            }
            ++bins[static_cast<std::size_t>(static_cast<int>((dz + 8.0f) / 4.0f) * 4 + static_cast<int>((dx + 8.0f) / 4.0f))];
            ++total;
            dxs.push_back(static_cast<double>(dx));
            dzs.push_back(static_cast<double>(dz));
            if (j == 0) {  // broadleaf: two or three a cell
                const std::size_t k = static_cast<std::size_t>(hit.y) * n + static_cast<std::size_t>(hit.x);
                if (first_dx[k] == NONE) {
                    first_dx[k] = dx;
                    first_dz[k] = dz;
                } else if (second_dx[k] == NONE) {
                    second_dx[k] = dx;
                }
            }
        }
    }
    REQUIRE(total > 1000000);
    for (std::size_t b = 0; b < bins.size(); ++b) {
        CAPTURE(b);
        CHECK(std::abs(static_cast<double>(bins[b]) * 16.0 / static_cast<double>(total) - 1.0) < 0.02);
    }
    CHECK(std::abs(correlation(dxs, dzs)) < 0.01);
    std::vector<double> a, b, c, d, e, f;
    for (int y = 1; y < n - 2; ++y) {
        for (int x = 1; x < n - 2; ++x) {
            const std::size_t k = static_cast<std::size_t>(y) * n + static_cast<std::size_t>(x);
            if (first_dx[k] == NONE || first_dx[k + 1] == NONE || first_dz[k + n] == NONE || second_dx[k] == NONE) {
                continue;
            }
            a.push_back(static_cast<double>(first_dx[k]));
            b.push_back(static_cast<double>(first_dx[k + 1]));  // east neighbour
            c.push_back(static_cast<double>(first_dz[k]));
            d.push_back(static_cast<double>(first_dz[k + n]));  // south neighbour
            e.push_back(static_cast<double>(first_dx[k]));
            f.push_back(static_cast<double>(second_dx[k]));     // next slot
        }
    }
    REQUIRE(a.size() > 200000);
    CHECK(std::abs(correlation(a, b)) < 0.01);
    CHECK(std::abs(correlation(c, d)) < 0.01);
    CHECK(std::abs(correlation(e, f)) < 0.01);
}

// --- Spec V3D-03 11.1: U17, U18, U24, U25 (vegetation part) ---------------------------

namespace {

VegetationInputs inputs_of(const World& w, std::uint64_t seed) {
    VegetationInputs in;
    in.layout = &w.layout;
    in.heights = w.heights;
    in.elevation = w.elevation;
    in.timber = w.timber;
    in.water = w.water;
    in.cells = &w.cells;
    in.seed = seed;
    return in;
}

bool same(const Candidate& a, const Candidate& b) { return same(a.instance, b.instance) && a.thin == b.thin && a.kind == b.kind; }

// The terrain cell of an instance: its nearest vertex (the jitter is under 8 m).
std::pair<int, int> cell_of(const Layout& layout, const Instance& i) {
    const CellHit hit = world_to_cell(layout, i.x, i.z);
    return {hit.x, hit.y};
}

}  // namespace

TEST_CASE("U17 place_candidates over rectangles equals the whole map's list filtered by cell") {
    const World w = mixed_world();
    const VegetationInputs in = inputs_of(w, 42);
    const std::vector<Candidate> whole = place_candidates(in, 0, 0, 512, 512);
    // The whole-map list is the test's own 6.4 list, field for field.
    const std::vector<Cand> own = own_candidates(w, 42);
    REQUIRE(whole.size() == own.size());
    std::size_t differ = 0;
    for (std::size_t p = 0; p < own.size(); ++p) {
        differ += (whole[p].kind == own[p].kind && whole[p].instance.x == own[p].x && whole[p].instance.z == own[p].z &&
                   whole[p].thin == own[p].thin)
                      ? 0u
                      : 1u;
    }
    CHECK(differ == 0);
    // The placement is built on it.
    const Placement all = place(w, 42, MAX_INSTANCE_CAP);
    CHECK(all.candidates == whole.size());
    struct Rect {
        int x0, y0, x1, y1;
    };
    const std::vector<Rect> rects = {
        {0, 0, 512, 512},     {0, 0, 1, 1},       {511, 511, 512, 512}, {0, 511, 1, 512},   {511, 0, 512, 1},
        {0, 0, 16, 16},       {496, 496, 512, 512}, {100, 200, 101, 201}, {0, 100, 512, 101}, {100, 0, 101, 512},
        {-20, -20, 30, 30},   {480, 480, 600, 600}, {256, 256, 272, 272}, {17, 33, 99, 34},   {300, 10, 301, 500},
        {40, 40, 40, 80},     {0, 255, 512, 257},  {255, 0, 257, 512},   {123, 321, 222, 444}, {-5, 200, 5, 600},
    };
    for (const Rect& r : rects) {
        CAPTURE(r.x0);
        CAPTURE(r.y0);
        CAPTURE(r.x1);
        CAPTURE(r.y1);
        const std::vector<Candidate> got = place_candidates(in, r.x0, r.y0, r.x1, r.y1);
        std::vector<Candidate> want;
        for (const Candidate& c : whole) {
            const auto [x, y] = cell_of(w.layout, c.instance);
            if (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1) {
                want.push_back(c);
            }
        }
        REQUIRE(got.size() == want.size());
        std::size_t bad = 0;
        for (std::size_t p = 0; p < got.size(); ++p) {
            bad += same(got[p], want[p]) ? 0u : 1u;
        }
        CHECK(bad == 0);
    }
}

TEST_CASE("U18 the partition between the far tier and the near cells") {
    const World w = mixed_world();
    const VegetationInputs in = inputs_of(w, 9);
    const std::vector<Candidate> whole = place_candidates(in, 0, 0, 512, 512);
    std::vector<std::uint32_t> thins;
    for (const Candidate& c : whole) {
        if (FAR_KIND[c.kind]) {
            thins.push_back(c.thin);
        }
    }
    std::sort(thins.begin(), thins.end());
    const std::uint64_t s_far = thins.size();
    REQUIRE(s_far > 2000);
    // No tie in the test world, so the far count is exactly min(B, S_far).
    CHECK(std::adjacent_find(thins.begin(), thins.end()) == thins.end());
    for (const std::uint64_t budget : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{1000}, s_far - 1, s_far,
                                       std::uint64_t{1000000000}}) {
        CAPTURE(budget);
        const std::uint64_t T = s_far <= budget ? THIN_ALL : thins[static_cast<std::size_t>(budget)];
        const FarTier tier = place_far_tier(in, budget);
        CHECK(tier.threshold == T);
        CHECK(tier.far_candidates == s_far);
        CHECK(tier.budget == budget);
        CHECK(tier.placement.candidates == whole.size());
        CHECK(tier.placement.kinds[2].empty());
        CHECK(tier.placement.kinds[3].empty());
        CHECK(tier.placement.kept() == std::min(budget, s_far));
        // Exactly the far set, in placement order.
        std::array<std::size_t, 5> next{};
        std::size_t bad = 0;
        for (const Candidate& c : whole) {
            if (!(FAR_KIND[c.kind] && c.thin < T)) {
                continue;
            }
            const auto& list = tier.placement.kinds[c.kind];
            std::size_t& at = next[c.kind];
            bad += (at < list.size() && same(list[at], c.instance)) ? 0u : 1u;
            ++at;
        }
        for (std::size_t k = 0; k < 5; ++k) {
            bad += next[k] == tier.placement.kinds[k].size() ? 0u : 1u;
        }
        CHECK(bad == 0);
        // Per near cell: near instances and far instances of its cells are
        // disjoint and together the candidates of its rectangle, per kind.
        std::vector<std::array<std::vector<Instance>, 5>> far_by_cell(32 * 32);
        for (std::size_t k = 0; k < 5; ++k) {
            for (const Instance& i : tier.placement.kinds[k]) {
                const auto [x, y] = cell_of(w.layout, i);
                far_by_cell[static_cast<std::size_t>((y / 16) * 32 + x / 16)][k].push_back(i);
            }
        }
        std::size_t wrong = 0;
        for (int j = 0; j < 32; j += (budget == 1000 ? 1 : 3)) {
            for (int i = 0; i < 32; i += (budget == 1000 ? 1 : 3)) {
                const auto nearby = near_instances(in, i, j, T);
                const std::vector<Candidate> cands = place_candidates(in, 16 * i, 16 * j, 16 * i + 16, 16 * j + 16);
                std::array<std::size_t, 5> ni{};
                std::array<std::size_t, 5> fi{};
                const auto& fars = far_by_cell[static_cast<std::size_t>(j * 32 + i)];
                for (const Candidate& c : cands) {
                    const bool far = FAR_KIND[c.kind] && c.thin < T;
                    const auto& list = far ? fars[c.kind] : nearby[c.kind];
                    std::size_t& at = far ? fi[c.kind] : ni[c.kind];
                    wrong += (at < list.size() && same(list[at], c.instance)) ? 0u : 1u;
                    ++at;
                }
                for (std::size_t k = 0; k < 5; ++k) {
                    wrong += (ni[k] == nearby[k].size() && fi[k] == fars[k].size()) ? 0u : 1u;
                }
            }
        }
        CHECK(wrong == 0);
    }
}

TEST_CASE("U24 instance_buffer") {
    std::vector<Instance> list(3);
    list[0] = {10.5f, 20.25f, -30.125f, 0, 1000, 0};
    list[1] = {-4000.0f, 0.0f, 4080.0f, 256, 700, 255};
    list[2] = {123.0078125f, -5.5f, 7.0f, 1000, 1400, 77};
    for (const std::uint8_t mode : {std::uint8_t{0}, std::uint8_t{1}}) {
        CAPTURE(static_cast<int>(mode));
        const std::vector<float> b = instance_buffer(list, mode);
        REQUIRE(b.size() == 48);
        for (std::size_t k = 0; k < 3; ++k) {
            const Instance& in = list[k];
            const float* p = &b[16 * k];
            const double s = in.scale_pm / 1000.0;
            const double a = in.yaw1024 * 2.0 * 3.14159265358979323846 / 1024.0;
            CHECK(p[3] == in.x);
            CHECK(p[7] == in.y);
            CHECK(p[11] == in.z);
            CHECK(p[1] == 0.0f);
            CHECK(p[4] == 0.0f);
            CHECK(p[6] == 0.0f);
            CHECK(p[9] == 0.0f);
            CHECK(p[5] == static_cast<float>(in.scale_pm) / 1000.0f);
            CHECK(std::abs(static_cast<double>(p[0]) - s * std::cos(a)) < 1e-6);
            CHECK(std::abs(static_cast<double>(p[2]) - s * std::sin(a)) < 1e-6);
            CHECK(std::abs(static_cast<double>(p[8]) + s * std::sin(a)) < 1e-6);
            CHECK(std::abs(static_cast<double>(p[10]) - s * std::cos(a)) < 1e-6);
            const float v = static_cast<float>(200 + in.tint * 55 / 255) / 255.0f;
            CHECK(p[12] == v);
            CHECK(p[13] == (mode == 1 ? 0.45f * v : v));
            CHECK(p[14] == (mode == 1 ? 0.45f * v : v));
            CHECK(p[15] == 1.0f);
        }
    }
    CHECK(instance_buffer({}, 0).empty());
}

TEST_CASE("U25 tier constants") {
    CHECK(DEFAULT_FAR_BUDGET == 2000000);  // Paul, 2026-11-27 (spec r1: 1,000,000)
    CHECK(FAR_KIND == std::array<bool, 5>{true, true, false, false, true});
    CHECK(THIN_ALL == 4294967296ull);
}
