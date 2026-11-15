// G2: connected components and the FIFO breadth-first search they are
// built from (spec P1-00 section 7.2, rule D5; P1-02 H3, H4 step 2).
//
// Written from docs/specs/p1-00-geography-overview.md,
// docs/specs/p1-02-hydrology.md and the public header
// worldgen/geo/components.hpp only. Hand-checked shapes, then agreement
// with a reference flood written here from the definition on seeded random
// masks, and a union-find check of the partition.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <numeric>
#include <ostream>
#include <string>
#include <vector>

#include "core_test_util.hpp"
#include "worldgen/geo/components.hpp"
#include "worldgen/geo/grid.hpp"

namespace geo = worldgen::geo;
using geo::Connectivity;
using geo::Grid;

namespace {

constexpr std::uint64_t KEY_G2 = 0x5445535447324332ULL;

// A mask from rows of '#' (in P) and '.' (not in P).
std::vector<std::uint8_t> mask_of(const std::vector<std::string>& rows) {
    std::vector<std::uint8_t> m;
    for (const std::string& r : rows) {
        for (const char c : r) {
            m.push_back(c == '#' ? std::uint8_t{1} : std::uint8_t{0});
        }
    }
    return m;
}

Grid grid_of(const std::vector<std::string>& rows) {
    return Grid{static_cast<std::int32_t>(rows.front().size()), static_cast<std::int32_t>(rows.size())};
}

// The reference of P1-00 7.2, written from the definition: scan cells in
// index order; each unlabelled P-cell starts a component with the next label,
// collected by FIFO BFS visiting neighbours in the 2.3 order; a step u -> v
// needs v in P and (no keys, or equal keys).
struct RefComponents {
    std::vector<std::uint32_t> label;
    std::vector<geo::Direction> parent;
    std::vector<geo::CellIndex> order;
    std::vector<std::uint32_t> start;
    std::uint32_t count = 0;
};

RefComponents reference(Grid g, const std::vector<std::uint8_t>& mask, Connectivity c,
                        const std::vector<std::int64_t>& keys) {
    static const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    const int codes8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    const int codes4[4] = {0, 2, 4, 6};
    const int* codes = c == Connectivity::eight ? codes8 : codes4;
    const int ncodes = c == Connectivity::eight ? 8 : 4;
    RefComponents r;
    const std::size_t n = mask.size();
    r.label.assign(n, 0U);
    r.parent.assign(n, geo::NO_DIRECTION);
    for (std::size_t s = 0; s < n; ++s) {
        if (mask[s] == 0U || r.label[s] != 0U) {
            continue;
        }
        ++r.count;
        r.start.push_back(static_cast<std::uint32_t>(r.order.size()));
        std::deque<std::size_t> q;
        q.push_back(s);
        r.label[s] = r.count;
        while (!q.empty()) {
            const std::size_t u = q.front();
            q.pop_front();
            r.order.push_back(static_cast<geo::CellIndex>(u));
            const int ux = static_cast<int>(u % static_cast<std::size_t>(g.width));
            const int uy = static_cast<int>(u / static_cast<std::size_t>(g.width));
            for (int j = 0; j < ncodes; ++j) {
                const int k = codes[j];
                const int vx = ux + dx[k];
                const int vy = uy + dy[k];
                if (vx < 0 || vy < 0 || vx >= g.width || vy >= g.height) {
                    continue;
                }
                const auto v = static_cast<std::size_t>(vy * g.width + vx);
                if (mask[v] == 0U || r.label[v] != 0U) {
                    continue;
                }
                if (!keys.empty() && keys[v] != keys[u]) {
                    continue;
                }
                r.label[v] = r.count;
                r.parent[v] = static_cast<geo::Direction>((k + 4) % 8);
                q.push_back(v);
            }
        }
    }
    r.start.push_back(static_cast<std::uint32_t>(r.order.size()));
    return r;
}

// Union-find over adjacent P-cells with equal keys: an independent check
// that the labels partition the P-cells exactly into the components.
struct UnionFind {
    std::vector<std::size_t> p;
    explicit UnionFind(std::size_t n) : p(n) { std::iota(p.begin(), p.end(), std::size_t{0}); }
    std::size_t find(std::size_t a) {
        while (p[a] != a) {
            p[a] = p[p[a]];
            a = p[a];
        }
        return a;
    }
    void unite(std::size_t a, std::size_t b) { p[find(a)] = find(b); }
};

void check_partition(Grid g, const std::vector<std::uint8_t>& mask, Connectivity c,
                     const std::vector<std::int64_t>& keys, const geo::Components& comp) {
    UnionFind uf(mask.size());
    for (geo::CellIndex i = 0; i < g.cells(); ++i) {
        if (mask[i] == 0U) {
            continue;
        }
        for (const geo::Direction code : geo::neighbour_order(c)) {
            geo::CellIndex m = 0;
            if (geo::neighbour(g, i, code, m) && mask[m] != 0U && (keys.empty() || keys[m] == keys[i])) {
                uf.unite(i, m);
            }
        }
    }
    for (geo::CellIndex i = 0; i < g.cells(); ++i) {
        CHECK((comp.label[i] == 0U) == (mask[i] == 0U));
    }
    for (geo::CellIndex i = 0; i < g.cells(); ++i) {
        for (geo::CellIndex j = i + 1U; j < g.cells() && j < i + 40U; ++j) {
            if (mask[i] != 0U && mask[j] != 0U) {
                CHECK((uf.find(i) == uf.find(j)) == (comp.label[i] == comp.label[j]));
            }
        }
    }
}

void check_against_reference(Grid g, const std::vector<std::uint8_t>& mask, Connectivity c,
                             const std::vector<std::int64_t>& keys) {
    const geo::Components got = geo::label_components(g, mask, c, keys);
    const RefComponents ref = reference(g, mask, c, keys);
    CHECK(got.count == ref.count);
    CHECK(got.label == ref.label);
    CHECK(got.parent == ref.parent);
    CHECK(got.order == ref.order);
    CHECK(got.start == ref.start);
    for (std::uint32_t k = 1; k <= got.count && k <= ref.count; ++k) {
        CHECK(got.size(k) == ref.start[k] - ref.start[k - 1U]);
    }
}

}  // namespace

