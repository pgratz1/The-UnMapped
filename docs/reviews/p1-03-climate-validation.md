# Validation: P1-03 climate

Validation specialist, 2026-11-17. Commit validated: `40a9235` (local `main`, "Review: world critic on the climate stage"; implementation, independent tests and determinism audit merged; also on origin as `p1/03-climate-final`), `GENERATOR_VERSION` 0.7.0, schema version 2, shipped `game/data` with content hash `79860d57…cb0b7fcf`. Specs on that commit: `docs/specs/p1-03-climate.md` (revised twice on 2026-11-17: triangle kernel C4, diagonal taps across the wind, `moisture_path_mm_km` 32000, `rain_oro_mm` 2500, `coast_rain_mm` 75, seam guards), `docs/specs/p1-00-geography-overview.md` (11, 13.1). Determinism audit: `docs/reviews/p1-03-climate-determinism-audit.md` (pass). World critic: `docs/reviews/p1-03-climate-critic.md` (merge as is; reviewed at `coast_rain_mm` 150). Machine: 6-core Linux desktop, 31 GB. Builds, tests, the sweep and the image work ran with several jobs at once (load 5 to 9); the timings of section 6 were taken afterwards, one world at a time, with only the desktop session running (load average about 2). Windows MSVC is run in CI by the lead.

