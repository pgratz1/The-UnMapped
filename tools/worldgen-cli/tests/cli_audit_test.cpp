// worldgen-cli behaviour decided by the lead after the P0-04 determinism
// audit (treated as spec, see the commit message):
//   - a failed or short write to stdout is exit 4 with the stderr line
//     "worldgen-cli: error: writing to stdout failed", for every command and
//     for --help / --version, whatever the output size (also instead of
//     exit 5 when the hash output was lost);
//   - anything at the output path counts as existing, a dangling symbolic
//     link included: exit 4 without --force, link unchanged; with --force
//     the link is replaced by a regular file and its target is not created;
//   - render refuses raw RGB data (3 x width x height x scale^2) above 1 GiB
//     with a usage error mentioning --scale (P0-04 amendment 7);
//   - on Windows, an argument or environment variable that is not valid
//     UTF-16 is a usage error.

#include <doctest/doctest.h>

#include <cstdint>
#include <ostream>
#include <string>
#include <system_error>
#include <vector>

#include "cli_test_support.hpp"
#include "p0_04_reference.hpp"
#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/testing.hpp"
#include "worldgen/stages/testing.hpp"

namespace {

using namespace clitest;

// Creates a symbolic link at link pointing to target; false if the
// platform or the account cannot create one.
bool make_symlink(const fs::path& target, const fs::path& link) {
    std::error_code ec;
    fs::create_symlink(target, link, ec);
    return !ec;
}

}  // namespace

// --- 1. Write errors on stdout ------------------------------------------------

#if defined(__linux__)

namespace {

constexpr std::string_view STDOUT_FAILED = "worldgen-cli: error: writing to stdout failed";

bool has_line(const std::string& text, std::string_view line) {
    for (const std::string& l : lines_of(text)) {
        if (l == line) {
            return true;
        }
    }
    return false;
}

void check_stdout_failed(const ProcessResult& r) {
    INFO("stderr: ", r.err);
    CHECK(r.exit_code == EXIT_OUTPUT);
    CHECK(!r.signalled);
    CHECK(has_line(r.err, STDOUT_FAILED));
}

RunOptions to_dev_full() {
    RunOptions o;
    o.process.stdout_path = "/dev/full";
    return o;
}

}  // namespace

TEST_CASE("cli audit: a failed write to stdout is exit 4 (small output, Linux /dev/full)") {
    if (!file_exists("/dev/full")) {
        MESSAGE("skipped: /dev/full is not available");
        return;
    }
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "0", "--test-pattern", "--out", u8(sb.file("w.db"))});
    const fs::path empty = sb.generate({"--until", "none", "--seed", "0", "--out", u8(sb.file("empty.db"))});

    for (const std::vector<std::string>& args :
         {std::vector<std::string>{"--version"},
          {"--help"},
          {"generate", "--help"},
          {"report", "--help"},
          {"report", "--db", u8(empty)},
          {"hash", "--db", u8(empty)},
          {"hash", "--db", u8(empty), "--detail"},
          {"hash", "--db", u8(empty), "--expect", file_sha256_hex(empty)}}) {
        CAPTURE(args[0]);
        CAPTURE(args.size());
        check_stdout_failed(sb.cli(args, to_dev_full()));
    }

    SUBCASE("hash --expect with a wrong value: exit 4, not 5") {
        std::string wrong = file_sha256_hex(empty);
        wrong[0] = wrong[0] == '0' ? '1' : '0';
        const ProcessResult r = sb.cli({"hash", "--db", u8(empty), "--expect", wrong}, to_dev_full());
        check_stdout_failed(r);
    }
    SUBCASE("generate: exit 4; the file, if left, is complete") {
        // The spec says nothing about the written file when only the path
        // line is lost. Chosen assertion: no .partial (Sandbox), and a file
        // left at the output path is byte-identical to a normal run.
        const fs::path out = sb.file("g.db");
        check_stdout_failed(sb.cli({"generate", "--until", "none", "--seed", "3", "--out", u8(out)}, to_dev_full()));
        REQUIRE(sb.cli({"generate", "--until", "none", "--seed", "3", "--out", u8(sb.file("ref.db"))}).exit_code == EXIT_OK);
        if (file_exists(out)) {
            CHECK(read_bytes(out) == read_bytes(sb.file("ref.db")));
        }
        check_stdout_failed(sb.cli({"generate", "--until", "none", "--seed", "3"}, to_dev_full()));
        if (file_exists(sb.out_dir() / "world-3.db")) {
            CHECK(read_bytes(sb.out_dir() / "world-3.db") == read_bytes(sb.file("ref.db")));
        }
    }
    SUBCASE("render: exit 4; the image, if left, is complete") {
        const fs::path out = sb.file("r.png");
        check_stdout_failed(sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--out", u8(out)}, to_dev_full()));
        REQUIRE(sb.cli({"render", "--db", u8(world), "--layer", "test.ramp", "--out", u8(sb.file("ref.png"))})
                    .exit_code == EXIT_OK);
        if (file_exists(out)) {
            CHECK(read_bytes(out) == read_bytes(sb.file("ref.png")));
        }
    }
    SUBCASE("report --out writes nothing on stdout, so /dev/full does not matter") {
        const ProcessResult r = sb.cli({"report", "--db", u8(empty), "--out", u8(sb.file("rep.txt"))}, to_dev_full());
        CHECK(r.exit_code == EXIT_OK);
        CHECK(read_text(sb.file("rep.txt")) == cli_report_seed_0());
    }
}

