# Validation: P1-02 hydrology

Validation specialist, 2026-11-16. Commit validated: `3d96001` (local `main`, "Merge branch 'p1/02-hydrology-audit'"; on origin as `p1/02-hydrology-final`): the hydrology stage, its independent tests and the determinism audit (passed, `docs/reviews/p1-02-hydrology-determinism-audit.md`). `GENERATOR_VERSION` 0.4.0, shipped `game/data` with `river_min_area_m2` = 125000 (T = 489), content hash `3fa05f6f…2085aea`. Specs: `docs/specs/p1-02-hydrology.md`, `docs/specs/p1-00-geography-overview.md` (13.1 as revised 2026-11-16, 14 X5). Machine: 6-core Linux desktop, 31 GB. Windows MSVC is run separately in CI by the lead.

**Verdict: pass.** Builds, tests (including the slow suites), cross-preset identity, thread independence, performance and golden hashes pass. The seed sweep finds no hard-bound violation in any row of P1-00 13.1, elevation or hydrology. The elevation layer of every sweep world is byte-identical to the P1-01 validation sweep's. Every lake in the sweep drains through a visible river. Section 4 proposes final ranges for the provisional hydrology rows; the main change is that `major_rivers` does not follow the ×4 count rule and needs per-size bounds.

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug`, `linux-clang-release`, `linux-clang-asan` | pass (all five) |
| `ctest` per preset (4 tests each) | pass: gcc-release 64 s, clang-release 66 s, gcc-debug 249 s, clang-debug 262 s, clang-asan 1102 s (`worldgen-tests` 743 s, `worldgen-cli-tests` 358 s; partly run alongside the slow suites, so a little high) |
| `worldgen-tests -ts=elevation-slow --no-skip`, gcc-release and clang-release | pass: 7 test cases, 41,949,281 assertions each (186 s, 207 s) |
| `worldgen-tests -ts=hydrology-slow --no-skip`, gcc-release and clang-release | pass: 5 test cases, 2,189 assertions each (100 s, 110 s) |
| `worldgen-cli-tests -ts=hydrology-slow --no-skip`, gcc-release and clang-release | pass: 1 test case, 23 assertions each (19 s, 18 s) |
| `determinism_lint.py --self-test` and the scan | pass: 13 snippets; 112 C++ and 11 CMake files, 0 findings |
| `tools/ci/world-hashes.sh local` over the five presets | pass: all 12 cases identical (`hash --detail` and `report` text), 414 s for the five presets |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on every preset |
| Committed `p0-worlds.txt` | confirmed: equals `sha256sum` of each P0 case world on all five presets; `p0_world_check.py check … --detail --report --generator-version 0.4.0 --content-hash 3fa05f6f…2085aea` passes for all eight cases (gcc-release worlds) |
| Seed sweep (gcc-release, default threads): 512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–7 | 108 of 108 `generate`, `report` and `hash --detail` succeed; no hard-bound violation (section 3) |
| Elevation unchanged by hydrology | pass: the `elevation` layer hash of all 108 sweep worlds equals that of the same world in the P1-01 validation sweep (`~/.cache/the-unmapped/out/p1-01-validation/`, generator 0.3.0); the four geo golden cases keep the P1-01 elevation layer hashes |
| Thread independence: 512 seeds 0, 13, 42; 1024 seeds 3, 17; 2048 seed 5; `--threads` 1, 2, 5, 8; gcc-release and clang-release | pass: all 48 files byte-identical to the sweep's (default threads) |

## 2. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` (SHA-256 of the world file; lines `<case> <hex>` sorted by case name, LF; P0-04 9, P1-00 14 X5), re-recorded for generator 0.4.0 after the five-preset comparison passed. It replaces the 0.3.0 elevation-only values (audit S1). The legitimate reason for the change: the default pipeline now runs hydrology, so these worlds gain seven layers and `meta.stages` = `elevation,hydrology`; the elevation layer itself is unchanged.

| Case | File hash | `elevation` layer (unchanged) | `water` layer |
| --- | --- | --- | --- |
| `geo-phrase-512` | `e8b18fdac60d23baeb9e82f83a2fcdcacd937b7ce6d68f3fcf04ec24f807d1de` | `c3d1c354…b8aa03` | `3dab0a6a…429fd3e8` |
| `geo-seed-0-512` | `862e7bb778291ec7b444897628c0a4335830e409378a67887f6cb06c02287867` | `b1e02da1…88b64c` | `6db0bf30…8bb9504` |
| `geo-seed-1-1024` | `e8bc7a81daed9f9f793f412f58a738f8e4db6a76ccc74b59f13637360d647fc4` | `0cc4f2c1…6d31d2` | `1e2c003e…aef6602` |
| `geo-seed-42-512` | `dc64438c95b6d14d6f06013ccd7bfc07501c829e12ce2323e9ff4d0a9f0ee084` | `cff0cce9…dd33757` | `e94af391…49c5d25` |

