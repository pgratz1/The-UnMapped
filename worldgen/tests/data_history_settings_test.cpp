// The world-creation settings of spec P3-01 6.1 and 6.2 in the settings
// file (8.2 tests 7 and 8): every key, its valid values, its errors with
// message and position, the unknown-key message, and the normalization of
// the races.
//
// Written from the spec only. The settings file API is P0-03's
// (worldgen::data::parse_settings); the values are read, and the races
// resolved against the data, through p3_01_history_data_api.hpp, the one
// place that names the fields P3-01 leaves unnamed.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "p3_01_history_data.hpp"
#include "p3_01_history_data_api.hpp"
#include "p3_01_history_support.hpp"
#include "worldgen/data/settings.hpp"

using namespace history_test;

namespace {

constexpr std::string_view PATH = "world-settings.toml";

std::string dump_errors(const std::vector<wd::LoadError>& errors) {
    std::string s;
    for (const wd::LoadError& e : errors) {
        s += e.to_string() + "\n";
    }
    return s;
}

// A settings file with `body` after "[world]\n".
std::string file(std::string_view body) { return "[world]\n" + std::string(body); }

history_api::History parsed(std::string_view body, const wd::ContentSet& content = shipped_content()) {
    const std::string text = file(body);
    const wd::LoadResult<wd::Settings> r = history_api::parse_settings_with_data(PATH, text, content);
    INFO(text << "\n" << dump_errors(r.errors));
    REQUIRE(r.ok());
    return history_api::history_of(*r.value, content);
}

// The one error of a file whose `key = value` line is line 2: its path,
// line 2 and the column of the value (P0-03 9: "at the value's position"),
// unless `column` is given.
wd::LoadError only_error(const std::string& line, bool with_data = false, std::uint32_t column = 0) {
    const std::string text = file(line + "\n");
    const wd::LoadResult<wd::Settings> r = with_data ? history_api::parse_settings_with_data(PATH, text, shipped_content())
                                                     : wd::parse_settings(PATH, text);
    INFO(text << "\n" << dump_errors(r.errors));
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.errors.size() == 1U);
    const wd::LoadError& e = r.errors[0];
    CHECK(e.path == PATH);
    CHECK(e.line == 2U);
    CHECK(e.column == (column != 0 ? column : static_cast<std::uint32_t>(line.find(" = ") + 4)));
    return e;
}

// The 1-based column of the n-th occurrence (0-based) of `needle` in `line`.
std::uint32_t column_of(const std::string& line, std::string_view needle, int n = 0) {
    std::size_t at = line.find(needle);
    for (int i = 0; i < n; ++i) {
        at = line.find(needle, at + 1);
    }
    REQUIRE(at != std::string::npos);
    return static_cast<std::uint32_t>(at + 1);
}

}  // namespace

// --- Defaults (6.1) ----------------------------------------------------------------------

TEST_CASE("P3-01 6.1: without the keys every setting has its default; all races present in race order") {
    CHECK(parsed("") == history_api::defaults());
    CHECK(parsed("seed = 42\nmap_size = 512\n") == history_api::defaults());
    // An absent settings file is the same as an empty one (P0-03 6).
    CHECK(history_api::history_of(wd::Settings{}, shipped_content()) == history_api::defaults());
}

// --- Valid values of each key (6.1, 8.2 test 7) -------------------------------------------

TEST_CASE("P3-01 6.1: world.history_span takes integers 2000 to 12000") {
    for (const char* v : {"2000", "2001", "6000", "11999", "12000", "0x1770", "6_000"}) {
        CAPTURE(std::string_view(v));
        const std::string text(v);
        history_api::History want = history_api::defaults();
        want.history_span = (text == "0x1770" || text == "6_000") ? "6000" : text;  // any TOML integer form
        CHECK(parsed(std::string("history_span = ") + v + "\n") == want);
    }
}

TEST_CASE("P3-01 6.1: world.history_detail, event_intensity, monster_pressure and divine_activity take their words") {
    for (const char* v : {"sparse", "normal", "dense"}) {
        history_api::History want = history_api::defaults();
        want.history_detail = v;
        CHECK(parsed(std::string("history_detail = \"") + v + "\"\n") == want);
    }
    for (const char* v : {"calm", "normal", "turbulent"}) {
        history_api::History want = history_api::defaults();
        want.event_intensity = v;
        CHECK(parsed(std::string("event_intensity = \"") + v + "\"\n") == want);
    }
    for (const char* v : {"low", "normal", "high"}) {
        history_api::History want = history_api::defaults();
        want.monster_pressure = v;
        CHECK(parsed(std::string("monster_pressure = '") + v + "'\n") == want);  // any TOML string form
    }
    for (const char* v : {"distant", "normal", "meddling"}) {
        history_api::History want = history_api::defaults();
        want.divine_activity = v;
        CHECK(parsed(std::string("divine_activity = \"") + v + "\"\n") == want);
    }
}

