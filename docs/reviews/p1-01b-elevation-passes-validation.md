# Validation: P1-01 elevation revision "natural passes and final talus"

Validation specialist, 2026-11-16. Commit validated: `820ddb5` (local `main`, "Merge branch 'p1/01b-elevation-passes-audit'": implementation, independent tests and determinism audit), `GENERATOR_VERSION` 0.6.0, schema version 2, shipped `game/data` with content hash `623da6ff…cd56b3`. Specs on that commit: `docs/specs/p1-01-elevation.md` fourth version (E6.5, E7, E8.7, 9.1), `docs/specs/p1-00-geography-overview.md` (9.3, 10.3, 11, 13.1). Determinism audit: `docs/reviews/p1-01b-elevation-passes-determinism-audit.md` (pass). Machine: 6-core Linux desktop, 31 GB. Builds, tests, sweep and image work ran with several jobs at once (load 5 to 7); the timings of section 6 were taken afterwards on a quiet machine, one world at a time. Windows MSVC is run in CI by the lead.

**Verdict: pass.** Every preset builds and passes `ctest` and the slow suites; the 12 hash cases are identical on the five Linux presets and `p1-worlds.txt` is recorded; threads do not change output; no hard bound of P1-00 13.1 is violated by any of the 108 sweep worlds (`step_p999_m` ≤ 150 m holds at every size; maximum 136.4 m); the land/ocean mask of every world equals the 0.5.0 one; every `pass` row is well formed; no world's highest point drops by more than 7.6 m; elevation time and memory are within budget. Three typical ranges miss at 2048 (8 seeds; two already known), and the passes leave visible marks that the world critic should judge (section 7). No defect report.

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug`, `linux-clang-debug`, `linux-clang-asan` | pass (all five, warnings as errors) |
| `ctest` per preset (4 tests each; asan with `--timeout 3600`) | pass on all five. Under load: gcc-release 146 s, clang-release 104 s, gcc-debug 463 s, clang-debug 519 s, clang-asan 1974 s (`worldgen-tests` 1461 s, `worldgen-cli-tests` 513 s). Quiet, afterwards: gcc-release 91 s, clang-release 91 s (`CLAUDE.md` said about 65 s; updated). The debug and asan times were not re-measured quietly, so `CLAUDE.md` keeps its figures for them |
| `worldgen-tests -ts=elevation-slow --no-skip`, gcc-release and clang-release | pass: 10 test cases, 95,017,942 assertions each (565 s, 509 s under load) |
| `worldgen-tests -ts=hydrology-slow --no-skip`, gcc-release and clang-release | pass: 5 test cases, 3,910 assertions each |
| `worldgen-cli-tests -ts=hydrology-slow --no-skip` (the CLI slow suite), gcc-release and clang-release | pass: 1 test case, 23 assertions each |
| `determinism_lint.py --self-test` and the scan | pass: 13 snippets; 113 C++ and 11 CMake files, 0 findings |
| `tools/ci/world-hashes.sh local` over the five presets | pass: all 12 cases identical (`hash --detail` and `report` text), 537 s |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on every preset |
| Committed `p0-worlds.txt` (re-recorded by the engineer for schema 2, `8c62481`) | confirmed: equals `sha256sum` of each of the eight P0 case worlds on all five presets; `p0_world_check.py check … --detail --report --generator-version 0.6.0 --content-hash 623da6ff…cd56b3` (schema version 2, table `pass` in the schema text) passes for all eight cases on gcc-release and clang-asan worlds |
| Elevation layer against the spec's reference values (P1-01 10.4.1) | the `elevation` layer hash of 1024 seeds 0–9 and 512 seeds 0, 41, 42 in the sweep equals the 13 values listed there |
| Seed sweep (gcc-release, default threads): 512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–7 | 108 of 108 `generate`, `report` and `hash --detail` succeed, empty stderr apart from `--verbose` progress; no hard-bound violation (section 3) |
| Outlines against the 0.5.0 sweep (`~/.cache/the-unmapped/out/p1-02b-validation/`) | pass: sign of `elevation` identical in all 108 worlds (section 2) |
| `pass` rows (113 lowered passes, 6 natural gaps) | pass: every row well formed and consistent with `report` (section 4) |
| Thread independence: 512 seeds 0, 5, 13; 1024 seeds 2, 15; 2048 seed 7 (worlds with 0 to 5 sites, one natural gap); `--threads` 1, 2, 5, 8; gcc-release and clang-release | pass: all 48 files have the file hash of the same world in the sweep |

## 2. Outlines (P1-01 invariant 10)

Layers decoded with a numpy decoder of the P0-02 codec, cross-checked value for value against `p0_world_check.decode_blob` on three layers. For every one of the 108 worlds, `elevation > 0` of the 0.6.0 world equals that of the 0.5.0 world of the same seed and size, cell for cell (0 cells differ), and no cell is 0. The outline rows of 13.1 (`land_fraction` to `land_solidity`, `edge_parallel_run_m`) are identical to 0.5.0 world by world.

Changes inside the land: in worlds without a site only the final talus acts (at 512, up to 3.1% of land cells change; at 1024 0.2%; at 2048 none). Over all worlds a median of 56% (512), 41% (1024) and 28% (2048) of land cells change, mostly by a few decimetres (erosion re-run; P1-01 limitation 16): in four sampled worlds with passes the median change of a changed cell is 0.2 to 0.5 m and 3% to 5% of land changes by more than 5 m (13% in 512 seed 13, where two pass bands cover much of a small range). The largest lowering against 0.5.0 is 766 m (512 seed 23, inside an uncapped pass band, section 4.3); the largest rise is 159 m (2048 seed 0 at (1308, 1013), about 8 km from any col: a 0.5.0 lake that is no longer kept and is filled, limitation 16) and 114 m (512 seed 43 at (432, 192), the foot of the coastal cliff of the north-east range, where the final talus moves material down).

## 3. Statistics against P1-00 13.1

Exact comparison as 13.1 requires: counts and decimetre values exact; `land_fraction`, `land_above_m`, `lake_share_of_land`, `river_share_of_land` as exact fractions of integer counts (the `land_above_m` counts recomputed from the decoded layer); `longest_river_m` against 0.15 × 16 N exactly. No printed `ratio4` value (`largest_landmass_share`, `land_solidity`, `largest_basin_share`) is within 0.00005 of a bound. Counts scaled per 13.1 (`landmasses` hard ≤ 10 / 40 / 160).

### 3.1 Hard bounds: no violation

| Statistic | Hard bound | 512 (50 seeds) | 1024 (50 seeds) | 2048 (8 seeds) |
| --- | --- | --- | --- | --- |
| `land_fraction` | 0.34–0.50 | 0.3992 / 0.4004 / 0.4175 | 0.3989 / 0.4007 / 0.4201 | 0.4000 / 0.4017 / 0.4113 |
| `largest_landmass_share` | ≥ 0.85 | 0.9364 / 1 / 1 | 0.9378 / 0.9977 / 1 | 0.9826 / 0.9983 / 1 |
| `landmasses` | ≤ 10 / 40 / 160 | 1 / 1 / 5 | 1 / 2 / 7 | 1 / 3 / 6 |
| `coast_ratio` | 1.4–6.0 | 1.5593 / 1.9198 / 2.8324 | 1.5574 / 1.9950 / 2.8392 | 1.7370 / 2.0337 / 2.8498 |
| `land_solidity` | 0.45–0.93 | 0.7197 / 0.8270 / 0.9123 | 0.7351 / 0.8232 / 0.9142 | 0.7552 / 0.8262 / 0.8880 |
| `elevation_m` p50 | 40–300 m | 82.5 / 103.0 / 200.1 | 83.1 / 98.0 / 153.9 | 83.0 / 88.5 / 95.0 |
| `elevation_m` p90 | 300–1300 m (2048: 200–1300) | 500.4 / 905.0 / 1056.9 | 405.0 / 852.5 / 1118.4 | 222.8 / 435.2 / 1018.0 |
| `elevation_m` p100 | 1200–2800 m | 1245.2 / 2108.1 / 2257.0 | 1529.6 / 2261.9 / 2584.0 | 2233.9 / 2390.7 / 2455.5 |
| `land_above_m` 1500 | ≤ 0.08 | 0 / 0.0196 / 0.0470 | 0.0001 / 0.0214 / 0.0589 | 0.0065 / 0.0155 / 0.0322 |
| `land_above_m` 500 | 0.06–0.35 | 0.1001 / 0.1986 / 0.3171 | 0.0847 / 0.1729 / 0.2997 | 0.0737 / 0.0901 / 0.2004 |
| 1 − `land_above_m` 50 | 0.10–0.50 | 0.1275 / 0.1948 / 0.2667 | 0.1208 / 0.2009 / 0.2649 | 0.1750 / 0.2166 / 0.2550 |
| `step_p999_m` | ≤ 150 m | 61.9 / 83.4 / 136.4 (s41) | 53.9 / 63.3 / 118.7 (s41) | 52.0 / 56.0 / 61.5 |
| `lakes` | ≤ 25; 1–100; 4–400 | 0 / 2 / 4 | 2 / 7 / 12 | 6 / 11.5 / 15 |
| `lake_share_of_land` | ≤ 0.031 | 0 / 0.0153 / 0.0300 | 0.0113 / 0.0292 / 0.0300 | 0.0298 / 0.0300 / 0.0300 |
| `river_share_of_land` | 0.005–0.05 | 0.0125 / 0.0161 / 0.0196 | 0.0169 / 0.0194 / 0.0225 | 0.0231 / 0.0240 / 0.0256 |
| `major_rivers` | 1–20; 1–40; 4–160 | 2 / 5 / 8 | 9 / 15 / 21 | 32 / 40 / 43 |
| `longest_river_m` ÷ side | ≥ 0.15 | 0.326 / 0.538 / 0.977 | 0.380 / 0.603 / 1.610 | 0.581 / 0.842 / 0.999 |
| `largest_basin_share` | 0.03–0.6 | 0.0841 / 0.1962 / 0.3694 | 0.0896 / 0.1774 / 0.3749 | 0.1093 / 0.1772 / 0.3052 |

(min / median / max; medians of an even count are the mean of the two middle values.) `step_p999_m` at 512 seed 41 is 136.4 m (0.5.0: 183.8 m, the earlier validation's D1), so the 150 m hard bound holds at every size, as P1-01 10.5 states.

### 3.2 Typical bounds

Seeds outside, and the share inside:

| Statistic | Typical | 512 | 1024 | 2048 |
| --- | --- | --- | --- | --- |
| `largest_landmass_share` | ≥ 0.95 | s20 (98%) | s20 (98%) | none |
| `landmasses` | ≤ 4 / 15 / 60 | s6 (98%) | none | none |
| `coast_ratio` | 1.6–3.5 | s18, s47 (96%) | s18 (98%) | none |
| `land_solidity` | 0.70–0.90 | s7, s37, s44, s47 (92%) | s21, s44 (96%) | none |
| p50 | 60–200 m | s2 (98%) | none | none |
| p90 | 400–1100 m (2048: 330–1100) | none | s12 (98%) | **s5 (88%)** |
| p100 | 1300–2400 (512), 1500–2600 m | s10 (98%) | none | none |
| `land_above_m` 1500 | ≤ 0.05 | none | s12 (98%) | none |
| `land_above_m` 500 | 0.08–0.30 | s2, s30, s44 (94%) | none | **s4, s5 (75%)** |
| 1 − `land_above_m` 50 | 0.15–0.40 | s21, s35, s43, s44, s48 (90%) | s21, s44, s48 (94%) | none |
| `lakes` | 0–5 / 1–13 / 7–16 | none | none | **s1: 6 (88%)** |
| `lake_share_of_land` | 0.003–0.03 | s25, s29, s36: no lake (94%) | none | none |
| `river_share_of_land`, `major_rivers`, `longest_river_m`, `largest_basin_share` | | none | none | none |

Three misses, all at 2048 where one seed of eight is 12.5%: p90 and `land_above_m` 500 are P1-01 limitation 12 (2048 seeds 4 and 5, one long range on a broad continent; unchanged in kind from 0.5.0, where the same seeds missed). New: 2048 seed 1 has 6 lakes (0.5.0: 8): a pass (site 3, col (1297, 1697)) erases a 706-cell tarn on its crossing and the lake share cap keeps one lake fewer elsewhere (limitation 14).

### 3.3 Before and after (0.5.0 sweep → 0.6.0 sweep, min / median / max)

| Statistic | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| p50 (m) | 82.5 / 103.0 / 207.4 → 82.5 / 103.0 / 200.1 | 83.1 / 98.0 / 155.8 → 83.1 / 98.0 / 153.9 | 83.0 / 88.5 / 95.1 → 83.0 / 88.5 / 95.0 |
| p90 (m) | 512.2 / 919.8 / 1060.0 → 500.4 / 905.0 / 1056.9 | 405.0 / 877.2 / 1140.3 → 405.0 / 852.5 / 1118.4 | 224.2 / 472.5 / 1025.7 → 222.8 / 435.2 / 1018.0 |
| p100 (m) | 1245.2 / 2109.6 / 2260.1 → 1245.2 / 2108.1 / 2257.0 | 1529.5 / 2261.9 / 2584.1 → 1529.6 / 2261.9 / 2584.0 | 2233.9 / 2390.7 / 2455.5 → same |
| `land_above_m` 500 | 0.1021 / 0.2091 / 0.3170 → 0.1001 / 0.1986 / 0.3171 | 0.0847 / 0.1767 / 0.2997 → 0.0847 / 0.1729 / 0.2997 | 0.0743 / 0.0960 / 0.2012 → 0.0737 / 0.0901 / 0.2004 |
| `land_above_m` 1500 | 0 / 0.0197 / 0.0472 → 0 / 0.0196 / 0.0470 | 0.0001 / 0.0220 / 0.0590 → 0.0001 / 0.0214 / 0.0589 | 0.0065 / 0.0158 / 0.0328 → 0.0065 / 0.0155 / 0.0322 |
| `step_p999_m` | 61.9 / 89.2 / 183.8 → 61.9 / 83.4 / 136.4 | 53.3 / 62.0 / 125.0 → 53.9 / 63.3 / 118.7 | 52.0 / 55.8 / 61.1 → 52.0 / 56.0 / 61.5 |
| `max_step_m` | 71.5 / 100.8 / 298.3 → 70.7 / 89.4 / 199.6 | 66.8 / 80.8 / 251.8 → 66.8 / 80.0 / 183.5 | 74.7 / 82.0 / 89.5 → 73.5 / 80.0 / 80.6 |
| `slope_over` 35 | 0.1920 / 0.3827 / 0.5145 → 0.1920 / 0.3800 / 0.5145 | 0.1347 / 0.2506 / 0.3372 → 0.1347 / 0.2497 / 0.3352 | 0.0731 / 0.1134 / 0.2095 → 0.0731 / 0.1135 / 0.2094 |
| `slope_over` 45 | 0.1542 / 0.3103 / 0.3973 → 0.1542 / 0.3093 / 0.3928 | 0.1013 / 0.1897 / 0.2477 → 0.1013 / 0.1897 / 0.2481 | 0.0514 / 0.0823 / 0.1591 → 0.0514 / 0.0837 / 0.1579 |
| `slope_p50_above_500m` | 1.1789 / 1.9798 / 3.3905 → 1.1789 / 2.0423 / 3.4932 | 0.9478 / 1.4526 / 1.9783 → 0.9478 / 1.4789 / 2.0572 | 1.1962 / 1.2932 / 1.8881 → 1.1962 / 1.3551 / 1.9148 |
| `lowland_flat_share` | 0.0019 / 0.0066 / 0.0142 → 0.0021 / 0.0066 / 0.0143 | 0.0079 / 0.0144 / 0.0236 → 0.0079 / 0.0147 / 0.0236 | 0.0199 / 0.0349 / 0.0390 → 0.0199 / 0.0349 / 0.0391 |
| `lakes` | 0 / 2 / 4 → same | 2 / 7 / 12 → same | 8 / 11.5 / 15 → 6 / 11.5 / 15 |
| `lake_share_of_land` | 0 / 0.0153 / 0.0300 → same | 0.0113 / 0.0295 / 0.0300 → 0.0113 / 0.0292 / 0.0300 | 0.0298 / 0.0299 / 0.0300 → 0.0298 / 0.0300 / 0.0300 |
| `river_share_of_land` | 0.0125 / 0.0161 / 0.0202 → 0.0125 / 0.0161 / 0.0196 | 0.0170 / 0.0197 / 0.0229 → 0.0169 / 0.0194 / 0.0225 | 0.0232 / 0.0242 / 0.0257 → 0.0231 / 0.0240 / 0.0256 |
| `major_rivers` | 2 / 5 / 8 → same | 9 / 14.5 / 21 → 9 / 15 / 21 | 32 / 40 / 43 → same |
| `longest_river_m` ÷ side | 0.329 / 0.530 / 0.977 → 0.326 / 0.538 / 0.977 | 0.363 / 0.596 / 1.615 → 0.380 / 0.603 / 1.610 | 0.578 / 0.842 / 1.013 → 0.581 / 0.842 / 0.999 |
| `largest_basin_share` | 0.0840 / 0.1968 / 0.3783 → 0.0841 / 0.1962 / 0.3694 | 0.0896 / 0.1770 / 0.3762 → 0.0896 / 0.1774 / 0.3749 | 0.1090 / 0.1802 / 0.3052 → 0.1093 / 0.1772 / 0.3052 |
| `river_sources` | 36 / 49.5 / 63 → 38 / 49 / 61 | 177 / 209.5 / 235 → 183 / 209 / 235 | 898 / 928 / 974 → 894 / 929 / 972 |
| `river_mouths` | 15 / 23 / 35 → 15 / 23.5 / 33 | 44 / 59.5 / 84 → 43 / 59.5 / 82 | 145 / 167.5 / 218 → 145 / 166 / 214 |

The passes lower the high ground a little (p90 median −15 / −25 / −37 m, `land_above_m` 500 median −0.010 / −0.004 / −0.006); the final talus cuts the steepest faces (`max_step_m` maxima −99 / −68 / −9 m, `step_p999_m` maxima −47 / −6 m at 512 / 1024). The median slope above 500 m rises a little (the lowered bands are below 500 m, so the remaining high ground is the steeper part). Lake counts change in 1 world at 512, 7 at 1024 and 1 at 2048 (by −2 to +1); the river rows move by at most a few percent. All 13.1 ranges hold except the typical misses of 3.2.

### 3.4 Ranges: proposed final values

13.1 marks the elevation rows provisional and the hydrology rows to be re-measured. Every hard bound holds at every size, so none needs to change. By the rule used for the earlier revisions (a typical range that holds for fewer than 90% of seeds is widened to the measured range plus about 5% of its width at each end, rounded outward, never past a hard bound), with the 2048 sample of 8 seeds:

| Statistic | Hard | Typical now | Proposed typical | Measured at 0.6.0, 512 / 1024 / 2048 |
| --- | --- | --- | --- | --- |
| `elevation_m` p90 | unchanged | 2048: 330–1100 m | **2048: 200–1100 m** (measured 222.8–1018.0; widening reaches the hard 200 m) | 500.4–1056.9 / 405.0–1118.4 / 222.8–1018.0 m |
| `land_above_m` 500 | unchanged | 0.08–0.30 | **2048: 0.067–0.30** (measured 0.0737–0.2004) | 0.100–0.317 / 0.085–0.300 / 0.074–0.200 |
| `lakes` | unchanged | 2048: 7–16 | **2048: 5–16** (measured 6–15) | 0–4 / 2–12 / 6–15 |
| `major_rivers` | unchanged | 2048: 32–43 | **2048: 31–44** (measured 32–43 exactly at the edges; proposed in the 0.5.0 validation, not yet adopted) | 2–8 / 9–21 / 32–43 |

The two 2048 elevation rows are limitation 12 again; an alternative is to keep them and accept the miss with the 8-seed sample, or sweep 2048 seeds 0–19 as 13.1 asks before deciding. Measured columns to update (all rows of 13.1 from `elevation_m` p50 on): as in 3.1 and 3.3, plus `max_step_m` 70.7–199.6 / 66.8–183.5 / 73.5–80.6 m, `slope_over` 45 0.154–0.393 / 0.101–0.248 / 0.051–0.158, `lowland_flat_share` 0.0021–0.0143 / 0.0079–0.0236 / 0.0199–0.0391, `river_sources` 38–61 / 183–235 / 894–972, and the pass rows of section 4. Every elevation row's measured range equals the round-five range already in 13.1 wherever 13.1 gives one (p50 to `step_p999_m`, `slope_over` 35, `slope_p50_above_500m`): the implementation reproduces the exploration's statistics, as its layer hashes do. For the lead's decision; the spec is not changed here.

## 4. Passes

From `report` and from the `pass` table read directly (table rows decoded, path blobs as `u32` little-endian).

### 4.1 Counts

| | 512 (50 worlds) | 1024 (50) | 2048 (8) |
| --- | --- | --- | --- |
| Sites per world (worlds) | 0: 21, 1: 25, 2: 4 | 0: 14, 1: 13, 2: 18, 3: 5 | 0: 1, 2: 2, 3: 3, 4: 1, 5: 1 |
| Lowered passes (`passes`) | 33 | 59 | 21 |
| Natural gaps | 0 | 5 (s0, s2, s8, s46, s47) | 1 (s1) |
| Capped (`passes_capped`) | 21 (64%) | 44 (75%) | 18 (86%) |
| Walkable (`passes_walkable`) | 24 (73%) | 24 (41%) | 8 (38%) |
| Col height after erosion, lowered passes (m) | 168.3 / 321.7 / 1076.9 | 172.5 / 506.9 / 1165.7 | 183.4 / 533.0 / 1070.3 |
| Cols below 300 m / above 800 m | 15 / 4 | 14 / 11 | 3 / 4 |

Worlds without a site: 512 seeds 0–4, 12, 14, 18, 21, 28–30, 32, 34–41 (21); 1024 seeds 1, 3, 4, 19, 20, 25, 29, 30, 32, 35, 36, 38, 39, 40; 2048 seed 4. Several have a long range (1024 s1, s20, s29, s39; P1-01 limitation 13 at 512).

The counts equal round five's (P1-00 13.1: 33 / 59 / 21 passes; 21, 14 and 1 worlds without), with one difference: **1024 has 5 natural gaps, where P1-00 13.1 and P1-01 limitation 17 say 4.** The layers of 1024 seeds 0–9 equal the spec's reference hashes, and gaps do not change the layer, so this is probably a miscount in the spec text (3 of the 5 gaps are in seeds 0–9); for the spec writer to re-check.

Walkability: an independent implementation of the P1-00 10.3 definition (Python, from the decoded layer and path) gives the same count as `report` in all 108 worlds. Against the exploration's measure quoted in 13.1 (open question 9 asks for both once): 512 24 of 33 here against 22 of 33; 1024 24 of 59 against 24 of 59; 2048 8 of 21 against 8 of 21. Every uncapped pass is walkable (12 of 12 at 512, 15 of 15 at 1024, 3 of 3 at 2048), so every non-walkable pass is a capped one; capped cols can be walkable too (9 of 44 at 1024, 5 of 18 at 2048, 12 of 21 at 512, for example 512 s24 with its col at 1036 m before erosion).

### 4.2 Row checks

For all 119 rows: `kind` and `capped` in {0, 1}, `capped` = 0 for every natural gap; `id` = 1, 2, … without gaps, `range_id` ≥ 1 and non-decreasing in `id` order (sites in range order); the col cell is land in the final layer; the path lies on the map (every index < N², consecutive cells 8-adjacent with no row wrap: Chebyshev step 1 in (x, y)), has no repeated cell, contains the col exactly once, and the cell after the col is a 4-neighbour of it (the partner); every path cell, both ends included, is land. `report`'s `passes`, `natural_gaps` and `passes_capped` equal the table's counts in every world. No failure.

### 4.3 Cut at the col

The spec's exact statement is invariant 14 (P1-01 6.1): for every lowered pass, max over the crossing of m'(c, ONE − depth) ≥ M − `pass_max_cut_m`, with M the largest natural mountain part on the crossing, before the floor drainage, overlapping passes and erosion. It bounds the crossing's highest point, not the col, and uses pre-erosion values that are not stored, so the table cannot check it; `stages_elevation_passes_test.cpp` (lines 901–912) checks it on the stage's own values, and passes. Section 6.5 adds what follows for the col: where lo ≥ `pass_texture_keep` every crossing cell is cut by about (1 − lo) × its mountain part ≤ the cap; only a crossing with M ≤ cap / (1 − keep) = 667 m can have its col cut by more; the drainage adjustment and erosion come on top. Its measurement: cut at the col after erosion, against 0.5.0, p50 498 / p90 501 / max 503 m at 1024.

Measured (decimetres converted to metres):

| | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| 0.5.0 layer minus 0.6.0 layer at the col, all lowered passes (p50 / p90 / max) | 493.5 / 500.6 / 502.9 | 497.5 / 500.7 / 502.8 | 496.3 / 500.1 / 504.2 |
| `saddle_dm` − `col_dm` (cut before erosion), capped (p50 / p90 / max) | 500.9 / 502.6 / 504.1 | 500.2 / 503.1 / 511.0 | 500.7 / 502.8 / 508.7 |
| the same, uncapped (max) | 479.5 | 476.9 | 485.2 |
| `saddle_dm` − final layer at the col, all (max) | 511.1 | 517.6 | 511.3 |

The 1024 row reproduces the spec's measurement. The excess over 500 m of `saddle_dm` − `col_dm` (at most 11 m) is expected: `saddle_dm` is the pair height max(K(col), K(partner)), not the col's own height, and the col takes the floor drainage's cut (Δ < 0, E6.5.8) on top. Every uncapped pass is cut by less than the cap. No finding.

### 4.4 Summits

No world's highest point drops by more than 50 m (P1-01 6.5, the spec's measured statement): the largest drop is 7.6 m (512, range −0.2 to 7.6 m), 1024 −0.3 to 0.2 m, 2048 0 in all eight.

Peaks near cols. The spec's measured statement "no 0.5.0 cell within 1 km of a col is lowered by more than 300 m, none within 3 km by more than 200 m" cannot mean every cell (the col itself is lowered by about 500 m); I read it as peaks and measured two definitions of a 0.5.0 peak (a land cell that is the maximum of the 0.5.0 layer in a square window of radius 62 cells, about 1 km, or 16 cells, 256 m):

| Largest lowering of a 0.5.0 peak (m) | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| 1 km peaks within 1 km of a col | **431.7** (s23 pass 2, col (311, 107)) | 72.9 (s15 pass 2) | 218.7 |
| 1 km peaks within 3 km | 431.7 | 72.9 | 218.7 |
| 256 m peaks within 1 km | 634.2 (s15 pass 1) | 471.3 | 466.1 |

In every world the largest lowering of a 1 km peak within 3 km is the one within 1 km, so no 1 km peak between 1 and 3 km from a col loses more than the figures above, and at 2048 one loses 218.7 m within 1 km (over the 200 m of the 3 km clause if that clause includes the inner kilometre). At 1024 the statement holds; at 512 seed 23 a 1 km peak next to an uncapped low gate loses 432 m (the range is cut through to 190 m, section 7). The 256 m peaks are minor tops inside the lowered bands, cut as designed (the cap applies to the crossing's highest point only). Descriptive (open question 10 leaves summits to rules); for the critic.

### 4.5 Lakes and rivers at passes

Lake cells within 19 cells of a lowered crossing, 0.5.0 → 0.6.0: tarns erased at 1024 s11 pass 2 (900 → 0), s21 pass 1 (1658 → 500), s22 pass 2 (320 → 0), s44 (487 → 0), s48 passes 1 and 2 (442 → 0, 441 → 67), 2048 s1 pass 3 (706 → 0): six tarns erased or mostly erased at 1024 and one at 2048, in line with limitation 14 and 10.5 (8 at 1024 and 1 at 2048 by the exploration's own measure, lakes over 100 cells keeping less than half their cells). Lakes on the crossing itself in 0.6.0 (path cells; 0.5.0 in brackets): 512 s19 34 (23), 512 s49 14 (8), 1024 s9 12 (4), 1024 s13 pass 2 4 (4); these crossings already ran past a lake in 0.5.0, and the lake now reaches further along them. New lake cells in a band where 0.5.0 had none: 1024 s23 pass 2 (35 cells, at the band's edge). The two lowland ends of every lowered crossing drain to different river mouths in every world; the col and its partner drain to the same mouth in 49 of 113 passes, against 46 of 113 at the same cells in 0.5.0 (the divide of the support is not a sea-to-sea divide), so the passes did not join drainage systems.

## 5. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` re-recorded for 0.6.0 after the five-preset comparison passed. The legitimate reasons for the change: generator 0.6.0 adds the pass layout (E6.5, E7) and the final talus (E8.7), which change every geography case's `elevation` layer and with it every hydrology layer, and schema version 2 adds table `pass` (schema text and table hashes change). The `elevation` layer hashes of seeds 0 and 42 at 512 and seed 1 at 1024 equal the spec's reference values (P1-01 10.4.1, marked below; the phrase case is not listed there).

