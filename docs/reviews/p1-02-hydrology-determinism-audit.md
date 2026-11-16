# Determinism audit: P1-02 hydrology

Auditor: determinism auditor, 2026-11-16. Change audited: branch `p1/02-hydrology`, commits `67ab84e` ("Hydrology stage (P1-02): H1 to H9, registered after elevation") and `6973972` ("GENERATOR_VERSION 0.4.0 for hydrology; P0 goldens re-recorded; tests for the two-stage pipeline"), diff `main...p1/02-hydrology` (merge base `3630582`). Specs: `docs/specs/p1-02-hydrology.md` (approved 2026-11-15), `docs/specs/p1-00-geography-overview.md` (D1 to D10, G2 to G6). Scope: `worldgen/src/stages/hydrology.cpp`, `worldgen/include/worldgen/stages/hydrology.hpp`, the pipeline registration, and the shared code the stage calls: G2 (`label_components`, `bfs_region`), G3 (`routing_flood` and its radix heap), G4 (`receivers`), G5 (`accumulate_topological`), G6 (`for_each_row_block`) and `chamfer_distance` (in `elevation.cpp`).

Line numbers are those of `6973972`.

**Verdict: pass.** No blocker. One should-fix, which is a merge-gate item and not a determinism defect (S1). Nine notes. No lint rule added (section 5).

Classification as in the P1-01 audit: **blocker** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** is fragile, or breaks a merge-gate check, and needs action before merge; **note** is for the record.

## 1. Findings

### Blockers

None.

### Should-fix

**S1. The geography golden file is stale: CI `expect` fails on every preset.** `worldgen/tests/golden/p1-worlds.txt` (unchanged by this branch, last recorded in `91d3c2d` for P1-01). The default pipeline now runs hydrology, so the four geography cases (`geo-seed-0-512`, `geo-seed-42-512`, `geo-phrase-512`, `geo-seed-1-1024`) gain seven layers and a new `meta.stages`, and every one of their file hashes changes. Commit `6973972` bumps `GENERATOR_VERSION` and re-records `p0-worlds.txt` only. Scenario: `tools/ci/world-hashes.sh expect linux-gcc-release` on this branch reports `FAIL ... hash differs from the golden value (exit 5)` for all four geo cases (expected for 512 seed 42: `ef21d468…275995d0`, the P1-01 value; got `bcfbbaf9…e98c2`). In CI the `build` job of every preset fails at that step. Also still stale on `p1/02-hydrology-int` (`ae34f2d`). This is not a cross-platform difference: GCC and Clang agree on all twelve cases (section 4). Fix: the validation specialist re-records `p1-worlds.txt` from the new output after the cross-preset comparison passes, as CLAUDE.md ("Golden values") requires in the same change as the version bump.

### Notes

**N1. The added H4 fatal check cannot fire on valid input, on any platform.** `hydrology.cpp:211-217`. The outlet o of lake k is the first cell of k in G3's `order` (lines 189-206); its flood parent r = `parent[o]` is the cell whose pop pushed o, so r precedes o in `order`, and r is therefore not in lake k. L[o] = max(e[o], L[r]) and L[o] > e[o] (o is a lake cell), so L[r] = L[o] = lake level. r is not ocean: then L[r] = 0 and L[o] = max(e[o], 0) = e[o], a contradiction. `downstream` cannot fail: o is land, so `parent[o]` is 0..7 and points inside the grid. The check reads only `order`, `parent`, `level` and the labels, all products of sequential D5/D6 steps with no platform-dependent input, so even on invalid input it fires or not identically everywhere. Checked on output: 300 random fixtures (3×3 to 152×152, 62,539 lakes, flats and extreme heights) never trip it. The size check at line 221 follows from G2: lake k is one 8-connected set, `bfs_region` with predicate `label == k` reaches all of it from any of its cells.

**N2. The rerouting BFS and the outlet choice are fully determined.** `hydrology.cpp:189-225`, `components.cpp:23-48, 83-103`. The outlet is a pop position (D6 order, N3). The BFS is FIFO (the `order` vector is the queue), seeded at the outlet, visiting codes 0..7 (`NEIGHBOURS_8`), and writes `rcv[m] = opposite(code)` (= (k + 4) mod 8, spec H4.2) only on first reach; the seed's receiver is set before the BFS and never written by it. `visited` is shared across lakes, which is correct because labels are disjoint; `bfs_order` is cleared per lake and `bfs` starts its head at the current size. The loop is in id order and the result does not depend on it (each lake writes only its own cells). Acyclicity after rerouting, which `accumulate_topological` also checks: along `rcv`, L never increases; at equal L, a non-lake cell steps to its flood parent (earlier in `order`), a lake cell steps toward its outlet inside the lake and the outlet to a rim cell earlier in `order` than the outlet, so (L, earliest pop position of the cell's lake or of the cell) strictly decreases.

**N3. The flood order is exactly (key, seq).** `flow.cpp:35-110, 117-181`. The spec (7) names a bucket queue; the code uses a monotone radix heap. Every pushed key is max(e, L[c]) ≥ `last`. Invariant: an entry sits in bucket `bit_length(key ^ last)` for the current `last` (when `last` moves to the minimum m of bucket b, entries above b keep their bucket because m agrees with the old `last` on bits ≥ b), so equal keys always share a bucket; appends and the in-order redistribution keep push order within a bucket; bucket 0 pops from the front. Hence pops are ascending (key, seq), the same order as a FIFO bucket queue, independent of the queue structure. Each cell is pushed once (marked when pushed). The scratch is reset on every call (`reset`, `level.assign`, `parent.assign`, `order.clear`); hydrology uses the one-shot overload, so no scratch is shared with elevation or between calls. Keys here are i16 decimetres, so at most 32768 + 1 distinct levels.

**N4. Receiver tie-breaks are fixed.** `flow.cpp:198-251`. Only strictly lower L neighbours compete; the score w·d² (w 2 orthogonal, 1 diagonal) is compared exactly (uint64 below 2^31 drops, U128 otherwise; here d ≤ 32767), and only a strictly greater score replaces the best, so the lowest code wins a tie. On a flat the flood parent is taken. Parallel by G6 blocks: each block writes only its rows of `rcv` and reads `level`, `parent`, `ocean`, fixed before the call.

**N5. Integer bounds hold at 2048² and at the extremes of `i16`.** Accumulation: `uint32`, totals ≤ land cells ≤ 2^22 at N = 2048 (2^24 for any valid `Grid`), indeg ≤ 8 (`flow.cpp:331-373`). River class (`hydrology.cpp:93-102`): A ≥ T ≥ 1, `bit_length(uint64(A div T))` ≤ 23, capped at 15 with `std::min<int32,int32>`; T = (a + 255) div 256 with a range-checked to [256, 10^9] before use, on non-negative operands. Coast distance (`hydrology.cpp:55-57, 234-249`): finite chamfer values ≤ 7 × 2047 + 5 × 0 < 2^14 at N = 2048, INF = 2^40 (`elevation.cpp:607-640`), `32 d + 5` < 2^46 in `int64`, saturated with `min(32767, ·)` before the cast to `int16`, and the ocean side is the negation of a value in [0, 32767], so −32767 at worst: the cast never truncates. `water_depth` (lines 275, 284): ocean −e with e ∈ [−32768, −1] gives 1..32768, which fits `u16` (the spec says ≤ 30000; the code is correct for the whole `i16` range); lake L − e with L ≤ 32767 (the largest land key) and e ≥ 1 gives 1..32766. Checked on a fixture with ocean −32768 and land 32767 around a pit of 1: depths 32768 (ocean) and 32766 (pit), identical at 1 and 5 threads.

**N6. The parallel steps are D1-clean, and failure is deterministic.** H4.1 (G4) and H6/H8 (`hydrology.cpp:269-294`): each block loops over `[index(0, y_begin), index(0, y_end − 1) + width)`, writes only `out.*[i]` of those cells (byte vectors, never `vector<bool>`), and reads `e`, `net` and `t`, fixed before the call. No reduction. The only worker-thread fatals (`river_class`, G4's flat check) are unreachable on valid input, and `run_blocks` rethrows the lowest failing block's exception, so even the error message does not depend on the thread count. H1, H2, H3, H4.2, H5 and H7 are sequential.

**N7. No width, library or environment dependence.** No floating type, `<cmath>`, `<random>`, `Rng` or stream key in the stage (`streams.hpp` adds only the test stream `test.hydro`, appended to `all_stream_names`, which reaches no world). No `long`, `size_t` or `wchar_t` in a stored value: `size_t` appears only in `queue.size()`/`bfs_order.size()` comparisons against `uint32` counts (widening, no sign change). No unordered container, sort, `nth_element`, pointer key, static state or global. `std::min` is called on matching types. Integer division only on non-negative operands. The stage reads its tuning from the content set and the elevation from the pipeline's in-memory copy of the stored layer; resume equality shows that copy is the same as the decoded layer (section 4). Layers are written in the order of spec H9 (lines 319-325) through the existing `write_layer`, codec 1; no new SQL or setting.

**N8. Stage tests are on the test-writer's branch.** This branch has only the pipeline-list updates. D10 thread comparisons, resume and the spec 8.3 queue-independence test are in `p1/02-hydrology-tests` (`d9b08d1`), merged into `p1/02-hydrology-int`. The merge gate should run them; my own thread and resume checks are in section 4.

**N9. Windows/MSVC not run.** No MSVC toolchain here. Nothing in the stage depends on `long`, char signedness, library algorithm choices or `size_t` in a stored value (N7). CI's `compare-hashes` over all seven presets closes this, once S1 is fixed so the `build` jobs reach it.

## 2. What was checked

| Hard rule or D-rule | Result |
| --- | --- |
| Integer only | Yes: `int16` input, `int64` keys and levels, `uint32` accumulation, `uint64` in `bit_length`, U128 in G4 scores. |
| No randomness (P1-00 3) | No stream, no `Rng`, no `hash_at`. |
| H1, H3 (D5) | FIFO 4-connected flood seeded with edge ocean cells in index order; G2 with 8-connectivity and the equal-L key, labels in index order of each lake's smallest cell. `lake_level` read from the component's first cell. |
| H2 (D6) | N3. |
| H4 (D1, D5) | N1, N2, N4. |
| H5 (D4) | Kahn's method: indeg counted in index order, queue seeded in index order, sums of integers, so the values are order-independent; cycle check at the end. |
| H6, H8 (D1) | N6. |
| H7 (D4) | Two in-place sweeps in index and reverse index order, sequential; lakes are land (mask from e < 0). |
| H9, database | N7. |
| Version and goldens | `GENERATOR_VERSION` 0.4.0; `p0-worlds.txt` re-recorded (content hash changes); `p1-worlds.txt` not (S1). |
| Precondition fatals | Fixed messages, sequential checks, identical on every platform. |

## 3. Pipeline registration

`stage.cpp:100-103` appends `{hydrology, run_hydrology}` after elevation; `check_stage_limit` and `geography_stages_until` accept `hydrology`; `world-hashes.sh` comment only. `run_hydrology` checks the map size, the element type and the N × N shape before use.

## 4. What was run

Audit worktree `p1/02-hydrology-audit` at `6973972`, build trees under `~/.cache/the-unmapped/build/hydro-audit/`, worlds under `~/.cache/the-unmapped/out/hydro-audit/`, shipped data (`--data game/data`).

| Run | Result |
| --- | --- |
| `determinism_lint.py --self-test`, then the default scan | self-test passed (13 snippet files); scan 112 C++ and 11 CMake files, 0 findings |
| `worldgen-cli generate`, `linux-gcc-release` and `linux-clang-release`, threads 1, 2, 3, 5, 8: 512 seeds 0, 13, 42 and 1024 seed 1 | one file hash per case over all ten runs: 512 s0 `79111293…2e768ac6`, s13 `bf2874d3…fffad81d`, s42 `bcfbbaf9…2e98c2`; 1024 s1 `9afae41d…d2d7fea2` |
| `world-hashes.sh generate` on both release presets, then `compare` | all 12 cases identical across the two presets |
| `world-hashes.sh expect linux-gcc-release` | the 8 P0 cases ok; the 4 geo cases FAIL against the stale `p1-worlds.txt` (S1) |
| Resume (library driver on `libworldgen.a`, GCC release; the CLI has no resume): `--until elevation` world, its decoded `elevation` passed to `hydrology_layers` at 1 and 4 threads, each layer compared with the decoded layer of the full run | 512 s42 and 1024 s1: elevation equal, all 7 layers equal at both thread counts (14 of 14 each) |
| Driver: 300 random fixtures (`test.hydro` stream, 3 to 152 cells per side, interior 1..8000 or 1..200, ocean border −1..−32768), threads 1, 2, 3, 5, 8, interleaved calls on different grids | all identical, no fatal, 62,539 lakes |
| Driver: extreme fixture (ocean −32768, land 32767, pits of 1 and 2) | depths 32768 / 32766 / 32765, threads 1 and 5 identical |
| ThreadSanitizer: `linux-clang-release` out of tree with `-fsanitize=thread`, 512 seed 13, 5 threads | no report; hash equal to the release builds |
| `linux-clang-asan` (ASan + UBSan): 512 seeds 13 and 42, 5 threads | clean (78 s and 43 s); hashes equal to the release builds |

Not run: ctest (the validation specialist's step), MSVC (N9).

## 5. Lint

The change adds no suppression and the scan is clean. Nothing found in this audit is a pattern the lint could catch that it does not already; no rule added. S1 is a golden-file process check, which belongs to `world-hashes.sh expect` in CI, where it is already caught.
