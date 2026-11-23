# P5-02 Beasts and monsters

Status: **draft r0, 2026-11-23, for the lead's and Paul's approval.** Written by the spec writer on `p5/02-spec`, cut from `origin/main` at 03a5a2e (generator 0.22.0, schema 8, report format v3; P5-01 and P5-12 merged). Phase P5 History depth, stage 02 (`docs/specs/p5-00-history-depth-overview.md` 3.1 "P5-02 Beasts and monsters", 4, 5, 6.1, 7, 8, 10). Generator **0.23.0** (the next minor version at merge); **schema 9** (the next free version at merge, P5-00 4.3); history report format **v4**.

Sources: design doc 5 ("Peoples, beasts and monsters", "Beasts as pressure", "Monsters as actors", "Undead from history", "Flare-ups", "From history to play"), 6 (`site`, `monster`, `beast_population`), 13 (the P5 exit check), 14; P5-00 (the P5-02 paragraph, 4, 5, 6.1, 7, 8.3, 10); P3-04 r22, P3-05, P3-06, P3-07 r22, P4-01 r3, P5-12 r2; Paul's decisions of 2026-11-23 (relayed by the lead): T16 is a rate (decided), the share of renamings by capital sack stays, and **Paul's wish for abandoned places to explore** (section 8). Carried over from P5-12: section 21.

The stage adds **one subsystem** (P5-00 4.1), `MonstersSubsystem`, which owns the beasts of each region, the named monsters and the first `site` rows (lairs and frontier outposts), and one small rule of peoples (ghost settlements, 8.2). Sections 3 to 20 are normative. Where an earlier spec is amended, its normative text is the section of this spec named in the table of section 2; the amended spec receives a revision note pointing here.

## 1. Purpose

