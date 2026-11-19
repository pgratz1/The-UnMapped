// worldgen-cli generate with the world-creation settings of spec P3-01 6
// (8.2 tests 6 and 7) and the worlds of the merge (8.3 test 10): every flag
// with its valid values and its usage errors (exit 2, P0-04 4.1), every
// settings-file key with its input errors (exit 3, P0-03 9), and the
// --until none worlds of the three data roots, whose only change is
// meta.content_hash and meta.generator_version.
//
// Written from the specs only. P3-01 writes no meta key and no table (6.3,
// 7), so a valid setting must not change a byte of the world: each accepted
// value is checked against the world generated without it.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cli_test_support.hpp"
#include "p3_01_history_data.hpp"
#include "p3_01_history_support.hpp"
#include "worldgen/core/sha256.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/schema.hpp"
#include "worldgen/version.hpp"

namespace {

using namespace clitest;

constexpr std::string_view GENERATE_HINT = "worldgen-cli: run 'worldgen-cli generate --help' for usage\n";

// generate --seed 0 --until none into files/<name> with extra args; REQUIREs
// exit 0 and returns the file's SHA-256.
std::string world_hash(const Sandbox& sb, const std::string& name, const std::vector<std::string>& args,
                       bool until_none = true) {
    std::vector<std::string> full = {"generate", "--seed", "0", "--out", u8(sb.file(name))};
    if (until_none) {
        full.insert(full.end(), {"--until", "none"});
    }
    full.insert(full.end(), args.begin(), args.end());
    const ProcessResult r = sb.cli(full);
    INFO("args: ", name, " stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    return file_sha256_hex(sb.file(name));
}

std::string joined(const std::vector<std::string>& args) {
    std::string s;
    for (const std::string& a : args) {
        s += "[" + a + "] ";
    }
    return s;
}

// generate with args that must be a usage error with exactly `message`
// (P0-04 4.1: the error line, then the hint line), nothing written.
void usage(const Sandbox& sb, const std::vector<std::string>& args, const std::string& message) {
    const fs::path out = sb.file("refused.db");
    std::vector<std::string> full = {"generate", "--seed", "0", "--until", "none", "--out", u8(out)};
    full.insert(full.end(), args.begin(), args.end());
    const ProcessResult r = sb.cli(full);
    INFO("args: ", joined(args), " stderr: ", r.err);
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.out.empty());
    CHECK(r.err == "worldgen-cli: error: " + message + "\n" + std::string(GENERATE_HINT));
    CHECK_FALSE(file_exists(out));
}

// The same where only the exit code and the error prefix are specified
// (P0-04 4.1: missing value, repeated option).
void usage_any(const Sandbox& sb, const std::vector<std::string>& args) {
    const fs::path out = sb.file("refused.db");
    std::vector<std::string> full = {"generate", "--seed", "0", "--until", "none", "--out", u8(out)};
    full.insert(full.end(), args.begin(), args.end());
    const ProcessResult r = sb.cli(full);
    INFO("args: ", joined(args), " stderr: ", r.err);
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(starts_with(r.err, "worldgen-cli: error: "));
    CHECK_FALSE(file_exists(out));
}

// generate --settings <file with text>: exit 3, the P0-03 error lines after
// the first line; returns those lines.
std::vector<std::string> input_error(const Sandbox& sb, const std::string& name, const std::string& text) {
    const fs::path settings = sb.file(name);
    write_text(settings, text);
    const fs::path out = sb.file("refused.db");
    const ProcessResult r = sb.cli({"generate", "--seed", "0", "--until", "none", "--settings", u8(settings), "--out", u8(out)});
    INFO("settings: ", text, " stderr: ", r.err);
    CHECK(r.exit_code == EXIT_INPUT);
    CHECK(r.out.empty());
    CHECK_FALSE(file_exists(out));
    std::vector<std::string> lines = lines_of(r.err);
    REQUIRE(lines.size() >= 2U);
    CHECK(starts_with(lines[0], "worldgen-cli: error: "));
    lines.erase(lines.begin());
    return lines;
}

}  // namespace

// --- 8.2 test 6: flags ----------------------------------------------------------------------

