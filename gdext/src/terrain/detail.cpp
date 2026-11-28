#include "terrain/detail.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>

#include "terrain/vegetation.hpp"

namespace unmapped::terrain {

const std::array<DetailAmplitude, 11> DETAIL_AMPLITUDE = {{
    {100, 0, 0},     // 0 unknown
    {200, 0, 600},   // 1 grassland (spec 120; tuned, V8: no visible swell at 2 or 20 m, rms 0.16 m)
    {300, 0, 300},   // 2 forest floor (spec 200; tuned with grassland, rms 0.26 m)
    {120, 0, 300},   // 3 carr
    {40, 0, 0},      // 4 marsh
    {60, 0, 0},      // 5 fen
    {160, 0, 600},   // 6 steppe
    {240, 300, 0},   // 7 heath
    {300, 400, 0},   // 8 moor
    {600, 1000, 0},  // 9 alpine (scree)
    {240, 500, 0},   // 10 waste
}};

namespace {

std::size_t at(int n, int x, int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(n) + static_cast<std::size_t>(x);
}

int clampi(int v, int lo, int hi) { return std::min(std::max(v, lo), hi); }

constexpr std::int64_t abs64(std::int64_t v) noexcept { return v < 0 ? -v : v; }

std::optional<int> whole(std::string_view v) {
    if (v.empty() || v.size() > 4) {
        return std::nullopt;
    }
    int out = 0;
    for (const char c : v) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        out = out * 10 + (c - '0');
    }
    return out;
}

// S16 of 6: smoothstep on 0..65536.
std::int64_t s16(std::int64_t t) noexcept { return (t * t * (196608 - 2 * t)) >> 32; }

std::int32_t lattice(std::uint32_t key, std::int64_t p, std::int64_t q) noexcept {
    return static_cast<std::int32_t>(hash32(static_cast<std::uint32_t>(p), static_cast<std::uint32_t>(q), key) >> 16) - 32768;
}

// Windowed max - min over (2r + 1)^2, cells outside skipped (separable).
std::vector<std::int32_t> window_range(std::span<const std::int16_t> e, int n, int r) {
    std::vector<std::int32_t> rmax(static_cast<std::size_t>(n) * static_cast<std::size_t>(n));
    std::vector<std::int32_t> rmin(rmax.size());
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            std::int32_t hi = e[at(n, x, y)];
            std::int32_t lo = hi;
            for (int k = std::max(0, x - r); k <= std::min(n - 1, x + r); ++k) {
                hi = std::max<std::int32_t>(hi, e[at(n, k, y)]);
                lo = std::min<std::int32_t>(lo, e[at(n, k, y)]);
            }
            rmax[at(n, x, y)] = hi;
            rmin[at(n, x, y)] = lo;
        }
    }
    std::vector<std::int32_t> out(rmax.size());
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            std::int32_t hi = rmax[at(n, x, y)];
            std::int32_t lo = rmin[at(n, x, y)];
            for (int k = std::max(0, y - r); k <= std::min(n - 1, y + r); ++k) {
                hi = std::max(hi, rmax[at(n, x, k)]);
                lo = std::min(lo, rmin[at(n, x, k)]);
            }
            out[at(n, x, y)] = hi - lo;
        }
    }
    return out;
}

// Windowed sum over (2r + 1)^2 of v, cells outside skipped (separable).
std::vector<std::int64_t> window_sum(const std::vector<std::int64_t>& v, int n, int r) {
    std::vector<std::int64_t> rows(v.size());
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            std::int64_t s = 0;
            for (int k = std::max(0, x - r); k <= std::min(n - 1, x + r); ++k) {
                s += v[at(n, k, y)];
            }
            rows[at(n, x, y)] = s;
        }
    }
    std::vector<std::int64_t> out(v.size());
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            std::int64_t s = 0;
            for (int k = std::max(0, y - r); k <= std::min(n - 1, y + r); ++k) {
                s += rows[at(n, x, k)];
            }
            out[at(n, x, y)] = s;
        }
    }
    return out;
}

struct Quad {
    int i0, i1, j0, j1;
    std::int64_t a, b;
};

Quad quad_of(const DetailPlan& p, int xf, int yf) noexcept {
    Quad q{};
    const int i = std::min(xf / p.f, p.n - 1);
    const int j = std::min(yf / p.f, p.n - 1);
    q.a = xf - i * p.f;
    q.b = yf - j * p.f;
    if (q.a >= p.f) {
        q.a = p.f - 1;
    }
    if (q.b >= p.f) {
        q.b = p.f - 1;
    }
    q.i0 = i;
    q.j0 = j;
    q.i1 = std::min(i + 1, p.n - 1);
    q.j1 = std::min(j + 1, p.n - 1);
    return q;
}

