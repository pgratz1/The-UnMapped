// Spec P5-04 (docs/specs/p5-04-alliances-trade.md r2) test 18.1 item 1: the race fields origin_allies,
// ally_join_permille and mining_permille with race_origin_allies' messages (one-sided origin included); every
// history.alliances field's bounds and absence; the named checks alliances_spoils, alliances_mine_forms,
// alliances_dispositions, alliances_blocs and alliances_trade with their messages; the seven history.conflict
// fields; the data of game/data and both fixture roots (4.1 to 4.3), the report entries of 8.3 and the cause words
// and renaming kinds of 11.1. The data are p5_04_alliances_data.hpp's (generated from the reference's listings).
//
// Written from the spec before the implementation (test-writer rule): on the 0.25.0 library these cases fail (the
// loader does not know the new fields), which is expected until integration.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../p3_01_history_support.hpp"
#include "p5_03_relations_checks.hpp"
#include "p5_04_alliances_data.hpp"
#include "worldgen/history/data_checks.hpp"

namespace {

namespace wd = worldgen::data;
namespace h = worldgen::history;
using history_data::Files;
using namespace history_test;

const std::string AL = "history/alliances.toml";
const std::string CO = "history/conflict.toml";

Files edited(Files f, const std::string& path, const std::string& from, const std::string& to) {
    std::string& t = f[path];
    const std::size_t at = t.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, from << " not in " << path);
    t.replace(at, from.size(), to);
    return f;
}
Files edited(const std::string& path, const std::string& from, const std::string& to) {
    return edited(p504data::files_026(), path, from, to);
}

Files without(const std::string& path, const std::string& name) {
    Files f = p504data::files_026();
    std::string& t = f[path];
    const std::size_t a = t.find("\n" + name + " = ");
    REQUIRE(a != std::string::npos);
    t.erase(a, t.find('\n', a + 1) - a);
    return f;
}

std::string line(std::string_view name, std::int64_t v) { return std::string(name) + " = " + std::to_string(v); }

const wd::Record& record_of(const wd::ContentSet& c, const char* kind, const char* id) {
    const wd::Definition* d = p503check::definition(c, kind, id);
    REQUIRE(d != nullptr);
    return d->record;
}

void one_failure(const Files& f, const std::string& path, const std::string& check, const std::string& def,
                 const std::string& message) {
    const Load l = load(f);
    INFO(dump(l));
    REQUIRE_FALSE(l.ok());
    REQUIRE(l.errors.size() == 1U);
    CHECK(l.errors[0].path == path);
    CHECK(l.errors[0].message == "check '" + check + "' failed for '" + def + "': " + message);
}

void alliances_failure(const Files& f, const std::string& check, const std::string& message) {
    one_failure(f, AL, check, "history.alliances", message);
}

// The fields whose range ends fail one of the named checks at the shipped values of the others.
bool in_a_named_check(std::string_view n) {
    for (const char* x : {"ally_min_disposition", "ally_full_disposition", "ally_break_disposition", "bloc_share_max",
                          "bloc_lapse_share", "trade_min_disposition", "spoils_floor_permille", "spoils_cap_permille",
                          "spoils_grant_min", "spoils_grant_max"}) {
        if (n == x) return true;
    }
    return false;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-04 4.2: history.alliances loads: 54 int fields in order at the start values, then mine_forms") {
    const Load l = load(p504data::files_026());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    CHECK(h::check_history_data(*l.content).empty());
    const wd::Record& r = record_of(*l.content, "history", "alliances");
    REQUIRE(r.size() == p504data::alliances_fields().size() + 1U);
    REQUIRE(r.size() == 55U);
    for (std::size_t i = 0; i < p504data::alliances_fields().size(); ++i) {
        const p504data::Field& f = p504data::alliances_fields()[i];
        CAPTURE(std::string(f.name));
        CHECK(r.name_at(i) == f.name);
        CHECK(r.get_int(std::string(f.name)) == f.shipped);
    }
    CHECK(r.name_at(54) == "mine_forms");
    const wd::Value* forms = r.find("mine_forms");
    REQUIRE(forms != nullptr);
    REQUIRE(forms->as_array().size() == 2U);
    CHECK(forms->as_array()[0].as_string() == "The Mines of {settlement}");
    CHECK(forms->as_array()[1].as_string() == "The Delvings of {settlement}");
}

