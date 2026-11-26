#pragma once

// The data of spec P5-04 (docs/specs/p5-04-alliances-trade.md r2: 4.1, 4.2, 4.3, 8.3, 11.1) applied to the 0.25.0
// shipped listing (p3_01_history_data.hpp): three race fields after stances, the new history.alliances file, seven
// history.conflict fields after capital_captured_salience, the report entries of kinds 63 to 78, allied_battle in the
// cause words of kinds 13, 16, 31, 43 and 45 and in the renaming rule slaughter. Generated from the reference's
// listings (p3_data_reference.py ALLIANCES_RANGES_026, RACE_FIELDS_026, CONFLICT_NEW_RANGES_026, REPORT_ENTRIES_026),
// so that the C++ and the reference read the same files. Each edit is applied only when the listing does not hold
// it already (at integration the shipped listing holds them).
//
// Reading D1 (reported): 11.1's allied_battle= is placed right after the battle_at_pass= entry of each list.

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p5_03_relations_data.hpp"

namespace p504data {

using history_data::Files;

struct Field {
    std::string_view name;
    std::int64_t lo, hi, shipped;
};
// 4.2: history.alliances, every field required, in this order; mine_forms last.
inline const std::vector<Field>& alliances_fields() {
    static const std::vector<Field> v = {
        {"ally_hops", -1, 8, 3},
        {"ally_min_disposition", -2000, 2000, -300},
        {"ally_full_disposition", -2000, 2000, 150},
        {"ally_friend_disposition", -2000, 2000, 150},
        {"ally_break_disposition", -2000, 2000, -350},
        {"allies_max", 0, 16, 3},
        {"ally_after_war_years", 0, 100000, 100},
        {"foe_years", 1, 100000, 100},
        {"fear_share", 0, 1000, 300},
        {"form_foe_permille", 0, 1000, 40},
        {"form_fear_permille", 0, 1000, 30},
        {"form_trade_permille", 0, 1000, 20},
        {"form_friend_permille", 0, 1000, 10},
        {"form_max_permille", 0, 1000, 60},
        {"bloc_share_max", 0, 1000, 500},
        {"bloc_lapse_share", 0, 1001, 700},
        {"term_years", 1, 100000, 150},
        {"refusals_to_break", 1, 100, 2},
        {"defend_permille", 0, 1000, 1000},
        {"attack_permille", 0, 1000, 500},
        {"hostility_full", 1, 2000, 500},
        {"ally_wars_max", 1, 16, 3},
        {"ally_memory", 0, 1000, 30},
        {"renew_memory", 0, 1000, 15},
        {"join_memory", 0, 1000, 30},
        {"refuse_memory", 0, 1000, 40},
        {"betray_memory", 0, 1000, 150},
        {"ally_war_memory", 0, 1000, 75},
        {"trade_hops", -1, 8, 3},
        {"trade_min_disposition", -2000, 2000, -250},
        {"trade_break_margin", 0, 2000, 100},
        {"trade_after_war_years", 0, 100000, 50},
        {"trade_permille", 0, 1000, 15},
        {"trade_max", 0, 16, 4},
        {"trade_credit_years", 1, 100000, 50},
        {"trade_memory", 0, 1000, 20},
        {"trade_ally_years", 0, 100000, 100},
        {"spoils_plunder_permille", 0, 1000, 250},
        {"spoils_floor_permille", 0, 1000, 100},
        {"spoils_cap_permille", 0, 1000, 400},
        {"spoils_grant_permille", 0, 1000, 350},
        {"spoils_warmth_permille", 0, 1000, 300},
        {"spoils_part_permille", 0, 1000, 500},
        {"spoils_need_permille", 0, 1000, 400},
        {"spoils_grant_min", 0, 1000, 100},
        {"spoils_grant_max", 0, 1000, 950},
        {"share_memory", 0, 1000, 25},
        {"withheld_memory", 0, 1000, 100},
        {"withheld_grudge", 0, 2000, 400},
        {"withheld_to_break", 1, 100, 1},
        {"mine_permille", 0, 1000, 40},
        {"mine_ore_min", 1, 100000, 1},
        {"mine_settlement_min", 0, 1000000, 200},
        {"arms_trade_permille", 0, 4000, 2000},
    };
    return v;
}
// 4.3: appended to history.conflict after capital_captured_salience.
inline const std::vector<Field>& conflict_fields() {
    static const std::vector<Field> v = {
        {"allied_strain_permille", 0, 1000, 160},
        {"ally_deterrence_permille", 0, 1000, 50},
        {"ally_effort_permille", 0, 1000, 1000},
        {"trade_strength_permille", 0, 1000, 50},
        {"trade_strength_max", 0, 16, 3},
        {"forge_strength_permille", 0, 1000, 50},
        {"arms_strength_permille", 0, 1000, 100},
    };
    return v;
}
struct RaceFields {
    std::string_view race, origin_allies;   // origin_allies as its TOML line value
    std::int64_t join, mining;
};
// 4.1
inline const std::vector<RaceFields>& race_fields() {
    static const std::vector<RaceFields> v = {
        {"elf", R"(["faerie"])", 700, 0},
        {"faerie", R"(["elf"])", 150, 0},
        {"dwarf", R"([])", 800, 1000},
        {"goblin", R"([])", 400, 0},
        {"human", R"([])", 600, 0},
        {"orc", R"([])", 600, 0},
    };
    return v;
}
inline constexpr std::string_view ALLIANCES_TOML = R"(# Alliances, trade, the dwarves' mines and arms (spec P5-04 4.2). Generator 0.26.0.
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
)";
// 8.3: (key, salience, template) of kinds 63 to 78
inline const std::vector<std::pair<std::string, std::pair<std::int64_t, std::string>>>& report_entries() {
    static const std::vector<std::pair<std::string, std::pair<std::int64_t, std::string>>> v = {
        {"alliance_formed", {50, R"({@causes:trade_opened=long years of trade,war_declared=the outbreak of war,disposition_turned=the friendship of their peoples,civ_conquered=the fall of a realm,settlement_captured=the fall of a neighbour,sacked=the sack of a neighbour,region_ceded=the loss of a neighbour's land,settlements_ceded=the loss of a neighbour's land|After |, }{ally_a} and {ally_b} {basis?swore the alliance their peoples had kept since the gods made them|swore to stand together|made common cause|swore friendship and alliance}{foe:against}.)"}},
        {"alliance_renewed", {10, R"({ally_a} and {ally_b} renewed their alliance{foe:against}.)"}},
        {"alliance_lapsed", {25, R"(The alliance of {ally_a} and {ally_b} lapsed{reason?, for together they had grown too great|, its cause long past}.)"}},
        {"alliance_broken", {40, R"({@causes:ally_refused=a call to arms refused|After |, }the alliance of {ally_a} and {ally_b} was broken{reason?, their friendship having cooled|}.)"}},
        {"alliance_betrayed", {65, R"({betrayer} broke faith and made war on {betrayed}, an ally.)"}},
        {"ally_joined_war", {40, R"({ally} {side?joined the war of|came to the defence of} {partner} against {enemy}.)"}},
        {"ally_refused", {30, R"({ally} would not take up arms for {partner} against {enemy}.)"}},
        {"ally_withdrew", {25, R"(Bled white, {ally} left the war of {partner} against {enemy}.)"}},
        {"allied_battle", {35, R"({winner}{winner_ally:with} defeated {loser}{loser_ally:with} {pass?in|at the pass into|at the pass into} {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}.)"}},
        {"trade_opened", {20, R"({@causes:peace=the peace,war_won=the war,truce=the truce,peoples_met=the first meeting of their peoples|After |, }{partner_a} and {partner_b} opened trade.)"}},
        {"trade_broken", {15, R"({@causes:war_declared=war came,alliance_betrayed=war came|When |, }the trade between {partner_a} and {partner_b} {reason?ended|dwindled as their friendship cooled|was lost with the roads between them}.)"}},
        {"spoils_shared", {20, R"({principal} gave {ally} its share of the spoils of {@region}.)"}},
        {"spoils_withheld", {35, R"({principal} kept the spoils of {@region} from {ally}, its ally.)"}},
        {"alliance_spurned", {50, R"(After the spoils of {@region} were kept from it, {ally} broke its alliance with {principal}.)"}},
        {"mine_opened", {20, R"({civilization} opened {site} in {@region}.)"}},
        {"arms_sold", {30, R"({seller} sold arms forged in {site} to {buyer}.)"}},
    };
    return v;
}
// 11.1: the five kinds whose cause words gain allied_battle=
inline const std::vector<std::string>& cause_kinds() {
    static const std::vector<std::string> v = {"settlement_dwindled", "settlement_abandoned", "settlement_abandoned_to", "region_renamed", "undead_rose"};
    return v;
}