TEST_CASE("cli P3-01 6.1, 8.2 test 6: every valid flag value is accepted and changes no byte of the world") {
    Sandbox sb;
    const std::string base = world_hash(sb, "base.db", {});
    const std::vector<std::vector<std::string>> cases = {
        {"--history-span", "2000"},       {"--history-span", "6000"},      {"--history-span", "12000"},
        {"--history-span", "7777"},       {"--history-detail", "sparse"},  {"--history-detail", "normal"},
        {"--history-detail", "dense"},    {"--civs-per-race", "auto"},     {"--civs-per-race", "1"},
        {"--civs-per-race", "2"},         {"--civs-per-race", "3"},        {"--civs-per-race", "4"},
        {"--races", "all"},               {"--races", "elf"},              {"--races", "elf,orc"},
        {"--races", "orc,elf"},           {"--races", "orc,human,faerie,dwarf,goblin,elf"},
        {"--event-intensity", "calm"},    {"--event-intensity", "normal"}, {"--event-intensity", "turbulent"},
        {"--monster-pressure", "low"},    {"--monster-pressure", "normal"}, {"--monster-pressure", "high"},
        {"--divine-activity", "distant"}, {"--divine-activity", "normal"}, {"--divine-activity", "meddling"},
        {"--history-span", "2000", "--history-detail", "dense", "--civs-per-race", "4", "--races", "goblin,orc",
         "--event-intensity", "turbulent", "--monster-pressure", "high", "--divine-activity", "meddling"},
    };
    int k = 0;
    for (const auto& args : cases) {
        CAPTURE(joined(args));
        CHECK(world_hash(sb, "w" + std::to_string(k++) + ".db", args) == base);
    }
}

TEST_CASE("cli P3-01 6.2: --history-span errors") {
    Sandbox sb;
    for (const char* v : {"1999", "12001", "0", "-6000", "+6000", "06000", "6000.0", "6e3", "0x1770", " 6000", "6000 ", "",
                          "abc", "6_000", "18446744073709551616"}) {
        CAPTURE(v);
        usage(sb, {"--history-span", v},
              std::string("invalid --history-span '") + v + "'; expected an integer from 2000 to 12000");
    }
}

TEST_CASE("cli P3-01 6.2: --history-detail, --event-intensity, --monster-pressure, --divine-activity errors") {
    Sandbox sb;
    struct Flag {
        const char* name;
        const char* expected;
    };
    for (const Flag& f : {Flag{"history-detail", "sparse, normal or dense"}, Flag{"event-intensity", "calm, normal or turbulent"},
                          Flag{"monster-pressure", "low, normal or high"}, Flag{"divine-activity", "distant, normal or meddling"}}) {
        for (const char* v : {"Normal", "NORMAL", "", "normal ", "extreme", "1"}) {
            CAPTURE(f.name);
            CAPTURE(v);
            usage(sb, {std::string("--") + f.name, v},
                  std::string("invalid --") + f.name + " '" + v + "'; expected " + f.expected);
        }
    }
    // Each flag's words are its own.
    usage(sb, {"--history-detail", "calm"}, "invalid --history-detail 'calm'; expected sparse, normal or dense");
    usage(sb, {"--monster-pressure", "dense"}, "invalid --monster-pressure 'dense'; expected low, normal or high");
}

TEST_CASE("cli P3-01 6.2: --civs-per-race errors") {
    Sandbox sb;
    for (const char* v : {"0", "5", "-1", "+1", "01", "1.0", "Auto", "AUTO", "", "two", "all"}) {
        CAPTURE(v);
        usage(sb, {"--civs-per-race", v}, std::string("invalid --civs-per-race '") + v + "'; expected auto or an integer from 1 to 4");
    }
}

TEST_CASE("cli P3-01 6.2: --races errors: empty list, empty item, not an id, repeated id") {
    Sandbox sb;
    for (const char* v : {"", ",", "elf,", ",elf", "elf,,orc", "Elf", "elf orc", "elf, orc", "1elf", "elf;orc", "elf,elf",
                          "orc,elf,orc", "ALL"}) {
        CAPTURE(v);
        usage(sb, {"--races", v}, std::string("invalid --races '") + v + "'; expected all or a comma-separated list of race ids");
    }
}

