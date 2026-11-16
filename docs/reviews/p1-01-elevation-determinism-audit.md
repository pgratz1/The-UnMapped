# Determinism audit: P1-01 elevation

Auditor: determinism auditor, 2026-11-16. Change audited: branch `p1/01-elevation`, commit `93a22f6` ("Add stage P1-01 elevation (design v16) and the worldgen tuning kind"), diff `main...p1/01-elevation` (merge base `01b07f5`). Specs: `docs/specs/p1-01-elevation.md` (third version), `docs/specs/p1-00-geography-overview.md` (rules D1 to D10, G1 to G6, appendix C), P0-01. Also audited on `main` (`e0c558b`), at the lead's request: the seed-phrase scrub and the five slope statistics of `report`.

Line numbers are those of `93a22f6` for the stage and of `main` for the core and inspect code.

**Verdict: pass.** No blocker, no should-fix. Eight notes. Two lint rules added on this branch (section 5).

Classification: **blocker** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** is fragile and could become a blocker with a plausible change or input; **note** is recorded for the record, no action required. (The role file's "blocking" is blocker; its "advisory" covers should-fix and note.)

## 1. Findings

### Blockers

None.

### Should-fix

None.

### Notes

**N1. `select_lakes` relies on unique labels without checking them.** `worldgen/src/stages/elevation.cpp:1083-1086`. The comparator (volume descending, label ascending) is a strict total order only if no two candidates share a label. In the stage they cannot: labels come from G2 (`label_components`), 1 to count, each once (line 1124). But `select_lakes` is a public seam (`elevation.hpp`), and a test that hands it two components with the same label and volume and different sizes would get a kept list whose order, and so whose cap outcome, depends on the standard library's `std::sort`. Scenario: a hand-made fixture with duplicate labels gives different `kept` on MSVC STL and libstdc++. Suggestion (optional): fatal on a duplicate label in `select_lakes`, or use `std::stable_sort`. The `unstable-sort` suppression is justified for the stage as written.

**N2. The `heap-order` suppression is justified.** `elevation.cpp:1146-1148`. Entries are `std::tuple<int64 P, uint64 seq, CellIndex>` compared with `std::greater<>`; seq is the global push counter (lines 1152, 1156, 1185), unique per entry, so the tuple order is a strict total order and `top()` is the smallest (P, seq) on every standard library, as E10.4 and D6 require. The trailing cell index never decides. The flood marks a cell visited when it is pushed (line 1171), so each cell is pushed once.

**N3. Arithmetic shift of a possibly negative value.** `elevation.cpp:592`: `(lo.raw() + hi.raw()) >> 1U` with `lo` starting at `segment_bias` ≥ −1. Right shift of a negative signed value is arithmetic by definition in C++20, which the project requires, and is arithmetic on all three compilers in any case. It is the floor the spec asks for. The sum is at most 4 × 2^32 in magnitude. No action.

**N4. `belt_gradient` guards a zero divisor that the spec leaves undefined.** `elevation.cpp:308-309` divides by `max(x1 − x0, 1)`. Spec E3.2 divides by `x1 − x0`, which is 0 only on a grid one cell wide or high. That never happens for N ≥ 16 and happens only on a test grid, where both difference and divisor are 0 and the code gives 0. No output differs. The spec could say so.

**N5. The E5.1 pre-filter is equivalent to the full count.** `elevation.cpp:494-540`. `FloorCandidates::build` keeps the cells with `mul(b, gm) ≥ lev`, and `count` evaluates `mul(mul(b, sg), gm)` with `sg = fade(clamp((HALF + β) + mul(contrast, ns), 0, ONE))`, which is the same expression as `mountain_mask` (line 489) since `mul(contrast, ns)` is computed once per cell either way. RHA `mul` is non-decreasing in each non-negative argument and `mul(b, ONE) = b`, so `sg ≤ ONE` gives `mul(b, sg) ≤ b` and then `mask ≤ mul(b, gm)`: every cell that can reach the core is in the list. The offset order (0, +d, −d, …), the strict-improvement rule, the early stop and the 32 fixed bisection steps follow the spec. Checked on output: 512 seed 13 gives o* raw −1530846568 and seed 9 raw 688414388, the values of spec 10.4.

**N6. Comment bound differs from the spec's.** `elevation.cpp:868` says A ≤ 2^24, so the root is < 2^29. Spec 7 uses A ≤ 2^22 (N ≤ 2048). Both fit: `root << 16` < 2^45 and S < 2^13 in `Fx`. Harmless.

**N7. No elevation tests in this commit.** D10 requires a 1-, 2- and 5-thread comparison for every stage with a parallel step, and P1-01 10.3 lists the thread, repeat and map-size independence tests. They are being written on the test-writer's branch (`p1/01-elevation-tests`, being integrated in `p1/01-elevation-int`). The merge gate should include them. My own thread comparisons are in section 4.

**N8. Windows/MSVC not run in this audit.** No MSVC toolchain here. Nothing in the stage depends on `long`, `wchar_t`, `size_t` in a stored value, char signedness or library-specific algorithms (section 2). CI's `compare-hashes` job over all seven presets is the check that closes this.

## 2. What was checked in the stage

`worldgen/src/stages/elevation.cpp` (all 1245 lines), `worldgen/include/worldgen/stages/elevation.hpp`, the `worldgen` kind in `worldgen/src/data/kinds.cpp` and `schema.hpp`, the stream registration in `worldgen/include/worldgen/core/streams.hpp`, the pipeline registration in `worldgen/src/stages/stage.cpp`, `version.hpp`, the tuning files, `tools/ci/world-hashes.sh`. Also the shared code that the stage is the first to call repeatedly: `geo::routing_flood` with a reused `RoutingFloodScratch`, `geo::receivers`, `geo::accumulate`, `geo::ridged_m`, `geo::kth_in_place` and `geo::quantile`, `run_blocks` / `for_each_row_block`.

| Hard rule or D-rule | Result |
| --- | --- |
| Integer and fixed-point only | Yes. No floating type, no `<cmath>`, no `<random>`. Only P0-01 `Fx` operations (checked `+`/`−`, RHA `mul`/`div`/`mul_div`/`from_ratio`, `fade`, `hypot`, `clamp`, `abs`, `round_int`), `isqrt_u64`, `checked_add`, `checked_mul`, plain `int64`/`uint64` integer expressions. Every `abs` in the stage takes an `Fx` and resolves by argument-dependent lookup to `worldgen::abs` (lines 473, 480; `geo/noise.cpp:39`); there is no `std::abs` or C `abs` anywhere in `worldgen/` or the CLI. |
| Random draws | No `Rng`. Fifteen keys from `stream_key(seed, "geo.elevation.<name>")`, all registered (`streams.hpp`), used only as noise keys or `hash_at` keys: the centre at `hash_at(K_continent, 0 or 1, 0)` (line 287) and the receivers at `hash_at(K_erosion, c, it)` (line 838), location-keyed, so order-independent (D9). |
| No unordered containers, pointer values, time, thread identity, environment | None. The tuning comes through the content set, never from the environment. |
| Parallel steps (D1, D2, D8, D10) | E2 (lines 336-369), E3.2 (300-313), E5.2 (715-747), E5.3 depth (756-763), E7 (784-792), E8.2 (818-848), E8.3 factors (858-876), E8.5 (913-932), E8.6 (941-967), E9 (1020-1025), E10.0 (1055-1068), G4. Each block writes only its own rows' cells and reads only arrays fixed before the call. E8.5 reads `hn` and writes `h`; E8.6 reads `h` and writes `hn`; the copy back is after the join (line 999). Byte grids are `std::vector<std::uint8_t>`, never `vector<bool>`. No reduction is done in parallel. The thread count is checked (1 to 256) and used only by `run_blocks`. Blocks are G6 row blocks, a function of the height only. |
| Sequential steps (D4, D5, D6) | E4 (G2, index-order seeds, FIFO BFS); E5.1 counts in land-index order; E5.3 chamfer in index order then reverse; E8.1 G3 (radix heap, (key, seq) order: buckets keep push order, `routing_flood` resets the scratch and reassigns `level`, `parent`, `order` on every call, so no stale entries); E8.3 G5 and E8.4 in G3's `order` (random receivers have strictly lower L and so are popped earlier: `order` stays topological, which `accumulate` also checks); E10.1 G3; E10.2 G2 with the L key; E10.4 (N2). |
| Sorting and selection (D7) | Values only from `nth_element` (`kth_in_place`, `quantile` on a copy): c_sea, g_med, s_land, E6's q and qc. Every in-place selected array (`g`, `above`) is discarded afterwards. One sort, on a strict total order (N1). |
| No uninitialised or stale reads | `Fx` and every result struct default to 0 (`fx.hpp:73`, `elevation.hpp:379-384`). `hn` is written for every cell by the solve (`order` is a permutation) before diffusion reads it. `rcv` is rewritten by G4 for every cell on each routing iteration; ocean `rcv` is `NO_DIRECTION` and never followed. `f` is written for every cell. `kf` is read only for non-kept land cells, all of which are visited. |
| Width and overload portability | No `long`, `size_t` or `wchar_t` in a value that reaches the output; `size_t` only for container sizes and loop counters. `std::min`/`std::max` are called with matching types or explicit `<std::int64_t>`. `hash_at` takes `int64` for both coordinates (`c` cast at line 838, `it` promoted from `int32`). `2U * q * q` is `uint64`. Integer division and `%` appear only on non-negative operands (`(a + 255) / 256`, `% total`, `it % route_interval`). |
| Overflow (spec section 7 bounds against the code's types) | Checked step by step: E1 `(hv − 2^31) × 2` in `int64`; E2 ratios ≤ 237.2 in `Fx`, the divisor ≥ 0.01 by named check 1; E3 `c − c_sea` < 2^37 as `int64`; E5.1 \|o\| ≤ 4 w_b ≤ 14144, `lo + hi` ≤ 2^34; E5.2 and E7 below 12800 m before the clamp; E5.3 `16 × min(dl, 2^20)` in `Fx`; E8.2 δ < 2^44 so q < 2^28, w < 2^57, W < 2^60 in `uint64`; E8.3 `A << 32` ≤ 2^54; E8.5 S4 ≤ 11960 m; E9 `h × 10` in `mul_div`'s 128-bit product; E10 `k × 2^16` via `checked_mul`, `lv + eps` via `checked_add`, `lake_max_cells` in `int64` (< 2^52), volume < 2^37. Every bound holds for N = 2048 at any tuning inside the schema bounds, which match spec 4.2 field for field. The `-fsanitize=implicit-conversion` run reported nothing (section 4). |
| Database | One `i16` layer, codec 1, written through the existing `write_layer`; no new SQL, no new setting. |
| Version and goldens | `GENERATOR_VERSION` 0.3.0 (P1-00 6.4 rule 4); `p0-worlds.txt` re-recorded because registering the required `worldgen` kind changes `content_hash` of every world. `world-hashes.sh` passes `--data` explicitly, so `UNMAPPED_DATA_DIR` cannot change a golden world. The shipped and `geo` fixture tuning are byte-identical and equal spec 4.4; `geo-small` differs in exactly the five lines spec 4.4 names. |
| Tuning load | The schema bounds and the three named checks match spec 4.2 and 4.3; integer fields are narrowed to `int32` only after a range check (`to_i32`, line 59). |

The result also matches the approved algorithm: the 1024 seed 0 layer hash is `070dbf12…9450786`, the first reference value of spec 10.4.

## 3. Earlier merges audited on `main`

### 3.1 Seed-phrase scrub (`worldgen/src/core/hash.cpp:26-38, 145-177`)

Pass, no finding. `scrub_byte` classifies the unsigned byte value with explicit ranges (0x41-0x5A to lower case, 0x61-0x7A and 0x30-0x39 kept, everything else to a space), so there is no `<cctype>`, locale or `char` signedness: `static_cast<std::uint8_t>(c)` is modular on all three compilers. Runs of spaces collapse; leading and trailing runs are never written, so the function is idempotent. Length and emptiness are checked after scrubbing, on `size_t` against 256. The hash is `hash_words` over `encode_bytes`, the length followed by little-endian packed words, built with shifts, independent of host byte order. The input bytes are the same on Windows and Linux: the CLI takes UTF-16 arguments through `wmain` and converts them to UTF-8 (`tools/worldgen-cli/main.cpp:14`), so no ANSI code-page or best-fit conversion reaches the scrub, and the settings loader reads the TOML file's UTF-8 bytes. A NUL byte is scrubbed like any other byte, since the input is a `string_view` with a size. `writer.cpp:465` and `reader.cpp:111` use the same function to validate stored phrases.

### 3.2 Slope statistics of `report` (`worldgen/src/inspect/geography.cpp:376-672`)

Pass, no finding. All five are integer computations on the stored `i16` layer, printed with `db::format_u64` (no `std::to_string`, no locale).

- `step_p999_m` (lines 499-519): m(c) ≤ 32766 for two positive `i16` values, counted in a 32767-bin histogram; the rank is `min(n − 1, floor(999 n / 1000))` on U128; the k-th value is read from the bins in ascending order, a value-only result.
- `slope_over`, `lowland_flat_share` (lines 547-579): S2 = gx² + gy² with \|gx\|, \|gy\| ≤ 65534, so S2 < 2^33 and 10^8 × S2 < 2^60 in `int64`. The comparisons are strict, as P1-00 10.3 defines them (`>` for steeper, `<` for flatter), against (320 t)² ≤ 1.024 × 10^15. Static asserts tie the constants to the S2 thresholds quoted in the spec (50205, 102400, 7). Edge handling uses `size_t` and `2U / (xb − xa)` with xb > xa, so no negative division.
- `slope_p50_above_500m` (lines 540-545, 576): `kth_in_place` over a scratch list that is then discarded (G1 value); `tan4` is `isqrt_u64(10^8 × S2)` on a non-negative value < 2^61, rounded with `(r + 160) / 320`.
- `edge_parallel_run_m` (lines 464-484, 612-672): edge distances from index loops, the longest window with range ≤ 8 by two monotone deques, integer only; the result does not depend on how ties are popped, because only the window length is kept.
- `ratio4` (lines 93-108): RHA computed as floor(floor((2 × 10^4 a + b) / b) / 2) in U128, which equals floor((2 × 10^4 a + b) / 2b).

## 4. What was run

All on the audit worktree (`p1/01-elevation-audit`, stage code identical to `93a22f6`), build trees under `~/.cache/the-unmapped/build/elev-audit/`, worlds under `~/.cache/the-unmapped/out/elev-audit/`, shipped tuning (`--data game/data`).

| Run | Result |
| --- | --- |
| `determinism_lint.py --self-test`, then the default scan | pass, 0 findings (before and after the new rules) |
| `worldgen-cli generate` + `hash`, `linux-gcc-release` with threads 1, 2, 3, 5, 8, and `linux-clang-release` with threads 1, 3, 5, 8: 512 seeds 0, 7, 13, 42 and 1024 seed 1 | one file hash per case over all nine runs: 512 s0 `275c2708…f3443e2`, s7 `c9cfbbf9…af888112`, s13 `a918763a…db403e35`, s42 `ef21d468…275995d0`; 1024 s1 `b6bd4718…49293d9e` |
| The same, 512 seed 9, GCC and Clang release, 5 threads | `05e1239d…47908cdde` on both |
| Library driver (GCC release, `libworldgen.a`): the stage step by step, then `elevation_grid` at 1 and 7 threads | Mountain floor triggers at 512 seeds 9 (o* +2d, raw 688414388) and 13 (o* −4d, raw −1530846568), the spec 10.4 values, and at 1024 seed 1 (raw 285837375), each with β* = `segment_bias`. Lakes kept: 512 s0 2 (935 cells), s7 1, s9 3, s13 2, s42 1 (2336 cells); 1024 s1 6 (12138 cells, cap 12603). The step-by-step grid, 1 thread and 7 threads agree for every case, and also at N = 80 (two row blocks, the last 16 rows). |
| 1024 seed 0, `hash --detail` | layer `070dbf1206416efcbed64aa009b89e5e3f23fec990a4397d99675cfba9450786` = spec 10.4 reference |
| ThreadSanitizer: `linux-clang-release` configured out of tree with `-fsanitize=thread`; 512 seed 13 with 5 threads, 512 seed 42 with 8 threads | no report; hashes equal to the release builds |
| `linux-clang-asan` (ASan + UBSan, `-fno-sanitize-recover=all`): 512 seeds 13 and 42, 5 threads | clean (35 s and 56 s); hashes equal to the release builds |
| Clang `-fsanitize=implicit-conversion` (out-of-tree build, logging, recoverable): 512 seed 13, 5 threads | no report: no implicit integer truncation or sign change in the whole run |

Not run: ctest (the validation specialist's step), MSVC (N8).

## 5. Lint

The two existing suppressions in the stage are justified (N1, N2). Nothing in the change needed a new rule, but two constructs from the audit brief had no automatic check. I added two rules on this branch, with self-tests (`tools/lint/determinism_lint.py`, `tools/lint/selftest/bad_constructs.cpp`, `good_new_ignored.cpp`, README tables):

- **`int-abs`**: `std::abs(`, `std::labs(`, `std::llabs(`, `std::imaxabs(`, their `::`-qualified forms, and free calls of `labs(`, `llabs(`, `imaxabs(`. Integer abs of the most negative value is undefined, and which integer overloads the global namespace holds differs between standard libraries, so `::abs(int64)` can bind to `int abs(int)` and truncate on one platform only. Unqualified `abs(` is not reported, because the project's `Fx` abs is found that way. Other namespaces' functions and members are not reported either. No current code matches.
- **`packed-bool`**: `std::vector<bool>` (also `std::pmr::`), `std::bitset`, and `#include <bitset>`. Writing different elements from different blocks is a data race on the shared word. `map_blocks` already refuses `bool` results with a `static_assert`, but nothing stopped a `for_each_row_block` body from writing a captured `vector<bool>`. Outside the tests, no current code matches.

Both pass the self-test, and the default scan still reports 0 findings.
