// worldgen-cli with the chronology stage of spec P3-03: --until chronology
// and the default run (14.1 item 12), --until history and the unknown-stage
// message (4.3), the [chronology] report block and the five table lines of
// hash --detail on real worlds against the golden vectors of
// worldgen/tests/golden/p3-03-chronology.txt (9.1, 9.2, 14.4), the data
// checks of section 6 as input errors (14.1 item 6, also with --until none
// and --test-pattern), test-pattern worlds recording the flags (14.1 item
// 9), every setting value reaching its meta key and flag-over-file
// precedence observed through meta (14.1 item 8, P3-01 8.2 test 7), the
// corrupt cases of 9.1 (exit 3, nothing on stdout), and the two hist-*
// cases of world-hashes.sh (13; their hashes are recorded by the
// validation specialist in p3-worlds.txt, not here).
//
// Written from the spec only. Slow checks (the 1024 world) are in the
// doctest suite "chronology-slow".

#include <doctest/doctest.h>

#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cli_test_support.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/stages/testing.hpp"

#ifndef UNMAPPED_P3_03_GOLDEN_PATH
#error "UNMAPPED_P3_03_GOLDEN_PATH must name worldgen/tests/golden/p3-03-chronology.txt"
#endif
#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

using namespace clitest;

constexpr std::string_view GENERATE_HINT = "worldgen-cli: run 'worldgen-cli generate --help' for usage\n";
const char* const ALL_RACES = "elf,goblin,dwarf,faerie,human,orc";

fs::path source_root() { return fs::path(UNMAPPED_TEST_SOURCE_ROOT); }

// "<case> <item> <rest>" lines of the golden file for one case.
std::vector<std::pair<std::string, std::string>> golden_items(const std::string& name) {
    static const std::string text = read_text(fs::path(UNMAPPED_P3_03_GOLDEN_PATH));
    std::vector<std::pair<std::string, std::string>> out;
    for (const std::string& line : lines_of(text)) {
        if (!starts_with(line, name + " ")) {
            continue;
        }
        const std::string rest = line.substr(name.size() + 1);
        const std::size_t sp = rest.find(' ');
        out.emplace_back(rest.substr(0, sp), sp == std::string::npos ? std::string() : rest.substr(sp + 1));
    }
    REQUIRE_MESSAGE(!out.empty(), "no golden case " << name);
    return out;
}

// The lines "table <t> <hex>" and the [chronology] section of a golden case.
std::vector<std::string> golden_table_lines(const std::string& name) {
    std::vector<std::string> out;
    for (const auto& [item, value] : golden_items(name)) {
        if (item == "table") {
            out.push_back("table " + value);
        }
    }
    return out;
}
std::string golden_report_section(const std::string& name) {
    std::string s = "[chronology]\n";
    for (const auto& [item, value] : golden_items(name)) {
        if (item == "report") {
            s += value + "\n";
        }
    }
    return s;
}

// A section of report text, from its header to the next "[" line.
std::string section(const std::string& text, const std::string& header) {
    const std::size_t at = text.find(header + "\n");
    if (at == std::string::npos) {
        return {};
    }
    const std::size_t end = text.find("\n[", at + 1);
    return text.substr(at, end == std::string::npos ? std::string::npos : end + 1 - at);
}

std::string meta_of(const fs::path& db, const std::string& key) {
    auto r = worldgen::db::WorldReader::open(u8(db));
    REQUIRE_MESSAGE(r.ok(), "cannot open " << u8(db));
    return r.value().meta(key).value_or("<absent>");
}

// A copy of game/data under the sandbox with one text edit.
fs::path edited_data(const Sandbox& sb, const std::string& name, const std::string& file, const std::string& from,
                     const std::string& to) {
    const fs::path dst = sb.file(name);
    fs::copy(source_root() / "game" / "data", dst, fs::copy_options::recursive);
    const fs::path f = dst / fs::path(file);
    std::string text = read_text(f);
    const std::size_t at = text.find(from);
    REQUIRE_MESSAGE(at != std::string::npos, file << " has no '" << from << "'");
    text.replace(at, from.size(), to);
    write_text(f, text);
    return dst;
}

