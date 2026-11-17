#pragma once

// The test's own B2 to B5, inputs, golden-file reader and report values for
// the P1-04 biome tests. Written from docs/specs/p1-04-biomes.md and P1-00
// G2, G7, G8 only; independent of the stage (every stage call is in
// p1_04_biomes_api.hpp). Mirrors worldgen/tests/reference/p1_biomes_reference.py.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
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
#include "p1_03_climate_support.hpp"
#include "p1_04_biomes_api.hpp"
#include "p1_04_biomes_data.hpp"
#include "worldgen/core/decimal.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/core/wide.hpp"
#include "worldgen/geo/grid.hpp"
#include "worldgen/geo/noise.hpp"

namespace biome_test {

using worldgen::Fx;
using worldgen::geo::Grid;
using Overrides = std::vector<std::pair<std::string, std::string>>;

inline constexpr std::int64_t ONE = std::int64_t{1} << 32;
inline constexpr std::int64_t HALF = std::int64_t{1} << 31;
inline constexpr std::string_view PRECONDITION = "biomes: input precondition: ";
inline constexpr std::string_view G7_PREFIX = "G7: ";
inline constexpr std::string_view STREAM_TJ = "geo.biomes.temp_jitter";
inline constexpr std::string_view STREAM_RJ = "geo.biomes.rain_jitter";
inline constexpr std::string_view STREAM_TEST = "test.biomes";
inline constexpr std::uint32_t THREADS = 1;

inline constexpr std::uint8_t WATER_DRY = 0;
inline constexpr std::uint8_t WATER_RIVER = 1;
inline constexpr std::uint8_t WATER_LAKE = 2;
inline constexpr std::uint8_t WATER_OCEAN = 3;

inline constexpr std::array<std::int32_t, 8> DX = {1, 1, 0, -1, -1, -1, 0, 1};
inline constexpr std::array<std::int32_t, 8> DY = {0, 1, 1, 1, 0, -1, -1, -1};

// Codes of the shipped definitions (P1-00 6.6).
inline constexpr std::uint8_t OCEAN = 1, LAKE = 2, FOREST = 3, GRASSLAND = 4, MARSH = 5, MOOR = 6, ALPINE = 7,
                              BLIGHTED = 8;

// --- Own definitions (P1-04 4.1), raw values ------------------------------------------

struct Rule {
    std::int64_t temp_min = 0, temp_max = 0;  // raw Fx
    std::int64_t rain_min = 0, rain_max = 0;  // mm
    std::int64_t elev_min = 0, elev_max = 0;  // m
    std::int64_t slope_max = 0;               // raw Fx
    std::int64_t hand_max = 0;                // raw Fx (metres)
    std::int64_t coast_max = 0;               // m
};

struct Def {
    std::string key;
    std::uint8_t code = 0;
    std::string name;
    std::int64_t colour = 0;
    bool water = false;
    std::string water_body;
    std::int64_t priority = 0;
    bool fallback = false;
    std::vector<Rule> rules;
};

inline std::int64_t fx_int(std::int64_t v) { return v * ONE; }

inline Rule full_rule() { return Rule{fx_int(-60), fx_int(50), 0, 10000, 0, 3000, fx_int(100), fx_int(3000), 32767}; }

inline const std::vector<Def>& shipped_defs() {
    static const std::vector<Def> d = [] {
        std::vector<Def> v;
        Rule forest = full_rule();
        forest.rain_min = 720;
        Rule marsh = full_rule();
        marsh.rain_min = 500;
        marsh.elev_max = 200;
        marsh.slope_max = 214748365;  // "0.05"
        marsh.hand_max = fx_int(1);
        Rule moor = full_rule();
        moor.temp_max = fx_int(4);
        moor.elev_min = 300;
        Rule alpine = full_rule();
        alpine.temp_max = HALF;  // "0.5"
        alpine.elev_min = 600;
        v.push_back(Def{"ocean", OCEAN, "Ocean", 0x183878, true, "ocean", 0, false, {}});
        v.push_back(Def{"lake", LAKE, "Lake", 0x4080CC, true, "lake", 0, false, {}});
        v.push_back(Def{"temperate_forest", FOREST, "Temperate forest", 0x2E6034, false, "none", 20, false, {forest}});
        v.push_back(Def{"grassland", GRASSLAND, "Grassland", 0x96AA5A, false, "none", 10, true, {}});
        v.push_back(Def{"marsh", MARSH, "Marsh", 0x5C6E54, false, "none", 40, false, {marsh}});
        v.push_back(Def{"highland_moor", MOOR, "Highland moor", 0x786468, false, "none", 30, false, {moor}});
        v.push_back(Def{"alpine", ALPINE, "Alpine", 0xBEBEB4, false, "none", 50, false, {alpine}});
        v.push_back(Def{"blighted_waste", BLIGHTED, "Blighted waste", 0x605446, false, "none", 0, false, {}});
        return v;
    }();
    return d;
}

// --- Own tuning (P1-04 4.2) ----------------------------------------------------------------

struct Values {
    worldgen::geo::FbmM jitter{500, 4, Fx::from_raw(HALF)};
    std::int64_t temp_jitter = ONE;           // raw
    std::int64_t rain_jitter = 858993459;     // "0.2"
    std::int64_t scale_512 = 4831838208;      // "1.125"
    std::int64_t scale_1024 = ONE;
    std::int64_t scale_2048 = 3758096384;     // "0.875"
    std::int64_t k = 3;
};

// An fx value written as decimal text without quotes, as raw.
inline std::int64_t fx_of(std::string_view text) {
    std::string t(text);
    t.erase(std::remove(t.begin(), t.end(), '"'), t.end());
    const worldgen::DecimalParse r = worldgen::parse_decimal(t);
    REQUIRE_MESSAGE(r.ok(), "bad decimal " << t);
    return r.value.raw();
}

inline Values values(const Overrides& o = {}) {
    Values v;
    for (const auto& [k, text] : o) {
        if (k == "temp_jitter_c") {
            v.temp_jitter = fx_of(text);
        } else if (k == "rain_jitter") {
            v.rain_jitter = fx_of(text);
        } else if (k == "rain_scale_512") {
            v.scale_512 = fx_of(text);
        } else if (k == "rain_scale_1024") {
            v.scale_1024 = fx_of(text);
        } else if (k == "rain_scale_2048") {
            v.scale_2048 = fx_of(text);
        } else if (k == "drain_river_class_min") {
            v.k = std::stoll(text);
        } else {
            FAIL("unknown tuning field " << k);
        }
    }
    return v;
}

// The loaded stage objects for shipped definitions and tuning with overrides
// (fx values in overrides are quoted for the TOML text).
inline Overrides toml_overrides(const Overrides& o) {
    Overrides out;
    for (const auto& [k, v] : o) {
        out.emplace_back(k, k == "drain_river_class_min" ? v : "\"" + v + "\"");
    }
    return out;
}

inline std::string all_errors(const biome_api::Load& l) {
    std::string s;
    for (const auto& e : l.errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

inline const worldgen::data::ContentSet& content(const Overrides& o = {}) {
    static std::map<std::string, worldgen::data::ContentSet> cache;
    const std::string text = biome_data::tuning_toml(toml_overrides(o));
    const auto it = cache.find(text);
    if (it != cache.end()) {
        return it->second;
    }
    const biome_api::Load l = biome_api::load(text, biome_data::directory());
    REQUIRE_MESSAGE(l.ok(), all_errors(l));
    return cache.emplace(text, *l.content).first->second;
}

inline const biome_api::Definitions& shipped_definitions() {
    static const biome_api::Definitions d = biome_api::definitions_from(content());
    return d;
}

inline biome_api::Tuning tuning(const Overrides& o = {}) { return biome_api::tuning_from(content(o)); }

// --- Own G7 (P1-00 7.7) and G8 (7.8) ---------------------------------------------------------

inline bool is_drain(std::uint8_t water, std::uint8_t river, std::int64_t k) {
    return water == WATER_LAKE || water == WATER_OCEAN || (water == WATER_RIVER && river >= k);
}

inline std::vector<std::int64_t> surface(std::span<const std::int16_t> e, std::span<const std::uint16_t> depth) {
    std::vector<std::int64_t> s(e.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        s[i] = std::int64_t{e[i]} + std::int64_t{depth[i]};
    }
    return s;
}

// The cell flow_dir points to, or -1 (code above 7 or off the map).
inline std::int64_t down_of(Grid g, std::size_t i, std::uint8_t code) {
    if (code > 7) {
        return -1;
    }
    const std::int32_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(i)) + DX[code];
    const std::int32_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(i)) + DY[code];
    if (x < 0 || y < 0 || x >= g.width || y >= g.height) {
        return -1;
    }
    return static_cast<std::int64_t>(g.index(x, y));
}

// G7 by following flow_dir from every cell (memoised), not the spec's FIFO:
// the test's second method. Returns std::nullopt where the spec says fatal.
inline std::optional<std::vector<std::int64_t>> own_hand(Grid g, std::span<const std::int64_t> s,
                                                         std::span<const std::uint8_t> water,
                                                         std::span<const std::uint8_t> river,
                                                         std::span<const std::uint8_t> flow_dir, std::int64_t k) {
    const std::size_t n = g.cells();
    std::vector<std::int64_t> drain(n, -1);
    std::vector<std::size_t> path;
    for (std::size_t start = 0; start < n; ++start) {
        path.clear();
        std::size_t c = start;
        while (drain[c] < 0 && !is_drain(water[c], river[c], k)) {
            path.push_back(c);
            if (path.size() > n) {
                return std::nullopt;  // a cycle
            }
            const std::int64_t d = down_of(g, c, flow_dir[c]);
            if (d < 0) {
                return std::nullopt;  // leaves the map or no downstream
            }
            c = static_cast<std::size_t>(d);
        }
        const std::int64_t target = drain[c] >= 0 ? drain[c] : static_cast<std::int64_t>(c);
        drain[c] = target;
        for (const std::size_t p : path) {
            drain[p] = target;
        }
    }
    std::vector<std::int64_t> out(n);
    for (std::size_t i = 0; i < n; ++i) {
        out[i] = s[i] - s[static_cast<std::size_t>(drain[i])];
        if (out[i] < 0) {
            return std::nullopt;
        }
    }
    return out;
}

// S2 of P1-00 10.3 "Cell slope" (e' = max(e, 0), one-sided differences
// doubled at the grid edge).
inline std::vector<std::int64_t> own_s2(Grid g, std::span<const std::int16_t> e) {
    std::vector<std::int64_t> out(g.cells());
    auto ep = [&](std::int32_t x, std::int32_t y) { return std::max<std::int64_t>(e[g.index(x, y)], 0); };
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const std::int32_t xa = std::max(x - 1, 0), xb = std::min(x + 1, g.width - 1);
            const std::int32_t ya = std::max(y - 1, 0), yb = std::min(y + 1, g.height - 1);
            const std::int64_t gx = xb > xa ? (ep(xb, y) - ep(xa, y)) * (2 / (xb - xa)) : 0;
            const std::int64_t gy = yb > ya ? (ep(x, yb) - ep(x, ya)) * (2 / (yb - ya)) : 0;
            out[g.index(x, y)] = gx * gx + gy * gy;
        }
    }
    return out;
}

