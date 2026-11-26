// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r1) 16 and 17: the golden file worldgen/tests/golden/p5-07-depths.txt
// (written by the independent reference, `p5_07_depths_reference.py golden --inputs DIR`, at integration; never
// regenerated to make this test pass): for each case, the P0-04 8 hashes of the tables site, monster, event and
// event_value of the generated world, and the statistics of 10 item 7 from its rows. The depths-small cases (seeds 0
// to 9 at 512) run by default; the shipped seed 42 at 1024 is in the suite "depths-slow". Skipped while the file is
// absent; every case also starts with P507_REQUIRE_INTEGRATED().
//
// Format (the reference's GOLDEN_HEADER): `case <name> <seed> <size> <data>` (data depths-small or shipped), then
// `table <name> <sha256>` and `stat <key> <value>` lines of that case; `#` lines are comments.

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "../p3_01_history_support.hpp"
#include "p3_04_engine_support.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_07_api.hpp"
#include "p5_07_checks.hpp"

namespace {

namespace fs = std::filesystem;
namespace db = worldgen::db;

struct GCase {
    std::string name, data;
    std::uint64_t seed = 0;
    std::int64_t size = 0;
    std::map<std::string, std::string> tables;
    std::vector<std::pair<std::string, std::int64_t>> stats;
};

fs::path golden_path() {
    return history_test::source_root() / "worldgen" / "tests" / "golden" / "p5-07-depths.txt";
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
            std::string t, h;
            s >> t >> h;
            out.back().tables[t] = h;
        } else if (head == "stat" && !out.empty()) {
            std::string k;
            std::int64_t v = 0;
            s >> k >> v;
            out.back().stats.emplace_back(k, v);
        }
    }
    return out;
}

void check_case(const GCase& c) {
    CAPTURE(c.name);
    const bool small = c.data == "depths-small";
    const auto& content = small ? p507api::depths_small() : conflict_test::shipped();
    const std::string path = small ? p507api::generated(c.seed, c.size, content, "small")
                                   : p503run::world_path(c.seed, c.size);
    const db::WorldReader r = chrono_test::open_world(path);
    const eng::Tables t = engine_test::tables_of_world(r);
    for (const auto& [name, hash] : c.tables) {
        CAPTURE(name);
        CHECK(engine_test::table_hash(engine_test::columns_of(name), t.of(name)) == hash);
    }
    CHECK(p507_check::stats(p503check::world_of_file(r, content)) == c.stats);
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-07 16, 17: the depths-small cases of p5-07-depths.txt (seeds 0 to 9 at 512)") {
    P507_REQUIRE_INTEGRATED();
    if (!fs::exists(golden_path())) {
        MESSAGE("p5-07-depths.txt absent (recorded at integration): skipped");
        return;
    }
    std::size_t n = 0;
    for (const GCase& c : golden()) {
        if (c.data != "depths-small") continue;
        ++n;
        check_case(c);
    }
    CHECK(n == 10U);
}

}  // TEST_SUITE

TEST_SUITE("depths-slow" * doctest::skip()) {

TEST_CASE("P5-07 16, 17: the shipped case of p5-07-depths.txt (seed 42 at 1024)") {
    P507_REQUIRE_INTEGRATED();
    if (!fs::exists(golden_path())) {
        MESSAGE("p5-07-depths.txt absent (recorded at integration): skipped");
        return;
    }
    for (const GCase& c : golden()) {
        if (c.data == "shipped") check_case(c);
    }
}

}  // TEST_SUITE