**Verdict: pass.** Every Linux preset builds and passes `ctest` and the slow suites; the 12 hash cases are identical on the five Linux presets and `p1-worlds.txt` is recorded; threads and compilers do not change output, for orthogonal and diagonal winds; in all 108 sweep worlds the `elevation` layer, the seven hydrology layers and table `pass` are byte-identical to the 0.6.0 sweep, so no elevation or hydrology statistic moves; no hard bound of P1-00 13.1 is violated; the four new `report` keys agree with an independent computation from the layers in every world; no cell reaches the rainfall floor or cap, and no value is out of range; the climate stage takes 0.24 / 0.8 / 2.9 s against budgets of 0.3 / 1.5 / 6 s, with no change to peak memory. Section 3.3 proposes final climate ranges. No defect report.

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug`, `linux-clang-debug`, `linux-clang-asan` | pass (all five, warnings as errors) |
| `ctest` per preset (4 tests each; asan with `--timeout 3600`) | pass on all five. Under load: gcc-release 154 s, clang-release 181 s, gcc-debug 473 s, clang-debug 494 s, clang-asan 2390 s (`worldgen-tests` 1732 s, `worldgen-cli-tests` 658 s). Quiet, afterwards: gcc-release 108 s, clang-release 111 s (`CLAUDE.md` said about 90 s; updated to about 110 s). The debug and asan times were taken under load only, so `CLAUDE.md` keeps its figures for them |
| `worldgen-tests -ts=elevation-slow --no-skip`, gcc-release and clang-release | pass: 10 test cases, 95,017,942 assertions each |
| `worldgen-tests -ts=hydrology-slow --no-skip`, both | pass: 5 test cases, 3,910 assertions each |
| `worldgen-tests -ts=climate-slow --no-skip`, both | pass: 8 test cases, 1,057,495 assertions each (includes the reference's N = 512 and 1024 cases of `p1-03-climate.txt`) |
| `worldgen-cli-tests -ts=hydrology-slow` and `-ts=climate-slow --no-skip`, both | pass: 1 test case each, 23 and 32 assertions |
| `determinism_lint.py --self-test` and the scan | pass: 13 snippets; 115 C++ and 11 CMake files, 0 findings |
| `tools/ci/world-hashes.sh local` over the five presets | pass: all 12 cases identical (`hash --detail` and `report` text), 625 s under load; `generate` alone with gcc-release, quiet: 14 s |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on every preset |
| Committed `p0-worlds.txt` (re-recorded for 0.7.0 by the engineer, `1acc6c0`) | confirmed: equals `sha256sum` of each of the eight P0 case worlds on all five presets (the comparison is the `local` run above); `p0_world_check.py check … --detail --report --generator-version 0.7.0 --content-hash 79860d57…cb0b7fcf` passes for all eight cases on gcc-release and clang-asan worlds (and fails, as it should, when given 0.6.0) |
| Climate layer hashes against the reference (P1-03 10.4) | the `temperature` and `rainfall` layer hashes of the four geo cases equal the reference values in `worldgen/tests/golden/p1-03-climate.txt` (section 5) |
| Seed sweep (gcc-release, default threads): 512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–7 | 108 of 108 `generate`, `report` and `hash --detail` succeed, stderr only the three `--verbose` lines; no hard-bound violation (section 3) |
| Upstream identity against the 0.6.0 sweep (`~/.cache/the-unmapped/out/p1-01b-validation/`) | pass: in all 108 worlds the layer hashes of `elevation`, `water`, `water_depth`, `flow_dir`, `flow_accum`, `river`, `lake`, `coast_dist` and the hash of table `pass` equal the 0.6.0 world's; every `[geography]` line of `report` present in 0.6.0 is identical; the only new lines are the four climate keys |
| Thread independence: 512 seeds 0 (SE), 6 (E), 8 (NW), 14 (S); 1024 seeds 1 (SW), 5 (N); 2048 seeds 1 (SW), 7 (S); `--threads` 1, 2, 5, 8; gcc-release and clang-release | pass: all 64 files have the file hash of the same world in the sweep (four diagonal and four orthogonal winds) |

## 2. Climate layers

Layers decoded with a numpy decoder of the P0-02 codec, cross-checked value for value against `p0_world_check.decode_blob` on four layers of 512 seed 0. "Land" is `elevation` > 0, as in `report`; ocean is `water` = 3 (the two are complements in every world).

### 2.1 Ranges and sentinels

| | 512 (50) | 1024 (50) | 2048 (8) |
| --- | --- | --- | --- |
| `temperature`, all cells (tenths °C) | −82 to 94 | −103 to 95 | −90 to 95 |
| `temperature`, ocean | 46 to 94 | 46 to 95 | 46 to 95 |
| Cells at −600 or 500 (clamp) | 0 | 0 | 0 |
| T + lapse × S − t_sea(y), every cell (°C; the noise term plus rounding) | −0.56 to 0.53 | −0.76 to 0.64 | −0.67 to 0.63 |
| `rainfall`, land (mm) | 529 to 9583 | 438 to 8972 | 345 to 6017 |
| `rainfall`, ocean | 564 to 1070 (world means 842–914) | 508 to 1070 (847–896) | 397 to 1072 (865–876) |
| Cells at the floor 250 mm (land / ocean) | 0 / 0 | 0 / 0 | 0 / 0 |
| Cells at the cap 10000 mm (land / ocean) | 0 / 0 | 0 / 0 | 0 / 0 |
| Values outside [−600, 500] or [250, 10000] | 0 | 0 | 0 |

The temperature residual stays inside the ±1 °C of `temp_noise_amp_c`, so every cell is t_sea minus the lapse on S plus the noise, as C3 states. Ocean temperatures are sea-level values (4.6 to 9.5 °C, north to south plus noise). Ocean rainfall is base rain from air near saturation plus the full coast bonus (P1-03 limitation 10): plausible, nothing reads it. The floor and the cap are never reached: the driest land cell is 345 mm (2048 seed 4), the wettest 9583 mm (512 seed 21 at (75, 188), a windward sea cliff under a west wind, 937 m high; 4% below the cap, against the spec's 8671 mm maximum on 10 worlds, limitation 4).

### 2.2 Land share by rainfall band

Land cells pooled over the worlds of a size (in brackets the per-world range); "land" here is all land, lakes included (excluding lakes moves no share by more than 0.7 points).

| N | < 500 mm | 500–700 | 700–1000 | 1000–2000 | > 2000 |
| --- | --- | --- | --- | --- | --- |
| 512 | 0.0% (0–0) | 14.5% (0–30.5) | 55.3% (37.8–73.2) | 19.3% (9.8–32.1) | 10.8% (5.4–16.7) |
| 1024 | 0.4% (0–4.6) | 35.8% (18.1–47.5) | 43.5% (30.9–59.9) | 14.8% (8.6–21.2) | 5.5% (1.5–9.9) |
| 2048 | 10.2% (2.4–17.5) | 46.9% (40.5–52.3) | 34.2% (31.3–37.5) | 7.2% (3.9–11.6) | 1.6% (0.3–3.4) |

Land (not lake) under 700 mm: 14.5% / 35.8% / 56.5%, against the coast experiment's E1 16.7% / 33.9% / 57.7% on 22 worlds. The size dependence of limitation 7 is as the spec describes.

### 2.3 Windward and lee

Measure of P1-03 11.1: a lowland cell is land, not lake, below 200 m; it is "lee" if tracing against the wind for up to 400 steps meets a cell of 800 m or more, "windward" if it is not lee and tracing with the wind meets one. Means are cell-weighted over all worlds of a size.

| N | Windward lowland mm | Lee lowland mm | Other lowland | Lee lowland share of land (min / median / max) | Worlds with lee ≤ 5% of land |
| --- | --- | --- | --- | --- | --- |
| 512 | 1176 | 759 | 926 | 0.012 / 0.126 / 0.330 | s1, s4, s12, s26 |
| 1024 | 924 | 658 | 821 | 0.012 / 0.138 / 0.329 | s4, s38 |
| 2048 | 737 | 535 | 713 | 0.021 / 0.102 / 0.136 | s4 |

Windward lowland is wetter than lee lowland in every one of the 108 worlds. The weakest contrasts (windward ÷ lee mean): 512 s43 1.05, s45 1.15, s26 1.19; 1024 s43 1.11, s26 1.17, s38 1.17; 2048 s4 1.26. The strongest: 512 s23 3.22, 1024 s19 2.34. The spec's figures (11.1, variant D at 1024: 931 / 671 mm) are reproduced. The low-lee worlds include the critic's (limitation 9: 512 s1, s4, s12; 1024 s4; 2048 s4) and add 512 s26 and 1024 s38.

Mean land rainfall by distance from the coast (0–300 m / 300–800 m / 0.8–1.5 km / 1.5–3 km): 512 1102 / 1168 / 1166 / 1205; 1024 940 / 961 / 941 / 922; 2048 836 / 793 / 742 / 716 mm. No coastal step at 512 and 1024; at 2048 the coast is about 100 mm wetter than 1.5–3 km inland, which is mostly the interior drying (limitation 7) plus the 75 mm bonus.

### 2.4 Wind directions

The direction is not stored; it was taken from `p1_climate_reference.py winds 0 49` (C2 with the shipped weights, which depends only on the seed, so every size shares it). The reference reproduces the spec's list for seeds 0 to 11 and 42, and the audit's codes for 512 seeds 13 and 42 and 1024 seeds 1 and 5.

| Seed | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Toward | SE | SW | W | SE | SE | N | E | S | NW | SE | S | SE | NW | W | S | N | S | W | SE | W | N | E | SE | W | S |

| Seed | 25 | 26 | 27 | 28 | 29 | 30 | 31 | 32 | 33 | 34 | 35 | 36 | 37 | 38 | 39 | 40 | 41 | 42 | 43 | 44 | 45 | 46 | 47 | 48 | 49 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Toward | NE | SE | SE | SW | NW | SW | W | N | NW | N | NW | SE | E | N | SW | S | W | N | E | SE | SE | NE | NE | NW | NW |

Seeds 0–49 (each used at 512 and 1024; 0–7 at 2048): E 4, SE 12, S 6, SW 4, W 7, NW 7, N 7, NE 3. Against the uniform 6.25 each, χ² = 8.9 with 7 degrees of freedom (p ≈ 0.26): consistent with uniform weights; 22 of 50 diagonal. The 2048 sample has no NW or NE world.

## 3. Statistics against P1-00 13.1

### 3.1 Hard bounds: no violation

The elevation and hydrology rows are those of the 0.6.0 validation (`docs/reviews/p1-01b-elevation-passes-validation.md` 3.1) world by world, because every input layer and every 0.6.0 `report` line is unchanged (section 1); the exact comparisons made there stand. Recomputed from the 0.7.0 reports (min / median / max):

| Statistic | Hard bound | 512 (50 seeds) | 1024 (50 seeds) | 2048 (8 seeds) |
| --- | --- | --- | --- | --- |
| `land_fraction` | 0.34–0.50 | 0.3992 / 0.4004 / 0.4175 | 0.3989 / 0.4007 / 0.4201 | 0.4000 / 0.4017 / 0.4113 |
| `largest_landmass_share` | ≥ 0.85 | 0.9364 / 1 / 1 | 0.9378 / 0.9977 / 1 | 0.9826 / 0.9984 / 1 |
| `landmasses` | ≤ 10 / 40 / 160 | 1 / 1 / 5 | 1 / 2 / 7 | 1 / 3 / 6 |
| `coast_ratio` | 1.4–6.0 | 1.5593 / 1.9198 / 2.8324 | 1.5574 / 1.9950 / 2.8392 | 1.7370 / 2.0337 / 2.8498 |
| `land_solidity` | 0.45–0.93 | 0.7197 / 0.8271 / 0.9123 | 0.7351 / 0.8232 / 0.9142 | 0.7552 / 0.8262 / 0.8880 |
| `elevation_m` p50 | 40–300 m | 82.5 / 103.0 / 200.1 | 83.1 / 98.0 / 153.9 | 83.0 / 88.6 / 95.0 |
| `elevation_m` p90 | 300–1300 m (2048: 200–1300) | 500.4 / 905.0 / 1056.9 | 405.0 / 852.6 / 1118.4 | 222.8 / 435.3 / 1018.0 |
| `elevation_m` p100 | 1200–2800 m | 1245.2 / 2108.1 / 2257.0 | 1529.6 / 2261.9 / 2584.0 | 2233.9 / 2390.7 / 2455.5 |
| `land_above_m` 1500 | ≤ 0.08 | 0 / 0.0197 / 0.0470 | 0.0001 / 0.0215 / 0.0589 | 0.0065 / 0.0155 / 0.0322 |
| `land_above_m` 500 | 0.06–0.35 | 0.1001 / 0.1986 / 0.3171 | 0.0847 / 0.1729 / 0.2997 | 0.0737 / 0.0900 / 0.2004 |
| 1 − `land_above_m` 50 | 0.10–0.50 | 0.1275 / 0.1949 / 0.2667 | 0.1208 / 0.2009 / 0.2649 | 0.1750 / 0.2166 / 0.2550 |
| `step_p999_m` | ≤ 150 m | 61.9 / 83.4 / 136.4 | 53.9 / 63.3 / 118.7 | 52.0 / 56.0 / 61.5 |
| `lakes` | ≤ 25; 1–100; 4–400 | 0 / 2 / 4 | 2 / 7 / 12 | 6 / 11.5 / 15 |
| `lake_share_of_land` | ≤ 0.031 | 0 / 0.0153 / 0.0300 | 0.0113 / 0.0292 / 0.0300 | 0.0298 / 0.0300 / 0.0300 |
| `river_share_of_land` | 0.005–0.05 | 0.0125 / 0.0161 / 0.0196 | 0.0169 / 0.0194 / 0.0225 | 0.0231 / 0.0240 / 0.0256 |
| `major_rivers` | 1–20; 1–40; 4–160 | 2 / 5 / 8 | 9 / 15 / 21 | 32 / 40 / 43 |
| `longest_river_m` ÷ side | ≥ 0.15 | 0.326 / 0.538 / 0.977 | 0.380 / 0.603 / 1.610 | 0.581 / 0.842 / 0.999 |
| `largest_basin_share` | 0.03–0.6 | 0.0841 / 0.1962 / 0.3694 | 0.0896 / 0.1774 / 0.3749 | 0.1093 / 0.1773 / 0.3052 |
| `temperature_c` p50 | catalogue range | 5.0 / 5.8 / 6.5 °C | 5.3 / 5.9 / 6.5 °C | 5.9 / 6.2 / 6.5 °C |
| `rainfall_mm` p50 | ≥ `rain_min_mm` (250) | 752 / 825.5 / 932 | 707 / 755.5 / 817 | 648 / 678.5 / 683 |
| `rain_contrast` | descriptive | 1.8648 / 3.1007 / 4.6423 | 1.6383 / 2.5345 / 3.3435 | 1.7413 / 1.8770 / 2.4664 |
| `land_below_0c` | descriptive | 0.0188 / 0.0592 / 0.1043 | 0.0312 / 0.0635 / 0.1043 | 0.0211 / 0.0347 / 0.0913 |

`rain_contrast` compared as the exact ratio p90 / p10 of the integer percentiles. The four climate keys were recomputed from the decoded layers (nearest-rank land percentiles of P1-00 10.3, `metres1` and `ratio4` with round-half-away) and equal the printed text in all 108 worlds. Other climate descriptives: `rainfall_mm` p10 614–740 / 531–664 / 466–547, p90 1255–2985 / 1001–1976 / 848–1211, p100 3418–9583 / 2884–8972 / 3028–6017 mm; land temperature minimum −8.2 to −1.6 / −10.3 to −2.7 / −9.0 to −7.3 °C, maximum 8.1–9.0 / 8.1–9.2 / 8.5–9.1 °C.

### 3.2 Typical bounds

Elevation and hydrology rows: unchanged world by world, so the misses are those of the 0.6.0 validation (3.2 there): at 2048, p90 (s5), `land_above_m` 500 (s4, s5) and `lakes` (s1); single seeds elsewhere.

Climate rows against the provisional typical ranges:

| Statistic | Typical (provisional) | 512 | 1024 | 2048 |
| --- | --- | --- | --- | --- |
| `temperature_c` p50 | 4.5–7.0 °C | none (100%) | none | none |
| `rainfall_mm` p50 | 700–1030 / 650–880 / 600–745 mm | none | none | none |
| `rain_contrast` | 1.5–4.6 | s31: 4.6423 (98%) | none | none |
| `land_below_0c` (measured 0.019–0.085, no typical range) | — | s2 0.0188, s5 0.0853, s11 0.0887, s21 0.1043, s34 0.0851 | s10, s12, s20, s21, s31, s40, s46 (0.0865–0.1043) | s0 0.0913 |

Every typical range holds for at least 90% of seeds. `land_below_0c` is descriptive; the experiment's range was measured on 22 worlds and 13 of the 108 fall outside it, by up to 0.019.

### 3.3 Climate ranges: proposed final values

By the rule of P1-03 9 and P1-00 13.1 (typical = measured min–max widened by about 10% of the value, low end × 0.9 and high end × 1.1, rounded outward; hard only where a value outside means a defect), from this sweep:

| Statistic | Hard (proposed) | Typical (proposed) | Measured, 512 / 1024 / 2048 |
| --- | --- | --- | --- |
| `temperature_c` p50 | catalogue range (construction); optionally ≤ 10.0 °C, which every land cell must meet (t_sea ≤ 9 °C plus 1 °C of noise, S ≥ 1 dm) | **4.5–7.2 °C** at every size (now 4.5–7.0) | 5.0–6.5 / 5.3–6.5 / 5.9–6.5 °C |
| `rainfall_mm` p50 | ≥ 250 (construction) | **680–1030 / 635–900 / 585–750 mm** (now 700–1030 / 650–880 / 600–745) | 752–932 / 707–817 / 648–683 mm |
| `rain_contrast` | descriptive | **per size 1.7–5.1 / 1.5–3.7 / 1.6–2.7** (now 1.5–4.6 for all; one range would be 1.5–5.1) | 1.86–4.64 / 1.64–3.34 / 1.74–2.47 |
| `land_below_0c` | descriptive | **0.017–0.115** at every size (now none) | 0.019–0.104 / 0.031–0.104 / 0.021–0.091 |
| `rainfall_mm` p100 | descriptive; < 10000 is not a requirement (the cap is part of C6) | — | 3418–9583 / 2884–8972 / 3028–6017 mm |

The rainfall statistics depend on map size (limitation 7), so they keep per-size ranges; `rain_contrast` narrows strongly with size (a larger land has a larger, uniformly dry interior that sets p10 and p90). Temperature does not depend on size beyond sampling. The 2048 ranges rest on 8 seeds; P1-00 13.1 asks for 2048 seeds 0–19 and 1024 seeds 0–199, which this sweep (the lead's seed set) does not cover. For the lead's decision; the specs are not changed here.

## 4. Images and what they show

Rendered with `worldgen-cli render --layer rainfall` and `--layer temperature` (registered palettes under `auto`; `--palette rainfall` gives the same PNG, and `--palette rainfall` on `temperature` is refused as P1-03 9 requires) and `--composite map` for every sweep world: `~/.cache/the-unmapped/out/p1-03-validation/renders/`. Contact sheets with the coastline drawn and the wind arrow and label from C2 on every tile: `…/sheets/` (1024 seeds 0–24 and 25–49 at 200 px, 2048 seeds 0–7 at 320 px, 512 seeds 0–49 at 128 px, each for rainfall and temperature). Full-resolution crops beside the composite map: `…/crops/`.

- **Rain shadows** sit on the correct side in every world I looked at on the sheets: the wet band hugs the face of each range that the arrow points into and the land beyond is buff (1024 s8, s19, s33, s34, s44 are clear examples; 2048 s5 and s6). Worlds with little visible shadow are those with the main range on the downwind coast (1024 s4, s38, s43; 512 s1, s4, s12, s26; 2048 s4), as section 2.3 measures and limitation 9 describes.
- **No banding or streaks** at sheet scale. At full resolution (2048 s0 under a diagonal wind, 2048 s2, s5; 1024 s8) the field is smooth and blobby; no comb bands across the wind and no stripes along it. On the windward face of 2048 ranges the rain breaks into a string of beads, one per spur facing the wind (2048 s5, s2, s0): it follows the terrain, not a lattice.
- **Coasts.** Lee coasts show no wet rim at 1024 (1024 s8 north-west coast); windward coastal plains are moderately wet with no step. The sea renders pale (base rain plus the full coast bonus, about 870 mm), so the coastline is not visible on the raw `rainfall` render where the adjacent land is near 870 mm; the sheets draw it. A windward cliff coast jumps from about 900 mm on the sea to several thousand on the first land cells (512 s21 wettest cell; limitation 10).
- **2048 maps** are mostly buff with narrow wet bands on the ranges (the 56.5% of land under 700 mm of section 2.2); limitation 7.
- **Temperature** is a smooth north-to-south ramp on the lowlands with the ranges cold; it looks the same in every world apart from where the ranges are (limitation 8). No seams or blocks.
- **Block seams.** Mean absolute differences between adjacent rows and columns are highest at multiples of 64 cells, in both directions: rainfall 20% above the median offset at 512 and 7% at 1024, temperature 10–14% / 5–7%. The same pattern is in the 0.6.0 `elevation` layer (11–12% at 512, both axes), and the climate stage's own fields show none (ocean temperature, which is t_sea plus the climate noise only: no peak at 64). So it is inherited from the elevation layer through S, not a G6 row-block seam of climate (that would appear along one axis only). It is not visible on any render I looked at. For the elevation side, not a climate finding.

Copied into the repository (580 KB):

- `images/p1-03/p1-03-1024-rainfall-seeds-0-24.jpg`: rainfall, 1024 seeds 0–24 at 200 px, coast outlined, wind arrow and label per world.
- `images/p1-03/p1-03-2048-rainfall-seeds-0-7.jpg`: rainfall, 2048 seeds 0–7 at 320 px, the same.
- `images/p1-03/p1-03-rainfall-crops.jpg`: rainfall beside the composite map at full resolution: 2048 s5 (range under a north wind), 512 s21 (the wettest land cell, windward sea cliff, 3×), 1024 s8 (lee coast without a wet rim).

## 5. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` re-recorded for 0.7.0 after the five-preset comparison passed. The legitimate reasons for the change: generator 0.7.0 adds the climate stage to the default pipeline, so each geography case gains the `temperature` and `rainfall` layers and `meta.stages` = `elevation,hydrology,climate`, and the new `worldgen.climate` definition changes `content_hash` (P1-03 4). Every other layer and table `pass` are byte-identical to 0.6.0 (section 1).

