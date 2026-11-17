#pragma once

// Support for the P1-03 climate tests: the tuning text of spec section 4 and
// the values the tests read from it, the test's own implementation of steps
// C2 to C6 written from the spec text (a 2-D prefix-sum box mean, a sweep
// over explicit lines, the per-cell formulas), the fixtures of 5.1 and 10.1,
// the random fixtures of 10.2, world inputs from the elevation and hydrology
// stages, grid transforms for the symmetry tests, the invariant checker of
// section 6 and the golden-file reader.
//
// Written from docs/specs/p1-03-climate.md, docs/specs/p1-00-geography-
// overview.md and the P0 specs only. Nothing here calls the climate stage;
// it is reached only through p1_03_climate_api.hpp.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "p1_01_elevation_support.hpp"
#include "p1_02_hydrology_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "p1_03_climate_api.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/noise.hpp"

namespace climate_test {

using worldgen::Fx;
using worldgen::geo::Grid;

inline constexpr std::int64_t ONE = std::int64_t{1} << 32;
inline constexpr std::string_view PRECONDITION = "climate: input precondition: ";
// Threads for calls that are not about threads (P1-00 D10: no result
// depends on it). 1: most grids here are tiny, where starting threads costs
// more than the work; the thread tests pass 1, 2 and 5 explicitly.
inline constexpr std::uint32_t THREADS = 1;

inline constexpr std::uint8_t WATER_DRY = 0;
inline constexpr std::uint8_t WATER_RIVER = 1;
inline constexpr std::uint8_t WATER_LAKE = 2;
inline constexpr std::uint8_t WATER_OCEAN = 3;

inline constexpr std::int64_t TEMP_MIN = -600;
inline constexpr std::int64_t TEMP_MAX = 500;
inline constexpr std::int64_t RAIN_MAX = 10000;

// Stream names of P1-03 section 2, and the test-only stream of 10.2.
inline constexpr std::string_view STREAM_WIND = "geo.climate.wind";
inline constexpr std::string_view STREAM_TEMP = "geo.climate.temperature";
inline constexpr std::string_view STREAM_RAIN = "geo.climate.rainfall";
inline constexpr std::string_view STREAM_TEST = "test.climate";

// D8 vectors (P1-00 2.3).
inline constexpr std::array<std::int32_t, 8> DX = {1, 1, 0, -1, -1, -1, 0, 1};
inline constexpr std::array<std::int32_t, 8> DY = {0, 1, 1, 1, 0, -1, -1, -1};

[[nodiscard]] inline bool diagonal(std::uint8_t code) { return DX[code] != 0 && DY[code] != 0; }

// --- Tuning text (P1-03 4) ---------------------------------------------------------

using elev_test::FieldText;
using elev_test::Overrides;

// The shipped file of section 4, in schema declaration order (identical to
// the `geo` and `geo-small` fixture files).
inline const std::vector<FieldText>& shipped_fields() {
    static const std::vector<FieldText> f = {
        {"temp_north_c", "5"},
        {"temp_south_c", "9"},
        {"lapse_c_per_km", "\"6.5\""},
        {"temp_noise", "{ wavelength_m = 6000, octaves = 3, gain = \"0.5\" }"},
        {"temp_noise_amp_c", "1"},
        {"wind_weights", "[1, 1, 1, 1, 1, 1, 1, 1]"},
        {"smooth_radius_m", "320"},
        {"rain_base_mm", "800"},
        {"rain_oro_mm", "2500"},
        {"moisture_path_mm_km", "32000"},
        {"loss_max", "\"0.25\""},
        {"ocean_recharge_m", "500"},
        {"coast_rain_mm", "75"},
        {"coast_rain_reach_m", "1500"},
        {"rain_noise", "{ wavelength_m = 5000, octaves = 3, gain = \"0.5\" }"},
        {"rain_noise_amp", "\"0.4\""},
        {"rain_min_mm", "250"},
    };
    return f;
}

// The exact text of the shipped file's body (section 4's TOML block).
inline constexpr std::string_view SHIPPED_TOML =
    "[worldgen.climate]\n"
    "temp_north_c = 5\n"
    "temp_south_c = 9\n"
    "lapse_c_per_km = \"6.5\"\n"
    "temp_noise = { wavelength_m = 6000, octaves = 3, gain = \"0.5\" }\n"
    "temp_noise_amp_c = 1\n"
    "wind_weights = [1, 1, 1, 1, 1, 1, 1, 1]\n"
    "smooth_radius_m = 320\n"
    "rain_base_mm = 800\n"
    "rain_oro_mm = 2500\n"
    "moisture_path_mm_km = 32000\n"
    "loss_max = \"0.25\"\n"
    "ocean_recharge_m = 500\n"
    "coast_rain_mm = 75\n"
    "coast_rain_reach_m = 1500\n"
    "rain_noise = { wavelength_m = 5000, octaves = 3, gain = \"0.5\" }\n"
    "rain_noise_amp = \"0.4\"\n"
    "rain_min_mm = 250\n";

[[nodiscard]] inline bool is_int_field(std::string_view name) {
    static const std::vector<std::string_view> ints = {
        "smooth_radius_m",  "rain_base_mm",  "rain_oro_mm",        "moisture_path_mm_km", "ocean_recharge_m",
        "coast_rain_mm",    "coast_rain_reach_m", "rain_min_mm",
    };
    return std::find(ints.begin(), ints.end(), name) != ints.end();
}

// [worldgen.climate] with each field's value from `overrides` (last one
// wins) when listed, the shipped value otherwise; `omit` drops a field.
inline std::string climate_toml(const Overrides& overrides = {}, std::string_view omit = {}) {
    std::string s = "[worldgen.climate]\n";
    for (const FieldText& f : shipped_fields()) {
        if (f.name == omit) {
            continue;
        }
        std::string value(f.value);
        for (const auto& o : overrides) {
            if (o.first == f.name) {
                value = o.second;
            }
        }
        s += std::string(f.name) + " = " + value + "\n";
    }
    return s;
}

// The TOML text of a wind_weights array.
inline std::string weights_text(const std::array<std::int64_t, 8>& w) {
    std::string s = "[";
    for (std::size_t i = 0; i < w.size(); ++i) {
        s += (i > 0 ? ", " : "") + std::to_string(w[i]);
    }
    return s + "]";
}

// One-hot weights: weight 5 at code k (5.1 uses [0, 0, 0, 0, 5, 0, 0, 0]).
inline std::array<std::int64_t, 8> one_hot(std::uint8_t k) {
    std::array<std::int64_t, 8> w{};
    w[k] = 5;
    return w;
}

inline constexpr std::array<std::int64_t, 8> UNIFORM = {1, 1, 1, 1, 1, 1, 1, 1};

// Overrides that fix the wind to code k.
inline Overrides wind_overrides(std::uint8_t k) { return {{"wind_weights", weights_text(one_hot(k))}}; }

// Both noise amplitudes 0 (5.1: "Noise amplitudes are 0 unless stated").
inline Overrides quiet_overrides() { return {{"temp_noise_amp_c", "0"}, {"rain_noise_amp", "0"}}; }

// The full fixture data root in memory: the shipped elevation and hydrology
// fixtures with the given climate text.
inline climate_api::TuningLoad load(std::string_view climate) {
    return climate_api::load_tuning_text(elev_test::elevation_toml(), hydro_test::hydrology_toml(hydro_test::SHIPPED_AREA),
                                         climate);
}

inline climate_api::Tuning tuning(const Overrides& o = {}) {
    // Loaded once per distinct text in a test run (a pure function of the
    // text; saves re-parsing the three files for every fixture).
    static std::map<std::string, climate_api::Tuning> cache;
    const std::string text = climate_toml(o);
    const auto it = cache.find(text);
    if (it != cache.end()) {
        return it->second;
    }
    const climate_api::TuningLoad l = load(text);
    if (!l.ok()) {
        std::string errors;
        for (const auto& e : l.errors) {
            errors += e.to_string() + "\n";
        }
        FAIL("climate.toml does not load: " << errors);
    }
    const climate_api::Tuning t = climate_api::tuning_from(*l.content);
    cache.emplace(text, t);
    return t;
}

inline const climate_api::Tuning& shipped_tuning() {
    static const climate_api::Tuning t = tuning();
    return t;
}

// The tuning values as the test reads them from its own text.
struct Values {
    std::int64_t temp_north_c = 0;  // raw
    std::int64_t temp_south_c = 0;
    std::int64_t lapse_c_per_km = 0;
    worldgen::geo::FbmM temp_noise;
    std::int64_t temp_noise_amp_c = 0;
    std::array<std::int64_t, 8> wind_weights{};
    std::int64_t smooth_radius_m = 0;
    std::int64_t rain_base_mm = 0;
    std::int64_t rain_oro_mm = 0;
    std::int64_t moisture_path_mm_km = 0;
    std::int64_t loss_max = 0;  // raw
    std::int64_t ocean_recharge_m = 0;
    std::int64_t coast_rain_mm = 0;
    std::int64_t coast_rain_reach_m = 0;
    worldgen::geo::FbmM rain_noise;
    std::int64_t rain_noise_amp = 0;  // raw
    std::int64_t rain_min_mm = 0;
};

inline std::array<std::int64_t, 8> parse_weights(std::string_view text) {
    std::array<std::int64_t, 8> w{};
    std::string body(text.substr(1, text.size() - 2));
    std::stringstream ss(body);
    std::string part;
    std::size_t i = 0;
    while (std::getline(ss, part, ',')) {
        REQUIRE(i < 8U);
        w[i++] = std::stoll(part);
    }
    REQUIRE(i == 8U);
    return w;
}

inline Values values(const Overrides& overrides = {}) {
    auto text_of = [&overrides](std::string_view name) {
        std::string value;
        for (const FieldText& f : shipped_fields()) {
            if (f.name == name) {
                value = std::string(f.value);
            }
        }
        for (const auto& o : overrides) {
            if (o.first == name) {
                value = o.second;
            }
        }
        REQUIRE_MESSAGE(!value.empty(), "unknown field " << std::string(name));
        return value;
    };
    auto fx = [&](std::string_view n) { return elev_test::parse_value(text_of(n), true).scalar; };
    auto in = [&](std::string_view n) { return elev_test::parse_value(text_of(n), false).scalar; };
    auto rec = [&](std::string_view n) {
        const elev_test::FieldValue v = elev_test::parse_value(text_of(n), false);
        return worldgen::geo::FbmM{v.rec_a, v.rec_octaves, Fx::from_raw(v.rec_gain)};
    };
    Values v;
    v.temp_north_c = fx("temp_north_c");
    v.temp_south_c = fx("temp_south_c");
    v.lapse_c_per_km = fx("lapse_c_per_km");
    v.temp_noise = rec("temp_noise");
    v.temp_noise_amp_c = fx("temp_noise_amp_c");
    v.wind_weights = parse_weights(text_of("wind_weights"));
    v.smooth_radius_m = in("smooth_radius_m");
    v.rain_base_mm = in("rain_base_mm");
    v.rain_oro_mm = in("rain_oro_mm");
    v.moisture_path_mm_km = in("moisture_path_mm_km");
    v.loss_max = fx("loss_max");
    v.ocean_recharge_m = in("ocean_recharge_m");
    v.coast_rain_mm = in("coast_rain_mm");
    v.coast_rain_reach_m = in("coast_rain_reach_m");
    v.rain_noise = rec("rain_noise");
    v.rain_noise_amp = fx("rain_noise_amp");
    v.rain_min_mm = in("rain_min_mm");
    return v;
}

// --- The test's own C2 to C6 ----------------------------------------------------------

[[nodiscard]] inline Fx R(std::int64_t raw) { return Fx::from_raw(raw); }

// C2: k_w = Rng(stream_key(seed, "geo.climate.wind")).weighted_index(weights).
inline std::uint8_t own_wind(std::uint64_t seed, const std::array<std::int64_t, 8>& w) {
    worldgen::Rng rng(worldgen::stream_key(seed, STREAM_WIND));
    std::array<std::uint64_t, 8> u{};
    for (std::size_t i = 0; i < 8; ++i) {
        u[i] = static_cast<std::uint64_t>(w[i]);
    }
    return static_cast<std::uint8_t>(rng.weighted_index(u));
}

struct TempParts {
    std::int64_t t_sea = 0, lap = 0, tn = 0, T = 0;  // raw
    std::int16_t temperature = 0;
};

// C3 for one cell of row y on a map of the given height.
inline TempParts own_temperature(std::int32_t y, std::int32_t height, std::int64_t s, std::int64_t noise_raw,
                                 const Values& v) {
    using worldgen::mul;
    const Fx V = Fx::from_ratio(2 * std::int64_t{y} + 1, 2 * std::int64_t{height});
    const Fx t_sea = R(v.temp_north_c) + mul(R(v.temp_south_c) - R(v.temp_north_c), V);
    const Fx lap = worldgen::div_int(worldgen::mul_int(R(v.lapse_c_per_km), s), 10000);
    const Fx tn = mul(R(v.temp_noise_amp_c), R(noise_raw));
    const Fx T = (t_sea - lap) + tn;
    const std::int64_t t10 = worldgen::mul_int(T, 10).round_int();
    TempParts p;
    p.t_sea = t_sea.raw();
    p.lap = lap.raw();
    p.tn = tn.raw();
    p.T = T.raw();
    p.temperature = static_cast<std::int16_t>(std::clamp(t10, TEMP_MIN, TEMP_MAX));
    return p;
}

// fbm_m(stream_key(seed, stream), X, Y, rec) at every cell centre (raw).
inline std::vector<std::int64_t> noise_grid(std::uint64_t seed, std::string_view stream, Grid g,
                                            const worldgen::geo::FbmM& rec) {
    const std::uint64_t key = worldgen::stream_key(seed, stream);
    std::vector<std::int64_t> out(g.cells());
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            out[g.index(x, y)] =
                worldgen::geo::fbm_m(key, Fx::from_int(16 * x + 8), Fx::from_int(16 * y + 8), rec).raw();
        }
    }
    return out;
}