struct DataCase {
    const char* label;
    const char* file;
    std::string from, to;
    std::vector<std::string> messages;
};

std::vector<DataCase> data_cases() {
    const std::string x42(42, 'X');
    std::vector<DataCase> v = {
        {"D1", "races/elf.toml", "  { biome = \"alpine\", weight = -600 },",
         "  { biome = \"alpine\", weight = -600 },\n  { biome = \"lake\", weight = 10 },",
         {"race 'elf' prefers water biome 'lake'"}},
        {"D2", "races/goblin.toml", "  { biome = \"alpine\", weight = -400 },",
         "  { biome = \"alpine\", weight = -400 },\n  { biome = \"blighted_waste\", weight = 10 },",
         {"race 'goblin' prefers biome 'blighted_waste', which no geography stage assigns"}},
        {"D3", "races/dwarf.toml", "culture = \"dwarven\"", "culture = \"elven\"",
         {"race 'dwarf' has culture 'elven' of race 'elf'"}},
        {"D4", "history/chronology.toml", "age_name_form = \"The Age of {plural}\"",
         "age_name_form = \"" + x42 + "{plural}\"",
         {"race 'dwarf' has the age name '" + x42 + "Dwarves', which is longer than 48 bytes",
          "race 'faerie' has the age name '" + x42 + "Faeries', which is longer than 48 bytes",
          "race 'goblin' has the age name '" + x42 + "Goblins', which is longer than 48 bytes"}},
        {"D5", "history/chronology.toml", "age_name_form = \"The Age of {plural}\"", "age_name_form = \"Age {plural} 2\"",
         {}},
    };
    for (const auto& [id, pl] : std::vector<std::pair<std::string, std::string>>{
             {"dwarf", "Dwarves"}, {"elf", "Elves"}, {"faerie", "Faeries"},
             {"goblin", "Goblins"}, {"human", "Humans"}, {"orc", "Orcs"}}) {
        v.back().messages.push_back("race '" + id + "' has the age name 'Age " + pl +
                                    " 2', which is not ASCII letters with single spaces, ' and -");
    }
    return v;
}

// The table and layer lines of hash --detail except table meta.
std::vector<std::string> content_lines(const std::string& detail) {
    std::vector<std::string> out;
    for (const std::string& l : lines_of(detail)) {
        if ((starts_with(l, "table ") && !starts_with(l, "table meta ")) || starts_with(l, "layer ")) {
            out.push_back(l);
        }
    }
    return out;
}

std::string hash_detail(const Sandbox& sb, const fs::path& db) {
    const ProcessResult r = sb.cli({"hash", "--db", u8(db), "--detail"});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    return r.out;
}

std::string report(const Sandbox& sb, const fs::path& db) {
    const ProcessResult r = sb.cli({"report", "--db", u8(db)});
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    return r.out;
}

// Golden world case `name`, generated with `args`: hash --detail's five
// table lines (in ascending-name position) and the [chronology] section.
void check_golden_world(const Sandbox& sb, const std::string& name, const std::vector<std::string>& args) {
    CAPTURE(name);
    std::vector<std::string> full = {"--data", u8(source_root() / "game" / "data"), "--out", u8(sb.file(name + ".db"))};
    full.insert(full.end(), args.begin(), args.end());
    const fs::path db = sb.generate(full);
    const std::string detail = hash_detail(sb, db);
    std::vector<std::string> tables;
    for (const std::string& l : lines_of(detail)) {
        if (starts_with(l, "table ")) {
            tables.push_back(l.substr(6, l.find(' ', 6) - 6));
        }
    }
    CHECK(tables == std::vector<std::string>{"age", "beast", "beast_population", "biome", "civilization", "climate", "culture", "event", "event_cause",
                                             "event_kind", "event_kind_role", "event_kind_value", "event_participant",
                                             "event_value", "god", "layer", "meta", "monster", "monster_kind", "pass", "phase", "race",
                                             "race_biome", "race_relation", "region", "region_biome", "region_border",
                                             "region_control", "region_name", "relation", "settlement",
                                             "settlement_census", "settlement_race", "site"});
    for (const std::string& want : golden_table_lines(name)) {
        CAPTURE(want);
        CHECK(contains(detail, "\n" + want + "\n"));
    }
    const std::string rep = report(sb, db);
    CHECK(section(rep, "[chronology]") == golden_report_section(name));
    // [chronology] follows [geography].
    const std::size_t g = rep.find("[geography]\n");
    const std::size_t c = rep.find("[chronology]\n");
    REQUIRE(g != std::string::npos);
    REQUIRE(c != std::string::npos);
    CHECK(g < c);
    CHECK(rep.find("\n[", g + 1) == c - 1);
    for (const auto& [item, value] : golden_items(name)) {
        if (item == "meta") {
            const std::size_t eq = value.find('=');
            CHECK(meta_of(db, value.substr(0, eq)) == value.substr(eq + 1));
        }
    }
}

}  // namespace

