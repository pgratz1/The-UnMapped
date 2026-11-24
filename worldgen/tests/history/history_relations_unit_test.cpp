// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) tests 24.1 items 2, 3, 4 and 6: the formulas of 6.2 and 6.3
// on values computed by hand from the spec (base with stances and the floor of a negative half sum, memory deltas
// and clamping, decay in ticks and quiet steps, truncation toward zero, the bands at a meeting and each turn of the
// hysteresis at its boundary, mixing with remainders and ties, conquest, drift and fade, standing targets, the
// group split), and the rules on the rows of the fixture relations-small (24.3; every kind of part A happens):
// meetings, turns and their causes, shares, standings, mixed events once per (settlement, race), expulsions with
// the majority guard; meet_hops -1 and 0; the DispositionView of a run (7.1).
//
// Written from the spec before the implementation (test-writer rule). Library names: the adapter's
// (p5_03_relations_api.hpp).

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_formulas.hpp"
#include "p5_03_relations_runs.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

using rel_f::Shares;
namespace h = worldgen::history;

const std::vector<std::uint64_t>& stress_seeds() {
    static const std::vector<std::uint64_t> v = {0U, 1U, 2U, 3U};
    return v;
}

std::int64_t count_kind(const p503check::World& w, const std::string& key) {
    const p503check::Index x(w);
    return static_cast<std::int64_t>(x.events_of(key).size());
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 6.2.1: B with stances; the floor of a negative half sum; D truncated toward zero") {
    // the shipped bases of 6.2.1: elf-faerie good_good 400 + (200 + 200) / 2; dwarf-goblin neutral_evil -250 - 100
    CHECK(rel_f::base(400, 200, 200) == 600);
    CHECK(rel_f::base(-250, -100, -100) == -350);
    CHECK(rel_f::base(100, -1, 0) == 99);    // floor_div(-1, 2) = -1, not 0
    CHECK(rel_f::base(100, 1, 0) == 100);
    CHECK(rel_f::base(100, -3, 0) == 98);
    CHECK(rel_f::disposition(100, -1500) == 99);   // -1.5 units truncate to -1
    CHECK(rel_f::disposition(100, -999) == 100);
    CHECK(rel_f::disposition(100, 1999) == 101);
    CHECK(rel_f::disposition(-350, -250000) == -600);
}

TEST_CASE("P5-03 6.2.2: memory deltas, clamping; decay in ticks and quiet steps, never across zero") {
    CHECK(rel_f::move_memory(0, -25, -250, 250) == -25000);
    CHECK(rel_f::move_memory(-249000, -25, -250, 250) == -250000);   // clamped
    CHECK(rel_f::move_memory(249000, 15, -250, 250) == 250000);
    CHECK(rel_f::decay(-25000, 1, 40) == -24600);   // a tick year: 400 milli-units at the shipped rate
    CHECK(rel_f::decay(25000, 1, 40) == 24600);
    CHECK(rel_f::decay(-25000, 30, 40) == -13000);  // a quiet step of 30 years: 30 ticks' worth
    CHECK(rel_f::decay(-300, 1, 40) == 0);          // toward 0, not across
    CHECK(rel_f::decay(300, 1, 40) == 0);
    std::int64_t m = -25000;
    for (int i = 0; i < 30; ++i) m = rel_f::decay(m, 1, 40);
    CHECK(m == rel_f::decay(-25000, 30, 40));   // ticks and quiet steps decay alike
}

TEST_CASE("P5-03 6.2.3: the band at a meeting; each turn of the hysteresis at its boundary") {
    const std::int64_t h0 = -500, f0 = 150, m0 = 50;
    CHECK(rel_f::plain_band(-500, h0, f0) == rel_f::HATRED);
    CHECK(rel_f::plain_band(-499, h0, f0) == rel_f::WARINESS);
    CHECK(rel_f::plain_band(150, h0, f0) == rel_f::FRIENDSHIP);
    CHECK(rel_f::plain_band(149, h0, f0) == rel_f::WARINESS);
    CHECK(rel_f::next_band(1, -500, h0, f0, m0) == 0);    // 1 -> 0 at D <= h
    CHECK(rel_f::next_band(1, -499, h0, f0, m0) == 1);
    CHECK(rel_f::next_band(0, -450, h0, f0, m0) == 0);    // 0 -> 1 only above h + m
    CHECK(rel_f::next_band(0, -449, h0, f0, m0) == 1);
    CHECK(rel_f::next_band(1, 150, h0, f0, m0) == 2);     // 1 -> 2 at D >= f
    CHECK(rel_f::next_band(1, 149, h0, f0, m0) == 1);
    CHECK(rel_f::next_band(2, 100, h0, f0, m0) == 2);     // 2 -> 1 only below f - m
    CHECK(rel_f::next_band(2, 99, h0, f0, m0) == 1);
    CHECK(rel_f::next_band(0, 2000, h0, f0, m0) == 1);    // one step per test
    CHECK(rel_f::next_band(2, -2000, h0, f0, m0) == 1);
}

