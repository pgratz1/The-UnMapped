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
// worldgen/tests/reference/p3_data_reference.py. Generator 0.15.0 (P3-05
// 12.1, 12.2, P3-01 r6): density_permille, the goblin and faerie changes
// (added biome entries appended at the end of `biomes`, an assumption),
// history.peoples and the report entries of kinds 6-19. Generator 0.16.0
// (P3-06 12.1, 12.2, P3-01 r7): prowess_permille after aggression, goblin
// aggression 600, history.conflict and the report entries of kinds 20-30
// (appended after ruin_faded in kind id order, the same assumption).
// Generator 0.17.0 (P3-08; P3-01 r8, P3-02 r5 11.4, P3-04 r10 4.2, P3-05 r6
// 11, 12.1, 12.3, P3-06 r7 11): race field cell_weights after site_weights
// (dwarf 400/600, every other race 0/0), the faerie biome weights of P3-05
// 12.3, culture field god_patterns after patterns and the elven `The House
// of {root}`, history.peoples famine_ppm 1250, plague_spread_permille 100,
// plague_major_dead and crisis_small_salience, the templates of kinds 4, 14,
// 16 and 26, and kind 31's entry appended after civ_conquered (kind id
// order, the same assumption). Generator 0.18.0 (P3-09; P3-01 r9 3.2,
// P3-05 r8 21 item 5, P3-06 r9 20.9 and 20.12): race fields muster_permille,
// conceal_min and home_defense_permille after prowess_permille, the eleven
// history.conflict fields after last_refuge, regions_per_extra_civ and
// civs_extra_max after regions_per_civ, claim salience 35 and the entries of
// kinds 32 and 33 appended after kind 31's (the same assumption); P3-09 r3
// (P3-06 r10 20.19, P3-01 r10 3.3): kill_max_permille, find_permille and
// search_years after enmity_hops, faerie home_defense_permille 0, kinds 34-36
// appended after 33. Mirrors
// SPEC_FILES_018 of p3_data_reference.py. Generator 0.19.0 (P3-10; P3-01 r11
// 3.4, 3.5, P3-05 r12 22.1-22.5, P3-06 r14 21.2, 21.7, 21.9, P3-04 r12 20.1,
// 20.4): cell_weights gains inland, port and habitability after height
// (dwarf port 0, habitability 500; faerie inland 400, port 0), faerie coast
// -500, odds_min_permille (orc 500, others 700) and awakening_quarrel (orc 1)
// after home_defense_permille, history.conflict without odds_min and with
// search_weary_years 500 and search_repeat_salience 15 after search_years,
// history.peoples cause_memory_years 50 and inland_cap 20 after
// civs_extra_max, the templates of kinds 10, 15, 16, 17, 22, 23 and 31, and
// the entries of kinds 37 and 38 appended after kind 36's (the same
// assumption). P3-10 r1 (P3-06 r15 21.12, P3-01 r12 3.5): quarrel_strain 600
// after search_repeat_salience. P3-10 second round (r3; P3-01 r13 3.6, P3-06
// r17 22.1, 22.2, P3-04 r15 4.1): race hidden_years after awakening_quarrel
// (orc 100, others 300), history.conflict without hidden_years and with
// quarrel_min_settlements 2 after quarrel_strain, history.engine
// quiet_min_years 50 after quiet_events_max. P3-11 (generator 0.20.0; P3-01
// r14 3.7, P3-02 r15 14.1, P3-06 r19 23.6): race awakening_population after
// hidden_years (orc 1200, others 0), the faerie lists of P3-02 14.1, the
// entries of kinds 39 and 40 after quiet_stretch. P5-01 (generator 0.21.0;
// P3-01 r15 3.8, P3-02 r17 15.2, P3-05 r18 24, P3-06 r22 24.6): the dwarven
// forbidden list gains didin and ririn, history.conflict
// quarrel_strength_permille 1000 and toll_spread_permille 200 after
// quarrel_min_settlements, history.engine flare_min_years 10 after
// quiet_min_years, the became_city template, the entries of kinds 41 and 42
// after settlements_ceded. Mirrors SPEC_FILES_021. P5-02 (generator 0.23.0) and P5-03 (generator 0.24.0;
// race stances, history.relations, the new history.monsters and history.peoples fields, the phrases,
// forms and report entries): the texts are those of SPEC_FILES_024 at the calibrated values.
// P5-03a (generator 0.25.0; 10.1 to 10.3 and the calibration): history.relations, history.monsters, the
// report templates and history.conflict.border_friction 300; the texts are those of SPEC_FILES_025.
// P5-04 (generator 0.26.0; 4.1 to 4.3, 8.3, 11.1 and the calibration): the race fields origin_allies,
// ally_join_permille and mining_permille, history.alliances, the seven history.conflict fields and border_friction
// 520, the report entries of kinds 63 to 78 and allied_battle in the cause words; the texts are those of
// SPEC_FILES_026.

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
# P5-07 r2 4.5: the pace of deep warren delving (a race delves mines or warrens, not both).
delving_permille = 0
temper_weights = { steady = 450, warlike = 100, peaceable = 250, grasping = 50, just = 150 }
general_permille = 500
hero_permille = 100
magic_permille = 1000
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
# P5-07 r2 4.5: the pace of deep warren delving (a race delves mines or warrens, not both).
delving_permille = 300
temper_weights = { steady = 300, warlike = 300, peaceable = 50, grasping = 300, just = 50 }
general_permille = 250
hero_permille = 40
magic_permille = 150
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
# P5-07 r2 4.5: the pace of deep warren delving (a race delves mines or warrens, not both).
delving_permille = 0
temper_weights = { steady = 450, warlike = 150, peaceable = 100, grasping = 200, just = 100 }
general_permille = 500
hero_permille = 120
magic_permille = 300
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
# P5-07 r2 4.5: the pace of deep warren delving (a race delves mines or warrens, not both).
delving_permille = 0
temper_weights = { steady = 400, warlike = 50, peaceable = 350, grasping = 50, just = 150 }
general_permille = 150
hero_permille = 60
magic_permille = 900
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
# P5-07 r2 4.5: the pace of deep warren delving (a race delves mines or warrens, not both).
delving_permille = 0
temper_weights = { steady = 400, warlike = 175, peaceable = 175, grasping = 125, just = 125 }
general_permille = 450
hero_permille = 150
magic_permille = 500
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
# P5-07 r2 4.5: the pace of deep warren delving (a race delves mines or warrens, not both).
delving_permille = 0
temper_weights = { steady = 250, warlike = 500, peaceable = 50, grasping = 150, just = 50 }
general_permille = 400
hero_permille = 80
magic_permille = 150
)";

// --- Naming blocks (P3-02 5, verbatim) ---------------------------------------------

inline constexpr std::string_view DWARVEN_NAMING = R"(onsets = ["d", "b", "th", "k", "g", "br", "dr", "gr", "t", "h", "kh", "n", "m", "thr"]
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
)";

inline constexpr std::string_view ELVEN_NAMING = R"(onsets = ["", "l", "th", "s", "f", "n", "m", "r", "v", "gl", "c", "el", "ith"]
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
)";

inline constexpr std::string_view FAERIE_NAMING = R"(onsets = ["", "v", "f", "w", "l", "s", "t", "gl", "sh", "wh", "fl", "tw", "m"]
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
)";

inline constexpr std::string_view GOBLIN_NAMING = R"(onsets = ["g", "gr", "sn", "k", "kr", "z", "sk", "b", "n", "gn", "dr", "sl", "skr"]
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
)";

inline constexpr std::string_view HUMAN_NAMING = R"(onsets = ["b", "c", "d", "g", "h", "m", "r", "s", "t", "w", "st", "br", "al", "ed", "ash"]
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
)";

inline constexpr std::string_view ORCISH_NAMING = R"(onsets = ["gh", "gr", "ug", "m", "k", "z", "sh", "b", "d", "th", "kr", "ur", "az"]
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
)";

// --- Chronology (P3-01 5.1) -------------------------------------------------------

inline constexpr std::string_view CHRONOLOGY_TOML = R"([history.chronology]
awakening_jitter_frac = "0.01"
awakening_min_gap_years = 100
age_name_form = "The Age of {plural}"
creator_title = "the Maker of {plural}"
)";

// --- history.engine and history.report (P3-04 4.1, 4.2; every root carries
// all fixed ids of kind history, P3-01 5) -----------------------------------------

inline constexpr std::string_view ENGINE_TOML = R"([history.engine]
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
)";

