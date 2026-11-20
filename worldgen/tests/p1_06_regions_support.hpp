#pragma once

// The test's own RG2 to RG13, inputs, golden-file reader and report values
// for the P1-06 regions tests (spec docs/specs/p1-06-regions.md), written
// from the spec text with the deterministic core (Fx, fbm2, hash_at) and
// the test's own G7 (p1_04_biomes_support.hpp) only; independent of the
// stage (every stage call is in p1_06_regions_api.hpp). The flood uses a
// std::priority_queue on (cost, seq), the merge works on the region graph,
// the borders and crossings follow the text of RG12.

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <ostream>
#include <queue>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "p1_02_hydrology_support.hpp"
#include "p1_04_biomes_api.hpp"
#include "p1_04_biomes_data.hpp"
#include "p1_04_biomes_support.hpp"
#include "p1_05_resources_api.hpp"
#include "p1_05_resources_support.hpp"
#include "p1_06_regions_api.hpp"
#include "p1_06_regions_data.hpp"
#include "p1_06_regions_names.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/noise.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/geo/grid.hpp"

namespace region_test {

using worldgen::Fx;
using worldgen::geo::Grid;
using Overrides = std::vector<std::pair<std::string, std::string>>;

inline constexpr std::int64_t ONE = std::int64_t{1} << 32;
inline constexpr std::int64_t INF = std::int64_t{1} << 62;
inline constexpr std::int64_t D160 = std::int64_t{6074001000} * 160;  // mul_int(SQRT2, 160)
inline constexpr std::string_view PRECONDITION = "regions: input precondition: ";
inline constexpr std::string_view STREAM_SEED = "geo.regions.seed";
inline constexpr std::string_view STREAM_COST = "geo.regions.cost";
inline constexpr std::uint32_t THREADS = 1;

inline constexpr std::uint8_t WATER_DRY = 0;
inline constexpr std::uint8_t WATER_RIVER = 1;
inline constexpr std::uint8_t WATER_LAKE = 2;
inline constexpr std::uint8_t WATER_OCEAN = 3;

inline constexpr std::array<std::int32_t, 8> DX = {1, 1, 0, -1, -1, -1, 0, 1};
inline constexpr std::array<std::int32_t, 8> DY = {0, 1, 1, 1, 0, -1, -1, -1};

inline bool is_land(std::uint8_t w) { return w != WATER_OCEAN; }
inline bool is_dry(std::uint8_t w) { return w == WATER_DRY || w == WATER_RIVER; }
inline std::size_t uz(std::int64_t v) { return static_cast<std::size_t>(v); }

// RHA(n / d), exact (P0-01 5.2), for test values that fit int64.
inline std::int64_t own_rha(std::int64_t n, std::int64_t d) {
    const bool neg = (n < 0) != (d < 0);
    const std::uint64_t an = n < 0 ? static_cast<std::uint64_t>(-(n + 1)) + 1U : static_cast<std::uint64_t>(n);
    const std::uint64_t ad = d < 0 ? static_cast<std::uint64_t>(-(d + 1)) + 1U : static_cast<std::uint64_t>(d);
    std::uint64_t q = an / ad;
    const std::uint64_t r = an % ad;
    if (2U * r >= ad) {
        ++q;
    }
    const auto sq = static_cast<std::int64_t>(q);
    return neg ? -sq : sq;
}

// --- Tuning values (section 4) -------------------------------------------------------------------

inline constexpr std::array<std::string_view, 11> SHIPPED_WORDS = {
    "Central", "North", "Northeast", "East", "Southeast", "South", "Southwest", "West", "Northwest", "Isle", "Fells"};

struct Values {
    std::int64_t seed_tile_m = 864;
    std::int64_t seed_spacing_m = 1200;
    std::int64_t hand_ref_m = 25;
    worldgen::geo::FbmM cost_noise{1500, 3, Fx::from_raw(ONE / 2)};
    std::int64_t cost_noise_amp = ONE / 2;
    std::int64_t lake_cost_factor = 2 * ONE;
    std::int64_t river_border_class = 6;
    std::int64_t river_cost_m = 2000;
    std::int64_t region_min_area_m2 = 750000;
    std::int64_t ore_rich_min = 128;
    std::int64_t ore_reach_m = 300;
    std::int64_t name_central_frac = 515396076;  // "0.12"
    std::array<std::string, 11> names{};
    std::vector<std::string> upland_own_noun = {"alpine", "heath", "highland_moor"};  // r2

    [[nodiscard]] std::int64_t min_cells() const { return (region_min_area_m2 + 255) / 256; }
};

inline std::int64_t fx_of(std::string_view text) { return biome_test::fx_of(text); }

inline std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, sep)) {
        out.push_back(part);
    }
    return out;
}

// Overrides as in the golden file: an integer, an fx as decimal text,
// cost_noise as wavelength/octaves/gain, names as eleven words joined by
// '/', upland_own_noun as biome keys joined by '/' ("none": empty).
inline Values values(const Overrides& o = {}) {
    Values v;
    for (std::size_t k = 0; k < 11; ++k) {
        v.names[k] = std::string(SHIPPED_WORDS[k]);
    }
    for (const auto& [k, t] : o) {
        if (k == "seed_tile_m") {
            v.seed_tile_m = std::stoll(t);
        } else if (k == "seed_spacing_m") {
            v.seed_spacing_m = std::stoll(t);
        } else if (k == "hand_ref_m") {
            v.hand_ref_m = std::stoll(t);
        } else if (k == "cost_noise") {
            const auto p = split(t, '/');
            REQUIRE(p.size() == 3U);
            v.cost_noise = worldgen::geo::FbmM{std::stoll(p[0]), static_cast<std::int32_t>(std::stoll(p[1])),
                                               Fx::from_raw(fx_of(p[2]))};
        } else if (k == "cost_noise_amp") {
            v.cost_noise_amp = fx_of(t);
        } else if (k == "lake_cost_factor") {
            v.lake_cost_factor = fx_of(t);
        } else if (k == "river_border_class") {
            v.river_border_class = std::stoll(t);
        } else if (k == "river_cost_m") {
            v.river_cost_m = std::stoll(t);
        } else if (k == "region_min_area_m2") {
            v.region_min_area_m2 = std::stoll(t);
        } else if (k == "ore_rich_min") {
            v.ore_rich_min = std::stoll(t);
        } else if (k == "ore_reach_m") {
            v.ore_reach_m = std::stoll(t);
        } else if (k == "name_central_frac") {
            v.name_central_frac = fx_of(t);
        } else if (k == "names") {
            const auto p = split(t, '/');
            REQUIRE(p.size() == 11U);
            for (std::size_t i = 0; i < 11; ++i) {
                v.names[i] = p[i];
            }
        } else if (k == "upland_own_noun") {
            v.upland_own_noun = t == "none" ? std::vector<std::string>{} : split(t, '/');
        } else {
            FAIL("unknown regions field " << k);
        }
    }
    return v;
}

// The overrides as TOML text for region_data::tuning_toml.
inline Overrides toml_overrides(const Overrides& o) {
    Overrides out;
    for (const auto& [k, t] : o) {
        if (k == "cost_noise") {
            const auto p = split(t, '/');
            out.emplace_back(k, "{ wavelength_m = " + p[0] + ", octaves = " + p[1] + ", gain = \"" + p[2] + "\" }");
        } else if (k == "cost_noise_amp" || k == "lake_cost_factor" || k == "name_central_frac") {
            out.emplace_back(k, "\"" + t + "\"");
        } else if (k == "names") {
            const auto p = split(t, '/');
            std::string s = "{ ";
            for (std::size_t i = 0; i < 11; ++i) {
                s += (i ? ", " : "") + std::string(region_data::name_fields()[i]) + " = \"" + p[i] + "\"";
            }
            out.emplace_back(k, s + " }");
        } else if (k == "upland_own_noun") {
            std::string s = "[";
            if (t != "none") {
                const auto p = split(t, '/');
                for (std::size_t i = 0; i < p.size(); ++i) {
                    s += (i ? ", " : "") + std::string("\"") + p[i] + "\"";
                }
            }
            out.emplace_back(k, s + "]");
        } else {
            out.emplace_back(k, t);
        }
    }
    return out;
}

