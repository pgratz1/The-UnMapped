// Spec P3-04 6.2 to 6.8, tests 16.1 items 5 to 8 and 16.3 items 22 to 24:
// the step seam (a test subsystem and tension sources plugged through
// EngineConfig), "created in a step, processed from the next step on",
// bookkeeping of region_control, relation and settlement_census driven by a
// test subsystem (rows written here by hand from 6.7), quiet eras broken by
// a charged event, a flare-up and an awakening, generations across a
// generation boundary, the intervention seam (every intervention dropped
// in P3), Y3 draw independence, Y4 insertion order, and history_stage twice.

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"

namespace {

using namespace engine_test;
using eng::EntityType;
using eng::Step;

std::vector<db::RegionRow> three_regions() {
    std::vector<db::RegionRow> r = {region_row(1, "Alpha", 0, 0, 7, 7, 5000, 100, 60),
                                    region_row(2, "Beta", 8, 0, 15, 7, 5000, 90, 50),
                                    region_row(3, "Gamma", 0, 8, 15, 15, 5000, 80, 40)};
    return r;
}

std::vector<Row> rows_where_kind(const Tables& t, std::int64_t kind) {
    std::vector<Row> out;
    for (const Row& r : t.event) {
        if (at(r, 3) == kind) {
            out.push_back(r);
        }
    }
    return out;
}

}  // namespace

TEST_SUITE("history") {

TEST_CASE("P3-04 6.2, 16.1 item 5: an entity created in a step is visible from the next step on") {
    std::vector<std::string> log;
    FnSubsystem t2({Step::t2}, [&](eng::StepContext& ctx) {
        auto visible = [&] {
            std::int64_t n = 0;
            for (const eng::Settlement& s : ctx.state().settlements()) {
                n += ctx.visible(s) ? 1 : 0;
            }
            return n;
        };
        if (ctx.year() == 5) {
            const std::int64_t s = static_cast<std::int64_t>(ctx.state().settlements().size()) + 1;
            eng::EventSpec e;
            e.kind = "settlement_founded";
            e.year = 5;
            e.end_year = 5;
            e.region = 1;
            e.settlement = s;
            e.participants = {{"settlement", EntityType::settlement, s}, {"founder", EntityType::civilization, 1}};
            e.values = {100};
            const std::int64_t id = ctx.append(e);
            const std::int64_t got =
                ctx.state().found_settlement(3, 3, 1, 1, eng::SettlementKind::hamlet, 100, "Newtown", id);
            CHECK(got == 2);
            CHECK(ctx.state().settlements().size() == 2U);
            log.push_back("t2@5 visible " + std::to_string(visible()));
        }
        if (ctx.year() == 6) {
            log.push_back("t2@6 visible " + std::to_string(visible()));
        }
    });
    FnSubsystem t3({Step::t3}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 5) {
            std::int64_t n = 0;
            for (const eng::Settlement& s : ctx.state().settlements()) {
                n += ctx.visible(s) ? 1 : 0;
            }
            log.push_back("t3@5 visible " + std::to_string(n));
        }
    });
    FnSubsystem t1({Step::t1}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 0) {  // the awakening step precedes T1 of the awakening year
            CHECK(ctx.state().civs().size() == 1U);
            CHECK(ctx.visible(ctx.state().civ(1)));
            CHECK(ctx.step() == Step::t1);
            CHECK(ctx.years() == 1);
            CHECK(ctx.phase_id() == 1);
            CHECK(ctx.phase_kind() == eng::PhaseKind::opening);
        }
    });
    Config cfg;
    cfg.subsystems = {&t1, &t2, &t3};
    const Tables t = eng::run_engine(shipped(), inputs_for({0}, 2000, "normal", {"elf"}), cfg.get());
    CHECK(log == std::vector<std::string>{"t2@5 visible 1", "t3@5 visible 2", "t2@6 visible 2"});
    CHECK(t.settlement.size() == 2U);
}

