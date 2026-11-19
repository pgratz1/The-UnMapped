# P1-06 Regions

Status: **second version, draft, spec writer, 2026-11-18, awaiting approval.** It replaces the first draft of 2026-11-16 (never implemented), which was written before the biome variety revision (generator 0.9.0) and the resources stage (P1-05 r2, generator 0.10.0); section 0 lists every change. Generator **0.11.0**; **schema version 5** (three new tables, section 3). Calibrated on real 0.10.0 worlds (512 seeds 0–19, 1024 seeds 0–29 and 75, 133, 185 and the phrase "The Unmapped", 2048 seeds 0–5; the P1-05 validation worlds, byte-identical to `main` at `a18d21c`) with a prototype of this spec (section 11). Phase P1 Geography, stage 6 (`regions`), the last geography stage. Design doc section 4 (stage 6; "Regions": about 2 km² each, roughly 12, 45 and 180 per map, bordered on ridges and large rivers, placeholder names such as "East Wood II", land only with lakes in land regions, border length, lowest crossing and whether walkable or a river stored for each pair of neighbours), 5 (civilizations have a home region and claim regions; beast populations are counts per region; passes are places to fortify and fight over; region borders tell history where a crossing needs a road or a bridge; events carry a `region_id`), 6 (table `region` with `id`, `name`, `biome`, `bounds` and summary statistics; schema version 5 planned with `region` and `region_border`). Depends on P0-01 to P0-04, P1-00 (G1, G2, G6, G7, D1 to D10), P1-01 (table `pass`), P1-02 (H7), P1-04 (the `biome` layer and definitions) and P1-05 (the score layers and `walk_area`). Reviews that shaped it: `docs/reviews/p1-05-resources-critic.md` section 6, `docs/reviews/p1-05-resources-validation.md` 3.3.

## 0. Changes from the first draft

| What | First draft (2026-11-16) | This version | Why |
| --- | --- | --- | --- |
| Cost of a step | horizontal × noise + 10 × \|Δheight\| (climbing both ways) + river cost per class step from class 4 | horizontal × noise × **(1 + height above drainage / 25 m)**; no height-change term; a flat **2000 m** to enter a river of class **≥ 6** | measured on 0.10.0 worlds (11.2): the symmetric climbing cost cut mountains into altitude bands, with borders along valley floors and contours, and put no more border length on crests than plain distance did; weighting by height above drainage makes valleys cheap and ridges dear, and about doubles the share of high-relief border on crests |
| Large rivers | class ≥ 4 (1 km² upstream), 60 m per class | class ≥ 6 (4 km² upstream, a river draining about two regions), 2000 m, and seeds on opposite banks do not block each other | class 4 is a stream inside a region; with the first draft's cost 7–9% of class-6 river cells were on a border (plain distance: 0–8%), with this version a median 70% at 1024 and 83% at 2048 |
| Seed key | habitability, then a hash | habitability, then the **lowest surface**, then the hash | in mountain tiles every cell has habitability 0; the hash put mountain seeds anywhere, often on summits; the lowest point is the upland region's natural centre |
| Merge target | longest shared border | longest shared border **not along a large river** | a sliver on one bank merged across the river |
| `region` columns | 18 | 28: adds `river_class_max`, `walk_cells`, the means of all six scores, `habitability_p90`, `ore_rich_cells`, `ore_reach_cells`; heights in `_dm` names | what history reads (3.2); critic F4: rich ore "near walkable ground" |
| `region_border` columns | 6 | 11: adds `low_x`, `low_y`, `river_class_max`, `pass_id`, `passes` | where to build the crossing, how big the river, which natural pass (table `pass`) crosses it |
| New table | — | **`region_biome`** (cells of each biome per region) | beasts are placed by biome and region, races prefer biomes (design 5); a dominant biome alone hides a fen beside a forest |
| Names | sector words hard-coded | sector words and `Isle` in the tuning file; nouns from the biome files | names built from data (lead) |
| Layer id | 18 | **19** | P1-05 added `forage` |
| Schema version | 4 | **5** | version 4 is `climate.temp_offset_dc` (0.9.0) |
| Statistics | float prototype, 5 worlds of 0.3.0 | prototype of this spec on 60 worlds of 0.10.0 | |

## 1. Purpose

Partition the land into regions of about 2 km² whose borders follow ridges and large rivers, give each a placeholder name and the summary figures history needs, and record which regions border which, how long and how high each border is, whether it can be walked, whether a large river runs along it and which natural pass crosses it. History reasons over this graph: a civilization has a home region and claims regions; beast populations are counted per region; events happen in a region; roads, bridges and improved passes are built across borders where `region_border` shows they are needed. The map and the viewer show regions as the named places of the world.

How the borders come about: each region grows from a seed cell over a travel cost. A step costs its length times a factor that grows with the cell's **height above drainage** (P1-00 G7), so travel along valley floors is cheap and over ridges dear; fronts from seeds in neighbouring valleys meet on the ridge between them. Entering a large river costs as much as 2 km of walking, so fronts stop at large rivers, and seeds on opposite banks are both kept. The sea is impassable, so each island has its own regions; lakes are crossed at a premium and belong to the regions around them.

Seeds are spaced about 1.2 km apart and placed at the best settlement site of their neighbourhood (highest `habitability`), so most regions have a natural centre where history may put its first village. Upland that holds no good site still gets seeds where it is far from every lowland seed, placed at the lowest point of their tile; so a massif wider than about the seed spacing is divided into upland regions of its own, and a narrower range is divided along its crest between the regions on either side (11.3, the decision the lead asked for).

