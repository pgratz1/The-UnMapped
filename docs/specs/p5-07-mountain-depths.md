# P5-07 Mountain depths

Status: **r2 (2026-11-26)**, for approval; r1 was approved and is built (parts A to D, `docs/reviews/p5-07-impl-notes.md`). r2 adds Paul's answers of 2026-11-26 (21): allies may answer a deep horror (6.11) and goblins delve warrens and wake the deep less often (6.10); it records the build's readings where r1 was silent (6.12) and fixes three text errors. Every changed place is marked **(r2)**. r1's rules are otherwise unchanged. Generator, schema and report format stay as r1 (0.29.0, 13, v10: none has merged). Phase P5 History depth, stage 07 (`docs/specs/p5-00-history-depth-overview.md` 3.1, the P5-07 paragraph and its amendment: "kept, as a small stage on P5-04's mines"; Paul's scope decision Q-P5-00-1 of 2026-11-24). **Base: P5-04a as built**: generator 0.28.0, schema 12 (36 tables), history report format v9, event kinds 1 to 92, 98 streams, browser groups 0 to 8. Generator **0.29.0**; schema **13** (one code-list change, no DDL change, 9); history report format **v10**.

Design: section 5, "The deep is dangerous. Delving deep can wake something that wipes out a hold or a whole realm, leaving a lost hold as a ruin and a site. The game uses its own deep horrors, drawn from the monster data. If the realm held its race's last settlements, the survivors flee as from any last refuge, so no race dies out"; "Dwarves are great forgers"; section 12, living monsters are a highlight of play; section 13, P6's site plans read what this stage records. Builds on P5-04 6.12 to 6.14 (mines, the seam), P5-02 (monsters, lairs, raids, hunts, overruns), P5-03 (the people ledger, fear, raid memory), P5-06 (heroes, the `{r:under}` form, ruler deeds) and P3-06 20.1 (the flight of a last refuge).

## 1. Purpose

A dwarven mine (a `site` of kind 3, P5-04 6.12) goes deeper with the years it is worked: three depth levels, each an event. Depth makes the realm's forge stronger in war and makes the mine dangerous: a mine at depth can wake a **deep horror**, a monster of three new kinds (`wake = "deep"`, TOML data). At the waking the hold either **seals** the horror in (the mine stays open, its depth frozen; the horror sleeps beneath to the end of history) or **falls**: the hold is ruined, the horror lairs in it, the survivors go to the realm's other holds or become refugees. A horror that took a hold raids like any monster and may take up to two more holds of the same realm, so a small realm can be wiped out. A realm that is its race's last flees into hiding as from a sacked last refuge (P3-06 20.1). What remains is a **lost hold**: a ruin (ending 2, overrun by a monster) with a lair on it and an ended mine at the same cell, and a horror alive or dead at E, all recorded for P6's site plans.

(r2) Goblins delve too: a goblin realm digs one **deep warren** under a settlement in upland or ore country, more slowly than dwarves mine, with no site and no forge (so no arms and no force in war), and wakes the same horrors less often. At a waking, an ally of the realm with a standing alliance may **send aid** to the gate: its warriors add to the gate's strength, the chronicle says so, and the ally's people are remembered warmly. No war starts and no alliance rule changes.

## 2. Items

| # | Item | In short | Section | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Depth | three levels by worked years and a keyed yearly chance; one event per level | 6.2 | yes |
| 2 | What depth gives | the forge's strength in war rises with the deepest open mine; the wake chance rises with depth | 6.3 | yes |
| 3 | The waking | per tick year at depth, at most one waking a step; the horror's kind by a keyed weighted draw | 6.4 | yes |
| 4 | The gate | sealed or fallen, by the hold's people against the horror's might | 6.5 | yes |
| 5 | The take (a hold lost) | ruin, toll, survivors by the ledger; the last refuge flees | 6.6 | yes |
| 6 | The horror's years | raids, up to two more takes of the waking realm, hunts, heroes, sleep | 6.7 | yes |
| 7 | Meeting the existing rules | last refuge, alliances, heroes, renaming, refugees, undead, caps, P3-04 6.9 | 6.9 | — |
| 8 | Carried items | audit A1 (taken), A2 (recorded), `p3_conflict_reference.py` version, two spec text corrections (done), `allies:` entity segments, critic F2 to F4 (taken), F1 and F5 (left) | 12 | A1 yes, rarely; the rest report only |
| 9 | (r2) Allies answer | at a waking, allies with a standing alliance and a settlement near the hold may send aid: a keyed chance per ally, the aid adds to the gate's strength; new kind 97 `ally_aided_hold`; goodwill by `credit`; no refusal event, no grudge, no war | 6.11 | yes |
| 10 | (r2) Goblin deep warrens | race field `delving_permille`; one warren per realm, depth on the settlement by events (no site, no forge); wakings at a lower rate; kinds 94 and 96 gain goblin wordings; new kinds 98 `warren_delved`, 99 `warren_sealed` | 4.5, 6.10 | yes |
| 11 | (r2) Readings of the build | the forced seal's plan, the later-take count, `hold_lost`'s mine, the in-memory `depths-small` content, and the rest of the build notes | 6.12 | no (as built) |

## 3. Inputs, streams, draws

