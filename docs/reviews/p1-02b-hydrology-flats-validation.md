# Validation addendum: P1-02 hydrology follow-up (flat tie-break, composite rivers)

Validation specialist, 2026-11-16. Addendum to `docs/reviews/p1-02-hydrology-validation.md`; sections not repeated here are unchanged. Commit validated: `b963c3c` (local `main`, "Merge branch 'p1/02b-hydrology-audit'"; on origin as `p1/02b-hydrology-final`), `GENERATOR_VERSION` 0.5.0: the H2 tie-break on flats (spec `docs/specs/p1-02-hydrology.md` H2, invariant 11) and the new river drawing of `render --composite map` (P1-00 10.2 step 4). Determinism audit passed (`docs/reviews/p1-02b-hydrology-flats-determinism-audit.md`; its S1 is closed by this change). Machine: 6-core Linux desktop, 31 GB. **The machine was shared with other work throughout** (load average 5 to 13 on 6 cores), so every wall time below is high; section 5 says how hydrology's own cost was separated. Windows MSVC is run in CI by the lead.

**Verdict: pass.** Builds, tests (including the slow suites), cross-preset identity, thread independence and golden hashes pass. Invariant 11 holds on all 108 sweep worlds: `elevation`, `water_depth`, `lake` and `coast_dist` are byte-identical to the 0.4.0 sweep, and so are the lake and ocean cells of `water`. No hard bound of P1-00 13.1 is violated. The river rows move by at most a few percent; one range change is proposed (`major_rivers` typical at 2048). Rivers are now readable on contact sheets.

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug`, `linux-clang-release`, `linux-clang-asan` | pass (all five) |
| `ctest` per preset (4 tests each) | pass on all five: gcc-release 117 s, clang-release 142 s, gcc-debug 375 s, clang-debug 363 s, clang-asan 2252 s (under load, see above; not comparable with the 0.4.0 times) |
| `worldgen-tests -ts=elevation-slow --no-skip`, gcc-release and clang-release | pass: 7 test cases, 41,949,281 assertions each |
| `worldgen-tests -ts=hydrology-slow --no-skip`, gcc-release and clang-release | pass: 5 test cases, 2,685 assertions each (2,189 at 0.4.0; the test-writer's tie-break tests) |
| `worldgen-cli-tests -ts=hydrology-slow --no-skip`, gcc-release and clang-release | pass: 1 test case, 23 assertions each |
| `determinism_lint.py --self-test` and the scan | pass: 13 snippets; 112 C++ and 11 CMake files, 0 findings |
| `tools/ci/world-hashes.sh local` over the five presets | pass: all 12 cases identical (`hash --detail` and `report` text), 845 s |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on every preset |
| Committed `p0-worlds.txt` (re-recorded by the engineer for 0.5.0, since the content hash changed) | confirmed: equals `sha256sum` of each of the eight P0 case worlds on all five presets |
| Seed sweep (gcc-release, default threads): 512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–7 | 108 of 108 `generate`, `report` and `hash --detail` succeed, empty stderr; no hard-bound violation (section 3) |
| Layer identity against the 0.4.0 sweep (`~/.cache/the-unmapped/out/p1-02-validation/`) | pass, section 2 |
| Thread independence: 512 seeds 0, 13, 42; 1024 seeds 3, 17; 2048 seed 5; `--threads` 1, 2, 5, 8; gcc-release and clang-release | pass: all 48 files have the file hash of the same world in the sweep (default threads). One thread at 2048: 76 s (gcc), 79 s (clang) |

## 2. Layer identity against 0.4.0 (invariant 11)

Per world, the `hash --detail` layer hashes of the 0.5.0 sweep compared with the same world of the 0.4.0 sweep, all 108 worlds present in both:

| Size | Worlds | Identical: `elevation`, `water_depth`, `lake`, `coast_dist` | Differ: `flow_dir`, `flow_accum`, `river`, `water` |
| --- | --- | --- | --- |
| 512 | 50 | 50 of 50, every layer | 50 of 50, every layer |
| 1024 | 50 | 50 of 50 | 50 of 50 |
| 2048 | 8 | 8 of 8 | 8 of 8 |

Every world's file differs (the content hash changed with the new `flat_noise` record). Decoding `water` (numpy, with `p0_world_check.decode_blob`): in all 108 worlds the cells with `water` 2 or 3 are the same cells with the same values; only 0 ↔ 1 trades occur, as invariant 11 allows. Cells traded per world: 512 2 / 58 / 208 (min / median / max), 1024 130 to 1469 (median about 400; most in 1024 seed 13, its flat ring-range basin), 2048 3449 to 5919. The trades are nearly balanced (for example 1024 seed 13: 748 cells became river, 721 stopped being river).

## 3. Statistics against P1-00 13.1

Comparison exact as 13.1 requires (counts exact; `land_fraction`, `lake_share_of_land` and `river_share_of_land` as exact fractions of the printed counts; no printed `ratio4` value within 0.00005 of a bound; `longest_river_m` against N × 16 m exactly).

**Hard bounds: no violation in any row**, elevation or hydrology. The elevation rows are unchanged from 0.4.0 (identical layers). `lakes` and `lake_share_of_land` are unchanged world by world (invariant 11). `largest_basin_share` changes in 29 of 108 worlds (7 at 512, 19 at 1024, 3 at 2048), mostly in the fourth decimal (flat cells on a divide change basin). Two worlds change visibly, because a flat that drained to two outlets now drains to one: 1024 seed 7 0.2151 → 0.3202 and 512 seed 18 0.1317 → 0.1552; then 1024 seed 45 0.2517 → 0.2582 and 512 seed 6 0.2793 → 0.2850. All stay inside the typical range.

### 3.1 River rows before and after the tie-break (min / median / max)

| Statistic | 512, 0.4.0 → 0.5.0 | 1024, 0.4.0 → 0.5.0 | 2048, 0.4.0 → 0.5.0 |
| --- | --- | --- | --- |
| `river_share_of_land` | 0.0125 / 0.0160 / 0.0203 → 0.0125 / 0.0161 / 0.0202 | 0.0170 / 0.0197 / 0.0228 → 0.0170 / 0.0197 / 0.0229 | 0.0231 / 0.0240 / 0.0254 → 0.0232 / 0.0242 / 0.0257 |
| `major_rivers` | 2 / 5 / 8 → 2 / 5 / 8 | 9 / 15 / 21 → 9 / 14.5 / 21 | 33 / 39.5 / 42 → **32** / 40 / **43** |
| `longest_river_m` ÷ side | 0.329 / 0.524 / 0.976 → 0.329 / 0.530 / 0.977 | 0.359 / 0.587 / 1.558 → 0.363 / 0.596 / 1.615 | 0.571 / 0.824 / 0.982 → 0.578 / 0.842 / 1.013 |
| `largest_basin_share` | 0.0840 / 0.1968 / 0.3783 → same | 0.0896 / 0.1765 / 0.3762 → 0.0896 / 0.1770 / 0.3762 | 0.1090 / 0.1802 / 0.3052 → same |
| `river_sources` (descriptive) | 37 / 50 / 62 → 36 / 49.5 / 63 | 178 / 212 / 235 → 177 / 209.5 / 235 | 891 / 931 / 971 → 898 / 928 / 974 |
| `river_mouths` (descriptive) | 15 / 23 / 35 → same | 44 / 59.5 / 83 → 44 / 59.5 / 84 | 151 / 167.5 / 219 → 145 / 167.5 / 218 |

Per world (0.5.0 minus 0.4.0): `major_rivers` changes by −1 to +1 at 512 and 1024 and −1 to +3 at 2048 (median 0); `river_cells` by −22 to +25 at 512, −35 to +85 at 1024 and +155 to +475 at 2048; `longest_river_m` by −23 to +830 m, −152 to +930 m and +171 to +1035 m (medians +8, +58, +492 m). Rivers on flats now meander instead of running in straight staircases, so they are a little longer; the longest river of 2048 seed 6 (33,198 m) now exceeds the map side (1.013 × side; P1-02 limitation 5 allows it).

### 3.2 Typical bounds

Every typical range holds for at least 90% of seeds at every size. Seeds outside: `lakes` none; `lake_share_of_land` at 512 seeds 25, 29, 36 (no lake, unchanged, 94% inside); `river_share_of_land`, `major_rivers`, `longest_river_m`, `largest_basin_share` none. `major_rivers` at 2048 now spans exactly its typical range 32–43 (seed minima and maxima on the bounds).

### 3.3 Ranges: re-confirmed, one change proposed

| Statistic | Hard | Typical | Measured at 0.5.0, 512 / 1024 / 2048 | Status |
| --- | --- | --- | --- | --- |
| `lakes` | 512: ≤ 25; 1024: 1–100; 2048: 4–400 | 512: 0–5; 1024: 1–13; 2048: 7–16 | 0–4 / 2–12 / 8–15 | unchanged by construction; confirmed |
| `lake_share_of_land` | ≤ 0.031 | 0.003–0.03 | 0–0.0300 / 0.0113–0.0300 / 0.0298–0.0300 | unchanged; confirmed |
| `river_share_of_land` | 0.005–0.05 | 0.011–0.027 | 0.0125–0.0202 / 0.0170–0.0229 / 0.0232–0.0257 | confirmed; measured column to update |
| `major_rivers` | 512: 1–20; 1024: 1–40; 2048: 4–160 | 512: 1–9; 1024: 8–22; **2048: 32–43 → 31–44 proposed** | 2–8 / 9–21 / 32–43 | hard confirmed. At 2048 the measured spread now equals the typical range; by the 13.1 widening rule (about 5% of the width at each end, rounded outward) it should be 31–44 |
| `longest_river_m` | ≥ 0.15 × side | ≥ 0.27 × side | 0.33–0.98 / 0.36–1.62 / 0.58–1.01 × side | confirmed; measured column to update |
| `largest_basin_share` | 0.03–0.6 | 0.07–0.40 | 0.084–0.378 / 0.090–0.376 / 0.109–0.305 | confirmed, measured unchanged |
| `river_sources` (descriptive) | — | — | 36–63 / 177–235 / 898–974 | measured column to update |

For the lead's decision; the spec is not changed here.

## 4. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` re-recorded for 0.5.0 after the five-preset comparison passed. The legitimate reasons for the change: the shipped tuning gained the `flat_noise` record (new content hash) and H2 now breaks ties on flats, which moves river cells (section 2). The `elevation` layer of each case is the 0.4.0 one.

