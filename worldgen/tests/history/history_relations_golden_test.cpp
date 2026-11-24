// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) 23 and 24.2: the golden file p5-03-relations.txt (written
// by reference/p5_03_relations_reference.py) against the C++ (table hashes of race_relation and settlement_race,
// the counts of kinds 56 to 62, the [stats] keys of 17.4); the invariants of 19 from the rows on generated worlds;
// I0 (7.4) on the stress fixtures and generated geographies, and P5-02's I0 with part B; resume from --until
// chronology; the same rows with 1 and 3 threads.
//
// Default suite: the fixtures, the 512 world cases of the golden file, invariants on seeds 42 and 0 at 512, I0 on
// stress 0 to 3 and seeds 0 and 1 at 512, resume and threads at 512. Suite "relations-slow": every other world
// case, invariants on seeds 0 to 9 at 512, 0 to 19 at 1024 and --monster-pressure high seeds 0 to 4 at 1024, I0 on
// seeds 2 to 4 at 512 and 0 to 4 at 1024 (24.2's lists; the 512 ones split for time, reading R-G1).
//
// Written from the spec before the implementation. Library names: the adapter's.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "../history_report_checks.hpp"
#include "p5_03_relations_runs.hpp"
#include "worldgen/inspect/history_report.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

using p503api::RCase;
namespace h = worldgen::history;

// --- golden cases ---------------------------------------------------------------------------------------

const wd::ContentSet& content_of(const std::string& data) {
    if (data == "relations-small") return p503api::relations_small_content();
    if (data == "monsters-small" || data == "monsters-small-fear") return p503api::monsters_small_fear_content();
    if (data == "zero") return p503api::zero_content();
    FAIL("unknown fixture data " << data);
    return p503api::zero_content();
}

std::string world_of(const RCase& g) {
    if (g.detail == "normal" && g.civs == "auto" && g.intensity == "normal") {
        if (g.pressure == "normal") {
            return chrono_test::world(chrono_test::WorldKey{g.seed, g.phrase, g.size, g.span, g.races, "", 1});
        }
        if (g.phrase.empty()) return p503run::world_path(g.seed, g.size, g.pressure, g.span, g.races);
    }
    static std::map<std::string, std::string> cache;
    if (const auto it = cache.find(g.name); it != cache.end()) return it->second;
    chrono_api::Params p = chrono_test::params_with(g.span, chrono_test::races_of_text(g.races));
    p.detail = g.detail;
    p.civs = g.civs == "auto" ? std::nullopt : std::optional<std::int64_t>(std::stoll(g.civs));
    p.intensity = g.intensity;
    p.pressure = g.pressure;
    const std::string path = chrono_test::world_dir().file("relg" + std::to_string(cache.size()) + ".db");
    worldgen::WorldParams wp = chrono_test::world_params(g.seed, g.size, p, conflict_test::shipped(), std::nullopt, 1);
    wp.seed_phrase = g.phrase;
    REQUIRE(worldgen::generate_world(wp, conflict_test::shipped(), path, db::Overwrite::refuse).ok());
    cache[g.name] = path;
    return path;
}

void check_case(const RCase& g) {
    CAPTURE(g.name);
    std::optional<p503check::World> w;
    std::string path;
    if (g.fixture()) {
        w = p503run::stage_fixture(g.seed, content_of(g.data));
    } else {
        path = world_of(g);
        w = p503run::world_rows(path);
    }
    const std::map<std::string, const std::vector<eng::Row>*> tables = {
        {"race_relation", &w->rows.race_relation}, {"settlement_race", &w->rows.settlement_race}};
    for (const auto& [name, rows] : tables) {
        const auto it = g.tables.find(name);
        if (it == g.tables.end()) {
            MESSAGE(g.name << ": no `table " << name << "` line yet");
            continue;
        }
        CHECK(engine_test::table_hash(5, *rows) == it->second);
    }
    const p503check::Index x(*w);
    for (const auto& [id, key] : p503api::new_kinds()) {
        const auto it = g.events.find(key);
        if (it == g.events.end()) {
            MESSAGE(g.name << ": no events_" << key << " line yet");
            continue;
        }
        CHECK(static_cast<std::int64_t>(x.events_of(key).size()) == it->second);
    }
    if (!g.stats.empty()) {
        if (g.fixture()) {
            MESSAGE(g.name << ": stat lines of a fixture are the reference's own (no world file here)");
        } else {
            const std::string full = hrtest::report_of(path, hrtest::HistoryLevel::full);
            std::map<std::string, std::string> got;
            for (const std::string& l : hrtest::block(full, "[stats]")) {
                const std::size_t eq = l.find(" = ");
                got[l.substr(0, eq)] = l.substr(eq + 3);
            }
            for (const auto& [k, v] : g.stats) {
                CAPTURE(k);
                CHECK(got[k] == v);
            }
        }
    }
    CHECK(p503check::violations(*w) == std::vector<std::string>{});
}