| Case | File hash (0.7.0) |
| --- | --- |
| `geo-phrase-512` | `e7e69424a44bd5aff98beecd755538457dcb823c74a59675eae247d99f365110` |
| `geo-seed-0-512` | `dbde4a3ed864678c6f8233960ba3f20d7aa86d6257228faf2b024dd257acba4d` |
| `geo-seed-1-1024` | `8e02165d866c5cabd74d6eb7978a6052b5d1e575e8dbf100fa471088d3b9584a` |
| `geo-seed-42-512` | `5baf3baeebc3dc873ebc5a4a61bbe0e22df20eac436ec0d60adfbe544f91b1ab` |

Full layer hashes (P0-04 8), as `hash --detail` prints them on all five presets:

```
geo-phrase-512  temperature 5a9148fb00b177b6572187c6a8c226dc0af6b418fe7de1621c82c5837799a331
geo-phrase-512  rainfall    7d97efed6ee036353c500ad8088fc893de5870078b6ca5e2597a6309e3a873bf
geo-seed-0-512  temperature 5223124d001f2a0a61f8a9e4e0f0d1952d5cf9ba228481d1a0a4e1eb8e4d0f9d
geo-seed-0-512  rainfall    659f08662634db0397254378313c020504ef6b0b163e87094e9387503f160871
geo-seed-1-1024 temperature 2f182f981a4a091b6a14469f0f1e6d6b5e3a610ee2dbb80e588a87a44beb822f
geo-seed-1-1024 rainfall    790bab751cee484408552b6960d92095b05df2a4114e3976c4ee8f2789951480
geo-seed-42-512 temperature 242e659fa6110e2efe7b6fc14689d02475a3922c01ddb146a6a23a303421cf2e
geo-seed-42-512 rainfall    abcc7e173867b7ae77aaeb27c0694ca9c05dbdeedc0921dc070d7fb189507b12
```

