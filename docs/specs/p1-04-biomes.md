# P1-04 Biomes

Status: draft, spec writer, 2026-11-16, awaiting approval. Phase P1 Geography, stage 4 (`biomes`). Design doc section 4 (stage 4: "a lookup on temperature and rainfall, plus special cases for coast, river and high peaks"; the starting biomes), 6 (`region.biome`), 7 (scatter "from biome rules"). Depends on P0-01 to P0-04, P1-00 (G1 to G8, D1 to D10, the `biome` kind of 6.6, the planned table of 9.2), P1-02 and P1-03. Tuning values marked "guess" and every statistics range are starting points (section 11).

## 1. Purpose

Give every cell one biome code from a small set defined in data. The biome layer is what the map, the viewer and the critic look at first; resources (P1-05) take fertility and timber from it; regions (P1-06) take their dominant biome and name from it; history places races by preferred biome (design doc section 5); the runtime scatters vegetation by it (milestone 4).

Design decisions carried from P1-00 6.6 (proposals "Biome categories and the biome table"): the five natural land biomes temperate forest, grassland, marsh, highland moor and alpine, plus `ocean` and `lake` for water, plus `blighted_waste`, which only history assigns. Rivers are sub-cell features and take the biome of their land. The coast, river and high-peak special cases of the design doc are rules that select among these biomes, not separate biomes:

- **High peaks**: alpine wherever the mean annual temperature is at or below the treeline value (about 1,000 m at mid-map with the shipped climate).
- **River and lake banks, and low coasts**: marsh on gently sloping land at most 3 m above the water it drains to (height above drainage, P1-00 G7), so marsh follows valley floors, lake shores and coastal flats alike.
- **Uplands**: highland moor on cool ground above 300 m that is not alpine.
- Otherwise forest where rainfall is at least 700 mm, grassland where drier.

