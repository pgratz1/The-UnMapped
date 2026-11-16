# P1-00 Geography overview

Status: approved by the lead, 2026-11-15 (tuning values marked "guess" and the statistics ranges are starting points, to be revised against the first seed sweep); originally a draft, 2026-11-15. Updated by the spec writer for the elevation redesign, 2026-11-15 (draft, awaiting approval): stream list (3), appendix C items 11–16 and 19 folded into 6, 7 and 8, five `[geography]` statistics added (10.3), 13.1 marked for revision, 13.2 known defects. Updated again for elevation candidate v16, 2026-11-16 (draft, awaiting approval): elevation streams (3), 13.1 ranges from the v16 sweeps, 13.2 known limitations and checklist item 10. Phase P1 Geography. Design doc sections 2, 4, 6, 7, 10, 11 (milestone 0), 13. Depends on P0-01 to P0-04 including their amendments. First pass: this overview, P1-01 elevation, P1-02 hydrology. Second pass (written after elevation and hydrology have been implemented and looked at): climate, biomes, resources and habitability, regions.

This document fixes everything the six geography stage specs share: scale and coordinates, the stage list, the layer catalogue, the representation rule, tuning as data, shared algorithms, determinism rules, schema, command-line changes, the performance budget, math functions and how results are judged. A stage spec cites the sections here instead of repeating them. Where this document and a stage spec disagree, the stage spec is wrong and must be fixed.

Requested amendments to the P0 specs are collected in appendix A. Proposals for the design doc are in appendix B.

## 1. Purpose

Phase P1 turns a seed into a stack of geography layers in `world.db`: an eroded elevation, water (ocean, lakes, rivers), climate, biomes, resource and habitability scores, and regions. History (P3) places peoples on these layers; the game (milestone 2 on) builds terrain from them. The exit check is milestone 0: `worldgen-cli generate` writes a world with every geography layer; images and the Godot viewer show plausible maps; the same seed gives an identical file hash on every platform and compiler; each stage has golden-seed tests.

## 2. Scale and coordinates

### 2.1 Cells and metres

