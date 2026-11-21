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
// quiet_min_years 50 after quiet_events_max. Mirrors SPEC_FILES_019.

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
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "orcish"
)";

// --- Naming blocks (P3-02 5, verbatim) ---------------------------------------------

inline constexpr std::string_view DWARVEN_NAMING = R"(onsets = ["d", "b", "th", "k", "g", "br", "dr", "gr", "t", "h", "kh", "n", "m", "thr"]
nuclei = ["a", "u", "o", "i", "ai", "ou"]
codas = ["r", "n", "m", "k", "d", "rn", "rd", "rk", "mm", "nd", "rin", "li", "din"]
patterns = ["ONC", "ONCONC", "ONCONC", "ONCON"]
god_patterns = ["ONCONC", "ONCONC", "ONCON"]
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
god_patterns = ["ONONC", "ONONC", "ONCON", "ONONON", "NCONC"]
min_letters = 4
max_letters = 11
forbidden = ["aea", "iei", "iae", "eae", "aa", "ii", "ee", "thth", "lll", "nnn", "ea", "ia"]
god_forms = ["{root}"]
civilization_forms = ["The Court of {root}", "The Realm of {root}", "The House of {root}"]
settlement_forms = ["{root}", "{root}", "{root}", "{root}dell"]
)";

inline constexpr std::string_view FAERIE_NAMING = R"(onsets = ["", "p", "f", "w", "l", "s", "t", "b", "sh", "wh", "fl", "tw", "m"]
nuclei = ["i", "ee", "ie", "ia", "a", "y", "ae", "ou", "o"]
codas = ["", "", "ll", "n", "sh", "th", "p", "sk", "ss"]
patterns = ["ONON", "ONONON", "ONC", "ONONC", "ONCON"]
god_patterns = ["ONON", "ONONON", "ONONC", "ONCON"]
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
god_patterns = ["ONCONC", "ONC-ONC", "ONONC"]
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
god_patterns = ["ONCONC"]
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
god_patterns = ["ONC'ONC", "ONCONC", "NC'ONC"]
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

// --- history.engine and history.report (P3-04 4.1, 4.2; every root carries
// all fixed ids of kind history, P3-01 5) -----------------------------------------

