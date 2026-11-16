// Property tests of the elevation stage (spec P1-01 sections 6 and 10.2):
// every invariant of section 6.1 over several seeds, sizes and both
// fixtures, each step checked by code independent of the stage (its own E3
// by sorting, E4 components by union-find, E5.1 search, chamfer by
// Dijkstra, flood levels by a minimax search), and E8 and E10 alone on
// random fixtures.
//
// The full sweep of 10.2 (seeds 0 to 63 at N = 128 with both fixtures,
// seeds 0 to 3 at N = 512) is in the suite "elevation-slow", skipped by
// default: worldgen-tests -ts=elevation-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <ostream>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/rng.hpp"

using namespace elev_test;
using worldgen::Fx;
using worldgen::geo::Grid;

namespace {

const elev_api::Tuning& tuning(bool small) {
    static const elev_api::Tuning shipped = [] {
        const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml());
        REQUIRE(l.ok());
        return elev_api::tuning_from(*l.content);
    }();
    static const elev_api::Tuning sm = [] {
        const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml(small_overrides()));
        REQUIRE(l.ok());
        return elev_api::tuning_from(*l.content);
    }();
    return small ? sm : shipped;
}

bool on_edge(Grid g, std::size_t i) {
    const std::int32_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(i));
    const std::int32_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(i));
    return x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1;
}

std::vector<std::int64_t> component_sizes(const std::vector<std::uint32_t>& label) {
    std::uint32_t count = 0;
    for (const std::uint32_t l : label) {
        count = std::max(count, l);
    }
    std::vector<std::int64_t> size(count + 1U, 0);
    for (const std::uint32_t l : label) {
        if (l != 0U) {
            size[l] += 1;
        }
    }
    return size;
}

// E4 by the test: provisional P, edge-touching 4-components of P, then
// land 8-components below I_min sunk (any W x H grid).
std::vector<std::uint8_t> own_ocean(Grid g, const std::vector<std::uint8_t>& p, std::int64_t i_min) {
    const std::size_t n = cells(g);
    const auto all = [](std::size_t, std::size_t) { return true; };
    const std::vector<std::uint32_t> pl = uf_components(g, p, false, all);
    std::vector<std::uint8_t> edge_label(component_sizes(pl).size(), 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (pl[i] != 0U && on_edge(g, i)) {
            edge_label[pl[i]] = 1;
        }
    }
    std::vector<std::uint8_t> o(n, 0);
    std::vector<std::uint8_t> land(n, 0);
    for (std::size_t i = 0; i < n; ++i) {
        o[i] = (pl[i] != 0U && edge_label[pl[i]] != 0U) ? 1 : 0;
        land[i] = o[i] != 0U ? 0 : 1;
    }
    const std::vector<std::uint32_t> ll = uf_components(g, land, true, all);
    const std::vector<std::int64_t> ls = component_sizes(ll);
    for (std::size_t i = 0; i < n; ++i) {
        if (ll[i] != 0U && ls[ll[i]] < i_min) {
            o[i] = 1;
        }
    }
    return o;
}

// Land cells drain to the sea along a path on which the surface never
// rises, kept lake cells counting at their flood level (see the note at
// check_conditioned). Returns the cells reached.
std::vector<std::uint8_t> drained(Grid g, std::span<const std::int64_t> surface, std::span<const std::uint8_t> ocean) {
    const std::size_t n = cells(g);
    std::vector<std::uint8_t> d(n, 0);
    std::deque<std::size_t> q;
    for (std::size_t i = 0; i < n; ++i) {
        if (ocean[i] != 0U) {
            d[i] = 1;
            q.push_back(i);
        }
    }
    while (!q.empty()) {
        const std::size_t c = q.front();
        q.pop_front();
        each_neighbour8(g, c, [&](std::uint8_t, std::size_t m) {
            if (d[m] == 0U && ocean[m] == 0U && surface[m] >= surface[c]) {
                d[m] = 1;
                q.push_back(m);
            }
        });
    }
    return d;
}

struct LakeLimits {
    std::int64_t min_cells = 0, max_cells = 0, min_depth = 0, max_share = 0;
};