TEST_CASE("G2: labels follow each component's minimum index") {
    // Component A (top-right) has the smallest index; B starts lower but
    // reaches further up-left later in index order than A's first cell.
    const std::vector<std::string> rows = {
        "..##.",
        "#....",
        "#.#..",
        "###.#",
    };
    const Grid g = grid_of(rows);
    const auto mask = mask_of(rows);
    const geo::Components c = geo::label_components(g, mask, Connectivity::four);
    REQUIRE(c.count == 3U);
    // (2,0),(3,0) first -> 1; (0,1),(0,2),(0,3),(1,3),(2,3),(2,2) -> 2; (4,3) -> 3.
    CHECK(c.label[g.index(2, 0)] == 1U);
    CHECK(c.label[g.index(3, 0)] == 1U);
    CHECK(c.label[g.index(0, 1)] == 2U);
    CHECK(c.label[g.index(2, 2)] == 2U);
    CHECK(c.label[g.index(2, 3)] == 2U);
    CHECK(c.label[g.index(4, 3)] == 3U);
    CHECK(c.label[g.index(1, 1)] == 0U);
    CHECK(c.size(1) == 2U);
    CHECK(c.size(2) == 6U);
    CHECK(c.size(3) == 1U);
    CHECK(c.start == std::vector<std::uint32_t>{0U, 2U, 8U, 9U});
    CHECK_FATAL(c.size(0));
    CHECK_FATAL(c.size(4));
    // With 8-connectivity (4,3) is still alone; (2,2) touches nothing new.
    const geo::Components c8 = geo::label_components(g, mask, Connectivity::eight);
    CHECK(c8.count == 3U);
    CHECK(c8.label == c.label);
}

TEST_CASE("G2: diagonal touching joins under 8-connectivity only") {
    const std::vector<std::string> rows = {
        "#..",
        ".#.",
        "..#",
    };
    const Grid g = grid_of(rows);
    const auto mask = mask_of(rows);
    const geo::Components c4 = geo::label_components(g, mask, Connectivity::four);
    CHECK(c4.count == 3U);
    CHECK(c4.label == std::vector<std::uint32_t>{1, 0, 0, 0, 2, 0, 0, 0, 3});
    const geo::Components c8 = geo::label_components(g, mask, Connectivity::eight);
    CHECK(c8.count == 1U);
    CHECK(c8.label == std::vector<std::uint32_t>{1, 0, 0, 0, 1, 0, 0, 0, 1});
    CHECK(c8.order == std::vector<geo::CellIndex>{0, 4, 8});
    // (1,1) reached from (0,0) through SE (1): parent NW (5); likewise (2,2).
    CHECK(c8.parent == std::vector<geo::Direction>{8, 8, 8, 8, 5, 8, 8, 8, 5});
}

