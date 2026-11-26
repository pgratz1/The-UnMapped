// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r2) test 16 items 1 and (r2) 15 on the shipped data root (game/data):
// r2 adds the eight warren and aid fields of history.monsters (4.2), the race field delving_permille with its bounds,
// its absence (0) and the check race_delves_one_way (4.5), and the shipped race values. The three
// deep kinds load (4.1: ids after wyvern, wake deep, no biomes, the table's values); check monster_deep_biomes; every
// new history.monsters field of 4.2 after undead_regions_ref in order, refused out of its bounds and when absent;
// check monsters_deep_years_ordered; history.conflict.forge_depth_permille after arms_strength_permille (4.3); renaming
// rule 8 `deep` (4.4) loaded and accepted by renaming_rules; the six kinds of 0.28.0 keep ids 1 to 6 (invariant 8).
// The values are the spec's starts (15.2's calibration may move them; a moved value is named here at integration).
// The data root is copied to a scratch directory under the build tree and one file is edited per case. Every case
// starts with P507_REQUIRE_INTEGRATED(): the 0.28.0 data have none of these files or fields.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../db_test_support.hpp"
#include "../p3_01_history_support.hpp"
#include "p5_03_relations_checks.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_07_api.hpp"

namespace {

namespace fs = std::filesystem;
namespace wd = worldgen::data;
using history_test::Load;

fs::path data_root() { return history_test::source_root() / "game" / "data"; }

std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

Load load_edited(const std::string& file, const std::string& text) {
    static int n = 0;
    static dbtest::TempDir dir("p507-data");
    const fs::path root = dir.path() / ("root-" + std::to_string(++n));
    fs::copy(data_root(), root, fs::copy_options::recursive);
    {
        std::ofstream out(root / file, std::ios::binary | std::ios::trunc);
        out << text;
    }
    Load l = history_test::load_disk(root);
    fs::remove_all(root);
    return l;
}

// `name = <rhs>` in place of the line of `name`.
std::string with_line(std::string text, const std::string& name, const std::string& rhs) {
    const std::size_t a = text.find("\n" + name + " = ");
    REQUIRE_MESSAGE(a != std::string::npos, name << " not in the file");
    const std::size_t e = text.find('\n', a + 1);
    text.replace(a, e - a, "\n" + name + " = " + rhs);
    return text;
}
std::string without(std::string text, const std::string& name) {
    const std::size_t a = text.find("\n" + name + " = ");
    REQUIRE_MESSAGE(a != std::string::npos, name << " not in the file");
    text.erase(a, text.find('\n', a + 1) - a);
    return text;
}

const wd::ContentSet& shipped() {
    static const wd::ContentSet c = [] {
        Load l = history_test::load_disk(data_root());
        REQUIRE_MESSAGE(l.ok(), history_test::dump(l));
        return std::move(*l.content);
    }();
    return c;
}

const wd::Record& history(const char* id) {
    const wd::Definition* d = p503check::definition(shipped(), "history", id);
    REQUIRE(d != nullptr);
    return d->record;
}

std::vector<std::int64_t> ints_of(const wd::Value& v) {
    std::vector<std::int64_t> out;
    for (const wd::Value& x : v.as_array()) out.push_back(x.as_int());
    return out;
}
std::vector<std::string> strings_of(const wd::Value& v) {
    std::vector<std::string> out;
    for (const wd::Value& x : v.as_array()) out.push_back(x.as_string());
    return out;
}

// Refused with an error that holds every word. Not necessarily the first error: a race whose field is out of range is
// left out, so the loader may report a check of the kind (race_order_dense) first (integration reading).
void refused(const std::string& file, const std::string& text, const std::vector<std::string>& words) {
    const Load l = load_edited(file, text);
    REQUIRE_FALSE(l.ok());
    REQUIRE_FALSE(l.errors.empty());
    CAPTURE(l.errors[0].message);
    bool found = false;
    for (const auto& err : l.errors) {
        bool all = true;
        for (const std::string& w : words) all = all && history_test::contains(err.message, w);
        found = found || all;
    }
    CAPTURE(words.front());
    CHECK(found);
}
void accepted(const std::string& file, const std::string& text) {
    const Load l = load_edited(file, text);
    CHECK_MESSAGE(l.ok(), history_test::dump(l));
}

struct Kind {
    const char* id;
    const char* name;
    bool grouped;
    std::int64_t weight, might, raid, toll_min, toll_max, kill_max, take, active, rewake, max_alive, hoard;
    const char* epithet;
};

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-07 4.1, invariant 8: the three deep kinds after wyvern, with the spec's values; ids 1 to 6 unchanged") {
    P507_REQUIRE_INTEGRATED();
    std::vector<std::string> ids;
    for (const wd::KindContent& k : shipped().kinds) {
        if (k.kind != "monster") continue;
        for (const wd::Definition& d : k.definitions) ids.push_back(d.id);
    }
    CHECK(ids == std::vector<std::string>{"dragon", "giant", "ogre", "troll", "undead", "wyvern", "ymrach", "ysgarth",
                                          "zhakul"});
    const std::vector<Kind> kinds = {
        {"ymrach", "Ymrach", false, 500, 4000, 60, 30, 100, 300, 600, 60, 2000, 2, 10, "Deepbane"},
        {"ysgarth", "Ysgarth", true, 350, 2500, 100, 20, 80, 150, 400, 100, 3000, 2, 2, "the Lightbearer"},
        {"zhakul", "Zhakul", false, 150, 7000, 40, 50, 150, 500, 800, 30, 1000, 1, 20, "Flamebreaker"}};
    for (const Kind& k : kinds) {
        CAPTURE(k.id);
        const wd::Definition* d = p503check::definition(shipped(), "monster", k.id);
        REQUIRE(d != nullptr);
        const wd::Record& r = d->record;
        CHECK(r.find("name")->as_string() == k.name);
        CHECK(r.find("grouped")->as_bool() == k.grouped);
        CHECK(r.find("wake")->as_string() == "deep");
        CHECK(r.find("biomes")->as_array().empty());
        CHECK(r.find("overruns")->as_array().empty());
        CHECK(r.find("naming_culture")->as_string() == "dwarven");
        CHECK(r.find("flare")->as_bool() == false);
        CHECK(r.get_int("lifespan_years") == 0);   // ageless
        CHECK(r.get_int("range_hops") == 1);
        // 15.2 may calibrate these (data only): the starts of 4.1, overrun_permille as calibrated (300, 200, 400 ->
        // 600, 400, 800; docs/reviews/p5-07-calibration.md)
        CHECK(r.get_int("wake_rate") == k.weight);
        CHECK(r.get_int("might") == k.might);
        CHECK(r.get_int("raid_permille") == k.raid);
        CHECK(r.get_int("toll_min_permille") == k.toll_min);
        CHECK(r.get_int("toll_max_permille") == k.toll_max);
        CHECK(r.get_int("kill_max") == k.kill_max);
        CHECK(r.get_int("overrun_permille") == k.take);
        CHECK(r.get_int("active_years") == k.active);
        CHECK(r.get_int("rewake_ppm") == k.rewake);
        CHECK(r.get_int("max_alive") == k.max_alive);
        CHECK(r.get_int("hoard_permille") == k.hoard);
        CHECK(r.find("slayer_epithet")->as_string() == k.epithet);
    }
    CHECK(strings_of(*p503check::definition(shipped(), "monster", "ymrach")->record.find("name_forms")) ==
          std::vector<std::string>{"{root} the Deep-Coiled", "{root} Stonegnawer", "{root}"});
    CHECK(strings_of(*p503check::definition(shipped(), "monster", "ysgarth")->record.find("lair_forms")) ==
          std::vector<std::string>{"The Warrens of {root}", "The Nest of {root}"});
    CHECK(strings_of(*p503check::definition(shipped(), "monster", "zhakul")->record.find("raid_phrases")) ==
          std::vector<std::string>{"came up burning beneath", "set the deep halls aflame under", "fell in fire upon"});
}

