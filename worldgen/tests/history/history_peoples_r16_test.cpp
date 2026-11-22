// Spec P3-05 r16 section 23 (P3-11, generator 0.20.0), tests 23.4: the awakening population. The first settlement
// of a civilization placed at an awakening has P0 = founding_population when the race's awakening_population is
// 0, else max(founding_population, min(awakening_population, C(H, r))); its kind is ladder(r, P0) away from an
// upland home; the settlement_founded value population is P0; nothing else (colonies, secessions) changes.
//
// Through the engine's public seam on a hand-made geography (one or four steppe regions, orcs or elves only), with
// race and peoples fields edited past the loader. C(H, r) is read from the library's PeoplesView::capacity, so the
// tests check the rule, not the capacity formula (P3-05 5.1 has its own tests). Items 2 to 7 of 23.4 are here;
// item 1's "pressure 800" and item 3 are not (see the report: 5.2's kind cap of an orc village is town's, 7,999,
// not 1,499, so the pressure 23.2 computes is not 9.3's); the loader cases (item 6) are in
// data_history_kinds_test.cpp, and item 8 is p3-05-peoples.txt (history_peoples_golden_test.cpp).
//
// Written from the spec before the implementation (test-writer rule).

#include "p3_05_peoples_support.hpp"

#if UNMAPPED_HAVE_PEOPLES

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace {

using namespace peoples_test;

// A race field set for the races of `keys` (past the loader).
wd::ContentSet with_race_field(const wd::ContentSet& base, const std::vector<std::string>& keys,
                               const std::string& field, std::int64_t v) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "race") continue;
        for (wd::Definition& d : k.definitions) {
            if (std::find(keys.begin(), keys.end(), d.id) == keys.end()) continue;
            wd::Record r;
            bool found = false;
            for (std::size_t i = 0; i < d.record.size(); ++i) {
                const bool hit = d.record.name_at(i) == field;
                found = found || hit;
                r.add(d.record.name_at(i), hit ? wd::Value::integer(v) : d.record.value_at(i));
            }
            REQUIRE_MESSAGE(found, "race " << d.id << " has no field " << field);
            d.record = std::move(r);
        }
    }
    return c;
}

// n regions of 32 x 128 cells in a row, walkable borders, steppe (biome 10).
Geo steppe_regions(std::int64_t n) {
    std::vector<db::RegionRow> rs;
    for (std::int64_t i = 0; i < n; ++i) {
        db::RegionRow r = plain_region(i + 1, 32 * i, 0, 32 * i + 31, 127);
        r.cells = 4096;
        r.walk_cells = 4000;
        rs.push_back(r);
    }
    Geo g = hand_made(32 * n, rs);
    for (std::int64_t i = 1; i < n; ++i) g.borders.push_back(db::RegionBorderRow{i, i + 1, 2048, 0, 0, 0, 10, 0, 0, 0, 0});
    for (std::int64_t i = 1; i <= n; ++i) g.region_biomes.push_back({i, 10, 4096});
    return g;
}

struct Awakened {
    std::int64_t settlement, region, race, population, value, kind;  // kind: the census row of its first year
    std::int64_t capacity;                                            // C(H, r), PeoplesView::capacity
};

struct Run {
    Tables t;
    std::vector<Awakened> first;  // the settlements founded at an awakening (caused by a civ_founded)
};

Run run(const wd::ContentSet& c, const std::vector<std::string>& races, std::int64_t regions = 1) {
    const Geo g = steppe_regions(regions);
    Settings s;
    s.span = 400;
    s.races = races;
    s.civs = 1;
    const eng::HistoryInputs in = inputs_of(g, 0, s, c);
    PeoplesConfig cfg(c);
    Run r;
    r.t = eng::run_engine(c, in, cfg.get());
    std::map<std::int64_t, std::int64_t> kind_of;
    for (const Row& e : r.t.event) kind_of[at(e, 0)] = at(e, 3);
    std::map<std::int64_t, std::vector<std::int64_t>> causes;
    for (const Row& x : r.t.event_cause) causes[at(x, 0)].push_back(at(x, 1));
    std::map<std::int64_t, std::int64_t> value1;
    for (const Row& v : r.t.event_value) {
        if (at(v, 1) == 1) value1[at(v, 0)] = at(v, 2);
    }
    std::map<std::int64_t, std::int64_t> civ_race;
    for (const Row& cv : r.t.civilization) civ_race[at(cv, 0)] = at(cv, 2);
    for (const Row& st : r.t.settlement) {
        const std::int64_t fe = at(st, 7);
        if (kind_of[fe] != k_settlement_founded) continue;
        bool at_awakening = false;
        for (const std::int64_t x : causes[fe]) at_awakening = at_awakening || kind_of[x] == k_civ_founded;
        if (!at_awakening) continue;
        Awakened a{};
        a.settlement = at(st, 0);
        a.region = at(st, 4);
        a.race = civ_race[at(st, 5)];
        a.value = value1[fe];
        a.kind = 0;
        a.population = -1;
        for (const Row& cs : r.t.settlement_census) {
            if (at(cs, 0) == a.settlement && at(cs, 1) == at(st, 6)) {
                a.kind = at(cs, 3);
                a.population = at(cs, 5);
            }
        }
        a.capacity = cfg.view().capacity(a.region, a.race);
        r.first.push_back(a);
    }
    return r;
}

