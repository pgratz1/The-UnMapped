# P1-05 Resources and habitability: world critic review

Generator 0.10.0, branch `p1/05-resources`. The binary is `~/.cache/the-unmapped/build/p1-05-resources/linux-gcc-release/tools/worldgen-cli/worldgen-cli`, run with `--data <worktree>/game/data`. Reviewed 2026-11-18.

**Worlds:** 512 seeds 0–11 and 28; 1024 seeds 0–11 and the phrase "The Unmapped"; 2048 seeds 0–3. That is 30 worlds.

Everything below is under `~/.cache/the-unmapped/out/p1-05-critic/` (written as `<dir>` from here on):

- `db/`: the worlds.
- `img/<size>-s<seed>.<layer>.png`: every layer, plus the `biomes` and `map` composites.
- `rep/`: the `report` output.
- `sheet-512-a.png`, `sheet-512-b.png`, `sheet-1024-a.png`, `sheet-1024-b.png`, `sheet-2048.png`: contact sheets. Columns are biomes, habitability, fertility, forage, timber, ore, fresh_water and walk_area.
- `crop-*.png`: detail crops.
- Per-world metrics:
  - `m-small.jsonl`: general metrics.
  - `rank.jsonl`: 95th percentile by biome, and habitability by river class and on coasts.
  - `shore.jsonl`: habitability in distance bands from lakes, big rivers and creeks.
  - `ore2.jsonl`: whether ore fields can be reached.

The `report` output for 512 s0 matches the prototype example in the spec digit for digit. The 2048 statistics fall inside the spec's typical ranges.

## 1. Verdict

**Merge with the tuning changes listed below.** The stage is sound, but:

- habitability prefers forest to open farmland, so it ranks farm sites against what a reader of the map expects;
- the flood penalty marks down lake shores and the banks of big rivers.

Both are fixed in `resources.toml` and need no code change.

## 2. What works

- **My recommendations from P1-04b were taken up, and they work:**
  - Steppe fertility is now 0.7.
  - `fertile_share` is measured at 0.65.
  - Wetlands are valued through `forage` instead of fertility. Wet woodland, marsh and fen have mean forage 240+, 240 and about 196.
- **The forage layer reads well.** It shows a fish halo about 300 m wide around coasts, lakes and class ≥ 4 rivers, rich wetlands, middling forest and poor uplands. See `<dir>/img/1024-s0.forage.png`. It does not swamp fertility: habitability correlates with fertility at r = 0.57–0.85, and with fresh water at r = 0.32–0.61.
- **Fresh water reaches a sensible distance.**
  - Dry interfluves exist: 12–24% of dry land has `fresh_water` < 50.
  - 40–56% of dry land has `fresh_water` ≥ 128.
  - The rain floor (median 102–159) keeps wet worlds from going dry.
- **Habitability is neither flat nor binary.** On walkable dry land, the 10th to 90th percentiles run about 30 to 150 at every size. The maximum is 194–198 in every world.
- **The best sites hug water.** The top 5% of cells lie a median 32 m from a river or lake in 25 of 26 worlds measured at 512 and 1024 (44 m in 512 s2). Compared with all dry land, these cells are 1.5 to 3 times more likely to lie within 400 m of a big river (class ≥ 6) or a lake.
- **Ore forms distinct fields on range flanks, not speckle.**
  - Rich fields (≥ 64 cells of `ore` ≥ 128): 2–7 per world at 512, 5–26 at 1024.
  - Fragments under 16 cells: 0–5 at 512, 1–19 at 1024.
  - The largest field holds 18–80% of the rich ore.
  - At 2048 the fields are about 1 km across and strung along the ranges. See `<dir>/crop-2048-s0-x700-y500-ore-hab.png`.
  - Every world has rich ore, and every field of ≥ 64 cells lies within 2 km of land with habitability ≥ 100. Mines can be fed.
