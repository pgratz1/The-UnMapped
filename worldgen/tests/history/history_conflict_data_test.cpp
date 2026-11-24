// Spec P3-06 16.1 items 11-13 (with P3-01 r7 and P3-04 8.2 r4): the data of
// generator 0.16.0 through the loader and the compiled catalogue: the
// history.conflict listing of 12.1 (43 fields in table order and their
// values, the shipped files equal to it), every field one step outside its
// range, every field required, each named check of 12.1 once with its
// message and the edges that pass; the event kinds 20-30 of 11 transcribed
// (keys, caused, region and settlement use, roles with counts, values,
// salience, template) and their templates; the report lists every kind;
// the five hist.conflict streams after hist.peoples.split (51 names at 0.16.0, 52 from P3-08; r9 adds
// no stream, 20.10). r9 (generator 0.18.0, P3-09): the eleven fields of 20.9 with their ranges and the
// named check conflict_enmity_hops, claim salience 35; kinds 32 and 33 in history_balance_data_test.cpp.
// The race-data items of 12.2 are in history_peoples_data_test.cpp and
// data_history_kinds_test.cpp (P3-01 tests).
//
// Written from the spec only; needs no P3-06 name of the library, so it
// compiles before the implementation exists and fails until the 0.16.0 data
// and catalogue land.

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../p3_01_history_data.hpp"
#include "../p3_01_history_support.hpp"
#include "p3_04_engine_support.hpp"
#include "p3_06_conflict_api.hpp"
#include "worldgen/core/streams.hpp"

namespace {

namespace wd = worldgen::data;
using history_data::Files;
using namespace history_test;

Files with_conflict_edit(std::string_view from, std::string_view to) {
    Files f = history_data::shipped_files();
    std::string& t = f["history/conflict.toml"];
    const std::size_t at = t.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, std::string(from) << " not in conflict.toml");
    t.replace(at, from.size(), to);
    return f;
}