At 0.22.0 the land is empty of everything but peoples: nothing hunts or is hunted, no lair waits in the hills, and the only dead place a player can find is a ruin burned in a war (Paul's wish, 8). P5-02 makes the wild part of the history:

- **Beasts** (wolves, bears, boar, deer, giant spiders) are counts per region and species. They fill their habitat, retreat before settlement, slow the growth of small settlements where predators are many and speed it where game is plentiful, and occasionally make the roads unsafe (a recorded event).
- **Monsters** (trolls, ogres, giants, wyverns, dragons and the undead) are rare named actors with a lair, a territory, a hoard and a kill list (the log). They appear in the wild, or rise where many died in battle; they raid the settlements in reach, may overrun one and make it their lair, sleep and wake, age, and are slain or driven off by the realms they harm. The waking of a great monster in a quiet age is a tension source and brings a flare-up.
- **Sites**: the lair of every monster, and watchtowers that realms raise on a frontier in war and leave when the frontier moves. Every site records who made it, when and by which event, and when and why it was left.
- **Abandoned places**: a settlement may now be overrun by a monster, emptied by plague or famine and left standing, or left after a long decline, and every dead settlement records how it ended. A region whose last settlement is overrun is free and shunned while the monster lives.

The smallest rule set that makes these matter to history is chosen; predator-prey dynamics, heroes as slayers (P5-06), the deep horrors (P5-07) and the sites P6 derives are left for later (section 22).

## 2. Items and where their normative text is

| # | Item | Normative text | Amends | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Data kinds `beast`, `monster`; tuning `history.monsters`; culture field `site_forms`; peoples fields `ghost_*` | 4 | P3-01 (new kinds and id), P3-02 3 (field), P3-05 12.1 | yes |
| 2 | `MonstersSubsystem`: beasts, monsters, sites | 5, 6 | P3-04 6.4 (the shipped configuration), P3-00 4.5 | yes |
| 3 | Seams into peoples and conflict | 7 | P3-05 9.1, 9.2, 9.3, 13; P3-06 7.2, 8.2 | yes |
| 4 | Abandoned places: ghost rule, endings, shunned regions | 8 | P3-05 8.5, 10.4 | yes |
| 5 | Names: kinds `monster` and `site` | 9 | P3-02 4.5 | yes |
| 6 | Event kinds 44 to 55, placeholders, W4, display names | 10 | P3-04 6.6, 8.2, 8.3, 8.4, 8.5 | yes |
| 7 | Schema 9 | 11 | P3-04 9, `docs/specs/schema.md` | yes |
| 8 | Report v4 | 12 | P3-07 (new section 27) | report |
| 9 | Timeline reads for sites, monsters and endings | 13 | P4-01 (r4) | no |
| 10 | Carried over from P5-12: names, audit A1 and A2, T16 | 21 | P5-12 4.1, 5; P3-07 26.6; P5-01 8 | yes |

## 3. Inputs

- **Geography** (`HistoryInputs`, unchanged; no member is added): `regions` (ids, `cells`, `seed_x`, `seed_y`), `region_biomes` (cells per region and biome), `biomes` (code and key), `region_borders` through `GeoView` (neighbours, islets), the layers `region`, `water`, `elevation` and `habitability` (cell choice, 6.6).
- **Live state** (`State`): settlements (region, cell, owner, kind, status, population, capital), civilizations (race, culture, ended), region owners and `owner_event`, relations (`war`, `enmity`).
- **The log so far**, through `StepContext`'s reads (P3-04 r21 23.3): kind, year, region, settlement, values; for the undead (6.3.2) only.
- **Data**: kinds `beast` (`game/data/beasts/`) and `monster` (`game/data/monsters/`), tuning `history.monsters` (`game/data/history/monsters.toml`), the race field `prowess_permille` (P3-06), the culture naming (P3-02, with `site_forms`), the name filter and placename lists (P3-02 11, P1-06 4.1), and the history engine tuning's `flare_threshold` of the world's detail (P3-04 4.1).
- **Setting**: `param.monster_pressure` (P5-00 5) through `HistoryParams::monster_pressure`. `param.event_intensity` does **not** scale any rule of this stage (P5-00 5 lets each stage choose; monsters have their own setting).
- **Streams** (P5-00 4.2), appended to `streams.hpp` after `hist.renaming.name`, in this order: `hist.beasts.bold`, `hist.monsters.wake`, `hist.monsters.rise`, `hist.monsters.sleep`, `hist.monsters.rewake`, `hist.monsters.raid`, `hist.monsters.raid_toll`, `hist.monsters.overrun`, `hist.monsters.hunt`, `hist.monsters.hunt_outcome`, `hist.monsters.lair_name`, `hist.sites.outpost`, `hist.peoples.ghost`, `hist.names.monster`, `hist.names.site`.

**Draws.** Every simulation draw is `StepContext::draw(stream, a, b, k)` (w0 = the step's year; a quiet step's start), written below as `D(stream; a, b, k)`. Chances: `chance_pm(u, p)`, `chance_ppm(u, p)` are P3-05 3.3's; **`chance_ppb(u, p)`** is the same helper with scale 10^9 (`chance_scaled(u, p, 1000000000)`: true iff (u >> 32) < min(2^32, floor(max(0, p) × 2^32 / 10^9))). `pick(u, n)` is P3-02 4.1's. All arithmetic is int64 with checked multiplication (`checked_mul`, fatal on overflow) and floor division of non-negative values (`div`).

## 4. Data

### 4.1 Kind `beast` (new required data kind; P3-01, next revision)

One definition per species, `[beast.<id>]`, in `game/data/beasts/<id>.toml` and both fixture roots (`worldgen/tests/data/geo/`, `geo-small/`). Fields, all required, in this order:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string(32, `display_name`: ASCII letters and space, starting with an upper-case letter) | singular display name (`Wolf`) |
| `plural` | string(32, `beast_plural`: lower-case ASCII letters and single spaces, starting and ending with a letter) | the word sentences use (`wolves`, `giant spiders`) |
| `habitat` | array(record {`biome`: string(32), `weight`: int(0, 1000)}, 1, 16) | per mille of a biome's cells that are habitat |
| `density` | int(0, 100000) | head per 10,000 habitat cells at full wild capacity |
| `growth_permille` | int(1, 1000) | yearly logistic rate |
| `menace` | int(0, 1000) | danger to people per head (0: not a predator) |
| `food` | int(0, 1000) | game value per head for hunting (0: not hunted) |

Checks (`… failed for 'beast.<id>': <message>`), in order: `beast_habitat_biomes`: every `habitat[i].biome` names a `biome` definition (`habitat[<i>].biome '<b>' is not a biome`) and none repeats (`habitat[<i>].biome '<b>' is listed twice`).

**Shipped** (taste; numbers interim until the engineer's calibration sweep, 17):

| id | name | plural | habitat (biome = weight; others 0) | density | growth | menace | food |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `bear` | Bear | bears | temperate_forest 900, wet_woodland 700, highland_moor 400, alpine 500, heath 200 | 5 | 60 | 30 | 30 |
| `boar` | Boar | boar | temperate_forest 800, wet_woodland 900, grassland 300, heath 300, marsh 400, fen 300 | 40 | 250 | 3 | 100 |
| `deer` | Deer | deer | temperate_forest 900, wet_woodland 600, grassland 800, heath 600, steppe 500, highland_moor 500, alpine 200, marsh 200, fen 200 | 100 | 250 | 0 | 60 |
| `giant_spider` | Giant spider | giant spiders | wet_woodland 800, temperate_forest 400, marsh 600, fen 500, blighted_waste 900 | 10 | 200 | 40 | 0 |
| `wolf` | Wolf | wolves | temperate_forest 700, wet_woodland 500, grassland 400, heath 600, steppe 700, highland_moor 800, alpine 400, marsh 100, fen 100, blighted_waste 200 | 20 | 150 | 10 | 0 |

`beast.id` in the world = 1, 2, … in canonical definition-id order (P0-03: `bear` 1, `boar` 2, `deer` 3, `giant_spider` 4, `wolf` 5).

### 4.2 Kind `monster` (new required data kind)

One definition per monster kind, `[monster.<id>]`, in `game/data/monsters/<id>.toml` and both fixture roots. Fields, all required, in this order:

| Field | Type | Meaning |
| --- | --- | --- |
| `name` | string(32, `display_name`) | the kind's display name (`Troll`) |
| `noun` | string(48, `monster_noun`: lower-case ASCII letters, space and `'`, starting with `a ` or `an `) | the phrase `{monster:kind}` renders (`a troll of the hills`) |
| `grouped` | bool | a group (a brood, a host) rather than one creature; recorded for P6, no rule reads it |
| `wake` | enum(`wild`, `dead`) | appears in the wild by habitat (6.3.1) or rises where many died (6.3.2) |
| `biomes` | array(record {`biome`: string(32), `weight`: int(0, 1000)}, 0, 16) | habitat, per mille (`wild` only) |
| `wake_rate` | int(0, 10000000) | `wild`: expected appearances per 10^6 habitat cells per 10,000 years; `dead`: chance in ppm per tick year of a qualifying region |
| `naming_culture` | string(32) | the culture whose syllables (P3-02 3) build its names |
| `name_forms` | array(string(48, `name_form`), 1, 8) | name forms with `{root}` (P3-02 3) |
| `lair_forms` | array(string(48, `name_form`), 1, 8) | lair name forms; `{root}` is the monster's root (9.2) |
| `might` | int(1, 10000000) | fighting strength in levy-equivalents |
| `range_hops` | int(0, 4) | its territory: regions within this many borders of the lair's |
| `raid_permille` | int(0, 1000) | chance of a raid per awake tick year |
| `toll_min_permille`, `toll_max_permille` | int(0, 1000) | the share of a settlement a raid kills |
| `kill_max` | int(0, 10000000) | the most one raid kills |
| `overruns` | array(enum(`hamlet`, `village`, `town`), 0, 3) | settlement kinds it may overrun (6.4.3) |
| `overrun_permille` | int(0, 1000) | chance a raid overruns an eligible target |
| `active_years` | int(1, 100000) | mean years awake before it sleeps |
| `rewake_ppm` | int(0, 1000000) | chance per asleep tick year to wake again |
| `lifespan_years` | int(0, 100000) | years from its appearance to its death of age; 0 ageless |
| `max_alive` | int(0, 64) | living monsters of this kind at once |
| `flare` | bool | its appearance in a quiet step is a tension source (6.5) |
| `hoard_permille` | int(0, 1000) | plunder of a raid, per mille of the target's population |

Checks, in order: `monster_biomes` (as `beast_habitat_biomes`, on `biomes`); `monster_wild_habitat`: a `wild` kind has a weight above 0 (`a wild monster needs a biome with a weight above 0`); `monster_culture`: `naming_culture` names a `culture` definition; `monster_toll_ordered`: `toll_min_permille` ≤ `toll_max_permille`; `monster_overruns_unique`; `monster_bare_form_last`: a form that is exactly `{root}` may appear only as the last entry of `name_forms` (so that 9.2's root is exact: `name_forms[<i>] '{root}' must be the last form`).

**Shipped** (taste; rates interim until calibration, 17). Habitat weights not listed are 0.

| id | name | noun | grouped | wake | biomes | wake_rate | culture | name_forms | lair_forms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dragon` | Dragon | a dragon of the old world | no | wild | alpine 800, highland_moor 400, blighted_waste 600 | 6 | elven | `{root} the Black`, `{root} the Pale`, `{root}` | `The Lair of {root}`, `{root}'s Deep` |
| `giant` | Giant | a giant of the mountains | no | wild | alpine 900, highland_moor 500 | 14 | dwarven | `{root} Stonehand`, `{root}` | `{root}'s Seat`, `The Hall of {root}` |
| `ogre` | Ogre | an ogre | no | wild | grassland 400, heath 600, steppe 700, highland_moor 300 | 10 | orcish | `{root} the Glutton`, `{root}` | `{root}'s Larder`, `The Pit of {root}` |
| `troll` | Troll | a troll of the hills | no | wild | highland_moor 800, alpine 600, temperate_forest 400, heath 300 | 17 | goblin | `Old {root}`, `{root}` | `{root}'s Hollow`, `The Den of {root}` |
| `undead` | Undead | a host of the restless dead | yes | dead | — | 9000 | human | `The Host of {root}`, `The Restless of {root}` | `The Barrows of {root}`, `{root}'s Howe` |
| `wyvern` | Wyvern | a brood of wyverns | yes | wild | alpine 700, heath 500, highland_moor 600 | 17 | goblin | `The {root} Brood`, `The Brood of {root}` | `The Eyrie of {root}`, `{root}'s Crag` |

| id | might | range | raid ‰ | toll ‰ | kill_max | overruns | overrun ‰ | active | rewake ppm | lifespan | max_alive | flare | hoard ‰ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `dragon` | 6000 | 2 | 30 | 50–200 | 600 | hamlet, village, town | 150 | 40 | 2000 | 0 | 1 | yes | 40 |
| `giant` | 1200 | 1 | 40 | 30–100 | 150 | hamlet, village | 60 | 150 | 3000 | 2500 | 2 | yes | 8 |
| `ogre` | 200 | 1 | 100 | 20–60 | 40 | hamlet | 80 | 80 | 8000 | 300 | 3 | no | 3 |
| `troll` | 300 | 1 | 80 | 20–80 | 60 | hamlet, village | 100 | 120 | 5000 | 800 | 3 | no | 5 |
| `undead` | 1500 | 1 | 60 | 30–120 | 200 | hamlet, village | 120 | 200 | 3000 | 1000 | 2 | no | 5 |
| `wyvern` | 600 | 2 | 80 | 20–80 | 50 | hamlet | 50 | 60 | 10000 | 300 | 2 | no | 2 |

`monster_kind.id` = canonical id order: `dragon` 1, `giant` 2, `ogre` 3, `troll` 4, `undead` 5, `wyvern` 6. Why these: design 5's roster exactly (trolls, ogres, giants, undead, wyverns, dragons); two `flare` kinds (the great ones), one `dead` kind; the deep horrors of P5-07 will be further rows of this kind (their stage adds a `wake` value), not a new kind. Names: trolls and wyverns take goblin syllables (harsh, short), ogres orcish, giants dwarven (old, heavy), dragons elven (long and ancient: `Saelith the Black`), the undead human (a host named for a dead lord: `The Host of Edric`).

### 4.3 Tuning `history.monsters` (new fixed id of kind `history`; P3-01, next revision)

`game/data/history/monsters.toml` and both fixture roots. Fields, all required, in this order:

| Field | Type | Shipped | Meaning |
| --- | --- | --- | --- |
| `pressure` | record {`low`, `normal`, `high`} int(0, 10000) | 500, 1000, 1600 | M, per mille, by `param.monster_pressure` (P5-00 5): scales `wake_rate`, `raid_permille`, `rewake_ppm` and the capacity of predators (`menace` > 0) |
| `living_permille` | int(0, 1000) | 100 | world cap on living monsters: max(1, R × this div 1000), R = region rows (4 at 49) |
| `undead_min_dead` | int(1, 10^9) | 4000 | violent dead in a region within the window for the undead to rise |
| `undead_window_years` | int(1, 100000) | 100 | that window |
| `hunt_hops` | int(0, 8) | 2 | settlements within this many borders of the lair send hunters |
| `hunt_levy_permille` | int(0, 1000) | 50 | hunters per mille of their population |
| `hunt_permille` | int(0, 1000) | 150 | chance per year that a realm raided recently sends a hunt |
| `hunt_memory_years` | int(1, 100000) | 30 | how long a raid is remembered |
| `slay_max_permille` | int(0, 1000) | 900 | cap on the odds of a hunt |
| `drive_permille` | int(0, 1000) | 300 | share of the hunts not won that drive the monster off |
| `drive_years` | int(0, 100000) | 100 | a driven-off monster sleeps at least this long |
| `displace_per_kpeople` | int(0, 1000) | 100 | wild capacity lost per 1,000 people in a region, per mille |
| `displace_max_permille` | int(0, 1000) | 900 | cap on that loss |
| `beast_decline_permille` | int(1, 1000) | 200 | yearly loss of beasts above capacity, per mille of the excess |
| `people_floor` | int(1, 100000) | 100 | the least population the per-person measures divide by |
| `drag_k` | int(0, 10000) | 100 | growth drag per unit of predator menace per person |
| `drag_max_permille` | int(0, 500) | 200 | cap on the drag |
| `feed_k` | int(0, 10000) | 2 | growth bonus per unit of game per person |
| `feed_max_permille` | int(0, 500) | 100 | cap on the bonus |
| `bold_on_permille` | int(1, 1000000) | 1500 | predator menace per person at which beasts grow bold |
| `bold_off_permille` | int(0, 1000000) | 800 | the level below which they are no longer bold |
| `bold_permille` | int(0, 1000) | 100 | chance per tick year that the boldness is recorded |
| `bold_toll_permille` | int(0, 1000) | 10 | dead of the region's largest settlement when it is |
| `outpost_permille` | int(0, 1000) | 30 | chance per frontier region and tick year of a war that a watchtower is raised |
| `outpost_max` | int(0, 16) | 2 | open outposts per civilization |
| `outpost_quiet_years` | int(1, 100000) | 200 | years without a hostile neighbour after which an outpost is left |

Check `monsters_bold_ordered`: `bold_off_permille` ≤ `bold_on_permille`.

### 4.4 Culture field `site_forms` (amends P3-02 3)

`culture_naming` gains `site_forms`, array(string(48, `name_form`), 1, 8), required, after `settlement_forms` (P3-02 4.5's rule: a later name kind adds its forms field). Shipped (and both fixture roots): dwarven `{root} Watch`, `The Tower of {root}`; elven `{root} Watch`, `The Spire of {root}`; faerie `{root} Watch`, `The Thorn of {root}`; goblin `{root} Lookout`, `{root} Stockade`; human `{root} Watch`, `{root} Tower`; orcish `{root} Stockade`, `The Watch of {root}`.

### 4.5 Peoples fields (amends P3-05 12.1)

Appended after `crisis_small_salience`: `ghost_permille` int(0, 1000), shipped 200; `ghost_max_population` int(0, 1000000), shipped 300 (8.2).

## 5. State

`MonstersSubsystem` (`worldgen/src/history/monsters.cpp`, with `monsters_beasts.cpp` and `monsters_sites.cpp` as the engineer likes; header `worldgen/history/monsters.hpp`) keeps, in its own members (ids only, never references into `State`):

- **Beasts**: per region r and species s the count B(r, s) (int64, 0 ≤ B ≤ 10^6, fatal `history: bound: beast count exceeds 1000000`, also for K0), the fixed habitat H(r, s) and wild capacity K0(r, s) (6.2); per region the displacement d(r), the growth factor g(r) and the flag `bold(r)`; the `beast_population` rows written so far; the phase of the last update.
- **Monsters**, ids 1, 2, … in creation order (H2; at most 100,000, fatal `history: bound: monsters exceed 100000`): kind, name, root, status (1 awake, 2 asleep, 3 slain, 4 died), born year and event, current lair site, the hop distances from the lair's region to every region (recomputed when the lair moves), its current waking event (the appearance or last `monster_woke`), its last sleeping event, `drive_until` (year), hoard, `holds_settlement` (its lair is a settlement), per civilization the year and event of its last raid on that civilization (a vector of (civ, year, event) in ascending civ), death year and event, slayer.
- **Sites**, ids 1, 2, … (at most 1,000,000): kind (1 lair, 2 outpost), name, region, cell, settlement (0 none), founder civilization (0 none), monster (0 none), origin year and event, end year and event, ending code (11.3), and for an outpost the last year its frontier was hostile.
- The **undead windows**: per region the list of (year, dead, event) in event order, and the log cursor (first event id not yet read).
- `pending_flare` (an event id or 0, 6.5).

## 6. Rules

### 6.1 Steps and order

`steps()` = T1, T4, T6, Q1, Q4. Installed in `EngineConfig::subsystems` **after** `RenamingSubsystem`: peoples, conflict, renaming, monsters (P5-00 4.1). It is also the second tension source: `EngineConfig::tension` = {conflict, monsters}.

| Step | What runs, in this order |
| --- | --- |
| T1 | beasts: one year (6.2) |
| T4 | read the log (6.3.2); beasts bold (6.2.3); the monsters' year (6.4), in ascending id; one creation at most: the undead (6.3.2), else a wild appearance (6.3.1) |
| T6 | outposts: ends (6.6.3), then foundings (6.6.2) |
| Q1 | beasts: L years (6.2) |
| Q4 | `pending_flare` = 0; deaths of age (6.4.4); one wild appearance at most (6.3.1); outposts: ends (6.6.3) |

Y is the step's year (a quiet step's start), L its length. Nothing of this stage acts in T2, T3 or Q3. Raids, sleep, waking again, hunts, risings, boldness and new outposts happen in tick years only: a quiet interval is the design's peace, in which "long-lived figures and monsters simply persist" (design 5).

### 6.2 Beasts

#### 6.2.1 Habitat and capacity (once, at the subsystem's first run)

```
H(r, s)  = Σ over r's region_biome rows (r, b, cells): cells × w(s, key(b))   div 1000
F_s      = M if menace_s > 0 else 1000                                          // M = pressure factor
K0(r, s) = H(r, s) × density_s × F_s div (10000 × 1000)
B(r, s)  = K0(r, s)                                                             // initial count
```

w(s, key) is the species' `habitat` weight for that biome key, 0 if unlisted. Regions in ascending id, species in ascending `beast.id`.

#### 6.2.2 One year of beasts (T1; Q1 repeats it L times)

With P(r) = Σ population of the standing settlements (status 1 or 2, any owner, visible or not) in region r, read once at the start of the step (in Q1 after peoples' drift, so fixed for the L years):

```
d(r)    = min(displace_max_permille, P(r) × displace_per_kpeople div 1000)
K(r, s) = K0(r, s) × (1000 − d(r)) div 1000
B < K:  B' = 0 → 1;  else B + max(1, B × growth_s × (K − B) div (1000 × K))       // then min(B', K)
B > K:  B' = B − max(1, (B − K) × beast_decline_permille div 1000)
B = K:  B' = B
```

After the last year of the step, for each region r:

```
Pd     = max(P(r), people_floor)
Pred   = Σ_s B(r, s) × menace_s ;   Game = Σ_s B(r, s) × food_s
drag   = min(drag_max_permille, Pred × drag_k div Pd)
feed   = min(feed_max_permille, Game × feed_k div Pd)
g(r)   = 1000 − drag + feed                                                    // 500 ≤ g ≤ 1500 by the data bounds
menace(r) = Pred × 1000 div Pd
```

g(r) is what peoples reads (7.1). Before the subsystem's first run g(r) = 1000 for every region.

**Rows** (`beast_population`, 11.2). At the start of a T1 or Q1 whose `phase_id()` differs from the phase of the previous beast update, before updating, and after the very first update of the run (year 0 when the history starts there), and finally when the stage ends the run (`finish`, year E − 1): for each (r, s) in ascending order, a row (r, s, year, B(r, s)) is appended when B differs from the last row of (r, s) (no row counts as 0). The year of a row written at a phase change is the last year of the previous update (the end of the previous phase). So a row is the count at the end of its year, recorded at year 0's end, at the end of every phase and at E − 1.

#### 6.2.3 Beasts grow bold (T4)

For each region r ascending with at least one standing visible settlement: if `bold(r)` and menace(r) < `bold_off_permille`: `bold(r)` = false. Else if not `bold(r)` and menace(r) ≥ `bold_on_permille` and `chance_pm(D(hist.beasts.bold; r, 0, 0), bold_permille)`: t = the region's standing visible settlement of the largest population (then smaller id); dead = min(P_t − 1, max(1, P_t × `bold_toll_permille` div 1000)); b = the species of the largest B(r, s) × menace_s (then smaller id); append `beasts_bold` (region r, settlement t; participants t `settlement`, owner `civilization`; values `beast` = b, `dead`); `set_population(t, P_t − dead)` when dead > 0; `note_reduction(t, event)` (P3-05 13); `bold(r)` = true. At most one per region per year by construction; quiet steps write none.

### 6.3 Monsters appear

A region **has a lair** when an open site of kind lair lies in it (one living monster per region at most). L_world = max(1, R × `living_permille` div 1000). A creation needs: living monsters < L_world, the kind's living < `max_alive`, and the region has no lair. At most **one creation per step** in the whole world.

#### 6.3.1 Wild appearances (T4, Q4)

For each kind k with `wake` = `wild`, in ascending id, while no creation has happened this step: for each non-islet region r ascending, if the caps allow and H_k(r) = Σ cells × weight_k div 1000 (as 6.2.1, with the kind's `biomes`) > 0:

```
p = wake_rate_k × H_k(r) div 10 × M div 1000 × (1000 − d(r)) div 1000 × L      // ppb; L = 1 in a tick
if chance_ppb(D(hist.monsters.wake; r, k, 0), min(p, 10^9)): appear(k, r); stop
```

In Q4 a creation also needs `quiet_events_left(0)` > 0. **appear(k, r)**: monster id m = count + 1, site id s = count + 1; its name (9.1), then its lair cell (6.6.1) and the lair name (9.2); append `monster_appeared` (year Y, region r, settlement 0; participants m `monster`, s `lair`; no cause); create the site (kind 1, region r, settlement 0, founder 0, monster m, origin Y and the event) and the monster (awake; born Y and the event; lair s; waking event = the event). In Q4, when `flare_k`: `pending_flare` = the event.

#### 6.3.2 The undead rise (T4)

**Read** every event e from the cursor to `last_event_id()` in ascending id, then advance the cursor. For e of kind `battle` or `battle_at_pass` (dead = `winner_dead` + `loser_dead`) or `sacked` (dead = `dead`), with region r ≠ 0: append (year(e), dead, e) to W[r]. Then for every r drop the entries with year ≤ Y − `undead_window_years`; S(r) = Σ dead.

For each region r ascending with S(r) ≥ `undead_min_dead`, while no creation has happened this step: for each kind k with `wake` = `dead`, ascending, if the caps allow and `chance_ppm(D(hist.monsters.rise; r, k, 0), wake_rate_k × M div 1000)`: **rise(k, r)**: as appear, with event `undead_rose` (region r; settlement = the lair's settlement or 0; participants m `monster`, s `lair`; **causes** = the at most 8 entries of W[r] with the largest dead, then smaller id, listed in ascending id); the lair is at the cell of settlement x when the entry of the largest dead (then smaller id) is a sack whose settlement x is ruined or faded and is the settlement of no open site (site.settlement = x), else the wild cell of 6.6.1. W[r] is cleared after a rising.

### 6.4 The monsters' year (T4)

For each monster m in ascending id, n = the count before the loop, skipping the slain and the dead:

1. **Age.** If `lifespan_years` > 0 and Y ≥ born + `lifespan_years`: append `monster_died` (region = its lair's; participant m `monster`; cause its born event); status 4, death Y and the event; its lair site ends (ending 2, 11.3). Next monster.
2. **Asleep.** If Y ≥ `drive_until` and `chance_ppm(D(hist.monsters.rewake; m, 0, 0), rewake_ppm × M div 1000)`: append `monster_woke` (lair region; participant m; cause its last sleeping event); awake, waking event = the event. Next monster.
3. **Sleep.** Awake: if `chance_pm(D(hist.monsters.sleep; m, 0, 0), max(1, 1000 div active_years))`: append `monster_slept` (lair region; participant m `monster`, its lair `lair`; cause its waking event); asleep, last sleeping event = the event. Next monster.
4. **Raid.** If `chance_pm(D(hist.monsters.raid; m, 0, 0), min(1000, raid_permille × M div 1000))`: the raid of 6.4.1 (and 6.4.3).
5. **Hunts** (6.4.2), if m is still awake and alive.

#### 6.4.1 A raid

The target t: among the standing visible settlements whose region lies within `range_hops` of the lair's region (hop distance over `GeoView` neighbours), the smallest key (hops, −population, id). None: no raid. With P = P_t and c = t's owner:

```
τ       = toll_min + pick(D(hist.monsters.raid_toll; m, 0, 0), toll_max − toll_min + 1)   // per mille
dead    = max(0, min(kill_max, P × τ div 1000, P − 1))
plunder = P × hoard_permille div 1000
```

Append `monster_raid` (region of t, settlement t; participants m `monster`, c `civilization`; values `dead`, `plunder`; cause m's waking event); `set_population(t, P − dead)` when dead > 0; `note_reduction(t, raid)`; hoard += plunder; m's last raid on c = (Y, raid). The raid does not change t's status (as P3-06's casualties; P3-05 13).

#### 6.4.2 Hunts

For each civilization c in ascending id of m's last-raid list with Y − year ≤ `hunt_memory_years`, alive and visible:

```
H_c = Σ over c's standing visible settlements u with hops(lair region, region(u)) ≤ hunt_hops: P_u × hunt_levy_permille div 1000
S_c = H_c × prowess_permille(race of c) div 1000;                 S_c = 0: next c
if not chance_pm(D(hist.monsters.hunt; m, c, 0), hunt_permille): next c
odds = min(slay_max_permille, S_c × 1000 div (S_c + might))
u    = pick(D(hist.monsters.hunt_outcome; m, c, 0), 1000)
u < odds:                                              slain
u < odds + (1000 − odds) × drive_permille div 1000:    driven off
else:                                                  nothing; next c
```

**Slain**: append `monster_slain` (lair region; participants m `monster`, c `slayer`; value `hunters` = H_c; cause m's last raid on c); status 3, death Y and the event, slayer c; the lair ends (ending 1). **Driven off**: append `monster_driven_off` (lair region; participants m `monster`, c `civilization`; cause m's last raid on c); asleep, last sleeping event = the event, `drive_until` = Y + `drive_years`. Either ends the hunts of the year. No population is lost by the hunters, and a failed hunt writes nothing (heroes and costly hunts: P5-06).

#### 6.4.3 An overrun

After a raid on t, if m holds no settlement as its lair, kind(t) is in the kind's `overruns`, t is not its owner's capital, its owner has another standing settlement (those created earlier in the step count, as in P3-05 8.5 item 2), no open lair of another monster lies in t's region, and `chance_pm(D(hist.monsters.overrun; m, t, 0), overrun_permille)`:

1. a new site s' (kind 1, region of t, at t's cell, settlement t, monster m), named by 9.2;
2. append `settlement_overrun` (region of t, settlement t; participants m `monster`, c `civilization`, s' `lair`; value `survivors` = P_t now; cause the raid);
3. `PeoplesSubsystem::ruin(ctx, t, event)` (P3-05 8.5's ruin procedure: the survivors join c's nearest standing settlement up to its room, t becomes ruined with that event as its `ruin_event` and owner 0, and c's hold on the region is released when it has no other standing settlement there; t is not the capital, so no seat moves);
4. m's old lair ends (ending 3, event: the overrun); m's lair = s', `holds_settlement` = true, the hop distances from the new region.

A monster overruns at most one settlement in its life.

#### 6.4.4 Deaths of age in a quiet step (Q4)

For each living monster in ascending id with `lifespan_years` > 0 and born + `lifespan_years` ≤ Y + L − 1, if `quiet_events_left(0)` > 0: 6.4 item 1 (the event dated Y). Otherwise it waits for the next step. Nothing else of 6.4 runs in a quiet step.

### 6.5 Tension (amends P3-06's tension list)

`MonstersSubsystem::tension(ctx)` returns {T_d, `pending_flare`} when `pending_flare` ≠ 0, else {0, 0}; T_d is `flare_threshold` of the world's detail (P3-04 4.1). It is a pure read (P3-04 6.9 rule 2): `pending_flare` is set and cleared in Q4 only. Since conflict is the first source and the engine keeps the first source on a tie (P3-04 6.5), a monster flares a quiet interval only when conflict would not have: every flare-up that conflict's tension would have started still starts with conflict's trigger.

### 6.6 Sites

#### 6.6.1 Cells

A cell is **taken** when it is the cell of a settlement (any status) or of an open site.

- **The wild cell** of region r: among the cells with layer `region` = r and layer `water` = 0 that are not taken, the smallest `habitability`, then the largest `elevation`, then smaller y, then smaller x. None: (`seed_x`, `seed_y`) of r.
- **The height** of region r (outposts): among the same cells, the largest `elevation`, then smaller y, then smaller x. None: no outpost.

#### 6.6.2 Outposts raised (T6)

For each civilization c alive and visible in ascending id, with fewer than `outpost_max` open outposts: E_c = the alive civilizations e with an open `war` relation between c and e (either order). E_c empty: next c. For each region R ascending with owner c, not an islet, with no open outpost in it, and with a `GeoView` neighbour whose owner is in E_c: if `chance_pm(D(hist.sites.outpost; c, R, 0), outpost_permille)` and the height of R exists: raise it and go to the next c (one per civilization per year). Raising: site id s; its name (9.1, kind `site`, culture of c); append `outpost_built` (region R; participants c `civilization`, e `enemy` (the smallest id of E_c owning a neighbour of R), s `site`; cause the `since_event` of the open `war` row of c and e); the site (kind 2, at the height, founder c, monster 0, settlement 0, origin Y and the event; hostile year Y).

#### 6.6.3 Outposts left (T6 and Q4)

For each open outpost s in ascending id, founder c, region R. First, its **hostile year** becomes Y when a neighbour of R is owned by a civilization with an open `war` or `enmity` relation with c. Then it ends, in this order of reasons:

| reason value | when | cause | site ending (11.3) |
| --- | --- | --- | --- |
| 2 | c has ended | c's `ending_event` | 5 realm fell |
| 1 | the owner of R is not c | R's `owner_event` (0: no cause) | 4 land lost |
| 0 | Y − hostile year ≥ `outpost_quiet_years` | none | 6 frontier quiet |

Ending: append `outpost_abandoned` (region R; participants c `civilization`, s `site`; value `reason`; the cause when there is one); the site ends with that event. In Q4 it needs `quiet_events_left(c)` > 0 (c is the charged civilization, W9), else it waits.

### 6.7 P3-04 6.9

1. No `found_civilization`, `found_settlement`, `open_relation` or `close_relation` call. `ruin` (6.4.3) changes a settlement and a region owner, never creates; every loop over settlements, civilizations or regions iterates ids up to a count taken before it and re-fetches after any change; members hold ids.
2. `tension()` is pure (6.5).
3. Fixed place: after renaming in `subsystems`, after conflict in `tension`, both filled in code in `stages/history.cpp`.
4. Visibility: below.
5. Keys: entity ids, region and kind ids, never list positions (3).
6. No relation is stored.
7. Region owners change only through `ruin`.

**What it sees in shared steps.** T1: peoples' growth of the year (live populations). T4: peoples' famine, plague, crisis and ghost abandonments of the year (live), and settlements and civilizations created in the step are invisible (raid targets, hunters). T6: conflict's and renaming's changes of the year (owners, relations; new entities invisible). Q4: peoples' foundings (invisible), conflict's cessions (live owners). Its own changes reach peoples and conflict as live state from the next subsystem call on (7).

**Quiet steps.** Per quiet step at most one appearance (charged to civilization 0), the deaths of age due in it (civilization 0), and the outposts left (their founders); every one checks the budget first, so W9 holds by construction. The bound of P5-00 4.1 for world-level events: at most 1 + (living monsters) per quiet step.

## 7. Seams into peoples and conflict

### 7.1 Growth (amends P3-05 9.1, 9.2)

`PeoplesSubsystem` gains `void set_wild(const WildView*)`, called once by the stage; `WildView` (declared in `peoples.hpp`) has `std::int64_t growth_permille(std::int64_t region) const` and `bool shunned(std::int64_t region) const`. With no view set, growth is 1000 and nothing is shunned. In 9.1's growth, with g the race's `growth_permille` and G = `growth_permille(region of s)` as the view holds it when peoples runs (the end of the monsters' last T1 or Q1):

```
d = (P × g × G × (C_s − P) + 500 × 1000 × C_s) div (1000 × 1000 × C_s);  if g > 0: d = max(1, d)
```

For G = 1000 this is 9.1's formula exactly (the same rational number, floored), so a world without beasts grows as at 0.22.0. Only the rate changes: shares, caps and the decline above the target do not read G, so every settlement still tends to the same size. Q1 uses the G of the step's start for all L years.

### 7.2 Shunned regions (amends P3-05 9.3)

`shunned(r)` ⇔ an open lair whose `settlement` ≠ 0 lies in r: a monster holds a settlement there (an overrun, or the undead in a sacked ruin). In 9.3 item 3, neither an inside target nor an outward target R1 may be a shunned region. Nothing else of peoples reads it (awakening homes and the flight of a last refuge, P3-06 20.1, are unchanged). No region is shunned before the first overrun or rising, so 9.3 is unchanged in a world without one.

### 7.3 Populations and ruins (P3-05 13, unchanged seams)

Raids and boldness lower populations through `State::set_population` and record `note_reduction`, exactly as P3-06's casualties: a settlement left below `abandon_min` is abandoned in the next T1 or Q1 with the raid as its cause (P3-05 8.5 (b)), and is then a ghost place (8.3). An overrun uses `ruin` as P3-06's sack does.

### 7.4 Flare-ups (amends P3-06 7.2, 8.2)

`StepContext` gains `std::int64_t phase_trigger_event() const` (the current phase row's `trigger_event`, 0 when none; amends P3-04 6.4). In P3-06 7.2, the declaration forced at the start of a flare-up (the best claim) is forced **only when the phase's trigger event is of kind `claim` or `claim_settlements`**. Before this stage every flare-up's trigger was conflict's report, whose trigger is the event of a stored claim, which is of one of those two kinds (P3-06 5.4, r19 23.2, r22 24.2), so the rule changes nothing in a world without a monster's flare-up. A flare-up started by a monster brings no forced war; conflict's ordinary declarations of its tick years run as in any active phase. The design summary's "a flare-up always brings its war" (design 5, P3 settled) becomes "a flare-up of strain always brings its war" (proposals entry).

### 7.5 What else peoples and conflict read

Nothing. Neither reads a beast count, a monster, a site, an event of this stage, or `settlement.ending`. Conflict keeps its own records of what it took (P3-06; it never reads a `ruin_event`), so an overrun or ghost settlement gives no claim. **Proof obligation (test I0, 19.2)**: with the fixture tuning `zero` (all `pressure` factors 0, `drag_max_permille` = `feed_max_permille` = 0, `bold_permille` 0, `outpost_permille` 0, peoples' `ghost_permille` 0), the history tables of a world (events, participants, causes, values, phases, civilizations, settlements but for the new column, census, control, relations, region names) are byte-identical to those of the same world with `MonstersSubsystem` not installed and no view set. What differs then is only the catalogue rows of the new kinds and the new tables.

Because every new event takes an id, and conflict keys some draws by event ids (a war's declaration), histories differ from 0.22.0's from a world's first new event on; earlier targets are re-measured as statistics (16), not pair for pair.

## 8. Abandoned places (Paul's wish of 2026-11-23)

**The wish**: "Are there any conditions that can lead to an area of the map losing its owner without gaining a new one? I'd like there to be situations that can lead to abandoned points of interest that the player can explore." **Measured at 0.22.0** (lead, 100 default 1024 worlds): about 440 regions lost their holder with no successor, every one because its last settlement was sacked; 90 once-held regions are free at the end (median 1 a world); of 1,123 dead settlements 1,071 were sacked and 52 left by migration; none was emptied by plague, famine or decline and left standing. The only abandoned place was a burned ruin.

### 8.1 What this stage answers, item by item

| Wish | Answer | Rule |
| --- | --- | --- |
| (a) a monster empties a settlement without destroying it and may take it as its lair; the region becomes free and is avoided while the monster lives | the **overrun**: the settlement dies with ending `overrun`, a lair site at its cell; its region is released when it was its owner's last there, and is shunned | 6.4.3, 7.2 |
| (b) a settlement emptied by plague or famine, or dwindled with no one to move to, is left standing, told apart from a sacked ruin | the **ghost rule** (8.2) and the existing abandonments; a **recorded ending cause**, not a new status (8.3) | 8.2, 8.3 |
| (c) outlying places that are not settlements and can be left behind; `site` carries "abandoned in Y because of e" from the start | **lairs** and **outposts** (watchtowers); `site.ended_year`, `ending_event`, `ending` | 6.6, 11.2 |
| (d) every dead place records how it ended and by which event, readable by year | `settlement.ending` with `ruin_event`; `site.ending` with `ending_event`; the timeline's reads by year | 8.3, 11, 13 |

### 8.2 The ghost rule (amends P3-05 10.4)

In 10.4's strike, after 8.5 (a)'s test, when the strike is a `famine`, `plague_outbreak` (its origin's strike) or `plague_spread`, s is still standing, its kind is hamlet or village, P (after the blow) < `ghost_max_population`, s is not its owner's capital, its owner has another standing settlement, and `chance_pm(draw(hist.peoples.ghost, Y, s, 0, 0), ghost_permille)`: s is **abandoned** by 8.5's procedure (item 1: `settlement_abandoned_to` to the nearest, or `settlement_abandoned`; causes: the strike and its decline start; then `ruin`; item 6 cannot apply). The outbreak's plan (10.2, r7: a replay) includes this test with the same draw, so the planned total still equals the real one. A race's last refuge is never abandoned (P3-05 21 item 6: the owner-has-another condition already excludes it). Quiet steps have no pressures, so no ghost abandonment. The emptied place keeps its houses: nothing burns it, and its ending says so (8.3).

### 8.3 How a settlement ended (new column, not a new status)

**Decision**: a recorded ending cause, `settlement.ending`, beside the unchanged status. Status 3 (ruined) and 4 (faded) stay the one notion every P3 rule reads (shares, fading after `ruin_fade_years`, the report's counts); a fifth status would touch every rule and reader that tests "standing" or "ruined" and would still not say *why*. The code is derived from the stored log and checked on read (11.4), so it is a convenience for P6, the viewer and the game, never a second truth.

| `ending` | Word | When (settlement status 3 or 4; k = kind of its `ruin_event`) |
| --- | --- | --- |
| 0 | — | standing (status 1 or 2) |
| 1 | `sacked` | k = `sacked` |
| 2 | `overrun` | k = `settlement_overrun` |
| 3 | `plague` | k is `settlement_abandoned` or `settlement_abandoned_to` and a cause has kind `plague`, `plague_spread` or `plague_outbreak` |
| 4 | `famine` | k as in 3, no plague cause, and a cause has kind `famine` |
| 5 | `left` | every other case (an abandonment after a raid, a decline, a crisis, war losses, tribute, emigration; any later stage's ruin kind until it names its own code) |

A burned ruin is ending 1, a monster-held place is ending 2 (with an open lair at its cell while the monster lives), a ghost place is 3, 4 or 5. A settlement once ruined never stands again (P3-05 8.3), so the ending at year Y is the stored code from `ruined_year` on (13).

### 8.4 Expected effect and how earlier targets are kept

Overruns are at most one per monster and need an eligible small, non-capital target; ghost abandonments need a hamlet or village struck below 300 people. The prototype estimate (no sweep was run for this draft; the engineer's calibration sweep measures it, 17): 1 to 3 overruns and 3 to 6 ghost places per default world, against about 90 standing settlements at E, so the standing count at E falls by a few per cent at most; target G2 bounds it (16). Free regions at E rise by about one a world. The P3-05 and P3-06 targets read standing settlements and wars, which these rules touch only through small places; they are re-measured (16, row P).

## 9. Names

### 9.1 Monster and site names (amends P3-02 4.5)

Two name kinds are added after `settlement`, in this order, with their streams: **`monster`** (`hist.names.monster`; entity id = monster id) and **`site`** (`hist.names.site`; entity id = site id). An attempt is P3-02 4.2's with:

- `monster`: D = the naming data of the kind's `naming_culture`, F = the kind's `name_forms`, c = that culture's `culture.id`;
- `site` (outposts only): D and c of the founder's culture, F = D.`site_forms`.

The name is P3-02 4.3's (64 attempts, the registry, the numbered fallback), through the name filter like every name (P3-02 11.3). The registry is the history stage's one registry (P5-12's seam), so a monster's or outpost's name is unique among every name of the world. Monster and site roots are **not** added to the core set K of P3-02 12.2 (cores are civilization and settlement roots). Names are drawn at creation, in creation order (H2).

### 9.2 Lair names and the monster's root

The **root** of monster m is P5-12 5.3's `realm_root(name, kind.name_forms)` (the first form A`{root}`B in list order with the name starting with A, ending with B and longer than |A| + |B|; with `monster_bare_form_last` a bare `{root}` form matches only when no other does). A lair's name: F = the kind's `lair_forms`, n = |F|, start = pick(D(hist.monsters.lair_name; s, 0, 0), n); for j = 0 … n − 1, N = F[(start + j) mod n] with `{root}` replaced by the root, with 21.2's possessive rule; the first N that is valid by P5-12 5.2 items 1 to 4 (with the parts: the form's literal runs and the root) and not in the registry is the name. None: the start form's N followed by ` ` and roman(k), the smallest k ≥ 2 giving a name not in the registry (fatal above 64 bytes, as P3-02 4.4). The name is added to the registry.

### 9.3 Renaming by a monster's ravage (amends P5-12 4)

`name_role` may also name a role of entity type monster: `{name}` is then that monster's root (9.2). The renaming subsystem reads the monster's name through `MonstersSubsystem::monster_name(id)` (a const read; the subsystem is constructed before renaming and passed to it; the stage builds both). `game/data/history/renaming.toml` gains rule 7:

| # | key | kinds | values | min | window | min salience | capital | chance ‰ | weight | name_role | namer_role | forms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 7 | `ravage` | `settlement_overrun` | — | 1 | 1 | 0 | no | 600 | 75 | `monster` | `civilization` | `The Haunt of {name}`, `{name}'s Shadow`, `The Forsaken {noun}`, `The Hollow of {name}` |

The renaming event's cause words gain `settlement_overrun=the ravage` (21.2). Example: `After the ravage, the Leas of the Harvest became known as the Haunt of Grakh.`

## 10. Events

### 10.1 Catalogue (appended after 43; ids provisional, the next free ones at merge, P5-00 4.4)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values |
| --- | --- | --- | --- | --- | --- | --- |
| 44 | `monster_appeared` | 0 | 1 | 0 | 1 `monster` monster 1–1; 2 `lair` site 1–1 | — |
| 45 | `undead_rose` | 1 | 1 | 2 | 1 `monster` monster 1–1; 2 `lair` site 1–1 | — |
| 46 | `monster_woke` | 1 | 1 | 0 | 1 `monster` monster 1–1 | — |
| 47 | `monster_slept` | 1 | 1 | 0 | 1 `monster` monster 1–1; 2 `lair` site 1–1 | — |
| 48 | `monster_raid` | 1 | 1 | 1 | 1 `monster` monster 1–1; 2 `civilization` civilization 1–1 | `dead`, `plunder` |
| 49 | `settlement_overrun` | 1 | 1 | 1 | 1 `monster` monster 1–1; 2 `civilization` civilization 1–1; 3 `lair` site 1–1 | `survivors` |
| 50 | `monster_slain` | 1 | 1 | 0 | 1 `monster` monster 1–1; 2 `slayer` civilization 1–1 | `hunters` |
| 51 | `monster_driven_off` | 1 | 1 | 0 | 1 `monster` monster 1–1; 2 `civilization` civilization 1–1 | — |
| 52 | `monster_died` | 1 | 1 | 0 | 1 `monster` monster 1–1 | — |
| 53 | `beasts_bold` | 0 | 1 | 1 | 1 `settlement` settlement 1–1; 2 `civilization` civilization 1–1 | `beast`, `dead` |
| 54 | `outpost_built` | 1 | 1 | 0 | 1 `civilization` civilization 1–1; 2 `enemy` civilization 1–1; 3 `site` site 1–1 | — |
| 55 | `outpost_abandoned` | 0 | 1 | 0 | 1 `civilization` civilization 1–1; 2 `site` site 1–1 | `reason` |

Order of appending = this table. The charged civilization of a quiet-step event (W9) is its first civilization participant: 0 for 44 and 52, the founder for 55. Region of 46, 47, 50 to 52: the monster's lair region.

### 10.2 Templates and salience (data, `history.report`)

| key | salience | template | summary? |
| --- | --- | --- | --- |
| `monster_appeared` | 45 | `{monster}, {monster:kind}, made its lair in {@region}.` | yes |
| `undead_rose` | 50 | `{@causes:battle=the slaughter,battle_at_pass=the slaughter,sacked=the sack|After |, }the dead of {@region} rose as {monster}.` | yes |
| `monster_woke` | 25 | `{monster} woke again in {@region}.` | at low thresholds |
| `monster_slept` | 10 | `{monster} sank into sleep in {lair}.` | no |
| `monster_raid` | 15 | `{monster} fell upon {settlement}; {dead?no one died|one died|# died}.` | no |
| `settlement_overrun` | 50 | `{monster} drove the people from {settlement} and made it its lair; {survivors?no one escaped|one escaped|# escaped}.` | yes |
| `monster_slain` | 55 | `{slayer} hunted down and slew {monster}, {monster:kind}, in {@region}.` | yes |
| `monster_driven_off` | 30 | `{civilization} drove {monster} back to its lair in {@region}.` | yes |
| `monster_died` | 30 | `{monster}, {monster:kind}, died of great age in {@region}.` | yes |
| `beasts_bold` | 15 | `In {@region} the {@beast} grew bold, and travellers from {settlement} went in fear; {dead?none|one|#} died on the roads.` | no |
| `outpost_built` | 12 | `{civilization} raised {site} in {@region} against {enemy}.` | no |
| `outpost_abandoned` | 10 | `{civilization} abandoned {site}{reason?, the frontier having fallen quiet|, the land being lost|, its realm having fallen}.` | no |

Saliences against the shipped scale (P5-00 7.1): an appearance or a slaying reads like a city (45) or a peace (60) rather than a sack (75); raids, sleep, boldness and outposts stay below `SUMMARY_MIN_SALIENCE` (20), so the summary sees about 15 new candidates a default world (6 appearances, 1 rising, 2 slayings, 1 or 2 overruns, 3 wakings, 1 or 2 drivings and deaths) against its budget of 250; in `civs4` (threshold 71 to 76) none enters. No companion is added (P3-07 24.2): each sentence names its monster and, where it is first or last seen, its kind. Samples (illustrative names): `Grakh, a troll of the hills, made its lair in the Grey Fells.` · `After the slaughter, the dead of the Red Downs rose as the Host of Edric.` · `Old Grakh drove the people from Nub Pit and made it its lair; 41 escaped.` · `The Kingdom of Cear hunted down and slew Saelith the Black, a dragon of the old world, in the Cold Peaks.` · `In Thornwood the wolves grew bold, and travellers from Brannock went in fear; three died on the roads.`

Causes (W6): an appearance has none (a random pressure, P3-00 6.2); a rising its window's battles and sacks; a waking its last sleep or driving off; a sleep its waking; a raid its monster's current waking event; an overrun its raid; a slaying or a driving off the monster's last raid on that realm; a death of age the appearance; an outpost its war's declaration; an outpost left the event that took the land or ended the realm, else none.

### 10.3 Rendering (amends P3-04 8.3, 8.4, 8.5)

- **Display names** of entity types 8 and 9: `monster.name` and `site.name` (8.3 had "none for 6-9"; 6 and 7 still none).
- **Form `kind`** (8.4's `form` gains `"kind"`): for a role of entity type monster, `monster_kind.noun` of its first participant's kind, copied as stored (lower case). `template_problem`: `form 'kind' needs a monster role at byte <n>` (checked where the race-role form check is).
- **Placeholder `{@beast}`** (8.4's `body` gains `"@beast"`): `beast.plural` of the `beast` row whose id is the event's value `beast`, as stored. `template_problem`: `{@beast} needs a kind with a value 'beast' at byte <n>`. `HistoryView::load` checks that every such value names a `beast` row.
- The names-inside-a-sentence rule applies to monster and site names as to any name (`The Host of Edric` mid-sentence renders `the Host of Edric`).

### 10.4 The writer (amends P3-04 6.6 W4)

For participants of entity types 8 and 9 the engine knows no entity: W4 checks only that the id is at least 1. The history stage checks at write time that every such participant names a row of `monster` or `site` (fatal `history: event <id> (<kind>): <type> <id> has no row`), and the reader checks it again (11.4).

## 11. Schema 9

### 11.1 DDL (amends P3-04 9; hard rule 7)

`settlement`'s statement gains a last column, byte for byte:

```
CREATE TABLE settlement (id INTEGER PRIMARY KEY, name TEXT NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, region INTEGER NOT NULL, founder_civ INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event INTEGER NOT NULL, owner_civ INTEGER NOT NULL, kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT NULL, ruined_year INTEGER, ruin_event INTEGER NOT NULL, ending INTEGER NOT NULL) STRICT
```

Five statements are appended after `region_name`'s, in this order:

```
CREATE TABLE beast (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT NULL, menace INTEGER NOT NULL, food INTEGER NOT NULL) STRICT
CREATE TABLE monster_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, noun TEXT NOT NULL, grouped INTEGER NOT NULL, wake INTEGER NOT NULL, might INTEGER NOT NULL) STRICT
CREATE TABLE beast_population (region INTEGER NOT NULL, beast INTEGER NOT NULL, year INTEGER NOT NULL, count INTEGER NOT NULL, PRIMARY KEY (region, beast, year)) STRICT, WITHOUT ROWID
CREATE TABLE monster (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, lair_site INTEGER NOT NULL, born INTEGER NOT NULL, born_event INTEGER NOT NULL, died INTEGER, death_event INTEGER NOT NULL, slain_by INTEGER NOT NULL, status INTEGER NOT NULL, hoard INTEGER NOT NULL) STRICT
CREATE TABLE site (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, region INTEGER NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, settlement INTEGER NOT NULL, founder_civ INTEGER NOT NULL, monster INTEGER NOT NULL, origin_year INTEGER NOT NULL, origin_event INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER NOT NULL, ending INTEGER NOT NULL) STRICT
```

`SCHEMA_VERSION`, `user_version` and `meta.schema_version` become **9**; readers refuse version 8 (`world schema version 8, this build reads 9`). The empty world gains the root pages of the five tables and the automatic indexes `sqlite_autoindex_beast_1` and `sqlite_autoindex_monster_kind_1`: expected 35 + 7 = **42 pages, 172032 bytes**, to be confirmed by the engineer with the vendored SQLite (P3-04 9.6's method). `db::HISTORY_TABLES` stays at 13 entries (P3-04 r22 23.4); the new tables are read by name, after `region_name`.

### 11.2 Columns, codes and row order

| Table | Rows, order | Columns |
| --- | --- | --- |
| `beast` | one per `beast` definition, ascending id (canonical id order, 4.1); written by the history stage, empty without history | `key` the definition id; `name`, `plural`, `menace`, `food` as in the data |
| `monster_kind` | one per `monster` definition, ascending id (4.2) | `key`; `name`; `noun`; `grouped` 0/1; `wake` 1 wild, 2 dead; `might` |
| `beast_population` | (region, beast, year) ascending; 6.2.2's rows | `count` ≥ 0, the count at the end of `year` |
| `monster` | ascending id (creation order) | `kind` = `monster_kind.id`; `lair_site` its last lair; `born` the year it appeared or rose and `born_event` that event; `died` NULL while it lives, else the year, with `death_event` (0 while alive); `slain_by` the slayer (0 unless slain); `status` at E: 1 awake, 2 asleep, 3 slain, 4 died; `hoard` the sum of its raids' `plunder` |
| `site` | ascending id (creation order) | `kind` 1 lair, 2 outpost; `region`; `x`, `y` its cell; `settlement` the settlement at that cell it occupies (0 none); `founder_civ` (outposts; 0 for lairs); `monster` (lairs; 0 for outposts); `origin_year`, `origin_event`; `ended_year` NULL while open, `ending_event` (0 while open), `ending` |
| `settlement` | as before, plus `ending` (8.3) | |

`site.ending`: 0 open; 1 its monster was slain; 2 its monster died of age; 3 its monster moved to an overrun settlement; 4 the founder lost the land; 5 the founder's realm fell; 6 the frontier fell quiet. Codes 4 to 6 are the outpost reasons (6.6.3); later stages append codes (mines worked out, P5-04; a delving lost, P5-07). The name `origin_event` is design 6's; `layout_seed` is P6's (P5-00 6.1).

Entity type codes 8 `monster` and 9 `site` come into use (no new code). `docs/specs/schema.md` gains the DDL, these notes, the changelog row and the catalogue rows of 10.1 (the engineer).

### 11.3 Report, hash and viewer

`report` (P3-04 11.1): `[tables]` gains `beast`, `beast_population`, `monster`, `monster_kind`, `site` in name order; `[history]` gains, directly after `region_names`: `beasts`, `monster_kinds`, `beast_populations`, `monsters`, `sites` (rows), also `[stats]` keys (12.6). `hash --detail`: one `table <name>` line each, in name order. The viewer reads worlds through the library and takes schema 9 with it; the viewer engineer is told the new tables and 13's reads (P5-00 4.3); `gdext-headless-check` compares layers and composites and is unaffected.

### 11.4 Checks on read (amends P3-07 5.3, 26.6; `read_history_tables` and `HistoryView::load`, one function)

After the `region_name` checks (21.3 included), the first failing row of each table in key order, as error values (`corrupt table '<table>': row <key>: <reason>`, code `corrupt_layer`, exit 3), reasons in the order listed. Every check is linear in its rows: references by id are bounds-checked index lookups (P3-07 26.7's helper form), and "every event has a row" is a pass that marks a `std::vector<std::uint8_t>` indexed by event id (A2's form, 21.3).

- **`beast`, `monster_kind`**: `id <i> is not <previous + 1>` (ids 1, 2, … in order); `key is not a definition id`; `name is not a name`; for `beast`, `plural is not a plural`, `menace` and `food` outside 0 to 1000; for `monster_kind`, `noun is not a noun`, `grouped` not 0 or 1, `wake` not 1 or 2, `might` outside 1 to 10000000.
- **`beast_population`** (region, beast, year): `not above the previous row`; `unknown region <r>`; `unknown beast <b>`; `year <y> outside 0 to <E - 1>`; `count <c> outside 0 to 1000000`; `count equals the previous row of this region and beast`.
- **`site`**: `id <i> is not <previous + 1>`; `kind <k> is not 1 or 2`; `unknown region <r>`; `cell <x> <y> outside the map`; `unknown settlement <s>` (s ≠ 0); `settlement <s> is not at the site's cell`; for kind 1 `monster <m> is not a monster row` and founder ≠ 0; for kind 2 `unknown civilization <c>` and monster ≠ 0; `origin_year <y> outside 0 to <E - 1>`; `origin_event <e> is not an event of its year naming this site` (an event of that year with a participant of type 9 and this id, of kind `monster_appeared`, `undead_rose`, `settlement_overrun` or `outpost_built`); `ended_year` NULL ⇔ `ending_event` 0 ⇔ `ending` 0 (`end columns disagree`); `ended_year <y> before origin_year`; `ending_event <e> is not an event of year <y>`; `ending <k> outside 0 to 6`.
- **`monster`**: `id <i> is not <previous + 1>`; `unknown kind <k>`; `name is not a name`; `lair_site <s> is not a lair of this monster` (site kind 1 with `monster` = id); `born_event <e> is not the appearance of this monster in year <born>` (kind `monster_appeared` or `undead_rose`, year `born`, participant m in role `monster`); `status <s> outside 1 to 4`; status 1 or 2 ⇔ `died` NULL ⇔ `death_event` 0 (`end columns disagree`); status 3 ⇔ `slain_by` ≠ 0, `death_event` a `monster_slain` of year `died` with this monster and `slayer` = `slain_by`; status 4 ⇔ `death_event` a `monster_died` of year `died` with this monster; `hoard` negative.
- **`settlement.ending`**: `ending <k> is not its ruin's ending <k'>` (8.3's derivation from the `ruin_event` and its causes).
- **Events**: every participant of type 8 or 9 names a row (`event <e>: <type> <id> has no row`); every event of kinds 44, 45, 49, 54 is the `origin_event` of exactly one site (`<kind> without its site row`); every value `beast` names a `beast` row.

Ids are dense and ascending, so every lookup is an index; nothing relies on SQLite's row order beyond `ORDER BY` the primary key, and no check has undefined behaviour on a damaged file (every index is checked before use).

## 12. Report v4 (amends P3-07; new section 27)

### 12.1 Format version

The header is **`# worldgen history v4`** at both levels: 12.2 changes what references of types 8 and 9 print and 12.3 adds words to an existing line, which the growth rule of 25.1 does not cover. The growth rule holds for v4 unchanged. `report --help` prints `format '# worldgen history v4'`.

### 12.2 References (amends 6.1 "Ids")

A monster prints `<name> (m<id>)`, a site `<name> (si<id>)` (escaped). Types 6 and 7 still print `<prefix><id>` alone.

### 12.3 `[present]` (amends 24.7, 25.3, 26.4)

- **Ruins**: `<how>` gains `settlement_overrun` → `overrun by <monster ref>`, and for `settlement_abandoned` and `settlement_abandoned_to` the word ` after plague` or ` after famine` after `abandoned` when the settlement's `ending` is 3 or 4 (`abandoned after plague, survivors to Gnokk Pit (s40)`). The region reference is followed by `, then called <name>` when the region's name at the `ruined_year` differs from its present name (21.2, the critic's finding 7): `s51 Edadbury in The Ruin of Huldbury (rg54), then called The Cruel Barrows: ruined 5778, e374 sacked by …`.
- Appended at the end of `[present]`, **full only**: `beasts: <plural> <n>, …` — the world totals at E (the last row of each (region, beast) summed), species in ascending id, every species listed (`beasts: bears 91, boar 1204, deer 3310, giant spiders 120, wolves 588`).

### 12.4 `[monsters]` (new block after `[present]`)

```
[monsters]
monsters: <n>; awake <a>, asleep <s>, slain <k>, died <d>
  m<id> <name>: <kind name>, <awake|asleep|slain by <civ ref>|died>, lair <site ref> in <region ref>, since <born> e<born_event>, raids <r>, killed <x>, hoard <h>[, ended <died> e<death_event>]
  and <x> more
```

`raids` and `killed` are the number of its `monster_raid` events and the sum of their `dead`. **Full**: one line per monster in ascending id. **Summary**: the monsters alive at E (status 1 or 2), ordered by (`hoard` descending, id), at most `SUMMARY_MONSTER_LINES` (**2**, a constant of section 8), then `  and <x> more` when more are alive (no such line when none are left out).

### 12.5 `[sites]` (new block after `[monsters]`)

```
[sites]
sites: <n>; open <o>; lairs <l>, outposts <p>
  si<id> <name>: <lair|outpost> in <region ref> at <x> <y>, since <origin_year> e<origin_event>[, of <founder ref>][, at <settlement ref>]; <open|ended <ended_year> e<ending_event> <ending word>>
```

Ending words: 1 `slain`, 2 `died`, 3 `moved`, 4 `lost`, 5 `fell`, 6 `quiet`. Full: every site, ascending id. Summary: the count line only.

### 12.6 `[stats]` (appended at the end of section 9's list, full only; `SUMMARY_STATS` unchanged)

In this order: `beasts`, `monster_kinds`, `beast_populations`, `monsters`, `sites` (rows; also `[history]` keys), `monsters_alive`, `monsters_slain`, `monsters_died`, `monster_raids`, `monster_killed` (Σ raid `dead`), `settlements_overrun`, `undead_risen`, `outposts`, `outposts_open`, `beasts_bold`, `settlements_left` (settlements with `ending` 3 to 5), `regions_shunned` (regions with an open lair whose `settlement` ≠ 0 at E). Every `events_<key>` line of the new kinds appears by itself (10.3 rule 1).

### 12.7 Notes (amends 21.4 item 2)

A `flare_up` whose first cause is a `monster_appeared` gets the note `The waking of <monster>, <noun>, stirred the land.` (names as in a sentence's middle).

### 12.8 Length

At summary the report grows by at most 1 + 1 + 2 + 1 = 5 lines of `[monsters]`, 2 of `[sites]`, and the phase lines of the flare-ups a great monster starts (about 0.3 a world). The summary event set keeps its budget of 250 (12 to 15 new candidates per default world, none in `civs4`). Expected: default median about 595 (0.22.0: 587), `civs4` median about 1,093 (1,085): within T18, with little room in `civs4` (question 4).

## 13. Timeline and viewer hooks (amends P4-01; r4)

`load_history` takes the new tables from `read_history_tables`. `Timeline` gains, without viewer parts:

```cpp
struct SiteState    { std::int64_t site; std::int64_t kind, region, x, y, settlement, monster, founder; bool open; std::int64_t ending; };
struct MonsterState { std::int64_t monster; std::int64_t kind; bool alive, awake; std::int64_t lair_site, region; };
std::span<const std::int64_t> sites_existing(std::int64_t year) const;   // sites with origin_year <= Y, ascending id
SiteState site_at(std::int64_t site, std::int64_t year) const;           // open iff ended_year NULL or Y < ended_year
MonsterState monster_at(std::int64_t monster, std::int64_t year) const;
std::int64_t beast_count(std::int64_t region, std::int64_t beast, std::int64_t year) const;  // the row with the largest year <= Y, else 0
```

- **Monster state at Y** (half-open, the state at the end of Y): alive iff `born` ≤ Y and (`died` NULL or Y < `died`); awake from the last of its events of kinds 44 to 52 with year ≤ Y: 44, 45, 46 awake; 47, 51 asleep; the lair at Y is its site with `monster` = m and `origin_year` ≤ Y < `ended_year` (or open). Built in one pass over the events of these kinds (by stored key, P3-07 21.2) and the site rows; answers by binary search over per-monster transition years, O(log k).
- **Settlement ending at Y**: `SettlementState` gains `ending`: 0 while standing at Y, else `settlement.ending` (8.3).
- Fatal texts for an unknown id or a year outside 0..E: those of P4-01 5.1.
- **Dumps** (P4-01 13.1): the static dump gains `site`, `monster` and `beast` lines; the year dump gains per year the open sites and the living monsters with their awake flag.
- **For P4-04** (markers): the sites existing at Y with their state, the living monsters' lairs, and the dead settlements with their ending, which is what the map draws.

## 14. Invariants

1. Counts: 0 ≤ B ≤ 10^6; `beast_population` rows change count from row to row; a species with K0(r, s) = 0 in every year has no row for r.
2. 800 ≤ g(r) ≤ 1100 with the shipped tuning (1000 − `drag_max_permille` ≤ g ≤ 1000 + `feed_max_permille`).
3. At any year at most one open lair per region; living monsters ≤ L_world; per kind ≤ `max_alive`.
4. Every monster has exactly one appearance (kind 44 or 45, its `born_event`); every monster event after it is between `born` and `died`; at most one `settlement_overrun` per monster; a slain monster has exactly one `monster_slain`, a dead one one `monster_died`, and no event after.
5. Every raid target lies within `range_hops` of the lair at the raid's year; `dead` ≤ min(`kill_max`, P − 1).
6. Every lair of a living monster is open, every lair of an ended monster is ended in the monster's death year; a site's `settlement` ≠ 0 only for a lair of an overrun or a rising at a ruin, and that settlement is ruined or faded from the site's `origin_year`.
7. An overrun settlement is never its owner's capital or last standing settlement at the overrun; no monster overruns more than one; overruns ≤ a quarter of the world's settlements (P5-00 8.3, hard).
8. No founding (`colony_founded`) targets a shunned region (7.2).
9. Every outpost lies in a region its founder owned at `origin_year`; an open outpost's founder is alive and owns its region at E.
10. `settlement.ending` equals 8.3's derivation for every settlement.
11. Every event of this stage satisfies 10.1's roles and causes; W9 holds in every quiet step.
12. With the `zero` tuning, I0 (7.5) holds.
13. P3's hard bounds (P3-00 12.3), "no race dies out" and the shelter rules hold.

## 15. Determinism notes (for the auditor)

- Keyed draws only, fifteen new streams (3), keys of ids (region, kind, monster, civilization, settlement, site), w0 the step's year; no key is a position in a list. A draw happens only after its preconditions, and its key never depends on how many draws came before.
- Iteration: regions, species, kinds, monsters, civilizations, sites in ascending id; the log in ascending id; undead windows in event order. No unordered container, no pointer key. Hop distances by breadth-first search over `GeoView` neighbours in ascending order.
- Arithmetic: int64 with checked multiplication, no 128-bit type. The largest product, B × growth × (K − B), is below 10^6 × 1000 × 10^6 = 10^15 by the bound on B and K0 (5; real counts are below 10^4). 7.1's numerator P × g × G × (C − P) is checked like 9.1's; `chance_ppb` scales by 10^9 as 3 states.
- Cells: one scan of the region's cells in row-major order per lair or outpost (a few tens per world), comparisons on integer layer values.
- Names: the shared registry, byte comparisons; roots by prefix and suffix compare.
- The subsystem writes `State` only through `set_population`, `note_reduction` and `ruin`.

## 16. Targets (validation)

At 1024 defaults (100 worlds, seeds 0–99) unless stated. Bounds marked "interim" are set from estimates, not a sweep (no 0.23.0 sweep exists before implementation); the engineer's calibration sweep (17 step 9) may change the shipped rates in data to meet them, and reports the measured figures in a spec revision, as P5-12 r2 did.

| Target | Definition | Bound |
| --- | --- | --- |
| **M1** | monsters per world (rows) | median 4 to 8, max at most 20; at least one in ≥ 95 worlds (interim) |
| **M2** | participation (P5-00 8.3) | each kind in ≥ 10% of worlds and in at least one event over the sweep; each kind at least once across the `low` sweep |
| **M3** | worlds with at least one `monster_raid` | ≥ 90 |
| **M4** | monster-held places | a living monster's lair at E in ≥ 90 worlds; a lair on a settlement (overrun or risen at a ruin) open at E in ≥ 50 worlds; overruns per world median 1 to 3 |
| **M5** | slain among ended monsters | 20% to 70% of the monsters that end are slain (reported by kind) |
| **M6** | invariants 3 to 7 | every world of every sweep (hard) |
| **G1** | ghost places (settlements with `ending` 3, 4 or 5) | median ≥ 3 per world (0.22.0: 0.5) |
| **G2** | standing settlements at E; world population at E | medians at least 95% of 0.22.0's (`settlements` and `population` of `[stats]`) |
| **G3** | once-held regions free at E | reported; expected median 2 (0.22.0: 1) |
| **S1** | outposts per world; ended outposts | median 3 to 15; at least one ended in ≥ 70 worlds |
| **B1** | `beasts_bold` per world; settlement-years with g < 1000 and > 1000 | median at most 30; reported |
| **F1** | flare-ups started by a monster | reported; at most 2 in any world |
| **R** | P5-12's R1 to R4 with rule 7; `ravage` renamings | R1 to R4 as bounded there; `ravage` at least 5 times over the sweep |
| **P** | P5-01's T1, T2, T5, T9 to T15, T17, T19, T20, quick aim wins, T3, T4, T6, T7; P3-00 12.3 | as bounded at 0.22.0 (statistics, not pair for pair, 7.5) |
| **T16** | battles that repeat an earlier battle's sides and both tolls exactly | **at most 2 per thousand battles** of the default sweep (Paul's decision of 2026-11-23; 0.22.0: 13 of 8,935, 1.5‰); mixed toll clauses 0 |
| **T18** | summary lines | default median ≤ 700, max ≤ 800; `civs4` median ≤ 1,100, max ≤ 1,400 |
| **timing** | `generate` at 1024, release, seed 0, three runs | at most 10% slower than 0.22.0 (about 0.95 s); `report --history` under 2 s |

**Which earlier figures may move, and by how much** (reported, not targets): wars per world and simulated years by up to 5% (monster flare-ups add about 15 active years a world); ruins (+1 to +4 a world from overruns and ghost places); free regions at E (+1); renamed regions per world (R1 median +0 to +1, by `ravage`); T17's calamity share (unchanged: ghost abandonments are not calamity kinds, but their abandonment lines (55) enter the summary); T5, T9 to T12 (orcs) within their run-to-run spread, since monsters prefer the uplands and moors where goblins and orcs live and slow their small settlements most.

Sweeps (P5-00 8.2): the default 100; 20 `civs4`; `calm`, `turbulent`, 512 seeds 0–19, 2048 seeds 0–4; and `--monster-pressure low` and `high` seeds 0–19 at 1024. Reported per set: M1 to M5, G1 to G3, S1, B1, F1.

## 17. Order of implementation and roles

One branch, one engineer, one test writer, working separately from the specs. `GENERATOR_VERSION` 0.23.0 and `SCHEMA_VERSION` 9 in step 1; goldens re-recorded once, in step 11.

| Step | What | Spec |
| --- | --- | --- |
| 1 | Schema 9: DDL, `SCHEMA_VERSION`, the empty world's pages, `schema.md`, `[tables]`, `hash --detail`; readers refuse 8 | 11.1, 11.3 |
| 2 | Data: kinds `beast`, `monster` (loader, checks, shipped files and both fixture roots), `history.monsters`, `site_forms` in every culture, peoples' `ghost_*`, renaming rule 7 and the forms of 21.2, `history.report` entries | 4, 9.3, 10.2, 21.2 |
| 3 | Catalogue kinds 44–55; form `kind`, `{@beast}`, display names of 8 and 9, W4 | 10 |
| 4 | Names: kinds `monster`, `site`; lair names; `realm_root` over monster forms | 9 |
| 5 | Seams: `WildView` in peoples (growth, shunned), the ghost rule, `StepContext::phase_trigger_event`, conflict's flare rule | 7, 8.2 |
| 6 | `MonstersSubsystem` (beasts, monsters, sites, tension), its streams, `HistoryResult` members and writers; `settlement.ending` | 5, 6, 8.3, 11.2 |
| 7 | Renaming: `name_role` monster, `{stem}`, the possessive rule | 9.3, 21.2 |
| 8 | Reader: 11.4 (with 21.3); `HistoryView`; `Timeline` reads | 11.4, 13 |
| 9 | Calibration sweep: the shipped rates against M1, M4, G1, S1 (data only); results in a spec revision | 16 |
| 10 | Report v4 | 12 |
| 11 | Goldens (18), `world-hashes.sh` case `p5-monsters-high-1024` (build engineer), README and CLAUDE.md (lead) | 18 |

- **Engineer**: steps 1 to 10; tell the viewer engineer the new tables and 13's reads; keep `gdext-headless-check` green.
- **Test writer**: section 19, from the specs only.
- **Determinism auditor**: 15, and the seams of 7.
- **Validation**: 16 on its sweeps, the cross-preset comparison, timing.
- **World critic**: 20's reading list.

## 18. Golden values

**Every world file changes**: schema 9, `GENERATOR_VERSION`, `content_hash` (new kinds, tuning, culture field, report entries, renaming rule). The empty world grows (11.1).

| Golden | Re-recorded | By | What moves |
| --- | --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`; `GOLDEN_CASES`, `stages_pipeline_test.cpp`; README's phrase hash | yes | validation, after the cross-preset comparison | every case |
| `core_vectors.txt`, `p1-01` … `p1-06` | no | — | must not move |
| `p3-01-data.txt` | yes | test writer | content hashes (new kinds) |
| `p3-02-names.txt` | yes | test writer | the `site_forms` field (no drawn name of the existing kinds changes: forms are per kind) |
| `p3-03-chronology.txt` | no | — | must not move (chronology reads no new data) |
| `p3-04-engine.txt` | yes | test writer | catalogue rows (44–55), the new tables empty in stub worlds |
| `p3-05-peoples.txt` | yes | test writer | the ghost rule; `settlement.ending` (peoples-only configuration: no monsters installed) |
| `p3-06-conflict.txt`, `hist-conflict-73-1024` | yes | test writer | every history (the full configuration) |
| `p3-07-report.txt` | yes | test writer | every report (v4) |
| `p4-01-timeline.txt` | yes | test writer | dumps gain sites, monsters, beasts |
| `p5-12-renaming.txt` | yes | test writer | every renaming (21.2, rule 7) |
| **`p5-02-monsters.txt`** (new) | new | test writer, `worldgen/tests/reference/p5_02_monsters_reference.py` | per world case and fixture: the hashes of `beast_population`, `monster`, `site`, `settlement.ending`, and the counts of 16's M and G figures |

`world-hashes.sh` gains the case **`p5-monsters-high-1024`** (`--seed 7 --map-size 1024 --monster-pressure high`; P5-00 5): sixteen cases.

## 19. Tests required

### 19.1 Unit (C++, new shard `worldgen-tests-monsters` for `*monsters_*`)

1. Data: every field's bounds and absence for `beast`, `monster`, `history.monsters`, `site_forms`, `ghost_*`; each named check's message.
2. Beasts: H, K0 with F for predators; the yearly step at B < K, B > K, B = K, B = 0; d(r) capped; g(r) at the caps; Q1 equals L years of T1 with P fixed; rows at year 0, at a phase change and at `finish`, none for an unchanged count.
3. Boldness: the hysteresis (on at 1500, stays bold at 900, off at 799); the toll and its cap at P − 1; the species chosen.
4. Appearances: the caps (world, kind, one per region, one per step); `chance_ppb`'s threshold at p = 0, 1, 10^9; islets skipped; quiet steps with and without budget; `pending_flare` set in Q4 only for `flare` kinds and cleared at the next Q4; `tension()` pure (called twice, same answer).
5. The undead: the window (an entry at Y − window drops, at Y − window + 1 stays); battle and sack magnitudes; the 8 causes by size then id, listed ascending; the lair at a ruined sacked settlement, and at the wild cell when that settlement already holds a site.
6. The monsters' year: the order age, rewake, sleep, raid, hunt; `drive_until`; raid targeting (hops, then population, then id); toll bounds; plunder and hoard; the hunt odds at S = 0, S = might, the cap; slain, driven, nothing by u at the thresholds; hunting realms by memory.
7. Overrun: every condition (kind, capital, last settlement, another monster's lair, once only); the new site, the old lair's ending 3; `ruin`'s effects; the region shunned and then no colony into it; the shun lifted when the monster dies.
8. Outposts: raised only at war, on the height, one per civilization per year, `outpost_max`; each ending reason and its cause; quiet-step budget.
9. Names: monster names per kind (culture, forms); roots with the bare form last; lair names cycling, the possessive rule, the fallback numeral; outposts from `site_forms`; all in the one registry.
10. Seams: g = 1000 gives P3-05 9.1's numbers exactly; the forced flare declaration with a claim trigger and none with a monster trigger.
11. Ghost rule: each condition; the outbreak's plan includes it (planned total equals the real one).
12. `settlement.ending`: each code from fixture logs.
13. Catalogue and rendering: kinds 44–55; `{monster:kind}`, `{@beast}`, their `template_problem` messages; display names of 8 and 9; the names-inside rule on `The Host of …`.
14. Reader: each message of 11.4 on a corrupt fixture; A1 and A2 of 21.3.
15. Report v4 (12) on fixture H13 (`h13-v4.toml`: two monsters, one slain, one holding an overrun settlement; two outposts, one left; a ghost place of each ending), both levels; `[stats]`.
16. Timeline: `site_at`, `monster_at`, `beast_count`, `ending` at years around each transition.

### 19.2 Property and identity

- **I0** (7.5): the `zero` tuning against the subsystem removed, on the fixture worlds and seeds 0–4 at 512 (default suite) and seeds 0–4 at 1024 (`*-slow`).
- Invariants 1 to 13 on seeds 0–9 at 512 (default suite) and 0–19 at 1024 (`*-slow`), and on `--monster-pressure high` seeds 0–4.
- Resume (P3-04 16.3): a world resumed from `--until chronology` equals the generated one.

### 19.3 Reference and fixtures

`p5_02_monsters_reference.py` on `p3_conflict_reference.py` and `p5_12_renaming_reference.py`: the subsystem of sections 6 to 8 inside the conflict reference's replay (the log and ids depend on it), and a `check-world` mode for invariants 1 to 11 and 11.4's checks. The conflict reference's schema gate becomes 9 (P5-12 validation note B). Fixture roots gain the new data; a hand-made root `monsters-small` (the conflict stress data with every monster `wake_rate` raised, `living_permille` 1000 and `overrun_permille` 1000) makes every kind appear, rise, raid, overrun, sleep, wake, be slain, be driven off and die on small maps.

## 20. For P6, and for the world critic

**For P6** (P5-00 10.2). P6 turns these rows into places without replaying the simulation: every `site` with its kind, cell, founder or monster, years and ending (lairs of living monsters: dungeon origin type "lair"; ended lairs: an empty den; outposts: a ruined watchtower); every dead settlement with its ending (a burned town, a plague village left standing, a monster-held hold) and `ruin_event`; every monster's hoard, kill list (its raids in the log) and state at E; the beast counts per region at E (placement of wildlife). Quest hooks from the end state: a living monster's lair within `range_hops` of a standing settlement ("a lair near a village", design 5), a shunned region, a hoard never taken.

**For the world critic** (0.23.0 sweep; default seeds 0, 5, 42, 72, 84; `civs4` 1 and 3; `high` seed 7): the monster names and lair names by kind (dark, never comic; `Old Grakh`, `Ogre the Glutton` forms), the appearance and slaying sentences, whether a monster's career reads as a story (appeared, raided, overran, was slain), the overrun and ghost places in `[present]` and `[sites]`, the `ravage` renamings, the boldness lines at full, and whether monsters crowd or vanish from the summary.

## 21. Carried over from P5-12 (reviewed apart)

These change world bytes or report text and ride with this stage. Sources: `docs/reviews/p5-12-renamed-regions-world-critic.md` (findings 1 to 8), `docs/reviews/p5-12-renamed-regions-audit.md` (A1, A2), `docs/reviews/p5-12-renamed-regions-validation.md` section 5 C (T16).

### 21.1 Decisions on the world critic's findings

| Finding | Decision | How |
| --- | --- | --- |
| 1 settlement names with a space in `{name} Waste`, `{name}'s Fall`, `{name}'s Pyre` | **adopt** (strip) and **adapt** (drop `{name} Waste`) | new slot `{stem}` (21.2) in the possessive forms; `{name} Waste` replaced; `Ashes of`/`Ruin of` keep the whole name |
| 2 the heartland cause clause | **adapt**: the `{@causes}` grammar holds fixed words only, so the realm cannot be named there; the word becomes `the rise of a realm's seat` | 21.2; a report note naming the seat is deferred (22) |
| 3 a name ending in `s` | **adopt**: `'` alone | 21.2 possessive rule |
| 4 `{name}'s Bane` | **adopt**: `The Rout of {name}` | 21.2 |
| 5 more variety in `slaughter` and `plague` | **adopt** for forms (two more each); **defer** more nouns per land (the land noun is P1-06's `region_noun`, one per biome; a list per biome is a geography data change) | 21.2 |
| 6 timing late | **open question** (Paul's taste; 23 item 1); no change | — |
| 7 ruins list and the former name | **adopt** for the ruins list (`, then called <name>`); civilization blocks unchanged | 12.3 |
| 8 the sack share | **closed by Paul** (2026-11-23): `capital_sack` keeps `chance_permille` 700 | — |

### 21.2 Renaming data and rules (amends P5-12 4, 4.1, 5.1)

- **Slot `{stem}`** (P5-12 4's `rename_form` grammar gains it, at most once, not together with `{name}`): for `name_role` `settlement` (or a settlement role), the settlement's name without a trailing **separate-word** suffix of its founder's culture: the first form `{root}`B of the culture's `settlement_forms` (list order) whose B starts with a space and such that the name ends with B and is longer than |B|; the name without B; else the whole name (`Nub Pit` → `Nub`, `Krax Hole` → `Krax`, `Bugg-Drux Pit` → `Bugg-Drux`; `Edadbury` stays, fused suffixes are never stripped). For any other `name_role` `{stem}` = `{name}`'s value. V8 treats `{stem}` as a slot value.
- **Possessive**: when a slot value ends in `s` or `S` and the literal run right after it starts with `'s`, that literal's `s` is dropped (`Omos' Fall`, `Iersaes' Fall`). Applies to renamed names and lair names (9.2).
- **Forms** (normative data; `game/data/history/renaming.toml`):
  - `capital_sack`: `{stem}'s Fall`, `The Ashes of {name}`, `The Ruin of {name}`, `The Fallen {noun}`;
  - `great_sack`: `{stem}'s Pyre`, `The Ashes of {name}`, `The Burnt {noun}`, `The Cinder {noun}`;
  - `slaughter`: `The Red {noun}`, `The Bloody {noun}`, `The Rout of {name}`, `The {noun} of the Slain`, `The Weeping {noun}`, `The Grim {noun}`;
  - `plague`: `The Emptied {noun}`, `The Fever {noun}`, `The Silent {noun}`, `{stem}'s Grave`, `The Pale {noun}`, `The Hollow {noun}`;
  - `conquest`, `heartland` unchanged; rule 7 `ravage` (9.3).
- **Cause words** of `region_renamed` (data, `history.report`): `became_city=the rise of a realm's seat` (was `the rise of a city`), and `settlement_overrun=the ravage` appended.

The engineer checks that every new form word passes the placename `forbidden` and `excluded` lists (P5-12 5.2 item 3); a word that fails is a data defect to report.

### 21.3 Audit advisories (amends P3-07 26.6)

- **A1**: `region_name` rows that do not strictly ascend in (region, from_year): reason `not above the previous row`, the first reason checked. `Timeline::region_name`'s binary search then has a partitioned range on every accepted file.
- **A2**: the check that every `region_renamed` event has a row is linear: one pass over the rows marks `event` in a `std::vector<std::uint8_t>` indexed by event id (bounds-checked), then one pass over the events of kind 43 tests the mark. The same form serves 11.4's site and monster checks.

### 21.4 T16 (amends P5-01 8)

**T16 is a rate (Paul's decision of 2026-11-23)**: at most **2 repeated battles per thousand battles** of the default sweep; the mixed-toll half stays 0. At 0.22.0, 13 of 8,935 (1.5‰) meets it. *A rule that would remove the coincidences, not specified here*: eleven of the 13 repeats are two battles of one war with small, equal tolls, because the same two levies meet again within a few years and the toll spread (`hist.conflict.toll`) is drawn over a small range; drawing the spread again with the next k when the pair (winner dead, loser dead) equals the war's previous battle's pair, at most a few times, would make an exact repeat within a war impossible except when the range has one value. It would change world bytes and is P3-06's to specify if Paul wants it.

## 22. Left for later

- Predators eating prey, and hunting that depletes game (a second coupling); beasts by settlement rather than region.
- Heroes as slayers, costly failed hunts, slayers named (P5-06); a monster slain by a figure is a new kind then (P5-00 4.4).
- The deep horrors and delvings (P5-07): rows of `monster` with a new `wake` value; lost holds as sites with their own ending codes.
- Gods sending monsters (P5-08: `divine_monster` creates a monster through this subsystem's appearance procedure).
- Mines and forges (P5-04), towers and libraries (P5-09) as `site` kinds with their own ending codes.
- Monster raids that start wars between realms, monsters allying with evil peoples, a hoard taken by a slayer (P5-04's wealth).
- The heartland renaming note naming the seat (21.1 item 2); more land nouns per biome (finding 5).
- Sites P6 derives (battlefields, barrows, tombs), dungeon records, `layout_seed`.

## 23. Open questions for Paul (each with a recommendation and the interim choice)

1. **Ancient renamings.** The world critic found renamings late (74% after year 5000): the rules need sacks, battles and plagues that the early, thinly peopled ages rarely reach. Recommendation: in a later balance pass, set the death-toll thresholds of `great_sack`, `slaughter` and `plague` relative to the population of the time (for example per mille of the world's population at the event), so an ancient massacre of a small world counts as a great one. Interim: no change in this stage.
2. **A monster's flare-up without a war.** A great monster's waking in a quiet age now starts a flare-up, which forces no declaration (7.4). Recommendation and interim: as specified; the design summary's "a flare-up always brings its war" is narrowed to flare-ups of strain (proposals entry). The alternative, that a monster's flare-up also forces conflict's best claim, ties monsters to wars without a cause in the story.
3. **How dead places are told apart.** Recommendation and interim: an ending column on `settlement` and `site` (8.3) beside the unchanged status. The alternative is a new settlement status `abandoned` (5), which every P3 rule and reader of "ruined" would have to learn.
4. **The `civs4` summary length.** The `civs4` median is at 1,085 of 1,100; this stage adds about 7 summary lines. Recommendation and interim: as specified (2 monster lines at summary). If validation finds the median above 1,100, `SUMMARY_MONSTER_LINES` becomes 0 at summary (a constant, report only), or Paul raises the `civs4` bound, which was set for a world without monsters.
5. **Outposts.** Recommendation and interim: watchtowers on a frontier in war, left when the land is lost, the realm falls or the frontier is quiet for 200 years. The alternative is to leave every non-monster site to P5-04 (mines) and P6, which would leave few abandoned sites outside lairs.

## 24. Proposals

Three entries in `docs/design/proposals.md`: "History: beasts and monsters, P5-02 (readings and scope)", "Schema 9: beasts, monsters and sites (P5-02)" (hard rule 7) and "Paul's wish of 2026-11-23: abandoned places to explore (P5-02)".

## Revision notes

- r0 (2026-11-23): first draft, with Paul's decisions of 2026-11-23 (T16 a rate; the capital sack share kept; abandoned places to explore) worked in.
