#pragma once

// The test's own R2 to R6, inputs, golden-file reader and report values for
// the P1-05 resources tests (spec docs/specs/p1-05-resources-habitability.md),
// written from the spec text with the deterministic core (Fx, isqrt, fbm2)
// and the test's own G7 and G8 (p1_04_biomes_support.hpp) only; independent
// of the stage (every stage call is in p1_05_resources_api.hpp).

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <map>
#include <numeric>
#include <optional>
#include <ostream>
#include <queue>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "p1_01_elevation_support.hpp"
#include "p1_02_hydrology_support.hpp"
#include "p1_03_climate_support.hpp"
#include "p1_04_biomes_api.hpp"
#include "p1_04_biomes_data.hpp"
#include "p1_04_biomes_support.hpp"
#include "p1_05_resources_api.hpp"
#include "p1_05_resources_data.hpp"
#include "worldgen/core/decimal.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/math.hpp"
#include "worldgen/core/noise.hpp"
#include "worldgen/core/wide.hpp"
#include "worldgen/geo/grid.hpp"

namespace resource_test {

using worldgen::Fx;
using worldgen::geo::Grid;
using Overrides = std::vector<std::pair<std::string, std::string>>;

inline constexpr std::int64_t ONE = std::int64_t{1} << 32;
inline constexpr std::int64_t HALF = std::int64_t{1} << 31;
inline constexpr std::int64_t INF = std::int64_t{1} << 40;
inline constexpr std::string_view PRECONDITION = "resources: input precondition: ";
inline constexpr std::string_view STREAM_ORE = "geo.resources.ore";
inline constexpr std::uint32_t THREADS = 1;

inline constexpr std::uint8_t WATER_DRY = 0;
inline constexpr std::uint8_t WATER_RIVER = 1;
inline constexpr std::uint8_t WATER_LAKE = 2;
inline constexpr std::uint8_t WATER_OCEAN = 3;

inline constexpr std::array<std::int32_t, 8> DX = {1, 1, 0, -1, -1, -1, 0, 1};
inline constexpr std::array<std::int32_t, 8> DY = {0, 1, 1, 1, 0, -1, -1, -1};

inline bool is_land(std::uint8_t water) { return water == WATER_DRY || water == WATER_RIVER; }

// An fx value written as decimal text (quotes allowed), as raw.
inline std::int64_t fx_of(std::string_view text) { return biome_test::fx_of(text); }

// --- Own data (4.1, 4.2) -------------------------------------------------------------------

// P1-05 4.1: (fertility, timber, forage) by code, raw.
struct BiomeValues {
    std::int64_t fertility = 0, timber = 0, forage = 0;
};
inline const std::map<std::uint8_t, BiomeValues>& biome_values() {
    static const std::map<std::uint8_t, BiomeValues> v = {
        {1, {0, 0, 0}},
        {2, {0, 0, 0}},
        {3, {fx_of("0.7"), ONE, fx_of("0.5")}},
        {4, {ONE, fx_of("0.15"), fx_of("0.3")}},
        {5, {fx_of("0.45"), fx_of("0.3"), fx_of("0.8")}},
        {6, {fx_of("0.3"), fx_of("0.1"), fx_of("0.25")}},
        {7, {0, 0, fx_of("0.05")}},
        {8, {0, 0, 0}},
        {9, {fx_of("0.2"), fx_of("0.05"), fx_of("0.2")}},
        {10, {fx_of("0.7"), fx_of("0.05"), fx_of("0.35")}},
        {11, {fx_of("0.5"), fx_of("0.7"), fx_of("0.7")}},
        {12, {fx_of("0.25"), fx_of("0.2"), fx_of("0.6")}},
    };
    return v;
}

// The tuning of 4.2 as raw values (fx raw, ints as they are).
struct Values {
    std::int64_t fresh_reach_m = 400;
    std::int64_t fresh_rain_weight = fx_of("0.4");
    std::int64_t fresh_rain_lo_mm = 300;
    std::int64_t fresh_rain_hi_mm = 1500;
    std::int64_t fert_slope_lo = fx_of("0.18");
    std::int64_t fert_slope_hi = fx_of("0.47");
    std::int64_t alluvial_bonus = fx_of("0.3");
    std::int64_t alluvial_hand_m = 5 * ONE;
    std::int64_t timber_slope_lo = fx_of("0.7");
    std::int64_t timber_slope_hi = fx_of("1.4");
    std::int64_t fish_river_class_min = 4;
    std::int64_t fish_weight = fx_of("0.3");
    std::int64_t fish_reach_m = 300;
    worldgen::geo::FbmM ore_noise{1500, 3, Fx::from_raw(HALF)};
    std::int64_t ore_vein_lo = 0;
    std::int64_t ore_vein_hi = fx_of("0.2");
    std::int64_t ore_elev_lo_m = 100;
    std::int64_t ore_elev_hi_m = 800;
    std::int64_t ore_lowland = fx_of("0.15");
    std::int64_t ore_slope_base = HALF;
    std::int64_t ore_slope_lo = fx_of("0.3");
    std::int64_t ore_slope_hi = ONE;
    std::int64_t walk_slope_max = fx_of("0.7002");
    std::int64_t walk_min_area_m2 = 250000;
    std::int64_t hab_slope_lo = fx_of("0.18");
    std::int64_t hab_slope_hi = fx_of("0.47");
    std::int64_t hab_weight_fertility = 55;
    std::int64_t hab_weight_forage = 20;
    std::int64_t hab_weight_timber = 15;
    std::int64_t hab_weight_ore = 10;
    std::int64_t hab_water_min = fx_of("0.25");
    std::int64_t hab_flood_hand_m = ONE;
    std::int64_t hab_flood_factor = HALF;
    std::int64_t hab_pocket_factor = fx_of("0.25");

