// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) test 24.1 item 1 (generator 0.24.0): the race field stances
// and its check race_stances (4.1), the fixed id history.relations with every field's bounds and absence and the
// check relations_bands (4.2), the new history.monsters fields and monsters_hunt_ordered (9.4), the new
// history.peoples fields and peoples_abandon_loss_ordered (11.1), raid_phrases and bold_phrases with the grammar
// `phrase` (9.5, 13), `{place}` in monster forms and monster_place_forms (12.2), through the loader and the history
// data checks. The data are the spec's (p5_03_relations_data.hpp over the P3-01 listing); the shipped values that
// calibration may move (22 step 10) are compared in the last case, against the listing as regenerated in phase 2.
//
// Written from the spec before the implementation (test-writer rule): no library name beyond P5-02's.

#include <doctest/doctest.h>

#include "worldgen/history/data_checks.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../p3_01_history_support.hpp"
#include "p5_03_relations_data.hpp"
#include "p5_03a_relations_data.hpp"

namespace {

namespace wd = worldgen::data;
using history_data::Files;
using namespace history_test;
// P5-03a (0.25.0): the loader requires the eleven new history.relations fields and undead_dead_permille, so the
// P5-03 cases run on the 0.25.0 listing (p5_03a_relations_data.hpp).
Files files_024() { return p503a::files_025(); }

Files with_edit(const std::string& path, std::string_view from, std::string_view to) {
    Files f = files_024();
    const auto it = f.find(path);
    REQUIRE_MESSAGE(it != f.end(), "no file " << path);
    const std::size_t at = it->second.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, std::string(from) << " not in " << path);
    it->second.replace(at, from.size(), to);
    return f;
}

Load load_checked(const Files& files) {
    Load l = load(files);
    if (!l.ok()) return l;
    std::vector<wd::LoadError> e = worldgen::history::check_history_data(*l.content);
    if (e.empty()) return l;
    Load out;
    out.errors = std::move(e);
    return out;
}

wd::LoadError one_error(const Load& l) {
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    return l.errors[0];
}

bool has(std::string_view s, std::string_view part) { return s.find(part) != std::string_view::npos; }

void check_failure(const wd::LoadError& e, std::string_view path, std::string_view check, std::string_view qualified,
                   std::string_view message) {
    CAPTURE(e.to_string());
    CHECK(e.path == path);
    CHECK(e.message == "check '" + std::string(check) + "' failed for '" + std::string(qualified) + "': " +
                           std::string(message));
}

std::string line_of(std::string_view name, std::int64_t v) { return std::string(name) + " = " + std::to_string(v); }

const wd::Definition& definition(const wd::ContentSet& c, std::string_view kind, std::string_view id) {
    const wd::KindContent* k = c.find_kind(kind);
    REQUIRE_MESSAGE(k != nullptr, "no kind " << kind);
    for (const wd::Definition& d : k->definitions) {
        if (d.id == id) return d;
    }
    FAIL("no definition " << kind << "." << id);
    return k->definitions.front();
}

// Field `name` of a history file cut out (the whole line).
Files without_line(const std::string& path, std::string_view name) {
    Files f = files_024();
    std::string& t = f[path];
    const std::size_t a = t.find("\n" + std::string(name) + " = ");
    REQUIRE(a != std::string::npos);
    const std::size_t b = t.find('\n', a + 1);
    t.erase(a, b - a);
    return f;
}

