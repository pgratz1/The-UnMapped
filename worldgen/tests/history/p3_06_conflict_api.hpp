#pragma once

// The one place where the P3-06 conflict tests touch library API that the
// spec introduces (docs/specs/p3-06-expansion-conflict.md r4, sections 3.3,
// 8.2, 13, 16). Written from the spec before the implementation existed
// (test-writer rule). Every other P3-06 test file uses the names below and
// API older than P3-06 (through p3_04_engine_api.hpp and
// p3_05_peoples_api.hpp).
//
// The tests that need the subsystem compile only when the implementation's
// header exists: UNMAPPED_HAVE_CONFLICT is 1 when
// worldgen/history/conflict.hpp is on the include path, else 0, and those
// files are empty then. The data, catalogue and golden-world tests that need
// no P3-06 name are not guarded: they fail until the 0.16.0 data and code
// land.
//
// Names used as the spec gives them (13):
//   worldgen/history/conflict.hpp  class ConflictSubsystem final : Subsystem, TensionSource,
//                                  ConflictSubsystem(PeoplesSubsystem&, const data::ContentSet&),
//                                  steps() (T2, T6, Q4), run(StepContext&), tension(const StepContext&)
//   worldgen/core/streams.hpp      the five hist.conflict.* names after hist.peoples.split, 51 in all (3.3)
//
// GUESSES (the spec leaves them open; change here only):
//   - the constructor reads and checks history.conflict and throws worldgen::FatalError with the
//     16.4 tuning messages (P3-05's PeoplesSubsystem reads its tuning in its first step instead: if
//     conflict does the same, tuning_error() below runs one tiny history to reach the message);
//   - TensionSource::tension may be called from a test subsystem in Q4 after the conflict subsystem
//     has run (it is a pure read of what Q4 stored, 8.3 item 2), and returns what the engine's Q5
//     would get.
//
// Every contract violation is expected to throw worldgen::FatalError.

#if __has_include("worldgen/history/conflict.hpp")
#define UNMAPPED_HAVE_CONFLICT 1
#else
#define UNMAPPED_HAVE_CONFLICT 0
#endif

#include <string_view>
#include <vector>

namespace cfl {

// The five stream names of 3.3, in registration order (the spec's text).
inline const std::vector<std::string_view>& stream_names() {
    static const std::vector<std::string_view> v = {"hist.conflict.declare", "hist.conflict.battle",
                                                    "hist.conflict.battle_win", "hist.conflict.siege",
                                                    "hist.conflict.sack"};
    return v;
}

}  // namespace cfl

#if UNMAPPED_HAVE_CONFLICT && __has_include("worldgen/history/peoples.hpp")

#include <memory>

#include "p3_05_peoples_api.hpp"
#include "worldgen/history/conflict.hpp"

namespace cfl {

namespace h = worldgen::history;

using h::ConflictSubsystem;

// The shipped pair and the conflict subsystem as the shipped EngineConfig installs them (8.2, P3-04 6.4):
// subsystems peoples, conflict; conflict the only tension source.
struct Conflict {
    ppl::Peoples peoples;
    std::unique_ptr<h::ConflictSubsystem> sub;
    explicit Conflict(const worldgen::data::ContentSet& c)
        : sub(std::make_unique<h::ConflictSubsystem>(*peoples.sub, c)) {}
    [[nodiscard]] h::Subsystem* subsystem() const { return sub.get(); }
    [[nodiscard]] h::TensionSource* tension() const { return sub.get(); }
};

}  // namespace cfl

#endif  // UNMAPPED_HAVE_CONFLICT
