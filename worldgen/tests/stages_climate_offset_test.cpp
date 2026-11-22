// The per-world temperature offset of the climate stage (spec P1-03 revised
// 2026-11-18, generator 0.9.0): C2b, C3 with the offset, invariants 1 and 8,
// the stream and the tuning field (10.1 tests 0b and 6, 10.2).
//
// Written from the spec only. Every stage call goes through
// p1_03_climate_api.hpp; the test's own C2b and C3 are in
// p1_03_climate_support.hpp. The same numbers are reproduced by
// reference/p1_climate_reference.py check.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p1_03_climate_api.hpp"
#include "p1_03_climate_support.hpp"
#include "worldgen/core/error.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/core/rng.hpp"
#include "worldgen/core/streams.hpp"

using namespace climate_test;

namespace {

constexpr std::uint64_t PHRASE_SEED = 15629206466951235176ULL;  // "The Unmapped" (P1-03 5.1)

}  // namespace

TEST_CASE("P1-03 5.1 C2b, 10.1 test 0b: offsets of seeds 0 to 11, 42 and the phrase with the shipped M = 20") {
    const std::vector<std::int64_t> want = {20, 10, -11, 14, 2, 8, 6, -2, -19, -13, 11, 10};
    const Values v = values();
    CHECK(own_offset_m(v) == 20);
    for (std::uint64_t seed = 0; seed < want.size(); ++seed) {
        CAPTURE(seed);
        CHECK(climate_api::temp_offset_dc(seed, shipped_tuning()) == want[seed]);
        CHECK(own_offset_dc(seed, v) == want[seed]);
    }
    CHECK(climate_api::temp_offset_dc(42, shipped_tuning()) == 9);
    CHECK(climate_api::temp_offset_dc(PHRASE_SEED, shipped_tuning()) == -10);
    // By hand: the first uniform_i64(-20, 20) of the stream, one draw.
    worldgen::Rng rng(worldgen::stream_key(0, STREAM_OFFSET));
    CHECK(rng.uniform_i64(-20, 20) == 20);
}

TEST_CASE("P1-03 C2b, 10.1 test 0b: M = 0 gives 0; 0.04 rounds to 0, 0.05 to 1; 5 reaches both -50 and 50") {
    const climate_api::Tuning t0 = tuning({{"temp_offset_max_c", "0"}});
    const climate_api::Tuning t004 = tuning({{"temp_offset_max_c", "\"0.04\""}});
    const climate_api::Tuning t005 = tuning({{"temp_offset_max_c", "\"0.05\""}});
    std::set<std::int64_t> s005;
    for (std::uint64_t seed = 0; seed < 200; ++seed) {
        CAPTURE(seed);
        CHECK(climate_api::temp_offset_dc(seed, t0) == 0);
        CHECK(climate_api::temp_offset_dc(seed, t004) == 0);
        const std::int64_t o = climate_api::temp_offset_dc(seed, t005);
        CHECK(o >= -1);
        CHECK(o <= 1);
        s005.insert(o);
    }
    CHECK(s005 == std::set<std::int64_t>{-1, 0, 1});
    const climate_api::Tuning t5 = tuning({{"temp_offset_max_c", "5"}});
    std::int64_t lo = 0;
    std::int64_t hi = 0;
    bool in_range = true;
    for (std::uint64_t seed = 0; seed < 10000; ++seed) {
        const std::int64_t o = climate_api::temp_offset_dc(seed, t5);
        in_range = in_range && o >= -50 && o <= 50;
        lo = std::min(lo, o);
        hi = std::max(hi, o);
    }
    CHECK(in_range);
    CHECK(lo == -50);
    CHECK(hi == 50);
    // The seeds of the golden extremes (p1-03-climate.txt).
    CHECK(climate_api::temp_offset_dc(88, shipped_tuning()) == -20);
    CHECK(climate_api::temp_offset_dc(70, shipped_tuning()) == 0);
    CHECK(climate_api::temp_offset_dc(88, t5) == -50);
    CHECK(climate_api::temp_offset_dc(218, t5) == 50);
}

TEST_CASE("P1-03 10: climate_temp_offset_dc is fatal unless temp_offset_max_c is in [0, 5]") {
    const std::int64_t five = std::int64_t{5} << 32;
    CHECK_NOTHROW(static_cast<void>(
        climate_api::temp_offset_dc(0, climate_api::with_offset_max_raw(shipped_tuning(), five))));
    CHECK_NOTHROW(static_cast<void>(
        climate_api::temp_offset_dc(0, climate_api::with_offset_max_raw(shipped_tuning(), 0))));
    CHECK_THROWS_AS(static_cast<void>(
                        climate_api::temp_offset_dc(0, climate_api::with_offset_max_raw(shipped_tuning(), five + 1))),
                    worldgen::FatalError);
    CHECK_THROWS_AS(static_cast<void>(
                        climate_api::temp_offset_dc(0, climate_api::with_offset_max_raw(shipped_tuning(), -1))),
                    worldgen::FatalError);
}

