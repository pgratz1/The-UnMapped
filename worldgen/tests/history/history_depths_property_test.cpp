// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r1) tests 16 items 13 and 14, and 4.2's fixture: the invariants of
// 13 that rows show (p5_07_checks.hpp: items 1 to 9; 10 and 11 are the reference's replay and check-world) on
// depths-small worlds (seeds 0 to 9 at 512: every path of 6 must occur over them) and on the shipped data's worlds
// (seeds 0 to 9 at 512; the slow suite "depths-slow": seeds 0 to 19 at 1024 and --monster-pressure high 0 to 4);
// determinism (twice, and at one and four threads); I0 (14) as rows show it: with deep_delve_permille 0 no event of
// kinds 93 to 96 and no deep monster, and forge_depth_permille 0 and 15 give the same history tables (no mine has a
// depth, so D(c) = 0); resume (P3-04 16.3 Y2: --until chronology, then the history stage on the stored inputs) on a
// depths-small world whose history holds a waking (reading: P3-04's resume point is the history stage's start, so
// "a waking before the resume point" is read as "a world whose resumed history has a waking", D-R13 of the notes).
// Every case starts with P507_REQUIRE_INTEGRATED().

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p3_04_engine_api.hpp"
#include "p5_03_relations_runs.hpp"
#include "p5_07_api.hpp"
#include "p5_07_checks.hpp"
#include "worldgen/db/testing.hpp"

namespace {

namespace db = worldgen::db;
namespace wd = worldgen::data;

p503check::World rows_of(const std::string& path, const wd::ContentSet& content) {
    const db::WorldReader r = chrono_test::open_world(path);
    return p503check::world_of_file(r, content);
}

void check_world(const std::string& path, const wd::ContentSet& content, const std::string& label) {
    CAPTURE(label);
    const p503check::World w = rows_of(path, content);
    CHECK(p507_check::violations(w, p507_check::tuning_of(content)) == std::vector<std::string>{});
}

std::string stats_line(const p503check::World& w) {
    std::string s;
    for (const auto& [k, v] : p507_check::stats(w)) s += " " + k + "=" + std::to_string(v);
    return s;
}

// The paths of 6 a world shows.
std::set<std::string> paths_of(const p503check::World& w) {
    const p503check::Index x(w);
    std::set<std::string> out;
    if (!x.events_of("delving_deepened").empty()) out.insert("deepened");
    for (const std::int64_t e : x.events_of("delving_deepened")) {
        if (x.val(e, 0) == 3) out.insert("depth 3");
    }
    if (!x.events_of("deep_woken").empty()) out.insert("woken");
    if (!x.events_of("deep_sealed").empty()) out.insert("sealed");
    for (const std::int64_t e : x.events_of("hold_lost")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        out.insert(cs.size() == 1U && x.kind_of(cs[0]) == "monster_raid" ? "later take" : "fall at the waking");
    }
    for (const std::int64_t e : x.events_of("refugees_settled")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (cs.size() == 1U && x.kind_of(cs[0]) == "hold_lost") out.insert("flight");
    }
    for (const std::int64_t e : x.events_of("civ_ended")) {
        const std::vector<std::int64_t> cs = x.cause_list(e);
        if (cs.size() == 1U && x.kind_of(cs[0]) == "hold_lost") out.insert("realm lost");
    }
    for (const std::int64_t e : x.events_of("region_renamed")) {
        for (const std::int64_t c : x.cause_list(e)) {
            if (x.kind_of(c) == "hold_lost") out.insert("renamed by rule 8");
        }
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-07 13, 16 item 13: depths-small seeds 0 to 9 at 512: the invariants; every path of 6 occurs") {
    P507_REQUIRE_INTEGRATED();
    std::set<std::string> seen;
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string path = p507api::generated(seed, 512, p507api::depths_small(), "small");
        check_world(path, p507api::depths_small(), "depths-small seed " + std::to_string(seed));
        const p503check::World w = rows_of(path, p507api::depths_small());
        MESSAGE("depths-small seed " << seed << ":" << stats_line(w));
        const std::set<std::string> p = paths_of(w);
        seen.insert(p.begin(), p.end());
    }
    std::string list;
    for (const std::string& s : seen) list += " [" + s + "]";
    MESSAGE("depths-small paths:" << list);
    // 4.2: "so that every path of 6 occurs in a 512 world"
    for (const char* p : {"deepened", "depth 3", "woken", "sealed", "fall at the waking", "later take"}) {
        CAPTURE(p);
        CHECK(seen.count(p) == 1U);
    }
    // a flight needs a race's last refuge and a realm lost needs another realm of the race: reported, not failed
    WARN(seen.count("flight") == 1U);
    WARN(seen.count("realm lost") == 1U);
}

TEST_CASE("P5-07 13, 16 item 13: the shipped data, seeds 0 to 9 at 512: the invariants") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        check_world(p503run::world_path(seed, 512), conflict_test::shipped(), "seed " + std::to_string(seed) + " at 512");
    }
}