TEST_CASE("P5-03 6.3.2: mixing with largest remainders, ties by race id; P_b = 0 gives G") {
    CHECK(rel_f::mix({{1, 1000}}, 300, {{2, 1000}}, 100) == Shares{{1, 750}, {2, 250}});
    CHECK(rel_f::mix({{1, 1000}}, 1, {{2, 1000}}, 2) == Shares{{1, 333}, {2, 667}});   // remainder 2 before 1
    CHECK(rel_f::mix({{1, 1000}}, 1, {{2, 500}, {3, 500}}, 2) == Shares{{1, 334}, {2, 333}, {3, 333}});   // tie
    CHECK(rel_f::mix({{4, 1000}}, 0, {{2, 600}, {5, 400}}, 77) == Shares{{2, 600}, {5, 400}});
    CHECK(rel_f::mix({{1, 999}, {2, 1}}, 1000, {{1, 1000}}, 1000000) == Shares{{1, 1000}});   // a share of 0 drops
}

TEST_CASE("P5-03 6.3.3: conquest: the conquerors settle, the others scaled; 6.3.4 drift and fade") {
    CHECK(rel_f::conquest_share(0, 100) == 100);
    CHECK(rel_f::conquest_share(200, 100) == 280);
    CHECK(rel_f::conquer({{1, 1000}}, 2, 100) == Shares{{1, 900}, {2, 100}});
    CHECK(rel_f::conquer({{1, 500}, {2, 300}, {3, 200}}, 3, 100) == Shares{{1, 450}, {2, 270}, {3, 280}});
    CHECK(rel_f::drift({{1, 700}, {2, 300}}, 1, 100, 500, 20) == Shares{{1, 850}, {2, 150}});
    CHECK(rel_f::drift({{1, 985}, {2, 15}}, 1, 100, 1, 20) == Shares{{1, 1000}});   // below fade: absorbed
    CHECK(rel_f::drift({{1, 600}, {2, 400}}, 1, 100, 100000, 20) == Shares{{1, 1000}});   // min(share, ...)
}

TEST_CASE("P5-03 6.3.5: standing targets") {
    CHECK(rel_f::target_standing(true, 199, 200, rel_f::FRIENDSHIP) == rel_f::SUBJECT);
    CHECK(rel_f::target_standing(true, 200, 200, rel_f::FRIENDSHIP) == rel_f::ACCEPTED);
    CHECK(rel_f::target_standing(false, 0, 200, rel_f::HATRED) == rel_f::SUBJECT);
    CHECK(rel_f::target_standing(false, 0, 200, rel_f::WARINESS) == rel_f::TOLERATED);
}

TEST_CASE("P5-03 6.4.3 item 1: groups split by the shares, left-over by remainder then race; small groups lost") {
    CHECK(rel_f::split_groups(1000, {{1, 333}, {2, 333}, {3, 334}}, 40) == Shares{{1, 333}, {2, 333}, {3, 334}});
    CHECK(rel_f::split_groups(100, {{1, 333}, {2, 333}, {3, 334}}, 1) == Shares{{1, 33}, {2, 33}, {3, 34}});
    CHECK(rel_f::split_groups(101, {{1, 500}, {2, 500}}, 1) == Shares{{1, 51}, {2, 50}});   // tie: race 1
    CHECK(rel_f::split_groups(100, {{1, 333}, {2, 333}, {3, 334}}, 40) == Shares{});
    CHECK(rel_f::room(500, 900) == 100);
    CHECK(rel_f::room(500, 1200) == 0);
}

TEST_CASE("P5-03 24.3: relations-small: every kind of part A happens; the invariants hold on the rows") {
    std::set<std::string> seen;
    // Stress seed 7 added at integration: with the calibrated data standing_changed happens on relations-small-7
    // only among the golden file's fixture seeds (p5-03-relations.txt: 2 events).
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U, 7U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, p503api::relations_small_content());
        CHECK(p503check::violations(w) == std::vector<std::string>{});
        for (const auto& [id, key] : p503api::new_kinds()) {
            if (id >= 57 && count_kind(w, key) > 0) seen.insert(key);
        }
        MESSAGE("relations-small " << seed << ": " << p503check::rr_line(w) << "; " << p503check::mx_line(w));
    }
    CHECK(seen == std::set<std::string>{"peoples_met", "disposition_turned", "refugees_arrived", "settlement_mixed",
                                        "standing_changed", "people_expelled"});
}

