#pragma once

// The stub subsystems of P3-04 (spec section 7), replaced by P3-05 and
// P3-06: S1 the stub home region, S2 the stub placement of one
// civilization with one settlement per awakening, and the test-only tension
// source S4.

#include <cstdint>

#include "worldgen/data/content.hpp"
#include "worldgen/history/engine.hpp"
#include "worldgen/history/inputs.hpp"
#include "worldgen/history/names.hpp"

namespace worldgen::history {

// The settlement population the stub founds (7.2 item 3).
inline constexpr std::int64_t STUB_POPULATION = 200;

// S1 and S2. The name registry is built when the handler is made, from every
// region.name, age.name and god.name of the inputs, in that order (7.2,
// P3-03 8.2); names are drawn in creation order (civilization, then
// settlement, per awakening).
class StubAwakening final : public AwakeningHandler {
public:
    // islet_max_dry_cells is history.engine's (4.1).
    StubAwakening(const data::ContentSet& content, const HistoryInputs& inputs, std::int64_t islet_max_dry_cells);

    // S1: the best-scoring region (habitability_mean desc, walk_cells desc,
    // id asc) of the first non-empty set of: non-islet free regions,
    // non-islet regions, all regions.
    std::int64_t awakening_place(const StepContext& context, std::int64_t race) override;

    // S2: civ_founded, the civilization, the home region if free,
    // settlement_founded, the settlement (the first settlement_kinds entry,
    // thriving, population 200), the capital. FatalError "history: stub: no
    // settlement site in region <id>" when no cell is free.
    void awaken(StepContext& context, std::int64_t race, std::int64_t awakening_event) override;

private:
    const data::ContentSet& content_;
    std::int64_t islet_max_dry_cells_;
    NameRegistry registry_;
    std::int64_t place_ = 0;  // the place awakening_place chose for the awakening in progress
};

namespace testing {

// S4 (test seam, not shipped): reports `value` when the quiet step's end
// year (its start + length) is a multiple of `period`, else 0; the trigger
// is the fixed event id `trigger`. FatalError if period < 1.
class ForcedTension final : public TensionSource {
public:
    ForcedTension(std::int64_t period, std::int64_t value, std::int64_t trigger);
    TensionReport tension(const StepContext& context) override;

private:
    std::int64_t period_;
    std::int64_t value_;
    std::int64_t trigger_;
};

}  // namespace testing

}  // namespace worldgen::history