// --- I0 (7.4) --------------------------------------------------------------------------------------------

const std::vector<std::string>& i0_tables() {
    static const std::vector<std::string> v = {"event", "event_participant", "event_cause", "event_value", "phase",
                                               "civilization", "settlement", "settlement_census", "region_control",
                                               "relation", "region_name", "beast", "monster_kind",
                                               "beast_population", "monster", "site"};
    return v;
}

void check_i0(const eng::HistoryInputs& in, const wd::ContentSet& c) {
    const p503api::RTables with = p503api::history_stage(c, in);
    p503api::RelConfig::Options o;
    o.relations = false;
    o.ledger = false;
    o.view = false;
    const p503api::RelConfig cfg(c, o);
    const p503api::RTables without = cfg.run(c, in);
    for (const std::string& name : i0_tables()) {
        CAPTURE(name);
        CHECK(with.t.of(name) == without.t.of(name));
    }
    // no event of part A; only a colony's founding rows and conquests' standings, which write no event (7.4)
    const p503check::World w = p503check::world_of_run(with, in, c);
    const p503check::Index x(w);
    for (const auto& [id, key] : p503api::new_kinds()) {
        if (id >= 57) CHECK(x.events_of(key).empty());
    }
    CHECK(with.race_relation.empty());
}

// P5-02 7.5's zero tuning with this spec's zero (P5-02's I0 with part B and part A both off)
const wd::ContentSet& zero_both() {
    static const wd::ContentSet c = monsters_content::with_monsters(
        p503api::zero_content(),
        {{"pressure", monsters_content::pressure_of(0, 0, 0)}, {"drag_max_permille", p503api::V(0)},
         {"feed_max_permille", p503api::V(0)}, {"bold_permille", p503api::V(0)}, {"outpost_permille", p503api::V(0)}},
        {}, {}, {{"ghost_permille", p503api::V(0)}});
    return c;
}

eng::HistoryInputs inputs_of_seed(std::uint64_t seed, std::int64_t size) {
    const db::WorldReader r = chrono_test::open_world(p503run::world_path(seed, size, "normal", 6000, "all", "chronology"));
    return eng::inputs_from_world(r, conflict_test::shipped());
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 23: the fixtures and the 512 world cases of p5-03-relations.txt") {
    const auto& g = p503api::relations_golden();
    if (g.empty()) {
        MESSAGE("p5-03-relations.txt absent or empty: recorded in phase 2");
        return;
    }
    for (const RCase& c : g) {
        if (c.fixture() || c.size <= 512) check_case(c);
    }
}

TEST_CASE("P5-03 19: the invariants from the rows on seeds 42 and 0 at 512") {
    for (const std::uint64_t seed : {42U, 0U}) {
        CAPTURE(seed);
        CHECK(p503check::violations(p503run::world_rows(p503run::world_path(seed, 512))) == std::vector<std::string>{});
    }
}

TEST_CASE("P5-03 7.4 I0: the zero tuning equals the configuration without the subsystem (stress 0 to 3)") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        check_i0(p503run::stress_inputs(seed, p503api::zero_content()), p503api::zero_content());
    }
}

