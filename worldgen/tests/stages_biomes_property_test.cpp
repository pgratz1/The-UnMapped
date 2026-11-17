// Property tests of the biome stage (spec P1-04 section 6 invariants 1 to 4
// and 7, 10.2; P1-00 G7 with the minimum river class and G8).
//
// Written from the spec only. Each world is checked against the test's own
// rule evaluation (every rule of every definition, highest priority kept),
// its own G7 (recursion along flow_dir with the class test) and its own B5.
// Seeds 0 to 7 run by default; seeds 0 to 63 (P1-04 10.2) and the 512 and
// 1024 statistics run in the suite "biomes-slow":
//   worldgen-tests -ts=biomes-slow --no-skip

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "p1_02_hydrology_api.hpp"
#include "p1_02_hydrology_support.hpp"
#include "p1_04_biomes_api.hpp"
#include "p1_04_biomes_support.hpp"
#include "worldgen/core/rng.hpp"

using namespace biome_test;

namespace {

// Invariants 1 to 4 of one layer; "" or the first violation.
std::string violation(const biome_api::Inputs& in, std::span<const std::uint8_t> b, const std::vector<Def>& defs,
                      const Values& v, std::uint64_t seed) {
    const Grid g = in.grid;
    std::ostringstream err;
    std::map<std::uint8_t, const Def*> by_code;
    for (const Def& d : defs) {
        by_code[d.code] = &d;
    }
    const std::uint8_t fallback = fallback_of(defs);
    const auto s = surface(in.elevation, in.water_depth);
    const auto hand = own_hand(g, s, in.water, in.river, in.flow_dir, v.k);
    if (!hand.has_value()) {
        return "inputs violate G7";
    }
    const std::vector<std::int64_t> s2 = own_s2(g, in.elevation);
    const std::int64_t sc = scale_for(v, std::max(g.width, g.height));
    const auto nt = noise_grid(seed, STREAM_TJ, g, in.water, v.jitter);
    const auto nr = noise_grid(seed, STREAM_RJ, g, in.water, v.jitter);
    std::vector<std::uint8_t> own(g.cells());
    for (std::size_t i = 0; i < g.cells(); ++i) {
        if (in.water[i] >= WATER_LAKE) {
            continue;
        }
        const Jittered j = own_jitter(in.temperature[i], in.rainfall[i], nt[i], nr[i], v);
        own[i] = own_lookup_exhaustive(defs, sc, Cell{j.tq, j.rq, in.elevation[i], s2[i], (*hand)[i], in.coast_dist[i]});
    }
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const std::size_t i = g.index(x, y);
            const auto it = by_code.find(b[i]);
            if (b[i] == 0 || it == by_code.end()) {
                err << "inv 1: code " << int{b[i]} << " at (" << x << ", " << y << ")";
                return err.str();
            }
            const Def& d = *it->second;
            if ((d.water_body == "ocean") != (in.water[i] == WATER_OCEAN) ||
                (d.water_body == "lake") != (in.water[i] == WATER_LAKE)) {
                err << "inv 2: " << d.key << " on water " << int{in.water[i]} << " at (" << x << ", " << y << ")";
                return err.str();
            }
            if (in.water[i] <= WATER_RIVER && d.priority == 0 && d.code != fallback) {
                err << "inv 3: " << d.key << " at (" << x << ", " << y << ")";
                return err.str();
            }
            std::uint8_t want = own[i];
            if (in.water[i] == WATER_RIVER) {
                std::map<std::uint8_t, int> count;
                for (std::size_t k = 0; k < 8; ++k) {
                    const std::int32_t mx = x + DX[k], my = y + DY[k];
                    if (mx >= 0 && my >= 0 && mx < g.width && my < g.height && in.water[g.index(mx, my)] == WATER_DRY) {
                        ++count[own[g.index(mx, my)]];
                    }
                }
                int best = 0;
                for (const auto& [code, k] : count) {  // ascending code: ties to the smallest
                    if (k > best) {
                        best = k;
                        want = code;
                    }
                }
            }
            if (in.water[i] <= WATER_RIVER && b[i] != want) {
                err << "inv 4: " << int{b[i]} << " != " << int{want} << " at (" << x << ", " << y << "), water "
                    << int{in.water[i]};
                return err.str();
            }
        }
    }
    return {};
}

void check_world(std::uint64_t seed, std::int32_t n, bool small) {
    CAPTURE(seed);
    CAPTURE(n);
    const biome_api::Inputs in = world_inputs(seed, n, small);
    const std::vector<std::uint8_t> b = biome_api::layer(in, shipped_definitions(), tuning(), seed, 4);
    const std::string v = violation(in, b, shipped_defs(), values(), seed);
    CHECK_MESSAGE(v.empty(), v);
    CHECK(std::count(b.begin(), b.end(), BLIGHTED) == 0);
}