- **Walk areas look right at 1024 and 2048.** One dominant lowland in 17 of 17 worlds; its share of land is 0.58–0.83 at 1024 and 0.72–0.85 at 2048. High valleys and cliff-bound pockets split off as small areas.

## 3. Findings, most serious first

### F1. Forest outranks open farmland as a settlement site (should fix, tuning)

What the numbers show:

- **Top sites.** Forest takes a larger share of the top 5% habitability cells than of dry land in 25 of the 26 worlds measured at 512 and 1024; the exception, 512 s1, has the same share in both. Grassland takes a smaller share of the top 5% than of dry land in 11 of 13 worlds at 1024.
  - 1024 s4: grassland is 27% of dry land but 16% of top sites; forest is 51% of land and 83% of top sites.
  - 1024 s6: grassland 25% of land, 8% of top sites.
- **Best values.** The 95th percentile of habitability is higher in forest than in grassland in 27 of 30 worlds: 162–169 against 150–157 at 1024, and 167–170 against 152–155 at 2048.
- **Cause: the weights.** With weights 55/20/15/10, flat forest by a river scores 162 and flat grassland by a river scores 161. Grassland's fertility advantage (255 against 179) is cancelled by forest's timber (255 against 38) and forage (128 against 77).
- **Cause: the alluvial cap.** `alluvial_bonus` 0.3 lifts forest to 232 but cannot lift grassland, which is already at 255. So on every floodplain forest wins: 207 against 177. The spec chose this ("forest by a river (207) … the best sites"). For a score that the spec calls "the generic site score of a farming people", it is the wrong order.
- **What it looks like.** In `<dir>/crop-1024-s3-x450-y150-hab.png`, the grass plain in the south, all yellow in fertility, is no brighter in habitability than the forest in the north. Both show only the halo of each stream.

Why it matters:

- The regions stage seeds its regions at the highest habitability.
- History places humans "over land left open".

With these weights, the first villages of every farming people go into the woods.

### F2. Little contrast between regions in the lowlands (should fix, mostly through F1)

What the numbers show:

- **Little spread between blocks.** I split the map into blocks of 1.5 km (96 cells) and kept blocks that are at least 80% walkable dry land. Their mean habitability spans only 69–94 (10th percentile) to 99–118 (90th percentile) at 1024. The best lowland block is about 1.2 to 1.5 times the worst.
  - 1024 s3: 92 / 98 / 107 (10th / 50th / 90th percentile).
  - 1024 s4: 87 / 103 / 108.
- **Little spread between worlds.** The median lowland block lies at 85–103 in every 1024 world.
- **The sheets show it.** In `<dir>/sheet-2048.png` and `<dir>/sheet-1024-a.png`, habitability is an even teal mesh whose only structure is the stream network, about 400 m apart. Grass plains, forests and steppe are hard to tell apart.

Causes:

- F1: grassland and forest score the same.
- The water factor: `hab_water_min` 0.25 halves a site 400 m from a stream. Streams are dense (dry land is a median 176–252 m from one), so most of the variation is at the scale of a single stream's halo, not of a region.

Why it matters:

- P1-06 uses habitability only by rank, so its seeds will still be placed.
- But its `habitability_mean` will hardly differ between regions, and history's "grow from local resources" will give similar regions similar results.

### F3. Lake shores and big-river banks score below their hinterland (should fix, tuning)

What the numbers show:

- **The first 48 m.** Within 3 cells (48 m) of a lake, habitability is clearly below that of the band 48–128 m out:

  | 1024 world | Lake shore, 0–48 m | 48–128 m out |
  | --- | --- | --- |
  | s1 | 88 | 136 |
  | s4 | 86 | 135 |
  | s5 | 99 | 140 |
  | s8 | 90 | 137 |
  | s9 | 90 | 131 |

  Banks of big rivers (class ≥ 6) behave the same way: 1024 s1 scores 88 against 131, and 1024 s9 scores 86 against 129.