These are P1-03 10.4's layer hashes at N = 512 (seeds 0, 42, "The Unmapped") and 1024 (seed 1): the geo cases use the shipped data, which equals the fixture tuning (`game/data/worldgen/` and `worldgen/tests/data/geo/worldgen/` are identical). All eight equal the independent reference's values in `worldgen/tests/golden/p1-03-climate.txt` (cases `shipped-512-s0`, `shipped-512-s42`, `shipped-512-phrase`, `shipped-1024-s1`), which `climate-slow` already checks; the N = 128 seed 0 case is in the same file and checked by the default suite. Spec 10.4 asks the validation specialist to record them but names no file; they are recorded here, and nothing in the test tree was added, since the existing golden file already holds the same values from an independent source. `geo-seed-0-512` equals the sweep's 512 seed 0 and `geo-seed-1-1024` the thread test's 1024 seed 1. (The audit's 512 seed 0 file hash `b09e7ac2…` was taken before `coast_rain_mm` became 75.) `p0-worlds.txt` is unchanged (the engineer's 0.7.0 values, confirmed in section 1). The MSVC presets are compared by CI's `compare-hashes`.

## 6. Performance

gcc-release, default threads (6), one world at a time, `/usr/bin/time` wall time and peak RSS, three repeats (two at 2048) of the full `generate` and of `generate --until hydrology`, interleaved; the machine had only the desktop session running (load average about 2). Climate time is the difference of the per-world medians (it includes encoding and writing the two layers).