TEST_CASE("G2: the standard pair: a diagonal pair of land cells separates two ocean cells") {
    // Land 8-connected, ocean 4-connected (P1-00 2.3).
    const std::vector<std::string> land = {
        "#.",
        ".#",
    };
    const std::vector<std::string> ocean = {
        ".#",
        "#.",
    };
    CHECK(geo::label_components(grid_of(land), mask_of(land), Connectivity::eight).count == 1U);
    CHECK(geo::label_components(grid_of(ocean), mask_of(ocean), Connectivity::four).count == 2U);
}

TEST_CASE("G2: a ring and nested regions") {
    // An outer ring, an empty moat, an inner ring, and a centre cell.
    const std::vector<std::string> rows = {
        "#######",
        "#.....#",
        "#.###.#",
        "#.#.#.#",
        "#.###.#",
        "#.....#",
        "#######",
    };
    const Grid g = grid_of(rows);
    const auto mask = mask_of(rows);
    for (const Connectivity conn : {Connectivity::four, Connectivity::eight}) {
        const geo::Components c = geo::label_components(g, mask, conn);
        CHECK(c.count == 2U);
        CHECK(c.label[g.index(0, 0)] == 1U);
        CHECK(c.label[g.index(6, 6)] == 1U);
        CHECK(c.label[g.index(2, 2)] == 2U);
        CHECK(c.label[g.index(4, 4)] == 2U);
        CHECK(c.label[g.index(3, 3)] == 0U);
        CHECK(c.size(1) == 24U);
        CHECK(c.size(2) == 8U);
    }
    // The complement: the moat is one ring, the centre a separate region
    // under both connectivities (it touches the inner ring only).
    std::vector<std::uint8_t> inverse(mask.size());
    for (std::size_t i = 0; i < mask.size(); ++i) {
        inverse[i] = mask[i] == 0U ? std::uint8_t{1} : std::uint8_t{0};
    }
    for (const Connectivity conn : {Connectivity::four, Connectivity::eight}) {
        const geo::Components c = geo::label_components(g, inverse, conn);
        CHECK(c.count == 2U);
        CHECK(c.label[g.index(1, 1)] == 1U);
        CHECK(c.label[g.index(3, 3)] == 2U);
        CHECK(c.size(1) == 16U);
        CHECK(c.size(2) == 1U);
    }
    // A diagonal-only gap in a ring: 4-connected it is a path, 8-connected the
    // two diagonal corners close it; the hole is still one region either way.
    const std::vector<std::string> broken = {
        ".###",
        "#..#",
        "#..#",
        "####",
    };
    CHECK(geo::label_components(grid_of(broken), mask_of(broken), Connectivity::four).count == 1U);
    CHECK(geo::label_components(grid_of(broken), mask_of(broken), Connectivity::eight).count == 1U);
}

TEST_CASE("G2: the key equivalence joins adjacent cells only when their keys are equal") {
    // One row of P-cells with keys 1 1 2 2 1: three components.
    const Grid g{5, 1};
    const std::vector<std::uint8_t> mask(5, 1U);
    const std::vector<std::int64_t> keys = {1, 1, 2, 2, 1};
    const geo::Components c = geo::label_components(g, mask, Connectivity::eight, keys);
    CHECK(c.count == 3U);
    CHECK(c.label == std::vector<std::uint32_t>{1, 1, 2, 2, 3});
    // Without keys: one component.
    CHECK(geo::label_components(g, mask, Connectivity::eight).count == 1U);
    // Diagonal neighbours with equal keys join under 8-connectivity even if
    // the orthogonal cells between them have other keys.
    const Grid g2{2, 2};
    const std::vector<std::uint8_t> all(4, 1U);
    const std::vector<std::int64_t> diag = {7, 9, 9, 7};
    const geo::Components d8 = geo::label_components(g2, all, Connectivity::eight, diag);
    CHECK(d8.count == 2U);
    CHECK(d8.label == std::vector<std::uint32_t>{1, 2, 2, 1});
    const geo::Components d4 = geo::label_components(g2, all, Connectivity::four, diag);
    CHECK(d4.count == 4U);
    // Keys on cells outside P do not matter; huge keys are compared exactly.
    const std::vector<std::int64_t> big = {worldgen_test::I64_MAX, worldgen_test::I64_MAX - 1, worldgen_test::I64_MAX,
                                           worldgen_test::I64_MAX};
    const geo::Components b = geo::label_components(g2, all, Connectivity::four, big);
    CHECK(b.label == std::vector<std::uint32_t>{1, 2, 1, 1});
    // A wrong-sized key array is fatal; an empty one means "always equal".
    const std::vector<std::int64_t> short_keys = {1, 2, 3};
    CHECK_FATAL(geo::label_components(g2, all, Connectivity::four, short_keys));
}

