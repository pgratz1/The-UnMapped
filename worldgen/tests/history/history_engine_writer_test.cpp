// Spec P3-04 6.6, test 16.1 item 4: the event writer's checks W1 to W9,
// each failing once through a test subsystem, plus the quiet-step rules (a
// quiet-step event dated after its step's start; the quiet budget with the
// shipped quiet_events_max 3 and with 10, the eleventh append failing).
//
// The message is `history: event <id> (<kind>): <what>`; the spec fixes the
// prefix but not <what> (spec defect, reported), so the tests check the
// prefix exactly. W3 (year below the previous event's) cannot fail while W2
// holds (an event of a tick is dated at the tick year, of a quiet step at
// its start, and every earlier event is dated no later): it is checked as
// "an earlier year is refused", whichever of W2 or W3 reports it.
//
// World: one race (elf), span 2000, normal: opening 0-49, quiet 50-1849,
// final 1850-1999. The test handler writes e1 awakening, e2 civ_founded
// (civilization 1), e3 settlement_founded (settlement 1, region 1) at year 0.

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"

namespace {

using namespace engine_test;
using eng::EntityType;
using eng::EventSpec;
using eng::Step;

EventSpec ev(const char* kind, std::int64_t year) {
    EventSpec e;
    e.kind = kind;
    e.year = year;
    e.end_year = year;
    return e;
}

// Runs the engine with a subsystem appending what `fn` builds, once, in
// step `step` of year `year`; returns the FatalError text or "<no error>".
std::string append_in(Step step, std::int64_t year, const std::function<void(eng::StepContext&)>& fn,
                      const wd::ContentSet& content = shipped()) {
    bool done = false;
    FnSubsystem s({step}, [&](eng::StepContext& ctx) {
        if (!done && ctx.year() == year) {
            done = true;
            fn(ctx);
        }
    });
    Config cfg;
    cfg.subsystems = {&s};
    return fatal_of([&] { (void)eng::run_engine(content, inputs_for({0}, 2000, "normal", {"elf"}), cfg.get()); });
}

std::string tick(const EventSpec& e) {
    return append_in(Step::t2, 1, [&](eng::StepContext& ctx) { (void)ctx.append(e); });
}

bool prefixed(const std::string& msg, const std::string& prefix) { return msg.rfind(prefix, 0) == 0; }

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 6.6: valid events of each shipped kind and of a test kind are accepted") {
    EventSpec a = ev("test_change", 1);
    a.participants = {{"actor", EntityType::civilization, 1}};
    CHECK(tick(a) == "<no error>");
    EventSpec b = ev("settlement_founded", 1);
    b.region = 1;
    b.settlement = 1;
    b.participants = {{"settlement", EntityType::settlement, 1}, {"founder", EntityType::civilization, 1}};
    b.values = {200};
    CHECK(tick(b) == "<no error>");
    EventSpec c = ev("quiet_era", 1);
    c.region = 1;
    c.end_year = 1999;  // end_year up to E - 1 in a tick
    c.participants = {{"civilization", EntityType::civilization, 1}};
    c.values = {1, 0};
    CHECK(tick(c) == "<no error>");
    // P3-04 r12 20.4 (0.19.0): kind 38 quiet_stretch, no region, role realm 1-0 (unbounded)
    EventSpec q = ev("quiet_stretch", 1);
    q.end_year = 1999;
    q.participants = {{"realm", EntityType::civilization, 1}};
    q.values = {1999, 1};
    CHECK(tick(q) == "<no error>");
    EventSpec d = ev("test_change", 1);
    d.salience = 0;
    d.causes = {1, 3};
    CHECK(tick(d) == "<no error>");
    d.salience = 100;
    CHECK(tick(d) == "<no error>");
}