TEST_CASE("P5-07 4.1: check monster_deep_biomes; a deep kind's wake must be an enum value") {
    P507_REQUIRE_INTEGRATED();
    const std::string f = "monsters/ymrach.toml";
    const std::string text = read(data_root() / f);
    refused(f, with_line(text, "biomes", "[{ biome = \"alpine\", weight = 100 }]"),
            {"monster_deep_biomes", "a deep monster has no biomes"});
    refused(f, with_line(text, "wake", "\"deeper\""), {"wake"});
}

TEST_CASE("P5-07 4.2: history.monsters: the sixteen fields after undead_regions_ref, their bounds and absence") {
    P507_REQUIRE_INTEGRATED();
    const wd::Record& r = history("monsters");
    const std::vector<std::string> names = {"deep_years", "deep_delve_permille", "deep_wake_ppm",
                                            "deep_gate_levy_permille", "deep_seal_max_permille", "deep_toll_permille",
                                            "deep_takes_max", "deep_reach_hops",
                                            // r2, in 4.2's order
                                            "deep_warren_population_min", "deep_warren_ore_min",
                                            "deep_warren_wake_permille", "deep_aid_permille", "deep_aid_hops",
                                            "deep_aid_levy_permille", "deep_aid_max", "deep_aid_memory"};
    std::size_t at = r.size();
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (r.name_at(i) == "undead_regions_ref") at = i;
    }
    REQUIRE(at < r.size());
    for (std::size_t i = 0; i < names.size(); ++i) {
        REQUIRE(at + 1 + i < r.size());
        CHECK(r.name_at(at + 1 + i) == names[i]);
    }
    // as calibrated (docs/reviews/p5-07-calibration.md; the starts of 4.2 were 150, 500, 1000; 10; 0, 150, 600; 2; 1;
    // 600; 1)
    CHECK(ints_of(*r.find("deep_years")) == std::vector<std::int64_t>{250, 1000, 2500});
    CHECK(r.get_int("deep_delve_permille") == 6);
    CHECK(ints_of(*r.find("deep_wake_ppm")) == std::vector<std::int64_t>{0, 350, 1400});
    CHECK(r.get_int("deep_gate_levy_permille") == 1000);
    CHECK(r.get_int("deep_seal_max_permille") == 800);
    CHECK(r.get_int("deep_toll_permille") == 400);
    CHECK(r.get_int("deep_takes_max") == 3);
    CHECK(r.get_int("deep_reach_hops") == 2);
    // r2 starts (15.2's steps 8 to 11 may calibrate them; a moved value is named here at integration)
    CHECK(r.get_int("deep_warren_population_min") == 3200);
    CHECK(r.get_int("deep_warren_ore_min") == 500);
    CHECK(r.get_int("deep_warren_wake_permille") == 200);
    CHECK(r.get_int("deep_aid_permille") == 500);
    CHECK(r.get_int("deep_aid_hops") == 2);
    CHECK(r.get_int("deep_aid_levy_permille") == 500);
    CHECK(r.get_int("deep_aid_max") == 2);
    CHECK(r.get_int("deep_aid_memory") == 30);
    const std::string f = "history/monsters.toml";
    const std::string text = read(data_root() / f);
    struct B {
        const char* name;
        std::int64_t lo, hi;
    };
    for (const B& b : {B{"deep_delve_permille", 0, 1000}, B{"deep_gate_levy_permille", 0, 4000},
                       B{"deep_seal_max_permille", 0, 1000}, B{"deep_toll_permille", 0, 1000},
                       B{"deep_takes_max", 0, 16}, B{"deep_reach_hops", 0, 4},
                       // r2
                       B{"deep_warren_population_min", 0, 1000000}, B{"deep_warren_ore_min", 0, 100000},
                       B{"deep_warren_wake_permille", 0, 1000}, B{"deep_aid_permille", 0, 1000},
                       B{"deep_aid_hops", 0, 8}, B{"deep_aid_levy_permille", 0, 4000}, B{"deep_aid_max", 0, 8},
                       B{"deep_aid_memory", 0, 500}}) {
        CAPTURE(b.name);
        const std::string expected = "expected an integer from " + std::to_string(b.lo) + " to " + std::to_string(b.hi);
        refused(f, with_line(text, b.name, std::to_string(b.lo - 1)), {b.name, expected});
        refused(f, with_line(text, b.name, std::to_string(b.hi + 1)), {b.name, expected});
        refused(f, without(text, b.name), {"missing required field", b.name});
    }
    // the arrays: three elements each, int(1, 100000) and int(0, 1000000)
    refused(f, with_line(text, "deep_years", "[0, 500, 1000]"), {"deep_years"});
    refused(f, with_line(text, "deep_years", "[150, 500, 100001]"), {"deep_years"});
    refused(f, with_line(text, "deep_years", "[150, 500]"), {"deep_years"});
    refused(f, with_line(text, "deep_years", "[150, 500, 1000, 2000]"), {"deep_years"});
    refused(f, with_line(text, "deep_wake_ppm", "[0, -1, 600]"), {"deep_wake_ppm"});
    refused(f, with_line(text, "deep_wake_ppm", "[0, 150, 1000001]"), {"deep_wake_ppm"});
    refused(f, without(text, "deep_years"), {"missing required field", "deep_years"});
    refused(f, without(text, "deep_wake_ppm"), {"missing required field", "deep_wake_ppm"});
    accepted(f, with_line(text, "deep_wake_ppm", "[0, 0, 1000000]"));
}

