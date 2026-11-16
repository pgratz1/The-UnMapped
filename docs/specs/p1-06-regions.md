# P1-06 Regions

Status: draft, spec writer, 2026-11-16, awaiting approval. Phase P1 Geography, stage 6 (`regions`). Design doc section 4 (stage 6: "cells are grouped into named regions of a few square kilometres, the unit the history simulation reasons about"), 5 (civilizations "claim a region"; events have a `region_id`; beast populations per region), 6 (table `region`: `id`, `name`, `biome`, `bounds`, summary statistics). Depends on P0-01 to P0-04, P1-00, P1-02, P1-04 and P1-05. Tuning values marked "guess" and every statistics range are starting points (section 11).

## 1. Purpose

Partition the land into regions of a few square kilometres whose borders follow natural divisions, give each a placeholder name and summary figures, and record which regions border which and how hard each border is to cross. History reasons over this graph: it settles regions, claims them, fights over borders and builds roads and passes across them; the map and the viewer show regions as the named places of the world.

Natural divisions come from the travel cost of growing regions outward from well-spaced seed cells: climbing is expensive, so two regions meet on a ridge or along a range rather than across it; crossing a large river costs extra, so major rivers become borders; the sea is impassable, so each island holds its own regions; lakes are crossable at a premium. Regions are seeded at the best settlement sites (highest `habitability`), so most regions have a natural centre where history will put its first village. Mountain massifs that hold no good site still get regions where they are far from every other seed.

