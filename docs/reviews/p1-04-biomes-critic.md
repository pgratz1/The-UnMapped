# World critic review: P1-04 biomes (generator 0.8.0 candidate)

Date: 2026-11-17. Reviewer: world critic. Build: `agent-a37c4e419c42080db/linux-gcc-release` (`worldgen-cli 0.8.0 schema 3 sqlite 3.54.1`).

## 1. Verdict

**Merge as is.** I found no blockers. The biome maps read as believable islands at map scale. At walking scale the forest and grassland form real groves and clearings, and rain shadows are easy to see. Four weaknesses should be fixed in follow-ups, in this order:

1. Every range carries the same smooth alpine cap and moor ring.
2. The first kilometre of every coast is forest, whichever way the wind blows.
3. Marsh never becomes a fen big enough to be a region.
4. Rivers through grassland carry no trees.

The first, third and fourth can be fixed inside the biome stage or its data. The second belongs to the climate stage.

## 2. Material examined

- **Engineer's material:** all of it (`sheets/`, `renders/`, `shares-vs-spec.txt`) and the 38 worlds in `w/` (512 seeds 0–9, 1024 seeds 0–19, 2048 seeds 0–7).
- **My own worlds:** 512 seeds 10–24, 1024 seeds 20–34 and 2048 seeds 8–9, in `critic/w/` with reports. That makes **25 worlds at 512, 35 at 1024 and 10 at 2048**, 70 in all.
- **Images:** a composite and a flat layer of every new world in `critic/renders/`, and contact sheets `critic/sheet-1024-s20-34.png`, `critic/sheet-512-s10-24.png` and `critic/sheet-2048-s8-9.png`.
- **Crops:** `critic/crop-*.png`. Each shows the composite on the left and the flat layer on the right. The file name gives the world, the origin cell, the size and the zoom.
- **Measurements:** I decoded every layer with numpy, with a decoder of P0-02 codec 1 copied from the spec writer's `wdb.py`. Per world I measured:
  - shares;
  - the coast rim;
  - the elevation spread of the alpine edge;
  - whether each large alpine area is fully enclosed by moor;
  - how thick marsh patches are;
  - the biomes of river corridors and lakeshores;
  - speckle;
  - "region blocks": 88 × 88-cell tiles of about 2 km², standing in for P1-06 regions;
  - a fertile-lowland proxy and the share of steep land.
- **Prototypes:** two rule changes, shown as images in section 6. They are illustrations computed in numpy, not the stage.

## 3. What works

- **Shares are inside the spec's ranges at every size.**

  | Size | Forest (min / median / max) | Grassland (min / median / max) |
  | --- | --- | --- |
  | 512 | .34 / .43 / .53 | .17 / .32 / .55 |
  | 1024 | .35 / .45 / .59 | .20 / .30 / .45 |
  | 2048 | .44 / .49 / .57 | .26 / .32 / .40 |

  - Every natural biome is present in all 70 worlds.
  - No biome is above 0.59; the highest is forest in 1024 s34.
- **The forest–grassland edge is the best part of the stage.**
  - Copses of 10 to 60 cells sit in open grass, clearings sit inside forest, and the edges are ragged.
  - The crops `crop-1024-s11-x300-y350-300x300-2x.png` and `crop-1024-s9-x330-y150-380x230-2x.png` show it. The second has a big grass clearing set in forest beside a lake.
  - It reads as landscape rather than as rainfall contours.
  - Speckle is negligible: cells in patches under 16 cells are 0.1–0.3 % of land in every world.
- **Rain shadows read at a glance.** Examples:
  - 1024 s8: a grass lee to the NW and forest to the SE.
  - 1024 s29: a grass NW and a forest wedge to the SE.
  - 1024 s32: a dry north and a wooded south.
  - 1024 s12 and s19: open interiors.
- **Windward ranges grow a montane forest belt below the moor**, as they should.
  - 1024 s3: the column at x = 600, from y 644 to 700, is the windward face. Rain there rises from 680 to 1,739 mm, and it is forest up to the moor line.
- **Variety at 1024.** The sample includes:
  - forest islands (8 of 35 above 52 % forest);
  - open interiors (3 above 40 % grassland, with 7 more at 35–40 %);
  - upland worlds (8 with moor and alpine together above 25 %);
  - a moor plateau (1024 s30), a distinctive map.
