# Validation: P1-04 biome variety, per-world temperature offset, schema 4 (generator 0.9.0)

Validation specialist, 2026-11-18. Commit validated: `77b7a5d` (local `main`, "World critic review of the biome variety revision"; also on origin as `p1/04b-variety-final`). `GENERATOR_VERSION` 0.9.0, schema version 4, shipped `game/data` with content hash `93238a8f…db1562e6`. Specs on that commit: `docs/specs/p1-04-biomes.md` (third version, amended 2026-11-18), `docs/specs/p1-03-climate.md` (C2b, section 9), `docs/specs/p1-00-geography-overview.md` (11, 13.1 as revised 2026-11-18, 14 X5). Determinism audit: `docs/reviews/p1-04b-biome-variety-determinism-audit.md` (pass). World critic: `docs/reviews/p1-04b-biome-variety-critic.md` (merge as is). Machine: 6-core Linux desktop, 31 GB. Builds, `ctest`, the sweep and the reference checks ran together (load 9 to 18). The timings of section 7 were taken afterwards with nothing of mine running (load 2.3 to 4.0 from other applications). Windows MSVC is CI's.

**Verdict: pass.**

- **Builds and tests.** All five Linux presets build with warnings as errors. `ctest` passes on gcc-release, clang-release, gcc-debug and clang-debug, and the slow suites pass on gcc-release. **`ctest` was not re-run on `linux-clang-asan`**: between the test writer's asan run on `5ef901d` and `77b7a5d` only `docs/` changed (`git diff --stat 5ef901d 77b7a5d`: two reviews and four specs). The asan build was rebuilt and generated all twelve world-hash cases (below).
- **Hashes.** The twelve CI cases are identical on the five Linux presets. `p1-worlds.txt` is re-recorded, and `expect` gives 12 of 12 on every preset. The committed `p0-worlds.txt` (re-recorded by the test writer in `b86fed6`) is confirmed. Empty worlds have 8 pages, 32768 bytes and `user_version` 4.
- **Determinism.** Neither the thread count nor the compiler changes output (64 of 64 files).
- **Reference checks.** The `temperature`, `rainfall` and `biome` layers and the `climate` row equal the test writer's independent references on every cell of every checked world (section 1).
- **Upstream identity.** In the 120 worlds that have a 0.8.0 counterpart, every elevation and hydrology layer, table `pass`, `wind_toward` and every elevation and hydrology `report` line equal 0.8.0's (P1-04 invariant 9).
- **Bounds.** No hard bound is violated by any of the 271 sweep worlds. The three 2048 rows that Paul made descriptive are missed by 2048 s13 and s15, as in 0.8.0; they are notes (section 3.3).
- **Cost.** The stage takes 0.07 / 0.22 / 0.81 s against budgets of 0.15 / 0.5 / 2 s. Peak memory is unchanged.

