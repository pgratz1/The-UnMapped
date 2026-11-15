// G3 routing flood, G4 receivers and G5 accumulation (spec P1-00 sections
// 7.3 to 7.5, rules D1, D4, D6, D10), with the worked example of P1-02
// section 5.1 and the shared-algorithm unit tests of P1-02 8.1 (items 1 to 4).
//
// Written from docs/specs/p1-00-geography-overview.md,
// docs/specs/p1-02-hydrology.md and the public header worldgen/geo/flow.hpp
// only. The references here are independent of any queue structure:
//   - G3: the pseudocode of 7.3 with Q an ordered set of (key, seq, cell),
//     popping its smallest element, so the pop order is literally
//     "ascending (key, seq)";
//   - flood levels: a brute-force minimax relaxation to a fixed point;
//   - G4: the definition of 7.4 with scores in U128;
//   - G5: walking every cell's downstream path.

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

#include "core_test_util.hpp"
#include "worldgen/core/core.hpp"
#include "worldgen/geo/components.hpp"
#include "worldgen/geo/flow.hpp"
#include "worldgen/geo/grid.hpp"

namespace geo = worldgen::geo;
using geo::CellIndex;
using geo::Direction;
using geo::Grid;
using geo::NO_DIRECTION;

namespace {

constexpr std::uint64_t KEY_FLOW = 0x5445535446303331ULL;

// --- The worked example of P1-02 5.1 --------------------------------------------

const Grid EX{8, 7};

const std::vector<std::int64_t>& ex_elevation() {
    static const std::vector<std::int64_t> e = {
        -5, -5, -5, -5, -5, -5, -5, -5,  //
        -5, 60, 70, 80, 70, 60, 50, -5,  //
        -5, 50, 20, 22, 60, 40, 30, -5,  //
        -5, 45, 21, 23, 55, 35, 25, -5,  //
        -5, 40, 30, 28, 40, 30, 20, -5,  //
        -5, 35, 33, 31, 25, 20, 10, -5,  //
        -5, -5, -5, -5, -5, -5, -5, -5,  //
    };
    return e;
}

std::vector<std::uint8_t> ocean_of(const std::vector<std::int64_t>& e) {
    std::vector<std::uint8_t> o(e.size());
    for (std::size_t i = 0; i < e.size(); ++i) {
        o[i] = e[i] < 0 ? std::uint8_t{1} : std::uint8_t{0};
    }
    return o;
}

const std::vector<std::int64_t>& ex_level() {
    static const std::vector<std::int64_t> l = {
        0, 0,  0,  0,  0,  0,  0,  0,  //
        0, 60, 70, 80, 70, 60, 50, 0,  //
        0, 50, 28, 28, 60, 40, 30, 0,  //
        0, 45, 28, 28, 55, 35, 25, 0,  //
        0, 40, 30, 28, 40, 30, 20, 0,  //
        0, 35, 33, 31, 25, 20, 10, 0,  //
        0, 0,  0,  0,  0,  0,  0,  0,  //
    };
    return l;
}

const std::vector<Direction>& ex_parent() {
    static const std::vector<Direction> p = {
        8, 8, 8, 8, 8, 8, 8, 8,  //
        8, 5, 5, 5, 5, 5, 5, 8,  //
        8, 5, 2, 3, 3, 1, 7, 8,  //
        8, 5, 1, 2, 3, 1, 7, 8,  //
        8, 5, 0, 1, 1, 1, 7, 8,  //
        8, 5, 3, 3, 3, 3, 7, 8,  //
        8, 8, 8, 8, 8, 8, 8, 8,  //
    };
    return p;
}

const std::vector<CellIndex>& ex_order() {
    static const std::vector<CellIndex> o = {0,  1,  2,  3,  4,  5,  6,  7,  8,  15, 16, 23, 24, 31,
                                             32, 39, 40, 47, 48, 49, 50, 51, 52, 53, 54, 55, 46, 38,
                                             45, 30, 44, 35, 26, 27, 18, 19, 22, 37, 34, 43, 42, 41,
                                             29, 33, 36, 21, 25, 14, 17, 28, 9,  13, 20, 10, 12, 11};
    return o;
}

// Final flow_dir of 5.1 (after the lake rerouting of H4 step 2).
const std::vector<Direction>& ex_flow_dir() {
    static const std::vector<Direction> f = {
        8, 8, 8, 8, 8, 8, 8, 8,  //
        8, 4, 6, 6, 6, 6, 0, 8,  //
        8, 4, 2, 3, 4, 1, 0, 8,  //
        8, 4, 1, 4, 4, 1, 0, 8,  //
        8, 4, 0, 1, 2, 1, 0, 8,  //
        8, 2, 2, 2, 2, 2, 0, 8,  //
        8, 8, 8, 8, 8, 8, 8, 8,  //
    };
    return f;
}

// G4 alone, before the rerouting: the lake cells keep their flood parents,
// so (3, 3) points S (2) to the rim (3, 4) instead of W (4) (P1-02 5.1 H4).
std::vector<Direction> ex_receivers_before_rerouting() {
    std::vector<Direction> r = ex_flow_dir();
    r[EX.index(3, 3)] = 2;
    return r;
}

const std::vector<std::uint32_t>& ex_flow_accum() {
    static const std::vector<std::uint32_t> a = {
        0, 0, 0, 0, 0,  0, 0, 0,  //
        0, 1, 1, 1, 1,  1, 1, 0,  //
        0, 1, 1, 2, 1,  1, 1, 0,  //
        0, 1, 6, 2, 1,  1, 2, 0,  //
        0, 1, 1, 8, 1,  1, 2, 0,  //
        0, 1, 1, 1, 10, 1, 2, 0,  //
        0, 0, 0, 0, 0,  0, 0, 0,  //
    };
    return a;
}

// --- References -------------------------------------------------------------------

struct RefFlood {
    std::vector<std::int64_t> level;
    std::vector<Direction> parent;
    std::vector<CellIndex> order;
    std::vector<std::int64_t> pop_key;  // the key of each pop, in pop order
};

// P1-00 7.3 literally, Q an ordered set of (key, seq, cell).
RefFlood reference_flood(Grid g, const std::vector<std::int64_t>& h, const std::vector<std::uint8_t>& ocean) {
    RefFlood r;
    const std::size_t n = h.size();
    r.level.assign(n, -1);
    r.parent.assign(n, NO_DIRECTION);
    std::vector<bool> visited(n, false);
    std::set<std::tuple<std::int64_t, std::uint64_t, CellIndex>> q;
    std::uint64_t seq = 0;
    for (CellIndex i = 0; i < n; ++i) {
        if (ocean[i] != 0U) {
            r.level[i] = 0;
            r.parent[i] = NO_DIRECTION;
            visited[i] = true;
            q.emplace(0, seq, i);
            ++seq;
        }
    }
    while (!q.empty()) {
        const auto [k, s, c] = *q.begin();
        q.erase(q.begin());
        static_cast<void>(s);
        r.order.push_back(c);
        r.pop_key.push_back(k);
        for (Direction code = 0; code < 8; ++code) {
            CellIndex m = 0;
            if (!geo::neighbour(g, c, code, m) || visited[m]) {
                continue;
            }
            visited[m] = true;
            r.level[m] = h[m] > r.level[c] ? h[m] : r.level[c];
            r.parent[m] = geo::opposite(code);
            q.emplace(r.level[m], seq, m);
            ++seq;
        }
    }
    return r;
}

// The minimax spill level by relaxation to a fixed point:
// L = 0 on ocean; L[i] = max(h[i], min over neighbours L[m]) elsewhere.
std::vector<std::int64_t> minimax_levels(Grid g, const std::vector<std::int64_t>& h,
                                         const std::vector<std::uint8_t>& ocean) {
    const std::int64_t INF = worldgen_test::I64_MAX;
    std::vector<std::int64_t> l(h.size(), INF);
    for (std::size_t i = 0; i < h.size(); ++i) {
        if (ocean[i] != 0U) {
            l[i] = 0;
        }
    }
    bool changed = true;
    int sweep = 0;
    while (changed) {
        changed = false;
        for (std::size_t step = 0; step < h.size(); ++step) {
            const auto i = static_cast<CellIndex>(sweep % 2 == 0 ? step : h.size() - 1U - step);
            if (ocean[i] != 0U) {
                continue;
            }
            std::int64_t best = INF;
            for (Direction code = 0; code < 8; ++code) {
                CellIndex m = 0;
                if (geo::neighbour(g, i, code, m) && l[m] < best) {
                    best = l[m];
                }
            }
            const std::int64_t v = best == INF ? INF : (h[i] > best ? h[i] : best);
            if (v < l[i]) {
                l[i] = v;
                changed = true;
            }
        }
        ++sweep;
    }
    return l;
}

// P1-00 7.4 from the definition.
std::vector<Direction> reference_receivers(Grid g, const std::vector<std::int64_t>& level,
                                           const std::vector<Direction>& parent,
                                           const std::vector<std::uint8_t>& ocean) {
    std::vector<Direction> rcv(level.size(), NO_DIRECTION);
    for (CellIndex c = 0; c < level.size(); ++c) {
        if (ocean[c] != 0U) {
            continue;
        }
        bool found = false;
        worldgen::U128 best{0, 0};
        Direction best_code = NO_DIRECTION;
        for (Direction code = 0; code < 8; ++code) {
            CellIndex m = 0;
            if (!geo::neighbour(g, c, code, m) || level[m] >= level[c]) {
                continue;
            }
            const auto d = static_cast<std::uint64_t>(level[c] - level[m]);
            worldgen::U128 score = worldgen::mul_u64_wide(d, d);
            if (code % 2U == 0U) {
                score = score << 1U;
            }
            if (!found || score > best) {
                found = true;
                best = score;
                best_code = code;
            }
        }
        rcv[c] = found ? best_code : parent[c];
    }
    return rcv;
}

// The downstream cell of c, or c itself if rcv[c] is not a code to a cell
// inside the grid.
CellIndex downstream(Grid g, CellIndex c, Direction code) {
    CellIndex m = c;
    if (code < 8U && geo::neighbour(g, c, code, m)) {
        return m;
    }
    return c;
}

// Path walking: A[c] = number of non-ocean cells whose downstream path
// (c included) passes through c. Also checks that every path reaches the
// ocean in fewer than n steps (no cycles), returning false otherwise.
bool path_walk_accumulation(Grid g, const std::vector<Direction>& rcv, const std::vector<std::uint8_t>& ocean,
                            std::vector<std::uint32_t>& a) {
    const std::size_t n = rcv.size();
    a.assign(n, 0U);
    for (CellIndex s = 0; s < n; ++s) {
        if (ocean[s] != 0U) {
            continue;
        }
        CellIndex c = s;
        std::size_t steps = 0;
        while (ocean[c] == 0U) {
            a[c] += 1U;
            const CellIndex m = downstream(g, c, rcv[c]);
            if (m == c || ++steps > n) {
                return false;
            }
            c = m;
        }
    }
    return true;
}

// Random keys for G3: `ocean_share` in percent, at least one ocean cell;
// land keys from `pick`.
template <typename Pick>
void random_terrain(worldgen::Rng& rng, Grid g, std::uint64_t ocean_share, Pick pick, std::vector<std::int64_t>& h,
                    std::vector<std::uint8_t>& ocean) {
    h.assign(g.cells(), 0);
    ocean.assign(g.cells(), 0U);
    for (CellIndex i = 0; i < g.cells(); ++i) {
        if (rng.uniform_u64(100U) < ocean_share) {
            ocean[i] = 1U;
            h[i] = -1 - static_cast<std::int64_t>(rng.uniform_u64(50U));
        } else {
            h[i] = pick(rng);
        }
    }
    const auto forced = static_cast<CellIndex>(rng.uniform_u64(g.cells()));
    ocean[forced] = 1U;
    h[forced] = -1;
}

// The P1-02 8.2 random fixture: ocean border of -1 to -50, interior keys
// from `pick`.
template <typename Pick>
void bordered_terrain(worldgen::Rng& rng, Grid g, Pick pick, std::vector<std::int64_t>& h,
                      std::vector<std::uint8_t>& ocean) {
    h.assign(g.cells(), 0);
    ocean.assign(g.cells(), 0U);
    for (CellIndex i = 0; i < g.cells(); ++i) {
        const std::int32_t x = g.x_of(i);
        const std::int32_t y = g.y_of(i);
        if (x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1) {
            ocean[i] = 1U;
            h[i] = -1 - static_cast<std::int64_t>(rng.uniform_u64(50U));
        } else {
            h[i] = pick(rng);
        }
    }
}

// Every G3 check of P1-00 7.3 against the references.
void check_flood(Grid g, const std::vector<std::int64_t>& h, const std::vector<std::uint8_t>& ocean) {
    const geo::RoutingFlood f = geo::routing_flood(g, h, ocean);
    const RefFlood ref = reference_flood(g, h, ocean);
    REQUIRE(f.level.size() == g.cells());
    REQUIRE(f.parent.size() == g.cells());
    REQUIRE(f.order.size() == g.cells());
    // Exact pop order of ascending (key, seq), levels and parents.
    CHECK(f.order == ref.order);
    CHECK(f.level == ref.level);
    CHECK(f.parent == ref.parent);
    // L is the minimax spill level, by an independent relaxation.
    CHECK(f.level == minimax_levels(g, h, ocean));
    // Monotone pops; every cell exactly once; parent and every strictly
    // lower neighbour popped before the cell; L >= h on land.
    std::vector<std::size_t> pos(g.cells(), g.cells());
    for (std::size_t p = 0; p < f.order.size(); ++p) {
        REQUIRE(f.order[p] < g.cells());
        CHECK(pos[f.order[p]] == g.cells());
        pos[f.order[p]] = p;
    }
    for (std::size_t p = 1; p < f.order.size(); ++p) {
        CHECK(f.level[f.order[p - 1U]] <= f.level[f.order[p]]);
    }
    for (CellIndex c = 0; c < g.cells(); ++c) {
        if (ocean[c] != 0U) {
            CHECK(f.level[c] == 0);
            CHECK(f.parent[c] == NO_DIRECTION);
            continue;
        }
        CHECK(f.level[c] >= h[c]);
        REQUIRE(f.parent[c] < 8U);
        CellIndex p = 0;
        REQUIRE(geo::neighbour(g, c, f.parent[c], p));
        CHECK(pos[p] < pos[c]);
        CHECK(f.level[c] == (h[c] > f.level[p] ? h[c] : f.level[p]));
        for (Direction code = 0; code < 8; ++code) {
            CellIndex m = 0;
            if (geo::neighbour(g, c, code, m) && f.level[m] < f.level[c]) {
                CHECK(pos[m] < pos[c]);
            }
        }
    }
    // Ocean cells first, in index order.
    std::vector<CellIndex> oceans;
    for (CellIndex i = 0; i < g.cells(); ++i) {
        if (ocean[i] != 0U) {
            oceans.push_back(i);
        }
    }
    CHECK(std::vector<CellIndex>(f.order.begin(), f.order.begin() + static_cast<std::ptrdiff_t>(oceans.size())) ==
          oceans);
}

// Every G4 check of P1-00 7.4: the definition, no cycles, the ocean is
// reached, and `order` lists every cell after its receiver.
void check_receivers(Grid g, const geo::RoutingFlood& f, const std::vector<std::uint8_t>& ocean,
                     const std::vector<Direction>& rcv) {
    CHECK(rcv == reference_receivers(g, f.level, f.parent, ocean));
    std::vector<std::uint32_t> walked;
    CHECK(path_walk_accumulation(g, rcv, ocean, walked));
    std::vector<std::size_t> pos(g.cells(), 0U);
    for (std::size_t p = 0; p < f.order.size(); ++p) {
        pos[f.order[p]] = p;
    }
    for (CellIndex c = 0; c < g.cells(); ++c) {
        if (ocean[c] != 0U) {
            CHECK(rcv[c] == NO_DIRECTION);
            continue;
        }
        REQUIRE(rcv[c] < 8U);
        CellIndex m = 0;
        REQUIRE(geo::neighbour(g, c, rcv[c], m));
        CHECK(pos[m] < pos[c]);
        CHECK(f.level[m] <= f.level[c]);
    }
}

// A 5 x 5 grid with an ocean border (level 0) around a ring of eight
// cells at the given levels (by D8 code from the centre) and a centre at
// level `centre`; returns G4's receiver of the centre. Parents point at
// the ocean border (outwards) for the ring and E for the centre, all valid.
Direction centre_receiver(std::int64_t centre, const std::int64_t (&ring)[8], Direction centre_parent = 0,
                          std::uint32_t threads = 1U) {
    const Grid g{5, 5};
    std::vector<std::int64_t> level(25, 0);
    std::vector<Direction> parent(25, NO_DIRECTION);
    std::vector<std::uint8_t> ocean(25, 1U);
    const CellIndex c = g.index(2, 2);
    level[c] = centre;
    parent[c] = centre_parent;
    ocean[c] = 0U;
    for (Direction k = 0; k < 8; ++k) {
        const CellIndex m = g.index(2 + geo::D8_DX[k], 2 + geo::D8_DY[k]);
        level[m] = ring[k];
        parent[m] = k;  // continuing outwards: a border cell
        ocean[m] = 0U;
    }
    const std::vector<Direction> rcv = geo::receivers(g, level, parent, ocean, threads);
    return rcv[c];
}

}  // namespace

