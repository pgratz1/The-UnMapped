// Noise records and compositions shared by the geography stages (spec
// P1-01 section 4.1; P1-00 sections 6.2 and 12; P0-01 section 15): fbm_map,
// fbm_m and ridged_m, and the TOML schema helpers of the fbm_map and fbm_m
// records.
//
// Written from docs/specs/p1-01-elevation.md 4.1,
// docs/specs/p1-00-geography-overview.md, docs/specs/p0-03-toml-loader.md and
// the public headers worldgen/geo/noise.hpp and worldgen/geo/noise_schema.hpp
// only. ridged_m is recomputed here from noise2 and substream statement by
// statement, as P1-01 4.1 writes it.

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "core_test_util.hpp"
#include "data_loader_support.hpp"
#include "worldgen/core/core.hpp"
#include "worldgen/geo/noise.hpp"
#include "worldgen/geo/noise_schema.hpp"

namespace geo = worldgen::geo;
namespace wd = worldgen::data;
using worldgen::Fx;
using worldgen_test::fxi;
using worldgen_test::raw;

namespace {

constexpr std::uint64_t KEY_NOISE_P1 = 0x544553544E503131ULL;

Fx dec(const char* text) { return worldgen::parse_decimal(text).value; }

// P1-01 4.1, ridged_m, one statement per line as written there.
Fx reference_ridged_m(std::uint64_t key, Fx a, Fx b, const geo::FbmM& rec, Fx weight) {
    const Fx LAC = Fx::from_int(2);
    Fx f = Fx::from_ratio(1, rec.wavelength_m);
    Fx amp = Fx::ONE;
    Fx wgt = Fx::ONE;
    Fx sum = Fx::from_raw(0);
    Fx norm = Fx::from_raw(0);
    for (std::int32_t o = 0; o < rec.octaves; ++o) {
        const Fx n = worldgen::noise2(worldgen::substream(key, static_cast<std::uint64_t>(o)), worldgen::mul(a, f),
                                      worldgen::mul(b, f));
        Fx s = Fx::ONE - worldgen::abs(n);
        s = worldgen::mul(s, s);
        s = worldgen::mul(s, wgt);
        wgt = worldgen::clamp(worldgen::mul(s, weight), Fx::from_raw(0), Fx::ONE);
        sum = sum + worldgen::mul(s, amp);
        norm = norm + amp;
        amp = worldgen::mul(amp, rec.gain);
        f = worldgen::mul(f, LAC);
    }
    return worldgen::clamp(worldgen::div(sum, norm), Fx::from_raw(0), Fx::ONE);
}

// A seeded sample point: map fractions (2x + 1) / (2N), or metres 16x + 8
// (P1-00 2.1), on maps up to N = 2048.
struct Sample {
    Fx u;
    Fx v;
    Fx xm;
    Fx ym;
};

Sample sample(worldgen::Rng& rng) {
    const std::uint64_t n_pick = rng.uniform_u64(3U);
    const std::int64_t n = n_pick == 0U ? 512 : (n_pick == 1U ? 1024 : 2048);
    const std::int64_t x = rng.uniform_i64(0, n - 1);
    const std::int64_t y = rng.uniform_i64(0, n - 1);
    return Sample{Fx::from_ratio(2 * x + 1, 2 * n), Fx::from_ratio(2 * y + 1, 2 * n), Fx::from_int(16 * x + 8),
                  Fx::from_int(16 * y + 8)};
}

geo::FbmMap random_fbm_map(worldgen::Rng& rng) {
    geo::FbmMap r;
    r.cycles = Fx::from_raw(rng.uniform_i64(536870912, std::int64_t{64} << 32U));  // [0.125, 64]
    r.octaves = static_cast<std::int32_t>(rng.uniform_i64(1, 10));
    r.gain = Fx::from_raw(rng.uniform_i64(268435456, worldgen_test::ONE_RAW));  // [0.0625, 1]
    return r;
}

geo::FbmM random_fbm_m(worldgen::Rng& rng) {
    geo::FbmM r;
    r.wavelength_m = rng.uniform_i64(64, 65536);
    r.octaves = static_cast<std::int32_t>(rng.uniform_i64(1, 10));
    r.gain = Fx::from_raw(rng.uniform_i64(268435456, worldgen_test::ONE_RAW));
    return r;
}

}  // namespace

