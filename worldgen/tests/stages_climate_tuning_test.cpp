// Tuning of the climate stage: definition `worldgen.climate` (spec P1-03
// section 4 and 10.1 tests 5 and 6; P1-00 section 6; P0-03 sections 4, 7.5
// and amendment 16).
//
// Written from the specs only. Every case loads an in-memory data root
// through the production registry (worldgen::data::builtin_kinds), with the
// shipped elevation and hydrology fixtures beside the climate text, so the
// checks are those a user meets when editing game/data/worldgen/
// climate.toml. The effect of each value is tested in
// stages_climate_examples_test.cpp.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "p1_03_climate_api.hpp"
#include "p1_03_climate_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"

using namespace climate_test;

namespace {

std::string all_errors(const climate_api::TuningLoad& l) {
    std::string s;
    for (const auto& e : l.errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

bool loads(const Overrides& o, std::string_view omit = {}) { return load(climate_toml(o, omit)).ok(); }

// The load fails, and an error names the field.
void check_rejected(const Overrides& o, std::string_view mention) {
    const climate_api::TuningLoad l = load(climate_toml(o));
    CHECK_FALSE(l.ok());
    const std::string text = all_errors(l);
    INFO(text);
    if (!mention.empty()) {
        CHECK(text.find(std::string(mention)) != std::string::npos);
    }
}

// Decimal text of the Fx value with this raw (18 digits parse back exactly).
std::string fx_text(std::int64_t raw) { return "\"" + Fx::from_raw(raw).to_decimal(18) + "\""; }

struct Bound {
    std::string_view field;
    bool is_int;
    const char* lo;
    const char* hi;
};

// The scalar bounds of section 4, in declaration order.
const std::vector<Bound>& scalar_bounds() {
    static const std::vector<Bound> b = {
        {"temp_north_c", false, "-30", "40"},
        {"temp_south_c", false, "-30", "40"},
        {"lapse_c_per_km", false, "0", "15"},
        {"temp_noise_amp_c", false, "0", "5"},
        {"smooth_radius_m", true, "0", "4000"},
        {"rain_base_mm", true, "0", "5000"},
        {"rain_oro_mm", true, "0", "100000"},
        {"moisture_path_mm_km", true, "100", "2000000"},
        {"loss_max", false, "0", "0.9"},
        {"ocean_recharge_m", true, "16", "100000"},
        {"coast_rain_mm", true, "0", "5000"},
        {"coast_rain_reach_m", true, "16", "32767"},
        {"rain_noise_amp", false, "0", "1"},
        {"rain_min_mm", true, "0", "5000"},
    };
    return b;
}

std::string read_text(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

TEST_CASE("P1-03 4: the shipped file loads, every field in declaration order") {
    const climate_api::TuningLoad l = load(SHIPPED_TOML);
    INFO(all_errors(l));
    REQUIRE(l.ok());
    CHECK(climate_toml() == std::string(SHIPPED_TOML));
    const auto* def = l.content->find("worldgen", "climate");
    REQUIRE(def != nullptr);
    REQUIRE(def->record.size() == shipped_fields().size());
    for (std::size_t i = 0; i < def->record.size(); ++i) {
        CHECK(def->record.name_at(i) == shipped_fields()[i].name);
    }
    // Values as written (P0-03 4: an integer in an fx field is from_int).
    CHECK(def->record.get_fx("temp_north_c") == Fx::from_int(5));
    CHECK(def->record.get_fx("lapse_c_per_km").raw() == 27917287424);
    CHECK(def->record.get_fx("loss_max").raw() == std::int64_t{1} << 30);
    CHECK(def->record.get_fx("rain_noise_amp").raw() == 1717986918);
    CHECK(def->record.get_int("smooth_radius_m") == 320);
    CHECK(def->record.get_int("moisture_path_mm_km") == 32000);
    CHECK(def->record.get_int("rain_oro_mm") == 2500);
    CHECK(def->record.get_record("temp_noise").get_int("wavelength_m") == 6000);
    CHECK(def->record.get_record("rain_noise").get_int("wavelength_m") == 5000);
    static_cast<void>(climate_api::tuning_from(*l.content));
}

TEST_CASE("P1-03 4, P1-00 6.1: the climate definition is required beside elevation and hydrology") {
    namespace wd = worldgen::data;
    const wd::KindRegistry registry = wd::builtin_kinds();
    wd::MemoryDirectory dir{"worldgen", {}};
    dir.entries.push_back(wd::MemoryEntry{"elevation.toml", elev_test::elevation_toml(), false});
    dir.entries.push_back(
        wd::MemoryEntry{"hydrology.toml", hydro_test::hydrology_toml(hydro_test::SHIPPED_AREA), false});
    wd::MemoryDataRoot root;
    root.directories.push_back(dir);
    const wd::LoadResult<wd::ContentSet> r = wd::load_content_from_memory(registry, root);
    CHECK_FALSE(r.ok());
    std::string text;
    for (const auto& e : r.errors) {
        text += e.to_string() + "\n";
    }
    INFO(text);
    CHECK(text.find("missing definition 'worldgen.climate'") != std::string::npos);
}

TEST_CASE("P1-03 4, P1-00 6.4: adding or changing climate tuning changes the content hash") {
    const climate_api::TuningLoad a = load(SHIPPED_TOML);
    const climate_api::TuningLoad b = load(climate_toml({{"rain_min_mm", "251"}}));
    const climate_api::TuningLoad c = load(climate_toml(wind_overrides(0)));
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    REQUIRE(c.ok());
    CHECK(worldgen::data::content_hash_hex(*a.content) != worldgen::data::content_hash_hex(*b.content));
    CHECK(worldgen::data::content_hash_hex(*a.content) != worldgen::data::content_hash_hex(*c.content));
    // The same values written differently (fx as an integer or a string) are
    // the same data.
    const climate_api::TuningLoad d = load(climate_toml({{"temp_north_c", "\"5\""}, {"loss_max", "\"0.250\""}}));
    REQUIRE(d.ok());
    CHECK(worldgen::data::content_hash_hex(*a.content) == worldgen::data::content_hash_hex(*d.content));
}

TEST_CASE("P1-03 4: the shipped file and both fixture files hold the values of section 4") {
    const std::filesystem::path root(UNMAPPED_TEST_SOURCE_ROOT);
    for (const char* rel : {"game/data/worldgen/climate.toml", "worldgen/tests/data/geo/worldgen/climate.toml",
                            "worldgen/tests/data/geo-small/worldgen/climate.toml"}) {
        CAPTURE(rel);
        const std::string text = read_text(root / rel);
        // Every line of the TOML block of section 4 (comments may differ).
        std::istringstream lines{std::string(SHIPPED_TOML)};
        std::string line;
        while (std::getline(lines, line)) {
            CAPTURE(line);
            CHECK(text.find(line + "\n") != std::string::npos);
        }
        const climate_api::TuningLoad l = load(text);
        CHECK_MESSAGE(l.ok(), all_errors(l));
        if (l.ok()) {
            const climate_api::TuningLoad s = load(SHIPPED_TOML);
            REQUIRE(s.ok());
            CHECK(worldgen::data::content_hash_hex(*l.content) == worldgen::data::content_hash_hex(*s.content));
        }
    }
}

TEST_CASE("P1-03 10.1 test 5: wind_weights is an array of exactly 8 ints in [0, 1000]") {
    CHECK(loads({{"wind_weights", "[0, 0, 0, 0, 5, 0, 0, 0]"}}));
    CHECK(loads({{"wind_weights", "[1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000]"}}));
    CHECK(loads({{"wind_weights", "[0, 0, 0, 0, 0, 0, 0, 1]"}}));
    // 7 or 9 entries: a type error (length out of range).
    check_rejected({{"wind_weights", "[1, 1, 1, 1, 1, 1, 1]"}}, "wind_weights");
    check_rejected({{"wind_weights", "[1, 1, 1, 1, 1, 1, 1, 1, 1]"}}, "wind_weights");
    check_rejected({{"wind_weights", "[]"}}, "wind_weights");
    // Element bounds and types (P0-03 10: elements are named key[i]).
    check_rejected({{"wind_weights", "[1, 1, 1, 1001, 1, 1, 1, 1]"}}, "wind_weights[3]");
    check_rejected({{"wind_weights", "[1, 1, 1, 1, 1, 1, 1, -1]"}}, "wind_weights[7]");
    check_rejected({{"wind_weights", "[1, 1, \"1\", 1, 1, 1, 1, 1]"}}, "wind_weights[2]");
    check_rejected({{"wind_weights", "[1, 1, 1, 1, 1, 1, 1, 1.0]"}}, "floating-point");
    check_rejected({{"wind_weights", "1"}}, "wind_weights");
    check_rejected({{"wind_weights", "\"[1, 1, 1, 1, 1, 1, 1, 1]\""}}, "wind_weights");
}

TEST_CASE("P1-03 4, 10.1 test 5: named check wind_weights_positive, at the definition's header, with the stated text") {
    const climate_api::TuningLoad l = load(climate_toml({{"wind_weights", "[0, 0, 0, 0, 0, 0, 0, 0]"}}));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    CHECK(l.errors[0].path == "worldgen/climate.toml");
    CHECK(l.errors[0].line == 1U);
    CHECK(l.errors[0].column == 1U);
    CHECK(l.errors[0].to_string() == "worldgen/climate.toml:1:1: error: check 'wind_weights_positive' failed for "
                                     "'worldgen.climate': wind_weights must not all be 0");
    // A sum of 1 is enough.
    CHECK(loads({{"wind_weights", "[0, 0, 0, 0, 0, 0, 1, 0]"}}));
    // The check runs only when the definition passed type validation: a bad
    // element gives the type error alone.
    const climate_api::TuningLoad t = load(climate_toml({{"wind_weights", "[0, 0, 0, 0, 0, 0, 0, -1]"}}));
    REQUIRE_FALSE(t.ok());
    CHECK(all_errors(t).find("wind_weights_positive") == std::string::npos);
}

TEST_CASE("P1-03 10.1 test 6: each scalar bound of section 4 rejects a value one step outside it") {
    for (const Bound& b : scalar_bounds()) {
        CAPTURE(std::string(b.field));
        const std::string f(b.field);
        if (b.is_int) {
            const std::int64_t lo = std::stoll(b.lo);
            const std::int64_t hi = std::stoll(b.hi);
            CHECK(loads({{f, std::to_string(lo)}}));
            CHECK(loads({{f, std::to_string(hi)}}));
            check_rejected({{f, std::to_string(lo - 1)}}, f);
            check_rejected({{f, std::to_string(hi + 1)}}, f);
        } else {
            const std::int64_t lo = elev_test::dec(b.lo);
            const std::int64_t hi = elev_test::dec(b.hi);
            CHECK(loads({{f, fx_text(lo)}}));
            CHECK(loads({{f, fx_text(hi)}}));
            check_rejected({{f, fx_text(lo - 1)}}, f);
            check_rejected({{f, fx_text(hi + 1)}}, f);
        }
    }
}

TEST_CASE("P1-03 4, P1-01 4.1: the two noise records are fbm_m with its bounds") {
    for (const char* field : {"temp_noise", "rain_noise"}) {
        CAPTURE(std::string(field));
        const std::string f(field);
        CHECK(loads({{f, "{ wavelength_m = 64, octaves = 1, gain = \"0.0625\" }"}}));
        CHECK(loads({{f, "{ wavelength_m = 65536, octaves = 10, gain = 1 }"}}));
        check_rejected({{f, "{ wavelength_m = 63, octaves = 3, gain = \"0.5\" }"}}, "wavelength_m");
        check_rejected({{f, "{ wavelength_m = 65537, octaves = 3, gain = \"0.5\" }"}}, "wavelength_m");
        check_rejected({{f, "{ wavelength_m = 6000, octaves = 0, gain = \"0.5\" }"}}, "octaves");
        check_rejected({{f, "{ wavelength_m = 6000, octaves = 11, gain = \"0.5\" }"}}, "octaves");
        check_rejected({{f, "{ wavelength_m = 6000, octaves = 3, gain = " + fx_text(elev_test::dec("0.0625") - 1) + " }"}},
                       "gain");
        check_rejected({{f, "{ wavelength_m = 6000, octaves = 3, gain = " + fx_text(elev_test::dec("1") + 1) + " }"}},
                       "gain");
        check_rejected({{f, "{ wavelength_m = 6000, octaves = 3 }"}}, "gain");
        check_rejected({{f, "{ cycles = \"3\", octaves = 3, gain = \"0.5\" }"}}, "cycles");
        check_rejected({{f, "{ wavelength_m = 6000, octaves = 3, gain = 0.5 }"}}, "floating-point");
        check_rejected({{f, "6000"}}, f);
    }
}

TEST_CASE("P1-03 10.1 test 6: a float literal in any field is a P0-03 error") {
    for (const FieldText& f : shipped_fields()) {
        CAPTURE(std::string(f.name));
        if (f.value.front() == '{' || f.value.front() == '[') {
            continue;  // records and the array: above
        }
        check_rejected({{std::string(f.name), "0.5"}}, is_int_field(f.name) ? "" : "floating-point");
        check_rejected({{std::string(f.name), "1.0"}}, "");
    }
}

TEST_CASE("P1-03 4, P1-00 6.1: every field is required; an unknown field is an error") {
    for (const FieldText& f : shipped_fields()) {
        CAPTURE(std::string(f.name));
        const climate_api::TuningLoad l = load(climate_toml({}, f.name));
        CHECK_FALSE(l.ok());
        CHECK(all_errors(l).find(std::string(f.name)) != std::string::npos);
    }
    CHECK(loads({}));
    const climate_api::TuningLoad u = load(std::string(SHIPPED_TOML) + "wind_speed = 3\n");
    CHECK_FALSE(u.ok());
    CHECK(all_errors(u).find("wind_speed") != std::string::npos);
}

TEST_CASE("P1-03 4: an fx field accepts an integer or a decimal string; an int field rejects a decimal string") {
    CHECK(loads({{"lapse_c_per_km", "6"}}));
    CHECK(loads({{"temp_north_c", "\"-2.5\""}}));
    CHECK(loads({{"temp_north_c", "-30"}}));
    CHECK(loads({{"loss_max", "0"}}));
    check_rejected({{"smooth_radius_m", "\"320\""}}, "smooth_radius_m");
    check_rejected({{"rain_min_mm", "\"250\""}}, "rain_min_mm");
    check_rejected({{"coast_rain_reach_m", "\"1500.5\""}}, "coast_rain_reach_m");
}