// --- G3 ---------------------------------------------------------------------------

TEST_CASE("G3 worked example (P1-02 5.1): flood levels, parents and the complete pop order") {
    const auto& e = ex_elevation();
    const auto ocean = ocean_of(e);
    const geo::RoutingFlood f = geo::routing_flood(EX, e, ocean);
    CHECK(f.level == ex_level());
    CHECK(f.parent == ex_parent());
    CHECK(f.order == ex_order());
    // The reference agrees with the spec's values too (a check of the
    // reference and of the worked example).
    const RefFlood ref = reference_flood(EX, e, ocean);
    CHECK(ref.order == ex_order());
    CHECK(ref.level == ex_level());
    CHECK(ref.parent == ex_parent());
    CHECK(minimax_levels(EX, e, ocean) == ex_level());
}

TEST_CASE("G3 properties on the worked example (P1-02 8.1 item 2)") {
    const auto& e = ex_elevation();
    check_flood(EX, e, ocean_of(e));
}

TEST_CASE("G3: the scratch overload gives the same result, also when reused across grids") {
    geo::RoutingFloodScratch scratch;
    geo::RoutingFlood out;
    worldgen::Rng rng(KEY_FLOW + 1U);
    for (int trial = 0; trial < 12; ++trial) {
        const Grid g{static_cast<std::int32_t>(1U + rng.uniform_u64(30U)),
                     static_cast<std::int32_t>(1U + rng.uniform_u64(30U))};
        std::vector<std::int64_t> h;
        std::vector<std::uint8_t> ocean;
        const std::int64_t span = trial % 2 == 0 ? 5 : (std::int64_t{1} << 42);
        random_terrain(rng, g, 10U, [span](worldgen::Rng& r) { return 1 + r.uniform_i64(0, span); }, h, ocean);
        geo::routing_flood(g, h, ocean, out, scratch);
        const geo::RoutingFlood fresh = geo::routing_flood(g, h, ocean);
        CAPTURE(trial);
        CHECK(out.level == fresh.level);
        CHECK(out.parent == fresh.parent);
        CHECK(out.order == fresh.order);
    }
    // The worked example through a used scratch.
    const auto& e = ex_elevation();
    geo::routing_flood(EX, e, ocean_of(e), out, scratch);
    CHECK(out.order == ex_order());
    // Move construction and assignment keep a usable scratch.
    geo::RoutingFloodScratch moved(std::move(scratch));
    geo::routing_flood(EX, e, ocean_of(e), out, moved);
    CHECK(out.level == ex_level());
    geo::RoutingFloodScratch assigned;
    assigned = std::move(moved);
    geo::routing_flood(EX, e, ocean_of(e), out, assigned);
    CHECK(out.parent == ex_parent());
}