TEST_CASE("P3-01 6.1: world.civs_per_race takes \"auto\" or an integer 1 to 4") {
    CHECK(parsed("civs_per_race = \"auto\"\n") == history_api::defaults());
    for (const char* v : {"1", "2", "3", "4"}) {
        history_api::History want = history_api::defaults();
        want.civs_per_race = v;
        CHECK(parsed(std::string("civs_per_race = ") + v + "\n") == want);
    }
}

TEST_CASE("P3-01 6.1, 8.2 test 8: world.races is \"all\" or a set of ids, stored in race order") {
    CHECK(parsed("races = \"all\"\n").races == "elf,goblin,dwarf,faerie,human,orc");
    CHECK(parsed("races = [\"elf\"]\n").races == "elf");
    // The order of the ids in the file does not matter (6.1).
    CHECK(parsed("races = [\"orc\", \"elf\"]\n") == parsed("races = [\"elf\", \"orc\"]\n"));
    CHECK(parsed("races = [\"orc\", \"elf\"]\n").races == "elf,orc");
    CHECK(parsed("races = [\"orc\", \"human\", \"dwarf\"]\n").races == "dwarf,human,orc");
    // All six listed in any order equal "all".
    CHECK(parsed("races = [\"orc\", \"human\", \"faerie\", \"dwarf\", \"goblin\", \"elf\"]\n") == parsed("races = \"all\"\n"));
    // Other keys are independent of it.
    const history_api::History h = parsed("races = [\"goblin\"]\nhistory_span = 2000\ncivs_per_race = 4\n");
    CHECK(h.races == "goblin");
    CHECK(h.history_span == "2000");
    CHECK(h.civs_per_race == "4");
}

TEST_CASE("P3-01 6.1, 8.2 test 8: the --races flag text normalizes as the settings-file array does") {
    const wd::ContentSet& c = shipped_content();
    CHECK(history_api::races_from_flag("orc,elf", c) == history_api::races_from_flag("elf,orc", c));
    CHECK(history_api::races_from_flag("orc,elf", c) == std::optional<std::string>("elf,orc"));
    CHECK(history_api::races_from_flag("all", c) == std::optional<std::string>("elf,goblin,dwarf,faerie,human,orc"));
    CHECK(history_api::races_from_flag("orc,human,faerie,dwarf,goblin,elf", c) == history_api::races_from_flag("all", c));
    CHECK(history_api::races_from_flag("dwarf", c) == std::optional<std::string>("dwarf"));
    // The same value as the settings file's array.
    CHECK(history_api::races_from_flag("human,dwarf", c) == std::optional<std::string>(parsed("races = [\"dwarf\", \"human\"]\n").races));
    // The invalid forms of 6.2.
    for (const char* bad : {"", ",", "elf,", ",elf", "elf,,orc", "Elf", "elf orc", "elf, orc", "elf,elf", "ALL"}) {
        CAPTURE(std::string_view(bad));
        CHECK_FALSE(history_api::races_from_flag(bad, c).has_value());
    }
}

TEST_CASE("P3-01 8.2 test 8: \"all\" is every race of the loaded data, in race order") {
    // Data with only the elves and the orcs (the orcs given order 2, so
    // that the orders are dense; their share stays above the elves').
    std::string orc(history_data::ORC_TOML);
    orc.replace(orc.find("order = 6"), 9, "order = 2");
    Files f;
    f["races/elf.toml"] = std::string(history_data::ELF_TOML);
    f["races/orc.toml"] = orc;
    f["cultures/elven.toml"] = history_data::culture_toml(history_data::culture_listings()[1]);
    f["cultures/orcish.toml"] = history_data::culture_toml(history_data::culture_listings()[5]);
    f["history/chronology.toml"] = std::string(history_data::CHRONOLOGY_TOML);
    f["history/engine.toml"] = std::string(history_data::ENGINE_TOML);
    f["history/peoples.toml"] = std::string(history_data::PEOPLES_TOML);
    f["history/conflict.toml"] = std::string(history_data::CONFLICT_TOML);
    f["history/report.toml"] = std::string(history_data::REPORT_TOML);
    f["history/renaming.toml"] = std::string(history_data::RENAMING_TOML);   // P5-12 4: a required fixed id
    f["history/relations.toml"] = std::string(history_data::RELATIONS_TOML);   // P5-03 4.2: a required fixed id
    history_data::add_monster_files(f);   // P5-02 4.1 to 4.3: required kinds and fixed id (dragon and ogre load)
    const Load l = load(f);
    INFO(dump(l));
    REQUIRE(l.ok());
    CHECK(parsed("", *l.content).races == "elf,orc");
    CHECK(parsed("races = \"all\"\n", *l.content).races == "elf,orc");
    CHECK(parsed("races = [\"orc\"]\n", *l.content).races == "orc");
    // With that data, "goblin" is not a race id.
    const wd::LoadResult<wd::Settings> r =
        history_api::parse_settings_with_data(PATH, file("races = [\"goblin\"]\n"), *l.content);
    REQUIRE_FALSE(r.ok());
    REQUIRE(r.errors.size() == 1U);
    CHECK(r.errors[0].message == "'world.races[0]' is \"goblin\", which is not a race id; expected one of elf, orc");
}

