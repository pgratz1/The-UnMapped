#pragma once

// The switch for the tests of spec P3-07 (docs/specs/p3-07-history-report.md) that call the library API
// of its section 5.1, in the pattern of p3_09_switch.hpp. The tests were written from the spec before the
// implementation existed (test-writer rule).
//
// UNMAPPED_P3_07_REPORT is 1 when worldgen/inspect/history_report.hpp exists. While it is 0 every block
// guarded by it compiles to nothing; the fixture-building tests (14.3: the C++ copy of each fixture has
// the reference's table hashes) and the generation of the golden world cases need no P3-07 name and run
// either way. A macro set on the command line wins.

#ifndef UNMAPPED_P3_07_REPORT
#if __has_include("worldgen/inspect/history_report.hpp")
#define UNMAPPED_P3_07_REPORT 1
#else
#define UNMAPPED_P3_07_REPORT 0
#endif
#endif
