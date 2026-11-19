# Determinism audit: P3-02 names library (generator unchanged, schema 5)

Auditor: determinism auditor, 2026-11-19. Change audited: `git diff origin/main...origin/p3/02-names`, the branch's own commit `9549429` on the merge base `fdf38c2`. A two-dot diff against `origin/main` also shows `docs/design/proposals.md`, because the branch predates `286213d`. That is not part of the change. Tests and the independent reference come from `origin/p3/02-names-tests` (`860f395`). Specs: `docs/specs/p3-02-names.md` (sections 4, 6 and 7) and `docs/specs/p3-00-history-overview.md` (4.5 rules H1 to H8, 4.6 keyed draws). Line numbers are those of `9549429`.

Scope:
- `worldgen/include/worldgen/core/hash.hpp`: `TAG_HIST`.
- `worldgen/include/worldgen/core/streams.hpp`: the three `hist.names.*` names.
- `worldgen/include/worldgen/history/draw.hpp` and `worldgen/src/history/draw.cpp`: `draw`, `pick`, `chance`, `weighted`.
- `worldgen/include/worldgen/history/names.hpp` and `worldgen/src/history/names.cpp`: `CultureNaming`, one attempt, `capitalize_root`, `NameRegistry`, `choose_name` and `make_name`, `roman`.
- `worldgen/CMakeLists.txt`.
- The engineer's smoke test, and the stream-count test edit in `stages_climate_offset_test.cpp`.

Code the change depends on was also read: `core/wide.hpp` (`U128` ordering), `core/checked.hpp`, `core/fx.hpp` (`Fx::ONE`), `core/error.hpp` (`fatal` is `[[noreturn]]`) and `stages::roman` in `regions.cpp:745-772`.

**Verdict: pass with notes.** No blocking finding. Every function in the change is a pure function of its integer and ASCII inputs, with bounds that hold in `uint64` and `U128`. The result does not depend on compiler, standard library, `char` signedness, locale or evaluation order. The library is not called by any stage, so no world byte changes (section 4). One finding needs a ruling before merge, although it is not a cross-platform defect: the implementation and the test writer disagree on whether `weighted` accepts a weight sum above 2^63 − 1 (S1). The integrated tests fail on that point on both compilers. Seven notes and two advisories. One lint rule added, `history-sequential-rng` (section 5).

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts. **Should-fix** breaks or leaves open a stated contract and needs action before merge. **Advisory** is fragile but currently safe. A **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

**S1. `weighted`: the bound on the weight sum is not agreed. The implementation is fatal above 2^63 − 1; the tests and the reference accept it.**
- **Implementation.** `draw.cpp:42-48` sums the weights with `checked_add` in `int64`. A sum above 2^63 − 1 is therefore fatal (`checked_add: int64 overflow`), as `draw.hpp:35-38` documents, citing P3-00 H6 ("sums in `int64`; a bound exceeded is fatal").
- **Tests.** `worldgen/tests/history/history_draw_test.cpp:241-247` (on `p3/02-names-tests`) and the golden vector `worldgen/tests/golden/p3-02-names.txt:100` (`weighted 0xffffffffffffffff 4611686018427387904,4611686018427387904,1 1`) use weights {2^62, 2^62, 1}, W = 2^63 + 1. They expect a result.
- **Reference.** `p3_names_reference.py:94-107` sums in unbounded integers.
- **Spec.** P3-02 4.1 says only "w ≥ 0, W ≥ 1, in uint128", and its seam list (`p3-02-names.md:272`) says "fatal if a weight < 0 or the sum is 0".
- **Effect.** On the integration (section 4), 2 test cases and 5 assertions fail, the same on GCC and Clang: "weighted gives a zero weight no u…" and "weighted golden vectors".
- **Why it is not blocking.** Both behaviours are defined and platform-independent, so this is not a determinism defect. It is a contract hole in the spec, and it has to be closed before a simulation spec relies on large weights.

Fix asked, preferred:
- **Spec writer.** P3-02 4.1 and the seam line state W ≤ 2^63 − 1, fatal above, as H6 says. Weights are `int64`, and with that bound the product (u >> 32) × W stays below 2^95, so one `mul_u64_wide` suffices.
- **Test writer.** Replace the W = 2^63 + 1 vectors with W just below the bound, for example {2^62, 2^62 − 2, 1}, keeping the "last weight's share below 2^-32" property. Add a fatal case at W = 2^63. Regenerate line 100 of the golden file from the reference with the bound added.
- **No change** to `draw.cpp`.

