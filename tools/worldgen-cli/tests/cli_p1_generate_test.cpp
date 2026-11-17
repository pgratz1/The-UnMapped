// worldgen-cli generate in phase P1 (spec P1-00 section 10.1 and 14 X1, X3;
// P0-04 amendment 16 (a), adopting P1-00 appendix A4): --until, --threads,
// and the P0 worlds under --until none.
//
// Written from the specs and `worldgen-cli generate --help` only.

#include <doctest/doctest.h>

#include <cstddef>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "cli_test_support.hpp"

#ifndef UNMAPPED_P0_WORLDS_GOLDEN_PATH
#error "UNMAPPED_P0_WORLDS_GOLDEN_PATH must be defined by tools/worldgen-cli/tests/CMakeLists.txt"
#endif

namespace {

using namespace clitest;

// The golden world hashes of P0-04 section 9 (p0-worlds.txt), by case.
std::map<std::string, std::string> p0_goldens() {
    std::map<std::string, std::string> g;
    const std::string text = read_text(from_u8(UNMAPPED_P0_WORLDS_GOLDEN_PATH));
    for (const std::string& line : lines_of(text)) {
        const std::size_t sp = line.find(' ');
        if (sp != std::string::npos) {
            g[line.substr(0, sp)] = line.substr(sp + 1);
        }
    }
    return g;
}

std::string golden(const std::string& name) {
    const auto g = p0_goldens();
    const auto it = g.find(name);
    REQUIRE_MESSAGE(it != g.end(), "no golden value for ", name);
    return it->second;
}

// generate into files/<name>; REQUIREs exit 0; returns the file's SHA-256.
std::string generate_hash(const Sandbox& sb, const std::string& name, std::vector<std::string> args,
                          const RunOptions& options = {}) {
    std::vector<std::string> full = {"generate", "--out", u8(sb.file(name))};
    full.insert(full.end(), args.begin(), args.end());
    const ProcessResult r = sb.cli(full, options);
    INFO("args: ", name, " stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    return file_sha256_hex(sb.file(name));
}

// generate with args that must be a usage error: exit 2, one error line
// first, nothing written. Returns stderr.
std::string usage_error(const Sandbox& sb, std::vector<std::string> args) {
    const fs::path out = sb.file("refused.db");
    std::vector<std::string> full = {"generate", "--seed", "0", "--out", u8(out)};
    full.insert(full.end(), args.begin(), args.end());
    const ProcessResult r = sb.cli(full);
    std::string joined;
    for (const std::string& a : args) {
        joined += "[" + a + "] ";
    }
    INFO("args: ", joined, " stderr: ", r.err);
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.out.empty());
    CHECK(starts_with(r.err, "worldgen-cli: error: "));
    CHECK_FALSE(file_exists(out));
    return r.err;
}

// The first line of stderr with every occurrence of `name` replaced by a
// placeholder.
std::string first_line_without(const std::string& err, const std::string& name) {
    std::string line = err.substr(0, err.find('\n'));
    for (std::size_t p = line.find(name); p != std::string::npos; p = line.find(name, p)) {
        line.replace(p, name.size(), "<NAME>");
    }
    return line;
}

}  // namespace

TEST_CASE("cli P1-00 10.1: --until none writes the P0 empty world, byte for byte") {
    Sandbox sb;
    const std::string expected = golden("empty-seed-0");
    CHECK(generate_hash(sb, "none.db", {"--seed", "0", "--until", "none"}) == expected);
    CHECK(generate_hash(sb, "none-t3.db", {"--seed", "0", "--until", "none", "--threads", "3"}) == expected);
    CHECK(generate_hash(sb, "phrase.db", {"--seed-phrase", "The Unmapped", "--until", "none"}) == golden("empty-phrase"));
    CHECK(generate_hash(sb, "512.db", {"--seed", "0", "--map-size", "512", "--until", "none"}) ==
          golden("empty-seed-0-512"));
}

TEST_CASE("cli P0-03 7.6, amendment 17: with the required kind worldgen, a nonexistent data root is an input error") {
    Sandbox sb;
    const std::string expected = golden("empty-seed-0");
    const std::string missing = u8(sb.file("no-such-data-dir"));
    const ProcessResult flag =
        sb.cli({"generate", "--seed", "0", "--until", "none", "--data", missing, "--out", u8(sb.file("flag.db"))});
    CHECK(flag.exit_code == EXIT_INPUT);
    CHECK_FALSE(file_exists(sb.file("flag.db")));
    RunOptions env;
    env.env.push_back({"UNMAPPED_DATA_DIR", missing});
    const ProcessResult from_env = sb.cli({"generate", "--seed", "0", "--until", "none", "--out", u8(sb.file("env.db"))}, env);
    CHECK(from_env.exit_code == EXIT_INPUT);
    CHECK_FALSE(file_exists(sb.file("env.db")));
    // Without UNMAPPED_DATA_DIR the configured root (the shipped game/data)
    // is read: the golden empty world.
    RunOptions unset;
    unset.env.push_back({"UNMAPPED_DATA_DIR", std::nullopt});
    CHECK(generate_hash(sb, "unset.db", {"--seed", "0", "--until", "none"}, unset) == expected);
}

