#pragma once

// The shipped history data of spec P3-01 (sections 3.1, 4 and 5.1) and the
// naming blocks of P3-02 5, transcribed from the specs (not read from the
// data files), for the P3-01 tests.
//
// The texts are the spec listings without their leading
// "# game/data/..." comment line, so every definition header is at line 1,
// column 1. (The data roots that the P1 tests build get the history kinds
// from the engineer's p3_01_history_fixture.hpp, which copies the fixture
// files; these transcriptions are the tests' independent reading.)
//
// Mirrors RACE_FILES, NAMING and CHRONOLOGY_FILE of
// worldgen/tests/reference/p3_data_reference.py.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "worldgen/data/content.hpp"
#include "worldgen/data/schema.hpp"

namespace history_data {

// --- Races (P3-01 3.1) -----------------------------------------------------------

inline constexpr std::string_view ELF_TOML = R"([race.elf]
order = 1
name = "Elf"
plural = "Elves"
adjective = "Elven"
camp = "good"
awakening_share = 0
lifespan_years = 700
generation_years = 150
growth_permille = 3
site_weights = { fertility = 20, forage = 35, timber = 35, ore = 0, fresh_water = 10 }
biomes = [
  { biome = "temperate_forest", weight = 500 },
  { biome = "wet_woodland", weight = 300 },
  { biome = "grassland", weight = 0 },
  { biome = "heath", weight = -200 },
  { biome = "steppe", weight = -300 },
  { biome = "highland_moor", weight = -300 },
  { biome = "alpine", weight = -600 },
]
upland = -300
coast = 0
expansion = 200
aggression = 150
settlement_kinds = ["hamlet", "village", "town", "city"]
culture = "elven"
)";

inline constexpr std::string_view GOBLIN_TOML = R"([race.goblin]
order = 2
name = "Goblin"
plural = "Goblins"
adjective = "Goblin"
camp = "evil"
awakening_share = "0.35"
lifespan_years = 40
generation_years = 15
growth_permille = 20
site_weights = { fertility = 40, forage = 20, timber = 10, ore = 30, fresh_water = 0 }
biomes = [
  { biome = "heath", weight = 200 },
  { biome = "highland_moor", weight = 200 },
  { biome = "steppe", weight = 150 },
  { biome = "grassland", weight = 100 },
  { biome = "marsh", weight = 100 },
  { biome = "alpine", weight = -400 },
]
upland = 100
coast = -100
expansion = 600
aggression = 700
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "goblin"
)";

inline constexpr std::string_view DWARF_TOML = R"([race.dwarf]
order = 3
name = "Dwarf"
plural = "Dwarves"
adjective = "Dwarven"
camp = "neutral"
awakening_share = "0.55"
lifespan_years = 300
generation_years = 70
growth_permille = 5
site_weights = { fertility = 10, forage = 10, timber = 10, ore = 60, fresh_water = 10 }
biomes = [
  { biome = "alpine", weight = 300 },
  { biome = "highland_moor", weight = 300 },
  { biome = "heath", weight = 200 },
  { biome = "temperate_forest", weight = -100 },
  { biome = "wet_woodland", weight = -300 },
  { biome = "marsh", weight = -400 },
  { biome = "fen", weight = -400 },
]
upland = 800
coast = -200
expansion = 250
aggression = 300
settlement_kinds = ["hamlet", "village", "town", "city", "fortress"]
culture = "dwarven"
)";

inline constexpr std::string_view FAERIE_TOML = R"([race.faerie]
order = 4
name = "Faerie"
plural = "Faeries"
adjective = "Faerie"
camp = "good"
awakening_share = "0.70"
lifespan_years = 400
generation_years = 100
growth_permille = 4
site_weights = { fertility = 0, forage = 60, timber = 20, ore = 0, fresh_water = 20 }
biomes = [
  { biome = "fen", weight = 600 },
  { biome = "wet_woodland", weight = 600 },
  { biome = "marsh", weight = 400 },
  { biome = "temperate_forest", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "grassland", weight = -200 },
  { biome = "alpine", weight = -300 },
  { biome = "steppe", weight = -400 },
]
upland = 0
coast = 0
expansion = 100
aggression = 50
settlement_kinds = ["hamlet", "village"]
culture = "faerie"
)";

