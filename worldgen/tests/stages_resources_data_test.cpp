// The tuning `worldgen.resources` and the biome field `forage` (spec P1-05
// 4.1, 4.2 and 10.1 tests 4 and 6; P1-00 6.1, 6.4 and 6.6; P0-03 7.5, 8).
//
// Written from the specs only. Every case loads an in-memory data root
// through the production registry (resource_api::load), so the stage's
// registration, schema and named checks are what is tested.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p1_04_biomes_data.hpp"
#include "p1_05_resources_api.hpp"
#include "p1_05_resources_data.hpp"
#include "p1_05_resources_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"

using namespace resource_test;
namespace wd = worldgen::data;

namespace {

resource_api::Load load_tuning(const Overrides& toml_overrides, std::string_view omit = {}) {
    return resource_api::load(resource_data::tuning_toml(toml_overrides, omit), biome_data::directory());
}

using Replace = std::vector<std::pair<std::string, std::string>>;

resource_api::Load load_defs(const Replace& replace) {
    return resource_api::load(std::string(resource_data::TUNING_TOML), biome_data::directory(replace));
}

// The shipped (0.10.0) text of one definition file with `from` replaced by
// `to` (the first occurrence, which must exist).
std::string edited(std::string_view file, std::string_view from, std::string_view to) {
    for (const auto& f : biome_data::shipped_files()) {
        if (f.name == file) {
            std::string t(f.text);
            const std::size_t at = t.find(from);
            REQUIRE_MESSAGE(at != std::string::npos, std::string(from) << " not in " << std::string(file));
            t.replace(at, from.size(), to);
            return t;
        }
    }
    FAIL("no file " << std::string(file));
    return {};
}

wd::LoadError only_error(const resource_api::Load& l) {
    INFO(all_errors(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

void check_rejected(const resource_api::Load& l, std::string_view mention) {
    CHECK_FALSE(l.ok());
    const std::string text = all_errors(l);
    INFO(text);
    if (!mention.empty()) {
        CHECK(text.find(std::string(mention)) != std::string::npos);
    }
}

std::string fx_text(std::int64_t raw) { return "\"" + Fx::from_raw(raw).to_decimal(18) + "\""; }

std::string read_text(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    REQUIRE_MESSAGE(in.good(), "cannot open " << p.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// The error a named check gives at the definition's header (P0-03 7.5).
std::string check_error(std::string_view check, std::string_view message) {
    return "worldgen/resources.toml:1:1: error: check '" + std::string(check) + "' failed for 'worldgen.resources': " +
           std::string(message);
}

}  // namespace

// --- The shipped tuning (4.2) ---------------------------------------------------------------------

TEST_CASE("P1-05 4.2: the shipped tuning loads; fields in declaration order with the values of 4.2") {
    const resource_api::Load l = load_tuning({});
    INFO(all_errors(l));
    REQUIRE(l.ok());
    CHECK(resource_data::tuning_toml() == std::string(resource_data::TUNING_TOML));
    const auto* def = l.content->find("worldgen", "resources");
    REQUIRE(def != nullptr);
    REQUIRE(def->record.size() == resource_data::tuning_fields().size());
    CHECK(resource_data::tuning_fields().size() == 34U);
    for (std::size_t i = 0; i < def->record.size(); ++i) {
        CHECK(def->record.name_at(i) == resource_data::tuning_fields()[i].name);
    }
    const Values v = values();
    CHECK(def->record.get_int("fresh_reach_m") == v.fresh_reach_m);
    CHECK(def->record.get_fx("fresh_rain_weight").raw() == v.fresh_rain_weight);
    CHECK(def->record.get_fx("alluvial_hand_m") == Fx::from_int(5));
    CHECK(def->record.get_record("ore_noise").get_int("wavelength_m") == 1500);
    CHECK(def->record.get_record("ore_noise").get_int("octaves") == 3);
    CHECK(def->record.get_record("ore_noise").get_fx("gain").raw() == HALF);
    CHECK(def->record.get_fx("ore_vein_lo").raw() == 0);
    CHECK(def->record.get_fx("ore_slope_hi") == Fx::from_int(1));
    CHECK(def->record.get_fx("walk_slope_max").raw() == 3007336101);
    CHECK(def->record.get_int("walk_min_area_m2") == 250000);
    CHECK(def->record.get_int("hab_weight_fertility") + def->record.get_int("hab_weight_forage") +
              def->record.get_int("hab_weight_timber") + def->record.get_int("hab_weight_ore") ==
          100);
    CHECK(def->record.get_fx("hab_flood_hand_m").raw() == HALF);  // r2
    CHECK(def->record.get_int("hab_weight_fertility") == 70);         // r2
    CHECK(def->record.get_fx("ore_slope_base").raw() == fx_of("0.65"));
    CHECK(def->record.get_int("ore_elev_hi_m") == 600);
    CHECK(def->record.get_fx("alluvial_bonus").raw() == fx_of("0.15"));
    CHECK(def->record.get_fx("hab_flood_factor").raw() == fx_of("0.75"));
    CHECK(def->record.get_fx("hab_pocket_factor").raw() == 1073741824);
    static_cast<void>(resource_api::tuning_from(*l.content));
    // walk_min_cells = (walk_min_area_m2 + 255) div 256 = 977 (5 R5).
    CHECK(v.walk_min_cells() == 977);
}

TEST_CASE("P1-00 6.1: worldgen.resources is required") {
    const resource_api::Load l = resource_api::load(std::nullopt, biome_data::directory());
    check_rejected(l, "missing definition 'worldgen.resources'");
}

TEST_CASE("P1-05 4.2, P1-00 6.4: the tuning is part of the content hash") {
    const resource_api::Load a = load_tuning({});
    const resource_api::Load b = load_tuning({{"fish_reach_m", "301"}});
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(wd::content_hash_hex(*a.content) != wd::content_hash_hex(*b.content));
}

// --- Named checks (4.2, 10.1 test 4) ---------------------------------------------------------------

TEST_CASE("P1-05 4.2, 10.1 test 4: each ordered pair, equal and reversed, fails its check at the header") {
    struct Pair {
        const char* check;
        const char* lo;
        const char* hi;
        bool is_int;
        const char* a;  // a value inside both bounds
        const char* b;  // a larger one
    };
    const std::vector<Pair> pairs = {
        {"fresh_rain_ordered", "fresh_rain_lo_mm", "fresh_rain_hi_mm", true, "700", "900"},
        {"fert_slope_ordered", "fert_slope_lo", "fert_slope_hi", false, "0.3", "0.4"},
        {"timber_slope_ordered", "timber_slope_lo", "timber_slope_hi", false, "0.9", "1.1"},
        {"ore_vein_ordered", "ore_vein_lo", "ore_vein_hi", false, "0.1", "0.15"},
        {"ore_elev_ordered", "ore_elev_lo_m", "ore_elev_hi_m", true, "300", "500"},
        {"ore_slope_ordered", "ore_slope_lo", "ore_slope_hi", false, "0.5", "0.6"},
        {"hab_slope_ordered", "hab_slope_lo", "hab_slope_hi", false, "0.3", "0.4"},
    };
    auto text = [](const Pair& p, const char* v) { return p.is_int ? std::string(v) : "\"" + std::string(v) + "\""; };
    for (const Pair& p : pairs) {
        CAPTURE(std::string_view(p.check));
        const std::string message = std::string(p.lo) + " must be below " + p.hi;
        // a < b: loads.
        CHECK(load_tuning({{p.lo, text(p, p.a)}, {p.hi, text(p, p.b)}}).ok());
        // Equal.
        CHECK(only_error(load_tuning({{p.lo, text(p, p.a)}, {p.hi, text(p, p.a)}})).to_string() ==
              check_error(p.check, message));
        // Reversed.
        CHECK(only_error(load_tuning({{p.lo, text(p, p.b)}, {p.hi, text(p, p.a)}})).to_string() ==
              check_error(p.check, message));
    }
    // Two failing checks: one error each, in registration order.
    const resource_api::Load two = load_tuning({{"fert_slope_lo", "\"0.5\""}, {"hab_slope_lo", "\"0.5\""}});
    INFO(all_errors(two));
    REQUIRE(two.errors.size() == 2U);
    CHECK(two.errors[0].to_string() == check_error("fert_slope_ordered", "fert_slope_lo must be below fert_slope_hi"));
    CHECK(two.errors[1].to_string() == check_error("hab_slope_ordered", "hab_slope_lo must be below hab_slope_hi"));
}

TEST_CASE("P1-05 4.2, 10.1 test 4: hab_weights_positive: all four weights 0 fail; any one at 1 loads") {
    const Overrides zero = {{"hab_weight_fertility", "0"}, {"hab_weight_forage", "0"}, {"hab_weight_timber", "0"},
                            {"hab_weight_ore", "0"}};
    CHECK(only_error(load_tuning(zero)).to_string() ==
          check_error("hab_weights_positive", "the hab_weight fields must not all be 0"));
    for (std::size_t k = 0; k < zero.size(); ++k) {
        Overrides one = zero;
        one[k].second = "1";
        CAPTURE(one[k].first);
        CHECK(load_tuning(one).ok());
    }
}

// --- Bounds (10.1 test 4) ------------------------------------------------------------------------

TEST_CASE("P1-05 4.2, 10.1 test 4: each field one step outside its bounds is a range error; each is required") {
    // A bound at which the field loads is tested only where no ordered pair
    // forbids it (the lower field at its lower bound, the upper field at its
    // upper bound; free fields at both).
    const std::vector<std::string> lower_of_pair = {"fresh_rain_lo_mm", "fert_slope_lo", "timber_slope_lo",
                                                    "ore_vein_lo",      "ore_elev_lo_m", "ore_slope_lo",
                                                    "hab_slope_lo"};
    const std::vector<std::string> upper_of_pair = {"fresh_rain_hi_mm", "fert_slope_hi", "timber_slope_hi",
                                                    "ore_vein_hi",      "ore_elev_hi_m", "ore_slope_hi",
                                                    "hab_slope_hi"};
    auto in = [](const std::vector<std::string>& v, const std::string& s) {
        return std::find(v.begin(), v.end(), s) != v.end();
    };
    for (const auto& f : resource_data::tuning_fields()) {
        if (f.kind == "fbm") {
            continue;
        }
        const std::string name(f.name);
        CAPTURE(name);
        const bool lo_ok = !in(upper_of_pair, name);
        const bool hi_ok = !in(lower_of_pair, name);
        if (f.kind == "int") {
            const std::int64_t lo = std::stoll(std::string(f.lo)), hi = std::stoll(std::string(f.hi));
            if (lo_ok) {
                CHECK_MESSAGE(load_tuning({{name, std::to_string(lo)}}).ok(), all_errors(load_tuning({{name, std::to_string(lo)}})));
            }
            if (hi_ok) {
                CHECK_MESSAGE(load_tuning({{name, std::to_string(hi)}}).ok(), all_errors(load_tuning({{name, std::to_string(hi)}})));
            }
            check_rejected(load_tuning({{name, std::to_string(lo - 1)}}), name);
            check_rejected(load_tuning({{name, std::to_string(hi + 1)}}), name);
            check_rejected(load_tuning({{name, "\"" + std::to_string(lo) + "\""}}), name);
        } else {
            const std::int64_t lo = fx_of(f.lo), hi = fx_of(f.hi);
            if (lo_ok) {
                CHECK_MESSAGE(load_tuning({{name, fx_text(lo)}}).ok(), all_errors(load_tuning({{name, fx_text(lo)}})));
            }
            if (hi_ok) {
                CHECK_MESSAGE(load_tuning({{name, fx_text(hi)}}).ok(), all_errors(load_tuning({{name, fx_text(hi)}})));
            }
            check_rejected(load_tuning({{name, fx_text(lo - 1)}}), name);
            check_rejected(load_tuning({{name, fx_text(hi + 1)}}), name);
            check_rejected(load_tuning({{name, "0.5"}}), "floating-point");
        }
        check_rejected(load_tuning({}, name), name);  // every field is required
    }
    // ore_noise is an fbm_m record (P1-01 4.1).
    const std::string o = "ore_noise";
    CHECK(load_tuning({{o, "{ wavelength_m = 64, octaves = 1, gain = \"0.0625\" }"}}).ok());
    CHECK(load_tuning({{o, "{ wavelength_m = 65536, octaves = 10, gain = 1 }"}}).ok());
    check_rejected(load_tuning({{o, "{ wavelength_m = 63, octaves = 3, gain = \"0.5\" }"}}), "wavelength_m");
    check_rejected(load_tuning({{o, "{ wavelength_m = 1500, octaves = 11, gain = \"0.5\" }"}}), "octaves");
    check_rejected(load_tuning({{o, "{ wavelength_m = 1500, octaves = 3 }"}}), "gain");
    check_rejected(load_tuning({}, o), o);
    // An unknown field.
    check_rejected(resource_api::load(std::string(resource_data::TUNING_TOML) + "coast_bonus = 1\n", biome_data::directory()),
                   "coast_bonus");
}

// --- The biome field forage and steppe's fertility (4.1, 10.1 tests 4 and 6) ---------------------

TEST_CASE("P1-05 4.1, 10.1 test 6: the twelve forage values and steppe's fertility 0.7; forage after timber") {
    REQUIRE(resource_data::forage_registered());
    const resource_api::Load l = load_defs({});
    INFO(all_errors(l));
    REQUIRE(l.ok());
    const std::vector<std::string> order = {"code",  "name",     "colour", "water",  "water_body", "priority",
                                            "fallback", "rules", "fertility", "timber", "forage",     "region_noun"};
    for (const auto& [code, want] : biome_values()) {
        CAPTURE(int{code});
        const auto it = std::find_if(biome_test::shipped_defs().begin(), biome_test::shipped_defs().end(),
                                     [&](const biome_test::Def& d) { return d.code == code; });
        REQUIRE(it != biome_test::shipped_defs().end());
        const auto* def = l.content->find("biome", it->key);
        REQUIRE(def != nullptr);
        REQUIRE(def->record.size() == order.size());
        for (std::size_t i = 0; i < order.size(); ++i) {
            CHECK(def->record.name_at(i) == order[i]);
        }
        CHECK(def->record.get_fx("fertility").raw() == want.fertility);
        CHECK(def->record.get_fx("timber").raw() == want.timber);
        CHECK(def->record.get_fx("forage").raw() == want.forage);
        // The stage's definitions carry the same values.
        const auto got = resource_api::biome_values(definitions(), code);
        REQUIRE(got.has_value());
        CHECK(got->fertility == want.fertility);
        CHECK(got->timber == want.timber);
        CHECK(got->forage == want.forage);
    }
    CHECK(l.content->find("biome", "steppe")->record.get_fx("fertility").raw() == fx_of("0.7"));
    CHECK(l.content->find("biome", "marsh")->record.get_fx("forage").raw() == fx_of("0.8"));
    CHECK(l.content->find("biome", "ocean")->record.get_fx("forage").raw() == 0);
}

TEST_CASE("P1-05 4.1, 10.1 test 4: a definition without forage is a missing-field error at its header; 1.5 is out of range") {
    {
        const wd::LoadError e = only_error(load_defs({{"marsh.toml", edited("marsh.toml", "forage = \"0.8\"\n", "")}}));
        CHECK(e.path == "biomes/marsh.toml");
        CHECK(e.line == 1U);
        CHECK(e.column == 1U);
        CHECK(e.to_string() == "biomes/marsh.toml:1:1: error: missing required field 'biome.marsh.forage'");
    }
    check_rejected(load_defs({{"marsh.toml", edited("marsh.toml", "forage = \"0.8\"", "forage = \"1.5\"")}}), "forage");
    check_rejected(load_defs({{"marsh.toml", edited("marsh.toml", "forage = \"0.8\"", "forage = \"1.000000001\"")}}),
                   "forage");
    check_rejected(load_defs({{"marsh.toml", edited("marsh.toml", "forage = \"0.8\"", "forage = \"-0.000000001\"")}}),
                   "forage");
    check_rejected(load_defs({{"marsh.toml", edited("marsh.toml", "forage = \"0.8\"", "forage = 0.8")}}),
                   "floating-point");
    CHECK(load_defs({{"marsh.toml", edited("marsh.toml", "forage = \"0.8\"", "forage = 1")}}).ok());
    CHECK(load_defs({{"marsh.toml", edited("marsh.toml", "forage = \"0.8\"", "forage = 0")}}).ok());
    // forage is part of the content hash (P1-00 6.4).
    const resource_api::Load a = load_defs({});
    const resource_api::Load b = load_defs({{"marsh.toml", edited("marsh.toml", "forage = \"0.8\"", "forage = \"0.7\"")}});
    REQUIRE(a.ok());
    REQUIRE(b.ok());
    CHECK(wd::content_hash_hex(*a.content) != wd::content_hash_hex(*b.content));
}

TEST_CASE("P1-05 4.1, 4.2, P1-00 6.4: the shipped and fixture files hold the data of 4.1 and 4.2, identical") {
    const std::filesystem::path root(UNMAPPED_TEST_SOURCE_ROOT);
    const resource_api::Load want = load_defs({});
    REQUIRE(want.ok());
    for (const char* base : {"game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
        CAPTURE(std::string_view(base));
        const std::string tuning_text = read_text(root / base / "worldgen" / "resources.toml");
        std::istringstream lines{std::string(resource_data::TUNING_TOML)};
        std::string line;
        while (std::getline(lines, line)) {
            CAPTURE(line);
            CHECK(tuning_text.find(line + "\n") != std::string::npos);
        }
        // Every biome file has its forage line right after timber, and the
        // definitions load to the content of the test's data.
        wd::MemoryDirectory dir{"biomes", {}};
        for (const auto& entry : std::filesystem::directory_iterator(root / base / "biomes")) {
            const std::string text = read_text(entry.path());
            const std::string file = entry.path().filename().string();
            CAPTURE(file);
            for (const auto& fv : resource_data::forage_values()) {
                if (fv.file == file) {
                    const std::size_t t = text.find("\ntimber = ");
                    REQUIRE(t != std::string::npos);
                    const std::size_t eol = text.find('\n', t + 1);
                    CHECK(text.substr(eol + 1, 9 + fv.value.size() + 1) == "forage = " + std::string(fv.value) + "\n");
                }
            }
            dir.entries.push_back(wd::MemoryEntry{file, text, false});
        }
        std::sort(dir.entries.begin(), dir.entries.end(),
                  [](const wd::MemoryEntry& x, const wd::MemoryEntry& y) { return x.name < y.name; });
        const resource_api::Load got = resource_api::load(tuning_text, dir);
        INFO(all_errors(got));
        REQUIRE(got.ok());
        CHECK(wd::content_hash_hex(*got.content) == wd::content_hash_hex(*want.content));
    }
}