TEST_CASE("G3 against the (key, seq) reference: small keys with heavy ties") {
    worldgen::Rng rng(KEY_FLOW + 2U);
    for (int trial = 0; trial < 60; ++trial) {
        const Grid g{static_cast<std::int32_t>(1U + rng.uniform_u64(24U)),
                     static_cast<std::int32_t>(1U + rng.uniform_u64(24U))};
        const std::int64_t top = 1 + rng.uniform_i64(0, 4);
        std::vector<std::int64_t> h;
        std::vector<std::uint8_t> ocean;
        random_terrain(rng, g, 1U + rng.uniform_u64(30U), [top](worldgen::Rng& r) { return r.uniform_i64(1, top); }, h,
                       ocean);
        CAPTURE(trial);
        check_flood(g, h, ocean);
    }
}

TEST_CASE("G3 against the (key, seq) reference: the P1-02 8.2 random fixture (32 x 32 interior, keys 1 to 200)") {
    worldgen::Rng rng(KEY_FLOW + 3U);
    for (int trial = 0; trial < 20; ++trial) {
        const Grid g{34, 34};
        std::vector<std::int64_t> h;
        std::vector<std::uint8_t> ocean;
        bordered_terrain(rng, g, [](worldgen::Rng& r) { return r.uniform_i64(1, 200); }, h, ocean);
        CAPTURE(trial);
        check_flood(g, h, ocean);
    }
}