inline std::string all_errors(const region_api::Load& l) {
    std::string s;
    for (const auto& e : l.errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

// The fixture data with worldgen/regions.toml carrying the overrides;
// cached by text.
inline const worldgen::data::ContentSet& content(const Overrides& o = {}) {
    static std::map<std::string, worldgen::data::ContentSet> cache;
    const std::string text = region_data::tuning_toml(toml_overrides(o));
    const auto it = cache.find(text);
    if (it != cache.end()) {
        return it->second;
    }
    const region_api::Load l = region_api::load(text, biome_data::directory());
    REQUIRE_MESSAGE(l.ok(), all_errors(l));
    return cache.emplace(text, *l.content).first->second;
}

inline const region_api::Definitions& definitions() {
    static const region_api::Definitions d = biome_api::definitions_from(content());
    return d;
}

inline region_api::Tuning tuning(const Overrides& o = {}) { return region_api::tuning_from(content(o)); }

// (water flag, region_noun) by biome code, from the loaded definitions (data).
struct Noun {
    bool water = false;
    std::string noun;
    std::string key;
    bool lake = false;  // water_body "lake"
};
inline std::map<std::int64_t, Noun> nouns_of(const region_api::Definitions& d) {
    std::map<std::int64_t, Noun> out;
    for (const auto& b : d.biomes) {
        out[b.code] = Noun{b.water, b.region_noun, b.key, b.water_body == worldgen::stages::WaterBody::lake};
    }
    return out;
}

// --- Own RG3 -------------------------------------------------------------------------------------

inline std::int64_t own_step_weight(std::int64_t nz, std::int64_t hand, const Values& v) {
    const Fx nf = Fx::from_raw(ONE) + worldgen::mul(Fx::from_raw(v.cost_noise_amp), Fx::from_raw(nz));
    const Fx hf = Fx::from_raw(ONE) + Fx::from_ratio(hand, 10 * v.hand_ref_m);
    return worldgen::mul(nf, hf).raw();
}

inline std::int64_t own_cost_noise(std::uint64_t key, std::int32_t x, std::int32_t y, const Values& v) {
    return resource_test::own_noise(key, x, y, v.cost_noise);
}

// --- Own RG4, RG5, RG6 ---------------------------------------------------------------------------

// The site order: true if cell a is better than cell b.
struct SiteKey {
    std::int64_t hab = 0, s = 0;
    std::uint64_t h = 0;
    std::int64_t index = 0;
};
inline bool better(const SiteKey& a, const SiteKey& b) {
    if (a.hab != b.hab) {
        return a.hab > b.hab;
    }
    if (a.s != b.s) {
        return a.s < b.s;
    }
    if (a.h != b.h) {
        return a.h > b.h;
    }
    return a.index < b.index;
}
inline SiteKey site_key(Grid g, std::int64_t i, std::span<const std::uint8_t> hab, std::span<const std::int64_t> s,
                        std::uint64_t key) {
    const auto c = static_cast<worldgen::geo::CellIndex>(i);
    return SiteKey{hab[uz(i)], s[uz(i)], worldgen::hash_at(key, g.x_of(c), g.y_of(c)), i};
}

inline std::vector<std::int64_t> own_candidates(Grid g, std::span<const std::uint8_t> water,
                                                std::span<const std::uint8_t> hab, std::span<const std::int64_t> s,
                                                std::uint64_t key, const Values& v) {
    const std::int32_t t = static_cast<std::int32_t>(v.seed_tile_m / 16);
    std::vector<std::int64_t> out;
    for (std::int32_t ty = 0; ty * t < g.height; ++ty) {
        for (std::int32_t tx = 0; tx * t < g.width; ++tx) {
            std::optional<SiteKey> best;
            for (std::int32_t y = ty * t; y < std::min(ty * t + t, g.height); ++y) {
                for (std::int32_t x = tx * t; x < std::min(tx * t + t, g.width); ++x) {
                    const auto i = static_cast<std::int64_t>(g.index(x, y));
                    if (water[uz(i)] != WATER_DRY) {
                        continue;
                    }
                    const SiteKey k = site_key(g, i, hab, s, key);
                    if (!best || better(k, *best)) {
                        best = k;
                    }
                }
            }
            if (best) {
                out.push_back(best->index);
            }
        }
    }
    return out;
}

inline std::vector<std::uint8_t> border_river(std::span<const std::uint8_t> water, std::span<const std::uint8_t> river,
                                              std::int64_t cls) {
    std::vector<std::uint8_t> out(water.size());
    for (std::size_t i = 0; i < water.size(); ++i) {
        out[i] = (water[i] == WATER_RIVER && river[i] >= cls) ? 1 : 0;
    }
    return out;
}

inline bool own_crosses(Grid g, std::span<const std::uint8_t> border, std::int64_t sx, std::int64_t sy, std::int64_t cx,
                        std::int64_t cy) {
    const std::int64_t dx = cx - sx, dy = cy - sy;
    const std::int64_t m = std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
    std::int64_t px = sx, py = sy;
    auto at = [&](std::int64_t x, std::int64_t y) {
        return border[g.index(static_cast<std::int32_t>(x), static_cast<std::int32_t>(y))] != 0;
    };
    for (std::int64_t j = 1; j <= m; ++j) {
        const std::int64_t qx = sx + own_rha(dx * j, m), qy = sy + own_rha(dy * j, m);
        if (at(qx, qy)) {
            return true;
        }
        if (qx != px && qy != py && at(qx, py) && at(px, qy)) {
            return true;
        }
        px = qx;
        py = qy;
    }
    return false;
}

struct SeedStats {
    std::int64_t rejected = 0, bank = 0;
};

// RG5 on candidates sorted best first.
inline std::vector<std::int64_t> own_select(Grid g, std::span<const std::int64_t> sorted,
                                            std::span<const std::uint8_t> border, const Values& v,
                                            SeedStats* st = nullptr) {
    const std::int64_t sp2 = v.seed_spacing_m * v.seed_spacing_m;
    std::vector<std::int64_t> acc;
    for (const std::int64_t c : sorted) {
        const auto cc = static_cast<worldgen::geo::CellIndex>(c);
        const std::int64_t cx = g.x_of(cc), cy = g.y_of(cc);
        bool ok = true;
        for (const std::int64_t s : acc) {
            const auto sc = static_cast<worldgen::geo::CellIndex>(s);
            const std::int64_t sx = g.x_of(sc), sy = g.y_of(sc);
            const std::int64_t ddx = 16 * (cx - sx), ddy = 16 * (cy - sy);
            if (ddx * ddx + ddy * ddy < sp2) {
                if (!own_crosses(g, border, sx, sy, cx, cy)) {
                    ok = false;
                    break;
                }
                if (st != nullptr) {
                    ++st->bank;
                }
            }
        }
        if (ok) {
            acc.push_back(c);
        } else if (st != nullptr) {
            ++st->rejected;
        }
    }
    return acc;
}

// G2 labels of the 8-connected land components (FIFO BFS, index order).
inline std::vector<std::int64_t> own_landmass(Grid g, std::span<const std::uint8_t> water, std::int64_t& count) {
    const std::size_t n = g.cells();
    std::vector<std::int64_t> label(n, 0);
    count = 0;
    for (std::size_t s = 0; s < n; ++s) {
        if (!is_land(water[s]) || label[s] != 0) {
            continue;
        }
        label[s] = ++count;
        std::deque<std::size_t> q{s};
        while (!q.empty()) {
            const std::size_t c = q.front();
            q.pop_front();
            const auto ci = static_cast<worldgen::geo::CellIndex>(c);
            for (std::size_t d = 0; d < 8; ++d) {
                const std::int32_t x = g.x_of(ci) + DX[d], y = g.y_of(ci) + DY[d];
                if (x < 0 || y < 0 || x >= g.width || y >= g.height) {
                    continue;
                }
                const std::size_t m = g.index(x, y);
                if (is_land(water[m]) && label[m] == 0) {
                    label[m] = count;
                    q.push_back(m);
                }
            }
        }
    }
    return label;
}

inline std::vector<std::int64_t> own_landmass_seeds(Grid g, std::span<const std::uint8_t> water,
                                                    std::span<const std::uint8_t> hab, std::span<const std::int64_t> s,
                                                    std::uint64_t key, std::span<const std::int64_t> label,
                                                    std::int64_t nlm, std::span<const std::int64_t> greedy) {
    std::vector<bool> has(uz(nlm) + 1, false);
    for (const std::int64_t c : greedy) {
        has[uz(label[uz(c)])] = true;
    }
    std::vector<std::optional<SiteKey>> best(uz(nlm) + 1);
    std::vector<std::int64_t> first(uz(nlm) + 1, -1);
    for (std::size_t i = 0; i < g.cells(); ++i) {
        const std::int64_t lm = label[i];
        if (lm == 0 || has[uz(lm)]) {
            continue;
        }
        if (first[uz(lm)] < 0) {
            first[uz(lm)] = static_cast<std::int64_t>(i);
        }
        if (water[i] == WATER_DRY) {
            const SiteKey k = site_key(g, static_cast<std::int64_t>(i), hab, s, key);
            if (!best[uz(lm)] || better(k, *best[uz(lm)])) {
                best[uz(lm)] = k;
            }
        }
    }
    std::vector<std::int64_t> out(greedy.begin(), greedy.end());
    for (std::int64_t lm = 1; lm <= nlm; ++lm) {
        if (!has[uz(lm)]) {
            out.push_back(best[uz(lm)] ? best[uz(lm)]->index : first[uz(lm)]);
        }
    }
    return out;
}

// --- Own RG7 -------------------------------------------------------------------------------------

inline std::int64_t own_step_cost(Grid g, std::size_t c, std::size_t m, std::size_t d, std::span<const std::int64_t> wt,
                                  std::span<const std::uint8_t> water, std::span<const std::uint8_t> border,
                                  const Values& v) {
    std::int64_t hz = (d & 1U) != 0U ? worldgen::mul(Fx::from_raw(wt[m]), Fx::from_raw(D160)).round_int()
                                     : Fx::from_raw(wt[m] * 160).round_int();
    if (water[m] == WATER_LAKE) {
        hz = worldgen::mul_int(Fx::from_raw(v.lake_cost_factor), hz).round_int();
    }
    const std::int64_t rv = 10 * v.river_cost_m;
    std::int64_t cost = hz + (border[m] != 0U ? rv : 0);
    if ((d & 1U) != 0U) {
        const auto ci = static_cast<worldgen::geo::CellIndex>(c), mi = static_cast<worldgen::geo::CellIndex>(m);
        if (border[g.index(g.x_of(mi), g.y_of(ci))] != 0U && border[g.index(g.x_of(ci), g.y_of(mi))] != 0U) {
            cost += rv;
        }
    }
    return cost;
}

struct OwnFlood {
    std::vector<std::int64_t> owner, cost;
};

inline OwnFlood own_flood(Grid g, std::span<const std::int64_t> wt, std::span<const std::uint8_t> water,
                          std::span<const std::uint8_t> border, std::span<const std::int64_t> seeds, const Values& v) {
    const std::size_t n = g.cells();
    OwnFlood f{std::vector<std::int64_t>(n, 0), std::vector<std::int64_t>(n, INF)};
    std::vector<bool> done(n, false);
    using Entry = std::tuple<std::int64_t, std::int64_t, std::size_t>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> q;
    std::int64_t seq = 0;
    for (std::size_t k = 0; k < seeds.size(); ++k) {
        const std::size_t s = uz(seeds[k]);
        f.cost[s] = 0;
        f.owner[s] = static_cast<std::int64_t>(k) + 1;
        q.emplace(0, seq++, s);
    }
    while (!q.empty()) {
        const auto [k, sq, c] = q.top();
        q.pop();
        static_cast<void>(sq);
        if (done[c]) {
            continue;
        }
        done[c] = true;
        const auto ci = static_cast<worldgen::geo::CellIndex>(c);
        for (std::size_t d = 0; d < 8; ++d) {
            const std::int32_t x = g.x_of(ci) + DX[d], y = g.y_of(ci) + DY[d];
            if (x < 0 || y < 0 || x >= g.width || y >= g.height) {
                continue;
            }
            const std::size_t m = g.index(x, y);
            if (!is_land(water[m]) || done[m]) {
                continue;
            }
            const std::int64_t nk = k + own_step_cost(g, c, m, d, wt, water, border, v);
            if (nk < f.cost[m]) {
                f.cost[m] = nk;
                f.owner[m] = f.owner[c];
                q.emplace(nk, seq++, m);
            }
        }
    }
    return f;
}

// --- Own RG8, RG9 --------------------------------------------------------------------------------

using PairCounts = std::map<std::pair<std::int64_t, std::int64_t>, std::array<std::int64_t, 2>>;  // n, nr

// Border pairs (E and S of each cell, index order) between different
// non-zero labels.
template <typename Visit>
inline void for_each_border_pair(Grid g, std::span<const std::int64_t> label, Visit&& visit) {
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const std::size_t c = g.index(x, y);
            if (label[c] == 0) {
                continue;
            }
            for (int k = 0; k < 2; ++k) {
                const std::int32_t xx = x + (k == 0 ? 1 : 0), yy = y + (k == 0 ? 0 : 1);
                if (xx >= g.width || yy >= g.height) {
                    continue;
                }
                const std::size_t m = g.index(xx, yy);
                if (label[m] == 0 || label[m] == label[c]) {
                    continue;
                }
                visit(c, m);
            }
        }
    }
}