// G8: not steeper than t <=> S2 x 2^64 <= (320 x t.raw)^2, in U128.
inline bool not_steeper(std::int64_t s2, std::int64_t t_raw) {
    const worldgen::U128 lhs{static_cast<std::uint64_t>(s2), 0};
    const std::uint64_t m = 320U * static_cast<std::uint64_t>(t_raw);
    return lhs <= worldgen::mul_u64_wide(m, m);
}

// --- Own B3, B4, B5 --------------------------------------------------------------------------

inline std::int64_t scale_for(const Values& v, std::int64_t n) {
    return n <= 512 ? v.scale_512 : (n <= 1024 ? v.scale_1024 : v.scale_2048);
}

inline std::int64_t rb(std::int64_t sc, std::int64_t b) {
    return worldgen::mul_int(Fx::from_raw(sc), b).raw();  // non-sentinel bounds only
}

struct Jittered {
    std::int64_t tq = 0, rq = 0;  // raw
};

inline Jittered own_jitter(std::int64_t temperature, std::int64_t rainfall, std::int64_t nt, std::int64_t nr,
                           const Values& v) {
    using worldgen::mul;
    const Fx tq = Fx::from_ratio(temperature, 10) + mul(Fx::from_raw(v.temp_jitter), Fx::from_raw(nt));
    const Fx f = Fx::from_int(1) + mul(Fx::from_raw(v.rain_jitter), Fx::from_raw(nr));
    const Fx rq = mul(Fx::from_int(rainfall), std::max(Fx::from_raw(0), f));
    return Jittered{tq.raw(), rq.raw()};
}

