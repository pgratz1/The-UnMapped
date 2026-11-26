# P5-00 History depth overview

Status: **draft r1, 2026-11-22, for the lead's and Paul's approval.** Phase P5 History depth, the counterpart of `p3-00-history-overview.md`. Design doc sections 5 (History simulation), 6 (World database), 9 (Magic: what spells and books are made of), 12 (open questions: sub-peoples, history renames regions, mountain peoples' own history), 13 (the P5 row and its exit check) and 14 (records a model can narrate; the divine script). Depends on the approved P3 (generator 0.20.0, schema 7) and on P3-00 to P3-11 with their amendments. Proposals read: the entries of 2026-11-20 to 2026-11-22, in particular "Paul's notes of 2026-11-20" (alliances, trade and wealth), "History: magical artifacts are made in response to conflicts", "Paul's decisions of 2026-11-22" (a fresh claim at a declaration), "Paul's decision of 2026-11-22: regions renamed by history, and a history for the mountain peoples" and "Paul's approval of phase P3 History core, 2026-11-22". Timing figures are from `docs/reviews/p3-10-followups-round2-validation.md` 6 and `docs/reviews/p3-11-followups-validation.md` 6.

This overview fixes what every P5 stage spec inherits: the split into stage specs and their order (3), the shared rules (4), how settings come into force (5), the schema and catalogue plan (6), the history report (7), the budget and validation towards the exit check (8), the design's open questions (9) and what P5 leaves to P6 (10). It specifies no subsystem's rules. Values marked "interim" are this spec's reading where the design is silent; section 11 lists the ones that change the split or the order for Paul.

## 1. Purpose

P3 gave every world peoples, settlements, territory, wars and ruin. P5 fills the rest of design section 5: the wider pantheon and its interventions, beasts and named monsters, figures and factions, race relations and mixed settlements, refugee migrations, casters and their spells, roads and improved passes, alliances, trade and wealth, artifacts made in response to conflicts, regions renamed by their history, and a history of their own for the mountain peoples.

**Exit check** (design 13): across a sweep of seeds, every race, god camp and monster kind takes part, and no world is empty or runaway. Section 8.3 turns this into measures. In addition, from this overview: the default world still generates in under 2 minutes (8.1), P3's hard bounds (P3-00 12.3) still hold on every seed, and the history report reads as a history to the world critic.

**Not in P5** (section 10): sites derived from the log (battlefields, tombs, barrows), dungeon records, quest hooks and spellbook placement (P6); the divine script (design 14: after the prototype); underground geography.

## 2. What P5 inherits unchanged

- The calendar, phases, tick and quiet-step order of P3-00 2 and 4 and P3-04 5 and 6. P5 adds subsystems; it does not change the scheduler.
- H1 to H8 (P3-00 4.5), the keyed draws (P3-00 4.6, P3-02 4.1) and P3-04 6.9's rules for subsystems, with the lint rules `history-live-iteration`, `history-held-reference` and `history-sequential-rng`.
- The event log contract (P3-00 6.2), the catalogue rules of P3-04 8.2 ("a shipped kind is never edited"; what is and is not a schema change) and rendering from stored templates (P3-04 8.4, 8.5).
- P3's history rules as summarized in design 5 "What phase P3 has settled about history", in particular "no race dies out" (flight of the last refuge) and the shelter of a new civilization. A P5 rule that would break one of them is a design question for Paul, not a stage decision.
- The history report `report --history` (P3-07), its selection and its golden texts.

## 3. Stage specs of P5

### 3.1 Breakdown

All P5 stages extend the stage `history`, apart from the pantheon part of P5-08, which extends `chronology` (6.1). Each stage after P5-01 adds **one new subsystem** under the P3-04 engine (4.1). Tables, kinds and data are given in outline; the stage spec fixes them.

**P5-01 Carried-over follow-ups.** The changes Paul's approval of P3 carried into P5, as one change. *World bytes change* (P3-11 section 12; Paul's decisions of 2026-11-22): (a) a fresh claim on the goal at a declaration whose goal differs from its claim's region (decision 6, option (b) of P3-11 10 item 1); this removes the report's "(The claim, since …, was to …)" note, and with it B6's second point; (b) the orc quarrel war gated on strength (P3-11 critic F1); (c) a lower share of grim region names (`placename` data); (d) `became_city` without the exact population (P3-07 A10); (e) name filter additions: `shiva` on the famous-name list, `fag` inside a word, the dwarven codas `din` and `rin`; (f) one number form per battle toll ("24 of the defeated and seven of the victors" becomes either all digits or all words); (g) a record of a refuge still hidden at E and of a claim dropped at a declaration with no allowed goal (P3-07 24.7), **as appended event kinds, not columns** (interim, so that P5-01 stays at schema 7); (h) the test reference's participant bound for kinds 39 and 40 (64 against 62; a test fix). *Report only, no world byte* (`docs/reviews/p3-07-history-report-world-critic-r17.md`): B1 a floor of 30 on the summary threshold or a cap on calamity lines (the spec measures both and picks one); B2 the kind of a ruin in words ("abandoned to <civ>", "faded"); B3 adversary lines only for realms with a war, plus a count of the others in enmity, and faded ruins counted apart; B4 the block of a realm that ended before its first war cut to one line; B5 the `flare_up` line of every phase with a war shown; B6 one end year per war across lines, colonies preferred to death tolls as milestones, `[stats]` at summary cut to the keys a reader wants (A9); audit A3, `find` in place of `std::map::at` in `digest.cpp`. B3 to B5 change line types, so the report format moves to `# worldgen history v2` here, with the growth rule of 7.2. Data: `placename`, `name_filter`, `culture` (dwarf), `history.report`, `history.conflict`. Depends on nothing; it goes first because it changes world bytes and every later stage re-records on top of it.

