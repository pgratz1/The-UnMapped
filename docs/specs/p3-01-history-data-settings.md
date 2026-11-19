# P3-01 History data and settings

Status: **draft, spec writer, 2026-11-19, awaiting approval.** Phase P3 History core, spec 1 (P3-00 3.1). Design doc sections 3 (data in TOML), 5 (the six races, their order, camps and timing; "Food and homelands"; world-creation settings), 6 (`meta` holds the settings). Depends on P0-03 (with amendments 15 to 17), P0-04, P1-00 6 and P3-00. Written together with P3-02, which defines the culture naming record that this spec's `culture` kind carries.

## 1. Purpose

Register the data the history phase reads, and the world-creation settings it obeys:

- three new data kinds, `race`, `culture` and the fixed-id tuning kind `history`, with the shipped files for six races and six cultures;
- eight world-creation settings, as `generate` flags and settings-file keys, parsed and validated.

Nothing here simulates. The merge changes every world's `content_hash` and nothing else (section 7).

## 2. What this spec touches (for the engineer)

| Area | Files |
| --- | --- |
| Kind registry | `worldgen/src/data/kinds.cpp` (registration of `race`, `culture`, `history`; the culture `naming` record type with the fields of P3-02 3), a new `worldgen/src/data/kinds_history.cpp` for the race checks if the engineer prefers; `worldgen/include/worldgen/data/content.hpp` only if a new accessor is needed |
| Settings | `worldgen/include/worldgen/data/settings.hpp`, `worldgen/src/data/settings.cpp` |
| Command line | `tools/worldgen-cli/cli.cpp` (flags, usage text, help) |
| Data | `game/data/races/*.toml`, `game/data/cultures/*.toml`, `game/data/history/chronology.toml`; identical copies under `worldgen/tests/data/geo/` and `worldgen/tests/data/geo-small/` |
| Version | `worldgen/include/worldgen/version.hpp`: **0.12.0** |
| Tests | new files under `worldgen/tests/data/` (loader) and `tools/worldgen-cli/tests/` (flags); golden `worldgen/tests/golden/p3-01-data.txt` |

P3-02 touches none of these files except that its seam reads the `naming` record this spec registers (P3-02 9). **Merge order: P3-01 first, then P3-02.** The two can be implemented in parallel: P3-02's engineer builds against the record layout of P3-02 3 and rebases on P3-01.

## 3. Kind `race`

Directory `races/`, **required**, one schema for all definitions. Fields, all required, in declaration order (the order fixes the canonical encoding, P0-03 8):

| Field | Type | Meaning |
| --- | --- | --- |
| `order` | int(1, 6) | awakening order (design 5: elves 1 … orcs 6) |
| `name` | string(32, word) | singular display word, e.g. `Elf` |
| `plural` | string(32, word) | `Elves` |
| `adjective` | string(32, word) | `Elven` |
| `camp` | enum(good, neutral, evil) | the camp of the creating gods (design 5) |
| `awakening_share` | fx(0, 1) | the awakening year as a share of the span, before the seed's jitter (P3-03) |
| `lifespan_years` | int(1, 10000) | typical lifespan |
| `generation_years` | int(1, 1000) | length of a generation; ruling lines turn over once per generation in quiet steps (P3-00 4.4) |
| `growth_permille` | int(0, 1000) | yearly growth of a population toward capacity, per mille (P3-05) |
| `site_weights` | record `race_site_weights` | weights of the region means when the race scores a region (P3-00 10.1) |
| `biomes` | array(record `race_biome`, 0, 16) | biome preferences; a biome not listed has weight 0 |
| `upland` | int(−1000, 1000) | preference for upland regions (4 × `walk_cells` < `cells`) |
| `coast` | int(−1000, 1000) | preference for coastal regions (`coast_cells` > 0) |
| `expansion` | int(0, 1000) | drive to claim regions (P3-06) |
| `aggression` | int(0, 1000) | readiness for war (P3-06) |
| `settlement_kinds` | array(enum(hamlet, village, town, city, fortress), 1, 5) | settlement kinds the race builds |
| `culture` | ref(culture) | the race's default culture |

