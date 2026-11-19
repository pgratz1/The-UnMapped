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

void check_message(const wd::LoadError& e, std::string_view prefix, std::string_view suffix) {
    CAPTURE(e.message);
    CHECK(e.message.substr(0, prefix.size()) == prefix);
    CHECK(e.message.size() >= prefix.size() + suffix.size());
    CHECK(e.message.substr(e.message.size() - std::min(e.message.size(), suffix.size())) == suffix);
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
        CAPTURE(v);
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
    const std::string prefix = "'world.races[0]' is \"goblin\", which is not a race id; expected one of ";
    CHECK(r.errors[0].message.substr(0, prefix.size()) == prefix);
    CHECK(contains(r.errors[0].message.substr(prefix.size()), "elf"));
    CHECK(contains(r.errors[0].message.substr(prefix.size()), "orc"));
    CHECK_FALSE(contains(r.errors[0].message.substr(prefix.size()), "human"));
}

// --- Errors (6.2, 8.2 test 7) ------------------------------------------------------------

TEST_CASE("P3-01 6.2: world.history_span errors") {
    const std::string expected = "; expected an integer from 2000 to 12000";
    for (const char* v : {"1999", "12001", "0", "-6000", "9223372036854775807"}) {
        CAPTURE(v);
        const wd::LoadError e = only_error(std::string("history_span = ") + v);
        CHECK(e.message == "'world.history_span' is " + std::string(v) + expected);
    }
    for (const char* v : {"\"6000\"", "true", "[6000]", "{ years = 6000 }"}) {
        CAPTURE(v);
        check_message(only_error(std::string("history_span = ") + v), "'world.history_span' is ", expected);
    }
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
        for (const char* v : {"\"Normal\"", "\"\"", "\"normal \"", "\"extreme\"", "1", "true", "[\"normal\"]"}) {
            CAPTURE(k.key);
            CAPTURE(v);
            check_message(only_error(std::string(k.key) + " = " + v), "'world." + std::string(k.key) + "' is ", k.expected);
        }
    }
}

TEST_CASE("P3-01 6.2: world.civs_per_race errors") {
    const std::string expected = "; expected \"auto\" or an integer from 1 to 4";
    for (const char* v : {"0", "5", "-1"}) {
        CAPTURE(v);
        CHECK(only_error(std::string("civs_per_race = ") + v).message == "'world.civs_per_race' is " + std::string(v) + expected);
    }
    // Only the string "auto": a number written as a string is not accepted
    // (6.1: "integer, or the string \"auto\"").
    for (const char* v : {"\"Auto\"", "\"AUTO\"", "\"\"", "\"2\"", "\"all\"", "true", "[2]"}) {
        CAPTURE(v);
        check_message(only_error(std::string("civs_per_race = ") + v), "'world.civs_per_race' is ", expected);
    }
    CHECK(contains(only_error("civs_per_race = 2.0").message, "floating-point"));
}

TEST_CASE("P3-01 6.2: world.races errors") {
    const std::string expected = "; expected \"all\" or an array of race ids";
    for (const char* v : {"\"All\"", "\"none\"", "\"\"", "\"elf\"", "1", "true", "{ elf = true }"}) {
        CAPTURE(v);
        check_message(only_error(std::string("races = ") + v), "'world.races' is ", expected);
    }
    // An empty array, and elements that are not ids: P3-01 6.2 gives no
    // message of their own (spec ambiguity reported); an error at the races
    // line naming the key is required.
    for (const char* v : {"[]", "[\"Elf\"]", "[\"elf\", 1]", "[\"\"]", "[\"elf orc\"]", "[[\"elf\"]]", "[\"elf,orc\"]"}) {
        CAPTURE(v);
        const std::string text = file(std::string("races = ") + v + "\n");
        const wd::LoadResult<wd::Settings> r = wd::parse_settings(PATH, text);
        INFO(dump_errors(r.errors));
        REQUIRE_FALSE(r.ok());
        for (const wd::LoadError& e : r.errors) {
            CHECK(e.line == 2U);
            CHECK(e.message.substr(0, 12) == "'world.races");
        }
    }
    // A repeated id: at the later element.
    {
        const std::string line = "races = [\"elf\", \"orc\", \"elf\"]";
        const wd::LoadError e = only_error(line, false, column_of(line, "\"elf\"", 1));
        CHECK(e.message == "'world.races[2]' is \"elf\", which is already listed");
    }
    // An id with no race definition, checked once the data is loaded, at the element.
    {
        const std::string line = "races = [\"elf\", \"dragon\"]";
        const wd::LoadError e = only_error(line, true, column_of(line, "\"dragon\""));
        const std::string prefix = "'world.races[1]' is \"dragon\", which is not a race id; expected one of ";
        CHECK(e.message.substr(0, prefix.size()) == prefix);
        // "<ids>": the spec does not give their order or separator; each
        // loaded id must be named.
        for (const std::string& id : history_data::race_order()) {
            CHECK(contains(e.message.substr(prefix.size()), id));
        }
        // Without the data the same file parses (the check needs the data).
        CHECK(wd::parse_settings(PATH, file(line + "\n")).ok());
    }
}

TEST_CASE("P3-01 6.2: world.divine_script is refused until P5") {
    for (const char* v : {"\"script.lua\"", "\"\"", "\"0123456789abcdef\""}) {
        CAPTURE(v);
        CHECK(only_error(std::string("divine_script = ") + v).message ==
              "'world.divine_script' is not supported yet: divine scripts need phase P5");
    }
    // Any other value is refused too.
    CHECK_FALSE(wd::parse_settings(PATH, file("divine_script = 1\n")).ok());
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