| Case | File hash (0.5.0) | `elevation` layer (unchanged) | `water` layer (0.5.0) |
| --- | --- | --- | --- |
| `geo-phrase-512` | `62b2376090d8566ec006c8ffcd27534b5493666f9858bcd56327cf112b202cae` | `c3d1c354…b8aa03` | `71ff1b4b…a6f3ef` |
| `geo-seed-0-512` | `0b76b30c34fde05ff5d6c9ee9dbc2d63802a9d673982cb78beb398f880ca8d73` | `b1e02da1…88b64c` | `2949219f…8e314f7` |
| `geo-seed-1-1024` | `2076a1de63116cdd3f94e6948c757ee72255f2c20c77b3699c457b187dbe70d7` | `0cc4f2c1…6d31d2` | `11d31f77…0f383c672` |
| `geo-seed-42-512` | `517a2ab152e0e10d7a92fcb59e964bc6571392ce33976c06182fb0dc58e09a23` | `cff0cce9…dd33757` | `910da366…a37fbef6` |

The file hashes of `geo-seed-0-512`, `geo-seed-42-512` and `geo-seed-1-1024` equal the 0.5.0 hashes of the audit (section 4 there), and `geo-seed-0-512` equals the sweep's 512 seed 0. The MSVC presets are compared by CI's `compare-hashes`.

