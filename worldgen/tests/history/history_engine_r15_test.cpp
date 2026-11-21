// Spec P3-04 r15 20.1 item 1a and 20.6 item 00 (P3-10 second round, docs/specs/p3-10-followups.md section 12 item
// 23; generator stays 0.19.0): no quiet_stretch for a stretch shorter than history.engine.quiet_min_years (50),
// decided in the stretch's first step from its assured length; the named check engine_quiet_min
// (quiet_min_years <= 2 x quiet_step_years) and the field's range int(0, 400).
//
// The stub engine (make_stub, no subsystem) on hand-made inputs (inputs_for: two races, ages at the given years,
// span 6000, normal: the opening of age 1 is 0-149, so a second age at 150 + L leaves one quiet row 150 to
// 149 + L): rows of 49 and 50 years, one full step and a 10-year step cut by an awakening (35 years), a 25-year
// step followed by a flare-up (ForcedTension(25, 5000, 1)), quiet_step_years 50, and quiet_min_years 0 (every
// row has one, as at r14). The invariant of 20.6 item 00 on every run here. The expected rows are written from the
// spec by hand; the reference (reference/p3_engine_reference.py check_quiet_r15) checks the same cases and an
// exactness sweep over many schedules.
//
// Written from the spec before the implementation (test-writer rule). It needs no new API: the content carries
// quiet_min_years (with_engine_r15), which the first round's library ignores, so the short-row cases fail there.

#include <doctest/doctest.h>

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "p3_04_engine_support.hpp"