template <class T>
std::int64_t bil(const DetailPlan& p, const std::vector<T>& field, const Quad& q) noexcept {
    const std::int64_t f = p.f;
    const std::int64_t s = static_cast<std::int64_t>(field[at(p.n, q.i0, q.j0)]) * (f - q.a) * (f - q.b) +
                           static_cast<std::int64_t>(field[at(p.n, q.i1, q.j0)]) * q.a * (f - q.b) +
                           static_cast<std::int64_t>(field[at(p.n, q.i0, q.j1)]) * (f - q.a) * q.b +
                           static_cast<std::int64_t>(field[at(p.n, q.i1, q.j1)]) * q.a * q.b;
    return fdiv(s + f * f / 2, f * f);
}

constexpr std::int64_t BLOCK_M = 256;

}  // namespace

// --- Options and layouts ------------------------------------------------------------------

std::optional<int> parse_ground_detail(std::string_view v) {
    if (v == "off") {
        return 0;
    }
    if (v == "8") {
        return 8;
    }
    if (v == "4") {
        return 4;
    }
    if (v == "2") {
        return 2;
    }
    return std::nullopt;
}

std::optional<int> parse_detail_threads(std::string_view v) {
    const std::optional<int> n = whole(v);
    if (!n || *n > DETAIL_MAX_THREADS) {
        return std::nullopt;
    }
    return n;
}

std::string ground_detail_refusal(std::string_view v) {
    return "--ground-detail " + std::string(v) + ": off, 8, 4 or 2";
}

std::string detail_threads_refusal(std::string_view v) {
    return "--detail-threads " + std::string(v) + ": a whole number from 0 to 16";
}

int effective_spacing(int n, int s) noexcept { return (s == 2 && n >= 2048) ? 4 : s; }

Layout fine_layout(const Layout& coarse, int s) {
    if (s != 2 && s != 4 && s != 8) {
        throw std::invalid_argument("fine_layout: spacing must be 2, 4 or 8");
    }
    const int f = 16 / s;
    const int v = coarse.n * f;
    Layout out;
    out.n = v;
    out.spacing = static_cast<float>(s);
    out.region_size = std::max(256, v / 16);
    out.region_min = -v / (2 * out.region_size);
    out.region_count = v / out.region_size;
    out.origin = -8.0f * static_cast<float>(coarse.n);
    return out;
}

int detail_mesh_lods(int s) noexcept {
    int lods = 7;
    for (int f = 16 / std::max(1, s); f > 1; f /= 2) {
        ++lods;
    }
    return lods;
}

// --- Upsampling ---------------------------------------------------------------------------

std::vector<std::int32_t> coarse_decimetres(std::span<const std::int16_t> elevation,
                                            std::span<const std::uint16_t> water_depth) {
    std::vector<std::int32_t> out(elevation.size());
    for (std::size_t k = 0; k < elevation.size(); ++k) {
        const std::int32_t e = elevation[k];
        const std::int32_t d = water_depth.empty() ? 0 : static_cast<std::int32_t>(water_depth[k]);
        out[k] = (e > 0 && d > 0) ? e + d : e;
    }
    return out;
}

std::array<std::int64_t, 4> catmull_rom_weights(int a_, int f_) noexcept {
    const std::int64_t a = a_;
    const std::int64_t f = f_;
    return {-a * a * a + 2 * a * a * f - a * f * f, 3 * a * a * a - 5 * a * a * f + 2 * f * f * f,
            -3 * a * a * a + 4 * a * a * f + a * f * f, a * a * a - a * a * f};
}

std::int64_t upsample_mm(std::span<const std::int32_t> hd, int n, int f, int xf, int yf) noexcept {
    const int i = std::min(xf / f, n - 1);
    const int j = std::min(yf / f, n - 1);
    const int a = std::min(xf - i * f, f - 1);
    const int b = std::min(yf - j * f, f - 1);
    const auto wx = catmull_rom_weights(a, f);
    const auto wy = catmull_rom_weights(b, f);
    std::int64_t s = 0;
    for (int q = -1; q <= 2; ++q) {
        const int y = clampi(j + q, 0, n - 1);
        std::int64_t row = 0;
        for (int p = -1; p <= 2; ++p) {
            const int x = clampi(i + p, 0, n - 1);
            row += wx[static_cast<std::size_t>(p + 1)] * hd[at(n, x, y)];
        }
        s += wy[static_cast<std::size_t>(q + 1)] * row;
    }
    const std::int64_t f6 = static_cast<std::int64_t>(f) * f * f * f * f * f;
    const std::int64_t cr = fdiv(100 * s + 2 * f6, 4 * f6);
    const int i1 = std::min(i + 1, n - 1);
    const int j1 = std::min(j + 1, n - 1);
    const std::int64_t c[4] = {hd[at(n, i, j)], hd[at(n, i1, j)], hd[at(n, i, j1)], hd[at(n, i1, j1)]};
    const std::int64_t lo = std::min(std::min(c[0], c[1]), std::min(c[2], c[3]));
    const std::int64_t hi = std::max(std::max(c[0], c[1]), std::max(c[2], c[3]));
    return std::clamp(cr, 100 * lo, 100 * hi);
}

// --- Factor fields ------------------------------------------------------------------------

