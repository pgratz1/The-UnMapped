// Spec P5-02 (docs/specs/p5-02-beasts-monsters.md r0) section 4 and 19.1 item 1 (generator 0.23.0): the new
// required kinds beast and monster, the fixed id history.monsters, the culture field site_forms and the peoples
// fields ghost_*, through the loader: fields in order with the shipped values (the listing of
// p3_01_history_data.hpp, copied from game/data, equal to both fixture roots), every scalar field one step
// outside its range, every field required, the grammars display_name, beast_plural, monster_noun and name_form,
// the array bounds, and each named check with its message where the spec gives one. Also the renaming data of
// 21.2 and 9.3 (rule 7, {stem}) and the report entries of kinds 44 to 55 (10.2).
//
// Written from the spec before the implementation (test-writer rule): it needs no new library name and fails
// until the 0.23.0 kinds are registered. The messages the spec does not give (monster_culture,
// monster_toll_ordered, monster_overruns_unique, monsters_bold_ordered) are checked by check name and
// definition only (ambiguity A3 of the test writer's report).

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

Files with_edit(const std::string& path, std::string_view from, std::string_view to) {
    Files f = history_data::shipped_files();
    const auto it = f.find(path);
    REQUIRE_MESSAGE(it != f.end(), "no file " << path);
    const std::size_t at = it->second.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, std::string(from) << " not in " << path);
    it->second.replace(at, from.size(), to);
    return f;
}