TEST_CASE("P1-01 4.1: LAC is the constant 2") {
    CHECK(geo::LACUNARITY == 2);
    CHECK(geo::lacunarity() == Fx::from_int(2));
    CHECK(geo::lacunarity().raw() == (std::int64_t{1} << 33U));
}

TEST_CASE("P1-01 4.1: fbm_map(K, a, b, rec) = fbm2(K, a, b, rec.octaves, rec.cycles, LAC, rec.gain)") {
    worldgen::Rng rng(KEY_NOISE_P1);
    for (int i = 0; i < 400; ++i) {
        const std::uint64_t key = rng.next();
        const geo::FbmMap rec = random_fbm_map(rng);
        const Sample s = sample(rng);
        CAPTURE(i);
        const Fx expected = worldgen::fbm2(key, s.u, s.v, rec.octaves, rec.cycles, Fx::from_int(2), rec.gain);
        CHECK(geo::fbm_map(key, s.u, s.v, rec) == expected);
    }
    // The bounds of the record.
    const std::uint64_t key = worldgen::stream_key(0, worldgen::streams::test_noise);
    for (const geo::FbmMap rec : {geo::FbmMap{dec("0.125"), 1, dec("0.0625")}, geo::FbmMap{fxi(64), 10, Fx::ONE},
                                  geo::FbmMap{fxi(2), 4, Fx::HALF}}) {
        for (const Fx u : {raw(1), Fx::HALF, raw(worldgen_test::ONE_RAW - 1)}) {
            CHECK(geo::fbm_map(key, u, u, rec) ==
                  worldgen::fbm2(key, u, u, rec.octaves, rec.cycles, Fx::from_int(2), rec.gain));
        }
    }
}

TEST_CASE("P1-01 4.1: fbm_m(K, a, b, rec) = fbm2(K, a, b, rec.octaves, from_ratio(1, rec.wavelength_m), LAC, rec.gain)") {
    worldgen::Rng rng(KEY_NOISE_P1 + 1U);
    for (int i = 0; i < 400; ++i) {
        const std::uint64_t key = rng.next();
        const geo::FbmM rec = random_fbm_m(rng);
        const Sample s = sample(rng);
        CAPTURE(i);
        const Fx expected = worldgen::fbm2(key, s.xm, s.ym, rec.octaves, Fx::from_ratio(1, rec.wavelength_m),
                                           Fx::from_int(2), rec.gain);
        CHECK(geo::fbm_m(key, s.xm, s.ym, rec) == expected);
    }
}

TEST_CASE("P1-01 4.1: ridged_m equals the pseudocode, statement by statement") {
    worldgen::Rng rng(KEY_NOISE_P1 + 2U);
    for (int i = 0; i < 600; ++i) {
        const std::uint64_t key = rng.next();
        const geo::FbmM rec = random_fbm_m(rng);
        const Sample s = sample(rng);
        // Weights in [0, 4], including 0 and exactly ONE.
        const std::uint64_t wpick = rng.uniform_u64(4U);
        const Fx weight = wpick == 0U   ? Fx::from_raw(0)
                          : wpick == 1U ? Fx::ONE
                                        : Fx::from_raw(rng.uniform_i64(0, 4 * worldgen_test::ONE_RAW));
        CAPTURE(i);
        const Fx expected = reference_ridged_m(key, s.xm, s.ym, rec, weight);
        const Fx got = geo::ridged_m(key, s.xm, s.ym, rec, weight);
        CHECK(got == expected);
        CHECK(got.raw() >= 0);
        CHECK(got.raw() <= worldgen_test::ONE_RAW);
    }
}

