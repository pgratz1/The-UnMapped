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

// 5.1 (revised 2026-11-17), wind E.
const std::vector<std::vector<std::int64_t>>& sweep_q() {
    static const std::vector<std::vector<std::int64_t>> q = {
        {4294967296, 4292578220, 4289950298, 4286870704, 4286633270, 4288375589},
        {4294967296, 4292242676, 4288516008, 4284675152, 4284396886, 4285979494},
        {4294967296, 4292913765, 4290620742, 4288210164, 4287302465, 4288699479},
    };
    return q;
}

const std::vector<std::vector<std::string>>& sweep_p() {
    static const std::vector<std::vector<std::string>> p = {
        {"800.000", "1112.500", "1463.423", "1852.908", "798.767", "798.732"},
        {"800.000", "1268.750", "1853.620", "2203.391", "798.350", "798.271"},
        {"800.000", "956.250", "1229.199", "1384.716", "798.891", "798.794"},
    };
    return p;
}

const std::vector<std::vector<std::uint16_t>>& sweep_rainfall() {
    static const std::vector<std::vector<std::uint16_t>> r = {
        {840, 1152, 1503, 1892, 838, 839},  // coast_rain_mm 40 (P1-03 revised 2026-11-18, 0.9.0)
        {840, 1308, 1893, 2243, 838, 838},
        {840, 996, 1269, 1424, 838, 839},
    };
    return r;
}

// 5.1 (revised), wind SE, raster order.
const std::vector<std::vector<std::int64_t>>& se_q() {
    static const std::vector<std::vector<std::int64_t>> q = {
        {4294967296, 4291866607, 4289918572, 4287755798, 4289060547, 4293557436},
        {4294967296, 4290924843, 4286792934, 4283581311, 4284617726, 4289340761},
        {4294967296, 4291444379, 4287597741, 4283771548, 4281470892, 4285462311},
    };
    return q;
}

const std::vector<std::vector<std::string>>& se_p() {
    static const std::vector<std::vector<std::string>> p = {
        {"800.000", "1020.971", "1407.416", "1959.521", "1351.859", "799.725"},
        {"800.000", "1076.019", "1434.014", "1653.805", "798.523", "798.902"},
        {"800.000", "827.427", "799.079", "798.365", "797.936", "798.146"},
    };
    return p;
}

const std::vector<std::vector<std::uint16_t>>& se_rainfall() {
    static const std::vector<std::vector<std::uint16_t>> r = {
        {840, 1061, 1447, 1999, 1391, 840},
        {840, 1116, 1474, 1693, 838, 839},
        {840, 867, 839, 838, 838, 838},
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
        CHECK(climate_api::temperature_cell(r.y, r.n, r.s, 0, 0, t) == r.temperature);
        // With amplitude 0 the noise value has no effect.
        CHECK(climate_api::temperature_cell(r.y, r.n, r.s, ONE, 0, t) == r.temperature);
        CHECK(climate_api::temperature_cell(r.y, r.n, r.s, -ONE, 0, t) == r.temperature);
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
    CHECK(climate_api::temperature_cell(0, 16, 1, ONE, 0, hot) == 450);
    const climate_api::Tuning cold = tuning(
        {{"temp_north_c", "-30"}, {"temp_south_c", "-30"}, {"lapse_c_per_km", "15"}, {"temp_noise_amp_c", "5"}});
    CHECK(climate_api::temperature_cell(0, 16, 30000, -ONE, 0, cold) == -600);
    CHECK(climate_api::temperature_cell(0, 16, 1, ONE, 0, cold) == -250);
    // The shipped amplitude 1: the noise adds amp x noise degrees.
    const Values v = values();
    for (const std::int64_t noise : {-ONE, -ONE / 2, std::int64_t{0}, ONE / 3, ONE}) {
        CAPTURE(noise);
        const TempParts own = own_temperature(17, 64, 1234, noise, v);
        CHECK(climate_api::temperature_cell(17, 64, 1234, noise, 0, shipped_tuning()) == own.temperature);
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
        CHECK(l.temperature[i] == own_temperature(y, 64, s[i], noise.raw(), v, own_offset_dc(0, v)).temperature);
        land += in.elevation[i] > 0 ? 1U : 0U;
    }
    CHECK(land >= 4U);
    // The noise does matter here: some cell differs from its amplitude-0 value.
    const climate_api::Layers q = stage_layers(in, tuning({{"temp_noise_amp_c", "0"}}), 0, THREADS);
    CHECK(q.temperature != l.temperature);
    CHECK(q.rainfall == l.rainfall);  // the temperature noise does not touch rainfall
}

// --- C4 (5.1 "One box pass", "C4 triangle", "edge clipping"; 10.1 test 3; section 7) -----------

namespace {

const std::vector<std::int64_t>& box_s() {
    static const std::vector<std::int64_t> s = {0, 10, 20, 30, 0, 40, 50, 60, 0, 0, 70, 80};
    return s;
}

std::vector<std::int64_t> random_s(std::uint64_t key, Grid g, std::int64_t lo, std::int64_t hi) {
    worldgen::Rng rng(worldgen::stream_key(key, STREAM_TEST));
    std::vector<std::int64_t> s(g.cells());
    for (auto& v : s) {
        v = rng.uniform_i64(lo, hi);
    }
    return s;
}

}  // namespace

TEST_CASE("P1-03 5.1: one box pass (climate_box_mean) of the 4 x 3 example, half-width 1") {
    const Grid g{4, 3};
    const std::vector<std::int64_t> want = {
        Fx::from_ratio(25, 2).raw(), Fx::from_int(20).raw(), Fx::from_int(35).raw(), Fx::from_int(40).raw(),
        35791394133,                 90671531804,            171798691840,           221906643627,
        Fx::from_int(10).raw(),      114532461227,           Fx::from_int(50).raw(), Fx::from_int(65).raw(),
    };
    CHECK(own_box_mean(g, box_s(), 1) == want);
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        CHECK(climate_api::box_mean(g, box_s(), 1, threads) == want);
    }
    // (0, 1): window x 0..1, y 0..2, sum 50 over 6 cells.
    CHECK(want[4] == Fx::from_ratio(50, 6).raw());
}

