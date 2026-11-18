# Validation: P1-05 resources and habitability (generator 0.10.0, schema 4)

Validation specialist, 2026-11-18. Commit validated: `38497cb` (local `main`, "Reviews and proposals: P1-05 critic review"; also on origin as `p1/05-resources-final`). `GENERATOR_VERSION` 0.10.0, schema version 4, shipped `game/data` with content hash `906ab678…a1a96f40`. Spec: `docs/specs/p1-05-resources-habitability.md`, revision r2. Determinism audit: `docs/reviews/p1-05-resources-determinism-audit.md` (pass). World critic: `docs/reviews/p1-05-resources-critic.md`. Machine: 6-core Linux desktop. Builds, `ctest`, the world-hash run and the sweep ran together, and another agent's work shared the machine (load 12 to 17). The stage timings of section 6 were taken afterwards at load 3.2 to 4.2, with one core busy with my reference check. Windows MSVC is checked by CI.

**Verdict: pass.**

- **Builds and tests.** All five Linux presets build with warnings as errors.
  - `ctest` passes on `linux-clang-release` and `linux-gcc-debug`, the two the test writer had not run.
  - The test writer's runs on `linux-gcc-release`, `linux-clang-debug` and `linux-clang-asan` were on `763e0d1`. `git diff 763e0d1 38497cb` touches only `docs/` (two reviews and `proposals.md`), so I did not repeat them. The asan build was rebuilt and generated all twelve world-hash cases.
  - The determinism lint passes.
- **Hashes.** The twelve CI cases are identical on the five Linux presets. `p1-worlds.txt` is re-recorded, and `expect` gives 12 of 12 on every preset. The committed `p0-worlds.txt` (re-recorded by the engineer) equals all five presets' worlds.
- **Determinism.** Output does not depend on the thread count or the compiler: 40 of 40 files are equal.
- **Reference.** The seven layers and the eight report keys equal the test writer's independent reference (`p1_resources_reference.py`), run on each file's own input layers. All of its section 6 invariant checks pass. This was run on 75 worlds (section 1).
- **Upstream identity.** In all 271 sweep worlds, every elevation, hydrology, climate and biome layer, tables `pass`, `biome` and `climate`, the schema hash and every 0.9.0 `[geography]` line are equal to 0.9.0's. So every earlier hard bound still holds, as checked in `p1-04b-biome-variety-validation.md`.
- **Bounds.** No world violates a hard bound of section 9. The closest is 1024 s133, with `walk_main_share` 0.3062 against the hard minimum of 0.30 (section 3.3).
- **Cost.** The stage computes in 95 ms / 0.34 s / 1.26 s at 512 / 1024 / 2048, against budgets of 0.2 / 1 / 4 s. Its run difference is 0.08–0.10 / 0.26–0.35 / 1.28 s.