| Quantity | Value |
| --- | --- |
| Cell size | 16 m × 16 m at every map size. A constant of the code (`CELL_M = 16`), not a tuning parameter. |
| Map side N | 512, 1024 (default) or 2048 cells, from `param.map_size` |
| Map side in metres | 16N: 8192, 16384, 32768 m (the design doc's "8, 16 or 32 km") |
| Cell (x, y) | x column, 0 at the west edge; y row, 0 at the north edge; index i = yN + x (P0-02 5.6) |
| Cell centre in metres | (16x + 8, 16y + 8) from the north-west corner of the map |
| Cell centre as map fraction | u = (2x + 1) / (2N), v = (2y + 1) / (2N), in (0, 1). As `Fx`: `from_ratio(2x + 1, 2N)`, exact for N a power of two |
| North | y = 0. Latitude-like quantities (climate) are functions of v, with v = 0 at the north edge |

Stage algorithms are defined for any N with 16 ≤ N ≤ 2048, so unit and property tests can run on small maps through the library. The command line accepts only 512, 1024 and 2048.

### 2.2 The map-size convention

**A larger map shows more land at the same scale, and the continent still fills it.** A 2048 world is a continent four times the area of a 1024 world, with hills, valleys, rivers and lakes of the same physical size. Two kinds of quantity follow, and every parameter of every geography stage belongs to exactly one:

| Kind | Specified in | Used for | Consequence at 2048 compared with 1024 |
| --- | --- | --- | --- |
| **Continental layout** | fractions of the map side (field names end in `_frac`, or noise given as `cycles` per map side) | continent outline and its large bays and peninsulas; the edge margin; where mountain belts run; the latitude span of the climate (second pass); the land fraction | The same outline and the same number of belts, stretched to fill the map |
| **Physical relief and process** | metres or square metres (field names end in `_m`, `_m2`, or noise given as `wavelength_m`) | hills, ridges and their spacing; domain warp of relief; erosion; diffusion; river and lake thresholds; island and lake minimum sizes; coast distances; lapse rate and rain-shadow distances (second pass); region size (second pass) | Four times as many ridges, valleys, rivers, lakes and regions; each the same size as at 1024 |

So a mountain belt at 2048 is twice as wide in metres as at 1024 and holds about four times as many 4 km ridges: a larger massif, not a larger ridge. The continental noise is evaluated at map-fraction coordinates, so one seed gives the same continent outline at every map size, with different local detail (the metre-scale noise covers a different extent).

The climate latitude span is a continental-layout quantity (the map stands for a larger land, design doc section 4, "scale is compressed"), so every map size has the full climate range. This is an interim reading for the climate spec; proposals entry B2.

### 2.3 Neighbourhoods

D8 direction codes, used by every stage and stored in `flow_dir`:

| Code | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Name | E | SE | S | SW | W | NW | N | NE |
| (dx, dy) | (1, 0) | (1, 1) | (0, 1) | (-1, 1) | (-1, 0) | (-1, -1) | (0, -1) | (1, -1) |

- The opposite of code k is (k + 4) mod 8. Odd codes are diagonal (distance 16√2 m), even codes orthogonal (16 m).
- **8-neighbour order** is codes 0, 1, …, 7. **4-neighbour order** is codes 0, 2, 4, 6 (E, S, W, N). Every loop over neighbours in every stage uses one of these two orders.
- A neighbour outside the map does not exist; it is skipped, never wrapped or clamped, unless a spec says otherwise for a filter boundary.
- Land components are 8-connected; ocean is 4-connected (the standard pair, so a diagonal pair of land cells separates two ocean cells and no region crosses another).

## 3. Stages

Stages run in this order. Each is recorded in `meta.stages` (P0-02 5.5) under its stage name. Every stage reads only what is listed and writes only its own layers (section 5).

| # | Stage name | Spec | Reads | Writes (in this order) | Streams |
| --- | --- | --- | --- | --- | --- |
| 1 | `elevation` | P1-01 | seed, N, `worldgen.elevation` | `elevation` | fifteen: `geo.elevation.continent_warp_x`, `geo.elevation.continent_warp_y`, `geo.elevation.continent`, `geo.elevation.belt`, `geo.elevation.coast`, `geo.elevation.segment`, `geo.elevation.relief_warp_x`, `geo.elevation.relief_warp_y`, `geo.elevation.ridge`, `geo.elevation.hills`, `geo.elevation.roughness`, `geo.elevation.basin`, `geo.elevation.detail`, `geo.elevation.erosion` (random erosion receivers, a `hash_at` key), `geo.elevation.fill` (fill cost noise); P1-01 section 2 |
| 2 | `hydrology` | P1-02 | `elevation`, `worldgen.hydrology` | `water`, `water_depth`, `flow_dir`, `flow_accum`, `river`, `lake`, `coast_dist` | none (hydrology is deterministic analysis) |
| 3 | `climate` | P1-03 (second pass) | `elevation`, `water`, `coast_dist`, `worldgen.climate` | `temperature`, `rainfall` (at least) | `geo.climate.*` |
| 4 | `biomes` | P1-04 (second pass) | `elevation`, `water`, `river`, `coast_dist`, `temperature`, `rainfall`, `biome` definitions, `worldgen.biomes` | `biome`; table `biome` | `geo.biomes.*` |
| 5 | `resources` | P1-05 (second pass) | the layers above, `worldgen.resources` | `fertility`, `ore`, `timber`, `fresh_water`, `habitability` | `geo.resources.*` |
| 6 | `regions` | P1-06 (second pass) | the layers above, `worldgen.regions` | `region`; table `region` | `geo.regions.*` |

Stream names are registered in `worldgen/include/worldgen/core/streams.hpp` (P0-01 7.3). The prefixes `geo.climate.`, `geo.biomes.`, `geo.resources.` and `geo.regions.` are reserved for the second-pass stages. A stage uses its streams as noise keys (`stream_key(seed, name)` passed to `noise2`, `fbm2` or `hash_at`) or as `Rng` keys only in sequential code (rule D9).

## 4. Layer catalogue

Every geography layer. "Fixed" means name, element type, units and meaning are decided now and second-pass specs must use them; "open" items are decided by the named spec. All layers are N × N, codec 1, written by exactly one stage.

| Layer | Type | Units and scale | Value range | Special values | Written by | Status |
| --- | --- | --- | --- | --- | --- | --- |
| `elevation` | `i16` | decimetres above sea level (0.1 m); sea level is the constant 0 | [-30000, 30000] | never 0; < 0 is ocean, > 0 is land (including lake beds) | `elevation` | fixed |
| `water` | `u8` | category | 0 to 3 | 0 dry land, 1 river, 2 lake, 3 ocean | `hydrology` | fixed |
| `water_depth` | `u16` | decimetres | [0, 30000] | 0 on land and river cells; ocean: −`elevation`; lake: lake level − `elevation` (> 0). Water surface = `elevation` + `water_depth` wherever there is standing water | `hydrology` | fixed |
| `flow_dir` | `u8` | D8 code (2.3) of the downstream neighbour | 0 to 8 | 8 = no downstream (ocean cells only) | `hydrology` | fixed |
| `flow_accum` | `u32` | cells draining through this cell, itself included (× 256 m² gives upstream area) | [0, N²] | 0 on ocean; ≥ 1 elsewhere | `hydrology` | fixed |
| `river` | `u8` | river size class: class k ≥ 1 means `flow_accum` in [T·2^(k−1), T·2^k), T the river threshold in cells; class 15 is open-ended | 0 to 15 | 0 not a river (also every lake and ocean cell) | `hydrology` | fixed |
| `lake` | `u32` | lake id, 1, 2, … in order of each lake's first cell in index order | [0, number of lakes] | 0 not a lake | `hydrology` | fixed |
| `coast_dist` | `i16` | metres, chamfer distance to the sea coast (P1-02) | [-32767, 32767] | > 0 on non-ocean cells (distance to the nearest ocean cell), < 0 on ocean cells (minus the distance to the nearest non-ocean cell); saturates at ±32767 | `hydrology` | fixed |
| `temperature` | `i16` | annual mean, tenths of a degree Celsius | [-600, 500] | none | `climate` | fixed; further climate layers (seasonality, wind) open |
| `rainfall` | `u16` | annual precipitation, millimetres | [0, 10000] | none | `climate` | fixed |
| `biome` | `u8` | biome code from the `biome` definitions (6.6) | 1 to 254 | 0 never present in a finished layer | `biomes` | fixed |
| `fertility`, `ore`, `timber`, `fresh_water` | `u8` | score, 0 = none to 255 = best on this map's scale | [0, 255] | 0 on ocean | `resources` | names and type fixed; definitions open |
| `habitability` | `u8` | score as above, combining the four | [0, 255] | 0 on ocean | `resources` | fixed; formula open |
| `region` | `u16` | region id, a row of table `region` | [0, 65535] | 0 no region (ocean, if sea areas get no regions) | `regions` | type fixed; whether sea and lakes get regions open |

Rules:

1. **Write once.** Each layer is written by exactly one stage, once. No stage updates or deletes another stage's layer. Within a stage, layers are written in the order of section 3 (which fixes their `layer.id` values, P0-02 5.6).
2. **Units in integers.** Every layer is an integer type with a stated unit; nothing is stored in `Fx` raw form. A reader converts to display units itself.
3. **The catalogue is part of the contract.** Changing a fixed layer's name, type, units or meaning is a schema change (section 9). Adding a layer is not: readers ignore layers they do not know, and a world generated with `--until` lacks later layers anyway. Each addition is recorded in `docs/specs/schema.md`.
4. **Sea level.** Sea level is elevation 0 in every world. Land fraction is controlled by where the elevation stage puts the coast, not by a variable sea level, so nothing downstream ever needs a sea-level parameter. No cell has elevation exactly 0. The elevation spec guarantees that the ocean (elevation < 0) is one 4-connected region containing every cell within the edge margin, so it touches all four map edges.

## 5. Working representation and stored representation

### 5.1 The rule

**A stage reads the stored, quantized layers of earlier stages, never their in-memory working values.** Each stage computes in whatever working precision its spec gives (`Fx`, `int64`, or narrower integers with a stated bound), then quantizes to the layer's type, writes the layer, and hands on nothing else. The next stage takes its inputs from the layer grids as written. An implementation may keep the written grid in memory instead of decoding it again, because it is the same array; it may not pass any other array between stages.

Consequences:

- **A stage's output is a pure function of the seed, the world parameters, the loaded content and the layers already in the database.** So a stage can be re-run or resumed alone: running `elevation`, writing the world, reading it back and running `hydrology` on the read-back layer gives the same layers as one uninterrupted run (test X2 in section 14).
- Each stage can be tested and golden-hashed from a fixture world holding only its input layers.
- The viewer, the game and the world critic see exactly what later stages used.
- Quantization error is fixed once, at a precision chosen per layer. Elevation in decimetres is the finest step any later stage needs (hydrology works on it directly; climate needs metres).

The alternative (stages pass working arrays, so later stages see `Fx` elevation) would keep a little precision but make every stage's result depend on the entire chain in one process: no resume, no per-stage fixtures, and a stored world that is not what downstream stages actually saw. It is rejected.

### 5.2 Working types

| Use | Type | Notes |
| --- | --- | --- |
| Continuous fields (noise, heights during erosion, interpolation) | `Fx` (Q32.32) | every bound below 2^31 in value; each spec shows its bound |
| Priority keys, quantile values, sums of `Fx` | `int64` (the raw value) | |
| Counts, areas, accumulations | `uint32` or `int64` | `uint32` only where the bound N² ≤ 2^22 is stated |
| Stored-layer arithmetic (hydrology on decimetres) | `int64` from the `i16` grid | |
| Squared differences of raw `Fx` | `U128` (P0-01 5.3) | e.g. slope comparisons |

## 6. Generator tuning as data

### 6.1 The `worldgen` kind

Generator tuning is TOML data, so it is hand-edited without a rebuild and covered by `content_hash` (P0-03 10 recommendation, adopted).

| Property | Value |
| --- | --- |
| Kind name | `worldgen` |
| Directory | `worldgen` (so files live in `game/data/worldgen/`) |
| Required | yes |
| Ids | a fixed set, one per stage: `elevation`, `hydrology` in the first pass; `climate`, `biomes`, `resources`, `regions` added by the second-pass specs. Each id has its own schema. An id outside the set is an error. When the kind's directory exists, every id in the set must be defined, whether or not the kind is required; a missing directory is an error only for a required kind, reported once against the directory |
| Fields | all required, no code defaults. The values in the shipped files are the defaults of the stage specs |

A kind with a fixed id set and a schema per id is new to the loader: amendment A3. The canonical encoding (P0-03 8) is unchanged: kind name, count, then each definition in ascending id order with its own schema's fields in declaration order.

Files: one per stage, named after the id, e.g. `game/data/worldgen/elevation.toml` holding `[worldgen.elevation]`. The file name carries no meaning (P0-03 7.4); the convention is for people.

Why all fields required: the shipped file is then the single place a value is written. With code defaults as well, the code and the file could disagree and nobody would notice, because the file's value would always win.

### 6.2 Field conventions

- Names are snake_case and carry their unit as a suffix: `_m` metres, `_m2` square metres, `_frac` fraction of the map side, no suffix for dimensionless values. Noise records say `cycles` (per map side) or `wavelength_m`.
- Fractions and real-valued parameters are `fx` fields written as decimal strings (P0-03 4); counts, areas in m² and wavelengths in metres are `int` fields.
- Every field has bounds in its schema chosen so that **no working value can overflow for N = 2048 at any value inside the bounds**. Each stage spec shows this. Cross-field rules are named checks (P0-03 7.5) listed in the stage spec.
- Noise parameters use shared record types (defined in P1-01 section 4.1 and reused by later stages): `fbm_map` (`cycles`, `octaves`, `gain`) and `fbm_m` (`wavelength_m`, `octaves`, `gain`). Lacunarity is fixed at 2 (P1-01 4.1).
- Each field in a stage spec's tuning table is marked **designed** (the value follows from the design or the algorithm) or **guess** (a starting value, to be tuned against the seed sweep).

### 6.3 Data root

From P1, P0-03 7.6 applies: without `--data`, the data root is `UNMAPPED_DATA_DIR` if set, otherwise `game/data/` in the source tree as recorded by CMake at configure time. An empty `UNMAPPED_DATA_DIR` counts as unset; a relative value resolves against the current directory. The path is never stored in the world.

### 6.4 Content hash and golden values when tuning changes

Two sets of tuning files exist:

| Set | Location | Used by | Changes when |
| --- | --- | --- | --- |
| Shipped | `game/data/worldgen/` (and later `game/data/biomes/`) | `worldgen-cli` by default; CI world cases; seed sweeps; the critic | tuning is improved |
| Fixture | `worldgen/tests/data/geo/` (same layout: `worldgen/`, later `biomes/`) | the per-stage golden-seed tests and property tests in `worldgen-tests` | only when an algorithm test needs it; a copy of the shipped values at the time the stage's spec is approved |

Policy:

1. Changing any tuning value changes `content_hash`, so it changes the bytes of every world generated with that data, including worlds that run no geography stage.
2. **A change to the shipped data that changes `content_hash` bumps `GENERATOR_VERSION` (PATCH) and re-records the CI world golden hashes in the same branch.** Reason: users regenerate worlds from a seed and a version; the shipped data is part of what a version produces. (Strictly, the world is reproducible from seed, parameters, data and version, and `content_hash` records the data; the bump keeps "same seed and version, same world" true for anyone using the shipped data.)
3. The per-stage golden tests use the fixture set, so tuning iterations during P1 do not churn them. They change only when an algorithm changes, which bumps `GENERATOR_VERSION` (MINOR while the prototype is pre-1.0) and re-records them.
4. The first merge that adds geography stages bumps `GENERATOR_VERSION` to `0.3.0`.

### 6.5 Tuning that is not data

Fixed constants that are part of an algorithm's definition (cell size, D8 codes, lacunarity 2, the river class cap 15, the chamfer weights) are constants of the code and named in the stage spec. World-creation settings (map size, later history span) stay in the settings file and in `meta` as `param.*` (P0-02 5.5); tuning never appears in `meta` except through `content_hash`.

### 6.6 The `biome` kind

Fixed now so that the second-pass specs, the render palette and the viewer agree. Registered by the biome stage's implementation (second pass), not in the first pass.

| Property | Value |
| --- | --- |
| Kind name / directory | `biome` / `biomes` (files in `game/data/biomes/`) |
| Required | yes, once registered |
| Fields fixed now | `code` int(1, 254), unique across definitions (named check); `name` string(64), the display name; `colour` int(0, 16777215), 0xRRGGBB for maps; `water` bool, true for water categories |
| Fields added later | the biome stage spec adds its lookup and special-case fields |

Definitions (ids, codes and colours fixed; names are the display defaults):

| Id | `code` | `water` | `name` | `colour` |
| --- | --- | --- | --- | --- |
| `ocean` | 1 | true | Ocean | 0x183878 |
| `lake` | 2 | true | Lake | 0x4080CC |
| `temperate_forest` | 3 | false | Temperate forest | 0x2E6034 |
| `grassland` | 4 | false | Grassland | 0x96AA5A |
| `marsh` | 5 | false | Marsh | 0x5C6E54 |
| `highland_moor` | 6 | false | Highland moor | 0x786468 |
| `alpine` | 7 | false | Alpine | 0xBEBEB4 |
| `blighted_waste` | 8 | false | Blighted waste | 0x605446 |

Interim readings (proposals entry B6): rivers are sub-cell features and get no biome of their own; a river cell carries the biome of its land. The design doc's special cases for coast, river and high peaks select among these biomes (for example marsh beside lowland rivers, alpine on peaks) rather than adding categories. `blighted_waste` is a history output (design doc section 4), so the P1 biome stage assigns it to no cell; P3 or later rewrites are a question for the history specs (a later stage cannot rewrite `biome` under rule 4.1; history would write its own overlay layer). The biome stage writes table `biome` (section 9.2) so a world file names its own codes.

## 7. Shared algorithms

Defined once here; stage specs cite them by number. Each is a mathematical definition plus the order that makes it unique. An implementation may use any method that gives the same result.

### 7.1 G1: k-th value and quantile

`kth(values, k)`: the k-th smallest element (0-based) of a multiset of `int64` values, 0 ≤ k < n. It is a value, so it does not depend on how ties are ordered: sorting, `std::nth_element` or a histogram refinement all give the same number.

`quantile(values, q)` for `Fx` q with 0 ≤ q ≤ ONE: k = `quantile_rank(q, n)` = min(n − 1, (q.raw × n) >> 32), computed in `uint64`; result `kth(values, k)`. Fatal if n = 0 or q outside [0, ONE]. G1 accepts up to 2^31 values (fatal above), so the product q.raw × n ≤ 2^32 × 2^31 = 2^63 fits `uint64`; maps have at most 2^22.

**In-place selection.** A selection helper that works in the caller's array leaves that array in an unspecified arrangement, which differs between standard libraries. After such a call the array may be used only as a multiset; no stage may read positions from it.

Worked example: values 5, 3, 9, 1, 7, 3, 8, 2 (n = 8; sorted 1, 2, 3, 3, 5, 7, 8, 9). q = 0 → k 0 → 1. q = HALF → k 4 → 5. q = ONE → k = min(7, 8) = 7 → 9. q = 0.42 (raw 1803886264) → (1803886264 × 8) >> 32 = 3 → 3.

### 7.2 G2: connected components

Input: a predicate P on cells, a connectivity (4 or 8) and optionally an equivalence E on adjacent cells (default: always true). The equivalence is always "the two cells' keys are equal", given as a key array (`int64` per cell). A component is a maximal set of P-cells connected by steps between adjacent P-cells u, v with E(u, v). Labels: scan cells in index order; each P-cell not yet labelled starts a new component with the next label (1, 2, …), whose cells are collected by FIFO breadth-first search from it, visiting neighbours in the 4- or 8-neighbour order of 2.3. So components are labelled in order of their minimum index, and the BFS visit order (needed where a spec uses BFS parents) is unique. BFS parents are stored as the D8 direction code from a cell back to its parent, as in G3.

### 7.3 G3: routing flood (priority flood)

Input: keys h[i] (`int64`) for every cell, and an ocean mask; every non-ocean cell must have h ≥ 1 (heights above sea level in the stage's unit, which may be decimetres or `Fx` raw). Output: flood level L[i], flood parent `parent[i]` (a D8 code, or 8 for ocean), and `order`, the sequence of all cells in pop order.

```
seq = 0; Q = empty
for i in 0 .. n-1 (index order):
    if ocean[i]: L[i] = 0; parent[i] = 8; visited[i] = true; push (0, seq, i); seq += 1
while Q not empty:
    pop the entry (k, s, c) with the smallest (k, s)
    append c to order
    for code in 0 .. 7:
        m = neighbour of c in direction code; skip if outside the map or visited[m]
        visited[m] = true
        L[m] = max(h[m], L[c]); parent[m] = (code + 4) mod 8
        push (L[m], seq, m); seq += 1
```

Pop order is ascending (key, seq) with `seq` the global push counter: a binary heap, a bucket queue (for small integer keys) or a radix heap all produce it, since (key, seq) pairs are unique. Every cell is pushed and popped exactly once (the map is connected and the ocean is not empty).

Preconditions, each a fatal error: a non-ocean key below 1; no ocean cell at all. G4 and G5 share them, and in addition G4 and G5 treat a negative level, or (for G5) an `order` that is not a permutation of the cells listing every receiver before its donors, as fatal.

Properties used by the stages (each is a test in P1-02):

- **Monotone pops.** Keys popped never decrease, because a pushed key L[m] = max(h[m], L[c]) ≥ L[c], the key being popped.
- **L is the spill level.** L[i] is the minimum over all paths from i to an ocean cell of the maximum h on the path (ocean cells counting as 0). L[i] ≥ h[i] for land; L[i] > h[i] exactly on cells in a depression.
- **Order is topological for the receivers of G4.** A cell's flood parent is popped before it, and so is any neighbour with a strictly smaller L.

Worked example: P1-02 section 5.

### 7.4 G4: receivers (steepest descent on the flood surface)

Input: L, parent and the ocean mask from G3. Output: `rcv[i]`, a D8 code for every non-ocean cell.

For a non-ocean cell c, among its existing neighbours m (8-neighbour order) with L[m] < L[c], choose the one with the steepest slope: with drop d = L[c] − L[m] (> 0), the slope is d/1 for an orthogonal and d/√2 for a diagonal neighbour, compared exactly as the score w·d² with w = 2 orthogonal, 1 diagonal, in `U128`. A candidate replaces the current best only if its score is strictly greater, so ties go to the lowest code. If no neighbour has a strictly smaller L (c is on a flat of the flood surface, including every depression cell), `rcv[c] = parent[c]`.

No cycles: a steepest step strictly decreases L; a parent step goes to an earlier-popped cell with L no greater. Around a cycle L would have to be constant, so every step would be a parent step, and pop positions would strictly decrease around it: impossible. Following `rcv` from any non-ocean cell reaches the ocean, and G3's `order` lists every cell after its receiver.

### 7.5 G5: accumulation

Input: `rcv` for non-ocean cells. A[c] for non-ocean c = the number of non-ocean cells whose downstream path (following `rcv`, c itself included) passes through c. Equivalently A[c] = 1 + Σ A[u] over the cells u with rcv pointing to c. A is a sum of integers, so any topological order gives the same values. With G3's `order`: set A = 1 on non-ocean cells, then for c in reverse `order`, if c is not ocean and its receiver m is not ocean, A[m] += A[c]. A ≤ N² ≤ 2^22 fits `uint32`. Where a stage changes some receivers after G4 (lake rerouting, P1-02), `order` may no longer be topological and the stage computes A in another topological order (P1-02 gives one).

### 7.6 G6: row blocks

Parallel per-cell work is split into blocks of 64 rows: block b covers rows 64b to min(64b + 64, N) − 1. The split depends only on N. On a W × H grid (tests only) the split is by rows of the grid's height H.

## 8. Determinism and parallelism

P0-01 4.2 applied to each kind of algorithm geography uses. A stage spec names the rule for each step (for example "E4 is D1; E8 step 4 is D4").

| Rule | Kind of algorithm | What is required | Parallel |
| --- | --- | --- | --- |
| **D1** | Per-cell function | The value at a cell depends only on its coordinates, the stage inputs and location-keyed noise or hashes; never on other cells being computed. | Yes, by G6 row blocks, any number of threads |
| **D2** | Neighbourhood filter (Jacobi) | Reads one complete input grid, writes a separate output grid; never in place. Behaviour at the map edge is stated (skip, or a stated substitute value). | Yes, G6 row blocks |
| **D3** | Iterative simulation | A fixed iteration count from tuning. No convergence test on a tolerance; an early exit only on an exact integer condition that is the same on every platform, and always with the fixed maximum. Each iteration is a fixed sequence of D1, D2, D4–D8 steps. | As its steps |
| **D4** | Sequential sweep (Gauss–Seidel, in place) | Single-threaded, in a stated order (index order, reverse index order, or an order such as G3's `order`). | No |
| **D5** | Flood fill, BFS, component labelling | FIFO queues; seeds in index order; neighbours in the 4- or 8-neighbour order; labels by G2. | No (labels and BFS parents depend on order) |
| **D6** | Priority queue | Pop order is ascending (key, seq), seq the global push counter starting at 0. The key is an integer (or `Fx` raw); never a value whose computation could differ by platform. Any queue structure. | No |
| **D7** | Sorting and selection | Sort only by a strict total order with the cell index (or another unique key) as the last tie-breaker, or use a value-only result such as G1. | Allowed, if the result is defined independently of the algorithm |
| **D8** | Reductions | Integer sums, histograms and `Fx` sums (exact integer additions of raw values) may be combined in any order **if the spec shows no partial sum can overflow**; otherwise combine in ascending block order. Min and max are values; an argmin or argmax breaks ties by the lowest index. Lists from blocks are concatenated in block order. | Yes, by G6 row blocks |
| **D9** | Randomness | Parallel code uses only location-keyed values (`hash_at`, `hash_at3`, noise). An `Rng` from a stage stream is used only in D4/D5/D6 code, or per block as `Rng(substream(key, b))`. One draw per statement (P0-01 4). | As above |
| **D10** | Thread count | Comes from `--threads` (or the library caller); used only to schedule blocks; never recorded in the world or used in a result. A count outside 1 to 256 given to the library is a fatal error; the command line's default (the hardware thread count) is capped at 256. If the system cannot start a thread for any reason, the work is done by the threads that did start, with the same result. Parallel calls are not nested. If several blocks raise the fatal error, the one reported is the lowest block's. Every stage with a parallel step has a test comparing 1, 2 and 5 threads (5 does not divide the block count of any power-of-two map). | — |

One further allowance, added to P0-01 4.2 by amendment A1: work may be split into parts defined by the data (for example independent drainage basins) when each part writes only its own cells, reads only values fixed before the parallel step, and the combined result is defined without reference to the split. No first-pass step needs it; it is the sanctioned way to parallelize the erosion solve if measurement requires (P1-01 section 8).

## 9. Schema

### 9.1 First pass: no schema change

Elevation and hydrology add layers only. The `layer` table, the blob format and the required `meta` keys are unchanged, so `SCHEMA_VERSION` stays 1. The values of `meta.stages` change (`elevation,hydrology`); that is content, not schema. No new `param.*` key is needed: tuning is data (section 6), and the map size is already `param.map_size`. Threads are never recorded.

`docs/specs/schema.md` is created now as P0-02 5.2 asks: the full current DDL, the required `meta` keys, a pointer to the layer catalogue and a changelog.

### 9.2 Second pass: planned changes

Planned here so the second-pass specs inherit them; each is made, with its version bump, by the branch of the stage that writes it.

| Version | Change | Branch |
| --- | --- | --- |
| 2 | `CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT`. `id` is the biome `code`, `key` the definition id, `water` 0 or 1. One row per loaded biome definition. Lets the viewer and `render` show biomes from the world file alone. | biomes |
| 3 | Table `region` (design doc section 6: `id`, `name`, `biome`, `bounds`, summary statistics), exact DDL in the regions spec. | regions |

If both land in one branch they share one bump (P0-02 5.2 rule 1).

## 10. Command-line changes

Amendment A4 carries the exact text for P0-04. Summary:

### 10.1 `generate`

| Option | Meaning |
| --- | --- |
| (default) | Runs every implemented geography stage in the order of section 3. In the first pass: `elevation`, `hydrology`. |
| `--until STAGE` | Runs the stages up to and including STAGE, a stage name from section 3. `--until none` runs no stage (the P0 "empty world"). An unknown or not-yet-implemented name is a usage error (exit 2). |
| `--test-pattern` | Runs only the P0 test-pattern stage, as before; no geography. Combining it with `--until` is a usage error. |
| `--threads N` | 1 to 256 threads for parallel steps (rule D10). Default: the number of hardware threads reported by the system, or 1 if unknown. Never affects the output. |

The P0 CI cases become: `empty-*` add `--until none`; `pattern-*` unchanged. New CI cases are in section 14.

### 10.2 `render`

`render` takes exactly one of `--layer NAME` and `--composite NAME`. Under `--palette auto` each layer gets its registered palette below; `gray` stays available for every layer. Every palette is an exact integer function; the RGB buffer is the cross-platform contract (P0-04 6).

**Interpolation rule** used by the ramps. For stops (t_k, colour_k) with t ascending, a value t with t_k ≤ t < t_{k+1} gets, per channel, c = c_k + floor((2(c_{k+1} − c_k)(t − t_k) + (t_{k+1} − t_k)) / (2(t_{k+1} − t_k))) (round half up, as P0-04's gray, with floor division on possibly negative numerators). Below the first stop: the first colour; at or above the last stop: the last colour.

**`hypsometric`** (layer `elevation`, value e in decimetres). Land (e > 0) uses the land stops on t = e; ocean (e ≤ 0) uses the ocean stops on t = −e:

| Land t (dm) | 0 | 500 | 2000 | 5000 | 10000 | 15000 | 20000 | 30000 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RGB | 70,120,64 | 104,146,78 | 150,168,96 | 178,160,108 | 150,120,86 | 128,106,92 | 196,192,188 | 250,250,250 |

| Ocean t (dm, depth) | 0 | 200 | 1000 | 5000 | 30000 |
| --- | --- | --- | --- | --- | --- |
| RGB | 120,170,205 | 88,140,190 | 56,104,168 | 32,66,130 | 16,34,82 |

Example: e = 250 (25 m) between stops 0 and 500: R = 70 + floor((2·34·250 + 500)/1000) = 70 + 17 = 87; G = 120 + floor((2·26·250 + 500)/1000) = 120 + 13 = 133; B = 64 + floor((2·14·250 + 500)/1000) = 64 + 7 = 71.

**`water`** (layer `water`): 0 → (214, 200, 160); 1 → (52, 100, 200); 2 → (64, 128, 204); 3 → (24, 56, 120); any other value → (255, 0, 255).

**`river`** (layer `river`): 0 → (16, 16, 16); 1 → (120, 170, 230); 2 → (100, 150, 225); 3 → (80, 130, 220); 4 → (60, 110, 210); 5 → (45, 90, 200); 6 to 15 → (30, 70, 190).

**`log2`** (layer `flow_accum`): gray g = min(255, 11 × bit_length(v)), bit_length as in amendment A1 (0 for 0). Pixel (g, g, g).

**`direction`** (layer `flow_dir`): 0 (230, 25, 75); 1 (245, 130, 48); 2 (255, 225, 25); 3 (210, 245, 60); 4 (60, 180, 75); 5 (70, 240, 240); 6 (0, 130, 200); 7 (145, 30, 180); 8 (0, 0, 0); other (255, 255, 255).

**`ids`** (layer `lake`, later `region`): 0 → (0, 0, 0). Value v ≥ 1: h = mix64(v) (P0-01 7.1); channel j (j = 0 red, 1 green, 2 blue) = 64 + floor(((h >> 8j) & 0xFF) × 191 / 255).

**`signed`** (layer `coast_dist`): with P = max(1, largest positive value) and M = max(1, −smallest negative value): v ≥ 0 → (g, g, g) with g = gray_level(v, P) of P0-04 6 (d = v, r = P); v < 0 → (0, 0, 255 − floor(3·gray_level(−v, M)/4)).

`water_depth` keeps `gray`. Second pass: `biome` → **`biome`** palette, colour from table `biome` (row with `id` = value; unknown value (255, 0, 255)); `temperature`, `rainfall` and the scores get ramps in their specs.

**Composite `map`** (`--composite map`; default output `<stem>.map.png`). Needs `elevation`; uses `water` and `river` when present. Per cell:

1. Ocean (e < 0): the hypsometric ocean colour.
2. Otherwise the hypsometric land colour, shaded: with e' = max(e, 0) of a neighbour and coordinates clamped to the map, gx = e'(x+1, y) − e'(x−1, y), gy = e'(x, y+1) − e'(x, y−1); f = clamp(256 + floor_div(gx + gy, 2), 160, 352); each channel c becomes min(255, (c·f + 128) >> 8). (Light from the north-west: slopes rising to the south-east are brighter.)
3. If `water` is present: a lake cell (2) is (64, 128, 204); a river cell (1) takes the `river` palette colour of its class (if `river` is absent, (52, 100, 200)).

The second pass may add composites (for example `biomes`: biome colour shaded as in step 2, water as in step 3).

### 10.3 `report`

Format version 2: the header becomes `# worldgen report v2`, sections `[meta]`, `[tables]`, `[layers]` as in v1, followed by `[geography]` when the world has an `elevation` layer. The second pass appends lines to `[geography]` for its layers. All numbers are integers or exact decimals:

- `ratio4(a, b)`: RHA(a × 10^4 / b) printed as an integer part, `.`, and four digits (e.g. `0.4213`); `n/a` if b = 0.
- `metres1(v)` for a value v in decimetres: `-` if v < 0, then |v| div 10, `.`, |v| mod 10 (e.g. `-12.5`).
- Percentile p (integer percent) of a multiset of n values: `kth(values, min(n − 1, floor(p·n/100)))`.

`[geography]` lines, in this order (`land` means `elevation` > 0, including lakes; `ocean` means `elevation` < 0). Percentiles over no values print `n/a`; `ratio4` with a zero denominator prints `n/a`; maxima over nothing are 0 (appendix C 6). The keys `step_p999_m` to `edge_parallel_run_m` were added on 2026-11-15 for the elevation redesign; the report stays format v2 (open question 8).

| Key | Value |
| --- | --- |
| `cells` | N² |
| `land_cells`, `ocean_cells` | counts |
| `land_fraction` | ratio4(land_cells, cells) |
| `landmasses` | number of 8-connected land components |
| `largest_landmass_share` | ratio4(cells of the largest, land_cells) |
| `coastline_m` | 16 × the number of 4-adjacent (land, ocean) cell pairs |
| `coast_ratio` | coastline_m divided by the perimeter of a circle of the land's area: with A = 256 × land_cells (m²) and Pq = isqrt_u128(4 × PI.raw × A × 2^32) (the perimeter as `Fx` raw), the value is `from_ratio(coastline_m × 2^32, Pq)` printed with `to_decimal(4)` |
| `land_solidity` | land area over the area of its convex hull: for each row y with land, the corner points (x_l, y), (x_l, y+1), (x_r + 1, y), (x_r + 1, y+1) of its westmost and eastmost land cells; the hull of all these points (Andrew's monotone chain on integers); 2H = twice the hull area by the shoelace formula; value ratio4(2 × land_cells, 2H) |
| `elevation_m` | land percentiles 0, 1, 10, 25, 50, 75, 90, 99, 100, each as metres1, separated by single spaces |
| `depth_m` | ocean percentiles 0, 50, 100 of −`elevation`, as metres1 |
| `land_above_m` | for thresholds 50, 500, 1000, 1500, 2000 m: `50:` ratio4(land cells with e ≥ 500, land_cells), then `500:` …, separated by spaces |
| `max_step_m` | metres1 of the largest \|e(a) − e(b)\| over 4-adjacent land pairs |
| `step_p999_m` | metres1 of the 99.9th percentile of the cell step m(c) over land (definition below) |
| `slope_over` | `35:` ratio4(land cells steeper than 35°, land_cells), a space, `45:` ratio4(land cells steeper than 45°, land_cells) |
| `slope_p50_above_500m` | tan4 of the median slope over land cells with e ≥ 5000 (500 m); `n/a` if there are none |
| `lowland_flat_share` | ratio4(land cells with e < 1000 (below 100 m) flatter than 0.5°, land cells with e < 1000); `n/a` if the denominator is 0 |
| `edge_parallel_run_m` | 16 × the longest edge-parallel coast run, in metres, an integer (definition below) |

**Cell step** (for `step_p999_m`). For each land cell c, m(c) = the largest \|e(c) − e(n)\| over the 4-neighbours n of c that exist and are land; m(c) = 0 if there are none. With n_L = land_cells, the value is kth(m over land, min(n_L − 1, floor(999 × n_L / 1000))) (`n/a` if n_L = 0). So `max_step_m` is the largest m(c), and for n_L < 1000 the two are equal.

**Cell slope** (for `slope_over`, `slope_p50_above_500m`, `lowland_flat_share`). Defined for land cells on the stored decimetre layer with 16 m cells, by central differences, the standard GIS gradient: it measures the surface over 32 m in each axis, so a single-cell spike counts in four cells but does not dominate as it would in a steepest-neighbour slope, and it is symmetric in direction. With e'(p) = max(e(p), 0) (the sea surface for ocean neighbours, as in the composite map) and, for cell (x, y) of a W × H layer:

```
xa = max(x − 1, 0); xb = min(x + 1, W − 1); ya = max(y − 1, 0); yb = min(y + 1, H − 1)
gx = (xb > xa) ? (e'(xb, y) − e'(xa, y)) × (2 div (xb − xa)) : 0      // decimetres per 2 cells
gy = (yb > ya) ? (e'(x, yb) − e'(x, ya)) × (2 div (yb − ya)) : 0
S2 = gx² + gy²                                                       // int64
```

At the edge of a grid (test layers only; on a real map land never touches the edge) the one-sided difference is doubled, so every gx and gy is in decimetres per 32 m = 320 dm. The slope's tangent is √S2 / 320. A tangent threshold is written as an exact decimal T = t / 10^4 and compared without square roots: **steeper than T** means 10^8 × S2 > (320 × t)²; **flatter than T** means 10^8 × S2 < (320 × t)². Thresholds, constants of the report: 35° is t = 7002 (tan 35° = 0.70021; 7002 is 34.9997°); 45° is t = 10000 (exact); 0.5° is t = 87 (tan 0.5° = 0.0087269; 87 is 0.4985°). In S2 terms: steeper than 35° ⟺ S2 ≥ 50205; steeper than 45° ⟺ S2 > 102400; flatter than 0.5° ⟺ S2 ≤ 7. Bounds: \|gx\|, \|gy\| ≤ 2 × 32767, so S2 < 2^33 and 10^8 × S2 < 2^60 fits `int64`. Resolution: the smallest non-zero tangent is 1/320 (0.18°), so "flatter than 0.5°" means a change of at most 2 dm over 32 m in each axis.

**tan4(S2)**: the tangent √S2 / 320 to four decimals: r = isqrt_u64(S2 × 10^8) (= floor(10^4 √S2)), T = (r + 160) div 320, printed as T div 10^4, `.`, and T mod 10^4 with four digits. The median is kth over the S2 values of the cells concerned at min(n − 1, floor(50 n / 100)) (the percentile rule above). For reading: tan 25° = 0.4663, 30° = 0.5774, 35° = 0.7002, 45° = 1.0000.

**Edge-parallel run** (for `edge_parallel_run_m`; it measures coasts that run near-straight, within ±64 m, parallel to a map edge). For each of the four sides, a sequence t over the lines parallel to that side: for the west side, t_a for each row a = 0 … H − 1 is the smallest x with land in row a; east, W − 1 − the largest such x; north, for each column a = 0 … W − 1, the smallest y with land in column a; south, H − 1 − the largest such y. t_a is undefined when the line has no land. The run from a start a with t_a defined is the largest r such that t_a … t_{a+r−1} are all defined and max − min of them is ≤ 8 (cells); 0 if t_a is undefined. The side's value is the largest run over all starts; the key's value is 16 × the largest side value (0 if there is no land). The tolerance 8 is a constant of the report.

Worked examples (each computed by the spec writer with an exact-integer script; X6 tests them):

1. A 7 × 6 layer, ocean −50 on the border, land interior (rows y = 1 to 4, columns x = 1 to 5): `1 2 2 3 2`, `2 2 2 200 400`, `2 3 100 500 900`, `1 2 3 400 700`. Then (gx, gy, S2) at (1, 1) is (2, 2, 8); at (2, 1) (1, 2, 5); at (4, 1) (0, 200, 40000) (32.0°, not over 35°); at (3, 2) (198, 98, 48808) (34.6°); at (4, 2) (398, 497, 405413). Land 20 cells; steeper than 35° and than 45°: the same 9 cells, (5, 1), (4..5, 2), (3..5, 3), (3..5, 4), so `slope_over` is `35:0.4500 45:0.4500`. All 20 cells are below 100 m; flatter than 0.5° (S2 ≤ 7): (2, 1), (3, 1), (1, 2), (2, 2), so `lowland_flat_share` is `0.2000`. No cell is ≥ 500 m: `slope_p50_above_500m` is `n/a`. Cell steps sorted: eight 1s, then 97, 197, 198, 300, 300, 397, 397, 398, 400, 400, 500, 500; rank min(19, floor(19980/1000)) = 19, so `step_p999_m` is `50.0`, equal to `max_step_m`.
2. A 4 × 3 layer, all land (rows `5000 5010 5040 5100`, `5000 5020 5080 5200`, `5010 5060 5150 5320`). One-sided differences at the edges are doubled: at (0, 0), gx = (5010 − 5000) × 2 = 20, gy = 0, S2 = 400. The twelve S2 values sorted: 400, 1700, 2000, 8900, 10400, 14500, 26000, 44500, 54400, 87200, 106000, 173200. Steeper than 35°: (3, 0), (3, 1), (2, 2), (3, 2): `35:0.3333`; steeper than 45° (S2 > 102400): (3, 1), (3, 2): `45:0.1667`. Median rank floor(600/100) = 6: S2 = 26000, r = isqrt(26000 × 10^8) = 1612451, T = (1612451 + 160) div 320 = 5039: `slope_p50_above_500m` is `0.5039` (26.7°). No lowland: `lowland_flat_share` is `n/a`.
3. Edge run: a west-side sequence over 12 rows t = (undefined, 20, 21, 25, 28, 29, 30, 12, 13, 14, undefined, 15). From a = 1: 20, 21, 25, 28 (range 8), adding 29 makes 9, so the run is 4; from a = 3: 25, 28, 29, 30 (range 5), adding 12 breaks it: 4; from a = 7: 12, 13, 14, then undefined: 3; from a = 11: 1. The side's value is 4; if it is the largest side, `edge_parallel_run_m` is `64`.

When `water`, `flow_dir`, `flow_accum`, `river` and `lake` are all present, these follow:

| Key | Value |
| --- | --- |
| `lakes` | the largest `lake` value |
| `lake_cells` / `lake_share_of_land` / `largest_lake_cells` | count of `water` = 2; ratio4 of it to land_cells; cells of the largest lake |
| `river_cells` / `river_share_of_land` | count of `water` = 1; ratio4 to land_cells |
| `river_sources` | river cells with no upstream neighbour (a cell whose `flow_dir` points at it) that is a river or lake cell |
| `river_mouths` | river cells whose downstream cell is ocean |
| `major_rivers` | river mouths with `river` class ≥ 4 |
| `longest_river_m` | for every cell, D = path length from it to the ocean along `flow_dir`, each orthogonal step 16 × ONE and each diagonal step 16 × SQRT2 (raw, amendment A1), summed in raw `int64`; the value is `round_int` of the largest D over river sources, in metres |
| `largest_basin_share` | ratio4(largest `flow_accum` over cells whose downstream is ocean, land_cells) |

## 11. Performance budget

Targets for the validation specialist to measure, not guarantees. Release build, default `--threads`, a 6-core desktop (Paul's development machine), shipped tuning, wall-clock time of `generate` including loading and writing the file.

| Stage | 512 | 1024 (default) | 2048 |
| --- | --- | --- | --- |
| `elevation` (noise, erosion, conditioning) | 2 s | 8 s | 35 s |
| `hydrology` | 0.3 s | 1.2 s | 5 s |
| `climate` | 0.3 s | 1.5 s | 6 s |
| `biomes` | 0.1 s | 0.5 s | 2 s |
| `resources` | 0.2 s | 1 s | 4 s |
| `regions` | 0.3 s | 1.5 s | 6 s |
| load data, encode layers, finalize | 0.3 s | 1 s | 4 s |
| **All of geography** | **4 s** | **15 s** | **65 s** |
| Peak memory | 200 MB | 600 MB | 2 GB |

Milestone 1 requires geography plus history under two minutes at the default size; geography at 15 s leaves at least 100 s for history and derivation. Debug and sanitizer builds are not budgeted (expect 5 to 20 times slower). A stage over twice its target at 1024 is a defect to report; P1-01 section 8 gives the fallbacks for erosion, the expensive part.

## 12. Math functions

Geography needs none of the functions deferred in P0-01 9.2 (`sin`, `cos`, `atan2`, `exp`, `log`, fractional `pow`). The first-pass formulations avoid them:

| Need | Formulation |
| --- | --- |
| Distance from the map centre | `hypot` (P0-01 9.1) |
| Stream-power erosion A^m | m fixed at 1/2: `isqrt_u64` (P1-01) |
| Slope to a diagonal neighbour | multiply by `INV_SQRT2`, or compare w·d² (G4) |
| River size class | `bit_length` of an integer ratio (an integer log2) |
| Mountain belts, warps | noise compositions; no rotation, so no trigonometry |
| Hillshade | finite differences (10.2) |
| Slope statistics of `report` (10.3) | central differences compared with exact decimal tangent thresholds by squared integer inequalities; `isqrt_u64` for the printed median tangent |

Expected for the second pass, recorded so its specs do not reach for transcendentals by habit: temperature linear in latitude and elevation; prevailing wind on the 8 D8 directions (or 16, with a committed table of integer vectors); moisture decay along the wind as a repeated multiply by a factor, per cell step. If a second-pass spec still needs a deferred function, it adds it to P0-01 in the form 9.2 demands.

Amendment A1 adds two named constants and one integer helper to P0-01: `SQRT2` (raw 6074001000, round(√2 · 2^32)), `INV_SQRT2` (raw 3037000500, round(2^32/√2)) and `bit_length(uint64)`.

## 13. How the result is judged

### 13.1 Statistics over a seed sweep

The validation specialist sweeps with shipped tuning: seeds 0 to 199 at 1024, 0 to 49 at 512, 0 to 19 at 2048, and collects the `[geography]` lines of `report` (10.3). The table gives acceptable ranges. "All seeds" bounds are hard: a seed outside them is a defect report with the seed. "Typical" bounds should hold for at least 90% of seeds; missing them sends tuning back. **Every range is an initial guess, to be tuned against the first sweep;** the critic and the lead may revise them once real terrain has been seen, and the revision is recorded here.

Fractions apply at every map size. Counts are given for 1024; at 512 divide by 4, at 2048 multiply by 4 (rounding the bounds outward).

**Revised 2026-11-16 for elevation candidate v16** (P1-01, third version). Ranges are set from the v16 sweeps (512 seeds 0–19, 1024 seeds 0–19, 2048 seeds 0–4; `v16-{512,1024,2048}-stats.txt` in the exploration output) with margins for the larger sweep above; the measured min–max is given beside each. Following Paul's decision (proposals "High peaks may be inaccessible; history opens the passes"), the steepness and flatness statistics describe the terrain and are not limits, with one exception set by the lead: `step_p999_m` ≤ 150 m, a hard check at every map size. The lead's earlier decision stands that `elevation_m` p90 and `land_above_m` 500 keep hard lower bounds (proposals "Every world has real uplands"). "Descriptive" rows are reported by the sweep with their range; a value outside the measured range is worth a look but is not a defect. Per-size bounds are written 512 / 1024 / 2048 where the size matters; otherwise the count rule above applies. **Comparison is exact:** each statistic is compared with its bounds as the exact value of its 10.3 definition (a ratio as the exact fraction of its two integer counts, for example `land_fraction` as land_cells / cells; `coast_ratio` as the exact quotient before `to_decimal`; heights and steps in exact decimetres), not as the rounded text `report` prints. The sweep tool computes these from the decoded layers, or from the integer counts `report` also prints where they suffice; a printed `ratio4` value may round onto the other side of a bound, and the exact value decides.

| Statistic | All seeds (hard) | Typical (≥ 90%) | v16 measured, 512 / 1024 / 2048 | What it checks |
| --- | --- | --- | --- | --- |
| `land_fraction` | 0.34–0.50 | 0.38–0.46 | 0.399–0.411 / 0.400–0.414 / 0.400–0.411 | the coast threshold and clean-up |
| `largest_landmass_share` | ≥ 0.85 | ≥ 0.95 | 0.977–1 / 0.982–1 / 0.990–1 | one continent |
| `landmasses` (1024) | ≤ 40 | ≤ 15 | 1–5 / 1–7 / 1–6 | few islands |
| `coast_ratio` | 1.4–6.0 | 1.6–3.5 | 1.56–2.83 / 1.56–2.84 / 1.74–2.85 | a coastline with bays and peninsulas (little 100 m–1 km detail: P1-01 limitation 9) |
| `land_solidity` | 0.45–0.93 | 0.70–0.90 | 0.741–0.900 / 0.736–0.890 / 0.755–0.888 | large bays and peninsulas |
| `elevation_m` p50 | 40–300 m | 60–200 m | 82–207 / 86–147 / 87–95 m | lowlands exist |
| `elevation_m` p90 | 300–1300 m | 400–1100 m (2048: 330–1100 m) | 520–1052 / 455–1140 / 342–1026 m | uplands exist |
| `elevation_m` p100 | 1200–2800 m | 1500–2600 m (512: 1300–2400 m) | 1245–2260 / 1530–2584 / 2234–2434 m | peaks for alpine |
| `land_above_m` 1500 | ≤ 0.08 | ≤ 0.05 | 0–0.044 / 0.0001–0.059 / 0.012–0.033 | alpine share; no lower bound (some 512 and 1024 worlds have no land above 1500 m; p100 checks the peaks) |
| `land_above_m` 500 | 0.06–0.35 | 0.08–0.30 | 0.103–0.316 / 0.092–0.267 / 0.074–0.201 | highland moor and forested uplands |
| 1 − `land_above_m` 50 | 0.10–0.50 | 0.15–0.40 | 0.160–0.225 / 0.164–0.228 / 0.175–0.238 | coastal and river lowlands for marsh and grassland |
| `max_step_m` | diagnostic | — | 72–140 / 68–127 / 75–90 m | the worst single step; reported, not tested |
| `step_p999_m` | ≤ 150 m at every map size | — | 65–130 / 54–87 / 52–61 m | no cliffs from artefacts beyond one land cell in a thousand |
| `slope_over` 35 | descriptive | — | 0.19–0.51 / 0.13–0.34 / 0.07–0.21 | how much land is too steep to walk |
| `slope_over` 45 | descriptive | — | 0.15–0.39 / 0.10–0.23 / 0.05–0.16 | crags |
| `slope_p50_above_500m` | descriptive | — | 1.18–2.94 / 1.02–1.88 / 1.20–1.43 (50° to 71°, 46° to 62°, 50° to 55°) | steepness of the high ground |
| `lowland_flat_share` | descriptive | — | 0.002–0.014 / 0.010–0.023 / 0.020–0.039 | floodplains and coastal flats (rare: P1-01 limitation 9) |
| `edge_parallel_run_m` | descriptive | — | 992–2128 / 1200–3200 / 1984–3040 m | coasts pressed along an edge or the falloff (checklist item 2) |
| `lakes` | 512: ≤ 25; 1024: 1–100; 2048: 4–400 | 512: ≥ 1; 1024: 3–25; 2048: 6–60 | 1–3 / 4–12 / 8–15 | |
| `lake_share_of_land` | ≤ 0.031 | 0.003–0.03 | 0.0038–0.0291 / 0.0113–0.0300 / 0.0298–0.0300 | at most `lake_max_share` (0.03) by construction; the cap binds in most worlds (P1-01 limitation 7) |
| `river_share_of_land` | 0.003–0.10 | 0.004–0.06 | 0.0040–0.0093 / 0.0064–0.0093 / 0.0103–0.0121 | river density at the 0.5 km² threshold (P1-02 open question 3; P1-01 open question 4) |
| `major_rivers` (1024) | 1–40 | 3–20 | 0–2 / 3–7 / 15–18 | |
| `longest_river_m` | ≥ 0.15 × map side in m | ≥ 0.25 × map side | 0.18–0.86 / 0.39–1.50 / 0.54–0.85 × side | rivers cross the land |
| `largest_basin_share` | 0.03–0.6 | 0.08–0.4 | 0.084–0.370 / 0.090–0.338 / 0.109–0.305 | neither one basin draining everything nor fragmentation |

Changed against the first ranges: `coast_ratio` hard 1.5 → 1.4 and typical 2.0–4.5 → 1.6–3.5 (the v16 median is 1.9 to 2.0); `land_solidity` hard upper 0.90 → 0.93, typical 0.55–0.85 → 0.70–0.90; p90 typical 450–1000 → 400–1100 m (2048 from 330 m); p100 hard 1200–2400 → 1200–2800 m and typical 1600–2400 → 1500–2600 m (512: 1300–2400 m); `land_above_m` 1500 lower bounds removed, upper 0.06 → 0.08; `land_above_m` 500 typical 0.10–0.25 → 0.08–0.30; `step_p999_m` hard ≤ 150 m at every size; the slope statistics, `lowland_flat_share` and `edge_parallel_run_m` settled as above; `lakes` per size; `lake_share_of_land` lower hard bound removed (a 512 world may have no lake), upper set by construction (the cap always applies, P1-01 E10.3); `river_share_of_land` hard 0.01 → 0.003 and typical 0.02–0.06 → 0.004–0.06; `longest_river_m` hard 0.25 → 0.15 and typical 0.4 → 0.25 of the side (512 worlds are mountain-heavy and their rivers short).

Second pass: the fraction of land in each of the five natural biomes is at least 0.02 in 90% of seeds and no natural biome exceeds 0.6; `blighted_waste` covers 0 cells in P1. The biome spec states these in its own terms.

### 13.2 Visual checklist

For the world critic, on `render --composite map --scale 1` (and `--layer elevation`, `river`, `lake`) of at least 20 seeds at 1024 and 5 each at 512 and 2048. Each item is a yes or no with seeds cited:

1. One dominant continent; ocean on all four sides; the land never runs into the map edge or along it.
2. The coastline has bays and peninsulas at several scales; no straight runs longer than about 1 km; no shapes aligned with the map axes or the map edge.
3. Mountains form ranges (each high area clearly longer than it is wide) with valleys between ridges, not an even scatter of bumps; peaks sit in ranges.
4. Broad lowlands exist: coastal plains and inland basins, flat enough for marsh along rivers.
5. Valleys form branching (dendritic) networks on slopes; no grid, no 45° or 90° striping, no long parallel straight channels on flats.
6. Rivers start in uplands, join downstream, grow in class, and reach the sea, mostly in bays; no river runs along the coast for a long way.
7. Lakes sit in basins, including some in the mountains; each has a visible outflow; no rash of tiny lakes.
8. No noise artefacts: no square lattice pattern, no repetition or tiling, no seams every 64 rows (G6 blocks) or at the map centre.
9. The three map sizes look like the same kind of world.
10. Places history needs exist on most maps: fertile river valleys and plains, hill and spur sites above them, gaps between the ends of ranges. (The first merged elevation has no natural saddle passes: ranges are barriers that history crosses with built roads; proposals "Natural passes through the ranges".)

Images of a few seeds, chosen by the critic, go into `docs/reviews/` with the review (small, deliberately copied, per `CLAUDE.md`).

**Known limitations of the first merged elevation.** P1-01 section 11 lists the limitations accepted for the first merge (closed ranges without passes, small filled pads among peaks, the floor's shapes in a few worlds, a few straight gullies, octagonal sea contours, coastal-cliff pockets, the lake cap, steep 512 worlds, v13's coasts and lowlands). A critic's checklist answers items 3, 4, 5, 7, 8 and 10 against that list and the reviews of v13 and v15 (`docs/reviews/p1-01-elevation-v13.md`, `docs/reviews/p1-01-elevation-v15.md`); a limitation on the list is reported with its seeds but is not a new defect.

## 14. Tests that span stages

Each stage spec lists its own tests. These belong to no single stage:

- **X1 Pipeline.** `generate` with default options writes `elevation` then the hydrology layers, with ids 1 to 8 in catalogue order, and `meta.stages` = `elevation,hydrology`. `--until elevation` writes only `elevation` and `stages` = `elevation`. `--until none` writes no layer and `stages` empty. `--until climate` (not yet implemented) and `--until bogus` exit 2. `--test-pattern --until elevation` exits 2.
- **X2 Resume equivalence.** For seeds 0 and 42 at N = 128 (library) and 512 (CLI): run `elevation`, write the world, open it with the reader, run `hydrology` on the decoded `elevation` grid alone; every hydrology layer equals the one from an uninterrupted run (layer hashes of P0-04 8).
- **X3 Threads.** `generate` at 512 for seeds 0 and 42 with `--threads 1`, `2` and `5`: byte-identical files.
- **X4 Map-size independence of the outline.** Not a hard test: the validation specialist reports, for 10 seeds, the share of cells on which the land mask at 1024, downsampled 2:1 by majority (land if at least 2 of 4), agrees with the land mask at 512. Expected well above 0.9.
- **X5 CI world cases.** `tools/ci/world-hashes.sh` gains `geo-seed-0-512` (`--seed 0 --map-size 512`), `geo-seed-42-512`, `geo-phrase-512` (`--seed-phrase "The Unmapped" --map-size 512`) and `geo-seed-1-1024` (`--seed 1`), compared across all presets; goldens in `worldgen/tests/golden/p1-worlds.txt`, recorded by the validation specialist. If the debug presets make the 1024 case too slow for CI, the build engineer may drop it from those presets only and say so.
- **X6 Report v2.** The exact `report` text of `--until none --seed 0` (header `v2`, no `[geography]`); for a geography world, the `[geography]` keys appear in the order of 10.3, and each value is recomputed by the test from the decoded layers with the definitions of 10.3 on one 128 × 128 library world.
- **X7 Palettes.** For each palette in 10.2, the RGB of hand-chosen values (the worked example of `hypsometric`; every `water`, `river`, `direction` value; `log2` of 0, 1, 2^22; `ids` of 1 computed by the test from `mix64`); and the SHA-256 of `render_rgb` of each layer of `geo-seed-0-512` as golden values.

## 15. What the second pass inherits

Fixed by this overview and the first-pass specs: stage order and names; layer names, types, units and special values (section 4); sea level 0 and the ocean's shape; coordinates and the map-size convention, including the latitude span as a map fraction; the stored-layer rule; the tuning kind, its conventions and the golden policy; the `biome` kind's ids, codes, colours and the planned `biome` table; shared algorithms G1 to G6 and rules D1 to D10; palettes and report conventions; the budget per stage; "no transcendentals unless added to P0-01 properly".

Left open for the second pass: the climate model (wind directions, rain shadow, lapse rate, seasonality, any extra climate layers); whether river discharge should be re-weighted by rainfall after climate (hydrology's `flow_accum` assumes uniform rain, P1-02 open question 2); the biome lookup and special-case rules and their tuning; how resources and habitability are scored; the region algorithm, sizes, whether sea and lakes get regions, region names (possibly a P3 language system), and the `region` table's columns; a river or lake table if history needs named rivers (P1-02 section 9).

## 16. Open questions

1. **The map-size convention** (2.2): more land at the same scale; continental layout in map fractions, relief in metres; climate latitude span in map fractions. Approve. Proposals B1, B2.
2. **Stored-layer rule** (5.1). Approve.
3. **Tuning as a fixed-id `worldgen` kind, all fields required, shipped and fixture sets, PATCH bump on shipped-data change** (section 6). Approve; needs amendment A3. Proposal B3.
4. **Biome categories** (6.6): six land biomes plus `ocean` and `lake`; rivers take their land's biome; no biome for coast or peaks; the `biome` table in the world. Approve. Proposal B6.
5. **Layer catalogue additions are not schema changes; changes to fixed layers are** (4, rule 3). Approve. Proposal B7.
6. **Budget** (section 11): 15 s for geography at 1024. Approve or set another split for milestone 1.
7. **`--threads` default** is the hardware thread count. The alternative, default 1, is slower for every user for no determinism benefit, since rule D10 makes the count irrelevant to output.
8. **Report format version.** The `[geography]` keys `step_p999_m`, `slope_over`, `slope_p50_above_500m`, `lowland_flat_share` and `edge_parallel_run_m` (10.3) change the report text of every geography world. Interim: the format stays v2, since no geography world is yet a released contract and the CI goldens of P1 are not recorded; if the lead prefers, the header becomes `# worldgen report v3`.

## Appendix A. Requested amendments to P0 specs

Exact text for the lead to add to each P0 spec's amendments section. Numbering continues each spec's list.

### A1. P0-01 (amendment 14, or the next number)

> **14. P1 additions (P1-00 sections 8 and 12).** (a) Named constants beside `PI` in 5.1: `SQRT2`, raw 6074001000 (= round(√2 × 2^32)), and `INV_SQRT2`, raw 3037000500 (= round(2^32 / √2)). (b) An integer helper in 9.1: `bit_length(uint64 v)` = 0 for v = 0, otherwise floor(log2 v) + 1 (the number of significant bits); exact, no table. Tests: `bit_length` of 0, 1, 2, 3, 2^63, 2^64 − 1 is 0, 1, 2, 2, 64, 64; `SQRT2.raw` and `INV_SQRT2.raw` equal the constants above and `mul(SQRT2, INV_SQRT2)` is exactly ONE. (c) Rule 4.2 gains: "5. The thread count is supplied by the caller, used only for scheduling, and never stored or used in a result. When several chunks fail, the fatal error reported is that of the lowest chunk index. 6. Work may also be split into parts defined by the data (for example drainage basins) provided each part writes only its own cells, reads only values fixed before the parallel step, and the combined result is defined without reference to the split; the spec using this states the partition." (d) Section 9.2: P1-00 section 12 records that the geography stages of the first pass need none of the deferred functions. (e) Section 8.2's last sentence now reads: "Domain warping and ridged noise are composed from `noise2` and `fbm2` in P1-01 section 4.1 (`ridged_m`); they may be implemented beside `fbm2` in the core."

### A2. P0-02 (amendment 13)

> **13. Layers in P1 (P1-00 section 4).** The geography layer catalogue is in `docs/specs/p1-00-geography-overview.md` section 4 and summarized in `docs/specs/schema.md`. Each layer is written exactly once, by the stage that owns it; no stage updates or deletes a layer written by another stage. Adding a layer to the catalogue is not a schema change (readers ignore unknown layer names). Changing the name, element type, units or meaning of a catalogued layer is a schema change under 5.2. `meta.stages` lists stage names from P1-00 section 3, e.g. `elevation,hydrology`; P1's first pass adds no table, column or required `meta` key, so `SCHEMA_VERSION` stays 1.

### A3. P0-03 (amendment 16 and 17)

> **16. Fixed-id kinds (P1-00 section 6).** A kind may be registered with a fixed set of ids, each with its own schema, instead of one schema for all definitions. For such a kind: a definition whose id is not in the set is an error at its header ("unknown id '<id>' for kind '<kind>'; expected one of …"); a missing id is an error ("missing definition '<kind>.<id>'") reported against the kind's directory with no position; each definition is validated against its id's schema, including named checks. The canonical encoding (section 8) is unchanged: definitions in ascending id order, each record encoded with the fields of its own schema in declaration order. The registry test checks that the id sets are non-empty, that ids match the id grammar, and that each id's schema is valid. The first such kind is `worldgen` (directory `worldgen`, required), with ids `elevation` and `hydrology` from P1-01 and P1-02; the second-pass specs add `climate`, `biomes`, `resources` and `regions`.
>
> **17. Data root from P1 (7.6, amendment 9).** With `worldgen` registered, the default data root of 7.6 is in force: `--data DIR`, else `UNMAPPED_DATA_DIR`, else `game/data/` as recorded at configure time. Because `worldgen` is required, a world can no longer be generated with no data root; `content_hash` of every world, including one generated with `--until none`, is the hash of the loaded data and no longer `e3b0c442…b855`. Tests that need a known data set use `worldgen/tests/data/geo/` (P1-00 6.4).

### A4. P0-04 (amendment 16)

> **16. Geography (P1-00 section 10).** (a) `generate` runs, by default, every implemented geography stage in the order of P1-00 section 3. New options: `--until STAGE` (a stage name from P1-00 section 3, or `none` for no stage; an unknown or unimplemented name is a usage error, exit 2) and `--threads N` (decimal 1 to 256; default the hardware thread count, or 1 if unknown; never affects the output). `--test-pattern` runs the test-pattern stage only, with no geography; with `--until` it is a usage error. `WorldParams` gains the stage limit and the thread count; the thread count is not part of what a world depends on (section 10 is unchanged). (b) `render` takes exactly one of `--layer NAME` and `--composite NAME` (usage error otherwise); `--composite map` is defined in P1-00 10.2, and its default output is `<build root>/out/<db file stem>.map.png`. `--palette` accepts `auto`, `gray` and the palette names of P1-00 10.2; a named palette that does not suit the layer's element type or values is a usage error. Under `auto`, the palettes of P1-00 10.2 are registered for `elevation`, `water`, `river`, `flow_accum`, `flow_dir`, `lake` and `coast_dist`; every other layer stays `gray`. (c) `report` emits format v2 (P1-00 10.3): header `# worldgen report v2`, then `[meta]`, `[tables]`, `[layers]` as in v1, then `[geography]` when an `elevation` layer exists. The expected text of section 7 changes accordingly. (d) In section 9 the `empty-*` cases add `--until none`; P1-00 section 14 X5 lists the geography cases.

## Appendix B. Proposals entries

In the format of `docs/design/proposals.md`, for the lead to add.

### B1. Map size means more land at the same scale

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 (16 m cells, map size a setting), 5 (map size "8, 16 or 32 km")
- Problem: the design does not say whether a larger map shows more land at the same scale or the same land in finer detail, nor which features keep their physical size.
- Proposal: cells are 16 m at every size, so 512, 1024 and 2048 cells are 8.2, 16.4 and 32.8 km. A larger map is more land at the same scale. The continent's outline, its large bays and the layout of mountain belts are specified in fractions of the map, so one continent fills every map; hills, ridges, erosion, rivers, lakes and region sizes are specified in metres and keep their size. State the nominal sizes as 8, 16 and 32 km with 16 m cells.
- Interim choice: as proposed, in `docs/specs/p1-00-geography-overview.md` section 2.2.
- Status: open

### B2. Climate's latitude span is a fraction of the map

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 (Climate: "temperature from latitude"; "scale is compressed")
- Problem: a 16 km map spans no real latitude. A gradient per kilometre would give an 8 km world almost no climate variety and a 32 km world four times as much.
- Proposal: the north-to-south temperature difference is a tuning value per map (a map-fraction quantity), the same at every map size, standing for the larger land the map represents.
- Interim choice: recorded in p1-00 section 2.2 for the climate spec (second pass).
- Status: open

### B3. Generator tuning is data, and shipped tuning changes bump the version

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 10 ("Content rules live in TOML data files under game/data/ and worldgen config"), 4 (generator version)
- Problem: the design says tuning lives in TOML but not where, how it is identified in a world, or whether changing it is a version change.
- Proposal: tuning is a data kind `worldgen` in `game/data/worldgen/`, one definition per stage, every field required, covered by `content_hash`. A change to the shipped data bumps the PATCH version and re-records the shipped-data golden hashes; algorithm golden tests use a frozen fixture copy so tuning work does not churn them.
- Interim choice: as proposed, in p1-00 section 6 (needs P0-03 amendment 16 for kinds with a schema per id).
- Status: open

### B4. Rivers are rasters in P1; a river table later if history needs one

- See P1-02 section 9 (the entry is written there in full).

### B5. Elevation is final after the elevation stage; hydrology does not carve

- See P1-02 section 9.

### B6. Biome categories and the biome table

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 (six starting biomes; special cases for coast, river and high peaks), 6 (`region.biome`)
- Problem: the biome layer needs codes for water cells; the design does not say whether coast, river and peak special cases are separate biomes; a world file does not say what its biome codes mean.
- Proposal: biome definitions carry a fixed integer `code` and a colour. Categories: the six land biomes plus `ocean` and `lake`. River cells take the biome of their land; coast and peak special cases choose among the six. `blighted_waste` is assigned only by history. The biome stage writes a `biome` table (code, key, name, colour, water) into the world (schema version 2).
- Interim choice: as proposed, in p1-00 section 6.6 and 9.2.
- Status: open

### B7. The layer catalogue is part of the schema contract

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6 (schema rules; "a schema reference lives at docs/specs/schema.md")
- Problem: layers are rows of the `layer` table, so the DDL does not change when a layer is added, yet a reader depends on each layer's type and units.
- Proposal: the layer catalogue (name, type, units, meaning) is part of the contract recorded in `schema.md`. Adding a layer needs no schema version bump; changing a catalogued layer does.
- Interim choice: as proposed, in p1-00 section 4 rule 3.
- Status: open

### B8. P1 schema changes (one entry per phase)

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6 (schema growth)
- Problem: the phase's schema changes need one proposals entry (P0-02 5.2 rule 3).
- Proposal: first pass (elevation, hydrology): none; schema stays at version 1. Second pass: version 2 adds table `biome`; version 3 (or the same bump, if in one branch) adds table `region`. This entry is updated when the second-pass specs fix the DDL.
- Interim choice: as proposed, in p1-00 section 9.
- Status: open

## Appendix C. Amendments after the first implementation (2026-11-15)

Decisions by the lead on points raised by the engineers and the determinism auditor. Where these differ from the text above, these win. Items 11 to 16 and 19 have been folded into the body (6.1, 6.3, 7.1, 7.2, 7.3, 7.6 and rule D10, 2026-11-15); they stay here as the record.

**Render (10.2)**

1. **Which layers a palette suits.** `hypsometric`: `i16`. `water` and `direction`: `u8`. `river`: `u8` with every value at most 15. `log2` and `ids`: any unsigned type. `signed`: any signed type. `gray`: any layer. Naming a palette that does not suit the layer is a usage error (exit 2) whose message says what the palette needs. Under `auto`, a registered palette that does not suit the layer falls back to `gray`.
2. **Out-of-range category values.** A `water` or `river` value outside the catalogue's range is drawn (255, 0, 255), so a bad layer is obvious.
3. **Elevation 0.** No finished `elevation` layer contains 0 (catalogue rule 4). On a hand-made layer that does, `hypsometric` draws it as ocean at depth 0, and the composite and the report statistics count it as neither land nor ocean.
4. **`--palette` with `--composite`** is a usage error. The composite needs an `i16` `elevation` layer; without one, or with an optional layer of the wrong type or size, `render` exits 3 and lists the layers present.
5. **Biome palette.** Deferred to the biome stage, with the `biome` table it reads.

**Report (10.3)**

6. **Empty sets.** Percentiles over no values print `n/a` for the key. `ratio4` with a zero denominator prints `n/a`. `coast_ratio` is `n/a` with no land and `0.0000` with land and no ocean. Maxima over nothing are 0.
7. **Ocean in the hydrology statistics** means `elevation` < 0.
8. **`longest_river_m`** is the largest downstream path length D over all river cells (`water` = 1), not only over river sources, so a river that begins at a lake outlet is measured from the outlet. `river_sources` keeps its definition (a river cell with no upstream river or lake cell), so a lake-fed river has no source and the count of sources can be 0 while rivers exist.
9. **Bad layers.** A `flow_dir` that does not lead every non-ocean cell to the ocean (a code above 7 on a non-ocean cell, a step off the map, or a cycle), an `elevation` layer that is not `i16`, or a hydrology layer of the wrong type or size when all five are present makes `report` exit 3, naming the layer and, for a flow fault, the lowest such cell. When any of the five hydrology layers is absent the hydrology lines are omitted and the others are not checked.
10. **`coast_ratio`** is computed in 128-bit arithmetic; the value is as defined.

**Shared algorithms and parallel work (7, 8)**

11. **G1 in place.** A selection helper that works in the caller's array leaves that array in an unspecified arrangement, which differs between standard libraries. After such a call the array may be used only as a multiset. No stage may read positions from it.
12. **G2.** The optional equivalence is "the two cells' keys are equal", given as a key array. BFS parents are stored as the direction code back to the parent, as in G3.
13. **G3 to G5 preconditions.** A non-ocean key below 1, no ocean cell at all, a negative level, or an `order` that is not a topological permutation for G5 is a fatal error.
14. **Thread start failure (D10).** If the system cannot start a thread for any reason, the work is done by the threads that did start, and the result is the same. A thread count outside 1 to 256 given to the library is a fatal error; the command line's default is capped at 256. Parallel calls are not nested.
15. **Fixed-id kinds.** When the kind's directory exists, every id in the set must be defined, whether or not the kind is required; a missing directory is an error only for a required kind, reported once against the directory.
16. **Data root.** An empty `UNMAPPED_DATA_DIR` counts as unset; a relative value resolves against the current directory.
17. **Report and render readings confirmed by the audit.** `largest_basin_share` and `river_mouths` consider non-ocean cells only (a cell with `elevation` ≥ 0 whose downstream cell is ocean); ocean cells never count, whatever their `flow_dir`. In the composite map a cell with `elevation` exactly 0 is drawn as shaded land at height 0, while the `hypsometric` palette draws it as ocean at depth 0; no finished layer contains such a cell. `report` refuses an `elevation` layer of more than 2^24 cells (exit 3), the limit of the shared component algorithm; real maps have at most 2^22.
18. **`land_above_m` with no land** prints each entry as `n/a` (`50:n/a 500:n/a …`). Entries are written `threshold:value` with no space after the colon.
19. **G6 on non-square grids** (tests only) splits by rows of the grid's height. **G1** accepts up to 2^31 values.
20. **Report format after the slope statistics.** The five keys added to `[geography]` in 10.3 (`step_p999_m`, `slope_over`, `slope_p50_above_500m`, `lowland_flat_share`, `edge_parallel_run_m`) are part of format v2; the header does not change. Format v2 has not been used for any recorded golden world, so nothing needs migrating. The format number changes only once a format has shipped in recorded goldens.
21. **Slope statistics, wording corrected after implementation.** "Flatter than 0.5°" is S2 ≤ 7, which allows a change of at most 2 dm over 32 m in one axis and at most 1 dm in the other; 2 dm in both axes gives S2 = 8 and is not flat. `step_p999_m` equals `max_step_m` whenever there are at most 2000 land cells, because the largest step belongs to both of its cells. `slope_over` with no land prints `35:n/a 45:n/a`. The central-difference slope does not see a cell's own height: the crest of a one-cell-wide ridge or an isolated spike has slope 0. The edge-parallel run uses the outermost land of each row or column whatever it is connected to, so an offshore islet farther out than the coast sets that line's value.
