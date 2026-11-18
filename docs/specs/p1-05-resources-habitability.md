# P1-05 Resources and habitability

Status: **second version, draft, spec writer, 2026-11-18, awaiting approval.** Generator **0.10.0**; **no schema change** (schema version stays 4). It replaces the first draft of 2026-11-16 (never implemented), which was written before the biome stage existed in its present form; section 0 lists every change. Calibrated on the 0.9.0 validation worlds (512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–9, layers of `~/.cache/the-unmapped/out/p1-04b-validation/`) with an exact-integer prototype of this spec (section 11). Phase P1 Geography, stage 5 (`resources`). Design doc section 4 (stage 5: "per-cell scores for fertility, ore, timber and fresh water. The history simulation uses these to place peoples"), 5 (awakenings "chosen to suit that race's preferred land"; dwarves "build in places the others avoid"; faeries in "wild places"; settlements "grow or shrink from local resources"; beasts "feed it through hunting"; civilizations "build a road"; sites include mines). Depends on P0-01 to P0-04, P1-00 (G1, G2, G6, G7, G8, D1 to D10), P1-02 (H7), P1-03 and P1-04 (the `biome` layer and the biome definitions). Proposals "High peaks may be inaccessible; history opens the passes", "More natural biomes, per-world temperature, a thinner coastal forest"; world critic `docs/reviews/p1-04b-biome-variety-critic.md` section 5.

## 0. Changes from the first draft