    [[nodiscard]] std::int64_t walk_min_cells() const { return (walk_min_area_m2 + 255) / 256; }
};

inline bool is_int_field(std::string_view k) {
    for (const auto& f : resource_data::tuning_fields()) {
        if (f.name == k) {
            return f.kind == "int";
        }
    }
    return false;
}

// Overrides as in the golden file: an integer, an fx as decimal text, or
// ore_noise as wavelength/octaves/gain.
inline Values values(const Overrides& o = {}) {
    Values v;
    std::map<std::string, std::int64_t*> fields = {
        {"fresh_reach_m", &v.fresh_reach_m},
        {"fresh_rain_weight", &v.fresh_rain_weight},
        {"fresh_rain_lo_mm", &v.fresh_rain_lo_mm},
        {"fresh_rain_hi_mm", &v.fresh_rain_hi_mm},
        {"fert_slope_lo", &v.fert_slope_lo},
        {"fert_slope_hi", &v.fert_slope_hi},
        {"alluvial_bonus", &v.alluvial_bonus},
        {"alluvial_hand_m", &v.alluvial_hand_m},
        {"timber_slope_lo", &v.timber_slope_lo},
        {"timber_slope_hi", &v.timber_slope_hi},
        {"fish_river_class_min", &v.fish_river_class_min},
        {"fish_weight", &v.fish_weight},
        {"fish_reach_m", &v.fish_reach_m},
        {"ore_vein_lo", &v.ore_vein_lo},
        {"ore_vein_hi", &v.ore_vein_hi},
        {"ore_elev_lo_m", &v.ore_elev_lo_m},
        {"ore_elev_hi_m", &v.ore_elev_hi_m},
        {"ore_lowland", &v.ore_lowland},
        {"ore_slope_base", &v.ore_slope_base},
        {"ore_slope_lo", &v.ore_slope_lo},
        {"ore_slope_hi", &v.ore_slope_hi},
        {"walk_slope_max", &v.walk_slope_max},
        {"walk_min_area_m2", &v.walk_min_area_m2},
        {"hab_slope_lo", &v.hab_slope_lo},
        {"hab_slope_hi", &v.hab_slope_hi},
        {"hab_weight_fertility", &v.hab_weight_fertility},
        {"hab_weight_forage", &v.hab_weight_forage},
        {"hab_weight_timber", &v.hab_weight_timber},
        {"hab_weight_ore", &v.hab_weight_ore},
        {"hab_water_min", &v.hab_water_min},
        {"hab_flood_hand_m", &v.hab_flood_hand_m},
        {"hab_flood_factor", &v.hab_flood_factor},
        {"hab_pocket_factor", &v.hab_pocket_factor},
    };
    for (const auto& [k, text] : o) {
        if (k == "ore_noise") {
            std::stringstream ss(text);
            std::string a, b, c;
            std::getline(ss, a, '/');
            std::getline(ss, b, '/');
            std::getline(ss, c, '/');
            v.ore_noise = worldgen::geo::FbmM{std::stoll(a), static_cast<std::int32_t>(std::stoll(b)),
                                              Fx::from_raw(fx_of(c))};
            continue;
        }
        const auto it = fields.find(k);
        REQUIRE_MESSAGE(it != fields.end(), "unknown tuning field " << k);
        *it->second = is_int_field(k) ? std::stoll(text) : fx_of(text);
    }
    return v;
}

// The overrides as TOML text (fx values quoted, ore_noise as an inline table).
inline Overrides toml_overrides(const Overrides& o) {
    Overrides out;
    for (const auto& [k, v] : o) {
        if (k == "ore_noise") {
            std::stringstream ss(v);
            std::string a, b, c;
            std::getline(ss, a, '/');
            std::getline(ss, b, '/');
            std::getline(ss, c, '/');
            out.emplace_back(k, "{ wavelength_m = " + a + ", octaves = " + b + ", gain = \"" + c + "\" }");
        } else {
            out.emplace_back(k, is_int_field(k) ? v : "\"" + v + "\"");
        }
    }
    return out;
}

inline std::string all_errors(const resource_api::Load& l) {
    std::string s;
    for (const auto& e : l.errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

// The loaded content with the shipped biome data (with `forage`) and the
// resources tuning with overrides; cached by text.
inline const worldgen::data::ContentSet& content(const Overrides& o = {}) {
    static std::map<std::string, worldgen::data::ContentSet> cache;
    const std::string text = resource_data::tuning_toml(toml_overrides(o));
    const auto it = cache.find(text);
    if (it != cache.end()) {
        return it->second;
    }
    const resource_api::Load l = resource_api::load(text, biome_data::directory());
    REQUIRE_MESSAGE(l.ok(), all_errors(l));
    return cache.emplace(text, *l.content).first->second;
}

inline const resource_api::Definitions& definitions() {
    static const resource_api::Definitions d = biome_api::definitions_from(content());
    return d;
}

inline resource_api::Tuning tuning(const Overrides& o = {}) { return resource_api::tuning_from(content(o)); }

// --- Own helpers (5) -------------------------------------------------------------------------

inline std::int64_t own_ramp(std::int64_t x, std::int64_t a, std::int64_t b) {
    if (!(a < b)) {
        FAIL("ramp with a >= b");
    }
    if (x <= a) {
        return 0;
    }
    if (x >= b) {
        return ONE;
    }
    return worldgen::div(Fx::from_raw(x - a), Fx::from_raw(b - a)).raw();
}

inline std::uint8_t own_q255(std::int64_t s) {
    const std::int64_t c = std::clamp<std::int64_t>(s, 0, ONE);
    const std::int64_t r = worldgen::mul_int(Fx::from_raw(c), 255).round_int();
    return static_cast<std::uint8_t>(std::clamp<std::int64_t>(r, 0, 255));
}

inline std::int64_t own_metres(std::int64_t d) { return std::min<std::int64_t>(32767, (32 * d + 5) / 10); }

// G8's tangent: div_int(from_raw(isqrt_u128(S2 x 2^64)), 320).
inline std::int64_t own_tangent(std::int64_t s2) {
    if (s2 < 0) {
        FAIL("S2 < 0");
    }
    const std::uint64_t r = worldgen::isqrt_u128(worldgen::U128{static_cast<std::uint64_t>(s2), 0});
    return worldgen::div_int(Fx::from_raw(static_cast<std::int64_t>(r)), 320).raw();
}

inline std::int64_t mul_raw(std::int64_t a, std::int64_t b) { return worldgen::mul(Fx::from_raw(a), Fx::from_raw(b)).raw(); }
inline std::int64_t from_int(std::int64_t n) { return Fx::from_int(n).raw(); }
inline std::int64_t from_ratio(std::int64_t a, std::int64_t b) { return Fx::from_ratio(a, b).raw(); }

// --- Own R3 -------------------------------------------------------------------------------------

// The chamfer of P1-02 H7 by Dijkstra over the 5/7 graph (not the spec's
// two passes; equal by construction of the chamfer), INF without a source.
inline std::vector<std::int64_t> own_chamfer(Grid g, std::span<const std::uint8_t> source) {
    std::vector<std::int64_t> d(g.cells(), INF);
    using Item = std::pair<std::int64_t, std::size_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> q;
    for (std::size_t i = 0; i < g.cells(); ++i) {
        if (source[i] != 0) {
            d[i] = 0;
            q.emplace(0, i);
        }
    }
    while (!q.empty()) {
        const auto [dc, c] = q.top();
        q.pop();
        if (dc != d[c]) {
            continue;
        }
        const std::int32_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(c));
        const std::int32_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(c));
        for (std::size_t k = 0; k < 8; ++k) {
            const std::int32_t mx = x + DX[k], my = y + DY[k];
            if (mx < 0 || my < 0 || mx >= g.width || my >= g.height) {
                continue;
            }
            const std::size_t m = g.index(mx, my);
            const std::int64_t v = dc + ((k & 1U) != 0 ? 7 : 5);
            if (v < d[m]) {
                d[m] = v;
                q.emplace(v, m);
            }
        }
    }
    return d;
}

inline std::vector<std::int64_t> metres_of(const std::vector<std::int64_t>& d) {
    std::vector<std::int64_t> out(d.size());
    std::transform(d.begin(), d.end(), out.begin(), own_metres);
    return out;
}

inline std::vector<std::uint8_t> fresh_sources(std::span<const std::uint8_t> water) {
    std::vector<std::uint8_t> s(water.size());
    for (std::size_t i = 0; i < water.size(); ++i) {
        s[i] = (water[i] == WATER_RIVER || water[i] == WATER_LAKE) ? 1 : 0;
    }
    return s;
}

inline std::vector<std::uint8_t> fish_sources(std::span<const std::uint8_t> water, std::span<const std::uint8_t> river,
                                              std::int64_t k) {
    std::vector<std::uint8_t> s(water.size());
    for (std::size_t i = 0; i < water.size(); ++i) {
        s[i] = (water[i] == WATER_LAKE || water[i] == WATER_OCEAN || (water[i] == WATER_RIVER && river[i] >= k)) ? 1 : 0;
    }
    return s;
}

struct FreshParts {
    std::int64_t fs = 0, fr = 0;
};
inline FreshParts own_fresh_parts(std::int64_t df, std::int64_t rainfall, const Values& v) {
    FreshParts p;
    p.fs = df >= v.fresh_reach_m ? 0 : from_ratio(v.fresh_reach_m - df, v.fresh_reach_m);
    p.fr = mul_raw(v.fresh_rain_weight,
                   own_ramp(from_int(rainfall), from_int(v.fresh_rain_lo_mm), from_int(v.fresh_rain_hi_mm)));
    return p;
}
inline std::uint8_t own_fresh(std::int64_t df, std::int64_t rainfall, const Values& v) {
    const FreshParts p = own_fresh_parts(df, rainfall, v);
    return own_q255(std::max(p.fs, p.fr));
}

// --- Own R4 -------------------------------------------------------------------------------------

struct FertParts {
    std::int64_t sf = 0, al = 0;
};
inline FertParts own_fert_parts(std::int64_t s2, std::int64_t hand, const Values& v) {
    FertParts p;
    p.sf = ONE - own_ramp(own_tangent(s2), v.fert_slope_lo, v.fert_slope_hi);
    const std::int64_t hm = from_ratio(hand, 10);
    p.al = ONE + mul_raw(v.alluvial_bonus, ONE - own_ramp(hm, 0, v.alluvial_hand_m));
    return p;
}
inline std::uint8_t own_fertility(std::int64_t fb, std::int64_t s2, std::int64_t hand, const Values& v) {
    const FertParts p = own_fert_parts(s2, hand, v);
    return own_q255(std::min(ONE, mul_raw(mul_raw(fb, p.sf), p.al)));
}

inline std::int64_t own_timber_factor(std::int64_t s2, const Values& v) {
    return ONE - own_ramp(own_tangent(s2), v.timber_slope_lo, v.timber_slope_hi);
}
inline std::uint8_t own_timber(std::int64_t tb, std::int64_t s2, const Values& v) {
    return own_q255(mul_raw(tb, own_timber_factor(s2, v)));
}

inline std::int64_t own_fish(std::int64_t dg, const Values& v) {
    return mul_raw(v.fish_weight, ONE - own_ramp(from_int(dg), 0, from_int(v.fish_reach_m)));
}
inline std::uint8_t own_forage(std::int64_t gb, std::int64_t dg, const Values& v) {
    return own_q255(std::min(ONE, gb + own_fish(dg, v)));
}

struct OreParts {
    std::int64_t eu = 0, su = 0, vn = 0;
};
inline OreParts own_ore_parts(std::int64_t elevation, std::int64_t s2, std::int64_t noise, const Values& v) {
    OreParts p;
    const std::int64_t em = from_ratio(elevation, 10);
    p.eu = std::max(v.ore_lowland, own_ramp(em, from_int(v.ore_elev_lo_m), from_int(v.ore_elev_hi_m)));
    p.su = v.ore_slope_base + mul_raw(ONE - v.ore_slope_base, own_ramp(own_tangent(s2), v.ore_slope_lo, v.ore_slope_hi));
    p.vn = own_ramp(noise, v.ore_vein_lo, v.ore_vein_hi);
    return p;
}
inline std::uint8_t own_ore(std::int64_t elevation, std::int64_t s2, std::int64_t noise, const Values& v) {
    const OreParts p = own_ore_parts(elevation, s2, noise, v);
    return own_q255(mul_raw(mul_raw(p.eu, p.su), p.vn));
}

// R4's noise at cell (x, y), by fbm2 itself (P0-01 8): base frequency
// from_ratio(1, wavelength_m), lacunarity 2.
inline std::int64_t own_noise(std::uint64_t key, std::int32_t x, std::int32_t y, const worldgen::geo::FbmM& rec) {
    return worldgen::fbm2(key, Fx::from_int(16 * x + 8), Fx::from_int(16 * y + 8), rec.octaves,
                          Fx::from_ratio(1, rec.wavelength_m), Fx::from_int(2), rec.gain)
        .raw();
}

// --- Own R5 -------------------------------------------------------------------------------------

inline bool own_walkable(std::uint8_t water, std::int64_t s2, std::int64_t walk_raw) {
    return is_land(water) && biome_test::not_steeper(s2, walk_raw);
}

// (dS)^2 x 2^64 <= w (160 raw)^2, w = 2 on a diagonal (U128).
inline bool own_linked(std::int64_t sa, std::int64_t sb, bool diagonal, std::int64_t walk_raw) {
    const std::int64_t d = sa - sb;
    const auto d2 = static_cast<std::uint64_t>(d * d);
    const auto m = static_cast<std::uint64_t>(160 * walk_raw);
    worldgen::U128 rhs = worldgen::mul_u64_wide(m, m);
    if (diagonal) {
        rhs = worldgen::U128{(rhs.hi << 1U) | (rhs.lo >> 63U), rhs.lo << 1U};
    }
    return worldgen::U128{d2, 0} <= rhs;
}

struct OwnWalk {
    std::vector<std::uint16_t> ids;
    std::int64_t kept = 0;
    std::int64_t dropped = 0;
};

// R5 by union-find over the linked pairs (each pair once: E, SE, S, SW),
// ids by smallest index (the spec allows any method giving the partition).
inline OwnWalk own_walk(Grid g, std::span<const std::int64_t> s, std::span<const std::uint8_t> water,
                        std::span<const std::int64_t> s2, std::int64_t walk_raw, std::int64_t min_cells) {
    const std::size_t n = g.cells();
    std::vector<std::size_t> parent(n);
    std::iota(parent.begin(), parent.end(), std::size_t{0});
    auto find = [&](std::size_t a) {
        while (parent[a] != a) {
            parent[a] = parent[parent[a]];
            a = parent[a];
        }
        return a;
    };
    std::vector<bool> wk(n);
    for (std::size_t i = 0; i < n; ++i) {
        wk[i] = own_walkable(water[i], s2[i], walk_raw);
    }
    for (std::size_t c = 0; c < n; ++c) {
        if (!wk[c]) {
            continue;
        }
        const std::int32_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(c));
        const std::int32_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(c));
        for (std::size_t k = 0; k < 4; ++k) {
            const std::int32_t mx = x + DX[k], my = y + DY[k];
            if (mx < 0 || my < 0 || mx >= g.width || my >= g.height) {
                continue;
            }
            const std::size_t m = g.index(mx, my);
            if (wk[m] && own_linked(s[c], s[m], (k & 1U) != 0, walk_raw)) {
                const std::size_t ra = find(c), rb = find(m);
                if (ra != rb) {
                    parent[std::max(ra, rb)] = std::min(ra, rb);
                }
            }
        }
    }
    std::map<std::size_t, std::int64_t> size;
    for (std::size_t c = 0; c < n; ++c) {
        if (wk[c]) {
            ++size[find(c)];
        }
    }
    // Roots are the smallest index of their part (parent links point down).
    std::map<std::size_t, std::uint16_t> id;
    OwnWalk w;
    for (const auto& [root, cells] : size) {
        if (cells >= min_cells) {
            ++w.kept;
            if (w.kept > 65535) {
                FAIL("more than 65535 walk areas");
            }
            id[root] = static_cast<std::uint16_t>(w.kept);
        } else {
            ++w.dropped;
        }
    }
    w.ids.assign(n, 0);
    for (std::size_t c = 0; c < n; ++c) {
        if (wk[c]) {
            const auto it = id.find(find(c));
            w.ids[c] = it == id.end() ? std::uint16_t{0} : it->second;
        }
    }
    return w;
}

