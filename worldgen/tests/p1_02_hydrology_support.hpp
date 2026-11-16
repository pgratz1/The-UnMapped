#pragma once

// Support for the P1-02 hydrology tests: the hand-made fixtures of spec
// sections 5.1 and 8.1 with their expected grids, the tuning texts, the
// random fixtures of 8.2, layer hashes, the golden-file reader, and code
// written independently of the stage from the spec text: the invariants of
// section 6 checked with flood levels by a Dijkstra-style minimax search,
// accumulation by walking upstream, lakes by union-find, the chamfer by
// Dijkstra, and the routing flood with a binary heap and with a bucket
// queue.
//
// Written from docs/specs/p1-02-hydrology.md, docs/specs/p1-00-geography-
// overview.md and the P0 specs only. Nothing here calls the stage; the
// stage is reached only through p1_02_hydrology_api.hpp. The expected grids
// of the designed fixtures (8.1 items 5 and 6) were computed by
// worldgen/tests/reference/p1_hydrology_reference.py (command `fixtures`).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <ostream>
#include <queue>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "p1_01_elevation_support.hpp"
#include "p1_02_hydrology_api.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/geo/grid.hpp"

namespace hydro_test {

using worldgen::geo::Grid;

inline constexpr std::int64_t INF = std::int64_t{1} << 40;
inline constexpr std::string_view PRECONDITION = "hydrology: elevation precondition: ";

// Threads for tests that are not about threads (P1-00 D10).
inline constexpr std::uint32_t THREADS = 4;

// --- Tuning (P1-02 section 4) ----------------------------------------------------------

inline std::string hydrology_toml(std::int64_t river_min_area_m2) {
    return "[worldgen.hydrology]\nriver_min_area_m2 = " + std::to_string(river_min_area_m2) + "\n";
}

// T = ceil(river_min_area_m2 / 256), the test's own (section 4).
[[nodiscard]] constexpr std::int64_t threshold(std::int64_t river_min_area_m2) {
    return (river_min_area_m2 + 255) / 256;
}

// The shipped value, which is also the `geo` and `geo-small` fixture value:
// 125000 m^2 (T = 489) by the lead's tuning decision of 2026-11-16; the spec's
// starting value was 500000 (T = 1954).
inline constexpr std::int64_t SHIPPED_AREA = 125000;

// Hydrology tuning with the given river_min_area_m2, loaded through the
// production loader beside the shipped elevation fixture.
inline hydro_api::Tuning tuning(std::int64_t river_min_area_m2) {
    const hydro_api::TuningLoad l =
        hydro_api::load_tuning_text(elev_test::elevation_toml(), hydrology_toml(river_min_area_m2));
    REQUIRE_MESSAGE(l.ok(), "hydrology.toml with river_min_area_m2 = " << river_min_area_m2 << " does not load");
    return hydro_api::tuning_from(*l.content);
}

inline const hydro_api::Tuning& shipped_tuning() {
    static const hydro_api::Tuning t = tuning(SHIPPED_AREA);
    return t;
}

// Elevation tuning of P1-01 4.4: shipped (`geo`) or the small-map fixture.
inline const worldgen::stages::ElevationTuning& elevation_tuning(bool small) {
    static const auto load = [](bool sm) {
        const hydro_api::TuningLoad l = hydro_api::load_tuning_text(
            sm ? elev_test::elevation_toml(elev_test::small_overrides()) : elev_test::elevation_toml(),
            hydrology_toml(SHIPPED_AREA));
        REQUIRE_MESSAGE(l.ok(), "elevation fixture does not load");
        return worldgen::stages::elevation_tuning_from(*l.content);
    };
    static const worldgen::stages::ElevationTuning shipped = load(false);
    static const worldgen::stages::ElevationTuning sm = load(true);
    return small ? sm : shipped;
}

// --- Fixtures ----------------------------------------------------------------------

struct Fixture {
    Grid grid;
    std::vector<std::int16_t> e;
};

inline Fixture fixture(const std::vector<std::vector<std::int16_t>>& rows) {
    Fixture f;
    f.grid = Grid{static_cast<std::int32_t>(rows[0].size()), static_cast<std::int32_t>(rows.size())};
    for (const auto& r : rows) {
        REQUIRE(r.size() == rows[0].size());
        f.e.insert(f.e.end(), r.begin(), r.end());
    }
    return f;
}

template <typename T>
inline std::vector<T> flat(const std::vector<std::vector<T>>& rows) {
    std::vector<T> out;
    for (const auto& r : rows) {
        out.insert(out.end(), r.begin(), r.end());
    }
    return out;
}

// Worked example 5.1 (8 x 7).
inline Fixture example_51() {
    return fixture({
        {-5, -5, -5, -5, -5, -5, -5, -5},
        {-5, 60, 70, 80, 70, 60, 50, -5},
        {-5, 50, 20, 22, 60, 40, 30, -5},
        {-5, 45, 21, 23, 55, 35, 25, -5},
        {-5, 40, 30, 28, 40, 30, 20, -5},
        {-5, 35, 33, 31, 25, 20, 10, -5},
        {-5, -5, -5, -5, -5, -5, -5, -5},
    });
}

// river_min_area_m2 for 5.1: any value in 513 to 768 gives T = 3.
inline constexpr std::int64_t EXAMPLE_AREA = 600;

// 8.1 item 3 (6 x 3).
inline Fixture ties_fixture() {
    return fixture({{-1, -1, -1, -1, -1, -1}, {-1, 10, 10, 10, 10, 10}, {-1, -1, -1, -1, -1, -1}});
}

// 8.1 item 4 (7 x 5).
inline Fixture plateau_fixture() {
    return fixture({
        {-1, -1, -1, -1, -1, -1, -1},
        {-1, 10, 10, 10, 10, 10, -1},
        {-1, 10, 10, 10, 10, 10, -1},
        {-1, 10, 10, 10, 10, 10, -1},
        {-1, -1, -1, -1, -1, -1, -1},
    });
}

// 8.1 item 5, designed by the test writer (11 x 11): two lakes at
// different levels. The upper lake (cells (5..7, 2..3), level 150) spills
// over its rim (6, 4) down a channel (5, 5), (5, 6) into the lower lake
// (cells (2..4, 7..8), level 80), which spills over (1, 8) into the sea.
// The upper lake has the smaller index, so it is lake 1 although it is
// flooded later: ids follow the smallest index, not the pop order or the
// level.
inline Fixture terrace_fixture() {
    constexpr std::int16_t B = 200;
    return fixture({
        {-10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10},
        {-10, B, B, B, B, B, B, B, B, B, -10},
        {-10, B, B, B, B, 120, 110, 120, B, B, -10},
        {-10, B, B, B, B, 125, 115, 130, B, B, -10},
        {-10, B, B, B, B, B, 150, B, B, B, -10},
        {-10, B, B, B, B, 140, B, B, B, B, -10},
        {-10, B, B, B, B, 120, B, B, B, B, -10},
        {-10, B, 50, 40, 50, B, B, B, B, B, -10},
        {-10, 80, 45, 35, 50, B, B, B, B, B, -10},
        {-10, B, B, B, B, B, B, B, B, B, -10},
        {-10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10},
    });
}

// river_min_area_m2 for the terrace: T = 4.
inline constexpr std::int64_t TERRACE_AREA = 1024;

// 8.1 item 6, designed by the test writer (9 x 6): a 3 x 2 basin (cells
// (3..5, 2..3), level 40) with two rim cells at the lake level, (2, 3) on
// the west and (6, 2) on the east, each with a step down (30) to the sea.
// G4 alone gives four exits ((3, 2) and (3, 3) to the west rim, (5, 2) and
// (5, 3) to the east rim); H4 keeps only the first-popped lake cell, (5, 3).
inline Fixture rims_fixture() {
    return fixture({
        {-5, -5, -5, -5, -5, -5, -5, -5, -5},
        {-5, 100, 100, 100, 100, 100, 100, 100, -5},
        {-5, 100, 100, 20, 25, 20, 40, 30, -5},
        {-5, 30, 40, 22, 21, 24, 100, 100, -5},
        {-5, 100, 100, 100, 100, 100, 100, 100, -5},
        {-5, -5, -5, -5, -5, -5, -5, -5, -5},
    });
}

// A corridor of one row: ocean at x = 0, land e = 1 + x div 2 (at most
// 1 + w / 2, inside the elevation range for w <= 59998). Every cell drains
// west (on each two-cell flat the east cell takes its flood parent, the west
// cell), so flow_accum at x is w - x: every value from 1 to w - 1 occurs.
inline Fixture corridor_fixture(std::int32_t w) {
    Fixture f;
    f.grid = Grid{w, 1};
    for (std::int32_t x = 0; x < w; ++x) {
        f.e.push_back(static_cast<std::int16_t>(x == 0 ? -1 : 1 + x / 2));
    }
    return f;
}

// Random fixture f of 8.2: 34 x 34, ocean border e in [-50, -1], interior
// e in [1, 200], drawn in index order from
// Rng(substream(stream_key(0, "test.hydro"), f)).
inline constexpr std::string_view RANDOM_STREAM = "test.hydro";

inline Fixture random_fixture(std::uint64_t f) {
    worldgen::Rng rng(worldgen::substream(worldgen::stream_key(0, RANDOM_STREAM), f));
    Fixture out;
    out.grid = Grid{34, 34};
    for (std::int32_t y = 0; y < 34; ++y) {
        for (std::int32_t x = 0; x < 34; ++x) {
            const bool border = x == 0 || y == 0 || x == 33 || y == 33;
            out.e.push_back(static_cast<std::int16_t>(border ? rng.uniform_i64(-50, -1) : rng.uniform_i64(1, 200)));
        }
    }
    return out;
}

// river_min_area_m2 for the random fixtures: T = 8.
inline constexpr std::int64_t RANDOM_AREA = 2048;

inline std::vector<std::uint8_t> ocean_of(std::span<const std::int16_t> e) {
    std::vector<std::uint8_t> o(e.size());
    for (std::size_t i = 0; i < e.size(); ++i) {
        o[i] = e[i] < 0 ? 1 : 0;
    }
    return o;
}

inline std::vector<std::int64_t> keys_of(std::span<const std::int16_t> e) {
    return std::vector<std::int64_t>(e.begin(), e.end());
}

// --- Layer hashes (P0-04 8) -----------------------------------------------------------

template <typename T>
[[nodiscard]] inline std::string layer_hash(Grid g, std::uint8_t type_code, std::span<const T> v) {
    worldgen::Sha256 s;
    elev_test::put_le(s, static_cast<std::uint64_t>(g.width), 4);
    elev_test::put_le(s, static_cast<std::uint64_t>(g.height), 4);
    elev_test::put_le(s, type_code, 1);
    for (const T x : v) {
        using U = std::make_unsigned_t<T>;
        elev_test::put_le(s, static_cast<std::uint64_t>(static_cast<U>(x)), static_cast<int>(sizeof(T)));
    }
    return worldgen::to_hex(s.final());
}

[[nodiscard]] inline std::string hash_u32(std::span<const std::uint32_t> v) {
    worldgen::Sha256 h;
    for (const std::uint32_t x : v) {
        elev_test::put_le(h, x, 4);
    }
    return worldgen::to_hex(h.final());
}

// The seven layer hashes, in H9 order, with their names.
inline std::vector<std::pair<std::string, std::string>> layer_hashes(Grid g, const hydro_api::Layers& l) {
    return {
        {"water", layer_hash<std::uint8_t>(g, 1, l.water)},
        {"water_depth", layer_hash<std::uint16_t>(g, 3, l.water_depth)},
        {"flow_dir", layer_hash<std::uint8_t>(g, 1, l.flow_dir)},
        {"flow_accum", layer_hash<std::uint32_t>(g, 5, l.flow_accum)},
        {"river", layer_hash<std::uint8_t>(g, 1, l.river)},
        {"lake", layer_hash<std::uint32_t>(g, 5, l.lake)},
        {"coast_dist", layer_hash<std::int16_t>(g, 4, l.coast_dist)},
    };
}

// --- Independent algorithms ----------------------------------------------------------

[[nodiscard]] inline std::int64_t step_to(Grid g, std::size_t i, std::uint8_t code) {
    worldgen::geo::CellIndex m = 0;
    if (code > 7 || !worldgen::geo::neighbour(g, static_cast<worldgen::geo::CellIndex>(i), code, m)) {
        return -1;
    }
    return static_cast<std::int64_t>(m);
}

[[nodiscard]] inline std::uint8_t bit_length(std::uint64_t v) {
    std::uint8_t b = 0;
    while (v != 0U) {
        ++b;
        v >>= 1U;
    }
    return b;
}

[[nodiscard]] inline std::uint8_t river_class(std::uint64_t a, std::uint64_t t) {
    return a < t ? 0 : std::min<std::uint8_t>(15, bit_length(a / t));
}

[[nodiscard]] inline std::int16_t coast_metres(std::int64_t d) {
    return static_cast<std::int16_t>(std::min<std::int64_t>(32767, (32 * d + 5) / 10));
}

// The routing flood of P1-00 7.3, the test's own, with a binary heap on
// (key, seq) or with a bucket queue (FIFO per level).
struct OwnFlood {
    std::vector<std::int64_t> level;
    std::vector<std::uint8_t> parent;
    std::vector<std::uint32_t> order;
};

inline OwnFlood own_flood(Grid g, std::span<const std::int64_t> keys, std::span<const std::uint8_t> ocean,
                          bool bucket) {
    const std::size_t n = elev_test::cells(g);
    OwnFlood f{std::vector<std::int64_t>(n, 0), std::vector<std::uint8_t>(n, 8), {}};
    std::vector<std::uint8_t> visited(n, 0);
    using Item = std::tuple<std::int64_t, std::uint64_t, std::size_t>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> heap;
    std::map<std::int64_t, std::deque<std::size_t>> buckets;
    std::uint64_t seq = 0;
    auto push = [&](std::int64_t k, std::size_t c) {
        if (bucket) {
            buckets[k].push_back(c);
        } else {
            heap.emplace(k, seq++, c);
        }
    };
    for (std::size_t i = 0; i < n; ++i) {
        if (ocean[i] != 0U) {
            visited[i] = 1;
            push(0, i);
        }
    }
    for (;;) {
        std::size_t c = 0;
        if (bucket) {
            while (!buckets.empty() && buckets.begin()->second.empty()) {
                buckets.erase(buckets.begin());
            }
            if (buckets.empty()) {
                break;
            }
            c = buckets.begin()->second.front();
            buckets.begin()->second.pop_front();
        } else {
            if (heap.empty()) {
                break;
            }
            c = std::get<2>(heap.top());
            heap.pop();
        }
        f.order.push_back(static_cast<std::uint32_t>(c));
        elev_test::each_neighbour8(g, c, [&](std::uint8_t code, std::size_t m) {
            if (visited[m] != 0U) {
                return;
            }
            visited[m] = 1;
            f.level[m] = std::max(keys[m], f.level[c]);
            f.parent[m] = static_cast<std::uint8_t>((code + 4U) % 8U);
            push(f.level[m], m);
        });
    }
    return f;
}

// Invariants 1 to 9 of P1-02 section 6 on one result, by code that does
// not reuse the stage's steps. Returns "" or the first violation found.
inline std::string invariant_violation(Grid g, std::span<const std::int16_t> e, const hydro_api::Layers& l,
                                       std::int64_t t) {
    const std::size_t n = elev_test::cells(g);
    std::ostringstream why;
    if (l.water.size() != n || l.water_depth.size() != n || l.flow_dir.size() != n || l.flow_accum.size() != n ||
        l.river.size() != n || l.lake.size() != n || l.coast_dist.size() != n) {
        return "a layer has the wrong size";
    }
    std::vector<std::uint8_t> ocean = ocean_of(e);
    // 1. flow_dir 8 exactly on ocean, else a code to a cell inside the map.
    std::vector<std::int64_t> down(n, -1);
    for (std::size_t i = 0; i < n; ++i) {
        if (ocean[i] != 0U) {
            if (l.flow_dir[i] != 8U) {
                why << "inv 1: flow_dir " << int(l.flow_dir[i]) << " on ocean cell " << i;
                return why.str();
            }
            continue;
        }
        down[i] = step_to(g, i, l.flow_dir[i]);
        if (down[i] < 0) {
            why << "inv 1: flow_dir " << int(l.flow_dir[i]) << " at land cell " << i << " leaves the map or is 8";
            return why.str();
        }
    }
    // 2. Every land cell reaches the ocean: no cycle (memoized walk).
    {
        std::vector<std::uint8_t> state(n, 0);  // 1 on the current path, 2 known to drain
        for (std::size_t s = 0; s < n; ++s) {
            if (ocean[s] != 0U || state[s] == 2U) {
                continue;
            }
            std::vector<std::size_t> path;
            std::size_t x = s;
            while (ocean[x] == 0U && state[x] == 0U) {
                state[x] = 1;
                path.push_back(x);
                x = static_cast<std::size_t>(down[x]);
            }
            if (ocean[x] == 0U && state[x] == 1U) {
                why << "inv 2: a flow_dir cycle through cell " << x;
                return why.str();
            }
            for (const std::size_t p : path) {
                state[p] = 2;
            }
        }
    }
    // 3. W = e + depth on land; W = L (minimax); W never rises downstream.
    const std::vector<std::int64_t> L = elev_test::minimax_levels(g, keys_of(e), ocean);
    std::vector<std::int64_t> W(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        W[i] = ocean[i] != 0U ? 0 : std::int64_t{e[i]} + std::int64_t{l.water_depth[i]};
    }
    for (std::size_t i = 0; i < n; ++i) {
        if (ocean[i] != 0U) {
            continue;
        }
        if (W[i] != L[i]) {
            why << "inv 3: surface " << W[i] << " is not the flood level " << L[i] << " at cell " << i;
            return why.str();
        }
        if (W[static_cast<std::size_t>(down[i])] > W[i]) {
            why << "inv 3: flow uphill from cell " << i;
            return why.str();
        }
    }
    // 4. flow_accum = 1 + the sum over donors (walked upstream); 0 on ocean.
    {
        std::vector<std::vector<std::size_t>> ups(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (ocean[i] == 0U) {
                ups[static_cast<std::size_t>(down[i])].push_back(i);
            }
        }
        std::vector<std::uint64_t> A(n, 0);
        std::vector<std::uint8_t> done(n, 0);
        for (std::size_t s = 0; s < n; ++s) {
            if (ocean[s] != 0U || done[s] != 0U) {
                continue;
            }
            std::vector<std::pair<std::size_t, std::size_t>> stack = {{s, 0}};
            while (!stack.empty()) {
                auto& [x, k] = stack.back();
                if (k < ups[x].size()) {
                    const std::size_t u = ups[x][k];
                    ++k;
                    if (done[u] == 0U) {
                        stack.emplace_back(u, 0);
                    }
                } else {
                    std::uint64_t sum = 1;
                    for (const std::size_t u : ups[x]) {
                        sum += A[u];
                    }
                    A[x] = sum;
                    done[x] = 1;
                    stack.pop_back();
                }
            }
        }
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint64_t want = ocean[i] != 0U ? 0 : A[i];
            if (l.flow_accum[i] != want) {
                why << "inv 4: flow_accum " << l.flow_accum[i] << " at cell " << i << ", expected " << want;
                return why.str();
            }
        }
    }
    // 5. Rivers.
    for (std::size_t i = 0; i < n; ++i) {
        const bool is_river = ocean[i] == 0U && l.lake[i] == 0U && std::int64_t{l.flow_accum[i]} >= t;
        if ((l.river[i] > 0U) != (l.water[i] == 1U) || (l.water[i] == 1U) != is_river) {
            why << "inv 5: river / water / threshold disagree at cell " << i;
            return why.str();
        }
        if (!is_river) {
            continue;
        }
        if (l.river[i] != river_class(l.flow_accum[i], static_cast<std::uint64_t>(t))) {
            why << "inv 5: river class " << int(l.river[i]) << " at cell " << i;
            return why.str();
        }
        const std::size_t m = static_cast<std::size_t>(down[i]);
        if (l.water[m] == 0U) {
            why << "inv 5: river cell " << i << " flows onto dry land";
            return why.str();
        }
        if (l.water[m] == 1U && l.river[m] < l.river[i]) {
            why << "inv 5: river class decreases downstream of cell " << i;
            return why.str();
        }
    }
    // 6. Lakes: union-find components of {land, L > e} joined where L is
    // equal, numbered by smallest index; flat; positive depth; one exit
    // over a rim at the level.
    {
        std::vector<std::uint8_t> mask(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            mask[i] = (ocean[i] == 0U && L[i] > e[i]) ? 1 : 0;
        }
        const std::vector<std::uint32_t> uf =
            elev_test::uf_components(g, mask, true, [&L](std::size_t a, std::size_t b) { return L[a] == L[b]; });
        for (std::size_t i = 0; i < n; ++i) {
            if (uf[i] != l.lake[i]) {
                why << "inv 6: lake id " << l.lake[i] << " at cell " << i << ", expected " << uf[i];
                return why.str();
            }
        }
        std::uint32_t k_max = 0;
        for (const std::uint32_t k : l.lake) {
            k_max = std::max(k_max, k);
        }
        std::vector<std::int64_t> level(k_max + 1U, -1);
        std::vector<std::uint32_t> exits(k_max + 1U, 0);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t k = l.lake[i];
            if (k == 0U) {
                continue;
            }
            if (l.water_depth[i] == 0U) {
                why << "inv 6: lake cell " << i << " with depth 0";
                return why.str();
            }
            if (level[k] < 0) {
                level[k] = W[i];
            } else if (level[k] != W[i]) {
                why << "inv 6: lake " << k << " is not flat";
                return why.str();
            }
        }
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t k = l.lake[i];
            if (k == 0U) {
                continue;
            }
            const std::size_t m = static_cast<std::size_t>(down[i]);
            if (l.lake[m] != k) {
                ++exits[k];
                if (ocean[m] != 0U || std::int64_t{e[m]} != level[k]) {
                    why << "inv 6: lake " << k << " leaves at cell " << i << " not over a rim at its level";
                    return why.str();
                }
            }
        }
        for (std::uint32_t k = 1; k <= k_max; ++k) {
            if (exits[k] != 1U) {
                why << "inv 6: lake " << k << " has " << exits[k] << " cells draining out of it";
                return why.str();
            }
        }
    }
    // 7. Water and depth.
    for (std::size_t i = 0; i < n; ++i) {
        const bool oc = ocean[i] != 0U;
        if ((l.water[i] == 3U) != oc || (oc && std::int64_t{l.water_depth[i]} != -std::int64_t{e[i]})) {
            why << "inv 7: ocean water or depth at cell " << i;
            return why.str();
        }
        if ((l.water[i] == 2U) != (l.lake[i] > 0U) || (l.lake[i] > 0U) != (!oc && l.water_depth[i] > 0U)) {
            why << "inv 7: lake water or depth at cell " << i;
            return why.str();
        }
        if (l.water[i] > 3U) {
            why << "inv 7: water value " << int(l.water[i]) << " at cell " << i;
            return why.str();
        }
    }
    // 8. The ocean of the water layer is one 4-connected region touching
    // the edge.
    {
        std::vector<std::uint8_t> seen(n, 0);
        std::deque<std::size_t> q;
        for (std::size_t i = 0; i < n; ++i) {
            const std::int32_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(i));
            const std::int32_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(i));
            if (l.water[i] == 3U && (x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1)) {
                seen[i] = 1;
                q.push_back(i);
            }
        }
        while (!q.empty()) {
            const std::size_t c = q.front();
            q.pop_front();
            for (const std::uint8_t code : {std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{6}}) {
                const std::int64_t m = step_to(g, c, code);
                if (m >= 0 && l.water[static_cast<std::size_t>(m)] == 3U && seen[static_cast<std::size_t>(m)] == 0U) {
                    seen[static_cast<std::size_t>(m)] = 1;
                    q.push_back(static_cast<std::size_t>(m));
                }
            }
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (l.water[i] == 3U && seen[i] == 0U) {
                why << "inv 8: ocean cell " << i << " not connected to the edge";
                return why.str();
            }
        }
    }
    // 9. Coast distance: sign, the 16 m neighbours, and the chamfer path
    // minimum by Dijkstra.
    {
        std::vector<std::uint8_t> land(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            land[i] = ocean[i] != 0U ? 0 : 1;
        }
        const std::vector<std::int64_t> dl = elev_test::chamfer_dijkstra(g, ocean);
        const std::vector<std::int64_t> dw = elev_test::chamfer_dijkstra(g, land);
        for (std::size_t i = 0; i < n; ++i) {
            const bool oc = ocean[i] != 0U;
            const std::int16_t want = oc ? static_cast<std::int16_t>(-coast_metres(dw[i])) : coast_metres(dl[i]);
            if (l.coast_dist[i] != want) {
                why << "inv 9: coast_dist " << l.coast_dist[i] << " at cell " << i << ", expected " << want;
                return why.str();
            }
            if (oc ? l.coast_dist[i] >= 0 : l.coast_dist[i] <= 0) {
                why << "inv 9: coast_dist sign at cell " << i;
                return why.str();
            }
            bool touches = false;
            for (const std::uint8_t code : {std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{6}}) {
                const std::int64_t m = step_to(g, i, code);
                touches = touches || (m >= 0 && (ocean[static_cast<std::size_t>(m)] != 0U) != oc);
            }
            if (touches && l.coast_dist[i] != (oc ? -16 : 16)) {
                why << "inv 9: cell " << i << " beside the coast has " << l.coast_dist[i];
                return why.str();
            }
        }
    }
    return {};
}

// --- Golden file (worldgen/tests/golden/p1-02-hydrology.txt) -----------------------

struct GoldenCase {
    std::string name;
    std::string fixture;  // shipped | small | random
    std::int32_t n = 0;
    std::uint64_t seed = 0;
    std::int64_t area = 0;
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
            ss >> n >> gc.seed >> gc.area;
            gc.n = static_cast<std::int32_t>(n);
            REQUIRE_MESSAGE(!ss.fail(), "bad case line: " << line);
            out.push_back(gc);
        } else {
            REQUIRE_MESSAGE((!out.empty() && out.back().name == a), "golden line outside its case: " << line);
            out.back().items.emplace_back(b, c);
        }
    }
    return out;
}

// Comma-separated integers of a golden item ("-" is the empty list).
inline std::vector<std::int64_t> golden_list(const std::string& v) {
    std::vector<std::int64_t> out;
    if (v == "-") {
        return out;
    }
    std::stringstream ss(v);
    std::string part;
    while (std::getline(ss, part, ',')) {
        out.push_back(std::stoll(part));
    }
    return out;
}

}  // namespace hydro_test