TEST_CASE("G3 against the (key, seq) reference: huge keys (Fx raw magnitudes 2^40 to 2^44)") {
    worldgen::Rng rng(KEY_FLOW + 4U);
    const std::int64_t lo = std::int64_t{1} << 40U;
    const std::int64_t hi = std::int64_t{1} << 44U;
    for (int trial = 0; trial < 30; ++trial) {
        const Grid g{static_cast<std::int32_t>(2U + rng.uniform_u64(30U)),
                     static_cast<std::int32_t>(2U + rng.uniform_u64(30U))};
        std::vector<std::int64_t> h;
        std::vector<std::uint8_t> ocean;
        if (trial % 3 == 0) {
            // A few huge distinct values, many ties among them.
            std::vector<std::int64_t> palette;
            for (int j = 0; j < 4; ++j) {
                palette.push_back(rng.uniform_i64(lo, hi));
            }
            random_terrain(rng, g, 8U,
                           [&palette](worldgen::Rng& r) {
                               return palette[static_cast<std::size_t>(r.uniform_u64(palette.size()))];
                           },
                           h, ocean);
        } else if (trial % 3 == 1) {
            random_terrain(rng, g, 8U, [&](worldgen::Rng& r) { return r.uniform_i64(lo, hi); }, h, ocean);
        } else {
            // Huge values mixed with tiny ones (1 to 3): keys spanning the
            // whole range in one flood.
            random_terrain(rng, g, 8U,
                           [&](worldgen::Rng& r) {
                               const std::uint64_t coin = r.uniform_u64(2U);
                               return coin == 0U ? r.uniform_i64(1, 3) : r.uniform_i64(lo, hi);
                           },
                           h, ocean);
        }
        CAPTURE(trial);
        check_flood(g, h, ocean);
    }
}

TEST_CASE("G3 against the (key, seq) reference: keys up to the int64 maximum") {
    worldgen::Rng rng(KEY_FLOW + 5U);
    const std::int64_t top = worldgen_test::I64_MAX;
    for (int trial = 0; trial < 10; ++trial) {
        const Grid g{static_cast<std::int32_t>(2U + rng.uniform_u64(20U)),
                     static_cast<std::int32_t>(2U + rng.uniform_u64(20U))};
        std::vector<std::int64_t> h;
        std::vector<std::uint8_t> ocean;
        random_terrain(rng, g, 8U,
                       [&](worldgen::Rng& r) {
                           const std::uint64_t pick = r.uniform_u64(4U);
                           if (pick == 0U) {
                               return top;
                           }
                           if (pick == 1U) {
                               return top - r.uniform_i64(0, 3);
                           }
                           return r.uniform_i64(1, top);
                       },
                       h, ocean);
        CAPTURE(trial);
        check_flood(g, h, ocean);
    }
}

TEST_CASE("G3 against the (key, seq) reference: all-equal keys") {
    for (const std::int64_t k : {std::int64_t{1}, std::int64_t{30000}, std::int64_t{1} << 43U,
                                 worldgen_test::I64_MAX}) {
        CAPTURE(k);
        for (const Grid g : {Grid{1, 1}, Grid{9, 5}, Grid{17, 23}}) {
            std::vector<std::int64_t> h(g.cells(), k);
            std::vector<std::uint8_t> ocean(g.cells(), 0U);
            ocean[g.cells() / 2U] = 1U;  // one ocean cell in the middle
            h[g.cells() / 2U] = -1;
            check_flood(g, h, ocean);
            // Several ocean cells on one edge.
            for (std::int32_t x = 0; x < g.width; x += 2) {
                ocean[g.index(x, g.height - 1)] = 1U;
            }
            check_flood(g, h, ocean);
        }
    }
}