void field_bounds(const std::string& path, const std::string& qualified, const std::vector<p503data::RelField>& fs,
                  const std::vector<std::string_view>& coupled = {}) {
    for (const p503data::RelField& r : fs) {
        CAPTURE(r.name);
        const wd::LoadError gone = one_error(load(without_line(path, r.name)));
        CHECK(gone.path == path);
        CHECK(has(gone.message, qualified + "." + std::string(r.name)));
        bool skip_range_ok = false;
        for (const std::string_view c : coupled) skip_range_ok = skip_range_ok || c == r.name;
        for (const std::int64_t v : {r.lo - 1, r.hi + 1}) {
            const wd::LoadError e = one_error(load(with_edit(path, line_of(r.name, r.shipped), line_of(r.name, v))));
            CHECK(has(e.message, qualified + "." + std::string(r.name)));
        }
        if (!skip_range_ok) {   // the bounds themselves load (a named check may refuse a coupled field's extreme)
            CHECK(load(with_edit(path, line_of(r.name, r.shipped), line_of(r.name, r.lo))).ok());
            CHECK(load(with_edit(path, line_of(r.name, r.shipped), line_of(r.name, r.hi))).ok());
        }
    }
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 4.1: stances after culture in every race; the shipped stances") {
    const Load l = load_checked(files_024());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    struct S {
        const char* race;
        std::vector<std::pair<std::string, std::int64_t>> st;
    };
    for (const S& s : {S{"elf", {{"faerie", 200}}}, S{"faerie", {{"elf", 200}}}, S{"dwarf", {{"goblin", -100}}},
                       S{"goblin", {{"dwarf", -100}}}, S{"human", {}}, S{"orc", {}}}) {
        CAPTURE(s.race);
        const wd::Record& r = definition(*l.content, "race", s.race).record;
        std::size_t culture = r.size(), stances = r.size();
        for (std::size_t i = 0; i < r.size(); ++i) {
            if (r.name_at(i) == "culture") culture = i;
            if (r.name_at(i) == "stances") stances = i;
        }
        CHECK(stances == culture + 1U);
        REQUIRE(stances < r.size());
        std::vector<std::pair<std::string, std::int64_t>> got;
        for (const wd::Value& v : r.value_at(stances).as_array()) {
            const wd::Record& e = v.as_record();
            REQUIRE(e.size() == 2U);
            CHECK(e.name_at(0) == "race");
            CHECK(e.name_at(1) == "value");
            got.emplace_back(e.get_string("race"), e.get_int("value"));
        }
        CHECK(got == s.st);
    }
}

TEST_CASE("P5-03 4.1: stances required; value int(-500, 500); at most 8 entries; race string(32)") {
    const std::string elf = "races/elf.toml";
    const std::string st = "stances = [{ race = \"faerie\", value = 200 }]";
    {
        Files f = files_024();
        std::string& t = f[elf];
        t.erase(t.find(st), st.size() + 1);
        // The definition does not load, so race_order_dense reports elf's order missing as well (the loader's
        // behaviour for any missing race field): the stances error is among the errors.
        const Load l = load(f);
        REQUIRE_FALSE(l.ok());
        bool found = false;
        for (const wd::LoadError& e : l.errors) found = found || has(e.message, "missing required field 'race.elf.stances'");
        CHECK(found);
    }
    CHECK_FALSE(load(with_edit(elf, st, "stances = [{ race = \"faerie\", value = 501 }]")).ok());
    CHECK_FALSE(load(with_edit(elf, st, "stances = [{ race = \"faerie\", value = -501 }]")).ok());
    CHECK(load_checked(with_edit(elf, st, "stances = [{ race = \"faerie\", value = 500 }]")).ok());
    CHECK(load_checked(with_edit(elf, st, "stances = [{ race = \"faerie\", value = -500 }]")).ok());
    CHECK(load_checked(with_edit(elf, st, "stances = []")).ok());
    std::string nine = "stances = [";
    for (int i = 0; i < 9; ++i) nine += std::string(i ? ", " : "") + "{ race = \"faerie\", value = 1 }";
    CHECK_FALSE(load(with_edit(elf, st, nine + "]")).ok());
    CHECK_FALSE(load(with_edit(elf, st, "stances = [{ race = \"" + std::string(33, 'a') + "\", value = 1 }]")).ok());
}