// --- Own R6 -------------------------------------------------------------------------------------

struct HabParts {
    std::int64_t sc = 0, b = 0, wat = 0, slope = 0;
    bool flood = false, pocket = false;
};
inline HabParts own_hab_parts(std::int64_t f, std::int64_t w, std::int64_t g, std::int64_t t, std::int64_t o,
                              std::int64_t s2, std::int64_t hand, std::int64_t walk, const Values& v) {
    HabParts p;
    const std::int64_t sw = v.hab_weight_fertility + v.hab_weight_forage + v.hab_weight_timber + v.hab_weight_ore;
    p.sc = v.hab_weight_fertility * f + v.hab_weight_forage * g + v.hab_weight_timber * t + v.hab_weight_ore * o;
    p.b = from_ratio(p.sc, 255 * sw);
    p.wat = v.hab_water_min + mul_raw(ONE - v.hab_water_min, from_ratio(w, 255));
    p.slope = ONE - own_ramp(own_tangent(s2), v.hab_slope_lo, v.hab_slope_hi);
    p.flood = from_int(hand) < worldgen::mul_int(Fx::from_raw(v.hab_flood_hand_m), 10).raw();
    p.pocket = walk == 0;
    return p;
}
inline std::uint8_t own_habitability(std::int64_t f, std::int64_t w, std::int64_t g, std::int64_t t, std::int64_t o,
                                     std::int64_t s2, std::int64_t hand, std::int64_t walk, const Values& v) {
    const HabParts p = own_hab_parts(f, w, g, t, o, s2, hand, walk, v);
    std::int64_t h = mul_raw(p.b, p.wat);
    h = mul_raw(h, p.slope);
    if (p.flood) {
        h = mul_raw(h, v.hab_flood_factor);
    }
    if (p.pocket) {
        h = mul_raw(h, v.hab_pocket_factor);
    }
    return own_q255(std::min(ONE, h));
}

