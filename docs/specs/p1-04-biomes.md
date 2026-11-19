# P1-04 Biomes

Status: **third version, "biome variety" (generator 0.9.0, schema version 4), draft, spec writer, 2026-11-18, awaiting approval;** amended 2026-11-18 after the implementation (branch `p1/04b-biome-variety`) and the test writer's reference: seams and types as implemented (10), fatal cases and check order (10.5), the hash encoding of the new golden items (10.4), the r_cells limit (7), skipping and widths of B2b and B2c (5), implementation timings and the 512 budget raised to 0.15 s (8); no algorithm or value changed. It replaces the second version (approved and merged as generator 0.8.0, schema 3; validated in `docs/reviews/p1-04-biomes-validation.md`). Decisions by Paul, recorded in `docs/design/proposals.md` ("More natural biomes, per-world temperature, a thinner coastal forest"; "Large maps may have unusual worlds"), and the lead's rulings on the exploration (branch `p1/04b-biome-variety-explore`, commits `063b07d` to `267d12d`; definition `~/.cache/the-unmapped/out/p1-04b-variety/variety-definition.md`). Section 0 lists every change. The climate half of the revision (per-world temperature offset, coast bonus 40 mm, the `climate` table's new column) is in P1-03; both land in one merge. Phase P1 Geography, stage 4 (`biomes`). Design doc section 4 (stage 4: "a lookup on temperature and rainfall, plus special cases for coast, river and high peaks"), 5 (peoples and beasts by biome), 6 (`region.biome`), 7 (scatter "from biome rules"). Depends on P0-01 to P0-04, P1-00 (G1 to G8, D1 to D10, the `biome` kind of 6.6, schema 9.2), P1-02 and P1-03. Every statistics range is provisional until the implementation's validation sweep (section 9).

## 0. Changes in this revision

| What | Second version (0.8.0) | Third version (0.9.0) | Why |
| --- | --- | --- | --- |
| Natural biomes | five: alpine, marsh, moor, forest, grassland | nine: alpine 50, **fen 45**, marsh 40, moor 30, **heath 25**, forest 20, **wet woodland 18**, **steppe 15**, grassland 10 (fallback); `blighted_waste` stays history-only | Paul: "add all of them for more variety" (proposals) |
| Codes | 1 to 8 | 1 to 8 unchanged in meaning; new 9 `heath`, 10 `steppe`, 11 `wet_woodland`, 12 `fen` | codes are stored in worlds and read by the viewer |
| Highland moor | ≤ 4 °C above 300 m | also rain ≥ 720 mm × scale (the forest bound) | the dry (lee) faces of ranges become heath |
| Marsh | colour 0x5C6E54, noun "Fen" | colour 0x6AA396, noun "Marsh"; rule unchanged | fen is now its own biome |
| Moor colour | 0x786468 | 0x7E6680 | world critic's palette |
| `biome_rule` | nine fields | two more, after `coast_max_m`: `river_dist_max_m`, `wet_frac_min` (4.1), with sentinels 32767 and 0 | wet woodland near rivers; fen where wet ground is broad |
| Stage steps | B1 to B6 | new **B2b** (3-4 chamfer distance to rivers of class ≥ `river_dist_class_min`) and **B2c** (share of wet cells in a triangle window, two exact box passes); B4 gains two comparisons | |
| `worldgen.biomes` | seven fields | six more: `river_dist_class_min`, `wet_window_m`, `wet_hand_m`, `wet_scale_512`, `wet_scale_1024`, `wet_scale_2048`; the window's two passes are fixed (4.2) | |
| Table `biome` | eight rows | twelve rows | |
| `report` | `biome_share` with 6 keys | 10 keys, ascending code; `temp_offset_c` is new (P1-03 9) | |
| Schema | 3 | **4**: table `climate` gains `temp_offset_dc` (P1-03 3); no change to table `biome` | Paul: per-world temperature |
| Not adopted | | the exploration's wet-share jitter (`wet_frac_jitter`, stream `geo.biomes.wet_jitter`) and elevation jitter (`elev_jitter_m`, stream `geo.biomes.elev_jitter`) | lead: almost no visible effect (11.2); the wet jitter cost 0.5 s at 2048 |
| Statistics | five biomes, 108 worlds | nine biomes, re-measured on 58 worlds with the final setting (9, 11.1) | |

Byte identity to 0.8.0 is in section 6, item 9.

## 1. Purpose

Give every cell one biome code from a set defined in data. The biome layer is what the map, the viewer and the critic look at first; resources (P1-05) take fertility and timber from it; regions (P1-06) take their dominant biome and name from it; history places races and beasts by preferred biome (design doc section 5); the runtime scatters vegetation by it (milestone 4).

Nine natural land biomes, plus `ocean` and `lake` for water, plus `blighted_waste`, which only history assigns (P1-00 6.6). Rivers are sub-cell features and take the biome of their land (B5). The coast, river and high-peak special cases of the design doc are rules that select among these biomes:

- **High peaks**: alpine wherever the mean annual temperature is at or below the treeline value (about 1,000 m at mid-map at the reference temperature; the per-world offset of P1-03 moves it by up to ±300 m).
- **Broad wet lowland**: fen on near-level ground at most 3 m above the water it drains to, below 200 m, where at least 45% of the ground within about 480 m (triangle-weighted) is wet; at 2048 the share must be 1.5 times higher (4.2).
- **Narrow wet margins**: marsh on near-level land at most 1 m above its drainage (river of class ≥ 3, lake or sea), as in the second version.
- **Uplands**: on cool ground above 300 m that is not alpine, highland moor where rain reaches the forest bound, heath where it does not (in practice the lee faces of ranges).
- **Forest** where rainfall reaches the forest bound for the map size.
- **Wet woodland** (carr) in valley floors within about 112 m of a river of class ≥ 4, at most 5 m above drainage, below 300 m, where it is too dry for forest but has at least 450 mm × scale.
- **Steppe** where rainfall is at most 600 mm × scale (the dry heart of a rain shadow); **grassland** otherwise.

The rules are data in the biome definitions, evaluated in a fixed priority order, so a biome can be added or retuned without code.

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation` (dm), `water`, `water_depth`, `flow_dir`, `river`, `coast_dist` | P1-01, P1-02 |
| `temperature` (0.1 °C), `rainfall` (mm) | P1-03 |
| Biome definitions | kind `biome`, `game/data/biomes/` (section 4.1) |
| Tuning | definition `worldgen.biomes` (section 4.2) |
| World seed, N | `meta.seed`, `param.map_size` |

Streams (unchanged): K_tj = `stream_key(seed, "geo.biomes.temp_jitter")` and K_rj = `stream_key(seed, "geo.biomes.rain_jitter")`, noise keys only (D1). B2b and B2c use no randomness.

Preconditions (fatal, message beginning `biomes: input precondition: `; from `generate` exit 1 with no world file and no `.partial`, P0-04 4), in this order:

1. The eight input layers exist, each N × N with its catalogue type (`i16`, `u8`, `u16`, `u8`, `u8`, `i16`, `i16`, `u16`).
2. Every `water` value is in 0..3; every `river` value is in 0..15.
3. The G7 preconditions (P1-00 7.7): every non-drain cell reaches a drain cell along `flow_dir`, and no height above drainage is negative.

**Prefixes.** Every precondition failure of the stage, including one found by G7 inside the stage, is reported as `biomes: input precondition: ` followed by the detail; for a G7 failure the detail is G7's own message, which begins `G7: ` (for example `biomes: input precondition: G7: cell (4, 2) does not reach a drain cell`). The G7 seam called directly (10, `height_above_drainage`) raises its message with the prefix `G7: ` only; so do its argument checks (k outside 1..15, arrays of the wrong length).

## 3. Outputs

| Output | Content |
| --- | --- |
| Layer `biome` (`u8`, layer id 11 in a full run) | the biome `code` of every cell; never 0; `ocean`'s code exactly on `water` = 3, `lake`'s exactly on `water` = 2; a biome whose `priority` is 0 never on a land cell, except the fallback |
| Table `biome` | one row per loaded biome definition (B6); twelve rows with the shipped data |

**Schema.** This stage's table `biome` is unchanged (schema 3 DDL). The revision's schema change, **version 3 → 4**, is the climate stage's: table `climate` gains the column `temp_offset_dc` (P1-03 3 holds the DDL and the reader and writer rules; P0-02 amendment 16). It is made by the same merge (generator 0.9.0). The layer catalogue is unchanged; the `biome` layer's meaning ("biome code, table `biome`") is unchanged, only the set of codes in use grows, which needs no bump (P1-00 4, rule 3). The planned `region` and `region_border` tables move to version 5 (P1-06, P1-00 9.2).

## 4. Data and tuning

### 4.1 The `biome` kind

Registered by this stage's implementation (P1-00 6.6). Fields in schema declaration order (this fixes the canonical encoding, P0-03 8); unchanged by this revision:

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
| `forage` | fx(0, 1) | wild food of the biome (fish, fowl, game, reeds), used by P1-05; **added 2026-11-18 by P1-05's second version (generator 0.10.0)** |
| `region_noun` | string(32) | noun for region names, used by P1-06 (e.g. `Fen`) |

**Amendment for P1-05 (generator 0.10.0, 2026-11-18).** The kind gains the required field `forage`, declared after `timber` and before `region_noun` (canonical encoding, P0-03 8), and steppe's `fertility` becomes `"0.7"` (was `"0.6"`; this section's open question 4, settled by P1-05 on the critic's advice). Every shipped and fixture biome file gains `forage = …` after `timber`, with the values of P1-05 4.1 (ocean 0, lake 0, temperate_forest "0.5", grassland "0.3", marsh "0.8", highland_moor "0.25", alpine "0.05", blighted_waste 0, heath "0.2", steppe "0.35", wet_woodland "0.7", fen "0.6"). The biome stage does not read either field: the `biome` layer and table are unchanged. The listing below shows the 0.9.0 files; read it with these two changes.

Record `biome_rule`: all eleven fields required (the two new ones too: there is no optional-field mechanism; every rule of every shipped and fixture file writes every field). A condition holds when the value lies in the closed interval. **Unbounded sentinels** (lead's decision, 2026-11-17): a bound equal to the extreme value of its field's type, on the side it bounds, means "no limit on that side", and B4 skips that comparison entirely. The inputs can lie outside the field ranges (tq down to −61 °C and up to 51 °C with the jitter, rq up to 20000 mm, S2 tangents up to about 290, `elevation` up to 3276.7 m, hand up to 3276.7 m, d34 = 2^40 on a world without a target river), so without the rule a rule written with full ranges would fail on those cells.

| Field | Type | Compared with | Sentinel (comparison skipped) |
| --- | --- | --- | --- |
| `temp_min_c`, `temp_max_c` | fx(−60, 50) | tq, the jittered temperature in °C | `temp_min_c` = −60; `temp_max_c` = 50 |
| `rain_min_mm`, `rain_max_mm` | int(0, 10000) | rq, the jittered rainfall in mm, against the bound **scaled for the map size** (B4) | `rain_min_mm` = 0; `rain_max_mm` = 10000 (neither is scaled) |
| `elev_min_m`, `elev_max_m` | int(0, 3000) | `elevation` in metres (compared as 10 × bound against decimetres) | `elev_min_m` = 0; `elev_max_m` = 3000 |
| `slope_max` | fx(0, 100) | the cell's slope tangent (G8), compared exactly as S2 | 100 |
| `hand_max_m` | fx(0, 3000) | height above drainage (G7 with k = `drain_river_class_min`) | 3000 |
| `coast_max_m` | int(0, 32767) | `coast_dist` | 32767 |
| `river_dist_max_m` (new) | int(0, 32767) | d34 of B2b, the 3-4 chamfer distance to the nearest river cell of class ≥ `river_dist_class_min`: the rule holds when 16 × d34 ≤ 3 × bound (d34 counts 3 per orthogonal and 4 per diagonal step of 16 m, so 16 × d34 / 3 is the distance in metres) | 32767 |
| `wet_frac_min` (new) | fx(0, 1) | the wet share count / total of B2c, against the bound **scaled by the per-size wet scale** ws (B4): the rule holds when need × total ≤ count × 2^32, need = mul(bound, ws).raw | 0 |

The lower sentinels `rain_min_mm` 0, `elev_min_m` 0 and `wet_frac_min` 0 and the upper sentinel `coast_max_m` 32767 can never change a result (rq ≥ 0; a classified cell has `elevation` ≥ 1; `coast_dist` ≤ 32767; count ≥ 0); they are listed so that the rule is the same for every field. `river_dist_max_m` 32767 does change results: it is the only way a rule matches a cell with d34 = 2^40. A bound one step inside the extreme (for example `rain_max_mm` 9999, `river_dist_max_m` 32766, or `wet_frac_min` raw 1) is compared normally.

**Field ranges at the seams.** Data loaded through P0-03 has every field in range. The seams that take definitions as values (`biome_lookup`, `biome_layer`, section 10) check every rule field against the range above before any cell is evaluated, in B4's order (land definitions with priority > 0 in descending priority, rules in array order, fields in declaration order), and stop at the first failure: fatal `biomes: rule: <field> outside its schema range` (for example `biomes: rule: wet_frac_min outside its schema range` for raw ONE + 1). This was already the 0.8.0 code's behaviour for the nine old fields; the two new fields are checked after `coast_max_m`, in their order.

**Canonical encoding.** The two new fields are appended to `biome_rule` in the order of the table, so every rule's encoding grows by two fields and `content_hash` of every world changes (P1-00 6.4).

Named checks (P0-03 7.5), unchanged. `biome_water_body` and `biome_water_rules` are **per-definition** checks (P0-03 7.5, run on each definition that passed type validation). `biome_code_unique`, `biome_water_unique`, `biome_fallback`, `biome_priority_unique` and `biome_rule_ranges` are **kind-level** checks (P0-03 amendment 16): each runs once over all the kind's definitions that passed type validation, in canonical id order (ascending id by bytes, P0-03 7.4; duplicate ids left out), and may report several failures. Error forms (P0-03 9; the same form as P1-01 4.3), where `<message>` is the table's message:

- **At a definition** (both kinds of check): `<path>:<line>:<column>: error: check '<name>' failed for 'biome.<id>': <message>`, at the definition's header (P0-03 amendment 10 gives the header position).
- **Against the directory** (kind-level only): `biomes: error: check '<name>' failed for kind 'biome': <message>`, no position.
- **Between two definitions** (checks 1, 3, 4, 6 when two clash): reported once for each later definition, at its header, naming the earliest. "Earlier" and "later" are the canonical id order; file order never matters. Three definitions sharing a code give two errors, both naming the first.

For example, two definitions `marsh` and `swamp` with code 5: `biomes/swamp.toml:1:1: error: check 'biome_code_unique' failed for 'biome.swamp': code 5 is also used by 'biome.marsh'`; no lake biome: `biomes: error: check 'biome_water_unique' failed for kind 'biome': no biome has water_body "lake"`. With the twelve shipped definitions, canonical id order is `alpine`, `blighted_waste`, `fen`, `grassland`, `heath`, `highland_moor`, `lake`, `marsh`, `ocean`, `steppe`, `temperate_forest`, `wet_woodland`.

| # | Check | Condition | Where | Message |
| --- | --- | --- | --- | --- |
| 1 | `biome_code_unique` (P1-00) | no two definitions share a `code` | at the later definition | `code <c> is also used by 'biome.<earlier id>'` |
| 2 | `biome_water_body` | `water` is true exactly when `water_body` is not "none" | at the definition | `water must be true exactly when water_body is not "none"` |
| 3 | `biome_water_unique` | exactly one definition has `water_body` "ocean", exactly one "lake" | none: against the directory; two or more: at each later one | none: `no biome has water_body "<v>"`; more: `water_body "<v>" is also used by 'biome.<earlier id>'` |
| 4 | `biome_fallback` | exactly one definition has `fallback` true, and it has `water_body` "none" | none: against the directory; two or more: at each later one; a fallback with `water_body` other than "none" (whether or not it is the only one): at that definition | none: `no biome has fallback = true`; more: `fallback = true is also set by 'biome.<earlier id>'`; water: `the fallback biome must have water_body "none"` |
| 5 | `biome_water_rules` | a definition with `water_body` not "none" has `priority` 0 and no rules | at the definition | `water biomes take no rules and priority 0` |
| 6 | `biome_priority_unique` | land definitions (`water_body` "none") with `priority` > 0 have distinct priorities | at the later definition | `priority <p> is also used by 'biome.<earlier id>'` |
| 7 | `biome_rule_ranges` (kind-level, so that it can report one error per failing pair) | in every rule, `temp_min_c ≤ temp_max_c`, `rain_min_mm ≤ rain_max_mm`, `elev_min_m ≤ elev_max_m` (the new fields form no pair) | at the definition; one error per failing pair, rules in array order, pairs in the order listed | `rules[<i>]: <min field> must not exceed <max field>`, with the field names written out, e.g. `rules[0]: temp_min_c must not exceed temp_max_c`, `rules[1]: rain_min_mm must not exceed rain_max_mm`, `rules[0]: elev_min_m must not exceed elev_max_m` |

The kind-level checks run after every definition of the kind has been validated, in the order 1, 3, 4, 6, 7, and only if at least one definition passed type validation (a kind whose every definition failed reports those failures only). All errors are then sorted as P0-03 9 sorts them. A directory that is missing or holds no definition is P0-03's required-kind error, not these.

**Shipped definitions**, `game/data/biomes/`, one file per biome (file names carry no meaning, P0-03 7.4), shown here as one listing. The fixture copies `worldgen/tests/data/geo/biomes/` and `worldgen/tests/data/geo-small/biomes/` are identical to them. Codes 1 to 8 keep their P1-00 6.6 meaning; codes 9 to 12 are new. Values differing from the second version are marked in comments; the comments are not part of the shipped files.

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
rain_min_mm = 720
rain_max_mm = 10000
elev_min_m = 0
elev_max_m = 3000
slope_max = 100
hand_max_m = 3000
coast_max_m = 32767
river_dist_max_m = 32767   # new field
wet_frac_min = 0           # new field

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
colour = 0x6AA396          # was 0x5C6E54
water = false
water_body = "none"
priority = 40
fallback = false
fertility = "0.45"
timber = "0.3"
region_noun = "Marsh"      # was "Fen"

[[biome.marsh.rules]]
temp_min_c = -60
temp_max_c = 50
rain_min_mm = 500
rain_max_mm = 10000
elev_min_m = 0
elev_max_m = 200
slope_max = "0.05"
hand_max_m = 1
coast_max_m = 32767
river_dist_max_m = 32767
wet_frac_min = 0

[biome.highland_moor]
code = 6
name = "Highland moor"
colour = 0x7E6680          # was 0x786468
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
rain_min_mm = 720          # was 0
rain_max_mm = 10000
elev_min_m = 300
elev_max_m = 3000
slope_max = 100
hand_max_m = 3000
coast_max_m = 32767
river_dist_max_m = 32767
wet_frac_min = 0

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
river_dist_max_m = 32767
wet_frac_min = 0

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

[biome.heath]               # new
code = 9
name = "Heath"
colour = 0x94664A
water = false
water_body = "none"
priority = 25
fallback = false
fertility = "0.2"
timber = "0.05"
region_noun = "Heath"

[[biome.heath.rules]]
temp_min_c = -60
temp_max_c = 4
rain_min_mm = 0
rain_max_mm = 10000
elev_min_m = 300
elev_max_m = 3000
slope_max = 100
hand_max_m = 3000
coast_max_m = 32767
river_dist_max_m = 32767
wet_frac_min = 0

[biome.steppe]              # new
code = 10
name = "Steppe"
colour = 0xE0D296
water = false
water_body = "none"
priority = 15
fallback = false
fertility = "0.6"
timber = "0.05"
region_noun = "Steppe"

[[biome.steppe.rules]]
temp_min_c = -60
temp_max_c = 50
rain_min_mm = 0
rain_max_mm = 600
elev_min_m = 0
elev_max_m = 3000
slope_max = 100
hand_max_m = 3000
coast_max_m = 32767
river_dist_max_m = 32767
wet_frac_min = 0

[biome.wet_woodland]        # new
code = 11
name = "Wet woodland"
colour = 0x23505E
water = false
water_body = "none"
priority = 18
fallback = false
fertility = "0.5"
timber = "0.7"
region_noun = "Carr"

[[biome.wet_woodland.rules]]
temp_min_c = -60
temp_max_c = 50
rain_min_mm = 450
rain_max_mm = 10000
elev_min_m = 0
elev_max_m = 300
slope_max = 100
hand_max_m = 5
coast_max_m = 32767
river_dist_max_m = 112
wet_frac_min = 0

[biome.fen]                 # new
code = 12
name = "Fen"
colour = 0x8A8070
water = false
water_body = "none"
priority = 45
fallback = false
fertility = "0.25"
timber = "0.2"
region_noun = "Fen"

[[biome.fen.rules]]
temp_min_c = -60
temp_max_c = 50
rain_min_mm = 500
rain_max_mm = 10000
elev_min_m = 0
elev_max_m = 200
slope_max = "0.05"
hand_max_m = 3
coast_max_m = 32767
river_dist_max_m = 32767
wet_frac_min = "0.45"
```

(In TOML a key such as `rules = []` must come before the definition's `[[…rules]]` tables; definitions with rule tables omit the `rules =` line, since the array-of-tables headers define it. A definition with neither is a missing-field error.)

**Summary by priority** (non-sentinel bounds only; "× sc" is the rain scale, "× ws" the wet scale of B4):

| Priority | Biome (code) | Rule | Colour | fertility / timber | `region_noun` |
| --- | --- | --- | --- | --- | --- |
| 50 | alpine (7) | tq ≤ 0.5 °C, elevation ≥ 600 m | 0xBEBEB4 | 0 / 0 | Peaks |
| 45 | fen (12) | rq ≥ 500 × sc, elevation ≤ 200 m, slope ≤ 0.05, hand ≤ 3 m, wet share ≥ 0.45 × ws | 0x8A8070 | 0.25 / 0.2 | Fen |
| 40 | marsh (5) | rq ≥ 500 × sc, elevation ≤ 200 m, slope ≤ 0.05, hand ≤ 1 m | 0x6AA396 | 0.45 / 0.3 | Marsh |
| 30 | highland_moor (6) | tq ≤ 4 °C, elevation ≥ 300 m, rq ≥ 720 × sc | 0x7E6680 | 0.3 / 0.1 | Moor |
| 25 | heath (9) | tq ≤ 4 °C, elevation ≥ 300 m | 0x94664A | 0.2 / 0.05 | Heath |
| 20 | temperate_forest (3) | rq ≥ 720 × sc | 0x2E6034 | 0.7 / 1 | Wood |
| 18 | wet_woodland (11) | rq ≥ 450 × sc, elevation ≤ 300 m, hand ≤ 5 m, river distance ≤ 112 m | 0x23505E | 0.5 / 0.7 | Carr |
| 15 | steppe (10) | rq ≤ 600 × sc | 0xE0D296 | 0.6 / 0.05 | Steppe |
| 10 | grassland (4), fallback | none | 0x96AA5A | 1 / 0.15 | Downs |
| 0 | ocean (1), lake (2), blighted_waste (8) | none | 0x183878, 0x4080CC, 0x605446 | 0 / 0 | Sea, Mere, Waste |

With the shipped scales (sc 1.125 / 1 / 0.875 at 512 / 1024 / 2048) the forest and moor bound is 810 / 720 / 630 mm, steppe 675 / 600 / 525 mm, the wet woodland minimum 506.25 / 450 / 393.75 mm, the fen and marsh minimum 562.5 / 500 / 437.5 mm; the fen's wet-share minimum (ws 1 / 1 / 1.5) is 0.45 / 0.45 / 0.675. Heath has no rainfall bound: below moor in priority, it takes the cool uplands moor rejects for dryness. Steppe has no other bound: above grassland, it takes every remaining cell dry enough, highland or low.

**Fertility, timber and nouns of the new biomes** (read by P1-05 and P1-06; values of the exploration, which the critic reviewed, with reasons):

- **Fen** fertility 0.25 (waterlogged peat; below marsh's 0.45, which is a narrow margin beside better ground, and above heath's 0.2), timber 0.2 (carr islands, alder and willow, no timber trees in quantity); noun "Fen" (the "Black Fen"). Marsh's noun becomes "Marsh", so that a fen region and a marsh region read differently.
- **Heath** fertility 0.2 (acid, dry upland soil; below moor's 0.3, which is wetter grazing), timber 0.05 (gorse and scrub); noun "Heath".
- **Steppe** fertility 0.6, **0.7 from generator 0.10.0** (P1-05 4.1) (dry grassland: grazing, and dry grain only in good years; below grassland's 1 and below forest's 0.7, so that a dry interior is poorer farmland than cleared woodland), timber 0.05 (almost treeless); noun "Steppe". The critic noted that 0.7 to 0.75 is defensible if the 2048 interiors must stay fertile; open question 4.
- **Wet woodland** fertility 0.5 (alluvial ground, but wet and wooded; between marsh and forest), timber 0.7 (alder, willow and ash: real wood, poorer than forest's 1); noun "Carr".

### 4.2 Tuning `worldgen.biomes`

Fields in schema declaration order, all required (P1-00 6.1); the first seven are unchanged:

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `jitter_noise` | `fbm_m` | wavelength 500, octaves 4, gain "0.5" | calibrated | scale of boundary irregularity (groves, clearings, ragged treelines) |
| `temp_jitter_c` | fx(0, 10) | 1 | calibrated | °C per unit of noise added to the temperature the rules see |
| `rain_jitter` | fx(0, 1) | "0.2" | calibrated | relative rainfall change per unit of noise |
| `rain_scale_512` | fx("0.25", 4) | "1.125" | calibrated | factor on every rule's rainfall bounds for N ≤ 512 |
| `rain_scale_1024` | fx("0.25", 4) | 1 | designed (1024 is the reference size) | factor for 512 < N ≤ 1024 |
| `rain_scale_2048` | fx("0.25", 4) | "0.875" | calibrated | factor for N > 1024 |
| `drain_river_class_min` | int(1, 15) | 3 | calibrated | rivers of at least this class (P1-02 H6) are drain cells of G7 for this stage; smaller streams are not |
| `river_dist_class_min` (new) | int(1, 15) | 4 | calibrated | B2b's targets: river cells (`water` = 1) with `river` ≥ this (class 4 is about 1 km² upstream at the shipped river threshold) |
| `wet_window_m` (new) | int(0, 2000) | 480 | calibrated | half-width of the support of B2c's triangle window: r = `wet_window_m` div 16 (30 cells), two box passes of half-width h = r div 2 (15 cells) |
| `wet_hand_m` (new) | fx(0, 100) | 5 | calibrated | a cell is wet for B2c when it is not ocean and its height above drainage (B2's hand) is at most this |
| `wet_scale_512` (new) | fx("0.25", 4) | 1 | calibrated | factor on every rule's `wet_frac_min` for N ≤ 512 |
| `wet_scale_1024` (new) | fx("0.25", 4) | 1 | designed (reference size) | factor for 512 < N ≤ 1024 |
| `wet_scale_2048` (new) | fx("0.25", 4) | "1.5" | calibrated | factor for N > 1024 |

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
river_dist_class_min = 4
wet_window_m = 480
wet_hand_m = 5
wet_scale_512 = 1
wet_scale_1024 = 1
wet_scale_2048 = "1.5"
```

**Fixed constants** (not tuning): the chamfer weights 3 and 4 of B2b; B2c is always two box passes (the triangle; the exploration's one-box variant `wet_window_passes` = 1 is not specified, open question 6). `wet_window_m` is bounded by 2000 (the exploration allowed 4000) so that every sum and product of B2c fits `int64` (section 7).

**Why a wet scale per size.** 2048 maps are flatter (P1-04 second version 11.2: median lowland slope 0.04 against 0.07 at 1024), so the same share threshold makes fens much more common there; three exact factors chosen by the same size rule as the rain scale, for the same reasons as the rain scale (second version 4.2: only three sizes; no fractional power; exact binary fractions). The scale applies to `wet_frac_min` only. `river_dist_max_m` is in metres and river classes are areas, so it needs no scale. Test grids (N < 512) use the 512 entries.

Registering the two new rule fields and the six new tuning fields, and the four new definitions, changes `content_hash` of every world (P1-00 6.4); the merge bumps `GENERATOR_VERSION` (MINOR: 0.9.0) and re-records the golden values.

## 5. Algorithm

Notation of P1-01 5.0; X = `from_int(16x + 8)`, Y = `from_int(16y + 8)` (metres). Steps B1 to B6, with B2b and B2c after B2.

### B1. Preconditions

Section 2.

### B2. Drainage height and slope (P1-00 G7, G8)

hand[i] (dm) = G7 with k = `drain_river_class_min` from `elevation`, `water`, `water_depth`, `river`, `flow_dir`; S2[i] by G8.

### B2b. River distance (sequential, exact)

Input: `water`, `river`, k_r = `river_dist_class_min`. Output: d34[i], `int64`, for every cell. A **target** is a cell with `water` = 1 and `river` ≥ k_r. FAR = 2^40.

```
d34[i] = 0 on targets, FAR elsewhere
forward pass:  for y = 0 .. H−1, for x = 0 .. W−1:
    d34(x, y) = min(d34(x, y), d34(x−1, y) + 3, d34(x−1, y−1) + 4, d34(x, y−1) + 3, d34(x+1, y−1) + 4)
backward pass: for y = H−1 .. 0, for x = W−1 .. 0:
    d34(x, y) = min(d34(x, y), d34(x+1, y) + 3, d34(x+1, y+1) + 4, d34(x, y+1) + 3, d34(x−1, y+1) + 4)
```

Neighbours outside the grid are skipped. Each pass reads values already updated by the same pass (in place, in the stated order). The four candidates of a cell are taken in the order written; the order does not change a minimum. The result on every cell equals the closed form min over targets t of 3 × max(|x − t_x|, |y − t_y|) + min(|x − t_x|, |y − t_y|) (an obstacle-free grid; the two-pass chamfer is exact for it), and FAR on every cell when the grid has no target (the passes never lower a FAR value: FAR + 3 > FAR). An implementation may compute d34 by any method that gives this closed form, but the reference is the two passes. Every cell is computed, also ocean and lake cells (whose values no rule reads). B2b is needed only when some rule of a land definition with priority > 0 has `river_dist_max_m` ≠ 32767; an implementation may skip it otherwise (the result is the same, since the comparison is skipped). **Skipping and width** (amended 2026-11-18): when no rule uses d34 (B4), B2b may be skipped; it uses no stream and writes no output, so nothing observable changes, and `biome_layer` then does not check `river_dist_class_min` either (section 10.5). The values may be held internally as `int32` with 2^30 in place of FAR: finite values are at most 3 × (max(W, H) − 1) + (min(W, H) − 1) < 3 × 2^24 < 2^26 on any grid of at most 2^24 cells (a 1 × 2^24 strip reaches 50,331,645), below 2^14 at 2048, so 2^30 exceeds every finite value and 2^30 + 4 fits `int32` without lowering a value; the seam `biome_river_distance` returns `int64` with 2^40 for no target.

### B2c. Wet share (exact; any split)

Input: `water`, hand of B2, `wet_hand_m`, r = `wet_window_m` div 16, h = r div 2. Output: wet_count[i] and wet_total[i], `int64`, for every cell.

```
wet[i] = 1 if water[i] ≠ 3 and from_int(hand[i]) ≤ mul_int(wet_hand_m, 10), else 0
one[i] = 1
Box(v, h)[x, y] = Σ v over the window [x − h, x + h] × [y − h, y + h] clipped to the W × H grid
wet_count = Box(Box(wet, h), h)
wet_total = Box(Box(one, h), h)
```

Clipping is the only edge rule, applied separately in each pass (as P1-03 C4): the second pass sums the first pass's clipped sums over its own clipped window. Ocean cells are never wet but count in `wet_total`, so a coastal flat needs more of its land wet. Lake cells and river cells of class ≥ `drain_river_class_min` have hand 0 and are wet; so is any other non-ocean cell within `wet_hand_m` of its drainage. Every value is an exact integer, so the result does not depend on the method (summed-area table, running row and column sums, direct sums) or the split. Pass 2 starts after pass 1 has finished.

Away from the edges, wet_count / wet_total is the share of wet cells under the separable triangle weights (1, 2, …, 2h + 1, …, 2, 1) / (2h + 1)² along each axis, support half-width 2h (30 cells, 480 m, with the shipped value; 2h = r for even r, r − 1 for odd r), and wet_total = (2h + 1)^4 = 923521. h = 0 (r ≤ 1) gives wet_count = wet[i], wet_total = 1.

Why a triangle: the exploration's single box of radius r drew fen outlines as staircases and discs; the triangle rounds them (11.2). B2c is needed only when some land rule has `wet_frac_min` ≠ 0; an implementation may skip it otherwise. **Skipping, threads and width** (amended 2026-11-18): when no rule uses the wet share, B2c may be skipped (no stream, no output, no observable change; nor is its entry guard then checked). Each pass may run in G6 row blocks (a row pass then a column pass, P1-00 D2); the stored first-pass and second-pass sums fit `int32`, since every one is a window sum of at most 125² first-pass values of at most 125² each: ≤ 125^4 = 244140625 < 2^28. A sliding window's running sum never holds more than one window, so it fits `int32` too; only a whole-row or whole-grid prefix (a summed-area table) needs `int64` (section 7). The seam `biome_wet_share` returns `int64`.

### B3. Jittered climate (D1, G6 row blocks)

Unchanged. For each cell with `water` ∈ {0, 1}:

```
nt = fbm_m(K_tj, X, Y, jitter_noise)
nr = fbm_m(K_rj, X, Y, jitter_noise)
tq = from_ratio(temperature, 10) + mul(temp_jitter_c, nt)
rq = mul(from_int(rainfall), max(0, ONE + mul(rain_jitter, nr)))
```

The jitter is used only for the lookup; the climate layers are not changed. The share and the river distance are not jittered.

### B4. Classification (D1, G6 row blocks)

Once per world: with N the map side (on a W × H test grid, N = max(W, H)), the size entry is 512 if N ≤ 512, 1024 if 512 < N ≤ 1024, otherwise 2048 (total: every N ≥ 1 selects exactly one). sc = `rain_scale_<entry>` and ws = `wet_scale_<entry>`. For a rainfall bound b that is not a sentinel, rb(b) = `mul_int(sc, b)` (exact). For each rule r whose `wet_frac_min` is not 0, need(r) = mul(r.`wet_frac_min`, ws).raw (P0-01 `mul`, round half away from zero; computed once per world).

```
water = 3:  biome = code of the definition with water_body "ocean"
water = 2:  biome = code of the definition with water_body "lake"
otherwise:  for each land definition with priority > 0, in descending priority:
                if any rule r of it matches (all of the comparisons below,
                    each skipped when its bound is the sentinel of 4.1):
                    r.temp_min_c ≤ tq                  (skipped if r.temp_min_c = −60)
                    tq ≤ r.temp_max_c                  (skipped if r.temp_max_c = 50)
                    rb(r.rain_min_mm) ≤ rq             (skipped if r.rain_min_mm = 0)
                    rq ≤ rb(r.rain_max_mm)             (skipped if r.rain_max_mm = 10000)
                    10 × r.elev_min_m ≤ elevation      (skipped if r.elev_min_m = 0)
                    elevation ≤ 10 × r.elev_max_m      (skipped if r.elev_max_m = 3000)
                    S2 × 2^64 ≤ (320 × r.slope_max.raw)²   (U128, exact, P1-00 G8; skipped if r.slope_max = 100)
                    from_int(hand) ≤ mul_int(r.hand_max_m, 10)   (skipped if r.hand_max_m = 3000)
                    coast_dist ≤ r.coast_max_m         (skipped if r.coast_max_m = 32767)
                    16 × d34 ≤ 3 × r.river_dist_max_m  (skipped if r.river_dist_max_m = 32767)
                    need(r) × wet_total ≤ wet_count × 2^32   (skipped if r.wet_frac_min = 0)
                then biome = its code; stop
            if none matched: biome = code of the fallback definition
```

Rules within a definition are tried in array order; the result does not depend on it (any match selects the definition). Priorities are distinct (named check 6), so the order is total. The order of the comparisons within a rule does not change the result. B4 is evaluated for river cells too (`water` = 1, whose d34 is 0 when they are targets); B5 decides whether its result is kept. When need(r) rounds to 0 (a `wet_frac_min` of a few raw units with ws < 1) the comparison holds on every cell, as for the sentinel; an implementation may skip it.

### B5. River cells (D1 on the B4 grid, G6 row blocks)

Unchanged. For each cell c with `water` = 1: among its existing 8-neighbours with `water` = 0, count the B4 codes. If there is at least one, biome[c] = the code with the largest count, ties to the smallest code; otherwise biome[c] keeps its B4 value. B5 reads only B4 values of dry cells, which B5 never changes, so it may write the same grid in place, in any order.

### B6. Write

Layer `biome`; then the `biome` table, one row per loaded definition in ascending `code`: `id` = `code`, `key` = definition id, `name`, `colour`, `water` (0 or 1). Both in the stage transaction.

### 5.1 Worked examples

Computed by the spec writer's exact-integer reference (`core_reference.py`, `p1_elevation_reference.py` and `p1_biomes_reference.py` from `worldgen/tests/reference/`, with B2b, B2c and the two new comparisons written from this section: `~/.cache/the-unmapped/out/p1-04b-spec/ex.py`), checked by hand where marked, and cross-checked against the exploration build on a real world (the real cells below). The G7 examples of the second version are unchanged and kept in full:

**G7 on P1-02's worked example (5.1 there).** Surface S = e + `water_depth`: 0 on the ocean ring, 28 on the four lake cells, e elsewhere. The two river cells (3, 4) and (4, 5) have class 2. hand (dm), rows y = 1 to 5 (rows 0 and 6 are ocean, 0), first with k = 1 (every river cell a drain; P1-05 uses this):

```
  1    0   60   70   80   70   60   50    0
  2    0   50    0    0   32   40   30    0
  3    0   45    0    0   27   35   25    0
  4    0   40    2    0   15   30   20    0
  5    0   35   33   31    0   20   10    0
```

With k = 3 (the shipped `drain_river_class_min`), the class-2 river cells are not drains; row 4 becomes `0 40 30 28 40 30 20 0` and row 5 `0 35 33 31 25 20 10 0`, rows 1 to 3 unchanged. By hand: (3, 4) drains SE (code 1) to (4, 5), which drains S to the ocean, so hand = 28 − 0; (2, 4) drains E to (3, 4) and on to the ocean: 30; (4, 4) drains SE to (5, 5) and on: 40. With k = 1, (4, 2) drains W into the lake (level 28): 60 − 28 = 32; (2, 4) drains E to the river cell (3, 4) at 28: 30 − 28 = 2.

**B2b river distance**, by hand. An 8 × 5 grid; column x = 7 is ocean; river cells (0, 1) and (1, 1) of class 2 (not targets with k_r = 4), (2, 2) of class 4 and (3, 3) of class 5 (targets). After the forward pass (F = FAR), rows y = 0 to 4:

```
 F  F  F  F  F  F  F  F
 F  F  F  F  F  F  F  F
 F  F  0  3  6  9 12 15
 F  4  3  0  3  6  9 12
 8  7  4  3  4  7 10 13
```

After the backward pass (the result):

```
 8  7  6  7  8 11 12 15
 7  4  3  4  7  8 11 14
 6  3  0  3  4  7 10 13
 7  4  3  0  3  6  9 12
 8  7  4  3  4  7 10 13
```

By hand: (0, 0) is two diagonal steps from (2, 2): 8; (7, 0) is dx 4, dy 3 from (3, 3): 3 × 4 + 3 = 15 (from (2, 2): 3 × 5 + 2 = 17). The class-2 cells (0, 1) and (1, 1) get 7 and 4 like dry cells. The same grid with k_r = 6 has no target: FAR everywhere. With the shipped `river_dist_max_m` 112: 3 × 112 = 336, so a cell matches when d34 ≤ 21 (16 × 21 = 336 ≤ 336; 16 × 22 = 352): seven orthogonal steps (112 m exactly) or five diagonal ones (113.1 m by Euclid; d34 20) match, eight orthogonal steps (d34 24) do not.

**B2c wet share**, by hand. A 6 × 4 grid; column x = 0 is ocean (hand 0, never wet); (1, 1) a river cell and (2, 2) a lake cell (hand 0); `wet_hand_m` 5 (50 dm); hand (dm), rows y = 0 to 3:

```
 0  10  40  80  90 120
 0   0  20  51  60  70
 0   5   0  30 200 300
 0  30  60  10 250 400
```

wet (51 dm is above 50; the ocean column is not wet):

```
0 1 1 0 0 0
0 1 1 0 0 0
0 1 1 1 0 0
0 1 0 1 0 0
```

With `wet_window_m` 32 to 47 (r = 2, h = 1), wet_count and wet_total:

```
count                 total
15 26 27 18  7  1     25 40 45 45 40 25
23 41 43 31 13  3     40 64 72 72 64 40
22 41 44 35 16  5     40 64 72 72 64 40
13 25 27 23 11  4     25 40 45 45 40 25
```

By hand, (0, 0): the first pass gives 2, 4, 3 and 6 wet cells at (0, 0), (1, 0), (0, 1) and (1, 1) (windows of 4, 6, 6 and 9 cells); the second pass's window at (0, 0) is those four cells, so count = 2 + 4 + 3 + 6 = 15 and total = 4 + 6 + 6 + 9 = 25 (share 0.6; a single box of radius 2 would give 6 / 9). At the shipped 0.45 (need raw 1932735283 at 512 and 1024) the cells (0, 0) to (2, 0), (0, 1) to (2, 1), (0, 2) to (3, 2) and (0, 3) to (3, 3) pass, for example (3, 2): 1932735283 × 72 = 139156940376 ≤ 35 × 2^32 = 150323855360; (3, 1) fails: 31 / 72 = 0.43. The shipped window (h = 15) on an interior cell gives wet_total = 31^4 = 923521.

**Exactness of the share comparison.** "0.45" is raw 1932735283 (0.45 × 2^32 = 1932735283.2), so a share of exactly 0.45 passes at 1024: count 9, total 20: 1932735283 × 20 = 38654705660 ≤ 9 × 2^32 = 38654705664. At 2048, need = mul(1932735283, "1.5" raw 6442450944) = RHA(2899102924.5) = 2899102925 (0.675 × 2^32 = 2899102924.8), so a share of exactly 0.675 fails: count 27, total 40: 115964117000 > 27 × 2^32 = 115964116992; count 28 passes.

**B4 lookups** with the shipped definitions. Inputs: N, `temperature` (tenths), `rainfall`, `elevation` (dm), S2, hand (dm), `coast_dist` (m), d34, wet count / total; nt = nr = 0, so tq = temperature / 10 and rq = rainfall. F = FAR (no target in the world). "Sole failure" names the one comparison that rejected the higher-priority biome named.

| Case | N | temp | rain | elev | S2 | hand | coast | d34 | count / total | Biome | Why |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| V1 | 1024 | 80 | 900 | 30 | 100 | 20 | 200 | F | 9 / 10 | fen | |
| V2 | 1024 | 80 | 900 | 30 | 100 | 20 | 200 | F | 8 / 20 | temperate_forest | fen's sole failure the share (0.4); marsh fails hand (2 m) |
| V3 | 1024 | 80 | 900 | 30 | 100 | 10 | 200 | F | 8 / 20 | marsh | fen's sole failure the share; hand 1 m inclusive for marsh |
| V4 | 1024 | 80 | 900 | 30 | 100 | 10 | 200 | F | 9 / 20 | fen | share exactly 0.45 passes (above) |
| V5 | 1024 | 80 | 900 | 30 | 100 | 31 | 200 | F | 20 / 20 | temperate_forest | fen's sole failure hand (3.1 m) |
| V6 | 1024 | 80 | 900 | 30 | 257 | 20 | 200 | F | 20 / 20 | temperate_forest | fen's sole failure slope |
| V7 | 1024 | 80 | 499 | 30 | 100 | 20 | 200 | F | 20 / 20 | steppe | fen's sole failure rain (500); forest fails rain; wet woodland fails river distance |
| V8 | 1024 | 80 | 900 | 2001 | 100 | 20 | 200 | F | 20 / 20 | temperate_forest | fen's sole failure elevation (200.1 m) |
| V9 | 2048 | 80 | 900 | 30 | 100 | 20 | 200 | F | 26 / 40 | temperate_forest | 0.65 < 0.675 (ws 1.5) |
| V10 | 2048 | 80 | 900 | 30 | 100 | 20 | 200 | F | 27 / 40 | temperate_forest | exactly 0.675 fails (above) |
| V11 | 2048 | 80 | 900 | 30 | 100 | 20 | 200 | F | 28 / 40 | fen | |
| V12 | 512 | 80 | 900 | 30 | 100 | 20 | 200 | F | 9 / 20 | fen | ws 1 at 512 |
| W1 | 1024 | 80 | 650 | 1000 | 100 | 40 | 2000 | 21 | 0 / 100 | wet_woodland | 112 m inclusive; forest fails rain |
| W2 | 1024 | 80 | 650 | 1000 | 100 | 40 | 2000 | 22 | 0 / 100 | grassland | wet woodland's sole failure river distance; steppe fails rain |
| W3 | 1024 | 80 | 449 | 1000 | 100 | 40 | 2000 | 21 | 0 / 100 | steppe | wet woodland's sole failure rain |
| W4 | 1024 | 80 | 650 | 3001 | 100 | 40 | 2000 | 21 | 0 / 100 | grassland | wet woodland's sole failure elevation (300.1 m); heath fails temperature |
| W5 | 1024 | 80 | 650 | 1000 | 100 | 51 | 2000 | 21 | 0 / 100 | grassland | wet woodland's sole failure hand (5.1 m) |
| W6 | 1024 | 80 | 600 | 1000 | 100 | 40 | 2000 | 22 | 0 / 100 | steppe | |
| W7 | 1024 | 80 | 650 | 1000 | 100 | 40 | 2000 | F | 0 / 100 | grassland | no target river in the world |
| W8 | 2048 | 80 | 394 | 1000 | 100 | 40 | 2000 | 21 | 0 / 100 | wet_woodland | minimum 0.875 × 450 = 393.75 |
| W9 | 2048 | 80 | 393 | 1000 | 100 | 40 | 2000 | 21 | 0 / 100 | steppe | |
| H1 | 1024 | 30 | 720 | 5000 | 10000 | 500 | 3000 | F | 0 / 100 | highland_moor | |
| H2 | 1024 | 30 | 719 | 5000 | 10000 | 500 | 3000 | F | 0 / 100 | heath | moor's sole failure rain |
| H3 | 512 | 30 | 809 | 5000 | 10000 | 500 | 3000 | F | 0 / 100 | heath | moor bound 810 at 512 |
| H4 | 512 | 30 | 810 | 5000 | 10000 | 500 | 3000 | F | 0 / 100 | highland_moor | |
| H5 | 1024 | 41 | 650 | 5000 | 10000 | 500 | 3000 | F | 0 / 100 | grassland | heath's sole failure temperature |
| H6 | 1024 | 41 | 590 | 5000 | 10000 | 500 | 3000 | F | 0 / 100 | steppe | steppe on high ground too |
| H7 | 1024 | 40 | 300 | 2999 | 10000 | 500 | 3000 | F | 0 / 100 | steppe | heath's sole failure elevation (299.9 m) |
| P1 | 1024 | 80 | 600 | 400 | 100 | 100 | 2000 | F | 0 / 100 | steppe | bound inclusive |
| P2 | 1024 | 80 | 601 | 400 | 100 | 100 | 2000 | F | 0 / 100 | grassland | steppe's sole failure rain |
| P3 | 512 | 80 | 675 | 400 | 100 | 100 | 2000 | F | 0 / 100 | steppe | 1.125 × 600 |
| P4 | 512 | 80 | 676 | 400 | 100 | 100 | 2000 | F | 0 / 100 | grassland | |
| P5 | 2048 | 80 | 525 | 400 | 100 | 100 | 2000 | F | 0 / 100 | steppe | 0.875 × 600 |
| P6 | 2048 | 80 | 526 | 400 | 100 | 100 | 2000 | F | 0 / 100 | grassland | |

**The second version's cases B1 to B21 and S1 to S6** (5.1 there: same inputs, nt and nr as given there), now with d34 = F and count / total = 0 / 1: B1 to B9, B11 to B14, B16 to B19 and S1 to S6 give the same biome as in the second version. Four change: **B10** (temp 40, rain 650, elev 3000, N 1024) and **B15** (tq 0.6, rain 650, elev 6500) are now **heath** (moor needs 720 mm); **B20** (N 2048, rain 437) and **B21** (N 1024, rain 499) are now **steppe** (525 and 600 mm). With count / total = 1 / 1 instead, B3, B5, B6 and B19 become **fen** (hand ≤ 3 m, share 1). With d34 = 0 instead, B20 and B21 become **wet_woodland**. S1 to S6 also show the new sentinels: alpine's `river_dist_max_m` 32767 and `wet_frac_min` 0 accept d34 = F and count 0.

**B5.** Unchanged: a river cell whose dry neighbours have codes 3, 3, 4, 4, 5 gets 3 (two each of 3 and 4; the tie goes to the smaller code); with 4, 4, 4, 3 it gets 4; a river cell with no dry neighbour keeps its B4 code.

**Real cells.** 1024 seed 3: elevation and hydrology layers of the 0.8.0 validation sweep (unchanged by this revision), temperature and rainfall from the exploration build with the final data (P1-03 revised: offset 1.4 °C for seed 3, coast 40 mm); d34, the wet share, hand (G7, k = 3), S2 and the noise by the exact reference from this text. The reference's B4 on 3000 random dry cells of this world gives the exploration build's `biome` on every one (0 mismatches; 1215 forest, 1048 grassland, 207 steppe, 150 alpine, 136 fen, 98 moor, 67 heath, 52 wet woodland, 27 marsh). wet_total is 923521 on all of these cells.

| Cell | temp | rain | elev | S2 | hand | coast | d34 | count | nt (raw) | nr (raw) | tq (raw) | rq (raw) | Biome |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| (259, 736) | 86 | 699 | 757 | 41 | 13 | 1882 | 16 | 641749 | 455563771 | −999575947 | 37392282517 (8.706) | 2862441422793 (666.464) | fen (share 0.695) |
| (284, 361) | 78 | 942 | 1 | 13 | 1 | 16 | 315 | 148404 | −1017221412 | −752647596 | 32483523497 (7.563) | 3904060385934 (908.985) | marsh (share 0.161, not fen) |
| (472, 386) | 75 | 724 | 858 | 545 | 12 | 1226 | 3 | 438811 | 83850000 | −253231258 | 32296104720 (7.520) | 3072888435856 (715.463) | wet_woodland (under 720; fen and marsh fail slope) |
| (684, 466) | 82 | 593 | 294 | 3250 | 157 | 2493 | 19 | 333131 | 350524620 | −202881697 | 35569256447 (8.282) | 2522853837501 (587.398) | steppe (wet woodland fails hand 15.7 m) |
| (279, 647) | 27 | 641 | 9106 | 79330 | 8351 | 3178 | 119 | 136 | −1289992732 | 286353072 | 10306418967 (2.400) | 2789784500310 (649.547) | heath |
| (311, 406) | 30 | 2293 | 7456 | 647744 | 7456 | 662 | 235 | 2481 | −2578119 | −184783332 | 12882323769 (2.999) | 9763618374590 (2273.270) | highland_moor |
| (289, 579) | −57 | 648 | 21387 | 1348085 | 12482 | 2224 | 307 | 0 | 350321980 | 1031307981 | −24130991607 (−5.618) | 2916796322016 (679.120) | alpine |
| (419, 381) | 73 | 711 | 1072 | 2601 | 506 | 691 | 51 | 40243 | 1277719975 | 1681520217 | 32630981236 (7.597) | 3292833922029 (766.673) | temperate_forest |
| (656, 727) | 63 | 653 | 5062 | 55060 | 2604 | 2211 | 144 | 1049 | −61014387 | 153219071 | 26997279578 (6.286) | 2824624054830 (657.659) | grassland |

## 6. Invariants

1. Every cell's `biome` is the `code` of a loaded definition; never 0.
2. `biome` = ocean's code ⟺ `water` = 3; = lake's code ⟺ `water` = 2.
3. No land cell has a biome with `priority` 0 other than the fallback (with the shipped data: `blighted_waste` covers 0 cells).
4. Every dry land cell's (`water` = 0) biome is the highest-priority definition with a matching rule, or the fallback if none matches; every river cell's biome is B5's mode of its dry neighbours' biomes, or its own B4 result if it has none (rechecked by the test with its own rule evaluation).
5. The `biome` table has exactly one row per loaded definition, `id` = `code`, ascending, and its contents equal the definitions.
6. Pure function of the seed, N, the input layers, the biome definitions and `worldgen.biomes`; independent of thread count.
7. With `rain_scale_512` = `rain_scale_1024` = `rain_scale_2048` and `wet_scale_512` = `wet_scale_1024` = `wet_scale_2048` the result does not depend on which entry applies.
8. **B2b and B2c.** d34 equals the closed form of B2b on every cell (FAR on a grid without a target); 0 ≤ wet_count ≤ wet_total; wet_total depends only on W, H, the cell's position and h, and equals (2h + 1)^4 where the cell is at least 2h from every edge; wet_count = wet_total where every cell within 2h is wet.
9. **Identity to 0.8.0.** Every elevation and hydrology layer, table `pass` and `climate.wind_toward` are byte-identical to 0.8.0 for every seed and size (this revision does not touch those stages or the wind draw). With the 0.8.0 data (eight definitions, no new rule fields, `coast_rain_mm` 75, no offset) the 0.8.0 algorithm is a special case of this one: adding `river_dist_max_m` = 32767 and `wet_frac_min` = 0 to every rule and a zero offset gives the 0.8.0 `biome` layer cell for cell (the exploration build reproduced the 0.8.0 validation worlds' file hashes this way for 512 seed 7 and 1024 seed 3; variety-definition section 4). Everything else in a world file changes: `content_hash`, `temperature` (offset), `rainfall` (coast bonus, near coasts only), `biome`, table `biome` (twelve rows), table `climate` (new column), the schema version and `generator_version`.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| B2 G7 | D5 | FIFO from drain cells in index order (P1-00 7.7); the result is a function, independent of the order |
| B2 G8 | D1 | |
| B2b | sequential | two raster passes in the stated order; integer minimum; the result equals a closed form, so any exact method agrees |
| B2c | D2, two passes, each parallel by G6 row blocks if wanted | exact integer sums; any split gives the same sums; pass 2 starts after pass 1 has finished |
| B3, B4 | D1, parallel by G6 row blocks | location-keyed noise; rules in a fixed order; B4 starts after B2, B2b and B2c have finished |
| B5 | D1 on the finished B4 grid, parallel by G6 row blocks | reads dry cells only, writes river cells only; all of B4 finishes before B5 starts |
| B6 | sequential | rows in ascending `code` |

No floating point, no new math function, no randomness in B2b or B2c, no new stream.

Overflow (N ≤ 2048; any data inside the ranges of 4):

- B3, B4 as before: |tq| ≤ 60 + 10; rq ≤ 10000 × 2; rb ≤ 4 × 10000 (`mul_int` raw ≤ 4 × 2^32 × 10^4 < 2^48); `mul_int(hand_max_m, 10)` ≤ 30000; S2 < 2^33 so S2 × 2^64 < 2^97 and 320 × `slope_max`.raw < 2^47, (·)² < 2^94, both `U128`. hand ≤ 30000. B5 counts ≤ 8.
- B2b: finite values ≤ 3 × (max(W, H) − 1) + (min(W, H) − 1): < 2^14 at 2048, < 2^26 on any grid of at most 2^24 cells (a 1 × 2^24 strip reaches 50,331,645); FAR = 2^40 and FAR + 4 < 2^41; 16 × d34 ≤ 16 × 2^40 = 2^44; 3 × `river_dist_max_m` ≤ 98301. No guard is needed for any grid the P1-00 grid checks admit (sides ≤ 2^31 − 1, so finite values < 2^33).
- B2c: h ≤ (2000 div 16) div 2 = 62, so a first-pass window holds at most 125² = 15625 cells and a second-pass sum at most 125^4 = 244140625 < 2^28. Any prefix sum the implementation forms over a whole row or the whole grid of first-pass values is at most W × H × 15625 ≤ 2^22 × 15625 < 2^36 at 2048 (< 2^45 for any grid with W × H ≤ 2^31). The comparison: need(r) ≤ mul(1, 4).raw = 2^34, so need × wet_total < 2^34 × 2^28 = 2^62, and wet_count × 2^32 < 2^60: both fit `int64`. With the shipped values wet_total ≤ 923521 < 2^20 and need ≤ 2899102925 < 2^32, so the products are below 2^52.
- **Why `wet_window_m` is bounded by 2000** (the lesson of P1-03 C4, where an unguarded seam could wrap): the exploration declared int(0, 4000) and compared in `int64`; at r = 250 (h = 125) wet_total reaches 251^4 ≈ 3.97 × 10^9 ≈ 2^31.9, so need × wet_total could reach 2^65.9 and wet_count × 2^32 2^63.9, both past `int64`. Halving the bound keeps every value above inside `int64` with a margin of 2. (Evaluating the comparison in `U128`, as G8 does, would also be exact and is allowed; the bound is still 2000.)
- **Entry guard of the B2c seam** (`biome_wet_share`, section 10), checked before any summation or allocation, fatal with the prefix `biomes: input precondition: `, in this order: (1) r_cells in 0..125, message `B2c window r_cells outside 0..125`; (2) W ≥ 1, H ≥ 1 and W × H ≤ 2^31, message `B2c grid is empty or has more than 2^31 cells`; (3) W × H ≤ 2^24 (the P1-00 grid limit), message `B2c grid has more than 2^24 cells`; (4) both arrays of length W × H, message `B2c array length is not width * height`. The real limit is r_cells ≤ 125, not `wet_window_m` ≤ 2000: since r = `wet_window_m` div 16, a hand-built tuning with `wet_window_m` 2001 to 2015 (outside the schema range, so never from data) still gives r = 125 and is inside the overflow bound; 2016 gives 126 and is fatal at the guard. The compile-time assertions that the stage cannot trip it: 2000 div 16 = 125, and 2048² ≤ 2^31. `biome_layer` reaches B2c only through this guard, after its own preconditions, G7 and the rule checks (section 10.5).

## 8. Performance

**Budget (amended 2026-11-18): 0.15 / 0.5 / 2 s at 512 / 1024 / 2048** (P1-00 11; the 512 figure was 0.1 s). **Implementation figures** (branch `p1/04b-biome-variety`, gcc-release, 6 threads, validation of 0.9.0): the biome stage as a run difference (full `generate` against `--until climate`) 0.10 / 0.24 / 0.76 s; `biome_layer` alone 71 / 237 / 883 ms; of that, B2b and B2c together 3.3 / 13.7 / 77.7 ms. At 1024 and 2048 the stage is at half its budget or less. At 512 the computation meets 0.1 s with a 30% margin, but the run difference, which also holds encoding and writing the layer, the twelve-row table and the stage bookkeeping (and has a noise of a few tens of milliseconds), sits exactly at it. **Decision: raise the 512 budget to 0.15 s.** Reasons: the new steps cost 3 ms there, so the stage is at its limit because of fixed per-stage costs, not the algorithm; the run difference is the quantity P1-00 11 measures and a budget that the measurement meets only on its edge makes every sweep a coin toss; the other fallbacks (3-octave jitter) change the maps for 30 ms that no user sees; and the geography total at 512 (4 s) is unchanged, since the stage rows already summed to 3.7 s. 1024 (the defect threshold of P1-00 11) and 2048 are not changed.

The exploration's figures, before implementation: the 0.8.0 stage measured 84–111 / 263–285 / 923–957 ms (`biome_layer` alone, best of five, 6 threads; validation 7). The two new steps are linear and cheap: two sequential raster passes (B2b) and four box passes of integer sums (B2c). Measured with the exploration's `worldgen-restage` (climate and biomes on stored layers, the stages' own library calls; gcc-release; quiet machine; two repeats, which agreed within 2%), wall time in ms for threads 1 / 2 / 5, 0.8.0 data against the final data:

| World | 0.8.0 data | final data (this spec) | difference | the exploration's round two (with the wet-share jitter) |
| --- | --- | --- | --- | --- |
| 512 s0 | 912 / 525 / 330 | 915 / 537 / 340 | +3–12 / +10 | 1030 / 597 / 370 |
| 1024 s3 | 3530 / 2026 / 1228 | 3576 / 2075 / 1262 | +46 / +49 / +34 | 4024 / 2310 / 1360 |
| 2048 s0 | 14030 / 8070 / 4546 | 14247 / 8246 / 4706 | +217 / +176 / +160 | 16019 / 9198 / 5084 |

So the exploration's biome stage gained about 0.01 / 0.04 / 0.2 s (B2b and B2c single-threaded there; the climate offset costs nothing measurable). The implementation (above) runs B2c in row blocks and B2b on `int32`, and the two steps cost 3.3 / 13.7 / 77.7 ms. Dropping the wet-share jitter saved most of the round-two cost (about 1.8 s single-threaded at 2048). If 512 must be held strictly: B2c's passes may run in G6 row blocks (exact in any split); B2b is sequential; beyond that the second version's fallback applies (the jitter at 3 octaves). Threads 1, 2 and 5 gave identical `biome`, `temperature` and `rainfall` in every run (108 comparisons over the two repeats and the three data sets, all equal).

Memory: d34, wet_count and wet_total as `int64` grids add 24 bytes per cell (100 MB at 2048) for the stage's lifetime; wet and the first-pass sums can be freed after B2c. An implementation may store d34 as `int32` with 2^30 in place of FAR (the comparisons are the same: 16 × 2^30 exceeds 3 × 32766, and finite values are below 2^26 on any admitted grid, below 2^14 at 2048) and the B2c results as `int32` (below 2^28); a seam that returns d34 returns 2^40 on cells without a target, as specified.

## 9. Report and render

**`report`**, appended to `[geography]` after the climate keys when `biome` and the `biome` table are present (unchanged rules; P1-03 9 adds `temp_offset_c` to the climate group):

| Key | Value |
| --- | --- |
| `biome_share` | for each `biome` table row with `water` = 0, ascending `id`: `<key>:<ratio4(cells with that code, cells whose biome row has water = 0)>`, separated by single spaces. With the shipped data ten keys: `temperate_forest`, `grassland`, `marsh`, `highland_moor`, `alpine`, `blighted_waste`, `heath`, `steppe`, `wet_woodland`, `fen` |
| `biome_patches` | the number of 8-connected components (G2, equivalence: equal `biome` value) of cells whose biome row has `water` = 0 |

For example (512 seed 0, final data): `biome_share = temperate_forest:0.5810 grassland:0.1447 marsh:0.0068 highland_moor:0.0882 alpine:0.0031 blighted_waste:0.0000 heath:0.0726 steppe:0.1021 wet_woodland:0.0015 fen:0.0000`, `biome_patches = 123`.

`[tables]` lists `biome = <rows>` and `climate = <rows>` in ascending name order with the others (every world of schema 4 has both tables, also `--until none`: `biome = 0`, `climate = 0`; a full run: `biome = 12`, `climate = 1`).

The two keys are printed when the `biome` layer exists. When the table has no row with `water` = 0, `biome_share` prints `n/a` (and `biome_patches` counts no cell: `0`). Then, unchanged:

- **Every row of table `biome` is checked** (whether or not the layer exists): `id` in 1..254, `key` non-empty, `colour` in 0..16777215, `water` 0 or 1. A row that fails is a corrupt-data error naming the table, as for `pass` (P1-00 10.3) and `climate` (P1-03 9): `worldgen-cli: error: corrupt table 'biome': row <id>: <reason>` on stderr, nothing on stdout, exit 3; the reason is the first failing check in the order listed (`id outside 1..254`, `empty key`, `colour outside 0..16777215`, `water not 0 or 1`); rows are checked in ascending `id`. (`id` and `key` uniqueness are guaranteed by the DDL.)
- **The `biome` layer must be `u8`, of the same size as `elevation`, and every value must have a row**: otherwise a corrupt-data error naming the layer, `worldgen-cli: error: corrupt layer 'biome': <reason>`, exit 3, with the reason `type <t>, expected u8`, `size <W>x<H>, expected <N>x<N>`, or `value <v> at (<x>, <y>) has no row in table 'biome'` for the lowest such cell index. An empty table with a `biome` layer is therefore this error at cell 0.
- A table with rows and no `biome` layer prints neither key.

**Statistics** (P1-00 13.1; **provisional** until the implementation's validation sweep). Measured by the spec writer on the exploration's 58 calibration worlds (512 seeds 0–19, 1024 seeds 0–29, 2048 seeds 0–7; elevation and hydrology of the 0.8.0 validation sweep) re-staged with the final data of this spec, without either jitter (section 11.1). Typical = measured min–max widened by about 10% (low end × 0.9, high end × 1.1, rounded outward); hard only where a value outside means a defect. The sample is smaller than the 0.8.0 sweep (120 worlds): it lacks the weak-shadow worlds 512 s41, s43 and 1024 s38, s41, s43 (forest 0.60–0.64 in 0.8.0) and the unusual 2048 worlds s13 (flat) and s15 (proposals "Large maps may have unusual worlds"), so the validation sweep will widen the forest and upland ranges.

| Statistic | All seeds (hard) | Typical (≥ 90%), 512 / 1024 / 2048 | Measured min / median / max, 512 / 1024 / 2048 |
| --- | --- | --- | --- |
| `biome_share` temperate_forest | ≤ 0.80 | 0.27–0.64 / 0.29–0.61 / 0.39–0.61 | 0.303 / 0.371 / 0.581; 0.333 / 0.434 / 0.546; 0.440 / 0.505 / 0.551 |
| `biome_share` grassland | ≤ 0.80 | 0.12–0.51 / 0.10–0.39 / 0.11–0.25 | 0.144 / 0.247 / 0.459; 0.116 / 0.230 / 0.349; 0.125 / 0.186 / 0.223 |
| `biome_share` steppe | ≤ 0.80 | 0.007–0.25 / 0.033–0.19 / 0.038–0.18 | 0.008 / 0.090 / 0.219; 0.037 / 0.082 / 0.170; 0.043 / 0.131 / 0.161 |
| `biome_share` wet_woodland | ≤ 0.15 | 0.0009–0.025 / 0.004–0.026 / 0.013–0.026 | 0.0011 / 0.0100 / 0.0222; 0.0045 / 0.0130 / 0.0229; 0.0150 / 0.0190 / 0.0235 |
| `biome_share` marsh | ≤ 0.15 | 0.001–0.016 / 0.0037–0.013 / 0.020–0.034 | 0.0012 / 0.0069 / 0.0141; 0.0042 / 0.0082 / 0.0114; 0.0233 / 0.0269 / 0.0306 |
| `biome_share` fen | ≤ 0.15 | 0–0.035 / 0.0008–0.060 / 0.020–0.084 | 0.0000 / 0.0056 / 0.0313; 0.0009 / 0.0292 / 0.0545; 0.0227 / 0.0645 / 0.0763 |
| `biome_share` highland_moor | ≤ 0.80 | 0.018–0.18 / 0.014–0.13 / (descriptive)–0.078 | 0.021 / 0.070 / 0.156; 0.017 / 0.063 / 0.117; 0.015 / 0.027 / 0.071 |
| `biome_share` heath | ≤ 0.80 | 0.013–0.15 / 0.018–0.11 / (descriptive)–0.045 | 0.015 / 0.054 / 0.132; 0.021 / 0.044 / 0.099; 0.016 / 0.023 / 0.040 |
| `biome_share` alpine | ≤ 0.80 | 0.002–0.18 / 0.007–0.18 / (descriptive)–0.12 | 0.003 / 0.078 / 0.162; 0.009 / 0.067 / 0.162; 0.018 / 0.036 / 0.107 |
| `biome_share` blighted_waste | 0 | — | 0 |
| `biome_patches` | descriptive | — | 104 / 144 / 169; 400 / 550 / 687; 1932 / 2477 / 2657 (a sudden rise means speckle) |

Notes: fen is under 0.1% of land in 5 of 20 worlds at 512 (seeds 0, 2, 10, 13, 17) and 1 of 30 at 1024 (seed 2); that is accepted (limitation 2), so its 512 range starts at 0. The 2048 lower bounds of the three upland biomes are descriptive: an occasional 2048 world is nearly flat (proposals "Large maps may have unusual worlds"; 0.8.0's 2048 s13 had 1.4% alpine and 2.1% moor), and the per-world offset moves alpine and moor further (−3.4 / −3.0 / −1.8 points of alpine per +1 °C at 512 / 1024 / 2048, variety-definition 9). Alpine's typical minimum is low at every size for the same reason: the warmest worlds (offset +2.0 °C) keep only a summit cap (512 s0: 0.31%). No biome exceeds 0.59 in any of the 58 worlds (largest: forest 0.581 at 512 s0). The hard bound 0.15 on marsh, fen and wet woodland marks a broken wet rule (the first marsh rule gave 14–28% at 2048).

**Palette `biome`** (unchanged): suits `u8` layers only (P1-00 appendix C 1; any values). Each value gets the colour of the `biome` table row whose `id` equals it, as 0xRRGGBB; a value with no row, or a world without the table or with an empty one, (255, 0, 255) (render draws; it does not validate as `report` does). Registered under `auto` for the layer `biome`. `--palette biome` on a layer of another type, for example `elevation` (`i16`), is a usage error (exit 2) whose message says the palette needs a `u8` layer. Since every colour comes from the table, the new biomes need no render or viewer change (the viewer draws the layer from table `biome` since `19a1624`). **Palette requirement on the data:** the twelve shipped colours are those of 4.1 (the world critic's palette, `~/.cache/the-unmapped/out/p1-04b-variety/critic-review.md` 6), pairwise distinct; a change of colour is a data change (bump and golden re-record), not a code change.

**Composite `biomes`** (unchanged; `--composite biomes`; default output `<stem>.biomes.png`). Needs `elevation` and `biome`; uses the table for colours (as the palette: a missing row, an empty table or a missing table draws (255, 0, 255) and is not refused; only a missing or unsuitable layer is) and `water`, `river` and `flow_dir` when present. Without `elevation`, or without `biome` (checked in that order), `render` exits 3 with the reader's message and the layer list, as for `--composite map` (P1-00 appendix C 4): `worldgen-cli: error: world file '<path>' has no layer 'biome'; available layers: <names, comma separated>` (or `…; the world has no layers`), and writes no file. A `biome` layer that is not `u8`, or not the size of `elevation`, is refused like the optional layers of `map` (`composite_biomes_problem`, which applies `composite_map_problem`, P1-00 10.2, then checks `biome`): exit 3 with `worldgen-cli: error: cannot render the composite biomes: layer 'biome' is i16, expected u8; available layers: <names>` (for an `i16` layer), or `…: layer 'biome' is not the size of layer 'elevation'; available layers: <names>`. Per cell: ocean (e < 0): the `hypsometric` ocean colour; otherwise the biome colour, shaded exactly as step 2 of the composite `map` (P1-00 10.2); then steps 3 and 4 of `map` (lake colour and the river drawing) when `water` is present. `render --help` lists every palette `--palette` accepts, `biome` included (P1-00 10.2).

## 10. Tests required

Library seams, as implemented in `worldgen/include/worldgen/stages/biomes.hpp` (namespace `worldgen::stages`; amended 2026-11-18 to the 0.9.0 implementation, branch `p1/04b-biome-variety`). Each raises `FatalError` on a contract violation (an array of the wrong length, threads outside [1, 256], a violated precondition); `threads` only schedules G6 row blocks of B2c to B5 and never changes a result. The engineer may add helpers but should not change these without a spec note.

```cpp
// Records (4.1, 4.2): BiomeRule gains, after coast_max_m,
//   std::int64_t river_dist_max_m;   // 16 d34 <= 3 bound; 32767 = no limit
//   Fx           wet_frac_min;       // scaled by the wet scale; 0 = no limit
// BiomeTuning gains river_dist_class_min (int64), wet_window_m (int64), wet_hand_m (Fx),
//   wet_scale_512, wet_scale_1024, wet_scale_2048 (Fx).
BiomeDefinitions biome_definitions_from(const data::ContentSet& content);
BiomeTuning biome_tuning_from(const data::ContentSet& content);          // the 13 fields of 4.2
Fx biome_rain_scale(const BiomeTuning& tuning, std::int64_t n);          // fatal if n < 1
Fx biome_wet_scale(const BiomeTuning& tuning, std::int64_t n);           // same size rule; fatal if n < 1
// B2b: int64 per cell, BIOME_D34_FAR = 2^40 on every cell when there is no target.
std::vector<std::int64_t> biome_river_distance(geo::Grid grid, std::span<const std::uint8_t> water,
                                               std::span<const std::uint8_t> river, std::int64_t k);
// B2c: h = r_cells div 2; entry guard of section 7.
struct BiomeWetShare { std::vector<std::int64_t> count; std::vector<std::int64_t> total; };
BiomeWetShare biome_wet_share(geo::Grid grid, std::span<const std::uint8_t> water,
                              std::span<const std::int64_t> hand, Fx wet_hand_m, std::int64_t r_cells,
                              std::uint32_t threads = 1);
// B4 for one rule and for the whole lookup (argument order as here).
bool biome_rule_matches(const BiomeRule& rule, Fx scale, Fx wet_scale, Fx tq, Fx rq,
                        std::int64_t elevation_dm, std::int64_t s2, std::int64_t hand_dm,
                        std::int64_t coast_dist, std::int64_t d34, std::int64_t wet_count,
                        std::int64_t wet_total);
std::uint8_t biome_lookup(const BiomeDefinitions& definitions, Fx scale, Fx wet_scale, Fx tq, Fx rq,
                          std::int64_t elevation_dm, std::int64_t s2, std::int64_t hand_dm,
                          std::int64_t coast_dist, std::int64_t d34, std::int64_t wet_count,
                          std::int64_t wet_total);
std::vector<std::uint8_t> biome_river_cells(geo::Grid grid, std::span<const std::uint8_t> water,
                                            std::span<const std::uint8_t> biome, std::uint32_t threads = 1);
std::vector<std::uint8_t> biome_layer(const BiomeInputs& inputs, const BiomeDefinitions& definitions,
                                      const BiomeTuning& tuning, std::uint64_t seed, std::uint32_t threads = 1);
```

G7 stays `height_above_drainage(grid, S, water, river, flow_dir, k)` (shared with P1-05, k = 1 there). The climate seams (`climate_temp_offset_dc`, the `offset_dc` argument of `climate_temperature_cell` and `climate_temperature`; `ClimateTuning` gains `Fx temp_offset_max_c`) are in P1-03 10.

### 10.1 Unit tests

1. G7 on P1-02's worked example exactly with k = 1 and k = 3 (5.1). Its failures, each fatal with the prefix `G7: ` through the seam (and `biomes: input precondition: G7: ` through the stage): k = 0 and k = 16; a `flow_dir` cycle among non-drain cells on a hand-made grid; a grid whose only water is river cells of class 2 with k = 3 and no lake or ocean; a non-drain cell whose drain cell has a higher S than the cell itself (negative hand). Not fatal: a non-drain cell c whose path passes through a higher non-drain cell before reaching a drain cell lower than c.
2. B4 cases V1 to W9, H1 to H7, P1 to P6 exactly, through `biome_lookup` with the shipped definitions, the scales of the case's N, and the tq, rq of the case; the second version's B1 to B21 and S1 to S6 with d34 = 2^40 and count / total = 0 / 1, with the four changed results of 5.1, and the variants given there (count / total 1 / 1; d34 0).
3. The size rule: `biome_rain_scale` and `biome_wet_scale` give the 512 entry for N = 1, 16, 128 and 512, the 1024 entry for 513 and 1024, the 2048 entry for 1025, 2048 and 4096; a W × H grid uses N = max(W, H). Sentinels: a rule with `rain_max_mm` 10000 matches rq = 9999 at scale 0.875 and rq = 15000 at any scale; with `rain_max_mm` 9999 it fails rq = 9999 at scale 0.875. A rule with `river_dist_max_m` 32767 matches d34 = 2^40; with 32766 it fails d34 = 2^40 and matches d34 = 32766 × 3 div 16 = 6143 (16 × 6143 = 98288 ≤ 98298) but not 6144. A rule with `wet_frac_min` 0 matches count 0; with raw 1 and ws "0.25" (need = RHA(0.25) = 0) it matches count 0; with raw 1 and ws 1 (need 1) it fails count 0, total 1 and matches count 1, total 1000.
4. B2b: the 8 × 5 example of 5.1 after each pass (the forward pass through a test hook or by the closed form of the half-plane is not required; the final grid is); the same grid with k = 6 (FAR everywhere); a 1 × 1 grid with one target (0); and on 200 random grids up to 12 × 12 (random `water` in {0, 1, 3}, random classes on river cells) the closed form of B2b, computed by brute force over the targets.
5. B2c: the 6 × 4 example of 5.1 exactly (count and total); r_cells 0 and 1 give count = wet, total = 1; r_cells 2 and 3 give the same result (h = 1); with h ≥ max(W, H) − 1 every window of both passes is the whole grid, so every cell gets total = (W × H)² and count = (number of wet cells) × W × H; a 2048 × 2048 grid with every cell wet and r_cells 125 returns exact values (centre total 125^4 = 244140625); the entry guard: r_cells −1 and 126 are fatal with the prefix, a 65536 × 32769 grid (W × H = 2^31 + 65536) is fatal with empty arrays (the guards are checked in the order listed in section 7, before the array lengths and before any allocation).
6. B5 examples of 5.1.
7. A definition with two rules: a cell matching only the second rule gets the biome.
8. Named checks 1 to 7: one failing fixture for each row and each "where" of the table in 4.1, with path, line, column and message; a float literal in a rule is a P0-03 error; a rule missing a field — including a rule missing `river_dist_max_m` or `wet_frac_min` — is a missing-field error at the rule's table header. Tuning: each bound of 4.2 rejects a value one step outside it (`wet_window_m` 2001 among them). Field ranges at the seams: a hand-built rule with `wet_frac_min` raw ONE + 1, or `river_dist_max_m` 32768, given to `biome_lookup` is fatal `biomes: rule: <field> outside its schema range`.
9. The `biome` table of a world generated with the shipped data equals the twelve rows of P1-00 6.6; with `--until climate` the table exists and is empty; `SCHEMA_VERSION` is 4; the `sqlite_schema` rows of an empty world list `biome` with the schema-3 DDL and `climate` with the schema-4 DDL of P1-03 3, byte for byte.
10. B3 structural: for seed 0, N = 64, at 16 cells chosen by the test, tq and rq equal the test's evaluation with `fbm2`.
11. Preconditions of section 2, each fatal with the prefix: a missing `river` layer, a `river` value of 16, a `water` value of 4.
12. Report: `biome_share` and `biome_patches` on a hand-made 6 × 6 world with a twelve-row `biome` table, recomputed by the test (ten keys in code order); `[tables]` lists `biome` and `climate`; each malformed-row case of section 9 and a layer value with no row exit 3 with the stated message and print nothing on stdout.
13. Render: as the second version (`--composite biomes` without `biome`, without `elevation`, with `biome` of type `i16`; an empty `biome` table; `--palette biome` on `elevation`; a missing code draws (255, 0, 255)), and the twelve shipped colours are pairwise distinct.

### 10.2 Property tests

Seeds 0 to 63 at N = 128 (fixture data, all earlier stages): invariants 1 to 5 and 8, with the test's own rule evaluation, its own G7 (recursion along `flow_dir` with the class test), its own B2b (closed form) and B2c (direct sums) and its own B5. Also with a test-only data set in which every biome's rules are the full ranges (all eleven fields at their sentinels): every dry land cell gets the highest-priority biome. Invariant 7 on seed 0 with the three rain scales and the three wet scales set equal.

### 10.3 Determinism tests

Threads 1, 2, 5 identical; resume from a world written after climate equals the uninterrupted pipeline.

### 10.4 Golden values

`worldgen/tests/golden/p1-04-biomes.txt` is regenerated by the test writer's independent reference for this version (the climate inputs change too, P1-03 10.4), with the items of the second version plus, per case, `d34`, `wet_count` and `wet_total`, each the SHA-256 of the grid's `int64` values in little-endian byte order, cell index order (y × W + x), with no header, exactly as the existing item `hand` (d34 with 2^40 on cells without a target), and the `counts` and `biome_share` items over twelve codes. The validation specialist checks the implementation against it.

**Reference values from the exploration build** (branch `p1/04b-biome-variety-explore` at `267d12d`, gcc-release, with the environment switch `UNMAPPED_EXP_SCHEMA4=1` and a data directory equal to the shipped data of this spec, i.e. the exploration's `game/data` with `wet_frac_jitter` removed and `river_dist_max_m` = 32767, `wet_frac_min` = 0 written into every rule that lacked them, which the branch treats as absent; `worldgen-cli generate --map-size N --data DIR`, then `hash --detail` and `report`). Layer hashes (P0-04 8) are content hashes and do not depend on `generator_version`, so they are the 0.9.0 values if the implementation is right; the file hashes are not given (meta differs). Shipped data = fixture `geo` data.

| Case | `temperature` | `rainfall` | `biome` | `temp_offset_dc` | `wind_toward` |
| --- | --- | --- | --- | --- | --- |
| 512 seed 0 | 9aed63dbbaa6a7b0d7e719936d1d06560ad8bcdba022a75d1ef0cadf748f3dfb | eab4acae1dd8def155141e49b23113d76cdac39e6ef77d5a1cf9044427998125 | d936d7be132e5802f5fcd875fa9e2dc96fd7c776c383d926110a318e8bdbba05 | 20 | SE |
| 512 seed 42 | 8f8bbd0f752b4d9cd18414e1aceb236ad4ce74be2e00f4452ec22850ddefde29 | 7432d34c16b6f98cdc7df96e8edfd5f58ae8d1926d72a709de9ec045ca842b18 | e9fa629f7036e64abbce5072538c6bc69e673e666ee486a85c850e30b3b736b0 | 9 | N |
| 512 "The Unmapped" | f8aa66d2161c48aa8b6b4f56eabcdbc4f7f4837175850ca2f6f4f3cb02ffb1a6 | c8f50ab603e9da432e68fca3d8d84927733bb0e0c9b96b058300f0caade0d394 | a88200dffffe3c0492aa90c0edd08ba906493fe9ca6f3e3b5fa224151fdb765a | −10 | NW |
| 1024 seed 1 | 21d4e4bb33d77f1380a66f5ceb783d002d5ada63608da6b0b407cc5474c357c1 | 88fb75484e9eb52ba6c31649f3768ecae3683d5257e9dff41b6e191426bd3bac | 521329a71553a5a6160184aee227b8ffd0b890a3f9612f9535cefe229ed325e3 | 10 | SW |

Logical hash of table `biome` (twelve rows) in all four: a7878ee4c9ef402dfe8dce7f3691f0c0c84e6eb9d15eeb4cd3390e2d199f5472. Logical hashes of table `climate` (three columns): 512 s0 9709df66fb80b17713c5168055a0c0b5c3d60eab2ba7a853ad870803bead7bee, s42 9db9624ecd088e286f671eb722d2395acb51d6355062316314d358dfc8bba645, phrase c96cf24baac00ce6cbfaffd73c3363a4ae7613e93421f4eb4ed22b2129c7362c, 1024 s1 2fd5c14fc4b31e2c2ec4ca8c02f3a2f55e656bb6fab2747cbd436d3833e24570. `report` of 512 seed 0 prints the `biome_share` line of section 9. An empty world (`--until none`) of this build is 8 pages (32768 bytes) with `user_version` 4. The N = 128 cases are computed by the reference only (the command line does not make them). The CI world cases (`p1-worlds.txt`) are re-recorded by the merge.

### 10.5 Fatal cases and the order of checks (amended 2026-11-18, as implemented)

- **`biome_rule_matches`, `biome_lookup`**: first the cell's inputs, in this order: `s2` < 0 (`<seam>: negative S2`), `d34` < 0 (`<seam>: negative d34`), not 0 ≤ `wet_count` ≤ `wet_total` with `wet_total` ≥ 1 (`<seam>: wet share counts outside 0 <= count <= total, total >= 1`), where `<seam>` is `biome_rule_matches` or `biome_lookup`. Then, per rule (for `biome_lookup`: every rule, in B4's order, before any cell is evaluated): the rule's fields in declaration order (`biomes: rule: <field> outside its schema range`), then the rain scale (`biomes: rule: the rain scale is outside [0.25, 4]`), then the wet scale (`biomes: rule: the wet scale is outside [0.25, 4]`). So a bad field of the first rule is reported before a bad scale, and a bad scale before a bad field of the second rule.
- **`biome_layer`**, in this order: threads outside [1, 256] (`biomes: thread count outside [1, 256]`); the grid (P1-00 grid checks); every input array of length W × H, `water` in 0..3, `river` in 0..15 (prefix `biomes: input precondition: `, section 2); G7 (`biomes: input precondition: G7: …`); the rule-field and scale checks above for every rule, once per world (`biomes: rule: …`, no precondition prefix); then, only if some rule uses d34, `river_dist_class_min` outside 1..15 (`biomes: river_dist_class_min outside 1..15`); then, only if some rule uses the share, the B2c entry guard of section 7 (prefix `biomes: input precondition: `; with data from P0-03 it can never fail).
- **`biome_river_distance`**: k outside 1..15 (`biome_river_distance: river class k outside 1..15`); an array of the wrong length.
- **`biome_wet_share`**: the entry guard of section 7, in its order.
- **`biome_rain_scale`, `biome_wet_scale`**: n < 1 (`biome_rain_scale: map size below 1`, `biome_wet_scale: map size below 1`).
- **C3 seams** (P1-03 10): `climate_temperature_cell` and `climate_temperature` are fatal for `offset_dc` outside −50..50.

## 11. Known limitations and calibration

### 11.1 How the revision was calibrated (2026-11-17 and 2026-11-18)

By the engineer on the exploration branch, in two rounds, on 58 worlds (512 s0–19, 1024 s0–29, 2048 s0–7; elevation and hydrology of the 0.8.0 validation sweep), with `worldgen-restage`, which runs the climate and biome stages' library calls on stored layers with a given data directory (checked equal to a full `generate` on samples); then reviewed by the world critic (`~/.cache/the-unmapped/out/p1-04b-variety/critic-review.md`). Round one fixed the biomes, the temperature offset and the coast; round two the palette (critic F2), the triangle window and the fen's 3 m drainage height (critic F1), and tried a wet-share jitter and an elevation jitter. The lead ruled out both jitters.

**Re-measured for this spec without the jitters** (the round-two tables include the wet-share jitter 0.08): the spec writer re-staged the same 58 worlds with the final data (`~/.cache/the-unmapped/out/p1-04b-spec/`, data `data/fin`, runs `runs/fin`, the exploration's analysis scripts with paths changed: `tools/run.sh fin full`, `tools/ana.py fin full`, `tools/summ.py fin full`, summary `fin-summary.txt`). Shares (min / median / max) are in section 9. **The jitter's effect, confirmed:** against the round-two worlds, 43,682 cells differ in all 58 worlds together (per world 0–122 at 512, 115–669 at 1024, 2520–5041 at 2048; at most 0.31% of a world's land, 2048 s7), all on fen and marsh edges; no biome share of any world moves by more than 0.063 percentage points (fen 512 s8 −0.063, marsh 512 s8 +0.062), and every summary measure below is the same to the printed precision or within one unit of its last digit. `temperature` and `rainfall` are identical (the jitter is the biome stage's).

| Measure (final, no jitter) | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| lee lowland forest / grassland / steppe (median) | 0.04 / 0.45 / 0.40 | 0.13 / 0.41 / 0.37 | 0.06 / 0.26 / 0.50 |
| windward lowland forest / grassland / steppe | 0.67 / 0.29 / 0.02 | 0.65 / 0.24 / 0.03 | 0.63 / 0.19 / 0.05 |
| heath share of upland on windward / lee faces (median) | 0.13 / 0.90 | 0.12 / 0.79 | 0.15 / 0.83 |
| large alpine areas ringed by one biome | 0 of 50 | 9 of 116 | 3 of 50 (0.8.0: all 223) |
| coast ≤ 1 km forest / (forest + open), windward / lee (median) | 0.80 / 0.21 | 0.97 / 0.33 | 1.00 / 0.51 |
| fen-dominated 2 km² tiles; worlds with one | 0 of 257; 0 of 20 | 4 of 1611; 3 of 30 | 30 of 1728; 7 of 8 |
| worlds where wetland is the plurality of some 2 km² window | 1 of 20 | 10 of 30 | 8 of 8 |
| largest wet (marsh + fen) patch, median cells | 379 | 2009 | 10542 |
| trees (forest + wet woodland) near class ≥ 4 lowland rivers vs all lowland (median) | 0.61 vs 0.50 | 0.55 vs 0.55 | 0.55 vs 0.59 |
| speckle (land in patches < 16 cells), median / max | 0.29% / 0.43% | 0.28% / 0.37% | 0.35% / 0.39% |
| `land_below_0c` min / median / max | 0.0008 / 0.065 / 0.135 | 0.0017 / 0.059 / 0.145 | 0.016 / 0.032 / 0.096 |

(Lee and windward lowland as in the second version: land below 200 m, lee if tracing against the wind for up to 400 cells meets ground of 800 m or more. Faces: upland cells whose terrain 8 cells upwind is more than 5 m lower (windward) or higher (lee).)

### 11.2 Variants (exploration, for the record)

| Variant | Result |
| --- | --- |
| round one: box window (r 30), fen hand 4 m | fen 0 / .021 / .039 median at 512 / 1024 / 2048, missing in 14 of 20 worlds at 512; discs and staircases |
| + wet-share jitter 0.08 (stream `geo.biomes.wet_jitter`) | almost unchanged: the share falls steeply at a fen's edge, so ±0.08 moves it a cell or two; +0.5 s single-threaded at 2048. **Not adopted** (lead) |
| triangle window (two passes of h 15) | staircase gone; fen .006 / .035 / .073, missing in 5 of 20 at 512; marsh shrinks at 1024 (.015 → .008), since broad river margins pass as fen |
| + fen hand 3 m (**adopted**) | the disc breaks into fen on the lowest ground with dry islands; fen .006 / .029 / .064 |
| fen hand 2 m | most organic, but no fen-dominated tile at 1024 |
| elevation jitter ±60 m (`elev_jitter_m`, stream `geo.biomes.elev_jitter`) | 0 to 1,600 cells per world, no share by more than 0.001: the upland edges are the 4 °C isotherm, not the 300 m bound. **Not adopted** (lead) |
| steppe 550 / 600 / 650 mm | Paul chose 600 (moderate) |
| temperature offset ±1.5, ±2.5, triangular | Paul chose uniform ±2 °C (P1-03 11.2) |

### 11.3 Limitations

1. **Large fens are still rounded.** On a whole plain that is wet and near-level the share is near 1 inside and falls fast at the edge, so a large fen is a rounded patch with dry islands (2048 s5), not a fen following the floodplains. The jitter did not help (11.2).
2. **Fen depends on map size.** Missing (under 0.1% of land) in 5 of 20 worlds at 512 and 1 of 30 at 1024, the plurality of some 2 km² window in 1 of 20 worlds at 512, common at 2048 (fen-dominated tiles in 7 of 8 worlds), even with the 2048 wet scale of 1.5. Small maps are steep with narrow valleys. Note for the history phase in appendix A.
3. **Marsh is thinner.** With fen taking broad wet ground, marsh is the narrow margins: median 0.7 / 0.8 / 2.7% of land.
4. **Heath is a band on the lee face**, and moor on the windward face: both follow the 4 °C isotherm, which follows height (P1-03 limitation 8). An elevation jitter does not break the band (11.2); aspect or a coarse jitter of several kilometres would, and is not proposed.
5. **Steppe is a uniform strip at the foot of lee ranges** where the shadow is deepest (2048 s5, s6; 1024 s8).
6. **Wet woodland reads as thick dark rivers at sheet scale** on 2048 maps (strips a few cells wide along every class ≥ 4 river through open country); at walking scale it is a corridor of carr.
7. **The lee-coast forest rim thins only modestly** (forest share of open-or-forest land within 1 km of a lee coast: 1024 0.40 → 0.33, 2048 0.55 → 0.51); at 2048 the wet rim is mostly the moisture path, not the coast bonus.
8. **Grassland falls to about a quarter of land** (median 0.25 / 0.23 / 0.19), steppe taking the dry interiors: fertile blocks (mean fertility ≥ 0.7 over 2 km² tiles) at 2048 fall from 48% to 33% (critic). P1-05's calibration must account for it (P1-05 11).
9. **Warm worlds keep only a summit cap of alpine** (512 s0, offset +2.0 °C: 0.3%); cold worlds have large snowfields (1024 s8, −1.9 °C: 16%). Intended variety (P1-03 11.2).
10. **Speckle about doubles** (median 0.15% → 0.29% of land in patches under 16 cells, at most 0.43%) and `biome_patches` rises 1.7 to 1.9 times, from the nine biomes' extra edges; still under 0.5% everywhere.
11. **Calibration sample.** 58 worlds, not the 120 of the 0.8.0 sweep (section 9); the validation sweep resets the ranges.
12. Carried from the second version: marsh and fen depend on the river threshold and on `drain_river_class_min` (changing `river_min_area_m2` shifts every class); no transitions or mixtures (one biome per 16 m cell; ecotones are the runtime's); weak-shadow worlds are forest-heavy.

## 12. Open questions for the lead

1. **Fixed two-pass window** (4.2): the exploration had `wet_window_passes` int(1, 2) = 2; this spec makes the triangle fixed, as P1-03 C4, one field fewer. Recommendation: approve (the box variant drew staircases; nobody needs it as data).
2. **`wet_window_m` bound 2000** (the exploration: 4000), so that B2c fits `int64` without a 128-bit comparison (section 7). Recommendation: approve.
3. **New biome values** (fertility, timber, nouns; 4.1). Recommendation: approve as the exploration set them; revisit steppe fertility in P1-05 (question 4).
4. **Steppe fertility 0.6.** At 2048 fertile blocks fall from 48% to 33%. Recommendation: keep 0.6 now (a dry interior should read poorer); P1-05's calibration decides whether to raise it to 0.7–0.75 (the critic's range), a data change.
5. **Fen at 512** (limitation 2). Recommendation: accept; the history phase gets the fallback of appendix A rather than a per-size fen rule, since 512 terrain has little broad wet ground.
6. **Rule-field checks at the seams** (4.1: `biomes: rule: <field> outside its schema range`): written into the spec from the 0.8.0 code, extended to the two new fields. Recommendation: approve.
7. **Second-version open questions** 1 to 10: resolved by the 0.8.0 merge and by this revision (1: categories, now nine; 8: marsh grows with map size, accepted; 10: the coast rim, `coast_rain_mm` 40 adopted, limitation 7).

## Appendix A. Note for the history phase: fen homelands

Fen is missing (under 0.1% of land) in about a quarter of 512 worlds (5 of 20 in the calibration) and is the plurality biome of some 2 km² area in only 1 of 20 there (1024: 10 of 30; 2048: 8 of 8). A race, culture or monster with a fen homeland therefore needs a **fallback on small maps**: the largest fen-and-marsh complex if there is one, else a lake shore or a river mouth; or history treats "fen" as "wetland" (fen ∪ marsh ∪ wet woodland) when it places a homeland, with fen preferred where present (critic: fall back to marsh, then wet woodland). The history specs should state which. Marsh is present in every calibration world; wet woodland too.

## Appendix B. Schema and catalogue impact (for the lead's proposals entry)

- **Schema version 3 → 4** in the variety merge (generator 0.9.0): table `climate` gains `temp_offset_dc INTEGER NOT NULL` (DDL, reader and writer rules in P1-03 3; P0-02 amendment 16). `SCHEMA_VERSION`, `PRAGMA user_version` and `meta.schema_version` become 4; readers refuse schema 3 worlds. Table `biome` unchanged (twelve rows with the shipped data). The empty world stays 8 pages (32768 bytes; checked on the exploration build).
- `region` and `region_border` move from planned version 4 to **planned version 5** (P1-06, P1-00 9.2).
- No layer catalogue change; no new `meta` or `param.*` key.
- Data: four new biome definitions (codes 9 `heath`, 10 `steppe`, 11 `wet_woodland`, 12 `fen`); `highland_moor` gains a rain bound and a colour; `marsh` a colour and the noun "Marsh"; `biome_rule` gains `river_dist_max_m` and `wet_frac_min`; `worldgen.biomes` gains six fields; `worldgen.climate` gains `temp_offset_max_c` and ships `coast_rain_mm` 40 (P1-03 4).
- Streams: one new, `geo.climate.temp_offset` (P1-03). None for biomes (the exploration's `geo.biomes.wet_jitter` and `geo.biomes.elev_jitter` are not registered: `all_stream_names` has 27 entries, the 0.8.0 26 plus one).
- `report`: `temp_offset_c` (P1-03 9); `biome_share` lists ten keys.
