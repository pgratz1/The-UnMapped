// Spec P5-12 sections 4 and 4.1 and P3-01 r16 5.4 (generator 0.22.0), tests 13 item 9: the fixed id
// history.renaming through the loader: its fields in order with the shipped values (the listing of
// p3_01_history_data.hpp, transcribed from the spec, equal to game/data and both fixture roots), the six shipped
// rules of 4.1 column by column, every scalar field one step outside its range, every field required, the grammar
// rename_form, an absent renaming.toml, and the four named checks with their messages. The stage check
// renaming_rules (needs the compiled catalogue) is in history_renaming_unit_test.cpp. Also the data of P3-02 r19
// 16.2 (human forbidden gains ass) and the report entry of kind 43 (P3-04 r21 23.2).
//
// Written from the specs before the implementation (test-writer rule): it needs no new library name and fails
// until the 0.22.0 kind is registered.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../p3_01_history_data.hpp"
#include "../p3_01_history_support.hpp"

namespace {

namespace wd = worldgen::data;
using history_data::Files;
using namespace history_test;

Files with_renaming_edit(std::string_view from, std::string_view to, std::size_t nth = 0) {
    Files f = history_data::shipped_files();
    std::string& t = f["history/renaming.toml"];
    std::size_t at = t.find(from);
    for (std::size_t i = 0; i < nth && at != std::string::npos; ++i) {
        at = t.find(from, at + 1);
    }
    REQUIRE_MESSAGE(at != std::string::npos, std::string(from) << " not in renaming.toml");
    t.replace(at, from.size(), to);
    return f;
}

wd::LoadError one_error(const Load& l) {
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

bool has(std::string_view s, std::string_view part) { return s.find(part) != std::string_view::npos; }

const wd::Record& renaming_record(const wd::ContentSet& c) {
    const wd::KindContent* h = c.find_kind("history");
    REQUIRE(h != nullptr);
    for (const wd::Definition& d : h->definitions) {
        if (d.id == "renaming") {
            return d.record;
        }
    }
    FAIL("no history.renaming");
    return h->definitions.front().record;
}

std::vector<std::string> strings(const wd::Value& v) {
    std::vector<std::string> out;
    for (const wd::Value& x : v.as_array()) {
        out.push_back(x.as_string());
    }
    return out;
}

struct Rule {
    std::string_view key;
    std::vector<std::string> kinds, values;
    std::int64_t min_magnitude, window_years, min_salience;
    bool capital;
    std::int64_t chance_permille, weight;
    std::string_view name_role, namer_role;
    std::vector<std::string> forms;
};

// P5-12 4.1, column by column.
const std::vector<Rule>& shipped_rules() {
    static const std::vector<Rule> v = {
        {"conquest", {"civ_conquered"}, {}, 1, 1, 0, false, 1000, 100, "civilization", "conqueror",
         {"{name}'s Grave", "The Grave of {name}", "The End of {name}", "{name}'s Barrow"}},
        {"capital_sack", {"sacked"}, {}, 1, 1, 90, false, 700, 90, "settlement", "attacker",
         {"{stem}'s Fall", "The Ashes of {name}", "The Ruin of {name}", "The Fallen {noun}"}},   // P5-02 21.2
        {"great_sack", {"sacked"}, {"dead"}, 1500, 1, 0, false, 500, 80, "settlement", "attacker",
         {"{stem}'s Pyre", "The Ashes of {name}", "The Burnt {noun}", "The Cinder {noun}"}},
        {"slaughter", {"battle", "battle_at_pass"}, {"winner_dead", "loser_dead"}, 5000, 100, 0, false, 600, 70, "loser",
         "winner", {"The Red {noun}", "The Bloody {noun}", "The Rout of {name}", "The {noun} of the Slain",
                    "The Weeping {noun}", "The Grim {noun}"}},
        {"plague", {"plague_outbreak"}, {"total"}, 3000, 1, 0, false, 400, 60, "settlement", "civilization",
         {"The Emptied {noun}", "The Fever {noun}", "The Silent {noun}", "{stem}'s Grave", "The Pale {noun}",
          "The Hollow {noun}"}},
        {"heartland", {"became_city"}, {}, 1, 1, 0, true, 300, 50, "civilization", "civilization",
         {"{name}land", "The Heart of {name}", "The {name} Reach", "{name}'s Hearth"}},
        // P5-02 9.3: rule 7
        {"ravage", {"settlement_overrun"}, {}, 1, 1, 0, false, 600, 75, "monster", "civilization",
         {"The Haunt of {name}", "{name}'s Shadow", "The Forsaken {noun}", "The Hollow of {name}"}},
    };
    return v;
}

const std::vector<std::string>& head_fields() {
    static const std::vector<std::string> v = {"region_max", "region_gap_years", "world_max_permille",
                                               "century_max_permille", "century_years", "rules"};
    return v;
}
const std::vector<std::string>& rule_fields() {
    static const std::vector<std::string> v = {"key", "kinds", "values", "min_magnitude", "window_years",
                                               "min_salience", "capital", "chance_permille", "weight", "name_role",
                                               "namer_role", "forms"};
    return v;
}

const std::string& check_prefix() {
    static const std::string s = "failed for 'history.renaming': ";
    return s;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-12 4, P3-01 r16 5.4: history.renaming's fields in order and the shipped values") {
    const Load l = load(history_data::shipped_files());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& r = renaming_record(*l.content);
    REQUIRE(r.size() == head_fields().size());
    for (std::size_t i = 0; i < r.size(); ++i) {
        CHECK(r.name_at(i) == head_fields()[i]);
    }
    CHECK(r.get_int("region_max") == 2);
    CHECK(r.get_int("region_gap_years") == 300);
    CHECK(r.get_int("world_max_permille") == 150);
    CHECK(r.get_int("century_max_permille") == 50);
    CHECK(r.get_int("century_years") == 100);
    const std::vector<wd::Value>& rules = r.value_at(5).as_array();
    REQUIRE(rules.size() == shipped_rules().size());
    for (std::size_t i = 0; i < rules.size(); ++i) {
        const Rule& w = shipped_rules()[i];
        CAPTURE(std::string_view(w.key));
        const wd::Record& x = rules[i].as_record();
        REQUIRE(x.size() == rule_fields().size());
        for (std::size_t f = 0; f < x.size(); ++f) {
            CHECK(x.name_at(f) == rule_fields()[f]);
        }
        CHECK(x.get_string("key") == w.key);
        CHECK(strings(x.value_at(1)) == w.kinds);
        CHECK(strings(x.value_at(2)) == w.values);
        CHECK(x.get_int("min_magnitude") == w.min_magnitude);
        CHECK(x.get_int("window_years") == w.window_years);
        CHECK(x.get_int("min_salience") == w.min_salience);
        CHECK(x.get_bool("capital") == w.capital);
        CHECK(x.get_int("chance_permille") == w.chance_permille);
        CHECK(x.get_int("weight") == w.weight);
        CHECK(x.get_string("name_role") == w.name_role);
        CHECK(x.get_string("namer_role") == w.namer_role);
        CHECK(strings(x.value_at(11)) == w.forms);
    }
    // the shipped content (game/data) and both fixture roots hold the same record (P3-01 2)
    for (const char* root : {"game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
        CAPTURE(root);
        const Load d = load_disk(source_root() / root);
        REQUIRE_MESSAGE(d.ok(), dump(d));
        CHECK(renaming_record(*d.content) == r);
    }
}

TEST_CASE("P5-12 4: every scalar field one step outside its range is a range error") {
    struct R {
        const char* field;
        std::int64_t lo, hi, shipped;
    };
    for (const R& rg : {R{"region_max", 0, 16, 2}, R{"region_gap_years", 1, 100000, 300},
                        R{"world_max_permille", 0, 1000, 150}, R{"century_max_permille", 0, 1000, 50},
                        R{"century_years", 1, 100000, 100}}) {
        CAPTURE(rg.field);
        const std::string line = std::string(rg.field) + " = " + std::to_string(rg.shipped);
        for (const std::int64_t v : {rg.lo - 1, rg.hi + 1}) {
            const wd::LoadError e = one_error(load(with_renaming_edit(line, std::string(rg.field) + " = " +
                                                                                  std::to_string(v))));
            CHECK(e.path == "history/renaming.toml");
            CHECK(has(e.message, "history.renaming." + std::string(rg.field)));
        }
        CHECK(load(with_renaming_edit(line, std::string(rg.field) + " = " + std::to_string(rg.lo))).ok());
        CHECK(load(with_renaming_edit(line, std::string(rg.field) + " = " + std::to_string(rg.hi))).ok());
    }
    // region_max 0 turns the subsystem off: valid data
    CHECK(load(with_renaming_edit("region_max = 2", "region_max = 0")).ok());
}

TEST_CASE("P5-12 4: the rule fields' ranges (the first rule)") {
    struct R {
        const char* line;
        const char* bad_low;
        const char* bad_high;
    };
    for (const R& c : {R{"min_magnitude = 1\nwindow_years = 1\nmin_salience = 0\ncapital = false\nchance_permille = 1000",
                         "min_magnitude = 0\nwindow_years = 1\nmin_salience = 0\ncapital = false\nchance_permille = 1000",
                         "min_magnitude = 1000000001\nwindow_years = 1\nmin_salience = 0\ncapital = false\n"
                         "chance_permille = 1000"},
                       R{"chance_permille = 1000\nweight = 100", "chance_permille = -1\nweight = 100",
                         "chance_permille = 1001\nweight = 100"},
                       R{"weight = 100", "weight = 0", "weight = 1001"},
                       R{"min_salience = 0\ncapital = false\nchance_permille = 1000",
                         "min_salience = -1\ncapital = false\nchance_permille = 1000",
                         "min_salience = 101\ncapital = false\nchance_permille = 1000"}}) {
        CAPTURE(std::string_view(c.bad_low));
        CHECK_FALSE(load(with_renaming_edit(c.line, c.bad_low)).ok());
        CHECK_FALSE(load(with_renaming_edit(c.line, c.bad_high)).ok());
    }
    CHECK_FALSE(load(with_renaming_edit("window_years = 100", "window_years = 0")).ok());
    CHECK_FALSE(load(with_renaming_edit("window_years = 100", "window_years = 100001")).ok());
    CHECK(load(with_renaming_edit("window_years = 100", "window_years = 100000")).ok());
}

TEST_CASE("P5-12 4: every field is required (the head and a rule)") {
    for (const char* line : {"region_max = 2\n", "region_gap_years = 300\n", "world_max_permille = 150\n",
                             "century_max_permille = 50\n", "century_years = 100\n"}) {
        CAPTURE(line);
        const wd::LoadError e = one_error(load(with_renaming_edit(line, "")));
        CHECK(has(e.message, "missing required field 'history.renaming."));
    }
    for (const char* line : {"key = \"conquest\"\n", "kinds = [\"civ_conquered\"]\n", "min_magnitude = 1\n",
                             "window_years = 1\n", "min_salience = 0\n", "capital = false\n",
                             "chance_permille = 1000\n", "weight = 100\n", "name_role = \"civilization\"\n",
                             "namer_role = \"conqueror\"\n"}) {
        CAPTURE(line);
        CHECK_FALSE(load(with_renaming_edit(line, "")).ok());
    }
    CHECK_FALSE(load(with_renaming_edit("century_years = 100\n", "century_years = 100\nyear_max = 1\n")).ok());
}

TEST_CASE("P5-12 4: grammar rename_form") {
    const std::string first = "forms = [\"{name}'s Grave\", ";
    for (const char* bad : {"{name}{noun}", "{name} ", " {name}", "The  {noun}", "{name}'s {name}", "The {other}",
                            "The-{noun}", "The {noun}}", "{noun}", "The 1st {noun}",
                            "abcdefghijabcdefghijabcdefghijabcdefghijabcdefghij"}) {
        CAPTURE(bad);
        const wd::LoadError e = one_error(load(with_renaming_edit(first, "forms = [\"" + std::string(bad) + "\", ")));
        CHECK(e.path == "history/renaming.toml");
        CHECK(has(e.message, "a region name form (letters, spaces, ' and the slots {name} and {noun})"));
    }
    for (const char* good : {"The Ashes", "{name} and {noun}", "{noun}'s End of {name}"}) {
        CAPTURE(good);
        CHECK(load(with_renaming_edit(first, "forms = [\"" + std::string(good) + "\", ")).ok());
    }
    CHECK_FALSE(load(with_renaming_edit("key = \"conquest\"", "key = \"Conquest\"")).ok());   // [a-z][a-z0-9_]*
}

TEST_CASE("P5-12 4: renaming.toml absent: the fixed id is missing") {
    Files f = history_data::shipped_files();
    f.erase("history/renaming.toml");
    const Load l = load(f);
    REQUIRE_FALSE(l.ok());
    bool named = false;
    for (const wd::LoadError& e : l.errors) {
        named = named || has(e.message, "renaming");
    }
    CHECK_MESSAGE(named, dump(l));
}

TEST_CASE("P3-01 r16 5.4: the four named checks of history.renaming, with their messages") {
    // renaming_rule_keys: the fourth rule's key repeats the first's
    {
        const wd::LoadError e =
            one_error(load(with_renaming_edit("key = \"slaughter\"", "key = \"conquest\"")));
        CHECK(e.message == "check 'renaming_rule_keys' " + check_prefix() + "rules[3] key 'conquest' repeats rules[0]");
    }
    // renaming_rule_weights: plague's weight 60 -> 90 (capital_sack's)
    {
        const wd::LoadError e = one_error(load(with_renaming_edit("weight = 60", "weight = 90")));
        CHECK(e.message == "check 'renaming_rule_weights' " + check_prefix() + "rules[4] weight 90 repeats rules[1]");
    }
    // renaming_rule_lists: a kind twice, a value twice, a form twice
    {
        const wd::LoadError e = one_error(
            load(with_renaming_edit("kinds = [\"battle\", \"battle_at_pass\"]", "kinds = [\"battle\", \"battle\"]")));
        CHECK(e.message == "check 'renaming_rule_lists' " + check_prefix() + "rules[3].kinds[1] 'battle' repeats kinds[0]");
    }
    {
        const wd::LoadError e = one_error(load(with_renaming_edit(
            "values = [\"winner_dead\", \"loser_dead\"]", "values = [\"winner_dead\", \"winner_dead\"]")));
        CHECK(e.message ==
              "check 'renaming_rule_lists' " + check_prefix() + "rules[3].values[1] 'winner_dead' repeats values[0]");
    }
    {
        const wd::LoadError e = one_error(load(with_renaming_edit("\"The Cinder {noun}\"]", "\"{stem}'s Pyre\"]")));
        CHECK(e.message ==
              "check 'renaming_rule_lists' " + check_prefix() + "rules[2].forms[3] '{stem}'s Pyre' repeats forms[0]");
    }
    // renaming_forms_slots: heartland with name_role "" uses {name}
    {
        const wd::LoadError e = one_error(load(
            with_renaming_edit("name_role = \"civilization\"\nnamer_role = \"civilization\"",
                               "name_role = \"\"\nnamer_role = \"civilization\"")));
        CHECK(e.message ==
              "check 'renaming_forms_slots' " + check_prefix() + "rules[5].forms[0] uses {name} but name_role is empty");
    }
    // a rule without {name} may have name_role "" (no slot used)
    CHECK(load(with_renaming_edit("forms = [\"The Red {noun}\", \"The Bloody {noun}\", \"The Rout of {name}\", "
                                  "\"The {noun} of the Slain\", \"The Weeping {noun}\", \"The Grim {noun}\"]\n",
                                  "forms = [\"The Red {noun}\", \"The Bloody {noun}\"]\n"))
              .ok());
    // ... and name_role "" with forms that use no {name} (slaughter: name_role is the field two lines above forms)
    CHECK(load(with_renaming_edit("name_role = \"loser\"\nnamer_role = \"winner\"\nforms = [\"The Red {noun}\", "
                                  "\"The Bloody {noun}\", \"The Rout of {name}\", \"The {noun} of the Slain\", "
                                  "\"The Weeping {noun}\", \"The Grim {noun}\"]\n",
                                  "name_role = \"\"\nnamer_role = \"winner\"\nforms = [\"The Red {noun}\"]\n"))
              .ok());
    // zero rules: valid
    {
        Files f = history_data::shipped_files();
        std::string& t = f["history/renaming.toml"];
        t = t.substr(0, t.find("[[history.renaming.rules]]"));
        t += "rules = []\n";
        CHECK(load(f).ok());
    }
}

TEST_CASE("P3-02 r19 16.2: human forbidden gains ass (appended); P3-04 r21 23.2: the report entry of kind 43") {
    const Load l = load(history_data::shipped_files());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::KindContent* cu = l.content->find_kind("culture");
    REQUIRE(cu != nullptr);
    bool seen = false;
    for (const wd::Definition& d : cu->definitions) {
        if (d.id == "human") {
            const std::vector<std::string> f = strings(d.record.get_record("naming").get("forbidden"));
            CHECK(f == std::vector<std::string>{"eae", "aea", "ckc", "ldl", "ntd", "rdd", "ckb", "ckm", "ass"});
            seen = true;
        }
    }
    CHECK(seen);
    const wd::KindContent* h = l.content->find_kind("history");
    REQUIRE(h != nullptr);
    for (const wd::Definition& d : h->definitions) {
        if (d.id != "report") continue;
        const std::vector<wd::Value>& ev = d.record.get("events").as_array();
        REQUIRE(ev.size() == 55U);   // P5-02 10.2
        const wd::Record& last = ev[42].as_record();   // kind 43; kinds 44 to 55 follow (P5-02 10.2)
        CHECK(last.get_string("kind") == "region_renamed");
        CHECK(last.get_int("salience") == 60);
        CHECK(last.get_string("template") ==   // P5-02 21.2: a realm's seat, the ravage
              "{@causes:civ_conquered=the fall of a realm,sacked=the sack,battle=the slaughter,battle_at_pass=the "
              "slaughter,plague_outbreak=the plague,became_city=the rise of a realm's seat,settlement_overrun=the "
              "ravage|After |, }{@region_former} became known as {@region}.");
    }
}

}  // TEST_SUITE