std::vector<std::int32_t> chamfer_distance(const std::vector<std::uint8_t>& source, int n) {
    const std::int32_t far = 6 * n;
    std::vector<std::int32_t> d(source.size());
    for (std::size_t k = 0; k < source.size(); ++k) {
        d[k] = source[k] != 0 ? 0 : far;
    }
    auto get = [&d, n, far](int x, int y) { return (x < 0 || y < 0 || x >= n || y >= n) ? far : d[at(n, x, y)]; };
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            std::int32_t v = d[at(n, x, y)];
            v = std::min(v, get(x - 1, y) + 3);
            v = std::min(v, get(x - 1, y - 1) + 4);
            v = std::min(v, get(x, y - 1) + 3);
            v = std::min(v, get(x + 1, y - 1) + 4);
            d[at(n, x, y)] = v;
        }
    }
    for (int y = n - 1; y >= 0; --y) {
        for (int x = n - 1; x >= 0; --x) {
            std::int32_t v = d[at(n, x, y)];
            v = std::min(v, get(x + 1, y) + 3);
            v = std::min(v, get(x + 1, y + 1) + 4);
            v = std::min(v, get(x, y + 1) + 3);
            v = std::min(v, get(x - 1, y + 1) + 4);
            d[at(n, x, y)] = v;
        }
    }
    return d;
}

std::int32_t detail_ramp(std::int32_t d) noexcept {
    if (d <= 4) {
        return 0;
    }
    return std::clamp((d - 3) * 1000 / 6, 0, 1000);
}

DetailFactors build_detail_factors(const Layout& layout, std::span<const std::int16_t> elevation,
                                   const GroundCells& cells, std::span<const std::uint8_t> water) {
    const int n = layout.n;
    const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
    if (elevation.size() != total || (!water.empty() && water.size() != total) ||
        (!cells.type.empty() && (cells.type.size() != total || cells.painted.size() != total))) {
        throw std::invalid_argument("build_detail_factors: a layer is not n * n long");
    }
    const bool have_cells = !cells.type.empty();
    auto type_of = [&](std::size_t k) -> std::uint8_t { return have_cells ? cells.type[k] : 0; };
    auto painted = [&](std::size_t k) -> bool { return have_cells && cells.painted[k] > 0; };
    std::vector<std::uint8_t> wet_noise(total);
    std::vector<std::uint8_t> wet_level(total);
    std::vector<std::uint8_t> alpine(total);
    std::vector<std::int64_t> amp(total), ridge(total), billow(total), land(total);
    for (std::size_t k = 0; k < total; ++k) {
        const std::uint8_t t = type_of(k);
        const std::uint8_t w = water.empty() ? 0 : water[k];
        wet_noise[k] = (w != 0 || painted(k) || !is_land_type(t)) ? 1 : 0;
        wet_level[k] = (w == 2 || w == 3 || t == 13 || t == 14) ? 1 : 0;
        alpine[k] = (t == 9 && !painted(k)) ? 1 : 0;
        if (is_land_type(t)) {
            const DetailAmplitude& a = DETAIL_AMPLITUDE[t];
            amp[k] = a.amp_cm;
            ridge[k] = a.ridge_pm;
            billow[k] = a.billow_pm;
            land[k] = 1;
        }
    }
    const std::vector<std::int32_t> dn = chamfer_distance(wet_noise, n);
    const std::vector<std::int32_t> dl = chamfer_distance(wet_level, n);
    const std::vector<std::int32_t> da = chamfer_distance(alpine, n);
    const std::vector<std::int32_t> relief = window_range(elevation, n, 4);
    const auto sa = window_sum(amp, n, 2);
    const auto sr = window_sum(ridge, n, 2);
    const auto sb = window_sum(billow, n, 2);
    const auto sl = window_sum(land, n, 2);
    DetailFactors out;
    out.n = n;
    out.a_cell.resize(total);
    out.ridge.resize(total);
    out.billow.resize(total);
    out.w_noise.resize(total);
    out.w_level.resize(total);
    out.m.resize(total);
    for (std::size_t k = 0; k < total; ++k) {
        const std::int32_t wn = detail_ramp(dn[k]);
        out.w_noise[k] = static_cast<std::int16_t>(wn);
        out.w_level[k] = static_cast<std::int16_t>(detail_ramp(dl[k]));
        const std::int32_t m_rel = std::clamp((relief[k] - 50) * 1000 / 950, 0, 1000);
        const std::int32_t prox = std::clamp(1000 - da[k] * 1000 / 192, 0, 1000);
        const std::int32_t M = 1000 + m_rel + prox / 2;
        out.m[k] = static_cast<std::int16_t>(M);
        const std::int64_t count = sl[k];
        const std::int64_t ab = count > 0 ? sa[k] / count : 0;
        out.ridge[k] = static_cast<std::int16_t>(count > 0 ? sr[k] / count : 0);
        out.billow[k] = static_cast<std::int16_t>(count > 0 ? sb[k] / count : 0);
        out.a_cell[k] = static_cast<std::int32_t>(std::min<std::int64_t>(A_MAX_CM, ab * M / 1000) * wn / 1000);
    }
    return out;
}

// --- Noise ----------------------------------------------------------------------------------

