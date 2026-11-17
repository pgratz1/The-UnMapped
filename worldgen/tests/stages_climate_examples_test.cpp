// Worked examples and unit tests of the climate stage (spec P1-03 section
// 5.1 and 10.1 tests 1 to 4).
//
// Written from the spec only. Every stage call goes through
// p1_03_climate_api.hpp; the test's own implementation of C2 to C6 is in
// p1_03_climate_support.hpp. Each worked example is checked through the
// stage's seam and, where 5.1 gives raw intermediates, through the test's
// own code as well, so a failure says whether the spec's number or the
// stage disagrees. The same numbers are reproduced by
// reference/p1_climate_reference.py check.

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "p1_03_climate_api.hpp"
#include "p1_03_climate_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/noise.hpp"
#include "worldgen/core/rng.hpp"

using namespace climate_test;

namespace {

// A raw Fx value in mm to three decimals (round half away from zero), as
// 5.1 prints p.
std::string mm3(std::int64_t raw) {
    const bool neg = raw < 0;
    const std::int64_t a = neg ? -raw : raw;
    const std::int64_t num = a * 1000;  // |p| < 2^44 here, so no overflow
    std::int64_t v = num >> 32;
    if (2 * (num & (ONE - 1)) >= ONE) {
        ++v;
    }
    std::string frac = std::to_string(v % 1000);
    frac.insert(0, 3U - frac.size(), '0');
    return (neg ? "-" : "") + std::to_string(v / 1000) + "." + frac;
}

// The 6 x 3 sweep grid of 5.1: columns 0 and 5 ocean (S = 0).
struct SweepGrid {
    Grid g{6, 3};
    std::vector<std::int64_t> s;
    std::vector<std::uint8_t> water;
    std::vector<std::int64_t> hs;  // r = 0: hs = from_int(S)
};

SweepGrid sweep_grid() {
    SweepGrid sg;
    sg.s = {0, 20, 60, 120, 80, 0, 0, 30, 90, 160, 100, 0, 0, 10, 40, 80, 60, 0};
    for (std::size_t i = 0; i < sg.s.size(); ++i) {
        const std::size_t x = i % 6;
        sg.water.push_back(x == 0 || x == 5 ? WATER_OCEAN : WATER_DRY);
        sg.hs.push_back(Fx::from_int(sg.s[i]).raw());
    }
    return sg;
}

const std::vector<std::vector<std::int64_t>>& sweep_q() {
    static const std::vector<std::vector<std::int64_t>> q = {
        {4294967296, 4288846968, 4280922232, 4270788875, 4271670939, 4276491274},
        {4294967296, 4287504790, 4276053289, 4263755588, 4265212810, 4269935559},
        {4294967296, 4290189145, 4283599746, 4276126894, 4274335696, 4277781016},
    };
    return q;
}

const std::vector<std::vector<std::string>>& sweep_p() {
    static const std::vector<std::vector<std::string>> p = {
        {"800.000", "1425.000", "2125.684", "2901.415", "796.295", "796.445"},
        {"800.000", "1737.500", "2905.002", "3599.202", "795.090", "795.183"},
        {"800.000", "1112.500", "1657.731", "1967.095", "796.792", "796.693"},
    };
    return p;
}

const std::vector<std::vector<std::uint16_t>>& sweep_rainfall() {
    static const std::vector<std::vector<std::uint16_t>> r = {
        {950, 1573, 2274, 3050, 945, 946},
        {950, 1886, 3053, 3748, 943, 945},
        {950, 1261, 1806, 2115, 945, 947},
    };
    return r;
}

template <typename T>
std::vector<std::vector<T>> rows_of(std::int32_t w, const std::vector<T>& v) {
    std::vector<std::vector<T>> out;
    for (std::size_t i = 0; i < v.size(); i += static_cast<std::size_t>(w)) {
        out.emplace_back(v.begin() + static_cast<std::ptrdiff_t>(i),
                         v.begin() + static_cast<std::ptrdiff_t>(i + static_cast<std::size_t>(w)));
    }
    return out;
}

}  // namespace

// --- C2 (5.1 "C2 wind", 10.1 test 5 first part) ------------------------------------------

TEST_CASE("P1-03 5.1 C2: shipped weights give k_w 1, 3, 4, 1, 1, 6, 0, 2, 5, 1, 2, 1 for seeds 0 to 11 and 6 for seed 42") {
    const std::vector<std::uint8_t> want = {1, 3, 4, 1, 1, 6, 0, 2, 5, 1, 2, 1};
    for (std::uint64_t seed = 0; seed < 12; ++seed) {
        CAPTURE(seed);
        CHECK(own_wind(seed, UNIFORM) == want[seed]);
        CHECK(climate_api::wind(seed, UNIFORM) == want[seed]);
    }
    CHECK(own_wind(42, UNIFORM) == 6);
    CHECK(climate_api::wind(42, UNIFORM) == 6);
    // One weighted_index call: the first output of Rng(K_wind), mapped by
    // uniform_u64(8) (P0-01 6.3), and exactly one draw for seeds 0 to 11.
    for (std::uint64_t seed = 0; seed < 12; ++seed) {
        worldgen::Rng a(worldgen::stream_key(seed, STREAM_WIND));
        worldgen::Rng b(worldgen::stream_key(seed, STREAM_WIND));
        const std::uint64_t first = b.next();
        CAPTURE(seed);
        CHECK(a.uniform_u64(8) == static_cast<std::uint64_t>(want[seed]));
        CHECK(a.next() == b.next());  // a consumed exactly one output
        static_cast<void>(first);
    }
}

