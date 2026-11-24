// worldgen-cli report, spec P0-04 sections 7 and 12.4, with the amendments
// of section 14.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "cli_test_support.hpp"
#include "p0_04_reference.hpp"
#include "worldgen/core/hash.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/testing.hpp"

namespace {

using namespace clitest;

template <class T>
std::string layer_line(const std::string& name, const char* format, const std::vector<T>& cells) {
    const auto blob = worldgen::db::encode_layer_blob(p004::grid_of<T>(256, 256, cells), worldgen::db::Codec::delta_varint);
    const auto [lo, hi] = std::minmax_element(cells.begin(), cells.end());
    return name + " " + format + " 256x256 codec=1 bytes=" + std::to_string(blob.size()) +
           " min=" + std::to_string(static_cast<long long>(*lo)) + " max=" + std::to_string(static_cast<long long>(*hi));
}

}  // namespace

TEST_CASE("cli 12.4: empty world, seed 0, exact text of section 7") {
    Sandbox sb;
    const fs::path world = sb.generate({"--until", "none", "--seed", "0"});
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    INFO("stderr: ", r.err);
    CHECK(r.exit_code == EXIT_OK);
    CHECK(r.out == cli_report_seed_0());
    CHECK(r.err.empty());
    // On Windows in particular: no CR byte (4.4, 12.1).
    CHECK(r.out.find('\r') == std::string::npos);
    // --force without --out is accepted and has no effect (amendment 9).
    const ProcessResult f = sb.cli({"report", "--db", u8(world), "--force"});
    CHECK(f.exit_code == EXIT_OK);
    CHECK(f.out == cli_report_seed_0());
}

// Amendment 15: a scrubbed phrase has no backslash or control byte, so the
// escaping of 12.4 is checked on a test-only world: the seed-0 empty world
// with one extra meta row written through the test hook. The reader tolerates
// unknown meta keys (P0-02 5.5); the report prints every meta row (section 7).
TEST_CASE("cli 12.4: escaping in meta values (test-only world)") {
    Sandbox sb;
    auto world_with = [&](const std::string& name, const std::string& sql_value) {
        const fs::path world = sb.generate({"--until", "none", "--seed", "0", "--out", u8(sb.file(name))});
        const auto s = worldgen::db::testing::execute_on_file(
            u8(world), "INSERT INTO meta (key, value) VALUES ('zz.test_value', " + sql_value + ")");
        REQUIRE(s.ok());
        return world;
    };
    auto report_with_line = [](const std::string& line) {
        std::string s = "# worldgen report v2\n[meta]\n";
        for (const auto& [k, v] : cli_meta_rows(0)) {
            s += v.empty() ? k + " =\n" : k + " = " + v + "\n";
        }
        s += line;
        s += "[tables]\nage = 0\nbeast = 0\nbeast_population = 0\nbiome = 0\ncivilization = 0\nclimate = 0\nculture = 0\nevent = 0\nevent_cause = 0\nevent_kind = 0\nevent_kind_role = 0\nevent_kind_value = 0\nevent_participant = 0\nevent_value = 0\ngod = 0\nlayer = 0\nmeta = 18\nmonster = 0\nmonster_kind = 0\npass = 0\nphase = 0\nrace = 0\nrace_biome = 0\nrace_relation = 0\nregion = 0\nregion_biome = 0\nregion_border = 0\nregion_control = 0\nregion_name = 0\nrelation = 0\nsettlement = 0\nsettlement_census = 0\nsettlement_race = 0\nsite = 0\n[layers]\n(none)\n";
        return s;
    };
    SUBCASE("backslash and tab") {
        const fs::path world = world_with("w.db", "'a\\b' || char(9) || 'c'");
        const ProcessResult r = sb.cli({"report", "--db", u8(world)});
        INFO("stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
        CHECK(contains(r.out, "\nzz.test_value = a\\\\b\\x09c\n"));
        CHECK(r.out == report_with_line("zz.test_value = a\\\\b\\x09c\n"));
    }
    SUBCASE("other control characters and UTF-8") {
        // x 0x01 y 0x1F 0x7F "Ünmapped Wörld" LF z CR
        const fs::path world = world_with(
            "w.db", "'x' || char(1) || 'y' || char(31) || char(127) || CAST(X'C39C6E6D61707065642057C3B6726C64' AS TEXT) || "
                    "char(10) || 'z' || char(13)");
        const ProcessResult r = sb.cli({"report", "--db", u8(world)});
        INFO("stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
        CHECK(contains(r.out, "\nzz.test_value = x\\x01y\\x1F\\x7F" + std::string(PHRASE_NON_ASCII) + "\\x0Az\\x0D\n"));
        CHECK(r.out.find('\r') == std::string::npos);
    }
}

TEST_CASE("cli 12.4: test-pattern world") {
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "0", "--test-pattern"});
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    REQUIRE(r.exit_code == EXIT_OK);
    const std::vector<std::string> lines = lines_of(r.out);
    REQUIRE(lines.size() == 56);  // 17 meta rows and 32 tables since schema 9 (P5-02 11.1)
    CHECK(r.out.back() == '\n');
    CHECK(lines[0] == "# worldgen report v2");
    CHECK(lines[1] == "[meta]");
    CHECK(lines[18] == "stages = test_pattern");
    CHECK(lines[19] == "[tables]");
    std::vector<std::string> tables;
    for (const char* t : {"age", "beast", "beast_population", "biome", "civilization", "climate", "culture", "event", "event_cause", "event_kind",
                          "event_kind_role", "event_kind_value", "event_participant", "event_value", "god", "layer",
                          "meta", "monster", "monster_kind", "pass", "phase", "race", "race_biome", "region", "region_biome", "region_border",
                          "region_control", "region_name", "relation", "settlement", "settlement_census", "site"}) {
        const std::string n(t);
        tables.push_back(n + " = " + (n == "layer" ? "3" : n == "meta" ? "17" : "0"));
    }
    for (std::size_t k = 0; k < tables.size(); ++k) {
        CHECK(lines[20 + k] == tables[k]);
    }
    CHECK(lines[52] == "[layers]");
    CHECK(starts_with(lines[53], "test.ramp u8 256x256 codec=1 bytes="));
    CHECK(lines[53].substr(lines[53].size() - 14) == " min=0 max=255");
    CHECK(lines[53] == layer_line("test.ramp", "u8", p004::ramp_cells()));
    CHECK(lines[54] == layer_line("test.hash", "u8", p004::hash_cells(0)));
    CHECK(lines[55] == layer_line("test.noise", "i16", p004::noise_cells(0)));
    MESSAGE("seed-0 pattern report layer lines:\n", lines[53], "\n", lines[54], "\n", lines[55]);
}