inline constexpr std::string_view HUMAN_TOML = R"([race.human]
order = 5
name = "Human"
plural = "Humans"
adjective = "Human"
camp = "neutral"
awakening_share = "0.83"
lifespan_years = 70
generation_years = 25
growth_permille = 15
site_weights = { fertility = 60, forage = 10, timber = 10, ore = 5, fresh_water = 15 }
biomes = [
  { biome = "grassland", weight = 300 },
  { biome = "steppe", weight = 100 },
  { biome = "temperate_forest", weight = 100 },
  { biome = "marsh", weight = -100 },
  { biome = "heath", weight = -100 },
  { biome = "fen", weight = -200 },
  { biome = "highland_moor", weight = -200 },
  { biome = "alpine", weight = -500 },
]
upland = -300
coast = 200
expansion = 700
aggression = 400
settlement_kinds = ["hamlet", "village", "town", "city", "fortress"]
culture = "human"
)";

inline constexpr std::string_view ORC_TOML = R"([race.orc]
order = 6
name = "Orc"
plural = "Orcs"
adjective = "Orcish"
camp = "evil"
awakening_share = "0.93"
lifespan_years = 50
generation_years = 18
growth_permille = 18
site_weights = { fertility = 35, forage = 25, timber = 10, ore = 30, fresh_water = 0 }
biomes = [
  { biome = "steppe", weight = 400 },
  { biome = "heath", weight = 200 },
  { biome = "grassland", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "temperate_forest", weight = -100 },
  { biome = "marsh", weight = -100 },
  { biome = "fen", weight = -200 },
  { biome = "wet_woodland", weight = -200 },
  { biome = "alpine", weight = -300 },
]
upland = 100
coast = -100
expansion = 600
aggression = 900
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "orcish"
)";

// --- Naming blocks (P3-02 5, verbatim) ---------------------------------------------

inline constexpr std::string_view DWARVEN_NAMING = R"(onsets = ["d", "b", "th", "k", "g", "br", "dr", "gr", "t", "h", "kh", "n", "m", "thr"]
nuclei = ["a", "u", "o", "i", "ai", "ou"]
codas = ["r", "n", "m", "k", "d", "rn", "rd", "rk", "mm", "nd", "rin", "li", "din"]
patterns = ["ONC", "ONCONC", "ONCONC", "ONCON"]
min_letters = 4
max_letters = 10
forbidden = ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk"]
god_forms = ["{root}"]
civilization_forms = ["The Hold of {root}", "Clan {root}", "The {root} Throne"]
settlement_forms = ["{root}", "{root} Deep", "{root} Hold", "{root}"]
)";

inline constexpr std::string_view ELVEN_NAMING = R"(onsets = ["", "l", "th", "s", "f", "n", "m", "r", "v", "gl", "c", "el", "ith"]
nuclei = ["a", "e", "i", "ae", "ie", "o"]
codas = ["", "", "l", "n", "th", "s", "r", "nd", "ll", "nn"]
patterns = ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"]
min_letters = 4
max_letters = 11
forbidden = ["aea", "iei", "iae", "eae", "aa", "ii", "ee", "thth", "lll", "nnn", "ea", "ia"]
god_forms = ["{root}"]
civilization_forms = ["The Court of {root}", "The Realm of {root}", "{root}"]
settlement_forms = ["{root}", "{root}", "{root}", "{root}dell"]
)";