TEST_CASE("P1-03 5.1 C2, 10.1 test 5: weights with a single non-zero entry always give that code") {
    for (std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{7}, std::uint64_t{42},
                               std::uint64_t{0xffffffffffffffffULL}}) {
        CAPTURE(seed);
        const std::array<std::int64_t, 8> four = {0, 0, 0, 0, 5, 0, 0, 0};
        CHECK(climate_api::wind(seed, four) == 4);
        for (std::uint8_t k = 0; k < 8; ++k) {
            CAPTURE(int{k});
            CHECK(climate_api::wind(seed, one_hot(k)) == k);
            std::array<std::int64_t, 8> big{};
            big[k] = 1000;
            CHECK(climate_api::wind(seed, big) == k);
        }
    }
    // A zero weight is never drawn, whatever the seed.
    const std::array<std::int64_t, 8> two = {0, 3, 0, 0, 0, 0, 0, 1};
    for (std::uint64_t seed = 0; seed < 64; ++seed) {
        const std::uint8_t k = climate_api::wind(seed, two);
        CAPTURE(seed);
        CHECK((k == 1 || k == 7));
        CHECK(k == own_wind(seed, two));
    }
}

TEST_CASE("P1-03 C2: the wind depends only on the seed and the weights (not on N or the terrain)") {
    // The same seed on two different grids and terrains: the moisture field
    // follows the same wind. Checked through climate_layers on a flat grid,
    // where the wind direction shows only through which edge is upwind: the
    // own implementation with k_w = own_wind(seed) reproduces the stage.
    const Values v = values(quiet_overrides());
    for (std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{5}, std::uint64_t{6}}) {
        for (const Grid g : {Grid{9, 7}, Grid{20, 20}}) {
            CAPTURE(seed);
            CAPTURE(g.width);
            std::vector<std::int64_t> s(g.cells());
            std::vector<std::uint8_t> water(g.cells());
            std::vector<std::int16_t> coast(g.cells(), 100);
            for (std::int32_t y = 0; y < g.height; ++y) {
                for (std::int32_t x = 0; x < g.width; ++x) {
                    const bool edge = x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1;
                    water[g.index(x, y)] = edge ? WATER_OCEAN : WATER_DRY;
                    s[g.index(x, y)] = edge ? 0 : 50 + 13 * ((x * 7 + y * 3) % 11);
                }
            }
            const Inputs in = inputs_of_surface(g, s, water, coast);
            const Own own = own_climate(in, v, seed);
            const climate_api::Layers l = stage_layers(in, tuning(quiet_overrides()), seed, THREADS);
            CHECK(l.rainfall == own.rainfall);
            CHECK(l.temperature == own.temperature);
        }
    }
}

// --- C3 (5.1 "C3 temperature", 10.1 test 2) -----------------------------------------------

TEST_CASE("P1-03 5.1 C3: the temperature table (temp_noise_amp_c 0)") {
    struct Row {
        std::int32_t n, y;
        std::int64_t s, t_sea, lap;
        std::int16_t temperature;
    };
    const std::vector<Row> rows = {
        {1024, 511, 12000, 30056382464, 33500744909, -8}, {1024, 0, 0, 21483225088, 0, 50},
        {1024, 1023, 0, 38646317056, 0, 90},              {512, 0, 30000, 21491613696, 83751862272, -145},
        {512, 255, 5, 30047993856, 13958644, 70},         {512, 100, 7000, 24847056896, 19542101197, 12},
    };
    const Values v = values(quiet_overrides());
    CHECK(v.lapse_c_per_km == 27917287424);
    const climate_api::Tuning t = tuning(quiet_overrides());
    for (const Row& r : rows) {
        CAPTURE(r.n);
        CAPTURE(r.y);
        CAPTURE(r.s);
        const TempParts own = own_temperature(r.y, r.n, r.s, 0, v);
        CHECK(own.t_sea == r.t_sea);
        CHECK(own.lap == r.lap);
        CHECK(own.temperature == r.temperature);
        CHECK(climate_api::temperature_cell(r.y, r.n, r.s, 0, t) == r.temperature);
        // With amplitude 0 the noise value has no effect.
        CHECK(climate_api::temperature_cell(r.y, r.n, r.s, ONE, t) == r.temperature);
        CHECK(climate_api::temperature_cell(r.y, r.n, r.s, -ONE, t) == r.temperature);
    }
    // By hand, first row: t_sea = 5 + 4 x 1023/2048 = 6.998046875 exactly.
    CHECK(rows[0].t_sea == 5 * ONE + (4 * 1023 * ONE) / 2048);
}