// The shipped definitions with every land rule over the full ranges of 4.1
// (temperature widened below by the jitter is still >= -60 on these
// worlds, which have no temperature near -59 C).
std::string full_range_rule(std::string_view id) {
    return "\n[[biome." + std::string(id) +
           ".rules]]\ntemp_min_c = -60\ntemp_max_c = 50\nrain_min_mm = 0\nrain_max_mm = 10000\nelev_min_m = 0\n"
           "elev_max_m = 3000\nslope_max = 100\nhand_max_m = 3000\ncoast_max_m = 32767\n";
}

}  // namespace

TEST_CASE("P1-04 6 invariants 1 to 4, 10.2: seeds 0 to 7 at N = 128, shipped and small-map fixtures") {
    for (std::uint64_t seed = 0; seed < 8; ++seed) {
        check_world(seed, 128, seed % 2 == 1);
    }
}

TEST_CASE("P1-04 6 invariants 1 to 4: the designed fixtures, with tuning variants") {
    for (std::uint64_t f = 0; f < 4; ++f) {
        CAPTURE(f);
        const biome_api::Inputs in = designed_inputs(f);
        for (const Overrides& o : {Overrides{}, Overrides{{"drain_river_class_min", "1"}},
                                   Overrides{{"drain_river_class_min", "15"}}, Overrides{{"rain_scale_512", "4"}},
                                   Overrides{{"rain_scale_512", "0.25"}, {"temp_jitter_c", "10"}, {"rain_jitter", "1"}}}) {
            CAPTURE(o.size());
            const std::vector<std::uint8_t> b = biome_api::layer(in, shipped_definitions(), tuning(o), f, 2);
            const std::string v = violation(in, b, shipped_defs(), values(o), f);
            CHECK_MESSAGE(v.empty(), v);
        }
    }
}

TEST_CASE("P1-04 10.2: with every rule over its full range, every dry land cell gets the highest-priority biome") {
    biome_data::DefinitionFile const* files = biome_data::shipped_files().data();
    std::vector<std::pair<std::string, std::string>> replace;
    for (std::size_t i = 0; i < biome_data::shipped_files().size(); ++i) {
        const std::string name(files[i].name);
        std::string text(files[i].text);
        const std::string id = name.substr(0, name.size() - 5);
        if (id == "grassland") {
            const std::size_t at = text.find("rules = []\n");
            REQUIRE(at != std::string::npos);
            text.erase(at, 11);
            text += full_range_rule(id);
        } else if (id == "temperate_forest" || id == "marsh" || id == "highland_moor" || id == "alpine") {
            text = text.substr(0, text.find("\n[[")) + "\n" + full_range_rule(id);
        } else {
            continue;
        }
        replace.emplace_back(name, text);
    }
    const biome_api::Load l = biome_api::load(std::string(biome_data::TUNING_TOML), biome_data::directory(replace));
    REQUIRE_MESSAGE(l.ok(), all_errors(l));
    const biome_api::Definitions defs = biome_api::definitions_from(*l.content);
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{3}}) {
        CAPTURE(seed);
        const biome_api::Inputs in = world_inputs(seed, 128, false);
        const std::vector<std::uint8_t> b = biome_api::layer(in, defs, tuning(), seed, 1);
        for (std::size_t i = 0; i < b.size(); ++i) {
            if (in.water[i] == WATER_DRY) {
                REQUIRE(b[i] == ALPINE);
            } else if (in.water[i] == WATER_OCEAN) {
                REQUIRE(b[i] == OCEAN);
            } else if (in.water[i] == WATER_LAKE) {
                REQUIRE(b[i] == LAKE);
            }
        }
    }
    // Lowering alpine below grassland's priority makes grassland the first.
    std::string alpine = replace.back().second;
    const std::size_t at = alpine.find("priority = 50");
    REQUIRE(at != std::string::npos);
    alpine.replace(at, 13, "priority = 5");
    replace.back().second = alpine;
    const biome_api::Load l2 = biome_api::load(std::string(biome_data::TUNING_TOML), biome_data::directory(replace));
    REQUIRE_MESSAGE(l2.ok(), all_errors(l2));
    const biome_api::Inputs in = designed_inputs(0);
    const std::vector<std::uint8_t> b = biome_api::layer(in, biome_api::definitions_from(*l2.content), tuning(), 0, 1);
    for (std::size_t i = 0; i < b.size(); ++i) {
        if (in.water[i] == WATER_DRY) {
            REQUIRE(b[i] == MARSH);  // marsh 40 is now the highest
        }
    }
}

