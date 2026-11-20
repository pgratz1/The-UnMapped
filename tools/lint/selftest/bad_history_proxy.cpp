// lint-selftest-path: worldgen/src/history/selftest_proxy.cpp
// Self-test snippet: scanned as history library code (P3-04 6.9, P3-06
// 20.10). A function of the same file that founds, directly or through
// another function of the file, counts as a founding for
// history-held-reference and history-live-iteration (P3-09's flight founds
// in flee(), called from siege(), called from battle()). Not compiled.
#include "worldgen/history/engine.hpp"

namespace worldgen::history {

void Sub::flee(StepContext& ctx, std::int64_t t) {
    State& state = ctx.state();
    state.found_settlement(0, 0, 1, 1, SettlementKind::hamlet, 10, "x", 1);
    state.set_population(t, 0);
}

void Sub::siege(StepContext& ctx, std::int64_t t) const {
    if (t > 0) {
        flee(ctx, t);
    }
}

bool Sub::reads(const StepContext& ctx) const noexcept {
    return ctx.state().settlement(1).population > 0;
}

void Sub::battle(StepContext& ctx, std::int64_t t) {
    const Settlement& s = ctx.state().settlement(t);  // expect: history-held-reference
    siege(ctx, t);
    (void)s.population;

    const auto& c = ctx.state().civ(1);  // expect: history-held-reference
    flee(ctx, t);
    (void)c.race;

    for (const Settlement& u : ctx.state().settlements()) {  // expect: history-live-iteration
        siege(ctx, u.id);
    }
}

// Allowed: a helper that does not found; fields copied before the proxy;
// re-fetched by id after it; the reference only in the proxy's arguments;
// a proxy called in a loop over ids, or over relations (founding does not
// change the relation map).
void Sub::fight(StepContext& ctx, std::int64_t t) {
    const Settlement& s = ctx.state().settlement(t);
    (void)reads(ctx);
    (void)s.population;
    const std::int64_t region = s.region;
    battle(ctx, t);
    (void)region;
    const Settlement& again = ctx.state().settlement(t);
    (void)again.population;
    const Settlement& arg = ctx.state().settlement(t);
    siege(ctx, arg.id);
    const auto n = static_cast<std::int64_t>(ctx.state().settlements().size());
    for (std::int64_t id = 1; id <= n; ++id) {
        siege(ctx, id);
    }
    for (const auto& [key, open] : ctx.state().relations()) {
        (void)open;
        siege(ctx, key.a_id);
    }
}

}  // namespace worldgen::history
