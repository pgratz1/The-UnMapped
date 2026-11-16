# Validation: P1-01 elevation

Validation specialist, 2026-11-16. Commit validated: `9da7a09` (branch `p1/01-elevation-int`: the elevation stage, its independent tests and the determinism audit fixes), `GENERATOR_VERSION` 0.3.0, shipped `game/data` with content hash `93b32f51…eb7ad`. Specs: `docs/specs/p1-01-elevation.md`, `docs/specs/p1-00-geography-overview.md` (13.1 as revised for v16, 14 X5), P0-04 section 9 with amendment 16. Machine: 6-core Linux desktop, 31 GB. Windows MSVC is run separately in CI by the lead.

**Verdict: builds, tests, cross-preset identity, thread independence, performance and golden hashes pass. The seed sweep finds two hard-bound violations (P1-00 13.1): 512 seed 41 `step_p999_m` 183.8 m (> 150 m) and 2048 seed 5 `elevation_m` p90 224.2 m (< 300 m).** Both are defect reports below; neither is a determinism problem.

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug`, `linux-clang-release`, `linux-clang-asan` | pass (all five) |
| `ctest` per preset (4 tests each) | pass: gcc-release 47 s, clang-release 49 s, gcc-debug 171 s, clang-debug 186 s, clang-asan 761 s (`worldgen-tests` 540 s) |
| `worldgen-tests -ts=elevation-slow --no-skip`, gcc-release and clang-release | pass: 7 test cases, 41,949,281 assertions each (174 s, 182 s) |
| `determinism_lint.py --self-test` and the scan | pass: 13 snippets; 110 C++ and 11 CMake files, 0 findings |
| `tools/ci/world-hashes.sh local` over the five presets | pass: all 12 cases identical (`hash --detail` and `report` text) |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on every preset, no "no golden file" note |
| Committed `p0-worlds.txt` | confirmed: equals `sha256sum` of each P0 case world on all five presets; `p0_world_check.py check … --detail --report --generator-version 0.3.0 --content-hash 93b32f51…` passes for all eight cases (gcc-release worlds) |
| Seed sweep (gcc-release, default threads): 512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–7 | 108 of 108 generate, `report` and `hash` succeed; two hard-bound violations (section 3) |
| Thread independence: 512 seeds 0, 13, 42; 1024 seeds 3, 17; 2048 seed 5; `--threads` 1, 2, 5, 8; gcc-release and clang-release | pass: all 48 files byte-identical to the sweep's (default threads) |

## 2. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` (SHA-256 of the world file; lines `<case> <hex>` sorted by case name, LF; P0-04 9, P1-00 14 X5), identical on all five presets:

| Case | File hash | `elevation` layer hash |
| --- | --- | --- |
| `geo-phrase-512` | `b50b96f57a3b3d856b5bf0bb04e6ac7352c440247f6144cb04589870a76cb906` | `c3d1c354a36eecfc4c6a9758704dd960fac0fda927c6cd27ad19bfdb45b8aa03` |
| `geo-seed-0-512` | `275c2708fd82d9fc1612cd781d7432d4ff0f5418c2ea3121018242cabf3443e2` | `b1e02da163b1e0c20261a0acab59532f76acdfbc2f81020e4d8dd599a588b64c` |
| `geo-seed-1-1024` | `b6bd47187b2f6972dc1d4b6f38507a7e2fb501672ce7cd16785ae81749293d9e` | `0cc4f2c15156aa89424cee960d9e434e8fe3a018b855d6a4a2e703cf314a31d2` |
| `geo-seed-42-512` | `ef21d468a9c87aab492db8b3426a1c56da89a2abedd6d17f521dc634275995d0` | `cff0cce9bc268713621744ed378744897f4f030edf891b3bcfe822936dd33757` |

The `geo-seed-1-1024` layer hash equals the exploration build's reference value for seed 1 in P1-01 10.4, an independent confirmation. `p0-worlds.txt` is unchanged (the engineer's values confirmed). The MSVC presets have not been compared here; the CI `compare-hashes` job does that.