TEST_CASE("P3-04 6.6 W1, W2, W3: kind, year rules") {
    const std::string p4 = "history: event 4 (";
    CHECK(prefixed(tick(ev("duel", 1)), p4 + "duel): "));  // not in the catalogue ("battle" is, since 0.16.0)
    CHECK(prefixed(tick(ev("test_change", 2)), p4 + "test_change): "));  // W2: not the tick year
    EventSpec e = ev("test_change", 1);
    e.end_year = 0;  // W2: end_year below year
    CHECK(prefixed(tick(e), p4 + "test_change): "));
    e.end_year = 2000;  // W2: past E - 1
    CHECK(prefixed(tick(e), p4 + "test_change): "));
    CHECK(prefixed(tick(ev("test_change", 0)), p4 + "test_change): "));  // W2/W3: an earlier year
}

TEST_CASE("P3-04 6.6 W4: participants") {
    const std::string p = "history: event 4 (";
    EventSpec e = ev("test_change", 1);
    e.participants = {{"ruler", EntityType::civilization, 1}};  // not a role of the kind
    CHECK(prefixed(tick(e), p + "test_change): "));
    e.participants = {{"actor", EntityType::race, 1}};  // wrong entity type
    CHECK(prefixed(tick(e), p + "test_change): "));
    e.participants = {{"actor", EntityType::civilization, 99}};  // no such civilization
    CHECK(prefixed(tick(e), p + "test_change): "));
    EventSpec f = ev("settlement_founded", 1);
    f.region = 1;
    f.settlement = 1;
    f.values = {200};
    f.participants = {{"settlement", EntityType::settlement, 1}, {"founder", EntityType::civilization, 1},
                      {"founder", EntityType::civilization, 1}};  // above max_count 1
    CHECK(prefixed(tick(f), p + "settlement_founded): "));
    f.participants = {{"settlement", EntityType::settlement, 1}};  // below min_count 1
    CHECK(prefixed(tick(f), p + "settlement_founded): "));
    f.participants = {{"founder", EntityType::civilization, 1}, {"settlement", EntityType::settlement, 1}};
    CHECK(prefixed(tick(f), p + "settlement_founded): "));  // not in ascending role seq
    f.participants = {{"settlement", EntityType::settlement, 2}, {"founder", EntityType::civilization, 1}};
    CHECK(prefixed(tick(f), p + "settlement_founded): "));  // settlement 2 not created
    EventSpec g = ev("awakening", 1);
    g.region = 1;
    g.participants = {{"race", EntityType::race, 7}, {"creator", EntityType::god, 1}};  // no race row 7
    CHECK(prefixed(tick(g), p + "awakening): "));
    g.participants = {{"race", EntityType::race, 1}, {"creator", EntityType::god, 9}};  // no god row 9
    CHECK(prefixed(tick(g), p + "awakening): "));
}

TEST_CASE("P3-04 6.6 W5, W6, W7, W8: values, causes, place, salience") {
    const std::string p = "history: event 4 (";
    EventSpec f = ev("settlement_founded", 1);
    f.region = 1;
    f.settlement = 1;
    f.participants = {{"settlement", EntityType::settlement, 1}, {"founder", EntityType::civilization, 1}};
    CHECK(prefixed(tick(f), p + "settlement_founded): "));  // W5: no value
    f.values = {200, 1};
    CHECK(prefixed(tick(f), p + "settlement_founded): "));  // W5: two values
    f.values = {200};
    f.region = 2;  // W7: settlement 1 lies in region 1
    CHECK(prefixed(tick(f), p + "settlement_founded): "));
    f.region = 0;  // W7: region_use 1
    CHECK(prefixed(tick(f), p + "settlement_founded): "));
    f.region = 1;
    f.settlement = 0;  // W7: settlement_use 1
    CHECK(prefixed(tick(f), p + "settlement_founded): "));
    EventSpec a = ev("awakening", 1);
    a.region = 1;
    a.settlement = 1;  // W7: settlement_use 0
    a.participants = {{"race", EntityType::race, 1}, {"creator", EntityType::god, 1}};
    CHECK(prefixed(tick(a), p + "awakening): "));
    EventSpec fl = ev("flare_up", 1);
    fl.values = {10};
    CHECK(prefixed(tick(fl), p + "flare_up): "));  // W6: caused kind without a cause
    fl.region = 1;
    fl.causes = {1};
    CHECK(prefixed(tick(fl), p + "flare_up): "));  // W7: region_use 0
    EventSpec t = ev("test_change", 1);
    for (const std::vector<std::int64_t>& c : {std::vector<std::int64_t>{2, 1}, {1, 1}, {0}, {4}, {5}}) {
        t.causes = c;
        CHECK(prefixed(tick(t), p + "test_change): "));  // W6
    }
    t.causes = {};
    t.region = 3;  // W7: no region 3
    CHECK(prefixed(tick(t), p + "test_change): "));
    t.region = 0;
    t.settlement = 2;  // W7: no settlement 2
    CHECK(prefixed(tick(t), p + "test_change): "));
    t.settlement = 0;
    t.salience = 101;
    CHECK(prefixed(tick(t), p + "test_change): "));  // W8
    t.salience = -1;
    CHECK(prefixed(tick(t), p + "test_change): "));
}