Size is physical (P1-00 2.2): seeds are about 1.2 km apart at every map size, so a 2048 map has four times as many regions as a 1024 map.

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation`, `water`, `water_depth`, `river` | P1-01, P1-02 |
| `biome`, biome definitions (`region_noun`, `water`) | P1-04 |
| `habitability`, `walk_area` | P1-05 |
| Tuning | definition `worldgen.regions` (section 4) |
| World seed, N | |

S = `elevation` + `water_depth` (dm), as in P1-03. Streams: K_seed = `stream_key(seed, "geo.regions.seed")` (location hash `hash_at`, tie-breaks), K_cost = `stream_key(seed, "geo.regions.cost")` (noise key).

Preconditions (fatal "regions: input precondition: …"): layers present, N × N, catalogue types.

## 3. Outputs

- Layer `region` (`u16`, layer id 18 in a full run): the region id of every land cell (`water` ≠ 3), lakes included; 0 on ocean. Catalogue entry made definite: sea and ocean get no regions in P1 (open question 2).
- Table `region`: one row per region.
- Table `region_border`: one row per pair of regions sharing at least one 4-adjacent pair of cells.

**Schema change (for the lead): schema version 3** (planned in P1-00 9.2 for `region`; `region_border` is new and needs the proposals entry for P1's schema changes updated, P1-00 appendix B8):

```
CREATE TABLE region (id INTEGER PRIMARY KEY, name TEXT NOT NULL, biome INTEGER NOT NULL, x_min INTEGER NOT NULL, y_min INTEGER NOT NULL, x_max INTEGER NOT NULL, y_max INTEGER NOT NULL, seed_x INTEGER NOT NULL, seed_y INTEGER NOT NULL, cells INTEGER NOT NULL, lake_cells INTEGER NOT NULL, river_cells INTEGER NOT NULL, coast_cells INTEGER NOT NULL, elev_mean INTEGER NOT NULL, elev_max INTEGER NOT NULL, habitability_mean INTEGER NOT NULL, walk_area INTEGER NOT NULL, landmass INTEGER NOT NULL) STRICT
CREATE TABLE region_border (a INTEGER NOT NULL, b INTEGER NOT NULL, length_m INTEGER NOT NULL, low_dm INTEGER NOT NULL, walk_pairs INTEGER NOT NULL, river_pairs INTEGER NOT NULL, PRIMARY KEY (a, b)) STRICT, WITHOUT ROWID
```

Executed after the version 2 DDL in every world (empty tables when the stage did not run). If the biome and region branches merge as one, they share one bump (P1-00 9.2). The design doc's `bounds` is stored as the four integer columns `x_min`, `y_min`, `x_max`, `y_max` (inclusive cell coordinates; tables hold no blobs or REAL values, design doc section 6). No foreign-key clauses: `biome` refers to `biome.id`, `walk_area` to the layer of that name, `a` and `b` to `region.id`, and invariants (section 6) check them.

`region` columns: `id` 1 to R; `name` (RG9); `biome` the dominant biome code; bounds; `seed_x`, `seed_y` the region's seed cell (a dry or river cell inside it); `cells` (land cells, lakes included), `lake_cells`, `river_cells`; `coast_cells` (cells with an ocean 4-neighbour); `elev_mean` (RHA mean of `elevation`, dm), `elev_max` (dm); `habitability_mean` (RHA mean over the region's cells, 0 to 255); `walk_area` (the most common non-zero `walk_area` in the region, ties to the smaller id; 0 if none); `landmass` (the 8-connected land component of the region, numbered by G2).

`region_border` columns: `a` < `b`, region ids; `length_m` = 16 × the number of 4-adjacent cell pairs (one cell in each); `low_dm` = the smallest over those pairs of max(S of the two cells), the height of the lowest crossing; `walk_pairs` = pairs whose two cells have the same non-zero `walk_area` (a walker can cross there); `river_pairs` = pairs in which either cell is a river cell of class ≥ `river_border_class`.

## 4. Tuning

Fields of `worldgen.regions`, all required, in declaration order:

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `seed_tile_m` | int(256, 32768) | 864 | guess | side of the square tiles from which one candidate seed each is taken |
| `seed_spacing_m` | int(16, 32768) | 1200 | guess | least distance between two seeds of the greedy selection |
| `cost_noise` | `fbm_m` | wavelength 1500, octaves 3, gain "0.5" | guess | irregularity of borders on open ground |
| `cost_noise_amp` | fx(0, "0.9") | "0.5" | guess | horizontal cost factor = 1 + amp × noise |
| `climb_cost` | fx(0, 100) | 10 | designed (Naismith: 1 m of climb ≈ 8–10 m of walking) | cost of 1 m of height change, in metres of level walking |
| `lake_cost_factor` | fx(1, 10) | 2 | guess | horizontal cost factor on lake cells |
| `river_border_class` | int(1, 16) | 4 | guess | smallest river class that costs extra to cross (16: none) |
| `river_cost_m` | int(0, 100000) | 60 | guess | extra cost per class step at and above `river_border_class`, metres |
| `region_min_area_m2` | int(0, 1000000000) | 750000 | guess | smaller regions are merged into a neighbour (2930 cells) |
| `name_central_frac` | fx(0, 1) | "0.12" | guess | regions whose seed lies within this fraction of the map side from the land centroid are "Central" |

```toml
# game/data/worldgen/regions.toml and the fixtures under worldgen/tests/data/geo*/worldgen/
[worldgen.regions]
seed_tile_m = 864
seed_spacing_m = 1200
cost_noise = { wavelength_m = 1500, octaves = 3, gain = "0.5" }
cost_noise_amp = "0.5"
climb_cost = 10
lake_cost_factor = 2
river_border_class = 4
river_cost_m = 60
region_min_area_m2 = 750000
name_central_frac = "0.12"
```

`seed_tile_m` and `seed_spacing_m` are physical (P1-00 2.2); `name_central_frac` is a map fraction. Named checks: none.

## 5. Algorithm

Notation of P1-01 5.0; X, Y as in P1-03 5. "Site cell": `water` ∈ {0, 1} (dry land or river; not lake). The **site key** of a cell is the pair (`habitability`, h) with h = `hash_at(K_seed, x, y)` (`uint64`), compared lexicographically; "best" means the largest key, ties (equal habitability and hash) to the smaller index. Steps RG1 to RG11.

### RG1. Preconditions

Section 2.

### RG2. Candidates (D1 per tile, then concatenated in tile order)

t = `seed_tile_m` div 16 cells (54 with the default). Tiles are the squares [tx·t, tx·t + t − 1] × [ty·t, ty·t + t − 1] clipped to the map, for tx, ty = 0, 1, …. Each tile with at least one site cell contributes its best site cell as a candidate.

### RG3. Greedy seeds (D7 sort, D4 selection)

Sort the candidates by descending site key, then ascending index. Walk the sorted list; accept a candidate when, for every seed accepted so far, (16 Δx)² + (16 Δy)² ≥ `seed_spacing_m`² (`int64`). Accepted seeds are numbered 1, 2, … in acceptance order.

### RG4. One seed per landmass (D5)

Label the 8-connected components of land (`water` ≠ 3) with G2 (labels are the `landmass` values). For each landmass in label order that contains no seed, append a seed at its best site cell (or, if it has none, its smallest-index cell), numbered after the existing seeds.

### RG5. Cost factor (D1, G6 row blocks)

nf = ONE + mul(`cost_noise_amp`, fbm_m(K_cost, X, Y, `cost_noise`)) per cell; nf ∈ [0.1, 1.9].

### RG6. Cost flood (D6)

A multi-source Dijkstra over land cells with integer costs in decimetres of level walking. For a step from cell c to its neighbour m (8-neighbour order):

```
hz    = orthogonal ? round_int(mul_int(nf[m], 160)) : round_int(mul(nf[m], mul_int(SQRT2, 160)))
if water[m] = 2:  hz = round_int(mul_int(lake_cost_factor, hz))
climb = round_int(mul_int(climb_cost, |S[m] − S[c]|))
rv(p) = (water[p] = 1 and river[p] ≥ river_border_class) ? river_cost_m × 10 × (river[p] − river_border_class + 1) : 0
cost  = hz + climb + rv(m)
if diagonal and water = 1 at both corner cells (m.x, c.y) and (c.x, m.y):
    cost = cost + max(rv(corner 1), rv(corner 2))       // a diagonal step may not slip between two river cells
