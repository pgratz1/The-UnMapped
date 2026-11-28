#include "terrain/vegetation.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace unmapped::terrain {

const std::array<VegetationKindInfo, VEGETATION_KINDS> VEGETATION_KINDS_INFO = {{
    {"broadleaf", 2, 600.0f, 6000.0f, 700, 1300},
    {"conifer", 2, 600.0f, 6000.0f, 700, 1400},
    {"shrub", 1, 1000.0f, 0.0f, 600, 1400},
    {"reed clump", 1, 800.0f, 0.0f, 700, 1300},
    {"snag", 1, 3000.0f, 0.0f, 800, 1200},
}};

// Columns: fallback t, c0 %, trees Q8 at t = 255, shrub, reed, snag Q8.
// The two forest types (forest floor, carr) carry 150 trees per hectare at
// t = 255 (983 = 150 * 256 / 39.0625) and 100 shrubs per hectare of
// undergrowth (655) on top of their open-ground shrubs (Paul, 2026-11-27);
// every other type keeps 60 trees per hectare (393).
const std::array<VegetationDensity, 11> VEGETATION_DENSITY = {{
    {0, 0, 393, 0, 0, 0},          // 0 unknown
    {38, 20, 393, 52, 0, 0},       // 1 grassland
    {255, 30, 983, 66 + 655, 0, 0}, // 2 forest floor
    {179, 10, 983, 98 + 655, 66, 0}, // 3 carr
    {77, 0, 393, 66, 393, 0},      // 4 marsh
    {51, 10, 393, 66, 262, 0},     // 5 fen
    {13, 40, 393, 98, 0, 0},       // 6 steppe
    {13, 80, 393, 262, 0, 0},      // 7 heath
    {26, 70, 393, 164, 0, 0},      // 8 moor
    {0, 100, 393, 26, 0, 0},       // 9 scree
    {0, 0, 393, 0, 0, 52},         // 10 waste
}};

std::array<std::uint32_t, VEGETATION_KINDS> expected_counts(std::uint8_t type, int timber, std::int64_t elevation_dm) {
    std::array<std::uint32_t, VEGETATION_KINDS> e{};
    if (type > 10) {
        return e;
    }
    const VegetationDensity& d = VEGETATION_DENSITY[type];
    const std::int64_t eT = (static_cast<std::int64_t>(d.tree_q8) * static_cast<std::int64_t>(timber) + 127) / 255;
    const std::int64_t conifer_pct = std::min<std::int64_t>(100, d.conifer_c0 + std::max<std::int64_t>(0, elevation_dm - 3000) / 100);
    const std::int64_t eConifer = (eT * conifer_pct + 50) / 100;
    e[0] = static_cast<std::uint32_t>(eT - eConifer);
    e[1] = static_cast<std::uint32_t>(eConifer);
    e[2] = static_cast<std::uint32_t>(d.shrub_q8);
    e[3] = static_cast<std::uint32_t>(d.reed_q8);
    e[4] = static_cast<std::uint32_t>(d.snag_q8);
    return e;
}