As P5-02 3, P5-04 3 and P5-06 3. Read: the open sites of kind 3 (mines) and their towns, founders and `origin_year` (`MonstersSubsystem`'s own site list, P5-04 7.16); the founder race's `mining_permille` (P5-04 4.1) and `prowess_permille` (P3-06); M = `history.monsters.pressure` by `param.monster_pressure`. (r2) Also: the race fields `delving_permille` (4.5) and `ally_join_permille` (P5-04 4.1); the settlements' owner, population, region and `owner_event` (P5-03); the regions' `cells`, `walk_cells` and `ore_reach_cells` (P1-06 3.2) and their borders; a realm's allies through `AllianceView::allies` (P5-04 7.1).

**Streams**, appended to `core/streams.hpp` after `hist.alliances.renew` (the 98th), in this order (r1 **103** streams; (r2) **107**):

| Stream | Key (a, b, k) | Drawn |
| --- | --- | --- |
| `hist.deep.delve` | site, depth to reach (1 to 3), 0 | once per mine, level and step at most (6.2) |
| `hist.deep.wake` | site, 0, 0 | once per mine and tick year at most (6.4) |
| `hist.deep.kind` | site, 0, 0 | once per waking (6.4) |
| `hist.deep.gate` | monster, 0, 0 | once per waking (6.5) |
| `hist.deep.take` | monster, settlement, 0 | once per raid of a woken horror on a settlement of its waking realm (6.7) |
| (r2) `hist.deep.aid` | monster, ally civilization, 0 | once per waking and eligible ally (6.11) |
| (r2) `hist.deep.warren_delve` | settlement, depth to reach (1 to 3), 0 | once per warren, level and step at most (6.10) |
| (r2) `hist.deep.warren_wake` | settlement, 0, 0 | once per warren and tick year at most (6.10) |
| (r2) `hist.deep.warren_kind` | settlement, 0, 0 | once per warren waking (6.10) |

(r2) The warren streams are separate from `hist.deep.delve`, `.wake` and `.kind` because their first key is a settlement id, which can equal a site id.

All through `ctx.draw` (the step's year is part of the draw, P3-02 4.1). History code uses keyed draws only. The monster's name (`hist.names.monster`, P5-02 9.1) and its lair's name (`hist.monsters.lair_name`, P5-02 9.2) use the existing streams with their existing keys.

# Rules

## 4. Data

### 4.1 Monster field `wake` gains `deep` (amends P5-02 4.2)

`wake`: enum(`wild`, `dead`, `deep`). A `deep` kind never appears in the wild and never rises from the dead; it wakes only from a mine (6.4). For a `deep` kind: `biomes` must be empty (new check `monster_deep_biomes`: `a deep monster has no biomes`); `wake_rate` is the kind's **weight** among the deep kinds at a waking (6.4); `overruns` is not read (a take, 6.6, is not an overrun; shipped empty); `overrun_permille` is the chance of a take after a raid (6.7); `flare` is not read (no deep kind wakes in a quiet step). Every other field means what P5-02 4.2, P5-03 9.5 (`raid_phrases`) and P5-06 4.3 (`slayer_epithet`) say.

**The ids sort after `wyvern`.** `monster_kind.id` is the canonical id order (byte order of ids, P0-03 7.4), and the keys of `hist.monsters.wake` and `hist.monsters.rise` hold the kind id. A deep kind whose id sorted before `wyvern` would renumber the six kinds and change every wild appearance and rising of every world. The three ids are therefore the dwarves' own words for these things, all after `wyvern`: dragon 1 … wyvern 6, **`ymrach` 7, `ysgarth` 8, `zhakul` 9** (invariant 8). The display names are the same words; the nouns say what they are.

**Shipped** (`game/data/monsters/ymrach.toml`, `ysgarth.toml`, `zhakul.toml`; taste; starting values for calibration, 15.2):

| id | name | noun | grouped | wake | wake_rate (weight) | culture | name_forms | lair_forms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `ymrach` | Ymrach | a wyrm of the lightless deep | no | deep | 500 | dwarven | `{root} the Deep-Coiled`, `{root} Stonegnawer`, `{root}` | `The Coils of {root}`, `{root}'s Delving` |
| `ysgarth` | Ysgarth | a host of eyeless things from under the stone | yes | deep | 350 | dwarven | `The Eyeless of {root}`, `The Swarm Beneath {root}` | `The Warrens of {root}`, `The Nest of {root}` |
| `zhakul` | Zhakul | a fire that walks in the roots of the world | no | deep | 150 | dwarven | `{root} the Unquenched`, `{root} Flame-Under-Stone`, `{root}` | `The Burning Deep of {root}`, `{root}'s Furnace` |

| id | might | range | raid ‰ | toll ‰ | kill_max | overrun ‰ (take) | active | rewake ppm | lifespan | max_alive | hoard ‰ | raid_phrases | slayer_epithet |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `ymrach` | 4000 | 1 | 60 | 30–100 | 300 | 300 | 60 | 2000 | 0 | 2 | 10 | `came up out of the stone beneath`, `burrowed into the halls of`, `fell upon` | `Deepbane` |
| `ysgarth` | 2500 | 1 | 100 | 20–80 | 150 | 200 | 100 | 3000 | 0 | 2 | 2 | `swarmed out of the dark upon`, `crept up the deep ways into`, `fell upon` | `the Lightbearer` |
| `zhakul` | 7000 | 1 | 40 | 50–150 | 500 | 400 | 30 | 1000 | 0 | 1 | 20 | `came up burning beneath`, `set the deep halls aflame under`, `fell in fire upon` | `Flamebreaker` |

All three are ageless (`lifespan_years` 0): a horror ends only by a hunt. `biomes` = `[]`, `overruns` = `[]`, `flare` = false. Both fixture roots gain the three files (the readers' and the renderer's deep paths are then exercised by the fixtures).

### 4.2 Tuning `history.monsters` (appended after `undead_regions_ref`, in this order, all required; `game/data/history/monsters.toml` and both fixture roots)

| Field | Type | Start | Meaning |
| --- | --- | --- | --- |
| `deep_years` | array(int(1, 100000), 3, 3) | 150, 500, 1000 | worked years (Y − the mine's `origin_year`) a mine needs before it can reach depth 1, 2, 3 |
| `deep_delve_permille` | int(0, 1000) | 10 | yearly chance, once the years are reached, before `mining_permille`; 0: no mine deepens |
| `deep_wake_ppm` | array(int(0, 1000000), 3, 3) | 0, 150, 600 | chance per tick year in ppm that a mine at depth 1, 2, 3 wakes a horror, before M |
| `deep_gate_levy_permille` | int(0, 4000) | 1000 | the hold's defenders at the gate, per mille of its people |
| `deep_seal_max_permille` | int(0, 1000) | 800 | cap on the odds of sealing |
| `deep_toll_permille` | int(0, 1000) | 400 | the share of a taken hold that dies |
| `deep_takes_max` | int(0, 16) | 2 | holds a horror may take after its first |
| `deep_reach_hops` | int(0, 4) | 1 | a later take is of a settlement of the waking realm within this many borders of the lair's region |
| (r2) `deep_warren_population_min` | int(0, 1000000) | 600 | least population of a settlement that starts a warren (depth 0 to 1) |
| (r2) `deep_warren_ore_min` | int(0, 100000) | 1 | a warren's region is upland (4 × `walk_cells` < `cells`, P1-06 3.2) or has `ore_reach_cells` ≥ this; 0: any region |
| (r2) `deep_warren_wake_permille` | int(0, 1000) | 200 | a warren's wake chance per mille of a mine's at the same depth (6.10) |
| (r2) `deep_aid_permille` | int(0, 1000) | 500 | an eligible ally's chance to send aid, before its race's `ally_join_permille` (6.11); 0: no aid |
| (r2) `deep_aid_hops` | int(0, 8) | 2 | an ally is eligible when it has a standing settlement within this many borders of the hold's region |
| (r2) `deep_aid_levy_permille` | int(0, 4000) | 500 | the aid's warriors per mille of that settlement's people |
| (r2) `deep_aid_max` | int(0, 8) | 2 | most allies that send aid to one waking |
| (r2) `deep_aid_memory` | int(0, 500) | 30 | goodwill units credited between the ally's race and the realm's race per aid (P5-04's `join_memory` is 30) |

Check `monsters_deep_years_ordered`: `deep_years` strictly ascending (`deep_years[<i>] <a> is not above deep_years[<i-1>] <b>`). **Code defaults** (a tuning built in code): `deep_delve_permille` 0, `deep_wake_ppm` 0, 0, 0 (inert); (r2) `deep_warren_wake_permille` 0, `deep_aid_permille` 0, the other r2 fields their starts. The `zero` fixture tuning: shipped values but `deep_delve_permille` 0 (I0, 14). (r2) Fixture content **`depths-small`** is in memory, not a root on disk (6.12 item 4): monsters-small (P5-02 19.3) with `deep_years` 5, 10, 20, `deep_delve_permille` 1000, `deep_wake_ppm` 0, 200000, 600000, `deep_takes_max` 3, and for r2 goblin `delving_permille` 1000, `deep_warren_population_min` 100, `deep_warren_wake_permille` 1000, `deep_aid_permille` 1000, so every path of 6, 6.10 and 6.11 occurs in a 512 world.

### 4.3 Tuning `history.conflict` (appended after `arms_strength_permille`)

`forge_depth_permille` int(0, 1000), start **15**: added to the forge's strength per depth level of the realm's deepest open mine (6.3). Code default 0.

### 4.4 Renaming rule 8 `deep` (`game/data/history/renaming.toml`, appended)

| # | key | kinds | values | min | window | min salience | capital | chance ‰ | weight | name_role | namer_role | forms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 8 | `deep` | `hold_lost` | — | 1 | 1 | 0 | no | 700 | 85 | `monster` | `civilization` | `The Deeps of {name}`, `{name}'s Dark`, `The Lost {noun}`, `The Sunken {noun}` |

`{name}` is the horror's root (P5-02 9.3). Weight 85, above `ravage` (75): a hold lost to the deep is a defining event of the region. The caps of P5-12 (`region_max`, `region_gap_years`, `world_max_permille`, `century_max_permille`) apply as to every rule. `region_renamed`'s cause words gain `hold_lost=the fall of a hold to the deep` (8.2).

### 4.5 (r2) Race field `delving_permille` (amends P5-04 4.1)

`delving_permille` int(0, 1000), optional, absent counts 0 (as `mining_permille` is read, 6.12 item 3): the yearly pace of a race's warren delving, as `mining_permille` is of mine delving (6.10). Check `race_delves_one_way`: a race with both `mining_permille` > 0 and `delving_permille` > 0 is refused (`a race delves mines or warrens, not both`). Shipped (`game/data/races/*.toml`, written in every file so the choice is visible):

| race | dwarf | goblin | human | elf | orc | faerie |
| --- | --- | --- | --- | --- | --- | --- |
| `mining_permille` (P5-04, unchanged) | 1000 | 0 | 0 | 0 | 0 | 0 |
| `delving_permille` (r2) | 0 | **300** | 0 | 0 | 0 | 0 |

Orcs: no change (no delving in their data; a later data change can give it them with no code change). The fixture roots `geo` and `geo-small` leave the field out (goblins 0 there), so their worlds do not change by r2's delving.

## 5. State

`MonstersSubsystem` (new file `worldgen/src/history/monsters_deep.cpp`; header unchanged in name) keeps, in its own members, ids only:

- per site of kind 3 (index site id): `depth` (0 to 3), `woken` (bool: this mine has woken a horror; never again), `sealed` (bool);
- per monster: `waking_realm` (the founder of the mine it woke from; 0 for other kinds), `takes` (holds taken, first included), `sealed` (bool; a sealed monster never wakes again);
- a reference-free pointer to `ConflictSubsystem` for the flight seam (7.3), set once by the stage;
- (r2) per settlement (index settlement id): `warren_depth` (0 to 3), `warren_delver` (the civilization of its first `warren_delved`, 0 before), `warren_woken`, `warren_sealed` (bools);
- (r2) per monster: `from_warren` (bool: it woke from a warren);
- (r2) pointers to the `AllianceView` (alliances) and to `RelationsSubsystem` for 6.11, set once by the stage (7.6).

The mines themselves stay `AlliancesSubsystem`'s (P5-04 6.12): openings and closings are unchanged.

## 6. Rules

### 6.1 Steps and order (amends P5-02 6.1)

| Step | What runs, in this order |
| --- | --- |
| T4 | read the log; beasts bold; the monsters' year (6.4 of P5-02, with 6.7 here for deep kinds); **deepening (6.2), (r2) then warren delving (6.10), then one waking at most (6.4 to 6.6; (r2) mines first, then warrens; aid 6.11 inside it)**; one creation at most (the undead, else a wild appearance) |
| Q4 | `pending_flare` = 0; deaths of age; one wild appearance at most; outposts left; **deepening (6.2), (r2) then warren delving (6.10)** |

A waking is not a creation in P5-02 6.3's sense: it neither uses nor blocks the step's one creation. Wakings, gates, takes and flights happen in tick years only; deepening also in quiet steps.

### 6.2 Depth (T4, Q4)

For each site s of kind 3, ascending id, that is open, not `sealed`, with `depth` d < 3, founder c alive and visible, town t = `site.settlement` standing, visible and owned by c, and `mining_permille`(r_c) > 0:

```
w = Y − origin_year(s)                                     // worked years
if w < deep_years[d]: next s                               // index d: the years for depth d + 1
p = deep_delve_permille × mining_permille(r_c) div 1000    // T4
p = min(1000, p × L)                                       // Q4
Q4 only: quiet_events_left(c) > 0, else next s (it waits; no draw)
if chance_pm(D(hist.deep.delve; s, d + 1, 0), p): append delving_deepened; depth(s) = d + 1
```

**`delving_deepened`** (region of s, settlement t; participants c `civilization`, s `site`; values `depth` = d + 1, `telling` = (s + d + 1) mod 3; cause the site's previous `delving_deepened`, else its `mine_opened`). At most one level per mine per step. A sealed mine never deepens; a closed mine keeps its last depth on record.

With the starts and dwarves' `mining_permille` 1000, depth 1 comes about 250 years after the opening, depth 2 about 600, depth 3 about 1,100 (each minimum plus an expected 100 years at 10 per mille).

### 6.3 What depth gives

Wealth is not a number in this build. Depth feeds three things:

1. **The forge** (amends P5-04 7.5 and P3-06 7.3's force): for a seller c, D(c) = the largest `depth` among c's open mines (sealed included), read live through `AllianceView::forge_depth(c)` (7.1). The force factor becomes

   ```
   f = f × (1000 + forge_strength_permille + forge_depth_permille × D(c)) div 1000     when forged(c)
   ```

   (was `1000 + forge_strength_permille`). With the starts a realm whose deepest mine is at depth 3 fights at +9.5% instead of +5%. Arms bought by partners (`arms_strength_permille`) do not change with depth.
2. **The danger**: `deep_wake_ppm[d − 1]` (6.4).
3. **The record**: each level is an event naming the site; P6's site plans read the deepest level of a mine as the depth of its plan (20), and P5-11's makers may read a depth-3 forge (deep metal) as a seam.

### 6.4 The waking (T4, after the deepening)

Candidates: sites s of kind 3 ascending id, open, not `woken`, not `sealed`, depth d ≥ 1 with `deep_wake_ppm[d − 1]` > 0, founder c alive and visible, town t standing, visible and owned by c. For each, while no waking has happened in this step:

1. K = the kinds with `wake` = `deep` whose living count is below `max_alive`, ascending id. K empty: next s (no draw).
2. p = min(10^6, `deep_wake_ppm[d − 1]` × M div 1000). If not `chance_ppm(D(hist.deep.wake; s, 0, 0), p)`: next s.
3. k = K[`weighted(D(hist.deep.kind; s, 0, 0), [wake_rate_k for k in K])`] (P3-02 4.1's `weighted`); a weight of 0 is never chosen; all weights 0: next s.
4. The monster m = the next monster id, its name (P5-02 9.1, the kind's `naming_culture`), its root (P5-02 9.2), `waking_realm` = c. (r2) The allies' aid A (6.11, its draws) and then the **gate** (6.5) are decided now, before any event, since the gate fixes the lair's settlement.
5. The lair l = the next site id (after m's name; `next_site_id()`), kind 1, at t's cell, `settlement` = t when the gate falls, 0 when it holds; its name by P5-02 9.2 (the kind's `lair_forms`); `founder_civ` 0, `monster` m, origin Y and the event below.
6. Append **`deep_woken`** (region of s, settlement t; participants m `monster`, c `civilization`, s `site`, l `lair`; values `depth` = d, `telling` = m mod 3; cause the site's last `delving_deepened`); create l and m (awake; born Y and the event; lair l; waking event the event; hop distances from the region). `woken(s)` = true. (r2) Then one **`ally_aided_hold`** per ally in A, ascending civilization id (6.11).
7. The gate's outcome: sealed (6.5) or the take (6.6) with cause `deep_woken` (r2: causes the `deep_woken`, then the `ally_aided_hold` events, ascending).

(r2) The same steps run for a warren (6.10) after every mine candidate, while no waking has happened in the step; 6.10 lists what differs.

**Caps.** A waking is limited by its kind's `max_alive` only. Deep kinds are not counted in, and not limited by, the world cap L_world of P5-02 6.3, and a waking does not need the region to be free of other lairs (the deep lies under the mountain, apart from the wild). The deep lair does count for every other rule that reads lairs: it is a lair for "has a lair" (no wild appearance or rising in its region while it is open), for an overrun's "no open lair of another monster lies in t's region", and, when on a settlement, for the shunned regions of P5-02 7.2. So the wild and undead rules are unchanged except where a deep lair stands.

### 6.5 The gate

```
S    = P_t × deep_gate_levy_permille div 1000 × prowess_permille(r_c) div 1000
S    = S + Σ_{a in A} S_a                                                // (r2) the allies' aid, 6.11; A empty: unchanged
odds = min(deep_seal_max_permille, S × 1000 div (S + might_k))          // S = 0: odds 0
u    = pick(D(hist.deep.gate; m, 0, 0), 1000)
u < odds: sealed;  else: the hold falls (6.6)
```

The draw is always made. With the starts a dwarven town of 3,000 (S = 4,200) seals an Ymrach at 512 per mille and a Zhakul at 375; a village of 800 (S = 1,120) at **218 and 137** (r2: r1 printed 219 and 138; `div` is floor, 1,120,000 div 5,120 = 218 and 1,120,000 div 8,120 = 137; the formula stands). With a human ally's town of 2,000 within reach (S_a = 1,000) the village seals an Ymrach at 2,120,000 div 6,120 = 346. A goblin warren of 1,000 (prowess 800, S = 800) seals an Ysgarth at 242, an Ymrach at 166 and a Zhakul at 102: goblins hold a gate less often than dwarves because of their prowess alone, with no new field. **A fall that cannot happen** (the hold is its race's last refuge and 6.6's flight has no plan) becomes a seal: "there was nowhere to flee", as P3-06 20.1 item 2 (r2: decided with the take's own plan, 6.12 item 1).

**Sealed.** Append **`deep_sealed`** (region of s, settlement t; participants c `civilization`, m `monster`, s `site`, l `lair`; value `telling` = (m + 1) mod 3; cause the `deep_woken`, (r2) then the `ally_aided_hold` events ascending). (r2) At a warren the event is `warren_sealed` instead (6.10). m becomes asleep (sleeping event the `deep_sealed`) and `sealed`: it never wakes again in history (6.4 item 2 of P5-02 skips it), never raids and is never hunted. `sealed(s)` = true: the mine stays open, forges and sells arms at its depth (6.3), never deepens. The lair l stays open to E with `settlement` 0.

### 6.6 The take: a hold lost

`take(m, t, cause)`, t standing and owned by c = `waking_realm(m)`:

1. P = P_t; dead = P × `deep_toll_permille` div 1000; survivors V = P − dead.
2. **Last refuge.** When t is its race's last refuge (`PeoplesSubsystem::settlement_last_refuge`, the one predicate of P3-05 r12 22.7): plan = `ConflictSubsystem::deep_flight_plan(ctx, c, t, V, lair region)` (7.3). No plan (no destination, `refuge_flight` = 0, `last_refuge` = 0, or n < `abandon_min`): **no take** (at a waking, the gate's outcome becomes sealed; at a later take, the raid stands and nothing else happens).
3. Append **`hold_lost`** (region of t, settlement t; participants m `monster`, c `civilization`, m's lair `lair`, and the mine at t `site` when one is open there (r2: the open kind 3 site whose `settlement` is t with the smallest id, 6.12 item 5); values `dead`, `survivors` = V, `telling` = (m + t) mod 3, (r2) plus 3 when `from_warren(m)` (the goblin wordings, 8.2); cause `cause`, (r2) at a waking followed by the `ally_aided_hold` events ascending). `set_population(t, V)` when dead > 0.
4. With a plan: `ConflictSubsystem::deep_flight(ctx, plan, hold_lost)` (7.3: `refugees_settled`, the hidden refuge, the ruin of t with the `hold_lost` as its ruin event). Without: `PeoplesSubsystem::ruin(ctx, t, hold_lost)` (P3-05 8.5 items 2 to 5: the survivors join c's nearest other standing settlement up to its room, recorded by `PeopleLedger::moved`; the rest are `displaced` and may arrive in another realm by P5-03 6.4.3, kin first; t ruined with owner 0; the region released when c has no other standing settlement in it; the capital moves).
5. `takes(m)` += 1; `holds_settlement(m)` = true.

**The realm's end.** A take of c's last standing settlement leaves c empty; peoples ends it at its next T1 (`civ_ended`, caused by the settlement's ruin event, the `hold_lost`; P3-05 `end_empty_civilizations`; P5-06 6.4 rule 4 names its last ruler). With a flight c lives on in its hidden refuge (P3-06 20.1: "the flight never ends its civilization").

**The mine.** The mine at a fallen hold is closed by alliances at its next closing (T6 of the same year) by P5-04 6.12's existing rule: ending 4, ending event t's ruin event (the `hold_lost`), ended year its year. No new ending code and no change to the closings. (r2) A warren has no site: nothing closes when it falls.

**The ending.** `settlement.ending` of a hold lost is **2** (overrun by a monster, P5-02 8.3): the derivation gains "ruin event of kind `hold_lost` → 2". A lost hold is told apart from an overrun by its ruin event's kind.

### 6.7 The horror's years (amends P5-02 6.4 for deep kinds)

A deep horror that is not sealed lives P5-02 6.4's year: age (never, ageless), waking again, sleep, raid (6.4.1's targets within `range_hops`, any realm's settlements), hunts (6.4.2 with P5-03 9.3 and P5-06 7.7). Its raids are cited by P5-03's fear rule (9.4) and by relations' common-enemy goodwill like any monster's. In place of P5-02 6.4.3's overrun (which it never makes: `holds_settlement` is true from its first take), after a raid on t:

```
if kind is deep and not sealed(m), owner(t) = waking_realm(m), that realm alive,
   hops(lair region, region(t)) ≤ deep_reach_hops, takes(m) ≤ deep_takes_max:   // (r2) takes counts the first, 6.12 item 6
    base = min(1000, overrun_permille_k × M div 1000)
    f_p  = min(1000, overrun_people_ref × 1000 div max(1, P_t))              // P5-03 14.1, after the raid
    if chance_pm(D(hist.deep.take; m, t, 0), base × f_p div 1000): take(m, t, raid)
```

A later take may take a capital, a city or a fortress (the deep comes from below; neither `overruns` nor the capital rule of 6.4.3 applies) but never a race's last refuge without its flight (6.6 item 2). The lair does not move. A horror takes at most 1 + `deep_takes_max` holds, all of its waking realm. The raid's fear rule (P5-03 9.4) runs after, when t still stands.

**Slain, driven back, sealed.** A deep horror is slain or driven off only by a hunt (P5-02 6.4.2): a realm it raided within `hunt_memory_years` hunts it, a hero (P5-06 7.7) raises the chance and doubles the strength, and the odds S × 1000 div (S + might) make it hard: a dwarven realm of 6,000 people within two borders, with a hero, has 840 against an Ymrach's 4,000 (17%) and a Zhakul's 7,000 (11%), at a yearly hunt chance of 1.2% to about 5%. Slain: the lair ends (ending 1); the lost hold stays a ruin. Driven off: it sleeps at least `drive_years`. A horror is sealed only at its waking; a hold it took later is never sealed (record only; Q-P5-07-2).

### 6.8 What remains

| Thing | Record | At E |
| --- | --- | --- |
| A deep mine | site kind 3; `delving_deepened` events (depth by event) | open, sealed or ended |
| A sealed horror | monster (kind 7 to 9, status 2), lair kind 1 at the hold's cell with `settlement` 0, `deep_sealed` | alive, asleep beneath a standing hold (or its later ruin) |
| A lost hold | settlement ruined (ending 2, ruin event `hold_lost`); lair kind 1 at its cell with `settlement` = it; the mine ended (4) at the same cell | the ruin always; the lair open while the horror lives |
| A flight | `refugees_settled` caused by the `hold_lost`; a hidden refuge | as P3-06 20.1 |
| A realm lost | `civ_ended` caused by a `hold_lost` | ended |
| (r2) A deep warren | a settlement with `warren_delved` events (depth by event; no site) | standing, sealed (a `warren_sealed`), lost (a `hold_lost`) or ruined otherwise |
| (r2) Aid at a gate | `ally_aided_hold` events, causes of the gate's outcome | record only |

### 6.9 How it meets the existing rules

| Rule | Here |
| --- | --- |
| Last refuge (P3-06 20.1, 20.20; P3-05 r9) | a take of a race's last refuge is a flight (7.3), else none; no race dies out (invariant 6). Casualties of a take are not reduced by 20.20's floor: the flight's n is checked against `abandon_min` instead |
| Alliances (P5-04 6.5) | (r2, Q-P5-07-4 decided) allies may answer at the gate of a waking (6.11), by a rule of their own: no call to arms, no `ally_joined_war` or `ally_refused`, no war row, no refusal count, no grudge. After the waking, hunts are unchanged: an ally raided by the horror hunts it for its own sake |
| Heroes (P5-06 6.7, 7.7) | unchanged: a raid may raise a hero; a hero's slaying is `monster_slain_by_hero` with the kind's `slayer_epithet`; a hero may die hunting (`hero_slain`) |
| Rulers (P5-06 6.4 rule 3) | the deed list gains `deep_woken` and `deep_sealed` (role `civilization`): a ruler is named at a waking and at a sealing; the templates name it with `{civilization:under}` |
| Renaming (P5-12) | rule 8 `deep` (4.4) |
| Refugees (P5-03 6.4) | every move of people goes through `PeopleLedger` (`ruin`'s `moved` and `displaced`; the flight's founding as in P3-06) |
| Undead (P5-02 6.3.2) | unchanged: the windows read battles and sacks; the dead of a take do not raise a host. A deep lair on a ruin makes the ruin "occupied" for a rising there |
| World cap, lairs per region | 6.4 "Caps" |
| P3-04 6.9 | 7.4 |
| Mines' closings (P5-04 6.12) | unchanged (6.6 "The mine") |
| Ruins stay (P5-06 25.1) | a lost hold stays a ruin to E, and its circle blocks foundings (no reclaiming in this stage) |

### 6.10 (r2) Other mountain peoples: goblin deep warrens

Any race with `mining_permille` > 0 opens mines (P5-04 6.12), deepens them and can wake the deep (6.2 to 6.7, unchanged). (r2, Paul 2026-11-26: "other races that delve, like goblins might also run into these things but less frequently".) A race with `delving_permille` > 0 (and `mining_permille` 0, 4.5) digs **deep warrens** instead: a warren is a settlement, its depth kept by `warren_delved` events. It opens no site, so the realm is never a seller: no forge (P5-04 7.5), no `arms_sold`, no forge factor in war, and `forge_depth` (7.1, which reads mines only) is untouched. This keeps the goblins, already the strongest race at E (median regions 12), from gaining strength by delving; a warren only adds danger. Shipped, only goblins delve (300); orcs unchanged.

**Delving** (T4 and Q4, after 6.2), settlements t ascending id up to the count taken before the loop:

```
skip t unless: t standing and visible; c = owner(t) alive and visible; mining_permille(r_c) = 0 < delving_permille(r_c);
               not warren_woken(t), not warren_sealed(t); d = warren_depth(t) < 3
if d = 0: skip t unless P_t ≥ deep_warren_population_min, region(t) qualifies (4.2: upland or ore_reach_cells ≥ deep_warren_ore_min),
          and c has no live warren (no standing u ≠ t owned by c with warren_delver(u) = c, warren_depth(u) ≥ 1, not sealed)
if d ≥ 1: skip t unless warren_delver(t) = c                 // a warren taken by another realm is frozen
w = Y − year(owner_event(t))                                 // years under its present owner (P5-03 Settlement::owner_event)
if w < deep_years[d]: next t
p = deep_delve_permille × delving_permille(r_c) div 1000     // T4
p = min(1000, p × L)                                         // Q4
Q4 only: quiet_events_left(c) > 0, else next t (no draw)
if chance_pm(D(hist.deep.warren_delve; t, d + 1, 0), p): append warren_delved; warren_depth(t) = d + 1; warren_delver(t) = c
```

**`warren_delved`** (region of t, settlement t; participant c `civilization`; values `depth` = d + 1, `telling` = (t + d + 1) mod 3; cause t's previous `warren_delved`, else `owner_event(t)`). So a goblin realm has at most one live warren; after a seal or the warren's loss it may start another. With the starts (3 per mille a year) depth 1 comes about 480 years after a large settlement came to its realm, depth 2 about 830, depth 3 about 1,330: few warrens live long enough under one realm.

**Waking** (T4, in 6.4's loop after every mine candidate, while no waking has happened in the step): warrens t ascending id, standing and visible, owner c = `warren_delver(t)` alive and visible, not woken, not sealed, d = `warren_depth(t)` ≥ 1 with `deep_wake_ppm[d − 1]` > 0. As 6.4 items 1 to 7 with:

- item 2: p = min(10^6, `deep_wake_ppm[d − 1]` × M div 1000 × `deep_warren_wake_permille` div 1000); draw `D(hist.deep.warren_wake; t, 0, 0)`;
- item 3: draw `D(hist.deep.warren_kind; t, 0, 0)`;
- items 5, 6: the lair at t's cell; `deep_woken` has **no `site` participant** (role 0–1, 8.1), `telling` = 3 + m mod 3, cause t's last `warren_delved`; `warren_woken(t)` = true, `from_warren(m)` = true;
- sealed: append **`warren_sealed`** (region of t, settlement t; participants c `civilization`, m `monster`, l `lair`; value `telling` = (m + 1) mod 3; causes the `deep_woken`, then the aid events) in place of `deep_sealed`; m asleep and sealed as 6.5; `warren_sealed(t)` = true (the warren never deepens again; t stays a settlement);
- the fall: 6.6 unchanged (no mine to close); `hold_lost`'s `telling` + 3 (goblin wordings).

The gate (6.5), the take (6.6), the horror's years and later takes (6.7), the aid (6.11), renaming rule 8, the flight and the realm's end apply to a warren's realm exactly as to a mine's: they are written for "the waking realm c", whatever its race. What reads "dwarf" stays dwarven: DP5 and DP9 (15.1). A goblin warren holds its gate less often than a dwarven town by prowess alone (800 against 1,400; 6.5's samples). Estimate at the starts: 0.05 to 0.15 goblin wakings a world, against r1's 0.3 to 0.6 from mines.

### 6.11 (r2) Allies answer a deep horror

(Paul 2026-11-26 on Q-P5-07-4: "I think they might yes.") At a waking (6.4 item 4, a mine's or a warren's) of horror m at hold t of realm c, before the gate, the aid A is decided:

```
H = hop distances from region(t) over region borders (P5-02 7.1's distances, as a monster's hops)
for a in AllianceView::allies(c), ascending id (copied to a local list first), while |A| < deep_aid_max:
    skip a unless a alive and visible
    q = a's standing visible settlement with H(region(q)) ≤ deep_aid_hops and the largest population (ties: smaller id)
    no q: next a (no draw)
    p = deep_aid_permille × ally_join_permille(r_a) div 1000
    if chance_pm(D(hist.deep.aid; m, a, 0), p):
        A += (a, q, S_a = P_q × deep_aid_levy_permille div 1000 × prowess_permille(r_a) div 1000)
```

**What the aid does.** The gate's S gains Σ S_a (6.5). Nothing else: the ally loses no people, the aid makes no relation row, no war, no claim, no strain or grudge, no refusal count, no renewal of the alliance, and moves no guard target of conflict or alliances (`AllianceView` is only read). When the hold falls anyway, survivors find shelter by the existing rules (P5-03 6.4.3; relations' `shelter_memory`). Later takes, raids and hunts get no aid (an ally raided by the horror hunts it for its own sake, P5-02 6.4.2).

**Record.** After the `deep_woken`, per (a, q) in A ascending a: **`ally_aided_hold`** (kind 97; region of t, settlement t; participants a `ally`, c `partner`, m `monster`, q `origin`; value `telling` = (m + a) mod 3; cause the `deep_woken`). The gate's outcome event (`deep_sealed`, `warren_sealed` or `hold_lost`) cites the `deep_woken` and then every aid event. An ally that does not answer leaves **no event** (no `ally_refused`: kinds 68 and 69 belong to wars and their motives and counts would be wrong here).

**Goodwill.** Straight after each aid event e, when r_a ≠ r_c: `RelationsSubsystem::credit(r_a, r_c, deep_aid_memory, e)` (an event of the current step, as the seam requires), whether the gate holds or falls. Same race: no credit.

With the starts a dwarven ally answers at 400 per mille, a human or orc at 300, a goblin at 200, an elf at 350, a faerie at 75. Estimate: aid at 10% to 30% of wakings.

### 6.12 (r2) Readings of the build (r1 was silent; `docs/reviews/p5-07-impl-notes.md`)

1. **The forced seal** (6.5) is decided with the plan the take would make: V = P − P × `deep_toll_permille` div 1000, threat region the lair's (the mine's or warren's) region. `deep_take` recomputes the plan from the same state; a disagreement is a FatalError ("found no flight"), never a silent seal.
2. State lives in the private `Site` and `Monster` structs (5), ids only; r2's warren state is a per-settlement member of `MonstersSubsystem`.
3. `mining_permille` and (r2) `delving_permille` are read per race from its definition; absent counts 0.
4. **`depths-small`** is in-memory content, not a data root: `worldgen/tests/history/p5_07_depths_content.hpp`, `depths_small_content()` (monsters-small with 4.2's edits) and `zero_depths(base)` (`deep_delve_permille` 0, for I0). Monsters-small's `overrun_permille` 1000 and `wake_rate` × 2000 apply to the deep kinds too (their weights keep their ratios; later takes are near certain within reach).
5. **`hold_lost`'s `site`** is the open kind 3 site whose `settlement` is t with the smallest id.
6. **Later takes** test `takes(m) ≤ deep_takes_max` before the draw, `takes` counting the first take, so a horror takes at most 1 + `deep_takes_max` holds; the reach is the hop count from the lair's region (the lair never moves).
7. Q4: p = min(1000, p × L) is computed, then the budget tested (no draw when spent); the draw's and the event's year are the step's first year.
8. `deep_flight` recomputes n from the population after the toll, which equals the plan's survivors, so n equals the plan's n.
9. Relations' common-enemy goodwill (P5-03a 3.3) reads raids and overruns; `hold_lost` is not added.
10. Report (part C): "sealed in" is set by any `deep_sealed` (r2: or `warren_sealed`) naming the monster; a mine's final depth is the largest `depth` of its kind 93 events; `deep_flights` and `realms_lost_to_deep` count `refugees_settled` and `civ_ended` with any `hold_lost` among their causes; `soured` is the first `spoils_withheld` of the pair by id with `grievance` 1 inside the spoils span.

## 7. Seams

### 7.1 `AllianceView::forge_depth` (amends P5-04 7.1)

`virtual std::int64_t forge_depth(std::int64_t c) const = 0;`: the largest `MonstersSubsystem::mine_depth(site)` over c's open mines, 0 when c has none (or no site seam). `MonstersSubsystem` gains `std::int64_t mine_depth(std::int64_t site) const` (0 for an unknown site or a site of another kind). Conflict reads it where it reads `forged` (6.3).

### 7.2 Monster reads

`MonstersSubsystem::living()` counts the kinds with `wake` ≠ deep (6.4 "Caps"); `living_of(kind)` counts every kind. `kind_wake(kind)` 1, 2, 3.

### 7.3 The flight seam (amends P3-06 20.1)

The flight procedure of P3-06 20.1 items 2 and 3 is shared between conflict's own flight and the deep:

```cpp
struct DeepFlightPlan { std::int64_t civ, settlement, region, survivors, n; SiteCell site; };
std::optional<DeepFlightPlan> ConflictSubsystem::deep_flight_plan(const StepContext&, std::int64_t civ,
        std::int64_t settlement, std::int64_t survivors, std::int64_t threat_region) const;   // pure
void ConflictSubsystem::deep_flight(StepContext&, const DeepFlightPlan&, std::int64_t cause);
```

- **Plan** (pure, no draw): `refuge_flight` = 1 and `last_refuge` = 1, else none. The destination as 20.1's with h(D) = min(`hops`(threat_region)(D), `march_hops` + 1) in place of h_x(D) (farthest from the horror's lair, then the best land, then the smaller id; the first with a site by P3-05 6.2); n = min(survivors, 2 × C(D, r_civ)); n < `abandon_min`: none.
- **Flight**: 20.1 items 2 and 3 with the `hold_lost` as the cause and ruin event in place of the sack (item 1, the sack, is not written: the take's `hold_lost` carries the dead); item 4 (the war) does not apply. `refugees_settled` (kind 32: region D, settlement h; participants h `settlement`, civ `founder`, t `origin`; value `population` = n; cause the `hold_lost`); `found_settlement`; `set_region_owner(D, civ, refugees_settled)`; `hidden_until[h]` = Y + `hidden_years`; `PeoplesSubsystem::lie_low`; P_t −= n; `ruin(ctx, t, hold_lost)`.
- Conflict's own flight calls the same code with the attacker's presence (20.1 unchanged in output).

The stage sets `monsters.set_conflict(&conflict)` beside `set_figures`.

### 7.4 P3-04 6.9 (amends P5-02 6.7)

1. `MonstersSubsystem` founds a settlement only through `deep_flight` (rule 1 of P5-02 6.7 had none), called inside the loop over monsters (its own vector, ids up to a count taken before the loop), never inside a loop over State's settlements, civilizations or regions; every such loop iterates ids up to a count taken before it and re-fetches after a change; members hold ids.
2. `tension()` unchanged and pure.
3. Order: monsters after renaming in `subsystems`; conflict is constructed before monsters.
4. Visibility: as P5-02 6.7 (settlements and civilizations created in the step are invisible to T4's rules, the hidden refuge included).
5. Keys: site, monster, settlement ids and depth levels; never list positions.
6. No relation is stored.
7. Region owners change only through `ruin` and the flight's `set_region_owner`.
8. (r2) The warren loops (6.10) iterate settlement ids up to a count taken before the loop and found nothing; a waking at a warren founds only through `deep_flight`, as item 1. The aid loop iterates a local copy of `allies(c)`; nothing in it changes alliances.

### 7.6 (r2) Wiring for the warrens and the aid

`MonstersSubsystem` gains `void set_alliances(const AllianceView*)` and `void set_relations(RelationsSubsystem*)` (set by the stage beside `set_conflict`; with either unset, no aid is decided and no draw is made) and, for tests, `std::int64_t warren_depth(std::int64_t settlement) const` (0 for an unknown id). `AlliancesSubsystem` already implements `AllianceView`. No other subsystem changes.

### 7.5 Relations: audit A1 (amends P5-04a 6.2 and P5-03a A4's seam)

`RelationsSubsystem::credit_units(std::int64_t race_a, std::int64_t race_b, std::int64_t units)`: M += units × 1000, clamped, as `credit`; **no contribution is kept** (no event enters the pair's lists). A quiet renewal (P5-04a 6.2) calls `credit_units` in place of `credit(…, a.renew_event)`. So `credit`'s contract holds without exception (a caller passes an event of the current step), an old event is never cited by a later turn, and a quiet renewal is no "fresh contribution" through the cool-down of P5-04a 7.2. Changes worlds where a turn cited a quiet renewal's event or passed the cool-down on it (expected rarely; RR6 may fall).

## 8. Events

### 8.1 Catalogue (appended after 92; ids final if no other stage merges first)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values |
| --- | --- | --- | --- | --- | --- | --- |
| 93 | `delving_deepened` | 1 | 1 | 1 | 1 `civilization` civ; 2 `site` site | `depth` (1 to 3), `telling` (0 to 2) |
| 94 | `deep_woken` | 1 | 1 | 1 | 1 `monster` monster; 2 `civilization` civ; 3 `site` site (r2: **0–1**, none at a warren); 4 `lair` site | `depth` (1 to 3), `telling` (r2: **0 to 5**; 3 to 5 at a warren) |
| 95 | `deep_sealed` | 1 | 1 | 1 | 1 `civilization` civ; 2 `monster` monster; 3 `site` site; 4 `lair` site | `telling` (0 to 2) |
| 96 | `hold_lost` | 1 | 1 | 1 | 1 `monster` monster; 2 `civilization` civ; 3 `lair` site; 4 `site` site 0–1 | `dead` (≥ 0), `survivors` (≥ 0), `telling` (r2: **0 to 5**; 3 to 5 for a horror from a warren) |
| (r2) 97 | `ally_aided_hold` | 1 | 1 | 1 | 1 `ally` civ; 2 `partner` civ; 3 `monster` monster; 4 `origin` settlement | `telling` (0 to 2) |
| (r2) 98 | `warren_delved` | 1 | 1 | 1 | 1 `civilization` civ | `depth` (1 to 3), `telling` (0 to 2) |
| (r2) 99 | `warren_sealed` | 1 | 1 | 1 | 1 `civilization` civ; 2 `monster` monster; 3 `lair` site | `telling` (0 to 2) |

(civ = entity type 3, monster 8, site 9, settlement as P3-04's catalogue; unlisted counts 1–1.) Only 93 and (r2) 98 occur in a quiet step (charged to their civilization). (r2) The engine as built writes 94 with `site` 1–1 and `telling` 0 to 2: the catalogue rows change (world bytes, not the schema; 9). `telling` picks one of three wordings (8.2); it is a function of ids (6.2, 6.4, 6.5, 6.6), so a reference reproduces it.

### 8.2 Templates and salience (data, `history.report`)

| key | sal. | template | summary? |
| --- | --- | --- | --- |
| `delving_deepened` | 15 | `{telling?Following a rich seam, |Year upon year, |By hammer and lamp, }{civilization} delved {site} {depth?deeper|below the reach of daylight|into the roots of the mountain|into the oldest dark beneath the world}.` | no |
| `deep_woken` (r2: six words) | 60 | `{telling?Delving too deep beneath |In the deepest halls under |Where the last seam ran out beneath |Burrowing ever deeper under |Digging after grubs and glitter beneath |In the stinking deep tunnels of }{@settlement}, {civilization}{civilization:under}{telling? woke | woke | broke into the old dark and woke | woke | blundered into the old dark and woke | woke }{monster}, {monster:kind}.` | yes |
| `deep_sealed` | 50 | `{telling?|Holding the gate at great cost, |With stone and rune, }{civilization}{civilization:under}{telling? sealed | walled | shut }{monster}{telling? in the deeps of | up beneath | away below }{site}{telling?||, and the deep ways were closed}.` | yes |
| `hold_lost` (r2: six words) | 70 | `{@causes:monster_raid=the raids|After |, }{monster}{telling? came up out of the deep and took | rose from below and overran | broke out of the deeps into | boiled up out of the burrows into | came up the goblins' own tunnels into | rose under the warrens of }{@settlement} of {civilization}; {dead?no one died|one died|# died}{survivors?, and none escaped|, and one escaped|, and # fled}.` | yes |
| (r2) `ally_aided_hold` | 40 | `{telling?Answering its ally's call, |Bound by old oaths, |Unbidden, }{ally}{ally:under}{telling? sent warriors from | marched out of | hurried its people from }{origin} to stand with {partner} at the gate of {@settlement} against {monster}.` | yes |
| (r2) `warren_delved` | 15 | `{telling?Ever greedy for the dark, |Squabbling and scraping, |Grub by grub and stone by stone, }{civilization} dug the warrens of {@settlement} {depth?deeper|below the reach of daylight|down among the mountain's roots|into the oldest dark beneath the world}.` | no |
| (r2) `warren_sealed` | 50 | `{telling?|Shrieking and stabbing in the dark, |With rockfall and fire, }{civilization}{civilization:under}{telling? caved in the tunnels over | blocked the deep ways against | buried }{monster}{telling? beneath | under | deep below }{@settlement}{telling?||, and no goblin went that way again}.` | yes |

(r2) The goblin words are data: should another race be given `delving_permille`, these templates name goblins and would need a choice by race (a later data and catalogue change). `{ally:under}` renders a ruler only when one is already recorded and reigning (P5-04a 4, as a refusal that is not a deed); `ally_aided_hold` is not added to P5-06's deed list.

Samples (illustrative names): `Year upon year, the Hold of Durn delved the Mines of Gimlin into the roots of the mountain.` · `Delving too deep beneath Gimlin, the Hold of Durn under Thrain the Old woke Bolgrim the Deep-Coiled, a wyrm of the lightless deep.` · `With stone and rune, the Hold of Durn shut Bolgrim the Deep-Coiled away below the Mines of Gimlin, and the deep ways were closed.` · `Bolgrim the Deep-Coiled broke out of the deeps into Gimlin of the Hold of Durn; 1,206 died, and 1,810 fled.` (r2: r1 printed `1206` and `1810`; `#` renders with the thousands separator) · (r2) `Bound by old oaths, the Kingdom of Harl marched out of Dunmere to stand with the Hold of Durn at the gate of Gimlin against Bolgrim the Deep-Coiled.` · `Grub by grub and stone by stone, Clan Skrag dug the warrens of Gnashgut down among the mountain's roots.` · `Digging after grubs and glitter beneath Gnashgut, Clan Skrag blundered into the old dark and woke the Swarm Beneath Gnashgut, a host of eyeless things from under the stone.` · `With rockfall and fire, Clan Skrag buried Ysmor Flame-Under-Stone deep below Gnashgut, and no goblin went that way again.` · `The Eyeless of Gnashgut boiled up out of the burrows into Gnashgut of Clan Skrag; 412 died, and 618 fled.` · `After the raids, the Swarm Beneath Durak rose from below and overran Kazmar of Clan Nirgurn; 214 died, and 321 fled.` · `After the loss of its last hold to the deep, Clan Nirgurn came to an end.` · `The survivors of Kazmar fled into hiding and founded Brunhal in the Grey Fells.` · `After the fall of a hold to the deep, the Iron Downs became known as the Deeps of Bolgrim.`

The choice placeholder with an empty first word (`{telling?|…}`) and with three words is P3-04 8.4's as widened by P5-04a 8.5; the test writer checks both render as written (16 item 9). The forms `{m:aside}`, `{m:where}` and `{m:died}` of a `deep` kind render as for `wild` (amends P5-03a 7.1 and P5-04a 10 item 5).

**Changed templates** (data; kinds unchanged):

| kind | change |
| --- | --- |
| `civ_ended` | `{@causes:hold_lost=the loss of its last hold to the deep|After |, }{civilization}{civilization:under} came to an end.` |
| `region_renamed` | cause words gain `hold_lost=the fall of a hold to the deep`, appended last |
| `refugees_arrived` | cause words gain `hold_lost=the fall of their hold`, appended last |
| `ally_refused` | motive 5's words `Grasping for its own gain` → `Hoping to profit by its ally's losses` (critic F3, 12) |

### 8.3 Causes (W6) and saliences

A deepening: the site's previous deepening, else its opening. A waking: the site's last deepening. A sealing: the waking. A hold lost: the waking (first) or the raid (later). A flight: the hold lost. A realm's end: its last ruin event. (r2) A warren's delving: its previous delving, else `owner_event(t)`; a warren's waking: its last delving; an aid: the waking; a sealing or a fall at a waking: the waking, then the aid events ascending. (r2) Saliences: an aid 40 (as `ally_joined_war`), a warren sealed 50, a warren delved 15; estimated 0.05 to 0.2 more summary candidates a world. Saliences: a waking reads like a peace or a great slaying (60), a lost hold just below a sack (70 against 75), a sealing like an overrun (50); a deepening is bookkeeping (15). Estimated summary candidates a default world: 0.3 to 0.6 wakings, 0.2 to 0.4 holds lost, 0.2 to 0.3 sealings, inside the budget of P5-02 12.9.

## 9. Schema 13 (hard rule 7)

**Depth is not stored in a column.** A `site.depth` column would hold the last `depth` value of the site's `delving_deepened` events, which are needed anyway for the chronicle, the causes and the depth at a year (the timeline). Deriving it costs a reader one pass over kind 93; a column costs a DDL change, a reader check that it equals the events, and a second source of truth. Recommended and specified: **derive** (the depth of a mine at year Y is the largest `depth` of its kind 93 events with year ≤ Y, 0 when none; its final depth the largest overall). "Sealed" is derived likewise (a kind 95 naming the site or the monster).

**What changes:**

- **No DDL statement changes**; no table or column is added. The empty world's page count and size are those of schema 12.
- **Code list:** `monster_kind.wake` gains **3 `deep`** (P5-02 11.2: 1 wild, 2 dead). This is why the version moves: a 0.28.0 reader would refuse a 0.29.0 world with `wake 3 not 1 or 2`; with the bump it refuses it as `world schema version 13, this build reads 12`, and 0.29.0 refuses version 12 worlds (`world schema version 12, this build reads 13`).
- `SCHEMA_VERSION`, `PRAGMA user_version` and `meta.schema_version` **13**.
- No new `site.ending` or `settlement.ending` code: a fallen hold's mine ends with 4, a hold lost has `ending` 2 (6.6), a sealed mine stays open.
- `event_kind` gains kinds 93 to 96 with their roles and values (catalogue rows written by the generator); (r2) and 97 to 99, with 94's `site` role 0–1 and 94's and 96's `telling` 0 to 5 (8.1). **(r2) No further schema change**: catalogue rows are data written by the generator, not DDL, and schema 13 has not merged, so r2 stays in schema 13 with generator 0.29.0. No column, table or code list is added: a warren is a settlement, its depth derived from kind 98 (the depth of t at Y is the largest `depth` of its kind 98 events with year ≤ Y), its seal from a kind 99 naming it.

**Reader checks** (both readers, `read_history_tables` and `HistoryView::load`, one function as P5-02 11.4; messages `corrupt table '<t>': row <k>: <reason>`):

- `monster_kind`: `wake <w> not 1 to 3` (was `not 1 or 2`).
- `monster`: `born_event` may be a `deep_woken` (kinds `monster_appeared`, `undead_rose`, `deep_woken`), for a kind with `wake` 3 only, and a kind with `wake` 3 is born only of a `deep_woken` (`born_event <e> is not the waking of this monster`).
- `site`: a lair's `origin_event` may be a `deep_woken` naming it as `lair`; every `deep_woken` is the `origin_event` of exactly one site (the P5-02 list 44, 45, 49, 54 gains 94).
- `settlement.ending`: the derivation maps a `hold_lost` ruin event to 2.
- Events: `depth`, `telling`, `dead`, `survivors` in the ranges of 8.1 (P5-03 16.4's form `value <k> <v> outside <a> to <b>`); a `delving_deepened` of depth d > 1 has a `delving_deepened` of depth d − 1 of the same site before it (`depth <d> without depth <d-1>`), and no two of one site share a depth (`depth <d> again`); its and a `deep_woken`'s `site` is a kind 3 site whose `founder_civ` is the event's `civilization` (`site <s> is not a mine of <c>`); at most one `deep_woken` per site (`a second waking of site <s>`); every `deep_sealed` and the first `hold_lost` of a monster cite that monster's `deep_woken` (`<kind> without its waking`); every `hold_lost` is the `ruin_event` of its settlement (`hold_lost <e> did not ruin its settlement`).
- (r2) Warrens and aid (same function, after the checks above, per event in id order): values of 97 to 99 in 8.1's ranges, `telling` of 94 and 96 0 to 5; a `warren_delved` follows 93's depth chain per settlement (`depth <d> without depth <d-1>`, `depth <d> again`, now naming the settlement), and all of one settlement name one `civilization` (`warren <t> delved by two realms`); a `deep_woken` without a `site` has a `warren_delved` of its settlement and civilization before it (`waking of settlement <t> without a warren`), at most one per settlement (`a second waking of settlement <t>`); every `warren_sealed` cites its monster's `deep_woken` (`<kind> without its waking`, as 95), and that waking has no `site`; every `ally_aided_hold` cites a `deep_woken` of its year whose `civilization` is its `partner` and whose monster is its `monster`, with `ally` ≠ `partner` (`ally_aided_hold <e> does not answer its waking`).

## 10. History report v10 (amends P3-07; new section 33)

1. Header **`# worldgen history v10`** at both levels; `report --help` prints it. The growth rule holds for v10.
2. `[monsters]`: a monster with status 2 whose last sleeping event is a `deep_sealed` prints `sealed in <lair site ref>` in place of `asleep`; the count line still counts it as asleep.
3. `[sites]`: a mine line gains, after its settlement part, `, depth <d>` when its final depth d ≥ 1, then `, sealed <year> e<id>` when a `deep_sealed` names it. Ending words unchanged.
4. `[present]` ruins: `<how>` gains `hold_lost` → `lost to <monster ref>` (as `settlement_overrun` → `overrun by <monster ref>`).
5. `[alliances]` (critic F4): an alliance line gains `; soured e<id>` after its spoils part, e the first `spoils_withheld` of the pair with `grievance` 1 whose id lies in the row's span (P5-04a 11 item 9's span).
6. `[civilizations]` milestones at summary (critic F2; amends P5-04a 12.2): the three shown are chosen with **at most one per kind**: rank the realm's milestones by (salience descending, id ascending), keep the first of each kind, take the first three, print them in ascending id. The count line `milestones: <n>` is unchanged.
7. `[stats]` (full; appended in this order): `mines_deepened` (kind 93 events), `mines_deepest` (mines with a depth-3 event), `deep_woken`, `deep_sealed`, `holds_lost`, `deep_flights` (`refugees_settled` caused by a `hold_lost`), `realms_lost_to_deep` (`civ_ended` caused by a `hold_lost`), `deep_alive` (monsters of `wake` 3 with status 1 or 2); (r2) then `warrens_delved` (kind 98 events), `warrens_deepest` (settlements with a depth-3 kind 98), `warrens_woken` (kind 94 without a `site`), `warrens_sealed` (kind 99), `deep_aided` (kind 97 events), `wakings_aided` (kind 94 cited by a kind 97). `deep_woken` counts every kind 94, mine or warren; `deep_sealed` counts kind 95 only.
7a. (r2) `[monsters]`: item 2's `sealed in` also when the last sleeping event is a `warren_sealed`.
8. **Length**: at most 3 chronicle candidates a world, inside the budget; no block gains a line at summary (2 to 5 change words in lines). T18 holds (15.1).

## 11. Timeline, browser, markers (library) and the viewer

### 11.1 Library (no viewer code)

- **Timeline** (amends P4-01; r8): `std::int64_t mine_depth_at(std::int64_t site, std::int64_t year) const` (9's derivation; 0 for a site of another kind) and `std::int64_t sealed_year(std::int64_t monster) const` (the year of its `deep_sealed`, −1 none). Monster state at Y (P5-02 13): `deep_woken` sets awake, `deep_sealed` asleep (the list of kinds 44 to 52 gains 94 and 95). The year dump D(Y) is unchanged in form.
- (r2) **Timeline**: `std::int64_t warren_depth_at(std::int64_t settlement, std::int64_t year) const` (9's derivation from kind 98; 0 when none); `sealed_year` also reads a `warren_sealed`; monster state: `warren_sealed` sets asleep (the list gains 99).
- **Browser** (amends P4-05 4.4): kind 93 in group `settlements` (with 77 `mine_opened`), kinds 94 to 96 in `monsters`; (r2) 97 in `alliances`, 98 in `settlements`, 99 in `monsters`. No new group; `ALL_GROUPS` unchanged.
- **Markers** (amends P4-04 3.4): `event_symbol("hold_lost")` = `sack`; `deep_woken` and `deep_sealed` plain; (r2) 97 to 99 plain. A warren has no glyph of its own (it is a settlement). **No new glyph**: a lost hold is a ruin (glyph 8) at its cell from the year of its `hold_lost`, an old ruin after `OLD_RUIN_YEARS`; its ended mine's glyph goes from the year of its end (as any ended mine); a sealed mine keeps its mine glyph. Lairs are not drawn (unchanged). The ruin's hover text already carries its ruin sentence (the `hold_lost` sentence).
- **Mine text**: `std::string_view mine_depth_word(std::int64_t depth)` in `worldgen/inspect/markers.hpp`: 1 `below daylight`, 2 `to the mountain's roots`, 3 `to the old deep`, else empty.
- **`allies:` entity segments** (12 item 4).

### 11.2 Viewer work (viewer engineer; minimal)

1. Mine hover (P5-04 11.3's `<site name> (mine of <civilization name>), since <year>`) gains `, delved <mine_depth_word(d)>` when `mine_depth_at(site, Y)` = d ≥ 1, and `, sealed in <year>` when the mine's horror was sealed by Y.
2. Headless check **M11**: on the smallest seed ≥ 42 at 512 whose full history report has a `, depth 3` mine line (the seed is found once by the viewer engineer and recorded in `headless_check.cmake`; if none of seeds 42 to 91 has one, the check is recorded as skipped with that note, and M11 runs on the `depths-small` root instead), the hover text of that mine at E as in item 1, and at a year before its first deepening without the depth part.
3. The `allies:` links (12 item 4): the browser block shows them as links; check B14 asserts one link per ally on a world with an `allies:` line.
4. (r2) Settlement hover gains `, warrens delved <mine_depth_word(d)>` when `warren_depth_at(t, Y)` = d ≥ 1. Headless check **M12**: on a `depths-small` world (the M11 fallback's), a settlement whose `warren_delved` reaches depth ≥ 1, at E and at a year before its first delving.

No other viewer change: the new kinds are in existing groups, the lost hold uses the ruin glyph.

## 12. Carried items (this stage is the next change to world bytes)

| # | Item | Taken? | Why, and where |
| --- | --- | --- | --- |
| 1 | P5-04a audit A1: a quiet renewal credits an old event | **taken** | 7.5; small (one overload), restores the seam contract, and may lower RR6 (missed on B: 9 worlds against 5) |
| 2 | P5-04a audit A2: a tower's land lost and won back inside one quiet step | **recorded, no rule change** | the rule means "not the realm's at the check"; the reading is now in P5-04a 21.4 (r3) |
| 3 | `p3_conflict_reference.py check-world` still names 0.27.0 | **taken** (test writer) | it reads `GENERATOR_VERSION` 0.29.0 and accepts `--version`, as the other references; with 7.3 it also replays the deep flight's `refugees_settled` (its invariant 3 reads "caused by a `sacked` or a `hold_lost` of the same year whose settlement is the event's `origin`…") |
| 4 | `allies:` lines as entity segments | **taken** | `SegmentKind` gains `tagged = 6`: report text `<tag> <escaped name>` (tag first, as printed today, so **no report text changes**); display `<tag> <name>` with a link over the whole text; the B4 transform (display → report) leaves a `tagged` segment's text as is (it adds no ` (<tag>)`). The `allies:` line at summary and full is built from `tagged` segments. Library and the viewer's B check |
| 5 | Spec text: P5-04 13 item 14 | **done** in P5-04 r5 | "at most one per opened trade and ordered (seller, buyer) pair" |
| 6 | Spec text: P5-04a 21.4, an ending event older than the tower | **done** in P5-04a r3 | the tower waits for the next change of owner |
| 7 | Critic F2 (milestones of one kind) | **taken** | 10 item 6; report only |
| 8 | Critic F3 (motive 5 says nothing) | **taken**, wording | 8.2; the template word only, the rule unchanged |
| 9 | Critic F4 (a first withholding invisible) | **taken** | 10 item 5; report only |
| 10 | Critic F1 (renewal lines dull) | left | a cap per pair is an alliance rule change; a later alliances round |
| 11 | Critic F5 (towers in one region) | left | a figures rule and a cause change; a later figures round |

## 13. Invariants (every seed; property tests and `check-world`)

1. Per mine: `delving_deepened` depths 1, 2, 3 in that order, each at most once, each at worked years ≥ `deep_years[d − 1]`, none after the mine's end or its sealing, none for a founder whose race has `mining_permille` 0.
2. Per mine at most one `deep_woken`, in a tick year, at depth d ≥ 1 with `deep_wake_ppm[d − 1]` > 0, its monster of a `wake` 3 kind, its lair at the mine's cell.
3. Every `deep_woken` is followed in the same step by exactly one `deep_sealed`, (r2) `warren_sealed` (a waking without a `site` only, and only it) or `hold_lost` of its monster, with no event of another monster between them ((r2) its `ally_aided_hold` events lie between, at most `deep_aid_max`); a sealing has its lair's `settlement` 0, a fall its lair's `settlement` = the event's settlement.
4. A sealed monster has status 2 at E, and no event of kinds 46, 48, 50, 51, 88, 89 or 96 names it after its `deep_sealed` (r2: or `warren_sealed`); its mine has no later `delving_deepened` ((r2) its warren no later `warren_delved`).
5. Every `hold_lost`: its settlement is ruined with it as ruin event and `ending` 2; its civilization is the monster's waking realm; a later take lies within `deep_reach_hops` of the lair's region and cites a `monster_raid` of the same monster and settlement of its year; a monster has at most 1 + `deep_takes_max` of them.
6. No race present goes extinct (P3-06 invariant 7): a `hold_lost` of a race's last refuge is followed in its step by a `refugees_settled` caused by it.
7. Every `civ_ended` caused by a `hold_lost` follows a step after which its realm had no standing settlement.
8. `monster_kind` ids 1 to 6 are `dragon`, `giant`, `ogre`, `troll`, `undead`, `wyvern` with their 0.28.0 rows; ids 7 to 9 are `ymrach`, `ysgarth`, `zhakul` with `wake` 3.
9. A mine whose town fell to the deep ends with `ending` 4, `ending_event` the `hold_lost`, in its year.
10. The force of a seller is P5-04 7.5's with `forge_depth_permille` × D(c) added (replayed by the reference from the open mines and their depths).
11. P5-02 14, P5-03, P5-04, P5-04a and P5-06 invariants hold with the deep kinds counted as monsters (the reference's invariant 4 of `p5_02_monsters_reference.py`: a deep monster's first event is its `deep_woken`).
12. (r2) Per warren settlement: `warren_delved` depths 1, 2, 3 in order, each at most once, all of one civilization whose race has `delving_permille` > 0 and `mining_permille` 0, each at years under that owner ≥ `deep_years[d − 1]`, none after the warren's waking or ruin; the first at a population ≥ `deep_warren_population_min` in a qualifying region; a realm has at most one live warren at every year's end.
13. (r2) No race with `mining_permille` 0 has a kind 3 site, an `arms_sold` as seller or a forge: a warren never feeds `forge_depth`, and the force of a goblin realm is that of the same world with warren delving off (replayed by the reference).
14. (r2) Every `ally_aided_hold`: its `ally` was allied with its `partner` at the event (an open alliance row), had a standing settlement `origin` within `deep_aid_hops` of the hold's region, and no `relation` row, war, claim, `ally_joined_war` or `ally_refused` is caused by it; a waking has at most `deep_aid_max` aids, in ascending ally id.
15. (r2) A warren's waking: no `site` participant, `telling` 3 to 5, its monster's later `hold_lost` events have `telling` 3 to 5; a mine's waking and its falls `telling` 0 to 2.

## 14. Determinism notes (for the auditor)

- Integer arithmetic, int64, checked multiplication. Largest products: P_t × 4000 × 1400 < 2^43; `deep_wake_ppm` × M ≤ 10^6 × 10^4; the force factor (1000 + 1000 + 3000) × f as P5-04's.
- Five keyed streams (3), keys of ids and depth levels, never positions. Every draw is behind a fixed test order: deepening (budget, then draw), waking (kind room, then wake, then kind), gate always drawn once per waking, take after its conditions.
- Loops: sites ascending id; monsters ascending id up to a count taken before the loop; K ascending kind id; the flight's destinations by P3-06 20.1's total order.
- `telling` is a function of ids. Depth and "sealed" are members rebuilt by the engine's replay on resume (P3-04 16.3) like every subsystem's state.
- No floating point, no unordered container, no new lint rule expected (the auditor decides).
- (r2) Nine keyed streams (3). Warren loops: settlements ascending id up to a count taken before the loop; at most one live warren per realm is checked within the same loop, so the first by id wins. The waking loop takes mines, then warrens. Aid: `allies(c)` ascending (a copy), hop distances by P5-02 7.1's BFS (ties by region id), the settlement q by (population descending, id ascending); the aid draw comes before the gate draw and both before any event. Largest new products: P_q × 4000 × 1600 < 2^44; `deep_wake_ppm` × M × `deep_warren_wake_permille` ≤ 10^6 × 10^4 × 10^3 < 2^44. The warren state and `from_warren` are rebuilt on resume by the engine's replay as the rest. `credit` is called only with the aid event just appended.
- **I0** (proof obligation): with `deep_delve_permille` 0 and `forge_depth_permille` 0, the history tables are byte-identical to those of the same build with the deep rules removed: no mine (r2: and no warren) deepens, so none wakes (r2: so no aid is decided), and the force is P5-04's. What differs from such a build: the catalogue rows of kinds 93 to 96 (r2: to 99), the changed templates, and `monster_kind` rows 7 to 9 (written for every definition). (r2) **I1**: with `delving_permille` 0 for every race and `deep_aid_permille` 0, the history tables other than the catalogue (`event_kind`, `event_kind_role`, `event_kind_value`) equal r1's on the same data, apart from the wider templates (a regression check for r2's engine against a build of r1).

## 15. Targets and calibration

### 15.1 Targets (1024 defaults; A = seeds 0–99, B = 100–199; pooled per sample unless stated)

| Target | Definition | Bound | 0.28.0 |
| --- | --- | --- | --- |
| **DP1** | worlds where a mine reaches depth 3 | 40 to 90 | — |
| **DP2** | worlds with a `deep_woken`; most in one world | 25 to 55; ≤ 4 | — |
| **DP3** | sealed share of wakings | 30% to 65% | — |
| **DP4** | worlds with a `hold_lost`; most in one world | 15 to 40; ≤ 4 | — |
| **DP5** | dwarven realms ended by the deep (`civ_ended` caused by a `hold_lost`) | ≤ 8% of the dwarven realms of the sample; at least one in A ∪ B (else reported, so the critic can read one); flights from the deep reported | — |
| **DP6** | deep horrors alive at E | ≥ 70% of those woken | — |
| **DP7** | deep horrors slain (by armies or heroes) | ≤ 25% of those woken; the count reported | — |
| **DP8** | renamings by rule 8 | reported (P5-12's caps hold) | — |
| **DP9** (guard) | regions held by dwarves at E | median ≥ 2 on each sample; mean not below 0.28.0 − 0.2 | 2 / 2.2; 2 / 2.4 |
| **P** (guard) | dwarves in an alliance or trade | ≥ 85% | 90%, 91% |
| **M4a** (guard, Paul) | a living monster at E | ≥ 85 worlds on each sample | 94, 93 |
| **M4b** (guard, Paul, Q-P5-06-1 decided) | a lair on a former settlement open at E | average of A and B ≥ 65, neither below 60 | 62, 68 (65) |
| **C1** | wars median | 22 to 28 | 25, 23 |
| **G2** | standing settlements; population at E (medians) | ≥ 81; ≥ 205,000 | 82 / 214,482; 86 / 210,881 |
| **T18** | summary lines | default median ≤ 670; every world ≤ L(C); `civs4` median ≤ 1,050 | 625 / 784, 627 / 801; `civs4` 915 |
| **T16** | repeated battles | ≤ 2 per thousand | 1.26, 1.39 |
| **UD2, UD3** | undead | as P5-06 15.1 (UD3 ≤ 50%) | 48.6%, 42.0% |
| **FG5** | slain by heroes | ≥ 25% | 30.8%, 33.8% |
| **RR6** (report) | worlds with a pair of four or more turns | ≤ 5 (A1's effect reported) | 5, **9** |
| **AL1, AL5** | as P5-04a 15.1 | as there | 14 / 9.5, 13.5 / 9; 29.7%, 31.2% |
| **timing** | `generate` seed 0 1024 release | ≤ +1 s over 0.28.0 (24.9 to 25.6 s user) | — |
| (r2) **GW1** | worlds where a goblin warren reaches depth 3 ("the deep") | 10 to 50 on each sample | — |
| (r2) **GW2** | goblin wakings (kind 94 without a `site`) as a share of all wakings, pooled per sample | 10% to 30% on each sample, and at least one goblin waking in each sample | — |
| (r2) **GW3** | goblin holds lost; goblin realms ended by the deep | reported | — |
| (r2) **GW4** (guard) | regions held by goblins at E | median not above r1's on each sample (and not above 12); mean not above r1's + 0.2 | r1: measured by calibration |
| (r2) **GW5** (guard) | regions held by dwarves at E | median not below r1's on each sample; DP9 holds | r1: measured |
| (r2) **AD1** | wakings with an aid (`wakings_aided` / `deep_woken`) | 10% to 40% pooled over A ∪ B; the sealed share with and without aid reported | — |
| (r2) **AD2** (guard) | AL1, AL5, C1, RR6 | as above (the aid's goodwill must not move them out of bounds) | — |

(r2) Every r1 target and guard stands; DP1 to DP7 are read for mines (dwarves) as r1 wrote them, except DP2, DP3 and DP4, which count every waking, mine or warren (DP2's bound 25 to 55 worlds was set for mines; calibration reports the mine-only figure beside it). Q-P5-07-1 is decided as DP4 and DP5 already read (Paul: "8% sounds right, but an individual city/mine/delving could fall more often than that").

**How the rules keep the dwarves (DP9, P, G2).** The deep takes only from the realm that woke it, within one border of the lair, at most three holds per horror, one horror per mine ever and at most five deep horrors alive at once; a large hold usually seals (the gate grows with its people); a region is released only when the realm has no other settlement in it; and the forge's depth makes every deep-mining realm stronger in war, so the regions it holds against its neighbours offset the few it loses to the deep. **Effect on M4.** Every fall puts a lair on a former settlement and the horrors are ageless and hard to kill, so M4b and M4a should rise (estimate: +3 to +8 worlds for M4b at DP4's middle). **Effect on G2**: about 0.3 holds lost a world, so the standing median may fall by 0 to 1 (A is at 82); calibration watches it.

### 15.2 Calibration (data only, the engineer, in this order; history replayed on cached geography, checked equal to `generate`)

1. DP1 by `deep_years` and `deep_delve_permille`.
2. DP2 by `deep_wake_ppm` (depth 3 first, then 2).
3. DP3 by `deep_gate_levy_permille`, `deep_seal_max_permille`, then the kinds' `might`.
4. DP4, DP5 by `deep_takes_max`, `deep_reach_hops`, the kinds' `overrun_permille` and `raid_permille`, `deep_toll_permille`.
5. DP6, DP7 checked; `might` if DP7 fails (FG5 checked).
6. `forge_depth_permille` last: C1, DP9, P, AL5.
7. After every step: M4a, M4b, G2, DP9, T18; a step that breaks one is undone and reported to the lead.

(r2) When the r2 engine is built, on top of the r1 values reached so far (the r1 sweep's last values are the baseline for GW4 and GW5):

8. GW1 by goblin `delving_permille` and `deep_warren_population_min` (`deep_warren_ore_min` only if warrens sit in lowlands).
9. GW2 by `deep_warren_wake_permille` (never by `deep_wake_ppm`, which would move DP2 for mines).
10. AD1 by `deep_aid_permille`, then `deep_aid_hops`; DP3 rechecked (the aid raises it), then `deep_aid_levy_permille` if DP3 leaves its bound.
11. `deep_aid_memory` last: AD2.
12. After every step: GW4, GW5 and step 7's list.

## 16. Tests required

C++ (new shard `worldgen-tests-depths` for `*depths_*`, slow `-slow-depths`; the alliances, conflict, relations and history-report shards for 7.1, 7.3, 7.5 and 10):

1. Data: the three kinds load; `wake` = `deep`; `monster_deep_biomes`; each new `history.monsters` field's bounds and absence; `monsters_deep_years_ordered`; `forge_depth_permille`; rule 8 passes `renaming_rules`; ids 1 to 6 unchanged (invariant 8).
2. Depth: the years at `deep_years[d] − 1` and at it; the chance with `mining_permille` 500; Q4's p × L and the budget wait (no draw); one level per step; sealed and closed mines never deepen; the cause chain.
3. Forge: `forge_depth` 0 to 3 and the force factor; a sealed mine counts; `forge_depth_permille` 0 gives P5-04's force.
4. Waking: tick only; one per step (two candidates, the first by id wins); K empty (no draw); weights; the world cap does not block and is not consumed; a lair elsewhere in the region does not block; `woken` once per mine.
5. Gate: odds at S = 0, S = might, the cap; sealed and fall at u = odds − 1 and odds; the forced seal of a last refuge with no plan.
6. Take: toll and survivors; ruin with the ledger (moved, displaced); the capital moves; the region kept or released; the realm's end at the next T1 with its cause; the flight (plan, `refugees_settled`, hidden refuge, `lie_low`); no plan → no take at a later take; the mine's ending 4 at T6.
7. The horror's years: raids of any realm; later takes only of the waking realm within reach, at most `deep_takes_max`; capitals and cities taken; a sealed horror never wakes, raids or is hunted; hunts and a hero's slaying with `slayer_epithet`.
8. Seams: `deep_flight_plan` equals conflict's own flight order with the threat region; conflict's own flights unchanged (the P3-09 goldens do not move except by the generator version); `credit_units` keeps no contribution; a quiet renewal cites nothing later (7.5).
9. Rendering: every new template at each `telling` and `depth`; the empty first choice word; `{m:aside}`, `{m:where}` for a deep kind; the four changed templates.
10. Reader: every check and message of 9 on corrupt fixtures; a 0.28.0-style world (schema 12) refused with the version message.
11. Report v10 on fixture **H19** (`h19-v10.toml`, from H18; r2: r1 named it H18, which P5-04a holds): a deep mine at depth 3, (r2) a goblin warren at depth 3, a warren sealed, a warren lost, a waking with two aids and one with none, a sealed horror, a lost hold, a later take, a flight, a realm lost to the deep, an alliance with a first withholding (`; soured`), a realm whose top milestones repeat a kind; both levels; the `allies:` segments' report text equal to v9's form.
12. Timeline: `mine_depth_at` across the deepening years; `sealed_year`; monster awake/asleep at the waking and sealing years. Browser groups. Markers: `hold_lost` symbol; `mine_depth_word`.
13. Invariants of 13 (default: seeds 0–9 at 512 with `depths-small`, and 0–9 at 512 shipped; **slow**: 0–19 at 1024 shipped and `--monster-pressure high` 0–4).
14. I0 (14) and resume (P3-04 16.3) with a world that has a waking before the resume point.
15. (r2) Data: `delving_permille` bounds and absence (0); `race_delves_one_way`; each r2 `history.monsters` field's bounds and absence; the shipped race values of 4.5.
16. (r2) Warrens: the first-level conditions one at a time (population at `deep_warren_population_min` − 1 and at it; upland by 4 × `walk_cells` < `cells` and not; ore at `deep_warren_ore_min` − 1 and at it); one live warren per realm (two candidates in one step, the smaller id wins; a second after a seal and after a loss); frozen when taken by another realm; w from `owner_event`; Q4's budget wait; the cause chain; no site, no `forge_depth`, no `arms_sold`, conflict's force unchanged (invariant 13); the wake chance with `deep_warren_wake_permille`; a mine candidate wins over a warren in the same step; `warren_sealed` and the fall; `telling` 3 to 5 on 94 and 96; later takes from a warren horror.
17. (r2) Aid: no ally, an ally with no settlement in reach (no draw), an ally that answers and one that does not (no event); `deep_aid_max` (three answering, two kept, ascending id); S with Σ S_a at the odds boundary; the causes of the outcome; `credit` called once per aid with the aid event, none for the same race; no relation row, war, `ally_joined_war` or `ally_refused` (invariant 14); a seal forced with aid present.
18. (r2) Rendering: kinds 94 and 96 at each `telling` 0 to 5, 97 to 99 at each `telling` and `depth`; reader checks of 9's r2 item with their messages; the six new stats; timeline `warren_depth_at`, `sealed_year` from a 99; browser groups of 97 to 99.
19. (r2) I1 (14) on the fixtures and seeds 0 to 4 at 512 shipped.

Corrections for the test writer (r2): the gate samples are 218 and 137 (6.5); the hold-lost sample prints `1,206` and `1,810` (8.2); the fixture is H19.

Reference: **`p5_07_depths_reference.py`** (test writer; it may build on `p5_02_monsters_reference.py`'s replay): replays 6.2 to 6.7 on the fixtures and `check-world` for 13; golden **`worldgen/tests/golden/p5-07-depths.txt`** (fixture `depths-small` worlds at 512, seeds 0–9, and shipped seed 42 at 1024: the hashes of `site`, `monster`, `event` and `event_value`, and the stat lines of 10 item 7). (r2) The reference also replays 6.10 and 6.11, and the golden gains the `depths-small` worlds' kind 97 to 99 counts. Carried test-side items: `p3_conflict_reference.py` (12 item 3); `p5_02_monsters_reference.py` invariant 4 (13 item 11); `p5_04_alliances_reference.py` reads the forge depth (invariant 10); `p5_12_renaming_reference.py` knows rule 8; `p4_browser_reference.py` the new kinds' groups.

## 17. Golden values and CI

Every world file and every history golden is re-recorded once, after the cross-preset comparison (generator 0.29.0, schema 13). Must not move: `core_vectors.txt`, `p1-0*`, `p3-02-names.txt`, `p3-03-chronology.txt`. `world-hashes.sh` keeps its sixteen cases (`p5-monsters-high-1024` and `hist-dense-1024` are the likeliest to hold a waking; validation reports which cases do, for audit A3's kind of coverage). New CTest entries `worldgen-tests-depths` and `worldgen-tests-slow-depths`.

## 18. Work split (for cost)

| Part | Kind | Agent | Sections |
| --- | --- | --- | --- |
| A | engine: the deep rules in `monsters_deep.cpp`, the deep kinds in `monster_kind_defs_from`, `living()` | generator engineer | 4.1, 4.2, 5, 6, 7.2, 7.4 |
| B | engine seams: `forge_depth` (alliances, conflict force), the flight seam (conflict), `credit_units` (relations), the deed list (figures), stage wiring | the same engineer, or a second on disjoint files | 4.3, 7.1, 7.3, 7.5, 6.9 rulers |
| C | catalogue, readers' checks, report v10, digest, the `tagged` segment, timeline, browser and markers library | one engineer for `history_report.cpp`, `digest*`, `timeline.cpp` | 8.1, 9, 10, 11.1, 12 item 4 |
| D | data: three monster files, the fields, rule 8, templates; both fixture roots and `depths-small` | with A to C (needed before any test) | 4, 8.2 |
| E | calibration sweeps, data only | a separate agent after A to D | 15.2 |
| F | tests and references | test writer | 16; 12 item 3 |
| G | goldens | validation, once, after the cross-preset comparison | 17 |
| H | viewer | viewer engineer | 11.2 |

A and B share `stages/history.cpp`'s wiring only; C needs the catalogue rows of A/D. E needs a build of A to D and no code.

**(r2) Delta, on top of the r1 build** (estimate under 200 engine lines for A2 and under 120 for C2):

| Part | Agent | What r2 adds | Sections |
| --- | --- | --- | --- |
| A2 | generator engineer | `monsters_deep.cpp`: warren delving and waking (6.10), `warren_sealed`, `from_warren` and the `telling` + 3, the aid (6.11) before the gate with its events, causes and `credit`; `set_alliances`, `set_relations`, `warren_depth` (7.6); stage wiring; four streams (107); catalogue rows 94 (widened), 97 to 99 | 3, 5, 6.4, 6.5, 6.10, 6.11, 7.6, 8.1 |
| D2 | the same engineer | race field `delving_permille` and check `race_delves_one_way` (`kinds_history.cpp`); goblin 300 and 0 elsewhere in `game/data/races/`; the eight r2 `history.monsters` fields in all three roots' `monsters.toml`; templates 94 and 96 widened and 97 to 99 in `history.report`; `depths_small_content()` gains 4.2's r2 edits; `schema.md` catalogue rows 97 to 99 and 94's changes | 4.2, 4.5, 8.2 |
| C2 | part C's engineer | reader checks of 9 (r2 item); stats of 10 item 7 and `sealed in` for 99; timeline `warren_depth_at`, 99 in monster state and `sealed_year`; browser groups of 97 to 99 | 9, 10, 11.1 |
| E2 | calibration | steps 8 to 12 of 15.2; the r1 sweep's last values are the baseline | 15 |
| F2 | test writer | 16 items 15 to 19, H19, the corrections; the reference's 6.10 and 6.11 | 16 |
| H2 | viewer engineer | 11.2 item 4 and check M12 | 11.2 |

G (goldens) is unchanged: once, after the comparison, for r1 and r2 together.

## 19. Amendments

| Spec | Section | What |
| --- | --- | --- |
| P5-02 | 4.2, 6.1, 6.3 (caps), 6.4 (deep kinds), 6.7, 8.3 (ending derivation), 11.2 (`wake` 3), 11.4 (checks), 13 (monster state) | 4.1, 6, 9, 11.1 |
| P5-04 | 6.14 (the seam taken), 7.1 (`forge_depth`), 7.5 (the factor); 13 item 14 (text, r5, done) | 6.3, 7.1 |
| P5-04a | 6.2 (quiet renewal credits units), 12.2 (milestones by kind), 12.3 (`; soured`); 21.4 (text, r3, done) | 7.5, 10 |
| P5-03 / P5-03a | the seam `credit` (A4's contract restored); 7.1 forms for `deep` | 7.5, 8.2 |
| P5-06 | 6.4 rule 3 (deeds `deep_woken`, `deep_sealed`); P5-06 20.1's seam answered (no `figure_died` how 3: no figure dies with a hold in this stage) | 6.9 |
| P3-06 | 20.1 (the shared flight code; the deep's threat region), invariant 3 (`hold_lost` as a flight's cause), 7.3 (force) | 7.3, 6.3 |
| P3-07 | new 33 (v10) | 10 |
| P4-01, P4-04, P4-05 | timeline reads, event symbol, groups | 11.1 |
| P5-12 | rule 8 (data) | 4.4 |
| `schema.md` | `monster_kind.wake` 3; catalogue 93 to 96; changelog row 13 | 9, 22 (the engineer) |
| (r2) P5-04 | 4.1 (race field `delving_permille`; `AllianceView::allies` read by monsters, no alliance rule changed) | 4.5, 6.11 |
| (r2) P5-03 / P5-03a | `credit` gains a caller (the aid, an event of the current step) | 6.11 |
| (r2) P5-02 | 6.1 (warren steps), 13 (monster state for 99) | 6.10, 11.1 |
| (r2) `schema.md` | catalogue 97 to 99, 94's `site` 0–1, 94 and 96 `telling` 0 to 5; changelog row 13 reads "event kinds 93 to 99" | 9, 22 |

## 20. For P6 (P5-00 10.2)

P6's site plans read, per mine: its cell, founder, final depth (levels for the plan: one level per depth), sealed or not, its end; per lost hold: the ruin, its `hold_lost`, the lair at its cell and the horror (alive at E, its kind, name, hoard); per sealed horror: the hold above it and the mine. Quest hooks: a sealed deep beneath a standing hold, a lost hold with its horror alive, a hidden refuge of a people fled from the deep. Whether the player can break a seal is runtime (Q-P5-07-2); the record is complete for it. (r2, Q-P5-07-2 decided) A sealed horror's lair is a quest place the runtime may open: the player may choose to go there and open it, or a quest may send the player into it; otherwise it stays sealed. (r2) Per deep warren: its settlement, its final depth (kind 98), sealed or lost; a sealed horror under a goblin settlement is the same quest place.

## 21. Open questions

**For Paul** (each also needs a row in `docs/questions-for-paul.md`, the lead's):

All four were **decided by Paul on 2026-11-26** (r2):

| Id | Question | Paul's answer | What the spec does |
| --- | --- | --- | --- |
| Q-P5-07-1 | How often may a whole realm fall to the deep? | "I agree that should be rare 8% sounds right, but an individual city/mine/delving could fall more often than that." | DP5 (≤ 8% of dwarven realms) and DP4 (holds lost, 15 to 40 worlds) as r1 wrote them |
| Q-P5-07-2 | May a sealed horror be released later, in history or by the player? | "Agreed the PC may choose to go there and open it or may be told to go into the sealed place by a quest but otherwise it should stay sealed." | seals hold to E (r1); 20 names the lair a quest place the runtime may open |
| Q-P5-07-3 | The three deep horrors Ymrach, Ysgarth, Zhakul? | "Love it, sounds great." | ship as named (4.1) |
| Q-P5-07-4 | Should allies answer a deep horror? | "I think they might yes." And: "Incidentally, I was thinking that maybe other races that delve, like goblins might also run into these things but less frequently." | aid at the gate (6.11); goblin warrens (6.10) |

No new question for Paul: GW2's bound (10% to 30%) reads his "less frequently"; the lead may put it to him with the first calibrated numbers.

**For the lead:**

| Id | Question | Recommendation (interim) |
| --- | --- | --- |
| L-1 | Schema 13 for one code (`wake` 3)? | Yes: without it a 0.28.0 reader fails with a row message instead of the version message; no DDL changes. Proposals text in 22 |
| L-2 | Deep kinds outside the world cap and the one-lair-per-region test at a waking? | Yes (6.4 "Caps"): keeps the wild and undead rules and M's targets as they are, and a waking is caused, not a random pressure |
| L-3 | The forge's depth in conflict's force (6.3)? | Yes at 15 per level; it is the lever that protects DP9; 0 if C1 or AL5 move |
| L-4 | Audit A1: fix by `credit_units` (changes relations rarely) or state the exception only? | Fix (7.5) |
| L-5 | `settlement.ending` 2 for a hold lost, or a new code 7? | 2: a lost hold is a settlement taken by a monster that lairs there; the ruin event's kind tells it apart; no code-list change for every reader |
| L-6 | The `tagged` segment and the viewer's B4 transform (12 item 4)? | Take; if the viewer engineer finds the transform needs more than the kind, leave it to a viewer round with no report change |
| L-7 | A deep flight without the sack event: P3-06 invariant 3 and the conflict reference widen (7.3) | Accept; the alternative (a last refuge always holds the gate) contradicts design 5's "the survivors flee as from any last refuge" |
| (r2) L-8 | Aid only at the waking's gate, with no refusal event and no grudge? | Yes: it is the one moment a hold stands or falls; a refusal kind would need motives and counts of its own, and a grudge over a monster would move AL targets for little story. Allied hunts stay out (a later monsters round) |
| (r2) L-9 | Goblin warrens as settlements with no site (not a kind 3 mine without a forge, not a new site kind)? | Yes: a kind 3 site means "mine, seller, forge" to P5-04's rules, readers and references; a new site kind would change schema 13's code list, the `[sites]` block, markers and P6. A settlement with events changes none of them, and keeps goblins out of the forge (GW4) |
| (r2) L-10 | Reuse kinds 94 and 96 for warrens (wider `telling`, 94's `site` 0–1) and add only 98 and 99? | Yes: every reader of a deep monster's birth, the lost hold's ruin, renaming rule 8, `civ_ended` and `refugees_arrived` cause words keeps working unchanged; 93 and 95 name `{site}` and so need warren twins |
| (r2) L-11 | One live warren per goblin realm | Yes, the main lever that keeps GW2 under the dwarves' share; raise only if GW1 falls short |
| (r2) L-12 | Kind ids 97 to 99 | Final if no other stage merges first (as 93 to 96) |

## 22. Text for the lead (`schema.md` and `proposals.md`)

**`docs/specs/schema.md`**, changelog row: `13 | 0.29.0 | P5-07 | monster_kind.wake gains 3 deep; event kinds 93 to 96 (delving_deepened, deep_woken, deep_sealed, hold_lost); no DDL change; readers refuse 12`. At `monster_kind.wake`: `1 wild, 2 dead, 3 deep (P5-07)`. At `settlement.ending`: `2 also when the ruin event is a hold_lost (P5-07)`. At `site` kind 3: `depth by its delving_deepened events (P5-07; no column)`. The catalogue rows of 8.1 (the engineer).

**`docs/design/proposals.md`**, entry "Schema 13: deep monsters (P5-07)": *Problem*: deep horrors need a `wake` value the schema 12 code list lacks. *Proposal*: schema 12 → 13 with generator 0.29.0; `monster_kind.wake` gains 3 `deep`; no DDL change, the empty world unchanged; readers refuse 12; depth is derived from `delving_deepened` events, not a `site.depth` column (9); no new `site.ending` or `settlement.ending` code (6.6). Entry "History: P5-07 mountain depths (readings)": the ids of deep kinds sort after `wyvern` (4.1); deep kinds outside the world cap (L-2); the shared flight (L-7); A1 fixed (L-4); the questions Q-P5-07-1 to 4.

(r2) **`schema.md`** changelog row 13 reads `event kinds 93 to 99 (delving_deepened, deep_woken, deep_sealed, hold_lost, ally_aided_hold, warren_delved, warren_sealed)`; the catalogue rows of 8.1. **`proposals.md`**, entry "History: P5-07 r2 (Paul's answers of 2026-11-26)": Q-P5-07-1 to 4 decided as 21; allies may aid a hold at a waking (6.11, L-8); goblins delve deep warrens with no site and no forge (6.10, L-9 to L-11); race field `delving_permille`; four streams (107); no schema change beyond 13. **`docs/questions-for-paul.md`**: rows Q-P5-07-1 to 4 move to decided.

## Revision notes

- r1 (2026-11-26): first draft, for approval.
- r2 (2026-11-26): Paul's answers to Q-P5-07-1 to 4 (21). New: 6.10 goblin deep warrens (race field `delving_permille`, three r2 tuning fields of 4.2 for warrens, kinds 98 `warren_delved` and 99 `warren_sealed`, kinds 94 and 96 widened to six wordings and 94's `site` optional); 6.11 allies' aid at the gate (five tuning fields, kind 97 `ally_aided_hold`, `credit`); 6.12 the build's readings; streams 103 → 107; 7.6 wiring; reader checks, stats, timeline, browser, viewer item 4 and M12; invariants 12 to 15, I1; targets GW1 to GW5, AD1, AD2; calibration steps 8 to 12; tests 15 to 19 and the r2 delta work split. Corrections: the gate samples 218 and 137 (the formula stands; r1's numbers were rounded up); `1,206` and `1,810` in the hold-lost sample; fixture H18 → H19. Schema 13, generator 0.29.0 and report v10 unchanged.