TEST_CASE("P1-04 6 invariant 7, B4: only the scale entry of N applies; nothing else depends on N") {
    // A 520 x 520 synthetic island (N = 520 uses rain_scale_1024): land
    // rising east, every land cell draining west to the sea; rain across the
    // forest and marsh bounds.
    biome_api::Inputs in;
    const std::int32_t n = 520;
    in.grid = Grid{n, n};
    for (std::int32_t y = 0; y < n; ++y) {
        for (std::int32_t x = 0; x < n; ++x) {
            const bool sea = x < 4 || y < 4 || x >= n - 4 || y >= n - 4;
            in.elevation.push_back(static_cast<std::int16_t>(sea ? -30 : 1 + (x / 8) + (y % 3)));
            in.water.push_back(sea ? WATER_OCEAN : WATER_DRY);
            in.water_depth.push_back(static_cast<std::uint16_t>(sea ? 30 : 0));
            in.flow_dir.push_back(static_cast<std::uint8_t>(sea ? 8 : 4));
            in.river.push_back(0);
            in.temperature.push_back(80);
            in.rainfall.push_back(static_cast<std::uint16_t>(400 + (y * 600) / n));
        }
    }
    std::vector<std::uint8_t> ocean(in.grid.cells());
    for (std::size_t i = 0; i < ocean.size(); ++i) {
        ocean[i] = in.water[i] == WATER_OCEAN ? 1 : 0;
    }
    in.coast_dist = hydro_api::coast_distance(in.grid, ocean);
    const auto at = [&](const Overrides& o) { return biome_api::layer(in, shipped_definitions(), tuning(o), 11, 4); };
    const std::vector<std::uint8_t> equal = at({{"rain_scale_512", "1"}, {"rain_scale_1024", "1"}, {"rain_scale_2048", "1"}});
    CHECK(at({{"rain_scale_512", "0.5"}, {"rain_scale_1024", "1"}, {"rain_scale_2048", "2"}}) == equal);
    CHECK(at({{"rain_scale_1024", "0.5"}}) != equal);
    CHECK(equal == own_biomes(in, shipped_defs(),
                              values({{"rain_scale_512", "1"}, {"rain_scale_1024", "1"}, {"rain_scale_2048", "1"}}), 11)
                       .biome);
    // On a 128 grid only rain_scale_512 applies.
    const biome_api::Inputs w = world_inputs(0, 128, false);
    const auto at128 = [&](const Overrides& o) { return biome_api::layer(w, shipped_definitions(), tuning(o), 0, 4); };
    const std::vector<std::uint8_t> e128 =
        at128({{"rain_scale_512", "1.125"}, {"rain_scale_1024", "1.125"}, {"rain_scale_2048", "1.125"}});
    CHECK(at128({}) == e128);
    CHECK(at128({{"rain_scale_1024", "3"}, {"rain_scale_2048", "0.25"}}) == e128);
}

// --- G7 with the minimum class against the test's own (P1-00 7.7) ------------------------------

TEST_CASE("P1-00 G7: the minimum river class k on the random hydrology fixtures; k = 1 is the definition before the amendment") {
    for (std::uint64_t f = 0; f < 12; ++f) {
        CAPTURE(f);
        const hydro_test::Fixture fx = hydro_test::random_fixture(f);
        const hydro_api::Layers h = hydro_api::layers_tie(fx.grid, fx.e, hydro_test::random_tie(f, fx.e),
                                                          hydro_test::tuning(hydro_test::RANDOM_AREA), 1);
        const std::vector<std::int64_t> s = surface(fx.e, h.water_depth);
        std::uint8_t max_class = 0;
        for (const std::uint8_t c : h.river) {
            max_class = std::max(max_class, c);
        }
        for (std::int64_t k = 1; k <= 15; ++k) {
            CAPTURE(k);
            const auto own = own_hand(fx.grid, s, h.water, h.river, h.flow_dir, k);
            REQUIRE(own.has_value());
            const std::vector<std::int64_t> got = biome_api::hand(fx.grid, s, h.water, h.river, h.flow_dir, k);
            CHECK(got == *own);
            // Drain cells have 0; for k above every class only water cells.
            for (std::size_t i = 0; i < got.size(); ++i) {
                if (is_drain(h.water[i], h.river[i], k)) {
                    CHECK(got[i] == 0);
                }
                CHECK(got[i] >= 0);
                CHECK(got[i] <= 30000);
            }
            if (k > max_class) {
                // Rivers no longer drains: equal to k = 15.
                CHECK(got == biome_api::hand(fx.grid, s, h.water, h.river, h.flow_dir, 15));
            }
        }
        // k = 1: every river cell is a drain (the pre-amendment G7).
        std::vector<std::uint8_t> all(h.river.size());
        for (std::size_t i = 0; i < all.size(); ++i) {
            all[i] = h.water[i] == WATER_RIVER ? 15 : 0;
        }
        CHECK(biome_api::hand(fx.grid, s, h.water, h.river, h.flow_dir, 1) ==
              own_hand(fx.grid, s, h.water, all, h.flow_dir, 15).value());
        // Monotone in k: fewer drains never lower a height.
        const auto h1 = biome_api::hand(fx.grid, s, h.water, h.river, h.flow_dir, 1);
        const auto h3 = biome_api::hand(fx.grid, s, h.water, h.river, h.flow_dir, 3);
        for (std::size_t i = 0; i < h1.size(); ++i) {
            CHECK(h1[i] <= h3[i]);
        }
    }
}