Size is physical (P1-00 2.2): about four times as many regions at 2048 as at 1024.

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation`, `water`, `water_depth`, `flow_dir`, `river` | P1-01, P1-02 |
| `biome`; biome definitions: `code`, `water`, `region_noun` | P1-04 |
| `fertility`, `ore`, `timber`, `fresh_water`, `forage`, `habitability`, `walk_area` | P1-05 |
| table `pass` (rows `id`, `path`) | P1-01 9.1 |
| tuning | definition `worldgen.regions` (4) |
| world seed, N | |

S = `elevation` + `water_depth` (dm), the water or land surface, as in P1-03 and P1-05. Streams (names unchanged from P1-00 3): **K_seed = `stream_key(seed, "geo.regions.seed")`**, used only by `hash_at` (site tie-break, RG4); **K_cost = `stream_key(seed, "geo.regions.cost")`**, a noise key used only by `fbm_m` (RG3). No `Rng`.

Checks, each fatal, in this order:

1. **At the stage entry** (`run_regions`, reading the stored world): every input layer present (`regions: input precondition: missing layer '<name>'`, inputs in the order of the table above); each stored layer of its catalogue type (`… layer '<name>' does not have its catalogue type`) and of the size of `elevation` (`… <name> length is not width * height`). Table `pass` is read in `id` order.
2. The thread count in [1, 256] (`regions: thread count outside [1, 256]`).
3. The tuning: each field inside its schema range of 4 (`regions: tuning: <field> outside its schema range`, declaration order), then the named checks of 4 (`regions: tuning: <message>`). With data loaded by P0-03 these cannot fail; they guard the seams.
4. Each biome definition's `region_noun` matches the word grammar of 4 (`regions: biome '<key>' has a region_noun that is not one word`, definitions in ascending `code`). The shipped nouns all match (`Sea`, `Mere`, `Wood`, `Downs`, `Marsh`, `Moor`, `Peaks`, `Waste`, `Heath`, `Steppe`, `Carr`, `Fen`).
5. With the prefix `regions: input precondition: `: the P1-00 grid checks; every input array of length W × H (`<name> length is not width * height`); `water` in 0..3 and `river` in 0..15 (`<name> value <v> at (<x>, <y>)`, lowest index); every `biome` value the `code` of a loaded definition (`biome value <v> at (<x>, <y>) has no definition`); every pass `path` non-empty, a whole number of `u32` cells, each index below W × H, consecutive cells equal or 8-neighbours (`pass <id>: path is not a chain of cells`); then G7 with k = 1 (`regions: input precondition: G7: …`).

## 3. Outputs

### 3.1 Layer and tables

- **Layer `region`** (`u16`, codec 1, **layer id 19** in a full run, P1-00 3): the region id of every land cell (`water` ≠ 3), lakes and rivers included; 0 on ocean. As catalogued (P1-00 4: "0 on ocean only; lakes belong to regions").
- **Table `region`**: one row per region, in `id` order.
- **Table `region_border`**: one row per pair of regions that share at least one 4-adjacent pair of cells, in (`a`, `b`) order.
- **Table `region_biome`**: one row per region and biome code present in it, in (`region`, `biome`) order.

Written in one transaction: the layer, then `region`, `region_border`, `region_biome`.

**Schema change: version 4 → 5** (hard rule 7; planned in P1-00 9.2). Three statements are added to the DDL, executed after `climate`'s, byte for byte:

```
CREATE TABLE region (id INTEGER PRIMARY KEY, name TEXT NOT NULL, biome INTEGER NOT NULL, x_min INTEGER NOT NULL, y_min INTEGER NOT NULL, x_max INTEGER NOT NULL, y_max INTEGER NOT NULL, seed_x INTEGER NOT NULL, seed_y INTEGER NOT NULL, landmass INTEGER NOT NULL, cells INTEGER NOT NULL, lake_cells INTEGER NOT NULL, river_cells INTEGER NOT NULL, river_class_max INTEGER NOT NULL, coast_cells INTEGER NOT NULL, walk_cells INTEGER NOT NULL, walk_area INTEGER NOT NULL, elev_mean_dm INTEGER NOT NULL, elev_max_dm INTEGER NOT NULL, fertility_mean INTEGER NOT NULL, forage_mean INTEGER NOT NULL, timber_mean INTEGER NOT NULL, ore_mean INTEGER NOT NULL, fresh_water_mean INTEGER NOT NULL, habitability_mean INTEGER NOT NULL, habitability_p90 INTEGER NOT NULL, ore_rich_cells INTEGER NOT NULL, ore_reach_cells INTEGER NOT NULL) STRICT
CREATE TABLE region_border (a INTEGER NOT NULL, b INTEGER NOT NULL, length_m INTEGER NOT NULL, low_dm INTEGER NOT NULL, low_x INTEGER NOT NULL, low_y INTEGER NOT NULL, walk_pairs INTEGER NOT NULL, river_pairs INTEGER NOT NULL, river_class_max INTEGER NOT NULL, pass_id INTEGER NOT NULL, passes INTEGER NOT NULL, PRIMARY KEY (a, b)) STRICT, WITHOUT ROWID
CREATE TABLE region_biome (region INTEGER NOT NULL, biome INTEGER NOT NULL, cells INTEGER NOT NULL, PRIMARY KEY (region, biome)) STRICT, WITHOUT ROWID
```

`SCHEMA_VERSION`, `user_version` and `meta.schema_version` become 5; a reader of version 5 refuses version 4 worlds (`world schema version 4, this build reads 5`, P0-02 amendment 16's form). Every world of schema 5 has the three tables, empty when the stage did not run (`--until resources`, `--until none`, `--test-pattern`). No automatic index is created (no `UNIQUE` column; uniqueness of names is invariant 6, checked by tests), so the **empty world grows from 8 to 11 pages, 45056 bytes** (the root pages of the three tables; measured with `VACUUM INTO` on an empty 0.10.0 world plus the three statements). No foreign-key clauses: `biome` refers to `biome.id`, `walk_area` to the layer of that name, `landmass` to the G2 labels of RG6, `a`, `b`, `region` to `region.id`, `pass_id` to `pass.id`; invariants (6) check them. Tables hold no REAL and no BLOB (design 6). The design's `bounds` is the four columns `x_min`, `y_min`, `x_max`, `y_max`.

`report` (P0-04 7) lists the three tables in `[tables]` in ascending name order with the others: `region = <rows>`, `region_biome = <rows>`, `region_border = <rows>` (an empty world: all 0). `hash --detail` (P0-04 8) prints `table region`, `table region_biome` and `table region_border` lines in ascending name order after `table pass` and before the layers, and a `layer region` line after `layer walk_area`. The logical table hash is P0-04's, unchanged.

### 3.2 Columns of `region`

"Dry" cells are cells with `water` ∈ {0, 1} (dry land and river cells; the score layers are defined there, P1-05 3). Every region contains its seed, a dry cell, so every region has at least one dry cell. Means are RHA(sum, count) (round half away from zero; P0-01 5).

| Column | Value | Range | Read by history for |
| --- | --- | --- | --- |
| `id` | 1 … R (RG9) | | `civilization.home_region`, `event.region_id`, `beast_population.region_id` |
| `name` | placeholder name (RG11) | unique | display until history names the region |
| `biome` | dominant biome code (RG10) | a non-water code | the region's character, beast and race preference |
| `x_min`, `y_min`, `x_max`, `y_max` | inclusive cell bounds of the region's cells | 0..N−1 | quick spatial lookup |
| `seed_x`, `seed_y` | the region's seed (RG4, RG7), its best settlement site | a dry cell of the region | the first village site |
| `landmass` | G2 label of the 8-connected land component holding the region (RG6) | ≥ 1 | islands, sea crossings |
| `cells` | land cells (lakes and rivers included) | ≥ 1 | area: `cells` × 256 m² |
| `lake_cells`, `river_cells` | cells with `water` 2, 1 | | lakes and rivers inside |
| `river_class_max` | largest `river` value over its river cells, 0 if none | 0..15 | a large river runs through or along it |
| `coast_cells` | cells with an ocean 4-neighbour | | coastal region, ports |
| `walk_cells` | cells with `walk_area` > 0 | ≤ `cells` − `lake_cells` | how much of it can be walked; 4 × `walk_cells` < `cells` is an upland region in `report` |
| `walk_area` | the most common non-zero `walk_area` value among its cells, ties to the smaller id; 0 if none | | which lowland it belongs to on foot |
| `elev_mean_dm`, `elev_max_dm` | mean and largest `elevation` over dry cells, decimetres | | height, cold, defence |
| `fertility_mean`, `forage_mean`, `timber_mean`, `ore_mean`, `fresh_water_mean`, `habitability_mean` | means of the six P1-05 scores over dry cells | 0..255 | settlement growth from local resources; race preference (each race weights the components itself, P1-05 1) |
| `habitability_p90` | 90th percentile of `habitability` over dry cells (rank rule of P1-00 10.3) | 0..255 | the quality of its best sites, which the mean dilutes in a region of crags and fields |
| `ore_rich_cells` | dry cells with `ore` ≥ `ore_rich_min` (128) | | mine sites |
| `ore_reach_cells` | those of them within `ore_reach_m` (300 m, chamfer of P1-02 H7) of a cell with `walk_area` > 0 | ≤ `ore_rich_cells` | mines that can be reached (critic F4: rich ore lies mostly on unwalkable crags; "test ore ≥ t within about 300 m of a walk area") |

Totals follow from mean × dry cells (dry cells = `cells` − `lake_cells`).

### 3.3 Columns of `region_border` and `region_biome`

A **border pair** is an unordered pair of 4-adjacent land cells in different regions, enumerated from each cell to its E and S neighbours, cells in index order (E before S). For a row (`a`, `b`), `a` < `b`, over its border pairs:

| Column | Value |
| --- | --- |
| `a`, `b` | the two region ids, `a` < `b` |
| `length_m` | 16 × the number of border pairs |
| `low_dm` | the smallest, over the pairs, of max(S(p), S(q)): the surface height of the lowest crossing (a lake surface on a lake) |
| `low_x`, `low_y` | the cell of the first pair (enumeration order) that attains `low_dm`: its cell with the larger S, the first cell of the pair if equal |
| `walk_pairs` | pairs whose two cells have the same non-zero `walk_area` (a walker crosses there) |
| `river_pairs` | pairs in which either cell is a river cell (`water` 1) of class ≥ `river_border_class` |
| `river_class_max` | the largest `river` value over the river cells of the pairs, 0 if none |
| `pass_id` | the smallest `pass.id` of a pass that crosses this border (RG12), 0 if none |
| `passes` | the number of passes that cross it |

The design's "whether it is walkable or a river": walkable ⟺ `walk_pairs` > 0; a river border ⟺ `river_pairs` > 0. A border with `walk_pairs` = 0 is a barrier on foot; its `low_dm`, `low_x`, `low_y` say where a road over it would be shortest in height, and a non-zero `pass_id` that a natural pass already crosses it.

`region_biome`: `region`, `biome` (a code), `cells` (> 0, the region's cells with that `biome` value). Lakes count under the lake code. Σ `cells` over a region's rows = `region.cells`.

### 3.4 What is left to history

History computes these itself from the tables and layers: per-race suitability of a region (its own weights over the means and `region_biome`); region capacity and population; beast populations (from `region_biome`, `forage_mean` and the predator rules of the beast data); cultural names; which borders a civilization claims; roads and bridges (`low_x`, `low_y` and `river_pairs` say where); sea regions and sea routes (none here, 12 question 2); groupings of regions into provinces or realms.

## 4. Data and tuning

New id `regions` of the fixed-id kind `worldgen` (P1-00 6.1), file **`game/data/worldgen/regions.toml`**, with identical copies in `worldgen/tests/data/geo/worldgen/` and `worldgen/tests/data/geo-small/worldgen/` (every data directory with a `worldgen/` directory defines every id, P0-03 amendment 16). All fields required, in this declaration order. "Fitted" = chosen by the calibration of 11; "taste" = a judgement listed in 12; "designed" = follows from the design or another spec.

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `seed_tile_m` | int(256, 32768) | 864 | taste | side of the square tiles from which one candidate seed each is taken (54 cells) |
| `seed_spacing_m` | int(160, 32768) | 1200 | fitted | least distance between two seeds on the same bank (RG5); sets the region size |
| `hand_ref_m` | int(1, 10000) | 25 | fitted | a cell this high above its drain costs twice as much to cross as one at the water |
| `cost_noise` | `fbm_m` | `{ wavelength_m = 1500, octaves = 3, gain = "0.5" }` | taste | irregularity of borders on open ground |
| `cost_noise_amp` | fx(0, "0.9") | "0.5" | taste | the noise factor is 1 + amp × noise, in [0.5, 1.5] |
| `lake_cost_factor` | fx(1, 10) | 2 | taste | extra factor for a step onto a lake cell |
| `river_border_class` | int(1, 16) | 6 | fitted | smallest river class that is a border (4 km² upstream); 16 = none |
| `river_cost_m` | int(0, 100000) | 2000 | fitted | cost of entering such a river cell, metres of level walking |
| `region_min_area_m2` | int(0, 100000000) | 750000 | taste | smaller regions are merged into a neighbour (2930 cells) |
| `ore_rich_min` | int(1, 255) | 128 | designed (P1-05 `ore_rich_share`) | `ore` counted as rich |
| `ore_reach_m` | int(0, 10000) | 300 | designed (P1-05 limitation 12) | reach from walkable ground for `ore_reach_cells` |
| `name_central_frac` | fx(0, "0.5") | "0.12" | taste | regions whose seed lies within this fraction of the map side from the land centroid are central |
| `names` | record `region_names` | below | taste | the words of the names |

Record `region_names`, all fields `string(32, word)` and required, in this order: `central`, `north`, `northeast`, `east`, `southeast`, `south`, `southwest`, `west`, `northwest`, `isle`. **Word grammar**: one ASCII capital letter followed by zero or more ASCII lower-case letters (`[A-Z][a-z]*`), 1 to 32 bytes. The same grammar applies to every biome's `region_noun` (check 4 of section 2). It makes every name a sequence of words without spaces or digits, which the uniqueness argument of RG11 uses. The loader's `string` type takes a grammar (P0-03 4); `word` is a new named grammar beside the id grammar, registered with this kind (a P0-03 amendment for the lead, one line).

**Named checks** (per-definition, P0-03 7.5; reported at the definition's header as `<path>:<line>:<column>: error: check '<name>' failed for 'worldgen.regions': <message>`), in this order:

| Check | Condition | Message |
| --- | --- | --- |
| `regions_tile_fits_spacing` | `seed_tile_m` ≤ 2 × `seed_spacing_m` | `seed_tile_m must be at most twice seed_spacing_m` |

(Without it a tile larger than twice the spacing could leave land more than one spacing from any seed's candidate; the shipped 864 ≤ 2400.)

```toml
# game/data/worldgen/regions.toml and the fixtures under worldgen/tests/data/geo*/worldgen/
[worldgen.regions]
seed_tile_m = 864
seed_spacing_m = 1200
hand_ref_m = 25
cost_noise = { wavelength_m = 1500, octaves = 3, gain = "0.5" }
cost_noise_amp = "0.5"
lake_cost_factor = 2
river_border_class = 6
river_cost_m = 2000
region_min_area_m2 = 750000
ore_rich_min = 128
ore_reach_m = 300
name_central_frac = "0.12"
names = { central = "Central", north = "North", northeast = "Northeast", east = "East", southeast = "Southeast", south = "South", southwest = "Southwest", west = "West", northwest = "Northwest", isle = "Isle" }
```

`seed_tile_m`, `seed_spacing_m`, `hand_ref_m`, `river_cost_m`, `region_min_area_m2` and `ore_reach_m` are physical (P1-00 2.2); `name_central_frac` is a map fraction. The biome nouns stay in the biome files (`region_noun`, P1-04 4.1); this stage changes no biome file.

## 5. Algorithm

Notation of P1-01 5.0 (`Fx` is Q32.32, ONE = 2^32, every operation rounds half away from zero, P0-01 5). X = from_int(16x + 8), Y = from_int(16y + 8). "Land" = `water` ≠ 3; "dry" = `water` ∈ {0, 1}; a **site cell** = `water` = 0 (dry land, not a river). A **border river cell** = `water` = 1 and `river` ≥ `river_border_class`. Neighbour orders of P1-00 2.3. Steps RG1 to RG14.

**Site key.** For a site cell c, key(c) = (`habitability`(c), −S(c), h(c)) with h(c) = `hash_at(K_seed, x, y)` (`uint64`, compared unsigned), compared lexicographically; c is **better** than d if key(c) > key(d), or the keys are equal and index(c) < index(d). So the best site is the most habitable; among equally habitable cells (in mountains all are 0) the lowest; then a hash, so that ties on flats fall at no fixed corner.

### RG1. Preconditions

Section 2.

### RG2. Drainage height (G7)

hand (dm, `int64`) by G7 with **k = 1** (every river, lake and ocean cell is a drain; 0 there), as P1-05 R2.

### RG3. Step weight (D1, G6 row blocks)

For each land cell:

```
nz = fbm_m(K_cost, X, Y, cost_noise)                       // in [−ONE, ONE]
nf = ONE + mul(cost_noise_amp, nz)
hf = ONE + from_ratio(hand, 10 × hand_ref_m)              // 1 + hand in metres / hand_ref_m
wt = mul(nf, hf)                                           // > 0
```

### RG4. Candidates (D1 per tile, then concatenated in tile order)

t = `seed_tile_m` div 16 cells (54). Tiles are the squares [tx·t, tx·t + t − 1] × [ty·t, ty·t + t − 1] clipped to the map, tx, ty = 0, 1, …, taken in order (ty, then tx). Each tile with at least one site cell contributes its best site cell.

### RG5. Seeds (D7 sort, D4 selection)

Sort the candidates best first (the site order above, a strict total order). Walk the sorted list; accept candidate c unless some accepted seed s is **near and on the same bank**:

```
near(s, c)  = (16 Δx)² + (16 Δy)² < seed_spacing_m²                      // int64
cross(s, c) = the segment from s to c meets a border river cell:
    m = max(|Δx|, |Δy|); (px, py) = s
    for j = 1 … m:
        (qx, qy) = (s.x + RHA(Δx · j, m), s.y + RHA(Δy · j, m))
        if (qx, qy) is a border river cell: true
        if qx ≠ px and qy ≠ py and (qx, py) and (px, qy) are both border river cells: true   // a diagonal slip
        (px, py) = (qx, qy)
    false
