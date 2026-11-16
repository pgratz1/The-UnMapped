# Determinism audit: P1-02 hydrology follow-up (H2 tie-break on flats, composite rivers)

Auditor: determinism auditor, 2026-11-16. Change audited: `git diff main...p1/02b-hydrology-flats`, branch tip `c1fba52` ("Hydrology: H2 as re-specified (variant B)"), over `main` `0b68064`; commits `44b7d2c` (tie-break, `GENERATOR_VERSION` 0.5.0), `53fbf3f` (composite river drawing), `acb0c41` (merge of the re-specified spec) and `c1fba52` (variant B). Specs: `docs/specs/p1-02-hydrology.md` (H2 variant B, invariants 10 and 11, worked example 5.2, sections 7 and 8.3), `docs/specs/p1-00-geography-overview.md` 10.2 step 4, P0-04 6 (render contract).

Scope: `worldgen/src/stages/hydrology.cpp` and its header (the hydrology-local flood `tie_flood`, `flat_tie`, G4 on decimetre levels, `run_hydrology`), `worldgen/src/inspect/render.cpp` and its header (composite step 4), `worldgen/src/data/kinds.cpp` (new field), `worldgen/include/worldgen/core/streams.hpp`, `version.hpp`, `tools/worldgen-cli/cli.cpp` (render wiring), the TOML data and fixtures. Line numbers are those of `c1fba52`.

**Verdict: pass.** No blocker. One should-fix, a merge-gate item that is not a determinism defect (S1). Eight notes. No lint rule added (section 5).

Classification: **blocker** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** is fragile or breaks a merge-gate check and needs action before merge; **note** is for the record.

## 1. Findings

### Blockers

None.

### Should-fix

**S1. Goldens and stale expectations on this branch alone: `p1-worlds.txt` and two test expectations still describe 0.4.0.** `worldgen/tests/golden/p1-worlds.txt` (last recorded in `e0ef899` for 0.4.0) is not re-recorded. On this branch `tools/ci/world-hashes.sh expect linux-gcc-release` fails `geo-seed-0-512`, `geo-seed-42-512` and `geo-seed-1-1024` (for example 512 seed 42: expected `dc64438c…`, the 0.4.0 value, which a build of `main` reproduces exactly; got `517a2ab1…`). `ctest` fails on GCC and Clang release in the same way: `worldgen-tests` at `inspect_composite_test.cpp:266, 270` (the 0.4.0 `river` palette colours expected on composite river cells, replaced by `composite_river_colour` per P1-00 10.2 step 4) and `worldgen-cli-tests` at `cli_hydrology_test.cpp:113, 150` (0.4.0 hydrology layer hashes and `river_cells`). The failures are identical on both compilers; they are stale expectations, not differences. The test-writer's branch `p1/02b-hydrology-flats-tests` (`3e2413f`) carries the updated tests and is merged in `p1/02b-hydrology-int`. Fix: the validation specialist re-records `p1-worlds.txt` (and the hydrology layer goldens of spec 8.4) on the integration branch after the cross-preset comparison, in the same change as the version bump, as CLAUDE.md "Golden values" requires; merge only the integration branch.

### Notes

**N1. The flood's pop sequence is fixed by (key, seq) alone, whatever the standard library's heap does.** `hydrology.cpp:49-111`. `pops_after(a, b)` (line 58) is `a.key > b.key`, else `a.seq > b.seq`: a lexicographic "greater" on (int64, uint64), a strict total order on distinct pairs. `seq` is the global push counter (line 103, ocean seeds at line 81), incremented once per push and never reused, so no two live entries are equivalent. `std::make_heap/push_heap/pop_heap` guarantee only that after `pop_heap` the back element is one for which no other element `x` has `comp(back, x)`, that is a minimum of (key, seq); because the order is total and the pairs are unique, that minimum is unique. The internal layout (which differs between libstdc++, libc++ and MSVC STL, and between heap-building strategies) therefore decides nothing: by induction, each pop takes the unique smallest live pair, the pushes that follow depend only on the popped cell and `visited`, and so the whole `order`, `parent` and `level_dm` sequence is a function of the inputs. `heap.back().cell` is read after `pop_heap` and before `pop_back` (lines 88-90), as required. Confirmed independently: a reference flood that pops from a `std::set<tuple<key, seq, cell>>` (no heap) gives identical `order`, `parent` and `level_dm` on worked example 5.2, on 381 random fixtures (3×3 to 62×62, one to six height levels including 10000-dm steps, t mixed 0 / 65535 / random / tiny ranges with many duplicates) and on the full 1024 seed 9 world with the shipped tie noise. The three `heap-order` suppressions (lines 84, 87, 104) state exactly this argument and meet the lint README's requirement (comparator a strict total order ending in the push sequence number).

