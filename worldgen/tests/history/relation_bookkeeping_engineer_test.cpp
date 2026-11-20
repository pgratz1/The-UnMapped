// The engineer's checks of the relation bookkeeping rule of the P3-06
// ruling (P3-04 6.7 as amended): every relation opened since the last
// bookkeeping gets a row, also one closed again before it (since = until =
// the year, both events kept); a key opened twice before one bookkeeping is
// fatal. The three cases are the call sequences of conflict's three paths,
// played by a probe subsystem: (1) an enmity opened in T2 housekeeping and
// closed by the conquest of its older side in the same year's T6; (2) a
// peace opened by the end of one war in T6 and closed by a conquest later
// in the same T6; (3) a war declared in T2 (closing the pair's peace) and
// closed by a conquest in another war in the same year's T6.

#include <doctest/doctest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "p3_04_engine_support.hpp"

namespace {

using namespace engine_test;
using eng::EntityType;
using eng::Step;

// Civilizations 1 (year 0, test handler) and 2 (year 1); then `play` in T2 and T6.
Tables run_probe(const std::function<void(eng::StepContext&)>& t2, const std::function<void(eng::StepContext&)>& t6) {
    FnSubsystem a({Step::t2}, [&](eng::StepContext& ctx) {
        if (ctx.year() == 1) {
            eng::EventSpec e;
            e.kind = "civ_founded";
            e.year = 1;
            e.end_year = 1;
            e.region = 1;
            e.participants = {{"civilization", EntityType::civilization, 2}, {"race", EntityType::race, 1}};
            e.causes = {1};
            const std::int64_t id = ctx.append(e);
            CHECK(ctx.state().found_civilization(1, ctx.state().civ(1).culture, 1, "Second", id) == 2);
            return;
        }
        t2(ctx);
    });
    FnSubsystem b({Step::t6}, [&](eng::StepContext& ctx) { t6(ctx); });
    Config cfg;
    cfg.subsystems = {&a, &b};
    return eng::run_engine(shipped(), inputs_for({0}, 2000, "normal", {"elf"}), cfg.get());
}

Row rel(const char* kind, std::int64_t a, std::int64_t b, std::int64_t since, std::optional<std::int64_t> until,
        std::int64_t since_event, std::int64_t until_event) {
    return {eng::I(3), eng::I(a), eng::I(3), eng::I(b), eng::T(kind), eng::I(since),
            until ? eng::I(*until) : Cell{}, eng::I(since_event), eng::I(until_event)};
}

}  // namespace

TEST_CASE("P3-06 ruling: an enmity opened in T2 and closed by a conquest in the same T6 keeps its row") {
    const eng::RelationKey enmity{3, 1, 3, 2, "enmity"};
    const Tables t = run_probe(
        [&](eng::StepContext& ctx) {
            if (ctx.year() == 3) ctx.state().open_relation(enmity, change(ctx));  // e5 (housekeeping)
        },
        [&](eng::StepContext& ctx) {
            if (ctx.year() == 3) ctx.state().close_relation(enmity, change(ctx));  // e6 (the conquest)
        });
    CHECK(str(t.relation) == str(std::vector<Row>{rel("enmity", 1, 2, 3, 3, 5, 6)}));
}

TEST_CASE("P3-06 ruling: a peace opened in T6 and closed by a later conquest in the same T6 keeps its row") {
    const eng::RelationKey war{3, 2, 3, 1, "war"};
    const eng::RelationKey peace{3, 1, 3, 2, "peace"};
    const Tables t = run_probe(
        [&](eng::StepContext& ctx) {
            if (ctx.year() == 3) ctx.state().open_relation(war, change(ctx));  // e5: declared
        },
        [&](eng::StepContext& ctx) {
            if (ctx.year() == 4) {
                const std::int64_t p = change(ctx);  // e6: the peace of the war
                ctx.state().close_relation(war, p);
                ctx.state().open_relation(peace, p);
                ctx.state().close_relation(peace, change(ctx));  // e7: a side conquered in another war
            }
        });
    CHECK(str(t.relation) == str(std::vector<Row>{rel("peace", 1, 2, 4, 4, 6, 7), rel("war", 2, 1, 3, 4, 5, 6)}));
}

TEST_CASE("P3-06 ruling: a war declared in T2 and ended by a conquest in the same T6 keeps its row") {
    const eng::RelationKey war{3, 2, 3, 1, "war"};
    const eng::RelationKey peace{3, 1, 3, 2, "peace"};
    const Tables t = run_probe(
        [&](eng::StepContext& ctx) {
            if (ctx.year() == 2) ctx.state().open_relation(peace, change(ctx));  // e5: an earlier peace
            if (ctx.year() == 3) {
                const std::int64_t d = change(ctx);  // e6: war_declared closes the peace and opens the war
                ctx.state().close_relation(peace, d);
                ctx.state().open_relation(war, d);
            }
        },
        [&](eng::StepContext& ctx) {
            if (ctx.year() == 3) ctx.state().close_relation(war, change(ctx));  // e7: civ_conquered
        });
    CHECK(str(t.relation) == str(std::vector<Row>{rel("peace", 1, 2, 2, 3, 5, 6), rel("war", 2, 1, 3, 3, 6, 7)}));
}

TEST_CASE("P3-06 ruling: a key opened, closed and opened again before one bookkeeping is fatal") {
    const eng::RelationKey war{3, 2, 3, 1, "war"};
    const std::string msg = fatal_of([&] {
        (void)run_probe(
            [&](eng::StepContext& ctx) {
                if (ctx.year() == 3) {
                    ctx.state().open_relation(war, change(ctx));
                    ctx.state().close_relation(war, change(ctx));
                    ctx.state().open_relation(war, change(ctx));
                }
            },
            [](eng::StepContext&) {});
    });
    CHECK(msg == "history: state: relation 'war' of 3 2 and 3 1 opened twice before one bookkeeping");
}
