#pragma once

// The one switch for the tests of the P3-10 second round (docs/specs/p3-10-followups.md section 12, items 18 to
// 26; generator stays 0.19.0) that need library API the round introduces or changes: the race field
// hidden_years in ConflictRace and history.conflict without hidden_years (P3-06 r17 22.2), the tuning field
// quarrel_min_settlements (22.1) and the named check conflict_quarrel_strain_max (22.7) in ConflictTuning, the
// peoples hook lie_low(civ, settlement, until_year) and the predicate lying_low(const State&, civ, year)
// (P3-05 r15 22.7a), and history.engine's quiet_min_years in the engine tuning (P3-04 r15 4.1). The tests were
// written from the specs before the implementation existed (test-writer rule).
//
// UNMAPPED_P3_10_R3_API is 0 while the library is that of the first P3-10 round: every block guarded by it
// compiles to nothing, and the tests that need no new API (data listings and loader errors, the triple-letter
// rule through the existing names API, quiet rows through the stub engine, row invariants, golden values)
// compile and fail until the implementation lands. At integration set it to 1 (or build with
// -DUNMAPPED_P3_10_R3_API=1) and adapt the interim names (marked GUESS in the files that use them) to the
// engineer's.
//
// A macro set on the command line wins.

#ifndef UNMAPPED_P3_10_R3_API
#define UNMAPPED_P3_10_R3_API 0
#endif
