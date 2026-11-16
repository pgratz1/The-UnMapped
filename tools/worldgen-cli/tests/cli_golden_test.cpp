// Golden world hashes, spec P0-04 sections 9 and 12.6.
//
// The expected values live in worldgen/tests/golden/p0-worlds.txt (lines
// "<case> <hex>", sorted by case name, LF), which the validation specialist
// records. Until that file exists the comparison test is SKIPPED (doctest
// reports it as skipped; it never passes silently). The cases themselves are
// always generated and checked for determinism and distinctness; run
// worldgen-cli-tests -s to print the observed hashes.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cli_test_support.hpp"

#ifndef UNMAPPED_P0_WORLDS_GOLDEN_PATH
#error "UNMAPPED_P0_WORLDS_GOLDEN_PATH must be defined by tools/worldgen-cli/tests/CMakeLists.txt"
#endif

namespace {

using namespace clitest;

struct GoldenCase {
    const char* name;
    std::vector<std::string> args;
};

// Section 9, in table order.
const std::vector<GoldenCase>& golden_cases() {
    static const std::vector<GoldenCase> cases = {
        // Amendment 16 (d): the empty-* cases pass --until none.
        {"empty-seed-0", {"--until", "none", "--seed", "0"}},
        {"empty-seed-1", {"--until", "none", "--seed", "1"}},
        {"empty-seed-42", {"--until", "none", "--seed", "42"}},
        {"empty-seed-max", {"--until", "none", "--seed", "18446744073709551615"}},
        {"empty-seed-0-512", {"--until", "none", "--seed", "0", "--map-size", "512"}},
        {"empty-phrase", {"--until", "none", "--seed-phrase", "The Unmapped"}},
        {"pattern-seed-0", {"--seed", "0", "--test-pattern"}},
        {"pattern-seed-42", {"--seed", "42", "--test-pattern"}},
    };
    return cases;
}

fs::path golden_path() { return from_u8(UNMAPPED_P0_WORLDS_GOLDEN_PATH); }

bool golden_file_present() { return file_exists(golden_path()); }

// Generates every case into the sandbox; returns case name -> file hash.
std::map<std::string, std::string> generate_cases(const Sandbox& sb, const std::string& tag) {
    std::map<std::string, std::string> hashes;
    for (const GoldenCase& c : golden_cases()) {
        std::vector<std::string> args = {"generate", "--out", u8(sb.file(tag + "-" + c.name + ".db"))};
        args.insert(args.end(), c.args.begin(), c.args.end());
        const ProcessResult r = sb.cli(args);
        INFO(c.name, " stderr: ", r.err);
        REQUIRE(r.exit_code == EXIT_OK);
        const ProcessResult h = sb.cli({"hash", "--db", u8(sb.file(tag + "-" + c.name + ".db"))});
        REQUIRE(h.exit_code == EXIT_OK);
        REQUIRE(h.out.size() == 65);
        hashes[c.name] = h.out.substr(0, 64);
    }
    return hashes;
}

}  // namespace

TEST_CASE("cli 9: the golden cases are deterministic and distinct") {
    Sandbox sb;
    const auto first = generate_cases(sb, "a");
    const auto second = generate_cases(sb, "b");
    CHECK(first == second);
    std::set<std::string> distinct;
    std::string listing;
    for (const auto& [name, hex] : first) {
        distinct.insert(hex);
        listing += name + " " + hex + "\n";
    }
    CHECK(distinct.size() == golden_cases().size());
    MESSAGE("observed world hashes (p0-worlds.txt format):\n", listing);
    // Case empty-phrase keeps its arguments and stores the scrubbed phrase
    // (amendment 15).
    const ProcessResult r = sb.cli({"report", "--db", u8(sb.file("a-empty-phrase.db"))});
    REQUIRE(r.exit_code == EXIT_OK);
    CHECK(contains(r.out, "\nseed_phrase = the unmapped\n"));
}

TEST_CASE("cli 9, 12.6: golden world hashes from p0-worlds.txt" * doctest::skip(!golden_file_present())) {
    const std::string text = read_text(golden_path());
    INFO("golden file: ", u8(golden_path()));
    CHECK(text.find('\r') == std::string::npos);
    REQUIRE(!text.empty());
    CHECK(text.back() == '\n');

    std::vector<std::pair<std::string, std::string>> entries;
    for (const std::string& line : lines_of(text)) {
        const std::size_t space = line.find(' ');
        REQUIRE_MESSAGE(space != std::string::npos, "malformed line: ", line);
        entries.emplace_back(line.substr(0, space), line.substr(space + 1));
    }
    CHECK(std::is_sorted(entries.begin(), entries.end()));
    std::set<std::string> expected_names;
    for (const GoldenCase& c : golden_cases()) {
        expected_names.insert(c.name);
    }
    std::set<std::string> names;
    for (const auto& [name, hex] : entries) {
        names.insert(name);
        // A placeholder is not a hash and fails here rather than passing.
        const bool is_hex = hex.size() == 64 && std::all_of(hex.begin(), hex.end(), [](char ch) {
                                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
                            });
        CHECK_MESSAGE(is_hex, name, ": not 64 lowercase hex digits: ", hex);
    }
    CHECK(names == expected_names);

    Sandbox sb;
    const auto observed = generate_cases(sb, "g");
    for (const auto& [name, hex] : entries) {
        CAPTURE(name);
        const auto it = observed.find(name);
        if (it == observed.end()) {
            continue;  // reported by the name check above
        }
        CHECK(it->second == hex);
        // The CI form of the check (section 9 step 3): hash --expect.
        const ProcessResult r = sb.cli({"hash", "--db", u8(sb.file("g-" + name + ".db")), "--expect", hex});
        CHECK(r.exit_code == EXIT_OK);
    }
}
