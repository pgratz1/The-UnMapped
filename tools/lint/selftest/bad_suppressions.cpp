// Self-test snippet: suppression hygiene. Not compiled.
// Malformed suppressions are written as block comments so that the trailing
// "expect:" marker is not read as part of their reason.
#include <algorithm>
#include <vector>

void g(std::vector<int>& v) {
    // A valid suppression on the line above silences only the named rule.
    // determinism-lint: allow(unstable-sort) -- ints compare as a strict total order, equal ints are indistinguishable
    std::sort(v.begin(), v.end());

    // A valid trailing suppression on the same line.
    std::sort(v.begin(), v.end());  // determinism-lint: allow(unstable-sort) -- ints: equal elements are indistinguishable

    // Several suppression lines may stack above one line.
    // determinism-lint: allow(unstable-sort) -- ints: equal elements are indistinguishable
    // determinism-lint: allow(float-type) -- test only: two rules on one line, each suppressed
    std::sort(v.begin(), v.end()); double d = 0;

    /* determinism-lint: allow(unstable-sort) */  // expect: bad-suppression (no reason)
    std::sort(v.begin(), v.end());  // expect: unstable-sort

    /* determinism-lint: allow(unstable-sort) -- ok */  // expect: bad-suppression (reason too short)
    std::sort(v.begin(), v.end());  // expect: unstable-sort

    /* determinism-lint: allow(no-such-rule) -- a reason that is long enough */  // expect: bad-suppression
    std::sort(v.begin(), v.end());  // expect: unstable-sort

    /* determinism-lint: allow unstable-sort */  // expect: bad-suppression (malformed)
    std::sort(v.begin(), v.end());  // expect: unstable-sort

    // The wrong rule does not silence the finding, and is itself unused.
    /* determinism-lint: allow(float-type) -- wrong rule named here on purpose */  // expect: unused-suppression
    std::sort(v.begin(), v.end());  // expect: unstable-sort

    /* determinism-lint: allow(unstable-sort) -- nothing to suppress below */  // expect: unused-suppression
    int x = 0;
    (void)x;
}
