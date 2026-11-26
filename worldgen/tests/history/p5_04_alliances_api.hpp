#pragma once

// The one place of the P5-04 tests' library names that the spec (docs/specs/p5-04-alliances-trade.md r2) gives but
// that do not exist before the implementation (phase 1 is written against p5/03a-int at 0.25.0), and of the names it
// does not fix:
//   12    Timeline::alliances_at(year) -> std::vector<AllianceState> {a, b, since, since_event}, allies_of(civ, year),
//         alliance_groups_at(year), trades_at(year) -> std::vector<std::pair<a, b>>   (spec names; AllianceState:
//         GUESS worldgen::history::AllianceState at namespace scope, as RaceRelationState)
//   10.2  SUMMARY_ALLIANCES_MAX (10, digest.hpp)                                        (spec name)
//   11.2  EventGroup::alliances = 7, ALL_GROUPS 0xFF (history_browser.hpp)                (the code by cast)
//   11.3  MarkerOptions::sites (default on)                                              (spec name)
//         YearMarkers' mine entries: GUESS `std::vector<SiteMarker> sites` with members site, x, y, kind (entry type
//         6, ascending site id), and marker_glyph(GlyphKey{19}) the mine glyph
//   9.2   db::SCHEMA_VERSION 11
//
// Integration (phase 2): define UNMAPPED_P504_API (one line below, or -D in worldgen/tests/CMakeLists.txt) once the
// implementation is merged; until then every adapter call FAILs with "P5-04 API not integrated", so the cases that
// need the new names fail by name instead of failing to compile. Everything else of the P5-04 tests uses names
// older than P5-04 (and fails on the 0.25.0 library where it tests new behaviour).

#define UNMAPPED_P504_API 1  // integrated (phase 2)

#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

#include "worldgen/history/digest.hpp"
#include "worldgen/history/timeline.hpp"
#include "worldgen/inspect/history_browser.hpp"
#include "worldgen/inspect/markers.hpp"

namespace p504api {

namespace h = worldgen::history;
namespace ins = worldgen::inspect;

// (a, b, since, since_event)
using Alliance = std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t>;
using Pair = std::pair<std::int64_t, std::int64_t>;
// (site, x, y, kind)
using Mine = std::tuple<std::int64_t, std::int64_t, std::int64_t, std::int64_t>;

inline constexpr std::int64_t SUMMARY_ALLIANCES_MAX_SPEC = 10;   // 10.2
inline constexpr std::uint32_t ALL_GROUPS_SPEC = 0x1FF;           // 11.2
inline constexpr std::uint8_t GROUP_ALLIANCES = 7;                // 11.2
inline constexpr std::uint8_t MINE_GLYPH = 19;                    // 11.3: 16 + site kind 3
inline constexpr std::int64_t SCHEMA_SPEC = 13;                   // 9.2 (13 at 0.29.0)

inline ins::EventGroup alliances_group() { return static_cast<ins::EventGroup>(GROUP_ALLIANCES); }

#if defined(UNMAPPED_P504_API)
inline constexpr bool INTEGRATED = true;
inline std::vector<Alliance> alliances_at(const h::Timeline& t, std::int64_t y) {
    std::vector<Alliance> out;
    for (const auto& a : t.alliances_at(y)) out.emplace_back(a.a, a.b, a.since, a.since_event);
    return out;
}
inline std::vector<std::int64_t> allies_of(const h::Timeline& t, std::int64_t c, std::int64_t y) {
    return t.allies_of(c, y);
}
inline std::vector<std::vector<std::int64_t>> alliance_groups_at(const h::Timeline& t, std::int64_t y) {
    return t.alliance_groups_at(y);
}
inline std::vector<Pair> trades_at(const h::Timeline& t, std::int64_t y) {
    std::vector<Pair> out;
    for (const auto& p : t.trades_at(y)) out.emplace_back(p.first, p.second);
    return out;
}
inline std::int64_t summary_alliances_max() { return static_cast<std::int64_t>(h::SUMMARY_ALLIANCES_MAX); }
inline void set_sites(ins::MarkerOptions& o, bool on) { o.sites = on; }
inline std::vector<Mine> mines(const ins::YearMarkers& m) {
    std::vector<Mine> out;
    for (const auto& s : m.sites) out.emplace_back(s.site, s.x, s.y, s.kind);
    return out;
}
#else
inline constexpr bool INTEGRATED = false;
inline std::vector<Alliance> alliances_at(const h::Timeline&, std::int64_t) {
    FAIL("P5-04 API not integrated: Timeline::alliances_at (12); define UNMAPPED_P504_API");
    return {};
}
inline std::vector<std::int64_t> allies_of(const h::Timeline&, std::int64_t, std::int64_t) {
    FAIL("P5-04 API not integrated: Timeline::allies_of (12); define UNMAPPED_P504_API");
    return {};
}
inline std::vector<std::vector<std::int64_t>> alliance_groups_at(const h::Timeline&, std::int64_t) {
    FAIL("P5-04 API not integrated: Timeline::alliance_groups_at (12); define UNMAPPED_P504_API");
    return {};
}
inline std::vector<Pair> trades_at(const h::Timeline&, std::int64_t) {
    FAIL("P5-04 API not integrated: Timeline::trades_at (12); define UNMAPPED_P504_API");
    return {};
}
inline std::int64_t summary_alliances_max() {
    FAIL("P5-04 API not integrated: SUMMARY_ALLIANCES_MAX (10.2); define UNMAPPED_P504_API");
    return -1;
}
inline void set_sites(ins::MarkerOptions&, bool) {
    FAIL("P5-04 API not integrated: MarkerOptions::sites (11.3); define UNMAPPED_P504_API");
}
inline std::vector<Mine> mines(const ins::YearMarkers&) {
    FAIL("P5-04 API not integrated: YearMarkers' site entries (11.3); define UNMAPPED_P504_API");
    return {};
}
#endif

}  // namespace p504api