If the lead rules for the unbounded reading instead, `weighted` must accumulate W and the prefixes in `U128` and compare 2^32 × prefix in more than 128 bits once W ≥ 2^96. That is more code for a range no history quantity needs (H6 bounds populations to 2^31).

### Advisory

**A1. P3-02 7 allows "an ordered set or a hash set" for the registry.** `p3-02-names.md:256`. The implementation uses `std::set<std::string, std::less<>>` (`names.hpp:95`), which is right. A hash set would be `std::unordered_set`, which the lint bans outright (`unordered-container`, `banned-include`) and hard rule 1 bans in practice. Fix asked: the spec writer drops "or a hash set" in the next revision of P3-02, so that no later spec copies it. Nothing to change in code.

**A2. `names.cpp` reaches into a stage for `roman`.** `names.cpp:12` includes `worldgen/stages/regions.hpp`, and `history::roman` (`:259-264`) forwards to `stages::roman`. The behaviour is identical, with the same guard [1, 100000] and P1-06 RG11's table. The naming of regions (P1-06) and of history entities (P3-02) now share one function, so a change to it for one would change the other. That is intended today. Fix asked when P3-03 adds history stage files: move `roman` to a shared header (for example `worldgen/core/` or a `text` helper), so that the history library does not depend on a stage header. Not urgent.

### Notes

**N1. `pick` cannot overflow, and its result is in [0, n).** `draw.cpp:23-29`. n is checked to be in [1, 2^32] before use. (u >> 32) ≤ 2^32 − 1, so the product is at most (2^32 − 1) × 2^32 = 2^64 − 2^32 < 2^64. It is `uint64` throughout, with unsigned shifts by `32U`, no wrap and no signed value. The result ((u >> 32) × n) >> 32 is at most n − 1. The engineer's claim, (u >> 32) × n < 2^64 for n ≤ 2^32, is correct. In `pick_from` (`names.cpp:41-46`) `list.size()` converts `std::size_t` to `uint64` (the same width on every supported target), and the result is cast back after the fatal check on n, so the index is always within the list.

**N2. `chance` is exact.** `draw.cpp:31-36`. `p.raw()` is checked to be in [0, 2^32] (`Fx::ONE` raw is 2^32, `fx.hpp:76`) before the cast to `uint64`, so the cast never sees a negative value. p = 0 is never true, p = ONE is always true, and otherwise the probability is p.raw / 2^32 exactly.

**N3. `weighted` is exact, matches both spec wordings and never picks a zero weight.**
- **Bounds.** `draw.cpp:38-63`. Negative weights are fatal, the sum is checked (S1), and W = 0 is fatal. Then lhs = (u >> 32) × W < 2^32 × W ≤ 2^95. The comparison 2^32 × prefix ≤ 2^95 is `mul_u64_wide` in `U128`, and `prefix` ≤ W cannot overflow `int64`.
- **Ordering.** `U128` compares numerically because its defaulted `<=>` compares members in declaration order, and the declaration order is `hi` and then `lo` (`wide.hpp:16-22`). This is load-bearing: swapping the two members would silently break `weighted`.
- **Spec wordings.** For integer prefixes P, lhs < 2^32 × P ⇔ ⌊lhs / 2^32⌋ < P. The P3-02 4.1 form is therefore identical to P3-00 4.6's "the first index whose prefix sum exceeds (u >> 32) × Σ >> 32". The reference checks both (`weighted_p3_00`).
- **Zero weights and termination.** A zero weight leaves the prefix unchanged, so its index can only be returned if the previous index already was. Index 0 with weight 0 compares lhs < 0, which is false. At the last positive weight, P = W > lhs / 2^32, so the loop returns. The trailing `fatal` (`:62`) is unreachable and `[[noreturn]]`, so MSVC needs no return value after it.

**N4. The keyed draw follows the spec word for word, and the draws are order-free.** `draw.cpp:18-21`: `hash_words(TAG_HIST, {stream_key, w0, a, b, k})`. In the generator (`names.cpp:100-132`), u(j) = draw(S_K, t, c, e, j), as P3-02 4.2 gives it (attempt in w0, culture in a, entity in b, counter in k). The counter is j = 0 for the form and j = 1 for the pattern. From there it increases by one per O, N or C symbol only, and a separator draws nothing. Each u(j) is a pure function of its arguments, with no shared state and no sequential generator (rule H4). So the unspecified evaluation order of operands (`out.root += pick_from(…, u(j))`) cannot change a value, and the `two-draws` concern does not arise. The stream key is `stream_key(seed, name_stream(kind))`, computed once per name (`:219`) or per attempt (`:202`). `TAG_HIST` = 0x545349483A4D4E55 is "UNM:HIST" in P0-01 7.2's little-endian packing and is distinct from every other tag. The three stream names are appended to `all_stream_names` (now 33), which only tests read.

**N5. String handling: ASCII bytes, no locale, no dependence on `char` signedness, identical on MSVC.**
- **Upper-casing.** `to_upper` (`names.cpp:29-31`) is `c - 'a' + 'A'`. It is applied only after `is_lower(c)`, a range test on `'a'…'z'` (`:20-22`, `:192-193`). The arithmetic is done in `int` and the result is in `'A'…'Z'`, so the cast back to `char` is exact. No `toupper` or `<cctype>` is used, and no locale.
- **Capitalization.** In `capitalize_root` (`:187-198`) the "after a `-`" flag is computed from the character before it is changed. A letter after `'` stays lower case, as the spec says.
- **Signedness.** `letter_count`, `separators_ok` and `root_valid` (`:48-85`) are byte tests and `std::string_view::find`. A byte ≥ 0x80 fails `is_lower` whether `char` is signed or unsigned. The loader only admits `[a-z]` syllables, and patterns and forms in the grammars of P3-01 (`kinds_history.cpp:39-100`), so names are ASCII.
- **Forms.** `fill_form` (`:89-98`) finds `{root}` and checks that there is exactly one. Only `std::string` and `std::string_view` operations with defined results are used, which behave the same in libstdc++, libc++ and the MSVC STL.

**N6. The registry's order is never observed, and would be portable if it were.** `NameRegistry` (`names.hpp:87-96`, `names.cpp:205-215`) offers `contains`, `add` and `size` only, and the generator never iterates it. Membership in an ordered set is byte equality, whatever the comparator. The comparator `std::less<>` on `std::string` and `std::string_view` uses `char_traits<char>::compare`. The standard specifies that comparison as one on `unsigned char`, so even the internal order is the same with every library. P3-00 H3 ("maps are ordered by integer keys only") does not apply to a membership set, and P3-02 7 allows it explicitly. `choose_name` (`:217-252`) follows 4.3 exactly:
- the first valid attempt t = 0 … 63 that is new is accepted;
- otherwise the first valid attempt is taken, with the smallest numeral k ≥ 2 that is new;
- the length is checked against 64 bytes after the numeral;
- `roman` is guarded to [1, 100000];
- every fatal message is fixed text plus the culture id and the kind name.

**N7. Nothing here can change existing world bytes.** No stage, writer, inspection or CLI file includes a `worldgen/history/` header (grep of `worldgen/src` and `tools/worldgen-cli`). The tag and stream names are new constants, and `all_stream_names` is read only by tests. Observed: the integration of P3-01 and P3-02 gives the same file hash as `p3/01-int` alone for `--until none` seed 42 (`7ce809fb…`) and 512 seed 42 (`dd76b4cf…`), and all 12 `world-hashes.sh` cases agree between GCC and Clang. `GENERATOR_VERSION` correctly stays unchanged: 0.11.0 on the branch, and 0.12.0 once it lands on top of P3-01.

## 2. What was checked against the hard rules and P3-00 H1 to H8

| Rule | Result |
| --- | --- |
| Integer and fixed-point only (H1) | yes: `uint64`, `int64`, `U128`, `Fx` raw comparison. No `float` or `double` in the change |
| No standard transcendentals or distributions | none. `pick`, `chance` and `weighted` are the project's own (P3-02 4.1) |
| One generator, streams by name; keyed draws (H4) | every draw is `hash_words(TAG_HIST, …)` of the stream key and four words. No `Rng`, no `splitmix64_next`. Now enforced by the lint in the history directories (section 5) |
| No unordered iteration, pointer order (H3) | the registry is an ordered set used for membership only (N6). No pointer comparison or pointer key |
| No thread timing, clock, environment (H7) | none. The library is sequential and takes no thread count |
| Overflow (H6) | `pick` < 2^64 (N1), `weighted` < 2^95 in `U128` with a checked `int64` sum (N3, S1), letter counts in `int64`, lengths in `size_t` against 48 and 64 |
| UB and implementation-defined behaviour | none found. Unsigned shifts only, casts after range checks, `char` signedness irrelevant (N5), no signed overflow |
| Pure function (H8, invariant 3) | a name is a function of (seed, kind, culture, entity, naming data, registry content) |

## 3. Lint

- **`p3/02-names` at `9549429`.** `--self-test` passed (13 snippet files, 0 failures), and the scan found 0 findings over 128 C++ files and 13 CMake files.
- **`p3/01-int` at `4e1a3c1`.** The same results: 13 snippet files, 0 findings over 125 C++ files.
- **Scan set.** The default scan set covers the history code. `collect()` lists `worldgen/include/worldgen/history/draw.hpp`, `names.hpp`, `worldgen/src/history/draw.cpp` and `names.cpp` as library files. `worldgen/tests/history/` is excluded with the other test directories, as intended.
- **With the new rule.** On the integration tree (P3-01 + P3-02 + P3-02 tests), with the audit branch's lint, the self-test passes (15 snippet files) and the scan finds 0 findings over 129 C++ files, `history-sequential-rng` included.

## 4. What was run

All runs were on Linux, in scratch worktrees outside the repository, with builds under `~/.cache/the-unmapped/build/<worktree>/`. The integration tree is `origin/p3/02-names` merged with `origin/p3/02-names-tests` and then `origin/p3/01-int`, built with `linux-gcc-release` and `linux-clang-release`. The only conflicts were in `worldgen/tests/CMakeLists.txt` (three adjacent `target_sources` blocks, all kept) and `worldgen/tests/reference/README.md` (both sections kept); the lead meets the same conflicts at merge.

| Run | Result |
| --- | --- |
| `p3_names_reference.py check` | 1132 checks, 0 failures |
| `p3_names_reference.py golden -` against `worldgen/tests/golden/p3-02-names.txt` | byte-identical |
| `p3_names_reference.py samples` | 0 differences from the spec's table 5.1 |
| `worldgen-tests -sf='*history*'`, GCC and Clang release | 93 test cases, 91 pass. The two failures are S1's, the same 5 assertions on both compilers. Everything else passes, including the golden vectors of 9.3 (draws, picks, attempts, names), the worked example of 4.6, and the property sweep of 9.2 test 8 (2,000 names per culture and kind, seeds 0 and 1, no fallback needed) |
| `world-hashes.sh local linux-gcc-release linux-clang-release` | all 12 cases identical |
| `p3/01-int` alone against the integration | identical world files (N7) |

Not run: MSVC (CI covers it, and nothing in the change has a platform branch), `linux-clang-asan`, and the debug presets (the validation specialist's runs cover them). The bounds of N1 and N3 were checked by argument and by the test writer's edge vectors, not by a separate UBSan probe.

## 5. Lint rules

**Added: `history-sequential-rng`.** Files: `tools/lint/determinism_lint.py` (`HISTORY_DIRS` and the rule), `tools/lint/README.md`, and the snippets `tools/lint/selftest/bad_history.cpp` and `good_history_scope.cpp`.

- **What it reports.** In library files whose path starts with one of `HISTORY_DIRS`, it reports:
  - the identifier `Rng` (the type, its constructor and `Rng::from_state`);
  - calls of `splitmix64_next(`;
  - `#include "worldgen/core/rng.hpp"`.
- **Where it applies.** `HISTORY_DIRS` is `worldgen/src/history/`, `worldgen/include/worldgen/history/`, `worldgen/src/stages/chronology…`, `worldgen/src/stages/history…`, and the same two stage prefixes under `worldgen/include/worldgen/stages/`.
- **Why.** Rule H4 says every random decision of the history is a keyed draw. A draw from a sequential stream makes every later decision depend on how many draws came before, so adding one event type would reshuffle the whole history. The P3-00 overview and P3-02 both state this rule, and until now nothing enforced it. The geography stages keep `Rng` (P0-01 6). The rule is therefore scoped by path and does not apply to self-test snippets without a `lint-selftest-path`. `good_history_scope.cpp` checks that a geography stage is not reported.
- **Coverage.** The stage prefixes cover the file names P3-00 5 implies (`chronology`, `history`). A history stage with another file name must be added to `HISTORY_DIRS`, and the README says so. The rule finds nothing in the current trees.

**Considered and not added:**
- A rule against string-keyed `std::map` and `std::set` in history code. H3 asks for integer keys in maps, but string ordering through `char_traits<char>` is portable, and the one string set (the registry) is membership-only. The rule would only produce suppressions.
- A review rule for iteration over ordered containers in history code. A range-`for` over a `std::map` is in key order, which is defined. Whether that order is the spec's order is a question for the audit, not something a syntactic rule can answer.
- A rule against `%` applied to a draw (modulo bias, or ignoring `pick`). A draw cannot be told from any other `uint64` syntactically.

`unordered-container`, `std-hash`, `pointer-order`, `unstable-sort`, `heap-order` and `two-draws` already apply to the history directories, as to the rest of the library.
