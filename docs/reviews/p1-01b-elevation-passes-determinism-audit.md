# Determinism audit: P1-01 elevation revision "natural passes and final talus" (generator 0.6.0, schema 2)

Auditor: determinism auditor, 2026-11-16. Change audited: `git diff main...p1/01b-elevation-passes`, branch tip `88da7d3` ("Elevation passes: drop two unused includes"), merge-base `aebd53d`. Specs (on `main`): `docs/specs/p1-01-elevation.md` fourth version (E5.2, E6.5.0 to E6.5.8, E7, E8.7, section 7 with its overflow bounds, section 8.1, 9.1), `docs/specs/p1-00-geography-overview.md` (D1 to D10, G3, 9.3, 10.3), `docs/specs/p0-02-world-db.md` (5.2 schema-change rules, 5.3 table rules). Line numbers are those of `88da7d3`.

Scope: `worldgen/src/stages/elevation_passes.cpp` (new), `worldgen/src/stages/elevation.cpp` (E5.2 mask and profile, early release of c and ns, E6.5 call, E7, E8.7 `final_talus`, `elevation_run`, pass rows in `run_elevation`), `worldgen/include/worldgen/stages/elevation.hpp`, `worldgen/include/worldgen/db/schema.hpp`, `worldgen/src/db/writer.cpp`, `writer_state.hpp`, `worldgen/src/inspect/geography.cpp` (`pass_walkable`, `pass_lines`), `worldgen/src/inspect/report.cpp` (reading table `pass`), `worldgen/src/data/kinds.cpp`, `streams.hpp`, `version.hpp`, the TOML data and fixtures.

**Verdict: pass.** No blocker. Three should-fix items, none of them a determinism defect: the schema reference is not updated for version 2 (S1), the goldens and several test expectations still describe 0.5.0 on this branch alone (S2), and spec invariant 8 contradicts E8.7 (S3). Twelve notes. No lint rule added (section 5).

Classification: **blocker** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** is fragile or breaks a merge-gate check and needs action before merge; **note** is for the record.

## 1. Findings

### Blockers

None.

### Should-fix

**S1. `docs/specs/schema.md` still describes schema version 1.** At `88da7d3` (and on `main`) the file says "Current schema version: 1", section 1 is "DDL at version 1" with only `meta` and `layer`, section 2 lists `schema_version` = `1`, and the `pass` DDL is still a row of section 4 "Planned changes". The code is at version 2 (`schema.hpp:20`, `DDL_STATEMENTS` with `DDL_PASS` third at `schema.hpp:48`), and a world file holds `user_version` 2 and the `pass` table. P1-01 9.1 and P0-02 5.2 rule 2 (hard rule 7) require section 1, section 2 and the changelog to be updated by the branch that makes the change. Not an output defect: the DDL string in the code is byte-equal to P1-01 9.1 and to the text in schema.md's section 4 (checked by comparing `sqlite_schema.sql` of a generated world with the schema.md text). Fix: move the `pass` DDL into section 1 ("DDL at version 2", executed after `layer`'s), set section 2's `schema_version` to `2`, record version 2 in the changelog, drop it from "Planned changes" and renumber the planned `biome` and `region` rows to 3 and 4, in this branch or the integration branch.

**S2. Goldens and stale test expectations on this branch alone.** `ctest --preset linux-gcc-release` at `88da7d3`: `worldgen-tests` fails `P1-01 golden: every case, step by step` (first divergent step h7, as expected for a change of E7; `stages_elevation_golden_test.cpp:247, 250, 277`), `P1-02 8.4 golden …` and `P1-01 6.1 invariant 8 …` (S3); `worldgen-cli-tests` fails `cli_hydrology_test.cpp:108, 113` (0.5.0 layer hashes). `worldgen/tests/golden/p1-worlds.txt` and `p1-01-elevation.txt` are the 0.5.0 values (last recorded in `8f9a1f9`). These are expectations of the previous version, not differences between builds: every world in section 4 has one hash across compilers and thread counts. Fix: the test writer's integration and the validation specialist's re-recording after the cross-preset comparison, in the same change as the 0.6.0 bump (CLAUDE.md "Golden values"); merge only the integrated branch.

