// worldgen-cli with the history stage of spec P3-04: the default run ends
// in `history` and --until history gives the same bytes (Y1, 16.1 item
// 14), --until chronology leaves the thirteen tables empty (9.5), the
// [history] section and hash --detail's thirteen table lines of golden
// worlds against worldgen/tests/golden/p3-04-engine.txt (11.1, 11.2,
// 16.4), the corrupt cases of 11.1 as exit 3 with nothing on stdout, an
// unknown stage still exits 2, and world-hashes.sh with its sixteen cases
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
    "event_value", "phase", "region_control", "relation", "settlement", "settlement_census", "site"};

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
    CHECK(meta_of(a, "schema_version") == "14");   // P5-03 16.1
    // The shipped history stage is peoples since 0.15.0: the golden comparison of this world is
    // cli_history_peoples_test.cpp's (p3-05-peoples.txt); p3-04-engine.txt describes stub worlds, which the
    // CLI cannot generate.
    struct Case {
        const char* sql;
        const char* message;
    };
    const std::vector<Case> cases = {
        {"UPDATE phase SET kind = 7 WHERE id = 2", "corrupt table 'phase': row 2: kind 7"},
        {"UPDATE phase SET end_year = 5998 WHERE id = (SELECT max(id) FROM phase)",
         "corrupt table 'phase': the last phase ends in 5998, expected 5999"},
        {"UPDATE event SET kind = 100 WHERE id = 1", "corrupt table 'event': row 1: kind 100 is not an event kind"},
        {"INSERT INTO event_cause VALUES (2, 5)", "corrupt table 'event_cause': row (2, 5): cause not below event"},
    };
    int k = 0;
    for (const Case& c : cases) {
        CAPTURE(std::string_view(c.sql));
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
        CAPTURE(std::string_view(until));
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

TEST_CASE("cli P3-04 15: world-hashes.sh lists sixteen cases, hist-dense-1024 with --seed 11 --history-detail dense") {
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
    CHECK(cases.size() == 16U);
    CHECK(std::find(cases.begin(), cases.end(), "hist-dense-1024") != cases.end());
}

}  // TEST_SUITE

// The history-slow cases that compared shipped worlds with p3-04-engine.txt are retired at 0.15.0 (P3-05:
// the shipped stage is peoples); cli_history_peoples_test.cpp compares the same worlds with p3-05-peoples.txt.