same bank   = not cross(s, c)
```

with Δx = c.x − s.x, Δy = c.y − s.y (RHA of a negative product rounds half away from zero). Accepted seeds are numbered 1, 2, … in acceptance order. The test runs against accepted seeds in acceptance order and stops at the first that is near and on the same bank. Seeds across a large river do not block each other, so both banks get seeds and the river can become their border.

### RG6. One seed per landmass (D5)

Label the 8-connected components of land with G2 (labels 1 … M in order of smallest index; `landmass`). For each landmass in label order that holds no seed, append a seed at its best site cell, or, if it has none, at its smallest-index cell, numbered after the existing seeds. (A landmass without a site cell does not occur on hydrology output: every island has dry land.)

### RG7. Cost flood (D6)

A multi-source Dijkstra over land cells; costs in decimetres of level walking (`int64`). The cost of a step from c to its neighbour m in direction code d:

```
D160  = mul_int(SQRT2, 160)                                // raw 971840160000 (226.27 dm)
hz    = (d even) ? round_int(mul_int(wt[m], 160)) : round_int(mul(wt[m], D160))
if water[m] = 2:  hz = round_int(mul_int(lake_cost_factor, hz))
rv(p) = (p is a border river cell) ? 10 × river_cost_m : 0
cost  = hz + rv(m)
if d odd and both corner cells (m.x, c.y) and (c.x, m.y) are border river cells:
    cost = cost + 10 × river_cost_m                        // no slipping between two diagonal river cells
