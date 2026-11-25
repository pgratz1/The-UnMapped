# P5-03 Race relations, mixed settlements and refugee migrations; P5-02 follow-ups

Status: **r3 (2026-11-24)**: pointer only. `docs/specs/p5-03a-relations-followups.md` (P5-03a, generator 0.25.0) amends the rules of sections 3, 4.2, 5, 6.1, 6.2.2 to 6.2.4, 6.3.2, 6.3.3, 6.4.3, 6.5, 7.1, 8.1, 8.2, 14.2, 17, 19 and 21 and its targets; where the two differ, P5-03a is normative. **r2, 2026-11-24**: correction round, text only, brought in line with what was built, calibrated and ruled on `p5/03-int` (generator 0.24.0); no change to code or output (revision notes at the end). r1 (draft, 2026-11-23) was written by the spec writer on `p5/03-spec`, cut from `origin/main` at 568df26 (generator 0.23.0, schema 9, report format v4; P5-01, P5-12, P5-02 and P4-05 merged). Phase P5 History depth, stage 03 (`docs/specs/p5-00-history-depth-overview.md` 3.1 "P5-03", 3.2, 4, 6.1, 7, 8), with the follow-ups of P5-02 bundled in (part B). Generator **0.24.0**; **schema 10**; history report format **v5** (each the next free one at merge, P5-00 4.3).

