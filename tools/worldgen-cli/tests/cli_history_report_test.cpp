// worldgen-cli report --history (spec P3-07 section 4, 14.2 items 9 to 13): the report of a default world on
// stdout at both levels with the golden hashes of worldgen/tests/golden/p3-07-report.txt (case
// geo-seed-42-512, written by reference/p3_report_reference.py), --out, --force; the usage errors; worlds
// without history (--until none, regions, chronology, --test-pattern) refused with the exact message of
// section 4; a schema 6 file refused by the reader; no CR byte, and the same bytes whatever TZ and LANG.
//
// Written from the spec only.

#include <doctest/doctest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cli_test_support.hpp"
#include "worldgen/db/testing.hpp"

#ifndef UNMAPPED_P3_07_GOLDEN_PATH
#error "UNMAPPED_P3_07_GOLDEN_PATH must name worldgen/tests/golden/p3-07-report.txt"
#endif
#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

using namespace clitest;

constexpr std::string_view REPORT_HINT = "worldgen-cli: run 'worldgen-cli report --help' for usage\n";

fs::path source_root() { return fs::path(UNMAPPED_TEST_SOURCE_ROOT); }

// The golden report hash of a case at a level ("" when absent).
std::string golden(const std::string& name, const std::string& level) {
    std::ifstream f(fs::path(UNMAPPED_P3_07_GOLDEN_PATH));
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ls(line);
        std::string n, l, h;
        ls >> n >> l >> h;
        if (n == name && l == level) return h;
    }
    return {};
}

std::string sha(std::string_view s) { return worldgen::sha256_hex(Bytes(s.begin(), s.end())); }

fs::path generate(const Sandbox& sb, const std::string& name, std::vector<std::string> args) {
    std::vector<std::string> full = {"--data", u8(source_root() / "game" / "data"), "--out", u8(sb.file(name + ".db"))};
    full.insert(full.end(), args.begin(), args.end());
    return sb.generate(full);
}

std::string no_history(const fs::path& db, const std::string& stages) {
    return "worldgen-cli: error: world file '" + u8(db) + "' has no history (stages: " + stages +
           "); generate it with the history stage\n";
}

}  // namespace

TEST_SUITE("history-report") {

TEST_CASE("cli P3-07 14.2 items 9 and 13: report --history on a default world, --out, --force, no CR, TZ and LANG") {
    Sandbox sb;
    const fs::path db = generate(sb, "geo-seed-42-512", {"--seed", "42", "--map-size", "512"});
    const ProcessResult s = sb.cli({"report", "--db", u8(db), "--history"});
    INFO("stderr: ", s.err);
    REQUIRE(s.exit_code == EXIT_OK);
    CHECK(s.err.empty());
    CHECK(starts_with(s.out, "# worldgen history v7\nlevel = summary\n[settings]\n"));
    CHECK(sha(s.out) == golden("geo-seed-42-512", "summary"));
    CHECK(s.out.find('\r') == std::string::npos);
    const ProcessResult s2 = sb.cli({"report", "--db", u8(db), "--history", "--level", "summary"});
    CHECK(s2.exit_code == EXIT_OK);
    CHECK(s2.out == s.out);
    const ProcessResult f = sb.cli({"report", "--level", "full", "--history", "--db", u8(db)});
    REQUIRE(f.exit_code == EXIT_OK);
    CHECK(starts_with(f.out, "# worldgen history v7\nlevel = full\n"));
    CHECK(sha(f.out) == golden("geo-seed-42-512", "full"));
    CHECK(f.out.find('\r') == std::string::npos);
    // --out: the same bytes, nothing on stdout
    const fs::path out = sb.file("history.txt");
    const ProcessResult o = sb.cli({"report", "--db", u8(db), "--history", "--out", u8(out)});
    CHECK(o.exit_code == EXIT_OK);
    CHECK(o.out.empty());
    CHECK(read_text(out) == s.out);
    // an existing --out without --force: exit 4, the file untouched; with --force: replaced
    const ProcessResult again = sb.cli({"report", "--db", u8(db), "--history", "--level", "full", "--out", u8(out)});
    CHECK(again.exit_code == EXIT_OUTPUT);
    CHECK(again.out.empty());
    CHECK(read_text(out) == s.out);
    const ProcessResult forced =
        sb.cli({"report", "--db", u8(db), "--history", "--level", "full", "--out", u8(out), "--force"});
    CHECK(forced.exit_code == EXIT_OK);
    CHECK(read_text(out) == f.out);
    // --force without --out is accepted and changes nothing
    const ProcessResult nf = sb.cli({"report", "--db", u8(db), "--history", "--force"});
    CHECK(nf.exit_code == EXIT_OK);
    CHECK(nf.out == s.out);
    // the world report is unchanged by the new options' existence
    const ProcessResult w = sb.cli({"report", "--db", u8(db)});
    CHECK(w.exit_code == EXIT_OK);
    CHECK_FALSE(starts_with(w.out, "# worldgen history v7"));
    // environment independence (P0-04 12.2)
    RunOptions env;
    env.env = {{"TZ", std::string("Pacific/Kiritimati")}, {"LANG", std::string("de_DE.UTF-8")},
               {"LC_ALL", std::string("de_DE.UTF-8")}};
    const ProcessResult e = sb.cli({"report", "--db", u8(db), "--history", "--level", "full"}, env);
    CHECK(e.exit_code == EXIT_OK);
    CHECK(e.out == f.out);
}

TEST_CASE("cli P3-07 14.2 item 10: usage errors and the help text") {
    Sandbox sb;
    const fs::path db = generate(sb, "none", {"--seed", "1", "--until", "none"});
    {
        const ProcessResult r = sb.cli({"report", "--db", u8(db), "--level", "full"});
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.out.empty());
        CHECK(r.err == std::string("worldgen-cli: error: --level needs --history\n") + std::string(REPORT_HINT));
    }
    {
        // checked before any file is opened
        const ProcessResult r = sb.cli({"report", "--db", u8(sb.file("missing.db")), "--level", "summary"});
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.err == std::string("worldgen-cli: error: --level needs --history\n") + std::string(REPORT_HINT));
    }
    {
        const ProcessResult r = sb.cli({"report", "--db", u8(db), "--history", "--level", "brief"});
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.out.empty());
        CHECK(r.err == std::string("worldgen-cli: error: invalid --level 'brief'; expected summary or full\n") +
                           std::string(REPORT_HINT));
    }
    {
        const ProcessResult r = sb.cli({"report", "--db", u8(db), "--history", "x"});
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.out.empty());
        CHECK(contains(r.err, std::string(REPORT_HINT)));
    }
    const ProcessResult h = sb.cli({"report", "--help"});
    CHECK(h.exit_code == EXIT_OK);
    CHECK(contains(h.out, "usage: worldgen-cli report --db PATH [--history [--level summary|full]] [--out PATH] "
                          "[--force] [--verbose]\n"));
    CHECK(contains(h.out, "\n  --history           print the history report (format '# worldgen history v7') "
                          "instead of the world report\n"));
    CHECK(contains(h.out, "\n  --level LEVEL       with --history: summary (default) or full\n"));
}

