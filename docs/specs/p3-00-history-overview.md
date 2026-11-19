# P3-00 History overview

Status: **approved by the lead 2026-11-19, with amendment A1** (the event catalogue is stored in the world and exposed by the library: 6.1, 6.3, 11). Corrected the same day while writing P3-01 and P3-02: the version plan (3.2), where the `meta` keys are written (5, 6.1) and which spec introduces the keyed draws (4.6). Phase P3 History core, the counterpart of `p1-00-geography-overview.md`. Design doc sections 5 (History simulation), 6 (World database), 11 (milestone 1), 12 (open questions), 13 (phase P3 and what P4 and P5 add) and 14 (narrative enrichment, the divine script). Depends on P0-01 to P0-04 with their amendments and on the finished geography (P1-00 to P1-06, generator 0.11.0, schema 5). Proposals read: "Paul's decision of 2026-11-19: P1 Geography approved; the critic's recommendations accepted" and the entries before it. Numbers measured for this spec are from the P1-06 validation worlds (`~/.cache/the-unmapped/out/p1-06-validation/`, generator 0.11.0: 512 seeds 0–49, 1024 seeds 0–199, 2048 seeds 0–19) with the script `~/.cache/the-unmapped/out/p3-00-spec/homes.py`.

This overview fixes what every P3 stage spec inherits: the breakdown into specs, the calendar and the simulation loop, the random streams and determinism rules of a simulation, the schema plan, the data kinds, what history reads from geography, the history report, the budget and the validation plan. It does not specify any subsystem's rules: those are the stage specs' (section 3). Values marked "interim" are this spec's reading where the design is silent; section 15 lists them for Paul.

## 1. Purpose

Phase P3 turns a finished geography into a history: six races awaken in a fixed order at scheduled times, found civilizations and settlements, expand over regions, meet, fight, sack and abandon, across a span of thousands of years, and every change is written to an event log with links to its causes. The log is the primary product (design 5); the end state (who holds which region, which settlements stand or lie in ruin) is what remains after the last tick.

Exit check (design 13): the default world (1024, 6,000 years) generates in under 2 minutes; races appear in the fixed order at the scheduled times; golden-seed tests pass. In addition, from this overview: no world ends empty or runaway (section 12.3), and the history report reads as a history to the world critic.

**Not in P3** (P5 and later; P3 leaves the seams of section 9): god interventions beyond creation, beasts, named monsters, figures and factions, race relations beyond the starting camps, mixed settlements, casters and spells, roads and improved passes, sites, quest hooks, cultural region names. P4 (viewer timeline) reads P3's tables; P3 makes them cheap to read by year (section 6.4).

## 2. Scale, time and the calendar

### 2.1 Years

| Quantity | Value |
| --- | --- |
| Time unit | the **year**, an integer. No months or days in the database. |
| Year 0 | the first elven awakening, the start of history |
| Present | year **E** = the history span (`param.history_span`, default 6000); the simulation ends after year E − 1 is processed and the world state is "as of year E" |
| Order within a year | event `id` order (section 6.2); ids are assigned in processing order, which is fully defined (section 4) |
| Spans | an event that summarizes a stretch (a quiet interval) has `year` = its first year and `end_year` = its last year; a point event has `end_year` = `year` |
| Ages | age k runs from race k's awakening year to the year before race k+1's (the last age to E − 1); age ids 1–6 in race order |

All years are in [0, E]. A calendar shown to players (eras, "in the 3rd year of the Goblin Age") is derived at runtime or by the report; the database stores plain years.

### 2.2 Abstract scale

Populations, armies and casualties are simulated figures, integers (design 4 "Scale is compressed": a city of 20,000, a host of 3,000). A settlement occupies one cell (its centre), lies in one region and is owned by one civilization. Territory is held **by region**: a region is controlled by at most one civilization at a time (section 6.4). Distances between regions are counted on the region graph (`region_border`), with crossing costs from the border columns (section 10).

### 2.3 Map sizes