// 11.1: "battle=<w>,battle_at_pass=<w>" -> "battle=<w>,battle_at_pass=<w>,allied_battle=<w>" in a template.
inline std::string with_allied_cause(std::string t) {
    const std::size_t a = t.find("battle=");
    if (a == std::string::npos || t.find("allied_battle=") != std::string::npos) return t;
    const std::size_t e1 = t.find_first_of(",|", a);
    const std::string word = t.substr(a + 7, e1 - a - 7);
    const std::string pass = "battle_at_pass=" + word;
    const std::size_t b = t.find(pass, e1);
    if (b == std::string::npos) return t;
    t.insert(b + pass.size(), ",allied_battle=" + word);
    return t;
}

// P5-06 8.2, 11.1 (0.27.0): the expectation of a shipped template over its 0.26.0 text: battle_led= after
// allied_battle=, blight= before tribute=, and {r:under} after the acting role of the six changed templates.
inline std::string with_p506_templates(std::string t) {
    t = with_allied_cause(std::move(t));
    const std::size_t a = t.find("allied_battle=");
    if (a != std::string::npos && t.find("battle_led=") == std::string::npos) {
        const std::size_t e1 = t.find_first_of(",|", a);
        const std::string word = t.substr(a + 14, e1 - a - 14);
        t.insert(e1, ",battle_led=" + word);
    }
    auto rep = [&t](const std::string& from, const std::string& to) {
        const std::size_t b = t.find(from);
        if (b != std::string::npos) t.replace(b, from.size(), to);
    };
    rep(",tribute=tribute", ",blight=blight,tribute=tribute");
    rep("{attacker} declared war on", "{attacker}{attacker:under} declared war on");
    rep("{betrayer} broke faith", "{betrayer}{betrayer:under} broke faith");
    rep("{principal} gave", "{principal}{principal:under} gave");
    rep("{principal} kept", "{principal}{principal:under} kept");
    rep("{civilization} fell to", "{civilization}{civilization:under} fell to");
    rep("{civilization} came to an end.", "{civilization}{civilization:under} came to an end.");
    return t;
}