- **The next band.** From 48 to 128 m, big rivers score about the same as creeks, within about 10 points in most worlds. A town by a big river gains almost nothing over a farm by a brook.
- **The flood factor is the cause.**
  - Every cell under 1 m above drainage is halved.
  - Large rivers and lakes have the widest such margins, and wetland biomes fill them: 21–55% of the 48 m lake ring is marsh, fen or wet woodland in 9 of 13 worlds at 1024.
  - Marsh's 95th percentile of habitability is 72–73 in 28 of the 29 worlds with enough walkable marsh to measure (512 s2 is 123), so marsh is effectively always halved.
- **Example.** `<dir>/crop-1024-s9-x526-y175-bigriver.png` shows the largest lake of 1024 s9, a natural site for a lake town. It sits in a dark ring of fen and marsh at habitability 30–70, and the class-7 river feeding it is a dark line.
- **Some lakes score near 0.** Lake shores score about 0–50 in 1024 s3, 1024 phrase, 512 s2 and 512 s3. These are steep-sided mountain tarns, which is correct.

### F4. Most rich ore sits on ground no one can walk to (moderate; history note plus optional tuning)

What the numbers show:

- **Rich ore is mostly unwalkable.** Only 10–73% of rich-ore cells lie within 300 m of a walk area; the median is about 0.3.
- **Some fields cannot be reached at all.** Taking fields of ≥ 64 cells and counting those within 160 m of a walk area:
  - 1024 s11: 0 of 12.
  - 1024 s5: 1 of 6.
  - 1024 s8: 4 of 14.
  - 512 s1: 0 of 2.
  - 512 s3, s6 and s8: one each.
- **Walkable ground is almost bare.** Ore ≥ 96 covers 0% of walkable land in 9 of 13 worlds at 512 and in 3 of 13 at 1024. The 99th percentile of ore on walkable ground is 22–98.
- **Cause.** Ore needs height (`ore_elev_hi_m` 800 m) and steepness (`ore_slope_base` 0.5, full exposure at a slope of 1.0), while walkability ends at 35°. By construction, ore peaks where no one can walk.

The result is plausible: the ore is in the crags and the adit is at their foot. But history must not read "ore under a walkable cell" as the test for a mine site, or dwarves will have nowhere to dig in a quarter of the worlds.

### F5. Wild uplands are almost never walkable, so faerie and dwarf homelands are missing as walk areas (moderate; elevation inheritance, history note)

- **Walkable moor, heath and alpine land:**
  - 512: 0 km² in 8 of 13 worlds and at most 0.53 km² in all of them.
  - 1024: under 0.3 km² in 6 of 13 worlds; at least 1 km² in 5 (the phrase world 4.7, s2 4.5, s7 3.8, s9 1.6, s0 1.5).
- **Wetlands** (marsh, fen and wet woodland as one connected piece) are a homeland of 0.5 km² or more:
  - in 2 of 13 worlds at 512 (s1 0.74, s3 0.56), and under 0.1 km² in s0, s2, s10 and s11;
  - in 10 of 13 worlds at 1024 (largest 0.28–2.14).
- **Farmland** (connected walkable land with fertility ≥ 166):
  - 512: the largest piece is only 1.7 km² in s0 and 2.0 km² in s2; at least 5 km² in 6 of 13 worlds.
  - 1024: at least 13 km² in every world.
- **Forest:** present in every world (2–9 km² at 512, 13–45 km² at 1024).
- **Ore near habitable land:** present in every world (see F4).

The uplands are steep: the spec's limitation 2, from the elevation stage. This stage did not cause it. But it means dwarves and faeries cannot be awakened "in their preferred land" through `walk_area` in most small worlds.

### F6. Coasts score below the inland lowlands everywhere (minor; taste)

Median habitability of walkable dry land within 48 m of the sea, against the median for all walkable lowland:

| Size | Coast | All lowland |
| --- | --- | --- |
| 512 | 9–46 | 46–91 |
| 1024 | 43–68 | 66–94 |
| 2048 | 68–71 | 89–92 |

The coast is lower in all 30 worlds. The causes are the steep coastal rims at 512 and the lack of fresh water between river mouths. The fish halo lifts forage, but its weight is 20.

This is defensible, since the spec dropped the coast bonus on purpose. Ports will have to come from history (river mouths, forage), not from habitability.

### F7. Small worlds are tight but still leave room for a history (record)

- **Two lowlands instead of one** in 5 of 13 worlds at 512: s0, s2, s9, s10 and s28. In these the main walk area holds only 0.18–0.31 of the land, and two areas each hold at least 10% of it.
  - Example: `<dir>/img/512-s0.walk_area.png`, with two lowlands of 4.9 km² and about 3 km².
- **Land with habitability ≥ 64:** 3.9 km² in 512 s2 and 5.5 km² in 512 s0, against 9–15 km² in the others.

At "a few km²" per region, 512 s2 has room for two or three farming regions. That is enough for a short history with few civilizations, not for six races at 1–2 civilizations each. This is the known steep-512 limitation; nothing in this stage makes it worse.

### F8. Artefacts (record; all inherited)

- **512 s28.** The straight diagonal elevation scarp runs from about (300, 290) to (420, 420). It shows as a straight wooded strip in timber and a straight dark line in habitability: `<dir>/img/512-s28.timber.png` and `<dir>/img/512-s28.habitability.png`.
- **512 s4.** The ore field is a saturated rectangle with straight north and south edges, at x 315–445, y 245–275. It follows the straight north scarp of a rectangular plateau and the ore noise. See `<dir>/crop-512-s4-x280-y200-ore-band.png`. It is the most artificial-looking feature in the sample; the source is the elevation stage, not this one.
- **Biome edges.** Forage and timber change in sharp steps at biome edges (forage grass 77 → forest 128). This is expected from per-biome values and does not read as banding.
- **No defects of this stage's own.** I saw no blockiness, contour stripes or speckle in any layer.
- **Stream channels.** Incised channels show as thin lines of fertility 0 with bright fringes (`<dir>/crop-1024-s3-x450-y150-hab.png`). This is correct (steep banks), and it is invisible at map scale.

## 4. Patterns across seeds (30 worlds)

| Measure | 512 (13) | 1024 (13) | 2048 (4) |
| --- | --- | --- | --- |
| `habitable_share` | 0.21–0.66 | 0.47–0.74 | 0.55–0.67 |
| Habitability, 90th percentile of dry land | 82–147 | 127–147 | 143–148 |
| Dry land at habitability 0 | 0.24–0.65 | 0.18–0.46 | — |
| Forest top-5% share > its land share | 12 of 13 (s1 equal) | 13 | not measured |
| Forest 95th percentile > grassland 95th percentile | 10 | 13 | 4 |
| Coast below lowland median | 13 | 13 | 4 |
| `ore_rich_share` | 0.006–0.094 | 0.026–0.060 | 0.020–0.051 |
| Rich ore fields of ≥ 64 cells | 2–7 | 5–26 | not measured |
| ...of which within 160 m of a walk area | 0–4 | 0–21 | not measured |
| Walk areas | 1–5 | 1–5 | 5–10 |
| Main walk-area share | 0.18–0.77 | 0.58–0.83 | 0.72–0.85 |
| Walkable upland ≥ 1 km² | 0 | 5 | not measured |
| Wetland piece ≥ 0.5 km² | 2 | 10 | not measured |
| Largest farmland piece ≥ 5 km² | 6 | 13 | not measured |

## 5. Tuning suggestions (`game/data/worldgen/resources.toml` unless stated)