inline constexpr std::string_view REPORT_TOML = R"([history.report]
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
  { kind = "civ_ended", salience = 85, template = "{@causes:hold_lost=the loss of its last hold to the deep|After |, }{civilization}{civilization:under} came to an end." },
  { kind = "ruin_faded", salience = 10, template = "The ruins of {settlement} faded from memory." },
  { kind = "claim", salience = 35, template = "{claimant} laid claim to {@region}, held by {holder}." },
  { kind = "war_declared", salience = 70, template = "{attacker}{attacker:under} declared war on {defender} over {@region}." },
  { kind = "battle", salience = 30, template = "{@causes:arms_sold=new-bought arms|With |, }{winner} defeated {loser} in {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "battle_at_pass", salience = 35, template = "{@causes:arms_sold=new-bought arms|With |, }{winner} defeated {loser} at the pass into {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "siege", salience = 25, template = "{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}." },
  { kind = "sacked", salience = 75, template = "{attacker} sacked {@settlement} of {defender}; {dead} died and {survivors} fled." },
  { kind = "settlement_captured", salience = 60, template = "{attacker} took {@settlement} from {defender}; {dead} died{fled?| and one fled| and # fled}." },
  { kind = "peace", salience = 60, template = "{loser} sued {winner} for peace after {years} {years?year|years} of war." },
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
  { kind = "region_renamed", salience = 60, template = "{@causes:civ_conquered=the fall of a realm,sacked=the sack,battle=the slaughter,battle_at_pass=the slaughter,allied_battle=the slaughter,battle_led=the slaughter,plague_outbreak=the plague,became_city=the rise of a realm's seat,settlement_overrun=the ravage,hold_lost=the fall of a hold to the deep|After |, }{@region_former} became known as {@region}." },
  { kind = "monster_appeared", salience = 45, template = "{monster}, {monster:kind}, made {monster:its} lair in {@region}." },
  { kind = "undead_rose", salience = 50, template = "{@causes:battle=the slaughter,battle_at_pass=the slaughter,allied_battle=the slaughter,battle_led=the slaughter,sacked=the sack|After |, }{monster} rose from {monster:its} graves{monster:where}." },
  { kind = "monster_woke", salience = 25, template = "{monster} woke again in {@region}." },
  { kind = "monster_slept", salience = 10, template = "{monster} sank into sleep in {lair}." },
  { kind = "monster_raid", salience = 15, template = "{monster} {monster:raid} {@settlement}; {dead?no one died|one died|# died}." },
  { kind = "settlement_overrun", salience = 50, template = "{monster} drove the people from {@settlement} and made it {monster:its} lair; {survivors?no one escaped|one escaped|# escaped}." },
  { kind = "monster_slain", salience = 55, template = "{slayer} hunted down and slew {monster}{monster:aside}{monster:where}." },
  { kind = "monster_driven_off", salience = 30, template = "{civilization} drove {monster} back to {monster:its} lair{monster:where}." },
  { kind = "monster_died", salience = 30, template = "{monster}{monster:aside} {monster:died}{monster:where}." },
  { kind = "beasts_bold", salience = 15, template = "In {@region} the {@beast} {@beast:bold} {settlement}; {dead?no one|one|#} died." },
  { kind = "outpost_built", salience = 12, template = "{civilization} raised {site} in {@region} against {enemy}." },
  { kind = "outpost_abandoned", salience = 10, template = "{civilization} abandoned {site}{reason?, the frontier having fallen quiet|, the land being lost|, its realm having fallen}." },
  { kind = "settlement_fled", salience = 45, template = "For fear of {monster}, the people of {@settlement} left their homes." },
  { kind = "peoples_met", salience = 25, template = "The {race_a:plural} {sealed?first had word of|first met} the {race_b:plural} in {@region}{band?, and hated them from the first|, and were wary of them|, and were glad of them}." },
  { kind = "disposition_turned", salience = 55, template = "{@causes:war_won=the wars,peace=the wars,truce=the wars,civ_conquered=the conquests,sacked=the sacks,settlement_captured=the conquests,people_expelled=the expulsions,refugees_arrived=years of shelter given,war_declared=wars against a common enemy,monster_raid=the raids of a common foe,settlement_overrun=the raids of a common foe,peoples_met=long years of peace,disposition_turned=long years apart|After |, }the {race_a:plural} and the {race_b:plural} {change?came to hate one another|put aside their hatred of one another|grew wary of one another|became friends}." },
  { kind = "refugees_arrived", salience = 25, template = "{@causes:sacked=the sack,settlement_captured=the fall,settlement_overrun=the ravage,people_expelled=the expulsion,settlement_abandoned=the abandonment of their home,settlement_abandoned_to=the abandonment of their home,plague=the plague,plague_outbreak=the plague,plague_spread=the plague,famine=the famine,settlement_fled=their flight,region_ceded=the cession,settlements_ceded=the cession,hold_lost=the fall of their hold|After |, }{people} {race:plural} from {origin} found shelter in {settlement}{standing?, held as subjects by|, tolerated by|, welcomed by|, among their own kin of} {civilization}." },
  { kind = "settlement_mixed", salience = 25, template = "{@causes:settlement_captured=the conquest,settlements_ceded=the cession,region_ceded=the cession,sacked=the sack,settlement_overrun=the ravage,refugees_arrived=the coming of refugees,refugees_settled=the flight,settlement_abandoned_to=the abandonment of a neighbour|After |, }in {settlement} the {ruling:plural} {standing?ruled over|tolerated|lived as equals with} the {race:plural}{majority?, who outnumbered them|}." },
  { kind = "standing_changed", salience = 15, template = "In {settlement}, {civilization} {standing?held the|tolerated the|accepted the} {race:plural}{standing? as subjects|| as equals}." },
  { kind = "people_expelled", salience = 40, template = "Hating the {race:plural}, {civilization} drove them out of {settlement}, where it had {standing?held them as subjects|tolerated them|tolerated them} for {years} {years?year|years}; {people} left." },
  { kind = "alliance_formed", salience = 50, template = "{@causes:trade_opened=long years of trade,war_declared=the outbreak of war,disposition_turned=the friendship of their peoples,civ_conquered=the fall of a realm,settlement_captured=the conquests of their foe,sacked=the sacks of their foe,region_ceded=the conquests of their foe,settlements_ceded=the conquests of their foe|After |, }{ally_a} and {ally_b} {basis?swore the alliance their peoples had kept since the gods made them|swore to stand together|made common cause|swore friendship and alliance}{foe:against}{band?, though their peoples hated one another||}." },
  { kind = "alliance_renewed", salience = 10, template = "{@causes:ally_joined_war=the war they fought together,spoils_shared=the spoils they shared,war_declared=the outbreak of a new war,trade_opened=long years of trade,disposition_turned=the friendship of their peoples,civ_conquered=the fall of a realm,settlement_captured=the conquests of their foe,sacked=the sacks of their foe,region_ceded=the conquests of their foe,settlements_ceded=the conquests of their foe|After |, }{ally_a} and {ally_b} renewed their alliance{foe:against}." },
  { kind = "alliance_lapsed", salience = 25, template = "The alliance of {ally_a} and {ally_b} lapsed{reason?, for together they had grown too great|, its cause long past}." },
  { kind = "alliance_broken", salience = 40, template = "{@causes:ally_refused=a call to arms refused|After |, }the alliance of {ally_a} and {ally_b} was broken{reason?, their friendship having cooled|}." },
  { kind = "alliance_betrayed", salience = 65, template = "{motive?Grasping for more|Its friendship long cooled|Seeing its ally weak|Coveting its ally's land}, {betrayer}{betrayer:under} broke an alliance of {years} {years?year|years} with {betrayed}." },
  { kind = "ally_joined_war", salience = 40, template = "{ally} {side?joined the war of|came to the defence of} {partner} against {enemy}." },
  { kind = "ally_refused", salience = 30, template = "{motive?Loath to send its people to war|Weary of war|With a war of its own to fight|Fearing the strength of the foe|Grown cold toward its ally|Hoping to profit by its ally's losses|Nursing a claim on its ally's land|Remembering the spoils kept from it}, {ally}{ally:under} would not take up arms for {partner} against {enemy}{count?|, and its ally did not forget it|}." },
  { kind = "ally_withdrew", salience = 25, template = "Bled white, {ally} left the war of {partner} against {enemy}." },
  { kind = "allied_battle", salience = 35, template = "{@causes:arms_sold=new-bought arms|With |, }{winner}{winner_ally:with} defeated {loser}{loser_ally:with} {pass?in|at the pass into|at the pass into} {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "trade_opened", salience = 20, template = "{@causes:peace=the peace,war_won=the war,truce=the truce,peoples_met=the first meeting of their peoples|After |, }{partner_a} and {partner_b} opened trade." },
  { kind = "trade_broken", salience = 15, template = "{@causes:war_declared=war came,alliance_betrayed=war came|When |, }the trade between {partner_a} and {partner_b} {reason?ended|dwindled as their friendship cooled|was lost with the roads between them}." },
  { kind = "spoils_shared", salience = 20, template = "{principal}{principal:under} gave {ally} its share of the spoils of {@region}, {share?no captives|one captive|# captives}." },
  { kind = "spoils_withheld", salience = 35, template = "{principal}{principal:under} kept the spoils of {@region}, {share?no captives|one captive|# captives}, from {ally}, its ally{grievance?|, which did not forget it|, which would not forgive it}." },
  { kind = "alliance_spurned", salience = 50, template = "{reason?Still bitter|Long resentful|Outraged} that the spoils of {@region} had been kept from it{years?| the year before| # years before}, {ally} broke its alliance with {principal}." },
  { kind = "mine_opened", salience = 20, template = "{civilization} opened {site} in {@region}." },
  { kind = "arms_sold", salience = 30, template = "{seller} sold arms forged in {site} to {buyer}." },
  # P5-06 8.2 (generator 0.27.0): kinds 79 to 92.
  { kind = "came_to_rule", salience = 15, template = "{figure} became the first ruler of {civilization}." },
  { kind = "figure_died", salience = 15, template = "{figure} of {civilization} died at the age of {age}{how?.|, and the succession was disputed.|, in exile.}" },
  { kind = "succession_disputed", salience = 30, template = "{ruler} took the throne of {civilization} against the claim of {claimant}{outcome?, who broke away with the far lands|, who was driven out}." },
  { kind = "exiled", salience = 25, template = "{figure}, who had claimed the throne of {civilization}, {hosted?went into exile with |fled into the wilds}{host:all}." },
  { kind = "reigned_at_end", salience = 10, template = "At the end of the age {civilization} was ruled by {figure}." },
  { kind = "general_appointed", salience = 15, template = "{figure} {again?again took|took} command of the armies of {civilization} against {enemy}." },
  { kind = "battle_led", salience = 35, template = "{@causes:arms_sold=new-bought arms|With |, }{winner}{winner_general:under}{winner_ally:with} defeated {loser}{loser_general:under}{loser_ally:with} {pass?in|at the pass into|at the pass into} {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "fell_in_battle", salience = 30, template = "{figure} of {civilization} fell in battle against {enemy} in {@region}." },
  { kind = "hero_rose", salience = 25, template = "{quiet?After the raids of |After the raids of |When, after # quiet years, }{monster}{quiet?|| raided again}, {figure} of {civilization} took up the hunt." },
  { kind = "monster_slain_by_hero", salience = 55, template = "{hero} of {slayer} hunted down and slew {monster}{monster:aside}{monster:where}." },
  { kind = "hero_slain", salience = 45, template = "{figure} of {civilization} was slain hunting {monster}{monster:where}." },
  { kind = "tower_founded", salience = 40, template = "{@causes:figure_died=the death of the last master|After |, }{figure} of {civilization} {library?founded|raised} {site} in {@region}." },
  { kind = "book_written", salience = 30, template = "{figure} wrote {@book} at {site}." },
  { kind = "blight", salience = 50, template = "The workings of {figure} at {site} went awry, and a blight fell on {@region}; {dead?no one|one|#} died." },
  # P5-07 8.2 (generator 0.29.0): kinds 93 to 96.
  { kind = "delving_deepened", salience = 15, template = "{telling?Following a rich seam, |Year upon year, |By hammer and lamp, |Chasing the silver down, |Shaft by shaft, |Heedless of the old warnings, |With new picks from the forge, |Through a hard generation, |Singing the old delving songs, }{civilization} {depth?worked|sank the shafts of|drove the halls of|broke the floor of} {site} {depth?deeper|below the reach of daylight|down to the mountain's roots|into the oldest dark beneath the world}{telling?, and struck a vein of silver|, and broke into a cavern of crystal|, and felt a cold breath rise from below|, and found the bones of an older people|, and heard hammering where none worked|, and struck black iron|, and found a lake no light had touched|, and lost a crew to a sudden fall|, and found gold in the dark}." },
  { kind = "deep_woken", salience = 60, template = "{telling?Delving too deep beneath |In the deepest halls under |Where the last seam ran out beneath |Burrowing ever deeper under |Digging after grubs and glitter beneath |In the stinking deep tunnels of }{@settlement}, {civilization}{civilization:under}{telling? woke | woke | broke into the old dark and woke | woke | blundered into the old dark and woke | woke }{monster}, {monster:kind}." },
  { kind = "deep_sealed", salience = 50, template = "{telling?|Holding the gate at great cost, |With stone and rune, }{civilization}{civilization:under}{telling? sealed | walled | shut }{monster}{telling? in the deeps of | up beneath | away below }{site}{telling?||, and the deep ways were closed}." },
  { kind = "hold_lost", salience = 70, template = "{@causes:monster_raid=the raids|After |, }{monster}{telling? came up out of the deep into| rose from below into| broke out of the deeps into| boiled up out of the burrows into| came up the goblins' own tunnels into| rose under the warrens of} {@settlement}{telling?, and tore it from|, and wrested it from|, and took it from|, and took it from|, and took it from|, and tore it from} {civilization}; {dead?no one died|one died|# died}{survivors?, and none escaped|, and one escaped|, and # fled}." },
  { kind = "ally_aided_hold", salience = 40, template = "{telling?Answering its ally's call, |Bound by old oaths, |Unbidden, }{ally}{ally:under}{telling? sent warriors from | marched out of | hurried its people from }{origin} to stand with {partner} at the gate of {@settlement} against {monster}{@causes:deep_sealed=and the gate held,warren_sealed=and the tunnels were closed|, |}{@causes:hold_lost=but the hold fell all the same|, |}." },
  { kind = "warren_delved", salience = 15, template = "{telling?Ever greedy for the dark, |Squabbling and scraping, |Grub by grub and stone by stone, |Whipped on by their chiefs, |Stealing each other's picks, |Gnawing like rats, |Chasing a rumour of gold, |Fleeing the daylight, |In fits of greed and spite, }{civilization} dug the warrens of {@settlement} {depth?deeper|below the reach of daylight|down among the mountain's roots|into the oldest dark beneath the world}{telling?, and ate what they found there|, and found a nest of blind worms|, and lost a dozen in a fall|, and fought over a lump of silver|, and found old bones to gnaw|, and heard something breathing|, and struck a cold black water|, and found a passage no goblin had cut|, and came up shrieking}." },
  { kind = "warren_sealed", salience = 50, template = "{telling?|Shrieking and stabbing in the dark, |With rockfall and fire, }{civilization}{civilization:under}{telling? caved in the tunnels over | blocked the deep ways against | buried }{monster}{telling? beneath | under | deep below }{@settlement}{telling?||, and no goblin went that way again}." },
  # P5-08 8.2 (generator 0.30.0): kinds 100 to 110.
  { kind = "divine_bless", salience = 55, template = "{@causes:sacked=the sack of a city,settlement_captured=a city taken,civ_conquered=a realm conquered,war_won=a war won,battle=a battle won,battle_led=a battle won,allied_battle=a battle won,monster_slain=a monster slain,monster_slain_by_hero=a monster slain,became_city=a city grown great,became_town=a town grown,deep_sealed=the deep sealed,deep_woken=greed in the deep,refuge_held=a refuge held,peace=a peace made,tribute=tribute paid,truce=a truce made,mine_opened=a mine opened,delving_deepened=deep delving,arms_sold=arms sold,blight=a blighted land,trade_opened=trade opened,tower_founded=a tower raised,book_written=a book written,alliance_formed=an oath sworn,spoils_shared=spoils shared,ally_joined_war=an ally's faith kept,alliance_betrayed=an oath broken,spoils_withheld=spoils withheld,ally_refused=a call to arms refused,ally_withdrew=an ally's flight,colony_founded=the felling of the wild,settlement_abandoned=land given back to the wild,outpost_abandoned=land given back to the wild,cult_adopted=a faith taken up,cult_suppressed=its faithful put down|Pleased by |, }{actor:titled}{telling? blessed| smiled upon| set its hand over}{effect?| the fields of| the arms of} {civilization}{years?| for a year| for # years}." },
  { kind = "divine_curse", salience = 60, template = "{@causes:sacked=the sack of a city,settlement_captured=a city taken,civ_conquered=a realm conquered,war_won=a war won,battle=a battle won,battle_led=a battle won,allied_battle=a battle won,monster_slain=a monster slain,monster_slain_by_hero=a monster slain,became_city=a city grown great,became_town=a town grown,deep_sealed=the deep sealed,deep_woken=greed in the deep,refuge_held=a refuge held,peace=a peace made,tribute=tribute paid,truce=a truce made,mine_opened=a mine opened,delving_deepened=deep delving,arms_sold=arms sold,blight=a blighted land,trade_opened=trade opened,tower_founded=a tower raised,book_written=a book written,alliance_formed=an oath sworn,spoils_shared=spoils shared,ally_joined_war=an ally's faith kept,alliance_betrayed=an oath broken,spoils_withheld=spoils withheld,ally_refused=a call to arms refused,ally_withdrew=an ally's flight,colony_founded=the felling of the wild,settlement_abandoned=land given back to the wild,outpost_abandoned=land given back to the wild,cult_adopted=a faith taken up,cult_suppressed=its faithful put down|Angered by |, }{actor:titled}{effect?| sent a pestilence upon| withered the fields of| turned the fortunes of war against} {civilization}{dead?|; one died|; # died}." },
  { kind = "divine_champion", salience = 55, template = "{@causes:sacked=the sack of a city,settlement_captured=a city taken,civ_conquered=a realm conquered,war_won=a war won,battle=a battle won,battle_led=a battle won,allied_battle=a battle won,monster_slain=a monster slain,monster_slain_by_hero=a monster slain,became_city=a city grown great,became_town=a town grown,deep_sealed=the deep sealed,deep_woken=greed in the deep,refuge_held=a refuge held,peace=a peace made,tribute=tribute paid,truce=a truce made,mine_opened=a mine opened,delving_deepened=deep delving,arms_sold=arms sold,blight=a blighted land,trade_opened=trade opened,tower_founded=a tower raised,book_written=a book written,alliance_formed=an oath sworn,spoils_shared=spoils shared,ally_joined_war=an ally's faith kept,alliance_betrayed=an oath broken,spoils_withheld=spoils withheld,ally_refused=a call to arms refused,ally_withdrew=an ally's flight,colony_founded=the felling of the wild,settlement_abandoned=land given back to the wild,outpost_abandoned=land given back to the wild,cult_adopted=a faith taken up,cult_suppressed=its faithful put down|Pleased by |, }{actor:titled}{telling? raised up| chose| set its mark upon} {figure} as the champion of {civilization}." },
  { kind = "divine_monster", salience = 60, template = "{@causes:sacked=the sack of a city,settlement_captured=a city taken,civ_conquered=a realm conquered,war_won=a war won,battle=a battle won,battle_led=a battle won,allied_battle=a battle won,monster_slain=a monster slain,monster_slain_by_hero=a monster slain,became_city=a city grown great,became_town=a town grown,deep_sealed=the deep sealed,deep_woken=greed in the deep,refuge_held=a refuge held,peace=a peace made,tribute=tribute paid,truce=a truce made,mine_opened=a mine opened,delving_deepened=deep delving,arms_sold=arms sold,blight=a blighted land,trade_opened=trade opened,tower_founded=a tower raised,book_written=a book written,alliance_formed=an oath sworn,spoils_shared=spoils shared,ally_joined_war=an ally's faith kept,alliance_betrayed=an oath broken,spoils_withheld=spoils withheld,ally_refused=a call to arms refused,ally_withdrew=an ally's flight,colony_founded=the felling of the wild,settlement_abandoned=land given back to the wild,outpost_abandoned=land given back to the wild,cult_adopted=a faith taken up,cult_suppressed=its faithful put down|Angered by |, }{actor:titled}{telling? sent| loosed| woke} {monster}, {monster:kind}, against {civilization}." },
  { kind = "divine_mark", salience = 45, template = "{@causes:sacked=the sack of a city,settlement_captured=a city taken,civ_conquered=a realm conquered,war_won=a war won,battle=a battle won,battle_led=a battle won,allied_battle=a battle won,monster_slain=a monster slain,monster_slain_by_hero=a monster slain,became_city=a city grown great,became_town=a town grown,deep_sealed=the deep sealed,deep_woken=greed in the deep,refuge_held=a refuge held,peace=a peace made,tribute=tribute paid,truce=a truce made,mine_opened=a mine opened,delving_deepened=deep delving,arms_sold=arms sold,blight=a blighted land,trade_opened=trade opened,tower_founded=a tower raised,book_written=a book written,alliance_formed=an oath sworn,spoils_shared=spoils shared,ally_joined_war=an ally's faith kept,alliance_betrayed=an oath broken,spoils_withheld=spoils withheld,ally_refused=a call to arms refused,ally_withdrew=an ally's flight,colony_founded=the felling of the wild,settlement_abandoned=land given back to the wild,outpost_abandoned=land given back to the wild,cult_adopted=a faith taken up,cult_suppressed=its faithful put down|After |, }{actor:titled}{profane? cursed| hallowed} the ground of {site}." },
  { kind = "cult_founded", salience = 50, template = "{telling?In the name of |Calling upon |Sworn to }{god:titled}, {founder} founded {faction} at {site} in {@settlement}." },
  { kind = "cult_spread", salience = 25, template = "{faction} spread from {from} into {civilization}{@causes:trade_opened=the roads of trade,alliance_formed=the bonds of alliance| along |}." },
  { kind = "cult_adopted", salience = 45, template = "{civilization}{civilization:under} took {faction} as the faith of the realm." },
  { kind = "cult_suppressed", salience = 50, template = "{civilization}{civilization:under} put down {faction}{hidden?, and its faithful fled into hiding|}." },
  { kind = "tower_taken_up", salience = 20, template = "{@causes:figure_died=the death of its last master|After |, }{figure} of {civilization} became master of {site}." },
  { kind = "tower_abandoned", salience = 25, template = "{@causes:figure_died=the death of its last master|After |, }{site} of {civilization} stood empty." },
]
)";

// --- history.renaming (P5-12 4 and 4.1, P3-01 r16 5.4; generator 0.22.0) ---

inline constexpr std::string_view RENAMING_TOML = R"(# Regions renamed by history (spec P5-12 section 4; P3-01 r16 5.4). Generator 0.22.0; P5-02 21.2 and rule 7, generator 0.23.0.
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

# P5-07 4.4 (generator 0.29.0): a hold lost to the deep.
[[history.renaming.rules]]
key = "deep"
kinds = ["hold_lost"]
values = []
min_magnitude = 1
window_years = 1
min_salience = 0
capital = false
chance_permille = 700
weight = 85
name_role = "monster"
namer_role = "civilization"
forms = ["The Deeps of {name}", "{name}'s Dark", "The Lost {noun}", "The Sunken {noun}"]
)";