| Case | File hash (0.6.0) | `elevation` layer |
| --- | --- | --- |
| `geo-phrase-512` | `c0f4df72bc8e80b3bb1c5cf71468a9729bbd62acd0bf89b0189aeb8659259c99` | `d763561a…cec7a34c` |
| `geo-seed-0-512` | `e07cbef819beb01b5ecdc47518cf3ea72fe7625247bf6611daab9d53bb1fb0bf` | `34f5b1dd…c821d7f2a` (10.4.1) |
| `geo-seed-1-1024` | `830a7943df18b90479bf84d0a55d7f81ced17b523a99a53d3c7b866d51f6105b` | `2e3b7dee…59ef16eec4` (10.4.1) |
| `geo-seed-42-512` | `3f299d2eac3bdb695f58f292628a76c255a2740a58d49b4d8bacbecbdb3069c5` | `590569e0…e31a3b91` (10.4.1) |

`geo-seed-0-512` equals the sweep's 512 seed 0 file hash. `p0-worlds.txt` is unchanged (the engineer's schema-2 values, confirmed in section 1). The MSVC presets are compared by CI's `compare-hashes`.

## 6. Performance

Quiet machine (nothing else of mine running; a desktop session idle), gcc-release, default threads (6), one world at a time, `/usr/bin/time` wall time and peak RSS, three repeats (two at 2048). "Elevation" is `generate --until elevation` (the stage plus loading the data and writing the file, so an upper value for the stage); `--verbose` prints only the total, not per-stage times.

