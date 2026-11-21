// Spec P3-09 (docs/specs/p3-09-conflict-balance.md r2; generator 0.18.0,
// schema 7) through the loader and the compiled catalogue: the eleven
// history.conflict fields of P3-06 r9 20.9 with their shipped values and
// place (after last_refuge, in order), the three race fields of P3-06 r9
// 20.12 / P3-01 r9 3.2 per race (shipped values), the two history.peoples
// fields of P3-05 r8 21 item 5, the event kinds 32 refugees_settled and 33
// tribute of 20.9 (key, caused, region and settlement use, roles with
// types and counts, values, salience, template), claim salience 35, the
// report entries in kind id order, and no new stream (20.10: "Draws: none
// new"). The ranges and the named check conflict_enmity_hops are in
// history_conflict_data_test.cpp, the race-field ranges in
// data_history_kinds_test.cpp.
//
// Written from the spec only; needs no new library name, so it compiles
// now and fails until the 0.18.0 data and catalogue land.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../p3_01_history_data.hpp"
#include "../p3_01_history_support.hpp"
#include "p3_04_engine_support.hpp"
#include "worldgen/core/streams.hpp"

namespace {

namespace wd = worldgen::data;
using namespace history_test;

const wd::Record& history_record(const wd::ContentSet& c, std::string_view id) {
    const wd::KindContent* h = c.find_kind("history");
    REQUIRE(h != nullptr);
    for (const wd::Definition& d : h->definitions) {
        if (d.id == id) return d.record;
    }
    FAIL("no history." << id);
    return h->definitions.front().record;
}

std::int64_t index_of(const wd::Record& r, std::string_view field) {
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (r.name_at(i) == field) return static_cast<std::int64_t>(i);
    }
    return -1;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 r9 20.9, r10 20.19, r14 21.7, 21.9: the new history.conflict fields follow last_refuge in order (odds_min out at 0.19.0), shipped values") {
    const std::vector<std::pair<const char*, std::int64_t>> want = {
        {"refuge_flight", 1}, {"flight_hops", 3}, {"hidden_years", 300}, {"weary_wars", 3}, {"tribute_permille", 100},
        {"revenge_aggression", 400}, {"levy_hops", 3}, {"odds_floor", 500},  // odds_min: removed by P3-06 r14 21.9
        {"truce_break_aggression", 600}, {"enmity_hops", 3},
        // r10 20.19 (P3-09 r3)
        {"kill_max_permille", 500}, {"find_permille", 250}, {"search_years", 3},
        // r14 21.7 (P3-10, 0.19.0)
        {"search_weary_years", 500}, {"search_repeat_salience", 15},
        // r15 21.12 item 2 (P3-10 r1)
        {"quarrel_strain", 600}};
    for (const wd::ContentSet* c : {&shipped_content()}) {
        const wd::Record& r = history_record(*c, "conflict");
        const std::int64_t at = index_of(r, "last_refuge");
        REQUIRE(at >= 0);
        REQUIRE(static_cast<std::size_t>(at) + want.size() + 1 < r.size());
        for (std::size_t i = 0; i < want.size(); ++i) {
            CAPTURE(std::string_view(want[i].first));
            CHECK(r.name_at(static_cast<std::size_t>(at) + 1 + i) == want[i].first);
            CHECK(r.get_int(want[i].first) == want[i].second);
        }
        CHECK(r.name_at(static_cast<std::size_t>(at) + 1 + want.size()) == "capital_sacked_salience");
        // the old fields keep their values (P3-09 6: only appended fields and data change)
        CHECK(r.get_int("march_hops") == 2);
        CHECK(r.get_int("last_refuge") == 1);
        CHECK(r.get_int("truce_years") == 200);
        CHECK(r.get_int("sack_toll_permille") == 300);
    }
    // the transcribed listing loads to the same record
    const Load l = load(history_data::shipped_files());
    INFO(dump(l));
    REQUIRE(l.ok());
    CHECK(history_record(*l.content, "conflict") == history_record(shipped_content(), "conflict"));
}

