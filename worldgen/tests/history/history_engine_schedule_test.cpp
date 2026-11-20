// Spec P3-04 section 5 (the scheduler), tests 16.1 items 1 to 3 and the
// schedule cases of 16.4: SC1 lengths at the five spans, every edge case
// E1-E10 on hand-made age lists with the phase table written here, the
// worked example of 17 and its two ForcedTension variants, every schedule
// line of worldgen/tests/golden/p3-04-engine.txt through run_engine, and
// the fatal guards of 3.4 and 5.4-6.5 with their messages. The engine is
// driven through run_engine with a test awakening handler that places
// nothing, so only the engine's awakening events exist.

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"

namespace {

using namespace engine_test;

// run_engine with no foundings, the given tension source (or none).
Tables schedule(const std::vector<std::int64_t>& starts, std::int64_t span, const std::string& detail,
                std::unique_ptr<eng::TensionSource> tension = nullptr, const wd::ContentSet& content = shipped()) {
    Config cfg;
    cfg.handler = std::make_unique<TestHandler>(false);
    if (tension) {
        cfg.add_tension(std::move(tension));
    }
    return eng::run_engine(content, inputs_for(starts, span, detail), cfg.get());
}

std::unique_ptr<eng::TensionSource> forced(std::int64_t p, std::int64_t v, std::int64_t t) {
    return eng::forced_tension(p, v, t);
}

// "ForcedTension(p,v,t)" or "none" (golden header token).
std::unique_ptr<eng::TensionSource> tension_of(const std::string& s) {
    if (s == "none") {
        return nullptr;
    }
    const std::string in = s.substr(s.find('(') + 1, s.find(')') - s.find('(') - 1);
    const std::vector<std::int64_t> v = ints(in);
    return forced(v.at(0), v.at(1), v.at(2));
}

// The schedule cases of the golden file selected by `pick`, through run_engine; returns how many ran.
std::size_t check_golden_scheds(const std::function<bool(const GoldenSched&)>& pick) {
    std::size_t n = 0;
    for (const GoldenSched& s : golden().scheds) {
        if (!pick(s)) {
            continue;
        }
        CAPTURE(s.header);
        REQUIRE(!s.starts.empty());
        const Tables t = schedule(s.starts, s.span, s.detail, tension_of(s.tension));
        CHECK(phases_text(t) == s.phases);
        CHECK(simulated(t) == s.simulated);
        // opening phases carry their awakening events (5.7), the others 0 or the tension's trigger; the
        // awakening of an opening phase is the event of kind 1 dated at its first year (flare_up events of
        // 6.3 take ids between them)
        for (const Row& r : t.phase) {
            if (at(r, 2) == 1) {
                std::int64_t aw = 0;
                for (const Row& e : t.event) {
                    if (at(e, 3) == 1 && at(e, 1) == at(r, 3)) {
                        aw = at(e, 0);
                    }
                }
                CHECK(aw != 0);
                CHECK(at(r, 5) == aw);
            } else if (at(r, 2) == 3) {
                CHECK(at(r, 5) == 1);
            } else {
                CHECK(at(r, 5) == 0);
            }
        }
        ++n;
    }
    return n;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 5.1, 16.1 item 1: SC1 scaled lengths at E = 2000, 2001, 6000, 11999, 12000 (the 5.5 table)") {
    // Six races spaced E/7 apart: no opening is cut and the floor does not
    // act, so every opening is A years and the final phase L years; with a
    // tension after every quiet step the first flare-up is F years.
    struct Want {
        std::string level;
        std::int64_t e, a, f, l;
    };
    const std::vector<Want> wants = {
        {"sparse", 2000, 25, 10, 83},   {"sparse", 2001, 25, 10, 83},   {"sparse", 6000, 75, 30, 250},
        {"sparse", 11999, 150, 60, 500}, {"sparse", 12000, 150, 60, 500}, {"normal", 2000, 50, 17, 100},
        {"normal", 2001, 50, 17, 100},  {"normal", 6000, 150, 50, 300}, {"normal", 11999, 300, 100, 600},
        {"normal", 12000, 300, 100, 600}, {"dense", 2000, 83, 25, 133},  {"dense", 2001, 83, 25, 133},
        {"dense", 6000, 250, 75, 400},  {"dense", 11999, 500, 150, 800}, {"dense", 12000, 500, 150, 800},
    };
    for (const Want& w : wants) {
        CAPTURE(w.level);
        CAPTURE(w.e);
        std::vector<std::int64_t> starts;
        for (std::int64_t k = 0; k < 6; ++k) {
            starts.push_back(k * (w.e / 7));
        }
        const Tables t = schedule(starts, w.e, w.level);
        for (const Row& r : t.phase) {
            if (at(r, 2) == 1) {
                CHECK(at(r, 4) - at(r, 3) + 1 == w.a);
            }
        }
        REQUIRE(at(t.phase.back(), 2) == 4);
        CHECK(at(t.phase.back(), 4) - at(t.phase.back(), 3) + 1 == w.l);
        CHECK(simulated(t) == 6 * w.a + w.l);
        const Tables f = schedule(starts, w.e, w.level, forced(1, 5000, 1));
        bool seen = false;
        for (const Row& r : f.phase) {
            if (at(r, 2) == 3 && !seen) {
                CHECK(at(r, 4) - at(r, 3) + 1 == w.f);
                seen = true;
            }
        }
        CHECK(seen);
    }
}

TEST_CASE("P3-04 5.6, 16.1 item 2: edge cases E1 to E10 on hand-made age lists") {
    struct Case {
        const char* name;
        std::vector<std::int64_t> starts;
        std::int64_t span;
        const char* level;
        std::int64_t period;  // ForcedTension(period, value, 1); 0 = none
        std::int64_t value;
        std::vector<IRow> phases;  // id, age, kind, start, end, trigger (trigger checked when given)
        std::int64_t simulated;
    };
    const std::vector<Case> cases = {
        // E1: 0, 100 with A = 150: the first opening ends at 100, no quiet between; the floor moves the
        // final phase back 50 years (550 -> 600).
        {"E1", {0, 100}, 6000, "normal", 0, 0,
         {{1, 1, 1, 0, 99, 1}, {2, 2, 1, 100, 249, 2}, {3, 2, 2, 250, 5649, 0}, {4, 2, 4, 5650, 5999, 0}}, 600},
        // E2: the quiet interval [150, 160) is one step of 10 years.
        {"E2", {0, 160, 3000}, 6000, "normal", 0, 0,
         {{1, 1, 1, 0, 149, 1}, {2, 1, 2, 150, 159, 0}, {3, 2, 1, 160, 309, 2}, {4, 2, 2, 310, 2999, 0},
          {5, 3, 1, 3000, 3149, 3}, {6, 3, 2, 3150, 5699, 0}, {7, 3, 4, 5700, 5999, 0}},
         750},
        // E3: the step [475, 500) asks; the flare-up is cut at the next awakening (20 years).
        {"E3", {0, 520}, 6000, "normal", 500, 1000, {}, 0},
        // E4: the last opening reaches past E - L: the final phase starts at its end (250 years).
        {"E4", {0, 3000, 5600}, 6000, "normal", 0, 0,
         {{1, 1, 1, 0, 149, 1}, {2, 1, 2, 150, 2999, 0}, {3, 2, 1, 3000, 3149, 2}, {4, 2, 2, 3150, 5599, 0},
          {5, 3, 1, 5600, 5749, 3}, {6, 3, 4, 5750, 5999, 0}},
         700},
        // E5: the last awakening at E - 1: one year, no final phase; the floor lengthens opening 1.
        {"E5", {0, 5999}, 6000, "normal", 0, 0,
         {{1, 1, 1, 0, 598, 1}, {2, 1, 2, 599, 5998, 0}, {3, 2, 1, 5999, 5999, 2}}, 600},
        // E6: E - y_P = 100: the last opening runs to E - 1 (5900-5998), the final phase is one year.
        {"E6", {0, 5900}, 6000, "normal", 0, 0,
         {{1, 1, 1, 0, 499, 1}, {2, 1, 2, 500, 5899, 0}, {3, 2, 1, 5900, 5998, 2}, {4, 2, 4, 5999, 5999, 0}}, 600},
        {"E6-2", {0, 5998}, 6000, "normal", 0, 0,
         {{1, 1, 1, 0, 597, 1}, {2, 1, 2, 598, 5997, 0}, {3, 2, 1, 5998, 5998, 2}, {4, 2, 4, 5999, 5999, 0}}, 600},
        // E7: elf,orc at seed 3 (0, 5602): 548 years raised to 600, the elves' opening is 202 years.
        {"E7", {0, 5602}, 6000, "normal", 0, 0,
         {{1, 1, 1, 0, 201, 1}, {2, 1, 2, 202, 5601, 0}, {3, 2, 1, 5602, 5751, 2}, {4, 2, 4, 5752, 5999, 0}}, 600},
        // E10: one race.
        {"E10", {0}, 6000, "normal", 0, 0, {{1, 1, 1, 0, 149, 1}, {2, 1, 2, 150, 5549, 0}, {3, 1, 4, 5550, 5999, 0}},
         600},
        {"E10 sparse 2000", {0}, 2000, "sparse", 0, 0,
         {{1, 1, 1, 0, 24, 1}, {2, 1, 2, 25, 1824, 0}, {3, 1, 4, 1825, 1999, 0}}, 200},
    };
    for (const Case& c : cases) {
        CAPTURE(c.name);
        const Tables t = schedule(c.starts, c.span, c.level, c.period ? forced(c.period, c.value, 1) : nullptr);
        if (std::string(c.name) == "E3") {
            REQUIRE(t.phase.size() >= 4U);
            CHECK(str(std::vector<Row>(t.phase.begin(), t.phase.begin() + 4)) ==
                  str(irows({{1, 1, 1, 0, 149, 1}, {2, 1, 2, 150, 499, 0}, {3, 1, 3, 500, 519, 1},
                             {4, 2, 1, 520, 669, 3}})));  // e2 is the flare-up's event (6.3), e3 the awakening
            continue;
        }
        CHECK(str(t.phase) == str(irows(c.phases)));
        CHECK(simulated(t) == c.simulated);
    }
    // E3 at the final phase: the step ending at 5675 asks; the flare-up is cut at S_f = 5700.
    {
        const Tables t = schedule({0, 4000}, 6000, "normal", forced(5675, 1000, 1));
        std::vector<Row> flares;
        for (const Row& r : t.phase) {
            if (at(r, 2) == 3) {
                flares.push_back(r);
            }
        }
        CHECK(str(flares) == str(irows({{5, 2, 3, 5675, 5699, 1}})));
    }
    // E3 at the interval's end: the step [500, 525) ends the interval [150, 525): no tension is asked.
    {
        const Tables t = schedule({0, 525}, 6000, "normal", forced(525, 1000, 1));
        for (const Row& r : t.phase) {
            CHECK_FALSE((at(r, 2) == 3 && at(r, 3) == 525));
        }
    }
    // E8, E9: a flare-up after every step that can have one: the first after one step, flare-ups until
    // the cap (2700), the next ones skipped.
    {
        const Tables t = schedule({0, 3000}, 6000, "normal", forced(25, 5000, 1));
        CHECK(str(std::vector<Row>(t.phase.begin() + 1, t.phase.begin() + 4)) ==
              str(irows({{2, 1, 2, 150, 174, 0}, {3, 1, 3, 175, 224, 1}, {4, 1, 2, 225, 249, 0}})));
        CHECK(simulated(t) == 2700);
        // every flare-up row lies between quiet rows of its interval (invariant 1)
        for (std::size_t i = 0; i < t.phase.size(); ++i) {
            if (at(t.phase[i], 2) == 3) {
                REQUIRE(i > 0);
                CHECK(at(t.phase[i - 1], 2) == 2);
            }
        }
    }
    // E8: the cap shortens the last flare-up: dense at 2000 (A 83, F 25, L 133, cap 900), one race,
    // a flare-up after every step (period 1: the quiet steps start at 83, so no step end is a multiple
    // of 25 and ForcedTension(25, ...) would never fire).
    {
        const Tables none = schedule({0}, 2000, "dense", forced(25, 5000, 1));
        CHECK(simulated(none) == 216);
        const Tables t = schedule({0}, 2000, "dense", forced(1, 5000, 1));
        CHECK(simulated(t) == 900);
        std::int64_t last = 0;
        for (const Row& r : t.phase) {
            if (at(r, 2) == 3) {
                last = at(r, 4) - at(r, 3) + 1;
            }
        }
        // base 83 + 133 = 216 (above the floor 200); room 684 = 27 x 25 + 9: the last flare-up is 9 years.
        CHECK(last == 9);
    }
}

TEST_CASE("P3-04 17: the worked example's schedule (seed 42, span 6000, normal) and both ForcedTension variants") {
    const std::vector<std::int64_t> starts = {0, 2074, 3242, 4243, 4969, 5596};
    const Tables t = schedule(starts, 6000, "normal");
    CHECK(phases_text(t, true) ==
          "1:1:1:0:149:1|2:1:2:150:2073:0|3:2:1:2074:2223:2|4:2:2:2224:3241:0|5:3:1:3242:3391:3|"
          "6:3:2:3392:4242:0|7:4:1:4243:4392:4|8:4:2:4393:4968:0|9:5:1:4969:5118:5|10:5:2:5119:5595:0|"
          "11:6:1:5596:5745:6|12:6:4:5746:5999:0");
    CHECK(simulated(t) == 1154);
    const Tables f = schedule(starts, 6000, "normal", forced(500, 1000, 1));
    CHECK(f.phase.size() == 20U);
    CHECK(simulated(f) == 1354);
    CHECK(str(std::vector<Row>(f.phase.begin() + 1, f.phase.begin() + 10)) ==
          str(irows({{2, 1, 2, 150, 499, 0}, {3, 1, 3, 500, 549, 1}, {4, 1, 2, 550, 999, 0}, {5, 1, 3, 1000, 1049, 1},
                     {6, 1, 2, 1050, 1499, 0}, {7, 1, 3, 1500, 1549, 1}, {8, 1, 2, 1550, 1999, 0},
                     {9, 1, 3, 2000, 2049, 1}, {10, 1, 2, 2050, 2073, 0}})));
    const Tables g = schedule(starts, 6000, "normal", forced(1, 5000, 1));
    std::int64_t flares = 0;
    for (const Row& r : g.phase) {
        flares += at(r, 2) == 3 ? 1 : 0;
    }
    CHECK(g.phase.size() == 75U);
    CHECK(flares == 32);
    CHECK(simulated(g) == 2700);
}

TEST_CASE("P3-04 16.4: the golden schedule cases of seed 42 and every edge case through run_engine") {
    // A fixed sample (all of it in history-slow): the edge cases E1-E10 and every case of seed 42 (spans,
    // levels, race sets, both ForcedTension variants).
    const std::size_t n = check_golden_scheds(
        [](const GoldenSched& s) { return s.races.empty() || s.name.rfind("sched-42-", 0) == 0; });
    CHECK(n >= 50U);
}

TEST_CASE("P3-04 3.4 items 2 to 5, 16.1 item 3: entry checks of history_stage with their messages, in order") {
    const eng::HistoryInputs in = inputs_for({0, 2074}, 6000);
    CHECK(fatal_of([&] { (void)eng::history_stage(shipped(), in); }) == "<no error>");
    auto stage_error = [&](const wd::ContentSet& c, const eng::HistoryInputs& i) {
        return fatal_of([&] { (void)eng::history_stage(c, i); });
    };
    {
        EngineTuning t;
        t.quiet_step_years = 0;
        CHECK(stage_error(with_engine(t), in) == "history: tuning: quiet_step_years outside its schema range");
        t = EngineTuning{};
        t.islet_max_dry_cells = 1000001;
        CHECK(stage_error(with_engine(t), in) == "history: tuning: islet_max_dry_cells outside its schema range");
        t = EngineTuning{};
        t.dense.flare_years = 1001;
        CHECK(stage_error(with_engine(t), in) == "history: tuning: dense.flare_years outside its schema range");
        t = EngineTuning{};
        t.sparse.flare_threshold = 0;
        CHECK(stage_error(with_engine(t), in) == "history: tuning: sparse.flare_threshold outside its schema range");
        t = EngineTuning{};
        t.simulated_min_permille = 460;
        CHECK(stage_error(with_engine(t), in) ==
              "history: tuning: simulated_min_permille must not be above simulated_max_permille");
        // engine_phase_budget, levels in the order sparse, normal, dense (ASCII x and -: the lead's ruling)
        t = EngineTuning{};
        t.normal.active_years = 400;
        t.dense.active_years = 500;
        CHECK(stage_error(with_engine(t), in) ==
              "history: tuning: normal: 6 x active_years + final_years must not be above 6 x "
              "simulated_max_permille - 30");
        // a range error comes before a named check
        t.quiet_events_max = 101;
        CHECK(stage_error(with_engine(t), in) == "history: tuning: quiet_events_max outside its schema range");
    }
    {
        std::vector<ReportEntry> r = shipped_report();
        r.insert(r.begin() + 1, ReportEntry{"duel", 10, "x"});  // ("battle" is a kind of P3-06 since 0.16.0)
        CHECK(stage_error(with_report(r), in) == "history: report: events[1]: kind 'duel' is not an event kind");
        r = shipped_report();
        r[2].templ = "{founder:plural}";
        CHECK(stage_error(with_report(r), in) ==
              "history: report: events[2]: template: form 'plural' needs a race role at byte 0");
        r = shipped_report();
        r.pop_back();
        CHECK(stage_error(with_report(r), in) == "history: report: event kind 'flare_up' has no entry");
    }
    {
        eng::HistoryInputs i = in;
        i.params.span = 1999;
        CHECK(stage_error(shipped(), i) == "history: settings: history_span 1999 outside 2000 to 12000");
        i = in;
        i.params.span = 12001;
        CHECK(stage_error(shipped(), i) == "history: settings: history_span 12001 outside 2000 to 12000");
        i = in;
        i.params.races = {"elf", "orc"};
        CHECK(stage_error(shipped(), i) == "history: settings: param.races does not match the present race rows");
        i = in;
        i.params.races = {"goblin", "elf"};
        CHECK(stage_error(shipped(), i) == "history: settings: param.races does not match the present race rows");
    }
    // Chronology rows (audit A1). The reason text is not fixed by the spec: the prefix is.
    {
        const std::string prefix = "history: chronology: ";
        auto chron = [&](auto&& edit) {
            eng::HistoryInputs i = in;
            edit(i);
            const std::string e = stage_error(shipped(), i);
            CAPTURE(e);
            CHECK(e.rfind(prefix, 0) == 0);
        };
        chron([](eng::HistoryInputs& i) { i.ages[1].id = 3; });
        chron([](eng::HistoryInputs& i) { i.ages[0].start_year = 1; });
        chron([](eng::HistoryInputs& i) { i.ages[1].start_year = 2075; });
        chron([](eng::HistoryInputs& i) { i.ages[1].end_year = 5998; });
        chron([](eng::HistoryInputs& i) { i.ages[1].race = 3; });
        chron([](eng::HistoryInputs& i) { i.races[1].awakening_year = 2073; });
        chron([](eng::HistoryInputs& i) {
            i.gods.erase(i.gods.begin() + 1);
        });
    }
}

TEST_CASE("P3-04 3.4 item 1: run_history's input preconditions in the pipeline") {
    dbtest::TempDir dir("engine-entry");
    auto run = [&](const std::string& name, bool regions) {
        chrono_test::synthetic_region_names() = {"North Downs"};
        std::vector<worldgen::stages::StageInfo> stages;
        if (regions) {
            stages.push_back(worldgen::stages::StageInfo{"regions", &chrono_test::synthetic_regions_stage});
        }
        stages.push_back(eng::stage_info());
        return fatal_of([&] {
            const auto s = worldgen::stages::testing::generate_world_with_stages(
                chrono_test::world_params(1, 1024, chrono_api::Params{}, shipped()), shipped(), stages,
                dir.file(name));
            if (!s.ok()) {
                throw worldgen::FatalError("status: " + s.error().message);
            }
        });
    };
    CHECK(run("a.db", false) == "history: input precondition: no region rows");
    CHECK(run("b.db", true) == "history: input precondition: no age rows");
}

TEST_CASE("P3-04 5.4, 16.1 item 3: the cap guard; a tuning with 6A + L above the cap passed past the loader") {
    // normal.active_years 500: 5 x 500 + 500 + 300 = 3300 > 2700 with six races 1000 years apart. The
    // stage's named check engine_phase_budget (3.4 item 2) fires first in history_stage; run_engine
    // either repeats it or reaches the scheduler guard (spec defect: 16.1 item 3 does not say which
    // seam skips 3.4), so only these two messages are accepted.
    EngineTuning t;
    t.normal.active_years = 500;
    const std::vector<std::int64_t> starts = {0, 1000, 2000, 3000, 4000, 5000};
    const std::string budget =
        "history: tuning: normal: 6 x active_years + final_years must not be above 6 x "
        "simulated_max_permille - 30";
    CHECK(fatal_of([&] { (void)eng::history_stage(with_engine(t), inputs_for(starts, 6000)); }) == budget);
    const std::string e = fatal_of([&] { (void)schedule(starts, 6000, "normal", nullptr, with_engine(t)); });
    CAPTURE(e);
    CHECK((e == budget || e == "history: schedule: scheduled active years 3300 above the cap 2700"));
}

TEST_CASE("P3-04 6.5: tension guards: a negative value, a trigger outside the log; the largest value wins") {
    const std::vector<std::int64_t> starts = {0, 2074};
    auto with = [&](std::int64_t value, std::int64_t trigger) {
        return fatal_of([&] {
            (void)schedule(starts, 6000, "normal", std::make_unique<FnTension>([=](const eng::StepContext&) {
                               return eng::TensionReport{value, trigger};
                           }));
        });
    };
    CHECK(with(-1, 0).rfind("history: tension: ", 0) == 0);
    CHECK(with(1000, 0).rfind("history: tension: ", 0) == 0);
    CHECK(with(1000, 3).rfind("history: tension: ", 0) == 0);  // only e1 exists in the first interval
    CHECK(with(999, 0) == "<no error>");                       // below the threshold the trigger is not checked
    CHECK(with(1000, 1) == "<no error>");
    // Two sources: the step's tension is the largest value, its trigger that of the first source reporting it.
    Config cfg;
    // (With the founding handler e1-e3 exist at year 0, so triggers 2 and 3 are in the log: 6.5 checks
    // every report at or above the threshold, not only the chosen one.)
    cfg.handler = std::make_unique<TestHandler>(true);
    cfg.add_tension(std::make_unique<FnTension>([](const eng::StepContext&) { return eng::TensionReport{500, 0}; }));
    cfg.add_tension(std::make_unique<FnTension>([](const eng::StepContext& c) {
        return eng::TensionReport{c.year() == 150 ? 1000 : 0, 2};
    }));
    cfg.add_tension(std::make_unique<FnTension>([](const eng::StepContext& c) {
        return eng::TensionReport{c.year() == 150 ? 1000 : 0, 3};
    }));
    const Tables t = eng::run_engine(shipped(), inputs_for(starts, 6000), cfg.get());
    CHECK(str(std::vector<Row>(t.phase.begin() + 1, t.phase.begin() + 4)) ==
          str(irows({{2, 1, 2, 150, 174, 0}, {3, 1, 3, 175, 224, 2}, {4, 1, 2, 225, 2073, 0}})));
}

}  // TEST_SUITE

TEST_SUITE("history-slow" * doctest::skip()) {

TEST_CASE("P3-04 16.4: every schedule case of the golden file through run_engine") {
    CHECK(check_golden_scheds([](const GoldenSched&) { return true; }) > 300U);
}

}  // TEST_SUITE