// The E10 postconditions (invariant 7, 6.3) for e -> e', on any grid.
//
// Spec 10.2 states "every non-kept land cell reaches the ocean along a path
// on which e' never increases". Taken literally this fails wherever a kept
// lake drains a slope (the path crosses the lake bed and rises to the
// rim): the reference finds 138, 591 and 560 such cells in the small-map
// worlds 128/0, 1 and 2 of 10.4. What 6.3 proves is that no non-kept cell
// is depressed; the test checks that, and the monotone path with kept cells
// counted at their flood level (their water surface).
void check_conditioned(Grid g, std::span<const std::int16_t> e, std::span<const std::int16_t> ef,
                       std::span<const std::uint8_t> ocean, const LakeLimits& lim) {
    const std::size_t n = cells(g);
    REQUIRE(ef.size() == n);
    std::vector<std::int64_t> k(ef.begin(), ef.end());
    const std::vector<std::int64_t> L = minimax_levels(g, k, ocean);
    std::vector<std::uint8_t> dep(n, 0);
    std::int64_t kept_total = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (ocean[i] != 0U) {
            REQUIRE(ef[i] == e[i]);  // E10 writes no ocean cell
            continue;
        }
        REQUIRE(ef[i] >= e[i]);  // claim (b): e' >= e on land
        REQUIRE(ef[i] >= 1);
        REQUIRE(ef[i] <= KEY_MAX);
        dep[i] = L[i] > k[i] ? 1 : 0;
        kept_total += dep[i];
    }
    // Depression components of e' are the kept lakes: bounds, unchanged.
    const std::vector<std::uint32_t> lab =
        uf_components(g, dep, true, [&L](std::size_t a, std::size_t b) { return L[a] == L[b]; });
    const std::vector<std::int64_t> size = component_sizes(lab);
    std::vector<std::int64_t> depth(size.size(), 0);
    for (std::size_t i = 0; i < n; ++i) {
        if (lab[i] != 0U) {
            depth[lab[i]] = std::max(depth[lab[i]], L[i] - k[i]);
            REQUIRE(ef[i] == e[i]);  // a kept lake keeps its E9 values
        }
    }
    for (std::size_t c = 1; c < size.size(); ++c) {
        CAPTURE(c);
        CHECK(size[c] >= lim.min_cells);
        CHECK(size[c] <= lim.max_cells);
        CHECK(depth[c] >= lim.min_depth);
    }
    CHECK(kept_total <= lim.max_share);
    // Every other land cell drains.
    std::vector<std::int64_t> surface(n);
    for (std::size_t i = 0; i < n; ++i) {
        surface[i] = dep[i] != 0U ? L[i] : k[i];
    }
    const std::vector<std::uint8_t> d = drained(g, surface, ocean);
    for (std::size_t i = 0; i < n; ++i) {
        if (ocean[i] == 0U && dep[i] == 0U) {
            REQUIRE(L[i] == k[i]);
            REQUIRE(d[i] == 1U);
        }
    }
    // The kept components of e' were depressions of e with the same cells.
    std::vector<std::int64_t> ke(e.begin(), e.end());
    const std::vector<std::int64_t> Le = minimax_levels(g, ke, ocean);
    for (std::size_t i = 0; i < n; ++i) {
        if (dep[i] != 0U) {
            REQUIRE(Le[i] > ke[i]);
        }
    }
}

elev_api::ErosionParams erosion_of(const Values& v, std::uint64_t seed) {
    elev_api::ErosionParams p;
    p.iterations = v.i("erosion_iterations");
    p.route_interval = v.i("erosion_route_interval");
    p.k = v.raw("erosion_k");
    p.diffusion = v.raw("erosion_diffusion");
    p.talus_slope = v.raw("talus_slope");
    p.talus_rate = v.raw("talus_rate");
    p.floor = v.raw("land_floor_m");
    p.key = key(seed, "erosion");
    return p;
}