// One box pass of half-width r on integers S (seam climate_box_mean, pass 1
// of C4), by a 2-D prefix sum (a second method to the spec's separable
// passes): hs1 = from_ratio(sum, c_w).
inline std::vector<std::int64_t> own_box_mean(Grid g, std::span<const std::int64_t> s, std::int64_t r) {
    const std::size_t w = static_cast<std::size_t>(g.width), h = static_cast<std::size_t>(g.height);
    std::vector<std::int64_t> P((w + 1) * (h + 1), 0);
    for (std::size_t y = 0; y < h; ++y) {
        for (std::size_t x = 0; x < w; ++x) {
            P[(y + 1) * (w + 1) + x + 1] = s[y * w + x] + P[y * (w + 1) + x + 1] + P[(y + 1) * (w + 1) + x] - P[y * (w + 1) + x];
        }
    }
    std::vector<std::int64_t> out(w * h);
    for (std::int64_t y = 0; y < g.height; ++y) {
        for (std::int64_t x = 0; x < g.width; ++x) {
            const std::int64_t x0 = std::max<std::int64_t>(0, x - r), x1 = std::min<std::int64_t>(g.width - 1, x + r);
            const std::int64_t y0 = std::max<std::int64_t>(0, y - r), y1 = std::min<std::int64_t>(g.height - 1, y + r);
            auto at = [&](std::int64_t yy, std::int64_t xx) {
                return P[static_cast<std::size_t>(yy) * (w + 1) + static_cast<std::size_t>(xx)];
            };
            const std::int64_t sum = at(y1 + 1, x1 + 1) - at(y0, x1 + 1) - at(y1 + 1, x0) + at(y0, x0);
            out[static_cast<std::size_t>(y) * w + static_cast<std::size_t>(x)] =
                Fx::from_ratio(sum, (x1 - x0 + 1) * (y1 - y0 + 1)).raw();
        }
    }
    return out;
}