wd::LoadError one_error(const Load& l) {
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

const wd::Record& conflict_record(const wd::ContentSet& c) {
    const wd::KindContent* h = c.find_kind("history");
    REQUIRE(h != nullptr);
    for (const wd::Definition& d : h->definitions) {
        if (d.id == "conflict") {
            return d.record;
        }
    }
    FAIL("no history.conflict");
    return h->definitions.front().record;
}

// 12.1: (field, low, high) in table order; records list the range of each of their fields.
struct Range {
    const char* field;
    std::int64_t lo, hi;
};
const std::vector<Range>& ranges() {
    static const std::vector<Range> v = {
        {"camp", -1000, 1000}, {"enmity_max", -1000, 1000}, {"border_friction", 0, 2000}, {"want_weight", 0, 2000},
        {"hunger_full", 1, 100000}, {"fear_share", 0, 999}, {"fear_weight", 0, 2000},
        {"tick_strain_permille", 0, 1000}, {"quiet_strain_permille", 0, 1000}, {"strain_max", 1, 1000000},
        {"claim_strain", 1, 1000000}, {"declare_min", 0, 1000000}, {"declare_span", 1, 1000000},
        {"declare_permille", 0, 1000}, {"declare_ratio", 1, 10000}, {"shelter_years", 0, 1000}, {"march_hops", 1, 4},
        {"cost_penalty", 0, 10000}, {"levy_permille", 0, 1000}, {"terrain_defense", 0, 10000},
        {"pass_defense", 0, 10000}, {"river_defense", 0, 10000}, {"fort_defense", 0, 10000},
        {"battle_permille", 0, 1000}, {"loss_loser_permille", 0, 1000}, {"loss_winner_permille", 0, 1000},
        {"garrison_permille", 0, 1000}, {"walls_permille", 1, 100000}, {"capital_walls", 0, 100000},
        {"sack_base", 0, 1000}, {"sack_hostility", 0, 2000}, {"sack_toll_permille", 0, 1000},
        {"capture_toll_permille", 0, 1000}, {"exhaustion_permille", 1, 1000}, {"war_max_years", 1, 1000},
        {"truce_years", 0, 10000}, {"war_grudge", 0, 2000}, {"capture_grudge", 0, 2000}, {"secession_grudge", 0, 2000},
        {"grudge_decay_per_century", 0, 10000}, {"last_refuge", 0, 1},
        // r9 20.9 (generator 0.18.0, P3-09): eleven fields after last_refuge
        {"refuge_flight", 0, 1}, {"flight_hops", 1, 8}, {"weary_wars", 0, 100},  // hidden_years out: r17 22.2
        {"tribute_permille", 0, 1000}, {"revenge_aggression", 0, 1000}, {"levy_hops", 0, 16}, {"odds_floor", 0, 1000},
        {"truce_break_aggression", 0, 999}, {"enmity_hops", 1, 4},  // r14 21.9 (0.19.0): odds_min removed
        // r10 20.19 (P3-09 r3): after enmity_hops
        {"kill_max_permille", 1, 10000}, {"find_permille", 0, 1000}, {"search_years", 1, 100},
        // r14 21.7 (0.19.0, P3-10): after search_years
        {"search_weary_years", 0, 10000}, {"search_repeat_salience", 0, 100},
        // r15 21.12 item 2 (0.19.0, P3-10 r1): after search_repeat_salience
        {"quarrel_strain", 0, 5000},
        // r17 22.1 item 4 (P3-10 second round): after quarrel_strain
        {"quarrel_min_settlements", 0, 1000},
        // P3-06 r22 24.2, 24.5 (P5-01, generator 0.21.0): after quarrel_min_settlements
        {"quarrel_strength_permille", 0, 1000}, {"toll_spread_permille", 0, 500},
        {"capital_sacked_salience", 0, 100}, {"capital_captured_salience", 0, 100},
    };
    return v;
}

std::string listing_line(std::string_view field) {
    const std::string t(history_data::CONFLICT_TOML);
    const std::size_t at = t.find("\n" + std::string(field) + " = ");
    REQUIRE(at != std::string::npos);
    const std::size_t end = t.find('\n', at + 1);
    return t.substr(at + 1, end - at - 1);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 12.1, r9 20.9: history.conflict loads with the listing's 59 fields in table order and values (r14, r15: 0.19.0)") {
    const Load l = load(history_data::shipped_files());
    INFO(dump(l));
    REQUIRE(l.ok());
    const wd::Record& r = conflict_record(*l.content);
    REQUIRE(r.size() == ranges().size());
    for (std::size_t i = 0; i < r.size(); ++i) {
        CHECK(r.name_at(i) == ranges()[i].field);
    }
    const wd::Record& camp = r.get_record("camp");
    REQUIRE(camp.size() == 6U);
    const char* camp_fields[6] = {"good_good", "good_neutral", "good_evil", "neutral_neutral", "neutral_evil",
                                  "evil_evil"};
    const std::int64_t camp_values[6] = {400, -150, -700, 100, -250, 300};
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(camp.name_at(i) == camp_fields[i]);
        CHECK(camp.get_int(camp_fields[i]) == camp_values[i]);
    }
    CHECK(r.get_int("enmity_max") == -500);
    CHECK(r.get_int("tick_strain_permille") == 25);
    CHECK(r.get_int("quiet_strain_permille") == 6);
    CHECK(r.get_int("shelter_years") == 150);
    CHECK(r.get_int("march_hops") == 2);
    CHECK(r.get_record("levy_permille").get_int("city") == 80);
    CHECK(r.get_record("levy_permille").get_int("fortress") == 250);
    CHECK(r.get_record("walls_permille").get_int("fortress") == 6000);
    CHECK(r.get_int("truce_years") == 200);
    CHECK(r.get_int("last_refuge") == 1);
    CHECK(r.get_int("capital_captured_salience") == 80);
    CHECK(r.size() == 61U);  // 43 at 0.16.0 and 0.17.0; r9 20.9 adds eleven, r10 20.19 three; r14 (0.19.0) -1 +2, r15 +1; r22 +2
    CHECK(r.get_int("quarrel_strain") == 600);
    // the shipped content (game/data) holds the same record (2: identical copies in the fixtures)
    CHECK(conflict_record(shipped_content()) == r);
}