// --- P5-02 4.1 to 4.3 (generator 0.23.0): kinds beast and monster, history.monsters; the files of game/data
// (both fixture roots hold identical copies). The rates are interim (P5-02 17 step 9): after the engineer's
// calibration these listings follow the data files.

inline constexpr std::string_view MONSTERS_TOML = R"(# Beasts, monsters and sites (spec P5-02 4.3). Generator 0.23.0; P5-03a 10.2 (generator 0.25.0).
[history.monsters]
pressure = { low = 500, normal = 1000, high = 1600 }
living_permille = 200
undead_min_dead = 1000
undead_window_years = 100
hunt_hops = 2
hunt_levy_permille = 50
hunt_permille = 12
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
# P5-03 9.1 to 9.4, 14.1 (generator 0.24.0).
raid_chain_years = 300
toll_over_permille = 250
hunt_raid_permille = 5
hunt_max_permille = 200
fear_raids = 2
fear_years = 50
fear_permille = 400
overrun_people_ref = 3000
undead_dead_permille = 28
# P5-06 25.3 (generator 0.27.0).
undead_regions_ref = 64
# P5-07 4.2 (generator 0.29.0): the mountain depths.
deep_years = [250, 1000, 2500]
deep_delve_permille = 6
deep_wake_ppm = [0, 350, 1400]
deep_gate_levy_permille = 1000
deep_seal_max_permille = 800
deep_toll_permille = 400
deep_takes_max = 3
deep_reach_hops = 2
# P5-07 r2 4.2: goblin deep warrens (6.10) and allies' aid at the gate (6.11).
deep_warren_population_min = 3200
deep_warren_ore_min = 500
deep_warren_wake_permille = 200
deep_aid_permille = 500
deep_aid_hops = 2
deep_aid_levy_permille = 500
deep_aid_max = 2
deep_aid_memory = 30
# P5-08 12.2 (generator 0.30.0): the deep stirs in quiet steps (0: off).
deep_stir_permille = 700
)";

