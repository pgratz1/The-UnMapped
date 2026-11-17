// Property tests of the climate stage: the invariants of spec P1-03 section
// 6 over seeds, map sizes, wind codes and the random fixtures of 10.2, by
// code independent of the stage (p1_03_climate_support.hpp), and the
// stream registrations of section 2.
//
// 10.2 asks for seeds 0 to 63 at N = 128 and 200 random fixtures. The 200
// random fixtures (34 x 34) run by default; of the worlds, seeds 0 to 3 of
// the shipped fixture and 0 to 2 of the small-map fixture run by default and
// the full 0 to 63 of both, plus N = 512 seeds 0 to 3, in the suite
// "climate-slow" (skipped by default: worldgen-tests -ts=climate-slow
// --no-skip), to keep the default test time low. The provisional statistics
// of P1-03 9 are reported there as information; only the bounds marked hard
// (both "by construction") are asserted.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "p1_03_climate_api.hpp"
#include "p1_03_climate_support.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/core/streams.hpp"

using namespace climate_test;

namespace {

// Invariants 1, 3 and 4 of one world under one tuning, through the stage's
// layers and seams; and the stage equals the test's own C2 to C6.
void check_world(const Inputs& in, const Overrides& o, std::uint64_t seed) {
    const Values v = values(o);
    const climate_api::Tuning t = tuning(o);
    const climate_api::Layers l = stage_layers(in, t, seed, THREADS);
    const std::uint8_t wind = climate_api::wind(seed, v.wind_weights);
    const std::vector<std::int64_t> s = surface(in);
    const std::vector<std::int64_t> hs =
        climate_api::box_mean(in.grid, s, static_cast<std::int32_t>(v.smooth_radius_m / 16), THREADS);
    const climate_api::Sweep sw = climate_api::sweep(in.grid, s, in.water, hs, wind, t);
    const std::string bad = invariant_violation(in, l, hs, sw, wind, v);
    CHECK_MESSAGE(bad.empty(), bad);
    const Own own = own_climate(in, v, seed);
    CHECK(own.wind == wind);
    CHECK(hs == own.hs);
    CHECK(sw.q == own.sweep.q);
    CHECK(sw.p == own.sweep.p);
    CHECK(l.temperature == own.temperature);
    CHECK(l.rainfall == own.rainfall);
}

// Invariant 2 with temp_noise_amp_c = 0: within a row, a higher surface is
// never warmer; at equal surface, temperature never falls from north to
// south when temp_south_c >= temp_north_c.
std::string lapse_violation(const Inputs& in, const climate_api::Layers& l) {
    const Grid g = in.grid;
    const std::vector<std::int64_t> s = surface(in);
    for (std::int32_t y = 0; y < g.height; ++y) {
        std::vector<std::pair<std::int64_t, std::int16_t>> row;
        for (std::int32_t x = 0; x < g.width; ++x) {
            row.emplace_back(s[g.index(x, y)], l.temperature[g.index(x, y)]);
        }
        std::sort(row.begin(), row.end());
        for (std::size_t i = 1; i < row.size(); ++i) {
            // S ascending; temperature must not rise with S (equal S equal T).
            if (row[i].second > row[i - 1].second) {
                return "row " + std::to_string(y) + ": S " + std::to_string(row[i].first) + " warmer than S " +
                       std::to_string(row[i - 1].first);
            }
        }
    }
    std::map<std::int64_t, std::int16_t> last;
    for (std::size_t i = 0; i < s.size(); ++i) {  // index order: y ascending
        const auto it = last.find(s[i]);
        if (it != last.end() && l.temperature[i] < it->second) {
            return "S " + std::to_string(s[i]) + ": colder further south at cell " + std::to_string(i);
        }
        last[s[i]] = l.temperature[i];
    }
    return {};
}

// Invariant 5: with rain_oro_mm = 0 and both noise amplitudes 0, land
// rainfall does not depend on the terrain: change every land surface height
// (keeping water, depth and coast_dist) and compare.
void check_no_terrain_effect(const Inputs& in, std::uint8_t wind, std::uint64_t seed) {
    Overrides o = {{"rain_oro_mm", "0"}, {"temp_noise_amp_c", "0"}, {"rain_noise_amp", "0"},
                   {"wind_weights", weights_text(one_hot(wind))}};
    const climate_api::Layers a = stage_layers(in, tuning(o), seed, THREADS);
    Inputs changed = in;
    for (std::size_t i = 0; i < changed.elevation.size(); ++i) {
        if (changed.water[i] != WATER_OCEAN && changed.water[i] != WATER_LAKE) {
            changed.elevation[i] = static_cast<std::int16_t>(1 + (std::int64_t{changed.elevation[i]} * 7 + 13) % 3000);
        }
    }
    const climate_api::Layers b = stage_layers(changed, tuning(o), seed + 1000, THREADS);
    std::size_t differ = 0;
    for (std::size_t i = 0; i < a.rainfall.size(); ++i) {
        if (in.elevation[i] > 0 && a.rainfall[i] != b.rainfall[i]) {
            ++differ;
        }
    }
    CHECK(differ == 0U);
    // And with the orographic term on, the same change does move the rain.
    const climate_api::Layers c = stage_layers(in, tuning({{"wind_weights", weights_text(one_hot(wind))}}), seed, THREADS);
    const climate_api::Layers d =
        stage_layers(changed, tuning({{"wind_weights", weights_text(one_hot(wind))}}), seed, THREADS);
    CHECK(c.rainfall != d.rainfall);
}

// Every invariant of section 6 on one world.
void check_all(const Inputs& in, std::uint64_t seed) {
    check_world(in, {}, seed);
    // Invariant 2 needs temp_noise_amp_c = 0.
    const climate_api::Layers l0 = stage_layers(in, tuning({{"temp_noise_amp_c", "0"}}), seed, THREADS);
    const std::string bad = lapse_violation(in, l0);
    CHECK_MESSAGE(bad.empty(), bad);
}

}  // namespace

TEST_CASE("P1-03 2, P1-00 3: the three climate streams and the test stream are registered") {
    const auto& names = worldgen::streams::all_stream_names;
    for (const std::string_view want : {STREAM_WIND, STREAM_TEMP, STREAM_RAIN, STREAM_TEST}) {
        CAPTURE(std::string(want));
        CHECK(std::find(names.begin(), names.end(), want) != names.end());
    }
}

TEST_CASE("P1-03 6, 10.2: invariants 1, 3, 4 on 200 random fixtures (34 x 34, S in [1, 3000], random wind)") {
    std::vector<int> codes(8, 0);
    for (std::uint64_t f = 0; f < 200; ++f) {
        CAPTURE(f);
        const RandomFixture rf = random_fixture(f);
        codes[rf.wind] += 1;
        check_world(rf.in, wind_overrides(rf.wind), f);
    }
    for (std::size_t k = 0; k < 8; ++k) {
        CAPTURE(k);
        CHECK(codes[k] >= 10);
    }
}

TEST_CASE("P1-03 6 invariants 2 and 5, 10.2: lapse and no terrain effect without orographic rain (random fixtures)") {
    for (std::uint64_t f = 0; f < 40; ++f) {
        CAPTURE(f);
        const RandomFixture rf = random_fixture(f);
        Overrides o = wind_overrides(rf.wind);
        o.emplace_back("temp_noise_amp_c", "0");
        const std::string bad = lapse_violation(rf.in, stage_layers(rf.in, tuning(o), f, THREADS));
        CHECK_MESSAGE(bad.empty(), bad);
        check_no_terrain_effect(rf.in, rf.wind, f);
    }
}

TEST_CASE("P1-03 6 invariant 2: with temp_south_c < temp_north_c the lapse part still holds; equal S warms south otherwise") {
    const RandomFixture rf = random_fixture(11);
    const climate_api::Layers rev =
        stage_layers(rf.in, tuning({{"temp_noise_amp_c", "0"}, {"temp_north_c", "12"}, {"temp_south_c", "-4"}}), 11,
                     THREADS);
    // Within rows, higher is never warmer whatever the latitude span.
    const Grid g = rf.in.grid;
    const std::vector<std::int64_t> s = surface(rf.in);
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t a = 0; a < g.width; ++a) {
            for (std::int32_t b = 0; b < g.width; ++b) {
                if (s[g.index(a, y)] >= s[g.index(b, y)]) {
                    REQUIRE(rev.temperature[g.index(a, y)] <= rev.temperature[g.index(b, y)]);
                }
            }
        }
    }
    // The ocean border: S = 0 on every ocean cell, so its temperature is the
    // sea-level value of its row, colder southward in the reversed span.
    CHECK(rev.temperature[g.index(0, 0)] > rev.temperature[g.index(0, g.height - 1)]);
}