TEST_CASE("P1-03 C3: the clamp to [-600, 500] and the noise term with the shipped amplitude") {
    // Extremes inside the bounds: 40 C everywhere with +5 C of noise gives
    // 450; -30 C at 3000 m with lapse 15 and -5 C of noise gives T = -80 C,
    // clamped to -600.
    const climate_api::Tuning hot =
        tuning({{"temp_north_c", "40"}, {"temp_south_c", "40"}, {"lapse_c_per_km", "0"}, {"temp_noise_amp_c", "5"}});
    CHECK(climate_api::temperature_cell(0, 16, 1, ONE, hot) == 450);
    const climate_api::Tuning cold = tuning(
        {{"temp_north_c", "-30"}, {"temp_south_c", "-30"}, {"lapse_c_per_km", "15"}, {"temp_noise_amp_c", "5"}});
    CHECK(climate_api::temperature_cell(0, 16, 30000, -ONE, cold) == -600);
    CHECK(climate_api::temperature_cell(0, 16, 1, ONE, cold) == -250);
    // The shipped amplitude 1: the noise adds amp x noise degrees.
    const Values v = values();
    for (const std::int64_t noise : {-ONE, -ONE / 2, std::int64_t{0}, ONE / 3, ONE}) {
        CAPTURE(noise);
        const TempParts own = own_temperature(17, 64, 1234, noise, v);
        CHECK(climate_api::temperature_cell(17, 64, 1234, noise, shipped_tuning()) == own.temperature);
    }
}

TEST_CASE("P1-03 10.1 test 2: the C3 noise term, seed 0, N = 64, shipped tuning, at 16 cells, with fbm2") {
    const Inputs in = world_inputs(0, 64, false);
    const climate_api::Layers l = stage_layers(in, shipped_tuning(), 0, THREADS);
    const Values v = values();
    const std::uint64_t key = worldgen::stream_key(0, STREAM_TEMP);
    const std::vector<std::int64_t> s = surface(in);
    // 16 cells: the four corners, the centre, cells on land and on ocean,
    // and cells on the G6 block boundary rows 63 / 0 of a 64-row map.
    const std::vector<std::pair<std::int32_t, std::int32_t>> cells = {
        {0, 0},   {63, 0},  {0, 63},  {63, 63}, {32, 32}, {31, 31}, {20, 40}, {40, 20},
        {10, 33}, {33, 10}, {45, 45}, {17, 50}, {50, 17}, {28, 36}, {36, 28}, {5, 58},
    };
    std::size_t land = 0;
    for (const auto& [x, y] : cells) {
        CAPTURE(x);
        CAPTURE(y);
        const std::size_t i = in.grid.index(x, y);
        // fbm_m of P1-01 4.1 written out as fbm2 with base frequency
        // from_ratio(1, wavelength_m) and lacunarity 2.
        const worldgen::Fx noise =
            worldgen::fbm2(key, Fx::from_int(16 * x + 8), Fx::from_int(16 * y + 8), v.temp_noise.octaves,
                           Fx::from_ratio(1, v.temp_noise.wavelength_m), Fx::from_int(2), v.temp_noise.gain);
        CHECK(l.temperature[i] == own_temperature(y, 64, s[i], noise.raw(), v).temperature);
        land += in.elevation[i] > 0 ? 1U : 0U;
    }
    CHECK(land >= 4U);
    // The noise does matter here: some cell differs from its amplitude-0 value.
    const climate_api::Layers q = stage_layers(in, tuning({{"temp_noise_amp_c", "0"}}), 0, THREADS);
    CHECK(q.temperature != l.temperature);
    CHECK(q.rainfall == l.rainfall);  // the temperature noise does not touch rainfall
}

// --- C4 (5.1 "C4 box mean", 10.1 test 3) ---------------------------------------------------

TEST_CASE("P1-03 5.1 C4: the box mean of the 4 x 3 example, r = 1") {
    const Grid g{4, 3};
    const std::vector<std::int64_t> s = {0, 10, 20, 30, 0, 40, 50, 60, 0, 0, 70, 80};
    const std::vector<std::int64_t> want = {
        Fx::from_ratio(25, 2).raw(), Fx::from_int(20).raw(), Fx::from_int(35).raw(), Fx::from_int(40).raw(),
        35791394133,                 90671531804,            171798691840,           221906643627,
        Fx::from_int(10).raw(),      114532461227,           Fx::from_int(50).raw(), Fx::from_int(65).raw(),
    };
    CHECK(own_box_mean(g, s, 1) == want);
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        CHECK(climate_api::box_mean(g, s, 1, threads) == want);
    }
    // (0, 1): window x 0..1, y 0..2, sum 50 over 6 cells.
    CHECK(want[4] == Fx::from_ratio(50, 6).raw());
}