inline constexpr std::string_view ENGINE_TOML = R"([history.engine]
quiet_step_years = 25
quiet_events_max = 3
quiet_min_years = 50
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
  { kind = "became_city", salience = 45, template = "{settlement} grew into a city of {population}." },
  { kind = "settlement_dwindled", salience = 15, template = "{@causes:famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,battle=war,battle_at_pass=war,tribute=tribute,colony_founded=emigration|Weakened by |, }{settlement} dwindled to {population} souls." },
  { kind = "famine", salience = 25, template = "Famine struck {settlement}; {dead} died." },
  { kind = "plague", salience = 45, template = "Plague broke out in {settlement}; {dead} died." },
  { kind = "plague_spread", salience = 25, template = "The plague spread to {settlement}; {dead} died." },
  { kind = "succession_crisis", salience = 40, template = "A succession crisis at {@settlement} shook {civilization}." },
  { kind = "civ_seceded", salience = 70, template = "{civilization} broke away from {from} with {settlements} {settlements?settlement|settlements}." },
  { kind = "settlement_abandoned", salience = 55, template = "{@causes:famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,battle=war,battle_at_pass=war,tribute=tribute|Weakened by |, }{settlement} was abandoned; {survivors?no one survived|its one survivor was scattered|its # survivors were scattered}." },
  { kind = "capital_moved", salience = 50, template = "The seat of {civilization} moved to {settlement}." },
  { kind = "civ_ended", salience = 85, template = "{civilization} came to an end." },
  { kind = "ruin_faded", salience = 10, template = "The ruins of {settlement} faded from memory." },
  { kind = "claim", salience = 35, template = "{claimant} laid claim to {@region}, held by {holder}." },
  { kind = "war_declared", salience = 70, template = "{attacker} declared war on {defender} over {@region}." },
  { kind = "battle", salience = 30, template = "{winner} defeated {loser} in {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "battle_at_pass", salience = 35, template = "{winner} defeated {loser} at the pass into {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}." },
  { kind = "siege", salience = 25, template = "{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}." },
  { kind = "sacked", salience = 75, template = "{attacker} sacked {@settlement} of {defender}; {dead} died and {survivors} fled." },
  { kind = "settlement_captured", salience = 60, template = "{attacker} took {@settlement} from {defender}; {dead} died{fled?| and one fled| and # fled}." },
  { kind = "peace", salience = 60, template = "{loser} sued {winner} for peace after {years} {years?year|years} of war." },
  { kind = "truce", salience = 50, template = "{parties:all} made peace after {years} {years?year|years} of war, neither the victor." },
  { kind = "region_ceded", salience = 55, template = "{from} ceded {@region} to {to}." },
  { kind = "civ_conquered", salience = 90, template = "{civilization} fell to {conqueror}." },
  { kind = "settlement_abandoned_to", salience = 55, template = "{@causes:famine=famine,plague=plague,plague_outbreak=plague,plague_spread=plague,succession_crisis=strife,battle=war,battle_at_pass=war,tribute=tribute|Weakened by |, }{settlement} was abandoned; {settled} {settled?survivor|survivors} went to {destination}{lost?|, and one was lost|, and # were lost}." },
  { kind = "refugees_settled", salience = 65, template = "The survivors of {origin} fled into hiding and founded {@settlement} in {@region}." },
  { kind = "tribute", salience = 50, template = "{payer} paid tribute to {payee}; {people} {people?was|were} carried off." },
  { kind = "war_won", salience = 60, template = "{winner} prevailed over {loser} after {years} {years?year|years} of war." },
  { kind = "search_abandoned", salience = 40, template = "{searcher} searched {@region} for {hidden} and found no one; the war ended after {years} {years?year|years}." },
  { kind = "refuge_held", salience = 25, template = "{attacker} besieged {@settlement} of {defender}; with {reason?nowhere to flee|too few to flee}, its people held on." },
  { kind = "plague_outbreak", salience = 45, template = "Plague broke out in {settlement}{reached?| and spread to one other settlement| and spread to # other settlements}; {total} died{reached?| in all| in all}." },
  { kind = "quiet_stretch", salience = 20, template = "For {years} {years?year|years}, from {@year} to {@end_year}, the land was quiet{realms?|; one realm held it|, and # realms kept the peace}." },
]
)";

// --- history.conflict (P3-06 12.1, the listing; generator 0.16.0; r9 20.9 from 0.18.0) ---

inline constexpr std::string_view CONFLICT_TOML = R"([history.conflict]
camp = { good_good = 400, good_neutral = -150, good_evil = -700, neutral_neutral = 100, neutral_evil = -250, evil_evil = 300 }
enmity_max = -500
border_friction = 200
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
capital_sacked_salience = 90
capital_captured_salience = 80
)";

// --- history.peoples (P3-05 12.1, the listing; generator 0.15.0). The
// fourteen report entries of kinds 6-19 above are P3-05 11's, appended in
// kind id order (the spec gives no file listing: an assumption shared with
// p3_data_reference.py) -----------------------------------------------------------

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
ruin_fade_years = 1500
colony_new_region_salience = 30
plague_small_salience = 25
plague_major_dead = 1000
crisis_small_salience = 25
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
    f["history/conflict.toml"] = std::string(CONFLICT_TOML);
    f["history/engine.toml"] = std::string(ENGINE_TOML);
    f["history/peoples.toml"] = std::string(PEOPLES_TOML);
    f["history/report.toml"] = std::string(REPORT_TOML);
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
};

