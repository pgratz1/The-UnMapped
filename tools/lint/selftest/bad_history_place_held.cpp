// lint-selftest-path: worldgen/src/history/selftest_place_held.cpp
// Self-test snippet: scanned as history library code (P3-04 6.9, P5-03
// 6.6). RelationsSubsystem keeps the shares of each settlement in the member
// vector places_, grown by know_settlements, which the people ledger's calls
// moved and displaced make; a Place& must not be used after such a call in
// the same block. Not compiled.
#include "worldgen/history/relations.hpp"

namespace worldgen::history {

void RelationsSubsystem::know_settlements(const State& state) {
    places_.push_back(Place{});
    (void)state;
}

void RelationsSubsystem::displaced(const State& state, std::int64_t from, std::int64_t race, std::int64_t people,
                                   std::int64_t cause) {
    know_settlements(state);
    (void)from;
    (void)race;
    (void)people;
    (void)cause;
}

void RelationsSubsystem::bad_places(StepContext& ctx, std::int64_t id) {
    Place& p = place(id);  // expect: history-member-held-reference
    displaced(ctx.state(), id, 1, 50, 7);
    p.owner = 2;

    const Place* q = &place(id);  // expect: history-member-held-reference
    places_.emplace_back();
    (void)q->ruling;

    auto& r = place(id);  // expect: history-member-held-reference
    know_settlements(ctx.state());
    r.entries.clear();
}

// Allowed: fields copied first; re-fetched after the call; the reference
// unused after it; a type of the same name prefix.
void RelationsSubsystem::good_places(StepContext& ctx, std::int64_t id) {
    const Place& p = place(id);
    const std::int64_t owner = p.owner;
    displaced(ctx.state(), id, 1, 50, owner);
    Place& w = place(id);
    w.owner = owner;
    const PlaceMemory& m = memory_.front();
    places_.push_back(Place{});
    (void)m.settlement;
}

}  // namespace worldgen::history