## 3. Statistics against P1-00 13.1

Comparison is exact as 13.1 requires: counts and decimetre values are exact in `report`; a `ratio4` or `to_decimal(4)` value is within 0.00005 of the exact value, so with bounds of two decimals the side is decided unless the printed value equals the bound, which happened for no seed. `land_fraction` is land_cells / cells. Counts (`landmasses`) are scaled per 13.1: 512 hard ≤ 10, typical ≤ 4; 2048 hard ≤ 160, typical ≤ 60. The hydrology rows of 13.1 do not apply (no hydrology stage yet).

Hard bounds, min / median / max:

| Statistic | Hard bound | 512 (50 seeds) | 1024 (50 seeds) | 2048 (8 seeds) | Violations |
| --- | --- | --- | --- | --- | --- |
| `land_fraction` | 0.34–0.50 | 0.3992 / 0.4004 / 0.4175 | 0.3989 / 0.4007 / 0.4201 | 0.4000 / 0.4017 / 0.4113 | none |
| `largest_landmass_share` | ≥ 0.85 | 0.9364 / 1 / 1 | 0.9378 / 0.9977 / 1 | 0.9826 / 0.9983 / 1 | none |
| `landmasses` | ≤ 10 / 40 / 160 | 1 / 1 / 5 | 1 / 2 / 7 | 1 / 3 / 6 | none |
| `coast_ratio` | 1.4–6.0 | 1.5593 / 1.9198 / 2.8324 | 1.5574 / 1.9950 / 2.8392 | 1.7370 / 2.0337 / 2.8498 | none |
| `land_solidity` | 0.45–0.93 | 0.7197 / 0.8270 / 0.9123 | 0.7351 / 0.8232 / 0.9142 | 0.7552 / 0.8262 / 0.8880 | none |
| `elevation_m` p50 | 40–300 m | 82.5 / 103.0 / 207.4 | 83.1 / 98.0 / 155.8 | 83.0 / 88.6 / 95.1 | none |
| `elevation_m` p90 | 300–1300 m | 512.2 / 919.8 / 1060.0 | 405.0 / 877.2 / 1140.3 | **224.2** / 472.5 / 1025.7 | **2048 seed 5** |
| `elevation_m` p100 | 1200–2800 m | 1245.2 / 2109.6 / 2260.1 | 1529.5 / 2262.0 / 2584.1 | 2233.9 / 2390.7 / 2455.5 | none |
| `land_above_m` 1500 | ≤ 0.08 | 0 / 0.0197 / 0.0472 | 0.0001 / 0.0220 / 0.0590 | 0.0065 / 0.0158 / 0.0328 | none |
| `land_above_m` 500 | 0.06–0.35 | 0.1021 / 0.2091 / 0.3170 | 0.0847 / 0.1767 / 0.2997 | 0.0743 / 0.0960 / 0.2012 | none |
| 1 − `land_above_m` 50 | 0.10–0.50 | 0.1275 / 0.1948 / 0.2667 | 0.1207 / 0.2009 / 0.2649 | 0.1750 / 0.2166 / 0.2550 | none |
| `step_p999_m` | ≤ 150 m | 61.9 / 89.2 / **183.8** | 53.3 / 62.0 / 125.0 | 52.0 / 55.8 / 61.1 | **512 seed 41** |

(Medians of an even count are the mean of the two middle values.)

Typical bounds (should hold for ≥ 90% of seeds), seeds outside:

| Statistic | Typical | 512 | 1024 | 2048 |
| --- | --- | --- | --- | --- |
| `land_fraction` | 0.38–0.46 | 0 | 0 | 0 |
| `largest_landmass_share` | ≥ 0.95 | 1 (s20) | 1 (s20) | 0 |
| `landmasses` | ≤ 4 / 15 / 60 | 1 (s6) | 0 | 0 |
| `coast_ratio` | 1.6–3.5 | 2 (s18, s47) | 1 (s18) | 0 |
| `land_solidity` | 0.70–0.90 | 4 (s7, s37, s44, s47) | 2 (s21, s44) | 0 |
| p50 | 60–200 m | 2 (s2, s9) | 0 | 0 |
| p90 | 400–1100 m (2048: 330–1100) | 0 | 2 (s12, s18) | 1 (s5) |
| p100 | 1300–2400 m (512), 1500–2600 m | 1 (s10) | 0 | 0 |
| `land_above_m` 1500 | ≤ 0.05 | 0 | 1 (s12) | 0 |
| `land_above_m` 500 | 0.08–0.30 | 3 (s2, s30, s44) | 0 | **2 (s4, s5): 75% inside** |
| 1 − `land_above_m` 50 | 0.15–0.40 | 5 (s21, s35, s43, s44, s48): exactly 90% inside | 3 (s21, s44, s48) | 0 |

Every typical range holds for at least 90% of seeds except at 2048, where `land_above_m` 500 is below 0.08 in 2 of 8 seeds (0.0743 s5, 0.0791 s4) and p90 below 330 m in 1 of 8. Eight seeds is a small sample, but both point the same way as the hard violation: some 2048 worlds have little upland.

Descriptive statistics (min / median / max):

| Statistic | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| `max_step_m` | 71.5 / 100.9 / 298.3 | 66.8 / 80.9 / 251.8 | 74.7 / 82.0 / 89.5 |
| `slope_over` 35 | 0.192 / 0.383 / 0.515 | 0.135 / 0.251 / 0.337 | 0.073 / 0.113 / 0.210 |
| `slope_over` 45 | 0.154 / 0.310 / 0.397 | 0.101 / 0.190 / 0.248 | 0.051 / 0.082 / 0.159 |
| `slope_p50_above_500m` | 1.18 / 1.98 / 3.39 | 0.95 / 1.45 / 1.98 | 1.20 / 1.29 / 1.89 |
| `lowland_flat_share` | 0.0019 / 0.0066 / 0.0142 | 0.0079 / 0.0144 / 0.0236 | 0.0199 / 0.0349 / 0.0390 |
| `edge_parallel_run_m` | 864 / 1384 / 2432 | 1104 / 1728 / 3200 | 1872 / 2200 / 3040 |

Outside the v16 measured ranges (worth a look, not defects): `max_step_m` 298.3 m at 512 and 251.8 m at 1024 (both seed 41; v16 maxima 140 and 127 m); `slope_p50_above_500m` 3.39 at 512 (v16 up to 2.94); `slope_over` 35 0.515 at 512 (v16 0.51).

## 4. Defect reports

### D1. 512 seed 41: `step_p999_m` 183.8 m exceeds the hard 150 m (P1-00 13.1, P1-01 12.3)

- Generator 0.3.0, shipped data, `--seed 41 --map-size 512`, any thread count.
- Reproduce: `worldgen-cli generate --seed 41 --map-size 512 --out /tmp/s41.db && worldgen-cli report --db /tmp/s41.db`
- Expected: `step_p999_m` ≤ 150.0 (P1-00 13.1, hard at every map size; P1-01 10.5 and 12.3).
- Actual: `step_p999_m = 183.8`, `max_step_m = 298.3`. File hash `b0b641cc4d1128754205ebbc36cb5c8a336577de8f7fa2bfb405801b501eda76`. Recomputed independently from the decoded layer (numpy, the 10.3 definition): p999 1838 dm, max 2983 dm, so the report is right.
- Narrowed down: 187 land cells (0.17% of land) have a cell step above 150 m, all inside x 108–128, y 53–82, where a range meets the north-west coast. The largest step is at (111, 72): e = 3047 dm with neighbours 6030, −29 (ocean), 3430, 2813. The steep cells' heights run from 128 m to 1547 m (median 730 m): a mountain flank falling straight to the sea, not a single spike. The composite map (`images/p1-01/p1-01-512-seed-41-steep-crop.jpg`, x 72–167, y 24–119 at 4×) shows a large smooth, untextured, very steep face there. Same seed at 1024 is also the sweep's worst (`step_p999_m` 125.0, `max_step_m` 251.8; next worst 86.5 and 89.5), so the shape of this world's outline and belt produces it at both sizes. The other 49 seeds at 512 are at most 139.6 m (s33). Root cause not determined (candidates: the coastal falloff cutting through a high belt, P1-01 limitation 6 in a stronger form; E8 cannot lower a range whose base is the shoreline). Stage logic, so back to the generator engineer or the lead's decision on the bound.