inline std::string entry_line(const std::string& key, std::int64_t sal, const std::string& templ) {
    return "  { kind = \"" + key + "\", salience = " + std::to_string(sal) + ", template = \"" + templ + "\" },";
}

// The 0.26.0 data of the spec over the 0.25.0 shipped listing.
inline Files files_026() {
    Files f = history_data::shipped_files();
    for (const RaceFields& r : race_fields()) {
        std::string& t = f["races/" + std::string(r.race) + ".toml"];
        if (t.find("\norigin_allies = ") != std::string::npos) continue;
        const std::size_t a = t.find("\nstances = ");
        const std::size_t b = t.find('\n', a + 1);
        t.insert(b + 1, "origin_allies = " + std::string(r.origin_allies) + "\nally_join_permille = " +
                            std::to_string(r.join) + "\nmining_permille = " + std::to_string(r.mining) + "\n");
    }
    if (f.count("history/alliances.toml") == 0U) f["history/alliances.toml"] = std::string(ALLIANCES_TOML);
    std::string& co = f["history/conflict.toml"];
    if (co.find("\nallied_strain_permille = ") == std::string::npos) {
        const std::size_t a = co.find("\ncapital_captured_salience = ");
        const std::size_t b = co.find('\n', a + 1);
        std::string add;
        for (const Field& x : conflict_fields()) add += std::string(x.name) + " = " + std::to_string(x.shipped) + "\n";
        co.insert(b + 1, add);
    }
    std::string& rep = f["history/report.toml"];
    for (const std::string& k : cause_kinds()) {
        const std::string head = "{ kind = \"" + k + "\", salience = ";
        const std::size_t a = rep.find(head);
        if (a == std::string::npos) continue;
        const std::size_t t0 = rep.find("template = \"", a) + 12;
        const std::size_t t1 = rep.find("\" }", t0);
        rep.replace(t0, t1 - t0, with_allied_cause(rep.substr(t0, t1 - t0)));
    }
    if (rep.find("\"alliance_formed\"") == std::string::npos) {
        const std::size_t end = rep.rfind("]\n");
        std::string add;
        for (const auto& [k, st] : report_entries()) add += entry_line(k, st.first, st.second) + "\n";
        rep.insert(end, add);
    }
    std::string& ren = f["history/renaming.toml"];
    const std::string old = "kinds = [\"battle\", \"battle_at_pass\"]\n";
    if (const std::size_t a = ren.find(old); a != std::string::npos) {
        ren.replace(a, old.size(), "kinds = [\"battle\", \"battle_at_pass\", \"allied_battle\"]\n");
    }
    return f;
}

}  // namespace p504data