// Every invariant of 6.1 for one world, step by step.
void check_world(std::uint64_t seed, std::int32_t n, bool small) {
    CAPTURE(seed);
    CAPTURE(n);
    CAPTURE(small);
    const Values& v = small ? small_values() : shipped_values();
    const elev_api::Tuning& t = tuning(small);
    const Grid g{n, n};
    const std::size_t nn = cells(g);
    const std::uint64_t un = static_cast<std::uint64_t>(nn);

    // E1 to E3.
    const elev_api::Fields f = elev_api::fields(seed, n, t, THREADS);
    REQUIRE(f.c.size() == nn);
    CHECK(f.cx == centre(seed, 0, v));
    CHECK(f.cy == centre(seed, 1, v));
    CHECK(f.c_sea == kth_sorted(f.c, q_rank(ONE - v.raw("land_fraction"), un)));
    const std::vector<std::int64_t> gr = belt_gradient(g, f.nb);
    const std::int64_t g_med = std::max<std::int64_t>(1, kth_sorted(gr, q_rank(HALF, un)));
    CHECK(f.w_b == std::max(R(1), worldgen::div_int(worldgen::mul(R(g_med), R(v.hw(n))), 16)).raw());
    std::vector<std::int64_t> above;
    for (const std::int64_t c : f.c) {
        if (c > f.c_sea) {
            above.push_back(c - f.c_sea);
        }
    }
    REQUIRE_FALSE(above.empty());
    CHECK(f.s_land == std::max<std::int64_t>(1, kth_sorted(above, q_rank(HALF, above.size()))));

    // E4 (invariants 2, 3).
    const elev_api::Ocean oc = elev_api::classify(f, t);
    std::vector<std::uint8_t> p(nn, 0);
    const std::int64_t margin = v.margin(n);
    for (std::size_t i = 0; i < nn; ++i) {
        const std::int64_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(i));
        const std::int64_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(i));
        p[i] = (f.c[i] <= f.c_sea || std::min({x, y, n - 1 - x, n - 1 - y}) < margin) ? 1 : 0;
        if (std::min({x, y, n - 1 - x, n - 1 - y}) < margin) {
            REQUIRE(oc.mask[i] == 1U);  // the margin is ocean
        }
    }
    REQUIRE(oc.mask == own_ocean(g, p, v.i_min()));
    std::int64_t land_cells = 0;
    std::vector<std::uint8_t> land(nn, 0);
    for (std::size_t i = 0; i < nn; ++i) {
        land[i] = oc.mask[i] != 0U ? 0 : 1;
        land_cells += land[i];
    }
    CHECK(oc.land_cells == land_cells);
    const auto all = [](std::size_t, std::size_t) { return true; };
    const std::vector<std::int64_t> osz = component_sizes(uf_components(g, oc.mask, false, all));
    CHECK(osz.size() == 2U);  // one 4-connected ocean
    const std::vector<std::int64_t> lsz = component_sizes(uf_components(g, land, true, all));
    for (std::size_t c = 1; c < lsz.size(); ++c) {
        CHECK(lsz[c] >= v.i_min());
    }

    // E5.1 (invariant 9), by the test's own search.
    std::vector<std::int64_t> nb, ns, gm;
    for (std::size_t i = 0; i < nn; ++i) {
        if (land[i] != 0U) {
            nb.push_back(f.nb[i]);
            ns.push_back(f.ns[i]);
            gm.push_back(gm_of(f.c[i], f.c_sea, f.s_land, v));
        }
    }
    const FloorOwn own = floor_own(nb, ns, gm, f.w_b, v);
    const elev_api::Heights h = elev_api::heights(seed, f, oc, t, THREADS);
    CHECK(h.floor.triggered == own.triggered);
    CHECK(h.floor.target == own.target);
    CHECK(h.floor.o_star == own.o_star);
    CHECK(h.floor.beta_star == own.beta_star);
    if (!h.floor.triggered) {
        CHECK(h.floor.o_star == 0);
        CHECK(h.floor.beta_star == v.raw("segment_bias"));
    } else {
        CHECK(h.floor.beta_star >= v.raw("segment_bias"));
    }
    // The floor seam on the same lists agrees.
    const elev_api::Floor fs = elev_api::floor_search(nb, ns, gm, f.w_b, t);
    CHECK(fs.o_star == own.o_star);
    CHECK(fs.beta_star == own.beta_star);

    // E5.3 and E7: the contract before E8.
    const std::vector<std::int64_t> dl = chamfer_dijkstra(g, land);
    std::int64_t max_land7 = 0;
    for (std::size_t i = 0; i < nn; ++i) {
        if (land[i] != 0U) {
            REQUIRE(h.h7[i] >= v.raw("land_floor_m"));
            REQUIRE(h.h7[i] <= H_MAX_RAW);
            max_land7 = std::max(max_land7, h.h7[i]);
        } else {
            REQUIRE(h.h7[i] == ocean_height(dl[i], v));
        }
    }

    // E8: ocean unchanged, land >= floor, the largest land height does not grow.
    const std::vector<std::int64_t> h8 = elev_api::erode(g, h.h7, oc.mask, erosion_of(v, seed), THREADS);
    std::int64_t max_land8 = 0;
    for (std::size_t i = 0; i < nn; ++i) {
        if (land[i] != 0U) {
            REQUIRE(h8[i] >= v.raw("land_floor_m"));
            max_land8 = std::max(max_land8, h8[i]);
        } else {
            REQUIRE(h8[i] == h.h7[i]);
        }
    }
    CHECK(max_land8 <= max_land7);

    // E9.
    const std::vector<std::int16_t> e = elev_api::quantize(h8, oc.mask);
    REQUIRE(e.size() == nn);
    for (std::size_t i = 0; i < nn; ++i) {
        REQUIRE(e[i] == quantize_cell(h8[i], oc.mask[i] != 0U));
        if (land[i] != 0U) {
            REQUIRE(e[i] <= 29900);
        }
    }

    // E10 (invariant 7) and the layer (invariants 1, 4, 5).
    const CostBump cb = cost_bump(seed, n, oc.mask, v);
    elev_api::ConditionParams cp;
    cp.lake_min_cells = v.lake_min_cells();
    cp.lake_max_cells = v.lake_max_cells(n);
    cp.lake_min_depth_dm = v.lake_min_depth_dm();
    cp.max_share_cells = v.max_share_cells(land_cells);
    cp.fine_slope = v.fine_slope();
    const std::vector<std::int16_t> ef = elev_api::condition(g, e, oc.mask, cb.cost, cb.bump, cp);
    for (std::size_t i = 0; i < nn; ++i) {
        REQUIRE(ef[i] != 0);
        REQUIRE((ef[i] < 0) == (oc.mask[i] != 0U));
        if (ef[i] < 0) {
            REQUIRE(ef[i] >= -3005);  // shipped ocean_depth_m
        } else {
            REQUIRE(ef[i] <= 29990);
        }
    }
    check_conditioned(g, e, ef, oc.mask,
                      LakeLimits{cp.lake_min_cells, cp.lake_max_cells, cp.lake_min_depth_dm, cp.max_share_cells});
}

}  // namespace

