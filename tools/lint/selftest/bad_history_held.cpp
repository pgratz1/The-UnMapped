// lint-selftest-path: worldgen/src/history/selftest_held.cpp
// Self-test snippet: scanned as history library code (P3-04 6.9, P3-05
// 13.1 item 1). A reference, pointer or span into State's civilization or
// settlement vector must not be used after a founding in the same block:
// the founding may reallocate the vector. Not compiled.
#include "worldgen/history/engine.hpp"

namespace worldgen::history {

void bad_refs(StepContext& ctx, State& state, std::int64_t id) {
    const Settlement& s = state.settlement(id);  // expect: history-held-reference
    state.found_settlement(0, 0, s.region, s.owner_civ, SettlementKind::hamlet, 10, "x", 1);
    state.set_population(s.id, 5);

    const Civilization* c = &state.civ(1);  // expect: history-held-reference
    state.found_civilization(1, 1, 1, "y", 1);
    (void)c->name;

    std::span<const Settlement> all = state.settlements();  // expect: history-held-reference
    state.found_settlement(0, 0, 1, 1, SettlementKind::hamlet, 10, "z", 1);
    (void)all.size();

    const auto& civ = ctx.state().civ(2);  // expect: history-held-reference
    if (civ.id > 0) {
        state.found_civilization(1, 1, 1, "w", 1);
    }
    (void)civ.race;

    auto list = state.civs();  // expect: history-held-reference
    state.found_civilization(1, 1, 1, "v", 1);
    (void)list.front();
}

// Allowed: fields copied before the founding; the reference used only in
// the founding's own arguments; re-fetched by id after it; a reference whose
// block ends before the founding; another type; a range-for variable.
void good_refs(State& state, std::int64_t id) {
    const Settlement& s = state.settlement(id);
    const std::int64_t region = s.region;
    state.found_settlement(0, 0, region, 1, SettlementKind::hamlet, 10, "x", 1);
    const Civilization& c = state.civ(1);
    state.found_civilization(c.race, c.culture, c.home_region, "y", 1);
    state.set_capital(1, 2);
    {
        const Settlement& t = state.settlement(id);
        (void)t.population;
    }
    state.found_settlement(0, 0, 1, 1, SettlementKind::hamlet, 10, "z", 1);
    const Settlement& again = state.settlement(id);
    (void)again.population;
    const PeoplesRace& r = race_of(1);
    state.found_civilization(1, 1, 1, "u", 1);
    (void)r.id;
    for (const Settlement& u : state.settlements()) {
        (void)u.id;
    }
    std::int64_t automatic = state.settlement(1).population;
    state.found_civilization(1, 1, 1, "t", 1);
    (void)automatic;
}

}  // namespace worldgen::history

// Allowed: an integer computed from the containers (the stub's next id).
namespace worldgen::history {
void good_count(State& state) {
    const auto c = static_cast<std::int64_t>(state.civs().size()) + 1;
    state.found_civilization(1, 1, 1, "s", 1);
    (void)c;
}
}  // namespace worldgen::history