**N2. Key range and overflow.** `hydrology.cpp:100-103`. `lm = max<int64>(e[m], L[c])` with `e` i16 and L either 0 (ocean) or some earlier `max` of i16 values, so 0 ≤ L ≤ 32767 for every cell (land L ≥ 1 by H1). Key = L · 2^16 + t computed in `int64` (`std::int64_t{1} << 16`, a constant shift of a positive value), with t ∈ [0, 65535] (N3): the largest key is 32767 · 65536 + 65535 = 2^31 − 1, so keys lie in [0, 2^31) as the spec says, with 32 bits of headroom in `int64`. `seq` ≤ n ≤ 2^22 at 2048² in `uint64`. Because t < 2^16 the key order refines the L order, which is what the spec's minimax proof relies on; with an empty tie span every key is L · 2^16, and scaling preserves the (key, seq) order, so the flood equals G3 on keys e (checked, N5).

**N3. Visited-at-push semantics are as specified.** `hydrology.cpp:75-107`. Ocean cells are marked visited when seeded in index order (lines 79-83) with L 0 (from `level_dm.assign(n, 0)`) and parent `NO_DIRECTION` (8); a land neighbour is marked visited and gets L and parent at push (lines 98-102), from the first popped neighbour in the 0..7 order, and is never pushed again. Each cell is pushed exactly once, so the heap holds at most n entries (`reserve(n)` at line 77; growth would not affect order anyway) and the reach check (line 108) is the only failure path, sequential and deterministic.

**N4. `flat_tie`: one stream, integer conversion, safe shift, D1-clean parallelism.** `hydrology.cpp:221-245`, `streams.hpp:47-49, 74`. The key is `stream_key(seed, "geo.hydrology.flat")`, a new name registered once and appended at the end of `all_stream_names` (no other stream's key changes; no `Rng`, so no draw-order question). Coordinates are `Fx::from_int(16x + 8)` and `from_int(16y + 8)` in `int64` (`CELL_M` is `int64`), at most 32760 at 2048, inside the P0-01 bounds. `fbm_m` → `fbm2` ends in `clamp(div(sum, norm), -ONE, ONE)` (`core/noise.cpp:86`), so `nz.raw()` ∈ [−2^32, 2^32] and `nz.raw() + 2^32` ∈ [0, 2^33] is never negative: the `>> 17` is a right shift of a non-negative `int64`, identical on every compiler (and C++20 defines it in any case). The result is in [0, 65536]; `std::min<int64>(65535, ·)` (matching types) clamps the single value 65536 (nz = ONE exactly), and the cast to `uint16` is then exact. Observed on the five sweep worlds: land t from 3592 to 65535, so the clamp is reached in practice (512 seed 42, 1024 seeds 1 and 9) and handled. Ocean cells keep 0. Parallelism: `for_each_row_block` with each block writing only `tie[i]` of its own rows into a `vector<uint16_t>` (distinct memory locations, not `vector<bool>`), reading `ocean`, `tuning` and `key`, which are fixed before the call; `fbm_m` has no static or thread-local state (`geo/noise.cpp`, `core/noise.cpp`). `flat_tie` at 1, 2, 3, 5 and 8 threads is identical on all five sweep worlds (section 4). ThreadSanitizer reports nothing (section 4).

**N5. G4 on decimetre levels with flood-parent fallback is unchanged code with the new flood's arrays.** `hydrology.cpp:285`. `geo::receivers(grid, level_dm, parent, ocean, threads)` is the shared G4 already audited (P1-02 audit N4): only strictly lower L competes, exact integer scores, lowest code on a tie, the flood parent when nothing is strictly lower; per-cell, G6 row blocks, inputs fixed before the call. Since L does not depend on t (spec H2 proof) the "strictly lower" sets are those of 0.4.0, and only the fallback parent on flats changes. The lake outlet (line 293, first lake cell in `order`) and the rim check (lines 316-320) read the new `order` and `parent`, both determined by N1. Checked against 0.4.0 (driver, section 4): with an empty tie span all seven hydrology layers equal the 0.4.0 layers byte for byte on all five worlds; with the shipped noise `water_depth`, `lake` and `coast_dist` equal 0.4.0 and the cells with `water` 2 or 3 are identical, as invariant 11 states; only 0↔1 trades occur in `water` (50 to 619 cells per world).

**N6. Composite river drawing: library-independent, bounded, and touches no world byte.** `render.cpp:598-645, 683-695`. The paint list is built by a sequential index-order scan (lines 605-629), with the corner entry only for odd codes ≤ 7 (line 621) and the two thickening entries for k ≥ 4. `std::stable_sort` (line 631) on `k` is specified by the standard to keep the relative order of equal elements; it may allocate a buffer or fall back to an in-place merge, but the permutation it produces is the same either way, so the order is library-independent. Moreover the pixels would not depend even on an unstable sort: entries of equal k paint the same colour, and painting in ascending k means each cell ends with the colour of the largest k targeting it. Bounds: x + dx ∈ [−1, w], y + 1 ≤ h, all in `int64`, rejected before indexing (line 635); `p.y · w + p.x` < 2^32 for valid grids; lake and ocean cells are skipped (line 639). `composite_river_colour` is a pure switch. The render reads layers and writes an RGB buffer; it is not in the generate path and changes no world byte (the world hashes in section 4 do not involve `render`). Contract: per P0-04 6 and P1-00 10.2, the **RGB buffer** of `render` is part of the cross-platform contract; the PNG's compressed bytes are not (P0-04 "PNG encoder output", approved decision 4). The buffer here is an exact integer function of the layers, so it is cross-platform as required.

**N7. The new tuning field is a required, integer-typed `fbm_m` record.** `kinds.cpp:127`, `hydrology.cpp:144`, `game/data/worldgen/hydrology.toml`, both test fixtures. `Field::required("flat_noise", geo::fbm_m_type())` uses the shared schema (`wavelength_m` int(64, 65536), `octaves` int(1, 10), `gain` fixed-point decimal text in [0.0625, 1] parsed to `Fx`, no float), so the record reaches the content hash through the canonical encoding and changes it, which is why every world file hash changes at 0.5.0 (including the empty and pattern P0 cases, re-recorded in `p0-worlds.txt`). `HydrologyTuning`'s defaulted `operator==` covers the new member. No schema-version change is needed: the database schema is untouched; only content and `GENERATOR_VERSION` (0.5.0, `version.hpp:17`) change.

**N8. Windows/MSVC not run; tie-specific tests live on the test branch.** No MSVC toolchain here. Nothing added depends on `long`, `char` signedness, `size_t` in a stored value, or library algorithm choice (N1, N6). CI's `compare-hashes` over all seven presets closes this on the integration branch. The spec 8.3 queue-independence test with random t, worked example 5.2 and the `flat_tie` unit tests are on `p1/02b-hydrology-flats-tests`; my own equivalents are in section 4.

## 2. What was checked

| Rule | Result |
| --- | --- |
| Integer only | Yes: i16 heights, int64 levels and keys, uint64 seq, uint16 t, `Fx` noise converted by an integer shift; render integer RGB. No float, `<cmath>`, `<random>`. |
| Streams (D9) | One new noise key, `geo.hydrology.flat`, registered and appended; location-keyed (cell centre), no `Rng`. |
| H2 step 1 (D1) | N4. |
| H2 step 2 (D6) | N1 to N3. |
| H4 (D1, D5) | N5. |
| H3, H5 to H9 | Unchanged code; `net.flood.*` renamed to `level_dm`, `parent`, `order`; layer write order unchanged. |
| Unordered containers, pointers, time, environment | None added. |
| Database | No new SQL, setting or table; layers written by the existing `write_layer` in spec order. |
| Version and goldens | `GENERATOR_VERSION` 0.5.0; `p0-worlds.txt` re-recorded; `p1-worlds.txt` not (S1). |
| Lint | Clean, three justified suppressions (N1). |

## 3. Lint

`python3 -B tools/lint/determinism_lint.py --self-test`: passed (13 snippet files, 0 failures). `python3 -B tools/lint/determinism_lint.py` on `c1fba52`: 112 C++ files, 11 CMake files, 0 findings.

## 4. What was run

Audit worktree `.claude/worktrees/p1-02b-hydrology-audit` (branch `p1/02b-hydrology-audit` from `c1fba52`), and a detached worktree of `main` `0b68064` for 0.4.0; all builds and outputs under `~/.cache/the-unmapped/`.

**Thread and compiler sweep.** `linux-gcc-release` and `linux-clang-release`, `generate --threads 1, 2, 3, 5, 8`, one file hash (`hash --db`) per run, ten runs per world, one distinct hash each:

| World | Runs | 0.5.0 file hash | 0.4.0 file hash (`main`) |
| --- | --- | --- | --- |
| 512 seed 0 | 10 | `0b76b30c34fde05ff5d6c9ee9dbc2d63802a9d673982cb78beb398f880ca8d73` | `862e7bb7…02287867` |
| 512 seed 13 | 10 | `6b494dba26ad439a3dbd15d1bf7dc2fc7f38cfbae6fb62af393680497a1e3387` | `29dc1082…5cadce7d` |
| 512 seed 42 | 10 | `517a2ab152e0e10d7a92fcb59e964bc6571392ce33976c06182fb0dc58e09a23` | `dc64438c…0a9f0ee084` |
| 1024 seed 1 | 10 | `2076a1de63116cdd3f94e6948c757ee72255f2c20c77b3699c457b187dbe70d7` | `e8bc7a81…0d647fc4` |
| 1024 seed 9 | 10 | `f87b0d62a39b1fcbdbe665832eee75b3b650361eed6e12d54f41e923ddbebeb2` | `1a8e84e1…d033d3` |

The 0.4.0 hashes of 512 seeds 0, 42 and 1024 seed 1 equal the `p1-worlds.txt` golden values, so the `main` build is the recorded 0.4.0.

**ThreadSanitizer.** Own build (Clang, RelWithDebInfo, `UNMAPPED_SANITIZE=thread`), `generate --seed 13 --map-size 512 --threads 5`, `halt_on_error=1`: exit 0, no report; file hash `6b494dba…` equal to the sweep.

**AddressSanitizer and UBSan.** `linux-clang-asan`, `generate --seed 42 --map-size 512 --threads 5`: exit 0, no report; file hash `517a2ab1…` equal to the sweep.

**Tie-break driver** (scratch program linked against the GCC release `libworldgen.a`; per world, reads `elevation` from the 0.5.0 world and compares with the 0.4.0 world). All five worlds pass:

- `elevation` of 0.5.0 equals 0.4.0 (layer hash).
- `hydrology_layers` with an **empty tie span**: all seven layers equal the 0.4.0 stored layers (layer hashes).
- With the **shipped noise** (`flat_tie(seed, …)`): all seven layers equal the stored 0.5.0 layers (the driver reproduces the stage); `water_depth`, `lake` and `coast_dist` equal 0.4.0; the `water` 2 and 3 cells are identical to t = 0.
- `flat_tie` and `hydrology_layers` at 2, 3, 5 and 8 threads equal 1 thread.

Example (1024 seed 9, layer hash prefixes): `water_depth` `9b73d29b…`, `lake` `94770db9…`, `coast_dist` `5e63963c…` in 0.4.0, t = 0 and 0.5.0; `flow_dir` `5d167f3c…` (0.4.0, t = 0) against `923ad465…` (0.5.0).

**Reference-queue driver** (N1): `std::set` flood against `hydrology_network`: worked example 5.2 reproduced exactly (land pop order 11 7 13 12 9 8 6 4 14 3 2 1 17 19 18 16; `flow_dir` and `flow_accum` rows as in the spec); 381 random fixtures and the 1024 seed 9 world identical in `order`, `parent` and `level_dm`.

**Goldens and tests.** `world-hashes.sh generate` and `expect linux-gcc-release`: the eight P0 cases pass; the three geo cases fail as in S1. `ctest` on both release presets: the stale expectations of S1 only, identical on GCC and Clang.

## 5. Lint rules

No rule added. The heap is already a review rule (`heap-order`) whose suppression demands the total-order argument, which these three meet. `std::stable_sort` is fully specified by the standard and needs no rule; a plain `std::sort` with a non-total comparator would be the risk, and the code base has none in the output path of this change.
