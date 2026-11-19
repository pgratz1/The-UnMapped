# P1-06 Regions: world critic review

Generator 0.11.0, schema 5, branch `p1/06-regions-int` (worktree `p1-06-regions-tests`, binary `~/.cache/the-unmapped/build/p1-06-regions-tests/linux-gcc-release/tools/worldgen-cli/worldgen-cli`, run with `--data <worktree>/game/data`). Reviewed 2026-11-18 against design sections 4 ("Regions"), 5 and 6 and spec `docs/specs/p1-06-regions.md`.

**Worlds (24):** 512 seeds 0–11; 1024 seeds 0, 1, 2, 3, 4, 75, 133, 185 and the phrase "The Unmapped"; 2048 seeds 0, 1, 2.

Everything is under `~/.cache/the-unmapped/out/p1-06-critic/` (`<dir>` below):

- `db/`, `rep/`, `img/<size>-<seed>.<layer or composite>.png`: worlds, `report` output, renders (`regions`, `biomes`, `map`, `region`, `habitability`, `elevation`, `ore`, `walk_area`).
- `ov/<size>-s<seed>.png`: overlays drawn for this review on the biome composite: borders **black** where walkable (`walk_pairs` > 0 for that pair), **red** where not, **cyan** on large-river pairs (class ≥ 6); class ≥ 6 river cells strong blue; seeds red with `id name h<habitability_mean>`, `U` for report-upland; pass paths magenta.
- `sheet-512-regions.png`, `sheet-1024-regions.png` (the nine 1024 overlays), `sheet-2048-regions.png`.
- `crops/`: the crops cited below (same colour key; `map` composite where relief matters).
- `an/*.json`: per-region and per-border metrics; `tools/` the scripts.

The `report` region keys of every world fall in the spec's typical ranges (`regions` 11–16 / 47–56 / 195–204; `region_area_km2` p50 1.34–2.36 / 1.71–2.26 / 1.97–2.05; p100 ≤ 6.37).

## 1. Verdict

**Merge, with one small rule change to how passes are attached to borders (F1) or, failing that, a documented meaning for `pass_id`; everything else is tuning for later or accepted.** Regions are compact, well-sized, well-connected units with a natural centre; in mountains their borders mostly sit on ridges, and the tables carry real contrast between homelands.

## 2. What works

- **Size and shape.** Median region 1.7–2.3 km² at every size, largest 3.3–6.4 km². No region of ≥ 0.5 km² has an isoperimetric quotient below 0.15; no slivers, no wrapped or ring regions, **no enclaves** (no inland region with a single neighbour in any of the 24 worlds). Mean neighbours per region 4.5–4.9 at 1024, 5.1–5.3 at 2048: a graph history can reason over.
- **Seeds are natural centres.** Seed offset from the region centroid, as a fraction of the region's equivalent radius: median 0.26–0.65. Twin seeds (< 200 m apart, the bank rule) 0–3 per world. My P1-05 worry (seeds strung along one creek) did not materialise.
- **Mountain borders read as divisions.** At 2048 the central massif of seed 0 is cut along its crests into upland regions seeded in high valleys and on tarn shores (`crops/2048-s0-centre-x800-y600.png`). Crest share of high-relief border pairs 0.18–0.32 at 1024 and 2048 (0.14–0.60 at 512) against a trough share of 0.01–0.07: borders are on crests three to ten times as often as in valley floors.
- **Large rivers are borders** where they matter most: class ≥ 6 river cells on a border 0.68–0.86 at 1024 (one exception, F4), 0.82–0.85 at 2048; class ≥ 8 0.77–1.00 in 7 of 8 1024 worlds that have one.
- **Uplands are places.** Report-upland regions: 0–6 at 512, 3–14 at 1024, 12–28 at 2048. Most have reachable rich ore (6 of 9 in the phrase world, 7 of 9 in 1024 s133, 6 of 12 in 1024 s0, 7 of 12 in 2048 s1), so dwarf homelands and lairs have regions of their own, ringed by barrier borders (14–40 per 1024 world).
- **Contrast between homelands** (which P1-05 F2 missed at cell scale). `habitability_mean` over lowland regions (≥ 50% walkable, ≥ 0.5 km²), 10th / 50th / 90th percentile: 23–44 / 60–110 / 96–123 at 1024, 55–57 / 98–105 / 122–123 at 2048. The best region is three to four times the poorest. The top regions are what a map reader would pick: phrase 40 Southwest Downs (grassland, coast, hab 133, fertility 220); 1024 s133 27 East Downs III (grassland on a class-8 river); 2048 s1 115 East Wood XIII (forest on a class-7 river).
- **Ore columns are sensible.** The largest `ore_reach_cells` are on range flanks and plateaus (phrase 36 Southeast Moor 4367 of 5597; 1024 s0 45 South Heath 1900 of 2027); purely upland regions with crag-only ore keep `ore_reach_cells` 0 (phrase 29 Central Peaks II, 760 / 0).