TEST_CASE("P1-03 C4, 10.1 test 3: r = 0 gives from_int(S); r at least the grid gives the grid mean everywhere") {
    const Grid g{4, 3};
    const std::vector<std::int64_t> s = {0, 10, 20, 30, 0, 40, 50, 60, 0, 0, 70, 80};
    std::vector<std::int64_t> exact;
    for (const std::int64_t v : s) {
        exact.push_back(Fx::from_int(v).raw());
    }
    CHECK(climate_api::box_mean(g, s, 0, THREADS) == exact);
    for (const std::int32_t r : {3, 4, 100, 250}) {
        CAPTURE(r);
        CHECK(climate_api::box_mean(g, s, r, THREADS) == std::vector<std::int64_t>(12, Fx::from_ratio(360, 12).raw()));
    }
    // A larger random grid: every window the whole grid.
    worldgen::Rng rng(worldgen::stream_key(3, STREAM_TEST));
    const Grid big{37, 70};
    std::vector<std::int64_t> sb(big.cells());
    std::int64_t sum = 0;
    for (auto& v : sb) {
        v = rng.uniform_i64(0, 30000);
        sum += v;
    }
    const std::int64_t mean = Fx::from_ratio(sum, big.cells()).raw();
    CHECK(climate_api::box_mean(big, sb, 70, THREADS) == std::vector<std::int64_t>(big.cells(), mean));
    CHECK(climate_api::box_mean(big, sb, 69, THREADS) != std::vector<std::int64_t>(big.cells(), mean));
}

TEST_CASE("P1-03 C4, D2: the box mean equals a 2-D prefix sum on ragged grids, any thread count") {
    worldgen::Rng rng(worldgen::stream_key(4, STREAM_TEST));
    for (const Grid g : {Grid{1, 1}, Grid{1, 130}, Grid{130, 1}, Grid{65, 129}, Grid{100, 200}}) {
        std::vector<std::int64_t> s(g.cells());
        for (auto& v : s) {
            v = rng.uniform_i64(0, 30000);
        }
        for (const std::int32_t r : {0, 1, 2, 20, 64}) {
            CAPTURE(g.width);
            CAPTURE(g.height);
            CAPTURE(r);
            const std::vector<std::int64_t> want = own_box_mean(g, s, r);
            CHECK(climate_api::box_mean(g, s, r, 1) == want);
            CHECK(climate_api::box_mean(g, s, r, 2) == want);
            CHECK(climate_api::box_mean(g, s, r, 5) == want);
        }
    }
}

// --- C5 (5.1 "C5 sweep", "Diagonal step") ----------------------------------------------------

TEST_CASE("P1-03 5.1 C5: the wind-E sweep of the 6 x 3 grid, every q raw and every p to three decimals") {
    const SweepGrid sg = sweep_grid();
    const Values v = values(quiet_overrides());
    const SweepConsts c = sweep_consts(0, v);
    CHECK(c.rec.raw() == 137438953);  // 16 / 500 = 0.032
    const OwnSweep own = own_sweep(sg.g, sg.hs, sg.water, 0, v);
    const climate_api::Sweep st = climate_api::sweep(sg.g, sg.s, sg.water, sg.hs, 0, tuning(quiet_overrides()));
    CHECK(rows_of(6, own.q) == sweep_q());
    CHECK(rows_of(6, st.q) == sweep_q());
    std::vector<std::string> pm;
    for (const std::int64_t p : st.p) {
        pm.push_back(mm3(p));
    }
    CHECK(rows_of(6, pm) == sweep_p());
    CHECK(st.p == own.p);

    // (1, 0) by hand: the upwind column is off the map (q_up = ONE, hs_up = 0);
    // rise 20 dm, g = 0.125, p = 1425 mm, lam = 0.001425, q = ONE - 6120328.
    CHECK(own.q_up[1] == ONE);
    CHECK(own.hs_up[1] == 0);
    const CellStep s10 = own_cell(false, sg.hs[1], ONE, 0, c, v);
    CHECK(s10.rise == Fx::from_int(20).raw());
    CHECK(s10.g == 536870912);
    CHECK(s10.p == Fx::from_int(1425).raw());
    CHECK(s10.lam == 6120328);
    CHECK(st.q[1] == 4294967296 - 6120328);
    CHECK(st.p[1] == Fx::from_int(1425).raw());
    // (2, 1) by hand: taps (1, 0), (1, 1), (1, 2); q_up = RHA(17154045693 / 4)
    // = 4288511423; hs_up = 22.5 dm; rise 67.5, g = 0.421875; p raw
    // 12476887921291 (2905.002 mm).
    const std::size_t i21 = sg.g.index(2, 1);
    CHECK(st.q[1] + 2 * st.q[7] + st.q[13] == 17154045693);
    CHECK(own.q_up[i21] == 4288511423);
    CHECK(own.hs_up[i21] == Fx::from_ratio(45, 2).raw());
    CHECK(own.rise[i21] == Fx::from_ratio(135, 2).raw());
    CHECK(own_cell(false, sg.hs[i21], 4288511423, Fx::from_ratio(45, 2).raw(), c, v).g == 1811939328);
    CHECK(st.p[i21] == 12476887921291);
    // Column 4 descends (rise 0): base rain only, from the depleted air.
    // Column 5 is ocean: p from q_up, and q recharging.
    for (std::int32_t y = 0; y < 3; ++y) {
        CAPTURE(y);
        const std::size_t i4 = sg.g.index(4, y), i5 = sg.g.index(5, y);
        CHECK(own.rise[i4] == 0);
        CHECK(st.p[i4] == worldgen::mul(R(own.q_up[i4]), Fx::from_int(800)).raw());
        CHECK(st.p[i5] == worldgen::mul(R(own.q_up[i5]), Fx::from_int(800)).raw());
        CHECK(st.q[i5] == (R(own.q_up[i5]) + worldgen::mul(Fx::ONE - R(own.q_up[i5]), c.rec)).raw());
        CHECK(st.q[i5] > own.q_up[i5]);
    }
}

