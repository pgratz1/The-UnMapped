#pragma once

// The switches for the tests of the P3-09 change (docs/specs/p3-09-conflict-balance.md r2, generator 0.18.0)
// that need library API the change introduces, in the pattern of p3_08_switch.hpp. The tests were written
// from the specs before the implementation existed (test-writer rule).
//
// UNMAPPED_P3_09_API is 0 while the library is that of 0.17.0: every block guarded by it compiles to
// nothing. It guards the seams the specs name: the eleven history.conflict fields of P3-06 r9 20.9 in
// ConflictTuning (names as the spec writes them) with the tuning message of conflict_enmity_hops, the three
// race fields of 20.12 in ConflictRace, and the PeoplesView readers of P3-05 r8 21 item 1 (site,
// founding_kind, hops; PeoplesSubsystem::settlement_name) with the signatures the spec gives. The tests that
// need no new API (data, catalogue, golden values, the invariants of 20.13 from rows, the [stats] keys of
// P3-07 r8 21.5 from rows) are not guarded: they compile now and fail until the implementation lands. At
// integration the lead (or whoever merges the engineer's branch) sets it to 1, or builds with
// -DUNMAPPED_P3_09_API=1, and adapts the interim names in history/p3_09_balance_api.hpp to the signatures
// the engineer records.
//
// UNMAPPED_P3_09_DIGEST is 1 when worldgen/history/digest.hpp (P3-07 5.1) exists: history_note of P3-07 r8
// 21.4 belongs to the history report, which is not implemented at 0.18.0; its tests compile once it is.
//
// A macro set on the command line wins.

#ifndef UNMAPPED_P3_09_API
#define UNMAPPED_P3_09_API 0
#endif

#ifndef UNMAPPED_P3_09_DIGEST
#if __has_include("worldgen/history/digest.hpp")
#define UNMAPPED_P3_09_DIGEST 1
#else
#define UNMAPPED_P3_09_DIGEST 0
#endif
#endif
