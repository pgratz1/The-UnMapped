// Spec P5-12 13 items 2, 3 and 9 through the public seams of worldgen/history/renaming.hpp (phase 2 of the P5-12
// tests): the stage check `renaming_rules` (P3-01 r16 5.4) with each problem text and its order, the realm root of
// 5.3 for every shipped culture's three civilization forms, the joins of 5.1 that V8 tests, and the caps of 6
// items 3 and 4. The subsystem's choice rules (gap, weight, caps in a world, quiet budget) have no seam smaller
// than the engine; they are checked against the reference on the fixtures rename-small and off
// (history_renaming_golden_test.cpp) and on the world cases.
//
// Written by the test writer from the spec.

#include "p5_12_renaming_support.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "worldgen/history/event_catalogue.hpp"
#include "worldgen/history/renaming.hpp"

namespace {

namespace wh = worldgen::history;
using renaming_test::shipped;

wh::RenamingTuning shipped_tuning() { return wh::renaming_tuning_from(shipped()); }

std::optional<std::string> problem_with(const wh::RenamingTuning& t) {
    const wh::EventCatalogue cat = wh::event_catalogue(shipped());
    return wh::renaming_rules_problem(t, cat);
}

std::string msg(const std::string& key, const std::string& problem) {
    return "history: renaming rule '" + key + "': " + problem;
}

// The shipped civilization_forms of each culture (game/data/cultures/*.toml, P3-02's naming record).
const std::vector<std::pair<std::string, std::vector<std::string>>>& culture_forms() {
    static const std::vector<std::pair<std::string, std::vector<std::string>>> v = {
        {"dwarven", {"The Hold of {root}", "Clan {root}", "The {root} Throne"}},
        {"elven", {"The Court of {root}", "The Realm of {root}", "The House of {root}"}},
        {"faerie", {"The {root} Ring", "The Circle of {root}", "The {root} Court"}},
        {"goblin", {"The {root} Horde", "The Warrens of {root}", "The {root} Mob"}},
        {"human", {"The Kingdom of {root}", "The {root} League", "The Duchy of {root}"}},
        {"orcish", {"The {root} Horde", "Clan {root}", "The Warband of {root}"}},
    };
    return v;
}

std::string fill_root(const std::string& form, const std::string& root) {
    std::string out = form;
    out.replace(out.find("{root}"), 6, root);
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-01 r16 5.4: the stage check renaming_rules passes on the shipped rules") {
    CHECK_FALSE(problem_with(shipped_tuning()).has_value());
    wh::RenamingTuning none = shipped_tuning();
    none.rules.clear();
    CHECK_FALSE(problem_with(none).has_value());
}

TEST_CASE("P3-01 r16 5.4: each problem of renaming_rules, with its text") {
    // rule index 3 is slaughter (battle, battle_at_pass; values winner_dead, loser_dead; loser, winner)
    struct Case {
        const char* what;
        std::size_t rule;
        void (*edit)(wh::RenameRule&);
        std::string key;
        std::string problem;
    };
    const std::vector<Case> cases = {
        {"unknown kind", 3, [](wh::RenameRule& r) { r.kinds = {"battle", "no_such_kind"}; }, "slaughter",
         "kind 'no_such_kind' is not in the catalogue"},
        {"a kind without a region", 3, [](wh::RenameRule& r) { r.kinds = {"tribute"}; r.values = {}; r.name_role = "";
                                                                r.namer_role = ""; r.forms = {"The Red {noun}"}; },
         "slaughter", "kind 'tribute' has no region"},
        {"a value of another kind", 3, [](wh::RenameRule& r) { r.values = {"winner_dead", "dead"}; }, "slaughter",
         "value 'dead' is not a value of kind 'battle'"},
        {"name_role no role", 3, [](wh::RenameRule& r) { r.name_role = "victim"; }, "slaughter",
         "name_role 'victim' is not a civilization or settlement role of kind 'battle'"},
        {"name_role settlement without settlement_use", 3, [](wh::RenameRule& r) { r.name_role = "settlement"; },
         "slaughter", "name_role 'settlement' is not a civilization or settlement role of kind 'battle'"},
        {"namer_role no role", 3, [](wh::RenameRule& r) { r.namer_role = "victor"; }, "slaughter",
         "namer_role 'victor' is not a civilization role of kind 'battle'"},
        {"capital without a settlement", 3, [](wh::RenameRule& r) { r.capital = true; }, "slaughter",
         "capital needs a kind with a settlement and a civilization role ('battle')"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.what);
        wh::RenamingTuning t = shipped_tuning();
        REQUIRE(t.rules.size() == 6U);
        REQUIRE(t.rules[c.rule].key == c.key);
        c.edit(t.rules[c.rule]);
        CHECK(problem_with(t) == std::optional<std::string>(msg(c.key, c.problem)));
        // check_renaming_rules raises the same text
        bool threw = false;
        try {
            wh::check_renaming_rules(t, wh::event_catalogue(shipped()));
        } catch (const worldgen::FatalError& e) {
            threw = true;
            CHECK(std::string(e.what()) == msg(c.key, c.problem));
        }
        CHECK(threw);
    }
}

TEST_CASE("P3-01 r16 5.4: renaming_rules reports the first problem: rules in order, kinds in order, then the listed "
          "order") {
    {
        // two rules broken: the earlier rule is reported
        wh::RenamingTuning t = shipped_tuning();
        t.rules[4].kinds = {"no_such_kind"};   // plague
        t.rules[1].namer_role = "victor";        // capital_sack
        CHECK(problem_with(t) == std::optional<std::string>(msg("capital_sack",
                                                                "namer_role 'victor' is not a civilization role of kind "
                                                                "'sacked'")));
    }
    {
        // within a rule, its first kind with a problem in the order of `kinds`
        wh::RenamingTuning t = shipped_tuning();
        t.rules[3].kinds = {"no_such_kind", "battle"};
        t.rules[3].values = {"dead_nowhere"};
        CHECK(problem_with(t) == std::optional<std::string>(msg("slaughter", "kind 'no_such_kind' is not in the "
                                                                             "catalogue")));
    }
    {
        // within a kind, the listed order: a value problem before a name_role problem before a namer_role one
        wh::RenamingTuning t = shipped_tuning();
        t.rules[3].values = {"dead"};
        t.rules[3].name_role = "victim";
        t.rules[3].namer_role = "victor";
        t.rules[3].capital = true;
        CHECK(problem_with(t) ==
              std::optional<std::string>(msg("slaughter", "value 'dead' is not a value of kind 'battle'")));
        t.rules[3].values = {};
        CHECK(problem_with(t) == std::optional<std::string>(msg(
                                     "slaughter", "name_role 'victim' is not a civilization or settlement role of kind "
                                                  "'battle'")));
        t.rules[3].name_role = "loser";
        CHECK(problem_with(t) ==
              std::optional<std::string>(msg("slaughter", "namer_role 'victor' is not a civilization role of kind "
                                                          "'battle'")));
        t.rules[3].namer_role = "winner";
        CHECK(problem_with(t) == std::optional<std::string>(msg(
                                     "slaughter", "capital needs a kind with a settlement and a civilization role "
                                                  "('battle')")));
    }
}

TEST_CASE("P5-12 5.3, 13 item 3: the root for each shipped culture's three civilization forms") {
    for (const auto& [culture, forms] : culture_forms()) {
        CAPTURE(culture);
        for (const std::string& f : forms) {
            CAPTURE(f);
            CHECK(wh::realm_root(fill_root(f, "Cear"), forms) == "Cear");
            CHECK(wh::realm_root(fill_root(f, "Uk'ghaam"), forms) == "Uk'ghaam");
            // a numbered fallback keeps its numeral in the root
            CHECK(wh::realm_root(fill_root(f, "Cear II"), forms) == "Cear II");
        }
    }
    // the first matching form in list order; the name must be longer than the form's literal parts
    const std::vector<std::string> human = culture_forms()[4].second;
    CHECK(wh::realm_root("The Kingdom of The Duchy of Nife", human) == "The Duchy of Nife");
    CHECK(wh::realm_root("The Kingdom of ", human) == "The Kingdom of ");
    CHECK(wh::realm_root("The  League", human) == "The  League");   // root would be empty: |A| + |B| = 11 < 11 + 1
    // a hand-made name no form matches: the whole name
    CHECK(wh::realm_root("Nife", human) == "Nife");
    CHECK(wh::realm_root("The Krurz Horde", culture_forms()[5].second) == "Krurz");
    CHECK(wh::realm_root("Clan Uk'ghaam", culture_forms()[5].second) == "Uk'ghaam");
}

TEST_CASE("P5-12 5.1, 5.2 item 4: the parts of a filled form and the joins between them") {
    {
        const wh::RenamedName n = wh::fill_rename_form("{name}land", "Tar", "Wood");
        CHECK(n.name == "Tarland");
        CHECK(n.joins == std::vector<std::size_t>{3});
    }
    {
        const wh::RenamedName n = wh::fill_rename_form("The Grave of {name}", "Cear", "Wood");
        CHECK(n.name == "The Grave of Cear");
        CHECK(n.joins == std::vector<std::size_t>{13});
    }
    {
        const wh::RenamedName n = wh::fill_rename_form("The {noun} of the Slain", "", "Heath");
        CHECK(n.name == "The Heath of the Slain");
        CHECK(n.joins == std::vector<std::size_t>{4, 9});
    }
    {
        const wh::RenamedName n = wh::fill_rename_form("{name}'s Bane", "Edadbury", "Fells");
        CHECK(n.name == "Edadbury's Bane");
        CHECK(n.joins == std::vector<std::size_t>{8});
    }
    {
        const wh::RenamedName n = wh::fill_rename_form("The {name} Reach", "Nife", "Downs");
        CHECK(n.name == "The Nife Reach");
        CHECK(n.joins == std::vector<std::size_t>{4, 8});
    }
}

TEST_CASE("P5-12 6 items 3 and 4, 13 item 2: max(1, R x permille div 1000)") {
    CHECK(wh::renaming_cap(12, 150) == 1);
    CHECK(wh::renaming_cap(49, 150) == 7);
    CHECK(wh::renaming_cap(200, 150) == 30);
    CHECK(wh::renaming_cap(6, 150) == 1);
    CHECK(wh::renaming_cap(0, 150) == 1);
    CHECK(wh::renaming_cap(49, 50) == 2);
    CHECK(wh::renaming_cap(19, 50) == 1);
    CHECK(wh::renaming_cap(20, 50) == 1);
    CHECK(wh::renaming_cap(40, 50) == 2);
    CHECK(wh::renaming_cap(100, 0) == 1);
    CHECK(wh::renaming_cap(100, 1000) == 100);
}

}  // TEST_SUITE
