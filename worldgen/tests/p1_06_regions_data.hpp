#pragma once

// The tuning `worldgen.regions` of P1-06 4 as test text, and the helpers
// that add it to a data root once the stage has registered the id
// (P1-00 6.1; P0-03 amendment 16: every data directory with a worldgen/
// directory defines every id).
//
// No dependency on the stage's API: the existing tests' data roots (the
// elevation, hydrology, climate, biome and resources helpers, the CLI
// sandboxes) use this header through p1_04_biomes_data.hpp and add
// worldgen/regions.toml only when the production registry knows the id, so
// they load both before and after the regions merge.

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

namespace region_data {

// P1-06 4, the shipped file and the fixtures' (identical).
inline constexpr std::string_view TUNING_TOML =
    "[worldgen.regions]\n"
    "seed_tile_m = 864\n"
    "seed_spacing_m = 1200\n"
    "hand_ref_m = 25\n"
    "cost_noise = { wavelength_m = 1500, octaves = 3, gain = \"0.5\" }\n"
    "cost_noise_amp = \"0.5\"\n"
    "lake_cost_factor = 2\n"
    "river_border_class = 6\n"
    "river_cost_m = 2000\n"
    "region_min_area_m2 = 750000\n"
    "ore_rich_min = 128\n"
    "ore_reach_m = 300\n"
    "name_central_frac = \"0.12\"\n"
    "names = { central = \"Central\", north = \"North\", northeast = \"Northeast\", east = \"East\", southeast = "
    "\"Southeast\", south = \"South\", southwest = \"Southwest\", west = \"West\", northwest = \"Northwest\", isle = "
    "\"Isle\", upland = \"Fells\" }\n"
    "upland_own_noun = [\"alpine\", \"heath\", \"highland_moor\"]\n"
    // P1-06 r3 (P3-08): the two name records.
    "name_pool_weights = { nouns = [0, 1, 0], suffixes = [0, 4, 1], prefixes = [6, 3, 2], adjectives = [6, 3, 2], "
    "features = [5, 1, 2], possessors = [8, 1, 1], places = [8, 1, 1], objects = [6, 3, 2] }\n"
    "name_traits = { coast_cells = 128, river_class = 7, lake_permille = 125, high_mean_m = 400, "
    "ore_rich_cells = 1000, barren_hab_max = 20, fertile_min = 200 }\n";

// The test-only tuning of P1-06 10.2 for N = 128 worlds: tiles of 256 m,
// seeds 400 m apart, regions of at least 65536 m^2 (256 cells).
inline constexpr std::string_view TEST128_TOML =
    "[worldgen.regions]\n"
    "seed_tile_m = 256\n"
    "seed_spacing_m = 400\n"
    "hand_ref_m = 25\n"
    "cost_noise = { wavelength_m = 1500, octaves = 3, gain = \"0.5\" }\n"
    "cost_noise_amp = \"0.5\"\n"
    "lake_cost_factor = 2\n"
    "river_border_class = 6\n"
    "river_cost_m = 2000\n"
    "region_min_area_m2 = 65536\n"
    "ore_rich_min = 128\n"
    "ore_reach_m = 300\n"
    "name_central_frac = \"0.12\"\n"
    "names = { central = \"Central\", north = \"North\", northeast = \"Northeast\", east = \"East\", southeast = "
    "\"Southeast\", south = \"South\", southwest = \"Southwest\", west = \"West\", northwest = \"Northwest\", isle = "
    "\"Isle\", upland = \"Fells\" }\n"
    "upland_own_noun = [\"alpine\", \"heath\", \"highland_moor\"]\n"
    // P1-06 r3 (P3-08): the two name records.
    "name_pool_weights = { nouns = [0, 1, 0], suffixes = [0, 4, 1], prefixes = [6, 3, 2], adjectives = [6, 3, 2], "
    "features = [5, 1, 2], possessors = [8, 1, 1], places = [8, 1, 1], objects = [6, 3, 2] }\n"
    "name_traits = { coast_cells = 128, river_class = 7, lake_permille = 125, high_mean_m = 400, "
    "ore_rich_cells = 1000, barren_hab_max = 20, fertile_min = 200 }\n";

// The fields of 4 in declaration order with their shipped TOML text, their
// kind ("int", "fx", "fbm" or "names") and their bounds as written in 4
// (fx bounds as decimals).
struct FieldText {
    std::string_view name;
    std::string_view value;
    std::string_view kind;
    std::string_view lo;
    std::string_view hi;
};

inline constexpr std::string_view SHIPPED_NAMES_TOML =
    "{ central = \"Central\", north = \"North\", northeast = \"Northeast\", east = \"East\", southeast = "
    "\"Southeast\", south = \"South\", southwest = \"Southwest\", west = \"West\", northwest = \"Northwest\", isle = "
    "\"Isle\", upland = \"Fells\" }";

inline const std::vector<FieldText>& tuning_fields() {
    static const std::vector<FieldText> f = {
        {"seed_tile_m", "864", "int", "256", "32768"},
        {"seed_spacing_m", "1200", "int", "160", "32768"},
        {"hand_ref_m", "25", "int", "1", "10000"},
        {"cost_noise", "{ wavelength_m = 1500, octaves = 3, gain = \"0.5\" }", "fbm", "", ""},
        {"cost_noise_amp", "\"0.5\"", "fx", "0", "0.9"},
        {"lake_cost_factor", "2", "fx", "1", "10"},
        {"river_border_class", "6", "int", "1", "16"},
        {"river_cost_m", "2000", "int", "0", "100000"},
        {"region_min_area_m2", "750000", "int", "0", "100000000"},
        {"ore_rich_min", "128", "int", "1", "255"},
        {"ore_reach_m", "300", "int", "0", "10000"},
        {"name_central_frac", "\"0.12\"", "fx", "0", "0.5"},
        {"names", SHIPPED_NAMES_TOML, "names", "", ""},
        {"upland_own_noun", "[\"alpine\", \"heath\", \"highland_moor\"]", "keys", "", ""},  // r2
    };
    return f;
}

// The ten fields of record `region_names`, in declaration order.
inline const std::vector<std::string_view>& name_fields() {
    static const std::vector<std::string_view> f = {"central", "north",     "northeast", "east", "southeast",
                                                    "south",   "southwest", "west",      "northwest", "isle", "upland"};
    return f;
}

// worldgen/regions.toml with some fields replaced (TOML text) and one
// field (or none) left out. tuning_toml() equals TUNING_TOML.
inline std::string tuning_toml(const std::vector<std::pair<std::string, std::string>>& overrides = {},
                               std::string_view omit = {}) {
    std::string s = "[worldgen.regions]\n";
    for (const FieldText& f : tuning_fields()) {
        if (f.name == omit) {
            continue;
        }
        std::string v(f.value);
        for (const auto& [k, text] : overrides) {
            if (k == f.name) {
                v = text;
            }
        }
        s += std::string(f.name) + " = " + v + "\n";
    }
    return s;
}

// A `names` record with one word replaced (or one field left out).
inline std::string names_toml(std::string_view field = {}, std::string_view word = {}, bool omit = false) {
    static const std::vector<std::string_view> words = {"Central", "North", "Northeast", "East", "Southeast",
                                                        "South",   "Southwest", "West", "Northwest", "Isle", "Fells"};
    std::string s = "{ ";
    bool first = true;
    for (std::size_t k = 0; k < name_fields().size(); ++k) {
        const std::string_view f = name_fields()[k];
        if (f == field && omit) {
            continue;
        }
        s += (first ? "" : ", ") + std::string(f) + " = \"" + std::string(f == field ? word : words[k]) + "\"";
        first = false;
    }
    return s + " }";
}

[[nodiscard]] inline bool registered(const worldgen::data::KindRegistry& registry) {
    const worldgen::data::Kind* k = registry.find("worldgen");
    return k != nullptr && k->schema_for("regions") != nullptr;
}

[[nodiscard]] inline bool registered() { return registered(worldgen::data::builtin_kinds()); }

// Adds worldgen/regions.toml to a memory `worldgen` directory when the id
// is registered.
inline void add_to(const worldgen::data::KindRegistry& registry, worldgen::data::MemoryDirectory& worldgen_dir) {
    if (registered(registry)) {
        worldgen_dir.entries.push_back(worldgen::data::MemoryEntry{"regions.toml", std::string(TUNING_TOML), false});
    }
}

// The same for a data root on disk: writes <root>/worldgen/regions.toml
// when the id is registered.
inline void write_to(const std::filesystem::path& root) {
    if (!registered()) {
        return;
    }
    std::filesystem::create_directories(root / "worldgen");
    std::ofstream out(root / "worldgen" / "regions.toml", std::ios::binary | std::ios::trunc);
    out.write(TUNING_TOML.data(), static_cast<std::streamsize>(TUNING_TOML.size()));
}

}  // namespace region_data