TEST_CASE("cli P3-01 6.2: --races with an id that has no race definition (checked after the data is loaded)") {
    Sandbox sb;
    // "<ids of the loaded races in race order, comma separated>": the spec
    // does not say whether a space follows each comma (ambiguity reported);
    // both are accepted, the race order is required.
    for (const char* id : {"dragon", "elves"}) {
        for (const std::string& given : {std::string(id), "elf," + std::string(id)}) {
            CAPTURE(given);
            const fs::path out = sb.file("refused.db");
            const ProcessResult r = sb.cli({"generate", "--seed", "0", "--until", "none", "--races", given, "--out", u8(out)});
            CHECK(r.exit_code == EXIT_USAGE);
            CHECK(r.out.empty());
            CHECK_FALSE(file_exists(out));
            const std::string head = "worldgen-cli: error: unknown race '" + std::string(id) + "' in --races; expected all or ids from: ";
            CHECK((r.err == head + "elf,goblin,dwarf,faerie,human,orc\n" + std::string(GENERATE_HINT) ||
                   r.err == head + "elf, goblin, dwarf, faerie, human, orc\n" + std::string(GENERATE_HINT)));
        }
    }
}

TEST_CASE("cli P3-01 6.2: --divine-script is always refused") {
    Sandbox sb;
    const fs::path script = sb.file("script.txt");
    write_text(script, "nothing\n");
    const std::string message = "--divine-script is not supported yet: divine scripts need phase P5";
    usage(sb, {"--divine-script", u8(script)}, message);
    usage(sb, {"--divine-script", u8(sb.file("missing.txt"))}, message);
    usage(sb, {"--divine-script", ""}, message);
    // Also with the test pattern.
    const ProcessResult r = sb.cli({"generate", "--seed", "0", "--test-pattern", "--divine-script", u8(script), "--out",
                                    u8(sb.file("tp.db"))});
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.err == "worldgen-cli: error: " + message + "\n" + std::string(GENERATE_HINT));
}

TEST_CASE("cli P0-04 4.1: a history flag without a value, or given twice, is a usage error") {
    Sandbox sb;
    for (const char* flag : {"--history-span", "--history-detail", "--civs-per-race", "--races", "--event-intensity",
                             "--monster-pressure", "--divine-activity", "--divine-script"}) {
        CAPTURE(flag);
        usage_any(sb, {flag});
    }
    usage_any(sb, {"--history-span", "6000", "--history-span", "6000"});
    usage_any(sb, {"--races", "elf", "--races", "orc"});
    usage_any(sb, {"--civs-per-race", "auto", "--civs-per-race", "auto"});
}

TEST_CASE("cli P3-01 6.2: --test-pattern accepts the history flags and ignores them") {
    Sandbox sb;
    const std::string base = world_hash(sb, "tp.db", {"--test-pattern"}, false);
    CHECK(world_hash(sb, "tp-flags.db",
                     {"--test-pattern", "--history-span", "12000", "--history-detail", "sparse", "--civs-per-race", "1",
                      "--races", "orc", "--event-intensity", "calm", "--monster-pressure", "low", "--divine-activity",
                      "distant"},
                     false) == base);
    // The values are still validated.
    const ProcessResult r =
        sb.cli({"generate", "--seed", "0", "--test-pattern", "--history-span", "100", "--out", u8(sb.file("bad.db"))});
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.err == "worldgen-cli: error: invalid --history-span '100'; expected an integer from 2000 to 12000\n" +
                       std::string(GENERATE_HINT));
}

TEST_CASE("cli P3-01 2: generate --help lists the history flags") {
    Sandbox sb;
    const ProcessResult r = sb.cli({"generate", "--help"});
    REQUIRE(r.exit_code == EXIT_OK);
    for (const char* flag : {"--history-span", "--history-detail", "--civs-per-race", "--races", "--event-intensity",
                             "--monster-pressure", "--divine-activity", "--divine-script"}) {
        CAPTURE(flag);
        CHECK(contains(r.out, flag));
    }
    // --help wins over invalid values (P0-04 amendment 2).
    const ProcessResult h = sb.cli({"generate", "--history-span", "1", "--races", "", "--divine-script", "x", "--help"});
    CHECK(h.exit_code == EXIT_OK);
    CHECK(h.out == r.out);
}

// --- 8.2 test 7: settings-file keys through generate --------------------------------------

