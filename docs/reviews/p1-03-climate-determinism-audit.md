# Determinism audit: P1-03 climate (generator 0.7.0 candidate)

Auditor: determinism auditor, 2026-11-17. Change audited: `git diff main...p1/03-climate`, merge-base `23803d0`, branch tip `aea0da7` ("Climate to the revised P1-03 …") and the follow-up `3c4e649` ("Streams: test.climate is not registered"), which only removes the `test.climate` registration from `streams.hpp`. Specs (on `main`): `docs/specs/p1-03-climate.md` as revised 2026-11-17 (C1 to C7, section 7, invariants 1 to 7, section 10 seams), `docs/specs/p1-00-geography-overview.md` (D1 to D10, G6 row blocks). Line numbers are those of `3c4e649` (`climate.cpp` is the same at `aea0da7`).

Scope: `worldgen/src/stages/climate.cpp` and `worldgen/include/worldgen/stages/climate.hpp` (new), `worldgen/src/data/kinds.cpp` (`worldgen.climate` schema, `wind_weights_positive`), `worldgen/include/worldgen/core/streams.hpp`, `worldgen/src/stages/stage.cpp` (pipeline), `worldgen/include/worldgen/version.hpp`, `worldgen/src/inspect/geography.cpp`, `render.cpp`, `report.cpp` and their headers, the three `climate.toml` files.

**Verdict: pass.** No blocker: on every input that `generate` can produce, the stage's output is a function of the seed, N, the four input layers and the tuning, independent of compiler and thread count, and every arithmetic step is exact or checked. One should-fix: the C4 seams have signed `int64` overflow (undefined behaviour, not a fatal error) on inputs that the public functions accept but the stage never passes (S1). Five notes. No lint rule added (section 5).

Classification: **blocker** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** is fragile or breaks a stated contract and needs action before merge or soon after; **note** is for the record.

## 1. Findings

### Blockers

None.

### Should-fix

**S1. The C4 window sums can overflow `int64` through the public seams; the bound is enforced only by the stage's callers.** `climate.cpp:229-298` (`box_pass`), reached from `climate_box_mean` (`:302`), `climate_triangle_mean` (`:308`) and `climate_layers` (`:451`).

The section 7 argument needs three facts: S ≤ 32767, a window of at most 251² cells (h ≤ 125), and a row no wider than the map. Only the first is checked at the seams (`check_surface_range`, `:209-219`). The other two come from the schema (`smooth_radius_m` ≤ 4000, `kinds.cpp:206`) and from `run_climate`'s N ≤ 2048 check (`:464`). Nothing in `box_pass` or its three callers enforces them. Pass 2 sums `Fx` raw values of up to 32767 × 2^32 = 1.407 × 10^14, so any sum of more than ⌊(2^63 − 1) / (32767 × 2^32)⌋ = **65538** values can overflow. Two sums in the code can hold that many:

- the column accumulator `acc` (`:265`, `:283`) holds one window, min(W, 2h + 1) × min(H, 2h + 1) values;
- the row pass's prefix `prefix[x + 1] = prefix[x] + surface[…]` (`:245`) runs over the **whole row**, W values, whatever the radius.

Grids accepted by every seam go up to 2^24 cells with any width (`grid.hpp:25`; 16777216 × 1 is valid). So a test or future caller with all S = 32767 overflows on:

| Input (S = 32767 everywhere) | Where | Observed |
| --- | --- | --- |
| 260 × 255, `climate_triangle_mean` r_cells 258 (h = 129, window 259 × 255 = 66045) | `:265` | UBSan: `signed integer overflow: 9221823963137572864 + 36449897087500288`; GCC release returns wrong values and no error |
| 260 × 255, r_cells 100000 (whole grid, 66300 cells) | `:265` | UBSan, same |
| 4096 × 17, r_cells 8192 (whole grid, 69632 cells) | `:265` | UBSan |
| 65539 × 1, r_cells 0 (the row prefix alone) | `:245` | UBSan: `9223372028264841216 + 140733193388032`; release happens to give the right answer, because the wrapped prefix differences cancel |
| `climate_layers` on a 65539 × 1 land strip, or with a hand-built `ClimateTuning::smooth_radius_m` above 4000 on a large grid | as above | same paths (no check of either in `climate_layers`) |