enum Condition : std::uint8_t { C_TEMP, C_RAIN, C_ELEV, C_SLOPE, C_HAND, C_COAST, C_COUNT };
inline constexpr std::array<std::string_view, C_COUNT> CONDITION_NAMES = {"temp", "rain", "elev",
                                                                           "slope", "hand", "coast"};

struct Cell {
    std::int64_t tq = 0, rq = 0, elevation = 0, s2 = 0, hand = 0, coast = 0;
};

// The conditions of B4 that rule r fails, as a bit set.
inline std::uint32_t failures(const Rule& r, std::int64_t sc, const Cell& c) {
    // Unbounded sentinels (4.1, amended 2026-11-17): a bound at the extreme
    // of its field's type skips that comparison.
    std::uint32_t f = 0;
    if ((r.temp_min != fx_int(-60) && c.tq < r.temp_min) || (r.temp_max != fx_int(50) && c.tq > r.temp_max)) {
        f |= 1U << C_TEMP;
    }
    if ((r.rain_min != 0 && c.rq < rb(sc, r.rain_min)) || (r.rain_max != 10000 && c.rq > rb(sc, r.rain_max))) {
        f |= 1U << C_RAIN;
    }
    if ((r.elev_min != 0 && c.elevation < 10 * r.elev_min) || (r.elev_max != 3000 && c.elevation > 10 * r.elev_max)) {
        f |= 1U << C_ELEV;
    }
    if (r.slope_max != fx_int(100) && !not_steeper(c.s2, r.slope_max)) {
        f |= 1U << C_SLOPE;
    }
    if (r.hand_max != fx_int(3000) && fx_int(c.hand) > r.hand_max * 10) {
        f |= 1U << C_HAND;
    }
    if (r.coast_max != 32767 && c.coast > r.coast_max) {
        f |= 1U << C_COAST;
    }
    return f;
}

