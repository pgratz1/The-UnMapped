// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) test 24.1 item 8, the monsters' part (P5-02 follow-ups,
// sections 9, 12, 13, 14): the formulas of the toll above kill_max (9.2), the hunt chance by R_c and its cap
// (9.3), the overrun factor at P = 800, 1,200 and 3,000 and with M (14.1, 14.2), the undead threshold by M (14.2),
// the epithet of a form (12.1); and on the rows of the fixture monsters-small with fear_raids 2 and fear_permille
// 1000 (24.3) and of a --monster-pressure high world: raid causes (the waking and the last raid on the place within
// raid_chain_years), flights for fear and their abandonments (ending 6), plunder 0 of undead and wyverns, bold
// beasts never of a species of menace 0, epithets not repeated while unused ones remain, hosts named for the ruin
// or the region in force, every monster name unique.
//
// In the -monsters shard (24.1). Written from the spec before the implementation. Library names: the adapter's.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_formulas.hpp"
#include "p5_03_relations_runs.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

std::int64_t kind_index(const wd::ContentSet& c, const std::string& kind, const std::string& id) {
    const wd::KindContent* k = nullptr;
    for (const wd::KindContent& x : c.kinds) {
        if (x.kind == kind) k = &x;
    }
    REQUIRE(k != nullptr);
    for (std::size_t i = 0; i < k->definitions.size(); ++i) {
        if (k->definitions[i].id == id) return static_cast<std::int64_t>(i + 1);
    }
    FAIL("no " << kind << "." << id);
    return 0;
}

void check_monster_rows(const p503check::World& w) {
    const p503check::Index x(w);
    const wd::ContentSet& c = *w.content;
    const std::int64_t undead = kind_index(c, "monster", "undead"), wyvern = kind_index(c, "monster", "wyvern");
    std::map<std::int64_t, std::int64_t> kind_of_monster;
    std::set<std::string> names;
    for (const eng::Row& m : w.rows.t.monster) {
        kind_of_monster[p503check::at(m, 0)] = p503check::at(m, 2);
        CHECK(names.insert(m[1].bytes).second);   // P3-02 4.3's registry: no name twice
        if (p503check::at(m, 2) == undead || p503check::at(m, 2) == wyvern) CHECK(p503check::at(m, 10) == 0);   // hoard
    }
    for (const std::int64_t e : x.events_of("monster_raid")) {
        const std::int64_t k = kind_of_monster[x.part(e, 1)];
        if (k == undead || k == wyvern) CHECK(x.val(e, 1) == 0);   // 9.2: plunder 0
        CHECK(x.val(e, 0) >= 0);
    }
    // 13: never a species of weight 0 (menace 0)
    std::map<std::int64_t, std::int64_t> menace;
    for (const eng::Row& b : w.rows.t.beast) menace[p503check::at(b, 0)] = p503check::at(b, 4);
    for (const std::int64_t e : x.events_of("beasts_bold")) CHECK(menace[x.val(e, 0)] > 0);
    // 12.2: a host rising at a ruin is named for it; else for the region's name in force at the year
    std::map<std::int64_t, std::string> geo;
    for (std::size_t i = 0; i < w.region_names.size(); ++i) geo[static_cast<std::int64_t>(i + 1)] = w.region_names[i];
    for (const std::int64_t e : x.events_of("undead_rose")) {
        const eng::Row& E = *x.ev.at(e);
        const std::int64_t m = x.part(e, 1), s = p503check::at(E, 6), r = p503check::at(E, 5), y = p503check::at(E, 1);
        std::string place = geo[r];
        for (const eng::Row& rn : w.rows.t.region_name) {
            if (p503check::at(rn, 0) == r && p503check::at(rn, 1) <= y) place = rn[2].bytes;
        }
        if (s != 0) place = x.stl.at(s)->at(1).bytes;
        if (place.rfind("The ", 0) == 0) place[0] = 't';
        std::string name;
        for (const eng::Row& mr : w.rows.t.monster) {
            if (p503check::at(mr, 0) == m) name = mr[1].bytes;
        }
        CAPTURE(name);
        CAPTURE(place);
        CHECK(name.find(" of " + place) != std::string::npos);
    }
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 9.2: the toll grows at toll_over_permille above kill_max; never all of a place") {
    CHECK(rel_f::toll(3000, 200, 600, 250) == 600);   // raw 600 = kill_max
    CHECK(rel_f::toll(3000, 300, 600, 250) == 675);   // raw 900: 600 + 300 x 250 / 1000
    CHECK(rel_f::toll(100, 300, 600, 250) == 30);
    CHECK(rel_f::toll(1, 1000, 600, 250) == 0);       // P - 1
    CHECK(rel_f::toll(10000, 1000, 60, 0) == 60);     // toll_over 0: the old cap
}