TEST_CASE("P3-06 r9 20.12, P3-01 r9 3.2: muster_permille, conceal_min, home_defense_permille after prowess_permille") {
    struct W {
        const char* race;
        std::int64_t muster, conceal, home;
    };
    const std::vector<W> want = {{"dwarf", 1000, 1001, 0}, {"elf", 1000, 1001, 1500}, {"faerie", 1000, 400, 0},  // r10 20.17 item 3, P3-01 r10 3.3: was 2000
                                 {"goblin", 1000, 1001, 0}, {"human", 1000, 1001, 0}, {"orc", 4000, 1001, 0}};
    const wd::KindContent* race = shipped_content().find_kind("race");
    REQUIRE(race != nullptr);
    REQUIRE(race->definitions.size() == want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        const wd::Definition& d = race->definitions[i];
        CAPTURE(d.id);
        CHECK(d.id == want[i].race);
        const std::int64_t at = index_of(d.record, "prowess_permille");
        REQUIRE(at >= 0);
        CHECK(d.record.name_at(static_cast<std::size_t>(at) + 1) == "muster_permille");
        CHECK(d.record.name_at(static_cast<std::size_t>(at) + 2) == "conceal_min");
        CHECK(d.record.name_at(static_cast<std::size_t>(at) + 3) == "home_defense_permille");
        // P3-01 r11 3.5 (0.19.0): odds_min_permille and awakening_quarrel follow home_defense_permille
        CHECK(d.record.name_at(static_cast<std::size_t>(at) + 4) == "odds_min_permille");
        CHECK(d.record.name_at(static_cast<std::size_t>(at) + 5) == "awakening_quarrel");
        CHECK(d.record.name_at(static_cast<std::size_t>(at) + 6) == "settlement_kinds");
        CHECK(d.record.get_int("muster_permille") == want[i].muster);
        CHECK(d.record.get_int("conceal_min") == want[i].conceal);
        CHECK(d.record.get_int("home_defense_permille") == want[i].home);
    }
}

TEST_CASE("P3-05 r8 21 item 5: regions_per_extra_civ 90 and civs_extra_max 2 follow regions_per_civ") {
    const wd::Record& r = history_record(shipped_content(), "peoples");
    const std::int64_t at = index_of(r, "regions_per_civ");
    REQUIRE(at >= 0);
    CHECK(r.name_at(static_cast<std::size_t>(at) + 1) == "regions_per_extra_civ");
    CHECK(r.name_at(static_cast<std::size_t>(at) + 2) == "civs_extra_max");
    CHECK(r.get_int("regions_per_extra_civ") == 90);
    CHECK(r.get_int("civs_extra_max") == 2);
    CHECK(r.get_int("regions_per_civ") == 4);
    CHECK(r.get_int("civs_two_permille") == 500);
}

TEST_CASE("P3-06 r9 20.9: kinds 32 refugees_settled and 33 tribute after kind 31; claim salience 35") {
    using ET = eng::EntityType;
    const eng::EventCatalogue& compiled = eng::compiled();
    REQUIRE(compiled.kinds.size() == 38U);
    const eng::EventCatalogue cat = eng::catalogue_of(engine_test::shipped());
    REQUIRE(cat.kinds.size() == 38U);
    for (const eng::EventCatalogue* c : {&compiled, &cat}) {
        const eng::KindDef& k32 = c->kinds[31];
        CHECK(k32.id == 32);
        CHECK(k32.key == "refugees_settled");
        CHECK(k32.caused);
        CHECK(k32.region_use == 1);
        CHECK(k32.settlement_use == 1);
        REQUIRE(k32.roles.size() == 3U);
        const std::pair<const char*, ET> r32[3] = {
            {"settlement", ET::settlement}, {"founder", ET::civilization}, {"origin", ET::settlement}};
        for (std::size_t r = 0; r < 3; ++r) {
            CHECK(k32.roles[r].seq == static_cast<std::int64_t>(r) + 1);
            CHECK(k32.roles[r].role == r32[r].first);
            CHECK(k32.roles[r].type == r32[r].second);
            CHECK(k32.roles[r].min_count == 1);
            CHECK(k32.roles[r].max_count == 1);
        }
        REQUIRE(k32.values.size() == 1U);
        CHECK(k32.values[0] == "population");
        const eng::KindDef& k33 = c->kinds[32];
        CHECK(k33.id == 33);
        CHECK(k33.key == "tribute");
        CHECK(k33.caused);
        CHECK(k33.region_use == 0);
        CHECK(k33.settlement_use == 0);
        REQUIRE(k33.roles.size() == 2U);
        const char* r33[2] = {"payer", "payee"};
        for (std::size_t r = 0; r < 2; ++r) {
            CHECK(k33.roles[r].seq == static_cast<std::int64_t>(r) + 1);
            CHECK(k33.roles[r].role == r33[r]);
            CHECK(k33.roles[r].type == ET::civilization);
            CHECK(k33.roles[r].min_count == 1);
            CHECK(k33.roles[r].max_count == 1);
        }
        REQUIRE(k33.values.size() == 1U);
        CHECK(k33.values[0] == "people");
        // kinds 1-31 unchanged in place (P3-04 8.2: kinds are appended)
        CHECK(c->kinds[30].key == "settlement_abandoned_to");
        CHECK(c->kinds[19].key == "claim");
    }
    CHECK(cat.kinds[31].salience == 65);
    CHECK(cat.kinds[31].templ == "The survivors of {origin} fled into hiding and founded {@settlement} in {@region}.");
    CHECK(eng::template_problem(cat.kinds[31], cat.kinds[31].templ) == std::nullopt);
    CHECK(cat.kinds[32].salience == 50);
    CHECK(cat.kinds[32].templ == "{payer} paid tribute to {payee}; {people} {people?was|were} carried off.");
    CHECK(eng::template_problem(cat.kinds[32], cat.kinds[32].templ) == std::nullopt);
    CHECK(cat.kinds[19].salience == 35);  // claim 45 -> 35 (P3-07 r8 21.6)
    // every other salience as at 0.17.0 (spot checks of the kinds the summary pairs with)
    CHECK(cat.kinds[20].salience == 70);   // war_declared
    CHECK(cat.kinds[26].salience == 60);   // peace
    CHECK(cat.kinds[27].salience == 50);   // truce
    CHECK(cat.kinds[28].salience == 55);   // region_ceded
    CHECK(cat.kinds[16].salience == 50);   // capital_moved
}