// Land definitions with priority > 0, highest first.
inline std::vector<const Def*> ordered(const std::vector<Def>& defs) {
    std::vector<const Def*> out;
    for (const Def& d : defs) {
        if (d.water_body == "none" && d.priority > 0) {
            out.push_back(&d);
        }
    }
    std::sort(out.begin(), out.end(), [](const Def* a, const Def* b) { return a->priority > b->priority; });
    return out;
}

inline std::uint8_t fallback_of(const std::vector<Def>& defs) {
    for (const Def& d : defs) {
        if (d.fallback) {
            return d.code;
        }
    }
    FAIL("no fallback");
    return 0;
}

// Sole-failure counts keyed "<key>.<rule>.<condition>" (golden item sole_fail).
using FailCounts = std::map<std::string, std::int64_t>;

// B4 for one cell in priority order; optionally records sole failures.
inline std::uint8_t own_lookup(const std::vector<Def>& defs, std::int64_t sc, const Cell& c, FailCounts* fails = nullptr) {
    for (const Def* d : ordered(defs)) {
        for (std::size_t ri = 0; ri < d->rules.size(); ++ri) {
            const std::uint32_t f = failures(d->rules[ri], sc, c);
            if (f == 0) {
                return d->code;
            }
            if (fails != nullptr && (f & (f - 1U)) == 0U) {
                for (std::uint8_t k = 0; k < C_COUNT; ++k) {
                    if (f == (1U << k)) {
                        (*fails)[d->key + "." + std::to_string(ri) + "." + std::string(CONDITION_NAMES[k])] += 1;
                    }
                }
            }
        }
    }
    return fallback_of(defs);
}

// Invariant 4 written differently: every rule of every definition is
// evaluated and the matching definition of highest priority kept.
inline std::uint8_t own_lookup_exhaustive(const std::vector<Def>& defs, std::int64_t sc, const Cell& c) {
    std::int64_t best = -1;
    std::uint8_t code = fallback_of(defs);
    for (const Def& d : defs) {
        if (d.water_body != "none" || d.priority == 0) {
            continue;
        }
        for (const Rule& r : d.rules) {
            if (failures(r, sc, c) == 0 && d.priority > best) {
                best = d.priority;
                code = d.code;
            }
        }
    }
    return code;
}

struct B5Stats {
    std::int64_t rivers = 0, changed = 0, ties = 0, nodry = 0;
};

