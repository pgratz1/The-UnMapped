#pragma once

// The data of spec P5-03a (docs/specs/p5-03a-relations-followups.md r1, 10.1 to 10.3) applied to the 0.24.0 data of
// p5_03_relations_data.hpp (P5-03's listing over the P3-01 one): two changed values and eleven appended fields of
// history.relations, undead_min_dead 1000 and the appended undead_dead_permille of history.monsters, and the eleven
// templates of 10.3. Transcribed from the spec, not read from game/data; the same values as the reference's
// listings (p3_data_reference.py RELATIONS_RANGES_025, MONSTERS_TUNING_025, REPORT_TEMPLATES_025). Each edit is
// applied only when the listing does not already hold it.
//
// Reading D3 (reported): 3.5 and 7.3 give words that a {@causes:...} list "gains"; they are appended after the
// list's last entry in the order the spec lists them (the rendering does not depend on the order; the data hash
// does).

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p5_03_relations_data.hpp"

namespace p503a {

using history_data::Files;
using p503data::RelField;

// 10.1: the 0.24.0 fields with shelter_memory 90 and conquest_settlers_permille 50, then eleven new ones.
inline const std::vector<RelField>& new_relations_fields() {
    static const std::vector<RelField> v = {
        {"peace_years", 0, 100000, 100},          {"peace_memory_per_century", 0, 10000, 60},
        {"peace_memory_max", 0, 2000, 300},       {"contact_hops", 0, 8, 1},
        {"foe_years", 1, 100000, 100},            {"foe_war_memory", 0, 1000, 60},
        {"foe_monster_memory", 0, 1000, 60},      {"meet_sealed_hops", -1, 8, 5},
        {"meet_sealed_years", 0, 100000, 150},    {"conquest_settlers_span_permille", 0, 1000, 400},
        {"expel_gap_years", 0, 100000, 60}};
    return v;
}
inline const std::vector<RelField>& relations_fields() {
    static const std::vector<RelField> v = [] {
        std::vector<RelField> out;
        for (RelField f : p503data::relations_fields()) {
            if (f.name == "shelter_memory") f.shipped = 90;
            if (f.name == "conquest_settlers_permille") f.shipped = 50;
            out.push_back(f);
        }
        for (const RelField& f : new_relations_fields()) out.push_back(f);
        // P5-04a 14.2 (0.28.0): three more after expel_gap_years, at their shipped values
        out.push_back({"turn_cooldown_years", 0, 100000, 800});
        out.push_back({"expel_settled_years", 0, 100000, 800});
        out.push_back({"expel_fresh_years", 0, 100000, 150});
        return out;
    }();
    return v;
}

// 10.2: after overrun_people_ref (the last field of 0.24.0).
inline const std::vector<RelField>& new_monsters_fields() {
    static const std::vector<RelField> v = {{"undead_dead_permille", 0, 1000, 25}};
    return v;
}
inline constexpr std::int64_t UNDEAD_MIN_DEAD = 1000;   // 10.2 (was 4000)

// 10.3: the templates of 3.5, 4.2, 5.2, 5.3, 7.2 and 7.3 (saliences unchanged).
inline const std::vector<std::pair<std::string, std::string>>& templates() {
    static const std::vector<std::pair<std::string, std::string>> v = {
        {"peoples_met",
         "The {race_a:plural} {sealed?first had word of|first met} the {race_b:plural} in {@region}{band?, and hated "
         "them from the first|, and were wary of them|, and were glad of them}."},
        {"disposition_turned",
         "{@causes:war_won=the wars,peace=the wars,truce=the wars,civ_conquered=the conquests,sacked=the sacks,"
         "settlement_captured=the conquests,people_expelled=the expulsions,refugees_arrived=years of shelter given,"
         "war_declared=wars against a common enemy,monster_raid=the raids of a common foe,settlement_overrun=the raids "
         "of a common foe,peoples_met=long years of peace,disposition_turned=long years apart|After |, }the "
         "{race_a:plural} and the {race_b:plural} {change?came to hate one another|put aside their hatred of one "
         "another|grew wary of one another|became friends}."},
        {"refugees_arrived",
         "{@causes:sacked=the sack,settlement_captured=the fall,settlement_overrun=the ravage,people_expelled=the "
         "expulsion,settlement_abandoned=the abandonment of their home,settlement_abandoned_to=the abandonment of "
         "their home,plague=the plague,plague_outbreak=the plague,plague_spread=the plague,famine=the famine,"
         "settlement_fled=their flight,region_ceded=the cession,settlements_ceded=the cession|After |, }{people} "
         "{race:plural} from {origin} found shelter in {settlement}{standing?, held as subjects by|, tolerated by|, "
         "welcomed by|, among their own kin of} {civilization}."},
        {"settlement_mixed",
         "{@causes:settlement_captured=the conquest,settlements_ceded=the cession,region_ceded=the cession,sacked=the "
         "sack,settlement_overrun=the ravage,refugees_arrived=the coming of refugees,refugees_settled=the flight,"
         "settlement_abandoned_to=the abandonment of a neighbour|After |, }in {settlement} the {ruling:plural} "
         "{standing?ruled over|tolerated|lived as equals with} the {race:plural}{majority?, who outnumbered them|}."},
        {"people_expelled",
         "Hating the {race:plural}, {civilization} drove them out of {settlement}, where it had {standing?held them "
         "as subjects|tolerated them|tolerated them} for {years} {years?year|years}; {people} left."},
        {"monster_appeared", "{monster}, {monster:kind}, made {monster:its} lair in {@region}."},
        {"settlement_overrun",
         "{monster} drove the people from {@settlement} and made it {monster:its} lair; {survivors?no one escaped|one "
         "escaped|# escaped}."},
        {"monster_driven_off", "{civilization} drove {monster} back to {monster:its} lair{monster:where}."},
        {"monster_slain", "{slayer} hunted down and slew {monster}{monster:aside}{monster:where}."},
        {"monster_died", "{monster}{monster:aside} {monster:died}{monster:where}."},
        {"undead_rose",
         "{@causes:battle=the slaughter,battle_at_pass=the slaughter,sacked=the sack|After |, }{monster} rose from "
         "{monster:its} graves{monster:where}."}};
    return v;
}

inline void set_field(std::string& text, std::string_view name, std::int64_t v) {
    p503data::replace_line(text, std::string(name) + " = ", std::string(name) + " = " + std::to_string(v));
}

// The 0.25.0 data of the spec over files_024().
inline Files files_025() {
    Files f = p503data::files_024();
    std::string& rel = f["history/relations.toml"];
    set_field(rel, "shelter_memory", 90);
    set_field(rel, "conquest_settlers_permille", 50);
    p503data::insert_after_line(rel, "expel_share_max = ", p503data::field_lines(new_relations_fields()));
    std::string& mon = f["history/monsters.toml"];
    set_field(mon, "undead_min_dead", UNDEAD_MIN_DEAD);
    p503data::insert_after_line(mon, "overrun_people_ref = ", p503data::field_lines(new_monsters_fields()));
    std::string& rep = f["history/report.toml"];
    for (const auto& [kind, templ] : templates()) p503data::set_template(rep, kind, templ);
    return f;
}

}  // namespace p503a