No defect. The spec's typical ranges hold for at least 95% of seeds at every size. Six single-world misses are listed in 3.2.

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug`, `linux-clang-debug`, `linux-clang-asan` | pass, all five |
| `ctest` (4 tests each), under load 12–17 | pass: `linux-clang-release` 432 s (`worldgen-tests` 304 s, `worldgen-cli-tests` 128 s); `linux-gcc-debug` 1571 s (1052 s, 520 s). The other three presets: the test writer's runs on `763e0d1` (only `docs/` has changed since) |
| `determinism_lint.py --self-test` and the scan | pass: 13 snippets; 122 C++ and 11 CMake files, 0 findings |
| `tools/ci/world-hashes.sh local` over the five presets | pass: all 12 cases identical (`hash --detail` and `report` text), 1204 s under load |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on all five presets |
| Committed `p0-worlds.txt` | confirmed: equals the eight P0 cases on all five presets |
| Stage golden vectors `p1-01-elevation.txt` … `p1-04-biomes.txt` | unchanged since 0.9.0 (`git diff 77b7a5d 38497cb`), as P1-05 10.4 requires |
| Seed sweep (gcc-release binary copied to the output folder, default threads, two worlds at a time): 512 seeds 0–49, 1024 seeds 0–199, 2048 seeds 0–19, phrase "The Unmapped" at 1024 | 271 of 271 `generate`, `report` and `hash --detail` succeed; stderr has only the three `--verbose` lines |
| Per-world file checks (`analyze.py`, section 2) | pass in all 271 worlds |
| Reference comparison (`p1_resources_reference.resources` and `report_items` on each file's decoded input layers, then `check_invariants`) | pass: all seven layers equal on every cell, all eight report lines equal, 9 of 9 invariant checks pass. Run on 512 seeds 0–49, 1024 seeds 0–19 and the phrase, and 2048 seeds 0–3 (75 worlds) |
| Upstream identity against the 0.9.0 sweep (`~/.cache/the-unmapped/out/p1-04b-validation/`, the same 271 worlds) | pass in all 271. The only hash lines that change are `file`, `table meta`, `table layer` and the seven new layers. The only new `report` keys are the eight of P1-05 9 |
| Thread independence (section 5) | pass: 40 of 40 files equal the sweep's |
| Timing (section 6) | within budget at every size |

## 2. Tables and layer rules

Checked from the files of all 271 sweep worlds, with a numpy decoder of the P0-02 codec:

- **meta**: `schema_version` 4, `generator_version` 0.10.0, `stages` `elevation,hydrology,climate,biomes,resources`.
- **Layer ids**: 1 to 18, in the catalogue order (`fertility` 12 … `walk_area` 18). `[tables] layer = 18`.
- **Types**: the six scores are `u8` and `walk_area` is `u16`, all N × N.
- **Invariant 1**: all seven layers are 0 on ocean. On lakes they are 0 except `fresh_water`, which is 255 there. `fresh_water` is 255 on every river cell.
- **Biome values**: `fertility` and `timber` are 0 on alpine and blighted cells.
- **Walk areas** (invariant 3 without the slope test, which the reference covers):
  - no area on water;
  - ids are dense, 1 to K;
  - ids are ordered by each area's smallest index;
  - every area has at least 977 cells (`walk_min_area_m2` 250,000 m² over 256 m² per cell).
- **Report**: all eight keys, recomputed from the layers with P1-00 10.3's percentile rule and `ratio4`, equal the printed text in every world. 512 s0 prints the spec's section 9 example exactly.

## 3. Statistics against P1-05 section 9

Values are exact: ratios come from their integer counts. 1024 has 200 seeds, 512 has 50 and 2048 has 20.

### 3.1 Every report key, min / median / max

| Key | Hard bound | 512 (50) | 1024 (200) | 2048 (20) | Phrase 1024 | Spec, measured by prototype (512 ; 1024 ; 2048) |
| --- | --- | --- | --- | --- | --- | --- |
| `habitable_share` | ≥ 0.05 | 0.1741 / 0.4116 / 0.6327 | 0.3437 / 0.5486 / 0.7528 | 0.5973 / 0.7123 / 0.8029 | 0.4303 | 0.174/0.412/0.633 ; 0.356/0.539/0.712 ; 0.597/0.715/0.780 |
| `fertile_share` | descriptive | 0.1598 / 0.3694 / 0.5897 | 0.2885 / 0.5112 / 0.7198 | 0.5758 / 0.6750 / 0.7926 | 0.3921 | 0.160/0.369/0.590 ; 0.320/0.504/0.680 ; 0.576/0.675/0.745 |
| `ore_rich_share` | > 0 | 0.0135 / 0.0657 / 0.1443 | 0.0202 / 0.0524 / 0.1137 | 0.0062 / 0.0314 / 0.0582 | 0.0668 | 0.0135/0.066/0.144 ; 0.0215/0.058/0.097 ; 0.0203/0.029/0.058 |
| `walk_areas` | ≥ 1 | 1 / 2 / 6 | 1 / 3 / 9 | 1 / 5 / 10 | 2 | 1/2/6 ; 1/3/7 ; 3/5.5/10 |
| `walk_main_share` | ≥ 0.30 at 1024, 2048 | 0.1826 / 0.5602 / 0.7674 | 0.3062 / 0.7022 / 0.8466 | 0.7196 / 0.8237 / 0.9084 | 0.6155 | 0.183/0.560/0.767 ; 0.487/0.679/0.829 ; 0.720/0.825/0.889 |
| `walk_share` | descriptive | 0.3893 / 0.5801 / 0.7674 | 0.5545 / 0.7134 / 0.8466 | 0.7359 / 0.8314 / 0.9103 | 0.6259 | 0.389/0.580/0.767 ; 0.586/0.703/0.833 ; 0.736/0.836/0.891 |
| `scores_p50` fertility | descriptive | 0 / 69.5 / 178 | 2 / 175.5 / 178 | 178 / 178 / 178 | 72 | 0–178 ; 35–178 ; 178 |
| `scores_p50` ore | descriptive | 0 / 0 / 11 | 0 / 0 / 3 | 0 / 0 / 1 | 0 | — |
| `scores_p50` timber | descriptive | 13 / 38 / 70 | 13 / 38 / 255 | 38 / 111 / 255 | 26 | 13–70 ; 38–255 ; 38–255 |
| `scores_p50` fresh_water | descriptive | 102 / 119 / 143 | 112 / 135 / 153 | 151 / 155 / 163 | 139 | — |
| `scores_p50` forage | descriptive | 88 / 119 / 128 | 89 / 128 / 128 | 128 / 128 / 128 | 89 | 88–128 ; 90–128 ; 128 |
| `scores_p50` habitability | descriptive | 0 / 17.5 / 81 | 0 / 68 / 95 | 77 / 95 / 109 | 40 | 0/18/81 ; 7/68/95 ; 77/98/105 |
| `scores_p90` fertility | descriptive | 178 / 255 / 255 | 189 / 255 / 255 | 240 / 255 / 255 | 255 | — |
| `scores_p90` ore | descriptive | 25 / 77 / 185 | 25 / 50 / 139 | 25 / 25 / 46 | 81 | — |
| `scores_p90` timber | descriptive | 255 / 255 / 255 | 255 / 255 / 255 | 255 / 255 / 255 | 255 | — |
| `scores_p90` fresh_water | descriptive | 224 / 231 / 235 | 231 / 235 / 235 | 235 / 235 / 241 | 235 | 224–235 ; 231–235 ; 235–241 |
| `scores_p90` forage | descriptive | 171 / 180 / 188 | 164 / 184 / 196 | 188 / 194 / 198 | 180 | — |
| `scores_p90` habitability | ≥ 1 | 95 / 147.5 / 163 | 136 / 153 / 165 | 153 / 159.5 / 165 | 142 | 95/148/163 ; 141/152/165 ; 153/158/163 |

**Agreement with the spec's prototype.** On the prototype's own seeds (512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–9), every min, median and max in this table equals the spec's figures to the precision the spec prints. The one exception is the 2048 median of `scores_p50` habitability: 97.5 here against 98 in the spec. That is the median of ten values, two ranks apart, not a difference in any world. The implementation and the prototype therefore agree, as section 9 expects.

### 3.2 Typical ranges: holds-for rates and misses

| Statistic | Typical, 512 / 1024 / 2048 | Holds for, 512 / 1024 / 2048 | Misses |
| --- | --- | --- | --- |
| `habitable_share` | 0.15–0.70 / 0.32–0.79 / 0.53–0.86 | 100 / 100 / 100% | — |
| `fertile_share` | 0.14–0.65 / 0.28–0.75 / 0.51–0.82 | 100 / 100 / 100% | — |
| `ore_rich_share` | 0.012–0.16 / 0.019–0.11 / 0.018–0.064 | 100 / 99.5 / 95% | 1024 s54 0.1137; 2048 s13 0.0062 |
| `walk_areas` | 1–7 / 1–8 / 2–12 | 100 / 99.5 / 95% | 1024 s185 9; 2048 s10 1 |
| `walk_main_share` | ≥ 0.16 / ≥ 0.43 / ≥ 0.64 | 100 / 99.5 / 100% | 1024 s133 0.3062 |
| `walk_share` | ≥ 0.35 / ≥ 0.52 / ≥ 0.66 | 100 / 100 / 100% | — |
| `scores_p90` habitability | 85–180 | 100 / 100 / 100% | — |

Every rate is at least 95%, so the ranges stand. I have tuned nothing. If the lead wants the ranges to cover the 200-seed sample (measured min–max, widened by about 10%), the changes would be:

- 1024 `ore_rich_share` up to 0.125;
- 1024 `walk_areas` 1–10;
- 2048 `walk_areas` 1–12;
- 2048 `ore_rich_share` from 0.005 (or descriptive at the low end, as for the other 2048 minimums).

I would keep `walk_main_share` ≥ 0.43 at 1024 typical. Widening it would put it below the hard bound.

### 3.3 Worlds worth a look (notes, not defects)

- **1024 s133 is 0.0062 from the hard `walk_main_share` bound** (0.3062 ≥ 0.30). It has two lowlands of almost equal size, 129,699 and 103,382 cells, on either side of a range, plus five small pockets. `walk_share` is 0.58, so the land is not unwalkable; it is split in two. The next lowest at 1024 is s45 at 0.487. The hard bound measures "one main lowland", and a world with two equal lowlands can come near it without being broken. The lead may want to consider whether 0.30 should stay hard, or whether `walk_share` (0.5545 minimum here) is the better hard check.
- **1024 s75 has `scores_p50` habitability 0.** This is below the prototype's 1024 minimum of 7 (seeds 0–49). It has the lowest `habitable_share` at 1024 (0.3437) and `fertile_share` (0.2885), and 38% of its land is steeper than 35°. It is a steep world, the 1024 case of limitation 1.
- **2048 s13** (the 0.9.0 "one short coastal range on a broad lowland" world) has `ore_rich_share` 0.0062, a third of the next lowest (s5 at 0.0203). It has little upland, so it has little ore above the ore height ramp. Its rich deposits exist (`ore` 255 is reached), so the hard bound holds.
- **2048 s10 has a single walk area** (typical 2–12). It is one connected lowland with no pocket of 977 cells or more.
- **1024 s185 has 9 walk areas.** It has a central plateau basin ringed by steep ground, which gives a second large area (34,308 cells) and seven pockets.
- **Habitability never reaches 255.** Each world's largest value is between 198 and 206, against the function's maximum of 255. Every other score reaches 255 in every world. This follows from the weights: no cell has fertility, forage, timber and ore all near 255. It is consistent with "scores are absolute", but history should not assume that 255 occurs.
- **Ore is concentrated.** The `scores_p50` ore median is 0 at every size, and `scores_p90` ore is 25 in many 1024 and 2048 worlds, while rich cells (≥ 128) are 0.6–14% of dry land. This is as designed: veins above the noise threshold only.
- **Forest against grassland** (critic F1): forest's 95th percentile of habitability exceeds grassland's in 2 of 50 worlds at 512 (s2, s24) and in none at 1024 or 2048. My measure is over all dry land of each biome, not only walkable land as in spec 11.3, which reports 1 of 50.
- **Per-biome means over dry land** (median over worlds; fertility, forage, timber, habitability, at 512 / 1024 / 2048) match spec section 9:
  - forest 97–162, 143–151, 213–249, 61–103;
  - grassland 160–232, 88–96, 33–37, 79–117;
  - steppe 88–158, 101–107, 10–13, 44–84;
  - wet woodland 135–137, 241–244, 178, 131–132;
  - marsh 131, 239–244, 77, 91–101;
  - fen 71, 195–200, 51, 68–70;
  - moor and heath 1–4, 53–69, 2–10, 0–1;
  - alpine 0, 13–16, 0, 0.

  Fen is absent from four 512 worlds (s0, s2, s10, s13), as at 0.9.0.
- **Walk areas = 1** in 21 of 50 worlds at 512, 43 of 200 at 1024 and 1 of 20 at 2048.

No world without rivers, ore, habitable land or a walk area occurred.

## 4. Images

Rendered with `worldgen-cli render` (palette `score` for the scores, `ids` for `walk_area`, and `--composite biomes`) into `~/.cache/the-unmapped/out/p1-05-validation/renders/`. The contact sheets are in `…/sheets/`. Each has one row per world, with columns biomes, habitability, fertility, ore, forage and walk_area, and a label giving the size, seed, `habitable_share`, `fertile_share`, `ore_rich_share`, `walk_areas` and `walk_main_share`:

- `512-resources.png`: seeds 0, 1, 2, 13, 28 and 41 (steep s0 and s2, open s1, ringed s13, the scarp world s28);
- `1024-resources.png`: the phrase, seeds 0, 1 and 3, and the three extremes of 3.3 (s75, s133, s185);
- `2048-resources.png`: seeds 0, 5, 13 and 15; `2048-resources-s10.png`: seed 10.

What the sheets show:

- Habitability is high on the lowlands along the streams and zero on the ranges.
- Fertility is brightest on grassland and steppe.
- Ore lies in compact fields on the flanks of the ranges.
- Forage has the fish halo along coasts, lakes and big rivers.
- Walk areas show one main lowland and small pockets.

I found nothing the critic's review does not already cover. No straight edge along a row or column appears in any layer that the terrain does not explain.

**README image** `docs/images/p1-habitability.jpg` (75 KB, 768 × 768) shows the phrase world "The Unmapped" at 1024. It is in the style of `p1-biomes.jpg`:

- land cells take the `score` colour of `habitability`, darkened or lightened by a north-west hillshade of `elevation`;
- water and river cells are copied from `render --composite biomes`;
- the result is box-reduced from 1024 and saved at JPEG quality 88.

The full-size version and the composition script are in `…/readme/`. The habitable west and north-east lowlands stand against the uninhabitable central massif and its lakes.

## 5. Thread independence

Worlds: 512 seeds 0 and 6, 1024 seeds 1 and 5, and 2048 seed 1. Each was generated with `--threads` 1, 2, 5 and 8 by both `linux-gcc-release` and `linux-clang-release`. All 40 files have the SHA-256 of the sweep's world (gcc-release, default threads, 6 on this machine). This includes 1024 seed 1, the CI case `geo-seed-1-1024`, at `--threads 1`.

## 6. Performance

gcc-release, 6 threads, one job at a time, load 3.2–4.2. One core was busy with the reference check.

**The stage alone.** `resources_layers` (R1 to R6) was run five times on each world's stored input layers by the scratch driver `restime` (section 9). Every run's output equals the stored seven layers.

| Size, seeds | Best of 5 | All runs | Budget (P1-00 11) |
| --- | --- | --- | --- |
| 512: 0, 5, 13, 41 | 95–98 ms | 94–118 ms | 0.2 s |
| 1024: 0, 3, 15, 41 | 336–347 ms | 335–368 ms | 1 s |
| 2048: 0, 7 | 1257–1286 ms | 1256–1324 ms | 4 s |

**End to end.** Full `generate` against `--until biomes`, two repeats each, wall time and peak RSS by `/usr/bin/time`:

| Size, seeds | Full `generate` | `--until biomes` | Difference (min of each) | Peak RSS, full / until biomes |
| --- | --- | --- | --- | --- |
| 512: 0, 13 | 2.33–2.46 s | 2.23–2.35 s | 0.08–0.10 s | 30.4–30.8 / 29.6–30.9 MB |
| 1024: 0, 3 | 8.42–8.94 s | 8.09–8.33 s | 0.26–0.33 s | 99.4–100.0 / 94.9–97.7 MB |
| 2048: 0 | 34.60–36.84 s | 33.32–33.44 s | 1.28 s | 390 / 357 MB |

- The stage uses about 50% of its budget at 512, 35% at 1024 and 32% at 2048, by either measure.
- **Geography totals** are 2.3–2.5 / 8.4–8.9 / 34.6–36.8 s, against 4 / 15 / 65 s. Peak memory grows by 1 / 2–5 / 33 MB, against budgets of 200 MB / 600 MB / 2 GB.
- **The sweep's own times** (two worlds at a time, load 12–17): 512 4.3–7.4 s, 1024 8.9–29.4 s, 2048 53.5–97.4 s; peak RSS 29.6–30.6 / 96.6–101.8 / 374.5–381.9 MB.
- The README's "a 1024 world takes about 8 seconds" is now about 8.5 seconds.

## 7. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` was re-recorded for 0.10.0 after the five-preset comparison passed. The output legitimately changed:

- `generator_version` is 0.10.0, which changes `meta`.
- The shipped data changed `content_hash`:
  - new `worldgen.resources`;
  - the new `forage` field of every biome;
  - steppe `fertility` 0.7.
- The seven new layers and their rows in table `layer`.

Every earlier layer, the schema and tables `pass`, `biome` and `climate` are byte-identical to 0.9.0 (section 1).

| Case | File hash (0.10.0) |
| --- | --- |
| `geo-phrase-512` | `5ef6c66858ad69b3243a4974b32d8d0246da3af424a05f70bb2c97a8d7b25445` |
| `geo-seed-0-512` | `30efddafc39ebc45c27b2628d11ad0a67662f6269bc95d0721c837d34c3ca05b` |
| `geo-seed-1-1024` | `da760fd4809816d8e72364b65fd3f0fd2dfcd20fbcd68dd69e33b9b0106c0c46` |
| `geo-seed-42-512` | `69598991fbc1fa856f247026b36c028dceab83f28fd7da79e82cd760ec47d92c` |

- `geo-seed-0-512` and `geo-seed-1-1024` equal the sweep's files for 512 s0 and 1024 s1.
- `p0-worlds.txt` is unchanged by this branch: the engineer's 0.10.0 values equal all five presets' worlds.
- No stage golden vector file was touched.
- The MSVC presets are compared by CI's `compare-hashes`. On the pushed branch `p1/05-resources-final`, the "Check the golden world hashes" step is expected to fail until this commit's `p1-worlds.txt` is merged.

