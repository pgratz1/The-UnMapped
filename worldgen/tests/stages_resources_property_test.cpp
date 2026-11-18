// Invariants of the resources stage as property tests (spec P1-05 section
// 6 invariants 1 to 5, 10.2; section 9 hard statistics bounds).
//
// Each world's layers come from the stage (seam resources_layers); every
// invariant is checked by the test's own code: G7 by recursion along
// flow_dir (p1_04_biomes_support.hpp, not the spec's FIFO), the chamfer by
// Dijkstra over the 5/7 graph, walk areas by union-find and their
// connectivity by a BFS within each id, and R6 recomputed from the stored
// layers. Invariant 5 is checked at the seams with each land cell's own
// arguments and a larger slope or distance.
//
// Seeds 0 to 63 at N = 128 and the statistics bounds at 512 and 1024 run in
// the suite "resources-slow":
//   worldgen-tests -ts=resources-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "p1_04_biomes_support.hpp"
#include "p1_05_resources_api.hpp"
#include "p1_05_resources_support.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"

using namespace resource_test;

namespace {

void check_invariants(const std::string& label, const resource_api::Inputs& in, const Values& v,
                      const resource_api::Tuning& t, std::uint64_t seed) {
    CAPTURE(label);
    const Grid g = in.grid;
    const std::size_t n = g.cells();
    const resource_api::Layers l = resource_api::layers(in, definitions(), t, seed, 3);
    const std::vector<std::int64_t> s = biome_test::surface(in.elevation, in.water_depth);
    const auto hand = biome_test::own_hand(g, s, in.water, in.river, in.flow_dir, 1);
    REQUIRE(hand.has_value());
    const std::vector<std::int64_t> s2 = biome_test::own_s2(g, in.elevation);
    const std::vector<std::int64_t> df = metres_of(own_chamfer(g, fresh_sources(in.water)));
    const std::vector<std::int64_t> dg = metres_of(own_chamfer(g, fish_sources(in.water, in.river, v.fish_river_class_min)));
    const std::uint64_t key = worldgen::stream_key(seed, STREAM_ORE);
    std::map<std::string, std::int64_t> bad;
    auto fail = [&](const char* what, std::size_t i) {
        if (bad[what]++ == 0) {
            MESSAGE(label << ": " << what << " at (" << g.x_of(static_cast<worldgen::geo::CellIndex>(i)) << ", "
                          << g.y_of(static_cast<worldgen::geo::CellIndex>(i)) << ")");
        }
    };
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint8_t w = in.water[i];
        // Invariant 1.
        if (w == WATER_OCEAN && (l.fertility[i] || l.ore[i] || l.timber[i] || l.fresh_water[i] || l.forage[i] ||
                                 l.habitability[i] || l.walk_area[i])) {
            fail("ocean not 0", i);
        }
        if (w == WATER_LAKE && (l.fertility[i] || l.ore[i] || l.timber[i] || l.forage[i] || l.habitability[i] ||
                                l.walk_area[i] || l.fresh_water[i] != 255)) {
            fail("lake not 0 / fresh 255", i);
        }
        if (w == WATER_RIVER && l.fresh_water[i] != 255) {
            fail("river fresh water not 255", i);
        }
        if (!is_land(w)) {
            continue;
        }
        const BiomeValues& b = biome_values().at(in.biome[i]);
        const std::int64_t tan = own_tangent(s2[i]);
        // Invariant 2.
        if (tan >= v.hab_slope_hi && l.habitability[i] != 0) {
            fail("habitability on steep ground", i);
        }
        if ((tan >= v.fert_slope_hi || b.fertility == 0) && l.fertility[i] != 0) {
            fail("fertility where it must be 0", i);
        }
        if ((tan >= v.timber_slope_hi || b.timber == 0) && l.timber[i] != 0) {
            fail("timber where it must be 0", i);
        }
        const std::int64_t noise = own_noise(key, g.x_of(static_cast<worldgen::geo::CellIndex>(i)),
                                             g.y_of(static_cast<worldgen::geo::CellIndex>(i)), v.ore_noise);
        if (noise <= v.ore_vein_lo && l.ore[i] != 0) {
            fail("ore without a vein", i);
        }
        if (l.forage[i] < own_q255(b.forage)) {
            fail("forage below the biome's", i);
        }
        // Invariant 4: R6 from the stored layers.
        const std::uint8_t h = own_habitability(l.fertility[i], l.fresh_water[i], l.forage[i], l.timber[i], l.ore[i],
                                                s2[i], (*hand)[i], l.walk_area[i], v);
        if (h != l.habitability[i]) {
            fail("habitability is not R6 of the stored layers", i);
        }
        // The other layers are R3 and R4 of the inputs (the test's own).
        if (l.fresh_water[i] != own_fresh(df[i], in.rainfall[i], v)) {
            fail("fresh_water differs from own R3", i);
        }
        if (l.fertility[i] != own_fertility(b.fertility, s2[i], (*hand)[i], v) ||
            l.timber[i] != own_timber(b.timber, s2[i], v) || l.forage[i] != own_forage(b.forage, dg[i], v) ||
            l.ore[i] != own_ore(in.elevation[i], s2[i], noise, v)) {
            fail("an R4 score differs from own R4", i);
        }
        // Invariant 5 at the seams, at this cell's arguments.
        for (const std::int64_t more : {s2[i] + 1, 2 * s2[i] + 7, s2[i] + 50000}) {
            if (resource_api::fertility(b.fertility, more, (*hand)[i], t) > l.fertility[i] ||
                resource_api::timber(b.timber, more, t) > l.timber[i] ||
                resource_api::habitability(l.fertility[i], l.fresh_water[i], l.forage[i], l.timber[i], l.ore[i], more,
                                           (*hand)[i], l.walk_area[i], t) > l.habitability[i]) {
                fail("a score increases with S2", i);
            }
        }
        if (resource_api::fresh_water(df[i] + 3, in.rainfall[i], t) > l.fresh_water[i] ||
            resource_api::forage(b.forage, dg[i] + 3, t) > l.forage[i]) {
            fail("a score increases with distance", i);
        }
        if (l.walk_area[i] != 0 && resource_api::habitability(l.fertility[i], l.fresh_water[i], l.forage[i],
                                                              l.timber[i], l.ore[i], s2[i], (*hand)[i], 0, t) >
                                       l.habitability[i]) {
            fail("habitability increases with the pocket factor", i);
        }
        if (resource_api::habitability(l.fertility[i], l.fresh_water[i], l.forage[i], l.timber[i], l.ore[i], s2[i], 0,
                                       l.walk_area[i], t) > l.habitability[i]) {
            fail("habitability increases with the flood factor", i);
        }
        if (l.walk_area[i] != 0 && !own_walkable(w, s2[i], v.walk_slope_max)) {
            fail("walk area on an unwalkable cell", i);
        }
    }
    // Invariant 3.
    const OwnWalk uf = own_walk(g, s, in.water, s2, v.walk_slope_max, v.walk_min_cells());
    CHECK(l.walk_area == uf.ids);
    std::map<std::uint16_t, std::size_t> first, size;
    for (std::size_t i = 0; i < n; ++i) {
        if (l.walk_area[i] != 0) {
            first.emplace(l.walk_area[i], i);
            ++size[l.walk_area[i]];
        }
    }
    std::uint16_t k = 0;
    std::size_t prev = 0;
    for (const auto& [id, at] : first) {
        CHECK(id == ++k);  // ids 1..K, each used
        if (id > 1) {
            CHECK(at > prev);  // increasing with the smallest index
        }
        prev = at;
        CHECK(static_cast<std::int64_t>(size[id]) >= v.walk_min_cells());
    }
    // Each id is one component under `linked` (a BFS within the id reaches
    // every cell of it), and linked walkable cells share their id.
    std::vector<bool> seen(n, false);
    for (const auto& [id, at] : first) {
        std::deque<std::size_t> q{at};
        seen[at] = true;
        std::size_t reached = 0;
        while (!q.empty()) {
            const std::size_t c = q.front();
            q.pop_front();
            ++reached;
            const std::int32_t x = g.x_of(static_cast<worldgen::geo::CellIndex>(c));
            const std::int32_t y = g.y_of(static_cast<worldgen::geo::CellIndex>(c));
            for (std::size_t d = 0; d < 8; ++d) {
                const std::int32_t mx = x + DX[d], my = y + DY[d];
                if (mx < 0 || my < 0 || mx >= g.width || my >= g.height) {
                    continue;
                }
                const std::size_t m = g.index(mx, my);
                if (!own_walkable(in.water[m], s2[m], v.walk_slope_max) ||
                    !own_linked(s[c], s[m], (d & 1U) != 0, v.walk_slope_max)) {
                    continue;
                }
                if (l.walk_area[m] != id) {
                    fail("linked cells with different walk areas", m);
                    continue;
                }
                if (!seen[m]) {
                    seen[m] = true;
                    q.push_back(m);
                }
            }
        }
        CHECK(reached == size[id]);
    }
    for (const auto& [what, count] : bad) {
        CAPTURE(what);
        CHECK(count == 0);
    }
}

