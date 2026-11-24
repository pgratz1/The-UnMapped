// Spec P5-02 (docs/specs/p5-02-beasts-monsters.md r0) 9.1, test 19.1 item 9: the name kinds monster and site
// after settlement with their streams; monster names by kind (the naming culture's data, the kind's name_forms)
// and outpost names by culture (site_forms), in one registry, against the reference's lines `mname` and `sname`
// of worldgen/tests/golden/p3-02-names.txt (p3_names_reference.py r23_golden_lines: seeds 0 and 42, one
// registry per seed, the six kinds in canonical order with 30 monster ids each, ids 1, 2, ... across the kinds,
// then the six cultures' outposts likewise). Lair names (9.2) need the step's year (stream
// hist.monsters.lair_name) and are covered through the history goldens (p5-02-monsters.txt `si` lines).
//
// Written from the spec before the implementation; the guesses are in p5_02_monsters_api.hpp.

#include <doctest/doctest.h>

#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "../p3_01_history_data.hpp"
#include "../p3_01_history_support.hpp"
#include "p5_02_monsters_api.hpp"
#include "worldgen/history/names.hpp"

namespace {

namespace wh = worldgen::history;
namespace wd = worldgen::data;

struct Line {
    std::uint64_t seed;
    std::string what, key;   // what: mname or sname; key: monster kind or culture
    std::uint64_t entity;
    std::string attempt, name;
};

std::vector<Line> golden_lines() {
    std::vector<Line> out;
    std::ifstream in(UNMAPPED_P3_02_GOLDEN_PATH, std::ios::binary);
    REQUIRE(in.good());
    std::string l;
    while (std::getline(in, l)) {
        if (l.rfind("mname ", 0) != 0 && l.rfind("sname ", 0) != 0) continue;
        std::vector<std::string> w;
        std::size_t at = 0;
        for (int k = 0; k < 5; ++k) {
            const std::size_t sp = l.find(' ', at);
            w.push_back(l.substr(at, sp - at));
            at = sp + 1;
        }
        out.push_back({std::stoull(w[1]), w[0], w[2], std::stoull(w[3]), w[4], l.substr(at)});
    }
    return out;
}

const wd::Definition& def(const wd::ContentSet& c, std::string_view kind, std::string_view id) {
    for (const wd::KindContent& k : c.kinds) {
        if (k.kind != kind) continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == id) return d;
        }
    }
    FAIL("no " << kind << "." << id);
    return c.kinds.front().definitions.front();
}

std::uint64_t culture_number(std::string_view id) {
    const std::vector<std::string_view> order = {"dwarven", "elven", "faerie", "goblin", "human", "orcish"};
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (order[i] == id) return i + 1;
    }
    FAIL("no culture " << id);
    return 0;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-02 9.1: kinds monster and site after settlement, with the streams hist.names.monster and .site") {
    CHECK(static_cast<int>(wh::NameKind::monster) == 3);
    CHECK(static_cast<int>(wh::NameKind::site) == 4);
    CHECK(wh::name_kind_text(wh::NameKind::monster) == "monster");
    CHECK(wh::name_kind_text(wh::NameKind::site) == "site");
    CHECK(wh::name_stream(wh::NameKind::monster) == "hist.names.monster");
    CHECK(wh::name_stream(wh::NameKind::site) == "hist.names.site");
}

TEST_CASE("P5-02 9.1: monster and outpost names in one registry equal the reference's (seeds 0 and 42)") {
    const history_test::Load l = history_test::load(history_data::shipped_files());
    REQUIRE(l.ok());
    const wd::ContentSet& c = *l.content;
    const wh::NameFilter filter = wh::name_filter_from(c);
    const std::vector<Line> lines = golden_lines();
    REQUIRE(lines.size() == 2U * (6U * 30U + 6U * 30U));
    std::map<std::uint64_t, wh::NameRegistry> reg;
    for (const Line& g : lines) {
        CAPTURE(g.seed);
        CAPTURE(g.key);
        CAPTURE(g.entity);
        wh::NameChoice got;
        if (g.what == "mname") {
            const wd::Record& m = def(c, "monster", g.key).record;
            const std::string culture = m.get_string("naming_culture");
            // The golden pins P3-02 4.3's attempt with the 0.23.0 forms of each kind (p3_names_reference.py's
            // MONSTERS_KINDS_023); P5-03 12.1 and 12.2 moved the shipped forms and the epithet rule, which the P5-03
            // tests cover. So the forms are the 0.23.0 ones, transcribed.
            const std::map<std::string, std::vector<std::string>> forms_023 = {
                {"dragon", {"{root} the Black", "{root} the Pale", "{root}"}},
                {"giant", {"{root} Stonehand", "{root}"}},
                {"ogre", {"{root} the Glutton", "{root}"}},
                {"troll", {"Old {root}", "{root}"}},
                {"undead", {"The Host of {root}", "The Restless of {root}"}},
                {"wyvern", {"The {root} Brood", "The Brood of {root}"}}};
            const std::vector<std::string> forms = forms_023.at(g.key);
            const wh::CultureNaming naming = p502api::monster_naming(
                wh::culture_naming_from(def(c, "culture", culture).record.get_record("naming")), forms);
            CHECK(wh::name_forms(naming, wh::NameKind::monster) == forms);
            got = wh::choose_name(naming, filter, culture, wh::NameKind::monster, g.seed, culture_number(culture),
                                  g.entity, reg[g.seed]);
        } else {
            const wh::CultureNaming naming =
                wh::culture_naming_from(def(c, "culture", g.key).record.get_record("naming"));
            got = wh::choose_name(naming, filter, g.key, wh::NameKind::site, g.seed, culture_number(g.key), g.entity,
                                  reg[g.seed]);
        }
        CHECK(got.name == g.name);
        CHECK((got.fallback ? std::string("F") : std::to_string(got.attempt)) == g.attempt);
    }
}

}  // TEST_SUITE