TEST_CASE("cli P3-01 6.1, 8.2 test 7: valid settings-file values are accepted and change no byte of the world") {
    Sandbox sb;
    const std::string base = world_hash(sb, "base.db", {});
    const std::vector<std::string> bodies = {
        "history_span = 2000\n",
        "history_span = 12000\nhistory_detail = \"dense\"\n",
        "civs_per_race = \"auto\"\n",
        "civs_per_race = 3\n",
        "races = \"all\"\n",
        "races = [\"orc\", \"elf\"]\n",
        "event_intensity = \"turbulent\"\nmonster_pressure = \"low\"\ndivine_activity = \"meddling\"\n",
    };
    int k = 0;
    for (const std::string& body : bodies) {
        CAPTURE(body);
        const fs::path settings = sb.file("s" + std::to_string(k) + ".toml");
        write_text(settings, "[world]\n" + body);
        CHECK(world_hash(sb, "f" + std::to_string(k++) + ".db", {"--settings", u8(settings)}) == base);
    }
    // A flag beside a file key (flags override key by key, 6.1). P3-01
    // writes neither value into the world, so only acceptance is visible
    // here; which value wins is visible from P3-03 on (meta keys).
    const fs::path settings = sb.file("both.toml");
    write_text(settings, "[world]\nhistory_span = 2000\nraces = [\"elf\"]\n");
    CHECK(world_hash(sb, "both.db", {"--settings", u8(settings), "--history-span", "12000", "--races", "orc"}) == base);
}

TEST_CASE("cli P3-01 6.2, 8.2 test 7: settings-file errors are input errors at the value") {
    Sandbox sb;
    struct Case {
        std::string line;    // line 2 of the file, after [world]
        std::string column;  // of the value
        std::string message;
    };
    const std::vector<Case> cases = {
        {"history_span = 1999", "16", "'world.history_span' is 1999; expected an integer from 2000 to 12000"},
        {"history_span = 12001", "16", "'world.history_span' is 12001; expected an integer from 2000 to 12000"},
        {"civs_per_race = 5", "17", "'world.civs_per_race' is 5; expected \"auto\" or an integer from 1 to 4"},
        {"divine_script = \"x.lua\"", "17", "'world.divine_script' is not supported yet: divine scripts need phase P5"},
        {"races = [\"elf\", \"elf\"]", "17", "'world.races[1]' is \"elf\", which is already listed"},
        {"history = 6000", "11",
         "unknown key 'world.history'; expected seed, seed_phrase, map_size, history_span, history_detail, civs_per_race, "
         "races, event_intensity, monster_pressure, divine_activity or divine_script"},
    };
    int k = 0;
    for (const Case& c : cases) {
        CAPTURE(c.line);
        const std::string name = "bad" + std::to_string(k++) + ".toml";
        const std::vector<std::string> lines = input_error(sb, name, "[world]\n" + c.line + "\n");
        REQUIRE(lines.size() == 1U);
        CHECK(lines[0] == u8(sb.file(name)) + ":2:" + c.column + ": error: " + c.message);
    }
    // The words of the four word keys: message form only.
    for (const auto& [key, expected] : std::vector<std::pair<std::string, std::string>>{
             {"history_detail", "; expected sparse, normal or dense"},
             {"event_intensity", "; expected calm, normal or turbulent"},
             {"monster_pressure", "; expected low, normal or high"},
             {"divine_activity", "; expected distant, normal or meddling"},
             {"races", "; expected \"all\" or an array of race ids"}}) {
        CAPTURE(key);
        const std::string name = "w" + key + ".toml";
        const std::vector<std::string> lines = input_error(sb, name, "[world]\n" + key + " = \"bogus\"\n");
        REQUIRE(lines.size() == 1U);
        const std::string prefix = u8(sb.file(name)) + ":2:" + std::to_string(key.size() + 4) + ": error: 'world." + key + "' is ";
        CHECK(starts_with(lines[0], prefix));
        CHECK(lines[0].substr(lines[0].size() - std::min(lines[0].size(), expected.size())) == expected);
    }
}