TEST_CASE("P1-03 5.1 C4: the triangle of the 4 x 3 example, r = 2 (h = 1), raw; r = 3 the same") {
    const Grid g{4, 3};
    const std::vector<std::int64_t> want = {
        66512340764, 98028651710,  148733126732, 178956970667,  //
        70588582874, 106711378805, 166761384518, 201624853618,  //
        70986265031, 111748686127, 182138427923, 221906643627,
    };
    CHECK(own_triangle_mean(g, box_s(), 2) == want);
    // By hand, (0, 0): window x 0..1, y 0..1 of hs1, sum2 266049363057, c_w 4.
    const std::vector<std::int64_t> hs1 = own_box_mean(g, box_s(), 1);
    CHECK(hs1[0] + hs1[1] + hs1[4] + hs1[5] == 266049363057);
    CHECK(want[0] == worldgen::div_int(Fx::from_raw(266049363057), 4).raw());
    for (const std::uint32_t threads : {1U, 2U, 5U}) {
        CAPTURE(threads);
        CHECK(climate_api::triangle_mean(g, box_s(), 2, threads) == want);
        CHECK(climate_api::triangle_mean(g, box_s(), 3, threads) == want);
    }
    // smooth_radius_m 32 to 47 give r = 2.
    for (std::int64_t m = 32; m <= 47; ++m) {
        CHECK(m / 16 == 2);
    }
}

TEST_CASE("P1-03 5.1 C4: edge clipping, 9 x 9 with S = 900 at the centre, r = 4") {
    const Grid g{9, 9};
    std::vector<std::int64_t> s(81, 0);
    s[g.index(4, 4)] = 900;
    const std::vector<std::int64_t> hs = climate_api::triangle_mean(g, s, 4, THREADS);
    CHECK(hs == own_triangle_mean(g, s, 4));
    const std::vector<std::int64_t> row(hs.begin() + 36, hs.begin() + 45);
    const std::vector<std::int64_t> head = {51539607552, 77309411328, 92771293594, 123695058125, 154618822656};
    CHECK(std::vector<std::int64_t>(row.begin(), row.begin() + 5) == head);
    CHECK(std::vector<std::int64_t>(row.rbegin(), row.rbegin() + 5) == head);  // symmetric
    // dm 12 18 21.6 28.8 36: 21.6 and 28.8 are the RHA raws of 108/5 and 144/5.
    CHECK(row[0] == Fx::from_int(12).raw());
    CHECK(row[1] == Fx::from_int(18).raw());
    CHECK(row[2] == Fx::from_ratio(108, 5).raw());
    CHECK(row[3] == Fx::from_ratio(144, 5).raw());
    CHECK(row[4] == Fx::from_int(36).raw());
    // Unclipped for x = 2 to 6: 900 w(x) w(4), w = (1,2,3,4,5,4,3,2,1)/25.
    const std::vector<std::int64_t> w = {1, 2, 3, 4, 5, 4, 3, 2, 1};
    for (std::int32_t x = 2; x <= 6; ++x) {
        CAPTURE(x);
        CHECK(row[static_cast<std::size_t>(x)] == Fx::from_ratio(900 * w[static_cast<std::size_t>(x)] * 5, 625).raw());
    }
}

TEST_CASE("P1-03 10.1 test 3: box and triangle at radii as large as the grid or larger; r 0 or 1; odd r") {
    const Grid g{4, 3};
    std::vector<std::int64_t> exact;
    for (const std::int64_t v : box_s()) {
        exact.push_back(Fx::from_int(v).raw());
    }
    const std::vector<std::int64_t> mean(12, Fx::from_ratio(360, 12).raw());
    CHECK(mean[0] == 128849018880);
    CHECK(climate_api::box_mean(g, box_s(), 0, THREADS) == exact);
    for (const std::int64_t r : {3, 4, 100, 250}) {  // r >= max(W, H) - 1 = 3
        CAPTURE(r);
        CHECK(climate_api::box_mean(g, box_s(), r, THREADS) == mean);
    }
    CHECK(climate_api::box_mean(g, box_s(), 2, THREADS) != mean);
    for (const std::int64_t r : {0, 1}) {
        CAPTURE(r);
        CHECK(climate_api::triangle_mean(g, box_s(), r, THREADS) == exact);
    }
    for (const std::int64_t r : {6, 7, 9, 40, 250}) {  // r div 2 >= 3
        CAPTURE(r);
        CHECK(climate_api::triangle_mean(g, box_s(), r, THREADS) == mean);
    }
    // Random grids, square and not: whole-grid windows, and odd r equals r - 1.
    for (const Grid gr : {Grid{37, 70}, Grid{1, 40}, Grid{25, 1}, Grid{19, 19}}) {
        CAPTURE(gr.width);
        CAPTURE(gr.height);
        const std::vector<std::int64_t> s = random_s(3, gr, 0, 32767);
        std::int64_t sum = 0;
        for (const std::int64_t v : s) {
            sum += v;
        }
        const std::int64_t m = std::max(gr.width, gr.height) - 1;
        const std::vector<std::int64_t> all(gr.cells(), Fx::from_ratio(sum, gr.cells()).raw());
        CHECK(climate_api::box_mean(gr, s, m, THREADS) == all);
        CHECK(climate_api::triangle_mean(gr, s, 2 * m, THREADS) == all);
        CHECK(climate_api::triangle_mean(gr, s, 2 * m + 1, THREADS) == all);
        for (const std::int64_t r : {1, 3, 5, 9, 21}) {
            CAPTURE(r);
            CHECK(climate_api::triangle_mean(gr, s, r, THREADS) == climate_api::triangle_mean(gr, s, r - 1, THREADS));
        }
    }
}

TEST_CASE("P1-03 C4, D2: both passes equal the test's own (2-D prefix sum, direct window sums) on ragged grids, any thread count") {
    for (const Grid g : {Grid{1, 1}, Grid{1, 130}, Grid{130, 1}, Grid{65, 129}, Grid{100, 200}}) {
        const std::vector<std::int64_t> s = random_s(4, g, 0, 32767);
        for (const std::int64_t r : {0, 1, 2, 3, 4, 20, 21, 64}) {
            CAPTURE(g.width);
            CAPTURE(g.height);
            CAPTURE(r);
            const std::vector<std::int64_t> box = own_box_mean(g, s, r);
            const std::vector<std::int64_t> tri = own_triangle_mean(g, s, r);
            for (const std::uint32_t threads : {1U, 2U, 5U}) {
                CHECK(climate_api::box_mean(g, s, r, threads) == box);
                CHECK(climate_api::triangle_mean(g, s, r, threads) == tri);
            }
            const std::string bound = hs_bound_violation(g, s, r, tri);
            CHECK_MESSAGE(bound.empty(), bound);
        }
    }
}