| Size, seeds | Full `generate` (wall) | `--until hydrology` | Climate (wall) | Climate (CPU, user + sys) | Budget, climate | Peak RSS, full / until hydrology |
| --- | --- | --- | --- | --- | --- | --- |
| 512: 0, 5, 13, 41 | 2.08–2.19 s | 1.83–2.16 s | 0.23–0.24 s | 0.63–0.65 s | 0.3 s | 28.0–29.9 / 27.9–29.9 MB |
| 1024: 0, 3, 15, 41 | 7.38–7.74 s | 6.57–6.87 s | 0.77–0.85 s | 2.51–2.59 s | 1.5 s | 91.9–94.5 / 91.8–94.7 MB |
| 2048: 0, 7 | 29.47–30.26 s | 26.75–29.00 s | 2.7–2.9 s (min of repeats; the medians give 1.6 s for seed 7 because one `--until hydrology` run was slow) | 9.83–9.88 s | 6 s | 348.6–375.5 / 348.6–375.4 MB |

All within budget: climate is 80% of its budget at 512, 57% at 1024 and under 50% at 2048; the whole of geography (so far three stages) is 2.2 / 7.7 / 30.3 s against 4 / 15 / 65 s. Peak RSS does not change: the climate stage's grids (about 40 bytes per cell) are smaller than the peak that elevation sets, so memory is as before (P1-00 budget 200 MB / 600 MB / 2 GB). The sweep's own times (under load): 512 2.1–5.6 s, 1024 8.3–10.9 s, 2048 34.9–37.6 s. One thread (thread test, under load): 1024 21–25 s, 2048 86–105 s.