inline std::vector<std::uint8_t> own_river_cells(Grid g, std::span<const std::uint8_t> water,
                                                 std::span<const std::uint8_t> b4, B5Stats* stats = nullptr) {
    std::vector<std::uint8_t> out(b4.begin(), b4.end());
    B5Stats s;
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const std::size_t i = g.index(x, y);
            if (water[i] != WATER_RIVER) {
                continue;
            }
            ++s.rivers;
            std::array<int, 256> count{};
            int total = 0;
            for (std::size_t k = 0; k < 8; ++k) {
                const std::int32_t mx = x + DX[k], my = y + DY[k];
                if (mx >= 0 && my >= 0 && mx < g.width && my < g.height && water[g.index(mx, my)] == WATER_DRY) {
                    ++count[b4[g.index(mx, my)]];
                    ++total;
                }
            }
            if (total == 0) {
                ++s.nodry;
                continue;
            }
            int best = 0, winners = 0;
            std::uint8_t code = 0;
            for (std::size_t v = 0; v < 256; ++v) {
                if (count[v] > best) {
                    best = count[v];
                    code = static_cast<std::uint8_t>(v);
                    winners = 1;
                } else if (count[v] == best && best > 0) {
                    ++winners;
                }
            }
            s.ties += winners > 1 ? 1 : 0;
            out[i] = code;
            s.changed += code != b4[i] ? 1 : 0;
        }
    }
    if (stats != nullptr) {
        *stats = s;
    }
    return out;
}

inline std::vector<std::int64_t> noise_grid(std::uint64_t seed, std::string_view stream, Grid g,
                                            std::span<const std::uint8_t> water, const worldgen::geo::FbmM& rec) {
    const std::uint64_t key = worldgen::stream_key(seed, stream);
    std::vector<std::int64_t> out(g.cells(), 0);
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            if (water[g.index(x, y)] <= WATER_RIVER) {
                out[g.index(x, y)] =
                    worldgen::geo::fbm_m(key, Fx::from_int(16 * x + 8), Fx::from_int(16 * y + 8), rec).raw();
            }
        }
    }
    return out;
}

// Every intermediate of the test's own stage.
struct Own {
    std::int64_t scale = 0;
    std::vector<std::int64_t> hand, s2, nt, nr, tq, rq;
    std::vector<std::uint8_t> b4, biome;
    B5Stats b5;
    FailCounts fails;
    std::map<std::uint8_t, std::int64_t> fired;
};

inline Own own_biomes(const biome_api::Inputs& in, const std::vector<Def>& defs, const Values& v, std::uint64_t seed) {
    const Grid g = in.grid;
    Own o;
    const std::vector<std::int64_t> s = surface(in.elevation, in.water_depth);
    auto h = own_hand(g, s, in.water, in.river, in.flow_dir, v.k);
    REQUIRE_MESSAGE(h.has_value(), "the inputs violate the G7 preconditions");
    o.hand = *h;
    o.s2 = own_s2(g, in.elevation);
    o.scale = scale_for(v, std::max(g.width, g.height));
    o.nt = noise_grid(seed, STREAM_TJ, g, in.water, v.jitter);
    o.nr = noise_grid(seed, STREAM_RJ, g, in.water, v.jitter);
    o.tq.assign(g.cells(), 0);
    o.rq.assign(g.cells(), 0);
    o.b4.assign(g.cells(), 0);
    for (std::size_t i = 0; i < g.cells(); ++i) {
        if (in.water[i] == WATER_OCEAN) {
            o.b4[i] = OCEAN;
            continue;
        }
        if (in.water[i] == WATER_LAKE) {
            o.b4[i] = LAKE;
            continue;
        }
        const Jittered j = own_jitter(in.temperature[i], in.rainfall[i], o.nt[i], o.nr[i], v);
        o.tq[i] = j.tq;
        o.rq[i] = j.rq;
        const Cell c{j.tq, j.rq, in.elevation[i], o.s2[i], o.hand[i], in.coast_dist[i]};
        const bool dry = in.water[i] == WATER_DRY;
        o.b4[i] = own_lookup(defs, o.scale, c, dry ? &o.fails : nullptr);
        if (dry) {
            o.fired[o.b4[i]] += 1;
        }
    }
    o.biome = own_river_cells(g, in.water, o.b4, &o.b5);
    return o;
}

// --- Inputs ------------------------------------------------------------------------------------