TEST_CASE("G3: every cell ocean, and a single land cell") {
    const Grid g{4, 3};
    std::vector<std::int64_t> h(12, -3);
    std::vector<std::uint8_t> ocean(12, 1U);
    const geo::RoutingFlood all = geo::routing_flood(g, h, ocean);
    CHECK(all.order == std::vector<CellIndex>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
    CHECK(all.level == std::vector<std::int64_t>(12, 0));
    ocean[5] = 0U;
    h[5] = 7;
    const geo::RoutingFlood one = geo::routing_flood(g, h, ocean);
    CHECK(one.level[5] == 7);
    // Cell 5 = (1,1) is first pushed by ocean cell 0 through SE: parent NW.
    CHECK(one.parent[5] == 5U);
    CHECK(one.order.back() == 5U);
}

TEST_CASE("G3: keys of ocean cells are ignored") {
    worldgen::Rng rng(KEY_FLOW + 6U);
    const Grid g{12, 9};
    std::vector<std::int64_t> h;
    std::vector<std::uint8_t> ocean;
    random_terrain(rng, g, 20U, [](worldgen::Rng& r) { return r.uniform_i64(1, 9); }, h, ocean);
    const geo::RoutingFlood a = geo::routing_flood(g, h, ocean);
    for (CellIndex i = 0; i < g.cells(); ++i) {
        if (ocean[i] != 0U) {
            h[i] = (i % 2U == 0U) ? worldgen_test::I64_MIN : worldgen_test::I64_MAX;
        }
    }
    const geo::RoutingFlood b = geo::routing_flood(g, h, ocean);
    CHECK(a.order == b.order);
    CHECK(a.level == b.level);
    CHECK(a.parent == b.parent);
}

TEST_CASE("G3: fatal preconditions") {
    const Grid g{3, 2};
    std::vector<std::int64_t> h = {-1, 5, 5, 5, 5, 5};
    std::vector<std::uint8_t> ocean = {1, 0, 0, 0, 0, 0};
    static_cast<void>(geo::routing_flood(g, h, ocean));  // valid
    // No ocean cell.
    CHECK_FATAL(geo::routing_flood(g, h, std::vector<std::uint8_t>(6, 0U)));
    // A non-ocean key below 1.
    std::vector<std::int64_t> zero = h;
    zero[4] = 0;
    CHECK_FATAL(geo::routing_flood(g, zero, ocean));
    std::vector<std::int64_t> negative = h;
    negative[2] = -7;
    CHECK_FATAL(geo::routing_flood(g, negative, ocean));
    std::vector<std::int64_t> minimum = h;
    minimum[2] = worldgen_test::I64_MIN;
    CHECK_FATAL(geo::routing_flood(g, minimum, ocean));
    // Wrong array lengths and invalid grids.
    CHECK_FATAL(geo::routing_flood(g, std::vector<std::int64_t>(5, 5), ocean));
    CHECK_FATAL(geo::routing_flood(g, h, std::vector<std::uint8_t>(7, 1U)));
    CHECK_FATAL(geo::routing_flood(Grid{0, 6}, h, ocean));
    CHECK_FATAL(geo::routing_flood(Grid{6, -1}, h, ocean));
    geo::RoutingFlood out;
    geo::RoutingFloodScratch scratch;
    CHECK_FATAL(geo::routing_flood(g, h, std::vector<std::uint8_t>(6, 0U), out, scratch));
}

// --- G4 ---------------------------------------------------------------------------

TEST_CASE("G4 worked example (P1-02 5.1): receivers before the lake rerouting, at 1, 2 and 5 threads") {
    const auto& e = ex_elevation();
    const auto ocean = ocean_of(e);
    const geo::RoutingFlood f = geo::routing_flood(EX, e, ocean);
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        const std::vector<Direction> rcv = geo::receivers(EX, f.level, f.parent, ocean, threads);
        CHECK(rcv == ex_receivers_before_rerouting());
        // Lake cells keep their flood parents (two exits, (2,3) SE and (3,3) S).
        CHECK(rcv[EX.index(2, 3)] == 1U);
        CHECK(rcv[EX.index(3, 3)] == 2U);
        CHECK(rcv[EX.index(2, 2)] == 2U);
        CHECK(rcv[EX.index(3, 2)] == 3U);
        std::vector<Direction> into(EX.cells(), 77U);
        geo::receivers(EX, f.level, f.parent, ocean, into, threads);
        CHECK(into == rcv);
    }
    CHECK(geo::receivers(EX, f.level, f.parent, ocean) == ex_receivers_before_rerouting());
}

TEST_CASE("G4 score ties (P1-02 8.1 item 3): orthogonal 200 beats diagonal 100; equal scores go to the lowest code") {
    // 6 x 3: rows 0 and 2 ocean (-1); row 1: ocean at x = 0, then 10.
    const Grid g{6, 3};
    std::vector<std::int64_t> e(18, -1);
    for (std::int32_t x = 1; x < 6; ++x) {
        e[g.index(x, 1)] = 10;
    }
    const auto ocean = ocean_of(e);
    const geo::RoutingFlood f = geo::routing_flood(g, e, ocean);
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        const std::vector<Direction> rcv = geo::receivers(g, f.level, f.parent, ocean, threads);
        CHECK(std::vector<Direction>(rcv.begin() + 6, rcv.begin() + 12) == std::vector<Direction>{8, 2, 2, 2, 2, 2});
        check_receivers(g, f, ocean, rcv);
    }
}

TEST_CASE("G4, G5 plateau (P1-02 8.1 item 4)") {
    // 7 x 5, border -1, all 15 interior cells 10.
    const Grid g{7, 5};
    std::vector<std::int64_t> e(35, -1);
    for (std::int32_t y = 1; y <= 3; ++y) {
        for (std::int32_t x = 1; x <= 5; ++x) {
            e[g.index(x, y)] = 10;
        }
    }
    const auto ocean = ocean_of(e);
    const geo::RoutingFlood f = geo::routing_flood(g, e, ocean);
    const std::vector<Direction> rcv = geo::receivers(g, f.level, f.parent, ocean);
    const std::vector<Direction> expected_rows = {
        8, 4, 6, 6, 6, 0, 8,  //
        8, 4, 5, 5, 5, 0, 8,  //
        8, 2, 2, 2, 2, 0, 8,  //
    };
    CHECK(std::vector<Direction>(rcv.begin() + 7, rcv.begin() + 28) == expected_rows);
    // The three cells (2..4, 2) have no lower neighbour: flood parent NW (5).
    for (std::int32_t x = 2; x <= 4; ++x) {
        CHECK(f.parent[g.index(x, 2)] == 5U);
    }
    check_receivers(g, f, ocean, rcv);
    const std::vector<std::uint32_t> a = geo::accumulate(g, rcv, ocean, f.order);
    const std::vector<std::uint32_t> expected_accum = {
        0, 2, 2, 2, 1, 1, 0,  //
        0, 1, 1, 1, 1, 1, 0,  //
        0, 1, 1, 1, 1, 1, 0,  //
    };
    CHECK(std::vector<std::uint32_t>(a.begin() + 7, a.begin() + 28) == expected_accum);
    CHECK(geo::accumulate_topological(g, rcv, ocean) == a);
}