TEST_CASE("P3-04 6.7, 16.1 item 6: bookkeeping of region control, relations and the census") {
    // One race (elf), span 2000: opening 0-49. e1-e3 by the test handler (civilization 1, settlement 1).
    FnSubsystem s({Step::t2}, [](eng::StepContext& ctx) {
        auto& st = ctx.state();
        const eng::RelationKey war{3, 1, 3, 2, "war"};
        const eng::RelationKey peace{3, 1, 3, 2, "peace"};
        switch (ctx.year()) {
            case 1: {  // e4: a second civilization
                eng::EventSpec e;
                e.kind = "civ_founded";
                e.year = 1;
                e.end_year = 1;
                e.region = 1;
                e.participants = {{"civilization", EntityType::civilization, 2}, {"race", EntityType::race, 1}};
                e.causes = {1};
                const std::int64_t id = ctx.append(e);
                CHECK(st.found_civilization(1, st.civ(1).culture, 1, "Second", id) == 2);
                break;
            }
            case 2:  // net change: free -> 1 -> 2 (e5, e6)
                st.set_region_owner(2, 1, change(ctx));
                st.set_region_owner(2, 2, change(ctx));
                break;
            case 3:  // no net change: 2 -> 1 -> 2 (e7, e8)
                st.set_region_owner(2, 1, change(ctx));
                st.set_region_owner(2, 2, change(ctx));
                break;
            case 4:  // region 2 freed (e9), region 3 taken (e10)
                st.set_region_owner(2, 0, change(ctx));
                st.set_region_owner(3, 1, change(ctx));
                break;
            case 5: st.open_relation(war, change(ctx)); break;   // e11
            case 6: st.close_relation(war, change(ctx)); break;  // e12
            case 7:  // opened and closed within one year: a row with since = until (P3-06 ruling; e13, e14)
                st.open_relation(peace, change(ctx));
                st.close_relation(peace, change(ctx));
                break;
            case 8: st.set_settlement_kind(1, eng::SettlementKind::village, change(ctx)); break;      // e15
            case 9: st.set_settlement_status(1, eng::SettlementStatus::declining, change(ctx)); break;  // e16
            case 10: st.set_settlement_owner(1, 2, change(ctx)); break;                               // e17
            case 12: st.set_population(1, 350); break;  // no census row now; one at the phase end (49)
            default: break;
        }
    });
    Config cfg;
    cfg.subsystems = {&s};
    const Tables t =
        eng::run_engine(shipped(), inputs_for({0}, 2000, "normal", {"elf"}, three_regions()), cfg.get());
    CHECK(str(t.region_control) == str(irows({{2, 2, 4, 2, 6}, {3, 4, NUL, 1, 10}})));
    CHECK(str(t.relation) == str(std::vector<Row>{{eng::I(3), eng::I(1), eng::I(3), eng::I(2), eng::T("peace"),
                                                  eng::I(7), eng::I(7), eng::I(13), eng::I(14)},
                                                 {eng::I(3), eng::I(1), eng::I(3), eng::I(2), eng::T("war"),
                                                  eng::I(5), eng::I(6), eng::I(11), eng::I(12)}}));
    CHECK(str(t.settlement_census) == str(irows({{1, 0, 1, 1, 1, 200},
                                                 {1, 8, 1, 2, 1, 200},
                                                 {1, 9, 1, 2, 2, 200},
                                                 {1, 10, 2, 2, 2, 200},
                                                 {1, 49, 2, 2, 2, 350}})));
    REQUIRE(t.settlement.size() == 1U);
    // end state: owner 2, village, declining, 350, never ruined
    CHECK(at(t.settlement[0], 8) == 2);
    CHECK(at(t.settlement[0], 9) == 2);
    CHECK(at(t.settlement[0], 10) == 2);
    CHECK(at(t.settlement[0], 11) == 350);
    CHECK(t.settlement[0][12].type == db::ValueType::null);
    CHECK(at(t.settlement[0], 13) == 0);
    REQUIRE(t.civilization.size() == 2U);
    CHECK(at(t.civilization[0], 5) == 1);  // capital
    CHECK(at(t.civilization[1], 5) == 0);
    CHECK(t.civilization[1][8].type == db::ValueType::null);
}