TEST_CASE("P5-04 4.2: each history.alliances field required; one step outside its range refused; its ends load") {
    for (const p504data::Field& f : p504data::alliances_fields()) {
        const std::string name(f.name);
        CAPTURE(name);
        const Load gone = load(without(AL, name));
        REQUIRE_FALSE(gone.ok());
        CHECK(dump(gone).find("history.alliances." + name) != std::string::npos);
        for (const std::int64_t v : {f.lo - 1, f.hi + 1}) {
            const Load e = load(edited(AL, line(name, f.shipped), line(name, v)));
            REQUIRE_FALSE(e.ok());
            CHECK(dump(e).find("history.alliances." + name) != std::string::npos);
        }
        if (!in_a_named_check(name)) {
            for (const std::int64_t v : {f.lo, f.hi}) {
                const Load e = load(edited(AL, line(name, f.shipped), line(name, v)));
                CHECK_MESSAGE(e.ok(), dump(e));
            }
        }
    }
    const Load gone = load(without(AL, "mine_forms"));
    REQUIRE_FALSE(gone.ok());
    CHECK(dump(gone).find("history.alliances.mine_forms") != std::string::npos);
    // array(string(64), 1, 4)
    CHECK_FALSE(load(edited(AL, R"(mine_forms = ["The Mines of {settlement}", "The Delvings of {settlement}"])",
                            "mine_forms = []"))
                    .ok());
    CHECK(load(edited(AL, R"(mine_forms = ["The Mines of {settlement}", "The Delvings of {settlement}"])",
                      R"(mine_forms = ["A {settlement}", "B {settlement}", "C {settlement}", "D {settlement}"])"))
              .ok());
    CHECK_FALSE(load(edited(AL, R"(mine_forms = ["The Mines of {settlement}", "The Delvings of {settlement}"])",
                            R"(mine_forms = ["A {settlement}", "B {settlement}", "C {settlement}", "D {settlement}", )"
                            R"("E {settlement}"])"))
                    .ok());
    const std::string long_form = std::string(53, 'x') + "{settlement}";   // 65 bytes
    CHECK_FALSE(load(edited(AL, R"("The Delvings of {settlement}")", "\"" + long_form + "\"")).ok());
    CHECK(load(edited(AL, R"("The Delvings of {settlement}")", "\"" + long_form.substr(1) + "\"")).ok());
}

TEST_CASE("P5-04 4.2: alliances_spoils at and one past its two boundaries") {
    CHECK(load(edited(AL, "spoils_floor_permille = 100", "spoils_floor_permille = 400")).ok());
    alliances_failure(edited(AL, "spoils_floor_permille = 100", "spoils_floor_permille = 401"), "alliances_spoils",
                      "spoils_floor_permille 401 is above spoils_cap_permille 400");
    CHECK(load(edited(AL, "spoils_grant_min = 100", "spoils_grant_min = 950")).ok());
    alliances_failure(edited(AL, "spoils_grant_min = 100", "spoils_grant_min = 951"), "alliances_spoils",
                      "spoils_grant_min 951 is above spoils_grant_max 950");
}

TEST_CASE("P5-04 4.2: alliances_mine_forms: {settlement} exactly once and no other placeholder") {
    const std::string forms = R"(mine_forms = ["The Mines of {settlement}", "The Delvings of {settlement}"])";
    alliances_failure(edited(AL, forms, R"(mine_forms = ["The Mines"])"), "alliances_mine_forms",
                      "mine_forms[0] 'The Mines' must contain {settlement} once");
    alliances_failure(edited(AL, forms, R"(mine_forms = ["The Mines of {settlement}", "{settlement} {settlement}"])"),
                      "alliances_mine_forms", "mine_forms[1] '{settlement} {settlement}' must contain {settlement} once");
    alliances_failure(edited(AL, forms, R"(mine_forms = ["{settlement} of {name}"])"), "alliances_mine_forms",
                      "mine_forms[0] '{settlement} of {name}' must contain {settlement} once");
    CHECK(load(edited(AL, forms, R"(mine_forms = ["{settlement} Delvings"])")).ok());
}

TEST_CASE("P5-04 4.2, 6.8: alliances_dispositions, alliances_blocs and alliances_trade at and one past") {
    CHECK(load(edited(AL, "ally_min_disposition = -300", "ally_min_disposition = 299")).ok());
    alliances_failure(edited(AL, "ally_min_disposition = -300", "ally_min_disposition = 300"),
                      "alliances_dispositions", "ally_min_disposition 300 is not below ally_full_disposition 300");
    CHECK(load(edited(AL, "ally_break_disposition = -350", "ally_break_disposition = -301")).ok());
    alliances_failure(edited(AL, "ally_break_disposition = -350", "ally_break_disposition = -300"),
                      "alliances_dispositions", "ally_break_disposition -300 is not below ally_min_disposition -300");
    CHECK(load(edited(AL, "bloc_share_max = 500", "bloc_share_max = 699")).ok());
    alliances_failure(edited(AL, "bloc_share_max = 500", "bloc_share_max = 700"), "alliances_blocs",
                      "bloc_share_max 700 is not below bloc_lapse_share 700");
    CHECK(load(edited(AL, "bloc_lapse_share = 700", "bloc_lapse_share = 1001")).ok());   // 1001: never
    CHECK(load(edited(AL, "trade_min_disposition = -250", "trade_min_disposition = 299")).ok());
    alliances_failure(edited(AL, "trade_min_disposition = -250", "trade_min_disposition = 300"), "alliances_trade",
                      "trade_min_disposition 300 is not below ally_full_disposition 300");
}

