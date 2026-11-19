// worldgen-cli with the history stage of spec P3-04: the default run ends
// in `history` and --until history gives the same bytes (Y1, 16.1 item
// 14), --until chronology leaves the thirteen tables empty (9.5), the
// [history] section and hash --detail's thirteen table lines of golden
// worlds against worldgen/tests/golden/p3-04-engine.txt (11.1, 11.2,
// 16.4), the corrupt cases of 11.1 as exit 3 with nothing on stdout, an
// unknown stage still exits 2, and world-hashes.sh with its fifteen cases
// including hist-dense-1024 (15).
//
// Written from the spec only. Slow checks (the 1024 worlds) are in the
// doctest suite "history-slow".

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cli_test_support.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/db/testing.hpp"

#ifndef UNMAPPED_P3_04_GOLDEN_PATH
#error "UNMAPPED_P3_04_GOLDEN_PATH must name worldgen/tests/golden/p3-04-engine.txt"
#endif
#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

using namespace clitest;

fs::path source_root() { return fs::path(UNMAPPED_TEST_SOURCE_ROOT); }

const char* const ALL_STAGES = "elevation,hydrology,climate,biomes,resources,regions,chronology,history";
const std::vector<std::string> NEW_TABLES = {
    "civilization", "event", "event_cause", "event_kind", "event_kind_role", "event_kind_value", "event_participant",
    "event_value", "phase", "region_control", "relation", "settlement", "settlement_census"};

// "<case> <item> <rest>" lines of the golden file for one world case.
std::vector<std::pair<std::string, std::string>> golden_items(const std::string& name) {
    static const std::string text = read_text(fs::path(UNMAPPED_P3_04_GOLDEN_PATH));
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

std::string run_ok(const Sandbox& sb, const std::vector<std::string>& args) {
    const ProcessResult r = sb.cli(args);
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    return r.out;
}

fs::path generate_case(const Sandbox& sb, const std::string& name, const std::vector<std::string>& args) {
    std::vector<std::string> full = {"--data", u8(source_root() / "game" / "data"), "--out", u8(sb.file(name + ".db"))};
    full.insert(full.end(), args.begin(), args.end());
    return sb.generate(full);
}

void check_golden_db(const Sandbox& sb, const std::string& name, const fs::path& db) {
    CAPTURE(name);
    const std::string detail = run_ok(sb, {"hash", "--db", u8(db), "--detail"});
    std::string rep = "[history]\n";
    std::size_t tables = 0;
    for (const auto& [item, value] : golden_items(name)) {
        if (item == "table") {
            CAPTURE(value);
            CHECK(contains(detail, "\ntable " + value + "\n"));
            ++tables;
        }
        if (item == "report") {
            rep += value + "\n";
        }
    }
    CHECK(tables == 13U);
    const std::string report = run_ok(sb, {"report", "--db", u8(db)});
    CHECK(section(report, "[history]") == rep);
    const std::size_t c = report.find("[chronology]\n");
    const std::size_t h = report.find("[history]\n");
    REQUIRE(c != std::string::npos);
    REQUIRE(h != std::string::npos);
    CHECK(report.find("\n[", c + 1) == h - 1);
}

void check_golden_world(const Sandbox& sb, const std::string& name, const std::vector<std::string>& args) {
    check_golden_db(sb, name, generate_case(sb, name, args));
}

}  // namespace