| Size, seeds | Elevation, wall (min–max) | Budget | Full `generate` (elevation + hydrology) | Budget, geography | Peak RSS, elevation / full | Memory budget, stage (1.3 × 0.5.0) / geography |
| --- | --- | --- | --- | --- | --- | --- |
| 512: 0, 5, 13, 41 | 1.97–2.14 s | 2.2 s | 2.02–2.16 s | 4 s | 28.8–30.7 / 28.8–30.7 MB | 40 / 200 MB |
| 1024: 0, 3, 15, 41 | 6.90–7.38 s | 8 s | 7.15–7.49 s | 15 s | 94.1–96.9 / 94.5–96.8 MB | 132 / 600 MB |
| 2048: 0, 7 | 27.4–28.4 s | 35 s | 28.6–29.5 s | 65 s | 357–385 / 358–385 MB | 490 MB / 2 GB |

All within budget. The 512 elevation time is within 3% of its 2.2 s budget on seed 13 (two passes): 2.08–2.14 s; seeds without a site take 1.97–2.01 s. Hydrology takes about 0.05 / 0.25 / 1.2 s (difference of the medians). Peak RSS is below 0.5.0's own (30 / 101 / 376 MB) except at 2048 seed 7 (five passes, 385 MB), well inside the stage's hard budget. The sweep's own times (taken under load, 6 threads): 512 2.05–7.80 s, 1024 7.35–9.86 s, 2048 33.7–38.6 s; peak RSS 29.8 / 94.6 / 375.7 MB. One thread (thread test, under load): 512 5.0–7.9 s, 1024 19–34 s, 2048 75–87 s.

