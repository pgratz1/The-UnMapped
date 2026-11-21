# P1-06 Regions

Status: **second version, approved by the lead 2026-11-18; revision r2 (2026-11-18, implemented in 0.11.0); revision r3, spec writer, 2026-11-19; Paul answered its questions the same day; r4, 2026-11-20, ships r3 with P3-08 (generator 0.17.0; `docs/specs/p3-08-followups.md`): two forbidden substrings, the stream's place in the registry, section 14.8; r5, 2026-11-20, corrections after the implementation, 14.8 items 1, 3, 4; r6, 2026-11-20, P3-10 (generator 0.19.0): church words out, grim words fewer and on barren land, three small list edits, rule V7 (one use of a word stem per world), the plain-name question to Paul, section 15; r7, 2026-11-20: Paul's decision, one plain name per land noun, 15.7** (region names with identity and no numerals, Paul's decision of 2026-11-19 in `docs/design/proposals.md`: RG11, section 4.1 (new data kind `placename`), the stream `geo.regions.name`, invariant 6, tests; no schema change, a generator version bump; section 14 lists the revision; Paul answered its five questions the same day, 14.5, and the one that changes behaviour, a leading `The` lower-cased mid-sentence, is specified for P3-04 and P3-07 in 14.7 and Appendix B). r2 was written after the world critic's review of the implementation (`~/.cache/the-unmapped/out/p1-06-critic/critic-review.md` F1 and F2: a pass is attached to one border only, and two noun rules; layer and schema unchanged, generator still 0.11.0; section 13 lists the revision). The second version replaces the first draft of 2026-11-16 (never implemented), which was written before the biome variety revision (generator 0.9.0) and the resources stage (P1-05 r2, generator 0.10.0); section 0 lists every change. Generator **0.11.0**; **schema version 5** (three new tables, section 3). Calibrated on real 0.10.0 worlds (512 seeds 0–19, 1024 seeds 0–29 and 75, 133, 185 and the phrase "The Unmapped", 2048 seeds 0–5; the P1-05 validation worlds, byte-identical to `main` at `a18d21c`) with a prototype of this spec (section 11). Phase P1 Geography, stage 6 (`regions`), the last geography stage. Design doc section 4 (stage 6; "Regions": about 2 km² each, roughly 12, 45 and 180 per map, bordered on ridges and large rivers, placeholder names such as "East Wood II", land only with lakes in land regions, border length, lowest crossing and whether walkable or a river stored for each pair of neighbours), 5 (civilizations have a home region and claim regions; beast populations are counts per region; passes are places to fortify and fight over; region borders tell history where a crossing needs a road or a bridge; events carry a `region_id`), 6 (table `region` with `id`, `name`, `biome`, `bounds` and summary statistics; schema version 5 planned with `region` and `region_border`). Depends on P0-01 to P0-04, P1-00 (G1, G2, G6, G7, D1 to D10), P1-01 (table `pass`), P1-02 (H7), P1-04 (the `biome` layer and definitions) and P1-05 (the score layers and `walk_area`). Reviews that shaped it: `docs/reviews/p1-05-resources-critic.md` section 6, `docs/reviews/p1-05-resources-validation.md` 3.3.

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
| **r2**: passes on borders | every border a pass path crosses | one border per pass, the crossing nearest the col (RG12) | critic F1: approach borders were flagged |
| **r2**: nouns | isle, else dominant biome | isle, then `Mere` (≥ half lake), then `Fells` for upland regions not dominated by alpine, heath or moor, else dominant biome | critic F2: crags named Downs, lakes named Wood |
| **r3**: names | base name (sector + noun), the k-th repeat numbered (`East Wood III`) | the largest region of each base name keeps it (`East Wood`); every other region draws a name of its land from the hand-written lists of the new data kind `placename` (`Harrowholt`, `The Scarred Downs`, `Witchgate Barrows`, `The Grove of the Hunt`, `Prince's Vow`), steered by its character (coast, river, lake, height, ore, pass, barren, fertile); no numerals | Paul, 2026-11-19: "'East Wood' is fine but 'West Wood III' is pretty boring" |

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
| name words (r3) | every definition of kind `placename` (4.1), in canonical (ascending id) order |
| world seed, N | |

S = `elevation` + `water_depth` (dm), the water or land surface, as in P1-03 and P1-05. Streams (names unchanged from P1-00 3): **K_seed = `stream_key(seed, "geo.regions.seed")`**, used only by `hash_at` (site tie-break, RG4); **K_cost = `stream_key(seed, "geo.regions.cost")`**, a noise key used only by `fbm_m` (RG3). From r3: **K_name = `substream(stream_key(seed, "geo.regions.name"), N)`** (P0-01 7.3; N the grid width), used only by `hash_at3` (the name draws of RG11); the map size is in the key so that worlds of one seed at two sizes do not share names region id by region id (measured before the change: 20 of 33 drawn names of 1024 seed 0 also in 2048 seed 0). The stream name is new (P1-00 section 3 gains it; a constant in `streams.hpp`). No `Rng`.

Checks, each fatal, in this order:

1. **At the stage entry** (`run_regions`, reading the stored world): every input layer present (`regions: input precondition: missing layer '<name>'`, inputs in the order of the table above); each stored layer of its catalogue type (`… layer '<name>' does not have its catalogue type`) and of the size of `elevation` (`… <name> length is not width * height`). Table `pass` is read in `id` order.
2. The thread count in [1, 256] (`regions: thread count outside [1, 256]`).
3. The tuning: each field inside its schema range of 4 (`regions: tuning: <field> outside its schema range`, declaration order), then the named checks of 4 (`regions: tuning: <message>`). With data loaded by P0-03 these cannot fail; they guard the seams.
4. Each biome definition's `region_noun` matches the word grammar of 4 (`regions: biome '<key>' has a region_noun that is not one word`, definitions in ascending `code`). The shipped nouns all match (`Sea`, `Mere`, `Wood`, `Downs`, `Marsh`, `Moor`, `Peaks`, `Waste`, `Heath`, `Steppe`, `Carr`, `Fen`).
4a. (r3) The `placename` definitions cover the land (the loader's checks of 4.1 have passed; these need the biome definitions too), in this order, each with the prefix `regions: placename: `: (i) every biome definition with `water` = false, in ascending `code`, is named in the `biomes` of exactly one definition of role `land` (`biome '<key>' has no land definition`; the loader already refuses a biome in two definitions); (ii) a land definition of each `special` value `isle`, `lake`, `upland`, in that order (`no land definition for '<special>'`); (iii) for every land definition (ascending id), every form (in order) and every slot of the form (left to right), with L the slot's list and (w0, w1, w2) = `name_pool_weights.<L>`: w0 > 0 and the common definition's L is not empty, or w1 > 0 and the land's own L is not empty (`'<id>' form <i> slot '<slot>' has no words`, with `<i>` the form's index in `forms` counted from 0 and `<slot>` the slot with its braces, as `regions: placename: 'fen' form 2 slot '{feature}' has no words`; r4). So every slot of every attempt has a word without help from traits.
5. With the prefix `regions: input precondition: `: the P1-00 grid checks; every input array of length W × H (`<name> length is not width * height`); `water` in 0..3 and `river` in 0..15 (`<name> value <v> at (<x>, <y>)`, lowest index); every `biome` value the `code` of a loaded definition (`biome value <v> at (<x>, <y>) has no definition`); every pass `path` non-empty, a whole number of `u32` cells, each index below W × H, consecutive cells equal or 8-neighbours (`pass <id>: path is not a chain of cells`); the col cell (`col_x`, `col_y`) inside the grid and on the path (`pass <id>: col cell is not on its path`; r2); then G7 with k = 1 (`regions: input precondition: G7: …`).

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

"Dry" cells are cells with `water` ∈ {0, 1} (dry land and river cells; the score layers are defined there, P1-05 3). Every region contains its seed. On hydrology output the seed is a site cell, so every region has a dry cell; a hand-made landmass without a site cell (RG6) can give a region with **no dry cell**, and then every value taken over dry cells is 0 (`elev_mean_dm`, `elev_max_dm`, the six means, `habitability_p90`, `ore_rich_cells`, `ore_reach_cells`). Means are RHA(sum, count) (round half away from zero; P0-01 5).

| Column | Value | Range | Read by history for |
| --- | --- | --- | --- |
| `id` | 1 … R (RG9) | | `civilization.home_region`, `event.region_id`, `beast_population.region_id` |
| `name` | the land's own name (RG11; r3: its base name, or a name drawn from the `placename` lists) | unique; ASCII letters, spaces and `'`; at most 65 bytes | display, history sentences, the history name registry (P3-02 4.4) |
| `biome` | dominant biome code (RG10) | a non-water code | the region's character, beast and race preference |
| `x_min`, `y_min`, `x_max`, `y_max` | inclusive cell bounds of the region's cells | 0..N−1 | quick spatial lookup |
| `seed_x`, `seed_y` | the region's seed (RG4, RG7), its best settlement site | a site cell of the region (RG6: the landmass's smallest-index cell if the landmass has no site cell) | the first village site |
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
| `river_class_max` | the largest `river` value over every river cell (`water` 1, any class) of the pairs, 0 if none; it is not limited to border river cells, so a border can have `river_pairs` 0 and `river_class_max` 1 to `river_border_class` − 1 |
| `pass_id` | the smallest `pass.id` of the passes **attached** to this border (RG12: each pass is attached to at most one border, the crossing of its path nearest its col), 0 if none |
| `passes` | the number of passes attached to it |

The design's "whether it is walkable or a river": walkable ⟺ `walk_pairs` > 0; a river border ⟺ `river_pairs` > 0. A border with `walk_pairs` = 0 is a barrier on foot; its `low_dm`, `low_x`, `low_y` say where a road over it would be shortest in height, and a non-zero `pass_id` that a natural pass crosses it near its col. The pass itself is located by `pass.col_x`, `col_y`, not by the border: the col usually lies 16–336 m (median about 150 m) inside one of the two regions, since the border runs below the crest on one flank.

`region_biome`: `region`, `biome` (a code), `cells` (> 0, the region's cells with that `biome` value). Lakes count under the lake code. Σ `cells` over a region's rows = `region.cells`.

### 3.4 What is left to history

History computes these itself from the tables and layers: per-race suitability of a region (its own weights over the means and `region_biome`); region capacity and population; beast populations (from `region_biome`, `forage_mean` and the predator rules of the beast data); cultural names; which borders a civilization claims; roads and bridges (`low_x`, `low_y` and `river_pairs` say where); sea regions and sea routes (none here, 12 question 2); groupings of regions into provinces or realms. Three notes from the world critic's review: (a) **passes** are located by `pass.col_x`, `col_y` and the `region` layer; `pass_id` names the border the pass leads across; (b) **river borders**: judge whether a large river runs *along* a border by `river_pairs` × 16 / `length_m` (a share of the border's length), not by `river_pairs` > 0, which also fires where a river merely flows from one region into the next; a region's own `river_class_max` ≥ 6 means a river town may need a bridge inside the region; (c) **islets**: regions with less than about 0.3 km² of dry land (`cells` − `lake_cells` < about 1200) are rocks, not homelands; do not seed civilizations there.

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
| `upland_own_noun` | array(ref(biome), 0, 16) | `["alpine", "heath", "highland_moor"]` | taste (r2) | upland biomes that keep their own noun in an upland region (RG11 3) |
| `name_pool_weights` | record `name_pools` | below | taste (r3) | for each word list, the weights of its three pools: common, land, trait (RG11 6) |
| `name_traits` | record `name_traits` | below | taste (r3) | the tests that give a region a trait (RG11 5) |

Record `region_names`, all fields `string(32, word)` and required, in this order: `central`, `north`, `northeast`, `east`, `southeast`, `south`, `southwest`, `west`, `northwest`, `isle`, `upland` (r2). **Word grammar**: one ASCII capital letter followed by zero or more ASCII lower-case letters (`[A-Z][a-z]*`), 1 to 32 bytes. The same grammar applies to every biome's `region_noun` (check 4 of section 2). It makes every name a sequence of words without spaces or digits, which the uniqueness argument of RG11 uses. The loader's `string` type takes a grammar (P0-03 4); `word` is a new named grammar beside the id grammar, registered with this kind (a P0-03 amendment for the lead, one line).

**Named checks** (per-definition, P0-03 7.5; reported at the definition's header as `<path>:<line>:<column>: error: check '<name>' failed for 'worldgen.regions': <message>`), in this order:

| Check | Condition | Message |
| --- | --- | --- |
| `regions_tile_fits_spacing` | `seed_tile_m` ≤ 2 × `seed_spacing_m` | `seed_tile_m must be at most twice seed_spacing_m` |

(Without it a tile larger than twice the spacing could leave land more than one spacing from any seed's candidate; the shipped 864 ≤ 2400.)

Record `name_pools` (r3), all fields required, in this order: `nouns`, `suffixes`, `prefixes`, `adjectives`, `features`, `possessors`, `places`, `objects`; each `array(int(0, 1000), 3, 3)`, the weights [w0, w1, w2] of the common pool, the land pool and the trait pool of that list (RG11 6). A pool with no words gets weight 0 whatever its tuning weight.

Record `name_traits` (r3), all fields required, in this order:

| Field | Type | Shipped | A region has the trait when | Measured share of regions, 512 / 1024 / 2048 |
| --- | --- | --- | --- | --- |
| `coast_cells` | int(0, 100000000) | 128 | `coast` ⟸ `coast_cells` ≥ this (about 2 km of shore) | 51 / 22 / 11% |
| `river_class` | int(1, 16) | 7 | `river` ⟸ `river_class_max` ≥ this (16: never) | 3 / 17 / 29% |
| `lake_permille` | int(1, 1000) | 125 | `lake` ⟸ `lake_cells` > 0 and 1000 × `lake_cells` ≥ this × `cells` | 4 / 7 / 7% |
| `high_mean_m` | int(0, 10000) | 400 | `high` ⟸ `elev_mean_dm` ≥ 10 × this | 20 / 23 / 15% |
| `ore_rich_cells` | int(0, 100000000) | 1000 | `ore` ⟸ `ore_rich_cells` ≥ this (0.26 km² of rich ore) | 17 / 20 / 12% |
| `barren_hab_max` | int(−1, 255) | 20 | `barren` ⟸ `habitability_mean` ≤ this (−1: never) | 30 / 25 / 10% |
| `fertile_min` | int(0, 256) | 200 | `fertile` ⟸ `fertility_mean` ≥ this (256: never) | 9 / 12 / 15% |

The eighth trait, `pass`, has no tuning: a region has it when it is `a` or `b` of a `region_border` row with `passes` > 0 (RG12; 6 / 6 / 4%). Shares measured by the prototype of 14.2 on its 8 worlds at 512, 11 at 1024 and 5 at 2048.

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
names = { central = "Central", north = "North", northeast = "Northeast", east = "East", southeast = "Southeast", south = "South", southwest = "Southwest", west = "West", northwest = "Northwest", isle = "Isle", upland = "Fells" }
upland_own_noun = ["alpine", "heath", "highland_moor"]
name_pool_weights = { nouns = [0, 1, 0], suffixes = [0, 4, 1], prefixes = [6, 3, 2], adjectives = [6, 3, 2], features = [5, 1, 2], possessors = [8, 1, 1], places = [8, 1, 1], objects = [6, 3, 2] }
name_traits = { coast_cells = 128, river_class = 7, lake_permille = 125, high_mean_m = 400, ore_rich_cells = 1000, barren_hab_max = 20, fertile_min = 200 }
```

`seed_tile_m`, `seed_spacing_m`, `hand_ref_m`, `river_cost_m`, `region_min_area_m2` and `ore_reach_m` are physical (P1-00 2.2); `name_central_frac` is a map fraction. The biome nouns stay in the biome files (`region_noun`, P1-04 4.1); this stage changes no biome file. r3 adds the last two lines of the block (written into `game/data/worldgen/regions.toml` by the engineer with the implementation, and into both fixture copies; the shipped file on this branch is unchanged so that `main` keeps loading until the loader knows the fields).

### 4.1 Data kind `placename` (r3)

The words of drawn region names. A new kind (P0-03 7.1): name **`placename`**, directory **`placenames`**, **required**, open ids. Shipped files, normative and committed with this revision: **`game/data/placenames/common.toml`** (role `common`), one file per land (`wood.toml`, `carr.toml`, `downs.toml`, `marsh.toml`, `fen.toml`, `moor.toml`, `heath.toml`, `peaks.toml`, `waste.toml`, `steppe.toml`, `isle.toml`, `mere.toml`, `fells.toml`, role `land`) and **`traits.toml`** (eight definitions `trait_<name>`, role `trait`). The fixtures `worldgen/tests/data/geo/placenames/` and `worldgen/tests/data/geo-small/placenames/` become identical copies with the implementation (every data root that loads must hold every required kind; the copies are not on this branch, which changes no test). A loader that does not know the kind ignores the directory (P0-03 7.2), so the files change no world until the kind is registered.

Kinds are processed in ascending name order, so `placename` comes after `history` and before `race`; it is a new required kind, so every content hash changes (P0-03 8).

**Fields**, all required, in this declaration order (it fixes the canonical encoding, P0-03 8; definitions in ascending id):

| Field | Type | Meaning |
| --- | --- | --- |
| `role` | enum(`common`, `land`, `trait`) | what the definition is |
| `biomes` | array(ref(biome), 0, 16) | role `land`: the biomes whose regions it names (by dominant biome, RG11 3 d) |
| `special` | enum(`none`, `isle`, `lake`, `upland`) | role `land`: the noun rules a, b, c of RG11 3 instead of biomes |
| `trait` | enum(`none`, `coast`, `river`, `lake`, `high`, `ore`, `pass`, `barren`, `fertile`) | role `trait`: which trait (RG11 5) |
| `forms` | array(record `place_form`, 0, 16) | role `land`: the name forms and their weights |
| `nouns` | array(string(24, word), 0, 1024) | the land's nouns: `Wood`, `Weald`, `Holt` |
| `suffixes` | array(string(24, element), 0, 1024) | second half of a one-word name: `holt`, `hurst` |
| `prefixes` | array(string(24, word), 0, 1024) | first half of a compound: `Thorn`, `Black` |
| `adjectives` | array(string(24, word), 0, 1024) | `Ashen`, `Drowned` |
| `features` | array(string(24, element), 0, 1024) | second half of a qualifier: `water`, `gate` |
| `possessors` | array(string(24, word), 0, 1024) | `Widow`, `Hangman` |
| `places` | array(string(24, word), 0, 1024) | `Reach`, `Folly` |
| `objects` | array(string(32, name_object), 0, 1024) | `Crows`, `the Drowned` |
| `forbidden` | array(string(16, element), 0, 64) | role `common`: substrings no word of a name may contain (RG11 7 V4) |
| `excluded` | array(string(48, place_name), 0, 1024) | role `common`: names, and single words, a name may not be or contain (RG11 7 V5) |

Record `place_form`: `form` string(48, place_form), `weight` int(1, 1000), both required, in that order.

**Grammars** (kind-local `StringGrammar`s; the description is printed in error messages):

| Grammar | Rule | Description |
| --- | --- | --- |
| `word` | P1-06 4's: `[A-Z][a-z]*` | (existing) |
| `element` | one or more ASCII lower-case letters `[a-z]+` | `lower-case letters a-z` |
| `name_object` | a `word`, optionally preceded by `the` and one space | `a capitalised word, optionally after 'the '` |
| `place_name` | ASCII letters, spaces and `'`; starts with a letter; no leading, trailing or double space | `a name (letters, single spaces and ')` |
| `place_form` | literal characters (ASCII letters, space, `'`) and slots; a slot is one of `{noun}`, `{suffix}`, `{prefix}`, `{adjective}`, `{feature}`, `{possessor}`, `{place}`, `{object}`; at least one slot, none twice; no other `{` or `}`; no leading, trailing or double space | `a name form (letters, spaces, ' and slots such as {noun}, each slot at most once)` |

**Named checks**, per definition (P0-03 7.5, reported `… failed for 'placename.<id>': <message>`), in this order:

| Check | Condition | Message |
| --- | --- | --- |
| `placename_role_fields` | role `common`: `biomes` empty, `special` `none`, `trait` `none`, `forms` and `nouns` empty; role `land`: `trait` `none`, exactly one of (`biomes` non-empty) and (`special` ≠ `none`), `forms`, `nouns` and `suffixes` non-empty, `forbidden` and `excluded` empty; role `trait`: `trait` ≠ `none`, `biomes` empty, `special` `none`, `forms`, `nouns`, `forbidden` and `excluded` empty | the first failing condition, in the order written: `a <role> definition must have <field> <empty / none / non-empty / not none>`; for the land's either-or: `a land definition must name biomes or a special, not both` / `… not neither` |
| `placename_lists_unique` | no string twice within one list (`biomes`, `nouns` … `excluded`; forms compared by `form` text) | `<field>[<j>] '<value>' repeats <field>[<i>]` (first list in declaration order, first repeat j, its first occurrence i) |

**Kind checks** (P0-03, over all definitions in id order), in this order:

| Check | Condition | Message |
| --- | --- | --- |
| `placename_one_common` | exactly one definition has role `common` | `there must be exactly one common definition, found <n>` |
| `placename_lands_distinct` | no biome key in the `biomes` of two land definitions; no `special` value other than `none` in two; no `trait` value other than `none` in two | `biome '<key>' is in '<id1>' and '<id2>'` / `special '<s>' is in …` / `trait '<t>' is in …` (ids ascending) |
| `placename_common_sizes` | the common definition has at least 100 `prefixes`, 100 `adjectives`, 16 `features`, 32 `possessors`, 16 `places` and 32 `objects` | `the common definition needs at least <n> <field>, has <m>` (first field in declaration order) |

The coverage of the biomes and the fillable slots need the biome kind and the tuning, so the stage checks them (section 2, check 4a).

**Shipped sizes** (the files are normative; counts checked by the prototype):

| Definition | nouns | suffixes | prefixes | adjectives | features | possessors | places | objects |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `common` | 0 | 0 | 156 | 159 | 33 | 94 | 52 | 78 |
| each land (13) | 5–11 | 4–8 | 7–23 | 4–10 | 0–2 | 0 | 0 | 3–7 |
| each trait (8) | 0 | 0–3 | 0–9 | 2–4 | 0–3 | 1–3 | 2–4 | 3–5 |
| distinct over all | 87 | 67 | 282 | 222 | 50 | 105 | 70 | 140 |

`common` also holds 21 `forbidden` substrings (19 in r3; r4 adds `boner` and `dung`, 14.8) and 90 `excluded` entries. Every land has the same six forms with weights 30, 22, 22, 12, 9, 5 (RG11 6); a land may have its own.

**Taste rules of the shipped lists** (for whoever edits them): dark, old, weathered English words, ASCII, never comic or modern; common words fit any land (colours, moods, omens, death and ritual, fire, weather, hedge plants), so they hold no land noun (`Wood`, `Fen`, `Moor`, `Barrow`, `Scar`, …), no compass word and no plant or beast tied to one land; a land's words are its own (fen: `Bog`, `Mire`, `Quag`, `Peat`, `Wisp`, `Fever`); a trait's words describe the trait (coast: `Gull`, `Wrack`, `strand`; ore: `Iron`, `Slag`, `seam`). Prefixes that lead mostly to famous names were left out (`Shadow`, `Dusk`, `Mirk`, `Storm`, `Winter`, `Dragon`, `Doom`, `Fel`); the `excluded` list catches the rest the lists can make (14.3).

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

Label the 8-connected components of land with G2 (labels 1 … M in order of smallest index; `landmass`). For each landmass in label order that holds no seed, append a seed at its best site cell, or, if it has none, at its smallest-index cell, numbered after the existing seeds. (A landmass without a site cell does not occur on hydrology output: every island has dry land. On hand-made input the smallest-index cell may be a river or lake cell; it is still the seed, `seed_x`, `seed_y`, and the sector of RG11; the region's dry-cell values follow 3.2.)

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

**Dominant biome**: the code with the most cells among the region's cells whose definition has `water` = false, ties to the smaller code. If the region has no such cell (only possible for a hand-made landmass without a site cell, RG6), the code with the most cells among all its cells, ties to the smaller code (for a lake-only landmass the `lake` code, so its noun is `Mere`, or `Isle` by RG11 3); everything else about the region (`walk_area` 0, `region_biome` rows, borders, `report`) follows the general rules.

### RG11. Names (r3; evaluated after RG12, since the `pass` trait reads the border rows)

With the words of `names` (record `region_names`), the biome nouns, the `placename` definitions (4.1) and the tuning records `name_pools` and `name_traits`. Every value below is an integer or an ASCII string; "lower case" maps `A`–`Z` to `a`–`z` and nothing else; string comparisons are by bytes.

1. **Land centroid**: L = the number of land cells, cx = RHA(Σx, L), cy = RHA(Σy, L) over land cells. r_c = round_int(mul_int(`name_central_frac`, N)), with N the grid **width** W on a non-square test grid (worlds are square).
2. **Sector** of a region from its seed: dx = seed_x − cx, dy = seed_y − cy (y grows southward). If dx² + dy² < r_c²: `central`. Otherwise, with ax = |dx|, ay = |dy|: if 29 ay ≤ 12 ax, `east` (dx > 0) or `west`; else if 29 ax ≤ 12 ay, `south` (dy > 0) or `north`; else `southeast`, `southwest`, `northeast` or `northwest` by the signs of dy and dx. (12/29 = 0.4138 approximates tan 22.5° = 0.4142.)
3. **Noun and land**, the first rule that applies (r2 added rules b and c; r3 adds the land, the `placename` definition of role `land` whose words the region draws):
   a. `isle` if the region is the only region of its landmass and that landmass is not the largest (most cells; ties to the smaller label); land: the definition with `special` `isle`;
   b. the `region_noun` of the lake biome (the definition with `water_body` "lake"; `Mere` as shipped) if 2 × `lake_cells` ≥ `cells`; land: `special` `lake`;
   c. `upland` (`Fells` as shipped) if the region is upland, 4 × `walk_cells` < `cells` (the definition of `regions_upland`, 9), and its dominant biome is not in `upland_own_noun`; land: `special` `upland`;
   d. the `region_noun` of its dominant biome; land: the definition whose `biomes` holds the dominant biome's key, or, if the dominant biome has `water` = true (only on hand-made input, RG10), `special` `lake`.

   So an upland region dominated by forest or grassland is `Fells`, not `Wood` or `Downs`; one dominated by alpine, heath or moor keeps `Peaks`, `Heath` or `Moor`.
4. **Base name** = sector word, one space, noun (`East Wood`). (From generator 0.19.0 the keeper is chosen per land noun, not per base name: 15.7.) The **keeper** of a base name is, among the regions with that base name, the one with the most `cells`, ties to the smaller id. Each keeper is named its base name. Every other region is named by drawing (6 to 8). (Before r3 every region after the first of its base name took a Roman numeral; r3 has no numerals.)
5. **Traits** of a region, from its row and the border rows (tests of record `name_traits`, 4): `coast`, `river`, `lake`, `high`, `ore`, `pass`, `barren`, `fertile`, each held or not. The **trait order** is this order (the order of the `trait` enumeration of 4.1), not the order of the definitions' ids.
6. **One attempt** t (t = 0, 1, …, 63) for region id r with land definition D, common definition C and the region's trait definitions T_1 … T_m (the role-`trait` definitions of the traits it holds, in trait order; a held trait without a definition contributes nothing). Draws: u(k) = `hash_at3`(K_name, r, t, k) (`uint64`; P0-01 7.3, x = r, y = t, z = k). `pick(u, n)` and `weighted(u, w)` are P3-02 4.1's (`pick(u, n)` = ((u >> 32) × n) >> 32; `weighted` = the smallest i with (u >> 32) × W < 2^32 × (w[0] + … + w[i]), W = Σ w, in `uint128`). Then:

   ```
   F    = D.forms                                       // non-empty (4.1)
   form = F[weighted(u(0), [f.weight for f in F])].form
   k    = 1
   for each slot s of form, left to right:              // a slot names a list L: {noun} nouns, {suffix} suffixes, ...
       P0 = C.L;  P1 = D.L;  P2 = T_1.L ++ … ++ T_m.L   // concatenation in trait order, duplicates kept
       w  = [len(Pi) > 0 ? name_pool_weights.L[i] : 0 for i = 0, 1, 2]
       p  = weighted(u(k), w)                            // Σ w > 0: check 4a (iii)
       value[s] = Pp[pick(u(k + 1), len(Pp))]
       k  = k + 2
   name = form with each slot replaced by its value
   ```

   A form has at most 8 slots, so k ≤ 16. The words keep their case from the lists: nouns, prefixes and the rest capitalised, suffixes and features lower case, so `{prefix}{suffix}` is one capitalised word (`Thorn` + `holt` = `Thornholt`) and an object keeps its `the` (`The Grove of the Hunt`).
7. **Validity** of the attempt, every rule must hold:
   - **V1 joins.** For every two slots that are adjacent in the form (nothing between them, such as `{prefix}{suffix}`), with x the lower-cased left value and y the lower-cased right value: the last byte of x differs from the first byte of y, they are not both vowels (`a`, `e`, `i`, `o`, `u`), and len(x) + len(y) ≤ 12 bytes. (Rejects `Birchhurst`, `Hollowwood`, `Bramblealder`, `Hemlockhollow`, `Cormorantskerry`.)
   - **V2 no triple letters**: the lower-cased name has no three equal consecutive letters.
   - **V3 no repeated word**: for any two different slots of the form, the lower-cased value of one is not a substring of the lower-cased value of the other (rejects `The Hollow Hollows`, `Fenmoss Moss`, `The Wood of Woods`).
   - **V4 forbidden**: no word of the lower-cased name (the name split at spaces; an apostrophe is part of its word) contains an entry of C.`forbidden` as a substring.
   - **V5 excluded**: the name is not an entry of C.`excluded`, and no word of the name (split at spaces) equals an entry of C.`excluded` that has no space (so `Blackrock` also rejects `Blackrock Downs`).
   - **V6 length**: the name has at most 48 bytes.
8. **Order, uniqueness and fallback.** U = the set of keeper names (all of them, before any draw). Then for each region that is not a keeper, in id order:

   ```
   for t = 0, 1, …, 63:
       if attempt t is valid and its name is not in U: name the region with it; add it to U; next region
   fallback: for i over P = C.prefixes ++ D.prefixes, for j over S = D.suffixes ++ C.suffixes (i outer, both in list order):
       n = P[i] + S[j]; if {prefix}{suffix} with these values passes V1 to V6 and n is not in U:
           name the region n; add it to U; next region
   fatal: regions: no free name for region <r>
   ```

**Uniqueness.** Keepers are named by distinct base names (one keeper per base name); every drawn name is checked against U, which holds every keeper name and every name drawn so far; so all names are distinct. Names never carry a numeral.

**Why 64 attempts and why the fallback is not reached with the shipped data.** An attempt fails when it is invalid or when its name is in U. Measured with the shipped data (14.3; 20,000 first attempts per land): 1.7% (fen) to 4.5% (moor) of attempts are invalid, and the most frequent single name of a land takes 0.16% (wood, `The Primeval Holt`) to 0.42% (isle, `The Sunken Isle`) of the attempts. A region competes only with the names of its own land in U: at most about 130 at 2048 (the drawn wood regions of the five 2048 worlds of 14.2: 97 to 115) plus a handful of keepers. Even if every one of them were the land's most frequent name, an attempt fails with probability below 0.045 + 130 × 0.0042 < 0.6, and 64 attempts all fail with probability below 0.6^64 < 10^-14 per region; measured on the 24 worlds of 14.2, 2.7% of 1208 attempts were invalid and 0.7% hit a taken name, and the accepted attempt was at most 2. The fallback is still exact and bounded: it enumerates at most |P| × |S| compounds (at most 2048 × 2048) and succeeds unless every valid one-word compound of the land (624 for steppe to 1331 for wood with the shipped r4 lists, 14.3) is taken, which needs more drawn regions of one land than that in one world, five to ten times the largest 2048 count. The fatal error is reached only with data or tuning far from the shipped (many more regions, or tiny lists), and it is an error rather than a numeral by Paul's decision.

**Determinism.** A region's attempts depend only on K_name (seed and N), its id, its land, its traits and the data; the only dependence between regions is U, which is built in a fixed order (keepers, then id order). So the names are a pure function of the region rows, the border rows, the data and the seed, independent of thread count and evaluation order. No `Rng`; `hash_at3` with a stage-specific key, as `hash_at` in RG4.

### RG12. Borders and passes (D8 counts per pair, then sorted; passes sequential)

Every border pair (3.3) adds to its row: `length_m` += 16; `low_dm`, `low_x`, `low_y` by the first pair attaining the minimum; `walk_pairs`, `river_pairs`, `river_class_max` as defined. Rows sorted by (a, b).

**Passes.** For each row of table `pass` in `id` order, with path cells c_0 … c_L: build the step sequence by inserting, between each diagonal step c_j → c_{j+1}, the corner (c_{j+1}.x, c_j.y) if it is land, else (c_j.x, c_{j+1}.y) if it is land, else nothing; equal consecutive cells are skipped. For every consecutive pair (u, v) of the sequence that are 4-neighbours, both land, with region(u) ≠ region(v): the pass **crosses** the border row (min, max) of the two regions (the row exists, since (u, v) is a border pair). **Attachment (r2).** Let q be the position of the pass's col cell (`col_x`, `col_y`) in its path (the first position if it occurs twice). A crossing made inside step j (the step from c_j to c_{j+1}, corner included) has distance d = min(|j − q|, |j + 1 − q|) path steps from the col. The pass is **attached** to the border of the crossing with the smallest (d, j), and among crossings of the same step to the first in sequence order; a pass whose path crosses no border is attached to none. Each row records, over the passes attached to it: `passes` = how many, `pass_id` = the smallest id. (Before r2 a pass was recorded on every border its path crossed: 31 of 43 such rows in the critic's 24 worlds were lowland borders on the approach, kilometres from the col.)

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

**RG11 names** at N = 1024, land centroid (500, 480), `name_central_frac` "0.12" (raw 515396076): r_c = round_int(122.88) = 123. Seeds and dominant biomes, regions in id order: (520, 470) grassland → d² = 500 < 15129: `Central Downs`; (800, 500) forest → dx 300, dy 20, 29 × 20 ≤ 12 × 300: `East Wood`; (200, 200) alpine → dx −300, dy −280: `Northwest Peaks`; (510, 900) marsh → 29 × 10 ≤ 12 × 420: `South Marsh`; (790, 560) forest → base name `East Wood` again: the two `East Wood` regions are compared by `cells` and the larger keeps `East Wood`, the other draws a name (r3; before r3 the second was `East Wood II`); a lone region on a small island north of the centre: `North Isle`. Rules b and c (r2): (700, 300) with 120 of its 200 cells lake → dx 200, dy −180, diagonal: `Northeast Mere`; (210, 700) dominated by forest with 4 × `walk_cells` < `cells` → dx −290, dy 220 (29 × 220 > 12 × 290 and 29 × 290 > 12 × 220): `Southwest Fells`; (230, 720), upland and dominated by alpine → `Southwest Peaks` (alpine is in `upland_own_noun`); an upland region 50% lake → `Mere` (b before c). Sector checks (dx, dy): (10, 4) east; (10, 5) southeast; (−3, −7) northwest; (0, −9) north; (4, 10) south; (5, 12) southeast. (r3: region names no longer use `roman`; it stays in `worldgen/core/roman.hpp` for P3-02's fallback.)

**RG11 draws (r3)**, the shipped data and tuning, the 1024 world of seed 42 (`worldgen-cli generate --seed 42 --map-size 1024`, its region and border rows as written by 0.11.0 to 0.16.0, which r3 leaves unchanged). stream_key(42, "geo.regions.name") = 0x224A148B98263929; K_name = substream(…, 1024) = **0xE1A9F22A266E243A**. Pools as [common, land, trait]; a weight 0 marks an empty pool.

- Region 1, base `North Downs` (4443 cells), not the keeper: of the four `North Downs` regions (ids 1, 2, 6, 16; 4443, 6898, 14036, 5593 cells) region 6 keeps the base name. Land `downs`; traits: coast (`coast_cells` 177 ≥ 128). t = 0: u(0) = 0x6859A960857622CB → form 1 `The {adjective} {noun}` (weights 30, 22, 22, 12, 9, 5); adjective: u(1) = 0x7EE3FBE2B3DAA4E4, weights [6, 3, 2] → pool 0 (159 words), u(2) = 0xA731588330C4201D → index 103 `Scarred`; noun: u(3) = 0xE72102C4301D4A38, weights [0, 1, 0] → pool 1 (8), u(4) = 0x26E0EFE6AE26CED5 → index 1 `Downs`. Valid, free: **`The Scarred Downs`**. (Attempt 1 would give `Fletcher's Wolds`.)
- Region 10, `Northeast Downs` base (river 8, fertility 237: traits river, fertile). t = 0: u(0) = 0x42002C340A52E948 → form 0 `{prefix}{suffix}`; prefix: u(1) = 0x2056F9F4076F2CC2, [6, 3, 2] → pool 0 (156), u(2) = 0x80DC08382FB40AEE → 78 `Knell`; suffix: u(3) = 0xA85791F9B2BC6F64, [0, 4, 1] → pool 1 (6), u(4) = 0x672EF6DA06D6A3DA → 2 `down`. **`Knelldown`**.
- Region 39, `Southeast Wood` base, trait coast. t = 0: form 0; prefix pool 0 index 62 `Harrow` (u(2) = 0x673CC715D61964E8), suffix pool 1 (8) index 4 `holt` (u(4) = 0x99255536F43A47E7): **`Harrowholt`**.
- Region 40, `Southeast Wood` base, no trait (coast 113, river class 4, mean elevation 101 m, habitability 59, fertility 112). t = 0: u(0) = 0x1DAD0A4F3DA9416D → form 0; prefix: u(1) = 0xD767ECABF011F7C6, weights [6, 3, 0] (no trait pool) → pool 1 (23), u(2) = 0x2574A736E868C8E5 → 3 `Birch`; suffix: u(3) = 0x21EE6D0295292197, [0, 4, 0] → pool 1, u(4) = 0xA4FFEA0405079A69 → 5 `hurst`: `Birchhurst` fails V1 (h, h). t = 1: u(0) = 0xF8C4AD91FCA3D645 → form 5 `{possessor}'s {place}`; possessor: u(1) = 0xB4FB47D898CFA306, [8, 0, 0] → pool 0 (94), u(2) = 0xA07474902FABD7AE → 58 `Prince`; place: u(3) = 0xDC51F8D891CC3346, [8, 0, 0] → pool 0 (52), u(4) = 0xF07596C36D36F72F → 48 `Vow`. **`Prince's Vow`**.

Validity checks (V1 to V6): `Birchhurst`, `Brambleedge`, `Hallowwell` V1 (same letter); `Bramblealder` V1 (two vowels); `The Hollow Hollow`, `Fenmoss Moss` V3; `Blackwood`, `The Haunted Forest` V5 (whole name); `Blackrock Downs` V5 (word); `Hollywood` V5. The fallback order for land `fen`: P = the 156 common prefixes then the 11 fen prefixes, S = the 6 fen suffixes (`bog`, `fen`, `mire`, `moss`, `quag`, `sike`); the first compound is `Adderbog`.

**RG12 borders and passes.** A 4 × 3 test world, all land, region 1 on columns 0–1 and region 2 on columns 2–3; S rows `10 20 30 10`, `10 50 40 10`, `10 25 25 10`; `walk_area` 1 everywhere except 0 at (1, 1) and (2, 1); (2, 0) a river cell of class 6. Border pairs (1, y)–(2, y) for y = 0, 1, 2 with max S 30, 50, 25. Row (1, 2): `length_m` 48, `low_dm` 25, `low_x`, `low_y` = (1, 2) (equal S: the first cell), `walk_pairs` 2, `river_pairs` 1, `river_class_max` 6. If (2, 0) were of class 4 instead (below `river_border_class`), the row would have `river_pairs` 0 and `river_class_max` 4. A pass with path (0, 1), (1, 0), (2, 1), (3, 1) and col (1, 0) (q = 1): the step (0, 1) → (1, 0) gets the corner (1, 1) (inside region 1); the step (1, 0) → (2, 1) (j = 1, d = 0) gets the corner (2, 0), and (1, 0)–(2, 0) crosses (1, 2): the pass is attached to row (1, 2). With this pass as id 3 and a second attached pass of id 5: `pass_id` 3, `passes` 2.

**RG12 attachment (r2).** A 9 × 1 strip of land, regions 1 on x 0–2, 2 on x 3–5, 3 on x 6–8, a pass with path (0, 0), (1, 0), …, (8, 0). Crossings at steps j = 2 (row (1, 2)) and j = 5 (row (2, 3)). Col at (5, 0), q = 5: d = 2 and 0, attached to (2, 3). Col at (4, 0), q = 4: for j = 2, d = min(|2 − 4|, |3 − 4|) = 1; for j = 5, d = min(|5 − 4|, |6 − 4|) = 1: a tie, the smaller j wins, attached to (1, 2). Col at (1, 0) on a path inside one region (all x in region 1): no crossing, attached to none.

## 6. Invariants

1. `region` > 0 exactly on land cells (`water` ≠ 3); 0 on ocean.
2. Region ids are 1 to R, each used, increasing with each region's smallest cell index; R = the number of `region` rows.
3. Each region is 8-connected, lies on one landmass and contains its seed (`seed_x`, `seed_y`), which is a site cell unless the landmass has none (then its smallest-index cell, RG6); `landmass` is that landmass's G2 label.
4. Every region has at least min_cells cells, unless it has no 4-adjacent neighbour region.
5. Every column of the three tables equals its definition recomputed from the layers, the definitions and table `pass` (bounds, counts, means, percentile, dominant biome, walk area, the border rows and exactly the pairs that share a 4-adjacent cell pair, with `a` < `b`; the biome rows sum to `cells`).
6. Names are unique and carry no numeral (r3; from 0.19.0 as restated in 15.7 item 4). For each base name (sector word + noun of RG11 3) in use, exactly one region is named it: the one with the most `cells`, ties to the smaller id. Every other region's name is one of its land's forms filled with words of the pools of RG11 6 (common, its land, its traits), passes V1 to V6, and equals no base name in use.
8. (r2) Σ `passes` over `region_border` ≤ the number of `pass` rows; each pass is attached to at most one row, and to one exactly when its path crosses a border.
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
| RG11 | D4 | evaluated after RG12; keepers by (`cells`, id); drawn names in id order against the set U; each draw keyed by (K_name, region id, attempt, k) with `hash_at3`, so a name does not depend on how many draws other regions made |
| RG12 passes | D4 | id order |

No floating point, no new math function, one noise key, one `hash_at` key and (r3) one `hash_at3` key, no `Rng`.

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

Corrupt data, each exit 3 with nothing on stdout (the forms of P1-04 9): a `region` layer that is not `u16` or not the size of `elevation` (`corrupt layer 'region': type <t>, expected u16` or `… size <W>x<H>, expected <N>x<N>`), whenever the layer exists; a `region_border` row with `a` ≥ `b` or with `a` or `b` not an id of table `region` (`corrupt table 'region_border': row (<a>, <b>): <reason>`, reasons `a not below b`, `unknown region <id>`, first failing row in key order); a `region` row with `cells` < 1 (`corrupt table 'region': row <id>: cells below 1`). `report` does not recompute the tables from the layer. **Order of the checks**, after every check of the earlier `[geography]` groups: (1) the `region` layer, when it exists (type, then size); (2) table `region`, every row in `id` order; (3) table `region_border`, every row in (`a`, `b`) order. The first failure is the one reported. The two table checks run whenever the tables have rows, also without the layer (as `pass` and `biome`); `region_border`'s `unknown region` check uses the `id` set of table `region`.

Example (512 seed 0, prototype): `regions = 14`, `region_area_km2 = 0.06 1.69 3.50`, `region_borders = 18`, `region_borders_walkable = 9`, `region_borders_river = 0`, `region_borders_pass = 0`, `regions_isolated = 3`, `regions_upland = 6`. (1024 seed 0: `regions = 56`, `region_area_km2 = 0.06 1.75 4.53`, `region_borders = 113`, `region_borders_walkable = 85`, `region_borders_river = 10`, `region_borders_pass = 2`, `regions_isolated = 6`, `regions_upland = 12`.) r2 changed only `region_borders_pass` (1024 seed 0: 4 → 2; 512 seed 0 has no pass).

**Statistics** (P1-00 13.1). Measured by the prototype of this spec on the 0.10.0 worlds of section 11 (512 seeds 0–19, 1024 seeds 0–29, 2048 seeds 0–5). Every value is taken with the report's own rule (the `region_area_km2` p50 by the rank rule of P1-00 10.3, the upper median for an even count; corrected 2026-11-18, the first table used the interpolated median). Typical = measured min–max widened by about 10%, rounded outward; hard only where a value outside means a defect. The 2048 ranges rest on six worlds and should be widened by the validation sweep (2048 seeds 0–19); the prototype's flood is exact, its noise and hand are those of the spec (they come from the P1-05 prototype's checked `fbm_m` and G7), so the validation sweep should reproduce these numbers closely.

| Statistic | All seeds (hard) | Typical (≥ 90%), 512 / 1024 / 2048 | Measured min / median / max, 512 / 1024 / 2048 |
| --- | --- | --- | --- |
| `regions` | ≥ 1 | 10–18 / 40–62 / 175–225 | 11 / 13 / 16; 45 / 49 / 56; 195 / 200 / 204 |
| `region_area_km2` p50 | 0.5–5 | 1.3–2.8 / 1.5–2.7 / 1.8–2.3 | 1.45 / 1.94 / 2.53; 1.74 / 2.13 / 2.43; 1.97 / 2.04 / 2.12 |
| `region_area_km2` p100 | ≤ 15 | ≤ 6.2 / ≤ 6.7 / ≤ 7.0 | 3.10 / 4.53 / 5.64; 3.37 / 4.78 / 6.05; 4.49 / 5.25 / 6.37 |
| `region_area_km2` p0 | descriptive (islands) | — | 0.05–1.06; 0.06–1.03; 0.06–0.81 |
| `region_borders` | descriptive | — | 18 / 24.5 / 30; 105 / 116 / 134; 510 / 522 / 545 |
| `region_borders_walkable` | descriptive | — | 9 / 17 / 23; 73 / 91 / 114; 400 / 476 / 503 |
| `region_borders_river` | descriptive | — / ≥ 7 / ≥ 85 | 0 / 1 / 6; 8 / 15 / 26; 98 / 122 / 136 |
| `region_borders_pass` | ≤ number of `pass` rows (r2) | — | 0 / 1 / 2; 0 / 2 / 3; 0 / 2.5 / 3 (before r2: 0 / 1 / 3; 0 / 2 / 7; 0 / 4.5 / 6) |
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

**Seams as implemented** (generator engineer, 2026-11-18; `worldgen/include/worldgen/stages/regions.hpp`, namespace `worldgen::stages`; the comments there state each function's fatal cases):

```
RegionTuning region_tuning_from(const data::ContentSet&);            // and from a data::Record
void check_region_tuning(const RegionTuning&);                        // schema ranges (a bad word: "names.<field>"), then the named check
std::int64_t region_min_cells(const RegionTuning&);
Fx region_cost_noise(std::uint64_t seed, std::int32_t x, std::int32_t y, const RegionTuning&);   // nz of RG3
Fx region_step_weight(Fx nz, std::int64_t hand, const RegionTuning&); // RG3; nz in [-ONE, ONE], hand in [0, 131070]
RegionSite region_site(geo::Grid, geo::CellIndex, std::int64_t habitability, std::int64_t surface, std::uint64_t seed);
bool region_site_better(const RegionSite& a, const RegionSite& b);   // the site order of 5
std::vector<RegionSite> region_candidates(geo::Grid, std::span<const std::uint8_t> water,
    std::span<const std::uint8_t> habitability, std::span<const std::int64_t> surface, std::uint64_t seed, const RegionTuning&);   // RG4, tile order
std::vector<std::uint8_t> region_border_rivers(geo::Grid, water, river, std::int64_t river_border_class);
bool region_crosses(geo::Grid, std::span<const std::uint8_t> border_river, geo::CellIndex s, geo::CellIndex c);   // RG5 cross
std::vector<geo::CellIndex> region_seeds(std::span<const RegionSite> candidates, geo::Grid, water, river, const RegionTuning&);   // RG5
RegionLandmasses region_landmasses(geo::Grid, water);                 // G2 labels, 8-connected land
std::vector<geo::CellIndex> region_landmass_seeds(geo::Grid, water, habitability, surface, const RegionLandmasses&,
    std::vector<geo::CellIndex> seeds, std::uint64_t seed);           // RG6: the extended list
std::int64_t region_step_cost(geo::Grid, std::span<const Fx> wt, water, border_river, geo::CellIndex c, geo::Direction d, const RegionTuning&);
RegionFlood region_flood(geo::Grid, std::span<const Fx> wt, water, river, std::span<const geo::CellIndex> seeds, const RegionTuning&);   // RG7: owner (u32), cost (i64, 2^62 unreached)
std::vector<std::uint32_t> region_merge(geo::Grid, std::span<const std::uint32_t> owner, water, river,
    std::uint32_t count, std::int64_t min_cells, const RegionTuning&);   // RG8: owners after merging
RegionNumbering region_renumber(std::span<const std::uint32_t> owner);   // RG9: region (u16), old_of_id
std::uint32_t region_sector(std::int64_t dx, std::int64_t dy, std::int64_t r_c);   // 0 central, 1 north, 2 northeast, ... 8 northwest
std::string roman(std::int64_t k);
std::vector<std::string> region_names(std::span<const RegionNameInput> regions, std::span<const std::uint64_t> landmass_cells,
    std::uint64_t land_cells, std::uint64_t sum_x, std::uint64_t sum_y, std::int64_t n, const BiomeDefinitions&, const RegionTuning&);   // RG11
std::vector<db::RegionBorderRow> region_borders(geo::Grid, std::span<const std::uint16_t> region, surface, water, river,
    std::span<const std::uint16_t> walk_area, const RegionTuning&);   // RG12 without pass columns
void region_pass_crossings(geo::Grid, region, water, std::span<const db::PassRow> passes, std::vector<db::RegionBorderRow>& borders);
RegionResult regions_stage(const RegionInputs&, const BiomeDefinitions&, std::span<const db::PassRow> passes,
    const RegionTuning&, std::uint64_t seed, std::uint32_t threads = 1);   // RG1 to RG13; also hand, weight, flood_owner, seeds (10.4)
void run_regions(StageContext&);
```

The rows are `db::RegionRow`, `db::RegionBorderRow`, `db::RegionBiomeRow` (`worldgen/db/writer.hpp`, columns in declared order), written by `StageWriter::write_region`, `write_region_border`, `write_region_biome` in strictly ascending key. The stage reads table `pass` through `StageContext::passes()` (the rows earlier stages of the run wrote, in id order). `regions_stage` takes the map side n of RG11 as the grid width. r2 (implemented 2026-11-18): `RegionNameInput` gains `cells`, `lake_cells`, `walk_cells` (defaults 1, 0, 1: neither Mere nor upland); `RegionNames` gains `upland`, `RegionTuning` gains `upland_own_noun` (keys; `region_names` is fatal, `regions: tuning: upland_own_noun '<key>' is not a loaded biome`, for a key with no loaded definition); `region_pass_crossings` attaches each pass to one border and checks, after every path-chain check, each col in the order given (`pass <id>: col cell is not on its path`, outside the grid included). Order of its fatal checks: threads; the tuning; the region nouns (ascending code); then with the prefix `regions: input precondition: ` the grid, lengths in the order of `RegionInputs`, `water`, `river`, `biome`, the pass paths (`pass <id>: path is not a chain of cells`, in the order given), G7. Through the stage, section 2 item 1 comes first. Word grammar seams: `data::is_valid_word`, `data::word_grammar()` (P0-03 amendment 17). Render seams: `inspect::composite_regions_problem(elevation, water, river, flow_dir, biome, region)`, `inspect::render_composite_regions(elevation, water, river, flow_dir, biome, region, scale, biome_table)`; report: `inspect::read_region_rows`, `read_region_border_rows`, `region_layer_problem`, `region_lines` (`worldgen/inspect/geography.hpp`).

### 10.1 Unit tests

1. Every worked example of 5.1 exactly, through the seams (owners and costs of each flood case; seeds and the bank exception; merge; sectors and names; border rows and pass crossings).
2. RG6: a fixture island with no candidate gets a seed at its best site cell, numbered after the greedy seeds; a landmass of only river and lake cells (hand-made) takes its smallest-index cell.
3. RG9 renumbering on a fixture where the seed numbering differs from the smallest-index order.
4. The tuning: each field one step outside its bounds is a P0-03 range error; `upland_own_noun` naming an unknown biome is a P0-03 reference error; a missing `names.upland` is a missing-field error; `seed_tile_m` above twice `seed_spacing_m` fails `regions_tile_fits_spacing` at the definition's header; a `names` word `east` or `North West` fails the word grammar; a missing `names.isle` is a missing-field error; a biome `region_noun` of `Wet Wood` is fatal at the stage (section 2, check 4).
5. Preconditions of section 2, each with its message: a missing `walk_area`; a `biome` value with no definition; a pass path of 6 bytes; a pass path with a jump of two cells; a pass whose col cell is not on its path (r2).
9. r2: the attachment examples of 5.1 (nearest crossing, the tie to the smaller j, no crossing); the noun rules b and c with their precedence (isle before mere before upland; upland with a dominant biome in `upland_own_noun` keeps its noun); invariant 8.
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

### 10.5 Tests for r3 (names)

**Seams** (interim; the engineer records the final signatures, as for r1 and r2): `region_names` gains the region rows' trait inputs (`coast_cells`, `river_class_max`, `lake_cells`, `elev_mean_dm`, `ore_rich_cells`, `habitability_mean`, `fertility_mean`, and whether a border row with `passes` > 0 touches the region), the `placename` definitions and the seed, and is called after `region_pass_crossings`; new seams `region_traits(row, has_pass, tuning) → bitset` (RG11 5), `region_name_attempt(key, id, t, land, common, traits, tuning) → (valid, name)` (RG11 6, 7; an invalid attempt's name unspecified, as P3-02), `region_name_valid(form, values, common) → bool` (V1 to V6), `region_name_fallback(land, common, used) → name` (RG11 8), and the data reader `placename_data_from(const data::ContentSet&)`. Item 1 of 10.1 no longer tests numerals of names (`roman` keeps its own tests in `worldgen/core`).

**Unit tests** (each against values written in the test, not produced by the code under test):

1. The four draw examples of 5.1 (seed 42, N 1024, regions 1, 10, 39, 40): K_name, every u(k) listed, the form, pool and index chosen, and the names `The Scarred Downs`, `Knelldown`, `Harrowholt`, `Prince's Vow` (the last through the invalid attempt 0, `Birchhurst`).
2. Keepers: a fixture of four regions with one base name and cells 4443, 6898, 14036, 5593 (the `North Downs` case) keeps the base name on the third; equal cells keep it on the smaller id; a base name used once is kept.
3. Each validity rule with a passing and a failing case: V1 (same letter `Birch` + `hurst`; vowels `Bramble` + `alder`; length `Hemlock` + `hollow` (13); `Adder` + `bog` passes, `Thistle` + `horn` (11) and `Bracken` + `stone` (12) pass), V2 (a fixture prefix `Brrr` + `holt`; V1 already rules out a triple across a join, so V2 guards the words themselves and the literals of forms), V3 (`The Hollow Hollow`, `Fen` + `moss` + `Moss`, substring case `The Wood of Woods`), V4 (a fixture `forbidden` entry inside a compound, and inside a word with an apostrophe), V5 (whole name `The Haunted Forest`; one-word entry `Blackrock` rejecting `Blackrock Downs`; a two-word entry rejecting only the whole name), V6 (49 bytes fails, 48 passes).
4. Traits: for each of the eight, a row one step either side of its threshold (including `lake_cells` 0 with `lake_permille` 1, `river_class` 16, `barren_hab_max` −1 and `fertile_min` 256 meaning never); the trait pool is the concatenation in trait order, not id order (fixture traits whose ids sort differently from the enumeration).
5. Pools: an empty pool gets weight 0 (a land with no `adjectives` draws only from the common pool for `{adjective}`); `weighted` boundaries on hand-made u values (the high word exactly at a prefix-sum boundary goes to the next pool, P3-02 4.1).
6. Collision and fallback: with a fixture land whose forms can make only three names, a fourth region of that land falls back to the first free compound in (P, S) order; with every compound also taken, the stage is fatal with `regions: no free name for region <id>`; a drawn name equal to a keeper's base name is rejected (fixture data that can make `East Wood`).
7. The loader (P0-03 forms of each message): each grammar with one failing string (`Wood land`, `Holt` as an element, `The Crows` as an object, `{noun} {noun}` and `{nouns}` as forms, a double space in an excluded name); each check of 4.1 with one failing definition, in the stated order (a land with both biomes and a special; a trait with nouns; a common with forms; a word twice in `prefixes`; two common definitions; `fen` in two lands; `special` `isle` twice; a common with 99 prefixes); `placename` missing from a data root is a required-kind error; the new tuning records: each field one step outside its bounds, a `name_pools` array of 2 or 4 weights.
8. Stage check 4a, each message: a fixture biome with no land definition; no `upland` land; a land form whose `{feature}` slot has no words when `name_pool_weights.features` = [0, 1, 1] and the land has no features.
9. Fallback order on the shipped `fen` land with an empty U: `Adderbog`.

**Seams as implemented (r3, generator engineer, P3-08, 2026-11-20;** `worldgen/include/worldgen/stages/regions.hpp`**):** `PlacenameData placename_data_from(const data::ContentSet&)` (definitions `PlacenameDef` in ascending id: `role` `PlaceRole`, `biomes`, `special` `PlaceSpecial`, `trait` index into `REGION_TRAIT_NAMES` or −1, `forms` (`PlaceForm{form, weight}`), `lists[8]` in the order of `PLACE_LIST_FIELDS`, `forbidden`, `excluded`); `check_placename_data(data, biome definitions, tuning)` (check 4a; slot text with braces, form index from 0, plus `regions: placename: there must be exactly one common definition` for seams given no common); `RegionTraits region_traits(const RegionNameInput&, const RegionTuning&)` (`std::array<bool, 8>`, trait order); `region_fill_form(form, values)`; `bool region_name_valid(form, PlaceSlotValues values, const PlacenameDef& common)` (values: (list index, word) per slot, left to right); `RegionNameAttempt region_name_attempt(k_name, region id, attempt, land, common, std::span<const PlacenameDef* const> traits, tuning)`; `std::optional<std::string> region_name_fallback(land, common, used)` (nullopt when none is free); `region_base_names(...)` (RG11 1 to 4, the r2 seam without numerals); `region_name_choices(...)` and `region_names(...)` (the r2 parameters plus `const PlacenameData&` and the seed; `RegionNameChoice{name, keeper, fallback, attempt}`); `regions_stage(inputs, definitions, placenames, passes, tuning, seed, threads)`, whose `RegionResult` gains `names` (the choices, id order). `RegionNameInput` gains `coast_cells`, `river_class_max`, `elev_mean_dm`, `ore_rich_cells`, `habitability_mean`, `fertility_mean`, `has_pass`; `RegionTuning` gains `name_pool_weights` (`[list][pool]`) and `name_traits` (`RegionNameTraits`); range failures are reported as `regions: tuning: name_pool_weights.<list> outside its schema range` and `name_traits.<field>`.

**Property tests** (10.2's worlds, seeds 0 to 63 at N = 128, both fixture data roots, with the shipped `placename` copies): invariant 6 recomputed by independent code: names unique; every base name in use held by exactly one region, the largest; every other name parses as one of its land's forms with every slot value from one of the three pools of that region; V1 to V6 hold; no Roman numeral token. Also: the names are the same with threads 1, 2 and 5; renaming a `placename` file, or moving a definition between files, changes no name (P0-03 7.4); reordering the definitions of `traits.toml` changes no name.

**Data tests**: the shipped `game/data/placenames/` loads; `common` meets `placename_common_sizes`; every list sorted (a convention test for editors, `the …` objects after the others); the fixture copies are byte-identical to the shipped files; the test writer's reference reproduces the space sizes of 14.3 (distinct valid names per land without traits) as a check of the reference, not of the C++.

**Golden values.** `p1-06-regions.txt`: the logical hash of table `region` changes for every case in which a base name repeats (all four shipped-data cases: 512 seed 0 draws 1 name, 512 seed 42 3, 512 "The Unmapped" 1, 1024 seed 1 35); the layer hash of `region`, the hashes of `region_border` and `region_biome`, hand, wt, owners and seeds are **unchanged**, and a difference there is a defect. Add per case one line `<case> names <id>:<name>|…` (every region, id order) and `<case> name_attempts <id>:<t or F or K>|…` (K keeper, F fallback), so a difference is legible. The reference implements RG11 r3 independently (it may use `core_reference.hash_at3` and `substream`).

**Version and what else changes.** `GENERATOR_VERSION`: the next minor version after the one on `main` when this merges (0.17.0 if it follows P3-06 at 0.16.0, as the proposals entry plans). Every world file changes: the `content_hash` (a new required kind and two new `worldgen.regions` fields), so `p0-worlds.txt`, `p1-worlds.txt` and `p3-worlds.txt` are re-recorded after the cross-preset comparison, and `p3-01-data.txt` (content hashes of the three data roots) is re-recorded. P1-01 to P1-05 goldens: unchanged. History goldens: tables keyed by ids do not change, and civilization, settlement, god and age names change only where a new region name equals a name history would have drawn (none on the golden worlds: checked by the prototype on all twelve P3 test input worlds of `~/.cache/the-unmapped/out/p3-06-tests/inputs/`, and P3-03 14.3 item 21 still holds there); the files that store rendered sentences with region names change: `p3-04-engine.txt` (the `e<n>` sentence lines, such as `Kruskux awakened the Goblins in West Wood II.`), `p3-05-peoples.txt` (on its branch), and any P3-06 or P3-07 golden that stores report text. `p3-02-names.txt` (hand-written preload lists) and `p3-03-chronology.txt` are unchanged. The `world-hashes.sh` cases are unchanged.

## 11. Calibration and known limitations

### 11.1 How it was calibrated

A prototype of this spec (`~/.cache/the-unmapped/out/p1-06-spec/tools/`: `regions.py` seeds, merge and renumbering; `flood.c` the exact integer flood, G2 labelling and the H7 chamfer; `table.py` the table rows and report keys; noise and `hash_at` from the P1-05 prototype, checked there against `core_reference`) ran on the stored 0.10.0 layers of the P1-05 validation sweep (`~/.cache/the-unmapped/out/p1-05-validation/`; 512 seed 0 regenerated on `a18d21c` has the same file hash). Per-world results: `~/.cache/the-unmapped/out/p1-06-spec/sweep/<variant>-<size>.jsonl`. Contact sheets (biome composite, region borders black, seeds red with their names, rivers of class ≥ 6 drawn strong blue), in `~/.cache/the-unmapped/out/p1-06-spec/sheets/`:

- `p1-06-1024-featured.png`: 1024 "The Unmapped", seeds 133, 75, 185, 0 and 1, full resolution with names;
- `p1-06-512-all.png` (seeds 0–19, with names), `p1-06-1024-all.png` (seeds 0–29), `p1-06-2048-all.png` (seeds 0–5) and `p1-06-2048-0.png` (full resolution with names);
- `p1-06-draft-vs-final-1024-0.png`: the central range of 1024 seed 0 on the shaded relief, first draft (left) against this version (right), borders red, seeds yellow.

Measures, besides the report keys: **crest share**, of the border pairs where the relief across the border is at least 40 m within 12 cells on either side, the share whose pair is higher than both cells 12 cells (192 m) out on either side, across the border (**trough share**: lower than both); **large-river share**, the share of river cells of class ≥ 6 (≥ 7) that are on or next to a border; the isoperimetric quotient of regions above 0.5 km² (`iq`); `chained`, the share of seeds whose nearest river cell lies downstream of another seed's within 4 km (critic: "do not let region seeds line up along one creek").

### 11.2 Variants (medians over worlds; 512 seeds 0–19, 1024 seeds 0–29, 2048 seeds 0–5)

| Variant | Size | `regions` | area p50 (interpolated median) / p100 km² | crest / trough | large-river share (≥ 6 / ≥ 7) | iq |
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
5. **Names (r3).** Before r3 names repeated with numerals (forest worlds up to `Northwest Wood VII`, 2048 worlds 83–85% numbered). From r3 the largest region of each base name keeps it (45% of regions at 1024, 16% at 2048, 88% at 512) and the others draw names. Drawn names are built from the elements of English place names, so some coincide with obscure real places (`Crowhurst`, `Blackmoor`, `Silverdale`); famous real places and names from well-known fiction and games that the lists can make are excluded (14.3), but no list can be complete. The kept names `North Downs` and `South Downs` are real English ranges (unchanged since r1).
6. **No sea regions** (question 2).
7. **2048 statistics rest on six worlds.**

## 12. Open questions and decisions of taste (for the lead and Paul)

1. **Placeholder names** "<sector> <biome noun> [numeral]". **Decided by Paul 2026-11-19**: names with identity, no numerals (r3, section 14); the questions r3 raises are in 14.5. History may still add cultural names per civilization in its own table later (P3-02 4.5 reserves the kind `region`).
2. **No sea regions; lakes belong to land regions** (design). Recommendation: approve for P1; sea areas for naval events are history's.
3. **Region size and count**: seeds 1200 m apart, merge below 0.75 km²; median 2.0–2.1 km², 13 / 49 / 200 regions. Recommendation: approve (the design's "about 2 km²"); `seed_spacing_m` 1250 would give 12 / 45 / 185 at about 2.3 km².
4. **Borders by height above drainage** instead of climbing cost (`hand_ref_m` 25 m). Recommendation: approve (11.2 finding 1).
5. **Large river = class ≥ 6** (≥ 4 km² upstream), 2 km crossing cost, and seeds on opposite banks both stand. Recommendation: approve; class 5 makes many streams borders, class 7 leaves most large rivers inside regions.
6. **Upland regions of their own where a massif is wider than the spacing** (11.3). Recommendation: approve.
7. **The new table `region_biome`** (not in the design's schema plan). Recommendation: approve; it is small (a few rows per region) and it is what beast placement and race preference need.
8. **The `region` summary columns** (3.2): means of all six scores, `habitability_p90`, `ore_rich_cells` and `ore_reach_cells` (300 m), walk and coast counts. Recommendation: approve; anything finer stays in the layers.
9. **Passes linked to borders** through `pass_id` (smallest) and `passes` on `region_border`, not a link table; from r2 each pass on the one border its path crosses nearest the col. Approved (r2 per the critic's F1).
10. **Islets of any size are regions** (11.4 item 4). Recommendation: approve; the alternative (no region) breaks "every land cell is in a region".

## Appendix A. Schema and catalogue impact (for the lead's proposals entry)

**r3 (names)**: no schema change (the `name` column keeps its type; values change); a new data kind `placename` (4.1); two new fields of `worldgen.regions`; a new stream `geo.regions.name` (P1-00 section 3 and `streams.hpp` gain it); a generator version bump. `docs/specs/schema.md` needs no change (it refers to section 3 for the columns' meaning).

**Schema version 5** (generator 0.11.0): tables `region`, `region_border`, `region_biome` (DDL in 3.1), executed after `climate`; the empty world grows from 8 to 11 pages. `docs/specs/schema.md` is updated on this branch (DDL, `meta`, catalogue, changelog). Needed with the merge: P0-02 amendment 17 (DDL, version 5, 11 pages, the refusal message for version 4) and P0-04 amendment (the three tables in `[tables]` and `hash --detail`, the `[geography]` region keys, the composite `regions`). Layer catalogue: `region` unchanged (`u16`, 0 on ocean only, id 19). Data: new `worldgen` id `regions` (`game/data/worldgen/regions.toml`); no biome file changes. Streams `geo.regions.seed`, `geo.regions.cost` as P1-00 3.

**Proposals entry for the lead** (update of "P1 schema changes", P1-00 B8):

- Design doc section: 4 ("Regions"), 6 (table list, schema versions)
- Proposal: schema version 5, with the regions stage, adds `region` (id, name, dominant biome, bounds as four integer columns, seed cell, landmass, cell counts of lake, river, coast and walkable ground, largest river class, main walk area, mean and highest elevation, the means of the six resource scores, the 90th percentile of habitability, and counts of rich ore and of rich ore within 300 m of walkable ground), `region_border` (one row per pair of neighbouring regions: border length, the height and cell of the lowest crossing, walkable and large-river pair counts, largest river class, and the smallest id and number of natural passes crossing it) and `region_biome` (cells of each biome in each region, new: beasts and race preferences need biome shares, not only the dominant biome).

**Further proposals entries** (text for the lead; the design is not wrong, but these settle what it leaves open):

1. *Regions: borders by height above drainage; large rivers are rivers of more than 4 km².* Design section 4 says regions are "bordered on ridges and large rivers". Proposal: state that region borders come from a travel cost that is cheap along valleys and dear over ridges (cost weighted by height above the nearest stream), and that a "large river" drains more than about 4 km² (river class 6), whose two banks belong to different regions where both have a settlement site.
2. *Upland regions.* Design section 4 (Regions). Proposal: add "Mountain massifs wider than about a kilometre form upland regions of their own; narrower ranges are divided along their crest between the regions on either side."
3. *Region counts.* Design section 4 says "roughly 12, 45 and 180 per map". Measured with 2 km² regions on 0.10.0 worlds: 13, 49 and 200 (land is about 40% of the map at 1024 and 42% at 2048). Proposal: "about 2 km² each, roughly 13, 50 and 200 per map", or keep the counts and accept 2.3 km² regions (seed spacing 1250 m); Paul's choice (12, question 3).

## 13. Revision r2 (2026-11-18)

After the world critic's review of the implementation (F1, F2; F4 and F6 as notes for history). The layer `region`, the schema and every column's type stay; only RG12's pass attachment and RG11's noun change, so `region_border.pass_id`, `passes` and `region.name` change bytes. Generator stays **0.11.0** (nothing is merged yet).

1. **RG12 (F1).** A pass is attached to at most one border: the crossing of its path nearest its col in path steps, ties to the smaller step index; none if the path crosses no border. New precondition: the col cell lies on the path. Measured (prototype, same worlds as 11): rows with `pass_id` > 0 fall from 17 / 65 / 22 to 13 / 41 / 13 (512 seeds 0–19 / 1024 seeds 0–29 / 2048 seeds 0–5; every pass that crosses a border keeps one row). The attached crossing lies a median 3 / 8 / 10 path steps from the col (at most 24 / 40 / 36 steps, 0.4–0.6 km), its height max(S) a median 8 / 35 / 68 m below the col's elevation (at most 192 / 305 / 289 m); before, crossings 2–3 km from the col and at 0.1 m height were flagged.
2. **RG11 (F2).** Two noun rules: `Mere` for regions at least half lake; the new word `names.upland` (`Fells`) for upland regions (the existing definition, 4 × `walk_cells` < `cells`) whose dominant biome is not in the new field `upland_own_noun` (alpine, heath, highland moor keep `Peaks`, `Heath`, `Moor`). The critic's wider test (under half walkable and ≥ 600 m of relief) needs the lowest elevation, which no column holds; the existing definition catches the critic's worst cases (phrase "North Downs" at 14% walkable, 1024 s0 "East Downs III" at 19%). Measured: names changed per world 0 / 2.5 / 3 (median; at most 3 / 12 / 33, renumbering included); `Fells` 0–3 / 0–4 / 0–3 regions; `Mere` 0 / 0 / 0–2; the highest numeral 3 / 9 / 29 (before: 3 / 9 / 30). Repetition at 2048 (83–85% numbered) is unchanged; ring words or 16 sectors were not taken.
3. **History notes (F1, F4, F6)** in 3.3 and 3.4: passes located by the col; river borders judged by `river_pairs` × 16 / `length_m`; islets under about 0.3 km² of dry land.
4. Data: `regions.toml` gains `names.upland = "Fells"` and `upland_own_noun`; the content hash changes, so `p0-worlds.txt` and `p1-worlds.txt` are recorded with r2 data.
5. Worked examples (RG11 names, RG12 attachment), report example (1024 seed 0 `region_borders_pass` 2), the `region_borders_pass` statistics, invariant 8, tests 10.1 items 4, 5 and 9.

## 14. Revision r3 (2026-11-19): region names with identity

Paul's decision of 2026-11-19 (`docs/design/proposals.md`, "region names get identity; no numbered names"): "'East Wood' is fine but 'West Wood III' is pretty boring". Only the `name` column of table `region` changes; the layer, the other tables and every other column are as in r2. **No schema change**; a new required data kind, two new tuning fields and a new stream, so every world file changes and the generator version is bumped (10.5). The proposals entry says this is implemented after P3-06 is integrated, before Paul's review of P3.

### 14.1 What changed

1. **RG11** (rewritten): base name as before; the largest region of each base name keeps it; every other region draws a name from its land's forms and words, with pools common, land and traits, keyed draws `hash_at3(K_name, id, attempt, k)`, validity rules V1 to V6, at most 64 attempts, an exact fallback; no numerals. RG11 is evaluated after RG12 (the `pass` trait).
2. **Section 4.1**: the data kind `placename` (`game/data/placenames/`, 15 files: `common`, 13 lands, `traits`), its grammars, checks and canonical form; the files are committed with this revision and are normative.
3. **Section 4**: `worldgen.regions` gains `name_pool_weights` (record `name_pools`) and `name_traits` (record `name_traits`).
4. **Section 2**: the input row, the stream `geo.regions.name` (keyed with N), check 4a.
5. **14.5 to 14.7, Appendix B** (after Paul's answers): questions marked answered; the rule for `The …` names inside rendered sentences, with patch text for P3-04 8.5 and P3-07.
6. **Invariant 6**, the determinism table, 3.2 (`name`), 5.1 (RG11 examples), 10.5 (tests and goldens), 11.4 item 5, 12 question 1, Appendix A.

### 14.2 How it was measured

A prototype of RG11 r3 in exact integers, `~/.cache/the-unmapped/out/p1-06-names/proto.py` (Python, on `worldgen/tests/reference/core_reference.py` for `stream_key`, `substream` and `hash_at3`; reads the shipped data with `tomllib` and table `region` and `region_border` of a world file with `sqlite3`; the sector and noun are taken from the stored name with its numeral stripped, which is exactly RG11 1 to 3). It is the spec's algorithm, so the worked examples of 5.1 are exact; the test writer's reference must reproduce them independently. Worlds (region tables written by 0.11.0 to 0.16.0, which r3 does not change): 512 seeds 0, 42, 1 to 4, 7 and the phrase "The Unmapped"; 1024 seeds 0, 1, 2, 3, 4, 11, 42, 75, 133, 185 and the phrase; 2048 seeds 0, 1, 2, 3 and 42 (24 worlds; `sweep.py`). Also the data checks of `proto.py --audit` (duplicates, grammar, sort order, space sizes, samples), `dict_audit.py` (compounds that are dictionary words, against `/usr/share/dict`) and `known_audit.py` (which well-known real and fictional names the lists can make).

| Size | Regions (worlds) | Keep their base name | Drawn | Attempts / invalid / taken | Fallback | Highest accepted attempt |
| --- | --- | --- | --- | --- | --- | --- |
| 512 | 106 (8) | 93 (88%) | 13 | 13 / 0 / 0 | 0 | 0 |
| 1024 | 569 (11) | 254 (45%) | 315 | 328 / 13 / 0 | 0 | 1 |
| 2048 | 995 (5) | 156 (16%) | 839 | 867 / 20 / 8 | 0 | 2 |

Names are unique in every world. **Repeats between worlds**: two different 2048 worlds share 0 to 5 drawn names of about 165 each (ten pairs); different 1024 worlds share 0 or 1. Before the map size went into the key, 1024 seed 0 and 2048 seed 0 shared 20 drawn names (same region ids, same draws); with it, none. **History names**: on the twelve P3 test input worlds (`~/.cache/the-unmapped/out/p3-06-tests/inputs/`: the four P1 golden cases, the `hist-*` cases, `example-42-1024` and four sweep worlds), no new region name equals any god, age, civilization or settlement name of the world, so the history tables of those worlds would not change.

### 14.3 The lists

Sizes in 4.1. What each land can make, enumerated over its six forms and all its pools (`proto.py --audit`; a name counted once however it is made):

| Land | Regions it names | Distinct valid names, no trait | of them one word (the fallback space) | With every trait |

(r4: the counts are those of the r4 lists, with `boner` and `dung` forbidden: each land loses 20 to 56 names, at most 2 one-word names; r3's counts were 33,174 to 70,880 without traits. Measured with the prototype of 14.2, `proto.py --audit`, 2026-11-20.)

| --- | --- | --- | --- | --- |
| `wood` | temperate forest (`Wood`) | 70,835 | 1,331 | 127,169 |
| `carr` | wet woodland (`Carr`) | 53,253 | 976 | 92,752 |
| `downs` | grassland (`Downs`) | 49,198 | 914 | 88,569 |
| `marsh` | marsh (`Marsh`) | 41,426 | 800 | 71,961 |
| `fen` | fen (`Fen`) | 51,412 | 984 | 90,727 |
| `moor` | highland moor (`Moor`) | 37,881 | 877 | 68,932 |
| `heath` | heath (`Heath`) | 33,154 | 783 | 60,025 |
| `peaks` | alpine (`Peaks`) | 60,570 | 1,282 | 107,053 |
| `waste` | blighted waste (`Waste`) | 38,255 | 636 | 68,985 |
| `steppe` | steppe (`Steppe`) | 38,216 | 624 | 69,428 |
| `isle` | isles (`Isle`) | 33,346 | 629 | 58,338 |
| `mere` | half lake (`Mere`) | 43,838 | 761 | 76,437 |
| `fells` | other upland (`Fells`) | 48,899 | 1,064 | 86,410 |

So each land offers 33,000 to 71,000 distinct names (r4: 33,154 to 70,835) to a region without traits and 58,000 to 127,000 to one with every trait; the largest number of drawn regions of one land in one 2048 world is 115 (wood). The pools spread the draws: the most frequent single name of a land takes 0.16% (wood) to 0.42% (isle) of first attempts.

**Thirty samples per land** (first valid attempts of distinct region ids for a land without traits, `proto.py --audit --samples 30`, in draw order; traits add their words, see 14.4):

- **wood**: The Starved Weald, Chillwood, Wangill Grove, The Sable Forest, Wightwood, The Woods of Antlers, Mutehurst, Dimstone Holt, Barkbrook Grove, Cairnhurst, The Yellow Holt, Hollowbourne Wood, Flintholt, Brigand's Brake, Forester's Dole, Stillhurst, Hermit's Woods, Boughshaw, Pedlar's Bend, Brigand's Hurst, The Thorned Woods, Hazelgill Holt, Illhurst, Brockholt, Hazelwood, The Overgrown Tangle, Crow's Wood, The Forest of Crows, The Thicket of Boars, Maiden's Bane.
- **carr**: Cindershaw, The Drowsing Wood, The Unlit Holt, The Withies of Candles, Gibbetmark Thicket, Rushmire Osiers, Woemire Carr, Stagmark Withies, Reedcarr, The Holt of Alders, Chieftain's Sallows, Willowpool Withies, The Wood of Herons, Archer's Pit, The Osiers of Alders, Heron's Carrs, The Osiers of Cinders, Charnelgate Carrs, Alderwood, Reedlaw Thicket, Frostholt, Gloamcarr, The Osiers of Silence, The Carrs of Gibbets, The Carr of Wyrms, The Sallows of Silence, Siltbeck Thicket, Fetchcombe Carr, Wyrmholt, Houndshaw.
- **downs**: Blackhowe Wolds, Ferryman's Cairn, Teaseldown, Chalkgate Barrows, Mason's Sward, Blacklea, The Downs of the Harvest, The Vale of Traitors, Pyredown, Wanderer's Portion, Corbiecombe, Dwalelea, Swineherd's Toll, Hawbeck Barrows, Harepike Sward, Vetchwold, Witherdown, The Vale of Candles, Gravedigger's Leas, Dreadbarrow, The Leas of Candles, The Bare Leas, The Barrows of Nettles, Stagcombe, Ivycombe, Umbercombe, Warden's Barrows, The Barren Wolds, Lichwold, Darnelstone Leas.
- **marsh**: Redhollow Flats, Dreadfleet, The Unhallowed Marshes, The Rank Slough, The Reeds of Moths, Ferryman's Marsh, Mothfleet, The Reeds of the Forsaken, Thane's Marshes, The Reeds of Ashes, Dwalefleet, Sloegate Reeds, The Reeds of the Drowned, Roodslough, The Marshes of Outlaws, Mutewash, Aguemarsh, Rushfleet, Rookslough, Wanwash, Brackhowe Reeds, The Sodden Reeds, The Unquiet Marsh, The Faded Marshes, Shroudslough, Squire's Barrow, The Sunken Marshes, Lornslough, Wyrmfleet, Bitternwell Slough.
- **fen**: Collier's Mark, The Mire of Worms, Squire's Mire, Banefen, Rushpool Fen, Leechlaw Morass, Hawkquag, Ivymire, Feverquag, Lornscar Quag, Leechsike, Palemire, The Fens of the Fallen, Sedgefen, Crooksike, The Drowned Mire, Hawslack Morass, The Murky Mire, Ashbog, Woodcutter's Fen, Cinderbog, Shrikepool Quag, Wyrmbeck Quag, Kitemire, The Raw Quag, Blackbog, Warlord's Hollow, The Sombre Mosses, Reavemoss, Wraithquag.
- **moor**: The Wild Tops, Shrikerigg, The Heights of Crows, Knellbank Tops, Miller's Edge, Wrenknowe, Houndedge Moor, Boneknowe, Shepherd's Tree, Grouserigg, The Ashen Rigg, Bearmoor, Ploverlaw, The Heights of the Hanged, Sablecairn Edge, The Silent Heights, Misthowe, The Ill Rigg, The Moor of Curses, Mournspring Heights, Lornspring Moor, Curlewknowe, The Edge of Peat, The Long Edge, The Pitiless Heights, Curlewridge Rigg, Curlewedge, Lingthorn Heights, The Wild Moors, The Fading Moors.
- **heath**: Gorselaw Heaths, The Bare Heath, Burrslack Brakes, Balegate Furze, Keenfurze, The Furze of Mists, The Scorched Whins, Gloamheath, Usurper's Whins, Umberbarrow Heath, Brackenstone Heath, Hallowheath, Palefurze, The Whins of Adders, Blackwhin, Spinner's Furze, The Furze of Nightjars, Butcher's Heath, Gorsebeck Whins, Prince's Furze, Wakefurze, Broomling, Raven's Debt, Heatherling, The Fey Heath, Hagbrake, The Desolate Heath, The Bare Whins, Reaver's Heaths, Dirgewhin.
- **peaks**: Eaglehorn, Tarescar, Cullcrag, Weeptooth, The Sallow Needles, Emberhollow Crags, Hawmark Teeth, The Unlit Crags, Keenfang, The Cloven Pinnacles, Hawcombe Teeth, The Cloven Needles, Nettlespire, The Peaks of the Blind, The Frowning Heights, Banebank Spires, Usurper's Oath, Sloegate Pikes, The Fangs of Kings, Umberhorn, King's Needles, The Peaks of Saints, Gloampeak, The Pale Teeth, The Peaks of Gallows, The Doleful Crags, The Hidden Pikes, The Fading Needles, The Joyless Pikes, Cloudtooth.
- **waste**: Reavewaste, Gravedigger's Wastes, Slagwaste, Dimwaste, Owl's Knoll, The Murmuring Waste, Ferryman's Scald, Widow's Scald, The Barrens of Barrows, Nightbeck Wastes, Bitterscar, The Scorched Waste, Pilgrim's Scald, The Barrens of the Unburied, Brockscald, Widow's Wilds, Lynxlaw Scald, Gloomblight, Slagscar, Carrionwaste, Fowler's Blight, The Cankered Scald, The Barrens of Plagues, Frostbank Scald, Bride's Sorrow, Pitchmark Waste, Wraithscald, Reekwaste, The Barren Blight, The Russet Wastes.
- **steppe**: The Dry Sweep, Redsweep, Horsegrass, Adder's Wilds, The Hushed Wilds, Widower's Plains, Ivyreach, The Plains of Worms, Thirstreach, Brockreach, The Sighing Steppe, Soursweep, The Steppe of Prayers, Dunmark, Ferryman's Steppe, The Steppe of Horses, Ravengrass, The Reaches of Bones, Dolesweep, Spinner's Howe, The Steppe of Saints, The Pale Reaches, The Shrouded Plains, The Forlorn Steppes, Reekdale Wilds, Dirgemark, Rimemark Steppe, The Plains of Mourners, The Cheerless Plains, Cairnsweep.
- **isle**: The Drowned Isle, Gullskerry, The Isle of Sorrows, The Sunken Rock, Usurper's Isle, Hushknoll Skerry, Orphan's Hollow, Fletcher's Rock, Kelpgill Isle, Gullstack, Reeve's Rock, The Rock of Tides, The Holm of Gulls, Reaver's Holm, The Salt Stack, Brineholm, Anchorite's Oath, The Skerry of Saints, Wrackside Isle, Dwalelaw Rock, The Stack of Echoes, Goatherd's Isle, Skuarock, The Dying Skerry, The Skerry of the Damned, Nightbourne Stack, The Veiled Rock, The Stack of Kites, The Worn Isle, Stoatlaw Stack.
- **mere**: Silvermere, Bowyer's Mere, Gloomwater, Palemere, Eelmark Water, Swartknoll Meres, Mirrortarn, Houndlake, The Worn Meres, Nighthorn Lake, Wayfarer's Drop, Prophet's Meres, Tansywater, Dirgebrook Meres, Horseman's Water, Banemark Mere, Spinner's Water, Rowangill Water, Wolftarn, Pitchedge Lake, The Deep Pool, Shroudfall Tarn, Crooktree Water, Fletcher's Meres, Gibbetcombe Tarn, The Drowned Water, Burrmere, Dwalelake, Carter's Meres, Saint's Mere.
- **fells**: The High Fells, Adderedge, Hartcrag, The Sable Uplands, The Drear Uplands, Frostedge, Pallrigg, The Godless Fells, The Fell of the Moon, The Steep Fell, Brandslack Fells, Prince's Heights, Hushcrag, Thief's Fell, Windfell, Slaterigg, Prince's Edges, The Cloven Uplands, The Ruined Fell, Stagbank Brows, The Stunted Scars, The Scars of the Moon, Hushrigg, Shaleknott, Screerigg Fell, Sorrowridge Crags, Murkdale Edges, The High Brows, Frostbeck Uplands, Witherknott.

**How bad combinations are avoided.** By rule (RG11 7): V1 joins (no `Ashhurst`, `Hollowwood`, `Bramblealder`; compounds of at most 12 letters, so no `Hemlockhollow`); V2 no triple letters; V3 no element inside another (`The Hollow Hollow`, `Fenmoss Moss`, `The Wood of Woods`); V4 the 21 forbidden substrings (r4: 19 in r3, plus `boner` and `dung`, 14.8); V5 the excluded names; V6 at most 48 bytes. By list discipline: common lists hold no land noun or land suffix (`Barrow`, `Fang`, `Horn`, `Scald`, `Scar` were moved out of the common prefixes), no compass word (so no drawn name can read as a sector name), no comic or modern words. By audit: every compound the lists can make was checked against the English word lists of `/usr/share/dict` (abstract nouns such as `Blackness` came from the coast suffix `ness`, which was removed; modern words such as `Foghorn`, `Smokestack`, `Floodgate`, `Blindside` were excluded) and against a hand list of about 250 well-known real and fictional place names (Tolkien, Forgotten Realms, Greyhawk, Warcraft, Elder Scrolls, Dark Souls, Elden Ring, Game of Thrones, Diablo, Path of Exile and English towns and landscapes). Excluded as whole names or words, among others: `Blackwood`, `Blackrock`, `Blackreach`, `Blackmarsh`, `Murkmire`, `Mistwood`, `Ravenholm`, `Ravenscar`, `Coldfell`, `Redhorn`, `Starkhorn`, `Mirrormere`, `Hoarwell`, `Harrowdale`, `Hornwood`, `Ironwood`, `Wychwood`, `Hollywood`, `Blackpool`, `Ashford`, `Lichfield`, `Stillwater`, `The Withered Heath`, `The Burning Steppes`, `The Dread Wastes`, `The Cold Plains`, `The Black Marsh`, `The Wild Wood`, `The High Moor`, `The Fallow Mire`. Paul's own examples are kept: `Blackwater Fen`, `The Ashen Downs`, `Thornwood`-style compounds, `Harrowmere`, `Widow's Reach` and `The Vale of Crows` can all be drawn.

### 14.4 Before and after

**1024, seed 42** (the world of the history examples; every region):

| id | old name | new name | land | traits | how |
| --- | --- | --- | --- | --- | --- |
| 1 | North Downs | The Scarred Downs | downs | coast | t0 |
| 2 | North Downs II | The Sour Plain | downs | coast | t0 |
| 3 | Northwest Downs | The Rises of Hares | downs |  | t0 |
| 4 | Northwest Downs II | Warlord's Rises | downs | fertile | t0 |
| 5 | Northwest Steppe | Northwest Steppe | steppe | coast, high, ore, barren | plain |
| 6 | North Downs III | North Downs | downs | river, fertile | plain |
| 7 | Northeast Downs | The Windswept Wolds | downs | fertile | t0 |
| 8 | Northwest Downs III | Northwest Downs | downs | ore | plain |
| 9 | Northwest Heath | Northwest Heath | heath | high, barren | plain |
| 10 | Northeast Downs II | Knelldown | downs | river, fertile | t0 |
| 11 | Northeast Downs III | Beggar's Plain | downs | fertile | t0 |
| 12 | Northeast Downs IV | Witchgate Barrows | downs | river, fertile | t0 |
| 13 | North Heath | North Heath | heath | high, barren | plain |
| 14 | Northeast Wood | Northeast Wood | wood | river | plain |
| 15 | Northwest Peaks | Northwest Peaks | peaks | high, ore, barren | plain |
| 16 | North Downs IV | Steward's Vale | downs | river | t0 |
| 17 | East Wood | Wrecker's Reach | wood | coast | t0 |
| 18 | Northeast Downs V | Northeast Downs | downs | river, fertile | plain |
| 19 | West Wood | The Shaw of Widows | wood |  | t0 |
| 20 | West Peaks | West Peaks | peaks | lake, high | plain |
| 21 | Central Heath | Central Heath | heath | high, ore, barren | plain |
| 22 | West Wood II | West Wood | wood |  | plain |
| 23 | Central Wood | Central Wood | wood | lake, high, ore | plain |
| 24 | East Wood II | Drover's Sorrow | wood | river | t0 |
| 25 | Central Downs | Central Downs | downs |  | plain |
| 26 | East Wood III | East Wood | wood | river | plain |
| 27 | West Wood III | Sprayridge Thicket | wood | coast, river | t0 |
| 28 | East Downs | East Downs | downs | river | plain |
| 29 | East Wood IV | Dawthorn Woods | wood | ore, barren | t0 |
| 30 | West Downs | Vetchdown | downs | river, lake, fertile | t0 |
| 31 | West Downs II | West Downs | downs | lake, fertile | plain |
| 32 | South Downs | South Downs | downs | high, ore | plain |
| 33 | East Wood V | The Sorrowing Shaw | wood |  | t0 |
| 34 | Southeast Wood | Southeast Wood | wood |  | plain |
| 35 | Southwest Wood | The Wood of Stags | wood |  | t0 |
| 36 | Southwest Steppe | Southwest Steppe | steppe |  | plain |
| 37 | Southeast Wood II | The Brake of the Forgotten | wood |  | t0 |
| 38 | Southwest Downs | Southwest Downs | downs | high | plain |
| 39 | Southeast Wood III | Harrowholt | wood | coast | t0 |
| 40 | Southeast Wood IV | Prince's Vow | wood |  | t1 |
| 41 | Southwest Downs II | Larkfell | downs | high, ore, pass, barren | t0 |
| 42 | South Wood | Stranger's Oath | wood |  | t0 |
| 43 | Southwest Wood II | Southwest Wood | wood | coast | plain |
| 44 | South Wood II | South Wood | wood | high, pass | plain |
| 45 | Southeast Wood V | Badgerhorn Wood | wood |  | t0 |
| 46 | South Wood III | The Tangle of Yews | wood |  | t0 |
| 47 | Southwest Wood III | Thistlehorn Shaw | wood |  | t0 |
| 48 | South Fells | South Fells | fells | high, barren | plain |
| 49 | South Wood IV | The Gnarled Forest | wood | coast, barren | t0 |
| 50 | Southeast Isle | Southeast Isle | isle | coast | plain |

`how`: `plain` the keeper of its base name, `t<n>` the accepted attempt. 24 regions keep their base name, 26 draw (`East Wood` and `West Wood` are now one each; `Northeast Downs V`, the largest of five, is `Northeast Downs`).

**2048, seed 42** (195 regions, 32 keep their base name, 163 drawn; attempts 170, 4 invalid, 3 taken, no fallback). The first 40 rows; the full table is `~/.cache/the-unmapped/out/p1-06-names/s42-2048.md`:

| id | old name | new name | land | traits | how |
| --- | --- | --- | --- | --- | --- |
| 1 | North Wood | North Wood | wood | coast | plain |
| 2 | North Wood II | Kelphollow Wood | wood | coast | t0 |
| 3 | North Downs | Sorrowdale Vale | downs | coast, fertile | t0 |
| 4 | North Wood III | Tidewood | wood | coast | t0 |
| 5 | Northwest Downs | Sentinel's Hope | downs | coast, river, fertile | t0 |
| 6 | Northwest Downs II | Wyrmtree Wolds | downs | coast, fertile | t0 |
| 7 | Northwest Downs III | The Wolds of Sheaves | downs | fertile | t1 |
| 8 | Northwest Downs IV | The Sward of Shepherds | downs | fertile | t0 |
| 9 | Northwest Downs V | Sheafgill Sward | downs | fertile | t0 |
| 10 | North Downs II | Knotwold | downs | coast, fertile | t0 |
| 11 | North Wood IV | Knellmouth Weald | wood | coast, fertile | t0 |
| 12 | North Downs III | The Sward of Scythes | downs | coast | t0 |
| 13 | Northwest Steppe | The Unhallowed Wilds | steppe | coast, river | t0 |
| 14 | Northwest Steppe II | Wrensweep | steppe | river | t0 |
| 15 | Northwest Downs VI | Owl's Vale | downs | fertile | t0 |
| 16 | Northwest Steppe III | The Plains of Kites | steppe |  | t0 |
| 17 | North Steppe | North Steppe | steppe |  | plain |
| 18 | North Downs IV | Teaselhorn Leas | downs | lake, fertile | t0 |
| 19 | Northeast Downs | Sheafcross Barrows | downs | fertile | t0 |
| 20 | Northeast Downs II | Sheafwold | downs | river, fertile | t0 |
| 21 | North Downs V | Barleydown | downs | river | t0 |
| 22 | Northeast Downs III | Bailiff's Knoll | downs | coast, fertile | t0 |
| 23 | Northeast Downs IV | Drover's Leas | downs | river, fertile | t0 |
| 24 | Northeast Downs V | The Bare Vale | downs | coast, fertile | t0 |
| 25 | Northeast Downs VI | Wayfarer's Penance | downs | river, fertile | t0 |
| 26 | Northwest Downs VII | Teaseldown | downs | fertile | t0 |
| 27 | Northwest Downs VIII | Northwest Downs | downs | fertile | plain |
| 28 | Northwest Steppe IV | Northwest Steppe | steppe |  | plain |
| 29 | Northwest Downs IX | Frostlea | downs |  | t0 |
| 30 | Northeast Downs VII | Hagwold | downs | fertile | t0 |
| 31 | North Downs VI | Tarebarrow | downs | river, fertile | t0 |
| 32 | Northeast Downs VIII | The Downs of the Harvest | downs | fertile | t0 |
| 33 | Northeast Downs IX | Dreadwold | downs | river, fertile | t0 |
| 34 | Northeast Downs X | Shroudlea | downs | fertile | t0 |
| 35 | Northeast Downs XI | The Bleached Wolds | downs | river, fertile | t0 |
| 36 | Northwest Heath | Northwest Heath | heath | high, ore, barren | plain |
| 37 | North Heath | North Heath | heath | lake, high, barren | plain |
| 38 | Northeast Downs XII | Stonegill Vale | downs | fertile | t0 |
| 39 | Northeast Downs XIII | The Wolds of the Unburied | downs | river, fertile | t0 |
| 40 | Northwest Wood | Coaledge Brake | wood | coast | t0 |

### 14.5 Questions for Paul: answered 2026-11-19

Paul's answers are recorded in `docs/design/proposals.md`, "Paul's decisions of 2026-11-19 on the region name questions"; the spec writer's entry "Region names: who keeps the plain name, and "The" inside history sentences" is accepted by them.

1. **Who keeps the plain name**: the largest region of each base name (as specified in RG11 4; no change).
2. **`The` in running text**: yes, a leading `The ` of a name is lower-cased inside a sentence; stored names keep the capital. This is the one answer that changes behaviour; it is a rule of the sentence renderer, specified in 14.7 for P3-04 and P3-07 (Appendix B holds the patch text). It changes no byte of a world file (sentences are rendered, not stored), only rendered text.
3. **Coincidence with real places**: exclude famous places and names from well-known fiction and games; incidental coincidences are not a concern (as shipped; no change).
4. **The lists** are Paul's to edit (`game/data/placenames/`); an edit is a data change.
5. **Trait steering**: as shipped (no change).

### 14.6 Notes for other specs (for the lead)

1. **P3-02 4.4** says region names "have a different form (`East Wood II`), so collisions with them are rare". From r3 a region name can be one capitalised word, the same form as a god or settlement name. Nothing in the algorithm changes: the history registry is already filled with every `region.name` first (P3-03 8, P3-04 7.2), so no god, civilization or settlement can take a region's name, and region names, fixed before history, never need to avoid history names. **Rule (unchanged, now load-bearing)**: the registry holds the region names before any history name is drawn. Only the sentence needs rewording.
2. **P3-03 section 2** ("with the shipped data no region name can equal a god or age name (their forms differ)") and **14.3 item 21** (the chronology rows do not depend on the region names on shipped data): no longer true by form; item 21 still holds on the four P1 golden worlds (checked, 14.2), so the test stands, but the sentence should say "on the golden worlds" rather than "by form".
3. **P3-04 and P3-05 goldens** store rendered sentences with region names (`p3-04-engine.txt` lines `<case> e<n> …`); they are re-recorded with r3 (10.5). P3-07's examples (`capital:` blocks, sentences) change likewise.
4. **Viewer**: region names are up to 65 bytes (a kept name: 32 + 1 + 32) and up to 48 for drawn names; ASCII with `'`. No change needed; the region panel shows longer names than before.
5. **P1-00 section 3** gains the stream `geo.regions.name`; **P0-03** gains the kind `placename` in its kind list and the grammars `element`, `name_object`, `place_name` and `place_form` (kind-local).
6. **The sentence renderer** (P3-04 8.5, P3-07): the rule of 14.7, patch text in Appendix B.

### 14.7 Names inside sentences (Paul's answer 2 of 14.5; owned by P3-04 8.5)

`render_event` (P3-04 8.5) is the only place the rule applies. Stated in full in Appendix B.1; in short:

- **Which substitutions**: every substitution that renders an entity's display name: `{role}` and `{role:name}` (regions, civilizations, settlements, gods, races: whatever the role's entity type), each name of `{role:all}`, `{@region}` and `{@settlement}`. Not `{role:plural}`, `{role:adjective}` (race words), values, `{value?w1|w2}` words, years, or template literals.
- **The rule**: a name whose first four bytes are `The ` (0x54 0x68 0x65 0x20) is appended as `the ` followed by the rest of the name, unless the name is appended at a **sentence start**: byte offset 0 of the rendered text, or directly after the two bytes `. ` (0x2E 0x20) in the text rendered so far. Everywhere else it is mid-sentence: after `: `, `; `, `, `, `(`, ` and ` and any other bytes, the name is lower-cased this way. In `{role:all}` each name is judged at its own offset, so only the first can be at a sentence start (`The Skib Horde and the Court of Saene`). Only `The ` exactly: `Thebes`, `THE `, `There` and a name equal to `The` are unchanged.
- **Stored names never change**: tables `region`, `civilization`, `settlement`, `god`, `age` keep `The …`; `HistoryView`'s display names keep it; so does every line of P3-07 that prints a name as a field (headings, `<name> (<prefix><id>)` references, `{<refs>}` lists, end-state and `[present]` lines, the civilization block's fields). Only the text `render_event` returns changes, and P3-07 prints that text unchanged (so a chronicle line `… e12 civ_ended/85: The Skib Horde came to an end. {…}` keeps the capital: the sentence starts at its own offset 0).
- **Examples** (real templates of `game/data/history/report.toml`, `origin/p3/06-int`; names in the forms of the shipped cultures and of r3; r5: illustrative, the goblin realm renamed `Gnonk` because P3-02 r5's filter rejects the earlier `Bonk-Gitch`):

| Template | Before | After |
| --- | --- | --- |
| `{creator} awakened the {race:plural} in {@region}.` | Fielie awakened the Elves in The Scarred Downs. | Fielie awakened the Elves in the Scarred Downs. |
| `The {race:plural} founded {civilization} in {@region}.` | The Goblins founded The Warrens of Gnonk in West Wood. | The Goblins founded the Warrens of Gnonk in West Wood. (the leading `The` is a template literal, untouched) |
| `{civilization} came to an end.` | The Skib Horde came to an end. | The Skib Horde came to an end. (sentence start: capital kept) |
| `{attacker} declared war on {defender} over {@region}.` | The Skib Horde declared war on The Hold of Durgarin over The Grove of the Hunt. | The Skib Horde declared war on the Hold of Durgarin over the Grove of the Hunt. |
| `{attacker} besieged {@settlement} of {defender}; it {fell?fell\|held}.` | Clan Durgarin besieged Makmoudin Deep of The Warrens of Gnonk; it fell. | Clan Durgarin besieged Makmoudin Deep of the Warrens of Gnonk; it fell. |
| `{parties:all} made peace after {years} {years?year\|years} of war, neither the victor.` | The Skib Horde and The Court of Saene made peace after 12 years of war, neither the victor. | The Skib Horde and the Court of Saene made peace after 12 years of war, neither the victor. |
| `{claimant} laid claim to {@region}, held by {holder}.` | Clan Durgarin laid claim to The Ashen Downs, held by The Court of Saene. | Clan Durgarin laid claim to the Ashen Downs, held by the Court of Saene. |

- **What it changes**: no world byte (sentences are not stored), so no world hash and no generator version on its own; it ships with r3's version bump. It changes every stored rendered sentence that contains a `The …` name not at a sentence start, and civilization names such as `The Skib Horde`, `The Warrens of Gnonk`, `The Court of Saene`, `The Hold of Durgarin` already exist, so it changes sentences even of worlds whose region names did not change. Counted on `origin/p3/06-int` (the `<case> e<n> <sentence>` lines): `p3-04-engine.txt` 46 of 387 sentence lines (e.g. `geo-seed-0-512 e6 The Goblins founded The Warrens of Bonk-Gitch in West Wood II.`), `p3-05-peoples.txt` 245 of 3648, `p3-06-conflict.txt` 3281 of 6772; and every P3-07 report golden (report text hashes) once P3-07 exists. P3-04's own example in 8.5 is unaffected by the rule (it has no `The …` name mid-sentence) but changes with r3's region names. Region names change those goldens anyway (10.5); both changes are re-recorded together.
- **Tests** (for the P3-04 test writer): each example above through `render_event` on a fixture world; a name `The` alone, `Thebes`, `There Hollow` unchanged; a name after `. ` inside a template (a fixture template `It ended. {civilization} was no more.`) keeps the capital; after `: `, `; `, `(` lower-cased; `{role:all}` with three `The …` names (`The A, the B and the C`) and with the placeholder mid-sentence (all three lower-cased); the stored `name` columns and `HistoryView`'s display names unchanged after rendering.

### 14.8 Revision r4 (2026-11-20): shipping with P3-08

r3 ships in the P3-08 change (generator 0.17.0) together with the history follow-ups; `docs/specs/p3-08-followups.md` gives the order of implementation and the golden values. r4 changes three things and checks the rest:

1. **Two forbidden substrings** (P3-02 r5 11.6, 11.10 question 2: the region namer keeps its own word checks and does not share the history name filter). `common.forbidden` in `game/data/placenames/common.toml` gains `boner` and `dung` (21 entries, sorted). They reject compounds such as `Bonereach`, `Boneridge`, `Bonerigg`, `Bonerock`, `Bonereed` and `Dungate`, `Dungap`, `Dungrass`, `Dungrove`, `Dungill`; no single list word contains either. Measured with the prototype of 14.2 on the region tables of 135 worlds of the P3-06 validation sweep (1024 seeds 0–99, 512 seeds 0–29, 2048 seeds 0–4; 6,339 regions): one name changes, 2048 seed 1 region 33 `Dungrove` → `Bittertree Wood`. Confirmed with the implementation (r5): the P3-08 engineer's `linux-gcc-release` `worldgen-cli` on `p3/08-impl` (32086ab), `generate --seed 1 --map-size 2048 --until regions`, names region 33 `Bittertree Wood` with the shipped data and `Dungrove` with a copy of the data whose `forbidden` lacks `boner` and `dung`; no other region of that world differs. A reference that does not reproduce it is to be checked against this case. On the 13 critic worlds of the P3-06 validation (among them seed 42 at 512, 1024 and 2048) and the seven geography and history cases of `world-hashes.sh` (1,164 regions) no name changes, so the tables of 14.4 and the examples of 5.1 stand.
2. **Stream registry** (a gap of r3): the stream-name registry of P0-01 7.3 (`worldgen/include/worldgen/core/streams.hpp`, `all_stream_names`) gains `geo.regions.name` directly after `geo.regions.cost`, 52 names in all (P0-01 amendment 20). Registering a name changes no world byte; the key is K_name of section 2.
3. **The space sizes of 14.3 and the object counts of 4.1** (r5): 14.3's table gives the r4 counts (the forbidden entries remove 20 to 56 names per land); 4.1's `common` holds 78 objects and the union 140 (r3 said 79 and 141, a counting error; the files are normative and unchanged). Check 4a (iii)'s message numbers forms from 0 and prints the slot with its braces.
4. **Kind checks' report position** (r5, as implemented): `placename_one_common` is reported against the `placenames` directory (it concerns no single definition); `placename_lands_distinct` is reported at the later of the two definitions (the one with the larger id), and only its first failure.
5. **The "The" rule** (14.7) is applied to P3-04 (r10, 8.5) and P3-07 (r7, 6.1) by the P3-08 spec writer, from Appendix B; nothing here changes.

Checked and complete as written, for the engineer: the kind `placename` (4.1: fields, grammars, checks, shipped files), the two tuning records of 4 with their fixture copies, the stream and K_name (2), check 4a, RG11 1–8 with the fallback and the fatal message, invariant 6, the r3 tests of 10.5 and the golden lines. The fixture roots `worldgen/tests/data/geo/` and `worldgen/tests/data/geo-small/` gain byte-identical copies of `game/data/placenames/` with the implementation (10.5 "Data tests"), and `game/data/worldgen/regions.toml` and both fixture copies gain the two r3 lines of section 4's block.

**Amendment record.** r4 (2026-11-20): items 1, 2 and 5 above as first written. r5 (2026-11-20, after the P3-08 implementation): item 1's confirmation with the engineer's binary, item 3 (space sizes, object counts, check 4a's message), item 4 (report positions).

## 15. Revision r6 (2026-11-20): tighter region names (P3-10, generator 0.19.0)

Normative for the change P3-10 (`docs/specs/p3-10-followups.md`), from generator **0.19.0**, not before; written after the world critic's review of P3-08 (`docs/reviews/p3-08-followups-world-critic.md` F7). Only `region.name` changes. No schema change, no new field or kind: four edits of the shipped `placename` lists (15.1 to 15.3, data, applied by the engineer to `game/data/placenames/` and both fixture copies with the implementation) and one validity rule (15.4). Paul decided that the lists are Paul's to edit and that only famous places are excluded (14.5); the edits below remove and move words, they add no exclusion. The keeper rule (RG11 4: the largest region of each base name keeps the plain name) was a question for Paul (15.5); r7 makes Paul's answer normative: one plain name per land noun (15.7).

### 15.1 Church words out

Words of an English parish, not of this world's pantheon (the critic counted 57 of 4,951 regions of the 100 default 1024 sweep worlds at 0.17.0 carrying one), are removed:

- `common.possessors`: `Abbess`, `Anchorite`, `Martyr`, `Penitent`, `Pilgrim`, `Prophet`, `Saint`, `Sexton`;
- `common.objects`: `Martyrs`, `Penance`, `Pilgrims`, `Prayers`, `Saints`, `the Unshriven`;
- `common.prefixes`: `Rood`, `Tithe`; `common.places`: `Penance`, `Tithe`;
- `trait_fertile.objects`: `Tithes`.

`Hallow`, `Hallowed`, `Unhallowed`, `Hermit`, `Bellringer` and `Godless` stay (they do not name a church office or rite).

### 15.2 Fewer grim words, and grim words on barren land

**The grim class** (for this revision and its measurements): the words of the common lists that name death, grief, mourning, ruin or a curse, exactly these:

- prefixes: `Bale`, `Bane`, `Bier`, `Charnel`, `Dirge`, `Dole`, `Dread`, `Drear`, `Gallow`, `Ghost`, `Gibbet`, `Grave`, `Grief`, `Grim`, `Knell`, `Lich`, `Lorn`, `Mourn`, `Pall`, `Pyre`, `Shroud`, `Sorrow`, `Tomb`, `Weep`, `Wight`, `Woe`, `Wraith`, `Wither`;
- adjectives: `Accursed`, `Baleful`, `Bereft`, `Bloodless`, `Cheerless`, `Crumbling`, `Cursed`, `Desolate`, `Dismal`, `Doleful`, `Dolorous`, `Dread`, `Drear`, `Dreary`, `Dying`, `Fallen`, `Forgotten`, `Forlorn`, `Forsaken`, `Friendless`, `Ghostly`, `Godless`, `Grieving`, `Grievous`, `Grim`, `Grisly`, `Haunted`, `Joyless`, `Lamenting`, `Lost`, `Moaning`, `Mouldering`, `Mourning`, `Orphaned`, `Pitiless`, `Ruined`, `Shunned`, `Sorrowing`, `Spectral`, `Stricken`, `Unhallowed`, `Wailing`, `Weeping`, `Widowed`, `Woeful`, `Wretched`;
- places: `Bane`, `Dole`, `Grave`, `Grief`, `Lament`, `Ruin`, `Shame`, `Sorrow`;
- possessors: `Gravedigger`, `Hangman`, `Headsman`, `Mourner`, `Orphan`, `Widow`, `Widower`;
- objects: `Bones`, `Curses`, `Gallows`, `Ghosts`, `Gibbets`, `Graves`, `Grief`, `Mourners`, `Ruin`, `Shades`, `Sorrows`, `Tombs`, `Widows`, `Wights`, `Wraiths`, `the Damned`, `the Fallen`, `the Forgotten`, `the Forsaken`, `the Hanged`, `the Lost`, `the Slain`, `the Unburied`, `the Weeping`.

A drawn name is **grim** when one of its words (split at spaces, a final `'s` dropped) is a word of the class, lower-cased, or starts with a prefix of the class (`Dolebrake`), or the name holds an object of the class. Bleak, dark and cold words (`Black`, `Ashen`, `Bleak`, `Leaden`, `Riven`) are not in the class: they are the setting's colour, not a lament. By this definition 13% of drawn names are grim at 1024 (400 of 2,863 in the 100 default sweep worlds at 0.17.0; seed 0 9 of 29) and 14% at 2048 (119 of 837, seeds 0 to 4); the critic's broader reading (gloom of any kind) counted 15 of 29 in seed 0.

**The edit**: of the grim words of each common list, in list order, the first, fourth, seventh, … (every third, starting with the first) stay in `common`; the others are **moved** to the same list of `trait_barren` (`traits.toml`; merged into its sorted list, a word it already holds not repeated), so they are drawn only for regions with the trait `barren` (habitability mean ≤ 20, record `name_traits`). Kept in `common`:

- prefixes `Bale`, `Charnel`, `Dread`, `Ghost`, `Grief`, `Lich`, `Pall`, `Sorrow`, `Wight`, `Wraith`;
- adjectives `Accursed`, `Bloodless`, `Cursed`, `Doleful`, `Drear`, `Fallen`, `Forsaken`, `Godless`, `Grim`, `Joyless`, `Moaning`, `Orphaned`, `Shunned`, `Stricken`, `Weeping`, `Wretched`;
- places `Bane`, `Grief`, `Shame`; possessors `Gravedigger`, `Mourner`, `Widower`;
- objects `Bones`, `Ghosts`, `Grief`, `Shades`, `Widows`, `the Damned`, `the Forsaken`, `the Slain`.

Moved to `trait_barren`: every other word of the class (18 prefixes, 30 adjectives, 5 places, 4 possessors, 16 objects; `Dole` is already a barren place). So the elves' forest homeland is rarely "Dreadlaw Hurst", and a waste or a bare crag still can be "the Desolate Waste" (design 4's later renaming by history is a P5 proposal, `docs/design/proposals.md` "History: two ideas from the world critic's review of P3-08").

### 15.3 Three small edits

- `wood.suffixes` loses `hanger` (`Starkhanger`, `Kitehanger`, `Gallowhanger` read as a coat hanger);
- `trait_high.adjectives` loses `High` (`The High Heights`, seed 11 at 0.17.0; a high region gains nothing from the word);
- `common.places` loses `Cleft` and `Gap`, and `trait_high.places` gains `Cleft` (`trait_pass.places` already holds `Gap`): a gap or a cleft only in high or pass regions, not in lowland grass (`Hawk's Gap`, seed 0, at most 118 m).

### 15.4 V7: one use of a word stem per world

**V7 stems** (added to RG11 7; checked after V1 to V6). stem(v) = v lower-cased, without a leading `the `, cut to its first five bytes (all of it when shorter). S = the set of the stems of the slot values of every name drawn so far in this world (RG11 8's id order), over the slots other than `{noun}` and `{suffix}` (land nouns and suffixes are few per land and repeat by nature: a forest world has many `… Wood`s). An attempt is invalid when the stem of one of its slot values other than `{noun}` and `{suffix}` is in S. When a region is named by an attempt, the stems of that attempt's values join S. Keepers have no slot values; the fallback of RG11 8 does not apply V7 and adds nothing to S (it stays exact and bounded). So `the Weald of Floods`, `the Flooded Thicket` and `the Flooded Weald` (stem `flood`), or four `… of the Harvest`, no longer occur in one world. Names now also depend on the drawn names of the regions before them in id order through S, as they did through U; RG11's determinism note holds (one fixed order, keepers first).

### 15.5 Measured (prototype on the stored region rows; the plain-name question, decided)

Method: the test writer's reference `worldgen/tests/reference/p1_regions_reference.py` (RG11 as shipped reproduces every stored name of the 0.17.0 worlds checked) with the edits above, on the region and border rows of the P3-08 validation sweep (`~/.cache/the-unmapped/out/p3-08-validation/sweep/`: 100 default 1024 worlds, seeds 0 to 99; 30 at 512, seeds 0 to 29; 5 at 2048, seeds 0 to 4); the geography is unchanged since 0.11.0, so these are the 0.18.0 regions. Harness: `~/.cache/the-unmapped/out/p3-10-spec/rn2.py`. "Repeats": second and later uses, among a world's drawn names, of the first five letters of a word other than the last.

| | 1024 (100 worlds) | 512 (30) | 2048 (5) |
| --- | --- | --- | --- |
| regions; plain (kept) names | 4,951; 2,088 (42%) | 393; 330 (83%) | 995; 158 (15%) |
| grim drawn names, shipped → r6 | 400 → 241 (13% → 8%) | 9 → 12 (14% → 19%; 63 names) | 119 → 55 (14% → 6%) |
| church words, shipped → r6 | 72 → 1 | 0 → 0 | 23 → 0 |
| repeats, shipped → r6 | 75 → 26 | 0 → 0 | 145 → 41 |
| attempts per drawn name, mean (max), r6 | 0.11 (4) | 0.05 (1) | 0.64 (9) |
| fallbacks | 0 | 0 | 0 |

(Church words are counted by substring: abbess, sexton, saint, rood, pilgrim, prophet, martyr, tithe, unshriven, penitent, anchorite, prayer, penance; the one left at 1024 is `The Brooding Hurst`, a false match of `rood`. The 512 grim count rises on 63 drawn names, within the noise of so few.) Seed 42 at 2048: `… of the Harvest` 4 → 1, `Antler` 3 → 1, `Wyrm` 4 → 1. Seed 11 at 1024: `The High Heights` is gone.

**The plain name (Paul's question; decided in r7).** The keeper rule gives the compass name to the largest region of its base name, which is often the most fought over (the critic: seed 42's South Wood is in 26 summary lines, and seed 42 keeps seven compass Woods), and leaves small maps mostly plain. Measured (same worlds, plain share of all regions):

| Keeper rule | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| (a) shipped: the largest region of each base name | 83% | 42% | 15% |
| (b) only a base name that occurs once keeps it | 68% | 18% | 4% |
| (c) one plain name per land noun: the largest region of each noun (one `… Wood`, one `… Downs` per world) | 33% | 12% | 4% |
| (d) as (a), but the middle region of the group by size (the critic's "a middling region") | 83% | 42% | 15% |

**Decided (r7): (c).** Paul accepted the recommendation on 2026-11-20 (`docs/design/proposals.md`, "Paul's decision of 2026-11-20: the P3-10 questions go with their recommendations", item 1), replacing the keeper rule of Paul's decision of 2026-11-19 (question 1 of 14.5). A world keeps one compass name per kind of land as orientation (`West Wood`, `North Downs`), small maps get named land, and seven compass Woods become one.

### 15.7 The keeper rule from 0.19.0 (r7; replaces RG11 4's keeper and step 8's U)

1. **The land noun** of a region is the noun of its base name: the word RG11 3 gives it (rule a: `names.isle`; b: the lake biome's `region_noun`; c: `names.upland`; d: the dominant biome's `region_noun`), compared as bytes. Two regions have the same land noun exactly when their base names end in the same noun, whatever their sectors: `North Wood`, `South Wood` and `Central Wood` share the noun `Wood`; `West Isle` and `East Isle` share `Isle`; `Fells` (upland rule c) is its own noun, distinct from the biome noun of the upland region's dominant biome. With the shipped data the nouns are the biome nouns (`Wood`, `Carr`, `Downs`, `Fen`, `Heath`, `Marsh`, `Mere`, `Moor`, `Peaks`, `Steppe`, `Waste`; each region noun belongs to one biome) and the words `Isle` and `Fells`.
2. **The keeper** of a land noun is, among all regions with that noun, the one with the most `cells`, ties to the smaller id. It is named its own base name (sector word + noun, as before), so the plain names of a world are distinct: one per noun, each with its keeper's sector. A noun that occurs in one region only makes that region its keeper. Every other region draws a name (RG11 6 to 8, with V7).
3. U (RG11 8) = the set of the keepers' names before any draw; the draws, V1 to V7, the fallback and the fatal message are unchanged. A drawn name may still never equal a keeper's name; it may now be a base name that no region keeps (`East Wood` when `West Wood` is the keeper): the lists cannot produce a sector word followed by a noun as one of their forms (no form is `{sector} {noun}`), so this does not occur.
4. RG11 4's sentence "The keeper of a base name is, among the regions with that base name, the one with the most `cells`, ties to the smaller id" and invariant 6's "exactly one region is named it" describe generators 0.17.0 and 0.18.0; from 0.19.0 invariant 6 reads: **for each land noun in use, exactly one region (the one with the most `cells`, ties to the smaller id) is named its base name; every other region's name is drawn (one of its land's forms with pool words, passing V1 to V7) or is the fallback compound, and equals no keeper's name.**

**Effect** (the prototype of 15.5, with 15.1 to 15.4 and this rule together, on the same region rows):

| | 512 (30 worlds) | 1024 (100) | 2048 (5) |
| --- | --- | --- | --- |
| plain names (of all regions) | 132 of 393 (33%) | 623 of 4,951 (12%) | 44 of 995 (4%) |
| drawn names: r5 → r7 | 63 → 261 | 2,863 → 4,328 | 837 → 951 |
| grim drawn names (15.2's class) | 29 (11%) | 390 (9%) | 70 (7%) |
| attempts per drawn name, mean (max) | 0.07 (1) | 0.14 (6) | 0.75 (11) |
| fallbacks | 0 | 0 | 0 |

**Measured at the implementation** (`origin/p3/10-impl` f5caea9; r8 note): plain names 12% at 1024 and 33% at 512, as above.

**The name space.** More regions draw (at 512 four times as many), but every land's space is far from full: a region competes with the drawn names of its own land, at most about 130 at 2048 before and now at most the land's regions of one world (the five 2048 worlds have about 200 regions each; the wood land has 1,331 one-word compounds alone, 14.3); with V7 the stems of earlier names also block, which is what raises the mean attempts at 2048 to 0.75. The largest accepted attempt is 11 of 64 and no region reached the fallback in the 135 worlds; the argument of RG11 "Why 64 attempts" holds with V7's extra failure share (a drawn name at 2048 blocks at most its four or five non-noun stems among the land's 150 to 200 prefixes, adjectives and objects).

### 15.6 Tests and goldens

- Data tests: the four edits of 15.1 to 15.3 are in the shipped files and both fixture copies (10.5's byte-identical copies); `trait_barren`'s lists are sorted without repeats; the placename checks of 4.1 pass.
- V7 unit tests: a fixture land whose first drawn region takes `The Flooded Thicket` and whose second region's attempt 0 is `The Weald of Floods`: attempt 0 is invalid (stem `flood`) and the next valid attempt names it; nouns and suffixes repeat freely (`Thornholt` then `Elmholt` in a `wood` land; r9: the example was `Ashholt`, which V1 rejects, `h` then `h`); a keeper does not add to S; a fallback name does not add to S.
- Keeper unit tests (r7): four `North Downs` regions and one `South Downs` region (cells 4443, 6898, 14036, 5593 and 20000): only the `South Downs` region keeps its name; equal cells keep it on the smaller id; a noun used once is kept by its region; `West Isle` and `East Isle` share one keeper; an upland forest region (`… Fells`) does not compete with the `… Wood` regions.
- Golden values: `p1-06-regions.txt` is re-recorded by the reference: the `region` table hash changes in every case in which a land noun occurs in two or more regions or a name is drawn: 33 of its 43 cases (every `designed-*` case but `designed-0-shipped`, `designed-0-bigmerge` and `designed-0-tile`; `terrace-0`, `terrace-1`; `shipped-512-s0`, `shipped-512-s42`, `shipped-512-phrase`, `shipped-1024-s1`, `shipped-64-s0-t`, `shipped-128-s0-t`, `shipped-128-s42-t`, `small-128-s2`, `small-128-s2-t`); the 10 cases whose plain names all have distinct nouns keep their names (`designed-0-shipped`, `designed-0-bigmerge`, `designed-0-tile`, `shipped-48-s3`, `shipped-64-s0`, `shipped-128-s0`, `shipped-128-s9`, `shipped-128-s42`, `small-128-s0`, `small-128-s1`; counted from the golden file's `names` lines), and, having no drawn name, keep their `region` hash. The layer, `region_border` and `region_biome` must not change. Every world hash changes with the data (`content_hash`) and the names.

**Amendment record.** r6 (2026-11-20): section 15 for P3-10. r7 (2026-11-20): Paul's decision on the plain name, 15.5 closed and 15.7 (the keeper per land noun) normative. r8 (2026-11-21): 15.7's shares confirmed by the implementation. r9 (2026-11-21): 15.6's suffix example is `Thornholt` then `Elmholt` (`Ash` + `holt` fails V1); text only.

## Appendix B. Patch text for the P3 specs (to be applied by the P3 spec writer)

Ready to apply to the newest text of each spec (`origin/p3/06-spec-fixes`), with the revision note each spec uses.

### B.1 P3-04 section 8.5 ("Rendering")

Replace the sentence "No other transformation: no capitalization, no trimming, no locale." with:

> **Names inside a sentence** (P1-06 r3 14.7; Paul's decision of 2026-11-19). Every substitution that renders an entity's display name (`{role}` and `{role:name}`, whatever the role's entity type; each name of `{role:all}`; `{@region}`; `{@settlement}`) applies one rule: if the name's first four bytes are `The ` (0x54 0x68 0x65 0x20) and the name is not appended at a **sentence start**, the bytes appended are `the ` followed by the name from its fifth byte on. A sentence start is byte offset 0 of the text being rendered, or a position directly after the two bytes `. ` (0x2E 0x20) of the text rendered so far (literals and earlier substitutions alike). Any other position is mid-sentence, including after `: `, `; `, `, `, `(` and ` and `. In `{role:all}` each name is judged at the offset where it is appended. Nothing else is transformed: `{role:plural}`, `{role:adjective}`, values, `?`-words, years and literals are copied as they are; no other capitalization, no trimming, no locale. Stored names and `HistoryView`'s display names never change; the rule exists only in the text `render_event` returns.

Add to 8.5's examples: `Fielie awakened the Elves in the Scarred Downs.` (region `The Scarred Downs`) and `The Skib Horde declared war on the Hold of Durgarin over the Grove of the Hunt.` (a `The …` name at the start keeps its capital, two later ones do not).

Add to P3-04's tests (16, `render_event` unit tests) the cases of P1-06 r3 14.7 "Tests". Revision note: "render_event lower-cases a leading `The ` of a name mid-sentence (P1-06 r3 14.7); no schema or format change; re-records every golden holding rendered sentences (with the region names of P1-06 r3)."

8.4 (template grammar) is unchanged.

### B.2 P3-07

P3-07 needs no change of behaviour: every sentence it prints is `render_event`'s text, printed unchanged after its head (`<head>: <sentence>`; the sentence begins at its own offset 0, so a leading `The …` name keeps its capital), and every other place it prints a name is a field, which keeps the stored name: age headings (`== a<id> <age name>: …`), the `[ages]` lines (`a<id> <name>: …`), the civilization block (`c<id> <name>`, `founded: … in <age name> (a<age>)`, `capital: <settlement ref>`, `home:`, relations and milestone references), every `<name> (<prefix><id>)` reference and `{<refs>}` list, the `[present]` and end-state lines (`c<id> <name>: …`, `  rg<id> <name>: …`, `  s<id> <name>: …, at <x> <y> in <region ref>, founded <year> by <founder ref>`). Patch text, appended to the "Sentences" bullet of section 6 (text conventions):

> A sentence is `render_event`'s text unchanged; its rule for `The …` names inside a sentence (P3-04 8.5) is not applied again and not undone. Every name the report prints outside a sentence (headings, references, field values, end-state lines) is the stored name, with its capital `The`.

And to 14 (tests), item 4's list: "including a sentence with a `The …` name mid-sentence (lower-cased by `render_event`) next to the same entity's reference in the refs (capitalised)". Revision note: "text only (P1-06 r3 14.7); the golden report hashes change with P3-04's rule."

