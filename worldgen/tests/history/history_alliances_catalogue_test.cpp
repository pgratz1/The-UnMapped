// Spec P5-04 (docs/specs/p5-04-alliances-trade.md r2) 8.1: the compiled catalogue's rows of kinds 63 to 78 (ids,
// keys, caused, region and settlement columns, roles with entity types and counts, values), the 62 kinds before
// them unchanged in id; 3: the five streams' names. Written from the spec before the implementation.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "worldgen/history/event_catalogue.hpp"

namespace {

namespace h = worldgen::history;

struct R {
    std::string role;
    std::int64_t type, lo, hi;
};
struct K {
    std::int64_t id;
    std::string key;
    std::int64_t caused, region, settlement;
    std::vector<R> roles;
    std::vector<std::string> values;
};

const std::vector<K>& spec_kinds() {
    const std::int64_t C = 3, S = 9;
    static const std::vector<K> v = {
        {63, "alliance_formed", 1, 0, 0, {{"ally_a", C, 1, 1}, {"ally_b", C, 1, 1}, {"foe", C, 0, 1}}, {"basis", "term", "band"}},
        {64, "alliance_renewed", 1, 0, 0, {{"ally_a", C, 1, 1}, {"ally_b", C, 1, 1}, {"foe", C, 0, 1}}, {"basis"}},
        {65, "alliance_lapsed", 1, 0, 0, {{"ally_a", C, 1, 1}, {"ally_b", C, 1, 1}}, {"reason"}},
        {66, "alliance_broken", 1, 0, 0, {{"ally_a", C, 1, 1}, {"ally_b", C, 1, 1}}, {"reason"}},
        {67, "alliance_betrayed", 1, 0, 0, {{"betrayer", C, 1, 1}, {"betrayed", C, 1, 1}}, {"motive", "years"}},
        {68, "ally_joined_war", 1, 0, 0, {{"ally", C, 1, 1}, {"partner", C, 1, 1}, {"enemy", C, 1, 1}}, {"side"}},
        {69, "ally_refused", 1, 0, 0, {{"ally", C, 1, 1}, {"partner", C, 1, 1}, {"enemy", C, 1, 1}}, {"motive", "count"}},
        {70, "ally_withdrew", 1, 0, 0, {{"ally", C, 1, 1}, {"partner", C, 1, 1}, {"enemy", C, 1, 1}}, {}},
        {71, "allied_battle", 1, 1, 0,
         {{"winner", C, 1, 1}, {"loser", C, 1, 1}, {"winner_ally", C, 0, 16}, {"loser_ally", C, 0, 16}},
         {"winner_dead", "loser_dead", "pass"}},
        {72, "trade_opened", 1, 0, 0, {{"partner_a", C, 1, 1}, {"partner_b", C, 1, 1}}, {}},
        {73, "trade_broken", 1, 0, 0, {{"partner_a", C, 1, 1}, {"partner_b", C, 1, 1}}, {"reason"}},
        {74, "spoils_shared", 1, 1, 0, {{"principal", C, 1, 1}, {"ally", C, 1, 1}, {"enemy", C, 1, 1}}, {"share"}},
        {75, "spoils_withheld", 1, 1, 0, {{"principal", C, 1, 1}, {"ally", C, 1, 1}, {"enemy", C, 1, 1}}, {"share", "grievance"}},
        {76, "alliance_spurned", 1, 1, 0, {{"ally", C, 1, 1}, {"principal", C, 1, 1}}, {"reason", "years"}},
        {77, "mine_opened", 1, 1, 1, {{"civilization", C, 1, 1}, {"site", S, 1, 1}}, {}},
        {78, "arms_sold", 1, 0, 0, {{"seller", C, 1, 1}, {"buyer", C, 1, 1}, {"site", S, 1, 1}}, {}}};
    return v;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04 8.1: the compiled catalogue holds kinds 63 to 78 as the table states, after the 62 of 0.25.0") {
    const h::EventCatalogue& cat = h::compiled_event_catalogue();
    REQUIRE(cat.kinds.size() == 110U);
    for (std::size_t i = 0; i < cat.kinds.size(); ++i) CHECK(cat.kinds[i].id == static_cast<std::int64_t>(i) + 1);
    CHECK(cat.kinds[61].key == "people_expelled");
    for (const K& k : spec_kinds()) {
        CAPTURE(k.key);
        const h::KindDef* d = cat.find_id(k.id);
        REQUIRE(d != nullptr);
        CHECK(d->key == k.key);
        CHECK(d->caused == (k.caused != 0));
        CHECK(d->region_use == k.region);
        CHECK(d->settlement_use == k.settlement);
        REQUIRE(d->roles.size() == k.roles.size());
        for (std::size_t j = 0; j < k.roles.size(); ++j) {
            CAPTURE(j);
            CHECK(d->roles[j].seq == static_cast<std::int64_t>(j) + 1);
            CHECK(d->roles[j].role == k.roles[j].role);
            CHECK(static_cast<std::int64_t>(d->roles[j].type) == k.roles[j].type);
            CHECK(d->roles[j].min_count == k.roles[j].lo);
            CHECK(d->roles[j].max_count == k.roles[j].hi);
        }
        CHECK(d->values == k.values);
    }
}

}  // TEST_SUITE