TEST_CASE("P1-03 6: invariants on worlds from the elevation and hydrology stages, N = 128 (seeds 0-3 shipped, 0-2 small)") {
    for (const bool small : {false, true}) {
        for (std::uint64_t seed = 0; seed < (small ? 3U : 4U); ++seed) {
            CAPTURE(small);
            CAPTURE(seed);
            check_all(world_inputs(seed, 128, small), seed);
        }
    }
}

TEST_CASE("P1-03 6: invariants on non-power-of-two maps (N = 48 and 100) and every forced wind on one world") {
    for (const std::int32_t n : {48, 100}) {
        CAPTURE(n);
        check_all(world_inputs(3, n, false), 3);
    }
    const Inputs in = world_inputs(2, 64, true);
    for (std::uint8_t k = 0; k < 8; ++k) {
        CAPTURE(int{k});
        check_world(in, wind_overrides(k), 2);
        check_no_terrain_effect(in, k, 2);
    }
}

TEST_CASE("P1-03 6: invariants at the extremes of the tuning bounds") {
    const RandomFixture rf = random_fixture(17);
    const std::vector<Overrides> extremes = {
        {{"rain_oro_mm", "100000"}, {"loss_max", "\"0.9\""}, {"moisture_path_mm_km", "100"}},  // air wrung out at once
        {{"rain_oro_mm", "0"}, {"loss_max", "0"}},                                         // air never dries
        {{"ocean_recharge_m", "16"}, {"smooth_radius_m", "0"}},                            // full recharge in one step
        {{"ocean_recharge_m", "100000"}, {"smooth_radius_m", "4000"}},                     // whole-grid box
        {{"rain_base_mm", "0"}, {"coast_rain_mm", "5000"}, {"coast_rain_reach_m", "32767"}, {"rain_min_mm", "0"}},
        {{"rain_base_mm", "5000"}, {"rain_noise_amp", "1"}, {"rain_min_mm", "5000"}},
        {{"temp_north_c", "-30"}, {"temp_south_c", "40"}, {"lapse_c_per_km", "15"}, {"temp_noise_amp_c", "5"}},
    };
    for (std::size_t i = 0; i < extremes.size(); ++i) {
        CAPTURE(i);
        for (std::uint8_t k : {std::uint8_t{0}, std::uint8_t{3}, std::uint8_t{6}}) {
            CAPTURE(int{k});
            check_world(rf.in, elev_test::concat(extremes[i], wind_overrides(k)), 17);
        }
    }
    // loss_max 0: the air never dries over land, so q = q_up on land and the
    // ocean only recharges: every q stays at ONE.
    const Overrides dry0 = {{"loss_max", "0"}, {"wind_weights", weights_text(one_hot(1))}};
    const std::vector<std::int64_t> s = surface(rf.in);
    const std::vector<std::int64_t> hs = climate_api::box_mean(rf.in.grid, s, 20, THREADS);
    const climate_api::Sweep sw = climate_api::sweep(rf.in.grid, s, rf.in.water, hs, 1, tuning(dry0));
    CHECK(std::all_of(sw.q.begin(), sw.q.end(), [](std::int64_t q) { return q == ONE; }));
}

