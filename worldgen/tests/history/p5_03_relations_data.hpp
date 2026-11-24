#pragma once

// The data of spec P5-03 (docs/specs/p5-03-race-relations.md r1) applied to the P3-01 listing
// (p3_01_history_data.hpp, the 0.23.0 data): the race field stances (4.1), history.relations (4.2), the new
// history.monsters (9.4) and history.peoples (11.1) fields, raid_phrases (9.5), bold_phrases (13), the hoards of
// 9.2, the monster name forms of 12.1 and 12.2, the report entries of kinds 56 to 62 (8.2, 11.3) and the templates
// of kinds 48 and 53 (9.5, 13). Transcribed from the spec, not read from game/data. Each edit is applied only when
// the listing does not already hold it, so the header keeps working once the listing is regenerated from the
// 0.24.0 files (phase 2, after the engineer's calibration: the values that calibration moves are compared by the
// tests against the listing, not against these).

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../p3_01_history_data.hpp"

namespace p503data {

using history_data::Files;

inline void replace_line(std::string& text, std::string_view head, const std::string& line) {
    const std::size_t a = text.find("\n" + std::string(head));
    if (a == std::string::npos) return;
    const std::size_t b = text.find('\n', a + 1);
    text.replace(a + 1, b - a - 1, line);
}

inline void insert_after_line(std::string& text, std::string_view head, const std::string& lines) {
    if (text.find("\n" + lines.substr(0, lines.find(' ')) + " = ") != std::string::npos) return;   // already there
    std::size_t a = text.find("\n" + std::string(head));
    if (a == std::string::npos && text.rfind(head, 0) == 0) a = 0;
    if (a == std::string::npos) return;
    const std::size_t b = text.find('\n', a + 1);
    text.insert(b + 1, lines + "\n");
}

// 4.2, shipped values in the order of the table.
inline constexpr std::string_view RELATIONS_TOML = R"([history.relations]
hatred_max = -500
friend_min = 150
band_margin = 50
memory_min = -250
memory_max = 250
memory_decay_per_century = 40
war_memory = 25
conquest_memory = 50
sack_memory = 10
capture_memory = 5
expel_memory = 30
shelter_memory = 15
meet_hops = 3
migrate_hops = 2
migrate_min = 40
welcome_min = -200
subject_years = 200
conquest_settlers_permille = 100
drift_permille_per_century = 100
fade_permille = 20
mixed_min_permille = 100
expel_permille = 30
expel_share_max = 500
)";

struct RelField {
    std::string_view name;
    std::int64_t lo, hi, shipped;
};
inline const std::vector<RelField>& relations_fields() {
    static const std::vector<RelField> v = {
        {"hatred_max", -2000, 0, -500},        {"friend_min", 0, 2000, 150},
        {"band_margin", 0, 500, 50},           {"memory_min", -2000, 0, -250},
        {"memory_max", 0, 2000, 250},          {"memory_decay_per_century", 0, 10000, 40},
        {"war_memory", 0, 1000, 25},           {"conquest_memory", 0, 1000, 50},
        {"sack_memory", 0, 1000, 10},          {"capture_memory", 0, 1000, 5},
        {"expel_memory", 0, 1000, 30},         {"shelter_memory", 0, 1000, 15},
        {"meet_hops", -1, 8, 3},               {"migrate_hops", 0, 8, 2},
        {"migrate_min", 1, 100000, 40},        {"welcome_min", -2000, 2001, -200},
        {"subject_years", 0, 100000, 200},     {"conquest_settlers_permille", 0, 1000, 100},
        {"drift_permille_per_century", 0, 1000, 100}, {"fade_permille", 0, 1000, 20},
        {"mixed_min_permille", 1, 1001, 100},  {"expel_permille", 0, 1000, 30},
        {"expel_share_max", 0, 1000, 500}};
    return v;
}

// 9.4, after prey_might_max_permille.
inline const std::vector<RelField>& monsters_fields_024() {
    static const std::vector<RelField> v = {{"raid_chain_years", 1, 100000, 100}, {"toll_over_permille", 0, 1000, 250},
                                            {"hunt_raid_permille", 0, 1000, 60},  {"hunt_max_permille", 0, 1000, 400},
                                            {"fear_raids", 1, 100, 3},            {"fear_years", 1, 100000, 50},
                                            {"fear_permille", 0, 1000, 300},      {"overrun_people_ref", 1, 1000000, 800}};
    return v;
}
// 11.1, after ghost_max_population.
inline const std::vector<RelField>& peoples_fields_024() {
    static const std::vector<RelField> v = {{"abandon_reach_hops", 0, 16, 2},
                                            {"abandon_loss_min_permille", 0, 1000, 50},
                                            {"abandon_loss_max_permille", 0, 1000, 200},
                                            {"ghost_fade_years", 0, 100000, 0}};
    return v;
}

