# Validation: P1-06 regions (generator 0.11.0, schema 5)

Validation specialist, 2026-11-18. Commit validated: `da26478` (local `main`, "Reviews: P1-06 regions world-critic review"; also on origin as `p1/06-regions-final`), the stage with revision r2 (one border per pass, the `Mere` and `Fells` nouns). `GENERATOR_VERSION` 0.11.0, schema version 5, shipped `game/data` with content hash `1543a4ae…83f9c175`. Spec: `docs/specs/p1-06-regions.md` (second version with r2). Determinism audit: `docs/reviews/p1-06-regions-determinism-audit.md` (pass). World critic: `docs/reviews/p1-06-regions-critic.md`. Machine: 6-core Linux desktop. Windows MSVC is checked by CI.

The first round ran on `396f761` (r1, before the critic's review). Its builds, `ctest` runs, extension check, thread check and sweep are kept here where r2 cannot have changed them. r2 changes only `region.name`, `region_border.pass_id` and `passes`, the report key `region_borders_pass` and the content hash (section 1.2).

**Verdict: pass.**

- **Builds and tests.**
  - All five Linux presets build with warnings as errors, on r1 and on r2.
  - `ctest` passes on `linux-clang-release` and `linux-gcc-debug`, the two presets the test writer had not run: on r1 and again on r2.
  - The test writer's gate on r2 covered `linux-gcc-release`, `linux-clang-debug` and `linux-clang-asan` with no sanitizer report, the `regions-slow` suites and the lint.
  - `gdext-headless-check` passes on r1 (`linux-gcc-release` built with the extension). r2 changes no layer and no render, and the `regions` composite of the phrase world is byte-identical between r1 and r2.
- **Hashes.**
  - The twelve CI cases are identical on the five Linux presets.
  - `p1-worlds.txt` is recorded, and `expect` gives 12 of 12 on every preset.
  - The committed `p0-worlds.txt` (re-recorded by the engineer for r2) equals all five presets' worlds.
- **Determinism.** Output does not depend on the thread count or the compiler: 32 of 32 files are equal (r1; r2 adds no parallel step).
- **Invariants.** Invariants 1 to 6 and invariant 8 of section 6 hold in all 271 sweep worlds. They were recomputed independently with numpy from the layers, as was every column of the three tables (section 2).
- **Reference.** The layer, the three tables and the report lines equal the test writer's independent reference (`p1_regions_reference.py`, with its own flood, merge, names, borders and pass attachment) in every compared world: 90 of 90 on r1 and 38 of 38 on r2.
- **Upstream identity.**
  - In all 271 worlds, layers 1 to 18 and tables `pass`, `biome` and `climate` are byte-identical to the 0.10.0 sweep, and every 0.10.0 `[geography]` line is unchanged.
  - So every earlier hard bound still holds, as checked in `p1-05-resources-validation.md`.
  - The r2 worlds equal the r1 worlds in every byte of every layer and table except `region.name`, `region_border.pass_id` and `passes`.
- **Bounds.** No world violates a hard bound of section 9.
  - The typical ranges hold for at least 99% of worlds.
  - Three single-world misses are listed in 3.2.
- **Cost.**
  - The stage computes in 68 ms / 0.29 s / 1.45–1.47 s at 512 / 1024 / 2048, against budgets of 0.3 / 1.5 / 6 s, on a quiet machine.
  - Full geography takes 2.4 / 9.0 / 34 s.

No defect.

## 1. What was run

### 1.1 Checks

| Check | Result |
| --- | --- |
| Configure and build the five Linux presets, r1 and r2 | pass, all five, both commits |
| `ctest` on r1 (19 tests each, four jobs) | pass: `linux-clang-release` 148 s, `linux-gcc-debug` 643 s (load 10–14) |
| `ctest` on r2 | pass: `linux-clang-release` 96 s, `linux-gcc-debug` 386 s (19 of 19 each; the sweep was running alongside) |
| `linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON` (own build tree), `gdext-headless-check`, r1 | pass. The `regions` composite pixels equal `render`, and the legend has the border row. A clean extension build took 773 s under load |
| `determinism_lint.py` | the test writer's gate on r2 (pass) |
| `tools/ci/world-hashes.sh local` over the five presets, r1 and r2 | pass: all 12 cases identical (`hash --detail` and `report` text). r1 took 1593 s under load, r2 took 841 s |
| `world-hashes.sh expect` per preset, after recording `p1-worlds.txt` | pass: 12 of 12 `ok` on all five presets |
| Committed `p0-worlds.txt` (engineer, r2) | confirmed: equals the eight P0 cases on all five presets |
| Stage golden vectors of P1-01 to P1-05 | unchanged since 0.10.0, as P1-06 10.4 requires. Since the 0.10.0 validation commit `38497cb`, `git diff` of `worldgen/tests/golden/` touches only `p0-worlds.txt`, the new `p1-06-regions.txt` and `p1-worlds.txt` (the 0.10.0 recording, merged after `38497cb`) |
| Seed sweep, r2 (gcc-release binary, default threads, two worlds at a time): 512 seeds 0–49, 1024 seeds 0–199, 2048 seeds 0–19, phrase "The Unmapped" at 1024 | 271 of 271 `generate`, `report` and `hash --detail` succeed. stderr holds only the three `--verbose` lines. The same sweep was run on r1 |
| Per-world checks (`analyze.py`, section 2) | pass in all 271 worlds, r1 and r2 |
| Reference comparison (`refcmp.py`: `p1_regions_reference.regions` on each file's stored input layers and table `pass`, compared with the stored layer, tables and report) | r1: 90 of 90 equal (512 seeds 0–49; 1024 seeds 0–29, 75, 133, 185 and the phrase; 2048 seeds 0–5). r2: 38 of 38 equal (1024 the phrase, seeds 0–11, 75, 133 and 185; 512 seeds 0–19; 2048 seeds 0 and 1) |
| r2 against r1 (`r1cmp.py`) | pass in all 271. Layers, `pass`, `biome`, `climate`, `region_biome`, the schema, every `region` column except `name` and every `region_border` column except `pass_id` and `passes` are identical. Names changed in 734 regions, pass columns in 178 border rows |
| Upstream identity against the 0.10.0 sweep (`~/.cache/the-unmapped/out/p1-05-validation/`, the same 271 worlds) | pass in all 271. The only hash lines that change are `file`, `schema`, `table meta`, `table layer`, `layer region` and the three new tables. The only new `report` keys are the eight of section 9, and the new `[tables]` lines are the three tables |
| Thread independence (r1; gcc-release and clang-release; `--threads` 1, 2, 5, 8; 512 s6, 1024 s1, 1024 s5, 2048 s1) | pass: 32 of 32 files equal the sweep's |
| Timing (section 6) | within budget at every size |

### 1.2 What r2 changes, and what it does not

Measured over the 271 worlds:

- `region_borders_pass` falls (3.1).
- Every pass whose path crosses a border is attached to exactly one row.
- Some names change: the new `Fells` and `Mere` nouns, and the numerals of the base names those regions left or joined.
- The `region` layer, `region_biome`, every other column, the schema and all earlier output are byte-identical to r1.
- The README image is therefore unchanged: the composite is byte-identical.

## 2. Invariants and table rules

Checked from the files of all 271 sweep worlds with a numpy decoder of the P0-02 codec. The checks are independent of the implementation. The flood itself is covered by the reference comparison.

- **meta and catalogue.**
  - `meta`: `schema_version` 5, `generator_version` 0.11.0, `stages` = `elevation,hydrology,climate,biomes,resources,regions`.
  - Layer ids 1 to 19, with `region` as id 19, type `u16`, N × N.
  - `[tables]` gives `layer = 19` and lists `region`, `region_biome` and `region_border` with their row counts.
- **Invariant 1:** `region` > 0 exactly on land.
- **Invariant 2:**
  - ids are 1 to R, every id is used, and R equals the number of rows;
  - ids increase with each region's smallest cell index.
- **Invariant 3:**
  - each region is 8-connected (union-find with neighbours joined only within a region);
  - each region lies on one landmass (G2 labels computed independently, numbered by smallest index), equal to its `landmass` column;
  - each region contains its seed, which is a site cell (`water` 0).
- **Invariant 4:** every region below 2930 cells has no neighbour.
- **Invariant 5:** every column is recomputed and equals the stored value.
  - Table `region`:
    - bounds, cells, lake, river and coast counts, `river_class_max`;
    - `walk_cells` and the most common walk area, ties to the smaller id;
    - the dry-cell means with round half away from zero, and `elev_max_dm`;
    - `habitability_p90` by the rank rule;
    - `ore_rich_cells`, and `ore_reach_cells` from an independent 5/7 chamfer distance to walkable ground;
    - the dominant non-water biome.
  - `region_border`, row for row:
    - border pairs enumerated E before S in index order;
    - `low_dm` and its cell from the first pair attaining it;
    - walk and river pairs, `river_class_max`;
    - the r2 attachment of each pass to the crossing with the smallest (d, j), then sequence order, with `pass_id` and `passes`.
  - `region_biome`, row for row.
- **Invariant 6:** names are recomputed in full by RG11 r2 and are unique:
  - the land centroid, `r_c`, the sector test;
  - noun precedence isle, mere, upland, then the biome noun;
  - Roman numerals in id order.
- **Invariant 8 (r2):** Σ `passes` ≤ the number of `pass` rows in every world. Every pass whose path crosses a border is attached to one row, and every pass with a col on its path was found there.
- **Report:** the eight region keys recomputed from the tables equal the printed text in every world. 512 s0 prints the spec's section 9 example exactly. 1024 s0 prints `region_borders_pass = 2`, the r2 figure of section 9.

## 3. Statistics against P1-06 section 9

1024 has 200 seeds, 512 has 50 and 2048 has 20. Areas are in km² (two decimals, as `report` prints them).

### 3.1 Every report key, min / median / max

| Key | Hard bound | 512 (50) | 1024 (200) | 2048 (20) | Phrase 1024 | Spec, measured by prototype (512 ; 1024 ; 2048) |
| --- | --- | --- | --- | --- | --- | --- |
| `regions` | ≥ 1 | 11 / 13 / 16 | 42 / 49 / 59 | 182 / 197.5 / 205 | 51 | 11/13/16 ; 45/49/56 ; 195/200/204 |
| `region_area_km2` p0 | descriptive | 0.05 / 0.81 / 1.17 | 0.05 / 0.56 / 1.08 | 0.06 / 0.41 / 0.81 | 0.82 | 0.05–1.06 ; 0.06–1.03 ; 0.06–0.81 |
| `region_area_km2` p50 | 0.5–5 | 1.38 / 1.96 / 2.54 | 1.60 / 2.05 / 2.67 | 1.96 / 2.07 / 2.28 | 1.78 | 1.45/1.94/2.53 ; 1.74/2.13/2.43 ; 1.97/2.04/2.12 |
| `region_area_km2` p100 | ≤ 15 | 2.52 / 3.88 / 5.64 | 3.37 / 4.84 / 7.41 | 4.49 / 5.31 / 6.37 | 5.25 | 3.10/4.53/5.64 ; 3.37/4.78/6.05 ; 4.49/5.25/6.37 |
| `region_borders` | descriptive | 18 / 24.5 / 31 | 98 / 117 / 143 | 480 / 520.5 / 554 | 126 | 18/24.5/30 ; 105/116/134 ; 510/522/545 |
| `region_borders_walkable` | descriptive | 9 / 16.5 / 23 | 72 / 94 / 124 | 400 / 453.5 / 528 | 95 | 9/17/23 ; 73/91/114 ; 400/476/503 |
| `region_borders_river` | descriptive | 0 / 1 / 6 | 5 / 16 / 36 | 88 / 120.5 / 141 | 19 | 0/1/6 ; 8/15/26 ; 98/122/136 |
| `region_borders_pass` (r2) | ≤ `pass` rows | 0 / 1 / 2 | 0 / 1 / 4 | 0 / 3 / 5 | 3 | 0/1/2 ; 0/2/3 ; 0/2.5/3 |
| `regions_isolated` | descriptive | 0 / 0 / 4 | 0 / 1 / 6 | 0 / 1 / 4 | 0 | 0/0/4 ; 0/1/6 ; 0/1/4 |
| `regions_upland` | descriptive | 0 / 2 / 6 | 0 / 5 / 14 | 1 / 9 / 29 | 9 | 0/2/6 ; 1/6/14 ; 5/10.5/29 |

**Agreement with the spec's prototype.** On the prototype's own seeds (512 seeds 0–19, 1024 seeds 0–29, 2048 seeds 0–5), every min, median and max of every key equals the spec's measured figures exactly. That includes the r1 pass key (0/1/3 ; 0/2/7 ; 0/4.5/6) and the r2 pass key: 0/1/2 ; 0/1.5/3 ; 0/2.5/3, and 13 / 41 / 13 rows with `pass_id` > 0 in total, as in the spec's r2 note. The spec prints the 1024 median as 2, which is the upper median of 30 values; the interpolated median is 1.5. The prototype and the implementation therefore agree, as section 9 expected.

### 3.2 Typical ranges: holds-for rates and misses

| Statistic | Typical, 512 / 1024 / 2048 | Holds for, 512 / 1024 / 2048 | Misses |
| --- | --- | --- | --- |
| `regions` | 10–18 / 40–62 / 175–225 | 100 / 100 / 100% | — |
| `region_area_km2` p50 | 1.3–2.8 / 1.5–2.7 / 1.8–2.3 | 100 / 100 / 100% | — |
| `region_area_km2` p100 | ≤ 6.2 / ≤ 6.7 / ≤ 7.0 | 100 / 99 / 100% | 1024 s84 6.84, s118 7.41 |
| `region_borders_river` | — / ≥ 7 / ≥ 85 | — / 99.5 / 100% | 1024 s130 5 |

The 2048 ranges were set from six worlds. All twenty sweep worlds fall inside them, so they need no widening. If the lead wants the 1024 ranges to cover all 200 seeds (measured min–max widened by about 10%), they would be:

- p100 ≤ 8.2;
- `region_borders_river` ≥ 4.

I have tuned nothing.

### 3.3 Pass attachment (r2)

For each attached pass, I measured how far along its path the attached crossing lies from the col (d, in path steps), and how far the crossing's height max(S(u), S(v)) lies below the elevation of the col cell:

| Size | Passes | Attached | Path steps from col, min / median / max | Metres below the col, min / median / max | Rows with `pass_id` > 0 |
| --- | --- | --- | --- | --- | --- |
| 512 (50) | 33 | 31 | 0 / 6 / 33 | −12.4 / 12.1 / 191.7 | 31 |
| 1024 (200 and the phrase) | 251 | 249 | 0 / 7 / 54 | −36.3 / 33.7 / 460.6 | 249 |
| 2048 (20) | 58 | 58 | 1 / 7.5 / 52 | −0.4 / 30.3 / 328.6 | 58 |

The four unattached passes have paths that cross no border; my recomputation finds no crossing for them either. No row carries two passes anywhere in the sweep, so `passes` is 0 or 1 in every row. A negative height means that the crossing pair's surface is a few metres above the col cell's own elevation, next to it.

The spec's prototype (11, r2 note) measured medians of 3 / 8 / 10 steps and 8 / 35 / 68 m, with maxima of 24 / 40 / 36 steps and 192 / 305 / 289 m.

On the prototype's own seeds the implementation reproduces those figures exactly: medians 3 / 8 / 10 steps and 8.2 / 35.4 / 68.0 m, maxima 24 / 40 / 36 steps and 191.7 / 304.6 / 288.6 m.

The wider sweep goes further. Fourteen attached crossings lie more than 40 steps from their col, or more than 305 m below it:

- 1024: s47 pass 2, s58 pass 2, s72 pass 1, s87 pass 1, s93 pass 1, s95 pass 1, s119 pass 2, s121 pass 1, s149 pass 2, s178 pass 1, and the phrase world's pass 3;
- 2048: s7 pass 2, s8 pass 5, s19 pass 2.

The farthest is 54 steps (about 0.9 km, 1024 s87). The deepest is 461 m below the col (1024 s95 pass 1, 37 steps). These are passes whose col lies deep inside one region: the nearest border the path meets is far down one flank. An example is the phrase world's pass 3, whose col lies inside `South Moor` (`sheets/1024-featured.png`, yellow square). This is the r2 rule working as written. History should still locate the pass itself by `pass.col_x` and `col_y`, as the critic noted.

### 3.4 Names (r2)

| Size | Regions | `Fells` | `Mere` | `Isle` | Worlds with a `Fells` | Worlds with a `Mere` | Names changed from r1 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 512 (50) | 661 | 58 | 0 | 29 | 33 | 0 | 68 |
| 1024 (200 and the phrase) | 9976 | 243 | 6 | 186 | 123 | 6 | 472 |
| 2048 (20) | 3946 | 22 | 13 | 26 | 9 | 8 | 194 |

- Names change by the new noun itself, and by the numerals of the base names that a `Fells` or `Mere` region left.
- `Fells` replaces a lowland noun on upland regions:
  - the critic's phrase-world "North Downs" (habitability mean 2) is now `North Fells`;
  - 1024 s131's thin coastal strip is now `Northwest Fells`.
- `Mere` regions occur only at 1024 and 2048, on large lakes; the critic's 2048 s2 lake regions are among them.
- Every name is recomputed independently and matches (section 2).

### 3.5 Other measures asked for

From the r2 sweep. These columns are identical to r1.

- **Regions with no walkable ground.**
  - Median per world: 0 at 512, 1 at 1024, 4 at 2048 (at most 4, 5 and 17).
  - Of those at least 0.75 km² (not islets): 15 regions in the 50 worlds at 512, 210 in the 201 at 1024, and 87 in the 20 at 2048.
  - They are almost all `Peaks` or `Moor` (from r2 also `Fells`): the upland regions the spec designs (11.3).
- **Largest and smallest regions.**
  - Largest: 512 s12 `Northwest Downs II` at 5.64 km²; 1024 s118 `South Wood` at 7.41 km²; 2048 s2 `East Wood` at 6.37 km².
  - Smallest: islets of 202–236 cells (0.05–0.06 km²), which are never merged (11.4 item 4).
- **Rich ore.**

  | Size | Regions with rich ore (median per world) | With reachable rich ore (`ore_reach_cells` > 0) | Share reachable, over the sweep |
  | --- | --- | --- | --- |
  | 512 | 8 | 7 | 0.89 |
  | 1024 | 21 | 18 | 0.86 |
  | 2048 | 46 | 36.5 | 0.82 |

- **Contrast in `habitability_mean` across regions** (what history needs).
  - The 90th minus 10th percentile spread per world has a median of 76 at 512, 102 at 1024 and 91 at 2048.
  - The smallest spread in any world is 35, 72 and 57. No world is flat.
  - Over lowland regions only (4 × `walk_cells` ≥ `cells`), the medians of the 10th / 90th percentile are 23 / 92, 27 / 116 and 52.5 / 126.
- **Elongated regions.**
  - The principal-axis ratio of a region's cells (regions of at least 0.75 km²) has a median of about 1.4.
  - At 1024, 122 regions in 201 worlds are above 3.5, and 18 are above 5.
  - The extremes are 1024 s63 `Northwest Wood II` at 8.8, 1024 s131 `Northwest Fells` at 7.4, 512 s6 `Northeast Downs` at 6.8 and 1024 s52 `North Steppe` at 6.7.
  - They are thin coastal strips between the sea and a coastal ridge whose crest is the border (`sheets/elongated-regions.png`). This is the cost rule doing what it should, but they make odd homelands.
- **Walk areas split oddly.**
  - A region whose main walk area lies in two or more pieces of at least 977 cells within the region occurs 2 times at 512, 71 at 1024 and 27 at 2048. For example, 1024 s0 `South Steppe` has pieces of 3263, 1723 and 1659 cells. The pieces join outside the region, round a spur.
  - Regions holding a sizeable second walk area occur 12, 124 and 25 times.
  - History should not assume that a region's walkable ground is one piece.
- **Large rivers.** The share of class-6 river cells on or next to a border has a median of 0.71 at 1024 (range 0.23–0.91) and 0.82 at 2048. Section 11 measured 0.70 and 0.83.
- **Seeds on the shore.** 29% of seeds (687 of 2347 in the phrase world, 1024 seeds 0–29 and 2048 seeds 0–3) lie within three cells (48 m) of the sea. The best settlement site of a coastal tile is often on the shoreline, so many coastal regions have their "first village" there. That suits ports, and it is visible on the sheets.

No world lacks regions, borders or a walkable region. No world has an enclave, a region on ocean or a split region.

## 4. Images

Rendered with `worldgen-cli render --composite regions` into `~/.cache/the-unmapped/out/p1-06-validation/renders/`. On the contact sheets in `…/sheets/`, seeds are drawn as red dots with their names, and pass cols as yellow squares with their ids. Each world carries a label with its eight report keys.

- `1024-featured.png`: the phrase, seeds 133, 75 and 185, full resolution;
- `512-regions.png`: seeds 0–11;
- `1024-regions-s0-11.png`;
- `2048-regions-s0-3.png` and `2048-regions-s5-10-13-15.png` (reduced, without names), and `2048-seed-0-names.png` (full resolution);
- `elongated-regions.png`: the four most elongated regions.

The r1 sheets the critic read are kept in `…/r1/sheets/`.

**README image** `docs/images/p1-regions.jpg` (122 kB, 768 × 768) shows the phrase world "The Unmapped" at 1024, framed like `p1-biomes.jpg`. It was composed as follows:

- the `biomes` composite is reduced with Lanczos;
- the border mask of the `regions` composite is box-reduced and blended in as black, so that the one-cell borders stay visible at 0.75 scale;
- the result is saved at JPEG quality 88.

The sources and the composition script are in `…/readme/`.

## 5. Thread independence

Run on r1 (r2 changed only sequential steps: RG11 names and the RG12 pass attachment). Worlds: 512 seed 6, 1024 seeds 1 and 5, and 2048 seed 1. Each was generated with `--threads` 1, 2, 5 and 8 by both `linux-gcc-release` and `linux-clang-release`. All 32 files have the SHA-256 of the sweep's world (gcc-release, default 6 threads). This includes 1024 seed 1, the CI case `geo-seed-1-1024`, at `--threads 1`.

## 6. Performance

The machine was quiet: gcc-release, 6 threads, no other job of mine running. The one-minute load average was 2.9–3.5 during the stage timings, and 3.4–6.0 while my own 6-thread `generate` runs were in flight. The r1 timings taken under load 10–14 are superseded.

**The stage alone.** `regions_stage` (RG1 to RG13) was run five times on each world's stored input layers and table `pass` by the scratch driver `regtime` (section 9). Every run's layer and row counts equal the stored world.

| Size, seeds | Best of 5 | All runs | Budget (P1-00 11) |
| --- | --- | --- | --- |
| 512: 0, 5, 13 | 68 ms | 68–70 ms | 0.3 s |
| 1024: 0, 3, 133 | 287–295 ms | 287–305 ms | 1.5 s |
| 2048: 0, 7 | 1448–1474 ms | 1448–1543 ms | 6 s |

**End to end.** Full `generate` against `--until resources`, three repeats each, wall time and peak RSS by `/usr/bin/time`:

| Size, seeds | Full `generate` | `--until resources` | Peak RSS, full / until resources | World file |
| --- | --- | --- | --- | --- |
| 512: 0, 13 | 2.37–2.68 s | 2.31–2.56 s | 33.1–33.8 / 30.9–31.4 MB | 1.69–1.71 MB |
| 1024: 0, 3 | 8.77–9.25 s | 8.04–9.42 s | 110.7–111.2 / 103.7–104.4 MB | 6.37–6.49 MB |
| 2048: 0 | 33.90–34.33 s | 32.40–32.56 s | 403 / 391 MB | 25.37 MB |

- The stage uses about 23% of its budget at 512, 20% at 1024 and 25% at 2048. At 1024 its run difference is within the run-to-run noise. At 2048 it is about 1.5 s, matching the stage alone.
- **Geography totals** (all six stages) are 2.4–2.7 / 8.8–9.3 / 34 s, against P1-00 11's 4 / 15 / 65 s.
- Peak memory grows by 2–3 / 7 / 12 MB, against budgets of 200 MB / 600 MB / 2 GB.
- **Sweep sizes**, over all 271 worlds: world files 1.62–1.72 MB at 512, 6.18–6.63 MB at 1024 and 24.9–25.4 MB at 2048. Peak RSS was 32–34 / 105–112 / 415 MB.
- The README's "a 1024 world takes about 8.5 seconds" is now about 9 seconds.

## 7. Golden hashes recorded

`worldgen/tests/golden/p1-worlds.txt` was re-recorded for 0.11.0 after the five-preset comparison of r2 passed. The output legitimately changed:

- `generator_version` is 0.11.0 and the schema version is 5, which changes `meta`, the DDL and the page count.
- The shipped data changed `content_hash`: the new `worldgen.regions`, with the r2 fields `names.upland` and `upland_own_noun`.
- The new layer `region`, its row in table `layer`, and the three new tables.

Every earlier layer and tables `pass`, `biome` and `climate` are byte-identical to 0.10.0 (section 1).

| Case | File hash (0.11.0) |
| --- | --- |
| `geo-phrase-512` | `adc1d6cfd13e994358dbeabb8c23ba99214981e2dee192b9f76402447c623123` |
| `geo-seed-0-512` | `6598e94b8a0d0cd20d3a5e6341d7427006a816af13060af93a2d3a7a91dcfce2` |
| `geo-seed-1-1024` | `732f5dac9fbf051821a04d027d1295cba86e7b5e939fb24b76856c2c6ffacac3` |
| `geo-seed-42-512` | `ccf7ad971f48a9ca9e51a319d00fb678f1f7a11edd9ef12cea0b8c7382f4d2d1` |

- `geo-seed-0-512` and `geo-seed-1-1024` equal the sweep's files for 512 s0 and 1024 s1.
- `p0-worlds.txt` is not changed by this branch: the engineer's r2 values equal all five presets' worlds.
- No stage golden vector file was touched.
- The MSVC presets are compared by CI's `compare-hashes`. On the pushed branch `p1/06-regions-final`, the "Check the golden world hashes" step is expected to fail until this commit's `p1-worlds.txt` is merged.

**README example world** (`worldgen-cli generate --seed-phrase "The Unmapped"`, default data root, map size 1024):

- file hash `816fe43fdba936f85b65f4ff8ac14c0fc3f9fed22cd17f0faac053ec61ef285f`;
- content hash `1543a4aef55105d5df66225432e37a7f39e50846e53741b86f084da283f9c175`;
- it equals the sweep's phrase world generated with `--data`.

Its `report` differs from the README's current text only in these lines:

- `[meta]`: `content_hash`, `generator_version = 0.11.0`, `schema_version = 5`, and `stages` ending in `,regions`;
- `[tables]`: `layer = 19`, plus `region = 51`, `region_biome = 255` and `region_border = 126`;
- `[layers]`: `region u16 1024x1024 codec=1 bytes=23636 min=0 max=51`;
- `[geography]`, appended:

  ```
  regions = 51
  region_area_km2 = 0.82 1.78 5.25
  region_borders = 126
  region_borders_walkable = 95
  region_borders_river = 19
  region_borders_pass = 3
  regions_isolated = 0
  regions_upland = 9
  ```

Every other line is unchanged. The full text is in `…/readme/phrase-report.txt`.

## 8. Defects and findings

**No defect.** There is no cross-preset or thread-count difference and no test failure. No invariant or table rule is broken. No earlier layer or table changes. No hard bound is violated. The stage's output equals the independent reference on every compared world.

Findings (not defects):

1. **Single typical misses at 1024**: s84 and s118 have a largest region of 6.84 and 7.41 km² (typical ≤ 6.7), and s130 has 5 river borders (typical ≥ 7). Holds-for is at least 99%. Optional widenings are in 3.2.
2. **Thin coastal strip regions** with principal-axis ratios of 5–9 occur in about one 1024 world in ten (3.5).
3. **A region's walkable ground can be several pieces** (71 regions at 1024; 3.5). History should read `walk_area` per cell, not per region, when it routes on foot.
4. **29% of region seeds lie on the shoreline** (3.5).
5. **Pass attachment farther from the col than the prototype measured**: up to 54 path steps and 461 m below the col in 14 of 338 attached passes (3.3). This is the rule as specified, not a defect. History should take the pass's location from table `pass`.
6. **My scratch timing driver** first passed the pass rows without `col_x` and `col_y`, and the stage correctly refused them with "col cell is not on its path" (the r2 precondition). That was a driver error, not a defect. It is fixed, and the timings above are from the fixed driver.

## 9. Where things are

- **Sweep worlds (r2)**: `~/.cache/the-unmapped/out/p1-06-validation/{512,1024,2048}/seed-<s>.*`, with `report`, `hash --detail`, stderr and `/usr/bin/time` for each world. The phrase world is at `1024/phrase.*`. The binary used is `…/worldgen-cli-0.11.0-gcc-release` (r2).
- **The r1 sweep, its analysis, renders and sheets**: `…/r1/`.
- **Analysis**, in `…/analysis/`:
  - `a512.jsonl`, `a1024.jsonl`, `a2048.jsonl` and `aphrase.jsonl` (per-world checks and statistics);
  - `summary.txt`;
  - `r1cmp.txt`;
  - `ref/ref.txt`.
  - The r1 thread check is in `…/r1/analysis/threads.txt`.
- **Renders and sheets**: `…/renders/` and `…/sheets/`.
- **README image sources**: `…/readme/`, with the report, the default-data world and both composites.
- **Timing**: `…/timing-r2/` (`regtime.txt`, `e2e.txt`, `load.txt`).
- **Logs**: `…/logs/` (builds, `ctest`, gdext, `world-hashes` and `expect`). The world-hash case files are in `~/.cache/the-unmapped/out/p1-06-val/world-hashes/`.
- **Scripts** (scratch, not in the repository): `/tmp/claude-1000/-home-pgratz-Dropbox-Documents-MyCode-The-UnMapped/ce48c02e-8da2-4932-954b-fdafdbb43139/scratchpad/p106val/`:
  - `sweep.sh`, `sweepall.sh`, `analyze.py`, `summary.py`, `r1cmp.py` and `wdecode.py`;
  - `refcmp.py` and `refall.sh`;
  - `threads.sh`;
  - `regtime.cpp` and `timing.sh`;
  - `render.sh`, `sheet.py` and `readme_img.py`.

## 10. Milestone 0 (geography)

With regions merged, phase P1's geography pipeline is complete: elevation, hydrology, climate, biomes, resources and regions.

- **Determinism.**
  - The twelve CI world-hash cases (eight P0 worlds and four geography worlds up to 1024) are byte-identical across `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug`, `linux-clang-debug` and `linux-clang-asan`.
  - Worlds are byte-identical across thread counts 1, 2, 5 and 8 with GCC and Clang at 512, 1024 and 2048.
  - MSVC (`windows-msvc-debug`, `windows-msvc-release`) is compared by CI's `compare-hashes` job.
- **Time and size per map size** (quiet 6-core machine, 6 threads, section 6):

  | Map | Land | Full geography | Budget | Peak RSS | World file |
  | --- | --- | --- | --- | --- | --- |
  | 512 (8 km) | about 40% | 2.4–2.7 s | 4 s | 34 MB | 1.6–1.7 MB |
  | 1024 (16 km) | about 40% | 8.8–9.3 s | 15 s | 111 MB | 6.2–6.6 MB |
  | 2048 (33 km) | about 42% | 34 s | 65 s | 403–415 MB | 24.9–25.4 MB |

- **Known limitations carried from the earlier reviews.**
  - **Elevation** (`p1-01b-elevation-passes-critic.md`, spec 11):
    - low gates on small maps can cut a range nearly to the plain;
    - about two in five 512 worlds have no pass;
    - rare very steep coastal faces (512 s41);
    - smooth filled basins;
    - ring-shaped ranges in a few worlds;
    - at 2048 about one world in twenty has little high ground or a peak above the intended maximum (`p1-04-biomes-validation.md` D1, D2).
  - **Hydrology** (`p1-02-hydrology-critic.md`): rivers on gentle lowland run in angular steps.
  - **Climate** (`p1-03-climate-critic.md`):
    - rainfall depends on map size (small maps are wetter);
    - every world has the same temperature range;
    - passes do not channel moisture.
  - **Biomes** (`p1-04b-biome-variety-critic.md`):
    - a visible heath stripe on some worlds;
    - blocky fen patches at 2048;
    - fen is absent from some 512 worlds.
  - **Resources** (`p1-05-resources-critic.md`, `p1-05-resources-validation.md`):
    - large-river banks score slightly below their hinterland;
    - lowland areas differ little at cell scale;
    - rich ore often lies on unwalkable crags (14–18% of rich-ore regions here have none within 300 m of walkable ground);
    - habitability never exceeds about 206;
    - 512 worlds are mostly mountain;
    - 1024 s133 lies close to the hard `walk_main_share` bound (two equal lowlands).
  - **Regions** (spec 11.4, `p1-06-regions-critic.md`, this review):
    - lowland borders are distance borders broken up by noise, not ridges;
    - long valleys are split along their length;
    - about 30% of class-6 river length runs inside a region;
    - islets of any size are regions;
    - names repeat heavily at 2048 (placeholders);
    - there are no sea regions;
    - thin coastal strip regions occur;
    - a region's walkable ground can be several pieces;
    - seeds often sit on the shoreline.
