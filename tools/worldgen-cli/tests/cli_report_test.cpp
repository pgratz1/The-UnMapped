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

// The report of an empty world with the given meta rows (escaping done by
// the caller).
std::string empty_report(const p004::MetaRows& rows) {
    std::string s = "# worldgen report v1\n[meta]\n";
    for (const auto& [k, v] : rows) {
        s += v.empty() ? k + " =\n" : k + " = " + v + "\n";
    }
    s += "[tables]\nlayer = 0\nmeta = 8\n[layers]\n(none)\n";
    return s;
}

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
    const fs::path world = sb.generate({"--seed", "0"});
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    INFO("stderr: ", r.err);
    CHECK(r.exit_code == EXIT_OK);
    CHECK(r.out == p004::REPORT_SEED_0);
    CHECK(r.err.empty());
    // On Windows in particular: no CR byte (4.4, 12.1).
    CHECK(r.out.find('\r') == std::string::npos);
    // --force without --out is accepted and has no effect (amendment 9).
    const ProcessResult f = sb.cli({"report", "--db", u8(world), "--force"});
    CHECK(f.exit_code == EXIT_OK);
    CHECK(f.out == p004::REPORT_SEED_0);
}

TEST_CASE("cli 12.4: escaping in meta values") {
    Sandbox sb;
    SUBCASE("backslash and tab") {
        const fs::path world = sb.generate({"--seed-phrase", "a\\b\tc", "--out", u8(sb.file("w.db"))});
        const ProcessResult r = sb.cli({"report", "--db", u8(world)});
        REQUIRE(r.exit_code == EXIT_OK);
        CHECK(contains(r.out, "\nseed_phrase = a\\\\b\\x09c\n"));
        p004::MetaRows rows = p004::meta_rows(worldgen::seed_from_phrase("a\\b\tc").seed);
        rows[6].second = "a\\\\b\\x09c";
        CHECK(r.out == empty_report(rows));
    }
    SUBCASE("other control characters and UTF-8") {
        // P0-01 7.3 accepts control characters inside a phrase given on the
        // command line; trailing ASCII whitespace is trimmed.
        const std::string phrase = std::string("x\x01y\x1F") + "\x7F" + std::string(PHRASE_NON_ASCII) + "\nz\r";
        const fs::path world = sb.generate({"--seed-phrase", phrase, "--out", u8(sb.file("w.db"))});
        const ProcessResult r = sb.cli({"report", "--db", u8(world)});
        REQUIRE(r.exit_code == EXIT_OK);
        CHECK(contains(r.out, "\nseed_phrase = x\\x01y\\x1F\\x7F" + std::string(PHRASE_NON_ASCII) + "\\x0Az\n"));
        CHECK(r.out.find('\r') == std::string::npos);
    }
}

TEST_CASE("cli 12.4: test-pattern world") {
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "0", "--test-pattern"});
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    REQUIRE(r.exit_code == EXIT_OK);
    const std::vector<std::string> lines = lines_of(r.out);
    REQUIRE(lines.size() == 17);
    CHECK(r.out.back() == '\n');
    CHECK(lines[0] == "# worldgen report v1");
    CHECK(lines[1] == "[meta]");
    CHECK(lines[9] == "stages = test_pattern");
    CHECK(lines[10] == "[tables]");
    CHECK(lines[11] == "layer = 3");
    CHECK(lines[12] == "meta = 8");
    CHECK(lines[13] == "[layers]");
    CHECK(starts_with(lines[14], "test.ramp u8 256x256 codec=1 bytes="));
    CHECK(lines[14].substr(lines[14].size() - 14) == " min=0 max=255");
    CHECK(lines[14] == layer_line("test.ramp", "u8", p004::ramp_cells()));
    CHECK(lines[15] == layer_line("test.hash", "u8", p004::hash_cells(0)));
    CHECK(lines[16] == layer_line("test.noise", "i16", p004::noise_cells(0)));
    MESSAGE("seed-0 pattern report layer lines:\n", lines[14], "\n", lines[15], "\n", lines[16]);
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
    const fs::path world = sb.generate({"--seed", "0", "--out", u8(sb.file("w.db"))});
    REQUIRE(worldgen::db::testing::execute_on_file(u8(world), "ANALYZE").ok());
    const ProcessResult r = sb.cli({"report", "--db", u8(world)});
    CHECK(r.exit_code == EXIT_OK);
    CHECK(r.out == p004::REPORT_SEED_0);
}
