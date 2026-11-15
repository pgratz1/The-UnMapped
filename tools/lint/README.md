# Banned-construct lint

`determinism_lint.py` scans the world generator's output-path C++ sources for
constructs that spec P0-01 section 4 (`docs/specs/p0-01-deterministic-core.md`)
bans because they can make `world.db` differ between platforms, compilers or
runs. It catches only what can be found mechanically with few false positives;
it does not replace the determinism audit.

Owner: the determinism auditor (`.claude/agents/determinism-auditor.md`).
Python 3 standard library only.

## Running it

From the repository root (`-B` stops Python writing `__pycache__` into the tree):

```
python3 -B tools/lint/determinism_lint.py              # scan the output path
python3 -B tools/lint/determinism_lint.py --self-test  # check the lint itself
python3 -B tools/lint/determinism_lint.py --list-rules
python3 -B tools/lint/determinism_lint.py PATH ...     # scan given files or directories
```

With no paths it scans every C++ file (`.h .hh .hpp .hxx .c .cc .cpp .cxx .ipp
.inl .tpp`) under `worldgen/`, excluding `worldgen/tests/`. `extern/` and
`tools/` are never in the default scope. Paths given on the command line are
scanned as given, with no exclusions. On Windows use `py -3 -B` or `python -B`.

Output is one line per finding, `path:line:col: rule: message`, sorted by
path and line, and a count on standard error. Exit status: 0 clean, 1 findings
(or a self-test failure), 2 usage error.

## Rules

Comments, string literals (including raw strings and encoding prefixes) and
character literals are ignored; digit separators (`1'000`) are handled. The
header name of an `#include` is checked.

| Rule | Reports | Spec |
| --- | --- | --- |
| `float-type` | `float`, `double`, `long double`, `__float128`, `_FloatN`, `std::floatN_t`, `std::bfloat16_t` | 4: integer and fixed-point only |
| `banned-include` | `#include` of `<cmath>`, `<math.h>`, `<random>`, `<chrono>`, `<ctime>`, `<time.h>`, `<unordered_map>`, `<unordered_set>`, `<locale>`, `<clocale>` | 4 |
| `std-math` | `std::sin`, `std::pow`, `std::sqrt`, `std::floor`, ... (every `<cmath>` function, called or instantiated as `std::name(` or `std::name<`) | 4, 9 |
| `std-random` | `std::shuffle`, `std::sample`, `std::random_shuffle` (and `std::ranges::` forms), every `<random>` engine and distribution | 4, 6 |
| `c-rand` | calls of `rand(`, `srand(`, `std::rand(`, `::rand(` | 4 |
| `wall-clock` | calls of `time(`, `clock(`, `timespec_get(`, `gettimeofday(`, `clock_gettime(`; any `std::chrono` | 4 |
| `std-hash` | `std::hash` | 4 |
| `unordered-container` | `std::unordered_map/set/multimap/multiset` (also `std::pmr::`), in case one arrives through a transitive include | 4 |
| `locale` | calls of `setlocale(`; `std::locale`; `.imbue(` / `->imbue(` | 4 |
| `long-type` | `long`, `unsigned long`, `signed long`, `long int` (not `long long`, not `long double`, which `float-type` reports) | 4: `long` is 32-bit on Windows |
| `wchar-type` | `wchar_t` | 4: 16-bit on Windows, 32-bit on Linux |
| `wide-int-type` | `__int128`, `__uint128_t`, `_umul128`, `_mul128`, `_udiv128`, `_div128`, `__umulh`, `__mulh` outside the allow-listed file | 5.3 |
| `unstable-sort` | `std::sort`, `std::partial_sort`, `std::partial_sort_copy`, `std::nth_element`, `std::partition` (and `std::ranges::` forms). Flagged for review: suppress each one with the reason the order is fully determined | 4 |
| `bad-suppression` | a suppression comment that is malformed, names an unknown rule or gives no reason | - |
| `unused-suppression` | a suppression that silences nothing (stale, or names the wrong rule) | - |

For `c-rand`, `wall-clock` and `setlocale`, member calls (`x.time()`,
`p->clock()`), other namespaces' functions (`other::time()`) and declarations
(`int time();`) are not reported. The declaration test is a heuristic: a name
preceded by an identifier other than `return`/`throw`/`case`/..., or by `*`,
`&` or `>`, is taken to be a declaration, so `a * time(nullptr)` is missed.

**Allow-list for `wide-int-type`.** Spec P0-01 5.3 permits a compiler-specific
fast path only inside `mul_u64_wide` and `divmod_u128_u64`, in
`worldgen/src/core/wide.cpp`. That file is the only entry in
`WIDE_TYPE_ALLOWLIST` at the top of the script. Every other rule still applies
there. Adding a file to the allow-list needs a spec change.

## Suppressing a finding

A finding the reviewer has checked is silenced with a comment that names the
rule and gives the reason:

```cpp
// determinism-lint: allow(unstable-sort) -- comparator orders by (region_id, cell_index), unique per element
std::sort(cells.begin(), cells.end(), by_region_then_index);

std::sort(ids.begin(), ids.end());  // determinism-lint: allow(unstable-sort) -- uint64 ids: equal elements are indistinguishable
```

- Syntax: `determinism-lint: allow(rule[, rule...]) -- reason`, in a `//` or
  `/* */` comment.
- Placement: at the end of the offending line, or on its own line directly
  above it (several suppression lines may be stacked). For a construct split
  over several lines, the finding is on the line where it starts.
- The reason is mandatory (at least 10 characters) and should say why the
  result cannot depend on platform or library, not merely that it is fine.
- A suppression that is malformed, names an unknown rule, has no reason, or
  suppresses nothing is itself a finding, so suppressions cannot go stale
  silently.
- Suppressions are reviewed in the determinism audit. Prefer changing the code
  (`std::stable_sort`, fixed-width types) over suppressing.

## Self-test

`selftest/` holds snippet files (not compiled, not part of any build). A line
that must be reported ends with a marker comment `// expect: rule[, rule]`;
every other line must produce nothing. `--self-test` checks that the lint
reports exactly the marked `(line, rule)` pairs in every file, and that every
rule is exercised by at least one snippet. A file whose name contains
`allowlisted` is scanned as if it were `worldgen/src/core/wide.cpp`.

| File | Checks |
| --- | --- |
| `bad_constructs.cpp` | every rule fires on its constructs |
| `bad_suppressions.cpp` | valid suppressions silence; malformed, reasonless, unknown-rule and unused ones are reported |
| `good_ignored.cpp` | comments, string/char/raw literals, digit separators, `long long`, identifiers containing banned words, member and other-namespace functions, declarations, the project's own functions |
| `good_wide_allowlisted.cpp` | `__int128` is allowed in the allow-listed file, other rules still apply |

When a rule is added or changed, add bad and good lines for it here.