TEST_CASE("P3-04 6.6 W2, W9: quiet-step dating and the quiet budget per charged civilization") {
    // First quiet step: [50, 75).
    auto quiet = [](const std::function<void(eng::StepContext&)>& fn, const wd::ContentSet& c = shipped()) {
        return append_in(Step::q1, 50, fn, c);
    };
    CHECK(quiet([](eng::StepContext& ctx) {
              CHECK(ctx.year() == 50);
              CHECK(ctx.years() == 25);
              (void)change(ctx, 1, 50);
          }) == "<no error>");
    CHECK(prefixed(quiet([](eng::StepContext& ctx) { (void)change(ctx, 1, 51); }), "history: event 4 (test_change): "));
    CHECK(quiet([](eng::StepContext& ctx) {
              EventSpec e = ev("test_change", 50);
              e.end_year = 74;  // the step's last year
              (void)ctx.append(e);
          }) == "<no error>");
    CHECK(prefixed(quiet([](eng::StepContext& ctx) {
                       EventSpec e = ev("test_change", 50);
                       e.end_year = 75;
                       (void)ctx.append(e);
                   }),
                   "history: event 4 (test_change): "));
    // quiet_events_max 3: three events charged to civilization 1 pass, the fourth fails; uncharged events
    // (no civilization participant) are charged to 0 and counted for 0.
    CHECK(quiet([](eng::StepContext& ctx) {
              CHECK(ctx.quiet_events_left(1) == 3);
              for (int i = 0; i < 3; ++i) {
                  (void)change(ctx, 1);
              }
              CHECK(ctx.quiet_events_left(1) == 0);
          }) == "<no error>");
    CHECK(prefixed(quiet([](eng::StepContext& ctx) {
                       for (int i = 0; i < 4; ++i) {
                           (void)change(ctx, 1);
                       }
                   }),
                   "history: event 7 (test_change): "));
    // quiet_events_max 10: the eleventh append fails (16.1 item 4).
    EngineTuning t;
    t.quiet_events_max = 10;
    const wd::ContentSet ten = with_engine(t);
    CHECK(quiet(
              [](eng::StepContext& ctx) {
                  for (int i = 0; i < 10; ++i) {
                      (void)change(ctx, 1);
                  }
              },
              ten) == "<no error>");
    CHECK(prefixed(quiet(
                       [](eng::StepContext& ctx) {
                           for (int i = 0; i < 11; ++i) {
                               (void)change(ctx, 1);
                           }
                       },
                       ten),
                   "history: event 14 (test_change): "));
    // The budget is per step: three in each of two consecutive steps pass.
    int steps = 0;
    FnSubsystem s({Step::q1}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 50 || ctx.year() == 75) {
            ++steps;
            for (int i = 0; i < 3; ++i) {
                (void)change(ctx, 1);
            }
        }
    });
    Config cfg;
    cfg.subsystems = {&s};
    CHECK(fatal_of([&] { (void)eng::run_engine(shipped(), inputs_for({0}, 2000, "normal", {"elf"}), cfg.get()); }) ==
          "<no error>");
    CHECK(steps == 2);
}

}  // TEST_SUITE
