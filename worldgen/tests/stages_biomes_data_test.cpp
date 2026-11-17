// The `biome` data kind and the `worldgen.biomes` tuning (spec P1-04 4.1,
// 4.2 and 10.1 test 6; P1-00 6.1, 6.4 and 6.6; P0-03 7.5, 8 and 9).
//
// Written from the specs only. Every case loads an in-memory data root
// through the production registry (biome_api::load), so the stage's
// registration, schemas and named checks are what is tested.

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

#include "p1_04_biomes_api.hpp"
#include "p1_04_biomes_data.hpp"
#include "p1_04_biomes_support.hpp"
#include "worldgen/core/fx.hpp"
#include "worldgen/data/content.hpp"

using namespace biome_test;
namespace wd = worldgen::data;

namespace {

using Replace = std::vector<std::pair<std::string, std::string>>;

biome_api::Load load_defs(const Replace& replace, const Replace& extra = {}) {
    return biome_api::load(std::string(biome_data::TUNING_TOML), biome_data::directory(replace, extra));
}

biome_api::Load load_tuning(const Overrides& o, std::string_view omit = {}) {
    return biome_api::load(biome_data::tuning_toml(o, omit), biome_data::directory());
}

// The shipped text of one definition file with `from` replaced by `to`
// (the first occurrence, which must exist).
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

std::string check_error(std::string_view name, std::string_view id, std::string_view message) {
    return "check '" + std::string(name) + "' failed for 'biome." + std::string(id) + "': " + std::string(message);
}

// The one error of a load that must fail with exactly one error.
wd::LoadError only_error(const biome_api::Load& l) {
    INFO(all_errors(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

void check_rejected(const biome_api::Load& l, std::string_view mention) {
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

}  // namespace

// --- The shipped data loads ----------------------------------------------------------------

TEST_CASE("P1-04 4.1: the shipped definitions load; fields in declaration order with the values of 4.1") {
    const biome_api::Load l = load_defs({});
    INFO(all_errors(l));
    REQUIRE(l.ok());
    const std::vector<std::string> order = {"code",     "name",  "colour",   "water",  "water_body",  "priority",
                                            "fallback", "rules", "fertility", "timber", "region_noun"};
    for (const Def& d : shipped_defs()) {
        CAPTURE(d.key);
        const auto* def = l.content->find("biome", d.key);
        REQUIRE(def != nullptr);
        REQUIRE(def->record.size() == order.size());
        for (std::size_t i = 0; i < order.size(); ++i) {
            CHECK(def->record.name_at(i) == order[i]);
        }
        CHECK(def->record.get_int("code") == d.code);
        CHECK(def->record.get_string("name") == d.name);
        CHECK(def->record.get_int("colour") == d.colour);
        CHECK(def->record.get_bool("water") == d.water);
        CHECK(def->record.get_string("water_body") == d.water_body);
        CHECK(def->record.get_int("priority") == d.priority);
        CHECK(def->record.get_bool("fallback") == d.fallback);
        CHECK(def->record.get_array("rules").size() == d.rules.size());
    }
    const auto* marsh = l.content->find("biome", "marsh");
    REQUIRE(marsh != nullptr);
    CHECK(marsh->record.get_fx("fertility").raw() == fx_of("0.45"));
    CHECK(marsh->record.get_string("region_noun") == "Fen");
    CHECK(l.content->find("biome", "temperate_forest")->record.get_fx("timber") == Fx::from_int(1));
    static_cast<void>(biome_api::definitions_from(*l.content));
    static_cast<void>(biome_api::tuning_from(*l.content));
}

TEST_CASE("P1-04 4.2: the shipped tuning loads with the values of 4.2") {
    const biome_api::Load l = load_tuning({});
    INFO(all_errors(l));
    REQUIRE(l.ok());
    CHECK(biome_data::tuning_toml() == std::string(biome_data::TUNING_TOML));
    const auto* def = l.content->find("worldgen", "biomes");
    REQUIRE(def != nullptr);
    REQUIRE(def->record.size() == biome_data::tuning_fields().size());
    for (std::size_t i = 0; i < def->record.size(); ++i) {
        CHECK(def->record.name_at(i) == biome_data::tuning_fields()[i].name);
    }
    CHECK(def->record.get_record("jitter_noise").get_int("wavelength_m") == 500);
    CHECK(def->record.get_record("jitter_noise").get_int("octaves") == 4);
    CHECK(def->record.get_fx("temp_jitter_c") == Fx::from_int(1));
    CHECK(def->record.get_fx("rain_jitter").raw() == 858993459);
    CHECK(def->record.get_fx("rain_scale_512").raw() == 4831838208);
    CHECK(def->record.get_fx("rain_scale_1024").raw() == ONE);
    CHECK(def->record.get_fx("rain_scale_2048").raw() == 3758096384);
    CHECK(def->record.get_int("drain_river_class_min") == 3);
}

TEST_CASE("P1-00 6.1, 6.6: the biome kind and worldgen.biomes are required") {
    // No biomes/ directory.
    const biome_api::Load a = biome_api::load(std::string(biome_data::TUNING_TOML), std::nullopt);
    check_rejected(a, "biomes");
    // No worldgen/biomes.toml.
    const biome_api::Load b = biome_api::load(std::nullopt, biome_data::directory());
    check_rejected(b, "missing definition 'worldgen.biomes'");
    // An empty biomes/ directory: no ocean, lake or fallback (named checks 3, 4).
    const biome_api::Load c = biome_api::load(std::string(biome_data::TUNING_TOML), wd::MemoryDirectory{"biomes", {}});
    check_rejected(c, "biome_water_unique");
    check_rejected(c, "biome_fallback");
}

TEST_CASE("P1-04 4.1, P1-00 6.4: definitions and tuning are part of the content hash") {
    const biome_api::Load a = load_defs({});
    const biome_api::Load b = load_defs({{"marsh.toml", edited("marsh.toml", "region_noun = \"Fen\"", "region_noun = \"Bog\"")}});
    const biome_api::Load c = load_tuning({{"drain_river_class_min", "4"}});
    // File names carry no meaning (P0-03 7.4): all definitions in one file.
    std::string one;
    for (const auto& f : biome_data::shipped_files()) {
        one += std::string(f.text) + "\n";
    }
    const biome_api::Load d =
        biome_api::load(std::string(biome_data::TUNING_TOML), wd::MemoryDirectory{"biomes", {{"all.toml", one, false}}});
    for (const biome_api::Load* x : {&a, &b, &c, &d}) {
        INFO(all_errors(*x));
        REQUIRE(x->ok());
    }
    CHECK(wd::content_hash_hex(*a.content) != wd::content_hash_hex(*b.content));
    CHECK(wd::content_hash_hex(*a.content) != wd::content_hash_hex(*c.content));
    CHECK(wd::content_hash_hex(*a.content) == wd::content_hash_hex(*d.content));
}

TEST_CASE("P1-04 4.1, 4.2, P1-00 6.4: the fixture files hold the shipped definitions and tuning") {
    const std::filesystem::path root(UNMAPPED_TEST_SOURCE_ROOT);
    const biome_api::Load want = load_defs({});
    REQUIRE(want.ok());
    for (const char* base : {"game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
        CAPTURE(base);
        const std::string tuning_text = read_text(root / base / "worldgen" / "biomes.toml");
        std::istringstream lines{std::string(biome_data::TUNING_TOML)};
        std::string line;
        while (std::getline(lines, line)) {
            CAPTURE(line);
            CHECK(tuning_text.find(line + "\n") != std::string::npos);
        }
        wd::MemoryDirectory dir{"biomes", {}};
        for (const auto& entry : std::filesystem::directory_iterator(root / base / "biomes")) {
            dir.entries.push_back(wd::MemoryEntry{entry.path().filename().string(), read_text(entry.path()), false});
        }
        std::sort(dir.entries.begin(), dir.entries.end(),
                  [](const wd::MemoryEntry& x, const wd::MemoryEntry& y) { return x.name < y.name; });
        const biome_api::Load got = biome_api::load(tuning_text, dir);
        INFO(all_errors(got));
        REQUIRE(got.ok());
        CHECK(wd::content_hash_hex(*got.content) == wd::content_hash_hex(*want.content));
    }
}

// --- Named checks (4.1, 10.1 test 6) ---------------------------------------------------------

TEST_CASE("P1-04 4.1 named check 1 biome_code_unique: two definitions with one code") {
    // grassland takes code 3, which temperate_forest has.
    const biome_api::Load l = load_defs({{"grassland.toml", edited("grassland.toml", "code = 4", "code = 3")}});
    INFO(all_errors(l));
    REQUIRE_FALSE(l.ok());
    // Which of the two reports is not stated (P1-04 4.1 "errors at the
    // definition's header"); each error is one of these two forms.
    const std::string at_forest = "biomes/temperate_forest.toml:1:1: error: " +
                                  check_error("biome_code_unique", "temperate_forest", "code 3 is also used by 'grassland'");
    const std::string at_grass = "biomes/grassland.toml:1:1: error: " +
                                 check_error("biome_code_unique", "grassland", "code 3 is also used by 'temperate_forest'");
    for (const auto& e : l.errors) {
        CHECK((e.to_string() == at_forest || e.to_string() == at_grass));
    }
}

TEST_CASE("P1-04 4.1 named check 2 biome_water_body: water exactly when water_body is not none") {
    {
        const wd::LoadError e = only_error(load_defs({{"marsh.toml", edited("marsh.toml", "water = false", "water = true")}}));
        CHECK(e.path == "biomes/marsh.toml");
        CHECK(e.line == 1U);
        CHECK(e.column == 1U);
        CHECK(e.message ==
              check_error("biome_water_body", "marsh", "water must be true exactly when water_body is not \"none\""));
    }
    {
        const wd::LoadError e = only_error(load_defs({{"ocean.toml", edited("ocean.toml", "water = true", "water = false")}}));
        CHECK(e.to_string() == "biomes/ocean.toml:1:1: error: " +
                                   check_error("biome_water_body", "ocean",
                                               "water must be true exactly when water_body is not \"none\""));
    }
}

TEST_CASE("P1-04 4.1 named check 3 biome_water_unique: exactly one ocean and one lake") {
    // lake becomes a second ocean: reported at the second in id order
    // ("ocean"), and the missing lake against the directory.
    const biome_api::Load l =
        load_defs({{"lake.toml", edited("lake.toml", "water_body = \"lake\"", "water_body = \"ocean\"")}});
    INFO(all_errors(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 2U);
    // Sorted by path (P0-03 9): "biomes" before "biomes/ocean.toml".
    CHECK(l.errors[0].path == "biomes");
    CHECK(l.errors[0].line == 0U);
    CHECK(l.errors[0].message.find("biome_water_unique") != std::string::npos);
    CHECK(l.errors[0].message.find("exactly one biome must have water_body \"lake\"") != std::string::npos);
    CHECK(l.errors[1].to_string() == "biomes/ocean.toml:1:1: error: " +
                                         check_error("biome_water_unique", "ocean",
                                                     "exactly one biome must have water_body \"ocean\""));
}

TEST_CASE("P1-04 4.1 named check 4 biome_fallback: exactly one land fallback") {
    {  // two: grassland and marsh; the second in id order is marsh
        const wd::LoadError e =
            only_error(load_defs({{"marsh.toml", edited("marsh.toml", "fallback = false", "fallback = true")}}));
        CHECK(e.to_string() == "biomes/marsh.toml:1:1: error: " +
                                   check_error("biome_fallback", "marsh", "exactly one land biome must have fallback = true"));
    }
    {  // none: against the directory
        const wd::LoadError e =
            only_error(load_defs({{"grassland.toml", edited("grassland.toml", "fallback = true", "fallback = false")}}));
        CHECK(e.path == "biomes");
        CHECK(e.line == 0U);
        CHECK(e.message.find("check 'biome_fallback'") != std::string::npos);
        CHECK(e.message.find("exactly one land biome must have fallback = true") != std::string::npos);
    }
    {  // the only fallback is a water biome
        const biome_api::Load l =
            load_defs({{"grassland.toml", edited("grassland.toml", "fallback = true", "fallback = false")},
                       {"ocean.toml", edited("ocean.toml", "fallback = false", "fallback = true")}});
        check_rejected(l, "exactly one land biome must have fallback = true");
    }
}

TEST_CASE("P1-04 4.1 named check 5 biome_water_rules: water biomes have priority 0 and no rules") {
    {
        const wd::LoadError e = only_error(load_defs({{"lake.toml", edited("lake.toml", "priority = 0", "priority = 5")}}));
        CHECK(e.to_string() == "biomes/lake.toml:1:1: error: " +
                                   check_error("biome_water_rules", "lake", "water biomes take no rules and priority 0"));
    }
    {
        const std::string ocean =
            edited("ocean.toml", "rules = []\n", "") +
            "\n[[biome.ocean.rules]]\ntemp_min_c = -60\ntemp_max_c = 50\nrain_min_mm = 0\nrain_max_mm = 10000\n"
            "elev_min_m = 0\nelev_max_m = 3000\nslope_max = 100\nhand_max_m = 3000\ncoast_max_m = 32767\n";
        const wd::LoadError e = only_error(load_defs({{"ocean.toml", ocean}}));
        CHECK(e.to_string() == "biomes/ocean.toml:1:1: error: " +
                                   check_error("biome_water_rules", "ocean", "water biomes take no rules and priority 0"));
    }
}

TEST_CASE("P1-04 4.1 named check 6 biome_priority_unique: land priorities above 0 are distinct") {
    // highland_moor takes marsh's 40: reported at the second in id order (marsh).
    const wd::LoadError e = only_error(
        load_defs({{"highland_moor.toml", edited("highland_moor.toml", "priority = 30", "priority = 40")}}));
    CHECK(e.to_string() == "biomes/marsh.toml:1:1: error: " +
                               check_error("biome_priority_unique", "marsh", "priority 40 is also used by 'highland_moor'"));
    // Priority 0 may repeat (blighted_waste and the water biomes already do).
    CHECK(load_defs({{"grassland.toml", edited("grassland.toml", "priority = 10", "priority = 0")}}).ok());
}

TEST_CASE("P1-04 4.1 named check 7 biome_rule_ranges: min not above max in every rule") {
    struct Case {
        const char* min_from;
        const char* min_to;
        const char* max_from;
        const char* max_to;
        const char* field;  // the spec's <field>
        const char* full;   // the field name with its unit suffix
    };
    for (const Case& c :
         {Case{"temp_min_c = -60", "temp_min_c = 10", "temp_max_c = 50", "temp_max_c = 9", "temp", "temp_c"},
          Case{"rain_min_mm = 720", "rain_min_mm = 10000", "rain_max_mm = 10000", "rain_max_mm = 9999", "rain",
               "rain_mm"},
          Case{"elev_min_m = 0", "elev_min_m = 3000", "elev_max_m = 3000", "elev_max_m = 2999", "elev", "elev_m"}}) {
        CAPTURE(c.min_to);
        std::string text = edited("temperate_forest.toml", c.min_from, c.min_to);
        const std::size_t at = text.find(c.max_from);
        REQUIRE(at != std::string::npos);
        text.replace(at, std::string_view(c.max_from).size(), c.max_to);
        const wd::LoadError e = only_error(load_defs({{"temperate_forest.toml", text}}));
        CHECK(e.path == "biomes/temperate_forest.toml");
        CHECK(e.line == 1U);
        CHECK(e.column == 1U);
        // The spec's template "rules[<i>]: <field>_min must not exceed
        // <field>_max" with <field> = temp, rain, elev; the field names carry
        // a unit suffix (temp_min_c), so the suffixed form is accepted too
        // (ambiguity reported to the lead).
        const std::string f(c.field);
        const std::string plain = "rules[0]: " + f + "_min must not exceed " + f + "_max";
        const std::string suffix(std::string(c.full).substr(f.size()));
        const std::string suffixed = "rules[0]: " + f + "_min" + suffix + " must not exceed " + f + "_max" + suffix;
        CHECK((e.message == check_error("biome_rule_ranges", "temperate_forest", plain) ||
               e.message == check_error("biome_rule_ranges", "temperate_forest", suffixed)));
    }
    // Equal bounds are allowed; the second rule of a definition is rules[1].
    std::string moor(biome_data::shipped_files()[5].text);
    moor +=
        "\n[[biome.highland_moor.rules]]\ntemp_min_c = 5\ntemp_max_c = 5\nrain_min_mm = 9\nrain_max_mm = 8\n"
        "elev_min_m = 0\nelev_max_m = 0\nslope_max = 100\nhand_max_m = 3000\ncoast_max_m = 32767\n";
    const wd::LoadError e = only_error(load_defs({{"highland_moor.toml", moor}}));
    CHECK(e.message.find("rules[1]: rain_min") != std::string::npos);
}

// --- Types and bounds (10.1 test 6) -------------------------------------------------------

TEST_CASE("P1-04 10.1 test 6: a float literal in a rule is a P0-03 error; a missing rule field is reported at the rule's header") {
    check_rejected(load_defs({{"marsh.toml", edited("marsh.toml", "slope_max = \"0.05\"", "slope_max = 0.05")}}),
                   "floating-point");
    check_rejected(load_defs({{"marsh.toml", edited("marsh.toml", "rain_min_mm = 500", "rain_min_mm = 500.0")}}),
                   "rain_min_mm");
    check_rejected(load_defs({{"marsh.toml", edited("marsh.toml", "fertility = \"0.45\"", "fertility = 0.45")}}),
                   "floating-point");
    // coast_max_m removed from marsh's rule: the [[biome.marsh.rules]]
    // header is line 13 of marsh.toml.
    const wd::LoadError e = only_error(load_defs({{"marsh.toml", edited("marsh.toml", "coast_max_m = 32767\n", "")}}));
    CHECK(e.path == "biomes/marsh.toml");
    CHECK(e.line == 13U);
    CHECK(e.column == 1U);
    CHECK(e.message.find("coast_max_m") != std::string::npos);
    // A definition with neither `rules = []` nor rule tables: missing field.
    const wd::LoadError r = only_error(load_defs({{"grassland.toml", edited("grassland.toml", "rules = []\n", "")}}));
    CHECK(r.path == "biomes/grassland.toml");
    CHECK(r.line == 1U);
    CHECK(r.message.find("rules") != std::string::npos);
    // An unknown key in a rule.
    check_rejected(load_defs({{"marsh.toml", edited("marsh.toml", "hand_max_m = 1\n", "hand_max_m = 1\nhand_min_m = 0\n")}}),
                   "hand_min_m");
}

TEST_CASE("P1-04 4.1: every bound of the definition and rule fields rejects one step outside") {
    struct Edit {
        const char* file;
        const char* from;
        const char* ok;
        const char* bad;
    };
    const std::vector<Edit> edits = {
        {"blighted_waste.toml", "code = 8", "code = 254", "code = 255"},
        {"blighted_waste.toml", "code = 8", "code = 9", "code = 0"},
        {"blighted_waste.toml", "colour = 0x605446", "colour = 16777215", "colour = 16777216"},
        {"blighted_waste.toml", "colour = 0x605446", "colour = 0", "colour = -1"},
        {"blighted_waste.toml", "priority = 0", "priority = 0", "priority = -1"},
        {"marsh.toml", "priority = 40", "priority = 255", "priority = 256"},
        {"blighted_waste.toml", "water_body = \"none\"", "water_body = \"none\"", "water_body = \"sea\""},
        {"marsh.toml", "fertility = \"0.45\"", "fertility = 1", "fertility = \"1.000000000000000001\""},
        {"marsh.toml", "timber = \"0.3\"", "timber = 0", "timber = \"-0.000000001\""},
        {"marsh.toml", "temp_min_c = -60", "temp_min_c = -60", "temp_min_c = \"-60.000000001\""},
        {"marsh.toml", "temp_max_c = 50", "temp_max_c = 50", "temp_max_c = \"50.000000001\""},
        {"marsh.toml", "rain_max_mm = 10000", "rain_max_mm = 10000", "rain_max_mm = 10001"},
        {"marsh.toml", "rain_min_mm = 500", "rain_min_mm = 0", "rain_min_mm = -1"},
        {"marsh.toml", "elev_max_m = 200", "elev_max_m = 3000", "elev_max_m = 3001"},
        {"marsh.toml", "elev_min_m = 0", "elev_min_m = 0", "elev_min_m = -1"},
        {"marsh.toml", "slope_max = \"0.05\"", "slope_max = 100", "slope_max = \"100.000000001\""},
        {"marsh.toml", "slope_max = \"0.05\"", "slope_max = 0", "slope_max = \"-0.000000001\""},
        {"marsh.toml", "hand_max_m = 1", "hand_max_m = 3000", "hand_max_m = \"3000.000000001\""},
        {"marsh.toml", "coast_max_m = 32767", "coast_max_m = 0", "coast_max_m = 32768"},
        {"marsh.toml", "coast_max_m = 32767", "coast_max_m = 0", "coast_max_m = -1"},
        {"marsh.toml", "name = \"Marsh\"", "name = \"" "0123456789012345678901234567890123456789012345678901234567890123" "\"",
         "name = \"" "01234567890123456789012345678901234567890123456789012345678901234" "\""},
        {"marsh.toml", "region_noun = \"Fen\"", "region_noun = \"" "01234567890123456789012345678901" "\"",
         "region_noun = \"" "012345678901234567890123456789012" "\""},
        {"marsh.toml", "water = false", "water = false", "water = 0"},
    };
    for (const Edit& e : edits) {
        CAPTURE(e.bad);
        const biome_api::Load good = load_defs({{e.file, edited(e.file, e.from, e.ok)}});
        CHECK_MESSAGE(good.ok(), all_errors(good));
        check_rejected(load_defs({{e.file, edited(e.file, e.from, e.bad)}}), "");
    }
    // rules: at most 16 records.
    std::string many(biome_data::shipped_files()[4].text);
    const std::string rule =
        "\n[[biome.marsh.rules]]\ntemp_min_c = -60\ntemp_max_c = 50\nrain_min_mm = 500\nrain_max_mm = 10000\n"
        "elev_min_m = 0\nelev_max_m = 200\nslope_max = \"0.05\"\nhand_max_m = 1\ncoast_max_m = 32767\n";
    for (int i = 1; i < 16; ++i) {
        many += rule;
    }
    CHECK_MESSAGE(load_defs({{"marsh.toml", many}}).ok(), "16 rules load");
    check_rejected(load_defs({{"marsh.toml", many + rule}}), "rules");
}

TEST_CASE("P1-04 4.2, 10.1 test 6: each tuning bound rejects one step outside") {
    struct Bound {
        const char* field;
        bool is_int;
        const char* lo;
        const char* hi;
    };
    const std::vector<Bound> bounds = {
        {"temp_jitter_c", false, "0", "10"},       {"rain_jitter", false, "0", "1"},
        {"rain_scale_512", false, "0.25", "4"},    {"rain_scale_1024", false, "0.25", "4"},
        {"rain_scale_2048", false, "0.25", "4"},   {"drain_river_class_min", true, "1", "15"},
    };
    for (const Bound& b : bounds) {
        CAPTURE(b.field);
        const std::string f(b.field);
        if (b.is_int) {
            const std::int64_t lo = std::stoll(b.lo), hi = std::stoll(b.hi);
            CHECK(load_tuning({{f, std::to_string(lo)}}).ok());
            CHECK(load_tuning({{f, std::to_string(hi)}}).ok());
            check_rejected(load_tuning({{f, std::to_string(lo - 1)}}), f);
            check_rejected(load_tuning({{f, std::to_string(hi + 1)}}), f);
            check_rejected(load_tuning({{f, "3.0"}}), f);
        } else {
            const std::int64_t lo = fx_of(b.lo), hi = fx_of(b.hi);
            CHECK(load_tuning({{f, fx_text(lo)}}).ok());
            CHECK(load_tuning({{f, fx_text(hi)}}).ok());
            check_rejected(load_tuning({{f, fx_text(lo - 1)}}), f);
            check_rejected(load_tuning({{f, fx_text(hi + 1)}}), f);
            check_rejected(load_tuning({{f, "0.5"}}), "floating-point");
        }
        check_rejected(load_tuning({}, f), f);  // every field is required
    }
    // jitter_noise is an fbm_m record (P1-01 4.1).
    const std::string j = "jitter_noise";
    CHECK(load_tuning({{j, "{ wavelength_m = 64, octaves = 1, gain = \"0.0625\" }"}}).ok());
    CHECK(load_tuning({{j, "{ wavelength_m = 65536, octaves = 10, gain = 1 }"}}).ok());
    check_rejected(load_tuning({{j, "{ wavelength_m = 63, octaves = 4, gain = \"0.5\" }"}}), "wavelength_m");
    check_rejected(load_tuning({{j, "{ wavelength_m = 500, octaves = 11, gain = \"0.5\" }"}}), "octaves");
    check_rejected(load_tuning({{j, "{ wavelength_m = 500, octaves = 0, gain = \"0.5\" }"}}), "octaves");
    check_rejected(load_tuning({{j, "{ wavelength_m = 500, octaves = 4 }"}}), "gain");
    check_rejected(load_tuning({}, j), j);
    // An unknown tuning field.
    check_rejected(biome_api::load(std::string(biome_data::TUNING_TOML) + "rain_scale_4096 = 1\n", biome_data::directory()),
                   "rain_scale_4096");
}