This breaks two contracts: the header's "Every function raises FatalError … on … an arithmetic overflow" (`climate.hpp:20-23`), and the comment at `climate.cpp:224-228`, which states the bound as if it held unconditionally. **No world is affected.** `generate` calls `climate_layers` only through `run_climate`, with N ≤ 2048 and a schema-validated radius ≤ 4000. That gives h ≤ 125, a window of at most 63001 values (peak 8.866 × 10^18, margin 3.6 × 10^17 below 2^63 − 1) and a row prefix of at most 2048 × 1.407 × 10^14 = 2.9 × 10^17. Section 4 shows these extremes run cleanly under UBSan. So this is not a blocker. It is the kind of defect that becomes one silently, though: a later stage could reuse `climate_triangle_mean` on a larger grid, someone could raise the `smooth_radius_m` bound, or a test could use a wide strip. And the release build gives a wrong answer, not an error.

Fix (for the engineer). I would rather see an explicit guard at entry than checked arithmetic or 128-bit sums in the loops:

```cpp
// in box_pass, or in check_surface_range with r = r_cells / 2 for the triangle
constexpr std::int64_t MAX_SUM_TERMS = INT64_MAX / (SURFACE_MAX_DM << 32);   // 65538
const std::int64_t span = 2 * r + 1;   // r already clipped to max(w, h)
if (w > MAX_SUM_TERMS || std::min(w, span) * std::min(h, span) > MAX_SUM_TERMS) {
    fatal("climate: C4: window or row too large for exact int64 sums");
}
static_assert((2 * (4000 / 16 / 2) + 1) * (2 * (4000 / 16 / 2) + 1) <= MAX_SUM_TERMS);  // schema bound
static_assert(ELEVATION_MAX_N <= MAX_SUM_TERMS);                                         // row prefix
```

Why a guard rather than the alternatives:
- It is O(1). It states the section 7 argument in code. It turns the violation into the spec's fatal error at the seam's entry. And it keeps the inner loops plain, exact and branch-free.
- `checked_add` on every element works too, but puts a call and a branch on every add of two passes for nothing on the legal domain. It would also fail part-way through a pass instead of at entry.
- 128-bit sums (`U128` from `wide.hpp`; all values here are non-negative) remove the margin question entirely. But they need a 128-by-64 division path for `from_ratio` and `div_int`, and they are only worth it if the radius bound or S bound is ever raised. Section 7 already names that case.

The `static_assert`s tie the schema bound and the map-size bound to the margin, so that raising either fails to compile instead of overflowing. Replacing the whole-row prefix with a running window along x, as the column pass does, would make the first condition unnecessary. The test writer should add one fatal-path test (for example 260 × 255 with r_cells 258).

### Notes

**N1. C2 wind (D9).** `climate.cpp:152-166`. Exactly one `Rng`, on `stream_key(seed, "geo.climate.wind")`, and one `weighted_index` call. It is the only `Rng` in the stage (`climate.cpp` has no other `Rng` or `substream`). Each `int64` weight is checked non-negative before it is converted to `uint64` (`:158-161`), so the conversion is value-preserving. `weighted_index` (`rng.cpp:145-170`) is fatal on a `uint64` sum overflow and on a zero sum. The named check `wind_weights_positive` (`kinds.cpp:64-73`) runs only after every field has passed its type check (`content.cpp:349`), so its plain `int64` sum is of 8 values in 0..1000, at most 8000. Rejected with exit status 3 and no file written: all-zero weights (`check 'wind_weights_positive' failed …: wind_weights must not all be 0`), a weight of −1, and `smooth_radius_m` = 4001. Seeds 0 and 42 give codes 1 and 6, as spec 5.1 says. Spec 5.1's "[0, 0, 0, 0, 5, 0, 0, 0] gives 4" holds for every one-hot vector on seed 42 (section 4).

**N2. C3 temperature (D1).** `climate.cpp:170-201`. Every operation is a checked `Fx` operation (`fx.cpp`: `from_ratio` and `div` with 128-bit intermediates, `mul_int` via `checked_mul`, `+`/`-` checked). An out-of-range value can therefore only raise a deterministic fatal error, never wrap. With S in [0, 32767] (precondition 3), `lapse_c_per_km` ≤ 15 and `|tn|` ≤ 5, section 7's bounds hold: T ∈ [−85, 45]. `round_int` is RHA on the magnitude (`fx.cpp:76-79`), so negative T rounds symmetrically, as P0-01 says. `clamp_checked` then the cast to `int16` are value-preserving after the clamp to [−600, 500]. The noise is `fbm_m` keyed by the stream and the cell centre (`cell_centre_m` is `int64` arithmetic on an `int32` index): location-keyed, so it does not depend on row-block order. One trivial seam remark: `climate_temperature_cell` computes `2 * height` (`:175`) in plain `int64`. A caller passing a height above 2^62 would overflow. Every grid height is below 2^24.

