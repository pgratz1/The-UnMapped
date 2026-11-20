// worldgen-cli common conventions, spec P0-04 sections 4 and 12.1, with the
// amendments of section 14.

#include <doctest/doctest.h>

#include <algorithm>
#include <ostream>
#include <string_view>
#include <string>
#include <vector>

#include "cli_test_support.hpp"
#include "worldgen/db/reader.hpp"

namespace {

using namespace clitest;

// Whether a usage error must end with the --help hint line. Section 4.1
// requires it for an unknown command or option, a missing value, a
// repeated option and a positional argument; for a bad value it is allowed
// but not required.
enum class Hint { required, optional };

// The hint line of amendment 2, "worldgen-cli: run 'worldgen-cli
// [<command>] --help' for usage", read as a template: the bracketed part is
// the command when one was recognised, and absent otherwise.
std::string expected_hint(const std::vector<std::string>& args) {
    if (!args.empty()) {
        for (const char* command : {"generate", "render", "report", "hash"}) {
            if (args[0] == command) {
                return std::string("worldgen-cli: run 'worldgen-cli ") + command + " --help' for usage";
            }
        }
    }
    return "worldgen-cli: run 'worldgen-cli --help' for usage";
}

// A usage error (exit 2) whose stderr starts "worldgen-cli: error: " (and
// ends with the hint line when required), with empty stdout.
void check_usage_error(const Sandbox& sb, const std::vector<std::string>& args, Hint hint = Hint::required,
                       const RunOptions& options = {}) {
    std::string shown;
    for (const std::string& a : args) {
        shown += a + " ";
    }
    INFO("arguments: ", shown);
    const ProcessResult r = sb.cli(args, options);
    INFO("stdout: ", r.out);
    INFO("stderr: ", r.err);
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.out.empty());
    const std::vector<std::string> lines = lines_of(r.err);
    REQUIRE(!lines.empty());
    CHECK(starts_with(lines.front(), "worldgen-cli: error: "));
    CHECK(r.err.back() == '\n');
    if (hint == Hint::required) {
        CHECK(lines.size() >= 2);
        CHECK(lines.back() == expected_hint(args));
    }
}

void check_generate_usage_error(const Sandbox& sb, const std::vector<std::string>& args,
                                Hint hint = Hint::optional) {
    std::vector<std::string> full = {"generate"};
    full.insert(full.end(), args.begin(), args.end());
    check_usage_error(sb, full, hint);
    // A usage error writes nothing at all.
    CHECK(!file_exists(sb.out_dir() / "world-0.db"));
}

}  // namespace

TEST_CASE("cli 12.1: no command, unknown command, unknown option are usage errors") {
    Sandbox sb;
    check_usage_error(sb, {});
    check_usage_error(sb, {"no-such-command"});
    check_usage_error(sb, {"--seed", "1"}, Hint::optional);  // an option where the command belongs
    check_usage_error(sb, {"Generate", "--seed", "1"});
    check_generate_usage_error(sb, {"--seed", "1", "--no-such-option"}, Hint::required);
    check_generate_usage_error(sb, {"--seed", "1", "-v"}, Hint::required);              // no short options
    check_generate_usage_error(sb, {"--seed=1"}, Hint::required);                       // no --name=value
    check_generate_usage_error(sb, {"--seed", "1", "positional"}, Hint::required);      // no positional arguments
    check_generate_usage_error(sb, {"--seed"}, Hint::required);                         // missing value
    check_generate_usage_error(sb, {"--seed", "1", "--seed", "1"}, Hint::required);     // repeated option
    check_generate_usage_error(sb, {"--seed", "1", "--force", "--force"}, Hint::required);
    check_usage_error(sb, {"report"}, Hint::optional);  // --db is required
    check_usage_error(sb, {"hash"}, Hint::optional);
    check_usage_error(sb, {"render", "--db", u8(sb.file("x.db"))}, Hint::optional);  // --layer is required
}

