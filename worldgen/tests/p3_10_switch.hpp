#pragma once

// The switch for the tests of the P3-10 change (docs/specs/p3-10-followups.md, generator 0.19.0) that need
// library API the change introduces, in the pattern of p3_08_switch.hpp and p3_09_switch.hpp. The tests were
// written from the specs before the implementation existed (test-writer rule).
//
// UNMAPPED_P3_10_API is 0 while the library is that of 0.18.0: every block guarded by it compiles to nothing.
// It guards the seams the specs name: the race fields cell_weights.inland/.port/.habitability,
// odds_min_permille and awakening_quarrel (P3-01 r11 3.4, 3.5), the history.peoples fields cause_memory_years
// and inland_cap (P3-05 r12 22.2, 22.5), the history.conflict fields search_weary_years and
// search_repeat_salience without odds_min (P3-06 r14 21.7, 21.9), PeoplesSubsystem::lie_low (P3-05 r12 22.7a)
// and the one last-refuge predicate race_last_refuge(SettlementsNow, civ) (22.7), with the names the specs
// write. Tests that need no new API (data, catalogue, rendering through render_event, golden values, the
// invariants from rows) are not guarded: they compile now and fail until the implementation lands. At
// integration the lead sets it to 1 (or builds with -DUNMAPPED_P3_10_API=1) and adapts the interim names to
// the signatures the engineer records.
//
// A macro set on the command line wins.

#ifndef UNMAPPED_P3_10_API
#define UNMAPPED_P3_10_API 0
#endif