TEST_CASE("G2: BFS visit order and parents on a full 3 x 3 grid (hand-checked)") {
    const Grid g{3, 3};
    const std::vector<std::uint8_t> mask(9, 1U);
    // 8-connected from 0: E 1, SE 4, S 3; from 1: E 2, SE 5; from 4: SE 8, S 7, SW 6.
    const geo::Components c8 = geo::label_components(g, mask, Connectivity::eight);
    CHECK(c8.count == 1U);
    CHECK(c8.order == std::vector<geo::CellIndex>{0, 1, 4, 3, 2, 5, 8, 7, 6});
    CHECK(c8.parent == std::vector<geo::Direction>{8, 4, 4, 6, 5, 5, 7, 6, 5});
    // 4-connected from 0: E 1, S 3; from 1: E 2, S 4; from 3: S 6; from 2: S 5;
    // from 4: S 7; from 5: S 8.
    const geo::Components c4 = geo::label_components(g, mask, Connectivity::four);
    CHECK(c4.order == std::vector<geo::CellIndex>{0, 1, 3, 2, 4, 6, 5, 7, 8});
    CHECK(c4.parent == std::vector<geo::Direction>{8, 4, 4, 6, 6, 6, 6, 6, 6});
    CHECK(c4.start == std::vector<std::uint32_t>{0U, 9U});
}

TEST_CASE("G2: an empty predicate and a full one") {
    const Grid g{4, 3};
    const geo::Components none = geo::label_components(g, std::vector<std::uint8_t>(12, 0U), Connectivity::four);
    CHECK(none.count == 0U);
    CHECK(none.order.empty());
    CHECK(none.label == std::vector<std::uint32_t>(12, 0U));
    CHECK(none.parent == std::vector<geo::Direction>(12, geo::NO_DIRECTION));
    CHECK(none.start == std::vector<std::uint32_t>{0U});
    // Any non-zero mask byte is "in P".
    const geo::Components all = geo::label_components(g, std::vector<std::uint8_t>(12, 0xFFU), Connectivity::four);
    CHECK(all.count == 1U);
    CHECK(all.size(1) == 12U);
}

TEST_CASE("G2: fatal preconditions") {
    const std::vector<std::uint8_t> mask(6, 1U);
    CHECK_FATAL(geo::label_components(Grid{3, 3}, mask, Connectivity::four));
    CHECK_FATAL(geo::label_components(Grid{0, 6}, std::vector<std::uint8_t>{}, Connectivity::four));
    CHECK_FATAL(geo::label_components(Grid{-2, -3}, mask, Connectivity::four));
}

TEST_CASE("G2: agreement with the reference flood on seeded random masks") {
    worldgen::Rng rng(KEY_G2);
    for (int trial = 0; trial < 160; ++trial) {
        const auto w = static_cast<std::int32_t>(1U + rng.uniform_u64(40U));
        const auto h = static_cast<std::int32_t>(1U + rng.uniform_u64(40U));
        const Grid g{w, h};
        const std::uint64_t density = 1U + rng.uniform_u64(9U);  // in tenths
        std::vector<std::uint8_t> mask(g.cells());
        for (auto& m : mask) {
            m = rng.uniform_u64(10U) < density ? std::uint8_t{1} : std::uint8_t{0};
        }
        std::vector<std::int64_t> keys;
        if (trial % 2 == 1) {
            keys.resize(g.cells());
            const std::uint64_t alphabet = 1U + rng.uniform_u64(4U);
            for (auto& k : keys) {
                k = static_cast<std::int64_t>(rng.uniform_u64(alphabet));
            }
        }
        const Connectivity conn = (trial % 4 < 2) ? Connectivity::four : Connectivity::eight;
        CAPTURE(trial);
        CAPTURE(w);
        CAPTURE(h);
        check_against_reference(g, mask, conn, keys);
        check_partition(g, mask, conn, keys, geo::label_components(g, mask, conn, keys));
    }
}

// --- bfs_region (P1-00 7.2; P1-02 H4 step 2) -------------------------------------