TEST_SUITE("history") {

// One generated 512 world (seed 42) serves Y1, the golden case geo-seed-42-512 and the corrupt cases, so
// that the default CTest entry stays fast; the other golden worlds are in history-slow.
TEST_CASE("cli P3-04 9.5, 11.1, 16.1 item 14, 16.4 (Y1): default run, --until history, golden seed 42, corrupt tables") {
    Sandbox sb;
    const fs::path a = generate_case(sb, "geo-seed-42-512", {"--seed", "42", "--map-size", "512"});
    const fs::path b = generate_case(sb, "until", {"--seed", "42", "--map-size", "512", "--until", "history"});
    CHECK(read_bytes(a) == read_bytes(b));
    CHECK(meta_of(a, "stages") == ALL_STAGES);
    CHECK(meta_of(a, "history_end_year") == "6000");
    CHECK(meta_of(a, "schema_version") == "7");
    check_golden_db(sb, "geo-seed-42-512", a);
    struct Case {
        const char* sql;
        const char* message;
    };
    const std::vector<Case> cases = {
        {"UPDATE phase SET kind = 7 WHERE id = 2", "corrupt table 'phase': row 2: kind 7"},
        {"UPDATE phase SET end_year = 5998 WHERE id = (SELECT max(id) FROM phase)",
         "corrupt table 'phase': the last phase ends in 5998, expected 5999"},
        {"UPDATE event SET kind = 99 WHERE id = 1", "corrupt table 'event': row 1: kind 99 is not an event kind"},
        {"INSERT INTO event_cause VALUES (2, 5)", "corrupt table 'event_cause': row (2, 5): cause not below event"},
    };
    int k = 0;
    for (const Case& c : cases) {
        CAPTURE(c.sql);
        const fs::path db = sb.file("c" + std::to_string(k++) + ".db");
        fs::copy_file(a, db, fs::copy_options::overwrite_existing);
        REQUIRE(worldgen::db::testing::execute_on_file(u8(db), c.sql).ok());
        const ProcessResult r = sb.cli({"report", "--db", u8(db)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(r.err == std::string("worldgen-cli: error: ") + c.message + "\n");
    }
}

TEST_CASE("cli P3-04 9.5, 16.1 item 14: --until chronology and --until none leave the thirteen tables empty") {
    Sandbox sb;
    for (const char* until : {"chronology", "none"}) {  // --until regions: P3-03's CLI tests
        CAPTURE(until);
        const fs::path db = sb.generate({"--seed", "42", "--map-size", "512", "--history-span", "2500", "--until", until,
                                         "--out", u8(sb.file(std::string(until) + ".db"))});
        const std::string report = run_ok(sb, {"report", "--db", u8(db)});
        const std::string tables = section(report, "[tables]");
        for (const std::string& t : NEW_TABLES) {
            CHECK_MESSAGE(contains(tables, "\n" + t + " = 0\n"), t);
        }
        CHECK(section(report, "[history]").empty());
        CHECK(meta_of(db, "history_end_year") == "2500");
    }
}

TEST_CASE("cli P3-04 9.5: an unknown stage still exits 2") {
    Sandbox sb;
    const ProcessResult r = sb.cli({"generate", "--seed", "1", "--until", "peoples", "--out", u8(sb.file("x.db"))});
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.out.empty());
}

TEST_CASE("cli P3-04 15: world-hashes.sh lists fifteen cases, hist-dense-1024 with --seed 11 --history-detail dense") {
    const std::string script = read_text(source_root() / "tools" / "ci" / "world-hashes.sh");
    CHECK(contains(script, "hist-dense-1024"));
    CHECK(contains(script, "--seed 11 --history-detail dense"));
    // The case lists of the script (P0_CASES, GEO_CASES, HIST_CASES), read from its text so that the
    // test needs no shell on Windows.
    std::vector<std::string> cases;
    for (const char* var : {"P0_CASES='", "GEO_CASES='", "HIST_CASES='"}) {
        const std::size_t at = script.find(var);
        REQUIRE_MESSAGE(at != std::string::npos, var);
        const std::size_t from = at + std::string(var).size();
        const std::string list = script.substr(from, script.find('\'', from) - from);
        std::size_t i = 0;
        while (i < list.size()) {
            const std::size_t sp = list.find(' ', i);
            const std::string w = list.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
            if (!w.empty()) {
                cases.push_back(w);
            }
            if (sp == std::string::npos) {
                break;
            }
            i = sp + 1;
        }
    }
    CHECK(cases.size() == 15U);
    CHECK(std::find(cases.begin(), cases.end(), "hist-dense-1024") != cases.end());
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("cli P3-04 11.1, 11.2, 16.4: [history] and hash --detail of the other 512 golden worlds") {
    Sandbox sb;
    check_golden_world(sb, "geo-seed-0-512", {"--seed", "0", "--map-size", "512"});
    check_golden_world(sb, "geo-phrase-512", {"--seed-phrase", "The Unmapped", "--map-size", "512"});
    check_golden_world(sb, "hist-span-2000-512", {"--seed", "7", "--map-size", "512", "--history-span", "2000"});
}

TEST_CASE("cli P3-04 11.1, 11.2, 16.4: [history] and hash --detail of the 1024 golden worlds") {
    Sandbox sb;
    check_golden_world(sb, "geo-seed-1-1024", {"--seed", "1"});
    check_golden_world(sb, "hist-races-elf-orc-1024", {"--seed", "3", "--races", "elf,orc"});
    check_golden_world(sb, "hist-dense-1024", {"--seed", "11", "--history-detail", "dense"});
    check_golden_world(sb, "example-42-1024", {"--seed", "42"});
}

}  // TEST_SUITE