// C4, the triangle kernel: h = r div 2; pass 1 own_box_mean(S, h); pass 2
// on the raw values of hs1, hs = div_int(from_raw(sum2), c_w). Pass 2 sums
// each window directly, a column of row sums at a time, so every partial sum
// is part of one window and stays under the bound of section 7 (a
// whole-grid prefix sum of raw values could overflow int64).
inline std::vector<std::int64_t> own_triangle_mean(Grid g, std::span<const std::int64_t> s, std::int64_t r) {
    const std::int64_t h = r / 2;
    const std::vector<std::int64_t> hs1 = own_box_mean(g, s, h);
    const std::int64_t W = g.width, H = g.height;
    std::vector<std::int64_t> rowsum(hs1.size());
    for (std::int64_t y = 0; y < H; ++y) {
        for (std::int64_t x = 0; x < W; ++x) {
            std::int64_t sum = 0;
            for (std::int64_t xx = std::max<std::int64_t>(0, x - h); xx <= std::min<std::int64_t>(W - 1, x + h); ++xx) {
                sum += hs1[static_cast<std::size_t>(y * W + xx)];
            }
            rowsum[static_cast<std::size_t>(y * W + x)] = sum;
        }
    }
    std::vector<std::int64_t> out(hs1.size());
    for (std::int64_t y = 0; y < H; ++y) {
        const std::int64_t y0 = std::max<std::int64_t>(0, y - h), y1 = std::min<std::int64_t>(H - 1, y + h);
        for (std::int64_t x = 0; x < W; ++x) {
            const std::int64_t x0 = std::max<std::int64_t>(0, x - h), x1 = std::min<std::int64_t>(W - 1, x + h);
            std::int64_t sum = 0;
            for (std::int64_t yy = y0; yy <= y1; ++yy) {
                sum += rowsum[static_cast<std::size_t>(yy * W + x)];
            }
            out[static_cast<std::size_t>(y * W + x)] =
                worldgen::div_int(Fx::from_raw(sum), (x1 - x0 + 1) * (y1 - y0 + 1)).raw();
        }
    }
    return out;
}