TEST_CASE("G4: steepest slope compared exactly as w * d^2 (orthogonal w = 2, diagonal w = 1)") {
    const std::int64_t C = 1000;
    const std::int64_t H = 5000;  // a ring level above the centre: not a candidate
    // Orthogonal E d = 5 (score 50) against diagonal SE d = 7 (49): E.
    CHECK(centre_receiver(C, {C - 5, C - 7, H, H, H, H, H, H}) == 0U);
    // Orthogonal E d = 5 (50) against diagonal SE d = 8 (64): SE.
    CHECK(centre_receiver(C, {C - 5, C - 8, H, H, H, H, H, H}) == 1U);
    // A diagonal at a lower code than an orthogonal with the same drop: the
    // orthogonal's score is twice as large.
    CHECK(centre_receiver(C, {H, C - 4, C - 4, H, H, H, H, H}) == 2U);
    // Equal orthogonal scores: the lowest code (E before S before W).
    CHECK(centre_receiver(C, {C - 3, H, C - 3, H, C - 3, H, H, H}) == 0U);
    CHECK(centre_receiver(C, {H, H, H, H, C - 6, H, C - 6, H}) == 4U);
    // Equal diagonal scores: the lowest code.
    CHECK(centre_receiver(C, {H, C - 6, H, C - 6, H, C - 6, H, C - 6}) == 1U);
    // The best is the last code.
    CHECK(centre_receiver(C, {H, H, H, H, H, H, C - 5, C - 8}) == 7U);
    // Diagonal d = 3 (9) after orthogonal d = 2 (8): diagonal; and the
    // orthogonal tie-breaking runs on the exact integer scores.
    CHECK(centre_receiver(C, {C - 2, C - 3, H, H, H, H, H, H}) == 1U);
    CHECK(centre_receiver(C, {C - 1, C - 1, C - 1, C - 1, C - 1, C - 1, C - 1, C - 1}) == 0U);
    CHECK(centre_receiver(C, {C - 1, C - 2, C - 1, C - 1, C - 1, C - 1, C - 1, C - 1}) == 1U);
}

TEST_CASE("G4: no strictly lower neighbour (a flat, an equal-level neighbour, a pit) takes the flood parent") {
    const std::int64_t C = 1000;
    CHECK(centre_receiver(C, {C, C, C, C, C, C, C, C}, 3U) == 3U);
    CHECK(centre_receiver(C, {C + 1, C, C + 9, C, C + 1, C, C, C}, 6U) == 6U);
    CHECK(centre_receiver(C, {C + 1, C + 2, C + 3, C + 4, C + 5, C + 6, C + 7, C + 8}, 7U) == 7U);
    // One level below is enough.
    CHECK(centre_receiver(C, {C, C, C, C, C, C, C, C - 1}, 3U) == 7U);
}

TEST_CASE("G4: exact comparison of huge drops in U128 (orthogonal against the nearest diagonal drops)") {
    // For an orthogonal drop d_o, the diagonal drop with the same slope is
    // d_o * sqrt(2), never an integer. With d_d = floor(d_o * sqrt(2)) =
    // isqrt(2 d_o^2), the orthogonal wins (2 d_o^2 > d_d^2); with d_d + 1 the
    // diagonal wins. Drops up to 2^61, scores up to 2^123.
    worldgen::Rng rng(KEY_FLOW + 7U);
    const std::int64_t C = std::int64_t{1} << 62U;
    const std::int64_t H = worldgen_test::I64_MAX;
    std::vector<std::uint64_t> drops = {1U, 2U, 3U, 5U, 12U, 29U, 70U, 3037000499U, 3037000500U,
                                        std::uint64_t{1} << 31U, std::uint64_t{1} << 32U, std::uint64_t{1} << 40U,
                                        std::uint64_t{1} << 61U, (std::uint64_t{1} << 61U) - 1U};
    for (int i = 0; i < 40; ++i) {
        drops.push_back(1U + rng.uniform_u64(std::uint64_t{1} << 61U));
    }
    for (const std::uint64_t d_o : drops) {
        const worldgen::U128 twice_sq = worldgen::mul_u64_wide(d_o, d_o) << 1U;
        const std::uint64_t d_d = worldgen::isqrt_u128(twice_sq);
        REQUIRE(worldgen::mul_u64_wide(d_d, d_d) < twice_sq);  // never equal
        REQUIRE(d_d + 1U < static_cast<std::uint64_t>(C));
        CAPTURE(d_o);
        const std::int64_t lo = C - static_cast<std::int64_t>(d_o);
        const std::int64_t dlo = C - static_cast<std::int64_t>(d_d);
        const std::int64_t dlo1 = C - static_cast<std::int64_t>(d_d + 1U);
        // Orthogonal N (6) against diagonal SE (1) at the floor: N.
        CHECK(centre_receiver(C, {H, dlo, H, H, H, H, lo, H}) == 6U);
        // ... and at floor + 1: SE.
        CHECK(centre_receiver(C, {H, dlo1, H, H, H, H, lo, H}) == 1U);
        // Orthogonal E (0) first, diagonal NE (7) last.
        CHECK(centre_receiver(C, {lo, H, H, H, H, H, H, dlo}) == 0U);
        CHECK(centre_receiver(C, {lo, H, H, H, H, H, H, dlo1}) == 7U);
    }
}

