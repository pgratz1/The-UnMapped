#pragma once

// The one place of the P5-03a tests' library names that the spec (docs/specs/p5-03a-relations-followups.md r1)
// gives but that do not exist before the implementation (phase 1 is written against main at 0.24.0):
//   3.5   void RelationsSubsystem::credit(std::int64_t race_a, std::int64_t race_b, std::int64_t units,
//         std::int64_t event)                                                       (spec name, public)
//   13(b) std::int64_t DispositionView::band(std::int64_t race_a, std::int64_t race_b) const
//         bool DispositionView::met(std::int64_t race_a, std::int64_t race_b) const  (spec names)
//   9.5   SUMMARY_MIXED_MAX, "new constant in digest.hpp": GUESS worldgen::history::SUMMARY_MIXED_MAX
//   8 A2  RelationsSubsystem::disposition valid from construction: GUESS the constructor
//         RelationsSubsystem(PeoplesSubsystem&, const ConflictSubsystem&, const data::ContentSet&,
//                            const HistoryInputs&) (the spec: "from HistoryInputs and the content set")
//
// Integration (phase 2): define UNMAPPED_P503A_API (one line below, or -D in worldgen/tests/CMakeLists.txt) once the
// implementation is merged; until then every adapter call FAILs with "P5-03a API not integrated", so the cases
// that need the new names fail by name instead of failing to compile. Everything else of the P5-03a tests uses
// names older than P5-03a.

#include <cstdint>
#include <memory>

#include "p5_03_relations_api.hpp"
#include "worldgen/history/digest.hpp"

namespace p503a_api {

namespace h = worldgen::history;

#if defined(UNMAPPED_P503A_API)
inline constexpr bool INTEGRATED = true;
inline void credit(h::RelationsSubsystem& r, std::int64_t a, std::int64_t b, std::int64_t units, std::int64_t event) {
    r.credit(a, b, units, event);
}
inline std::int64_t view_band(const h::DispositionView& v, std::int64_t a, std::int64_t b) { return v.band(a, b); }
inline bool view_met(const h::DispositionView& v, std::int64_t a, std::int64_t b) { return v.met(a, b); }
inline std::int64_t summary_mixed_max() { return static_cast<std::int64_t>(h::SUMMARY_MIXED_MAX); }
// 8 A2: "B(a, b) and the pairs are computed when the subsystem is constructed in stages/history.cpp (from
// HistoryInputs and the content set)": GUESS a constructor taking the inputs after the content set.
inline std::unique_ptr<h::RelationsSubsystem> constructed(h::PeoplesSubsystem& peoples, h::ConflictSubsystem& conflict,
                                                          const worldgen::data::ContentSet& c,
                                                          const eng::HistoryInputs& in) {
    return std::make_unique<h::RelationsSubsystem>(peoples, conflict, c, in);
}
#else
inline constexpr bool INTEGRATED = false;
inline void credit(h::RelationsSubsystem&, std::int64_t, std::int64_t, std::int64_t, std::int64_t) {
    FAIL("P5-03a API not integrated: RelationsSubsystem::credit (3.5); define UNMAPPED_P503A_API");
}
inline std::int64_t view_band(const h::DispositionView&, std::int64_t, std::int64_t) {
    FAIL("P5-03a API not integrated: DispositionView::band (13); define UNMAPPED_P503A_API");
    return -1;
}
inline bool view_met(const h::DispositionView&, std::int64_t, std::int64_t) {
    FAIL("P5-03a API not integrated: DispositionView::met (13); define UNMAPPED_P503A_API");
    return false;
}
inline std::int64_t summary_mixed_max() {
    FAIL("P5-03a API not integrated: SUMMARY_MIXED_MAX (9.5); define UNMAPPED_P503A_API");
    return -1;
}
inline std::unique_ptr<h::RelationsSubsystem> constructed(h::PeoplesSubsystem&, h::ConflictSubsystem&,
                                                          const worldgen::data::ContentSet&, const eng::HistoryInputs&) {
    FAIL("P5-03a API not integrated: the RelationsSubsystem constructor of 8 A2; define UNMAPPED_P503A_API");
    return nullptr;
}
#endif

inline constexpr std::int64_t SUMMARY_MIXED_MAX_SPEC = 3;   // 9.5 (the spec's value)

}  // namespace p503a_api