TEST_CASE("P5-07 4.2: check monsters_deep_years_ordered") {
    P507_REQUIRE_INTEGRATED();
    const std::string f = "history/monsters.toml";
    const std::string text = read(data_root() / f);
    refused(f, with_line(text, "deep_years", "[150, 150, 1000]"),
            {"monsters_deep_years_ordered", "deep_years[1] 150 is not above deep_years[0] 150"});
    refused(f, with_line(text, "deep_years", "[5, 10, 9]"),
            {"monsters_deep_years_ordered", "deep_years[2] 9 is not above deep_years[1] 10"});
    accepted(f, with_line(text, "deep_years", "[5, 10, 20]"));   // depths-small
}

TEST_CASE("P5-07 r2 4.5: race field delving_permille: the shipped values, bounds, absence and race_delves_one_way") {
    P507_REQUIRE_INTEGRATED();
    const std::vector<std::pair<std::string, std::int64_t>> want = {{"dwarf", 0}, {"elf", 0}, {"faerie", 0},
                                                                     {"goblin", 300}, {"human", 0}, {"orc", 0}};
    for (const auto& [race, v] : want) {
        CAPTURE(race);
        const wd::Definition* d = p503check::definition(shipped(), "race", race);
        REQUIRE(d != nullptr);
        // 4.5: "written in every file so the choice is visible"
        REQUIRE(d->record.find("delving_permille") != nullptr);
        CHECK(d->record.get_int("delving_permille") == v);
        CHECK(d->record.get_int("mining_permille") == (race == "dwarf" ? 1000 : 0));   // P5-04, unchanged
    }
    const std::string gob = "races/goblin.toml";
    const std::string gt = read(data_root() / gob);
    refused(gob, with_line(gt, "delving_permille", "-1"), {"delving_permille", "expected an integer from 0 to 1000"});
    refused(gob, with_line(gt, "delving_permille", "1001"), {"delving_permille", "expected an integer from 0 to 1000"});
    accepted(gob, with_line(gt, "delving_permille", "1000"));
    // optional: absent counts 0 (4.5, 6.12 item 3)
    {
        const Load l = load_edited(gob, without(gt, "delving_permille"));
        REQUIRE_MESSAGE(l.ok(), history_test::dump(l));
        const wd::Definition* d = p503check::definition(*l.content, "race", "goblin");
        REQUIRE(d != nullptr);
        const wd::Value* v = d->record.find("delving_permille");
        CHECK((v == nullptr || v->as_int() == 0));
    }
    // race_delves_one_way: mining and delving both above 0 refused
    const std::string dw = "races/dwarf.toml";
    refused(dw, with_line(read(data_root() / dw), "delving_permille", "1"),
            {"race_delves_one_way", "a race delves mines or warrens, not both"});
    refused(gob, with_line(with_line(gt, "mining_permille", "1"), "delving_permille", "300"),
            {"race_delves_one_way", "a race delves mines or warrens, not both"});
    accepted(dw, with_line(read(data_root() / dw), "delving_permille", "0"));
}