TEST_CASE("cli audit: a failed write to stdout is exit 4 (large output, Linux /dev/full)") {
    if (!file_exists("/dev/full")) {
        MESSAGE("skipped: /dev/full is not available");
        return;
    }
    Sandbox sb;
    const fs::path world = sb.generate({"--until", "none", "--seed", "0", "--out", u8(sb.file("big.db"))});
    // 300 extra tables with long names: report and hash --detail print
    // well over 16 KiB, far above any stdio buffer.
    std::string sql = "BEGIN;";
    for (int i = 0; i < 300; ++i) {
        std::string n = std::to_string(i);
        n.insert(0, 3 - n.size(), '0');
        sql += "CREATE TABLE t_" + n + "_" + std::string(56, 'x') + " (id INTEGER PRIMARY KEY) STRICT;";
    }
    sql += "COMMIT;";
    REQUIRE(worldgen::db::testing::execute_on_file(u8(world), sql).ok());

    const ProcessResult report = sb.cli({"report", "--db", u8(world)});
    REQUIRE(report.exit_code == EXIT_OK);
    CHECK(report.out.size() > 16384U);
    const ProcessResult detail = sb.cli({"hash", "--db", u8(world), "--detail"});
    REQUIRE(detail.exit_code == EXIT_OK);
    CHECK(detail.out.size() > 16384U);

    check_stdout_failed(sb.cli({"report", "--db", u8(world)}, to_dev_full()));
    check_stdout_failed(sb.cli({"hash", "--db", u8(world), "--detail"}, to_dev_full()));
    std::string wrong = file_sha256_hex(world);
    wrong[0] = wrong[0] == '0' ? '1' : '0';
    check_stdout_failed(sb.cli({"hash", "--db", u8(world), "--detail", "--expect", wrong}, to_dev_full()));
}

#else

TEST_CASE("cli audit: a failed write to stdout is exit 4 (needs Linux /dev/full)" * doctest::skip()) {}

#endif

// --- 2. A dangling symbolic link at the output path -----------------------------

