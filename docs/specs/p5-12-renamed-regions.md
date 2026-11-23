# P5-12 Regions renamed by history

Status: **draft r0, 2026-11-22, for the lead's approval; r2, 2026-11-23: corrections after the implementation and integration on `p5/12-int` (the lead's rulings; text only, no rule or data change; see the revision notes).** Written by the spec writer on `p5/12-spec`, cut from `origin/p5/01-final` at 0b78b31 (`main` plus P5-01: generator 0.21.0, schema 7, report format v2). Phase P5 History depth, stage 12 (`docs/specs/p5-00-history-depth-overview.md` 3.1 "P5-12 Regions renamed by history", 3.2, 4, 6.1, 7, 8). Generator **0.22.0**; **schema 8**; history report format **v3**.

Sources: Paul's decision of 2026-11-22, "regions renamed by history" (`docs/design/proposals.md`, decision 1: "a region's defining event (a great sack, a long war, a founding, a plague) can give it its present-day name, with the old name kept in the record"); Paul's decision of 2026-11-22 on the P5 overview's questions (item 3: renamed regions right after P5-01; the kinds that can rename are data); Paul's decision of 2026-11-22 "the P5-01 critic's three notes go to P5-12" (`docs/reviews/p5-01-followups-world-critic.md` F1 to F3); the P5-01 audit's advisory A1 (`docs/reviews/p5-01-followups-audit.md`); P5-01 section 12 (`Tarden`). Design doc sections 4 (regions and their names), 5 (history), 6 (world database), 12 (open question "history renames regions"), 14 (records a model can narrate).

The stage adds **one subsystem** (P5-00 4.1), `RenamingSubsystem`, and carries four follow-ups. It is a stage spec for the subsystem (sections 3 to 9 are normative for it) and a change spec for the rest, in the form of `docs/specs/p5-01-followups.md`: each amended spec holds its own normative text in a new bounded section with a revision note (table in section 2).

## 1. Purpose