TEST_CASE("P1-03 7: the overflow margin of C4, S = 32767 with smooth_radius_m 4000 (r 250, h 125)") {
    // Full 251 x 251 windows exist on a 260 x 255 grid: the largest pass-2
    // sum is 63001 x 32767 x 2^32, just below 2^63 - 1. Every hs is exactly
    // from_int(32767) (a mean of equal values) and nothing is fatal.
    CHECK(std::int64_t{63001} * 32767 < (std::int64_t{1} << 31));
    CHECK(static_cast<std::uint64_t>(63001) * 32767U * (std::uint64_t{1} << 32) < (std::uint64_t{1} << 63) - 1U);
    for (const Grid g : {Grid{260, 255}, Grid{300, 1}, Grid{1, 300}}) {
        CAPTURE(g.width);
        CAPTURE(g.height);
        const std::vector<std::int64_t> s(g.cells(), 32767);
        const std::vector<std::int64_t> want(g.cells(), Fx::from_int(32767).raw());
        CHECK(climate_api::triangle_mean(g, s, 250, THREADS) == want);
        CHECK(climate_api::triangle_mean(g, s, 251, 5) == want);
        CHECK(climate_api::box_mean(g, s, 125, THREADS) == want);  // a 251 x 251 box (r 250 would exceed the guard)
    }
    // Values near the top that differ, against the reference's exact
    // integers (golden case "overflow" of p1-03-climate.txt).
    const std::vector<GoldenCase> golden = read_golden(UNMAPPED_P1_03_GOLDEN_PATH);
    bool found = false;
    for (const GoldenCase& c : golden) {
        if (c.fixture != "overflow") {
            continue;
        }
        found = true;
        const Grid g{c.n, c.n - 5};
        std::vector<std::int64_t> s(g.cells());
        for (std::int32_t y = 0; y < g.height; ++y) {
            for (std::int32_t x = 0; x < g.width; ++x) {
                s[g.index(x, y)] = 32767 - (7 * std::int64_t{x} + 13 * std::int64_t{y}) % 768;
            }
        }
        CHECK(hash_i64(s) == c.get("S"));
        const std::vector<std::int64_t> hs = climate_api::triangle_mean(g, s, 250, THREADS);
        CHECK(hash_i64(hs) == c.get("hs"));
        CHECK(hash_i64(own_triangle_mean(g, s, 250)) == c.get("hs"));
        CHECK(hash_i64(climate_api::box_mean(g, s, 125, THREADS)) == c.get("hs1"));
        CHECK(std::to_string(hs[0]) == c.get("hs_corner"));
    }
    CHECK(found);
}

namespace {

// The entry guard of both C4 seams (P1-03 10, audit fix): with MAX = 65538,
// fatal with the precondition prefix unless W <= MAX and min(W, 2r + 1) x
// min(H, 2r + 1) <= MAX, r = r_cells for the box and r_cells div 2 for the
// triangle. The bound keeps every window sum below 2^63: 65538 x 32767 x
// 2^32 = 9223372028264841216.
constexpr std::int64_t GUARD_MAX = 65538;

bool guard_allows(Grid g, std::int64_t r) {
    const std::int64_t span = 2 * r + 1;
    return g.width <= GUARD_MAX && std::min<std::int64_t>(g.width, span) * std::min<std::int64_t>(g.height, span) <= GUARD_MAX;
}

template <typename F>
void expect_prefixed_fatal(F&& f, const char* what) {
    CAPTURE(std::string_view(what));
    try {
        static_cast<void>(f());
        FAIL("no fatal error");
    } catch (const worldgen::FatalError& e) {
        CHECK(std::string_view(e.what()).substr(0, PRECONDITION.size()) == PRECONDITION);
    }
}

}  // namespace

TEST_CASE("P1-03 10: the window guard of both C4 seams, the three fatal paths") {
    CHECK(static_cast<std::uint64_t>(GUARD_MAX) * 32767U * (std::uint64_t{1} << 32) < (std::uint64_t{1} << 63) - 1U);
    // S = 32767 on 260 x 255 with r_cells 258: box r = 258, triangle r = 129;
    // both windows exceed 65538 cells (66300 and 259 x 255 = 66045).
    {
        const Grid g{260, 255};
        const std::vector<std::int64_t> s(g.cells(), 32767);
        CHECK_FALSE(guard_allows(g, 258));
        CHECK_FALSE(guard_allows(g, 129));
        expect_prefixed_fatal([&] { return climate_api::box_mean(g, s, 258, THREADS); }, "box 260 x 255 r 258");
        expect_prefixed_fatal([&] { return climate_api::triangle_mean(g, s, 258, THREADS); }, "triangle 260 x 255 r 258");
        // r_cells 250 (the bound of smooth_radius_m) is allowed: 251 x 251.
        CHECK(guard_allows(g, 125));
    }
    // 4096 x 17 with r_cells 8192: 4096 x 17 = 69632 cells per window.
    {
        const Grid g{4096, 17};
        const std::vector<std::int64_t> s(g.cells(), 32767);
        CHECK_FALSE(guard_allows(g, 8192));
        CHECK_FALSE(guard_allows(g, 4096));
        expect_prefixed_fatal([&] { return climate_api::box_mean(g, s, 8192, THREADS); }, "box 4096 x 17 r 8192");
        expect_prefixed_fatal([&] { return climate_api::triangle_mean(g, s, 8192, THREADS); }, "triangle 4096 x 17 r 8192");
    }
    // 65539 x 1 with r_cells 0: W above MAX.
    {
        const Grid g{65539, 1};
        const std::vector<std::int64_t> s(g.cells(), 32767);
        CHECK_FALSE(guard_allows(g, 0));
        expect_prefixed_fatal([&] { return climate_api::box_mean(g, s, 0, THREADS); }, "box 65539 x 1 r 0");
        expect_prefixed_fatal([&] { return climate_api::triangle_mean(g, s, 0, THREADS); }, "triangle 65539 x 1 r 0");
    }
}