**README example world** (`worldgen-cli generate --seed-phrase "The Unmapped"`, default data root, map size 1024): file hash `ef7dc97caa5ebe1f0886c9a78ec0813f65e5916b7977a8c8c87054f14bb70a06`, content hash `906ab678185b2a157a15b1dc26402772476bd7965e3fdddc92ab6f70a1a96f40`. It equals the sweep's phrase world with `--data`. The full `report` text is in `…/readme/phrase-report.txt`.

## 8. Defects and findings

**No defect.** There is no cross-preset or thread-count difference and no test failure. No layer or table rule is broken. No upstream layer changes. No hard bound is violated. The seven layers equal the independent reference on every cell of the 75 checked worlds.

Findings (not defects):

1. **1024 s133 is 0.006 above the hard `walk_main_share` minimum** (two equal lowlands; 3.3). Whether the bound should stay hard is the lead's call.
2. **Single typical misses** at 1024 (s54 ore 0.1137, s185 nine walk areas, s133 walk main 0.306) and at 2048 (s13 ore 0.0062, s10 one walk area). Holds-for is at least 95% everywhere. Optional widenings are in 3.2.
3. **Habitability tops out at 198–206** in every world, not 255 (3.3).
4. **1024 s75**: median habitability 0 (steep world; limitation 1 at 1024).
5. **`CLAUDE.md` times**: `world-hashes.sh` and `ctest` were run here only under heavy load (`ctest` clang-release 432 s, gcc-debug 1571 s, against the recorded 2.5 and 10 minutes), so I have not changed the figures. A quiet re-measurement would show the stage's share; the stage itself adds about 0.3 s per 1024 world.