TEST_CASE("cli audit: a dangling symbolic link at --out counts as existing") {
    Sandbox sb;
    const fs::path world = sb.generate({"--seed", "0", "--test-pattern", "--out", u8(sb.file("w.db"))});
    const fs::path probe_target = sb.file("probe-target");
    if (!make_symlink(probe_target, sb.file("probe-link"))) {
        MESSAGE("skipped: symbolic links cannot be created here");
        return;
    }
    fs::remove(sb.file("probe-link"));

    struct Case {
        const char* name;
        std::vector<std::string> args;  // without --out
    };
    const std::vector<Case> cases = {
        {"generate", {"generate", "--until", "none", "--seed", "5"}},
        {"render", {"render", "--db", u8(world), "--layer", "test.ramp"}},
        {"report", {"report", "--db", u8(world)}},
    };
    for (const Case& c : cases) {
        CAPTURE(c.name);
        const fs::path target = sb.file(std::string(c.name) + "-target");
        const fs::path link = sb.file(std::string(c.name) + "-link");
        REQUIRE(make_symlink(target, link));

        std::vector<std::string> args = c.args;
        args.insert(args.end(), {"--out", u8(link)});
        const ProcessResult refused = sb.cli(args);
        INFO("stderr: ", refused.err);
        CHECK(refused.exit_code == EXIT_OUTPUT);
        CHECK(refused.out.empty());
        CHECK(fs::is_symlink(fs::symlink_status(link)));
        CHECK(fs::read_symlink(link) == target);
        CHECK(!file_exists(target));

        args.emplace_back("--force");
        const ProcessResult forced = sb.cli(args);
        INFO("stderr: ", forced.err);
        CHECK(forced.exit_code == EXIT_OK);
        const fs::file_status st = fs::symlink_status(link);
        CHECK(!fs::is_symlink(st));
        CHECK(fs::is_regular_file(st));
        CHECK(!file_exists(target));
        CHECK(fs::file_size(link) > 0U);
    }
    // The replaced files hold what a normal run writes.
    REQUIRE(sb.cli({"report", "--db", u8(world), "--out", u8(sb.file("report-ref"))}).exit_code == EXIT_OK);
    CHECK(read_bytes(sb.file("report-link")) == read_bytes(sb.file("report-ref")));
    REQUIRE(sb.cli({"generate", "--until", "none", "--seed", "5", "--out", u8(sb.file("generate-ref"))}).exit_code == EXIT_OK);
    CHECK(read_bytes(sb.file("generate-link")) == read_bytes(sb.file("generate-ref")));

    // The default output path too.
    fs::create_directories(sb.out_dir());
    const fs::path default_link = sb.out_dir() / "world-6.db";
    const fs::path default_target = sb.file("default-target");
    REQUIRE(make_symlink(default_target, default_link));
    CHECK(sb.cli({"generate", "--until", "none", "--seed", "6"}).exit_code == EXIT_OUTPUT);
    CHECK(fs::is_symlink(fs::symlink_status(default_link)));
    CHECK(sb.cli({"generate", "--until", "none", "--seed", "6", "--force"}).exit_code == EXIT_OK);
    CHECK(fs::is_regular_file(fs::symlink_status(default_link)));
    CHECK(!file_exists(default_target));
}

// --- 3. Render size limit (amendment 7) -----------------------------------------