No defect. One typical range holds for fewer than 90% of seeds (steppe at 1024, 88% of 200 seeds), and several 1024 ranges change with the larger sample. Final ranges are proposed in section 3.4.

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug`, `linux-clang-debug`, `linux-clang-asan` | pass, all five (82–106 s each with ccache) |
| `ctest` (4 tests each), under load 9–18 | pass: gcc-release 293 s, clang-release 276 s, gcc-debug 964 s, clang-debug 818 s (`worldgen-tests` 211 / 185 / 705 / 561 s). Asan not re-run (see verdict); the test writer's run on `5ef901d` passed (about 42 minutes) |
| Slow suites, `linux-gcc-release` (`-ts=<suite> --no-skip`) | pass. Library: `elevation-slow` 10 cases, 95,017,939 assertions, 567 s; `hydrology-slow` 5, 3,910, 119 s; `climate-slow` 8, 1,058,054, 303 s; `biomes-slow` 5, 5,843, 75 s. CLI: `hydrology-slow` 1, 23; `climate-slow` 1, 32; `biomes-slow` 1, 12 (22–25 s each) |
| `determinism_lint.py --self-test` and the scan | pass: 13 snippets; 120 C++ and 11 CMake files, 0 findings |
| `tools/ci/world-hashes.sh local` over the five presets | pass: all 12 cases identical (`hash --detail` and `report` text), 848 s under load |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on all five |
| Committed `p0-worlds.txt` | confirmed: equals the eight P0 cases on all five presets. `p0_world_check.py check … --detail --report --generator-version 0.9.0 --content-hash 93238a8f…db1562e6` passes for all eight on gcc-release and clang-asan worlds, and fails as it should with `--generator-version 0.8.0`. Header: `pages=8 … size=32768` |
| Geography cases against P1-04 10.4 | pass: in all four cases (512 s0, s42, phrase; 1024 s1) the hashes of `temperature`, `rainfall`, `biome`, table `biome` (`a7878ee4…`) and table `climate`, `wind_toward` and the offset (`2.0`, `0.9`, `-1.0`, `1.0`) equal the exploration build's values in the spec. 512 s0 prints the spec's `biome_share` line exactly |
| Seed sweep (gcc-release binary copied to the output folder, default threads, two worlds at a time): 512 seeds 0–49, **1024 seeds 0–199** (P1-00 13.1's full set), 2048 seeds 0–19, phrase "The Unmapped" at 1024 | 271 of 271 `generate`, `report` and `hash --detail` succeed; stderr has only the three `--verbose` lines |
| Per-world file checks (`analyze.py`, section 10) | pass in all 271 worlds (section 2) |
| Climate reference (`p1_climate_reference.climate`, C1–C6 with C2b, on each file's own elevation and hydrology layers) | pass: `temperature` and `rainfall` equal on every cell and the `climate` row equals `(1, wind, offset)`. Run on 512 seeds 0–49, 1024 seeds 0–49 and the phrase, and 2048 seeds 0–19 (121 worlds) |
| Biome reference (`p1_biomes_reference.biomes`, B2–B5 with B2b and B2c, on each file's own input layers) | pass: `biome` equal on every cell; `biome_share` and `biome_patches` text equal to `report`. Same 121 worlds |
| Upstream identity against the 0.8.0 sweep (`~/.cache/the-unmapped/out/p1-04-validation/`: 512 and 1024 seeds 0–49, 2048 seeds 0–19) | pass in all 120 worlds. Hashes of the eight elevation and hydrology layers and table `pass` are equal. Every `[geography]` line of the 0.8.0 `report` except the climate and biome keys is equal, `wind_toward` included. The only hash lines that change are `file`, `schema`, `table meta`, `table layer`, `table biome`, `table climate`, `layer temperature`, `layer rainfall` and `layer biome`, the set of P1-04 invariant 9. The only new key is `temp_offset_c`. 1024 seeds 50–199 have no 0.8.0 world |
| Thread independence (section 6) | pass: 64 of 64 files equal the sweep's |

## 2. Tables and layer rules

Checked from the files of every sweep world (271), with the numpy decoder of the P0-02 codec:

- **meta**: `schema_version` 4, `generator_version` 0.9.0.
- **`climate`**: exactly one row `(1, k, off)`. k is `p1_climate_reference.wind(seed)` and off is `offset_dc(seed)` (C2b, `temp_offset_max_c` 2). `report` prints `wind_toward` and `temp_offset_c` with the `metres1` format (for example `-1.0`, `2.0`). `[tables]` lists `biome = 12` and `climate = 1`.
- **`biome`**: the twelve rows `(id, key, name, colour, water)` equal the definitions parsed from `game/data/biomes/*.toml`, in ascending `id`.
- **Layer `biome`**: `u8`, N × N. Every value has a row and none is 0. Ocean's code lies exactly on `water` = 3 and lake's exactly on `water` = 2. Every land or river cell has a land biome. `blighted_waste` covers 0 cells.
- **Invariant 4** (rules and B5 on every cell) and **invariant 8** (B2b, B2c) are covered by the reference comparison, which recomputes G7, G8, B2b, B2c, the jitter, B4 and B5 exactly.
- **Climate layers**: `temperature` lies in [−600, 500] and `rainfall` is at most 10000 in every world.
- **Land temperature** (P1-03 9 hard bound: every land cell ≤ 12.0 °C): the maximum is 10.7 / 11.2 / 10.8 °C at 512 / 1024 / 2048.

## 3. Statistics against P1-00 13.1

Values are exact. Ratios are computed from their integer counts, and biome shares from the decoded layer: cells with each code ÷ cells whose biome row has `water` = 0, as `report` defines them. Recomputed shares equal the printed text in every world. 1024 has 200 seeds; 512 has 50 and 2048 has 20.

### 3.1 Hard bounds: none violated

| Statistic | Hard bound | 512 (50) min / med / max | 1024 (200) | 2048 (20) | Violations |
| --- | --- | --- | --- | --- | --- |
| `temp_offset_c` | within ±2.0 | −1.9 / 0.3 / 2.0 | −2.0 / 0.2 / 2.0 | −1.9 / 0.45 / 2.0 | none |
| land temperature maximum | ≤ 12.0 °C | 6.7 / 8.9 / 10.7 | 6.3 / 8.85 / 11.2 | 6.9 / 9.2 / 10.8 | none |
| `temperature_c` p50 | catalogue | 3.7 / 6.15 / 8.3 | 3.7 / 6.1 / 8.1 | 4.1 / 6.55 / 7.9 | none |
| `rainfall_mm` p50 | ≥ 250 | 732 / 806.5 / 903 | 634 / 736 / 814 | 627 / 669.5 / 705 | none |
| `rain_contrast` | descriptive | 1.87 / 3.15 / 4.74 | 1.58 / 2.40 / 3.62 | 1.66 / 1.93 / 2.48 | — |
| `land_below_0c` | descriptive | 0.0000 / 0.0600 / 0.1477 | 0.0017 / 0.0597 / 0.1636 | 0.0148 / 0.0374 / 0.1013 | — |
| `biome_share` temperate_forest | ≤ 0.80 | 0.240 / 0.374 / 0.581 | 0.210 / 0.422 / 0.612 | 0.400 / 0.485 / 0.576 | none |
| `biome_share` grassland | ≤ 0.80 | 0.141 / 0.277 / 0.490 | 0.115 / 0.228 / 0.438 | 0.125 / 0.178 / 0.281 | none |
| `biome_share` steppe | ≤ 0.80 | 0.0009 / 0.082 / 0.219 | 0.0064 / 0.102 / 0.304 | 0.043 / 0.126 / 0.205 | none |
| `biome_share` wet_woodland | ≤ 0.15 | 0.0011 / 0.0100 / 0.0222 | 0.0042 / 0.0145 / 0.0280 | 0.0143 / 0.0210 / 0.0304 | none |
| `biome_share` marsh | ≤ 0.15 | 0.0012 / 0.0078 / 0.0143 | 0.0042 / 0.0084 / 0.0170 | 0.0232 / 0.0262 / 0.0332 | none |
| `biome_share` fen | ≤ 0.15 | 0 / 0.0073 / 0.0362 | 0.0009 / 0.0370 / 0.0944 | 0.0227 / 0.0572 / 0.0763 | none |
| `biome_share` highland_moor | ≤ 0.80 | 0.018 / 0.071 / 0.156 | 0.017 / 0.058 / 0.158 | 0.015 / 0.035 / 0.080 | none |
| `biome_share` heath | ≤ 0.80 | 0.015 / 0.060 / 0.135 | 0.0096 / 0.044 / 0.190 | 0.0024 / 0.021 / 0.040 | none |
| `biome_share` alpine | ≤ 0.80 | 0.0014 / 0.074 / 0.180 | 0.0085 / 0.070 / 0.190 | 0.018 / 0.045 / 0.107 | none |
| `biome_share` blighted_waste | 0 | 0 | 0 | 0 | none |
| `biome_patches` | descriptive | 93 / 145 / 182 | 360 / 559 / 753 | 1932 / 2411 / 2786 | — |

Elevation and hydrology rows: no hard violation in any world. They are byte-identical to 0.8.0 for the 120 worlds that have one, so their 512 and 2048 figures are those of `p1-04-biomes-validation.md` 3.1. New at 1024 with seeds 50–199 (all inside the hard bounds):

- `elevation_m` p90: 317.6–1118.4 m (s144 at 317.6 m is the closest to the 300 m floor).
- `elevation_m` p100: 1415.8–2584.0 m.
- `land_above_m` 500: 0.0732–0.3455.
- `land_fraction`: 0.3974–0.4317.
- `coast_ratio`: 1.513–2.869.
- `land_solidity`: 0.735–0.924.
- `largest_basin_share`: up to 0.521 (s103).
- `lakes`: 2–16. `major_rivers`: 8–22.

The full table is in `analysis/summary.txt` (section 10).

### 3.2 Biome presence and patches

| Biome | Worlds with 0 cells, 512 / 1024 / 2048 | Under 0.1% of land | Patches per world, min / med / max, 512 / 1024 / 2048 | Largest patch ÷ biome (median) |
| --- | --- | --- | --- | --- |
| temperate_forest | 0 / 0 / 0 | none | 6/17/36; 19/64/141; 236/311/450 | 0.80 / 0.86 / 0.91 |
| grassland | 0 / 0 / 0 | none | 17/29/47; 60/110/187; 317/407/528 | 0.55 / 0.45 / 0.31 |
| steppe | 0 / 0 / 0 | 512 s30 (0.09%) | 2/16/39; 11/49/124; 120/178/260 | 0.61 / 0.69 / 0.69 |
| wet_woodland | 0 / 0 / 0 | none | 5/18/38; 35/89/165; 245/379/524 | 0.35 / 0.15 / 0.07 |
| marsh | 0 / 0 / 0 | none | 14/35/60; 83/133/181; 641/769/894 | 0.22 / 0.11 / 0.03 |
| fen | **4 / 0 / 0** (512 s0, s2, s10, s13) | 512: 10 worlds (s0, 2, 10, 13, 17, 28, 29, 40, 42, 45); 1024: s2 (0.09%) | 0/5.5/23; 10/70/133; 123/219/276 | 0.59 / 0.21 / 0.09 |
| highland_moor | 0 / 0 / 0 | none | 1/7/17; 1/13/46; 14/31/62 | 0.61 / 0.49 / 0.42 |
| heath | 0 / 0 / 0 | none | 1/6/17; 2/14/39; 7/24/65 | 0.62 / 0.50 / 0.40 |
| alpine | 0 / 0 / 0 | none | 1/4/10; 1/6/19; 4/10/35 | 0.80 / 0.68 / 0.62 |

- **Fen is absent from 4 of 50 worlds at 512 and under 0.1% of land in 10.** These are the spec's accepted cases (limitation 2, appendix A), seeds 0, 2, 10 and 13 among them, as the calibration found. At 1024 it is present in all 200 worlds and under 0.1% only in s2, as the calibration found too. At 2048 it is present everywhere at 2.3% or more.
- **Steppe is nearly absent in 512 s30** (0.09%). The next lowest are 1024 s184 at 0.64% and s170 at 0.72%. These are weak-shadow, forest-and-grassland worlds.
- **Alpine is smallest in 512 s28** (0.14%). This is a warm (+1.7 °C), low world (peak 1399 m) with **no land below 0 °C** (`land_below_0c` 0.0000, the only such world). This is the intended effect of the offset (P1-04 limitation 9), but it is the first world with zero frozen land.
- **The largest single share** in any world is forest at 0.612 (1024 s43). The other high ones are 1024 s194 0.607, s125 0.606 and s38 0.605, and 512 s0 0.581. No biome exceeds 0.62 anywhere.
- **Speckle** (land in patches of fewer than 16 cells): 0.17–0.43% at 512 (median 0.28%, worst s1), 0.17–0.43% at 1024 (median 0.29%, worst s74) and 0.23–0.39% at 2048 (median 0.34%, worst s7). All are under the spec's 0.5% (limitation 10). Marsh makes up the largest part of it (median 25–31% of small-patch cells), then grassland (19–21%), forest and wet woodland (11–17% each).
- **Lee and windward lowland** (the measure of P1-04 11.1): in all 270 numbered worlds, grassland plus steppe takes a larger share of the lee lowland than of the windward lowland. Medians, lee against windward:

  | Size | Lee steppe | Lee grassland | Windward steppe | Windward grassland |
  | --- | --- | --- | --- | --- |
  | 512 | 0.31 | 0.51 | 0.00 | 0.19 |
  | 1024 | 0.36 | 0.39 | 0.00 | 0.10 |
  | 2048 | 0.38 | 0.28 | 0.03 | 0.16 |

### 3.3 Paul's 2048 descriptive sides (notes, not failures)

| Row | Bound (now descriptive at 2048) | Worlds outside | Value |
| --- | --- | --- | --- |
| `elevation_m` p90 minimum | 200 m | 2048 s13 | 126.8 m |
| `land_above_m` 500 minimum | 0.06 | 2048 s13 | 0.0328 |
| `elevation_m` p100 maximum | 2800 m | 2048 s15 | 2931.1 m |

These are the same two worlds and values as in 0.8.0's D1 and D2, byte-identical in elevation. Under 0.9.0, s13 has alpine 0.021, moor 0.022 and heath 0.0024. Its heath is the lowest at 2048, by far (next: s15 0.0195). These are within the 2048 rows whose minimum is descriptive.

### 3.4 Typical ranges: misses and proposed final values

The ranges in force are the spec writer's provisional ones from 58 calibration worlds. Holds-for rates over this sweep, 512 / 1024 / 2048:

| Statistic | Typical now | Misses (seed value) | Holds for |
| --- | --- | --- | --- |
| temperate_forest | 0.27–0.64 / 0.29–0.61 / 0.39–0.61 | 512 s36 0.240, s35 0.261, s46 0.270; 1024 nine below 0.29 (lowest s132 0.210), s43 0.612 | 94% / 95% / 100% |
| grassland | 0.12–0.51 / 0.10–0.39 / 0.11–0.25 | 1024 s184 0.438; 2048 s13 0.281 | 100% / 100% / 95% |
| **steppe** | 0.007–0.25 / 0.033–0.19 / 0.038–0.18 | 512 s30 0.0009; **1024: 12 below 0.033 (lowest s184 0.0064), 11 above 0.19 (highest s103 0.304)**; 2048 s8 0.193, s11 0.205 | 98% / **88%** / 90% |
| wet_woodland | 0.0009–0.025 / 0.004–0.026 / 0.013–0.026 | 1024 s103 0.028; 2048 s11 0.028, s18 0.030 | 100% / 100% / 90% |
| marsh | 0.001–0.016 / 0.0037–0.013 / 0.020–0.034 | 1024 13 worlds 0.0131–0.0170 | 100% / 94% / 100% |
| fen | 0–0.035 / 0.0008–0.060 / 0.020–0.084 | 512 s23 0.0360, s35 0.0362; 1024 20 worlds 0.0603–0.0944 | 96% / 90% / 100% |
| highland_moor | 0.018–0.18 / 0.014–0.13 / ≤ 0.078 | 1024 s54 0.134, s30 0.158; 2048 s19 0.0796 | 100% / 99% / 95% |
| heath | 0.013–0.15 / 0.018–0.11 / ≤ 0.045 | 1024 six below 0.018 (lowest s184 0.0096), s185 0.126, s77 0.139, s54 0.190 | 100% / 96% / 100% |
| alpine | 0.002–0.18 / 0.007–0.18 / ≤ 0.12 | 512 s28 0.0014, s40 0.1804; 1024 s40 0.1904 | 96% / 100% / 100% |
| `land_below_0c` | 0.0007–0.16 | 512 s28 0.0000; 1024 s40 0.1636 | 98% / 100% / 100% |
| `temperature_c` p50, `rainfall_mm` p50, `rain_contrast` | as P1-00 13.1 | 1024 s103 rainfall p50 634 (range from 635) | 100% each (rainfall at 1024: 99.5%) |

Only steppe at 1024 falls below 90%, because the 200-seed sample spans more shadow strengths than the 30 calibration worlds. I have tuned nothing. Proposed final ranges follow, by the rule of 13.1: measured min–max, low end × 0.9, high end × 1.1, rounded outward. Hard bounds are unchanged: no world comes near them (largest wet share: fen 0.094; largest share overall: 0.612).

| Statistic | Proposed typical, 512 / 1024 / 2048 |
| --- | --- |
| temperate_forest | 0.21–0.64 / 0.18–0.68 / 0.35–0.64 |
| grassland | 0.12–0.54 / 0.10–0.49 / 0.11–0.31 |
| steppe | 0.0008–0.25 / 0.005–0.34 / 0.038–0.23 |
| wet_woodland | 0.0009–0.025 / 0.0037–0.031 / 0.012–0.034 |
| marsh | 0.001–0.016 / 0.0037–0.019 / 0.020–0.037 |
| fen | 0–0.040 / 0.0008–0.11 / 0.020–0.084 |
| highland_moor | 0.016–0.18 / 0.014–0.18 / (descriptive)–0.088 |
| heath | 0.013–0.15 / 0.0086–0.21 / (descriptive)–0.045 |
| alpine | 0.0012–0.20 / 0.0076–0.21 / (descriptive)–0.12 |
| `land_below_0c` | 0–0.18 at every size (512 s28 has none) |
| `temperature_c` p50, `rain_contrast` | unchanged (hold for every seed) |
| `rainfall_mm` p50 | unchanged (1024 s103, 634 mm, is the only miss, 1 mm under 635; the rule would give 570–900 at 1024) |

Whether 1024 should take the wide 200-seed ranges or keep tighter ones that leave the extreme shadow worlds as "worth a look" is the lead's call. The steppe extremes are 1024 s103 (0.304, a warm +1.9 °C strong-shadow world whose largest basin is 0.52) and s184 (0.006).

## 4. Temperature offset

- **Distribution.** Over the 270 numbered worlds the offset spans −2.0 to +2.0 °C, the full range. At 1024 (200 seeds) the mean is +0.07 and the standard deviation 1.21 °C; a uniform distribution over the 41 tenths has a standard deviation of 1.18. At 512 the mean is +0.17 (sd 1.13); at 2048, +0.14 (sd 1.18). The extremes −2.0 and +2.0 occur in 4 and 2 of the 200 worlds at 1024 (about 5 each are expected).
- **Values.** Seeds 0 to 11, 42 and the phrase give the values of P1-03 5.1 (20, 10, −11, 14, 2, 8, 6, −2, −19, −13, 11, 10; 9; −10).
- **Effect.** Cold worlds have the most alpine and frozen land: 512 and 1024 s40 (−1.5 °C) have alpine 0.180 and 0.190 and `land_below_0c` 0.148 and 0.164, the maxima at those sizes. The warmest keep a summit cap: 512 s0 (+2.0) has alpine 0.003, and s28 (+1.7) 0.0014 with no land below 0 °C.

## 5. Images

Every sweep world of 512 and 2048 and 1024 seeds 0–49 plus the phrase was rendered with `worldgen-cli render --composite biomes` into `~/.cache/the-unmapped/out/p1-04b-validation/renders/` (121 PNGs, no render error). Contact sheets in `…/sheets/` label each tile with the size, seed, wind (name, plus an arrow whose red head points where the wind blows to), offset and the nine shares (F forest, G grassland, S steppe, W wet woodland, M marsh, N fen, H moor, E heath, A alpine, in %):

- `512-biomes-seeds-0-49.png`, 128 px tiles;
- `1024-biomes-seeds-0-24.png` and `1024-biomes-seeds-25-49.png`, 200 px tiles;
- `2048-biomes-seeds-0-19.png`, 256 px tiles.

For the README, the phrase world "The Unmapped" at 1024 (NW wind, −1.0 °C):

- `readme-phrase-1024-biomes.png`, full size, 1024 × 1024;
- `readme-phrase-1024-biomes-768.png` and `readme-phrase-1024-biomes-768.jpg` (107 KB), 768 × 768.

Its shares are forest 29%, grassland 14%, steppe 16%, wet woodland 1.8%, marsh 0.5%, fen 3.7%, moor 10%, heath 8% and alpine 15%. The image shows a heath lee and a moor windward face on the central massif, and steppe across the north-west interior.

On the sheets, the lee sides of ranges carry the rust heath band and the steppe heart that the critic describes. The windward coasts and faces are forest and moor, and the 2048 interiors carry fen basins and dark wet-woodland river corridors (P1-04 limitations 1, 4, 5 and 6). I found nothing the critic's review does not already cover. No straight biome edge along a row or column appears that the terrain does not explain.

## 6. Thread independence

Worlds: 512 seeds 0, 6, 8 and 14; 1024 seeds 1 and 5; 2048 seeds 1 and 13. These cover four orthogonal and four diagonal winds and all three sizes. Each was generated with `--threads` 1, 2, 5 and 8 by both `linux-gcc-release` and `linux-clang-release`. All 64 files have the SHA-256 of the sweep's world (gcc-release, default threads), so B2c's row blocks and the new steps depend on neither the thread count nor the compiler.

## 7. Performance

gcc-release, 6 threads, one job at a time, other applications at load 2.3–4.0.

**The stage alone.** `biome_layer` (B2 to B5 with B2b and B2c) was run five times on each world's stored inputs by the scratch driver `biotime` (section 10). Every run's output equals the stored layer.

| Size, seeds | Best of 5 | All runs | Budget (P1-00 11) | Spec's implementation figure (P1-04 8) |
| --- | --- | --- | --- | --- |
| 512: 0, 5, 13, 41 | 71–73 ms | 70–84 ms | 0.15 s | 71 ms |
| 1024: 0, 3, 15, 41 | 220–239 ms | 220–265 ms | 0.5 s | 237 ms |
| 2048: 0, 7 | 908–920 ms | 908–944 ms | 2 s | 883 ms |

**End to end.** Full `generate` against `--until climate`, wall time and peak RSS by `/usr/bin/time`. 512 and 1024 have three repeats per world, 2048 two.

| Size | Full `generate` | `--until climate` | Difference (min of each) | Peak RSS, full / until climate |
| --- | --- | --- | --- | --- |
| 512 | 2.15–2.31 s | 2.08–2.21 s | 0.06–0.08 s | 28.3–30.1 / 28.4–30.2 MB |
| 1024 | 7.61–8.00 s | 7.38–7.81 s | 0.20–0.24 s | 92.6–94.7 / 92.7–94.7 MB |
| 2048 | 30.13–31.17 s | 29.31–30.26 s | 0.80–0.82 s | 350.1 / 349.9 MB (s0); 376.6 / 376.6 MB (s7) |

- **The stage is within budget at every size**: about 50% at 512, 45% at 1024 and 41% at 2048. The figures agree with the spec's (0.10 / 0.24 / 0.76 s as run differences).
- **Memory is unchanged** against 0.8.0 (28.0–30.1 / 92.3–94.7 / 349.8–376.7 MB).
- **The climate stage's offset costs nothing measurable.** `--until climate` takes 2.1 / 7.4–7.8 / 29.3–30.3 s against 0.8.0's 2.1–2.6 / 7.4–9.4 / 29.7–40.2 s, which were taken under load.
- **Geography total**: 2.2–2.3 / 7.6–8.0 / 30.1–31.2 s against 4 / 15 / 65 s.
- **The sweep's own times** (two worlds at a time, load 9–18): 512 3.1–7.2 s, 1024 9.0–23.7 s, 2048 44.4–65.9 s; peak RSS 27.9–30.1 / 91.4–95.3 / 346.9–376.6 MB.

## 8. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` was re-recorded for 0.9.0 after the five-preset comparison passed. The output legitimately changed:

- `generator_version` 0.9.0 and schema 4: `user_version`, `meta.schema_version`, and the `climate` DDL gains `temp_offset_dc`.
- The shipped data changed `content_hash`: four new biome definitions, two new rule fields, six new `worldgen.biomes` fields, `temp_offset_max_c`, and `coast_rain_mm` 40.
- `temperature` changes through the offset, and `rainfall` through the coast bonus.
- The `biome` layer and the twelve-row `biome` table change.
- The `climate` row gains the offset.

Every elevation and hydrology layer and table `pass` is byte-identical to 0.8.0 (section 1). Each case's `temperature`, `rainfall`, `biome`, `biome`-table and `climate`-table hashes equal the spec's reference values (P1-04 10.4) and the test writer's references.

| Case | File hash (0.9.0) |
| --- | --- |
| `geo-phrase-512` | `0a7f58858610d60eb3fc248ef34bed11b5123ec3df380a1baf293bf481338df7` |
| `geo-seed-0-512` | `3bf22b5828cdd220db40a09caacd4765b5b14cc402063915d5c940fd587900fd` |
| `geo-seed-1-1024` | `7f35198ac92d36f8b3bad7d76695d6714a6e3eacff6175095537f8ef5e100875` |
| `geo-seed-42-512` | `433383b11adc57b6c316bf3a9f9a7a4ade15741d8892d698c277d06b39b02cee` |

`geo-seed-0-512` equals the sweep's 512 seed 0 file. `p0-worlds.txt` is unchanged by this branch: the test writer's 0.9.0 values (`b86fed6`) equal all five presets' worlds. No stage golden vector file (`p1-0x-*.txt`) was touched. The MSVC presets are compared by CI's `compare-hashes`; on the pushed branch `p1/04b-variety-final` the "Check the golden world hashes" step is expected to fail until this commit's `p1-worlds.txt` is merged.

## 9. Defects and findings

**No defect.** There is no cross-preset or thread-count difference and no test failure. No table or layer rule is broken. No upstream layer changes. No hard bound is violated. The climate and biome layers equal the independent references on every cell of the 121 checked worlds.

Findings (not defects):

1. **Typical ranges with 200 seeds at 1024** (3.4): steppe holds for 88%, and fen, forest, marsh and heath for 90–96%. Proposed final ranges are in 3.4.
2. **512 s28: no land below 0 °C** (+1.7 °C, peak 1399 m; alpine 0.14%). It is the only such world in 270. It is intended by the offset, but it is the first world where a "cold" homeland would have no frozen ground, which matters for history placement.
3. **Fen is absent from 4 of 50 worlds at 512** (s0, s2, s10, s13) and under 0.1% in 10. This is the spec's accepted limitation 2, as calibrated. Appendix A's fallback is needed.
4. **Steppe is nearly absent in weak-shadow worlds** (512 s30 0.09%; 1024 s184 0.64%) and reaches 30% in 1024 s103. The share follows shadow strength.
5. **2048 s13 and s15** miss the three descriptive 2048 elevation sides as before (3.3). Notes only.
6. **`CLAUDE.md` `ctest` times** are still the 0.4.0 figures. Every run here was under load (293 / 276 / 964 / 818 s), so I have not changed them.

## 10. Where things are

- Sweep worlds, `report`, `hash --detail`, stderr and `/usr/bin/time` per world: `~/.cache/the-unmapped/out/p1-04b-validation/{512,1024,2048}/seed-<s>.*`, and the phrase world at `1024/phrase.*`. The binary used is `…/worldgen-cli-0.9.0-gcc-release`.
- Analysis in `…/analysis/`:
  - `a512.jsonl`, `a1024.jsonl`, `a2048.jsonl` and `aphrase.jsonl` (per-world checks and statistics);
  - `summary.txt` (bounds tables);
  - `ref/<N>-<seed>.txt` (climate and biome reference comparisons);
  - `threads.txt`.
- Renders and sheets: `…/renders/` and `…/sheets/`. Timing runs: `…/timing/` (`biotime.txt`, `e2e.txt`, `e2e-2048.txt`).
- World-hash case files: `~/.cache/the-unmapped/out/p1-04b-val/world-hashes/`.
- Scripts (scratch, not in the repository): `/tmp/claude-1000/-home-pgratz-Dropbox-Documents-MyCode-The-UnMapped/ce48c02e-8da2-4932-954b-fdafdbb43139/scratchpad/p104bval/mytools/`. They are `sweep.sh`, `sweepall.sh`, `analyze.py`, `summary.py`, `biosum.py`, `refcmp.py`, `climcmp.py`, `refone.sh`, `refall.sh`, `patches.c`, `threads.sh`, `timing.sh`, `timeall.sh`, `biotime.cpp`, `p0check.sh`, `renders.sh` and `sheet.py`.