TEST_CASE("P1-03 5.1 C5: the diagonal step, wind SE (k_w = 1), same grid") {
    const SweepGrid sg = sweep_grid();
    const Values v = values(quiet_overrides());
    const SweepConsts c = sweep_consts(1, v);
    CHECK(c.step_m == worldgen::mul_int(Fx::SQRT2, 16));
    CHECK(c.step_dm == worldgen::mul_int(Fx::SQRT2, 160));
    const climate_api::Sweep st = climate_api::sweep(sg.g, sg.s, sg.water, sg.hs, 1, tuning(quiet_overrides()));
    const OwnSweep own = own_sweep(sg.g, sg.hs, sg.water, 1, v);
    // (1, 0): taps (0, -2), (0, -1), (0, 0), the first two off the map.
    const auto t10 = taps(1, 1, 0);
    CHECK(t10[0] == std::pair<std::int32_t, std::int32_t>{0, -2});
    CHECK(t10[1] == std::pair<std::int32_t, std::int32_t>{0, -1});
    CHECK(t10[2] == std::pair<std::int32_t, std::int32_t>{0, 0});
    CHECK(own.q_up[1] == ONE);
    CHECK(own.hs_up[1] == 0);
    const CellStep s10 = own_cell(false, sg.hs[1], ONE, 0, c, v);
    CHECK(s10.g == 379625062);
    CHECK(s10.g == worldgen::div(Fx::from_int(20), worldgen::mul_int(Fx::SQRT2, 160)).raw());
    CHECK(s10.p == 5334099146800);
    CHECK(s10.lam == 7543555);
    CHECK(st.p[1] == 5334099146800);
    CHECK(mm3(st.p[1]) == "1241.942");
    CHECK(st.q[1] == 4287423741);
    // (3, 0): taps (2, -2), (2, -1), (2, 0): q_up 4291438217, hs_up 15 dm,
    // p 3117.630 mm.
    CHECK(own.q_up[3] == 4291438217);
    CHECK(own.hs_up[3] == Fx::from_int(15).raw());
    CHECK(mm3(st.p[3]) == "3117.630");
    CHECK(st.q == own.q);
    CHECK(st.p == own.p);
}

TEST_CASE("P1-03 C5: every wind code on the 5.1 grid equals the test's own sweep") {
    const SweepGrid sg = sweep_grid();
    for (const auto& o : {quiet_overrides(), Overrides{}}) {
        const Values v = values(o);
        const climate_api::Tuning t = tuning(o);
        for (std::uint8_t k = 0; k < 8; ++k) {
            CAPTURE(int{k});
            const OwnSweep own = own_sweep(sg.g, sg.hs, sg.water, k, v);
            const climate_api::Sweep st = climate_api::sweep(sg.g, sg.s, sg.water, sg.hs, k, t);
            CHECK(st.q == own.q);
            CHECK(st.p == own.p);
        }
    }
}

// --- C6 (5.1 "C6 rainfall") --------------------------------------------------------------

TEST_CASE("P1-03 5.1 C6: the rainfall table (shipped values, rn given)") {
    struct Row {
        std::int64_t p_mm;
        std::int16_t coast;
        const char* rn;
        std::int64_t cb_raw;
        std::uint16_t rainfall;
    };
    const std::vector<Row> rows = {
        {800, 16, "0", Fx::from_ratio(150 * 1484, 1500).raw(), 948},
        {800, 1500, "0", 0, 800},
        {100, 3000, "0", 0, 250},
        {12000, 50, "0", Fx::from_int(145).raw(), 10000},
        {800, 700, "0.5", Fx::from_int(80).raw(), 1040},
        {800, -40, "-0.3", Fx::from_int(150).raw(), 854},
    };
    const Values v = values();
    for (const Row& r : rows) {
        CAPTURE(r.p_mm);
        CAPTURE(r.coast);
        CAPTURE(std::string(r.rn));
        const std::int64_t rn = elev_test::dec(r.rn);
        const RainParts own = own_rainfall(Fx::from_int(r.p_mm).raw(), r.coast, rn, v);
        CHECK(own.cb == r.cb_raw);
        CHECK(own.rainfall == r.rainfall);
        CHECK(climate_api::rainfall_cell(Fx::from_int(r.p_mm).raw(), r.coast, rn, shipped_tuning()) == r.rainfall);
    }
    CHECK(mm3(Fx::from_ratio(150 * 1484, 1500).raw()) == "148.400");
}

