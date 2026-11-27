// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1) test 16 item 1 on the shipped data root (game/data): the data
// kind god (4.1: nine definitions, ids in canonical order, each field as the table), its checks (god_culture,
// god_monsters, god_monster_weight, god_favour_weights, god_wrath_weights, grammar god_title, the domain `creation`
// refused) through an added definition file; history.pantheon (4.2: the 37 fields in order with their starts, bounds,
// absence and the two checks pantheon_flare_above_act and pantheon_act_below_max); the deeds' checks (4.3: an unknown
// kind, a role that is not a civilization role, against = role, points 0, a positive deed for foes); the five culture
// fields (4.4) and history.chronology.creator_title; the follow-up fields of 12 (deep_stir_permille,
// renew_written_max, tower_lapse_years) with their starts. The values are the spec's starts (15.2's calibration may
// move them; a moved value is named here at integration). The data root is copied to a scratch directory and one file
// is edited or added per case. Every case starts with P508_REQUIRE_INTEGRATED().

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
#include "p5_08_api.hpp"
#include "worldgen/history/data_checks.hpp"

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

// The root with `file` replaced by (or added with) `text`.
Load load_edited(const std::string& file, const std::string& text) {
    static int n = 0;
    static dbtest::TempDir dir("p508-data");
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

void refused(const std::string& file, const std::string& text, const std::vector<std::string>& words) {
    const Load l = load_edited(file, text);
    CAPTURE(words.empty() ? std::string() : words[0]);
    // the cross-kind checks (god_culture, god_monsters) run in the history data checks, after every kind is loaded
    const std::vector<wd::LoadError> errs =
        l.ok() ? worldgen::history::check_history_data(*l.content) : l.errors;
    REQUIRE_FALSE(errs.empty());
    CAPTURE(errs[0].message);
    bool found = false;
    for (const auto& err : errs) {
        bool all = true;
        for (const std::string& w : words) all = all && history_test::contains(err.message, w);
        found = found || all;
    }
    CHECK(found);
}
void accepted(const std::string& file, const std::string& text) {
    const Load l = load_edited(file, text);
    CHECK_MESSAGE(l.ok(), history_test::dump(l));
}

// A whole god definition file in 4.1's field order (the test's own layout: one inline table per record).
std::string god_file(const std::string& id, const std::string& culture, const std::string& camp, const std::string& domain,
                     const std::string& title, const std::string& favour, const std::string& wrath,
                     const std::string& monsters, const std::string& deeds) {
    return "[god." + id + "]\nname_culture = \"" + culture + "\"\ncamp = \"" + camp + "\"\ndomain = \"" + domain +
           "\"\ntitle = \"" + title + "\"\nfavour = " + favour + "\nwrath = " + wrath +
           "\nbless = \"plenty\"\ncurse = \"withering\"\nmonsters = " + monsters + "\ndeeds = " + deeds + "\n";
}
const char* const FAV = "{ bless = 500, champion = 0, mark = 500 }";
const char* const WR = "{ curse = 300, monster = 0, mark = 700 }";
const char* const DEED = "[{ kind = \"sacked\", role = \"attacker\", points = 10, who = \"any\", against = \"\" }]";
std::string test_god(const std::string& culture = "human", const std::string& domain = "lore",
                     const std::string& title = "the Lamp", const std::string& favour = FAV,
                     const std::string& wrath = WR, const std::string& monsters = "[]",
                     const std::string& deeds = DEED) {
    return god_file("zztest", culture, "good", domain, title, favour, wrath, monsters, deeds);
}
const char* const GOD_FILE = "gods/zztest.toml";

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-08 4.1: the nine shipped gods in canonical id order with the table's values") {
    P508_REQUIRE_INTEGRATED();
    // Integration: the favour and wrath weights are the calibrated values of game/data/gods/ (15.2 moved 4.1's
    // starts, e.g. conquest favour 400/500/100 to 120/1000/30); every other column is 4.1's table.
    struct G {
        const char* id;
        const char* culture;
        const char* camp;
        const char* domain;
        const char* title;
        std::int64_t fb, fc, fm, wc, wm, wk;
        const char* bless;
        const char* curse;
        std::vector<std::string> monsters;
    };
    const std::vector<G> want = {
        {"conquest", "orcish", "evil", "war", "the Red Banner", 120, 1000, 30, 700, 0, 300, "valour", "defeat", {}},
        {"craft", "dwarven", "neutral", "craft", "the Hammer Under the Mountain", 150, 1000, 45, 700, 0, 300, "valour",
         "defeat", {}},
        {"death", "goblin", "evil", "death", "the Hollow Keeper", 120, 1000, 180, 300, 350, 200, "valour", "pestilence",
         {"undead"}},
        {"harvest", "faerie", "good", "harvest", "the Giver of Harvests", 210, 1000, 90, 700, 0, 300, "plenty", "withering", {}},
        {"lore", "elven", "good", "lore", "the Lamp of the Wise", 120, 1000, 75, 600, 0, 400, "plenty", "withering", {}},
        {"oaths", "elven", "good", "oaths", "the Keeper of Oaths", 90, 1000, 60, 700, 0, 300, "valour", "defeat", {}},
        {"ruin", "goblin", "evil", "ruin", "the Burner of Halls", 120, 1000, 45, 400, 350, 200, "valour", "pestilence",
         {"dragon", "giant"}},
        {"war", "human", "neutral", "war", "the Shield of Battles", 120, 1000, 30, 700, 0, 300, "valour", "defeat", {}},
        {"wild", "human", "neutral", "wild", "the Hunter in the Green", 150, 1000, 150, 300, 350, 200, "plenty", "withering",
         {"troll", "ogre", "wyvern"}}};
    std::vector<const wd::Definition*> gods;
    for (const wd::KindContent& k : shipped().kinds) {
        if (k.kind != "god") continue;
        for (const wd::Definition& d : k.definitions) gods.push_back(&d);
    }
    REQUIRE(gods.size() == want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        const G& g = want[i];
        CAPTURE(g.id);
        const wd::Record& r = gods[i]->record;
        CHECK(gods[i]->id == g.id);
        CHECK(r.get_string("name_culture") == g.culture);
        CHECK(r.get_string("camp") == g.camp);
        CHECK(r.get_string("domain") == g.domain);
        CHECK(r.get_string("title") == g.title);
        const wd::Record& f = r.find("favour")->as_record();
        const wd::Record& w = r.find("wrath")->as_record();
        CHECK(f.get_int("bless") == g.fb);
        CHECK(f.get_int("champion") == g.fc);
        CHECK(f.get_int("mark") == g.fm);
        CHECK(w.get_int("curse") == g.wc);
        CHECK(w.get_int("monster") == g.wm);
        CHECK(w.get_int("mark") == g.wk);
        CHECK(r.get_string("bless") == g.bless);
        CHECK(r.get_string("curse") == g.curse);
        std::vector<std::string> ms;
        for (const wd::Value& v : r.find("monsters")->as_array()) ms.push_back(v.as_string());
        CHECK(ms == g.monsters);
        CHECK(!r.find("deeds")->as_array().empty());
    }
}

