// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) 18 (P4-01 r5), test 24.1 item 12: Timeline::race_relation_at
// before, at and after each meeting and each turn, and settlement_races around each arrival, each capture and each
// phase start (drift), against 18's definitions applied to the stored rows, on the generated 512 world of seed 42
// (seeds 0 and 1 at 1024 in the suite relations-slow). Symmetry of race_relation_at; unmet pairs give met
// false, disposition and band 0; a settlement not standing at Y gives no shares.
//
// Written from the spec before the implementation. Library names: load_history (P4-01), race_relation_at,
// settlement_races, RaceRelationState, RaceShare (18; GUESS namespace scope, the adapter).

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

namespace h = worldgen::history;

struct Rows {
    std::vector<std::vector<std::int64_t>> rr, sr;
    std::map<std::pair<std::int64_t, std::int64_t>, std::int64_t> met_year;
    std::vector<std::int64_t> turn_years, arrival_years, capture_years, phase_starts;
    std::vector<std::int64_t> races;
    std::int64_t E = 0;
};

Rows rows_of(const db::WorldReader& r) {
    Rows out;
    out.E = std::stoll(r.meta("history_end_year").value_or("0"));
    for (const auto& x : chrono_test::rows_of(r, "race_relation")) {
        out.rr.push_back({x[0].integer, x[1].integer, x[2].integer, x[3].integer, x[4].integer});
    }
    for (const auto& x : chrono_test::rows_of(r, "settlement_race")) {
        out.sr.push_back({x[0].integer, x[1].integer, x[2].integer, x[3].integer, x[4].integer});
    }
    for (const auto& x : chrono_test::rows_of(r, "race")) {
        if (x[9].integer != 0) out.races.push_back(x[0].integer);
    }
    std::map<std::string, std::int64_t> kid;
    for (const auto& k : chrono_test::rows_of(r, "event_kind")) kid[k[1].bytes] = k[0].integer;
    std::map<std::int64_t, std::vector<std::int64_t>> parts;
    for (const auto& p : chrono_test::rows_of(r, "event_participant")) parts[p[0].integer].push_back(p[4].integer);
    for (const auto& e : chrono_test::rows_of(r, "event")) {
        const std::int64_t k = e[3].integer, y = e[1].integer;
        if (k == kid["peoples_met"]) out.met_year[{parts[e[0].integer].at(0), parts[e[0].integer].at(1)}] = y;
        if (k == kid["disposition_turned"]) out.turn_years.push_back(y);
        if (k == kid["refugees_arrived"]) out.arrival_years.push_back(y);
        if (k == kid["settlement_captured"]) out.capture_years.push_back(y);
    }
    for (const auto& p : chrono_test::rows_of(r, "phase")) out.phase_starts.push_back(p[3].integer);
    return out;
}

// 18: disposition at Y.
h::RaceRelationState want_relation(const Rows& w, std::int64_t a, std::int64_t b, std::int64_t y) {
    const std::int64_t lo = std::min(a, b), hi = std::max(a, b);
    h::RaceRelationState s{};
    s.race_a = lo;
    s.race_b = hi;
    s.met = false;
    s.disposition = 0;
    s.band = 0;
    const auto it = w.met_year.find({lo, hi});
    if (it == w.met_year.end() || it->second > y) return s;
    s.met = true;
    for (const auto& r : w.rr) {
        if (r[0] == lo && r[1] == hi && r[2] <= y) {
            s.disposition = r[3];
            s.band = r[4];
        }
    }
    return s;
}

// 18: shares at Y for a settlement standing at Y (the caller's test).
std::vector<std::vector<std::int64_t>> want_shares(const Rows& w, std::int64_t s, std::int64_t y) {
    std::map<std::int64_t, std::vector<std::int64_t>> last;
    for (const auto& r : w.sr) {
        if (r[0] == s && r[2] <= y) last[r[1]] = {r[1], r[3], r[4]};
    }
    std::vector<std::vector<std::int64_t>> out;
    for (const auto& [g, v] : last) {
        if (v[1] > 0) out.push_back(v);
    }
    return out;
}

void check_world(const std::string& path) {
    const db::WorldReader r = chrono_test::open_world(path);
    auto loaded = h::load_history(r);
    REQUIRE_MESSAGE(loaded.ok(), "load_history failed: " << (loaded.ok() ? "" : loaded.error().message));
    const h::Timeline& t = loaded.value().timeline;
    const Rows w = rows_of(r);
    std::set<std::int64_t> years{0, w.E};
    for (const auto& [p, y] : w.met_year) {
        for (const std::int64_t d : {-1, 0, 1}) years.insert(y + d);
    }
    for (const std::int64_t y : w.turn_years) {
        for (const std::int64_t d : {-1, 0, 1}) years.insert(y + d);
    }
    for (const std::int64_t y : years) {
        if (y < 0 || y > w.E) continue;
        CAPTURE(y);
        for (const std::int64_t a : w.races) {
            for (const std::int64_t b : w.races) {
                if (a >= b) continue;
                const h::RaceRelationState got = t.race_relation_at(a, b, y);
                const h::RaceRelationState sym = t.race_relation_at(b, a, y);
                const h::RaceRelationState want = want_relation(w, a, b, y);
                CHECK(got.race_a == want.race_a);
                CHECK(got.race_b == want.race_b);
                CHECK(got.met == want.met);
                CHECK(got.disposition == want.disposition);
                CHECK(got.band == want.band);
                CHECK(sym.met == got.met);
                CHECK(sym.disposition == got.disposition);
                CHECK(sym.race_a == got.race_a);
            }
        }
    }
    std::set<std::int64_t> syears{w.E};
    for (const auto* v : {&w.arrival_years, &w.capture_years, &w.phase_starts}) {
        for (const std::int64_t y : *v) {
            for (const std::int64_t d : {-1, 0}) syears.insert(y + d);
        }
    }
    for (const std::int64_t y : syears) {
        if (y < 0 || y > w.E) continue;
        CAPTURE(y);
        const h::YearState ys = t.year_state(y);
        for (const h::SettlementState& s : ys.settlements) {
            const bool standing = s.exists && (s.status == 1 || s.status == 2);
            const std::vector<h::RaceShare> got = t.settlement_races(s.settlement, y);
            CAPTURE(s.settlement);
            if (!standing) {
                CHECK(got.empty());
                continue;
            }
            const auto want = want_shares(w, s.settlement, y);
            REQUIRE(got.size() == want.size());
            std::int64_t sum = 0;
            for (std::size_t i = 0; i < got.size(); ++i) {
                CHECK(got[i].race == want[i][0]);
                CHECK(got[i].share == want[i][1]);
                CHECK(got[i].standing == want[i][2]);
                sum += got[i].share;
            }
            CHECK(sum == 1000);
        }
    }
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 18: race_relation_at and settlement_races against the rows (generated 512 world, seed 42)") {
    check_world(p503run::world_path(42, 512));
}

}  // TEST_SUITE

TEST_SUITE("relations-slow" * doctest::skip()) {

TEST_CASE("P5-03 18: race_relation_at and settlement_races against the rows (seeds 0 and 1 at 1024)") {
    check_world(p503run::world_path(0, 1024));
    check_world(p503run::world_path(1, 1024));
}

}  // TEST_SUITE