TEST_CASE("P1-01 4.1: ridged_m up to 16 octaves and at lattice points") {
    const std::uint64_t key = worldgen::stream_key(42, worldgen::streams::test_noise);
    for (const std::int32_t oct : {1, 2, 10, 16}) {
        const geo::FbmM rec{64, oct, Fx::HALF};
        for (const std::int64_t m : {std::int64_t{0}, std::int64_t{8}, std::int64_t{64}, std::int64_t{32760}}) {
            CAPTURE(oct);
            CAPTURE(m);
            const Fx a = Fx::from_int(m);
            CHECK(geo::ridged_m(key, a, a, rec, Fx::ONE) == reference_ridged_m(key, a, a, rec, Fx::ONE));
        }
    }
    // At the origin every octave's noise is 0, so s = ONE * wgt and with
    // weight ONE every octave contributes ONE: the result is exactly ONE.
    CHECK(geo::ridged_m(key, Fx::from_raw(0), Fx::from_raw(0), geo::FbmM{1000, 5, Fx::HALF}, Fx::ONE) == Fx::ONE);
    // With weight 0 only the first octave contributes: sum = ONE * ONE,
    // norm = 1 + gain + gain^2 ...; the result is 1 / norm.
    const Fx r0 = geo::ridged_m(key, Fx::from_raw(0), Fx::from_raw(0), geo::FbmM{1000, 2, Fx::HALF}, Fx::from_raw(0));
    CHECK(r0 == worldgen::div(Fx::ONE, Fx::ONE + Fx::HALF));
}

TEST_CASE("P1-01 4.1: the compositions stay in range") {
    worldgen::Rng rng(KEY_NOISE_P1 + 3U);
    for (int i = 0; i < 300; ++i) {
        const std::uint64_t key = rng.next();
        const Sample s = sample(rng);
        const Fx a = geo::fbm_map(key, s.u, s.v, random_fbm_map(rng));
        const Fx b = geo::fbm_m(key, s.xm, s.ym, random_fbm_m(rng));
        const Fx c = geo::ridged_m(key, s.xm, s.ym, random_fbm_m(rng), Fx::from_int(2));
        CHECK(a.raw() >= -worldgen_test::ONE_RAW);
        CHECK(a.raw() <= worldgen_test::ONE_RAW);
        CHECK(b.raw() >= -worldgen_test::ONE_RAW);
        CHECK(b.raw() <= worldgen_test::ONE_RAW);
        CHECK(c.raw() >= 0);
        CHECK(c.raw() <= worldgen_test::ONE_RAW);
    }
}