TEST_CASE("P3-06 16.1 item 12: every history.conflict field one step outside its range is a range error") {
    for (const Range& rg : ranges()) {
        CAPTURE(std::string_view(rg.field));
        const std::string line = listing_line(rg.field);
        const bool record = line.find('{') != std::string::npos;
        auto with = [&](std::int64_t v) {
            if (!record) {
                return with_conflict_edit(line, std::string(rg.field) + " = " + std::to_string(v));
            }
            const std::size_t eq = line.find(" = ", line.find('{'));
            const std::size_t comma = line.find(',', eq);
            return with_conflict_edit(line, line.substr(0, eq + 3) + std::to_string(v) + line.substr(comma));
        };
        const wd::LoadError e = one_error(load(with(rg.hi + 1)));
        CHECK(e.path == "history/conflict.toml");
        CHECK(contains(e.message, "history.conflict." + std::string(rg.field)));
        CHECK(contains(e.message, "expected"));
        const wd::LoadError e2 = one_error(load(with(rg.lo - 1)));
        CHECK(contains(e2.message, "history.conflict." + std::string(rg.field)));
    }
}

TEST_CASE("P3-06 12.1: every history.conflict field is required and no other key is accepted") {
    for (const Range& rg : ranges()) {
        CAPTURE(std::string_view(rg.field));
        const std::string line = listing_line(rg.field);
        const wd::LoadError e = one_error(load(with_conflict_edit(line + "\n", "")));
        CHECK(e.message == "missing required field 'history.conflict." + std::string(rg.field) + "'");
    }
    CHECK_FALSE(load(with_conflict_edit("enmity_max = -500", "enmity_max = -500\nalliance_max = 1")).ok());
}

TEST_CASE("P3-06 12.1, 16.1 item 12: each named check of history.conflict once, with its message; the edges pass") {
    struct C {
        const char* from;
        const char* to;
        const char* check;
        const char* message;
    };
    for (const C& c : std::vector<C>{
             {"claim_strain = 300", "claim_strain = 301", "conflict_declare", "claim_strain must not be above declare_min"},
             {"declare_min = 300", "declare_min = 299", "conflict_declare", "claim_strain must not be above declare_min"},
             {"strain_max = 5000", "strain_max = 899", "conflict_strain_max",
              "declare_min + declare_span must not be above strain_max"},
             {"declare_span = 600", "declare_span = 4701", "conflict_strain_max",
              "declare_min + declare_span must not be above strain_max"},
             // r9 20.9 (0.18.0): enmity_hops >= march_hops
             {"enmity_hops = 3", "enmity_hops = 1", "conflict_enmity_hops", "enmity_hops must not be below march_hops"},
             {"march_hops = 2", "march_hops = 4", "conflict_enmity_hops", "enmity_hops must not be below march_hops"},
             // r15 21.12 item 2 (0.19.0): quarrel_strain >= claim_strain (300)
             {"quarrel_strain = 600", "quarrel_strain = 299", "conflict_quarrel_strain",
              "quarrel_strain must not be below claim_strain"},
         }) {
        CAPTURE(std::string_view(c.to));
        const wd::LoadError e = one_error(load(with_conflict_edit(c.from, c.to)));
        CHECK(e.path == "history/conflict.toml");
        CHECK(e.line == 1U);
        CHECK(e.message == "check '" + std::string(c.check) + "' failed for 'history.conflict': " + c.message);
    }
    CHECK(load(with_conflict_edit("claim_strain = 300", "claim_strain = 299")).ok());
    CHECK(load(with_conflict_edit("strain_max = 5000", "strain_max = 900")).ok());
    CHECK(load(with_conflict_edit("enmity_hops = 3", "enmity_hops = 2")).ok());  // r9: the edge, equal to march_hops
    CHECK(load(with_conflict_edit("quarrel_strain = 600", "quarrel_strain = 300")).ok());  // r15: the edge, = claim_strain
}

