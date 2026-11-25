#pragma once

// The 0.27.0 data of spec P5-06 r3 (4.1 to 4.4, 25.1, 25.3) at the calibrated values (15.2, the engineer's df29146),
// generated from the one listing of worldgen/tests/reference/p3_data_reference.py (FIGURES_RANGES_027, EPITHETS_027,
// RACE_FIELDS_027, CULTURE_FIELDS_027, SLAYER_EPITHET_027, UNDEAD_REGIONS_REF_027, MONSTER_CALIBRATED_027,
// MONSTERS_CALIBRATED_027) by worldgen/tests/reference/p5_06_figures_data_writer.py, never by hand. The tests compare
// the data files (game/data and both fixture roots) with these values.

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>

namespace p506data {

// 4.4: game/data/history/figures.toml as the spec states it.
inline constexpr std::string_view FIGURES_TOML = R"toml(# Figures: rulers, generals, heroes and casters (spec P5-06 4.4). Generator 0.27.0.
[history.figures]
name_rulers = true
reign_min_permille = 500
reign_span_permille = 1000
accession_age_permille = 250
accession_span_permille = 250
temper_strain_permille = { steady = 1000, warlike = 2000, peaceable = 0, grasping = 1000, just = 1000 }
temper_allied_permille = { steady = 1000, warlike = 1000, peaceable = 1000, grasping = 2000, just = 500 }
temper_spoils_permille = { steady = 0, warlike = 0, peaceable = 0, grasping = -400, just = 400 }
short_reign_permille = 500
short_reign_split_permille = 100
exile_permille = 250
exile_hops = 3
general_scale_permille = 850
skill_min = 500
skill_max = 800
career_permille = 300
fall_loser_permille = 120
fall_winner_permille = 25
general_age_permille = 300
general_age_span_permille = 300
hero_age_permille = 150
hero_age_span_permille = 200
life_min_permille = 700
life_span_permille = 500
hero_scale_permille = 1000
hero_hunt_permille = 20
hero_might_permille = 1000
hero_fall_permille = 300
caster_ppm = 1500
caster_age_permille = 400
caster_age_span_permille = 300
book_permille = 10
books_max = 3
blight_ppm = 500
blight_toll_permille = 80
regnal_max = 9
epithets = [
  { role = "ruler", deed = "conquests", min = 1, substantial = 2, text = "the Conqueror" },
  { role = "ruler", deed = "betrayals", min = 1, substantial = 1, text = "the Faithless" },
  { role = "ruler", deed = "realm_fell", min = 1, substantial = 0, text = "the Last" },
  { role = "ruler", deed = "wars_won", min = 2, substantial = 4, text = "the Victorious" },
  { role = "ruler", deed = "reign_permille", min = 1300, substantial = 1450, text = "the Old" },
  { role = "ruler", deed = "founded_years", min = 1, substantial = 3000, text = "the Founder" },
  { role = "general", deed = "battles_unbeaten", min = 3, substantial = 6, text = "the Unbeaten" },
  { role = "general", deed = "battles_won", min = 5, substantial = 10, text = "the Hammer" },
  { role = "hero", deed = "slew_might", min = 1, substantial = 1200, text = "{slayer}" },
  { role = "claimant", deed = "exiled", min = 1, substantial = 0, text = "the Exile" },
  { role = "caster", deed = "blights", min = 1, substantial = 1, text = "the Accursed" },
  { role = "caster", deed = "books", min = 3, substantial = 0, text = "the Learned" },
]
epithet_repeat_max = 2
)toml";

// 4.1: race -> temper_weights (steady, warlike, peaceable, grasping, just), general, hero, magic permille.
struct RaceFields {
    std::string_view race;
    std::array<std::int64_t, 5> temper;
    std::int64_t general, hero, magic;
};
inline constexpr std::array<RaceFields, 6> RACES = {{
    {"dwarf", {450, 150, 100, 200, 100}, 500, 120, 300},
    {"elf", {450, 100, 250, 50, 150}, 500, 100, 1000},
    {"faerie", {400, 50, 350, 50, 150}, 150, 60, 900},
    {"goblin", {300, 300, 50, 300, 50}, 250, 40, 150},
    {"human", {400, 175, 175, 125, 125}, 450, 150, 500},
    {"orc", {250, 500, 50, 150, 50}, 400, 80, 150},
}};

// 4.2: culture -> regnal_permille (person_forms ["{root}"] in every culture).
struct CultureFields {
    std::string_view culture;
    std::int64_t regnal_permille;
    std::string_view tower_form0, library_form0, title0;
};
inline constexpr std::array<CultureFields, 6> CULTURES = {{
    {"dwarven", 200, "The Rune-Tower of {root}", "The Rune-Halls of {root}", "The Deep Reckoning"},
    {"elven", 0, "The Star-Tower of {root}", "The Leaf-House of {root}", "The Lay of the First Stars"},
    {"faerie", 0, "The Thorn-Tower of {root}", "The Dream-Hollow of {root}", "The Book of Glimmers"},
    {"goblin", 0, "The Smoke-Tower of {root}", "The Scratching-House of {root}", "The Black Scratchings"},
    {"human", 250, "The Tower of {root}", "The Library of {root}", "The Codex of Hours"},
    {"orcish", 0, "The Skull-Tower of {root}", "The Bone-Hall of {root}", "The Skull-Songs"},
}};

// 4.3: monster kind -> slayer_epithet.
struct Slayer {
    std::string_view kind, epithet;
};
inline constexpr std::array<Slayer, 6> SLAYERS = {{
    {"dragon", "Dragonslayer"},
    {"giant", "Giantsbane"},
    {"ogre", "Ogrebane"},
    {"troll", "Trollbane"},
    {"undead", "the Gravewarden"},
    {"wyvern", "Wyrmbane"},
}};

// 25.3: history.monsters.undead_regions_ref; 25.1: the two history.peoples fields removed.
inline constexpr std::int64_t UNDEAD_REGIONS_REF = 64;
inline constexpr std::array<std::string_view, 2> PEOPLES_REMOVED = {"ruin_fade_years", "ghost_fade_years"};