Record `race_site_weights`, fields int(0, 1000), in this order: `fertility`, `forage`, `timber`, `ore`, `fresh_water` (each multiplies the region's mean of that score, P1-06 3.2). Record `race_biome`: `biome` ref(biome), `weight` int(−1000, 1000).

Reserved for P5 and not declared now: temperament, magic school leanings, stances toward other races. Adding a field later is a data change (content hash, version bump), not a schema change.

**Named checks**, per definition (P0-03 7.5; reported at the definition's header as `<path>:<line>:<column>: error: check '<name>' failed for 'race.<id>': <message>`), in this order:

| Check | Condition | Message |
| --- | --- | --- |
| `race_generation_below_lifespan` | `generation_years` < `lifespan_years` | `generation_years must be below lifespan_years` |
| `race_site_weights_positive` | the five weights sum to at least 1 | `the site_weights must not all be 0` |
| `race_biomes_unique` | no `biome` twice in `biomes` | `biomes[<i>]: biome '<id>' is already listed at biomes[<j>]` (the later index i, the first index j) |
| `race_settlement_kinds_unique` | no kind twice | `settlement_kinds[<i>]: '<kind>' is already listed` |

**Kind-level checks** (P0-03 amendment 16), in this order:

| Check | Rule | Failure (position, message) |
| --- | --- | --- |
| `race_order_unique` | no two definitions share `order` | at each later one in canonical id order: `order <k> is also used by 'race.<id>'` |
| `race_order_dense` | the orders are exactly 1 … n for n definitions | against the directory: `orders must be 1 to <n> without gaps; <k> is missing` (the smallest missing k); not run if `race_order_unique` failed |
| `race_awakening_order` | the race of order 1 has `awakening_share` 0; shares strictly increase with order | at the definition: `the race of order 1 must have awakening_share 0`, or `awakening_share must be above that of 'race.<id>' (order <k−1>)` |

**Cross-kind conditions**, which the loader cannot check (a named check sees one kind): checked at the entry of the `chronology` stage (P3-03), each fatal with the prefix `chronology: data: `, in race order:

1. every `biomes[i].biome` is a land biome (`water` false): `race '<id>' prefers water biome '<biome id>'`;
2. the race's `culture` belongs to it (`culture.race` = the race): `race '<id>' has culture '<culture id>' of race '<other id>'`;
3. every race has at least one culture.

With the shipped data none can fail; the P3-03 tests make each fail once.

### 3.1 Shipped races

Values from design 5 (order, camps, awakening shares, the race characters and "Food and homelands"; the P1 critic's notes on homelands: dwarves on reachable ore in uplands, faeries in wetlands and wild places, farming peoples on the lowland habitability ranking). Weights are first values, reasoned below and to be tuned by P3-05 against the 1024 sweep. A weight of 1000 is the strongest preference; site weights are relative (the sum is the denominator).

```toml
# game/data/races/elf.toml
[race.elf]
order = 1
name = "Elf"
plural = "Elves"
adjective = "Elven"
camp = "good"
awakening_share = 0
lifespan_years = 700
generation_years = 150
growth_permille = 3
site_weights = { fertility = 20, forage = 35, timber = 35, ore = 0, fresh_water = 10 }
biomes = [
  { biome = "temperate_forest", weight = 500 },
  { biome = "wet_woodland", weight = 300 },
  { biome = "grassland", weight = 0 },
  { biome = "heath", weight = -200 },
  { biome = "steppe", weight = -300 },
  { biome = "highland_moor", weight = -300 },
  { biome = "alpine", weight = -600 },
]
upland = -300
coast = 0
expansion = 200
aggression = 150
settlement_kinds = ["hamlet", "village", "town", "city"]
culture = "elven"
```

```toml
# game/data/races/goblin.toml
[race.goblin]
order = 2
name = "Goblin"
plural = "Goblins"
adjective = "Goblin"
camp = "evil"
awakening_share = "0.35"
lifespan_years = 40
generation_years = 15
growth_permille = 20
site_weights = { fertility = 40, forage = 20, timber = 10, ore = 30, fresh_water = 0 }
biomes = [
  { biome = "heath", weight = 200 },
  { biome = "highland_moor", weight = 200 },
  { biome = "steppe", weight = 150 },
  { biome = "grassland", weight = 100 },
  { biome = "marsh", weight = 100 },
  { biome = "alpine", weight = -400 },
]
upland = 100
coast = -100
expansion = 600
aggression = 700
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "goblin"
```

```toml
# game/data/races/dwarf.toml
[race.dwarf]
order = 3
name = "Dwarf"
plural = "Dwarves"
adjective = "Dwarven"
camp = "neutral"
awakening_share = "0.55"
lifespan_years = 300
generation_years = 70
growth_permille = 5
site_weights = { fertility = 10, forage = 10, timber = 10, ore = 60, fresh_water = 10 }
biomes = [
  { biome = "alpine", weight = 300 },
  { biome = "highland_moor", weight = 300 },
  { biome = "heath", weight = 200 },
  { biome = "temperate_forest", weight = -100 },
  { biome = "wet_woodland", weight = -300 },
  { biome = "marsh", weight = -400 },
  { biome = "fen", weight = -400 },
]
upland = 800
coast = -200
expansion = 250
aggression = 300
settlement_kinds = ["hamlet", "village", "town", "city", "fortress"]
culture = "dwarven"
```

```toml
# game/data/races/faerie.toml
[race.faerie]
order = 4
name = "Faerie"
plural = "Faeries"
adjective = "Faerie"
camp = "good"
awakening_share = "0.70"
lifespan_years = 400
generation_years = 100
growth_permille = 4
site_weights = { fertility = 0, forage = 60, timber = 20, ore = 0, fresh_water = 20 }
biomes = [
  { biome = "fen", weight = 600 },
  { biome = "wet_woodland", weight = 600 },
  { biome = "marsh", weight = 400 },
  { biome = "temperate_forest", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "grassland", weight = -200 },
  { biome = "alpine", weight = -300 },
  { biome = "steppe", weight = -400 },
]
upland = 0
coast = 0
expansion = 100
aggression = 50
settlement_kinds = ["hamlet", "village"]
culture = "faerie"
```

```toml
# game/data/races/human.toml
[race.human]
order = 5
name = "Human"
plural = "Humans"
adjective = "Human"
camp = "neutral"
awakening_share = "0.83"
lifespan_years = 70
generation_years = 25
growth_permille = 15
site_weights = { fertility = 60, forage = 10, timber = 10, ore = 5, fresh_water = 15 }
biomes = [
  { biome = "grassland", weight = 300 },
  { biome = "steppe", weight = 100 },
  { biome = "temperate_forest", weight = 100 },
  { biome = "marsh", weight = -100 },
  { biome = "heath", weight = -100 },
  { biome = "fen", weight = -200 },
  { biome = "highland_moor", weight = -200 },
  { biome = "alpine", weight = -500 },
]
upland = -300
coast = 200
expansion = 700
aggression = 400
settlement_kinds = ["hamlet", "village", "town", "city", "fortress"]
culture = "human"
```

```toml
# game/data/races/orc.toml
[race.orc]
order = 6
name = "Orc"
plural = "Orcs"
adjective = "Orcish"
camp = "evil"
awakening_share = "0.93"
lifespan_years = 50
generation_years = 18
growth_permille = 18
site_weights = { fertility = 35, forage = 25, timber = 10, ore = 30, fresh_water = 0 }
biomes = [
  { biome = "steppe", weight = 400 },
  { biome = "heath", weight = 200 },
  { biome = "grassland", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "temperate_forest", weight = -100 },
  { biome = "marsh", weight = -100 },
  { biome = "fen", weight = -200 },
  { biome = "wet_woodland", weight = -200 },
  { biome = "alpine", weight = -300 },
]
upland = 100
coast = -100
expansion = 600
aggression = 900
settlement_kinds = ["hamlet", "village", "town", "fortress"]
culture = "orcish"
```

Reasons, in short: **elves** (good, very long-lived, few) live in forest and wet woodland, grow slowest and expand least; **goblins** (evil, short-lived, numerous) take dry uplands and poorer lowland, grow fast, are aggressive, and through the camp rule are hostile to elves from their first year; **dwarves** (neutral, long-lived) want reachable ore in uplands (`upland` 800, ore 60; the P1 critic: report-upland regions with `ore_reach_cells` > 0, median 3 per 1024 world), and avoid wetland; **faeries** (good, small, hidden) take forage-rich wetland and wet woodland (regions with a quarter or more wetland: median 1 per 1024 world, so they must also accept forest), and build only hamlets and villages; **humans** (neutral, adaptable, fast-growing) farm (fertility 60, grassland), like coasts and expand fastest; **orcs** (evil, warlike) take steppe and heath, most aggressive of all. Lifespans and generations are of the design's characters (elves "very long-lived", goblins "short-lived"); they matter only for ruling-line turnover in P3.

## 4. Kind `culture`

Directory `cultures/`, **required**. Fields, all required, in this order:

| Field | Type | Meaning |
| --- | --- | --- |
| `race` | ref(race) | the race whose culture this is |
| `name` | string(32, word) | display word, e.g. `Elven` |
| `naming` | record `culture_naming` | the naming data: **fields, checks and meaning in P3-02 3**; this spec registers the record type with exactly those fields |

No named check of its own; the `culture_naming` checks of P3-02 3 are per-definition checks of this kind, reported as `… failed for 'culture.<id>': <message>`.

Shipped cultures, one per race (P3-00 question 9), ids `dwarven`, `elven`, `faerie`, `goblin`, `human`, `orcish`, files `game/data/cultures/<id>.toml`. Each file is:

```toml
[culture.elven]
race = "elf"
name = "Elven"

[culture.elven.naming]
# the elven block of P3-02 section 5, verbatim
```

with (`race`, `name`) = (`dwarf`, `Dwarven`), (`elf`, `Elven`), (`faerie`, `Faerie`), (`goblin`, `Goblin`), (`human`, `Human`), (`orc`, `Orcish`) and the `naming` table of P3-02 5 for that culture. The naming values are defined only in P3-02 (one place); this spec defines the file.

## 5. Kind `history`

Directory `history/`, **required**, a **fixed-id kind** (P0-03 amendment 15), one schema per id, like `worldgen`. Ids are added by the spec that uses them:

| Id | Spec | Status |
| --- | --- | --- |
| `chronology` | P3-03 | **complete here** |
| `engine` | P3-04 | added by P3-04 |
| `peoples` | P3-05 | added by P3-05 |
| `conflict` | P3-06 | added by P3-06 |
| `report` | P3-07 (with the event templates written from P3-04 on) | added by P3-04 or P3-07, whichever lands first |

**A not-yet-registered id** is handled exactly as for `worldgen`: the id set of the kind is what the build registers; a definition with any other id is an error at its header, `unknown id 'engine' for kind 'history'; expected one of chronology` (P0-03 amendment 15's message), and every registered id must be defined (`missing definition 'history.<id>'`, against the directory). So each data root carries exactly the files of the ids registered by its build: a spec that adds an id adds its file to `game/data/history/` and to the two fixture roots in the same merge. A file for a future id must not be committed early.

### 5.1 `history.chronology`

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `awakening_jitter_frac` | fx(0, "0.05") | "0.01" | taste | the seed moves each awakening after the first by up to this share of the span either way (60 years at 6,000; design 5 "the seed shifts each date by a small amount") |
| `awakening_min_gap_years` | int(1, 2000) | 100 | taste | least distance between two consecutive awakenings after the jitter; the order never changes (P3-03) |
| `age_name_form` | string(64) | `"The Age of {plural}"` | taste | name of each age; `{plural}` and `{adjective}` are replaced by the race's words |

Named check (per definition): `chronology_age_name_form`: the form contains `{plural}` or `{adjective}` at least once and no other `{`: message `age_name_form must contain {plural} or {adjective} and no other placeholder`.

```toml
# game/data/history/chronology.toml and the fixtures under worldgen/tests/data/geo*/history/
[history.chronology]
awakening_jitter_frac = "0.01"
awakening_min_gap_years = 100
age_name_form = "The Age of {plural}"
```

## 6. World-creation settings

### 6.1 Settings, flags and keys

Settings-file keys are in `[world]`, as P0-03 6 requires (correcting P3-00 5, which wrote `history.span`); flags override the file key by key.

| Setting | Flag | Settings key | Type in the file | Default | Valid values |
| --- | --- | --- | --- | --- | --- |
| History span | `--history-span N` | `world.history_span` | integer | 6000 | 2000 … 12000 |
| History detail | `--history-detail D` | `world.history_detail` | string | `normal` | `sparse`, `normal`, `dense` |
| Civilizations per race | `--civs-per-race C` | `world.civs_per_race` | integer, or the string `"auto"` | `auto` | `auto`, 1 … 4 |
| Races present | `--races LIST` | `world.races` | array of race ids, or the string `"all"` | `all` | `all`, or a non-empty set of ids of the loaded `race` data, each once |
| Event intensity | `--event-intensity I` | `world.event_intensity` | string | `normal` | `calm`, `normal`, `turbulent` |
| Monster pressure | `--monster-pressure M` | `world.monster_pressure` | string | `normal` | `low`, `normal`, `high` |
| Divine activity | `--divine-activity A` | `world.divine_activity` | string | `normal` | `distant`, `normal`, `meddling` |
| Divine script | `--divine-script FILE` | `world.divine_script` | string (a path) | none | **refused until P5** |

On the command line `--races` takes a comma-separated list (`--races elf,orc`, no spaces) or `all`; `--civs-per-race` takes `auto` or a decimal integer. The order of ids in a list does not matter; the stored value is in race order (6.3).

### 6.2 Errors

Command line: usage errors, exit 2, with the hint line (P0-04 4.1), message forms as `--map-size`'s:

| Case | Message |
| --- | --- |
| span not an integer, or outside the range | `invalid --history-span '<text>'; expected an integer from 2000 to 12000` |
| detail | `invalid --history-detail '<text>'; expected sparse, normal or dense` |
| civilizations | `invalid --civs-per-race '<text>'; expected auto or an integer from 1 to 4` |
| races: empty list, empty item, an item that is not an id, or a repeated id | `invalid --races '<text>'; expected all or a comma-separated list of race ids` |
| races: an id with no `race` definition (checked after the data is loaded) | `unknown race '<id>' in --races; expected all or ids from: <ids of the loaded races in race order, comma separated>` |
| intensity, pressure, activity | `invalid --event-intensity '<text>'; expected calm, normal or turbulent` (and the same form for the other two with their values) |
| divine script | `--divine-script is not supported yet: divine scripts need phase P5` |

Settings file: input errors (exit 3) at the value's position, in the loader's form (P0-03 9): `'world.history_span' is <found>; expected an integer from 2000 to 12000`, and the same pattern for each key (`expected sparse, normal or dense`, `expected "auto" or an integer from 1 to 4`, `expected "all" or an array of race ids`, …); an array with a repeated id: `'world.races[<i>]' is "<id>", which is already listed`; an id with no definition: `'world.races[<i>]' is "<id>", which is not a race id; expected one of <ids>` (checked after the data is loaded, still reported at the value's position); `'world.divine_script' is not supported yet: divine scripts need phase P5`. The unknown-key message of `[world]` lists the new keys: `expected seed, seed_phrase, map_size, history_span, history_detail, civs_per_race, races, event_intensity, monster_pressure, divine_activity or divine_script`.

`--test-pattern` with any history flag: the flags are accepted and have no effect (the test pattern writes no history); interim, since the settings are world-creation settings like `--map-size`, which `--test-pattern` also accepts.

### 6.3 The `meta` keys

`Settings` carries the eight values from P3-01 on. **They are written into `meta` by the `chronology` stage's merge, P3-03, with schema 6** (P3-00 5 and 6.5, corrected): a new required `meta` key is a schema change, and P3-03 makes the one bump to schema 6. P3-01 changes no table and no `meta` key.

| Key | Value text |
| --- | --- |
| `param.history_span` | decimal, e.g. `6000` |
| `param.history_detail` | `sparse`, `normal` or `dense` |
| `param.civs_per_race` | `auto`, or `1` … `4` |
| `param.races` | the present race ids in race order, comma separated, e.g. `elf,goblin,dwarf,faerie,human,orc` (also when `all` was given) |
| `param.event_intensity` | `calm`, `normal` or `turbulent` |
| `param.monster_pressure` | `low`, `normal` or `high` |
| `param.divine_activity` | `distant`, `normal` or `meddling` |
| `param.divine_script` | empty (a SHA-256 in lower-case hex from P5 on) |

## 7. What the merge changes in a world

- `content_hash` of every world: the three new kinds enter the canonical encoding (P0-03 8: kinds in ascending name, so `biome`, `culture`, `history`, `race`, `worldgen`). So every world file changes, the P0 empty worlds included.
- Nothing else: no table, no `meta` key, no layer. The golden vectors of P1-01 to P1-06 (layer and table hashes) must not change; a difference is a defect.
- `GENERATOR_VERSION` **0.12.0**; `p0-worlds.txt` and `p1-worlds.txt` re-recorded by the validation specialist after the cross-preset comparison.

## 8. Tests required

For the independent test writer.

### 8.1 Loading

1. The shipped data loads; every value of the six races (3.1) and the chronology tuning (5.1) equals the listing (a table transcribed in the test, not read from the files); the six cultures have the `race` and `name` of section 4.
2. Each per-definition and kind-level check of section 3 fails once on a fixture with its exact message and position; `race_order_dense` is not reported when `race_order_unique` fails.
3. Word grammar on `name`, `plural`, `adjective`: `elf`, `High Elf` fail (P0-03 amendment 17's message).
4. Fixed-id behaviour: a fixture data root with `history/engine.toml` fails with `unknown id 'engine' for kind 'history'; expected one of chronology`; a root without `history/chronology.toml` fails with `missing definition 'history.chronology'`; a root without `races/` fails as a missing required kind directory.
5. Ranges: each bounded field one step outside its bounds is a range error.

### 8.2 Settings

6. Every flag: each valid value accepted, each invalid form of 6.2 rejected with its message and exit 2; `--divine-script` always rejected.
7. Every settings key: valid and invalid values with message and position (exit 3); flag overrides the file key; an unknown `[world]` key lists the new keys.
8. `--races` normalization: `orc,elf` and `elf,orc` give the same `Settings` value; `all` gives every loaded race in race order.

### 8.3 Content hash and worlds

9. **Golden** `worldgen/tests/golden/p3-01-data.txt`: the `content_hash` of `game/data`, `worldgen/tests/data/geo` and `worldgen/tests/data/geo-small`, one line `<root> <hex>` each, computed by the test writer's **independent reference** of the canonical encoding (P0-03 8) in `worldgen/tests/reference/p3_data_reference.py` from the TOML files, not from the C++ loader. The C++ `content_hash` of each root must equal it.
10. A world generated with `--until none` has `meta.content_hash` equal to the golden value for its data root and `generator_version` 0.12.0, and its other bytes equal a 0.11.0 world's apart from those two `meta` values (compare table by table).
11. The P1 stage golden vectors are unchanged.

## 9. Open questions

1. **Shipped weights** are first values (3.1). Recommendation: approve as a starting point; P3-05 tunes them on the 1024 sweep.
2. **`--test-pattern` with history flags** accepted and ignored (6.2). Recommendation: approve.
3. **Cross-kind data conditions** checked at the stage, not by the loader (3). Recommendation: approve; a loader-level cross-kind check is a P0-03 feature no kind needed before.