TEST_CASE("P1-03 C6: the coast bonus falls linearly to 0 at coast_rain_reach_m; the noise factor is floored at 0") {
    const Values v = values();
    const std::int64_t p = Fx::from_int(800).raw();
    CHECK(own_rainfall(p, 1499, 0, v).cb == Fx::from_ratio(150, 1500).raw());
    CHECK(climate_api::rainfall_cell(p, 1499, 0, shipped_tuning()) == 800);
    CHECK(climate_api::rainfall_cell(p, 0, 0, shipped_tuning()) == 950);
    CHECK(climate_api::rainfall_cell(p, -32767, 0, shipped_tuning()) == 950);  // ocean counts as 0
    CHECK(climate_api::rainfall_cell(p, 32767, 0, shipped_tuning()) == 800);
    // rain_noise_amp 1 and rn = -1: f = max(0, 0) = 0, so only the coast bonus
    // and the floor remain.
    const climate_api::Tuning amp1 = tuning({{"rain_noise_amp", "1"}});
    CHECK(climate_api::rainfall_cell(p, 5000, -ONE, amp1) == 250);
    CHECK(climate_api::rainfall_cell(p, 0, -ONE, amp1) == 250);  // 0 + 150, floored at 250
    CHECK(climate_api::rainfall_cell(p, 0, ONE, amp1) == 1750);  // 800 x 2 + 150
    // rain_min_mm 0 and the cap.
    const climate_api::Tuning floor0 = tuning({{"rain_min_mm", "0"}});
    CHECK(climate_api::rainfall_cell(0, 5000, 0, floor0) == 0);
    CHECK(climate_api::rainfall_cell(Fx::from_int(20000).raw(), 5000, 0, floor0) == 10000);
}

TEST_CASE("P1-03 5.1 C6: the wind-E sweep through the whole stage gives the rainfall rows of 5.1") {
    // The 6 x 3 grid as layers: S = elevation on land, ocean elevation -5 with
    // depth 5; coast_dist 16 on land and -16 on ocean; r = 0
    // (smooth_radius_m 15, 15 div 16 = 0); wind forced to E; noise 0.
    const SweepGrid sg = sweep_grid();
    std::vector<std::int16_t> coast;
    for (const std::uint8_t w : sg.water) {
        coast.push_back(static_cast<std::int16_t>(w == WATER_OCEAN ? -16 : 16));
    }
    const Inputs in = inputs_of_surface(sg.g, sg.s, sg.water, coast);
    Overrides o = quiet_overrides();
    o.emplace_back("smooth_radius_m", "15");
    o.emplace_back("wind_weights", weights_text(one_hot(0)));
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{99}}) {
        CAPTURE(seed);
        const climate_api::Layers l = stage_layers(in, tuning(o), seed, THREADS);
        CHECK(rows_of(6, l.rainfall) == sweep_rainfall());
        CHECK(l.rainfall == own_climate(in, values(o), seed).rainfall);
        // The sweep rainfall from the seam values directly.
        const climate_api::Sweep st = climate_api::sweep(sg.g, sg.s, sg.water, sg.hs, 0, tuning(o));
        std::vector<std::uint16_t> r;
        for (std::size_t i = 0; i < st.p.size(); ++i) {
            r.push_back(climate_api::rainfall_cell(st.p[i], coast[i], 0, tuning(o)));
        }
        CHECK(rows_of(6, r) == sweep_rainfall());
    }
}

// --- 10.1 test 4: symmetry --------------------------------------------------------------------

namespace {

struct Terrain {
    Grid g;
    std::vector<std::int64_t> s;
    std::vector<std::uint8_t> water;
};

// 10.1 test 4: 5 x 5, S = 10 on land with an ocean border.
Terrain symmetric_terrain() {
    Terrain t{Grid{5, 5}, {}, {}};
    for (std::int32_t y = 0; y < 5; ++y) {
        for (std::int32_t x = 0; x < 5; ++x) {
            const bool border = x == 0 || y == 0 || x == 4 || y == 4;
            t.water.push_back(border ? WATER_OCEAN : WATER_DRY);
            t.s.push_back(border ? 0 : 10);
        }
    }
    return t;
}

// A W x H terrain with random S in [1, 3000] inside an ocean border, so that
// a symmetry cannot hold by accident.
Terrain random_terrain(std::uint64_t f, Grid g) {
    worldgen::Rng rng(worldgen::substream(worldgen::stream_key(1, STREAM_TEST), f));
    Terrain t{g, {}, {}};
    for (std::int32_t y = 0; y < g.height; ++y) {
        for (std::int32_t x = 0; x < g.width; ++x) {
            const bool border = x == 0 || y == 0 || x == g.width - 1 || y == g.height - 1;
            t.water.push_back(border ? WATER_OCEAN : WATER_DRY);
            t.s.push_back(border ? 0 : rng.uniform_i64(1, 3000));
        }
    }
    return t;
}

climate_api::Sweep sweep_of(const Terrain& t, std::uint8_t k, std::int32_t r, const climate_api::Tuning& tu) {
    const std::vector<std::int64_t> hs = climate_api::box_mean(t.g, t.s, r, THREADS);
    return climate_api::sweep(t.g, t.s, t.water, hs, k, tu);
}

// Sweeps the transformed terrain with the transformed wind and maps the
// result back.
climate_api::Sweep transformed_sweep(const Terrain& t, Transform tr, std::uint8_t k, std::int32_t r,
                                     const climate_api::Tuning& tu) {
    Terrain t2{transformed_grid(t.g, tr), transform<std::int64_t>(t.g, t.s, tr),
               transform<std::uint8_t>(t.g, t.water, tr)};
    const climate_api::Sweep s2 = sweep_of(t2, code_under(tr, k), r, tu);
    // Back: the inverse of a mirror or transpose is itself; of a 90-degree
    // clockwise rotation, three more rotations.
    auto back = [&](const std::vector<std::int64_t>& v) {
        if (tr != Transform::rotate) {
            return transform<std::int64_t>(t2.g, v, tr);
        }
        std::vector<std::int64_t> a = v;
        Grid g = t2.g;
        for (int i = 0; i < 3; ++i) {
            a = transform<std::int64_t>(g, a, Transform::rotate);
            g = transformed_grid(g, Transform::rotate);
        }
        return a;
    };
    return climate_api::Sweep{back(s2.q), back(s2.p)};
}

}  // namespace

