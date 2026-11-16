// Tuning of the elevation stage: definition `worldgen.elevation` (spec
// P1-01 section 4; P1-00 section 6; P0-03 sections 4, 7.5 and amendment 16).
//
// Written from the specs only. Every case loads an in-memory data root
// through the production registry (worldgen::data::builtin_kinds), so the
// checks are those a user meets when editing game/data/worldgen/
// elevation.toml.

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "p1_01_elevation_api.hpp"
#include "p1_01_elevation_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"

using namespace elev_test;
using worldgen::Fx;

namespace {

std::string all_errors(const elev_api::TuningLoad& l) {
    std::string s;
    for (const auto& e : l.errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

// Loads elevation.toml with the overrides; true if it loads.
bool loads(const Overrides& o, std::string_view omit = {}) {
    return elev_api::load_tuning_text(elevation_toml(o, omit)).ok();
}

// The load fails, and an error names the field (or, for a named check,
// is reported at all).
void check_rejected(const Overrides& o, std::string_view mention) {
    const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml(o));
    CHECK_FALSE(l.ok());
    const std::string text = all_errors(l);
    INFO(text);
    if (!mention.empty()) {
        CHECK(text.find(std::string(mention)) != std::string::npos);
    }
}

// Decimal text of the Fx one raw step from `raw` (to_decimal with 18
// digits parses back to the same raw value).
std::string fx_text(std::int64_t raw) { return "\"" + Fx::from_raw(raw).to_decimal(18) + "\""; }

struct Bound {
    std::string_view field;
    bool is_int;
    const char* lo;
    const char* hi;
};

// The scalar bounds of 4.2, in declaration order.
const std::vector<Bound>& scalar_bounds() {
    static const std::vector<Bound> b = {
        {"land_fraction", false, "0.1", "0.8"},
        {"edge_margin_frac", false, "0", "0.25"},
        {"continent_warp_amp_frac", false, "0", "0.5"},
        {"continent_noise_amp", false, "0", "4"},
        {"continent_center_jitter", false, "0", "0.25"},
        {"edge_falloff_frac", false, "0.01", "0.5"},
        {"edge_falloff_depth", false, "0", "8"},
        {"edge_noise_amp", false, "0", "0.25"},
        {"edge_radial_start", false, "0", "2"},
        {"edge_radial_end", false, "0.01", "2"},
        {"coast_noise_amp", false, "0", "2"},
        {"island_min_area_m2", true, "0", "1000000000"},
        {"belt_halfwidth_m", false, "100", "20000"},
        {"belt_halfwidth_frac", false, "0.01", "1"},
        {"segment_contrast", false, "0", "16"},
        {"segment_bias", false, "-1", "1"},
        {"segment_on_share", false, "0", "1"},
        {"segment_floor_mask", false, "0.01", "1"},
        {"belt_offset_steps", true, "0", "8"},
        {"floor_inland_gm", false, "0", "1"},
        {"mountain_coast_t", false, "0.01", "16"},
        {"relief_warp_amp_m", false, "0", "5000"},
        {"ridge_weight", false, "0", "4"},
        {"foothill_fraction", false, "0", "1"},
        {"mountain_height_m", false, "0", "2900"},
        {"mountain_peak_rank", false, "0.5", "1"},
        {"mountain_cover_height_m", false, "0", "2900"},
        {"mountain_cover_rank", false, "0.5", "1"},
        {"lowland_height_m", false, "0", "2000"},
        {"basin_amp_m", false, "0", "1000"},
        {"roughness_min", false, "0", "1"},
        {"hill_height_m", false, "0", "1000"},
        {"hill_coast_min", false, "0", "1"},
        {"detail_amp_m", false, "0", "100"},
        {"ocean_depth_m", false, "0", "2900"},
        {"ocean_shelf_m", false, "0", "50000"},
        {"erosion_iterations", true, "0", "200"},
        {"erosion_route_interval", true, "1", "64"},
        {"erosion_k", false, "0", "1"},
        {"erosion_diffusion", false, "0", "0.25"},
        {"talus_slope", false, "0.05", "4"},
        {"talus_rate", false, "0", "0.0625"},
        {"land_floor_m", false, "0.05", "10"},
        {"lake_min_area_m2", true, "0", "1000000000"},
        {"lake_max_area_m2", true, "0", "1000000000"},
        {"lake_area_ref_n", true, "16", "2048"},
        {"lake_min_depth_m", false, "0", "100"},
        {"lake_max_share", false, "0.001", "1"},
        {"fill_slope_dm", false, "0", "10"},
        {"fill_cost_amp", false, "0", "16"},
        {"fill_detail_m", false, "0", "10"},
    };
    return b;
}

// Overrides that keep the three named checks satisfied while one field
// sits at a bound (the checks relate some pairs of fields).
Overrides companions(std::string_view field, bool at_lo) {
    if (field == "edge_radial_start") {
        return at_lo ? Overrides{} : Overrides{{"edge_radial_end", "2"}, {"edge_radial_start", "\"1.99\""}};
    }
    if (field == "edge_radial_end" && at_lo) {
        return {{"edge_radial_start", "0"}};
    }
    if (field == "mountain_cover_rank" && !at_lo) {
        return {{"mountain_peak_rank", "1"}};
    }
    if (field == "mountain_peak_rank" && at_lo) {
        return {{"mountain_cover_rank", "\"0.5\""}};
    }
    if (field == "lake_min_area_m2" && !at_lo) {
        return {{"lake_max_area_m2", "1000000000"}};
    }
    if (field == "lake_max_area_m2" && at_lo) {
        return {{"lake_min_area_m2", "0"}};
    }
    return {};
}

}  // namespace

TEST_CASE("P1-01 4.4: the shipped and small-map files load") {
    const elev_api::TuningLoad shipped = elev_api::load_tuning_text(elevation_toml());
    INFO(all_errors(shipped));
    REQUIRE(shipped.ok());
    const elev_api::TuningLoad small = elev_api::load_tuning_text(elevation_toml(small_overrides()));
    INFO(all_errors(small));
    REQUIRE(small.ok());
    // The definition is worldgen.elevation, with every field of 4.2 in
    // declaration order (this order fixes the canonical encoding).
    const auto* def = shipped.content->find("worldgen", "elevation");
    REQUIRE(def != nullptr);
    REQUIRE(def->record.size() == shipped_fields().size());
    for (std::size_t i = 0; i < def->record.size(); ++i) {
        CHECK(def->record.name_at(i) == shipped_fields()[i].name);
    }
    // Values as written (spot checks; P0-03 4: an integer in an fx field is from_int).
    CHECK(def->record.get_fx("land_fraction").raw() == dec("0.40"));
    CHECK(def->record.get_fx("edge_falloff_depth") == Fx::from_int(2));
    CHECK(def->record.get_int("belt_offset_steps") == 4);
    CHECK(def->record.get_record("coast_noise").get_int("wavelength_m") == 2000);
    // The two files differ in values, so in content hash (4.4 last paragraph).
    CHECK(worldgen::data::content_hash_hex(*shipped.content) != worldgen::data::content_hash_hex(*small.content));
}

TEST_CASE("P1-01 4.3: named check 1, edge_radial_end - edge_radial_start >= 0.01") {
    check_rejected({{"edge_radial_start", "\"1.0\""}, {"edge_radial_end", "\"1.005\""}}, "elevation");
    check_rejected({{"edge_radial_start", "\"1.0\""}, {"edge_radial_end", "\"0.9\""}}, "elevation");
    check_rejected({{"edge_radial_start", "\"1.0\""}, {"edge_radial_end", "\"1.0\""}}, "elevation");
    CHECK(loads({{"edge_radial_start", "\"0.99\""}, {"edge_radial_end", "\"1.0\""}}));  // exactly 0.01
}

TEST_CASE("P1-01 4.3: named check 2, mountain_cover_rank <= mountain_peak_rank") {
    check_rejected({{"mountain_cover_rank", "\"0.999\""}}, "elevation");
    CHECK(loads({{"mountain_cover_rank", "\"0.998\""}}));  // equal is allowed
}

TEST_CASE("P1-01 4.3: named check 3, lake_min_area_m2 <= lake_max_area_m2") {
    check_rejected({{"lake_min_area_m2", "200000"}, {"lake_max_area_m2", "100000"}}, "elevation");
    CHECK(loads({{"lake_min_area_m2", "100000"}, {"lake_max_area_m2", "100000"}}));
}

TEST_CASE("P1-01 4.3: a named-check failure is a load error at the definition's header") {
    const elev_api::TuningLoad l =
        elev_api::load_tuning_text(elevation_toml({{"mountain_cover_rank", "\"0.999\""}}));
    REQUIRE_FALSE(l.ok());
    REQUIRE_FALSE(l.errors.empty());
    CHECK(l.errors[0].path == "worldgen/elevation.toml");
    CHECK(l.errors[0].line == 1U);  // [worldgen.elevation] is line 1
}

TEST_CASE("P1-01 10.1: a float literal in any field is an error") {
    for (const FieldText& f : shipped_fields()) {
        CAPTURE(std::string(f.name));
        if (f.value.front() == '{') {
            continue;
        }
        check_rejected({{std::string(f.name), "0.5"}}, is_int_field(f.name) ? "" : "floating-point");
    }
    check_rejected({{"continent_warp", "{ cycles = 1.5, octaves = 3, gain = \"0.5\" }"}}, "floating-point");
    check_rejected({{"coast_noise", "{ wavelength_m = 2000, octaves = 5, gain = 0.5 }"}}, "floating-point");
}

TEST_CASE("P1-01 10.1: every field is required") {
    for (const FieldText& f : shipped_fields()) {
        CAPTURE(std::string(f.name));
        const elev_api::TuningLoad l = elev_api::load_tuning_text(elevation_toml({}, f.name));
        CHECK_FALSE(l.ok());
        CHECK(all_errors(l).find(std::string(f.name)) != std::string::npos);
    }
    // A record field missing one of its fields.
    check_rejected({{"ridge_noise", "{ wavelength_m = 2000, octaves = 6 }"}}, "gain");
}

TEST_CASE("P1-01 10.1: each scalar bound of 4.2 rejects a value one step outside it") {
    for (const Bound& b : scalar_bounds()) {
        CAPTURE(std::string(b.field));
        const std::string f(b.field);
        if (b.is_int) {
            const std::int64_t lo = std::stoll(b.lo);
            const std::int64_t hi = std::stoll(b.hi);
            CHECK(loads(concat(companions(b.field, true), {{f, std::to_string(lo)}})));
            CHECK(loads(concat(companions(b.field, false), {{f, std::to_string(hi)}})));
            check_rejected(concat(companions(b.field, true), {{f, std::to_string(lo - 1)}}), f);
            check_rejected(concat(companions(b.field, false), {{f, std::to_string(hi + 1)}}), f);
        } else {
            const std::int64_t lo = dec(b.lo);
            const std::int64_t hi = dec(b.hi);
            CHECK(loads(concat(companions(b.field, true), {{f, fx_text(lo)}})));
            if (b.field != "edge_radial_start") {  // start = 2 cannot meet check 1 (end <= 2)
                CHECK(loads(concat(companions(b.field, false), {{f, fx_text(hi)}})));
            }
            check_rejected(concat(companions(b.field, true), {{f, fx_text(lo - 1)}}), f);
            check_rejected(concat(companions(b.field, false), {{f, fx_text(hi + 1)}}), f);
        }
    }
    // The examples named in 10.1.
    check_rejected({{"lake_area_ref_n", "15"}}, "lake_area_ref_n");
    check_rejected({{"lake_max_share", "\"0.0009\""}}, "lake_max_share");
}

TEST_CASE("P1-01 4.1: noise record bounds") {
    // fbm_map: cycles fx(0.125, 64), octaves int(1, 10), gain fx(0.0625, 1).
    check_rejected({{"belt_noise", "{ cycles = " + fx_text(dec("0.125") - 1) + ", octaves = 2, gain = \"0.5\" }"}},
                   "cycles");
    check_rejected({{"belt_noise", "{ cycles = " + fx_text(dec("64") + 1) + ", octaves = 2, gain = \"0.5\" }"}},
                   "cycles");
    check_rejected({{"belt_noise", "{ cycles = \"1.25\", octaves = 0, gain = \"0.5\" }"}}, "octaves");
    check_rejected({{"belt_noise", "{ cycles = \"1.25\", octaves = 11, gain = \"0.5\" }"}}, "octaves");
    check_rejected({{"belt_noise", "{ cycles = \"1.25\", octaves = 2, gain = " + fx_text(dec("0.0625") - 1) + " }"}},
                   "gain");
    check_rejected({{"belt_noise", "{ cycles = \"1.25\", octaves = 2, gain = " + fx_text(dec("1") + 1) + " }"}},
                   "gain");
    CHECK(loads({{"belt_noise", "{ cycles = \"0.125\", octaves = 1, gain = \"0.0625\" }"}}));
    CHECK(loads({{"belt_noise", "{ cycles = 64, octaves = 10, gain = 1 }"}}));
    // fbm_m: wavelength_m int(64, 65536).
    check_rejected({{"detail_noise", "{ wavelength_m = 63, octaves = 3, gain = \"0.5\" }"}}, "wavelength_m");
    check_rejected({{"detail_noise", "{ wavelength_m = 65537, octaves = 3, gain = \"0.5\" }"}}, "wavelength_m");
    CHECK(loads({{"detail_noise", "{ wavelength_m = 64, octaves = 10, gain = 1 }"}}));
    CHECK(loads({{"detail_noise", "{ wavelength_m = 65536, octaves = 1, gain = \"0.0625\" }"}}));
}

TEST_CASE("P1-01 4.2: an fx field accepts an integer; an int field rejects a decimal string") {
    CHECK(loads({{"edge_falloff_depth", "3"}}));
    CHECK(loads({{"land_fraction", "\"0.4\""}}));
    check_rejected({{"belt_offset_steps", "\"4\""}}, "belt_offset_steps");
    check_rejected({{"erosion_iterations", "\"24\""}}, "erosion_iterations");
}