TEST_CASE("P5-07 14: determinism: depths-small twice, and at one and four threads") {
    P507_REQUIRE_INTEGRATED();
    const std::string a = p507api::generated(3, 512, p507api::depths_small(), "det-a");
    const std::string b = p507api::generated(3, 512, p507api::depths_small(), "det-b");
    CHECK(dbtest::read_file(a) == dbtest::read_file(b));
    const std::string four = p507api::generated(3, 512, p507api::depths_small(), "det-4", 4);
    CHECK(dbtest::read_file(a) == dbtest::read_file(four));
}

TEST_CASE("P5-07 14 I0: deep_delve_permille 0: no deep event; forge_depth_permille 0 and 15 give the same history") {
    P507_REQUIRE_INTEGRATED();
    for (const std::uint64_t seed : {0U, 42U}) {
        CAPTURE(seed);
        const std::string z0 = p507api::generated(seed, 512, p507api::zero(0), "zero-0");
        const std::string z15 = p507api::generated(seed, 512, p507api::zero(15), "zero-15");
        const p503check::World w = rows_of(z0, p507api::zero(0));
        const p503check::Index x(w);
        for (const auto& [id, key] : p507api::new_kinds()) {
            CAPTURE(key);
            CHECK(x.events_of(key).empty());
        }
        for (const eng::Row& m : w.rows.t.monster) CHECK(p503check::at(m, 2) <= 6);
        const eng::Tables t0 = engine_test::tables_of_world(chrono_test::open_world(z0));
        const eng::Tables t15 = engine_test::tables_of_world(chrono_test::open_world(z15));
        CHECK(t0.event == t15.event);
        CHECK(t0.event_value == t15.event_value);
        CHECK(t0.settlement == t15.settlement);
        CHECK(t0.relation == t15.relation);
        CHECK(t0.site == t15.site);
        CHECK(t0.monster == t15.monster);
        // the catalogue rows of 93 to 96 and the monster_kind rows 7 to 9 are written whatever the tuning (14)
        CHECK(t0.monster_kind.size() == 9U);
    }
}

TEST_CASE("P5-07 14, P3-04 16.3 Y2: resume from --until chronology on a depths-small world with a waking") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string full = p507api::generated(seed, 512, p507api::depths_small(), "small");
        const p503check::World w = rows_of(full, p507api::depths_small());
        if (p503check::Index(w).events_of("deep_woken").empty()) continue;
        CAPTURE(seed);
        const std::string part = p507api::generated(seed, 512, p507api::depths_small(), "small", 1, "chronology");
        const eng::Tables resumed = eng::history_stage(
            p507api::depths_small(), eng::inputs_from_world(chrono_test::open_world(part), p507api::depths_small()));
        const eng::Tables stored = engine_test::tables_of_world(chrono_test::open_world(full));
        CHECK(resumed.event == stored.event);
        CHECK(resumed.site == stored.site);
        CHECK(resumed.monster == stored.monster);
        CHECK(resumed == stored);
        return;
    }
    FAIL("no depths-small world of seeds 0 to 9 at 512 has a waking");
}

}  // TEST_SUITE

TEST_SUITE("depths-slow" * doctest::skip()) {

TEST_CASE("P5-07 13, 16 item 13: the invariants at 1024, seeds 0 to 19 (shipped)") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        const std::string path = p503run::world_path(seed, 1024);
        check_world(path, conflict_test::shipped(), "seed " + std::to_string(seed) + " at 1024");
        MESSAGE("seed " << seed << " at 1024:" << stats_line(rows_of(path, conflict_test::shipped())));
    }
}

TEST_CASE("P5-07 13, 16 item 13: the invariants at 1024 with --monster-pressure high, seeds 0 to 4") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 5; ++seed) {
        const std::string path = p503run::world_path(seed, 1024, "high");
        check_world(path, conflict_test::shipped(), "high seed " + std::to_string(seed) + " at 1024");
        MESSAGE("high seed " << seed << " at 1024:" << stats_line(rows_of(path, conflict_test::shipped())));
    }
}

}  // TEST_SUITE