TEST_CASE("P1-01 4.1: argument checks") {
    const std::uint64_t key = 12345U;
    const Fx a = Fx::from_int(100);
    // fbm_map: as fbm2 (1 <= octaves <= 16, base_freq > 0, 0 < gain <= ONE).
    CHECK_FATAL(geo::fbm_map(key, a, a, geo::FbmMap{fxi(2), 0, Fx::HALF}));
    CHECK_FATAL(geo::fbm_map(key, a, a, geo::FbmMap{fxi(2), 17, Fx::HALF}));
    CHECK_FATAL(geo::fbm_map(key, a, a, geo::FbmMap{Fx::from_raw(0), 4, Fx::HALF}));
    CHECK_FATAL(geo::fbm_map(key, a, a, geo::FbmMap{fxi(-1), 4, Fx::HALF}));
    CHECK_FATAL(geo::fbm_map(key, a, a, geo::FbmMap{fxi(2), 4, Fx::from_raw(0)}));
    CHECK_FATAL(geo::fbm_map(key, a, a, geo::FbmMap{fxi(2), 4, raw(worldgen_test::ONE_RAW + 1)}));
    static_cast<void>(geo::fbm_map(key, a, a, geo::FbmMap{fxi(2), 16, Fx::ONE}));  // valid for fbm2
    // fbm_m: wavelength 0 is fatal in from_ratio; a negative one gives base_freq < 0.
    CHECK_FATAL(geo::fbm_m(key, a, a, geo::FbmM{0, 4, Fx::HALF}));
    CHECK_FATAL(geo::fbm_m(key, a, a, geo::FbmM{-64, 4, Fx::HALF}));
    CHECK_FATAL(geo::fbm_m(key, a, a, geo::FbmM{64, 0, Fx::HALF}));
    CHECK_FATAL(geo::fbm_m(key, a, a, geo::FbmM{64, 17, Fx::HALF}));
    CHECK_FATAL(geo::fbm_m(key, a, a, geo::FbmM{64, 4, Fx::from_raw(0)}));
    // ridged_m: octaves outside [1, 16]; wavelength 0.
    CHECK_FATAL(geo::ridged_m(key, a, a, geo::FbmM{64, 0, Fx::HALF}, Fx::ONE));
    CHECK_FATAL(geo::ridged_m(key, a, a, geo::FbmM{64, 17, Fx::HALF}, Fx::ONE));
    CHECK_FATAL(geo::ridged_m(key, a, a, geo::FbmM{64, -1, Fx::HALF}, Fx::ONE));
    CHECK_FATAL(geo::ridged_m(key, a, a, geo::FbmM{0, 4, Fx::HALF}, Fx::ONE));
    // An overflow of mul(a, f) in a later octave is fatal, as in the
    // reference: a = 2^30 m, wavelength 64 -> f = 2^-6 * 2^o; at o = 7,
    // a * f = 2^31 (out of Fx range).
    const Fx far = Fx::from_int(std::int64_t{1} << 30U);
    CHECK_FATAL(reference_ridged_m(key, far, a, geo::FbmM{64, 10, Fx::HALF}, Fx::ONE));
    CHECK_FATAL(geo::ridged_m(key, far, a, geo::FbmM{64, 10, Fx::HALF}, Fx::ONE));
    CHECK_FATAL(geo::fbm_m(key, far, a, geo::FbmM{64, 10, Fx::HALF}));
}

// --- Schema helpers (P1-01 4.1, P1-00 6.2) ----------------------------------------

TEST_CASE("P1-01 4.1: fbm_map and fbm_m schemas: fields, order, types and bounds") {
    const wd::Schema m = geo::fbm_map_schema();
    REQUIRE(m.fields().size() == 3U);
    CHECK(m.fields()[0].name == "cycles");
    CHECK(m.fields()[1].name == "octaves");
    CHECK(m.fields()[2].name == "gain");
    for (const wd::Field& f : m.fields()) {
        CHECK(f.is_required());
    }
    CHECK(m.fields()[0].type.kind() == wd::TypeKind::fixed);
    CHECK(m.fields()[0].type.fx_min().raw() == 536870912);                    // 0.125
    CHECK(m.fields()[0].type.fx_max().raw() == (std::int64_t{64} << 32U));     // 64
    CHECK(m.fields()[1].type.kind() == wd::TypeKind::integer);
    CHECK(m.fields()[1].type.int_min() == 1);
    CHECK(m.fields()[1].type.int_max() == 10);
    CHECK(m.fields()[2].type.kind() == wd::TypeKind::fixed);
    CHECK(m.fields()[2].type.fx_min().raw() == 268435456);  // 0.0625
    CHECK(m.fields()[2].type.fx_max().raw() == worldgen_test::ONE_RAW);
    CHECK(m.checks().empty());

    const wd::Schema w = geo::fbm_m_schema();
    REQUIRE(w.fields().size() == 3U);
    CHECK(w.fields()[0].name == "wavelength_m");
    CHECK(w.fields()[1].name == "octaves");
    CHECK(w.fields()[2].name == "gain");
    for (const wd::Field& f : w.fields()) {
        CHECK(f.is_required());
    }
    CHECK(w.fields()[0].type.kind() == wd::TypeKind::integer);
    CHECK(w.fields()[0].type.int_min() == 64);
    CHECK(w.fields()[0].type.int_max() == 65536);
    CHECK(w.fields()[1].type.int_min() == 1);
    CHECK(w.fields()[1].type.int_max() == 10);
    CHECK(w.fields()[2].type.fx_min().raw() == 268435456);
    CHECK(w.fields()[2].type.fx_max().raw() == worldgen_test::ONE_RAW);

    CHECK(geo::fbm_map_type().kind() == wd::TypeKind::record);
    CHECK(geo::fbm_map_type().schema().fields().size() == 3U);
    CHECK(geo::fbm_m_type().kind() == wd::TypeKind::record);
    CHECK(geo::fbm_m_type().schema().fields()[0].name == "wavelength_m");
}