## 7. Defects and findings

No defect: no hard-bound violation, no cross-preset or thread-count difference, no test failure, no out-of-range value, no change to an upstream layer.

For the lead and the spec writer:

1. Climate ranges (3.3): proposed final typical ranges for `temperature_c` p50, `rainfall_mm` p50 (per size), `rain_contrast` (per size) and `land_below_0c`; the measured `land_below_0c` range of 13.1 (0.019–0.085) is too narrow for 13 of 108 worlds. The 2048 sample is 8 seeds.
2. P1-03 10.4 does not name where the validation specialist records the climate layer hashes; they are in section 5 and equal the existing reference vectors. If a separate golden file is wanted, the spec should name it.
3. `worldgen-cli render --help` lists the palettes `auto, gray, hypsometric, water, river, log2, direction, ids, signed` but not `temperature` and `rainfall`, which `--palette` accepts (P1-03 9). Cosmetic, for the engineer.
4. The 64-cell periodicity of section 4 comes from the elevation layer (0.6.0, unchanged); worth a look by the elevation side or the auditor, though it is not visible in any render.

For the world critic: 512 s21 (75, 188), the wettest land cell at 9583 mm, 4% under the cap (limitation 4 says 8671 mm); the low-lee worlds 512 s26, s43 and 1024 s38, s43 (windward ÷ lee lowland 1.05–1.17); the bead pattern of rain on 2048 windward faces (2048 s5 x 850–1350, y 600–1100); 2048 interiors dry on the windward side of ranges as well as the lee (2048 s5 south of the arc, limitation 7).