// --- The test's own stage -----------------------------------------------------------------------

struct Own {
    std::vector<std::int64_t> hand, s2, surface, df, dg, noise;
    resource_api::Layers layers;
    std::int64_t walk_kept = 0, walk_dropped = 0;
};

inline Own own_resources(const resource_api::Inputs& in, const Values& v, std::uint64_t seed) {
    const Grid g = in.grid;
    const std::size_t n = g.cells();
    Own o;
    o.surface = biome_test::surface(in.elevation, in.water_depth);
    auto h = biome_test::own_hand(g, o.surface, in.water, in.river, in.flow_dir, 1);
    REQUIRE_MESSAGE(h.has_value(), "the inputs violate the G7 preconditions");
    o.hand = *h;
    o.s2 = biome_test::own_s2(g, in.elevation);
    o.df = metres_of(own_chamfer(g, fresh_sources(in.water)));
    o.dg = metres_of(own_chamfer(g, fish_sources(in.water, in.river, v.fish_river_class_min)));
    const std::uint64_t key = worldgen::stream_key(seed, STREAM_ORE);
    o.noise.assign(n, 0);
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            o.noise[g.index(x, y)] = own_noise(key, x, y, v.ore_noise);
        }
    }
    resource_api::Layers& l = o.layers;
    for (auto* p : {&l.fertility, &l.ore, &l.timber, &l.fresh_water, &l.forage, &l.habitability}) {
        p->assign(n, 0);
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (in.water[i] == WATER_LAKE) {
            l.fresh_water[i] = 255;
        }
        if (!is_land(in.water[i])) {
            continue;
        }
        const BiomeValues& b = biome_values().at(in.biome[i]);
        l.fresh_water[i] = own_fresh(o.df[i], in.rainfall[i], v);
        l.fertility[i] = own_fertility(b.fertility, o.s2[i], o.hand[i], v);
        l.timber[i] = own_timber(b.timber, o.s2[i], v);
        l.forage[i] = own_forage(b.forage, o.dg[i], v);
        l.ore[i] = own_ore(in.elevation[i], o.s2[i], o.noise[i], v);
    }
    const OwnWalk w = own_walk(g, o.surface, in.water, o.s2, v.walk_slope_max, v.walk_min_cells());
    l.walk_area = w.ids;
    o.walk_kept = w.kept;
    o.walk_dropped = w.dropped;
    for (std::size_t i = 0; i < n; ++i) {
        if (is_land(in.water[i])) {
            l.habitability[i] = own_habitability(l.fertility[i], l.fresh_water[i], l.forage[i], l.timber[i], l.ore[i],
                                                 o.s2[i], o.hand[i], l.walk_area[i], v);
        }
    }
    return o;
}

