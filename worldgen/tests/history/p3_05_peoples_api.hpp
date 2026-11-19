#pragma once

// The one place where the P3-05 peoples tests touch library API that the
// spec introduces (docs/specs/p3-05-peoples.md r2, sections 3.2, 3.3, 11,
// 13, 16). Written from the spec before the implementation existed
// (test-writer rule). Every other P3-05 test file uses the names below and
// API older than P3-05 (through p3_04_engine_api.hpp).
//
// The peoples tests compile only when the implementation's header exists:
// UNMAPPED_HAVE_PEOPLES is 1 when worldgen/history/peoples.hpp is on the
// include path, else 0, and every P3-05 test file is empty then. (The
// P3-01/P3-03 data expectations of 0.15.0 are not guarded: they fail until
// the 0.15.0 data lands.)
//
// Names used as the spec gives them:
//   worldgen/history/peoples.hpp  PeoplesView (score, capacity, share,
//                                 neighbours, population, regions,
//                                 settlements, wanted_held, hunger,
//                                 last_reduction), PeoplesSubsystem (view,
//                                 note_reduction, ruin) (13)
//   HistoryInputs::biomes, habitability_layer, river_layer, flow_dir_layer (3.2)
//   worldgen/core/streams.hpp     the twelve hist.peoples.* names after
//                                 hist.chronology.awakening, 46 in all (3.3)
//
// Where the spec leaves a name open, the guess is marked GUESS and is the
// first thing to check at integration:
//   GUESS: the awakening handler is history::PeoplesAwakening (P3-05 2,
//          18.2 item 7), constructed from the PeoplesSubsystem it places
//          for: PeoplesAwakening(PeoplesSubsystem&).
//   GUESS: PeoplesSubsystem is default-constructible and reads its inputs
//          from the StepContext of its first step.
//   GUESS: Neighbour (13, `std::span<const Neighbour> neighbours`) is
//          history::Neighbour with members `region` and `cost`.
//   GUESS: the stream constants are streams::hist_peoples_<name> (the
//          P0-01 7.3 naming of the existing constants).
//
// Every contract violation is expected to throw worldgen::FatalError.

#if __has_include("worldgen/history/peoples.hpp")
#define UNMAPPED_HAVE_PEOPLES 1
#else
#define UNMAPPED_HAVE_PEOPLES 0
#endif

#if UNMAPPED_HAVE_PEOPLES

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "p3_04_engine_api.hpp"
#include "worldgen/core/streams.hpp"
#include "worldgen/history/peoples.hpp"

namespace ppl {

namespace h = worldgen::history;

using h::PeoplesSubsystem;
using h::PeoplesView;

// The twelve stream names of 3.3, in registration order (the spec's text).
inline const std::vector<std::string_view>& stream_names() {
    static const std::vector<std::string_view> v = {
        "hist.peoples.home",          "hist.peoples.civs",       "hist.peoples.found",
        "hist.peoples.outward",       "hist.peoples.famine",     "hist.peoples.famine_toll",
        "hist.peoples.plague",        "hist.peoples.plague_toll", "hist.peoples.plague_spread",
        "hist.peoples.abandon",       "hist.peoples.crisis",     "hist.peoples.split"};
    return v;
}
inline std::span<const std::string_view> registered_streams() { return worldgen::streams::all_stream_names; }

// A neighbour of 13's PeoplesView::neighbours. GUESS: member names.
struct Nb {
    std::int64_t region, cost;
    friend bool operator==(const Nb&, const Nb&) = default;
};
inline std::vector<Nb> neighbours(const PeoplesView& v, std::int64_t region) {
    std::vector<Nb> out;
    for (const auto& n : v.neighbours(region)) {
        out.push_back(Nb{n.region, n.cost});
    }
    return out;
}

// The peoples pair as the shipped EngineConfig installs it (6.4, 18.2 item 7).
// GUESS: the constructors.
struct Peoples {
    std::unique_ptr<h::PeoplesSubsystem> sub = std::make_unique<h::PeoplesSubsystem>();
    std::unique_ptr<h::PeoplesAwakening> awakening = std::make_unique<h::PeoplesAwakening>(*sub);
};

// The 3.2 members of HistoryInputs (names of the spec).
inline void set_peoples_inputs(eng::HistoryInputs& in, std::vector<worldgen::db::BiomeRow> biomes,
                               worldgen::db::LayerGrid habitability, worldgen::db::LayerGrid river,
                               worldgen::db::LayerGrid flow_dir) {
    in.biomes = std::move(biomes);
    in.habitability_layer = std::move(habitability);
    in.river_layer = std::move(river);
    in.flow_dir_layer = std::move(flow_dir);
}

}  // namespace ppl

#endif  // UNMAPPED_HAVE_PEOPLES