TEST_CASE("P1-03 5.1 C3 with the offset, 10.1 test 0b: the offset table exactly; offset_dc 51 is fatal") {
    struct Row {
        std::int32_t n, y;
        std::int64_t s, off, T_raw;
        std::int16_t temperature;
    };
    const std::vector<Row> rows = {
        {1024, 511, 12000, -19, -11604800307, -27}, {1024, 511, 12000, 9, 421108121, 1},
        {1024, 511, 12000, 20, 5145572147, 12},     {1024, 0, 0, -19, 13322787226, 31},
        {1024, 0, 0, 20, 30073159680, 70},          {512, 100, 7000, -19, -2855482163, -7},
        {512, 100, 7000, 9, 9170426265, 21},
    };
    const Overrides quiet = {{"temp_noise_amp_c", "0"}};
    const Values v = values(quiet);
    const climate_api::Tuning t = tuning(quiet);
    for (const Row& r : rows) {
        CAPTURE(r.n);
        CAPTURE(r.y);
        CAPTURE(r.off);
        const TempParts own = own_temperature(r.y, r.n, r.s, 0, v, r.off);
        CHECK(own.T == r.T_raw);
        CHECK(own.temperature == r.temperature);
        CHECK(climate_api::temperature_cell(r.y, r.n, r.s, 0, r.off, t) == r.temperature);
    }
    CHECK(Fx::from_ratio(-19, 10).raw() == -8160437862);
    CHECK(Fx::from_ratio(9, 10).raw() == 3865470566);
    CHECK(Fx::from_ratio(20, 10).raw() == 8589934592);
    CHECK((30056382464 - 8160437862) - 33500744909 == -11604800307);  // by hand, first row
    CHECK_NOTHROW(static_cast<void>(climate_api::temperature_cell(0, 16, 0, 0, 50, t)));
    CHECK_NOTHROW(static_cast<void>(climate_api::temperature_cell(0, 16, 0, 0, -50, t)));
    CHECK_THROWS_AS(static_cast<void>(climate_api::temperature_cell(0, 16, 0, 0, 51, t)), worldgen::FatalError);
    CHECK_THROWS_AS(static_cast<void>(climate_api::temperature_cell(0, 16, 0, 0, -51, t)), worldgen::FatalError);
}

TEST_CASE("P1-03 C3: climate_temperature on a grid adds the given offset; climate_layers uses the seed's offset") {
    const Inputs in = world_inputs(0, 64, false);
    const std::vector<std::int64_t> s = surface(in);
    const Values v = values();
    const std::vector<std::int64_t> noise = noise_grid(0, STREAM_TEMP, in.grid, v.temp_noise);
    for (const std::int64_t off : {std::int64_t{-50}, std::int64_t{0}, std::int64_t{20}, std::int64_t{50}}) {
        CAPTURE(off);
        const std::vector<std::int16_t> got = climate_api::temperature(0, in.grid, s, off, shipped_tuning(), THREADS);
        bool same = true;
        for (std::int32_t y = 0; y < in.grid.height; ++y) {
            for (std::int32_t x = 0; x < in.grid.width; ++x) {
                const std::size_t i = in.grid.index(x, y);
                same = same && got[i] == own_temperature(y, in.grid.height, s[i], noise[i], v, off).temperature;
            }
        }
        CHECK(same);
    }
    const climate_api::Layers l = stage_layers(in, shipped_tuning(), 0, THREADS);
    CHECK(l.temperature == climate_api::temperature(0, in.grid, s, 20, shipped_tuning(), THREADS));
}