TEST_CASE("P1-03 10: in-bounds extremes of the window guard: 65538 x 1 with a huge radius") {
    const Grid g{65538, 1};
    const std::vector<std::int64_t> s = random_s(5, g, 32000, 32767);
    std::int64_t sum = 0;
    for (const std::int64_t v : s) {
        sum += v;
    }
    CHECK(guard_allows(g, 1000000));
    const std::vector<std::int64_t> mean(g.cells(), Fx::from_ratio(sum, g.cells()).raw());
    CHECK(climate_api::box_mean(g, s, 1000000, THREADS) == mean);
    CHECK(climate_api::triangle_mean(g, s, 2000000, THREADS) == mean);
    // All at the top: exactly 32767 everywhere.
    const std::vector<std::int64_t> top(g.cells(), 32767);
    CHECK(climate_api::triangle_mean(g, top, 2000000, 2) ==
          std::vector<std::int64_t>(g.cells(), Fx::from_int(32767).raw()));
    // A single column likewise.
    const Grid c{1, 65538};
    CHECK(climate_api::box_mean(c, top, 70000, THREADS) == std::vector<std::int64_t>(c.cells(), Fx::from_int(32767).raw()));
}

TEST_CASE("P1-03 10: climate_temperature_cell rejects a height above INT32_MAX") {
    const std::int64_t big = std::int64_t{2147483647};
    CHECK_NOTHROW(static_cast<void>(climate_api::temperature_cell(0, big, 10, 0, 0, shipped_tuning())));
    CHECK_NOTHROW(static_cast<void>(climate_api::temperature_cell(big - 1, big, 10, 0, 0, shipped_tuning())));
    CHECK_THROWS_AS(static_cast<void>(climate_api::temperature_cell(0, big + 1, 10, 0, 0, shipped_tuning())),
                    worldgen::FatalError);
}

TEST_SUITE("climate-slow" * doctest::skip()) {
TEST_CASE("P1-03 10, 7 (slow): in-bounds extreme, 2048 x 2048 triangle with r_cells 250 at S = 32767") {
    const Grid g{2048, 2048};
    const std::vector<std::int64_t> s(g.cells(), 32767);
    CHECK(climate_api::triangle_mean(g, s, 250, 4) == std::vector<std::int64_t>(g.cells(), Fx::from_int(32767).raw()));
}
}  // TEST_SUITE

TEST_CASE("P1-03 10: both C4 seams are fatal, with the prefix, for r_cells < 0 and for any S outside [0, 32767]") {
    const Grid g{3, 1};
    const std::vector<std::int64_t> ok = {1, 2, 3};
    expect_prefixed_fatal([&] { return climate_api::box_mean(g, ok, -1, THREADS); }, "box r -1");
    expect_prefixed_fatal([&] { return climate_api::triangle_mean(g, ok, -1, THREADS); }, "triangle r -1");
    for (const std::int64_t bad : {std::int64_t{-1}, std::int64_t{32768}}) {
        CAPTURE(bad);
        const std::vector<std::int64_t> s = {1, bad, 3};
        expect_prefixed_fatal([&] { return climate_api::box_mean(g, s, 1, THREADS); }, "box bad S");
        expect_prefixed_fatal([&] { return climate_api::triangle_mean(g, s, 2, THREADS); }, "triangle bad S");
    }
    CHECK_NOTHROW(static_cast<void>(climate_api::triangle_mean(g, std::vector<std::int64_t>{0, 32767, 0}, 2, THREADS)));
}

// --- C5 (5.1 "C5 sweep", "Diagonal step") ----------------------------------------------------