inline constexpr std::string_view FAERIE_NAMING = R"(onsets = ["", "p", "f", "w", "l", "s", "t", "b", "sh", "wh", "fl", "tw", "m"]
nuclei = ["i", "ee", "ie", "ia", "a", "y", "ae", "ou", "o"]
codas = ["", "", "ll", "n", "sh", "th", "p", "sk", "ss"]
patterns = ["ONON", "ONONON", "ONC", "ONONC", "ONCON"]
min_letters = 3
max_letters = 10
forbidden = ["eee", "iee", "yy", "ouou", "shsh"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Ring", "The Circle of {root}", "The {root} Court"]
settlement_forms = ["{root}", "{root} Glade", "{root} Hollow", "{root}"]
)";

inline constexpr std::string_view GOBLIN_NAMING = R"(onsets = ["g", "gr", "sn", "k", "kr", "z", "sk", "b", "n", "gn", "dr", "sl", "skr"]
nuclei = ["a", "i", "u", "o", "ee"]
codas = ["k", "g", "b", "z", "t", "x", "nk", "rk", "gg", "zz", "tch"]
patterns = ["ONC", "ONC", "ONCONC", "ONC-ONC", "ONONC"]
min_letters = 3
max_letters = 10
forbidden = ["eee", "ggg", "zzz", "xg", "xk"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Horde", "The Warrens of {root}", "The {root} Mob"]
settlement_forms = ["{root}", "{root}", "{root} Pit", "{root} Hole"]
)";

inline constexpr std::string_view HUMAN_NAMING = R"(onsets = ["b", "c", "d", "g", "h", "m", "r", "s", "t", "w", "st", "br", "al", "ed", "ash"]
nuclei = ["a", "e", "i", "o", "u", "ea"]
codas = ["n", "d", "r", "s", "l", "rd", "ck", "ld", "m", "nt"]
patterns = ["ONC", "ONC", "ONCONC", "ONC"]
min_letters = 4
max_letters = 9
forbidden = ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm"]
god_forms = ["{root}"]
civilization_forms = ["The Kingdom of {root}", "The {root} League", "The Duchy of {root}"]
settlement_forms = ["{root}", "{root}ford", "{root}ton", "{root}bury", "{root}wick", "{root}ham", "{root}stead"]
)";

inline constexpr std::string_view ORCISH_NAMING = R"(onsets = ["gh", "gr", "ug", "m", "k", "z", "sh", "b", "d", "th", "kr", "ur", "az"]
nuclei = ["a", "u", "o", "aa", "uu"]
codas = ["g", "sh", "k", "rz", "z", "gh", "rk", "l", "m", "", "kh", "ld"]
patterns = ["ONC", "ONC", "ONC'ONC", "ONCONC", "NC'ONC"]
min_letters = 3
max_letters = 10
forbidden = ["aaa", "uuu", "ghgh", "shsh"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Horde", "Clan {root}", "The Warband of {root}"]
settlement_forms = ["{root}", "{root} Camp", "{root}", "{root} Fort"]
)";

// --- Chronology (P3-01 5.1) -------------------------------------------------------

inline constexpr std::string_view CHRONOLOGY_TOML = R"([history.chronology]
awakening_jitter_frac = "0.01"
awakening_min_gap_years = 100
age_name_form = "The Age of {plural}"
)";

// --- Culture files (P3-01 4) ------------------------------------------------------

struct CultureListing {
    std::string_view id, race, name, naming;
};

// Canonical id order (P0-03 7.4).
inline const std::vector<CultureListing>& culture_listings() {
    static const std::vector<CultureListing> v = {
        {"dwarven", "dwarf", "Dwarven", DWARVEN_NAMING}, {"elven", "elf", "Elven", ELVEN_NAMING},
        {"faerie", "faerie", "Faerie", FAERIE_NAMING},   {"goblin", "goblin", "Goblin", GOBLIN_NAMING},
        {"human", "human", "Human", HUMAN_NAMING},       {"orcish", "orc", "Orcish", ORCISH_NAMING},
    };
    return v;
}