The rules are data in the biome definitions (design doc: biomes are hand-edited TOML), evaluated in a fixed priority order, so a biome can be added or retuned without code.

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation` (dm), `water`, `water_depth`, `flow_dir`, `coast_dist` | P1-01, P1-02 |
| `temperature` (0.1 °C), `rainfall` (mm) | P1-03 |
| Biome definitions | kind `biome`, `game/data/biomes/` (section 4.1) |
| Tuning | definition `worldgen.biomes` (section 4.2) |
| World seed, N | `meta.seed`, `param.map_size` |

Streams: K_tj = `stream_key(seed, "geo.biomes.temp_jitter")` and K_rj = `stream_key(seed, "geo.biomes.rain_jitter")`, noise keys only (D1).

Preconditions (fatal "biomes: input precondition: …"): every input layer present, N × N, catalogue type; the hydrology preconditions of G7 (P1-00 7.7).

## 3. Outputs

| Output | Content |
| --- | --- |
| Layer `biome` (`u8`, layer id 11 in a full run) | the biome `code` of every cell; never 0; `ocean`'s code exactly on `water` = 3, `lake`'s exactly on `water` = 2; a biome whose `priority` is 0 never on a land cell |
| Table `biome` | one row per loaded biome definition (section 5, B5) |

**Schema change (for the lead): schema version 3**, as planned in P1-00 9.2 (version 2 is the elevation revision's `pass` table, P1-01 9.1), made by this stage's branch:

```
CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT
```

Executed after the version 2 DDL in every world (P0-02 5.3), so a world generated with `--until climate` or earlier has an empty `biome` table. `docs/specs/schema.md` gets the DDL and a changelog row; the P1 schema proposals entry is updated (P1-00 appendix B8). The catalogue entry of `biome` is unchanged.

## 4. Data and tuning

### 4.1 The `biome` kind

Registered by this stage's implementation (P1-00 6.6). Fields in schema declaration order (this fixes the canonical encoding, P0-03 8); the first four were fixed by P1-00:

| Field | Type | Meaning |
| --- | --- | --- |
| `code` | int(1, 254) | value in the `biome` layer; unique |
| `name` | string(64) | display name |
| `colour` | int(0, 16777215) | 0xRRGGBB for maps |
| `water` | bool | true for water categories |
| `water_body` | enum("none", "lake", "ocean") | which hydrology water cells get this biome |
| `priority` | int(0, 255) | land rule order, higher first; 0 = never assigned by a rule (water biomes, history-only biomes) |
| `fallback` | bool | assigned to a land cell that no rule matches |
| `rules` | array(record(`biome_rule`), 0, 16) | the biome applies where any of its rules matches |
| `fertility` | fx(0, 1) | agricultural potential of the biome, used by P1-05 |
| `timber` | fx(0, 1) | usable wood of the biome, used by P1-05 |
| `region_noun` | string(32) | noun for region names, used by P1-06 (e.g. `Fen`) |

Record `biome_rule` (all fields required; a condition holds when the value lies in the closed interval):

| Field | Type | Compared with |
| --- | --- | --- |
| `temp_min_c`, `temp_max_c` | fx(−60, 50) | tq, the jittered temperature in °C |
| `rain_min_mm`, `rain_max_mm` | int(0, 10000) | rq, the jittered rainfall in mm |
| `elev_min_m`, `elev_max_m` | int(0, 3000) | `elevation` in metres (compared as 10 × bound against decimetres) |
| `slope_max` | fx(0, 100) | the cell's slope tangent (G8), compared exactly as S2 |
| `hand_max_m` | fx(0, 3000) | height above drainage (G7) |
| `coast_max_m` | int(0, 32767) | `coast_dist` |

Named checks (P0-03 7.5, run after type validation; errors at the definition's header, form as P1-01 4.3, the message given):

1. `biome_code_unique` (P1-00): no two definitions share a `code` — `code <c> is also used by '<other id>'`.
2. `biome_water_body`: `water` is true exactly when `water_body` is not "none" — `water must be true exactly when water_body is not "none"`.
3. `biome_water_unique` (kind-level, reported at the second definition in id order, or against the directory when missing): exactly one definition has `water_body` "ocean" and exactly one "lake" — `exactly one biome must have water_body "<v>"`.
4. `biome_fallback` (kind-level, as 3): exactly one definition has `fallback` true, and it has `water_body` "none" — `exactly one land biome must have fallback = true`.
5. `biome_water_rules`: a definition with `water_body` not "none" has `priority` 0 and no rules — `water biomes take no rules and priority 0`.
6. `biome_priority_unique` (kind-level, as 3): land definitions with `priority` > 0 have distinct priorities — `priority <p> is also used by '<other id>'`.
7. `biome_rule_ranges`: in every rule, `temp_min_c ≤ temp_max_c`, `rain_min_mm ≤ rain_max_mm`, `elev_min_m ≤ elev_max_m` — `rules[<i>]: <field>_min must not exceed <field>_max`.

Shipped definitions, `game/data/biomes/` (shown as one file; one file per biome is the recommended layout, and file names carry no meaning, P0-03 7.4). The fixture copy is `worldgen/tests/data/geo/biomes/` and `worldgen/tests/data/geo-small/biomes/`. Codes, names and colours are those fixed in P1-00 6.6.

```toml
[biome.ocean]
code = 1
name = "Ocean"
colour = 0x183878
water = true
water_body = "ocean"
priority = 0
fallback = false
rules = []
fertility = 0
timber = 0
region_noun = "Sea"

[biome.lake]
code = 2
name = "Lake"
colour = 0x4080CC
water = true
water_body = "lake"
priority = 0
fallback = false
rules = []
fertility = 0
timber = 0
region_noun = "Mere"

[biome.temperate_forest]
code = 3
name = "Temperate forest"
colour = 0x2E6034
water = false
water_body = "none"
priority = 20
fallback = false
fertility = "0.7"
timber = 1
region_noun = "Wood"