void check_world(std::uint64_t seed, bool small) {
    const resource_api::Inputs in = world_inputs(seed, 128, small);
    check_invariants(std::string(small ? "small" : "shipped") + " 128 seed " + std::to_string(seed), in, values(),
                     tuning(), seed);
}

}  // namespace

TEST_CASE("P1-05 6 invariants 1 to 5, 10.2: seeds 0 and 1 at N = 128, shipped and small-map fixtures") {
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{1}}) {
        check_world(seed, false);
        check_world(seed, true);
    }
}

TEST_CASE("P1-05 6 invariants 1 to 5: the designed and terrace fixtures with every golden tuning variant") {
    std::size_t ran = 0;
    for (const GoldenCase& c : golden()) {
        if (c.fixture == "designed" || c.fixture == "terrace") {
            check_invariants(c.name, case_inputs(c), values(c.overrides), tuning(c.overrides), c.seed);
            ++ran;
        }
    }
    CHECK(ran >= 20U);
}

TEST_CASE("P1-05 6 invariant 5: the cell seams are monotone over random arguments (shipped tuning)") {
    worldgen::Rng rng(worldgen::substream(worldgen::stream_key(0, "test.resources"), 5));
    const resource_api::Tuning t = tuning();
    for (int k = 0; k < 4000; ++k) {
        const std::int64_t f = rng.uniform_i64(0, ONE);
        const std::int64_t a = rng.uniform_i64(0, 300000);
        const std::int64_t b = a + rng.uniform_i64(0, 300000);
        const std::int64_t hand = rng.uniform_i64(0, 30000);
        CAPTURE(f);
        CAPTURE(a);
        CAPTURE(b);
        CAPTURE(hand);
        CHECK(resource_api::fertility(f, a, hand, t) >= resource_api::fertility(f, b, hand, t));
        CHECK(resource_api::timber(f, a, t) >= resource_api::timber(f, b, t));
        const std::int64_t da = rng.uniform_i64(0, 32767);
        const std::int64_t db = da + rng.uniform_i64(0, 32767 - da);
        const std::int64_t rain = rng.uniform_i64(0, 10000);
        CHECK(resource_api::fresh_water(da, rain, t) >= resource_api::fresh_water(db, rain, t));
        CHECK(resource_api::forage(f, da, t) >= resource_api::forage(f, db, t));
        const auto u8 = [&] { return static_cast<std::uint8_t>(rng.uniform_i64(0, 255)); };
        const std::uint8_t F = u8(), W = u8(), G = u8(), T = u8(), O = u8();
        const auto walk = static_cast<std::uint16_t>(rng.uniform_i64(0, 3));
        const std::uint8_t h = resource_api::habitability(F, W, G, T, O, a, hand, walk, t);
        CHECK(h >= resource_api::habitability(F, W, G, T, O, b, hand, walk, t));
        CHECK(h >= resource_api::habitability(F, W, G, T, O, a, 0, walk, t));      // flood factor applies
        CHECK(h >= resource_api::habitability(F, W, G, T, O, a, hand, 0, t));      // pocket factor applies
        CHECK(h == own_habitability(F, W, G, T, O, a, hand, walk, values()));
        // Invariant 2 at the seams.
        const std::int64_t e = rng.uniform_i64(1, 30000);
        const std::int64_t noise = rng.uniform_i64(-ONE, 0);
        CHECK(resource_api::ore(e, a, noise, t) == 0);
        CHECK(resource_api::forage(f, da, t) >= own_q255(f));
    }
    // Ends of the slope ramps: 0 at and above the top.
    CHECK(resource_api::fertility(ONE, 22621, 0, t) == 0);  // tan(S2 22621) >= 0.47
    CHECK(resource_api::timber(ONE, 200704, t) == 0);       // tan = 1.4 exactly
    CHECK(resource_api::timber(ONE, 190000, t) > 0);
    CHECK(own_tangent(200704) == fx_of("1.4"));
    CHECK(resource_api::habitability(255, 255, 255, 255, 255, 22621, 300, 1, t) == 0);
    CHECK(resource_api::habitability(255, 255, 255, 255, 255, 22118, 300, 1, t) > 0);
    CHECK(own_tangent(22621) >= fx_of("0.47"));
    CHECK(own_tangent(22620) < fx_of("0.47"));
}