std::uint64_t mix64(std::uint64_t z) noexcept {
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

std::uint64_t scatter_key(std::uint64_t seed) noexcept { return mix64(seed ^ 0x7633642D73636174ull); }

float cell_jitter(std::uint64_t draw) noexcept {
    return static_cast<float>(2 * (draw >> 54) + 1) / 128.0f - 8.0f;
}

namespace {

template <class T>
void require(std::span<const T> span, std::size_t cells, const char* name, bool optional) {
    if ((optional && span.empty()) || span.size() == cells) {
        return;
    }
    throw std::invalid_argument(std::string("place_vegetation: ") + name + " has " + std::to_string(span.size()) +
                                " cells, expected " + std::to_string(cells));
}

void check_inputs(const VegetationInputs& in) {
    if (in.layout == nullptr || in.cells == nullptr) {
        throw std::invalid_argument("place_vegetation: no layout or ground cells");
    }
    const int n = in.layout->n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    require(in.heights, total, "heights", false);
    require(in.elevation, total, "elevation", false);
    require(in.timber, total, "timber", true);
    require(in.water, total, "water", true);
    require(std::span<const std::uint8_t>(in.cells->type), total, "ground type", false);
}

// Appends the candidates of terrain cells [x0, x1) x [y0, y1) (already
// clamped) to out, in placement order. visit(cand) is called for each.
template <class Visit>
void for_each_candidate(const VegetationInputs& in, int x0, int y0, int x1, int y1, Visit&& visit) {
    const Layout& layout = *in.layout;
    const GroundCells& cells = *in.cells;
    const int n = layout.n;
    const std::uint64_t key0 = scatter_key(in.seed);
    const float lo = -8.0f * static_cast<float>(n);
    const float hi = 8.0f * static_cast<float>(n) - 16.0f;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const std::size_t c = static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
            const std::uint8_t type = cells.type[c];
            if (type < 1 || type > 10 || cells.painted[c] > 0 || cells.rock[c] >= 128 || cells.snow[c] >= 128 ||
                (!in.water.empty() && in.water[c] != 0) || in.elevation[c] <= 0) {
                continue;
            }
            const int t = in.timber.empty() ? VEGETATION_DENSITY[type].fallback_timber : in.timber[c];
            const auto e = expected_counts(type, t, in.elevation[c]);
            const std::uint64_t hb = mix64(key0 ^ ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) |
                                                   static_cast<std::uint64_t>(static_cast<std::uint32_t>(y))));
            for (std::uint64_t j = 0; j < VEGETATION_KINDS; ++j) {
                const std::uint32_t ej = e[j];
                if (ej == 0) {
                    continue;
                }
                const std::uint64_t hc = mix64(hb + 64 * j);
                const std::uint32_t count = (ej >> 8) + ((hc & 0xFF) < (ej & 0xFF) ? 1u : 0u);
                const VegetationKindInfo& info = VEGETATION_KINDS_INFO[j];
                for (std::uint64_t k = 0; k < count; ++k) {
                    const std::uint64_t h = mix64(hb + 64 * j + 1 + k);
                    const float jx = cell_jitter(mix64(h ^ JITTER_KEY_X));
                    const float jz = cell_jitter(mix64(h ^ JITTER_KEY_Z));
                    const float X = 16.0f * static_cast<float>(x - n / 2) + jx;
                    const float Z = 16.0f * static_cast<float>(y - n / 2) + jz;
                    if (X < lo || X > hi || Z < lo || Z > hi) {
                        continue;
                    }
                    Candidate cand{};
                    cand.kind = static_cast<std::uint8_t>(j);
                    cand.instance.x = X;
                    cand.instance.z = Z;
                    cand.instance.yaw1024 = static_cast<std::uint16_t>((h >> 20) & 0x3FF);
                    const std::uint64_t range = static_cast<std::uint64_t>(info.scale_max_pm - info.scale_min_pm);
                    cand.instance.scale_pm = static_cast<std::uint16_t>(
                        static_cast<std::uint64_t>(info.scale_min_pm) + (range * ((h >> 30) & 0x3FF) + 511) / 1023);
                    cand.instance.tint = static_cast<std::uint8_t>((h >> 40) & 0xFF);
                    cand.thin = static_cast<std::uint32_t>(mix64(h ^ 0xD1B54A32D192ED03ull) >> 32);
                    cand.instance.y = ground_low(layout, in.heights, X, Z);
                    visit(cand);
                }
            }
        }
    }
}

}  // namespace

std::vector<Candidate> place_candidates(const VegetationInputs& in, int x0, int y0, int x1, int y1) {
    check_inputs(in);
    const int n = in.layout->n;
    x0 = std::clamp(x0, 0, n);
    x1 = std::clamp(x1, 0, n);
    y0 = std::clamp(y0, 0, n);
    y1 = std::clamp(y1, 0, n);
    std::vector<Candidate> out;
    for_each_candidate(in, x0, y0, x1, y1, [&out](const Candidate& c) { out.push_back(c); });
    return out;
}

