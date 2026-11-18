#pragma once

// The tuning `worldgen.resources` of P1-05 4.2 and the biome field `forage`
// of 4.1 as test text, and the helpers that add them to a data root once
// the stage has registered them (P1-00 6.1, 6.6).
//
// No dependency on the stage's API: the existing tests' data roots (the
// elevation, hydrology, climate and biome helpers, the CLI sandboxes) use
// this header through p1_04_biomes_data.hpp and add the resources data only
// when the production registry knows it, so they load both before and after
// the resources merge.

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/data/schema.hpp"

namespace resource_data {

// P1-05 4.2 (r2 tuning, 2026-11-18), the shipped file and the fixtures' (identical).
inline constexpr std::string_view TUNING_TOML =
    "[worldgen.resources]\n"
    "fresh_reach_m = 400\n"
    "fresh_rain_weight = \"0.4\"\n"
    "fresh_rain_lo_mm = 300\n"
    "fresh_rain_hi_mm = 1500\n"
    "fert_slope_lo = \"0.18\"\n"
    "fert_slope_hi = \"0.47\"\n"
    "alluvial_bonus = \"0.15\"\n"
    "alluvial_hand_m = 5\n"
    "timber_slope_lo = \"0.7\"\n"
    "timber_slope_hi = \"1.4\"\n"
    "fish_river_class_min = 4\n"
    "fish_weight = \"0.3\"\n"
    "fish_reach_m = 300\n"
    "ore_noise = { wavelength_m = 1500, octaves = 3, gain = \"0.5\" }\n"
    "ore_vein_lo = 0\n"
    "ore_vein_hi = \"0.2\"\n"
    "ore_elev_lo_m = 100\n"
    "ore_elev_hi_m = 600\n"
    "ore_lowland = \"0.15\"\n"
    "ore_slope_base = \"0.65\"\n"
    "ore_slope_lo = \"0.3\"\n"
    "ore_slope_hi = \"1.0\"\n"
    "walk_slope_max = \"0.7002\"\n"
    "walk_min_area_m2 = 250000\n"
    "hab_slope_lo = \"0.18\"\n"
    "hab_slope_hi = \"0.47\"\n"
    "hab_weight_fertility = 70\n"
    "hab_weight_forage = 15\n"
    "hab_weight_timber = 10\n"
    "hab_weight_ore = 5\n"
    "hab_water_min = \"0.25\"\n"
    "hab_flood_hand_m = \"0.5\"\n"
    "hab_flood_factor = \"0.75\"\n"
    "hab_pocket_factor = \"0.25\"\n";

// The fields of 4.2 in declaration order with their shipped text, their
// kind and their bounds as written in 4.2 (lo, hi as TOML text; for fx
// fields the bounds are decimals). kind: "int", "fx" or "fbm".
struct FieldText {
    std::string_view name;
    std::string_view value;
    std::string_view kind;
    std::string_view lo;
    std::string_view hi;
};

inline const std::vector<FieldText>& tuning_fields() {
    static const std::vector<FieldText> f = {
        {"fresh_reach_m", "400", "int", "16", "10000"},
        {"fresh_rain_weight", "\"0.4\"", "fx", "0", "1"},
        {"fresh_rain_lo_mm", "300", "int", "0", "10000"},
        {"fresh_rain_hi_mm", "1500", "int", "0", "10000"},
        {"fert_slope_lo", "\"0.18\"", "fx", "0", "10"},
        {"fert_slope_hi", "\"0.47\"", "fx", "0", "10"},
        {"alluvial_bonus", "\"0.15\"", "fx", "0", "4"},
        {"alluvial_hand_m", "5", "fx", "0.1", "100"},
        {"timber_slope_lo", "\"0.7\"", "fx", "0", "10"},
        {"timber_slope_hi", "\"1.4\"", "fx", "0", "10"},
        {"fish_river_class_min", "4", "int", "1", "15"},
        {"fish_weight", "\"0.3\"", "fx", "0", "1"},
        {"fish_reach_m", "300", "int", "16", "10000"},
        {"ore_noise", "{ wavelength_m = 1500, octaves = 3, gain = \"0.5\" }", "fbm", "", ""},
        {"ore_vein_lo", "0", "fx", "-1", "1"},
        {"ore_vein_hi", "\"0.2\"", "fx", "-1", "1"},
        {"ore_elev_lo_m", "100", "int", "0", "3000"},
        {"ore_elev_hi_m", "600", "int", "0", "3000"},
        {"ore_lowland", "\"0.15\"", "fx", "0", "1"},
        {"ore_slope_base", "\"0.65\"", "fx", "0", "1"},
        {"ore_slope_lo", "\"0.3\"", "fx", "0", "10"},
        {"ore_slope_hi", "\"1.0\"", "fx", "0", "10"},
        {"walk_slope_max", "\"0.7002\"", "fx", "0.01", "10"},
        {"walk_min_area_m2", "250000", "int", "16384", "1000000000"},
        {"hab_slope_lo", "\"0.18\"", "fx", "0", "10"},
        {"hab_slope_hi", "\"0.47\"", "fx", "0", "10"},
        {"hab_weight_fertility", "70", "int", "0", "1000"},
        {"hab_weight_forage", "15", "int", "0", "1000"},
        {"hab_weight_timber", "10", "int", "0", "1000"},
        {"hab_weight_ore", "5", "int", "0", "1000"},
        {"hab_water_min", "\"0.25\"", "fx", "0", "1"},
        {"hab_flood_hand_m", "\"0.5\"", "fx", "0", "100"},
        {"hab_flood_factor", "\"0.75\"", "fx", "0", "1"},
        {"hab_pocket_factor", "\"0.25\"", "fx", "0", "1"},
    };
    return f;
}

// [worldgen.resources] with overrides (field, TOML text; last wins) and an
// optional omitted field.
inline std::string tuning_toml(const std::vector<std::pair<std::string, std::string>>& overrides = {},
                               std::string_view omit = {}) {
    std::string s = "[worldgen.resources]\n";
    for (const FieldText& f : tuning_fields()) {
        if (f.name == omit) {
            continue;
        }
        std::string value(f.value);
        for (const auto& o : overrides) {
            if (o.first == f.name) {
                value = o.second;
            }
        }
        s += std::string(f.name) + " = " + value + "\n";
    }
    return s;
}

// P1-05 4.1: the `forage` line of each shipped definition file, as TOML text
// (strings where fractional, integers where whole).
struct ForageText {
    std::string_view file;   // file name in biomes/
    std::string_view value;  // TOML value
};
inline const std::vector<ForageText>& forage_values() {
    static const std::vector<ForageText> f = {
        {"ocean.toml", "0"},           {"lake.toml", "0"},
        {"temperate_forest.toml", "\"0.5\""}, {"grassland.toml", "\"0.3\""},
        {"marsh.toml", "\"0.8\""},     {"highland_moor.toml", "\"0.25\""},
        {"alpine.toml", "\"0.05\""},   {"blighted_waste.toml", "0"},
        {"heath.toml", "\"0.2\""},     {"steppe.toml", "\"0.35\""},
        {"wet_woodland.toml", "\"0.7\""}, {"fen.toml", "\"0.6\""},
    };
    return f;
}

// True when the production registry's `worldgen` kind has the id
// `resources` (P1-05 4.2; every id of the set must then be defined).
[[nodiscard]] inline bool registered(const worldgen::data::KindRegistry& registry) {
    const worldgen::data::Kind* k = registry.find("worldgen");
    return k != nullptr && k->schema_for("resources") != nullptr;
}

// True when the production registry's `biome` kind has the field `forage`
// (P1-05 4.1; required, so every definition must then carry it).
[[nodiscard]] inline bool forage_registered(const worldgen::data::KindRegistry& registry) {
    const worldgen::data::Kind* k = registry.find("biome");
    return k != nullptr && k->schema.find("forage") != nullptr;
}
[[nodiscard]] inline bool forage_registered() { return forage_registered(worldgen::data::builtin_kinds()); }

// A definition text of 0.9.0 brought to 0.10.0 (P1-05 4.1): `forage = <v>`
// after the `timber = ` line, and steppe's fertility "0.6" -> "0.7". Text
// without a `timber = ` line is returned unchanged.
inline std::string with_forage(std::string_view file, std::string text) {
    std::string_view value;
    for (const ForageText& f : forage_values()) {
        if (f.file == file) {
            value = f.value;
        }
    }
    if (value.empty()) {
        return text;
    }
    const std::size_t at = text.find("\ntimber = ");
    if (at == std::string::npos) {
        return text;
    }
    const std::size_t eol = text.find('\n', at + 1);
    text.insert(eol + 1, "forage = " + std::string(value) + "\n");
    if (file == "steppe.toml") {
        const std::size_t f = text.find("fertility = \"0.6\"");
        if (f != std::string::npos) {
            text.replace(f, 17, "fertility = \"0.7\"");
        }
    }
    return text;
}

}  // namespace resource_data