- **River cells take their banks' biome (B5).** The grey threads that ran down the middle of rivers are gone.

## 4. Findings

### Blockers

None.

### Should-fix (follow-ups; none holds the merge)

**S1. Every range has the same zonation: a smooth alpine cap ringed by a moor band of even width.**

- **Frequency:**
  - Every alpine area of at least 200 cells is fully enclosed by moor; not one touches forest or grassland. That is 71 of 71 at 512, 135 of 135 at 1024 and 65 of 65 at 2048.
  - The alpine edge spans a median of 230 m of height from its 10th to its 90th percentile (115–630 m). It is a contour line.
  - Temperature follows raw elevation exactly: the regression is −0.0065 °C per m of local relief, correlation −0.96 at 9 cells (1024 s5). So the ±1 °C jitter is the only thing that roughens the edge, and on steep flanks it moves the edge only a few cells.
- **Evidence:**
  - `critic/crop-1024-s5-x380-y300-300x300-2x.png`: the moor ribbon is 20–30 cells wide on both flanks, follows no valley or spur, and has a lens-smooth alpine cap.
  - The engineer's `sheets/crop-2048-s5.png` shows parallel stripes of alpine, moor, forest and grassland along the whole range.
- **Why it matters:**
  - At map scale every world reads as green and yellow with white-and-mauve ridges.
  - At walking scale, crossing a range means the same three bands on both sides.
  - The wind, the one thing that makes ranges differ, has no say above 300 m.
- **Smallest fixes, all inside the biome stage:**
  1. **Split moor by rainfall (a data change).**
     - About half of all moor lies below the forest bound: 47–64 % of moor cells at or above 720 mm × scale, in 11 worlds sampled.
     - Moor needs `rain_min_mm` equal to the forest bound. The cool, dry upland then goes either to grassland (no new biome) or to a new `heath` biome (a new code; see section 6).
     - Either way, belts become asymmetric: wet moor on the windward side, heath or upland grass on the lee. Prototype: `critic/proto-heath-steppe-1024.png`.
  2. **An aspect term on `tq`** (a code change; the rule reads it like the jitter).
     - For example ±2 °C scaled by how far the slope faces south.
     - Prototyped on 1024 s5 (`critic/proto-aspect-1024-s5.png`, middle panel): it thins the moor on sunny slopes and adds some ragged steps.
     - The ring stays: still 3 of 3 large alpine areas fully enclosed by moor. It is not worth doing on its own.
  3. **A steep-rock rule** (slope above about 0.7, above 600 m, `tq` at most 2 °C goes to alpine). It breaks the moor band on cliffs (right panel), but the alpine share rises from 7.7 % to 9.6 % on 1024 s5.
- **Recommendation:** option 1. It needs no climate re-validation.

**S2. The first kilometre of coast is forest in every world, including lee coasts.** This is the climate's coast bonus.

- **Frequency:**
  - Forest's share of forest and grassland on lowland within 1 km of the sea is 0.65–0.83 in all 20 worlds I banded (10 at 2048, 10 at 1024).
  - In the same worlds it is 0.34–0.84 at 1–3 km and 0.15–0.54 beyond 6 km at 2048.
  - On coasts whose hinterland is grassy (over 60 % grass between 200 and 800 m inland), forest covers the shoreline cells at a median of 19 %, 19 % and 15 % at 512, 1024 and 2048. It is above 25 % in 5 of 25, 8 of 35 and 0 of 10 worlds.
- **How visible:**
  - As a thin dark line in the sheets, it is milder than the spec suggests. The engineer's 1024 s8 NW coast (`sheets/crop-1024-s8-nw.png`) is nearly clean. 2048 s5's north (lee) coast (`critic/crop-2048-s5-x850-y120-600x400-1x.png`) is grass to the shore.
  - Where it shows, it is a wedge 30–80 cells deep, for example 2048 s0 east coast (`critic/crop-2048-s0-x1550-y650-450x550-1x.png`, cells x 1,720–1,810, y 820–900).
- **The larger effect is at 2048, where it makes the "wet rim, open interior" pattern.**
  - In 5 of the 8 worlds at 2048 with land more than 6 km from the sea, that interior is at most 25 % forest. In 2 more it is 42 % and 52 %, and only 2048 s7 (54 %) breaks the pattern.
  - The pattern shows in 2048 s1, s3, s4, s6 and s8.