inline PairCounts own_pair_counts(Grid g, std::span<const std::int64_t> owner, std::span<const std::uint8_t> border) {
    PairCounts out;
    for_each_border_pair(g, owner, [&](std::size_t c, std::size_t m) {
        const auto key = std::minmax(owner[c], owner[m]);
        auto& e = out[{key.first, key.second}];
        ++e[0];
        if (border[c] == 0U && border[m] == 0U) {
            ++e[1];
        }
    });
    return out;
}

// RG8 on the region graph; returns number -> absorbing number.
inline std::map<std::int64_t, std::int64_t> own_merge_graph(std::map<std::int64_t, std::int64_t> cells, PairCounts pairs,
                                                            std::int64_t min_cells, std::int64_t* merges = nullptr) {
    std::map<std::int64_t, std::int64_t> into;
    for (const auto& kv : cells) {
        into[kv.first] = kv.first;
    }
    for (;;) {
        std::optional<std::pair<std::int64_t, std::int64_t>> small;  // (cells, number)
        for (const auto& [r, n] : cells) {
            if (n == 0 || n >= min_cells) {
                continue;
            }
            bool has = false;
            for (const auto& kv : pairs) {
                if (kv.first.first == r || kv.first.second == r) {
                    has = true;
                    break;
                }
            }
            if (has && (!small || std::make_pair(n, r) < *small)) {
                small = std::make_pair(n, r);
            }
        }
        if (!small) {
            break;
        }
        const std::int64_t i = small->second;
        std::optional<std::tuple<std::int64_t, std::int64_t, std::int64_t>> pick;  // (-nr, -n, j)
        for (const auto& [key, e] : pairs) {
            if (key.first != i && key.second != i) {
                continue;
            }
            const std::int64_t j = key.first == i ? key.second : key.first;
            const auto t = std::make_tuple(-e[1], -e[0], j);
            if (!pick || t < *pick) {
                pick = t;
            }
        }
        const std::int64_t j = std::get<2>(*pick);
        if (merges != nullptr) {
            ++*merges;
        }
        cells[j] += cells[i];
        cells[i] = 0;
        PairCounts next;
        for (const auto& [key, e] : pairs) {
            std::int64_t a = key.first == i ? j : key.first;
            std::int64_t b = key.second == i ? j : key.second;
            if (a == b) {
                continue;
            }
            if (a > b) {
                std::swap(a, b);
            }
            auto& t = next[{a, b}];
            t[0] += e[0];
            t[1] += e[1];
        }
        pairs = std::move(next);
        into[i] = j;
    }
    std::map<std::int64_t, std::int64_t> out;
    for (const auto& kv : into) {
        std::int64_t r = kv.first;
        while (into[r] != r) {
            r = into[r];
        }
        out[kv.first] = r;
    }
    return out;
}

inline std::vector<std::int64_t> own_merge(Grid g, std::span<const std::int64_t> owner,
                                           std::span<const std::uint8_t> border, std::int64_t count,
                                           std::int64_t min_cells, std::int64_t* merges = nullptr) {
    std::map<std::int64_t, std::int64_t> cells;
    for (std::int64_t r = 1; r <= count; ++r) {
        cells[r] = 0;
    }
    for (const std::int64_t o : owner) {
        if (o != 0) {
            ++cells[o];
        }
    }
    const auto final_of = own_merge_graph(cells, own_pair_counts(g, owner, border), min_cells, merges);
    std::vector<std::int64_t> out(owner.size(), 0);
    for (std::size_t i = 0; i < owner.size(); ++i) {
        out[i] = owner[i] != 0 ? final_of.at(owner[i]) : 0;
    }
    return out;
}

// --- Own RG11 ------------------------------------------------------------------------------------