std::uint32_t octave_key(std::uint64_t seed, int o) noexcept {
    return static_cast<std::uint32_t>(mix64(seed ^ DETAIL_KEY) >> 32) ^
           (0x9E3779B9u * static_cast<std::uint32_t>(o + 1));
}

std::uint32_t warp_key(std::uint64_t seed, int axis) noexcept {
    return static_cast<std::uint32_t>(mix64(seed ^ (DETAIL_KEY ^ static_cast<std::uint64_t>(axis))) >> 32);
}

std::int32_t noise_octave(std::uint32_t key, std::int32_t lambda, std::int64_t xm, std::int64_t zm) noexcept {
    const std::int64_t L = xm / lambda;
    const std::int64_t K = zm / lambda;
    const std::int64_t sx = s16((xm % lambda) * 65536 / lambda);
    const std::int64_t sy = s16((zm % lambda) * 65536 / lambda);
    const std::int64_t top = static_cast<std::int64_t>(lattice(key, L, K)) * (65536 - sx) +
                             static_cast<std::int64_t>(lattice(key, L + 1, K)) * sx;
    const std::int64_t bot = static_cast<std::int64_t>(lattice(key, L, K + 1)) * (65536 - sx) +
                             static_cast<std::int64_t>(lattice(key, L + 1, K + 1)) * sx;
    return static_cast<std::int32_t>((top * (65536 - sy) + bot * sy) >> 32);
}

int octave_count(int s) noexcept {
    int count = 0;
    for (const std::int32_t lambda : OCT_LAMBDA) {
        count += lambda >= 2 * s ? 1 : 0;
    }
    return count;
}

std::int32_t detail_noise(const std::array<std::uint32_t, 6>& keys, int octaves, std::int64_t xm, std::int64_t zm,
                          std::int32_t ridge, std::int32_t billow) noexcept {
    std::int64_t sum = 0;
    for (int o = 0; o < octaves; ++o) {
        const std::int64_t n = noise_octave(keys[static_cast<std::size_t>(o)], OCT_LAMBDA[static_cast<std::size_t>(o)], xm, zm);
        const std::int64_t mag = n < 0 ? -n : n;
        const std::int64_t r = std::max<std::int64_t>(-32768, 32767 - 2 * mag);
        const std::int64_t b = 2 * mag - 32768;
        const std::int64_t c = n + fdiv(ridge * (r - n) + billow * (b - n), 1000);
        sum += OCT_PM[static_cast<std::size_t>(o)] * c;
    }
    return static_cast<std::int32_t>(fdiv(sum, 1000));
}

// --- Pads -----------------------------------------------------------------------------------

std::vector<Pad> build_pads(std::span<const PadPlace> places, int n, std::span<const std::int32_t> hd,
                            std::span<const std::int16_t> elevation, std::span<const std::uint8_t> water) {
    std::vector<Pad> out;
    for (const PadPlace& place : places) {
        if (place.cell_x < 0 || place.cell_y < 0 || place.cell_x >= n || place.cell_y >= n) {
            continue;
        }
        const std::size_t k = at(n, place.cell_x, place.cell_y);
        // Built: no pad on a lake or the sea (water 2 or 3) or at E <= 0. The
        // spec's water != 0 also drops river cells (water 1), where nearly
        // every settlement stands, so almost no pad would remain.
        if ((!water.empty() && (water[k] == 2 || water[k] == 3)) || elevation[k] <= 0) {
            continue;
        }
        Pad pad;
        pad.cell_x = place.cell_x;
        pad.cell_y = place.cell_y;
        pad.x = 16 * static_cast<std::int64_t>(place.cell_x);
        pad.z = 16 * static_cast<std::int64_t>(place.cell_y);
        pad.r0 = place.r0;
        pad.p_mm = 100 * static_cast<std::int64_t>(hd[k]);
        const int reach = place.r0 / 16;
        std::int32_t lo = hd[k];
        std::int32_t hi = hd[k];
        for (int y = std::max(0, place.cell_y - reach); y <= std::min(n - 1, place.cell_y + reach); ++y) {
            for (int x = std::max(0, place.cell_x - reach); x <= std::min(n - 1, place.cell_x + reach); ++x) {
                lo = std::min(lo, hd[at(n, x, y)]);
                hi = std::max(hi, hd[at(n, x, y)]);
            }
        }
        pad.rel_dm = hi - lo;
        pad.level_pm = pad.rel_dm <= PAD_FLAT_DM ? 1000 : 1000 * PAD_FLAT_DM / pad.rel_dm;
        out.push_back(pad);
    }
    return out;
}

std::int64_t pad_smooth(std::int64_t t) noexcept { return t * t * (3000 - 2 * t) / 1000000; }

std::int64_t pad_rho(const Pad& pad, std::int64_t d) noexcept {
    if (d <= pad.r0) {
        return 1000;
    }
    if (d >= pad.r0 + PAD_FALL) {
        return 0;
    }
    return pad_smooth((pad.r0 + PAD_FALL - d) * 1000 / PAD_FALL);
}

// --- The plan --------------------------------------------------------------------------------