**N3. C4 exactness, summation order, threads and rounding.**
- *Summation order.* The column pass drops the leaving row before it adds the entering one (`:274-285`). The initial fill (`:262-267`) adds exactly the window of the block's first row, so `acc` never holds more than one window: peak 251² × 32767 × 2^32 = 8 866 331 916 639 404 032 at the stage's largest radius. The spec's section 7 says the implementation "adds the entering row before dropping the leaving one" and holds 252 × 251 rows (8.902 × 10^18). That describes the code before this revision. The code comment (`:225-228`) is right. Spec text for the spec writer: the bound is the window, 8.866 × 10^18. The row pass's whole-row prefix is bounded by N × 1.407 × 10^14 ≤ 2.9 × 10^17 at N = 2048. Both fit with the stage's inputs. For other inputs see S1.
- *Thread independence.* G6 blocks are fixed 64-row blocks (`blocks.hpp:23`), independent of the thread count. The row pass writes only its own rows of `rowsum`. The column pass reads `rowsum` rows outside its block, but only after the row pass's `run_blocks` has joined, and it restarts its accumulator in every block. It writes only its own rows of `hs`. Pass 2 reads `raw`, built sequentially from the returned pass-1 vector (`:312-319`), so pass 1 is complete before any of pass 2 starts.
- *Rounding.* Pass 1 is `Fx::from_ratio(sum, c_w)` (128-bit, RHA). Pass 2 is `div_int(from_raw(sum), c_w)` (RHA). All sums are ≥ 0 since S ≥ 0 is checked at both seams, so neither ever divides a negative value. The `r` clip to max(W, H) (`:234`) gives the same window as the unclipped radius.
- *Checked against an independent reference.* A driver compares `climate_triangle_mean` (and `climate_box_mean` on a quarter of the cases) with a 2-D summed-area table in `__int128`, which has no running sums and no prefix along rows. It covers 3000 random grids (1 to 300 wide, 1 to 200 high, so several row blocks; S uniform, all 32767, 0/32767 checkerboards, or 32700..32767; r_cells 0 to 2 max(W, H) + 4) at 1, 2, 3, 5 and 8 threads, on GCC and Clang release: equal raw for raw everywhere.

**N4. C5 sweep (D4), order and symmetry.** `climate.cpp:324-405`. Single-threaded, no `for_each_row_block`.
- *Order.* Orthogonal winds by lines: columns x in the order of dx with taps in column x − dx (`:385-393`), rows y in the order of dy with taps in row y − dy (`:394-403`). Every tap lies on a line that is already finished. Diagonal winds in raster order (`:374-384`): rows in the order of dy, cells in the order of dx. Tap a = (x − dx, y) is the previous cell of the same row in that order, and b = (x − dx, y − dy) and c = (x, y − dy) lie in the previous row. So every in-map tap is written before it is read, and no cell reads an unwritten 0.
- *Off-map taps and position.* An off-map tap is `{ONE, 0}` (`:350-353`). A cell's value depends only on its position, its three taps, `water`, `hs` and the tuning. The 1-2-1 means use checked `+` and `mul_int` with RHA `div_int` on non-negative values, so a and c enter symmetrically and exactly.
- *Symmetry (invariant 7), own driver.* 300 random grids from 1 × 1 to 24 × 24 (fixture 0 is 7 × 5), random `water` with or without an ocean border, S up to 3000 or 32767, r_cells 0 to 12, shipped or extreme in-bounds tuning (`rain_oro_mm` to 100000, `moisture_path_mm_km` 100 to 2 × 10^6, `loss_max` to 0.9, `ocean_recharge_m` 16 to 100000). For each of the seven non-identity symmetries (the three generators and their compositions, the transpose swapping W and H) and all 8 codes, the driver checks sweep(σG, σk) = σ(sweep(G, k)) for both q and p, and checks that `climate_triangle_mean` commutes with σ. That is 16800 sweep pairs and 2100 C4 pairs, 0 differences on GCC and Clang. Negative control: the same driver with σ(k) replaced by k reports 900 failures on 20 fixtures, so the comparison is live.