TEST_CASE("P1-03 6 invariants 1 and 8, 10.2: the offset is a shift; rainfall and wind do not depend on it (N = 128)") {
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{2}, std::uint64_t{8}, std::uint64_t{42}}) {
        CAPTURE(seed);
        const Inputs in = world_inputs(seed, 128, false);
        const climate_api::Tuning ta = tuning({{"temp_noise_amp_c", "0"}});
        const climate_api::Tuning t0 = tuning({{"temp_noise_amp_c", "0"}, {"temp_offset_max_c", "0"}});
        const climate_api::Layers a = stage_layers(in, ta, seed, THREADS);
        const climate_api::Layers b = stage_layers(in, t0, seed, THREADS);
        const std::int64_t off = climate_api::temp_offset_dc(seed, ta);
        CHECK(climate_api::temp_offset_dc(seed, t0) == 0);
        CHECK(off >= -20);
        CHECK(off <= 20);
        bool shift = true;
        for (std::size_t i = 0; i < a.temperature.size(); ++i) {
            const std::int64_t ta_i = a.temperature[i];
            const std::int64_t tb_i = b.temperature[i];
            if (ta_i > TEMP_MIN && ta_i < TEMP_MAX && tb_i > TEMP_MIN && tb_i < TEMP_MAX) {
                const std::int64_t d = ta_i - tb_i - off;
                shift = shift && d >= -1 && d <= 1;
            }
        }
        CHECK(shift);
        CHECK(a.rainfall == b.rainfall);
        // With offset 0 the test's own C3 (no offset term) gives the stage's temperature.
        const Own own0 = own_climate(in, values({{"temp_noise_amp_c", "0"}, {"temp_offset_max_c", "0"}}), seed);
        CHECK(b.temperature == own0.temperature);
        // Any other temp_offset_max_c changes only temperature.
        const climate_api::Layers c = stage_layers(in, tuning({{"temp_offset_max_c", "5"}}), seed, THREADS);
        const climate_api::Layers d = stage_layers(in, shipped_tuning(), seed, THREADS);
        CHECK(c.rainfall == d.rainfall);
        CHECK(climate_api::wind(seed, values().wind_weights) ==
              climate_api::wind(seed, values({{"temp_offset_max_c", "5"}}).wind_weights));
    }
}

TEST_CASE("P1-03 C2b, P1-00 3: the stream geo.climate.temp_offset is registered; 52 stream names in all (P3-08)") {
    const auto& names = worldgen::streams::all_stream_names;
    CHECK(std::find(names.begin(), names.end(), STREAM_OFFSET) != names.end());
    CHECK(names.size() == 54U);  // P3-06 r22 24.5 adds hist.conflict.toll (0.21.0); r10 20.17 hist.conflict.search (0.18.0). P1-04 appendix B: the 0.8.0 26 plus one; P1-05 geo.resources.ore; P1-06 two; P3-02 three; P3-03 one; P3-05 twelve; P3-06 five; P1-06 r4 geo.regions.name (P0-01 amendment 20)
    // The biome exploration's streams are not registered (P1-04 0, appendix B).
    CHECK(std::find(names.begin(), names.end(), std::string_view("geo.biomes.wet_jitter")) == names.end());
    CHECK(std::find(names.begin(), names.end(), std::string_view("geo.biomes.elev_jitter")) == names.end());
    CHECK(worldgen::stream_key(0, STREAM_OFFSET) != worldgen::stream_key(0, STREAM_WIND));
}

TEST_CASE("P1-03 golden, invariant 8: with M = 0 the temperature item is the 0.8.0 one; offsets at both extremes") {
    // fe2a4438... is shipped-64-s0's temperature in the 0.8.0 file (offset 0, and the coast bonus does not touch
    // temperature); shipped-64-s0-off0 and -off004 (M = 0) must reproduce it.
    const std::string t080 = "fe2a4438b0ad7647aaa8bd8e6dc54c33095b656b82ff87b5a0cd3ef48f722c22";
    std::map<std::string, std::string> offsets;
    for (const GoldenCase& c : read_golden(UNMAPPED_P1_03_GOLDEN_PATH)) {
        if (c.name == "shipped-64-s0-off0" || c.name == "shipped-64-s0-off004") {
            CHECK(c.get("temperature") == t080);
        }
        if (c.fixture != "overflow") {
            offsets[c.name] = c.get("temp_offset_dc");
        }
    }
    CHECK(offsets["shipped-64-s0"] == "20");
    CHECK(offsets["shipped-64-s88"] == "-20");
    CHECK(offsets["shipped-64-s70"] == "0");
    CHECK(offsets["shipped-64-s0-off0"] == "0");
    CHECK(offsets["shipped-64-s88-off5"] == "-50");
    CHECK(offsets["shipped-64-s218-off5"] == "50");
}

TEST_CASE("P1-03 6 invariant 8: with temp_offset_max_c 0 the stage is the 0.8.0 stage on the 0.8.0 tuning") {
    // P1-04 6 item 9: with coast_rain_mm 75 and no offset the climate layers
    // are those of 0.8.0; checked against the test's own C3 to C6, which add
    // no offset term when off_dc = 0.
    const Inputs in = world_inputs(0, 64, false);
    const climate_api::Layers l = stage_layers(in, tuning(legacy_overrides()), 0, THREADS);
    const Own own = own_climate(in, values(legacy_overrides()), 0);
    CHECK(own.offset_dc == 0);
    CHECK(l.temperature == own.temperature);
    CHECK(l.rainfall == own.rainfall);
}