TEST_CASE("P5-03 9.3: the hunt chance by the raids remembered, and its cap") {
    CHECK(rel_f::hunt_chance(1, 20, 60, 400) == 20);
    CHECK(rel_f::hunt_chance(4, 20, 60, 400) == 200);   // the spec's "struck four times ... at 200"
    CHECK(rel_f::hunt_chance(7, 20, 60, 400) == 380);
    CHECK(rel_f::hunt_chance(8, 20, 60, 400) == 400);
    CHECK(rel_f::hunt_chance(100, 20, 60, 400) == 400);
}

TEST_CASE("P5-03 14.1, 14.2: the overrun factor by the population after the raid, scaled by M; the undead threshold") {
    CHECK(rel_f::overrun_factor(800, 800) == 1000);
    CHECK(rel_f::overrun_factor(800, 1200) == 666);    // the spec's "two thirds"
    CHECK(rel_f::overrun_factor(800, 3000) == 266);    // "a quarter"
    CHECK(rel_f::overrun_factor(800, 0) == 1000);      // max(1, P)
    CHECK(rel_f::overrun_chance(900, 1600, 800, 500) == 1000);   // a giant at high: 1440 capped
    CHECK(rel_f::overrun_chance(600, 1600, 800, 500) == 960);    // a dragon at high
    CHECK(rel_f::overrun_chance(900, 500, 800, 500) == 450);     // halved at low
    CHECK(rel_f::overrun_chance(600, 1000, 800, 1200) == 399);   // 600 x 666 / 1000
    CHECK(rel_f::undead_threshold(4000, 1000) == 4000);
    CHECK(rel_f::undead_threshold(4000, 1600) == 2500);
    CHECK(rel_f::undead_threshold(4000, 500) == 8000);
    CHECK(rel_f::undead_threshold(4000, 0) == 4000000);
}

TEST_CASE("P5-03 12.1: the epithet of a form") {
    CHECK(rel_f::epithet("{root} the Glutton") == "the Glutton");
    CHECK(rel_f::epithet("Old {root}") == "Old");
    CHECK(rel_f::epithet("{root}") == "");
    CHECK(rel_f::epithet("{root} of the Fells") == "of the Fells");
    CHECK(rel_f::epithet("The Brood of {root}") == "The Brood of");
}

TEST_CASE("P5-03 9, 12, 13 on monsters-small with fear: raid chains, flights for fear, plunder, bold, names") {
    std::int64_t fled = 0, chained = 0, ended6 = 0;
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, p503api::monsters_small_fear_content());
        CHECK(p503check::violations(w) == std::vector<std::string>{});
        check_monster_rows(w);
        const p503check::Index x(w);
        fled += static_cast<std::int64_t>(x.events_of("settlement_fled").size());
        for (const std::int64_t e : x.events_of("monster_raid")) chained += x.cause_list(e).size() == 2U ? 1 : 0;
        for (const eng::Row& s : w.rows.t.settlement) ended6 += p503check::at(s, 14) == p503api::ENDING_FLED ? 1 : 0;
    }
    CHECK(fled > 0);
    CHECK(chained > 0);
    CHECK(ended6 == fled);   // every flight ends its place (an abandonment follows in the same step)
}

TEST_CASE("P5-03 9.4: fear_permille 0: no flight; raid_chain_years 1: a raid cites only its waking") {
    const wd::ContentSet never = p503api::with_history(p503api::monsters_small_fear_content(), "monsters",
                                                       {{"fear_permille", p503api::V(0)}});
    const wd::ContentSet unchained = p503api::with_history(p503api::monsters_small_fear_content(), "monsters",
                                                           {{"raid_chain_years", p503api::V(1)}});
    for (const std::uint64_t seed : {0U, 1U}) {
        CAPTURE(seed);
        const p503check::World a = p503run::stage_fixture(seed, never);
        CHECK(p503check::Index(a).events_of("settlement_fled").empty());
        const p503check::World b = p503run::stage_fixture(seed, unchained);
        const p503check::Index xb(b);
        for (const std::int64_t e : xb.events_of("monster_raid")) {
            const auto cs = xb.cause_list(e);
            // within 1 year: only a raid of the same year on the same place (one raid a year: none)
            CHECK(cs.size() == 1U);
        }
    }
}

TEST_CASE("P5-03 9, 12, 13, 14 on a 512 world with --monster-pressure high (seed 7)") {
    const p503check::World w = p503run::world_rows(p503run::world_path(7, 512, "high"));
    CHECK(p503check::violations(w) == std::vector<std::string>{});
    check_monster_rows(w);
}

}  // TEST_SUITE