**N5. C6, preconditions, widths, streams, database and report.**
- *C6* (`:409-438`). All `Fx` operations are checked. There are two plain `int64` products: `coast_rain_mm × (reach − d)` (`:414`, at most 5000 × 32767 under the schema; reach − d ∈ (0, reach] on that branch) and `1000 × moisture_path_mm_km` (`:338`, at most 2 × 10^9). Both are far inside `int64` with schema tuning. A hand-built `ClimateTuning` would need values above 10^14 to overflow them. The cast to `uint16` follows the clamp to [`rain_min_mm`, 10000] with `rain_min_mm` ≤ 5000.
- *Preconditions* (`:119-148`, `:461-485`). These are checked before any computation, in the spec's order:
  - precondition 1: the four layers in order elevation, water, water_depth, coast_dist; existence, then type, then size; after the N range check;
  - precondition 2: on every cell, in its own loop;
  - precondition 3: per cell in index order (ocean S = 0, else S ≥ 1, then S ≤ 32767). Within precondition 3, the first violating cell decides the message. The spec's "in this order" is about preconditions 1 to 3, so that is acceptable.

  Every message begins `climate: input precondition: `. The stage fails inside `generate`, so the P0-04 removal of the `.partial` applies. I exercised the load-error path (no file, no `.partial`) but not a stage precondition through `generate`: no in-bounds upstream world violates one. That path is the test writer's test 10.1.7.
- *Widths.* No `long`, no `size_t` in a stored value. `size_t` appears only as an index. `int32` grid coordinates are widened to `int64` or `size_t` before arithmetic. `static_cast<Direction>(k)` has k < 8, and the `uint32` casts of n come after the N check. Nothing depends on MSVC's 32-bit `long`. The only `long` seen here is the UBSan spelling of `int64_t` on Linux.
- *No unordered containers, no floating point, no `<cmath>`, `<random>`, `<chrono>` or `<thread>`* in any added line (grep of the diff; lint clean).
- *Streams* (`streams.hpp:56-58, 87-89`). Three names, appended at the end of `all_stream_names`. Keys are `stream_key(seed, name)`, a hash of the name, so the registration order and the removed `test.climate` entry (`3c4e649`) change no key. World hashes at `3c4e649` equal those at `aea0da7` (section 4).
- *Pipeline.* `climate` is the third entry of `implemented_geography_stages` (`stage.cpp`). It writes `temperature` then `rainfall` through `context.write_layer` in the stage's transaction, so the rows are inserted in id order. A `--until hydrology` world of 512 seed 13 has the same layer hashes for every upstream layer as the full world; the full world adds only `temperature` and `rainfall` (and the `file`, `meta` and `layer` table hashes change accordingly).
- *Report and render.* `land_layer_percentiles_of` (`geography.cpp:333-358`) uses an exact 65536-bin histogram with `kth_in_bins` and `percentile_rank`, the project's selection functions. `climate_lines` (`:1090`) formats through `metres1`, `db::format_i64` and `ratio4`, which are integer formatting without locale. `rain_contrast` is n/a when p10 = 0 or there is no land. The palettes (`render.cpp:180-187, 467-485`) are `ramp_unchecked` on integer stops. `report.cpp` reads the two layers by name in layer-id order.

## 2. What was checked

| Rule | Result |
| --- | --- |
| Integer only in the output path | Yes: `Fx` (checked) and `int64` sums. No float, double, `<cmath>`, `<random>`, `std::rand`. |
| Platform math | None; `fbm_m` is the project's noise. |
| Unordered iteration, pointer keys | None. |
| Time, environment, locale | None in the stage. Report numbers are locale-free (N5). |
| Threading (D10) | C3, C4, C6 in G6 row blocks with disjoint writes; C4 pass 2 after pass 1; C5 sequential (N3, N4). Hashes equal at 1/2/3/5/8 threads, TSan clean (section 4). |
| Undefined behaviour | None on stage inputs (UBSan clean at the extremes). Signed overflow possible through the seams outside the stage's bounds (S1). No shifts of negative values in the change. |
| Overflow bounds of section 7 | Hold for the stage, against the code's actual order (N3). The spec's add-first remark is stale (N3). |
| Random stream discipline | One `Rng`, one draw, sequential; two noise keys (N1, N2). |
| Uninitialised data | Every vector is value-initialised (`Fx` default raw 0); every cell is written. ASan clean. |
| Database | Two layers, fixed order, one transaction; no schema change. |
| Build flags | Unchanged (lint `cmake` rules clean). |
| MSVC widths | No `long` or `size_t` assumption (N5). Windows not run here; CI's `compare-hashes` covers it. |

## 3. Lint