## 5. Performance

Sweep, gcc-release, default threads (6), `generate` wall time; measured under load, so these are upper values: 512 2.00 / 2.27 / 4.47 s (min / median / max), 1024 6.72 / 7.96 / 11.85 s, 2048 30.3 / 33.2 / 39.8 s; peak RSS 30.8 MB, 101.9 MB and 376 MB, the same as 0.4.0. The 512 maximum is over the 4 s budget only because of the load: re-run three times each on a quieter moment, 512 seeds 5, 6 and 45 (the three slowest) take 1.97 to 2.05 s, and 1024 seeds 3 and 6 take 7.5 s. Every 2048 time is within its 65 s budget and every peak RSS within its budget.

Hydrology's own cost, separated from the load: 0.5.0 and the 0.4.0 build (`main` `0b68064` with its own `game/data`; it reproduces the 0.4.0 golden of `geo-seed-0-512`) run interleaved, full `generate` and `--until elevation`, 3 repeats each on 512 and 1024 seeds 0–2 and 2048 seeds 0–1, comparing medians of CPU time (user + system), which the load disturbs less than wall time:

| Size | Hydrology CPU, 0.4.0 | Hydrology CPU, 0.5.0 | Budget (wall) |
| --- | --- | --- | --- |
| 512 | about 0 s | about 0.1 s | 0.3 s |
| 1024 | about 0 s | about 0.35 s | 1.2 s |
| 2048 | about 0.6 s | about 1.9 s | 5 s |