TEST_CASE("P5-03 7.4 I0 on seeds 0 and 1 at 512") {
    for (const std::uint64_t seed : {0U, 1U}) {
        CAPTURE(seed);
        check_i0(inputs_of_seed(seed, 512), p503api::zero_content());
    }
}

TEST_CASE("P5-03 24.2: P5-02's I0 holds with part B (and part A at zero), stress 0 to 3") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const wd::ContentSet& c = zero_both();
        const eng::HistoryInputs in = p503run::stress_inputs(seed, c);
        const p503api::RTables with = p503api::history_stage(c, in);
        p503api::RelConfig::Options o;
        o.relations = false;
        o.ledger = false;
        o.view = false;
        o.monsters = false;
        const p503api::RelConfig cfg(c, o);
        const p503api::RTables without = cfg.run(c, in);
        for (const char* name : {"event", "event_participant", "event_cause", "event_value", "phase",
                                        "civilization", "settlement", "settlement_census", "region_control",
                                        "relation", "region_name"}) {
            CAPTURE(name);
            CHECK(with.t.of(name) == without.t.of(name));
        }
        CHECK(with.t.monster.empty());
        CHECK(with.t.site.empty());
    }
}

TEST_CASE("P5-03 24.2: resume from a --until chronology world equals one run (seed 42, 512)") {
    const db::WorldReader part = chrono_test::open_world(p503run::world_path(42, 512, "normal", 6000, "all", "chronology"));
    const p503api::RTables resumed = p503api::history_stage(conflict_test::shipped(), eng::inputs_from_world(part, conflict_test::shipped()));
    const p503check::World one = p503run::world_rows(p503run::world_path(42, 512));
    CHECK(resumed.race_relation == one.rows.race_relation);
    CHECK(resumed.settlement_race == one.rows.settlement_race);
    CHECK(resumed.t.event == one.rows.t.event);
    CHECK_FALSE(one.rows.race_relation.empty());
}

TEST_CASE("P5-03 20: the two tables are the same with 1 and 3 threads (seed 42, 512)") {
    const db::WorldReader a = chrono_test::open_world(p503run::world_path(42, 512));
    const db::WorldReader b = chrono_test::open_world(p503run::world_path(42, 512, "normal", 6000, "all", "", 3));
    for (const char* t : {"race_relation", "settlement_race", "event"}) {
        CAPTURE(t);
        CHECK(chrono_test::rows_of(a, t) == chrono_test::rows_of(b, t));
    }
}

}  // TEST_SUITE

TEST_SUITE("relations-slow" * doctest::skip()) {

TEST_CASE("P5-03 23: every other world case of p5-03-relations.txt") {
    for (const RCase& c : p503api::relations_golden()) {
        if (!c.fixture() && c.size > 512) check_case(c);
    }
}

TEST_CASE("P5-03 24.2: invariants on seeds 0 to 9 at 512, 0 to 19 at 1024 and high seeds 0 to 4 at 1024") {
    for (std::uint64_t seed = 0; seed < 10; ++seed) {
        CAPTURE(seed);
        CHECK(p503check::violations(p503run::world_rows(p503run::world_path(seed, 512))) == std::vector<std::string>{});
    }
    for (std::uint64_t seed = 0; seed < 20; ++seed) {
        CAPTURE(seed);
        CHECK(p503check::violations(p503run::world_rows(p503run::world_path(seed, 1024))) == std::vector<std::string>{});
    }
    for (std::uint64_t seed = 0; seed < 5; ++seed) {
        CAPTURE(seed);
        CHECK(p503check::violations(p503run::world_rows(p503run::world_path(seed, 1024, "high"))) ==
              std::vector<std::string>{});
    }
}

TEST_CASE("P5-03 7.4 I0 on seeds 2 to 4 at 512 and 0 to 4 at 1024") {
    for (const std::uint64_t seed : {2U, 3U, 4U}) {
        CAPTURE(seed);
        check_i0(inputs_of_seed(seed, 512), p503api::zero_content());
    }
    for (std::uint64_t seed = 0; seed < 5; ++seed) {
        CAPTURE(seed);
        check_i0(inputs_of_seed(seed, 1024), p503api::zero_content());
    }
}

}  // TEST_SUITE
