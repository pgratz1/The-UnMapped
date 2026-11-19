// lint-selftest-path: worldgen/src/stages/elevation_selftest.cpp
// Self-test snippet: scanned as a geography stage, where the sequential
// generator is the project's generator (P0-01 6) and history-sequential-rng
// does not apply. Not compiled.
#include "worldgen/core/rng.hpp"

namespace worldgen::stages {

std::uint64_t geography_draw(std::uint64_t key) {
    Rng rng(key);
    return rng.next();
}

}  // namespace worldgen::stages

// history-live-iteration applies to history code only: the same loop in a
// geography stage is not reported (no such State exists there).
namespace worldgen::stages {

void geography_loop(Things& state) {
    for (const auto& c : state.civs()) {
        state.found_civilization(c.id);
    }
}

}  // namespace worldgen::stages