// The taps a, b (weight 2), c of cell (x, y) for wind code k (C5).
inline std::array<std::pair<std::int32_t, std::int32_t>, 3> taps(std::uint8_t k, std::int32_t x, std::int32_t y) {
    const std::int32_t dx = DX[k], dy = DY[k];
    if (dx != 0 && dy != 0) {
        return {{{x - dx, y}, {x - dx, y - dy}, {x, y - dy}}};
    }
    if (dx != 0) {
        return {{{x - dx, y - 1}, {x - dx, y}, {x - dx, y + 1}}};
    }
    return {{{x - 1, y - dy}, {x, y - dy}, {x + 1, y - dy}}};
}

// q_up and hs_up of a cell from a q grid and an hs grid (off-map taps: q =
// ONE, hs = 0).
inline std::pair<std::int64_t, std::int64_t> upwind(Grid g, std::uint8_t k, std::int32_t x, std::int32_t y,
                                                    std::span<const std::int64_t> q, std::span<const std::int64_t> hs) {
    std::array<std::int64_t, 3> qa{}, ha{};
    const auto t = taps(k, x, y);
    for (std::size_t j = 0; j < 3; ++j) {
        const auto [xx, yy] = t[j];
        if (xx >= 0 && yy >= 0 && xx < g.width && yy < g.height) {
            qa[j] = q[g.index(xx, yy)];
            ha[j] = hs[g.index(xx, yy)];
        } else {
            qa[j] = ONE;
            ha[j] = 0;
        }
    }
    const Fx q_up = worldgen::div_int((R(qa[0]) + worldgen::mul_int(R(qa[1]), 2)) + R(qa[2]), 4);
    const Fx hs_up = worldgen::div_int((R(ha[0]) + worldgen::mul_int(R(ha[1]), 2)) + R(ha[2]), 4);
    return {q_up.raw(), hs_up.raw()};
}

struct SweepConsts {
    Fx step_m, step_dm, rec;
};

inline SweepConsts sweep_consts(std::uint8_t k, const Values& v) {
    SweepConsts c;
    c.step_m = diagonal(k) ? worldgen::mul_int(Fx::SQRT2, 16) : Fx::from_int(16);
    c.step_dm = diagonal(k) ? worldgen::mul_int(Fx::SQRT2, 160) : Fx::from_int(160);
    c.rec = worldgen::min(Fx::ONE, worldgen::div(c.step_m, Fx::from_int(v.ocean_recharge_m)));
    return c;
}

struct CellStep {
    std::int64_t q = 0, p = 0, rise = 0, g = 0, lam = 0;  // raw; rise, g, lam 0 on ocean
};

inline CellStep own_cell(bool ocean, std::int64_t hs_c, std::int64_t q_up_raw, std::int64_t hs_up_raw,
                         const SweepConsts& c, const Values& v) {
    using worldgen::mul;
    const Fx q_up = R(q_up_raw);
    const Fx base = Fx::from_int(v.rain_base_mm);
    CellStep s;
    if (ocean) {
        s.q = (q_up + mul(Fx::ONE - q_up, c.rec)).raw();
        s.p = mul(q_up, base).raw();
        return s;
    }
    const Fx rise = worldgen::max(Fx::from_raw(0), R(hs_c) - R(hs_up_raw));
    const Fx g = worldgen::div(rise, c.step_dm);
    const Fx p = mul(q_up, base + worldgen::mul_int(g, v.rain_oro_mm));
    const Fx lam = worldgen::min(R(v.loss_max), worldgen::div_int(mul(p, c.step_m), 1000 * v.moisture_path_mm_km));
    s.q = (q_up - mul(q_up, lam)).raw();
    s.p = p.raw();
    s.rise = rise.raw();
    s.g = g.raw();
    s.lam = lam.raw();
    return s;
}