`python3 -B tools/lint/determinism_lint.py --self-test`: 13 snippet files, 0 failures. `python3 -B tools/lint/determinism_lint.py`: 115 C++ files, 11 CMake files, 0 findings, at `aea0da7` and at `3c4e649`.

## 4. Runs

Builds out of tree under `~/.cache/the-unmapped/build/p1-03-climate-audit/`: `linux-gcc-release`, `linux-clang-release`, `linux-clang-asan` (presets), and an own ThreadSanitizer build `audit-tsan` (Clang, RelWithDebInfo, `-DUNMAPPED_SANITIZE=thread`). `ctest --preset linux-gcc-release`: 4/4 passed. Drivers (auditor's, not committed): `c4_check.cpp`, `sym_check.cpp`, `resume_check.cpp`, `common.hpp` in the session scratch directory, linked against each build's `libworldgen.a`.

**Thread counts and compilers.** `generate --data game/data` at `--threads` 1, 2, 3, 5 and 8. One file hash (first 16 hex digits) per world, identical across the five thread counts and across GCC and Clang:

| World | Wind code | File SHA-256 |
| --- | --- | --- |
| 512 seed 0 | 1 SE (diagonal) | `b09e7ac2b6259921` |
| 512 seed 13 | 4 W | `c590d6cf05c59e20` |
| 512 seed 42 | 6 N | `1ffcfc8a27284c19` |
| 1024 seed 1 | 3 SW (diagonal) | `9f0ac20c5ee0a34b` |
| 1024 seed 5 | 6 N | `b64e40f38c928659` |

**Every wind through `wind_weights`.** 512 seed 42 with one-hot `wind_weights` (5 at code k), threads 1 and 5, GCC and Clang. Each code drew k, and the four builds agree per code: w0 `3c6a9d8368ea7961`, w1 `c02822bf22b2afc3`, w2 `6ec62b9e0265737a`, w3 `8e5140422b80788b`, w4 `c73615d140e595e1`, w5 `4d36ff0374af39f3`, w6 `2eb29b031cee8d8d`, w7 `2cf2f83f56e764eb`. The w6 hash differs from the shipped 512 seed 42 (also wind 6) only through `content_hash`.

**Sanitizers.**
- TSan, 512 seed 42 at 5 threads: no report, hash `1ffcfc8a27284c19`. With wind 1 (diagonal): no report, `c02822bf22b2afc3`.
- `linux-clang-asan` (ASan and UBSan, `-fno-sanitize-recover=all`): 512 seed 42 at 5 threads gives `1ffcfc8a27284c19`; wind 5 at 3 threads gives `4d36ff0374af39f3`; no report.
- C4 seams at the extreme allowed inputs under ASan/UBSan, all S = 32767, both seams, 5 threads. Every cell came out exactly 32767 dm, and a 32767/32766 pattern equalled the `__int128` reference at 1, 2, 3, 5 and 8 threads:
  - 260 × 255 with r_cells 250 and 251 (h = 125);
  - 4096 × 251 and 4096 × 1 with r_cells 250;
  - 2048 × 2048 with r_cells 250 (the stage's largest case).
- Outside the stage's bounds: overflow reported (S1).

**Exact sums.** `c4_check random 3000` on GCC and Clang: 0 differences from the `__int128` summed-area reference (N3).

**Symmetry.** `sym_check game/data 300` on GCC and Clang: 16800 sweep pairs and 2100 C4 pairs, 0 differences. The negative control fails (N4).

**Resume equality.** `climate_layers`, recomputed at 1 and 4 threads from the stored `elevation`, `water`, `water_depth` and `coast_dist` with the content's tuning, equals the stored `temperature` and `rainfall` byte for byte on the five worlds above and on the eight one-hot-wind worlds. A `--until hydrology` world has the same upstream layer hashes as the full world (N5).

**Follow-up commit.** After fast-forwarding to `3c4e649` and rebuilding `linux-gcc-release`, 512 seeds 0 and 42 give the same hashes as above.

## 5. Lint rule

None added. S1 is a missing precondition, not a construct. The pattern "plain `int64` `+=` on an accumulator" appears 13 times in `worldgen/src/stages` and `worldgen/src/geo`, and most are bounded by construction. A syntactic rule cannot tell a bounded sum from an unbounded one. Making each site carry a suppression would record a claim without checking it: the comment at `climate.cpp:224-228` is exactly such a claim, and true only under its callers' bounds. The `static_assert`s and the entry guard proposed in S1 are the mechanical check that fits.