TEST_CASE("G2 bfs_region: the P1-02 5.1 lake searched from its outlet") {
    // The lake cells (2,2), (3,2), (2,3), (3,3) of the 8 x 7 worked example,
    // outlet (2,3) = index 26. P1-02 H4: code 0 reaches (3,3) (receiver W, 4),
    // code 6 reaches (2,2) (receiver S, 2), code 7 reaches (3,2) (receiver SW, 3).
    const Grid g{8, 7};
    std::vector<std::uint32_t> region(g.cells(), 0U);
    for (const geo::CellIndex i : {18U, 19U, 26U, 27U}) {
        region[i] = 1U;
    }
    std::vector<geo::Direction> parent(g.cells(), 99U);
    std::vector<std::uint8_t> visited(g.cells(), 0U);
    std::vector<geo::CellIndex> order;
    geo::bfs_region(g, region, 26U, Connectivity::eight, parent, visited, order);
    CHECK(order == std::vector<geo::CellIndex>{26, 27, 18, 19});
    CHECK(parent[27] == 4U);
    CHECK(parent[18] == 2U);
    CHECK(parent[19] == 3U);
    CHECK(parent[26] == 99U);  // the seed's parent is not changed
    for (geo::CellIndex i = 0; i < g.cells(); ++i) {
        const bool lake = region[i] == 1U;
        CHECK(visited[i] == (lake ? 1U : 0U));
        if (!lake) {
            CHECK(parent[i] == 99U);
        }
    }
}

TEST_CASE("G2 bfs_region: from a component's minimum cell it reproduces label_components") {
    worldgen::Rng rng(KEY_G2 + 7U);
    for (int trial = 0; trial < 40; ++trial) {
        const Grid g{static_cast<std::int32_t>(2U + rng.uniform_u64(25U)),
                     static_cast<std::int32_t>(2U + rng.uniform_u64(25U))};
        std::vector<std::uint8_t> mask(g.cells());
        for (auto& m : mask) {
            m = rng.uniform_u64(3U) != 0U ? std::uint8_t{1} : std::uint8_t{0};
        }
        const Connectivity conn = trial % 2 == 0 ? Connectivity::four : Connectivity::eight;
        const geo::Components comp = geo::label_components(g, mask, conn);
        std::vector<geo::Direction> parent(g.cells(), geo::NO_DIRECTION);
        std::vector<std::uint8_t> visited(g.cells(), 0U);
        std::vector<geo::CellIndex> order;
        for (std::uint32_t k = 1; k <= comp.count; ++k) {
            const geo::CellIndex seed = comp.order[comp.start[k - 1U]];
            geo::bfs_region(g, comp.label, seed, conn, parent, visited, order);
        }
        CAPTURE(trial);
        CHECK(order == comp.order);
        CHECK(parent == comp.parent);
    }
}

TEST_CASE("G2 bfs_region: appends to order, respects visited and region, 4-connected") {
    // Region values: two regions with value 5 and 6; label 0 cells are a
    // region too if the seed is in one.
    const Grid g{4, 2};
    const std::vector<std::uint32_t> region = {5, 5, 6, 5,  //
                                               5, 6, 6, 5};
    std::vector<geo::Direction> parent(8, geo::NO_DIRECTION);
    std::vector<std::uint8_t> visited(8, 0U);
    std::vector<geo::CellIndex> order = {77U};  // existing content is kept
    geo::bfs_region(g, region, 0U, Connectivity::four, parent, visited, order);
    CHECK(order == std::vector<geo::CellIndex>{77U, 0U, 1U, 4U});
    // (3,0) and (3,1) have value 5 but are not 4-connected to the seed's set.
    CHECK(visited == std::vector<std::uint8_t>{1, 1, 0, 0, 1, 0, 0, 0});
    CHECK(parent[1] == 4U);
    CHECK(parent[4] == 6U);
    // Searching again from an already-visited seed is fatal.
    CHECK_FATAL(geo::bfs_region(g, region, 1U, Connectivity::four, parent, visited, order));
    // A seed outside the grid, and wrong array sizes, are fatal.
    std::vector<std::uint8_t> fresh(8, 0U);
    CHECK_FATAL(geo::bfs_region(g, region, 8U, Connectivity::four, parent, fresh, order));
    std::vector<std::uint8_t> short_visited(7, 0U);
    CHECK_FATAL(geo::bfs_region(g, region, 3U, Connectivity::four, parent, short_visited, order));
    std::vector<geo::Direction> short_parent(7, 0U);
    CHECK_FATAL(geo::bfs_region(g, region, 3U, Connectivity::four, short_parent, fresh, order));
    const std::vector<std::uint32_t> short_region(7, 5U);
    CHECK_FATAL(geo::bfs_region(g, short_region, 3U, Connectivity::four, parent, fresh, order));
}
