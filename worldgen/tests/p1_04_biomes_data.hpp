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

#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

namespace biome_data {

// P1-04 4.2, the shipped file and the fixtures' (identical).
inline constexpr std::string_view TUNING_TOML =
    "[worldgen.biomes]\n"
    "jitter_noise = { wavelength_m = 500, octaves = 4, gain = \"0.5\" }\n"
    "temp_jitter_c = 1\n"
    "rain_jitter = \"0.2\"\n"
    "rain_scale_512 = \"1.125\"\n"
    "rain_scale_1024 = 1\n"
    "rain_scale_2048 = \"0.875\"\n"
    "drain_river_class_min = 3\n";

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
// comments.
struct DefinitionFile {
    std::string_view name;  // file name in biomes/
    std::string_view text;
};

inline const std::vector<DefinitionFile>& shipped_files() {
    static const std::vector<DefinitionFile> f = {
        {"ocean.toml",
         "[biome.ocean]\ncode = 1\nname = \"Ocean\"\ncolour = 0x183878\nwater = true\nwater_body = \"ocean\"\n"
         "priority = 0\nfallback = false\nrules = []\nfertility = 0\ntimber = 0\nregion_noun = \"Sea\"\n"},
        {"lake.toml",
         "[biome.lake]\ncode = 2\nname = \"Lake\"\ncolour = 0x4080CC\nwater = true\nwater_body = \"lake\"\n"
         "priority = 0\nfallback = false\nrules = []\nfertility = 0\ntimber = 0\nregion_noun = \"Mere\"\n"},
        {"temperate_forest.toml",
         "[biome.temperate_forest]\ncode = 3\nname = \"Temperate forest\"\ncolour = 0x2E6034\nwater = false\n"
         "water_body = \"none\"\npriority = 20\nfallback = false\nfertility = \"0.7\"\ntimber = 1\n"
         "region_noun = \"Wood\"\n\n[[biome.temperate_forest.rules]]\ntemp_min_c = -60\ntemp_max_c = 50\n"
         "rain_min_mm = 720\nrain_max_mm = 10000\nelev_min_m = 0\nelev_max_m = 3000\nslope_max = 100\n"
         "hand_max_m = 3000\ncoast_max_m = 32767\n"},
        {"grassland.toml",
         "[biome.grassland]\ncode = 4\nname = \"Grassland\"\ncolour = 0x96AA5A\nwater = false\n"
         "water_body = \"none\"\npriority = 10\nfallback = true\nrules = []\nfertility = 1\ntimber = \"0.15\"\n"
         "region_noun = \"Downs\"\n"},
        {"marsh.toml",
         "[biome.marsh]\ncode = 5\nname = \"Marsh\"\ncolour = 0x5C6E54\nwater = false\nwater_body = \"none\"\n"
         "priority = 40\nfallback = false\nfertility = \"0.45\"\ntimber = \"0.3\"\nregion_noun = \"Fen\"\n\n"
         "[[biome.marsh.rules]]\ntemp_min_c = -60\ntemp_max_c = 50\nrain_min_mm = 500\nrain_max_mm = 10000\n"
         "elev_min_m = 0\nelev_max_m = 200\nslope_max = \"0.05\"\nhand_max_m = 1\ncoast_max_m = 32767\n"},
        {"highland_moor.toml",
         "[biome.highland_moor]\ncode = 6\nname = \"Highland moor\"\ncolour = 0x786468\nwater = false\n"
         "water_body = \"none\"\npriority = 30\nfallback = false\nfertility = \"0.3\"\ntimber = \"0.1\"\n"
         "region_noun = \"Moor\"\n\n[[biome.highland_moor.rules]]\ntemp_min_c = -60\ntemp_max_c = 4\n"
         "rain_min_mm = 0\nrain_max_mm = 10000\nelev_min_m = 300\nelev_max_m = 3000\nslope_max = 100\n"
         "hand_max_m = 3000\ncoast_max_m = 32767\n"},
        {"alpine.toml",
         "[biome.alpine]\ncode = 7\nname = \"Alpine\"\ncolour = 0xBEBEB4\nwater = false\nwater_body = \"none\"\n"
         "priority = 50\nfallback = false\nfertility = 0\ntimber = 0\nregion_noun = \"Peaks\"\n\n"
         "[[biome.alpine.rules]]\ntemp_min_c = -60\ntemp_max_c = \"0.5\"\nrain_min_mm = 0\nrain_max_mm = 10000\n"
         "elev_min_m = 600\nelev_max_m = 3000\nslope_max = 100\nhand_max_m = 3000\ncoast_max_m = 32767\n"},
        {"blighted_waste.toml",
         "[biome.blighted_waste]\ncode = 8\nname = \"Blighted waste\"\ncolour = 0x605446\nwater = false\n"
         "water_body = \"none\"\npriority = 0\nfallback = false\nrules = []\nfertility = 0\ntimber = 0\n"
         "region_noun = \"Waste\"\n"},
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
    if (registered(registry)) {
        root.directories.push_back(directory());
    }
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
    if (registered(registry)) {
        for (const DefinitionFile& f : shipped_files()) {
            write(root / "biomes" / std::string(f.name), f.text);
        }
    }
}

}  // namespace biome_data
