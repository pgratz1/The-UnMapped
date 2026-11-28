# P5-11 Artifacts

Status: **r1 (2026-11-28)**, for approval. Phase P5 History depth, stage 11 (`docs/specs/p5-00-history-depth-overview.md` 3.1, the P5-11 paragraph and its amendment: "its makers are P5-06's figures (casters, or smiths as a new role) and P5-04's forges"; 6.1's row `artifact`, `artifact_holding`, entity type 10). **Base: P5-08 as built** (spec r1a, `docs/reviews/p5-08-*`): generator 0.30.0, schema 14, history report format v11, event kinds 1 to 110, 120 streams, browser groups 0 to 9, 37 tables. This stage: generator **0.31.0**; schema **15**; history report format **v12**; event kinds **111 to 116**; **127** streams; browser group 10; 40 tables.

Design: section 5 ("Magical artifacts, made in response to conflicts. Each has a maker, an occasion and a chain of holders and places, and where it lies in the present is a site and a quest hook"; "Dwarves are great forgers. They make wealth and weapons and trade them to the other kingdoms, which ties the mountain peoples into trade and into the making of artifacts"), section 2 (deferred: "artifacts with lineages" beyond this), section 9 ("Ruler lines send the player after artifacts once the artifacts stage of phase P5 exists"), section 13 (the P5 row). Proposals entry "History: magical artifacts are made in response to conflicts" (2026-11-19). Builds on P3-06 (wars, battles, sacks, conquests), P5-02 (monsters, hoards, lairs, sites), P5-04 (mines, forges, partners), P5-06 (figures: generals, heroes, casters, towers; deaths and burials), P5-07 (forge depth, the deep), P5-08 (gods, champions, cults, temples, sanctums).

## 1. Purpose

History makes a few dozen **artifacts** at most per world, each in answer to an event of conflict or of a god (an **occasion**: a war declared, a city sacked, a realm conquered, a throne disputed, a hunt begun against a monster, a town overrun, a horror woken in the deep, a champion raised, a cult founded). Each has a **kind** from data (a sword, a crown, a reliquary, a staff, …), a **name** drawn in its maker's culture, and a **maker**: a named **smith** (a new figure role) at a forge, best of all a dwarven forge-hold with a deep mine, which may forge for an ally or trade partner; a caster at its tower; or a god, for its champion or its cult's temple. From its making an artifact moves through a **chain of holdings**, each with years and the event that moved it: kept in a realm's treasury, borne by a general, hero or champion, hoarded by a monster, lost on a battlefield or in a ruin, buried with its bearer, laid up at a temple, tower or sanctum, taken as spoils, recovered by a hero or a won war. Artifacts are never destroyed in history, so each lies somewhere at the present day: in a realm's hands, in a hoard, a ruin, a grave or a sanctum, which P6 turns into sites and quest hooks.

Artifacts act on the world in small, integer ways: a general's borne arms add to its side's odds in battle, a hero's to its hunt; a realm whose regalia or relic is held by another feels strain toward the holder and its declarations cite the loss; a realm hunts harder a monster that hoards its treasure.

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

Checks: `artifacts_occasion_*` (4.4). **Code defaults** (a tuning built in code): `occasions` empty, the others their starts (inert). The **`zero`** fixture tuning: shipped values with `occasions` empty (I0, 14).

`history.figures` gains, appended after its last field, `smith_age_permille` int(0, 1000) start **250** and `smith_age_span_permille` int(0, 1000) start **300** (a smith's age at entry, per mille of lifespan, P5-06 6.9); code defaults the starts.

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

In memory, as P5-08's `pantheon-small`, plus: every shipped occasion with `chance_permille` 1000, `realm_rest_years` 0, `world_max` 500, `burial_permille` 500, `battle_take_permille` 500, `raid_take_permille` 500, so that every path of 6 occurs in a 512 world.

## 5. State

`ArtifactsSubsystem` (new: `worldgen/src/history/artifacts.cpp`, `artifacts_moves.cpp`; header `worldgen/history/artifacts.hpp`; tuning id `history.artifacts`) keeps, in its own members, ids only:

- per artifact (ids 1, 2, … in creation order, H2; at most 100,000, fatal `history: bound: artifacts exceed 100000`): every column of `artifact` (9), and its holdings in order (every column of `artifact_holding`), the last one open;
- per civilization (index civ id − 1): `last_made` (year, −∞ none), `seat` (6.2);
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

Causes as 8.1. **Companions** (amends P3-07 24.2's table, with P5-00 7.1): `fell_in_battle`, `sacked`, `settlement_captured`, `civ_conquered`, `civ_ended`: every `artifact_taken` or `artifact_lost` with e among its causes; `hero_slain`, `hold_lost`: every `artifact_hoarded` with e among its causes; `monster_slain`, `monster_slain_by_hero`, `peace`, `war_won`: every `artifact_recovered` with e among its causes. Companions are not followed further. Saliences: a making reads like a cult's founding (45 to 50); a hoard taken and a recovery like a hero slain (50); takings and losses read with their cause; a burial is bookkeeping. Estimated summary candidates a default world: 8 to 15 makings, 1 to 3 hoards and recoveries, inside P5-02 12.9's budget (T18).

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

## 10. History report v12 (amends P3-07; new section 35)

1. Header **`# worldgen history v12`** at both levels; `report --help` prints it. The growth rule holds.
2. **`[artifacts]`** (new block after `[cults]`): `artifacts: <n> (kept <k>, borne <b>, hoarded <h>, lost <l>, buried <u>, enshrined <s>); made by smiths <m> (for other realms <x>), casters <c>, gods <g>; of deep iron <d>; changed hands <t>` (t: artifacts with an event of kinds 112 to 115), then per artifact `  ar<id> <name>, <kind form>: made <year> e<id> by <maker>[ at si<id> <name>] for c<id> <name>; <present> since <year> e<id>`, maker `f<id> <name> of c<id> <name>` (kinds 1 to 3) or `g<id> <name>` (4); present `kept by c<id> <name>`, `borne by f<id> <name> of c<id> <name>`, `in the hoard of m<id> <name>`, `lost in r<id> <name>` (type 5), `lost in the ruin of s<id> <name>` (4), `lost in si<id> <name>` (9), `buried with f<id> <name> in r<id> <name>`, `at si<id> <name>`; region names as at E. **Full**: every artifact in id order, each followed by `    held: <from> <state words> <ref> e<id>[; …]` for every holding (state words `kept by`, `borne by`, `hoarded by`, `lost in`, `buried with`, `at`). **Summary**: at most `SUMMARY_ARTIFACTS_MAX` (8), by (holdings descending, id ascending), no `held:` lines, then `  and <n> more`.
3. **`[civilizations]`** (both levels, under P5-08 10 item 4's rule at summary): `  artifacts: made for it ar<id> <name> e<id>[, …]; holds ar<id> <name>[, …]; taken from it ar<id> <name> e<id>[, …]` (holds: kept by it or borne by its figures at E or at its end; taken from it: its home artifacts held at E by another realm, a figure of another realm or a monster, with the current holding's event; parts only when non-empty, at most 4 entries a part then ` and <n> more`). A block line with entity segments.
4. `[figures]`: role `smith` in the role counts and lists; full: a figure line gains `; bore ar<id>[, …]` (every artifact it bore) and a smith's `; made ar<id>`.
5. `[monsters]`: a monster line gains `, hoard ar<id>[, …]` when it hoards at E.
6. `[stats]` (full; appended in this order): `artifacts_made`, `artifacts_smiths`, `artifacts_commissioned`, `artifacts_casters`, `artifacts_gods`, `artifacts_deep`, `artifacts_arms`, `artifacts_armour`, `artifacts_regalia`, `artifacts_relic`, `artifacts_staff`, `artifacts_taken`, `artifacts_lost`, `artifacts_hoarded`, `artifacts_recovered`, `artifacts_buried`, `artifact_holdings`, `artifacts_changed_hands`, `artifacts_lost_at_end` (status 3, 4 or 5, or 6 at a sanctum or an ended site), `artifacts_held_by_others` (home alive, held by another realm or its figure), `smiths`, `wars_citing_artifacts`, `battles_with_artifacts`, `battles_under_acts` (12.1), `cult_returns_barred` (12.2).
7. **Length**: about +10 lines at summary, +1 per realm with artifacts. Bound: T18 (15.1).

## 11. Timeline, browser, markers (library) and the viewer

### 11.1 Library

- **Timeline** (amends P4-01; r10): `struct ArtifactPlace { std::int64_t state = 0, holder_type = 0, holder = 0, region = 0, settlement = 0, site = 0, x = 0, y = 0, since = 0, event = 0; };` `ArtifactPlace artifact_at(std::int64_t artifact, std::int64_t year) const` (the last holding with `from_year` ≤ year; state 0 before its making; for state 3 the place is the monster's lair at that year (`monster_at`), for states 1 and 2 all place fields 0: a kept or borne artifact is shown through its holder, not on the map); `std::vector<std::int64_t> artifacts_held(std::int64_t holder_type, std::int64_t holder, std::int64_t year) const` (ascending); `std::vector<std::int64_t> artifacts_placed(std::int64_t year) const` (the artifacts in state 3 to 6 at that year, ascending).
- **Browser** (amends P4-05 4.4): eleventh kind group **`artifacts` = 10** for kinds 111 to 116 (`ALL_GROUPS` 0x7FF). Entities of type 10 are selectable; `block` returns its `[artifacts]` line and its `held:` line; the realm `artifacts:` line links them. `history_entity_tag` gains `ar` for type 10.
- **Markers** (amends P4-04 3.6): glyph **24 `artifact`**, drawn at the place of each artifact of `artifacts_placed(year)`; `MarkerOptions::artifacts`; `event_symbol` for 111 to 116 plain. `std::string_view artifact_state_word(std::int64_t state)`: `kept`, `borne`, `hoarded`, `lost`, `buried`, `enshrined`.

### 11.2 Viewer work (viewer engineer)

1. Markers group box `Artifacts`; glyph 24, drawn with a dark halo (P5-08 critic F10).
2. Hover: `<name>, <kind form> (<state word>[ by <holder name>]), since <year>`; a realm's territory hover gains `, artifacts: <n>` when `artifacts_held` of the realm is not empty at the shown year.
3. Browser group `Artifacts`; artifact blocks with links.
4. Headless checks, the next free ids (expected **M16**, **B16**): on an `artifacts-small` world at 512, an artifact marker present at E and absent before its holding's year; the `artifacts` group lists the world's kinds 111 to 116; selecting an artifact shows its block with links.

## 12. Follow-ups carried (this stage changes world bytes)

### 12.1 P5-08 critic F1: battles cite the act in force (amends P3-06 7.3, P5-04 7.7, P5-06 7.6)

`DivineView` (P5-08 7.1) gains `virtual std::int64_t force_act(std::int64_t civ, std::int64_t year, bool valour) const = 0;`: the largest event id among the acts of kind 100 with `effect` 2 (`valour` true) or of kind 101 with `effect` 3 (false) on civ whose effect is in force at year (P5-08 6.4); 0 none; unset 0. A battle of kind `battle`, `battle_at_pass`, `allied_battle` or `battle_led` also cites `force_act(W, Y, true)` and `force_act(L, Y, false)` (W, L its winner and loser principals) when not 0, and a `battle_led` also 7.2's `battle_artifact`; the causes are listed ascending and distinct. No template changes. A reader that needs a battle's war must look for the `war_declared` cause, not the first (as P5-04a's arms sale). **Target FU4**: reported (`battles_under_acts`).

### 12.2 P5-08 critic F2: a cult does not spread straight back (amends P5-08 6.8)

A realm a is no candidate of cult k in 6.8 while there is a `cult_suppressed` of k with civilization a in the years Y − `history.pantheon.cult_return_years` < y ≤ Y. Q4 likewise at its Y. Each barred (cult, realm, step) that would otherwise have been a candidate counts in `cult_returns_barred`. **Target FU5**: per sample, at most 2 cults with six or more suppressions and median spreads per cult ≤ 4, max ≤ 12; CU1 to CU5 kept.

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

## 14. Determinism notes (for the auditor)

- Integer arithmetic, int64, checked multiplication. Largest products: `chance_permille` × `forge_chance_permille` ≤ 1000 × 4000; the win factor A × (1000 + 1000 + 500); hunts H_c × prowess × (1000 + 4000 + 2000); strain rate × (1000 + 4000).
- Seven keyed streams (3), keys of event, civilization, artifact and figure ids. Every draw sits behind a fixed order of tests: eligibility (pure), then the make draw, then class, then kind; in a move, the row's tests before its draw; artifacts in ascending id; events in ascending id.
- Ordered containers only (maps keyed by ids; vectors by id). The holder indexes are rebuilt deterministically after each move.
- `seat` is refreshed at fixed points (6.2), so the "at the seat" tests do not depend on when a capital moved inside a step.
- Resume (P3-04 16.3) rebuilds every member by the replay.
- No floating point, no unordered container.
- **I0** (proof obligation): with the `zero` tuning (`occasions` empty), no artifact and no smith exists, every `ArtifactView` read is 0, and the history tables are byte-identical to those of the same build with `ArtifactsSubsystem` not installed and no view set, apart from the catalogue rows of kinds 111 to 116 and the `artifact_kind` rows. (12.1, 12.2 and 4.5 are the same in both builds.)

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

### 15.2 Calibration (data only, a separate agent, in this order; history replayed on cached geography, checked equal to `generate`)

1. AR1 by the occasions' `chance_permille` (all scaled together first), then `realm_rest_years`.
2. AR2 by `forge_chance_permille` and the class weights.
3. AR3, AR4, AR6 by `battle_take_permille`, `sack_take_permille`, `burial_permille`, `raid_take_permille`.
4. AR7, C1 by the kinds' `claim_permille`; T16, C1 by `battle_permille` and `force_max_permille`.
5. FU5 by `cult_return_years`.
6. After every step: C1, G2, M4a, M4b, T16, T18, PN, CU; a step that breaks one is undone and reported to the lead. At most 8 sweeps; a miss is reported, not looped on.

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

Reference: **`p5_11_artifacts_reference.py`** (test writer): replays 6.3 to 6.8 from the stored log and the fixtures' tables and `check-world` for 13; golden **`worldgen/tests/golden/p5-11-artifacts.txt`** (`artifacts-small` worlds at 512 seeds 0–9 and shipped seed 42 at 1024: hashes of `artifact_kind`, `artifact`, `artifact_holding`, `figure`, `event`, `event_cause`, and the stat lines of 10 item 6). Carried test-side items: `p5_06_figures_reference.py` knows role 8; `p5_08_pantheon_reference.py` the return bar and the new deeds; `p3_conflict_reference.py` the extra causes; `p4_browser_reference.py` group 10.

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
| `schema.md` | the DDL of 9, code lists, entity type 10, changelog row 15, catalogue 111 to 116 | 9, 22 |

## 20. For P6 (P5-00 10.2)

P6 reads, per artifact: its present holding (6.9): a hoard (the lair is a dungeon with a treasure), a ruin or a field (a site to place), a grave (the bearer's tomb, `figure.death_region`, `death_settlement`), a sanctum, temple or tower (already a site), a realm's treasury or a living bearer (a ruler's quest line: design 9 "Ruler lines send the player after artifacts"; a realm whose home artifact is held by another, a monster or a ruin has a natural quest). Quest hooks: a stolen artifact never recovered (P5-00 10's example), a crown in a dragon's hoard, a sword in a hero's grave. The chain of holdings is the artifact's lore.

## 21. Open questions

**For Paul** (each also needs a row in `docs/questions-for-paul.md`, the lead's):

| Id | Question | Options | Recommendation | What the build does meanwhile |
| --- | --- | --- | --- | --- |
| Q-P5-11-1 | How many artifacts should a default world make? | (a) a handful (3 to 6), each a legend; (b) 8 to 20, so most realms and great wars have one; (c) many (40+), as common treasure | (b): enough that a third or more change hands and several lie lost at the present as quest hooks, few enough that each is named in the chronicle; a cap of 40 | AR1: median 8 to 20, cap 40 |
| Q-P5-11-2 | May artifacts be destroyed in history (melted down by a conqueror, unmade by a god, broken in battle)? | (a) never; (b) rarely, with an event | (a) never: every artifact then lies somewhere at the present day as a hook, and its chain of holders is its story; destruction can be a game event later | never |
| Q-P5-11-3 | Do evil artifacts corrupt their bearers (a relic of an evil god makes its holder's rulers grasping or warlike)? | (a) no, not in history; (b) yes, through the ruler's temper | (a): the relic records its god (`artifact.god`), so the game can give it a nature; a corrupting rule would need a new temper path and its own calibration | no corruption |
| Q-P5-11-4 | Should the makers be named smiths (a new figure role, about 5 to 10 a world), so that the chronicle says "Thrain forged Kazrak … at the forges of the Hold of Durn"? | (a) yes; (b) no, the forge realm alone is the maker | (a): design 5 asks for a maker, and a named smith of a dwarven hold is the clearest way to show the dwarves as great forgers | smiths are figures (role 8) |
| Q-P5-11-5 | The kinds: five classes (arms, armour, regalia, relic, staff) and fifteen kinds (axe, chalice, crown, hammer, hauberk, helm, idol, orb, reliquary, ring, sceptre, shield, spear, staff, sword). Take, or add others (bows, banners, horns, cloaks)? | take; or extend | Take: data only, so more can be added at any time without code | ships the fifteen |
| Q-P5-11-6 | Lost, buried and enshrined artifacts are never found again in history (no one digs ruins or opens graves); only a slain monster's hoard, a won war and a fleeing cult move an artifact out of another's keeping. Is that right, so that the present day has artifacts waiting for the player? | (a) yes; (b) let heroes or rulers sometimes recover lost artifacts in history | (a): it keeps lost things lost for the player, and a recovery by a hero already happens through hoards | as recommended |
| Q-P5-11-7 | Should a realm's regalia or relic held by another realm be a grievance (more strain toward the holder, and its declaration of war citing the loss) and come back when the realm wins a war against the holder? | (a) yes; (b) no effect on wars | (a): it makes artifacts matter to history, gives ruler quest lines a real motive, and the strain is small and calibrated (AR7) | as recommended |

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

## 22. Text for the lead (`schema.md` and `proposals.md`)

**`docs/specs/schema.md`**: section 1 at version 15 with the three statements of 9 appended after `faction`; a paragraph for the tables of version 15 (9's bullets); `figure.role` 8 smith; entity type 10 `artifact` in the codes paragraph; the catalogue rows 111 to 116; changelog row: `15 | 0.31.0 | P5-11 | tables artifact_kind, artifact, artifact_holding; figure.role 8 smith; entity type 10 artifact in use; event kinds 111 to 116; readers refuse 14`.

**`docs/design/proposals.md`**: entries "Schema 15: artifacts (P5-11)" and "History: P5-11 artifacts (readings)" (appended by the spec writer, r1).

## Revision notes

- r1 (2026-11-28): first draft, for approval.