Noise is about ±0.2 s at 2048. The comparison-heap flood with tie keys and the `flat_tie` noise add about 1.3 s of CPU at 2048. The flood is sequential, so its wall cost is about its CPU cost: under 2 s against the 5 s budget. Peak RSS of the full run equals `--until elevation` at every size, as at 0.4.0.

## 6. The new river drawing

Composite maps of every sweep world: `~/.cache/the-unmapped/out/p1-02b-validation/maps/`; contact sheets: `…/sheets/` (1024 seeds 0–24 and 25–49 at 200 px per map, 512 seeds 0–49 at 128 px, 2048 seeds 0–7 at 320 px); before/after crops (0.4.0 world with the old drawing next to the 0.5.0 world with the new one, from the 0.4.0 sweep's maps) and river mouths at 6× in `…/crops/`.

- **Readable at sheet scale.** On the 1024 sheet at 200 px (a fifth of full size) the river network is now visible on every world as thin dark lines: trunks reaching the sea, the branching on the lowland, the drainage out of ring basins (seed 13). On the 0.4.0 sheet of the same worlds only lakes were visible. At 2048 (320 px, a sixth) the trunks and many tributaries read clearly. At 512 (128 px) only the larger rivers show; that sheet is small anyway.
- **Flats.** Where 0.4.0 drew long orthogonal staircases on flat ground, 0.5.0 rivers wander with curves (1024 seed 13 basin interior, 1024 seed 49, 2048 seed 2). The look is more natural. Some meanders are tight: hairpins and small pigtail loops where a river turns back on itself within a few cells (2048 seed 2, two loops in a 256-cell window; 1024 seed 13). At full resolution the two arms of a hairpin can touch and read as a short blob of river colour. Worth the critic's eye; the runtime smoothing (P1-02 9.2) will treat them.
- **Thickness.** Class ≥ 4 rivers are two cells wide and darker; the step from thin to thick at the class-4 boundary is visible and helps reading the hierarchy. No thickening over the sea or lakes was seen: the drawing never paints a lake or ocean cell, so a mouth ends cleanly at the shore (1024 seed 13, mouths of class 7 and 9). In a few places a thin first-order river runs next to a thick trunk for a few cells and the two read as one wider ribbon.
- **Corners and breaks.** Diagonal runs are continuous (the corner cell closes the gap); no dotted diagonals and no broken lines were seen in the crops. Confluences draw as a clean join, no blob, apart from the hairpin case above.
- **Concentric loops.** In 1024 seed 32 (x 145–337, y 116–308) rivers run in nested loops round a low hill in a basin, before and after the change; this is the elevation shape (the kept depression noted for 2048 seed 3 in the earlier validation), not the drawing.

Copied into the repository (432 KB in all):

- `images/p1-02b/p1-02b-1024-seeds-0-24.jpg`: contact sheet, 1024 seeds 0–24 with the new drawing (compare `images/p1-02/p1-02-1024-seeds-0-24.jpg`).
- `images/p1-02b/p1-02b-2048-seeds-0-7.jpg`: contact sheet, 2048 seeds 0–7.
- `images/p1-02b/p1-02b-1024-seed-13-before-after.jpg`: 1024 seed 13, cells x 472–663, y 448–639 at 3×: left 0.4.0 with the old drawing, right 0.5.0 with the new one; staircases replaced by meanders, a hairpin visible.

## 7. Defects

None. No hard-bound violation, no cross-preset or thread-count difference, no test failure. Proposed: `major_rivers` typical at 2048 widened to 31–44 and the measured columns of the river rows updated (section 3.3).