TEST_CASE("P5-08 4.1: the god checks on an added definition") {
    P508_REQUIRE_INTEGRATED();
    accepted(GOD_FILE, test_god());
    refused(GOD_FILE, test_god("dwarf"), {"name_culture dwarf is not a culture"});
    refused(GOD_FILE, test_god("human", "creation"), {"domain"});
    refused(GOD_FILE, test_god("human", "lore", "Lamp of the Wise"), {"title"});
    refused(GOD_FILE, test_god("human", "lore", "the  Lamp"), {"title"});
    refused(GOD_FILE, test_god("human", "lore", "the Lamp", FAV, WR, "[\"ymrach\"]"),
            {"monsters[0] ymrach is not a wild or dead monster"});
    refused(GOD_FILE, test_god("human", "lore", "the Lamp", FAV, "{ curse = 300, monster = 100, mark = 0 }"),
            {"wrath.monster needs monsters"});
    refused(GOD_FILE, test_god("human", "lore", "the Lamp", "{ bless = 0, champion = 0, mark = 0 }"),
            {"favour weights must not all be 0"});
    refused(GOD_FILE, test_god("human", "lore", "the Lamp", FAV, "{ curse = 0, monster = 0, mark = 0 }"),
            {"wrath weights must not all be 0"});
}