DetailPlan make_detail_plan(const Layout& coarse, int s, std::uint64_t seed, std::span<const std::int16_t> elevation,
                            std::span<const std::uint16_t> water_depth, const GroundCells& cells,
                            std::span<const std::uint8_t> water, std::span<const PadPlace> places,
                            std::span<const std::uint32_t> control, std::span<const std::uint8_t> colour, bool textured,
                            const Look& look) {
    DetailPlan p;
    p.n = coarse.n;
    p.s = s;
    p.f = 16 / s;
    p.v = p.n * p.f;
    p.seed = seed;
    p.hd = coarse_decimetres(elevation, water_depth);
    p.factors = build_detail_factors(coarse, elevation, cells, water);
    p.pads = build_pads(places, p.n, p.hd, elevation, water);
    p.blocks = p.n / 16;
    p.block_pads.assign(static_cast<std::size_t>(p.blocks) * static_cast<std::size_t>(p.blocks), {});
    for (std::size_t i = 0; i < p.pads.size(); ++i) {
        const Pad& pad = p.pads[i];
        const std::int64_t r = pad.r0 + PAD_FALL;
        const int bx0 = static_cast<int>(std::max<std::int64_t>(0, fdiv(pad.x - r, BLOCK_M)));
        const int bx1 = static_cast<int>(std::min<std::int64_t>(p.blocks - 1, fdiv(pad.x + r, BLOCK_M)));
        const int bz0 = static_cast<int>(std::max<std::int64_t>(0, fdiv(pad.z - r, BLOCK_M)));
        const int bz1 = static_cast<int>(std::min<std::int64_t>(p.blocks - 1, fdiv(pad.z + r, BLOCK_M)));
        for (int bz = bz0; bz <= bz1; ++bz) {
            for (int bx = bx0; bx <= bx1; ++bx) {
                p.block_pads[at(p.blocks, bx, bz)].push_back(static_cast<std::uint32_t>(i));
            }
        }
    }
    for (int o = 0; o < 6; ++o) {
        p.keys[static_cast<std::size_t>(o)] = octave_key(seed, o);
    }
    p.octaves = octave_count(s);
    p.warp_x = warp_key(seed, 1);
    p.warp_z = warp_key(seed, 2);
    p.control = control;
    p.colour = colour;
    p.textured = textured;
    p.look = look;
    return p;
}

std::int32_t pad_noise_factor(const DetailPlan& plan, std::int64_t xm, std::int64_t zm) noexcept {
    if (plan.pads.empty()) {
        return 1000;
    }
    const int bx = static_cast<int>(std::min<std::int64_t>(plan.blocks - 1, xm / BLOCK_M));
    const int bz = static_cast<int>(std::min<std::int64_t>(plan.blocks - 1, zm / BLOCK_M));
    std::int64_t rho = 0;
    for (const std::uint32_t i : plan.block_pads[at(plan.blocks, bx, bz)]) {
        const Pad& pad = plan.pads[i];
        const std::int64_t d = std::max(abs64(xm - pad.x), abs64(zm - pad.z));
        rho = std::max(rho, pad_rho(pad, d));
    }
    return static_cast<std::int32_t>(1000 - rho);
}

std::int64_t field_mm(const DetailPlan& p, int xf, int yf) noexcept {
    const std::int64_t xm = static_cast<std::int64_t>(xf) * p.s;
    const std::int64_t zm = static_cast<std::int64_t>(yf) * p.s;
    const Quad q = quad_of(p, xf, yf);
    const std::int64_t up = upsample_mm(p.hd, p.n, p.f, xf, yf);
    const std::vector<std::uint32_t>* list = nullptr;
    std::int64_t rho_max = 0;
    if (!p.pads.empty()) {
        const int bx = static_cast<int>(std::min<std::int64_t>(p.blocks - 1, xm / BLOCK_M));
        const int bz = static_cast<int>(std::min<std::int64_t>(p.blocks - 1, zm / BLOCK_M));
        list = &p.block_pads[at(p.blocks, bx, bz)];
        for (const std::uint32_t i : *list) {
            const Pad& pad = p.pads[i];
            const std::int64_t d = std::max(abs64(xm - pad.x), abs64(zm - pad.z));
            rho_max = std::max(rho_max, pad_rho(pad, d));
        }
    }
    const std::int64_t n_pad = 1000 - rho_max;
    std::int64_t a_mm = 10 * bil(p, p.factors.a_cell, q) * n_pad / 1000;
    a_mm = std::min(a_mm, std::max<std::int64_t>(0, up - 200));
    std::int64_t h1 = up;
    if (a_mm > 0) {
        const auto ridge = static_cast<std::int32_t>(bil(p, p.factors.ridge, q));
        const auto billow = static_cast<std::int32_t>(bil(p, p.factors.billow, q));
        const std::int64_t noise = detail_noise(p.keys, p.octaves, xm, zm, ridge, billow);
        h1 = up + fdiv(a_mm * noise + 16384, 32768);
    }
    if (list == nullptr || rho_max == 0) {
        return h1;
    }
    std::int64_t ls = 0;
    std::int64_t hs = 0;
    for (const std::uint32_t i : *list) {
        const Pad& pad = p.pads[i];
        const std::int64_t d = std::max(abs64(xm - pad.x), abs64(zm - pad.z));
        const std::int64_t l = static_cast<std::int64_t>(pad.level_pm) * pad_rho(pad, d) / 1000;
        ls += l;
        hs += l * (pad.p_mm - h1);
    }
    const std::int64_t wl = bil(p, p.factors.w_level, q);
    return h1 + fdiv(hs * wl, 1000 * std::max<std::int64_t>(1000, ls));
}