```

```
best[·] = INF; owner[·] = 0; done[·] = false; seq = 0
for each seed s in seed order:  best[s] = 0; owner[s] = s's number; push (0, seq, s); seq += 1
while the queue is not empty:
    pop the entry (k, q, c) with the smallest (k, q)
    if done[c]: continue
    done[c] = true
    for each neighbour m of c in the 8-neighbour order, m land and not done[m]:
        nk = k + cost(c → m)
        if nk < best[m]:  best[m] = nk; owner[m] = owner[c]; push (nk, seq, m); seq += 1
```

Every land cell is reached (each landmass has a seed). Equal costs keep the earlier owner (strict improvement only), so ties go to the seed whose front got there first in (key, seq) order. Each region is 8-connected (every cell's owner is that of the cell that last improved it, which is in the same region and closer to the seed).

### RG7. Merge small regions (D4)

min_cells = (`region_min_area_m2` + 255) div 256. Border counts n(i, j) = the number of 4-adjacent land-cell pairs with owners i ≠ j. Repeat: among regions with fewer than min_cells cells and at least one neighbour (n > 0), take the one with the smallest (cells, number); merge it into its neighbour with the largest n, ties to the smaller number. The absorbing region keeps its number and its seed; cells and border counts are added. Stop when no such region remains. A region with no neighbour (an island region) is never merged.

### RG8. Renumber (D7)

Regions get ids 1, 2, … in order of their smallest cell index; `region` = id on land, 0 on ocean. More than 65535 regions is fatal (impossible with the shipped tuning: at most one seed per 54 × 54 tile plus one per landmass).

### RG9. Properties and names (D8 reductions, then sequential)

The columns of section 3, by exact integer reductions over each region's cells (sums, counts, minima and maxima; argmax ties to the smaller code or id).

**Dominant biome**: the biome code with the most cells among the region's cells whose biome row has `water` = 0, ties to the smaller code; if the region has none, the code of the `lake` biome.

**Name** = "<sector> <noun>", with:

- Land centroid (cx, cy) = (RHA(Σx / L), RHA(Σy / L)) over all land cells (L of them, `elevation` > 0). r_c = round_int(mul_int(`name_central_frac`, N)). For the region's seed, dx = seed_x − cx, dy = seed_y − cy (y grows southward).
- Sector: `Central` if dx² + dy² < r_c²; otherwise with ax = |dx|, ay = |dy|: if 29 ay ≤ 12 ax, `East` (dx > 0) or `West`; else if 29 ax ≤ 12 ay, `South` (dy > 0) or `North`; else `North` or `South` by the sign of dy followed by `east` or `west` by the sign of dx (`Northeast`, `Southwest`, …). (12/29 = 0.4138 approximates tan 22.5° = 0.4142.)
- Noun: `Isle` if the region is the only region on its landmass and that landmass is not the largest (most cells; ties to the smaller label); otherwise the `region_noun` of the dominant biome.
- Duplicates: going through regions in id order, the k-th region (k ≥ 2) with a given name gets " " and the Roman numeral of k appended (`East Wood II`). Roman numerals in the standard subtractive form (I, II, III, IV, …, IX, …, XL, …).

The names are placeholders in plain English for the map, the viewer and the critic; history may give regions cultural names in its own tables (open question 1).

### RG10. Borders (D8 counts per pair, then sorted)

For every 4-adjacent pair of land cells in different regions (each unordered pair once: from each cell to its E and S neighbours), with a < b the two ids: add 16 to `length_m`; take min into `low_dm` of max(S, S); add to `walk_pairs` and `river_pairs` as defined in section 3. Rows sorted by (a, b).

### RG11. Write

Layer `region`; table `region` rows in id order; table `region_border` rows in (a, b) order; one transaction.

### 5.1 Worked examples

Computed by the spec writer's exact-integer reference (P1-03 5.1) and, for the flood, checked by hand on the middle row. Unless stated: 9 × 5 test grids of land (`water` 0) with S = 10 dm, nf = ONE (`cost_noise_amp` 0), shipped `climb_cost` 10, `lake_cost_factor` 2, `river_border_class` 4, `river_cost_m` 60; orthogonal steps cost 160 and diagonal steps 226 (round of 226.27).

**RG6, flat, seeds A (1, 2) and B (7, 2).** Owner: columns 0 to 4 → A, 5 to 8 → B. Costs, row 2: `160 0 160 320 480 320 160 0 160`. (4, 2) is a tie (A and B both reach it at 480); A's front got there first in (key, seq) order (A's seed has seq 0), so A keeps it. Row 0: `386 320 386 452 612 452 386 320 386`.

**RG6, ridge**: as above with S = 110 in column 3. Owner: columns 0 to 3 → A, 4 to 8 → B. Row 2 costs `160 0 160 1320 480 320 160 0 160`: A climbs onto the ridge for 320 + 1000; from there (4, 2) would cost 1320 + 1000 + 160 = 2480 against B's 480. The border moves from between columns 4 and 5 to the ridge's far foot.

**RG6, river**: seeds A (1, 2) and B (8, 2), column 3 a river (`water` 1). Without the river the border is between columns 4 and 5 (row 2: `160 0 160 320 480 480 320 160 0`). With class 4 (rv = 600): owner columns 0 to 3 → A (the river cells: A 920 against B 1400), 4 to 8 → B (row 2: `160 0 160 920 640 480 320 160 0`); the border is now the river. With class 3 (below `river_border_class`): no extra cost, the border stays between columns 4 and 5.

**RG6, diagonal river**: river cells (class 5, rv = 1200) at (2, 0), (3, 1), (4, 2), (5, 3), (6, 4); seeds A (0, 4), B (8, 0). Owner rows: `1 1 1 2 2 2 2 2 2`, `1 1 1 1 2 2 2 2 2`, `1 1 1 1 1 2 2 2 2`, `1 1 1 1 1 2 2 2 2`, `1 1 1 1 1 1 2 2 2`. Costs row 0: `640 706 1972 800 640 480 320 160 0`. Each river cell goes to the side that reaches it cheaper; no front crosses between two diagonal river cells for free (the corner rule), so every cell right of the river belongs to B.

**RG3 greedy** (`seed_spacing_m` 1200: the condition is Δx² + Δy² ≥ 5625 cells²). Candidates (habitability, cell), with c1 and c2 tied at 200 and h(c2) > h(c1): c2 (160, 100), c1 (100, 100), c3 (150, 175) at 180, c4 (40, 40) at 150, c5 (175, 120) at 90. Accept c2; reject c1 (3600); accept c3 (10² + 75² = 5725); accept c4 (from c2 18000, from c3 30325); reject c5 (625 from c2). Seeds 1 = c2, 2 = c3, 3 = c4.

**RG7 merge** (min_cells 100): regions 1 (500 cells), 2 (120), 3 (90), 4 (60, an island: no neighbour); n(1, 2) = 30, n(1, 3) = 12, n(2, 3) = 12. Region 3 is merged (smallest small region with a neighbour) into 1 (tie 12 = 12 to the smaller number); region 1 now has 590 cells and n(1, 2) = 42. Region 4 stays. Result: 1 (590), 2 (120), 4 (60).

**RG9 names** at N = 1024, land centroid (500, 480), `name_central_frac` "0.12" (raw 515396076): r_c = round_int(122.88) = 123. Seeds and dominant biomes: (520, 470) grassland → d² = 500 < 15129: `Central Downs`; (800, 500) forest → dx 300, dy 20, 29 × 20 ≤ 12 × 300: `East Wood`; (200, 200) alpine → dx −300, dy −280: `Northwest Peaks`; (510, 900) marsh → 29 × 10 ≤ 12 × 420: `South Fen`; (790, 560) forest → `East Wood` again, so as the fifth region in id order it is `East Wood II`. Sector checks: (10, 4) East; (10, 5) Southeast; (−3, −7) Northwest; (0, −9) North; (4, 10) South; (5, 12) Southeast. Roman: 1 I, 4 IV, 9 IX, 14 XIV, 40 XL.

**RG10 borders.** The ridge example: the regions meet along columns 3 and 4, five pairs: one row (1, 2), `length_m` 80, `low_dm` 110 (every pair has the ridge cell at 110), `walk_pairs` 5 if all cells share `walk_area` 1, `river_pairs` 0. The class-4 river example: (1, 2), `length_m` 80, `low_dm` 10, `river_pairs` 5.

## 6. Invariants

1. `region` > 0 exactly on land cells (`water` ≠ 3), lakes included; 0 on ocean.
2. Region ids are 1 to R, each used; increasing with each region's smallest cell index; R equals the number of `region` rows.
3. Each region is 8-connected and lies on one landmass; `landmass` is that landmass's G2 label.
4. Every region has at least min_cells cells, unless it has no 4-adjacent neighbour region.
5. Every table value equals its definition recomputed from the layers (cells, bounds, means, dominant biome, walk area, seed inside the region on a site cell); `region_border` has exactly the pairs that share a 4-adjacent cell pair, with a < b and the recounted values.
6. Names are unique.
7. Pure function of the seed, N, the inputs, the biome definitions and the tuning; independent of thread count.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| RG2 | D1 per tile; argmax ties to the smaller index (D8) | candidates concatenated in tile order (ty, then tx) |
| RG3 | D7, D4 | strict total order (key, index); sequential acceptance |
| RG4 | D5 (G2) | |
| RG5 | D1, G6 row blocks | location-keyed noise |
| RG6 | D6 | pop by (cost, seq); strict improvement; neighbours in 8-neighbour order |
| RG7 | D4 | stated selection order and tie-breaks |
| RG8 | D7 | by smallest index |
| RG9, RG10 | D8 | integer sums, counts, min/max; argmax ties stated; rows sorted |

Overflow: per step hz ≤ round(1.9 × 226.3) × 10 = 4300, climb ≤ 100 × 30000 = 3 × 10^6, river terms ≤ 2 × 100000 × 10 × 15 = 3 × 10^7, so a step costs < 2^25 and a path of at most 2^22 steps < 2^47 (`int64`). Spacing products ≤ 2 × (16 × 2048)² + 32768² < 2^32. Sums of `elevation` over a region ≤ 2^22 × 30000 < 2^37.

## 8. Performance

Budget (P1-00 11): 0.3 / 1.5 / 6 s. The flood dominates: each land cell is finalized once with up to 8 relaxations, a binary heap of at most 8 entries per cell (about 0.5–1 s at 1024 by estimate); 3 noise octaves per cell (RG5, parallel); the rest is linear or over at most a few hundred regions.

## 9. Report and render

**`report`**, appended to `[geography]` when `region` and the two tables are present:

| Key | Value |
| --- | --- |
| `regions` | number of `region` rows |
| `region_area_km2` | percentiles 0, 50, 100 of `cells` over the rows, each as km² with two decimals: v = RHA(cells × 256 / 10^4), printed v div 100, `.`, v mod 100 (two digits) |
| `region_borders` | number of `region_border` rows |
| `region_borders_walkable` | rows with `walk_pairs` > 0 |
| `regions_isolated` | regions with no border row |

**Statistics** (P1-00 13.1; **provisional**, from the floating-point prototype of P1-03 section 11 with the shipped spacing and merge values but without cost noise, on 3 worlds at 512 and 2 at 1024; reset from the first implementation sweep):

| Statistic | All seeds (hard) | Typical (≥ 90%) | Prototype |
| --- | --- | --- | --- |
| `regions` (1024; ÷ 4 at 512, × 4 at 2048, P1-00 13.1 count rule) | 10–200 | 25–70 | 43–46 (512: 12–13) |
| `region_area_km2` p50 | descriptive | 1.5–4.0 | 1.9–2.6 |
| `region_area_km2` p100 | ≤ 20 | ≤ 8 | 3.3–5.2 |
| `regions_isolated` | descriptive | — | islands only |

**Palette**: `region` is registered with `ids` (P1-00 10.2).

**Composite `regions`** (`--composite regions`, default output `<stem>.regions.png`): the `biomes` composite (P1-04 9), then every land cell whose E or S neighbour is a land cell of another region is drawn (0, 0, 0). Needs `region` in addition to the `biomes` composite's layers.

## 10. Tests required

Library seams (interim): `region_candidates`, `region_greedy_seeds`, `region_flood(S, water, river, nf, seeds, tuning) → owner`, `region_merge(owner, min_cells)`, `region_sector(dx, dy, r_c)`, `roman(k)`, `region_names(...)`, `regions_stage(inputs, definitions, tuning, seed, threads)`.

### 10.1 Unit tests

1. The worked examples of 5.1 exactly (owners and costs of every flood case; greedy; merge; names and sectors; borders).
2. RG4: a test world with a 300-cell island and no candidate on it gets a seed at the island's best site cell, numbered after the greedy seeds; a lake-only landmass cannot occur, but a landmass with no site cell takes its smallest-index cell.
3. RG8 renumbering on a fixture where the greedy numbering differs from the smallest-index order.
4. Table round trip: the rows read back equal the computed ones; `SCHEMA_VERSION` is 3 (or 2 + 1 as merged).

### 10.2 Property tests

Seeds 0 to 63 at N = 128 with fixture data (the shipped spacing gives few regions at 128; also run with a test-only `regions.toml` of `seed_tile_m` 256, `seed_spacing_m` 400, `region_min_area_m2` 65536): invariants 1 to 6 by independent code (its own Dijkstra may differ in queue structure but must reproduce the owners exactly, since pop order is (cost, seq)).

### 10.3 Determinism tests

Threads 1, 2, 5; resume from a world written after resources.

### 10.4 Golden values

Layer hash of `region` and logical hashes of `region` and `region_border` for the cases of P1-03 10.4.

## 11. Known limitations and calibration

1. **Calibration** from the float prototype only (P1-03 section 11): regions followed range flanks and spanned single ranges well; on open lowland, borders were Voronoi-like straight lines, which the cost noise (not in the prototype) is meant to break up.
2. **Regions are compact, not watersheds.** A long river valley is split across several regions along its length; ridges become borders, valley floors are inside regions. That suits settlement (a village and its valley), but a "river basin" is a set of regions, not one.
3. **Only rivers of class ≥ 4 are borders** (≥ 8 × 0.5 km² = 4 km² upstream); with the hydrology tuning of P1-02 many worlds have only a few such rivers per map.
4. **Placeholder names** are English, compass-based and repetitive (`East Wood II`); see open question 1.
5. **No sea regions**: history needs sea areas only for naval events; not in P1.

## 12. Open questions for the lead

1. **Who names regions** (Paul's call). Chosen: geography writes a placeholder name from a compass sector and the dominant biome's noun, so the map and the viewer have readable labels now. Recommendation: keep the placeholder in `region.name`, and let history (P3) add cultural names per civilization in its own table, since a region's name depends on who lives there and when.
2. **No sea regions; lakes belong to land regions.** Recommendation: approve for P1.
3. **Region size** (taste): seeds 1.2 km apart, regions below 0.75 km² merged; prototype median 1.9–2.6 km², about 45 regions at 1024, 12 at 512, 180 at 2048. The design says "a few square kilometres". Recommendation: approve; history can group regions into provinces itself.
4. **The `region_border` table** (new, schema 3): border length, lowest crossing height, walkable pairs, river pairs. Recommendation: approve; it gives history the adjacency graph and tells it which borders need a built pass (`walk_pairs` = 0 with a low `low_dm` is where a pass is worth building).
5. **Borders on ridges vs. along valleys.** Climbing cost puts borders on ridges; the alternative (regions = drainage basins) puts whole valleys in one region but makes huge lowland regions. Recommendation: approve the cost flood.

**Schema and catalogue impact (for the lead):** schema version 3 adds `region` and `region_border` (DDL in section 3); the proposals entry "P1 schema changes" needs the final DDL and the new `region_border`. Catalogue: `region` made definite (0 on ocean only; lakes have regions). New `worldgen` id `regions`; two stream names.