TEST_CASE("P1-01 6.1: invariants, shipped fixture, N = 128 seeds 0 to 5 and 9, 10, 12, 13") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 4U, 5U, 9U, 10U, 12U, 13U}) {
        check_world(seed, 128, false);
    }
}

TEST_CASE("P1-01 6.1: invariants, small-map fixture, N = 128 seeds 0 to 5") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 4U, 5U}) {
        check_world(seed, 128, true);
    }
}

TEST_CASE("P1-01 6.1: invariants at other sizes (64, 96, 48)") {
    check_world(0, 64, false);
    check_world(1, 64, true);
    check_world(2, 96, false);
    check_world(3, 48, false);
}

TEST_CASE("P1-01 10.2: the floor triggers at N = 128 among seeds 0 to 15 exactly at 9, 10, 12, 13") {
    std::vector<std::uint64_t> triggered;
    for (std::uint64_t seed = 0; seed < 16; ++seed) {
        const elev_api::Fields f = elev_api::fields(seed, 128, tuning(false), THREADS);
        const elev_api::Ocean oc = elev_api::classify(f, tuning(false));
        const elev_api::Heights h = elev_api::heights(seed, f, oc, tuning(false), THREADS);
        if (h.floor.triggered) {
            triggered.push_back(seed);
            CHECK(h.floor.beta_star == shipped_values().raw("segment_bias"));
        }
        if (seed == 13) {
            CHECK(h.floor.o_star == worldgen::mul_int(worldgen::div_int(R(f.w_b), 2), -4).raw());
        }
    }
    CHECK(triggered == std::vector<std::uint64_t>{9, 10, 12, 13});
}

// --- E8 and E10 alone on random fixtures --------------------------------------------

namespace {

// Fixed Rng keys for the random fixtures (not world streams; spec 10.2
// names a test stream `test.elevation.fill`, which is not registered in
// streams.hpp, so the generators are keyed directly as core_test_util does).
constexpr std::uint64_t KEY_FILL_FIXTURES = 0x4C4C49462E54534BULL;
constexpr std::uint64_t KEY_ERODE_FIXTURES = 0x45444F52452E5453ULL;

}  // namespace