wd::LoadError one_error(const Load& l) {
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

bool has(std::string_view s, std::string_view part) { return s.find(part) != std::string_view::npos; }

const wd::Definition& definition(const wd::ContentSet& c, std::string_view kind, std::string_view id) {
    const wd::KindContent* k = c.find_kind(kind);
    REQUIRE_MESSAGE(k != nullptr, "no kind " << kind);
    for (const wd::Definition& d : k->definitions) {
        if (d.id == id) return d;
    }
    FAIL("no definition " << kind << "." << id);
    return k->definitions.front();
}

std::vector<std::string> strings(const wd::Value& v) {
    std::vector<std::string> out;
    for (const wd::Value& x : v.as_array()) out.push_back(x.as_string());
    return out;
}

struct Weight {
    std::string_view biome;
    std::int64_t weight;
};

std::vector<Weight> weights(const wd::Value& v) {
    std::vector<Weight> out;
    for (const wd::Value& x : v.as_array()) {
        const wd::Record& r = x.as_record();
        REQUIRE(r.size() == 2U);
        CHECK(r.name_at(0) == "biome");
        CHECK(r.name_at(1) == "weight");
        out.push_back({r.get_string("biome"), r.get_int("weight")});
    }
    return out;
}

bool operator==(const Weight& a, const Weight& b) { return a.biome == b.biome && a.weight == b.weight; }

// --- 4.1: the shipped beasts, column by column ---------------------------------------------------------------

struct Beast {
    std::string_view id, name, plural;
    std::vector<Weight> habitat;
    std::int64_t density, growth, menace, food;
};

const std::vector<Beast>& beasts() {
    static const std::vector<Beast> v = {
        {"bear", "Bear", "bears",
         {{"temperate_forest", 900}, {"wet_woodland", 700}, {"highland_moor", 400}, {"alpine", 500}, {"heath", 200}},
         5, 60, 30, 30},
        {"boar", "Boar", "boar",
         {{"temperate_forest", 800}, {"wet_woodland", 900}, {"grassland", 300}, {"heath", 300}, {"marsh", 400},
          {"fen", 300}},
         40, 250, 3, 100},
        {"deer", "Deer", "deer",
         {{"temperate_forest", 900}, {"wet_woodland", 600}, {"grassland", 800}, {"heath", 600}, {"steppe", 500},
          {"highland_moor", 500}, {"alpine", 200}, {"marsh", 200}, {"fen", 200}},
         100, 250, 0, 60},
        {"giant_spider", "Giant spider", "giant spiders",
         {{"wet_woodland", 800}, {"temperate_forest", 400}, {"marsh", 600}, {"fen", 500}, {"blighted_waste", 900}}, 10,
         200, 40, 0},
        {"wolf", "Wolf", "wolves",
         {{"temperate_forest", 700}, {"wet_woodland", 500}, {"grassland", 400}, {"heath", 600}, {"steppe", 700},
          {"highland_moor", 800}, {"alpine", 400}, {"marsh", 100}, {"fen", 100}, {"blighted_waste", 200}},
         20, 150, 10, 0},
    };
    return v;
}

const std::vector<std::string_view>& beast_fields() {
    static const std::vector<std::string_view> v = {"name", "plural", "habitat", "density", "growth_permille",
                                                    "menace", "food"};
    return v;
}

// --- 4.2: the shipped monster kinds, the two tables joined -------------------------------------------------

struct Monster {
    std::string_view id, name, noun;
    bool grouped;
    std::string_view wake;
    std::vector<Weight> biomes;
    std::int64_t wake_rate;
    std::string_view culture;
    std::vector<std::string> name_forms, lair_forms;
    std::int64_t might, range, raid, toll_min, toll_max, kill_max;
    std::vector<std::string> overruns;
    std::int64_t overrun, active, rewake, lifespan, max_alive;
    bool flare;
    std::int64_t hoard;
};

const std::vector<Monster>& monsters() {
    static const std::vector<Monster> v = {
        {"dragon", "Dragon", "a dragon of the old world", false, "wild",
         {{"alpine", 800}, {"highland_moor", 400}, {"blighted_waste", 600}}, 6, "elven",
         {"{root} the Black", "{root} the Pale", "{root}"}, {"The Lair of {root}", "{root}'s Deep"}, 6000, 2, 30, 50,
         200, 600, {"hamlet", "village", "town"}, 150, 40, 2000, 0, 1, true, 40},
        {"giant", "Giant", "a giant of the mountains", false, "wild", {{"alpine", 900}, {"highland_moor", 500}}, 14,
         "dwarven", {"{root} Stonehand", "{root}"}, {"{root}'s Seat", "The Hall of {root}"}, 1200, 1, 40, 30, 100,
         150, {"hamlet", "village"}, 60, 150, 3000, 2500, 2, true, 8},
        {"ogre", "Ogre", "an ogre", false, "wild",
         {{"grassland", 400}, {"heath", 600}, {"steppe", 700}, {"highland_moor", 300}}, 10, "orcish",
         {"{root} the Glutton", "{root}"}, {"{root}'s Larder", "The Pit of {root}"}, 200, 1, 100, 20, 60, 40,
         {"hamlet"}, 80, 80, 8000, 300, 3, false, 3},
        {"troll", "Troll", "a troll of the hills", false, "wild",
         {{"highland_moor", 800}, {"alpine", 600}, {"temperate_forest", 400}, {"heath", 300}}, 17, "goblin",
         {"Old {root}", "{root}"}, {"{root}'s Hollow", "The Den of {root}"}, 300, 1, 80, 20, 80, 60,
         {"hamlet", "village"}, 100, 120, 5000, 800, 3, false, 5},
        {"undead", "Undead", "a host of the restless dead", true, "dead", {}, 9000, "human",
         {"The Host of {root}", "The Restless of {root}"}, {"The Barrows of {root}", "{root}'s Howe"}, 1500, 1, 60, 30,
         120, 200, {"hamlet", "village"}, 120, 200, 3000, 1000, 2, false, 5},
        {"wyvern", "Wyvern", "a brood of wyverns", true, "wild", {{"alpine", 700}, {"heath", 500}, {"highland_moor", 600}},
         17, "goblin", {"The {root} Brood", "The Brood of {root}"}, {"The Eyrie of {root}", "{root}'s Crag"}, 600, 2,
         80, 20, 80, 50, {"hamlet"}, 50, 60, 10000, 300, 2, false, 2},
    };
    return v;
}

const std::vector<std::string_view>& monster_fields() {
    static const std::vector<std::string_view> v = {
        "name", "noun", "grouped", "wake", "biomes", "wake_rate", "naming_culture", "name_forms", "lair_forms",
        "might", "range_hops", "raid_permille", "toll_min_permille", "toll_max_permille", "kill_max", "overruns",
        "overrun_permille", "active_years", "rewake_ppm", "lifespan_years", "max_alive", "flare", "hoard_permille"};
    return v;
}

// --- 4.3: history.monsters ---------------------------------------------------------------------------------

struct Field {
    std::string_view name;
    std::int64_t lo, hi, shipped;
};

const std::vector<Field>& tuning_fields() {
    static const std::vector<Field> v = {
        {"living_permille", 0, 1000, 100},       {"undead_min_dead", 1, 1000000000, 4000},
        {"undead_window_years", 1, 100000, 100}, {"hunt_hops", 0, 8, 2},
        {"hunt_levy_permille", 0, 1000, 50},     {"hunt_permille", 0, 1000, 150},
        {"hunt_memory_years", 1, 100000, 30},    {"slay_max_permille", 0, 1000, 900},
        {"drive_permille", 0, 1000, 300},        {"drive_years", 0, 100000, 100},
        {"displace_per_kpeople", 0, 1000, 100},  {"displace_max_permille", 0, 1000, 900},
        {"beast_decline_permille", 1, 1000, 200}, {"people_floor", 1, 100000, 100},
        {"drag_k", 0, 10000, 100},               {"drag_max_permille", 0, 500, 200},
        {"feed_k", 0, 10000, 2},                 {"feed_max_permille", 0, 500, 100},
        {"bold_on_permille", 1, 1000000, 1500},  {"bold_off_permille", 0, 1000000, 800},
        {"bold_permille", 0, 1000, 100},         {"bold_toll_permille", 0, 1000, 10},
        {"outpost_permille", 0, 1000, 30},       {"outpost_max", 0, 16, 2},
        {"outpost_quiet_years", 1, 100000, 200},
    };
    return v;
}

std::string line_of(std::string_view name, std::int64_t v) { return std::string(name) + " = " + std::to_string(v); }

std::string beast_path(std::string_view id) { return "beasts/" + std::string(id) + ".toml"; }
std::string monster_path(std::string_view id) { return "monsters/" + std::string(id) + ".toml"; }

// A failure of named check `check` at definition `qualified` (the loader's form, P0-03 amendment 8).
void check_failure(const wd::LoadError& e, std::string_view path, std::string_view check, std::string_view qualified,
                   std::string_view message = {}) {
    CAPTURE(e.to_string());
    CHECK(e.path == path);
    const std::string head = "check '" + std::string(check) + "' failed for '" + std::string(qualified) + "': ";
    if (message.empty()) {
        CHECK(e.message.rfind(head, 0) == 0U);
    } else {
        CHECK(e.message == head + std::string(message));
    }
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-02 4.1: kind beast, the five shipped species in canonical order, fields in order, the values") {
    const Load l = load(history_data::shipped_files());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::KindContent* k = l.content->find_kind("beast");
    REQUIRE(k != nullptr);
    REQUIRE(k->definitions.size() == beasts().size());
    for (std::size_t i = 0; i < beasts().size(); ++i) {
        const Beast& b = beasts()[i];
        CAPTURE(b.id);
        const wd::Definition& d = k->definitions[i];
        CHECK(d.id == b.id);   // P0-03 canonical order: beast.id 1 bear ... 5 wolf
        REQUIRE(d.record.size() == beast_fields().size());
        for (std::size_t f = 0; f < d.record.size(); ++f) CHECK(d.record.name_at(f) == beast_fields()[f]);
        CHECK(d.record.get_string("name") == b.name);
        CHECK(d.record.get_string("plural") == b.plural);
        CHECK(weights(d.record.value_at(2)) == b.habitat);
        CHECK(d.record.get_int("density") == b.density);
        CHECK(d.record.get_int("growth_permille") == b.growth);
        CHECK(d.record.get_int("menace") == b.menace);
        CHECK(d.record.get_int("food") == b.food);
    }
}

TEST_CASE("P5-02 4.2: kind monster, the six shipped kinds in canonical order, fields in order, the values") {
    const Load l = load(history_data::shipped_files());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::KindContent* k = l.content->find_kind("monster");
    REQUIRE(k != nullptr);
    REQUIRE(k->definitions.size() == monsters().size());
    for (std::size_t i = 0; i < monsters().size(); ++i) {
        const Monster& m = monsters()[i];
        CAPTURE(m.id);
        const wd::Definition& d = k->definitions[i];
        CHECK(d.id == m.id);   // monster_kind.id: dragon 1 ... wyvern 6
        REQUIRE(d.record.size() == monster_fields().size());
        for (std::size_t f = 0; f < d.record.size(); ++f) CHECK(d.record.name_at(f) == monster_fields()[f]);
        const wd::Record& r = d.record;
        CHECK(r.get_string("name") == m.name);
        CHECK(r.get_string("noun") == m.noun);
        CHECK(r.get_bool("grouped") == m.grouped);
        CHECK(r.get_string("wake") == m.wake);
        CHECK(weights(r.value_at(4)) == m.biomes);
        CHECK(r.get_int("wake_rate") == m.wake_rate);
        CHECK(r.get_string("naming_culture") == m.culture);
        CHECK(strings(r.value_at(7)) == m.name_forms);
        CHECK(strings(r.value_at(8)) == m.lair_forms);
        CHECK(r.get_int("might") == m.might);
        CHECK(r.get_int("range_hops") == m.range);
        CHECK(r.get_int("raid_permille") == m.raid);
        CHECK(r.get_int("toll_min_permille") == m.toll_min);
        CHECK(r.get_int("toll_max_permille") == m.toll_max);
        CHECK(r.get_int("kill_max") == m.kill_max);
        CHECK(strings(r.value_at(15)) == m.overruns);
        CHECK(r.get_int("overrun_permille") == m.overrun);
        CHECK(r.get_int("active_years") == m.active);
        CHECK(r.get_int("rewake_ppm") == m.rewake);
        CHECK(r.get_int("lifespan_years") == m.lifespan);
        CHECK(r.get_int("max_alive") == m.max_alive);
        CHECK(r.get_bool("flare") == m.flare);
        CHECK(r.get_int("hoard_permille") == m.hoard);
    }
}

TEST_CASE("P5-02 4.3, 4.4, 4.5: history.monsters, site_forms and ghost_* shipped; three roots identical") {
    const Load l = load(history_data::shipped_files());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& t = definition(*l.content, "history", "monsters").record;
    REQUIRE(t.size() == tuning_fields().size() + 1U);
    CHECK(t.name_at(0) == "pressure");
    const wd::Record& p = t.value_at(0).as_record();
    REQUIRE(p.size() == 3U);
    CHECK(p.name_at(0) == "low");
    CHECK(p.name_at(1) == "normal");
    CHECK(p.name_at(2) == "high");
    CHECK(p.get_int("low") == 500);
    CHECK(p.get_int("normal") == 1000);
    CHECK(p.get_int("high") == 1600);
    for (std::size_t i = 0; i < tuning_fields().size(); ++i) {
        CAPTURE(tuning_fields()[i].name);
        CHECK(t.name_at(i + 1) == tuning_fields()[i].name);
        CHECK(t.get_int(tuning_fields()[i].name) == tuning_fields()[i].shipped);
    }
    // 4.4: site_forms after settlement_forms, in every culture
    const std::vector<std::pair<std::string_view, std::vector<std::string>>> site = {
        {"dwarven", {"{root} Watch", "The Tower of {root}"}}, {"elven", {"{root} Watch", "The Spire of {root}"}},
        {"faerie", {"{root} Watch", "The Thorn of {root}"}},  {"goblin", {"{root} Lookout", "{root} Stockade"}},
        {"human", {"{root} Watch", "{root} Tower"}},           {"orcish", {"{root} Stockade", "The Watch of {root}"}}};
    for (const auto& [cid, forms] : site) {
        CAPTURE(cid);
        const wd::Record& n = definition(*l.content, "culture", cid).record.get_record("naming");
        REQUIRE(n.size() == 12U);
        CHECK(n.name_at(10) == "settlement_forms");
        CHECK(n.name_at(11) == "site_forms");
        CHECK(strings(n.value_at(11)) == forms);
    }
    // 4.5: ghost_permille 200, ghost_max_population 300 appended after crisis_small_salience
    const wd::Record& pe = definition(*l.content, "history", "peoples").record;
    REQUIRE(pe.size() >= 3U);
    CHECK(pe.name_at(pe.size() - 3) == "crisis_small_salience");
    CHECK(pe.name_at(pe.size() - 2) == "ghost_permille");
    CHECK(pe.name_at(pe.size() - 1) == "ghost_max_population");
    CHECK(pe.get_int("ghost_permille") == 200);
    CHECK(pe.get_int("ghost_max_population") == 300);
    for (const char* root : {"game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
        CAPTURE(root);
        const Load d = load_disk(source_root() / root);
        REQUIRE_MESSAGE(d.ok(), dump(d));
        for (const char* kind : {"beast", "monster", "culture", "history"}) {
            CAPTURE(kind);
            const wd::KindContent* a = d.content->find_kind(kind);
            const wd::KindContent* b = l.content->find_kind(kind);
            REQUIRE(a != nullptr);
            REQUIRE(b != nullptr);
            CHECK(*a == *b);
        }
    }
}

TEST_CASE("P5-02 4.1-4.3: the kinds and the fixed id are required") {
    Files f = history_data::shipped_files();
    f.erase("history/monsters.toml");
    {
        const Load l = load(f);
        REQUIRE_FALSE(l.ok());
        CHECK(has(dump(l), "history.monsters"));
    }
    Files g = history_data::shipped_files();
    for (const Beast& b : beasts()) g.erase(beast_path(b.id));
    CHECK_FALSE(load(g).ok());   // directory beasts/ missing
    Files h = history_data::shipped_files();
    for (const Monster& m : monsters()) h.erase(monster_path(m.id));
    CHECK_FALSE(load(h).ok());   // directory monsters/ missing
}

TEST_CASE("P5-02 4.1: every beast field required; ranges one step outside; grammars; habitat bounds") {
    const std::string path = beast_path("wolf");
    for (const std::string_view field : beast_fields()) {
        CAPTURE(field);
        Files f = history_data::shipped_files();
        std::string& t = f[path];
        const std::size_t at = t.find("\n" + std::string(field) + " = ");
        REQUIRE(at != std::string::npos);
        std::size_t end = t.find('\n', at + 1);
        if (field == "habitat") end = t.find("]\n", at) + 1;
        t.erase(at, end - at);
        const wd::LoadError e = one_error(load(f));
        CHECK(e.path == path);
        CHECK(has(e.message, "beast.wolf." + std::string(field)));
    }
    struct R {
        std::string_view field;
        std::int64_t lo, hi, shipped;
    };
    for (const R& r : {R{"density", 0, 100000, 20}, R{"growth_permille", 1, 1000, 150}, R{"menace", 0, 1000, 10},
                       R{"food", 0, 1000, 0}}) {
        CAPTURE(r.field);
        for (const std::int64_t v : {r.lo - 1, r.hi + 1}) {
            const wd::LoadError e = one_error(load(with_edit(path, line_of(r.field, r.shipped), line_of(r.field, v))));
            CHECK(has(e.message, "beast.wolf." + std::string(r.field)));
        }
        CHECK(load(with_edit(path, line_of(r.field, r.shipped), line_of(r.field, r.lo))).ok());
        CHECK(load(with_edit(path, line_of(r.field, r.shipped), line_of(r.field, r.hi))).ok());
    }
    // habitat weights int(0, 1000)
    CHECK_FALSE(load(with_edit(path, "{ biome = \"heath\", weight = 600 }", "{ biome = \"heath\", weight = 1001 }")).ok());
    CHECK_FALSE(load(with_edit(path, "{ biome = \"heath\", weight = 600 }", "{ biome = \"heath\", weight = -1 }")).ok());
    CHECK(load(with_edit(path, "{ biome = \"heath\", weight = 600 }", "{ biome = \"heath\", weight = 0 }")).ok());
    // habitat array(…, 1, 16): an empty habitat is refused
    {
        Files f = history_data::shipped_files();
        std::string& t = f[path];
        const std::size_t a = t.find("habitat = [");
        const std::size_t b = t.find("]\n", a);
        t.replace(a, b + 1 - a, "habitat = []");
        CHECK_FALSE(load(f).ok());
    }
    // grammar display_name: ASCII letters and space, starting with an upper-case letter (32 bytes)
    for (const char* bad : {"wolf", " Wolf", "Wolf2", "Wo-lf", "Wolfwolfwolfwolfwolfwolfwolfwolfw"}) {
        CAPTURE(bad);
        CHECK_FALSE(load(with_edit(path, "name = \"Wolf\"", "name = \"" + std::string(bad) + "\"")).ok());
    }
    CHECK(load(with_edit(path, "name = \"Wolf\"", "name = \"Grey Wolf\"")).ok());
    // grammar beast_plural: lower-case letters and single spaces, starting and ending with a letter
    for (const char* bad : {"Wolves", "wolves ", " wolves", "grey  wolves", "wolves2", "wol-ves"}) {
        CAPTURE(bad);
        CHECK_FALSE(load(with_edit(path, "plural = \"wolves\"", "plural = \"" + std::string(bad) + "\"")).ok());
    }
    CHECK(load(with_edit(path, "plural = \"wolves\"", "plural = \"grey wolves\"")).ok());
}

TEST_CASE("P5-02 4.1 beast_habitat_biomes: an unknown biome, a biome listed twice (messages of 4.1)") {
    const std::string path = beast_path("bear");
    check_failure(one_error(load(with_edit(path, "{ biome = \"heath\", weight = 200 }",
                                           "{ biome = \"moon\", weight = 200 }"))),
                  path, "beast_habitat_biomes", "beast.bear", "habitat[4].biome 'moon' is not a biome");
    check_failure(one_error(load(with_edit(path, "{ biome = \"heath\", weight = 200 }",
                                           "{ biome = \"alpine\", weight = 200 }"))),
                  path, "beast_habitat_biomes", "beast.bear", "habitat[4].biome 'alpine' is listed twice");
}

TEST_CASE("P5-02 4.2: every monster field required; ranges one step outside; enums; grammar monster_noun") {
    const std::string path = monster_path("troll");
    for (const std::string_view field : monster_fields()) {
        CAPTURE(field);
        Files f = history_data::shipped_files();
        std::string& t = f[path];
        const std::size_t at = t.find("\n" + std::string(field) + " = ");
        REQUIRE(at != std::string::npos);
        std::size_t end = t.find('\n', at + 1);
        if (field == "biomes") end = t.find("]\n", at) + 1;
        t.erase(at, end - at);
        const wd::LoadError e = one_error(load(f));
        CHECK(e.path == path);
        CHECK(has(e.message, "monster.troll." + std::string(field)));
    }
    struct R {
        std::string_view field;
        std::int64_t lo, hi, shipped;
    };
    for (const R& r : {R{"wake_rate", 0, 10000000, 17}, R{"might", 1, 10000000, 300}, R{"range_hops", 0, 4, 1},
                       R{"raid_permille", 0, 1000, 80}, R{"kill_max", 0, 10000000, 60},
                       R{"overrun_permille", 0, 1000, 100}, R{"active_years", 1, 100000, 120},
                       R{"rewake_ppm", 0, 1000000, 5000}, R{"lifespan_years", 0, 100000, 800},
                       R{"max_alive", 0, 64, 3}, R{"hoard_permille", 0, 1000, 5}}) {
        CAPTURE(r.field);
        for (const std::int64_t v : {r.lo - 1, r.hi + 1}) {
            const wd::LoadError e = one_error(load(with_edit(path, line_of(r.field, r.shipped), line_of(r.field, v))));
            CHECK(has(e.message, "monster.troll." + std::string(r.field)));
        }
        CHECK(load(with_edit(path, line_of(r.field, r.shipped), line_of(r.field, r.lo))).ok());
        CHECK(load(with_edit(path, line_of(r.field, r.shipped), line_of(r.field, r.hi))).ok());
    }
    // the tolls' ranges (with the other toll moved so that monster_toll_ordered holds)
    CHECK_FALSE(load(with_edit(path, "toll_min_permille = 20", "toll_min_permille = -1")).ok());
    CHECK_FALSE(load(with_edit(path, "toll_max_permille = 80", "toll_max_permille = 1001")).ok());
    CHECK(load(with_edit(path, "toll_max_permille = 80", "toll_max_permille = 1000")).ok());
    // enum wake (wild, dead); overruns enum (hamlet, village, town), at most 3
    CHECK_FALSE(load(with_edit(path, "wake = \"wild\"", "wake = \"sleepy\"")).ok());
    CHECK_FALSE(load(with_edit(path, "overruns = [\"hamlet\", \"village\"]", "overruns = [\"city\"]")).ok());
    CHECK_FALSE(load(with_edit(path, "overruns = [\"hamlet\", \"village\"]",
                               "overruns = [\"hamlet\", \"village\", \"town\", \"hamlet\"]")).ok());
    CHECK(load(with_edit(path, "overruns = [\"hamlet\", \"village\"]", "overruns = []")).ok());
    // name_forms and lair_forms: array(string(48, name_form), 1, 8)
    CHECK_FALSE(load(with_edit(path, "name_forms = [\"Old {root}\", \"{root}\"]", "name_forms = []")).ok());
    CHECK_FALSE(load(with_edit(path, "lair_forms = [\"{root}'s Hollow\", \"The Den of {root}\"]", "lair_forms = []")).ok());
    CHECK_FALSE(load(with_edit(path, "name_forms = [\"Old {root}\", \"{root}\"]",
                               "name_forms = [\"a {root}\", \"b {root}\", \"c {root}\", \"d {root}\", \"e {root}\", "
                               "\"f {root}\", \"g {root}\", \"h {root}\", \"{root}\"]")).ok());
    CHECK_FALSE(load(with_edit(path, "\"The Den of {root}\"", "\"The Den of the troll\"")).ok());   // no {root}
    CHECK_FALSE(load(with_edit(path, "\"The Den of {root}\"", "\"The Den of {root} {root}\"")).ok());
    CHECK_FALSE(load(with_edit(path, "\"The Den of {root}\"", "\"The Den of {root}!\"")).ok());
    // grammar monster_noun: lower-case letters, space and ', starting with `a ` or `an `; at most 48 bytes
    for (const char* bad : {"troll of the hills", "A troll", "a Troll", "a troll-kin", "the troll",
                            "a trollllllllllllllllllllllllllllllllllllllllllllll"}) {
        CAPTURE(bad);
        CHECK_FALSE(load(with_edit(path, "noun = \"a troll of the hills\"", "noun = \"" + std::string(bad) + "\"")).ok());
    }
    CHECK(load(with_edit(path, "noun = \"a troll of the hills\"", "noun = \"an old troll's kin\"")).ok());
    // biomes array(…, 0, 16) with weights int(0, 1000)
    CHECK_FALSE(load(with_edit(path, "{ biome = \"heath\", weight = 300 }", "{ biome = \"heath\", weight = 1001 }")).ok());
}

TEST_CASE("P5-02 4.2: the named checks of kind monster in order, with the messages the spec gives") {
    const std::string troll = monster_path("troll");
    // monster_biomes: as beast_habitat_biomes, on biomes
    check_failure(one_error(load(with_edit(troll, "{ biome = \"heath\", weight = 300 }",
                                           "{ biome = \"sky\", weight = 300 }"))),
                  troll, "monster_biomes", "monster.troll", "biomes[3].biome 'sky' is not a biome");
    check_failure(one_error(load(with_edit(troll, "{ biome = \"heath\", weight = 300 }",
                                           "{ biome = \"alpine\", weight = 300 }"))),
                  troll, "monster_biomes", "monster.troll", "biomes[3].biome 'alpine' is listed twice");
    // monster_wild_habitat
    {
        Files f = history_data::shipped_files();
        std::string& t = f[troll];
        const std::size_t a = t.find("biomes = [");
        const std::size_t b = t.find("]\n", a);
        t.replace(a, b + 1 - a, "biomes = [{ biome = \"heath\", weight = 0 }]");
        check_failure(one_error(load(f)), troll, "monster_wild_habitat", "monster.troll",
                      "a wild monster needs a biome with a weight above 0");
    }
    {   // a dead kind needs none (the shipped undead has no biomes)
        const Load l = load(history_data::shipped_files());
        CHECK(l.ok());
    }
    // monster_culture: naming_culture names a culture definition (message not given by the spec)
    check_failure(one_error(load(with_edit(troll, "naming_culture = \"goblin\"", "naming_culture = \"trollish\""))),
                  troll, "monster_culture", "monster.troll");
    // monster_toll_ordered: toll_min_permille <= toll_max_permille
    check_failure(one_error(load(with_edit(troll, "toll_min_permille = 20", "toll_min_permille = 81"))), troll,
                  "monster_toll_ordered", "monster.troll");
    CHECK(load(with_edit(troll, "toll_min_permille = 20", "toll_min_permille = 80")).ok());
    // monster_overruns_unique
    check_failure(one_error(load(with_edit(troll, "overruns = [\"hamlet\", \"village\"]",
                                           "overruns = [\"hamlet\", \"hamlet\"]"))),
                  troll, "monster_overruns_unique", "monster.troll");
    // monster_bare_form_last
    check_failure(one_error(load(with_edit(troll, "name_forms = [\"Old {root}\", \"{root}\"]",
                                           "name_forms = [\"{root}\", \"Old {root}\"]"))),
                  troll, "monster_bare_form_last", "monster.troll", "name_forms[0] '{root}' must be the last form");
    CHECK(load(with_edit(troll, "name_forms = [\"Old {root}\", \"{root}\"]", "name_forms = [\"Old {root}\"]")).ok());
}

TEST_CASE("P5-02 4.3: history.monsters every field one step outside its range, required; monsters_bold_ordered") {
    const std::string path = "history/monsters.toml";
    for (const Field& fd : tuning_fields()) {
        CAPTURE(fd.name);
        for (const std::int64_t v : {fd.lo - 1, fd.hi + 1}) {
            if (fd.name == "bold_off_permille" && v > 1500) continue;   // would fail monsters_bold_ordered instead
            if (fd.name == "bold_on_permille" && v < 800) {
                // on below off fails the ordering check too; the range error comes first in the loader (one error)
            }
            const Load l = load(with_edit(path, line_of(fd.name, fd.shipped), line_of(fd.name, v)));
            REQUIRE_FALSE(l.ok());
            CHECK(has(dump(l), "history.monsters." + std::string(fd.name)));
        }
        {
            Files f = history_data::shipped_files();
            std::string& t = f[path];
            const std::size_t at = t.find("\n" + line_of(fd.name, fd.shipped) + "\n");
            REQUIRE(at != std::string::npos);
            t.erase(at, line_of(fd.name, fd.shipped).size() + 1);
            const wd::LoadError e = one_error(load(f));
            CHECK(has(e.message, "history.monsters." + std::string(fd.name)));
        }
    }
    // the pressure record: int(0, 10000) each
    for (const char* bad : {"pressure = { low = -1, normal = 1000, high = 1600 }",
                            "pressure = { low = 500, normal = 10001, high = 1600 }",
                            "pressure = { low = 500, normal = 1000 }"}) {
        CAPTURE(bad);
        CHECK_FALSE(load(with_edit(path, "pressure = { low = 500, normal = 1000, high = 1600 }", bad)).ok());
    }
    CHECK(load(with_edit(path, "pressure = { low = 500, normal = 1000, high = 1600 }",
                         "pressure = { low = 0, normal = 0, high = 10000 }")).ok());
    // monsters_bold_ordered: bold_off_permille <= bold_on_permille (message not given by the spec)
    check_failure(one_error(load(with_edit(path, "bold_off_permille = 800", "bold_off_permille = 1501"))), path,
                  "monsters_bold_ordered", "history.monsters");
    CHECK(load(with_edit(path, "bold_off_permille = 800", "bold_off_permille = 1500")).ok());
}

TEST_CASE("P5-02 4.4, 4.5: site_forms required, name_form grammar, 1 to 8 forms; ghost_* ranges and required") {
    const std::string path = "cultures/human.toml";
    {
        Files f = history_data::shipped_files();
        std::string& t = f[path];
        const std::size_t at = t.find("site_forms = ");
        REQUIRE(at != std::string::npos);
        t.erase(at, t.find('\n', at) + 1 - at);
        const wd::LoadError e = one_error(load(f));
        CHECK(has(e.message, "site_forms"));
    }
    CHECK_FALSE(load(with_edit(path, "site_forms = [\"{root} Watch\", \"{root} Tower\"]", "site_forms = []")).ok());
    CHECK_FALSE(load(with_edit(path, "site_forms = [\"{root} Watch\", \"{root} Tower\"]",
                               "site_forms = [\"Watch\", \"{root} Tower\"]")).ok());
    CHECK_FALSE(load(with_edit(path, "site_forms = [\"{root} Watch\", \"{root} Tower\"]",
                               "site_forms = [\" {root} Watch\", \"{root} Tower\"]")).ok());
    CHECK(load(with_edit(path, "site_forms = [\"{root} Watch\", \"{root} Tower\"]",
                         "site_forms = [\"{root}'s Watch\", \"The Watch-Tower of {root}\"]")).ok());
    const std::string pe = "history/peoples.toml";
    for (const Field& fd : {Field{"ghost_permille", 0, 1000, 200}, Field{"ghost_max_population", 0, 1000000, 300}}) {
        CAPTURE(fd.name);
        for (const std::int64_t v : {fd.lo - 1, fd.hi + 1}) {
            const wd::LoadError e = one_error(load(with_edit(pe, line_of(fd.name, fd.shipped), line_of(fd.name, v))));
            CHECK(has(e.message, "history.peoples." + std::string(fd.name)));
        }
        CHECK(load(with_edit(pe, line_of(fd.name, fd.shipped), line_of(fd.name, fd.lo))).ok());
        CHECK(load(with_edit(pe, line_of(fd.name, fd.shipped), line_of(fd.name, fd.hi))).ok());
        Files f = history_data::shipped_files();
        std::string& t = f[pe];
        const std::size_t at = t.find(line_of(fd.name, fd.shipped) + "\n");
        t.erase(at, line_of(fd.name, fd.shipped).size() + 1);
        CHECK(has(one_error(load(f)).message, "history.peoples." + std::string(fd.name)));
    }
}

TEST_CASE("P5-02 9.3, 21.2: renaming rule 7 ravage, the forms of rules 2 to 5, {stem}; kind 43's cause words") {
    const Load l = load(history_data::shipped_files());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& r = definition(*l.content, "history", "renaming").record;
    const std::vector<wd::Value>& rules = r.value_at(5).as_array();
    REQUIRE(rules.size() == 7U);
    const wd::Record& seven = rules[6].as_record();
    CHECK(seven.get_string("key") == "ravage");
    CHECK(strings(seven.value_at(1)) == std::vector<std::string>{"settlement_overrun"});
    CHECK(strings(seven.value_at(2)).empty());
    CHECK(seven.get_int("min_magnitude") == 1);
    CHECK(seven.get_int("window_years") == 1);
    CHECK(seven.get_int("min_salience") == 0);
    CHECK_FALSE(seven.get_bool("capital"));
    CHECK(seven.get_int("chance_permille") == 600);
    CHECK(seven.get_int("weight") == 75);
    CHECK(seven.get_string("name_role") == "monster");
    CHECK(seven.get_string("namer_role") == "civilization");
    CHECK(strings(seven.value_at(11)) == std::vector<std::string>{"The Haunt of {name}", "{name}'s Shadow",
                                                                   "The Forsaken {noun}", "The Hollow of {name}"});
    const std::vector<std::vector<std::string>> forms = {
        {"{stem}'s Fall", "The Ashes of {name}", "The Ruin of {name}", "The Fallen {noun}"},
        {"{stem}'s Pyre", "The Ashes of {name}", "The Burnt {noun}", "The Cinder {noun}"},
        {"The Red {noun}", "The Bloody {noun}", "The Rout of {name}", "The {noun} of the Slain", "The Weeping {noun}",
         "The Grim {noun}"},
        {"The Emptied {noun}", "The Fever {noun}", "The Silent {noun}", "{stem}'s Grave", "The Pale {noun}",
         "The Hollow {noun}"}};
    for (std::size_t i = 0; i < forms.size(); ++i) {
        CAPTURE(i + 1);
        CHECK(strings(rules[i + 1].as_record().value_at(11)) == forms[i]);
    }
    // the grammar rename_form gains {stem}: at most once, not together with {name}
    const std::string path = "history/renaming.toml";
    CHECK_FALSE(load(with_edit(path, "\"{stem}'s Fall\"", "\"{stem}'s {name}\"")).ok());
    CHECK_FALSE(load(with_edit(path, "\"{stem}'s Fall\"", "\"{stem}{stem}\"")).ok());
    CHECK(load(with_edit(path, "\"{stem}'s Fall\"", "\"The Fall of {stem}\"")).ok());
    // the report entries: kind 43's cause words (21.2) and the twelve entries of 10.2
    const wd::Record& rep = definition(*l.content, "history", "report").record;
    const std::vector<wd::Value>& ev = rep.value_at(0).as_array();
    REQUIRE(ev.size() == 55U);
    CHECK(has(ev[42].as_record().get_string("template"),
              "became_city=the rise of a realm's seat,settlement_overrun=the ravage|After |, }"));
    const std::vector<std::pair<std::string_view, std::int64_t>> want = {
        {"monster_appeared", 45}, {"undead_rose", 50},       {"monster_woke", 25},        {"monster_slept", 10},
        {"monster_raid", 15},     {"settlement_overrun", 50}, {"monster_slain", 55},       {"monster_driven_off", 30},
        {"monster_died", 30},     {"beasts_bold", 15},        {"outpost_built", 12},       {"outpost_abandoned", 10}};
    for (std::size_t i = 0; i < want.size(); ++i) {
        const wd::Record& e = ev[43 + i].as_record();
        CHECK(e.get_string("kind") == want[i].first);
        CHECK(e.get_int("salience") == want[i].second);
    }
    // D2 (the test writer's reading): kinds 48 and 49 read {@settlement}; neither kind has a role `settlement`
    CHECK(ev[47].as_record().get_string("template") ==
          "{monster} fell upon {@settlement}; {dead?no one died|one died|# died}.");
    CHECK(ev[52].as_record().get_string("template") ==
          "In {@region} the {@beast} grew bold, and travellers from {settlement} went in fear; {dead?none|one|#} died "
          "on the roads.");
}

}  // TEST_SUITE