**S3. Spec invariant 8 contradicts E8.7, and its test fails for that reason.** `p1-01-elevation.md:985` (invariant 8): "With `erosion_iterations` = 0, the layer equals E9 and E10 applied to the E7 output." E8's notes (`:863`) say "With `erosion_iterations` = 0 the iterations do nothing (E8.7 still runs)", and the code does exactly that (`elevation.cpp`, `elevation_run`: `final_talus` after `erode`). With the shipped `final_talus_iterations` = 100, any world whose E7 surface has a step steeper than 5:1 (every world with a range at the shore) has a layer that differs from E10(E9(E7)). The test `stages_elevation_golden_test.cpp:408` follows the invariant's text and fails. The code is correct by E8.7; the invariant needs "E10(E9(E8.7(E7)))", or "and `final_talus_iterations` = 0". Fix: a spec correction by the spec writer (proposals entry if needed), then the test writer's test. Not a determinism defect.

### Notes

**N1. Every queue pops by a unique key, so the standard library's heap decides nothing.** Four `std::priority_queue` with `std::greater<>` (lexicographic on `pair`/`tuple` of integers, a strict total order on distinct values):
- `dijkstra` (`elevation_passes.cpp:118-158`, suppression `:122`): entries (distance, cell). A pair is pushed for m only when nd is strictly below m's current distance (`:147`), so the distances pushed for one cell strictly decrease and no pair repeats; a source listed twice is skipped (`:125`). Stale entries are skipped by `d != s.dist[c]` (`:138`). The relaxation order (8-neighbour order, first strictly shorter wins, `src` copied at `:154`) is the spec's (E6.5.0).
- `divide_flood_impl` (`:162-209`, suppression `:169`): entries (level, seq, cell), seq the global push counter (`:191, :205`), unique per entry. Seeds in index order, level, parent and side set at push from the popped cell, visited-at-push via `level[m] >= 0` (`:199`), as E6.5.3 2.
- the range geodesic in `ranges` (`:353-389`, suppression `:358`): unique (distance, cell) as for `dijkstra`; only the distances (values) are used, and the far ends are chosen by an explicit (largest distance, smallest index) argmax (`:380-387`).
- the basin flood in `basins` (`:554-593`, suppression `:560`): each land cell is pushed exactly once (`level[m] >= 0` check at `:580`; seeds break after the first ocean neighbour at `:571`), so the pairs (level, cell) are unique. Only the levels Lb are used, and the minimax level of a priority flood does not depend on the order among equal levels in any case.

A strict total order on unique entries makes the minimum at each pop unique, so by induction the whole pop sequence (and every parent, source and side) is a function of the inputs, whatever heap layout libstdc++, libc++ or the MSVC STL uses. Confirmed by experiment: a build in which all four queues were replaced by a `std::set`-backed queue that aborts on a duplicate insert (an uncommitted local change, `$BUILD/setq`) never aborted and gave the same world file hash as the normal builds on 512 seeds 13 and 42 and 1024 seeds 1, 5 and 47 (section 4). The four suppressions state this argument and meet the lint README's requirement.

**N2. The basin flood equals G3's levels on every map, but has no G3 precondition check.** `elevation_passes.cpp:562-574`. G3 (P1-00 7.3) seeds the ocean at level 0 and pops all ocean entries before any land entry (land keys ≥ 1), so every land cell next to the ocean gets L = max(kb, 0) = kb from an ocean neighbour, which is how this flood seeds it; after that both are the same minimax recursion. Equal on the N × N map. On a test grid with no ocean cell at all, G3 is fatal (P1-00 appendix 13) while this flood quietly finds no basin; the header comment of `pass_layout` says "on any W × H grid with an ocean cell" but nothing checks it. Test grids only, no output effect. Suggestion: a `fatal` when the grid has no ocean cell, to keep G3's precondition.