std::int64_t p0(std::int64_t founding, std::int64_t awakening, std::int64_t cap) {
    return awakening == 0 ? founding : std::max(founding, std::min(awakening, cap));
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-05 r16 23.2, 23.4 items 1, 2: an orc awakening in a free steppe home starts at min(1,200, C), a village") {
    const wd::ContentSet& c = shipped();
    const Run r = run(c, {"orc"});
    REQUIRE(r.first.size() == 1U);
    const Awakened& a = r.first[0];
    CAPTURE(a.capacity);
    CHECK(a.value == p0(300, 1200, a.capacity));
    if (a.capacity >= 1200) {
        CHECK(a.value == 1200);
        CHECK(a.kind == 2);  // ladder(orc, 1200): village (250 <= 1,200 < town_min 1,500)
    }
    CHECK(a.value <= 2 * a.capacity);  // invariant 3
}

TEST_CASE("P3-05 r16 23.4 item 2: P0 clamps to C(H, r) and never falls below founding_population") {
    // a low-capacity home: people_per_kcell 1 leaves C at capacity_floor; floors 400 and 900 (>= founding 300)
    for (const std::int64_t floor : {400, 900}) {
        CAPTURE(floor);
        const wd::ContentSet c = with_peoples({{"people_per_kcell", I64(1)}, {"capacity_floor", I64(floor)}});
        const Run r = run(c, {"orc"});
        REQUIRE(r.first.size() == 1U);
        CHECK(r.first[0].capacity == floor);
        CHECK(r.first[0].value == floor);
    }
    // awakening_population 200 with founding_population 300: P0 = 300
    const Run low = run(with_race_field(shipped(), {"orc"}, "awakening_population", 200), {"orc"});
    REQUIRE(low.first.size() == 1U);
    CHECK(low.first[0].value == 300);
}

TEST_CASE("P3-05 r16 23.4 item 4: a race with awakening_population 0 starts at founding_population; the field of "
          "another race changes nothing") {
    const Run elves = run(shipped(), {"elf"});
    REQUIRE(elves.first.size() == 1U);
    CHECK(elves.first[0].value == 300);
    // an elf-only world: the orcs' field (0 or 1,200) changes no row
    const Run elves0 = run(with_race_field(shipped(), {"orc"}, "awakening_population", 0), {"elf"});
    CHECK(str(elves.t.event) == str(elves0.t.event));
    CHECK(str(elves.t.settlement) == str(elves0.t.settlement));
    CHECK(str(elves.t.settlement_census) == str(elves0.t.settlement_census));
    CHECK(str(elves.t.event_value) == str(elves0.t.event_value));
    // the orcs at 0: the 0.19.0 rule (founding_population 300), and the world differs from the shipped one
    const Run orcs0 = run(with_race_field(shipped(), {"orc"}, "awakening_population", 0), {"orc"});
    REQUIRE(orcs0.first.size() == 1U);
    CHECK(orcs0.first[0].value == 300);
    const Run orcs = run(shipped(), {"orc"});
    CHECK(str(orcs.t.event_value) != str(orcs0.t.event_value));
}

TEST_CASE("P3-05 r16 23.4 items 5, 7: colonies and secessions are not awakenings; invariant 3 at every awakening with "
          "the field 0 and 100,000") {
    for (const std::int64_t v : {0, 100000}) {
        CAPTURE(v);
        const wd::ContentSet c = with_race_field(shipped(), {"orc", "elf"}, "awakening_population", v);
        const Run r = run(c, {"elf", "orc"}, 4);
        REQUIRE(r.first.size() >= 2U);
        for (const Awakened& a : r.first) {
            CAPTURE(a.settlement);
            CHECK(a.value == p0(300, v, a.capacity));
            CHECK(a.value <= 2 * a.capacity);
        }
        // every other founding (colony_founded, a seceded capital) keeps its own rule: none starts at 100,000
        std::map<std::int64_t, std::int64_t> kind_of;
        for (const Row& e : r.t.event) kind_of[at(e, 0)] = at(e, 3);
        for (const Row& val : r.t.event_value) {
            if (kind_of[at(val, 0)] == k_colony_founded && at(val, 1) == 1) CHECK(at(val, 2) < 100000);
        }
    }
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES
