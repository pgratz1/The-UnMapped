# Banned-construct lint

`determinism_lint.py` scans the world generator (`worldgen/`) and the command
line (`tools/worldgen-cli/`) for constructs that can make output differ by
platform, compiler, standard library or thread count: the bans of spec P0-01
section 4 (`docs/specs/p0-01-deterministic-core.md`, with 4.2 as amended by
section 15) and P1-00 section 8 (`docs/specs/p1-00-geography-overview.md`,
rules D1 to D10). It also reads the SQL text of the database and inspection
code, the project's CMake files for floating-point flags, and the pinned
toml++ configuration. It catches only what can be found mechanically with few
false positives; it does not replace the determinism audit.

Owner: the determinism auditor (`.claude/agents/determinism-auditor.md`).
Python 3 standard library only.

## Running it

From the repository root (`-B` stops Python writing `__pycache__` into the tree):

```
python3 -B tools/lint/determinism_lint.py              # the default scope
python3 -B tools/lint/determinism_lint.py --self-test  # check the lint itself
python3 -B tools/lint/determinism_lint.py --list-rules # rules, where each applies
python3 -B tools/lint/determinism_lint.py PATH ...     # scan given files or directories
```

**Default scope** (no paths):

- every C++ file (`.h .hh .hpp .hxx .c .cc .cpp .cxx .ipp .inl .tpp`) under
  `worldgen/` without `worldgen/tests/`, and under `tools/worldgen-cli/`
  without `tools/worldgen-cli/tests/` (`report` and `hash` text is part of the
  cross-platform contract);
- the project's CMake files: `CMakeLists.txt`, `CMakePresets.json`,
  `extern/CMakeLists.txt`, and every `CMakeLists.txt` and `*.cmake` under
  `cmake/`, `worldgen/`, `tools/` and `gdext/` (not the vendored trees under
  `extern/*/`, not `tools/lint/selftest/`);
- the toml++ configuration check (`toml-config`).

`extern/`, `gdext/` sources, `game/` and the test directories are never C++
scope. Paths given on the command line are scanned as given, with no
exclusions and without the toml++ check; a `CMakeLists.txt`, `*.cmake` or
`CMakePresets.json` given explicitly is read as CMake. On Windows use
`py -3 -B` or `python -B`.

Output is one line per finding, `path:line:col: rule: message`, sorted by
path and line, and a count on standard error. Exit status: 0 clean, 1 findings
(or a self-test failure), 2 usage error. The full default scan takes well
under a second.

## Where rules apply

A file under `worldgen/` is **library** code and a file under
`tools/worldgen-cli/` is **CLI** code. Any other path (an explicit path
elsewhere, a self-test snippet) gets the rules of both.

## Rules

Comments, string literals (including raw strings and encoding prefixes) and
character literals are ignored by the code rules; digit separators (`1'000`)
are handled. The header name of an `#include` is checked. Rules marked
**review** flag every use: the reviewer checks it and suppresses it with the
reason it is safe.

### P0-01 section 4

| Rule | Reports | Applies to |
| --- | --- | --- |
| `float-type` | `float`, `double`, `long double`, `__float128`, `_FloatN`, `std::floatN_t`, `std::bfloat16_t` | library, CLI |
| `banned-include` | `#include` of `<cmath>`, `<math.h>`, `<random>`, `<chrono>`, `<ctime>`, `<time.h>`, `<unordered_map>`, `<unordered_set>`, `<locale>`, `<clocale>` | library, CLI |
| `std-math` | `std::sin`, `std::pow`, `std::sqrt`, `std::floor`, ... (every `<cmath>` function, as `std::name(` or `std::name<`) | library, CLI |
| `std-random` | `std::shuffle`, `std::sample`, `std::random_shuffle` (and `std::ranges::` forms), every `<random>` engine and distribution | library, CLI |
| `c-rand` | calls of `rand(`, `srand(`, `std::rand(`, `::rand(` | library, CLI |
| `wall-clock` | calls of `time(`, `clock(`, `timespec_get(`, `gettimeofday(`, `clock_gettime(`; any `std::chrono` | library, CLI |
| `std-hash` | `std::hash` | library, CLI |
| `unordered-container` | `std::unordered_map/set/multimap/multiset` (also `std::pmr::`) | library, CLI |
| `locale` | calls of `setlocale(`; `std::locale`; `.imbue(` / `->imbue(` | library, CLI |
| `long-type` | `long`, `unsigned long`, `signed long`, `long int` (not `long long`; `long double` is `float-type`) | library, CLI |
| `wchar-type` | `wchar_t` | library, CLI |
| `wide-int-type` | `__int128`, `__uint128_t`, `_umul128`, `_mul128`, `_udiv128`, `_div128`, `__umulh`, `__mulh` | library and CLI except `worldgen/src/core/wide.cpp` |
| `unstable-sort` | **review**: `std::sort`, `std::partial_sort`, `std::partial_sort_copy`, `std::nth_element`, `std::partition` (and `std::ranges::` forms). Suppress with the reason the order is fully determined | library, CLI |

### Threads and shared state (P0-01 4.2, P1-00 D6, D8, D9, D10)

| Rule | Reports | Applies to |
| --- | --- | --- |
| `thread` | `std::thread`, `std::jthread`, `std::async`, `std::launch`, `std::packaged_task`, `std::promise`, `std::future`, `std::shared_future`, `std::stop_token`/`stop_source`, `std::execution::`, `#pragma omp`, `pthread_create(`, `CreateThread(`, `_beginthread(ex)(`; `#include` of `<thread>`, `<future>`, `<execution>`, `<stop_token>`, `<omp.h>`, `<pthread.h>` | library and CLI except `worldgen/src/core/parallel.cpp` (its header needs none of it) |
| `shared-state` | `std::atomic*` (`atomic_ref`, `atomic_flag`, ...), `std::memory_order*`, every mutex, lock, condition variable, semaphore, latch and barrier, `std::call_once`/`once_flag`, `_Atomic`, `__atomic_*(`/`__sync_*(` builtins, `_Interlocked*(`; `#include` of `<atomic>`, `<mutex>`, `<shared_mutex>`, `<condition_variable>`, `<semaphore>`, `<latch>`, `<barrier>`, `<stdatomic.h>` | library and CLI except `parallel.cpp` |
| `thread-identity` | `std::this_thread::` (also `sleep_for`, `yield`), `std::thread::id`, `.get_id(`, `thread_local`, `_Thread_local`, `__thread`, `__declspec(thread)`, `pthread_self(`, `GetCurrentThreadId(`, `gettid(` | library and CLI, `parallel.cpp` included |
| `hardware-concurrency` | **review**: `hardware_concurrency`, `_SC_NPROCESSORS_*`, `get_nprocs(`, `GetSystemInfo(`, `GetActiveProcessorCount(`, ... | library (never: the caller supplies the count, D10) and CLI (only the `--threads` default, with a suppression) |
| `unordered-reduction` | `std::reduce`, `std::transform_reduce`, `std::inclusive_scan`, `std::exclusive_scan`, `std::transform_inclusive_scan`, `std::transform_exclusive_scan` | library, CLI |
| `heap-order` | **review**: `std::priority_queue`, `std::make_heap`, `std::push_heap`, `std::pop_heap`, `std::sort_heap` (and `std::ranges::` forms). The suppression must show the comparator is a strict total order ending in the push sequence number (D6) | library, CLI |
| `two-draws` | two random draws on the same generator in one full expression: `.next(`, `.next_u32(`, `.uniform_*(`, `.chance(`, `.chance_ratio(`, `.weighted_index(`, `.shuffle(` on the same receiver text, or `splitmix64_next(` on the same argument (P0-01 4, D9) | library, CLI |

### Platform input and output

| Rule | Reports | Applies to |
| --- | --- | --- |
| `environment` | calls of `getenv(`, `std::getenv(`, `secure_getenv(`, `_wgetenv(`, `getenv_s(`, `_dupenv_s(`, ...; `GetEnvironmentVariable*`, `GetEnvironmentStrings*`; `environ`, `_environ`, `_wenviron`. The library takes the environment as an argument (P0-03 7.6, P1-00 6.3) | library only |
| `directory-iteration` | **review**: `directory_iterator`, `recursive_directory_iterator`, `opendir(`, `readdir(`, `scandir(`, `_findfirst*(`, `FindFirstFile*(`, `FindNextFile*(`. The suppression must show the entries are sorted (or their order discarded) before use | library, CLI |
| `number-text` | text to number or number to text through the C library or locale: `strto*`, `wcsto*`, `ato*`, `std::sto*`, `std::to_string`, `std::to_wstring`, the `printf` and `scanf` families (with `_s` and `_snprintf` forms), `ecvt`/`fcvt`/`gcvt`, `strfrom*`. Use `std::to_chars`/`std::from_chars` on integers or the project's formatting | library, CLI |
| `stream-format` | `std::cout`/`cerr`/`clog`/`cin` (and `w` forms), string streams (`[w][i\|o]stringstream`, `strstream`, `spanstream`, `osyncstream`), iomanip manipulators (`setprecision`, `setw`, `fixed`, `put_time`, ...); `#include` of `<iostream>`, `<sstream>`, `<iomanip>`, `<strstream>`, `<spanstream>`, `<syncstream>` | library, CLI |
| `unchecked-write` | a call of `fwrite`, `fputs`, `fputc`, `putc`, `puts`, `putchar`, `fflush`, `fclose`, `fprintf`, `printf`, ... used as a statement, so its result is thrown away (including `(void)` and `static_cast<void>(...)`). A failed write must change the exit status (P0-04 4.6) | library, CLI |
| `sqlite-value-type` | `sqlite3_bind/column/result/value_double`, `sqlite3_bind/column/result/value_int` (not `_int64`): a float or a 32-bit integer stored or read (P0-02 5.3) | library, CLI |

### SQL text

String literals in `worldgen/src/db/`, `worldgen/src/inspect/`,
`worldgen/include/worldgen/db/` and `worldgen/include/worldgen/inspect/` are
read as SQL. Adjacent literals (`"SELECT a " "FROM t"`) are joined, raw strings
are read, and SQL comments and quoted SQL strings and identifiers (`'real'`,
`"real"`) are skipped. A literal is taken to be SQL when it starts with an
upper-case statement keyword (`SELECT`, `CREATE`, `PRAGMA`, ...) or contains
an upper-case clause keyword (`FROM`, `WHERE`, `ORDER BY`, `VALUES`, ...); this
is the project's convention, and it keeps prose in messages ("select a
layer") out. Inside SQL the checks ignore case. A finding is reported on the
line of the first literal of the group.

| Rule | Reports |
| --- | --- |
| `sql-autoincrement` | `AUTOINCREMENT` |
| `sql-maintenance` | `ANALYZE`, `PRAGMA optimize`, `PRAGMA incremental_vacuum`, `VACUUM` not followed by `INTO` (or `VACUUM schema INTO`) |
| `sql-setting` | `journal_mode = WAL`; `auto_vacuum =` anything but `NONE` or `0` |
| `sql-real` | `REAL`, `FLOAT`, `DOUBLE`, `NUMERIC`, `DECIMAL` as words in SQL (a column type or a `CAST`) |
| `sql-unordered-select` | **review**: a `SELECT` with no `ORDER BY` in the same joined text. Suppress with the reason order cannot matter: at most one row (unique key, aggregate without `GROUP BY`), or `ORDER BY` appended to a built string later |

### Build configuration

| Rule | Reports | Applies to |
| --- | --- | --- |
| `fp-pragma` | `#pragma float_control`, `fp_contract`, `fenv_access`, `STDC FP_CONTRACT`/`FENV_ACCESS`/`FENV_ROUND`/`CX_LIMITED_RANGE`, `clang fp`, `GCC optimize`, `optimize`; `_Pragma(`, `__pragma(` (their text cannot be read); `__attribute__((optimize...` and `[[gnu::optimize...` | library, CLI |
| `cmake-fp-flag` | `-ffast-math`, `-Ofast`, `-funsafe-math-optimizations`, `-ffinite-math-only`, `-fassociative-math`, `-freciprocal-math`, `-fno-signed-zeros`, `-fno-trapping-math`, `-fcx-limited-range`, `-ffp-contract=fast` or `=on`, `-ffp-model=fast`, `/fp:fast`, `/fp:contract` (also `-fp:` and inside generator expressions). `#` comments are ignored; `-fno-fast-math`, `-ffp-contract=off` and `/fp:precise` are fine | the CMake files of the default scope |
| `toml-config` | every macro that the `#if !defined(TOML_X) \|\| TOML_X != v ... #error` guard of `worldgen/src/data/toml_document.cpp` requires must appear, with the same value, in `target_compile_definitions(unmapped_tomlplusplus INTERFACE ...)` of `extern/CMakeLists.txt`. A missing macro is reported on the `target_compile_definitions` line, a wrong value on its own line; a missing guard is itself a finding | default scan only |

### Suppression hygiene

| Rule | Reports |
| --- | --- |
| `bad-suppression` | a suppression comment that is malformed, names an unknown rule or gives no reason |
| `unused-suppression` | a suppression that silences nothing (stale, or names the wrong rule) |

### Heuristics and known limits

- For `c-rand`, `wall-clock`, `setlocale` and every C function named in
  `environment`, `directory-iteration`, `number-text`, `hardware-concurrency`,
  `thread`, `thread-identity` and `unchecked-write`: member calls
  (`x.time()`, `p->printf()`), other namespaces' functions (`other::getenv()`)
  and declarations (`int time();`) are not reported. The declaration test is
  a heuristic: a name preceded by an identifier other than
  `return`/`throw`/`case`/..., or by `*`, `&` or `>`, is taken to be a
  declaration, so `a * time(nullptr)` is missed.
- `two-draws` treats as sequence points: `;`, braces, the parentheses of
  `if`/`while`/`for`/`switch`, the `?` and `:` of a conditional (the condition
  is evaluated first, then one branch), and a comma that is not inside
  parentheses or brackets (declarators, braced-init-list elements and comma
  operands are evaluated left to right). It pairs only draws on the same
  receiver text, so `a.next() == b.next()` on two generators is allowed and an
  alias of one generator under two names is missed. Draws on either side of a
  brace (`Pair{r.next()} + r.next()`) or in different operands of `?:`
  (`f(c ? r.next() : 0, r.next())`) are not paired.
- `unchecked-write` sees only a call used as a whole statement; a result
  stored in a variable that is never read is not detected.
- `stream-format` bans formatted string and console streams outright; number
  insertion into an `std::ofstream` (`file << n`) cannot be told from a shift
  and is not reported. Binary `std::ifstream`/`std::ofstream` `read`/`write`
  are fine.
- `std::format` and `std::to_chars` are not reported (integer formatting
  through them is locale-independent; floating-point values are already
  `float-type`).

**Allow-lists.** `WIDE_TYPE_ALLOWLIST` (spec P0-01 5.3: the fast path of
`mul_u64_wide` and `divmod_u128_u64`) holds `worldgen/src/core/wide.cpp`, the
only file exempt from `wide-int-type`. `PARALLEL_ALLOWLIST` (P0-01 4.2, P1-00
D10: the one parallel facility) holds `worldgen/src/core/parallel.cpp`, the
only file exempt from `thread` and `shared-state`. Every other rule still
applies in both. Adding a file to either needs a spec change.

## Suppressing a finding

A finding the reviewer has checked is silenced with a comment that names the
rule and gives the reason:

```cpp
// determinism-lint: allow(unstable-sort) -- comparator orders by (region_id, cell_index), unique per element
std::sort(cells.begin(), cells.end(), by_region_then_index);

std::sort(ids.begin(), ids.end());  // determinism-lint: allow(unstable-sort) -- uint64 ids: equal elements are indistinguishable

// determinism-lint: allow(heap-order) -- comparator orders by (key, seq), seq the unique push counter (P1-00 D6)
std::priority_queue<Entry, std::vector<Entry>, ByKeyThenSeq> queue;
```

- Syntax: `determinism-lint: allow(rule[, rule...]) -- reason`, in a `//` or
  `/* */` comment.
- Placement: at the end of the offending line, or on its own line directly
  above it (several suppression lines may be stacked). For a construct split
  over several lines, the finding is on the line where it starts; for SQL,
  on the line of the first string literal of the statement.
- The reason is mandatory (at least 10 characters) and should say why the
  result cannot depend on platform or library, not merely that it is fine.
- A suppression that is malformed, names an unknown rule, has no reason, or
  suppresses nothing is itself a finding, so suppressions cannot go stale
  silently.
- `cmake-fp-flag` and `toml-config` cannot be suppressed: fix the build files.
- Suppressions are reviewed in the determinism audit. Prefer changing the code
  (`std::stable_sort`, fixed-width types, `ORDER BY`) over suppressing.

## Self-test

`selftest/` holds snippet files (not compiled, not part of any build). A line
that must be reported ends with a marker comment `// expect: rule[, rule]`
(`# expect: rule` in `.cmake` snippets); every other line must produce
nothing. `--self-test` checks that the lint reports exactly the marked
`(line, rule)` pairs in every file, and that every rule is exercised by at
least one snippet. A C++ snippet whose first lines contain
`lint-selftest-path: <path>` is scanned as if it were that repository path,
which selects its area (library, CLI), the allow-lists and the SQL
directories; other snippets get every rule.

| File | Checks |
| --- | --- |
| `bad_constructs.cpp` | every P0-01 rule fires on its constructs |
| `bad_suppressions.cpp` | valid suppressions silence; malformed, reasonless, unknown-rule and unused ones are reported |
| `bad_concurrency.cpp` | `thread`, `shared-state`, `thread-identity`, `hardware-concurrency`, `unordered-reduction`, `heap-order` (and a suppressed heap) |
| `bad_platform_io.cpp` | `fp-pragma`, `environment`, `directory-iteration`, `number-text`, `stream-format`, `sqlite-value-type`, `unchecked-write`, `two-draws` |
| `bad_sql.cpp` | the SQL rules, as a file of `worldgen/src/db/`; joined and raw literals; near misses (`VACUUM INTO`, `journal_mode = MEMORY`, `'real'` in an SQL string, `REAL` in a message, lower-case prose) |
| `bad_flags.cmake` | `cmake-fp-flag`, with comments and allowed flags as near misses |
| `toml_config/` | `toml-config`: a guard and a CMake fixture with one macro missing and one wrong (a source without a guard must also fail) |
| `good_ignored.cpp` | comments, string/char/raw literals, digit separators, `long long`, identifiers containing banned words, member and other-namespace functions, declarations, the project's own functions |
| `good_new_ignored.cpp` | the same for the P1 rules: banned names in comments and literals, identifiers that contain them, members, used write results, one draw per statement, sequenced draws and draws on two generators |
| `good_wide_allowlisted.cpp` | `__int128` is allowed in `wide.cpp`; other rules still apply |
| `good_parallel_allowlisted.cpp` | threads and atomics are allowed in `parallel.cpp`; thread identity, `hardware_concurrency` and other rules still apply |
| `good_cli_scope.cpp` | as CLI code: `getenv` and SQL text are not reported; threads, `number-text` and `unchecked-write` are; suppressed `<thread>` and `hardware_concurrency` |
| `good_lib_scope.cpp` | as library code outside the SQL directories: SQL text is not read; `environment`, `thread` and `shared-state` are reported |

When a rule is added or changed, add bad and good lines for it here.

## Not in scope: the Godot extension

`gdext/` is presentation code that never writes a world, so the lint does not
scan it (its CMake files are scanned for floating-point flags, because the
project-wide flags reach it). It legitimately uses `float`, Godot's math and
the engine's threads. It reads worlds only through the library's
`worldgen::db::WorldReader`, whose SQL this lint already reads. What would be
worth checking there, by review or a small separate rule set, is that it stays
that way: no `#include <sqlite3.h>` or `sqlite3_*` call of its own, no use of
`WorldWriter` or `generate_world`, so it can never write or reorder a world.
