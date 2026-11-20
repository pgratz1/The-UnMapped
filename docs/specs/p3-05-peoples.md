# P3-05 Peoples

Status: **approved by the lead 2026-11-19** (the seven questions of section 19 go to Paul; the interim choices stand). **Revision r6 (2026-11-20, for P3-08, generator 0.17.0; `docs/specs/p3-08-followups.md`)**: the faerie weights of 12.3 ship; dwarves choose high, ore-rich cells (6.2, 8.4); a civilization's last settlement is not abandoned by chance; abandonment names its destination and the blow that began the decline (8.5, new kind 31); refugees above a kind cap drain (9.1); saliences of plague and succession crisis (10.2, 10.3); famine and plague-spread rates halved for the log's volume (10.2, 12.1); invariant 6 read for captured settlements (14); the 100-seed figures of the P3-05 validation (15); see the revision note. Written by the spec writer 2026-11-19; amended 2026-11-19 for P3-06 (r1, r2) and after implementation (r3; notes at the end), note at the end); the amendments of section 18 applied the same day on `p3/05-spec` (P3-00 correction note for P3-05, P3-04 r2, P0-01 section 18, P3-01 r6, P3-07 r1; `schema.md` is the engineer's), after merging `origin/p3/07-spec` at 1cb9d48. Phase P3 History core, spec 5 (P3-00 3.1 row P3-05), a subsystem of pipeline stage 8, `history`. Generator **0.15.0**; **no schema change** (schema stays 7: P3-05 appends event kinds 6–19 to the catalogue and writes rows, never columns; P3-04 8.2). Design doc sections 4 ("Scale is compressed": a city of 20,000; resources and habitability; regions), 5 (the six races and their characters; "Food and homelands"; entities; the tick loop items 1, 2 and 4; quiet intervals; world-creation settings `civs_per_race` and `event_intensity`), 6 (`civilization`, `settlement`), 13 (P3 row) and 14 (records a model can narrate). Depends on P3-00 (binding; amended in 18.2), P0-01 (stream list, 18.3), P3-01 (kind `race`; amended in 18.4: one new field, three field meanings made precise), P3-02 (keyed draws, names), P3-03 (chronology rows), P3-04 (engine, step seam, `State`, event writer, catalogue; amended in 18.2) and P3-07 (report: section 9's statistics, section 20's requirements; amended in 18.5). Branch read: `origin/p3/04-spec` at 4a00615 for P3-00 to P3-04, `origin/p3/07-spec` at 1cb9d48 for P3-07. Proposals read: up to "History: active phases scale with the history span". Reviews read: `docs/reviews/p1-05-resources-critic.md` 7 and `docs/reviews/p1-06-regions-critic.md` 6–7.

Numbers in this spec come from the spec writer's exact-integer prototype `~/.cache/the-unmapped/out/p3-05-spec/peoples.py` (on the P3-02 and P3-03 references for names and chronology and the P3-04 prototype `engine_proto.py` for the schedule, quiet steps and quiet eras), run on the P1-06 validation worlds `~/.cache/the-unmapped/out/p1-06-validation/` (geography unchanged since 0.11.0): 1024 seeds 0–49 (the reference sweep), 512 seeds 0–49, 2048 seeds 0–19, and 1024 seeds 0–49 with each non-default setting of section 15.3. The prototype is evidence, not the reference: the test writer's reference is written from this text (16).

## 1. Purpose

P3-05 gives the history its peoples: where each race awakens (the homeland rule of P3-00 10.1, replacing P3-04's stub S1), how many civilizations each race founds and where they stand apart (10.2), how much people a region supports for a race (10.3), and the life of settlements: growth and decline toward capacity every year (T1) and in quiet steps (Q1), foundings inside a region and into free neighbouring regions (T2, Q4), kinds from hamlet to city and fortresses, capitals, the random pressures plague, famine and succession crisis (T4), abandonment, ruin and its fading (Q3), secession and the end of a civilization by decline. It writes no conflict: claims of held regions, tension, war, sacking and conquest are P3-06's, and section 13 is the seam P3-06 reads. The result of the 0.15.0 merge is a world of peaceful peoples whose history already reads as one: old elven realms, goblin hordes that split, dwarven holds in the hills, faerie courts in the wettest woods, human kingdoms that fill the farmland in their last thousand years, and orcs arriving on the steppe.

## 2. What this spec touches (for the engineer)

| Area | Files |
| --- | --- |
| Subsystem | new `worldgen/include/worldgen/history/peoples.hpp`, `worldgen/src/history/peoples.cpp` (the awakening handler and the subsystem, section 13's view), `worldgen/src/history/peoples_geo.cpp` (scores, capacities, borders, sites: sections 4 to 6) |
| Stage | `worldgen/src/stages/history.cpp`: the shipped `EngineConfig` uses `PeoplesAwakening` and `PeoplesSubsystem` instead of the stubs (P3-04 6.4); the stubs stay as test seams |
| Inputs | `history/inputs.hpp`, `inputs.cpp`: `HistoryInputs` gains `biomes` (rows of table `biome`) and the layers `habitability`, `river`, `flow_dir` (3.2); both constructors and tests R1, R2 extended (P3-04 3.2); r6 (0.17.0): the layers `elevation`, `ore`, `walk_area` (3.2) |
| Catalogue | `event_catalogue.cpp`: kinds 6–19 (11); r6 (0.17.0): kind 31 `settlement_abandoned_to`, after P3-06's kinds 20–30 |
| Streams | `worldgen/include/worldgen/core/streams.hpp`: twelve `hist.peoples.*` names (3.3) |
| Data | new `game/data/history/peoples.toml` and identical copies under `worldgen/tests/data/geo/history/` and `worldgen/tests/data/geo-small/history/`; registration of id `peoples` (kind `history`); `game/data/history/report.toml` and the fixtures: fourteen entries; `game/data/races/*.toml` and the fixture races: field `density_permille` and the value changes of 12.2; the race kind's schema gains `density_permille` (P3-01 amendment, 18.4) |
| Report | `worldgen/src/inspect/report.cpp`: `[history]` gains the keys of 15.4; P3-07's `[stats]` gains them and more (15.4, amendment 18.5) |
| Version | `worldgen/include/worldgen/version.hpp`: **0.15.0** |
| r6 (0.17.0, P3-08) | race files and fixtures: field `cell_weights` (P3-01 r8), the faerie weights of 12.3; `peoples.toml` and fixtures: `plague_major_dead`, `crisis_small_salience` (12.1); `report.toml` and fixtures: kind 31's entry, the templates of kinds 14 and 16 (11); `peoples_geo.cpp`: the cell term (6.2); `peoples.cpp`: 8.4, 8.5, 9.1, 10.2, 10.3 |
| Docs | `docs/specs/schema.md`: the catalogue table gains kinds 6–19 (18.1) |
| Tests | `worldgen/tests/history/` (new CTest group `worldgen-tests-peoples`, pattern `*history_peoples_*`), `tools/worldgen-cli/tests/`; reference `worldgen/tests/reference/p3_peoples_reference.py`; golden `worldgen/tests/golden/p3-05-peoples.txt` |

## 3. Inputs

### 3.1 What the subsystem reads

| Input | Source | Used for |
| --- | --- | --- |
| Region rows | `region` (P1-06): `cells`, `lake_cells`, `walk_cells`, `coast_cells`, `seed_x`, `seed_y`, `x_min` … `y_max`, the means `fertility_mean`, `forage_mean`, `timber_mean`, `fresh_water_mean`, and `ore_reach_cells` | scores (4), capacity (5), sites (6) |
| Biome counts | `region_biome`, with `biome.water` | biome shares (4.1) |
| Borders | `region_border`: `walk_pairs`, `pass_id`, `river_pairs`, `length_m` | the region graph and crossing costs (4.4) |
| Layers | `region`, `water` (P3-04 already), `habitability`, `river`, `flow_dir`; r6: `elevation`, `ore`, `walk_area` | settlement sites, river mouths (6); r6: the cell term of 6.2 and the fortress cell rule of 8.4 |
| Race rows | `race` and `race_biome` (P3-03): ids, `generation_years`, biome weights | scores, crisis rates |
| Race data | kind `race` fields not echoed in the world: `site_weights`, `upland`, `coast`, `growth_permille`, `expansion`, `settlement_kinds`, `density_permille` (new, 18.4), `culture`; r6: `cell_weights` (P3-01 r8) | scores, capacity, growth, foundings, kinds, names; r6: sites (6.2) |
| Settings | `param.civs_per_race`, `param.event_intensity` (P3-01 6, `HistoryParams`) | civilizations per race (7.2), pressure rates (10) |
| Tuning | `history.peoples` (12.1), `history.engine.islet_max_dry_cells` (P3-04 4.1) | everything numeric |

Not read: `pass.col_x`, `col_y` (P3-00 7 locates a pass by its col; P3-05 needs only that a border has one, `pass_id` > 0; P3-06 reads the col for battle sites), `habitability_mean`, `habitability_p90`, `elev_mean_dm`, `ore_mean` (ore under crags is not a mine: P3-00 7 "Mines"; 4.1). Before r6 the `walk_area`, `elevation` and `ore` layers and `elev_max_dm` were not read either (the region's `walk_cells` gives the walkable area; the `habitability` layer carries the pocket and slope penalties); from r6 (0.17.0) the cell term of 6.2 reads `elevation`, `ore` and the region's `elev_max_dm`, and 8.4 reads `walk_area`.

### 3.2 `HistoryInputs` (P3-04 3.3)

Gains, in this order after `water_layer`: `std::vector<db::BiomeRow> biomes` (id order), `db::LayerGrid habitability_layer`, `db::LayerGrid river_layer`, `db::LayerGrid flow_dir_layer`; r6 (0.17.0), after `flow_dir_layer`: `db::LayerGrid elevation_layer` (`i16`), `db::LayerGrid ore_layer` (`u8`), `db::LayerGrid walk_area_layer` (`u16`), each N × N, a wrong type or size fatal with the layer message of 16.0a (r7: it names the layer's own type). Both constructors fill them (from the context's rows and decoded layers; from the world file's `biome` table and layers), R1 compares them, R2's structured binding grows to the new member count. The race data comes from the `ContentSet`, as P3-04 3.1 already does for `settlement_kinds` (the resume path checks the content hash, P3-04 3.2).

### 3.3 Random streams (P3-00 4.6, P3-02 4.1)

Registered in `streams.hpp` after `hist.chronology.awakening`, in this order (P0-01 7.3; 46 names in all, amendment 18.3): `hist.peoples.home`, `hist.peoples.civs`, `hist.peoples.found`, `hist.peoples.outward`, `hist.peoples.famine`, `hist.peoples.famine_toll`, `hist.peoples.plague`, `hist.peoples.plague_toll`, `hist.peoples.plague_spread`, `hist.peoples.abandon`, `hist.peoples.crisis`, `hist.peoples.split`. Every draw is `draw(stream, Y, a, b, k)` with Y the step's year (`StepContext::year()`, the quiet step's start in a quiet step) and (a, b, k) as each rule states. Names are drawn by P3-02's `make_name` on `hist.names.civilization` and `hist.names.settlement`, keyed by the new entity's id, in creation order (P3-04 7.2's registry rule).

Helpers (P3-02 4.1): `chance_pm(u, p)` = `chance(u, Fx raw min(ONE, p × 2^32 div 1000))` for p in per mille; `chance_ppm(u, p)` = `chance(u, Fx raw min(ONE, p × 2^32 div 10^6))` for p in parts per million; `pick(u, n)`.

## 4. The region graph and the region score

All arithmetic `int64`. `div` is floor division (toward −∞; every divisor below is positive); C++ must not use `/` on a possibly negative numerator (P1-00 D2 style: write `floor_div`).

### 4.1 Per-region quantities (once, in `GeoView`)

For region R:

```
dry(R)          = cells − lake_cells
islet(R)        = dry(R) < islet_max_dry_cells                         // P3-04 4.1, decided P3-00 7
upland(R)       = 4 × walk_cells < cells                                // P3-00 7, P1-06 `regions_upland`
land(R)         = Σ region_biome.cells over the rows of R whose biome has biome.water = 0
share_b(R)      = cells_b × 1000 div land(R)        (0 when land(R) = 0) // per mille, per biome code b
rough(R)        = (dry − walk_cells) × 1000 div dry  (0 when dry = 0)    // per mille of dry ground that is not walkable
ore_m(R)        = min(255, ore_reach_cells × 255 div ore_reach_full)    // reachable ore as a 0–255 score
coast_m(R)      = min(1000, coast_cells × 1000 div coast_full)          // per mille
```

`ore_m` replaces `ore_mean` in the site term: rich ore within 300 m of walkable ground is what a mine needs (P3-00 7 "Mines", P1-05 critic F4); `ore_mean` counts crags no one can work. Measured at 1024 (50 worlds): under the literal P3-01 reading (`ore_mean`, a binary upland flag, a binary coast flag) the dwarves' best region had `ore_reach_cells` 0 in 17 of 50 worlds (seed 42: region 21 Central Heath, `ore_mean` 137, 5045 rich cells, none reachable, habitability 2).

### 4.2 Score σ(R, r) of region R for race r

```
S = Σ_k w_k × m_k(R) × 1000 div (255 × Σ_k w_k)        // k = fertility, forage, timber, ore, fresh_water; m_ore = ore_m, others the region means
B = (Σ_b weight_b × share_b(R)) div 1000                // over the race_biome rows of r; a biome without a row weighs 0
U = (upland_r × rough(R)) div 1000
C = (coast_r × coast_m(R)) div 1000
σ = (score_site × S + score_biome × B + score_upland × U + score_coast × C) div 1000
```

S ∈ [0, 1000], B, U, C ∈ [−1000, 1000], σ ∈ [−3000, 4000] with the shipped multipliers (all 1000). The **fit** is q(R, r) = min(1000, max(0, σ)).

**Decisions against P3-01's field texts** (amendment 18.4): `upland` is weighted by the region's rough share rather than applied on the 4 × `walk_cells` < `cells` flag, and `coast` by the coast share rather than on `coast_cells` > 0. Reasons: 53% of non-islet regions at 1024 touch the sea (median of 50 worlds), so a flag cannot tell a coastal region from one that touches the shore at a corner; and with a flag the dwarves' 800 outweighed everything else, so the binary upland term sent them to bare alpine regions with no reachable ore (the 17 of 50 above). Graded, the dwarves land in uplands with mines (15.1). The upland *flag* is still used for fortresses (8.4), and the report's `, upland` (P3-07 6.6) keeps the flag.

### 4.3 Measured scores

1024, 50 worlds, shipped data with the changes of 12.2, the best non-islet region of each race (medians over worlds):

| Race | σ | S | B | U | C | median region C | best region C (median, max) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| elves | 1241 | 749 | 497 | 0 | 0 | 2,480 | 9,399, 17,221 |
| goblins | 663 | 592 | 51 | 24 | 0 | 4,720 | 16,957, 27,364 |
| dwarves | 1607 | 685 | 212 | 732 | 0 | 3,100 | 24,540, 36,002 |
| faeries | 844 | 649 | 189 | 0 | 0 | 2,004 | 6,111, 11,454 |
| humans | 1043 | 674 | 251 | 0 | 137 | 9,039 | 28,524, 40,615 |
| orcs | 793 | 566 | 220 | 24 | 0 | 5,138 | 17,687, 27,497 |

Elves, dwarves and humans saturate the fit (q = 1000) on their best land; goblins and orcs, whose preferences are broad and whose best land is middling, have q about 650–800. The dwarves' best region is an upland with mines (U 732, the ore part of S full); its capacity is large because rough ground counts at 630‰ for them, but the dwarves' reach (1 hop) and their late awakening keep them to a few holds (15.3).

### 4.4 Borders and crossing cost

A border row (a, b) is **open** if `walk_pairs` > 0 or `pass_id` > 0; otherwise it is a barrier (P3-00 7, 10.4). Lowland borders are free political lines (decided). Cost of an open border:

```
cost = 1                         if walk_pairs > 0
     = pass_cost                 otherwise (a pass, walkable or a high col)
cost += river_cost               if river_pairs × 16 × 1000 ≥ river_border_share_permille × length_m and length_m > 0   // P3-00 7, decided
```

`neighbours(R)` = the regions joined to R by an open border, ascending id, each with its cost. `hops(R0)` = breadth-first distances from R0 over open borders (neighbours in ascending id; unreachable regions have no distance, treated as +∞). Costs are integers; P3-06 adds its own army costs on the same rows.

## 5. Capacity

### 5.1 Region capacity C(R, r)

```
rough_r = rough_base + max(0, upland_r) × rough_upland div 1000          // how much of rough ground race r can live on, per mille
area    = walk_cells + (dry − walk_cells) × rough_r div 1000
C(R, r) = max(capacity_floor, area × q(R, r) × density_r × people_per_kcell div 10^9)
```

P3-00 10.3's linear function of score and dry area: walkable ground counts in full, rough ground at `rough_r` (dwarves 630‰, everyone else with `upland` ≤ 0 at 150‰, faeries 270‰), so dwarven and faerie homelands do not need `walk_area` (P1-05 critic 7). `density_r` is the new race field `density_permille` (18.4): "few in number" and "numerous" are the design's (5), and no other field can carry them (lifespans are about generations, growth about speed, not about how many live on a square kilometre). Shipped: elves 300, goblins 900, dwarves 700, faeries 250, humans 1000, orcs 800.

**Calibration** (design 4: "a city of 20,000"): the human region of largest capacity in a 1024 world has a median of 13,200 walkable cells and C 28,500 (largest seen 40,600) with `people_per_kcell` 2500; its capital, holding half or more of the region (5.2), is a city of 12,000–25,000 (`city_max`). A median region holds 2,000–2,500 for elves and faeries, 3,100 for dwarves, 4,700–5,100 for goblins and orcs and 9,000 for humans (table of 4.3). Bounds: area < 2^17, q ≤ 1000, density ≤ 1000, `people_per_kcell` ≤ 10^5, so the product < 2^57 and C < 2^31 (fatal guard otherwise).

### 5.2 Shares among the settlements of a region

The standing settlements (status 1 or 2) of region R share the region: settlement s has the **share**

```
w_s   = capital_weight if s is its owner's capital, else 1000
C_s   = max(1, C(R, race of owner(s)) × w_s div Σ_{t standing in R} w_t)
```

With the shipped `capital_weight` 3000 and the slot limit of four (6.3), a capital holds half to all of its region and each other settlement a sixth to a quarter, so capitals are the towns and cities and their satellites villages (seed 42: 15.2). Settlements of different owners in one region (a fallback home, 7.1; P3-06's conquests) each take their own race's capacity in the formula; the weights are shared.

**Kind cap.** A settlement does not grow above `kind_cap(s)`: for a fortress `fortress_max`; otherwise the top of the highest ladder kind the race builds (8.1): hamlet → `village_min` − 1, village → `town_min` − 1, town → `city_min` − 1, city → `city_max`. So faerie villages stop at 1,499 and goblin and orc towns at 7,999 (design: the races without cities spread wider instead, 9.1).

## 6. Settlement sites

### 6.1 River mouths and ports (P3-00 7, decided: ports come from history)

A **mouth cell** is a cell with `river` ≥ `mouth_class_min` and `water` = 1 whose `flow_dir` (D8, P1-00 2.3) points to an in-grid cell with `water` = 3. A cell is **near a mouth** (**near the sea**) if a mouth cell (a `water` = 3 cell) lies within Chebyshev distance `port_radius`, inside the grid. The port bonus of a cell for race r:

```
port(c, r) = (port_mouth_bonus if near a mouth else port_coast_bonus if near the sea else 0) × (1000 + coast_r) div 1000
```

### 6.2 Choosing a site in region R for race r

Candidates: cells c with `region`(c) = R and `water`(c) = 0. A candidate is **free** if every settlement with status 1, 2 or 3 (standing or ruined, not faded) lies at squared Euclidean distance ≥ `site_spacing`² from it (settlements of any region, including those created earlier in the same step: the spacing is about what stands, not about which entities a step may process). Then:

1. the region's seed cell (`seed_x`, `seed_y`) if it is a candidate and free (P3-00 7: a region's first settlement stands at its seed); **r6: only for a race whose `cell_weights` are both 0** (a race with a cell term starts at item 2);
2. otherwise the free candidate with the largest **value** v(c, r), ties to the smaller cell index y × N + x, where (r6)

   ```
   hgt(c)  = min(255, max(0, elevation(c)) × 255 div max(1, elev_max_dm(R)))     // the cell's height in its region, 0–255
   v(c, r) = habitability(c) + port(c, r) + (cell_ore_r × ore(c) + cell_height_r × hgt(c)) div 1000
   ```

   with `cell_ore_r`, `cell_height_r` the race's `cell_weights` (P3-01 r8; shipped dwarf 400 and 600, every other race 0 and 0, so for them v is the r5 value `habitability`(c) + port(c, r) exactly); `elevation` in decimetres (land cells ≥ 1), `ore` the P1-05 score (0–255), `elev_max_dm` the region row's largest dry-cell elevation (P1-06 3.2), all non-negative, so the division is a plain floor; the products are below 2^20;
3. otherwise, only where 7.3 says "relaxed": the candidate with the largest value of item 2 on which no settlement of any status stands, same tie rule;
4. otherwise no site (fatal for an awakening, `history: peoples: no settlement site in region <id>`, unreachable for a non-islet region).

Faded settlements (status 4) do not block: their ground has gone back to the land (8.6).

**Why dwarves have a cell term** (r6; `docs/reviews/p3-05-peoples-world-critic.md` F1, `docs/reviews/p3-06-conflict-world-critic.md` section 5: "more urgent"). The region's seed cell (RG4 of P1-06) is its most habitable and lowest site, and item 2 ranked the rest by habitability too, so every race, dwarves included, settled the lowest farmland of its region: the median elevation rank of a dwarven settlement within its region (the share of the region's dry cells lower than it) was 0.21 at 0.16.0 (1024 seeds 0–49, 350 dwarven settlements founded), against 0.12 (faeries) to 0.21 (goblins) for the other races, and dwarven forest regions grew dwarven cities. With the cell term the dwarves take the high, ore-rich ground of their regions: rank 0.67, settlements on a rich-ore cell (`ore` ≥ 128) 59% against 16%, standing dwarven cities 7 against 14 in the 50 worlds, standing settlements and wars unchanged (P3-08 6.2 has the table). Regions with no high ground keep their lowland sites (habitability still counts, and a coast or river mouth adds up to 48 for dwarves).

### 6.3 Slots

A founding *inside* region R needs fewer than `region_slots_max` standing settlements in R (any owner). Expansion into a free region needs only a site.

## 7. Awakening (replaces stubs S1 and S2; P3-04 6.3, 6.4)

The handler computes the race's homes once in `awakening_place` (no state change) and places them in `awaken`; the awakening event's region is the first home.

### 7.1 The homeland rule (P3-00 10.1)

The **order** of region R for race r at year Y: (σ(R, r) descending, `draw(hist.peoples.home, Y, r, R, 0)` ascending, R ascending): the score decides, the keyed draw breaks exact ties without favouring low ids, and the id ends the order (H5). Eligible: non-islet regions with `RegionState.owner` = 0 and C(R, r) ≥ `home_min_capacity`. Civilization 1 of the race takes the first eligible region in order; if none is eligible, the first free non-islet region; if none, the first non-islet region (held or not); if there is none, the first region (P3-00 10.1: the awakening happens on every world; with a held home the new civilization holds no region until it expands, 9). The capacity condition is a decision against P3-00 10.1's wording (amendment 18.2 item 8): a homeland must feed a people. Without it the elves of an elf-only world (1024 seed 49) awoke on West Isle II, a forest islet of σ 1236 and capacity 974, and never had more than two settlements.

### 7.2 Civilizations per race (P3-00 10.2)

Let `alive` = the civilizations not ended at the awakening and `free` = the number of eligible regions of 7.1.

- `auto`: n = 1 + [chance_pm(draw(hist.peoples.civs, Y, r, 0, 0), `civs_two_permille`)]; then n = 1 if `free` < `regions_per_civ` × `alive` + 2.
- 1 … 4: n = the setting.

Civilization j ≥ 2 takes the first region in the order of 7.1 that is eligible, not already a home of this awakening, and at `hops` ≥ `home_min_hops` from every earlier home of this awakening (unreachable counts as far: a second elven realm across a range or on an island is apart). When no region qualifies, civilization j and the later ones are not founded: a race never has fewer than one civilization and never more than the land allows. The count is not otherwise reduced for explicit settings (Paul's "1 to 4" is a request, not a guarantee; question 3).

### 7.3 Placing a civilization (one step, P3-04 6.3 (d))

For each home H in order, with c the next civilization id and s the next settlement id:

1. name (P3-02, kind civilization, entity c); append `civ_founded` (region H, participants c as `civilization`, r as `race`, cause the awakening event; as the stub, P3-04 7.2); `found_civilization`; if H is free, `set_region_owner(H, c, civ_founded)`.
2. site: 6.2 relaxed.
3. name (settlement, entity s); kind by 8.4; append `settlement_founded` (region H, settlement s, participants s as `settlement`, c as `founder`, value `population` = `founding_population`, cause `civ_founded`); `found_settlement` with status thriving and that population; `set_capital(c, s)`.

## 8. Settlements

### 8.1 Kinds

The ladder hamlet < village < town < city with thresholds `village_min`, `town_min`, `city_min`; a race builds only the kinds in its `settlement_kinds` (P3-01). `ladder(r, P)` = the highest kind k the race builds with min(k) ≤ P (hamlet's min is 0); if the race builds none of the kinds at or below P, its lowest ladder kind. A race with no ladder kind (only `fortress`) builds every settlement as a fortress.

### 8.2 Kind changes (end of T1; Q1, 9.2)

For each standing visible settlement in ascending id that is not a fortress: k = its kind, k' = `ladder(r, P)`.

- k' above k: the kind becomes k'; append `became_<k'>` (`became_village`, `became_town` or `became_city`; settlement s; participants s as `settlement`, owner as `civilization`; value `population` = P; cause the settlement's founding event).
- k' below k and P × 1000 < min(k) × `kind_shrink_permille`: the kind becomes k'; append `settlement_dwindled` (same roles; value `population`; cause the settlement's last reduction event (10.4) if any, else none).
- otherwise nothing (the hysteresis keeps a town of 1,490 a town until it falls below 1,200).

### 8.3 Status

Thriving (1) at founding. Declining (2) when a pressure strikes it (10.4), recording `before` = its population just before the blow if it was thriving. Back to thriving in T1 or Q1 once P × 1000 ≥ `recover_permille` × `before` (no event: the census row records it, P3-04 6.7). Ruined (3) by abandonment (8.5) or by P3-06. Faded (4) by Q3 (8.6).

### 8.4 Fortresses

A settlement founded by a race whose `settlement_kinds` include `fortress` in an **upland** region (the flag of 4.1), **or (r6) by such a race with a cell term (`cell_weights` not both 0) on a cell off the walkable ground (`walk_area`(c) = 0: a hill, crag or upland cell)**, is a fortress for good: its kind never changes and its cap is `fortress_max`. So the dwarves' holds in the hills are fortresses, and so are goblin, orcish and human settlements in uplands. The cell rule (r6, P3-05 critic F1 "make a dwarven settlement on a rough or upland cell a fortress, not only one in an upland region") applies with the shipped data to dwarves only: dwarven fortresses rose from 74% to 81% of dwarven foundings (1024 seeds 0–49), and dwarven settlements on unwalkable cells from 159 to 253 of 350 to 377. In a world whose `walk_area` is 0 on every cell (synthetic test worlds without a walk-area layer computed) every dwarven founding is therefore a fortress; that is the rule, not a defect (r7). Fortresses at passes and borders built for war are P3-06's (it may found them with its own kind).

### 8.5 Abandonment

A standing settlement s owned by civilization c is abandoned (a) in T4 right after a pressure that leaves P < `abandon_min`, or, when s was already declining before that blow and P < `abandon_frail_max`, with chance_pm(draw(hist.peoples.abandon, Y, s, 0, 0), `abandon_permille[kind]`), **unless c has no other standing settlement** (r6: the chance abandonment never takes a civilization's last settlement; "a people with nowhere to go does not walk away from its only home", `docs/reviews/p3-05-peoples-world-critic.md` question (i); the settlements created earlier in the step count, as in item 2; no draw is made then); (b) in T1 and Q1, when P < `abandon_min` (after growth: P3-06's casualties of the year before land here), with cause its last reduction event, else its founding event. The deterministic rules (P < `abandon_min` in (a) and (b)) apply to a last settlement as to any other: a realm can still die of a blow that leaves too few to go on.

**Causes** (r6; the critic's F4, "cite both the last blow and the blow that began the decline"): the abandonment's causes are, ascending without repeats, the event named above (the blow of (a), or for (b) the last reduction event, else the founding event) and, when s is declining, its **decline start**: the event that last turned it from thriving to declining (a strike of 10.4 or a crisis loss of 10.3 that found it thriving; P3-05 keeps it per settlement id with `before`, 8.3). P3-06's casualties do not change a status and are never a decline start (they are last reduction events, 13).

The procedure, in this order:

1. **The destination** (r6; the critic's F4, "name the destination"): if c has another standing settlement, t = the nearest one (squared distance of cells, then id), room = max(0, 2 × C(R_t, r) − P_t) and settled = min(P, room); otherwise settled = 0. If settled ≥ 1: append **`settlement_abandoned_to`** (kind 31; region and settlement s; participants s as `settlement`, c as `civilization`, t as `destination`; values `settled`, `lost` = P − settled; causes as above). Otherwise append `settlement_abandoned` (kind 16; region and settlement s; participants s as `settlement`, c as `civilization`; value `survivors` = P; causes as above): its survivors had nowhere to go. Before r6 (generator 0.15.0 to 0.16.0) every abandonment was kind 16, with the first cause only.
2. the survivors join t up to its room: t gains settled (item 1); the rest are lost. No other settlement: they are lost;
3. `set_population(s, 0)`, `set_settlement_status(s, ruined, abandoned)` (P3-04: `ruined_year` = Y, `ruin_event` = the abandonment, the row's ruin event required by P3-07 20 item 3), `set_settlement_owner(s, 0, abandoned)` (P3-04 9.2: `owner_civ` 0 when abandoned);
4. if c holds R and has no other standing settlement in R: `set_region_owner(R, 0, abandoned)`;
5. if s was c's capital and c has another standing settlement: the most populous (then smaller id) becomes capital, append `capital_moved` (region and settlement of the new capital; participants c as `civilization`, the new capital as `settlement`; cause the abandonment), `set_capital`;
6. if c has no standing settlement left: append `civ_ended` (region R and settlement s, the abandoned settlement (r3); participant c as `civilization`; cause the abandonment), `end_civilization(c, civ_ended)`, and every region c still holds is freed with that event (P3-00 10.5 amended, 18.2: a people with no settlement cannot hold land).

**The ruin procedure** `ruin(s, e)` (r1, for P3-06; P3-06 18.1) is items 2–5 above with the event e in place of the abandonment (item 2's t and room computed as in item 1): the survivors are s's population P at the call; `set_settlement_status(s, ruined, e)` makes e the `ruin_event`; the region is released and `capital_moved` appended with cause e. It does **not** apply item 6 (its caller ends the civilization if it has no standing settlement left). **Abandonment** is: item 1 (append `settlement_abandoned_to` or `settlement_abandoned`), then `ruin(s, that event)`, then item 6. This is a restructuring only: a world of this spec is byte for byte what it was.

"Other standing settlements" in items 2, 5 and 6 include those created earlier in the same step: they exist, and the procedure is the owner's own bookkeeping, not a step processing them (P3-04 6.2 allows the creator's follow-up; this is the one place P3-05 reads a new entity, and only as a destination).

In a quiet step an abandonment needs `quiet_events_left(c)` ≥ the number of events it will append (1, plus 1 for item 5, plus 1 for item 6); otherwise it waits for the next step (P3-04 W9 is never violated). Either kind of item 1 is one event.

### 8.6 Ruins fade (Q3)

In each quiet step, for each settlement in ascending id with status ruined and Y − `ruined_year` ≥ `ruin_fade_years`: if `quiet_events_left(0)` > 0, `set_settlement_status(s, faded, e)` with e = append `ruin_faded` (region and settlement s; participant s as `settlement`; cause its `ruin_event`); otherwise stop for this step (the rest wait). Ruins fade only in quiet steps: a ruin made in the final phase is recent and stays a ruin; P6 turns ruins into sites.

## 9. Growth and foundings

### 9.1 T1: growth and decline

First, each alive civilization (ascending id) with no standing settlement, which only P3-06 can leave behind, ends (only here, in T1; r3): append `civ_ended` (region and settlement: those of its settlement ruined last (r3); cause: the `ruin_event` of its settlement ruined last, ties to the larger id), `end_civilization`, its regions freed with that event (13). Then the shares C_s of 5.2 are computed for every standing settlement (one snapshot), then for each standing visible settlement in ascending id, with P its population and g the owner race's `growth_permille`:

```
T_s = min(C_s, kind_cap(s))                                                  // r6: the target
if P < T_s:  d = (P × g × (C_s − P) + 500 × C_s) div (1000 × C_s);  if g > 0: d = max(1, d)
             P' = min(P + d, kind_cap(s))
else if P > T_s:  P' = P − max(1, (P − T_s) × decline_permille div 1000)
else:             P' = P
```

(r6; the critic's question (ii) and the P3-05 validation's observation 3.) Before r6 the rule was `if P < C_s: … P' = min(P + d, max(P, kind_cap(s)))`, `else if P > C_s: P' = P − max(1, (P − C_s) × decline_permille div 1000)`: a settlement above its kind cap but below its share (a faerie village of 2,277 after taking in refugees, a capital's share being above 1,499) never drained. With T_s the excess over the cap drains at `decline_permille` a year like any excess over the share. For P ≤ kind_cap(s) the two rules are equal (P < C_s and P < cap gives the same d and min(P + d, cap); P ≥ C_s gives the same decline), so the rule changes only settlements above their cap: refugees (8.5 item 2), colonists and P3-06's captures and cessions, whose new owner's race may build a lower top kind (a goblin owner of a captured human city drains it toward 7,999, the goblin town cap; its kind changes by 8.2's hysteresis as before). At 0.16.0, 6 standing settlements of 1024 seeds 0–49 ended above their cap; with r6 2 do (each drains within a few decades of its last refugee intake).

each followed by its status recovery (8.3); then the abandonments of 8.5 (b), then the kind changes of 8.2. Integer logistic growth at the race's rate: humans (15‰) double in about 50 years from small, elves (3‰) in about 230; the `max(1, …)` keeps a hamlet of elves from stalling at rounding. Above its share a settlement loses `decline_permille` of the excess a year (a share shrinks when a new settlement joins the region, or a capital moves). `set_population` for every changed value.

### 9.2 Q1: drift in a quiet step (amends P3-00 4.4)

Q1 of a quiet step of L years is the T1 growth of 9.1 with 8.3's recovery applied L times, year by year (shares recomputed each time; no pressures, no events between; r3: recovery is part of each year, as in T1), then 8.5 (b) and 8.2 once at the end, settlements in ascending id, each event charged to the owner (W9; a kind change or an abandonment without budget waits). **Decision**: P3-00 4.4 asks for a closed form with "no per-year loop". No closed form of an integer logistic exists that equals the yearly rule, and two formulas (one for ticks, one for quiet steps) would make the same land support different populations depending on the phase. The L-fold iterate is exact, makes a quiet step equal to L tick years without events (a property test, 16.2), and costs L × settlements integer steps: 25 × 150 × about 200 steps ≈ 750,000 at 1024, about 10 ms.

### 9.3 T2 and Q4: foundings

Per civilization c alive and visible, in ascending id; at most one founding per civilization per step:

1. **Pressure** of a standing settlement s: p(s) = max(P × 1000 div C_s, P × 1000 div kind_cap(s)) (shares from a snapshot at the step's start). *Ready* = c's standing visible settlements with p ≥ `found_pressure_permille`. None ready: nothing.
2. **Source** among the ready: the key (not c's capital, p, P, −id) largest: colonists leave a crowded satellite before the capital, so capitals keep their size.
3. **Targets.** *Inside*: if c holds the source's region R and 6.3 allows another settlement there, the site of 6.2 in R (not relaxed). *Outward*: over every region R0 that c holds and every R1 in `neighbours(R0)` with cost k: R1 free, not an islet, `hops`(c's capital region)(R1) ≤ `reach_r` = `reach_base` + `expansion_r` × `reach_per_expansion` div 1000, σ(R1, r) ≥ σ_min = max(`expand_min_score`, σ(home, r) × `expand_relative_permille` div 1000), C(R1, r) ≥ `expand_min_capacity`; the R1 with the largest σ(R1, r) − `cost_penalty` × k (k the smallest cost over c's held neighbours of R1), ties to the smaller id; its site by 6.2 (not relaxed; none: no outward target).
4. **Land hunger** (13): no target: `hunger(c)` += the step's years; nothing else. A target: `hunger(c)` = 0.
5. **Chance**: p = `found_rate_permille` × `expansion_r` div 1000 × years (years = 1 in a tick, L in a quiet step), × `quiet_found_permille` div 1000 in every quiet step, whatever its length L (r3); founding if chance_pm(draw(hist.peoples.found, Y, c, 0, 0), min(1000, p)).
6. In a quiet step, `quiet_events_left(c)` = 0: nothing.
7. **Colonists**: n = max(`colonists_min`, P_source × `colonist_permille` div 1000); if P_source − n < 2 × `colonists_min`: nothing.
8. **Choice**: both targets: outward if chance_pm(draw(hist.peoples.outward, Y, c, 0, 0), `expansion_r`), else inside; one target: that one.
9. **Found**: n = min(n, C(target region, r)); the source loses n; name (settlement, next id s); kind by 8.4 (fortress) or `ladder(r, n)`; append `colony_founded` (region and settlement s; participants s as `settlement`, c as `founder`, the source as `origin`; value `population` = n; no cause; **salience** `colony_new_region_salience` if the region was free before the founding, else the base salience); `found_settlement` (thriving, n); if the region is free, `set_region_owner(R1, c, colony_founded)`.

**Reach** (decision; question 2). Without a limit the oldest peoples fill every region they like long before the next race awakens (first prototype, seed 42: at the present the elves held 19 of 50 regions and the goblins, in 13 civilizations, 27; the humans awoke in the one free region, an alpine one with σ −138). A civilization governs only so far from its seat: elves, dwarves and faeries 1 hop (`expansion` 100–250), goblins and orcs 2, humans 3. Splits (10.3) are how a people spreads beyond its reach. With the race-relative threshold σ_min each people expands only into land like its home (elves into forest, humans into farmland).

## 10. Pressures (T4)

### 10.1 Rates

Every rate is scaled by the event intensity I = `intensity.calm` / `normal` / `turbulent` (shipped 500, 1000, 2000 per mille) for `param.event_intensity`: rate × I div 1000. Spread chances and tolls are not scaled.

### 10.2 Famine and plague

For each standing visible settlement s in ascending id, with C_s from the snapshot at the step's start and P its current population (a blow earlier in the step counts), crowding k = P × 1000 div C_s:

1. **Famine**: if k ≥ `famine_crowding_permille` and chance_ppm(draw(hist.peoples.famine, Y, s, 0, 0), `famine_ppm` × I div 1000): toll t = `famine_toll_min` + pick(draw(hist.peoples.famine_toll, Y, s, 0, 0), `famine_toll_max` − `famine_toll_min` + 1) per mille; **strike** s with kind `famine`, toll t, no cause. Add s to the step's struck set.
2. If s is still standing, **plague**: chance_ppm(draw(hist.peoples.plague, Y, s, 0, 0), `plague_ppm[kind(s)]` × I div 1000): toll t = `plague_toll_min` + pick(draw(hist.peoples.plague_toll, Y, s, 0, 0), range + 1); strike s with kind `plague` (the origin event o; its salience is `plague_small_salience` when s is a hamlet or village, else the base; **r6: its salience is the outbreak's toll rule below**); add s to the struck set. Then **spread**, unless the outbreak's strike abandoned s (8.5 (a): an outbreak that empties s spreads nowhere; r3), for each settlement u in ascending id, u ≠ s, not struck this step, standing, visible, owned by s's owner (read after the outbreak's strike), in s's region or a region of `neighbours`(s's region): if chance_pm(draw(hist.peoples.plague_spread, Y, s, u, 0), `plague_spread_permille`): toll (`plague_toll_min` + pick(draw(hist.peoples.plague_toll, Y, u, 1, 0), range + 1)) × `plague_spread_toll_permille` div 1000; strike u with kind `plague_spread`, cause o; add u to the struck set.

**Log volume** (r6; `docs/reviews/p3-05-peoples-world-critic.md` F2: famine, plague, spread and quiet-era lines were about 60% of the log at 0.15.0 and 44% at 0.16.0). `famine_ppm` is halved to 1250 and `plague_spread_permille` to 100 (data, 12.1). A famine rule was considered and rejected: a famine rarely strikes the same settlement twice (the median gap between two famines of one settlement was 588 years at 0.16.0, 1024 seeds 0–49), so a rest period, or one event per region and year, would remove few lines; the volume comes from many settlements at their share, which the rate sets. Measured (P3-08 6.4): each halving alone removes about half of its kind's lines (famines 97.5 → 48, spreads 108 → 52 per world at the median) with every race's standing settlements and population within the run-to-run spread of the variants.

A settlement struck by spread earlier in the loop still makes its own draws when its turn comes (draws are keyed; the struck set only stops a second spread to it).

**The outbreak's salience by its toll** (r6; `docs/reviews/p3-05-peoples-world-critic.md` F3: plagues were 40–46% of the summary, most of them small). The outbreak is **planned before it is written**: with P the population of s, dead_0 = P × t div 1000; s is *emptied* if P − dead_0 < `abandon_min`, or if s is declining, P − dead_0 < `abandon_frail_max`, c has another standing settlement and chance_pm(draw(hist.peoples.abandon, Y, s, 0, 0), `abandon_permille[kind(s)]`) (8.5 (a) as amended; the same keyed draw the strike makes, so the plan and the strike agree); the planned spreads are, if s is not emptied, the targets u of the spread rule above whose spread draw succeeds; and **total** = dead_0 + Σ dead_u, where dead_u is the toll the spread's strike of u will take. **The plan is a replay** (r7, ruling of the lead after the implementation): the strikes of the outbreak and of its spreads are carried out in their order on a copy of the populations and statuses, with every effect a strike has on another settlement included, in particular an abandonment of the struck settlement (8.5 (a)) whose survivors join its owner's nearest standing settlement (8.5 item 2), which may be the origin or a later spread target, so that its P_u at its own strike is the replayed one; it uses the same keyed draws as the strikes (`plague_spread`, `plague_toll`, `abandon`), appends no event and changes no state. dead_u is then exactly the `dead` of the real strike of u, so the planned total equals the real one and invariant 13 holds. (r6 said that no state changes between the plan and the strikes; that is false whenever a spread target is abandoned and its survivors move.) The outbreak's event o has the kind's base salience (45, `history.report`) if total ≥ `plague_major_dead` (1000), else `plague_small_salience` (25). The events are then appended exactly as before (the outbreak's strike, then each planned spread in ascending u); the plan only fixes o's salience, which W8 requires at append (P3-04 6.6). Before r6 the salience was 25 in a hamlet or village and 45 elsewhere, whatever the toll: at 0.16.0 (1024 seeds 0–99) 4,655 of the 5,607 plagues of salience 45 killed fewer than 1,000 in their own settlement. Measured on those worlds' stored logs (`~/.cache/the-unmapped/out/p3-08-spec/an/sal.py`): with the r6 rule plagues are 121‰ of the summary at the median (205‰ before), crises 1 per summary (10), the threshold unchanged (26 / 31 / 51).

### 10.3 Succession crisis

For each civilization c alive and visible in ascending id, with r its race: rate (ppm per year) = `crisis_per_generation_permille` × 1000 div `generation_years_r` × I div 1000 (a crisis comes about once in 1/0.06 ≈ 17 generations: goblins every 250 years, elves every 2,500). If chance_ppm(draw(hist.peoples.crisis, Y, c, 0, 0), rate): append `succession_crisis` (region and settlement = c's capital; participant c as `civilization`; no cause), the event e. **r6: the split test of the next item is made first** (its conditions and its draw do not depend on e), and e's salience is the kind's base (40) if the crisis splits the realm, else `crisis_small_salience` (25): a crisis that changes nothing visible does not rank with a city or a secession (the critic's F3; 6 to 17 crises per default summary at 0.15.0, a median of 10 at 0.16.0, 1 with r6). Then:

- **Split** if c has ≥ `split_min_settlements` standing visible settlements, holds ≥ `split_min_regions` regions and chance_pm(draw(hist.peoples.split, Y, c, 0, 0), `split_permille`): d_cap = hop distances from the capital's region over open borders **within c's held regions**; the far region F = the held region with the largest (d_cap (+∞ when unreachable), population of c in it, −id); d_F likewise from F; the seceding part = the held regions with d_F < d_cap (+∞ = +∞ is not less). If the part holds at least one and not all of c's standing settlements: a new civilization c' of race r (name: civilization kind, entity c'), capital the part's most populous settlement (then smaller id); append `civ_seceded` (region and settlement = the new capital; participants c' as `civilization`, c as `from`; value `settlements` = the number of settlements that leave; cause e) — this is c''s founding event (`founding_event`, 18.2); `found_civilization` (home = the new capital's region; event before entity, in the order of P3-04 7.2's `civ_founded`); `set_capital`; `set_settlement_owner` of each of the part's settlements and `set_region_owner` of each of the part's regions to c' with that event.
- **Otherwise** (no split): the capital loses P × `crisis_loss_permille` div 1000 and becomes declining (8.3; `before` = P if it was thriving; the crisis is the capital's last reduction event, 10.4). The loss is not a strike: no 8.5 (a) check follows (r3).

### 10.4 Strike

strike(s, kind, t, causes): dead = P × t div 1000; if s is thriving, `before` = P; P −= dead (`set_population`); append the event (region and settlement s; participants s as `settlement`, the owner as `civilization`; values `dead`, `population` = the new P; causes); status declining; s's **last reduction event** = this event; then 8.5 (a). A settlement struck more than once in one T4 while already declining (its own famine and then its own plague, or a spread and then its own outbreak) draws `abandon` with the same key (s, 0) each time, so the later chances repeat the first outcome: in effect one abandonment chance per settlement per year (as specified; noted by the P3-05 audit, N6; r4).

### 10.5 Quiet steps

No pressure acts in a quiet step: the quiet interval is the design's peace ("populations drift toward what the land supports"). The quiet-event budget (W9, `quiet_events_max` 3 per charged civilization) is used by foundings (one per civilization at most), kind changes, abandonments of 9.2 and the fading of ruins (charged to civilization 0). Every emitter checks the budget before it changes state, so W9 holds by construction; a kind change, abandonment, founding or fading that finds no budget is deferred to the next step, not lost (the prototype asserts the budget on every append).

## 11. Event kinds (appended to P3-04 8.2's catalogue)

Ids continue P3-04's; roles and values in `seq` order; entity types as P3-04 6.1 (`settlement` 4, `civilization` 3); every count 1–1. `region_use` and `settlement_use` 1 for every kind below. Base salience is data (`history.report`), shipped as listed; templates as listed (P3-04 8.4 grammar).

| id | key | caused | roles | values | salience | template |
| --- | --- | --- | --- | --- | --- | --- |
| 6 | `colony_founded` | 0 | 1 `settlement`, 2 `founder` (civ), 3 `origin` (settlement) | `population` | 15 (30 into a free region, 9.3) | `{founder} sent settlers from {origin} to found {settlement} in {@region}.` |
| 7 | `became_village` | 1 | 1 `settlement`, 2 `civilization` | `population` | 10 | `{settlement} grew into a village of {population}.` |
| 8 | `became_town` | 1 | same | `population` | 18 | `{settlement} grew into a town of {population}.` |
| 9 | `became_city` | 1 | same | `population` | 45 | `{settlement} grew into a city of {population}.` |
| 10 | `settlement_dwindled` | 0 | same | `population` | 15 | `{settlement} dwindled to {population} souls.` |
| 11 | `famine` | 0 | 1 `settlement`, 2 `civilization` | `dead`, `population` | 25 | `Famine struck {settlement}; {dead} died.` |
| 12 | `plague` | 0 | same | `dead`, `population` | 45 (r6: 25 when the outbreak and its spreads killed fewer than 1,000; before r6: 25 in a hamlet or village), 10.2 | `Plague broke out in {settlement}; {dead} died.` |
| 13 | `plague_spread` | 1 | same | `dead`, `population` | 25 | `The plague spread to {settlement}; {dead} died.` |
| 14 | `succession_crisis` | 0 | 1 `civilization` | — | 40 (25 when it does not split, r6) | `A succession crisis at {@settlement} shook {civilization}.` (r6; before: `A succession crisis shook {civilization}.`) |
| 15 | `civ_seceded` | 1 | 1 `civilization`, 2 `from` (civ) | `settlements` | 70 | `{civilization} broke away from {from}, taking {settlements} {settlements?settlement\|settlements} with it.` |
| 16 | `settlement_abandoned` | 1 | 1 `settlement`, 2 `civilization` | `survivors` | 55 | `{settlement} was abandoned; {survivors?no one survived\|its one survivor was scattered\|its # survivors were scattered}.` (r6: kind 16 is used only when no survivor reaches another settlement, 8.5 item 1; before: `{settlement} was abandoned; {survivors} {survivors?survivor\|survivors} left.`) |
| 17 | `capital_moved` | 1 | 1 `civilization`, 2 `settlement` | — | 50 | `{civilization} moved its seat to {settlement}.` |
| 18 | `civ_ended` | 1 | 1 `civilization` | — | 85 | `{civilization} came to an end.` |
| 19 | `ruin_faded` | 1 | 1 `settlement` | — | 10 | `The ruins of {settlement} faded from memory.` |
| 31 | `settlement_abandoned_to` (r6, generator 0.17.0) | 1 | 1 `settlement`, 2 `civilization`, 3 `destination` (settlement) | `settled`, `lost` | 55 | `{settlement} was abandoned; {settled} {settled?survivor\|survivors} went to {destination}{lost?\|, and one was lost\|, and # were lost}.` |

(`\|` is a table escape; the template has a plain `|`.) The charged civilization of a quiet-step event is its first `civilization` participant (W9): the owner, the founder, or 0 for `ruin_faded`.

**Kind 31** (r6) is appended after P3-06's kinds 20–30, with `region_use` 1, `settlement_use` 1, `caused` 1 and every role count 1–1: a new kind with the structure the destination needs, as P3-04 8.2 requires (a shipped kind is never edited); from 0.17.0 the abandonment of 8.5 item 1 writes it whenever a survivor reaches another settlement, and kind 16 otherwise. Appending a kind is not a schema change (P3-04 8.2); `docs/specs/schema.md`'s catalogue gains the row. Its three-word placeholder `{lost?…}` is P3-04 r10's form. Example (illustrative: seed 42 at 1024, 0.16.0's e522, re-told with 0.16.0's names): `Rannve was abandoned; 869 survivors went to Voeledell.`; with the room of the destination short by 12: `…; 857 survivors went to Voeledell, and 12 were lost.`; a last settlement left with 31 people (below `abandon_min`; r6 no longer abandons a last settlement by chance): `Risfae was abandoned; its 31 survivors were scattered.`

**Cause rules.** A capital's founding is caused by its `civ_founded` (P3-04's kind 3, as the stub). A colony is uncaused (P3-04 8.2's reason for an uncaused `settlement_founded`); its `origin` participant links it to the parent. Kind growth is caused by the settlement's founding event, so a reader can follow a city back to the day it was a camp. Famine, plague and crisis are random pressures (uncaused, P3-00 6.2); a spread is caused by its outbreak; a dwindling by the blow that brought it down, when there is one. An abandonment (kind 16 or 31) is caused by the blow that broke it (10.4) or, in T1, by its last reduction event (the famine, plague or crisis; P3-06's battles through `note_reduction`, 13), else its founding (a settlement that never thrived); and (r6) also by its decline start when it is declining (8.5). A moved capital and an ended civilization are caused by the abandonment; a secession by its crisis; a faded ruin by its `ruin_event`. Every cause is an earlier event (W6).

**Report requirements** (P3-07 20): `settlement_abandoned` is the ruined row's `ruin_event` and `civ_ended` the ended row's `ending_event`, salience 55 and 85, both ≥ 20 and above every other event a civilization usually has except its founding (80) and secessions (70), so they rank among its six milestones (P3-07 7.3). No kind needs a new report block or line (format v1).

**Why these saliences.** P3-07's summary shows at most 250 events, from salience 20 up (7.4). Shown at the threshold of a typical 1024 world (26; 20 to 41 over every 1024 sweep of 15.3): awakenings, civilizations, capitals (40), colonies into new land (30), plagues (45), crises (40), cities (45), secessions, abandonments, moved capitals and endings; hidden: colonies inside a region (15), villages and towns (10, 18), dwindling (15), famines and spreads (25, shown only in small logs), faded ruins (10) and, once the log grows, the quiet eras (20). Seed 42: 15.2.

## 12. Tuning and race data

### 12.1 `history.peoples` (new id of kind `history`, P3-01 5)

All fields required. Status: **taste** (tuned on the sweep), **calibration** (scale), **decided** (by P3-00 or Paul).

| Field | Type | Shipped | Status | Meaning |
| --- | --- | --- | --- | --- |
| `score_site`, `score_biome`, `score_upland`, `score_coast` | int(0, 2000) | 1000 each | taste | multipliers of the four terms of σ (4.2) |
| `ore_reach_full` | int(1, 100000) | 200 | taste | reachable rich-ore cells that score 255 (4.1) |
| `coast_full` | int(1, 100000) | 200 | taste | coast cells that make a region fully coastal (4.1) |
| `rough_base`, `rough_upland` | int(0, 1000) | 150, 600 | taste | rough ground a people can live on (5.1) |
| `people_per_kcell` | int(1, 100000) | 2500 | calibration | people per 1000 cells at full fit and density 1000 (5.1) |
| `capacity_floor` | int(1, 100000) | 400 | bound | least capacity of any region (≥ `founding_population`, so the ×2 bound holds at an awakening) |
| `capital_weight` | int(1000, 10000) | 3000 | taste | a capital's share weight against 1000 (5.2) |
| `founding_population` | int(1, 10000) | 300 | taste | a civilization's first settlement (7.3) |
| `village_min`, `town_min`, `city_min`, `city_max` | int(1, 1000000) | 250, 1500, 8000, 25000 | calibration | the ladder (8.1); a city of 20,000 (design 4) |
| `fortress_max` | int(1, 1000000) | 6000 | taste | a fortress's cap (5.2) |
| `kind_shrink_permille` | int(0, 1000) | 800 | taste | hysteresis of a downgrade (8.2) |
| `site_spacing` | int(1, 1000) | 32 | taste | cells between settlements (512 m) (6.2) |
| `region_slots_max` | int(1, 64) | 4 | taste | standing settlements per region for a founding inside (6.3) |
| `mouth_class_min` | int(1, 15) | 4 | decided | river class of a mouth (P3-00 7) |
| `port_radius` | int(0, 16) | 3 | taste | cells to a mouth or the sea (6.1) |
| `port_mouth_bonus`, `port_coast_bonus` | int(0, 255) | 60, 20 | taste | site bonus (habitability scale) (6.1) |
| `decline_permille` | int(1, 1000) | 100 | taste | yearly loss of the excess over a share (9.1) |
| `recover_permille` | int(0, 1000) | 900 | taste | recovery to thriving (8.3) |
| `colonist_permille`, `colonists_min` | int(1, 1000), int(1, 10000) | 120, 60 | taste | colonists (9.3) |
| `abandon_min` | int(1, 10000) | 40 | taste | below this a settlement is abandoned (8.5) |
| `abandon_frail_max` | int(1, 1000000) | 1200 | taste | a struck declining settlement below this may be abandoned (8.5) |
| `abandon_permille` | record {hamlet, village, town, city, fortress} int(0, 1000) | 300, 250, 50, 0, 0 | taste | that chance by kind (8.5) |
| `found_pressure_permille` | int(1, 2000) | 750 | taste | pressure that makes settlers (9.3) |
| `found_rate_permille` | int(0, 1000) | 100 | taste | yearly founding chance at `expansion` 1000 (9.3) |
| `quiet_found_permille` | int(0, 1000) | 500 | taste | the same in quiet steps, against L years (9.3) |
| `reach_base`, `reach_per_expansion` | int(0, 64), int(0, 64) | 1, 3 | taste | hops from the capital a people expands to (9.3) |
| `expand_min_score` | int(−3000, 4000) | 250 | taste | least σ of new land (9.3) |
| `expand_relative_permille` | int(0, 1000) | 700 | taste | least σ of new land as a share of the home's (9.3) |
| `expand_min_capacity` | int(0, 1000000) | 800 | taste | least capacity of new land (9.3) |
| `cost_penalty`, `pass_cost`, `river_cost` | int(0, 10000), int(1, 100), int(0, 100) | 100, 3, 2 | taste | crossing costs (4.4, 9.3) |
| `river_border_share_permille` | int(0, 1000) | 500 | decided | P3-00 7 `river_border_share` 0.5 |
| `civs_two_permille` | int(0, 1000) | 500 | taste | chance of two civilizations with `auto` (7.2) |
| `regions_per_civ` | int(0, 100) | 4 | decided | P3-00 10.2 interim |
| `home_min_hops` | int(1, 64) | 3 | taste | spacing of one race's homes (7.2) |
| `home_min_capacity` | int(0, 1000000) | 1500 | taste | least capacity of an eligible home (7.1) |
| `intensity` | record {calm, normal, turbulent} int(0, 10000) | 500, 1000, 2000 | taste | the multiplier of 10.1 |
| `famine_crowding_permille` | int(0, 2000) | 900 | taste | crowding at which famine can strike (10.2) |
| `famine_ppm` | int(0, 1000000) | **1250** (r6; 2500 before 0.17.0) | taste | yearly famine chance (10.2) |
| `famine_toll_min`, `famine_toll_max` | int(0, 1000) | 50, 200 | taste | per mille killed (10.2) |
| `plague_ppm` | record {hamlet, village, town, city, fortress} int(0, 1000000) | 300, 600, 1500, 3000, 600 | taste | yearly outbreak chance by kind (10.2) |
| `plague_toll_min`, `plague_toll_max` | int(0, 1000) | 100, 300 | taste | (10.2) |
| `plague_spread_permille`, `plague_spread_toll_permille` | int(0, 1000) | **100** (r6; 200 before 0.17.0), 500 | taste | (10.2) |
| `crisis_per_generation_permille` | int(0, 1000) | 60 | taste | (10.3) |
| `crisis_loss_permille` | int(0, 1000) | 50 | taste | (10.3) |
| `split_min_settlements`, `split_min_regions` | int(2, 1000), int(2, 1000) | 6, 3 | taste | (10.3) |
| `split_permille` | int(0, 1000) | 300 | taste | (10.3) |
| `ruin_fade_years` | int(1, 100000) | 1500 | taste | (8.6) |
| `colony_new_region_salience` | int(0, 100) | 30 | taste | a colony into a free region (9.3) |
| `plague_small_salience` | int(0, 100) | 25 | taste | an outbreak in a hamlet or village (10.2); r6: an outbreak whose total toll is below `plague_major_dead` |
| `plague_major_dead` | int(0, 1000000) | 1000 | taste (r6) | the total dead of an outbreak and its spreads from which the outbreak keeps the base salience (10.2) |
| `crisis_small_salience` | int(0, 100) | 25 | taste (r6) | a succession crisis that does not split the realm (10.3) |

Named checks (per definition, in this order; messages as given):

| Check | Condition | Message |
| --- | --- | --- |
| `peoples_ladder` | `village_min` < `town_min` < `city_min` ≤ `city_max` | `village_min, town_min, city_min must increase and city_max must not be below city_min` |
| `peoples_tolls` | `famine_toll_min` ≤ `famine_toll_max` and `plague_toll_min` ≤ `plague_toll_max` | `a toll minimum must not be above its maximum` |
| `peoples_floor` | `capacity_floor` ≥ `founding_population` | `capacity_floor must not be below founding_population` |
| `peoples_colonists` | `abandon_min` < `colonists_min` | `colonists_min must be above abandon_min` |

```toml
# game/data/history/peoples.toml and the fixtures under worldgen/tests/data/geo*/history/
[history.peoples]
score_site = 1000
score_biome = 1000
score_upland = 1000
score_coast = 1000
ore_reach_full = 200
coast_full = 200
rough_base = 150
rough_upland = 600
people_per_kcell = 2500
capacity_floor = 400
capital_weight = 3000
founding_population = 300
village_min = 250
town_min = 1500
city_min = 8000
city_max = 25000
fortress_max = 6000
kind_shrink_permille = 800
site_spacing = 32
region_slots_max = 4
mouth_class_min = 4
port_radius = 3
port_mouth_bonus = 60
port_coast_bonus = 20
decline_permille = 100
recover_permille = 900
colonist_permille = 120
colonists_min = 60
abandon_min = 40
abandon_frail_max = 1200
abandon_permille = { hamlet = 300, village = 250, town = 50, city = 0, fortress = 0 }
found_pressure_permille = 750
found_rate_permille = 100
quiet_found_permille = 500
reach_base = 1
reach_per_expansion = 3
expand_min_score = 250
expand_relative_permille = 700
expand_min_capacity = 800
cost_penalty = 100
pass_cost = 3
river_cost = 2
river_border_share_permille = 500
civs_two_permille = 500
regions_per_civ = 4
home_min_hops = 3
home_min_capacity = 1500
intensity = { calm = 500, normal = 1000, turbulent = 2000 }
famine_crowding_permille = 900
famine_ppm = 1250               # r6 (0.17.0); 2500 before
famine_toll_min = 50
famine_toll_max = 200
plague_ppm = { hamlet = 300, village = 600, town = 1500, city = 3000, fortress = 600 }
plague_toll_min = 100
plague_toll_max = 300
plague_spread_permille = 100   # r6 (0.17.0); 200 before
plague_spread_toll_permille = 500
crisis_per_generation_permille = 60
crisis_loss_permille = 50
split_min_settlements = 6
split_min_regions = 3
split_permille = 300
ruin_fade_years = 1500
colony_new_region_salience = 30
plague_small_salience = 25
plague_major_dead = 1000        # r6 (0.17.0)
crisis_small_salience = 25      # r6 (0.17.0)
```

With the new id, every data root carries `chronology.toml`, `engine.toml`, `peoples.toml` and `report.toml` (P3-01 5's rule); P3-01's unknown-id test expects `'chronology', 'engine', 'peoples', 'report'`, and its "file for a future id" example becomes `conflict` (amendment 18.4).

### 12.2 Race data (a data change in this merge)

1. **New field** `density_permille` int(1, 1000), required, after `growth_permille` (P3-01 amendment 18.4): elf 300, goblin 900, dwarf 700, faerie 250, human 1000, orc 800 (5.1).
2. **Goblins**: add `{ biome = "temperate_forest", weight = -200 }` and `{ biome = "wet_woodland", weight = -300 }`. Their first weights liked every lowland, so the second race settled in the woods next to the first (prototype, seed 42: the goblins' best free region was East Wood III, 72% forest, beside the elven realm). Made in opposition to the elves (design 5), they now take the open dry country and poorer lowland: 41 of 76 goblin homes grassland, 20 steppe, 12 forest (15.1).
3. **Faeries**: `upland` 0 → 200; biome weights fen 600 → 800, wet woodland 600 → 800, marsh 400 → 600, temperate forest 200 → 100, highland moor 100 → 300, add heath 100. With the first values the faeries' best free land was plain forest in 19 of 20 worlds (wetland share of the home 41‰, like the elves' 5‰); the design's "wild places" (P1-05 critic: forage, wetland, wild uplands) now win where they are free: on seeds 0–11 the faeries' home wetland share rose from a median of 41‰ to 102‰, and over seeds 0–49 it is 88‰ against the elves' 5‰ and the humans' 24‰ (15.1). Wetland-rich regions are rare (P3-00 2.3: a median of one per 1024 world), so the faeries still live mostly in the wettest free woodland (question 7). **Superseded from generator 0.17.0 by 12.3** (r6).
4. Unchanged: elves, dwarves, humans, orcs (the dwarves' outcome came from 4.1's reachable ore and the graded upland, not from new weights).

These change the `race_biome` rows (P3-03) and every world's `content_hash`: P3-03's table golden lines for `race_biome` and P3-01's content-hash lines are re-recorded with this merge by their references (15.5).

### 12.3 Faerie weights after Paul's decision (amendment r5; **shipped from generator 0.17.0 with P3-08, r6**)

Paul's decision of 2026-11-19 on question 7: "Faeries definitely need to be in forested lands, doesn't have to be wet." The 0.15.0 weights of 12.2 item 3 put fen and wet woodland first (800) and plain forest last but one (100). This amendment changes the faerie race file only (`game/data/races/faerie.toml` and the fixture race files): it changes every world's `content_hash`, the `race_biome` rows and where faeries live, so it ships with a `GENERATOR_VERSION` bump and re-recorded golden values. **r6: it ships with the P3-08 change, generator 0.17.0** (Paul's decision: faeries live in forest, wet or not; `docs/specs/p3-08-followups.md` item 4). The 0.15.0 and 0.16.0 data stay as they were; nothing here applies to them.

**Shipped weights (candidate A; r6)**, replacing the faerie `biomes` array in this order (nine entries, `heath` kept with weight 0 so that `race_biome` has its row, P3-03 6); `upland` 200, `coast`, `site_weights`, `density_permille`, `awakening_share`, `expansion` unchanged:

```toml
biomes = [
  { biome = "temperate_forest", weight = 800 },
  { biome = "wet_woodland", weight = 800 },
  { biome = "fen", weight = 300 },
  { biome = "marsh", weight = 200 },
  { biome = "highland_moor", weight = 100 },
  { biome = "heath", weight = 0 },
  { biome = "grassland", weight = -300 },
  { biome = "alpine", weight = -400 },
  { biome = "steppe", weight = -400 },
]
```

The values against 0.15.0's:

| Biome | 0.15.0 | A (recommended) |
| --- | --- | --- |
| `temperate_forest` | 100 | **800** |
| `wet_woodland` | 800 | **800** |
| `fen` | 800 | 300 |
| `marsh` | 600 | 200 |
| `highland_moor` | 300 | 100 |
| `heath` | 100 | 0 |
| `grassland` | −200 | −300 |
| `alpine` | −300 | −400 |
| `steppe` | −400 | −400 |

Forest is the first preference, wet or dry, with one weight for both (Paul: "doesn't have to be wet"); fen and marsh stay liked, below forest, as the wild places of design 5.

**Candidates measured** (prototypes `peoples.py` and `conflict.py` with `fae.py`, `~/.cache/the-unmapped/out/p3-06-spec/`): B = forest 700, wet woodland 900, fen 400, marsh 300, moor 200, heath 100, grassland −300, alpine −300, steppe −400 (wet still slightly first); C = forest 900, wet woodland 900, fen 200, marsh 100, moor 0, heath −100, grassland −400, alpine and steppe −500, `upland` 100, `site_weights` forage 50, timber 40, fresh water 10.

**Where faeries awaken** (homes of every faerie civilization founded at an awakening; medians of the home's shares, ‰; "best is elven": the region of the highest faerie σ among non-islet regions with C ≥ `home_min_capacity`, held or not, is held by elves at the faerie awakening; "home / best" the faerie home's σ against that region's, ‰, in those worlds):

| 1024, seeds 0–49, peoples alone | 0.15.0 | A | B | C |
| --- | --- | --- | --- | --- |
| homes | 61 | 64 | 65 | 63 |
| forest (temperate + wet woodland) | 829 | 983 | 981 | 992 |
| wetland (wet woodland, fen, marsh) | 88 | 6 | 7 | 3 |
| homes with fen + marsh above forest | 1 | 0 | 0 | 0 |
| dominant biome forest | 60 of 61 | 64 of 64 | 65 of 65 | 63 of 63 |
| habitability, capacity of the home | 79, 3,215 | 80, 3,533 | 80, 3,408 | 80, 3,148 |
| best faerie region elven | 16 of 50 | 35 | 36 | 42 |
| home / best when elven | 841 / 952 / 985 | 870 / 978 / 999 | 870 / 977 / 999 | 857 / 986 / 999 |
| faerie population (min / median / max) | 1,817 / 6,182 / 15,097 | 2,723 / 6,114 / 15,396 | 2,723 / 6,114 / 14,737 | 2,465 / 5,655 / 14,478 |
| faerie settlements standing | 3 / 7 / 17 | 4 / 8 / 19 | 4 / 8 / 19 | 4 / 8 / 18 |
| faerie regions held | 1 / 4 / 9 | 1 / 3 / 7 | 1 / 3 / 7 | 1 / 3 / 7 |
| elven settlements standing (median) | 20 | 19.5 | 19.5 | 19 |

| 512, seeds 0–19, peoples alone | 0.15.0 | A | B | C |
| --- | --- | --- | --- | --- |
| forest share of the home | 702 | 713 | 713 | 713 |
| homes not forest-dominated (grassland) | 3 of 20 | 3 | 3 | 3 |
| awakenings with no eligible region (fallback home) | 2 | 2 | 2 | 2 |
| best faerie region elven | 12 of 20 | 17 | 17 | 17 |
| home / best when elven | 436 / 858 / 978 | 312 / 820 / 984 | 314 / 831 / 986 | 234 / 801 / 970 |
| faerie population | 810 / 2,789 / 4,261 | 1,116 / 3,583 / 5,432 | 1,045 / 3,629 / 5,432 | 948 / 3,312 / 5,205 |

**With conflict** (P3-06 prototype as of P3-06 r2: no battle in a war's declaration year):

| | 1024: 0.15.0 | 1024: A | 512: 0.15.0 | 512: A |
| --- | --- | --- | --- | --- |
| faerie population | 295 / 2,641 / 14,608 | 417 / 3,321 / 13,956 | 300 / 1,274 / 4,075 | 178 / 1,442 / 5,333 |
| faerie settlements standing | 1 / 4 / 16 | 1 / 5 / 18 | 1 / 1 / 6 | 1 / 2 / 6 |
| faerie civilizations conquered | 8 of 62 | 4 of 64 | 0 of 20 | 0 of 20 |
| wars against faeries | 0 / 4 / 9 | 0 / 3 / 12 | 1 / 5 / 11 | 1 / 6 / 10 |
| faerie capitals sacked | 0 / 1 / 8 | 0 / 1 / 8 | 0 / 1 / 6 | 0 / 2 / 5 |
| `last_refuge` uses for a faerie settlement (all races) | 88 (183) | 114 (203) | 146 (209) | 111 (197) |
| worlds where the refuge saved a faerie settlement | 18 | 16 | 16 | 13 |
| wars, simulated years (medians) | 23, 1,293 | 24, 1,277 | 27, 1,275 | 27, 1,275 |

**Bounds.** Every hard bound of 15.5 (not empty, capacity, events, runaway, sizes) and of P3-06 14 / 15.5 (races alive, runaway, events, phases, capacity) holds on every world of every candidate, with and without conflict; no race goes extinct.

**Reading.** All three candidates put every 1024 faerie home in forest (median forest share 980–992‰ against 829‰) and none in a fen or marsh. The elves, four thousand years earlier, already hold the faeries' best forest in 35 of 50 worlds (A) against 16 before, since both peoples now want the same land; the faeries then take a forest of 98% of its value (median), so they are not pushed to poor land at 1024. At 512 they are, as before and in every candidate (3 of 20 homes on grassland, 2 awakenings with no eligible region): the small maps' forest is gone to the elves by the faerie awakening. Population and settlements barely move (6,114 against 6,182). A is recommended: it states the decision most directly (one weight for dry and wet forest), keeps the faeries' liking for wild wetland second, and changes nothing else; B is indistinguishable in every measure; C crowds the faeries hardest into elven forest (42 of 50) and lowers their population for no gain.

**No other adjustment needed.** `awakening_share` (0.70), reach (1 hop, `expansion` 100) and `home_min_capacity` (1500) keep "few, hidden, in the woods" true: at 1024 every awakening finds an eligible forest region and faerie realms stay at a median of 8 settlements; moving the awakening earlier would put the faeries' choice before the elves have spread and is not what the design's order says.

**Measured again with the implementation's rules** (r6): the test writers' exact references (`p3_conflict_reference.py` on `p3_peoples_reference.py`) with candidate A alone, on the 0.16.0 sweep worlds 1024 seeds 0–49 (`~/.cache/the-unmapped/out/p3-08-spec/p308.py`, variant `fae`): faerie standing settlements 1 / 5 / 18 against 1 / 4 / 16, faerie population 417 / 3,289 / 13,956 against 295 / 2,638 / 14,608, wars 7 / 23 / 49 against 7 / 23 / 48, sacked 2 / 12 / 24 against 2 / 12 / 32; elves, humans, orcs within a few per cent; the faeries' median elevation rank in their regions falls from 0.12 to 0.07 (they sit in the lowest, wettest woods). This agrees with the prototype's table above.

**Consequences for P3-06's numbers** (P3-06 section 15 is not rewritten until the change ships): the faerie rows and every per-world figure move, because the faeries' homes and neighbours change: faerie standing settlements (4 → 5), faerie civilizations conquered (8 → 4 of about 63), wars against faeries (median 4 → 3, maximum 9 → 12), `last_refuge` uses; the whole-world medians move slightly (wars 23 → 24, simulated years 1,293 → 1,277, events 815 → 785); the seed 42 sample changes (the Teeshti Court awakens in another region, of 983‰ forest; 1,051 events, 44 wars, 12 of them against faeries, against 1,030, 38 and 6), so P3-06 15.2, 15.3, the golden case `hist-conflict-42-1024` and every world hash are re-recorded with the data change.

## 13. The seam for P3-06

P3-05 owns the state of peoples; P3-06 reads it through a read-only view and changes the world only through P3-04's `State` and the two hooks below.

```cpp
namespace worldgen::history {
class PeoplesView {                                      // owned by PeoplesSubsystem; valid during any step
public:
    std::int64_t score(std::int64_t region, std::int64_t race) const;          // σ (4.2)
    std::int64_t capacity(std::int64_t region, std::int64_t race) const;       // C (5.1)
    std::int64_t share(const StepContext&, std::int64_t settlement) const;     // C_s now (5.2); 0 if not standing (r3: the view's state readers take the step's context)
    std::span<const Neighbour> neighbours(std::int64_t region) const;          // open borders with cost (4.4)
    std::int64_t population(const StepContext&, std::int64_t civ) const;       // Σ P of its standing settlements
    std::vector<std::int64_t> regions(const StepContext&, std::int64_t civ) const;      // held, ascending
    std::vector<std::int64_t> settlements(const StepContext&, std::int64_t civ) const;  // standing, ascending
    std::vector<std::int64_t> wanted_held(const StepContext&, std::int64_t civ) const;            // regions held by others that pass 9.3's outward tests but for being held, ascending
    std::int64_t hunger(std::int64_t civ) const;                               // 9.3 item 4: years crowded with no free target
    std::int64_t last_reduction(std::int64_t settlement) const;                // 10.4; 0 none
};
class PeoplesSubsystem final : public Subsystem {
public:
    const PeoplesView& view() const;
    void note_reduction(std::int64_t settlement, std::int64_t event);         // P3-06 after casualties: the cause of a later abandonment
    void ruin(StepContext&, std::int64_t settlement, std::int64_t event);     // 8.5 items 2-5 with `event` as the ruin event (r1); not item 6; the settlement must be standing and owned
};
class PeoplesAwakening final : public AwakeningHandler {
public:
    explicit PeoplesAwakening(PeoplesSubsystem&);                              // r3
};
}
```

The state readers of the view and `ruin` take the step's `StepContext` (r3): a subsystem member may not hold `State` or a reference into it between steps (13.1 item 1), so each call reads the state of the step it is made in.

- **Order** (P3-04 6.4): peoples runs before conflict in every step they share (T2; P3-06's T6 comes after T4). P3-06 is installed after peoples in `EngineConfig::subsystems`.
- **Claims** of free land are peoples' (with a settlement, 9.3); claims of held land, tension, war, battles, sieges, sacking and conquest are P3-06's. `wanted_held` and `hunger` are the inputs of P3-06's tension ("two expanding borders meet", design 5): a civilization with `hunger` > 0 is boxed in, and `wanted_held` says against whom.
- **Military potential** is not defined here: P3-06 derives armies from `population` and its own tuning.
- **Ownership changes by P3-06** (`set_settlement_owner`, `set_region_owner`) are respected by every rule above from the next step: shares use the new owner's race, foundings start from the new owner's regions, the capital rule of 8.5 applies to the new owner.
- **Ruin by sacking** is P3-06's: it calls `PeoplesSubsystem::ruin(settlement, sack event)` (8.5, r1), not `abandon`, so the ruined row's `ruin_event` is the `sacked` event (P3-07 20 item 3); peoples' Q3 fades it like any ruin.
- **Ending by conquest**: P3-06 appends its own kind `civ_conquered` (30, roles `civilization` and `conqueror`, P3-06 11), not `civ_ended` (r1). A civilization with no standing settlement at the start of T1 is ended by peoples with cause its last settlement's `ruin_event` (the rule of 8.5 item 6, applied to P3-06's sackings that left none).
- **Quiet steps**: P3-06's Q4 (P3-00 4.4) may claim only after peoples' foundings of the same step; P3-05 does not claim held land in Q4 and has no pressures there.

### 13.1 Implementation rules from the P3-04 audit (normative; r2)

P3-04 6.9's rules, as P3-05 meets them:

1. **A1, loops.** P3-05 founds civilizations in an awakening (7.3) and by secession (10.3, inside the T4 loop over civilizations) and settlements in T2 and Q4 (9.3, inside the loop over civilizations) and at awakenings. Every such loop runs over ids up to the count taken before it (a civilization or settlement created in the loop is not processed in it anyway, P3-04 6.2) and re-fetches `civ(id)` / `settlement(id)` after a founding; no reference, pointer or span into `State` is kept across `found_civilization` or `found_settlement`, nor in a member between steps (members hold ids: `before`, last reduction, `hunger` are keyed by id). The spread loop of 10.2 and the abandonment of 8.5 found nothing. P3-05 opens and closes no relation.
2. **A2.** P3-05 has no tension source.
3. **N1, order.** Shipped `EngineConfig::subsystems` = peoples, then conflict (P3-06, from 0.16.0), in that fixed order in `stages/history.cpp`. Per step: T1, T4, Q1, Q3 peoples only; **T2 and Q4 peoples first, then conflict**; T6 conflict only. `steps()` lists T1, T2, T4, Q1, Q3, Q4, each once.
4. **N2, shared steps (T2, Q4).** Peoples runs first and sees none of conflict's same-step changes (there are none yet); conflict's changes of earlier steps are live state. What conflict sees of peoples' part of the same step is stated in P3-06 8.3. Peoples reads its own entities through `visible()` everywhere except the destinations of 8.5 items 2, 5, 6 (the stated exception).
5. **N3, draw keys.** Every key is built from ids and the year: `home` (race, region), `civs` (race, 0), `found` and `outward` (civilization, 0), `famine`, `famine_toll`, `plague`, `plague_toll`, `abandon` (settlement, 0), the spread's toll (target settlement, 1: a fixed discriminator, not a position), `plague_spread` (origin settlement, target settlement), `crisis` and `split` (civilization, 0). No key is a position in a list.
6. **N4.** P3-05 stores no relation.
7. **N5, regions.** Within one step a region's owner can change more than once only through P3-05's own sequence (an abandonment's release in T4 and a later secession's transfer in the same T4 do not touch the same region: a released region has no settlement of the seceding part); if it does, the last change and its event are recorded (P3-04 6.7). Two civilizations never colonize the same free region in one T2: the first founding makes it held, and the later civilization's outward test reads the live owner.

## 14. Invariants

For every seed, setting, data set within the schemas, and map size:

1. Every present race has at least one civilization founded at its awakening year by `civ_founded` (cause the awakening), and at most its setting (`auto`: 2); homes are by 7.1 and 7.2 recomputed independently.
2. Every settlement row was founded by `settlement_founded` (a capital) or `colony_founded`; its cell is in its region, `water` 0; no two settlements share a cell, unless the earlier one had faded before the later was founded (a faded settlement blocks no site, 6.2; r3); any two founded while both stood are ≥ `site_spacing` apart, except a relaxed awakening site (6.2 item 3).
3. Every standing settlement has P ≤ 2 × C(R, owner race) (P3-00 12.3) and P ≤ max(kind_cap, its population when the cap was last reached by survivors or colonists); P ≥ `abandon_min` at the end of every T1 and Q1; (r6) a settlement above its kind cap at the start of a T1, or of a year of Q1, loses at least 1 in it (9.1's target T_s).
4. A region's owner is a civilization that has a standing settlement in it, or 0 (with P3-06 too: P3-06 transfers regions only with settlements, P3-06 14 item 5; r1).
5. Every alive civilization has a capital that is one of its standing settlements; every ended one has no standing settlement and holds no region.
6. Kinds: a fortress stays a fortress; a ladder kind is a kind built by a race that has owned the settlement (r6: the founder's race or the race of any later owner, every census row's `owner_civ` and, for a ruined or faded row, the owner before the ruin; a captured settlement keeps its kind and changes it only by 8.2, P3-06 13: a goblin-held former human city stays a city until 8.2's hysteresis lowers it, and a city ruined under goblin owners is a city of a human founder); `became_*` events increase the kind, `settlement_dwindled` decreases it; (r6) a dwarven settlement on a cell with `walk_area` 0 is a fortress (8.4).
7. Every ruined row's `ruin_event` is a `settlement_abandoned` (from P3-06 on, generator 0.16.0, or a `sacked`; r1; from generator 0.17.0 also a `settlement_abandoned_to`, r6); every faded row has a `ruin_faded` event whose cause is the ruin event it had, dated ≥ that event's year + `ruin_fade_years`; a faded row's `ruined_year` and `ruin_event` are cleared (NULL and 0: P3-04 9.2, they hold only while the row is ruined), so the check goes through the `ruin_faded` event's cause (r3).
8. Caused kinds have the causes of 11; no event of 11 is dated in a quiet step except kinds 6–10, 16–19 and 31 (r6).
9. A quiet step of L years with no event of peoples equals L tick years without pressures (9.2): growth and recovery are the same yearly rule; this covers every quiet step in which peoples appends nothing (r3).
10. Draw independence (P3-00 Y3): a test-only extra draw on any `hist.peoples.*` stream before a decision changes nothing else.
11. (r6) **Last settlements**: every abandonment whose population at the abandonment (`survivors` of kind 16, `settled` + `lost` of kind 31) is at least `abandon_min` (so it was the chance of 8.5 (a)) concerns a settlement whose civilization had another standing settlement: such an abandonment is never followed by that civilization's `civ_ended` in the same step.
12. (r6) **Destinations**: every `settlement_abandoned_to` has `settled` ≥ 1, its `destination` is a settlement standing and owned by the event's civilization at that year, at least as near (squared cell distance, then id) as any other of its standing settlements; every `settlement_abandoned` (from 0.17.0) belongs to a civilization that had no other standing settlement or whose nearest had no room; every abandonment's causes include its decline start when the settlement was declining.
13. (r6) **Saliences**: a `plague` has salience 45 exactly when its `dead` plus the `dead` of the `plague_spread` events it causes is at least `plague_major_dead`, else 25; a `succession_crisis` has 40 exactly when a `civ_seceded` names it as cause, else 25.

## 15. Measurements (prototype)

### 15.1 Where each race lands (1024, seeds 0–49, all six races, `auto`)

Homes of every civilization founded at an awakening (not by secession): n homes; the share that are upland (flag of 4.1), that have reachable ore, that are at least a quarter wetland; medians of the home's wetland share (fen, marsh, wet woodland, ‰), `habitability_mean`, `fertility_mean`, forest share (temperate forest and wet woodland, ‰), grassland share (‰); share on the coast (`coast_cells` > 0); the most common dominant biomes.

| Race | n | upland | ore reach > 0 | wet ≥ 250‰ | wet | hab | fert | forest | grass | coast | dominant biome of the home |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| elves | 79 | 0.00 | 0.00 | 0.00 | 5 | 106 | 174 | 992 | 0 | 0.89 | forest 79 |
| goblins | 76 | 0.00 | 1.00 | 0.01 | 60 | 70 | 126 | 118 | 357 | 0.28 | grassland 41, steppe 20, forest 12, heath 2, highland moor 1 |
| dwarves | 73 | 0.79 | 0.93 | 0.00 | 0 | 3 | 6 | 124 | 20 | 0.11 | alpine 38, forest 15, highland moor 14, grassland 4, steppe 1 |
| faeries | 61 | 0.00 | 0.26 | 0.05 | 88 | 79 | 137 | 829 | 1 | 0.61 | forest 60, fen 1 |
| humans | 64 | 0.00 | 0.00 | 0.00 | 24 | 105 | 219 | 168 | 743 | 0.88 | grassland 52, forest 12 |
| orcs | 50 | 0.02 | 0.76 | 0.00 | 24 | 51 | 91 | 78 | 277 | 0.48 | steppe 25, grassland 17, forest 8 |

Read against the critic's checklist (P3-00 13): **elves** in the deepest forest (99% forest, the most fertile of the forests, near the sea); **goblins** in open dry country, steppe and poorer lowland (habitability 70 against the humans' 105), always with a mine in reach; **dwarves** in the uplands (79% upland by the flag, the rest hill forest and moor with a rough share above half) with reachable ore in 93% of homes and almost no farmland (fertility 6): "places the others avoid"; **faeries** in the wettest free woodland, eighteen times the elves' wetland share, poorer and wilder than the elven woods (habitability 79 against 106); **humans** on open farmland (74% grassland, the most fertile homes of all, on the coast); **orcs** on steppe and grassland with mines, the poorest lowland homes (habitability 51), since they arrive last. Under the literal P3-01 reading the dwarves' best region lacked reachable ore in 17 of 50 worlds (4.1); now 5 of 73 dwarf homes do (seeds 5, 6, 32, 38, and seed 42's second realm in Northwest Peaks): second realms, placed three hops from the first, where the uplands with mines lie next to the first.

**Over seeds 0–99** (r6; the P3-05 validation 4.4, the implementation): the table is the same within a few per mille in every column (elves 155 homes, forest 991‰; goblins 159, grassland 85, steppe 40; dwarves 144, 78% upland, ore in reach 94%; faeries 124, forest 848‰; humans 121, grassland 708‰; orcs 105, steppe 54). These are the homes of 0.15.0 and 0.16.0; from 0.17.0 the faerie homes are those of 12.3 (every 1024 home forest-dominated).

**Poor worlds.** At 512 (50 worlds, 11–16 non-islet regions) every race still awakens; an awakening founds two civilizations in 13% of cases (34% at 1024), since the `regions_per_civ` rule and `home_min_capacity` leave room for one each once the land fills. The late races take what is left: in 5 of 50 worlds the orcs' home has σ below 300, in 8 the orcs end with one or two settlements, and in 8 worlds some race holds no region at the present (its home was held: 7.1's fallback; the humans in 2). No world ended with fewer than three standing settlements (minimum 28 at 512, 56 at 1024).

### 15.2 Sample: seed 42, 1024, default settings

**Illustrative (r7).** This sample is the prototype's world of generator 0.15.0 (peoples alone): its names (region names with numerals, bare elven realms such as `Finen`, gods such as `Krok`), sentences ("(0 generations)", "survivors left") and numbers predate P1-06 r3, P3-02 r5, P3-04 r10 and this spec's r6. The golden case `hist-peoples-42-1024` (16.3 item 22) is re-recorded from the reference at 0.17.0 and does not compare with this text.

Seed 42 at 1024, span 6000, `normal`, all races, `auto` (prototype). Ages as P3-04 17 (starts 0, 2074, 3242, 4243, 4969, 5596); 530 events over 1154 simulated years (0.46 per simulated year); 12 civilizations (9 at awakenings, 3 by secession), 91 settlements founded, 90 standing, 1 ruined. Lines are `<span> e<id> <kind>/<salience>: <sentence> [causes]`, P3-07's chronicle line without its `{refs}`.

The first 60 events:

```
0 e1 awakening/100: Fielie awakened the Elves in West Wood III.
0 e2 civ_founded/80: The Elves founded Finen in West Wood III. [causes e1]
0 e3 settlement_founded/40: Finen founded Voeledell in West Wood III. [causes e2]
0 e4 civ_founded/80: The Elves founded The Realm of Ithieglie in Southeast Isle. [causes e1]
0 e5 settlement_founded/40: The Realm of Ithieglie founded Mofaesi in Southeast Isle. [causes e4]
150-699 e6 quiet_era/20: Finen passed 550 quiet years (3 generations), from 150 to 699.
150-874 e7 quiet_era/20: The Realm of Ithieglie passed 725 quiet years (4 generations), from 150 to 874.
700 e8 became_town/18: Voeledell grew into a town of 1520. [causes e3]
725-1249 e9 quiet_era/20: Finen passed 525 quiet years (4 generations), from 725 to 1249.
875 e10 colony_founded/15: The Realm of Ithieglie sent settlers from Mofaesi to found Saelo in Southeast Isle.
900-949 e11 quiet_era/20: The Realm of Ithieglie passed 50 quiet years (0 generations), from 900 to 949.
950 e12 colony_founded/15: The Realm of Ithieglie sent settlers from Mofaesi to found Raenme in Southeast Isle.
975-999 e13 quiet_era/20: The Realm of Ithieglie passed 25 quiet years (0 generations), from 975 to 999.
1000 e14 became_village/10: Saelo grew into a village of 269. [causes e10]
1025-1049 e15 quiet_era/20: The Realm of Ithieglie passed 25 quiet years (1 generation), from 1025 to 1049.
1050 e16 colony_founded/15: The Realm of Ithieglie sent settlers from Saelo to found Glalivedell in Southeast Isle.
1075 e17 became_village/10: Raenme grew into a village of 256. [causes e12]
1100-1249 e18 quiet_era/20: The Realm of Ithieglie passed 150 quiet years (1 generation), from 1100 to 1249.
1250 e19 became_village/10: Glalivedell grew into a village of 256. [causes e16]
1250 e20 colony_founded/15: Finen sent settlers from Voeledell to found Minno in West Wood III.
1275-1424 e21 quiet_era/20: Finen passed 150 quiet years (1 generation), from 1275 to 1424.
1275-2073 e22 quiet_era/20: The Realm of Ithieglie passed 799 quiet years (5 generations), from 1275 to 2073.
1425 e23 colony_founded/15: Finen sent settlers from Voeledell to found Iennmo in West Wood III.
1450 e24 colony_founded/15: Finen sent settlers from Minno to found Rienigla in West Wood III.
1475 e25 colony_founded/30: Finen sent settlers from Minno to found Glathenddell in Southwest Wood.
1500-1574 e26 quiet_era/20: Finen passed 75 quiet years (0 generations), from 1500 to 1574.
1575 e27 colony_founded/30: Finen sent settlers from Minno to found Rannve in West Wood II.
1600-1649 e28 quiet_era/20: Finen passed 50 quiet years (1 generation), from 1600 to 1649.
1650 e29 became_village/10: Rienigla grew into a village of 265. [causes e24]
1675 e30 became_village/10: Glathenddell grew into a village of 260. [causes e25]
1700-1774 e31 quiet_era/20: Finen passed 75 quiet years (0 generations), from 1700 to 1774.
1775 e32 became_village/10: Rannve grew into a village of 265. [causes e27]
1800-2073 e33 quiet_era/20: Finen passed 274 quiet years (1 generation), from 1800 to 2073.
2074 e34 awakening/100: Krok awakened the Goblins in Southwest Steppe.
2074 e35 civ_founded/80: The Goblins founded The Grork Horde in Southwest Steppe. [causes e34]
2074 e36 settlement_founded/40: The Grork Horde founded Zugnob Hole in Southwest Steppe. [causes e35]
2122 e37 famine/25: Famine struck Mofaesi; 66 died.
2160 e38 became_town/18: Zugnob Hole grew into a town of 1513. [causes e36]
2195 e39 famine/25: Famine struck Raenme; 40 died.
2200 e40 plague/25: Plague broke out in Rienigla; 81 died.
2200 e41 plague_spread/25: The plague spread to Voeledell; 158 died. [causes e40]
2224-2373 e42 quiet_era/20: Finen passed 150 quiet years (1 generation), from 2224 to 2373.
2224-3241 e43 quiet_era/20: The Realm of Ithieglie passed 1018 quiet years (7 generations), from 2224 to 3241.
2224-2248 e44 quiet_era/20: The Grork Horde passed 25 quiet years (1 generation), from 2224 to 2248.
2249 e45 colony_founded/15: The Grork Horde sent settlers from Zugnob Hole to found Snozz-Skink in Southwest Steppe.
2274 e46 colony_founded/30: The Grork Horde sent settlers from Zugnob Hole to found Beedruzz in South Downs.
2299 e47 became_town/18: Snozz-Skink grew into a town of 1577. [causes e45]
2299 e48 colony_founded/30: The Grork Horde sent settlers from Zugnob Hole to found Skrotch in Southwest Downs II.
2324 e49 became_town/18: Beedruzz grew into a town of 2020. [causes e46]
2324 e50 colony_founded/15: The Grork Horde sent settlers from Zugnob Hole to found Niskreerk in Southwest Steppe.
2349 e51 became_town/18: Skrotch grew into a town of 1683. [causes e48]
2349 e52 colony_founded/30: The Grork Horde sent settlers from Snozz-Skink to found Snugg in Central Wood.
2374 e53 became_town/18: Glathenddell grew into a town of 1559. [causes e25]
2374 e54 colony_founded/30: The Grork Horde sent settlers from Snozz-Skink to found Gruz-Keegg in South Wood.
2399 e55 became_town/18: Niskreerk grew into a town of 1768. [causes e50]
2399 e56 colony_founded/30: The Grork Horde sent settlers from Snozz-Skink to found Groz in West Downs II.
2399-2498 e57 quiet_era/20: Finen passed 100 quiet years (1 generation), from 2399 to 2498.
2424 e58 became_village/10: Groz grew into a village of 395. [causes e56]
2424 e59 colony_founded/15: The Grork Horde sent settlers from Snozz-Skink to found Drirk Pit in Southwest Steppe.
2449 e60 became_town/18: Snugg grew into a town of 1519. [causes e52]
```

Notable later events: `4017 e146 became_city/45: Grirnbra grew into a city of 8531. [causes e108]` (the dwarven capital); `4392 e197 succession_crisis/40` and `e198 civ_seceded/70: The Warrens of Dribnark broke away from The Grork Horde, taking 4 settlements with it. [causes e197]`; the same horde splits again at 5097 (The Zak-Neez Mob) and 5637 (The Slig Mob); ten human towns become cities between 5244 and 5666; `5989 e520 plague/45: Plague broke out in Voeledell; 207 died.` → `e521 plague_spread/25: The plague spread to Rannve; 68 died. [causes e520]` → `e522 settlement_abandoned/55: Rannve was abandoned; 869 survivors left. [causes e521]` (the elven village's second blow while declining).

The end state at year 6000:

```
c1 Finen (Elves), home West Wood III, founded 0, alive; capital Voeledell (town 1316); 11 settlements, 3 regions, population 11624
c2 The Realm of Ithieglie (Elves), home Southeast Isle, founded 0, alive; capital Mofaesi (village 659); 4 settlements, 1 regions, population 1411
c3 The Grork Horde (Goblins), home Southwest Steppe, founded 2074, alive; capital Zugnob Hole (town 6260); 12 settlements, 3 regions, population 22321
c4 The Hold of Dulimin (Dwarves), home South Wood II, founded 3242, alive; capital Grirnbra (town 6139); 4 settlements, 1 regions, population 13898
c5 The Narinthri Throne (Dwarves), home Northwest Peaks, founded 3242, alive; capital Kholikhu (fortress 3287); 4 settlements, 1 regions, population 6572
c6 The Teeshti Court (Faeries), home East Wood III, founded 4243, alive; capital Matia Glade (village 1499); 8 settlements, 5 regions, population 6182
c7 The Warrens of Dribnark (Goblins), home South Wood, founded 4392, alive; capital Sneezskrub Hole (town 4595); 4 settlements, 1 regions, population 9088
c8 The Kingdom of Stard (Humans), home Northeast Downs II, founded 4969, alive; capital Meadstead (city 12212); 17 settlements, 8 regions, population 108290
c9 The Duchy of Dant (Humans), home Northwest Downs II, founded 4969, alive; capital Bildheldham (town 5006); 5 settlements, 2 regions, population 20319
c10 The Zak-Neez Mob (Goblins), home South Downs, founded 5097, alive; capital Beedruzz (town 3693); 12 settlements, 3 regions, population 30892
c11 The Ghuughzuz Horde (Orcs), home Northwest Downs III, founded 5596, alive; capital Aa'azuugh Fort (town 4801); 4 settlements, 1 regions, population 9139
c12 The Slig Mob (Goblins), home West Downs II, founded 5637, alive; capital Groz (town 3657); 5 settlements, 2 regions, population 9913
settlements standing 90 ruined 1 faded 0
kinds Counter({'town': 52, 'village': 27, 'city': 7, 'fortress': 4})
regions held 31 of 50 nonislet 50
largest [(17428, 'Bromstead', 'city', 'human'), (13846, 'Dessimton', 'city', 'human'), (12811, 'Ashildton', 'city', 'human'), (12212, 'Meadstead', 'city', 'human'), (11938, 'Alessas', 'city', 'human'), (10127, 'Wurdham', 'city', 'human')]
```

31 of 50 regions are held; 19 are free for P3-06's claims and the land hunger of the boxed-in civilizations (13). The largest settlements are human cities of 17,428, 13,846, 12,811 and 12,212 (the Kingdom of Stard's capital Meadstead); the goblins' largest town is their capital Zugnob Hole, 6,260; the elves' first settlement Voeledell is a town of 1,316 in a realm of 11 settlements and 11,624 elves.

**What P3-07's summary would show** (7.4): threshold T = 26 (n(26) = 116 ≤ 250; n(25) is above 250 because of the 101 famines and 75 spreads), so the chronicle shows the 6 awakenings, the 9 `civ_founded` and their 9 capitals (40), 22 colonies into free regions (30), 11 cities (45), 45 plagues in towns, cities and fortresses (45), 10 crises (40), 3 secessions (70) and the one abandonment (55); it hides the quiet eras (20), famines, spreads and village plagues (25), towns (18), villages (10), inner colonies (15) and dwindlings (15). Every civilization's milestones (P3-07 7.3) are its founding, its capital, its colonies into new land, its cities and secessions.

The sample reads as a history of peaceful peoples: two elven realms alone for two thousand years, growing slowly and settling the neighbouring woods; a goblin horde that fills the open country in three centuries and splits three times; two dwarven holds in the hills; a faerie court spread over five small woodland regions; two human kingdoms that turn the downs into cities in a thousand years; orcs arriving on the last free downs. What it lacks is conflict (the goblins and elves live side by side for four thousand years), which is P3-06's.

### 15.3 Sweep statistics

**Per race at the present** (1024, seeds 0–49; min / median / max over the worlds; population is the sum over the race's standing settlements; "peak" is the largest of the snapshots taken every 100 years):

| Race | civilizations | settlements founded | standing | regions held | population | peak population | largest settlement |
| --- | --- | --- | --- | --- | --- | --- | --- |
| elves | 1 / 2 / 4 | 4 / 21 / 49 | 4 / 20 / 48 | 1 / 6 / 13 | 3,992 / 23,887 / 63,780 | 4,208 / 26,205 / 73,671 | 1,175 / 2,727 / 9,311 |
| goblins | 1 / 3 / 7 | 4 / 33 / 89 | 4 / 33 / 87 | 1 / 9 / 24 | 7,977 / 72,214 / 178,274 | 8,484 / 74,696 / 189,223 | 3,949 / 7,152 / 7,999 |
| dwarves | 1 / 1 / 3 | 3 / 7 / 16 | 3 / 7 / 16 | 1 / 2 / 6 | 2,891 / 15,321 / 80,588 | 3,230 / 16,585 / 80,588 | 1,745 / 5,342 / 22,167 |
| faeries | 1 / 1 / 2 | 3 / 7 / 18 | 3 / 7 / 17 | 1 / 4 / 9 | 1,817 / 6,182 / 15,097 | 2,008 / 6,182 / 15,097 | 876 / 1,499 / 1,499 |
| humans | 1 / 1 / 5 | 4 / 11 / 38 | 4 / 11 / 38 | 1 / 6 / 16 | 7,889 / 76,786 / 242,728 | 7,911 / 76,786 / 242,728 | 3,914 / 15,931 / 24,616 |
| orcs | 1 / 1 / 2 | 3 / 5 / 12 | 3 / 5 / 12 | 1 / 2 / 7 | 1,871 / 12,886 / 35,406 | 1,871 / 12,886 / 35,406 | 954 / 4,801 / 7,999 |

**The implementation over 1024 seeds 0–99** (r6; the P3-05 validation of generator 0.15.0, `docs/reviews/p3-05-peoples-validation.md` 4.2; peak population was not measured there):

| Race | civilizations | settlements founded | standing | regions held | population | largest settlement |
| --- | --- | --- | --- | --- | --- | --- |
| elves | 1 / 2 / 4 | 3 / 19 / 49 | 3 / 18.5 / 48 | 1 / 5 / 13 | 1,385 / 22,593 / 63,780 | 764 / 2,516 / 10,261 |
| goblins | 1 / 3 / 8 | 4 / 34.5 / 93 | 4 / 34.5 / 92 | 1 / 9 / 25 | 5,570 / 82,972 / 211,114 | 2,786 / 7,516 / 7,999 |
| dwarves | 1 / 1 / 3 | 3 / 7.5 / 16 | 3 / 7.5 / 16 | 1 / 2 / 6 | 2,891 / 15,933 / 80,588 | 1,745 / 5,443 / 23,798 |
| faeries | 1 / 1 / 2 | 3 / 8 / 19 | 3 / 8 / 19 | 1 / 4 / 10 | 1,817 / 5,975 / 16,328 | 523 / 1,499 / 1,499 |
| humans | 1 / 1 / 5 | 3 / 11 / 38 | 3 / 10.5 / 38 | 1 / 4.5 / 16 | 4,395 / 59,802 / 242,728 | 2,671 / 15,196 / 25,000 |
| orcs | 1 / 1 / 3 | 3 / 6 / 17 | 3 / 6 / 17 | 1 / 2.5 / 8 | 1,871 / 14,084 / 59,136 | 954 / 5,350 / 7,999 |

Largest people: goblins in 54 worlds, humans 41, dwarves 2, elves 2, orcs 1. Seeds 50–99 give fewer humans and more goblins than seeds 0–49 (human median 59,802 against 76,786; goblin median 82,972 against 72,214); on seeds 0–49 the implementation reproduces the prototype's table above. One civilization ended by decline in the whole P3-05 validation: turbulent seed 17, an elven realm of one village abandoned by the frail chance at year 40 (r6's 8.5 (a) now keeps a last settlement from the chance).

No civilization ended in any of the 50 worlds (decline alone rarely kills a people: abandonment needs a settlement struck twice while small, 8.5); goblins reach 7 civilizations by secession in the most divided world (median 3); the elves 4 (two at the awakening, two by secession).

**Over time** (median population / standing settlements of each race, over the worlds where it exists by then; median regions held):

| Year | elves | goblins | dwarves | faeries | humans | orcs | regions held |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1000 | 4,107 / 2 |  |  |  |  |  | 2 |
| 2000 | 9,069 / 10 |  |  |  |  |  | 5 |
| 3000 | 17,739 / 11 | 58,840 / 21 |  |  |  |  | 12 |
| 4000 | 23,037 / 12 | 67,760 / 27 | 6,032 / 2 |  |  |  | 14 |
| 5000 | 22,062 / 17 | 67,121 / 28 | 15,148 / 5 | 2,150 / 3 | 748 / 1 |  | 17 |
| 5500 | 24,552 / 18 | 73,001 / 29 | 16,028 / 5 | 3,961 / 6 | 32,025 / 7 |  | 25 |
| 6000 | 23,726 / 20 | 72,213 / 33 | 14,851 / 6 | 6,080 / 7 | 76,378 / 11 | 12,604 / 5 | 30 |

The elves are alone for two thousand years and still few (under 25,000 at the present: P3-00 13 "old and few"); the goblins fill the open country within a thousand years of their awakening and then grow little; the humans go from one settlement to the largest people in a thousand years ("spread quickly", design 5).

**Whole worlds** (min / median / max):

| Sweep | civilizations | standing settlements | ruined or faded | population | regions held (of non-islet) | events | events per simulated year (median) | worlds with < 3 standing | summary threshold |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1024, seeds 0–49 | 7 / 10 / 15 | 56 / 90 / 128 | 0 / 1 / 4 | 132,928 / 225,212 / 340,800 | 22 / 30 / 39 (44 / 49 / 58) | 310 / 564 / 966 | 0.48 | 0 | 20 / 26 / 26 |
| 1024, seeds 0–99 (r6: the implementation, P3-05 validation 4.1) | 7 / 10 / 15 | 56 / 88 / 136 | 0 / 1 / 4 | 132,928 / 225,176 / 364,648 | 20 / 30 / 42 (42 / 49 / 58) | 287 / 551 / 966 | 0.48 (0.25–0.81) | 0 | 20 / 26 / 26 |
| 512, seeds 0–49 | 6 / 7 / 9 | 28 / 42 / 54 | 0 / 0 / 3 | 37,762 / 66,941 / 93,752 | 8 / 11 / 14 (11 / 13 / 16) | 153 / 269 / 355 | 0.23 | 0 | 20 / 20 / 21 |
| 2048, seeds 0–19 | 8 / 10 / 18 | 98 / 142 / 233 | 0 / 2 / 6 | 304,917 / 475,238 / 821,705 | 41 / 59 / 99 (181 / 197 / 205) | 510 / 802 / 1,289 | 0.67 | 0 | 26 / 26 / 31 |
| 1024 `calm` | 7 / 9 / 13 | 55 / 86 / 128 | 0 / 0 / 2 | 142,615 / 236,962 / 346,125 | 22 / 29 / 41 | 244 / 390 / 603 | 0.33 | 0 | 20 / 21 / 26 |
| 1024 `turbulent` | 8 / 12 / 19 | 60 / 90 / 132 | 0 / 5 / 16 | 128,366 / 213,446 / 337,970 | 22 / 31 / 43 | 443 / 811 / 1,466 | 0.70 | 0 | 26 / 26 / 41 |
| 1024 `--civs-per-race 1` | 6 / 7 / 11 | 40 / 68 / 115 | 0 / 1 / 4 | 107,711 / 188,738 / 308,634 | 15 / 24 / 34 | 197 / 413 / 841 | 0.35 | 0 | 20 / 26 / 26 |
| 1024 `--civs-per-race 4` | 20 / 25 / 29 | 147 / 172 / 204 | 0 / 3 / 9 | 242,764 / 307,591 / 406,387 | 42 / 47 / 55 | 857 / 1,158 / 1,439 | 0.99 | 0 | 26 / 26 / 31 |
| 1024 `--history-detail dense` | 7 / 11 / 16 | 60 / 90 / 137 | 0 / 1 / 5 | 135,157 / 228,523 / 343,510 | 22 / 31 / 40 | 318 / 634 / 1,196 | 0.38 | 0 | 20 / 26 / 31 |
| 1024 span 2000 | 6 / 9 / 12 | 18 / 46 / 76 | 0 / 0 / 1 | 43,350 / 102,857 / 176,287 | 8 / 19 / 31 | 75 / 178 / 302 | 0.46 | 0 | 20 / 20 / 20 |
| 1024 span 12000 | 7 / 12 / 17 | 51 / 116 / 148 | 1 / 4 / 16 | 86,283 / 233,497 / 368,850 | 16 / 31 / 39 | 343 / 1,195 / 1,656 | 0.49 | 0 | 21 / 26 / 41 |
| 1024 `--races elf` | 1 / 2 / 3 | 4 / 20 / 38 | 0 / 0 / 3 | 3,992 / 22,900 / 52,454 | 1 / 5 / 11 | 16 / 116 / 234 | 0.19 | 0 | 20 / 20 / 20 |
| 1024 `--races elf,orc` | 2 / 3 / 6 | 10 / 33 / 50 | 0 / 0 / 3 | 20,514 / 59,904 / 92,660 | 4 / 13 / 20 | 34 / 145 / 263 | 0.24 | 0 | 20 / 20 / 20 |

With `--civs-per-race 4`, 226 of 300 awakenings founded four civilizations, 29 three, 24 two and 21 one (7.2: the later races find no free land three hops from the first home); at 512 with `auto`, 262 of 300 founded one (197 at 1024). At 2048 the histories are larger, not longer (P3-00 2.3): 30% of the non-islet regions are held at the present against 61% at 1024, since reach and late awakenings, not land, limit the peoples there.

**Events by kind** at 1024 (min / median / max per world; seeds 0–49, prototype; the implementation's seeds 0–99 are within a few counts of these, P3-05 validation 4.3: `famine` 39 / 98.5 / 176, `plague_spread` 28 / 111 / 276, `quiet_era` 42 / 71 / 107, `colony_founded` 49 / 80.5 / 130): `colony_founded` 49 / 83 / 122, `became_village` 12 / 21 / 36, `became_town` 19 / 49 / 93, `became_city` 1 / 6 / 16, `settlement_dwindled` 3 / 10 / 20, `famine` 49 / 98 / 176, `plague` 27 / 63 / 128, `plague_spread` 37 / 112 / 276, `succession_crisis` 3 / 15 / 33, `civ_seceded` 0 / 2 / 6, `settlement_abandoned` 0 / 1 / 4, `capital_moved` 0 / 0 / 1, `civ_ended` 0, `ruin_faded` 0 / 0 / 1; with P3-04's `awakening` 6, `civ_founded` 6 / 8 / 9, `settlement_founded` 6 / 8 / 9, `quiet_era` 51 / 73 / 107. Standing kinds at the present: hamlets 0 / 0 / 5, villages 21 / 35 / 55, towns 18 / 42 / 84, cities 0 / 4 / 12, fortresses 0 / 4 / 13. `hunger` (13), the longest boxed-in time of any civilization: 1,543 / 3,072 / 4,925 years: most worlds have a people with nowhere to go for thousands of years, which is P3-06's tension.

**What could not be achieved with peoples alone.** Ruins are rare (median one per world): decline without war seldom empties a town, and P3-00 13's "ruins come from sackings or decline" will mostly be sackings (P3-06). No civilization ends by decline at `normal` or `turbulent` in these sweeps; the mechanism (8.5 item 6) is exercised by the unit and fixture tests. Goblins and elves live side by side for four thousand years without a quarrel. Faeries rarely find real wetland (question 7).

### 15.4 Report keys

**World report `[history]`** (P3-04 11.1), appended after `relations`, integers only, in this order: `population` (Σ `population` of the standing settlements, as P3-07 9 defines it); then for each `race` row in id order, present or not, `population_<race key>` (Σ over standing settlements whose owner's race is that race), then for each race `settlements_standing_<race key>` (those settlements), then for each race `regions_held_<race key>` (`region_control` rows with `to_year` NULL whose civilization is of that race).

**History report `[stats]`** (P3-07 9 and 10.3 rule 1: appended at the end of the table, after `wars_open`): the keys above in the same order and with the same definitions (P3-07 9: every `[history]` key appears in `[stats]` with the same name and value), then `civ_settlements_max` (max over alive civilizations of their standing settlements; 0 when none), `settlements_colonies` (settlements whose `founding_event` is of kind `colony_founded`), `civilizations_seceded` (civilizations whose `founding_event` is of kind `civ_seceded`). Capacities, scores and `hunger` are not stored and are checked by this spec's tests (P3-07 20 item 2).

### 15.5 Hard bounds and typical ranges (the validation sweep, P3-00 12.2)

**Hard bounds** (defects on any seed, P3-00 12.3; checked by the validation sweep from `[stats]` and by test 18 from the tables):

| Bound | Rule | Measured (every swept world) |
| --- | --- | --- |
| not empty | ≥ 1 civilization alive; `settlements_standing` ≥ `min_settlements_alive` (3 at 1024 and 2048, 1 at 512); `civilizations_<race>` ≥ 1 for every present race | civilizations alive ≥ 6; standing ≥ 30 (512), ≥ 40 (1024, every setting), ≥ 98 (2048) |
| capacity | every standing settlement P ≤ 2 × C(R, owner race) (P3-05's own test: C is not stored) | holds in every world |
| events | ≤ 200 per simulated year on average (`events_per_1000_simulated_years` ≤ 200000) | at most 1.28 per simulated year (any sweep) |
| runaway | `share_over_800_years` ≤ 1000 (P3-07 7.5) | far below: the civilizations of one race together held at most 24 of a world's 44–58 non-islet regions at 1024 |
| sizes | history tables under 5 MB at 1024 | at most 1,656 events per 1024 world (span 12000): well under 1 MB |

**Typical ranges** for the sweep (1024, default settings, peoples alone; outside them is a finding to report, not a defect; r6: widened to the implementation's seeds 0–99, P3-05 validation 9 item 1, where seeds 50–99 fell outside the 0–49 ranges): civilizations 7–15; standing settlements 55–140; population 130,000–370,000; regions held 20–42; events 280–1,000, 0.25–0.9 per simulated year; summary threshold 20–30; per race as the tables of 15.3 (elves under 70,000 and under 50 settlements; humans the largest people in 40–50% of worlds (41 of 100; 26 of seeds 0–49); dwarven homes upland or with reachable ore in at least 90% (144 of 144)). These are the peoples-only figures of generator 0.15.0; with conflict (0.16.0 on) P3-06 15 governs, and P3-08's changes (0.17.0) are re-measured by its validation.

### 15.6 For the validation specialist

Sweeps with shipped data and default settings, release build, from `report --history`'s `[stats]` (P3-07 9 with this spec's keys): **1024 seeds 0–199** (the reference; this spec measured 0–49), **512 seeds 0–49**, **2048 seeds 0–19**; and 1024 seeds 0–49 with each of `--event-intensity calm` and `turbulent`, `--civs-per-race 1` and `4`, `--history-detail dense`, `--history-span 2000` and `12000`, `--races elf` and `--races elf,orc` (the eleven rows of 15.3). On every world: the hard bounds of 15.5 (any failure is a defect with seed and setting); the capacity bound through test 18's seam (C is not stored); purity (threads 1 and 5, resume). Report the typical ranges of 15.5 and the tables of 15.1 and 15.3 recomputed; a median more than 20% away from 15.3's is a finding for the spec writer, not a defect. Performance: 16.4's budget at 1024 seeds 0–19.

## 16. Tests required

For the independent test writer; the engineer records final signatures here after implementation.

**Signatures as implemented** (generator 0.15.0, `worldgen/include/worldgen/history/peoples.hpp`): `PeoplesTuning peoples_tuning_from(const data::ContentSet&)`; `PeoplesRace peoples_race(const db::RaceRow&, std::span<const db::RaceBiomeRow>, std::span<const db::CultureRow>, const data::ContentSet&)`; `PeoplesRegion peoples_region(const db::RegionRow&, std::span<const db::RegionBiomeRow>, std::span<const db::BiomeRow>, const PeoplesTuning&, std::int64_t islet_max_dry_cells)`; `region_score`, `region_capacity(const PeoplesRegion&, const PeoplesRace&, const PeoplesTuning&)`, `region_fit(std::int64_t)`; `std::optional<std::int64_t> border_cost(const db::RegionBorderRow&, const PeoplesTuning&)`; `ladder_kind(race, population, tuning)`, `kind_min(kind, tuning)`, `kind_cap(race, kind, tuning)`, `grow_population(P, share, growth_permille, cap, decline_permille)`; `class PeoplesGeo` (constructor `(const HistoryInputs&, const data::ContentSet&, const PeoplesTuning&, std::int64_t islet_max_dry_cells)`; `score`, `capacity`, `neighbours`, `hops`, `is_mouth`, `near_mouth`, `near_sea`, `site_value`, `site(region, race, std::span<const Settlement>, bool relaxed)`); `class PeoplesSubsystem` (`view()`, `note_reduction(settlement, event)`, `ruin(StepContext&, settlement, event)`, `homes(const StepContext&, race)`, `awaken(StepContext&, race, awakening_event)`, `geo()`, `tuning()`); `class PeoplesAwakening(PeoplesSubsystem&)`. Section 13 differs in one respect: `ruin` and the `PeoplesView` functions that read the state (`share`, `population`, `regions`, `settlements`, `wanted_held`) take the step's `StepContext` as their first parameter, since a member may not hold a reference into `State` between steps (13.1 item 1).

**What the reference must reproduce.** The reference `worldgen/tests/reference/p3_peoples_reference.py` (Python, exact integers, on `core_reference`, `p3_names_reference`, `p3_chronology_reference` and `p3_engine_reference`) is written **from this spec alone**, without reading or porting the C++. From a world's stored geography (tables `region`, `region_border`, `region_biome`, `biome`, layers `region`, `water`, `habitability`, `river`, `flow_dir`), its chronology rows and the data root, it computes **every row of all thirteen history tables** of P3-04 9.1 (`event_kind`, `event_kind_role`, `event_kind_value`, `phase`, `event`, `event_participant`, `event_cause`, `event_value`, `civilization`, `settlement`, `settlement_census`, `region_control`, `relation`) with peoples installed, and compares them with the world **byte for byte through the logical table hashes** of P0-04 8 (and row by row on a mismatch, naming the first differing row). It also prints the per-region σ and C table of section 4–5, the homes of every awakening, the `[history]` lines of 15.4, and the rendered sentence of every event. **The spec writer's prototype `~/.cache/the-unmapped/out/p3-05-spec/peoples.py` is not the reference**: it is evidence for the numbers of section 15, uses its own engine emulation (`engine_proto.py`) and skips the bookkeeping tables; where it and this text differ, the text rules and the difference is reported to the lead.

### 16.0 Readings agreed in implementation (normative; r3)

Agreed by the engineer and the test writer and matching the sample of 15.2:

1. 4.2: S is the sum over the five keys first, then one floor division: S = (Σ_k w_k × m_k(R) × 1000) div (255 × Σ_k w_k).
2. 7.3: a capital that is not a fortress gets the kind `ladder(r, founding_population)` (300).
3. T4 order: famine and plague over the settlements (10.2), then the crises over the civilizations (10.3).
4. 10.3: split distances start at the capital's region and run over c's held regions only; a seceding civilization keeps the parent's culture; its home is the new capital's region.
5. W9's budget and the quiet era's "charged" rule count per whole quiet step (Q1 to Q6).
6. The end of Q1 processes abandonments, then kind changes, each in ascending settlement id; one without budget waits for the next step.
7. 9.3: an outward target region with no site means no outward target (the next region is not tried).
8. 9.3: reach (`hops` from the capital's region) is counted over every open border, held or not.
9. 15.4: `regions_held_<race>` counts open `region_control` rows.
10. 4.1: `share_b` is computed for every `region_biome` row, water biomes included (land(R) itself counts only the non-water biomes, 4.1).
11. 6.1: the near-sea and near-mouth neighbourhoods are taken inside the grid, without wrapping.

### 16.0a Fatal messages (normative; P3-04 16.6's form, all ASCII; r3)

As implemented on `p3/05-int` (b62c1cf). Tests compare these texts exactly. `<...>` are decimal integers unless stated.

| Group | Messages |
| --- | --- |
| Tuning | `history: tuning: the content has no definition history.peoples`; the range and named-check messages of 12.1 with the prefix `history: tuning: ` |
| Inputs | `history: peoples: layer '<name>' is not <N> x <N> <type>` (r7: `<type>` is the layer's catalogue type, `u8`, `u16` or `i16`: `region` and `walk_area` `u16`, `water`, `habitability`, `river`, `flow_dir` and `ore` `u8`, `elevation` `i16`; before r7 the text was `… is not <N> x <N> u8 or u16` for every layer, untrue for `elevation`; the engineer changes the string); `history: peoples: race '<key>' or culture '<key>' is not in the content`; `history: peoples: race <r> has no generation_years above 0`; `history: peoples: race <r> does not exist`; `history: peoples: region <R> does not exist`; `history: peoples: cell (<x>, <y>) is outside the map`; `history: peoples: no region`; `history: peoples: '<text>' is not a settlement kind` |
| Bounds (H6) | `history: bound: capacity of region <R> for race <r> exceeds 2147483647` |
| Seams and internal guards | `history: peoples: ruin: settlement <s> is not standing`; `history: peoples: civilization <c> has no standing settlement and none was ruined`; `history: peoples: used before the first step`; `history: peoples: the civilization id is not the expected one`; `history: peoples: the settlement id is not the expected one`; `history: peoples: id <n> below 1` |
| Sites (6.2 item 4) | `history: peoples: no settlement site in region <id>` |

### 16.1 Unit tests (seams on hand-made input)

1. `region_score`, `region_capacity` on hand-made region rows: each term alone (a region of only fen for faeries; an upland with `rough` 1000 and no ore; a coastal region at `coast_cells` 0, 100, 200, 400), negative numerators (floor division: σ for B = −1 must be −1, not 0), the clamp of q, `capacity_floor`, the bound guard.
2. Borders: walkable, pass-only, barrier, river share at exactly 500‰ (open with `river_cost`), `length_m` 0.
3. Sites: seed cell taken; seed cell blocked (spacing 31 vs 32); best-value candidate with a mouth bonus and a sea bonus for races with `coast` −1000, 0, 1000; ties by cell index; faded settlements do not block; relaxed site; no site (fatal message).
4. Mouths: a class-4 river cell draining to the ocean is a mouth; class 3 is not; a mouth whose `flow_dir` points off the grid is not.
5. Homes: ties broken by the keyed draw then id; a high-σ region below `home_min_capacity` skipped; fallback to a free non-islet region of any capacity, then a held one, then any region; `auto` reduced to 1 by `regions_per_civ`; explicit 4 with only two regions far apart (two civilizations); `home_min_hops` across a barrier (unreachable = far).
6. Growth: the formula of 9.1 at P < C_s, P = C_s, P > C_s, g = 0, the `max(1, …)` and the kind cap; recovery; Q1 = L × T1 (property 9).
7. Shares: capital weight; two owners in one region.
8. Kinds: each upgrade, the hysteresis edge (P × 1000 = min × 800 does not downgrade), fortresses fixed, a race with only `fortress`, faeries capped at village.
9. Foundings: each of 9.3's items failing once (no ready settlement; capital not chosen while a satellite is ready; inside without a slot; outward out of reach, below σ_min, below capacity, across a barrier; the chance; colonists too few; quiet budget 0), the outward/inside choice, the salience 30 into a free region, `hunger`.
10. Pressures: famine only at crowding ≥ 900; plague by kind; spread to own settlements in the region and its open neighbours only, not to one already struck; a spread target still draws its own outbreak; strike values; abandonment (a) by `abandon_min` and by chance only when already declining and below `abandon_frail_max`; intensity calm/turbulent scaling.
11. Abandonment: survivors to the nearest (tie by id), room-limited; region release; capital moved (most populous, tie id); civilization ended and its regions freed; the quiet-step budget rule (an abandonment needing three events waits with two left).
12. Crisis: rate per generation; split partition on a hand-made chain of five held regions (the far half leaves), on an exclave (it leaves alone), no split when the part would take every or no settlement; capital loss otherwise.
13. Ruins: fade at exactly `ruin_fade_years`; budget of civilization 0 stops the fading for the step.
14. Catalogue: kinds 6–19 equal section 11 transcribed; templates pass `template_problem`; `history.report` lists every kind (P3-04's `report_events` check).
15. Tuning: each field's range, each named check's message once, the unknown-id expectation of P3-01 (12.1).
16. Race data: `density_permille` required and ranged (P3-01's loader tests).

### 16.1a Unit tests for r6 (generator 0.17.0)

17a. **Cell term** (6.2): a hand-made region with a low habitable cell (`habitability` 200) and a high ore cell (`habitability` 0, `ore` 200, elevation at the region's `elev_max_dm`): a race with `cell_weights` 0, 0 takes the seed cell, then the habitable cell; a race with 400, 600 skips the seed and takes the high cell (v = 0 + (400 × 200 + 600 × 255) div 1000 = 233 against 200); `hgt` at `elev_max_dm` 0 (the max(1, …) guard); ties by cell index.
17b. **Fortress cell rule** (8.4): a dwarven founding on a cell with `walk_area` 0 in a non-upland region is a fortress; the same for a goblin (no cell term) is not; a dwarven founding on a walkable cell of a non-upland region follows the ladder.
17c. **Last settlement** (8.5 (a)): a declining civilization with one settlement below `abandon_frail_max` struck by a plague draws no abandonment and is not abandoned (the draw is not made); with a second settlement created earlier in the same step it is (the draw decides); a last settlement struck below `abandon_min` is abandoned with `settlement_abandoned` and `civ_ended`.
17d. **Destination** (8.5 item 1): survivors 869 with the nearest at 2C − 900 room gives `settlement_abandoned_to` with `settled` 869, `lost` 0 and the sentence `… was abandoned; 869 survivors went to <t>.`; room 857 gives `settled` 857, `lost` 12 and `…, and 12 were lost.`; room 0 gives kind 16 with `its 869 survivors were scattered.`; one survivor and no other settlement gives `its one survivor was scattered.`; the nearest by squared distance, then id.
17e. **Decline start** (8.5 causes): a thriving settlement struck by famine (event a), then by plague (b), then abandoned by chance has causes (a, b); one struck once while thriving and left below `abandon_min` has the one cause; recovery to thriving before the second blow resets the decline start to the second blow; P3-06's `note_reduction` never sets it.
17f. **Cap drain** (9.1): a faerie village of 2,277 with share 3,000 and cap 1,499 loses max(1, 778 × 100 div 1000) = 77 in a T1; at 1,499 it stays; P ≤ cap gives the r5 values exactly (P < C_s, P = C_s, P > C_s).
17g. **Plague salience** (10.2): an outbreak of 600 dead with two spreads of 250 each (total 1,100) has salience 45; with one spread (850) 25; an outbreak that empties its settlement has no spread and its own toll decides; the plan and the strikes agree (the same events, values and order as without the plan).
17h. **Crisis salience** (10.3): a crisis that splits has 40 and the `civ_seceded` names it; one that does not has 25; the template renders `A succession crisis at <capital> shook <civilization>.`
17i. **Catalogue**: kind 31 equals 11 transcribed (roles, counts, values, `caused` 1, uses 1, 1); its template passes `template_problem`; `history.report` lists it.

### 16.2 Property tests

17. Over the fixture worlds (P3-04 16.2 item 17's synthetic regions stage: 2 to 60 regions, islets, barriers, regions with no walkable cell) × seeds 0–15 × spans 2000, 6000 × races all, `elf`, `dwarf,faerie` × `civs_per_race` auto, 1, 4 × intensities: invariants 1–10; rows equal the reference.
18. On the 1024 golden cases: the hard bounds of 15.5.
19. Y3 for each `hist.peoples.*` stream (a test-only extra draw shifts nothing); Y4 (P3-04 16.3 item 23) with peoples installed.
20. Threads 1 and 5 identical; resume (P3-04 Y2) with peoples installed.

### 16.3 Golden vectors

21. `worldgen/tests/golden/p3-05-peoples.txt`, written by the reference: for the four P1 golden cases and the three `hist-*` cases (P3-00 12.4; P3-04 15): (a) per region and race, `<case> score rg<R> ra<r> <σ> <C>`; (b) homes per awakening; (c) the logical hashes of the thirteen history tables (P0-04 8); (d) the `[history]` lines; (e) the rendered sentence of every event of the 512 cases and of the first 300 and last 50 events of the 1024 cases (all in a `*-slow` suite). The C++ must match every line.
22. The sample of 15.2 (seed 42 at 1024, default settings) is a golden case of its own, `hist-peoples-42-1024`, in `p3-05-peoples.txt` only (not in `world-hashes.sh`): its first 60 sentences and its end state must equal 15.2 (the prototype and the reference are independent, so a difference is a defect of one of them or of this text, to be resolved before merge).

### 16.4 Performance

`history` at 1024 with peoples (prototype: 12.5 s per world in Python, single-threaded, including the layer decoding): budget **2 s** in a release build (P3-00 12.1 gives 15 s for P3-04 to P3-06 together); 512: 0.5 s; 2048: 8 s. Costs: 1,150 tick years × ~100 settlements × (growth + 3 draws), ~200 quiet steps × 25 growth years, foundings with a site search over one region's sorted candidates (computed once per region and race, about 8,000 cells), and the shares snapshot per step (O(settlements)). A microbenchmark tagged like P3-04 14.3's reports it.

## 17. Determinism notes

| Concern | Rule |
| --- | --- |
| Arithmetic | `int64` only; floor division for every possibly negative numerator (σ, B, U, C); products bounded (5.1); no `Fx` except `chance` thresholds built by integer division |
| Order | civilizations and settlements by ascending id; regions ascending; neighbours ascending; candidates by (value desc, cell index asc); BFS in ascending neighbour order; spread targets ascending |
| Snapshots | shares and crowding from a snapshot at the start of T1, T2, T4 (and each year of Q1); populations change during T2 only for the founding civilization's own source, which no other civilization reads |
| Draws | all keyed (H4), streams and keys as stated; no draw depends on another's outcome except through state (a settlement that is gone draws nothing) |
| Containers | vectors by id; per-(region, race) caches in vectors; no unordered container |
| Visibility | entities created in a step are not processed in it (P3-04 6.2); site spacing and region ownership read the current state |

Lint impact: none expected.

## 18. Amendments requested

Applied 2026-11-19 on `p3/05-spec` (lead's approval), each in its spec's convention: P3-00 "Correction note (2026-11-19, for P3-05)", P3-04 "Revision note r2", P0-01 section 18, P3-01 "Revision note r6", P3-07 "Revision note r1 (… for P3-05)". 18.1 (`schema.md`) is the engineer's, with the implementation.

### 18.1 `docs/specs/schema.md` (the engineer)

The "Event catalogue" section gains kinds 6–19 of section 11 (structure only: key, caused, region and settlement use, roles, values). No schema change; `schema_version` stays 7.

### 18.2 P3-00 and P3-04 (applied)

1. **P3-00 3.1, 4.4; P3-04 6.2 table**: Q4 "slow expansion" into **free** regions (with a settlement) is P3-05's (9.3); P3-06 may add claims in Q4 after it; `quiet_claims` is not a field (the limit is one founding per civilization per step and the quiet-event budget).
2. **P3-00 4.4 Q1**: "closed form" becomes "the T1 growth rule iterated over the step's years" (9.2).
3. **P3-00 10.5**: a civilization with no standing settlement ends and releases its regions (8.5 item 6), whether by decline (P3-05) or conquest (P3-06).
4. **P3-00 7, "Settlement sites"**: P3-05 reads `habitability`, `water`, `river`, `flow_dir`, not `walk_area` (3.1).
5. **P3-04 9.2** `civilization.founding_event`: the `civ_founded` event, or the `civ_seceded` event of a civilization born by secession (10.3).
6. **P3-04 3.3**: `HistoryInputs` gains `biomes`, `habitability_layer`, `river_layer`, `flow_dir_layer` (3.2).
7. **P3-04 6.4**: the shipped `EngineConfig` installs `PeoplesAwakening` and `PeoplesSubsystem` (steps T1, T2, T4, Q1, Q3, Q4); the stubs S1–S3 remain as test seams only.
8. **P3-00 10.1**: an eligible home also has capacity ≥ `home_min_capacity` for the race (7.1).

### 18.3 P0-01 (section 16 stream list)

The registry of 7.3 gains, after `hist.chronology.awakening`, the twelve `hist.peoples.*` names of 3.3 (46 names). No world byte changes by registration.

### 18.4 P3-01

1. Race field `density_permille` int(1, 1000), required, after `growth_permille`, meaning "people a region supports for this race, per mille of the standard (P3-05 5.1)"; shipped values 12.2; the fixture race files gain it.
2. Field meanings: `site_weights.ore` weighs **reachable** ore (P3-05 4.1); `upland` is weighted by the region's rough share and `coast` by its coast share (P3-05 4.2).
3. Section 5's id table: `peoples` "added by P3-05" (12.1); the unknown-id example list `'chronology', 'engine', 'peoples', 'report'` and the future-id example `conflict`.
4. Section 3.1's shipped goblin and faerie weights as 12.2.

### 18.5 P3-07 (appended keys, format stays v1)

Section 9 gains, after `wars_open`, the keys of 15.4 in that order with those definitions. Section 10.2 already describes what P3-05 fills; no new block or line.

## 19. Open questions for Paul

1. **Population density per race** (5.1, 12.2): a new race field `density_permille`. Recommendation and interim: yes; elves 300 and faeries 250 against humans 1000 makes "few in number" a number. Alternative: derive density from lifespan. **Answered by Paul (2026-11-19): the interim choice stands**, a field (he likes deriving density from a race's age but expects it to be hard to tune).
2. **Reach from the capital** (9.3): a people expands only a few hops from its seat, and spreads farther by splitting. Recommendation and interim: yes (elves 1 hop, humans 3); without it the first two races fill the continent before the humans awaken.
3. **`civs_per_race` 1–4 is a request**: a second civilization needs free land three hops from the first (7.2). Recommendation and interim: yes; at 512 an explicit 4 often gives 2.
4. **No pressures in quiet steps** (10.5): famine, plague and crises happen only in active phases and flare-ups. Recommendation and interim: yes, it is the design's peace; quiet steps write growth, foundings and fading.
5. **Secession as the outcome of some crises** (10.3): a crisis in a large realm may split it. Recommendation and interim: yes; it gives P3-06 rivals of one race and the design's "factions" a forerunner.
6. **Migration deferred** (1): colonists and survivors move; whole peoples do not. Recommendation and interim: refugee migration with P3-06 (after sackings) or P5.
7. **Faeries in forest**: wetland-rich regions are rare (median one per 1024 world), so faeries mostly live in the wettest free woodland. Recommendation and interim: accept; a stronger preference would leave them landless on most worlds. **Answered by Paul (2026-11-19): "Faeries definitely need to be in forested lands, doesn't have to be wet."** The weights are re-tuned in 12.3 (candidate A), as a data change after P3-06.
8. **A plague that empties a settlement** (10.2, r3): should the outbreak still spread to its neighbours? Recommendation and interim: no, as measured (the prototype and the implementation agree; an emptied town has no one left to carry it). Alternative: spread from the outbreak's event regardless.

## 20. For the P3-06 spec writer

What P3-06 reads, may call and must not change, and what it has to address. Section 13 is the interface; this is its summary with numbers.

**State it reads** (P3-04 `State`, read-only for this purpose): civilizations (race, capital, home, alive), settlements (owner, region, cell, kind, status, population), region owners; and through `PeoplesView`: σ(R, r) and C(R, r) per region and race (sections 4–5, integers, σ ∈ [−3000, 4000], C ∈ [`capacity_floor`, 2^31)), the current share C_s of a settlement, `neighbours(R)` with crossing cost (1 walkable, 3 pass, +2 large river; barriers absent), `population(c)`, `regions(c)`, `settlements(c)`, `last_reduction(s)`.

**The two tension inputs** (integer; measured at the present, 1024 seeds 0–49, over the alive civilizations, min / quartile / median / quartile / max):

| Input | Definition | Range on the sweep |
| --- | --- | --- |
| `hunger(c)` | years (tick years count 1, a quiet step its L) during which c had a settlement at founding pressure (9.3 item 1) and no inside or outward target (item 3), counted since c last had a target (a step without pressure adds nothing and resets nothing); reset to 0 in a step that finds a target; ≥ 0, < E | 0 / 96 / 410 / 1,737 / 4,925 years; per world, 1 / 3 / 4 / 4 / 5 civilizations at ≥ 1,000; the largest per world 1,543 / 3,072 / 4,925 (15.3) |
| `wanted_held(c)` | the regions held by **another** civilization that pass every outward test of 9.3 item 3 except "free" (adjacent by an open border to a region c holds, within reach of c's capital, σ ≥ σ_min, C ≥ `expand_min_capacity`, not an islet), ascending; its size is the input | size 0 / 0 / 1 / 2 / 8 per civilization; per world 1 / 4 / 6 / 8 / 11 civilizations with size > 0 |

A tension calibrated so that "a boxed-in civilization with a wanted neighbour" is due (P3-04's scale, 1000 = due) can start from these: `hunger` ≥ 1,000 together with `wanted_held` non-empty is true for a few civilizations in every 1024 world.

**The two hooks** (13): `note_reduction(settlement, event)` after P3-06's casualties (the cause of a later abandonment in T1, 8.5 (b)); `ruin(settlement, event)` (r1), 8.5 items 2–5 for a settlement P3-06 sacks (refugees, ruin, region release, capital move; P3-06 ends the civilization itself with `civ_conquered`). (Superseded by P3-06: it does not use `abandon` or `civ_ended`, and founds no settlements.)

**What P3-06 must not change**: the rows and catalogue kinds 6–19 of this spec (a shipped kind is never edited, P3-04 8.2; it adds its own kinds from id 20); populations except by casualties and transfers through `State` (and then `note_reduction`); peoples' private members (shares, `before`, last reduction, `hunger`); foundings into free land (peoples' only, in T2 before P3-06 and in Q4 before its claims); the step order (peoples before conflict in every shared step). Ownership changes through `State` are respected by every peoples rule from the next step (13).

**What the peaceful sweep leaves to conflict** (facts of 15.1–15.3): ruins are rare (median one per 1024 world, from decline only); no civilization ends in any of the eleven sweeps; elves and goblins live side by side for four thousand years without a quarrel (goblins are hostile to elves from their first year in the design, P3-00 13); on seed 42, 31 of 50 regions are held at the present and 19 are free; the goblins fill the open country within a thousand years of their awakening and then grow little, boxed in; humans become the largest people in their last thousand years only by peaceful expansion, while the design has them spread "over land left open or ruined by older wars". P3-06's wars, sackings and conquests are what should produce ruins, ended civilizations and land for the humans; the flare-up threshold is reached by nothing in P3-05 (it supplies no tension source).

## Appendix A. Proposals entries (added to `docs/design/proposals.md` with this spec)

- "History: peoples expand within reach of their capital and spread by secession" (questions 2 and 5).
- "History: a population density per race" (question 1).
- "History: random pressures act only in active phases; migration waits for conflict" (questions 4 and 6).

## Revision note r1 (2026-11-19, amendments for P3-06)

Requested by P3-06 18.1, applied on `p3/06-spec` after the lead approved P3-06. **What P3-05's implementer at 0.15.0 does: only the restructuring of 8.5 into `ruin(s, e)` (items 2–5) and `abandon` (item 1, `ruin`, item 6), with `PeoplesSubsystem::ruin` public in 13.** It changes no world byte, no golden value and no data. Everything else below describes how P3-06 (generator 0.16.0) uses the seam, and ships nothing at 0.15.0: 13 "Ruin by sacking" (P3-06 calls `ruin`; the `ruin_event` is the `sacked` event) and "Ending by conquest" (kind `civ_conquered`, not `civ_ended`); 14 invariant 4 (no exception: P3-06 moves regions only with settlements) and invariant 7 (`sacked` as a ruin event, from 0.16.0); 20's hook paragraph. Section 15's measurements remain those of a world without conflict (0.15.0); P3-06 15 has the 0.16.0 figures. P3-06's race field `prowess_permille`, goblin `aggression` 600 and tuning id `conflict` are **not** part of 0.15.0 (P3-01 r7).

## Revision note r2 (2026-11-19, rules from the P3-04 audit)

New 13.1: how P3-05 meets P3-04 6.9 (the determinism audit's A1, A2, N1–N5): loops over ids counted before a founding, members holding ids, the fixed order peoples then conflict and which subsystem runs first in each shared step, visibility, draw keys (every key checked: all ids, none a position), no relations, regions changing hands within a step. Normative for the implementer at 0.15.0; no rule, number or world byte changes.

## Revision note r3 (2026-11-19, rulings after implementation)

The lead's rulings on the questions the implementation (`p3/05-int`, b62c1cf) raised; text only, no world byte or number changes. 13: `ruin(StepContext&, settlement, event)`; the `PeoplesView` state readers (`share`, `population`, `regions`, `settlements`, `wanted_held`) take `const StepContext&`; `PeoplesAwakening(PeoplesSubsystem&)`. 14 invariant 2: two settlements may share a cell when the earlier had faded first; invariant 7 through the `ruin_faded` event's cause, faded rows' `ruined_year` and `ruin_event` being cleared; invariant 9 covers every quiet step with no peoples event. 9.2: recovery is applied each year inside Q1; abandonments and kind changes once at the end, in ascending id. 9.3: the quiet factor applies in every quiet step. 10.2: the spread reads s's owner after the outbreak's strike, and an outbreak that abandons s spreads nowhere. 8.5 item 6 and 9.1: `civ_ended`'s region and settlement are the abandoned (or last ruined) settlement's; a civilization without standing settlement ends only in T1. 10.3: a crisis loss sets `before`, declining and the last reduction, and is not a strike. New 16.0 (the test writer's readings, agreed) and 16.0a (fatal messages, normative). Question 8 added (a plague that empties a settlement).

## Revision note r4 (2026-11-19, the P3-05 audit)

10.4: a settlement struck twice in one T4 while declining repeats the same abandonment draw (the audit's N6; as specified). The audit's N5 (9.2's recovery "once at the end") was already corrected by r3: recovery is applied each year inside Q1. What P3-06 must respect in this code is recorded in P3-06 8.4. No rule or world byte changes.

## Revision note r5 (2026-11-19, Paul's answers on questions 1 and 7)

Questions 1 (density stays a field) and 7 (faeries in forest, wet or not) answered. New 12.3: the recommended faerie biome weights (candidate A: temperate forest and wet woodland 800, fen 300, marsh 200, moor 100, heath 0, grassland −300, alpine and steppe −400), measured against two other candidates at 1024 seeds 0–49 and 512 seeds 0–19 with peoples alone and with conflict, the bounds, and the P3-06 figures that move. **Not yet shipped**: a data change scheduled after P3-06, with a generator version bump; the 0.15.0 and 0.16.0 data are unchanged.

## Revision note r6 (2026-11-20, for P3-08; generator 0.17.0)

Normative for the P3-08 change (`docs/specs/p3-08-followups.md`), not before it; sections 1–15 otherwise describe 0.15.0 and 0.16.0. From the world critics' reviews of P3-05 (`docs/reviews/p3-05-peoples-world-critic.md` F1, F2, F3, F4, questions (i) and (ii)) and P3-06 (`docs/reviews/p3-06-conflict-world-critic.md` section 5), the P3-05 validation (`docs/reviews/p3-05-peoples-validation.md` 9) and the P3-06 validation's D1. Rules: 6.2 a per-race cell term (race field `cell_weights`, P3-01 r8; dwarves ore 400, height 600) and no seed-cell rule for a race that has one; 8.4 a dwarven settlement off the walkable ground is a fortress; 8.5 (a) the chance abandonment never takes a civilization's last settlement; 8.5 the abandonment's causes add the decline start, and item 1 names a destination with the new kind 31 `settlement_abandoned_to` (values `settled`, `lost`) or keeps kind 16 when no survivor reaches another settlement; 9.1 refugees above a kind cap drain toward it; 10.2 an outbreak's salience by its total toll (planned before it is written; new field `plague_major_dead`); 10.3 a crisis that does not split has salience 25 (new field `crisis_small_salience`) and its sentence names the capital; 10.2 the famine rate and the plague spread chance halved (data, `famine_ppm` 1250, `plague_spread_permille` 100), for the log's volume. Data: 12.3's faerie weights ship; templates of kinds 14 and 16 (11). Inputs: layers `elevation`, `ore`, `walk_area` and the region's `elev_max_dm` (3). Invariants 3, 6 (read for settlements owned by more than one race: the validation's D1), 7, 8; new 11 to 13. Tests 16.1a. Figures: 15.1, 15.3 and 15.5 gain the implementation's seeds 0–99 (P3-05 validation 4.1–4.4, 9 item 1); the rest of 15 stays the prototype's 0.15.0 measurement. Not adopted (P3-08 2): a cumulative-loss condition for the chance abandonment, a salience bonus for a civilization's first town and city, a regional famine event.

## Revision note r7 (2026-11-20, corrections after the P3-08 implementation)

The lead's rulings on what the P3-08 engineer and test writer found. 10.2: the outbreak's plan is a replay of the outbreak's and its spreads' strikes on a copy of the populations, including an abandoned spread target's refugees joining its owner's nearest settlement, with the same keyed draws and no state change, so that the planned toll equals the real one (invariant 13); r6's "no state changes between the plan and the strikes" was wrong. 11 and the report: kind 31's `[stats]` line (below). 6.2, 8.4: in a world whose `walk_area` is 0 everywhere (synthetic test worlds) every dwarven founding is a fortress; that is the rule, not a defect. Example sentences of this spec checked against 0.17.0's names (15.2's sample and kind 31's example are marked illustrative). 16.0a: the layer message names the layer's catalogue type (`u8`, `u16` or `i16`), true for the new `i16` layer `elevation`; a string change for the engineer. No world byte changes against the implementation of r6 as ruled.

**Report line of kind 31.** P3-07's `[stats]` prints one `events_<key>` line per kind in id order (P3-07 9), so `events_settlement_abandoned_to` is the line after `events_civ_conquered` (kind 30), as both writers implemented.
