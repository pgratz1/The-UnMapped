#pragma once

// The one switch for the tests of the P3-08 change (docs/specs/p3-08-followups.md,
// generator 0.17.0) that need library API the change introduces: the
// region-name seams of P1-06 r3 10.5, the name filter of P3-02 r5 11.5,
// the race field cell_weights and the new HistoryInputs layers of P3-05 r6,
// and the new history.peoples fields. The tests were written from the specs
// before the implementation existed (test-writer rule).
//
// UNMAPPED_P3_08_API is 0 while the library is that of 0.16.0: every block
// guarded by it compiles to nothing, and the tests that need no new API
// (behaviour and expectations only: the renderer's "the" rule, the
// three-word value form, quiet eras, golden values) compile and fail until
// the implementation lands. At integration the lead (or whoever merges the
// engineer's branch) sets it to 1, or builds with
// -DUNMAPPED_P3_08_API=1, and adapts the interim seam names in the *_api.hpp
// headers to the signatures the engineer records.
//
// A macro set on the command line wins.

#ifndef UNMAPPED_P3_08_API
#define UNMAPPED_P3_08_API 1  // integrated with p3/08-impl (32086ab)
#endif