// --- Inputs ---------------------------------------------------------------------------------------

// The seven input layers from the biome test's eight (P1-04 inputs) and the
// biome layer of the biome stage (shipped definitions and tuning).
inline resource_api::Inputs from_biome_inputs(const biome_api::Inputs& b, std::uint64_t seed, std::uint32_t threads = 4) {
    resource_api::Inputs in;
    in.grid = b.grid;
    in.elevation = b.elevation;
    in.water = b.water;
    in.water_depth = b.water_depth;
    in.flow_dir = b.flow_dir;
    in.river = b.river;
    in.rainfall = b.rainfall;
    in.biome = biome_api::layer(b, biome_test::shipped_definitions(), biome_test::tuning(), seed, threads);
    return in;
}

// A world's seven inputs (elevation fixture, hydrology, climate, biomes with
// the shipped data). Cached per (seed, N, fixture) for N <= 128.
inline resource_api::Inputs world_inputs(std::uint64_t seed, std::int32_t n, bool small) {
    static std::map<std::tuple<std::uint64_t, std::int32_t, bool>, resource_api::Inputs> cache;
    const auto key = std::make_tuple(seed, n, small);
    const auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }
    resource_api::Inputs in = from_biome_inputs(biome_test::world_inputs(seed, n, small), seed);
    if (n <= 128) {
        cache.emplace(key, in);
    }
    return in;
}

