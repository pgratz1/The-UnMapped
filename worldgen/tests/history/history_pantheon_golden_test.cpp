// Spec P5-08 (docs/specs/p5-08-pantheon-cults.md r1) 16 and 17: the golden file worldgen/tests/golden/p5-08-pantheon.txt
// (written by the independent reference, `p5_08_pantheon_reference.py golden [--inputs DIR]`; never regenerated to
// make this test pass): for each world case (pantheon-small at 512 on seeds 0 to 9; the shipped seed 42 at 1024 in the
// suite "pantheon-slow") the P0-04 8 hashes of the tables god, faction, site, figure, event and event_value of the
// generated world and the statistics of 10 item 8 from its rows. The fixture cases (size 0: the conflict stress
// geographies, the files of worldgen/tests/data/pantheon/) are the reference's replay of fixture data, which the C++
// side cannot build without the engineer's in-memory content (integration item 3 of the notes): their `path` lines are
// checked here for the paths each fixture file promises (its `paths` list). Every case starts with
// P508_REQUIRE_INTEGRATED(); skipped while the file is absent.
//
// Format (the reference's GOLDEN_HEADER): `case <name> <seed> <size> <data>`, then `table <name> <sha256>`,
// `stat <key> <value>` and (fixture cases) `path <path>` lines of that case; `#` lines are comments. The stats
// renewals_capped and deep_stirred are the replay's (not in rows): they are compared only for fixture cases.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../p3_01_history_support.hpp"
#include "p3_04_engine_support.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_08_property_common.hpp"

namespace {

namespace fs = std::filesystem;
namespace db = worldgen::db;

struct GCase {
    std::string name, data;
    std::uint64_t seed = 0;
    std::int64_t size = 0;
    std::map<std::string, std::string> tables;
    std::vector<std::pair<std::string, std::int64_t>> stats;
    std::set<std::string> paths;
};

fs::path golden_path() {
    return history_test::source_root() / "worldgen" / "tests" / "golden" / "p5-08-pantheon.txt";
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
    if (t == "god") return 6U;
    if (t == "faction") return 13U;
    if (t == "figure") return 19U;
    return engine_test::columns_of(t);
}

void check_world_case(const GCase& c) {
    CAPTURE(c.name);
    const bool small = c.data == "pantheon-small";
    const auto& content = small ? p508api::pantheon_small() : conflict_test::shipped();
    const std::string path = small ? p508api::generated(c.seed, c.size, content, "small")
                                   : p503run::world_path(c.seed, c.size);
    const p508_check::World w = pantheon_prop::rows_of(path);
    for (const auto& [name, hash] : c.tables) {
        CAPTURE(name);
        std::vector<eng::Row> rows;
        for (const auto& r : w.rows(name)) rows.push_back(r);
        CHECK(engine_test::table_hash(columns(name), rows) == hash);
    }
    std::vector<std::pair<std::string, std::int64_t>> want;
    for (const auto& kv : c.stats) {
        if (kv.first != "renewals_capped" && kv.first != "deep_stirred") want.push_back(kv);
    }
    CHECK(p508_check::stats(w) == want);
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

TEST_CASE("P5-08 16, 17: the pantheon-small cases of p5-08-pantheon.txt (seeds 0 to 9 at 512)") {
    P508_REQUIRE_INTEGRATED();
    if (!fs::exists(golden_path())) {
        MESSAGE("p5-08-pantheon.txt absent (recorded at integration): skipped");
        return;
    }
    std::size_t n = 0;
    for (const GCase& c : golden()) {
        if (c.data != "pantheon-small" || c.size == 0) continue;
        ++n;
        check_world_case(c);
    }
    if (n == 0U) MESSAGE("p5-08-pantheon.txt holds no world case yet (fixture lines only before integration)");
}

TEST_CASE("P5-08 16: every fixture file's promised paths are in its golden path lines") {
    P508_REQUIRE_INTEGRATED();
    if (!fs::exists(golden_path())) return;
    const std::vector<GCase> g = golden();
    const fs::path dir = history_test::source_root() / "worldgen" / "tests" / "data" / "pantheon";
    for (const auto& entry : fs::directory_iterator(dir)) {
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
}

}  // TEST_SUITE

TEST_SUITE("pantheon-slow" * doctest::skip()) {

TEST_CASE("P5-08 16, 17: the shipped case of p5-08-pantheon.txt (seed 42 at 1024)") {
    P508_REQUIRE_INTEGRATED();
    if (!fs::exists(golden_path())) return;
    for (const GCase& c : golden()) {
        if (c.data == "shipped") check_world_case(c);
    }
}

}  // TEST_SUITE