[[biome.temperate_forest.rules]]
temp_min_c = -60
temp_max_c = 50
rain_min_mm = 700
rain_max_mm = 10000
elev_min_m = 0
elev_max_m = 3000
slope_max = 100
hand_max_m = 3000
coast_max_m = 32767

[biome.grassland]
code = 4
name = "Grassland"
colour = 0x96AA5A
water = false
water_body = "none"
priority = 10
fallback = true
rules = []
fertility = 1
timber = "0.15"
region_noun = "Downs"

[biome.marsh]
code = 5
name = "Marsh"
colour = 0x5C6E54
water = false
water_body = "none"
priority = 40
fallback = false
fertility = "0.45"
timber = "0.3"
region_noun = "Fen"

[[biome.marsh.rules]]
temp_min_c = -60
temp_max_c = 50
rain_min_mm = 500
rain_max_mm = 10000
elev_min_m = 0
elev_max_m = 200
slope_max = "0.25"
hand_max_m = 3
coast_max_m = 32767

[biome.highland_moor]
code = 6
name = "Highland moor"
colour = 0x786468
water = false
water_body = "none"
priority = 30
fallback = false
fertility = "0.3"
timber = "0.1"
region_noun = "Moor"

[[biome.highland_moor.rules]]
temp_min_c = -60
temp_max_c = 4
rain_min_mm = 0
rain_max_mm = 10000
elev_min_m = 300
elev_max_m = 3000
slope_max = 100
hand_max_m = 3000
coast_max_m = 32767

[biome.alpine]
code = 7
name = "Alpine"
colour = 0xBEBEB4
water = false
water_body = "none"
priority = 50
fallback = false
fertility = 0
timber = 0
region_noun = "Peaks"

[[biome.alpine.rules]]
temp_min_c = -60
temp_max_c = "0.5"
rain_min_mm = 0
rain_max_mm = 10000
elev_min_m = 600
elev_max_m = 3000
slope_max = 100
hand_max_m = 3000
coast_max_m = 32767

[biome.blighted_waste]
code = 8
name = "Blighted waste"
colour = 0x605446
water = false
water_body = "none"
priority = 0
fallback = false
rules = []
fertility = 0
timber = 0
region_noun = "Waste"
```

(In TOML a key such as `rules = []` must come before the definition's `[[…rules]]` tables; definitions with rule tables omit the `rules =` line, since the array-of-tables headers define it. A definition with neither is a missing-field error.)

In words, by priority: alpine (50) at or below 0.5 °C on ground above 600 m; marsh (40) on gentle land (≤ 14°) at most 3 m above its drainage, below 200 m, with at least 500 mm of rain; highland moor (30) at or below 4 °C above 300 m; temperate forest (20) with at least 700 mm; grassland otherwise (fallback). All values are guesses tuned on the prototype (section 11).

### 4.2 Tuning `worldgen.biomes`

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `jitter_noise` | `fbm_m` | wavelength 600, octaves 3, gain "0.5" | guess | scale of boundary irregularity (groves, clearings, ragged treelines) |
| `temp_jitter_c` | fx(0, 10) | "0.8" | guess | °C per unit of noise added to the temperature the rules see |
| `rain_jitter` | fx(0, 1) | "0.15" | guess | relative rainfall change per unit of noise |

```toml
# game/data/worldgen/biomes.toml and the fixtures under worldgen/tests/data/geo*/worldgen/
[worldgen.biomes]
jitter_noise = { wavelength_m = 600, octaves = 3, gain = "0.5" }
temp_jitter_c = "0.8"
rain_jitter = "0.15"
```

Registering the `biome` kind and the `worldgen.biomes` id changes `content_hash` of every world (PATCH bump, P1-00 6.4).

## 5. Algorithm

Notation of P1-01 5.0; X, Y as in P1-03 5. Steps B1 to B5.

### B1. Preconditions

Section 2.

### B2. Drainage height and slope (P1-00 G7, G8)

hand[i] (dm) by G7 from `elevation`, `water`, `water_depth`, `flow_dir`; S2[i] by G8.

### B3. Jittered climate (D1, G6 row blocks)

For each cell with `water` ∈ {0, 1}:

```
tq = from_ratio(temperature, 10) + mul(temp_jitter_c, fbm_m(K_tj, X, Y, jitter_noise))
rq = mul(from_int(rainfall), max(0, ONE + mul(rain_jitter, fbm_m(K_rj, X, Y, jitter_noise))))
```

The jitter is used only for the lookup; the climate layers are not changed.

### B4. Classification (D1, G6 row blocks)

```
water = 3:  biome = code of the definition with water_body "ocean"
water = 2:  biome = code of the definition with water_body "lake"
otherwise:  for each land definition with priority > 0, in descending priority:
                if any rule r of it matches (all of):
                    r.temp_min_c ≤ tq ≤ r.temp_max_c
                    from_int(r.rain_min_mm) ≤ rq ≤ from_int(r.rain_max_mm)
                    10 × r.elev_min_m ≤ elevation ≤ 10 × r.elev_max_m
                    S2 × 2^64 ≤ (320 × r.slope_max.raw)²                    (U128, exact; P1-00 G8)
                    from_int(hand) ≤ mul_int(r.hand_max_m, 10)
                    coast_dist ≤ r.coast_max_m
                then biome = its code; stop
            if none matched: biome = code of the fallback definition