TEST_CASE("cli 12.4: --out writes the same bytes as stdout") {
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "0", "--test-pattern"});
    const ProcessResult to_stdout = sb.cli({"report", "--db", u8(world)});
    REQUIRE(to_stdout.exit_code == EXIT_OK);
    const fs::path out = sb.file("report.txt");
    const ProcessResult to_file = sb.cli({"report", "--db", u8(world), "--out", u8(out)});
    REQUIRE(to_file.exit_code == EXIT_OK);
    CHECK(to_file.out.empty());  // amendment 9
    CHECK(read_text(out) == to_stdout.out);

    const ProcessResult refused = sb.cli({"report", "--db", u8(world), "--out", u8(out)});
    CHECK(refused.exit_code == EXIT_OUTPUT);
    CHECK(refused.out.empty());
    write_text(out, "old");
    CHECK(sb.cli({"report", "--db", u8(world), "--out", u8(out)}).exit_code == EXIT_OUTPUT);
    CHECK(read_text(out) == "old");
    CHECK(sb.cli({"report", "--db", u8(world), "--out", u8(out), "--force"}).exit_code == EXIT_OK);
    CHECK(read_text(out) == to_stdout.out);
    CHECK(sb.cli({"report", "--db", u8(world), "--out", u8(sb.file("no/dir/r.txt"))}).exit_code == EXIT_OUTPUT);
}

TEST_CASE("cli 12.4: report errors and excluded tables") {
    Sandbox sb;
    write_text(sb.file("text.db"), "plain text\n");
    CHECK(sb.cli({"report", "--db", u8(sb.file("text.db"))}).exit_code == EXIT_INPUT);
    CHECK(sb.cli({"report", "--db", u8(sb.files())}).exit_code == EXIT_INPUT);  // a directory

    // sqlite_* tables (here sqlite_stat1, made by ANALYZE) are not listed.
    const fs::path world = sb.generate({"--until", "none", "--seed", "0", "--out", u8(sb.file("w.db"))});
    REQUIRE(worldgen::db::testing::execute_on_file(u8(world), "ANALYZE").ok());
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    CHECK(r.exit_code == EXIT_OK);
    CHECK(r.out == cli_report_seed_0());
}
