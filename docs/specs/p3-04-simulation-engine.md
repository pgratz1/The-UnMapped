# P3-04 Simulation engine

Status: **approved by the lead 2026-11-19; corrected after implementation (r3, the lead's rulings of 2026-11-19)** **r12 (2026-11-20, P3-10, generator 0.19.0): section 20, quiet stretches, `{@causes:…}`, thousands separators** **r15 (2026-11-21, P3-10 second round): no quiet line for a stretch shorter than `quiet_min_years` (20.1 item 1a)** **r17 (2026-11-21, P3-11, generator 0.20.0): section 21, numbers one to nine as words, kinds 39 and 40** **r19 (2026-11-22, P5-01, generator 0.21.0): section 22, one number form per clause, kinds 41 and 42, `flare_min_years`** **r20 (2026-11-22): section 22's text after the P5-01 integration** (the six questions of section 19 go to Paul; the interim choices stand). Written by the spec writer 2026-11-19; amendments 18.2 and 18.3 applied the same day; revision r1 (P3-07's amendments) the same day. Phase P3 History core, spec 4 (P3-00 3.1), pipeline stage 8, `history`, as an engine with stub subsystems. Generator **0.14.0**; **schema version 7** (thirteen new tables, one new required `meta` key, section 9). Design doc sections 5 (deep time, ages, active phases, quiet intervals, flare-ups, records, determinism; creation; entities; tick loop), 6 (tables `phase`, `event`, `event_participant`, `event_cause`, `civilization`, `settlement`, `relation`; `meta` end year), 13 (P3 row and exit check; P4 timeline and territory by year; P5 interventions) and 14 (records a model can narrate and validate; the divine script as an intervention source). Depends on P0-01 to P0-04 with their amendments, P1-06 (region tables), P3-00 (binding; amended in 18.3), P3-01 (kind `history`), P3-02 (keyed draws, names), P3-03 (chronology tables, `HistoryParams`, the registry rule 8.2). Implementation read: `origin/p3/03-int` at 3ddc942 (generator 0.13.0, schema 6). Proposals read: up to "History: with a subset of races the first one present opens history; every race keeps its creator god". Also addresses the P3-03 determinism audit's advisories A1 (fatal guards for schedule invariants) and A2 (H8 must not rest on in-memory values): sections 3.2, 3.3 and 5.6. Revision r2 (P3-05's amendments) 2026-11-19. **Revision r10 (2026-11-20, for P3-08, generator 0.17.0; `docs/specs/p3-08-followups.md`)**: names inside a sentence lose a leading capital `The` (8.5, P1-06 r3 14.7), a three-word form of `{value?…}` (8.4, 8.5), one quiet era per civilization and quiet phase row (6.6), the `quiet_era` template (4.2), the engine microbenchmark (14.3); see the revision note.

Numbers in this spec come from the spec writer's exact-integer prototype `~/.cache/the-unmapped/out/p3-04-spec/engine_proto.py` (on the P3-03 and P3-02 references; awakening years over seeds 0–999 and all 63 race subsets) and from the P1-06 validation worlds (`~/.cache/the-unmapped/out/p1-06-validation/`, whose geography is unchanged at 0.13.0). The empty-world size was measured with Python's SQLite 3.46.1 (9.6); the engineer confirms it with the vendored SQLite.

## 1. Purpose

The `history` stage turns the frame of P3-03 (present races, awakening years, ages, creator gods) into a schedule of phases and runs the simulation loop over it: yearly ticks in active phases and flare-ups, coarse quiet steps between them, an event log with causes, and the end state. P3-04 is the **engine**: the scheduler, the in-memory state, the step interface that P3-05 (peoples) and P3-06 (conflict) plug into, the event log writer, the bookkeeping that records territory and settlement state by year for P4, the event catalogue with its renderer, and the full schema 7. It merges with **stub subsystems**: each awakening places one civilization with one settlement for its race in a stub home region; nothing grows or fights; quiet intervals are summarized. That world already passes the exit check's order and timing items (design 13) and gives P3-07, P4 and the critic real tables to read.

## 2. What this spec touches (for the engineer)

| Area | Files |
| --- | --- |
| Engine | new `worldgen/include/worldgen/history/engine.hpp` (state, step seam, event writer), `worldgen/src/history/engine.cpp`, `worldgen/src/history/schedule.cpp` (`history/schedule.hpp`), `worldgen/src/history/stubs.cpp` (`history/stubs.hpp`) |
| Catalogue | new `worldgen/include/worldgen/history/event_catalogue.hpp`, `worldgen/src/history/event_catalogue.cpp` (compiled catalogue, template parser, `render_event`, `HistoryView`) |
| Inputs | new `worldgen/include/worldgen/history/inputs.hpp`, `worldgen/src/history/inputs.cpp` (`HistoryInputs`, from the context and from a stored world) |
| Stage | new `worldgen/include/worldgen/stages/history.hpp`, `worldgen/src/stages/history.cpp`; `stage.hpp`/`stage.cpp` (the stage in the implemented list, new row accessors and writers, 3.2) |
| Database | `worldgen/include/worldgen/db/schema.hpp` (version 7, thirteen DDL statements, 17 required keys), `schema.cpp`, `writer.hpp`/`writer.cpp` (row types and writers, `history_end_year`), `reader.hpp`/`reader.cpp` (row readers of every table the stage reads, value check of `history_end_year`) |
| Data | `game/data/history/engine.toml`, `game/data/history/report.toml` and identical copies under `worldgen/tests/data/geo/history/` and `worldgen/tests/data/geo-small/history/`; registration of ids `engine` and `report` in `worldgen/src/data/kinds.cpp` (or `kinds_history.cpp`) |
| Data checks | `worldgen/src/history/data_checks.cpp`: none new (the template checks are named checks of `history.report`, 4.2) |
| Report | `worldgen/src/inspect/report.cpp` and a reader in `inspect/`: section `[history]` (11.1) |
| CLI | `tools/worldgen-cli/cli.cpp`: `--until history` accepted; the stage list message |
| CI | `tools/ci/world-hashes.sh`: case `hist-dense-1024`; `p3-worlds.txt` |
| Version | `worldgen/include/worldgen/version.hpp`: **0.14.0** |
| Docs | `docs/specs/schema.md` (18.1) |
| Tests | `worldgen/tests/history/` (new CTest group `worldgen-tests-engine`, pattern `*history_engine_*`), `tools/worldgen-cli/tests/`; reference `worldgen/tests/reference/p3_engine_reference.py`; golden `worldgen/tests/golden/p3-04-engine.txt` |

No stream is added: the stubs draw only names (`hist.names.civilization`, `hist.names.settlement`, P3-02 4.5). The engine itself makes no random decision.

## 3. Inputs

### 3.1 What the stage reads

| Input | Source | Used for |
| --- | --- | --- |
| Seed | `meta.seed` | names (P3-02) and the keyed draws of later subsystems |
| Map side N | `param.map_size` | cell bounds (stub fallback, 7.2) |
| `HistoryParams` | the eight `param.*` keys (P3-03 4.1) | span E, detail level; `races` (checked against the `race` rows); the others are carried for P3-05, P3-06 and P5 and unused by the stubs |
| Region tables | `region`, `region_border`, `region_biome` (P1-06) | stub homes (7.1), the region graph for subsystems |
| Chronology tables | `race`, `race_biome`, `culture`, `god`, `age` (P3-03) | the schedule (age start years), awakening participants, names |
| Layers | `region`, `water` | the stub's settlement cell fallback (7.2); later subsystems add layers to the inputs (3.3) |
| Data | kinds `race` (fields not echoed in `race`: `settlement_kinds`), `culture` (`naming`), `history.engine` (4.1), `history.report` (4.2) | stub settlement kind, names, tuning, catalogue templates |

### 3.2 Every input comes from the stored world (H8; audit advisory A2)

**Rule.** The `history` stage is a pure function of a value `HistoryInputs` (3.3) and the loaded `ContentSet`. `HistoryInputs` holds only what the world file stores: values parsed from `meta`, rows of tables written by earlier stages, layers written by earlier stages. There are two constructors, and **they must give equal values for the same world**:

```cpp
namespace worldgen::history {
// The rows, layers and settings of the stored world the stage reads, from the pipeline's context:
// StageContext::seed(), map_size(), history_params() and the committed rows and layers of earlier stages.
HistoryInputs history_inputs_from(const stages::StageContext&);
// The same from a world file (resume): meta parsed (history_params_from_meta, P3-03 4.1), tables and layers read back.
// FatalError `history: resume: <reason>` when the world cannot be resumed (below).
HistoryInputs history_inputs_from_world(const db::WorldReader&, const data::ContentSet&);
}
```

In one run, `history_inputs_from` reads in-memory copies. They equal the stored values because (a) `StageContext::history_params()` is exactly what `generate_world` records in `meta` (P3-03 4.1 step 4) and `history_params_from_meta` inverts `history_meta_entries` (P3-03 14.1 item 8), (b) the context's rows and layers are those the stage writers committed, and the writers store integers and text exactly (P0-02), and (c) layers are decoded from their stored blobs (P1-00 5.1). This is not left to argument: test R1 (16.3) compares the two constructors field by field on every world case, and the field-count guard R2 makes a new `HistoryInputs` or `HistoryParams` field fail to compile until the round trip covers it.

`history_inputs_from_world` refuses, in this order: `history: resume: schema version <v>, this build reads 7` (the reader already refuses it; repeated for the seam), `history: resume: the world's data content hash differs from the loaded data` (`meta.content_hash` ≠ the content's hash: the race, culture and history data are not echoed in full, so the data must be the same), `history: resume: the world was generated by <version>, this build is <GENERATOR_VERSION>`, `history: resume: meta.stages is '<stages>', expected the stages up to chronology` (the stored stages must be exactly `elevation,…,regions,chronology`; a world that already ran `history` is not resumed).

**Resume path: a library seam, not a command.** `generate` has no `--resume` option in P3. A resume in the CLI would write a world from another world, which P0-02's writer (one working database, `VACUUM INTO` a new file) and P0-04's command set do not provide, and nothing but the H8 test needs it before P5's history mode. The resume is `history_inputs_from_world` followed by the pure stage function `stages::history_stage` (3.3) and a comparison of its rows with the uninterrupted world's tables (Y2, 16.3). Question 5 (19).

### 3.3 `HistoryInputs` and the stage function

```cpp
namespace worldgen::history {
struct HistoryInputs {
    std::uint64_t seed = 0;
    std::int64_t map_size = 0;
    HistoryParams params;
    std::vector<db::RegionRow> regions;              // id order
    std::vector<db::RegionBorderRow> region_borders; // (a, b) order
    std::vector<db::RegionBiomeRow> region_biomes;   // (region, biome) order
    std::vector<db::RaceRow> races;                  // id order
    std::vector<db::RaceBiomeRow> race_biomes;
    std::vector<db::CultureRow> cultures;
    std::vector<db::GodRow> gods;
    std::vector<db::AgeRow> ages;
    db::LayerGrid region_layer;                      // layer `region`
    db::LayerGrid water_layer;                       // layer `water`
    std::vector<db::BiomeRow> biomes;                // id order (P3-05 3.2)
    db::LayerGrid habitability_layer;                // P3-05
    db::LayerGrid river_layer;                       // P3-05
    db::LayerGrid flow_dir_layer;                    // P3-05
    friend bool operator==(const HistoryInputs&, const HistoryInputs&) = default;
};
}
namespace worldgen::stages {
using HistoryResult = history::HistoryResult;   // r3: defined in history/engine.hpp; the rows of the thirteen tables of 9.1, each in its write order (9.3)
HistoryResult history_stage(const data::ContentSet&, const history::HistoryInputs&);  // C1 items 2 to 5, then sections 5 to 7
void run_history(StageContext&);  // C1 item 1, history_stage(content, history_inputs_from(context)), write (9.3)
}
```

P3-05 and P3-06 add members to `HistoryInputs` when they read more (for example layers `habitability` and `walk_area`); R2 then forces them to extend both constructors and R1. `StageContext` gains read accessors like `regions()`: `region_borders()`, `region_biomes()`, `races()`, `race_biomes()`, `cultures()`, `gods()`, `ages()` (rows of earlier stages of this run, exactly as written), and writers for the thirteen tables (9.3).

### 3.4 Entry checks (fatal, in this order)

1. `history: input precondition: no region rows`; `history: input precondition: no age rows` (the chronology stage did not run) — `run_history` only.
2. Tuning (seam guards; unreachable with loaded data): each field of `history.engine` inside its schema range (`history: tuning: <field> outside its schema range`, declaration order, level records as `<level>.<field>`), then its named checks (`history: tuning: <message>`, 4.1).
3. Catalogue data (seam guard): the named check `report_events` of 4.2 (`history: report: <message>`, the first message).
4. Settings: `history: settings: history_span <E> outside 2000 to 12000`; `history: settings: param.races does not match the present race rows` (the ids of `params.races` must be the `key`s of the rows with `present` = 1, in id order).
5. Chronology rows: `history: chronology: <reason>`: the age rows are not ids 1 … P in order; the first `start_year` is not 0; a `start_year` is not the previous `end_year` + 1; the last `end_year` is not E − 1; an age's `race` is not a present race or its `awakening_year` ≠ `start_year`; a `god` row is missing for a present race. (P3-03 guarantees all of these; the engine's schedule depends on them, audit A1.) The exact texts of every entry check are in 16.6 (r3).
6. Region layer (r6, audit A4): the layer `region` of the inputs is N × N (N = `map_size`, 1 ≤ N ≤ the largest layer side) with element type `u16` and a valid grid, else `history: engine: layer 'region' is not <N> x <N> u16`.

## 4. Tuning and catalogue data

### 4.1 `history.engine` (new id of kind `history`, P3-01 5)

All fields required. Lengths are years **at the reference span of 6,000 years** and are scaled to the world's span (5.1).

| Field | Type | Shipped | Status | Meaning |
| --- | --- | --- | --- | --- |
| `quiet_step_years` | int(1, 200) | 25 | taste (Paul, 2026-11-19) | length of one quiet step, not scaled |
| `quiet_events_max` | int(1, 100) | 3 | taste | events per civilization per quiet step at most (6.6), not counting the engine's `quiet_era` |
| `quiet_min_years` | int(0, 400) | 50 | taste (r15, from 0.19.0; the world critic's F3 of P3-10) | a quiet stretch shorter than this gets no `quiet_stretch` event (20.1 item 1a), not scaled; 0: every quiet row has one |
| `simulated_min_permille` | int(100, 500) | 100 | bound | floor on the simulated years (kinds 1, 3, 4) as a share of E (P3-00 12.3: 10%) |
| `simulated_max_permille` | int(100, 500) | 450 | bound | cap on them, flare-ups included (P3-00 12.3: 50%); leaves 5% margin |
| `islet_max_dry_cells` | int(0, 1000000) | 1172 | decided (P3-00 7) | a region with `cells` − `lake_cells` < this is an islet: never a home or settlement site; shared by every subsystem |
| `sparse`, `normal`, `dense` | record `engine_detail` | below | taste | the lengths and threshold of each `history_detail` level |

Record `engine_detail`, fields in this order:

| Field | Type | `sparse` | `normal` | `dense` | Meaning |
| --- | --- | --- | --- | --- | --- |
| `active_years` | int(1, 2000) | 75 | 150 | 250 | opening active phase of each age |
| `flare_years` | int(1, 1000) | 30 | 50 | 75 | a flare-up |
| `final_years` | int(1, 3000) | 250 | 300 | 400 | the final active phase, at most |
| `flare_threshold` | int(1, 1000000) | 1500 | 1000 | 700 | tension (6.5) at or above which a flare-up starts; tension is on P3-06's scale, 1000 = "a flare is due" |

Named checks (per definition, P0-03 7.5, in this order):

| Check | Condition | Message |
| --- | --- | --- |
| `engine_simulated_bounds` | `simulated_min_permille` ≤ `simulated_max_permille` | `simulated_min_permille must not be above simulated_max_permille` |
| `engine_quiet_min` (r15) | `quiet_min_years` ≤ 2 × `quiet_step_years` | `quiet_min_years must not be above 2 x quiet_step_years` |
| `engine_phase_budget` | for each level in the order sparse, normal, dense: 6 × `active_years` + `final_years` ≤ 6 × `simulated_max_permille` − 30 | `<level>: 6 x active_years + final_years must not be above 6 x simulated_max_permille - 30` (ASCII: `x` and `-`, r3) |

`engine_phase_budget` makes the cap unreachable by the fixed phases alone for any span 2000–12000 and at most six races (`race.order` ≤ 6): scaling rounds each of the seven lengths by at most half a year, and 30 reference years are at least 10 scaled years at E = 2000 (5.1). Shipped: 700 / 1200 / 1900 against 2670.

```toml
# game/data/history/engine.toml and the fixtures under worldgen/tests/data/geo*/history/
[history.engine]
quiet_step_years = 25
quiet_events_max = 3
quiet_min_years = 50        # r15, from 0.19.0
simulated_min_permille = 100
simulated_max_permille = 450
islet_max_dry_cells = 1172
sparse = { active_years = 75, flare_years = 30, final_years = 250, flare_threshold = 1500 }
normal = { active_years = 150, flare_years = 50, final_years = 300, flare_threshold = 1000 }
dense = { active_years = 250, flare_years = 75, final_years = 400, flare_threshold = 700 }
```

### 4.2 `history.report` (new id; P3-01 5 says "P3-04 or P3-07, whichever lands first")

P3-04 lands first and defines one field; P3-07 adds none (r1: its selection constants are part of the report format, P3-07 8).

| Field | Type | Meaning |
| --- | --- | --- |
| `events` | array(record `report_event`, 1, 256) | one entry per event kind of the compiled catalogue (8.2) |

Record `report_event`, in this order: `kind` string(32, the P0-03 definition id grammar); `salience` int(0, 100), the kind's base salience; `template` string(256), the English sentence (grammar 8.4); **from 0.19.0 string(512)** (r13: P3-05 22.3's cause-list templates of kinds 16 and 31 are about 280 and 300 bytes; the bound is a loader limit, not a column type, and no shorter wording keeps the cause map readable).

Check `report_events` (r3: a **kind-level check of kind `history`**, P0-03 amendment 16, reported at the header of `history.report`'s definition in the P0-03 7.5 form `<path>:<line>:<column>: error: check 'report_events' failed for 'history.report': <message>`), messages in this order: for each entry i in array order: `events[<i>]: kind '<key>' is not an event kind` (not in the compiled catalogue), `events[<i>]: kind '<key>' is already listed at events[<j>]` (j the first), `events[<i>]: template: <problem>` (8.4, the first problem of that template); then for each compiled kind in id order without an entry: `event kind '<key>' has no entry`. Each message is reported (the check reports all, as `race_order_unique` does). A check of one kind may consult the compiled catalogue: it is code of the library, not another kind.

```toml
# game/data/history/report.toml and the fixtures under worldgen/tests/data/geo*/history/
[history.report]
events = [
  { kind = "awakening", salience = 100, template = "{creator} awakened the {race:plural} in {@region}." },
  { kind = "civ_founded", salience = 80, template = "The {race:plural} founded {civilization} in {@region}." },
  { kind = "settlement_founded", salience = 40, template = "{founder} founded {settlement} in {@region}." },
  { kind = "quiet_era", salience = 20, template = "{civilization} passed {years} quiet {years?year|years}, from {@year} to {@end_year}{generations?, under one generation of rulers|, in which one new generation of rulers came to power|, in which # new generations of rulers came to power}." },
  { kind = "flare_up", salience = 50, template = "In {@year} tensions flared into {years} {years?year|years} of upheaval." },
]
```

The `quiet_era` template above is r10's (generator 0.17.0, P3-08); before it read `{civilization} passed {years} quiet {years?year|years} ({generations} {generations?generation|generations}), from {@year} to {@end_year}.`, which wrote "(0 generations)" for an era in which no new generation came to power (`docs/reviews/p3-05-peoples-world-critic.md` F2). A template is data: the change alters `content_hash` and the stored `event_kind` row, not the schema (8.2).

Since `history` is a fixed-id kind, every data root now carries exactly `chronology.toml`, `engine.toml` and `report.toml` (P3-01 5's rule); P3-01's test of an unknown id changes its expected list to `expected one of 'chronology', 'engine', 'report'`, and its "a root with `history/engine.toml` fails" case uses another id (`peoples`).

## 5. The scheduler

E = the span; P present races; y_1 = 0 < y_2 < … < y_P the age `start_year`s (P3-03 C3); the detail level's record d. All arithmetic in `int64`. Steps SC1 to SC6; SC1–SC4 fix the **base schedule** before year 0; SC5 and SC6 run during the simulation, because a flare-up depends on the state.

### 5.1 SC1 Lengths

```
scale(v) = max(1, (v × E + 3000) div 6000)        // years at the reference span, rounded half up
A = scale(d.active_years)   F = scale(d.flare_years)   L = scale(d.final_years)   Q = quiet_step_years (not scaled)
cap_years   = (simulated_max_permille × E) div 1000                          // floor
floor_years = min((simulated_min_permille × E + 999) div 1000, cap_years)    // ceil, never above the cap (r3)
```

The floor is capped because the two roundings differ: with both permilles 100 (allowed by `simulated_min_permille` ≤ `simulated_max_permille`, the only tuning check on them), level `normal` = {`active_years` 10, `flare_years` 1, `final_years` 10}, span 2001 and one race, ceil(200.1) = 201 > floor(200.1) = 200, so floor_years = cap_years = 200.

**Why lengths scale with the span** (decision; question 1). The design gives "about 1,200 simulated years" for the default span and allows spans of 2,000 to 12,000, and P3-00 bounds the simulated share to 10–50% on every world. Fixed lengths cannot satisfy both: 1,200 fixed years are 60% of a 2,000-year span and 10% of a 12,000-year one. Scaling keeps the share a property of the detail level alone (table in 5.5), so a short history is a compressed one and a long one has longer eras, not more of them. The quiet step is a resolution, not a length, and stays 25 years.

### 5.2 SC2 Opening phases

```
for k = 1 … P − 1:  e_k = min(y_k + A, y_{k+1})                       // E1: cut at the next awakening
k = P:  if E − y_P ≥ 2: e_P = min(y_P + A, E − 1)  else: e_P = E      // leave the final phase at least one year
```

Opening phase k is [y_k, e_k).

### 5.3 SC3 Final phase

```
if E − y_P ≥ 2:  S_f = max(E − L, e_P)     // the final phase starts no earlier than the end of the last opening phase
else:            S_f = E                   // no final phase (E5)
```

The final phase is [S_f, E). It is **at most** L years: when the last race's opening phase reaches past E − L, the final phase begins where it ends (E4). With the shipped data this is the usual case at `normal` (73% of seeds) and every case at `dense`: the orcs' age (0.07 E ± jmax) is then active from their awakening to the present, which is the design's "their coming opens the most recent age of conflict".

### 5.4 SC4 Floor

```
O = Σ_k (e_k − y_k);  sched = O + (E − S_f);  D = floor_years − sched
if D > 0 and S_f < E:  S_f' = max(e_P, S_f − D);  D −= S_f − S_f';  S_f = S_f'     // lengthen the final phase backwards
for k = 1 … P − 1 while D > 0:  e_k' = min(y_{k+1}, e_k + D);  D −= e_k' − e_k;  e_k = e_k'   // then the openings, in age order
sched = Σ_k (e_k − y_k) + (E − S_f)
```

After SC4, D ≤ 0 always (every year can become active: openings 1 … P − 1 up to the next awakening, the final phase back to e_P, the last opening is [y_P, e_P)). The floor acts only for race subsets (one race: A + L is 7.5% at normal; `elf,orc` at seed 3: 548 years, raised to 600 by lengthening the elves' opening to 202 years). Guard: `history: schedule: scheduled active years <sched> above the cap <cap_years>` if sched > cap_years (unreachable by `engine_phase_budget`, audit A1).

The **quiet intervals** are the gaps: [e_k, y_{k+1}) for k < P and [e_P, S_f), each when non-empty. The base schedule is opening 1, quiet 1, opening 2, quiet 2, …, opening P, quiet P, final, with empty quiet intervals left out.

### 5.5 SC5 Quiet intervals in steps, SC6 flare-ups

A quiet interval [q_s, q_e) is run as a sequence of **quiet phase rows**, each a sequence of steps:

```
cur = q_s
while cur < q_e:
    open a quiet phase row starting at cur
    s = cur
    loop:
        len = min(Q, q_e − s)                       // E2: the last step (or the only one) may be shorter
        run quiet step [s, s + len)                 // 6.4: Q1 … Q6
        s = s + len
        if s == q_e: close the row at s − 1; cur = q_e; break          // E3: no flare-up at the interval's end
        (tension, trigger) = the step's tension (6.5)
        if tension ≥ d.flare_threshold:
            f = min(F, q_e − s, cap_years − sched − flare_total)       // E3: cut at the next awakening or the final phase; E8: the cap
            if f ≥ 1:
                close the row at s − 1
                run a flare-up phase [s, s + f) (kind 3, trigger_event = trigger; 6.3)
                flare_total += f; cur = s + f; break                   // the interval resumes as a new quiet row
```

Steps of a resumed row are counted from its own start (`cur`), not aligned to the earlier row. A flare-up may follow the first step of a resumed row (E9); P3-06's tension measure decides whether it does. Guards (fatal, `history: schedule: <what>`): a step length < 1; a flare length outside [1, q_e − s]; a phase row with `end_year` < `start_year`; and, after the run, the rows not tiling [0, E) contiguously in id order, or simulated years outside [floor_years, cap_years]. Exact texts in 16.6 (r3).

**Simulated years** (kinds 1, 3 and 4) with the shipped data, all six races, seeds 0–999, no flare-up (the stubs' tension is 0); per mille of E in brackets:

| Detail | E = 2000 (A, F, L) | E = 6000 | E = 12000 | Phase rows |
| --- | --- | --- | --- | --- |
| sparse | 233 (25, 10, 83) [117] | **700** (75, 30, 250) [117] | 1400 (150, 60, 500) [117] | 13 |
| normal | 370–400, median 390 (50, 17, 100) [185–200] | **1110–1200, median 1171** (150, 50, 300) [185–200] | 2220–2400, median 2342 (300, 100, 600) [185–200] | 12–13 |
| dense | 535–575 (83, 25, 133) [268–288] | **1610–1730**, median 1671 (250, 75, 400) [268–288] | 3220–3460 (500, 150, 800) [268–288] | 12 |

Arithmetic at E = 6000, normal: no awakening gap is below 483 years (P3-03 12), so no opening is cut (6 × 150 = 900); the orcs awaken at 5520–5640, so e_6 = 5670–5790; S_f = max(5700, e_6), final = 330 − (e_6 − 5670) capped at 300: 210–300; total 900 + 210 … 300 = 1110–1200. The design's 1,200 is the top of that range; P3-06's flare-ups add to it (P3-00 4.1: the 1024 sweep median must be 1000–1400; P3-04's is 1171). Over the 63 race subsets × the three spans × the three levels × seeds 0–49 the share is 100–288 per mille (the floor binds for one-race worlds at `sparse`), never above the cap 450.

### 5.6 Edge cases and their rule (P3-00 4.1)

| # | Case | Rule |
| --- | --- | --- |
| E1 | two awakenings closer than A | the earlier opening ends at the later awakening (SC2); no quiet interval between them |
| E2 | quiet interval shorter than Q | a single step of its length (SC5) |
| E3 | flare-up reaching the next awakening or S_f | cut there (SC6); no flare-up when the step ends the interval |
| E4 | last opening reaching past E − L | the final phase starts at its end (SC3) and is shorter than L |
| E5 | last awakening at E − 1 | the last opening is that one year; no final phase (reachable only with edited tuning: P3-03 keeps the last age ≥ G ≥ 1 years) |
| E6 | E − y_P = 2 … A | the last opening runs to E − 1 at most; the final phase is ≥ 1 year |
| E7 | scheduled years below the floor | SC4 lengthens the final phase backwards, then the openings in age order |
| E8 | flare-ups reaching the cap | the flare-up is shortened to the room left, or skipped when no year is left |
| E9 | tension still high after a flare-up | a new flare-up can start after the next quiet step |
| E10 | one present race | one opening, at most one quiet interval, the final phase (lengthened by the floor) |

### 5.7 Phase rows

Phase ids 1, 2, … in time order; `age` = the age containing `start_year` (a phase never crosses an age boundary: openings start at awakenings, quiet rows and flare-ups end at the next awakening at the latest, the final phase starts at or after y_P); `kind` 1–4 (P3-00 4.1); `start_year`, `end_year` inclusive (`end_year` = the last year, as `age.end_year`); `trigger_event`: an opening phase's own awakening event (6.3), a flare-up's trigger (6.5), 0 for quiet and final phases.

## 6. The engine

### 6.1 State (in memory; P3-00 4.3)

```cpp
namespace worldgen::history {
enum class EntityType : std::int64_t { race = 1, god = 2, civilization = 3, settlement = 4, region = 5 };  // 6 figure, 7 faction, 8 monster, 9 site: reserved (P5, P6)
enum class PhaseKind : std::int64_t { opening = 1, quiet = 2, flare = 3, final = 4 };   // r3: declared in history/schedule.hpp
enum class SettlementKind : std::int64_t { hamlet = 1, village = 2, town = 3, city = 4, fortress = 5 };   // P3-01 race enum order
enum class SettlementStatus : std::int64_t { thriving = 1, declining = 2, ruined = 3, faded = 4 };

struct Civilization {        // index id − 1; ids 1, 2, … in creation order (H2)
    std::int64_t id, race, culture, home_region, capital /*0 none*/, founded_year, founding_event;
    std::optional<std::int64_t> ended_year; std::int64_t ending_event /*0*/;
    std::string name; std::uint64_t created_step;
};
struct Settlement {
    std::int64_t id, x, y, region, founder_civ, founded_year, founding_event;
    std::int64_t owner_civ, population; SettlementKind kind; SettlementStatus status;
    std::optional<std::int64_t> ruined_year; std::int64_t ruin_event /*0*/;
    std::string name; std::uint64_t created_step;
};
struct RegionState { std::int64_t owner /*0 free*/, owner_event /*0*/; };     // index region id − 1
struct RelationKey { std::int64_t a_type, a_id, b_type, b_id; std::string kind; auto operator<=>(const RelationKey&) const = default; };
struct RelationState { std::int64_t event; };                                  // the event that opened it
struct GeoView {             // read-only, built once from HistoryInputs
    // per region: dry_cells = cells − lake_cells, islet flag, neighbours (ascending id, with the border row index)
};
class State {                // the only mutable world state; subsystems change it through these calls
public:
    const Civilization& civ(std::int64_t id) const; std::span<const Civilization> civs() const;
    const Settlement& settlement(std::int64_t id) const; std::span<const Settlement> settlements() const;
    const RegionState& region(std::int64_t id) const;
    const std::map<RelationKey, RelationState>& relations() const;
    std::int64_t found_civilization(race, culture, home_region, std::string name, event);   // id; year = now
    std::int64_t found_settlement(x, y, region, founder_civ, kind, population, std::string name, event);
    void set_region_owner(region, civ /*0 frees*/, event);
    void set_settlement_owner(id, civ, event); void set_settlement_kind(id, kind, event);
    void set_settlement_status(id, status, event); void set_population(id, population);
    void set_capital(civ, settlement); void end_civilization(civ, event);
    void open_relation(RelationKey, event); void close_relation(const RelationKey&, event);
};
}
```

**Bounds** (H6; each fatal `history: bound: <what> exceeds <n>`): civilizations ≤ 4096, settlements ≤ 1,000,000, events ≤ 10,000,000, participants per event ≤ 64, causes per event ≤ 64, population in [0, 2^31 − 1], ids < 2^31. Relation keys: `kind` matches the P0-03 definition id grammar; a symmetric relation is stored with (a_type, a_id) < (b_type, b_id), an asymmetric one with the subject as a (P3-06 lists its kinds and which are symmetric). A setter that names a missing entity or an ended civilization as owner is fatal (`history: state: <what>`). "A settlement of a region other than its own" (r3): `found_settlement` checks that the cell (x, y) lies on the map and that the `region` layer's value at it is the given region, `history: state: found_settlement: cell (<x>, <y>) lies in region <r'>, not <r>`. The check is unconditional (r6): 3.4 item 6 guarantees the layer is N × N `u16` before the run, so no layer shape skips it. The full message list is 16.6.

### 6.2 Steps and their order

A **step** is one of: an awakening (6.3), a flare-up start (6.3), one tick step T1 … T7 of a year, one quiet sub-step Q1 … Q6. Each step has a serial number, 1, 2, … in run order. Per year of an active phase (opening, flare-up, final), in this order: the awakening if the year is an age's `start_year`; the flare-up start if the year starts a flare-up; T1 … T7 (P3-00 4.2). Per quiet step: Q1 … Q6.

| Step | Owner | P3-04 |
| --- | --- | --- |
| T1 settlements grow or shrink | P3-05 | — |
| T2 civilizations act | P3-05, P3-06 | — |
| T3 figures | P5 | no-op |
| T4 random pressures | P3-05 | — |
| T5 divine interventions | engine | the intervention source (6.8), empty |
| T6 consequences | P3-06 | — |
| T7 bookkeeping | engine | 6.7 |
| Q1 population drift | P3-05 | — |
| Q2 generations | engine | counted from years (6.6); no state |
| Q3 ruin decay | P3-05 | — |
| Q4 slow expansion | P3-05, P3-06 | — |
| Q5 tension | engine | asks the tension sources (6.5) |
| Q6 bookkeeping | engine | quiet eras (6.6), then 6.7 |

Q6 is new against P3-00 4.4 (amendment 18.3): the bookkeeping that T7 does in a tick. Q2 is the engine's because a count of generations is arithmetic on years and `race.generation_years`.

**"Created in a step, processed from the next step on"** (P3-00 4.2): an entity records `created_step`, the serial of the step that created it. In a step of serial s, no subsystem iterates over, scores or chooses as a target an entity with `created_step` = s; the context's iteration helpers skip them (`visible(e)` ⇔ `e.created_step < s`). The creator's own follow-up in the same operation is allowed (the awakening founds a civilization and then its settlement). The engine's bookkeeping (T7, Q6) is a step of its own and sees everything created before it.

### 6.3 The engine's own steps

**Awakening** of age k's race r at y_k (before T1 of y_k; one step): (a) `place` = the awakening handler's `awakening_place(r)` (7.1); (b) append event `awakening` (year y_k, region `place`, participants race r as `race` and god `race.creator_god` as `creator`, no cause); (c) set the opening phase's `trigger_event` to its id; (d) the handler's `awaken(r, awakening_event)` (7.2). P3-00 4.2 puts the event before the placement; the place is chosen first so that the event carries it (design 5: "an event with a god, a year and a place"; P3-00 10.1: the place is the home region). No state changes before (b).

**Flare-up start** at the first year of each flare-up (before T1; one step): append event `flare_up`, a **point event** (`end_year` = `year` = the flare-up's first year; region 0, value `years` = the phase's length, cause = the trigger event). It takes an event id like any other, so when flare-ups occur the awakening events are not at consecutive ids and an opening phase's `trigger_event` is not predictable from the age number alone. Example (r3): ages at 0 and 520, span 6000, `normal`, an awakening handler that founds nothing (so no quiet eras), `ForcedTension(500, 1000, 1)`: phases 1 opening 0–149, 2 quiet 150–499, 3 flare-up 500–519 (cut at the awakening), 4 opening 520–669, …; events e1 `awakening` (year 0), e2 `flare_up` (year 500, cause e1), e3 `awakening` (year 520); phase 4's `trigger_event` is 3.

### 6.4 The step seam

```cpp
namespace worldgen::history {
enum class Step : std::uint8_t { t1 = 1, t2, t3, t4, t5, t6, t7, q1, q2, q3, q4, q5, q6, awakening, flare };  // 14, 15: the engine's own steps of 6.3 (r3)

struct Participant { std::string_view role; EntityType type; std::int64_t id; };
struct EventSpec {
    std::string_view kind;                       // a key of the catalogue
    std::int64_t year, end_year;                 // 6.6 rules; end_year = year for a point event
    std::int64_t region = 0, settlement = 0;
    std::vector<Participant> participants;       // ascending role seq, then the emitter's order (ascending id unless its spec says otherwise)
    std::vector<std::int64_t> values;            // one per value key of the kind, in seq order
    std::vector<std::int64_t> causes;            // strictly ascending
    std::optional<std::int64_t> salience;        // nullopt: the kind's base salience
};

class StepContext {
public:
    Step step() const; std::uint64_t serial() const;
    std::int64_t year() const;                   // the tick year, or the quiet step's start
    std::int64_t years() const;                  // 1, or the quiet step's length
    std::int64_t phase_id() const; PhaseKind phase_kind() const;
    const HistoryInputs& inputs() const; const GeoView& geo() const; const data::ContentSet& content() const;
    const State& state() const; State& state();  // mutations through the State API only
    bool visible(const Civilization&) const; bool visible(const Settlement&) const;
    std::int64_t append(const EventSpec&);       // the event writer, 6.6; returns the new id
    std::string_view event_kind(std::int64_t event) const;    // r8: the key of an event's kind; fatal `history: state: event <e> does not exist`
    std::int64_t event_participant(std::int64_t event, std::string_view role) const;   // r8: the id of the first participant with that role (0 if none); same fatal
    std::int64_t quiet_events_left(std::int64_t civ) const;   // 6.6; outside a quiet step: quiet_events_max (r3)
    std::uint64_t draw(std::uint64_t stream_key, std::uint64_t a, std::uint64_t b, std::uint64_t k) const;  // draw(key, year(), a, b, k), P3-02 4.1
};

class Subsystem {                                // P3-05, P3-06, P5
public:
    virtual ~Subsystem() = default;
    virtual std::span<const Step> steps() const = 0;     // the steps it runs in (T1–T4, T6, Q1, Q3, Q4)
    virtual void run(StepContext&) = 0;                  // its private state is its own members
};
class AwakeningHandler {                         // exactly one: the stub (7), then P3-05
public:
    virtual ~AwakeningHandler() = default;
    virtual std::int64_t awakening_place(const StepContext&, std::int64_t race) = 0;   // a region id; no state change
    virtual void awaken(StepContext&, std::int64_t race, std::int64_t awakening_event) = 0;
};
struct TensionReport { std::int64_t value = 0; std::int64_t trigger_event = 0; };   // r3: declared in history/schedule.hpp
class TensionSource {                            // P3-06
public:
    virtual ~TensionSource() = default;
    virtual TensionReport tension(const StepContext&) = 0;   // called in Q5; must not change state or append
};
struct EngineConfig {
    AwakeningHandler* awakening;                 // not null
    std::vector<Subsystem*> subsystems;          // run order within each step
    std::vector<TensionSource*> tension;         // empty: tension 0
    InterventionSource* interventions;           // not null; the empty source in P3
    const EventCatalogue* catalogue;             // the stage's catalogue (8.2); tests may extend it
};
HistoryResult run_engine(const data::ContentSet&, const HistoryInputs&, const EngineConfig&);
}
```

**Engine-owned steps** (r3): T5, T7, Q2, Q5, Q6 and the two engine steps `awakening` and `flare` are the engine's. A subsystem whose `steps()` lists one of them, or a value outside 1–13, is refused when the engine is configured, fatal `history: engine: a subsystem lists step <n>, which is not a subsystem step` (`<n>` the enumerator's number). `quiet_events_left` returns `quiet_events_max` outside quiet steps (r3).

What a subsystem receives: the step, the year (and length), the phase, the read-only inputs, geography view and data, the state, the event writer and keyed draws. What it may change: the state through `State`'s calls and its own members; nothing else (not the schedule, not another subsystem's members, not an earlier event). The shipped `EngineConfig` of P3-04 is: awakening = the stub (7), no subsystems, no tension source, the empty intervention source, the compiled catalogue with the data's templates. P3-05 and P3-06 replace the stub and add subsystems in the order peoples, conflict; P5 appends. **From generator 0.16.0** (P3-06 8.2, r4) the shipped configuration also installs `ConflictSubsystem` after `PeoplesSubsystem` (steps T2, T6, Q4) and as the only tension source; before 0.16.0 there is no tension source.

### 6.5 Tension

In Q5 the engine calls each tension source in config order; the step's tension is the largest `value`, its trigger that of the first source reporting it. A report with value < 0, or a trigger outside [1, the last event id] when value ≥ the threshold, is fatal (`history: tension: <what>`, texts in 16.6). The tension of a step that ends its interval is not asked for (SC6). With no source the tension is 0 and no flare-up happens: so in the shipped P3-04 world `phase` has no kind 3 rows, and the flare-up path is exercised by the test source of 7.4.

### 6.6 The event writer

`append` assigns the next id (1, 2, … in append order, H2), stores the event in memory and returns the id. It checks, fatal `history: event <id> (<kind>): <what>` (the `<what>` texts are in 16.6, r3):

| # | Check |
| --- | --- |
| W1 | the kind is in the catalogue |
| W2 | year: in a tick, `year` = the tick year; in a quiet step, `year` = the step's start (every event of a quiet step is dated at its start); `year` ≤ `end_year` ≤ E − 1, and in a quiet step `end_year` ≤ the step's last year (the engine's `quiet_era` is lengthened afterwards, 6.6) |
| W3 | `year` ≥ the year of the previous event (so years never decrease with id) |
| W4 | participants: each role is a role of the kind, with its entity type; per role the count is within [`min_count`, `max_count`] (0 = unbounded); in ascending role seq; each id exists (races and gods: a row; regions: a row; civilizations, settlements: created, **or equal to the next id about to be created**, r3: the writer then checks, before the next step begins, that the entity was founded, `history: event <id> (<kind>): <type> <id> was not founded in the step that named it`) |
| W5 | values: exactly one per value key of the kind |
| W6 | causes: strictly ascending, each in [1, id − 1]; non-empty if the kind is `caused` |
| W7 | region and settlement against the kind's `region_use` / `settlement_use` (0 must be 0, 1 must be an existing id, 2 either); a settlement given with a region must lie in it |
| W8 | salience in [0, 100] |
| W9 | in a quiet step: `quiet_events_left(c)` > 0 for the event's **charged civilization** c, the first `civilization` participant (0 if none); appending uses one; `quiet_era` is exempt |

`phase` is the current phase id, set by the engine. Participant `seq` is 1, 2, … in the given order.

**Year order** (strengthens P3-00 6.2; amendment 18.3): `event.year` is non-decreasing with `id`, without exception. Summary events are created at the **start** of the run they summarize and lengthened in memory while the run lasts, so they need no exception: they are dated by their first year and were created then.

**Quiet eras** (Q6, the engine's summary; P3-00 4.4; r10): for each civilization not ended, in id order: if it has an open era in the current quiet phase row, that event's `end_year` becomes the step's last year and its values are recomputed; otherwise a `quiet_era` event is appended (year = the step's start, `end_year` = its last year, region = the civilization's `home_region`). **Events charged to the civilization in the step do not close its era** (r10): a civilization has at most one `quiet_era` per quiet phase row, from the first step of the row in which it exists and is not ended to the last step of the row before it ends (an era is not lengthened in or after the step in which its civilization ends). The era is the stretch of the civilization's history without an active phase: its foundings, kind changes, abandonments, claims and war endings of the same years are dated inside it and stay separate events. All eras close at the end of the quiet phase row. (Before r10, generator 0.14.0 to 0.16.0, an event charged to the civilization closed its open era and the next step opened a new one; at 0.16.0 that split a civilization's quiet row into a median of 84 eras per 1024 world, 33 of them 50 years or shorter: P3-08 4.) Values: `years` = `end_year` − `year` + 1; `generations` = g(`end_year` + 1) − g(`year`), g(t) = (t − `founded_year`) div `race.generation_years` (the ruling generations that began within the era). A run never spans two phase rows, so an era never spans a flare-up or an awakening. Only `end_year` and the two values of an open era change after it is appended; the writer offers this to the engine only.

### 6.7 Bookkeeping (T7, Q6) and the end

At the end of every tick year (T7) and quiet step (after the quiet eras in Q6), with Y = the tick year or the step's start:

1. **Regions**, ascending id: if the owner differs from the owner last recorded, the open `region_control` interval of the region (if any) gets `to_year` = Y, and if the new owner ≠ 0 an interval opens with `from_year` = Y, `civ` = owner, `event` = `owner_event`. Changes within one tick or step that leave the owner as it was record nothing (the last change of the year wins; the events stay in the log).
2. **Relations** (r9, as implemented on `p3/06-conflict` 37d27ab): `State` keeps every `open_relation` and `close_relation` call since the last bookkeeping, in call order. The bookkeeping walks that list: an opening adds a row (`since` = Y, `since_event` = its event, `until` NULL, `until_event` 0); a closing finds the open row of its key and sets `until` = Y and `until_event` = its event. So a relation opened and closed before one bookkeeping has a row with `since` = `until` = Y and both events, and a relation closed and opened again in one bookkeeping (it was opened in an earlier year) has two rows, the old one closed at Y and a new one from Y. The same key opened twice before one bookkeeping (opened, closed and opened again) would need two rows with one primary key and is fatal, `history: state: relation '<kind>' of <a_type> <a_id> and <b_type> <b_id> opened twice before one bookkeeping`; a closing with no open row is fatal, `history: engine: a relation was closed that has no row`. After the walk the open rows must be exactly the state's open relations, else `history: engine: a relation vanished without a closing event`. The rows are then stable-sorted by the primary key (a_type, a_id, b_type, b_id, kind, since) as before (9.3). A row with `since` = `until` is in force in no year under 10's half-open convention (`since` ≤ Y < `until`): it records a relation that began and ended within year Y; so a `peace` closed by a declaration in Y ([…, Y)) and the `war` opened in Y and ended in Y ([Y, Y)) do not overlap.
3. **Census**, ascending settlement id: a row (`year` = Y, the current owner, kind, status, population) for a settlement with no census row yet, or whose owner, kind or status differs from its last row.
4. **Phase end**: if this tick or step ends the phase (its last year is the phase's `end_year`), a census row at `year` = `end_year` for every settlement whose last row differs from its current state in any of the four values (population included). At most one row per (settlement, year): a phase-end row that would repeat the row of item 3 in the same year is not written (it would be identical).

**End of the run** (after year E − 1): open `region_control` rows keep `to_year` NULL; open relations keep `until` NULL; the `civilization` and `settlement` rows hold the end state (`owner_civ`, `kind`, `status`, `population`, `ruined_year`, `ruin_event`, `capital`, `ended_year`, `ending_event`); `phase` rows as built.

### 6.9 Rules for subsystems (normative; from the P3-04 determinism audit, r5)

The audit of the engine (`docs/reviews/p3-04-simulation-engine-determinism-audit.md` on `p3/04-audit`, A1, A2, N1–N5) found the engine itself safe and named hazards for the subsystems that plug into it. Every subsystem (P3-05, P3-06, P5) follows these rules; the subsystem specs state how they meet them.

1. **No live iteration across a creation or a relation change (A1).** Never call `found_civilization`, `found_settlement`, `open_relation` or `close_relation` inside a loop over `civs()`, `settlements()` or `relations()`, and never keep a reference, pointer or span obtained from `State` across such a call. Iterate ids up to a count taken before the loop (`for (id = 1; id <= n; ++id)`, re-fetching `civ(id)` after any founding), or collect the changes and apply them after the loop; for relations, copy the keys first. A subsystem's members hold ids, never references into `State`. The vectors grow differently under different standard libraries (libstdc++ and libc++ double their capacity, MSVC's grows by half), so a violation can read freed memory on one platform and not another. The lint rule `history-live-iteration` reports the range-for form; references held across a founding remain an audit item.
2. **`tension()` is a pure read (A2).** The engine does not call it for the step that ends a quiet interval (SC6, E3), so anything a source accumulated inside it would depend on where awakenings fall. A tension source accumulates in its subsystem steps and `tension()` reports what is stored.
3. **Fixed subsystem order (N1).** `EngineConfig::subsystems` is a vector filled in a fixed order in code (`stages/history.cpp`), never from a container ordered by pointer. Shipped: peoples, then conflict (from 0.16.0). A subsystem whose `steps()` lists one step twice is refused when the engine is configured, fatal `history: engine: a subsystem lists step <n> twice` (`<n>` the step's number, 1 for T1; r6). Two different subsystems may list the same step (peoples and conflict share T2 and Q4).
4. **Visibility in shared steps (N2).** `civs()` and `settlements()` return every entity, also those created in the current step; a subsystem reads entities through `visible()` (6.2). In a step shared by two subsystems, each spec states what it sees of the other's same-step changes: entities created earlier in the step are invisible; region owners, populations and statuses are live state.
5. **Draw keys (N3).** `draw(stream, a, b, k)` fixes w0 to the step's year, so two decisions on one stream with equal (a, b, k) in different steps of one year get the same value. One declared stream per decision kind; a, b are entity ids or ids stored in the world, k a decision ordinal; never a position in a filtered or sorted list.
6. **Symmetric relations are normalized by the subsystem (N4).** `open_relation` stores the key as given; a subsystem stores a symmetric relation with (a_type, a_id) < (b_type, b_id), through one helper.
7. **What one bookkeeping records (N5, 6.7; r9).** Relations: every opening and closing since the last bookkeeping (T7 of the tick year, or Q6 of the quiet step) is recorded in call order (6.7 item 2): opened and closed gives a row with `since` = `until`; closed and opened again gives two rows; the same key opened twice is fatal. Regions keep the net rule: a region whose owner is set several times records the last owner and the last `owner_event`. (P3-06 7.3 delays the first battle of a war to the year after its declaration so that a war's own row spans at least a year.)

### 6.8 Divine interventions (P3-00 9.2)

```cpp
namespace worldgen::history {
struct Intervention { std::int64_t year, god; std::string kind; EntityType target_type; std::int64_t target_id, region; bool scripted; };
class InterventionSource {
public:
    virtual ~InterventionSource() = default;
    virtual std::vector<Intervention> interventions(const StepContext&) = 0;   // in T5 of each tick year; read-only view
};
class EmptyInterventionSource final : public InterventionSource { /* returns {} */ };
}
```

The engine validates each returned intervention in source order: `year` = the tick year, the god has a row, `kind` is a supported intervention kind, the target exists and (for a civilization or settlement) is not ended or ruined, `region` is 0 or a row. **In P3 the supported kind list is empty**, so every intervention is invalid; invalid ones are dropped without an event (design 14: "anything that fails is regenerated or dropped"). P5 fixes the kinds (bless, curse, raise a champion, send a monster, mark a site), the events `divine_<kind>` (god as `actor`, value `divine_script` 1 when `scripted`), the seeded source (keyed draws on `param.divine_activity`) and the script source (`param.divine_script`). Replacing the source changes nothing else because no other decision consumes draws from a shared sequence (H4).

## 7. The stub subsystems (replaced by P3-05 and P3-06)

### 7.1 S1 Stub home region (`awakening_place`)

Score of a region: (`habitability_mean`, `walk_cells`), larger first, then smaller `id`. The home of race r is the best-scoring region of the first non-empty set of:

1. non-islet regions (`cells` − `lake_cells` ≥ `islet_max_dry_cells`) not held (`RegionState.owner` = 0);
2. non-islet regions;
3. all regions.

The stub ignores race preferences (P3-05's homeland rule, P3-00 10.1, replaces it). Measured: the P1-06 validation worlds have at least 11 non-islet regions at 512 (50 worlds), 42 at 1024 (200), 181 at 2048 (20), so with at most six races set 1 is never empty on shipped worlds; sets 2 and 3 exist for hand-made fixtures.

### 7.2 S2 Stub placement (`awaken`)

For race r (civilization id c = the number of civilizations + 1, settlement id s likewise), in one step:

1. name n_c = `make_name(naming of r's culture, NameKind::civilization, seed, culture.id, c, registry)` (P3-02 4.3); append `civ_founded` (region = home, participants c as `civilization`, r as `race`, cause the awakening event), then `found_civilization` (the event names c before it exists; W4 accepts the next id, r3); if the home region is free, `set_region_owner(home, c, civ_founded)`.
2. site: the home region's seed cell (`seed_x`, `seed_y`) if no settlement stands on it; otherwise the first cell in row-major order (y, then x) of the region's bounding box whose `region` layer value is the region's id, whose `water` layer value is not 2 (lake), and on which no settlement stands (fatal `history: stub: no settlement site in region <id>` if none; unreachable for a non-islet region).
3. name n_s = `make_name(…, NameKind::settlement, seed, culture.id, s, registry)`; append `settlement_founded` (region = home, settlement = s, participants s as `settlement`, c as `founder`, value `population` = 200, cause `civ_founded`), then `found_settlement` (likewise) with kind = the first entry of r's `settlement_kinds` (shipped: `hamlet` for every race), status `thriving`, population 200; `set_capital(c, s)`.

`param.civs_per_race` is ignored by the stub (one civilization per present race); P3-05 applies it.

**Registry** (H8, P3-03 8.2): the stage's first step builds the registry from every `region.name`, `age.name` and `god.name` of the inputs; names are then drawn in creation order (civilization, then settlement, per awakening in age order).

### 7.3 S3 Nothing else

No subsystem runs in any tick or quiet step; populations stay 200; no relation is opened; no civilization ends. The log of a stub world is, per age, `awakening`, `civ_founded`, `settlement_founded`, then one `quiet_era` per living civilization and quiet phase row.

### 7.4 S4 The test tension source (test seam, not shipped)

`history::testing::ForcedTension(period, value, trigger)`: reports `value` when the quiet step's end year (its start + length) is a multiple of `period`, else 0; the trigger is the fixed event id `trigger`. It is reachable only through `run_engine` with a test `EngineConfig`; `run_history` never installs it. Golden cases use `ForcedTension(500, 1000, 1)` and `ForcedTension(1, 5000, 1)` (16.4). A period that no quiet step end meets forces nothing (r3): `dense`, span 2000, one race: A = 83, L = 133, the quiet interval [83, 1867) has steps ending at 108, 133, …, all ≡ 8 (mod 25) except the interval's end 1867, where no flare-up starts; so `ForcedTension(25, 5000, 1)` gives no flare-up and 216 simulated years.

## 8. The event catalogue

### 8.1 Where it lives (decision; question 4)

**Kinds, roles, value keys and flags are code; base salience and templates are data** (`history.report`). The structure of a kind is a contract with the code that emits it: a role or value key that no code fills means nothing, and changing one is a schema change that needs a code review anyway. Salience and sentences are taste, tuned by the critic and the report, and P3-00 8.4 puts templates in data. The world carries both (A1): the stage writes the compiled structure with the loaded salience and templates into `event_kind`, `event_kind_role`, `event_kind_value`, so a world renders the same sentences whatever data is installed later.

### 8.2 Compiled catalogue at P3-04

Kind ids in this order; later specs append (P3-05 from 6, P3-06 kinds 20–30 from generator 0.16.0, r4), never renumber. Entity type codes as 6.1. `region_use`, `settlement_use`: 0 never, 1 always, 2 optional. `caused`: 1 = every event of the kind has a cause.

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values (seq: key) | written by |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `awakening` | 0 | 1 | 0 | 1: `race`, race, 1–1; 2: `creator`, god, 1–1 | — | engine (6.3) |
| 2 | `civ_founded` | 1 | 1 | 0 | 1: `civilization`, civilization, 1–1; 2: `race`, race, 1–1 | — | stub, P3-05 |
| 3 | `settlement_founded` | 0 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `founder`, civilization, 1–1 | 1: `population` | stub, P3-05 |
| 4 | `quiet_era` | 0 | 1 | 0 | 1: `civilization`, civilization, 1–1 | 1: `years`; 2: `generations` | engine (6.6) |
| 5 | `flare_up` | 1 | 0 | 0 | — | 1: `years` | engine (6.3) |

`settlement_founded` is uncaused so that P3-05's foundings by expansion need no cause; the stub's foundings have one. Within a kind, role keys and value keys are distinct from each other and from each other's keys, and match `[a-z][a-z0-9_]*` (a compile-time check of the catalogue, and a unit test).

```cpp
namespace worldgen::history {
struct RoleDef { std::int64_t seq; std::string role; EntityType type; std::int64_t min_count, max_count; };
struct KindDef { std::int64_t id; std::string key; bool caused; std::int64_t region_use, settlement_use;
                 std::vector<RoleDef> roles; std::vector<std::string> values; std::int64_t salience; std::string templ; };
struct EventCatalogue { std::vector<KindDef> kinds; const KindDef* find(std::string_view key) const; };
const EventCatalogue& compiled_event_catalogue();                       // structure only: salience 0, empty templates
EventCatalogue event_catalogue(const data::ContentSet&);                // compiled + history.report (what the stage writes)
EventCatalogue read_event_catalogue(const db::WorldReader&);            // the three tables of a world
std::optional<std::string> template_problem(const KindDef&, std::string_view text);   // 8.4, the first problem
}
```

**What is a schema change** (P3-00 6.3 made precise): renaming, removing or renumbering a kind; adding, removing, renaming or reordering a role or value key of an existing kind; changing a role's entity type or counts, or a kind's `caused`, `region_use` or `settlement_use`; changing the meaning of any of them; changing a code list (entity types, phase kinds, settlement kinds and statuses); any column. **Not a schema change**: appending a kind (P3-05, P3-06, P5 do this; `docs/specs/schema.md` lists every kind); changing a template or a base salience (a data change: content hash, version bump, golden re-recording).

**A shipped kind is never edited.** When P3-05, P3-06 or a later spec finds that an existing kind needs one more role or value key, it adds a **new kind** with the structure it needs (for example `settlement_founded_by_migration` beside `settlement_founded`), and its emitters use the new kind from then on; the old kind keeps its structure and its id. The only alternative is a schema change: the change goes to the lead, with a schema version bump, a spec update and a proposals entry (hard rule 7). Readers can therefore rely on a kind id meaning the same structure in every world of a schema version.

### 8.3 Values, participants and names for rendering

The display name of an entity: race → `race.name` (forms `plural`, `adjective` give those columns), god → `god.name`, civilization → `civilization.name`, settlement → `settlement.name`, region → `region.name`.

### 8.4 Template grammar

```
template    = { literal | "{{" | "}}" | placeholder }          // "{{" renders "{", "}}" renders "}"
literal     = any byte except "{" and "}"
placeholder = "{" body "}"
body        = role [ ":" form ] | value | value "?" word "|" word | value "?" word "|" word "|" word
            | "@year" | "@end_year" | "@region" | "@settlement"
role, value = a role key or a value key of the kind
form        = "name" | "plural" | "adjective" | "all"            // plural, adjective: race roles only
word        = zero or more bytes other than "{", "}", "|", "?"
```

The three-word form `{value?w0|w1|w2}` (r10) chooses by the value: w0 for 0, w1 for 1, w2 for any other value; in the chosen word every byte `#` (0x23) is replaced by the value in decimal. It lets a sentence drop a clause whose number is 0 (`{fled?| and one fled| and # fled}`). In the two-word form `#` is an ordinary byte, as before r10. A body with three or more `|` is still an unknown placeholder. Before r10 the three-word body was an unknown placeholder, so no valid template of an earlier world changes its meaning.

Problems reported by `template_problem` (the first, scanning left to right; `<n>` = the byte offset of the `{` or `}`, from 0, except for a non-ASCII byte, reported at its own offset, also inside a placeholder, r3; a placeholder's body runs to the next `}`, and a `{` before that `}` makes the placeholder unclosed, r3; within one placeholder the race-role form check comes before the `min_count` check, r3): `unmatched } at byte <n>`; `unclosed placeholder at byte <n>`; `unknown placeholder '{<body>}' at byte <n>` (body not of the grammar, or naming no role or value of the kind); `form '<form>' needs a race role at byte <n>`; `{@region} needs a kind with a region at byte <n>` (`region_use` ≠ 1), and the same for `{@settlement}`; `role '<role>' may have no participant at byte <n>` (a role with `min_count` 0 used without `:all`); and, after the scan, `the template is empty`. Templates are ASCII (names are, P3-02); a non-ASCII byte is `non-ASCII byte at byte <n>`.

### 8.5 Rendering

```cpp
namespace worldgen::history {
class HistoryView {          // the history tables of a world in memory, read once
public:
    // r1 (P3-07 18.1): errors as values, never FatalError: code corrupt_layer, message
    // `corrupt table '<table>': <detail>` (wording recorded in 16 by the engineer).
    static db::Result<HistoryView> load(const db::WorldReader&);
    const EventCatalogue& catalogue() const;           // as stored: kind keys, role keys, value keys
    // read accessors (names are the engineer's): the number of events; per event its row (year, end_year,
    // kind, phase, region, settlement, salience), participants (seq, role seq, entity type, id) in seq order,
    // values in seq order, causes ascending; the display name of an entity by type and id (8.3), none for 6-9
};
std::string render_event(const HistoryView&, std::int64_t event_id);
}
```

`render_event` scans the event's kind's **stored** template left to right and appends: literal bytes; `{` for `{{`, `}` for `}}`; for `{role}` / `{role:name}` the display name of the first participant with that role (lowest participant `seq`); `{role:plural}`, `{role:adjective}` the race's plural or adjective; `{role:all}` the names of all participants with that role in `seq` order joined as `A`, `A and B`, `A, B and C` (`, ` between all but the last two, ` and ` before the last; empty text for none); `{value}` the value in decimal (`-` for a negative, no leading zeros); `{value?w1|w2}` w1 if the value is 1, else w2; `{value?w0|w1|w2}` (r10) w0 if the value is 0, w1 if it is 1, else w2, with each `#` of the chosen word replaced by the value in decimal (as `{value}`); `{@year}`, `{@end_year}` in decimal; `{@region}`, `{@settlement}` the display name of the event's region or settlement.

**Names inside a sentence** (r10; P1-06 r3 14.7; Paul's decision of 2026-11-19, `docs/design/proposals.md` "Paul's decisions of 2026-11-19 on the region name questions", point 2). Every substitution that renders an entity's display name (`{role}` and `{role:name}`, whatever the role's entity type; each name of `{role:all}`; `{@region}`; `{@settlement}`) applies one rule: if the name's first four bytes are `The ` (0x54 0x68 0x65 0x20) and the name is not appended at a **sentence start**, the bytes appended are `the ` followed by the name from its fifth byte on. A sentence start is byte offset 0 of the text being rendered, or a position directly after the two bytes `. ` (0x2E 0x20) of the text rendered so far (literals and earlier substitutions alike). Any other position is mid-sentence, including after `: `, `; `, `, `, `(` and ` and `. In `{role:all}` each name is judged at the offset where it is appended, so only the first can be at a sentence start (`The Skib Horde and the Court of Saene`). Only `The ` exactly: `Thebes`, `THE `, `There Hollow` and a name equal to `The` are unchanged. Nothing else is transformed: `{role:plural}`, `{role:adjective}`, values, `?`-words, years and literals are copied as they are (a literal `The` of a template keeps its capital); no other capitalization, no trimming, no locale. Stored names and `HistoryView`'s display names never change (tables `region`, `civilization`, `settlement`, `god`, `age` keep `The …`); the rule exists only in the text `render_event` returns. It changes no world byte (sentences are not stored), only every stored golden sentence that holds a `The …` name mid-sentence. An event id outside the log is fatal (`render_event: event <id>: <reason>`): a caller's bug. **`HistoryView::load` validates** (r1, P3-07 18.1), so that rendering a loaded view cannot fail on stored data: every stored template (`template_problem` against its stored kind), the catalogue tables against each other, and every reference of the log (event kinds, participant role seq and entity type against the kind's roles and counts, values one per key, causes below the event, the ids of races, gods, civilizations, settlements and regions it names); a failure is an error value (above), exit 3 in the CLI. It accepts every kind present in the world's catalogue tables, including kinds the running build does not compile (a world of a later generator of the same schema, or a test fixture): the world is self-describing (A1); the compiled catalogue is used only by the writer. P3-07's report, P4's event browser and the game call this function; nobody re-implements it (P3-00 6.3).

Seed 42, 1024, the first events of 17 (r10: region 12 is `Witchgate Barrows` from P1-06 r3, the `quiet_era` template of 4.2): `Fielie awakened the Elves in Witchgate Barrows.` · `The Elves founded the House of Finen in Witchgate Barrows.` · `The House of Finen founded Voeledell in Witchgate Barrows.` · `The House of Finen passed 1924 quiet years, from 150 to 2073, in which 12 new generations of rulers came to power.` (r11: civilization 1's elven form is P3-02 r5's `The House of {root}`; the other stub names of 17 pass the name filter and are unchanged) Names inside a sentence (r10), on templates of P3-05 and P3-06 (illustrative names in the forms of the shipped cultures, not of a particular world): `Fielie awakened the Elves in the Scarred Downs.` (region `The Scarred Downs`); `The Skib Horde declared war on the Hold of Durgarin over the Grove of the Hunt.` (a `The …` name at the start keeps its capital, the two later ones do not); `The Goblins founded the Warrens of Gnonk in West Wood.` (the leading `The` is a template literal); `The Skib Horde and the Court of Saene made peace after 12 years of war, neither the victor.` (`{parties:all}`).

## 9. Schema 7

### 9.1 DDL

`SCHEMA_VERSION`, `user_version` and `meta.schema_version` become **7**. Thirteen statements are added after `age`'s (fourteenth to twenty-sixth), byte for byte:

```
CREATE TABLE event_kind (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, caused INTEGER NOT NULL, region_use INTEGER NOT NULL, settlement_use INTEGER NOT NULL, salience INTEGER NOT NULL, template TEXT NOT NULL) STRICT
CREATE TABLE event_kind_role (kind INTEGER NOT NULL, seq INTEGER NOT NULL, role TEXT NOT NULL, entity_type INTEGER NOT NULL, min_count INTEGER NOT NULL, max_count INTEGER NOT NULL, PRIMARY KEY (kind, seq)) STRICT, WITHOUT ROWID
CREATE TABLE event_kind_value (kind INTEGER NOT NULL, seq INTEGER NOT NULL, key TEXT NOT NULL, PRIMARY KEY (kind, seq)) STRICT, WITHOUT ROWID
CREATE TABLE phase (id INTEGER PRIMARY KEY, age INTEGER NOT NULL, kind INTEGER NOT NULL, start_year INTEGER NOT NULL, end_year INTEGER NOT NULL, trigger_event INTEGER NOT NULL) STRICT
CREATE TABLE event (id INTEGER PRIMARY KEY, year INTEGER NOT NULL, end_year INTEGER NOT NULL, kind INTEGER NOT NULL, phase INTEGER NOT NULL, region INTEGER NOT NULL, settlement INTEGER NOT NULL, salience INTEGER NOT NULL) STRICT
CREATE TABLE event_participant (event INTEGER NOT NULL, seq INTEGER NOT NULL, role INTEGER NOT NULL, entity_type INTEGER NOT NULL, entity_id INTEGER NOT NULL, PRIMARY KEY (event, seq)) STRICT, WITHOUT ROWID
CREATE TABLE event_cause (event INTEGER NOT NULL, cause INTEGER NOT NULL, PRIMARY KEY (event, cause)) STRICT, WITHOUT ROWID
CREATE TABLE event_value (event INTEGER NOT NULL, seq INTEGER NOT NULL, value INTEGER NOT NULL, PRIMARY KEY (event, seq)) STRICT, WITHOUT ROWID
CREATE TABLE civilization (id INTEGER PRIMARY KEY, name TEXT NOT NULL, race INTEGER NOT NULL, culture INTEGER NOT NULL, home_region INTEGER NOT NULL, capital INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event INTEGER NOT NULL, ended_year INTEGER, ending_event INTEGER NOT NULL) STRICT
CREATE TABLE settlement (id INTEGER PRIMARY KEY, name TEXT NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL, region INTEGER NOT NULL, founder_civ INTEGER NOT NULL, founded_year INTEGER NOT NULL, founding_event INTEGER NOT NULL, owner_civ INTEGER NOT NULL, kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT NULL, ruined_year INTEGER, ruin_event INTEGER NOT NULL) STRICT
CREATE TABLE settlement_census (settlement INTEGER NOT NULL, year INTEGER NOT NULL, owner_civ INTEGER NOT NULL, kind INTEGER NOT NULL, status INTEGER NOT NULL, population INTEGER NOT NULL, PRIMARY KEY (settlement, year)) STRICT, WITHOUT ROWID
CREATE TABLE region_control (region INTEGER NOT NULL, from_year INTEGER NOT NULL, to_year INTEGER, civ INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID
CREATE TABLE relation (a_type INTEGER NOT NULL, a_id INTEGER NOT NULL, b_type INTEGER NOT NULL, b_id INTEGER NOT NULL, kind TEXT NOT NULL, since INTEGER NOT NULL, until INTEGER, since_event INTEGER NOT NULL, until_event INTEGER NOT NULL, PRIMARY KEY (a_type, a_id, b_type, b_id, kind, since)) STRICT, WITHOUT ROWID
```

STRICT, no REAL, no BLOB, no foreign-key clause, no secondary index (P4 loads the tables, 10). `event_kind.key` is UNIQUE (looked up by key), so SQLite adds `sqlite_autoindex_event_kind_1`. WITHOUT ROWID for every table with a composite key, as P3-00 6.1 says for `event_kind_role` and `event_participant`, and for the other composite-key tables by the same reasoning (`region_biome`, `race_biome` precedent). Nullable columns: `civilization.ended_year`, `settlement.ruined_year`, `region_control.to_year`, `relation.until`, NULL exactly while the year has not happened (P3-00 6.1 conventions).

**Against P3-00 6.1's outline** (amendment 18.3, all within "column names may still be adjusted" or additions that save a later bump): `event_kind` gains `region_use`, `settlement_use`; `event_kind_role` gains `min_count`, `max_count`; `event_participant.role` is the role's `seq` in `event_kind_role` (INTEGER, not the role text) and `entity_type` an INTEGER code; `event_value.key` becomes `seq`, the value key's `seq` in `event_kind_value`; `civilization` gains `capital`; `settlement` gains `ruin_event`; `relation` gains `until_event`, and its key includes `kind` (two relations of one pair may start in the same year); entity types are INTEGER codes everywhere (the bulk table `event_participant` stays small; the names are in `event_kind_role.role` and the code list in `schema.md` and `entity_type_key()` of the library).

### 9.2 Columns and codes

| Table | Column | Value |
| --- | --- | --- |
| `event_kind` | all | 8.2: `caused` 0/1, `region_use` and `settlement_use` 0/1/2, `salience` the data's base salience 0–100, `template` the data's text |
| `event_kind_role` | `kind`, `seq`, `role`, `entity_type`, `min_count`, `max_count` | 8.2; `max_count` 0 = unbounded |
| `event_kind_value` | `kind`, `seq`, `key` | 8.2 |
| `phase` | `id`, `age`, `kind`, `start_year`, `end_year`, `trigger_event` | 5.7 |
| `event` | `year`, `end_year` | 6.6; `end_year` = `year` for point events |
| | `kind` | `event_kind.id` |
| | `phase`, `region`, `settlement`, `salience` | the phase of `year`; 0 = none; 0–100 |
| `event_participant` | `seq`, `role`, `entity_type`, `entity_id` | 1, 2, … ; `event_kind_role.seq`; the role's code; the id |
| `event_cause` | `cause` | an event id < `event` |
| `event_value` | `seq`, `value` | `event_kind_value.seq`; every key of the kind has exactly one row |
| `civilization` | `race`, `culture` | `race.id`, `culture.id` |
| | `home_region`, `capital` | region id; settlement id or 0 |
| | `founded_year`, `founding_event`, `ended_year`, `ending_event` | the `civ_founded` event (or `civ_seceded` for a civilization born by secession, P3-05 10.3); NULL and 0 while alive |
| `settlement` | `x`, `y`, `region` | its cell (0 ≤ x, y < N) and region |
| | `founder_civ`, `founded_year`, `founding_event` | at founding |
| | `owner_civ`, `kind`, `status`, `population` | end state; `kind` 1 hamlet, 2 village, 3 town, 4 city, 5 fortress; `status` 1 thriving, 2 declining, 3 ruined, 4 faded; `owner_civ` 0 when abandoned |
| | `ruined_year`, `ruin_event` | NULL and 0 unless ruined |
| `settlement_census` | all | 6.7 |
| `region_control` | all | 6.7: `civ` ≥ 1 (free intervals have no row), `event` ≥ 1 |
| `relation` | all | 6.7; `kind` a key of P3-06's list; `until_event` 0 while open |

Entity type codes: 1 race, 2 god, 3 civilization, 4 settlement, 5 region; reserved 6 figure, 7 faction, 8 monster, 9 site. Phase kinds 1 opening, 2 quiet, 3 flare-up, 4 final.

### 9.3 Writing

One stage transaction, through new `StageContext` writers (`write_event_kind`, `write_event_kind_role`, `write_event_kind_value`, `write_phase`, `write_event`, `write_event_participant`, `write_event_cause`, `write_event_value`, `write_civilization`, `write_settlement`, `write_settlement_census`, `write_region_control`, `write_relation`), each in strictly ascending primary key (FatalError otherwise, as the region writers), tables in DDL order, with row types `db::EventKindRow` … `db::RelationRow`, columns in declared order, `std::optional<std::int64_t>` for the nullable ones. Every kind of the catalogue is written, used or not.

### 9.4 `meta.history_end_year`

A new **required** key, = E, canonical decimal (P0-02 5.5), written by `generate_world` for every world with the eight `param.*` keys (P3-03 4.1 step 3; so the empty and test-pattern worlds have it too). The writer derives it from `param.history_span` (r3: no separate setter; `finalize` writes it). The reader checks its canonical decimal form in its place among the required keys, and its equality with `param.history_span` after every other key has been checked (r3) ("meta value invalid", exit 3, the existing message `world file '<path>' has an invalid value '<value>' for meta key 'history_end_year'`); P5's history mode may relax that. `REQUIRED_META_KEYS`, ascending: `content_hash`, `format`, `generator_version`, `history_end_year`, `param.civs_per_race`, … , `stages` (17 keys).

### 9.5 Worlds at schema 7

| World | 13 new tables | `meta.stages` |
| --- | --- | --- |
| default `generate` (= `--until history`) | filled | `elevation,hydrology,climate,biomes,resources,regions,chronology,history` |
| `--until chronology` | present, empty (the catalogue tables too: they are written by the stage) | ends in `chronology` |
| `--until regions` and earlier | present, empty | the stages run |
| `--until none` | present, empty | empty |
| `--test-pattern` | present, empty | `test_pattern` |

`--until history` is accepted and is the default (P3-00 5; `check_stage_limit` returns `none` for it). A reader of version 7 refuses version 6: `world schema version 6, this build reads 7` (exit 3).

### 9.6 Empty-world size (to be confirmed with the vendored SQLite)

Measured by the spec writer: the 0.13.0 `--until none` world (18 pages, schema text 2928 bytes) plus the thirteen statements, the `history_end_year` row and `schema_version` 7, written with `VACUUM INTO` by Python's SQLite 3.46.1: **34 pages, 139264 bytes**. The schema text grows to **5839 bytes** (the thirteen statements are 2911 bytes) and no longer fits page 1, so `sqlite_schema` becomes a two-level b-tree: page 1 is its interior page and **two leaf pages** are added; the other fourteen new pages are the roots of the thirteen tables and `sqlite_autoindex_event_kind_1`. Root pages in that measurement: `event_kind` 19, `sqlite_autoindex_event_kind_1` 20, `event_kind_role` 21, (schema leaves 22, 23), `event_kind_value` 24, `phase` 25, … , `relation` 34. Method for the engineer: `generate --until none` with the new build, then `PRAGMA page_count` and `SELECT type, name, rootpage FROM sqlite_schema ORDER BY rootpage`; record the result in 16.5. The split point, and so the leaf pages' numbers, depend on SQLite's b-tree balancing, which is why the vendored version decides. A spilled page 1 has no correctness impact (P0-02 fixes the page size and writes with `VACUUM INTO`); P0-02 6.1's header checks are unaffected.

## 10. Territory and state by year (for P4)

| Question at year Y | Query | Cost |
| --- | --- | --- |
| owner of region r | `SELECT civ FROM region_control WHERE region = r AND from_year <= Y AND (to_year IS NULL OR Y < to_year)` | primary-key range on `region`; P4 loads the whole table (one row per change; stub worlds: one per civilization) |
| map of owners | all of `region_control`, kept in memory per region, ordered by `from_year`; binary search | 50 regions × a few hundred changes |
| settlement s's state | `SELECT … FROM settlement_census WHERE settlement = s AND year <= Y ORDER BY year DESC LIMIT 1`; exists iff `founded_year` ≤ Y | primary-key range |
| relations at Y | `SELECT … FROM relation WHERE since <= Y AND (until IS NULL OR Y < until)` | full scan of a table of hundreds of rows |
| events of Y | `SELECT … FROM event WHERE year = Y` (or a span overlapping Y: `year <= Y AND end_year >= Y`); P4 loads the log | `event` < 1 MB |
| the phase at Y | `SELECT … FROM phase WHERE start_year <= Y AND Y <= end_year` | tens of rows |

All tables are read once by a viewer and answered in memory; no index is needed (sizes: stub worlds 4–39 events; P3-00 6.1 expects 5,000–30,000 events at 1024 with P3-06). Semantics: the state at Y is the state at the end of year Y (after its events); in a quiet step, changes are dated at the step's first year.

## 11. Report, hash and viewer

### 11.1 `report`: section `[history]`

`[tables]` lists the 26 tables in ascending name. A new section **`[history]`**, after `[chronology]`, printed when table `phase` has a row. Keys in this order, integers only:

| Key | Value |
| --- | --- |
| `end_year` | `meta.history_end_year` |
| `phases_opening`, `phases_quiet`, `phases_flare`, `phases_final` | `phase` rows by kind |
| `simulated_years` | Σ (`end_year` − `start_year` + 1) over kinds 1, 3, 4 |
| `simulated_permille` | `simulated_years` × 1000 div `end_year` |
| `events` | rows of `event` |
| `events_<key>` | rows of `event` of each kind, one line per `event_kind` row in id order (zeros included) |
| `civilizations`, `civilizations_alive` | rows; rows with `ended_year` NULL |
| `settlements`, `settlements_standing` | rows; rows with `status` 1 or 2 |
| `regions_held` | `region_control` rows with `to_year` NULL |
| `census_rows`, `relations` | rows |

Seed 42, 1024, default settings (prototype):

```
[history]
end_year = 6000
phases_opening = 6
phases_quiet = 5
phases_flare = 0
phases_final = 1
simulated_years = 1154
simulated_permille = 192
events = 33
events_awakening = 6
events_civ_founded = 6
events_settlement_founded = 6
events_quiet_era = 15
events_flare_up = 0
civilizations = 6
civilizations_alive = 6
settlements = 6
settlements_standing = 6
regions_held = 6
census_rows = 6
relations = 0
```

Report format stays v2 (P3-03 9.1's reasoning). Every key of `[history]` is also a key of the history report's `[stats]`, with the same definition (r1; P3-07 9). **Corrupt data** (exit 3, nothing on stdout, after the `[chronology]` checks, when `phase` or `event` has rows; first failure, in this order): row shapes as P3-03 9.1 (only the four nullable columns may be NULL); `corrupt table 'phase': row <id>: <reason>` (in this order per row: `id not consecutive`, `start_year <s>, expected <x>`, `end_year below start_year`, `kind <k>`, then, before any arithmetic on the row's years (r6, audit A3), `end_year <e> above <E - 1>` for a row other than the last), `corrupt table 'phase': the last phase ends in <e>, expected <E - 1>` (for the last row with `end_year` > E − 1 the same check reports this message, and after the loop it reports a last row ending below E − 1); `corrupt table 'event': row <id>: <reason>` (`id not consecutive`, `year <y> below the previous event's <p>`, `end_year below year`, `kind <k> is not an event kind`); `corrupt table 'event_cause': row (<e>, <c>): cause not below event`. Everything else (participants against roles, templates) is P3-07's `report --history`, which owns the history format (P3-00 11).

### 11.2 `hash --detail`

Unchanged rules: one `table` line per table in ascending name (thirteen new lines), NULL as tag 0x00; the `schema` line changes with the DDL.

### 11.3 Viewer

**Schema bump only.** The viewer reads worlds through worldgen's reader and is rebuilt for schema 7, refusing schema 6. `gdext-headless-check` generates its real world with every stage (now `history`, about 0.1 s more) and stops reading `report` at the line after `[layers]`'s block, so `[history]` must not disturb it; the viewer engineer confirms on both CI jobs. The timeline is P4.

## 12. Invariants

For every seed, span, detail level, race subset and accepted data set:

1. **Schedule**: phases tile [0, E) in id order (`start_year` of row 1 is 0, each next `start_year` = the previous `end_year` + 1, the last `end_year` = E − 1); each has `end_year` ≥ `start_year`; exactly one kind 1 row per age, starting at the age's `start_year`; at most one kind 4 row, the last row (absent only in case E5); every kind 3 row lies between two kind 2 rows of the same quiet interval or between a kind 2 row and the interval's end; simulated years in [floor_years, cap_years]; the base schedule equals SC1–SC4 recomputed independently; quiet rows and flare-ups follow SC5–SC6 for the given tension.
2. **Log**: ids 1 … n; years non-decreasing with id; `end_year` ≥ `year`; every cause < its event; every caused kind has a cause; participants and values match the catalogue (W4, W5, W7); every id refers to an existing row; `event.phase` is the phase containing `year`; awakening events: one per age, at its `start_year`, the age's race as `race`, its creator god as `creator`, the phase's `trigger_event`.
3. **Catalogue**: the three tables equal `event_catalogue(content)`; every `event.kind` has a row.
4. **Territory**: per region, `region_control` intervals do not overlap and increase in `from_year`; `to_year` > `from_year` when not NULL; at most one open interval; owners exist and were founded no later than `from_year`.
5. **Census**: per settlement, the first row is at `founded_year`; rows are at most one per year; a row exists for every change of owner, kind or status and at each phase end where the state differs from the last row; the last row equals the settlement's end state.
6. **Stub world** (P3-04 only): P civilizations and P settlements, one per present race in race order, homes by S1, sites by S2, names by S2 and the registry rule; P `region_control` rows when every home was free; `phase` has no kind 3 row; `relation` is empty; populations 200; events = 3P + Σ over quiet rows of the civilizations alive then.
7. **Purity**: independent of `--threads`; `history_stage` on `history_inputs_from_world` of a `--until chronology` world equals the full run's tables (H8).

## 13. Determinism notes

| Concern | Rule |
| --- | --- |
| Arithmetic | integers only (H1): the scheduler's divisions are `div` of non-negative `int64`; `scale` < 2^43; no `Fx` |
| Order | phases in time order; steps in fixed order; entities by ascending id; regions, settlements, relation keys by ascending key in bookkeeping; subsystems in config order (H3) |
| Containers | `State` uses vectors indexed by id and `std::map<RelationKey, …>` ordered by integers then the kind text (byte order); no unordered container; the name registry is never iterated (P3-02 7) |
| Draws | the engine draws nothing; names are keyed by (kind, attempt, culture, entity) (H4) |
| Ties | home: (`habitability_mean` desc, `walk_cells` desc, id asc); site: seed cell, then row-major (H5) |
| Resume | H8 through `HistoryInputs` equality (3.2, R1, R2) |
| Writing | one transaction, rows in primary-key order (P0-02) |

**Lint impact**: none expected. The new code uses no floating point, no `<random>`, no unordered containers, no time. `std::map` keyed by a struct with `operator<=>` defaulted over integers and `std::string` is ordered by values, not addresses: the auditor should confirm the lint does not flag it (it flags unordered containers and pointer keys only, `tools/lint/README.md`). Virtual dispatch over subsystems in a fixed vector order is deterministic.

## 14. Validation

### 14.1 Hard bounds (P3-00 12.3, the part the stubs can meet; defects on any seed)

- Order and schedule: one `awakening` per present race, in race order, at its age's `start_year`, with its age row (re-checked from the stored tables by the reference's `check-world`).
- Phases: invariant 1 (tiling, 10–50% bounded by the floor and cap).
- Not empty: every present race has a civilization and a settlement; every civilization alive at E. (P3-00's `min_settlements_alive` 3 at 1024 holds for the default six races; a one-race world has one settlement until P3-05: the sweep checks the bound only on all-race worlds at P3-04.)
- Log integrity: invariants 2, 4, 5.
- Purity: threads 1 and 5 identical; Y2.
- Table sizes: under 5 MB (stub worlds: a few kilobytes).

### 14.2 Statistics for the sweep (from `[history]`)

| Statistic | Hard | Typical (P3-04 stubs) |
| --- | --- | --- |
| `simulated_permille` | 100–450 | sparse 116–117; normal 185–200; dense 268–288 (5.5) |
| `phases_*` | opening = races present; final ≤ 1; flare 0 at P3-04 | 6 / 5–6 / 0 / 1 at all six races (5.5: 12–13 rows) |
| `events` | 3P + Σ over quiet rows of the living civilizations | 6 races: 33 (five quiet rows, 15 eras) or 39 (six quiet rows, 21 eras); at `normal` about 30% of worlds have 39 (r7, from validation), at `dense` 33, at `sparse` 39 |
| `civilizations`, `settlements`, `regions_held` | = P | 6 |

### 14.3 Performance

The engine alone (scheduler, 1,200 tick years and about 200 quiet steps dispatched to no subsystem, bookkeeping over a few entities, names, writing) at 1024: **under 50 ms** in a release build; under 0.2 s at every size and span. Of P3-00 12.1's 15 s for `history` at 1024, the engine's own overhead per simulated year must stay below 10 µs with empty subsystems (a microbenchmark: a tagged doctest case, skipped by default and not in `ctest`'s default run, reports it; to be written, r7). **The microbenchmark (r10, written with P3-08)**: test suite `engine-bench`, skipped by default (`* doctest::skip()`, run with `worldgen-tests -ts=engine-bench --no-skip`), in `worldgen/tests/history/history_engine_bench_test.cpp` (CTest group `-engine`). It builds the `HistoryInputs` (`history_inputs_from_world`, outside the clock) of a 1024 world: the file named by the environment variable `UNMAPPED_ENGINE_BENCH` (any world stored with `--until chronology` or later), else a world it generates into its scratch directory with seed 1, map size 1024 and `--until chronology`, the geography of case `geo-seed-1-1024`. It runs `run_engine` with an `EngineConfig` that installs no subsystem, no awakening handler beyond the stubs S1 and S2 and no tension source, five times, and prints per run the wall time (`std::chrono::steady_clock`: a measurement printed by a test, never stored) and the simulated years (r11: the years of the opening, flare-up and final phase rows, phase kinds 1, 3 and 4); it `CHECK`s that the median time divided by the simulated years is below 10 µs and that the median is below 50 ms in a build with `NDEBUG` (in a debug or sanitizer build it prints and checks nothing). Wall-clock time is used only in this skipped test, never in the library (hard rule 1). The writing of the stub tables is negligible; P3-00's 1 s for writing the history tables applies with P3-06's log.

## 15. Golden cases

- **`world-hashes.sh`**: every geography and `hist-*` case now runs `history`; **`hist-dense-1024`** is added (`--seed 11 --history-detail dense`, P3-00 12.4), the first case where the detail level changes table bytes; `cases` lists fifteen. `p0-worlds.txt`, `p1-worlds.txt` and `p3-worlds.txt` re-recorded, `hist-dense-1024` added to `p3-worlds.txt`, by the validation specialist after the cross-preset comparison.
- **Stage golden** `worldgen/tests/golden/p3-04-engine.txt` (16.4).
- `GENERATOR_VERSION` **0.14.0**. The P1-01 to P1-06, P3-01, P3-02 and P3-03 golden vectors must not change (P3-01's `content_hash` lines do change: the data roots gain two files, so `p3-01-data.txt` is re-recorded by its reference; P3-03's table hashes must not change).

## 16. Tests required

For the independent test writer; the engineer records final signatures here after implementation. The test writer's reference `worldgen/tests/reference/p3_engine_reference.py` (Python, exact integers, on `core_reference`, `p3_names_reference` and `p3_chronology_reference`) reproduces from this spec alone: (a) the base schedule SC1–SC4 and the phase rows SC5–SC6 for a given list of age start years, span, tuning and a tension function; (b) the whole stub world: every row of the thirteen tables, from a world's stored region tables and layers `region`, `water`, the chronology rows and the data root; (c) `template_problem` and `render_event`; (d) the `[history]` keys. It does not reproduce P3-05 or P3-06.

**Rendering is an independent contract.** The reference implements the template grammar and the rendering algorithm of 8.4 and 8.5 itself, from this spec, without calling or porting the C++ (`template_problem`, `render_event`). It renders every event of every golden world from the world's stored `event_kind` templates and the stored names, and the golden file records each sentence (16.4); the C++ `render_event` must reproduce every sentence **byte for byte**. The sentences are a cross-platform contract: P3-07's `report --history`, P4's browser and the game print them, so a difference between the two implementations, or between compilers, is a defect, not a tolerance.

### 16.1 Unit tests

1. SC1: `scale` at E = 2000, 2001, 6000, 11999, 12000 for each shipped length; the shipped A, F, L table of 5.5.
2. Each edge case E1–E10 on hand-made age lists (for example E1: starts 0, 100 with A = 150; E5: starts 0, 5999 at E = 6000; E7: `elf,orc` seed 3 → elves' opening 202 years), with the expected phase table written in the test.
3. Guards of 3.4 and 5.4–5.6, each with its message (hand-made inputs through the seams, including a tuning with 6A + L above the cap passed past the loader).
4. Event writer W1–W9: each check failing once with its message; a quiet-step event dated after its step's start; an eleventh append in a quiet step with `quiet_events_max` 10.
5. Created-step rule: a test subsystem in T2 that founds a settlement and then iterates `visible` settlements does not see it; T3 does.
6. Bookkeeping: a test subsystem that changes a region's owner twice in a year (net change and no net change), opens and closes relations, changes kind, status and population; the `region_control`, `relation` and census rows of 6.7 written in the test; a `peace` between civilizations 1 and 2 opened and closed in year 7 (events e13, e14) gives the row (3, 1, 3, 2, `peace`, 7, 7, e13, e14) (r9); closed and reopened in one bookkeeping, two rows; opened twice, the fatal message of 6.7 item 2.
7. Quiet eras (r10): a run is **not** broken by an event charged to its civilization (a test subsystem that appends one event charged to civilization 1 in every second quiet step leaves one era per quiet row); runs broken by a flare-up and by an awakening; a civilization ended in a quiet step keeps the era as of its last full step; `generations` across a generation boundary; a civilization created in an opening phase gets its first era in the next quiet row.
8. Interventions: the empty source; a test source returning interventions (any kind) — all dropped, tables identical to the empty source's.
9. Catalogue: `compiled_event_catalogue()` equals 8.2 transcribed; keys distinct; `event_catalogue(shipped)` has the shipped salience and templates.
10. Templates: every problem of 8.4 once (unmatched `}`, unclosed, unknown body, `:plural` on a non-race role, `{@settlement}` on `awakening`, empty, non-ASCII, a `min_count` 0 role without `:all` on a test kind); `{{`/`}}`; `:all` with 0, 1, 2, 3 participants; `?` with 0, 1, 2; the three-word form (r10) with 0, 1, 2 and −1, `#` replaced in the chosen word only, `#` kept as a byte in the two-word form, a body with four words an unknown placeholder. **Names inside a sentence** (r10, P1-06 r3 14.7): each example of 8.5 through `render_event` on a fixture world; a name `The` alone, `Thebes`, `There Hollow` unchanged; a name after `. ` inside a template (a fixture template `It ended. {civilization} was no more.`) keeps its capital; after `: `, `; `, `(` lower-cased; `{role:all}` with three `The …` names at the start (`The A, the B and the C`) and mid-sentence (all three lower-cased); the stored `name` columns and `HistoryView`'s display names unchanged after rendering.
11. `history.report` named check: each message once on a fixture root, positions at the definition; the unknown-id messages of P3-01 updated (4.2).
12. Schema: `SCHEMA_VERSION` 7; the DDL of 9.1 byte for byte; the empty world's page count of 16.5; refusal of a version 6 file; round trip of every new row type including NULLs; no REAL (`typeof`); `history_end_year` written for every world kind of 9.5 and refused when it differs from `param.history_span`.
13. Report: `[history]` on a hand-made world recomputed by the test; absent with an empty `phase`; each corrupt case of 11.1; `[tables]` lists 26 tables.
14. CLI: default `meta.stages` ends `,history`; `--until history` gives the same bytes as the default; `--until chronology` leaves the thirteen tables empty.

### 16.2 Property tests

15. Scheduler over seeds 0–99 by default and the full 0–999 in the `history-slow` suite (r7) × spans 2000, 2001, 6000, 11999, 12000 × the three levels × all 63 subsets of the shipped races (age starts from `awakening_years`): invariant 1 with SC1–SC4 recomputed by independent code; and with `ForcedTension(25 × k, 5000, 1)` for k = 1, 3, 7: the flare rows of SC6 recomputed, simulated years ≤ cap.
16. Hand-made tuning within schema ranges (seeded random lengths satisfying the named checks, and race shares packed close as in P3-03 14.2 item 14): invariant 1 always, no guard fires.
17. Stub worlds on fixture worlds through `generate_world_with_stages` with a synthetic regions stage (2 to 60 regions, some islets, some fewer non-islet regions than races so that S1 sets 2 and 3 and the S2 row-major fallback are reached), seeds 0–15, spans and subsets varied: invariants 2–6, rows equal to the reference.

### 16.3 Determinism and resume

18. Threads 1, 2, 5: identical history tables.
19. **R1 (A2)**: for each world case of 16.4 and the fixture worlds of item 17, `history_inputs_from` captured in the pipeline (a test stage through `generate_world_with_stages` that calls it) equals `history_inputs_from_world` on the written file, member by member with a message naming the first differing member.
20. **R2 (A2)**: the test destructures `HistoryInputs` and `HistoryParams` with structured bindings of exactly their current member counts (`const auto& [seed, map_size, params, regions, …] = inputs;`), so adding a member fails to compile until the test, both constructors and R1 are extended. The engineer adds a comment at each struct pointing to this test.
21. **Y2 Resume**: `--until chronology` world → `history_inputs_from_world` → `history_stage` → rows equal the default world's thirteen tables; each refusal of 3.2 once.
22. **Y3 Draw independence**: a test subsystem in T1 and Q1 that makes keyed draws on a test stream and changes nothing leaves every table unchanged; adding a test-only extra draw before the stub's names changes nothing.
23. **Y4 Ordering**: a test subsystem that opens the same set of relations and sets the same region owners in two different call orders within one step gives byte-identical tables.
24. `history_stage` twice on the same inputs gives the same rows (no hidden state).

### 16.4 Golden vectors

25. `worldgen/tests/golden/p3-04-engine.txt`, written by the independent reference:
    - **Schedule cases**: seeds 0, 1, 42, 2^64 − 1, `phrase_seed("the unmapped")` × spans 2000, 6000, 12000 × the three levels × races `all`, `elf,orc`, `orc`, `goblin,dwarf,human`; one line `<case> phases <id:age:kind:start:end|…>` (trigger omitted: it needs events) and one `<case> simulated <n>`; plus each with `ForcedTension(500, 1000, 1)` and `ForcedTension(1, 5000, 1)` at `normal`.
    - **World cases**: the four P1 golden cases, the three `hist-*` cases: logical hashes (P0-04 8) of the thirteen tables, the `[history]` lines, and the rendered sentence of every event (`<case> e<id> <text>`). The 1024 cases in a `*-slow` suite.
    - The worked example of 17 must appear unchanged (the test writer states where).

### 16.5 Measured values to confirm

The empty world of 9.6 (34 pages, 139264 bytes; schema text 5839 bytes; two `sqlite_schema` leaf pages) with the vendored SQLite; the engineer records the confirmed figures and root pages here.

**Confirmed by the engineer (2026-11-19, vendored SQLite 3.54.1, generator 0.14.0, `generate --seed 0 --until none`):** 34 pages, 139264 bytes, schema text 5839 bytes. Root pages: `meta` 2, `layer` 3, `sqlite_autoindex_layer_1` 4, `pass` 5, `biome` 6, `sqlite_autoindex_biome_1` 7, `climate` 8, `region` 9, `region_border` 10, `region_biome` 11, `race` 12, `sqlite_autoindex_race_1` 13, `race_biome` 14, `culture` 15, `sqlite_autoindex_culture_1` 16, `god` 17, `age` 18, `event_kind` 19, `sqlite_autoindex_event_kind_1` 20, `event_kind_role` 21, (`sqlite_schema` leaves 22, 23), `event_kind_value` 24, `phase` 25, `event` 26, `event_participant` 27, `event_cause` 28, `event_value` 29, `civilization` 30, `settlement` 31, `settlement_census` 32, `region_control` 33, `relation` 34: identical to 9.6.

**Final signatures** (engineer): `worldgen/include/worldgen/history/{inputs,schedule,engine,stubs,event_catalogue}.hpp`, `worldgen/include/worldgen/stages/history.hpp`, `worldgen/include/worldgen/inspect/history.hpp`, typed row readers `db::read_<table>_rows` in `db/reader.hpp`. `HistoryResult` is `history::HistoryResult` (`stages::HistoryResult` is an alias); `PhaseKind` and `TensionReport` live in `history/schedule.hpp`; `Step` has two more enumerators, `awakening` and `flare`, for the engine's own steps of 6.3. `HistoryView::load` errors (r1): code `corrupt_layer`, message `corrupt table '<table>': <detail>` (for example `corrupt table 'event': row 7: kind 9 is not an event kind`, `corrupt table 'event_kind': row 4: template: unclosed placeholder at byte 3`, `corrupt table 'event_participant': row (3, 1): civilization 9 does not exist`); a table that cannot be read returns the reader's own error. The `[history]` values are `inspect::history_stats` over `inspect::read_history_tables` (11.1 r1).

### 16.6 Fatal messages (normative, r3)

Every `FatalError` text of the engine, the stage and the stubs, as implemented on `p3/04-int` and confirmed by the lead's ruling of 2026-11-19. Tests compare these texts exactly; where another section names a message, this list is the authority. All texts are ASCII (messages are part of the cross-platform contract). `<...>` are decimal integers unless stated; `<E - 1>` and similar placeholders print one number.

| Group | Messages |
| --- | --- |
| Entry (3.4 item 1) | `history: input precondition: no region rows`; `history: input precondition: no age rows`; `history: input precondition: region ids are not 1 to <n>`; `history: input precondition: a region_border row names a region that does not exist` |
| Tuning (3.4 item 2, 4.1) | `history: tuning: the content has no definition history.engine`; `history: tuning: <field> outside its schema range` (level fields as `<level>.<field>`); `history: tuning: simulated_min_permille must not be above simulated_max_permille`; `history: tuning: <level>: 6 x active_years + final_years must not be above 6 x simulated_max_permille - 30` |
| Catalogue data (3.4 item 3) | `history: report: the content has no definition history.report`; `history: report: <message>` (the first message of `report_events`, 4.2) |
| Settings (3.4 item 4) | `history: settings: history_span <E> outside 2000 to 12000`; `history: settings: param.races does not match the present race rows` |
| Chronology rows (3.4 item 5) | `history: chronology: the age rows are not ids 1 to <P> in order`; `history: chronology: age <id>: the first start_year is <s>, expected 0`; `history: chronology: age <id>: start_year <s> is not the previous end_year + 1`; `history: chronology: age <id>: end_year below start_year`; `history: chronology: age <id>: race <r> is not a present race`; `history: chronology: age <id>: race <r> awakens in <y>, expected its start_year <s>` (`<y>` is `no year` for a NULL `awakening_year`); `history: chronology: the last end_year is <e>, expected <E - 1>`; `history: chronology: no god row for present race <r>` |
| Region layer (3.4 item 6; r6) | `history: engine: layer 'region' is not <N> x <N> u16` |
| Resume (3.2) | `history: resume: schema version <v>, this build reads 7`; `history: resume: the world's data content hash differs from the loaded data`; `history: resume: the world was generated by <version>, this build is <version>`; `history: resume: meta.stages is '<stages>', expected the stages up to chronology`; `history: resume: <what>: <reader message>` (a table or layer the reader cannot return) |
| Schedule (5) | `history: schedule: span <E> below 2`; `history: schedule: the age start years must begin at 0`; `history: schedule: the age start years must increase`; `history: schedule: an age starts at or after the span`; `history: schedule: scheduled active years <sched> above the cap <cap>`; `history: schedule: quiet step length <n> below 1`; `history: schedule: flare-up length <f> outside [1, <m>]`; `history: schedule: phase <id> ends before it starts`; `history: schedule: phase rows do not tile [0, <E>) at phase <id>`; `history: schedule: phase rows do not tile [0, <E>) at the end`; `history: schedule: simulated years <n> outside [<floor>, <cap>]` |
| Engine configuration (6.4) | `history: engine: the configuration needs an awakening handler, an intervention source and a catalogue`; `history: engine: a subsystem is null`; `history: engine: a tension source is null`; `history: engine: a subsystem lists step <n>, which is not a subsystem step`; `history: engine: a subsystem lists step <n> twice` (r6) |
| Engine bookkeeping (6.7; internal guards) | `history: state: relation '<kind>' of <a_type> <a_id> and <b_type> <b_id> opened twice before one bookkeeping` (r9); `history: engine: a relation was closed that has no row` (r9); `history: engine: a relation vanished without a closing event`; `history: engine: two census rows of settlement <s> in year <y>` |
| Awakening (6.3) | `history: awakening: the place <r> of race <race> is not a region` |
| Tension (6.5) | `history: tension: value <v> below 0`; `history: tension: trigger event <t> outside [1, <n>]` |
| Bounds (6.1) | `history: bound: civilizations exceeds 4096`; `history: bound: settlements exceeds 1000000`; `history: bound: events exceeds 10000000`; `history: bound: participants per event exceeds 64`; `history: bound: causes per event exceeds 64`; `history: bound: population exceeds 2147483647`; `history: bound: steps exceed 2^64 - 1` |
| Event writer (6.6), prefix `history: event <id> (<kind>): ` | W1 `the kind is not in the catalogue`; W2 `year <y>, expected <x>`, `end_year <e> outside [<y>, <max>]`; W3 `year <y> below the previous event's <p>`; W4 `'<role>' is not a role of the kind`, `participant <i> of role '<role>' is a <type>, expected a <type>`, `participants are not in ascending role seq`, `<type> <id> does not exist`, `role '<role>' has <n> participants, expected <min> to <max>` (`<max>` is `any` for 0), `<type> <id> was not founded in the step that named it` (checked before the next step, 7.2); W5 `<n> values, expected <m>`; W6 `cause <c> outside [1, <id - 1>]`, `causes are not strictly ascending`, `the kind needs a cause`; W7 `the kind has no <region|settlement>, but <region|settlement> <v> is given`, `the kind needs a <region|settlement>; <v> is not one`, `<region|settlement> <v> does not exist`, `settlement <s> does not lie in region <r>`, `settlement <s> was not founded in the step that named it`; W8 `salience <v> outside [0, 100]`; W9 `civilization <c> has no quiet events left in this step` (`<type>` is the entity type key: `race`, `god`, `civilization`, `settlement`, `region`) |
| State (6.1), prefix `history: state: ` | `<call>: event <e> does not exist`; `<call>: civilization <c> does not exist`; `<call>: civilization <c> has ended`; `<call>: settlement <s> does not exist`; `civilization <c> does not exist`; `settlement <s> does not exist`; `region <r> does not exist`; `race <r> has no generation_years above 0`; `found_civilization: race <r> does not exist`; `found_civilization: culture <c> does not exist`; `found_civilization: region <r> does not exist`; `found_settlement: cell (<x>, <y>) is outside the map`; `found_settlement: region <r> does not exist`; `found_settlement: cell (<x>, <y>) lies in region <r'>, not <r>`; `found_settlement: kind is not a settlement kind`; `set_region_owner: region <r> does not exist`; `set_settlement_kind: kind is not a settlement kind`; `set_settlement_status: status is not a settlement status`; `open_relation: kind '<kind>' is not of the definition id grammar`; `open_relation: an entity of the relation does not exist`; `open_relation: relation '<kind>' is already open`; `close_relation: relation '<kind>' is not open` (`<call>` is the `State` function: `found_civilization`, `found_settlement`, `set_region_owner`, `set_settlement_owner`, `set_settlement_kind`, `set_settlement_status`, `set_population`, `set_capital`, `end_civilization`, `open_relation`, `close_relation`) |
| Stub (7) | `history: stub: no region`; `history: stub: no settlement site in region <r>`; `history: stub: race <r> has no row`; `history: stub: culture <c> has no row`; `history: stub: race '<key>' or culture '<key>' is not in the content`; `history: stub: race '<key>' has no settlement kind`; `history: stub: '<text>' is not a settlement kind`; `history: stub: the civilization id is not the expected one`; `history: stub: the settlement id is not the expected one`; test seam: `ForcedTension: period below 1` |
| Catalogue (8.2; self-checks of the compiled catalogue and of `read_event_catalogue`) | `compiled_event_catalogue: kind <id> has a bad id or key`; `compiled_event_catalogue: kind key '<key>' is used twice`; `compiled_event_catalogue: kind '<key>' has a region or settlement use outside 0 to 2`; `compiled_event_catalogue: kind '<key>' has a bad role '<role>'`; `compiled_event_catalogue: kind '<key>' has a bad value key '<key>'`; `compiled_event_catalogue: kind '<key>' repeats a role or value key`; `read_event_catalogue: event_kind ids are not 1, 2, ... at id <id>`; `read_event_catalogue: event_kind_role row (<kind>, <seq>) is not valid`; `read_event_catalogue: event_kind_value row (<kind>, <seq>) is not valid` |
| Rendering (8.5), prefix `render_event: event <id>: ` | `not in the log` (the only one reachable through `HistoryView::load`, r1); guards for a view built otherwise: `kind <k> is not an event kind`, `no name for <type> <id>`, `no participant with role '<role>'`, `no region <r>`, `no settlement <s>` |

Template problems (8.4) and the `report_events` messages (4.2) are listed in those sections; `HistoryView::load`'s corrupt-table errors are error values, not fatal (16.5, r1).

## 17. Worked example: seed 42, 1024, span 6000, normal, all races

Chronology (P3-03 7.1): ages start 0, 2074, 3242, 4243, 4969, 5596. SC1: A = 150, F = 50, L = 300, Q = 25, floor 600, cap 2700. SC2: openings end 150, 2224, 3392, 4393, 5119, 5746. SC3: S_f = max(5700, 5746) = 5746 (E4). SC4: scheduled 900 + 254 = 1154 ≥ 600.

| Phase | Age | Kind | Years | Trigger | Steps |
| --- | --- | --- | --- | --- | --- |
| 1 | 1 | opening | 0–149 | e1 | |
| 2 | 1 | quiet | 150–2073 | 0 | 77 (76 × 25 + 24) |
| 3 | 2 | opening | 2074–2223 | e5 | |
| 4 | 2 | quiet | 2224–3241 | 0 | 41 (40 × 25 + 18) |
| 5 | 3 | opening | 3242–3391 | e10 | |
| 6 | 3 | quiet | 3392–4242 | 0 | 35 (34 × 25 + 1) |
| 7 | 4 | opening | 4243–4392 | e16 | |
| 8 | 4 | quiet | 4393–4968 | 0 | 24 (23 × 25 + 1) |
| 9 | 5 | opening | 4969–5118 | e23 | |
| 10 | 5 | quiet | 5119–5595 | 0 | 20 (19 × 25 + 2) |
| 11 | 6 | opening | 5596–5745 | e31 | |
| 12 | 6 | final | 5746–5999 | 0 | |

Stub homes (S1, best `habitability_mean` free non-islet region): elves region 12 `Northeast Downs IV`, goblins 10 `Northeast Downs II`, dwarves 14 `Northeast Wood`, faeries 30 `West Downs`, humans 18 `Northeast Downs V`, orcs 4 `Northwest Downs II` (r11: from 0.17.0 the regions are named `Witchgate Barrows`, `Knelldown`, `Northeast Wood`, `Vetchdown`, `Northeast Downs` and `Warlord's Rises`, P1-06 r3 14.4; the table below is otherwise unchanged). Civilizations 1–6: `Finen` (r11: `The House of Finen` from 0.17.0, P3-02 r5 11.4), `The Skib Horde`, `The Kaindnimm Throne`, `The Circle of Fysoli`, `The Kingdom of Alird`, `The Warband of Baaldkrok`. Settlements 1–6 (hamlets of 200 at the seed cells): `Voeledell` (755, 293), `Skrisligg Pit` (808, 251), `Daidmin` (722, 376), `Lowhou` (304, 534), `Aledsealdbury` (678, 426), `Uld'kag` (241, 201).

| Event | Years | Kind | Phase | Region | Participants (role: type id) | Values | Causes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| e1 | 0 | awakening | 1 | 12 | race: race 1; creator: god 1 | | |
| e2 | 0 | civ_founded | 1 | 12 | civilization: civ 1; race: race 1 | | e1 |
| e3 | 0 | settlement_founded | 1 | 12 (settlement 1) | settlement: 1; founder: civ 1 | population 200 | e2 |
| e4 | 150–2073 | quiet_era | 2 | 12 | civilization: civ 1 | years 1924, generations 12 | |
| e5–e7 | 2074 | awakening, civ_founded, settlement_founded (goblins, civ 2, settlement 2) | 3 | 10 | | population 200 | –, e5, e6 |
| e8, e9 | 2224–3241 | quiet_era (civ 1, civ 2) | 4 | 12, 10 | | years 1018; generations 7, 67 | |
| e10–e12 | 3242 | dwarves (civ 3, settlement 3) | 5 | 14 | | | –, e10, e11 |
| e13–e15 | 3392–4242 | quiet_era (civ 1–3) | 6 | 12, 10, 14 | | years 851; generations 6, 57, 12 | |
| e16–e18 | 4243 | faeries (civ 4, settlement 4) | 7 | 30 | | | –, e16, e17 |
| e19–e22 | 4393–4968 | quiet_era (civ 1–4) | 8 | 12, 10, 14, 30 | | years 576; generations 4, 39, 8, 6 | |
| e23–e25 | 4969 | humans (civ 5, settlement 5) | 9 | 18 | | | –, e23, e24 |
| e26–e30 | 5119–5595 | quiet_era (civ 1–5) | 10 | 12, 10, 14, 30, 18 | | years 477; generations 3, 31, 7, 5, 19 | |
| e31–e33 | 5596 | orcs (civ 6, settlement 6) | 11 | 4 | | | –, e31, e32 |

33 events; `region_control`: six rows (region, from, NULL, civ, the `civ_founded` event): (4, 5596, 6, e32), (10, 2074, 2, e6), (12, 0, 1, e2), (14, 3242, 3, e11), (18, 4969, 5, e24), (30, 4243, 4, e17); `settlement_census`: six founding rows (s, y_k, civ, 1, 1, 200), no phase-end rows (nothing changes). Simulated 1154 years (192 per mille).

**With `ForcedTension(500, 1000, 1)`** (16.4): the first quiet interval becomes quiet 150–499, flare-up 500–549 (trigger e1), quiet 550–999, flare-up 1000–1049, quiet 1050–1499, flare-up 1500–1549, quiet 1550–1999, flare-up 2000–2049, quiet 2050–2073; the later intervals' step ends are not multiples of 500 (their steps start at 2224, 3392, …), so they are unchanged; 20 phases, simulated 1354; events e4 `quiet_era` 150–499, e5 `flare_up` 500 (years 50, cause e1), e6 `quiet_era` 550–999, …, e12 `quiet_era` 2050–2073 (years 24, generations 0), e13 the goblin awakening; 41 events. **With `ForcedTension(1, 5000, 1)`** (a flare-up after every step that can have one): 75 phases, 32 flare-ups, simulated years exactly the cap, 2700.

## 18. Amendments requested

### 18.1 `docs/specs/schema.md` (the engineer, with the implementation)

1. Header: current version 7 (generator 0.14.0; version 6 is 0.13.0).
2. Section 1 "DDL at version 7": the thirteen statements of 9.1 after `age`'s; a paragraph per group (catalogue, phases and log, entities, by-year tables) pointing to this spec's 9.2, with the code lists (entity types, phase kinds, settlement kinds and statuses).
3. The autoindex sentence gains `sqlite_autoindex_event_kind_1`; "The empty world is 34 pages, 139264 bytes (18 at version 6); its schema text no longer fits page 1" (with the confirmed figures).
4. Section 2 "Required `meta` keys at version 7": `history_end_year`; `schema_version` 7.
5. A new section "Event catalogue": the kinds table of 8.2 (kept current by every spec that appends a kind) and the schema-change rule of 8.2.
6. Section 4: version 7 moves to sections 1 and 2; no planned change listed.
7. Changelog: "7 | 2026-11-19 | 6 → 7 with generator 0.14.0 (the engine merge): thirteen tables added, executed fourteenth to twenty-sixth; required `meta` key `history_end_year`; readers refuse version 6; the empty world grows from 18 to 34 pages. Proposals "History: schema versions 6 and 7" | P3-04 9, P3-00 6.1".

### 18.2 P0-02, P0-04, P0-01, P1-00, P3-01, P3-02, P3-03 (applied 2026-11-19 on `p3/04-spec`: P0-02 amendment 19, P0-04 amendment 20, P0-01 section 17, P1-00 appendix C item 24, P3-01 r4, P3-02 r4, P3-03 r2)

- **P0-02** amendment "Schema version 7": the thirteen statements, version 7, the empty-world layout of 9.6, `history_end_year` (writer and reader), refusal text.
- **P0-04** amendment: `generate` runs `history` by default; `--until history` accepted; the stage list message unchanged (it already names eight stages); `report` lists 26 tables and prints `[history]`; the `--until none` report text gains `history_end_year` and thirteen `[tables]` rows; `world-hashes.sh` gains `hist-dense-1024`.
- **P0-01**: no new stream (state it in the stream list's note: P3-04 draws only names).
- **P1-00**: stage 8 `history` (P3-04) in section 3's table and 10.1.
- **P3-01**: section 5 table: `engine` and `report` added by P3-04; 5.2 and 5.3 describe them (this spec's 4.1, 4.2); 8.1 item 4's expected list `'chronology', 'engine', 'report'` and its example id `peoples`.
- **P3-02**: 4.5: civilization and settlement names are drawn by the `history` stage from P3-04 on (stub) and P3-05.
- **P3-03**: 4.3: `--until history` accepted from P3-04; 5.1: the empty world is 34 pages from schema 7; 8.2: the rule is applied by P3-04 S2.

### 18.3 P3-00 (corrections; where this spec departs from it; applied in place 2026-11-19 with P3-00's second correction note)

1. **4.1**: lengths scale with the span (5.1); the final phase is at most `final_years` and starts no earlier than the end of the last opening phase (5.3, E4); the floor and cap (5.4, SC6) make the 10–50% bound a property of the scheduler; phase `end_year` is inclusive; `trigger_event` of an opening phase is its awakening event, 0 for quiet and final phases.
2. **4.2**: the awakening chooses the place before appending the event (6.3), so the event carries it; a flare-up start is an engine step with event `flare_up`.
3. **4.4**: Q2 is the engine's (generations from years); new Q6 bookkeeping; every event of a quiet step is dated at the step's start; `quiet_events_max` counts per charged civilization, `quiet_era` excluded.
4. **6.1**: the column changes listed at the end of 9.1; `meta.history_end_year` is written for every world (9.4).
5. **6.2 and 12.3**: years are non-decreasing with id without exception (summaries are created at their first step and lengthened, 6.6); the "summary rule" exception is removed.
6. **6.3**: catalogue structure in code, salience and templates in `history.report` (8.1); the precise schema-change rule of 8.2; the template grammar is P3-04's (8.4), not P3-07's (P3-00 11 "Rendering").
7. **10.1** is P3-05's; P3-04's stub home rule (7.1) is interim and not a reading of it.
8. **12.3** "Not empty": `min_settlements_alive` is checked from P3-05 on (14.1).

## 19. Open questions for Paul

Each with the recommendation and the interim choice this spec follows.

1. **Phase lengths scale with the span.** Recommendation and interim: yes (5.1); the detail level fixes the share of simulated years (sparse about 12%, normal 19–20%, dense 27–29%), so a 12,000-year history has twice as long active phases as the default and a 2,000-year one a third. Alternative: fixed years, which breaks the 10–50% bound at both ends of the span range.
2. **The last age is mostly active.** With the shipped numbers the orcs' opening phase usually runs straight into the final phase (73% of seeds at normal, all at dense), so the last four centuries are simulated year by year. Recommendation and interim: accept; it is the design's "most recent age of conflict". Alternative: a shorter `active_years` for the last age.
3. **Flare-up cap 45% and floor 10%.** Recommendation and interim: as tuning (4.1), guaranteed by the scheduler.
4. **Catalogue in code, templates and salience in data** (8.1). Recommendation and interim: yes.
5. **Resume is a library seam, not a `generate --resume` option** (3.2). Recommendation and interim: yes until P5's history mode needs a command.
6. **A `flare_up` event marks each flare-up.** Recommendation and interim: yes, so a reader of the log sees why detail resumes; it costs one event per flare-up.

## 20. Wording and quiet stretches (amendment r12, P3-10, generator 0.19.0)

Normative for the change P3-10 "wording and site follow-ups" (`docs/specs/p3-10-followups.md`), from generator **0.19.0**, not before. Each subsection replaces or extends the section it names; the text of those sections describes generator 0.14.0 to 0.18.0. **No schema change** (20.5).

### 20.1 One quiet line per quiet stretch (replaces 6.6 "Quiet eras" from 0.19.0)

Why: since r10 a civilization has one `quiet_era` per quiet phase row, but every civilization writes its own line for the same row. In the 40 default 1024 worlds of generator 0.18.0 (seeds 0 to 39) every quiet row's eras share one start and one end year (325 rows, 1,640 era lines); with four civilizations per race a row holds up to 12 identical lines (`docs/reviews/p3-08-followups-world-critic.md` F3). From 0.19.0 the engine writes **one event per quiet phase row** for the whole world, kind 38 `quiet_stretch` (20.4).

In Q6 of each quiet step, after the subsystems:

1. If the current quiet phase row has no `quiet_stretch` yet and at least one civilization is not ended: append `quiet_stretch` with `year` = the step's start, `end_year` = the step's last year, `region` 0, `settlement` 0, no cause, values `years` = `end_year` − `year` + 1 and `realms` = **the number of civilizations not ended at this step**, and as participants (each as `realm`, in ascending id) those civilizations, **at most `MAX_PARTICIPANTS` (64, the event writer's bound)**: when more are alive, the 64 with the most people (the sum of their standing settlements' populations at this step), ties to the smaller id (r13). So the sentence's count stays right with any number of realms, and the participants are the largest realms. P3-07 22.2's generations note is written only for participants.
1a. **No line for a short stretch** (r15, P3-10 second round; the world critic's F3 of P3-10). Item 1 appends only when the stretch's **assured length** A ≥ `quiet_min_years`; otherwise the row is marked **decided without a stretch**, and no later step of the row appends one (item 1 applies only to a row not yet decided). With s the step's start and len its length, the step is the **row's last** when s + len = q_e (the interval ends) or when this step's tension, asked in Q5 before Q6, starts a flare-up by 5.5 (tension ≥ `flare_threshold` and f ≥ 1, f computed as 5.5 computes it); then A = len. Otherwise the next step of the row runs whole, and A = len + min(Q, q_e − (s + len)). Everything in it is known in Q6, so no event is removed or moved and W3's year order holds. **Exactness**: `engine_quiet_min` (`quiet_min_years` ≤ 2 × Q, 4.1) makes "A ≥ `quiet_min_years`" equal to "the stretch, at the row's close, has `years` ≥ `quiet_min_years`". If the step is the row's last, A is the stretch's length. If it is not, len = Q (a step shorter than Q ends its interval), and A < `quiet_min_years` ≤ 2Q gives min(Q, q_e − (s + len)) < Q, so the next step ends the interval and the stretch's length is A. A step that is not the row's first appends only when no civilization was alive at the row's earlier steps; A then measures the stretch from that step. Measured at 0.19.0 r16 (1024 defaults, 100 worlds): 141 of 873 stretches are shorter than 50 years, 82 of them exactly 25 years, and 68 of them are followed at once by a war declaration (`docs/reviews/p3-10-followups-world-critic.md` F3); none of them is written with `quiet_min_years` 50. The phase rows are unchanged.
2. Otherwise, if the row has its `quiet_stretch`: its `end_year` becomes the step's last year and `years` is recomputed. Its participants and `realms` never change: a civilization that ends within the row stays a participant (it was at peace when the stretch began); no civilization can be founded within a quiet row with the shipped subsystems (P3-05 founds civilizations in ticks only: awakenings, secessions), and one founded by a later subsystem would not be added.
3. No civilization alive: nothing (a world with no civilization has no quiet line).

The stretch closes at the end of its quiet phase row, like the eras before it. W9 does not apply (as for `quiet_era`); W2's rule for `quiet_era` (lengthened afterwards) applies to `quiet_stretch`. Kind 4 `quiet_era` stays in the catalogue with its structure (a shipped kind is never removed, 8.2) and is **not written** from 0.19.0, so `events_quiet_era` is 0 in every 0.19.0 world.

**What the era carried.** `generations` (the ruling generations of one civilization in its era) is not stored any more. It is derivable from the stored rows: for civilization c and a stretch [y, e], g(e + 1) − g(y) with g(t) = (t − c.`founded_year`) div `race.generation_years` (6.6's formula); P3-07 22.2 prints it in a civilization's own section. The region (a civilization's `home_region`) is not needed: the stretch belongs to the whole world.

P3-06 16.2 item 18a's exception (the eras open in the first claim's step may differ between the peoples-only and the conflict log) applies to the `quiet_stretch` open in that step, whose `end_year` and `years` may differ; its participants do not.

**The stubs** (7.3): the log of a stub world is, per age, `awakening`, `civ_founded`, `settlement_founded`, then one `quiet_stretch` per quiet phase row (participants: every civilization).

Template (data, `history.report`; salience 20 as `quiet_era`'s):

```
For {years} {years?year|years}, from {@year} to {@end_year}, the land was quiet{realms?|; one realm held it|, and # realms kept the peace}.
```

Example (illustrative, re-told from seed 42 at 1024, generator 0.17.0: the row 4393–4417 whose seven eras were e211–e217, and the elves' first row): `For 25 years, from 4393 to 4417, the land was quiet, and 7 realms kept the peace.`; the first row of a world with the elves alone: `For 1,924 years, from 150 to 2073, the land was quiet; one realm held it.`

Measured on the stored logs (one line per quiet row is the count of rows): default 1024 (0.18.0, seeds 0 to 39) 1,640 era lines become 325 lines (median 40 → 8 per world); at 0.17.0 the 100 default worlds of the P3-08 validation 3,670 → 732, `civs4` (20 worlds) 2,296 → 203, `turbulent` (20) 1,263 → 212, 512 (30) 942 → 244.

### 20.2 The cause list placeholder `{@causes:…}` (extends 8.4 and 8.5)

Why: an abandonment's causes are correct but cannot be said in its sentence (the review's F2: at summary level towns empty "for no stated reason"). The renderer gains one placeholder that names the kinds of the event's causes in words the template supplies.

Grammar (added to 8.4's `body`):

```
body     = … | "@causes:" entry { "," entry } "|" word "|" word
entry    = key "=" cword
key      = [a-z][a-z0-9_]*                     // an event kind key
cword    = one or more bytes other than "{", "}", "|", "?", ",", "="
```

Rendering (8.5): L = for each cause of the event in ascending cause id, the key of the cause event's kind (as stored in the world's `event_kind`); if the key is an entry's key, that entry's cword (the first entry with that key); a cause whose key has no entry contributes nothing; a cword already in L is not added again (first occurrence kept). If L is empty the placeholder renders nothing. Otherwise it renders the first word, then L joined as `{role:all}` joins names (`A`, `A and B`, `A, B and C`), then the second word. The cwords and both words are copied as they are (no name rule; they are literals). Example: causes a famine and then a plague, entries `famine=famine,plague=plague`, words `Weakened by ` and `, `: `Weakened by famine and plague, `.

`template_problem` (8.4): a body that starts with `@causes:` and does not match the grammar (no entry, an empty cword, a key not of the key syntax, the same key twice, fewer or more than two words) is `unknown placeholder '{<body>}' at byte <n>`, the existing message. The keys need not be kinds of the catalogue (a key that matches no kind is never used). `{@causes:…}` is allowed for every kind, caused or not.

A sentence start (8.5's names rule) is judged on the text rendered so far, so a name that follows the placeholder's second word `, ` is mid-sentence and one that follows an empty placeholder at offset 0 is at the sentence start: `Weakened by war, the Warrens of Zeez …` against `The Warrens of Zeez …`.

### 20.3 Thousands separators (amends 8.5's `{value}` and the `#` of the three-word form)

From 0.19.0 a value v with |v| ≥ 1000 renders with a comma (0x2C) between groups of three digits counted from the right, after the sign: `1,155`, `52,691`, `-12,000`; |v| ≤ 999 renders as before. This applies to `{value}` and to each `#` of a chosen three-word form; it does not apply to `{@year}` and `{@end_year}` (years stay `4393`), to the choice of `{value?w1|w2}` and `{value?w0|w1|w2}` (which depends on the value, not on its text), or to anything outside `render_event`. Design 4 writes "a city of 20,000". Stored values are integers and do not change: the rule changes no world byte, and it changes every stored golden sentence that holds a value of 1,000 or more (0.18.0, 40 default 1024 worlds: 10,030 of 27,971 events, 36%).

### 20.4 Catalogue (appended to 8.2)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values (seq: key) | written by |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 37 | `plague_outbreak` | 0 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `dead`; 2: `population`; 3: `total`; 4: `reached` | P3-05 (22.1) |
| 38 | `quiet_stretch` | 0 | 0 | 0 | 1: `realm`, civilization, 1–0 (unbounded) | 1: `years`; 2: `realms` | engine (20.1) |

Kind 37 replaces kind 12 `plague` from 0.19.0 (P3-05 22.1); kind 38 replaces kind 4 `quiet_era`. Kinds 4 and 12 keep their rows, roles, values, salience and template in every world's catalogue and are not written. `docs/specs/schema.md` 6 gains both rows (the engineer).

### 20.5 Schema and compatibility

No schema change (8.2's list): two kinds are appended, two kinds stop being written (their structure is unchanged), templates and saliences change (data), and the template grammar gains a form that was an unknown placeholder before (20.2), as r10's three-word form did. Every template valid before r12 renders as before except for the separators of 20.3, which are a rendering rule, not a stored value. A world of 0.19.0 whose templates use `{@causes:…}` is refused by `HistoryView::load` of 0.18.0 or earlier; no shipped program of those versions renders sentences (P3-07 is not implemented and the viewer does not render events), so, as for r10, no proposals entry for a schema change is needed. The flare-up sentence (kind 5) is unchanged: its parties are named by P3-07's note (P3-07 21.4 item 2, worded as amended in P3-07 22.3), the one place for them (`docs/specs/p3-10-followups.md` item 8).

### 20.6 Tests (amends 16)

00. (r15) `quiet_min_years`: with Q = 25 and 50, a row of one 25-year step followed by a flare-up has no `quiet_stretch`; a row of one full step and one of 10 years cut by an awakening (35 years) has none; a row of two full steps (50 years) has one, appended in its first step; with `quiet_min_years` 0 every row has one, as at r14; `quiet_min_years` 51 with Q = 25 fails `engine_quiet_min`. Invariant: every `quiet_stretch` has `years` ≥ `quiet_min_years`, and every quiet phase row with a civilization alive at its first step and at least `quiet_min_years` long has exactly one. The stub golden cases keep the lines of their long rows and lose those of their short ones (r16: `geo-seed-0-512`'s 16-year quiet row 5684–5699 loses its line); the `ForcedTension` cases lose the lines of their rows shorter than 50 years (for instance 2050–2073, 24 years).
0. (r13) A test world with 70 living civilizations at a quiet row: one `quiet_stretch` with `realms` 70 and 64 participants, the 64 most populous (ties by id), in ascending id.
1. `quiet_stretch`: on the stub golden cases and on the `ForcedTension` cases, one event per quiet phase row with a living civilization, participants every civilization not ended at the row's first step in ascending id, `realms` their number, `years` = `end_year` − `year` + 1 = the row's length; no `quiet_era` event; a civilization ended within the row (a hand-made subsystem in a test configuration that ends one in a quiet step) stays a participant.
2. `{@causes:…}` on fixture events: causes (famine, plague) → `Weakened by famine and plague, `; (plague, plague_spread) with both mapped to `plague` → `Weakened by plague, `; three distinct words → `A, B and C`; no mapped cause → empty, and the next name is at the sentence start; every malformed body of 20.2 → `unknown placeholder`.
3. Separators: values 0, 999, 1000, 1155, 25000, 1000000, −1000, −999 in `{value}` and in `#` render `0`, `999`, `1,000`, `1,155`, `25,000`, `1,000,000`, `-1,000`, `-999`; `{@year}` 4393 renders `4393`.
4. The P3-04 golden vectors (`p3-04-engine.txt`): the stub worlds' `e<n>` sentence lines change (quiet lines, separators); their `phase` rows' years and kinds, event years except the quiet lines and their kinds' ids, and their participants other than the quiet lines do not. (r14) The `phase` rows' `trigger_event` column does move: it holds event ids, and event ids renumber when one `quiet_stretch` replaces a row's `quiet_era` lines.

## 21. Small numbers as words; two conflict kinds (amendment r17, P3-11, generator 0.20.0)

Normative for the change P3-11 (`docs/specs/p3-11-followups.md`, items 2 and 5), from generator **0.20.0**, not before. **No schema change**: two kinds are appended (21.2) and a rendering rule changes (21.1); no role or value key of an existing kind, no code list and no column changes (8.2's list).

### 21.1 Numbers one to nine as words (amends 8.5's `{value}`, the `#` of the three-word form, and 20.3)

Why: sentences mix words and digits for the same small counts, as in "spread to one other settlement" (a three-word form) beside "spread to 2 other settlements", "4 realms kept the peace", "broke away from the Kingdom of Bocksul with 1 settlement" (the P3-10 world critic's F5, still open in round 2: "with 1 settlement" 40 times in 100 default worlds). The critic suggests words for 1 to 9 and digits from 10.

Rule: in `render_event`, a value v with **1 ≤ v ≤ 9** renders as the lower-case English word `one`, `two`, `three`, `four`, `five`, `six`, `seven`, `eight`, `nine`. This applies wherever 20.3's separators apply: to `{value}` and to each `#` of a chosen three-word form. Every other value renders as 20.3 says: 0, negative values and 10 to 999 as digits, and |v| ≥ 1,000 with separators. It does not apply to `{@year}` or `{@end_year}` (a year stays `5`), to the choice made by `{value?w1|w2}` and `{value?w0|w1|w2}` (which depends on the value, not on its text), to `?`-words, which are copied as they are (`one` in `{fled?| and one fled| and # fled}` is the template's own word), or to anything outside `render_event` (P3-07's own numbers, `[stats]`, refs and notes keep digits). The word is never capitalized. No shipped template puts a value at a sentence start, and a test checks that (21.3 item 2).

| Template (shipped) | 0.19.0 | 0.20.0 |
| --- | --- | --- |
| `civ_seceded` (17) | `The Berd League broke away from the Kingdom of Bocksul with 1 settlement.` | `… with one settlement.` |
| `plague_outbreak` (37) | `Plague broke out in X and spread to 2 other settlements; 1,084 died in all.` | `… spread to two other settlements; 1,084 died in all.` |
| `quiet_stretch` (38) | `…, the land was quiet, and 4 realms kept the peace.` | `…, and four realms kept the peace.` |
| `peace`, `war_won`, `truce`, `search_abandoned` | `… after 3 years of war.` | `… after three years of war.` |
| `battle` (22) | `…; 7 of the defeated and one of the victors fell.` | `…; seven of the defeated and one of the victors fell.` |
| `famine` (13), `settlement_captured` (26) | `Famine struck X; 8 died.` | `Famine struck X; eight died.` |

No template changes: the rule is in the renderer, so every template, old and new, reads it. Stored values do not change. The rule changes no world byte (sentences are not stored), only stored golden sentences holding a value from 1 to 9.

### 21.2 Catalogue (appended to 8.2 and 20.4)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values (seq: key) | written by |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 39 | `claim_settlements` | 1 | 1 | 0 | 1: `claimant`, civilization, 1–1; 2: `holder`, civilization, 1–1; 3: `settlement`, settlement, 1–0 (unbounded) | — | P3-06 (23.2) |
| 40 | `settlements_ceded` | 1 | 1 | 0 | 1: `from`, civilization, 1–1; 2: `to`, civilization, 1–1; 3: `settlement`, settlement, 1–0 (unbounded) | — | P3-06 (23.3) |

Kind 39 is written in place of kind 20 `claim` for a claim on a region the holder role does not hold (P3-06 23.2); kind 40 in place of kind 29 `region_ceded` for the foothold cession of P3-06 22.5 item 3.2 (P3-06 23.3). Kinds 20 and 29 keep their structure and are still written for every other claim and cession. Adding the settlements as a role of kinds 20 or 29 would be a schema change (8.2), so each case gets its own kind. An event has at most `MAX_PARTICIPANTS` (64, 20.1) participants in all, so the `settlement` role lists at most **62** settlements after the two civilizations, those with the smallest ids (r18; r17 said 64 settlements). The implementation lists 62; the test writer's reference lists up to 64, a known difference that no world reaches (`docs/specs/p3-11-followups.md` 7). No shipped world comes near it, since a region holds a few settlements (P3-05 6.3). `docs/specs/schema.md` 6 gains both rows (the engineer).

Templates (data, `history.report`; P3-06 23.2, 23.3):

```
claim_settlements   salience 35   {claimant} laid claim to {settlement:all} of {holder} in {@region}.
settlements_ceded   salience 55   {from} gave up {settlement:all} in {@region} to {to}.
```

### 21.3 Tests (amends 20.6)

1. Numbers: values 0, 1, 2, 9, 10, 11, 999, 1000 and −3 in `{value}` and in `#` render `0`, `one`, `two`, `nine`, `10`, `11`, `999`, `1,000`, `-3`; `{@year}` 5 renders `5`; `{n?one|#}` with n = 1 renders `one` and with n = 4 `#` (the two-word form's `#` is an ordinary byte, 8.4); `{n?|one|#}` with n = 4 renders `four`; the six rows of 21.1's table.
2. Every shipped template (`game/data/history/report.toml`): no `{value}` or three-word value body at byte 0 or directly after `. `.
3. Kinds 39 and 40: the catalogue unit test (role and value keys distinct, 8.2); `template_problem` accepts both templates, accepts `{settlement}` (min count 1: the first participant), and refuses `{@settlement}` in both (`settlement_use` 0); rendering with one, two and three `settlement` participants gives `A`, `A and B`, `A, B and C`, each name under the names rule of 8.5 (r10).
4. The P3-04 golden vectors (`p3-04-engine.txt`): stub sentence lines with a value from 1 to 9 change; nothing else of them.

## 22. One number form per clause; two kinds; no flare-up of a few years (amendment r19, P5-01, generator 0.21.0)

Normative for the change P5-01 (`docs/specs/p5-01-followups.md`, items 6, 7 and 9), from generator **0.21.0**, not before. **No schema change**: two kinds are appended (22.2), a rendering rule is refined (22.1) and one tuning field is added (22.3); no role or value key of an existing kind, no code list and no column changes (8.2's list).

### 22.1 One number form per clause (amends 21.1)

Why: 21.1 renders each value from one to nine as a word on its own, so a battle toll can mix the two forms: "24 of the defeated and seven of the victors fell" (the P3-11 world critic's F6: 230 of 9,066 battle sentences of the 100 default 1024 worlds at 0.20.0, every one with a side from 2 to 9 beside a side of 10 or more). The critic's rule: when either side of a toll is 10 or more, both are digits.

**Clauses of a template.** Scan the stored template as 8.4 parses it. The template's **literal text** is the bytes outside placeholders (a `?`-word's text is inside its placeholder and is not literal text); the escapes `{{` and `}}` (8.4) are literal text. The template is cut into **clauses** at every occurrence of the two bytes `; ` (0x3B 0x20) or `, ` (0x2C 0x20) in a run of literal text between placeholders (or before the first or after the last); a placeholder belongs to the clause in which it starts. A **number** of a clause is each value rendered by a `{value}` placeholder of the clause and each three-word form `{value?w0|w1|w2}` of the clause **whose chosen word holds `#`** (the two places 20.3 and 21.1 apply to); a three-word form whose chosen word has no `#` (`none`, ` and one fled`, an empty word) is no number of the clause. Years (`{@year}`, `{@end_year}`) are not numbers here and are never words.

**Rule** (replaces 21.1's "a value v with 1 ≤ v ≤ 9 renders as the lower-case English word"): in `render_event`, the numbers of a clause render as words (21.1's `one` … `nine`) **only when every number of the clause is from 1 to 9**; otherwise every number of the clause renders in digits with 20.3's separators. A clause with one number renders exactly as 21.1 says. Words that a template writes itself (`one` in `{fled?| and one fled| and # fled}`, `none`) are copied as they are (21.1). The rendering needs the clause's values before its text, so the renderer reads every value of the clause first (two passes over the template, or one pass with a look-ahead to the clause's end: the result is defined by the rule, not by the method).

| Template (shipped) | 0.20.0 | 0.21.0 |
| --- | --- | --- |
| `battle` (22), toll 24 and 7 | `…; 24 of the defeated and seven of the victors fell.` | `…; 24 of the defeated and 7 of the victors fell.` |
| `battle` (22), toll 8 and 3 | `…; eight of the defeated and three of the victors fell.` | unchanged |
| `battle` (22), toll 24 and 1 | `…; 24 of the defeated and one of the victors fell.` | unchanged (`one` is the template's word) |
| `sacked` (25), 8 died, 19 fled | `…; eight died and 19 fled.` | `…; 8 died and 19 fled.` |
| `plague_outbreak` (37), 2 reached, 1,084 died | `… and spread to two other settlements; 1,084 died in all.` | unchanged (`; ` is literal: two clauses) |
| `quiet_stretch` (38), 683 years, 4 realms | `For 683 years, from …, the land was quiet, and four realms kept the peace.` | unchanged (the literal `, ` after `{years?year\|years}` separates the clauses) |
| `settlement_abandoned_to` (31), 12 settled, 3 lost | `… 12 survivors went to X, and three were lost.` | `… 12 survivors went to X, and 3 were lost.` (`, and # were lost` is inside its `?`-word: one clause) |

With the shipped templates only `battle`, `battle_at_pass`, `sacked`, `settlement_captured` and `settlement_abandoned_to` have a clause with two numbers. The rule changes no world byte (sentences are not stored), only stored golden sentences.

### 22.2 Catalogue (appended to 8.2, 20.4 and 21.2)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values (seq: key) | written by |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 41 | `claim_dropped` | 1 | 1 | 0 | 1: `claimant`, civilization, 1–1; 2: `holder`, civilization, 1–1 | — | P3-06 (24.3) |
| 42 | `refuge_hidden` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `founder`, civilization, 1–1 | — | P3-06 (24.4) |

Kind 41 records a claim dropped at a declaration that found no allowed goal (P3-06 20.2 item 2), until now silent; its cause is the claim. Kind 42 records the years a fled refuge stays hidden (P3-06 20.1 item 2, 22.2): its `year` is the flight's and its `end_year` the last hidden year within the history. Both ids are provisional until the lead fixes the merge order (P5-00 4.4); code emits them by key. `docs/specs/schema.md` 6 gains both rows (the engineer).

Templates (data, `history.report`; P3-06 24.3, 24.4):

```
claim_dropped   salience 15   {claimant} let the claim against {holder} over {@region} lapse.
refuge_hidden   salience 10   {@settlement} lay hidden from {@year} to {@end_year}.
```

Both are below `SUMMARY_MIN_SALIENCE` (P3-07: never in the summary chronicle) and are read by P3-07 r19 25's `[present]` lists.

**The participant bound of kinds 39 and 40** (P3-11's known difference, 21.2): the test writer's reference lists at most **62** settlements, as the implementation does and 21.2 (r18) states. A test fix; no world reached either bound.

### 22.3 No flare-up of a few years (amends SC6, 5.5; E3)

Why: SC6 cuts a flare-up at the next awakening or the final phase (E3), so a flare-up can last one year: "In 5699 tensions flared into one year of upheaval." (default seed 88, e901; the P3-07 world critic's (b)). At 0.20.0, 11 of the 393 flare-ups of the 100 default 1024 worlds last less than 10 years (1 year 3, 2 years 3, then 4, 5, 5, 7, 9); `civs4` 4 of 243, `turbulent` 3 of 163, `calm` none.

**Field.** `history.engine` gains **`flare_min_years`** int(1, 1000), appended after `quiet_min_years`, shipped **10**, taste; a length at the reference span, **scaled** to the world's span as the lengths of the detail records are (5.1). The data field is required: a `history.engine` record without it is refused content. New named check after `engine_quiet_min`: **`engine_flare_min`**, condition: for each level in the order sparse, normal, dense, `flare_min_years` ≤ `flare_years`; message `<level>: flare_min_years must not be above flare_years`. The default of hand-made tuning (tuning built in code, `EngineTuning`) is 1; scaled, that is 1 for spans below 9000, which leaves SC6 as before, and 2 for spans of 9000 and more (`scale_years` rounds 1.5 up), which does not.

**Rule.** In SC6, `if f ≥ 1:` becomes `if f ≥ m:` with **m = max(1, scale_years(`flare_min_years`, E))**. f is the length after every cut of SC6, so a flare-up cut by the cap budget (E8, `cap_years − sched − flare_total`) to fewer than m years gives no flare-up either, as one cut at the next awakening or the final phase. When f < m, no flare-up starts at s: the quiet row goes on with its next step, as when the tension is below the threshold, and the tension is asked again after that step (unless it ends the interval, E3). Nothing is written for the skipped flare-up. The quarrel behind it is still open; when the interval ends at an awakening or the final phase, that active phase follows at once and P3-06's declarations run there as in any tick.

E3's row in 5.6 reads: "flare-up reaching the next awakening or S_f: cut there (SC6); none when the cut (or the cap budget, E8) leaves fewer than `flare_min_years` (scaled) or when the step ends the interval". Invariant 1 of 16 (schedule) gains: every kind 3 row is at least m years long. The guard "a flare length outside [1, q_e − s]" is unchanged.

### 22.4 Tests (amends 21.3)

1. Number clauses: the seven rows of 22.1's table; a clause with one number of each range (0, 1, 9, 10, 1,000, −3) as 21.1; a clause with 2 and 10 renders `2` and `10`; with 2 and 0 renders `2` and `0`; with 2 and 3 `two` and `three`; `; ` and `, ` inside a `?`-word do not cut; `,` without a space and `;` at the template's end do not cut; a template whose two numbers are separated only by literal ` and ` is one clause.
2. Kinds 41 and 42: the catalogue unit test (8.2); `template_problem` accepts both templates and refuses `{@settlement}` in kind 41 (`settlement_use` 0); rendering of each.
3. `flare_min_years`: with `ForcedTension` and Q = 25, an interval whose last step leaves 5 years before the final phase: with m = 10 no flare-up and the row continues to the interval's end; with m = 1 a 5-year flare-up as before; a tension crossing with 40 years left gives a flare-up of min(F, 40); `flare_min_years` 31 at `sparse` (`flare_years` 30) fails `engine_flare_min`; at E = 2000 the scaled value is used (10 → 3). Invariant: every kind 3 row is at least m years long.
4. The reference's participant bound for kinds 39 and 40: 62 (a hand-made claim on a region with 70 settlements of the holder lists the 62 smallest ids).
5. The P3-04 golden vectors (`p3-04-engine.txt`): the `ForcedTension` cases whose flare-ups are shorter than 10 years lose them (to be listed by the test writer); stub sentence lines change where a clause had two numbers; nothing else.

## Appendix A. Proposals entry (added to `docs/design/proposals.md` with this spec)

"History: active phases scale with the history span; the detail level sets the simulated share" (questions 1 to 3). Schema 7 itself is recorded in "History: schema versions 6 and 7"; its column details are this spec's 9.1.

## Revision note r1 (2026-11-19, amendments for P3-07)

Requested by P3-07 18.1, applied on `p3/07-spec`. 4.2: P3-07 adds no field. 8.5: `HistoryView::load` returns `db::Result<HistoryView>` (a corrupt world is an error value, code `corrupt_layer`, never a `FatalError`, P0-02 9); read accessors for the log, the stored catalogue and names; `load` validates every stored template and every reference of the log, so `render_event` fails only for an id outside the log; kinds the build does not compile are accepted. 11.1: the `[history]` keys are a subset of the history report's `[stats]`. No world byte changes.

## Revision note r2 (2026-11-19, amendments for P3-05)

Made with P3-05 (its 18.2 items 1, 5–7), in place. 3.3: `HistoryInputs` gains `biomes`, `habitability_layer`, `river_layer`, `flow_dir_layer` (both constructors, R1, R2). 6.2: Q4 is shared by P3-05 (foundings in free regions) and P3-06. 6.4: the shipped `EngineConfig` installs P3-05's `PeoplesAwakening` and `PeoplesSubsystem` (steps T1, T2, T4, Q1, Q3, Q4) from generator 0.15.0; the stubs S1–S3 of section 7 remain as test seams only. 9.2: `civilization.founding_event` may be a `civ_seceded` event. The catalogue of 8.2 is extended by P3-05 with kinds 6–19 (appended; no kind of this spec changes).

## Revision note r3 (2026-11-19, corrections after implementation)

The lead's rulings of 2026-11-19 on the questions the implementation (`p3/04-int`) raised; text brought in line with them, no new open question. 5.1 (and 5.4): floor_years = min(ceil(min_permille × E / 1000), cap_years), with the example of both permilles 100, `normal` {10, 1, 10}, span 2001, one race (floor = cap = 200); the tuning check stays min ≤ max. 4.1: the `engine_phase_budget` message is ASCII (`x`, `-`); 11.1's `<E - 1>` likewise; P3-07's messages checked and corrected (its r2), P3-05's are ASCII already. 4.2: `report_events` is a kind-level check of kind `history`, reported at `history.report`'s header in the P0-03 7.5 form. 6.4: `Step` gains `awakening` and `flare` (14, 15); a subsystem listing an engine-owned step (T5, T7, Q2, Q5, Q6, `awakening`, `flare`) is refused with `history: engine: a subsystem lists step <n>, which is not a subsystem step`; `quiet_events_left` returns `quiet_events_max` outside quiet steps. 6.6 W4 and 7.2: the writer accepts the next id about to be created for a civilization or settlement and checks before the next step that it was founded; the stub appends each event before creating its entity. 6.3: `flare_up` is a point event and takes an id, so awakening events are not consecutive when flare-ups occur (example: ages 0 and 520, phase 4's trigger is e3). 7.4: a forced period no step end meets forces nothing (dense, span 2000, one race: 216 simulated years). 8.4: a `{` inside a placeholder leaves it unclosed; a non-ASCII byte is reported at its own offset, also inside a placeholder; the race-role form check precedes the `min_count` check. 6.1: `found_settlement` checks the cell's region in the `region` layer. 9.4: the writer derives `history_end_year` from `param.history_span`; the reader checks its form in place and its equality with the span after every other key. 3.3, 6.1, 6.4: `HistoryResult` is `history::HistoryResult` with an alias in `stages`; `PhaseKind` and `TensionReport` are declared in `history/schedule.hpp`. New 16.6: the normative list of fatal messages, as implemented; 3.4, 5.5, 6.5 and 6.6 point to it. No world byte or golden value changes.

## Revision note r4 (2026-11-19, amendments for P3-06)

Made with P3-06 (its 18.5 item 2), in place. 6.4: **from generator 0.16.0** the shipped `EngineConfig` installs `ConflictSubsystem` after `PeoplesSubsystem`, with steps T2, T6, Q4 (all subsystem steps, so the r3 check accepts them), and as the only tension source (6.5), so flare-ups occur in shipped worlds from 0.16.0 on. 8.2: P3-06 appends kinds 20–30 (P3-06 11) from 0.16.0. No engine rule, message or golden value of this spec changes.

## Revision note r5 (2026-11-19, rules from the determinism audit)

Requested by the lead after the P3-04 determinism audit (advisories A1, A2, notes N1–N5). New 6.9: the normative rules for subsystems (no live iteration across a creation or relation change; `tension()` a pure read; fixed subsystem order and the refusal of a step listed twice, whose message the engineer adds to 16.6; visibility in shared steps; draw keys from ids; symmetric relations normalized by the subsystem; net change per bookkeeping). No engine rule, world byte or golden value changes; the duplicate-step refusal is a configuration guard that shipped configurations never trip.

## Revision note r6 (2026-11-19, hardening after the audit)

The engineer's follow-up to the determinism audit, as implemented on `p3/04-int` at fa87955; text only, no world byte changes. 3.4 item 6 and 16.6: the region-layer precondition `history: engine: layer 'region' is not <N> x <N> u16`, checked after the chronology rows (A4); 6.1: `found_settlement`'s cell-region check is therefore unconditional. 6.9 and 16.6: `history: engine: a subsystem lists step <n> twice` (`<n>` the step number); two different subsystems may list one step. 11.1: each phase row's `end_year` is checked against `history_end_year` − 1 before any arithmetic (A3): `row <id>: end_year <e> above <E - 1>` on a row other than the last, the existing last-phase message on the last.

## Revision note r7 (2026-11-19, after validation)

From the P3-04 validation; text only. 14.2: the stub world's typical event count is 33 or 39 at `normal` (about 30% of worlds have six quiet rows and 39 events). 16.2 item 15: the scheduler property test runs seeds 0-99 by default and 0-999 in the `history-slow` suite. 14.3: the engine-time microbenchmark is a tagged doctest case skipped by default, still to be written.

## Revision note r8 (2026-11-19, for the P3-06 implementation)

6.4: `StepContext` gains two read-only accessors, `event_kind(id)` and `event_participant(id, role)`, fatal `history: state: event <e> does not exist` for an unknown id (P3-06 8.1 item 2 reads a `civ_seceded` event's `from` with them). 6.7 item 2: a relation opened and closed within one step still gets its `relation` row (`since` = `until`, both events), from opens and closes recorded per step in `State`; P3-06 reaches it on three paths (P3-06 8.3 item 7). No existing world changes: no P3-04 or P3-05 world opens and closes a relation in one step.

## Revision note r9 (2026-11-19, relation bookkeeping as implemented)

6.7 item 2 states the relation bookkeeping of `p3/06-conflict` (37d27ab) exactly: opens and closes recorded in call order and walked at each bookkeeping; opened and closed gives `since` = `until`; closed and reopened gives two rows (the earlier net rule kept one; no code does this today); the same key opened twice before one bookkeeping is fatal; the open rows must equal the state's open relations. 6.9 item 7 rewritten accordingly (regions keep the net rule). 16.1 item 6 gains the worked row (3, 1, 3, 2, peace, 7, 7, e13, e14); 16.6 gains two messages. No existing world changes.

## Revision note r10 (2026-11-20, for P3-08; generator 0.17.0)

Normative for the P3-08 change (`docs/specs/p3-08-followups.md`), not before it. (1) 8.5: `render_event` lower-cases a leading `The ` of a name that is not at a sentence start (P1-06 r3 14.7 and Appendix B.1, applied here); no schema or format change; every golden holding rendered sentences is re-recorded (with the region names of P1-06 r3). (2) 8.4, 8.5: the three-word value form `{value?w0|w1|w2}` with `#` for the value; a body that was an unknown placeholder before, so every earlier template keeps its meaning; not a schema change (no table, column, code list or kind structure changes; 8.2), but a world whose templates use the form is refused by a `HistoryView::load` of 0.16.0 or earlier (no shipped program renders sentences: P3-07 is not implemented and the viewer does not render events). (3) 6.6: a civilization's quiet era is not closed by its own events: one era per civilization and quiet phase row. (4) 4.2: the `quiet_era` template uses the three-word form for `generations` (no more "(0 generations)"). (5) 14.3: the engine microbenchmark specified (a skipped doctest suite). (6) 8.5's seed 42 example: P1-06 r3's name for region 12, the new template. Tests 16.1 items 7 and 10 amended. Stub worlds (16.4's world cases) have no charged quiet events, so (3) changes none of their rows; (1), (2) and (4) change their sentence lines.

## Revision note r11 (2026-11-20, corrections after the P3-08 implementation)

Text only. 8.5's seed 42 example and 17's names: civilization 1 is `The House of Finen` from 0.17.0 (P3-02 r5's elven realm form), and 17's stub homes carry P1-06 r3's region names; 8.5's illustrative goblin name `Bonk-Gitch` is replaced by `Gnonk` (P3-02 r5's filter rejects `Bonk-Gitch`). 14.3: "simulated years" in the microbenchmark are the years of the opening, flare-up and final phase rows (phase kinds 1, 3 and 4), as 4.1 counts the simulated years. 16.0a (P3-05's fatal message for a layer, now also used for the `i16` layer `elevation`): see P3-05 16.0a r7.

## Revision note r12 (2026-11-20, for P3-10; generator 0.19.0)

New section 20, normative from generator 0.19.0 (`docs/specs/p3-10-followups.md`): one `quiet_stretch` event per quiet phase row for the whole world (kind 38) in place of one `quiet_era` per civilization (20.1, replacing 6.6's eras); the cause list placeholder `{@causes:…}` (20.2, extending 8.4 and 8.5); thousands separators in rendered values (20.3); kinds 37 and 38 appended, kinds 4 and 12 kept and no longer written (20.4); no schema change (20.5); tests (20.6). No earlier section is renumbered; sections 6.6, 8.4 and 8.5 describe generators 0.14.0 to 0.18.0 where 20 amends them.

## Revision note r13 (2026-11-21, after the P3-10 implementation)

20.1: `quiet_stretch` participants are at most `MAX_PARTICIPANTS` (64), the most populous realms; `realms` counts every living civilization; test 20.6 item 0. 4.2: `report_event.template` is string(512) from 0.19.0 (the cause-list templates of P3-05 22.3 exceed 256 bytes), as implemented.

## Revision note r14 (2026-11-21, text after the P3-10 integration)

20.6 item 4: the `phase` rows keep their years and kinds, but their `trigger_event` column moves with the event ids, which renumber when the quiet lines collapse. No rule or output change.

## Revision note r15 (2026-11-21, P3-10 second round; generator stays 0.19.0)

20.1 item 1a: a quiet stretch whose assured length is below the new `history.engine` field `quiet_min_years` (shipped 50, not scaled; 4.1, appended after `quiet_events_max`) gets no `quiet_stretch` event; the decision is made in Q6 of the step that would append it, from that step's length, whether Q5 ends the row, and the next step's length, so the year order of W3 holds and no event is removed. New check `engine_quiet_min` (`quiet_min_years` ≤ 2 × `quiet_step_years`) makes the decision exact. Test 20.6 item 00. The stub goldens' `ForcedTension` cases change.

## Revision note r16 (2026-11-21, text after the P3-10 second-round integration)

20.6 item 00: the plain stub cases do not keep every quiet line; a quiet row shorter than `quiet_min_years` loses its line there too, as in `geo-seed-0-512` (5684 to 5699, 16 years). No rule or output change.

## Revision note r17 (2026-11-21, P3-11; generator 0.20.0)

New section 21 (`docs/specs/p3-11-followups.md` items 2 and 5): values from one to nine render as words in sentences (21.1, a rendering rule beside 20.3's separators; no template changes); kinds 39 `claim_settlements` and 40 `settlements_ceded` appended, with their templates, for P3-06 23.2 and 23.3 (21.2); tests (21.3). No schema change; no earlier section is renumbered.

## Revision note r18 (2026-11-22, text after the P3-11 validation)

21.2: kinds 39 and 40 list at most 62 settlements, since an event has at most 64 participants and two are the civilizations. No rule change: the implementation does this.

## Revision note r19 (2026-11-22, P5-01; generator 0.21.0)

New section 22 (`docs/specs/p5-01-followups.md` items 6, 7 and 9): the numbers of one clause of a template render all as words or all as digits (22.1, refining 21.1); kinds 41 `claim_dropped` and 42 `refuge_hidden` appended, with their templates, for P3-06 r22 24.3 and 24.4, and the reference's participant bound of kinds 39 and 40 set to 62 (22.2); the `history.engine` field `flare_min_years` (10, scaled) and the check `engine_flare_min`: no flare-up shorter than it (22.3, amending SC6 and E3); tests (22.4). No schema change; no earlier section is renumbered.

## Revision note r20 (2026-11-22, text after the P5-01 integration)

Text only; the implementation and tests on `p5/01-int` (generator 0.21.0) already follow every point. 22.1: `{{` and `}}` are literal text; a clause is cut at `; ` or `, ` in the literal text between placeholders; a three-word form counts as a number only when its chosen word holds `#`. 22.3: m = max(1, scale_years(`flare_min_years`, E)); a flare-up cut by the cap budget to fewer than m years is also skipped; the hand-made default 1 (`EngineTuning`, tuning built in code) leaves SC6 as before only for spans below 9000 (scaled, it is 2 from 9000); the data field is required, so a record without it is refused content.