inline constexpr std::string_view BEAST_BEAR_TOML = R"(# Beast species (spec P5-02 4.1). Generator 0.24.0.
[beast.bear]
name = "Bear"
plural = "bears"
habitat = [{ biome = "temperate_forest", weight = 900 }, { biome = "wet_woodland", weight = 700 }, { biome = "highland_moor", weight = 400 }, { biome = "alpine", weight = 500 }, { biome = "heath", weight = 200 }]
density = 5
growth_permille = 60
menace = 30
food = 30
bold_phrases = ["broke into the stores of", "came down on the herds of"]
)";
inline constexpr std::string_view BEAST_BOAR_TOML = R"(# Beast species (spec P5-02 4.1). Generator 0.24.0.
[beast.boar]
name = "Boar"
plural = "boar"
habitat = [{ biome = "temperate_forest", weight = 800 }, { biome = "wet_woodland", weight = 900 }, { biome = "grassland", weight = 300 }, { biome = "heath", weight = 300 }, { biome = "marsh", weight = 400 }, { biome = "fen", weight = 300 }]
density = 40
growth_permille = 250
menace = 3
food = 100
bold_phrases = ["trampled the fields of", "tore up the gardens of"]
)";
inline constexpr std::string_view BEAST_DEER_TOML = R"(# Beast species (spec P5-02 4.1). Generator 0.24.0.
[beast.deer]
name = "Deer"
plural = "deer"
habitat = [{ biome = "temperate_forest", weight = 900 }, { biome = "wet_woodland", weight = 600 }, { biome = "grassland", weight = 800 }, { biome = "heath", weight = 600 }, { biome = "steppe", weight = 500 }, { biome = "highland_moor", weight = 500 }, { biome = "alpine", weight = 200 }, { biome = "marsh", weight = 200 }, { biome = "fen", weight = 200 }]
density = 100
growth_permille = 250
menace = 0
food = 60
bold_phrases = ["strayed into the fields of"]
)";
inline constexpr std::string_view BEAST_GIANT_SPIDER_TOML = R"(# Beast species (spec P5-02 4.1). Generator 0.24.0.
[beast.giant_spider]
name = "Giant spider"
plural = "giant spiders"
habitat = [{ biome = "wet_woodland", weight = 800 }, { biome = "temperate_forest", weight = 400 }, { biome = "marsh", weight = 600 }, { biome = "fen", weight = 500 }, { biome = "blighted_waste", weight = 900 }]
density = 10
growth_permille = 200
menace = 40
food = 0
bold_phrases = ["spun their webs across the paths of", "lay in wait on the roads from"]
)";
inline constexpr std::string_view BEAST_WOLF_TOML = R"(# Beast species (spec P5-02 4.1). Generator 0.24.0.
[beast.wolf]
name = "Wolf"
plural = "wolves"
habitat = [{ biome = "temperate_forest", weight = 700 }, { biome = "wet_woodland", weight = 500 }, { biome = "grassland", weight = 400 }, { biome = "heath", weight = 600 }, { biome = "steppe", weight = 700 }, { biome = "highland_moor", weight = 800 }, { biome = "alpine", weight = 400 }, { biome = "marsh", weight = 100 }, { biome = "fen", weight = 100 }, { biome = "blighted_waste", weight = 200 }]
density = 20
growth_permille = 150
menace = 10
food = 0
bold_phrases = ["hunted the roads from", "grew bold and stalked the herds of"]
)";
inline constexpr std::string_view MONSTER_DRAGON_TOML = R"(# Monster kind (spec P5-02 4.2). Generator 0.24.0.
[monster.dragon]
name = "Dragon"
noun = "a dragon of the old world"
grouped = false
wake = "wild"
biomes = [{ biome = "alpine", weight = 800 }, { biome = "highland_moor", weight = 400 }, { biome = "blighted_waste", weight = 600 }]
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
overrun_permille = 1000
active_years = 40
rewake_ppm = 700
lifespan_years = 0
max_alive = 1
flare = true
hoard_permille = 40
raid_phrases = ["fell in fire upon", "burned the fields of", "came down in flame on"]
slayer_epithet = "Dragonslayer"
)";
inline constexpr std::string_view MONSTER_GIANT_TOML = R"(# Monster kind (spec P5-02 4.2). Generator 0.24.0.
[monster.giant]
name = "Giant"
noun = "a giant of the mountains"
grouped = false
wake = "wild"
biomes = [{ biome = "alpine", weight = 900 }, { biome = "highland_moor", weight = 500 }]
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
)";
inline constexpr std::string_view MONSTER_OGRE_TOML = R"(# Monster kind (spec P5-02 4.2). Generator 0.24.0.
[monster.ogre]
name = "Ogre"
noun = "an ogre"
grouped = false
wake = "wild"
biomes = [{ biome = "grassland", weight = 400 }, { biome = "heath", weight = 600 }, { biome = "steppe", weight = 700 }, { biome = "highland_moor", weight = 300 }]
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
)";
inline constexpr std::string_view MONSTER_TROLL_TOML = R"(# Monster kind (spec P5-02 4.2). Generator 0.24.0.
[monster.troll]
name = "Troll"
noun = "a troll of the hills"
grouped = false
wake = "wild"
biomes = [{ biome = "highland_moor", weight = 800 }, { biome = "alpine", weight = 600 }, { biome = "temperate_forest", weight = 400 }, { biome = "heath", weight = 300 }]
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
)";
inline constexpr std::string_view MONSTER_UNDEAD_TOML = R"(# Monster kind (spec P5-02 4.2). Generator 0.24.0.
[monster.undead]
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
)";
inline constexpr std::string_view MONSTER_WYVERN_TOML = R"(# Monster kind (spec P5-02 4.2). Generator 0.24.0.
[monster.wyvern]
name = "Wyvern"
noun = "a brood of wyverns"
grouped = true
wake = "wild"
biomes = [{ biome = "alpine", weight = 700 }, { biome = "heath", weight = 500 }, { biome = "highland_moor", weight = 600 }]
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
)";