inline std::string own_roman(std::int64_t k) {
    static const std::array<std::pair<std::int64_t, std::string_view>, 13> t = {
        {{1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"}, {50, "L"}, {40, "XL"},
         {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}}};
    std::string s;
    for (const auto& [v, r] : t) {
        while (k >= v) {
            s += r;
            k -= v;
        }
    }
    return s;
}

// 0 central, then north, northeast, east, southeast, south, southwest,
// west, northwest (the record order of `region_names`).
inline int own_sector(std::int64_t dx, std::int64_t dy, std::int64_t rc) {
    if (dx * dx + dy * dy < rc * rc) {
        return 0;
    }
    const std::int64_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (29 * ay <= 12 * ax) {
        return dx > 0 ? 3 : 7;
    }
    if (29 * ax <= 12 * ay) {
        return dy > 0 ? 5 : 1;
    }
    if (dy > 0) {
        return dx > 0 ? 4 : 6;
    }
    return dx > 0 ? 2 : 8;
}

inline std::vector<std::string> own_name_list(std::span<const region_api::NameInput> in, std::int64_t cx,
                                              std::int64_t cy, std::int64_t rc, const Values& v,
                                              const std::map<std::int64_t, Noun>& nouns) {
    std::map<std::string, std::int64_t> seen;
    std::vector<std::string> out;
    std::string mere;
    for (const auto& [code, nn] : nouns) {
        if (nn.lake) {
            mere = nn.noun;
        }
    }
    for (const auto& e : in) {
        const int sec = own_sector(e.seed_x - cx, e.seed_y - cy, rc);
        // RG11 3 (r2): a isle, b at least half lake, c upland unless the
        // dominant biome is in upland_own_noun, d the dominant biome.
        const bool own = std::find(v.upland_own_noun.begin(), v.upland_own_noun.end(), nouns.at(e.biome).key) !=
                         v.upland_own_noun.end();
        const std::string noun = e.isle                           ? v.names[9]
                                 : 2 * e.lake_cells >= e.cells    ? mere
                                 : 4 * e.walk_cells < e.cells && !own ? v.names[10]
                                                                  : nouns.at(e.biome).noun;
        const std::string base = v.names[static_cast<std::size_t>(sec)] + " " + noun;
        ++seen[base];
        out.push_back(base);  // r3: the base name, never a numeral (RG11 4)
    }
    return out;
}

// RG11 3 (r3): the land definition whose words a region draws: isle, lake
// (b), upland (c), else the land of the dominant biome (a water biome:
// lake; only on hand-made input).
inline const region_names::PlaceDef* own_land_of(const region_api::NameInput& e, const Values& v,
                                                 const std::map<std::int64_t, Noun>& nouns,
                                                 const region_names::PlaceData& data) {
    const bool own = std::find(v.upland_own_noun.begin(), v.upland_own_noun.end(), nouns.at(e.biome).key) !=
                     v.upland_own_noun.end();
    if (e.isle) {
        return data.special("isle");
    }
    if (2 * e.lake_cells >= e.cells) {
        return data.special("lake");
    }
    if (4 * e.walk_cells < e.cells && !own) {
        return data.special("upland");
    }
    if (nouns.at(e.biome).water) {
        return data.special("lake");
    }
    return data.land_of_biome(nouns.at(e.biome).key);
}

// --- Own RG12 ------------------------------------------------------------------------------------

inline std::vector<std::int64_t> path_cells(std::span<const std::uint8_t> path) {
    std::vector<std::int64_t> out;
    for (std::size_t k = 0; k + 4 <= path.size(); k += 4) {
        out.push_back(static_cast<std::int64_t>(std::uint32_t{path[k]} | (std::uint32_t{path[k + 1]} << 8U) |
                                                (std::uint32_t{path[k + 2]} << 16U) |
                                                (std::uint32_t{path[k + 3]} << 24U)));
    }
    return out;
}

inline std::vector<std::uint8_t> path_bytes(Grid g, const std::vector<std::pair<std::int32_t, std::int32_t>>& cells) {
    std::vector<std::uint8_t> out;
    for (const auto& [x, y] : cells) {
        const auto i = static_cast<std::uint32_t>(g.index(x, y));
        for (std::uint32_t b = 0; b < 4; ++b) {
            out.push_back(static_cast<std::uint8_t>((i >> (8U * b)) & 0xFFU));
        }
    }
    return out;
}

inline std::vector<std::int64_t> own_pass_steps(Grid g, std::span<const std::int64_t> cells,
                                                std::span<const std::uint8_t> water) {
    std::vector<std::int64_t> seq;
    auto add = [&](std::int64_t i) {
        if (seq.empty() || seq.back() != i) {
            seq.push_back(i);
        }
    };
    for (std::size_t j = 0; j < cells.size(); ++j) {
        if (j > 0) {
            const auto p = static_cast<worldgen::geo::CellIndex>(cells[j - 1]);
            const auto c = static_cast<worldgen::geo::CellIndex>(cells[j]);
            if (g.x_of(p) != g.x_of(c) && g.y_of(p) != g.y_of(c)) {
                const auto c1 = static_cast<std::int64_t>(g.index(g.x_of(c), g.y_of(p)));
                const auto c2 = static_cast<std::int64_t>(g.index(g.x_of(p), g.y_of(c)));
                if (is_land(water[uz(c1)])) {
                    add(c1);
                } else if (is_land(water[uz(c2)])) {
                    add(c2);
                }
            }
        }
        add(cells[j]);
    }
    return seq;
}

// RG12's step sequence with, per cell, the step j that produced it (the
// step from c_j to c_j+1, its corner included; -1 for c_0).
inline std::vector<std::pair<std::int64_t, std::int64_t>> own_pass_steps_j(Grid g, std::span<const std::int64_t> cells,
                                                                          std::span<const std::uint8_t> water) {
    std::vector<std::pair<std::int64_t, std::int64_t>> seq;
    auto add = [&](std::int64_t i, std::int64_t j) {
        if (seq.empty() || seq.back().first != i) {
            seq.emplace_back(i, j);
        }
    };
    for (std::size_t j = 0; j < cells.size(); ++j) {
        const auto jj = static_cast<std::int64_t>(j) - 1;
        if (j > 0) {
            const auto p = static_cast<worldgen::geo::CellIndex>(cells[j - 1]);
            const auto c = static_cast<worldgen::geo::CellIndex>(cells[j]);
            if (g.x_of(p) != g.x_of(c) && g.y_of(p) != g.y_of(c)) {
                const auto c1 = static_cast<std::int64_t>(g.index(g.x_of(c), g.y_of(p)));
                const auto c2 = static_cast<std::int64_t>(g.index(g.x_of(p), g.y_of(c)));
                if (is_land(water[uz(c1)])) {
                    add(c1, jj);
                } else if (is_land(water[uz(c2)])) {
                    add(c2, jj);
                }
            }
        }
        add(cells[j], jj);
    }
    return seq;
}

struct Crossing {
    std::int64_t d = 0, j = 0, order = 0;
    std::pair<std::int64_t, std::int64_t> key;
};
struct Attachment {
    std::optional<std::pair<std::int64_t, std::int64_t>> key;
    std::vector<Crossing> found;
};

// RG12 attachment (r2): the crossing with the smallest (d, j), then the
// first in sequence order; q the first position of the col on the path.
inline Attachment own_pass_attachment(Grid g, std::span<const std::int64_t> region, std::span<const std::uint8_t> water,
                                      const region_api::Pass& p) {
    const std::vector<std::int64_t> cells = path_cells(p.path);
    const auto col = static_cast<std::int64_t>(g.index(static_cast<std::int32_t>(p.col_x), static_cast<std::int32_t>(p.col_y)));
    const auto it = std::find(cells.begin(), cells.end(), col);
    REQUIRE_MESSAGE(it != cells.end(), "pass " << p.id << ": col not on the path");
    const std::int64_t q = it - cells.begin();
    const auto seq = own_pass_steps_j(g, cells, water);
    Attachment a;
    for (std::size_t k = 0; k + 1 < seq.size(); ++k) {
        const auto u = static_cast<worldgen::geo::CellIndex>(seq[k].first);
        const auto w = static_cast<worldgen::geo::CellIndex>(seq[k + 1].first);
        const std::int64_t ddx = g.x_of(u) - g.x_of(w), ddy = g.y_of(u) - g.y_of(w);
        if ((ddx < 0 ? -ddx : ddx) + (ddy < 0 ? -ddy : ddy) != 1) {
            continue;
        }
        const std::int64_t ru = region[uz(seq[k].first)], rw = region[uz(seq[k + 1].first)];
        if (ru == 0 || rw == 0 || ru == rw) {
            continue;
        }
        const std::int64_t j = seq[k + 1].second;
        const std::int64_t d = std::min(j > q ? j - q : q - j, j + 1 > q ? j + 1 - q : q - j - 1);
        a.found.push_back(Crossing{d, j, static_cast<std::int64_t>(k), std::minmax(ru, rw)});
    }
    const Crossing* best = nullptr;
    for (const auto& c : a.found) {
        if (best == nullptr || std::tie(c.d, c.j, c.order) < std::tie(best->d, best->j, best->order)) {
            best = &c;
        }
    }
    if (best != nullptr) {
        a.key = best->key;
    }
    return a;
}

inline std::vector<region_api::BorderRow> own_borders(Grid g, std::span<const std::int64_t> region,
                                                      std::span<const std::int64_t> s,
                                                      std::span<const std::uint16_t> walk,
                                                      std::span<const std::uint8_t> water,
                                                      std::span<const std::uint8_t> river,
                                                      std::span<const std::uint8_t> border,
                                                      std::span<const region_api::Pass> passes,
                                                      std::int64_t* crossings = nullptr,
                                                      std::int64_t* crossed_pairs = nullptr) {
    std::map<std::pair<std::int64_t, std::int64_t>, region_api::BorderRow> rows;
    std::map<std::pair<std::int64_t, std::int64_t>, bool> seen_low;
    for_each_border_pair(g, region, [&](std::size_t c, std::size_t m) {
        const auto key = std::minmax(region[c], region[m]);
        auto& r = rows[{key.first, key.second}];
        r.a = key.first;
        r.b = key.second;
        r.length_m += 16;
        const std::int64_t hi = std::max(s[c], s[m]);
        bool& has = seen_low[{key.first, key.second}];
        if (!has || hi < r.low_dm) {
            has = true;
            r.low_dm = hi;
            const std::size_t cell = s[m] > s[c] ? m : c;
            const auto ci = static_cast<worldgen::geo::CellIndex>(cell);
            r.low_x = g.x_of(ci);
            r.low_y = g.y_of(ci);
        }
        if (walk[c] != 0U && walk[c] == walk[m]) {
            ++r.walk_pairs;
        }
        if (border[c] != 0U || border[m] != 0U) {
            ++r.river_pairs;
        }
        for (const std::size_t p : {c, m}) {
            if (water[p] == WATER_RIVER) {
                r.river_class_max = std::max<std::int64_t>(r.river_class_max, river[p]);
            }
        }
    });
    std::map<std::pair<std::int64_t, std::int64_t>, std::set<std::int64_t>> crossed;
    std::vector<const region_api::Pass*> order;
    for (const auto& p : passes) {
        order.push_back(&p);
    }
    std::sort(order.begin(), order.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    for (const auto* p : order) {
        const auto att = own_pass_attachment(g, region, water, *p);
        if (att.key) {
            crossed[*att.key].insert(p->id);
            if (crossings != nullptr) {
                ++*crossings;
            }
        }
        if (crossed_pairs != nullptr) {
            std::set<std::pair<std::int64_t, std::int64_t>> keys;
            for (const auto& c : att.found) {
                keys.insert(c.key);
            }
            *crossed_pairs += static_cast<std::int64_t>(keys.size());
        }
    }
    std::vector<region_api::BorderRow> out;
    for (auto& [key, r] : rows) {
        const auto it = crossed.find(key);
        if (it != crossed.end()) {
            r.passes = static_cast<std::int64_t>(it->second.size());
            r.pass_id = *it->second.begin();
        }
        out.push_back(r);
    }
    return out;
}

// --- Own stage -----------------------------------------------------------------------------------

struct OwnStats {
    std::int64_t greedy = 0, landmass_seeds = 0, rejected = 0, bank = 0, merges = 0, isle = 0, central = 0,
                 numbered = 0, crossings = 0, lake_cells = 0, border_river_cells = 0, corner = 0, mere = 0, fells = 0,
                 crossed = 0;
};

struct Own {
    std::vector<std::int64_t> surface, hand, noise, wt;
    std::vector<std::int64_t> candidates, greedy, seeds;
    std::vector<std::int64_t> landmass;
    std::int64_t landmasses = 0;
    std::vector<std::int64_t> owner0, cost, owner;
    region_api::Output out;
    OwnStats stats;
    std::vector<std::string> name_how;  // r3: per region "K", "F" or the accepted attempt
    std::vector<std::string> bases;     // r3: base name per region (RG11 4)
    std::vector<const region_names::PlaceDef*> lands;     // r3: land definition per region (RG11 3)
    std::vector<region_names::TraitRow> trait_rows;       // r3: trait inputs per region (RG11 5)
};

inline std::int64_t own_metres(std::int64_t d) { return std::min<std::int64_t>(32767, (32 * d + 5) / 10); }

inline Own own_regions(const region_api::Inputs& in, std::span<const region_api::Pass> passes, const Values& v,
                       std::uint64_t seed, const region_api::Definitions& defs = definitions()) {
    const Grid g = in.grid;
    const std::size_t n = g.cells();
    Own o;
    o.surface = biome_test::surface(in.elevation, in.water_depth);
    auto hand = biome_test::own_hand(g, o.surface, in.water, in.river, in.flow_dir, 1);
    REQUIRE_MESSAGE(hand.has_value(), "own G7 failed on the inputs");
    o.hand = std::move(*hand);
    const std::uint64_t kc = worldgen::stream_key(seed, STREAM_COST);
    const std::uint64_t ks = worldgen::stream_key(seed, STREAM_SEED);
    o.noise.assign(n, 0);
    o.wt.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (is_land(in.water[i])) {
            const auto ci = static_cast<worldgen::geo::CellIndex>(i);
            o.noise[i] = own_cost_noise(kc, g.x_of(ci), g.y_of(ci), v);
            o.wt[i] = own_step_weight(o.noise[i], o.hand[i], v);
        }
    }
    o.candidates = own_candidates(g, in.water, in.habitability, o.surface, ks, v);
    std::vector<std::int64_t> sorted = o.candidates;
    std::sort(sorted.begin(), sorted.end(), [&](std::int64_t a, std::int64_t b) {
        return better(site_key(g, a, in.habitability, o.surface, ks), site_key(g, b, in.habitability, o.surface, ks));
    });
    const std::vector<std::uint8_t> border = border_river(in.water, in.river, v.river_border_class);
    SeedStats ss;
    o.greedy = own_select(g, sorted, border, v, &ss);
    o.landmass = own_landmass(g, in.water, o.landmasses);
    o.seeds = own_landmass_seeds(g, in.water, in.habitability, o.surface, ks, o.landmass, o.landmasses, o.greedy);
    OwnFlood f = own_flood(g, o.wt, in.water, border, o.seeds, v);
    o.owner0 = f.owner;
    o.cost = f.cost;
    o.owner = own_merge(g, o.owner0, border, static_cast<std::int64_t>(o.seeds.size()), v.min_cells(), &o.stats.merges);
    // RG9.
    std::map<std::int64_t, std::int64_t> id_of;
    for (std::size_t i = 0; i < n; ++i) {
        if (o.owner[i] != 0 && id_of.find(o.owner[i]) == id_of.end()) {
            const std::int64_t next = static_cast<std::int64_t>(id_of.size()) + 1;
            id_of[o.owner[i]] = next;
        }
    }
    const auto R = static_cast<std::int64_t>(id_of.size());
    std::vector<std::int64_t> region(n, 0);
    o.out.region.assign(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (o.owner[i] != 0) {
            region[i] = id_of.at(o.owner[i]);
            o.out.region[i] = static_cast<std::uint16_t>(region[i]);
        }
    }
    std::vector<std::int64_t> seed_of(uz(R) + 1, -1);
    for (std::size_t k = 0; k < o.seeds.size(); ++k) {
        const auto it = id_of.find(static_cast<std::int64_t>(k) + 1);
        if (it != id_of.end()) {
            seed_of[uz(it->second)] = o.seeds[k];
        }
    }
    // RG10.
    std::vector<std::uint8_t> walk_src(n);
    for (std::size_t i = 0; i < n; ++i) {
        walk_src[i] = in.walk_area[i] > 0 ? 1 : 0;
    }
    std::vector<std::int64_t> dw = resource_test::own_chamfer(g, walk_src);
    for (auto& d : dw) {
        d = own_metres(d);
    }
    const auto nouns = nouns_of(defs);
    struct Acc {
        std::int64_t cells = 0, lake = 0, riv = 0, rcmax = 0, coast = 0, walk = 0, xmin = 0, ymin = 0, xmax = -1,
                     ymax = -1, dry = 0, esum = 0, emax = 0, rich = 0, reach = 0;
        std::array<std::int64_t, 6> sums{};
        std::vector<std::int64_t> hab;
        std::map<std::int64_t, std::int64_t> wa, biome;
    };
    std::vector<Acc> acc(uz(R) + 1);
    for (auto& a : acc) {
        a.xmin = g.width;
        a.ymin = g.height;
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (region[i] == 0) {
            continue;
        }
        Acc& a = acc[uz(region[i])];
        const auto ci = static_cast<worldgen::geo::CellIndex>(i);
        const std::int64_t x = g.x_of(ci), y = g.y_of(ci);
        ++a.cells;
        a.xmin = std::min(a.xmin, x);
        a.ymin = std::min(a.ymin, y);
        a.xmax = std::max(a.xmax, x);
        a.ymax = std::max(a.ymax, y);
        const std::uint8_t w = in.water[i];
        if (w == WATER_LAKE) {
            ++a.lake;
        }
        if (w == WATER_RIVER) {
            ++a.riv;
            a.rcmax = std::max<std::int64_t>(a.rcmax, in.river[i]);
        }
        bool coast = false;
        for (const std::size_t d : {std::size_t{0}, std::size_t{2}, std::size_t{4}, std::size_t{6}}) {
            const std::int64_t xx = x + DX[d], yy = y + DY[d];
            if (xx >= 0 && yy >= 0 && xx < g.width && yy < g.height &&
                in.water[g.index(static_cast<std::int32_t>(xx), static_cast<std::int32_t>(yy))] == WATER_OCEAN) {
                coast = true;
            }
        }
        a.coast += coast ? 1 : 0;
        if (in.walk_area[i] > 0) {
            ++a.walk;
            ++a.wa[in.walk_area[i]];
        }
        ++a.biome[in.biome[i]];
        if (is_dry(w)) {
            ++a.dry;
            a.esum += in.elevation[i];
            a.emax = a.dry == 1 ? in.elevation[i] : std::max<std::int64_t>(a.emax, in.elevation[i]);
            a.sums[0] += in.fertility[i];
            a.sums[1] += in.forage[i];
            a.sums[2] += in.timber[i];
            a.sums[3] += in.ore[i];
            a.sums[4] += in.fresh_water[i];
            a.sums[5] += in.habitability[i];
            a.hab.push_back(in.habitability[i]);
            if (in.ore[i] >= v.ore_rich_min) {
                ++a.rich;
                if (dw[i] <= v.ore_reach_m) {
                    ++a.reach;
                }
            }
        }
    }
    for (std::int64_t r = 1; r <= R; ++r) {
        Acc& a = acc[uz(r)];
        region_api::RegionRow row;
        row.id = r;
        std::optional<std::pair<std::int64_t, std::int64_t>> dom;  // (-cells, code)
        for (const auto& [code, cnt] : a.biome) {
            if (!nouns.at(code).water && (!dom || std::make_pair(-cnt, code) < *dom)) {
                dom = std::make_pair(-cnt, code);
            }
        }
        if (!dom) {
            // 3.2, RG10 (ruling of 2026-11-18): no dry cell, so the most
            // common code over all the region's cells.
            for (const auto& [code, cnt] : a.biome) {
                if (!dom || std::make_pair(-cnt, code) < *dom) {
                    dom = std::make_pair(-cnt, code);
                }
            }
        }
        REQUIRE(dom.has_value());
        row.biome = dom->second;
        row.x_min = a.xmin;
        row.y_min = a.ymin;
        row.x_max = a.xmax;
        row.y_max = a.ymax;
        const auto sc = static_cast<worldgen::geo::CellIndex>(seed_of[uz(r)]);
        row.seed_x = g.x_of(sc);
        row.seed_y = g.y_of(sc);
        row.landmass = o.landmass[uz(seed_of[uz(r)])];
        row.cells = a.cells;
        row.lake_cells = a.lake;
        row.river_cells = a.riv;
        row.river_class_max = a.rcmax;
        row.coast_cells = a.coast;
        row.walk_cells = a.walk;
        std::optional<std::pair<std::int64_t, std::int64_t>> wa;
        for (const auto& [id, cnt] : a.wa) {
            if (!wa || std::make_pair(-cnt, id) < *wa) {
                wa = std::make_pair(-cnt, id);
            }
        }
        row.walk_area = wa ? wa->second : 0;
        // A region with no dry cell: every value over dry cells is 0 (3.2).
        auto mean = [&](std::int64_t sum) { return a.dry > 0 ? own_rha(sum, a.dry) : 0; };
        row.elev_mean_dm = mean(a.esum);
        row.elev_max_dm = a.dry > 0 ? a.emax : 0;
        row.fertility_mean = mean(a.sums[0]);
        row.forage_mean = mean(a.sums[1]);
        row.timber_mean = mean(a.sums[2]);
        row.ore_mean = mean(a.sums[3]);
        row.fresh_water_mean = mean(a.sums[4]);
        row.habitability_mean = mean(a.sums[5]);
        std::sort(a.hab.begin(), a.hab.end());
        row.habitability_p90 = a.dry > 0 ? a.hab[uz(std::min(a.dry - 1, (90 * a.dry) / 100))] : 0;
        row.ore_rich_cells = a.rich;
        row.ore_reach_cells = a.reach;
        o.out.regions.push_back(row);
        for (const auto& [code, cnt] : a.biome) {
            o.out.biomes.push_back(region_api::BiomeRow{r, code, cnt});
        }
    }
    // RG11 1 to 4 (base names and lands); the draws follow RG12.
    std::vector<region_api::NameInput> name_inputs;
    std::vector<std::string> bases;
    if (R > 0) {
        std::int64_t land = 0, sx = 0, sy = 0;
        std::map<std::int64_t, std::int64_t> lm_cells;
        for (std::size_t i = 0; i < n; ++i) {
            if (is_land(in.water[i])) {
                const auto ci = static_cast<worldgen::geo::CellIndex>(i);
                ++land;
                sx += g.x_of(ci);
                sy += g.y_of(ci);
                ++lm_cells[o.landmass[i]];
            }
        }
        const std::int64_t cx = own_rha(sx, land), cy = own_rha(sy, land);
        const std::int64_t rc = worldgen::mul_int(Fx::from_raw(v.name_central_frac), g.width).round_int();
        std::optional<std::pair<std::int64_t, std::int64_t>> largest;  // (-cells, label)
        for (const auto& [lm, cnt] : lm_cells) {
            if (!largest || std::make_pair(-cnt, lm) < *largest) {
                largest = std::make_pair(-cnt, lm);
            }
        }
        std::map<std::int64_t, std::int64_t> per_lm;
        for (const auto& row : o.out.regions) {
            ++per_lm[row.landmass];
        }
        std::vector<region_api::NameInput> ni;
        for (const auto& row : o.out.regions) {
            const bool isle = per_lm[row.landmass] == 1 && row.landmass != largest->second;
            ni.push_back(region_api::NameInput{row.seed_x, row.seed_y, row.biome, isle, row.cells, row.lake_cells,
                                               row.walk_cells});
            o.stats.isle += isle ? 1 : 0;
            const bool mere = !isle && 2 * row.lake_cells >= row.cells;
            const bool own_noun = std::find(v.upland_own_noun.begin(), v.upland_own_noun.end(),
                                            nouns.at(row.biome).key) != v.upland_own_noun.end();
            o.stats.mere += mere ? 1 : 0;
            o.stats.fells += !isle && !mere && 4 * row.walk_cells < row.cells && !own_noun ? 1 : 0;
            o.stats.central += own_sector(row.seed_x - cx, row.seed_y - cy, rc) == 0 ? 1 : 0;
        }
        name_inputs = ni;
        bases = own_name_list(ni, cx, cy, rc, v, nouns);
        std::map<std::string, std::int64_t> seen;
        for (const std::string& b : bases) {
            // cover's `numbered`: every region of a base name but its first
            // (r2's numbered names; r3's drawn names are the same regions).
            o.stats.numbered += ++seen[b] >= 2 ? 1 : 0;
        }
    }
    // RG12.
    o.out.borders = own_borders(g, region, o.surface, in.walk_area, in.water, in.river, border, passes,
                                &o.stats.crossings, &o.stats.crossed);
    // RG11 r3 (after RG12: the pass trait reads the border rows).
    if (R > 0) {
        const region_names::PlaceData& data = region_names::shipped();
        std::set<std::int64_t> passed;
        for (const auto& b : o.out.borders) {
            if (b.passes > 0) {
                passed.insert(b.a);
                passed.insert(b.b);
            }
        }
        std::vector<region_names::NamedRegion> nr;
        for (std::size_t k = 0; k < o.out.regions.size(); ++k) {
            const auto& row = o.out.regions[k];
            region_names::NamedRegion x;
            x.id = row.id;
            x.cells = row.cells;
            x.base = bases[k];
            x.land = own_land_of(name_inputs[k], v, nouns, data);
            REQUIRE(x.land != nullptr);
            x.traits = region_names::TraitRow{row.coast_cells,    row.river_class_max,   row.lake_cells,
                                              row.cells,          row.elev_mean_dm,      row.ore_rich_cells,
                                              row.habitability_mean, row.fertility_mean, passed.count(row.id) > 0};
            nr.push_back(x);
            o.lands.push_back(x.land);
            o.trait_rows.push_back(x.traits);
        }
        o.bases = bases;
        const auto named = region_names::name_regions(nr, seed, static_cast<std::uint64_t>(g.width), data,
                                                      region_names::NameTuning{});
        REQUIRE_MESSAGE(named.has_value(), "own RG11: no free name");
        for (std::size_t k = 0; k < named->names.size(); ++k) {
            o.out.regions[k].name = named->names[k];
        }
        o.name_how = named->how;
    }
    o.stats.greedy = static_cast<std::int64_t>(o.greedy.size());
    o.stats.landmass_seeds = static_cast<std::int64_t>(o.seeds.size() - o.greedy.size());
    o.stats.rejected = ss.rejected;
    o.stats.bank = ss.bank;
    for (std::size_t i = 0; i < n; ++i) {
        o.stats.lake_cells += in.water[i] == WATER_LAKE ? 1 : 0;
        o.stats.border_river_cells += border[i];
        const auto ci = static_cast<worldgen::geo::CellIndex>(i);
        const std::int32_t x = g.x_of(ci), y = g.y_of(ci);
        for (const std::size_t d : {std::size_t{1}, std::size_t{3}}) {
            const std::int32_t xx = x + DX[d], yy = y + DY[d];
            if (xx < 0 || yy < 0 || xx >= g.width || yy >= g.height) {
                continue;
            }
            if (border[g.index(xx, y)] != 0U && border[g.index(x, yy)] != 0U && is_land(in.water[i]) &&
                is_land(in.water[g.index(xx, yy)])) {
                ++o.stats.corner;
            }
        }
    }
    return o;
}

inline std::string cover_of(const OwnStats& s) {
    return "greedy:" + std::to_string(s.greedy) + ",landmass_seeds:" + std::to_string(s.landmass_seeds) +
           ",rejected:" + std::to_string(s.rejected) + ",bank:" + std::to_string(s.bank) +
           ",merges:" + std::to_string(s.merges) + ",isle:" + std::to_string(s.isle) +
           ",central:" + std::to_string(s.central) + ",numbered:" + std::to_string(s.numbered) +
           ",crossings:" + std::to_string(s.crossings) + ",lake_cells:" + std::to_string(s.lake_cells) +
           ",border_river_cells:" + std::to_string(s.border_river_cells) + ",corner:" + std::to_string(s.corner) +
           ",mere:" + std::to_string(s.mere) + ",fells:" + std::to_string(s.fells) +
           ",crossed:" + std::to_string(s.crossed);
}

// r3 golden lines (10.5): "<id>:<name>|..." and "<id>:<how>|..." in id order.
inline std::string names_line(const std::vector<region_api::RegionRow>& rows) {
    std::string s;
    for (const auto& r : rows) {
        s += (s.empty() ? "" : "|") + std::to_string(r.id) + ":" + r.name;
    }
    return s.empty() ? "-" : s;
}
inline std::string attempts_line(const std::vector<region_api::RegionRow>& rows, const std::vector<std::string>& how) {
    std::string s;
    for (std::size_t k = 0; k < rows.size() && k < how.size(); ++k) {
        s += (s.empty() ? "" : "|") + std::to_string(rows[k].id) + ":" + how[k];
    }
    return s.empty() ? "-" : s;
}

// --- Hashes (P0-04 8; golden items) --------------------------------------------------------------

using elev_test::hash_i64;
inline std::string u16_hash(Grid g, std::span<const std::uint16_t> v) {
    return hydro_test::layer_hash<std::uint16_t>(g, 3, v);
}
inline std::string u8_hash(Grid g, std::span<const std::uint8_t> v) { return hydro_test::layer_hash<std::uint8_t>(g, 1, v); }

inline std::string hash_words(std::span<const std::int64_t> v, int bytes) {
    worldgen::Sha256 h;
    for (const std::int64_t x : v) {
        elev_test::put_le(h, static_cast<std::uint64_t>(x), bytes);
    }
    return worldgen::to_hex(h.final());
}
inline std::string hash_i32(std::span<const std::int64_t> v) { return hash_words(v, 4); }
inline std::string hash_u32(std::span<const std::int64_t> v) { return hash_words(v, 4); }

// The P0-04 8 logical hash of a table of integer and text columns.
struct TableHash {
    worldgen::Sha256 s;
    explicit TableHash(std::uint64_t columns) { elev_test::put_le(s, columns, 8); }
    void integer(std::int64_t v) {
        elev_test::put_le(s, 1, 1);
        elev_test::put_le(s, static_cast<std::uint64_t>(v), 8);
    }
    void text(std::string_view t) {
        elev_test::put_le(s, 3, 1);
        biome_test::put_bytes(s, t);
    }
    std::string hex() { return worldgen::to_hex(s.final()); }
};

inline std::string region_table_hash(const std::vector<region_api::RegionRow>& rows) {
    TableHash t(28);
    for (const auto& r : rows) {
        t.integer(r.id);
        t.text(r.name);
        for (const std::int64_t v :
             {r.biome, r.x_min, r.y_min, r.x_max, r.y_max, r.seed_x, r.seed_y, r.landmass, r.cells, r.lake_cells,
              r.river_cells, r.river_class_max, r.coast_cells, r.walk_cells, r.walk_area, r.elev_mean_dm,
              r.elev_max_dm, r.fertility_mean, r.forage_mean, r.timber_mean, r.ore_mean, r.fresh_water_mean,
              r.habitability_mean, r.habitability_p90, r.ore_rich_cells, r.ore_reach_cells}) {
            t.integer(v);
        }
    }
    return t.hex();
}

inline std::string border_table_hash(const std::vector<region_api::BorderRow>& rows) {
    TableHash t(11);
    for (const auto& r : rows) {
        for (const std::int64_t v : {r.a, r.b, r.length_m, r.low_dm, r.low_x, r.low_y, r.walk_pairs, r.river_pairs,
                                     r.river_class_max, r.pass_id, r.passes}) {
            t.integer(v);
        }
    }
    return t.hex();
}

inline std::string biome_table_hash(const std::vector<region_api::BiomeRow>& rows) {
    TableHash t(3);
    for (const auto& r : rows) {
        t.integer(r.region);
        t.integer(r.biome);
        t.integer(r.cells);
    }
    return t.hex();
}

// Golden item `passes`.
// r2: each row's id, col cell index (col_y W + col_x) and path length as
// int64 LE, then the path.
inline std::string passes_hash(std::span<const region_api::Pass> passes, Grid g) {
    if (passes.empty()) {
        return "-";
    }
    std::vector<const region_api::Pass*> v;
    for (const auto& p : passes) {
        v.push_back(&p);
    }
    std::sort(v.begin(), v.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    worldgen::Sha256 h;
    for (const auto* p : v) {
        elev_test::put_le(h, static_cast<std::uint64_t>(p->id), 8);
        elev_test::put_le(h, static_cast<std::uint64_t>(p->col_y * g.width + p->col_x), 8);
        elev_test::put_le(h, p->path.size(), 8);
        h.update(std::span<const std::uint8_t>(p->path));
    }
    return worldgen::to_hex(h.final());
}

// --- Report values (9) ---------------------------------------------------------------------------

inline std::string km2(std::int64_t cells) {
    const std::int64_t v = own_rha(cells * 256, 10000);
    const std::string frac = std::to_string(v % 100);
    return std::to_string(v / 100) + "." + (frac.size() < 2 ? "0" : "") + frac;
}

inline std::vector<std::pair<std::string, std::string>> report_values(const std::vector<region_api::RegionRow>& rows,
                                                                      const std::vector<region_api::BorderRow>& brs) {
    std::vector<std::pair<std::string, std::string>> out;
    out.emplace_back("regions", std::to_string(rows.size()));
    if (rows.empty()) {
        out.emplace_back("region_area_km2", "n/a n/a n/a");
    } else {
        std::vector<std::int64_t> c;
        for (const auto& r : rows) {
            c.push_back(r.cells);
        }
        std::sort(c.begin(), c.end());
        const auto n = static_cast<std::int64_t>(c.size());
        std::string s;
        for (const std::int64_t p : {0, 50, 100}) {
            s += (s.empty() ? "" : " ") + km2(c[uz(std::min(n - 1, (p * n) / 100))]);
        }
        out.emplace_back("region_area_km2", s);
    }
    std::int64_t walkable = 0, rv = 0, pass = 0;
    std::set<std::int64_t> used;
    for (const auto& b : brs) {
        walkable += b.walk_pairs > 0 ? 1 : 0;
        rv += b.river_pairs > 0 ? 1 : 0;
        pass += b.pass_id > 0 ? 1 : 0;
        used.insert(b.a);
        used.insert(b.b);
    }
    std::int64_t isolated = 0, upland = 0;
    for (const auto& r : rows) {
        isolated += used.count(r.id) == 0U ? 1 : 0;
        upland += 4 * r.walk_cells < r.cells ? 1 : 0;
    }
    out.emplace_back("region_borders", std::to_string(brs.size()));
    out.emplace_back("region_borders_walkable", std::to_string(walkable));
    out.emplace_back("region_borders_river", std::to_string(rv));
    out.emplace_back("region_borders_pass", std::to_string(pass));
    out.emplace_back("regions_isolated", std::to_string(isolated));
    out.emplace_back("regions_upland", std::to_string(upland));
    return out;
}

// The `ids` palette of P1-00 10.2.
struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    friend bool operator==(const Rgb&, const Rgb&) = default;
};
inline Rgb ids_colour(std::uint64_t v) {
    if (v == 0) {
        return Rgb{};
    }
    const std::uint64_t h = worldgen::mix64(v);
    auto ch = [&](unsigned j) { return static_cast<std::uint8_t>(64U + (((h >> (8U * j)) & 0xFFU) * 191U) / 255U); };
    return Rgb{ch(0), ch(1), ch(2)};
}

// P0-04 amendment 18 (e): true where the composite draws the region border.
inline std::vector<bool> border_cells(Grid g, std::span<const std::uint16_t> region) {
    std::vector<bool> out(g.cells(), false);
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const std::uint16_t v = region[g.index(x, y)];
            if (v == 0) {
                continue;
            }
            const bool e = x + 1 < g.width && region[g.index(x + 1, y)] != 0 && region[g.index(x + 1, y)] != v;
            const bool s = y + 1 < g.height && region[g.index(x, y + 1)] != 0 && region[g.index(x, y + 1)] != v;
            out[g.index(x, y)] = e || s;
        }
    }
    return out;
}