1. **Habitability weights for farming** (F1, F2):
   - `hab_weight_fertility` 55 → **70**
   - `hab_weight_forage` 20 → **15**
   - `hab_weight_timber` 15 → **10**
   - `hab_weight_ore` 10 → **5**
2. **Alluvial bonus:** `alluvial_bonus` 0.3 → **0.15**.

   Hand-computed with suggestions 1 and 2 (flat, beside a river, not flood-prone):

   | Site | Now | Proposed |
   | --- | --- | --- |
   | Grassland | 161 | 194 |
   | Forest on the floodplain | 207 | 188 |
   | Forest above the floodplain | 162 | 170 |
   | Steppe | 118 | 140 |
   | Wet woodland with fish | 148 | about 146 |
   | Marsh before the flood factor | 116 | 119 |

   Grassland then beats forest everywhere, and the order follows fertility. Ore stays almost entirely out of the lowland score, as it should for a farming site.
3. **Flood penalty** (F3): `hab_flood_factor` 0.5 → **0.75**. Optionally also `hab_flood_hand_m` 1 → **0.5**. Lake shores and big-river banks would then score at or above their hinterland while wetlands stay below dry ground.
4. **Optional: weaker water factor** (F2): `hab_water_min` 0.25 → **0.35**, so that fertility's regional pattern shows through the halo of each stream. Re-measure the block contrast after 1 to 3 before taking this.
5. **Optional: some ore on walkable foothills** (F4):
   - `ore_slope_base` 0.5 → **0.65**
   - `ore_elev_hi_m` 800 → **600**

   This puts more of each field on walkable foothill ground without moving the fields. Check `ore_rich_share` against the spec's bounds afterwards.
6. Leave `fresh_reach_m`, the `fish_*` fields, `walk_slope_max` and `walk_min_area_m2` as they are.

Suggestions 1 to 5 change the habitability and ore statistics of spec section 9. The validation specialist must re-measure the typical ranges, and the spec's worked example for "forest by a river" changes.

## 6. Notes for the regions stage (P1-06)

- Habitability's main variation is the 400 m halo of each stream, so seeds at the highest habitability will sit on creek banks. That is fine as a village site, but check after re-tuning that `seed_spacing_m` 1200 does not put several seeds along one creek while the plain beside it gets none.
- `habitability_mean` will differ little between lowland regions until F1 and F2 are fixed. Do not calibrate region statistics on 0.10.0 habitability as it stands.
- Wide wetland and floodplain margins of lakes and big rivers currently fall in rank (F3). With the RG3 greedy, lakeside seeds lose to creek-side seeds nearby.
- Rich ore lies in walk area 0 or in tiny pockets (F4). A mountain region's `walk_area` will usually be 0 or the lowland's id, not one of its own.

## 7. Notes for the history phase (P3)

- **Mine sites:** test "ore ≥ t within about 300 m of a walk area", not "ore under a walkable cell". In 1024 s11, none of the 12 large fields is within 160 m of a walk area.
- **Dwarves and faeries:** these homelands must not require `walk_area` > 0. Walkable upland is about 0 km² in most 512 worlds. Use the pockets (walk area 0 with habitability > 0) and the biome. The pocket factor of 0.25 already marks these places.
- **Ports and fishing villages:** take them from `forage` and river mouths. `habitability` ranks the coast below the inland lowland in every world (F6).
- **Race weighting:** each race should weight the component layers itself, as the spec intends. Elves (forest, timber, forage), goblins and orcs (fertility, ore), dwarves (ore, steepness, pockets), faeries (forage, wetland, wild uplands) and humans (fertility, water) all have land to choose in every 1024 and 2048 world. At 512 the wetland and upland homelands are missing in about half the worlds (F5), so race placement needs a fallback.
- **Small worlds:** some 512 worlds have two separate lowlands of 3–5 km² each (s0, s2, s9, s10, s28). These suit two rival civilizations across a range, which is good for the tone. History's defaults for civilizations per race should scale with the habitable area.