| What | First draft (2026-11-16) | This version | Why |
| --- | --- | --- | --- |
| Layers | six | seven: **`forage`** added (wild food: fish, fowl, game, reeds) | critic: wetlands need value "as resources (timber, fowl, reed, peat) rather than as fertility"; design: hunting feeds settlements, faeries and elves in wild places |
| Biome kind | `fertility`, `timber` | new field **`forage`** (fx(0, 1), after `timber`) in every biome file; **steppe `fertility` 0.6 → 0.7** | per-biome values are data (4.1); critic: steppe "about 0.7 (grazing and dry grain)" |
| Timber | biome × rain ramp × slope | biome × slope | the biome rules already encode rain (forest needs 720 mm); the ramp double-counted it |
| Fresh water reach | 600 m | 400 m | the river threshold fell to 0.125 km² after the draft; 72–94% of dry land is within 400 m of a river or lake |
| Ore | height ramp 200–1200 m, veins above noise 0.05 | 100–800 m, veins above noise 0, lowland 0.15 | the draft left 512 seed 1 without a single cell of ore ≥ 128 |
| Habitability | weighted mean of four scores × slope, flood, coast bonus, pocket | weighted mean of fertility, forage, timber, ore × a **water factor** × slope, flood, pocket; **no coast bonus** | water is a need, not one asset among others; the sea's value now enters through `forage` (fish) |
| Report | six keys | adds `fertile_share` (fertility ≥ 166, i.e. 0.65, below forest's 0.7) and `ore_rich_share` | critic: "do not judge fertile land with a hard 0.7 cut that equals forest's value" |
| Inputs | `coast_dist` | `river` instead (the fish distance covers the sea) | |
| Layer ids | 12–17, `region` 18 | 12–18, `region` 19 | `forage` is 16 |
| Statistics | provisional, from a float prototype | measured on 110 real 0.9.0 worlds, per map size | |

## 1. Purpose

Score every land cell for what history needs to place peoples and grow settlements, and mark which land is connected on foot. Each output has a consumer:

| Layer | Meaning | Consumers |
| --- | --- | --- |
| `fertility` | farming and grazing potential | history: food a settlement can grow (tick loop step 1), homelands of farming peoples (humans, goblins, orcs by their race data); `report` `fertile_share` |
| `forage` | wild food: fish, fowl, game, nuts, reeds | history: the second food source (fishing villages, wetland and forest peoples; elves and faeries in wild places); beast habitat with the biome |
| `timber` | usable wood | history: building, ships, settlement growth; forest homelands |
| `ore` | metal near the surface | history: mine sites (design 5, "Sites"), dwarf homelands, wealth |
| `fresh_water` | drinking water | history: where a settlement can stand; the water factor of `habitability` |
| `habitability` | one generic settlement-site score | P1-06 (region seeds at the best sites, `region.habitability_mean`); history: candidate sites for founding when no race preference applies |
| `walk_area` | connected walkable ground | P1-06 (`region.walk_area`, `region_border.walk_pairs`); history: which lowlands a range separates, where a road needs a pass, a boat or a detour |

Race preferences are **not** computed here: each race's data will weight these layers, the biome and the terrain (P3). `habitability` is the generic site score of a farming people.

Three facts shape the scores:

- **Steep ground is unusable.** Fertility and habitability fall to 0 between 10° and 25° of slope; timber between 35° and 54°; ground steeper than 35° is not walkable and belongs to no walk area. At 0.9.0, 11–34% of land is steeper than 35°, so a steep 512 world has much land at habitability 0 (critic A5: "some worlds at 512 will have only two or three habitable regions").
- **Ranges separate lowlands.** Walk areas are the connected pieces of walkable land. A small walkable pocket (a high valley, a coastal pocket behind cliffs, P1-01 limitation 6) is too small to be an area, and its habitability is reduced (few people settle where nobody can walk in; dwarves may, by their own preference).
- **Scores are absolute.** 255 is the maximum of the scoring function, the same on every map, so a poor world scores low everywhere. History can normalize within a world; it cannot recover an absolute scale from normalized data.

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation`, `water`, `water_depth`, `flow_dir`, `river` | P1-01, P1-02 |
| `rainfall` | P1-03 |
| `biome` | P1-04 |
| biome definitions: `fertility`, `timber`, `forage` of each `code` | kind `biome` (4.1) |
| tuning | definition `worldgen.resources` (4.2) |
| world seed, N | |

Stream: **K_ore = `stream_key(seed, "geo.resources.ore")`**, a noise key (D1), used only by `fbm_m` in R4. No `Rng`, no other randomness.

Preconditions, each fatal with the prefix `resources: input precondition: ` (checked in this order): every input layer present (`missing layer '<name>'`); the P1-00 grid checks; every array of length W × H (`<name> length is not width * height`); `water` in 0..3 (`water value <v> at (<x>, <y>)`, lowest index); `river` in 0..15 (same form); every `biome` value is the `code` of a loaded definition (`biome value <v> at (<x>, <y>) has no definition`, lowest index); then G7 with k = 1 (`resources: input precondition: G7: …`). The layer types are those of the catalogue (P1-00 4); a world read from disk with another type is refused by the reader.

## 3. Outputs

Seven layers, N × N, codec 1, written in this order in one transaction; layer ids 12 to 18 in a full run (P1-00 3):

| Id | Layer | Type | Ocean (`water` 3) | Lake (`water` 2) | Dry land and river (`water` 0, 1) |
| --- | --- | --- | --- | --- | --- |
| 12 | `fertility` | `u8` | 0 | 0 | R4, 0..255 |
| 13 | `ore` | `u8` | 0 | 0 | R4, 0..255 |
| 14 | `timber` | `u8` | 0 | 0 | R4, 0..255 |
| 15 | `fresh_water` | `u8` | 0 | 255 | R3, 0..255; 255 on every river cell |
| 16 | `forage` | `u8` | 0 | 0 | R4, 0..255 |
| 17 | `habitability` | `u8` | 0 | 0 | R6, 0..255 |
| 18 | `walk_area` | `u16` | 0 | 0 | R5: the walk-area id 1..K, 0 if not walkable or in an area below the minimum size |

All scores: 0 = none, 255 = the maximum of this spec's scoring function; absolute, not normalized per map. Lake cells are water: their fish count in the `forage` of the shore, not on the lake.

**Schema and catalogue (for the lead).** No table and no column: **schema version stays 4**, and regions (P1-06) keep version 5. Catalogue (P1-00 4, `docs/specs/schema.md` 3): **new layer `forage`** (`u8`, score as above; an addition, not a schema change under P1-00 4 rule 3); `walk_area` as catalogued on 2026-11-16; the meaning of `habitability` becomes "combining fertility, forage, timber and ore with fresh water, slope, flooding and walk area" (it named the coast before; no world has ever held the layer). The layer ids shift: `region` becomes 19.

## 4. Data and tuning

### 4.1 The `biome` kind: new field `forage`, steppe fertility

**Amendment to P1-04 4.1 and P1-00 6.6** (lands with this stage; the biome stage itself is unchanged and does not read the field). The `biome` kind gains, declared **after `timber` and before `region_noun`** (this fixes the canonical encoding, P0-03 8):

| Field | Type | Meaning |
| --- | --- | --- |
| `forage` | fx(0, 1) | wild food of the biome (fish, fowl, game, nuts, reeds, eggs), used by P1-05 R4 |

The field is required (there is no optional-field mechanism): every file of `game/data/biomes/` and the fixture copies `worldgen/tests/data/geo/biomes/` and `worldgen/tests/data/geo-small/biomes/` gain one line `forage = …` after `timber`, and the fixture copies stay identical to the shipped files. Steppe's `fertility` changes from `"0.6"` to `"0.7"` in the same files. The values the stage reads, per biome (**bold** = changed or new in this version):

| Id (code) | `fertility` | `timber` | `forage` | Reading |
| --- | --- | --- | --- | --- |
| `ocean` (1) | 0 | 0 | **0** | water; never read (scores are 0 on water) |
| `lake` (2) | 0 | 0 | **0** | water; never read |
| `temperate_forest` (3) | "0.7" | 1 | **"0.5"** | game, nuts, mushrooms |
| `grassland` (4) | 1 | "0.15" | **"0.3"** | the best farmland; some game |
| `marsh` (5) | "0.45" | "0.3" | **"0.8"** | fish, fowl, reeds: the richest wild food |
| `highland_moor` (6) | "0.3" | "0.1" | **"0.25"** | grouse, deer |
| `alpine` (7) | 0 | 0 | **"0.05"** | almost nothing |
| `blighted_waste` (8) | 0 | 0 | **0** | history overlay; dead land |
| `heath` (9) | "0.2" | "0.05" | **"0.2"** | |
| `steppe` (10) | **"0.7"** (was "0.6") | "0.05" | **"0.35"** | grazing and dry grain; herds |
| `wet_woodland` (11) | "0.5" | "0.7" | **"0.7"** | fowl, fish, game; alder and willow |
| `fen` (12) | "0.25" | "0.2" | **"0.6"** | eels, fowl, reeds, peat: poor to farm, not worthless |

Written as TOML strings where fractional (`forage = "0.5"`), integers where whole (`forage = 0`), as the existing fields. No new named check.

### 4.2 Tuning `worldgen.resources`

New id `resources` of the fixed-id kind `worldgen` (P1-00 6.1), file **`game/data/worldgen/resources.toml`**, with identical copies in `worldgen/tests/data/geo/worldgen/` and `worldgen/tests/data/geo-small/worldgen/` (every data directory holding a `worldgen/` directory must define every id, P0-03 amendment 16). All fields required, in this declaration order. Slopes are tangents (0.18 ≈ 10°, 0.47 ≈ 25°, 0.7002 ≈ 35°, 1.0 = 45°, 1.4 ≈ 54°). "Fitted" = chosen by the calibration of section 11; "taste" = a judgement, listed in section 12.

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `fresh_reach_m` | int(16, 10000) | 400 | fitted | distance from a river or lake at which its contribution falls to 0 |
| `fresh_rain_weight` | fx(0, 1) | "0.4" | taste | the largest contribution of rain (wells, springs) |
| `fresh_rain_lo_mm` | int(0, 10000) | 300 | taste | rain contribution 0 at or below |
| `fresh_rain_hi_mm` | int(0, 10000) | 1500 | taste | rain contribution full at or above |
| `fert_slope_lo` | fx(0, 10) | "0.18" | taste | fertility full at or below this slope |
| `fert_slope_hi` | fx(0, 10) | "0.47" | taste | fertility 0 at or above |
| `alluvial_bonus` | fx(0, 4) | "0.3" | fitted | extra fertility factor on the floodplain (grassland by a river is capped at 255; flat forest by a river is 232) |
| `alluvial_hand_m` | fx("0.1", 100) | 5 | taste | height above drainage at which the bonus is gone |
| `timber_slope_lo` | fx(0, 10) | "0.7" | taste | timber full at or below |
| `timber_slope_hi` | fx(0, 10) | "1.4" | taste | timber 0 at or above |
| `fish_river_class_min` | int(1, 15) | 4 | taste | rivers of this class or more carry fish (class 4: about 1 km² upstream, P1-06 limitation 3) |
| `fish_weight` | fx(0, 1) | "0.3" | taste | the largest forage added by fish water |
| `fish_reach_m` | int(16, 10000) | 300 | taste | distance from the sea, a lake or a fish river at which the fish term is 0 |
| `ore_noise` | `fbm_m` | `{ wavelength_m = 1500, octaves = 3, gain = "0.5" }` | taste | where deposits lie (record of P1-01 4.1) |
| `ore_vein_lo` | fx(−1, 1) | 0 | fitted | noise at or below: no ore |
| `ore_vein_hi` | fx(−1, 1) | "0.2" | fitted | noise at or above: a full deposit |
| `ore_elev_lo_m` | int(0, 3000) | 100 | fitted | exposure ramp with height, start |
| `ore_elev_hi_m` | int(0, 3000) | 800 | fitted | exposure ramp, full |
| `ore_lowland` | fx(0, 1) | "0.15" | fitted | least exposure factor (lowland deposits) |
| `ore_slope_base` | fx(0, 1) | "0.5" | taste | exposure factor on flat ground |
| `ore_slope_lo` | fx(0, 10) | "0.3" | taste | slope ramp from base … |
| `ore_slope_hi` | fx(0, 10) | "1.0" | taste | … to full exposure |
| `walk_slope_max` | fx("0.01", 10) | "0.7002" | designed (35°, the report's `slope_over` threshold) | steeper cells are not walkable |
| `walk_min_area_m2` | int(16384, 1000000000) | 250000 | taste | smallest walk area kept (977 cells; about a village and its fields) |
| `hab_slope_lo` | fx(0, 10) | "0.18" | taste | habitability full at or below |
| `hab_slope_hi` | fx(0, 10) | "0.47" | taste | habitability 0 at or above |
| `hab_weight_fertility` | int(0, 1000) | 55 | taste | weight of `fertility` |
| `hab_weight_forage` | int(0, 1000) | 20 | taste | weight of `forage` |
| `hab_weight_timber` | int(0, 1000) | 15 | taste | weight of `timber` |
| `hab_weight_ore` | int(0, 1000) | 10 | taste | weight of `ore` |
| `hab_water_min` | fx(0, 1) | "0.25" | taste | water factor where `fresh_water` is 0 |
| `hab_flood_hand_m` | fx(0, 100) | 1 | taste | below this height above drainage a site floods |
| `hab_flood_factor` | fx(0, 1) | "0.5" | taste | habitability factor on flood-prone cells |
| `hab_pocket_factor` | fx(0, 1) | "0.25" | taste | habitability factor on cells in no walk area |

**Named checks** (per-definition, P0-03 7.5; each reported at the definition's header as `<path>:<line>:<column>: error: check '<name>' failed for 'worldgen.resources': <message>`), in this registration order:

| Check | Condition | Message |
| --- | --- | --- |
| `fresh_rain_ordered` | `fresh_rain_lo_mm` < `fresh_rain_hi_mm` | `fresh_rain_lo_mm must be below fresh_rain_hi_mm` |
| `fert_slope_ordered` | `fert_slope_lo` < `fert_slope_hi` | `fert_slope_lo must be below fert_slope_hi` |
| `timber_slope_ordered` | `timber_slope_lo` < `timber_slope_hi` | `timber_slope_lo must be below timber_slope_hi` |
| `ore_vein_ordered` | `ore_vein_lo` < `ore_vein_hi` | `ore_vein_lo must be below ore_vein_hi` |
| `ore_elev_ordered` | `ore_elev_lo_m` < `ore_elev_hi_m` | `ore_elev_lo_m must be below ore_elev_hi_m` |
| `ore_slope_ordered` | `ore_slope_lo` < `ore_slope_hi` | `ore_slope_lo must be below ore_slope_hi` |
| `hab_slope_ordered` | `hab_slope_lo` < `hab_slope_hi` | `hab_slope_lo must be below hab_slope_hi` |
| `hab_weights_positive` | the four `hab_weight_*` sum to at least 1 | `the hab_weight fields must not all be 0` |

```toml
# game/data/worldgen/resources.toml and the fixtures under worldgen/tests/data/geo*/worldgen/
[worldgen.resources]
fresh_reach_m = 400
fresh_rain_weight = "0.4"
fresh_rain_lo_mm = 300
fresh_rain_hi_mm = 1500
fert_slope_lo = "0.18"
fert_slope_hi = "0.47"
alluvial_bonus = "0.3"
alluvial_hand_m = 5
timber_slope_lo = "0.7"
timber_slope_hi = "1.4"
fish_river_class_min = 4
fish_weight = "0.3"
fish_reach_m = 300
ore_noise = { wavelength_m = 1500, octaves = 3, gain = "0.5" }
ore_vein_lo = 0
ore_vein_hi = "0.2"
ore_elev_lo_m = 100
ore_elev_hi_m = 800
ore_lowland = "0.15"
ore_slope_base = "0.5"
ore_slope_lo = "0.3"
ore_slope_hi = "1.0"
walk_slope_max = "0.7002"
walk_min_area_m2 = 250000
hab_slope_lo = "0.18"
hab_slope_hi = "0.47"
hab_weight_fertility = 55
hab_weight_forage = 20
hab_weight_timber = 15
hab_weight_ore = 10
hab_water_min = "0.25"
hab_flood_hand_m = 1
hab_flood_factor = "0.5"
hab_pocket_factor = "0.25"
```

## 5. Algorithm

Notation of P1-01 5.0 (`Fx` is Q32.32, ONE = 2^32, every operation rounds half away from zero as P0-01 5). X = from_int(16x + 8), Y = from_int(16y + 8), the cell centre in metres (P1-03 5). Helpers:

```
ramp(x, a, b)  (a < b)  = x ≤ a ? 0 : x ≥ b ? ONE : div(x − a, b − a)
q255(s)                 = clamp(round_int(mul_int(clamp(s, 0, ONE), 255)), 0, 255)        // Fx → u8
metres(d)               = min(32767, (32 × d + 5) div 10)                                  // chamfer units → m, as H7
tan                     = the slope tangent of G8 from S2 (Fx)
```

"Land" in this section means `water` ∈ {0, 1} (dry land and river cells); lake and ocean cells take the values of section 3 and are not evaluated. Steps R1 to R7; R2 to R4 and R6 are per-cell functions of fixed inputs.

### R1. Preconditions

Section 2.

### R2. Drainage height, slope (G7, G8)

hand (dm, `int64`) by G7 with **k = 1** (every river, lake and ocean cell is a drain; P1-04 uses k = 3). S2 and tan by G8 on the stored `elevation`.

### R3. Distances and fresh water (D4, then D1)

Two chamfer transforms of P1-02 H7 (`chamfer(source)`, weights 5/7, INF = 2^40):

```
df = metres(chamfer(source = water ∈ {1, 2}))                                       // to a river or lake
dg = metres(chamfer(source = water ∈ {2, 3} or (water = 1 and river ≥ fish_river_class_min)))   // to fish water
```

INF saturates to 32767 m. Then, for each land cell:

```
fs = (df ≥ fresh_reach_m) ? 0 : from_ratio(fresh_reach_m − df, fresh_reach_m)
fr = mul(fresh_rain_weight, ramp(from_int(rainfall), from_int(fresh_rain_lo_mm), from_int(fresh_rain_hi_mm)))
fresh_water = q255(max(fs, fr))                         // river cells: df = 0, so 255
```

### R4. Fertility, timber, forage, ore (D1, G6 row blocks)

For each land cell, with fb, tb, gb the `fertility`, `timber`, `forage` of the definition whose `code` is the cell's `biome`:

```
sf  = ONE − ramp(tan, fert_slope_lo, fert_slope_hi)
hm  = from_ratio(hand, 10)                                                  // metres
al  = ONE + mul(alluvial_bonus, ONE − ramp(hm, 0, alluvial_hand_m))
fertility = q255(min(ONE, mul(mul(fb, sf), al)))

st  = ONE − ramp(tan, timber_slope_lo, timber_slope_hi)
timber = q255(mul(tb, st))

fish = mul(fish_weight, ONE − ramp(from_int(dg), 0, from_int(fish_reach_m)))
forage = q255(min(ONE, gb + fish))

em  = from_ratio(elevation, 10)                                             // metres
eu  = max(ore_lowland, ramp(em, from_int(ore_elev_lo_m), from_int(ore_elev_hi_m)))
su  = ore_slope_base + mul(ONE − ore_slope_base, ramp(tan, ore_slope_lo, ore_slope_hi))
vn  = ramp(fbm_m(K_ore, X, Y, ore_noise), ore_vein_lo, ore_vein_hi)
ore = q255(mul(mul(eu, su), vn))
```

Fertility is the biome's farming value, lost on slopes and raised on the low floodplain; timber is the biome's wood, lost on crags; forage is the biome's wild food plus fish within `fish_reach_m` of the sea, a lake or a river of class ≥ 4; ore is exposure (height and steepness) times a patchy deposit noise, so uplands carry most of it, in veins, and lowlands a little. `fbm_m` is P1-01 4.1's (`fbm2` with base frequency from_ratio(1, `wavelength_m`), lacunarity 2), evaluated on land cells only (any other cell's value is never used).

### R5. Walk areas (D5)

```
walkable(c)  = water(c) ∈ {0, 1}  and  S2(c) × 2^64 ≤ (320 × walk_slope_max.raw)²            (U128)
linked(a, b) = walkable(a) and walkable(b) and (S(a) − S(b))² × 2^64 ≤ w × (160 × walk_slope_max.raw)²   (U128)
               w = 1 for an orthogonal and 2 for a diagonal neighbour; S = elevation + water_depth (dm)
```

Label the components of walkable cells under `linked` like G2 (scan cells in index order; each unlabelled walkable cell starts a component, collected by FIFO BFS visiting neighbours in the 8-neighbour order of P1-00 2.3; an edge is followed only if `linked`). The step condition stops a component from crossing a cliff edge between two cells that each look flat by central differences (G8 does not see a cell's own height). Then, with walk_min_cells = (`walk_min_area_m2` + 255) div 256: components with fewer cells get 0; the others get ids 1, 2, … in order of their smallest cell index (= label order). An id above 65535 is fatal, `resources: more than 65535 walk areas` (impossible with the shipped tuning: each kept area has at least 977 cells and a map at most 2^22 cells). Any labelling method that yields the same partition (union–find, for example) is allowed: the result depends only on the partition and the smallest index of each part.

With `walk_slope_max` "0.7002" (raw 3007336101) a cell is walkable iff S2 ≤ 50204, matching the report's "steeper than 35°" (S2 ≥ 50205); two orthogonal neighbours are linked iff |ΔS| ≤ 112 dm, two diagonal ones iff |ΔS| ≤ 158 dm.

### R6. Habitability (D1, G6 row blocks)

For each land cell, from the `u8` values of R3 and R4 (F `fertility`, W `fresh_water`, G `forage`, T `timber`, O `ore`) and the id of R5:

```
Σw  = hab_weight_fertility + hab_weight_forage + hab_weight_timber + hab_weight_ore
sc  = hab_weight_fertility × F + hab_weight_forage × G + hab_weight_timber × T + hab_weight_ore × O     // int64
b   = from_ratio(sc, 255 × Σw)
wat = hab_water_min + mul(ONE − hab_water_min, from_ratio(W, 255))
h   = mul(b, wat)
h   = mul(h, ONE − ramp(tan, hab_slope_lo, hab_slope_hi))
if from_int(hand) < mul_int(hab_flood_hand_m, 10):  h = mul(h, hab_flood_factor)
if walk_area = 0:                                   h = mul(h, hab_pocket_factor)
habitability = q255(min(ONE, h))
```

The products are taken in exactly this order (each `mul` rounds). Using the stored `u8` scores makes `habitability` reproducible from the stored layers plus `elevation`, `water`, `water_depth`, `flow_dir`, `river`. A cell at or steeper than `hab_slope_hi` gets 0, so with the shipped values every unwalkable cell does (`hab_slope_hi` 0.47 < `walk_slope_max` 0.7002); the pocket factor therefore acts on walkable cells of small areas. Water multiplies rather than adds: a fertile site with no river and little rain (`fresh_water` 0) keeps a quarter of its value (wells), a site by a river keeps all of it.

### R7. Write

`fertility`, `ore`, `timber`, `fresh_water`, `forage`, `habitability`, `walk_area`, in this order (section 3).

### 5.1 Worked examples

Computed by the spec writer's exact-integer reference (`core_reference` operations), shipped tuning and biome values.

**Helpers.** ramp(0.3, 0.18, 0.47) = raw 1777227848 (0.413793). q255: HALF → 128; 0.762 → 194; 0.001 → 0; 0.002 → 1; ONE → 255. Slope tangent from S2 (G8): 0 → 0; 3318 → raw 773122075 (0.180007); 9216 → raw 1288490189 (0.3); 22118 → raw 1996102368 (0.464754); 50205 → raw 3007345802 (0.700202); 102400 → ONE; 250000 → raw 6710886400 (1.5625).

**Chamfer to metres** (one river cell at (4, 1) of a 9 × 3 land grid): chamfer rows `22 17 12 7 5 7 12 17 22`, `20 15 10 5 0 5 10 15 20`, `22 17 …`; metres `70 54 38 22 16 22 38 54 70`, `64 48 32 16 0 16 32 48 64`, `70 54 …`.

**R3 fresh_water** (df m, rainfall mm → value; fs, fr raw): (0, 800) → 255; (16, 800) → 245 (fs 4123168604); (38, 800) → 231; (64, 800) → 214; (200, 800) → 128 (fs HALF); (400, 800) → 43 (rain only: fr = 0.4 × 500/1200 = raw 715827883, 42.50000002 rounds up); (399, 200) → 1 (fs = from_ratio(1, 400) = raw 10737418, 0.6375 rounds up; fr 0); (1000, 1500) → 102; (1000, 900) → 51; (1000, 2500) → 102; (1000, 300) → 0.

**R4 fertility** (fb, S2, hand dm → value; sf, al raw): (1, 9216, 20) → 176 (sf 2517739448 = 0.586207, al 5068061410 = 1.18); (1, 0, 0) → 255 (al 5583457485 = 1.3, capped); (0.7, 0, 0) → 232 (0.91); (0.7, 0, 60) → 178 (al ONE); (0.7, 22118, 100) → 3 (sf 77697452); (0.45, 0, 5) → 146 (al 1.27); (1, 30000, 0) → 0 (tan 0.541 > 0.47); (0.3, 3318, 49) → 77 (sf 4294870875, al 4320737100).

**R4 timber** (tb, S2 → value): (1, 9216) → 255; (1, 50205) → 255 (st raw 4293726303); (1, 102400) → 146 (st raw 2454267026 = 0.571429); (0.15, 0) → 38; (0.7, 250000) → 0 (tan 1.5625 > 1.4).

**R4 forage** (gb, dg m → value; fish raw): (0.8, 0) → 255 (0.8 + 0.3, capped); (0.5, 0) → 204 (fish 1288490189 = 0.3); (0.3, 16) → 149 (fish 1219770712); (0.3, 150) → 115 (fish 644245095); (0.05, 299) → 13 (fish 4294967 = 0.001); (0.3, 300) → 77 (fish 0); (0.2, 5000) → 51.

**R4 ore** (elevation dm, S2, noise n → value; eu, su, vn raw): (8000, 102400, 0.2) → 255 (eu ONE, su ONE, vn ONE); (8000, 102400, 0.1) → 128 (vn raw 2147483651); (500, 0, 0.3) → 19 (eu = `ore_lowland` raw 644245094, su HALF, vn ONE); (15000, 20000, 0.05) → 38 (su raw 2582937308, vn raw 1073741825); (4500, 9216, 0.15) → 48 (eu HALF, su HALF, vn raw 3221225471); (8000, 102400, −0.04) → 0. Noise values are given as decimals; a test feeds their `parse_decimal` raws (0.1 = raw 429496730, −0.04 = raw −171798692).

**R6 habitability** (F, W, G, T, O; S2; hand dm; walk_area → value):

| F, W, G, T, O | S2 | hand | walk_area | sc | b (raw) | wat (raw) | slope factor (raw) | flood | pocket | habitability |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 232, 255, 204, 255, 3 | 0 | 20 | 1 | 20695 | 3485660713 | ONE | ONE | no | no | 207 |
| 232, 255, 204, 255, 3 | 0 | 0 | 1 | 20695 | 3485660713 | ONE | ONE | × 0.5 | no | 103 |
| 255, 255, 153, 38, 0 | 0 | 5 | 1 | 17655 | 2973633240 | ONE | ONE | × 0.5 | no | 88 |
| 255, 255, 153, 38, 0 | 0 | 5 | 0 | 17655 | 2973633240 | ONE | ONE | × 0.5 | × 0.25 | 22 |
| 179, 102, 128, 255, 0 | 3318 | 300 | 1 | 16230 | 2733620361 | 2362232013 | 4294870875 | no | no | 89 |
| 255, 40, 90, 38, 0 | 0 | 300 | 1 | 16395 | 2761411326 | 1579032094 | ONE | no | no | 60 |
| 170, 13, 89, 13, 0 | 0 | 300 | 2 | 11325 | 1907470770 | 1237961162 | ONE | no | no | 33 |
| 200, 200, 200, 200, 200 | 30000 | 300 | 1 | 20000 | 3368601801 | 3600193175 | 0 | no | no | 0 |

(Rows: forest by a river, above and at the water; grassland on a floodplain, in and out of a walk area; forest on a 10° slope 400 m from water; dry grassland far from water; dry steppe; steep ground.)

**R5.** (a) A 5 × 5 test grid, all land (`water` 0, `water_depth` 0), elevation 20 everywhere except 300 at the centre (2, 2). S2 is 78400 at the centre's four orthogonal neighbours (not walkable) and 0 elsewhere, including the centre. The centre is walkable but its diagonal links fail (|300 − 20| = 280 dm > 158), so it is a component of 1 cell. With walk_min_cells 2: `walk_area` is 1 on the 20 outer and diagonal cells and 0 on the centre and its four neighbours.
(b) A 9 × 5 grid, rows y = 0 to 4: `20 21 22 23 400 401 402 403 404`, `20 21 22 23 400 401 402 403 404`, `21 22 23 24 401 402 403 404 405`, `21 22 23 24 401 402 403 404 405`, `22 23 24 25 402 403 404 405 406`. Columns 3 and 4 have S2 ≥ 142884 (the 37.7 m step); with walk_min_cells 2, `walk_area` is 1 on columns 0 to 2, 0 on columns 3 and 4, and 2 on columns 5 to 8: two lowlands that history must join by a pass.
(c) Step limit: a 4 × 1 land strip with elevations 1000, 1000, 1000 + h, 1000 + h (G8's edge rule gives S2 = 0, h², h², 0, all walkable for h ≤ 224) with walk_min_cells 1: h = 112 gives one area of 4 cells; h = 113 gives two areas (ids 1 on x 0–1, 2 on x 2–3). The diagonal limit (158 linked, 159 not) is tested on a layout of the test's choice.

The examples use walk_min_cells 1 or 2, below what `walk_min_area_m2`'s bounds allow; the `walk_areas` seam therefore takes walk_min_cells as an argument (section 10).

## 6. Invariants

1. Every score is in [0, 255]; all seven layers are 0 on ocean; on lake cells all are 0 except `fresh_water` = 255; `fresh_water` = 255 on every river cell.
2. `habitability` = 0 wherever tan ≥ `hab_slope_hi`; `fertility` = 0 wherever tan ≥ `fert_slope_hi` or the biome's `fertility` is 0; `timber` = 0 wherever tan ≥ `timber_slope_hi` or the biome's `timber` is 0; `ore` = 0 wherever the vein noise ≤ `ore_vein_lo`; `forage` ≥ q255(gb).
3. `walk_area` > 0 only on walkable cells; every kept area has at least walk_min_cells cells and is connected under `linked`; two `linked` cells have the same `walk_area`, and two linked walkable cells of a dropped component both have 0; ids are 1 to K, each used, increasing with each area's smallest index.
4. `habitability` is the R6 function of the stored `fertility`, `fresh_water`, `forage`, `timber`, `ore`, `walk_area` and the inputs (rechecked from the written layers).
5. At the cell seams, with all other arguments equal: `fertility_cell`, `timber_cell` and `habitability_cell` do not increase with S2; `fresh_water_cell` does not increase with df; `forage_cell` does not increase with dg; `habitability_cell` does not increase when the flood or pocket factor applies.
6. Pure function of the seed, N, the inputs, the biome values and the tuning; independent of the thread count.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| R2 | D5 (G7), D1 (G8) | |
| R3 chamfers | D4 | H7's two in-place passes, in the stated order; not parallel (each transform is linear) |
| R3 rest, R4, R6 | D1, parallel by G6 row blocks | location-keyed noise; R6 starts after R3, R4 and R5 have finished |
| R5 | D5 | index-order seeds, FIFO, 8-neighbour order; kept ids by smallest index |

No floating point, no new math function, one noise key, no `Rng`.

Overflow (N ≤ 2048, values inside the ranges of 4): chamfer values < 2^14 at 2048 (INF = 2^40; 32 × INF + 5 < 2^46); ramps divide by b − a > 0 (named checks), every numerator below the denominator; `from_int(rainfall)` ≤ 65535 × 2^32 < 2^49; `from_int(dg)` ≤ 32767 × 2^32; every product of factors ≤ ONE except al ≤ 5 (fertility; mul(mul(fb, sf), al) ≤ 5 × ONE) and gb + fish ≤ 2 × ONE; `from_ratio(hand, 10)` with hand ≤ 30000; em ≤ 3276.7 m; sc ≤ 4 × 1000 × 255 < 2^20, so `from_ratio(sc, 255 Σw)` ≤ ONE; walkability and links in `U128` as G8 (S2 < 2^33, so S2 × 2^64 < 2^97; |ΔS| ≤ 65535 + 32767, (ΔS)² × 2^64 < 2^98; (160 × raw)² × 2 < 2^96 for raw ≤ 10 × 2^32); `mul_int(hab_flood_hand_m, 10)` ≤ 1000 × 2^32.

## 8. Performance

**Budget (P1-00 11, unchanged): 0.2 / 1 / 4 s at 512 / 1024 / 2048.** Cost: one G7 pass (as P1-04), two chamfer transforms (as P1-02 H7, sequential, linear), 3 `noise2` octaves per land cell (the biome jitter evaluates 8 per cell and the whole biome stage runs in 0.24 s at 1024), one BFS, per-cell arithmetic. Memory: about 8 bytes per cell of outputs plus `int64` work grids for hand, the two chamfers and the labels (about 32 bytes per cell, 130 MB at 2048; df and dg may be held as `int32` with INF replaced by 2^30, whose `metres` also saturates to 32767). The exact-integer Python prototype is no guide to the C++ time.

## 9. Report and render

**`report`**, appended to `[geography]` after the biome keys (P1-00 10.3) when all seven layers exist. "Dry land" = cells with `water` ∈ {0, 1} (requires `water`; without it the group prints no key); "land" = `elevation` > 0, as P1-00 10.3. Percentiles by the rule of P1-00 10.3, `n/a` over no values; `ratio4` as there.

| Key | Value |
| --- | --- |
| `scores_p50` | `fertility:<p50> ore:<p50> timber:<p50> fresh_water:<p50> forage:<p50> habitability:<p50>`, medians over dry land, integers, separated by single spaces |
| `scores_p90` | the same with the 90th percentiles |
| `fertile_share` | ratio4(dry cells with `fertility` ≥ 166, dry cells) (166 = q255(0.65), below flat forest's 179) |
| `habitable_share` | ratio4(dry cells with `habitability` ≥ 64, dry cells) |
| `ore_rich_share` | ratio4(dry cells with `ore` ≥ 128, dry cells) |
| `walk_areas` | the largest `walk_area` value (0 if none) |
| `walk_main_share` | ratio4(cells of the most common non-zero `walk_area` value, land cells); `0.0000` if there is no walk area |
| `walk_share` | ratio4(cells with `walk_area` > 0, land cells) |

A score layer that is not `u8`, or `walk_area` that is not `u16`, or any of them not the size of `elevation`, is a corrupt-data error: `worldgen-cli: error: corrupt layer '<name>': type <t>, expected <u8|u16>` or `… size <W>x<H>, expected <N>x<N>`, exit 3, nothing on stdout (the form of P1-04 9). Example (512 seed 0, prototype): `scores_p50 = fertility:0 ore:2 timber:38 fresh_water:102 forage:128 habitability:0`, `scores_p90 = fertility:178 ore:104 timber:255 fresh_water:226 forage:184 habitability:110`, `fertile_share = 0.2046`, `habitable_share = 0.2062`, `ore_rich_share = 0.0866`, `walk_areas = 4`, `walk_main_share = 0.1826`, `walk_share = 0.4286`.

**Statistics** (P1-00 13.1). Measured by the spec writer's exact prototype on the 0.9.0 validation worlds (110: 512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–9), with the data of section 4. Typical = measured min–max widened by about 10% (rounded outward); hard only where a value outside means a defect. Comparison is exact (P1-00 13.1). The 2048 ranges rest on ten worlds and should be widened by the validation sweep (2048 seeds 0–19). The implementation's validation sweep confirms them; the prototype's arithmetic is that of this spec, so a difference is a defect of one of the two, not noise.

| Statistic | All seeds (hard) | Typical (≥ 90%), 512 / 1024 / 2048 | Measured min / median / max, 512 / 1024 / 2048 |
| --- | --- | --- | --- |
| `habitable_share` | ≥ 0.05 | 0.13–0.62 / 0.29–0.71 / 0.49–0.79 | 0.143 / 0.371 / 0.562; 0.323 / 0.485 / 0.644; 0.550 / 0.654 / 0.715 |
| `fertile_share` | descriptive | 0.14–0.65 / 0.29–0.75 / 0.52–0.82 | 0.161 / 0.371 / 0.592; 0.322 / 0.507 / 0.682; 0.578 / 0.678 / 0.747 |
| `ore_rich_share` | > 0 (every world has a rich deposit) | 0.005–0.13 / 0.015–0.09 / 0.016–0.056 | 0.0061 / 0.052 / 0.122; 0.0165 / 0.046 / 0.079; 0.0178 / 0.022 / 0.051 |
| `walk_areas` | ≥ 1 | 1–7 / 1–8 / 2–12 | 1 / 2 / 6; 1 / 3 / 7; 3 / 5.5 / 10 |
| `walk_main_share` | 512 descriptive; ≥ 0.30 at 1024 and 2048 | ≥ 0.16 / ≥ 0.43 / ≥ 0.64 | 0.183 / 0.560 / 0.767; 0.487 / 0.679 / 0.829; 0.720 / 0.825 / 0.889 |
| `walk_share` | descriptive | ≥ 0.35 / ≥ 0.52 / ≥ 0.66 | 0.389 / 0.580 / 0.767; 0.586 / 0.703 / 0.833; 0.736 / 0.836 / 0.891 |
| `scores_p50` habitability | descriptive | — | 0 / 18 / 72; 8 / 62 / 83; 69 / 82 / 90 |
| `scores_p90` habitability | ≥ 1 | 70–160 at every size | 82 / 134 / 144; 127 / 141 / 148; 143 / 147 / 151 |
| `scores_p90` fresh_water | descriptive | — | 224–235 / 231–235 / 235–241 |
| `scores_p50` fertility, forage, timber | descriptive | — | fertility 0–178 / 35–178 / 178; forage 88–128 / 90–128 / 128; timber 13–70 / 38–255 / 38–255 |

Per-biome means over dry land (median over worlds, 512 / 1024 / 2048; fertility, forage, timber, habitability): forest 99–167, 143–151, 213–250, 58–99; grassland 160–229, 88–96, 33–37, 66–94; steppe 89–152, 101–107, 10–13, 37–66; wet woodland about 145, 242, 178, 127–129; marsh about 147, 240, 77, 59–65; fen about 79, 196, 51, 57–59; moor and heath 1–5, 53–69, 2–10, 0–1.4; alpine 0, 13–16, 0, 0. Steep 512 worlds (seeds 0 and 2) have about 60% of dry land at or steeper than 25°, so habitability 0 there. Percentiles in this table were taken by the sweep as the ⌈pn/100⌉-th value, one rank from the report's rule; the bounds' margins cover that.

**Palette `score`** (any `u8` layer; registered under `auto` for `fertility`, `ore`, `timber`, `fresh_water`, `forage`, `habitability`), interpolation rule of P1-00 10.2, listed by `render --help`:

| t | 0 | 64 | 128 | 192 | 255 |
| --- | --- | --- | --- | --- | --- |
| RGB | 68,1,84 | 59,82,139 | 33,145,140 | 94,201,98 | 253,231,37 |

Example: t = 100 → (44, 117, 140): R = 59 + floor((2·(33 − 59)·36 + 64)/128) = 59 − 15 = 44 (floor of −1808/128 = −14.125 → −15); G = 82 + floor((2·63·36 + 64)/128) = 82 + 35 = 117; B = 139 + floor((2·1·36 + 64)/128) = 139 + 1 = 140. `--palette score` on a layer of another type is a usage error (exit 2) whose message says the palette needs a `u8` layer. Water cells are drawn by their value like any other (0, or 255 for lakes in `fresh_water`); no composite is added.

`walk_area` is registered with `ids` (P1-00 10.2), which accepts `u16`.

**Viewer (for the viewer engineer).** The headless check compares every layer the report lists with `render`; the viewer must therefore draw the six score layers with `score` and `walk_area` with `ids` before this stage merges, or the check fails.

## 10. Tests required

Library seams (interim; the engineer may add helpers, and records the final signatures in this section after implementation, as P1-04 10): `resource_ramp(x, a, b)`, `resource_q255(s)`, `slope_tangent(S2)` (G8, shared with P1-04), `chamfer_metres(grid, source)`, `fresh_water_cell(df, rainfall, tuning)`, `fertility_cell(fb, s2, hand, tuning)`, `timber_cell(tb, s2, tuning)`, `forage_cell(gb, dg, tuning)`, `ore_cell(elevation, s2, noise, tuning)`, `walk_areas(grid, S, water, s2, walk_slope_max, walk_min_cells) → (ids, count)`, `habitability_cell(F, W, G, T, O, s2, hand, walk_area, tuning)`, `resources_layers(inputs, definitions, tuning, seed, threads)`. Each raises `FatalError` on a contract violation (wrong array length, threads outside [1, 256], a violated precondition).

### 10.1 Unit tests

1. Every worked example of 5.1 exactly, through the seams.
2. The ore noise at 16 cells of seed 0, N = 64, recomputed by the test with `fbm2` (P0-01 8) and K_ore.
3. Walk areas: examples R5 (a) to (c), including the diagonal limit at 158 and 159; renumbering after dropping small components (ids dense and ordered by smallest index); a world whose only walkable component is below the minimum has `walk_area` 0 everywhere and `walk_areas = 0`, `walk_main_share = 0.0000` in `report`.
4. Named checks: each ordered pair equal and reversed fails with its message at the definition's header; all four weights 0 fails; each field one step outside its bounds is a P0-03 range error; a biome file without `forage` is a missing-field error at the definition header; a `forage` of `"1.5"` is a range error.
5. Preconditions of section 2, each fatal with its message: a missing `rainfall`; a `biome` value with no definition; a `water` value of 4; a `river` value of 16; G7's failures through the stage's prefix (one case).
6. The loaded biome definitions: the twelve shipped `forage` values and steppe's `fertility` 0.7 equal 4.1; the `biome` table of a generated world is unchanged (it carries no `forage` column).
7. Report: the eight keys on a hand-made 8 × 8 world, recomputed by the test; the group is absent when any of the seven layers is missing; each corrupt-layer case exits 3 with its message.
8. Render: `score` on t = 0, 64, 100, 128, 192, 255 (the colours of 9); `--palette score` on `elevation` exits 2; `render --help` lists `score`.

### 10.2 Property tests

Seeds 0 to 63 at N = 128 (fixtures; all earlier stages): invariants 1 to 5 by independent code (its own chamfer by Dijkstra over the 5/7 graph, its own union–find for walk areas, its own R6 from the stored layers, its own G7 by recursion).

### 10.3 Determinism tests

Threads 1, 2, 5: identical layers. Resume: a world written after `biomes` (`--until biomes`), read back, and the stage run on the decoded layers equals the uninterrupted pipeline (P1-00 X2's form). P1-00 X1: the default run writes ids 1 to 18 with `stages` = `elevation,hydrology,climate,biomes,resources`; `--until resources` the same; `--until regions` exits 2 until regions exists.

### 10.4 Golden values

`worldgen/tests/golden/p1-05-resources.txt`, written by the test writer's independent reference in `worldgen/tests/reference/` for the cases of P1-04 10.4 (the N = 128 fixture cases and 512 seed 0, 512 seed 42, 512 "The Unmapped", 1024 seed 1 with the shipped data): per case the layer hash (P0-04 8) of each of the seven layers, and the SHA-256 of `hand` (k = 1), `df` and `dg` as grids of `int64` little-endian in cell index order with no header (the encoding of P1-04 10.4), and of the ore noise `fbm_m(K_ore, X, Y, ore_noise)` raw on every cell. The large cases are read by the `*-slow` suites.

**Version and golden re-recording.** `GENERATOR_VERSION` becomes **0.10.0** (`worldgen/include/worldgen/version.hpp`). Every geography world changes (seven new layers), and every world, the P0 cases included, changes its `content_hash` (new `worldgen.resources`, new biome field, steppe's value; P1-00 6.4): `p0-worlds.txt` and `p1-worlds.txt` are re-recorded by the validation specialist after the cross-preset comparison. The golden vectors of P1-01 to P1-04 (`p1-01-elevation.txt` … `p1-04-biomes.txt`) hold layer and table hashes, which this change must leave **unchanged**; a difference there is a defect. `tools/ci/world-hashes.sh` needs no new case (the geography cases run every implemented stage); its timings in `CLAUDE.md` grow by this stage's share.

## 11. Calibration and known limitations

### 11.1 How it was calibrated

An exact-integer prototype (`~/.cache/the-unmapped/out/p1-05-spec/tools/proto.py`, numpy `int64` with exact Q32.32 products and divisions; its chamfer, G7, G8 and `fbm_m` checked against the repository references `p1_hydrology_reference.chamfer`, `p1_biomes_reference.hand_fifo` and `slope_s2`, and `p1_elevation_reference.fbm_m` on random cells) ran this spec on the stored 0.9.0 layers of the validation sweep. Per-world statistics: `~/.cache/the-unmapped/out/p1-05-spec/sweep/final-*.jsonl`. Contact sheet of every layer for 512 seeds 0 and 28, 1024 seeds 1 and 3, 2048 seeds 0 and 5: `~/.cache/the-unmapped/out/p1-05-spec/sheets/p1-05-layers.png` (columns: biomes, habitability, fertility, forage, timber, ore, fresh_water, walk_area; palette `score`, `ids` for walk areas). The worlds the 0.9.0 validation singled out behave: 512 seed 28 (no land below 0 °C) scores like its neighbours (`habitable_share` 0.296, `ore_rich_share` 0.070); the four 512 worlds without fen lose nothing the bounds depend on. Variants tried (512, 50 worlds):

| Ore variant | `ore_elev` (m) | `ore_vein` | `ore_lowland` | `ore_rich_share` min / median / max | worlds with no cell ≥ 128 |
| --- | --- | --- | --- | --- | --- |
| first draft | 200–1200 | 0.05–0.25 | 0.1 | 0 / 0.020 / 0.059 | 1 (seed 1; largest value 112) |
| lower ramp | 100–800 | 0.05–0.25 | 0.1 | 0.0025 / 0.033 / 0.093 | 0 |
| ramp 150–900, veins from 0 | 150–900 | 0–0.25 | 0.1 | 0.0020 / 0.035 / 0.093 | 0 |
| **shipped** | 100–800 | 0–0.2 | 0.15 | 0.0061 / 0.052 / 0.122 | 0 |

Other choices: `fresh_reach_m` 400 (72–94% of dry land lies within 400 m of a river or lake at 0.9.0's river threshold, so 600 m made fresh water nearly uniform); `alluvial_bonus` 0.3 (0.5 lifted flat forest by a river to 255, the same as grassland); weights 55 / 20 / 15 / 10 so that grassland by a river (177) and forest by a river (207) are the best sites and dry steppe far from water stays poor (about 30).

### 11.2 Limitations

1. **Steep worlds.** On 512 worlds such as seeds 0 and 2, about 60% of the dry land is at or steeper than 25° and habitability is 0 there; the main walk area holds as little as 18% (seed 0) of land. This is the elevation stage's steep-512 limitation (P1-01, critic A5), not a defect here; the 512 walk bounds are descriptive.
2. **Uplands score low.** Moor and heath lie mostly on slopes above 25° at 0.9.0 (median slope above 500 m is about 45°), so their mean fertility and habitability are near 0 although their biome values are 0.3 and 0.2; their forage remains. Upland grazing and shielings are history's to place from `forage`, the biome and race preference.
3. **One generic habitability**, suited to a farming people. Race preferences (dwarves in ore-rich, steep, walled-off ground; elves in forest; faeries in wild places) are history's, from the component layers.
4. **Fresh water ignores streams below the river threshold** (0.125 km²), except through the rain term.
5. **Walk areas are coarse about passes.** A range is a barrier where its flanks are steeper than 35° everywhere; a single walkable gully joins two areas. Natural passes (P1-01 9.1) are mostly not walkable end to end, so the main lowland is usually one area and high valleys and cliff-bound pockets are others.
6. **No ore kinds.** One `ore` score; iron, copper, silver or gold would be history's or the runtime's draw from it, keyed by location.
7. **Fish water is the sea, lakes and rivers of class ≥ 4**, with no distinction between them; coasts are rich in forage regardless of the shore's form (cliffs included: slope still zeroes habitability there).
8. **Fen and marsh** score as forage-rich, poor-farming, flood-prone land: habitability about 60 (median of world means 57–65), a homeland only for a people who weight forage. Fen is under 0.1% of land on some 512 worlds; nothing here assumes it exists.
9. **The moor/heath boundary may move** (a possible small biome follow-up). Their values are close (fertility 0.3 / 0.2, forage 0.25 / 0.2) and both are mostly steep, so no bound of section 9 depends on their shares; the statistics are re-confirmed by the validation sweep.
10. **Straight artefacts of the elevation stage show through.** 512 seed 28 has a straight diagonal scarp (P1-01, "a few straight gullies") that cuts a straight dark band through habitability, fertility and timber; nothing here smooths it.

## 12. Open questions and decisions of taste (for the lead and Paul)

1. **A fifth score, `forage`** (catalogue addition; new biome field). Recommendation: approve; it is how wetlands, forests and coasts get the value the critic asked for without pretending marsh is farmland, and history needs a second food source for fishing and wild peoples. Proposals entry suggested in the report.
2. **Steppe fertility 0.7** (data change in the biome files). Recommendation: approve (critic).
3. **Absolute scores** (unchanged from the first draft). Recommendation: approve.
4. **Habitability: water as a factor, no coast bonus, weights 55 / 20 / 15 / 10, water minimum 0.25.** Taste; history reweights by race.
5. **Fertile threshold 0.65** for `fertile_share` (critic: below forest's 0.7). A report statistic only; history is free to use its own.
6. **Small walkable pockets** (below 0.25 km²) are no walk area and keep a quarter of their habitability. Recommendation: approve.
7. **Timber without a rain term.** Approve; the biome encodes rain.

**Schema and catalogue impact (for the lead):** no schema change (version 4). Catalogue: new layer `forage` (`u8`, id 16); `habitability`'s wording; ids shift so that `region` is 19. Data: new `worldgen` id `resources` (file `game/data/worldgen/resources.toml`), new required field `forage` of kind `biome`, steppe `fertility` 0.7. One stream name, `geo.resources.ore`. Generator 0.10.0.
