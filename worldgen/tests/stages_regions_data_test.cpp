// The tuning `worldgen.regions` and the string grammar `word` (spec P1-06
// 4 and 10.1 test 4; P0-03 7.5 and amendment 17; P1-00 6.1, 6.4).
//
// Written from the specs only. Every case loads an in-memory data root
// through the production registry (region_api::load), so the stage's
// registration, schema, named check and grammar are what is tested.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p1_04_biomes_data.hpp"
#include "p1_06_regions_api.hpp"
#include "p1_06_regions_data.hpp"
#include "p1_06_regions_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

using namespace region_test;
namespace wd = worldgen::data;

namespace {

region_api::Load load_tuning(const Overrides& toml_overrides, std::string_view omit = {}) {
    return region_api::load(region_data::tuning_toml(toml_overrides, omit), biome_data::directory());
}

wd::LoadError only_error(const region_api::Load& l) {
    INFO(all_errors(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

void check_rejected(const region_api::Load& l, std::string_view mention) {
    CHECK_FALSE(l.ok());
    const std::string text = all_errors(l);
    INFO(text);
    if (!mention.empty()) {
        CHECK(text.find(std::string(mention)) != std::string::npos);
    }
}

std::string fx_text(std::int64_t raw) { return "\"" + Fx::from_raw(raw).to_decimal(18) + "\""; }

// The error a named check gives at the definition's header (P0-03 7.5).
std::string check_error(std::string_view check, std::string_view message) {
    return "worldgen/regions.toml:1:1: error: check '" + std::string(check) + "' failed for 'worldgen.regions': " +
           std::string(message);
}

constexpr std::string_view WORD_EXPECTED = "expected a string of at most 32 bytes, a word ([A-Z][a-z]*)";

}  // namespace

// --- The shipped tuning (4) ----------------------------------------------------------------------

TEST_CASE("P1-06 4: the shipped tuning loads; fields in declaration order with the values of 4") {
    const region_api::Load l = load_tuning({});
    INFO(all_errors(l));
    REQUIRE(l.ok());
    CHECK(region_data::tuning_toml() == std::string(region_data::TUNING_TOML));
    const auto* def = l.content->find("worldgen", "regions");
    REQUIRE(def != nullptr);
    REQUIRE(def->record.size() == region_data::tuning_fields().size());
    // r3 adds name_pool_weights and name_traits (generator 0.17.0).
    CHECK(region_data::tuning_fields().size() == (UNMAPPED_P3_08_API ? 16U : 14U));
    for (std::size_t i = 0; i < def->record.size(); ++i) {
        CHECK(def->record.name_at(i) == region_data::tuning_fields()[i].name);
    }
    const Values v = values();
    CHECK(def->record.get_int("seed_tile_m") == 864);
    CHECK(def->record.get_int("seed_spacing_m") == 1200);
    CHECK(def->record.get_int("hand_ref_m") == 25);
    CHECK(def->record.get_record("cost_noise").get_int("wavelength_m") == 1500);
    CHECK(def->record.get_record("cost_noise").get_int("octaves") == 3);
    CHECK(def->record.get_record("cost_noise").get_fx("gain").raw() == ONE / 2);
    CHECK(def->record.get_fx("cost_noise_amp").raw() == v.cost_noise_amp);
    CHECK(def->record.get_fx("lake_cost_factor") == Fx::from_int(2));
    CHECK(def->record.get_int("river_border_class") == 6);
    CHECK(def->record.get_int("river_cost_m") == 2000);
    CHECK(def->record.get_int("region_min_area_m2") == 750000);
    CHECK(def->record.get_int("ore_rich_min") == 128);
    CHECK(def->record.get_int("ore_reach_m") == 300);
    CHECK(def->record.get_fx("name_central_frac").raw() == 515396076);
    const auto& names = def->record.get_record("names");
    REQUIRE(names.size() == 11U);
    for (std::size_t i = 0; i < 11; ++i) {
        CHECK(names.name_at(i) == region_data::name_fields()[i]);
        CHECK(names.get_string(std::string(region_data::name_fields()[i])) == std::string(SHIPPED_WORDS[i]));
    }
    static_cast<void>(region_api::tuning_from(*l.content));
    // min_cells = (750000 + 255) div 256 = 2930 (RG8); tiles of 54 cells (RG4).
    CHECK(v.min_cells() == 2930);
    CHECK(v.seed_tile_m / 16 == 54);
}

TEST_CASE("P1-06 4: the test-only tuning of 10.2 loads") {
    const region_api::Load l = region_api::load(std::string(region_data::TEST128_TOML), biome_data::directory());
    INFO(all_errors(l));
    CHECK(l.ok());
}

TEST_CASE("P1-00 6.1: worldgen.regions is required") {
    const region_api::Load l = region_api::load(std::nullopt, biome_data::directory());
    check_rejected(l, "missing definition 'worldgen.regions'");
}

TEST_CASE("P1-06 4, P1-00 6.4: the tuning, words included, is part of the content hash") {
    const region_api::Load a = load_tuning({});
    const region_api::Load b = load_tuning({{"ore_reach_m", "301"}});
    const region_api::Load c = load_tuning({{"names", region_data::names_toml("isle", "Holm")}});
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    REQUIRE(c.ok());
    CHECK(wd::content_hash_hex(*a.content) != wd::content_hash_hex(*b.content));
    CHECK(wd::content_hash_hex(*a.content) != wd::content_hash_hex(*c.content));
}

// --- Named check (4, 10.1 test 4) ----------------------------------------------------------------

TEST_CASE("P1-06 4, 10.1 test 4: seed_tile_m above twice seed_spacing_m fails regions_tile_fits_spacing at the header") {
    CHECK(load_tuning({{"seed_tile_m", "2400"}}).ok());
    CHECK(only_error(load_tuning({{"seed_tile_m", "2401"}})).to_string() ==
          check_error("regions_tile_fits_spacing", "seed_tile_m must be at most twice seed_spacing_m"));
    CHECK(only_error(load_tuning({{"seed_tile_m", "321"}, {"seed_spacing_m", "160"}})).to_string() ==
          check_error("regions_tile_fits_spacing", "seed_tile_m must be at most twice seed_spacing_m"));
    CHECK(load_tuning({{"seed_tile_m", "320"}, {"seed_spacing_m", "160"}}).ok());
}

// --- Bounds (10.1 test 4) ------------------------------------------------------------------------

TEST_CASE("P1-06 4, 10.1 test 4: each field one step outside its bounds is a range error; each is required") {
    for (const auto& f : region_data::tuning_fields()) {
        const std::string name(f.name);
        CAPTURE(name);
        // The other field of the named check kept compatible at each bound.
        Overrides lo_extra, hi_extra;
        if (name == "seed_tile_m") {
            hi_extra = {{"seed_spacing_m", "32768"}};
        } else if (name == "seed_spacing_m") {
            lo_extra = {{"seed_tile_m", "256"}};
        }
        auto with = [](Overrides o, const std::string& k, const std::string& v) {
            o.emplace_back(k, v);
            return o;
        };
        if (f.kind == "int") {
            const std::int64_t lo = std::stoll(std::string(f.lo)), hi = std::stoll(std::string(f.hi));
            const region_api::Load a = load_tuning(with(lo_extra, name, std::to_string(lo)));
            CHECK_MESSAGE(a.ok(), all_errors(a));
            const region_api::Load b = load_tuning(with(hi_extra, name, std::to_string(hi)));
            CHECK_MESSAGE(b.ok(), all_errors(b));
            check_rejected(load_tuning(with(lo_extra, name, std::to_string(lo - 1))), name);
            check_rejected(load_tuning(with(hi_extra, name, std::to_string(hi + 1))), name);
            check_rejected(load_tuning({{name, "\"" + std::to_string(lo) + "\""}}), name);
        } else if (f.kind == "fx") {
            const std::int64_t lo = fx_of(f.lo), hi = fx_of(f.hi);
            const region_api::Load a = load_tuning({{name, fx_text(lo)}});
            CHECK_MESSAGE(a.ok(), all_errors(a));
            const region_api::Load b = load_tuning({{name, fx_text(hi)}});
            CHECK_MESSAGE(b.ok(), all_errors(b));
            check_rejected(load_tuning({{name, fx_text(lo - 1)}}), name);
            check_rejected(load_tuning({{name, fx_text(hi + 1)}}), name);
            check_rejected(load_tuning({{name, "0.5"}}), "floating-point");
        }
        check_rejected(load_tuning({}, name), name);  // every field is required
    }
    // cost_noise is an fbm_m record (P1-01 4.1).
    const std::string o = "cost_noise";
    CHECK(load_tuning({{o, "{ wavelength_m = 64, octaves = 1, gain = \"0.0625\" }"}}).ok());
    CHECK(load_tuning({{o, "{ wavelength_m = 65536, octaves = 10, gain = 1 }"}}).ok());
    check_rejected(load_tuning({{o, "{ wavelength_m = 63, octaves = 3, gain = \"0.5\" }"}}), "wavelength_m");
    check_rejected(load_tuning({{o, "{ wavelength_m = 1500, octaves = 11, gain = \"0.5\" }"}}), "octaves");
    check_rejected(load_tuning({{o, "{ wavelength_m = 1500, octaves = 3 }"}}), "gain");
    // An unknown field.
    check_rejected(region_api::load(std::string(region_data::TUNING_TOML) + "sea_regions = 1\n", biome_data::directory()),
                   "sea_regions");
}

// --- Record region_names and the word grammar (4; P0-03 amendment 17) ---------------------------------

TEST_CASE("P1-06 4, 10.1 test 4: a names word `east` or `North West` fails the word grammar at the value") {
    for (const char* bad : {"east", "North West", "North-West", "NORTH", "North2", "N\xC3\xB6rth"}) {
        CAPTURE(bad);
        const region_api::Load l = load_tuning({{"names", region_data::names_toml("east", bad)}});
        const std::string text = all_errors(l);
        INFO(text);
        REQUIRE_FALSE(l.ok());
        REQUIRE(l.errors.size() == 1U);
        CHECK(text.find("is \"" + std::string(bad) + "\"; " + std::string(WORD_EXPECTED)) != std::string::npos);
        CHECK(text.find("east") != std::string::npos);
        CHECK(text.find("worldgen/regions.toml:") == 0U);
    }
    // The empty string fails the grammar too; a 33-byte word fails the length rule first.
    check_rejected(load_tuning({{"names", region_data::names_toml("north", "")}}), "north");
    const std::string long_word = "A" + std::string(32, 'b');
    const region_api::Load l33 = load_tuning({{"names", region_data::names_toml("north", long_word)}});
    check_rejected(l33, "at most 32 bytes");
    // A 32-byte word and a single capital load.
    CHECK(load_tuning({{"names", region_data::names_toml("north", "A" + std::string(31, 'b'))}}).ok());
    CHECK(load_tuning({{"names", region_data::names_toml("isle", "I")}}).ok());
}

TEST_CASE("P1-06 4, 10.1 test 4: a missing names.isle is a missing-field error; an unknown name field is refused") {
    check_rejected(load_tuning({{"names", region_data::names_toml("isle", "", true)}}), "isle");
    std::string extra = region_data::names_toml();
    extra.insert(extra.size() - 2, ", middle = \"Middle\"");
    check_rejected(load_tuning({{"names", extra}}), "middle");
    check_rejected(load_tuning({{"names", "\"Central\""}}), "names");
}

TEST_CASE("P0-03 amendment 17: word_grammar() matches [A-Z][a-z]* of 1 to 32 bytes; its description") {
    const wd::StringGrammar g = wd::word_grammar();
    REQUIRE(g.matches != nullptr);
    REQUIRE(g.description != nullptr);
    CHECK(std::string_view(g.description) == "a word ([A-Z][a-z]*)");
    for (const std::string& s : {std::string("North"), std::string("Isle"), std::string("A"), "A" + std::string(31, 'b')}) {
        CAPTURE(s);
        CHECK(region_api::word_matches(s));
    }
    for (const std::string& s : {std::string("north"), std::string("North West"), std::string("North-West"),
                                 std::string("N\xC3\xB6rth"), std::string("NORTH"), std::string("North2"), std::string()}) {
        CAPTURE(s);
        CHECK_FALSE(region_api::word_matches(s));
    }
    // The id grammar is unchanged.
    CHECK(wd::id_grammar().matches("north"));
    CHECK_FALSE(wd::id_grammar().matches("North"));
}

TEST_CASE("P1-06 2 check 4: every shipped region_noun is one word; the biome kind still takes any string(32)") {
    for (const auto& b : definitions().biomes) {
        CAPTURE(b.key);
        CHECK(region_api::word_matches(b.region_noun));
    }
    // A noun of two words loads (the biome kind is unchanged) and is refused
    // by the stage (stages_regions_golden_test.cpp).
    std::string forest;
    for (const auto& f : biome_data::shipped_files()) {
        if (f.name == "temperate_forest.toml") {
            forest = std::string(f.text);
        }
    }
    const std::size_t at = forest.find("region_noun = \"Wood\"");
    REQUIRE(at != std::string::npos);
    forest.replace(at, std::string_view("region_noun = \"Wood\"").size(), "region_noun = \"Wet Wood\"");
    const region_api::Load l =
        region_api::load(std::string(region_data::TUNING_TOML), biome_data::directory({{"temperate_forest.toml", forest}}));
    INFO(all_errors(l));
    CHECK(l.ok());
}

// --- r2: names.upland and upland_own_noun (4; 10.1 test 4) -----------------------------------------------

TEST_CASE("P1-06 4 (r2), 10.1 test 4: names.upland is required; upland_own_noun is an array of biome refs") {
    const region_api::Load l = load_tuning({});
    REQUIRE(l.ok());
    const auto* def = l.content->find("worldgen", "regions");
    REQUIRE(def != nullptr);
    CHECK(def->record.name_at(13) == "upland_own_noun");
    CHECK(def->record.get_record("names").get_string("upland") == "Fells");
    // A missing names.upland is a missing-field error.
    check_rejected(load_tuning({{"names", region_data::names_toml("upland", "", true)}}), "upland");
    // upland must be a word too.
    check_rejected(load_tuning({{"names", region_data::names_toml("upland", "fells")}}), std::string(WORD_EXPECTED));
    // upland_own_noun: required; an unknown biome is a reference error; empty loads; 16 entries load, 17 do not.
    check_rejected(load_tuning({}, "upland_own_noun"), "upland_own_noun");
    check_rejected(load_tuning({{"upland_own_noun", "[\"alpine\", \"glacier\"]"}}), "glacier");
    CHECK(load_tuning({{"upland_own_noun", "[]"}}).ok());
    std::string sixteen = "[";
    for (int k = 0; k < 16; ++k) {
        sixteen += std::string(k ? ", " : "") + "\"alpine\"";
    }
    const region_api::Load l16 = load_tuning({{"upland_own_noun", sixteen + "]"}});
    CHECK_MESSAGE(l16.ok(), all_errors(l16));
    check_rejected(load_tuning({{"upland_own_noun", sixteen + ", \"heath\"]"}}), "upland_own_noun");
    check_rejected(load_tuning({{"upland_own_noun", "\"alpine\""}}), "upland_own_noun");
    // Part of the content hash.
    const region_api::Load a = load_tuning({{"upland_own_noun", "[\"alpine\"]"}});
    REQUIRE(a.ok());
    CHECK(wd::content_hash_hex(*a.content) != wd::content_hash_hex(*l.content));
}