TEST_CASE("P1-01 10.2: E10 alone on 200 random fixtures (32 x 32 interior, ocean border)") {
    worldgen::Rng rng(KEY_FILL_FIXTURES);
    const Grid g{34, 34};
    const std::size_t n = cells(g);
    for (int fixture = 0; fixture < 200; ++fixture) {
        CAPTURE(fixture);
        std::vector<std::int16_t> e(n);
        std::vector<std::uint8_t> o(n);
        std::vector<std::int64_t> cost(n, ONE);
        std::vector<std::int64_t> bump(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            o[i] = on_edge(g, i) ? 1 : 0;
            e[i] = static_cast<std::int16_t>(o[i] != 0U ? -rng.uniform_i64(1, 50) : rng.uniform_i64(1, 200));
            if (o[i] == 0U) {
                cost[i] = rng.uniform_i64(ONE, 4 * ONE);
                bump[i] = rng.uniform_i64(0, std::int64_t{1} << 20);
            }
        }
        elev_api::ConditionParams p;
        p.lake_min_cells = rng.uniform_i64(1, 10);
        p.lake_max_cells = rng.uniform_i64(p.lake_min_cells, 80);
        p.lake_min_depth_dm = rng.uniform_i64(0, 40);
        p.max_share_cells = rng.uniform_i64(1, 300);
        p.fine_slope = rng.uniform_i64(0, 10 * 65536);
        const std::vector<std::int16_t> ef = elev_api::condition(g, e, o, cost, bump, p);
        check_conditioned(g, e, ef, o, LakeLimits{p.lake_min_cells, p.lake_max_cells, p.lake_min_depth_dm,
                                                  p.max_share_cells});
    }
}

TEST_CASE("P1-01 10.2: E8 alone on random inputs meeting the contract of E7") {
    worldgen::Rng rng(KEY_ERODE_FIXTURES);
    const Grid g{24, 20};
    const std::size_t n = cells(g);
    for (int fixture = 0; fixture < 40; ++fixture) {
        CAPTURE(fixture);
        elev_api::ErosionParams p;
        p.iterations = rng.uniform_i64(1, 6);
        p.route_interval = rng.uniform_i64(1, 3);
        p.k = rng.uniform_i64(0, ONE);
        p.diffusion = rng.uniform_i64(0, ONE / 4);
        p.talus_slope = rng.uniform_i64(dec("0.05"), 4 * ONE);
        p.talus_rate = rng.uniform_i64(0, ONE / 16);
        p.floor = rng.uniform_i64(dec("0.05"), 10 * ONE);
        p.key = rng.next();
        std::vector<std::int64_t> h(n);
        std::vector<std::uint8_t> o(n);
        std::int64_t max_land = 0;
        for (std::size_t i = 0; i < n; ++i) {
            // Ocean border plus some interior ocean (lakes of sea level).
            o[i] = (on_edge(g, i) || rng.uniform_i64(0, 19) == 0) ? 1 : 0;
            if (o[i] != 0U) {
                h[i] = -rng.uniform_i64(HALF, dec("2900.5"));
            } else {
                // Mostly smooth slopes with spikes, so that every part acts.
                h[i] = rng.uniform_i64(0, 3) == 0 ? rng.uniform_i64(p.floor, H_MAX_RAW)
                                                  : rng.uniform_i64(p.floor, 200 * ONE);
                max_land = std::max(max_land, h[i]);
            }
        }
        const std::vector<std::int64_t> out = elev_api::erode(g, h, o, p, 1 + static_cast<std::uint32_t>(fixture % 3));
        REQUIRE(out.size() == n);
        std::int64_t max_out = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (o[i] != 0U) {
                REQUIRE(out[i] == h[i]);
            } else {
                REQUIRE(out[i] >= p.floor);
                max_out = std::max(max_out, out[i]);
            }
        }
        CHECK(max_out <= max_land);
        // Thread count never matters.
        CHECK(elev_api::erode(g, h, o, p, 5) == out);
    }
}

// --- The full sweep of 10.2 (opt-in) -------------------------------------------------

TEST_SUITE("elevation-slow" * doctest::skip()) {

TEST_CASE("P1-01 10.2 (slow): invariants, N = 128 seeds 0 to 63, both fixtures") {
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        check_world(seed, 128, false);
        check_world(seed, 128, true);
    }
}

TEST_CASE("P1-01 10.2 (slow): invariants, N = 512 seeds 0 to 3, shipped fixture") {
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        check_world(seed, 512, false);
    }
}

}  // TEST_SUITE