// --- Fine control and colour -----------------------------------------------------------------

std::int64_t fine_rock(std::int64_t g_f, int s) noexcept {
    const std::int64_t D = 2000 * static_cast<std::int64_t>(s);
    if (g_f * 320 <= 224 * D) {
        return 0;
    }
    if (g_f >= D) {
        return 255;
    }
    return ((320 * g_f - 224 * D) * 255 + 48 * D) / (96 * D);
}

std::array<std::int32_t, 2> fine_warp(const DetailPlan& p, int xf, int yf) noexcept {
    const std::int64_t xm = static_cast<std::int64_t>(xf) * p.s;
    const std::int64_t zm = static_cast<std::int64_t>(yf) * p.s;
    return {static_cast<std::int32_t>(fdiv(64 * static_cast<std::int64_t>(noise_octave(p.warp_x, 32, xm, zm)), 32768)),
            static_cast<std::int32_t>(fdiv(64 * static_cast<std::int64_t>(noise_octave(p.warp_z, 32, xm, zm)), 32768))};
}

namespace {

struct Corners {
    std::array<std::size_t, 4> k{};
    std::array<std::int64_t, 4> w{};
};

Corners warped_corners(const DetailPlan& p, int xf, int yf) noexcept {
    const auto warp = fine_warp(p, xf, yf);
    const std::int64_t u = 16 * static_cast<std::int64_t>(xf) * p.s + warp[0];
    const std::int64_t v = 16 * static_cast<std::int64_t>(yf) * p.s + warp[1];
    const int i = static_cast<int>(std::clamp<std::int64_t>(fdiv(u, 256), 0, p.n - 1));
    const int j = static_cast<int>(std::clamp<std::int64_t>(fdiv(v, 256), 0, p.n - 1));
    const std::int64_t fu = std::clamp<std::int64_t>(u - 256 * i, 0, 255);
    const std::int64_t fv = std::clamp<std::int64_t>(v - 256 * j, 0, 255);
    const int i1 = std::min(i + 1, p.n - 1);
    const int j1 = std::min(j + 1, p.n - 1);
    Corners c;
    c.k = {at(p.n, i, j), at(p.n, i1, j), at(p.n, i, j1), at(p.n, i1, j1)};
    c.w = {(256 - fu) * (256 - fv), fu * (256 - fv), (256 - fu) * fv, fu * fv};
    return c;
}

bool base_id(int k) { return k <= 10 || k == 13 || k == 14; }

}  // namespace

namespace {

std::uint32_t control_at(const DetailPlan& p, const Corners& c, int xf, int yf, std::int64_t h, std::int64_t hl,
                         std::int64_t hr, std::int64_t hu, std::int64_t hd_) noexcept {
    if (!p.textured || p.control.empty()) {
        return 0;
    }
    std::array<std::int64_t, 17> acc{};
    std::size_t nearest = 0;
    for (std::size_t q = 0; q < 4; ++q) {
        const std::uint32_t cc = p.control[c.k[q]];
        const std::uint32_t base = (cc >> 27) & 0x1Fu;
        const std::uint32_t overlay = (cc >> 22) & 0x1Fu;
        const std::int64_t blend = (cc >> 14) & 0xFFu;
        acc[std::min<std::uint32_t>(base, 16)] += c.w[q] * (255 - blend);
        acc[std::min<std::uint32_t>(overlay, 16)] += c.w[q] * blend;
        if (c.w[q] > c.w[nearest]) {
            nearest = q;
        }
    }
    int k1 = -1;
    std::int64_t w1 = 0;
    for (int k = 0; k <= 16; ++k) {
        if (base_id(k) && (k1 < 0 || acc[static_cast<std::size_t>(k)] > w1)) {
            k1 = k;
            w1 = acc[static_cast<std::size_t>(k)];
        }
    }
    if (w1 == 0) {
        // Built: every corner fully overlaid; the nearest corner's base.
        k1 = static_cast<int>((p.control[c.k[nearest]] >> 27) & 0x1Fu);
        if (!base_id(k1)) {
            k1 = 0;
        }
    }
    int k2 = -1;
    std::int64_t w2 = 0;
    for (int k = 0; k <= 16; ++k) {
        if (k != k1 && (k2 < 0 || acc[static_cast<std::size_t>(k)] > w2)) {
            k2 = k;
            w2 = acc[static_cast<std::size_t>(k)];
        }
    }
    const std::uint32_t base = static_cast<std::uint32_t>(k1);
    std::uint32_t overlay = w2 == 0 ? base : static_cast<std::uint32_t>(k2);
    std::int64_t blend = w2 == 0 ? 0 : (255 * w2 + (w1 + w2) / 2) / (w1 + w2);
    if (base <= 10) {
        const std::int64_t dx = hr - hl;
        const std::int64_t dz = hd_ - hu;
        const std::int64_t g_f = isqrt64(dx * dx + dz * dz);
        const std::int64_t rock = fine_rock(g_f, p.s);
        if (overlay == 12) {
            const std::int64_t g = g_f * 320 / (2000 * static_cast<std::int64_t>(p.s));
            constexpr std::int64_t span = SNOW_HOLD_NONE_G - SNOW_HOLD_FULL_G;
            const std::int64_t hold =
                g <= SNOW_HOLD_FULL_G ? 255 : g >= SNOW_HOLD_NONE_G ? 0 : ((SNOW_HOLD_NONE_G - g) * 255 + span / 2) / span;
            blend = blend * hold / 255;
        }
        if (rock > blend && !(overlay == 12 && blend >= 128)) {
            overlay = p.look.rock ? rock_variant(xf, yf, static_cast<std::int32_t>(fdiv(h, 100))) : 11u;
            blend = rock;
        }
    }
    return (base << 27) | (overlay << 22) | (static_cast<std::uint32_t>(blend) << 14);
}

std::array<std::uint8_t, 4> colour_at(const DetailPlan& p, const Corners& c) noexcept {
    std::array<std::uint8_t, 4> out{};
    if (p.colour.empty()) {
        return out;
    }
    for (std::size_t ch = 0; ch < 4; ++ch) {
        std::int64_t s = 0;
        for (std::size_t q = 0; q < 4; ++q) {
            s += c.w[q] * p.colour[4 * c.k[q] + ch];
        }
        out[ch] = static_cast<std::uint8_t>((s + 32768) >> 16);
    }
    return out;
}

}  // namespace

