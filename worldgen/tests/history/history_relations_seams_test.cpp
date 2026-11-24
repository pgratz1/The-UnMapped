// Spec P5-03 (docs/specs/p5-03-race-relations.md r1) test 24.1 item 7: the disposition seam into conflict (7.1).
// Test views set with ConflictSubsystem::set_dispositions: a view that returns the camp value is 0.23.0's
// behaviour (no view); a view of friendship between every two races allows no enmity between races; a view of
// hatred opens enmity rows with the younger realm's founding as since_event (hatred_event 0); on relations-small
// with the subsystem's own view every enmity row's since_event is the younger's founding or a turn of the two
// peoples into hatred. camp_table() is history.conflict.camp.
//
// Written from the spec before the implementation. Library names: DispositionView, set_dispositions, camp_table
// (7.1) and the adapter's.

#include <doctest/doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "p5_03_relations_runs.hpp"

namespace {

namespace wd = worldgen::data;
namespace db = worldgen::db;

using p503api::FixedView;

struct Run {
    p503check::World w;
};

p503check::World with_view(std::uint64_t seed, const wd::ContentSet& c, const FixedView* view) {
    p503api::RelConfig::Options o;
    o.relations = false;
    o.ledger = false;
    o.other_view = view;
    return p503run::config_fixture(seed, c, o);
}

std::map<std::int64_t, std::int64_t> race_camps(const eng::HistoryInputs& in) {
    std::map<std::int64_t, std::int64_t> m;
    for (const auto& r : in.races) m[r.id] = r.camp;
    return m;
}

std::vector<const eng::Row*> enmities(const p503check::World& w) {
    std::vector<const eng::Row*> out;
    for (const eng::Row& r : w.rows.t.relation) {
        if (r[4].bytes == "enmity") out.push_back(&r);
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P5-03 7.1: camp_table() is history.conflict.camp") {
    const wd::ContentSet& c = conflict_test::shipped();
    const cfl::Conflict pair(c);
    const auto& t = pair.sub->camp_table();
    CHECK(t.of(0, 0) == p503check::camp_value(c, 0, 0));
    CHECK(t.of(0, 2) == p503check::camp_value(c, 0, 2));
    CHECK(t.of(2, 1) == p503check::camp_value(c, 1, 2));
    CHECK(t.of(2, 2) == p503check::camp_value(c, 2, 2));
}

TEST_CASE("P5-03 7.1: a view of the camp values (hatred_event 0) gives the history of no view") {
    const wd::ContentSet& c = conflict_test::stress_content();
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const auto camps = race_camps(p503run::stress_inputs(seed, c));
        const cfl::Conflict probe(c);
        const auto& table = probe.sub->camp_table();
        const FixedView camp([&](std::int64_t a, std::int64_t b) { return table.of(camps.at(a), camps.at(b)); });
        const p503check::World a = with_view(seed, c, &camp);
        const p503check::World b = with_view(seed, c, nullptr);
        CHECK(a.rows.t == b.rows.t);
    }
}

TEST_CASE("P5-03 7.1 (4.3): friendship between races: no enmity across races; hatred: since_event the founding") {
    const wd::ContentSet& c = conflict_test::stress_content();
    std::int64_t opened = 0;
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const auto camps = race_camps(p503run::stress_inputs(seed, c));
        const cfl::Conflict probe(c);
        const auto& table = probe.sub->camp_table();
        const FixedView friends([&](std::int64_t a, std::int64_t b) {
            return a == b ? table.of(camps.at(a), camps.at(b)) : std::int64_t{2000};
        });
        const p503check::World fw = with_view(seed, c, &friends);
        const p503check::Index fx(fw);
        for (const eng::Row* r : enmities(fw)) {
            CHECK(fx.civ_race(p503check::at(*r, 1)) == fx.civ_race(p503check::at(*r, 3)));
        }
        const FixedView foes([&](std::int64_t a, std::int64_t b) {
            return a == b ? table.of(camps.at(a), camps.at(b)) : std::int64_t{-2000};
        });
        const p503check::World hw = with_view(seed, c, &foes);
        const p503check::Index hx(hw);
        for (const eng::Row* r : enmities(hw)) {
            const std::int64_t a = p503check::at(*r, 1), b = p503check::at(*r, 3);
            if (hx.civ_race(a) == hx.civ_race(b)) continue;
            ++opened;
            const eng::Row& ca = *hx.civ.at(a);
            const eng::Row& cb = *hx.civ.at(b);
            const bool a_younger = p503check::at(ca, 6) > p503check::at(cb, 6) ||
                                   (p503check::at(ca, 6) == p503check::at(cb, 6) && a > b);
            CHECK(p503check::at(*r, 7) == p503check::at(a_younger ? ca : cb, 7));
        }
    }
    CHECK(opened > 0);
}

TEST_CASE("P5-03 7.1 (4.3): with the subsystem's view, an enmity's since_event is the founding or a hatred turn") {
    for (const std::uint64_t seed : {0U, 1U, 2U, 3U}) {
        CAPTURE(seed);
        const p503check::World w = p503run::stage_fixture(seed, p503api::relations_small_content());
        const p503check::Index x(w);
        for (const eng::Row* r : enmities(w)) {
            const std::int64_t a = p503check::at(*r, 1), b = p503check::at(*r, 3), since = p503check::at(*r, 7);
            const std::int64_t ra = x.civ_race(a), rb = x.civ_race(b);
            const std::int64_t younger_founding = std::max(p503check::at(*x.civ.at(a), 7), p503check::at(*x.civ.at(b), 7));
            CAPTURE(since);
            CHECK(since >= younger_founding);
            if (since == younger_founding || ra == rb) continue;
            CHECK(x.kind_of(since) == "disposition_turned");
            CHECK(x.part(since, 1) == std::min(ra, rb));
            CHECK(x.part(since, 2) == std::max(ra, rb));
            CHECK(x.val(since, 1) == p503api::HATRED);
        }
    }
}

}  // TEST_SUITE