- **Fix:** in the climate stage, lower `coast_rain_mm` from 75 to about 40 (spec open question 10), and re-validate climate. A coast rule in the biome stage would hide the cause. I do not recommend one.

**S3. Marsh never becomes a fen big enough to be a region, and it reads as grey lace.**

- **Frequency:**
  - Marsh is the dominant biome of **0 of 4,355** region-size blocks across all 70 worlds: 0 of 325 at 512, 0 of 1,881 at 1024 and 0 of 2,149 at 2048.
  - Most marsh is thin. The share of marsh cells at least 3 cells inside a patch has a median of 5 %, 11 % and 18 % at 512, 1024 and 2048.
  - At 512, marsh is 0.1–2.1 % of land (median 1.2 %); seed 10 has 52 cells in its largest patch.
- **The largest fens:**

  | World | Largest patch | Location | Crop |
  | --- | --- | --- | --- |
  | 2048 s2 | 13,775 cells (3.5 km²) | x 703–1,000, y 984–1,208 | `crop-2048-s2-x680-y950-360x290-2x.png` |
  | 1024 s13 | 5,606 cells | x 480–692, y 461–622 | `crop-1024-s13-x460-y440-260x200-2x.png` |

  Both are networks of ribbons 3–15 cells wide along class 3 or larger rivers, with broader pools at confluences. At walking scale that is a believable wet floodplain meadow, but not a "Fens" region.
- **What it means downstream:** P1-06 will never name a marsh region, and a race or beast that prefers marsh will find only threads.
- **Colour:** marsh is `0x5C6E54`, which under the composite's shading reads as grey, close to the shadowed forest.
- **Fixes:**
  1. Recolour marsh to a distinct wet teal or olive. This is data only, a palette value in the biome definition. A sample is in the prototype image.
  2. If Paul wants fens, raise `hand_max_m` to 1.5–2 m only where the slope is at most 0.02 and the flow accumulation is large, or add `hand_scale_*` per size (spec open question 8). Both are inside the stage.
  3. Or accept thin marsh, and have P1-06 and history treat marsh as a feature of a region rather than its biome.

**S4. River corridors in grassland are bare. There is no riparian woodland.**

- **Frequency, every size:**

  | Size | Forest within 4 cells of a class 4 or larger river, below 200 m | Lowland overall |
  | --- | --- | --- |
  | 512 | 0.48 | 0.57 |
  | 1024 | 0.41 | 0.57 |
  | 2048 | 0.39 | 0.58 |

- **Evidence:** in `crop-1024-s11-x300-y350-300x300-2x.png` the main rivers cross open grassland with marsh flecks and no trees.
- **Why it matters:** real dry country carries strips of alder and willow along its streams. They are the obvious place for a traveller's path and the first wood a settler cuts.
- **Fix, inside the stage:** a forest rule (or a `wet_woodland` sub-type) with a small `hand_max_m` (about 3 m) to rivers of class 3 or larger and a rainfall bound about 150 mm below the forest bound. Placed after marsh in priority, it turns the river margins in grass country into wooded strips.

### Acceptable (record only)

- **A1. Weak-shadow worlds are forest-heavy.** The highest forest share is 0.59 (1024 s34, wind toward N); 3 of 35 worlds at 1024 are above 0.55. These maps are the least varied but are realistic.
- **A2. One-bank marsh on meanders (512 s1).** It sits on the inner, lower bank, like a point bar, and reads as natural (`sheets/crop-512-s1-biomes.png`). I saw it in 2 of the 25 worlds at 512.
- **A3. Marsh loops in 2048 s2 and s5** trace rivers that themselves loop. That is hydrology's doing; it is noticeable at 1:1 but not wrong.
- **A4. Lakeshores are mixed.** Median marsh share of lake-edge cells is 4 %, 23 % and 31 % at 512, 1024 and 2048, with forest and grassland making up the rest. Small reed fringes sit at inlets (1024 s9). This is fine.
- **A5. Steep, fertile-poor 512 worlds.** This comes from elevation, not biomes, but P1-05 should know.
  - At 512 a median 47 % of land has a slope above 0.5, with 512 s2 at 0.62.
  - My fertile-lowland proxy (forest or grassland below 200 m with slope under 0.15) has a median of 27 % of land at 512, with 512 s2 at 9 % and 512 s0 at 13 %. That is against 45 % at 1024 and 68 % at 2048.
  - Some worlds at 512 will have only two or three habitable regions.

