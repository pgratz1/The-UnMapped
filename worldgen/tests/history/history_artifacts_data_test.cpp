// Spec P5-11 (docs/specs/p5-11-artifacts.md r1) test 16 item 1 on the shipped data root (game/data): the data kind
// artifact (4.1: fifteen definitions in canonical id order with the table's class, title and noun; the numeric fields
// within their bounds and the class checks, whose values 15.2 may calibrate), its checks and grammars through an added
// definition file; the culture field artifact_forms (4.2: 1 to 8 forms, {root} once, {title} at most once); the tuning
// history.artifacts (4.3: the fifteen fields in order and their bounds; the ten shipped occasions' kind, role and bearer
// of 4.4; absence; the occasion checks artifacts_occasion_* through an edited file); history.figures'
// smith_age_permille and smith_age_span_permille and history.pantheon.cult_return_years (appended last); the deeds and
// cause words of 4.5 and the faerie form; the report entries 111 to 116 (8.2). The data root is copied to a scratch
// directory and one file is edited or added per case. Every case starts with P511_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../db_test_support.hpp"
#include "../p3_01_history_support.hpp"
#include "p5_11_api.hpp"
#include "worldgen/history/data_checks.hpp"

namespace {

namespace fs = std::filesystem;
namespace wd = worldgen::data;
using history_test::Load;

fs::path data_root() { return history_test::source_root() / "game" / "data"; }

Load load_edited(const std::string& file, const std::string& text, bool remove = false) {
    static int k = 0;
    static dbtest::TempDir dir("p511-data");
    const fs::path root = dir.path() / ("root-" + std::to_string(++k));
    fs::copy(data_root(), root, fs::copy_options::recursive);
    if (remove) {
        fs::remove(root / file);
    } else {
        std::ofstream out(root / file, std::ios::binary | std::ios::trunc);
        out << text;
    }
    Load l = history_test::load_disk(root);
    fs::remove_all(root);
    return l;
}

const wd::ContentSet& shipped() {
    static const wd::ContentSet c = [] {
        Load l = history_test::load_disk(data_root());
        REQUIRE_MESSAGE(l.ok(), history_test::dump(l));
        return std::move(*l.content);
    }();
    return c;
}

void refused(const std::string& file, const std::string& text, const std::vector<std::string>& words, bool remove = false) {
    const Load l = load_edited(file, text, remove);
    CAPTURE(words.empty() ? std::string() : words[0]);
    const std::vector<wd::LoadError> errs = l.ok() ? worldgen::history::check_history_data(*l.content) : l.errors;
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

std::string artifact_file(const std::string& id, const std::string& cls, const std::string& title,
                          const std::string& noun, std::int64_t battle, std::int64_t hunt) {
    return "[artifact." + id + "]\nclass = \"" + cls + "\"\ntitle = \"" + title + "\"\nnoun = \"" + noun +
           "\"\nweight = 100\nbattle_permille = " + std::to_string(battle) + "\nhunt_permille = " +
           std::to_string(hunt) + "\nclaim_permille = 100\n";
}
const char* const ART_FILE = "artifacts/zztest.toml";

// history/artifacts.toml with one occasion (the scalar fields at in-bound values).
std::string artifacts_file(const std::string& occasion) {
    return "[history.artifacts]\noccasions = [" + occasion +
           "]\nforge_chance_permille = 2000\nrealm_rest_years = 50\nworld_max = 40\ndeep_metal_depth = 3\n"
           "deep_metal_permille = 30\nforce_max_permille = 150\nhunt_max_permille = 200\nclaim_max_permille = 1000\n"
           "burial_permille = 300\nbattle_take_permille = 500\nsack_take_permille = 700\ncapture_take_permille = 500\n"
           "raid_take_permille = 30\nhoard_hunt_permille = 50\n";
}
std::string occasion(const std::string& kind, const std::string& role, const std::string& bearer,
                     const std::string& classes = "{ arms = 1, armour = 0, regalia = 0, relic = 0, staff = 0 }") {
    return "{ kind = \"" + kind + "\", role = \"" + role + "\", chance_permille = 100, bearer = \"" + bearer +
           "\", classes = " + classes + " }";
}

const wd::Definition& history_def(const std::string& id) {
    const wd::Definition* d = shipped().find("history", id);
    REQUIRE_MESSAGE(d != nullptr, "history." << id << " absent");
    return *d;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-11 4.1: the fifteen shipped kinds in canonical id order, their classes, titles and nouns") {
    P511_REQUIRE_INTEGRATED();
    struct K {
        const char *id, *cls, *title, *noun;
    };
    static const K want[15] = {{"axe", "arms", "Axe", "axe"},           {"chalice", "relic", "Chalice", "chalice"},
                               {"crown", "regalia", "Crown", "crown"},  {"hammer", "arms", "Hammer", "hammer"},
                               {"hauberk", "armour", "Hauberk", "hauberk"}, {"helm", "armour", "Helm", "helm"},
                               {"idol", "relic", "Idol", "idol"},       {"orb", "staff", "Orb", "orb"},
                               {"reliquary", "relic", "Reliquary", "reliquary"}, {"ring", "regalia", "Ring", "ring"},
                               {"sceptre", "regalia", "Sceptre", "sceptre"}, {"shield", "armour", "Shield", "shield"},
                               {"spear", "arms", "Spear", "spear"},     {"staff", "staff", "Staff", "staff"},
                               {"sword", "arms", "Sword", "sword"}};
    const wd::KindContent* k = shipped().find_kind("artifact");
    REQUIRE(k != nullptr);
    REQUIRE(k->definitions.size() == 15U);
    for (std::size_t i = 0; i < 15U; ++i) {
        const wd::Definition& d = k->definitions[i];
        CAPTURE(d.id);
        CHECK(d.id == want[i].id);
        CHECK(d.record.get_string("class") == want[i].cls);
        CHECK(d.record.get_string("title") == want[i].title);
        CHECK(d.record.get_string("noun") == want[i].noun);
        const std::int64_t w = d.record.get_int("weight"), b = d.record.get_int("battle_permille"),
                           h = d.record.get_int("hunt_permille"), c = d.record.get_int("claim_permille");
        CHECK((1 <= w && w <= 1000));
        CHECK((0 <= b && b <= 500));
        CHECK((0 <= h && h <= 1000));
        CHECK((0 <= c && c <= 1000));
        const std::string cls = want[i].cls;
        if (cls != "arms" && cls != "armour") CHECK(b == 0);
        if (cls == "regalia" || cls == "relic") CHECK(h == 0);
    }
}

TEST_CASE("P5-11 4.1: an added artifact definition: the class checks and the grammars") {
    P511_REQUIRE_INTEGRATED();
    refused(ART_FILE, artifact_file("zztest", "relic", "Cup", "cup", 10, 0), {"battle_permille needs arms or armour"});
    refused(ART_FILE, artifact_file("zztest", "regalia", "Cup", "cup", 0, 10), {"hunt_permille"});
    refused(ART_FILE, artifact_file("zztest", "arms", "cup", "cup", 0, 0), {"title"});
    refused(ART_FILE, artifact_file("zztest", "arms", "Cup", "Cup", 0, 0), {"noun"});
    refused(ART_FILE, artifact_file("zztest", "arms", "Cup", "cup  of", 0, 0), {"noun"});
    refused(ART_FILE, artifact_file("zztest", "bow", "Bow", "bow", 0, 0), {"class"});
    refused(ART_FILE, artifact_file("zztest", "arms", "Cup", "cup", 501, 0), {"battle_permille"});
    const Load ok = load_edited(ART_FILE, artifact_file("zztest", "arms", "Great-Axe", "great axe", 20, 20));
    CHECK_MESSAGE(ok.ok(), history_test::dump(ok));
}

TEST_CASE("P5-11 4.2: artifact_forms of every culture") {
    P511_REQUIRE_INTEGRATED();
    const wd::KindContent* k = shipped().find_kind("culture");
    REQUIRE(k != nullptr);
    for (const wd::Definition& d : k->definitions) {
        CAPTURE(d.id);
        const auto& forms = d.record.get_record("naming").get_array("artifact_forms");
        CHECK((1U <= forms.size() && forms.size() <= 8U));
        for (const wd::Value& f : forms) {
            const std::string s = f.as_string();
            CAPTURE(s);
            CHECK(s.size() <= 48U);
            const std::size_t r = s.find("{root}");
            CHECK(r != std::string::npos);
            CHECK(s.find("{root}", r + 1U) == std::string::npos);
            const std::size_t t = s.find("{title}");
            if (t != std::string::npos) CHECK(s.find("{title}", t + 1U) == std::string::npos);
        }
        if (d.id == "faerie") {   // 4.5 (critic F5)
            const auto& cf = d.record.get_record("naming").get_array("cult_forms");
            bool silver = false, hidden = false;
            for (const wd::Value& f : cf) {
                silver = silver || f.as_string() == "The Silver Ring of {god}";
                hidden = hidden || f.as_string() == "The Hidden Ring of {god}";
            }
            CHECK(silver);
            CHECK_FALSE(hidden);
        }
    }
}

TEST_CASE("P5-11 4.3, 4.4: history.artifacts: the fields in order, bounds and the ten shipped occasions") {
    P511_REQUIRE_INTEGRATED();
    const wd::Record& r = history_def("artifacts").record;
    static const char* const names[15] = {"occasions", "forge_chance_permille", "realm_rest_years", "world_max",
                                          "deep_metal_depth", "deep_metal_permille", "force_max_permille",
                                          "hunt_max_permille", "claim_max_permille", "burial_permille",
                                          "battle_take_permille", "sack_take_permille", "capture_take_permille",
                                          "raid_take_permille", "hoard_hunt_permille"};
    static const std::int64_t hi[15] = {0, 4000, 10000, 1000, 3, 500, 1000, 2000, 4000, 1000, 1000, 1000, 1000, 1000, 1000};
    REQUIRE(r.size() == 15U);
    for (std::size_t i = 0; i < 15U; ++i) {
        CAPTURE(names[i]);
        CHECK(r.name_at(i) == names[i]);
        if (i == 0U) continue;
        const std::int64_t v = r.value_at(i).as_int();
        CHECK((v >= (i == 4U ? 1 : 0) && v <= hi[i]));
    }
    struct O {
        const char *kind, *role, *bearer;
    };
    static const O want[10] = {{"war_declared", "defender", "general"},
                               {"war_declared", "attacker", "general"},
                               {"sacked", "defender", "realm"},
                               {"civ_conquered", "conqueror", "realm"},
                               {"succession_disputed", "civilization", "realm"},
                               {"hero_rose", "civilization", "figure"},
                               {"settlement_overrun", "civilization", "realm"},
                               {"deep_woken", "civilization", "realm"},
                               {"divine_champion", "civilization", "figure"},
                               {"cult_founded", "civilization", "site"}};
    const auto& occ = r.get_array("occasions");
    REQUIRE(occ.size() == 10U);
    for (std::size_t i = 0; i < 10U; ++i) {
        const wd::Record& o = occ[i].as_record();
        CAPTURE(i);
        CHECK(o.get_string("kind") == want[i].kind);
        CHECK(o.get_string("role") == want[i].role);
        CHECK(o.get_string("bearer") == want[i].bearer);
        CHECK((0 <= o.get_int("chance_permille") && o.get_int("chance_permille") <= 1000));
    }
}

TEST_CASE("P5-11 4.3, 4.4: history.artifacts: absence and the occasion checks") {
    P511_REQUIRE_INTEGRATED();
    const char* const f = "history/artifacts.toml";
    refused(f, "", {"artifacts"}, true);
    refused(f, artifacts_file(occasion("nonesuch", "x", "realm")), {"occasions[0]: kind nonesuch is unknown"});
    refused(f, artifacts_file(occasion("sacked", "settlement", "realm")),
            {"occasions[0]: role settlement of sacked is not a civilization role"});
    refused(f, artifacts_file(occasion("sacked", "defender", "general")), {"occasions[0]: bearer general needs war_declared"});
    refused(f, artifacts_file(occasion("sacked", "defender", "figure")),
            {"occasions[0]: bearer figure needs a figure role in sacked"});
    refused(f, artifacts_file(occasion("sacked", "defender", "site")), {"occasions[0]: bearer site needs a site role in sacked"});
    refused(f, artifacts_file(occasion("sacked", "defender", "realm",
                                       "{ arms = 0, armour = 0, regalia = 0, relic = 0, staff = 0 }")),
            {"occasions[0]"});
    const Load ok = load_edited(f, artifacts_file(occasion("hero_rose", "civilization", "figure")));
    CHECK_MESSAGE(ok.ok(), history_test::dump(ok));
    const Load none = load_edited(f, artifacts_file(""));   // 4.3: occasions may be empty (the zero tuning)
    CHECK_MESSAGE(none.ok(), history_test::dump(none));
}

TEST_CASE("P5-11 4.3, 12.2: the smith ages and cult_return_years, appended last") {
    P511_REQUIRE_INTEGRATED();
    const wd::Record& f = history_def("figures").record;
    REQUIRE(f.size() >= 2U);
    CHECK(f.name_at(f.size() - 2U) == "smith_age_permille");
    CHECK(f.name_at(f.size() - 1U) == "smith_age_span_permille");
    for (const char* k : {"smith_age_permille", "smith_age_span_permille"}) {
        CHECK((0 <= f.get_int(k) && f.get_int(k) <= 1000));
    }
    const wd::Record& p = history_def("pantheon").record;
    CHECK(p.name_at(p.size() - 1U) == "cult_return_years");
    CHECK((0 <= p.get_int("cult_return_years") && p.get_int("cult_return_years") <= 10000));
}

TEST_CASE("P5-11 4.5, 8.2: the deeds, the act cause words and the report entries 111 to 116") {
    P511_REQUIRE_INTEGRATED();
    const auto has_deed = [](const std::string& god, const std::string& kind, const std::string& role, std::int64_t points) {
        const wd::Definition* d = shipped().find("god", god);
        REQUIRE(d != nullptr);
        for (const wd::Value& x : d->record.get_array("deeds")) {
            const wd::Record& r = x.as_record();
            if (r.get_string("kind") == kind && r.get_string("role") == role && r.get_int("points") == points &&
                r.get_string("who") == "any") {
                return true;
            }
        }
        return false;
    };
    CHECK(has_deed("craft", "artifact_made", "maker_realm", 20));
    CHECK(has_deed("war", "artifact_taken", "taker", 10));
    std::map<std::string, std::pair<std::int64_t, std::string>> rep;
    for (const wd::Value& e : history_def("report").record.get_array("events")) {
        const wd::Record& r = e.as_record();
        rep[r.get_string("kind")] = {r.get_int("salience"), r.get_string("template")};
    }
    for (const char* k : {"divine_bless", "divine_curse", "divine_champion", "divine_monster", "divine_mark"}) {
        CAPTURE(k);
        CHECK(history_test::contains(rep[k].second, "artifact_made=a great work forged"));
        CHECK(history_test::contains(rep[k].second, "artifact_taken=a treasure taken"));
    }
    const std::vector<std::pair<std::string, std::int64_t>> sal = {{"artifact_made", 45}, {"artifact_taken", 40},
                                                                   {"artifact_lost", 35}, {"artifact_hoarded", 50},
                                                                   {"artifact_recovered", 50}, {"artifact_buried", 25}};
    for (const auto& [k, s] : sal) {
        CAPTURE(k);
        REQUIRE(rep.count(k) == 1U);
        CHECK(rep[k].first == s);
    }
    CHECK(rep["artifact_buried"].second ==
          "{artifact}, {artifact:kind}, was laid in the grave of {figure} of {civilization}.");
    CHECK(history_test::contains(rep["artifact_made"].second,
                                 "{maker:all}{god:titled}{maker_kind?| forged| forged| wrought| bestowed} {artifact}, "
                                 "{artifact:kind},{maker_kind?| for| for| for| upon} {civilization}{maker_kind?|| at the "
                                 "forges of ||}{maker_realm:all}."));
    CHECK(history_test::contains(rep["artifact_recovered"].second,
                                 "{civilization} recovered {artifact}, {artifact:kind}{how?|, from the hoard of |, from }"
                                 "{monster:all}{from:all}."));
}

}  // TEST_SUITE