`p0-worlds.txt` is unchanged (confirmed, section 1). The MSVC presets have not been compared here; the CI `compare-hashes` job does that, and its `expect` step should now pass on the four geo cases.

## 3. Statistics against P1-00 13.1

Comparison is exact as 13.1 requires: counts are exact in `report`; `land_fraction`, `lake_share_of_land` and `river_share_of_land` are computed as exact fractions of the printed integer counts; a `ratio4` or `to_decimal(4)` value is within 0.00005 of the exact value and no printed value equals a bound; `longest_river_m` is a rounded metre value, and no seed is within 0.5 m of a bound. Count bounds follow the 13.1 count rule (512 ÷ 4, 2048 × 4, rounded outward): `major_rivers` 512 hard 0–10, typical 0–5; 2048 hard 4–160, typical 12–80. `lakes` has its own per-size bounds. `longest_river_m` is given as a fraction of the map side (N × 16 m).

### 3.1 Elevation rows

All values are identical to the P1-01 validation (same elevation layers); against the bounds as revised after it (p90 at 2048 ≥ 200 m, `step_p999_m` ≤ 200 m at 512) there is **no hard violation**. The former D1 (512 seed 41, `step_p999_m` 183.8 m) and D2 (2048 seed 5, p90 224.2 m) are now inside. Min / median / max and typical misses are as in `docs/reviews/p1-01-elevation-validation.md` section 3; the typical misses are unchanged: at 2048 `land_above_m` 500 is below 0.08 in 2 of 8 seeds (s4, s5) and p90 below 330 m in 1 of 8 (s5); the new typical `step_p999_m` ≤ 150 m at 512 holds for 49 of 50 seeds (98%; s41 outside).

### 3.2 Hydrology rows, hard bounds

Min / median / max:

| Statistic | Hard bound | 512 (50 seeds) | 1024 (50 seeds) | 2048 (8 seeds) | Violations |
| --- | --- | --- | --- | --- | --- |
| `lakes` | ≤ 25 / 1–100 / 4–400 | 0 / 2 / 4 | 2 / 7 / 12 | 8 / 11.5 / 15 | none |
| `lake_share_of_land` | ≤ 0.031 | 0 / 0.0153 / 0.0300 | 0.0113 / 0.0295 / 0.0300 | 0.0298 / 0.0299 / 0.0300 | none |
| `river_share_of_land` | 0.005–0.10 (provisional) | 0.0125 / 0.0159 / 0.0203 | 0.0170 / 0.0197 / 0.0227 | 0.0231 / 0.0240 / 0.0254 | none |
| `major_rivers` | 0–10 / 1–40 / 4–160 (provisional) | 2 / 5 / 8 | 9 / 15 / 21 | 33 / 39.5 / 42 | none |
| `longest_river_m` ÷ side | ≥ 0.15 (provisional) | 0.329 / 0.524 / 0.976 | 0.359 / 0.587 / 1.558 | 0.571 / 0.824 / 0.982 | none |
| `largest_basin_share` | 0.03–0.6 | 0.0840 / 0.1968 / 0.3783 | 0.0896 / 0.1766 / 0.3762 | 0.1090 / 0.1802 / 0.3052 | none |

Descriptive (min / median / max), not in 13.1:

| Statistic | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| `river_sources` | 37 / 50 / 62 | 178 / 212 / 235 | 891 / 931 / 971 |
| `river_mouths` | 15 / 23 / 35 | 44 / 59.5 / 83 | 151 / 167.5 / 219 |
| `largest_lake_cells` | 0 / 871.5 / 2752 | 850 / 4078.5 / 11160 | 11495 / 17313.5 / 32145 |
| smallest lake (cells) | 391 | 392 | 398 |

The river source count at the shipped threshold, which P1-02 4 lists as not yet measured, is 178 to 235 at 1024 (was 45 to 52 at 0.5 km²).

### 3.3 Hydrology rows, typical bounds: seeds outside

