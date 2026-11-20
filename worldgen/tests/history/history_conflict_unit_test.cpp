// Spec P3-06 16.1 and 16.2 item 17 through the public seam (13): the
// subsystem's steps (T2, T6, Q4) and the shipped configuration (8.2, N1:
// peoples, then conflict, conflict the one tension source: the shipped
// stage equals that configuration run by hand); the fatal messages of 16.4
// (tuning, exact texts); the tension (5.4) read from Q4 by a probe equals
// the golden tension of every quiet step and is a pure read (8.3 item 2);
// draw independence (14 item 9, Y3: test-only draws on the five streams
// before the subsystem change nothing); Y4 with conflict installed
// (relations opened in two orders in one step give the same rows); and the
// guards: with last_refuge 0 and no shelter (the golden cases norefuge-*) a
// race dies out where the shipped guards keep every race (7.3 item 7, 7.2).
//
// Written from the spec only; empty until the implementation's header
// exists (p3_06_conflict_api.hpp).

#include "p3_06_conflict_support.hpp"

#if UNMAPPED_HAVE_PEOPLES && UNMAPPED_HAVE_CONFLICT

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "worldgen/core/streams.hpp"

namespace {

using namespace conflict_test;
using eng::Step;

// The FatalError of configuring and running one small history (syn-0's inputs) with content c: the tuning
// is checked at configuration or in the first step (GUESS in p3_06_conflict_api.hpp).
std::string configure_error(const wd::ContentSet& c) {
    return fatal_of([&] {
        ConflictConfig cfg(c);
        (void)eng::run_engine(c, synthetic_inputs(conflict_case("syn-0")), cfg.get());
    });
}

wd::ContentSet without_conflict(const wd::ContentSet& base) {
    wd::ContentSet c = base;
    for (wd::KindContent& k : c.kinds) {
        if (k.kind != "history") continue;
        std::vector<wd::Definition> keep;
        for (wd::Definition& d : k.definitions) {
            if (d.id != "conflict") keep.push_back(std::move(d));
        }
        k.definitions = std::move(keep);
    }
    return c;
}

wd::Value record_with(const wd::Record& r, const std::string& field, std::int64_t v) {
    wd::Record out;
    for (std::size_t i = 0; i < r.size(); ++i) {
        out.add(r.name_at(i), r.name_at(i) == field ? wd::Value::integer(v) : r.value_at(i));
    }
    return wd::Value::record(std::move(out));
}
const wd::Record& shipped_conflict_record() {
    for (const wd::KindContent& k : shipped().kinds) {
        if (k.kind != "history") continue;
        for (const wd::Definition& d : k.definitions) {
            if (d.id == "conflict") return d.record;
        }
    }
    FAIL("no history.conflict");
    return shipped().kinds.front().definitions.front().record;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-06 8.2, 8.3 item 3 (N1): steps T2, T6, Q4; the shipped stage is peoples then conflict") {
    ConflictConfig cfg(shipped());
    std::vector<Step> steps(cfg.pair->sub->steps().begin(), cfg.pair->sub->steps().end());
    CHECK(steps == std::vector<Step>{Step::t2, Step::t6, Step::q4});
    for (const char* name : {"syn-0", "syn-5"}) {
        const CCase& g = conflict_case(name);
        CAPTURE(g.name);
        const eng::HistoryInputs in = synthetic_inputs(g);
        ConflictConfig c(shipped());
        const Tables a = eng::run_engine(shipped(), in, c.get());
        CHECK(a == eng::history_stage(shipped(), in));
        CHECK(table_hashes(a) == g.tables);
    }
}

TEST_CASE("P3-06 16.4: the tuning messages, exact texts") {
    CHECK(configure_error(without_conflict(shipped())) ==
          "history: tuning: the content has no definition history.conflict");
    CHECK(configure_error(with_conflict({{"march_hops", I64(5)}})) ==
          "history: tuning: conflict.march_hops outside its schema range");
    CHECK(configure_error(with_conflict({{"last_refuge", I64(2)}})) ==
          "history: tuning: conflict.last_refuge outside its schema range");
    const wd::Record& r = shipped_conflict_record();
    CHECK(configure_error(with_conflict({{"levy_permille", record_with(r.get_record("levy_permille"), "city", 1001)}})) ==
          "history: tuning: conflict.levy_permille.city outside its schema range");
    CHECK(configure_error(with_conflict({{"camp", record_with(r.get_record("camp"), "good_evil", -1001)}})) ==
          "history: tuning: conflict.camp.good_evil outside its schema range");
    CHECK(configure_error(with_conflict({{"claim_strain", I64(301)}})) ==
          "history: tuning: claim_strain must not be above declare_min");
    CHECK(configure_error(with_conflict({{"strain_max", I64(899)}})) ==
          "history: tuning: declare_min + declare_span must not be above strain_max");
    // the order of 12.1: a range error before a named check
    CHECK(configure_error(with_conflict({{"claim_strain", I64(301)}, {"march_hops", I64(0)}})) ==
          "history: tuning: conflict.march_hops outside its schema range");
    CHECK(configure_error(shipped()) == "<no error>");
}

TEST_CASE("P3-06 5.4, 8.3 item 2, 16.1 item 5: the tension read in Q4 equals the golden tension; it is a pure read") {
    for (const char* name : {"syn-1", "syn-5", "stress-0", "stress-5", "stress-7"}) {
        const CCase& g = conflict_case(name);
        CAPTURE(g.name);
        const wd::ContentSet& c = content_of(g);
        ConflictConfig cfg(c);
        ConflictConfig* p = &cfg;
        std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> seen;
        cfg.add_after({Step::q4}, [p, &seen](eng::StepContext& ctx) {
            const eng::TensionReport a = p->pair->tension()->tension(ctx);
            const std::size_t rel = ctx.state().relations().size();
            const eng::TensionReport b = p->pair->tension()->tension(ctx);
            CHECK(a.value == b.value);
            CHECK(a.trigger_event == b.trigger_event);
            CHECK(ctx.state().relations().size() == rel);
            seen[ctx.year()] = {a.value, a.trigger_event};
        });
        const Tables t = eng::run_engine(c, synthetic_inputs(g), cfg.get());
        CHECK(table_hashes(t) == g.tables);  // the probe changes nothing
        for (const auto& [start, vt] : g.tension) {
            CAPTURE(start);
            REQUIRE(seen.count(start) == 1U);
            CHECK(seen.at(start) == vt);
        }
    }
}

TEST_CASE("P3-06 14 item 9 (Y3): test-only draws on the five hist.conflict streams change nothing") {
    for (const char* name : {"syn-1", "stress-5"}) {
        const CCase& g = conflict_case(name);
        CAPTURE(g.name);
        const wd::ContentSet& c = content_of(g);
        const eng::HistoryInputs in = synthetic_inputs(g);
        std::vector<std::uint64_t> keys;
        for (const std::string_view s : cfl::stream_names()) keys.push_back(worldgen::stream_key(in.seed, s));
        ConflictConfig cfg(c);
        std::uint64_t sink = 0;
        cfg.add_between({Step::t2, Step::t6, Step::q4}, [keys, &sink](eng::StepContext& ctx) {
            for (std::size_t i = 0; i < keys.size(); ++i) {
                for (std::uint64_t a = 1; a <= 4; ++a) {
                    sink ^= ctx.draw(keys[i], a, a + 1, 7);
                }
            }
        });
        CHECK(table_hashes(eng::run_engine(c, in, cfg.get())) == g.tables);
        CHECK(sink != 1U);
    }
}

TEST_CASE("P3-06 16.2 item 17 (Y4): relations opened in two orders in one step give the same rows, with conflict") {
    const CCase& g = conflict_case("syn-5");
    const eng::HistoryInputs in = synthetic_inputs(g);
    std::vector<Tables> runs;
    for (int order = 0; order < 2; ++order) {
        ConflictConfig cfg(shipped());
        cfg.add_after({Step::t6}, [order](eng::StepContext& ctx) {
            if (ctx.state().civs().size() < 2U || ctx.year() % 97 != 0) return;
            const std::int64_t e = 1;
            const eng::RelationKey k1{3, 1, 3, 2, "test_bond"}, k2{3, 2, 3, 1, "test_feud"};
            const bool open1 = ctx.state().relations().count(k1) != 0, open2 = ctx.state().relations().count(k2) != 0;
            if (open1 || open2) {
                if (order == 0) {
                    if (open1) ctx.state().close_relation(k1, e);
                    if (open2) ctx.state().close_relation(k2, e);
                } else {
                    if (open2) ctx.state().close_relation(k2, e);
                    if (open1) ctx.state().close_relation(k1, e);
                }
                return;
            }
            if (order == 0) {
                ctx.state().open_relation(k1, e);
                ctx.state().open_relation(k2, e);
            } else {
                ctx.state().open_relation(k2, e);
                ctx.state().open_relation(k1, e);
            }
        });
        runs.push_back(eng::run_engine(shipped(), in, cfg.get()));
    }
    CHECK(runs[0].relation == runs[1].relation);
    CHECK(runs[0] == runs[1]);
    std::size_t test_rows = 0;
    for (const Row& r : runs[0].relation) test_rows += r[4].bytes.rfind("test_", 0) == 0 ? 1U : 0U;
    CHECK(test_rows > 0U);
}

TEST_CASE("P3-06 7.3 item 7, 7.2, 9, question 4: without last_refuge and shelter a race dies out; with them none") {
    std::size_t extinct_cases = 0, n = 0;
    for (const CCase& g : conflict_golden()) {
        if (g.kind != "norefuge") continue;
        ++n;
        CAPTURE(g.name);
        const wd::ContentSet& c = content_of(g);
        const eng::HistoryInputs in = synthetic_inputs(g);
        const Tables t = eng::history_stage(c, in);
        CHECK(table_hashes(t) == g.tables);
        check_invariants_06(t, in, 25, false);
        bool extinct = false;
        for (const db::AgeRow& a : in.ages) {
            bool alive = false;
            for (const Row& civ : t.civilization) alive = alive || (at(civ, 2) == a.race && is_null(civ[8]));
            extinct = extinct || !alive;
        }
        extinct_cases += extinct ? 1U : 0U;
    }
    CHECK(n >= 1U);
    CHECK(extinct_cases >= 1U);
}

}  // TEST_SUITE

#endif  // UNMAPPED_HAVE_PEOPLES && UNMAPPED_HAVE_CONFLICT
