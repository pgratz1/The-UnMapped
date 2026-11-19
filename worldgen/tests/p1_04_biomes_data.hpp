#pragma once

// The biome data of P1-04 4.1 and the tuning of 4.2 as test text, and the
// helpers that add them to a data root once the stage has registered the
// `biome` kind and the `worldgen.biomes` id (P1-00 6.1, 6.6).
//
// No dependency on the stage's API: the existing tests' data roots (the
// elevation, hydrology and climate helpers, the CLI sandboxes) include this
// header and add the biome data only when the production registry knows the
// kind, so they load both before and after the biomes merge.

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p3_01_history_data.hpp"
#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

namespace biome_data {

// P1-04 4.2 (third version, 0.9.0), the shipped file and the fixtures' (identical).
inline constexpr std::string_view TUNING_TOML =
    "[worldgen.biomes]\n"
    "jitter_noise = { wavelength_m = 500, octaves = 4, gain = \"0.5\" }\n"
    "temp_jitter_c = 1\n"
    "rain_jitter = \"0.2\"\n"
    "rain_scale_512 = \"1.125\"\n"
    "rain_scale_1024 = 1\n"
    "rain_scale_2048 = \"0.875\"\n"
    "drain_river_class_min = 3\n"
    "river_dist_class_min = 4\n"
    "wet_window_m = 480\n"
    "wet_hand_m = 5\n"
    "wet_scale_512 = 1\n"
    "wet_scale_1024 = 1\n"
    "wet_scale_2048 = \"1.5\"\n";

// The tuning fields of 4.2 in declaration order with their shipped text.
struct FieldText {
    std::string_view name;
    std::string_view value;
};
inline const std::vector<FieldText>& tuning_fields() {
    static const std::vector<FieldText> f = {
        {"jitter_noise", "{ wavelength_m = 500, octaves = 4, gain = \"0.5\" }"},
        {"temp_jitter_c", "1"},
        {"rain_jitter", "\"0.2\""},
        {"rain_scale_512", "\"1.125\""},
        {"rain_scale_1024", "1"},
        {"rain_scale_2048", "\"0.875\""},
        {"drain_river_class_min", "3"},
        {"river_dist_class_min", "4"},
        {"wet_window_m", "480"},
        {"wet_hand_m", "5"},
        {"wet_scale_512", "1"},
        {"wet_scale_1024", "1"},
        {"wet_scale_2048", "\"1.5\""},
    };
    return f;
}

// [worldgen.biomes] with overrides (last wins) and an optional omitted field.
inline std::string tuning_toml(const std::vector<std::pair<std::string, std::string>>& overrides = {},
                               std::string_view omit = {}) {
    std::string s = "[worldgen.biomes]\n";
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

// One shipped definition file (one file per biome, the recommended layout of
// 4.1; the spec shows them as one file). Text exactly as in 4.1 without the
// comments; twelve definitions since 0.9.0.
struct DefinitionFile {
    std::string_view name;  // file name in biomes/
    std::string_view text;
};

inline const std::vector<DefinitionFile>& shipped_files() {
    static const std::vector<DefinitionFile> f = {
        {"ocean.toml",
         "[biome.ocean]\ncode = 1\nname = \"Ocean\"\ncolour = 0x183878\nwater = true\n"
         "water_body = \"ocean\"\npriority = 0\nfallback = false\nrules = []\nfertility = 0\n"
         "timber = 0\nforage = 0\nregion_noun = \"Sea\"\n"},
        {"lake.toml",
         "[biome.lake]\ncode = 2\nname = \"Lake\"\ncolour = 0x4080CC\nwater = true\n"
         "water_body = \"lake\"\npriority = 0\nfallback = false\nrules = []\nfertility = 0\n"
         "timber = 0\nforage = 0\nregion_noun = \"Mere\"\n"},
        {"temperate_forest.toml",
         "[biome.temperate_forest]\ncode = 3\nname = \"Temperate forest\"\ncolour = 0x2E6034\n"
         "water = false\nwater_body = \"none\"\npriority = 20\nfallback = false\n"
         "fertility = \"0.7\"\ntimber = 1\nforage = \"0.5\"\nregion_noun = \"Wood\"\n\n"
         "[[biome.temperate_forest.rules]]\ntemp_min_c = -60\ntemp_max_c = 50\nrain_min_mm = 720\n"
         "rain_max_mm = 10000\nelev_min_m = 0\nelev_max_m = 3000\nslope_max = 100\n"
         "hand_max_m = 3000\ncoast_max_m = 32767\nriver_dist_max_m = 32767\nwet_frac_min = 0\n"},
        {"grassland.toml",
         "[biome.grassland]\ncode = 4\nname = \"Grassland\"\ncolour = 0x96AA5A\nwater = false\n"
         "water_body = \"none\"\npriority = 10\nfallback = true\nrules = []\nfertility = 1\n"
         "timber = \"0.15\"\nforage = \"0.3\"\nregion_noun = \"Downs\"\n"},
        {"marsh.toml",
         "[biome.marsh]\ncode = 5\nname = \"Marsh\"\ncolour = 0x6AA396\nwater = false\n"
         "water_body = \"none\"\npriority = 40\nfallback = false\nfertility = \"0.45\"\n"
         "timber = \"0.3\"\nforage = \"0.8\"\nregion_noun = \"Marsh\"\n\n[[biome.marsh.rules]]\ntemp_min_c = -60\n"
         "temp_max_c = 50\nrain_min_mm = 500\nrain_max_mm = 10000\nelev_min_m = 0\n"
         "elev_max_m = 200\nslope_max = \"0.05\"\nhand_max_m = 1\ncoast_max_m = 32767\n"
         "river_dist_max_m = 32767\nwet_frac_min = 0\n"},
        {"highland_moor.toml",
         "[biome.highland_moor]\ncode = 6\nname = \"Highland moor\"\ncolour = 0x7E6680\n"
         "water = false\nwater_body = \"none\"\npriority = 30\nfallback = false\n"
         "fertility = \"0.3\"\ntimber = \"0.1\"\nforage = \"0.25\"\nregion_noun = \"Moor\"\n\n"
         "[[biome.highland_moor.rules]]\ntemp_min_c = -60\ntemp_max_c = 4\nrain_min_mm = 720\n"
         "rain_max_mm = 10000\nelev_min_m = 300\nelev_max_m = 3000\nslope_max = 100\n"
         "hand_max_m = 3000\ncoast_max_m = 32767\nriver_dist_max_m = 32767\nwet_frac_min = 0\n"},
        {"alpine.toml",
         "[biome.alpine]\ncode = 7\nname = \"Alpine\"\ncolour = 0xBEBEB4\nwater = false\n"
         "water_body = \"none\"\npriority = 50\nfallback = false\nfertility = 0\ntimber = 0\nforage = \"0.05\"\n"
         "region_noun = \"Peaks\"\n\n[[biome.alpine.rules]]\ntemp_min_c = -60\n"
         "temp_max_c = \"0.5\"\nrain_min_mm = 0\nrain_max_mm = 10000\nelev_min_m = 600\n"
         "elev_max_m = 3000\nslope_max = 100\nhand_max_m = 3000\ncoast_max_m = 32767\n"
         "river_dist_max_m = 32767\nwet_frac_min = 0\n"},
        {"blighted_waste.toml",
         "[biome.blighted_waste]\ncode = 8\nname = \"Blighted waste\"\ncolour = 0x605446\n"
         "water = false\nwater_body = \"none\"\npriority = 0\nfallback = false\nrules = []\n"
         "fertility = 0\ntimber = 0\nforage = 0\nregion_noun = \"Waste\"\n"},
        {"heath.toml",
         "[biome.heath]\ncode = 9\nname = \"Heath\"\ncolour = 0x94664A\nwater = false\n"
         "water_body = \"none\"\npriority = 25\nfallback = false\nfertility = \"0.2\"\n"
         "timber = \"0.05\"\nforage = \"0.2\"\nregion_noun = \"Heath\"\n\n[[biome.heath.rules]]\ntemp_min_c = -60\n"
         "temp_max_c = 4\nrain_min_mm = 0\nrain_max_mm = 10000\nelev_min_m = 300\n"
         "elev_max_m = 3000\nslope_max = 100\nhand_max_m = 3000\ncoast_max_m = 32767\n"
         "river_dist_max_m = 32767\nwet_frac_min = 0\n"},
        {"steppe.toml",
         "[biome.steppe]\ncode = 10\nname = \"Steppe\"\ncolour = 0xE0D296\nwater = false\n"
         "water_body = \"none\"\npriority = 15\nfallback = false\nfertility = \"0.7\"\n"
         "timber = \"0.05\"\nforage = \"0.35\"\nregion_noun = \"Steppe\"\n\n[[biome.steppe.rules]]\ntemp_min_c = -60\n"
         "temp_max_c = 50\nrain_min_mm = 0\nrain_max_mm = 600\nelev_min_m = 0\nelev_max_m = 3000\n"
         "slope_max = 100\nhand_max_m = 3000\ncoast_max_m = 32767\nriver_dist_max_m = 32767\n"
         "wet_frac_min = 0\n"},
        {"wet_woodland.toml",
         "[biome.wet_woodland]\ncode = 11\nname = \"Wet woodland\"\ncolour = 0x23505E\n"
         "water = false\nwater_body = \"none\"\npriority = 18\nfallback = false\n"
         "fertility = \"0.5\"\ntimber = \"0.7\"\nforage = \"0.7\"\nregion_noun = \"Carr\"\n\n"
         "[[biome.wet_woodland.rules]]\ntemp_min_c = -60\ntemp_max_c = 50\nrain_min_mm = 450\n"
         "rain_max_mm = 10000\nelev_min_m = 0\nelev_max_m = 300\nslope_max = 100\nhand_max_m = 5\n"
         "coast_max_m = 32767\nriver_dist_max_m = 112\nwet_frac_min = 0\n"},
        {"fen.toml",
         "[biome.fen]\ncode = 12\nname = \"Fen\"\ncolour = 0x8A8070\nwater = false\n"
         "water_body = \"none\"\npriority = 45\nfallback = false\nfertility = \"0.25\"\n"
         "timber = \"0.2\"\nforage = \"0.6\"\nregion_noun = \"Fen\"\n\n[[biome.fen.rules]]\ntemp_min_c = -60\n"
         "temp_max_c = 50\nrain_min_mm = 500\nrain_max_mm = 10000\nelev_min_m = 0\n"
         "elev_max_m = 200\nslope_max = \"0.05\"\nhand_max_m = 3\ncoast_max_m = 32767\n"
         "river_dist_max_m = 32767\nwet_frac_min = \"0.45\"\n"},
    };
    return f;
}

// The shipped definitions as a biomes/ directory, with `replace` entries
// substituted by file name (an empty text drops the file) and `extra` files
// appended.
inline worldgen::data::MemoryDirectory directory(const std::vector<std::pair<std::string, std::string>>& replace = {},
                                                 const std::vector<std::pair<std::string, std::string>>& extra = {}) {
    worldgen::data::MemoryDirectory d{"biomes", {}};
    for (const DefinitionFile& f : shipped_files()) {
        std::string text(f.text);
        bool keep = true;
        for (const auto& r : replace) {
            if (r.first == f.name) {
                text = r.second;
                keep = !text.empty();
            }
        }
        if (keep) {
            d.entries.push_back(worldgen::data::MemoryEntry{std::string(f.name), text, false});
        }
    }
    for (const auto& e : extra) {
        d.entries.push_back(worldgen::data::MemoryEntry{e.first, e.second, false});
    }
    return d;
}

// worldgen.resources (P1-05 4.2), the shipped file and the fixtures'
// (identical). Every data root with a worldgen/ directory must define it
// once the id is registered (P1-00 6.1).
inline constexpr std::string_view RESOURCES_TOML =
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

// worldgen.regions (P1-06 4), the shipped file and the fixtures'
// (identical values). Every data root with a worldgen/ directory must define
// it once the id is registered (P1-00 6.1).
inline constexpr std::string_view REGIONS_TOML =
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
    "names = { central = \"Central\", north = \"North\", northeast = \"Northeast\", east = \"East\", "
    "southeast = \"Southeast\", south = \"South\", southwest = \"Southwest\", west = \"West\", "
    "northwest = \"Northwest\", isle = \"Isle\", upland = \"Fells\" }\n"
    "upland_own_noun = [\"alpine\", \"heath\", \"highland_moor\"]\n";

[[nodiscard]] inline bool regions_registered(const worldgen::data::KindRegistry& registry) {
    const worldgen::data::Kind* k = registry.find("worldgen");
    return k != nullptr && k->schema_for("regions") != nullptr;
}

[[nodiscard]] inline bool resources_registered(const worldgen::data::KindRegistry& registry) {
    const worldgen::data::Kind* k = registry.find("worldgen");
    return k != nullptr && k->schema_for("resources") != nullptr;
}

// True when the registry has the stage's kind (the biomes merge registers
// `biome` and the `worldgen` id `biomes` together, P1-04 4.1, 4.2).
[[nodiscard]] inline bool registered(const worldgen::data::KindRegistry& registry) {
    return registry.find("biome") != nullptr;
}
[[nodiscard]] inline bool tuning_registered(const worldgen::data::KindRegistry& registry) {
    const worldgen::data::Kind* k = registry.find("worldgen");
    return k != nullptr && k->schema_for("biomes") != nullptr;
}

// Adds worldgen/biomes.toml to `worldgen_dir` and the biomes/ directory to
// `root`, each only when the registry has it.
inline void add_to(const worldgen::data::KindRegistry& registry, worldgen::data::MemoryDataRoot& root,
                   worldgen::data::MemoryDirectory& worldgen_dir) {
    if (tuning_registered(registry)) {
        worldgen_dir.entries.push_back(worldgen::data::MemoryEntry{"biomes.toml", std::string(TUNING_TOML), false});
    }
    if (resources_registered(registry)) {
        worldgen_dir.entries.push_back(
            worldgen::data::MemoryEntry{"resources.toml", std::string(RESOURCES_TOML), false});
    }
    if (regions_registered(registry)) {
        worldgen_dir.entries.push_back(worldgen::data::MemoryEntry{"regions.toml", std::string(REGIONS_TOML), false});
    }
    if (registered(registry)) {
        root.directories.push_back(directory());
    }
    history_data::add_to(registry, root);  // races/, cultures/, history/ (P3-01) when registered
}

// The same for a data root on disk (CLI sandboxes): writes
// <root>/worldgen/biomes.toml and <root>/biomes/*.toml when registered.
inline void write_to(const std::filesystem::path& root) {
    const worldgen::data::KindRegistry registry = worldgen::data::builtin_kinds();
    auto write = [](const std::filesystem::path& p, std::string_view text) {
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
    };
    if (tuning_registered(registry)) {
        write(root / "worldgen" / "biomes.toml", TUNING_TOML);
    }
    if (resources_registered(registry)) {
        write(root / "worldgen" / "resources.toml", RESOURCES_TOML);
    }
    if (regions_registered(registry)) {
        write(root / "worldgen" / "regions.toml", REGIONS_TOML);
    }
    if (registered(registry)) {
        for (const DefinitionFile& f : shipped_files()) {
            write(root / "biomes" / std::string(f.name), f.text);
        }
    }
    history_data::write_to(root);  // races/, cultures/, history/ (P3-01) when registered
}

}  // namespace biome_data