TEST_CASE("P1-03 5.1 C5: the wind-E sweep of the 6 x 3 grid, every q raw and every p to three decimals") {
    const SweepGrid sg = sweep_grid();
    const Values v = values(quiet_overrides());
    const SweepConsts c = sweep_consts(0, v);
    CHECK(c.rec.raw() == 137438953);  // 16 / 500 = 0.032
    const OwnSweep own = own_sweep(sg.g, sg.hs, sg.water, 0, v);
    const climate_api::Sweep st = climate_api::sweep(sg.g, sg.water, sg.hs, 0, tuning(quiet_overrides()));
    CHECK(rows_of(6, own.q) == sweep_q());
    CHECK(rows_of(6, st.q) == sweep_q());
    std::vector<std::string> pm;
    for (const std::int64_t p : st.p) {
        pm.push_back(mm3(p));
    }
    CHECK(rows_of(6, pm) == sweep_p());
    CHECK(st.p == own.p);

    // (1, 0) by hand: the upwind column is off the map (q_up = ONE, hs_up = 0);
    // rise 20 dm, g = 0.125, p = 800 + 2500 x 0.125 = 1112.5 mm,
    // lam = 0.00055625 (raw 2389076), q = 4292578220.
    CHECK(own.q_up[1] == ONE);
    CHECK(own.hs_up[1] == 0);
    const CellStep s10 = own_cell(false, sg.hs[1], ONE, 0, c, v);
    CHECK(s10.rise == Fx::from_int(20).raw());
    CHECK(s10.g == 536870912);
    CHECK(s10.p == Fx::from_ratio(2225, 2).raw());
    CHECK(s10.lam == 2389076);
    CHECK(st.q[1] == 4294967296 - 2389076);
    CHECK(st.q[1] == 4292578220);
    CHECK(st.p[1] == Fx::from_ratio(2225, 2).raw());
    // (2, 1) by hand: taps (1, 0), (1, 1), (1, 2); q_up = RHA(17169977337 / 4)
    // = 4292494334; hs_up = 22.5 dm; rise 67.5, g = 0.421875; p raw
    // 7961235585091 (1853.620 mm).
    const std::size_t i21 = sg.g.index(2, 1);
    CHECK(st.q[1] + 2 * st.q[7] + st.q[13] == 17169977337);
    CHECK(own.q_up[i21] == 4292494334);
    CHECK(own.hs_up[i21] == Fx::from_ratio(45, 2).raw());
    CHECK(own.rise[i21] == Fx::from_ratio(135, 2).raw());
    CHECK(own_cell(false, sg.hs[i21], 4292494334, Fx::from_ratio(45, 2).raw(), c, v).g == 1811939328);
    CHECK(st.p[i21] == 7961235585091);
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

TEST_CASE("P1-03 C5: the tap table of the diagonal winds and the orthogonal taps") {
    using P = std::pair<std::int32_t, std::int32_t>;
    using T = std::array<P, 3>;
    // Diagonal: a = (x - dx, y), b = (x - dx, y - dy), c = (x, y - dy).
    CHECK(taps(1, 5, 5) == T{P{4, 5}, P{4, 4}, P{5, 4}});  // SE
    CHECK(taps(3, 5, 5) == T{P{6, 5}, P{6, 4}, P{5, 4}});  // SW
    CHECK(taps(5, 5, 5) == T{P{6, 5}, P{6, 6}, P{5, 6}});  // NW
    CHECK(taps(7, 5, 5) == T{P{4, 5}, P{4, 6}, P{5, 6}});  // NE
    // Orthogonal: columns for E and W, rows for S and N.
    CHECK(taps(0, 5, 5) == T{P{4, 4}, P{4, 5}, P{4, 6}});
    CHECK(taps(4, 5, 5) == T{P{6, 4}, P{6, 5}, P{6, 6}});
    CHECK(taps(2, 5, 5) == T{P{4, 4}, P{5, 4}, P{6, 4}});
    CHECK(taps(6, 5, 5) == T{P{4, 6}, P{5, 6}, P{6, 6}});
}

TEST_CASE("P1-03 5.1 C5: the diagonal sweep, wind SE (k_w = 1), raster order, same grid") {
    const SweepGrid sg = sweep_grid();
    const Values v = values(quiet_overrides());
    const SweepConsts c = sweep_consts(1, v);
    CHECK(c.step_m == worldgen::mul_int(Fx::SQRT2, 16));
    CHECK(c.step_dm.raw() == 971840160000);
    CHECK(c.rec.raw() == 194368032);
    const climate_api::Sweep st = climate_api::sweep(sg.g, sg.water, sg.hs, 1, tuning(quiet_overrides()));
    const OwnSweep own = own_sweep(sg.g, sg.hs, sg.water, 1, v);
    CHECK(rows_of(6, own.q) == se_q());
    CHECK(rows_of(6, st.q) == se_q());
    std::vector<std::string> pm;
    for (const std::int64_t p : st.p) {
        pm.push_back(mm3(p));
    }
    CHECK(rows_of(6, pm) == se_p());
    CHECK(st.p == own.p);
    // (1, 0) by hand: a = (0, 0) ocean (q ONE, hs 0), b and c off the map.
    CHECK(own.q_up[1] == ONE);
    CHECK(own.hs_up[1] == 0);
    const CellStep s10 = own_cell(false, sg.hs[1], ONE, 0, c, v);
    CHECK(s10.g == 379625062);
    CHECK(s10.g == worldgen::div(Fx::from_int(20), worldgen::mul_int(Fx::SQRT2, 160)).raw());
    CHECK(s10.p == 4385036491800);
    CHECK(s10.lam == 3100689);
    CHECK(st.p[1] == 4385036491800);
    CHECK(mm3(st.p[1]) == "1020.971");
    CHECK(st.q[1] == 4291866607);
    // (2, 1): a = (1, 1), b = (1, 0), c = (2, 0); q_up 4291144157, hs_up 32.5,
    // rise 57.5, p raw 6159041656350, q 4286792934.
    const std::size_t i21 = sg.g.index(2, 1);
    CHECK(st.q[sg.g.index(1, 1)] == 4290924843);
    CHECK(st.q[sg.g.index(2, 0)] == 4289918572);
    CHECK(own.q_up[i21] == 4291144157);
    CHECK(own.hs_up[i21] == Fx::from_ratio(65, 2).raw());
    CHECK(own.rise[i21] == Fx::from_ratio(115, 2).raw());
    CHECK(st.p[i21] == 6159041656350);
    CHECK(st.q[i21] == 4286792934);
    // (4, 0): a = (3, 0) (q 4287755798, hs 120), b and c off the map: q_up
    // 4293164422, hs_up 30, rise 50, p 1351.859 mm (the off-map rule).
    CHECK(own.q_up[4] == 4293164422);
    CHECK(own.hs_up[4] == Fx::from_int(30).raw());
    CHECK(own.rise[4] == Fx::from_int(50).raw());
    CHECK(mm3(st.p[4]) == "1351.859");
}

TEST_CASE("P1-03 C5: every wind code on the 5.1 grid equals the test's own sweep") {
    const SweepGrid sg = sweep_grid();
    for (const auto& o : {quiet_overrides(), Overrides{}}) {
        const Values v = values(o);
        const climate_api::Tuning t = tuning(o);
        for (std::uint8_t k = 0; k < 8; ++k) {
            CAPTURE(int{k});
            const OwnSweep own = own_sweep(sg.g, sg.hs, sg.water, k, v);
            const climate_api::Sweep st = climate_api::sweep(sg.g, sg.water, sg.hs, k, t);
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
        std::uint16_t rainfall_75;  // with the 0.8.0 coast_rain_mm 75 (5.1, "with 75 ... gave")
    };
    const std::vector<Row> rows = {
        // coast_rain_mm 40 (P1-03 revised 2026-11-18, generator 0.9.0); cb raw as printed in 5.1.
        {800, 16, "0", 169966172460, 840, 874},
        {800, 1500, "0", 0, 800, 800},
        {100, 3000, "0", 0, 250, 250},
        {12000, 50, "0", 166072068779, 10000, 10000},
        {800, 700, "0.5", 91625968981, 981, 1000},
        {800, -40, "-0.3", Fx::from_int(40).raw(), 744, 779},
    };
    const climate_api::Tuning t75 = tuning({{"coast_rain_mm", "75"}});
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
        CHECK(climate_api::rainfall_cell(Fx::from_int(r.p_mm).raw(), r.coast, rn, t75) == r.rainfall_75);
    }
    CHECK(Fx::from_ratio(40 * 1484, 1500).raw() == 169966172460);
    CHECK(mm3(169966172460) == "39.573");
    CHECK(mm3(166072068779) == "38.667");
    CHECK(mm3(91625968981) == "21.333");
}

TEST_CASE("P1-03 C6: the coast bonus falls linearly to 0 at coast_rain_reach_m; the noise factor is floored at 0") {
    const Values v = values();
    const std::int64_t p = Fx::from_int(800).raw();
    CHECK(own_rainfall(p, 1499, 0, v).cb == Fx::from_ratio(40, 1500).raw());
    CHECK(own_rainfall(p, 1499, 0, v).cb == 114532461);  // 5.1, d = coast_rain_reach_m - 1
    CHECK(climate_api::rainfall_cell(p, 1499, 0, shipped_tuning()) == 800);
    CHECK(climate_api::rainfall_cell(p, 0, 0, shipped_tuning()) == 840);
    CHECK(climate_api::rainfall_cell(p, -32767, 0, shipped_tuning()) == 840);  // ocean counts as 0
    CHECK(climate_api::rainfall_cell(p, 32767, 0, shipped_tuning()) == 800);
    // rain_noise_amp 1 and rn = -1: f = max(0, 0) = 0, so only the coast bonus
    // and the floor remain.
    const climate_api::Tuning amp1 = tuning({{"rain_noise_amp", "1"}});
    CHECK(climate_api::rainfall_cell(p, 5000, -ONE, amp1) == 250);
    CHECK(climate_api::rainfall_cell(p, 0, -ONE, amp1) == 250);  // 0 + 40, floored at 250
    CHECK(climate_api::rainfall_cell(p, 0, ONE, amp1) == 1640);  // 800 x 2 + 40
    // rain_min_mm 0 and the cap.
    const climate_api::Tuning floor0 = tuning({{"rain_min_mm", "0"}});
    CHECK(climate_api::rainfall_cell(0, 5000, 0, floor0) == 0);
    CHECK(climate_api::rainfall_cell(Fx::from_int(20000).raw(), 5000, 0, floor0) == 10000);
}

TEST_CASE("P1-03 5.1 C6: both sweeps through the whole stage give the rainfall rows of 5.1") {
    // The 6 x 3 grid as layers: S = elevation on land, ocean elevation -5 with
    // depth 5; coast_dist 16 on land and -16 on ocean; h = 0 (smooth_radius_m
    // 0, 15 and 31 give r = 0, 0, 1); wind forced; noise 0.
    const SweepGrid sg = sweep_grid();
    std::vector<std::int16_t> coast;
    for (const std::uint8_t w : sg.water) {
        coast.push_back(static_cast<std::int16_t>(w == WATER_OCEAN ? -16 : 16));
    }
    const Inputs in = inputs_of_surface(sg.g, sg.s, sg.water, coast);
    for (const std::uint8_t k : {std::uint8_t{0}, std::uint8_t{1}}) {
        const auto& want = k == 0 ? sweep_rainfall() : se_rainfall();
        for (const char* radius : {"0", "15", "31"}) {
            CAPTURE(int{k});
            CAPTURE(std::string(radius));
            Overrides o = quiet_overrides();
            o.emplace_back("smooth_radius_m", radius);
            o.emplace_back("wind_weights", weights_text(one_hot(k)));
            for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{99}}) {
                const climate_api::Layers l = stage_layers(in, tuning(o), seed, THREADS);
                CHECK(rows_of(6, l.rainfall) == want);
                CHECK(l.rainfall == own_climate(in, values(o), seed).rainfall);
            }
            // From the seam values: sweep, then climate_rainfall and the cell function.
            const climate_api::Sweep st = climate_api::sweep(sg.g, sg.water, sg.hs, k, tuning(o));
            std::vector<std::uint16_t> r;
            for (std::size_t i = 0; i < st.p.size(); ++i) {
                r.push_back(climate_api::rainfall_cell(st.p[i], coast[i], 0, tuning(o)));
            }
            CHECK(rows_of(6, r) == want);
            CHECK(rows_of(6, climate_api::rainfall(5, sg.g, st.p, coast, tuning(o), THREADS)) == want);
        }
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

climate_api::Sweep sweep_of(const Terrain& t, std::uint8_t k, std::int64_t r, const climate_api::Tuning& tu) {
    const std::vector<std::int64_t> hs = climate_api::triangle_mean(t.g, t.s, r, THREADS);
    return climate_api::sweep(t.g, t.water, hs, k, tu);
}

// Applies the transform's inverse to a grid of the transformed shape: the
// inverse of a mirror or a transpose is itself; of a 90-degree clockwise
// rotation, three more rotations.
template <typename T>
std::vector<T> inverse(Grid g2, const std::vector<T>& v, Transform tr) {
    if (tr != Transform::rotate) {
        return transform<T>(g2, v, tr);
    }
    std::vector<T> a = v;
    Grid g = g2;
    for (int i = 0; i < 3; ++i) {
        a = transform<T>(g, a, Transform::rotate);
        g = transformed_grid(g, Transform::rotate);
    }
    return a;
}

// Invariant 7 for C5: sweeps sigma(water) and sigma(hs) with sigma(k) and
// maps q and p back (hs is transformed directly, as 10.1 test 4 states).
climate_api::Sweep transformed_sweep(Grid g, std::span<const std::uint8_t> water, std::span<const std::int64_t> hs,
                                     Transform tr, std::uint8_t k, const climate_api::Tuning& tu) {
    const Grid g2 = transformed_grid(g, tr);
    const climate_api::Sweep s2 = climate_api::sweep(g2, transform<std::uint8_t>(g, water, tr),
                                                     transform<std::int64_t>(g, hs, tr), code_under(tr, k), tu);
    return climate_api::Sweep{inverse(g2, s2.q, tr), inverse(g2, s2.p, tr)};
}

// The (transform, code) pairs of invariant 7 that fail on one grid, "" if none.
std::string symmetry_failures(Grid g, std::span<const std::uint8_t> water, std::span<const std::int64_t> hs,
                              const climate_api::Tuning& tu, bool with_rotation) {
    std::string bad;
    std::vector<Transform> trs = {Transform::mirror_x, Transform::mirror_y, Transform::transpose};
    if (with_rotation) {
        trs.push_back(Transform::rotate);
    }
    for (std::uint8_t k = 0; k < 8; ++k) {
        const climate_api::Sweep base = climate_api::sweep(g, water, hs, k, tu);
        for (const Transform tr : trs) {
            if (transformed_sweep(g, water, hs, tr, k, tu) != base) {
                bad += " (transform " + std::to_string(static_cast<int>(tr)) + ", code " + std::to_string(int{k}) + ")";
            }
        }
    }
    return bad;
}

// The triangle commutes with the transforms.
std::string triangle_symmetry_failures(Grid g, std::span<const std::int64_t> s, std::int64_t r) {
    std::string bad;
    const std::vector<std::int64_t> base = climate_api::triangle_mean(g, s, r, THREADS);
    for (const Transform tr : {Transform::mirror_x, Transform::mirror_y, Transform::transpose, Transform::rotate}) {
        const Grid g2 = transformed_grid(g, tr);
        const std::vector<std::int64_t> t2 = climate_api::triangle_mean(g2, transform<std::int64_t>(g, s, tr), r, THREADS);
        if (inverse(g2, t2, tr) != base) {
            bad += " (transform " + std::to_string(static_cast<int>(tr)) + ")";
        }
    }
    return bad;
}

// Grids without an ocean border and with random water (lakes, rivers and
// ocean anywhere; the off-map rule is symmetric too).
Terrain random_water_terrain(std::uint64_t f) {
    worldgen::Rng rng(worldgen::substream(worldgen::stream_key(2, STREAM_TEST), f));
    const Grid g{static_cast<std::int32_t>(2 + rng.uniform_u64(9)), static_cast<std::int32_t>(2 + rng.uniform_u64(9))};
    Terrain t{g, {}, {}};
    for (std::uint32_t i = 0; i < g.cells(); ++i) {
        const auto w = static_cast<std::uint8_t>(rng.uniform_u64(4));
        t.water.push_back(w);
        t.s.push_back(w == WATER_OCEAN ? 0 : rng.uniform_i64(1, 32767));
    }
    return t;
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
    CHECK(inverse(transformed_grid(t.g, Transform::rotate), transform<std::int64_t>(t.g, t.s, Transform::rotate),
                  Transform::rotate) == t.s);
    CHECK(code_under(Transform::rotate, 0) == 2);  // E -> S under a clockwise turn
    CHECK(code_under(Transform::rotate, 1) == 3);  // SE -> SW
    CHECK(code_under(Transform::mirror_x, 1) == 3);
    CHECK(code_under(Transform::mirror_y, 1) == 7);
    CHECK(code_under(Transform::transpose, 0) == 2);
    CHECK(code_under(Transform::transpose, 1) == 1);  // SE and NW map onto themselves
    CHECK(code_under(Transform::transpose, 5) == 5);
    CHECK(code_under(Transform::transpose, 3) == 7);  // SW and NE swap
}

namespace {

// 10.1 test 4 on random fixtures first to last - 1.
void check_fixture_symmetry(std::uint64_t first, std::uint64_t last) {
    const climate_api::Tuning tu = tuning(quiet_overrides());
    for (std::uint64_t f = first; f < last; ++f) {
        CAPTURE(f);
        const RandomFixture rf = random_fixture(f);
        const std::vector<std::int64_t> s = surface(rf.in);
        const std::vector<std::int64_t> hs = climate_api::triangle_mean(rf.in.grid, s, 20, THREADS);
        const std::string bad = symmetry_failures(rf.in.grid, rf.in.water, hs, tu, f < 20);
        CHECK_MESSAGE(bad.empty(), "failing:" << bad);
        if (f < 20) {
            const std::string tb = triangle_symmetry_failures(rf.in.grid, s, 20);
            CHECK_MESSAGE(tb.empty(), "triangle failing:" << tb);
        }
    }
}

}  // namespace

// The 200 fixtures take about 7 s of CPU on a release build: 0 to 49 run by
// default, 50 to 199 in climate-slow.
TEST_CASE("P1-03 6 invariant 7, 10.1 test 4: all 24 (generator, code) pairs on random fixtures 0 to 49") {
    check_fixture_symmetry(0, 50);
}

TEST_SUITE("climate-slow" * doctest::skip()) {
TEST_CASE("P1-03 6 invariant 7, 10.1 test 4 (slow): all 24 pairs on random fixtures 50 to 199") {
    check_fixture_symmetry(50, 200);
}
}  // TEST_SUITE

TEST_CASE("P1-03 6 invariant 7, 10.1 test 4: a non-square 7 x 5 grid, the 5 x 5 grid, and grids with random water") {
    std::vector<Terrain> terrains = {random_terrain(1, Grid{7, 5}), symmetric_terrain(), random_terrain(2, Grid{5, 7}),
                                     random_terrain(3, Grid{1, 6}), random_terrain(4, Grid{6, 1})};
    for (std::uint64_t f = 0; f < 12; ++f) {
        terrains.push_back(random_water_terrain(f));
    }
    for (const Overrides& o : {quiet_overrides(), Overrides{}}) {
        const climate_api::Tuning tu = tuning(o);
        for (std::size_t ti = 0; ti < terrains.size(); ++ti) {
            const Terrain& t = terrains[ti];
            for (const std::int64_t r : {0, 2, 3, 6}) {
                CAPTURE(ti);
                CAPTURE(t.g.width);
                CAPTURE(t.g.height);
                CAPTURE(r);
                const std::vector<std::int64_t> hs = climate_api::triangle_mean(t.g, t.s, r, THREADS);
                const std::string bad = symmetry_failures(t.g, t.water, hs, tu, true);
                CHECK_MESSAGE(bad.empty(), "failing:" << bad);
                const std::string tb = triangle_symmetry_failures(t.g, t.s, r);
                CHECK_MESSAGE(tb.empty(), "triangle failing:" << tb);
                const std::string bound = hs_bound_violation(t.g, t.s, r, hs);
                CHECK_MESSAGE(bound.empty(), bound);
            }
        }
    }
}

TEST_CASE("P1-03 C5 symmetry: on the symmetric 5 x 5 grid the orthogonal winds are rotations of E, the diagonal ones of SE") {
    const Terrain t = symmetric_terrain();
    const climate_api::Tuning tu = tuning(quiet_overrides());
    auto image = [&](const climate_api::Sweep& s, Transform tr) {
        return climate_api::Sweep{transform<std::int64_t>(t.g, s.q, tr), transform<std::int64_t>(t.g, s.p, tr)};
    };
    auto rot = [&](climate_api::Sweep s, int times) {
        for (int i = 0; i < times; ++i) {
            s = image(s, Transform::rotate);
        }
        return s;
    };
    for (const std::int64_t r : {0, 2}) {
        CAPTURE(r);
        const climate_api::Sweep e = sweep_of(t, 0, r, tu), se = sweep_of(t, 1, r, tu);
        CHECK(sweep_of(t, 2, r, tu) == rot(e, 1));
        CHECK(sweep_of(t, 4, r, tu) == rot(e, 2));
        CHECK(sweep_of(t, 6, r, tu) == rot(e, 3));
        CHECK(sweep_of(t, 3, r, tu) == rot(se, 1));
        CHECK(sweep_of(t, 5, r, tu) == rot(se, 2));
        CHECK(sweep_of(t, 7, r, tu) == rot(se, 3));
        CHECK(sweep_of(t, 3, r, tu) == image(se, Transform::mirror_x));
        CHECK(sweep_of(t, 7, r, tu) == image(se, Transform::mirror_y));
        CHECK(sweep_of(t, 1, r, tu) == image(se, Transform::transpose));
    }
}

TEST_CASE("P1-03 10.3: climate_triangle_mean is independent of the thread count on the random fixtures") {
    for (std::uint64_t f = 0; f < 200; f += 7) {
        CAPTURE(f);
        const RandomFixture rf = random_fixture(f);
        const std::vector<std::int64_t> s = surface(rf.in);
        const std::vector<std::int64_t> a = climate_api::triangle_mean(rf.in.grid, s, 20, 1);
        CHECK(climate_api::triangle_mean(rf.in.grid, s, 20, 2) == a);
        CHECK(climate_api::triangle_mean(rf.in.grid, s, 20, 5) == a);
        CHECK(a == own_triangle_mean(rf.in.grid, s, 20));
    }
}

TEST_CASE("P1-03 10.1 test 4, C6: the whole stage's rainfall turns with the terrain and the wind (rain noise 0)") {
    // With rain_noise_amp 0, rainfall depends on p and coast_dist only, so a
    // turned world gives the turned rainfall for every wind and every grid
    // symmetry. Temperature depends on the row (V), so only a mirror in x
    // leaves it unchanged (temp_noise_amp_c 0).
    const RandomFixture rf = random_fixture(3);
    const Inputs& in = rf.in;
    for (std::uint8_t k = 0; k < 8; ++k) {
        CAPTURE(int{k});
        Overrides o = quiet_overrides();
        o.emplace_back("wind_weights", weights_text(one_hot(k)));
        const climate_api::Layers base = stage_layers(in, tuning(o), 11, THREADS);
        for (const Transform tr : {Transform::mirror_x, Transform::mirror_y, Transform::transpose, Transform::rotate}) {
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

TEST_CASE("P1-03 2, C1, 10.1 test 7: input preconditions are fatal with the climate prefix (climate_layers and climate_surface)") {
    const SweepGrid sg = sweep_grid();
    std::vector<std::int16_t> coast(sg.s.size(), 16);
    const Inputs good = inputs_of_surface(sg.g, sg.s, sg.water, coast);
    const climate_api::Tuning t = tuning(quiet_overrides());
    CHECK_NOTHROW(static_cast<void>(stage_layers(good, t, 0, THREADS)));
    CHECK(climate_api::surface(good.grid, good.elevation, good.water, good.water_depth) == surface(good));
    auto prefixed = [](const worldgen::FatalError& e) {
        return std::string_view(e.what()).substr(0, PRECONDITION.size()) == PRECONDITION;
    };
    auto fatal_with_prefix = [&](const Inputs& in, const char* what) {
        CAPTURE(std::string_view(what));
        try {
            static_cast<void>(stage_layers(in, t, 0, THREADS));
            FAIL("climate_layers: no fatal error");
        } catch (const worldgen::FatalError& e) {
            CHECK(prefixed(e));
        }
        try {
            static_cast<void>(climate_api::surface(in.grid, in.elevation, in.water, in.water_depth));
            FAIL("climate_surface: no fatal error");
        } catch (const worldgen::FatalError& e) {
            CHECK(prefixed(e));
        }
    };
    {
        Inputs in = good;  // precondition 2: a water value of 4
        in.water[1] = 4;
        fatal_with_prefix(in, "water 4");
        in.water[1] = 255;
        fatal_with_prefix(in, "water 255");
    }
    {
        Inputs in = good;  // precondition 3: S > 32767 on a land cell
        in.elevation[1] = 32767;
        in.water_depth[1] = 1;
        fatal_with_prefix(in, "S = 32768");
    }
    {
        Inputs in = good;  // S = 32767 exactly is allowed, on a lake or on land
        in.water[1] = WATER_LAKE;
        in.elevation[1] = 32000;
        in.water_depth[1] = 767;
        in.elevation[2] = 32767;
        CHECK_NOTHROW(static_cast<void>(stage_layers(in, t, 0, THREADS)));
        CHECK_NOTHROW(static_cast<void>(climate_api::surface(in.grid, in.elevation, in.water, in.water_depth)));
    }
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

TEST_CASE("P1-03 C2, C5 seams: wind and sweep argument checks") {
    // climate_wind: fatal unless 8 weights, none negative, sum >= 1.
    const std::vector<std::int64_t> seven(7, 1), nine(9, 1), zeros(8, 0);
    std::vector<std::int64_t> negative(8, 1);
    negative[3] = -1;
    const std::vector<const std::vector<std::int64_t>*> bad = {&seven, &nine, &zeros, &negative};
    for (const std::vector<std::int64_t>* w : bad) {
        CHECK_THROWS_AS(static_cast<void>(climate_api::wind(0, *w)), worldgen::FatalError);
    }
    // climate_sweep: wind > 7 is fatal.
    const SweepGrid sg = sweep_grid();
    CHECK_THROWS_AS(static_cast<void>(climate_api::sweep(sg.g, sg.water, sg.hs, 8, shipped_tuning())),
                    worldgen::FatalError);
}