TEST_CASE("G4 on seeded random floods: the definition, no cycles, ocean reached, order topological; 1, 2, 5 threads") {
    worldgen::Rng rng(KEY_FLOW + 8U);
    for (int trial = 0; trial < 24; ++trial) {
        // Tall grids so that there are several G6 blocks (up to 5).
        const Grid g{static_cast<std::int32_t>(1U + rng.uniform_u64(30U)),
                     static_cast<std::int32_t>(1U + rng.uniform_u64(300U))};
        std::vector<std::int64_t> h;
        std::vector<std::uint8_t> ocean;
        if (trial % 3 == 0) {
            random_terrain(rng, g, 5U, [](worldgen::Rng& r) { return r.uniform_i64(1, 3); }, h, ocean);
        } else if (trial % 3 == 1) {
            random_terrain(rng, g, 5U, [](worldgen::Rng& r) { return r.uniform_i64(1, 200); }, h, ocean);
        } else {
            random_terrain(rng, g, 5U,
                           [](worldgen::Rng& r) { return r.uniform_i64(std::int64_t{1} << 40U, std::int64_t{1} << 44U); },
                           h, ocean);
        }
        const geo::RoutingFlood f = geo::routing_flood(g, h, ocean);
        const std::vector<Direction> one = geo::receivers(g, f.level, f.parent, ocean, 1U);
        CAPTURE(trial);
        check_receivers(g, f, ocean, one);
        CHECK(geo::receivers(g, f.level, f.parent, ocean, 2U) == one);
        CHECK(geo::receivers(g, f.level, f.parent, ocean, 5U) == one);
        CHECK(geo::receivers(g, f.level, f.parent, ocean, worldgen::MAX_THREADS) == one);
    }
}

TEST_CASE("G4 at 1, 2 and 5 threads on a 512 x 512 bordered fixture") {
    worldgen::Rng rng(KEY_FLOW + 9U);
    const Grid g{512, 512};
    std::vector<std::int64_t> h;
    std::vector<std::uint8_t> ocean;
    bordered_terrain(rng, g, [](worldgen::Rng& r) { return r.uniform_i64(1, 30000); }, h, ocean);
    const geo::RoutingFlood f = geo::routing_flood(g, h, ocean);
    const std::vector<Direction> one = geo::receivers(g, f.level, f.parent, ocean, 1U);
    CHECK(geo::receivers(g, f.level, f.parent, ocean, 2U) == one);
    CHECK(geo::receivers(g, f.level, f.parent, ocean, 5U) == one);
    CHECK(one == reference_receivers(g, f.level, f.parent, ocean));
    // G5 on the same grid: both forms equal.
    const std::vector<std::uint32_t> a = geo::accumulate(g, one, ocean, f.order);
    CHECK(geo::accumulate_topological(g, one, ocean) == a);
    std::uint64_t mouths = 0;
    for (CellIndex c = 0; c < g.cells(); ++c) {
        if (ocean[c] == 0U && ocean[downstream(g, c, one[c])] != 0U) {
            mouths += a[c];
        }
    }
    CHECK(mouths == static_cast<std::uint64_t>(510 * 510));  // every land cell drains to exactly one mouth
}

TEST_CASE("G4: fatal preconditions") {
    const auto& e = ex_elevation();
    const auto ocean = ocean_of(e);
    const geo::RoutingFlood f = geo::routing_flood(EX, e, ocean);
    CHECK_FATAL(geo::receivers(EX, f.level, f.parent, ocean, 0U));
    CHECK_FATAL(geo::receivers(EX, f.level, f.parent, ocean, worldgen::MAX_THREADS + 1U));
    // A negative level.
    std::vector<std::int64_t> neg = f.level;
    neg[EX.index(5, 5)] = -1;
    CHECK_FATAL(geo::receivers(EX, neg, f.parent, ocean));
    // A non-ocean cell that needs its parent (a lake cell) with parent 8.
    std::vector<Direction> no_parent = f.parent;
    no_parent[EX.index(2, 2)] = NO_DIRECTION;
    CHECK_FATAL(geo::receivers(EX, f.level, no_parent, ocean));
    // ... but a cell with a strictly lower neighbour does not need it.
    std::vector<Direction> unneeded = f.parent;
    unneeded[EX.index(6, 5)] = NO_DIRECTION;
    CHECK(geo::receivers(EX, f.level, unneeded, ocean) == ex_receivers_before_rerouting());
    // Wrong sizes.
    CHECK_FATAL(geo::receivers(EX, std::vector<std::int64_t>(55, 1), f.parent, ocean));
    CHECK_FATAL(geo::receivers(EX, f.level, std::vector<Direction>(57, 0U), ocean));
    CHECK_FATAL(geo::receivers(EX, f.level, f.parent, std::vector<std::uint8_t>(10, 1U)));
    std::vector<Direction> out(55, 0U);
    CHECK_FATAL(geo::receivers(EX, f.level, f.parent, ocean, out, 1U));
}

// --- G5 ---------------------------------------------------------------------------

TEST_CASE("G5 worked example (P1-02 5.1): accumulation after the lake rerouting, both forms") {
    const auto& e = ex_elevation();
    const auto ocean = ocean_of(e);
    const geo::RoutingFlood f = geo::routing_flood(EX, e, ocean);
    // Kahn's form (P1-02 H5) on the final flow_dir.
    CHECK(geo::accumulate_topological(EX, ex_flow_dir(), ocean) == ex_flow_accum());
    // In this example the rerouted receivers still follow G3's order (the
    // outlet 26 is popped before 27, 18 and 19), so the order form applies.
    CHECK(geo::accumulate(EX, ex_flow_dir(), ocean, f.order) == ex_flow_accum());
    std::vector<std::uint32_t> walked;
    REQUIRE(path_walk_accumulation(EX, ex_flow_dir(), ocean, walked));
    CHECK(walked == ex_flow_accum());
}

TEST_CASE("G5 worked example: accumulation of G4's receivers before the rerouting") {
    const auto& e = ex_elevation();
    const auto ocean = ocean_of(e);
    const geo::RoutingFlood f = geo::routing_flood(EX, e, ocean);
    const std::vector<Direction> rcv = ex_receivers_before_rerouting();
    // By hand: (3,2) 2, (2,2) 1, outlet (2,3) 1 + 1 + 2 = 4, (3,3) 1 + 1 = 2,
    // the rim (3,4) 1 + 4 + 2 + 1 = 8, (4,5) 10; elsewhere as after rerouting.
    std::vector<std::uint32_t> expected = ex_flow_accum();
    expected[EX.index(2, 3)] = 4U;
    const std::vector<std::uint32_t> a = geo::accumulate(EX, rcv, ocean, f.order);
    CHECK(a == expected);
    CHECK(geo::accumulate_topological(EX, rcv, ocean) == expected);
}