TEST_CASE("cli 12.1: seed values") {
    Sandbox sb;
    check_generate_usage_error(sb, {"--seed", "1", "--seed-phrase", "x"});
    check_generate_usage_error(sb, {"--seed-phrase", "x", "--seed", "1"});
    for (const char* bad : {"-1", "042", "18446744073709551616", "0x", "+1", " 1", "1 ", "1_000", "", "0X2A",
                            "0x10000000000000000", "0xg", "1.0", "99999999999999999999"}) {
        check_generate_usage_error(sb, {"--seed", bad});
    }
    // An option value may start with '-' (4.1): "-1" above is a value, so the
    // error is the bad seed, not a missing value; a phrase may start with '-'.
    const fs::path out = sb.file("dash.db");
    const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed-phrase", "-dash", "--out", u8(out)});
    CHECK(r.exit_code == EXIT_OK);
}

TEST_CASE("cli 12.1, amendment 15: a phrase that scrubs to nothing or to more than 256 bytes is a usage error") {
    Sandbox sb;
    const std::vector<std::string> bad = {
        "   ",
        "",
        "!!!",
        "?! .,;",
        "\xE6\x9D\xB1\xE4\xBA\xAC",          // only a non-Latin script ("東京")
        "\xD0\x9C\xD0\xB8\xD1\x80",          // "Мир"
        "\xC3\xA9\xC3\xA8",                  // only accented letters
        "\xF0\x9F\x97\xBA",                  // U+1F5FA
        std::string(257, 'a'),
        std::string(257, 'A'),
        std::string(128, 'a') + "!" + std::string(128, 'b'),  // scrubs to 257 bytes
    };
    for (const std::string& phrase : bad) {
        CAPTURE(phrase.size());
        check_generate_usage_error(sb, {"--seed-phrase", phrase});
        // The message says what a phrase may contain (stable fragments only).
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed-phrase", phrase});
        const std::vector<std::string> lines = lines_of(r.err);
        REQUIRE(!lines.empty());
        INFO("stderr: ", r.err);
        CHECK(contains(lines.front(), "--seed-phrase"));
        CHECK(contains(lines.front(), "a-z"));
        CHECK(contains(lines.front(), "0-9"));
    }
    // The limit is on the scrubbed phrase: 256 bytes are accepted, and so is
    // a much longer input that scrubs short.
    CHECK(sb.cli({"generate", "--until", "none", "--seed-phrase", std::string(128, 'a') + " " + std::string(127, 'b'), "--out",
                  u8(sb.file("p256.db"))})
              .exit_code == EXIT_OK);
    CHECK(sb.cli({"generate", "--until", "none", "--seed-phrase", std::string(2000, '!') + "Short" + std::string(2000, '?'), "--out",
                  u8(sb.file("short.db"))})
              .exit_code == EXIT_OK);
}

TEST_CASE("cli 12.1: no seed and bad map size are usage errors") {
    Sandbox sb;
    check_usage_error(sb, {"generate"}, Hint::optional);
    const ProcessResult r = sb.cli({"generate"});
    CHECK(starts_with(r.err, "worldgen-cli: error: no seed given; pass --seed or --seed-phrase\n"));
    check_generate_usage_error(sb, {"--test-pattern"});
    for (const char* bad : {"1000", "0", "-512", "0512", "4096", "256", "0x200", "", "1024.0"}) {
        check_generate_usage_error(sb, {"--seed", "0", "--map-size", bad});
    }
    // An empty settings file gives no seed either.
    const fs::path settings = sb.file("empty.toml");
    write_text(settings, "");
    check_generate_usage_error(sb, {"--settings", u8(settings)});
}