TEST_CASE("cli P3-01 6.2: a race id in the settings file with no race definition is an input error at the element") {
    Sandbox sb;
    const std::string name = "dragon.toml";
    const std::vector<std::string> lines = input_error(sb, name, "[world]\nraces = [\"elf\", \"dragon\"]\n");
    REQUIRE(lines.size() == 1U);
    const std::string prefix = u8(sb.file(name)) + ":2:17: error: 'world.races[1]' is \"dragon\", which is not a race id; expected one of ";
    CHECK(starts_with(lines[0], prefix));
    for (const std::string& id : history_data::race_order()) {
        CHECK(contains(lines[0].substr(prefix.size()), id));
    }
}

// --- 8.3 test 10: the --until none worlds of the three data roots ------------------------

TEST_CASE("cli P3-01 8.3 test 10: --until none worlds differ from 0.11.0 only in content_hash and generator_version") {
    const auto golden = history_test::golden_hashes();
    for (const char* root : {"game/data", "worldgen/tests/data/geo", "worldgen/tests/data/geo-small"}) {
        CAPTURE(root);
        Sandbox sb;
        const fs::path data = from_u8(UNMAPPED_TEST_SOURCE_ROOT) / from_u8(root);
        const fs::path world = sb.file("none.db");
        REQUIRE(sb.cli({"generate", "--seed", "0", "--until", "none", "--data", u8(data), "--out", u8(world)}).exit_code ==
                EXIT_OK);
        // The meta rows of a P0 empty world (P0-02 8, P0-04 9) with the two
        // values of the merge; every other table, the schema and the layers
        // as at 0.11.0: no new table, no new meta key (P3-01 6.3, 7).
        p004::MetaRows rows = p004::meta_rows(0);
        for (auto& [key, value] : rows) {
            if (key == "content_hash") {
                value = golden.at(root);
            } else if (key == "generator_version") {
                value = "0.12.0";
            }
        }
        const ProcessResult d = sb.cli({"hash", "--db", u8(world), "--detail"});
        INFO("stderr: ", d.err);
        REQUIRE(d.exit_code == EXIT_OK);
        const std::vector<std::string> lines = lines_of(d.out);
        REQUIRE(lines.size() == 10U);
        CHECK(lines[0] == "file " + file_sha256_hex(world));
        CHECK(lines[1] == "schema " + worldgen::to_hex(p004::p0_schema_hash()));
        CHECK(lines[2] == "table biome " + worldgen::to_hex(p004::empty_biome_table_hash()));
        CHECK(lines[3] == "table climate " + worldgen::to_hex(p004::empty_climate_table_hash()));
        CHECK(lines[4] == "table layer " + worldgen::to_hex(p004::layer_table_hash({})));
        CHECK(lines[5] == "table meta " + worldgen::to_hex(p004::meta_table_hash(rows)));
        CHECK(lines[6] == "table pass " + worldgen::to_hex(p004::empty_pass_table_hash()));
        CHECK(lines[7] == "table region " + worldgen::to_hex(p004::empty_region_table_hash()));
        CHECK(lines[8] == "table region_biome " + worldgen::to_hex(p004::empty_region_biome_table_hash()));
        CHECK(lines[9] == "table region_border " + worldgen::to_hex(p004::empty_region_border_table_hash()));
        // The report: the 0.11.0 text of P0-04 7 with the two values replaced.
        std::string want(p004::REPORT_SEED_0);
        const auto replace_line = [&](const std::string& key, const std::string& value) {
            const std::size_t at = want.find("\n" + key + " = ");
            REQUIRE(at != std::string::npos);
            const std::size_t end = want.find('\n', at + 1);
            want.replace(at + 1, end - at - 1, key + " = " + value);
        };
        replace_line("content_hash", golden.at(root));
        replace_line("generator_version", "0.12.0");
        const ProcessResult rep = sb.cli({"report", "--db", u8(world)});
        REQUIRE(rep.exit_code == EXIT_OK);
        CHECK(rep.out == want);
    }
}

TEST_CASE("cli P3-01 7: the version line names generator 0.12.0 and schema 5") {
    Sandbox sb;
    const ProcessResult v = sb.cli({"--version"});
    REQUIRE(v.exit_code == EXIT_OK);
    CHECK(v.out == std::string("worldgen-cli 0.12.0 schema 5 sqlite ") + worldgen::db::sqlite_library_version() + "\n");
}

TEST_CASE("cli P3-01 7: the shipped data the CLI reads by default has the golden content hash") {
    CHECK(shipped_content_hash() == history_test::golden_hashes().at("game/data"));
}