// The files of the 0.27.0 kinds race, culture, monster and history (p3_data_reference.SPEC_FILES_027), as the
// spec states them: "<directory>/<file name>" -> text. Literals are cut in pieces of at most 4000 bytes (MSVC's
// limit on one string literal).
inline std::map<std::string, std::string> files_027() {
    std::map<std::string, std::string> f;
    f["races/dwarf.toml"] = std::string(R"p506([race.dwarf]
order = 3
name = "Dwarf"
plural = "Dwarves"
adjective = "Dwarven"
camp = "neutral"
awakening_share = "0.55"
lifespan_years = 300
generation_years = 70
growth_permille = 5
density_permille = 700
site_weights = { fertility = 10, forage = 10, timber = 10, ore = 60, fresh_water = 10 }
cell_weights = { ore = 400, height = 600, inland = 0, port = 0, habitability = 500 }
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
prowess_permille = 1400
muster_permille = 1000
conceal_min = 1001
home_defense_permille = 0
odds_min_permille = 700
awakening_quarrel = 0
hidden_years = 300
awakening_population = 0
settlement_kinds = ["hamlet", "village", "town", "city", "fortress"]
culture = "dwarven"
stances = [{ race = "goblin", value = -100 }]
origin_allies = []
ally_join_permille = 800
mining_permille = 1000
temper_weights = { steady = 450, warlike = 150, peaceable = 100, grasping = 200, just = 100 }
general_permille = 500
hero_permille = 120
magic_permille = 300
)p506");
    f["races/elf.toml"] = std::string(R"p506([race.elf]
order = 1
name = "Elf"
plural = "Elves"
adjective = "Elven"
camp = "good"
awakening_share = 0
lifespan_years = 700
generation_years = 150
growth_permille = 3
density_permille = 300
site_weights = { fertility = 20, forage = 35, timber = 35, ore = 0, fresh_water = 10 }
cell_weights = { ore = 0, height = 0, inland = 0, port = 1000, habitability = 1000 }
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
prowess_permille = 1600
muster_permille = 1000
conceal_min = 1001
home_defense_permille = 1500
odds_min_permille = 700
awakening_quarrel = 0
hidden_years = 300
awakening_population = 0
settlement_kinds = ["hamlet", "village", "town", "city"]
culture = "elven"
stances = [{ race = "faerie", value = 200 }]
origin_allies = ["faerie"]
ally_join_permille = 700
mining_permille = 0
temper_weights = { steady = 450, warlike = 100, peaceable = 250, grasping = 50, just = 150 }
general_permille = 500
hero_permille = 100
magic_permille = 1000
)p506");
    f["races/faerie.toml"] = std::string(R"p506([race.faerie]
order = 4
name = "Faerie"
plural = "Faeries"
adjective = "Faerie"
camp = "good"
awakening_share = "0.70"
lifespan_years = 400
generation_years = 100
growth_permille = 4
density_permille = 250
site_weights = { fertility = 0, forage = 60, timber = 20, ore = 0, fresh_water = 20 }
cell_weights = { ore = 0, height = 0, inland = 400, port = 0, habitability = 1000 }
biomes = [
  { biome = "temperate_forest", weight = 800 },
  { biome = "wet_woodland", weight = 800 },
  { biome = "fen", weight = 300 },
  { biome = "marsh", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "heath", weight = 0 },
  { biome = "grassland", weight = -300 },
  { biome = "alpine", weight = -400 },
  { biome = "steppe", weight = -400 },
]
upland = 200
coast = -500
expansion = 100
aggression = 50
prowess_permille = 1200
muster_permille = 1000
conceal_min = 400
home_defense_permille = 0
odds_min_permille = 700
awakening_quarrel = 0
hidden_years = 300
awakening_population = 0
settlement_kinds = ["hamlet", "village"]
culture = "faerie"
stances = [{ race = "elf", value = 200 }]
origin_allies = ["elf"]
ally_join_permille = 150
mining_permille = 0
temper_weights = { steady = 400, warlike = 50, peaceable = 350, grasping = 50, just = 150 }
general_permille = 150
hero_permille = 60
magic_permille = 900
)p506");
    f["races/goblin.toml"] = std::string(R"p506([race.goblin]
order = 2
name = "Goblin"
plural = "Goblins"
adjective = "Goblin"
camp = "evil"
awakening_share = "0.35"
lifespan_years = 40
generation_years = 15
growth_permille = 20
density_permille = 900
site_weights = { fertility = 40, forage = 20, timber = 10, ore = 30, fresh_water = 0 }
cell_weights = { ore = 0, height = 0, inland = 0, port = 1000, habitability = 1000 }
biomes = [
  { biome = "heath", weight = 200 },
  { biome = "highland_moor", weight = 200 },
  { biome = "steppe", weight = 150 },
  { biome = "grassland", weight = 100 },
  { biome = "marsh", weight = 100 },
  { biome = "alpine", weight = -400 },
  { biome = "temperate_forest", weight = -200 },
  { biome = "wet_woodland", weight = -300 },
]
upland = 100
coast = -100
expansion = 600
aggression = 600
prowess_permille = 800
muster_permille = 1000
conceal_min = 1001
home_defense_permille = 0
odds_min_permille = 700
awakening_quarrel = 0
hidden_years = 300
awakening_population = 0
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "goblin"
stances = [{ race = "dwarf", value = -100 }]
origin_allies = []
ally_join_permille = 400
mining_permille = 0
temper_weights = { steady = 300, warlike = 300, peaceable = 50, grasping = 300, just = 50 }
general_permille = 250
hero_permille = 40
magic_permille = 150
)p506");
    f["races/human.toml"] = std::string(R"p506([race.human]
order = 5
name = "Human"
plural = "Humans"
adjective = "Human"
camp = "neutral"
awakening_share = "0.83"
lifespan_years = 70
generation_years = 25
growth_permille = 15
density_permille = 1000
site_weights = { fertility = 60, forage = 10, timber = 10, ore = 5, fresh_water = 15 }
cell_weights = { ore = 0, height = 0, inland = 0, port = 1000, habitability = 1000 }
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
prowess_permille = 1000
muster_permille = 1000
conceal_min = 1001
home_defense_permille = 0
odds_min_permille = 700
awakening_quarrel = 0
hidden_years = 300
awakening_population = 0
settlement_kinds = ["hamlet", "village", "town", "city", "fortress"]
culture = "human"
stances = []
origin_allies = []
ally_join_permille = 600
mining_permille = 0
temper_weights = { steady = 400, warlike = 175, peaceable = 175, grasping = 125, just = 125 }
general_permille = 450
hero_permille = 150
magic_permille = 500
)p506");
    f["races/orc.toml"] = std::string(R"p506([race.orc]
order = 6
name = "Orc"
plural = "Orcs"
adjective = "Orcish"
camp = "evil"
awakening_share = "0.93"
lifespan_years = 50
generation_years = 18
growth_permille = 18
density_permille = 800
site_weights = { fertility = 35, forage = 25, timber = 10, ore = 30, fresh_water = 0 }
cell_weights = { ore = 0, height = 0, inland = 0, port = 1000, habitability = 1000 }
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
prowess_permille = 1300
muster_permille = 4000
conceal_min = 1001
home_defense_permille = 0
odds_min_permille = 500
awakening_quarrel = 1
hidden_years = 100
awakening_population = 1200
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "orcish"
stances = []
origin_allies = []
ally_join_permille = 600
mining_permille = 0
temper_weights = { steady = 250, warlike = 500, peaceable = 50, grasping = 150, just = 50 }
general_permille = 400
hero_permille = 80
magic_permille = 150
)p506");
    f["cultures/dwarven.toml"] = std::string(R"p506([culture.dwarven]
race = "dwarf"
name = "Dwarven"

[culture.dwarven.naming]
onsets = ["d", "b", "th", "k", "g", "br", "dr", "gr", "t", "h", "kh", "n", "m", "thr"]
nuclei = ["a", "u", "o", "i", "ai", "ou"]
codas = ["r", "n", "m", "k", "d", "rn", "rd", "rk", "mm", "nd", "rin", "li", "din"]
patterns = ["ONC", "ONCONC", "ONCONC", "ONCON"]
god_patterns = ["ONCONC", "ONCONC", "ONCON"]
min_letters = 4
max_letters = 10
forbidden = ["mmm", "rr", "dd", "kk", "rdt", "rkd", "rdd", "mmb", "kdr", "kthr", "dkh", "ndb", "rnk", "didin", "ririn"]
god_forms = ["{root}"]
civilization_forms = ["The Hold of {root}", "Clan {root}", "The {root} Throne"]
settlement_forms = ["{root}", "{root} Deep", "{root} Hold", "{root}"]
site_forms = ["{root} Watch", "The Tower of {root}"]
person_forms = ["{root}"]
tower_forms = ["The Rune-Tower of {root}", "{root}'s Delving-Spire"]
library_forms = ["The Rune-Halls of {root}"]
book_titles = ["The Deep Reckoning", "On the Hearts of Stones", "The Rune-Book of the Forge", "The Iron Annals", "On the Binding of Runes", "The Book of Seams", "The Hammer-Canon", "The Grey Ledger"]
regnal_permille = 200
)p506");
    f["cultures/elven.toml"] = std::string(R"p506([culture.elven]
race = "elf"
name = "Elven"

[culture.elven.naming]
onsets = ["", "l", "th", "s", "f", "n", "m", "r", "v", "gl", "c", "el", "ith"]
nuclei = ["a", "e", "i", "ae", "ie", "o"]
codas = ["", "", "l", "n", "th", "s", "r", "nd", "ll", "nn"]
patterns = ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"]
god_patterns = ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"]
min_letters = 4
max_letters = 11
forbidden = ["aea", "iei", "iae", "eae", "aa", "ii", "ee", "thth", "lll", "nnn", "ea", "ia"]
god_forms = ["{root}"]
civilization_forms = ["The Court of {root}", "The Realm of {root}", "The House of {root}"]
settlement_forms = ["{root}", "{root}", "{root}", "{root}dell"]
site_forms = ["{root} Watch", "The Spire of {root}"]
person_forms = ["{root}"]
tower_forms = ["The Star-Tower of {root}", "{root}'s White Tower"]
library_forms = ["The Leaf-House of {root}"]
book_titles = ["The Lay of the First Stars", "On the Speech of Trees", "The Book of Falling Leaves", "The Silver Canticle", "On Starlight and Shadow", "The Long Remembering", "The Song of the Waking", "The Mirror of Waters"]
regnal_permille = 0
)p506");
    f["cultures/faerie.toml"] = std::string(R"p506([culture.faerie]
race = "faerie"
name = "Faerie"

[culture.faerie.naming]
onsets = ["", "v", "f", "w", "l", "s", "t", "gl", "sh", "wh", "fl", "tw", "m"]
nuclei = ["i", "i", "ee", "ie", "ie", "ia", "ia", "a", "a", "y", "y", "ae", "ae", "ou", "o", "o"]
codas = ["", "", "ll", "n", "sh", "th", "sk", "ss"]
patterns = ["ONON", "ONONON", "ONC", "ONONC", "ONCON"]
god_patterns = ["ONON", "ONONON", "ONONC", "ONCON"]
min_letters = 3
max_letters = 10
forbidden = ["eee", "iee", "yy", "ouou", "shsh", "ass"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Ring", "The Circle of {root}", "The {root} Court"]
settlement_forms = ["{root}", "{root} Glade", "{root} Hollow", "{root}"]
site_forms = ["{root} Watch", "The Thorn of {root}"]
person_forms = ["{root}"]
tower_forms = ["The Thorn-Tower of {root}", "{root}'s Moonspire"]
library_forms = ["The Dream-Hollow of {root}"]
book_titles = ["The Book of Glimmers", "On the Dreams of Moths", "The Dew-Book", "The Little Mysteries", "On Hidden Ways", "The Thistle Pages", "The Riddle of Hollows", "The Book of Moonmilk"]
regnal_permille = 0
)p506");
    f["cultures/goblin.toml"] = std::string(R"p506([culture.goblin]
race = "goblin"
name = "Goblin"

[culture.goblin.naming]
onsets = ["g", "gr", "sn", "k", "kr", "z", "sk", "b", "n", "gn", "dr", "sl", "skr"]
nuclei = ["a", "i", "u", "o", "ee"]
codas = ["k", "g", "b", "z", "t", "x", "nk", "rk", "gg", "zz", "tch"]
patterns = ["ONC", "ONC", "ONCONC", "ONC-ONC", "ONONC"]
god_patterns = ["ONCONC", "ONC-ONC", "ONONC"]
min_letters = 3
max_letters = 10
forbidden = ["eee", "ggg", "zzz", "xg", "xk"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Horde", "The Warrens of {root}", "The {root} Mob"]
settlement_forms = ["{root}", "{root}", "{root} Pit", "{root} Hole"]
site_forms = ["{root} Lookout", "{root} Stockade"]
person_forms = ["{root}"]
tower_forms = ["The Smoke-Tower of {root}", "{root}'s Bone-Tower"]
library_forms = ["The Scratching-House of {root}"]
book_titles = ["The Black Scratchings", "On Biting Things", "The Book of Smokes", "The Gnawed Pages", "On the Fears of Elves", "The Rot-Book", "The Twelve Curses", "The Book of Spite"]
regnal_permille = 0
)p506");
    f["cultures/human.toml"] = std::string(R"p506([culture.human]
race = "human"
name = "Human"

[culture.human.naming]
onsets = ["b", "c", "d", "g", "h", "m", "r", "s", "t", "w", "st", "br", "al", "ed", "ash"]
nuclei = ["a", "e", "i", "o", "u", "ea"]
codas = ["n", "d", "r", "s", "l", "rd", "ck", "ld", "m", "nt"]
patterns = ["ONC", "ONC", "ONCONC", "ONC"]
god_patterns = ["ONCONC"]
min_letters = 4
max_letters = 9
forbidden = ["eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm", "ass"]
god_forms = ["{root}"]
civilization_forms = ["The Kingdom of {root}", "The {root} League", "The Duchy of {root}"]
settlement_forms = ["{root}", "{root}ford", "{root}ton", "{root}bury", "{root}wick", "{root}ham", "{root}stead"]
site_forms = ["{root} Watch", "{root} Tower"]
person_forms = ["{root}"]
tower_forms = ["The Tower of {root}", "{root}'s Tower"]
library_forms = ["The Library of {root}"]
book_titles = ["The Codex of Hours", "On the Nature of Fire", "The Grey Grimoire", "The Book of Wards", "On the Turning of the Stars", "The Treatise of Bindings", "The Book of Keys", "The Ember Codex"]
regnal_permille = 250
)p506");
    f["cultures/orcish.toml"] = std::string(R"p506([culture.orcish]
race = "orc"
name = "Orcish"

[culture.orcish.naming]
onsets = ["gh", "gr", "ug", "m", "k", "z", "sh", "b", "d", "th", "kr", "ur", "az"]
nuclei = ["a", "u", "o", "aa", "uu"]
codas = ["g", "sh", "k", "rz", "z", "gh", "rk", "l", "m", "", "kh", "ld"]
patterns = ["ONC", "ONC", "ONC'ONC", "ONCONC", "NC'ONC"]
god_patterns = ["ONC'ONC", "ONCONC", "NC'ONC"]
min_letters = 3
max_letters = 10
forbidden = ["aaa", "uuu", "ghgh", "shsh"]
god_forms = ["{root}"]
civilization_forms = ["The {root} Horde", "Clan {root}", "The Warband of {root}"]
settlement_forms = ["{root}", "{root} Camp", "{root}", "{root} Fort"]
site_forms = ["{root} Stockade", "The Watch of {root}"]
person_forms = ["{root}"]
tower_forms = ["The Skull-Tower of {root}", "{root}'s War-Spire"]
library_forms = ["The Bone-Hall of {root}"]
book_titles = ["The Skull-Songs", "On Blood and Iron", "The Book of the Storm", "The Bone-Tally", "The Red Chants", "The War-Runes", "The Ash-Book", "The Chant of Ruin"]
regnal_permille = 0
)p506");
    f["monsters/dragon.toml"] = std::string(R"p506([monster.dragon]
name = "Dragon"
noun = "a dragon of the old world"
grouped = false
wake = "wild"
biomes = [
  { biome = "alpine", weight = 800 },
  { biome = "highland_moor", weight = 400 },
  { biome = "blighted_waste", weight = 600 },
]
wake_rate = 60
naming_culture = "elven"
name_forms = ["{root} the Black", "{root} the Pale", "{root} the Red", "{root} the Old", "{root} Ashwing", "{root} the Gold-Eyed", "{root}"]
lair_forms = ["The Lair of {root}", "{root}'s Deep"]
might = 6000
range_hops = 2
raid_permille = 60
toll_min_permille = 50
toll_max_permille = 200
kill_max = 600
overruns = ["hamlet", "village", "town"]
overrun_permille = 600
active_years = 40
rewake_ppm = 700
lifespan_years = 0
max_alive = 1
flare = true
hoard_permille = 40
raid_phrases = ["fell in fire upon", "burned the fields of", "came down in flame on"]
slayer_epithet = "Dragonslayer"
)p506");
    f["monsters/giant.toml"] = std::string(R"p506([monster.giant]
name = "Giant"
noun = "a giant of the mountains"
grouped = false
wake = "wild"
biomes = [
  { biome = "alpine", weight = 900 },
  { biome = "highland_moor", weight = 500 },
]
wake_rate = 25
naming_culture = "dwarven"
name_forms = ["{root} Stonehand", "{root} the Tall", "{root} Cliffbreaker", "{root} the Grey", "{root}"]
lair_forms = ["{root}'s Seat", "The Hall of {root}"]
might = 1200
range_hops = 1
raid_permille = 100
toll_min_permille = 30
toll_max_permille = 100
kill_max = 150
overruns = ["hamlet", "village"]
overrun_permille = 1000
active_years = 150
rewake_ppm = 1000
lifespan_years = 4000
max_alive = 2
flare = true
hoard_permille = 8
raid_phrases = ["came down from the heights upon", "hurled stones upon", "fell upon"]
slayer_epithet = "Giantsbane"
)p506");
    f["monsters/ogre.toml"] = std::string(R"p506([monster.ogre]
name = "Ogre"
noun = "an ogre"
grouped = false
wake = "wild"
biomes = [
  { biome = "grassland", weight = 400 },
  { biome = "heath", weight = 600 },
  { biome = "steppe", weight = 700 },
  { biome = "highland_moor", weight = 300 },
]
wake_rate = 40
naming_culture = "orcish"
name_forms = ["{root} the Glutton", "{root} the Gaunt", "{root} the Hungering", "{root} Bonebreaker", "{root} the Fat", "{root}"]
lair_forms = ["{root}'s Larder", "The Pit of {root}"]
might = 200
range_hops = 1
raid_permille = 150
toll_min_permille = 20
toll_max_permille = 60
kill_max = 40
overruns = ["hamlet"]
overrun_permille = 1000
active_years = 80
rewake_ppm = 2000
lifespan_years = 2000
max_alive = 3
flare = false
hoard_permille = 3
raid_phrases = ["fell upon", "plundered the stores of", "carried off the herds of"]
slayer_epithet = "Ogrebane"
)p506");
    f["monsters/troll.toml"] = std::string(R"p506([monster.troll]
name = "Troll"
noun = "a troll of the hills"
grouped = false
wake = "wild"
biomes = [
  { biome = "highland_moor", weight = 800 },
  { biome = "alpine", weight = 600 },
  { biome = "temperate_forest", weight = 400 },
  { biome = "heath", weight = 300 },
]
wake_rate = 50
naming_culture = "goblin"
name_forms = ["Old {root}", "{root} the Grey", "{root} Stonehide", "{root} of the Fells", "Mossback {root}", "{root}"]
lair_forms = ["{root}'s Hollow", "The Den of {root}"]
might = 300
range_hops = 1
raid_permille = 150
toll_min_permille = 20
toll_max_permille = 80
kill_max = 60
overruns = ["hamlet", "village"]
overrun_permille = 1000
active_years = 120
rewake_ppm = 1500
lifespan_years = 4000
max_alive = 3
flare = false
hoard_permille = 5
raid_phrases = ["came down by night on", "fell upon", "prowled the edges of"]
slayer_epithet = "Trollbane"
)p506");
    f["monsters/undead.toml"] = std::string(R"p506([monster.undead]
name = "Undead"
noun = "a host of the restless dead"
grouped = true
wake = "dead"
biomes = []
wake_rate = 25000
naming_culture = "human"
name_forms = ["The Host of {place}", "The Restless of {place}", "The Dead of {place}"]
lair_forms = ["The Barrows of {root}", "The Howes of {root}"]
might = 1500
range_hops = 1
raid_permille = 80
toll_min_permille = 30
toll_max_permille = 120
kill_max = 200
overruns = ["hamlet", "village"]
overrun_permille = 1000
active_years = 200
rewake_ppm = 3000
lifespan_years = 5000
max_alive = 5
flare = false
hoard_permille = 0
raid_phrases = ["came out of the barrows against", "walked by night in", "crept into"]
slayer_epithet = "the Gravewarden"
)p506");
    f["monsters/wyvern.toml"] = std::string(R"p506([monster.wyvern]
name = "Wyvern"
noun = "a brood of wyverns"
grouped = true
wake = "wild"
biomes = [
  { biome = "alpine", weight = 700 },
  { biome = "heath", weight = 500 },
  { biome = "highland_moor", weight = 600 },
]
wake_rate = 50
naming_culture = "goblin"
name_forms = ["The {root} Brood", "The Brood of {root}", "The Screamers of {root}"]
lair_forms = ["The Eyrie of {root}", "{root}'s Crag"]
might = 600
range_hops = 2
raid_permille = 120
toll_min_permille = 20
toll_max_permille = 80
kill_max = 50
overruns = ["hamlet"]
overrun_permille = 1000
active_years = 60
rewake_ppm = 2500
lifespan_years = 2000
max_alive = 2
flare = false
hoard_permille = 0
raid_phrases = ["stooped on", "swept down on", "came screaming down on"]
slayer_epithet = "Wyrmbane"
)p506");
    f["history/alliances.toml"] = std::string(R"p506(# Alliances, trade, the dwarves' mines and arms (spec P5-04 4.2). Generator 0.26.0.
[history.alliances]
ally_hops = 3
ally_min_disposition = -300
ally_full_disposition = 150
ally_friend_disposition = 150
ally_break_disposition = -350
allies_max = 3
ally_after_war_years = 100
foe_years = 100
fear_share = 300
form_foe_permille = 40
form_fear_permille = 30
form_trade_permille = 20
form_friend_permille = 10
form_max_permille = 60
bloc_share_max = 500
bloc_lapse_share = 700
term_years = 150
refusals_to_break = 2
defend_permille = 1000
attack_permille = 500
hostility_full = 500
ally_wars_max = 3
ally_memory = 30
renew_memory = 15
join_memory = 30
refuse_memory = 40
betray_memory = 150
ally_war_memory = 75
trade_hops = 3
trade_min_disposition = -250
trade_break_margin = 100
trade_after_war_years = 50
trade_permille = 15
trade_max = 4
trade_credit_years = 50
trade_memory = 20
trade_ally_years = 100
spoils_plunder_permille = 250
spoils_floor_permille = 100
spoils_cap_permille = 400
spoils_grant_permille = 350
spoils_warmth_permille = 300
spoils_part_permille = 500
spoils_need_permille = 400
spoils_grant_min = 100
spoils_grant_max = 950
share_memory = 25
withheld_memory = 100
withheld_grudge = 400
withheld_to_break = 1
mine_permille = 40
mine_ore_min = 1
mine_settlement_min = 200
arms_trade_permille = 2000
mine_forms = ["The Mines of {settlement}", "The Delvings of {settlement}"]
)p506");
    f["history/chronology.toml"] = std::string(R"p506([history.chronology]
awakening_jitter_frac = "0.01"
awakening_min_gap_years = 100
age_name_form = "The Age of {plural}"
)p506");
    f["history/conflict.toml"] = std::string(R"p506([history.conflict]
camp = { good_good = 400, good_neutral = -150, good_evil = -700, neutral_neutral = 100, neutral_evil = -250, evil_evil = 300 }
enmity_max = -500
border_friction = 520
want_weight = 500
hunger_full = 300
fear_share = 400
fear_weight = 1000
tick_strain_permille = 25
quiet_strain_permille = 6
strain_max = 5000
claim_strain = 300
declare_min = 300
declare_span = 600
declare_permille = 150
declare_ratio = 1000
shelter_years = 150
march_hops = 2
cost_penalty = 100
levy_permille = { hamlet = 100, village = 100, town = 100, city = 80, fortress = 250 }
terrain_defense = 1000
pass_defense = 1000
river_defense = 500
fort_defense = 500
battle_permille = 300
loss_loser_permille = 250
loss_winner_permille = 80
garrison_permille = 300
walls_permille = { hamlet = 1000, village = 1500, town = 2500, city = 4000, fortress = 6000 }
capital_walls = 2000
sack_base = 200
sack_hostility = 1000
sack_toll_permille = 300
capture_toll_permille = 100
exhaustion_permille = 80
war_max_years = 40
truce_years = 200
war_grudge = 150
capture_grudge = 50
secession_grudge = 300
grudge_decay_per_century = 50
last_refuge = 1
refuge_flight = 1
flight_hops = 3
weary_wars = 3
tribute_permille = 100
revenge_aggression = 400
levy_hops = 3
odds_floor = 500
truce_break_aggression = 600
enmity_hops = 3
kill_max_permille = 500
find_permille = 250
search_years = 3
search_weary_years = 500
search_repeat_salience = 15
quarrel_strain = 600
quarrel_min_settlements = 2
quarrel_strength_permille = 1000
toll_spread_permille = 200
capital_sacked_salience = 90
capital_captured_salience = 80
allied_strain_permille = 160
ally_deterrence_permille = 50
ally_effort_permille = 1000
trade_strength_permille = 50
trade_strength_max = 3
forge_strength_permille = 50
arms_strength_permille = 100
)p506");
    f["history/engine.toml"] = std::string(R"p506([history.engine]
quiet_step_years = 25
quiet_events_max = 3
quiet_min_years = 50
flare_min_years = 10
simulated_min_permille = 100
simulated_max_permille = 450
islet_max_dry_cells = 1172
sparse = { active_years = 75, flare_years = 30, final_years = 250, flare_threshold = 1500 }
normal = { active_years = 150, flare_years = 50, final_years = 300, flare_threshold = 1000 }
dense = { active_years = 250, flare_years = 75, final_years = 400, flare_threshold = 700 }
)p506");
    f["history/figures.toml"] = std::string(R"p506(# Figures: rulers, generals, heroes and casters (spec P5-06 4.4). Generator 0.27.0.
[history.figures]
name_rulers = true
reign_min_permille = 500
reign_span_permille = 1000
accession_age_permille = 250
accession_span_permille = 250
temper_strain_permille = { steady = 1000, warlike = 2000, peaceable = 0, grasping = 1000, just = 1000 }
temper_allied_permille = { steady = 1000, warlike = 1000, peaceable = 1000, grasping = 2000, just = 500 }
temper_spoils_permille = { steady = 0, warlike = 0, peaceable = 0, grasping = -400, just = 400 }
short_reign_permille = 500
short_reign_split_permille = 100
exile_permille = 250
exile_hops = 3
general_scale_permille = 850
skill_min = 500
skill_max = 800
career_permille = 300
fall_loser_permille = 120
fall_winner_permille = 25
general_age_permille = 300
general_age_span_permille = 300
hero_age_permille = 150
hero_age_span_permille = 200
life_min_permille = 700
life_span_permille = 500
hero_scale_permille = 1000
hero_hunt_permille = 20
hero_might_permille = 1000
hero_fall_permille = 300
caster_ppm = 1500
caster_age_permille = 400
caster_age_span_permille = 300
book_permille = 10
books_max = 3
blight_ppm = 500
blight_toll_permille = 80
regnal_max = 9
epithets = [
  { role = "ruler", deed = "conquests", min = 1, substantial = 2, text = "the Conqueror" },
  { role = "ruler", deed = "betrayals", min = 1, substantial = 1, text = "the Faithless" },
  { role = "ruler", deed = "realm_fell", min = 1, substantial = 0, text = "the Last" },
  { role = "ruler", deed = "wars_won", min = 2, substantial = 4, text = "the Victorious" },
  { role = "ruler", deed = "reign_permille", min = 1300, substantial = 1450, text = "the Old" },
  { role = "ruler", deed = "founded_years", min = 1, substantial = 3000, text = "the Founder" },
  { role = "general", deed = "battles_unbeaten", min = 3, substantial = 6, text = "the Unbeaten" },
  { role = "general", deed = "battles_won", min = 5, substantial = 10, text = "the Hammer" },
  { role = "hero", deed = "slew_might", min = 1, substantial = 1200, text = "{slayer}" },
  { role = "claimant", deed = "exiled", min = 1, substantial = 0, text = "the Exile" },
  { role = "caster", deed = "blights", min = 1, substantial = 1, text = "the Accursed" },
  { role = "caster", deed = "books", min = 3, substantial = 0, text = "the Learned" },
]
epithet_repeat_max = 2
)p506");
    f["history/monsters.toml"] = std::string(R"p506(# Beasts and monsters (spec P5-02 4.3; P5-03 9.4; P5-03a 10.2). Generator 0.25.0.
[history.monsters]
pressure = { low = 500, normal = 1000, high = 1600 }
living_permille = 200
undead_min_dead = 1000
undead_window_years = 100
hunt_hops = 2
hunt_levy_permille = 50
hunt_permille = 20
hunt_memory_years = 30
slay_max_permille = 900
drive_permille = 300
drive_years = 100
displace_per_kpeople = 100
displace_max_permille = 900
beast_decline_permille = 200
people_floor = 100
drag_k = 40
drag_max_permille = 100
feed_k = 2
feed_max_permille = 100
bold_on_permille = 1500
bold_off_permille = 800
bold_permille = 100
bold_toll_permille = 10
outpost_permille = 30
outpost_max = 2
outpost_quiet_years = 200
prey_weight = { hamlet = 1000, village = 600, town = 150, city = 40, fortress = 20 }
prey_capital_permille = 250
prey_hop_permille = 500
prey_might_ref = 1500
prey_might_max_permille = 4000
raid_chain_years = 300
toll_over_permille = 250
hunt_raid_permille = 5
hunt_max_permille = 200
fear_raids = 2
fear_years = 50
fear_permille = 400
overrun_people_ref = 3000
undead_dead_permille = 28
undead_regions_ref = 64
)p506");
    f["history/peoples.toml"] = std::string(R"p506([history.peoples]
score_site = 1000
score_biome = 1000
score_upland = 1000
score_coast = 1000
ore_reach_full = 200
coast_full = 200
rough_base = 150
rough_upland = 600
people_per_kcell = 2500
capacity_floor = 400
capital_weight = 3000
founding_population = 300
village_min = 250
town_min = 1500
city_min = 8000
city_max = 25000
fortress_max = 6000
kind_shrink_permille = 800
site_spacing = 32
region_slots_max = 4
mouth_class_min = 4
port_radius = 3
port_mouth_bonus = 60
port_coast_bonus = 20
decline_permille = 100
recover_permille = 900
colonist_permille = 120
colonists_min = 60
abandon_min = 40
abandon_frail_max = 1200
abandon_permille = { hamlet = 300, village = 250, town = 50, city = 0, fortress = 0 }
found_pressure_permille = 750
found_rate_permille = 100
quiet_found_permille = 500
reach_base = 1
reach_per_expansion = 3
expand_min_score = 250
expand_relative_permille = 700
expand_min_capacity = 800
cost_penalty = 100
pass_cost = 3
river_cost = 2
river_border_share_permille = 500
civs_two_permille = 500
regions_per_civ = 4
regions_per_extra_civ = 90
civs_extra_max = 2
cause_memory_years = 50
inland_cap = 20
home_min_hops = 3
home_min_capacity = 1500
intensity = { calm = 500, normal = 1000, turbulent = 2000 }
famine_crowding_permille = 900
famine_ppm = 1250
famine_toll_min = 50
famine_toll_max = 200
plague_ppm = { hamlet = 300, village = 600, town = 1500, city = 3000, fortress = 600 }
plague_toll_min = 100
plague_toll_max = 300
plague_spread_permille = 100
plague_spread_toll_permille = 500
crisis_per_generation_permille = 60
crisis_loss_permille = 50
split_min_settlements = 6
split_min_regions = 3
split_permille = 300
colony_new_region_salience = 30
plague_small_salience = 25
plague_major_dead = 1000
crisis_small_salience = 25
ghost_permille = 200
ghost_max_population = 600
abandon_reach_hops = 0
abandon_loss_min_permille = 50
abandon_loss_max_permille = 200
)p506");
    f["history/relations.toml"] = std::string(R"p506(# Race relations, mixed settlements and migrations (spec P5-03 4.2; P5-03a 10.1). Generator 0.25.0.
[history.relations]
hatred_max = -500
friend_min = 150
band_margin = 50
memory_min = -500
memory_max = 500
memory_decay_per_century = 20
war_memory = 75
conquest_memory = 150
sack_memory = 30
capture_memory = 15
expel_memory = 90
shelter_memory = 90
meet_hops = 8
migrate_hops = 2
migrate_min = 40
welcome_min = -200
subject_years = 200
conquest_settlers_permille = 50
drift_permille_per_century = 100
fade_permille = 20
mixed_min_permille = 100
expel_permille = 3
expel_share_max = 500
peace_years = 100
peace_memory_per_century = 60
peace_memory_max = 300
contact_hops = 1
foe_years = 100
foe_war_memory = 60
foe_monster_memory = 60
meet_sealed_hops = 5
meet_sealed_years = 150
conquest_settlers_span_permille = 400
expel_gap_years = 60
)p506");
    f["history/renaming.toml"] = std::string(R"p506(# Regions renamed by history (spec P5-12 section 4; P3-01 r16 5.4). Generator 0.22.0; forms of P5-02 21.2
# and rule 7 (P5-02 9.3), generator 0.23.0; rule 7's {name}'s Waste (P5-03 12.3), generator 0.24.0.
[history.renaming]
region_max = 2
region_gap_years = 300
world_max_permille = 150
century_max_permille = 50
century_years = 100

[[history.renaming.rules]]
key = "conquest"
kinds = ["civ_conquered"]
values = []
min_magnitude = 1
window_years = 1
min_salience = 0
capital = false
chance_permille = 1000
weight = 100
name_role = "civilization"
namer_role = "conqueror"
forms = ["{name}'s Grave", "The Grave of {name}", "The End of {name}", "{name}'s Barrow"]

[[history.renaming.rules]]
key = "capital_sack"
kinds = ["sacked"]
values = []
min_magnitude = 1
window_years = 1
min_salience = 90
capital = false
chance_permille = 700
weight = 90
name_role = "settlement"
namer_role = "attacker"
forms = ["{stem}'s Fall", "The Ashes of {name}", "The Ruin of {name}", "The Fallen {noun}"]

[[history.renaming.rules]]
key = "great_sack"
kinds = ["sacked"]
values = ["dead"]
min_magnitude = 1500
window_years = 1
min_salience = 0
capital = false
chance_permille = 500
weight = 80
name_role = "settlement"
namer_role = "attacker"
forms = ["{stem}'s Pyre", "The Ashes of {name}", "The Burnt {noun}", "The Cinder {noun}"]

[[history.renaming.rules]]
key = "slaughter"
kinds = ["battle", "battle_at_pass", "allied_battle", "battle_led"]
values = ["winner_dead", "loser_dead"]
min_magnitude = 5000
window_years = 100
min_salience = 0
capital = false
chance_permille = 600
weight = 70
name_role = "loser"
namer_role = "winner"
forms = ["The Red {noun}", "The Bloody {noun}", "The Rout of {name}", "The {noun} of the Slain", "The Weeping {noun}", "The Grim {noun}"]

[[history.renaming.rules]]
key = "plague"
kinds = ["plague_outbreak"]
values = ["total"]
min_magnitude = 3000
window_years = 1
min_salience = 0
capital = false
chance_permille = 400
weight = 60
name_role = "settlement"
namer_role = "civilization"
forms = ["The Emptied {noun}", "The Fever {noun}", "The Silent {noun}", "{stem}'s Grave", "The Pale {noun}", "The Hollow {noun}"]

[[history.renaming.rules]]
key = "heartland"
kinds = ["became_city"]
values = []
min_magnitude = 1
window_years = 1
min_salience = 0
capital = true
chance_permille = 300
weight = 50
name_role = "civilization"
namer_role = "civilization"
forms = ["{name}land", "The Heart of {name}", "The {name} Reach", "{name}'s Hearth"]

[[history.renaming.rules]]
key = "ravage"
kinds = ["settlement_overrun"]
values = []
min_magnitude = 1
window_years = 1
min_salience = 0
capital = false
chance_permille = 600
weight = 75
name_role = "monster"
namer_role = "civilization"
forms = ["The Haunt of {name}", "{name}'s Shadow", "The Forsaken {noun}", "{name}'s Waste"]
)p506");
    f["history/report.toml"] = std::string(R"p506([history.report]
events = [
  { kind = "awakening", salience = 100, template = "{creator} awakened the {race:plural} in {@region}." },
  { kind = "civ_founded", salience = 80, template = "The {race:plural} founded {civilization} in {@region}." },
  { kind = "settlement_founded", salience = 40, template = "{founder} founded {settlement} in {@region}." },
  { kind = "quiet_era", salience = 20, template = "{civilization} passed {years} quiet {years?year|years}, from {@year} to {@end_year}{generations?, under one generation of rulers|, in which one new generation of rulers came to power|, in which # new generations of rulers came to power}." },
  { kind = "flare_up", salience = 50, template = "In {@year} tensions flared into {years} {years?year|years} of upheaval." },
  { kind = "colony_founded", salience = 15, template = "{founder} sent settlers from {origin} to found {settlement} in {@region}." },
  { kind = "became_village", salience = 10, template = "{settlement} grew into a village of {population}." },
  { kind = "became_town", salience = 18, template = "{settlement} grew into a town of {population}." },
  { kind = "became_city", salience = 45, template = "{settlement} in {@region} grew into a city." },
  { kind = "settlement_dwindled", salience = 15, template = "{@causes:famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,battle=war,battle_at_pass=war,allied_battle=war,battle_led=war,blight=blight,tribute=tribute,colony_founded=emigration|Weakened by |, }{settlement} dwindled to {population} souls." },
  { kind = "famine", salience = 25, template = "Famine struck {settlement}; {dead} died." },
  { kind = "plague", salience = 45, template = "Plague broke out in {settlement}; {dead} died." },
  { kind = "plague_spread", salience = 25, template = "The plague spread to {settlement}; {dead} died." },
  { kind = "succession_crisis", salience = 40, template = "A succession crisis at {@settlement} shook {civilization}." },
  { kind = "civ_seceded", salience = 70, template = "{civilization} broke away from {from} with {settlements} {settlements?settlement|settlements}." },
  { kind = "settlement_abandoned", salience = 55, template = "{@causes:famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,battle=war,battle_at_pass=war,allied_battle=war,battle_led=war,blight=blight,tribute=tribute|Weakened by |, }{settlement} was abandoned; {survivors?no one survived|its one survivor was scattered|its # survivors were scattered}." },
  { kind = "capital_moved", salience = 50, template = "The seat of {civilization} moved to {settlement}." },
  { kind = "civ_ended", salience = 85, template = "{civilization}{civilization:under} came to an end." },
  { kind = "ruin_faded", salience = 10, template = "The ruins of {settlement} faded from memory." },
  { kind = "claim", salience = 35, template = "{claimant} laid claim to {@region}, held by {holder}." },
  { kind = "war_declared", salience = 70, template = "{attacker}{attacker:under} declared war on {defender} over {@region}." },
  { kind = "battle", salience = 30, template = "{winner} defeated {loser} in {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "battle_at_pass", salience = 35, template = "{winner} defeated {loser} at the pass into {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "siege", salience = 25, template = "{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}." },
  { kind = "sacked", salience = 75, template = "{attacker} sacked {@settlement} of {defender}; {dead} died and {survivors} fled." },
  { kind = "settlement_captured", salience = 60, template = "{attacker} took {@settlement} from {defender}; {dead} died{fled?| and one fled| and # fled}." },
  {)p506"
R"p506( kind = "peace", salience = 60, template = "{loser} sued {winner} for peace after {years} {years?year|years} of war." },
  { kind = "truce", salience = 50, template = "{parties:all} made peace after {years} {years?year|years} of war, neither the victor." },
  { kind = "region_ceded", salience = 55, template = "{from} ceded {@region} to {to}." },
  { kind = "civ_conquered", salience = 90, template = "{civilization}{civilization:under} fell to {conqueror}." },
  { kind = "settlement_abandoned_to", salience = 55, template = "{@causes:famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,battle=war,battle_at_pass=war,allied_battle=war,battle_led=war,blight=blight,tribute=tribute|Weakened by |, }{settlement} was abandoned; {settled} {settled?survivor|survivors} went to {destination}{lost?|, and one was lost|, and # were lost}." },
  { kind = "refugees_settled", salience = 65, template = "The survivors of {origin} fled into hiding and founded {@settlement} in {@region}." },
  { kind = "tribute", salience = 50, template = "{payer} paid tribute to {payee}; {people} {people?was|were} carried off." },
  { kind = "war_won", salience = 60, template = "{winner} prevailed over {loser} after {years} {years?year|years} of war." },
  { kind = "search_abandoned", salience = 40, template = "{searcher} searched {@region} for {hidden} and found no one; the war ended after {years} {years?year|years}." },
  { kind = "refuge_held", salience = 25, template = "{attacker} besieged {@settlement} of {defender}; with {reason?nowhere to flee|too few to flee}, its people held on." },
  { kind = "plague_outbreak", salience = 45, template = "Plague broke out in {settlement}{reached?| and spread to one other settlement| and spread to # other settlements}; {total} died{reached?| in all| in all}." },
  { kind = "quiet_stretch", salience = 20, template = "For {years} {years?year|years}, from {@year} to {@end_year}, the land was quiet{realms?|; one realm held it|, and # realms kept the peace}." },
  { kind = "claim_settlements", salience = 35, template = "{claimant} laid claim to {settlement:all} of {holder} in {@region}." },
  { kind = "settlements_ceded", salience = 55, template = "{from} gave up {settlement:all} in {@region} to {to}." },
  { kind = "claim_dropped", salience = 15, template = "{claimant} let the claim against {holder} over {@region} lapse." },
  { kind = "refuge_hidden", salience = 10, template = "{@settlement} lay hidden from {@year} to {@end_year}." },
  { kind = "region_renamed", salience = 60, template = "{@causes:civ_conquered=the fall of a realm,sacked=the sack,battle=the slaughter,battle_at_pass=the slaughter,allied_battle=the slaughter,battle_led=the slaughter,plague_outbreak=the plague,became_city=the rise of a realm's seat,settlement_overrun=the ravage|After |, }{@region_former} became known as {@region}." },
  { kind = "monster_appeared", salience = 45, template = "{monster}, {monster:kind}, made {monster:its} lair in {@region}." },
  { kind = "undead_rose", salience = 50, template = "{@causes:battle=the slaughter,battle_at_pass=the slaughter,allied_battle=the slaughter,battle_led=the slaughter,sacked=the sack|After |, }{monster} rose from {monster:its} graves{monster:where}." },
  { kind = "monster_woke", salience = 25, template = "{monster} woke again in {@region}." },
  { kind = "monster_slept", salience = 10, template = "{monster} sank into sleep in {lair}." },
  { kind = "monster_raid", salience = 15, template = "{monster} {monster:raid} {@settlement}; {dead?no one died|one died|# died}." },
  { kind = "settlement_overrun", salience = 50, template = "{monster} drove the people from {@settlement} and made it {monster:its} lair; {survivors?no one escaped|one escaped|# escaped}." },
  { kind = "monster_slain", salience = 55, template = "{slayer} hunted down and slew {monster}{monster:aside}{monster:where}." },
  { kind = "monster_driven_off", salience = 30, template = "{civilization} drove {m)p506"
R"p506(onster} back to {monster:its} lair{monster:where}." },
  { kind = "monster_died", salience = 30, template = "{monster}{monster:aside} died of great age{monster:where}." },
  { kind = "beasts_bold", salience = 15, template = "In {@region} the {@beast} {@beast:bold} {settlement}; {dead?no one|one|#} died." },
  { kind = "outpost_built", salience = 12, template = "{civilization} raised {site} in {@region} against {enemy}." },
  { kind = "outpost_abandoned", salience = 10, template = "{civilization} abandoned {site}{reason?, the frontier having fallen quiet|, the land being lost|, its realm having fallen}." },
  { kind = "settlement_fled", salience = 45, template = "For fear of {monster}, the people of {@settlement} left their homes." },
  { kind = "peoples_met", salience = 25, template = "The {race_a:plural} {sealed?first had word of|first met} the {race_b:plural} in {@region}{band?, and hated them from the first|, and were wary of them|, and were glad of them}." },
  { kind = "disposition_turned", salience = 55, template = "{@causes:war_won=the wars,peace=the wars,truce=the wars,civ_conquered=the conquests,sacked=the sacks,settlement_captured=the conquests,people_expelled=the expulsions,refugees_arrived=years of shelter given,war_declared=wars against a common enemy,monster_raid=the raids of a common foe,settlement_overrun=the raids of a common foe,peoples_met=long years of peace,disposition_turned=long years apart|After |, }the {race_a:plural} and the {race_b:plural} {band?came to hate one another|grew wary of one another|became friends}." },
  { kind = "refugees_arrived", salience = 25, template = "{@causes:sacked=the sack,settlement_captured=the fall,settlement_overrun=the ravage,people_expelled=the expulsion,settlement_abandoned=the abandonment of their home,settlement_abandoned_to=the abandonment of their home,plague=the plague,plague_outbreak=the plague,plague_spread=the plague,famine=the famine,settlement_fled=their flight,region_ceded=the cession,settlements_ceded=the cession|After |, }{people} {race:plural} from {origin} found shelter in {settlement}{standing?, held as subjects by|, tolerated by|, welcomed by|, among their own kin of} {civilization}." },
  { kind = "settlement_mixed", salience = 25, template = "{@causes:settlement_captured=the conquest,settlements_ceded=the cession,region_ceded=the cession,sacked=the sack,settlement_overrun=the ravage,refugees_arrived=the coming of refugees,refugees_settled=the flight,settlement_abandoned_to=the abandonment of a neighbour|After |, }in {settlement} the {ruling:plural} {standing?ruled over|tolerated|lived as equals with} the {race:plural}{majority?, who outnumbered them|}." },
  { kind = "standing_changed", salience = 15, template = "In {settlement}, {civilization} {standing?held the|tolerated the|accepted the} {race:plural}{standing? as subjects|| as equals}." },
  { kind = "people_expelled", salience = 40, template = "Hating the {race:plural}, {civilization} drove them out of {settlement}, where it had {standing?held them as subjects|tolerated them|tolerated them} for {years} {years?year|years}; {people} left." },
  { kind = "alliance_formed", salience = 50, template = "{@causes:trade_opened=long years of trade,war_declared=the outbreak of war,disposition_turned=the friendship of their peoples,civ_conquered=the fall of a realm,settlement_captured=the fall of a neighbour,sacked=the sack of a neighbour,region_ceded=the loss of a neighbour's land,settlements_ceded=the loss of a neighbour's land|After |, }{ally_a} and {ally_b} {basis?swore the alliance their peoples had kept since the gods made them|swore to stand together|made common cause|swore friendship and alliance}{foe:against}." },
  { kind = "alliance_renewed", salience = 10, template = "{ally_a} and {ally_b} renewed their alliance{foe:against}." },
  { kind = "alliance_lapsed", salience = 25, template = "The alliance of {ally_a} and {ally_b} lapsed{reason?, for together they had grown too great|, its cause long pas)p506"
R"p506(t}." },
  { kind = "alliance_broken", salience = 40, template = "{@causes:ally_refused=a call to arms refused|After |, }the alliance of {ally_a} and {ally_b} was broken{reason?, their friendship having cooled|}." },
  { kind = "alliance_betrayed", salience = 65, template = "{betrayer}{betrayer:under} broke faith and made war on {betrayed}, an ally." },
  { kind = "ally_joined_war", salience = 40, template = "{ally} {side?joined the war of|came to the defence of} {partner} against {enemy}." },
  { kind = "ally_refused", salience = 30, template = "{ally} would not take up arms for {partner} against {enemy}." },
  { kind = "ally_withdrew", salience = 25, template = "Bled white, {ally} left the war of {partner} against {enemy}." },
  { kind = "allied_battle", salience = 35, template = "{winner}{winner_ally:with} defeated {loser}{loser_ally:with} {pass?in|at the pass into|at the pass into} {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "trade_opened", salience = 20, template = "{@causes:peace=the peace,war_won=the war,truce=the truce,peoples_met=the first meeting of their peoples|After |, }{partner_a} and {partner_b} opened trade." },
  { kind = "trade_broken", salience = 15, template = "{@causes:war_declared=war came,alliance_betrayed=war came|When |, }the trade between {partner_a} and {partner_b} {reason?ended|dwindled as their friendship cooled|was lost with the roads between them}." },
  { kind = "spoils_shared", salience = 20, template = "{principal}{principal:under} gave {ally} its share of the spoils of {@region}." },
  { kind = "spoils_withheld", salience = 35, template = "{principal}{principal:under} kept the spoils of {@region} from {ally}, its ally." },
  { kind = "alliance_spurned", salience = 50, template = "After the spoils of {@region} were kept from it, {ally} broke its alliance with {principal}." },
  { kind = "mine_opened", salience = 20, template = "{civilization} opened {site} in {@region}." },
  { kind = "arms_sold", salience = 30, template = "{seller} sold arms forged in {site} to {buyer}." },
  { kind = "came_to_rule", salience = 15, template = "{figure} became the first ruler of {civilization}." },
  { kind = "figure_died", salience = 15, template = "{figure} of {civilization} died at the age of {age}{how?.|, and the succession was disputed.|, in exile.}" },
  { kind = "succession_disputed", salience = 30, template = "{ruler} took the throne of {civilization} against the claim of {claimant}{outcome?, who broke away with the far lands|, who was driven out}." },
  { kind = "exiled", salience = 25, template = "{figure}, who had claimed the throne of {civilization}, {hosted?went into exile with |fled into the wilds}{host:all}." },
  { kind = "reigned_at_end", salience = 10, template = "At the end of the age {civilization} was ruled by {figure}." },
  { kind = "general_appointed", salience = 15, template = "{figure} {again?again took|took} command of the armies of {civilization} against {enemy}." },
  { kind = "battle_led", salience = 35, template = "{winner}{winner_general:under}{winner_ally:with} defeated {loser}{loser_general:under}{loser_ally:with} {pass?in|at the pass into|at the pass into} {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "fell_in_battle", salience = 30, template = "{figure} of {civilization} fell in battle against {enemy} in {@region}." },
  { kind = "hero_rose", salience = 25, template = "After the raids of {monster}, {figure} of {civilization} took up the hunt." },
  { kind = "monster_slain_by_hero", salience = 55, template = "{hero} of {slayer} hunted down and slew {monster}{monster:aside}{monster:where}." },
  { kind = "hero_slain", salience = 45, template = "{figure} of {civilization} was slain hunting {monster}{monster:where}." },
  { kind = "tower_fo)p506"
R"p506(unded", salience = 40, template = "{figure} of {civilization} {library?founded|raised} {site} in {@region}." },
  { kind = "book_written", salience = 30, template = "{figure} wrote {@book} at {site}." },
  { kind = "blight", salience = 50, template = "The workings of {figure} at {site} went awry, and a blight fell on {@region}; {dead?no one|one|#} died." },
]
)p506");
    return f;
}

}  // namespace p506data