struct OwnSweep {
    std::vector<std::int64_t> q, p, q_up, hs_up, rise;
};

// C5 in the stated order: orthogonal winds by lines (columns when dx != 0,
// rows otherwise), each line read from the previous line only; diagonal
// winds in raster order (rows in the order of dy, cells in the order of
// dx), in place. A tap is read only after it is computed (checked).
inline OwnSweep own_sweep(Grid g, std::span<const std::int64_t> hs, std::span<const std::uint8_t> water,
                          std::uint8_t k, const Values& v) {
    const SweepConsts c = sweep_consts(k, v);
    OwnSweep out;
    const std::size_t n = g.cells();
    out.q.assign(n, -1);  // -1: not computed yet
    out.p.assign(n, 0);
    out.q_up.assign(n, 0);
    out.hs_up.assign(n, 0);
    out.rise.assign(n, 0);
    const std::int32_t dx = DX[k], dy = DY[k];
    auto visit = [&](std::int32_t x, std::int32_t y) {
        for (const auto& [xx, yy] : taps(k, x, y)) {
            if (xx >= 0 && yy >= 0 && xx < g.width && yy < g.height && out.q[g.index(xx, yy)] < 0) {
                FAIL("C5 order: a tap is read before it is computed");
            }
        }
        const auto [q_up, hs_up] = upwind(g, k, x, y, out.q, hs);
        const std::size_t i = g.index(x, y);
        const CellStep st = own_cell(water[i] == WATER_OCEAN, hs[i], q_up, hs_up, c, v);
        out.q[i] = st.q;
        out.p[i] = st.p;
        out.q_up[i] = q_up;
        out.hs_up[i] = hs_up;
        out.rise[i] = st.rise;
    };
    if (diagonal(k)) {
        for (std::int32_t yi = 0; yi < g.height; ++yi) {
            const std::int32_t y = dy == 1 ? yi : g.height - 1 - yi;
            for (std::int32_t xi = 0; xi < g.width; ++xi) {
                visit(dx == 1 ? xi : g.width - 1 - xi, y);
            }
        }
        return out;
    }
    const bool columns = dx != 0;
    const std::int32_t lines = columns ? g.width : g.height;
    const std::int32_t len = columns ? g.height : g.width;
    for (std::int32_t li = 0; li < lines; ++li) {
        const std::int32_t line = columns ? (dx == 1 ? li : g.width - 1 - li) : (dy == 1 ? li : g.height - 1 - li);
        for (std::int32_t j = 0; j < len; ++j) {
            visit(columns ? line : j, columns ? j : line);
        }
    }
    return out;
}

struct RainParts {
    std::int64_t f = 0, pr = 0, cb = 0;  // raw
    std::uint16_t rainfall = 0;
};

// C6 for one cell.
inline RainParts own_rainfall(std::int64_t p_raw, std::int64_t coast_dist, std::int64_t rn_raw, const Values& v) {
    using worldgen::mul;
    RainParts r;
    const Fx f = worldgen::max(Fx::from_raw(0), Fx::ONE + mul(R(v.rain_noise_amp), R(rn_raw)));
    const Fx pr = mul(R(p_raw), f);
    const std::int64_t d = std::max<std::int64_t>(0, coast_dist);
    const Fx cb = d >= v.coast_rain_reach_m
                      ? Fx::from_raw(0)
                      : Fx::from_ratio(v.coast_rain_mm * (v.coast_rain_reach_m - d), v.coast_rain_reach_m);
    r.f = f.raw();
    r.pr = pr.raw();
    r.cb = cb.raw();
    r.rainfall = static_cast<std::uint16_t>(std::clamp((pr + cb).round_int(), v.rain_min_mm, RAIN_MAX));
    return r;
}

// --- Inputs ------------------------------------------------------------------------------

struct Inputs {
    Grid grid;
    std::vector<std::int16_t> elevation;
    std::vector<std::uint8_t> water;
    std::vector<std::uint16_t> water_depth;
    std::vector<std::int16_t> coast_dist;
};

inline std::vector<std::int64_t> surface(const Inputs& in) {
    std::vector<std::int64_t> s(in.elevation.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = std::int64_t{in.elevation[i]} + std::int64_t{in.water_depth[i]};
    }
    return s;
}

// The input layers of a world: the elevation stage (P1-01 fixture) and the
// hydrology stage with the shipped tuning and the seed's flat_tie (P1-02 8).
// Computed once per (seed, N, fixture) in a test run and kept (a pure
// function of its arguments, so the cache cannot make tests depend on
// their order; it only saves recomputing upstream stages).
inline Inputs world_inputs(std::uint64_t seed, std::int32_t n, bool small) {
    static std::map<std::tuple<std::uint64_t, std::int32_t, bool>, Inputs> cache;
    const auto key = std::make_tuple(seed, n, small);
    const auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }
    Inputs in;
    in.grid = Grid{n, n};
    in.elevation = hydro_api::elevation_grid(seed, n, hydro_test::elevation_tuning(small), THREADS);
    const hydro_api::Layers h =
        hydro_api::world_layers(seed, in.grid, in.elevation, hydro_test::shipped_tuning(), THREADS);
    in.water = h.water;
    in.water_depth = h.water_depth;
    in.coast_dist = h.coast_dist;
    if (n <= 256) {  // keep only small worlds
        cache.emplace(key, in);
    }
    return in;
}