// The file text of culture `id`: "[culture.<id>]", race, name, a blank line,
// "[culture.<id>.naming]" (line 5) and the naming block.
inline std::string culture_toml(const CultureListing& c) {
    return "[culture." + std::string(c.id) + "]\nrace = \"" + std::string(c.race) + "\"\nname = \"" +
           std::string(c.name) + "\"\n\n[culture." + std::string(c.id) + ".naming]\n" + std::string(c.naming);
}

// --- The files as a map: "<directory>/<file name>" -> text --------------------------

using Files = std::map<std::string, std::string>;

inline Files shipped_files() {
    Files f;
    f["races/dwarf.toml"] = std::string(DWARF_TOML);
    f["races/elf.toml"] = std::string(ELF_TOML);
    f["races/faerie.toml"] = std::string(FAERIE_TOML);
    f["races/goblin.toml"] = std::string(GOBLIN_TOML);
    f["races/human.toml"] = std::string(HUMAN_TOML);
    f["races/orc.toml"] = std::string(ORC_TOML);
    for (const CultureListing& c : culture_listings()) {
        f["cultures/" + std::string(c.id) + ".toml"] = culture_toml(c);
    }
    f["history/chronology.toml"] = std::string(CHRONOLOGY_TOML);
    return f;
}

// The kind whose directory holds a file of `files` ("races/x" -> "race").
inline std::string_view kind_of_directory(std::string_view directory) {
    if (directory == "races") return "race";
    if (directory == "cultures") return "culture";
    if (directory == "history") return "history";
    return {};
}

// The directories of `files`, each as one MemoryDirectory, sorted by name.
// A directory none of whose files is in `files` is not listed (missing).
inline std::vector<worldgen::data::MemoryDirectory> directories(const Files& files) {
    std::map<std::string, worldgen::data::MemoryDirectory> by_name;
    for (const auto& [path, text] : files) {
        const std::size_t slash = path.find('/');
        const std::string d = path.substr(0, slash);
        auto& dir = by_name[d];
        dir.name = d;
        dir.entries.push_back(worldgen::data::MemoryEntry{path.substr(slash + 1), text, false});
    }
    std::vector<worldgen::data::MemoryDirectory> out;
    for (auto& [name, dir] : by_name) {
        out.push_back(std::move(dir));
    }
    return out;
}

// --- The values of the listings, as a table (P3-01 8.1 test 1) ----------------------

struct RaceBiome {
    std::string_view biome;
    std::int64_t weight;
};

struct Race {
    std::string_view id;
    std::int64_t order;
    std::string_view name, plural, adjective, camp;
    std::int64_t awakening_share_raw;  // Fx raw (P0-01 5.6 of the decimal text)
    std::int64_t lifespan_years, generation_years, growth_permille;
    std::int64_t fertility, forage, timber, ore, fresh_water;  // site_weights
    std::vector<RaceBiome> biomes;
    std::int64_t upland, coast, expansion, aggression;
    std::vector<std::string_view> settlement_kinds;
    std::string_view culture;
};