TEST_CASE("P5-04 4.2: the zero tuning of I0 and the alliances-small tuning of 18.3 load") {
    Files z = edited(AL, "ally_hops = 3", "ally_hops = -1");
    z = edited(z, AL, "trade_hops = 3", "trade_hops = -1");
    z = edited(z, AL, "mine_permille = 20", "mine_permille = 0");
    const Load lz = load(z);
    CHECK_MESSAGE(lz.ok(), dump(lz));
    Files s = p504data::files_026();
    for (const auto& [n, v] : std::vector<std::pair<std::string, std::int64_t>>{
             {"form_foe_permille", 1000}, {"form_fear_permille", 1000}, {"form_trade_permille", 1000},
             {"form_friend_permille", 1000}, {"form_max_permille", 1000}, {"defend_permille", 1000},
             {"refusals_to_break", 1}, {"term_years", 10}, {"trade_permille", 1000},
             {"spoils_grant_permille", 500}, {"spoils_grant_min", 300}, {"spoils_grant_max", 700},
             {"withheld_to_break", 1}, {"mine_permille", 1000}, {"mine_settlement_min", 0}}) {
        std::string& t = s[AL];
        const std::size_t a = t.find("\n" + n + " = ");
        REQUIRE(a != std::string::npos);
        t.replace(a + 1, t.find('\n', a + 1) - a - 1, line(n, v));
    }
    const Load ls = load(s);
    CHECK_MESSAGE(ls.ok(), dump(ls));
}

TEST_CASE("P5-04 4.1: the race fields after stances, shipped values, ranges and absence") {
    const Load l = load(p504data::files_026());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    for (const p504data::RaceFields& rf : p504data::race_fields()) {
        CAPTURE(std::string(rf.race));
        const wd::Record& r = record_of(*l.content, "race", std::string(rf.race).c_str());
        REQUIRE(r.size() >= 4U);
        CHECK(r.name_at(r.size() - 4U) == "stances");
        CHECK(r.name_at(r.size() - 3U) == "origin_allies");
        CHECK(r.name_at(r.size() - 2U) == "ally_join_permille");
        CHECK(r.name_at(r.size() - 1U) == "mining_permille");
        CHECK(r.get_int("ally_join_permille") == rf.join);
        CHECK(r.get_int("mining_permille") == rf.mining);
        const std::string path = "races/" + std::string(rf.race) + ".toml";
        for (const char* n : {"origin_allies", "ally_join_permille", "mining_permille"}) {
            const Load gone = load(without(path, n));
            CHECK_FALSE(gone.ok());
        }
        for (const char* n : {"ally_join_permille", "mining_permille"}) {
            const std::int64_t v = std::string(n) == "mining_permille" ? rf.mining : rf.join;
            CHECK_FALSE(load(edited(path, line(n, v), line(n, -1))).ok());
            CHECK_FALSE(load(edited(path, line(n, v), line(n, 1001))).ok());
            CHECK(load(edited(path, line(n, v), line(n, 1000))).ok());
            CHECK(load(edited(path, line(n, v), line(n, 0))).ok());
        }
    }
    const wd::Record& elf = record_of(*l.content, "race", "elf");
    REQUIRE(elf.find("origin_allies") != nullptr);
    REQUIRE(elf.find("origin_allies")->as_array().size() == 1U);
    CHECK(elf.find("origin_allies")->as_array()[0].as_string() == "faerie");
    CHECK(record_of(*l.content, "race", "dwarf").find("origin_allies")->as_array().empty());
    // array(string(32), 0, 8): nine entries refused (the check never reached)
    CHECK_FALSE(load(edited("races/dwarf.toml", "origin_allies = []",
                            R"(origin_allies = ["a", "b", "c", "d", "e", "f", "g", "h", "i"])"))
                    .ok());
}