At 0.21.0 a region keeps the name geography drew for it before the first awakening, so the land a realm died in reads exactly as it did 5,000 years earlier, and the largest, most fought-over regions keep their plain compass names (the P3-08 critic's observation behind Paul's decision). P5-12 lets history give a region its **present-day name** from its **defining event**: the fall of a realm, the sack of a capital or a great city, a slaughter of armies fought over it, a plague that emptied it, the rise of a realm's seat. The new name is built from the deed and the people or place it concerns (`Edadbury's Fall`, `The Grave of Cear`, `The Red Wood`, `The Heart of Nife`); the old names stay on record (table `region_name`), and every sentence names a region as it was called in the event's year. Renamed regions are rare (about one region in twelve at 1024, at most one in seven) and each traces to an event, so they read weightier than drawn names.

Carried with it:
- **F1** settlement names that double a place suffix (`Sudsteadstead`, `Galwickwick`, `Stonton`) or join `s` to `stead` (`Deasstead`): a validity rule of the name builder (P3-02 r19 16);
- **F2** the name filter covers region names, drawn and renamed (`Tarden`, default seed 84): a validity rule of P1-06 (r11 17);
- **F3** `[present]` lists only the refuges still hidden at E (P3-07 r21 26.3);
- **A1** one bounds-checked helper for the two settlement-indexed lookups of `history_report.cpp` (P3-07 r21 26.7).

## 2. Items and where their normative text is

| # | Item | Resolution, in short | Normative text | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | The renaming subsystem | rules in data (`history.renaming`), one subsystem in T6 and Q4, kind 43 `region_renamed`, table `region_name` | this spec 3 to 9; `docs/specs/p3-01-history-data-settings.md` r16 5.4 | yes |
| 2 | Names at an event's year | `{@region}` and every region name in a sentence is the name in force at the event; new placeholder `{@region_former}`; `StepContext` reads of the log | `docs/specs/p3-04-simulation-engine.md` r21 23 | sentences; schema |
| 3 | Schema 8 | table `region_name` | P3-04 r21 23.4; `docs/specs/schema.md` | yes |
| 4 | F1 doubled place suffixes | a root may not end in a fused suffix of its kind (or its first syllable), nor in the suffix's first letter; human `forbidden` gains `ass` | `docs/specs/p3-02-names.md` r19 16 | yes (names) |
| 5 | F2 the filter on region names | V8: a filter entry that spans a join of a region name's parts rejects the attempt; drawn, fallback and renamed names | `docs/specs/p1-06-regions.md` r11 17 | yes (4 region names in 100 worlds) |
| 6 | F3 hidden refuges | `hidden refuges: <h> of <n> founded by flight`, listing only those hidden at E | `docs/specs/p3-07-history-report.md` r21 26.3 | no (report) |
| 7 | Report v3 | header v3; renamed regions in `[present]`; region references outside sentences by the present name; renamings as companions; `[stats]` keys | P3-07 r21 26 | no (report) |
| 8 | A1 | `settlement_row(span, id)`, null out of range, two call sites | P3-07 r21 26.7 | no |
| 9 | Timeline and viewer | `Timeline::region_name(region, year)`; the viewer's hover text by the shown year | `docs/specs/p4-01-timeline.md` r3 16; `docs/specs/p4-02-layer-catalogue.md` r3 note | no |

## 3. Inputs

- **The event log so far**, read through `StepContext` (P3-04 r21 23.3): kind key, year, region, settlement, salience, values by key, participants by role, of every event with an id up to the last one appended.
- **Live state**: a settlement's owner and a civilization's capital and culture (`State`), for the `capital` condition and the namer's culture.
- **Region rows** (`HistoryInputs`): for the land noun `{noun}`, computed exactly as P1-06 RG11 3 (rules a to d) and 15.7 item 1 give it, from the stored rows (landmass cells summed from `region.cells` per `landmass`), the biome definitions' `region_noun`, and the `worldgen.regions` tuning (`names.isle`, `names.upland`, `upland_own_noun`). The engineer exposes the regions stage's land-noun function as a pure function of those inputs and calls it from both stages (H8: no value held in memory from the geography stages). (r2) From stored rows the `landmass` labels are renumbered 1, 2, …, k in ascending label order before RG11 3 is applied (a generated world's labels are already 1 to k; ascending order keeps rule a's tie to the smaller label); a region whose `biome` has no biome definition (a hand-made row) has an **empty** noun, whatever rules a to d give. The synthetic geographies of the tests (C++ `p3_05_peoples_support.hpp` `inputs_of`, and the references) carry `landmass` 1 for every region and, as `biome`, the dominant dry biome of the region's `region_biome` rows (most cells, ties to the smaller code); no peoples or conflict rule reads those two columns.
- **The culture data** (`culture_naming.civilization_forms`) for a realm's root (5.3).
- **The name registry** of the history stage (P3-02 4.4) and **the filter** (P3-02 11.2), with the placename lists' `forbidden` and `excluded` (P1-06 4.1).
- **Data**: tuning id `history.renaming` (section 4).
- **Streams** (P5-00 4.2), appended to `streams.hpp` after `hist.conflict.toll`, in this order: `hist.renaming.chance`, `hist.renaming.name`.

## 4. Data: `history.renaming`

A new fixed id of kind `history` (P3-01 r16 5.4 holds the field table and checks; summary here). Shipped in `game/data/history/renaming.toml` and both fixture roots.

| Field | Type | Shipped | Meaning |
| --- | --- | --- | --- |
| `region_max` | int(0, 16) | 2 | most renamings of one region in a history; 0 turns the subsystem off |
| `region_gap_years` | int(1, 100000) | 300 | least years between two renamings of one region |
| `world_max_permille` | int(0, 1000) | 150 | most renamings in a world: max(1, R × this div 1000), R = region rows |
| `century_max_permille` | int(0, 1000) | 50 | most renamings in any `century_years`: max(1, R × this div 1000) |
| `century_years` | int(1, 100000) | 100 | the window of the line above |
| `rules` | array(record `rename_rule`, 0, 64) | six (4.1) | what can rename a region; order = rule number 1, 2, … |

Record `rename_rule`, all fields required, in this order:

| Field | Type | Meaning |
| --- | --- | --- |
| `key` | string(32, `[a-z][a-z0-9_]*`) | the rule's name (messages, report, tests); unique |
| `kinds` | array(string(32), 1, 8) | event kind keys the rule reads; each a kind of the compiled catalogue with `region_use` 1 (stage check) |
| `values` | array(string(32), 0, 4) | value keys summed into the event's magnitude; empty: magnitude 1 per event; each a value key of every kind of the rule |
| `min_magnitude` | int(1, 10^9) | the least sum over the window |
| `window_years` | int(1, 100000) | the window: events of the rule in the region with `year` > Y_e − `window_years` (1: the events of e's year) |
| `min_salience` | int(0, 100) | the least `event.salience` |
| `capital` | bool | the event's settlement must be the capital of the event's first civilization participant when the subsystem reads it |
| `chance_permille` | int(0, 1000) | the keyed chance that a qualifying event is a candidate |
| `weight` | int(1, 1000) | priority among candidates, and the measure of "weightier" (6 item 2); unique among rules |
| `name_role` | string(32) | what `{name}` is: `""` (no `{name}`), `settlement` (the event's settlement), or a role key of every kind of the rule with entity type civilization (the realm's root, 5.3) or settlement |
| `namer_role` | string(32) | a civilization role of every kind of the rule: the people credited with the name (the event's `namer`, `region_name.culture`); `""`: none |
| `forms` | array(string(48, `rename_form`), 1, 16) | the name forms (5.1) |

Grammar `rename_form`: ASCII letters, space and `'`, plus slots `{name}` and `{noun}`, each at most once; at least one letter outside the slots; no leading, trailing or double space; no other `{` or `}`. Description `a region name form (letters, spaces, ' and the slots {name} and {noun})`.

### 4.1 Shipped rules (normative data; taste)

| # | key | kinds | values | min | window | min salience | capital | chance ‰ | weight | name_role | namer_role | forms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `conquest` | `civ_conquered` | — | 1 | 1 | 0 | no | 1000 | 100 | `civilization` | `conqueror` | `{name}'s Grave`, `The Grave of {name}`, `The End of {name}`, `{name}'s Barrow` |
| 2 | `capital_sack` | `sacked` | — | 1 | 1 | 90 | no | 700 | 90 | `settlement` | `attacker` | `{name}'s Fall`, `The Ashes of {name}`, `The Ruin of {name}`, `{name} Waste` |
| 3 | `great_sack` | `sacked` | `dead` | 1500 | 1 | 0 | no | 500 | 80 | `settlement` | `attacker` | `{name}'s Pyre`, `The Ashes of {name}`, `{name} Waste`, `The Burnt {noun}` |
| 4 | `slaughter` | `battle`, `battle_at_pass` | `winner_dead`, `loser_dead` | 5000 | 100 | 0 | no | 600 | 70 | `loser` | `winner` | `The Red {noun}`, `The Bloody {noun}`, `{name}'s Bane`, `The {noun} of the Slain` |
| 5 | `plague` | `plague_outbreak` | `total` | 3000 | 1 | 0 | no | 400 | 60 | `settlement` | `civilization` | `The Emptied {noun}`, `The Fever {noun}`, `The Silent {noun}`, `{name}'s Grave` |
| 6 | `heartland` | `became_city` | — | 1 | 1 | 0 | yes | 300 | 50 | `civilization` | `civilization` | `{name}land`, `The Heart of {name}`, `The {name} Reach`, `{name}'s Hearth` |

Why these, against Paul's list: "a great sack or the fall of a capital" is rules 1 to 3 (`capital_sack` reads the sack's salience, which P3-06 sets to `capital_sacked_salience` 90 for a capital; whoever changes that value changes this field with it); "a long or costly war fought over the region" is rule 4, read as the blood shed in the region's battles within a century, which is what makes a field remembered whatever war it belongs to; "a founding that makes the region a realm's heartland" is rule 6, the moment a realm's seat becomes a city (a realm's founding itself is too common to be defining: 6 to 12 per world, every home region); "a plague that empties it" is rule 5, the great outbreaks (the top 1% by toll; an outbreak's region is its seat's). Later stages add rules for their own kinds (monsters, gods, deep horrors) by data, with no change here (P5-00 3.1).