// A world's eight input layers: elevation (P1-01 fixture), hydrology with the
// shipped tuning and the seed's flat_tie, climate with the shipped tuning.
// Cached per (seed, N, fixture) for N <= 256 (a pure function).
inline biome_api::Inputs world_inputs(std::uint64_t seed, std::int32_t n, bool small) {
    static std::map<std::tuple<std::uint64_t, std::int32_t, bool>, biome_api::Inputs> cache;
    const auto key = std::make_tuple(seed, n, small);
    const auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }
    biome_api::Inputs in;
    in.grid = Grid{n, n};
    in.elevation = hydro_api::elevation_grid(seed, n, hydro_test::elevation_tuning(small), 4);
    const hydro_api::Layers h = hydro_api::world_layers(seed, in.grid, in.elevation, hydro_test::shipped_tuning(), 4);
    in.water = h.water;
    in.water_depth = h.water_depth;
    in.flow_dir = h.flow_dir;
    in.river = h.river;
    in.coast_dist = h.coast_dist;
    const climate_api::Layers c = climate_api::layers(in.grid, in.elevation, in.water, in.water_depth, in.coast_dist,
                                                      climate_test::shipped_tuning(), seed, 4);
    in.temperature = c.temperature;
    in.rainfall = c.rainfall;
    if (n <= 256) {
        cache.emplace(key, in);
    }
    return in;
}

// Designed fixture f (golden header of p1-04-biomes.txt; reference
// designed_elevation / designed_layers): 40 x 40.
inline constexpr std::int32_t DESIGNED_W = 40;
inline constexpr std::array<std::int64_t, 4> DESIGNED_BASE = {90, 60, 20, 0};
inline constexpr std::int64_t DESIGNED_AREA = 2560;

inline biome_api::Inputs designed_inputs(std::uint64_t f) {
    worldgen::Rng rng(worldgen::substream(worldgen::stream_key(0, STREAM_TEST), f));
    biome_api::Inputs in;
    in.grid = Grid{DESIGNED_W, DESIGNED_W};
    std::vector<std::int64_t> ut, ur;
    for (std::int32_t y = 0; y < DESIGNED_W; ++y) {
        for (std::int32_t x = 0; x < DESIGNED_W; ++x) {
            const std::int64_t a = rng.uniform_i64(0, 60);
            const std::int64_t b = rng.uniform_i64(-15, 15);
            const std::int64_t c = rng.uniform_i64(-40, 40);
            ut.push_back(b);
            ur.push_back(c);
            std::int64_t e = 0;
            if (x == 0 || y == 0 || x == DESIGNED_W - 1 || y == DESIGNED_W - 1) {
                e = -20;
            } else if (x <= 13) {
                e = 3 + x + a % 4;
            } else if (x <= 19) {
                e = 17 + 350 * (x - 13) + a % 41;
            } else if (x <= 27) {
                e = 2105 + a % 5;
            } else {
                e = 2110 + 110 * (x - 27) * (x - 27) + a;
            }
            in.elevation.push_back(static_cast<std::int16_t>(e));
        }
    }
    const hydro_api::Layers h = hydro_api::layers(in.grid, in.elevation, hydro_test::tuning(DESIGNED_AREA), 1);
    in.water = h.water;
    in.water_depth = h.water_depth;
    in.flow_dir = h.flow_dir;
    in.river = h.river;
    in.coast_dist = h.coast_dist;
    const std::int64_t base = DESIGNED_BASE[f % DESIGNED_BASE.size()];
    for (std::size_t i = 0; i < in.grid.cells(); ++i) {
        const std::int64_t e = std::max<std::int64_t>(in.elevation[i], 0);
        // floor division of a non-negative numerator
        in.temperature.push_back(static_cast<std::int16_t>(base - (65 * e) / 10000 + ut[i]));
        in.rainfall.push_back(static_cast<std::uint16_t>(380 + 18 * static_cast<std::int64_t>(i / DESIGNED_W) + ur[i]));
    }
    return in;
}

// --- Hashes (P0-04 8) ------------------------------------------------------------------------

using elev_test::hash_i64;

inline std::string u8_hash(Grid g, std::span<const std::uint8_t> v) { return hydro_test::layer_hash<std::uint8_t>(g, 1, v); }

inline void put_bytes(worldgen::Sha256& s, std::string_view b) {
    elev_test::put_le(s, b.size(), 8);
    s.update(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(b.data()), b.size()));
}