TEST_CASE("P5-04 4.1: race_origin_allies: not a race, the race itself, twice, one-sided") {
    const std::string elf = "races/elf.toml";
    one_failure(edited(elf, R"(origin_allies = ["faerie"])", R"(origin_allies = ["dragon"])"), elf,
                "race_origin_allies", "race.elf", "origin_allies[0] 'dragon' is not a race");
    one_failure(edited(elf, R"(origin_allies = ["faerie"])", R"(origin_allies = ["elf"])"), elf, "race_origin_allies",
                "race.elf", "origin_allies[0] 'elf' is the race itself");
    one_failure(edited(elf, R"(origin_allies = ["faerie"])", R"(origin_allies = ["faerie", "faerie"])"), elf,
                "race_origin_allies", "race.elf", "origin_allies[1] 'faerie' is listed twice");
    // one-sided: the elves list the dwarves, who do not list them (reading P11: the message names the listed race)
    one_failure(edited(elf, R"(origin_allies = ["faerie"])", R"(origin_allies = ["faerie", "dwarf"])"), elf,
                "race_origin_allies", "race.elf", "origin_allies 'dwarf' is not returned by race 'dwarf'");
    // both sides cleared: no origin alliance, no error
    Files f = edited("races/elf.toml", R"(origin_allies = ["faerie"])", "origin_allies = []");
    f = edited(f, "races/faerie.toml", R"(origin_allies = ["elf"])", "origin_allies = []");
    CHECK(load(f).ok());
}

TEST_CASE("P5-04 4.3: history.conflict: seven fields after capital_captured_salience, required, in range") {
    const Load l = load(p504data::files_026());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const wd::Record& r = record_of(*l.content, "history", "conflict");
    const std::size_t n = p504data::conflict_fields().size();
    REQUIRE(n == 7U);
    REQUIRE(r.size() > n);
    CHECK(r.name_at(r.size() - n - 1U) == "capital_captured_salience");
    for (std::size_t i = 0; i < n; ++i) {
        const p504data::Field& f = p504data::conflict_fields()[i];
        const std::string name(f.name);
        CAPTURE(name);
        CHECK(r.name_at(r.size() - n + i) == f.name);
        CHECK(r.get_int(name) == f.shipped);
        CHECK_FALSE(load(without(CO, name)).ok());
        CHECK_FALSE(load(edited(CO, line(name, f.shipped), line(name, f.lo - 1))).ok());
        CHECK_FALSE(load(edited(CO, line(name, f.shipped), line(name, f.hi + 1))).ok());
        CHECK(load(edited(CO, line(name, f.shipped), line(name, f.lo))).ok());
        CHECK(load(edited(CO, line(name, f.shipped), line(name, f.hi))).ok());
    }
}

TEST_CASE("P5-04 8.3, 11.1: the report entries of kinds 63 to 78; allied_battle in five cause lists and slaughter") {
    const Load l = load(p504data::files_026());
    REQUIRE_MESSAGE(l.ok(), dump(l));
    const h::EventCatalogue cat = h::event_catalogue(*l.content);
    for (const auto& [k, st] : p504data::report_entries()) {
        CAPTURE(k);
        const h::KindDef* d = cat.find(k);
        REQUIRE(d != nullptr);
        CHECK(d->salience == st.first);
        CHECK(d->templ == st.second);
    }
    for (const std::string& k : p504data::cause_kinds()) {
        CAPTURE(k);
        const h::KindDef* d = cat.find(k);
        REQUIRE(d != nullptr);
        CHECK(d->templ.find("allied_battle=") != std::string::npos);
        // the same word as battle='s
        const std::size_t a = d->templ.find("battle=");
        const std::string word = d->templ.substr(a + 7, d->templ.find_first_of(",|", a) - a - 7);
        CHECK(d->templ.find("allied_battle=" + word) != std::string::npos);
    }
    const Files f = p504data::files_026();
    CHECK(f.at("history/renaming.toml").find(R"(kinds = ["battle", "battle_at_pass", "allied_battle"])") !=
          std::string::npos);
}

TEST_CASE("P5-04 4.1 to 4.3: the shipped game/data and both fixture roots hold the 0.26.0 data") {
    for (const std::filesystem::path& root :
         {source_root() / "game" / "data", source_root() / "worldgen" / "tests" / "data" / "geo",
          source_root() / "worldgen" / "tests" / "data" / "geo-small"}) {
        CAPTURE(root.string());
        const Load l = load_disk(root);
        REQUIRE_MESSAGE(l.ok(), dump(l));
        CHECK(h::check_history_data(*l.content).empty());
        const wd::Definition* d = p503check::definition(*l.content, "history", "alliances");
        REQUIRE(d != nullptr);
        CHECK(d->record.size() == 55U);
        for (const p504data::Field& f : p504data::alliances_fields()) CHECK(d->record.find(std::string(f.name)) != nullptr);
        CHECK(record_of(*l.content, "history", "conflict").find("arms_strength_permille") != nullptr);
        CHECK(record_of(*l.content, "race", "elf").find("origin_allies") != nullptr);
    }
}

}  // TEST_SUITE