| Statistic | Typical | 512 | 1024 | 2048 |
| --- | --- | --- | --- | --- |
| `lakes` | ≥ 1 / 3–25 / 6–60 | 3 (s25, s29, s36: no lake) | 1 (s35: 2) | 0 |
| `lake_share_of_land` | 0.003–0.03 | 3 (s25, s29, s36) | 0 | 0 |
| `river_share_of_land` | 0.01–0.06 (provisional) | 0 | 0 | 0 |
| `major_rivers` | 0–5 / 3–20 / 12–80 (provisional) | **17 of 50 (34%)**: s4, 5, 12, 18, 21, 23, 27, 28, 32, 34, 37, 40, 43, 44, 45, 46, 47 (6 to 8) | 1 (s34: 21) | 0 |
| `longest_river_m` | ≥ 0.25 × side (provisional) | 0 | 0 | 0 |
| `largest_basin_share` | 0.08–0.4 | 0 | 0 | 0 |

Every typical range holds for at least 90% of seeds except `major_rivers` at 512. The cause is the count rule, not the terrain: mouths of class ≥ 4 (upstream ≥ 8T, about 1 km²) grow by about 2.5 times per map-size doubling (medians 5, 15, 39.5), not 4 times (a count of features along the coast, which grows less than the land area). Dividing the 1024 typical range by 4 puts the 512 upper bound at 5, which is the 512 median.

## 4. Proposed final ranges for the hydrology rows

By the rule of P1-00 13.1 (second-pass paragraph): typical = the measured min–max widened by about 10% (about 5% of the width at each end, rounded outward), and must contain at least 90% of seeds; hard only where a value outside would mean a defect. For the lead's decision; the spec is not changed here.

| Statistic | Proposed hard | Proposed typical | Measured 512 / 1024 / 2048 | Change |
| --- | --- | --- | --- | --- |
| `lakes` | 512: ≤ 25; 1024: 1–100; 2048: 4–400 (unchanged) | 512: 0–5; 1024: 1–13; 2048: 7–16 | 0–4 / 2–12 / 8–15 | typical narrowed to the measured spread; 512 lower bound dropped (6% of 512 worlds have no lake, P1-01 E10 at small size) |
| `lake_share_of_land` | ≤ 0.031 (unchanged; the cap of P1-01 E10.3) | 0.003–0.03 (unchanged) | 0–0.0300 / 0.0113–0.0300 / 0.0298–0.0300 | none (94% / 100% / 100% inside) |
| `river_share_of_land` | 0.005–0.05 | 0.011–0.027 | 0.0125–0.0203 / 0.0170–0.0227 / 0.0231–0.0254 | provisional mark removed; hard upper 0.10 → 0.05 (more than 5% of land as river cells at T = 489 would mean the threshold or accumulation is wrong); typical tightened from 0.01–0.06. The share rises slowly with map size (longer trunks), so one range fits all sizes |
| `major_rivers` | per size, not by the count rule: 512: 1–20; 1024: 1–40; 2048: 4–160 | 512: 1–9; 1024: 8–22; 2048: 32–43 | 2–8 / 9–21 / 33–42 | per-size typical ranges (section 3.3); hard lower bound 1 at 512 |
| `longest_river_m` | ≥ 0.15 × side (unchanged) | ≥ 0.27 × side | 0.33–0.98 / 0.36–1.56 / 0.57–0.98 × side | provisional mark removed; typical 0.25 → 0.27 |
| `largest_basin_share` | 0.03–0.6 (unchanged) | 0.07–0.40 | 0.084–0.378 / 0.090–0.376 / 0.109–0.305 | typical lower 0.08 → 0.07 by the widening rule (no seed below 0.084) |
| `river_sources` (new, descriptive) | — | — | 37–62 / 178–235 / 891–971 | scales about ×4 per doubling, as the count rule assumes |

The shipped threshold (0.125 km²) is confirmed: river density is 1.3% to 2.5% of land on every world, with no world sparse or flooded with rivers, and every lake's outflow is a drawn river (section 6).

## 5. Performance (gcc-release, default threads = 6, `generate` wall time including load and write; peak RSS from `/usr/bin/time`)

| Size | Seeds | Time min / median / max | Budget, all geography | Peak RSS | Budget |
| --- | --- | --- | --- | --- | --- |
| 512 | 50 | 1.78 / 1.88 / 2.13 s | 4 s | 30.7 MB | 200 MB |
| 1024 | 50 | 6.52 / 6.76 / 8.22 s | 15 s | 101.8 MB | 600 MB |
| 2048 | 8 | 26.4 / 27.0 / 27.5 s | 65 s | 376 MB | 2 GB |

Hydrology's own cost, as the difference between the full `generate` and `--until elevation` on the same seed (3 repeats each, seeds 0–2 at 512 and 1024, 0–1 at 2048, quiet machine): about 0.02 s at 512, 0.10 s at 1024 and 0.45 s at 2048, including encoding and writing seven layers, against budgets of 0.3, 1.2 and 5 s. Peak RSS is unchanged from elevation alone. The 1024 maximum (8.22 s) and some 512 times were measured while other work was running on the machine at the start of the sweep. One thread at 2048: 78 s (gcc and clang); 8 threads on 6 cores is no slower than 5 (29 to 32 s against 34 to 35 s).

