# Validation: P1-04 biomes and schema 3

Validation specialist, 2026-11-17. Commit validated: `b09211a` (local `main`, "Review: world critic on the biome stage"; implementation, independent tests and determinism audit merged; also on origin as `p1/04-biomes-final`), `GENERATOR_VERSION` 0.8.0, schema version 3, shipped `game/data` with content hash `5129d642…14218938`. Specs on that commit: `docs/specs/p1-04-biomes.md` (second version), `docs/specs/p1-03-climate.md` (the `climate` table, C7, `wind_toward`), `docs/specs/p1-00-geography-overview.md` (11, 13.1). Determinism audit: `docs/reviews/p1-04-biomes-determinism-audit.md` (pass). World critic: `docs/reviews/p1-04-biomes-critic.md` (merge as is). Machine: 6-core Linux desktop, 31 GB. Builds, tests, the sweep and the image work ran with several jobs at once (load 6 to 9). The timings of section 7 and the later `ctest` runs were taken afterwards, one job at a time, but another session was then using two to three cores at times, so none of them is a quiet measurement; section 7 says which runs were affected. Windows MSVC is run in CI by the lead.

**Verdict: pass for the biome stage and the schema-3 change.**

- **Builds and tests.** Every Linux preset builds and passes `ctest` and the slow suites.
- **Hashes.** The 12 hash cases are identical on the five Linux presets; `p1-worlds.txt` is recorded and `expect` gives 12 of 12 on every preset. The committed `p0-worlds.txt` is confirmed, with empty worlds of 8 pages and 32768 bytes.
- **Determinism.** Threads and compilers do not change output.
- **Upstream identity.** In all 120 sweep worlds (512 and 1024 seeds 0–49, 2048 seeds 0–19) every 0.7.0 layer and table `pass` are byte-identical to 0.7.0.
- **Tables.** `climate` holds the reference's wind in every world, and `biome` equals the shipped definitions.
- **Biome layer.** The layer equals the test writer's independent reference **on every cell of all 120 worlds**.
- **Biome bounds.** No biome hard bound is violated. Every natural biome is present in every world, no biome exceeds 0.65, and speckle stays under 0.3% of land.
- **Cost.** The stage takes about 0.1 / 0.28 / 0.95 s against budgets of 0.1 / 0.5 / 2 s, with no change to peak memory.