// --- G8 threshold exactness (P1-00 7.8) -------------------------------------------------------

TEST_CASE("P1-00 G8, P1-04 B4: a rule's slope_max admits exactly the S2 not steeper than it") {
    worldgen::Rng rng(worldgen::stream_key(1, STREAM_TEST));
    std::vector<std::int64_t> ts = {0, 1, 214748365, 4294967296, fx_of("0.7002"), fx_of("99.999999999")};
    for (int i = 0; i < 6; ++i) {
        ts.push_back(rng.uniform_i64(1, 100 * ONE));
    }
    for (const std::int64_t t : ts) {
        CAPTURE(t);
        // The largest S2 not steeper than t, by bisection on the test's own
        // exact comparison.
        std::int64_t lo = 0, hi = std::int64_t{1} << 34;
        if (!not_steeper(0, t)) {
            FAIL("0 is steeper");
        }
        while (hi - lo > 1) {
            const std::int64_t mid = lo + (hi - lo) / 2;
            (not_steeper(mid, t) ? lo : hi) = mid;
        }
        std::string marsh(biome_data::shipped_files()[4].text);
        const std::size_t at = marsh.find("slope_max = \"0.05\"");
        REQUIRE(at != std::string::npos);
        marsh.replace(at, 18, "slope_max = \"" + Fx::from_raw(t).to_decimal(18) + "\"");
        const biome_api::Load l =
            biome_api::load(std::string(biome_data::TUNING_TOML), biome_data::directory({{"marsh.toml", marsh}}));
        REQUIRE_MESSAGE(l.ok(), all_errors(l));
        const biome_api::Definitions d = biome_api::definitions_from(*l.content);
        if (lo < (std::int64_t{1} << 33)) {
            CHECK(biome_api::lookup(d, ONE, 8 * ONE, 900 * ONE, 30, lo, 0, 100) == MARSH);
            CHECK(biome_api::lookup(d, ONE, 8 * ONE, 900 * ONE, 30, lo + 1, 0, 100) == FOREST);
        }
    }
}

// --- Slow (opt-in) ----------------------------------------------------------------------------

TEST_SUITE("biomes-slow" * doctest::skip()) {

TEST_CASE("P1-04 10.2 (slow): seeds 0 to 63 at N = 128") {
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        check_world(seed, 128, false);
    }
}

TEST_CASE("P1-04 9 (slow): hard statistics bounds at 512 (seeds 0 to 3) and 1024 (seed 0)") {
    // Only the bounds the spec marks hard ("All seeds (hard)"): forest,
    // grassland, moor, alpine <= 0.80 of land; marsh <= 0.15; blighted 0.
    for (const auto& [seed, n] : std::vector<std::pair<std::uint64_t, std::int32_t>>{{0, 512}, {1, 512}, {2, 512},
                                                                                   {3, 512}, {0, 1024}}) {
        CAPTURE(seed);
        CAPTURE(n);
        const biome_api::Inputs in = world_inputs(seed, n, false);
        const std::vector<std::uint8_t> b = biome_api::layer(in, shipped_definitions(), tuning(), seed, 4);
        std::map<std::uint8_t, std::uint64_t> count;
        std::uint64_t land = 0;
        for (const std::uint8_t v : b) {
            if (v >= FOREST) {
                ++count[v];
                ++land;
            }
        }
        REQUIRE(land > 0U);
        for (const std::uint8_t c : {FOREST, GRASSLAND, MOOR, ALPINE}) {
            CHECK(count[c] * 100U <= land * 80U);
        }
        CHECK(count[MARSH] * 100U <= land * 15U);
        CHECK(count[BLIGHTED] == 0U);
        // Provisional ranges of section 9 are reported, not asserted.
        MESSAGE("seed " << seed << " N " << n << " forest " << count[FOREST] * 10000U / land << " grass "
                        << count[GRASSLAND] * 10000U / land << " marsh " << count[MARSH] * 10000U / land << " moor "
                        << count[MOOR] * 10000U / land << " alpine " << count[ALPINE] * 10000U / land << " (1e-4)");
    }
}

}  // TEST_SUITE