## 3. Findings, most serious first

### F1. `pass_id` marks lowland borders the pass path merely runs through (fix before merge: small rule change in RG12, no schema change; or document)

RG12 attaches a pass to every border its path crosses, and pass paths run 50–230 cells, down into the lowland on both sides. Of the **43** `region_border` rows with `pass_id` > 0 in the 24 worlds, only **12** are crossed near the col (lowest crossing within 50 m of height of `col_dm`; 18 within 100 m). The other 31 are approach borders:

- 1024 s2, P1 (col (666, 440), 421 m): flags 21/26 (correct, low point 138 m from the col) and also 26/31, 26/40 and 31/40, whose lowest crossings are 2.4–3.2 km from the col at 73, 74 and **0.1 m**. The path zigzags along the 26/31 border in the valley below (`crops/1024-s2-pass1-x560-y340.png`, bottom right).
- 1024 s0: none of its four flagged rows has its lowest point within 580 m of a col. Row 14/23 has 124 walkable pairs and a low point at 104 m, 1.3 km from P1's col at 626 m.
- 2048 s0 P1 flags 57/65 (lowest crossing 83 m, 2.4 km from the 1033 m col) as well as the real crest border 65/86.

Why it matters: design 5 makes passes "places to fortify, tax and fight over", and history will look for them through `pass_id`. Read naïvely, `region_borders_pass` (5 in the phrase world with 3 passes, 7 in 1024 s2 with 3) puts pass forts on farmland borders.

Recommendation: attach a pass to a border only where the path crosses it within some height of the col (e.g. max(S(u), S(v)) ≥ `col_dm` − 100 m), or to the single crossing nearest the col along the path, with `passes` still counting every crossing if wanted. If the lead prefers no output change now, amend spec 3.3 to say that `pass_id` means "the pass path crosses this border somewhere", and record in the P3 notes that history locates the pass by `pass.col_x`, `col_y` and the `region` layer.

Related, accepted: the col itself lies 16–336 m from the nearest border (median about 150 m; 9 of 25 passes over 200 m), so the col usually belongs to one region. That is acceptable (one side holds the pass), and `crops/1024-phrase-pass1-x280-y240.png` shows why: the border runs along the north flank about 350 m below the crest.

### F2. Names: heavy repetition, and the noun often misdescribes the region (fix later: names are placeholders, but two cheap RG11 rules would help)

- **Repetition.** At 2048, **83–85%** of regions carry a numeral and the commonest base name has 21–30 members ("Northeast Wood XXX" in 2048 s1, "West Wood XXVIII" in 2048 s0). `crops/2048-s2-lake-names-x1350-y300.png` shows Northeast Wood II to XXV in one view. At 1024: 47–65% numbered, up to "Northeast Wood IX" (1024 s1). At 512: 0–25%, never past II. Only 31–34 distinct base names exist per 2048 world.
- **Upland regions named after lowland biomes.** 3–15 regions per 1024 world have a lowland noun (Wood, Downs, Steppe, Fen) while being under half walkable with ≥ 600 m relief (15 in 1024 s75, 10 in 1024 s0, 9 in 1024 s133). The worst cases are report-upland:
  - phrase 1 "North Downs" (habitability mean 2, 14% walkable);
  - 512 s2 6 "East Wood" (hab 8), which holds the whole eastern range and its lake;
  - 1024 s0 34 "East Downs III" (hab 21, 19% walkable).

  The cause is that the dominant biome is taken by cells, and an upland's cover is split among alpine, moor, heath and forest bands. 8–45 regions per world have a dominant biome under 50% of their land.
- **Lake regions named Wood.** In 2048 s2, Northeast Wood XV is 65% lake and Northeast Wood X 52% (also two in 2048 s1).
- **Taste.** "Downs" for a flat grass plain reads oddly (512 s4, 1024 s3). It is a biome-file noun, so changing it costs nothing.