TEST_CASE("cli 12.1: invalid settings file is an input error with P0-03 error lines") {
    Sandbox sb;
    SUBCASE("unknown key") {
        const fs::path settings = sb.file("bad.toml");
        write_text(settings, "[world]\nseed = 42\nbogus = 1\n");
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--settings", u8(settings)});
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        const std::vector<std::string> lines = lines_of(r.err);
        REQUIRE(lines.size() >= 2);
        CHECK(starts_with(lines[0], "worldgen-cli: error: "));
        // P0-03 9: "<path>:<line>:<column>: error: <message>", the path as
        // given on the command line; amendment 10: an unknown key is
        // reported at its value's position (line 3, column 9).
        CHECK(starts_with(lines[1], u8(settings) + ":3:9: error: "));
        CHECK(!file_exists(sb.out_dir() / "world-42.db"));
    }
    SUBCASE("floating-point value") {
        const fs::path settings = sb.file("float.toml");
        write_text(settings, "[world]\nseed = 1\nmap_size = 1024.0\n");
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--settings", u8(settings)});
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_INPUT);
        const std::vector<std::string> lines = lines_of(r.err);
        REQUIRE(lines.size() >= 2);
        CHECK(starts_with(lines[0], "worldgen-cli: error: "));
        CHECK(starts_with(lines[1], u8(settings) + ":3:"));
        CHECK(contains(lines[1], "floating-point"));
    }
    SUBCASE("missing settings file") {
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "1", "--settings", u8(sb.file("none.toml"))});
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(starts_with(r.err, "worldgen-cli: error: "));
    }
}

TEST_CASE("cli 12.1: missing --db file is an input error") {
    Sandbox sb;
    const std::string missing = u8(sb.file("missing.db"));
    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"report", "--db", missing}, {"hash", "--db", missing},
          {"hash", "--db", missing, "--detail"}, {"render", "--db", missing, "--layer", "test.ramp"}}) {
        const ProcessResult r = sb.cli(args);
        INFO(args[0], " stderr: ", r.err);
        CHECK(r.exit_code == EXIT_INPUT);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: "));
    }
    CHECK(!file_exists(sb.out_dir() / "missing.test.ramp.png"));
}

TEST_CASE("cli 12.1: output errors") {
    Sandbox sb;
    const fs::path out = sb.file("w.db");
    REQUIRE(sb.cli({"generate", "--until", "none", "--seed", "1", "--out", u8(out)}).exit_code == EXIT_OK);
    SUBCASE("output exists without --force") {
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "1", "--out", u8(out)});
        CHECK(r.exit_code == EXIT_OUTPUT);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: "));
    }
    SUBCASE("--out into a missing directory") {
        const fs::path deep = sb.file("no/such/dir/w.db");
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "1", "--out", u8(deep)});
        CHECK(r.exit_code == EXIT_OUTPUT);
        CHECK(!file_exists(sb.file("no")));  // the CLI never creates that directory (4.3)
        CHECK(r.out.empty());
    }
    SUBCASE("empty --out and a directory as --out (amendment 5)") {
        CHECK(sb.cli({"generate", "--until", "none", "--seed", "1", "--out", ""}).exit_code == EXIT_OUTPUT);
        CHECK(sb.cli({"generate", "--until", "none", "--seed", "1", "--out", u8(sb.files())}).exit_code == EXIT_OUTPUT);
        CHECK(sb.cli({"generate", "--until", "none", "--seed", "1", "--out", u8(sb.files()), "--force"}).exit_code == EXIT_OUTPUT);
        CHECK(sb.cli({"report", "--db", u8(out), "--out", u8(sb.files())}).exit_code == EXIT_OUTPUT);
        CHECK(sb.cli({"report", "--db", u8(out), "--out", ""}).exit_code == EXIT_OUTPUT);
    }
}

TEST_CASE("cli 4.1: --version and --help") {
    Sandbox sb;
    const ProcessResult v = sb.cli({"--version"});
    CHECK(v.exit_code == EXIT_OK);
    CHECK(v.err.empty());
    CHECK(v.out == std::string("worldgen-cli 0.17.0 schema 7 sqlite ") + worldgen::db::sqlite_library_version() + "\n");

    const ProcessResult h = sb.cli({"--help"});
    CHECK(h.exit_code == EXIT_OK);
    CHECK(starts_with(h.out, "usage: worldgen-cli"));
    CHECK(h.err.empty());
    for (const char* command : {"generate", "render", "report", "hash"}) {
        INFO(command);
        const ProcessResult c = sb.cli({command, "--help"});
        CHECK(c.exit_code == EXIT_OK);
        CHECK(starts_with(c.out, std::string("usage: worldgen-cli ") + command));
        CHECK(c.err.empty());
    }
    // Amendment 2: --help and --version at the top level must be alone.
    check_usage_error(sb, {"--version", "extra"}, Hint::optional);
    check_usage_error(sb, {"--help", "generate"}, Hint::optional);
    check_usage_error(sb, {"--version", "--help"}, Hint::optional);
    // Amendment 2: options are tokenized first, then --help wins before any
    // value is validated.
    const ProcessResult hv = sb.cli({"generate", "--seed", "not-a-seed", "--map-size", "7", "--help"});
    CHECK(hv.exit_code == EXIT_OK);
    CHECK(starts_with(hv.out, "usage: worldgen-cli generate"));
    check_usage_error(sb, {"generate", "--bogus", "--help"});
    check_usage_error(sb, {"generate", "--help", "--help"});
    check_usage_error(sb, {"generate", "--help", "positional"});
    // --help writes nothing.
    CHECK(!file_exists(sb.out_dir()));
}