TEST_CASE("P3-04 6.6, 16.1 item 7: quiet eras broken by a charged event, a flare-up and an awakening") {
    // elf at 0, goblin at 1000, span 2000, normal (A 50, F 17, L 100, Q 25), ForcedTension(500, 1000, 1);
    // a test subsystem charges one event to civilization 1 in the quiet step starting at 150.
    FnSubsystem q({Step::q1}, [](eng::StepContext& ctx) {
        if (ctx.year() == 150) {
            (void)change(ctx, 1);
        }
    });
    Config cfg;
    cfg.subsystems = {&q};
    cfg.add_tension(eng::forced_tension(500, 1000, 1));
    const Tables t = eng::run_engine(shipped(), inputs_for({0, 1000}, 2000), cfg.get());
    CHECK(phases_text(t, true) ==
          "1:1:1:0:49:1|2:1:2:50:499:0|3:1:3:500:516:1|4:1:2:517:999:0|5:2:1:1000:1049:9|6:2:2:1050:1499:0|"
          "7:2:3:1500:1516:1|8:2:2:1517:1899:0|9:2:4:1900:1999:0");
    CHECK(str(t.event) == str(irows({{1, 0, 0, 1, 1, 1, 0, 100},
                                     {2, 0, 0, 2, 1, 1, 0, 80},
                                     {3, 0, 0, 3, 1, 1, 1, 40},
                                     {4, 50, 149, 4, 2, 1, 0, 20},
                                     {5, 150, 150, 6, 2, 0, 0, 10},
                                     {6, 175, 499, 4, 2, 1, 0, 20},
                                     {7, 500, 500, 5, 3, 0, 0, 50},
                                     {8, 517, 999, 4, 4, 1, 0, 20},
                                     {9, 1000, 1000, 1, 5, 1, 0, 100},
                                     {10, 1000, 1000, 2, 5, 1, 0, 80},
                                     {11, 1000, 1000, 3, 5, 1, 2, 40},
                                     {12, 1050, 1499, 4, 6, 1, 0, 20},
                                     {13, 1050, 1499, 4, 6, 1, 0, 20},
                                     {14, 1500, 1500, 5, 7, 0, 0, 50},
                                     {15, 1517, 1899, 4, 8, 1, 0, 20},
                                     {16, 1517, 1899, 4, 8, 1, 0, 20}})));
    // years and generations: elves 150 years a generation (founded 0), goblins 15 (founded 1000)
    CHECK(str(t.event_value) == str(irows({{3, 1, 200},
                                           {4, 1, 100}, {4, 2, 1},
                                           {6, 1, 325}, {6, 2, 2},
                                           {7, 1, 17},
                                           {8, 1, 483}, {8, 2, 3},
                                           {11, 1, 200},
                                           {12, 1, 450}, {12, 2, 3},
                                           {13, 1, 450}, {13, 2, 30},
                                           {14, 1, 17},
                                           {15, 1, 383}, {15, 2, 2},
                                           {16, 1, 383}, {16, 2, 26}})));
    CHECK(str(t.event_cause) == str(irows({{2, 1}, {3, 2}, {7, 1}, {10, 9}, {11, 10}, {14, 1}})));
    CHECK(str(t.event_participant) ==
          str(irows({{1, 1, 1, 1, 1}, {1, 2, 2, 2, 1}, {2, 1, 1, 3, 1}, {2, 2, 2, 1, 1}, {3, 1, 1, 4, 1},
                     {3, 2, 2, 3, 1}, {4, 1, 1, 3, 1}, {5, 1, 1, 3, 1}, {6, 1, 1, 3, 1}, {8, 1, 1, 3, 1},
                     {9, 1, 1, 1, 2}, {9, 2, 2, 2, 2}, {10, 1, 1, 3, 2}, {10, 2, 2, 1, 2}, {11, 1, 1, 4, 2},
                     {11, 2, 2, 3, 2}, {12, 1, 1, 3, 1}, {13, 1, 1, 3, 2}, {15, 1, 1, 3, 1}, {16, 1, 1, 3, 2}})));
    // The test kind is written to the catalogue tables (9.3: every kind of the config's catalogue).
    REQUIRE(t.event_kind.size() == 6U);
    CHECK(t.event_kind[5][1].bytes == "test_change");
}

TEST_CASE("P3-04 6.8, 16.1 item 8: the intervention seam; every intervention is dropped in P3") {
    class Meddler : public eng::InterventionSource {
    public:
        std::vector<eng::Intervention> interventions(const eng::StepContext& ctx) override {
            ++calls;
            CHECK(ctx.step() == Step::t5);
            std::vector<eng::Intervention> v;
            v.push_back(eng::Intervention{ctx.year(), 1, "bless", EntityType::civilization, 1, 0, false});
            v.push_back(eng::Intervention{ctx.year(), 2, "curse", EntityType::settlement, 1, 1, true});
            v.push_back(eng::Intervention{ctx.year() + 1, 99, "x", EntityType::region, 7, 9, false});
            return v;
        }
        std::int64_t calls = 0;
    };
    const eng::HistoryInputs in = inputs_for({0, 2074, 3242, 4243, 4969, 5596}, 6000);
    Config a;
    const Tables base = eng::run_engine(shipped(), in, a.get());
    Config b;
    auto m = std::make_unique<Meddler>();
    Meddler* mp = m.get();
    b.interventions = std::move(m);
    const Tables with = eng::run_engine(shipped(), in, b.get());
    CHECK(with == base);
    CHECK(mp->calls == simulated(base));  // T5 of every tick year
}