TEST_CASE("P1-03 C5, 6 invariant 4: a ridge across the wind casts a rain shadow") {
    // A designed 40 x 9 strip: ocean in columns 0 and 39, a ridge (S up to
    // 600 m) in columns 10 to 14, lowland (S = 50) elsewhere; wind E.
    const Grid g{40, 9};
    std::vector<std::int64_t> s(g.cells());
    std::vector<std::uint8_t> water(g.cells());
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const bool sea = x == 0 || x == 39;
            water[g.index(x, y)] = sea ? WATER_OCEAN : WATER_DRY;
            const std::int64_t ridge = x >= 10 && x <= 14 ? 6000 - 1400 * std::abs(x - 12) : 0;
            s[g.index(x, y)] = sea ? 0 : 50 + ridge;
        }
    }
    const climate_api::Tuning t = tuning(elev_test::concat(quiet_overrides(), {{"smooth_radius_m", "0"}}));
    std::vector<std::int64_t> hs;
    for (const std::int64_t v : s) {
        hs.push_back(Fx::from_int(v).raw());
    }
    const climate_api::Sweep sw = climate_api::sweep(g, s, water, hs, 0, t);
    const std::size_t y = 4;
    auto at = [&](std::int32_t x) { return g.index(x, static_cast<std::int32_t>(y)); };
    // Windward slope wetter than the lowland before it; the lee drier than both.
    CHECK(sw.p[at(11)] > sw.p[at(5)]);
    CHECK(sw.p[at(13)] < sw.p[at(5)]);
    CHECK(sw.p[at(20)] < sw.p[at(5)]);
    // Along a row q may recover in the lee: the off-map taps above and below
    // the strip feed saturated air in sideways (C5: "A tap outside the map
    // has q = ONE"); invariant 4 is per cell, q <= q_up.
    CHECK(sw.q[at(12)] < sw.q[at(9)]);
}

// --- Slow checks (opt-in) ------------------------------------------------------------------

TEST_SUITE("climate-slow" * doctest::skip()) {

TEST_CASE("P1-03 10.2 (slow): invariants, N = 128 seeds 0 to 63 with both fixtures, N = 512 seeds 0 to 3") {
    for (const bool small : {false, true}) {
        for (std::uint64_t seed = 0; seed < 64; ++seed) {
            CAPTURE(small);
            CAPTURE(seed);
            check_all(world_inputs(seed, 128, small), seed);
        }
    }
    for (std::uint64_t seed = 0; seed < 4; ++seed) {
        CAPTURE(seed);
        check_all(world_inputs(seed, 512, false), seed);
    }
}

TEST_CASE("P1-03 9 (slow): statistics at N = 512 seeds 0 to 9 and N = 1024 seeds 0 to 3; hard bounds asserted, the rest reported") {
    // Hard (P1-03 9, P1-00 13.1 second pass): temperature_c p50 within the
    // catalogue range and rainfall_mm p50 >= rain_min_mm, both by
    // construction. Typical ranges (3.0 to 9.0 C, 600 to 1200 mm,
    // rain_contrast 1.5 to 10) are provisional: printed, not asserted.
    struct Row {
        std::int32_t n;
        std::uint64_t seed;
        std::vector<std::pair<std::string, std::string>> report;
    };
    std::vector<Row> rows;
    for (const auto& [n, count] : {std::pair{512, 10}, std::pair{1024, 4}}) {
        for (std::uint64_t seed = 0; seed < static_cast<std::uint64_t>(count); ++seed) {
            CAPTURE(n);
            CAPTURE(seed);
            const Inputs in = world_inputs(seed, n, false);
            const climate_api::Layers l = stage_layers(in, shipped_tuning(), seed, THREADS);
            const std::vector<std::int64_t> tp = land_percentiles<std::int16_t>(in.elevation, l.temperature);
            const std::vector<std::int64_t> rp = land_percentiles<std::uint16_t>(in.elevation, l.rainfall);
            REQUIRE(tp.size() == 5U);
            CHECK(tp[2] >= TEMP_MIN);
            CHECK(tp[2] <= TEMP_MAX);
            CHECK(rp[2] >= 250);
            rows.push_back(Row{n, seed, report_values(in.elevation, l)});
        }
    }
    std::string text = "P1-03 9 statistics (provisional typical: temperature_c p50 3.0-9.0, rainfall_mm p50 600-1200, "
                       "rain_contrast 1.5-10):\n";
    for (const Row& r : rows) {
        text += "  N " + std::to_string(r.n) + " seed " + std::to_string(r.seed) + ":";
        for (const auto& [k, v] : r.report) {
            text += " " + k + " = " + v + ";";
        }
        text += "\n";
    }
    MESSAGE(text);
}

}  // TEST_SUITE