**N3. Tie rules match the spec.** Box queries `box` (`:79-97`) visit y ascending, x ascending, so index order. Argmax and argmin: `world_top` strict `>` in index order (`:325`); range end and top with explicit index tie (`:383, :403`); partner first smallest K in E, S, W, N order with strict `<` (`:442`); crest-axis cmin/cmax strict `<`/`>` in box order (`:726-731`); local minimum test lexicographic on (dh, index) (`:502`). Col choice (`:903-904`): smallest dh, then largest score, then smallest cell, a strict total order on the candidate list (candidates are distinct divide cells). Overlapping pass zones: P is a minimum (order-free, `:971`); adj (`:1006-1017`): records are appended in site order and, within a site, in index order of the sorted `touched`; `std::stable_sort` by cell keeps site order among records of one cell (guaranteed by the standard whether or not it obtains a buffer), and the first record with the strictly largest bump wins, which is the spec's "largest bump, ties to the lower site index" with `best` starting at −1 (a bump of 0 raw gives adj 0 in both). E6.5.8 reads P after every site of E6.5.7 (`factor` runs the second loop only after the first, `:986-1004`).

**N4. The three `unstable-sort` suppressions are sound.** `elevation_passes.cpp:957`: `s.touched` gets a cell only when its distance goes from unset to set (`:129, :151`), and `reset` clears it per site, so it holds unique cell indices; sorting unique integers has one result. `elevation.cpp:1170`: `next` is filled through `mark`, which uses `flag` to add each cell once (`:1153-1158`) and clears `flag` before the sort; unique cell indices. `geography.cpp:1024`: plain `int64` values of which only the elements at percentile ranks are read; equal elements are indistinguishable.