// --- Inputs ----------------------------------------------------------------------------------------

// The thirteen inputs from the resources inputs and the resources layers.
inline region_api::Inputs from_resources(const resource_api::Inputs& r, const resource_api::Layers& l) {
    region_api::Inputs in;
    in.grid = r.grid;
    in.elevation = r.elevation;
    in.water = r.water;
    in.water_depth = r.water_depth;
    in.flow_dir = r.flow_dir;
    in.river = r.river;
    in.biome = r.biome;
    in.fertility = l.fertility;
    in.ore = l.ore;
    in.timber = l.timber;
    in.fresh_water = l.fresh_water;
    in.forage = l.forage;
    in.habitability = l.habitability;
    in.walk_area = l.walk_area;
    return in;
}

// Table pass of an N x N world of the elevation fixture (the rows the
// elevation stage writes: id s + 1, path = the site's axis; P1-01 9.1),
// through the elevation seams of p1_01_elevation_api.hpp.
inline Grid g_of_n(std::int32_t n) { return Grid{n, n}; }

inline std::vector<region_api::Pass> world_passes(std::uint64_t seed, std::int32_t n, bool small) {
    static std::map<std::tuple<std::uint64_t, std::int32_t, bool>, std::vector<region_api::Pass>> cache;
    const auto key = std::make_tuple(seed, n, small);
    const auto it = cache.find(key);
    if (it != cache.end()) {
        return it->second;
    }
    const auto& t = hydro_test::elevation_tuning(small);
    const elev_api::Fields f = elev_api::fields(seed, n, t, 4);
    const elev_api::Ocean ocean = elev_api::classify(f, t);
    const elev_api::Heights h = elev_api::heights(seed, f, ocean, t, 4);
    std::vector<region_api::Pass> out;
    for (std::size_t s = 0; s < h.passes.sites.size(); ++s) {
        region_api::Pass p;
        p.id = static_cast<std::int64_t>(s) + 1;
        p.col_x = g_of_n(n).x_of(h.passes.sites[s].col);
        p.col_y = g_of_n(n).y_of(h.passes.sites[s].col);
        for (const auto c : h.passes.sites[s].axis) {
            const auto u = static_cast<std::uint32_t>(c);
            for (std::uint32_t b = 0; b < 4; ++b) {
                p.path.push_back(static_cast<std::uint8_t>((u >> (8U * b)) & 0xFFU));
            }
        }
        out.push_back(std::move(p));
    }
    cache.emplace(key, out);
    return out;
}