TEST_CASE("P5-07 4.3: history.conflict.forge_depth_permille right after arms_strength_permille") {
    P507_REQUIRE_INTEGRATED();
    const wd::Record& r = history("conflict");
    std::size_t at = r.size();
    for (std::size_t i = 0; i < r.size(); ++i) {
        if (r.name_at(i) == "arms_strength_permille") at = i;
    }
    REQUIRE(at + 1 < r.size());
    CHECK(r.name_at(at + 1) == "forge_depth_permille");
    CHECK(r.get_int("forge_depth_permille") == 15);
    const std::string f = "history/conflict.toml";
    const std::string text = read(data_root() / f);
    refused(f, with_line(text, "forge_depth_permille", "-1"), {"forge_depth_permille", "expected an integer from 0 to 1000"});
    refused(f, with_line(text, "forge_depth_permille", "1001"), {"forge_depth_permille"});
    refused(f, without(text, "forge_depth_permille"), {"missing required field", "forge_depth_permille"});
}

TEST_CASE("P5-07 4.4: renaming rule 8 deep is loaded as the spec's table and passes renaming_rules") {
    P507_REQUIRE_INTEGRATED();
    const wd::Value* rules = history("renaming").find("rules");
    REQUIRE(rules != nullptr);
    const auto& rs = rules->as_array();
    REQUIRE(rs.size() >= 8U);
    const wd::Record& r = rs[7].as_record();
    CHECK(r.find("key")->as_string() == "deep");
    CHECK(strings_of(*r.find("kinds")) == std::vector<std::string>{"hold_lost"});
    CHECK(r.find("values")->as_array().empty());
    CHECK(r.get_int("min_magnitude") == 1);
    CHECK(r.get_int("window_years") == 1);
    CHECK(r.get_int("min_salience") == 0);
    CHECK(r.find("capital")->as_bool() == false);
    CHECK(r.get_int("chance_permille") == 700);
    CHECK(r.get_int("weight") == 85);
    CHECK(r.find("name_role")->as_string() == "monster");
    CHECK(r.find("namer_role")->as_string() == "civilization");
    CHECK(strings_of(*r.find("forms")) ==
          std::vector<std::string>{"The Deeps of {name}", "{name}'s Dark", "The Lost {noun}", "The Sunken {noun}"});
    // renaming_rules is a stage check (P5-12): the history stage refuses a rule it rejects, so a generated world with
    // the shipped data is the check (the property tests generate them; here one 512 world's history runs)
    CHECK(worldgen::db::WorldReader::open(p503run::world_path(0, 512)).ok());
}

}  // TEST_SUITE