### D2. 2048 seed 5: `elevation_m` p90 224.2 m is below the hard 300 m (P1-00 13.1)

- Generator 0.3.0, shipped data, `--seed 5 --map-size 2048`, any thread count.
- Reproduce: `worldgen-cli generate --seed 5 --map-size 2048 --out /tmp/s5.db && worldgen-cli report --db /tmp/s5.db`
- Expected: p90 of land elevation in 300–1300 m (hard; the lead's decision "Every world has real uplands").
- Actual: `elevation_m = 0.1 0.9 24.4 51.0 83.0 109.6 224.2 1725.8 2268.9`; `land_above_m = 50:0.7579 500:0.0743 1000:0.0470 1500:0.0222 2000:0.0019`. File hash `f18fefce7a48654b862504003df554ddb5daf056e9e679ec3526f83c3b240084`. `land_above_m` 500 (0.0743) is inside its hard bound (≥ 0.06) but under the typical 0.08.
- Narrowed down: one narrow range crossing a single broad, low continent (`images/p1-01/p1-01-2048-seed-5.jpg`); 7.4% of land is above 500 m and almost all of it sits in that range, so the 90th percentile falls on the lowland. The mountain floor (E5.1) counts core cells (mask ≥ 0.25), not heights, so it cannot guarantee p90 ≥ 300 m. v16's 2048 sweep had only seeds 0–4 (p90 down to 342 m); seed 4 here is 342.3 m too. Either a tuning or floor issue at 2048 or a bound to revise; for the lead.

## 5. Performance (gcc-release, default threads = 6, `generate` wall time including load and write; peak RSS from `/usr/bin/time`)

| Size | Seeds | Time min / median / max | Budget, elevation (all geography) | Peak RSS | Budget |
| --- | --- | --- | --- | --- | --- |
| 512 | 50 | 1.78 / 1.85 / 1.96 s | 2 s (4 s) | 30 MB | 200 MB |
| 1024 | 50 | 6.42 / 6.61 / 7.01 s | 8 s (15 s) | 101 MB | 600 MB |
| 2048 | 8 | 25.9 / 26.4 / 27.0 s | 35 s (65 s) | 376 MB | 2 GB |

All within budget. One thread: 512 about 4.6 s, 1024 about 18 s, 2048 about 72 s; 8 threads on 6 cores is no slower than 5 (thread test of section 1).

## 6. Images

Maps (`render --composite map`) of every sweep world are under `~/.cache/the-unmapped/out/p1-01-validation/maps/`, contact sheets under `~/.cache/the-unmapped/out/p1-01-validation/sheets/` (1024 seeds 0–24 and 25–49, 512 seeds 0–49, 2048 seeds 0–7; JPEG, about 120 to 220 KB each). Copied into the repository:

- `images/p1-01/p1-01-1024-seeds-0-24.jpg`: contact sheet of the 1024 sweep, seeds 0–24, 200 px per map.
- `images/p1-01/p1-01-512-seed-41-steep-crop.jpg`: D1, 512 seed 41, cells x 72–167, y 24–119 at 4×.
- `images/p1-01/p1-01-2048-seed-5.jpg`: D2, 2048 seed 5, downscaled to 512 px.

The visual checklist of P1-00 13.2 is the world critic's.