std::uint32_t fine_control(const DetailPlan& p, int xf, int yf, std::int64_t h, std::int64_t hl, std::int64_t hr,
                           std::int64_t hu, std::int64_t hd_) noexcept {
    return control_at(p, warped_corners(p, xf, yf), xf, yf, h, hl, hr, hu, hd_);
}

std::array<std::uint8_t, 4> fine_colour(const DetailPlan& p, int xf, int yf) noexcept {
    return colour_at(p, warped_corners(p, xf, yf));
}

void build_detail_band(const DetailPlan& p, int y0, int y1, std::span<float> heights, std::span<std::uint32_t> control,
                       std::span<std::uint8_t> colour) {
    const int v = p.v;
    y0 = std::clamp(y0, 0, v);
    y1 = std::clamp(y1, y0, v);
    if (y0 == y1) {
        return;
    }
    const bool maps = !control.empty() || !colour.empty();
    const int r0 = maps ? std::max(0, y0 - 1) : y0;
    const int r1 = maps ? std::min(v, y1 + 1) : y1;
    std::vector<std::int32_t> rows(static_cast<std::size_t>(r1 - r0) * static_cast<std::size_t>(v));
    for (int y = r0; y < r1; ++y) {
        for (int x = 0; x < v; ++x) {
            rows[at(v, x, y - r0)] = static_cast<std::int32_t>(field_mm(p, x, y));
        }
    }
    for (int y = y0; y < y1; ++y) {
        for (int x = 0; x < v; ++x) {
            heights[at(v, x, y)] = static_cast<float>(rows[at(v, x, y - r0)]) / 1000.0f;
        }
    }
    if (!maps) {
        return;
    }
    auto H = [&](int x, int y) -> std::int64_t {
        return rows[at(v, clampi(x, 0, v - 1), clampi(y, r0, r1 - 1) - r0)];
    };
    for (int y = y0; y < y1; ++y) {
        for (int x = 0; x < v; ++x) {
            const std::size_t k = at(v, x, y);
            const Corners c = warped_corners(p, x, y);
            if (!control.empty()) {
                control[k] = control_at(p, c, x, y, H(x, y), H(x - 1, y), H(x + 1, y), H(x, y - 1), H(x, y + 1));
            }
            if (!colour.empty()) {
                const auto rgba = colour_at(p, c);
                for (std::size_t ch = 0; ch < 4; ++ch) {
                    colour[4 * k + ch] = rgba[ch];
                }
            }
        }
    }
}

DetailBuffers build_detail_buffers(const DetailPlan& plan, int bands, bool with_maps) {
    DetailBuffers out;
    const std::size_t total = static_cast<std::size_t>(plan.v) * static_cast<std::size_t>(plan.v);
    out.heights.resize(total);
    if (with_maps) {
        out.control.resize(total);
        out.colour.resize(4 * total);
    }
    bands = std::max(1, bands);
    const int rows = (plan.v + bands - 1) / bands;
    for (int b = 0; b < bands; ++b) {
        build_detail_band(plan, b * rows, (b + 1) * rows, out.heights, out.control, out.colour);
    }
    return out;
}