Recommendations, in order of value:

1. Noun from the most common **upland** biome (moor, alpine, heath) when the region is report-upland (4 × `walk_cells` < `cells`).
2. `Mere` when `lake_cells` ≥ half of `cells`.
3. A ring word or a 16-point sector at 2048, or the spec's own alternative (12 question 1: a second noun from the second biome or the coast).

None blocks P3, since history renames regions.

### F3. Lowland borders are arbitrary lines (accept; known limitation 11.4.1)

In forest and plain the borders are noise-broken Voronoi lines that cross streams at random (1024 s1 north, `ov/1024-s1.png`; 2048 s2 north-east, `crops/2048-s2-lake-names-x1350-y300.png`; 1024 s185 north, `crops/1024-s185-north-x100-y240.png`). A player will not notice, because lowland borders have no ground feature to disagree with. For history it means lowland frontiers are political, which suits claims and wars. No change.

### F4. Some large rivers run inside regions, and the river flag does not say "along" or "across" (accept; history note)

- **1024 s1:** only 50% of class ≥ 6 cells and **44% of class ≥ 8 cells** are on a border. The main north-eastern river runs inside region 4 and is cut crosswise by borders (`crops/1024-s1-bigriver-x600-y230.png`). The 3–13% of class ≥ 6 cells lying 2–8 cells off a parallel border are the same effect in miniature. Other 1024 worlds: 0.68–0.86.
- **River flag:** `river_pairs` > 0 also fires where a river crosses a border. 15–26 river borders per 2048 world have ≤ 3 river pairs (the river runs from one region into the next, so no bridge is needed between them).

For history:

- Bridge need ≈ `river_pairs` × 16 / `length_m` (the median of this ratio is 0.26–1.0 per world), not `river_pairs` > 0.
- A region's own `river_class_max` ≥ 6 means "a river town may need a bridge inside the region".

No generator change.

### F5. Valley-and-slope regions: half farmland, half crag (accept)

15–29 of ~50 regions at 1024 combine ≥ 150 m of relief with 25–75% walkable ground. Examples:

- phrase 42 South Downs: 2.8 km², 1392 m of relief, 49% walkable, p90 habitability 143 and 660 rich ore cells;
- 1024 s0 47 South Wood.

Regions that are also ≥ 15% alpine with p90 habitability ≥ 120 are rarer: 0–4 per 1024 world; 2048 s2 has 19. These are the flanks of narrow ranges, attached to the valley below by design (11.3). For history they are good regions, with a village below and a mine above. The means dilute them, but `habitability_p90` and `ore_reach_cells` recover both halves. The only cost is the misleading noun (F2).

### F6. Islet regions (accept)

Rocks of 0.05–0.07 km² are regions: 1024 s0 has five, among them "Northwest Isle", "II" and "III" in a row (`ov/1024-s0.png`), and 512 s6 has four. They pad the region count and the name list. History should not seed civilizations on regions under about 0.3 km² of dry land.

### F7. Large lakes cut like a pie (accept; cosmetic)

In 2048 s2 a 3 km lake is split among five regions whose borders meet in mid-lake (`crops/2048-s2-lake-names-x1350-y300.png`). In 1024 s1 a lake is split three ways (`crops/1024-s1-bigriver-x600-y230.png`, bottom). This is correct by the design ("lakes belong to land regions"). A viewer that draws region borders should not draw them over open water.

### F8. Small worlds have flat lowland contrast (record; inherited)

Lowland regions at 512:

| 512 world | `habitability_mean` p10 / p50 / p90 | Upland regions |
| --- | --- | --- |
| s0 | 33 / 46 / 54 | 6 of 14 |
| s2 | 37 / 46 / 49 | 6 of 13 |

Every lowland region in these two worlds is about the same. With six upland regions, s0 and s2 offer too few farmland homelands to rank. This is the steep-512 limitation already recorded in P1-05 F7. The regions stage adds nothing to it.

## 4. Patterns across seeds (24 worlds)