// Inputs from rows of S with a water grid: ocean cells get elevation -5 and
// water_depth 5 (S = 0), other cells elevation S and depth 0.
inline Inputs inputs_of_surface(Grid g, std::span<const std::int64_t> s, std::span<const std::uint8_t> water,
                                std::span<const std::int16_t> coast) {
    Inputs in;
    in.grid = g;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const bool ocean = water[i] == WATER_OCEAN;
        in.elevation.push_back(static_cast<std::int16_t>(ocean ? -5 : s[i]));
        in.water_depth.push_back(static_cast<std::uint16_t>(ocean ? 5 : 0));
    }
    in.water.assign(water.begin(), water.end());
    in.coast_dist.assign(coast.begin(), coast.end());
    return in;
}

inline std::vector<std::uint8_t> ocean_mask(std::span<const std::uint8_t> water) {
    std::vector<std::uint8_t> o(water.size());
    for (std::size_t i = 0; i < o.size(); ++i) {
        o[i] = water[i] == WATER_OCEAN ? 1 : 0;
    }
    return o;
}

// Random fixture f of 10.2 (the test writer's construction, as in
// reference/p1_climate_reference.py random_fixture): 34 x 34, the 32 x 32
// interior land (S = elevation in [1, 3000], water 0, depth 0), the border
// ocean (elevation -10, water 3, depth 10). Draws from
// Rng(substream(stream_key(0, "test.climate"), f)): the wind code
// uniform_u64(8) first, then uniform_i64(1, 3000) per interior cell in index
// order. coast_dist is P1-02 H7 on the ocean mask.
struct RandomFixture {
    Inputs in;
    std::uint8_t wind = 0;
};

inline RandomFixture random_fixture(std::uint64_t f) {
    worldgen::Rng rng(worldgen::substream(worldgen::stream_key(0, STREAM_TEST), f));
    RandomFixture r;
    r.wind = static_cast<std::uint8_t>(rng.uniform_u64(8));
    r.in.grid = Grid{34, 34};
    for (std::int32_t y = 0; y < 34; ++y) {
        for (std::int32_t x = 0; x < 34; ++x) {
            const bool border = x == 0 || y == 0 || x == 33 || y == 33;
            if (border) {
                r.in.elevation.push_back(-10);
                r.in.water.push_back(WATER_OCEAN);
                r.in.water_depth.push_back(10);
            } else {
                r.in.elevation.push_back(static_cast<std::int16_t>(rng.uniform_i64(1, 3000)));
                r.in.water.push_back(WATER_DRY);
                r.in.water_depth.push_back(0);
            }
        }
    }
    r.in.coast_dist = hydro_api::coast_distance(r.in.grid, ocean_mask(r.in.water));
    return r;
}

// --- The test's whole stage -----------------------------------------------------------------

struct Own {
    std::uint8_t wind = 0;
    std::vector<std::int64_t> S, tnoise, T;
    std::vector<std::int16_t> temperature;
    std::int64_t r = 0;
    std::vector<std::int64_t> hs;
    OwnSweep sweep;
    std::vector<std::int64_t> rnoise;
    std::vector<std::uint16_t> rainfall;
};

inline Own own_climate(const Inputs& in, const Values& v, std::uint64_t seed) {
    Own o;
    const Grid g = in.grid;
    o.wind = own_wind(seed, v.wind_weights);
    o.S = surface(in);
    o.tnoise = noise_grid(seed, STREAM_TEMP, g, v.temp_noise);
    o.T.resize(g.cells());
    o.temperature.resize(g.cells());
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const std::size_t i = g.index(x, y);
            const TempParts t = own_temperature(y, g.height, o.S[i], o.tnoise[i], v);
            o.T[i] = t.T;
            o.temperature[i] = t.temperature;
        }
    }
    o.r = v.smooth_radius_m / 16;
    o.hs = own_triangle_mean(g, o.S, o.r);
    o.sweep = own_sweep(g, o.hs, in.water, o.wind, v);
    o.rnoise = noise_grid(seed, STREAM_RAIN, g, v.rain_noise);
    o.rainfall.resize(g.cells());
    for (std::size_t i = 0; i < o.rainfall.size(); ++i) {
        o.rainfall[i] = own_rainfall(o.sweep.p[i], in.coast_dist[i], o.rnoise[i], v).rainfall;
    }
    return o;
}

inline climate_api::Layers stage_layers(const Inputs& in, const climate_api::Tuning& t, std::uint64_t seed,
                                        std::uint32_t threads) {
    return climate_api::layers(in.grid, in.elevation, in.water, in.water_depth, in.coast_dist, t, seed, threads);
}

// --- Grid transforms (10.1 test 4) -----------------------------------------------------------

enum class Transform : std::uint8_t { mirror_x, mirror_y, transpose, rotate };

// The grid after the transform: mirror_x (x -> W-1-x), mirror_y, transpose
// ((x, y) -> (y, x)) and rotate (90 degrees clockwise: (x, y) -> (H-1-y, x)
// on the H x W result).
inline Grid transformed_grid(Grid g, Transform t) {
    return (t == Transform::transpose || t == Transform::rotate) ? Grid{g.height, g.width} : g;
}