// --- Hashes and report values -------------------------------------------------------------------

using elev_test::hash_i64;
inline std::string u8_hash(Grid g, std::span<const std::uint8_t> v) { return hydro_test::layer_hash<std::uint8_t>(g, 1, v); }
inline std::string u16_hash(Grid g, std::span<const std::uint16_t> v) {
    return hydro_test::layer_hash<std::uint16_t>(g, 3, v);
}

using climate_test::ratio4;

// Percentile p of P1-00 10.3: kth(values, min(n - 1, floor(p n / 100))); n/a over none.
inline std::string percentile(std::vector<std::int64_t> v, std::int64_t p) {
    if (v.empty()) {
        return "n/a";
    }
    std::sort(v.begin(), v.end());
    const auto n = static_cast<std::int64_t>(v.size());
    return std::to_string(v[static_cast<std::size_t>(std::min(n - 1, (p * n) / 100))]);
}

inline constexpr std::array<std::string_view, 6> SCORE_NAMES = {"fertility", "ore",    "timber",
                                                                "fresh_water", "forage", "habitability"};

inline const std::vector<std::uint8_t>& score_of(const resource_api::Layers& l, std::size_t k) {
    switch (k) {
        case 0: return l.fertility;
        case 1: return l.ore;
        case 2: return l.timber;
        case 3: return l.fresh_water;
        case 4: return l.forage;
        default: return l.habitability;
    }
}