inline constexpr std::string_view MONSTER_YMRACH_TOML = R"(# Monster kind (spec P5-07 4.1): a deep horror, woken only from a mine. Generator 0.29.0.
# Its id sorts after wyvern so that the six earlier kinds keep their ids.
[monster.ymrach]
name = "Ymrach"
noun = "a wyrm of the lightless deep"
grouped = false
wake = "deep"
biomes = []
wake_rate = 500
naming_culture = "dwarven"
name_forms = ["{root} the Deep-Coiled", "{root} Stonegnawer", "{root}"]
lair_forms = ["The Coils of {root}", "{root}'s Delving"]
might = 4000
range_hops = 1
raid_permille = 60
toll_min_permille = 30
toll_max_permille = 100
kill_max = 300
overruns = []
overrun_permille = 600
active_years = 60
rewake_ppm = 2000
lifespan_years = 0
max_alive = 2
flare = false
hoard_permille = 10
raid_phrases = ["came up out of the stone beneath", "burrowed into the halls of", "fell upon"]
slayer_epithet = "Deepbane"
)";

inline constexpr std::string_view MONSTER_YSGARTH_TOML = R"(# Monster kind (spec P5-07 4.1): a deep horror, woken only from a mine. Generator 0.29.0.
# Its id sorts after wyvern so that the six earlier kinds keep their ids.
[monster.ysgarth]
name = "Ysgarth"
noun = "a host of eyeless things from under the stone"
grouped = true
wake = "deep"
biomes = []
wake_rate = 350
naming_culture = "dwarven"
name_forms = ["The Eyeless of {root}", "The Swarm Beneath {root}"]
lair_forms = ["The Warrens of {root}", "The Nest of {root}"]
might = 2500
range_hops = 1
raid_permille = 100
toll_min_permille = 20
toll_max_permille = 80
kill_max = 150
overruns = []
overrun_permille = 400
active_years = 100
rewake_ppm = 3000
lifespan_years = 0
max_alive = 2
flare = false
hoard_permille = 2
raid_phrases = ["swarmed out of the dark upon", "crept up the deep ways into", "fell upon"]
slayer_epithet = "the Lightbearer"
)";

inline constexpr std::string_view MONSTER_ZHAKUL_TOML = R"(# Monster kind (spec P5-07 4.1): a deep horror, woken only from a mine. Generator 0.29.0.
# Its id sorts after wyvern so that the six earlier kinds keep their ids.
[monster.zhakul]
name = "Zhakul"
noun = "a fire that walks in the roots of the world"
grouped = false
wake = "deep"
biomes = []
wake_rate = 150
naming_culture = "dwarven"
name_forms = ["{root} the Unquenched", "{root} Flame-Under-Stone", "{root}"]
lair_forms = ["The Burning Deep of {root}", "{root}'s Furnace"]
might = 7000
range_hops = 1
raid_permille = 40
toll_min_permille = 50
toll_max_permille = 150
kill_max = 500
overruns = []
overrun_permille = 800
active_years = 30
rewake_ppm = 1000
lifespan_years = 0
max_alive = 1
flare = false
hoard_permille = 20
raid_phrases = ["came up burning beneath", "set the deep halls aflame under", "fell in fire upon"]
slayer_epithet = "Flamebreaker"
)";

// The files of the two kinds as "<directory>/<file name>" -> text.
inline const std::vector<std::pair<std::string, std::string_view>>& monster_files() {
    static const std::vector<std::pair<std::string, std::string_view>> v = {
        {"beasts/bear.toml", BEAST_BEAR_TOML},
        {"beasts/boar.toml", BEAST_BOAR_TOML},
        {"beasts/deer.toml", BEAST_DEER_TOML},
        {"beasts/giant_spider.toml", BEAST_GIANT_SPIDER_TOML},
        {"beasts/wolf.toml", BEAST_WOLF_TOML},
        {"monsters/dragon.toml", MONSTER_DRAGON_TOML},
        {"monsters/giant.toml", MONSTER_GIANT_TOML},
        {"monsters/ogre.toml", MONSTER_OGRE_TOML},
        {"monsters/troll.toml", MONSTER_TROLL_TOML},
        {"monsters/undead.toml", MONSTER_UNDEAD_TOML},
        {"monsters/wyvern.toml", MONSTER_WYVERN_TOML},
        {"monsters/ymrach.toml", MONSTER_YMRACH_TOML},
        {"monsters/ysgarth.toml", MONSTER_YSGARTH_TOML},
        {"monsters/zhakul.toml", MONSTER_ZHAKUL_TOML},
    };
    return v;
}

// --- history.relations (P5-03 4.2, P5-03a 10.1; generator 0.25.0, the calibrated values) ---

inline constexpr std::string_view RELATIONS_TOML = R"(# Race relations, mixed settlements and migrations (spec P5-03 4.2).
# Generator 0.24.0; P5-03a 10.1 (generator 0.25.0).
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
# P5-04a 7.2, 8 (generator 0.28.0), in this order.
turn_cooldown_years = 800
expel_settled_years = 800
expel_fresh_years = 150
)";

// --- history.conflict (P3-06 12.1, the listing; generator 0.16.0; r9 20.9 from 0.18.0) ---

inline constexpr std::string_view CONFLICT_TOML = R"([history.conflict]
camp = { good_good = 400, good_neutral = -150, good_evil = -700, neutral_neutral = 100, neutral_evil = -250, evil_evil = 300 }
enmity_max = -500
# P5-03a calibration (generator 0.25.0): 200 -> 300, restoring the wars that peace between neighbours removed.
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
# P5-07 4.3 (generator 0.29.0): per depth level of the deepest open mine.
forge_depth_permille = 15
)";

// --- history.peoples (P3-05 12.1, the listing; generator 0.15.0). The
// fourteen report entries of kinds 6-19 above are P3-05 11's, appended in
// kind id order (the spec gives no file listing: an assumption shared with
// p3_data_reference.py) -----------------------------------------------------------