| Measure | 512 (12) | 1024 (9) | 2048 (3) |
| --- | --- | --- | --- |
| `regions` | 11–16 | 47–56 | 195–204 |
| Area p0 / p50 / p100, km² | 0.05–1.06 / 1.34–2.36 / 3.3–5.0 | 0.06–0.83 / 1.71–2.26 / 4.0–5.6 | 0.06–0.39 / 1.97–2.05 / 5.4–6.4 |
| Regions < 0.3 km² (islets) | 0–4 | 0–5 | 0–3 |
| Enclaves; IQ < 0.15 (≥ 0.5 km²) | 0; 0 | 0; 0 | 0; 0–2 |
| Report-upland regions | 0–6 | 3–14 | 12–28 |
| Crest / trough share (my measure, 12-cell offsets, ≥ 40 m relief) | 0.14–0.60 / 0.01–0.06 | 0.21–0.32 / 0.02–0.07 | 0.18–0.24 / 0.03–0.05 |
| Class ≥ 6 cells on a border | 0–0.95 | 0.50–0.86 | 0.82–0.85 |
| Barrier borders (`walk_pairs` = 0) | 2–11 | 14–40 | 59–114 |
| Pass-flagged border rows / of them crossed within 50 m of col height | 9 / 6 | 19 / 4 | 15 / 2 |
| Regions with a numeral | 0–25% | 47–65% | 83–85% |
| Largest numeral | II | V–IX | XXVII–XXX |
| Lowland noun on a ≥ 600 m relief, < 50% walkable region | 1–6 | 3–15 | 9–12 |
| Lowland region habitability p10 / p50 / p90 | 23–80 / 44–94 / 49–123 | 23–44 / 60–110 / 96–123 | 55–57 / 98–105 / 122–123 |

## 5. Suggestions (as rules or parameters)

1. **F1 (before merge, or document):** RG12 attaches a pass only to crossings within about 100 m of `col_dm` (or the crossing nearest the col). No schema change; it changes `region_border` bytes and the `region_borders_pass` statistic.
2. **F2 (later; RG11, small):**
   - Noun of the most common upland biome for report-upland regions.
   - `Mere` for regions that are at least half lake.
   - Optionally a different `region_noun` for grassland (biome data).
   - At 2048, consider a ring word or 16 sectors.
3. **Leave the tuning as it is.** `seed_spacing_m` 1200, `hand_ref_m` 25, `river_cost_m` 2000 and `river_border_class` 6 give what the design asks for. I see no case for `seed_spacing_m` 1250 beyond matching the design's counts (Paul's call, spec 12 question 3).

## 6. Notes for history (P3)

- **Passes:** locate them by `pass.col_x`, `col_y` and the `region` layer. Treat `pass_id` as a hint until F1 is changed.
- **Bridges:** use `river_pairs` × 16 / `length_m` (along against across), and the region's own `river_class_max` for crossings inside a region.
- **Homelands:**
  - Dwarves: report-upland regions with `ore_reach_cells` > 0; every 1024 and 2048 world has several.
  - Faeries: `region_biome` wetland shares, which are present but thin at 512.
  - Farming peoples: the lowland `habitability_mean` ranking, which has real spread at 1024 and 2048.
- **Islets:** ignore regions with under about 0.3 km² of dry land for settlement.
- **Size:** use 1024 as the default history size. At 512, worlds such as s0 and s2 have six upland regions and lowlands of nearly equal value (F8).

## 7. Overall judgement of the geography (P1) as a base for history

The geography phase now gives a world that a history can be told on. These parts are sound and need no further work before P3:

- one continent with ranges, plateaus, lakes and rivers that drain sensibly;
- biomes that follow climate and relief;
- resource layers that separate farmland, forest, wetland and ore;
- a region graph of named, compact units with recorded borders, crossings and large-river frontiers.

At 1024 and 2048 every world has distinct lowland homelands, real upland regions with ore, barrier ranges with a few natural passes, and rivers that divide neighbours. That is the material of rival civilizations, contested passes and old frontiers.

Remaining weaknesses, all known and none blocking:

- **Wild uplands are mostly unwalkable** (P1-05 F5), so dwarf and faerie homelands must be placed by region and biome, not by walk area.
- **Coasts score below the interior** (P1-05 F6), so ports must come from history.
- **Lowland regions are political shapes**, not basins.
- **Names are placeholders** that repeat heavily at 2048.
- **512 worlds are tight:** two or three farming regions in some seeds. They suit a short history with few peoples, not all six races.

I recommend closing P1 after F1 is settled, with 1024 as the reference size for P3.