// The eight report keys of P1-05 9, in order, recomputed from the layers.
inline std::vector<std::pair<std::string, std::string>> report_values(std::span<const std::int16_t> elevation,
                                                                       std::span<const std::uint8_t> water,
                                                                       const resource_api::Layers& l) {
    std::vector<std::size_t> dry;
    for (std::size_t i = 0; i < water.size(); ++i) {
        if (is_land(water[i])) {
            dry.push_back(i);
        }
    }
    const auto land = static_cast<std::uint64_t>(
        std::count_if(elevation.begin(), elevation.end(), [](std::int16_t e) { return e > 0; }));
    std::vector<std::pair<std::string, std::string>> out;
    for (const std::int64_t p : {50, 90}) {
        std::string s;
        for (std::size_t k = 0; k < SCORE_NAMES.size(); ++k) {
            std::vector<std::int64_t> v;
            for (const std::size_t i : dry) {
                v.push_back(score_of(l, k)[i]);
            }
            s += (s.empty() ? "" : " ") + std::string(SCORE_NAMES[k]) + ":" + percentile(v, p);
        }
        out.emplace_back("scores_p" + std::to_string(p), s);
    }
    auto count = [&](const std::vector<std::uint8_t>& v, std::uint8_t at_least) {
        return static_cast<std::uint64_t>(
            std::count_if(dry.begin(), dry.end(), [&](std::size_t i) { return v[i] >= at_least; }));
    };
    out.emplace_back("fertile_share", ratio4(count(l.fertility, 166), dry.size()));
    out.emplace_back("habitable_share", ratio4(count(l.habitability, 64), dry.size()));
    out.emplace_back("ore_rich_share", ratio4(count(l.ore, 128), dry.size()));
    std::map<std::uint16_t, std::uint64_t> areas;
    std::uint16_t largest = 0;
    std::uint64_t walk = 0;
    for (const std::uint16_t v : l.walk_area) {
        largest = std::max(largest, v);
        if (v != 0) {
            ++areas[v];
            ++walk;
        }
    }
    out.emplace_back("walk_areas", std::to_string(largest));
    std::uint64_t main = 0;
    for (const auto& kv : areas) {
        main = std::max(main, kv.second);
    }
    out.emplace_back("walk_main_share", areas.empty() ? std::string("0.0000") : ratio4(main, land));
    out.emplace_back("walk_share", ratio4(walk, land));
    return out;
}