**Tone.** The forms are dark and plain, of the register of the drawn names (P1-06 4.1's taste rules): a grave, a fall, ashes, a waste, blood, fever, silence, a hearth. A renamed region is the only place a proper name of history enters a region name, which is what makes it read as earned. `heartland` is the one hopeful rule and the rarest.

## 5. Names

### 5.1 Building a name

For a candidate (rule i, event e, region r), the slot values are fixed by the event:
- `{name}`: by `name_role`: `settlement`: the stored name of the event's settlement (the event's `settlement` column; r2: always this, also for a kind that has a role named `settlement`); a settlement role: the first participant's stored name; a civilization role: the **root** of its first participant's name (5.3). (r2) The event has **no value** for `{name}` when `name_role` is `settlement` and the event's settlement is 0, or when the role has no participant;
- `{noun}`: the land noun of r (section 3), as stored in the data (`Wood`, `Downs`, `Isle`, `Fells`); empty for a region whose biome has no definition (section 3).

```
F     = rule i's forms; n = |F|
start = pick(draw(hist.renaming.name, e, r, 0), n)          // StepContext::draw: w0 = the step's year
for j = 0, 1, …, n − 1:
    f = F[(start + j) mod n]
    if f holds {name} and the event has no value for it: next j   // r2
    if f holds {noun} and the noun is empty: next j                // r2
    N = f with {name} and {noun} replaced
    if valid(N, f) and N is not in the registry: return N
return none                                                 // the candidate is dropped (6 item 4)
```

One draw per candidate; every form is tried once, so the result is exact and bounded. A skipped form counts as an invalid one: when every form is skipped or invalid, the candidate is dropped. With the shipped rules and catalogue no form is skipped (every kind's `name_role` participant is required and every settlement-named kind has its settlement).

### 5.2 Validity of a renamed name (amends P1-06 for renamed names)

N is valid when all hold:
1. at most 48 bytes (P1-06 V6);
2. no three equal letters in a row, case-insensitive (V2);
3. no word of N contains a placename `forbidden` entry (V4) and N is not, and holds no word that is, an `excluded` entry (V5);
4. **V8** (P1-06 r11 17): no filter entry spans a join of N's parts. The parts of N are the literal runs of f and the two slot values, in order; a join is a byte offset between two consecutive parts. With the slot values themselves already names of the world (or list words), only what a join creates is new, so only that is tested (r2, real cases of the shipped filter: `{name}land` with the root `Thalo` gives `Thaloland`, where `lol` spans `Thalo|land`; `{name}'s Fall` with the root `Ufas` gives `Ufas's Fall`, where `fass` spans `Ufas|'s` once `'` is removed; r0's `Tar…` + `land` example was impossible, since `tarland` holds no `tard`).

The registry test makes a renamed name unique among every name of the world: region names (geographic and renamed), ages, gods, civilizations, settlements. The renamed name is added to the registry when the event is written; it is not added to the core set K of P3-02 12.2 (cores are civilization and settlement roots).

### 5.3 A realm's root

For a civilization with name C and culture data D: the first form A`{root}`B of D.`civilization_forms` (list order) such that C starts with A, ends with B and is longer than |A| + |B|; the root is C without A and B (`The Kingdom of Cear` → `Cear`; `The Krurz Horde` → `Krurz`; `Clan Uk'ghaam` → `Uk'ghaam`). If no form matches (a hand-made fixture), the root is C. A numbered fallback name (P3-02 4.3) keeps its numeral in the root (`Cear II`); none occurs in the shipped sweeps.

### 5.4 Who names it

The event's `namer` is the first participant of `namer_role` when the rule has one and that civilization exists, else none. `region_name.culture` is the namer's `civilization.culture`, 0 when there is no namer. The forms are English; the culture records whose tongue the name is told in, for P6's lore and a later per-culture list (question 3).

## 6. The subsystem

`RenamingSubsystem` (`worldgen/src/history/renaming.cpp`, `renaming.hpp`), steps **T6** and **Q4**, installed **after** `ConflictSubsystem` (P5-00 4.1: peoples, conflict, then P5 subsystems in merge order). It changes no `State`: it appends events and keeps its own members.

**Members**: `cursor` (the first event id not yet read, initially 1); per rule i and region r, the window list W[i][r] of (year, magnitude) in event order; per region, its renamings (year, weight) in order; the years of every renaming in the world, ascending; the `region_name` rows in creation order.