// The synthetic pass table "X" of the golden file (40-wide grids).
inline std::vector<region_api::Pass> synthetic_passes(std::string_view name, Grid g) {
    REQUIRE(name == "X");
    using P = std::vector<std::pair<std::int32_t, std::int32_t>>;
    P row, diag, col, anti;
    for (std::int32_t x = 2; x < 38; ++x) {
        row.emplace_back(x, 20);
    }
    for (std::int32_t k = 4; k < 36; ++k) {
        diag.emplace_back(k, k);
    }
    for (std::int32_t y = 2; y < 38; ++y) {
        col.emplace_back(20, y);
    }
    for (std::int32_t k = 0; k < 8; ++k) {
        anti.emplace_back(35 - k, 4 + k);
    }
    const P loop = {{10, 30}, {11, 29}, {11, 29}, {12, 30}, {13, 29}, {14, 30}, {13, 31}, {12, 32}, {11, 31}, {10, 30}};
    // Cols (r2) as in the reference's synthetic_passes.
    return {region_api::Pass{1, path_bytes(g, row), 30, 20}, region_api::Pass{2, path_bytes(g, diag), 10, 10},
            region_api::Pass{4, path_bytes(g, col), 20, 2}, region_api::Pass{7, path_bytes(g, loop), 10, 30},
            region_api::Pass{9, path_bytes(g, anti), 31, 8}};
}

