// worldgen-cli with the peoples of spec P3-05 (generator 0.15.0): the
// default run (= --until history) of a golden world against
// worldgen/tests/golden/p3-05-peoples.txt (hash --detail's thirteen table
// lines, report's [history] with the keys of 15.4, placed after
// [chronology]), --threads 5 byte-identical (16.2 item 20), --until
// chronology leaves the history tables empty, the settings civs_per_race
// and event_intensity now reach the world (3.1), and the generator version.
//
// Written from the spec only. These cases need no P3-05 header and compile
// before the implementation exists; they fail until it lands. The other
// golden worlds are in the doctest suite "history-slow".

#include <doctest/doctest.h>

#include <string>
#include <utility>
#include <vector>

#include "cli_test_support.hpp"
#include "worldgen/db/reader.hpp"

#ifndef UNMAPPED_P3_05_GOLDEN_PATH
#error "UNMAPPED_P3_05_GOLDEN_PATH must name worldgen/tests/golden/p3-05-peoples.txt"
#endif
#ifndef UNMAPPED_TEST_SOURCE_ROOT
#error "UNMAPPED_TEST_SOURCE_ROOT must name the repository root"
#endif

namespace {

using namespace clitest;

fs::path source_root() { return fs::path(UNMAPPED_TEST_SOURCE_ROOT); }

std::vector<std::pair<std::string, std::string>> golden_items(const std::string& name) {
    static const std::string text = read_text(fs::path(UNMAPPED_P3_05_GOLDEN_PATH));
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

std::string run_ok(const Sandbox& sb, const std::vector<std::string>& args) {
    const ProcessResult r = sb.cli(args);
    INFO("stderr: ", r.err);
    REQUIRE(r.exit_code == EXIT_OK);
    return r.out;
}

std::string meta_of(const fs::path& db, const std::string& key) {
    auto r = worldgen::db::WorldReader::open(u8(db));
    REQUIRE_MESSAGE(r.ok(), "cannot open " << u8(db));
    return r.value().meta(key).value_or("<absent>");
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
    // 15.4: population, then population_<race>, settlements_standing_<race>, regions_held_<race> after relations
    CHECK(contains(rep, "\nrelations = 0\npopulation = "));
    CHECK(contains(rep, "\npopulation_elf = "));
    CHECK(contains(rep, "\nsettlements_standing_orc = "));
    CHECK(contains(rep, "\nregions_held_orc = "));
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("cli P3-05 16.3, 16.2 item 20: the golden world geo-seed-42-512, --until history, --threads 5") {
    Sandbox sb;
    const fs::path a = generate_case(sb, "geo-seed-42-512", {"--seed", "42", "--map-size", "512"});
    CHECK(meta_of(a, "generator_version") == "0.16.0");
    CHECK(meta_of(a, "schema_version") == "7");
    check_golden_db(sb, "geo-seed-42-512", a);
    const fs::path b = generate_case(sb, "threads", {"--seed", "42", "--map-size", "512", "--threads", "5"});
    CHECK(read_bytes(a) == read_bytes(b));
}

TEST_CASE("cli P3-05 3.1: --event-intensity and --civs-per-race reach the history; --until chronology leaves it empty") {
    Sandbox sb;
    const fs::path base = generate_case(sb, "hist-span-2000-512", {"--seed", "7", "--map-size", "512", "--history-span", "2000"});
    check_golden_db(sb, "hist-span-2000-512", base);
    const fs::path busy = generate_case(sb, "busy", {"--seed", "7", "--map-size", "512", "--history-span", "2000",
                                                     "--event-intensity", "turbulent", "--civs-per-race", "4"});
    const std::string r0 = section(run_ok(sb, {"report", "--db", u8(base)}), "[history]");
    const std::string r1 = section(run_ok(sb, {"report", "--db", u8(busy)}), "[history]");
    CHECK(r0 != r1);
    const fs::path chron = generate_case(sb, "chron", {"--seed", "7", "--map-size", "512", "--history-span", "2000",
                                                       "--until", "chronology"});
    const std::string rep = run_ok(sb, {"report", "--db", u8(chron)});
    CHECK(section(rep, "[history]").empty());
    CHECK(contains(section(rep, "[tables]"), "\nsettlement = 0\n"));
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("cli P3-05 16.3: [history] and hash --detail of the other golden worlds") {
    Sandbox sb;
    check_golden_db(sb, "geo-seed-0-512", generate_case(sb, "geo-seed-0-512", {"--seed", "0", "--map-size", "512"}));
    check_golden_db(sb, "geo-phrase-512",
                    generate_case(sb, "geo-phrase-512", {"--seed-phrase", "The Unmapped", "--map-size", "512"}));
    check_golden_db(sb, "geo-seed-1-1024", generate_case(sb, "geo-seed-1-1024", {"--seed", "1"}));
    check_golden_db(sb, "hist-races-elf-orc-1024",
                    generate_case(sb, "hist-races-elf-orc-1024", {"--seed", "3", "--races", "elf,orc"}));
    check_golden_db(sb, "hist-dense-1024",
                    generate_case(sb, "hist-dense-1024", {"--seed", "11", "--history-detail", "dense"}));
    check_golden_db(sb, "hist-peoples-42-1024", generate_case(sb, "hist-peoples-42-1024", {"--seed", "42"}));
}

}  // TEST_SUITE