**1024 is the reference size** for tuning, for judging history and for the 2-minute budget (Paul's decision of 2026-11-19). Measured at 1024 (200 worlds): 42–59 regions (median 49), of which 16–43 are lowland regions with at least half walkable ground and `habitability_mean` ≥ 60 (median 30); upland regions (4 × `walk_cells` < `cells`) with reachable ore 0–9 (median 3); regions with at least a quarter fen, marsh or wet woodland 0–10 (median 1). At 512 (50 worlds): 11–16 regions, 0–11 lowland regions (median 5); at 2048 (20 worlds): 182–205 regions, 138–191 lowland.

- **512** must run and never fail: fewer civilizations (section 10.3 scales the count by habitable land), races may share a home region's neighbourhood, and a race with no suitable land still awakens at its best available region. No quality bar beyond the hard bounds of section 12.3.
- **2048** must run within its own budget (section 11); its histories are larger, not longer. Tuning targets 1024.

## 3. Stage specs of P3

### 3.1 Breakdown

Two new pipeline stages, `chronology` and `history`, follow `regions`. The `history` stage is one simulation loop; its subsystems are separate specs with separate seams, each testable on a hand-made state, so that one engineer and one test writer can take each.

| Spec | Title | Pipeline stage | Reads | Writes | Depends on |
| --- | --- | --- | --- | --- | --- |
| P3-01 | History data and settings | (loader, `generate` options) | TOML kinds `race`, `culture`, `history` tuning (section 8); world-creation settings | `meta` keys `param.history_*` (6.5); registers the kinds; no table | P0-03 |
| P3-02 | Names | (library) | culture naming data | a function: (culture, kind, key) → name; uniqueness per world | P3-01 |
| P3-03 | Chronology | `chronology` | settings, `race` data, seed | tables `race`, `race_biome`, `culture`, `god`, `age` (**schema 6**) | P3-01, P3-02 |
| P3-04 | Simulation engine | `history` (skeleton) | the whole stored world (geography, chronology tables), `history` tuning | tables `event_kind`, `event_kind_role`, `event_kind_value` (A1), `phase`, `event`, `event_participant`, `event_cause`, `event_value`, `civilization`, `settlement`, `settlement_census`, `region_control`, `relation` (**schema 7**, the full P3 DDL); the scheduler of active phases, quiet intervals and flare-ups; the event log writer; awakening events | P3-03 |
| P3-05 | Peoples | `history` (subsystem) | region tables, `race` preferences | homeland choice at awakening, civilizations, settlement founding, growth and decline, migration, abandonment, quiet-interval drift | P3-04 |
| P3-06 | Expansion and conflict | `history` (subsystem) | engine state | claims of regions, tension between neighbours, war and peace, battles, sieges, sacking, ruin, conquest; the tension measure that triggers flare-ups | P3-05 |
| P3-07 | History report | `report --history` | the stored world | text (section 11) | P3-04 (schema 7); its content grows with 05 and 06 |

### 3.2 Order and parallel work

- **P3-01 and P3-02** can be written and built in parallel (P3-02 needs only the naming fields of `culture`, which P3-01 fixes first).
- **P3-03** after both. Merge 1: schema 6.
- **P3-04** after P3-03. Merge 2: schema 7, with the full DDL so that P3-05 and P3-06 add rows and rules but no columns. The engine merges with stub subsystems: awakenings place one civilization with one settlement per race; nothing grows or fights. That world already passes the exit check's order and timing items.
- **P3-07** can be written and implemented in parallel with P3-05 and P3-06 once P3-04 is merged; it reads tables only.
- **P3-05, then P3-06.** Their specs can be written in parallel (P3-06 depends on P3-05's state but not its rules); P3-06 is implemented after P3-05 merges, since conflict needs settlements and territory.

Each merge that changes world bytes bumps `GENERATOR_VERSION` (MINOR) and re-records the golden values, as in P1. Version plan: **0.12.0 (P3-01**: the new data kinds change every world's `content_hash`; no schema change**)**; P3-02 adds library code and the culture naming data, which P3-01's files already carry, so it changes no world byte and needs no bump; 0.13.0 (P3-03, schema 6); 0.14.0 (P3-04, schema 7); 0.15.0 (P3-05); 0.16.0 (P3-06); P3-07 changes no world byte (its report templates are written by the `history` stage from P3-04 on, A1), so it bumps only if it changes their data.

## 4. The simulation model

### 4.1 Phases

The span [0, E) is cut into a **schedule** of phases, each a row of table `phase`:

| Kind | Code | Steps | Written to the log |
| --- | --- | --- | --- |
| Opening active phase | 1 | yearly ticks, from an awakening year for `active_years` years (tuning, by `history_detail`) | the detailed log |
| Quiet interval | 2 | quiet steps of `quiet_step_years` (interim 25) years each, from the end of an active phase to the next awakening (or to the final phase) | a few summary events per step at most (design: "four centuries of peace under the same dynasty" is one event spanning several steps; section 4.4) |
| Flare-up | 3 | yearly ticks for `flare_years` years, inserted into a quiet interval when the tension measure crosses `flare_threshold` at the end of a quiet step | the detailed log; `phase.trigger_event` names the event that raised the tension |
| Final active phase | 4 | yearly ticks over the last `final_years` years, ending at E − 1 | the detailed log |

Rules: phases tile [0, E) without gaps or overlap, in order; the opening phase of age k starts exactly at race k's awakening year; if two awakenings are closer than `active_years`, the earlier opening phase ends at the later awakening; a quiet interval shorter than one quiet step is a single shorter step; a flare-up never extends past the next awakening or the start of the final phase (it is cut there). The quiet interval resumes after a flare-up as a new `phase` row. The design's "1,200 simulated years of 6,000" is the expected sum of active, flare-up and final years at `history_detail` normal; P3-04 fixes the tuning so that the median over the 1024 sweep is 1000–1400.

**Interim** (proposals entry): the design says a quiet interval advances "in one jump", and also that it "can be interrupted" when tension crosses a threshold. A single jump cannot be interrupted part-way, so a quiet interval is a sequence of quiet steps (25 years each by default) with the tension checked after each.

### 4.2 The yearly tick (active phases and flare-ups)

The design's tick loop, in this fixed order; each step is a function of the state at the start of the step and the keyed draws of 4.4:

| Step | Design | P3 owner | P3 content |
| --- | --- | --- | --- |
| T1 | settlements grow or shrink | P3-05 | population toward carrying capacity; decline and abandonment |
| T2 | civilizations act on goals | P3-05, P3-06 | found a settlement, claim a region, declare war, sue for peace (roads: P5) |
| T3 | figures | — | reserved for P5 (no-op) |
| T4 | random pressures | P3-05 | plague, famine, succession crisis (monster incursion, blight: P5) |
| T5 | divine interventions | — | **seam** (section 9.2); empty in P3 |
| T6 | consequences resolve | P3-06 | battles, sieges, sackings, conquest, migrations, ruin |
| T7 | bookkeeping | P3-04 | close and open `region_control` and `relation` intervals; census rows at phase ends; tension update |

Step 6 of the design ("every change is appended to the event log") is not a step: every step appends its events as it makes its changes (4.3). Within a step, entities are processed in ascending id; pairs (a, b) with a < b in lexicographic order; a step that needs a different order (for example by priority) sorts by a strict total order ending in the id (P1-00 D7). An entity created in a step is processed from the next step on, not in the step that created it.

**Awakenings** are not a tick step: at the start of an awakening year, before T1, the engine (P3-04) appends the awakening event and P3-05 places the race's civilizations (10.3).

### 4.3 State and the event log

The simulation state is in memory: arrays indexed by id for each entity kind (civilization, settlement, region state), and ordered maps (`std::map` keyed by integers or id tuples) for sparse relations. Nothing is read back from the database during the loop. The event log is appended in memory and written in id order at the end, in one transaction with the other history tables (P0-02 rule: rows in a defined order).

Every state change that history must explain is an event. An event is created with its kind, year (and end year), place (region, settlement), participants with roles, numeric values, and causes; causes are ids of earlier events (strictly smaller ids). The kinds, roles and value keys form the **event catalogue** (6.3), part of the schema contract like the layer catalogue.

### 4.4 Quiet steps

A quiet step of length L years applies, in this order: Q1 population drift toward capacity over L years in closed form (P3-05; integer, no per-year loop needed); Q2 generational turnover counts (ruling lines as a count of generations: `race.generation_years`); Q3 ruin decay (ruined settlements past `ruin_fade_years` are marked faded; they stay rows); Q4 slow expansion: at most `quiet_claims` claims of free neighbouring regions per civilization per step (P3-06), no wars; Q5 the tension measure (P3-06) is evaluated; if ≥ `flare_threshold`, a flare-up starts at the next year. A quiet step writes at most a fixed number of events (`quiet_events_max`, interim 3 per civilization per step), mostly summaries; consecutive quiet steps of one civilization with no change merge into one summary event spanning them (the "four centuries of peace" record), written when the run of quiet steps ends.

### 4.5 Determinism rules specific to simulation

In addition to P1-00 section 8 (which still applies to any per-cell work a history stage does):

| Rule | Requirement |
| --- | --- |
| H1 Integers only | every quantity is an integer or `Fx`; probabilities are `Fx` in [0, ONE] or integer per-mille; no floating point anywhere in the history stages or the report |
| H2 Ids | dense, starting at 1, assigned in creation order; creation order is defined by the step order, entity id order and the rule "processed from the next step on" (4.2). 0 means "none" in every id column |
| H3 Order | iterate entities by ascending id, pairs by (a, b); never iterate an unordered container; maps are ordered by integer keys only; no pointer comparison or address-dependent order |
| H4 Draws | every random decision is a **keyed draw** (4.6), never a draw from a shared sequential generator; so adding a decision elsewhere does not shift any other decision |
| H5 Tie-breaks | every argmax, argmin and choice among equals is resolved by a stated key ending in an id (or a cell index) |
| H6 Overflow | each spec states bounds; populations ≤ 2^31, sums in `int64`; a bound exceeded is fatal, never wrapped |
| H7 Single thread | the loop is sequential (D4). A subsystem may parallelize a per-region scoring pass by D1 or D8 only if its spec shows the result does not depend on the split; none is expected to need it |
| H8 Pure function | the history is a function of the stored geography, the chronology tables, the loaded data, the settings and the seed; a history stage reads only the stored world (P1-00 5.1), so `--until chronology` then `history` on the read-back world equals one run |

### 4.6 Random streams

Stream names: `hist.<stage or subsystem>.<decision>` (P0-01 7.3 grammar), registered in `streams.hpp`; each spec lists its names, as P1-00 3 did for geography. A keyed draw is

```
draw(name, year, a, b, k) = hash_words(TAG_HIST, [stream_key(seed, name), year, a, b, k])     // uint64
```

with a, b the ids of the entities the decision is about (0 if none) and k a small counter for several draws of one decision. `TAG_HIST` (`UNM:HIST`) is a new domain tag (P0-01 amendment). **P3-02 introduces it and the helpers** (it merges before P3-03 and P3-04 and is the first user of keyed draws; P3-02 4). Helpers: `chance(u, p)` = (u >> 32) < p.raw for `Fx` p in [0, ONE]; `pick(u, n)` = the index (u >> 32) × n >> 32 (integer, no modulo bias beyond 2^-32); `weighted(u, weights)` = the first index whose prefix sum exceeds (u >> 32) × Σ >> 32, over weights in a defined order. Chronology uses the same scheme with year 0.

## 5. Pipeline, settings and `--until`

| # | Stage | Spec | Reads | Writes |
| --- | --- | --- | --- | --- |
| 7 | `chronology` | P3-03 | settings (`meta`), `race`, `culture`, `history.chronology` | `race`, `race_biome`, `culture`, `god`, `age` |
| 8 | `history` | P3-04 to P3-06 | everything above, `history.*` tuning | the history tables of 6.1 |

`generate` runs every implemented stage by default; `--until regions` gives a geography-only world (the history tables exist and are empty), `--until chronology` stops after the schedule. `meta.stages` lists the stage names. The thread count does not affect history (H7).

**Settings** (design 5, "World-creation settings"; P3-01 adds the flags and settings-file keys in `[world]`, P0-03 6 and P0-04 4; exact forms and messages in P3-01 6):

| Setting | Flag, settings key | `meta` key | Default | Range | In P3 |
| --- | --- | --- | --- | --- | --- |
| History span | `--history-span`, `world.history_span` | `param.history_span` | 6000 | 2000–12000 | used |
| History detail | `--history-detail`, `world.history_detail` | `param.history_detail` | `normal` | `sparse`, `normal`, `dense` | used (phase lengths, flare threshold) |
| Civilizations per race | `--civs-per-race`, `world.civs_per_race` | `param.civs_per_race` | `auto` | `auto`, 1–4 | used (`auto`: 1–2 from the seed, scaled by land, 10.3) |
| Races present | `--races`, `world.races` | `param.races` | all six | a set of race keys, written in race order, comma separated | used |
| Event intensity | `--event-intensity`, `world.event_intensity` | `param.event_intensity` | `normal` | `calm`, `normal`, `turbulent` | used (pressure rates) |
| Monster pressure | `--monster-pressure` | `param.monster_pressure` | `normal` | `low`, `normal`, `high` | stored, unused until P5 |
| Divine activity | `--divine-activity` | `param.divine_activity` | `normal` | `distant`, `normal`, `meddling` | stored, unused until P5 |
| Divine script | `--divine-script FILE` | `param.divine_script` | empty | SHA-256 of the script file, lower-case hex, or empty | refused in P3 ("divine scripts need phase P5"); the key exists so that the world records "no script" (9.2) |

P3-01 adds the flags and settings-file keys and validates them; **the eight keys are written into `meta` from P3-03 on, with schema 6** (a new required `meta` key is a schema change, and P3-03 makes the one schema 6 bump; P3-01 changes only `content_hash`). From schema 6 every world records them, a geography-only world (`--until regions`) included; P5 changes their effect, not the schema.

## 6. Schema plan

### 6.1 Versions

| Version | Branch | Adds |
| --- | --- | --- |
| 6 | P3-03 chronology | `race`, `race_biome`, `culture`, `god`, `age`; required `meta` keys `param.history_*`, `param.civs_per_race`, `param.races`, `param.event_intensity`, `param.monster_pressure`, `param.divine_activity`, `param.divine_script` |
| 7 | P3-04 engine | `event_kind`, `event_kind_role`, `event_kind_value` (A1), `phase`, `event`, `event_participant`, `event_cause`, `event_value`, `civilization`, `settlement`, `settlement_census`, `region_control`, `relation` |

P3-05 and P3-06 add rows, event kinds and value keys to the catalogue (a catalogue addition is not a schema change, as a layer addition is not: P1-00 4 rule 3), and no columns. A column they find they need is a schema change and goes through the lead. The exact DDL is P3-03's and P3-04's; the outline below is binding on their content (column names may still be adjusted by those specs).

| Table | Key columns (outline) | Row order | Notes |
| --- | --- | --- | --- |
| `race` | `id` (= order 1–6), `key`, `name`, `camp` (0 good, 1 neutral, 2 evil), `lifespan_years`, `generation_years`, `creator_god`, `awakening_year`, `present` (0/1) | id | echo of the data, as `biome` (design 6 `race`); all six rows even when `--races` leaves some out (`present` 0, `awakening_year` NULL) |
| `race_biome` | `race`, `biome`, `weight` | (race, biome) | preferred land (design: `preferred_biomes`) |
| `culture` | `id`, `key`, `name`, `race` | id | echo; naming data stays in the content hash, not in the table |
| `god` | `id`, `name`, `camp` | id | **P3 rows: the creator gods only**, one per race, named by P3-02; P5 adds the rest of the pantheon and columns (`domain`) with a bump |
| `age` | `id` (1–6), `race`, `start_year`, `end_year`, `name` | id | design 6 `age` |
| `phase` | `id`, `age`, `kind` (4.1), `start_year`, `end_year`, `trigger_event` | id (= time order) | design 6 `phase` |
| `event_kind` | `id`, `key`, `caused` (0/1), `salience`, `template` | id | the event catalogue (6.3, A1), one row per kind, written by the `history` stage as `biome` is written by the biome stage |
| `event_kind_role` | `kind`, `seq`, `role`, `entity_type` | (kind, seq) | the participant roles of each kind, WITHOUT ROWID |
| `event_kind_value` | `kind`, `seq`, `key` | (kind, seq) | the value keys of each kind, WITHOUT ROWID |
| `event` | `id`, `year`, `end_year`, `kind` (→ `event_kind.id`), `phase`, `region`, `settlement`, `salience` | id | the log (6.2) |
| `event_participant` | `event`, `seq`, `entity_type`, `entity_id`, `role` | (event, seq) | WITHOUT ROWID |
| `event_cause` | `event`, `cause` | (event, cause) | cause < event |
| `event_value` | `event`, `key`, `value` | (event, key) | integers only; keys per kind in the catalogue |
| `civilization` | `id`, `name`, `race`, `culture`, `home_region`, `founded_year`, `founding_event`, `ended_year`, `ending_event` | id | design 6 `civilization` |
| `settlement` | `id`, `name`, `x`, `y`, `region`, `founder_civ`, `founded_year`, `founding_event`, and its end state: `owner_civ`, `kind`, `status`, `population`, `ruined_year` | id | design 6 `settlement` (`civ_id` = `owner_civ`) |
| `settlement_census` | `settlement`, `year`, `owner_civ`, `kind`, `status`, `population` | (settlement, year) | state by year (6.4) |
| `region_control` | `region`, `from_year`, `to_year`, `civ`, `event` | (region, from_year) | territory by year (6.4) |
| `relation` | `a_type`, `a_id`, `b_type`, `b_id`, `kind`, `since`, `until`, `since_event` | (a_type, a_id, b_type, b_id, since) | design 6 `relation` with an end year; P3 uses civilization pairs only (kinds: war, peace treaty, vassal…, per P3-06) |

Conventions for all P3 tables: STRICT, no REAL, no BLOB; ids from 1; 0 = none in id columns; NULL only for a year that has not happened (`ended_year`, `ruined_year`, `to_year`, `until`), stated per column; TEXT keys from data or from a catalogue, never free prose (design 6: "the database stores facts and names"). The design's `event.summary` and `event.site_id` are not columns in P3: summaries are rendered from the catalogue's templates (11), sites arrive in P6 (proposals entry). Expected sizes at 1024 (interim, to be measured by P3-04): 5,000–30,000 events, under 1 MB of tables; the three catalogue tables hold a few dozen kinds and a few hundred short rows, a few kilobytes, inside that estimate.

### 6.2 The event log contract

- `id` increases with (year, processing order): every event's year is ≥ its predecessor's (`end_year` of a summary is ≥ its `year`; summaries are dated by their first year, and are appended when written, so a summary's id may follow events of later years only within its own quiet step; P3-04 states the exact rule).
- **Causes**: each `event_cause.cause` < `event`; causes point to the events that made the event possible or provoked it (the war that led to the siege, the famine that led to the abandonment). Every event other than an awakening and the first events of a stub has at least one cause, or its kind is marked "uncaused" in the catalogue (a random pressure, a founding by expansion). The cause graph is a DAG by construction.
- **Participants**: `entity_type` from a fixed list (`race`, `god`, `civilization`, `settlement`, `region`; P5 adds `figure`, `faction`, `monster`, `site`), `role` from the kind's catalogue entry (`actor`, `target`, `winner`, `loser`, `founder`, `creator`, …), `seq` 1, 2, … in a defined order.
- **Values**: integer facts (`population_before`, `population_after`, `dead`, `army`, `years`…), keys from the catalogue.
- **Salience**: an integer 0–100, set by the kind's rule (an awakening 100, a capital sacked 90, a hamlet founded 10), used by the report to choose what to print and by the enrichment pass to choose what to narrate.

This is what section 14 needs: every event names its kind, time, place, actors in roles, quantities and causes, so a language model can narrate a chain of events from records alone and every reference it writes can be validated against ids, names and years (design 14 "Everything is validated"). The enrichment pass reads the world; nothing it writes enters `world.db`.

### 6.3 The event catalogue

A table in P3-04 (extended by P3-05, P3-06 and later P5), one row per kind: key (`awakening`, `civ_founded`, `settlement_founded`, `settlement_grew`, `famine`, `plague`, `abandoned`, `region_claimed`, `war_declared`, `battle`, `siege`, `sacked`, `conquered`, `peace`, `quiet_era`, …), the participant roles, the value keys, whether it is caused or uncaused, its salience rule, and its report template (section 11). The catalogue is part of the schema contract: renaming a kind or changing a role's meaning is a schema change; adding a kind is not. `docs/specs/schema.md` keeps the catalogue summary, as it keeps the layer catalogue.

**The catalogue travels with the world (A1).** The `history` stage writes the catalogue it ran with into `event_kind`, `event_kind_role` and `event_kind_value`: kind ids 1, 2, … in the catalogue's order (P3-04 fixes it; later kinds are appended, never renumbered), the key, the caused flag, the base salience, the report template taken from the loaded `history.report` data, and per kind its roles (with the entity type each takes) and value keys in a defined `seq` order. `event.kind` refers to `event_kind.id`. So a world file is self-describing: the viewer, the game and the enrichment pass list kinds, roles and value keys, and render the same sentences with whatever data set is installed later, from the world alone. Child tables rather than a text encoding in one column: they are STRICT-typed, need no second parser in every reader, and are queried with the same row reader as every other table; they are tiny (a few hundred rows).

**One copy in code (A1).** worldgen's public API exposes the catalogue read-only (`worldgen/history/event_catalogue.hpp`: the compiled catalogue the stage writes, and a reader of the three tables of a world) and the template renderer (`render_event(const HistoryView&, event id) → std::string`, P3-07), both in the `worldgen` library, not in the CLI. The CLI's `report --history`, the viewer's event browser (P4) and the game call them; nobody restates the catalogue or re-implements template rendering (compare the open proposals entry "Viewer: layer units and region names come from a copy of the catalogue").

### 6.4 Territory and state by year (for P4)

P4's scrubber must show, at any year Y, who holds each region and which settlements exist with what owner and size, without replaying the log:

- **`region_control`**: one row per interval during which a region is held by one civilization: owner at year Y = the row with `from_year` ≤ Y < `to_year` (or `to_year` NULL); no row = free. Intervals of one region do not overlap; each opens with the event that caused it (`event`). With about 50 regions and a few hundred changes per world, a viewer loads the table once and answers any year in memory.
- **`settlement_census`**: a row at the founding, at every change of owner, kind or status, and at the end of every phase (so at most one row per settlement per phase plus its changes). State at Y = the last row with `year` ≤ Y; a settlement with `founded_year` > Y does not exist yet.
- **`relation`** intervals give wars and treaties at Y the same way.

P4 needs nothing else for the timeline; events at Y come from `event.year`.

### 6.5 `meta`

Required from schema 6 (written by P3-03): the eight `param.*` keys of section 5. From schema 7: `history_end_year` = E (redundant with `param.history_span`, but it is the design's "end year" in `meta`, and P5's history mode would let them differ). `meta.stages` gains `chronology`, `history`.

## 7. What history reads from geography

| Need | Read | Rule (Paul's decision of 2026-11-19 where marked) |
| --- | --- | --- |
| Homeland suitability | `region` (means of the six scores, `habitability_p90`, `walk_cells`, `ore_reach_cells`, `coast_cells`, `river_class_max`, `elev_mean_dm`), `region_biome` | each race weights these itself (P1-05 1): race data gives score weights and biome preferences (8.1); no single habitability decides |
| Upland homelands (dwarves) and wild homelands (faeries) | `region_biome`, upland flag 4 × `walk_cells` < `cells`, `ore_reach_cells` | **by region and biome, not by walk area** (decided); measured at 1024: upland regions with reachable ore 0–9, median 3; wetland-rich regions 0–10, median 1, so placement must fall back to the best-scoring region, never fail |
| Mines | `ore_reach_cells` | rich ore within 300 m of walkable ground, not ore under walkable cells (P1-05 critic) |
| Islets | `cells` − `lake_cells` | regions with under about 0.3 km² of dry land (1172 cells; interim threshold `islet_max_dry_cells` 1172, tuning) are never homes or settlement sites (**decided**); they may still be claimed |
| Movement and adjacency | `region_border` | the region graph; walkable borders (`walk_pairs` > 0) are open; a border with `walk_pairs` = 0 is a barrier unless a pass is attached |
| Passes | `pass` (`col_x`, `col_y`), the `region` layer, `region_border.pass_id` | a pass is located by its col and the `region` layer (**decided**); `pass_id` names the border it leads across (P1-06 r2); passes are places to fortify and fight over (P3-06 may make a battle site of a col; sites themselves are P6) |
| River borders | `region_border.river_pairs`, `length_m` | a border is a river border by the share of its length along the river, `river_pairs` × 16 / `length_m` ≥ `river_border_share` (interim 0.5), not by `river_pairs` > 0 (**decided**); a region's own `river_class_max` ≥ 6 means a river runs through it |
| Ports and fishing | `coast_cells`, `forage_mean`, river mouths (cells where a river of class ≥ 4 meets the ocean, from the `river`, `water`, `flow_dir` layers) | ports come from history from forage and river mouths, not from `habitability` (**decided**); coastal regions can be thin |
| Settlement sites | `seed_x`, `seed_y` of the region (its best site), then the layers `habitability`, `walk_area` | a region's first settlement stands at its seed cell; further settlements at the best site cells by the subsystem's rule (P3-05) |
| Lowland borders | — | lowland borders are political lines, not features (**decided**); history may treat them as freely crossable |

History reads only these stored tables and layers (H8). It never rewrites a geography layer or table (P1-00 4 rule 1); blight and ruin are history's own records (P5 may add an overlay layer).

## 8. Data kinds

All new kinds follow P0-03 (fields required unless stated, no floating point, named checks) and enter `content_hash`, so the P3-01 merge changes every world's hash (P1-00 6.4; a version bump and golden re-recording).

### 8.1 `race` (directory `races/`, required)

Six definitions, ids `elf`, `goblin`, `dwarf`, `faerie`, `human`, `orc` (any ids are allowed; the order is data).

| Field | Type | P3 meaning |
| --- | --- | --- |
| `order` | int(1, 6) | awakening order; kind-level check: unique, 1 to the number of races, no gaps |
| `name`, `plural`, `adjective` | string(32) | display words (`Elf`, `Elves`, `Elven`) |
| `camp` | enum(good, neutral, evil) | creator camp; starting dispositions between camps (8.3) |
| `awakening_share` | fx(0, 1) | share of the span at which the race awakens (design 5 table: 0, 0.35, 0.55, 0.70, 0.83, 0.93); check: 0 for order 1, strictly increasing with order |
| `lifespan_years`, `generation_years` | int | lifespans (design: elves very long-lived); generations count ruling-line turnover in quiet steps |
| `growth_permille` | int | yearly population growth toward capacity |
| `site_weights` | record {`fertility`, `forage`, `timber`, `ore`, `fresh_water`, `habitability`} of int(0, 1000) | how the race scores a region's means (7) |
| `biomes` | array of record {`biome` ref(biome), `weight` int(−1000, 1000)} | biome preferences over the nine natural biomes (design 5 "Food and homelands") |
| `upland` | int(−1000, 1000) | preference for upland regions (dwarves positive) |
| `coast` | int(−1000, 1000) | preference for coastal regions |
| `expansion`, `aggression` | int(0, 1000) | drive to claim regions and to go to war (P3-06) |
| `settlement_kinds` | array(enum(hamlet, village, town, city, fortress)) | kinds the race builds |
| `culture` | ref(culture) | the default culture |
| reserved (P5) | — | `temperament`, `magic_schools`, relations by race: not declared in P3 (adding a field later is a data change with a version bump, not a schema change) |

### 8.2 `culture` (directory `cultures/`, required)

Fields: `race` ref(race); `name` string(32); naming data for P3-02: syllable arrays (`onsets`, `nuclei`, `codas`) of word-grammar strings, `patterns` (array of pattern strings such as `"ON"`, `"ONC-ON"`), and name templates per entity kind (`civilization`, `settlement`, `god`), for example `"The {adjective} {noun}"` with word lists. P3 ships one culture per race; a race may have several (a civilization takes its race's default culture in P3).

### 8.3 `history` (directory `history/`, required, fixed ids)

A fixed-id kind like `worldgen` (P0-03 amendment 15): ids `chronology`, `engine`, `peoples`, `conflict`, `report`, one per spec, each with its own schema of tuning fields (phase lengths per detail level, quiet step, flare threshold, capacities, growth, war odds, salience thresholds, the starting disposition table by camp pair). Kept apart from `worldgen` because it tunes a different phase and its files are edited by different reviews; the mechanism is the same.

### 8.4 Event rules

The design lists "event rules" among the hand-edited TOML. In P3 the rules are code (the subsystem specs), and their numbers are `history` tuning; the event catalogue's English report templates are data (`history.report`). Data-driven event rules (conditions and effects in TOML) are a P5 question (15, question 6).

## 9. Seams for later phases

### 9.1 P5 subsystems

Tick steps T3 (figures) and T5 (divine interventions) exist as no-ops; T4 has slots for monster incursions and blight. Entity types `figure`, `faction`, `monster`, `site` are reserved in `event_participant.entity_type`; `relation` already carries entity types. Beasts are per-region counts (P5 adds a table and a quiet-step rule); settlement racial mix is P5 (`settlement_race`). P3 keeps each subsystem's state in its own structure behind the engine's step interface (P3-04), so P5 adds subsystems without changing the engine's order of existing steps.

### 9.2 Divine interventions and the divine script

P3-04 defines, without implementing any source:

- **Intervention record**: (year, god id, kind key, target entity type and id, optional region), kinds from a closed list fixed by P5 (design 14: bless, curse, raise a champion, send a monster, mark a site).
- **Source interface**: at T5 of each tick the engine asks the active source for the interventions of that year, given a read-only view of the state; it validates each (the god exists, the target exists and is alive, the kind is supported) and applies the valid ones in source order as events of kind `divine_<kind>` with the god as participant `actor` and a cause link to the intervention's own record event (a script-sourced intervention gets a `divine_script` value 1).
- **Sources**: P3 has the empty source. P5 adds the seeded source (keyed draws, `param.divine_activity`). The divine script (design 14, milestone 12) is a third source that reads a script file; `param.divine_script` stores its SHA-256, so a world is a function of seed, settings and script. Replacing the source must not change anything else: the seeded source's draws are keyed (H4), so a script that removes one intervention does not shift any other decision.

### 9.3 Enrichment

Nothing in P3 writes for the enrichment pass beyond the log contract (6.2) and the names on entities. The pass's lore file is keyed to the world file's hash (design 14) and is out of the generator.

## 10. Rules settled here for every subsystem

1. **Homeland choice** (P3-05): a race scores each eligible region (not an islet, not held by another civilization) as Σ weights × the region's means + biome preferences × biome shares + upland and coast terms; the civilization's home region is the best score with a keyed tie-break; the race's creator god's "place" (design 5 "Creation") is that region. If no region is eligible, the best region by score among all non-islet regions, held or not (the awakening still happens: the exit check's order item must hold on every world).
2. **Civilizations per race** with `auto`: 1 or 2 by a keyed draw, reduced to 1 when the free eligible regions are fewer than `regions_per_civ` (interim 4) × the civilizations already present plus 2; never 0 for a present race.
3. **Capacity** of a region for a race: a linear function of its score and dry area (P3-05); settlement populations are integers that move toward capacity at `growth_permille`.
4. **Region graph travel**: neighbours by `region_border`; open if walkable or crossed by a pass or (from P5) a road; a large-river border (7) costs extra for armies and expansion; costs are integers.
5. **Conquest and ruin** (P3-06): a sacked settlement becomes `ruined` or changes owner; a civilization with no settlement and no region ends (`ended_year`, `ending_event`); ruined settlements stay rows (P6 turns them into sites).
6. **Starting dispositions**: same camp friendly, opposed camps (good against evil) hostile, neutral wary of both (design 5); a 3 × 3 table in `history.conflict`. Goblins are hostile to elves from their first year (design 5 table) through the camp rule.

## 11. The history report

**Interface** (P0-04 7 "Later" announced it): `worldgen-cli report --db PATH --history [--level summary|full] [--out PATH] [--force]`, a separate text format `# worldgen history v1`, printed instead of the world report. Interim: an option of `report`, not a new subcommand, since it reads the same file with the same reader and output rules (proposals entry; P3-07 may propose otherwise).

**Content** (P3-07 fixes the exact format):

1. `[settings]`: seed, phrase, span, detail, races, civilizations per race, generator version.
2. `[ages]`: one line per age (race, start and end year, creator god).
3. `[phases]`: one line per phase (kind, years, trigger event id).
4. `[civilizations]`: one block per civilization: name, race, home region (id and name), founded, ended or alive, peak regions and population with years, settlements founded, wars fought, fate.
5. `[chronicle]`: events in id order, those with `salience` ≥ the level's threshold (all at `full`), one line each: `<year> e<id> <kind>: <rendered sentence> [causes e<id>, …]`, the sentence rendered from the kind's template with names, so that the critic can follow causes by id.
6. `[present]`: the end state: regions held per civilization, settlements by status and kind.
7. `[stats]`: key = value lines for sweeps (section 12.2), stable names, integers and exact decimals.

**Rendering**: sentences come from `render_event` in the `worldgen` library (6.3, A1), using the templates stored in the world's `event_kind` table, so the report, the viewer and the game print the same sentence for an event whatever data set is installed. The template language (placeholders for roles, values, place and year) is P3-07's.

**Determinism**: the report is part of the cross-platform contract, as `report` and `hash` are (P0-04): UTF-8, LF, no trailing spaces, integer formatting only, templates from data, rows in id order, no locale, no time. Golden: the report text of the golden cases is hashed in the stage golden files (P3-07).

**Reviewability**: every line carries ids and years; names are printed with ids (`Ashwood (s12)`); the chronicle can be grepped by civilization or region; `[stats]` lets the critic compare worlds; a world critic review cites seeds and event ids (design 13).

## 12. Budget and validation

### 12.1 Performance budget

Release build, Paul's 6-core machine, as P1-00 11. Geography at 1024 takes about 9 s (all of it; P1 budget 15 s), so history has about 100 s under the 2-minute milestone; the budget below keeps a large margin, because P5 adds subsystems.

| Stage | 512 | 1024 (reference) | 2048 |
| --- | --- | --- | --- |
| `chronology` | 0.1 s | 0.1 s | 0.1 s |
| `history` (P3-04 to P3-06 together, 6,000 years, detail normal) | 3 s | 15 s | 60 s |
| writing the history tables | 0.2 s | 1 s | 4 s |
| `report --history` | 0.5 s | 2 s | 8 s |
| **Whole `generate` (geography + history)** | **10 s** | **30 s** | **2 min** |

A history stage over twice its 1024 target is a defect to report. Span 12,000 years and detail `dense` may take twice the default; no budget applies to debug or sanitizer builds.

### 12.2 Sweeps

The validation specialist sweeps with shipped data and default settings: 1024 seeds 0–199 (the reference), 512 seeds 0–49, 2048 seeds 0–19; plus 1024 seeds 0–49 at span 2000 and at span 12000, and with `--races` subsets (one race; elves and orcs only). Statistics are read from the `[stats]` block of `report --history`, as P1 read `[geography]`. Each stage spec states its statistics with hard and typical ranges, as P1-00 13.1.

### 12.3 Hard bounds (defects on any seed)

- **Order and schedule**: every present race awakens exactly once, in race order, at its scheduled year (P3-03's rule including the seed's jitter), with an `awakening` event and an age row; ages tile [0, E).
- **Phases**: phases tile [0, E); simulated years (kinds 1, 3, 4) between 10% and 50% of E.
- **Not empty**: at year E at least one civilization is alive, at least `min_settlements_alive` (interim 3 at 1024, 1 at 512) settlements are not ruined, and every present race founded at least one civilization.
- **Not runaway**: no civilization holds more than `runaway_share` (interim 0.8) of non-islet regions for more than 1,000 consecutive years; events ≤ 200 per simulated year on average; no population above its region's capacity × 2; table sizes under 5 MB at 1024.
- **Log integrity**: every cause id is smaller than its event's id; every participant and place id exists; years non-decreasing with id except within the summary rule (6.2); each `region_control` region has non-overlapping intervals.
- **Purity**: the result is independent of `--threads`; `--until chronology` then `history` on the read-back world equals one run.

### 12.4 Golden cases

The geography cases of `world-hashes.sh` (P1-00 14 X5) run every stage by default, so from P3-03 on they are full-history worlds; their hashes are re-recorded at each P3 merge. Added cases (interim): `hist-span-2000-512` (`--seed 7 --map-size 512 --history-span 2000`), `hist-races-elf-orc-1024` (`--seed 3 --races elf,orc`) and `hist-dense-1024` (`--seed 11 --history-detail dense`), so that the settings paths are compared across compilers. Each stage spec adds golden vectors of its own (P1 style: table hashes of fixture and shipped worlds, written by the test writer's independent reference); P3-07 adds the report text hashes.

### 12.5 Tests that span stages

- **Y1 Pipeline**: default `generate` writes `meta.stages` = `elevation,…,regions,chronology,history`; `--until chronology` and `--until regions` leave the later tables empty; an unknown stage exits 2.
- **Y2 Resume**: H8's resume property for `chronology` and `history`.
- **Y3 Draw independence**: a test-only extra keyed draw in one subsystem changes no other subsystem's decisions (catches a shared sequential generator).
- **Y4 Ordering**: the history tables are byte-identical when the in-memory maps are filled in a different insertion order (test seam), which catches iteration over insertion order.
- **Y5 Settings**: every setting's range is enforced by flag and settings file; the eight `meta` keys are present in every world.

## 13. What the critic will judge

As P1-00 13.2 did for maps, a checklist for the world critic on the history report and the viewer timeline (P4): the elves are old and few; goblins are hostile to elves from their first year; dwarves arrive into a world already at war and settle uplands; faeries in wild places; humans spread fast over land left open or ruined; orcs open the last age of conflict; wars have causes that read sensibly; ruins come from sackings or decline, with causes; quiet intervals read as eras, not as gaps; no world is empty or dominated by one civilization for millennia without a flare-up; names are pronounceable and distinct per culture.

## 14. Inherited from P1

Everything in P1-00 sections 2.1 (cells), 6 (tuning as data, content hash policy), 7 (shared algorithms, where a history spec needs a cell-level step), 8 (D1–D10) and 9 (schema growth rules) applies. History adds H1–H8 (4.5), the keyed-draw scheme (4.6) and the event catalogue (6.3).

## 15. Open questions for Paul

Each with the recommendation and the interim choice the specs follow until he decides.

1. **Quiet intervals in steps.** The design's "one jump" cannot be interrupted by a flare-up. Recommendation and interim: quiet steps of 25 years, tension checked after each (4.1).
2. **Year 0 and the present.** Recommendation and interim: year 0 is the first elven awakening and the present is year E (6,000 by default); the report and the game may show other eras.
3. **Gods in P3.** The awakening event needs its creator god (design 5 "Creation"). Recommendation and interim: P3 creates one named creator god per race, with its camp; the rest of the pantheon, domains and interventions are P5.
4. **Territory by region.** Recommendation and interim: civilizations hold whole regions (one owner per region at a time); finer frontiers would need a territory layer per period and are not needed for the design's claims and wars.
5. **No stored event text.** The design's `event.summary` column would store prose. Recommendation and interim: store facts only (kind, roles, values, causes) and render sentences from catalogue templates in the report, the viewer and the game (design 6 rule "text shown to players is generated at runtime from records and templates").
6. **Event rules in TOML.** Recommendation and interim: P3's rules are code with their numbers in TOML tuning; data-driven rules (conditions and effects in TOML) wait until P5 shows which rules need to vary.
7. **Roads and improved passes.** The design's tick loop has "build a road" and history "improves passes"; the P3 row of section 13 does not list them. Recommendation and interim: P5 (with the `road` table); P3 treats passes as fixed crossings.
8. **Small worlds.** Recommendation and interim: 512 worlds run all present races with fewer civilizations (10.2) and are not judged for quality; 1024 is the reference (decided 2026-11-19).
9. **One culture per race in P3.** Recommendation and interim: yes; several cultures per race (and cultural region names) come with P5 or P6.
10. **History report as `report --history`.** Recommendation and interim: an option of `report` with its own format version (P0-04 7 already announced it).
11. **Settings for P5 stored now.** Recommendation and interim: all world-creation settings are written to `meta` from schema 6 (5), so that P5 needs no schema bump for them; `--divine-script` is refused until P5.

## Appendix A. Proposals entries (added to `docs/design/proposals.md` with this spec)

- "History: quiet intervals advance in steps so that flare-ups can interrupt them" (question 1).
- "History: the calendar starts at the first elven awakening" (question 2).
- "History: P3 creates only the creator gods" (question 3).
- "History: territory is held by whole regions" (question 4).
- "History: the event log stores facts, not summaries; no `site_id` until P6" (question 5).
- "History: event rules are code with TOML tuning in P3" (question 6), with roads and improved passes deferred to P5 (question 7).
- "History: schema versions 6 and 7 and the new tables" (6.1, design 6 table list: new tables `race_biome`, `event_value`, `settlement_census`, `region_control`; `relation` gains `until`).