**Each run** (step year Y, which in a quiet step is the step's start):

1. **Read.** For every event e from `cursor` to `last_event_id()`, ascending; then `cursor` = `last_event_id()` + 1. For e with region r ≠ 0, for each rule i (data order) whose `kinds` hold kind(e):
   - m = Σ of e's values named in `values` (1 when `values` is empty); append (year(e), m) to W[i][r]; remove the entries with year ≤ year(e) − `window_years`; S = Σ of the magnitudes left;
   - e **qualifies** for i when S ≥ `min_magnitude`, salience(e) ≥ `min_salience`, and (if `capital`) e's settlement ≠ 0 is the `capital` of e's first civilization participant (r2: the first participant of the first civilization role of e's kind, in role seq order, that has a participant; participants are stored in ascending role seq, W4, so this is also the first participant of entity type civilization);
   - a qualifying e is a **candidate** (i, e, r) when `chance_pm(draw(hist.renaming.chance, e, r, k), chance_permille)` with k = i's rule number counted from 1 (r2: r0 wrote `i` in the key). The name draw of 5.1 is keyed (e, r, 0); both draws use w0 = the step's year (a quiet step's start).
2. **Choose.** For each region r with candidates, in ascending r: the candidate of the largest `weight`, then the smallest e. It is **taken** when, in this order:
   1. r has fewer than `region_max` renamings;
   2. r has no renaming, or Y − (its last renaming's year) ≥ `region_gap_years` **and** the rule's `weight` > the weight of the rule of its last renaming (a second name must come from a weightier deed);
   3. the world has fewer than max(1, R × `world_max_permille` div 1000) renamings;
   4. fewer than max(1, R × `century_max_permille` div 1000) renamings have a year > Y − `century_years`;
   5. in a quiet step, the event's charged civilization has `quiet_events_left` > 0 (P3-04 W9): the namer, or civilization 0 when there is none (`namer_role` `""`, or no participant of that role) (r2);
   6. 5.1 gives a name N.
   A candidate that fails is dropped (no retry later); its region's windows are kept.
3. **Write** a taken candidate: append `region_renamed` (year Y, `end_year` Y; region r; settlement = e's settlement when it lies in r, else 0; participant the namer as `namer` when there is one; cause e); add N to the registry; append the row (r, Y, N, culture, that event's id); record the renaming; clear W[i][r] for every rule i.

At most one renaming per region per step and, since `region_gap_years` ≥ 1, per year: the key (region, from_year) is unique.

**Visibility** (P3-04 6.9 rule 4): it reads every event already appended, including those of the same step by the subsystems before it; events of T7, Q5, Q6 and of the next year's awakening and flare-up start are read at the next T6 or Q4. Live state (capital) is read as it stands when the subsystem runs.

**Rules of P3-04 6.9.** No founding, no relation call, no iteration over `State`'s live lists (it iterates its own members and the log by id); draws keyed by event and region ids and the rule number (rule 5); fixed place in `EngineConfig::subsystems` (rule 3).

**Quiet steps.** A renaming in a quiet step is charged by the event writer to its namer, or to civilization 0's quiet budget when it has no namer (W9's charged civilization; r2), at most one per region; the world bound is the subsystem's own caps (items 3 and 4), which keep it far below P3-04's per-step limits.

**Effect on the rest of history.** No rule of peoples or conflict reads a name, the log's kinds or the new table. But every renaming takes an event id, and conflict keys some draws by event ids (a war's declaration id, P3-06 24.5): from the first renaming of a world on, later histories differ from 0.21.0's. So P5-01's targets are re-measured as statistics (section 11), not compared pair for pair.

## 7. Outputs

### 7.1 Kind 43 `region_renamed` (provisional id: the next free one at merge, P5-00 4.4)

| id | key | caused | region | settlement | roles | values |
| --- | --- | --- | --- | --- | --- | --- |
| 43 | `region_renamed` | 1 | 1 | 2 | 1: `namer`, civilization, 0–1 | — |

Template and salience (data, `history.report`):

```
region_renamed   salience 60
{@causes:civ_conquered=the fall of a realm,sacked=the sack,battle=the slaughter,battle_at_pass=the slaughter,plague_outbreak=the plague,became_city=the rise of a city|After |, }{@region_former} became known as {@region}.
```

Samples (illustrative names): `After the sack, the Holt of Yews became known as Edadbury's Fall.` · `After the fall of a realm, the Cruel Barrows became known as the Grave of Cear.` · `After the slaughter, South Heath became known as the Red Heath.` · `After the rise of a city, the Leas of the Harvest became known as the Heart of Nife.` A kind a later stage adds without a cause word renders `The Holt of Yews became known as …` (P3-04 20.2: an empty placeholder leaves the sentence start). `{@region_former}` is new (P3-04 r21 23.1).

Salience 60: a summary candidate in every default world (the summary threshold is 26 to 51), below a conquest (90) and a capital's sack (90) and with a peace (60). It is a companion of its cause both ways (P3-07 r21 26.5), so the summary shows the deed and the name together.

### 7.2 Table `region_name` (schema 8)

```
CREATE TABLE region_name (region INTEGER NOT NULL, from_year INTEGER NOT NULL, name TEXT NOT NULL, culture INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID
```

| Column | Value |
| --- | --- |
| `region` | `region.id` |
| `from_year` | the renaming's year: the name is in force from the end of that year (P4-01's half-open years) and, in sentences, from the renaming event on (P3-04 r21 23.1) |
| `name` | the new name: ASCII letters, space, `'` and `-` (r2: a `{name}` value may hold `-`, as goblin names do: `Slak-Grub's Grave`; P3-07 26.6's reader admits it); at most 48 bytes; unique in the world |
| `culture` | `culture.id` of the namer's culture, 0 none |
| `event` | the `region_renamed` event |

Executed after `relation` (the 27th table). Rows in primary-key order; written by the `history` stage in the same transaction as the other history tables; empty in a world without history. No row for the geographic name: **`region.name` stays geography's and is never rewritten** (P3-00 7, P5-00 3.1); the name of region r in force at year Y is the `name` of r's row with the largest `from_year` ≤ Y, or `region.name` when there is none. **The present name** is that rule at Y = E.

### 7.3 Who reads which name

| Reader | Name it shows | Change |
| --- | --- | --- |
| `render_event` (`{@region}`, region participants) | the name in force at the event (P3-04 r21 23.1) | yes |
| `report` (world report) | none printed; `[tables]` gains `region_name = <rows>`, `[history]` gains `region_names` directly after `relations` (P3-04 r22 23.4) | keys only |
| `report --history` sentences | as `render_event` | yes |
| `report --history` references `<name> (rg<id>)` outside sentences (`home:`, `[present]`, ruins, claims, wars) | the present name (P3-07 r21 26.4) | yes |
| `report --history` `[present]` | a new list of renamings with the former names (26.2) | yes |
| `region_name()` of `worldgen/inspect/layer_catalogue.hpp` (P4-02 4.6) | `region.name`, the geographic name, unchanged | no |
| `Timeline::region_name(region, year)` (P4-01 r3 16) | the name in force at the year | new |
| viewer hover text and region list | the name in force at the year shown (E on opening), by `Timeline`; until the viewer engineer's branch, `region.name` | viewer, after merge |
| `hash --detail` | a `table region_name` line | yes |

## 8. Invariants

1. Every `region_name` row has a `region_renamed` event with the same region and year, and every such event has exactly one row (`event` = its id).
2. Per region: at most `region_max` rows; consecutive rows at least `region_gap_years` apart; each later row's rule weight is larger than the earlier's, where a row's weight is that of **the exact rule that fired** for it (r2: a cause kind can belong to two rules, as `sacked` belongs to `capital_sack` and `great_sack`, so the cause's kind alone does not name the rule; the rule is the one of the taken candidate, known to the subsystem and to the reference's log, not stored in the world).
3. World: at most max(1, R × `world_max_permille` div 1000) rows; in any window of `century_years` at most max(1, R × `century_max_permille` div 1000).
4. Every `name` is valid by 5.2 and differs from every other name of the world (all region names, geographic and renamed, ages, gods, civilizations, settlements).
5. Every `region_renamed` has exactly one cause, an earlier event of a kind of a shipped rule, with the same region, which qualifies for that rule by 6 item 1.
6. `region.name` is the same as at 0.22.0 without history (`--until regions`) for every region.

## 9. Determinism notes (for the auditor)

- Keyed draws only, two new streams (section 3), keys (event id, region id, rule number) with w0 the step's year; a candidate's draws never depend on how many candidates came before.
- Iteration: the log by ascending id; rules in data order; regions ascending; windows are vectors in event order. No unordered container, no pointer key.
- Arithmetic: int64 sums of event values (each at most a settlement's population or a battle's dead, below 10^7; at most 64 entries a window in practice, never more than the events of the window); `R × permille div 1000` in int64.
- Strings: byte comparisons, ASCII; the root by prefix and suffix byte compare; the registry an ordered set.
- The land noun is recomputed from stored rows (H8); the stage reads no value held from the geography stages.
- The subsystem runs after conflict in T6 and Q4 only; it writes nothing through `State`.

## 10. Order of implementation and roles

One branch, one engineer, one test writer, working separately from the specs. `GENERATOR_VERSION` becomes **0.22.0** and `SCHEMA_VERSION` **8** in step 1. Goldens are re-recorded once, in step 9.

| Step | What | Spec |
| --- | --- | --- |
| 1 | Schema 8: DDL, `SCHEMA_VERSION`, empty world size, `schema.md`; `[tables]`, `hash --detail`; readers refuse version 7 | P3-04 r21 23.4 |
| 2 | Data: `history.renaming` (file in `game/data/history/` and both fixtures, P3-01 r16 5.4); `history.report` entry `region_renamed`; human `forbidden` `ass` (P3-02 r19 16.2) | 4; P3-01; P3-02 |
| 3 | Names: P3-02 r19 16 (fused suffix rule) in `name_attempt`; P1-06 r11 17 (V8) in the regions stage, fallback included | P3-02 16; P1-06 17 |
| 4 | Catalogue: kind 43; `{@region_former}` in `template_problem` and `render_event`; `HistoryView` reads `region_name` and renders names at the event | P3-04 r21 23.1, 23.2 |
| 5 | `StepContext` reads (P3-04 r21 23.3); the shared name registry seam; the land-noun function exposed | P3-04 23.3; 3 |
| 6 | `RenamingSubsystem`, its streams, `HistoryResult` and the writer for `region_name` | 5, 6, 7 |
| 7 | Reader: `read_history_tables` and its checks for `region_name`; `Timeline::region_name` | P3-07 r21 26.6; P4-01 r3 16 |
| 8 | Report v3 and A1 | P3-07 r21 26 |
| 9 | Goldens (section 12), README hash, CLAUDE.md and README status (lead) | 12 |

- **Engineer**: steps 1 to 8; tell the viewer engineer the new table and `Timeline::region_name` (P5-00 4.3), and keep `gdext-headless-check` green (it compares layers and composites, not names, so only the schema bump reaches it).
- **Test writer**: section 13, from the specs only; the conflict reference grows the renaming subsystem (every history table and event id changes with it).
- **Validation**: section 11 on the sweeps of P5-01 (100 default 1024 worlds, seeds 0–99; 20 `civs4`; plus `calm`, `turbulent`, `d512` seeds 0–19, `d2048` seeds 0–4), the cross-preset comparison, timing. A missed target goes to the lead; validation changes no tuning on its own.
- **World critic**: section 14.

## 11. Targets (validation)

At 1024 defaults (100 worlds) unless stated. "Prototype" figures are the spec writer's, from the 0.21.0 sweep (section 15). (r2) "Measured" figures are **the engineer's measurement** at 0.22.0 on `p5/12-int` (100 default 1024 worlds, seeds 0–99; 20 `civs4` worlds for R8); they replace the prototype's where a target quotes one. Validation's sweep follows and is the record.

| Target | Definition | Prototype / 0.21.0 | Measured at 0.22.0 (engineer, r2) | Bound at 0.22.0 |
| --- | --- | --- | --- | --- |
| **R1** | renamed regions per world (regions with at least one row) | median 4 (80‰), 10th percentile 2, max 7 (148‰); 2 worlds with none | median 4, 10th percentile 2, max 8 | median 3 to 6; at least one in ≥ 90 worlds |
| **R2** | the caps of 8 items 2 and 3 | — | — (validation) | every world (hard) |
| **R3** | renamings by rule over the sweep | `capital_sack` 169, `slaughter` 68, `heartland` 42, `great_sack` 36, `plague` 36, `conquest` 36 of 387 | `capital_sack` 185, `slaughter` 66, `plague` 46, `conquest` 44, `heartland` 34, `great_sack` 27 of 402 | no rule above 60%; every rule at least 10 times |
| **R4** | candidates dropped for want of a name (6 item 2.6) | — | 0 | at most 1% of the candidates that pass items 2.1 to 2.5 |
| **R5** | second renamings of a region | 0 | 17 | reported |
| **R6** | invariants 1 to 6 (8) | — | — (validation) | every world of every sweep (hard) |
| **R7** | `civs4`, `calm`, `turbulent`, 512, 2048: R1 to R5 | — | — (validation) | reported; R2 and R6 hard |
| **R8** | summary lines (P5-01 T18) and calamity share (T17) with the renaming lines | T18 default median 571, max 754; `civs4` median 1,046, max 1,353 | default median 587, max 777; `civs4` median 1,085, max 1,357 | T18's bounds still hold (default median ≤ 700, max ≤ 800; `civs4` median ≤ 1,100, max ≤ 1,400); T17 holds |
| **N1** (F1) | human settlement names with a doubled or double-joined suffix, or `ass` (P3-02 r19 16.3's test; r2: the suffix part counted on the attempts by replay, `ass` on stored names, P3-02 r20 16.4) | 40 names in 30 of 100 default worlds; 14 in 11 of 20 `civs4` | — (validation) | 0 in every sweep; name fallbacks 0 |
| **N2** (F2) | region names (drawn, fallback, renamed) with a filter entry across a join; `Tarden` | 4 drawn names in 4 worlds; `Tarden` in seed 84 | — (validation) | 0; the drawn names equal 0.21.0's except exactly `Wanegrass` (seed 15), `Pinegrove` (59, 91) and `Tarden` (84), measured |
| **N3** (F3) | `[present]`'s hidden refuges against the reference's refuges hidden at E | 10 of 40 listed refuges hidden | — (validation) | equal in every world; no listed refuge found before E |
| **P** | P5-01's T1, T2, T5, T9 to T17, T19, T20, quick aim wins, T3, T4, T6, T7, P3-00 12.3's hard bounds | P5-01 8 at 0.21.0 | T16: 13 repeated battles of 8,935 (9 at 0.21.0); the others to validation | P5-01 8's bounds (statistics, not pair for pair: section 6) |
| timing | `generate` at 1024, release, seed 0, three runs (P5-00 8.1) | 0.21.0 | — (validation) | at most 5 s more (expected well under 0.5 s); the whole `generate` under 60 s; `report --history` under 2 s |

**T16 at 0.22.0** (r2). P5-01's T16 bounds the battles that repeat an earlier battle's sides and both tolls exactly at **at most 10** over the 100 default worlds; the engineer measured **13 of 8,935 battles** at 0.22.0 (9 at 0.21.0). No battle rule changes in P5-12: the renamings shift the event ids that conflict keys its draws by (6, "Effect on the rest of history"), so which tolls repeat moves from world to world. **The bound is not changed here.** The lead proposes restating T16 as a rate, **at most 2 per thousand battles** (13 of 8,935 is 1.5‰; 9 at 0.21.0 was about 1‰), since a fixed count grows with the number of battles a sweep holds; the decision is Paul's (`docs/design/proposals.md`, "History: P5-01's T16 restated as a rate (P5-12)", open). Until he decides, T16 as written is missed at 0.22.0 and validation reports it.

Reported, no target: renamings by year and by age; the share of renamed regions that were plain compass names (keepers) at 0.21.0; the forms drawn per rule; the namer's race per rule; the summary's renaming lines per world; per culture, the attempts rejected by P3-02 r19 16 (expected about 3% of human settlement attempts, 0 elsewhere but `dell`).

## 12. Effect on output: golden values

**Every world file changes**: schema 8 (DDL, `user_version`), `GENERATOR_VERSION`, `content_hash` (new tuning, the human culture, the report entry). The empty world grows by one page (the root of `region_name`): 35 pages, 143360 bytes, to be confirmed by the engineer with the vendored SQLite (P3-04 9.6's method).

| Golden | Re-recorded | By | What moves |
| --- | --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`; `GOLDEN_CASES`, `stages_pipeline_test.cpp`'s hashes; README's phrase hash | yes | validation, after the cross-preset comparison | every case |
| `core_vectors.txt`, `p1-01` … `p1-05` | no | — | must not move |
| `p1-06-regions.txt` | yes (by the reference) | test writer | the `region` hash of a case only where V8 rejects an attempt (r2: one case moves, `shipped-512-phrase`: region 11 `Tarden` → `Lichenknoll Forest`; r0 expected none) |
| `p3-01-data.txt` | yes | test writer | content hashes |
| `p3-02-names.txt` | yes | test writer | human names through 16 |
| `p3-03-chronology.txt` | yes where a human god root holds `ass` | test writer | god names |
| `p3-04-engine.txt` | yes | test writer | catalogue rows (kind 43); stub sentences unchanged |
| `p3-05-peoples.txt` | yes | test writer | every history (names, P3-02 16); **no kind 43 event** (r2: the renaming subsystem is installed only after conflict, P5-00 4.1, so the peoples-only configuration of these worlds has none) |
| `p3-06-conflict.txt`, `hist-conflict-73-1024` | yes | test writer | every history (names; kind 43 events; ids after the first renaming) |
| `p3-07-report.txt` | yes | test writer | every report (v3) |
| `p4-01-timeline.txt` | yes | test writer | the dumps gain names (P4-01 r3 16) |
| **`p5-12-renaming.txt`** (new) | new | test writer, from `worldgen/tests/reference/p5_12_renaming_reference.py` | `region_name` table hash and lines per world case and fixture |

`world-hashes.sh` keeps its fifteen cases; every history case's files change. (r2) The phrase world (`geo-phrase-512`) moves twice over: its region 11 is `Lichenknoll Forest` from 0.22.0 (V8 rejects `Tarden`, P1-06 r12 17.3), so every history golden that holds that world carries the new name as well as the renamings.

## 13. Tests required

**Unit** (C++, a new shard `worldgen-tests-renaming` for `*renaming_*`):
1. Rules: magnitude sums over `values`; the window (an entry at year Y − `window_years` drops, at Y − `window_years` + 1 stays); `min_salience`; `capital` true and false; the chance draw's key (event, region, rule number) and w0.
2. Choice and caps: two candidates in one region (weight, then smaller event); `region_max`; the gap (299 refused, 300 taken; r2: the second candidate must come from a **weightier** rule than the first renaming's, or 6 item 2.2 refuses it for its weight whatever the gap, so the test pairs a first renaming with a second of a larger weight); a second renaming of equal weight refused, of larger weight taken; the world cap at R = 12 (1), 49 (7), 200 (30); the century cap and its window; a quiet step whose namer has no quiet events left drops the candidate; windows cleared after a renaming, kept after a drop.
3. Names: the root for each shipped culture's three civilization forms, a fixture name no form matches, a numbered fallback; `{noun}` for each RG11 3 rule (isle, lake, upland, biome); the start form and the cycle; a name in the registry skips to the next form; V8 at a join (r2: `{name}land` with the root `Thalo`, `lol` spanning `Thalo|land`, rejected and the next form taken; `{name}'s Fall` with the root `Ufas`, `fass` spanning `Ufas|'s` with `'` removed; r0's `tar` + `land` example held no `tard`) and not inside a value; all forms invalid gives no renaming; the new name enters the registry.
4. The written event: kind 43, region, settlement (in r, or 0), namer (present, absent), one cause; the row.
5. Rendering (P3-04 r21 23.5): the name at the event; an event of the renaming's year before it keeps the old name; `{@region_former}`; `template_problem` refuses `{@region_former}` where `region_use` ≠ 1; the `{@causes}` words of kind 43.
6. Reader (P3-07 r21 26.6): each corrupt-row message.
7. Report v3 (P3-07 r21 26.8) and `Timeline::region_name` (P4-01 r3 16).
8. Names F1 and F2 (P3-02 r19 16.4, P1-06 r11 17.3).
9. Data: `history.renaming`'s fields, bounds, absence, and its checks (P3-01 r16 5.4); the stage check `renaming_rules` with each message.

**Property** (the `*-slow` suites at 1024, seeds 0–19 at defaults; 512 in the default suites): invariants 1 to 6; R2.

**Fixtures**: a hand-made world root `rename-small` and one with `region_max` 0 (no row, no kind 43 event; every other table equal to the same world with the subsystem removed). (r2) `rename-small` is the conflict reference's **stress data** (wars, sacks and conquests on small synthetic maps) with every `min_magnitude` 1, every `chance_permille` 1000 **and both caps opened** (`world_max_permille` and `century_max_permille` 1000: on maps of a few regions the shipped caps allow one or two renamings, and the rules could not all appear). Every rule but `heartland` renames at least once on it: synthetic maps without cities never raise a capital to a city, so `heartland` is covered by the world cases (it fires in the default worlds, R3).

**Golden**: `p5-12-renaming.txt` (section 12); every history golden through the conflict reference with the subsystem.

**Reference**: `p5_12_renaming_reference.py` on `p3_conflict_reference.py`: the subsystem of section 6 inside the conflict reference's replay (the event log and ids depend on it), and a `check-world` mode for invariants 1 to 6.

## 14. For the world critic

On the 0.22.0 sweep (default seeds 0, 5, 42, 72, 84; `civs4` 1 and 3; the phrase world) at both levels:
1. The names: dark, weighty, never comic; how they sit beside drawn names on one map; the forms that repeat across worlds; `{name}land` with goblin and orc roots (`Uk'ghaamland`); `The Heart of …` as the hopeful one.
2. The frequency: does about one region in twelve feel like history, or too few to notice, or a theme?
3. The renaming sentence and its cause words; the chronicle before and after (an old event keeps the old name; a later war is "over Edadbury's Fall").
4. `[present]`'s renamed regions and hidden refuges.
5. F1's settlement names (the human names of the seeds the P5-01 review quoted: 20, 41, 82) and F2's four changed region names (`Pinegrove` is a false positive of V8: accepted cost, question 2).

## 15. Measurements

Harnesses in `~/.cache/the-unmapped/out/p5-12-spec/`, run on the 0.21.0 sweep (`~/.cache/the-unmapped/out/p5-01-impl/sweep/`, 100 default and 20 `civs4` 1024 worlds):

- **`cand.py`** (candidates): regions per world 42 to 59 (median 49); sacks 1,295 (median 12 a world), of them 412 of a capital (salience 90); a sack's `dead` median 447, 90th percentile 1,487, max 6,183; 88 conquests (0 to 4 a world); per region, the largest battle dead within 100 years median 1,470, 90th percentile 4,397, 99th 8,795; 7,804 plague outbreaks, `total` 90th percentile 1,549, 99th 3,539; `became_city` median 5 a world. The thresholds are set from these: `great_sack` 1,500 (the top tenth of sacks), `slaughter` 5,000 (about the top tenth of contested regions), `plague` 3,000 (about the top 1.5% of outbreaks).
- **`proto2.py`** (the rules, approximated on the stored log: a capital by salience 90 for sacks and by the end-state capital or first settlement for `became_city`; no quiet-step budget; every name found): renamed regions 0 / 2 / 3 / 4 / 5 / 6 / 7 at percentiles 0, 10, 25, 50, 75, 90, 100 (shares 0 to 148‰, median 80‰), two worlds with none; the world cap binds in the busiest worlds; first renamings median year 5,610 (most fall in the later, warlike ages). Variants: thresholds 1,000 / 4,000 / 2,500 give median 5 (96‰); 1,500 / 6,000 / 3,500 give median 4 (75‰).
- **`v8.py`** (F2): P1-06's reference (`worldgen/tests/reference/p1_regions_reference.py`, which reproduces every stored name of the 100 worlds) with V8 added to the validity: 4 names change in 4 worlds, no other region's name changes. Without the join condition the shared filter would hit 140 region names (`Knight's`, `Benighted`, `Nightwood` by `nig`; `Sicklemark` by `sick`; `Slagwold` by `slag`; `South` by `words.whole`), which is why V8 tests joins only.
- **`f1.py`** (F1): of 1,211 human settlement names in the default sweep, 40 break 16's rule (join 30: `Deasstead`, `Bantton`, `Wentton`, …; another suffix 4: `Canashamford`, `Steadham`, `Ashambury`, `Deanttonwick`; the same suffix 3: `Sudsteadstead`, `Galwickwick`, `Stonton`; `ass` inside 3: `Beasstontstead`, `Edasserford`, `Reassintford`); 14 of 279 in `civs4`. No other culture's name is touched: `ass` occurs in human names and in one elven settlement, `Thassie` (critic G7), which the human list does not reach; left as it is.

## 16. Decisions made

1. **`region.name` stays geographic; the present name is derived.** P3-00 7 forbids history to rewrite geography's tables, and P5-00 3.1 fixed it. Every reader that wants the present name asks for the name at E; the layer catalogue's `region_name()` keeps its meaning (the geographic name), so no P4 function changes behaviour.
2. **Sentences by the event, references by the present.** A sentence is a record of its moment (`… declared war … over the Holt of Yews` stays true in 4500); a reference is a lookup, and one name per id keeps `rg12` and its name together across the report.
3. **Rules in data, magnitude by value keys.** A rule names kinds, value keys, a window and conditions, so a later stage adds its kinds with a data row and no subsystem change (Paul's decision 3 of the P5 overview). The `capital` flag and `min_salience` are the two conditions the shipped rules need; they are generic.
4. **Rarity by thresholds, a chance and caps.** The thresholds make an event defining; the chance keeps a defining event from always renaming (not every sack names a land); the caps bound the share (150‰), the pace (one per century per 20 regions) and the churn (two per region, 300 years apart, the second weightier).
5. **F1's rule is wider than the critic's.** "Reject `ss` before a suffix starting with `s`" is the case x = `s` of the join rule (the root's last letter equals the suffix's first), which also removes `Bantton` and `Wentton` (21 names), stutters of the same kind; and "the same suffix" is widened to any fused suffix of the kind (`Canashamford`). Question 2.
6. **V8 tests joins only** (15, `v8.py`): region words are curated English words, and the filter was built for syllable roots.
7. **F3: a heading of its own.** `hidden refuges: <h> of <n> founded by flight` keeps the count of all refuges in one number and lists only those a player could still find hidden.

## 17. Open questions (for Paul; each with a recommendation and the interim choice)

1. **How many renamed regions.** Recommendation and interim: about one region in twelve at 1024 (median 4 of 49), capped at 15%, a second name only after 300 years and for a weightier deed. The alternative is more (the 1,000 / 4,000 / 2,500 thresholds: median 5) or fewer (`world_max_permille` 100).
2. **The wider F1 rule and V8's false positives.** Recommendation and interim: as specified (it also removes `Wentton`-type names; `Pinegrove`, `Wanegrass` are redrawn because `negr` spans the join). The alternative is the critic's two rules exactly, which leaves 21 `-ntton` names, and V8 on slurs only, which keeps `Pinegrove`.
3. **Names in English only.** Recommendation and interim: one English form list per rule; `region_name.culture` records the namer, so a later change can add per-culture forms (a dwarven `The Halls of …`, an orcish `…'s Skulls`) as data. The alternative is per-culture forms now (a `forms_<culture>` field; six lists per rule).

## 18. Left for later (with the P5-01 critic's evidence, `docs/reviews/p5-01-followups-world-critic.md`)

- **Old standing claims never lapse, and the claims list has no cap** (G1): 612 standing claims at E in the default worlds, median age 102 years, 113 older than 500, the oldest 3,773 years (seed 47 e27); `civs4` seed 1 lists 47 (about 50 lines). Suggested: `history.conflict.claim_max_years` (about 1,500) unless at war, and the list cut to the 10 oldest plus "and N more". A balance pass of conflict; changes worlds.
- **"Found no one" against a realm that still holds regions** (G2): seed 84 e441 (the Weeli Court holds 5 regions in 6000, no refuge); seed 72 e74 and e101; 332 `search_abandoned` against 51 `refuge_hidden` in the default sweep. Needs a line for where a hiding realm went, or a rule change in P3-06 20.17.
- **A realm with settlements and no region** (G5): `civs4` seed 1, c16 the Koz-Krurk Horde, 0 regions and 4 settlements. A wording or a rule (settlements in land held by others).
- **The calamity share at `civs4` and 2048**: T17 is met at defaults (median 7.8%), but the `civs4` and 2048 summaries show almost none (median 0.0%, max 0.5% and 1.8%; `docs/reviews/p5-01-followups-validation.md`, notes). The summary threshold scales with the world; whether a large world's summary should keep a famine or plague is a report question for the critic and Paul.
- The elven `Thassie` and the critic's other weak names (G7: `Whomoutwia`, `Kaaghdam`, `Tirkbumm`): not filtered; a names change if wanted.
- Per-culture renaming forms (question 3); renaming rules for P5-02's monsters, P5-07's deep horrors and P5-08's gods (their stages, by data).

## 19. Proposals

Two entries in `docs/design/proposals.md`: "History: regions renamed by history, P5-12 (readings and scope)" and "Schema 8: table `region_name` (P5-12)" (hard rule 7).

## Revision notes

- r0 (2026-11-22): first draft.
- r2 (2026-11-23): corrections after the implementation and integration on `p5/12-int` (the lead's rulings; no implementation defect was found, and the code and the independent references agree; text only, no rule, data or byte changes). 3: the land noun from stored rows renumbers the landmass labels 1..k in ascending order; a biome with no definition gives an empty noun; the synthetic geographies' landmass and biome. 5.1: `name_role` `settlement` is always the event's settlement column; a form with `{name}` is skipped when the event has no value for it, a form with `{noun}` when the noun is empty. 5.2 item 4 and 13 item 3: real V8 examples (`lol` in Thalo + land, `fass` in Ufas + 's); r0's `tar` + `land` example was impossible. 6 item 1: the first civilization participant defined; the chance key (e, r, rule number from 1), the name key (e, r, 0), w0 the step's year. 6 item 2.5 and "Quiet steps": a renaming with no namer is charged to civilization 0's quiet budget (W9). 7.2: names may hold `-`. 7.3: `[history]` `region_names` after `relations`. 8 item 2: the weight of the exact rule that fired. 11: the engineer's measurements at 0.22.0 beside the prototype's; the T16 note (restating it as a rate is proposed, Paul decides). 12: `shipped-512-phrase` moves in `p1-06-regions.txt`, and with it every history golden of `geo-phrase-512`; `p3-05-peoples.txt` has no kind 43 event. 13: the gap test needs a weightier second rule; `rename-small` is the stress data with both caps opened, `heartland` covered by the world cases. Amended specs of this round: P1-06 r12, P3-01 r17, P3-02 r20, P3-04 r22, P3-06 r24, P3-07 r22; proposals entry for T16.

## Pointer to P5-02 (2026-11-23, generator 0.23.0, before implementation)

Carried-over items: slot `{stem}`, the possessive rule, the new forms of `capital_sack`, `great_sack`, `slaughter` and `plague`, the cause words `the rise of a realm's seat` and `the ravage`, `name_role` of a monster role, rule 7 `ravage`; audit A1 and A2. Normative text: `docs/specs/p5-02-beasts-monsters.md` sections 9.3, 21. This note is the revision record here; the section numbers of this spec are unchanged.