TEST_CASE("P5-03 4.1 race_stances: not a race, the race itself, listed twice; the first failing entry only") {
    const std::string elf = "races/elf.toml";
    const std::string st = "stances = [{ race = \"faerie\", value = 200 }]";
    // after every kind is loaded (P5-02 25 item 1's path: the kind's directory)
    check_failure(one_error(load_checked(with_edit(elf, st, "stances = [{ race = \"troll\", value = 1 }]"))), "races",
                  "race_stances", "race.elf", "stances[0].race 'troll' is not a race");
    check_failure(one_error(load_checked(with_edit(elf, st, "stances = [{ race = \"faerie\", value = 1 }, "
                                                            "{ race = \"elf\", value = 1 }]"))),
                  "races", "race_stances", "race.elf", "stances[1].race 'elf' is the race itself");
    check_failure(one_error(load_checked(with_edit(elf, st, "stances = [{ race = \"orc\", value = 1 }, "
                                                            "{ race = \"faerie\", value = 1 }, "
                                                            "{ race = \"orc\", value = -1 }]"))),
                  "races", "race_stances", "race.elf", "stances[2].race 'orc' is listed twice");
    check_failure(one_error(load_checked(with_edit(elf, st, "stances = [{ race = \"moon\", value = 1 }, "
                                                            "{ race = \"elf\", value = 1 }]"))),
                  "races", "race_stances", "race.elf", "stances[0].race 'moon' is not a race");
}

TEST_CASE("P5-03 4.2: history.relations fields in order with the shipped values") {
    const Load l = load_checked(files_024());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& r = definition(*l.content, "history", "relations").record;
    REQUIRE(r.size() == p503a::relations_fields().size());
    for (std::size_t i = 0; i < r.size(); ++i) {
        CAPTURE(i);
        CHECK(r.name_at(i) == p503a::relations_fields()[i].name);
        CHECK(r.get_int(p503a::relations_fields()[i].name) == p503a::relations_fields()[i].shipped);
    }
    // a missing file: the fixed id is required
    Files f = files_024();
    f.erase("history/relations.toml");
    CHECK_FALSE(load(f).ok());
}

TEST_CASE("P5-03 4.2: every history.relations field required and one step outside its range") {
    // hatred_max, friend_min and band_margin are bound by relations_bands at their extremes; P5-03a 10.1: memory_max
    // and peace_memory_max by relations_peace_cap, the two settlers' fields by relations_settlers
    field_bounds("history/relations.toml", "history.relations", p503a::relations_fields(),
                 {"hatred_max", "friend_min", "band_margin", "memory_max", "peace_memory_max",
                  "conquest_settlers_permille", "conquest_settlers_span_permille"});
}

TEST_CASE("P5-03 4.2 relations_bands: hatred_max + band_margin < friend_min - band_margin") {
    const std::string p = "history/relations.toml";
    // -500 + 50 < 150 - 50 holds; band_margin 325: -175 < -175 fails
    check_failure(one_error(load(with_edit(p, "band_margin = 50", "band_margin = 325"))), p, "relations_bands",
                  "history.relations", "bands overlap: hatred_max -500 and friend_min 150 with band_margin 325");
    CHECK(load(with_edit(p, "band_margin = 50", "band_margin = 324")).ok());
    Files f = with_edit(p, "friend_min = 150", "friend_min = 0");   // -500 + 300 < 0 - 300 fails
    f[p].replace(f[p].find("band_margin = 50"), 16, "band_margin = 300");
    check_failure(one_error(load(f)), p, "relations_bands", "history.relations",
                  "bands overlap: hatred_max -500 and friend_min 0 with band_margin 300");
    CHECK(load(with_edit(p, "band_margin = 50", "band_margin = 0")).ok());
}

TEST_CASE("P5-03 9.4: the new history.monsters fields after prey_might_max_permille; monsters_hunt_ordered") {
    const Load l = load_checked(files_024());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& r = definition(*l.content, "history", "monsters").record;
    std::size_t at = r.size();
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (r.name_at(i) == "prey_might_max_permille") at = i;
    }
    REQUIRE(at + p503data::monsters_fields_024().size() + p503a::new_monsters_fields().size() + 1U == r.size());
    for (std::size_t i = 0; i < p503data::monsters_fields_024().size(); ++i) {
        CHECK(r.name_at(at + 1 + i) == p503data::monsters_fields_024()[i].name);
        CHECK(r.get_int(p503data::monsters_fields_024()[i].name) == p503data::monsters_fields_024()[i].shipped);
    }
    const std::string p = "history/monsters.toml";
    field_bounds(p, "history.monsters", p503data::monsters_fields_024(), {"hunt_max_permille"});
    check_failure(one_error(load(with_edit(p, "hunt_max_permille = 200", "hunt_max_permille = 19"))), p,
                  "monsters_hunt_ordered", "history.monsters", "hunt_permille 20 is above hunt_max_permille 19");
    CHECK(load(with_edit(p, "hunt_max_permille = 200", "hunt_max_permille = 20")).ok());
}

