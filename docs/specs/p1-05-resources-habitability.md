# P1-05 Resources and habitability

Status: draft, spec writer, 2026-11-16, awaiting approval. Phase P1 Geography, stage 5 (`resources`). Design doc section 4 (stage 5: "per-cell scores for fertility, ore, timber and fresh water. The history simulation uses these to place peoples"), 5 (settlements grow "from local resources"; dwarves "build in places the others avoid"; civilizations build roads and passes). Depends on P0-01 to P0-04, P1-00 (G1 to G8, D1 to D10), P1-02, P1-03 and P1-04 (the `biome` layer and the biome definitions' `fertility` and `timber`). Proposals "High peaks may be inaccessible; history opens the passes". Tuning values marked "guess" and every statistics range are starting points (section 11). Amended 2026-11-18 for the biome variety revision (P1-04 third version, generator 0.9.0): the biome set and its `fertility` and `timber` values (R4 reads them from the definitions, so no algorithm changes) and limitation 6, which the calibration must take into account.

## 1. Purpose

Score every cell for what history needs to place and grow settlements: farmland (`fertility`), metal (`ore`), wood (`timber`), drinking water (`fresh_water`), and one combined `habitability` for a generic settlement site. Mark which land is connected on foot (`walk_area`), because the ranges are walls (P1-01: 11–34% of land steeper than 35°) and history must know which lowlands are reachable from which without a pass.

Two facts about the terrain shape the scores:

- **Steep ground is unusable.** Fertility and habitability fall to 0 between 10° and 25° of slope; ground steeper than 35° is not walkable at all and belongs to no walk area.
- **Ranges separate lowlands.** Walk areas are the connected pieces of walkable land. History treats two settlements in different walk areas as needing a pass (which it can build), a boat or a long detour. A small walkable pocket (a high valley, a coastal pocket behind cliffs, P1-01 limitation 6) is its own area or none, and its habitability is reduced: few people settle where nobody can walk in, though dwarves may (race preferences are history's, from these layers).

Scores are on an **absolute scale**: 255 is the best the scoring function can give, the same on every map, so a poor world scores low everywhere. (P1-00's catalogue said "best on this map's scale"; this spec replaces that reading, see section 12.)

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation`, `water`, `water_depth`, `flow_dir`, `coast_dist` | P1-01, P1-02 |
| `rainfall` | P1-03 |
| `biome`, biome definitions (`fertility`, `timber`) | P1-04 |
| Tuning | definition `worldgen.resources` (section 4) |
| World seed, N | |

Stream: K_ore = `stream_key(seed, "geo.resources.ore")`, a noise key (D1).

Preconditions (fatal "resources: input precondition: …"): the layers present, N × N, catalogue types; every `biome` value is the code of a loaded definition; G7's preconditions.

## 3. Outputs

Six layers, N × N, codec 1, in this order in one transaction (layer ids 12 to 17 in a full run):

| Layer | Type | Ocean | Lake | Land (`water` 0 or 1) |
| --- | --- | --- | --- | --- |
| `fertility` | `u8` | 0 | 0 | R4 |
| `ore` | `u8` | 0 | 0 | R4 |
| `timber` | `u8` | 0 | 0 | R4 |
| `fresh_water` | `u8` | 0 | 255 | R3 (255 on river cells) |
| `habitability` | `u8` | 0 | 0 | R6 |
| `walk_area` | `u16` | 0 | 0 | R5: id of the walk area, 0 if the cell is not walkable or its area is too small |

**Catalogue changes (for the lead; additions and clarifications, not schema changes under P1-00 4 rule 3):**

1. **New layer `walk_area`**: `u16`, walk-area id 1, 2, … in order of each area's smallest cell index; 0 = water, a cell steeper than `walk_slope_max`, or a walkable component smaller than `walk_min_area_m2`. Written by `resources`. Recorded in `docs/specs/schema.md`.
2. **Meaning of the four scores and `habitability` made definite:** 0 = none, 255 = the maximum of the scoring function of this spec; absolute, not normalized per map; 0 on ocean; on lake cells 0 except `fresh_water` 255. P1-00 4 listed the definitions as open, so this fixes an open item rather than changing a fixed one; the catalogue text changes from "255 = best on this map's scale" to "255 = best on the absolute scale of P1-05".

No table, so no schema version change.

## 4. Tuning

Fields of `worldgen.resources`, all required, in declaration order. Slopes are tangents (0.18 ≈ 10°, 0.47 ≈ 25°, 0.7002 ≈ 35°, 1.0 = 45°, 1.4 ≈ 54°).

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `fresh_reach_m` | int(16, 10000) | 600 | guess | distance from a river or lake at which its contribution falls to 0 |
| `fresh_rain_weight` | fx(0, 1) | "0.4" | guess | largest contribution of rain (wells, springs) |
| `fresh_rain_lo_mm`, `fresh_rain_hi_mm` | int(0, 10000) | 300, 1500 | guess | rain ramp for that contribution |
| `fert_slope_lo`, `fert_slope_hi` | fx(0, 10) | "0.18", "0.47" | guess | fertility falls from full to 0 between these slopes |
| `alluvial_bonus` | fx(0, 4) | "0.5" | guess | extra fertility on the floodplain |
| `alluvial_hand_m` | fx("0.1", 100) | 5 | guess | height above drainage at which the bonus is gone |
| `timber_rain_lo_mm`, `timber_rain_hi_mm` | int(0, 10000) | 300, 1200 | guess | timber grows from 0 to full between these rainfalls |
| `timber_slope_lo`, `timber_slope_hi` | fx(0, 10) | "0.7", "1.4" | guess | timber becomes unharvestable between these slopes |
| `ore_noise` | `fbm_m` | wavelength 1500, octaves 3, gain "0.5" | guess | where deposits lie |
| `ore_vein_lo`, `ore_vein_hi` | fx(−1, 1) | "0.05", "0.25" | guess | noise ramp: below lo no ore, above hi a full deposit |
| `ore_elev_lo_m`, `ore_elev_hi_m` | int(0, 3000) | 200, 1200 | guess | exposure ramp with height |
| `ore_lowland` | fx(0, 1) | "0.1" | guess | least exposure factor (lowland deposits) |
| `ore_slope_base` | fx(0, 1) | "0.5" | guess | exposure factor on flat ground |
| `ore_slope_lo`, `ore_slope_hi` | fx(0, 10) | "0.3", "1.0" | guess | slope ramp from base to full exposure |
| `walk_slope_max` | fx("0.01", 10) | "0.7002" | designed (35°, the report's walkability statistic) | steeper cells are not walkable |
| `walk_min_area_m2` | int(16384, 1000000000) | 250000 | guess | smallest walk area kept (977 cells) |
| `hab_slope_lo`, `hab_slope_hi` | fx(0, 10) | "0.18", "0.47" | guess | habitability falls from full to 0 between these slopes |
| `hab_weight_fertility`, `hab_weight_fresh_water`, `hab_weight_timber`, `hab_weight_ore` | int(0, 1000) | 45, 30, 15, 10 | guess | weights of the four scores |
| `hab_flood_hand_m` | fx(0, 100) | 1 | guess | below this height above drainage a site floods |
| `hab_flood_factor` | fx(0, 1) | "0.5" | guess | habitability factor on flood-prone cells |
| `hab_coast_bonus` | fx(0, 1) | "0.2" | guess | extra habitability at the sea coast (harbour, fishing) |
| `hab_coast_reach_m` | int(16, 32767) | 300 | guess | distance over which the coast bonus falls to 0 |
| `hab_pocket_factor` | fx(0, 1) | "0.25" | guess | habitability factor on cells in no walk area |

(Paired fields are listed together; each is its own field, declared in the order shown.)

Named checks: `ramps_ordered` — each lo < hi pair (`fresh_rain`, `fert_slope`, `timber_rain`, `timber_slope`, `ore_vein`, `ore_elev`, `ore_slope`, `hab_slope`) has lo strictly below hi (message `<name>_lo must be below <name>_hi`, one error per pair); `hab_weights_positive` — the four weights sum to at least 1.

```toml
# game/data/worldgen/resources.toml and the fixtures under worldgen/tests/data/geo*/worldgen/
[worldgen.resources]
fresh_reach_m = 600
fresh_rain_weight = "0.4"
fresh_rain_lo_mm = 300
fresh_rain_hi_mm = 1500
fert_slope_lo = "0.18"
fert_slope_hi = "0.47"
alluvial_bonus = "0.5"
alluvial_hand_m = 5
timber_rain_lo_mm = 300
timber_rain_hi_mm = 1200
timber_slope_lo = "0.7"
timber_slope_hi = "1.4"
ore_noise = { wavelength_m = 1500, octaves = 3, gain = "0.5" }
ore_vein_lo = "0.05"
ore_vein_hi = "0.25"
ore_elev_lo_m = 200
ore_elev_hi_m = 1200
ore_lowland = "0.1"
ore_slope_base = "0.5"
ore_slope_lo = "0.3"
ore_slope_hi = "1.0"
walk_slope_max = "0.7002"
walk_min_area_m2 = 250000
hab_slope_lo = "0.18"
hab_slope_hi = "0.47"
hab_weight_fertility = 45
hab_weight_fresh_water = 30
hab_weight_timber = 15
hab_weight_ore = 10
hab_flood_hand_m = 1
hab_flood_factor = "0.5"
hab_coast_bonus = "0.2"
hab_coast_reach_m = 300
hab_pocket_factor = "0.25"
```

## 5. Algorithm

Notation of P1-01 5.0; X, Y as in P1-03 5. Helper functions, `Fx`:

```
ramp(x, a, b)  (a < b) = x ≤ a ? 0 : x ≥ b ? ONE : div(x − a, b − a)
q255(s)              = clamp(round_int(mul_int(clamp(s, 0, ONE), 255)), 0, 255)       // to u8
em                   = from_ratio(elevation, 10)                                       // metres
hm                   = from_ratio(hand, 10)                                            // metres
tan                  = the slope tangent of G8
```

"Land" in this section means `water` ∈ {0, 1} (dry land and river cells); lake and ocean cells take the values of section 3. Steps R1 to R7.

### R1. Preconditions

Section 2.

### R2. Drainage height, slope (P1-00 G7, G8)

hand (dm) by G7 with minimum river class k = 1 (every river cell, lake and ocean cell is a drain; P1-00 7.7; P1-04 uses k = 3); S2 and tan by G8.

### R3. Fresh water (D4 chamfer, then D1)

```
d  = chamfer(source = cells with water ∈ {1, 2})       // P1-02 H7's two-pass 5/7 transform; INF = 2^40
dm = min(32767, (32 × d + 5) div 10)                   // metres, as coast_dist (INF saturates)
fs = (dm ≥ fresh_reach_m) ? 0 : from_ratio(fresh_reach_m − dm, fresh_reach_m)
fr = mul(fresh_rain_weight, ramp(from_int(rainfall), from_int(fresh_rain_lo_mm), from_int(fresh_rain_hi_mm)))
fresh_water = q255(max(fs, fr))                        // river and lake cells: dm = 0, so 255
```

### R4. Fertility, timber, ore (D1, G6 row blocks)

For each land cell, with fb and tb the `fertility` and `timber` of the cell's biome definition:

```
sf  = ONE − ramp(tan, fert_slope_lo, fert_slope_hi)
al  = ONE + mul(alluvial_bonus, ONE − ramp(hm, 0, alluvial_hand_m))
fertility = q255(min(ONE, mul(mul(fb, sf), al)))

rf  = ramp(from_int(rainfall), from_int(timber_rain_lo_mm), from_int(timber_rain_hi_mm))
st  = ONE − ramp(tan, timber_slope_lo, timber_slope_hi)
timber = q255(mul(mul(tb, rf), st))

eu  = max(ore_lowland, ramp(em, from_int(ore_elev_lo_m), from_int(ore_elev_hi_m)))
su  = ore_slope_base + mul(ONE − ore_slope_base, ramp(tan, ore_slope_lo, ore_slope_hi))
vn  = ramp(fbm_m(K_ore, X, Y, ore_noise), ore_vein_lo, ore_vein_hi)
ore = q255(mul(mul(eu, su), vn))
```

Fertility is the biome's farming value, reduced on slopes and raised on low floodplain; timber is the biome's wood, growing with rain and lost on crags; ore is exposure (height and steepness) times a patchy deposit noise, so mountains carry most of it in veins and lowlands a little.

### R5. Walk areas (D5)

```
walkable(c)  = water(c) ∈ {0, 1}  and  S2(c) × 2^64 ≤ (320 × walk_slope_max.raw)²          (U128)
linked(a, b) = walkable(a) and walkable(b) and |S(a) − S(b)|² × 2^64 ≤ w × (160 × walk_slope_max.raw)²
               with w = 1 for an orthogonal and 2 for a diagonal neighbour, S = elevation + water_depth (dm)
```

Label the components of walkable cells under `linked` like G2 (scan cells in index order; each unlabelled walkable cell starts a component, collected by FIFO BFS visiting neighbours in the 8-neighbour order; an edge is followed only if `linked`). The step condition stops a component from crossing a cliff edge between two cells that each look flat by central differences (G8 does not see a cell's own height). Then, with walk_min_cells = (`walk_min_area_m2` + 255) div 256: components with fewer cells get 0; the others get ids 1, 2, … in order of their smallest cell index. An id above 65535 is fatal (impossible: each kept component has at least 64 cells and the map at most 2^22 cells with ocean on its edge).

### R6. Habitability (D1, G6 row blocks)

For each land cell, from the u8 values written by R3 and R4 and the id of R5:

```
sc  = hab_weight_fertility × fertility + hab_weight_fresh_water × fresh_water
      + hab_weight_timber × timber + hab_weight_ore × ore                       // int64
b   = from_ratio(sc, 255 × (sum of the four weights))
u   = ONE − ramp(tan, hab_slope_lo, hab_slope_hi)
h   = mul(b, u)
if from_int(hand) < mul_int(hab_flood_hand_m, 10):  h = mul(h, hab_flood_factor)
cbf = ONE + mul(hab_coast_bonus, ONE − ramp(from_int(max(coast_dist, 0)), 0, from_int(hab_coast_reach_m)))
h   = mul(h, cbf)
if walk_area = 0:  h = mul(h, hab_pocket_factor)
habitability = q255(min(ONE, h))
```

Using the stored u8 scores makes `habitability` reproducible from the stored layers alone. A cell steeper than `hab_slope_hi` has habitability 0, so every unwalkable cell does (`hab_slope_hi` < `walk_slope_max` with the shipped values); the pocket factor therefore acts on small walkable pockets.

### R7. Write

`fertility`, `ore`, `timber`, `fresh_water`, `habitability`, `walk_area`.

### 5.1 Worked examples

Computed by the spec writer's exact-integer reference (P1-03 5.1), shipped tuning unless stated.

**Helpers.** ramp(0.3, 0.18, 0.47) = raw 1777227848 (0.413793). q255: HALF → 128; 0.762 → 194; 0.001 → 0; 0.002 → 1; ONE → 255. Slope tangent from S2: 0 → 0; 7 → raw 35510673 (0.008268); 3318 → raw 773122075 (0.180007); 9216 → ONE × 0.3 = raw 1288490189; 22118 → raw 1996102368 (0.464754); 50205 → raw 3007345802 (0.700202); 102400 → ONE; 10^6 → raw 13421772800 (3.125). The walkable bound with `walk_slope_max` "0.7002" (raw 3007336101) is S2 ≤ 50204, matching the report's "steeper than 35°" (S2 ≥ 50205).

**R3.** One river cell at (4, 1) on a 9 × 3 grid: chamfer rows `22 17 12 7 5 7 12 17 22`, `20 15 10 5 0 5 10 15 20`, `22 17 12 7 5 7 12 17 22`; metres `70 54 38 22 16 22 38 54 70`, `64 48 32 16 0 16 32 48 64`, `70 54 …`. fresh_water by (dm, rainfall): (0, 800) 255; (16, 800) 248; (38, 800) 239; (64, 800) 228; (300, 800) 128; (600, 800) 43 (rain only: 0.4 × 500/1200, 42.50000002 rounds up); (599, 200) 0 (fs = 1/600 gives 0.43, rounds to 0); (1000, 1500) 102; (1000, 900) 51; (1000, 2500) 102.

**R4 fertility** (fb, S2, hand dm → value): (1, 9216, 20) → 194 (sf raw 2517739448 = 0.586207, al raw 5583457485 = 1.3); (1, 0, 0) → 255 (capped: 1.5); (0.7, 22118, 100) → 3 (sf raw 77697452); (0.45, 0, 5) → 166 (al 1.45); (1, 30000, 0) → 0 (tan 0.541 > 0.47); (0.3, 3318, 49) → 77.

**R4 timber** (tb, rainfall, S2): (1, 900, 9216) → 170 (rf 2/3); (1, 1500, 102400) → 146 (st raw 2454267026 = 0.571429); (0.15, 650, 0) → 15; (1, 250, 0) → 0; (1, 1200, 250000) → 0 (tan 1.5625).

**R4 ore** (elevation dm, S2, noise n): (8000, 102400, 0.2) → 115 (eu 0.6, su 1, vn raw 3221225471 = 0.75); (8000, 102400, 0.3) → 153; (500, 0, 0.3) → 13 (eu = `ore_lowland` 0.1, su 0.5); (15000, 20000, 0.1) → 38 (su raw 2582937308, vn raw 1073741825); (8000, 102400, 0.04) → 0.

**R6 habitability** (fertility, fresh_water, timber, ore, S2, hand dm, coast_dist, walk_area → value):

| Inputs | b (raw) | u (raw) | flood | cbf (raw) | pocket | habitability |
| --- | --- | --- | --- | --- | --- | --- |
| 194, 228, 170, 0; 9216; 20; 1000; 1 | 3051953232 (18120/25500) | 2517739448 | no | ONE | no | 106 |
| 255, 255, 15, 0; 0; 5; 100; 1 | 3259122242 | ONE | × 0.5 | 4867629602 (1.1333) | no | 110 |
| as above, walk_area 0 | | | × 0.5 | | × 0.25 | 27 |
| 120, 43, 200, 60; 3318; 300; 16; 3 | 1733145627 | 4294870875 | no | 5108147771 | no | 122 |
| 200, 200, 200, 200; 30000; 300; 5000; 1 | 3368601801 | 0 | | | | 0 |

**R5.** (a) A 5 × 5 test grid, all land (`water` 0), elevation 20 everywhere except 300 at the centre (2, 2). S2 is 78400 at the centre's four orthogonal neighbours (not walkable) and 0 elsewhere, including the centre. The centre is walkable but its diagonal links fail (|300 − 20| = 280 dm; 280² × 2^64 > 2 × (160 × 3007336101)², i.e. 280 > 158.4), so it is a component of 1 cell. With walk_min_cells 2: `walk_area` is 1 on the 20 outer and diagonal cells and 0 on the centre and its four neighbours.
(b) A 9 × 5 grid, rows y = 0 to 4: `20 21 22 23 400 401 402 403 404`, `20 21 22 23 400 401 402 403 404`, `21 22 23 24 401 402 403 404 405`, `21 22 23 24 401 402 403 404 405`, `22 23 24 25 402 403 404 405 406`. Columns 3 and 4 have S2 ≥ 142884 (the 37.7 m step); with walk_min_cells 2, `walk_area` is 1 on columns 0 to 2, 0 on columns 3 and 4, and 2 on columns 5 to 8: two lowlands that history must join by a pass.

## 6. Invariants

1. Every score is in [0, 255]; all six layers are 0 on ocean; on lake cells all are 0 except `fresh_water` = 255; `fresh_water` = 255 on every river cell.
2. `habitability` = 0 wherever tan ≥ `hab_slope_hi`; `fertility` = 0 wherever tan ≥ `fert_slope_hi` or the biome's `fertility` is 0.
3. `walk_area` > 0 only on walkable cells; every kept component has at least walk_min_cells cells and is connected under `linked`; two `linked` cells have the same `walk_area`; ids are 1 to K, each used, increasing with each area's smallest index.
4. `habitability` is the R6 function of the stored `fertility`, `fresh_water`, `timber`, `ore`, `walk_area` and the inputs (rechecked from the written layers).
5. Pure function of the seed, N, the inputs and the tuning; independent of thread count.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| R2 | D5 (G7), D1 (G8) | |
| R3 chamfer | D4 | P1-02 H7's passes |
| R3 rest, R4, R6 | D1, G6 row blocks | location-keyed noise |
| R5 | D5 | index-order seeds, FIFO, 8-neighbour order; kept ids by smallest index |

Overflow: ramps divide by b − a > 0 (named checks); `from_ratio(sc, …)` with sc ≤ 4 × 1000 × 255; every product of factors ≤ 1 except al ≤ 5 and cbf ≤ 2; U128 comparisons as P1-04 7 (|ΔS| ≤ 30000, so |ΔS|² × 2^64 < 2^94; (160 × raw)² × 2 < 2^94 for raw ≤ 10 × 2^32).

## 8. Performance

Budget (P1-00 11): 0.2 / 1 / 4 s. Cost: 3 `noise2` octaves per land cell (R4), two linear passes (G7, chamfer), one BFS (R5), per-cell arithmetic. Memory: about 10 bytes per cell of outputs plus `int64` work grids for the chamfer and labels.

## 9. Report and render

**`report`**, appended to `[geography]` when the six layers are present ("dry land" = `water` ∈ {0, 1}; "land" = `elevation` > 0 as in P1-00 10.3):

| Key | Value |
| --- | --- |
| `scores_p50` | `fertility:<p50> ore:<p50> timber:<p50> fresh_water:<p50> habitability:<p50>`, medians over dry land, integers |
| `scores_p90` | the same with the 90th percentiles |
| `habitable_share` | ratio4(dry-land cells with `habitability` ≥ 64, dry-land cells) |
| `walk_areas` | the largest `walk_area` value |
| `walk_main_share` | ratio4(cells of the most common non-zero `walk_area` value, land cells) |
| `walk_share` | ratio4(cells with `walk_area` > 0, land cells) |

**Statistics** (P1-00 13.1; **provisional**, from the floating-point prototype of P1-03 section 11, whose habitability lacked the coast and pocket factors; reset from the first implementation sweep, measured min–max widened by about 10% for typical, hard bounds only for defects):

| Statistic | All seeds (hard) | Typical (≥ 90%) | Prototype |
| --- | --- | --- | --- |
| `walk_main_share` | ≥ 0.30 | ≥ 0.50 | 0.55–0.83 (main area with 0.5 km² minimum) |
| `walk_share` | descriptive | — | 0.62–0.84 |
| `walk_areas` (1024) | ≥ 1 | 1–20 | 1–2 areas of at least 0.5 km² |
| `habitable_share` | descriptive | — | not measured |
| `scores_p50` habitability | descriptive | — | 0–74 |
| `scores_p90` ore | ≥ 1 (some ore exists) | — | not measured |

**Palette `score`** (any `u8` layer; registered under `auto` for `fertility`, `ore`, `timber`, `fresh_water`, `habitability`), interpolation rule of P1-00 10.2:

| t | 0 | 64 | 128 | 192 | 255 |
| --- | --- | --- | --- | --- | --- |
| RGB | 68,1,84 | 59,82,139 | 33,145,140 | 94,201,98 | 253,231,37 |

`walk_area` is registered with `ids` (P1-00 10.2).

## 10. Tests required

Library seams (interim): `resource_ramp`, `resource_q255`, `slope_tangent(S2)` (G8, shared), `fresh_water_layer(...)`, `resource_cell(...)` for R4, `walk_areas(S, water, tuning) → (ids, count)`, `habitability_cell(...)`, `resources_layers(inputs, definitions, tuning, seed, threads)`.

### 10.1 Unit tests

1. Every worked example of 5.1 exactly.
2. The ore noise term at 16 cells of seed 0, N = 64, recomputed by the test with `fbm2`.
3. Walk areas: the step condition alone (two walkable flats one cell apart at a height difference exactly at and one above the orthogonal and the diagonal limits); renumbering after dropping small components (ids stay dense and ordered by smallest index).
4. Named checks: each ramp pair equal or reversed fails; all four weights 0 fails.

### 10.2 Property tests

Seeds 0 to 63 at N = 128 (fixtures; all earlier stages): invariants 1 to 4 by independent code (its own chamfer by Dijkstra, its own union–find for walk areas, its own R6 from the stored layers).

### 10.3 Determinism tests

Threads 1, 2, 5; resume from a world written after biomes.

### 10.4 Golden values

Layer hashes of the six layers for the cases of P1-03 10.4.

## 11. Known limitations and calibration

1. **Calibration** is the floating-point prototype of P1-03 section 11. Its habitability was zero on 17–60% of land (slopes) and the main walk area held 55–83% of land; scores are otherwise unmeasured.
2. **One generic habitability.** It suits a farming people. Race preferences (dwarves in ore-rich, steep, walled-off ground; elves in forest) are history's, computed from the component layers.
3. **Fresh water ignores streams below the river threshold**, except through the rain term. The threshold is 0.125 km² as shipped since 2026-11-16 (it was 0.5 km² when the prototype of P1-03 section 11 calibrated `fresh_reach_m` and the provisional statistics); with about four times as many river cells `fresh_water` and habitability are higher near streams, and the calibration needs re-doing.
4. **Walk areas are coarse about passes.** A range is a barrier if its flanks are steeper than 35° everywhere along it; a single walkable gully through it joins two areas. P1-01 has no natural passes, so in practice the main lowland is one area and high valleys and cliff-bound pockets are others (P1-01 limitation 1, proposals "Natural passes through the ranges").
5. **No ore kinds.** One `ore` score; iron, copper, silver or gold would be history's or the runtime's draw from it.
6. **The biome mix changed under this spec** (P1-04 third version, 2026-11-18; not yet calibrated here). R4 takes `fertility` and `timber` from the biome definitions, which are now (fertility / timber): forest 0.7 / 1, grassland 1 / 0.15, **steppe 0.6 / 0.05**, **wet woodland 0.5 / 0.7**, marsh 0.45 / 0.3, **fen 0.25 / 0.2**, highland moor 0.3 / 0.1, **heath 0.2 / 0.05**, alpine 0 / 0 (new biomes in bold). Grassland, the only fully fertile biome, falls to about a quarter of land (median share 0.25 / 0.23 / 0.19 at 512 / 1024 / 2048, from about 0.32 / 0.32 / 0.30 in 0.8.0), steppe taking the dry interiors. The world critic's measure with these values (2 km² tiles, mean fertility ≥ 0.7): fertile tiles 0.48 → 0.46 at 512, 0.52 → 0.44 at 1024 and **0.48 → 0.33 at 2048**; mean land fertility 0.66 at every size (`~/.cache/the-unmapped/out/p1-04b-variety/critic-review.md` 4). P1-05's calibration (thresholds, `alluvial_bonus`, the habitability weights and the statistics of section 9) must be done on 0.9.0 worlds, not on the prototype or on 0.8.0. If the 2048 interiors must stay fertile cores, steppe fertility 0.7 to 0.75 is defensible (critic; P1-04 open question 4), a data change in the biome definitions.

## 12. Open questions for the lead

1. **Absolute scores** rather than per-map normalization (catalogue wording change, section 3). Recommendation: approve; history can still normalize within a world, but cannot recover an absolute scale from normalized data.
2. **The `walk_area` layer** (catalogue addition). Recommendation: approve; it is how history learns which lowlands a range separates, and where a pass would join two areas (with P1-06's region borders).
3. **Habitability weights** (taste): farmland 45, water 30, timber 15, ore 10. Recommendation: approve for P1; history will weight by race.
4. **Small walkable pockets** count as no walk area and get a quarter of their habitability. Recommendation: approve; 0.25 km² is about a village and its fields.

**Schema and catalogue impact (for the lead):** no schema change. Catalogue: new layer `walk_area` (`u16`); the meaning of `fertility`, `ore`, `timber`, `fresh_water`, `habitability` fixed as absolute scales (section 3). New `worldgen` id `resources`; one stream name.
