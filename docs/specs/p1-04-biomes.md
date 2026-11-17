# P1-04 Biomes

Status: draft, spec writer, 2026-11-16; **revised 2026-11-17 for implementation** (second version, awaiting approval): recalibrated on the 0.7.0 climate (generator 0.7.0, `coast_rain_mm` 75) instead of the floating-point prototype; per-size rainfall scale; marsh rule and drainage height (G7 with a minimum river class); jitter retuned; river cells take the biome of their banks; schema version 3 also adds the climate stage's one-row table `climate` (the wind). Section 0 lists every change. Phase P1 Geography, stage 4 (`biomes`). Design doc section 4 (stage 4: "a lookup on temperature and rainfall, plus special cases for coast, river and high peaks"; the starting biomes), 6 (`region.biome`), 7 (scatter "from biome rules"). Depends on P0-01 to P0-04, P1-00 (G1 to G8, D1 to D10, the `biome` kind of 6.6, schema 9.2), P1-02 and P1-03. Tuning values marked "calibrated" were set on the 108 worlds of the climate validation sweep (section 11); every statistics range is provisional until the implementation's validation sweep.

## 0. Changes in this revision

| What | Before | Now | Why (section 11) |
| --- | --- | --- | --- |
| Rainfall bounds | fixed mm | each rule's `rain_min_mm` and `rain_max_mm` multiplied by a per-size scale from `worldgen.biomes`: 1.125 / 1 / 0.875 at 512 / 1024 / 2048 (4.2, B4) | rainfall depends on map size (P1-03 limitation 7); fixed bounds gave grassland medians 6% / 19% / 37% |
| Forest | rain ≥ 700 mm | rain ≥ 720 mm × scale (810 / 720 / 630 mm) | grassland median 33 / 32 / 30%, no world above 65% forest |
| Marsh | hand ≤ 3 m, slope ≤ 0.25, below 200 m, rain ≥ 500 mm; hand to every river | hand ≤ 1 m, slope ≤ 0.05, below 200 m, rain ≥ 500 mm × scale; hand measured to rivers of class ≥ 3, lakes and the sea (`drain_river_class_min`, G7 gains a parameter) | the old rule gave 5–17% / 14–28% marsh at 1024 / 2048, mostly beside class 1 and 2 streams; now 1.2 / 2.4 / 5.7% median |
| Jitter | 600 m, 3 octaves, ±0.8 °C, ±15% | 500 m, 4 octaves, ±1 °C, ±20% | edges break into groves and clearings at walking scale; no speckle |
| River cells | classified like dry land (hand 0, so lowland rivers became threads of marsh) | take the most common biome of their dry 8-neighbours (B5) | "rivers are sub-cell features and take the biome of their land" (P1-00 6.6) |
| Inputs | — | `river` added (for G7's class test) | |
| Schema 3 | table `biome` | tables `biome` and `climate` (the climate stage's wind, P1-03 3) | world critic S2; P1-03 open question 4 |
| Alpine, moor, grassland fallback, kind fields, named checks, `blighted_waste`, palette, composite, report keys | | unchanged | re-measured; sound (11) |

## 1. Purpose

Give every cell one biome code from a small set defined in data. The biome layer is what the map, the viewer and the critic look at first; resources (P1-05) take fertility and timber from it; regions (P1-06) take their dominant biome and name from it; history places races by preferred biome (design doc section 5); the runtime scatters vegetation by it (milestone 4).

Design decisions carried from P1-00 6.6 (proposals "Biome categories and the biome table"): the five natural land biomes temperate forest, grassland, marsh, highland moor and alpine, plus `ocean` and `lake` for water, plus `blighted_waste`, which only history assigns. Rivers are sub-cell features and take the biome of their land. The coast, river and high-peak special cases of the design doc are rules that select among these biomes, not separate biomes:

- **High peaks**: alpine wherever the mean annual temperature is at or below the treeline value (about 1,000 m at mid-map with the shipped climate).
- **Valley floors, lake shores, river mouths**: marsh on near-level land at most 1 m above the water it drains to, where that water is a river of some size (class 3, about 0.5 km² upstream), a lake or the sea (height above drainage, P1-00 G7).
- **Uplands**: highland moor on cool ground above 300 m that is not alpine.
- Otherwise forest where rainfall is at least the forest bound for the map size, grassland where drier.

The rules are data in the biome definitions (design doc: biomes are hand-edited TOML), evaluated in a fixed priority order, so a biome can be added or retuned without code.

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation` (dm), `water`, `water_depth`, `flow_dir`, `river`, `coast_dist` | P1-01, P1-02 |
| `temperature` (0.1 °C), `rainfall` (mm) | P1-03 |
| Biome definitions | kind `biome`, `game/data/biomes/` (section 4.1) |
| Tuning | definition `worldgen.biomes` (section 4.2) |
| World seed, N | `meta.seed`, `param.map_size` |

Streams: K_tj = `stream_key(seed, "geo.biomes.temp_jitter")` and K_rj = `stream_key(seed, "geo.biomes.rain_jitter")`, noise keys only (D1).

Preconditions (fatal, message beginning `biomes: input precondition: `; from `generate` exit 1 with no world file and no `.partial`, P0-04 4), in this order:

1. The eight input layers exist, each N × N with its catalogue type (`i16`, `u8`, `u16`, `u8`, `u8`, `i16`, `i16`, `u16`).
2. Every `water` value is in 0..3; every `river` value is in 0..15.
3. The G7 preconditions (P1-00 7.7): every non-drain cell reaches a drain cell along `flow_dir`, and no height above drainage is negative.

## 3. Outputs

| Output | Content |
| --- | --- |
| Layer `biome` (`u8`, layer id 11 in a full run) | the biome `code` of every cell; never 0; `ocean`'s code exactly on `water` = 3, `lake`'s exactly on `water` = 2; a biome whose `priority` is 0 never on a land cell, except the fallback |
| Table `biome` | one row per loaded biome definition (B6) |

**Schema change (for the lead): schema version 3**, as planned in P1-00 9.2 (version 2 is the elevation revision's `pass` table, P1-01 9.1), made by this stage's branch. Two tables, executed in this order after the version 2 DDL in every world (P0-02 5.3):

```
CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT
CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL) STRICT
```

`biome` is written by this stage (B6). `climate` is written by the climate stage (P1-03 3 and C7: one row, `id` 1, `wind_toward` = the C2 code); it is part of this change because the lead folded the wind into the same schema bump (world critic S2, P1-03 open question 4). A world generated with `--until hydrology` or earlier has both tables empty; `--until climate` has one `climate` row and an empty `biome` table. `docs/specs/schema.md` gets both DDL lines and a changelog row; the proposals entry "P1 schema changes" is updated (P1-00 appendix B8). The layer catalogue is unchanged. Everything in the change, for the proposals entry, is listed at the end of section 12.

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
| `rain_min_mm`, `rain_max_mm` | int(0, 10000) | rq, the jittered rainfall in mm, against the bound **scaled for the map size** (B4); 10000 means "no upper limit" and is never scaled |
| `elev_min_m`, `elev_max_m` | int(0, 3000) | `elevation` in metres (compared as 10 × bound against decimetres) |
| `slope_max` | fx(0, 100) | the cell's slope tangent (G8), compared exactly as S2 |
| `hand_max_m` | fx(0, 3000) | height above drainage (G7 with k = `drain_river_class_min`) |
| `coast_max_m` | int(0, 32767) | `coast_dist` |

Named checks (P0-03 7.5, run after type validation; errors at the definition's header, form as P1-01 4.3, the message given):

1. `biome_code_unique` (P1-00): no two definitions share a `code` — `code <c> is also used by '<other id>'`.
2. `biome_water_body`: `water` is true exactly when `water_body` is not "none" — `water must be true exactly when water_body is not "none"`.
3. `biome_water_unique` (kind-level, reported at the second definition in id order, or against the directory when missing): exactly one definition has `water_body` "ocean" and exactly one "lake" — `exactly one biome must have water_body "<v>"`.
4. `biome_fallback` (kind-level, as 3): exactly one definition has `fallback` true, and it has `water_body` "none" — `exactly one land biome must have fallback = true`.
5. `biome_water_rules`: a definition with `water_body` not "none" has `priority` 0 and no rules — `water biomes take no rules and priority 0`.
6. `biome_priority_unique` (kind-level, as 3): land definitions with `priority` > 0 have distinct priorities — `priority <p> is also used by '<other id>'`.
7. `biome_rule_ranges`: in every rule, `temp_min_c ≤ temp_max_c`, `rain_min_mm ≤ rain_max_mm`, `elev_min_m ≤ elev_max_m` — `rules[<i>]: <field>_min must not exceed <field>_max`.

Shipped definitions, `game/data/biomes/` (shown as one file; one file per biome is the recommended layout, and file names carry no meaning, P0-03 7.4). The fixture copies are `worldgen/tests/data/geo/biomes/` and `worldgen/tests/data/geo-small/biomes/`. Codes, names and colours are those fixed in P1-00 6.6. Rule values differing from the first version are marked in the comments.

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
rain_min_mm = 720        # was 700
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
slope_max = "0.05"       # was "0.25"
hand_max_m = 1           # was 3
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

(In TOML a key such as `rules = []` must come before the definition's `[[…rules]]` tables; definitions with rule tables omit the `rules =` line, since the array-of-tables headers define it. A definition with neither is a missing-field error. The comments are not part of the shipped files.)

In words, by priority: alpine (50) at or below 0.5 °C on ground above 600 m; marsh (40) on near-level land (tangent ≤ 0.05, about 3°) at most 1 m above its drainage, below 200 m, with at least 500 mm × scale of rain; highland moor (30) at or below 4 °C above 300 m; temperate forest (20) with at least 720 mm × scale; grassland otherwise (fallback). With the shipped scale the forest bound is 810 / 720 / 630 mm and the marsh minimum 562.5 / 500 / 437.5 mm at 512 / 1024 / 2048.

### 4.2 Tuning `worldgen.biomes`

Fields in schema declaration order, all required (P1-00 6.1):

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `jitter_noise` | `fbm_m` | wavelength 500, octaves 4, gain "0.5" | calibrated | scale of boundary irregularity (groves, clearings, ragged treelines) |
| `temp_jitter_c` | fx(0, 10) | 1 | calibrated | °C per unit of noise added to the temperature the rules see |
| `rain_jitter` | fx(0, 1) | "0.2" | calibrated | relative rainfall change per unit of noise |
| `rain_scale_512` | fx("0.25", 4) | "1.125" | calibrated | factor on every rule's rainfall bounds for N ≤ 512 |
| `rain_scale_1024` | fx("0.25", 4) | 1 | designed (1024 is the reference size) | factor for 512 < N ≤ 1024 |
| `rain_scale_2048` | fx("0.25", 4) | "0.875" | calibrated | factor for N > 1024 |
| `drain_river_class_min` | int(1, 15) | 3 | calibrated | rivers of at least this class (P1-02 H6) are drain cells of G7 for this stage; smaller streams are not |

```toml
# game/data/worldgen/biomes.toml and the fixtures under worldgen/tests/data/geo*/worldgen/
[worldgen.biomes]
jitter_noise = { wavelength_m = 500, octaves = 4, gain = "0.5" }
temp_jitter_c = 1
rain_jitter = "0.2"
rain_scale_512 = "1.125"
rain_scale_1024 = 1
rain_scale_2048 = "0.875"
drain_river_class_min = 3
```

**Why a table per size, not a power law.** Only three map sizes exist; the measured ratios (section 11.2: the bound that gives the same median grassland share is 1.12 / 1 / 0.88 of the 1024 value) are not a clean power of N (700 × (1024/N)^0.22 would give 1.165 / 0.858); a fractional power needs a deferred transcendental (P1-00 12); and three numbers in the tuning file are exact (1.125 and 0.875 are binary fractions, so every scaled bound is an exact `Fx`), visible and individually retunable when climate changes. Test grids (N < 512) use the 512 entry, by the size rule above. Not per-world rainfall quantiles: they would fix each world's grassland share by construction and remove the variety that range placement and wind give (world critic S1).

Registering the `biome` kind and the `worldgen.biomes` id changes `content_hash` of every world (P1-00 6.4); the biomes merge bumps `GENERATOR_VERSION` (MINOR, a new stage and schema 3: 0.8.0) and re-records the golden hashes.

## 5. Algorithm

Notation of P1-01 5.0; X = `from_int(16x + 8)`, Y = `from_int(16y + 8)` (metres). Steps B1 to B6.

### B1. Preconditions

Section 2.

### B2. Drainage height and slope (P1-00 G7, G8)

hand[i] (dm) = G7 with k = `drain_river_class_min` from `elevation`, `water`, `water_depth`, `river`, `flow_dir`; S2[i] by G8.

### B3. Jittered climate (D1, G6 row blocks)

For each cell with `water` ∈ {0, 1}:

```
nt = fbm_m(K_tj, X, Y, jitter_noise)
nr = fbm_m(K_rj, X, Y, jitter_noise)
tq = from_ratio(temperature, 10) + mul(temp_jitter_c, nt)
rq = mul(from_int(rainfall), max(0, ONE + mul(rain_jitter, nr)))
```

The jitter is used only for the lookup; the climate layers are not changed.

### B4. Classification (D1, G6 row blocks)

Once per world: sc = `rain_scale_512` if N ≤ 512, `rain_scale_1024` if 512 < N ≤ 1024, otherwise `rain_scale_2048`; and for a rainfall bound b, rb(b) = `from_int(10000)` if b = 10000, otherwise `mul_int(sc, b)` (exact).

```
water = 3:  biome = code of the definition with water_body "ocean"
water = 2:  biome = code of the definition with water_body "lake"
otherwise:  for each land definition with priority > 0, in descending priority:
                if any rule r of it matches (all of):
                    r.temp_min_c ≤ tq ≤ r.temp_max_c
                    rb(r.rain_min_mm) ≤ rq ≤ rb(r.rain_max_mm)
                    10 × r.elev_min_m ≤ elevation ≤ 10 × r.elev_max_m
                    S2 × 2^64 ≤ (320 × r.slope_max.raw)²                    (U128, exact; P1-00 G8)
                    from_int(hand) ≤ mul_int(r.hand_max_m, 10)
                    coast_dist ≤ r.coast_max_m
                then biome = its code; stop
            if none matched: biome = code of the fallback definition
```

Rules within a definition are tried in array order; the result does not depend on it (any match selects the definition). Priorities are distinct (named check 6), so the order is total. B4 is evaluated for river cells too (`water` = 1); B5 decides whether its result is kept.

### B5. River cells (D1 on the B4 grid, G6 row blocks)

For each cell c with `water` = 1: among its existing 8-neighbours with `water` = 0, count the B4 codes. If there is at least one, biome[c] = the code with the largest count, ties to the smallest code; otherwise biome[c] keeps its B4 value. B5 reads only B4 values of dry cells, which B5 never changes, so it may write the same grid in place, in any order.

### B6. Write

Layer `biome`; then the `biome` table, one row per loaded definition in ascending `code`: `id` = `code`, `key` = definition id, `name`, `colour`, `water` (0 or 1). Both in the stage transaction.

### 5.1 Worked examples

Computed by the spec writer's exact-integer reference (`core_reference.py`, `p1_elevation_reference.py`'s `fbm_m` and `p1_hydrology_reference.py` from `worldgen/tests/reference/`, with B2 to B5 written from this section), checked by hand where marked.

**G7 on P1-02's worked example (5.1 there).** Surface S = e + `water_depth`: 0 on the ocean ring, 28 on the four lake cells, e elsewhere. The two river cells (3, 4) and (4, 5) have class 2. hand (dm), rows y = 1 to 5 (rows 0 and 6 are ocean, 0), first with k = 1 (every river cell a drain; P1-05 uses this):

```
  1    0   60   70   80   70   60   50    0
  2    0   50    0    0   32   40   30    0
  3    0   45    0    0   27   35   25    0
  4    0   40    2    0   15   30   20    0
  5    0   35   33   31    0   20   10    0
```

With k = 3 (the shipped `drain_river_class_min`), the class-2 river cells are not drains; row 4 becomes `0 40 30 28 40 30 20 0` and row 5 `0 35 33 31 25 20 10 0`, rows 1 to 3 unchanged. By hand: (3, 4) drains SE (code 1) to (4, 5), which drains S to the ocean, so hand = 28 − 0; (2, 4) drains E to (3, 4) and on to the ocean: 30; (4, 4) drains SE to (5, 5) and on: 40. With k = 1, (4, 2) drains W into the lake (level 28): 60 − 28 = 32; (2, 4) drains E to the river cell (3, 4) at 28: 30 − 28 = 2.

**B4 lookups** with the shipped definitions. Inputs: N, `temperature` (tenths), `rainfall`, `elevation` (dm), S2, hand (dm), `coast_dist` (m), and the noise values nt, nr (0 unless given):

| Case | N | temp | rain | elev | S2 | hand | coast | nt, nr | tq, rq | Biome | Why |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| B1 | 1024 | 3 | 1200 | 8000 | 200000 | 500 | 3000 | | 0.3, 1200 | alpine | ≤ 0.5 °C, ≥ 600 m |
| B2 | 1024 | 3 | 1200 | 5000 | 200000 | 500 | 3000 | | 0.3, 1200 | highland_moor | 500 m < 600 m: not alpine |
| B3 | 1024 | 80 | 900 | 30 | 100 | 8 | 200 | | 8.0, 900 | marsh | |
| B4 | 1024 | 80 | 900 | 30 | 257 | 8 | 200 | | 8.0, 900 | temperate_forest | one above the slope bound |
| B5 | 1024 | 80 | 900 | 30 | 256 | 10 | 200 | | 8.0, 900 | marsh | S2 256 is the largest not steeper than 0.05; hand 1 m inclusive |
| B6 | 1024 | 80 | 900 | 30 | 100 | 11 | 200 | | 8.0, 900 | temperate_forest | hand 1.1 m |
| B7 | 1024 | 80 | 900 | 2001 | 100 | 5 | 200 | | 8.0, 900 | temperate_forest | 200.1 m > 200 m |
| B8 | 1024 | 80 | 719 | 400 | 100 | 100 | 2000 | | 8.0, 719 | grassland | fallback |
| B9 | 1024 | 80 | 720 | 400 | 100 | 100 | 2000 | | 8.0, 720 | temperate_forest | bound inclusive |
| B10 | 1024 | 40 | 650 | 3000 | 100 | 100 | 2000 | | 4.0, 650 | highland_moor | bound inclusive |
| B11 | 1024 | 41 | 650 | 3000 | 100 | 100 | 2000 | | 4.1, 650 | grassland | |
| B12 | 1024 | 80 | 650 | 400 | 100 | 100 | 2000 | nr 0.5 | 8.0, 715.0 (raw 3070901616900) | grassland | jitter × 1.1 |
| B13 | 1024 | 80 | 650 | 400 | 100 | 100 | 2000 | nr 0.6 (raw 2576980378) | 8.0, raw 3126736191150 (728.0) | temperate_forest | jitter × 1.12 |
| B14 | 1024 | 10 | 650 | 6500 | 100 | 100 | 2000 | nt −0.5 | raw 2147483648 (0.5), 650 | alpine | 1.0 − 0.5, inclusive |
| B15 | 1024 | 10 | 650 | 6500 | 100 | 100 | 2000 | nt −0.4 (raw −1717986918) | raw 2576980378 (0.6), 650 | highland_moor | |
| B16 | 512 | 80 | 800 | 400 | 100 | 100 | 2000 | | 8.0, 800 | grassland | forest bound 1.125 × 720 = 810 |
| B17 | 512 | 80 | 810 | 400 | 100 | 100 | 2000 | | 8.0, 810 | temperate_forest | |
| B18 | 2048 | 80 | 630 | 400 | 100 | 100 | 2000 | | 8.0, 630 | temperate_forest | 0.875 × 720 = 630 |
| B19 | 2048 | 80 | 438 | 30 | 100 | 8 | 200 | | 8.0, 438 | marsh | marsh minimum 0.875 × 500 = 437.5 |
| B20 | 2048 | 80 | 437 | 30 | 100 | 8 | 200 | | 8.0, 437 | grassland | below 437.5 and below 630 |
| B21 | 1024 | 80 | 499 | 30 | 100 | 8 | 200 | | 8.0, 499 | grassland | marsh needs 500 |

`slope_max` "0.05" is raw 214748365; (320 × raw)² / 2^64 = 256.0000005, so S2 = 256 is not steeper and 257 is (B4, B5). `rain_scale_512` "1.125" is raw 4831838208 and `rain_scale_2048` "0.875" raw 3758096384, so every scaled bound of the shipped rules is exact. In B12 f = ONE + mul(0.2, 0.5) = 1.1 (raw 4724464026), 650 × f = raw 3070901616900.

**B5.** A river cell whose dry neighbours have codes 3, 3, 4, 4, 5 gets 3 (two each of 3 and 4; the tie goes to the smaller code); with 4, 4, 4, 3 it gets 4; a river cell with no dry neighbour (for example between a lake and another river cell on every side) keeps its B4 code.

**Real cells** (cross-check on a generated world: 1024 seed 3, generator 0.7.0, shipped data; layers decoded from the world file, hand by G7 with k = 3, S2 by G8, nt and nr by the exact reference; the same cells evaluated by the spec writer's calibration model agree):

| Cell | `water` | temp | rain | elev | S2 | hand | coast | nt (raw) | nr (raw) | tq (raw) | rq (raw) | Biome |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| (375, 469) | 0 | 58 | 716 | 1401 | 9250 | 262 | 1936 | 23557280 | 120289931 | 24934367597 (5.806) | 3092422101912 (720.011) | temperate_forest (just over 720) |
| (612, 635) | 0 | 68 | 665 | 1297 | 17 | 718 | 2819 | −773053270 | 126360917 | 28432724343 (6.620) | 2872959253535 (668.913) | grassland |
| (496, 597) | 0 | 64 | 662 | 1222 | 20 | 0 | 4470 | 371464994 | −710571975 | 27859255688 (6.487) | 2749188620462 (640.095) | marsh (a lake margin: hand 0) |
| (514, 767) | 0 | 25 | 590 | 8659 | 139634 | 3695 | 1866 | −1090468039 | −442498318 | 9646950201 (2.246) | 2481815902880 (577.843) | highland_moor |
| (313, 451) | 0 | −13 | 748 | 12129 | 174941 | 9549 | 992 | 98162261 | 904479069 | −5485295224 (−1.277) | 3347945606280 (779.504) | alpine |
| (571, 380) | 1 | 59 | 725 | 1140 | 1 | 67 | 2352 | 1293082526 | −319437946 | 26633389572 (6.201) | 3067532787575 (714.216) | B4 grassland; class 1 river; dry neighbours 4, 4, 4, 4, 4, 3, 3 → B5 grassland |

## 6. Invariants

1. Every cell's `biome` is the `code` of a loaded definition; never 0.
2. `biome` = ocean's code ⟺ `water` = 3; = lake's code ⟺ `water` = 2.
3. No land cell has a biome with `priority` 0 other than the fallback (with the shipped data: `blighted_waste` covers 0 cells).
4. Every dry land cell's (`water` = 0) biome is the highest-priority definition with a matching rule, or the fallback if none matches; every river cell's biome is B5's mode of its dry neighbours' biomes, or its own B4 result if it has none (rechecked by the test with its own rule evaluation).
5. The `biome` table has exactly one row per loaded definition, `id` = `code`, ascending, and its contents equal the definitions.
6. Pure function of the seed, N, the input layers, the biome definitions and `worldgen.biomes`; independent of thread count.
7. With `rain_scale_512` = `rain_scale_1024` = `rain_scale_2048` the result does not depend on which entry applies (only N's use in the scale rule; everything else in B2 to B5 is independent of N).

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| B2 G7 | D5 | FIFO from drain cells in index order (P1-00 7.7); the result is a function, independent of the order |
| B2 G8 | D1 | |
| B3, B4 | D1, parallel by G6 row blocks | location-keyed noise; rules in a fixed order |
| B5 | D1 on the finished B4 grid, parallel by G6 row blocks | reads dry cells only, writes river cells only; all of B4 finishes before B5 starts |
| B6 | sequential | rows in ascending `code` |

Overflow: |tq| ≤ 60 + 10; rq ≤ 10000 × 2; rb ≤ 4 × 10000 (`mul_int` raw ≤ 4 × 2^32 × 10^4 < 2^48); `mul_int(hand_max_m, 10)` ≤ 30000; S2 < 2^33 so S2 × 2^64 < 2^97 and 320 × `slope_max`.raw < 2^47, (·)² < 2^94, both `U128`. hand ≤ 30000. B5 counts ≤ 8.

## 8. Performance

Budget (P1-00 11): 0.1 / 0.5 / 2 s at 512 / 1024 / 2048. Cost per non-water cell (about 40% of cells): 8 `noise2` evaluations (two 4-octave fields, B3, parallel) and at most 16 × (number of land definitions) rule tests; G7, G8 and B5 are linear. For comparison the climate stage, with 6 octaves on every cell plus the sweep, takes 0.24 / 0.8 / 2.9 s (P1-03 validation), so biomes should land near 0.1 / 0.4 / 1.5 s. If it misses its budget, drop the jitter to 3 octaves (a tuning change; the finest octave then is 125 m, edges somewhat smoother) or evaluate the two fields on a coarser lattice and interpolate (a spec change).

## 9. Report and render

**`report`**, appended to `[geography]` after the climate keys when `biome` and the `biome` table are present:

| Key | Value |
| --- | --- |
| `biome_share` | for each `biome` table row with `water` = 0, ascending `id`: `<key>:<ratio4(cells with that code, cells whose biome row has water = 0)>`, separated by single spaces |
| `biome_patches` | the number of 8-connected components (G2, equivalence: equal `biome` value) of cells whose biome row has `water` = 0 |

`[tables]` lists `biome = <rows>` and `climate = <rows>` in ascending name order with the others (every world of schema 3 has both tables, also `--until none`: `biome = 0`, `climate = 0`).

**Statistics** (P1-00 13.1, replacing the prototype rows; **provisional**). Measured by the spec writer on the 108 worlds of the climate validation sweep (512 and 1024 seeds 0–49, 2048 seeds 0–7; generator 0.7.0) with a calibration model of B2 to B5 (section 11.1). The validation specialist resets them from the implementation's sweep, typical = measured min–max widened by about 10%:

| Statistic | All seeds (hard) | Typical (≥ 90%), 512 / 1024 / 2048 | Measured min / median / max, 512 / 1024 / 2048 |
| --- | --- | --- | --- |
| `biome_share` temperate_forest | ≤ 0.80 | 0.25–0.67 at every size | 0.272 / 0.425 / 0.611; 0.347 / 0.448 / 0.635; 0.489 / 0.498 / 0.566 |
| `biome_share` grassland | ≤ 0.80 | 0.15–0.64 / 0.15–0.49 / 0.23–0.42 | 0.165 / 0.326 / 0.585; 0.166 / 0.317 / 0.446; 0.261 / 0.299 / 0.382 |
| `biome_share` marsh | ≤ 0.15 | 0.001–0.028 / 0.008–0.043 / 0.030–0.071 | 0.001 / 0.012 / 0.025; 0.009 / 0.024 / 0.039; 0.034 / 0.057 / 0.064 |
| `biome_share` highland_moor | ≤ 0.80 | 0.035–0.32 / 0.035–0.28 / 0.030–0.12 | 0.041 / 0.132 / 0.287; 0.043 / 0.105 / 0.253; 0.033 / 0.064 / 0.105 |
| `biome_share` alpine | ≤ 0.80 | 0.025–0.14 at every size | 0.040 / 0.075 / 0.123; 0.038 / 0.077 / 0.126; 0.028 / 0.042 / 0.104 |
| `biome_share` blighted_waste | 0 | — | 0 |
| `biome_patches` | descriptive | — | 53 / 82 / 123; 228 / 302 / 393; 1040 / 1321 / 1498 (a sudden rise means speckle) |

Every natural biome is present (≥ 0.1% of land) in every one of the 108 worlds; the smallest share is marsh at 512 seed 10 (0.12%). No biome exceeds 0.65 in any world. The hard marsh bound 0.15 marks the defect the first rules had (14–28% at 2048).

**Palette `biome`** (layer `biome`, `u8`): the colour of the `biome` table row whose `id` equals the value, as 0xRRGGBB; a value with no row, or a world without the table, (255, 0, 255). Registered under `auto`. (This implements P1-00 appendix C item 5.)

**Composite `biomes`** (`--composite biomes`; default output `<stem>.biomes.png`). Needs `elevation`, `biome` and the table; uses `water`, `river` and `flow_dir` when present. Per cell: ocean (e < 0): the `hypsometric` ocean colour; otherwise the biome colour, shaded exactly as step 2 of the composite `map` (P1-00 10.2); then steps 3 and 4 of `map` (lake colour and the river drawing) when `water` is present. P0-04 amendment for the lead: `--composite` accepts `map`, `biomes` (and `regions`, P1-06). `render --help` lists every palette `--palette` accepts, `biome` included (P1-00 10.2).

## 10. Tests required

Library seams (interim; the engineer may add helpers): `biome_definitions_from(content)`; `biome_tuning_from(content)`; `height_above_drainage(grid, S, water, river, flow_dir, k)` (G7, shared with P1-05, which passes k = 1); `biome_rain_scale(tuning, n)`; `biome_lookup(definitions, scale, tq, rq, elevation, S2, hand, coast_dist) → code`; `biome_river_cells(grid, water, biome)` (B5); `biome_layer(inputs, definitions, tuning, seed, threads)`.

### 10.1 Unit tests

1. G7 on P1-02's worked example exactly with k = 1 and k = 3 (5.1), and its precondition failures: a `flow_dir` cycle on a hand-made grid and a dry cell draining to a cell with higher S are fatal; with k = 16 on a grid whose only drains are rivers, a cell that reaches no drain is fatal.
2. B4 cases B1 to B21 exactly, through `biome_lookup` with the shipped definitions, the scale of the case's N, and the tq, rq given.
3. The size rule: `biome_rain_scale` gives the 512 entry for N = 16, 128 and 512, the 1024 entry for 513 and 1024, the 2048 entry for 1025 and 2048; a bound of 10000 is not scaled (a rule with `rain_max_mm` 10000 matches rq = 9999 at scale 0.875).
4. B5 examples of 5.1, and the real-cell example (the test may build the 3 × 3 neighbourhood by hand).
5. A definition with two rules: a cell matching only the second rule gets the biome.
6. Named checks 1 to 7: one failing fixture each, with path, line, column and message; a float literal in a rule is a P0-03 error; a rule missing a field is a missing-field error at the rule's table header. Tuning: each bound of 4.2 rejects a value one step outside it.
7. The `biome` table of a world generated with the shipped data equals the eight rows of P1-00 6.6; with `--until climate` the table exists and is empty; `SCHEMA_VERSION` is 3; the `sqlite_schema` rows of an empty world list `biome` and `climate` with the DDL of section 3 byte for byte.
8. B3 structural: for seed 0, N = 64, at 16 cells chosen by the test, tq and rq equal the test's evaluation with `fbm2`.
9. Preconditions of section 2, each fatal with the prefix: a missing `river` layer, a `river` value of 16, a `water` value of 4.
10. Report: `biome_share` and `biome_patches` on a hand-made 6 × 6 world with a `biome` table, recomputed by the test; `[tables]` lists `biome` and `climate`.

### 10.2 Property tests

Seeds 0 to 63 at N = 128 (fixture data, all earlier stages): invariants 1 to 5, with the test's own rule evaluation, its own G7 (recursion along `flow_dir` with the class test) and its own B5. Also with a test-only data set in which every biome's rules are the full ranges: every dry land cell gets the highest-priority biome. Invariant 7 on seed 0 with the three scales set equal.

### 10.3 Determinism tests

Threads 1, 2, 5 identical; resume from a world written after climate equals the uninterrupted pipeline.

### 10.4 Golden values

Layer hash of `biome` and the logical hash of table `biome` (P0-04 8) for the cases of P1-03 10.4 (fixture tuning; N = 512 seeds 0, 42 and "The Unmapped", 1024 seed 1, 128 seed 0), in `worldgen/tests/golden/p1-04-biomes.txt`, written by the test writer's independent reference and checked by the validation specialist against the implementation (as P1-03's climate vectors in `p1-03-climate.txt`). The CI world cases (`p1-worlds.txt`) are re-recorded by the biomes merge.

## 11. Known limitations and calibration

### 11.1 How the rules were calibrated (2026-11-17)

On real layers, not the prototype: the 108 worlds of the climate validation sweep (`~/.cache/the-unmapped/out/p1-03-validation/`, generator 0.7.0, `coast_rain_mm` 75), decoded with numpy. G7 (with the class test) and G8 are exact integer computations on the stored layers; the jitter fields use the exact lattice hashes of `noise2` with floating-point interpolation (agreement with the exact `fbm_m` to about 10^-9 at test points); B4 and B5 as written. Shares are of cells with `water` ∈ {0, 1}. The six real cells of 5.1 were evaluated both ways and agree. Scripts and contact sheets: `~/.cache/the-unmapped/out/p1-04-spec/` (`tools/`, `sheets/final-1024-s0-19.png`, `sheets/final-512-s0-9.png`, `sheets/final-2048-s0-7.png`, crops `sheets/final-crop-*.png`, and the variants tried in `logs/`).

The lead's target picture and what the shipped values give (min / median / max over the worlds of a size):

| Target | 512 (50) | 1024 (50) | 2048 (8) | Met? |
| --- | --- | --- | --- | --- |
| every biome present in most worlds | all five in all 50 | all five in all 50 | all five in all 8 | yes |
| grassland roughly 15–35%, with spread | 16.5 / 32.6 / 58.5% | 16.6 / 31.7 / 44.6% | 26.1 / 29.9 / 38.2% | medians yes; 21 / 16 / 2 worlds above 35% (dry-interior and weak-shadow worlds) |
| forest roughly 35–55% | 27.2 / 42.5 / 61.1% | 34.7 / 44.8 / 63.5% | 48.9 / 49.8 / 56.6% | medians yes; 11 / 6 / 2 worlds outside |
| alpine + moor follow the ranges | moor 4.1 / 13.2 / 28.7%, alpine 4.0 / 7.5 / 12.3% | moor 4.3 / 10.5 / 25.3%, alpine 3.8 / 7.7 / 12.6% | moor 3.3 / 6.4 / 10.5%, alpine 2.8 / 4.2 / 10.4% | yes (rings on every range) |
| marsh 1–6% | 0.1 / 1.2 / 2.5% | 0.9 / 2.4 / 3.9% | 3.4 / 5.7 / 6.4% | 1024 yes; 18 of 50 worlds at 512 under 1%; 3 of 8 at 2048 slightly over 6% |
| no biome above about 65% | max 61.1% | max 63.5% | max 56.6% | yes |
| rain shadows read as grassland in the lee | lee lowland 48 / 85 / 100% grass, windward 0 / 16 / 59% | lee 49 / 81 / 99%, windward 0 / 17 / 42% | lee 64 / 86 / 99%, windward 3 / 30 / 56% | yes |

(Lee and windward lowland as in P1-03 11.1: land, not lake, below 200 m, lee if tracing against the wind for up to 400 cells meets ground of 800 m or more.)

### 11.2 Variants tried

| Variant | Change | Result |
| --- | --- | --- |
| old rules, fixed mm | first version of this spec | reproduces the critic's trial (1024 s0–3: grass 18–30%, marsh 5–14%; 2048 s1, s5: marsh 20–26%) |
| marsh grid | hand 1 / 1.5 / 2 / 3 m × slope 0.03 / 0.05 / 0.1 × drain class ≥ 1 to 6 (dry cells only) | marsh at k ≥ 1, hand 3 m, slope 0.1: 5.4 / 10.0 / 20.8% median; k ≥ 3, 1 m, 0.05: 1.3 / 2.2 / 5.2%; k ≥ 4, 1.5 m, 0.05: 1.1 / 2.3 / 5.9%. Every setting gives about twice the share per doubling of the side: 2048 lowland is 89% of land and flatter (median lowland slope 0.04 against 0.07 at 1024) |
| rain scale | bound giving 25% grassland, per world, medians | 772 / 690 / 605 mm: ratios 1.12 / 1 / 0.877 (shipped 1.125 / 1 / 0.875) |
| forest 700, scales 1.12 / 1 / 0.875, 600 m jitter | | grass median 28 / 28 / 26%; forest above 0.65 in five weak-shadow worlds (512 s41, s43; 1024 s38, s41, s43; max 0.706) |
| forest 720 (shipped) | | as 11.1 |
| jitter 600 m / 3 oct / ±15% / ±0.8 °C; 800 / 5; 400 / 4 / ±20% / ±1.2 °C; 1000 / 5 / ±20%; 500 / 4 / ±20% / ±1 °C (shipped) | crops of 1024 s3 | 600/3: smooth lobes, no small groves; 400/4: groves and clearings with ragged edges, a few specks; 500/4 between, no speck: cells in patches under 16 cells ≤ 0.3% of land in every world |

### 11.3 Limitations

1. **Calibration model.** Not the implementation: the jitter uses floating-point interpolation of the exact lattice; a cell within about 10^-7 of a bound could fall the other way. Shares will move by far less than the ranges' widening. The implementation's sweep replaces section 9's ranges.
2. **Marsh depends on the river threshold and on k.** Drainage height is measured to rivers of class ≥ `drain_river_class_min` (0.5 km² upstream at the shipped 0.125 km² river threshold), lakes and the sea; the floodplains of smaller streams are not marsh, and their own cells take their banks' biome (B5). Changing `river_min_area_m2` (P1-02) shifts every class and so the marsh share.
3. **Marsh scales with map size.** 1.2 / 2.4 / 5.7% median: 512 worlds are steep with narrow valleys (18 of 50 have under 1% marsh, 512 s10 0.12%), 2048 worlds have broad flat interiors. No single rule found gives 1–6% at all three sizes (11.2); a per-size marsh limit would be possible but hides a real difference of the terrain (open question 8). At 2048 the marsh draws thin grey threads along the class ≥ 3 rivers across the flat interior (`sheets/final-2048-s0-7.png`, every world) more than broad fens.
4. **Treeline and moor line follow contours.** Temperature is mostly height (P1-03 limitation 8), and ±1 °C of jitter is about ±150 m of height, a few cells on steep slopes. So alpine and moor are smooth contour bands on the ranges (checklist item 12 asks for ragged edges); the forest–grassland edge is ragged, the upland edges are not. A larger `temp_jitter_c` would break them up at the cost of moor patches in lowland valleys near the line; the per-world temperature offset (P1-03 open question 7) changes where the line sits, not its shape.
5. **A forest rim on lee coasts.** The 75 mm coast bonus plus the rain jitter keeps a strip of forest along most coasts, including lee coasts (1024 s8 north-west coast, s14 south coast, s19 east coast; 2048 every world). Thinner than with the 150 mm bonus but visible at sheet scale. It is the climate's (P1-03 C6); options are in open question 10.
6. **Weak-shadow worlds are forest-heavy.** Where the main range lies on the downwind coast (P1-03 limitation 9) forest reaches 0.61–0.64 (512 s41, s43; 1024 s38, s41, s43). Realistic; they are the least varied maps.
7. **2048 interiors.** The scale fixes the median, not the gradient: at 2048 windward lowland is 30% grassland (median), against 16–17% at 512 and 1024, because the interior dries with path length (P1-03 limitation 7). 2048 maps read "wet coastal rim, open interior with parkland" more than "wet windward, dry lee".
8. **No transitions or mixtures.** One biome per 16 m cell; ecotones and mixed woodland are the runtime's job (scatter density from the climate layers).
9. **Slope speckle.** The marsh slope limit uses the per-cell slope; the shipped 0.05 gives no visible speckle (small patches ≤ 0.3% of land). `biome_patches` will show a regression.

## 12. Open questions for the lead

1. **Biome categories** (taste; Paul should see them). Chosen: P1-00's five natural biomes, nothing more. Recommendation: keep five for the prototype; a sixth (for example `heath` for dry uplands) is a data change plus a code, colour and noun.
2. **Rules as data with priorities** instead of a code-side Whittaker table. Recommendation: approve.
3. **Marsh by height above drainage to rivers of class ≥ 3, lakes and the sea** (new: G7 gains the class parameter, P1-00 7.7; P1-05 keeps k = 1). Recommendation: approve; the alternative, hand to every stream, puts marsh beside every brook (11.2).
4. **Alpine by temperature only.** Recommendation: approve; steep rock is the runtime's (the prototype's "rock" rule swallowed whole 512 ranges).
5. **Biome fields for later stages** (`fertility`, `timber`, `region_noun`). Recommendation: approve.
6. **Blighted waste** stays a history overlay (P1-00 6.6). Recommendation: confirm when the history specs are written.
7. **Per-size rainfall scale as three tuning fields** (1.125 / 1 / 0.875), not a power law and not per-world quantiles (4.2). Recommendation: approve; revisit if climate's moisture path changes (P1-03 open question 6).
8. **Marsh share grows with map size** (limitation 3). Recommendation: accept; it follows the terrain (flat broad interiors at 2048). If Paul wants equal marsh, add `hand_scale_*` fields like the rain scale (a data-shaped change, PATCH after this merge).
9. **River cells take the biome of their banks** (B5). Recommendation: approve; it is the literal reading of "rivers take the biome of their land", and P1-05 can still see rivers in `water` and `river`.
10. **Forest rim on lee coasts** (limitation 5). Recommendation: leave for the world critic's review of this stage; if it reads as wrong, lower `coast_rain_mm` to about 40 (a climate data change) rather than add a coast rule here.

**Schema and catalogue impact (for the lead's proposals entry "P1 schema changes", B8):**

- Schema version 2 → 3, in the biomes merge (generator 0.8.0). `SCHEMA_VERSION`, `PRAGMA user_version` and `meta.schema_version` become 3.
- New table `biome` (DDL in section 3): one row per loaded biome definition, written by the biome stage; `id` = biome `code`, `key` = definition id, `name`, `colour` (0xRRGGBB), `water` (0 or 1).
- New table `climate` (DDL in section 3): one row, `id` 1, `wind_toward` = the prevailing wind's D8 code (0 E to 7 NE, the direction the wind blows toward; P1-03 C2), written by the climate stage in C7. The climate layers are byte-identical to 0.7.0's.
- DDL order: `meta`, `layer`, `pass`, `biome`, `climate`. Both tables exist in every world of schema 3; empty when their stage did not run. The empty world grows by one page per table (5 → 7 pages expected; the validation confirms, P0-02 amendment).
- `report`: `[tables]` lists both; `[geography]` gains `wind_toward` (P1-03 9), `biome_share` and `biome_patches`; a malformed `climate` row is a corrupt-table error (P1-03 9).
- No layer catalogue change, no new `meta` or `param.*` key. `region` and `region_border` stay planned as version 4 (P1-06).
- New data kind `biome` (fields in 4.1, P1-00 6.6); new `worldgen` id `biomes`; two stream names (`geo.biomes.temp_jitter`, `geo.biomes.rain_jitter`); G7 gains the minimum river class parameter (P1-00 7.7).