// P5-04 4.2 (generator 0.26.0): history.alliances at the calibrated values.
inline constexpr std::string_view FIGURES_TOML = R"(# P5-06 4.4 (generator 0.27.0): figures. Rulers named by deed, their
# reigns and tempers; generals, heroes, exiles and casters; epithets earned
# by deeds.
[history.figures]
name_rulers = true
reign_min_permille = 500
reign_span_permille = 1000
accession_age_permille = 250
accession_span_permille = 250
temper_strain_permille = { steady = 1000, warlike = 2000, peaceable = 0, grasping = 1000, just = 1000 }
temper_allied_permille = { steady = 300, warlike = 300, peaceable = 300, grasping = 3000, just = 150 }
temper_spoils_permille = { steady = 0, warlike = 0, peaceable = 0, grasping = -400, just = 400 }
short_reign_permille = 500
short_reign_split_permille = 100
exile_permille = 250
exile_hops = 3
general_scale_permille = 850
skill_min = 600
skill_max = 900
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
  { role = "ruler", deed = "reign_permille", min = 1400, substantial = 1480, text = "the Old" },
  { role = "ruler", deed = "founded_years", min = 1, substantial = 3000, text = "the Founder" },
  { role = "general", deed = "battles_unbeaten", min = 3, substantial = 6, text = "the Unbeaten" },
  { role = "general", deed = "battles_won", min = 5, substantial = 10, text = "the Hammer" },
  { role = "hero", deed = "slew_might", min = 1, substantial = 1200, text = "{slayer}" },
  { role = "claimant", deed = "exiled", min = 1, substantial = 0, text = "the Exile" },
  { role = "caster", deed = "blights", min = 1, substantial = 1, text = "the Accursed" },
  { role = "caster", deed = "books", min = 3, substantial = 0, text = "the Learned" },
]
epithet_repeat_max = 2
# P5-04a 14.2a (generator 0.28.0), in this order.
tower_spacing_permille = 30
hero_return_years = 100
# P5-08 12.6 (generator 0.30.0): a tower without a master lapses after these years (0: never).
tower_lapse_years = 200
)";

inline constexpr std::string_view ALLIANCES_TOML = R"(# Alliances, trade and the dwarves' mines and arms (spec P5-04 4.2).
# Generator 0.26.0.
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
withheld_grudge = 150
withheld_to_break = 2
mine_permille = 40
mine_ore_min = 1
mine_settlement_min = 200
arms_trade_permille = 2000
mine_forms = ["The Mines of {settlement}", "The Delvings of {settlement}"]
# P5-04a 3.7, 4.4, 5 (generator 0.28.0), in this order.
spoils_large_share = 475
spoils_large_part_permille = 401
soured_renew_permille = 250
soured_join_permille = 500
weary_years = 50
join_weary_permille = 750
join_busy_permille = 150
join_fear_floor = 600
join_warmth_floor = 700
grasping_join_permille = 600
join_claim_permille = 500
excused_memory = 15
refuse_grudge = 80
betray_cooled_disposition = 0
betray_weak_permille = 500
# P5-08 12.5 (generator 0.30.0): written renewals per alliance (0: no cap).
renew_written_max = 1
)";

inline constexpr std::string_view PEOPLES_TOML = R"([history.peoples]
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
# P5-03 11.1, 11.2 (generator 0.24.0).
abandon_reach_hops = 0
abandon_loss_min_permille = 50
abandon_loss_max_permille = 200
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

// P5-08 6.7 (generator 0.30.0): the five naming fields of cults, shrines and temples of each culture.
inline std::string_view cult_forms_of(std::string_view id) {
    if (id == "dwarven") {
        return R"(cult_forms = ["The Hammer of {god}", "The Oathsworn of {god}"]
temple_forms = ["The Hall of {god}", "The Deep Shrine of {god}"]
sanctum_forms = ["The Sealed Hall of {god}"]
sacred_forms = ["The Hallowed Stone of {god}"]
profane_forms = ["The Shunned Delving of {god}"]
)";
    }
    if (id == "elven") {
        return R"(cult_forms = ["The Children of {god}", "The Starward Circle of {god}"]
temple_forms = ["The House of {god}", "The White Fane of {god}"]
sanctum_forms = ["The Hidden Fane of {god}"]
sacred_forms = ["The Blessed Grove of {god}"]
profane_forms = ["The Withered Grove of {god}"]
)";
    }
    if (id == "faerie") {
        return R"(cult_forms = ["The Dreamers of {god}", "The Hidden Ring of {god}"]
temple_forms = ["The Bower of {god}", "The Moon-Glade of {god}"]
sanctum_forms = ["The Lost Bower of {god}"]
sacred_forms = ["The Shining Ring of {god}"]
profane_forms = ["The Black Ring of {god}"]
)";
    }
    if (id == "goblin") {
        return R"(cult_forms = ["The Teeth of {god}", "The Grovellers of {god}"]
temple_forms = ["The Pit of {god}", "The Bone-House of {god}"]
sanctum_forms = ["The Secret Pit of {god}"]
sacred_forms = ["The Fat Stone of {god}"]
profane_forms = ["The Stinking Ground of {god}"]
)";
    }
    if (id == "human") {
        return R"(cult_forms = ["The Brotherhood of {god}", "The Order of {god}"]
temple_forms = ["The Temple of {god}", "The Sanctuary of {god}"]
sanctum_forms = ["The Hidden Sanctum of {god}"]
sacred_forms = ["The Holy Ground of {god}"]
profane_forms = ["The Cursed Ground of {god}"]
)";
    }
    if (id == "orcish") {
        return R"(cult_forms = ["The Blood of {god}", "The Howlers of {god}"]
temple_forms = ["The Blood-Hall of {god}", "The War-Fane of {god}"]
sanctum_forms = ["The Black Cave of {god}"]
sacred_forms = ["The Red Stone of {god}"]
profane_forms = ["The Ashen Field of {god}"]
)";
    }
    return "";
}

// The file text of culture `id`: "[culture.<id>]", race, name, a blank line,
// "[culture.<id>.naming]" (line 5) and the naming block.
inline std::string culture_toml(const CultureListing& c) {
    return "[culture." + std::string(c.id) + "]\nrace = \"" + std::string(c.race) + "\"\nname = \"" +
           std::string(c.name) + "\"\n\n[culture." + std::string(c.id) + ".naming]\n" + std::string(c.naming) +
           std::string(cult_forms_of(c.id));
}

// --- The files as a map: "<directory>/<file name>" -> text --------------------------

using Files = std::map<std::string, std::string>;

// P5-02 (generator 0.23.0): the required kinds beast and monster and the fixed id history.monsters, which every
// hand-made set of files needs from 0.23.0 on. A monster kind is added only when the files hold its naming
// culture (the named check monster_culture, P5-02 4.2), so that a set with a subset of the cultures still loads.
inline void prune_monster_files(Files& f);

inline void add_monster_files(Files& f) {
    f["history/monsters.toml"] = std::string(MONSTERS_TOML);
    for (const auto& [path, text] : monster_files()) {
        const std::string_view key = "naming_culture = \"";
        const std::size_t at = text.find(key);
        if (at != std::string_view::npos) {
            const std::size_t from = at + key.size();
            const std::string culture(text.substr(from, text.find('"', from) - from));
            if (f.count("cultures/" + culture + ".toml") == 0U) {
                continue;
            }
        }
        f[path] = std::string(text);
    }
}

// Removes the monster files whose naming culture the files no longer hold (a test that erases a culture).
inline void prune_monster_files(Files& f) {
    for (const auto& [path, text] : monster_files()) {
        const std::string_view key = "naming_culture = \"";
        const std::size_t at = text.find(key);
        if (at == std::string_view::npos) continue;
        const std::size_t from = at + key.size();
        const std::string culture(text.substr(from, text.find('"', from) - from));
        if (f.count("cultures/" + culture + ".toml") == 0U) f.erase(path);
    }
}


// P5-08 (generator 0.30.0): history.pantheon and the nine gods, the shipped files.
inline constexpr std::string_view PANTHEON_TOML = R"(# P5-08 4.2 (generator 0.30.0): the pantheon. Regard from deeds, the gods'
# acts (bless, curse, champion, monster, mark), their effects and the cults.
[history.pantheon]
activity_permille = { distant = 300, normal = 1000, meddling = 2500 }
regard_max = 400
regard_fade_per_century = 8
regard_memory_years = 300
contributions_max = 8
act_threshold = 70
act_permille = 14
god_rest_years = 20
pair_rest_years = 80
flare_regard = 130
flare_permille = 100
bless_years = 50
curse_years = 50
plenty_growth_permille = 800
withering_growth_permille = 500
valour_permille = 50
defeat_permille = 60
pestilence_toll_permille = 50
send_hops = 3
sacred_growth_permille = 200
profane_years = 300
champion_epithet = "Chosen of {god}"
cult_found_permille = 400
cult_fear_permille = 700
cult_spread_permille = 20
affinity_permille = { same = 1000, near = 500, opposed = 100 }
adopt_regard = 50
adopt_permille = 15
suppress_permille = 8
temper_adopt_permille = { steady = 1000, warlike = 500, peaceable = 1500, grasping = 700, just = 1000 }
temper_suppress_permille = { steady = 1000, warlike = 1500, peaceable = 500, grasping = 1000, just = 1300 }
faith_points = 60
rival_faith_points = 40
suppress_points = 120
hide_permille = 800
sanctum_hops = 3