TEST_CASE("P5-03 11.1: the new history.peoples fields after ghost_max_population; peoples_abandon_loss_ordered") {
    const Load l = load_checked(files_024());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& r = definition(*l.content, "history", "peoples").record;
    std::size_t at = r.size();
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (r.name_at(i) == "ghost_max_population") at = i;
    }
    REQUIRE(at + p503data::peoples_fields_024().size() + 1U == r.size());
    for (std::size_t i = 0; i < p503data::peoples_fields_024().size(); ++i) {
        CHECK(r.name_at(at + 1 + i) == p503data::peoples_fields_024()[i].name);
    }
    const std::string p = "history/peoples.toml";
    field_bounds(p, "history.peoples", p503data::peoples_fields_024(),
                 {"abandon_loss_min_permille", "abandon_loss_max_permille"});
    check_failure(one_error(load(with_edit(p, "abandon_loss_min_permille = 50", "abandon_loss_min_permille = 201"))),
                  p, "peoples_abandon_loss_ordered", "history.peoples",
                  "abandon_loss_min_permille 201 is above abandon_loss_max_permille 200");
    CHECK(load(with_edit(p, "abandon_loss_min_permille = 50", "abandon_loss_min_permille = 200")).ok());
}

TEST_CASE("P5-03 9.5, 13: raid_phrases after hoard_permille, bold_phrases after food; the grammar phrase") {
    const Load l = load_checked(files_024());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    for (const p503data::MonsterEdit& m : p503data::monster_edits()) {
        CAPTURE(m.kind);
        const wd::Record& r = definition(*l.content, "monster", m.kind).record;
        CHECK(r.name_at(r.size() - 1U) == "raid_phrases");
        CHECK(r.name_at(r.size() - 2U) == "hoard_permille");
        CHECK(r.get_array("raid_phrases").size() == 3U);
        if (m.kind == "undead" || m.kind == "wyvern") CHECK(r.get_int("hoard_permille") == 0);   // 9.2
    }
    for (const auto& [b, line] : p503data::bold_phrases()) {
        CAPTURE(b);
        const wd::Record& r = definition(*l.content, "beast", b).record;
        CHECK(r.name_at(r.size() - 1U) == "bold_phrases");
        CHECK(r.name_at(r.size() - 2U) == "food");
    }
    const std::string troll = "monsters/troll.toml";
    const std::string tp = R"(raid_phrases = ["came down by night on", "fell upon", "prowled the edges of"])";
    // array(string(48, phrase), 1, 4), required
    CHECK_FALSE(load(with_edit(troll, tp, "raid_phrases = []")).ok());
    CHECK_FALSE(load(with_edit(troll, tp, R"(raid_phrases = ["a", "b", "c", "d", "e"])")).ok());
    CHECK(load(with_edit(troll, tp, R"(raid_phrases = ["a", "b", "c", "d"])")).ok());
    {
        Files f = files_024();
        std::string& t = f[troll];
        t.erase(t.find(tp), tp.size() + 1);
        CHECK(has(one_error(load(f)).message, "monster.troll.raid_phrases"));
    }
    for (const char* bad : {"Fell upon", "fell upon ", " fell upon", "fell  upon", "fell upon2", "fell, upon", "-fell",
                            "fell'", "fell upon fell upon fell upon fell upon fell upon"}) {
        CAPTURE(bad);
        CHECK_FALSE(load(with_edit(troll, "\"fell upon\"", "\"" + std::string(bad) + "\"")).ok());
    }
    for (const char* good : {"fell upon", "o'er-ran", "a"}) {
        CAPTURE(good);
        CHECK(load(with_edit(troll, "\"fell upon\"", "\"" + std::string(good) + "\"")).ok());
    }
    const std::string deer = "beasts/deer.toml";
    CHECK_FALSE(load(with_edit(deer, R"(bold_phrases = ["strayed into the fields of"])", "bold_phrases = []")).ok());
    CHECK_FALSE(load(with_edit(deer, "\"strayed into the fields of\"", "\"Strayed\"")).ok());
}