TEST_CASE("P3-06 11 (r7), 16.1 item 11: the compiled kinds 20-30 equal section 11; templates parse; report lists them") {
    using ET = eng::EntityType;
    const ET C = ET::civilization;
    struct K {
        const char* key;
        std::int64_t region_use, settlement_use;
        std::vector<std::pair<const char*, std::int64_t>> roles;  // (role, count: 1 for 1-1, 2 for 2-2)
        std::vector<const char*> values;
        std::int64_t salience;
        const char* templ;
    };
    const std::vector<K> want = {
        // r9 20.9 (0.18.0): claim salience 45 -> 35 (P3-07 r8 21.6)
        {"claim", 1, 0, {{"claimant", 1}, {"holder", 1}}, {}, 35, "{claimant} laid claim to {@region}, held by {holder}."},
        {"war_declared", 1, 0, {{"attacker", 1}, {"defender", 1}}, {}, 70,
         "{attacker} declared war on {defender} over {@region}."},
        // P3-06 r14 21.2 (0.19.0): no "0 of the victors", the three-word form for both counts
        {"battle", 1, 0, {{"winner", 1}, {"loser", 1}}, {"winner_dead", "loser_dead"}, 30,
         "{winner} defeated {loser} in {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none "
         "of the victors| and one of the victors fell| and # of the victors fell}."},
        {"battle_at_pass", 1, 0, {{"winner", 1}, {"loser", 1}}, {"winner_dead", "loser_dead", "pass"}, 35,
         "{winner} defeated {loser} at the pass into {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? "
         "fell, and none of the victors| and one of the victors fell| and # of the victors fell}."},
        {"siege", 1, 1, {{"attacker", 1}, {"defender", 1}}, {"fell"}, 25,
         "{attacker} besieged {@settlement} of {defender}; it {fell?fell|held}."},
        {"sacked", 1, 1, {{"attacker", 1}, {"defender", 1}}, {"dead", "survivors"}, 75,
         "{attacker} sacked {@settlement} of {defender}; {dead} died and {survivors} fled."},
        {"settlement_captured", 1, 1, {{"attacker", 1}, {"defender", 1}}, {"dead", "fled"}, 60,
         // r7 (0.17.0): no "and 0 fled", P3-04 r10's three-word form
         "{attacker} took {@settlement} from {defender}; {dead} died{fled?| and one fled| and # fled}."},
        {"peace", 0, 0, {{"winner", 1}, {"loser", 1}}, {"years"}, 60,
         "{loser} sued {winner} for peace after {years} {years?year|years} of war."},
        {"truce", 0, 0, {{"parties", 2}}, {"years"}, 50,
         "{parties:all} made peace after {years} {years?year|years} of war, neither the victor."},
        {"region_ceded", 1, 0, {{"from", 1}, {"to", 1}}, {}, 55, "{from} ceded {@region} to {to}."},
        {"civ_conquered", 1, 0, {{"civilization", 1}, {"conqueror", 1}}, {}, 90, "{civilization} fell to {conqueror}."},
    };
    const eng::EventCatalogue& compiled = eng::compiled();
    // P3-05 r6 appends kind 31 after kinds 20-30 (0.17.0); P3-06 r9 and r10 kinds 32-36 (0.18.0); P3-04 r12 20.4
    // kinds 37 and 38 (0.19.0)
    REQUIRE(compiled.kinds.size() == 62U);  // P5-02 10.1 (0.23.0): kinds 44 to 55; P3-04 r21 23.2 (0.22.0): kind 43; r19 22.2 (0.21.0): kinds 41, 42; r17 21.2 (0.20.0): kinds 39, 40
    const eng::EventCatalogue cat = eng::catalogue_of(engine_test::shipped());
    REQUIRE(cat.kinds.size() == 62U);  // P5-02 10.1 (0.23.0): kinds 44 to 55; P3-04 r21 23.2 (0.22.0): kind 43; r19 22.2 (0.21.0): kinds 41, 42; r17 21.2 (0.20.0): kinds 39, 40
    for (std::size_t i = 0; i < want.size(); ++i) {
        const K& w = want[i];
        CAPTURE(std::string_view(w.key));
        for (const eng::KindDef* k : {&compiled.kinds[i + 19], &cat.kinds[i + 19]}) {
            CHECK(k->id == static_cast<std::int64_t>(i) + 20);
            CHECK(k->key == w.key);
            CHECK(k->caused);  // every kind of 11 is caused
            CHECK(k->region_use == w.region_use);
            CHECK(k->settlement_use == w.settlement_use);
            REQUIRE(k->roles.size() == w.roles.size());
            for (std::size_t r = 0; r < w.roles.size(); ++r) {
                CHECK(k->roles[r].seq == static_cast<std::int64_t>(r) + 1);
                CHECK(k->roles[r].role == w.roles[r].first);
                CHECK(k->roles[r].type == C);
                CHECK(k->roles[r].min_count == w.roles[r].second);
                CHECK(k->roles[r].max_count == w.roles[r].second);
            }
            REQUIRE(k->values.size() == w.values.size());
            for (std::size_t v = 0; v < w.values.size(); ++v) {
                CHECK(k->values[v] == w.values[v]);
            }
        }
        CHECK(cat.kinds[i + 19].salience == w.salience);
        CHECK(cat.kinds[i + 19].templ == w.templ);
        CHECK(eng::template_problem(cat.kinds[i + 19], w.templ) == std::nullopt);
    }
    // the kinds of P3-04 and P3-05 are unchanged (a shipped kind is never edited, P3-04 8.2)
    CHECK(compiled.kinds[0].key == "awakening");
    CHECK(compiled.kinds[18].key == "ruin_faded");
}