Sources: design doc 5 ("Gods and the order of the races": alignment sets starting relations; the race table; "Peoples, beasts and monsters": stances, "Race relations", "Mixed settlements"; "Waiting for phase P5": migrations of whole peoples as refugees; "The plan for phase P5": mixed peoples, state by year), 6 (`race_relation`, `settlement_race`), 9 ("Disposition": an NPC's disposition starts from the relation between their race and the player's and is adjusted by the settlement's mix), 13, 14; P5-00 (3.1 P5-03, 4, 6.1, 7, 8.3); P3-04 r22 (6.9), P3-05 (8.5, 8.6, 9.3, 13, 21), P3-06 (4, 5, 7.5, 20.1), P3-07 r22, P4-01 r4, P5-12 r2, P5-02 r2; `docs/reviews/p5-02-beasts-monsters-world-critic.md` (F1 to F8); Paul's standing decisions in `docs/design/proposals.md` (monsters prefer smaller settlements; an undead host risen in a ruin may still overrun once; the share of capital-sack renamings stays; T16 at most 2 repeated battles per thousand; standing settlements not below P5-02's floor, a median of at least 81 of the default sweep, 83 measured; the `civs4` summary median read as about 1,100) and his wish of 2026-11-23 for abandoned places to explore.

## 0. Size, and where it would split

**Recommendation: one stage, one engineer.** Part A adds one subsystem (`RelationsSubsystem`, about the size of P5-12's renaming subsystem plus a share table) and two seams; part B is a set of small rule, data and report changes to code that exists (the monsters subsystem, peoples' abandonment, the report). Together they are about the size of P5-02, which one engineer built in one branch. They share one schema bump, one report version, one calibration sweep and one re-recording of the goldens, which is cheaper than two rounds of audit, validation and critic.

**The clean split, if the lead prefers smaller reviews**: **P5-02a** = part B (sections 9 to 15, with the part-B rows of 16 to 24), **first**; then **P5-03** = part A (sections 4 to 8 and the rest). P5-02a goes first because P5-03's migrations read P5-02a's abandonment rule (11.1: who has no destination) and both re-measure P5-02's targets. The event kind and stream lists below are already ordered part B first, so a split renumbers nothing. With a split: P5-02a takes generator 0.24.0, schema 10 (the two phrase columns, 16.1) and report v5; P5-03 takes 0.25.0, schema 11 (the two tables) and v6.

## 1. Purpose

At 0.23.0 peoples meet only as realms: two civilizations have a disposition (camp minus their own grudge, P3-06 4.1), but the elves as a people do not remember the goblins' wars, a human kingdom founded in 5000 meets the goblins as if no war had ever been fought, every settlement is of one race whatever happened to it, and the survivors of a sacked town either reach their own realm's nearest settlement or are lost. P5-03 makes the peoples' history:

- **Race relations.** Every pair of races present has a **disposition** that starts from the camps of their creators and the races' own stances (design 5: "alignment sets starting relations", "stances toward the others"), and that wars, sacks, conquests, expulsions and shelter given move over time, with a slow return toward the start. It falls into three **bands** (hatred, wariness, friendship); a pair whose band changes is an event. Realms read their peoples' disposition in place of the bare camp table, so "old wars leave grudges that shape later choices" (design 5) across realms and generations.
- **Mixed settlements.** Every settlement records the **share** of each race living in it and that race's **standing** there (ruling, accepted, tolerated, subject). A captured or ceded town keeps its people under new rulers; refugees bring their race into the town that takes them in; over generations minorities fade into the ruling people. A civilization keeps one principal race (P5-00 3.1, interim reading of design 5).
- **Refugee migrations.** People whom P3 leaves "lost" (no room in their own realm's nearest settlement, no settlement of their own within reach, or driven out) seek shelter in another people's settlement nearby when that people will take them in. A sack, a famine, a plague or a monster can now move a people into another's land (design 5 "Waiting for phase P5").
- **Expulsions**: a ruling people that hates a subject minority may drive it out, which sends refugees and deepens the grudge.

Part B answers the world critic's review of P5-02: raids that repeat for centuries with no response (F1), monsters hidden from the summary in busy worlds (F2), abandonment that is always a tidy move and ancient ruins that all fade (F3), flat monster names (F4), beasts that are all wolves (F5) and overruns of large towns (F8); F6 and F7 are sorted into what is cheap now and what waits for figures (P5-06).

## 2. Items and where their normative text is

| # | Item | Normative text | Amends | Changes worlds |
| --- | --- | --- | --- | --- |
| A1 | Race field `stances`; tuning `history.relations` | 4 | P3-01 (field, id) | yes |
| A2 | `RelationsSubsystem`: dispositions, shares and standing, migrations, expulsions | 5, 6 | P3-04 6.4 (configuration), P3-00 4.5 | yes |
| A3 | Seams: dispositions into conflict; the people ledger in peoples and conflict; `Settlement::owner_event` | 7 | P3-06 4.1, 4.3, 7.5, 20.1; P3-05 8.5; P3-04 6.1 | yes |
| A4 | Event kinds 57 to 62 | 8 | P3-04 8.2 | yes |
| B1 | F1 raids: causes, tolls, hunts called sooner, flight for fear, phrases by kind | 9 | P5-02 4.2, 4.3, 6.4, 10.2 | yes |
| B2 | F2 monster stories in the summary; ended monsters in `[monsters]` | 10 | P3-07 24.3; P5-02 12.4 | report |
| B3 | F3 abandonment without a destination or with losses; flight for fear; ruins that do not fade | 11 | P3-05 8.5, 8.6, 12.1; P5-02 8.3 | yes |
| B4 | F4 names: epithets, hosts named for their place, the filter, rule 7 | 12 | P3-02 4.2, 4.5; P5-02 4.2, 9; P5-12 4 | yes |
| B5 | F5 bold beasts | 13 | P5-02 4.1, 6.2.3, 10.2 | yes |
| B6 | F8 overruns of small places; F7 cheap part; F6 deferred | 14 | P5-02 6.3.2, 6.4.3 | yes |
| B7 | Test item: `p5_12_renaming_reference.py check-world` | 15 | — | no |
| C1 | Schema 10 | 16 | P3-04 9, `schema.md` | yes |
| C2 | Report v5 | 17 | P3-07 (new section 28) | report |
| C3 | Timeline reads | 18 | P4-01 (r5) | no |

Event kind 56 `settlement_fled` is part B's; 57 to 62 are part A's (8, 11.3).

## 3. Inputs

- **Geography**: `GeoView` and `PeoplesView` as P3-06 uses them (`hops`, `capacity`, `neighbours`); nothing new in `HistoryInputs`.
- **Live state** (`State`): civilizations (race, ended), settlements (region, cell, owner, population, status, founder, and the new `owner_event`, 7.3), relations (`war`), region owners.
- **The log so far** through `StepContext`'s reads (P3-04 r21 23.3): kind, year, participants by role, settlement, values.
- **Data**: race rows (`race.camp`) and the race field `stances` (4.1); `history.conflict.camp` (P3-06 4.1, read through `ConflictSubsystem::camp_table()`, a const read); tuning `history.relations` (4.2); peoples' `lying_low` (P3-06 22.3, a const read).
- **Settings**: none. `param.event_intensity` does not scale this stage's rules (P5-00 5 lets a stage choose; the pressures that displace people already scale by it).
- **Streams** (P5-00 4.2), appended to `streams.hpp` after `hist.monsters.raid_target`, in this order: `hist.beasts.bold_kind` (13), `hist.monsters.fear` (9.4), `hist.peoples.abandon_loss` (11.1), `hist.relations.expel` (6.5).

**Draws.** As P5-02 3: `D(stream; a, b, k)` = `StepContext::draw(stream, a, b, k)` with w0 the step's year (a quiet step's start); `chance_pm`, `pick` and `weighted` as there. All arithmetic int64 with checked multiplication; `div` is floor division of non-negative values; where a value can be negative the spec says `floor_div` or "truncated toward zero".

| Stream | a, b, k | Section |
| --- | --- | --- |
| `hist.beasts.bold_kind` | region, 0, 0 | 13 |
| `hist.monsters.fear` | monster, settlement, 0 | 9.4 |
| `hist.peoples.abandon_loss` | settlement, 0, 0 | 11.1 |
| `hist.relations.expel` | settlement, race, 0 | 6.5 |

Everything else of part A is deterministic (orders and keys, no draw).

# Part A. Race relations, mixed settlements and migrations

## 4. Data

### 4.1 Race field `stances` (P3-00 8.1's reserved "relations by race"; amends P3-01)

Appended after `culture` in every race definition (shipped and both fixture roots): `stances`, array(record {`race`: string(32), `value`: int(−500, 500)}, 0, 8), required (an empty array is allowed). The race's own leaning toward another race, added to the camp table (6.2.1).

Check `race_stances` (with the history data checks, after every kind is loaded, P5-02 25 item 1's error path; `check 'race_stances' failed for 'race.<id>': <message>`), first failing entry only: `stances[<i>].race '<r>' is not a race`; `stances[<i>].race '<r>' is the race itself`; `stances[<i>].race '<r>' is listed twice`.

Shipped (taste; small on purpose, so that P3's war balance moves little, 21 C1):

| race | stances |
| --- | --- |
| `elf` | `faerie` +200 |
| `faerie` | `elf` +200 |
| `dwarf` | `goblin` −100 |
| `goblin` | `dwarf` −100 |
| `human`, `orc` | none |

Why: the faeries are "allies of the elves by origin" (design 5's race table; P5-04 brings the alliance itself); the dwarves "arrive into a world already at war" and build in the uplands the goblins hold, so the two start colder than neutral and evil camps alone make them. **Temperament** (design 6's `race.temperament`) is not needed: P3's `aggression` already carries a race's readiness for war (P3-06 5.2), and no rule of this stage needs another trait (the overview's "if the spec needs it").

### 4.2 Tuning `history.relations` (new fixed id of kind `history`; `game/data/history/relations.toml` and both fixture roots)

All fields required, in this order:

| Field | Type | Shipped | Meaning |
| --- | --- | --- | --- |
| `hatred_max` | int(−2000, 0) | −500 | a disposition at or below this is hatred (equal to `history.conflict.enmity_max`, 7.1) |
| `friend_min` | int(0, 2000) | 150 | at or above this is friendship |
| `band_margin` | int(0, 500) | 50 | hysteresis of a band change (6.2.3) |
| `memory_min`, `memory_max` | int(−2000, 0), int(0, 2000) | −500, 500 (r1: −250, 250) | bounds of a pair's memory, in disposition units |
| `memory_decay_per_century` | int(0, 10000) | 20 (r1: 40) | memory's return toward 0 |
| `war_memory` | int(0, 1000) | 75 (r1: 25) | lost per war between them that ends by peace, truce or victory |
| `conquest_memory` | int(0, 1000) | 150 (r1: 50) | lost per realm conquered |
| `sack_memory`, `capture_memory` | int(0, 1000) | 30, 15 (r1: 10, 5) | lost per settlement sacked, captured |
| `expel_memory` | int(0, 1000) | 90 (r1: 30) | lost per expulsion |
| `shelter_memory` | int(0, 1000) | 45 (r1: 15) | gained per arrival of refugees of one race in the other's settlement |
| `meet_hops` | int(−1, 8) | 8 (r1: 3) | two races meet when realms of both lie within this many borders (6.2.4); −1: never (a test seam) |
| `migrate_hops` | int(0, 8) | 2 | refugees seek a host within this many borders of their old home |
| `migrate_min` | int(1, 100000) | 40 | the smallest group that migrates (equal to `abandon_min`) |
| `welcome_min` | int(−2000, 2001) | −200 | a host's people takes in a race whose disposition toward it is at least this; 2001: never |
| `subject_years` | int(0, 100000) | 200 | years a conquered people stays subject whatever the disposition |
| `conquest_settlers_permille` | int(0, 1000) | 100 | the conquerors' own people settling in a taken town (6.3.3) |
| `drift_permille_per_century` | int(0, 1000) | 100 | the share of each minority that passes to the ruling people per century |
| `fade_permille` | int(0, 1000) | 20 | a minority below this share is absorbed |
| `mixed_min_permille` | int(1, 1001) | 100 | a minority at or above this makes the settlement mixed (6.3.6's event); 1001: never. The report's "mixed at E" uses its own constant, 17.2 |
| `expel_permille` | int(0, 1000) | 3 (r1: 30) | chance per tick year of an expulsion that may happen |
| `expel_share_max` | int(0, 1000) | 500 | a minority above this share is too many to drive out |

Every value is the shipped (calibrated, 22 step 10) value at 0.24.0; r1's value is in brackets where calibration moved it.

Checks: `relations_bands`: `hatred_max` + `band_margin` < `friend_min` − `band_margin` (`bands overlap: hatred_max <a> and friend_min <b> with band_margin <m>`); `relations_memory`: `memory_min` ≤ 0 ≤ `memory_max` (by the types).

**The `zero` fixture tuning** (proof obligation I0, 7.4): every `*_memory` 0, `meet_hops` −1, `welcome_min` 2001, `subject_years` 100000, `mixed_min_permille` 1001, `expel_permille` 0, the others as shipped; with every race's `stances` empty.

## 5. State

`RelationsSubsystem` (`worldgen/src/history/relations.cpp`, with `relations_shares.cpp` as the engineer likes; header `worldgen/history/relations.hpp`) keeps in its own members, ids only:

- **Pairs**: for each pair of race ids (a, b), a < b, of the races present: B(a, b) (fixed, 6.2.1), the memory M in **milli-units** (int64; M ∈ [`memory_min` × 1000, `memory_max` × 1000]), `met` (bool), the met year and event, the band, the year and event of its last turn (0 none), the **contributions**: the ids of the last at most 8 events that moved M since the last turn or meeting, ascending, and the rows written so far.
- **Shares**: per settlement s (index s − 1, every settlement up to the count, 6.3.1) its known owner o_s (the owner when last seen standing), its ruling race q_s, and the list of **entries** (race g, share in per mille, standing, `since_year`, `since_event`, `conquered` flag, `mixed_told` flag), ascending race id, shares summing to 1000, every share ≥ 1 (an entry at 0 is removed); and the rows written so far.
- **Pending moves and displacements** (the people ledger, 6.4): in record order, each (kind move or displacement, from, to or 0, race or 0, people, cause, the from-settlement's entries copied at the call, the from-settlement's owner at the call).
- The **log cursor** (first event id not yet read), the year of the last drift, the phase of the last T1 or Q1.

Bounds: settlements and races are those of `State`; at most 6 × 5 / 2 = 15 pairs with the design's races (any count the race rows allow); entries per settlement ≤ races present.

## 6. Rules

### 6.1 Steps and order

`steps()` = T1, T4, T6, Q1, Q4. Installed in `EngineConfig::subsystems` **after** `MonstersSubsystem`: peoples, conflict, renaming, monsters, relations (P5-00 4.1). Not a tension source.

| Step | What runs, in this order |
| --- | --- |
| T1, Q1 | shares of new settlements (6.3.1); pending moves (6.4.2); if `phase_id()` differs from the last T1 or Q1's: the rows of the previous phase's end (6.7), drift (6.3.4) and standings (6.3.5); pending displacements (6.4.3); mixed (6.3.6) |
| T4 | shares of new settlements; pending moves; pending displacements; mixed |
| T6, Q4 | shares of new settlements; pending moves; the log (6.2.2); owner changes (6.3.3); expulsions (6.5, T6 only); pending displacements; decay (6.2.2); turns (6.2.3); meetings (6.2.4); mixed |

Y is the step's year (a quiet step's start), L its length. Nothing acts in T2, T3 or Q3. Every step ends by writing the `settlement_race` rows of the step (6.7).

### 6.2 Dispositions

#### 6.2.1 Base, memory and disposition

For races a ≠ b with camps k_a, k_b (P3-03 `race.camp`), with `stance(x→y)` the `value` of race x's `stances` entry for y, else 0:

```
B(a, b) = camp_table[min(k_a, k_b)][max(k_a, k_b)] + floor_div(stance(a→b) + stance(b→a), 2)
D(a, b) = B(a, b) + (M(a, b) truncated toward zero to whole units: M / 1000 in C++'s integer division)
```

D is symmetric. For a = b, D(a, a) = camp_table[k_a][k_a] (the same race; no memory). Shipped bases: elf–faerie 600, dwarf–goblin −350, the rest the camp table (P3-06 4.1). Before a pair has met, M = 0 and D = B.

#### 6.2.2 What moves the memory (T6, Q4)

**The log.** Read every event e from the cursor to `last_event_id()` in ascending id, then advance the cursor. For e of a kind below, take its two civilizations x, y (by the roles listed, through `StepContext::event_participant`; for `truce` the two `parties` in participant order, through `StepContext::event_participants(event, role)`, which returns every participant id of the role in participant order) with races r_x, r_y; if r_x ≠ r_y and the pair has met: M(r_x, r_y) −= Δ × 1000 (or += for shelter), clamped to its bounds; append e to the pair's contributions (keeping the last 8).

| Kind | x, y | Δ |
| --- | --- | --- |
| `war_won`, `peace` | `winner`, `loser` | `war_memory` |
| `truce` | the two `parties` | `war_memory` |
| `civ_conquered` | `civilization`, `conqueror` | `conquest_memory` |
| `sacked` | `attacker`, `defender` | `sack_memory` |
| `settlement_captured` | `attacker`, `defender` | `capture_memory` |
| `people_expelled` (8) | `civilization`; the `race` participant is r_y | `expel_memory` |
| `refugees_arrived` (8) | `civilization`; the `race` participant is r_y | + `shelter_memory` |

Events of this subsystem written in this step are read at the next T6 or Q4.

**Decay** (T6: years 1; Q4: years L): for every pair, d = years × `memory_decay_per_century` × 1000 div 100; M > 0: M = max(0, M − d); M < 0: M = min(0, M + d). In milli-units a tick year decays 200 at the shipped rate (20 a century), so ticks and quiet steps decay alike.

#### 6.2.3 Bands and turns (T6, Q4)

Bands: 0 **hatred**, 1 **wariness**, 2 **friendship**. The band at a meeting is plain: 0 if D ≤ `hatred_max`, 2 if D ≥ `friend_min`, else 1. Afterwards, for each met pair (a, b) ascending, with h = `hatred_max`, f = `friend_min`, m = `band_margin`:

```
band 1 → 0  if D ≤ h          band 0 → 1  if D > h + m
band 1 → 2  if D ≥ f          band 2 → 1  if D < f − m
```

(at most one change per pair per step; band 0 and 2 never change directly into each other in one step). A change appends **`disposition_turned`** (region 0, settlement 0; participants a `race_a`, b `race_b`, both of entity type race; values `disposition` = D, `band` = the new band; causes = the pair's contributions, or when it has none (a turn by decay alone) its last turn event, else its met event); the pair's band, last turn year and event are set and its contributions cleared; a row is written (6.7). In Q4 it needs `quiet_events_left(0)` > 0, else it waits for the next T6 or Q4 (the turn is tested again then).

#### 6.2.4 Meetings (T6, Q4)

For each pair (a, b) ascending that has not met, with both races awakened: the **presence** of a civilization is P3-06 5.1's simple definition (`presence(c)`: the regions c holds and the regions of its standing visible settlements, ascending), not P3-06 20.1's, which leaves out hidden refuges: a hidden refuge's region counts for a meeting. Over the pairs of alive, visible civilizations x of race a and y of race b, and regions R_x ∈ presence(x), R_y ∈ presence(y), find the smallest key (h, R_x, R_y, x, y) with h = `PeoplesView::hops(R_x)[R_y]` (unreachable: none). If such a key exists with h ≤ `meet_hops`: append **`peoples_met`** (region = the region of the younger of x and y (larger `founded_year`, then larger id) in the key, settlement 0; participants a `race_a`, b `race_b`; values `disposition` = B(a, b), `band` = the plain band; cause the younger's `founding_event`); the pair is met (year Y, the event), M = 0, its band set; a row is written (6.7). In Q4 it needs `quiet_events_left(0)` > 0, else it waits. `meet_hops` = −1: no pair ever meets.

### 6.3 Shares and standing

Standing codes: **0 subject**, **1 tolerated**, **2 accepted**, **3 ruling** (higher is better). Standing words: `subject`, `tolerated`, `accepted`, `ruling`.

#### 6.3.1 A new settlement (every step, first)

For every settlement id above the count this subsystem knows, ascending (its own bookkeeping, not a step that processes the new settlement: no draw, no event, no state change; P3-04 6.2's visibility rule is about which entities a step acts on), with c = its `founder_civ` and r = race(c): o_s = c, q_s = r, one entry (r, 1000, ruling, its `founded_year`, its `founding_event`). A colony is of its founder's people only (colonists are the realm's own; reading, 25). A refuge founded by flight (P3-06 20.1) gets its mix from the ledger's move (6.4.2), which follows in the same step.

#### 6.3.2 Mixing a group into a settlement

`mix(s, G, n, P_b)`: the settlement s with entries S and weight P_b (its population before the group came) takes n people whose shares are G (a list of (race, share), summing to 1000). For each race g in S ∪ G: c_g = S_g × P_b + G_g × n (per-mille people; S_g, G_g = 0 when absent); T = P_b + n; share_g = c_g div T; the 1000 − Σ share_g left over go one each to the races in the order (c_g mod T descending, race id ascending). Races with share 0 are dropped. P_b = 0 gives G exactly. A race new to s gets standing by 6.3.5's target, `since_year` Y and `since_event` the event of the move (the arrival or the abandonment it came with), `conquered` false.

#### 6.3.3 Owner changes (T6, Q4)

For each standing settlement s ascending whose `owner_civ` c ≠ o_s: o_s = c; if q = race(c) ≠ q_s (a people of another race now rules it):

1. the old ruling race's entry becomes standing 0 (subject), `conquered` true, `since_year` Y, `since_event` = `Settlement::owner_event` (7.3: the capture or cession);
2. q becomes the ruling race: its share becomes x + (1000 − x) × `conquest_settlers_permille` div 1000 (x its share before, 0 if absent; reading: the conquerors settle their own people among the conquered), the other entries are scaled to fill 1000 − that share by 6.3.2's largest-remainder rule (with weights their shares), standing 3, `since_event` as in item 1;
3. every other entry's standing becomes 6.3.5's target under q, without an event.

The same race taking it (a secession, a capture between two realms of one race): o_s changes, nothing else. A row is written for s (6.7).

#### 6.3.4 Drift (at a phase start, T1 or Q1)

Δ = Y − the year of the last drift (the history's first year for the first), then the last drift year = Y. For each standing settlement with more than one entry, each entry g ≠ q_s ascending: lost_g = min(share_g, share_g × `drift_permille_per_century` × Δ div 100000); share_g −= lost_g; share_q += lost_g. Then every entry g ≠ q_s with share_g < `fade_permille` passes its whole share to q_s and is removed. Written as rows (6.7).

#### 6.3.5 Standing

The **target** standing of an entry g ≠ q_s: 0 if `conquered` and Y − `since_year` < `subject_years`; else by the band of the pair (q_s, g) (its band if met, else the plain band of B): hatred 0, wariness 1, friendship 2. At a phase start (T1 or Q1, after drift), for each standing settlement s ascending and each entry g ≠ q_s ascending whose standing differs from its target: append **`standing_changed`** (region and settlement s; participants s `settlement`, o_s `civilization`, g `race`; values `standing` = the target, `share`; cause: the pair's last turn event if it is later than the entry's `since_event`, else the entry's `since_event`); standing = target, `since_event` = the event, `since_year` = Y, `conquered` false once the target is not 0 by conquest. In Q1 it needs `quiet_events_left(o_s)` > 0, else it waits for the next phase start.

#### 6.3.6 Mixed (every step, last)

For each standing visible settlement s ascending, each entry g ≠ q_s ascending with share_g ≥ `mixed_min_permille` and `mixed_told` false: append **`settlement_mixed`** (region and settlement s; participants s `settlement`, o_s `civilization`, g `race`, q_s `ruling`; value `share`; cause the entry's `since_event`); `mixed_told` = true. In Q1 and Q4 it needs `quiet_events_left(o_s)` > 0, else it waits. `mixed_told` belongs to the entry, not to the pair (settlement, race): an entry removed (expulsion, fade) takes its flag with it, and a race that comes back gets a new entry with `mixed_told` false, so its return is told again. A settlement is **mixed** at Y when some entry g ≠ q_s has share ≥ `mixed_min_permille`.

### 6.4 Migrations: the people ledger

#### 6.4.1 What is recorded, and by whom

`PeoplesSubsystem` gains `void set_ledger(PeopleLedger*)` and `PeopleLedger* ledger() const`; `PeopleLedger` (declared in `peoples.hpp`) has two calls, each of which only records (no `State` change, no append, no draw):

- `moved(state, from, to, people, cause)`: called right after `people` of settlement `from` were added to settlement `to` by the caller (so `to`'s population before is its population now minus `people`); `cause` is the event the move came with (the ruin, the capture or cession, the refuge's founding), and the races new to `to` date from it (6.4.2);
- `displaced(state, from, race, people, cause)`: `people` of settlement `from` have nowhere to go (race 0: of every race of `from`, by its shares; else of that race only); `cause` is the event that displaced them.

Both take the live `State` (const) as their first argument; both declared pure virtual in `PeopleLedger`.

At each call the subsystem first runs 6.3.1's bookkeeping (so a settlement founded earlier in the step is known), then copies `from`'s entries and o_from (as they are at the call) into the record. The callers (7.2): P3-05's ruin procedure item 2 (`moved` for the survivors that reach t, `displaced` for the rest, cause the ruin's event e), which serves abandonment, sack and overrun; P3-06's `refugees` after a capture or a cession (`moved` for those placed, `displaced` for the rest, cause the capture, or the `settlements_ceded` or `region_ceded` event); P3-06's flight (`moved` into the refuge, after `found_settlement`, cause the `refugees_settled` event); this subsystem's expulsion (6.5, `displaced` with the race). With no ledger set, no call is made.

#### 6.4.2 Pending moves

In record order: when `to` is not standing when the record is resolved (it fell after the call), the record is skipped: its people went with it. Else `mix(to, from's copied entries, people, P_to − people)` with P_to read at the call (stored in the record); a race new to `to` gets `since_year` Y (the resolving step's year) and `since_event` the record's cause. A row is written for `to` (6.7).

#### 6.4.3 Pending displacements

In record order, for record (from, race, n, cause, entries E, owner o):

1. **Groups**: race ≠ 0: one group (race, n). Else split n over E: n_g = n × share_g div 1000, the n − Σ n_g left over one each by (n × share_g mod 1000 descending, race ascending). Groups with n_g < `migrate_min` are lost (they scattered). Groups in ascending race id.
2. **Hosts** of group (g, n_g): standing visible settlements u ≠ from with owner c_u alive, visible and not lying low (`PeoplesSubsystem::lying_low`), c_u ≠ o, no open `war` relation between c_u and o (either order; none when o = 0), h_u = `hops(region(from))[region(u)]` ≤ `migrate_hops`, D(race(c_u), g) ≥ `welcome_min`, and room_u = max(0, 2 × `capacity(region(u), race(c_u))` − P_u) ≥ `migrate_min` (P3-05 8.5's room rule).
3. **Choice**: the host u with the smallest key (h_u, kin_u, −D(race(c_u), g), squared cell distance from `from`, u), kin_u = 0 if race(c_u) = g else 1. None: the group is lost.
4. **Arrival**: taken = min(n_g, room_u); append **`refugees_arrived`** (region of u, settlement u; participants u `settlement`, c_u `civilization`, from `origin`, g `race`; values `people` = taken, `standing` = the group's target standing in u (6.3.5; 3 when g = q_u); cause the record's cause); `set_population(u, P_u + taken)`; `mix(u, [(g, 1000)], taken, P_u)` with `since_event` the arrival. The rest of the group is lost. In Q1 and Q4 an arrival needs `quiet_events_left(c_u)` > 0, else the group is lost. Hosts are re-evaluated for every group (a host's room shrinks as it fills).

The people lost were already gone from `State` when the caller displaced them; an arrival adds them back in the host. A displaced group never founds a settlement (P3's flight of a last refuge is unchanged; 25).

### 6.5 Expulsions (T6)

For each standing visible settlement s ascending whose owner o_s is alive, visible and not lying low, each entry g ≠ q_s ascending with standing ≤ 1, band(q_s, g) = hatred, share_g ≤ `expel_share_max`, and n = P_s × share_g div 1000 ≥ `migrate_min`: if `chance_pm(D(hist.relations.expel; s, g, 0), expel_permille)`: append **`people_expelled`** (region and settlement s; participants s `settlement`, o_s `civilization`, g `race`; value `people` = n; causes the entry's `since_event` and, when later, the pair's last turn event, ascending); `set_population(s, P_s − n)`; `PeoplesSubsystem::note_reduction(s, event)` (so a place left below `abandon_min` is abandoned at the next T1 with the expulsion as its cause, P3-05 8.5 (b)); the entry is removed and the others scaled to 1000 by 6.3.2's rule; `displaced(s, g, n, event)`. At most one expulsion per settlement per year. Quiet steps have none.

The majority guard (`expel_share_max`) keeps a conquered majority in place until drift has made it a minority, so an expulsion never takes more than half of a settlement.

### 6.6 P3-04 6.9

1. No `found_civilization`, `found_settlement`, `open_relation` or `close_relation` call. `State` is changed only by `set_population` (arrivals, expulsions) and the peoples call `note_reduction`. Loops over settlements and civilizations iterate ids up to a count taken before the loop and re-fetch after each change; members hold ids.
2. Not a tension source.
3. Fixed place: after monsters in `subsystems`, filled in `stages/history.cpp`, which also sets the ledger (`peoples.set_ledger(&relations)`) and the disposition view (`conflict.set_dispositions(&relations)`, 7.1).
4. Visibility, below.
5. Keys: settlement and race ids.
6. No `relation` row is written (race relations are this subsystem's tables).
7. No region owner changes.

**What it sees in shared steps.** T1: peoples' growth and abandonments (8.5 (b)) of the year, live; its ledger records. T4: peoples' pressures and abandonments, monsters' raids, overruns and flights (part B), live; the ledger. T6: conflict's battles, sieges, sacks, captures, cessions and flights, renaming's and monsters' outposts, live; the ledger. Q1, Q4: peoples' drift, foundings and abandonments, conflict's cessions, live. Settlements and civilizations created in the step are invisible as hosts and as expellers, and are given shares (6.3.1). Its own changes (dispositions, populations) reach conflict and peoples from their next call (conflict reads D from its next T2).

**Quiet steps.** Meetings and turns charged to civilization 0; arrivals to the host's owner; standings and mixed to the owner; every one checks the budget first (W9 by construction). The world-level bound of P5-00 4.1: at most (pairs) meetings and turns per quiet step, and they wait when the budget of civilization 0 is used up.

### 6.7 Rows

**`race_relation`** (16.2): a row (a, b, year, disposition, band) is written at a pair's meeting (the meeting's year), at each turn (the turn's year), at the start of each T1 or Q1 whose phase differs from the last (year = the previous phase's last year), and when the stage ends the run (`finish`, year E − 1), each only when (disposition, band) differs from the pair's last row; when two rows of one pair fall in the same year, the later replaces the earlier. A pair that never met has no row.

**`settlement_race`** (16.2): a row (s, g, year, share, standing) for each entry whose (share, standing) differs from the last row of (s, g), written at the end of every step of this subsystem for every standing settlement (unchanged entries write nothing), at the step's year Y; a quiet step's rows are dated at its start Y, like the census (P3-04 6.7), drift and standings included (r1 said Y + L − 1 for drift). `finish` writes no `settlement_race` row (the last step's rows are the end state). An entry removed writes a row with share 0 and standing 0 (the race left); a first row of share 0 is never written. No row is written for a settlement after it is ruined. Same-year rows of one (s, g): the later replaces the earlier. So the shares of a standing settlement at the end of year Y are, per race, the last row with year ≤ Y and share > 0.

## 7. Seams

### 7.1 Dispositions into conflict (amends P3-06 4.1, 4.3)

`ConflictSubsystem` gains `void set_dispositions(const DispositionView*)` and `const ConflictCamp& camp_table() const`; `DispositionView` (declared in `conflict.hpp`) has `std::int64_t disposition(std::int64_t race_a, std::int64_t race_b) const` (6.2.1's D, symmetric, D(a, a) the camp value) and `std::int64_t hatred_event(std::int64_t race_a, std::int64_t race_b) const` (the pair's last `disposition_turned` into band 0, 0 when none). With no view set, conflict is as at 0.23.0.

- **4.1**: d(a, b) = D(r_a, r_b) − grudge{a, b}, read from the view (as it holds when conflict runs: the end of relations' last T6 or Q4). For r_a = r_b this is camp(a, b) − grudge, unchanged.
- **4.3 `enmity`**: a pair of civilizations qualifies when D(r_a, r_b) ≤ `enmity_max` (in place of camp(a, b) ≤ `enmity_max`); the row is opened with `since_event` = the larger id of the younger civilization's `founding_event` and `hatred_event(r_a, r_b)`; its `since` is the year it opens (the bookkeeping year, as every `relation` row's, P3-04 6.7), so a row opened by a later turn into hatred has `since` = that later year while its `since_event` may be older. So a pair that qualifies at first sight opens as before, and a pair whose peoples came to hate each other later opens with the turn that brought it. The rest of 4.3 is unchanged (at most one row per pair, ever; closed by the end of a side). With the shipped values `hatred_max` = `enmity_max` = −500, an enmity row exists between realms exactly when their peoples are in hatred (or were, since the row stays).

### 7.2 The people ledger in peoples and conflict (amends P3-05 8.5, P3-06 7.5, 20.1)

- P3-05 8.5 item 2 (the ruin procedure, so also abandonment, sack and overrun): after t gains `settled`, `ledger()->moved(state, s, t, settled, e)` when settled ≥ 1; then, when P − settled ≥ 1 (the people with no room or no destination), `ledger()->displaced(state, s, 0, P − settled, e)` with e the ruin's event. Abandonment's road losses (11.1) are dead, not displaced: they are taken off before the ruin.
- P3-06 7.5's `refugees` after a capture, and after a cession (P3-06 22.5's excess over the winner's room, kind 29 or 40): `moved(from, nearest, placed, cause)` when placed ≥ 1, then `displaced(from, 0, people − placed, cause)` when that is ≥ 1 (and `displaced(from, 0, people, cause)` when there is no own settlement), with cause the capture or the cession event.
- P3-06 20.1's flight: `moved(t, h, n, the refugees_settled event)` right after the refuge h is founded with n people.
- Nothing else in P3 moves people between settlements (colonists found new settlements of the founder's people, 6.3.1; a tribute is casualties, P3-06 20.3).

### 7.3 `Settlement::owner_event` (amends P3-04 6.1)

`Settlement` gains `std::int64_t owner_event`: the event of its last `set_settlement_owner`, its `founding_event` at founding. Not stored in the world (as `RegionState::holder`). Read by 6.3.3.

### 7.4 Proof obligation I0

With the `zero` tuning (4.2) and every race's `stances` empty, the history tables of a world (events, participants, causes, values, phases, civilizations, settlements, census, control, relations, region names, beasts, monsters, sites) are byte-identical to those of the same world with `RelationsSubsystem` not installed, no ledger and no disposition view set: no event is written (no meeting, so no band, no turn, no standing change beyond conquest's, which writes no event; no arrival; no expulsion; no mixed event), no population changes, and D = camp. What differs is the catalogue rows of the new kinds and the two new tables.

### 7.5 What else peoples, conflict and monsters read

Nothing of this stage: not the shares, not the standings, not the events of kinds 57 to 62. Conflict reads D and `hatred_event` only. (P5-04 is expected to read dispositions for trade and alliances and to add a trade path into the shares through a seam of its own: `RelationsSubsystem::add_people(settlement, race, people, event)`; not specified here.)

## 8. Events (part A)

### 8.1 Catalogue (appended after 56, 11.3; ids provisional, the next free ones at merge)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values |
| --- | --- | --- | --- | --- | --- | --- |
| 57 | `peoples_met` | 1 | 1 | 0 | 1 `race_a` race 1–1; 2 `race_b` race 1–1 | `disposition`, `band` |
| 58 | `disposition_turned` | 1 | 0 | 0 | 1 `race_a` race 1–1; 2 `race_b` race 1–1 | `disposition`, `band` |
| 59 | `refugees_arrived` | 1 | 1 | 1 | 1 `settlement` settlement 1–1; 2 `civilization` civilization 1–1; 3 `origin` settlement 1–1; 4 `race` race 1–1 | `people`, `standing` |
| 60 | `settlement_mixed` | 1 | 1 | 1 | 1 `settlement` settlement 1–1; 2 `civilization` civilization 1–1; 3 `race` race 1–1; 4 `ruling` race 1–1 | `share` |
| 61 | `standing_changed` | 1 | 1 | 1 | 1 `settlement` settlement 1–1; 2 `civilization` civilization 1–1; 3 `race` race 1–1 | `standing`, `share` |
| 62 | `people_expelled` | 1 | 1 | 1 | 1 `settlement` settlement 1–1; 2 `civilization` civilization 1–1; 3 `race` race 1–1 | `people` |

The charged civilization of a quiet-step event (W9) is its first civilization participant: 0 for 57 and 58. In 57 and 58, `race_a` < `race_b` by id.

### 8.2 Templates and salience (data, `history.report`)

| key | salience | template | summary? |
| --- | --- | --- | --- |
| `peoples_met` | 25 | `The {race_a:plural} first met the {race_b:plural} in {@region}{band?, and hated them from the first|, and were wary of them|, and were glad of them}.` | at low thresholds |
| `disposition_turned` | 55 | `{@causes:war_won=the wars,peace=the wars,truce=the wars,civ_conquered=the conquests,sacked=the sacks,settlement_captured=the conquests,people_expelled=the expulsions,refugees_arrived=years of shelter given|After |, }the {race_a:plural} and the {race_b:plural} {band?came to hate one another|grew wary of one another|became friends}.` | yes |
| `refugees_arrived` | 25 | `{@causes:sacked=the sack,settlement_captured=the fall,settlement_overrun=the ravage,people_expelled=the expulsion|After |, }{people} {race:plural} from {origin} found shelter in {settlement}{standing?, held as subjects by|, tolerated by|, welcomed by|, among their own kin of} {civilization}.` | at low thresholds |
| `settlement_mixed` | 25 | `{@causes:settlement_captured=the conquest,settlements_ceded=the cession,region_ceded=the cession|After |, }the {race:plural} and the {ruling:plural} lived side by side in {settlement}.` | at low thresholds |
| `standing_changed` | 15 | `In {settlement}, {civilization} {standing?held the|tolerated the|accepted the} {race:plural}{standing? as subjects|| as equals}.` | no |
| `people_expelled` | 40 | `{civilization} drove the {race:plural} out of {settlement}; {people} left.` | yes |

`refugees_arrived`'s `{standing?…}` has four alternatives for 0, 1, 2 and 3: **the choice placeholder gains a four-word form** (amends P3-04 8.4, r10's three-word form): `{v?w0|w1|w2|w3}` renders w0 for 0, w1 for 1, w2 for 2 and w3 for every other value (`#` is the value, as in the three-word form); `template_problem` unchanged otherwise. `people` ≥ `migrate_min` (40), so the number is never a word.

**A sentence starts with a capital** (amends P3-04 8.5 and 21; r23): after every substitution, when the first byte of the rendered sentence is a lower-case ASCII letter (`a` to `z`), `render_event` makes it the upper-case letter; nothing else changes. It is a general rule of the renderer, not of these templates: it is needed by templates that open with a `{@causes:…|After |, }` clause followed by a literal `the ` when no cause has a word (`disposition_turned`, `settlement_mixed`), and it applies to any template whose first substitution renders lower-case, a number word included. It supersedes P3-04 r17's "the word is never capitalized" for a word at byte 0 (no shipped template puts a value there). No world byte changes (sentences are not stored). Samples (illustrative): `The Dwarves first met the Goblins in the Grey Fells, and were wary of them.` · `After the wars and the sacks, the Dwarves and the Goblins came to hate one another.` · `After the sack, 240 Humans from Ashford found shelter in Kazad Rune, tolerated by the Hold of Durn.` · `After the conquest, the Goblins and the Elves lived side by side in Fievos.` · `The House of Aelin drove the Goblins out of Fievos; 310 left.`

Causes (W6): a meeting, the younger realm's founding; a turn, the events that moved the memory since the last turn (or the last turn, or the meeting); an arrival, the event that displaced the group (an abandonment, a sack, an overrun, a capture, an expulsion); a mixed settlement, the event that brought the minority or put it under new rulers; a standing change, the turn that changed the band, else the event the standing dates from; an expulsion, the event that made the people subjects and, when later, the turn into hatred.

**Summary candidates**: about 15 meetings (every pair, RR1), a median of 1 turn (RR2), 5 arrivals (MX2), 3 expulsions (MX3) and about 8 mixed settlements (MX1) a default world, against the 12.9 budget (P5-02); no companion is added. T18 (21) holds: default median 620 lines.

# Part B. P5-02 follow-ups

Source: `docs/reviews/p5-02-beasts-monsters-world-critic.md`. Each finding with its answer; sections 9 to 15 amend P5-02 (and P3-05 where named), whose normative text for these points is here.

## 9. F1 Raids that repeat with no response

**Decision.** Repeated raids now force a consequence by two rules: the raided realm **calls a hunt sooner** with every raid it remembers (9.3), and a small place raided again and again may be **abandoned for fear** of the monster (9.4). Both fit existing mechanics: the hunt is the realm's answer P5-02 already has, now tied to how often it is struck; the flight for fear is Paul's wish for abandoned places, and ends a run of raids on a hamlet or village. **Tribute to a monster** is not chosen: a tribute of people is already war's cost of last resort (P3-06 20.3), and a hoard paid in goods needs wealth (P5-04); it is listed for P5-04 (27). A fortress or town raided often (the critic's Tourn, 50 raids by Thound) is answered by the hunt alone.

### 9.1 A raid cites the last raid on the same place (amends P5-02 6.4.1)

The monster keeps, per settlement, the years and ids of its raids on it within the last max(`raid_chain_years`, `fear_years`) years. A raid's causes are, ascending without repeats: the monster's current waking event and, when m raided t within the last `raid_chain_years` (new field of `history.monsters`, shipped **300**, r1 100; "within": Y − year < `raid_chain_years`), its last such raid. So a run of raids reads as a chain, and a reader of the log can follow a place's suffering back to the monster's waking.

### 9.2 Tolls scale with the place (amends P5-02 6.4.1)

```
raw  = P × τ div 1000
dead = raw                                                         if raw ≤ kill_max
       kill_max + (raw − kill_max) × toll_over_permille div 1000   otherwise
dead = max(0, min(dead, P − 1))
```

`toll_over_permille` (new, shipped **250**): above the kind's `kill_max` the toll grows at a quarter of the rate, so no toll sits on a cap and a large town loses more than a hamlet. **Plunder**: `hoard_permille` becomes **0** for `undead` and `wyvern` (data: the dead and the broods take lives, not goods), so their raids carry `plunder` 0 and their `hoard` stays 0.

### 9.3 Hunts called sooner (amends P5-02 6.4.2)

R_c = the number of m's raids on civilization c with Y − year ≤ `hunt_memory_years` (P5-02's 30; ≥ 1 for every c in the hunt loop). Note the two windows differ at their edge, as built: the hunt memory includes a raid exactly `hunt_memory_years` old, the raid chain (9.1) and the fear count (9.4) exclude one exactly `raid_chain_years` or `fear_years` old. The chance of a hunt becomes

```
p_hunt = min(hunt_max_permille, hunt_permille + (R_c − 1) × hunt_raid_permille)
```

with `hunt_raid_permille` (new, shipped **5**, r1 60) and `hunt_max_permille` (new, shipped **200**, r1 400): a realm struck once hunts at 20‰ a year, struck four times in thirty years at 35‰; the cap is reached at 37 remembered raids. Odds, outcomes and causes as P5-02. Measured (21 F1b): the median from a monster's first raid on a realm to its slaying or driving off by that realm is 27 years. The monster keeps per civilization the years of its raids within `hunt_memory_years` (P5-02 25 item 11's one entry per civilization gains this list; entries older than the memory are dropped when the list is read).

### 9.4 Flight for fear (new; amends P5-02 6.4.3)

After a raid on t and its overrun test (P5-02 6.4.3), if t is still standing: F_t = the number of m's raids on t with year > Y − `fear_years`, this raid included. If F_t ≥ `fear_raids` (shipped 2), kind(t) is hamlet or village, t is not its owner's capital, its owner c has another standing settlement (those created earlier in the step count), and `chance_pm(D(hist.monsters.fear; m, t, 0), fear_permille)`:

1. append **`settlement_fled`** (region and settlement t; participants m `monster`, c `civilization`; causes: m's raids on t within `fear_years`, at most the last 4, ascending);
2. `PeoplesSubsystem::abandon(ctx, t, {that event})`: P3-05 8.5's abandonment, made public (items 1 to 6 with the given causes in place of the blow's, plus t's decline start when it is declining, as 8.5 says; item 6 cannot apply). Its event (kind 31 or 16, with 11.1's rules) follows the flight, and its ending is 6 `fled` (11.3).

New fields of `history.monsters`, appended after `prey_might_max_permille` in this order: `raid_chain_years` int(1, 100000) **300** (r1: 100); `toll_over_permille` int(0, 1000) 250; `hunt_raid_permille` int(0, 1000) **5** (r1: 60); `hunt_max_permille` int(0, 1000) **200** (r1: 400); `fear_raids` int(1, 100) **2** (r1: 3); `fear_years` int(1, 100000) 50; `fear_permille` int(0, 1000) **400** (r1: 300); `overrun_people_ref` int(1, 1000000) 800 (14.1). Shipped (calibrated) values at 0.24.0; r1's in brackets where they moved. Check `monsters_hunt_ordered`: `hunt_permille` ≤ `hunt_max_permille` (`hunt_permille <a> is above hunt_max_permille <b>`). The `zero` fixture tuning of P5-02 7.5 keeps them as shipped (nothing raids at pressure 0).

**Cost against G2** (standing settlements at E, median ≥ 81). A flight needs two raids on one small place within fifty years. Measured at 0.24.0 by the engineer (21): flights median 1 a default world (max 9), standing settlements at E median 83, so G2 holds at `fear_permille` 400 (question 4 is answered).

### 9.5 Raid sentences by kind (amends P5-02 4.2, 10.2, 10.3)

Monster data gains `raid_phrases`, array(string(48, `phrase`), 1, 4), required, after `hoard_permille`. Grammar `phrase`: lower-case ASCII letters, single spaces, `'` and `-`, starting and ending with a letter. Stored in `monster_kind.raid_phrases` (16.1), joined with `|`. **Form `raid`** (8.4's `form` gains `"raid"`, for a role of entity type monster): the phrase number ((event id − 1) mod n) of the first participant's kind, as stored; `template_problem`: `form 'raid' needs a monster role at byte <n>`. Kind 48's template becomes `{monster} {monster:raid} {@settlement}; {dead?no one died|one died|# died}.` Shipped (taste):

| kind | `raid_phrases` |
| --- | --- |
| `dragon` | `fell in fire upon`, `burned the fields of`, `came down in flame on` |
| `giant` | `came down from the heights upon`, `hurled stones upon`, `fell upon` |
| `ogre` | `fell upon`, `plundered the stores of`, `carried off the herds of` |
| `troll` | `came down by night on`, `fell upon`, `prowled the edges of` |
| `undead` | `came out of the barrows against`, `walked by night in`, `crept into` |
| `wyvern` | `stooped on`, `swept down on`, `came screaming down on` |

Sample: `The Brood of Skarr stooped on Nub Pit; 12 died.`

## 10. F2 Monster stories in the summary

### 10.1 The monster floor (amends P3-07 24.3 items 2 and 3)

**harm(m)** = Σ `dead` of m's `monster_raid` events (negative values as 0, saturating, P5-02 25 item 17) + `SUMMARY_MONSTER_TAKEN_HARM` × (the number of m's `settlement_overrun` events + the number of `settlement_fled` events naming m). The **notable** monsters: harm ≥ `SUMMARY_MONSTER_HARM_MIN`; the **told** monsters: the first `SUMMARY_MONSTER_STORIES` of the notable by (harm descending, id ascending). The **story** of a told monster: its `born_event`; its raid of the largest `dead` (then smaller id), if it raided; its `settlement_overrun`, if any; its `death_event`, if it ended. At most four events each.

The floor set of 24.3 item 3 starts as F ∪ the stories (in place of F alone); the age walks, the threshold, the 12.9 budget B and every other rule are unchanged. A story event is in the summary set always, outside the threshold and whatever its salience (so a raid line can be shown), and counts toward the age of its own year; it brings its own unit (24.2) only as 24.2 defines one (a raid, an appearance, a rising, a slaying, a death have none; an overrun none). New constants of section 8: `SUMMARY_MONSTER_STORIES` = **3**, `SUMMARY_MONSTER_HARM_MIN` = **200**, `SUMMARY_MONSTER_TAKEN_HARM` = **500**.

The floor grows by at most 12 events; B ≥ 100, so the bound of 12.9 holds as before. Where the budget binds (`civs4`, threshold 71 to 76) the stories displace other events and add no line; where it does not (most default worlds) they add up to 12 chronicle lines. The critic's default s1 (threshold 56): the Restless of Stead (19 raids, 1,465 killed, an overrun) is told with its rising, its worst raid and the overrun.

### 10.2 Ended monsters in the summary `[monsters]` (amends P5-02 12.4)

At summary, after the living monsters (at most `SUMMARY_MONSTER_LINES`, 2, by hoard as before), the **ended** notable monsters (status 3 or 4, harm ≥ `SUMMARY_MONSTER_HARM_MIN`), by (harm descending, id), at most `SUMMARY_MONSTER_ENDED_LINES` (**2**, new constant), each as the full form's line. Then `  and <x> more` with x = the monsters not listed (every status), when x > 0. When the world has more than `SUMMARY_LINES_CIVS` (16) civilizations, `[monsters]` stays its heading and count line (P5-02 23 item 4). Full is unchanged. The meaning of `and <x> more` changes (it counted living monsters only), hence part of format v5 (17.1).

## 11. F3 Abandonment and ruins

### 11.1 No destination out of reach; losses on the road (amends P3-05 8.5 items 1 and 2, 12.1)

- **Destination within reach.** In item 1, t = the nearest of c's other standing settlements **with `hops(region(s))[region(t)]` ≤ `abandon_reach_hops`** (squared cell distance, then id); none: settled = 0 and kind 16 (`settlement_abandoned`) is written, its survivors scattered. The ruin procedure (sack, overrun) uses the same t, so a sacked town's survivors with no settlement of their realm within reach are displaced (7.2) as well.
- **Losses on the road** (abandonment only, not a sack or an overrun, whose tolls are their own): with P the population at the abandonment, road = P × (`abandon_loss_min_permille` + pick(D(hist.peoples.abandon_loss; s, 0, 0), `abandon_loss_max_permille` − `abandon_loss_min_permille` + 1)) div 1000, and road = min(road, P − 1) when P ≥ 1. The survivors are P − road; `set_population(s, P − road)` before the ruin. Kind 31: settled = min(P − road, room), lost = P − settled (the dead of the road and those with no room). Kind 16: `survivors` = P − road. The people with no room are displaced (7.2); the dead of the road are not.

- **The plague plan** (P3-05 10.2, amended): the replayed plan of an outbreak's spreads applies the same reach and the same road losses to a spread that abandons its target (its survivors less `road` go to the nearest standing settlement of the owner within `abandon_reach_hops`, up to its room), so the planned total equals the real toll of the strikes (P3-05 invariant 13).

New `history.peoples` fields, appended after `ghost_max_population` in this order: `abandon_reach_hops` int(0, 16) **0** (r1: 2; calibrated: the destination must lie in the settlement's own region); `abandon_loss_min_permille` int(0, 1000) **50**; `abandon_loss_max_permille` int(0, 1000) **200**; `ghost_fade_years` int(0, 100000) **0** (11.2). Check `peoples_abandon_loss_ordered`: min ≤ max (`abandon_loss_min_permille <a> is above abandon_loss_max_permille <b>`). The I0 fixture of P5-02 7.5 and this spec's 7.4 keep them as shipped (they change worlds on both sides of the identity alike).

Expected: every abandonment of 50 people or more loses some on the road (the critic's 308 of 309 with `lost` = 0 becomes none). With `abandon_reach_hops` 0 an abandonment has a destination only when its realm has another standing settlement in the same region; the people of the others are displaced (7.2) and may find shelter abroad (6.4.3). The share of kind 16 is F3a's (21), measured by validation. Standing settlements and world population at E: G2 (21).

### 11.2 Ruins that do not fade (amends P3-05 8.6)

**How fading works now.** P3-05 8.6: in Q3, a ruined settlement (status 3) with Y − `ruined_year` ≥ `ruin_fade_years` (1500) becomes faded (status 4) by `ruin_faded`. A faded settlement no longer blocks a site (P3-05 6.2), drops out of `[present]`'s ruins (P3-07 25.3 counts it apart), and P4-04 draws it as a faint marker; its row stays in `settlement` with its cell, and its ruin event stays readable through `ruin_faded`'s cause (P5-02 25 item 3). **What P6 needs**: a place, its kind and how it ended; every settlement row, faded or not, keeps that, so P6 can derive "ancient foundations" from faded rows; what fading costs a player is the ruin's presence in the present-day lists and on the map.

**Rule.** In 8.6 a ruined settlement fades only when, in addition: (a) its `ending` (P5-02 8.3) is 1 (`sacked`) or `ghost_fade_years` > 0 and Y − `ruined_year` ≥ `ghost_fade_years`; and (b) no open site (P5-02 `site`, any kind) has it as its `settlement`. With the shipped `ghost_fade_years` 0, **a ruin left standing (overrun, plague, famine, left, fled: endings 2 to 6) never fades**, and **a ruin holding an open lair never fades while the lair is open** (from the year it closes, the 1500 years count from `ruined_year` as before, so a sacked ruin whose host was slain long ago fades at the next Q3). Burned ruins fade as before.

**Effect** (estimated from the critic's figures): the ghost places and monster-held ruins of the first four ages stay ruins at E, about 1 to 3 a default world (P5-02's G1 median of 3 ghost places a world, about half older than 1,500 years at E); present-day ruins older than year 4000 appear in most worlds. **Markers** (P4-04): the total count of settlement markers is unchanged by this rule; about 1 to 3 a world move from faint (faded) to full (ruin); `p4-04-markers.txt` is re-recorded (with every world change). Site spacing: an unfaded ruin keeps blocking its 32-cell circle (P3-05 6.2), a few cells of thousands; no measurable effect on foundings is expected (22 step 10 measures G2).

### 11.3 The flight for fear as an ending (amends P5-02 8.3, 11.4)

Kind 56, appended before part A's kinds:

| id | key | caused | region | settlement | roles | values |
| --- | --- | --- | --- | --- | --- | --- |
| 56 | `settlement_fled` | 1 | 1 | 1 | 1 `monster` monster 1–1; 2 `civilization` civilization 1–1 | — |

Template and salience (`history.report`): `settlement_fled`, **45**, `For fear of {monster}, the people of {@settlement} left their homes.` (summary: yes). **Companion** (amends P3-07 24.2 part 2): a `settlement_fled` brings the abandonment it caused (the event of kind 16 or 31 with it among its causes). Unit size stays ≤ 8.

`settlement.ending` gains code **6 `fled`**: k is `settlement_abandoned` or `settlement_abandoned_to` and a cause has kind `settlement_fled`; it is tested before codes 3 and 4 (a place fled for fear after a plague is `fled`). The reader's range becomes 0 to 6 (`ending <k> outside 0 to 6` is unchanged in form). `[present]`'s ruin words (P5-02 12.3) gain ` for fear of <monster ref>` after `abandoned` for ending 6 (the monster of the cause). `[stats]`'s `settlements_left` counts endings 3 to 6.

## 12. F4 Names

### 12.1 Epithets (amends P3-02 4.2 for kind `monster`, P5-02 4.2)

The **epithet** of a name form is the form with `{root}` removed and outer spaces trimmed (`{root} the Glutton` → `the Glutton`; `Old {root}` → `Old`; `{root}` → empty). For a name attempt of kind `monster` (P5-02 9.1), F' = the kind's `name_forms` without every form whose epithet is non-empty and is the epithet of an earlier monster's name in the world (the form its root was matched by, P5-02 9.2), in list order; F' empty: F' = the bare form if the kind has one, else F. The attempt draws its form from F' in place of F. So no epithet repeats in a world while the kind has an unused one. The epithets used are compared across **every wild monster of the world**, whatever its kind (one list for the world, as built): `{root} the Grey` used by a giant excludes `{root} the Grey` for a troll. Hosts of the dead (12.2) have no epithet and take no part.

Shipped `name_forms` (taste; `monster_bare_form_last` holds):

| kind | `name_forms` |
| --- | --- |
| `dragon` | `{root} the Black`, `{root} the Pale`, `{root} the Red`, `{root} the Old`, `{root} Ashwing`, `{root} the Gold-Eyed`, `{root}` |
| `giant` | `{root} Stonehand`, `{root} the Tall`, `{root} Cliffbreaker`, `{root} the Grey`, `{root}` |
| `ogre` | `{root} the Glutton`, `{root} the Gaunt`, `{root} the Hungering`, `{root} Bonebreaker`, `{root} the Fat`, `{root}` |
| `troll` | `Old {root}`, `{root} the Grey`, `{root} Stonehide`, `{root} of the Fells`, `Mossback {root}`, `{root}` |
| `wyvern` | `The {root} Brood`, `The Brood of {root}`, `The Screamers of {root}` |
| `undead` | 12.2 |

The engineer checks every new form word against the placename `forbidden` and `excluded` lists and the name filter (a failing word is a data defect to report).

### 12.2 Hosts named for where they rose (amends P5-02 4.2, 6.3.2, 9.1, 9.2)

A kind with `wake` = `dead` has name forms with the slot **`{place}`** (exactly once, no `{root}`); `{place}` is the name of the settlement at whose ruin the host rises (P5-02 6.3.2's lair at a ruin), else the region's name in force at Y (`RenamingSubsystem::name_now(region)`: its last `region_name` row, else `region.name`; a const read, set on the monsters subsystem by the stage), with a leading `The ` written `the `. The name: F = the kind's forms, start = pick(D(hist.names.monster; m, 0, 0), |F|); the first form in cyclic order from start whose name is valid (P3-02 4.4's attempt length, at most 48 bytes, `NAME_ATTEMPT_MAX_BYTES`) and not in the registry; none: the start form's name followed by ` ` and roman(k), the smallest k ≥ 2 that is free, which may be up to 64 bytes (`NAME_MAX_BYTES`; `names: a name longer than 64 bytes`, fatal, above). No syllable is drawn and the name filter is not applied (the parts are names that passed it). The host's **root** (P5-02 9.2) is the `{place}` value, which its lair names use. `monster_root` (P5-12's seam) returns the empty string for such a host, so renaming's forms with `{name}` or `{stem}` are skipped for it (P5-02 25 item 8) and an overrun by a host renames only by `The Forsaken {noun}`.

Shipped undead: `name_forms` = `The Host of {place}`, `The Restless of {place}`, `The Dead of {place}`; `lair_forms` = `The Barrows of {root}`, `The Howes of {root}`. Check `monster_place_forms` (with the kind's loader): a `dead` kind's form without `{place}` or with `{root}`: `name_forms[<i>] '<f>' of a dead kind needs {place} and no {root}`; a `wild` kind's form with `{place}`: `name_forms[<i>] '<f>' holds {place}, which only a dead kind may`. The `name_form` string type accepts `{place}` (P3-02 4's grammar, for monster forms only). Samples: `the dead of Wolf's Brows rose as the Restless of Wolf's Brows`; at a ruin: `the Host of Geackham`.

### 12.3 The filter and rule 7

- `game/data/name_filters/words.toml`: `bird` added to `whole`. The engineer lists every monster root of the calibration sweep that is a word of the system English word list (P3-02 11.11's method) for the critic; words that read as a joke for a monster are added the same way.
- Renaming rule 7 `ravage` (P5-02 9.3): the form `The Hollow of {name}` is replaced by `{name}'s Waste`, so a region is no longer "the Hollow of" a troll whose lair is "its Hollow". Forms: `The Haunt of {name}`, `{name}'s Shadow`, `The Forsaken {noun}`, `{name}'s Waste`.

## 13. F5 Beasts that grow bold (amends P5-02 4.1, 6.2.3, 10.2)

- **Which beast.** b = `weighted(D(hist.beasts.bold_kind; r, 0, 0), [B(r, s) × menace_s for s ascending])` in place of the largest; a species of weight 0 (deer, menace 0) is never chosen. With the shipped data a forest region's bold beasts are wolves about four times in ten, bears and boar the rest; a marsh's spiders often.
- **Phrases.** Beast data gains `bold_phrases`, array(string(48, `phrase`), 1, 4), required, after `food`; stored in `beast.bold_phrases` joined with `|` (16.1). Placeholder **`{@beast:bold}`** (8.4's body gains it): phrase number ((event id − 1) mod n) of the `beast` row named by the event's value `beast`; `template_problem` as `{@beast}`'s, naming the placeholder used: `{@beast:bold} needs a kind with a value 'beast'` (and `{@beast} …` for the plain one). Kind 53's template becomes `In {@region} the {@beast} {@beast:bold} {settlement}; {dead?no one|one|#} died.` Shipped: bear `broke into the stores of`, `came down on the herds of`; boar `trampled the fields of`, `tore up the gardens of`; deer `strayed into the fields of`; giant spider `spun their webs across the paths of`, `lay in wait on the roads from`; wolf `hunted the roads from`, `grew bold and stalked the herds of`.

## 14. F8, F7 and F6

### 14.1 F8 Overruns take small places (amends P5-02 6.4.3)

The overrun's chance becomes `min(1000, overrun_permille × M div 1000) × f_P div 1000`, with M the pressure factor (14.2) and f_P = min(1000, `overrun_people_ref` × 1000 div max(1, P_t)), P_t the target's population after the raid. Shipped `overrun_people_ref` 800: a hamlet or a village of up to 800 has the kind's full chance, a village of 1,200 two thirds, a town of 3,000 a quarter. The kinds a monster may overrun (`overruns`) are unchanged, so a dragon may still take a town, rarely. Measured at 0.24.0 by the engineer (21 F8): towns are 5.3% of overruns (about a sixth at 0.23.0), overruns per world median 2.5 (3 at 0.23.0).

### 14.2 F7 What is cheap now: pressure shapes overruns and risings (amends P5-02 6.3.2, 6.4.3)

- The overrun chance is scaled by M (14.1): at `high` (1600) a giant's 900 becomes 1000, a dragon's 600 becomes 960; at `low` (500) they halve.
- The undead's threshold: a region qualifies when S(r) ≥ `undead_min_dead` × 1000 div max(1, M): 4,000 dead at `normal`, 2,500 at `high`, 8,000 at `low`. M = 0 (the `zero` tuning) gives a threshold of 4 × 10^6, and no rising happens since the chance is 0 anyway.

So a `high` world has more monster-held places and more hosts, not only more raids. **Deferred to P5-06** (F7's "two more overruns" by rule, and F6): named slayers and companies, heroes and costly hunts, a god's hand in a monster's story (P5-08), a varied "woke again" line tied to a cause. **Cheap now and done**: the hunt delay (9.3). The hunters' count stays a value; its sentence is unchanged.

### 14.3 F8's other notes

Not changed, with reasons: undead late in history (they need slaughter, which the later ages have; F7's lower threshold at `high` moves some earlier); lairs before any settlement (the land is wild then; their monsters are invisible to the reader but their lairs are sites P6 places); worlds with few sites (the flights of 9.4 and the unfaded ruins of 11.2 add places to find).

## 15. The test-side item (for the test writer)

`worldgen/tests/reference/p5_12_renaming_reference.py check-world` reports a false renaming defect on seed 5 at 1024 with `--monster-pressure high`: its `max_weight` takes the largest weight over every rule naming the event's kind, so when two rules share a cause kind (`great_sack` and `capital_sack` both name `sacked`) an event that only the lighter rule's conditions accept is checked against the heavier weight. The check must take the weight of the rules whose conditions (values, `min_magnitude`, `min_salience`, `capital`) the event meets. **The rule in force** (as built, P5-12 4): a renaming event can meet the conditions of two rules (`great_sack` and `capital_sack` on one sack of a capital); each rule whose conditions are met makes its own chance draw (`hist.renaming.chance` with k the rule number), and the candidate of the region is the weightiest rule whose conditions are met **and** whose chance draw passed (then the smaller event id). A heavier rule whose draw failed does not block a lighter one whose draw passed. A test fix only; the C++ is right. The test writer confirms the cause on that world before changing the reference.

# Common to both parts

## 16. Schema 10

### 16.1 DDL (amends P3-04 9; hard rule 7)

Two statements gain a last column, byte for byte:

```
CREATE TABLE beast (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT NULL, menace INTEGER NOT NULL, food INTEGER NOT NULL, bold_phrases TEXT NOT NULL) STRICT
CREATE TABLE monster_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, noun TEXT NOT NULL, grouped INTEGER NOT NULL, wake INTEGER NOT NULL, might INTEGER NOT NULL, raid_phrases TEXT NOT NULL) STRICT
```

Two statements are appended after `site`'s, in this order:

```
CREATE TABLE race_relation (race_a INTEGER NOT NULL, race_b INTEGER NOT NULL, year INTEGER NOT NULL, disposition INTEGER NOT NULL, band INTEGER NOT NULL, PRIMARY KEY (race_a, race_b, year)) STRICT, WITHOUT ROWID
CREATE TABLE settlement_race (settlement INTEGER NOT NULL, race INTEGER NOT NULL, year INTEGER NOT NULL, share INTEGER NOT NULL, standing INTEGER NOT NULL, PRIMARY KEY (settlement, race, year)) STRICT, WITHOUT ROWID
```

`SCHEMA_VERSION`, `user_version` and `meta.schema_version` become **10**; readers refuse 9 (`world schema version 9, this build reads 10`). The empty world gains two root pages: **45 pages, 184,320 bytes** (43 + 2; measured with the vendored SQLite, `sqlite_schema` still fits its pages; recorded in `docs/specs/schema.md`). `db::HISTORY_TABLES` stays at 13 entries; the new tables are read by name after `site`.

### 16.2 Columns, codes and row order

| Table | Rows, order | Columns |
| --- | --- | --- |
| `race_relation` | (race_a, race_b, year) ascending; 6.7's rows | race_a < race_b, both `race` ids; `disposition` D (−4000 to 4000 by the bounds); `band` 0 hatred, 1 wariness, 2 friendship. The design's end-year table (design 6: race_a, race_b, disposition) is each pair's last row (year E − 1), as P5-02's `beast_population` holds its end state (P5-00 4.3) |
| `settlement_race` | (settlement, race, year) ascending; 6.7's rows | `share` 0 to 1000 per mille (0: the race left); `standing` 0 subject, 1 tolerated, 2 accepted, 3 ruling (0 when share is 0). Design 6's `settlement_race` (settlement_id, race_id, share, standing) by year |
| `beast.bold_phrases`, `monster_kind.raid_phrases` | as before | the phrases of the data joined with `|`, in data order |

### 16.3 Report, hash and viewer

`report` (P3-04 11.1): `[tables]` gains `race_relation`, `settlement_race` in name order; `[history]` gains, after `sites`: `race_relations`, `settlement_races` (rows). `hash --detail`: one `table <name>` line each. The viewer reads schema 10 through the library; the viewer engineer is told the two tables and section 18's reads; `gdext-headless-check` is unaffected.

**Browser kind groups** (amends P4-05 4.4; lead ruling): kind 56 `settlement_fled` is in group `monsters` (5); kinds 57 to 62 are in the new group **`peoples` = 6**, appended (codes are never reused), and `ALL_GROUPS` becomes **0x7F**. `event_group_word` gives `peoples`; the viewer's group menu lists it last.

### 16.4 Checks on read (amends P5-02 11.4)

After the `site` and `monster` checks, the first failing row of each table in key order (`corrupt table '<table>': row <key>: <reason>`, code `corrupt_layer`, exit 3), reasons in this order; every check linear (index lookups, one pass):

- **`beast.bold_phrases`, `monster_kind.raid_phrases`**: `bold_phrases is not a phrase list` / `raid_phrases is not a phrase list` (1 to 4 phrases of the `phrase` grammar joined with `|`).
- **`race_relation`**: `not above the previous row`; `unknown race <r>`; `race_a <a> is not below race_b <b>`; `year <y> outside 0 to <E - 1>`; `disposition <d> outside -4000 to 4000`; `band <k> outside 0 to 2`; `equals the previous row of this pair`; for a pair's first row, `first row is not at its meeting` (a `peoples_met` of that pair in that year).
- **`settlement_race`**: `not above the previous row`; `unknown settlement <s>`; `unknown race <r>`; `year <y> outside <founded_year> to <E - 1>`; `share <x> outside 0 to 1000`; `standing <k> outside 0 to 3`; `standing <k> with share 0` (share 0 needs standing 0); `equals the previous row of this settlement and race`; per settlement, at the year of each of its rows, the shares in force (the last row per race with year ≤ that year) sum to 1000 with exactly one standing 3: `shares in force at <y> sum to <n>` / `<n> ruling races at <y>`.
- **Events**: every `peoples_met` names a pair with rows (`peoples_met without its rows`); at most one `peoples_met` per pair (`<a>-<b> met twice`); every participant of entity type 1 names a `race` row.
- **`settlement.ending`**: the derivation of 11.3 (code 6).

## 17. Report v5 (amends P3-07; new section 28)

### 17.1 Format version

The header is **`# worldgen history v5`** at both levels: 10.2 changes what an existing line means, which 25.1's growth rule does not cover. The growth rule holds for v5 unchanged. `report --help` prints `format '# worldgen history v5'`.

### 17.2 `[peoples]` (new block after `[sites]`)

```
[peoples]
pairs: <n>; met <m>; hatred <h>, wariness <w>, friendship <f>
  <race a plural> and <race b plural>: <hatred|wariness|friendship> <D>, met <year> e<id>[, turned <year> e<id>]
  <race a plural> and <race b plural>: never met
mixed: <x> settlements at E; refugees: <a> arrivals, <p> people; expelled: <e> times, <q> people
  s<id> <name> (<owner ref>): <race plural> <share> <standing word>; <race plural> <share> <standing word>[; …]
```

The counts are of the pairs of races with `race.present` = 1 (a race that never awakened in the world is in no pair), their bands at E (met pairs only), the mixed standing settlements at E and the events of kinds 59 and 62 with their `people`. **Mixed at E** (lead ruling): a settlement standing at E with an entry in force at E − 1 (its last `settlement_race` row with share > 0) whose standing is not 3 and whose share is at least the report's constant **`MIXED_SHARE_MIN` = 100** (`worldgen/history/digest.hpp`); the data field `mixed_min_permille` governs only the event of 6.3.6, so the report needs no tuning. The count is written `1 settlement` in the singular (`<x> settlements` otherwise). `turned` names the pair's last `disposition_turned`. Pair lines in ascending (race_a, race_b); settlement lines in ascending id, entries by share descending, then race id. **Full**: every pair line and every mixed settlement line. **Summary**: the two count lines, and the pair lines of the pairs with at least one turn; when the world has more than `SUMMARY_LINES_CIVS` (16) civilizations, the two count lines only.

### 17.3 `[present]`, `[monsters]`, chronicle

`[present]`'s ruins (11.3: ` for fear of <monster ref>`); `[monsters]` at summary (10.2); the summary set's floor (10.1). Nothing else.

### 17.4 `[stats]` (appended at the end of the list, full only; `SUMMARY_STATS` unchanged)

In this order: `race_relations`, `settlement_races` (rows; also `[history]` keys), `race_pairs` (pairs of races with `race.present` = 1), `race_pairs_met`, `dispositions_turned`, `pairs_hatred`, `pairs_friendship`, `refugees_arrived`, `refugees_people`, `people_expelled`, `expelled_people`, `settlements_mixed` (at E, by `MIXED_SHARE_MIN`, 17.2), `settlements_mixed_ever` (settlements with a `settlement_mixed` event), `settlements_fled`, `abandoned_without_destination` (kind 16 events), `abandonment_lost` (Σ `lost` of kind 31), `monster_stories` (told monsters, 10.1), `ruins_oldest_year` (the smallest `ruined_year` of a status-3 settlement at E, or `-` when none). `events_<key>` lines of the new kinds appear by themselves.

### 17.5 Length

At summary: `[peoples]` 2 lines plus 0 to 4 turned pairs in a default world; `[monsters]` up to 2 more lines; the chronicle up to 12 story events and about 25 to 35 new candidates of part A within the budget. Measured at 0.24.0 by the engineer (21 T18): default median 620 (0.23.0: 599.5), max 785, held by 12.9's L (790). `civs4`: `[peoples]` 2 lines, `[monsters]` unchanged (count line), chronicle unchanged in size (the budget binds): median **1,112** (0.23.0: 1,101); the lead restates the bound as at most 1,150 (21, question 5).

## 18. Timeline reads (amends P4-01; r5)

`load_history` takes the new tables from `read_history_tables`. `Timeline` gains:

```cpp
struct RaceRelationState { std::int64_t race_a, race_b; bool met; std::int64_t disposition, band; };
struct RaceShare         { std::int64_t race, share, standing; };
RaceRelationState race_relation_at(std::int64_t race_a, std::int64_t race_b, std::int64_t year) const;
std::vector<RaceShare> settlement_races(std::int64_t settlement, std::int64_t year) const;
```

- **Disposition at Y**: `met` iff the pair's `peoples_met` has year ≤ Y; then `disposition` and `band` are the row with the largest year ≤ Y (the meeting's row exists). Before the meeting, or for a pair that never met: `met` false, `disposition` and `band` 0 (the world records no disposition before peoples meet; the viewer shows "not met"). Binary search per pair, O(log k). Symmetric in its arguments (it orders them). Fatal texts for an unknown race or a year outside 0..E: those of P4-01 5.1.
- **Shares at Y**: for a settlement standing at Y (P4-01's census reading), per race the last row with year ≤ Y and share > 0, ascending race id (shares sum to 1000); empty when not standing at Y. O(rows of s).
- **Dumps** (P4-01 13.1): S gains, after the `beast` lines, `relation <a> <b> <year> <disposition> <band>` per `race_relation` row and `share <s> <race> <year> <share> <standing>` per `settlement_race` row, in key order; D(Y) gains `races <a> <b> <met> <disposition> <band>` per pair ascending and `mix <s> <race> <share> <standing>` per standing settlement and race at Y with more than one race, ascending.
- **For the viewer** (P4): a settlement's mix and the standing of each race at the shown year (hover text), and a pair's disposition by year (a table or a graph of the fifteen pairs). The colours of a mixed settlement's marker are a viewer choice, not specified here.

## 19. Invariants

1. Every pair of races has at most one `peoples_met`; rows of a pair start at its meeting; `disposition_turned` follows its meeting and changes the band by one step.
2. B(a, b) + `memory_min` ≤ D ≤ B(a, b) + `memory_max` for every row.
3. For every standing settlement at every row year: shares sum to 1000, exactly one race is ruling, and it is the race of the settlement's owner at that year.
4. A colony's first rows are its founder's race at 1000; a refuge's first rows are its origin's mix.
5. Every `refugees_arrived` host is within `migrate_hops` of the origin, owned by a realm not at war with the origin's owner at the event, with D ≥ `welcome_min`; its `people` ≤ the host's room; the host's population rises by `people` in that step.
6. Every `people_expelled` takes a share ≤ `expel_share_max` of a race in hatred with the ruling race; the settlement's population falls by `people`.
7. With the `zero` tuning, I0 (7.4) holds.
8. (Part B) No two monsters of a world share an epithet while their kind has an unused one; a host of kind `dead` has a name with its place.
9. (Part B) A raid's causes are its waking event and at most one earlier raid on the same place; a `settlement_fled` is followed in the same step by an abandonment of the same settlement citing it; a fled settlement was a hamlet or village, not a capital, not its owner's last.
10. (Part B) A ruined settlement with `ending` 2 to 6 is never faded while `ghost_fade_years` is 0; no ruin at an open site is faded.
11. P3's hard bounds (P3-00 12.3), "no race dies out", the shelter rules, and P5-02's invariants 1 to 13 as amended.

## 20. Determinism notes (for the auditor)

- Four new streams with keys of ids (3); part A draws only for expulsions. Every other choice is a total order ending in an id (hosts 6.4.3, meetings 6.2.4).
- Iteration: pairs, settlements, races, entries, pending records and the log in ascending order or record order; no unordered container; members hold ids.
- Arithmetic: int64, checked multiplication. Largest products: c_g = share × P ≤ 1000 × 2^31 < 2^41 (6.3.2); memory in milli-units within ±2 × 10^6; drift share × rate × Δ ≤ 1000 × 1000 × 12000 < 2^34. Truncation toward zero for M / 1000 is C++'s `/` on int64, specified because M can be negative.
- The ledger only records; the only `State` changes are `set_population` and `note_reduction` (6.6). The copied entries make the outcome independent of when within the step the record is resolved.
- Rendering: the phrase number ((event id − 1) mod n) depends on stored values only.

## 21. Targets (validation)

At 1024 defaults, 100 worlds (seeds 0–99), unless stated. r2 (lead rulings after calibration): the bounds below replace r1's estimates where they were restated; the column **Measured** gives the engineer's final sweep, **measured at 0.24.0 by the engineer; validation's figures supersede**.

| Target | Definition | Bound | Measured |
| --- | --- | --- | --- |
| **RR1** | pairs met by E | every pair of races that can reach each other over open borders within `meet_hops` meets; all fifteen pairs meet in at least 80 of 100 worlds (r1: 90%, median ≥ 13 of 15) | 84 of 100 |
| **RR2** | `disposition_turned` per world | median 1 to 4, max ≤ 10; at least one in ≥ 50 worlds; every band entered by a turn somewhere in the sweep (r1, unchanged) | median 1; across the sweep hatred entered 116 times, wariness 30, friendship 3 |
| **RR3** | participation (P5-00 8.3): every present race is a participant (as a race, or through one of its civilizations) of an event of a P5 subsystem | **reported, not bounded** (r1: hard); a world where a race never meets another and no P5 event names it is a race alone on its own land | 10 worlds have a race alone on its own land |
| **MX1** | mixed standing settlements at E (17.2) | median 3 to 12; at least one in ≥ 80 worlds | median 8 |
| **MX2** | `refugees_arrived` per world; people sheltered | median 3 to 15; reported | median 5 |
| **MX3** | `people_expelled` per world | median ≤ 5, the maximum reported (r1: median ≤ 3, max ≤ 10) | median 3, max 22 |
| **C1** | conflict regressions (P5-00 8.2 item 3) | wars per world median within ±10% of 0.23.0's (23.5: 21 to 26); T1 to T20 of P5-01 as bounded at 0.23.0; enmity rows per world reported against 0.23.0 | wars median 25.5 (+8.5%) |
| **T16** | repeated battles | at most 2 per thousand (Paul) | 0.93 per thousand |
| **G2** | standing settlements at E; world population at E | medians at least 81 and 214,500 (P5-02 r1, unchanged) | 83; 217,827 |
| **F1a** | the most raids by one monster on one settlement | median per world ≤ 8, max ≤ 40 (r1: max ≤ 20; 0.23.0: max 50) | median 5, max 33 |
| **F1b** | years from a monster's first raid on a realm to its slaying or driving off by that realm | median ≤ 150; reported by kind | median 27 |
| **F1c** | raid lines | every raid on a place the same monster raided within `raid_chain_years` cites that raid (by rule, 9.1), and such raids are at least 30% of raids (r1: ≥ 50% cite an earlier raid); no phrase above 40% of raid lines; the most common (dead, plunder) pair ≤ 1% of raids (0.23.0: 1.4%) | 38.7% cite; top phrase 11.8%; top pair 0.61% |
| **F1d** | `settlement_fled` per world | median 1 to 3 | median 1, max 9 |
| **F2** | told monsters | every default world with a notable monster tells at least one in its summary (by construction); T18 holds | — |
| **F3a** | abandonments with `lost` > 0 (kind 31) | ≥ 80%; kind 16 among abandonments 5% to 25% (r1's bound; with `abandon_reach_hops` 0, validation reports the figure) | — |
| **F3b** | present-day ruins older than 1,500 years at E | at least one in ≥ 50 worlds (0.23.0: none before year 4000) | — |
| **F4** | the most used epithet of a kind across the sweep | ≤ 40% of that kind's named monsters (0.23.0: `the Glutton` 68 of 90) | 37.7% |
| **F5** | `beasts_bold` by species | wolves ≤ 60%; boar ≥ 5%; bears ≥ 5% | wolves 33% |
| **F8** | overruns of towns | ≤ 10% of overruns; M4's overrun median 2 to 4 holds | towns 5.3%; overruns median 2.5 |
| **M** | P5-02's M1 to M6, G1, G3, S1, B1 | as bounded in P5-02 r1 (M5: 20% to 70% slain). M4: a living lair at E in ≥ 85 worlds; **a lair on a former settlement open at E in ≥ 65 worlds, Paul's bound: OPEN FOR PAUL** pending validation's figure | lair on a former settlement: 62 of 100 |
| **T18** | summary lines | default median ≤ 700, max ≤ 800; `civs4` median at most 1,150 (r1: "about 1,100", Paul's reading at 0.23.0), max ≤ 1,400 | default median 620, max 785; `civs4` median 1,112 |
| **timing** | `generate` at 1024, release, seed 0, three runs | at most 5 s more than 0.23.0 (P5-00 8.1); `report --history` under 2 s | validation |

**RR2 and the staticness of relations.** With the shipped camps and stances, four pairs start in hatred (good–evil, −700) and leave it only by 250 units of shelter; of the pairs that start in wariness, dwarf–goblin (−350) is 150 units from hatred, the neutral–evil pairs (−250) 250 and the good–neutral pairs (−150) 350, against memory bounds of ±500 and a decay of 20 a century; friendship needs +300 or more for every pair but elf–faerie (which starts in it). By these bounds every pair that starts in wariness can reach hatred with enough grudge, and dwarf–goblin, the only pair a stance moves toward it, needs the least; the lead's reading of the sweep is that in practice hatred is entered by a turn only by dwarf–goblin. A median of one turn a world means that dispositions mostly stay where the camps put them. **Open question for Paul after the world critic's review** (28 item 7): whether relations should move more.

**Costs against G2** (Paul's rule): measured together (G2 above): standing settlements median 83 at 0.24.0, as at 0.23.0; world population median 217,827.

Sweeps (P5-00 8.2): default 100; 20 `civs4`; `calm`, `turbulent`; 512 seeds 0–19, 2048 seeds 0–4; `--monster-pressure low` and `high` seeds 0–19; `--races` subsets (`elf,orc`, `dwarf,human`, a single race), where RR3 is read.

## 22. Order of implementation and roles

One branch, one engineer, one test writer working from the specs separately. `GENERATOR_VERSION` 0.24.0 and `SCHEMA_VERSION` 10 in step 1; goldens re-recorded once, in step 12.

| Step | What | Spec |
| --- | --- | --- |
| 1 | Schema 10: DDL, version, empty world, `schema.md`, `[tables]`, `hash --detail`; readers refuse 9 | 16.1, 16.3 |
| 2 | Data: `stances`, `history.relations`, the new `history.monsters` and `history.peoples` fields, `raid_phrases`, `bold_phrases`, monster forms and `{place}`, hoards, the filter word, rule 7's form, `history.report` entries and templates; both fixture roots | 4, 9, 11.1, 12, 13 |
| 3 | Catalogue kinds 56 to 62; forms `raid`, `{@beast:bold}`, the four-word choice | 8, 9.5, 11.3, 13 |
| 4 | Part B rules in monsters and peoples: raid causes and tolls, hunts, flight for fear and `PeoplesSubsystem::abandon`, overrun factor, pressure scaling, bold species, epithets, place names, reach and road losses, fading, ending 6 | 9 to 14 |
| 5 | Seams: `Settlement::owner_event`, the ledger in peoples and conflict, `DispositionView` in conflict, `name_now` | 7, 12.2 |
| 6 | `RelationsSubsystem`, its stream, rows, `HistoryResult` members and writers | 5, 6 |
| 7 | Reader checks; `HistoryView`; `Timeline` reads | 16.4, 18 |
| 8 | Report v5 | 10, 17 |
| 9 | I0 (7.4) and P5-02's I0 on the fixtures | 7.4 |
| 10 | Calibration (data only) against 21: part B first (G2 by `fear_permille`; F8 by `overrun_people_ref`; M4, M5 by `hunt_raid_permille`), then part A (C1 by the `*_memory` values, MX1 by `migrate_hops` and `welcome_min`, RR2 by `band_margin`); results in r2 | 21 |
| 11 | Timing | 21 |
| 12 | Goldens (23), README and CLAUDE.md (lead) | 23 |

- **Engineer**: steps 1 to 11; tell the viewer engineer the two tables and 18's reads; keep `gdext-headless-check` green.
- **Test writer**: 24, from the specs only; 15.
- **Determinism auditor**: 20, the seams of 7 and the ledger.
- **Validation**: 21 on its sweeps, the cross-preset comparison, timing.
- **World critic**: 25's reading list.

## 23. Golden values and CI

**Every world file changes**: schema 10, `GENERATOR_VERSION`, `content_hash` (new id, fields, phrases, forms, filter, report entries), and every history (part B from the first raid or abandonment, part A from the first meeting). The empty world grows (16.1).

| Golden | Re-recorded | By | What moves |
| --- | --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`; `GOLDEN_CASES`, `stages_pipeline_test.cpp`; README's phrase hash | yes | validation, after the cross-preset comparison | every case |
| `core_vectors.txt`, `p1-01` … `p1-06`, `p3-03-chronology.txt` | no | — | must not move |
| `p3-01-data.txt` | yes | test writer | content hashes |
| `p3-02-names.txt` | yes, if it hashes the filter lists or monster forms; else no | test writer | the filter word `bird` (a name that drew `Bird` changes) |
| `p3-04-engine.txt` | yes | test writer | catalogue rows 56–62, the new tables empty in stub worlds |
| `p3-05-peoples.txt` | yes | test writer | abandonment reach, road losses, fading (peoples-only configuration: no ledger, no relations) |
| `p3-06-conflict.txt`, `hist-conflict-73-1024` | yes | test writer | every history (full configuration) |
| `p3-07-report.txt` | yes | test writer | every report (v5) |
| `p4-01-timeline.txt` | yes | test writer | dumps gain relations and shares |
| `p4-04-markers.txt` | yes | test writer | ruins kept, flights, every world change |
| `p5-12-renaming.txt` | yes | test writer | every renaming (rule 7's form, worlds) |
| `p5-02-monsters.txt` | yes | test writer | part B |
| **`p5-03-relations.txt`** (new) | new | test writer, `worldgen/tests/reference/p5_03_relations_reference.py` | per world case and fixture: hashes of `race_relation`, `settlement_race`, the events of kinds 56–62, and the counts of 21's RR, MX and F rows |

**CI**: `world-hashes.sh` keeps its sixteen cases (no new setting); every case's files are re-recorded and compared across presets as before. No new CTest shard beyond 24.1's.

## 24. Tests required

### 24.1 Unit (C++; new shard `worldgen-tests-relations` for `*relations_*`; part B tests in the existing `-monsters` and `-peoples` shards)

1. Data: `stances` and its check messages; every `history.relations` field's bounds and absence, `relations_bands`; the new `history.monsters` and `history.peoples` fields, `monsters_hunt_ordered`, `peoples_abandon_loss_ordered`; `raid_phrases`, `bold_phrases` and the `phrase` grammar; `{place}` and `monster_place_forms`.
2. Dispositions: B with stances (floor of a negative half sum); memory deltas per kind and role, clamping, decay in ticks and quiet steps, truncation toward zero; bands at a meeting; each turn of the hysteresis at its boundary (h, h + m, f, f − m); causes of a turn (contributions, the fallback); the quiet budget.
3. Meetings: the key over presences, `meet_hops` at h and h + 1, −1; the region and cause.
4. Shares: 6.3.2's mixing with remainders (ties by race id), P_b = 0; a new settlement; owner change to another race (settlers, subjects, the others' targets) and to the same race; drift and fade; standing targets and their events; mixed events once per (settlement, race).
5. Ledger: `moved` and `displaced` from each caller (ruin, abandonment, capture refugees, flight, expulsion), the copies taken at the call; group splitting; host conditions one by one (war, lying low, kin order, room, `welcome_min`); arrivals and the quiet budget.
6. Expulsions: each condition, the majority guard, the population and `note_reduction`, the displaced group.
7. Conflict seam: D in `disposition_of`; enmity on D with `since_event` the later of the founding and the hatred turn; no view: 0.23.0's behaviour.
8. Part B: raid causes (with and without an earlier raid in `raid_chain_years`); the toll above `kill_max`; plunder 0; the hunt chance by R_c and its cap; the flight's conditions and its two events; the overrun factor at P = 800, 1,200, 3,000 and with M; the undead threshold by M; the bold species draw (weight 0 never chosen); epithets excluded by use and the fallback; place names at a ruin and in a renamed region, the numeral fallback; reach and road losses (kinds 16 and 31's values); fading with endings and open sites; ending 6.
9. Rendering: form `raid`, `{@beast:bold}`, the four-word choice, and their `template_problem` messages; every new template renders on a fixture event.
10. Reader: each message of 16.4 on a corrupt fixture.
11. Report v5 on fixture **H14** (`h14-v5.toml`: three races, a met pair turned to hatred, an unmet pair, a mixed settlement of three races, an arrival, an expulsion; a monster told by the floor; a fled settlement), both levels; the floor's selection and `[monsters]`' ended lines; `[stats]`.
12. Timeline: `race_relation_at` before, at and after a meeting and a turn; `settlement_races` around an arrival, a capture and a drift.
13. The test-side fix of 15.

### 24.2 Property and identity

- **I0** (7.4) on the fixture worlds and seeds 0–4 at 512 (default suite) and 0–4 at 1024 (`*-slow`); P5-02's I0 still holds with part B.
- Invariants 1 to 11 on seeds 0–9 at 512 (default) and 0–19 at 1024 (`*-slow`), and on `--monster-pressure high` seeds 0–4.
- Resume (P3-04 16.3): a world resumed from `--until chronology` equals the generated one.

### 24.3 Reference and fixtures

`p5_03_relations_reference.py` on `p5_02_monsters_reference.py` (which gains part B) inside the conflict reference's replay: sections 6 and 7 (the ledger at the callers' points, the disposition in conflict's strain and enmity), with a `check-world` mode for invariants 1 to 10 and 16.4's checks. The schema gate of the references becomes 10. A hand-made root **`relations-small`** (the conflict stress data with large `*_memory` values, `band_margin` 0, `expel_permille` 1000, `welcome_min` −2000 and `abandon_reach_hops` 0) makes every kind of part A happen on small maps; `monsters-small` (P5-02) gains `fear_raids` 2 and `fear_permille` 1000 so that flights happen. **Fixtures of an earlier schema** (as built): the report fixture loader (`worldgen/tests/history_report_fixture.hpp`) gives a `beast` row without the `bold_phrases` column the phrase `grew bold near` and a `monster_kind` row without `raid_phrases` the phrase `fell upon`, so the v4 fixtures load unchanged.

## 25. For P6, and for the world critic

**For P6** (P5-00 10.2). Without replaying the simulation: each settlement's mix and standings at E (`settlement_race`'s last rows) and the pair dispositions at E (`race_relation`'s last rows) are design 9's starting point for an NPC's disposition ("starts from the relation between their race and the player's, as history left it", "adjusted by the settlement's racial mix"); the events of kinds 59 to 62 are the reasons an NPC can give ("we took the Dwarves in after the sack of Krax"). Quest hooks: a people expelled with no host (a displaced group lost: no row, but its expulsion event), a subject majority under hated rulers. Part B: fled places (ending 6) and kept ruins are places to explore; a monster's story events are its lore.

**For the world critic** (0.24.0 sweep; default seeds 0, 1, 17, 42, 84; `civs4` 1; `high` seed 3; `--races dwarf,human` seed 0): whether the meetings, turns and arrivals read as a history of peoples; whether mixed towns and their standings make sense after conquest and flight; the expulsion lines; whether dispositions at E follow the history (the elves and goblins after their wars; humans and dwarves); and for part B: raid chains and the hunts that end them, the flights for fear, the summary's monster stories, the new epithets and host names, the bold beasts, and the old ruins in `[present]`.

## 26. Amendments to earlier specs

| Spec | Section | What | Revision note |
| --- | --- | --- | --- |
| P3-01 | race fields; kind `history` ids | `stances`; id `relations` | next revision, pointing here |
| P3-02 | 4, 4.2, 4.5 | `{place}` in monster forms; epithet exclusion | next revision |
| P3-04 | 6.1, 6.4, 8.2, 8.4, 8.5, 9, 21 | `Settlement::owner_event`; the configuration; kinds 56–62; form `raid`, `{@beast:bold}`, the four-word choice; a sentence's first lower-case letter capitalised (8.2 here; supersedes 21's "never capitalized" at byte 0); `StepContext::event_participants`; schema 10 | r23 (pointer section added in this round) |
| P3-05 | 8.5, 8.6, 10.2, 12.1 | reach, road losses, `abandon` public, the ledger calls; the plague plan's reach and road losses; fading; fields | r19 |
| P3-06 | 4.1, 4.3, 7.5, 20.1, 22.5 | dispositions from the view; enmity; the ledger calls (capture, cession, flight) | r23 |
| P3-07 | new 28; 24.2, 24.3 | v5, `[peoples]`, the floor, `[monsters]`, the fled companion, `[stats]` | r23 |
| P4-01 | 5, 13.1 | race relations and shares by year, dumps | r5 |
| P4-05 | 4.4 | kind 56 in `monsters`; group `peoples` = 6 for kinds 57–62; `ALL_GROUPS` 0x7F (16.3) | r3 (applied in this round) |
| P5-02 | 4.1, 4.2, 4.3, 6.2.3, 6.3.2, 6.4, 8.3, 9, 10.2, 11, 12.4 | part B | r3 pointer |
| P5-12 | 4 | rule 7's form; an empty monster root | pointer |
| `schema.md` | — | schema 10, catalogue rows 56–62 | the engineer |

Each amended spec gets a revision note pointing to this spec's section; the normative text stays here.

## 27. Left for later

- Trade as a path into mixed settlements and dispositions moved by trade and alliances (P5-04, through `add_people`, 7.5); tribute in goods to a monster, and a hoard taken by a slayer (P5-04).
- A tribute of people paid as captives into the winner's settlements (a further mixing path; P3-06 20.3 keeps it as casualties).
- Factions of a subject people rising against its rulers, a mixed town seceding (P5-10).
- Refugee peoples founding a settlement of their own in free land, beyond P3's flight of a last refuge (25 reading; a later stage if Paul wants it).
- Figures: named hunters, heroes, companies (P5-06); gods in monster stories (P5-08); F6's sentences.
- Dispositions by culture (sub-peoples, P5-00 9.3).

## 28. Open questions for Paul

1. **Do realms read their peoples' disposition?** It is what makes "old wars leave grudges that shape later choices" reach new realms, and it moves P3's approved war balance. *Recommendation*: yes, with the memory values shipped and C1 as the check (wars within ±10%; r2: the calibrated values of 4.2 give +8.5%). *Alternative*: dispositions recorded and reported only in this stage, read by conflict from P5-04 with trade.
2. **Expulsions.** A ruling people in hatred with a minority may drive it out (at most half a settlement, by the majority guard). *Recommendation*: keep; it is the darkest outcome of standing and the strongest reason a refugee can give. *Alternative*: standing only, no expulsion.
3. **Ruins left standing never fade.** *Recommendation*: never (`ghost_fade_years` 0), and a ruin with an open lair never while the lair is open; burned ruins fade as before. *Alternative*: a long fade (4,000 years), so the first age's ghost places still go.
4. **The answer to repeated raids.** *Recommendation*: hunts called sooner and flight for fear (9); tribute to a monster in P5-04. (r2: G2 holds at `fear_permille` 400, median 83; no choice is needed.)
5. **The `civs4` summary length.** Measured median 1,112 against Paul's "about 1,100" (0.23.0: 1,101). *Lead's ruling* (r2): the bound is restated as at most 1,150 (21 T18). *Alternative*: no `[peoples]` lines at summary in worlds of more than 16 realms.
6. **One stage or two.** *Recommendation*: one stage (0). *Alternative*: P5-02a (part B) first, then P5-03 (part A), as marked in 0. (r2: built as one stage.)
7. **Relations that barely move** (r2; after the world critic's review). With the calibrated memory a world has a median of one turn; most pairs keep the band their camps give them (21 RR2). *Question*: should dispositions move more (larger memory values or bounds, a smaller `band_margin`, stances closer to the bands), or is a world in which old camps decide most of the peoples' feelings the intended reading of design 5? The interim choice is the shipped values.
8. **M4, a lair on a former settlement open at E** (r2). Paul's bound is 65 of 100 worlds; the engineer measured 62. *Open for Paul* pending validation's figure: keep the bound (and calibrate further), or lower it.

## 29. Proposals

Three entries in `docs/design/proposals.md`: "History: race relations, mixed settlements and migrations, P5-03 (readings and scope)", "Schema 10: race relations, settlement races and phrases (P5-03)" (hard rule 7) and "History: P5-02's follow-ups in P5-03 (readings)".

## Revision notes

- r1 (2026-11-23): first draft, with the P5-02 world critic's findings and Paul's standing decisions worked in.
- **r2** (2026-11-24), text only, from what was built, calibrated and ruled on `p5/03-int` (generator 0.24.0, schema 10, report v5); no change to code or output. 4.2, 9.1, 9.3, 9.4, 11.1: every tuning value set to the shipped (calibrated) value, r1's in brackets where it moved (relations `memory_*` ±500, decay 20, the five `*_memory` values tripled, `meet_hops` 8, `expel_permille` 3; monsters `raid_chain_years` 300, `hunt_raid_permille` 5, `hunt_max_permille` 200, `fear_raids` 2, `fear_permille` 400; peoples `abandon_reach_hops` 0), and the estimates they made stale removed or replaced by measured figures (9.3, 9.4, 11.1, 14.1, 8.2, 17.5). 6.1, 6.7: `settlement_race` rows at the end of every step, a quiet step's at its start Y, none at `finish`. 6.2.2: decay 200 milli-units a tick year; `StepContext::event_participants`. 6.2.4: P3-06 5.1's simple presence. 6.3.6: `mixed_told` per entry. 6.4.1: the ledger signatures with `state` and `cause`; a move's new entries date from its cause. 6.4.2: a pending move into a settlement no longer standing is skipped. 7.1: an enmity row's `since` is the year it opens. 7.2: cession refugees through the ledger with the cession as cause; the flight's cause. 8.2: a rendered sentence's first lower-case letter is capitalised (amends P3-04 r23). 9.3: the hunt memory counts Y − year ≤ `hunt_memory_years`; the raid chain Y − year < `raid_chain_years`. 11.1: the plague plan applies reach and road losses (P3-05 10.2). 12.1: epithets across every wild monster of the world; 12.2: name lengths 48 and 64. 13: `{@beast:bold}`'s `template_problem` names it. 15: the rule in force. 16.1: the empty world measured, 45 pages, 184,320 bytes. 16.3: browser groups (amends P4-05 4.4). 17.2, 17.4: `MIXED_SHARE_MIN` = 100, `1 settlement`, pairs of present races. 21: targets restated by lead ruling with the engineer's measured figures (RR1, RR3, F1a, F1c, MX3, T18 `civs4`), M4's lair bound open for Paul. 24.3: the fixture loader's default phrases. 26: P3-04, P3-05, P3-06 and P4-05 rows. 28: questions 4 and 5 answered, 7 (static relations) and 8 (M4) added.