TEST_CASE("P5-03 6.2.4: meet_hops -1: no pair ever meets, no row; meet_hops 0: presences share a region") {
    const wd::ContentSet never =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"meet_hops", p503api::V(-1)}});
    const wd::ContentSet same =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"meet_hops", p503api::V(0)}});
    for (const std::uint64_t seed : stress_seeds()) {
        CAPTURE(seed);
        const p503check::World a = p503run::stage_fixture(seed, never);
        CHECK(count_kind(a, "peoples_met") == 0);
        CHECK(count_kind(a, "disposition_turned") == 0);
        CHECK(a.rows.race_relation.empty());
        const p503check::World b = p503run::stage_fixture(seed, same);
        CHECK(p503check::violations(b) == std::vector<std::string>{});
        const p503check::Index x(b);
        // 6.2.4: the region is the younger realm's presence and the cause its founding (from the rows)
        for (const std::int64_t e : x.events_of("peoples_met")) {
            const std::vector<std::int64_t> cs = x.cause_list(e);
            REQUIRE(cs.size() == 1U);
            std::int64_t younger = 0;
            for (const auto& [c, row] : x.civ) {
                if (p503check::at(*row, 7) == cs[0]) younger = c;
            }
            CHECK(younger != 0);
            const std::int64_t ry = x.civ_race(younger);
            CHECK((ry == x.part(e, 1) || ry == x.part(e, 2)));
        }
    }
}

TEST_CASE("P5-03 6.2.3: turns' causes are the pair's contributions (kinds of 6.2.2 naming both races) or its fallback") {
    static const std::set<std::string> moving = {"war_won", "peace", "truce", "civ_conquered", "sacked",
                                                 "settlement_captured", "people_expelled", "refugees_arrived"};
    for (const std::uint64_t seed : stress_seeds()) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, p503api::relations_small_content());
        const p503check::Index x(w);
        for (const std::int64_t e : x.events_of("disposition_turned")) {
            const std::vector<std::int64_t> cs = x.cause_list(e);
            REQUIRE_FALSE(cs.empty());
            const bool fallback = cs.size() == 1U && (x.kind_of(cs[0]) == "disposition_turned" ||
                                                      x.kind_of(cs[0]) == "peoples_met");
            if (fallback) continue;
            for (const std::int64_t c : cs) {
                CAPTURE(c);
                CHECK(moving.count(x.kind_of(c)) == 1U);
                CHECK(c < e);
            }
        }
        // an expulsion's causes: the entry's since_event and, when later, the turn into hatred; ascending (6.5)
        for (const std::int64_t e : x.events_of("people_expelled")) {
            const std::vector<std::int64_t> cs = x.cause_list(e);
            CHECK((cs.size() == 1U || cs.size() == 2U));
            if (cs.size() == 2U) CHECK(x.kind_of(cs[1]) == "disposition_turned");
        }
    }
}

TEST_CASE("P5-03 7.1: the subsystem's DispositionView after a run: symmetric, D(a, a) the camp, the last rows") {
    const wd::ContentSet& c = p503api::relations_small_content();
    const eng::HistoryInputs in = p503run::stress_inputs(1, c);
    const p503api::RelConfig cfg(c, {});
    const p503check::World w = p503check::world_of_run(cfg.run(c, in), in, c);
    const p503check::Index x(w);
    const auto prows = p503check::pair_rows(w);
    const h::DispositionView& v = *cfg.relations;
    for (const p503check::RaceInfo& a : w.races) {
        if (!a.present) continue;
        CHECK(v.disposition(a.id, a.id) == p503check::camp_value(c, a.camp, a.camp));
        for (const p503check::RaceInfo& b : w.races) {
            if (!b.present || b.id <= a.id) continue;
            CAPTURE(a.id);
            CAPTURE(b.id);
            CHECK(v.disposition(a.id, b.id) == v.disposition(b.id, a.id));
            const auto it = prows.find({a.id, b.id});
            if (it == prows.end()) {
                CHECK(v.disposition(a.id, b.id) == p503check::base_of(w, x, a.id, b.id));   // before a meeting D = B
            } else {
                CHECK(v.disposition(a.id, b.id) == p503check::at(*it->second.back(), 3));   // finish's row
            }
            std::int64_t hatred = 0;
            for (const std::int64_t e : x.events_of("disposition_turned")) {
                if (x.part(e, 1) == a.id && x.part(e, 2) == b.id && x.val(e, 1) == p503api::HATRED) hatred = e;
            }
            CHECK(v.hatred_event(a.id, b.id) == hatred);
            CHECK(v.hatred_event(b.id, a.id) == hatred);
        }
    }
}

TEST_CASE("P5-03 6.3.6, 6.5: mixed events once; expulsions under the majority guard; none with expel_permille 0") {
    const wd::ContentSet none =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"expel_permille", p503api::V(0)}});
    const wd::ContentSet guard0 =
        p503api::with_history(p503api::relations_small_content(), "relations", {{"expel_share_max", p503api::V(0)}});
    for (const std::uint64_t seed : stress_seeds()) {
        CAPTURE(seed);
        CHECK(count_kind(p503run::stage_fixture(seed, none), "people_expelled") == 0);
        CHECK(count_kind(p503run::stage_fixture(seed, guard0), "people_expelled") == 0);   // every share is > 0
        const wd::ContentSet never_mixed = p503api::with_history(p503api::relations_small_content(), "relations",
                                                                 {{"mixed_min_permille", p503api::V(1001)}});
        CHECK(count_kind(p503run::stage_fixture(seed, never_mixed), "settlement_mixed") == 0);
    }
}

}  // TEST_SUITE