// The six races of P3-01 3.1 in canonical id order. Raw values of the
// shares: "0.35" 1503238554, "0.55" 2362232013, "0.70" 3006477107, "0.83"
// 3564822856, "0.93" 3994319585 (core_reference.parse_decimal).
inline const std::vector<Race>& races() {
    static const std::vector<Race> v = {
        {"dwarf", 3, "Dwarf", "Dwarves", "Dwarven", "neutral", 2362232013, 300, 70, 5, 10, 10, 10, 60, 10,
         {{"alpine", 300}, {"highland_moor", 300}, {"heath", 200}, {"temperate_forest", -100}, {"wet_woodland", -300},
          {"marsh", -400}, {"fen", -400}},
         800, -200, 250, 300, {"hamlet", "village", "town", "city", "fortress"}, "dwarven"},
        {"elf", 1, "Elf", "Elves", "Elven", "good", 0, 700, 150, 3, 20, 35, 35, 0, 10,
         {{"temperate_forest", 500}, {"wet_woodland", 300}, {"grassland", 0}, {"heath", -200}, {"steppe", -300},
          {"highland_moor", -300}, {"alpine", -600}},
         -300, 0, 200, 150, {"hamlet", "village", "town", "city"}, "elven"},
        {"faerie", 4, "Faerie", "Faeries", "Faerie", "good", 3006477107, 400, 100, 4, 0, 60, 20, 0, 20,
         {{"fen", 600}, {"wet_woodland", 600}, {"marsh", 400}, {"temperate_forest", 200}, {"highland_moor", 100},
          {"grassland", -200}, {"alpine", -300}, {"steppe", -400}},
         0, 0, 100, 50, {"hamlet", "village"}, "faerie"},
        {"goblin", 2, "Goblin", "Goblins", "Goblin", "evil", 1503238554, 40, 15, 20, 40, 20, 10, 30, 0,
         {{"heath", 200}, {"highland_moor", 200}, {"steppe", 150}, {"grassland", 100}, {"marsh", 100}, {"alpine", -400}},
         100, -100, 600, 700, {"hamlet", "village", "town", "fortress"}, "goblin"},
        {"human", 5, "Human", "Humans", "Human", "neutral", 3564822856, 70, 25, 15, 60, 10, 10, 5, 15,
         {{"grassland", 300}, {"steppe", 100}, {"temperate_forest", 100}, {"marsh", -100}, {"heath", -100},
          {"fen", -200}, {"highland_moor", -200}, {"alpine", -500}},
         -300, 200, 700, 400, {"hamlet", "village", "town", "city", "fortress"}, "human"},
        {"orc", 6, "Orc", "Orcs", "Orcish", "evil", 3994319585, 50, 18, 18, 35, 25, 10, 30, 0,
         {{"steppe", 400}, {"heath", 200}, {"grassland", 200}, {"highland_moor", 100}, {"temperate_forest", -100},
          {"marsh", -100}, {"fen", -200}, {"wet_woodland", -200}, {"alpine", -300}},
         100, -100, 600, 900, {"hamlet", "village", "town", "fortress"}, "orcish"},
    };
    return v;
}

// Race ids in race order (design 5: elves 1 ... orcs 6).
inline const std::vector<std::string>& race_order() {
    static const std::vector<std::string> v = {"elf", "goblin", "dwarf", "faerie", "human", "orc"};
    return v;
}

// history.chronology (P3-01 5.1): "0.01" is raw 42949673.
inline constexpr std::int64_t CHRONOLOGY_JITTER_RAW = 42949673;
inline constexpr std::int64_t CHRONOLOGY_MIN_GAP_YEARS = 100;
inline constexpr std::string_view CHRONOLOGY_AGE_NAME_FORM = "The Age of {plural}";

// Field declaration orders (P3-01 3, 4, 5.1; P3-02 3).
inline const std::vector<std::string_view>& race_fields() {
    static const std::vector<std::string_view> v = {
        "order",           "name",      "plural", "adjective", "camp",      "awakening_share", "lifespan_years",
        "generation_years", "growth_permille", "site_weights", "biomes", "upland", "coast",          "expansion",
        "aggression",      "settlement_kinds", "culture"};
    return v;
}
inline const std::vector<std::string_view>& site_weight_fields() {
    static const std::vector<std::string_view> v = {"fertility", "forage", "timber", "ore", "fresh_water"};
    return v;
}
inline const std::vector<std::string_view>& culture_fields() {
    static const std::vector<std::string_view> v = {"race", "name", "naming"};
    return v;
}
inline const std::vector<std::string_view>& naming_fields() {
    static const std::vector<std::string_view> v = {"onsets",          "nuclei",      "codas",     "patterns",
                                                     "min_letters",     "max_letters", "forbidden", "god_forms",
                                                     "civilization_forms", "settlement_forms"};
    return v;
}
inline const std::vector<std::string_view>& chronology_fields() {
    static const std::vector<std::string_view> v = {"awakening_jitter_frac", "awakening_min_gap_years",
                                                    "age_name_form"};
    return v;
}

}  // namespace history_data