// --- Golden file (worldgen/tests/golden/p1-06-regions.txt) ---------------------------------------

struct GoldenCase {
    std::string name;
    std::string base;     // the case of p1-05-resources.txt whose inputs are used
    std::string fixture;  // shipped | small | designed | terrace
    std::int32_t n = 0;
    std::uint64_t seed = 0;
    Overrides overrides;
    std::string passes;  // world | X
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
            ss >> gc.fixture >> n >> gc.seed >> ov >> gc.passes;
            REQUIRE_MESSAGE(!ss.fail(), "bad case line: " << line);
            gc.n = static_cast<std::int32_t>(n);
            if (ov != "-") {
                for (const std::string& part : split(ov, ',')) {
                    const std::size_t eq = part.find('=');
                    REQUIRE(eq != std::string::npos);
                    gc.overrides.emplace_back(part.substr(0, eq), part.substr(eq + 1));
                }
            }
            out.push_back(gc);
        } else {
            REQUIRE_MESSAGE((!out.empty() && out.back().name == a), "golden line outside its case: " << line);
            if (b == "names" || b == "name_attempts") {
                // r3: the value is the rest of the line (names hold spaces).
                c = line.substr(a.size() + 1 + b.size() + 1);
            }
            out.back().items.emplace_back(b, c);
        }
    }
    return out;
}