TEST_CASE("P3-06 r9 20.9, r10 20.21 item 9: history.report lists kinds 1-38 in kind id order (P3-04 r12 20.4: 37, 38)") {
    const wd::Record& r = history_record(shipped_content(), "report");
    const auto& events = r.get_array("events");
    REQUIRE(events.size() == 38U);
    const eng::EventCatalogue& compiled = eng::compiled();
    for (std::size_t i = 0; i < events.size(); ++i) {
        CHECK(events[i].as_record().get_string("kind") == compiled.kinds[i].key);
    }
}

TEST_CASE("P3-06 r9 20.10, r10 20.17 item 5: one new stream, hist.conflict.search, after hist.conflict.sack (53)") {
    const auto names = worldgen::streams::all_stream_names;
    CHECK(names.size() == 53U);
    std::size_t conflict = 0, sack = 0, search = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const std::string_view n(names[i]);
        conflict += n.rfind("hist.conflict.", 0) == 0 ? 1U : 0U;
        if (n == "hist.conflict.sack") sack = i;
        if (n == "hist.conflict.search") search = i;
    }
    CHECK(conflict == 6U);
    CHECK(search == sack + 1);
}

TEST_CASE("P3-06 r10 20.15, 20.17, 20.22: kinds 34 war_won, 35 search_abandoned, 36 refuge_held") {
    using ET = eng::EntityType;
    struct K {
        std::int64_t id;
        const char* key;
        std::int64_t region_use, settlement_use;
        const char* r1;
        const char* r2;
        const char* value;
        std::int64_t salience;
        const char* templ;
    };
    const std::vector<K> want = {
        {34, "war_won", 0, 0, "winner", "loser", "years", 60,
         "{winner} prevailed over {loser} after {years} {years?year|years} of war."},
        {35, "search_abandoned", 1, 0, "searcher", "hidden", "years", 40,
         "{searcher} searched {@region} for {hidden} and found no one; the war ended after {years} {years?year|years}."},
        {36, "refuge_held", 1, 1, "attacker", "defender", "reason", 25,
         "{attacker} besieged {@settlement} of {defender}; with {reason?nowhere to flee|too few to flee}, its people "
         "held on."},
    };
    const eng::EventCatalogue& compiled = eng::compiled();
    REQUIRE(compiled.kinds.size() == 38U);
    const eng::EventCatalogue cat = eng::catalogue_of(engine_test::shipped());
    REQUIRE(cat.kinds.size() == 38U);
    for (const K& w : want) {
        CAPTURE(std::string_view(w.key));
        for (const eng::EventCatalogue* c : {&compiled, &cat}) {
            const eng::KindDef& k = c->kinds[static_cast<std::size_t>(w.id - 1)];
            CHECK(k.id == w.id);
            CHECK(k.key == w.key);
            CHECK(k.caused);
            CHECK(k.region_use == w.region_use);
            CHECK(k.settlement_use == w.settlement_use);
            REQUIRE(k.roles.size() == 2U);
            CHECK(k.roles[0].role == w.r1);
            CHECK(k.roles[1].role == w.r2);
            for (const auto& r : k.roles) {
                CHECK(r.type == ET::civilization);
                CHECK(r.min_count == 1);
                CHECK(r.max_count == 1);
            }
            REQUIRE(k.values.size() == 1U);
            CHECK(k.values[0] == w.value);
        }
        const eng::KindDef& k = cat.kinds[static_cast<std::size_t>(w.id - 1)];
        CHECK(k.salience == w.salience);
        CHECK(k.templ == w.templ);
        CHECK(eng::template_problem(k, k.templ) == std::nullopt);
    }
}

}  // TEST_SUITE