## 7. Images and what they show

Composite maps of every sweep world: `~/.cache/the-unmapped/out/p1-01b-validation/maps/`; contact sheets with the cols marked from the `pass` table (red ring: capped lowered pass, yellow: uncapped, cyan: natural gap): `…/sheets/` (1024 seeds 0–24 and 25–49 at 200 px, 512 seeds 0–49 at 128 px, 2048 seeds 0–7 at 320 px); 17 before/after crops at 2× (one overview) and the four-pass montage (0.5.0 left, 0.6.0 right, cols ringed, crossing dotted every fourth cell): `…/crops/`.

- **Sheet scale.** The rings sit on the ranges' crests, spaced along them; at 200 px the lowered bands are not visible except as a slight colour break on a few ranges. Several worlds with long ranges have none (1024 s1, s20, s29, s39; at 512 most of the 21). At 2048 every world but s4 has two to five, and s3 has three cols within a third of one range.
- **Low gates** (uncapped, 1024 s8, 512 s23): a green notch through the range, rivers running up both sides; the band keeps some texture (`pass_texture_keep`) as dark blotches. At 512 s23 pass 2 the range is cut through to 190 m in a gap roughly 1 km wide (estimated from the crop): it reads as a broad valley severing the range rather than a pass; the 432 m lowering of a nearby peak (4.4) is here.
- **High capped cols** (1024 s13, col 1174 m): a smooth, untextured, paler notch in the snowline roughly 0.5 km wide; plausible as a saddle, but the cut surface is smoother than the mountainside around it ("pale smooth band").
- **Flat-edged pad** (512 s15 pass 1, uncapped, cut 477 m): the lowered floor is a flat green shelf with a straight east edge across the ridge, the most artificial-looking pass in the sweep.
- **Tarns erased** (1024 s11 pass 2, s44, s48, 2048 s1): the two lakes of 1024 s11 are replaced by a smooth tan slope; the erased tarns are the sites limitation 14 accepts, but the replacement surface is visibly smoother than its surroundings.
- **Lake on a pass** (512 s19, 1024 s9): a lake lies on the crossing; at 512 s19 the lowered band forms a broad trough across the whole range holding a lake on its west half.
- **Range end** (1024 s14): the col sits where the high snow range gives way to lower hills to the north-east; a traveller could perhaps walk round (limitation 15).
- **Rivers beside the col** (1024 s23 pass 2, 2048 s7 pass 1): a river runs along the lowered band a few cells from the col (no river cell on the crossing within 3 cells of the col), so at map scale the pass reads as a through valley.
- **Away from passes**: the final talus leaves smoothed, paler cliff faces above the sea (512 s43 north-east cape, cliff foot raised by up to 114 m), and a filled 0.5.0 lake shows as a smooth pad at 2048 s0 (1308, 1013).