namespace {

// A test kind `noisy` (directory noisies) with one field of each record type.
wd::KindRegistry noisy_registry() {
    std::vector<wd::Field> fields;
    fields.push_back(wd::Field::required("map", geo::fbm_map_type()));
    fields.push_back(wd::Field::required("metres", geo::fbm_m_type()));
    wd::Kind k;
    k.name = "noisy";
    k.directory = "noisies";
    k.required = true;
    k.schema = wd::Schema(std::move(fields));
    wd::KindRegistry r;
    r.add(std::move(k));
    return r;
}

wd::LoadResult<wd::ContentSet> load_noisy(const std::string& map_rec, const std::string& m_rec) {
    const std::string text = "[noisy.n]\nmap = " + map_rec + "\nmetres = " + m_rec + "\n";
    wd::MemoryDataRoot root;
    root.directories.push_back(loader_test::dir("noisies", {loader_test::file("n.toml", text)}));
    return wd::load_content_from_memory(noisy_registry(), root);
}

const std::string GOOD_MAP = R"({ cycles = "2", octaves = 4, gain = "0.5" })";
const std::string GOOD_M = R"({ wavelength_m = 3000, octaves = 3, gain = "0.5" })";

}  // namespace

TEST_CASE("P1-01 4.1: the record schemas accept their bounds and reject values just outside") {
    const auto ok = [](const wd::LoadResult<wd::ContentSet>& r) {
        loader_test::check_content_result(r);
        return r.ok();
    };
    CHECK(ok(load_noisy(GOOD_MAP, GOOD_M)));
    // fbm_map.cycles fx(0.125, 64).
    CHECK(ok(load_noisy(R"({ cycles = "0.125", octaves = 4, gain = "0.5" })", GOOD_M)));
    CHECK(ok(load_noisy(R"({ cycles = 64, octaves = 4, gain = "0.5" })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = "0.1249", octaves = 4, gain = "0.5" })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = "64.0001", octaves = 4, gain = "0.5" })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = 0, octaves = 4, gain = "0.5" })", GOOD_M)));
    // octaves int(1, 10).
    CHECK(ok(load_noisy(R"({ cycles = "2", octaves = 1, gain = "0.5" })", GOOD_M)));
    CHECK(ok(load_noisy(R"({ cycles = "2", octaves = 10, gain = "0.5" })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = "2", octaves = 0, gain = "0.5" })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = "2", octaves = 11, gain = "0.5" })", GOOD_M)));
    // gain fx(0.0625, 1).
    CHECK(ok(load_noisy(R"({ cycles = "2", octaves = 4, gain = "0.0625" })", GOOD_M)));
    CHECK(ok(load_noisy(R"({ cycles = "2", octaves = 4, gain = 1 })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = "2", octaves = 4, gain = "0.0624" })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = "2", octaves = 4, gain = "1.0001" })", GOOD_M)));
    // fbm_m.wavelength_m int(64, 65536).
    CHECK(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 64, octaves = 3, gain = "0.5" })")));
    CHECK(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 65536, octaves = 3, gain = "0.5" })")));
    CHECK_FALSE(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 63, octaves = 3, gain = "0.5" })")));
    CHECK_FALSE(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 65537, octaves = 3, gain = "0.5" })")));
    CHECK_FALSE(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = "3000", octaves = 3, gain = "0.5" })")));
    CHECK_FALSE(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 3000, octaves = 0, gain = "0.5" })")));
    CHECK_FALSE(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 3000, octaves = 11, gain = "0.5" })")));
    CHECK_FALSE(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 3000, octaves = 3, gain = "0.0624" })")));
    CHECK_FALSE(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 3000, octaves = 3, gain = "1.0001" })")));
    // Every field required (P1-00 6.1), no unknown fields; fx fields reject
    // a TOML float.
    CHECK_FALSE(ok(load_noisy(R"({ cycles = "2", octaves = 4 })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ octaves = 4, gain = "0.5" })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(GOOD_MAP, R"({ wavelength_m = 3000, gain = "0.5" })")));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = "2", octaves = 4, gain = "0.5", lacunarity = 2 })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(R"({ cycles = 2.0, octaves = 4, gain = "0.5" })", GOOD_M)));
    CHECK_FALSE(ok(load_noisy(GOOD_M, GOOD_M)));
}

TEST_CASE("P1-01 4.1: fbm_map_from / fbm_m_from read a loaded record; canonical encoding of a record field") {
    const auto r = load_noisy(R"({ gain = "0.75", octaves = 6, cycles = "1.5" })",
                              R"({ wavelength_m = 4096, octaves = 2, gain = "0.25" })");
    REQUIRE(r.ok());
    const wd::Definition* d = r.value->find("noisy", "n");
    REQUIRE(d != nullptr);
    const geo::FbmMap m = geo::fbm_map_from(d->record.get("map"));
    CHECK(m == geo::FbmMap{dec("1.5"), 6, dec("0.75")});
    const geo::FbmM w = geo::fbm_m_from(d->record.get("metres"));
    CHECK(w == geo::FbmM{4096, 2, dec("0.25")});
    // Fields are held in declaration order whatever the order in the file.
    const wd::Record& mr = d->record.get_record("map");
    REQUIRE(mr.size() == 3U);
    CHECK(mr.name_at(0) == "cycles");
    CHECK(mr.name_at(1) == "octaves");
    CHECK(mr.name_at(2) == "gain");
    // Canonical encoding (P0-03 8): value(record) = 0x06 then the fields.
    std::vector<std::uint8_t> expect;
    const auto u64 = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            expect.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFU));
        }
    };
    const auto str = [&](const std::string& s) {
        u64(s.size());
        for (const char c : s) {
            expect.push_back(static_cast<std::uint8_t>(static_cast<unsigned char>(c)));
        }
    };
    str("noisy");
    u64(1);
    str("n");
    str("map");
    expect.push_back(0x06);
    str("cycles");
    expect.push_back(0x02);
    u64(static_cast<std::uint64_t>(dec("1.5").raw()));
    str("octaves");
    expect.push_back(0x01);
    u64(6);
    str("gain");
    expect.push_back(0x02);
    u64(static_cast<std::uint64_t>(dec("0.75").raw()));
    str("metres");
    expect.push_back(0x06);
    str("wavelength_m");
    expect.push_back(0x01);
    u64(4096);
    str("octaves");
    expect.push_back(0x01);
    u64(2);
    str("gain");
    expect.push_back(0x02);
    u64(static_cast<std::uint64_t>(dec("0.25").raw()));
    CHECK(wd::canonical_encoding(*r.value) == expect);
    // A value that is not such a record is fatal.
    CHECK_FATAL(geo::fbm_map_from(wd::Value::integer(3)));
    CHECK_FATAL(geo::fbm_m_from(d->record.get("map")));
    CHECK_FATAL(geo::fbm_map_from(d->record.get("metres")));
}