TEST_CASE("P1-03 10.1 test 4: the test's grid transforms and wind mapping are consistent") {
    // Rotating four times is the identity; the wind code turns with the grid.
    const Terrain t = random_terrain(0, Grid{7, 4});
    std::vector<std::int64_t> a = t.s;
    Grid g = t.g;
    for (int i = 0; i < 4; ++i) {
        a = transform<std::int64_t>(g, a, Transform::rotate);
        g = transformed_grid(g, Transform::rotate);
    }
    CHECK(a == t.s);
    CHECK(code_under(Transform::rotate, 0) == 2);  // E -> S under a clockwise turn
    CHECK(code_under(Transform::rotate, 1) == 3);  // SE -> SW
    CHECK(code_under(Transform::mirror_x, 1) == 3);
    CHECK(code_under(Transform::mirror_y, 1) == 7);
    CHECK(code_under(Transform::transpose, 0) == 2);
}

TEST_CASE("P1-03 10.1 test 4: orthogonal winds are the 90-degree rotations of code 0, diagonal winds the mirrors of code 1") {
    // The 5 x 5 terrain of 10.1 test 4 and random terrains (square and not).
    std::vector<Terrain> terrains = {symmetric_terrain(), random_terrain(1, Grid{9, 9}), random_terrain(2, Grid{11, 7})};
    const climate_api::Tuning tu = tuning(quiet_overrides());
    for (std::size_t ti = 0; ti < terrains.size(); ++ti) {
        const Terrain& t = terrains[ti];
        for (const std::int32_t r : {0, 1}) {
            CAPTURE(ti);
            CAPTURE(r);
            // Orthogonal: rotation by 90 degrees maps E -> S -> W -> N, and
            // rotating the terrain and the wind together rotates the result.
            for (const std::uint8_t k : {std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{4}, std::uint8_t{6}}) {
                CAPTURE(int{k});
                CHECK(transformed_sweep(t, Transform::rotate, k, r, tu) == sweep_of(t, k, r, tu));
                CHECK(transformed_sweep(t, Transform::mirror_x, k, r, tu) == sweep_of(t, k, r, tu));
                CHECK(transformed_sweep(t, Transform::mirror_y, k, r, tu) == sweep_of(t, k, r, tu));
                CHECK(transformed_sweep(t, Transform::transpose, k, r, tu) == sweep_of(t, k, r, tu));
            }
            // Diagonal: the mirrors in x and in y map SE to SW and NE (and
            // both to NW). The upwind taps of a diagonal wind are always a
            // column triple (dx != 0, C5), so a transpose or a 90-degree
            // rotation of a diagonal sweep is not a diagonal sweep of the
            // turned grid; the spec does not make that exact and it is not
            // asserted (see the test writer's report).
            for (const std::uint8_t k : {std::uint8_t{1}, std::uint8_t{3}, std::uint8_t{5}, std::uint8_t{7}}) {
                CAPTURE(int{k});
                CHECK(transformed_sweep(t, Transform::mirror_x, k, r, tu) == sweep_of(t, k, r, tu));
                CHECK(transformed_sweep(t, Transform::mirror_y, k, r, tu) == sweep_of(t, k, r, tu));
            }
        }
    }
    // On the 5 x 5 terrain, every code's result is the image of code 0 or 1.
    const Terrain& t = terrains[0];
    const climate_api::Sweep e = sweep_of(t, 0, 0, tu), se = sweep_of(t, 1, 0, tu);
    auto image = [&](const climate_api::Sweep& s, Transform tr) {
        return climate_api::Sweep{transform<std::int64_t>(t.g, s.q, tr), transform<std::int64_t>(t.g, s.p, tr)};
    };
    auto rot = [&](const climate_api::Sweep& s, int times) {
        climate_api::Sweep a = s;
        for (int i = 0; i < times; ++i) {
            a = image(a, Transform::rotate);
        }
        return a;
    };
    CHECK(sweep_of(t, 2, 0, tu) == rot(e, 1));
    CHECK(sweep_of(t, 4, 0, tu) == rot(e, 2));
    CHECK(sweep_of(t, 6, 0, tu) == rot(e, 3));
    CHECK(sweep_of(t, 3, 0, tu) == image(se, Transform::mirror_x));
    CHECK(sweep_of(t, 7, 0, tu) == image(se, Transform::mirror_y));
    CHECK(sweep_of(t, 5, 0, tu) == image(image(se, Transform::mirror_x), Transform::mirror_y));
}