TEST_CASE("G5 on seeded random floods: both forms equal path walking, also after lake rerouting") {
    worldgen::Rng rng(KEY_FLOW + 10U);
    for (int trial = 0; trial < 30; ++trial) {
        const Grid g{34, 34};
        std::vector<std::int64_t> h;
        std::vector<std::uint8_t> ocean;
        bordered_terrain(rng, g, [](worldgen::Rng& r) { return r.uniform_i64(1, 200); }, h, ocean);
        const geo::RoutingFlood f = geo::routing_flood(g, h, ocean);
        std::vector<Direction> rcv = geo::receivers(g, f.level, f.parent, ocean);
        std::vector<std::uint32_t> walked;
        REQUIRE(path_walk_accumulation(g, rcv, ocean, walked));
        CAPTURE(trial);
        CHECK(geo::accumulate(g, rcv, ocean, f.order) == walked);
        CHECK(geo::accumulate_topological(g, rcv, ocean) == walked);
        // Local form: A = 1 + sum over donors.
        for (CellIndex c = 0; c < g.cells(); ++c) {
            if (ocean[c] != 0U) {
                CHECK(walked[c] == 0U);
            }
        }

        // The rerouting of P1-02 H4 step 2, done here from its definition:
        // lakes = 8-connected components of {L > h} with equal L; outlet =
        // the lake cell first in `order`; receiver of the outlet = its flood
        // parent; every other lake cell = its BFS parent from the outlet.
        std::vector<std::uint8_t> lake_mask(g.cells(), 0U);
        for (CellIndex c = 0; c < g.cells(); ++c) {
            lake_mask[c] = (ocean[c] == 0U && f.level[c] > h[c]) ? std::uint8_t{1} : std::uint8_t{0};
        }
        const geo::Components lakes = geo::label_components(g, lake_mask, geo::Connectivity::eight, f.level);
        std::vector<std::size_t> pos(g.cells(), 0U);
        for (std::size_t p = 0; p < f.order.size(); ++p) {
            pos[f.order[p]] = p;
        }
        std::vector<std::uint8_t> visited(g.cells(), 0U);
        for (std::uint32_t k = 1; k <= lakes.count; ++k) {
            CellIndex outlet = lakes.order[lakes.start[k - 1U]];
            for (std::uint32_t j = lakes.start[k - 1U]; j < lakes.start[k]; ++j) {
                if (pos[lakes.order[j]] < pos[outlet]) {
                    outlet = lakes.order[j];
                }
            }
            rcv[outlet] = f.parent[outlet];
            std::vector<CellIndex> visit;
            geo::bfs_region(g, lakes.label, outlet, geo::Connectivity::eight, rcv, visited, visit);
        }
        REQUIRE(path_walk_accumulation(g, rcv, ocean, walked));
        CHECK(geo::accumulate_topological(g, rcv, ocean) == walked);
    }
}

TEST_CASE("G5: fatal preconditions") {
    const auto& e = ex_elevation();
    const auto ocean = ocean_of(e);
    const geo::RoutingFlood f = geo::routing_flood(EX, e, ocean);
    const std::vector<Direction> rcv = ex_receivers_before_rerouting();
    static_cast<void>(geo::accumulate(EX, rcv, ocean, f.order));  // valid
    // `order` not a permutation: a repeated cell, a short list, an index out of range.
    std::vector<CellIndex> dup = f.order;
    dup[10] = dup[11];
    CHECK_FATAL(geo::accumulate(EX, rcv, ocean, dup));
    std::vector<CellIndex> short_order(f.order.begin(), f.order.end() - 1);
    CHECK_FATAL(geo::accumulate(EX, rcv, ocean, short_order));
    std::vector<CellIndex> out_of_range = f.order;
    out_of_range[3] = 56U;
    CHECK_FATAL(geo::accumulate(EX, rcv, ocean, out_of_range));
    // A donor listed before its receiver: (3,4) = 35 drains SE to (4,5) = 44,
    // which is popped just before it; swap the two.
    REQUIRE(rcv[35] == 1U);
    std::vector<CellIndex> bad_order = f.order;
    std::size_t p35 = 0;
    std::size_t p44 = 0;
    for (std::size_t p = 0; p < bad_order.size(); ++p) {
        p35 = bad_order[p] == 35U ? p : p35;
        p44 = bad_order[p] == 44U ? p : p44;
    }
    REQUIRE(p44 < p35);
    std::swap(bad_order[p35], bad_order[p44]);
    CHECK_FATAL(geo::accumulate(EX, rcv, ocean, bad_order));
    // A non-ocean receiver that is NO_DIRECTION, out of range, or points outside the grid.
    for (const auto& [cell, code] : {std::pair<CellIndex, Direction>{EX.index(3, 3), NO_DIRECTION},
                                     std::pair<CellIndex, Direction>{EX.index(3, 3), 9U},
                                     std::pair<CellIndex, Direction>{EX.index(1, 1), 99U}}) {
        std::vector<Direction> bad = rcv;
        bad[cell] = code;
        CHECK_FATAL(geo::accumulate(EX, bad, ocean, f.order));
        CHECK_FATAL(geo::accumulate_topological(EX, bad, ocean));
    }
    // A receiver pointing outside a grid without an ocean border.
    {
        const Grid g{2, 1};
        const std::vector<std::uint8_t> o = {1, 0};
        const std::vector<Direction> r = {NO_DIRECTION, 0U};  // E of (1,0) is outside
        CHECK_FATAL(geo::accumulate(g, r, o, std::vector<CellIndex>{0, 1}));
        CHECK_FATAL(geo::accumulate_topological(g, r, o));
    }
    // A cycle: (2,2) and (2,3) pointing at each other.
    std::vector<Direction> cycle = rcv;
    cycle[EX.index(2, 2)] = 2U;  // S to (2,3)
    cycle[EX.index(2, 3)] = 6U;  // N to (2,2)
    CHECK_FATAL(geo::accumulate_topological(EX, cycle, ocean));
    // Wrong sizes.
    CHECK_FATAL(geo::accumulate(EX, std::vector<Direction>(55, 0U), ocean, f.order));
    CHECK_FATAL(geo::accumulate_topological(EX, rcv, std::vector<std::uint8_t>(55, 0U)));
}