```

Rules within a definition are tried in array order; the result does not depend on it (any match selects the definition). Priorities are distinct (named check 6), so the order is total.

### B5. Write

Layer `biome`; then the `biome` table, one row per loaded definition in ascending `code`: `id` = `code`, `key` = definition id, `name`, `colour`, `water` (0 or 1). Both in the stage transaction.

### 5.1 Worked examples

Computed by the spec writer's exact-integer reference (section P1-03 5.1 describes it) and checked by hand against the rules.

**G7 on P1-02's worked example (5.1 there).** Surface S = e + `water_depth`: 0 on the ocean ring, 28 on the four lake cells, e elsewhere. hand (dm), rows y = 1 to 5 (rows 0 and 6 are ocean, 0):

```
  1    0   60   70   80   70   60   50    0
  2    0   50    0    0   32   40   30    0
  3    0   45    0    0   27   35   25    0
  4    0   40    2    0   15   30   20    0
  5    0   35   33   31    0   20   10    0
```

For example (4, 2) drains W into the lake (level 28): 60 − 28 = 32; (2, 4) drains E to the river cell (3, 4) at 28: 30 − 28 = 2; (3, 5) drains S to the ocean: 31 − 0 = 31; drain cells (lake, river, ocean) have 0.

**B4 lookups** with the shipped definitions. Inputs: `temperature` (tenths), `rainfall`, `elevation` (dm), S2, hand (dm), `coast_dist` (m), and the two noise values nt, nr (0 unless given):

| Case | temp | rain | elev | S2 | hand | coast | nt, nr | tq, rq | Biome | Why |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| B1 | 3 | 1200 | 8000 | 200000 | 500 | 3000 | | 0.3, 1200 | alpine | ≤ 0.5 °C, ≥ 600 m |
| B2 | 3 | 1200 | 5000 | 200000 | 500 | 3000 | | 0.3, 1200 | highland_moor | 500 m < 600 m: not alpine |
| B3 | 80 | 900 | 30 | 100 | 12 | 200 | | 8.0, 900 | marsh | |
| B4 | 80 | 900 | 30 | 10000 | 12 | 200 | | 8.0, 900 | temperate_forest | tan 0.3125 > 0.25 |
| B5 | 80 | 650 | 400 | 100 | 100 | 2000 | | 8.0, 650 | grassland | fallback |
| B6 | 80 | 700 | 400 | 100 | 100 | 2000 | | 8.0, 700 | temperate_forest | bound inclusive |
| B7 | 40 | 650 | 3000 | 100 | 100 | 2000 | | 4.0, 650 | highland_moor | bound inclusive |
| B8 | 41 | 650 | 3000 | 100 | 100 | 2000 | | 4.1, 650 | grassland | |
| B9 | 80 | 900 | 30 | 6400 | 30 | 200 | | 8.0, 900 | marsh | S2 = (320 × 0.25)² exactly; hand 3 m inclusive |
| B10 | 80 | 900 | 30 | 6401 | 30 | 200 | | 8.0, 900 | temperate_forest | one above the slope bound |
| B11 | 80 | 900 | 30 | 100 | 31 | 200 | | 8.0, 900 | temperate_forest | hand 3.1 m |
| B12 | 80 | 650 | 400 | 100 | 100 | 2000 | nr 0.5 | 8.0, 698.75 (raw 3001108397950) | grassland | jitter ×1.075 |
| B13 | 80 | 650 | 400 | 100 | 100 | 2000 | nr 0.6 | 8.0, raw 3042984328800 (708.5) | temperate_forest | jitter ×1.09 |
| B14 | 10 | 650 | 6500 | 100 | 100 | 2000 | nt −0.7 | raw 1889785610 (0.44), 650 | alpine | 1.0 − 0.56 |
| B15 | 10 | 650 | 6500 | 100 | 100 | 2000 | nt −0.6 | raw 2233382993 (0.52), 650 | highland_moor | |

`slope_max` "0.25" is raw 1073741824; (320 × raw)² / 2^64 = 6400 exactly, which is why B9 and B10 straddle it.

## 6. Invariants

1. Every cell's `biome` is the `code` of a loaded definition; never 0.
2. `biome` = ocean's code ⟺ `water` = 3; = lake's code ⟺ `water` = 2.
3. No land cell has a biome with `priority` 0 other than the fallback (with the shipped data: `blighted_waste` covers 0 cells).
4. Every land cell's biome is the highest-priority definition with a matching rule, or the fallback if none matches (rechecked by the test with its own rule evaluation).
5. The `biome` table has exactly one row per loaded definition, `id` = `code`, ascending, and its contents equal the definitions.
6. Pure function of the seed, N, the input layers, the biome definitions and `worldgen.biomes`; independent of thread count.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| B2 G7 | D5 | FIFO from drain cells in index order (P1-00 7.7); the result is a function, independent of the order |
| B2 G8 | D1 | |
| B3, B4 | D1, parallel by G6 row blocks | location-keyed noise; rules in a fixed order |
| B5 | sequential | rows in ascending `code` |

Overflow: |tq| ≤ 60 + 10; rq ≤ 10000 × 2; `mul_int(hand_max_m, 10)` ≤ 30000; S2 < 2^33 so S2 × 2^64 < 2^97 and 320 × `slope_max`.raw < 2^47, (·)² < 2^94, both `U128`. hand ≤ 30000.

## 8. Performance

Budget (P1-00 11): 0.1 / 0.5 / 2 s at 512 / 1024 / 2048. Cost per land cell: 6 `noise2` octaves (B3, parallel) and at most 16 × (number of land definitions) rule tests; G7 and G8 are linear. The noise dominates; if the stage misses its budget, evaluate the two jitter fields at a coarser lattice and interpolate (a spec change) or drop to 2 octaves (a tuning change).

## 9. Report and render

**`report`**, appended to `[geography]` when `biome` and the `biome` table are present:

| Key | Value |
| --- | --- |
| `biome_share` | for each `biome` table row with `water` = 0, ascending `id`: `<key>:<ratio4(cells with that code, cells whose biome row has water = 0)>`, separated by single spaces |
| `biome_patches` | the number of 8-connected components (G2, equivalence: equal `biome` value) of cells whose biome row has `water` = 0 |

**Statistics** (P1-00 13.1, replacing its second-pass sentence; **provisional**, from the floating-point prototype of section 11 on 20 worlds; the validation specialist resets them from the first implementation sweep as the measured min–max widened by about 10%):

| Statistic | All seeds (hard) | Typical (≥ 90%) | Prototype, 512 and 1024 |
| --- | --- | --- | --- |
| `biome_share` of each of temperate_forest, grassland, marsh, highland_moor, alpine | ≤ 0.80 | 0.02–0.65 | forest 0.35–0.66; grassland 0.11–0.43; marsh 0.017–0.098; moor 0.05–0.27; alpine 0.04–0.12 |
| `biome_share` blighted_waste | 0 | — | 0 |
| `biome_patches` (1024) | descriptive | — | not measured; a sudden rise means speckle |

P1-00 13.1 said "no natural biome exceeds 0.6"; the prototype's forest reached 0.66 in one world in twenty, so the typical upper bound is 0.65 and the hard bound 0.80 until the sweep. Alpine and moor depend on the world having uplands (proposals "Every world has real uplands"); a world below 0.02 alpine is most likely one with low peaks in the north-south middle (P1-01 limitation 12).

**Palette `biome`** (layer `biome`, `u8`): the colour of the `biome` table row whose `id` equals the value, as 0xRRGGBB; a value with no row, or a world without the table, (255, 0, 255). Registered under `auto`. (This implements P1-00 appendix C item 5.)

**Composite `biomes`** (`--composite biomes`; default output `<stem>.biomes.png`). Needs `elevation`, `biome` and the table; uses `water` and `river` when present. Per cell: ocean (e < 0): the `hypsometric` ocean colour; otherwise the biome colour, shaded exactly as step 2 of the composite `map` (P1-00 10.2); then steps 3 and 4 of `map` (lake colour and the river drawing) when `water` is present. P0-04 amendment for the lead: `--composite` accepts `map`, `biomes` (and `regions`, P1-06).

## 10. Tests required

Library seams (interim): `biome_definitions_from(content)`; `height_above_drainage(S, water, flow_dir)` (G7, shared with P1-05); `biome_lookup(definitions, tq, rq, elevation, S2, hand, coast_dist) → code`; `biome_layer(inputs, definitions, tuning, seed, threads)`.

### 10.1 Unit tests

1. G7 on P1-02's worked example exactly (5.1 above), and its precondition failures: a `flow_dir` cycle on a hand-made grid and a dry cell draining to a cell with higher S are fatal.
2. B4 cases B1 to B15 exactly, through `biome_lookup` with the shipped definitions and the tq, rq given.
3. A definition with two rules: a cell matching only the second rule gets the biome.
4. Named checks 1 to 7: one failing fixture each, with path, line, column and message; a float literal in a rule is a P0-03 error; a rule missing a field is a missing-field error at the rule's table header.
5. The `biome` table of a world generated with the shipped data equals the eight rows of P1-00 6.6; with `--until climate` the table exists and is empty; `SCHEMA_VERSION` is 3.
6. B3 structural: for seed 0, N = 64, at 16 cells chosen by the test, tq and rq equal the test's evaluation with `fbm2`.

### 10.2 Property tests

Seeds 0 to 63 at N = 128 (fixture data, all earlier stages): invariants 1 to 5, with the test's own rule evaluation and its own G7 (recursion along `flow_dir`). Also with a test-only data set in which every biome's rules are the full ranges: every land cell gets the highest-priority biome.

### 10.3 Determinism tests

Threads 1, 2, 5 identical; resume from a world written after climate equals the uninterrupted pipeline.

### 10.4 Golden values

Layer hash of `biome` and the logical hash of table `biome` (P0-04 8) for the cases of P1-03 10.4.

## 11. Known limitations and calibration

1. **Calibration.** The shipped rules were set with the floating-point prototype described in P1-03 section 11 (with no jitter and without the marsh slope limit), on 20 worlds: every one of the five natural biomes covered at least 1.7% of land in every world and at least 2% in 19 of 20; forest reached 66%. Treat every number as a guess for the first sweep.
2. **Marsh depends on the river threshold.** Drainage height is measured to river cells (upstream area ≥ `river_min_area_m2`, 0.125 km² as shipped since 2026-11-16, P1-02), lakes and the sea; the floodplains of smaller streams are not marsh. Lowering `river_min_area_m2` in hydrology increases marsh. **Recalibration needed:** the prototype calibration of item 1 assumed the former 0.5 km² threshold (about four times fewer river cells), so the marsh share and the provisional `biome_share` ranges must be re-measured at the shipped threshold.
3. **Hard thresholds.** Biome edges are isolines of the jittered climate: treelines follow contours (natural in mountains); the forest–grassland edge follows rainfall isolines, which can show the faint wind streaks of P1-03 limitation 3. The 600 m jitter breaks them into groves and clearings at the scale a walker sees.
4. **Slope speckle.** The marsh slope limit uses the per-cell slope, so a marsh edge on a terrace can be ragged at cell scale. `biome_patches` will show if this is a problem.
5. **No transitions or mixtures.** One biome per 16 m cell; ecotones and mixed woodland are the runtime's job (scatter density from the climate layers).
6. **Rainfall thresholds need re-deriving after climate merges** (flagged by the P1-03 revision of 2026-11-17). Climate was retuned on its first implementation (P1-03 11.1: triangle smoothing, `moisture_path_mm_km` 32000, `rain_oro_mm` 2500), so the rainfall the prototype calibration of item 1 assumed has changed, and rainfall now visibly depends on map size (P1-03 limitation 7): land median about 880 / 790 / 680 mm at 512 / 1024 / 2048, land under 700 mm about 11% / 30% / 56%. A fixed 700 mm forest threshold (and the 500 mm marsh minimum) would make a 2048 map much more open than a 512 map. **Recalibration needed:** re-derive `rain_min_mm` and `rain_max_mm` of the shipped rules either per map size or from land rainfall quantiles of the world, and re-measure the `biome_share` ranges, once the climate stage has merged (P1-03 open question 6).

## 12. Open questions for the lead

1. **Biome categories** (taste; Paul should see them). Chosen: P1-00's five natural biomes, nothing more (no desert, tundra, heath or coastal dune). Recommendation: keep five for the prototype; the data-driven rules mean a sixth (for example `heath` for dry uplands) is a data change plus a code, colour and noun.
2. **Rules as data with priorities** instead of a code-side Whittaker table. Recommendation: approve; it keeps the design's "biomes are hand-edited TOML" and lets the critic's fixes be data changes (PATCH bumps).
3. **Marsh defined by height above drainage** (rivers, lakes and the sea alike) rather than by distance to water. Recommendation: approve; it puts marsh on valley floors and coastal flats, not on banks of incised streams.
4. **Alpine by temperature only.** Steep rock at lower heights stays forest, moor or grassland; the runtime can expose rock on steep cells. Recommendation: approve; an alternative "rock" rule (steep and above 900 m → alpine) made alpine swallow whole 512 ranges in the prototype.
5. **Biome fields for later stages** (`fertility`, `timber`, `region_noun`) are defined here so the kind is registered once. Recommendation: approve; P1-05 and P1-06 read them.
6. **Blighted waste** stays a history overlay (P1-00 6.6): history writes its own layer rather than rewriting `biome` (catalogue rule 1). Recommendation: confirm when the history specs are written.

**Schema and catalogue impact (for the lead):** schema version 3 adds table `biome` (DDL in section 3; already planned in P1-00 9.2, appendix B6 and B8). No catalogue change. New data kind `biome` with the fields of 4.1 (P1-00 6.6 updated). New `worldgen` id `biomes`. Two stream names.