TEST_CASE("P5-08 4.3: the deeds' checks") {
    P508_REQUIRE_INTEGRATED();
    auto deed = [](const std::string& kind, const std::string& role, std::int64_t points, const std::string& who,
                   const std::string& against) {
        return "[{ kind = \"" + kind + "\", role = \"" + role + "\", points = " + std::to_string(points) + ", who = \"" +
               who + "\", against = \"" + against + "\" }]";
    };
    auto with_deed = [](const std::string& d) { return test_god("human", "lore", "the Lamp", FAV, WR, "[]", d); };
    refused(GOD_FILE, with_deed(deed("no_such_kind", "attacker", 10, "any", "")), {"deeds[0]", "kind no_such_kind is unknown"});
    refused(GOD_FILE, with_deed(deed("sacked", "settlement", 10, "any", "")),
            {"deeds[0]", "role settlement of sacked is not a civilization role"});
    refused(GOD_FILE, with_deed(deed("sacked", "attacker", 10, "any", "attacker")), {"deeds[0]"});
    refused(GOD_FILE, with_deed(deed("sacked", "attacker", 0, "any", "")), {"deeds[0]"});
    refused(GOD_FILE, with_deed(deed("sacked", "attacker", 10, "foe", "")), {"deeds[0]", "a deed for foes must be negative"});
    accepted(GOD_FILE, with_deed(deed("sacked", "attacker", -60, "foe", "defender")));
}

TEST_CASE("P5-08 4.2: history.pantheon in order with its starts, bounds, absence and its two checks") {
    P508_REQUIRE_INTEGRATED();
    const wd::Definition* d = p503check::definition(shipped(), "history", "pantheon");
    REQUIRE(d != nullptr);
    const std::vector<std::string> order = {
        "activity_permille", "regard_max", "regard_fade_per_century", "regard_memory_years", "contributions_max",
        "act_threshold", "act_permille", "god_rest_years", "pair_rest_years", "flare_regard", "flare_permille",
        "bless_years", "curse_years", "plenty_growth_permille", "withering_growth_permille", "valour_permille",
        "defeat_permille", "pestilence_toll_permille", "send_hops", "sacred_growth_permille", "profane_years",
        "champion_epithet", "cult_found_permille", "cult_fear_permille", "cult_spread_permille", "affinity_permille",
        "adopt_regard", "adopt_permille", "suppress_permille", "temper_adopt_permille", "temper_suppress_permille",
        "faith_points", "rival_faith_points", "suppress_points", "hide_permille", "sanctum_hops", "creator"};
    REQUIRE(d->record.size() == order.size());
    for (std::size_t i = 0; i < order.size(); ++i) CHECK(d->record.name_at(i) == order[i]);
    // Integration: the shipped (calibrated, 15.2) values; 4.2's starts were regard_fade_per_century 20,
    // act_threshold 100, act_permille 150, god_rest_years 30, pair_rest_years 100, flare_regard 200, ...
    const std::vector<std::pair<const char*, std::int64_t>> starts = {
        {"regard_max", 400}, {"regard_fade_per_century", 8}, {"regard_memory_years", 300}, {"contributions_max", 8},
        {"act_threshold", 70}, {"act_permille", 14}, {"god_rest_years", 20}, {"pair_rest_years", 80},
        {"flare_regard", 130}, {"flare_permille", 100}, {"bless_years", 50}, {"curse_years", 50},
        {"plenty_growth_permille", 800}, {"withering_growth_permille", 500}, {"valour_permille", 50},
        {"defeat_permille", 60}, {"pestilence_toll_permille", 50}, {"send_hops", 3}, {"sacred_growth_permille", 200},
        {"profane_years", 300}, {"cult_found_permille", 400}, {"cult_fear_permille", 700}, {"cult_spread_permille", 20},
        {"adopt_regard", 50}, {"adopt_permille", 15}, {"suppress_permille", 8}, {"faith_points", 60},
        {"rival_faith_points", 40}, {"suppress_points", 120}, {"hide_permille", 800}, {"sanctum_hops", 3}};
    for (const auto& [k, v] : starts) {
        CAPTURE(k);
        CHECK(d->record.get_int(k) == v);
    }
    CHECK(d->record.get_string("champion_epithet") == "Chosen of {god}");
    const wd::Record& a = d->record.find("activity_permille")->as_record();
    CHECK(a.get_int("distant") == 300);
    CHECK(a.get_int("normal") == 1000);
    CHECK(a.get_int("meddling") == 2500);
    const std::string file = "history/pantheon.toml";
    const std::string text = read(data_root() / file);
    refused(file, with_line(text, "regard_max", "0"), {"regard_max"});
    refused(file, with_line(text, "act_permille", "1001"), {"act_permille"});
    refused(file, with_line(text, "send_hops", "9"), {"send_hops"});
    refused(file, with_line(text, "valour_permille", "501"), {"valour_permille"});
    refused(file, with_line(text, "champion_epithet", "\"Chosen\""), {"champion_epithet"});
    refused(file, without(text, "hide_permille"), {"hide_permille"});
    refused(file, with_line(text, "flare_regard", "60"), {"flare_regard 60 is below act_threshold 70"});
    refused(file, with_line(with_line(text, "act_threshold", "500"), "flare_regard", "600"),
            {"act_threshold 500 is above regard_max 400"});
    accepted(file, with_line(text, "act_permille", "0"));
}