**N5. Integer bounds hold in the code's types at N = 2048 and extreme in-bounds tuning; one spec bound is understated.** Raw `int64` products outside `Fx` (whose operations are all checked and fatal on overflow, so an `Fx` overflow could only be a deterministic error, never a difference):
- `d2`, scores (`:896-901`): d2 ≤ 2 × 2047² < 2^23, score < 2^23 × 1330 < 2^34.
- `along` (`:685-695`): exits are grid cells, |v| ≤ 2047 per component; |t| ≤ 74 per component (difference of two floor-divided centroids of offsets within radius 37); |t'| ≤ 2 × rise_b ≤ 2500. Largest product: 2 (v·t')² ≤ 2 × (2 × 2047 × 2500)² < 2^48.
- chord (`:740-754`): 100 (chord·t')² < 2^54, 33 chord2 |t'|² < 2^52, 4 arc5² with arc5 ≤ 62500 after the side test. All well inside `int64`.
- Cross products `ax * dy - ay * dx`: |a| ≤ 2047, box radius ≤ 1250 (and 187 for (n)), < 2^23.
- Search distances are `int32`: geodesic < 7 × 2^22 < 2^25; reach5 ≤ 6250.
- **Understated:** section 7 gives "lim5 ≤ mul_div(9500 m, 5, 16) + 2 = 2971 fifths", using the largest halfwidth (5000 × 1.9 = 9500 m), but lim5 is computed from hwmax = halfwidth × (1 + `pass_width_var`) ≤ 9500 × 1.9 = 18050 m (`:949-950`), so lim5 ≤ 5643. Still far inside `int32` and every downstream bound (fm(5643) ≈ 18 km), so no effect; the spec sentence should read hwmax.
- The 8-bit and 16-bit derived values (`derived`, `:305-319`): `spacing_` ≥ 156 (pass_spacing_m ≥ 500), so the division at `:395` cannot be by 0; `min_len_` ≤ 100000 × 5 × 2048 < 2^30.

**N6. Signed shifts and divisions.** `>> 16U` on `h0` (`:247`) may see negative values; C++20 defines `>>` on negative signed values as the arithmetic shift (floor), which is what E6.5.0 specifies and what GCC, Clang and MSVC have always produced. All `<<` are of non-negative values (`maxm`, `:767, :1125`). Divisions of possibly negative values use `fdiv` (`:73-75`, floor, b > 0, checked for both signs). Every other `/` has non-negative operands: `(hj * 3) / 10`, `(r.length - min_len_) / spacing_` (length ≥ min_len there), `whole_cells(...) / 2` and `floor_int() / 16` on fields bounded ≥ 0 (halfwidth ≥ 0 since `pass_width_jitter` ≤ 0.9), `(coast_clear_ - 1) / 5` under `coast_clear_ > 0`. `fifths_m` of a negative offset (`:961`) goes through `Fx::from_ratio`, an exact integer rounding of P0-01.

**N7. Widths.** No `long`, no `size_t` in a stored value. Sizes and positions are `size_t` only as indices; values stored in vectors are `uint32` (cells, axis positions), `int32` (distances, labels) or `int64`. `comp.size(k)` is brace-initialised into `int64` (`:596`), which would not compile if it narrowed. `CellIndex` is `uint32`, `x_of`/`y_of` return `int32` and are widened before subtraction. `row.id` from `size_t s` + 1 (cast to `int64`). Nothing depends on the 32-bit `long` of MSVC.

**N8. Streams.** One new name, `geo.elevation.pass`, appended to `all_stream_names` (`streams.hpp:47, 79`), so no other key changes. Uses: `hash_at(key_, cell, j)` for the score (`:900`), `hash_at(key_, col, 16 / 17)` for the jitters (`:769`), `noise2(substream(key_, 3), …)` with the band coordinate 3s + 2 (`:931, :958, :964`). All location-keyed: a value depends on (seed, cell, slot) or on (seed, position along the crossing, site index), never on how many draws came before, so the order of evaluation cannot change it. No `Rng`. The lazy `basins()` (`:538`) is a pure function of the inputs, so running it only when a candidate reaches test (g) changes nothing.

**N9. E7 is as specified.** `elevation.cpp:850-865`. `natural = mul(scale, unit)`, replaced by `pass_lowered(…, P)` only when P < ONE, then `min(m − adj, natural)` only when adj ≠ 0, then the clamp. This is E7 text exactly, including P1-01 section 12 item 16 (d) (the `min` applies only where adj ≠ 0). Per cell, G6 row blocks, reading `factor` and `adj` that are complete before the call: D1.

**N10. E8.7: the parallel active-set evaluation equals plain Jacobi steps and does not depend on threads or block size.** `elevation.cpp:1091-1173`. Within a step, `run_blocks` over blocks of 4096 entries of `active` reads only `h` and `ocean` (not written during the call) and writes only `next_h[k]` and `changed[k]` of its own k (distinct elements of `vector<int64_t>` and `vector<uint8_t>`, distinct memory locations); the new values are applied after the call, sequentially in list order, and since each `h[c]` is written by exactly one entry the application order is irrelevant. Each entry is a function of h alone, so the block size (4096) and the thread count cannot change any value. Active-set equivalence: by induction, a cell outside the step-(it + 1) set had δ = 0 in step it and so did each of its neighbours; a cell with δ = 0 keeps its height (t = max(floor, h) = h when h ≥ floor), so the cell and its neighbourhood are unchanged and its δ is again 0. `changed` is set on δ ≠ 0 rather than on a changed height, a superset, which is harmless. The 8-neighbour sum is a sum of exact integers. Confirmed by a driver (section 4) that compares `final_talus` with a full-grid Jacobi loop written from the E8.7 text, on the real post-erosion grids, at 1, 2, 3, 5, 8 and 17 threads: identical on 512 seeds 0 and 41 and 1024 seed 1 with the shipped values (60 to 2506 cells changed), and on 512 seeds 13 and 41 and 1024 seed 47 with extreme in-bounds tuning (`final_talus_slope` 0.05 or 3, rate 0.125, 50 to 300 steps; 13 069 to 346 314 cells changed, so many blocks per step).

One precondition is assumed, not checked: equivalence needs every land h ≥ floor on entry (contract of E7/E8). A caller of the public `final_talus` with a land cell below the floor would see it raised to floor by a full Jacobi step but left unchanged by the active set if its δ is 0. The stage never does this (E7 clamps, E8 keeps `max(·, floor)`), so there is no output effect. Suggestion: a check (fatal) or a sentence in the header comment.

**N11. Early freeing.** `c` and `ns` are released (`elevation.cpp:791-794`) after the E5.2 row blocks have joined, and nothing after E5.2 reads them (E5.3 reads `ocean`, E6 `out.unit`, E6.5 `f.nb` and `f.w_b`, E7 `out.*`); the early `check_size` calls precede the release. `f` and `owned` alias one non-const object declared in `elevation_run`, so the release through `owned` is defined. In `Layout::run` (`elevation_passes.cpp:1041-1050`) the range, divide and basin scratch is released before `factor`, which reads only `in_`, `out`, `reached_` and the tuning. In `elevation_run`, `h7` is read from the moved-to `h` and the sites from the moved-to `result.sites`. ASan/UBSan and TSan runs are clean (section 4).

**N12. Database and report.** `write_pass` (`writer.cpp:438-466`): ids must be exactly 1, 2, 3, … in call order (`:442`), kind and capped must be 0 or 1, every column bound as `int64`, the path as a blob; rollback restores `next_pass_id` (`:417`) as it does the layer id. `run_elevation` writes the rows in site order after the layer row, in the stage's transaction, so rows are inserted in primary-key order. The path is encoded little-endian by shifts and masks (`elevation.cpp:1447-1451`), never by copying host memory, so it is the same on a big-endian host; the report decodes it the same way, converting each `char` through `uint8_t` first (`report.cpp:149-153`), so `char` signedness does not matter. `saddle_dm` and `col_dm` are `mul_div` (RHA) of integers. The table exists in every world (DDL executed for every world, including empty and test-pattern ones; `p0-worlds.txt` re-recorded on this branch). A world at 512 seed 13 has `user_version` 2, page size 4096 and the DDL byte-equal to the spec. The reader's `table_rows` appends `ORDER BY` the primary key, and `pass_lines` uses only counts and a sorted multiset, so the report does not depend on row order either. `pass_walkable` is integer only (squared distances, i16 heights, the existing integer slope test against `SLOPE_TAN4_35_DEG`); its stack search order cannot change a yes/no answer. All numbers are printed through `db::format_u64` and `metres1`, which use no locale.

**N13. Tuning fields and named checks.** `kinds.cpp:131-164`: the 34 fields in the order and with the types and bounds of P1-01 4.2, all required, all integer or decimal-text fixed point (no float). `pass_masks` and `pass_saddle_clamp` (`kinds.cpp:46-60, 170-171`) with the spec's messages. `ElevationTuning` initialises every new member and its defaulted `operator==` covers them. Spec slip for the spec writer: 4.3 check 5 says the clamp of "E6.5.6 (g)"; the clamp is in (h).

**N14. Windows/MSVC not run.** No MSVC toolchain here. Nothing added depends on `long` width, `char` signedness, `size_t` in a stored value, host byte order, or library algorithm choice (N1, N3, N4, N7, N12). CI's `compare-hashes` over all seven presets closes this on the integration branch.

## 2. What was checked

| Rule | Result |
| --- | --- |
| Integer only | Yes: `Fx` (checked operations), `int64`/`int32`/`uint32` integers. No `float`, `double`, `<cmath>`, `<random>`, `std::rand`. |
| Queues and searches (D6) | N1, N2. |
| Box queries, argmin/argmax, col choice, overlapping zones (D7, D4) | N3. |
| Sorts | N4. |
| Overflow bounds (section 7) | N5. |
| Signed shifts and divisions | N6. |
| Width differences (MSVC) | N7. |
| Streams (D9) | N8. |
| E7 (D1) | N9. |
| E8.7 (D3 of D2 steps, parallel) | N10. |
| Grid lifetimes | N11. |
| Unordered containers, pointer values, capacity, time, environment, thread ids | None in the diff. No container is ordered by pointer or hash; no result depends on a vector's capacity. |
| Database (schema 2, row order, blob encoding, settings) | N12; schema reference S1. |
| Tuning and checks | N13. |
| Build flags | No change to `cmake/UnmappedCompilerFlags.cmake` or `extern/CMakeLists.txt`; lint scans for floating-point flags. |
| Version and goldens | `GENERATOR_VERSION` 0.6.0, `SCHEMA_VERSION` 2; `p0-worlds.txt` re-recorded; P1 goldens not yet (S2). |

## 3. Lint

`python3 -B tools/lint/determinism_lint.py --self-test`: passed (13 snippet files, 0 failures). `python3 -B tools/lint/determinism_lint.py` on `88da7d3`: 113 C++ files, 11 CMake files, 0 findings. Seven suppressions in the diff (four `heap-order`, three `unstable-sort`), each justified (N1, N4).

## 4. What was run

Audit worktree `.claude/worktrees/p1-01b-audit` (branch `p1/01b-elevation-passes-audit` from `88da7d3`); every build and output under `~/.cache/the-unmapped/` (`build/p1-01b-audit/…`, `out/p1-01b-audit/…`).

**Thread and compiler sweep.** `linux-gcc-release` and `linux-clang-release`, `generate` (all stages, shipped data) at `--threads` 1, 2, 3, 5 and 8 for 512 seeds 0, 13, 41, 42 and 1024 seeds 1, 5, 47: 70 files, one SHA-256 per world.

| World | Sites (`report` and table `pass`) | File SHA-256 (all 10 builds × threads) |
| --- | --- | --- |
| 512 seed 0 | none | `e07cbef819beb01b5ecdc47518cf3ea72fe7625247bf6611daab9d53bb1fb0bf` |
| 512 seed 13 | 2 lowered, both capped, two ranges | `19567a64b54268815563a1c77038dc44147e53ca995ea21cff3845de57b5e499` |
| 512 seed 41 | none | `00477f180022792854536997c96c42b2a18a9ab88c2a3e7b34034ca2a7d9e9b5` |
| 512 seed 42 | 1 lowered, uncapped, walkable | `3f299d2eac3bdb695f58f292628a76c255a2740a58d49b4d8bacbecbdb3069c5` |
| 1024 seed 1 | none | `830a7943df18b90479bf84d0a55d7f81ced17b523a99a53d3c7b866d51f6105b` |
| 1024 seed 5 | 2 lowered, both capped, one range | `44a426d5eb38bcf576ea22b7b235e9afe4c072e4b06ac7eb15317910fd9dd118` |
| 1024 seed 47 | 1 natural gap, 1 lowered capped, one range | `df6a59d88df062c05a0a7ee51e9eb697142972f8cdf081cbd846c370b85bfc97` |

The sample has worlds with zero, one and several sites, several sites on one range, a natural gap, capped and uncapped cuts.

**Heap independence.** A local build with the four priority queues replaced by an abort-on-duplicate `std::set` queue (N1): same hashes as the table for 512 seeds 13, 42 and 1024 seeds 1, 5, 47; no abort.

**E8.7 reference driver** (scratch, linked against the GCC release `libworldgen.a`): E1 to E8 through the public API, then `final_talus` at 1, 2, 3, 5, 8 and 17 threads against a full-grid Jacobi loop written from the E8.7 text. Zero differing cells in all 36 comparisons (N10).

**ThreadSanitizer.** Own build (`linux-clang-release` with `-fsanitize=thread -g`, `build/p1-01b-audit/tsan`), 512 seed 13 (two lowered passes) at 5 threads: no report, exit 0, file hash equal to the table.

**ASan/UBSan.** `linux-clang-asan`, 512 seed 13 at 5 threads: no report, exit 0, file hash equal to the table.

**Big-endian review.** By reading the code (N12): the path blob is written and read by explicit little-endian shifts; layer blobs use the existing P0-02 codec; no `memcpy` of host integers into a blob in the diff.

**ctest** `linux-gcc-release`: fails only on stale 0.5.0 expectations and invariant 8 (S2, S3).

## 5. Lint rules

None added. Every pattern found is either already covered (`heap-order`, `unstable-sort`, floating point, unordered containers) or is a spec and documentation matter (S1, S3) that a source lint cannot see.