TEST_SUITE("chronology") {

// P3-03 14.1 item 12 held at 0.13.0, when chronology was the last stage; from
// P3-04 (9.5) the default run is --until history, and --until chronology
// stops before it.
TEST_CASE("cli P3-03 4.3, 14.1 item 12: --until chronology ends meta.stages in chronology; the default adds history") {
    Sandbox sb;
    const fs::path a = sb.generate({"--seed", "42", "--map-size", "512", "--out", u8(sb.file("default.db"))});
    const fs::path b =
        sb.generate({"--seed", "42", "--map-size", "512", "--until", "chronology", "--out", u8(sb.file("until.db"))});
    CHECK(read_bytes(a) != read_bytes(b));
    CHECK(meta_of(a, "stages") == "elevation,hydrology,climate,biomes,resources,regions,chronology,history");
    CHECK(meta_of(b, "stages") == "elevation,hydrology,climate,biomes,resources,regions,chronology");
    // --until regions: the five tables exist and are empty; the keys are recorded.
    const fs::path g =
        sb.generate({"--seed", "42", "--map-size", "512", "--until", "regions", "--out", u8(sb.file("geo.db"))});
    CHECK(meta_of(g, "stages") == "elevation,hydrology,climate,biomes,resources,regions");
    const std::string tables = section(report(sb, g), "[tables]");
    for (const char* t : {"age = 0\n", "culture = 0\n", "god = 0\n", "race = 0\n", "race_biome = 0\n"}) {
        CHECK(contains(tables, std::string("\n") + t));
    }
    CHECK(meta_of(g, "param.races") == ALL_RACES);
    CHECK(section(report(sb, g), "[chronology]").empty());
}

TEST_CASE("cli P3-03 4.3, 14.1 item 12: --until history is accepted (P3-04 9.5); an unknown stage lists eight names") {
    Sandbox sb;
    const fs::path out = sb.file("x.db");
    const fs::path h = sb.generate({"--seed", "1", "--map-size", "512", "--until", "history", "--out", u8(sb.file("h.db"))});
    CHECK(meta_of(h, "stages") == "elevation,hydrology,climate,biomes,resources,regions,chronology,history");
    const ProcessResult u = sb.cli({"generate", "--seed", "1", "--until", "chronicle", "--out", u8(out)});
    CHECK(u.exit_code == EXIT_USAGE);
    CHECK(u.err == "worldgen-cli: error: unknown stage 'chronicle' given to --until; expected none or a stage name "
                   "(elevation, hydrology, climate, biomes, resources, regions, chronology, history)\n" +
                       std::string(GENERATE_HINT));
    CHECK_FALSE(file_exists(out));
}

TEST_CASE("cli P3-03 9.1, 9.2, 14.4: [chronology] and hash --detail of the 512 golden worlds") {
    Sandbox sb;
    check_golden_world(sb, "geo-seed-42-512", {"--seed", "42", "--map-size", "512"});
    check_golden_world(sb, "hist-span-2000-512", {"--seed", "7", "--map-size", "512", "--history-span", "2000"});
}

TEST_CASE("cli P3-03 6, 14.1 item 6: each data check is an input error, also with --until none and --test-pattern") {
    Sandbox sb;
    int k = 0;
    for (const DataCase& dc : data_cases()) {
        CAPTURE(std::string_view(dc.label));
        const fs::path data = edited_data(sb, std::string("data-") + dc.label, dc.file, dc.from, dc.to);
        for (const std::vector<std::string>& plan :
             {std::vector<std::string>{}, std::vector<std::string>{"--until", "none"},
              std::vector<std::string>{"--test-pattern"}, std::vector<std::string>{"--until", "regions"}}) {
            const fs::path out = sb.file("bad" + std::to_string(k++) + ".db");
            std::vector<std::string> args = {"generate", "--seed", "1", "--map-size", "512", "--data", u8(data), "--out",
                                             u8(out)};
            args.insert(args.end(), plan.begin(), plan.end());
            const ProcessResult r = sb.cli(args);
            INFO("stderr: ", r.err);
            CHECK(r.exit_code == EXIT_INPUT);
            CHECK(r.out.empty());
            CHECK_FALSE(file_exists(out));
            const std::vector<std::string> lines = lines_of(r.err);
            REQUIRE(lines.size() == dc.messages.size() + 1);
            // P0-04 4.5 with P3-03 6: "invalid data in '<data root>'" (the root as given).
            CHECK(starts_with(lines[0], "worldgen-cli: error: invalid data in '"));
            CHECK(contains(lines[0], u8(data.filename())));
            for (std::size_t i = 0; i < dc.messages.size(); ++i) {
                CHECK(lines[i + 1] == "races: error: " + dc.messages[i]);
            }
        }
    }
}

TEST_CASE("cli P3-03 6 D0: a data root without races is refused by the loader first, whatever the plan") {
    // D0 itself is unreachable through the CLI: an empty required kind is a
    // loader error (exit 3), with one error per culture whose race is gone.
    Sandbox sb;
    const fs::path data = sb.file("data-d0");
    fs::copy(source_root() / "game" / "data", data, fs::copy_options::recursive);
    for (const auto& e : fs::directory_iterator(data / "races")) {
        fs::remove(e.path());
    }
    int k = 0;
    for (const std::vector<std::string>& plan :
         {std::vector<std::string>{}, std::vector<std::string>{"--until", "none"},
          std::vector<std::string>{"--test-pattern"}, std::vector<std::string>{"--until", "regions"}}) {
        const fs::path out = sb.file("d0-" + std::to_string(k++) + ".db");
        std::vector<std::string> args = {"generate", "--seed", "1", "--map-size", "512", "--data", u8(data), "--out", u8(out)};
        args.insert(args.end(), plan.begin(), plan.end());
        const ProcessResult r = sb.cli(args);
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK_FALSE(file_exists(out));
        CHECK(contains(r.err, "required kind 'race' has no definitions in 'races/'"));
        CHECK_FALSE(contains(r.err, "no race is defined"));
        std::size_t culture_errors = 0;
        for (const std::string& l : lines_of(r.err)) {
            culture_errors += starts_with(l, "cultures/") ? 1U : 0U;
        }
        CHECK(culture_errors == 6U);
    }
}

TEST_CASE("cli P3-03 4.3, 14.1 item 9: --test-pattern records the history flags and changes nothing else") {
    Sandbox sb;
    const fs::path plain = sb.generate({"--seed", "42", "--test-pattern", "--out", u8(sb.file("plain.db"))});
    const fs::path flagged = sb.generate({"--seed", "42", "--test-pattern", "--history-span", "2000", "--races", "orc",
                                          "--out", u8(sb.file("flagged.db"))});
    CHECK(meta_of(flagged, "param.history_span") == "2000");
    CHECK(meta_of(flagged, "param.races") == "orc");
    CHECK(meta_of(plain, "param.history_span") == "6000");
    CHECK(meta_of(plain, "param.races") == ALL_RACES);
    CHECK(meta_of(flagged, "stages") == "test_pattern");
    CHECK(content_lines(hash_detail(sb, flagged)) == content_lines(hash_detail(sb, plain)));
    CHECK(read_bytes(flagged) != read_bytes(plain));
}

TEST_CASE("cli P3-03 4.2, 14.1 item 8: each setting value reaches its meta key") {
    Sandbox sb;
    struct Case {
        std::vector<std::string> args;
        std::string key;
        std::string value;
    };
    std::vector<Case> cases = {
        {{}, "param.history_span", "6000"},
        {{}, "param.history_detail", "normal"},
        {{}, "param.civs_per_race", "auto"},
        {{}, "param.races", ALL_RACES},
        {{}, "param.event_intensity", "normal"},
        {{}, "param.monster_pressure", "normal"},
        {{}, "param.divine_activity", "normal"},
        {{}, "param.divine_script", ""},
        {{"--history-span", "2000"}, "param.history_span", "2000"},
        {{"--history-span", "12000"}, "param.history_span", "12000"},
        {{"--history-span", "4321"}, "param.history_span", "4321"},
        {{"--races", "orc,elf"}, "param.races", "elf,orc"},
        {{"--races", "elf,orc"}, "param.races", "elf,orc"},
        {{"--races", "all"}, "param.races", ALL_RACES},
        {{"--races", "human,dwarf,goblin"}, "param.races", "goblin,dwarf,human"},
        {{"--civs-per-race", "auto"}, "param.civs_per_race", "auto"},
    };
    for (const char* v : {"sparse", "normal", "dense"}) {
        cases.push_back({{"--history-detail", v}, "param.history_detail", v});
    }
    for (const char* v : {"1", "2", "3", "4"}) {
        cases.push_back({{"--civs-per-race", v}, "param.civs_per_race", v});
    }
    for (const char* v : {"calm", "normal", "turbulent"}) {
        cases.push_back({{"--event-intensity", v}, "param.event_intensity", v});
    }
    for (const char* v : {"low", "normal", "high"}) {
        cases.push_back({{"--monster-pressure", v}, "param.monster_pressure", v});
    }
    for (const char* v : {"distant", "normal", "meddling"}) {
        cases.push_back({{"--divine-activity", v}, "param.divine_activity", v});
    }
    int k = 0;
    for (const Case& c : cases) {
        std::vector<std::string> args = {"--seed", "0", "--until", "none", "--out", u8(sb.file("m" + std::to_string(k++) + ".db"))};
        args.insert(args.end(), c.args.begin(), c.args.end());
        const fs::path db = sb.generate(args);
        CAPTURE(c.key);
        CAPTURE(c.value);
        CHECK(meta_of(db, c.key) == c.value);
    }
    // The [meta] block prints them, param.divine_script with no trailing space.
    const fs::path db = sb.generate({"--seed", "0", "--until", "none", "--out", u8(sb.file("rep.db"))});
    const std::string meta = section(report(sb, db), "[meta]");
    CHECK(contains(meta, "\nparam.civs_per_race = auto\nparam.divine_activity = normal\nparam.divine_script =\n"
                         "param.event_intensity = normal\nparam.history_detail = normal\nparam.history_span = 6000\n"
                         "param.map_size = 1024\nparam.monster_pressure = normal\nparam.races = " +
                             std::string(ALL_RACES) + "\nschema_version = 10\n"));
}

TEST_CASE("cli P3-03 4.1, P3-01 8.2 test 7: file values are recorded, and a flag wins over its file key") {
    Sandbox sb;
    const fs::path settings = sb.file("settings.toml");
    write_text(settings,
               "[world]\nhistory_span = 3000\nhistory_detail = \"dense\"\ncivs_per_race = 2\nraces = [\"orc\", \"elf\"]\n"
               "event_intensity = \"calm\"\nmonster_pressure = \"high\"\ndivine_activity = \"meddling\"\n");
    const fs::path file_only = sb.generate({"--seed", "0", "--until", "none", "--settings", u8(settings), "--out",
                                            u8(sb.file("file.db"))});
    const std::map<std::string, std::string> from_file = {
        {"param.history_span", "3000"},       {"param.history_detail", "dense"},   {"param.civs_per_race", "2"},
        {"param.races", "elf,orc"},           {"param.event_intensity", "calm"},   {"param.monster_pressure", "high"},
        {"param.divine_activity", "meddling"}, {"param.divine_script", ""}};
    for (const auto& [k, v] : from_file) {
        CAPTURE(k);
        CHECK(meta_of(file_only, k) == v);
    }
    const fs::path both = sb.generate({"--seed", "0", "--until", "none", "--settings", u8(settings), "--history-span",
                                       "4000", "--races", "goblin", "--civs-per-race", "auto", "--event-intensity",
                                       "turbulent", "--out", u8(sb.file("both.db"))});
    std::map<std::string, std::string> want = from_file;
    want["param.history_span"] = "4000";
    want["param.races"] = "goblin";
    want["param.civs_per_race"] = "auto";
    want["param.event_intensity"] = "turbulent";
    for (const auto& [k, v] : want) {
        CAPTURE(k);
        CHECK(meta_of(both, k) == v);
    }
}

TEST_CASE("cli P3-03 9.1, 14.1 item 10: corrupt chronology rows exit 3 with nothing on stdout") {
    Sandbox sb;
    const fs::path base = sb.file("base.db");
    REQUIRE(worldgen::stages::testing::write_fixture_world(u8(base), 9, {}).ok());
    const std::string rows =
        "INSERT INTO race VALUES (1, 'elf', 'Elf', 'Elves', 'Elven', 0, 700, 150, 2, 1, 0, 1),"
        "(6, 'orc', 'Orc', 'Orcs', 'Orcish', 2, 50, 18, 6, 1, 5000, 6);"
        "INSERT INTO age VALUES (1, 1, 0, 4999, 'The Age of Elves'), (2, 6, 5000, 5999, 'The Age of Orcs');";
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"", ""},
        {"UPDATE race SET present = 2 WHERE id = 6", "corrupt table 'race': row 6: present is 2"},
        {"UPDATE age SET start_year = 5001 WHERE id = 2", "corrupt table 'age': row 2: start_year 5001, expected 5000"},
        {"UPDATE age SET end_year = 5998 WHERE id = 2", "corrupt table 'age': the last age ends in 5998, expected 5999"},
        {"DELETE FROM age", "corrupt table 'age': 0 rows for 2 present races"},
    };
    int k = 0;
    for (const auto& [sql, message] : cases) {
        CAPTURE(sql);
        const fs::path db = sb.file("c" + std::to_string(k++) + ".db");
        fs::copy_file(base, db);
        REQUIRE(worldgen::db::testing::execute_on_file(u8(db), rows + sql).ok());
        const ProcessResult r = sb.cli({"report", "--db", u8(db)});
        if (message.empty()) {
            CHECK(r.exit_code == EXIT_OK);
            CHECK(section(r.out, "[chronology]") ==
                  "[chronology]\nraces_present = 2\nawakening_years = 0 5000\nage_years = 5000 1000\n"
                  "age_years_min = 1000\ngods = 0\n");
        } else {
            CHECK(r.exit_code == EXIT_INPUT);
            CHECK(r.out.empty());
            CHECK(r.err == "worldgen-cli: error: " + message + "\n");
        }
    }
}

TEST_CASE("cli P3-03 13: world-hashes.sh lists the two hist-* cases with their arguments and p3-worlds.txt") {
    // The hashes themselves are recorded by the validation specialist
    // (p3-worlds.txt); here only the case list of the script is checked.
    const std::string script = read_text(source_root() / "tools" / "ci" / "world-hashes.sh");
    CHECK(contains(script, "hist-races-elf-orc-1024 hist-span-2000-512"));
    CHECK(contains(script, "--seed 7 --map-size 512 --history-span 2000"));
    CHECK(contains(script, "--seed 3 --races elf,orc"));
    CHECK(contains(script, "p3-worlds.txt"));
}

}  // TEST_SUITE

TEST_SUITE("chronology-slow" * doctest::skip()) {

TEST_CASE("cli P3-03 9.1, 9.2, 14.4: [chronology] and hash --detail of the 1024 golden worlds") {
    Sandbox sb;
    check_golden_world(sb, "hist-races-elf-orc-1024", {"--seed", "3", "--races", "elf,orc"});
    check_golden_world(sb, "geo-seed-1-1024", {"--seed", "1"});
}

}  // TEST_SUITE