Placement place_vegetation(const Layout& layout, std::span<const float> heights, std::span<const std::int16_t> elevation,
                           std::span<const std::uint8_t> timber, std::span<const std::uint8_t> water,
                           const GroundCells& cells, std::uint64_t seed, std::uint64_t cap) {
    VegetationInputs in;
    in.layout = &layout;
    in.heights = heights;
    in.elevation = elevation;
    in.timber = timber;
    in.water = water;
    in.cells = &cells;
    in.seed = seed;
    const std::vector<Candidate> list = place_candidates(in, 0, 0, layout.n, layout.n);

    Placement out;
    out.candidates = list.size();
    out.cap = cap;
    auto key = [](const Candidate& c, std::size_t p) {
        return (static_cast<std::uint64_t>(c.thin) << 32) | static_cast<std::uint64_t>(p);
    };
    std::uint64_t limit = UINT64_MAX;  // keep keys <= limit
    if (out.candidates > cap) {
        if (cap == 0) {
            return out;
        }
        std::vector<std::uint64_t> keys(list.size());
        for (std::size_t p = 0; p < list.size(); ++p) {
            keys[p] = key(list[p], p);
        }
        const auto nth = keys.begin() + static_cast<std::ptrdiff_t>(cap - 1);
        std::nth_element(keys.begin(), nth, keys.end());
        limit = *nth;
    }
    for (std::size_t p = 0; p < list.size(); ++p) {
        if (key(list[p], p) <= limit) {
            out.kinds[list[p].kind].push_back(list[p].instance);
        }
    }
    return out;
}

FarTier place_far_tier(const VegetationInputs& in, std::uint64_t budget) {
    check_inputs(in);
    const int n = in.layout->n;
    FarTier tier;
    tier.budget = budget;
    tier.placement.cap = budget;
    // One pass: keep the far-kind candidates (instance and thin), in
    // placement order, and count every candidate.
    struct Far {
        Instance instance;
        std::uint32_t thin;
        std::uint8_t kind;
    };
    std::vector<Far> far;
    std::uint64_t all = 0;
    for_each_candidate(in, 0, 0, n, n, [&](const Candidate& c) {
        ++all;
        if (FAR_KIND[c.kind]) {
            far.push_back({c.instance, c.thin, c.kind});
        }
    });
    tier.placement.candidates = all;
    tier.far_candidates = far.size();
    // T = sorted_thin(F)[B], or 2^32 when every far candidate fits.
    if (far.size() > budget) {
        std::vector<std::uint32_t> thins(far.size());
        for (std::size_t p = 0; p < far.size(); ++p) {
            thins[p] = far[p].thin;
        }
        const auto nth = thins.begin() + static_cast<std::ptrdiff_t>(budget);
        std::nth_element(thins.begin(), nth, thins.end());
        tier.threshold = *nth;
    } else {
        tier.threshold = THIN_ALL;
    }
    for (const Far& f : far) {
        if (static_cast<std::uint64_t>(f.thin) < tier.threshold) {
            tier.placement.kinds[f.kind].push_back(f.instance);
        }
    }
    return tier;
}

std::array<std::vector<Instance>, VEGETATION_KINDS> near_instances(const VegetationInputs& in, int i, int j,
                                                                    std::uint64_t threshold) {
    check_inputs(in);
    std::array<std::vector<Instance>, VEGETATION_KINDS> out;
    const int x0 = 16 * i;
    const int y0 = 16 * j;
    const int n = in.layout->n;
    for_each_candidate(in, std::clamp(x0, 0, n), std::clamp(y0, 0, n), std::clamp(x0 + 16, 0, n),
                       std::clamp(y0 + 16, 0, n), [&out, threshold](const Candidate& c) {
                           if (!is_far(c, threshold)) {
                               out[c.kind].push_back(c.instance);
                           }
                       });
    return out;
}

std::vector<float> instance_buffer(std::span<const Instance> instances, std::uint8_t tint_mode) {
    std::vector<float> out(16 * instances.size());
    float* p = out.data();
    for (const Instance& in : instances) {
        const float s = static_cast<float>(in.scale_pm) / 1000.0f;
        const float a = static_cast<float>(in.yaw1024) * (6.28318530717958647692f / 1024.0f);
        const float c = std::cos(a);
        const float sn = std::sin(a);
        const float v = static_cast<float>(200 + static_cast<int>(in.tint) * 55 / 255) / 255.0f;
        p[0] = s * c;
        p[1] = 0.0f;
        p[2] = s * sn;
        p[3] = in.x;
        p[4] = 0.0f;
        p[5] = s;
        p[6] = 0.0f;
        p[7] = in.y;
        p[8] = -(s * sn);
        p[9] = 0.0f;
        p[10] = s * c;
        p[11] = in.z;
        p[12] = v;
        p[13] = tint_mode == 1 ? 0.45f * v : v;
        p[14] = tint_mode == 1 ? 0.45f * v : v;
        p[15] = 1.0f;
        p += 16;
    }
    return out;
}

}  // namespace unmapped::terrain