// --- Errors (6.2, 8.2 test 7) ------------------------------------------------------------

// <found> of P3-01 6.2 for a TOML literal: an integer as written, a string
// as its text in double quotes, otherwise the kind of value.
namespace {
std::string found(std::string_view literal) {
    if (literal == "true" || literal == "false") {
        return "a boolean";
    }
    if (!literal.empty() && literal[0] == '[') {
        return "an array";
    }
    if (!literal.empty() && literal[0] == '{') {
        return "a table";
    }
    return std::string(literal);  // integers and basic strings without escapes are written as they are shown
}
}  // namespace

TEST_CASE("P3-01 6.2: world.history_span errors") {
    const std::string expected = "; expected an integer from 2000 to 12000";
    for (const char* v : {"1999", "12001", "0", "-6000", "9223372036854775807", "\"6000\"", "\"\"", "true", "[6000]",
                          "{ years = 6000 }"}) {
        CAPTURE(std::string_view(v));
        CHECK(only_error(std::string("history_span = ") + v).message == "'world.history_span' is " + found(v) + expected);
    }
    // A float gets the loader's message only (6.2: no second message).
    CHECK(contains(only_error("history_span = 6000.0").message, "floating-point"));
}

TEST_CASE("P3-01 6.2: world.history_detail, event_intensity, monster_pressure and divine_activity errors") {
    struct Key {
        const char* key;
        const char* expected;
    };
    for (const Key& k : {Key{"history_detail", "; expected sparse, normal or dense"},
                         Key{"event_intensity", "; expected calm, normal or turbulent"},
                         Key{"monster_pressure", "; expected low, normal or high"},
                         Key{"divine_activity", "; expected distant, normal or meddling"}}) {
        for (const char* v : {"\"Normal\"", "\"\"", "\"normal \"", "\"extreme\"", "1", "-2", "true", "[\"normal\"]",
                              "{ v = \"normal\" }"}) {
            CAPTURE(std::string_view(k.key));
            CAPTURE(std::string_view(v));
            CHECK(only_error(std::string(k.key) + " = " + v).message ==
                  "'world." + std::string(k.key) + "' is " + found(v) + k.expected);
        }
        CAPTURE(std::string_view(k.key));
        CHECK(contains(only_error(std::string(k.key) + " = 1.5").message, "floating-point"));
    }
}

TEST_CASE("P3-01 6.2: world.civs_per_race errors") {
    const std::string expected = "; expected \"auto\" or an integer from 1 to 4";
    // Only the string "auto": a number written as a string is not accepted
    // (6.1: "integer, or the string \"auto\"").
    for (const char* v : {"0", "5", "-1", "\"Auto\"", "\"AUTO\"", "\"\"", "\"2\"", "\"all\"", "true", "[2]", "{ n = 2 }"}) {
        CAPTURE(std::string_view(v));
        CHECK(only_error(std::string("civs_per_race = ") + v).message == "'world.civs_per_race' is " + found(v) + expected);
    }
    CHECK(contains(only_error("civs_per_race = 2.0").message, "floating-point"));
}