// The six races of P3-01 3.1 in canonical id order. Raw values of the
// shares: "0.35" 1503238554, "0.55" 2362232013, "0.70" 3006477107, "0.83"
// 3564822856, "0.93" 3994319585 (core_reference.parse_decimal).
inline const std::vector<Race>& races() {
    static const std::vector<Race> v = {
        {"dwarf", 3, "Dwarf", "Dwarves", "Dwarven", "neutral", 2362232013, 300, 70, 5, 700, 10, 10, 10, 60, 10, 400, 600,
         {{"alpine", 300}, {"highland_moor", 300}, {"heath", 200}, {"temperate_forest", -100}, {"wet_woodland", -300},
          {"marsh", -400}, {"fen", -400}},
         800, -200, 250, 300, 1400, 1000, 1001, 0, {"hamlet", "village", "town", "city", "fortress"}, "dwarven", 0, 0, 500, 700, 0, 300},
        {"elf", 1, "Elf", "Elves", "Elven", "good", 0, 700, 150, 3, 300, 20, 35, 35, 0, 10, 0, 0,
         {{"temperate_forest", 500}, {"wet_woodland", 300}, {"grassland", 0}, {"heath", -200}, {"steppe", -300},
          {"highland_moor", -300}, {"alpine", -600}},
         -300, 0, 200, 150, 1600, 1000, 1001, 1500, {"hamlet", "village", "town", "city"}, "elven", 0, 1000, 1000, 700, 0, 300},
        {"faerie", 4, "Faerie", "Faeries", "Faerie", "good", 3006477107, 400, 100, 4, 250, 0, 60, 20, 0, 20, 0, 0,
         {{"temperate_forest", 800}, {"wet_woodland", 800}, {"fen", 300}, {"marsh", 200}, {"highland_moor", 100},
          {"heath", 0}, {"grassland", -300}, {"alpine", -400}, {"steppe", -400}},
         200, -500, 100, 50, 1200, 1000, 400, 0, {"hamlet", "village"}, "faerie", 400, 0, 1000, 700, 0, 300},  // home 0: P3-01 r10 3.3
        {"goblin", 2, "Goblin", "Goblins", "Goblin", "evil", 1503238554, 40, 15, 20, 900, 40, 20, 10, 30, 0, 0, 0,
         {{"heath", 200}, {"highland_moor", 200}, {"steppe", 150}, {"grassland", 100}, {"marsh", 100}, {"alpine", -400},
          {"temperate_forest", -200}, {"wet_woodland", -300}},
         100, -100, 600, 600, 800, 1000, 1001, 0, {"hamlet", "village", "town", "fortress"}, "goblin", 0, 1000, 1000, 700, 0, 300},
        {"human", 5, "Human", "Humans", "Human", "neutral", 3564822856, 70, 25, 15, 1000, 60, 10, 10, 5, 15, 0, 0,
         {{"grassland", 300}, {"steppe", 100}, {"temperate_forest", 100}, {"marsh", -100}, {"heath", -100},
          {"fen", -200}, {"highland_moor", -200}, {"alpine", -500}},
         -300, 200, 700, 400, 1000, 1000, 1001, 0, {"hamlet", "village", "town", "city", "fortress"}, "human", 0, 1000, 1000, 700, 0, 300},
        {"orc", 6, "Orc", "Orcs", "Orcish", "evil", 3994319585, 50, 18, 18, 800, 35, 25, 10, 30, 0, 0, 0,
         {{"steppe", 400}, {"heath", 200}, {"grassland", 200}, {"highland_moor", 100}, {"temperate_forest", -100},
          {"marsh", -100}, {"fen", -200}, {"wet_woodland", -200}, {"alpine", -300}},
         100, -100, 600, 900, 1300, 4000, 1001, 0, {"hamlet", "village", "town", "fortress"}, "orcish", 0, 1000, 1000, 500, 1, 100},
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
        "settlement_kinds", "culture"};
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
                                                     "civilization_forms", "settlement_forms"};
    return v;
}
inline const std::vector<std::string_view>& chronology_fields() {
    static const std::vector<std::string_view> v = {"awakening_jitter_frac", "awakening_min_gap_years",
                                                    "age_name_form"};
    return v;
}

}  // namespace history_data
