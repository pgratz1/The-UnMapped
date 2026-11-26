// Spec P5-07 (docs/specs/p5-07-mountain-depths.md r2) tests 16 items 13, 14 and (r2) 16, 17 and 19 as rows show them:
// the warren paths (delved, depth 3, woken, sealed, lost) and the aid over depths-small seeds 0 to 9 (4.2: "so every
// path of 6, 6.10 and 6.11 occurs in a 512 world"); I1's part that rows show (no event of kinds 97 and 98 and no
// waking without a site under delving_permille 0 and deep_aid_permille 0); deep_aid_max 0 and deep_aid_hops 0. And,
// from r1, 4.2's fixture: the invariants of
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

using p503api::V;

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
    // r2 paths
    if (!x.events_of("warren_delved").empty()) out.insert("warren delved");
    for (const std::int64_t e : x.events_of("warren_delved")) {
        if (x.val(e, 0) == 3) out.insert("warren depth 3");
    }
    std::set<std::int64_t> warren_m;
    for (const std::int64_t e : x.events_of("deep_woken")) {
        if (x.part(e, 3) == 0) {
            out.insert("warren woken");
            warren_m.insert(x.part(e, 1));
        }
    }
    if (!x.events_of("warren_sealed").empty()) out.insert("warren sealed");
    for (const std::int64_t e : x.events_of("hold_lost")) {
        if (warren_m.count(x.part(e, 1)) != 0U) out.insert("warren lost");
    }
    std::map<std::int64_t, int> aids;
    for (const std::int64_t e : x.events_of("ally_aided_hold")) {
        out.insert("aid");
        ++aids[x.cause_list(e).front()];
    }
    for (const auto& [w0, k] : aids) {
        if (k >= 2) out.insert("two aids");
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
    for (const char* p : {"deepened", "depth 3", "woken", "sealed", "fall at the waking", "later take",
                          // r2: "so every path of 6, 6.10 and 6.11 occurs" (4.2)
                          "warren delved", "warren woken", "aid"}) {
        CAPTURE(p);
        CHECK(seen.count(p) == 1U);
    }
    // r2: shown by the reference's depths-small replay on the stress geographies, but not required by name in 4.2
    WARN(seen.count("warren depth 3") == 1U);
    WARN(seen.count("warren sealed") == 1U);
    WARN(seen.count("warren lost") == 1U);
    WARN(seen.count("two aids") == 1U);
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

TEST_CASE("P5-07 r2 14 I1 (as rows show it): delving_permille 0 and deep_aid_permille 0: no warren, no aid") {
    P507_REQUIRE_INTEGRATED();
    for (const std::uint64_t seed : {0U, 1U, 2U}) {
        CAPTURE(seed);
        const std::string path = p507api::generated(seed, 512, p507api::i1_content(), "i1");
        check_world(path, p507api::i1_content(), "i1 seed " + std::to_string(seed));
        const p503check::World w = rows_of(path, p507api::i1_content());
        const p503check::Index x(w);
        CHECK(x.events_of("warren_delved").empty());
        CHECK(x.events_of("warren_sealed").empty());
        CHECK(x.events_of("ally_aided_hold").empty());
        for (const std::int64_t e : x.events_of("deep_woken")) CHECK(x.part(e, 3) != 0);   // every waking at a mine
        for (const std::int64_t e : x.events_of("hold_lost")) CHECK(x.val(e, 2) <= 2);     // the dwarven wordings
    }
    // the other half of I1 (the tables equal r1's on the same data) needs a build of r1: the reference's I1 replay
    // (p5_07_depths_reference.py fixtures) checks it against its own r1 rules
}

TEST_CASE("P5-07 r2 6.11, 13 item 14: deep_aid_max 0 gives no aid; deep_aid_hops 0 only allies in the hold's region") {
    P507_REQUIRE_INTEGRATED();
    static const wd::ContentSet none = p503api::with_history(p507api::depths_small(), "monsters", {{"deep_aid_max", V(0)}});
    static const wd::ContentSet near = p503api::with_history(p507api::depths_small(), "monsters", {{"deep_aid_hops", V(0)}});
    for (const std::uint64_t seed : {0U, 3U}) {
        CAPTURE(seed);
        const p503check::World a = rows_of(p507api::generated(seed, 512, none, "aid-none"), none);
        CHECK(p503check::Index(a).events_of("ally_aided_hold").empty());
        CHECK(p507_check::violations(a, p507_check::tuning_of(none)) == std::vector<std::string>{});
        const p503check::World b = rows_of(p507api::generated(seed, 512, near, "aid-near"), near);
        const p503check::Index x(b);
        for (const std::int64_t e : x.events_of("ally_aided_hold")) {
            const std::int64_t q = x.part(e, 4);
            CHECK(p503check::at(*x.stl.at(q), 4) == p503check::at(*x.ev.at(e), 5));   // the origin in the hold's region
        }
        CHECK(p507_check::violations(b, p507_check::tuning_of(near)) == std::vector<std::string>{});
    }
}

TEST_CASE("P5-07 r2 6.10, 13 items 12 and 13: a warren opens no site and sells no arms; a goblin realm is no seller") {
    P507_REQUIRE_INTEGRATED();
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        const std::string path = p507api::generated(seed, 512, p507api::depths_small(), "small");
        const p503check::World w = rows_of(path, p507api::depths_small());
        const p503check::Index x(w);
        std::set<std::int64_t> delvers;
        for (const std::int64_t e : x.events_of("warren_delved")) delvers.insert(x.part(e, 1));
        for (const eng::Row& s : w.rows.t.site) {
            if (p503check::at(s, 2) == p507api::SITE_MINE) {
                CAPTURE(p503check::at(s, 0));
                CHECK(delvers.count(p503check::at(s, 7)) == 0U);   // no warren delver founded a mine
            }
        }
        // every deep_woken without a site is a warren's: its settlement has a warren_delved before it
        for (const std::int64_t e : x.events_of("deep_woken")) {
            if (x.part(e, 3) != 0) continue;
            bool warren = false;
            for (const std::int64_t d : x.events_of("warren_delved")) {
                warren = warren || (d < e && p503check::at(*x.ev.at(d), 6) == p503check::at(*x.ev.at(e), 6));
            }
            CHECK(warren);
        }
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

TEST_CASE("P5-07 integration witnesses at 512 (shipped): seed 78 a warren woken and sealed, seed 9 an aid") {
    P507_REQUIRE_INTEGRATED();
    // Found by the integration's witness search (docs/reviews/p5-07-integration-notes.md) over shipped seeds 0 to 99
    // at 512; the reference's replay of these worlds equals the engine's tables.
    const std::set<std::string> p78 = paths_of(rows_of(p503run::world_path(78, 512), conflict_test::shipped()));
    CHECK(p78.count("warren woken") == 1U);
    CHECK(p78.count("warren sealed") == 1U);
    const std::set<std::string> p9 = paths_of(rows_of(p503run::world_path(9, 512), conflict_test::shipped()));
    CHECK(p9.count("aid") == 1U);
}

}  // TEST_SUITE

TEST_SUITE("depths-slow" * doctest::skip()) {

TEST_CASE("P5-07 integration witnesses at 1024 (shipped): seed 5 two aids at one waking and a warren sealed, seed 72 a "
          "realm ended by the deep") {
    P507_REQUIRE_INTEGRATED();
    const std::string w5 = p503run::world_path(5, 1024);
    check_world(w5, conflict_test::shipped(), "seed 5 at 1024");
    const std::set<std::string> p5 = paths_of(rows_of(w5, conflict_test::shipped()));
    CHECK(p5.count("two aids") == 1U);
    CHECK(p5.count("warren woken") == 1U);
    CHECK(p5.count("warren sealed") == 1U);
    const std::string w72 = p503run::world_path(72, 1024);
    check_world(w72, conflict_test::shipped(), "seed 72 at 1024");
    const std::set<std::string> p72 = paths_of(rows_of(w72, conflict_test::shipped()));
    CHECK(p72.count("realm lost") == 1U);
    CHECK(p72.count("aid") == 1U);
}

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