TEST_CASE("cli P1-00 10.1, X1: every unimplemented stage name is a usage error, unknown and unimplemented names differ") {
    Sandbox sb;
    const std::string bogus = usage_error(sb, {"--until", "bogus"});
    CHECK(contains(bogus, "bogus"));
    // elevation (P1-01), hydrology (P1-02) and climate (P1-03) are
    // implemented; the later stages are not yet.
    for (const char* name : {"biomes", "resources", "regions"}) {
        CAPTURE(name);
        const std::string err = usage_error(sb, {"--until", name});
        CHECK(contains(err, name));
        // The message for a stage that exists but is not implemented is not
        // the message for an unknown name.
        CHECK(first_line_without(err, name) != first_line_without(bogus, "bogus"));
    }
    for (const char* name : {"Elevation", "NONE", "", "test_pattern", "elevation,hydrology"}) {
        CAPTURE(name);
        const std::string err = usage_error(sb, {"--until", name});
        if (std::string_view(name).size() > 0U) {
            CHECK(first_line_without(err, name) == first_line_without(bogus, "bogus"));
        }
    }
    // A missing value and a repeated option are usage errors as for every option.
    usage_error(sb, {"--until"});
    usage_error(sb, {"--until", "none", "--until", "none"});
}

TEST_CASE("cli P1-00 10.1, X1: --until with --test-pattern is a usage error") {
    Sandbox sb;
    usage_error(sb, {"--test-pattern", "--until", "none"});
    usage_error(sb, {"--until", "none", "--test-pattern"});
    usage_error(sb, {"--test-pattern", "--until", "elevation"});
    usage_error(sb, {"--test-pattern", "--until", "bogus"});
}

TEST_CASE("cli P1-00 10.1: --threads accepts 1 to 256 in decimal without leading zeros") {
    Sandbox sb;
    const std::string expected = golden("empty-seed-0");
    for (const char* n : {"1", "2", "9", "10", "100", "255", "256"}) {
        CAPTURE(n);
        CHECK(generate_hash(sb, std::string("t") + n + ".db", {"--seed", "0", "--until", "none", "--threads", n}) ==
              expected);
    }
    for (const char* bad : {"0", "257", "1000", "01", "001", "-1", "+1", "", " 1", "1 ", "abc", "1.5", "0x10", "4294967297",
                            "18446744073709551617", "1e2"}) {
        CAPTURE(bad);
        usage_error(sb, {"--threads", bad});
    }
    usage_error(sb, {"--threads"});
    usage_error(sb, {"--threads", "2", "--threads", "2"});
}

TEST_CASE("cli P1-00 10.1, D10, X3: --threads never changes the output (test-pattern world at 1, 2 and 5 threads)") {
    Sandbox sb;
    const std::string expected = golden("pattern-seed-0");
    for (const char* n : {"1", "2", "5"}) {
        CAPTURE(n);
        CHECK(generate_hash(sb, std::string("p") + n + ".db", {"--seed", "0", "--test-pattern", "--threads", n}) ==
              expected);
    }
    CHECK(generate_hash(sb, "p42.db", {"--seed", "42", "--test-pattern", "--threads", "5"}) == golden("pattern-seed-42"));
    CHECK(generate_hash(sb, "pdef.db", {"--seed", "0", "--test-pattern"}) == expected);
}

TEST_CASE("cli P0-04 4.1: generate --help lists the new options") {
    Sandbox sb;
    const ProcessResult r = sb.cli({"generate", "--help"});
    REQUIRE(r.exit_code == EXIT_OK);
    CHECK(r.err.empty());
    CHECK(contains(r.out, "--until"));
    CHECK(contains(r.out, "none"));
    CHECK(contains(r.out, "--threads"));
    CHECK(contains(r.out, "--test-pattern"));
    CHECK(contains(r.out, "--data"));
    // --help still wins over values that would be invalid (amendment 2).
    const ProcessResult h = sb.cli({"generate", "--until", "bogus", "--threads", "0", "--help"});
    CHECK(h.exit_code == EXIT_OK);
    CHECK(h.out == r.out);
}