```

```
best[·] = INF (2^62); owner[·] = 0; done[·] = false; seq = 0
for each seed s in seed order:  best[s] = 0; owner[s] = s's number; push (0, seq, s); seq += 1
while the queue is not empty:
    pop the entry (k, q, c) with the smallest (k, q)
    if done[c]: continue
    done[c] = true
    for d = 0 … 7: m = neighbour of c in direction d; skip unless m exists, is land and not done[m]
        nk = k + cost(c → m)
        if nk < best[m]:  best[m] = nk; owner[m] = owner[c]; push (nk, seq, m); seq += 1
```

Every land cell is reached (each landmass has a seed). Only a strict improvement changes an owner, so equal costs keep the owner whose entry came first in (key, seq) order. Each region is 8-connected and contains its seed.

Climbing has no term of its own: the height above drainage already makes slopes and ridges expensive, and a height-change term made regions follow contours (11.2).

### RG8. Merge small regions (D4)

min_cells = (`region_min_area_m2` + 255) div 256. For two regions i ≠ j: n(i, j) = the number of border pairs (3.3) between them; nr(i, j) = those that are not river pairs (neither cell a border river cell). Repeat: among regions with fewer than min_cells cells and at least one neighbour (some n > 0), take the one with the smallest (cells, number); merge it into the neighbour j with the largest (nr, n), ties to the smaller number. The absorbing region keeps its number and its seed; cells and both counts are added (pairs between the two vanish). Stop when no such region remains. A region with no neighbour (an island region) is never merged, whatever its size.

### RG9. Renumber (D7)

Regions get ids 1, 2, … in order of their smallest cell index; `region` = id on land, 0 on ocean. More than 65535 regions is fatal (`regions: more than 65535 regions`; impossible with the shipped tuning: at most one seed per tile and one per landmass, and a landmass has at least one cell).

### RG10. Region properties (D8 reductions)

Each column of 3.2 by exact integer reductions over the region's cells (sums, counts, minima, maxima). The 90th percentile is kth over the region's dry `habitability` values at rank min(n − 1, (90 n) div 100) (G1). `ore_reach_cells` uses dw = metres(chamfer(source = `walk_area` > 0)) by P1-02 H7 (5/7 weights, metres(d) = min(32767, (32 d + 5) div 10), as P1-05 R3): a dry cell counts if `ore` ≥ `ore_rich_min` and dw ≤ `ore_reach_m`.

**Dominant biome**: the code with the most cells among the region's cells whose definition has `water` = false, ties to the smaller code. (Every region has a dry cell, and dry cells carry non-water biomes, P1-04 B5.)

### RG11. Names (sequential, id order)

With the words of `names` (record `region_names`) and the biome nouns:

1. **Land centroid**: L = the number of land cells, cx = RHA(Σx, L), cy = RHA(Σy, L) over land cells. r_c = round_int(mul_int(`name_central_frac`, N)).
2. **Sector** of a region from its seed: dx = seed_x − cx, dy = seed_y − cy (y grows southward). If dx² + dy² < r_c²: `central`. Otherwise, with ax = |dx|, ay = |dy|: if 29 ay ≤ 12 ax, `east` (dx > 0) or `west`; else if 29 ax ≤ 12 ay, `south` (dy > 0) or `north`; else `southeast`, `southwest`, `northeast` or `northwest` by the signs of dy and dx. (12/29 = 0.4138 approximates tan 22.5° = 0.4142.)
3. **Noun**: `isle` if the region is the only region of its landmass and that landmass is not the largest (most cells; ties to the smaller label); otherwise the `region_noun` of its dominant biome.
4. **Base name** = sector word, one space, noun. **Name**: going through regions in id order, the k-th region with a given base name is named the base name for k = 1, and the base name, one space and the Roman numeral of k for k ≥ 2 (`East Wood II`). Roman numerals: standard subtractive form (I, II, III, IV, V, …, IX, X, …, XL, L, …, XC, C, …, CD, D, …, CM, M), thousands as repeated M.

Names are unique: base names have exactly two words, numbered names three; two numbered names differ in base or numeral. (A word never contains a space, by the word grammar of 4.)

### RG12. Borders and passes (D8 counts per pair, then sorted; passes sequential)

Every border pair (3.3) adds to its row: `length_m` += 16; `low_dm`, `low_x`, `low_y` by the first pair attaining the minimum; `walk_pairs`, `river_pairs`, `river_class_max` as defined. Rows sorted by (a, b).

**Passes.** For each row of table `pass` in `id` order, with path cells c_0 … c_L: build the step sequence by inserting, between each diagonal step c_j → c_{j+1}, the corner (c_{j+1}.x, c_j.y) if it is land, else (c_j.x, c_{j+1}.y) if it is land, else nothing; equal consecutive cells are skipped. For every consecutive pair (u, v) of the sequence that are 4-neighbours, both land, with region(u) ≠ region(v): the pass **crosses** the border row (min, max) of the two regions (the row exists, since (u, v) is a border pair). Each row records, over the passes that cross it: `passes` = how many (a pass counts once per row however often it crosses), `pass_id` = the smallest id. A pass that ends in the region it started from still crosses every border it passed on the way.

### RG13. Biome counts (D8)

For each region and code: the number of its cells with that `biome` value; rows with a non-zero count, sorted by (region, biome).

### RG14. Write

Layer `region`; rows of `region` in id order; `region_border` in (a, b) order; `region_biome` in (region, biome) order; one transaction (P0-02).

### 5.1 Worked examples

Computed by the spec writer's exact-integer prototype (`~/.cache/the-unmapped/out/p1-06-spec/tools/`, the flood in C with `__int128` products, the rest in Python with `core_reference`). Flood examples: 9 × 5 test grids of dry land (`water` 0), hand 0 and `cost_noise_amp` 0 (so wt = ONE) unless stated, shipped `hand_ref_m` 25, `lake_cost_factor` 2, `river_border_class` 6, `river_cost_m` 2000 (rv = 20000). An orthogonal step costs 160, a diagonal step 226 (round of 226.27). Cost rows are listed top (y = 0) to bottom.

**RG3 weight.** hand 600 dm, nz = 0: hf = ONE + from_ratio(600, 250) = 3.4 (raw 14602888806), wt the same; hand 300 dm: raw 9448928051 (2.2). An orthogonal step onto the first costs round_int(3.4 × 160) = 544, a diagonal step round(3.4 × 226.27) = 769.

**RG7, flat**, seeds A (1, 2) and B (8, 2). Owners: columns 0 to 4 → A, 5 to 8 → B (every row). Row 2 costs `160 0 160 320 480 480 320 160 0`; (4, 2): A 480 against B 640.

**RG7, ridge**: as the flat case with hand 600 dm in column 3. Owners: columns 0 to 3 → A, 4 to 8 → B; the border moves from between columns 4 and 5 onto the ridge's east foot. Costs, rows 0 to 2: `386 320 386 930 772 612 452 386 320`, `226 160 226 770 706 546 386 226 160`, `160 0 160 704 640 480 320 160 0` (rows 3, 4 mirror rows 1, 0). A pays 544 to enter the ridge cell (3, 2) and would reach (4, 2) at 864, B at 640.

**RG7, large river**: seeds A (1, 2), B (8, 2), column 3 river cells (`water` 1) of class 6. Owners: columns 0 to 3 → A, 4 to 8 → B; row 2 costs `160 0 160 20320 640 480 320 160 0`. Each river cell goes to the side that reaches it cheaper (A: 160 + 160 + 20000), and nobody crosses: (4, 2) would cost A 20480. With class 5 (below `river_border_class`) the owners and costs are those of the flat case.

**RG7, diagonal river**: river cells of class 7 at (2, 0), (3, 1), (4, 2), (5, 3), (6, 4); seeds A (0, 4), B (8, 0). Owner rows (y = 0 … 4): `1 1 1 2 2 2 2 2 2`, `1 1 1 1 2 2 2 2 2`, `1 1 1 1 1 2 2 2 2`, `1 1 1 1 1 2 2 2 2`, `1 1 1 1 1 1 2 2 2`. Costs row 0: `640 706 20772 800 640 480 320 160 0`; row 4: `0 160 320 480 640 800 20772 706 640`. Without the corner rule A would step diagonally from (3, 0)'s neighbour (2, 1) to (3, 0) between (2, 0) and (3, 1) for 226; with it the step costs 20226, so (3, 0) is B's (800).

**RG7, lake**: lake cells (`water` 2) at columns 3 to 5, rows 1 to 3; seeds A (1, 2), B (8, 2). Owners as the flat case; row 2 costs `160 0 160 480 800 640 320 160 0` (a step onto the lake costs 320, a diagonal one 452).

**RG5 seeds** (`seed_spacing_m` 1200: near ⟺ Δx² + Δy² < 5625 cells²). A border river runs down column x = 168 for every y. Candidates (habitability, cell): c1 (100, 100) and c2 (160, 100) at 200 with equal S and h(c2) > h(c1); c3 (150, 175) at 180; c4 (40, 40) at 150; c5 (175, 120) at 90. Order c2, c1, c3, c4, c5. Accept c2; reject c1 (near c2, 3600, no river between); accept c3 (from c2 10² + 75² = 5725, not near); accept c4 (from c2 18000, from c3 30325); c5 is near c2 (625) and near c3 (3650), but both segments cross column 168 (c2 → c5: Δ = (15, 20), m = 20, the points reach x = 168 first at j = 10, (168, 110), RHA(150, 20) = 8): accept. Seeds 1 = c2, 2 = c3, 3 = c4, 4 = c5. Without the river c5 would be rejected.

**RG8 merge** (min_cells 100): regions 1 (500 cells), 2 (120), 3 (90), 4 (60, an island). n(1, 2) = 30 (nr 30), n(1, 3) = 12 of which 10 are river pairs (nr 2), n(2, 3) = 8 (nr 8). Region 3 is the smallest small region with a neighbour; it merges into 2 (nr 8 > 2), although its longest border is with 1. Region 2 now has 210 cells and n(1, 2) = 42, nr 32. No region with a neighbour is below 100; region 4 stays. Result: 1 (500), 2 (210), 4 (60).

**RG11 names** at N = 1024, land centroid (500, 480), `name_central_frac` "0.12" (raw 515396076): r_c = round_int(122.88) = 123. Seeds and dominant biomes, regions in id order: (520, 470) grassland → d² = 500 < 15129: `Central Downs`; (800, 500) forest → dx 300, dy 20, 29 × 20 ≤ 12 × 300: `East Wood`; (200, 200) alpine → dx −300, dy −280: `Northwest Peaks`; (510, 900) marsh → 29 × 10 ≤ 12 × 420: `South Marsh`; (790, 560) forest → `East Wood` again, the second: `East Wood II`; a lone region on a small island north of the centre: `North Isle`. Sector checks (dx, dy): (10, 4) east; (10, 5) southeast; (−3, −7) northwest; (0, −9) north; (4, 10) south; (5, 12) southeast. Roman: 2 II, 4 IV, 9 IX, 14 XIV, 40 XL, 90 XC, 400 CD, 1994 MCMXCIV, 4000 MMMM.

**RG12 borders and passes.** A 4 × 3 test world, all land, region 1 on columns 0–1 and region 2 on columns 2–3; S rows `10 20 30 10`, `10 50 40 10`, `10 25 25 10`; `walk_area` 1 everywhere except 0 at (1, 1) and (2, 1); (2, 0) a river cell of class 6. Border pairs (1, y)–(2, y) for y = 0, 1, 2 with max S 30, 50, 25. Row (1, 2): `length_m` 48, `low_dm` 25, `low_x`, `low_y` = (1, 2) (equal S: the first cell), `walk_pairs` 2, `river_pairs` 1, `river_class_max` 6. A pass with path (0, 1), (1, 0), (2, 1), (3, 1): the step (0, 1) → (1, 0) gets the corner (1, 1) (inside region 1); the step (1, 0) → (2, 1) gets the corner (2, 0), and (1, 0)–(2, 0) crosses (1, 2). With this pass as id 3 and a second crossing pass of id 5: `pass_id` 3, `passes` 2.

## 6. Invariants

1. `region` > 0 exactly on land cells (`water` ≠ 3); 0 on ocean.
2. Region ids are 1 to R, each used, increasing with each region's smallest cell index; R = the number of `region` rows.
3. Each region is 8-connected, lies on one landmass and contains its seed (`seed_x`, `seed_y`), a site cell; `landmass` is that landmass's G2 label.
4. Every region has at least min_cells cells, unless it has no 4-adjacent neighbour region.
5. Every column of the three tables equals its definition recomputed from the layers, the definitions and table `pass` (bounds, counts, means, percentile, dominant biome, walk area, the border rows and exactly the pairs that share a 4-adjacent cell pair, with `a` < `b`; the biome rows sum to `cells`).
6. Names are unique; each is a sector word and a noun, optionally followed by a Roman numeral ≥ II, all words from the data.
7. Pure function of the seed, N, the inputs, the biome definitions, table `pass` and the tuning; independent of the thread count.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| RG2 | D5 (G7) | |
| RG3 | D1, G6 row blocks | location-keyed noise |
| RG4 | D1 per tile; argmax by the site order (unique) | candidates concatenated in tile order |
| RG5 | D7, D4 | strict total order; sequential acceptance; integer segment test |
| RG6 | D5 (G2) | |
| RG7 | D6 | pop by (cost, seq); strict improvement; 8-neighbour order |
| RG8 | D4 | stated selection order and tie-breaks |
| RG9 | D7 | by smallest index |
| RG10, RG12, RG13 | D8 | integer sums, counts, min/max; ties stated; rows sorted; the chamfer is D4 (H7's two passes) |
| RG11, RG12 passes | D4 | id order |

No floating point, no new math function, one noise key and one `hash_at` key, no `Rng`.

Overflow (N ≤ 2048, values within the bounds of 4): nf ∈ [0.1, 1.9] ONE; hand ≤ 30000 dm on hydrology output, so hf ≤ 1 + 30000 / 10 = 3001 ONE with `hand_ref_m` 1 (the seam refuses a hand above 131070, the largest |ΔS| of P1-05 7, giving hf ≤ 13108 ONE); wt < 2^15 ONE (raw < 2^47); hz ≤ round(2^15 × 226.3) × 10 < 2^27 (lake factor ≤ 10); rv and the corner term ≤ 10^6 each; a step < 2^28 and a path of at most 2^22 steps < 2^50 (`int64`; INF = 2^62). Spacing: (16 Δx)² + (16 Δy)² ≤ 2 × 2^30. Segment: |Δx · j| ≤ 2^22. Sums over a region: `elevation` ≤ 2^22 × 32767 < 2^37, scores ≤ 2^22 × 255 < 2^30, Σx < 2^33. Centroid and sector: dx² + dy² < 2^23; 29 × 2048 < 2^16.

## 8. Performance

Budget (P1-00 11): **0.3 / 1.5 / 6 s** at 512 / 1024 / 2048. Work: G7 (linear), 3 noise octaves per land cell (parallel), one chamfer (linear), one Dijkstra over the land cells with up to 8 relaxations each (a binary heap; about 2^22 pops at 2048), the rest linear or over at most a few hundred regions. The seed test is at most (number of candidates)² pairs × a segment of at most 75 cells for near pairs only (1444 candidates at 2048). The prototype's flood (C, binary heap) takes 0.02 / 0.08 / 0.5 s single-threaded on 512 / 1024 / 2048 seed 0, so the budget is comfortable. Memory: about 40 bytes per cell of work grids (hand, wt, best, owner, chamfer), 170 MB at 2048.

## 9. Report and render

**`report`**, appended to `[geography]` after the resources keys (P1-00 10.3) when the `region` layer exists:

| Key | Value |
| --- | --- |
| `regions` | the number of `region` rows |
| `region_area_km2` | percentiles 0, 50, 100 of `cells` over the rows (rank rule of P1-00 10.3), each as km² with two decimals: v = RHA(cells × 256, 10^4), printed v div 100, `.`, v mod 100 (two digits); `n/a n/a n/a` with no rows |
| `region_borders` | the number of `region_border` rows |
| `region_borders_walkable` | rows with `walk_pairs` > 0 |
| `region_borders_river` | rows with `river_pairs` > 0 |
| `region_borders_pass` | rows with `pass_id` > 0 |
| `regions_isolated` | regions that appear in no `region_border` row |
| `regions_upland` | regions with 4 × `walk_cells` < `cells` |

Corrupt data, each exit 3 with nothing on stdout (the forms of P1-04 9): a `region` layer that is not `u16` or not the size of `elevation` (`corrupt layer 'region': type <t>, expected u16` or `… size <W>x<H>, expected <N>x<N>`), whenever the layer exists; a `region_border` row with `a` ≥ `b` or with `a` or `b` not an id of table `region` (`corrupt table 'region_border': row (<a>, <b>): <reason>`, reasons `a not below b`, `unknown region <id>`, first failing row in key order); a `region` row with `cells` < 1 (`corrupt table 'region': row <id>: cells below 1`). `report` does not recompute the tables from the layer.

Example (512 seed 0, prototype): `regions = 14`, `region_area_km2 = 0.06 1.69 3.50`, `region_borders = 18`, `region_borders_walkable = 9`, `region_borders_river = 0`, `region_borders_pass = 0`, `regions_isolated = 3`, `regions_upland = 6`. (1024 seed 0: `regions = 56`, `region_area_km2 = 0.06 1.75 4.53`, `region_borders = 113`, `region_borders_walkable = 85`, `region_borders_river = 10`, `region_borders_pass = 4`, `regions_isolated = 6`, `regions_upland = 12`.)

**Statistics** (P1-00 13.1). Measured by the prototype of this spec on the 0.10.0 worlds of section 11 (512 seeds 0–19, 1024 seeds 0–29, 2048 seeds 0–5). Typical = measured min–max widened by about 10%, rounded outward; hard only where a value outside means a defect. The 2048 ranges rest on six worlds and should be widened by the validation sweep (2048 seeds 0–19); the prototype's flood is exact, its noise and hand are those of the spec (they come from the P1-05 prototype's checked `fbm_m` and G7), so the validation sweep should reproduce these numbers closely.

| Statistic | All seeds (hard) | Typical (≥ 90%), 512 / 1024 / 2048 | Measured min / median / max, 512 / 1024 / 2048 |
| --- | --- | --- | --- |
| `regions` | ≥ 1 | 10–18 / 40–62 / 175–225 | 11 / 13 / 16; 45 / 49 / 56; 195 / 200 / 204 |
| `region_area_km2` p50 | 0.5–5 | 1.2–2.6 / 1.5–2.7 / 1.8–2.3 | 1.34 / 1.89 / 2.36; 1.71 / 2.10 / 2.43; 1.97 / 2.04 / 2.12 |
| `region_area_km2` p100 | ≤ 15 | ≤ 6.2 / ≤ 6.7 / ≤ 7.0 | 3.10 / 4.53 / 5.64; 3.37 / 4.78 / 6.05; 4.49 / 5.25 / 6.37 |
| `region_area_km2` p0 | descriptive (islands) | — | 0.05–1.06; 0.06–1.03; 0.06–0.81 |
| `region_borders` | descriptive | — | 18 / 24.5 / 30; 105 / 116 / 134; 510 / 522 / 545 |
| `region_borders_walkable` | descriptive | — | 9 / 17 / 23; 73 / 91 / 114; 400 / 476 / 503 |
| `region_borders_river` | descriptive | — / ≥ 7 / ≥ 85 | 0 / 1 / 6; 8 / 15 / 26; 98 / 122 / 136 |
| `region_borders_pass` | descriptive | — | 0 / 1 / 3; 0 / 2 / 7; 0 / 4.5 / 6 |
| `regions_isolated` | descriptive (islands) | — | 0 / 0 / 4; 0 / 1 / 6; 0 / 1 / 4 |
| `regions_upland` | descriptive | — | 0 / 2 / 6; 1 / 6 / 14; 5 / 10.5 / 29 |

The design's "roughly 12, 45 and 180" is met in area (median region 1.9–2.1 km², mean land per region 2.0–2.2 km²); the counts run a little above it (49 and 200) because land covers about 40% of a 1024 map (107 km²) and 2048 maps have 431 km² of land. A seed spacing of 1250 m would bring the counts to the design's figures at about 2.3 km² per region (12, question 3).

**Palette**: `render --layer region` uses `ids` (P1-00 10.2) under `auto`, as `lake` and `walk_area`: 0 → (0, 0, 0); v ≥ 1 → h = mix64(v), channel j = 64 + floor(((h >> 8j) & 0xFF) × 191 / 255).

**Composite `regions`**: exact rule, validation and messages in P0-04 amendment 18 (e): the `biomes` composite, then (0, 0, 0) on every cell with `region` v > 0 whose E or S neighbour has a `region` value w > 0, w ≠ v.

**Viewer (owner: viewer engineer), required for this stage's merge:**

1. List the layer `region` (it appears in `report`) and draw it with `ids`, pixel for pixel as `render --layer region`; the headless check compares every layer the report lists, so it fails without this.
2. Offer the composite `regions` beside `map` and `biomes`, drawn exactly as P0-04 amendment 18 (e), and extend `gdext-headless-check` to compare it with `render --composite regions` on the real world (`--seed 42 --map-size 512`), as it does for `biomes`. On a world without a `region` layer the viewer does not offer it.
3. Read the schema 5 world (the reader refuses version 4 files; regenerate test worlds).

Region names, a click-to-inspect panel with the table columns, and border or pass overlays are later viewer features, not required here.

## 10. Tests required

Library seams (interim; the engineer records the final signatures here after implementation, as P1-05 10): `region_step_weight(nz, hand, tuning) → Fx`, `region_candidates(grid, water, habitability, S, seed, tuning)`, `region_seeds(candidates, grid, water, river, tuning) → seeds` (RG5), `region_crosses(grid, border_river, s, c) → bool`, `region_landmass_seeds(...)` (RG6), `region_flood(grid, wt, water, river, seeds, tuning) → (owner, cost)`, `region_merge(grid, owner, water, river, count, min_cells, tuning)`, `region_sector(dx, dy, r_c) → index`, `roman(k)`, `region_names(...)`, `region_borders(...)`, `region_pass_crossings(...)`, `regions_stage(inputs, definitions, passes, tuning, seed, threads)`, `run_regions(context)`. Each raises `FatalError` on a contract violation.

### 10.1 Unit tests

1. Every worked example of 5.1 exactly, through the seams (owners and costs of each flood case; seeds and the bank exception; merge; sectors, numerals and names; border rows and pass crossings).
2. RG6: a fixture island with no candidate gets a seed at its best site cell, numbered after the greedy seeds; a landmass of only river and lake cells (hand-made) takes its smallest-index cell.
3. RG9 renumbering on a fixture where the seed numbering differs from the smallest-index order.
4. The tuning: each field one step outside its bounds is a P0-03 range error; `seed_tile_m` above twice `seed_spacing_m` fails `regions_tile_fits_spacing` at the definition's header; a `names` word `east` or `North West` fails the word grammar; a missing `names.isle` is a missing-field error; a biome `region_noun` of `Wet Wood` is fatal at the stage (section 2, check 4).
5. Preconditions of section 2, each with its message: a missing `walk_area`; a `biome` value with no definition; a pass path of 6 bytes; a pass path with a jump of two cells.
6. Schema: `SCHEMA_VERSION` is 5; an empty world (`--until none`) has the three empty tables and 11 pages; a version 4 file is refused with `world schema version 4, this build reads 5`; table round trip (rows read back equal the computed ones; no REAL value, `typeof` check).
7. Report: the eight keys on a hand-made 8 × 8 world, recomputed by the test; `region_area_km2` with no rows prints `n/a n/a n/a`; each corrupt case exits 3 with its message; `[tables]` lists the three tables.
8. Render: `--layer region` uses `ids`; `--composite regions` on the hand-made world (borders black, the rest equal to `--composite biomes`); a missing `region` exits 3; `render --help` lists `regions`.

### 10.2 Property tests

Seeds 0 to 63 at N = 128 with the fixture data (all earlier stages; the shipped spacing gives a handful of regions at 128, so also a test-only `regions.toml` with `seed_tile_m` 256, `seed_spacing_m` 400, `region_min_area_m2` 65536): invariants 1 to 6 by independent code, including its own Dijkstra (any queue structure reproduces the owners exactly, since the pop order is (cost, seq)), its own merge and its own border and pass tables.

### 10.3 Determinism tests

Threads 1, 2, 5: identical layer and tables. Resume: a world written after `resources` (`--until resources`), read back, and the stage run on the decoded layers equals the uninterrupted pipeline (P1-00 X2). P1-00 X1: the default run writes ids 1 to 19 with `stages` = `elevation,hydrology,climate,biomes,resources,regions`; `--until regions` the same; `--until resources` leaves the three tables empty.

### 10.4 Golden values

`worldgen/tests/golden/p1-06-regions.txt`, written by the test writer's independent reference in `worldgen/tests/reference/` for the cases of P1-05 10.4 (the N = 128 fixture cases and 512 seed 0, 512 seed 42, 512 "The Unmapped", 1024 seed 1 with the shipped data): per case the layer hash of `region`, the logical hashes of the three tables, and the SHA-256 of hand (as P1-05), wt (raw `int64`, every land cell, 0 elsewhere), the flood owners before merging (`int32` per cell, 0 on ocean) and the seed list (`u32` cell indices in seed order), each little-endian in index order with no header. The large cases are read by the `*-slow` suites.

**Version and golden re-recording.** `GENERATOR_VERSION` becomes **0.11.0**. Every geography world changes (a new layer and rows), every world changes its schema version and page count, and every world changes its `content_hash` (new `worldgen.regions`; P1-00 6.4): `p0-worlds.txt` and `p1-worlds.txt` are re-recorded by the validation specialist after the cross-preset comparison. The golden vectors of P1-01 to P1-05 hold layer and table hashes, which this change must leave **unchanged**; a difference there is a defect. `tools/ci/world-hashes.sh` needs no new case; its timings in `CLAUDE.md` grow by this stage's share.

## 11. Calibration and known limitations

### 11.1 How it was calibrated

A prototype of this spec (`~/.cache/the-unmapped/out/p1-06-spec/tools/`: `regions.py` seeds, merge and renumbering; `flood.c` the exact integer flood, G2 labelling and the H7 chamfer; `table.py` the table rows and report keys; noise and `hash_at` from the P1-05 prototype, checked there against `core_reference`) ran on the stored 0.10.0 layers of the P1-05 validation sweep (`~/.cache/the-unmapped/out/p1-05-validation/`; 512 seed 0 regenerated on `a18d21c` has the same file hash). Per-world results: `~/.cache/the-unmapped/out/p1-06-spec/sweep/<variant>-<size>.jsonl`. Contact sheets (biome composite, region borders black, seeds red with their names, rivers of class ≥ 6 drawn strong blue), in `~/.cache/the-unmapped/out/p1-06-spec/sheets/`:

- `p1-06-1024-featured.png`: 1024 "The Unmapped", seeds 133, 75, 185, 0 and 1, full resolution with names;
- `p1-06-512-all.png` (seeds 0–19, with names), `p1-06-1024-all.png` (seeds 0–29), `p1-06-2048-all.png` (seeds 0–5) and `p1-06-2048-0.png` (full resolution with names);
- `p1-06-draft-vs-final-1024-0.png`: the central range of 1024 seed 0 on the shaded relief, first draft (left) against this version (right), borders red, seeds yellow.

Measures, besides the report keys: **crest share**, of the border pairs where the relief across the border is at least 40 m within 12 cells on either side, the share whose pair is higher than both cells 12 cells (192 m) out on either side, across the border (**trough share**: lower than both); **large-river share**, the share of river cells of class ≥ 6 (≥ 7) that are on or next to a border; the isoperimetric quotient of regions above 0.5 km² (`iq`); `chained`, the share of seeds whose nearest river cell lies downstream of another seed's within 4 km (critic: "do not let region seeds line up along one creek").

### 11.2 Variants (medians over worlds; 512 seeds 0–19, 1024 seeds 0–29, 2048 seeds 0–5)

| Variant | Size | `regions` | area p50 / p100 km² | crest / trough | large-river share (≥ 6 / ≥ 7) | iq |
| --- | --- | --- | --- | --- | --- | --- |
| plain distance (no weight, no river cost, no noise; same seed rule) | 512 / 1024 / 2048 | 14 / 51 / 210 | 1.87 / 3.56; 2.04 / 3.85; 2.04 / 4.34 | 0.17 / 0.08; 0.14 / 0.05; 0.12 / 0.06 | 0.00; 0.07 / 0.06; 0.08 / 0.08 | 0.41 / 0.46 / 0.47 |
| first draft (climb 10 both ways, class 4 at 60 m, hash seeds) | 512 / 1024 / 2048 | 13 / 46 / 171 | 1.79 / 4.30; 2.31 / 4.80; 2.63 / 4.98 | 0.16 / 0.04; 0.10 / 0.04; 0.08 / 0.04 | 0.09; 0.07 / 0.07; 0.09 / 0.10 | 0.31 / 0.36 / 0.44 |
| climb uphill only (descent free), class 6 at 1000 m; 4 worlds per size | 512 / 1024 | 12 / 45 | — | 0.15 / 0.05; 0.08 / 0.04 | —; 0.17–0.57 | 0.37 |
| **this spec** | 512 / 1024 / 2048 | 13 / 49 / 200 | 1.89 / 4.53; 2.10 / 4.78; 2.04 / 5.25 | **0.39 / 0.03; 0.28 / 0.02; 0.23 / 0.04** | **0.36; 0.70 / 0.80; 0.83 / 0.92** | 0.33 / 0.37 / 0.39 |

Single-factor variants of this spec (1024 medians; crest, large-river share ≥ 6, area p100): `hand_ref_m` 10: 0.30, 0.26, 5.3; 50: 0.26, 0.31, 4.9 (river cost 1000 in all three); cost noise 0 or 0.3: no measurable change (0.28, 0.30); `river_border_class` 5: 0.33 large-river share at class ≥ 6 but many stream borders; 7: 0.14; river cost 500 / 1000 / 2000 / 5000 without the bank rule: 0.16 / 0.29 / 0.48 / 0.57, and the largest region grows to 8.2 km² at 5000; with the bank rule 1000 / 2000 / 5000: 0.52 / 0.65 / 0.66 (before the merge preference of RG8; 0.70 after); ranking candidates by the mean habitability of their tile instead of the best cell: no change in `chained` (0.06) or shape; the hash instead of the lowest surface as second key: no measurable difference (crest 0.28 either way at 1024); the lowest surface is kept because an upland region's natural centre, and its first hamlet, is in its valley, not on a summit.

Findings:

1. **Symmetric climbing cost makes contour borders.** With 10 m of walking per metre climbed or descended, a region claims the altitude band of its seed: borders ran along mountain feet and valley floors (draft-vs-final sheet, left), and crest share fell below that of plain distance. Charging only the climb helped little. Weighting each step by the height above drainage does what the design asks: valley floors are fast, ridges slow, fronts meet on crests; crest share 0.23–0.39 against 0.12–0.17 for plain distance, trough share halved.
2. **Large rivers need the bank rule.** Seeds sit by rivers (a median 74–87% of seeds within 48 m of one): the best sites are there. A seed on one bank claimed the other bank unless the crossing cost was very high, and a very high cost alone stretched far-bank regions along the river. Letting seeds on opposite banks both stand, with a 2 km crossing cost, puts 70% (1024) and 83% (2048) of class-6 river cells on a border, while area and shape stay as before.
3. **Seeds on one creek** (critic, section 6): `chained` is 0.03 / 0.16 / 0.38 at 512 / 1024 / 2048, and most chained pairs are the intended pairs on opposite banks of a large river (0.06 at 1024 without the bank rule). The greedy spacing bounds every gap: a candidate farther than 1200 m from every seed is always accepted, so a plain beside a creek cannot be starved. Ranking by tile mean instead of best cell changed nothing; the simpler rule is kept.
4. **Upland.** Regions with less than a quarter of walkable ground: 0–6 at 512 (seeds 0 and 2: 6 of 14 and 6 of 13), 1–14 at 1024, 5–29 at 2048. They are the massifs wider than the spacing (Central Peaks of the phrase world, the plateau of seed 185, the ring of seed 75).
5. **Two lowlands** (validation 3.3: 1024 seed 133, walk areas of 0.31 and 0.24 of land): its 48 regions split along the range between them; the range itself has 9 upland regions; nothing assumes one dominant lowland (`walk_area` is per region).
6. **Contrast for history.** Per-region `habitability_mean` (1024, median world): 10th / 50th / 90th percentile 10 / 71 / 113 over all regions, 30 / — / 116 over lowland regions; the regional contrast the critic missed at cell scale (P1-05 F2) is present between regions. Regions with rich ore: median 20 of 49 at 1024, 17 of them with reachable ore (`ore_reach_cells` > 0); at 512, 7 and 6.5 of 13.

### 11.3 Upland: regions of their own or attached to neighbours (the lead's question)

Decided: **both, by geometry, with no special rule.** Upland tiles still contribute candidates (habitability 0, ranked last, placed at the tile's lowest point); a candidate farther than the spacing from every accepted seed becomes a seed. So a massif wider than about 1.2 km gets upland regions of its own, seeded in its high valleys and bounded by its crests; a narrower range is divided along its crest between the regions on either side. Reasons: history needs the uplands as places (dwarf homelands that "build in places the others avoid", lairs and mountain beasts per region, passes between two upland regions); attached to lowland regions, a steep 512 world's crags would make regions of 5–10 km² whose means describe neither part; and a range split along its crest is what "bordered on ridges" means.

### 11.4 Limitations

1. **Lowland borders are not ridges.** Where relief is a few metres, borders are distance borders broken up by the cost noise; regions there are compact, not drainage basins. A region holds several small streams (stream spacing is 300–500 m, regions 1.5 km across), so most border length crosses minor valleys.
2. **Regions are valleys, not basins.** A long valley is split into several regions along its length; whole basins are sets of regions.
3. **Large rivers are borders where seeds lie on both banks.** About 30% of class-6 river cells at 1024 run inside a region: mostly the upper reaches of class-6 rivers, where both banks are one valley, and stretches where one bank has no seed within reach.
4. **Islands are regions of any size**: 0.05 km² islets are regions (`Isle`), never merged (`regions_isolated`).
5. **Names repeat.** Forest worlds have up to `Northwest Wood VII`; the names are placeholders (question 1).
6. **No sea regions** (question 2).
7. **2048 statistics rest on six worlds.**

## 12. Open questions and decisions of taste (for the lead and Paul)

1. **Placeholder names** "<sector> <biome noun> [numeral]", words from data (Paul's call). Recommendation: keep; history adds cultural names per civilization in its own table, since a name depends on who lives there and when. Alternative if the repetition bothers: a second noun from the second most common biome or from the coast (`East Wood Shore`), a small change to RG11.
2. **No sea regions; lakes belong to land regions** (design). Recommendation: approve for P1; sea areas for naval events are history's.
3. **Region size and count**: seeds 1200 m apart, merge below 0.75 km²; median 2.0–2.1 km², 13 / 49 / 200 regions. Recommendation: approve (the design's "about 2 km²"); `seed_spacing_m` 1250 would give 12 / 45 / 185 at about 2.3 km².
4. **Borders by height above drainage** instead of climbing cost (`hand_ref_m` 25 m). Recommendation: approve (11.2 finding 1).
5. **Large river = class ≥ 6** (≥ 4 km² upstream), 2 km crossing cost, and seeds on opposite banks both stand. Recommendation: approve; class 5 makes many streams borders, class 7 leaves most large rivers inside regions.
6. **Upland regions of their own where a massif is wider than the spacing** (11.3). Recommendation: approve.
7. **The new table `region_biome`** (not in the design's schema plan). Recommendation: approve; it is small (a few rows per region) and it is what beast placement and race preference need.
8. **The `region` summary columns** (3.2): means of all six scores, `habitability_p90`, `ore_rich_cells` and `ore_reach_cells` (300 m), walk and coast counts. Recommendation: approve; anything finer stays in the layers.
9. **Passes linked to borders** through `pass_id` (smallest) and `passes` on `region_border`, not a link table. Recommendation: approve: a world has 0–3 passes and a border rarely has two.
10. **Islets of any size are regions** (11.4 item 4). Recommendation: approve; the alternative (no region) breaks "every land cell is in a region".

## Appendix A. Schema and catalogue impact (for the lead's proposals entry)

**Schema version 5** (generator 0.11.0): tables `region`, `region_border`, `region_biome` (DDL in 3.1), executed after `climate`; the empty world grows from 8 to 11 pages. `docs/specs/schema.md` is updated on this branch (DDL, `meta`, catalogue, changelog). Needed with the merge: P0-02 amendment 17 (DDL, version 5, 11 pages, the refusal message for version 4) and P0-04 amendment (the three tables in `[tables]` and `hash --detail`, the `[geography]` region keys, the composite `regions`). Layer catalogue: `region` unchanged (`u16`, 0 on ocean only, id 19). Data: new `worldgen` id `regions` (`game/data/worldgen/regions.toml`); no biome file changes. Streams `geo.regions.seed`, `geo.regions.cost` as P1-00 3.

**Proposals entry for the lead** (update of "P1 schema changes", P1-00 B8):

- Design doc section: 4 ("Regions"), 6 (table list, schema versions)
- Proposal: schema version 5, with the regions stage, adds `region` (id, name, dominant biome, bounds as four integer columns, seed cell, landmass, cell counts of lake, river, coast and walkable ground, largest river class, main walk area, mean and highest elevation, the means of the six resource scores, the 90th percentile of habitability, and counts of rich ore and of rich ore within 300 m of walkable ground), `region_border` (one row per pair of neighbouring regions: border length, the height and cell of the lowest crossing, walkable and large-river pair counts, largest river class, and the smallest id and number of natural passes crossing it) and `region_biome` (cells of each biome in each region, new: beasts and race preferences need biome shares, not only the dominant biome).

**Further proposals entries** (text for the lead; the design is not wrong, but these settle what it leaves open):

1. *Regions: borders by height above drainage; large rivers are rivers of more than 4 km².* Design section 4 says regions are "bordered on ridges and large rivers". Proposal: state that region borders come from a travel cost that is cheap along valleys and dear over ridges (cost weighted by height above the nearest stream), and that a "large river" drains more than about 4 km² (river class 6), whose two banks belong to different regions where both have a settlement site.
2. *Upland regions.* Design section 4 (Regions). Proposal: add "Mountain massifs wider than about a kilometre form upland regions of their own; narrower ranges are divided along their crest between the regions on either side."
3. *Region counts.* Design section 4 says "roughly 12, 45 and 180 per map". Measured with 2 km² regions on 0.10.0 worlds: 13, 49 and 200 (land is about 40% of the map at 1024 and 42% at 2048). Proposal: "about 2 km² each, roughly 13, 50 and 200 per map", or keep the counts and accept 2.3 km² regions (seed spacing 1250 m); Paul's choice (12, question 3).