TEST_CASE("P3-06 3.3, 18.2, r10 20.17: the hist.conflict streams follow hist.peoples.split, 53 names in all") {
    const auto names = worldgen::streams::all_stream_names;
    // 51 at 0.16.0; P1-06 r4 adds geo.regions.name (52); r10 20.17 hist.conflict.search after hist.conflict.sack
    REQUIRE(names.size() == 77U);   // P5-03a 2 (0.25.0): hist.relations.settlers; P5-02 3, 26.1 (0.23.0): fifteen streams after hist.renaming.name, then hist.monsters.raid_target; P5-12 3 (0.22.0): hist.renaming.chance, hist.renaming.name after hist.conflict.toll; P3-06 r22 24.5 (0.21.0): hist.conflict.toll after hist.conflict.search
    std::size_t at = names.size();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == "hist.peoples.split") at = i;
    }
    REQUIRE(at + 5 < names.size());
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK(names[at + 1 + i] == cfl::stream_names()[i]);
    }
    REQUIRE(at + 6 < names.size());
    CHECK(names[at + 6] == "hist.conflict.search");
    REQUIRE(at + 7 < names.size());
    CHECK(names[at + 7] == "hist.conflict.toll");
    // P5-12 3 (generator 0.22.0): the renaming streams after hist.conflict.toll, in this order
    REQUIRE(at + 9 < names.size());
    CHECK(names[at + 8] == "hist.renaming.chance");
    CHECK(names[at + 9] == "hist.renaming.name");
    // P5-02 3 (generator 0.23.0): the fifteen streams of the monsters after hist.renaming.name, in this order, and
    // (r1, 6.4.1) hist.monsters.raid_target after hist.names.site, the last name
    const std::vector<std::string_view> p502 = {
        "hist.beasts.bold", "hist.monsters.wake", "hist.monsters.rise", "hist.monsters.sleep", "hist.monsters.rewake",
        "hist.monsters.raid", "hist.monsters.raid_toll", "hist.monsters.overrun", "hist.monsters.hunt",
        "hist.monsters.hunt_outcome", "hist.monsters.lair_name", "hist.sites.outpost", "hist.peoples.ghost",
        "hist.names.monster", "hist.names.site", "hist.monsters.raid_target"};
    // P5-03 3 (generator 0.24.0): four more after hist.monsters.raid_target, in this order
    // P5-03a 2 (generator 0.25.0): hist.relations.settlers after hist.relations.expel
    const std::vector<std::string_view> p503 = {"hist.beasts.bold_kind", "hist.monsters.fear",
                                                "hist.peoples.abandon_loss", "hist.relations.expel",
                                                "hist.relations.settlers"};
    REQUIRE(at + 9 + p502.size() + p503.size() == names.size() - 1);
    for (std::size_t i = 0; i < p502.size(); ++i) CHECK(names[at + 10 + i] == p502[i]);
    for (std::size_t i = 0; i < p503.size(); ++i) CHECK(names[at + 10 + p502.size() + i] == p503[i]);
}

}  // TEST_SUITE