TEST_CASE("P1-03 10.1 test 4, C6: the whole stage's rainfall turns with the terrain and the wind (noise 0)") {
    // With rain_noise_amp 0, rainfall depends on p and coast_dist only, so a
    // turned world gives the turned rainfall: rotations for orthogonal
    // winds, mirrors for every wind. Temperature depends on the row (V), so
    // only a mirror in x leaves it unchanged (temp_noise_amp_c 0).
    const RandomFixture rf = random_fixture(3);
    const Inputs& in = rf.in;
    for (std::uint8_t k = 0; k < 8; ++k) {
        CAPTURE(int{k});
        Overrides o = quiet_overrides();
        o.emplace_back("wind_weights", weights_text(one_hot(k)));
        const climate_api::Layers base = stage_layers(in, tuning(o), 11, THREADS);
        std::vector<Transform> trs = {Transform::mirror_x, Transform::mirror_y};
        if (!diagonal(k)) {
            trs.push_back(Transform::rotate);
            trs.push_back(Transform::transpose);
        }
        for (const Transform tr : trs) {
            CAPTURE(static_cast<int>(tr));
            Inputs t2;
            t2.grid = transformed_grid(in.grid, tr);
            t2.elevation = transform<std::int16_t>(in.grid, in.elevation, tr);
            t2.water = transform<std::uint8_t>(in.grid, in.water, tr);
            t2.water_depth = transform<std::uint16_t>(in.grid, in.water_depth, tr);
            t2.coast_dist = transform<std::int16_t>(in.grid, in.coast_dist, tr);
            Overrides o2 = quiet_overrides();
            o2.emplace_back("wind_weights", weights_text(one_hot(code_under(tr, k))));
            const climate_api::Layers l2 = stage_layers(t2, tuning(o2), 11, THREADS);
            CHECK(l2.rainfall == transform<std::uint16_t>(in.grid, base.rainfall, tr));
            if (tr == Transform::mirror_x) {
                CHECK(l2.temperature == transform<std::int16_t>(in.grid, base.temperature, tr));
            }
        }
    }
}

// --- C1 preconditions (section 2) -------------------------------------------------------------

TEST_CASE("P1-03 2, C1: input preconditions are fatal with the climate prefix") {
    const SweepGrid sg = sweep_grid();
    std::vector<std::int16_t> coast(sg.s.size(), 16);
    const Inputs good = inputs_of_surface(sg.g, sg.s, sg.water, coast);
    const climate_api::Tuning t = tuning(quiet_overrides());
    CHECK_NOTHROW(static_cast<void>(stage_layers(good, t, 0, THREADS)));
    auto fatal_with_prefix = [&](const Inputs& in, const char* what) {
        CAPTURE(what);
        try {
            static_cast<void>(stage_layers(in, t, 0, THREADS));
            FAIL("no fatal error");
        } catch (const worldgen::FatalError& e) {
            CHECK(std::string_view(e.what()).substr(0, PRECONDITION.size()) == PRECONDITION);
        }
    };
    {
        Inputs in = good;  // S = 4 on an ocean cell
        in.water_depth[0] = 4;
        fatal_with_prefix(in, "ocean surface not 0");
    }
    {
        Inputs in = good;  // a land cell at surface 0
        in.elevation[1] = 0;
        fatal_with_prefix(in, "land surface below 1");
    }
    {
        Inputs in = good;  // a lake cell whose surface is negative
        in.water[2] = WATER_LAKE;
        in.elevation[2] = -3;
        in.water_depth[2] = 2;
        fatal_with_prefix(in, "lake surface below 1");
    }
    {
        Inputs in = good;  // a river cell at surface 0
        in.water[1] = WATER_RIVER;
        in.elevation[1] = 0;
        fatal_with_prefix(in, "river surface below 1");
    }
    for (int which = 0; which < 4; ++which) {
        Inputs in = good;  // a layer of the wrong size (the seam: not W x H)
        CAPTURE(which);
        switch (which) {
            case 0: in.elevation.pop_back(); break;
            case 1: in.water.pop_back(); break;
            case 2: in.water_depth.pop_back(); break;
            default: in.coast_dist.pop_back(); break;
        }
        CHECK_THROWS_AS(static_cast<void>(stage_layers(in, t, 0, THREADS)), worldgen::FatalError);
    }
}