[history.pantheon.creator]
favour = { bless = 400, champion = 450, mark = 150 }
wrath = { curse = 700, monster = 0, mark = 300 }
bless = "plenty"
curse = "pestilence"
monsters = []
deeds = [
    { kind = "sacked", role = "attacker", points = -60, who = "any", against = "defender" },
    { kind = "settlement_captured", role = "attacker", points = -25, who = "any", against = "defender" },
    { kind = "civ_conquered", role = "conqueror", points = -120, who = "any", against = "civilization" },
    { kind = "war_won", role = "winner", points = 30, who = "kin", against = "" },
    { kind = "monster_slain", role = "slayer", points = 25, who = "kin", against = "" },
    { kind = "monster_slain_by_hero", role = "slayer", points = 35, who = "kin", against = "" },
    { kind = "became_city", role = "civilization", points = 20, who = "kin", against = "" },
    { kind = "deep_sealed", role = "civilization", points = 40, who = "kin", against = "" },
    { kind = "refuge_held", role = "defender", points = 30, who = "kin", against = "" },
]
)";
inline constexpr std::string_view GOD_CONQUEST_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.conquest]
name_culture = "orcish"
camp = "evil"
domain = "war"
title = "the Red Banner"
favour = { bless = 120, champion = 1000, mark = 30 }
wrath = { curse = 700, monster = 0, mark = 300 }
bless = "valour"
curse = "defeat"
monsters = []
deeds = [
    { kind = "civ_conquered", role = "conqueror", points = 100, who = "kin", against = "" },
    { kind = "war_won", role = "winner", points = 30, who = "kin", against = "" },
    { kind = "sacked", role = "attacker", points = 15, who = "kin", against = "" },
    { kind = "peace", role = "loser", points = -30, who = "kin", against = "" },
    { kind = "tribute", role = "payer", points = -20, who = "kin", against = "" },
    { kind = "truce", role = "parties", points = -20, who = "kin", against = "" },
]
)";
inline constexpr std::string_view GOD_CRAFT_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.craft]
name_culture = "dwarven"
camp = "neutral"
domain = "craft"
title = "the Hammer Under the Mountain"
favour = { bless = 150, champion = 1000, mark = 45 }
wrath = { curse = 700, monster = 0, mark = 300 }
bless = "valour"
curse = "defeat"
monsters = []
deeds = [
    { kind = "mine_opened", role = "civilization", points = 25, who = "kin", against = "" },
    { kind = "delving_deepened", role = "civilization", points = 15, who = "kin", against = "" },
    { kind = "arms_sold", role = "seller", points = 10, who = "any", against = "" },
    { kind = "deep_sealed", role = "civilization", points = 30, who = "any", against = "" },
    { kind = "deep_woken", role = "civilization", points = -40, who = "any", against = "" },
]
)";
inline constexpr std::string_view GOD_DEATH_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.death]
name_culture = "goblin"
camp = "evil"
domain = "death"
title = "the Hollow Keeper"
favour = { bless = 120, champion = 1000, mark = 180 }
wrath = { curse = 300, monster = 350, mark = 200 }
bless = "valour"
curse = "pestilence"
monsters = ["undead"]
deeds = [
    { kind = "sacked", role = "attacker", points = 20, who = "any", against = "" },
    { kind = "blight", role = "civilization", points = 30, who = "any", against = "" },
    { kind = "battle_led", role = "winner", points = 5, who = "any", against = "" },
    { kind = "monster_slain", role = "slayer", points = -30, who = "any", against = "" },
    { kind = "monster_slain_by_hero", role = "slayer", points = -40, who = "any", against = "" },
]
)";
inline constexpr std::string_view GOD_HARVEST_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.harvest]
name_culture = "faerie"
camp = "good"
domain = "harvest"
title = "the Giver of Harvests"
favour = { bless = 210, champion = 1000, mark = 90 }
wrath = { curse = 700, monster = 0, mark = 300 }
bless = "plenty"
curse = "withering"
monsters = []
deeds = [
    { kind = "became_town", role = "civilization", points = 20, who = "kin", against = "" },
    { kind = "became_city", role = "civilization", points = 30, who = "kin", against = "" },
    { kind = "trade_opened", role = "partner_a", points = 20, who = "kin", against = "" },
    { kind = "trade_opened", role = "partner_b", points = 20, who = "kin", against = "" },
    { kind = "blight", role = "civilization", points = -80, who = "any", against = "" },
    { kind = "sacked", role = "attacker", points = -30, who = "any", against = "" },
]
)";
inline constexpr std::string_view GOD_LORE_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.lore]
name_culture = "elven"
camp = "good"
domain = "lore"
title = "the Lamp of the Wise"
favour = { bless = 120, champion = 1000, mark = 75 }
wrath = { curse = 600, monster = 0, mark = 400 }
bless = "plenty"
curse = "withering"
monsters = []
deeds = [
    { kind = "tower_founded", role = "civilization", points = 60, who = "any", against = "" },
    { kind = "book_written", role = "civilization", points = 40, who = "any", against = "" },
    { kind = "blight", role = "civilization", points = -60, who = "any", against = "" },
]
)";
inline constexpr std::string_view GOD_OATHS_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.oaths]
name_culture = "elven"
camp = "good"
domain = "oaths"
title = "the Keeper of Oaths"
favour = { bless = 90, champion = 1000, mark = 60 }
wrath = { curse = 700, monster = 0, mark = 300 }
bless = "valour"
curse = "defeat"
monsters = []
deeds = [
    { kind = "alliance_formed", role = "ally_a", points = 30, who = "any", against = "" },
    { kind = "alliance_formed", role = "ally_b", points = 30, who = "any", against = "" },
    { kind = "spoils_shared", role = "principal", points = 25, who = "any", against = "" },
    { kind = "ally_joined_war", role = "ally", points = 20, who = "any", against = "" },
    { kind = "alliance_betrayed", role = "betrayed", points = 40, who = "any", against = "" },
    { kind = "alliance_betrayed", role = "betrayer", points = -150, who = "any", against = "" },
    { kind = "spoils_withheld", role = "principal", points = -50, who = "any", against = "" },
    { kind = "ally_refused", role = "ally", points = -25, who = "any", against = "" },
]
)";
inline constexpr std::string_view GOD_RUIN_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.ruin]
name_culture = "goblin"
camp = "evil"
domain = "ruin"
title = "the Burner of Halls"
favour = { bless = 120, champion = 1000, mark = 45 }
wrath = { curse = 400, monster = 350, mark = 200 }
bless = "valour"
curse = "pestilence"
monsters = ["dragon", "giant"]
deeds = [
    { kind = "sacked", role = "attacker", points = 40, who = "kin", against = "" },
    { kind = "settlement_captured", role = "attacker", points = 15, who = "kin", against = "" },
    { kind = "truce", role = "parties", points = -40, who = "kin", against = "" },
    { kind = "trade_opened", role = "partner_a", points = -10, who = "kin", against = "" },
    { kind = "trade_opened", role = "partner_b", points = -10, who = "kin", against = "" },
    { kind = "became_city", role = "civilization", points = -15, who = "foe", against = "" },
]
)";
inline constexpr std::string_view GOD_WAR_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.war]
name_culture = "human"
camp = "neutral"
domain = "war"
title = "the Shield of Battles"
favour = { bless = 120, champion = 1000, mark = 30 }
wrath = { curse = 700, monster = 0, mark = 300 }
bless = "valour"
curse = "defeat"
monsters = []
deeds = [
    { kind = "battle", role = "winner", points = 10, who = "any", against = "" },
    { kind = "battle_led", role = "winner", points = 15, who = "any", against = "" },
    { kind = "allied_battle", role = "winner", points = 10, who = "any", against = "" },
    { kind = "war_won", role = "winner", points = 25, who = "any", against = "" },
    { kind = "ally_refused", role = "ally", points = -30, who = "any", against = "" },
    { kind = "ally_withdrew", role = "ally", points = -30, who = "any", against = "" },
]
)";
inline constexpr std::string_view GOD_WILD_TOML = R"(# P5-08 4.1 (generator 0.30.0): a god of the pantheon.
[god.wild]
name_culture = "human"
camp = "neutral"
domain = "wild"
title = "the Hunter in the Green"
favour = { bless = 150, champion = 1000, mark = 150 }
wrath = { curse = 300, monster = 350, mark = 200 }
bless = "plenty"
curse = "withering"
monsters = ["troll", "ogre", "wyvern"]
deeds = [
    { kind = "colony_founded", role = "founder", points = -15, who = "any", against = "" },
    { kind = "became_city", role = "civilization", points = -20, who = "any", against = "" },
    { kind = "mine_opened", role = "civilization", points = -30, who = "any", against = "" },
    { kind = "monster_slain", role = "slayer", points = -40, who = "any", against = "" },
    { kind = "settlement_abandoned", role = "civilization", points = 15, who = "any", against = "" },
    { kind = "outpost_abandoned", role = "civilization", points = 10, who = "any", against = "" },
]
)";