// The score palette of P1-05 9 by the interpolation rule of P1-00 10.2.
struct Rgb {
    std::int64_t r = 0, g = 0, b = 0;
    friend bool operator==(const Rgb&, const Rgb&) = default;
};
inline Rgb score_colour(std::int64_t t) {
    static const std::array<std::pair<std::int64_t, Rgb>, 5> stops = {{{0, {68, 1, 84}},
                                                                        {64, {59, 82, 139}},
                                                                        {128, {33, 145, 140}},
                                                                        {192, {94, 201, 98}},
                                                                        {255, {253, 231, 37}}}};
    if (t < stops.front().first) {
        return stops.front().second;
    }
    if (t >= stops.back().first) {
        return stops.back().second;
    }
    for (std::size_t k = 0; k + 1 < stops.size(); ++k) {
        const auto [t0, c0] = stops[k];
        const auto [t1, c1] = stops[k + 1];
        if (t0 <= t && t < t1) {
            auto ch = [&](std::int64_t a, std::int64_t b) {
                const std::int64_t num = 2 * (b - a) * (t - t0) + (t1 - t0);
                const std::int64_t den = 2 * (t1 - t0);
                std::int64_t q = num / den;
                if ((num % den != 0) && (num < 0)) {
                    --q;  // floor division
                }
                return a + q;
            };
            return Rgb{ch(c0.r, c1.r), ch(c0.g, c1.g), ch(c0.b, c1.b)};
        }
    }
    return Rgb{};
}

// --- Golden file (worldgen/tests/golden/p1-05-resources.txt) --------------------------------------

struct GoldenCase {
    std::string name;
    std::string base;     // the case of p1-04-biomes.txt whose inputs are used
    std::string fixture;  // shipped | small | designed | terrace
    std::int32_t n = 0;
    std::uint64_t seed = 0;
    Overrides overrides;
    std::vector<std::pair<std::string, std::string>> items;

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
            gc.base = c;
            std::int64_t n = 0;
            std::string ov;
            ss >> gc.fixture >> n >> gc.seed >> ov;
            REQUIRE_MESSAGE(!ss.fail(), "bad case line: " << line);
            gc.n = static_cast<std::int32_t>(n);
            if (ov != "-") {
                std::stringstream parts(ov);
                std::string part;
                while (std::getline(parts, part, ',')) {
                    const std::size_t eq = part.find('=');
                    REQUIRE(eq != std::string::npos);
                    gc.overrides.emplace_back(part.substr(0, eq), part.substr(eq + 1));
                }
            }
            out.push_back(gc);
        } else {
            REQUIRE_MESSAGE((!out.empty() && out.back().name == a), "golden line outside its case: " << line);
            out.back().items.emplace_back(b, c);
        }
    }
    return out;
}

inline const std::vector<GoldenCase>& golden() {
    static const std::vector<GoldenCase> g = read_golden(UNMAPPED_P1_05_GOLDEN_PATH);
    return g;
}

// The inputs of a golden case: the earlier stages on the fixture, then the
// biome stage (both checked against the golden input hashes by the caller).
inline resource_api::Inputs case_inputs(const GoldenCase& c) {
    if (c.fixture == "designed" || c.fixture == "terrace") {
        return from_biome_inputs(biome_test::designed_inputs(c.seed, c.fixture == "terrace"), c.seed, 1);
    }
    REQUIRE((c.fixture == "shipped" || c.fixture == "small"));
    if (c.n <= 128) {
        return world_inputs(c.seed, c.n, c.fixture == "small");
    }
    return from_biome_inputs(biome_test::world_inputs(c.seed, c.n, c.fixture == "small"), c.seed);
}

}  // namespace resource_test