TEST_CASE("cli 4.3: build root resolution (amendment 4)") {
    Sandbox sb;
#if defined(_WIN32)
    const char* fallback = "LOCALAPPDATA";
    const fs::path fallback_dir = sb.file("localappdata");
    const fs::path fallback_out = fallback_dir / "the-unmapped" / "out" / "world-7.db";
#else
    const char* fallback = "HOME";
    const fs::path fallback_dir = sb.file("home");
    const fs::path fallback_out = fallback_dir / ".cache" / "the-unmapped" / "out" / "world-7.db";
#endif
    fs::create_directories(fallback_dir);

    SUBCASE("relative UNMAPPED_BUILD_ROOT is a usage error") {
        RunOptions o;
        o.env = {{"UNMAPPED_BUILD_ROOT", "relative/root"}};
        check_usage_error(sb, {"generate", "--until", "none", "--seed", "7"}, Hint::optional, o);
    }
    SUBCASE("empty UNMAPPED_BUILD_ROOT falls back to the platform location") {
        RunOptions o;
        o.env = {{"UNMAPPED_BUILD_ROOT", ""}, {fallback, u8(fallback_dir)}};
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "7"}, o);
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_OK);
        CHECK(r.out == printed_path(fallback_out));
        CHECK(file_exists(fallback_out));
    }
    SUBCASE("unset UNMAPPED_BUILD_ROOT falls back to the platform location") {
        RunOptions o;
        o.env = {{"UNMAPPED_BUILD_ROOT", std::nullopt}, {fallback, u8(fallback_dir)}};
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "7"}, o);
        CHECK(r.exit_code == EXIT_OK);
        CHECK(r.out == printed_path(fallback_out));
    }
    SUBCASE("no build root and no --out is a usage error") {
        RunOptions o;
        o.env = {{"UNMAPPED_BUILD_ROOT", std::nullopt}, {fallback, std::nullopt}};
        check_usage_error(sb, {"generate", "--until", "none", "--seed", "7"}, Hint::optional, o);
        // With --out no build root is needed.
        const fs::path out = sb.file("explicit.db");
        const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "7", "--out", u8(out)}, o);
        CHECK(r.exit_code == EXIT_OK);
        CHECK(file_exists(out));
    }
}

TEST_CASE("cli 4.4: stdout and stderr use LF only") {
    Sandbox sb;
    const fs::path out = sb.file("w.db");
    const ProcessResult g = sb.cli({"generate", "--seed", "0", "--test-pattern", "--out", u8(out), "--verbose"});
    REQUIRE(g.exit_code == EXIT_OK);
    const ProcessResult rep = sb.cli({"report", "--db", u8(out)});
    const ProcessResult hash = sb.cli({"hash", "--db", u8(out), "--detail"});
    const ProcessResult bad = sb.cli({"report", "--db", u8(sb.file("none.db"))});
    REQUIRE(rep.exit_code == EXIT_OK);
    REQUIRE(hash.exit_code == EXIT_OK);
    for (const ProcessResult* r : {&g, &rep, &hash, &bad}) {
        CHECK(r->out.find('\r') == std::string::npos);
        CHECK(r->err.find('\r') == std::string::npos);
    }
    // --verbose prints progress on stderr only: stdout is the path alone.
    CHECK(g.out == printed_path(out));
}