In the crops looked at, nothing reads as a trench (a narrow cut with walls), and no world's highest summit is cut (4.4); minor tops inside the bands are.

Copied into the repository (565 KB in all):

- `images/p1-01b/p1-01b-1024-seeds-0-24.jpg`: contact sheet, 1024 seeds 0–24 at 200 px, cols ringed.
- `images/p1-01b/p1-01b-2048-seeds-0-7.jpg`: contact sheet, 2048 seeds 0–7 at 320 px, cols ringed.
- `images/p1-01b/p1-01b-pass-crops.jpg`: four passes at 2×, 0.5.0 left and 0.6.0 right: 1024 s8 low gate (walkable), 1024 s13 capped high col, 1024 s11 capped col erasing a tarn, 512 s15 flat-edged pad.

## 8. Defects and findings

No defect: no hard-bound violation, no cross-preset or thread-count difference, no test failure, no malformed row, no outline change.

For the lead and the spec writer:

1. Typical ranges at 2048 (3.4): p90, `land_above_m` 500 and `lakes` miss at 2048 with 8 seeds; proposed widenings, or a 20-seed sweep at 2048 before deciding.
2. P1-00 13.1 and P1-01 limitation 17 count 4 natural gaps at 1024; the implementation, whose layers match the reference values, has 5 (4.1).
3. P1-01 6.5's "no 0.5.0 cell within 1 km of a col is lowered by more than 300 m" should say what a peak is; with a 1 km peak it holds at 1024 and 2048 and fails at 512 seed 23 (432 m), 4.4.

For the world critic: 512 s23 (311, 107) and 512 s15 (170, 144) (uncapped cuts that sever the range or leave a flat-edged pad); 1024 s13 (597, 179) (smooth pale notch in a snow range); 1024 s11 (545, 359) and 2048 s1 (1297, 1697) (tarns erased); 512 s19 (360, 228) (lake trough across a range); 1024 s14 (725, 430) (col at a range end); 1024 s1, s20, s29, s39 (long ranges without a pass); 512 s43 north-east cape (talus-smoothed sea cliff).