inline const std::vector<GoldenCase>& golden() {
    static const std::vector<GoldenCase> g = read_golden(UNMAPPED_P1_06_GOLDEN_PATH);
    return g;
}

inline const GoldenCase* find_case(std::string_view name) {
    for (const GoldenCase& c : golden()) {
        if (c.name == name) {
            return &c;
        }
    }
    return nullptr;
}

inline const resource_test::GoldenCase& resources_case(std::string_view base) {
    for (const resource_test::GoldenCase& c : resource_test::golden()) {
        if (c.name == base) {
            return c;
        }
    }
    FAIL("no case " << std::string(base) << " in p1-05-resources.txt");
    return resource_test::golden().front();
}

// The thirteen inputs of a golden case: the earlier stages (through the
// resources test's inputs and the resources stage, with the p1-05 case's
// tuning) and the pass table (the world's, through the elevation seams,
// or a synthetic one). N <= 128 only; the large cases read a generated world.
struct CaseInputs {
    region_api::Inputs in;
    std::vector<region_api::Pass> passes;
};

inline CaseInputs case_inputs(const GoldenCase& c, std::uint32_t threads = 4) {
    const resource_test::GoldenCase& rc = resources_case(c.base);
    const resource_api::Inputs ri = resource_test::case_inputs(rc);
    const resource_api::Layers l =
        resource_api::layers(ri, resource_test::definitions(), resource_test::tuning(rc.overrides), rc.seed, threads);
    CaseInputs out{from_resources(ri, l), {}};
    if (c.passes == "world") {
        if (c.fixture == "shipped" || c.fixture == "small") {
            out.passes = world_passes(c.seed, c.n, c.fixture == "small");
        }
    } else {
        out.passes = synthetic_passes(c.passes, out.in.grid);
    }
    return out;
}

}  // namespace region_test