## 6. Rivers and lakes on the maps

Composite maps (`render --composite map`) of every sweep world are under `~/.cache/the-unmapped/out/p1-02-validation/maps/`; contact sheets under `~/.cache/the-unmapped/out/p1-02-validation/sheets/` (1024 seeds 0–24 and 25–49, 512 seeds 0–49, 2048 seeds 0–7; JPEG, 125 to 225 KB each). On the contact sheets rivers are one cell wide and so mostly invisible (a 1024 map is drawn at 200 px); lakes show. The river observations below come from full-resolution crops (`~/.cache/the-unmapped/out/p1-02-validation/crops/`: 1024 seeds 3, 13 and 14, 2048 seed 3) and from statistics over the decoded layers of all 108 worlds (a numpy decoder checked against `p0_world_check.py`'s `decode_blob` on every layer of 512 seed 0).

- **Density.** The network is dense and branching: about 210 sources and 60 mouths on a 1024 world, rivers on 1.7% to 2.3% of land. Rivers start on range flanks and on the lowland, join, grow in class and reach the sea, often in bays (1024 seed 14). No world is without rivers; none looks flooded with them.
- **Range flanks.** Many short first-order rivers run down range flanks as parallel straight channels, the same diagonal for many neighbours (1024 seed 14, east range; 1024 seed 3). These follow the gullies cut by elevation's erosion (P1-01 E8; P1-01 limitation "a few straight gullies"), not a hydrology defect.
- **Staircase runs on the lowland.** On low gentle ground, larger rivers wander in orthogonal steps with right-angle turns, a visible D8 "staircase" (1024 seed 13 basin interior, `images/p1-02/p1-02-1024-seed-13-crop.jpg`; 1024 seed 3 plain). The runs are short: the longest run of river cells with the same `flow_dir` is 13 to 33 cells at 512, 16 to 34 at 1024 and 24 to 46 at 2048 (at most 740 m); no world has a run of 64 cells; 52% of river steps are orthogonal. No long straight channel on flats. The runtime smooths river paths at sub-cell scale (P1-02 9.2), so this is expected at this stage, but the critic should judge it against checklist item 5.
- **Rivers along range feet.** Trunk rivers often follow the foot of a range for several kilometres before turning to the sea (2048 seed 3: about 6 km along the north foot of the central range; 1024 seed 13: along the inner foot of the eastern ring range). Plausible as strike valleys; noted for checklist item 6. No river was seen running along the coast for a long way.
- **Lakes.** 536 lakes in 108 worlds. Every one has its outlet draining into a river cell: no lake drains to dry land, another lake directly, or the sea directly, so P1-02 limitation 1 (a small lake with no visible outflow) does not occur at the shipped values; the smallest lake is 391 cells (0.1 km²), already near T. No rash of tiny lakes; lakes sit in mountain basins and lowland hollows. Three 512 worlds (s25, s29, s36) have no lake. 2048 lakes are large (up to 32,145 cells, 8.2 km²) and the lake cap (0.03 of land) binds in every 2048 world.
- **Odd shapes (elevation, not hydrology).** 2048 seed 3 has a thin, curved, moat-like lake around a low hill (cells x 450–850, y 1330–1650; `sheets/p1-02-2048-seed-3-ring-lake.jpg` in the out directory) and a long narrow lake along the foot of the central range: hydrology fills the depressions it is given exactly, so these are kept depressions of elevation's E10. Ring-range basins at 512 (s13, s30, s31, s49) have flat interiors drained through one gap. Worth a look by the critic.

Copied into the repository (388 KB in all):

- `images/p1-02/p1-02-1024-seeds-0-24.jpg`: contact sheet of the 1024 sweep, seeds 0–24, 200 px per map (lakes visible; rivers too thin at this scale).
- `images/p1-02/p1-02-2048-seeds-0-7.jpg`: contact sheet of the 2048 sweep, 320 px per map.
- `images/p1-02/p1-02-1024-seed-13-crop.jpg`: 1024 seed 13, cells x 256–767, y 256–767 at full resolution: the river network, lowland staircase runs, a river along a range foot, lakes with outflow rivers.

The visual checklist of P1-00 13.2 is the world critic's.

## 7. Defects

None. No hard-bound violation, no cross-preset or thread-count difference, no test failure. The `major_rivers` typical miss at 512 is a range to revise (section 4), not a stage defect.
