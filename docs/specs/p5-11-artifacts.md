# P5-11 Artifacts

Status: **r3 (2026-11-28)**, for approval of the r3 passages: r2 (being built: rulers only) plus **corruption of a bearer** (6.12; Paul, 2026-11-28: "Actually, I like the idea that it could also corrupt a hero or a general that bears the artifact"), a pure addition on top of r2 for a second short engineering pass (r2's ruler rule, stream 128, kind 117 and its tuning names are unchanged). r2: r1 as approved by the lead (L-1 to L-11) and built, plus Paul's answers to Q-P5-11-1 to 7 (21) and one new rule he asked for, **corruption by an evil artifact** (6.11). Every passage new or changed in r2 is marked **(r2)**, in r3 **(r3)**; the lists are in the revision notes. Phase P5 History depth, stage 11 (`docs/specs/p5-00-history-depth-overview.md` 3.1, the P5-11 paragraph and its amendment: "its makers are P5-06's figures (casters, or smiths as a new role) and P5-04's forges"; 6.1's row `artifact`, `artifact_holding`, entity type 10). **Base: P5-08 as built** (spec r1a, `docs/reviews/p5-08-*`): generator 0.30.0, schema 14, history report format v11, event kinds 1 to 110, 120 streams, browser groups 0 to 9, 37 tables. This stage: generator **0.31.0**; schema **15**; history report format **v12**; event kinds **111 to 117** ((r2) 117 `artifact_corrupted`; no DDL change) ((r3) **and 118** `bearer_corrupted`); **128** streams ((r2) `hist.artifacts.corrupt`) ((r3) **129**: `hist.artifacts.corrupt_bearer`); browser group 10; 40 tables.

Design: section 5 ("Magical artifacts, made in response to conflicts. Each has a maker, an occasion and a chain of holders and places, and where it lies in the present is a site and a quest hook"; "Dwarves are great forgers. They make wealth and weapons and trade them to the other kingdoms, which ties the mountain peoples into trade and into the making of artifacts"), section 2 (deferred: "artifacts with lineages" beyond this), section 9 ("Ruler lines send the player after artifacts once the artifacts stage of phase P5 exists"), section 13 (the P5 row). Proposals entry "History: magical artifacts are made in response to conflicts" (2026-11-19). Builds on P3-06 (wars, battles, sacks, conquests), P5-02 (monsters, hoards, lairs, sites), P5-04 (mines, forges, partners), P5-06 (figures: generals, heroes, casters, towers; deaths and burials), P5-07 (forge depth, the deep), P5-08 (gods, champions, cults, temples, sanctums).

## 1. Purpose

History makes a few dozen **artifacts** at most per world, each in answer to an event of conflict or of a god (an **occasion**: a war declared, a city sacked, a realm conquered, a throne disputed, a hunt begun against a monster, a town overrun, a horror woken in the deep, a champion raised, a cult founded). Each has a **kind** from data (a sword, a crown, a reliquary, a staff, …), a **name** drawn in its maker's culture, and a **maker**: a named **smith** (a new figure role) at a forge, best of all a dwarven forge-hold with a deep mine, which may forge for an ally or trade partner; a caster at its tower; or a god, for its champion or its cult's temple. From its making an artifact moves through a **chain of holdings**, each with years and the event that moved it: kept in a realm's treasury, borne by a general, hero or champion, hoarded by a monster, lost on a battlefield or in a ruin, buried with its bearer, laid up at a temple, tower or sanctum, taken as spoils, recovered by a hero or a won war. Artifacts are never destroyed in history, so each lies somewhere at the present day: in a realm's hands, in a hoard, a ruin, a grave or a sanctum, which P6 turns into sites and quest hooks.

Artifacts act on the world in small, integer ways: a general's borne arms add to its side's odds in battle, a hero's to its hunt; a realm whose regalia or relic is held by another feels strain toward the holder and its declarations cite the loss; a realm hunts harder a monster that hoards its treasure.

(r2) **An evil artifact corrupts.** A treasure given by a god of the evil camp, kept in a realm's treasury, may turn the heart of the realm's reigning ruler: after it has been kept for a span of years, a keyed draw decides, and a corrupted ruler's temper becomes warlike or grasping for the rest of the reign or until the treasure leaves the realm (Paul, Q-P5-11-3: "an evil relic corrupting a bearer would be great"). The chronicle records it (`artifact_corrupted`), the wars the corrupted ruler declares cite it, and each new ruler faces the treasure anew (6.11).

The stage also carries two P5-08 follow-ups (section 12): battles that cite the god's blessing or curse in force, and a cool-down that stops a cult from spreading back into a realm that just put it down.

## 2. Items

| # | Item | In short | Section | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Data | kind `artifact` (15 shipped); culture `artifact_forms`; tuning `history.artifacts`; two `history.figures` fields; pantheon deeds | 4 | yes |
| 2 | Making | occasions (data), the forge, smiths, casters, gods; names | 6.3 to 6.6 | yes |
| 3 | Holdings | six states, one open holding; moves read from the log | 6.2, 6.7, 6.8 | yes |
| 4 | Effects | win chance, hunt strength, claim strain and cause, hoard motive | 7 | yes |
| 5 | Schema 15 | `artifact_kind`, `artifact`, `artifact_holding`; role 8 smith; entity type 10 | 9 | schema |
| 6 | Report v12, timeline, browser, markers, viewer | `[artifacts]`, realm, figure and monster lines; group 10; glyph 24 | 10, 11 | report and viewer |
| 7 | Follow-ups | P5-08 critic F1 (battles cite acts), F2 (cult return cool-down), F5 (a form word) | 12 | yes |
| 8 | (r2) Corruption | an evil god's artifact kept by a realm corrupts its ruler's temper; event 117; wars cite it; epithet `the Dark` | 4.3, 6.11, 7.7, 8.4 | yes |
| 9 | (r3) Corrupted bearers | a general or hero who bears an evil god's arms or armour: a corrupted general's sieges end in sacks, a corrupted hero forsakes the realm for the wilds; event 118; epithets `the Butcher`, `the Forsworn`; a newly risen hero takes up a kept evil arm | 4.3, 6.12, 7.8, 8.5 | yes |

## 3. Inputs, streams, draws

**Read**: the log from the subsystem's cursor (P3-04 23.3); civilizations (race, culture, capital, home region, ended, visible), settlements (owner, region, cell, status); regions (`seed_x`, `seed_y`); `FigureView` (7.3), `AllianceView::partners`, `forge_depth`, `forge_site` (7.4), monsters and sites through `MonstersSubsystem` (7.5); data `artifact` (4.1), the culture field of 4.2, tuning `history.artifacts` (4.3). No setting scales this stage (`param.event_intensity` scales the wars and sacks it reads).

**Streams**, appended to `core/streams.hpp` after `hist.deep.warren_stir` (the 120th), in this order (**127** streams):

| # | Stream | Key (a, b, k) | Drawn |
| --- | --- | --- | --- |
| 121 | `hist.artifacts.make` | occasion event e, civilization c, 0 | once per (occasion entry's candidate realm) at most (6.3) |
| 122 | `hist.artifacts.class` | e, c, 0 | once per making that passed 121 |
| 123 | `hist.artifacts.kind` | artifact a, 0, 0 | once per artifact |
| 124 | `hist.artifacts.take` | the moving event, artifact a, 0 | once per (event, artifact) at most (M3, M9, M10) |
| 125 | `hist.artifacts.bury` | artifact a, figure f, 0 | once per (artifact, bearer's death) at most (M2) |
| 126 | `hist.artifacts.raid` | the `monster_raid` r, 0, 0 | once per raid on a seat at most (M7) |
| 127 | `hist.names.artifact` | P3-02 4.2's attempt keys (t, culture, artifact id, j) | per name attempt (6.6) |
| 128 | (r2) `hist.artifacts.corrupt` | `D_at` at year t_j: artifact a, civilization c, period j | once per (kept holding of an evil artifact, period j) at most (6.11) |

| 129 | (r3) `hist.artifacts.corrupt_bearer` | `D_at` at year t_j: artifact a, figure f, period j | once per (borne holding of an evil artifact by a general or hero, period j) at most (6.12) |

(r3) A stream of its own, not stream 128 with another key: civilization and figure ids overlap as numbers, so (a, c, j) and (a, f, j) on one stream would give the same value to a ruler's and a bearer's draw whenever c = f; a separate stream name makes the two families independent by construction and leaves r2's keys as built.

(r2) Stream 128 is appended after 127, so the keys of streams 1 to 127 do not change. `D_at(stream; t; a, b, k)` is P5-06 6.3's draw at a fixed year t (`ctx.draw_at`), whatever step evaluates it.

All through `ctx.draw` (the step's year is w0; P3-02 4.1). `chance_pm`, `pick`, `weighted` as P5-02 3. Smiths use the existing streams with their existing keys: `hist.names.person` (figure id) and `hist.figures.life` (figure id, `D_at` the entry year). All arithmetic int64 with checked multiplication; `div` is floor division of non-negative values. Y is the step's year (a quiet step's start), L its length.

# Rules

## 4. Data

### 4.1 Data kind `artifact` (new, required; `game/data/artifacts/`, one file per kind)

A required kind whose directory may hold no definition (then no artifact is ever made). Fields, all required:

| Field | Type | Meaning |
| --- | --- | --- |
| `class` | enum(`arms`, `armour`, `regalia`, `relic`, `staff`) | its class (codes 1 to 5 in this order) |
| `title` | string(16, `artifact_word`) | the word in a name form's `{title}` (`Sword`); grammar `artifact_word`: ASCII letters and `-`, starting with an upper-case letter |
| `noun` | string(16, `artifact_noun`) | the word in sentences (`sword`); grammar: lower-case ASCII letters and single spaces |
| `weight` | int(1, 1000) | its weight among the kinds of its class (6.3) |
| `battle_permille` | int(0, 500) | added to a bearing general's side's odds (7.1); check `artifact_battle_class`: above 0 only for `arms` and `armour` (`battle_permille needs arms or armour`) |
| `hunt_permille` | int(0, 1000) | added to a bearing hero's hunt strength (7.1); 0 for `regalia` and `relic` (check `artifact_hunt_class`) |
| `claim_permille` | int(0, 1000) | strain of its home realm toward a realm that holds it (7.1) |

**Ids and order.** Kind ids 1, 2, … in canonical id order (P0-03 7.4). Shipped (taste; starting values for calibration, 15.2):

| id | def id | class | title | noun | weight | battle ‰ | hunt ‰ | claim ‰ |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `axe` | arms | Axe | axe | 250 | 40 | 60 | 100 |
| 2 | `chalice` | relic | Chalice | chalice | 300 | 0 | 0 | 300 |
| 3 | `crown` | regalia | Crown | crown | 500 | 0 | 0 | 500 |
| 4 | `hammer` | arms | Hammer | hammer | 200 | 40 | 60 | 100 |
| 5 | `hauberk` | armour | Hauberk | hauberk | 300 | 30 | 30 | 100 |
| 6 | `helm` | armour | Helm | helm | 300 | 30 | 30 | 100 |
| 7 | `idol` | relic | Idol | idol | 300 | 0 | 0 | 300 |
| 8 | `orb` | staff | Orb | orb | 300 | 0 | 40 | 100 |
| 9 | `reliquary` | relic | Reliquary | reliquary | 400 | 0 | 0 | 300 |
| 10 | `ring` | regalia | Ring | ring | 200 | 0 | 0 | 300 |
| 11 | `sceptre` | regalia | Sceptre | sceptre | 300 | 0 | 0 | 400 |
| 12 | `shield` | armour | Shield | shield | 400 | 30 | 20 | 100 |
| 13 | `spear` | arms | Spear | spear | 250 | 40 | 80 | 100 |
| 14 | `staff` | staff | Staff | staff | 700 | 0 | 40 | 100 |
| 15 | `sword` | arms | Sword | sword | 500 | 50 | 60 | 150 |

Both fixture roots (`geo`, `geo-small`) gain the fifteen files. Kind data is echoed in table `artifact_kind` (9).

### 4.2 Culture field `artifact_forms` (amends P3-02 3)

Appended after `profane_forms` (P5-08 4.4), required (shipped and both fixture roots): array(string(48, `artifact_form`), 1, 8); grammar `artifact_form`: `name_form`'s with `{root}` exactly once and `{title}` at most once. Shipped (taste; every word checked against the name filter by the engineer, a failing word a data defect):

| culture | `artifact_forms` |
| --- | --- |
| dwarven | `{root}`, `The {title} of {root}`, `{root}'s Rune-{title}` |
| elven | `{root}`, `The {title} of {root}`, `The Silver {title} of {root}` |
| faerie | `{root}`, `The Moon-{title} of {root}`, `The {title} of {root}` |
| goblin | `{root}`, `{root}'s {title}`, `The Black {title} of {root}` |
| human | `{root}`, `The {title} of {root}`, `{root}'s {title}` |
| orcish | `{root}`, `The Red {title} of {root}`, `{root}'s {title}` |

### 4.3 Tuning `history.artifacts` (new fixed id of kind `history`; `game/data/history/artifacts.toml` and both fixture roots)

All required, in this order.

| Field | Type | Start | Meaning |
| --- | --- | --- | --- |
| `occasions` | array(occasion, 0, 32) | 4.4 | what may occasion a making |
| `forge_chance_permille` | int(0, 4000) | 2000 | factor on an occasion's chance when the realm has a forge (6.4) |
| `realm_rest_years` | int(0, 10000) | 50 | a realm has at most one artifact made for it in this many years |
| `world_max` | int(0, 1000) | 40 | no making once a world has this many artifacts |
| `deep_metal_depth` | int(1, 3) | 3 | the forge depth at which an artifact is of deep metal |
| `deep_metal_permille` | int(0, 500) | 30 | added to a deep artifact's `battle_permille` and `hunt_permille` when they are above 0 |
| `force_max_permille` | int(0, 1000) | 150 | cap on a general's sum (7.1) |
| `hunt_max_permille` | int(0, 2000) | 200 | cap on a hero's sum |
| `claim_max_permille` | int(0, 4000) | 1000 | cap on a realm's claim toward one holder |
| `burial_permille` | int(0, 1000) | 300 | chance that a bearer who dies with its realm alive is buried with what it bore (M2) |
| `battle_take_permille` | int(0, 1000) | 500 | chance that a fallen general's arms are taken by the enemy, else lost on the field (M3) |
| `sack_take_permille` | int(0, 1000) | 700 | chance per treasure that a seat's sacker takes it (M9) |
| `capture_take_permille` | int(0, 1000) | 500 | likewise for a captured seat (M10) |
| `raid_take_permille` | int(0, 1000) | 30 | chance that a raid on a seat carries off a treasure (M7) |
| `hoard_hunt_permille` | int(0, 1000) | 50 | added to a realm's hunt chance against a monster that hoards its treasure (7.1) |
| (r2) `corrupt_years` | int(1, 10000) | 25 | the period of 6.11: a kept evil artifact is tested once each this many years of its holding |
| (r2) `corrupt_permille` | int(0, 1000) | 150 | the chance per period that it corrupts the reigning ruler (lever of CR1, CR2) |
| (r2) `corrupt_camp_permille` | record {`good`, `neutral`, `evil`} int(0, 1000) | 1000, 1000, 600 | factor on `corrupt_permille` by the camp of the keeping realm's race (an evil people is less turned by an evil god's gift; lever) |
| (r2) `corrupt_warlike_permille` | int(0, 1000) | 500 | the share of corruptions that make the ruler warlike; the rest grasping (lever of C1, FX2) |
| (r3) `corrupt_bearer_years` | int(1, 10000) | 10 | the period of 6.12 for a borne holding (shorter than `corrupt_years`: a goblin general commands for 12 to 36 years, P5-06 6.9) |
| (r3) `corrupt_bearer_permille` | int(0, 1000) | 300 | the chance per period that it corrupts its general or hero (lever of CB1, CB2); 0 turns 6.12 off, M15 included (I2) |
| (r3) `corrupt_sack_permille` | int(0, 1000) | 500 | added to the sack chance of a siege fall won by a side whose general is corrupted (7.8; lever of CB3, G2) |

Checks: `artifacts_occasion_*` (4.4). **Code defaults** (a tuning built in code): `occasions` empty, the others their starts (inert). The **`zero`** fixture tuning: shipped values with `occasions` empty (I0, 14).

`history.figures` gains, appended after its last field, `smith_age_permille` int(0, 1000) start **250** and `smith_age_span_permille` int(0, 1000) start **300** (a smith's age at entry, per mille of lifespan, P5-06 6.9); code defaults the starts.

(r2) `history.figures`' `deed` enum (P5-06 6.10) gains **`corrupted`**, appended last: for a ruler, 1 when an `artifact_corrupted` (8.4) fell in its reign, else 0; 0 for other roles. The shipped `epithets` list gains one entry, inserted as the **third** (after `the Faithless`, before `the Last`): role `ruler`, deed `corrupted`, `min` 1, `substantial` 0, text **`the Dark`** (ordinary only, so at most `epithet_repeat_max` (2) rulers of a world: a conqueror whose conquests reach `the Conqueror`'s substantial threshold keeps that epithet, since entry 1 is tried first). Both fixture roots likewise; the `zero` fixture tuning keeps the list as shipped.

(r3) The deed `corrupted` also applies to generals and heroes: 1 when a `bearer_corrupted` (8.5) names the figure, else 0 (rulers as r2; 0 for other roles). Two more `epithets` entries, each inserted as the **first entry of its role** (so the corruption is what the figure is remembered by; ordinary only, at most `epithet_repeat_max` (2) bearers a world): before `the Unbeaten`: role `general`, deed `corrupted`, `min` 1, `substantial` 0, text **`the Butcher`**; before `{slayer}`: role `hero`, deed `corrupted`, `min` 1, `substantial` 0, text **`the Forsworn`**. (A hero's `{slayer}` by a substantial deed is now tried second, so a corrupted dragonslayer is `the Forsworn`, within the cap of 2; a third corrupted hero keeps `{slayer}`.)

`history.pantheon` gains, appended, `cult_return_years` int(0, 10000), start **400**, code default 0 (12.2).

### 4.4 Occasions

An occasion is a record {`kind`: event kind key, `role`: role key, `chance_permille`: int(0, 1000), `bearer`: enum(`realm`, `general`, `figure`, `site`), `classes`: record {`arms`, `armour`, `regalia`, `relic`, `staff`} int(0, 1000)}. Checks (`artifacts_occasion_*`, each `occasions[<i>]: …`): `kind` is a kind of the compiled catalogue (`kind <k> is unknown`); `role` is a role of it with entity type civilization (`role <r> of <k> is not a civilization role`); `bearer` `general` needs `kind` `war_declared` (`bearer general needs war_declared`); `figure` needs a figure role in the kind, `site` a site role (`bearer <b> needs a <type> role in <k>`); the class weights are not all 0.

**Shipped** (taste; calibrated in 15.2):

| # | kind.role | chance ‰ | bearer | arms / armour / regalia / relic / staff | reading |
| --- | --- | --- | --- | --- | --- |
| 1 | `war_declared.defender` | 60 | general | 500 / 300 / 0 / 0 / 200 | arms against the attacker |
| 2 | `war_declared.attacker` | 30 | general | 600 / 300 / 0 / 0 / 100 | arms for a war of choice |
| 3 | `sacked.defender` | 100 | realm | 600 / 200 / 0 / 0 / 200 | vengeance for a sack |
| 4 | `civ_conquered.conqueror` | 200 | realm | 0 / 0 / 1000 / 0 / 0 | a crown of conquest |
| 5 | `succession_disputed.civilization` | 120 | realm | 0 / 0 / 1000 / 0 / 0 | regalia to settle a throne |
| 6 | `hero_rose.civilization` | 150 | figure | 700 / 300 / 0 / 0 / 0 | the hero armed for the hunt |
| 7 | `settlement_overrun.civilization` | 150 | realm | 700 / 0 / 0 / 0 / 300 | arms against the monster |
| 8 | `deep_woken.civilization` | 400 | realm | 600 / 0 / 0 / 0 / 400 | arms or wards against the horror |
| 9 | `divine_champion.civilization` | 500 | figure | 600 / 400 / 0 / 0 / 0 | the god arms its champion |
| 10 | `cult_founded.civilization` | 300 | site | 0 / 0 / 0 / 1000 / 0 | a relic for the temple |

Estimated at the starts: 8 to 15 artifacts a default world (wars about 25, sacks about 10, heroes about 10 a world; forges double the chance for about a third of the makings). AR1 (15.1) decides.

### 4.5 Pantheon deeds and cause words (data; amends P5-08 4.3, 8.2)

Deeds appended: `craft`: `artifact_made.maker_realm +20 any`; `war`: `artifact_taken.taker +10 any`. The act cause words gain `artifact_made=a great work forged`, `artifact_taken=a treasure taken`. P5-08 4.4's faerie `cult_forms` `The Hidden Ring of {god}` becomes `The Silver Ring of {god}` (critic F5: "hidden" is a state, not a name).

### 4.6 Fixture content `artifacts-small`

In memory, as P5-08's `pantheon-small`, plus: every shipped occasion with `chance_permille` 1000, `realm_rest_years` 0, `world_max` 500, `burial_permille` 500, `battle_take_permille` 500, `raid_take_permille` 500, so that every path of 6 occurs in a 512 world; (r2) and `corrupt_years` 5, `corrupt_permille` 1000, `corrupt_warlike_permille` 500, so that a corruption, a lifting and a corrupted war occur where an evil artifact is kept; (r3) and `corrupt_bearer_years` 2, `corrupt_bearer_permille` 1000, `corrupt_sack_permille` 1000.

## 5. State

`ArtifactsSubsystem` (new: `worldgen/src/history/artifacts.cpp`, `artifacts_moves.cpp`; header `worldgen/history/artifacts.hpp`; tuning id `history.artifacts`) keeps, in its own members, ids only:

- per artifact (ids 1, 2, … in creation order, H2; at most 100,000, fatal `history: bound: artifacts exceed 100000`): every column of `artifact` (9), and its holdings in order (every column of `artifact_holding`), the last one open;
- per civilization (index civ id − 1): `last_made` (year, −∞ none), `seat` (6.2);
- (r2) per holding of state 1 of an evil artifact (6.11): `checked` (the last period year evaluated; starts at the holding's `from_year`), kept beside the holding;
  ((r3) correction of wording: r2's `checked` is the count `periods(h)` of 6.11's pseudocode, 0 at opening; the pseudocode rules.)
- (r3) per holding of state 2 of an evil artifact (6.12): `periods` likewise (0 at opening); per figure: `corrupt_event` (0 none; the figure's one `bearer_corrupted`) and `corrupt_artifact`, kept for the figure's life;
- indexes rebuilt from the holdings after every move, all ordered: kept by civilization, borne by figure, hoarded by monster (`std::map` keyed by holder id to ascending artifact ids);
- the log cursor.

## 6. Rules

### 6.1 Steps and order

`steps()` = T3, T4, T6, Q4. Installed **last** in `EngineConfig::subsystems`: peoples, conflict, renaming, monsters, relations, alliances, figures, pantheon, **artifacts** (it reads every other subsystem's events of the step). Not a tension source.

| Step | In this order |
| --- | --- |
| T3 | the log (6.7) [this year's T1, T2 and figures' T3 events, last year's T5 and T6 events after the previous cursor]; then `seat` refreshed |
| T4 | the log [peoples', monsters', relations', figures' and pantheon's T4 events]; `seat` refreshed |
| T6 | the log [T5 acts, conflict's battles, sacks, conquests and war ends, figures' falls and stand-downs]; `seat` refreshed |
| Q4 | the log [the quiet step's events]; `seat` refreshed |

For each event of the log, in ascending id: first the **moves** of 6.7 it triggers, then the **making** of 6.3 it may occasion.

(r2) After `seat` is refreshed, every step ends with the **corruption pass** of 6.11: in T4 and T6 its lifting part only; in T3 and Q4 its lifting part and then its draws.

(r3) In T3 and Q4 the bearers' draws of 6.12 follow the rulers' draws of 6.11 in the same pass. The move M15 (6.12) is one more row of the log's moves, read in T4.

### 6.2 Holdings

An artifact is at every moment in exactly one **holding**: (state, holder type, holder id, place), opened by an event and open until the next holding of the artifact opens.

| state (code) | holder (entity type) | place columns (`region`, `settlement`, `site`, `x`, `y`) |
| --- | --- | --- |
| 1 kept | the realm (3), its treasury at its seat | all 0 (movable: the realm's seat at the year) |
| 2 borne | a living figure (6) | all 0 (movable) |
| 3 hoarded | a living monster (8) | all 0 (movable: the monster's lair at the year) |
| 4 lost | a region (5): on a field; a settlement (4): in a ruin; a site (9): in an empty lair | the place: region; settlement (type 4) or 0; site (type 9) or 0; the cell (a ruin's or lair's cell, else the region's (`seed_x`, `seed_y`)) |
| 5 buried | the dead figure (6) | its `death_region` and `death_settlement`; site 0; the death settlement's cell, else the region's seed cell |
| 6 enshrined | a site (9): a temple, sanctum or tower | the site's region, settlement, id and cell |

**The seat** of a realm c is its capital as seen at the end of the subsystem's last step (refreshed for every alive civilization ascending at the end of each step: `seat(c)` = c's capital when it is not 0, else unchanged). A realm's kept artifacts are at `seat(c)`. Moves that test "at the seat" (M7 to M12) compare an event's settlement with `seat(c)` as it was before the step's reads, so a seat sacked and moved in the same step is still recognised.

**Moving** an artifact a to a new holding h with event e in year Y: the open holding gets `to_year` Y and `until_event` e; h opens with `from_year` Y and `event` e; seq + 1. A holding may open and close in one year. `holder` of state 4 with type 5 is the region id.

**A figure bears at most one artifact of each class** (a kept artifact is taken up only by a bearer with none of its class; M1, M5).

### 6.3 Making (occasions)

For an event e of kind κ read in the log, for each occasion o with `o.kind` = κ in list order, for each participant c of e in role `o.role` in ascending seq (skipping 0), until one making succeeds for e (**at most one artifact per event**):

```
eligible: c alive and visible; Y − last_made(c) ≥ realm_rest_years; artifacts made < world_max;
          in Q4: quiet_events_left(c) > 0                                         // else: next c, no draw
F = forge(c)                                                                      // 6.4, pure
p = min(1000, o.chance_permille × (F ≠ 0 ? forge_chance_permille : 1000) div 1000)
if not chance_pm(D(hist.artifacts.make; e, c, 0), p): next c
W = o.classes, with relic := 0 unless e has a god participant;
                    staff := 0 unless caster_of(c) ≠ 0 and it masters an open kind 4 site (FigureView, 7.3)
all W = 0: next c
κ' = the class of index weighted(D(hist.artifacts.class; e, c, 0), W in the order arms, armour, regalia, relic, staff)
a  = the next artifact id; K = the kinds of class κ' in id order (none: next c)
k  = K[weighted(D(hist.artifacts.kind; a, 0, 0), [weight of each])]
maker by 6.4; name by 6.6; append artifact_made (8.1); the first holding by 6.5; last_made(c) = Y
```

A god participant of e is a participant of entity type 2 (the first by seq): `divine_champion`'s `actor`, `cult_founded`'s `god`. e's figure (for `bearer` `figure`) is its first participant of type 6; e's site (for `site`) its first of type 9.

### 6.4 Makers

**The forge** of c, `forge(c)` (pure): c when `forge_depth(c)` ≥ 1 (an open mine, P5-07 7.1); else, among `partners(c)` (P5-08 7.4: an open alliance or trade, ascending id), the one with the largest `forge_depth` ≥ 1, then the smaller id; else 0. So a realm without a mine has an ally or trade partner's forge make its arms: the dwarves forge for the other kingdoms.

The maker, by κ' and e:

| case | `maker_kind` | maker (figure) | god | maker realm (`maker_civ`) | `maker_site` | `deep` | name culture |
| --- | --- | --- | --- | --- | --- | --- | --- |
| e has a god G and κ' ≠ staff | 4 god-given | 0 | G | 0 | e's site when it is an open kind 6 site, else 0 | 0 | c's |
| κ' = staff | 3 caster | `caster_of(c)` | 0 | c | the caster's open kind 4 site | 0 | c's |
| otherwise, F ≠ 0 | 1 when F = c, 2 when F ≠ c (a commission) | a new smith of F | 0 | F | `forge_site(F)` | 1 when `forge_depth(F)` ≥ `deep_metal_depth` | F's |
| otherwise, F = 0 | 1 | a new smith of c | 0 | c | 0 | 0 | c's |

**A smith** f = `FiguresSubsystem::next_figure_id()`, created after the event is appended by `FiguresSubsystem::raise_maker(ctx, f, realm, event)` (7.3): role **8 smith**, race and culture of the realm, civilization the realm, entry Y, ages by P5-06 6.9 with `smith_age_*`, `start_event` the `artifact_made`; its name by P5-06 6.11; it lives and dies as a caster does (`figure_died`, its place the region of its start event), and has no epithet.

### 6.5 The first holding

| class | first holding |
| --- | --- |
| regalia | kept by c |
| staff | borne by the caster (its maker) |
| relic | enshrined at e's site when it is an open kind 6 site, else kept by c |
| arms, armour | `bearer` `general`: borne by `general_of(e, c)` (P5-06 7.1; e the `war_declared`) when it is a living figure bearing none of the class; `figure`: borne by e's figure when it is living, of civilization c and bears none of the class; else (and for `realm`, `site`) kept by c |

The holding's event is the `artifact_made`.

### 6.6 Names (amends P3-02 4.5)

Name kind **`artifact`** is appended after the last kind (`person`), stream `hist.names.artifact`, entity id = artifact id. An attempt is P3-02 4.2's with D = the naming data of the name culture of 6.4, F = that culture's `artifact_forms` with `{title}` replaced by the kind's `title`, c = that culture's `culture.id`. The name is P3-02 4.3's (64 attempts, the registry, the numbered fallback), through the name filter. Drawn when the artifact is made, before the event is appended (H2).

### 6.7 Moves (read from the log)

In each row, "each" is in ascending artifact id; a move that writes an artifact event writes it at once, then makes the holding with that event; a **quiet move** writes no event and uses the triggering event as the holding's event. Quiet steps: 6.8.

| # | Event read | Artifacts | Move |
| --- | --- | --- | --- |
| M1 | `general_appointed` (f, c) | for class arms, then armour: when f lives and bears none of the class, the smallest-id artifact of the class kept by c | borne by f; quiet |
| M2 | `figure_died` (f) | each borne by f | a staff whose maker is f, when f's kind 4 site is open: enshrined there, quiet. Else when f's civilization c' is alive and visible: if `chance_pm(D(hist.artifacts.bury; a, f, 0), burial_permille)`: buried with f, **`artifact_buried`**; else kept by c', quiet. Else (c' ended or hidden): buried with f, `artifact_buried` (no draw) |
| M3 | `fell_in_battle` (f, c, enemy x) | each borne by f | if x is alive and `chance_pm(D(hist.artifacts.take; e, a, 0), battle_take_permille)`: kept by x, **`artifact_taken`** (`how` 1); else lost on the field: state 4, holder the battle's region (type 5), its seed cell, **`artifact_lost`** |
| M4 | `hero_slain` (f, c, m) | each borne by f | hoarded by m, **`artifact_hoarded`** (`how` 1) |
| M5 | `monster_slain_by_hero` (m, c, hero h), `monster_slain` (m, c) | each hoarded by m | arms or armour, when h ≠ 0 lives and bears none of the class: borne by h; else kept by c; **`artifact_recovered`** (`how` 1) |
| M6 | `monster_died` (m) | each hoarded by m | lost in m's lair (state 4, holder the lair site, type 9; the lair's place); quiet |
| M7 | `monster_raid` (m, c) at settlement t = `seat(c)` | when c keeps one or more and `chance_pm(D(hist.artifacts.raid; e, 0, 0), raid_take_permille)`: the smallest-id kept by c | hoarded by m, `artifact_hoarded` (`how` 2) |
| M8 | `hold_lost` (m, c) at t = `seat(c)` | each kept by c | hoarded by m, `artifact_hoarded` (`how` 3) |
| M9 | `sacked` (attacker x, defender d) at t = `seat(d)` | each kept by d | if x alive and `chance_pm(D(hist.artifacts.take; e, a, 0), sack_take_permille)`: kept by x, `artifact_taken` (`how` 2); else if t is ruined at the read: lost in t's ruin (holder t, type 4), `artifact_lost`; else unchanged |
| M10 | `settlement_captured` (x, d) at t = `seat(d)` | each kept by d | if `chance_pm(D(hist.artifacts.take; e, a, 0), capture_take_permille)`: kept by x, `artifact_taken` (`how` 3); else unchanged (the court carried it away) |
| M11 | `civ_conquered` (c by k) | each kept by c | kept by k, `artifact_taken` (`how` 4), when k is alive; else as M12 |
| M12 | `civ_ended` (c) | each kept by c | when `seat(c)` = t ≠ 0 and t is ruined: lost in t's ruin, `artifact_lost`; when t stands (another's): kept by t's owner, `artifact_taken` (`how` 4); t = 0: lost at c's home region's seed cell, `artifact_lost` |
| M13 | `peace`, `war_won` (winner w, loser l) | each whose home (`artifact.civ`) is w and which is kept by l or borne by a living figure of civilization l | kept by w, **`artifact_recovered`** (`how` 2) |
| M14 | `cult_suppressed` (k, c, sanctum s′ 0–1) | each enshrined at a temple of k that ended with this event (ending 8) | enshrined at s′ when s′ ≠ 0; else kept by c; quiet |

No other event moves an artifact. In particular: a borne artifact stays with its bearer through an exile, a war's end, a realm's end and a stand-down; a hoard follows its monster to a new lair (the holder is the monster); a site that ends (a tower abandoned, a temple ended with code 4 or 7, a lair after M6) keeps what is enshrined or lost in it; an artifact is never destroyed; a lost, buried or enshrined artifact is never found again in history (only M5's slaying of a keeper, M13's won war and M14's flight move an artifact out of another's keeping; Q-P5-11-6).

### 6.8 Quiet steps and persistence

Artifacts persist through quiet steps (P5-00 4.1). In Q4 every move of 6.7 is decided as in a tick (the same tests and draws); an artifact event is written only when `quiet_events_left` of its charged civilization (its first civilization participant, 8.1) is above 0, otherwise the move is quiet (no event; the triggering event is the holding's event). A making in Q4 needs the budget before its draw (6.3). In practice Q4 reads `figure_died`, `monster_died`, `peace`, `war_won`, `civ_ended`, `general_appointed` (none in quiet steps), `cult_suppressed` and `cult_founded`.

### 6.9 The present

Nothing runs at the end: every artifact's last holding is open at E, and `artifact.status` is its state. Where it lies at E: kept (the holder's capital at E, or its seat when the realm is hidden), borne (with the living figure), hoarded (in the monster's lair at E), or the place columns of a lost, buried or enshrined holding. The library function `artifact_place_at(view, artifact, year)` (11.1) gives the place for any year.

### 6.10 P3-04 6.9, visibility

1. `ArtifactsSubsystem` founds nothing, opens and closes no relation and changes no population, status or owner. It creates figures only through `raise_maker`, never inside a loop over another subsystem's lists; its loops run over ids up to a count taken before the loop; members hold ids.
2. Not a tension source; no intervention.
3. Order: last in `subsystems` (6.1); `stages/history.cpp` sets `conflict.set_artifacts(&artifacts)`, `monsters.set_artifacts(&artifacts)`, and `artifacts.set_figures(&figures)`, `set_alliances(&alliances)`, `set_sites(&monsters)`.
4. **Visibility**: artifacts sees every change of the step (it is last). Its own changes reach the others at their next call: arms taken up in T3 count in that year's T6 battles; a hoard taken in T4 adds to hunts from the next T4; a claim from a T6 sack adds strain from the next T2.
5. Keys: civilization, figure, monster, site, artifact and event ids, never list positions.
6. (r2) The corruption pass (6.11) creates figures only through `FiguresSubsystem::corrupt_reign` (a ruler named by the deed, 7.7), after the step's log is read; it moves no artifact.

### 6.11 (r2) Corruption by an evil artifact (Paul, Q-P5-11-3, 2026-11-28)

**Evil.** An artifact a is **evil** when `a.god` ≠ 0 and that god's `camp` is 2 (`evil`, P5-08 4.1). Nothing else counts: an artifact's class, kind, maker and holders do not make it evil. Since `god` is set only for `maker_kind` 4, only god-given arms, armour, regalia and relics can be evil. Derived from rows; no column.

**Who.** Only the **reigning ruler of a realm that keeps an evil artifact** (holding state 1, holder c) is exposed. A bearer is not (a champion bearing its god's gift is its devotee, and regalia and relics are never borne, invariant 4); a hoard, a ruin, a grave and a shrine corrupt no one. The realm may be the artifact's home or any realm that took it.

**When and how.** T = `corrupt_years`. For a kept holding h of an evil artifact a by c, opened in year f_h, the **periods** are the years t_j = f_h + j × T, j = 1, 2, …. Each period is evaluated once, in the first corruption pass with step year Y ≥ t_j, and draws only when c's reign is not already corrupted. So every ruler who reigns under the kept artifact faces a draw each T years, a new ruler faces new draws, and an artifact taken and later regained starts its periods again from its new holding.

```
lifting (end of T3, T4, T6, Q4), each alive c ascending:
    R = figures.reign_brief(c)                                   // 7.7
    if R.corrupt_artifact = a ≠ 0 and a's open holding is not (state 1, holder c):
        figures.lift_corruption(c)                               // no event
draws (end of T3 and Q4, after lifting), each alive c ascending:
    R = figures.reign_brief(c); if R.number = 0: next c
    for each a kept by c, ascending id, with evil(a):
        h = a's open holding; f = h.from_year
        for j = periods(h) + 1, … while f + j × T ≤ Y:
            t = f + j × T
            if figures.reign_brief(c).corrupt_event ≠ 0: periods(h) = j; continue      // in force: no draw
            if Q4 and quiet_events_left(c) = 0: next c                                  // periods unchanged
            p = corrupt_permille × corrupt_camp_permille[camp of c's race] div 1000
            u = pick(D_at(hist.artifacts.corrupt; t; a, c, j), 1000)
            periods(h) = j
            if u < p:
                τ' = 1 (warlike) if u < p × corrupt_warlike_permille div 1000, else 3 (grasping)
                append artifact_corrupted e (8.4; year Y)
                figures.corrupt_reign(ctx, c, a, τ', e)          // 7.7
```

`periods(h)` is 0 when h opens (5). The corruption is written even when τ' equals the reign's drawn temper (the treasure feeds what was there). At most one corruption is in force for a realm at a time.

**The effect.** From the corruption until it ends, c's **effective temper** τ*(c) is τ' (7.7): every reader of a reign's temper (strain, allied strain, the grant of spoils, betrayal motives, cult suppression) reads τ'. The drawn temper stays in the reign and in `figure.temper`. A `war_declared` of a corrupted attacker cites the corruption (7.7). The ruler earns the deed `corrupted` (4.3), so may be remembered as `the Dark`.

**What ends it.** (1) The reign's end (death, a disputed succession, the realm's end): the next reign starts uncorrupted (P5-06 6.3) and faces the next periods. (2) The artifact leaving c's keeping by any move of 6.7 (taken, hoarded, lost, recovered by its home, taken up by a general of c (M1: borne is not kept)): lifted at the end of the step of the move. Nothing else; corruption moves, destroys or finds no artifact, and lost things stay lost (Q-P5-11-2, 6).

**Q4.** The pass runs at the quiet step's Y; periods with t_j in (Y, Z] wait for the next pass. The event is charged to c (`quiet_events_left`), checked before the draw; a realm with no budget keeps its periods for the next pass (the draw is keyed by t_j, so a later evaluation gives the same result).

### 6.12 (r3) Corruption of a bearer (Paul, 2026-11-28: "Actually, I like the idea that it could also corrupt a hero or a general that bears the artifact")

This reverses r2's L-12 ("a bearer is not exposed") for generals and heroes; 6.11 is unchanged.

**Who.** A living figure f of role **3 general** or **4 hero** that bears (holding state 2) an evil artifact (6.11's definition; only arms and armour are ever borne, so an evil god's arms or armour). **The champion (role 6) is immune**: the god armed its own devotee, and the corruption takes the **later bearers** who inherit or take the gift (L-18). Casters bear only staffs, which are never god-given. A figure is corrupted **at most once** in its life.

**How the gift reaches a later bearer** (the r1 and r2 moves, plus M15):

| Path | Rule | How often |
| --- | --- | --- |
| the champion dies, its realm alive | M2: kept by the realm (85% with `burial_permille` 150), then **M1**: the realm's next `general_appointed` takes up the smallest-id kept arms (then armour) when the general bears none of the class | the main path: an evil god's champions are armed at about 0.3 a default world (gods make 7% of artifacts, a third of the gods are evil, divine champions most of them), and a realm with wars appoints a general within years |
| a bearing general falls | M3: kept by the enemy (850‰), then the enemy's next general by M1 | a chain inside wars |
| a hoarding monster is slain by a hero | M5: borne by the hero | rare |
| **(new) a hero rises** | **M15**, below | as often as heroes rise in a realm that keeps an evil arm |

So generals have a real pool through M1, which already exists. Heroes had only M5, nearly a dead path, so r3 adds one small move:

| # | Event read | Artifacts | Move |
| --- | --- | --- | --- |
| M15 | `hero_rose` (f, c), only when `corrupt_bearer_permille` > 0 | for class arms, then armour: when f lives and bears none of the class, the smallest-id **evil** artifact of the class kept by c | borne by f; quiet (as M1). The dark gift finds a bearer: a non-evil kept artifact is never taken up by M15, so r1's moves of every other artifact are unchanged |

6.7's sentence "No other event moves an artifact" reads with M15 added.

**When and how** (end of T3 and Q4, after 6.11's draws). Tb = `corrupt_bearer_years`. For the open holding h of each evil artifact a in state 2, ascending artifact id, held by figure f (opened in f_h):

```
B = figures.brief(f)
if not B.alive or B.role ∉ {3, 4} or corrupt_event(f) ≠ 0: next a            // champions, the dead, the already corrupted
c = B.civ
for j = periods(h) + 1, … while f_h + j × Tb ≤ Y:
    t = f_h + j × Tb
    if Q4 and quiet_events_left(c) = 0: next a                                  // periods unchanged
    u = pick(D_at(hist.artifacts.corrupt_bearer; t; a, f, j), 1000)
    periods(h) = j
    if u < corrupt_bearer_permille:
        append bearer_corrupted e (8.5; year Y)
        corrupt_event(f) = e; corrupt_artifact(f) = a
        figures.corrupt_bearer(ctx, f, e)                                       // 7.8
        next a
```

No camp factor (a bearer's people does not shield the bearer; L-20). A figure bearing two evil artifacts (an arm and an armour) is tested on each, the smaller id first, and corrupted once at most.

**The consequence, one per role** (through existing rules only):

- **General: he puts cities to the sword.** While a corrupted general lives and bears the corrupting artifact (**in force**), a siege fall won by the side he leads in that war is sacked more often: P3-06 7.5 item 1's sack chance gains `corrupt_sack_permille` (inside its `min(1000, …)`), and a `sacked` so decided also cites the `bearer_corrupted` (7.8). The general earns `the Butcher`. Chosen over a claim to the throne, which would need a new path in P5-06 6.5 (a crisis is peoples' event, and its claimant is a new figure): the sack rule is a single term and a cause, and a sack is the most visible beat of a war.
- **Hero: he forsakes the realm for the wilds.** At once, `corrupt_bearer` makes the hero an outlaw: status 2 (alive, in exile), its place region the `bearer_corrupted`'s region, no host; c's hero becomes 0, so the outlaw no longer hunts for c (`hero_of(c)` gives no hero) and c may raise a new hero at its next raid (P5-06 6.7's "no living hero" counts only c's serving hero). The outlaw keeps its arms (a borne artifact stays with its bearer through an exile, 6.7) and dies of age as an exile (`figure_died`, `how` 2, 6.9). The hero earns `the Forsworn`. Chosen over keeping a hoard (M5 is rare, so it would hardly show): the exile path exists, and the fate `in the wilds` is already a report word.

**What ends it.** A general's corruption is in force until his death (any of 6.9, a fall in battle) or until he no longer bears the artifact (taken, lost, hoarded, buried: by the existing moves only); the deed and epithet stay. A hero's outlawry is for life (the deed is done); its artifact moves on its death by M2 as for any bearer: buried with it, or kept by its realm (where 6.11 then exposes the ruler, and M1 or M15 the next general or hero: the gift goes on turning hearts). Nothing is destroyed; lost things stay lost.

**Q4.** As 6.11: the event is charged to f's civilization and needs its budget before the draw; periods with t_j > Y wait.

## 7. Seams

### 7.1 `ArtifactView` (new, `worldgen/history/artifacts.hpp`; implemented by `ArtifactsSubsystem`)

```cpp
class ArtifactView {
public:
    virtual ~ArtifactView() = default;
    virtual std::int64_t battle_permille(std::int64_t figure) const = 0;          // [0, force_max_permille]
    virtual std::int64_t battle_artifact(std::int64_t figure) const = 0;          // artifact_made of the smallest-id contributor; 0 none
    virtual std::int64_t hunt_permille(std::int64_t figure) const = 0;            // [0, hunt_max_permille]
    virtual std::int64_t claim_permille(std::int64_t home, std::int64_t holder) const = 0;  // [0, claim_max_permille]
    virtual std::int64_t claim_event(std::int64_t home, std::int64_t holder) const = 0;     // 0 when no claim
    virtual std::int64_t hoard_motive_permille(std::int64_t monster, std::int64_t civ) const = 0;
};
```

- `battle_permille(f)` = min(`force_max_permille`, Σ over the artifacts borne by f of (`battle_permille` + `deep_metal_permille` when `deep` and `battle_permille` > 0)); `hunt_permille` likewise with `hunt_permille`, capped `hunt_max_permille`.
- `claim_permille(h, b)` (h ≠ b) = min(`claim_max_permille`, Σ `claim_permille` of the artifacts whose home is h and which are kept by b or borne by a living figure of civilization b); 0 when h or b has ended. `claim_event(h, b)` = the largest holding `event` among them.
- `hoard_motive_permille(m, c)` = `hoard_hunt_permille` when m hoards an artifact whose home is c or which was kept by c or borne by a figure of c in the holding before the hoard, else 0.

Unset (tests, the I0 build): every read 0.

### 7.2 Conflict and monsters (amend P5-06 7.6, P3-06 5.2 and 7.1, P5-06 7.7)

- **Win chance** (P5-06 7.6): A′ = A × (1000 + skill(G_x) + `battle_permille`(G_x)) div 1000, and D′'s factor likewise (1000 + skill(G_y) + `battle_permille`(G_y)); casualties and the score unchanged. G = 0: 0.
- **Strain** (P3-06 5.2, after P5-06 7.2's temper factor): rate(a, b) = rate(a, b) × (1000 + `claim_permille`(a, b)) div 1000.
- **A declaration's cause**: a `war_declared` of a on b with `claim_permille`(a, b) > 0 at its T2 also cites `claim_event`(a, b) (the causes listed ascending, distinct).
- **Battle causes** (with 12.1): a `battle_led` whose winner's general has `battle_artifact` ≠ 0 also cites it.
- **Hunts** (P5-06 7.7): S_c = H_c × prowess div 1000 × (1000 + H.might_permille + `hunt_permille`(H.figure)) div 1000; p_hunt gains `hoard_motive_permille`(m, c) in both terms of its min, as H.hunt_permille.

### 7.3 Figures (amends P5-06 7.1, P5-08 7.5)

`void raise_maker(StepContext&, std::int64_t figure, std::int64_t civ, std::int64_t event)` (role 8, ages by `smith_age_*`; fatal if `figure` is not the next id); `FigureView` gains `struct Brief { std::int64_t role = 0, civ = 0; bool alive = false; std::int64_t death_region = 0, death_settlement = 0; }; virtual Brief brief(std::int64_t figure) const = 0;` and `virtual std::int64_t caster_site(std::int64_t figure) const = 0;` (the open kind 4 site whose master is the caster, or whose last master it was when it has died; 0 none). Smiths are in role counts, the browser and the report as figures; they lead nothing and earn no epithet (6.10 of P5-06 lists no role `smith`).

### 7.4 `AllianceView::forge_site` (amends P5-07 7.1)

`virtual std::int64_t forge_site(std::int64_t c) const = 0;`: the open mine of c with the largest `mine_depth`, then the smallest id; 0 none.

### 7.5 Monsters

Const reads used: `monster_alive(m)`, `lair_of(m)` (site id), site rows (kind, region, cell, settlement, open, `ending`, `ending_event`). The site seam is unchanged (artifacts opens no site).

### 7.6 Rendering (amends P3-04 8.3, 8.4)

- **Display name** of entity type 10: the artifact's name.
- **Form `kind`** for a role of entity type 10: `a` or `an` (an before a noun starting with a, e, i, o or u) + ` ` + the kind's `noun`, then ` of deep iron` when `deep` is 1 (`an axe`, `a sword of deep iron`).
- **Empty roles**: the forms `all` and `titled` of a role with no participant render nothing (P5-08's `titled`; already so for `all`).
- `HistoryView::load` reads `artifact_kind` and `artifact`. Not a schema change by themselves.

### 7.7 (r2) Corruption seams (amend P5-06 7.1 to 7.3, 6.10, P5-04a 2, P3-06 7.1)

**`FigureView`** gains:

```cpp
struct ReignBrief { std::int64_t number = 0, start = 0, figure = 0, corrupt_event = 0, corrupt_artifact = 0; };
virtual ReignBrief reign_brief(std::int64_t civ) const;   // number 0: no reign; corrupt_* 0: not corrupted now
```

(a default returning `{}`, as `temper` has one).

**`FiguresSubsystem`** gains, on the current reign of each line, `corrupt_temper` (−1 none), `corrupt_event` (0), `corrupt_artifact` (0) and `was_corrupted` (bool); a new reign starts with −1, 0, 0, false. Two calls, for `ArtifactsSubsystem` only:

- `void corrupt_reign(StepContext& ctx, std::int64_t civ, std::int64_t artifact, std::int64_t temper, std::int64_t event)`: fatal when c has no reign, is ended, or its reign's `corrupt_event` ≠ 0, or `temper` is not 1 or 3. If the reign is unnamed and `name_rulers` is true, it is **named by the deed** (P5-06 6.4 rule 3, with `artifact_corrupted` added to that rule's kinds): the record event is `event`, no event is appended, `office_from` = the reign's start. Then `corrupt_temper` = temper, `corrupt_event` = event, `corrupt_artifact` = artifact, `was_corrupted` = true (moved to the figure's tallies with the others).
- `void lift_corruption(std::int64_t civ)`: `corrupt_temper` = −1, `corrupt_event` = 0, `corrupt_artifact` = 0; `was_corrupted` stays.

**The override enters in one place.** The private function `tau(const Line&)` = `corrupt_temper` when ≥ 0, else the drawn `temper`; the readers `strain_permille`, `allied_strain_permille` and `spoils_permille` index their tables by `tau(l)` instead of `l->reign.temper`, and `temper` returns `tau(l)`. Nothing else changes: the race normalization `mean_r` is from data, the stored `figure.temper` and the reign log keep the drawn temper, and every reader that already calls these four (conflict's strain, P5-04's allied strain and grant, P5-04a's betrayal motive `temper(c) = 3`, P5-08's suppression `temper_of`) sees the corrupted temper with no change of its own.

**Epithet.** The deed `corrupted` (4.3) of a ruler is its `was_corrupted`.

**A declaration's cause** (amends 7.2's bullet; P3-06 7.1): a `war_declared` of a on b, at its T2 or Q4, also cites `reign_brief(a).corrupt_event` when not 0 (with the claim's event; causes ascending, distinct). Conflict already holds `figures_`. No other event cites a corruption.

**Visibility.** A corruption written in a T3 pass acts from the next call of each reader: conflict's T2 of the next year, alliances' and pantheon's next steps. A lifting at the end of T4 or T6 likewise; figures' own reign change in T3 clears the override at once (the new reign has none).

### 7.8 (r3) Bearer seams (amend P3-06 7.5, P5-06 6.7, 6.9, 6.10, 9 F28)

- **`ArtifactView`** gains `virtual std::int64_t bearer_corruption(std::int64_t figure) const;` (default 0): the figure's `bearer_corrupted` event when it is a living general that bears its `corrupt_artifact` now, else 0 (heroes: 0, since their consequence is applied once by figures).
- **`FiguresSubsystem::corrupt_bearer(StepContext& ctx, std::int64_t figure, std::int64_t event)`**, for `ArtifactsSubsystem` only: fatal unless the figure lives, has role 3 or 4 and was not corrupted before. Sets the figure's deed `corrupted` (4.3). For a hero (role 4) also: status 2, its place region = `event`'s region (P5-06 6.9's "place" of an exile without host), its realm's hero = 0, and the figure is skipped by `hero_of` and by every later rise test as c's hero. No event is appended by figures; no relation opens or closes (a hero has none). A general keeps serving its war (`served` unchanged).
- **Sacks** (P3-06 7.5 item 1): for a fall in war w taken by x, g = `figures.general_of(w, x).figure`; when g ≠ 0 and `artifacts.bearer_corruption(g)` = e_g ≠ 0: the chance is `min(1000, sack_base + h(x, y) × sack_hostility div 1000 + corrupt_sack_permille)` and, when sacked, the `sacked` also cites e_g (causes ascending, distinct). The draw and its key are unchanged; with e_g = 0 nothing changes. Conflict already holds both views (7.2, 12.1).
- **Reader check F28** (P5-06 9): status 2 is allowed for role 2, **or for role 4 when a `bearer_corrupted` names the figure**; the message text `in exile but not a claimant` is unchanged.
- **One hero a realm** (P5-06 6.7 and its invariant): "a realm has at most one living hero" counts heroes with status 1 only; an outlaw (status 2) is not its realm's hero, so a new hero may rise beside it.
- **Death of an outlaw** (P5-06 6.9): `how` 2 (in exile), its place region as set above, settlement 0.
- **Visibility**: a corruption in the T3 pass acts on the same year's T6 sieges (conflict's T6 reads the view live); a hero outlawed in T3 or Q4 hunts no more from monsters' next T4.

## 8. Events

### 8.1 Catalogue (appended after 110; ids final if no other stage merges first)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values |
| --- | --- | --- | --- | --- | --- | --- |
| 111 | `artifact_made` | 1 | 1 | 2 | 1 `artifact` artifact; 2 `civilization` civ; 3 `maker` figure 0–1; 4 `god` god 0–1; 5 `maker_realm` civ 0–1; 6 `site` site 0–1 | `maker_kind` (1–4), `deep` (0–1) |
| 112 | `artifact_taken` | 1 | 1 | 2 | 1 `artifact` artifact; 2 `taker` civ; 3 `from` civ; 4 `figure` figure 0–1 | `how` (1–4) |
| 113 | `artifact_lost` | 1 | 1 | 2 | 1 `artifact` artifact; 2 `civilization` civ; 3 `figure` figure 0–1 | — |
| 114 | `artifact_hoarded` | 1 | 1 | 2 | 1 `artifact` artifact; 2 `monster` monster; 3 `civilization` civ; 4 `figure` figure 0–1 | `how` (1–3) |
| 115 | `artifact_recovered` | 1 | 1 | 0 | 1 `artifact` artifact; 2 `civilization` civ; 3 `figure` figure 0–1; 4 `monster` monster 0–1; 5 `from` civ 0–1 | `how` (1–2) |
| 116 | `artifact_buried` | 1 | 1 | 2 | 1 `artifact` artifact; 2 `figure` figure; 3 `civilization` civ | — |

(artifact = entity type **10**, first use; god 2, civ 3, figure 6, monster 8, site 9; unlisted counts 1–1.) Participants and places:

- 111: `maker` exactly when `maker_kind` 1 to 3; `god` exactly when 4; `maker_realm` exactly when 2; region: the maker site's region, else c's capital's region, else c's home region; settlement: the maker site's settlement when not 0. Causes: e (the occasion), and when `deep` the forge site's last `delving_deepened`, ascending.
- 112: `from` the realm it was taken from (the fallen bearer's realm for `how` 1); `figure` the fallen bearer (`how` 1 only); region and settlement: the cause's. `how`: 1 on the field, 2 in a sack, 3 at a capture, 4 with the fall of the realm.
- 113: `civilization` the realm it was lost from (the bearer's realm on a field); `figure` the fallen bearer; region and settlement: the place of 6.2.
- 114: `civilization` the realm it came from (the slain hero's realm, the raided or lost realm); `figure` the slain hero; region the lair's; settlement t for `how` 2 and 3. `how` 1 its bearer slain, 2 a raid, 3 a hold lost.
- 115: `figure` the hero who took it up or whose realm took it (`monster_slain_by_hero`); `monster` for `how` 1; `from` for `how` 2; region: the lair's (1), the winner's capital's region else home region (2). `how` 1 from a slain keeper's hoard, 2 by a won war.
- 116: region and settlement: the grave of 6.2.
- Causes of 112 to 116: the one triggering event. The charged civilization (quiet steps): the first civilization participant.

### 8.2 Templates and salience (data, `history.report`)

| key | sal. | template | summary? |
| --- | --- | --- | --- |
| `artifact_made` | 45 | `{@causes:war_declared=the coming of war,sacked=the sack of a city,civ_conquered=a realm conquered,succession_disputed=a throne disputed,hero_rose=a hunt begun,settlement_overrun=a town overrun,deep_woken=a horror woken in the deep,divine_champion=a champion raised,cult_founded=a cult founded|In answer to |, }{maker:all}{god:titled}{maker_kind?| forged| forged| wrought| bestowed} {artifact}, {artifact:kind},{maker_kind?| for| for| for| upon} {civilization}{maker_kind?|| at the forges of ||}{maker_realm:all}.` | yes |
| `artifact_taken` | 40 | `{@causes:fell_in_battle=the fall of its bearer,sacked=the sack of a city,settlement_captured=the taking of a city,civ_conquered=the fall of a realm,civ_ended=the end of a realm|After |, }{taker} took {artifact}, {artifact:kind}, from {from}.` | companion |
| `artifact_lost` | 35 | `{@causes:fell_in_battle=the fall of its bearer,sacked=the sack of a city,civ_ended=the end of a realm|After |, }{artifact}, {artifact:kind}, was lost to {civilization} in {@region}.` | companion |
| `artifact_hoarded` | 50 | `{@causes:hero_slain=the death of its bearer,monster_raid=a raid,hold_lost=the fall of a hold|After |, }{monster} carried {artifact}, {artifact:kind}, off from {civilization} to its hoard.` | yes |
| `artifact_recovered` | 50 | `{@causes:monster_slain_by_hero=the slaying of its keeper,monster_slain=the slaying of its keeper,peace=a war won,war_won=a war won|After |, }{civilization} recovered {artifact}, {artifact:kind}{how?|, from the hoard of |, from }{monster:all}{from:all}.` | yes |
| `artifact_buried` | 25 | `{artifact}, {artifact:kind}, was laid in the grave of {figure} of {civilization}.` | no |

Samples (illustrative names): `In answer to the coming of war, Thrain forged Kazrak, a sword of deep iron, for the Kingdom of Cear at the forges of the Hold of Durn.` · `In answer to a champion raised, Ormala the Keeper of Oaths bestowed the Silver Helm of Ithil, a helm, upon the House of Finen.` · `After the fall of its bearer, the Gub Horde took Kazrak, a sword of deep iron, from the Kingdom of Cear.` · `After the death of its bearer, Saelith the Black carried the Crown of Aldmar, a crown, off from the Kingdom of Cear to its hoard.` · `After the slaying of its keeper, the Kingdom of Cear recovered Kazrak, a sword of deep iron, from the hoard of Saelith the Black.` · `Kazrak, a sword of deep iron, was laid in the grave of Maela Dragonslayer of the Kingdom of Cear.`

### 8.3 Causes, companions, saliences

Causes as 8.1. **Companions** (amends P3-07 24.2's table, with P5-00 7.1): `fell_in_battle`, `sacked`, `settlement_captured`, `civ_conquered`, `civ_ended`: every `artifact_taken` or `artifact_lost` with e among its causes; `hero_slain`, `hold_lost`: every `artifact_hoarded` with e among its causes; `monster_slain`, `monster_slain_by_hero`, `peace`, `war_won`: every `artifact_recovered` with e among its causes. Companions are not followed further. (r2) `artifact_corrupted` is no companion and has none. Saliences: a making reads like a cult's founding (45 to 50); a hoard taken and a recovery like a hero slain (50); takings and losses read with their cause; a burial is bookkeeping. Estimated summary candidates a default world: 8 to 15 makings, 1 to 3 hoards and recoveries, inside P5-02 12.9's budget (T18).

### 8.4 (r2) Event 117 `artifact_corrupted`

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values |
| --- | --- | --- | --- | --- | --- | --- |
| 117 | `artifact_corrupted` | 1 | 1 | 2 | 1 `artifact` artifact; 2 `civilization` civ; 3 `god` god | `temper` (1–3; only 1 or 3 occur) |

- `god` = `artifact.god` (camp 2); `temper` τ' (1 warlike, 3 grasping); region and settlement: `seat(c)` and its region, else (seat 0) c's home region and settlement 0; year Y.
- Causes: the open holding's `event` (the event that put it in c's keeping) and the artifact's `made_event`, ascending, distinct (one cause when the holding is seq 1).
- The ruler is not a participant: the text names it through `{civilization:under}` (P5-06 7.9), which is never empty with `name_rulers` (the corruption names an unnamed reign, 7.7).
- Charged civilization (Q4): c. Browser group `artifacts` (10); `event_symbol` plain.

| key | sal. | template | summary? |
| --- | --- | --- | --- |
| `artifact_corrupted` | 50 | `{artifact}, {artifact:kind} of {god:titled}, turned the heart of {civilization}{civilization:under}{temper?|| to war|| to greed}.` | yes |

Sample (illustrative names): `The Crown of Aldmar, a crown of Ghor the Red Banner, turned the heart of the Kingdom of Cear under Aldric II to greed.` · `Kazrak, a sword of Murn the Hollow Keeper, turned the heart of the Gub Horde under Grishnak to war.` (A god-given artifact is never of deep iron, 6.4, so the `kind` form is always `a <noun>`.) Salience 50, as a hoard taken: a turn of a reign is a story beat. Estimated 0 to 2 a default world (CR2), inside T18.

### 8.5 (r3) Event 118 `bearer_corrupted`

A kind of its own rather than a role added to 117: r2's kind 117, its template and its reader checks stay as built, and each kind has one template and one participant rule.

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values |
| --- | --- | --- | --- | --- | --- | --- |
| 118 | `bearer_corrupted` | 1 | 1 | 0 | 1 `artifact` artifact; 2 `figure` figure; 3 `civilization` civ; 4 `god` god | `role` (3–4) |

- `figure` the bearer; `civilization` its realm (`figure.civ`); `god` = `artifact.god` (camp 2); `role` the figure's role; region: `seat(c)`'s region when c is alive, else c's home region; year Y. Causes: the borne holding's `event` and the artifact's `made_event`, ascending, distinct. Charged civilization (Q4): c. Browser group `artifacts` (10); `event_symbol` plain. No companion.

| key | sal. | template | summary? |
| --- | --- | --- | --- |
| `bearer_corrupted` | 50 | `{artifact}, {artifact:kind} of {god:titled}, turned the heart of {figure} of {civilization}{role?|||, who would put cities to the sword|, who forsook the realm for the wilds}.` | yes |

Samples (illustrative names): `Kazrak, a sword of Murn the Hollow Keeper, turned the heart of Gorbag of the Gub Horde, who would put cities to the sword.` · `The Helm of Drork, a helm of Ghor the Red Banner, turned the heart of Maela of the Kingdom of Cear, who forsook the realm for the wilds.` (The figure's display name is as at E, so it reads `Gorbag the Butcher` when the epithet was earned.)

## 9. Schema 15 (hard rule 7)

**DDL** (appended after `faction`; 40 tables):

```
CREATE TABLE artifact_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, title TEXT NOT NULL, noun TEXT NOT NULL, class INTEGER NOT NULL, battle_permille INTEGER NOT NULL, hunt_permille INTEGER NOT NULL, claim_permille INTEGER NOT NULL) STRICT
CREATE TABLE artifact (id INTEGER PRIMARY KEY, name TEXT NOT NULL, kind INTEGER NOT NULL, civ INTEGER NOT NULL, maker_kind INTEGER NOT NULL, maker INTEGER NOT NULL, god INTEGER NOT NULL, maker_civ INTEGER NOT NULL, maker_site INTEGER NOT NULL, made_year INTEGER NOT NULL, made_event INTEGER NOT NULL, occasion_event INTEGER NOT NULL, deep INTEGER NOT NULL, status INTEGER NOT NULL) STRICT
CREATE TABLE artifact_holding (artifact INTEGER NOT NULL, seq INTEGER NOT NULL, from_year INTEGER NOT NULL, to_year INTEGER, state INTEGER NOT NULL, holder_type INTEGER NOT NULL, holder INTEGER NOT NULL, region INTEGER NOT NULL, settlement INTEGER NOT NULL, site INTEGER NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, event INTEGER NOT NULL, until_event INTEGER NOT NULL, PRIMARY KEY (artifact, seq)) STRICT, WITHOUT ROWID
```

- `artifact_kind`: one row per `artifact` definition, ascending id (canonical id order), written by the history stage whether used or not (as `monster_kind`); `key` the definition id; `class` 1 arms, 2 armour, 3 regalia, 4 relic, 5 staff.
- `artifact`: ascending id (creation order). `civ` its **home**, the realm it was made for; `maker_kind` 1 smith of its home or of no forge, 2 smith of another realm's forge, 3 caster, 4 god; `maker` the smith or caster (0 for 4); `god` (4 only, else 0); `maker_civ` the smith's or caster's realm (0 for 4); `maker_site` the mine, tower or temple (0 none); `made_year`, `made_event` its `artifact_made`; `occasion_event` that event's first cause; `deep` 0/1; `status` at E = its open holding's `state` (1 to 6).
- `artifact_holding`: (`artifact`, `seq`) ascending; `seq` 1, 2, …; `to_year` NULL and `until_event` 0 for the open holding; codes of 6.2.
- Code lists: `figure.role` gains **8 smith**. Entity type **10 `artifact`** comes into use (`history::entity_type_key()` gains `artifact`).
- `SCHEMA_VERSION`, `PRAGMA user_version`, `meta.schema_version` **15**; readers refuse 14 (`world schema version 14, this build reads 15`). The empty world gains the root pages of the three tables and `sqlite_autoindex_artifact_kind_1` (and possibly a `sqlite_schema` page): expected 52 or 53 pages, measured by the engineer with the vendored SQLite.
- `event_kind` gains 111 to 116.

**Reader checks** (both readers, one function as P5-02 11.4 and P5-08 9; messages `corrupt table '<t>': row <k>: <reason>`):

- `artifact_kind`: ids dense; `class` 1 to 5; `battle_permille` 0 to 500 and 0 unless class 1 or 2; `hunt_permille` 0 to 1000 and 0 for class 3 or 4; `claim_permille` 0 to 1000.
- `artifact`: ids dense; `kind` a row of `artifact_kind`; `civ` a civilization; `made_event` an `artifact_made` naming the artifact, of year `made_year`, whose participants and values agree with `civ`, `maker_kind`, `maker`, `god`, `maker_civ` (the `maker_realm` when 2; the maker's `civ` when 1 or 3), `maker_site`, `deep`; `occasion_event` its first cause; a `maker` of kind 1 or 2 is a role 8 figure whose `start_event` is `made_event`, of kind 3 a role 5 figure; `deep` 1 only for `maker_kind` 1 or 2; `status` = the last holding's `state`.
- `artifact_holding`: every artifact has seq 1 … n dense; seq 1 has `from_year` = `made_year` and `event` = `made_event`; each holding k < n has `to_year` = `from_year` of k + 1 and `until_event` = `event` of k + 1; holding n has `to_year` NULL and `until_event` 0; `from_year` ≤ `to_year`; `event` of year `from_year`; `state` 1 to 6 with `holder_type` 3, 6, 8, (4, 5 or 9), 6, 9 respectively; the holder exists; place columns all 0 for states 1 to 3; for 4 to 6 `region` a region, (x, y) in the map, `site` = `holder` for type 9 and `settlement` = `holder` for type 4 (`holding <k> of artifact <a>: <reason>`).
- `figure`: `role` 1 to 8; a role 8's `start_event` is an `artifact_made` naming it as `maker`.
- Events: values of 111 to 116 in 8.1's ranges; the participant rules of 8.1 (`maker_kind` against roles 3 to 5); every participant of type 10 names an `artifact` row made at or before the event.

(r2) **Event kind 117.** `event_kind` also gains 117 (8.4): a catalogue row, no DDL change, so schema 15 stands and no further bump is needed. No column is added (evil is derived from `artifact.god` and `god.camp`; the corrupted temper from the event). Reader checks added to the same function, in this order after the checks above, each as `corrupt table 'event': row <k>: <reason>`:

- `temper` is 1 or 3: `artifact_corrupted temper <t> is not 1 or 3`;
- the `god` participant is the artifact's `god` and that god's `camp` is 2: `artifact_corrupted: ar<a> is not of an evil god`;
- the artifact has a holding of state 1 with holder the event's `civilization`, `from_year` ≤ the event's year and `to_year` NULL or ≥ it: `artifact_corrupted: ar<a> not kept by c<c> in <year>`;
- its causes include that holding's `event` and the artifact's `made_event`: `artifact_corrupted: ar<a> does not cite its holding and making`.

(r3) **Event kind 118.** `event_kind` also gains 118 (8.5); no DDL change, schema 15 stands. Checks added after those of 117, same form:

- `role` is 3 or 4 and equals the `figure` participant's `role`: `bearer_corrupted role <r> is not the figure's`;
- the `god` participant is the artifact's `god` and its `camp` is 2: `bearer_corrupted: ar<a> is not of an evil god`;
- the artifact has a holding of state 2 with holder the figure, `from_year` ≤ the event's year and `to_year` NULL or ≥ it: `bearer_corrupted: ar<a> not borne by f<f> in <year>`;
- the figure is alive at the event (`born` ≤ year, `died` NULL or ≥ year) and no other `bearer_corrupted` names it: `bearer_corrupted: f<f> dead or corrupted twice`;
- its causes include that holding's `event` and the artifact's `made_event`: `bearer_corrupted: ar<a> does not cite its holding and making`.

## 10. History report v12 (amends P3-07; new section 35)

1. Header **`# worldgen history v12`** at both levels; `report --help` prints it. The growth rule holds.
2. **`[artifacts]`** (new block after `[cults]`): `artifacts: <n> (kept <k>, borne <b>, hoarded <h>, lost <l>, buried <u>, enshrined <s>); made by smiths <m> (for other realms <x>), casters <c>, gods <g>; of deep iron <d>; changed hands <t>` (t: artifacts with an event of kinds 112 to 115), then per artifact `  ar<id> <name>, <kind form>: made <year> e<id> by <maker>[ at si<id> <name>] for c<id> <name>; <present> since <year> e<id>`, maker `f<id> <name> of c<id> <name>` (kinds 1 to 3) or `g<id> <name>` (4); present `kept by c<id> <name>`, `borne by f<id> <name> of c<id> <name>`, `in the hoard of m<id> <name>`, `lost in rg<id> <name>` (type 5), `lost in the ruin of s<id> <name>` (4), `lost in si<id> <name>` (9), `buried with f<id> <name> in rg<id> <name>`, `at si<id> <name>`; region names as at E. **Full**: every artifact in id order, each followed by `    held: <from> <state words> <ref> e<id>[; …]` for every holding (state words `kept by`, `borne by`, `hoarded by`, `lost in`, `buried with`, `at`). **Summary**: at most `SUMMARY_ARTIFACTS_MAX` (8), by (holdings descending, id ascending), no `held:` lines, then `  and <n> more`.
3. **`[civilizations]`** (both levels, under P5-08 10 item 4's rule at summary): `  artifacts: made for it ar<id> <name> e<id>[, …]; holds ar<id> <name>[, …]; taken from it ar<id> <name> e<id>[, …]` (holds: kept by it or borne by its figures at E or at its end; taken from it: its home artifacts held at E by another realm, a figure of another realm or a monster, with the current holding's event; parts only when non-empty, at most 4 entries a part then ` and <n> more`). A block line with entity segments.
4. `[figures]`: role `smith` in the role counts and lists; full: a figure line gains `; bore ar<id>[, …]` (every artifact it bore) and a smith's `; made ar<id>`.
5. `[monsters]`: a monster line gains `, hoard ar<id>[, …]` when it hoards at E.
6. `[stats]` (full; appended in this order): `artifacts_made`, `artifacts_smiths`, `artifacts_commissioned`, `artifacts_casters`, `artifacts_gods`, `artifacts_deep`, `artifacts_arms`, `artifacts_armour`, `artifacts_regalia`, `artifacts_relic`, `artifacts_staff`, `artifacts_taken`, `artifacts_lost`, `artifacts_hoarded`, `artifacts_recovered`, `artifacts_buried`, `artifact_holdings`, `artifacts_changed_hands`, `artifacts_lost_at_end` (status 3, 4 or 5, or 6 at a sanctum or an ended site), `artifacts_held_by_others` (home alive, held by another realm or its figure), `smiths`, `wars_citing_artifacts`, `battles_with_artifacts`, `battles_under_acts` (12.1). A war cites an artifact (`wars_citing_artifacts`) when one of its causes is the opening event of a kept or borne holding of an artifact whose home is the attacker and which the defender keeps or a figure of the defender bears (lead ruling on part D; `cult_returns_barred` is not printed, since the world file cannot give it exactly).
7. **Length**: about +10 lines at summary, +1 per realm with artifacts. Bound: T18 (15.1).
8. (r2) **Corruption** (no new block or line; parts appended to item 2 to 4 and 6's lines):
   - `[artifacts]` count line: appended `; evil <v>, rulers corrupted <r>` (v: evil artifacts; r: events 117). An evil artifact's line: appended `; evil` and, when it has events 117, `, corrupted <n>`. Full: after its `held:` line, `    corrupted: <year> c<id> <name> e<id>[; …]` (its events 117, ascending).
   - `[civilizations]` `artifacts:` line: a part `; corrupted by ar<id> <name> e<id>[, …]` (events 117 of the realm, ascending; at most 4, then ` and <n> more`); the line is printed when any part is non-empty.
   - `[figures]`: a ruler's deeds gain `corrupted to <temper word> by ar<id> <name>` for each event 117 in its reign (the ruler is the figure of the `ruled` row of c open at the event); the `reigned … (reign <n>, <temper word>)` part keeps the drawn temper.
   - `[stats]` (full; appended after `battles_under_acts`): `artifacts_evil`, `rulers_corrupted` (events 117), `artifacts_corrupting` (artifacts with an event 117), `wars_by_corrupted` (`war_declared` citing an event 117), `corrupted_warlike` (events 117 with `temper` 1).
   - Length: at most +1 line a corruption at summary (the chronicle line); T18.
9. (r3) **Corrupted bearers** (parts appended to item 8's lines):
   - `[artifacts]` count line: `, bearers corrupted <b>` after `rulers corrupted <r>`; an evil artifact's `, corrupted <n>` counts events 117 and 118; the full `corrupted:` line lists events 118 too, as `<year> f<id> <name> of c<id> <name> e<id>`, all in event id order.
   - `[figures]`: a general's or hero's deeds gain `corrupted by ar<id> <name>`; an outlawed hero's fate is the existing `in the wilds` (status 2 without host).
   - `[civilizations]` `artifacts:` line: the part `corrupted by` also lists the realm's events 118 (one list, event id order, the cap of 4 as r2).
   - `[stats]` (full; appended after r2's five): `bearers_corrupted` (events 118), `generals_corrupted` (`role` 3), `heroes_outlawed` (`role` 4), `sacks_by_corrupted` (`sacked` citing an event 118), `evil_taken_up_by_heroes` (M15 holdings).

## 11. Timeline, browser, markers (library) and the viewer

### 11.1 Library

- **Timeline** (amends P4-01; r10): `struct ArtifactPlace { std::int64_t state = 0, holder_type = 0, holder = 0, region = 0, settlement = 0, site = 0, x = 0, y = 0, since = 0, event = 0; };` `ArtifactPlace artifact_at(std::int64_t artifact, std::int64_t year) const` (the last holding with `from_year` ≤ year; state 0 before its making; for state 3 the place is the monster's lair at that year (`monster_at`), for states 1 and 2 all place fields 0: a kept or borne artifact is shown through its holder, not on the map); `std::vector<std::int64_t> artifacts_held(std::int64_t holder_type, std::int64_t holder, std::int64_t year) const` (ascending); `std::vector<std::int64_t> artifacts_placed(std::int64_t year) const` (the artifacts in state 3 to 6 at that year, ascending).
- **Browser** (amends P4-05 4.4): eleventh kind group **`artifacts` = 10** for kinds 111 to 116 ((r2) and 117) ((r3) and 118) (`ALL_GROUPS` 0x7FF). Entities of type 10 are selectable; `block` returns its `[artifacts]` line and its `held:` line; the realm `artifacts:` line links them. `history_entity_tag` gains `ar` for type 10.
- **Markers** (amends P4-04 3.6): glyph **24 `artifact`**, drawn at the place of each artifact of `artifacts_placed(year)`, one glyph cell (7 cells) above it when the place has a settlement (`artifact_glyph_offset`, lead ruling on part D); `MarkerOptions::artifacts`; `event_symbol` for 111 to 116 plain. `std::string_view artifact_state_word(std::int64_t state)`: `kept`, `borne`, `hoarded`, `lost`, `buried`, `enshrined`.

### 11.2 Viewer work (viewer engineer)

1. Markers group box `Artifacts`; glyph 24, drawn with a dark halo (P5-08 critic F10).
2. Hover: `<name>, <kind form> (<state word>[ by <holder name>]), since <year>`; a realm's territory hover gains `, artifacts: <n>` when `artifacts_held` of the realm is not empty at the shown year.
3. Browser group `Artifacts`; artifact blocks with links.
4. Headless checks, the next free ids (expected **M16**, **B16**): on an `artifacts-small` world at 512, an artifact marker present at E and absent before its holding's year; the `artifacts` group lists the world's kinds 111 to 116 ((r2) and 117 when the world has one); selecting an artifact shows its block with links.

## 12. Follow-ups carried (this stage changes world bytes)

### 12.1 P5-08 critic F1: battles cite the act in force (amends P3-06 7.3, P5-04 7.7, P5-06 7.6)

`DivineView` (P5-08 7.1) gains `virtual std::int64_t force_act(std::int64_t civ, std::int64_t year, bool valour) const = 0;`: the largest event id among the acts of kind 100 with `effect` 2 (`valour` true) or of kind 101 with `effect` 3 (false) on civ whose effect is in force at year (P5-08 6.4); 0 none; unset 0. A battle of kind `battle`, `battle_at_pass`, `allied_battle` or `battle_led` also cites `force_act(W, Y, true)` and `force_act(L, Y, false)` (W, L its winner and loser principals) when not 0, and a `battle_led` also 7.2's `battle_artifact`; the causes are listed ascending and distinct. No template changes. A reader that needs a battle's war must look for the `war_declared` cause, not the first (as P5-04a's arms sale). **Target FU4**: reported (`battles_under_acts`).

### 12.2 P5-08 critic F2: a cult does not spread straight back (amends P5-08 6.8)

A realm a is no candidate of cult k in 6.8 while there is a `cult_suppressed` of k with civilization a in the years Y − `history.pantheon.cult_return_years` < y ≤ Y. Q4 likewise at its Y. **Target FU5**: per sample, at most 2 cults with six or more suppressions and median spreads per cult ≤ 4, max ≤ 12; CU1 to CU5 kept.

### 12.3 P5-08 critic F5 (data): 4.5.

### 12.4 Left

| Item | Why |
| --- | --- |
| P5-08 critic F3 (champions who do nothing), F4 (cause phrases repeat), F7 (gods without acts) | pantheon calibration; each moves PN targets; a pantheon follow-up |
| P5-08 critic F6 (temple name stutter), F8 (acts late) | wording and timing; no rule fits this stage |
| P5-08 critic F10 (marker size) | viewer only; 11.2 item 1 adds the halo for the new glyph and the viewer engineer may apply it to shrines and temples |
| P5-07 and P5-04a open items not named here | as listed in `CLAUDE.md`; not this stage's |

## 13. Invariants (every seed; property tests and `check-world`)

1. Every artifact has a dense chain of holdings (9's checks) with exactly one open holding at E; no holding is open on an ended realm (kept), a dead figure (borne) or a dead monster (hoarded) at E, and a kept, borne or hoarded holding closes in the year of its holder's end.
2. Every `artifact_made` cites an event whose kind and civilization role match an occasion of the shipped tuning with c in that role; at most one artifact per occasion event; at most one artifact made for a realm in any `realm_rest_years` window; at most `world_max` artifacts.
3. A smith-made artifact (1, 2): its maker is a role 8 figure created by it, of the maker realm; `maker_kind` 2 only when the home realm had no open mine and the maker realm was an alliance or trade partner with one at the making; `deep` 1 exactly when the maker realm's deepest open mine had depth ≥ `deep_metal_depth`. A caster's: role 5 of the home realm with an open kind 4 site. A god's: the occasion names that god.
4. A figure bears at most one artifact of each class at every year's end; only arms, armour and staffs are ever borne; regalia and relics are never borne.
5. Each holding's opening event is the `artifact_made` (seq 1), an artifact event of kinds 112 to 116 naming the artifact, or (quiet moves) an event of a kind of 6.7's rows M1, M2, M6, M14 or, in a quiet step, of any row; the move matches its row (e.g. a hoarded holding opened by `hero_slain` follows a borne holding by that hero).
6. No artifact leaves states 4 or 5, or state 6 except by M14; no artifact is destroyed.
7. `battle_led` win chances replay with 7.2's factor; `war_declared` cites `claim_event` exactly when the claim was above 0 at its T2.
8. 12.1: every cited act was in force for its battle's side; 12.2: no `cult_spread` of k into a realm within `cult_return_years` of its `cult_suppressed` of k.
9. Every earlier invariant holds (P5-02 to P5-08), with smiths as figures.
10. (r2) Every `artifact_corrupted` e of year y, artifact a, civilization c passes 9's checks for 117, and: y ≥ h.`from_year` + `corrupt_years` for the holding h it cites; region and settlement as 8.4; with `name_rulers`, a `ruled` row of c is open at e.
11. (r2) **In force.** A corruption e of c (year y_e, artifact a, holding h, ruler f of the `ruled` row open at e) is in force at year y when y_e < y, f's `ruled` row has `to_year` NULL or ≥ y, and h has `to_year` NULL or ≥ y. At most one corruption of c is in force at any year (the pass draws only for a reign with no corruption, after lifting). Every `war_declared` of year y whose attacker has a corruption in force at y cites it, and no `war_declared` cites an event 117 not in force at its year (or one of another civilization).
12. (r2) **Replay.** For every kept holding of an evil artifact, the events 117 are exactly those of 6.11's pass replayed at each pass of the step replay, where "corrupted now" at a pass is: an earlier event 117 of c whose holding the replay still has open and whose ruler's `ruled` row is open or closed by an event after the pass's log cursor (the reference's check; the draw needs only a, c, j and t_j). Invariant 11's year form is for readers that see only the rows.
13. (r2) The epithet `the Dark` is borne only by rulers with an event 117 in their reign; corruption moves no artifact (invariants 5 and 6 unchanged).
14. (r3) Every `bearer_corrupted` passes 9's checks for 118 and: year ≥ the cited holding's `from_year` + `corrupt_bearer_years`; the figure is of role 3 or 4 (never 6); at most one per figure.
15. (r3) A hero named by a `bearer_corrupted` has status 2 from the event to its death, no `hero_slain` or `monster_slain_by_hero` after it, and is not its realm's hero afterwards (a later `hero_rose` of the realm may follow); a general so named keeps role 3, and every `sacked` citing the event is of a war in which he was the taker's general (`served` open at the sack) while he bore the artifact.
16. (r3) Every holding opened by a `hero_rose` (M15) is of an evil artifact of class arms or armour, borne by that event's figure, following a kept holding by its realm.
17. (r3) **Replay**: for every borne holding of an evil artifact by a general or hero, the events 118 are exactly those of 6.12 replayed at the passes of the step replay (no in-force rule is needed: a figure is corrupted once, so the test is the figure's earlier events 118); the epithets `the Butcher` and `the Forsworn` only on figures named by an event 118.

## 14. Determinism notes (for the auditor)

- Integer arithmetic, int64, checked multiplication. Largest products: `chance_permille` × `forge_chance_permille` ≤ 1000 × 4000; the win factor A × (1000 + 1000 + 500); hunts H_c × prowess × (1000 + 4000 + 2000); strain rate × (1000 + 4000).
- Seven keyed streams (3), keys of event, civilization, artifact and figure ids. Every draw sits behind a fixed order of tests: eligibility (pure), then the make draw, then class, then kind; in a move, the row's tests before its draw; artifacts in ascending id; events in ascending id.
- Ordered containers only (maps keyed by ids; vectors by id). The holder indexes are rebuilt deterministically after each move.
- `seat` is refreshed at fixed points (6.2), so the "at the seat" tests do not depend on when a capital moved inside a step.
- Resume (P3-04 16.3) rebuilds every member by the replay.
- No floating point, no unordered container.
- **I0** (proof obligation): with the `zero` tuning (`occasions` empty), no artifact and no smith exists, every `ArtifactView` read is 0, and the history tables are byte-identical to those of the same build with `ArtifactsSubsystem` not installed and no view set, apart from the catalogue rows of kinds 111 to 116 and the `artifact_kind` rows. (12.1, 12.2 and 4.5 are the same in both builds.)
- (r2) **Corruption.** One keyed draw `D_at(hist.artifacts.corrupt; t_j; a, c, j)` per evaluated period, at the fixed year t_j, so the outcome does not depend on which step evaluates it (a Q4 without budget defers it to the same result). One `pick(…, 1000)` gives both the chance and the temper. Products: `corrupt_permille` × camp factor ≤ 10^6; p × `corrupt_warlike_permille` ≤ 10^6; `j × corrupt_years` checked. Order: after the step's log and seat refresh; civilizations ascending, their kept evil artifacts ascending id, periods ascending. `periods(h)` is a member rebuilt by the replay on resume. The temper override enters only through `tau()` (7.7).
- (r2) **I1** (proof obligation): with `corrupt_permille` 0 every history table is byte-identical to that of the same build with the corruption pass removed, apart from the catalogue row of kind 117 and the `history.figures` content hash (the inserted epithet entry never applies, the stream is never drawn, no ruler is named by the deed).
- (r3) **Bearers.** Stream 129, `D_at(hist.artifacts.corrupt_bearer; t_j; a, f, j)` at the fixed year t_j, one `pick(…, 1000)` per evaluated period; independent of stream 128's draws by name (3). Order: after 6.11's draws in the same pass; artifacts in state 2 ascending id, periods ascending. M15 sits in the log's moves (event id order, as M1). Products: `j × corrupt_bearer_years` checked; the sack term is a sum inside the existing `min`. Members `periods(h)`, `corrupt_event(f)` and `corrupt_artifact(f)` rebuilt by the replay.
- (r3) **I2** (proof obligation): with `corrupt_bearer_permille` 0 every history table is byte-identical to that of the r2 build, apart from the catalogue row of kind 118 and the `history.figures` and `history.artifacts` content hashes: M15 is off, stream 129 is never drawn, no figure is corrupted, so `bearer_corruption` is 0 and no sack chance or cause changes.

## 15. Targets and calibration

### 15.1 Targets (1024 defaults; A = seeds 0–99, B = 100–199)

| Target | Definition | Bound |
| --- | --- | --- |
| **AR1** | artifacts a world | median 8 to 20 on each sample; max ≤ `world_max` (40); at least one in ≥ 95 worlds |
| **AR2** | makers | smith-made ≥ 45% pooled; of those, by a dwarven maker realm ≥ 30%; commissions (2) ≥ 10% of smith-made; casters ≥ 5%; gods ≥ 5% |
| **AR3** | changed hands | ≥ 30% of artifacts have an event of kinds 112 to 115 |
| **AR4** | lost at E (quest hooks) | `artifacts_lost_at_end` median 2 to 8; ≥ 1 in ≥ 80 worlds |
| **AR5** | held by others | `artifacts_held_by_others` ≥ 1 in ≥ 40 worlds (ruler quest seeds) |
| **AR6** | recoveries | ≥ 15 `artifact_recovered` pooled per sample |
| **AR7** | claims | wars citing an artifact ≤ 10% of wars |
| **AR8** | smiths | figures a world max ≤ 250 (FG target kept) |
| **FU4**, **FU5** | 12.1, 12.2 | as there |
| **C1** (guard) | wars median | 22 to 28 |
| **G2** (guard) | standing settlements; population at E | ≥ 81; ≥ 205,000 |
| **M4a**, **M4b** (guards, Paul) | as P5-08 15.1 | as there |
| **T16** (guard) | repeated battles | ≤ 2 per thousand |
| **T18** (guard) | summary lines | default median ≤ **750**; `civs4` median ≤ 1,100 |
| **PN1 to PN8, CU1 to CU6** (guards) | P5-08 15.1 | as there |
| **timing** | `generate` seed 0 1024 release | ≤ +0.5 s over 0.30.0 |
| (r2) **CR0** (reported) | the pool | worlds in which an evil artifact is ever kept by a realm; evil artifacts a world |
| (r2) **CR1** | worlds with a corrupted ruler | 20 to 60 of 100 on each sample (if CR0 is below 25 on a sample: at least 70% of the pool worlds, and the lead is told, L-13) |
| (r2) **CR2** | corruptions a world | median 0 to 2 on each sample; max ≤ 8 |
| (r2) **CR3** | wars of corrupted rulers | `wars_by_corrupted` ≥ 1 in ≥ 10 worlds of each sample (visible); ≤ 8% of wars pooled |
| (r2) **CR4** | tempers | both warlike and grasping corruptions occur on each sample |
| (r3) **CB0** (reported) | the bearers' pool | worlds in which an evil artifact is ever borne by a general or hero; M15 take-ups |
| (r3) **CB1** | worlds with a corrupted bearer | 10 to 40 of 100 on each sample (if CB0 is below 15 on a sample: at least 60% of the pool worlds, and the lead is told, L-21) |
| (r3) **CB2** | corrupted bearers a world | at most 4 in every world; both roles occur on each sample |
| (r3) **CB3** | sacks by corrupted generals | `sacks_by_corrupted` ≥ 1 in ≥ 5 worlds of each sample; ≤ 5% of sacks pooled |
| (r3) guards | as r2's guards, and | G2 (standing settlements, population), P5-06's targets on disputed successions and exiles (unchanged by rule: an outlaw writes no `exiled`), heroes (rises, and monsters slain by heroes ≥ 25%), the FG bounds (FG11 epithets; AR8's figures max), M4a and M4b |
| (r2) guards | as above | C1, G2, T16, T18 (median ≤ +5 lines over r1's 675 / 685.5), AR1 to AR8 (AR8 as restated by the lead, 320), and P5-06's FX2 and FX3, FG11, and P5-04a's betrayals ≤ 18% of ended alliances, each kept |

### 15.2 Calibration (data only, a separate agent, in this order; history replayed on cached geography, checked equal to `generate`)

1. AR1 by the occasions' `chance_permille` (all scaled together first), then `realm_rest_years`.
2. AR2 by `forge_chance_permille` and the class weights.
3. AR3, AR4, AR6 by `battle_take_permille`, `sack_take_permille`, `burial_permille`, `raid_take_permille`.
4. AR7, C1 by the kinds' `claim_permille`; T16, C1 by `battle_permille` and `force_max_permille`.
5. FU5 by `cult_return_years`.
6. After every step: C1, G2, M4a, M4b, T16, T18, PN, CU; a step that breaks one is undone and reported to the lead. At most 8 sweeps; a miss is reported, not looped on.
7. (r2) **Corruption, a brief rerun** on r1's calibrated values (steps 1 to 6 are not redone): measure CR0 first; then CR1 and CR2 by `corrupt_permille` (first) and `corrupt_years`; then `corrupt_camp_permille`'s `evil` value if evil peoples hold most of the pool; then CR4, C1 and FX2 by `corrupt_warlike_permille`. At most 4 sweeps on seeds 0–49, then A and B once with the guards of 15.1. No other value moves; a guard that breaks is reported to the lead.
8. (r3) **Bearers, after step 7** (r2's values fixed): CB0 first; then CB1 and CB2 by `corrupt_bearer_permille`, then `corrupt_bearer_years`; then CB3 and G2 by `corrupt_sack_permille`. At most 3 sweeps on seeds 0–49, then A and B once with every guard. A miss is reported, not looped on.

## 16. Tests required

C++ (new shards `worldgen-tests-artifacts` for `*artifacts_*`, `-artifacts-property` and `-slow-artifacts`; no test is added to the `windows-msvc-debug` entries that already run above 1700 s; the chronology, figures, monsters, pantheon, conflict and history-report shards for 7 and 12):

1. Data: the `artifact` kind and its checks and grammars; `artifact_forms`; `history.artifacts` bounds, absence and the occasion checks; the new `history.figures` and `history.pantheon` fields; the deeds of 4.5.
2. Making: each occasion of 4.4 on fixtures; eligibility (ended, hidden, rest, world cap, Q4 budget with no draw); the forge (own mine, partner by depth then id, none); the chance factor; class weights with relic and staff zeroed; kinds by weight; at most one per event.
3. Makers: each row of 6.4; the smith's row, life and death; the name (forms with `{title}`, the filter, the fallback); `deep` at depths 2 and 3.
4. First holdings: each row of 6.5, including a general who already bears a sword and a dead hero.
5. Moves: each of M1 to M14, with both outcomes of each draw; the seat test after a capital move in the same step; M5 with a hero bearing the class; M12's three cases; M13 for a borne artifact; M14 with and without a sanctum.
6. Quiet steps: a move with and without budget (the quiet holding's event); a making in Q4.
7. Seams: `ArtifactView` sums, caps, deep bonus, `claim_event`, `hoard_motive_permille`; unset is 0; conflict's win factor, strain factor and declaration cause; hunts; `raise_maker` refuses a wrong id; `forge_site`; `brief`, `caster_site`.
8. Rendering: each template at each `maker_kind` and `how`; the `kind` form (a/an, deep); empty `titled` and `all`.
9. Reader: every check and message of 9 on corrupt fixtures; a schema 14 world refused.
10. Report v12 on fixture **H21** (`h21-v12.toml`, from H20): every maker kind, a commission, a deep artifact, each of M1 to M14, a claim cited by a war, both levels, the realm, figure and monster lines.
11. Follow-ups: 12.1 (valour and defeat in force, ascending causes, the artifact cause); 12.2 (the bar at Y − years + 1 and its end at Y − years).
12. Timeline, browser, markers: `artifact_at` for each state and before the making, `artifacts_held`, `artifacts_placed`; group 10; glyph 24; `artifact_state_word`.
13. Invariants of 13: seeds 0–9 at 512 with `artifacts-small` and shipped; slow: 0–19 at 1024 shipped and 0–4 with `--event-intensity turbulent`.
14. I0 (14) and resume (P3-04 16.3) with an artifact made, taken and hoarded before the resume point.
15. (r2) Corruption, unit (on fixtures with a hand-placed evil artifact): evil only for camp 2 (a good god's and a smith's artifact never draw); the periods t_j at f + T and f + 2T and none before; no draw while in force, a draw again after the lifting; a new reign draws again (its own j continues from `periods(h)`); both tempers from one `pick` at the boundary u = p × w div 1000 − 1 and u = p × w div 1000; the camp factor; Q4 with and without budget (the deferred draw gives the same result); lifting on each kind of leaving (M1 general, M7 raid, M9 sack, M11 conquest, M13 recovery) and on the reign's end; `corrupt_reign` names an unnamed reign (record event 117, `office_from` the reign's start) and refuses a corrupted reign; `tau()` in all four readers (strain, allied strain, spoils, `temper`) and the drawn temper kept in `figure.temper`; the declaration cites the event in force and not after the lifting; the epithet `the Dark` and its cap of 2; the template at both tempers; the four reader checks of 117 on corrupt fixtures.
16. (r2) Property (seeds 0–9 at 512 with `artifacts-small`, shipped; slow as item 13): invariants 10 to 13; I1.
17. (r2) H21 gains one evil artifact kept and a corruption in force at a `war_declared` (the chronicle line, the `[artifacts]` mark and `corrupted:` line, the realm part, the ruler's deed, the five stats).
18. (r3) Bearers, unit: a champion bearing its god's evil gift never draws; a general who took it up by M1 and a hero by M5 and by M15 draw at f + Tb and not before; one corruption a figure (no draw after it, a second evil artifact not drawn); Q4 with and without budget; M15 takes only an evil arm or armour, only with `corrupt_bearer_permille` > 0, arms before armour, not when the hero bears the class; the general's sack term and the `sacked` cause in force, and not after he has lost the artifact or died; the hero's outlawry (status 2, place region, `hero_of` 0, a new hero may rise, its death `how` 2 and M2 on its artifact); F28's new allowance and its unchanged message; the epithets `the Butcher` and `the Forsworn` before the role's other entries and their cap of 2; the template at both roles; the five reader checks of 118 on corrupt fixtures; `corrupt_bearer` refusals (dead, role, twice).
19. (r3) Property (as item 16): invariants 14 to 17; I2.
20. (r3) H21 gains a general corrupted before a sack that cites him and a hero outlawed by an artifact taken up by M15 (chronicle lines, the `[artifacts]`, `[figures]` and realm parts, the five stats).

Reference: **`p5_11_artifacts_reference.py`** (test writer): replays 6.3 to 6.8 from the stored log and the fixtures' tables and `check-world` for 13; golden **`worldgen/tests/golden/p5-11-artifacts.txt`** (`artifacts-small` worlds at 512 seeds 0–9 and shipped seed 42 at 1024: hashes of `artifact_kind`, `artifact`, `artifact_holding`, `figure`, `event`, `event_cause`, and the stat lines of 10 item 6). (r2) The reference mirrors 6.11 from the stored rows alone: the kept holdings of evil artifacts (`artifact.god`, `god.camp`), its existing step replay for the pass years and Q4 budgets, the `ruled` rows and events 117 for the in-force test (invariant 11), and `D_at` for the draw; it does not need the reign draws of P5-06. The golden file adds the five stats of 10 item 8 and the hash of the events 117. (r3) Likewise 6.12 and M15 from the stored rows (`figure.role`, the borne holdings, the figure's earlier events 118, `D_at` on stream 129), the sack chance's extra term in `p3_conflict_reference.py` (the taker's general from the `served` rows, in force from the holdings), and the golden file adds the five stats of 10 item 9 and the hash of the events 118. Carried test-side items: `p5_06_figures_reference.py` knows role 8 (r2) and the epithet entry `the Dark` with the deed `corrupted`; `p5_08_pantheon_reference.py` the return bar and the new deeds; `p3_conflict_reference.py` the extra causes; `p4_browser_reference.py` group 10.

## 17. Golden values and CI

Every world file and every history golden is re-recorded once, after the cross-preset comparison (generator 0.31.0, schema 15). Must not move: `core_vectors.txt`, `p1-0*`, `p3-02-names.txt`, `p3-03-chronology.txt` (no chronology change). `world-hashes.sh` keeps its seventeen cases (validation reports which hold a commission, a hoard and a recovery). New CTest entries `worldgen-tests-artifacts`, `-artifacts-property` and `-slow-artifacts`.

## 18. Work split (for cost)

| Part | Kind | Agent | Sections |
| --- | --- | --- | --- |
| A | data kind `artifact`, culture field, tunings, name kind, DDL, catalogue | generator engineer 1 | 4, 6.6, 8.1, 9 (DDL) |
| B | `ArtifactsSubsystem`: making, makers, holdings, moves, quiet steps, `ArtifactView` | generator engineer 1 | 5, 6, 7.1 |
| C | seams: conflict (win factor, strain, causes), monsters (hunts), figures (`raise_maker`, `brief`, `caster_site`, role 8), alliances (`forge_site`); follow-ups 12.1, 12.2 | generator engineer 2, on disjoint files | 7.2 to 7.5, 12 |
| D | rendering forms, readers, report v12, digest, timeline, browser, markers library | generator engineer 2 after C | 7.6, 9 (checks), 10, 11.1 |
| E | calibration sweeps, data only | a separate agent after A to D | 15.2 |
| F | tests and references | test writer | 16 |
| G | goldens | validation, once | 17 |
| H | viewer | viewer engineer | 11.2 |
| (r2) I | corruption, on the built stage (about a day) | one generator engineer | 3 (stream 128), 4.3, 4.6, 6.1, 6.11, 7.7, 8.4, 9 (117), 10 item 8, 11.1 |
| (r2) J | corruption tests and reference | test writer | 13 items 10 to 13, 14, 16 items 15 to 17 |
| (r2) K | brief rerun | calibration agent, after I | 15.2 step 7 |
| (r2) L | goldens | validation, once, after K | 17 |
| (r3) M | bearers, the second engineering pass on r2 as built (about half a day) | one generator engineer | 3 (stream 129), 4.3, 4.6, 6.1, 6.12, 7.8, 8.5, 9 (118), 10 item 9, 11.1 |
| (r3) N | bearer tests and reference | test writer | 13 items 14 to 17, 14, 16 items 18 to 20 |
| (r3) O | bearer rerun | calibration agent, after M | 15.2 step 8 |

Parts L (goldens) runs once, after O, when r3 is taken with r2; if r2 merges first, L runs after K and again after O.

(r2) **Part I, files.** `core/streams.hpp` (stream 128); `src/data/kinds_history.cpp` (the four `history.artifacts` fields, the deed `corrupted`); data `history/artifacts.toml`, `history/figures.toml` (epithet entry 3), `history/report.toml` (template 117), in `game/data` and both fixture roots, and the `artifacts-small` values; `event_catalogue.cpp` (kind 117, its reader checks in the one problem function); `figures_view.hpp`, `figures.hpp`, `figures.cpp` (`ReignBrief`, `reign_brief`, the reign fields, `tau()`, `corrupt_reign`, `lift_corruption`, rule 3's kind, the deed); `artifacts.cpp` or a new `artifacts_corrupt.cpp` (the pass, `periods`); `conflict_war.cpp` (the declaration cause); `src/inspect/history_report.cpp` and the digest (10 item 8); `history_browser.cpp` (117 in group 10). No other subsystem changes. Part J mirrors 6.11 in `p5_11_artifacts_reference.py` and adds the epithet to `p5_06_figures_reference.py`; new tests go to the `-artifacts` and `-artifacts-property` entries, none to the `windows-msvc-debug` entries above 1700 s.

(r3) **Part M, files** (the brief's "Part J"; lettered M because r2's J is its tests). `core/streams.hpp` (stream 129); `src/data/kinds_history.cpp` (the three `history.artifacts` fields; the deed `corrupted` for roles 3 and 4); data `history/artifacts.toml`, `history/figures.toml` (the two epithet entries), `history/report.toml` (template 118), in `game/data` and both fixture roots, and the `artifacts-small` values; `event_catalogue.cpp` (kind 118 and its checks; F28's allowance); `artifacts.hpp` (`bearer_corruption`) and the corruption pass file of Part I (the bearers' draws, `periods`, the figure members) and `artifacts_moves.cpp` (M15); `figures.hpp`, `figures.cpp` (`corrupt_bearer`, the outlaw's status, place and `hero_of`, the deed); `conflict_war.cpp` (the sack term and cause, P3-06 7.5); `src/inspect/history_report.cpp` and the digest (10 item 9); `history_browser.cpp` (118 in group 10). Nothing of r2's code changes. Part N mirrors 6.12 in `p5_11_artifacts_reference.py`, the sack term in `p3_conflict_reference.py`, the epithets and F28 in `p5_06_figures_reference.py`; tests go to `-artifacts` and `-artifacts-property`.

## 19. Amendments

| Spec | Section | What |
| --- | --- | --- |
| P3-02 | 3 (culture field), 4.5 (name kind `artifact`) | 4.2, 6.6 |
| P3-04 | 8.3, 8.4 (entity type 10, form `kind`, empty `titled`) | 7.6 |
| P3-06 | 5.2 (strain), 7.1 (declaration causes), 7.3 (battle causes) | 7.2, 12.1 |
| P3-07 | 24.2 (companions), new 35 (v12) | 8.3, 10 |
| P5-04 | 7.7 (allied battle causes) | 12.1 |
| P5-06 | 4.4 (smith ages), 7.1 (`brief`, `caster_site`), 7.6 (win factor, causes), 7.7 (hunts), 9.1 (role 8) | 4.3, 7.2, 7.3, 9 |
| P5-07 | 7.1 (`forge_site`) | 7.4 |
| P5-08 | 4.2 (`cult_return_years`), 4.3 and 8.2 (deeds, cause words), 4.4 (faerie form), 6.8 (the bar), 7.1 (`force_act`) | 4.5, 12 |
| P4-01, P4-04, P4-05 | timeline reads, glyph 24, group 10 | 11.1 |
| (r2) P5-06 | 6.4 rule 3 (`artifact_corrupted` names a ruler by deed), 6.10 (deed `corrupted`, epithet `the Dark`), 7.1 (`reign_brief`), 7.2 and 7.3 (the effective temper) | 4.3, 7.7 |
| (r2) P5-04a | 2 (`FigureView::temper` returns the effective temper) | 7.7 |
| (r2) P3-06 | 7.1 (a declaration cites a corruption in force) | 7.7 |
| (r3) P3-06 | 7.5 item 1 (the sack chance and cause of a corrupted general) | 7.8 |
| (r3) P5-06 | 6.7 (an outlawed hero is no hero of its realm), 6.9 (an outlaw's place and death), 6.10 (deed `corrupted` for generals and heroes; `the Butcher`, `the Forsworn`), 9 F28 (status 2 for an outlawed hero) | 4.3, 7.8 |
| `schema.md` | the DDL of 9, code lists, entity type 10, changelog row 15, catalogue 111 to 116 | 9, 22 |

## 20. For P6 (P5-00 10.2)

P6 reads, per artifact: its present holding (6.9): a hoard (the lair is a dungeon with a treasure), a ruin or a field (a site to place), a grave (the bearer's tomb, `figure.death_region`, `death_settlement`), a sanctum, temple or tower (already a site), a realm's treasury or a living bearer (a ruler's quest line: design 9 "Ruler lines send the player after artifacts"; a realm whose home artifact is held by another, a monster or a ruin has a natural quest). Quest hooks: a stolen artifact never recovered (P5-00 10's example), a crown in a dragon's hoard, a sword in a hero's grave. The chain of holdings is the artifact's lore.

(r2) Two hooks from Paul's answers (2026-11-28): (1) **Unmaking.** Artifacts are "magical so very difficult to destroy" and are never destroyed in history (Q-P5-11-2); unmaking one is a candidate quest line for the game ("such and such item needs to be unmade to save a town/city"), for design 9's quest lines: an evil artifact with events 117 is the natural candidate, since its chain of holdings and the rulers it turned are its case. (2) **Corruption as a plot point.** The game can build on 6.11: a realm whose ruler is corrupted at the present (invariant 11 at E) is a court under a shadow; the artifact's events 117 name each ruler it turned; a player who carries an evil artifact off ends a corruption by the rule history used.

(r3) A corrupted hero alive at E is an outlaw in the wilds with a dark blade (a figure for a quest: hunted, redeemed, or met); a dead one lies in a grave or left the blade to its realm, where it turns the next ruler or general. A corrupted general's sacks name him in the chronicle of every ruined city he took.

## 21. Open questions

**For Paul: decided** (r2; Paul, 2026-11-28, his words; the lead moves the rows of `docs/questions-for-paul.md` to its decided section):

| Id | Question (short) | Recommendation of r1 | Paul's answer | What the stage does |
| --- | --- | --- | --- | --- |
| Q-P5-11-1 | How many artifacts in a default world? | (b) 8 to 20, cap 40 | "8-20 sounds reasonable" | AR1 as written (calibrated median 15) |
| Q-P5-11-2 | May artifacts be destroyed in history? | (a) never | "I'd say they are magical so very difficult to destroy but maybe that could be a quest line for later (like such and such item needs to be unmade to save a town/city)" | never destroyed in history, as built; unmaking is a quest-line candidate for the game (20, proposals) |
| Q-P5-11-3 | Do evil artifacts corrupt their holders? | (a) no | "Actually, I really like that as a plot point, yes an evil relic corrupting a bearer would be great." | **reversed**: the new rule 6.11 (an evil god's artifact kept by a realm corrupts its ruler's temper; event 117) |
| Q-P5-11-4 | Named smiths as makers? | (a) yes | "Agree, yes" | smiths are figures (role 8), as built |
| Q-P5-11-5 | The five classes and fifteen kinds? | take | "Agree" | ships the fifteen |
| Q-P5-11-6 | Lost, buried and enshrined artifacts never found again in history? | (a) yes | "Yes" | as built; corruption moves nothing either |
| Q-P5-11-7 | Regalia or relic held by another as a grievance, recovered by a won war? | (a) yes | "yes" | as built (7.2, M13, AR7) |

(r2) No new question for Paul: his words "corrupting a bearer" are read as the holder's corruption through the realm that keeps the artifact (L-12); the lead may put the bearer reading back to him if it does not satisfy.

(r3) Paul did, on the same day: "Actually, I like the idea that it could also corrupt a hero or a general that bears the artifact". L-12 is superseded for generals and heroes by 6.12; the ruler rule of r2 stands beside it. No new question for Paul.

**For the lead:**

| Id | Question | Recommendation (interim) |
| --- | --- | --- |
| L-1 | Schema 15 as in 9 (three tables, role 8, entity type 10) | Yes, one bump; proposals text in 22 |
| L-2 | An echo table `artifact_kind` beside `artifact` and `artifact_holding` (P5-00 6.1 named two tables) | Yes, as `monster_kind`: the report, the viewer and P6 need the noun and class without the data files |
| L-3 | Holdings of a moving holder (kept, borne, hoarded) carry no place; the place is derived for any year (6.2, 11.1) | Yes: otherwise every capital move and lair move would add rows |
| L-4 | Quiet moves (6.7, 6.8): routine moves write no artifact event and cite the triggering event in the holding | Yes: it keeps the log to story beats, as P5-04a's quiet renewals |
| L-5 | Artifacts last in `subsystems`, steps T3, T4, T6, Q4 | Yes: it reads every subsystem's events of the step and is read only through `ArtifactView` at the next call |
| L-6 | The general's artifacts act in the win chance only, beside skill (7.2), not on force or casualties | Yes, as P5-06's "a general wins battles, not men" |
| L-7 | A declaration cites the claim's event (7.2), a conflict change | Yes: the motive is then in the causal graph; no template changes |
| L-8 | The seat snapshot (6.2) for the "at the seat" moves | Yes: a capital sacked and moved in one step is still recognised, deterministically |
| L-9 | Follow-ups 12.1 and 12.2 taken; F3, F4, F6 to F10 left (12.4) | Yes: the two are small, have no wording effect and answer the critic's two strongest notes |
| L-10 | Kind ids 111 to 116 | Final if no other stage merges first |
| L-11 | `raise_maker` beside P5-08's `raise_divine` (both create figures for another subsystem) | Yes; the engineer may share their body |
| (r2) L-12 | Who is corrupted: the reigning ruler of the realm that keeps the evil artifact, not a figure who bears it (Paul said "a bearer") | Yes: regalia and relics are never borne, the borne ones are arms given by an evil god to its own champion (devotion, not corruption), and the ruler's temper is an existing path that already acts on war, allies, spoils and cults. A bearer rule would need a new effect on generals or heroes and its own calibration |
| (r2) L-13 | CR1's floor if the pool is small (god-given artifacts are about 7% of makings, a third of gods evil) | If CR0 is below 25 worlds on a sample, CR1 becomes "at least 70% of the pool worlds"; widening "evil" (for example to artifacts hoarded by an evil monster) would be a new rule, not calibration |
| (r2) L-14 | One period test each `corrupt_years` of a kept holding, keyed `D_at(t_j; a, c, j)`, rather than one draw per reign | Yes: the reference then needs no reign starts of unnamed reigns (stored only for named rulers), the draw is stateless across steps, and every ruler under the artifact still faces new draws |
| (r2) L-15 | The corrupted temper is the effective one only; `figure.temper` keeps the drawn temper and no column is added | Yes: schema 15 is unchanged; the event's `temper` value records the corrupted one |
| (r2) L-16 | Only `war_declared` cites a corruption (not betrayals, refusals or spoils, which read the temper too) | Yes: one cause path, as the artifact claim; the temper's other effects show in the realm's record |
| (r2) L-17 | Epithet `the Dark`, ordinary only (at most two a world), as entry 3 of the ruler list | Yes: a corrupted reign is a story; entry 3 keeps `the Conqueror` and `the Faithless` first |
| (r3) L-18 | The champion is immune; the corruption takes the later bearers (generals by M1, M3; heroes by M5, M15) | Yes: the god armed its devotee, and the stronger story is the gift passing to lesser hands after the champion's death. Exposing champions too would be a one-line change (role 6 in the set) if the lead prefers |
| (r3) L-19 | M15: a newly risen hero takes up a kept **evil** arm or armour (only evil ones, only with `corrupt_bearer_permille` > 0) | Yes: without it a hero bears an evil gift only through M5 and the hero rule would be nearly dead; limiting it to evil artifacts leaves every other holding of r1 and r2 unchanged (I2). The alternative, M15 for every kept arm, changes AR targets and hunts and would need AR recalibration |
| (r3) L-20 | No camp factor for bearers; a period of its own (`corrupt_bearer_years` 10) and a chance of its own (`corrupt_bearer_permille` 300) | Yes: generals' careers are short (goblins 12 to 36 years), so r2's 25-year period would rarely fire; a bearer holds the thing in hand, so its people's camp does not shield it |
| (r3) L-21 | CB1's floor if the pool is small | as L-13: at least 60% of the pool worlds when CB0 is below 15 on a sample |
| (r3) L-22 | One consequence per role: a general's sacks (`corrupt_sack_permille` in P3-06 7.5, the `sacked` cites him); a hero's outlawry (status 2, no `exiled` event, F28 relaxed) | Yes: both are existing paths with one term or one status; a general's claim to the throne would need a new path in P5-06 6.5, and a hero's kept hoard would hardly show (M5 is rare) |
| (r3) L-23 | Kind 118 of its own, not a role on 117 | Yes: r2's 117 is being built; one template and one check list per kind |

## 22. Text for the lead (`schema.md` and `proposals.md`)

**`docs/specs/schema.md`**: section 1 at version 15 with the three statements of 9 appended after `faction`; a paragraph for the tables of version 15 (9's bullets); `figure.role` 8 smith; entity type 10 `artifact` in the codes paragraph; the catalogue rows 111 to 116; changelog row: `15 | 0.31.0 | P5-11 | tables artifact_kind, artifact, artifact_holding; figure.role 8 smith; entity type 10 artifact in use; event kinds 111 to 116; readers refuse 14`.

**`docs/design/proposals.md`**: entries "Schema 15: artifacts (P5-11)" and "History: P5-11 artifacts (readings)" (appended by the spec writer, r1); (r2) "Artifacts: Paul's answers to Q-P5-11-1 to 7 (2026-11-28)".

(r2) **`schema.md`**: the catalogue rows and the changelog row 15 read "event kinds 111 to 117" (117 `artifact_corrupted`, a catalogue row; no DDL change). (r3) "111 to 118" (118 `bearer_corrupted`); `figure.status` 2 also for a hero outlawed by a `bearer_corrupted`. (r3) `proposals.md`: "Artifacts: corruption also takes a bearer (Paul, 2026-11-28)".

## Revision notes

- r1 (2026-11-28): first draft, for approval.
- r2 (2026-11-28): Paul's answers to Q-P5-11-1 to 7 (21, decided); the new rule of corruption by an evil artifact (Q-P5-11-3 reversed). Sections changed, each passage marked (r2): status line; 1 (the paragraph on corruption); 2 (item 8); 3 (stream 128); 4.3 (four `history.artifacts` fields; the deed `corrupted` and the epithet `the Dark` of `history.figures`); 4.6 (fixture values); 5 (`periods`); 6.1 (the pass); 6.10 (item 6); **6.11 (new)**; **7.7 (new)**; 8.3 (no companion); **8.4 (new: event 117)**; 9 (kind 117 and its reader checks; schema 15 unchanged; no existing message text changed); 10 (item 8); 11.1 and 11.2 (117 in group 10); 13 (invariants 10 to 13); 14 (corruption notes, I1); 15.1 (CR0 to CR4, guards); 15.2 (step 7); 16 (items 15 to 17, the reference); 18 (parts I to L, files); 19 (three rows); 20 (unmaking and corruption hooks); 21 (Paul's answers; L-12 to L-17). Event kinds 111 to 117; 128 streams. Nothing built in r1 changes when `corrupt_permille` is 0 (I1).
- r3 (2026-11-28): Paul extends corruption to bearers ("Actually, I like the idea that it could also corrupt a hero or a general that bears the artifact"), superseding L-12 for generals and heroes; a pure addition on r2 (its ruler rule, stream 128, kind 117 and tuning names unchanged). Sections changed, each passage marked (r3): status line; 2 (item 9); 3 (stream 129); 4.3 (three `history.artifacts` fields; the deed `corrupted` for generals and heroes, epithets `the Butcher` and `the Forsworn`); 4.6; 5 (bearer members; a wording correction of r2's `checked`); 6.1; **6.12 (new; M15)**; **7.8 (new)**; **8.5 (new: event 118)**; 9 (kind 118 checks); 10 (item 9); 11.1; 13 (invariants 14 to 17); 14 (bearer notes, I2); 15.1 (CB0 to CB3, guards); 15.2 (step 8); 16 (items 18 to 20, reference); 18 (parts M to O, files); 19 (two rows); 20; 21 (L-18 to L-23); 22. Event kinds 111 to 118; 129 streams. With `corrupt_bearer_permille` 0 the r2 build's worlds are unchanged (I2).