GroundField build_ground_field(const DetailPlan& plan) {
    std::string problem;
    const std::optional<Layout> coarse = layout_for(plan.n, plan.n, problem);
    if (!coarse) {
        throw std::invalid_argument("build_ground_field: " + problem);
    }
    GroundField field;
    field.layout = fine_layout(*coarse, plan.s);
    field.f = plan.f;
    field.heights = build_detail_buffers(plan, 1, false).heights;
    return field;
}

// --- Stats ------------------------------------------------------------------------------------

std::vector<DetailTypeStats> detail_stats(const DetailPlan& p, const GroundField& field, const GroundCells& cells) {
    std::vector<DetailTypeStats> out;
    if (cells.type.empty() || field.heights.size() != static_cast<std::size_t>(p.v) * static_cast<std::size_t>(p.v)) {
        return out;
    }
    const int v = p.v;
    const int win = std::max(1, 64 / p.s);
    const int need = win * win / 2;
    std::array<std::vector<double>, 11> windows;
    std::array<std::uint64_t, 11> count{};
    std::array<std::uint64_t, 11> steep{};
    std::array<std::uint64_t, 11> steep_base{};
    const double tan35 = 0.7;
    for (int wy = 0; wy + win <= v; wy += win) {
        for (int wx = 0; wx + win <= v; wx += win) {
            std::array<double, 11> s{};
            std::array<double, 11> s2{};
            std::array<int, 11> c{};
            for (int y = wy; y < wy + win; ++y) {
                for (int x = wx; x < wx + win; ++x) {
                    const int i = std::min(x / p.f, p.n - 1);
                    const int j = std::min(y / p.f, p.n - 1);
                    const std::size_t k = at(p.n, i, j);
                    const std::uint8_t t = cells.type[k];
                    if (!is_land_type(t) || p.factors.w_noise[k] != 1000 ||
                        pad_noise_factor(p, static_cast<std::int64_t>(x) * p.s, static_cast<std::int64_t>(y) * p.s) != 1000) {
                        continue;
                    }
                    const double h = static_cast<double>(field.heights[at(v, x, y)]);
                    const double d = h - static_cast<double>(upsample_mm(p.hd, p.n, p.f, x, y)) / 1000.0;
                    s[t] += d;
                    s2[t] += d * d;
                    ++c[t];
                    const double dx = static_cast<double>(field.heights[at(v, std::min(x + 1, v - 1), y)]) -
                                      static_cast<double>(field.heights[at(v, std::max(x - 1, 0), y)]);
                    const double dz = static_cast<double>(field.heights[at(v, x, std::min(y + 1, v - 1))]) -
                                      static_cast<double>(field.heights[at(v, x, std::max(y - 1, 0))]);
                    if (std::sqrt(dx * dx + dz * dz) / (2.0 * p.s) > tan35) {
                        ++steep[t];
                    }
                    const auto up = [&](int ux, int uy) {
                        return static_cast<double>(upsample_mm(p.hd, p.n, p.f, std::clamp(ux, 0, v - 1), std::clamp(uy, 0, v - 1))) / 1000.0;
                    };
                    const double bx = up(x + 1, y) - up(x - 1, y);
                    const double bz = up(x, y + 1) - up(x, y - 1);
                    if (std::sqrt(bx * bx + bz * bz) / (2.0 * p.s) > tan35) {
                        ++steep_base[t];
                    }
                    ++count[t];
                }
            }
            for (std::size_t t = 0; t < 11; ++t) {
                if (c[t] >= need) {
                    const double mean = s[t] / c[t];
                    windows[t].push_back(std::sqrt(std::max(0.0, s2[t] / c[t] - mean * mean)));
                }
            }
        }
    }
    for (std::size_t t = 0; t < 11; ++t) {
        if (count[t] == 0) {
            continue;
        }
        DetailTypeStats st;
        st.type = static_cast<int>(t);
        st.vertices = count[t];
        st.windows = windows[t].size();
        if (!windows[t].empty()) {
            auto& w = windows[t];
            std::nth_element(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(w.size() / 2), w.end());
            st.rms = w[w.size() / 2];
        }
        st.steep_pm = 1000.0 * static_cast<double>(steep[t]) / static_cast<double>(count[t]);
        st.steep_base_pm = 1000.0 * static_cast<double>(steep_base[t]) / static_cast<double>(count[t]);
        out.push_back(st);
    }
    return out;
}

std::string detail_stats_text(std::span<const DetailTypeStats> stats) {
    static const char* names[11] = {"unknown", "grassland", "forest", "carr", "marsh", "fen",
                                    "steppe",  "heath",     "moor",   "alpine", "waste"};
    std::string out;
    for (const DetailTypeStats& s : stats) {
        if (!out.empty()) {
            out += "; ";
        }
        char buffer[128];
        std::snprintf(buffer, sizeof buffer, "%s rms %.2f m, steep %.2f (%.2f without detail)", names[s.type], s.rms,
                      s.steep_pm, s.steep_base_pm);
        out += buffer;
    }
    return out;
}

std::uint64_t fnv1a64(const void* data, std::size_t bytes) noexcept {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint64_t h = 0xCBF29CE484222325ull;
    for (std::size_t i = 0; i < bytes; ++i) {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

}  // namespace unmapped::terrain