TEST_CASE("P3-01 6.2: world.races errors") {
    const std::string expected = "; expected \"all\" or an array of race ids";
    for (const char* v : {"\"All\"", "\"none\"", "\"\"", "\"elf\"", "1", "true", "{ elf = true }"}) {
        CAPTURE(std::string_view(v));
        CHECK(only_error(std::string("races = ") + v).message == "'world.races' is " + found(v) + expected);
    }
    // An empty array: at the array.
    CHECK(only_error("races = []").message == "'world.races' is an empty array" + expected);
    // An element that is not a string, or not an id: at the element.
    struct Element {
        const char* line;
        const char* element;  // its literal, located in the line
        int index;
    };
    for (const Element& c : {Element{"races = [\"Elf\"]", "\"Elf\"", 0}, Element{"races = [\"elf\", 1]", "1", 1},
                             Element{"races = [\"\"]", "\"\"", 0}, Element{"races = [\"elf orc\"]", "\"elf orc\"", 0},
                             Element{"races = [[\"elf\"]]", "[\"elf\"]", 0}, Element{"races = [\"elf,orc\"]", "\"elf,orc\"", 0},
                             Element{"races = [\"elf\", true]", "true", 1}, Element{"races = [{ id = \"elf\" }]", "{ id = \"elf\" }", 0}}) {
        CAPTURE(std::string_view(c.line));
        const std::string line(c.line);
        const wd::LoadError e = only_error(line, false, column_of(line, c.element));
        CHECK(e.message == "'world.races[" + std::to_string(c.index) + "]' is " + found(c.element) + "; expected a race id");
    }
    CHECK(contains(only_error("races = [\"elf\", 1.5]", false, 17).message, "floating-point"));  // at the element
    // A repeated id: at the later element.
    {
        const std::string line = "races = [\"elf\", \"orc\", \"elf\"]";
        const wd::LoadError e = only_error(line, false, column_of(line, "\"elf\"", 1));
        CHECK(e.message == "'world.races[2]' is \"elf\", which is already listed");
    }
    // An id with no race definition, checked once the data is loaded, at the
    // element; ids in race order separated by ", ".
    const std::string ids = "elf, goblin, dwarf, faerie, human, orc";
    {
        const std::string line = "races = [\"elf\", \"dragon\"]";
        const wd::LoadError e = only_error(line, true, column_of(line, "\"dragon\""));
        CHECK(e.message == "'world.races[1]' is \"dragon\", which is not a race id; expected one of " + ids);
        // Without the data the same file parses (the check needs the data).
        CHECK(wd::parse_settings(PATH, file(line + "\n")).ok());
    }
    {  // every unknown element is reported
        const std::string text = file("races = [\"dragon\", \"elf\", \"wyrm\"]\n");
        const wd::LoadResult<wd::Settings> r = history_api::parse_settings_with_data(PATH, text, shipped_content());
        INFO(dump_errors(r.errors));
        REQUIRE(r.errors.size() == 2U);
        CHECK(r.errors[0].column == 10U);
        CHECK(r.errors[0].message == "'world.races[0]' is \"dragon\", which is not a race id; expected one of " + ids);
        CHECK(r.errors[1].column == 27U);
        CHECK(r.errors[1].message == "'world.races[2]' is \"wyrm\", which is not a race id; expected one of " + ids);
    }
}

TEST_CASE("P3-01 6.2: world.divine_script is refused until P5, for a value of any type") {
    for (const char* v : {"\"script.lua\"", "\"\"", "\"0123456789abcdef\"", "1", "true", "[\"a\"]", "{ path = \"a\" }"}) {
        CAPTURE(std::string_view(v));
        CHECK(only_error(std::string("divine_script = ") + v).message ==
              "'world.divine_script' is not supported yet: divine scripts need phase P5");
    }
    // A float gets the loader's floating-point message only.
    const wd::LoadError f = only_error("divine_script = 1.5");
    CHECK(contains(f.message, "floating-point"));
    CHECK_FALSE(contains(f.message, "not supported"));
}

TEST_CASE("P3-01 6.2: an unknown [world] key lists the new keys") {
    const wd::LoadError e = only_error("history_length = 6000");
    CHECK(e.message ==
          "unknown key 'world.history_length'; expected seed, seed_phrase, map_size, history_span, history_detail, "
          "civs_per_race, races, event_intensity, monster_pressure, divine_activity or divine_script");
    // The key spelled as P3-00 5 wrote it (a [history] table) is not a settings key (P3-01 6.1).
    const wd::LoadResult<wd::Settings> r = wd::parse_settings(PATH, "[history]\nspan = 6000\n");
    CHECK_FALSE(r.ok());
}

TEST_CASE("P3-01 6.2: every error of a file is reported, sorted (P0-03 9)") {
    const std::string text = file(
        "history_span = 100\nhistory_detail = \"busy\"\ncivs_per_race = 9\nevent_intensity = \"wild\"\n"
        "monster_pressure = \"none\"\ndivine_activity = \"absent\"\ndivine_script = \"x\"\nraces = [\"elf\", \"elf\"]\n");
    const wd::LoadResult<wd::Settings> r = wd::parse_settings(PATH, text);
    INFO(dump_errors(r.errors));
    REQUIRE(r.errors.size() == 8U);
    for (std::size_t i = 0; i < r.errors.size(); ++i) {
        CHECK(r.errors[i].line == static_cast<std::uint32_t>(i + 2));
    }
}