TEST_CASE("cli P3-07 14.2 item 11: worlds without history exit 3 with the message of section 4") {
    Sandbox sb;
    struct Case {
        const char* name;
        std::vector<std::string> args;
        const char* stages;
    };
    const std::vector<Case> cases = {
        {"none", {"--seed", "1", "--until", "none"}, "none"},
        {"pattern", {"--seed", "1", "--test-pattern", "--map-size", "512"}, "test_pattern"},
        {"regions", {"--seed", "1", "--map-size", "512", "--until", "regions"},
         "elevation,hydrology,climate,biomes,resources,regions"},
        {"chronology", {"--seed", "1", "--map-size", "512", "--until", "chronology"},
         "elevation,hydrology,climate,biomes,resources,regions,chronology"},
    };
    for (const Case& c : cases) {
        CAPTURE(std::string(c.name));
        const fs::path db = generate(sb, c.name, c.args);
        for (const char* level : {"summary", "full"}) {
            const ProcessResult r = sb.cli({"report", "--db", u8(db), "--history", "--level", level});
            CHECK(r.exit_code == EXIT_INPUT);
            CHECK(r.out.empty());
            CHECK(r.err == no_history(db, c.stages));
        }
        const fs::path out = sb.file(std::string(c.name) + ".txt");
        const ProcessResult r = sb.cli({"report", "--db", u8(db), "--history", "--out", u8(out)});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK_FALSE(file_exists(out));
        // the world report of the same file still works
        CHECK(sb.cli({"report", "--db", u8(db)}).exit_code == EXIT_OK);
    }
}

TEST_CASE("cli P3-07 14.2 item 12: a schema 6 file is refused by the reader") {
    Sandbox sb;
    const fs::path db = generate(sb, "s6", {"--seed", "1", "--until", "none"});
    REQUIRE(worldgen::db::testing::execute_on_file(
                u8(db), "PRAGMA user_version = 6; UPDATE meta SET value = '6' WHERE key = 'schema_version'")
                .ok());
    const ProcessResult r = sb.cli({"report", "--db", u8(db), "--history"});
    CHECK(r.exit_code == EXIT_INPUT);
    CHECK(r.out.empty());
    CHECK(r.err == "worldgen-cli: error: world schema version 6, this build reads 10\n");
}

TEST_CASE("cli P5-12 10 step 1: a schema 7 file (0.21.0) is refused by the reader") {
    Sandbox sb;
    const fs::path db = generate(sb, "s7", {"--seed", "1", "--until", "none"});
    REQUIRE(worldgen::db::testing::execute_on_file(
                u8(db), "PRAGMA user_version = 7; UPDATE meta SET value = '7' WHERE key = 'schema_version'")
                .ok());
    const ProcessResult r = sb.cli({"report", "--db", u8(db), "--history"});
    CHECK(r.exit_code == EXIT_INPUT);
    CHECK(r.out.empty());
    CHECK(r.err == "worldgen-cli: error: world schema version 7, this build reads 10\n");
}

}  // TEST_SUITE