## 9. Where things are

- **Sweep worlds**: `~/.cache/the-unmapped/out/p1-05-validation/{512,1024,2048}/seed-<s>.*`, with `report`, `hash --detail`, stderr and `/usr/bin/time` for each world. The phrase world is at `1024/phrase.*`. The binary used is `…/worldgen-cli-0.10.0-gcc-release`.
- **Analysis**, in `…/analysis/`:
  - `a512.jsonl`, `a1024.jsonl`, `a2048.jsonl` and `aphrase.jsonl` (per-world checks and statistics);
  - `summary.txt` (bounds tables);
  - `ref/ref.txt` (reference comparisons);
  - `threads.txt`.
- **Renders and sheets**: `…/renders/` and `…/sheets/`.
- **README image sources**: `…/readme/`, with the full-size `p1-habitability-1024.png`, the report and the default-data world.
- **Timing runs**: `…/timing/` (`restime.txt`, `e2e.txt`).
- **Logs**: `…/logs/` (builds, `ctest`, `world-hashes` and `expect`). The world-hash case files are in `~/.cache/the-unmapped/out/p1-05-val/world-hashes/`.
- **Scripts** (scratch, not in the repository): `/tmp/claude-1000/-home-pgratz-Dropbox-Documents-MyCode-The-UnMapped/ce48c02e-8da2-4932-954b-fdafdbb43139/scratchpad/p105val/`:
  - `sweep.sh`, `sweepall.sh`, `analyze.py`, `summary.py` and `wdecode.py`;
  - `refcmp.py` and `refall.sh`;
  - `threads.sh`;
  - `restime.cpp`;
  - `renders.sh`, `sheet.py` and `readme_img.py`.