**Two defect reports outside this stage** (section 9): 2048 seeds 13 and 15 violate elevation hard bounds of P1-00 13.1. This is the first sweep with twenty 2048 worlds. Both worlds are byte-identical under 0.7.0, so neither comes from this change. Section 3.3 proposes final biome ranges, and section 3.4 proposes corrections to three 2048 ranges that fail with twenty seeds.

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug`, `linux-clang-debug`, `linux-clang-asan` | pass (all five, warnings as errors) |
| `ctest` per preset (4 tests each; asan with `--timeout 3600`) | pass on all five. Times under load: clang-asan 3325 s during the sweep (`worldgen-tests` 2283 s, `worldgen-cli-tests` 1042 s). The other four ran afterwards one at a time, while another session's jobs used two to three cores: gcc-release 143 s, clang-release 146 s, gcc-debug 592 s, clang-debug 609 s (`worldgen-tests` 95 / 95 / 400 / 414 s). No run was quiet, so `CLAUDE.md`'s `ctest` table (110 s, 4.5 minutes, 18 minutes, measured at 0.4.0) is left alone. The debug and asan figures look stale even allowing for the load; section 9, finding 7 |
| Slow suites, `linux-gcc-release` and `linux-clang-release` (`worldgen-tests -ts=<suite> --no-skip`; `worldgen-cli-tests -ts=<suite> --no-skip`) | pass on both, identical counts. Library: `elevation-slow` 10 cases, 95,017,942 assertions; `hydrology-slow` 5, 3,910; `climate-slow` 8, 1,057,498; `biomes-slow` 5, 3,321 (includes the reference's N = 256, 512 and 1024 cases of `p1-04-biomes.txt`). CLI: `hydrology-slow` 1 case, 23 assertions; `climate-slow` 1, 32; `biomes-slow` 1, 12. Under load: gcc-release 804 / 157 / 395 / 130 s and 33 / 34 / 27 s, clang-release 782 / 143 / 349 / 75 s and 24 / 26 / 24 s |
| `determinism_lint.py --self-test` and the scan | pass: 13 snippets; 120 C++ and 11 CMake files, 0 findings |
| `tools/ci/world-hashes.sh local` over the five presets | pass: all 12 cases identical (`hash --detail` and `report` text), 649 s under load |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on every preset |
| Committed `p0-worlds.txt` (re-recorded for 0.8.0 by the engineer) | confirmed: equals the eight P0 case hashes on all five presets (the `local` run above); `p0_world_check.py check … --detail --report --generator-version 0.8.0 --content-hash 5129d642…14218938` passes for all eight cases on gcc-release and clang-asan worlds and fails, as it should, with `--generator-version 0.7.0`. Every empty world is 8 pages, 32768 bytes, `user_version` and `meta.schema_version` 3 (the checker's header line: `pages=8 … size=32768`) |
| Seed sweep (gcc-release, default threads): 512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–19, one world at a time | 120 of 120 `generate`, `report` and `hash --detail` succeed; stderr only the three `--verbose` lines |
| Upstream identity against the 0.7.0 sweep (`~/.cache/the-unmapped/out/p1-03-validation/`, 512 and 1024 seeds 0–49, 2048 seeds 0–7) | pass: in all 108 worlds the layer hashes of `elevation`, `water`, `water_depth`, `flow_dir`, `flow_accum`, `river`, `lake`, `coast_dist`, `temperature`, `rainfall` and the hash of table `pass` equal the 0.7.0 world's; every `[geography]` line of the 0.7.0 `report` is present and identical; the only new lines are `wind_toward`, `biome_share` and `biome_patches` |
| The same for 2048 seeds 8–19 (no 0.7.0 world existed) | pass: generated with the 0.7.0 gcc-release binary of that sweep and `game/data` of `40a9235`; all ten upstream layers and table `pass` equal the 0.8.0 world's in all 12 |
| Biome layer against the independent reference (`p1_biomes_reference.py`'s B2–B5 run on each world file's own input layers) | pass: **every cell of all 120 sweep worlds** equal (`biome_share` and `biome_patches` text equal too); driver `refcmp.py` (section 10) |
| Tables and layer rules from the files (section 2) | pass in all 120 worlds |
| Partial worlds and render paths | `--until climate`: `climate` 1 row, `biome` 0 rows, `wind_toward` printed, no biome keys; `--until hydrology` and `--until none`: both tables empty; `render --layer biome` (auto) equals `--palette biome`; `--palette biome` on `elevation` exit 2 with the u8 message; `--composite biomes` on a world without `biome` exit 3 with the layer list, no file |
| Thread independence (section 6) | pass: 64 of 64 files (8 worlds × 4 thread counts × gcc and clang release) equal the sweep's |

## 2. Tables and layer rules

Checked from the files of every sweep world with `analyze.py` (section 10; numpy decoder of the P0-02 codec).

- **`climate`**: exactly one row, `(1, k)`, with k the wind of `p1_climate_reference.wind(seed)` (C2 with the shipped weights) in all 120 worlds; `report` prints the same name as `wind_toward`, and `[tables]` lists `biome = 8` and `climate = 1`. Winds of the 2048 seeds 8–19 (new to the sweep): 8 NW, 9 SE, 10 S, 11 SE, 12 NW, 13 W, 14 S, 15 N, 16 S, 17 W, 18 SE, 19 W (seeds 0–49 as in `p1-03-climate-validation.md` 2.4).
- **`biome`**: the eight rows `(id, key, name, colour, water)` equal the shipped definitions parsed from `game/data/biomes/*.toml`, in ascending `id` (= `code`), in every world. Every field of the shipped TOML, rules included, equals spec 4.1 as the reference encodes it (compared field by field). `game/data/worldgen/biomes.toml` equals the `geo` fixture's.
- **Layer `biome`**: `u8`, N × N; every value has a row; no 0; ocean's code exactly on `water` = 3 and lake's exactly on `water` = 2; every land cell (`water` 0 or 1) has a land biome; `blighted_waste` covers 0 cells in every world.
- **Invariant 4** (every dry cell the highest-priority matching rule, every river cell B5's mode) is covered by the reference comparison above, which recomputes G7 (k = 3), G8, the jittered climate, B4 and B5 exactly, on every cell of the sweep.
- **Land temperature** (P1-00 13.1 hard: every land cell ≤ 10.0 °C): maximum 9.0 / 9.2 / 9.1 °C at 512 / 1024 / 2048.

## 3. Statistics against P1-00 13.1

### 3.1 Every row: min / median / max and hard violations

Each cell gives min / median / max. Values are exact: ratios from their integer counts where `report` prints them, biome shares from the decoded layer, the rest from the exact decimal text. No value lies within a rounding step of a hard bound. The elevation and hydrology rows of 512, 1024 and 2048 seeds 0–7 are those of the 0.7.0 validation, world by world, because every input layer is unchanged (section 1).

| Statistic | Hard bound | 512 (50 seeds) | 1024 (50 seeds) | 2048 (20 seeds) | Violations |
| --- | --- | --- | --- | --- | --- |
| `land_fraction` | 0.34–0.50 | 0.3992 / 0.4004 / 0.4175 | 0.3989 / 0.4007 / 0.4201 | 0.3999 / 0.4006 / 0.4113 | none |
| `largest_landmass_share` | ≥ 0.85 | 0.9364 / 1 / 1 | 0.9378 / 0.9977 / 1 | 0.9800 / 0.9984 / 1 | none |
| `landmasses` | ≤ 10 / 40 / 160 | 1 / 1 / 5 | 1 / 2 / 7 | 1 / 2 / 6 | none |
| `coast_ratio` | 1.4–6.0 | 1.5593 / 1.9198 / 2.8324 | 1.5574 / 1.9950 / 2.8392 | 1.6225 / 2.0892 / 2.8837 | none |
| `land_solidity` | 0.45–0.93 | 0.7197 / 0.8270 / 0.9123 | 0.7351 / 0.8232 / 0.9142 | 0.7446 / 0.8200 / 0.8880 | none |
| `elevation_m` p50 | 40–300 m | 82.5 / 103.0 / 200.1 | 83.1 / 98.0 / 153.9 | 82.2 / 88.8 / 96.2 | none |
| `elevation_m` p90 | 300–1300 m (2048: 200–1300) | 500.4 / 905.0 / 1056.9 | 405.0 / 852.5 / 1118.4 | **126.8** / 512.5 / 1018.0 | **2048 s13** |
| `elevation_m` p100 | 1200–2800 m | 1245.2 / 2108.1 / 2257.0 | 1529.6 / 2261.9 / 2584.0 | 2221.6 / 2403.7 / **2931.1** | **2048 s15** |
| `land_above_m` 1500 | ≤ 0.08 | 0 / 0.0197 / 0.0470 | 0.0001 / 0.0215 / 0.0589 | 0.0065 / 0.0166 / 0.0355 | none |
| `land_above_m` 500 | 0.06–0.35 | 0.1001 / 0.1986 / 0.3171 | 0.0847 / 0.1729 / 0.2997 | **0.0328** / 0.1017 / 0.2004 | **2048 s13** |
| 1 − `land_above_m` 50 | 0.10–0.50 | 0.1275 / 0.1948 / 0.2667 | 0.1208 / 0.2009 / 0.2649 | 0.1750 / 0.2148 / 0.2550 | none |
| `step_p999_m` | ≤ 150 m | 61.9 / 83.4 / 136.4 | 53.9 / 63.3 / 118.7 | 52.0 / 57.9 / 85.8 | none |
| `lakes` | ≤ 25; 1–100; 4–400 | 0 / 2 / 4 | 2 / 7 / 12 | 6 / 10.5 / 21 | none |
| `lake_share_of_land` | ≤ 0.031 | 0 / 0.0153 / 0.0300 | 0.0113 / 0.0292 / 0.0300 | 0.0298 / 0.0300 / 0.0300 | none |
| `river_share_of_land` | 0.005–0.05 | 0.0125 / 0.0161 / 0.0196 | 0.0169 / 0.0194 / 0.0225 | 0.0226 / 0.0240 / 0.0262 | none |
| `major_rivers` | 1–20; 1–40; 4–160 | 2 / 5 / 8 | 9 / 15 / 21 | 28 / 39 / 54 | none |
| `longest_river_m` ÷ side | ≥ 0.15 | 0.326 / 0.538 / 0.977 | 0.380 / 0.603 / 1.610 | 0.491 / 0.761 / 1.394 | none |
| `largest_basin_share` | 0.03–0.6 | 0.0841 / 0.1962 / 0.3694 | 0.0896 / 0.1774 / 0.3749 | 0.0918 / 0.1683 / 0.3052 | none |
| `temperature_c` p50 | catalogue range | 5.0 / 5.8 / 6.5 °C | 5.3 / 5.9 / 6.5 °C | 5.8 / 6.1 / 6.5 °C | none |
| land temperature maximum | ≤ 10.0 °C | 8.1 / 8.6 / 9.0 °C | 8.1 / 8.7 / 9.2 °C | 8.4 / 8.8 / 9.1 °C | none |
| `rainfall_mm` p50 | ≥ 250 | 752 / 825.5 / 932 | 707 / 755.5 / 817 | 630 / 672.5 / 714 | none |
| `rain_contrast` | descriptive | 1.8648 / 3.1007 / 4.6423 | 1.6383 / 2.5345 / 3.3435 | 1.6710 / 1.9620 / 2.4664 | — |
| `land_below_0c` | descriptive | 0.0188 / 0.0592 / 0.1043 | 0.0312 / 0.0635 / 0.1043 | 0.0123 / 0.0391 / 0.0974 | — |
| `biome_share` temperate_forest | ≤ 0.80 | 0.2722 / 0.4255 / 0.6111 | 0.3473 / 0.4484 / 0.6351 | 0.4239 / 0.4940 / 0.5755 | none |
| `biome_share` grassland | ≤ 0.80 | 0.1649 / 0.3256 / 0.5850 | 0.1657 / 0.3168 / 0.4465 | 0.2567 / 0.3195 / 0.4203 | none |
| `biome_share` marsh | ≤ 0.15 | 0.0012 / 0.0122 / 0.0254 | 0.0092 / 0.0237 / 0.0392 | 0.0341 / 0.0539 / 0.0644 | none |
| `biome_share` highland_moor | ≤ 0.80 | 0.0412 / 0.1322 / 0.2874 | 0.0426 / 0.1047 / 0.2534 | 0.0209 / 0.0622 / 0.1106 | none |
| `biome_share` alpine | ≤ 0.80 | 0.0397 / 0.0749 / 0.1232 | 0.0382 / 0.0774 / 0.1261 | 0.0143 / 0.0467 / 0.1041 | none |
| `biome_share` blighted_waste | 0 | 0 in every world | 0 | 0 | none |
| `biome_patches` | descriptive | 53 / 81.5 / 123 | 228 / 301.5 / 393 | 1040 / 1257.5 / 1523 | — |

Diagnostic rows: `max_step_m` 70.7–199.6 / 66.8–183.5 / 73.5–107.3 m; `river_sources` 38–61 / 183–235 / 874–972; `passes` 0–2 / 0–3 / 0–5 per world. The rows of P1-05 and P1-06 have no layers yet.

**Three hard-bound violations, all at 2048 on seeds the earlier sweeps did not cover, all in the elevation rows, all present unchanged in 0.7.0:** 2048 seed 13 (`elevation_m` p90 126.8 m against ≥ 200 m, `land_above_m` 500 0.0328 against ≥ 0.06) and 2048 seed 15 (`elevation_m` p100 2931.1 m against ≤ 2800 m). Defects D1 and D2 (section 9). No biome, climate or hydrology row is violated.

### 3.2 Typical misses

| Statistic | Typical | Misses (seed value) | Holds for 512 / 1024 / 2048 |
| --- | --- | --- | --- |
| `largest_landmass_share` | ≥ 0.95 | 512 s20 0.9364; 1024 s20 0.9378 | 98% / 98% / 100% |
| `landmasses` | ≤ 4 / 15 / 60 | 512 s6 5 | 98% / 100% / 100% |
| `coast_ratio` | 1.6–3.5 | 512 s47 1.5593, s18 1.5628; 1024 s18 1.5574 | 96% / 98% / 100% |
| `land_solidity` | 0.70–0.90 | 512 s7, s37, s47, s44 (0.9003–0.9123); 1024 s21 0.9014, s44 0.9142 | 92% / 96% / 100% |
| `elevation_m` p50 | 60–200 m | 512 s2 200.1 | 98% / 100% / 100% |
| `elevation_m` p90 | 400–1100 m (2048: 330–1100) | 1024 s12 1118.4; 2048 s13 126.8, s5 222.8 | 100% / 98% / 90% |
| `elevation_m` p100 | 1300–2400 / 1500–2600 / 1500–2600 m | 512 s10 1245.2; 2048 s15 2931.1 | 98% / 100% / 95% |
| `land_above_m` 1500 | ≤ 0.05 | 1024 s12 0.0589 | 100% / 98% / 100% |
| `land_above_m` 500 | 0.08–0.30 | 512 s44 0.3087, s2 0.3160, s30 0.3171; **2048 s13 0.0328, s5 0.0737, s4 0.0743** | 94% / 100% / **85%** |
| 1 − `land_above_m` 50 | 0.15–0.40 | 512 s44, s21, s48, s43, s35 (0.1275–0.1398); 1024 s21 0.1208, s44 0.1410, s48 0.1424 | 90% / 94% / 100% |
| `lakes` | 0–5 / 1–13 / 7–16 | **2048 s1 6, s11 6, s12 17, s19 21** | 100% / 100% / **80%** |
| `lake_share_of_land` | 0.003–0.03 | 512 s25, s29, s36 (no lake) | 94% / 100% / 100% |
| `major_rivers` | 1–9 / 8–22 / 32–43 | **2048 s13 28, s8 48, s19 50, s10 54** | 100% / 100% / **80%** |
| `land_below_0c` | 0.017–0.115 | 2048 s13 0.0123 | 100% / 100% / 95% |
| `biome_share` grassland | 0.15–0.64 / 0.15–0.49 / 0.23–0.42 | 2048 s11 0.4203 | 100% / 100% / 95% |
| `biome_share` highland_moor | 0.035–0.32 / 0.035–0.28 / 0.030–0.12 | 2048 s13 0.0209 | 100% / 100% / 95% |
| `biome_share` alpine | 0.025–0.14 | 2048 s13 0.0143 | 100% / 100% / 95% |

Every other typical range holds for every seed (`land_fraction`, `river_share_of_land`, `longest_river_m`, `largest_basin_share`, `temperature_c` p50, `rainfall_mm` p50, `rain_contrast`, forest and marsh shares). Three ranges hold for fewer than 90% of 2048 seeds (bold): `land_above_m` 500, `lakes` and `major_rivers`. All three were set on 8 worlds at 2048; section 3.4 proposes corrections. The 512 and 1024 misses are the 0.7.0 sweep's, unchanged.

### 3.3 Biome ranges: proposed final values

By the rule of P1-00 13.1 (typical = measured min–max, low end × 0.9 and high end × 1.1, rounded outward; hard only where a value outside means a defect), from this sweep. The implementation's shares equal the spec writer's calibration model (P1-04 11.1) to the third decimal at 512 and 1024 (for example forest 0.272 / 0.425 / 0.611 at 512, the model's 0.272 / 0.425 / 0.611), so the provisional ranges were already close; the 2048 rows change with the larger sample.

| Statistic | Hard (proposed) | Typical (proposed), 512 / 1024 / 2048 | Now (provisional) | Measured min–max, 512 / 1024 / 2048 |
| --- | --- | --- | --- | --- |
| `biome_share` temperate_forest | ≤ 0.80 (keep) | **0.24–0.68 / 0.31–0.70 / 0.38–0.64** (one range for all sizes: 0.24–0.70) | 0.25–0.67 | 0.272–0.611 / 0.347–0.635 / 0.424–0.576 |
| `biome_share` grassland | ≤ 0.80 (keep) | **0.14–0.65 / 0.14–0.50 / 0.23–0.47** | 0.15–0.64 / 0.15–0.49 / 0.23–0.42 | 0.165–0.585 / 0.166–0.447 / 0.257–0.420 |
| `biome_share` marsh | ≤ 0.15 (keep) | **0.001–0.028 / 0.008–0.044 / 0.030–0.071** | 0.001–0.028 / 0.008–0.043 / 0.030–0.071 | 0.0012–0.0254 / 0.0092–0.0392 / 0.0341–0.0644 |
| `biome_share` highland_moor | ≤ 0.80 (keep) | **0.037–0.32 / 0.038–0.28 / 0.018–0.13** (2048 without s13: 0.029–0.13) | 0.035–0.32 / 0.035–0.28 / 0.030–0.12 | 0.041–0.287 / 0.043–0.253 / 0.021–0.111 |
| `biome_share` alpine | ≤ 0.80 (keep) | **0.035–0.14 / 0.034–0.14 / 0.012–0.12** (2048 without s13: 0.025–0.12) | 0.025–0.14 | 0.040–0.123 / 0.038–0.126 / 0.014–0.104 |
| `biome_share` blighted_waste | 0 (keep: history assigns it) | — | — | 0 |
| `biome_patches` | descriptive | — (a sudden rise means speckle; see the speckle measure in 4.3) | — | 53–123 / 228–393 / 1040–1523 |

Notes for the lead:

- The hard bounds stay as they are: no world comes near them (largest natural share 0.635, largest marsh 0.064), and the marsh bound 0.15 still marks the first rules' defect.
- 2048 s13 is the only world below the provisional moor and alpine floors at 2048. It is also the world with the elevation hard violations (D2): a flat world with one short coastal range. If D2 is resolved by changing the terrain, the 2048 floors can stay near 0.03 and 0.025; if it is resolved by accepting such worlds, the floors should be the strict values above.
- Every natural biome is present in all 120 worlds (no share under 0.1% of land; the smallest is marsh at 512 s10, 0.12%). No biome exceeds 0.65 in any world (largest: forest 0.635 at 1024 s43).
- Marsh grows with map size as the spec says (limitation 3): medians 1.2 / 2.4 / 5.4%.
- Shares depend on size for grassland, marsh, moor and alpine, so per-size typical ranges are needed for those four. Forest could take one range.

### 3.4 The other rows at 2048 with twenty worlds

P1-00 13.1 asks for twenty seeds at 2048; the climate, hydrology and elevation ranges were set on eight. With twenty, by the same rule:

| Statistic | Now, 2048 | Measured, 2048 seeds 0–7 / 0–19 | Proposed, 2048 | Why |
| --- | --- | --- | --- | --- |
| `lakes` | hard 4–400, typical 7–16 | 6–15 / 6–21 | typical **5–24** (hard unchanged) | four of 20 outside (80%) |
| `major_rivers` | hard 4–160, typical 32–43 | 32–43 / 28–54 | typical **25–60** (hard unchanged) | four of 20 outside (80%) |
| `land_above_m` 500 | hard 0.06–0.35, typical 0.08–0.30 | 0.074–0.200 / 0.033–0.200 | typical **0.066–0.23** (from the 19 worlds without s13; the strict rule with s13 gives 0.029–0.23); hard: see D2 | three of 20 outside (85%) |
| `elevation_m` p90 | hard 200–1300, typical 330–1100 m | 223–1018 / 127–1018 m | typical **200–1120 m** (19 worlds without s13: min 222.8 × 0.9); hard: see D2 | two of 20 outside (90%, at the limit) |
| `elevation_m` p100 | hard 1200–2800, typical 1500–2600 m | 2234–2456 / 2222–2931 m | typical **1990–2760 m** from the 19 worlds without s15 (2221.6 × 0.9 and 2507.9 × 1.1; with s15, 1990–3230 m); hard: see D1 | one of 20 outside |
| `land_below_0c` | typical 0.017–0.115 | 0.021–0.091 / 0.012–0.097 | typical **0.011–0.115** if s13 is kept | one of 20 outside (95%, holds) |
| `rainfall_mm` p50 | typical 585–750 mm | 648–683 / 630–714 | **unchanged** (holds for all 20; the rule would give 567–786) | |
| `rain_contrast` | typical 1.6–2.7 | 1.74–2.47 / 1.67–2.47 | **unchanged** | |
| `temperature_c` p50 | typical 4.5–7.2 °C | 5.9–6.5 / 5.8–6.5 | **unchanged** | |
| `river_share_of_land`, `lake_share_of_land`, `largest_basin_share`, `longest_river_m`, outline rows | | inside their ranges for all 20 | **unchanged** | |

The 2048 `rainfall_mm` p50 range could narrow to 565–790 by the rule, but the current range holds for every seed, so I would leave it. 2048 seeds 8–19 add the first NW worlds at 2048 (s8, s12) and no NE world. Wind directions over seeds 0–19: SE 6, W 4, S 4, N 2, NW 2, SW 1, E 1.

## 4. Biome statistics

Shares are of cells whose biome row has `water` = 0 (land and river cells), as `report` defines them; `biome_share` and `biome_patches` recomputed from the layer equal the printed text in all 120 worlds.

### 4.1 Shares (min / median / max)

| Biome | 512 (50) | 1024 (50) | 2048 (20) | Pooled land share, 512 / 1024 / 2048 |
| --- | --- | --- | --- | --- |
| temperate_forest | 0.272 / 0.425 / 0.611 | 0.347 / 0.448 / 0.635 | 0.424 / 0.494 / 0.576 | 0.430 / 0.468 / 0.498 |
| grassland | 0.165 / 0.326 / 0.585 | 0.166 / 0.317 / 0.447 | 0.257 / 0.319 / 0.420 | 0.338 / 0.314 / 0.333 |
| marsh | 0.001 / 0.012 / 0.025 | 0.009 / 0.024 / 0.039 | 0.034 / 0.054 / 0.064 | 0.012 / 0.024 / 0.053 |
| highland_moor | 0.041 / 0.132 / 0.287 | 0.043 / 0.105 / 0.253 | 0.021 / 0.062 / 0.111 | 0.142 / 0.115 / 0.063 |
| alpine | 0.040 / 0.075 / 0.123 | 0.038 / 0.077 / 0.126 | 0.014 / 0.047 / 0.104 | 0.077 / 0.079 / 0.053 |

- **Worlds missing a biome:** none. Every natural biome covers at least 0.1% of land in every world. The smallest shares are marsh at 512 s10 (0.12%), then 2048 s13's alpine (1.4%) and moor (2.1%).
- **Largest share in a world** (any biome): the top five at 512 are forest at s41 0.611 and s43 0.599, grassland at s36 0.585 and s1 0.548, and forest at s39 0.544. At 1024 and 2048 all are forest. At 1024: s43 0.635, s41 0.634, s38 0.617, s34 0.588, s32 0.563. At 2048: s10 0.576, s6 0.566, s5 0.557. No world has a biome above 0.65. The forest-heavy worlds are the weak-shadow worlds of P1-04 limitation 6 (512 s41, s43; 1024 s38, s41, s43).
- **Against the lead's target picture** (P1-04 11.1), worlds at 512 / 1024 / 2048:
  - Grassland above 35%: 21 / 16 / 8.
  - Forest outside 35–55%: 11 / 6 / 4 (2048: s5, s6, s10, s15, all above 55%).
  - Marsh under 1%: 18 / 2 / 0.
  - Marsh above 6%: 0 / 0 / 3 (2048 s4, s5, s7, at 6.4% at most).

  At 512 and 1024 these are the spec's counts. At 2048 the twenty worlds add more grassland-heavy worlds than the eight did (2 of 8 above 35%).

### 4.2 Patches

| | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| `biome_patches` | 53 / 81.5 / 123 | 228 / 301.5 / 393 | 1040 / 1257.5 / 1523 |
| forest patches; largest patch ÷ forest | 5 / 17 / 43; 0.29 / 0.85 / 1.00 | 31 / 53 / 102; 0.58 / 0.91 / 0.99 | 155 / 242 / 353; 0.54 / 0.93 / 0.97 |
| grassland patches; largest ÷ grassland | 10 / 18 / 42; 0.38 / 0.81 / 1.00 | 32 / 60 / 108; 0.34 / 0.83 / 0.98 | 190 / 265 / 374; 0.53 / 0.87 / 0.97 |
| marsh patches; largest ÷ marsh | 14 / 37 / 66; 0.10 / 0.24 / 0.56 | 88 / 171 / 254; 0.04 / 0.10 / 0.37 | 607 / 741 / 814; 0.03 / 0.04 / 0.21 |
| moor patches | 1 / 4 / 13 | 1 / 7 / 32 | 4 / 13 / 42 |
| alpine patches | 1 / 4 / 11 | 1 / 6 / 15 | 4 / 12 / 27 |

Patches are 8-connected components of equal code over land, as `biome_patches` counts them, with the patch sizes from a small C helper (`patches.c`, section 10). Most of the count is marsh: hundreds of small ribbons along class ≥ 3 rivers, with the largest marsh patch under 4% of a 2048 world's marsh in the median world. This is the world critic's item 3 (no fen big enough to be a region) in numbers.

### 4.3 Speckle

Share of land in patches of fewer than 16 cells: 0.07–0.29% at 512 (median 0.15%; worst s14 0.29%, s12 0.24%, s33 0.23%), 0.10–0.21% at 1024, 0.14–0.21% at 2048. This is within the spec's claim of at most 0.3% in every world (P1-04 11.2). Marsh makes up about half of it (median 52–56% of the small-patch cells at each size, range 23–83%). The rest is forest and grassland in about equal parts. Moor and alpine have essentially none. No world is speckled.

### 4.4 Lee and windward lowland

The measure is that of P1-03 11.1 and P1-04 11.1. A lowland cell is land, not lake, below 200 m. It is lee if tracing against the wind for up to 400 cells meets ground of 800 m or more. It is windward if it is not lee and tracing with the wind meets such ground. Shares are per world, min / median / max:

| | Lee lowland grassland | Windward lowland grassland | Lee lowland forest | Windward lowland forest |
| --- | --- | --- | --- | --- |
| 512 | 0.483 / 0.847 / 0.996 | 0.000 / 0.155 / 0.587 | 0.000 / 0.131 / 0.512 | 0.367 / 0.827 / 0.998 |
| 1024 | 0.485 / 0.810 / 0.989 | 0.001 / 0.168 / 0.421 | 0.000 / 0.164 / 0.508 | 0.519 / 0.800 / 0.989 |
| 2048 | 0.322 / 0.799 / 0.987 | 0.002 / 0.276 / 0.560 | 0.000 / 0.125 / 0.610 | 0.348 / 0.654 / 0.960 |

In all 120 worlds the lee lowland is grassier than the windward lowland. The spec's figures are reproduced: P1-04 11.1 gives lee 48 / 85 / 100% and windward 0 / 16 / 59% at 512, and lee 49 / 81 / 99% and windward 0 / 17 / 42% at 1024. At 2048 the windward lowland is grassier (median 28%) because the interior dries with path length (limitation 7). Worlds with lee lowland at or under 5% of land: 512 s1, s4, s12, s26; 1024 s4; 2048 s4 (the low-lee worlds of the climate validation).

**Coast rim** (limitation 5, critic item 2). This is the forest share of lee lowland by `coast_dist`, pooled. At 1024: 0.286 within 300 m of the coast, 0.188 at 0.3–1 km, 0.142 at 1–3 km, 0.152 beyond. At 2048: 0.332, 0.241, 0.141, 0.154. At 512: 0.187, 0.151, 0.165. So on lee coasts at 1024 and 2048 the first 300 m holds about twice the forest of the interior. That is a rim, though thinner than on windward coasts (0.96 within 300 m at 1024). A strong rim, more than 30 points above the lee lowland 1–3 km inland, appears in 4 / 6 / 5 worlds: 512 s30, s38, s40, s43; 1024 s2, s11, s18, s35, s40, s49; 2048 s2, s7, s9, s11, s15.

### 4.5 River cells (B5)

The biome of river cells, pooled, against land as a whole:

| | Forest | Grassland | Marsh | Moor | Alpine |
| --- | --- | --- | --- | --- | --- |
| River cells, 512 | 0.446 | 0.402 | 0.085 | 0.059 | 0.008 |
| All land, 512 | 0.430 | 0.338 | 0.012 | 0.142 | 0.077 |
| River cells, 1024 | 0.431 | 0.342 | 0.134 | 0.072 | 0.021 |
| All land, 1024 | 0.468 | 0.314 | 0.024 | 0.115 | 0.079 |
| River cells, 2048 | 0.412 | 0.286 | 0.247 | 0.039 | 0.016 |
| All land, 2048 | 0.498 | 0.333 | 0.053 | 0.063 | 0.053 |

River cells take their banks' biome. They are marsh five to ten times as often as land as a whole, because marsh lines class ≥ 3 rivers. They are seldom alpine, since rivers start below the peaks. On the composite the river cell is drawn as a river, so B5 is invisible on the map. It matters for P1-05 and P1-06.

## 5. Images and what they show

Every sweep world was rendered with `worldgen-cli render --composite biomes` into `~/.cache/the-unmapped/out/p1-04-validation/renders/`. Contact sheets in `…/sheets/` label each tile with seed and size, the wind (name, plus an arrow whose red head points where the wind blows to) and the five shares:

- `1024-biomes-seeds-0-24.png` and `1024-biomes-seeds-25-49.png`, 200 px tiles;
- `512-biomes-seeds-0-49.png`, 128 px tiles;
- `2048-biomes-seeds-0-19.png`, 256 px tiles.

Crops at 2× are in `…/crops/` (`p1-04-biome-crops.png`, plus the working crops `work1.png`, `work2.png` and `s5-map-vs-biomes.png`). What I see:

- **Rain shadows read as biomes.** On the sheets, forest covers the face the arrow points into and grassland lies beyond it. In every one of the 120 worlds the lee lowland is grassier than the windward lowland (4.4). The strongest contrasts (lee minus windward lowland grassland share, worlds whose lee is over 8% of land) are 1024 s19 0.93, s7 0.88, s46 0.87, s20 0.85; 512 s13 0.91, s7 0.91, s46 0.90; 2048 s17 0.85, s0 0.74. The weakest are 512 s45 0.19, 2048 s10 0.13 (a forest-heavy world), and 1024 s26, s37, s3 at about 0.37. The weak-shadow worlds are almost all forest (1024 s38, s41, s43; 512 s41, s43), as limitation 6 says.
- **Forest–grassland edges** break into groves and clearings at walking scale, with ragged outlines and no speckle (crop 1024 s3). At 2× the edges are cell-stepped but not blocky.
- **Alpine caps and moor rings** are smooth contour bands on every range (limitation 4, critic item 1). On 1024 s5 (cells 560–730, 340–435) the moor and alpine form a near-rectangle with a straight vertical east side and a straight horizontal south edge, visible at sheet scale. The relief map shows the same shelf in the elevation layer (`s5-map-vs-biomes.png`), so the biome rules are only colouring a straight-sided terrain feature. This is an elevation matter: P1-01's limitation list mentions "the floor's shapes in a few worlds", and this may be one of them. At cell scale the edge is ragged, and the longest exactly straight axis-aligned biome edge in any world is 43 cells (median 20–21 cells per world), so it is not a seam. For the critic.
- **Marsh** is thin grey ribbons along class ≥ 3 rivers and round lake shores. Even the largest 1024 marsh (s49) is a ribbon network, not a fen. The 2048 interiors carry a mesh of marsh threads (s4, s5, s7, all near 6.4%), as limitation 3 says. Some marsh margins send one-cell filaments into the grassland along flat valley lines (1024 s49 around (650, 410)). That is cosmetic, and within the speckle measure.
- **Lee coasts:** some have no rim (1024 s8's north-west coast is grassland to the shore); others have a forest rim, measured in 4.4 (1024 s40's south coast is the crop's example).
- **Rivers through grassland** carry no trees and no marsh below class 3 (1024 s19, s3), as the critic's item 4 notes.
- **Seams:** no straight biome edges along rows or columns that the terrain does not explain, and nothing at multiples of 64 cells on any sheet. Following the climate validation's correction, I did not fold differences by position mod 64. The straight-edge measure above compares each cell with its own neighbours.
- **2048** maps read as "wet coastal rim, open interior with parkland" (limitation 7). s13 (one short coastal range in a lowland continent) has almost no upland biome.

Copied into the repository (600 KB):

- `images/p1-04/p1-04-1024-biomes-seeds-0-24.jpg`: composite biomes, 1024 seeds 0–24, 200 px tiles, wind and shares per tile.
- `images/p1-04/p1-04-2048-biomes-seeds-0-19.jpg`: the same for 2048 seeds 0–19, 200 px tiles.
- `images/p1-04/p1-04-biome-crops.jpg`: six 192-cell crops at 2×. They show a forest–grassland edge (1024 s3), a treeline (1024 s5, the shelf above), the largest 1024 marsh (s49), 2048 marsh threads (s4), a lee coast with a rim (1024 s40, wind S) and a river through grassland (1024 s19).

## 6. Thread independence

Worlds:

- 512 seeds 0 (wind SE), 6 (E), 8 (NW) and 14 (S);
- 1024 seeds 1 (SW) and 5 (N);
- 2048 seeds 1 (SW) and 13 (W, the flat world of D2).

Each was generated with `--threads` 1, 2, 5 and 8 by both `linux-gcc-release` and `linux-clang-release`. All 64 files have the SHA-256 of the same world in the gcc-release sweep (default threads). The sample covers four orthogonal and four diagonal winds and all three sizes. Together with the five-preset hash comparison, the biome layer depends neither on the thread count nor on the compiler.

## 7. Performance

**The machine was not fully quiet.** Another session (the P1-04b variety work, `agent-a04bc0f…`) started reading the sweep worlds at nice 10 while the timings ran, using about two of the six cores at times. Each figure below says whether it was affected. All runs use gcc-release with 6 threads, one world at a time.

**The stage alone.** `biome_layer` (B2–B5: G7, G8, both 4-octave jitter fields, the lookup and B5) was timed by a scratch driver (`biotime.cpp`, section 10) on each world's stored input layers. The driver links `libworldgen.a`, loads the shipped data and runs five repeats; every repeat's output equals the stored `biome` layer. Encoding and writing the one `u8` layer and the eight-row table are not included; at 2048 the whole `biome` blob is under 0.2 MB. The figures were taken while the other session's niced jobs were running, so they are upper bounds.

| Size, seeds | `biome_layer`, best of 5 | All runs | Budget (P1-00 11) | Spec's estimate (P1-04 8) |
| --- | --- | --- | --- | --- |
| 512: 0, 5, 13, 41 | 84–111 ms | 84–159 ms | 0.1 s | 0.1 s |
| 1024: 0, 3, 15, 41 | 263–285 ms | 262–414 ms | 0.5 s | 0.4 s |
| 2048: 0, 7 | 923–957 ms | 922–1057 ms | 2 s | 1.5 s |

**End to end.** Full `generate` against `--until climate`, interleaved, wall time and peak RSS by `/usr/bin/time`. The 512 and 1024 runs have three repeats per world; the first two were quiet and the third overlapped the other session. The 2048 runs have four repeats per world, all under the other session's load.

| Size | Full `generate` (wall) | `--until climate` | Difference, min of each | Peak RSS, full / until climate |
| --- | --- | --- | --- | --- |
| 512 | 2.15–2.63 s | 2.08–2.55 s | −0.02 to 0.15 s (median 0.09 s) | 28.3–30.1 / 28.0–30.1 MB |
| 1024 | 7.59–9.67 s | 7.40–9.40 s | 0.13–0.20 s | 92.4–94.7 / 92.3–94.7 MB |
| 2048 | 32.6–38.9 s | 29.7–40.2 s | within noise (±5 s between repeats) | 349.7–350.0 / 349.8–350.0 MB (s0); 376.3–376.6 / 376.5–376.7 MB (s7) |

- **Time.** The stage is within budget at 1024 (about 55%) and 2048 (under 50%). At 512 it is at its budget: best-of-5 figures of 84–111 ms against 0.1 s, under contention. The end-to-end differences agree: about 0.1 s at 512 and about 0.2 s at 1024. Those are smaller than `biome_layer` alone at 1024, because the extra `climate` row and the `--until` bookkeeping are in both runs and the noise is about ±0.3 s. This is not a defect (P1-00 11 calls a stage over twice its target at 1024 a defect). If the 512 budget is to be held strictly, the spec's fallback is to drop the jitter to 3 octaves (P1-04 8).
- **Memory.** Peak RSS is the same with and without the stage at every size, and the same as 0.7.0's (28.0–29.9 / 91.9–94.5 / 348.6–375.5 MB): the stage's grids are smaller than the elevation stage's peak.
- **Whole of geography.** Four stages so far take 2.2–2.6 / 7.6–9.7 / 32.6–38.9 s, against budgets of 4 / 15 / 65 s.
- **The sweep's own times** (under heavy load): 512 2.0–2.6 s, 1024 8.3–17.3 s, 2048 38.0–58.9 s; peak RSS 28.1–29.9 / 91.4–95.0 / 346.9–376.5 MB.

## 8. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` was re-recorded for 0.8.0 after the five-preset comparison passed. The output legitimately changed for four reasons. Generator 0.8.0 adds the biome stage to the default pipeline, so each geography case gains the `biome` layer and `meta.stages` becomes `elevation,hydrology,climate,biomes`. Schema 3 adds tables `biome` and `climate`, now filled. The climate stage writes its `climate` row. Registering the `biome` kind and the `worldgen.biomes` id changes `content_hash` (P1-04 4.2). Every layer of 0.7.0 and table `pass` are byte-identical (section 1).

| Case | File hash (0.8.0) |
| --- | --- |
| `geo-phrase-512` | `a97678fa939098befb5a59622711e2a3d24dd22915b2b48591434914f8c769d8` |
| `geo-seed-0-512` | `1f399532b63488e69e904bd7fa49e9932cea4e87f94424ec788d3e2edb117f10` |
| `geo-seed-1-1024` | `8105663bb5f1822fe567d5a34f1337394f5544f0844498ae56140b8f9ced27f0` |
| `geo-seed-42-512` | `7bc71931e69eff8902199925c52592b94a28bf9a3939b8aa34e79998524535a4` |

`geo-seed-0-512` equals the sweep's 512 seed 0 file hash. The engineer's `p0-worlds.txt` for 0.8.0 is confirmed unchanged (section 1); its eight values are those of the five-preset `local` run. The biome layer and table hashes of the geography cases come from the independent reference in `worldgen/tests/golden/p1-04-biomes.txt` (P1-04 10.4) and are checked by `biomes-slow` and the default suite (section 1). They are not repeated here: the reference also matched every cell of the 120 sweep worlds, which covers the golden cases' sizes. The MSVC presets are compared by CI's `compare-hashes`.

## 9. Defects and findings

No defect in the biome stage or the schema-3 change. There is no cross-preset or thread-count difference, no test failure, no table or layer rule broken, no change to any upstream layer, and the biome layer equals the independent reference on every cell of the sweep.

Two defect reports against **P1-00 13.1 hard bounds in the elevation rows**. They were found because this sweep is the first with 2048 seeds 8–19. Both worlds are byte-identical in every upstream layer under 0.7.0 (section 1), so neither comes from this change. They are not blocking for P1-04 in my reading, but 13.1 says a seed outside a hard bound is a defect, so they go to the lead for routing: elevation engineer, or a range decision.

### D1. 2048 seed 15: `elevation_m` p100 2931.1 m, above the hard bound 2800 m

- **World:** seed 15, `--map-size 2048`, shipped `game/data` (content hash `5129d642…14218938`), generator 0.8.0 (identical elevation in 0.7.0).
- **Reproduce** (from the repository root, with a built `linux-gcc-release`):

  ```
  B=~/.cache/the-unmapped/build/$(basename "$PWD")/linux-gcc-release/tools/worldgen-cli/worldgen-cli
  "$B" generate --seed 15 --map-size 2048 --until elevation --out ~/.cache/the-unmapped/out/d1.db --force && "$B" report --db ~/.cache/the-unmapped/out/d1.db | grep '^elevation_m'
  ```

- **Expected** (P1-00 13.1, row `elevation_m` p100): 1200–2800 m for all seeds; typical 1500–2600 m.
- **Actual:** `elevation_m = 0.1 1.3 24.5 55.5 87.2 111.2 467.9 1657.9 2931.1`. The highest cell is 2931.1 m at (751, 753). 59 cells lie above 2800 m and 201 above 2600 m, in a smooth massif, not a spike: the 7 × 7 neighbourhood of the summit runs 2549–2931 m with no step over 90 m. The next-highest world at any size is 2584.0 m (1024 s12); at 2048 it is 2507.9 m (s16).
- **Narrowed to:** the elevation stage's height scaling, as far as I can tell from reading, not running, the code. `worldgen/src/stages/elevation.cpp` around line 707 scales mountain heights by `mountain_height_m ÷ q`, where q is the land quantile at `mountain_peak_rank` 0.998. That fixes the 99.8th percentile of the mountain field at 1900 m and leaves the top 0.2% unbounded above, so a world with a long upper tail gets a summit far above 1900 m: here 1.54 × the rank height. Whether to cap peaks (a tuning or spec change in P1-01) or to raise the hard bound (for example ≤ 3000 m at 2048) is a decision for the lead and the elevation spec. The `i16` layer allows up to 3276.7 m, so nothing overflows.

### D2. 2048 seed 13: `elevation_m` p90 126.8 m (hard ≥ 200 m at 2048) and `land_above_m` 500 0.0328 (hard ≥ 0.06)

- **World:** seed 13, `--map-size 2048`, shipped data, generator 0.8.0 (identical elevation in 0.7.0).
- **Reproduce:** as D1 with `--seed 13`, and `grep -E '^elevation_m|^land_above_m'`.
- **Expected** (P1-00 13.1, rows `elevation_m` p90 and `land_above_m` 500; proposals "Every world has real uplands"): p90 ≥ 200 m at 2048, and `land_above_m` 500 ≥ 0.06 at every size.
- **Actual:** `elevation_m = 0.1 2.0 23.9 54.3 84.7 108.0 126.8 1198.2 2418.5` and `land_above_m = 50:0.7730 500:0.0328 1000:0.0140 1500:0.0066 2000:0.0018`. The world is one broad lowland continent with a single short range on the east coast and an isolated massif in the south (tile 2048 s13 of the 2048 sheet). As a result its biomes are 1.4% alpine and 2.1% moor, both the lowest of the sweep.
- **Narrowed to:** this is the pattern that set the 2048 p90 hard bound at 200 m (2048 seed 5, 222.8 m, P1-00 13.1 "one long range on a broad continent puts its 90th percentile on the lowland"), in a more extreme form. Here the belt is short as well as narrow relative to the map. The mountain floor (P1-01 E5.1) guarantees core area but not p90, and evidently not 6% of land above 500 m at 2048 either. Root cause not determined beyond that. It needs the elevation side: either the floor's guaranteed area should scale with N², or the 2048 bounds should accept such worlds (p90 ≥ 120 m, `land_above_m` 500 ≥ 0.03).

### Findings (not defects)

1. **Biome ranges** (3.3): proposed final typical ranges per size. The provisional 2048 moor and alpine floors are missed only by D2's world.
2. **2048 ranges with twenty seeds** (3.4): `lakes`, `major_rivers` and `land_above_m` 500 hold for only 80–85% of 2048 worlds. Corrections are proposed.
3. **1024 s5 moor and alpine shelf** (section 5): a straight-sided upland in the elevation layer, painted by the moor rule. For the critic and the elevation side.
4. **Lee-coast forest rim** (4.4): measured. Lee lowland within 300 m of the coast holds twice the forest of the lee interior at 1024 and 2048, and a strong rim appears in 15 of 120 worlds. It belongs to the climate stage's coast bonus (P1-04 limitation 5, critic item 2).
5. **Marsh is ribbons** (4.2): the largest marsh patch is a median 4% of a 2048 world's marsh, and the most marsh patches in one world is 814. This is critic item 3 in numbers.
6. The previous validation's cosmetic finding 3 is fixed: `render --help` now lists `temperature`, `rainfall` and `biome`.
7. **`CLAUDE.md` `ctest` times.** The table gives about 110 s, 4.5 minutes and 18 minutes, measured at generator 0.4.0. This run measured 143–146 s, 592–609 s and 3325 s, all under load. The 0.7.0 validation measured asan at 2390 s under load, and the lead's own estimate for asan is about 40 minutes. The debug and asan rows are probably stale, but I could not take a quiet measurement while another session was running, so the table is unchanged. One quiet run of `ctest --preset linux-clang-asan --timeout 3600` on an idle machine would settle it. I added to `CLAUDE.md` only the stage golden-vector files (`p1-01-elevation.txt` to `p1-04-biomes.txt`), which its "Golden values" list did not mention. Its schema-version statement (3) is correct.
8. **Process note:** the shared scratchpad directory holds other sessions' files. While setting up, my sweep script overwrote `scratchpad/tools/sweep.sh` (an earlier session's file, last written 14:05) before I moved my work to `scratchpad/p104val/`. If another session needs that script, it has to be rewritten. Nothing else of theirs was changed.

## 10. Where things are

- Sweep worlds, reports, `hash --detail`, stderr and `/usr/bin/time` per world: `~/.cache/the-unmapped/out/p1-04-validation/{512,1024,2048}/seed-<s>.*`. The gcc-release binary used is `…/worldgen-cli-0.8.0-gcc-release`.
- Analysis: `…/analysis/a{512,1024,2048}.jsonl` (per-world checks and statistics), `summary.txt` (the tables of section 3), `ref/<N>-<seed>.txt` (reference comparisons), `old070-2048.txt` (0.7.0 comparison of 2048 seeds 8–19), `rim2048.txt`, `threads.txt`.
- Renders, sheets, crops: `…/renders/`, `…/sheets/`, `…/crops/`. Timing runs: `…/timing/`.
- World-hash case files: `~/.cache/the-unmapped/out/agent-a0386ad1bc93d8d5b/world-hashes/`.
- Scripts (scratch, not in the repository): `/tmp/claude-1000/-home-pgratz-Dropbox-Documents-MyCode-The-UnMapped/ce48c02e-8da2-4932-954b-fdafdbb43139/scratchpad/p104val/mytools/`:
  - `sweep.sh`;
  - `analyze.py` (tables, layer rules, shares, patches, lee/windward, upstream identity);
  - `refcmp.py` and `refone.sh` (the reference cross-check);
  - `patches.c`, `summary.py`, `biostats.py`, `rim.py`, `straight.py`, `threads.sh`, `timing.sh`, `old070.sh`, `p0check.sh`, `sheet.py`, `crops.py`, `findcrops.py`, `wdecode.py`.