TEST_CASE("P3-04 16.3 item 22 (Y3): keyed draws in T1 and Q1 and an extra draw before the stub's names change nothing") {
    const eng::HistoryInputs in = inputs_for({0, 2074, 3242, 4243, 4969, 5596}, 6000);
    Config a;
    a.handler = eng::make_stub();
    a.catalogue = eng::catalogue_of(shipped());
    const Tables base = eng::run_engine(shipped(), in, a.get());
    REQUIRE(base.civilization.size() == 6U);
    std::uint64_t sink = 0;
    FnSubsystem d({Step::t1, Step::q1}, [&](eng::StepContext& ctx) {
        const std::uint64_t key = worldgen::stream_key(ctx.inputs().seed, "test.engine.draws");
        for (std::uint64_t k = 0; k < 4; ++k) {
            sink ^= ctx.draw(key, 1, 2, k);
        }
    });
    Config b;
    b.handler = eng::make_stub();
    b.catalogue = eng::catalogue_of(shipped());
    b.subsystems = {&d};
    CHECK(eng::run_engine(shipped(), in, b.get()) == base);
    class Extra : public eng::AwakeningHandler {
    public:
        std::unique_ptr<eng::AwakeningHandler> stub = eng::make_stub();
        std::uint64_t sink = 0;
        std::int64_t awakening_place(const eng::StepContext& ctx, std::int64_t race) override {
            return stub->awakening_place(ctx, race);
        }
        void awaken(eng::StepContext& ctx, std::int64_t race, std::int64_t ev) override {
            sink ^= ctx.draw(worldgen::stream_key(ctx.inputs().seed, "test.engine.extra"), static_cast<std::uint64_t>(race), 0, 0);
            stub->awaken(ctx, race, ev);
        }
    };
    Config c;
    c.handler = std::make_unique<Extra>();
    c.catalogue = eng::catalogue_of(shipped());
    CHECK(eng::run_engine(shipped(), in, c.get()) == base);
    CHECK(sink != 0);
}

TEST_CASE("P3-04 16.3 item 23 (Y4): the same changes in two call orders within one step give the same tables") {
    auto run = [](bool reversed) {
        FnSubsystem s({Step::t2}, [reversed](eng::StepContext& ctx) {
            if (ctx.year() != 5) {
                return;
            }
            auto& st = ctx.state();
            const std::int64_t e = change(ctx);
            const std::vector<eng::RelationKey> keys = {{3, 1, 3, 2, "war"}, {3, 1, 3, 2, "alliance"}, {3, 2, 4, 1, "claim"}};
            std::vector<int> order = {0, 1, 2};
            if (reversed) {
                order = {2, 1, 0};
            }
            for (const int i : order) {
                st.open_relation(keys[static_cast<std::size_t>(i)], e);
            }
            if (reversed) {
                st.set_region_owner(3, 2, e);
                st.set_region_owner(2, 1, e);
            } else {
                st.set_region_owner(2, 1, e);
                st.set_region_owner(3, 2, e);
            }
        });
        Config cfg;
        cfg.subsystems = {&s};
        return eng::run_engine(shipped(), inputs_for({0, 3}, 2000, "normal", {}, three_regions()), cfg.get());
    };
    const Tables a = run(false);
    const Tables b = run(true);
    CHECK(a == b);
    CHECK(a.relation.size() == 3U);
    // RelationKey order: (3,1,3,2,"alliance") < (3,1,3,2,"war") < (3,2,4,1,"claim")
    REQUIRE(a.relation.size() == 3U);
    CHECK(a.relation[0][4].bytes == "alliance");
    CHECK(a.relation[1][4].bytes == "war");
    CHECK(a.relation[2][4].bytes == "claim");
    CHECK(a.region_control.size() == 2U);
}

TEST_CASE("P3-04 16.3 item 24: history_stage twice on the same inputs gives the same rows") {
    const eng::HistoryInputs in = inputs_for({0, 2074, 3242, 4243, 4969, 5596}, 6000);
    const Tables a = eng::history_stage(shipped(), in);
    const Tables b = eng::history_stage(shipped(), in);
    CHECK(a == b);
    CHECK(a.civilization.size() == 6U);
    CHECK(rows_where_kind(a, 1).size() == 6U);
}

}  // TEST_SUITE