inline void add_pantheon_files(std::map<std::string, std::string>& f) {
    f["history/pantheon.toml"] = std::string(PANTHEON_TOML);
    f["gods/conquest.toml"] = std::string(GOD_CONQUEST_TOML);
    f["gods/craft.toml"] = std::string(GOD_CRAFT_TOML);
    f["gods/death.toml"] = std::string(GOD_DEATH_TOML);
    f["gods/harvest.toml"] = std::string(GOD_HARVEST_TOML);
    f["gods/lore.toml"] = std::string(GOD_LORE_TOML);
    f["gods/oaths.toml"] = std::string(GOD_OATHS_TOML);
    f["gods/ruin.toml"] = std::string(GOD_RUIN_TOML);
    f["gods/war.toml"] = std::string(GOD_WAR_TOML);
    f["gods/wild.toml"] = std::string(GOD_WILD_TOML);
}

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
    f["history/conflict.toml"] = std::string(CONFLICT_TOML);
    f["history/engine.toml"] = std::string(ENGINE_TOML);
    f["history/peoples.toml"] = std::string(PEOPLES_TOML);
    f["history/renaming.toml"] = std::string(RENAMING_TOML);
    f["history/relations.toml"] = std::string(RELATIONS_TOML);   // P5-03 4.2
    f["history/report.toml"] = std::string(REPORT_TOML);
    f["history/figures.toml"] = std::string(FIGURES_TOML);   // P5-06
    f["history/alliances.toml"] = std::string(ALLIANCES_TOML);   // P5-04 4.2
    add_monster_files(f);   // P5-02 4.1 to 4.3
    add_pantheon_files(f);   // P5-08 4.1, 4.2
    return f;
}

// The kind whose directory holds a file of `files` ("races/x" -> "race").
inline std::string_view kind_of_directory(std::string_view directory) {
    if (directory == "races") return "race";
    if (directory == "cultures") return "culture";
    if (directory == "history") return "history";
    if (directory == "beasts") return "beast";      // P5-02 4.1
    if (directory == "monsters") return "monster";  // P5-02 4.2
    if (directory == "gods") return "god";          // P5-08 4.1
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
    std::int64_t density_permille;  // P3-05 12.2 item 1 (P3-01 r6)
    std::int64_t fertility, forage, timber, ore, fresh_water;  // site_weights
    std::int64_t cell_ore, cell_height;  // cell_weights (P3-01 r8 (1), generator 0.17.0)
    std::vector<RaceBiome> biomes;
    std::int64_t upland, coast, expansion, aggression;
    std::int64_t prowess_permille;  // P3-06 12.2 item 1 (P3-01 r7, generator 0.16.0)
    // P3-01 r9 3.2 / P3-06 r9 20.12 (generator 0.18.0, P3-09)
    std::int64_t muster_permille, conceal_min, home_defense_permille;
    std::vector<std::string_view> settlement_kinds;
    std::string_view culture;
    // P3-01 r11 3.4 and 3.5 (generator 0.19.0, P3-10): cell_weights inland, port, habitability (after height in
    // the record), odds_min_permille and awakening_quarrel (after home_defense_permille in the record). Kept at
    // the end of the struct so the aggregate initializers above them stay as they were.
    std::int64_t cell_inland, cell_port, cell_habitability;
    std::int64_t odds_min_permille, awakening_quarrel;
    std::int64_t hidden_years;  // P3-01 r13 3.6 (P3-10 second round): after awakening_quarrel in the record
    std::int64_t awakening_population;  // P3-01 r14 3.7 (P3-11, generator 0.20.0): after hidden_years
};

// The six races of P3-01 3.1 in canonical id order. Raw values of the
// shares: "0.35" 1503238554, "0.55" 2362232013, "0.70" 3006477107, "0.83"
// 3564822856, "0.93" 3994319585 (core_reference.parse_decimal).
inline const std::vector<Race>& races() {
    static const std::vector<Race> v = {
        {"dwarf", 3, "Dwarf", "Dwarves", "Dwarven", "neutral", 2362232013, 300, 70, 5, 700, 10, 10, 10, 60, 10, 400, 600,
         {{"alpine", 300}, {"highland_moor", 300}, {"heath", 200}, {"temperate_forest", -100}, {"wet_woodland", -300},
          {"marsh", -400}, {"fen", -400}},
         800, -200, 250, 300, 1400, 1000, 1001, 0, {"hamlet", "village", "town", "city", "fortress"}, "dwarven", 0, 0, 500, 700, 0, 300, 0},
        {"elf", 1, "Elf", "Elves", "Elven", "good", 0, 700, 150, 3, 300, 20, 35, 35, 0, 10, 0, 0,
         {{"temperate_forest", 500}, {"wet_woodland", 300}, {"grassland", 0}, {"heath", -200}, {"steppe", -300},
          {"highland_moor", -300}, {"alpine", -600}},
         -300, 0, 200, 150, 1600, 1000, 1001, 1500, {"hamlet", "village", "town", "city"}, "elven", 0, 1000, 1000, 700, 0, 300, 0},
        {"faerie", 4, "Faerie", "Faeries", "Faerie", "good", 3006477107, 400, 100, 4, 250, 0, 60, 20, 0, 20, 0, 0,
         {{"temperate_forest", 800}, {"wet_woodland", 800}, {"fen", 300}, {"marsh", 200}, {"highland_moor", 100},
          {"heath", 0}, {"grassland", -300}, {"alpine", -400}, {"steppe", -400}},
         200, -500, 100, 50, 1200, 1000, 400, 0, {"hamlet", "village"}, "faerie", 400, 0, 1000, 700, 0, 300, 0},  // home 0: P3-01 r10 3.3
        {"goblin", 2, "Goblin", "Goblins", "Goblin", "evil", 1503238554, 40, 15, 20, 900, 40, 20, 10, 30, 0, 0, 0,
         {{"heath", 200}, {"highland_moor", 200}, {"steppe", 150}, {"grassland", 100}, {"marsh", 100}, {"alpine", -400},
          {"temperate_forest", -200}, {"wet_woodland", -300}},
         100, -100, 600, 600, 800, 1000, 1001, 0, {"hamlet", "village", "town", "fortress"}, "goblin", 0, 1000, 1000, 700, 0, 300, 0},
        {"human", 5, "Human", "Humans", "Human", "neutral", 3564822856, 70, 25, 15, 1000, 60, 10, 10, 5, 15, 0, 0,
         {{"grassland", 300}, {"steppe", 100}, {"temperate_forest", 100}, {"marsh", -100}, {"heath", -100},
          {"fen", -200}, {"highland_moor", -200}, {"alpine", -500}},
         -300, 200, 700, 400, 1000, 1000, 1001, 0, {"hamlet", "village", "town", "city", "fortress"}, "human", 0, 1000, 1000, 700, 0, 300, 0},
        {"orc", 6, "Orc", "Orcs", "Orcish", "evil", 3994319585, 50, 18, 18, 800, 35, 25, 10, 30, 0, 0, 0,
         {{"steppe", 400}, {"heath", 200}, {"grassland", 200}, {"highland_moor", 100}, {"temperate_forest", -100},
          {"marsh", -100}, {"fen", -200}, {"wet_woodland", -200}, {"alpine", -300}},
         100, -100, 600, 900, 1300, 4000, 1001, 0, {"hamlet", "village", "town", "fortress"}, "orcish", 0, 1000, 1000, 500, 1, 100, 1200},
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
        "generation_years", "growth_permille", "density_permille", "site_weights", "cell_weights", "biomes", "upland", "coast",          "expansion",
        "aggression",      "prowess_permille", "muster_permille", "conceal_min", "home_defense_permille",
        "odds_min_permille", "awakening_quarrel",  // P3-01 r11 3.5 (generator 0.19.0)
        "hidden_years",                            // P3-01 r13 3.6 (P3-10 second round)
        "awakening_population",                    // P3-01 r14 3.7 (P3-11, generator 0.20.0)
        "settlement_kinds", "culture", "stances",    // stances: P5-03 4.1 (generator 0.24.0)
        "origin_allies", "ally_join_permille", "mining_permille",   // P5-04 4.1 (generator 0.26.0)
        "temper_weights", "general_permille", "hero_permille", "magic_permille",   // P5-06 (generator 0.27.0)
        "delving_permille"};   // P5-07 4.1 (generator 0.29.0): the last field, optional (absent = 0)
    return v;
}
// P3-01 r8 (1): record race_cell_weights.
inline const std::vector<std::string_view>& cell_weight_fields() {
    static const std::vector<std::string_view> v = {"ore", "height", "inland", "port", "habitability"};  // r11 3.4
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
    static const std::vector<std::string_view> v = {"onsets",          "nuclei",      "codas",     "patterns", "god_patterns",
                                                     "min_letters",     "max_letters", "forbidden", "god_forms",
                                                     "civilization_forms", "settlement_forms",
                                                     "site_forms",   // P5-02 4.4
                                                     "person_forms", "tower_forms", "library_forms", "book_titles",
                                                     "regnal_permille"};   // P5-06 (generator 0.27.0)
    return v;
}
inline const std::vector<std::string_view>& chronology_fields() {
    static const std::vector<std::string_view> v = {"awakening_jitter_frac", "awakening_min_gap_years",
                                                    "age_name_form"};
    return v;
}

}  // namespace history_data
