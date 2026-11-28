// Spec P5-11 (docs/specs/p5-11-artifacts.md r1) 16 and 17: the golden file worldgen/tests/golden/p5-11-artifacts.txt
// (written by the independent reference, `p5_11_artifacts_reference.py golden [--inputs DIR]`; never regenerated to
// make this test pass): for each world case (artifacts-small at 512 on seeds 0 to 9; the shipped seed 42 at 1024 in
// the suite "artifacts-slow") the P0-04 8 hashes of the tables artifact_kind, artifact, artifact_holding, figure, event
// and event_cause of the generated world and the statistics of 10 item 6 from its rows. The fixture cases (size 0: the
// conflict stress geographies) are the reference's replay of fixture data, which the C++ side cannot build: their
// `path` lines are checked here (before integration too: a file check) for the paths each fixture file promises.
//
// Format (the reference's GOLDEN_HEADER): `case <name> <seed> <size> <data>`, then `table <name> <sha256>`,
// `stat <key> <value>` and (fixture cases) `path <path>` lines of that case; `#` lines are comments. The stat
// cult_returns_barred was dropped by the lead (2026-11-28).

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../p3_01_history_support.hpp"
#include "p3_04_engine_support.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_11_property_common.hpp"

namespace {

namespace fs = std::filesystem;

struct GCase {
    std::string name, data;
    std::uint64_t seed = 0;
    std::int64_t size = 0;
    std::map<std::string, std::string> tables;
    std::vector<std::pair<std::string, std::int64_t>> stats;
    std::set<std::string> paths;
};

fs::path golden_path() {
    return history_test::source_root() / "worldgen" / "tests" / "golden" / "p5-11-artifacts.txt";
}

std::vector<GCase> golden() {
    std::vector<GCase> out;
    std::ifstream f(golden_path(), std::ios::binary);
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream s(line);
        std::string head;
        s >> head;
        if (head == "case") {
            GCase c;
            s >> c.name >> c.seed >> c.size >> c.data;
            out.push_back(c);
        } else if (head == "table" && !out.empty()) {
            std::string t, hx;
            s >> t >> hx;
            out.back().tables[t] = hx;
        } else if (head == "stat" && !out.empty()) {
            std::string k;
            std::int64_t v = 0;
            s >> k >> v;
            out.back().stats.emplace_back(k, v);
        } else if (head == "path" && !out.empty()) {
            std::string rest;
            std::getline(s, rest);
            out.back().paths.insert(rest.substr(rest.find_first_not_of(' ')));
        }
    }
    return out;
}

std::size_t columns(const std::string& t) {
    if (t == "artifact_kind") return 8U;
    if (t == "artifact" || t == "artifact_holding") return 14U;
    if (t == "figure") return 19U;
    return engine_test::columns_of(t);
}

void check_world_case(const GCase& c) {
    CAPTURE(c.name);
    const bool small = c.data == "artifacts-small";
    const auto& content = small ? p511api::artifacts_small() : conflict_test::shipped();
    const std::string path = small ? p511api::generated(c.seed, c.size, content, "small")
                                   : p503run::world_path(c.seed, c.size);
    const p511_check::World w = artifacts_prop::rows_of(path);
    for (const auto& [name, hash] : c.tables) {
        CAPTURE(name);
        std::vector<eng::Row> rows;
        for (const auto& r : w.base.rows(name)) rows.push_back(r);
        CHECK(engine_test::table_hash(columns(name), rows) == hash);
    }
    CHECK(p511_check::stats(w) == c.stats);
}

std::vector<std::string> promised_paths(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::string line;
    std::vector<std::string> out;
    while (std::getline(in, line)) {
        if (line.rfind("paths = [", 0) != 0) continue;
        std::size_t i = 0;
        while ((i = line.find('"', i)) != std::string::npos) {
            const std::size_t j = line.find('"', i + 1);
            out.push_back(line.substr(i + 1, j - i - 1));
            i = j + 1;
        }
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-11 16, 17: the artifacts-small cases of p5-11-artifacts.txt (seeds 0 to 9 at 512)") {
    P511_REQUIRE_INTEGRATED();
    if (!fs::exists(golden_path())) {
        MESSAGE("p5-11-artifacts.txt absent (recorded at integration): skipped");
        return;
    }
    std::size_t k = 0;
    for (const GCase& c : golden()) {
        if (c.data != "artifacts-small" || c.size == 0) continue;
        ++k;
        check_world_case(c);
    }
    if (k == 0U) MESSAGE("p5-11-artifacts.txt holds no world case yet (fixture lines only before integration)");
}

TEST_CASE("P5-11 16: the fixture files' promised paths and the stress cases' required paths in the golden lines") {
    REQUIRE(fs::exists(golden_path()));
    const std::vector<GCase> g = golden();
    const fs::path dir = history_test::source_root() / "worldgen" / "tests" / "data" / "artifacts";
    std::size_t files = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".toml") continue;
        ++files;
        const std::string stem = entry.path().stem().string();
        std::set<std::string> seen;
        std::size_t cases = 0;
        for (const GCase& c : g) {
            if (c.data != stem) continue;
            ++cases;
            seen.insert(c.paths.begin(), c.paths.end());
        }
        CAPTURE(stem);
        CHECK(cases == p508api::fixture_cases(entry.path()).size());
        for (const std::string& p : promised_paths(entry.path())) {
            CAPTURE(p);
            CHECK(seen.count(p) == 1U);
        }
    }
    CHECK(files >= 1U);
    std::set<std::string> stress;
    for (const GCase& c : g) {
        if (c.data == "artifacts-small" && c.size == 0) stress.insert(c.paths.begin(), c.paths.end());
    }
    for (const std::string& p : artifacts_prop::required_paths()) {
        CAPTURE(p);
        CHECK(stress.count(p) == 1U);
    }
}

}  // TEST_SUITE

TEST_SUITE("artifacts-slow" * doctest::skip()) {

TEST_CASE("P5-11 16, 17: the shipped case of p5-11-artifacts.txt (seed 42 at 1024)") {
    P511_REQUIRE_INTEGRATED();
    if (!fs::exists(golden_path())) return;
    for (const GCase& c : golden()) {
        if (c.data == "shipped") check_world_case(c);
    }
}

}  // TEST_SUITE