TEST_CASE("cli audit: render refuses raw RGB data above 1 GiB") {
    Sandbox sb;
    using worldgen::stages::testing::FixtureLayer;
    const fs::path world = sb.file("huge.db");
    {
        // 3 x 65536 x 5462 = 1073938432 > 2^30 = 1073741824 at scale 1.
        // All zeros: the encoded blob is a single zero-run token.
        std::vector<FixtureLayer> layers;
        layers.push_back(
            {"test.wide", worldgen::db::make_layer_grid(worldgen::db::ElementType::u8, 65536, 5462), worldgen::db::Codec::delta_varint});
        // 3 x 2048 x 2048 x 10^2 = 1258291200 > 2^30 at scale 10.
        layers.push_back({"test.square", worldgen::db::make_layer_grid(worldgen::db::ElementType::u8, 2048, 2048),
                          worldgen::db::Codec::delta_varint});
        REQUIRE(worldgen::stages::testing::write_fixture_world(u8(world), 0, layers).ok());
    }
    auto check_refused = [&](const std::vector<std::string>& args, const fs::path& would_write) {
        const ProcessResult r = sb.cli(args);
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.out.empty());
        CHECK(starts_with(r.err, "worldgen-cli: error: "));
        CHECK(contains(r.err, "--scale"));
        CHECK(!file_exists(would_write));
    };
    check_refused({"render", "--db", u8(world), "--layer", "test.wide"}, sb.out_dir() / "huge.test.wide.png");
    check_refused({"render", "--db", u8(world), "--layer", "test.wide", "--scale", "16"},
                  sb.out_dir() / "huge.test.wide.png");
    check_refused({"render", "--db", u8(world), "--layer", "test.square", "--scale", "10", "--out",
                   u8(sb.file("square.png"))},
                  sb.file("square.png"));
    check_refused({"render", "--db", u8(world), "--layer", "test.square", "--scale", "16", "--force", "--out",
                   u8(sb.file("square.png"))},
                  sb.file("square.png"));
    const bool nothing_in_out = !file_exists(sb.out_dir()) || fs::is_empty(sb.out_dir());
    CHECK(nothing_in_out);
    // The just-under side (for example 2048 x 2048 at scale 9, 3 x 2048^2 x
    // 81 = 1019215872 bytes) is not run: it would allocate about 1 GB.
}

// --- 4. Text that is not valid UTF-16 (Windows) ---------------------------------

#if defined(_WIN32)

TEST_CASE("cli audit: an environment variable that is not valid UTF-16 is a usage error (Windows)") {
    Sandbox sb;
    RunOptions o;
    std::wstring root = L"C:\\the-unmapped-";
    root += static_cast<wchar_t>(0xD800);  // lone high surrogate
    o.process.wide_env.emplace_back(L"UNMAPPED_BUILD_ROOT", root);
    const ProcessResult r = sb.cli({"generate", "--until", "none", "--seed", "1"}, o);
    INFO("stderr: ", r.err);
    CHECK(r.exit_code == EXIT_USAGE);
    CHECK(r.out.empty());
    CHECK(starts_with(r.err, "worldgen-cli: error: "));
    CHECK(contains(r.err, "not valid UTF-16"));
}

TEST_CASE("cli audit: an argument that is not valid UTF-16 is a usage error (Windows)") {
    Sandbox sb;
    std::wstring bad = L"gen";
    bad += static_cast<wchar_t>(0xDC00);  // lone low surrogate
    {
        // As the command: no command was given, so the hint has none.
        RunOptions o;
        o.process.wide_args = {bad};
        const ProcessResult r = sb.cli({}, o);
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_USAGE);
        CHECK(r.out.empty());
        const std::vector<std::string> lines = lines_of(r.err);
        REQUIRE(lines.size() == 2);
        CHECK(starts_with(lines[0], "worldgen-cli: error: "));
        CHECK(lines[1] == "worldgen-cli: run 'worldgen-cli --help' for usage");
    }
    {
        // As an option value.
        RunOptions o;
        o.process.wide_args = {bad};
        const ProcessResult r = sb.cli({"generate", "--seed-phrase"}, o);
        INFO("stderr: ", r.err);
        CHECK(r.exit_code == EXIT_USAGE);
        const std::vector<std::string> lines = lines_of(r.err);
        REQUIRE(lines.size() >= 2);
        CHECK(starts_with(lines[0], "worldgen-cli: error: "));
        CHECK(starts_with(lines.back(), "worldgen-cli: run 'worldgen-cli "));
        CHECK(!file_exists(sb.out_dir()));
    }
}

#else

TEST_CASE("cli audit: text that is not valid UTF-16 is a usage error (Windows only)" * doctest::skip()) {}

#endif