// Source cell (in g) of the cell (x2, y2) of the transformed grid.
inline std::pair<std::int32_t, std::int32_t> source_of(Grid g, Transform t, std::int32_t x2, std::int32_t y2) {
    switch (t) {
        case Transform::mirror_x:
            return {g.width - 1 - x2, y2};
        case Transform::mirror_y:
            return {x2, g.height - 1 - y2};
        case Transform::transpose:
            return {y2, x2};
        case Transform::rotate:
            return {y2, g.height - 1 - x2};
    }
    return {x2, y2};
}

template <typename T>
inline std::vector<T> transform(Grid g, std::span<const T> v, Transform t) {
    const Grid g2 = transformed_grid(g, t);
    std::vector<T> out(g2.cells());
    for (std::int32_t y = 0; y < g2.height; ++y) {
        for (std::int32_t x = 0; x < g2.width; ++x) {
            const auto [sx, sy] = source_of(g, t, x, y);
            out[g2.index(x, y)] = v[g.index(sx, sy)];
        }
    }
    return out;
}

// The wind code after the transform.
inline std::uint8_t code_under(Transform t, std::uint8_t k) {
    std::int32_t dx = DX[k], dy = DY[k];
    switch (t) {
        case Transform::mirror_x:
            dx = -dx;
            break;
        case Transform::mirror_y:
            dy = -dy;
            break;
        case Transform::transpose:
            std::swap(dx, dy);
            break;
        case Transform::rotate: {  // (x, y) -> (H-1-y, x): a vector (dx, dy) -> (-dy, dx)
            const std::int32_t ndx = -dy;
            dy = dx;
            dx = ndx;
            break;
        }
    }
    for (std::uint8_t c = 0; c < 8; ++c) {
        if (DX[c] == dx && DY[c] == dy) {
            return c;
        }
    }
    FAIL("no D8 code");
    return 0;
}

// --- Invariants of section 6 -----------------------------------------------------------------

// Invariants 1, 3 and 4 for one world, from the stage's layers and the
// stage's sweep (q, p) on the given hs (by code independent of the stage:
// q_up is recomputed here from the stage's q grid). Returns "" or the first
// violation.
inline std::string invariant_violation(const Inputs& in, const climate_api::Layers& l, std::span<const std::int64_t> hs,
                                       const climate_api::Sweep& sw, std::uint8_t wind, const Values& v) {
    const Grid g = in.grid;
    std::ostringstream err;
    for (std::size_t i = 0; i < g.cells(); ++i) {
        if (l.temperature[i] < TEMP_MIN || l.temperature[i] > TEMP_MAX) {
            err << "inv 1: temperature " << l.temperature[i] << " at " << i;
            return err.str();
        }
        if (l.rainfall[i] < v.rain_min_mm || l.rainfall[i] > RAIN_MAX) {
            err << "inv 1: rainfall " << l.rainfall[i] << " at " << i;
            return err.str();
        }
    }
    const Fx base = Fx::from_int(v.rain_base_mm);
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const std::size_t i = g.index(x, y);
            const auto [q_up, hs_up] = upwind(g, wind, x, y, sw.q, hs);
            if (sw.q[i] < 0 || sw.q[i] > ONE || q_up < 0 || q_up > ONE) {
                err << "inv 3: q " << sw.q[i] << " q_up " << q_up << " at (" << x << ", " << y << ")";
                return err.str();
            }
            if (in.water[i] == WATER_OCEAN) {
                continue;
            }
            if (sw.q[i] > q_up) {
                err << "inv 4: q " << sw.q[i] << " > q_up " << q_up << " at (" << x << ", " << y << ")";
                return err.str();
            }
            if (hs[i] <= hs_up && sw.p[i] != worldgen::mul(R(q_up), base).raw()) {
                err << "inv 4: lee cell (" << x << ", " << y << ") p " << sw.p[i] << " != mul(q_up, base)";
                return err.str();
            }
        }
    }
    return {};
}

// Invariant 7, second part: each hs lies between the smallest and the
// largest S of the cells within (Chebyshev) distance 2h, h = r div 2.
inline std::string hs_bound_violation(Grid g, std::span<const std::int64_t> s, std::int64_t r,
                                      std::span<const std::int64_t> hs) {
    const std::int64_t d = 2 * (r / 2);
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            std::int64_t lo = s[g.index(x, y)], hi = lo;
            for (std::int64_t yy = std::max<std::int64_t>(0, y - d); yy <= std::min<std::int64_t>(g.height - 1, y + d); ++yy) {
                for (std::int64_t xx = std::max<std::int64_t>(0, x - d); xx <= std::min<std::int64_t>(g.width - 1, x + d); ++xx) {
                    const std::int64_t v = s[static_cast<std::size_t>(yy * g.width + xx)];
                    lo = std::min(lo, v);
                    hi = std::max(hi, v);
                }
            }
            const std::int64_t v = hs[g.index(x, y)];
            if (v < Fx::from_int(lo).raw() || v > Fx::from_int(hi).raw()) {
                return "inv 7: hs at (" + std::to_string(x) + ", " + std::to_string(y) + ") outside [" +
                       std::to_string(lo) + ", " + std::to_string(hi) + "]";
            }
        }
    }
    return {};
}

// --- Hashes ----------------------------------------------------------------------------------

using elev_test::hash_i64;