## 5. Patterns across seeds and sizes

- **Region contrast** (share of neighbouring blocks with a different dominant biome, median): 0.53, 0.39 and 0.26 at 512, 1024 and 2048. Block purity (median of the dominant share): 0.64, 0.78 and 0.86.
  - So 2048 gives large, uniform forest and grassland regions with weak contrast between neighbours.
  - 512 gives small, mixed regions that are a third upland: moor and alpine have a median of 24 % of land, and 21 of 25 worlds have a moor-dominated block.
  - 1024 is the best balance.
- **Sizes are not equally interesting.**
  - 1024 has the most varied archetypes.
  - 512 reads as mountains with a skirt: 10 of 25 worlds have moor and alpine above 25 %, marsh is under 2.1 % in every one, and patches are blobbier because the 500 m jitter is 31 cells on a smaller map.
  - 2048 worlds converge: forest 0.44–0.57, grassland 0.26–0.40, no world above 40 % grassland or with uplands above 25 %. Each reads as a wet rim with an open interior laced with grey river marsh.
  - None is empty or cluttered, but 2048 is the most alike from world to world.
- **Fertile cores and marginal land for P1-05 and P1-06.** Forest and grassland lowland gives clear fertile cores at 1024 and 2048. The marginal land is a consistent moor ring at 300–600 m (the lower moor edge sits at a median of 400–600 m by world). Ridges separate peoples well: every range is a moor-and-alpine wall.
  - What is missing is marginal land that is not upland: dry steppe, heath, or wet fen big enough to matter. So every world's frontier is "up the mountain".

## 6. Options for Paul (choices of taste, not requirements)

1. **A sixth natural biome, `heath` (dry upland moor).** This is my first recommendation. Moor keeps the wet uplands; heath takes the cool uplands below the forest rainfall bound.
   - It fixes S1 (asymmetric belts that the wind decides) and gives each range a wet face and a dry face.
   - It is about 3–5 % of land: 4.5 %, 5.4 %, 3.0 % and 4.7 % on 1024 s3, s8, s12 and s19.
   - It costs one data definition and a colour; moor's rule gains a rainfall minimum.
   - Image: `critic/proto-heath-steppe-1024.png` (tan is heath).
2. **A dry `steppe` sub-type of grassland** (rain below about 600 mm × scale) to mark the heart of each rain shadow. It is 5–14 % of land on the same four worlds (pale yellow in the same image).
   - It gives "a dry lee plain" a distinct look and lower fertility for P1-05.
   - The boundary is a smooth rain contour unless it gets the same jitter as the rest.
3. **`wet_woodland` along rivers in grass country** (S4): riparian strips, timber in open land, and a home for faerie or elf sites.
4. **Coastal dunes or cliffs**: I do not recommend these for P1. They are features 16 to 50 m wide, better left to the runtime's scatter from `coast_dist` and slope.
5. **Marsh**: accept thin floodplain marsh, or tune for fens (S3, fix 2). Paul's call, since it decides whether any race or monster can claim a marsh homeland.

The design doc counts six biomes including blighted waste. Adding `heath` makes seven, which should go to `docs/design/proposals.md` through the lead. I have not edited it.

## 7. Summary table

| Id | Class | Frequency | Fix in |
| --- | --- | --- | --- |
| S1 | should-fix | 271 / 271 large alpine areas fully enclosed by moor; edge spread median 230 m | biome data (moor rain split, optional `heath`) |
| S2 | should-fix | coastal 1 km 65–83 % forest in 20 / 20 worlds; 5 of 8 deep interiors at 2048 ≤ 25 % forest | climate (`coast_rain_mm` about 40) |
| S3 | should-fix | 0 / 4,355 blocks marsh-dominated; core share 5 / 11 / 18 % | data (colour); optional stage rule |
| S4 | should-fix | river corridor forest below lowland at every size (0.41 against 0.57 at 1024) | biome stage (riparian rule) |
| A1–A5 | acceptable | see section 4 | none / P1-05 awareness |