namespace {

using namespace engine_test;

using Stretch = std::tuple<std::int64_t, std::int64_t, std::int64_t>;  // year, end_year, years

struct QuietRun {
    std::vector<std::pair<std::int64_t, std::int64_t>> rows;  // quiet phase rows (start, end)
    std::map<std::int64_t, Stretch> stretch;                  // phase id -> its quiet_stretch
    std::map<std::int64_t, std::int64_t> row_of_phase;        // phase id -> index in rows
    Tables t;
};

QuietRun quiet_run(const std::vector<std::int64_t>& starts, std::int64_t q, std::int64_t qmin,
                   std::int64_t tension_period = 0) {
    EngineTuning tun;
    tun.quiet_step_years = q;
    tun.quiet_min_years = qmin;
    const wd::ContentSet c = with_engine_r15(tun);
    const eng::HistoryInputs in = inputs_for(starts, 6000);
    Config cfg;
    cfg.handler = eng::make_stub();
    cfg.catalogue = eng::catalogue_of(c);
    if (tension_period > 0) cfg.add_tension(eng::forced_tension(tension_period, 5000, 1));
    QuietRun r;
    r.t = eng::run_engine(c, in, cfg.get());
    std::int64_t k38 = 0;
    for (const Row& k : r.t.event_kind) {
        if (k.at(1).bytes == "quiet_stretch") k38 = at(k, 0);
    }
    REQUIRE(k38 == 38);
    for (const Row& ph : r.t.phase) {
        if (at(ph, 2) != 2) continue;
        r.row_of_phase[at(ph, 0)] = static_cast<std::int64_t>(r.rows.size());
        r.rows.emplace_back(at(ph, 3), at(ph, 4));
    }
    std::map<std::int64_t, std::int64_t> years;
    for (const Row& v : r.t.event_value) {
        if (at(v, 1) == 1) years[at(v, 0)] = at(v, 2);
    }
    for (const Row& e : r.t.event) {
        if (at(e, 3) != k38) continue;
        CHECK_MESSAGE(r.stretch.count(at(e, 4)) == 0U, "two quiet_stretch events in phase " << at(e, 4));
        r.stretch[at(e, 4)] = Stretch{at(e, 1), at(e, 2), years[at(e, 0)]};
    }
    // 20.6 item 00's invariant: every quiet_stretch has years >= quiet_min_years; every quiet row at least
    // quiet_min_years long (a civilization is alive at every row here: the stub founds one per awakening) has one,
    // spanning the row
    for (const auto& [phase, st] : r.stretch) {
        CHECK(std::get<2>(st) >= qmin);
    }
    for (const auto& [phase, i] : r.row_of_phase) {
        const auto [a, b] = r.rows[static_cast<std::size_t>(i)];
        CAPTURE(a);
        if (b - a + 1 >= qmin) {
            REQUIRE(r.stretch.count(phase) == 1U);
            CHECK(r.stretch.at(phase) == Stretch{a, b, b - a + 1});
        } else {
            CHECK(r.stretch.count(phase) == 0U);
        }
    }
    return r;
}

// The quiet_stretch of the row that starts in `year`, if any.
bool has_stretch_at(const QuietRun& r, std::int64_t year) {
    for (const auto& [phase, st] : r.stretch) {
        if (std::get<0>(st) == year) return true;
    }
    return false;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 r15 20.1 item 1a: quiet rows of 49 and 50 years (Q 25): none and one, spanning the row") {
    const QuietRun r49 = quiet_run({0, 199}, 25, 50);
    REQUIRE_FALSE(r49.rows.empty());
    CHECK(r49.rows.front() == std::pair<std::int64_t, std::int64_t>{150, 198});
    CHECK_FALSE(has_stretch_at(r49, 150));
    const QuietRun r50 = quiet_run({0, 200}, 25, 50);
    REQUIRE_FALSE(r50.rows.empty());
    CHECK(r50.rows.front() == std::pair<std::int64_t, std::int64_t>{150, 199});
    CHECK(has_stretch_at(r50, 150));
    // with quiet_min_years 0 the 49-year row has its line, as at r14
    CHECK(has_stretch_at(quiet_run({0, 199}, 25, 0), 150));
}

TEST_CASE("P3-04 r15 20.6 item 00: one full step and one of 10 years cut by an awakening (35 years): no line") {
    const QuietRun r = quiet_run({0, 185}, 25, 50);
    REQUIRE_FALSE(r.rows.empty());
    CHECK(r.rows.front() == std::pair<std::int64_t, std::int64_t>{150, 184});
    CHECK_FALSE(has_stretch_at(r, 150));
}

TEST_CASE("P3-04 r15 20.6 item 00: a row of one 25-year step followed by a flare-up has no line; quiet_min 0 has") {
    // ForcedTension(25, 5000, 1): every quiet step's tension starts a flare-up, so every quiet row before the
    // final phase is one step of 25 years
    const QuietRun r = quiet_run({0, 3000}, 25, 50, 25);
    std::size_t short_rows = 0;
    for (const auto& [a, b] : r.rows) short_rows += b - a + 1 == 25 ? 1U : 0U;
    CHECK(short_rows >= 5U);
    for (const auto& [a, b] : r.rows) {
        if (b - a + 1 == 25) CHECK_FALSE(has_stretch_at(r, a));
    }
    const QuietRun r0 = quiet_run({0, 3000}, 25, 0, 25);
    CHECK(r0.stretch.size() == r0.rows.size());
}

TEST_CASE("P3-04 r15 20.6 item 00: quiet_step_years 50: rows of 25, 35, 49 and 50 years") {
    for (const auto& [second, want] : std::vector<std::pair<std::int64_t, bool>>{
             {175, false}, {185, false}, {199, false}, {200, true}}) {
        CAPTURE(second);
        const QuietRun r = quiet_run({0, second}, 50, 50);
        REQUIRE_FALSE(r.rows.empty());
        CHECK(r.rows.front() == std::pair<std::int64_t, std::int64_t>{150, second - 1});
        CHECK(has_stretch_at(r, 150) == want);
    }
    // quiet_min_years 100 = 2 x Q: 99 years none, 100 years one
    CHECK_FALSE(has_stretch_at(quiet_run({0, 249}, 50, 100), 150));
    CHECK(has_stretch_at(quiet_run({0, 250}, 50, 100), 150));
}

TEST_CASE("P3-04 r15 4.1: engine_quiet_min and the range of quiet_min_years, with their messages") {
    const eng::HistoryInputs in = inputs_for({0, 2074}, 6000);
    auto stage_error = [&](const wd::ContentSet& c) { return fatal_of([&] { (void)eng::history_stage(c, in); }); };
    EngineTuning t;
    t.quiet_min_years = 50;  // = 2 x 25: passes
    CHECK(stage_error(with_engine_r15(t)) == "<no error>");
    t.quiet_min_years = 51;
    CHECK(stage_error(with_engine_r15(t)) == "history: tuning: quiet_min_years must not be above 2 x quiet_step_years");
    t.quiet_step_years = 200;
    t.quiet_min_years = 400;  // the top of int(0, 400), = 2 x 200
    CHECK(stage_error(with_engine_r15(t)) == "<no error>");
    t.quiet_min_years = 401;
    CHECK(stage_error(with_engine_r15(t)) == "history: tuning: quiet_min_years outside its schema range");
    t = EngineTuning{};
    t.quiet_min_years = -1;
    CHECK(stage_error(with_engine_r15(t)) == "history: tuning: quiet_min_years outside its schema range");
    // the named check comes after engine_simulated_bounds (4.1's order)
    t = EngineTuning{};
    t.quiet_min_years = 51;
    t.simulated_min_permille = 460;
    CHECK(stage_error(with_engine_r15(t)) ==
          "history: tuning: simulated_min_permille must not be above simulated_max_permille");
}

}  // TEST_SUITE