inline std::string field_lines(const std::vector<RelField>& fs) {
    std::string s;
    for (const RelField& f : fs) s += (s.empty() ? "" : "\n") + std::string(f.name) + " = " + std::to_string(f.shipped);
    return s;
}

// 4.1
inline const std::vector<std::pair<std::string, std::string>>& stances() {
    static const std::vector<std::pair<std::string, std::string>> v = {
        {"dwarf", "stances = [{ race = \"goblin\", value = -100 }]"},
        {"elf", "stances = [{ race = \"faerie\", value = 200 }]"},
        {"faerie", "stances = [{ race = \"elf\", value = 200 }]"},
        {"goblin", "stances = [{ race = \"dwarf\", value = -100 }]"},
        {"human", "stances = []"},
        {"orc", "stances = []"}};
    return v;
}

// 9.5, 12.1, 12.2 by kind: raid_phrases, name_forms (and lair_forms for undead).
struct MonsterEdit {
    std::string kind, phrases, name_forms, lair_forms;
};
inline const std::vector<MonsterEdit>& monster_edits() {
    static const std::vector<MonsterEdit> v = {
        {"dragon", R"(raid_phrases = ["fell in fire upon", "burned the fields of", "came down in flame on"])",
         R"(name_forms = ["{root} the Black", "{root} the Pale", "{root} the Red", "{root} the Old", "{root} Ashwing", "{root} the Gold-Eyed", "{root}"])",
         ""},
        {"giant", R"(raid_phrases = ["came down from the heights upon", "hurled stones upon", "fell upon"])",
         R"(name_forms = ["{root} Stonehand", "{root} the Tall", "{root} Cliffbreaker", "{root} the Grey", "{root}"])", ""},
        {"ogre", R"(raid_phrases = ["fell upon", "plundered the stores of", "carried off the herds of"])",
         R"(name_forms = ["{root} the Glutton", "{root} the Gaunt", "{root} the Hungering", "{root} Bonebreaker", "{root} the Fat", "{root}"])",
         ""},
        {"troll", R"(raid_phrases = ["came down by night on", "fell upon", "prowled the edges of"])",
         R"(name_forms = ["Old {root}", "{root} the Grey", "{root} Stonehide", "{root} of the Fells", "Mossback {root}", "{root}"])",
         ""},
        {"undead", R"(raid_phrases = ["came out of the barrows against", "walked by night in", "crept into"])",
         R"(name_forms = ["The Host of {place}", "The Restless of {place}", "The Dead of {place}"])",
         R"(lair_forms = ["The Barrows of {root}", "The Howes of {root}"])"},
        {"wyvern", R"(raid_phrases = ["stooped on", "swept down on", "came screaming down on"])",
         R"(name_forms = ["The {root} Brood", "The Brood of {root}", "The Screamers of {root}"])", ""}};
    return v;
}

// 13 by species.
inline const std::vector<std::pair<std::string, std::string>>& bold_phrases() {
    static const std::vector<std::pair<std::string, std::string>> v = {
        {"bear", R"(bold_phrases = ["broke into the stores of", "came down on the herds of"])"},
        {"boar", R"(bold_phrases = ["trampled the fields of", "tore up the gardens of"])"},
        {"deer", R"(bold_phrases = ["strayed into the fields of"])"},
        {"giant_spider", R"(bold_phrases = ["spun their webs across the paths of", "lay in wait on the roads from"])"},
        {"wolf", R"(bold_phrases = ["hunted the roads from", "grew bold and stalked the herds of"])"}};
    return v;
}