**P5-02 Beasts and monsters.** Beasts as counts per region and species that rise and fall with habitat, slow settlement growth when predators are many, feed it by hunting, and attack travellers (design 5 "Beasts as pressure"); named monsters and monster groups as actors with a lair, a territory, a hoard and a kill list, which wake, raid, claim lairs, destroy settlements and are slain or survive (design 5 "Monsters as actors"); undead that rise at places of much violent death or broken burial (design 5 "Undead from history"), tied to the events that caused them. A monster's waking is a tension source (design 5 "Flare-ups … a monster wakes"). Tables: `beast` and `monster_kind` (echoes of the data, as `biome` and `race`), `beast_population` (per region and species, the end state and rows by year as `settlement_census`), `monster` (design 6 columns plus its start and end events), `site` (first rows: lairs; 10.1). Entity types 8 `monster` and 9 `site` come into use. Event kinds in outline: monster woke, raid, lair claimed, settlement destroyed by a monster, monster slain (by an army until P5-06 brings heroes), beast attacks, undead rose. New required data kinds `beast` and `monster` (design 5: "every kind is defined in data files"), with the starting roster of design 5 and the deep horrors of P5-07 as monster rows from the start. `--monster-pressure` takes effect (5). Amends P3-05's growth (predators) and P3-06's tension list. Depends on P5-01.

**P5-03 Race relations, mixed settlements and refugee migrations.** A disposition per pair of races that history moves (old wars leave grudges; design 5 "Race relations"), distinct from the per-civilization relations of P3-06; settlements with several races, each with its share and standing, made by conquest, trade and migration (design 5 "Mixed settlements"); migrations of whole peoples as refugees from a sack, a famine, a plague or a monster into another people's land (design 5 "Waiting for phase P5"), beyond P3's flight of a last refuge. Tables: `race_relation` (end state, and intervals by year), `settlement_race` (by year, as `settlement_census`). Event kinds: refugees arrived, settlement became mixed, a people's standing changed, disposition turned. Data: the race field for stances toward other races (P3-00 8.1 "relations by race", reserved), and `temperament` if the spec needs it. A civilization keeps one principal race (P3's `civilization.race`); the mix is per settlement (interim reading of design 5 "a society of one race, or a mix"). Depends on P5-01; runs in parallel with P5-02.