inline std::string temperature_hash(Grid g, std::span<const std::int16_t> v) {
    return hydro_test::layer_hash<std::int16_t>(g, 4, v);
}
inline std::string rainfall_hash(Grid g, std::span<const std::uint16_t> v) {
    return hydro_test::layer_hash<std::uint16_t>(g, 3, v);
}

// --- Report values (P1-03 9, P1-00 10.3) -------------------------------------------------------

inline std::string metres1(std::int64_t v) {
    const std::int64_t a = v < 0 ? -v : v;
    return (v < 0 ? "-" : "") + std::to_string(a / 10) + "." + std::to_string(a % 10);
}

inline std::string ratio4(std::uint64_t a, std::uint64_t b) {
    if (b == 0U) {
        return "n/a";
    }
    const std::uint64_t num = a * 10000U;
    std::uint64_t q = num / b;
    if (2U * (num % b) >= b) {
        ++q;
    }
    std::string frac = std::to_string(q % 10000U);
    frac.insert(0, 4U - frac.size(), '0');
    return std::to_string(q / 10000U) + "." + frac;
}

template <typename T>
inline std::vector<std::int64_t> land_percentiles(std::span<const std::int16_t> elevation, std::span<const T> v) {
    std::vector<std::int64_t> land;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (elevation[i] > 0) {
            land.push_back(std::int64_t{v[i]});
        }
    }
    std::sort(land.begin(), land.end());
    std::vector<std::int64_t> out;
    if (land.empty()) {
        return out;
    }
    for (const std::uint64_t p : {0U, 10U, 50U, 90U, 100U}) {
        const std::uint64_t n = land.size();
        out.push_back(land[std::min<std::uint64_t>(n - 1U, (p * n) / 100U)]);
    }
    return out;
}

// The four climate keys of P1-03 9 with their values, from the layers.
inline std::vector<std::pair<std::string, std::string>> report_values(std::span<const std::int16_t> elevation,
                                                                      const climate_api::Layers& l) {
    std::vector<std::pair<std::string, std::string>> out;
    const std::vector<std::int64_t> tp = land_percentiles<std::int16_t>(elevation, l.temperature);
    const std::vector<std::int64_t> rp = land_percentiles<std::uint16_t>(elevation, l.rainfall);
    auto joined = [](const std::vector<std::int64_t>& p, bool tenths) {
        if (p.empty()) {
            return std::string("n/a");  // P1-00 appendix C 6, as elevation_m
        }
        std::string s;
        for (std::size_t i = 0; i < p.size(); ++i) {
            s += (i > 0 ? " " : "") + (tenths ? metres1(p[i]) : std::to_string(p[i]));
        }
        return s;
    };
    out.emplace_back("temperature_c", joined(tp, true));
    out.emplace_back("rainfall_mm", joined(rp, false));
    std::uint64_t land = 0, below = 0;
    for (std::size_t i = 0; i < elevation.size(); ++i) {
        if (elevation[i] > 0) {
            ++land;
            below += l.temperature[i] < 0 ? 1U : 0U;
        }
    }
    out.emplace_back("land_below_0c", ratio4(below, land));
    out.emplace_back("rain_contrast", rp.empty() || rp[1] == 0 ? std::string("n/a")
                                                              : ratio4(static_cast<std::uint64_t>(rp[3]),
                                                                       static_cast<std::uint64_t>(rp[1])));
    return out;
}

// --- Golden file (worldgen/tests/golden/p1-03-climate.txt) ---------------------------------

struct GoldenCase {
    std::string name;
    std::string fixture;  // shipped | small | random
    std::int32_t n = 0;
    std::uint64_t seed = 0;
    std::array<std::int64_t, 8> weights{};
    std::vector<std::pair<std::string, std::string>> items;  // in file (step) order

    [[nodiscard]] std::string get(std::string_view item) const {
        for (const auto& kv : items) {
            if (kv.first == item) {
                return kv.second;
            }
        }
        FAIL("golden case " << name << " has no item " << std::string(item));
        return {};
    }
};

inline std::vector<GoldenCase> read_golden(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << path);
    std::vector<GoldenCase> out;
    std::string line;
    while (std::getline(in, line)) {
        REQUIRE_MESSAGE((line.empty() || line.back() != '\r'), "golden file must be LF: " << path);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream ss(line);
        std::string a, b, c;
        ss >> a >> b >> c;
        if (a == "case") {
            GoldenCase gc;
            gc.name = b;
            gc.fixture = c;
            std::int64_t n = 0;
            std::string weights;
            ss >> n >> gc.seed >> weights;
            REQUIRE_MESSAGE(!ss.fail(), "bad case line: " << line);
            gc.n = static_cast<std::int32_t>(n);
            gc.weights = parse_weights("[" + weights + "]");
            out.push_back(gc);
        } else {
            REQUIRE_MESSAGE((!out.empty() && out.back().name == a), "golden line outside its case: " << line);
            out.back().items.emplace_back(b, c);
        }
    }
    return out;
}

// The overrides of a golden case's climate tuning (wind_weights only).
inline Overrides case_overrides(const GoldenCase& c) { return {{"wind_weights", weights_text(c.weights)}}; }

// The input layers of a golden case.
inline Inputs case_inputs(const GoldenCase& c) {
    if (c.fixture == "random") {
        return random_fixture(c.seed).in;
    }
    REQUIRE((c.fixture == "shipped" || c.fixture == "small"));
    return world_inputs(c.seed, c.n, c.fixture == "small");
}

}  // namespace climate_test