// 8.2, 11.3: the entries of kinds 56 to 62 (appended after kind 55's, kind id order), and 9.5 / 13's templates.
inline const std::vector<std::string>& report_entries() {
    static const std::vector<std::string> v = {
        R"(  { kind = "settlement_fled", salience = 45, template = "For fear of {monster}, the people of {@settlement} left their homes." },)",
        R"(  { kind = "peoples_met", salience = 25, template = "The {race_a:plural} first met the {race_b:plural} in {@region}{band?, and hated them from the first|, and were wary of them|, and were glad of them}." },)",
        R"(  { kind = "disposition_turned", salience = 55, template = "{@causes:war_won=the wars,peace=the wars,truce=the wars,civ_conquered=the conquests,sacked=the sacks,settlement_captured=the conquests,people_expelled=the expulsions,refugees_arrived=years of shelter given|After |, }the {race_a:plural} and the {race_b:plural} {band?came to hate one another|grew wary of one another|became friends}." },)",
        R"(  { kind = "refugees_arrived", salience = 25, template = "{@causes:sacked=the sack,settlement_captured=the fall,settlement_overrun=the ravage,people_expelled=the expulsion|After |, }{people} {race:plural} from {origin} found shelter in {settlement}{standing?, held as subjects by|, tolerated by|, welcomed by|, among their own kin of} {civilization}." },)",
        R"(  { kind = "settlement_mixed", salience = 25, template = "{@causes:settlement_captured=the conquest,settlements_ceded=the cession,region_ceded=the cession|After |, }the {race:plural} and the {ruling:plural} lived side by side in {settlement}." },)",
        R"(  { kind = "standing_changed", salience = 15, template = "In {settlement}, {civilization} {standing?held the|tolerated the|accepted the} {race:plural}{standing? as subjects|| as equals}." },)",
        R"(  { kind = "people_expelled", salience = 40, template = "{civilization} drove the {race:plural} out of {settlement}; {people} left." },)"};
    return v;
}
inline constexpr std::string_view TEMPLATE_48 = "{monster} {monster:raid} {@settlement}; {dead?no one died|one died|# died}.";
inline constexpr std::string_view TEMPLATE_53 =
    "In {@region} the {@beast} {@beast:bold} {settlement}; {dead?no one|one|#} died.";

inline void set_template(std::string& report, std::string_view kind, std::string_view templ) {
    const std::string head = "{ kind = \"" + std::string(kind) + "\"";
    const std::size_t a = report.find(head);
    if (a == std::string::npos) return;
    const std::size_t t0 = report.find("template = \"", a) + 12;
    const std::size_t t1 = report.find("\" },", t0);
    report.replace(t0, t1 - t0, std::string(templ));
}

// The P3-01 listing with the 0.24.0 data of the spec.
inline Files files_024() {
    Files f = history_data::shipped_files();
    for (const auto& [race, line] : stances()) {
        std::string& t = f["races/" + race + ".toml"];
        if (t.find("\nstances = ") == std::string::npos) {
            const std::size_t a = t.find("\nculture = ");
            const std::size_t b = t.find('\n', a + 1);
            t.insert(b + 1, line + "\n");
        }
    }
    if (f.count("history/relations.toml") == 0U) f["history/relations.toml"] = std::string(RELATIONS_TOML);
    insert_after_line(f["history/monsters.toml"], "prey_might_max_permille = ", field_lines(monsters_fields_024()));
    insert_after_line(f["history/peoples.toml"], "ghost_max_population = ", field_lines(peoples_fields_024()));
    for (const MonsterEdit& m : monster_edits()) {
        std::string& t = f["monsters/" + m.kind + ".toml"];
        replace_line(t, "name_forms = [", m.name_forms);
        if (!m.lair_forms.empty()) replace_line(t, "lair_forms = [", m.lair_forms);
        if (m.kind == "undead" || m.kind == "wyvern") replace_line(t, "hoard_permille = ", "hoard_permille = 0");
        insert_after_line(t, "hoard_permille = ", m.phrases);
    }
    for (const auto& [beast, line] : bold_phrases()) insert_after_line(f["beasts/" + beast + ".toml"], "food = ", line);
    std::string& rep = f["history/report.toml"];
    if (rep.find("\"settlement_fled\"") == std::string::npos) {
        const std::size_t end = rep.rfind("]\n");
        std::string add;
        for (const std::string& e : report_entries()) add += e + "\n";
        rep.insert(end, add);
    }
    set_template(rep, "monster_raid", TEMPLATE_48);
    set_template(rep, "beasts_bold", TEMPLATE_53);
    // 12.3: the filter word and rule 7's form are in game/data/name_filters and history/renaming.toml
    std::string& ren = f["history/renaming.toml"];
    const std::size_t h = ren.find("\"The Hollow of {name}\"");
    if (h != std::string::npos) ren.replace(h, 22, "\"{name}'s Waste\"");
    return f;
}

}  // namespace p503data