TEST_CASE("P5-03 12.2: {place} in a dead kind's forms; monster_place_forms") {
    const std::string undead = "monsters/undead.toml";
    const std::string forms = R"(name_forms = ["The Host of {place}", "The Restless of {place}", "The Dead of {place}"])";
    CHECK(load_checked(files_024()).ok());
    check_failure(one_error(load(with_edit(undead, forms,
                                           R"(name_forms = ["The Host of {place}", "The Restless of {root}"])"))),
                  undead, "monster_place_forms", "monster.undead",
                  "name_forms[1] 'The Restless of {root}' of a dead kind needs {place} and no {root}");
    // A form with neither {root} nor {place} fails the name_form grammar itself (one of the two exactly once),
    // before the named check.
    CHECK(has(one_error(load(with_edit(undead, forms, R"(name_forms = ["The Host"])"))).message,
              "'monster.undead.name_forms[0]' is \"The Host\"; expected"));
    const std::string troll = "monsters/troll.toml";
    check_failure(one_error(load(with_edit(troll, "\"Mossback {root}\"", "\"Mossback of {place}\""))), troll,
                  "monster_place_forms", "monster.troll",
                  "name_forms[4] 'Mossback of {place}' holds {place}, which only a dead kind may");
    // {place} exactly once (the name_form grammar for monster forms)
    CHECK_FALSE(load(with_edit(undead, "\"The Dead of {place}\"", "\"The Dead of {place} {place}\"")).ok());
}

TEST_CASE("P5-03 12.1: the shipped name forms; monster_bare_form_last holds") {
    const Load l = load_checked(files_024());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const std::vector<std::pair<std::string, std::size_t>> counts = {{"dragon", 7}, {"giant", 5}, {"ogre", 6},
                                                                     {"troll", 6}, {"wyvern", 3}, {"undead", 3}};
    for (const auto& [k, nforms] : counts) {
        CAPTURE(k);
        const wd::Record& r = definition(*l.content, "monster", k).record;
        CHECK(r.get_array("name_forms").size() == nforms);
    }
}

TEST_CASE("P5-03 12.3: rule 7 ravage's forms") {
    const Load l = load_checked(files_024());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& r = definition(*l.content, "history", "renaming").record;
    bool found = false;
    for (const wd::Value& rule : r.get_array("rules")) {
        const wd::Record& x = rule.as_record();
        if (x.get_string("key") != "ravage") continue;
        found = true;
        std::vector<std::string> fs;
        for (const wd::Value& v : x.get_array("forms")) fs.push_back(v.as_string());
        CHECK(fs == std::vector<std::string>{"The Haunt of {name}", "{name}'s Shadow", "The Forsaken {noun}",
                                             "{name}'s Waste"});
    }
    CHECK(found);
}

TEST_CASE("P5-03 4, 9, 11, 12: the shipped game/data and both fixture roots load and pass the history checks") {
    // the listing's values are the spec's interim ones; calibration (22 step 10) may move the rates, so this case
    // checks only that each root loads and holds every new field (phase 2 compares the values)
    for (const std::filesystem::path& root :
         {source_root() / "game" / "data", source_root() / "worldgen" / "tests" / "data" / "geo",
          source_root() / "worldgen" / "tests" / "data" / "geo-small"}) {
        CAPTURE(root.string());
        Load l = load_disk(root);
        REQUIRE_MESSAGE(l.ok(), dump(l));
        CHECK(worldgen::history::check_history_data(*l.content).empty());
        const wd::Record& r = definition(*l.content, "history", "relations").record;
        CHECK(r.size() == p503a::relations_fields().size());
        CHECK(definition(*l.content, "history", "monsters").record.find("overrun_people_ref") != nullptr);
        CHECK(definition(*l.content, "history", "peoples").record.find("ghost_fade_years") != nullptr);
        CHECK(definition(*l.content, "race", "elf").record.find("stances") != nullptr);
    }
}

}  // TEST_SUITE