TEST_SUITE("resources-slow" * doctest::skip()) {

TEST_CASE("P1-05 10.2 (slow): seeds 0 to 63 at N = 128") {
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        check_world(seed, false);
    }
}

TEST_CASE("P1-05 9 (slow): hard statistics bounds at 512 (seeds 0 to 3) and 1024 (seed 0)") {
    struct World {
        std::uint64_t seed;
        std::int32_t n;
    };
    for (const World w : {World{0, 512}, World{1, 512}, World{2, 512}, World{3, 512}, World{0, 1024}}) {
        CAPTURE(w.seed);
        CAPTURE(w.n);
        const resource_api::Inputs in = from_biome_inputs(biome_test::world_inputs(w.seed, w.n, false), w.seed);
        const resource_api::Layers l = resource_api::layers(in, definitions(), tuning(), w.seed, 4);
        std::map<std::string, std::string> r;
        for (const auto& [k, val] : report_values(in.elevation, in.water, l)) {
            r[k] = val;
        }
        CAPTURE(r["habitable_share"]);
        CAPTURE(r["ore_rich_share"]);
        CAPTURE(r["walk_areas"]);
        CAPTURE(r["walk_main_share"]);
        CAPTURE(r["scores_p90"]);
        CHECK(r["habitable_share"] >= std::string("0.0500"));
        CHECK(r["ore_rich_share"] > std::string("0.0000"));
        CHECK(std::stoll(r["walk_areas"]) >= 1);
        if (w.n >= 1024) {
            CHECK(r["walk_main_share"] >= std::string("0.3000"));
        }
        const std::string p90 = r["scores_p90"];
        const std::size_t at = p90.find("habitability:");
        REQUIRE(at != std::string::npos);
        CHECK(std::stoll(p90.substr(at + 13)) >= 1);
    }
}

}  // TEST_SUITE
