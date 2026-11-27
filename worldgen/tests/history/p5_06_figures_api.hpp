#pragma once

// The one place of the P5-06 tests' library names that the spec (docs/specs/p5-06-figures.md r3) gives but that do
// not exist before the implementation (phase 1 is written against P5-04 as built at 0.26.0), and of the names it
// does not fix (GUESS: to be set at integration):
//   12    Timeline::ruler_at(civ, year), figures_alive(year), generals_of(civ, year), figure_at(figure, year) ->
//         FigureState {id, role, civ, status, alive}, events_of_figure(figure)                (spec names)
//   11.3  EventGroup::figures = 8, ALL_GROUPS 0x1FF, event_group_word "figures"           (the code by cast)
//   11.4  GLYPH_TOWER = 20, MARKER_TOWER (120, 60, 180), marker_glyph(GlyphKey{20})        (spec names)
//   9.4   db::SCHEMA_VERSION 12
//   7.11  I0: the history stage without FiguresSubsystem and with no FigureView set: GUESS
//         `p506api::history_stage_without_figures(content, inputs)` filled at integration from the engineer's
//         configuration (the P5-04 tests built theirs from the stage's own configuration, p503api::RelConfig)
//   7.10  StepContext::draw_at: GUESS `p506api::draw_at(stream key, Y, Z, year, a, b, k)` through a StepContext the
//         engineer's tests can build
//
// Integration (phase 2): define UNMAPPED_P506_API (one line below, or -D in worldgen/tests/CMakeLists.txt) once the
// implementation is merged. Until then INTEGRATED is false and every test case that needs the stage's behaviour
// returns at its start with a MESSAGE ("P5-06 not integrated"), so the tree builds and its tests pass at 0.26.0; the
// cases that test the spec's arithmetic by hand (history_figures_unit_test.cpp) run either way.

#include <cstdint>
#include <string>
#include <vector>

#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/markers.hpp"

#define UNMAPPED_P506_API 1

namespace p506api {

namespace h = worldgen::history;
namespace ins = worldgen::inspect;

inline constexpr std::int64_t SCHEMA_SPEC = 14;                    // 9.4
inline constexpr const char* GENERATOR_SPEC = "0.30.0";            // header
inline constexpr std::int64_t STREAMS_SPEC = 107;                   // 3
inline constexpr std::int64_t KINDS_SPEC = 99;                     // 8.1
inline constexpr std::uint32_t ALL_GROUPS_SPEC = 0x3FF;            // 11.3
inline constexpr std::uint8_t GROUP_FIGURES = 8;                   // 11.3
inline constexpr std::uint8_t GLYPH_TOWER_SPEC = 20;               // 11.4: 16 + site kind 4
inline constexpr std::int64_t SUMMARY_FIGURES_MAX_SPEC = 6;        // 10.2
inline constexpr std::int64_t OLD_RUIN_YEARS_SPEC = 1500;          // 25.1 (e)
inline constexpr std::int64_t CALAMITY_MAJOR_PERMILLE_SPEC = 100;  // 25.2
inline constexpr std::int64_t CALAMITY_MAJOR_MAX_SPEC = 8;         // 25.2

inline ins::EventGroup figures_group() { return static_cast<ins::EventGroup>(GROUP_FIGURES); }

struct FigureState {
    std::int64_t id = 0, role = 0, civ = 0, status = 0;
    bool alive = false;
};

#if defined(UNMAPPED_P506_API)
inline constexpr bool INTEGRATED = true;
inline std::int64_t ruler_at(const h::Timeline& t, std::int64_t civ, std::int64_t y) { return t.ruler_at(civ, y); }
inline std::vector<std::int64_t> figures_alive(const h::Timeline& t, std::int64_t y) { return t.figures_alive(y); }
inline std::vector<std::int64_t> generals_of(const h::Timeline& t, std::int64_t civ, std::int64_t y) {
    return t.generals_of(civ, y);
}
inline FigureState figure_at(const h::Timeline& t, std::int64_t f, std::int64_t y) {
    const auto s = t.figure_at(f, y);
    return FigureState{s.id, s.role, s.civ, s.status, s.alive};
}
inline std::vector<std::int64_t> events_of_figure(const h::Timeline& t, std::int64_t f) {
    const auto s = t.events_of_figure(f);
    return std::vector<std::int64_t>(s.begin(), s.end());
}
#else
inline constexpr bool INTEGRATED = false;
inline std::int64_t ruler_at(const h::Timeline&, std::int64_t, std::int64_t) { return 0; }
inline std::vector<std::int64_t> figures_alive(const h::Timeline&, std::int64_t) { return {}; }
inline std::vector<std::int64_t> generals_of(const h::Timeline&, std::int64_t, std::int64_t) { return {}; }
inline FigureState figure_at(const h::Timeline&, std::int64_t, std::int64_t) { return {}; }
inline std::vector<std::int64_t> events_of_figure(const h::Timeline&, std::int64_t) { return {}; }
#endif

}  // namespace p506api

// Every test case that needs the implementation starts with this: at 0.26.0 it returns (a MESSAGE, no check).
#define P506_REQUIRE_INTEGRATED()                                                        \
    do {                                                                                 \
        if (!p506api::INTEGRATED) {                                                      \
            MESSAGE("P5-06 not integrated (define UNMAPPED_P506_API): case not run");    \
            return;                                                                      \
        }                                                                                \
    } while (false)