**P5-04 Wealth, trade and alliances; mines and forges.** Wealth per civilization from land, mines and trade; trade partnerships between peoples in contact whose disposition allows it, which add to both sides' growth and strength; alliances, in which an ally may join a partner's war or battle (Paul's notes of 2026-11-20, item 3); vassalage, now that tribute has something to pay with (design 5 "Relations"; P3-06 4 deferred it); the faeries' alliance with the elves by origin (design 5 race table). The dwarves' part (Paul, 2026-11-22): mines opened on reachable ore (`ore_reach_cells`), forges that turn ore into wealth and arms, and arms traded to other kingdoms, raising the buyer's strength. Relation kinds `trade`, `alliance`, `vassal` (text keys of `relation`, no column); table `civilization_census` (wealth and arms by year, as `settlement_census`); mines as `site` rows. Event kinds: trade opened and broken, alliance formed and broken, ally joined a war, became vassal, mine opened, arms traded. Data: race fields for trade and craft, tuning id `history.economy`. A bloc of allies is measured for runaway (8.3). Depends on P5-02 (`site`) and P5-03 (dispositions decide who trades and allies).

*Amendment (P5-04 r2, approved by Paul 2026-11-24; `docs/specs/p5-04-alliances-trade.md` 20):* this paragraph is superseded on three points. Vassalage (relation `vassal`, "became vassal") is not in P5-04; it is deferred to a small stage after P5-06 Figures. Wealth and table `civilization_census` are not written (no rule needs a wealth number; a share of spoils is counted in people). The tuning id is `history.alliances`, not `history.economy`. P5-04 does take the mines (`site` kind 3 `mine`, schema 11) and the arms trade, and adds a share of the spoils for allies.

**P5-05 Roads and improved passes.** Civilizations build roads across steep ground and improve passes as they grow, which opens land that was unreachable; passes become places to fortify, tax and fight over (design 5 "Mountains, passes and roads in history"; P3-00 15 question 7). Road travel lowers the crossing cost of a border from the year it is built (P3-00 10.4 "or (from P5) a road"), so expansion, armies and trade read the region graph by year. Tables: `road` (design 6: id, kind, points as integer cells, with the borders it crosses and its building event), pass improvements as events on `pass.id` (a value, no new entity type). Event kinds: road built, pass improved, pass fortified, toll taken, road fell into disuse. Data: tuning id `history.roads`. Depends on P5-04 (trade and wealth motivate and pay for roads). The road polylines are the first history output a map overlay can draw (P4).

*Amendment (Paul's scope decision of 2026-11-24, Q-P5-00-1: "take the recommendation except for mountain depths"; `docs/specs/p5-06-figures.md` 24):* this stage is moved to P6; P5 builds no roads, and passes stay fixed crossings through P5.

**P5-06 Figures.** Named individuals: rulers, heirs, generals, heroes and outlaws, with birth and death years and ties to each other (design 5 "Figures"), born, rising, acting and dying in T3 (P3-04 6.2, reserved for figures). Rulers turn P3's ruling generations and succession crises into named lines; generals lead battles; heroes slay monsters (P5-02). Long-lived figures persist through quiet steps (design 5). Table `figure` (design 6 columns plus start and end events, and the place of death for P6's tombs); ties between figures as `relation` rows with entity type 6. Event kinds: born, came to rule, led a battle, slew a monster, exiled, died. Data: person names in each `culture` (a new name kind, P3-02), figure roles in tuning `history.figures`. Depends on P5-02 (heroes and monsters); runs in parallel with P5-04 and P5-05.

*Amendment (Paul's scope decision of 2026-11-24, Q-P5-00-1: "take the recommendation except for mountain depths"; `docs/specs/p5-06-figures.md` 24):* P5-06 also takes a minimal form of casters (a figure role; towers and libraries as `site` kind 4; named books in a table `book`; rare blights), in place of the full P5-09. Its spec is `p5-06-figures.md` (schema 12, generator 0.27.0 at the planned merge).

**P5-07 Mountain depths.** The mountain peoples' own history (Paul, 2026-11-22): delvings that go deeper with the years a mine is worked, and the great dangers of the deep, which delving can wake and which can wipe out a hold or a whole realm, leaving a lost hold as a ruin and a site. The horrors are monster rows of the `monster` data (design 5; the game needs its own, not borrowed ones). A realm wiped out this way follows P3's rule that no race dies out: if it held the race's last settlements, the survivors flee (interim reading; proposals entry). Event kinds: delving deepened, a deep thing woke, hold lost, expedition lost (a P6 quest seed). Depends on P5-02 (monsters, sites) and P5-04 (mines). Hill dwarves, if Paul wants them, would follow this stage (9.3).

*Amendment (Paul's scope decision of 2026-11-24, Q-P5-00-1: "take the recommendation except for mountain depths"; `docs/specs/p5-06-figures.md` 24):* kept, as a small stage on P5-04's mines (Paul did not take the recommendation to defer it).

**P5-08 Pantheon and divine interventions.** The rest of the pantheon in three camps with domains (design 5 "Gods and the order of the races"; design 6 `god.domain`), written by `chronology`; the seeded intervention source of P3-04 6.8 with the five kinds of design 5 and 14: bless, curse, raise a champion (a figure, P5-06), send a monster (P5-02) and mark a site as sacred or profane (P5-02's `site`). A god acting is a tension source (design 5 "Flare-ups"). Tables: `god` gains rows and the `domain` column. Event kinds: `divine_bless`, `divine_curse`, `divine_champion`, `divine_monster`, `divine_mark` (P3-04 6.8's names). Data: a pantheon kind or tuning (`history.pantheon`), god names from the cultures (P3-02 god names). `--divine-activity` takes effect (5); `--divine-script` stays refused (5). Depends on P5-02 and P5-06.

*Amendment (Paul's scope decision of 2026-11-24, Q-P5-00-1: "take the recommendation except for mountain depths"; `docs/specs/p5-06-figures.md` 24):* P5-08 also takes cults (a god's followers and their sanctums), in place of P5-10's cults.

**P5-09 Casters, spells and spellbooks.** Some figures are casters; they found towers and libraries (`site` rows), invent named spells from the components of design 9 (school, effect, element, form, parameters), write spellbooks, and sometimes cause disasters such as a blight (design 5 "Magic in history"). Each race leans to certain schools (race field `magic_schools`, P3-00 8.1 reserved). Tables: `spell` (design 6 columns; parameters as integers), books written as events with author, spell and place (their present locations are P6's `spellbook`, 10.1); a blight as events and a by-year record of blighted regions (history never rewrites a geography layer, P3-00 7). Data: the magic components that history needs (schools, effects, forms, elements and the fixed spells), as data kinds design 9 already calls for; P5-09 defines the subset and the game's later rules extend it. Depends on P5-06; P5-02 for sites.

*Amendment (Paul's scope decision of 2026-11-24, Q-P5-00-1: "take the recommendation except for mountain depths"; `docs/specs/p5-06-figures.md` 24):* deferred: the minimal casters are in P5-06; spell invention, the `spell` table, `magic_schools` and blights by year wait for a later stage (after P5 or in P6), which extends P5-06's books (P5-06 19).

**P5-10 Factions.** Groups inside or across civilizations: noble houses, cults, bandit bands and orders (design 5 "Factions"), with mage orders tied to casters (design 9 "the best places belong to mage orders that history produced") and cults to gods. Table `faction` (design 6 columns plus start and end events); membership as `relation` rows between figures and factions. Event kinds: faction founded, split, outlawed, rose against its realm, ended. Depends on P5-06, P5-08 (cults) and P5-09 (orders).

*Amendment (Paul's scope decision of 2026-11-24, Q-P5-00-1: "take the recommendation except for mountain depths"; `docs/specs/p5-06-figures.md` 24):* deferred: cults go to P5-08; noble houses, bandit bands and orders wait for a later stage.

**P5-11 Artifacts.** Artifacts made in response to conflicts (proposals entry of 2026-11-19; design 5 "Waiting for phase P5"): each has a maker (a figure; a forge of P5-04; a caster of P5-09), an occasion (a cause link to a conflict event: a war declared, a siege endured, a capital sacked, a long enmity) and a chain of holders and places: carried to battle, captured in a sack, lost in a ruin, hidden, recovered. Where it lies at E is a record P6 turns into a site and a quest hook. Tables: `artifact`, `artifact_holding` (intervals by year, holder and place, each with its event). Entity type 10 `artifact` (new code, 6.1). Data: artifact kinds and naming in `culture`. Depends on P5-04, P5-06 and P5-09.

*Amendment (Paul's scope decision of 2026-11-24, Q-P5-00-1: "take the recommendation except for mountain depths"; `docs/specs/p5-06-figures.md` 24):* kept; its makers are P5-06's figures (casters, or smiths as a new role) and P5-04's forges, since the full P5-09 is deferred.

**P5-12 Regions renamed by history.** A region's defining event (a great sack, a long war, a founding, a plague, a monster's ravage) can give it its present-day name, with the old name kept on record (Paul, 2026-11-22, decision 1). Table `region_name` (region, from year, name, culture, event); `region.name` stays geography's and is never rewritten (P3-00 7). `render_event` and the report name a region by the name in force in the event's year (a P3-04 rendering amendment), so an old event keeps its old name. Which kinds can rename is data (a weight per kind key), so stages that merge later add their kinds without touching this subsystem. New names go through the registry and the filter (4.5). Depends on P5-01 only.

### 3.2 Order, dependencies and parallel work

| Stage | Depends on | Can be implemented in parallel with |
| --- | --- | --- |
| P5-01 Carried-over follow-ups | — | (alone) |
| P5-02 Beasts and monsters | 01 | 03, 12 |
| P5-03 Race relations, mixed settlements, migrations | 01 | 02, 12 |
| P5-04 Wealth, trade, alliances; mines and forges | 02, 03 | 06 |
| P5-05 Roads and improved passes | 04 | 06, 07 |
| P5-06 Figures | 02 | 04, 05, 07 |
| P5-07 Mountain depths | 02, 04 | 05, 06 |
| P5-08 Pantheon and divine interventions | 02, 06 | 05, 07, 09 |
| P5-09 Casters, spells, spellbooks | 06 (02) | 07, 08 |
| P5-10 Factions | 06, 08, 09 | 11 |
| P5-11 Artifacts | 04, 06, 09 | 10 |
| P5-12 Regions renamed by history | 01 | 02 to 11 |

- **Specs** may be written ahead of their stage, in any order, since the rules they depend on are fixed by earlier specs, not by code.
- **Implementation** runs in parallel where the table says; **merges are serial**: every merge changes world bytes, so each merges onto the previous one, re-records the golden values and passes the cross-preset comparison again. The lead fixes the merge order; it is the stage order above unless the lead says otherwise.
- Every merge leaves a valid, complete world: a stage never needs a later one to make sense of its rows. Where a later stage adds a role to an earlier one's events (heroes slaying monsters, a champion), it adds a new kind (P3-04 8.2), and the earlier kind stays as it was.
- The required orderings of the brief hold: wealth and trade (04) before forged arms in trade and artifacts (04, 11); figures (06) before factions (10) and casters (09); monsters (02) before the deep dangers (07); roads (05) after trade (04).
- **P5-12** may merge at any point after P5-01; recommended straight after it, in parallel with 02 and 03, so that renamed regions reach Paul early (question 3).

## 4. Shared rules for every P5 stage

### 4.1 One subsystem per stage

- Each stage after P5-01 adds one `Subsystem` (P3-04 6.4) in its own source files under `worldgen/src/history/`, with its private state in its own members. It may also add an `AwakeningHandler` hook only through P3-05's (there is one handler), a `TensionSource` (P5-02, P5-08) and, for P5-08 only, the `InterventionSource`.
- Steps: a subsystem lists only subsystem steps (T1 to T4, T6, Q1, Q3, Q4); T3 is the figures' step (P5-06), and others may share it. The engine's steps T5, T7, Q2, Q5, Q6 stay the engine's.
- **Run order**: `EngineConfig::subsystems` is peoples, conflict, then the P5 subsystems in merge order (P3-04 6.9 rule 3). A stage that must run before an existing subsystem in a shared step says so and why; it changes every world, which a stage does anyway.
- **Changes to P3 subsystems**: a P5 stage that changes a rule of P3-05 or P3-06 (predators slowing growth, an ally joining a war, roads lowering crossing costs) does it through a seam named in its spec and amends the P3 spec in the same branch, as P3-09 to P3-11 did. It never reaches into another subsystem's members.
- **Shared-step visibility** (P3-04 6.9 rule 4): each spec states what it sees of other subsystems' same-step changes.
- **Quiet steps**: each spec states which of its events a quiet step may write and the bound on them. World-level events with no civilization charged (a monster in the wild, a god) count against a per-subsystem bound of their own, stated in the spec (P3-04 W9 charges civilizations only).
- **Persistence**: figures, monsters and artifacts persist through quiet steps (design 5); births, deaths and decay over a quiet step are computed for its whole length, as Q1 is, not skipped.

### 4.2 Draws and determinism

Keyed draws only (H4, lint `history-sequential-rng`); stream names `hist.<subsystem>.<decision>`, registered in `streams.hpp` and listed in the spec; keys are entity ids or ids stored in the world (P3-04 6.9 rule 5). No new domain tag. Every new entity table has dense ids from 1 in creation order (H2) and its own bounds (H6). Every order is total and ends in an id (H5). P3-04 6.9 rules 1 and 2 apply to every new founding or relation call. Every P5 change goes through the determinism auditor.

### 4.3 Schema changes

- A stage that adds a table, a column or an entity type code is a schema change: one schema version bump per merged branch (design 6), a `docs/specs/schema.md` update, and its own proposals entry (hard rule 7). The version number is the next free one **at merge**; a spec says "the next schema version", and the lead records the number. Expected: P5-01 none (schema 7); P5-02 schema 8; most later stages one bump each.
- New tables follow P3's conventions (P3-00 6.1): STRICT, no REAL, no BLOB except integer-coded blobs specified like `pass.path` (road points), ids from 1, 0 = none, NULL only for a year that has not happened, rows in a defined order.
- **State by year**: where design 6 gives a table "at the end year" (`race_relation`, `beast_population`), the stage also writes rows by year in the manner of `settlement_census` or `region_control` (P3-00 6.4), so that the viewer's timeline (P4) can answer any year without replaying the log (interim reading; proposals entry).
- **Entities that history creates and ends** carry their start and end years and events (as `civilization` does: `founded_year`, `founding_event`, `ended_year`, `ending_event`), so every fact traces to an event (design 14).
- **The viewer**: the Godot extension reads worlds through the `worldgen` library, which accepts exactly the current version (design 6), so a bump reaches it with the library. A stage that bumps the schema keeps `gdext-headless-check` green in its branch, and tells the viewer engineer which tables it added (P4 runs in parallel).

### 4.4 Event kinds and their numbering

- New kinds are **appended** after kind 40 (`settlements_ceded`, P3-11), never renumbered (P3-04 8.2). Ids are assigned in **merge order**: P5-01's kinds take 41 onward, each later stage the next free ids when it merges.
- A spec names its kinds by key and lists them in the order they are to be appended; its ids are provisional ("next free") until the lead fixes the merge order. Code emits kinds by key (`EventSpec::kind`), and the report finds them by key (P3-07 21.2), so a stage that merges in a different order than planned only re-orders its own catalogue entries before merge. Test references and golden files use keys wherever they can; the table hashes are re-recorded at merge anyway.
- An existing kind that needs another role or value gets a new kind (P3-04 8.2). Expected cases: a battle with an ally's army, a battle led by a general, a monster slain by a hero.
- **New entity type codes** after 9 are appended in merge order (expected: 10 `artifact` with P5-11; others only if a stage needs an entity no existing type names). Using a reserved code (6 to 9) for the first time is still a schema change, since the table it points to is new.

### 4.5 Names

New name kinds (persons, monsters, artifacts, spells, factions, sites, roads, renamed regions) are added to P3-02's builder as name kinds with templates in each `culture`; they are unique across the world and across kinds, pass the name filter, and are drawn by keyed draws in creation order. A name kind a culture lacks is a load error naming the culture, never a silent fallback. A stage that adds a name kind measures its rejects and its names' quality on the sweep (the critic reads them).

### 4.6 Data

New TOML kinds and fields follow P0-03 and enter `content_hash` (every merge changes it). Expected new kinds: `beast`, `monster` (P5-02), the magic components (P5-09); new `history` tuning ids per subsystem (`monsters`, `relations`, `economy`, `roads`, `figures`, `depths`, `pantheon`, `magic`, `factions`, `artifacts`, `names`; the spec fixes the id). Event rules stay code with their numbers in TOML, as in P3 (P3-00 15 question 6): no P5 stage has yet shown a rule that needs to vary by data. This stays open for the design conversation and is not a P5 deliverable (interim; design 5 "Whether event rules move from code into data").

## 5. Settings

| Setting | `meta` key | Comes into force with | Effect (outline) |
| --- | --- | --- | --- |
| Monster pressure | `param.monster_pressure` | P5-02 | `low`, `normal`, `high` scale the rates of monster waking and raids and the predator caps of beasts, by factors in `history.monsters` (interim 500, 1000, 1600 per mille) |
| Divine activity | `param.divine_activity` | P5-08 | `distant`, `normal`, `meddling` scale the rate of interventions, by factors in `history.pantheon` (interim 300, 1000, 2500 per mille); at `distant` gods still act, rarely |
| Event intensity | `param.event_intensity` | (P3) | P5 stages may also scale their own pressures by it; each spec says whether |
| Divine script | `param.divine_script` | not in P5 | stays refused ("divine scripts need phase P5" becomes "divine scripts are not supported yet"); P5-08 fixes the intervention kinds and the validation a script source would use (design 14 places the script after the prototype) |

Both keys are already in every world's `meta` (P3-00 5), so neither needs a schema bump. Until its stage merges, a setting is stored and has no effect; from that merge on, each value gives its own world bytes, and `world-hashes.sh` gains one case per setting (`p5-monsters-high-1024`, `p5-divine-meddling-1024`, P5-02 and P5-08) so that the path is compared across compilers. Design 13's P6 row lists "world-creation settings wired to the command line"; P3-01 has done that, so P6 has nothing left there but the divine script (proposals entry).

## 6. Schema and catalogue plan

### 6.1 Tables by stage (outline; each spec fixes the DDL)

| Stage | Tables added or changed | Entity types | Relation kinds |
| --- | --- | --- | --- |
| P5-01 | none (appended kinds only) | — | — |
| P5-02 | `beast`, `monster_kind`, `beast_population`, `monster`, `site` | 8, 9 in use | — |
| P5-03 | `race_relation`, `settlement_race` | — | — |
| P5-04 | none (site kind 3 `mine`, schema 11; P5-04 r2 supersedes `civilization_census`, 3.1's note) | 9 (mines) | `trade`, `alliance` (`vassal` deferred) |
| P5-05 | `road` | — | — |
| P5-06 | `figure` | 6 in use | ties between figures (kinds named by the spec) |
| P5-07 | none expected (kinds, monster rows, site kinds) | — | — |
| P5-08 | `god` gains rows and `domain` (written by `chronology`) | — | — |
| P5-09 | `spell`, a by-year blight record | — | — |
| P5-10 | `faction` | 7 in use | membership (figure, faction) |
| P5-11 | `artifact`, `artifact_holding` | 10 `artifact` | — |
| P5-12 | `region_name` | — | — |

Design 6's `spellbook` and `quest_hook`, and the dungeon columns of `site` (`layout_seed`), are P6's (10.1).

### 6.2 What every new entity row records for P6 and for narration

Its name; its start and end years and events; the place it is tied to (region, and a settlement, site or cell where one exists); its state at E (alive or ended, where it lies, who holds it); and its participation in events through `event_participant`, so that a kill list, a line of rulers or an artifact's journey is a query on the log, not a stored list (design 14 "structure from the simulation"). A stored summary of something the log already holds is added only where the viewer needs it by year (4.3).

## 7. The history report

### 7.1 How new kinds enter the report

- **Salience and template**: every new kind ships its base salience and template in `history.report` (P3-04 8.1), with templates passing `template_problem`. Salience is set against the shipped scale: 100 awakening, 90 a capital sacked, 35 claims, below 20 never in the summary (P3-07 21, 24). A stage states for each kind whether it is meant for the summary.
- **Budget**: the summary keeps its fixed budget (at most 250 events, at least 20 per age, P3-07 24.3). New kinds compete for it, so each stage measures the summary's make-up over 1024 seeds 0–39 (share of lines by subsystem, wars still shown with their endings, ages still at the floor) and keeps P3's conflict lines readable; a stage that crowds wars out of the summary lowers its own salience.
- **Companions**: a kind that reads only with its head (an ally joining a war, a hero's slaying with the monster's death, an artifact captured in a sack) is added to P3-07's companion table (24.2) by an amendment in the stage's branch. Companions are still not followed further.
- **`[stats]`**: each stage appends its keys (P3-07 10.3 rule 1), including those of the exit check (8.3); `events_<key>` lines appear by themselves.

### 7.2 Blocks and the format version

P5-01 moves the format to `# worldgen history v2` (3.1) and amends P3-07 10.3 with one growth rule for P5: **within v2**, a stage may (a) append a block named for its entity kind (`[monsters]`, `[figures]`, `[factions]`, `[artifacts]`, `[roads]`) after `[present]` and before `[stats]`, in merge order, and (b) append new line types at the end of a `[civilizations]` block (wealth, allies, rulers) or of `[present]`. Any other change still needs a new version. Every new block has a summary form short enough that a default world's summary stays under about 1,000 lines (B4's concern), measured by the stage.

## 8. Budget and validation

### 8.1 Performance budget

Measured: the whole `generate` at 1024 takes about 9.3 s (release, quiet machine, generator 0.19.0; 0.20.0 within noise), 2.7 s at 512 and 37.5 s at 2048. Design 13's limit for the default world is 2 minutes.

| | 1024 (reference) | 2048 |
| --- | --- | --- |
| Each P5 stage adds at most (release, seed 0, three runs) | 5 s | 20 s |
| Whole `generate` at the end of P5 | **60 s** | **4 min** |
| `report --history` | 2 s | 8 s |
| History tables | 8 MB (interim; P3's bound was 5 MB) | — |

A stage over its share is a defect to report; the lead may trade shares between stages. Span 12,000 and `dense` may take twice as long; no budget applies to debug or sanitizer builds.

### 8.2 Sweeps per stage

The P3 sweeps (P3-00 12.2): 1024 seeds 0–199 (the reference), 512 seeds 0–49, 2048 seeds 0–19, `calm` and `turbulent`, four civilizations per race, `--races` subsets; plus the settings a stage brings into force (5) at each value. Each stage reports:

1. its own statistics, with hard and typical ranges, from `[stats]`;
2. its part of the participation table of 8.3;
3. **regressions** against the previous merge, pair for pair on the same seeds: wars per world, wars by race, ruins, settlements standing at E, `share_max_permille`, orc statistics T10 and the one-settlement bound (P3-11 ruling), flights and refuges; a change beyond the P3 typical ranges is explained or is a defect;
4. P3's hard bounds (P3-00 12.3), unchanged and still defects on any seed;
5. timing (8.1) and the summary's make-up (7.1).

### 8.3 Towards the exit check

"Takes part" is read as: is a participant of at least one event of a P5 subsystem, or of P3's (interim). Measured on the 1024 reference sweep with default settings; the stage that brings each item in sets its typical target, and the lead confirms it.

| Item | Hard (every world, a defect otherwise) | Over the sweep |
| --- | --- | --- |
| Every present race | founds a civilization (P3); takes part in at least one event of a P5 subsystem once P5-03 has merged | each race takes part in each P5 subsystem that concerns it in at least 90% of worlds (dwarves in mines and forges, P5-04) |
| Every god camp | — | from P5-08: each camp intervenes in at least 90% of default worlds and in every world at `meddling`; every camp at least once at `distant` across the sweep |
| Every monster kind | — | from P5-02: each kind in the `monster` data appears in at least 10% of worlds and in at least one event; every kind at least once across the sweep at `low` |
| Not empty | P3-00 12.3 | — |
| Not runaway | P3-00 12.3, and no **bloc** (a civilization with its allies and vassals, P5-04) holds more than 800‰ of non-islet regions for more than 1,000 consecutive years; no monster destroys more than a quarter of a world's settlements; event count ≤ 200 per simulated year | the largest bloc's share is reported per world with typical ranges |

The phase's last stage reports the whole table; Paul's review of P5 reads it with the critic's review.

### 8.4 Golden cases

The existing cases of `world-hashes.sh` (fifteen) are re-recorded at every merge. Added: the two settings cases of 5. Each stage adds golden vectors of its own, written by the test writer's independent reference in `worldgen/tests/reference/` (`p5-NN-<stage>.txt`), in the manner of P3-04 to P3-06, with the large cases in the `*-slow` suites.

## 9. The design's open questions

### 9.1 History renames regions (accepted by Paul, 2026-11-22)

Scheduled as P5-12. The record of renamed places (`region_name`) is what P6 sites and quests use. The lower share of grim drawn names is P5-01 (c).

### 9.2 The mountain peoples' own history (accepted by Paul, 2026-11-22)

Scheduled in two parts: mines, forges and arms in trade with wealth (P5-04), and delvings and the deep dangers with lost holds (P5-07), after monsters (P5-02).

### 9.3 Sub-peoples (still open)

Recommendation: no sub-peoples inside P5's stages. Hill dwarves fit the present geography and the schema already allows several cultures per race (`culture`, `civilization.culture`), so they can be a small stage after P5-07 (a second dwarven culture with upland-hill preferences, taken by a second civilization or a secession), if Paul wants them. Dark elves need underground ground, which geography lacks; they wait for the dungeon and underground work (P6 or later). **Interim**: one culture per race; every P5 stage keys its race-specific rules by race and culture data fields, never by race key in code, so that a second culture is a data change.

## 10. What P5 leaves to P6, and records for it

### 10.1 The split

| P6 derives | What P5 records for it |
| --- | --- |
| `site` rows from the log (battlefields, barrows, tombs, sacked towns as ruins) and the dungeon records with `layout_seed` | battle and sack events with region, settlement and pass (P3); a figure's place of death (P5-06); P5's own `site` rows for places history acts on: lairs, mines and delvings, towers and libraries, shrines marked by gods, lost holds |
| Dungeon origin types (tomb, mine, fallen fortress, cult sanctum, lair) | the site's kind and origin event; the faction of a sanctum (P5-10); the monster of a lair (P5-02) |
| Quest hooks from unresolved tensions | a stolen artifact never recovered (P5-11), an heir in exile (P5-06), a lair within reach of a village (P5-02), a lost expedition (P5-07), a standing claim (P5-01) |
| Spellbook placement (`spellbook`) | spells with inventor and year (P5-09); each book written, with author, spell and place, and the events that moved or lost it |
| Lore rendered from records | every entity's name, years, place and events (6.2) |

**Interim** (question 1): `site` is created by P5-02 for the places history itself acts on; P6 adds the derived sites and the dungeon columns with its own schema bump. The design places sites in P6 (design 13) but has history refer to them (design 5 "Passes are stored as sites"; design 6 `monster.lair_site_id`).

### 10.2 Rule for every P5 stage

A spec lists in a section "For P6" what of its output P6 will turn into sites, hooks or placements, and checks that the record is enough without replaying the simulation.

## 11. Open questions for Paul

Only those that change the split or the order; each with the recommendation and the interim choice the specs follow until Paul decides.

1. **Sites in P5 or P6.** History needs places to refer to (lairs, mines, towers, marked shrines, lost holds) before P6. Recommendation and interim: P5-02 creates `site` for the places history acts on; P6 adds derived sites and dungeon records. The alternative, positions stored on each entity and every `site` row made in P6, moves no stage but leaves gods unable to mark a site until P6.
2. **Gods late, with all five kinds.** Recommendation and interim: the pantheon and interventions come as P5-08, after monsters and figures, so that bless, curse, champion, monster and mark arrive together and the intervention list is final for a later divine script. The alternative is an early stage with bless and curse only, and the other three added by later stages.
3. **Renamed regions early.** Recommendation and interim: P5-12 merges right after P5-01, in parallel with P5-02 and P5-03; the kinds that can rename are data, so later stages add theirs. The alternative is to wait until the end so the first renaming rule sees every kind.
4. **Sub-peoples.** Recommendation and interim: none in P5's stages; hill dwarves as an optional small stage after P5-07 if wanted; dark elves after underground geography (9.3).

## 12. Readings taken here (proposals entry)

Recorded in `docs/design/proposals.md`, "History depth: the P5 overview's readings": the stage split and order (3); one subsystem per stage (4.1); kind ids and schema versions assigned in merge order (4.3, 4.4); P5-01's records as event kinds, not columns; tables by year beside the design's end-year tables (4.3); a civilization's principal race with mixing per settlement (P5-03); `site` in P5 (10.1); `spellbook` placement in P6; the report's v2 and its growth rule (7.2); the settings' effects and the divine script kept refused (5); event rules still code (4.6); the budget (8.1); the exit check's measures (8.3); a deep horror respecting the flight of a last refuge (P5-07); design 13's P6 row and settings already wired.