TEST_CASE("P5-08 4.2, 4.4, 12: creator_title, the culture forms and the follow-up fields") {
    P508_REQUIRE_INTEGRATED();
    const wd::Definition* ch = p503check::definition(shipped(), "history", "chronology");
    REQUIRE(ch != nullptr);
    CHECK(ch->record.get_string("creator_title") == "the Maker of {plural}");
    // the calibrated values (12.2's and 12.5's starts were 1000 and 3)
    CHECK(p508api::tuning_or(shipped(), "monsters", "deep_stir_permille", -1) == 700);
    CHECK(p508api::tuning_or(shipped(), "alliances", "renew_written_max", -1) == 1);
    CHECK(p508api::tuning_or(shipped(), "figures", "tower_lapse_years", -1) == 200);
    const std::vector<std::pair<const char*, const char*>> human = {
        {"cult_forms", "The Brotherhood of {god}"}, {"temple_forms", "The Temple of {god}"},
        {"sanctum_forms", "The Hidden Sanctum of {god}"}, {"sacred_forms", "The Holy Ground of {god}"},
        {"profane_forms", "The Cursed Ground of {god}"}};
    const wd::Definition* hc = p503check::definition(shipped(), "culture", "human");
    REQUIRE(hc != nullptr);
    const wd::Record& naming = hc->record.find("naming")->as_record();
    for (const auto& [field, first] : human) {
        CAPTURE(field);
        REQUIRE(naming.find(field) != nullptr);
        CHECK(naming.find(field)->as_array().at(0).as_string() == first);
    }
    const std::string file = "cultures/human.toml";
    const std::string text = read(data_root() / file);
    refused(file, with_line(text, "cult_forms", "[\"The Order of {root}\"]"), {"cult_forms"});
    refused(file, with_line(text, "temple_forms", "[]"), {"temple_forms"});
    const std::string chf = "history/chronology.toml";
    refused(chf, with_line(read(data_root() / chf), "creator_title", "\"the Maker\""), {"creator_title"});
}

}  // TEST_SUITE
