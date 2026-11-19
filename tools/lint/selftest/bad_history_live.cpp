// lint-selftest-path: worldgen/src/history/selftest_live.cpp
// Self-test snippet: scanned as history library code (P3-04 6.1, 6.2). A
// range-for over the State's live civilizations, settlements or relations
// must not found, open or close in its body: the container it iterates
// changes under it. Not compiled.
#include "worldgen/history/engine.hpp"

namespace worldgen::history {

void bad_loops(StepContext& ctx, State& state) {
    for (const Civilization& c : ctx.state().civs()) {  // expect: history-live-iteration
        if (ctx.visible(c)) {
            state.found_settlement(0, 0, c.home_region, c.id, SettlementKind::hamlet, 10, "x", 1);
        }
    }
    for (const auto& s : state.settlements())  // expect: history-live-iteration
        state.found_civilization(1, 1, s.region, "y", 1);
    for (const auto& [key, open] : state.relations()) {  // expect: history-live-iteration
        state.close_relation(key, open.event);
    }
    for (const Civilization& c : civs_) {  // expect: history-live-iteration
        if (c.id == 1) {
            open_relation(RelationKey{3, c.id, 3, 2, "war"}, 1);
        }
    }
}

// Allowed: reading only; ids up to a count taken before the loop; changes
// collected and applied after the loop; another container; a mutator after
// the loop's body.
void good_loops(StepContext& ctx, State& state) {
    std::int64_t n = 0;
    for (const Civilization& c : ctx.state().civs()) {
        n += ctx.visible(c) ? 1 : 0;
    }
    const auto count = static_cast<std::int64_t>(state.civs().size());
    for (std::int64_t id = 1; id <= count; ++id) {
        state.found_settlement(0, 0, state.civ(id).home_region, id, SettlementKind::hamlet, 10, "x", 1);
    }
    std::vector<RelationKey> closing;
    for (const auto& [key, open] : state.relations()) {
        closing.push_back(key);
    }
    for (const RelationKey& key : closing) {
        state.close_relation(key, 1);
    }
    for (const Settlement& s : state.settlements()) {
        n += s.population;
    }
    state.found_civilization(1, 1, 1, "z", 1);
    // for (const auto& c : state.civs()) { state.found_civilization(...); } in a comment
    const char* text = "for (const auto& c : state.civs()) { found_civilization(1); }";
    (void)text;
}

}  // namespace worldgen::history