// Logical hash of table biome (B6 rows of the definitions).
inline std::string biome_table_hash(const std::vector<Def>& defs) {
    std::vector<const Def*> rows;
    for (const Def& d : defs) {
        rows.push_back(&d);
    }
    std::sort(rows.begin(), rows.end(), [](const Def* a, const Def* b) { return a->code < b->code; });
    worldgen::Sha256 s;
    elev_test::put_le(s, 5, 8);
    auto integer = [&](std::int64_t v) {
        elev_test::put_le(s, 1, 1);
        elev_test::put_le(s, static_cast<std::uint64_t>(v), 8);
    };
    auto text = [&](std::string_view t) {
        elev_test::put_le(s, 3, 1);
        put_bytes(s, t);
    };
    for (const Def* d : rows) {
        integer(d->code);
        text(d->key);
        text(d->name);
        integer(d->colour);
        integer(d->water ? 1 : 0);
    }
    return worldgen::to_hex(s.final());
}

inline std::string climate_table_hash(std::int64_t wind) {
    worldgen::Sha256 s;
    elev_test::put_le(s, 2, 8);
    for (const std::int64_t v : {std::int64_t{1}, wind}) {
        elev_test::put_le(s, 1, 1);
        elev_test::put_le(s, static_cast<std::uint64_t>(v), 8);
    }
    return worldgen::to_hex(s.final());
}

// --- Report values (P1-04 9) -----------------------------------------------------------------

using climate_test::ratio4;

inline std::string biome_share(std::span<const std::uint8_t> biome, const std::vector<Def>& defs) {
    std::vector<const Def*> land;
    for (const Def& d : defs) {
        if (!d.water) {
            land.push_back(&d);
        }
    }
    std::sort(land.begin(), land.end(), [](const Def* a, const Def* b) { return a->code < b->code; });
    std::map<std::uint8_t, std::uint64_t> count;
    std::uint64_t den = 0;
    for (const std::uint8_t v : biome) {
        for (const Def* d : land) {
            if (d->code == v) {
                ++count[v];
                ++den;
            }
        }
    }
    std::string s;
    for (const Def* d : land) {
        s += (s.empty() ? "" : " ") + d->key + ":" + ratio4(count[d->code], den);
    }
    return s;
}

// 8-connected components of equal biome among land-biome cells (G2).
inline std::int64_t biome_patches(Grid g, std::span<const std::uint8_t> biome, const std::vector<Def>& defs) {
    std::array<bool, 256> land{};
    for (const Def& d : defs) {
        land[d.code] = !d.water;
    }
    std::vector<bool> seen(g.cells(), false);
    std::int64_t comps = 0;
    for (std::size_t s = 0; s < g.cells(); ++s) {
        if (!land[biome[s]] || seen[s]) {
            continue;
        }
        ++comps;
        std::deque<std::size_t> q{s};
        seen[s] = true;
        while (!q.empty()) {
            const std::size_t c = q.front();
            q.pop_front();
            const std::int32_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(c));
            const std::int32_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(c));
            for (std::size_t k = 0; k < 8; ++k) {
                const std::int32_t mx = x + DX[k], my = y + DY[k];
                if (mx < 0 || my < 0 || mx >= g.width || my >= g.height) {
                    continue;
                }
                const std::size_t m = g.index(mx, my);
                if (!seen[m] && land[biome[m]] && biome[m] == biome[c]) {
                    seen[m] = true;
                    q.push_back(m);
                }
            }
        }
    }
    return comps;
}

inline constexpr std::array<std::string_view, 8> WIND_NAMES = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};

// --- Golden file (worldgen/tests/golden/p1-04-biomes.txt) --------------------------------------

struct GoldenCase {
    std::string name;
    std::string fixture;  // shipped | small | designed
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
    [[nodiscard]] bool has(std::string_view item) const {
        return std::any_of(items.begin(), items.end(), [&](const auto& kv) { return kv.first == item; });
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
            std::string ov;
            ss >> n >> gc.seed >> ov;
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
    static const std::vector<GoldenCase> g = read_golden(UNMAPPED_P1_04_GOLDEN_PATH);
    return g;
}

inline biome_api::Inputs case_inputs(const GoldenCase& c) {
    if (c.fixture == "designed") {
        return designed_inputs(c.seed);
    }
    REQUIRE((c.fixture == "shipped" || c.fixture == "small"));
    return world_inputs(c.seed, c.n, c.fixture == "small");
}

}  // namespace biome_test
