# P3-07 History report

Status: **approved by the lead 2026-11-19** (r11, 2026-11-20: section 22 for P3-10, generator 0.19.0) (the six questions of section 19 go to Paul; the interim choices stand). Written by the spec writer 2026-11-19; the amendments of 18.1–18.5 applied the same day on `p3/07-spec`. Phase P3 History core, spec 7 (P3-00 3.1): the text history report, `worldgen-cli report --db PATH --history`. It reads a world file only and changes no world byte, so it needs **no generator version bump and no schema change**. Design doc sections 5 (history: the log is the primary product), 6 (the database stores facts and names; text is rendered from records and templates), 13 (P3 delivers "a text history report"; milestone 1: "the event log reads as coherent history") and 14 (the enrichment pass narrates from the log: this report is its deterministic, template-only counterpart and is not a chronicle in the sense of 14; it is a tool for the world critic, the validation sweeps and Paul). Depends on P0-02 (reader, errors as values), P0-04 (`report`: options, `--out`, `--force`, exit codes, text conventions, the "Later" note), P3-00 (binding: 11, 6.2–6.4, 12.2, 12.3, 13), P3-03 (tables `race`, `god`, `age`; `[chronology]` and its checks, 9.1), P3-04 (binding: schema 7 tables and codes of 9; the catalogue, `render_event` and `HistoryView` of 8, used and not redefined; the world report's `[history]` and its checks, 11.1; the queries of 10). Read on `origin/p3/04-spec` at 4a00615 (the newest text of every spec; the code there is P3-03's). Proposals read: up to "History: active phases scale with the history span; the detail level sets the simulated share". Revision r1 (P3-05's appended `[stats]` keys) 2026-11-19.

The sample report (section 16) and the illustrative excerpt (17) were produced by the spec writer's exact-integer prototype `~/.cache/the-unmapped/out/p3-07-spec/history_report.py`, run on worlds built by `build_world.py` (the P3-04 prototype's stub world for seed 42, 1024, written as SQLite tables) and `illus.py` (a hand-made world). The prototype is scratch; the test writer's reference is written from this spec.

## 1. Purpose

The event log is the primary product of history (design 5), and it is stored as facts: kinds, years, places, participants in roles, values and causes (P3-00 6.2). Nobody can judge a history from those rows. The history report turns one world file into a deterministic text that reads as a history: the frame (settings, ages, phases), one block per civilization telling its arc (founding, home, peak, relations, fate, its most salient events), the chronicle grouped by age and phase with every event rendered by `render_event` from the world's own templates, the end state, and a `[stats]` block of integers for the validation sweeps (P3-00 12.2). At level `summary` a 6,000-year history of tens of thousands of events reads in a few hundred lines; at level `full` every event is printed. Every line carries ids and years, so the world critic can cite seeds and event ids and follow causes (design 13), and the text is part of the cross-platform contract like `report` and `hash` (P0-04 10).

## 2. What this spec touches (for the engineer)

| Area | Files |
| --- | --- |
| History digest | new `worldgen/include/worldgen/history/digest.hpp`, `worldgen/src/history/digest.cpp`: the tables the report computes from, their checks (5.3) and the computations of section 7 (`HistoryTables`, `HistoryDigest`), for the report and later for P4's browser |
| Report | new `worldgen/include/worldgen/inspect/history_report.hpp`, `worldgen/src/inspect/history_report.cpp`: `build_history_report`, the text of section 6 |
| `HistoryView` | `worldgen/include/worldgen/history/event_catalogue.hpp` (P3-04): errors as values and read accessors (amendment 18.1) |
| Database errors | `worldgen/include/worldgen/db/result.hpp`, `result.cpp`: `ErrorCode::history_missing` (18.2) |
| CLI | `tools/worldgen-cli/cli.cpp`: options `--history`, `--level` of `report`, usage text, exit codes (section 4) |
| Data | none (section 8: no field is added to `history.report`) |
| Version | none: no world byte changes (3.3) |
| CI | `tools/ci/world-hashes.sh`: save and compare the history report of every case (18.6) |
| Tests | `worldgen/tests/history_report_*.cpp` (new CTest group `worldgen-tests-history-report`, pattern `*history_report_*`), `tools/worldgen-cli/tests/cli_history_report_test.cpp`, fixtures `worldgen/tests/data/history-report/`; reference `worldgen/tests/reference/p3_report_reference.py`; golden `worldgen/tests/golden/p3-07-report.txt` |

No stream, no stage, no tuning field, no table, no `meta` key.

## 3. Inputs

### 3.1 Tables read

Only the world file, through the P0-02 reader (`WorldReader::table_rows`, rows in primary-key order) and P3-04's `HistoryView`. Nothing is read from a data root: the report does not load `game/data` (3.2).

| Table | Columns used | For |
| --- | --- | --- |
| `meta` | `seed`, `seed_phrase`, `param.map_size`, the eight `param.*` keys of P3-00 5, `generator_version`, `history_end_year` (E), `stages` | `[settings]`; E; the history check (5.2) |
| `region` | `id`, `name`, `biome`, `cells`, `lake_cells`, `walk_cells` | names; home biome and upland flag; islets for the share statistics (7.5) |
| `biome` | `id`, `key` | the home region's biome key |
| `race`, `god`, `age` | all (P3-03 5.2) | `[ages]`, names, camps |
| `event_kind`, `event_kind_role`, `event_kind_value`, `event`, `event_participant`, `event_value`, `event_cause` | through `HistoryView` (P3-04 8.5) | rendering, participants, values, causes, salience |
| `phase` | all | `[phases]`, the chronicle's grouping |
| `civilization`, `settlement` | all | `[civilizations]`, `[present]` |
| `settlement_census` | all | peak population (7.2) |
| `region_control` | all | peak regions, regions held, shares (7.1, 7.5) |
| `relation` | all | relations, wars |

Tables not read: `layer`, `pass`, `climate`, `region_border`, `region_biome`, `race_biome`, `culture` (the culture's display name is not needed; the civilization's race is).

### 3.2 The world is self-describing; the report reads no data

Every sentence comes from the templates stored in `event_kind` (A1, P3-00 6.3), every salience from `event.salience`. The report's own choices (the summary budget, the milestone count) are **constants of the format** (section 8), not tuning in `history.report`, so the same world file gives the same report whatever data set is installed, on every machine. This is the reason the report does not add the level thresholds to `history.report` as P3-01 5 anticipated: a threshold in data would make the report depend on the installed data, and storing it in the world would need a schema change for a knob that the per-kind base salience, which is data and is stored in every world, already provides (question 1).

### 3.3 No world byte changes

P3-07 adds library code, a CLI option and tests. No world file, table, layer, `meta` key or data file changes, so `GENERATOR_VERSION` is not bumped and no existing golden value is re-recorded (P3-00 3.2: "P3-07 changes no world byte ... so it bumps only if it changes their data"; it changes none). It adds one golden file (14.4).

## 4. Command line

```
worldgen-cli report --db PATH [--history [--level summary|full]] [--out PATH] [--force] [--verbose]
```

| Option | Default | Meaning |
| --- | --- | --- |
| `--history` | off | print the history report (section 6) instead of the world report |
| `--level summary\|full` | `summary` | `summary`: the chronicle prints the events chosen by salience (7.4) and `[present]` prints totals; `full`: every event, and one line per region and per settlement in `[present]` |
| `--db`, `--out`, `--force`, `--verbose` | as P0-04 7 | unchanged: stdout by default; `--out` writes the file atomically (P0-04 4.4) and prints nothing on stdout; `--force` replaces an existing `--out` file and is accepted without `--out` (no effect) |

**Usage errors** (exit 2, P0-04 4.1 form, with the hint line `worldgen-cli: run 'worldgen-cli report --help' for usage`): `--level needs --history` (a `--level` without `--history`; checked after tokenizing, before any file is opened); `invalid --level '<text>'; expected summary or full`. `--history` is a flag (no value). `report --help` lists both options:

```
usage: worldgen-cli report --db PATH [--history [--level summary|full]] [--out PATH] [--force] [--verbose]
  --history           print the history report (format '# worldgen history v1') instead of the world report
  --level LEVEL       with --history: summary (default) or full
```

(the existing lines for `--db`, `--out`, `--force`, `--verbose` stay as they are; the engineer keeps the existing column layout).

**No filters in v1** (decision; question 3). A `--civ ID`, `--region ID` or year-range option was considered for the critic and Paul. Every chronicle line carries the ids of its place and participants (`=c3`, `@rg12`) and starts with its year, so `report --history --level full | grep -E '=c3[ }]'` follows one civilization through the whole log and `awk` cuts a year range, with no second output format to specify, golden and keep stable. Following a civilization interactively is P4's browser (design 13: "Paul can follow one civilization from awakening to the present without leaving the viewer").

**Worlds without history** (exit 3, input error, nothing on stdout, no `--out` file). A world has history exactly when `meta.stages` names `history`. When it does not (`--until regions` and earlier, `--until chronology`, `--until none`, `--test-pattern`), and tables `phase` and `event` are empty, the report fails with

```
worldgen-cli: error: world file '<path>' has no history (stages: <stages>); generate it with the history stage
```

`<stages>` is `meta.stages` escaped as in 6.1 (`elevation,hydrology,climate,biomes,resources,regions,chronology`), or `none` when it is empty; `<path>` as the reader prints it in its other messages. The inconsistent cases are corrupt (5.2). Interim: refuse rather than print a partial report with empty blocks, because a sweep script that reads `[stats]` from a geography-only world by mistake should fail loudly (question 4).

**Other schema versions**: the reader refuses them before anything else (`world schema version <v>, this build reads 7`, exit 3, P0-02; P0-02's rule "readers accept exactly the current version"). The report never reads an older or newer schema; there are no migrations in the prototype.

**Exit codes** (P0-04 4.6): 0 success; 2 usage; 3 input (unreadable or not a world, schema, a missing required `meta` key, no history, corrupt tables); 4 output (`--out` exists without `--force`, missing parent, write failure, failed stdout write: P0-04 amendment 14 (a)); 1 internal error (a bug, or out of memory).

## 5. Library split, API and errors

### 5.1 Public API (P3-00 A1: the viewer and the game call the same code)

```cpp
// worldgen/include/worldgen/history/digest.hpp
namespace worldgen::history {

// The tables the report computes from, read once and checked (5.3). Rows in primary-key order,
// as db::...Row types of P3-03 and P3-04 where they exist (RaceRow, GodRow, AgeRow, PhaseRow,
// CivilizationRow, SettlementRow, SettlementCensusRow, RegionControlRow, RelationRow); region rows
// reduced to the columns of 3.1 and biome keys by id.
struct HistoryTables { /* E, the meta values of [settings], regions, biome keys, races, gods, ages, phases,
                          civilizations, settlements, census, region_control, relations */ };
[[nodiscard]] db::Result<HistoryTables> read_history_tables(const db::WorldReader&);

struct RegionPeak { std::int64_t count = 0, from_year = 0, to_year = 0; };   // 7.1; to_year inclusive; count 0: none
struct PopulationPeak { std::int64_t population = 0, year = 0; };            // 7.2; population 0: none
struct CivilizationDigest {
    std::int64_t id = 0;
    RegionPeak peak_regions; PopulationPeak peak_population;
    std::int64_t settlements_founded = 0, settlements_held = 0, settlements_ruined = 0;
    std::int64_t regions_held = 0, population = 0, events = 0;               // at E; events with the civilization as participant
    std::vector<std::int64_t> milestones;                                     // 7.3, ascending event id
    std::vector<std::size_t> relations;                                       // indexes into HistoryTables relations, in 6.6 order
};
struct HistoryDigest {
    std::vector<CivilizationDigest> civilizations;                           // id order
    std::int64_t summary_threshold = 0;                                       // 7.4
    std::vector<std::pair<std::string, std::int64_t>> stats;                  // section 9, in order
};
[[nodiscard]] HistoryDigest digest_history(const HistoryView&, const HistoryTables&);
}

// worldgen/include/worldgen/inspect/history_report.hpp
namespace worldgen::inspect {
inline constexpr std::string_view HISTORY_REPORT_HEADER_V1 = "# worldgen history v1";
enum class HistoryLevel : std::uint8_t { summary = 1, full = 2 };
[[nodiscard]] std::optional<HistoryLevel> parse_history_level(std::string_view);   // "summary", "full"
[[nodiscard]] std::string escape_history_text(std::string_view);                    // 6.1
// The whole history report of an open world (section 6), or the first error of 5.2.
[[nodiscard]] db::Result<std::string> build_history_report(const db::WorldReader&, HistoryLevel);
}
```

The structs list the fields the report needs; the engineer may add members. `digest_history` is pure (no I/O) and does not fail: everything it relies on was checked by `read_history_tables` and `HistoryView::load`. The CLI only parses the options, opens the world (P0-04's `open_world`), calls `build_history_report` and writes the text: it formats nothing itself (P0-04 amendment 12).

### 5.2 Order of checks in `build_history_report` (first failure reported)

1. **History present.** Let `h` = `meta.stages` names `history` (the comma-separated list contains the item `history`). If not `h` and `phase` and `event` are empty: `db::ErrorCode::history_missing` with the message of section 4 (without the `worldgen-cli: error: ` prefix). If not `h` and `phase` or `event` has a row: `corrupt table '<t>': <n> rows, but meta.stages does not name history` (t the first of `phase`, `event` with rows). If `h` and `phase` is empty: `corrupt table 'phase': no rows, but meta.stages names history`.
2. **The world report's checks**, through the same library functions the world report uses, with their messages unchanged: P3-03 9.1 (tables `race`, `age`), then P3-04 11.1 (tables `phase`, `event`, `event_cause`; from P3-04 r6 the phase rows' `end_year` is bounded by `history_end_year` − 1 before any arithmetic, `corrupt table 'phase': row <id>: end_year <e> above <E - 1>`, so the `[stats]` sums over phases, which share 11.1's definitions, cannot overflow). Not restated here.
3. **This spec's checks** (5.3), in `read_history_tables`.
4. **`HistoryView::load`** (P3-04 8.5 with amendment 18.1): participants against the catalogue's roles and entity types, values, referenced ids of the log, templates. Its messages are passed through unchanged. Not restated here.
5. Compute (`digest_history`) and format (section 6): cannot fail except by running out of memory (`ErrorCode::out_of_memory`, exit 1, as the world report).

Every corrupt-data error is `db::ErrorCode::corrupt_layer` (the code the world report already uses for corrupt tables: P1-06, P3-03, P3-04), message `corrupt table '<table>': <detail>`; exit 3 in the CLI. `HistoryView::load` must return its error as a value, never as a `FatalError` (amendment 18.1): a corrupt file is an input error, not a bug, and the viewer binds the reader without exception translation (P0-02 9).

### 5.3 This spec's checks (`read_history_tables`)

Run after items 1 and 2, so `race`, `age`, `phase`, `event` and `event_cause` are known to be well formed as far as P3-03 9.1 and P3-04 11.1 go. E = `history_end_year`. Each table in this order, rows in primary-key order, the first failure reported. `<key>` is the row's id for a table with an `id` key, else the key in parentheses, comma-separated: `(<settlement>, <year>)`, `(<region>, <from_year>)`, `(<a_type>, <a_id>, <b_type>, <b_id>, <kind>, <since>)` (the kind escaped as in 6.1). Row shapes first, as P3-03 9.1 (`row <key>: has <k> columns, expected <n>`, `row <key>: column <c> is not an integer`, `row <key>: column <c> is not text`, `<key>` `?` when it cannot be read; NULL only in the nullable columns of P3-04 9.1). "is not a <x>" means the id is not a row of that table. Messages are `corrupt table '<t>': row <key>: <reason>`:

| Table | Reasons, in this order per row |
| --- | --- |
| `region` | `cells below 1`; `lake_cells outside 0 to cells`; `walk_cells outside 0 to cells`; `biome <b> is not a biome` |
| `phase` | `age <a>, expected <x>` (x = the id of the age whose years contain `start_year`); `end_year <e> beyond the end of age <x>, <z>` |
| `event` | `phase <p>, expected <x>` (x = the id of the phase whose years contain `year`) |
| `civilization` | `id not consecutive` (first id not 1, or not the previous + 1); `race <r> is not a present race`; `home_region <h> is not a region`; `founded_year <y> outside 0 to <E - 1>`; `founding_event <e> is not an event`; `capital <s> is not a settlement` (when ≠ 0); `ended_year and ending_event disagree` (NULL exactly when 0); `ended_year <y> outside <founded_year> to <E - 1>`; `ending_event <e> is not an event` |
| `settlement` | `id not consecutive`; `region <r> is not a region`; `founder_civ <c> is not a civilization`; `founded_year <y> outside 0 to <E - 1>`; `founding_event <e> is not an event`; `owner_civ <c> is not a civilization` (when ≠ 0); `kind <k>` (not 1–5); `status <s>` (not 1–4); `population <p> is negative`; `ruined_year and ruin_event disagree` (NULL exactly when 0); `ruined_year <y> outside <founded_year> to <E - 1>`; `ruin_event <e> is not an event` |
| `settlement_census` | `settlement <s> is not a settlement`; `first row in <y>, expected <f>` (the settlement's first row is not at its `founded_year`); `year <y> beyond <E - 1>`; `owner_civ <c> is not a civilization` (when ≠ 0); `kind <k>`; `status <s>`; `population <p> is negative`; then, after a settlement's last row: `corrupt table 'settlement_census': settlement <s>: the last row differs from the settlement's end state` (owner, kind, status or population ≠ the `settlement` row: P3-04 invariant 5) and `corrupt table 'settlement_census': settlement <s>: no rows` (a settlement with no census row) |
| `region_control` | `region <r> is not a region`; `civ <c> is not a civilization`; `from_year <y> outside 0 to <E - 1>`; `to_year <t> outside <from_year + 1> to <E - 1>` (when not NULL); `overlaps the previous interval` (the region's previous row has `to_year` NULL or above this `from_year`); `event <e> is not an event` |
| `relation` | `a_type <t>`, `b_type <t>` (not 1–9); `a <prefix><id> does not exist`, `b <prefix><id> does not exist` (types 1–5 only; prefixes of 6.1); `since <y> outside 0 to <E - 1>`; `until and until_event disagree` (NULL exactly when 0); `until <u> outside <since + 1> to <E - 1>`; `since_event <e> is not an event`; `until_event <e> is not an event` (when ≠ 0) |

These are the conditions without which a computation of section 7 would be undefined or two blocks of the report would contradict each other; they are not a full validation of the log (P3-04's invariants are tested on the generator). An "is not an event" check is `1 ≤ id ≤` the number of `event` rows (P3-04 11.1 has made the ids consecutive).

## 6. Format `# worldgen history v1`

### 6.1 Conventions

- **Bytes.** Every byte of the report is LF (0x0A) or in 0x20–0x7E: ASCII only, a subset of P3-00 11's UTF-8. The report ends with LF. No CR, no tab, no empty line, no line starts with a space except the indentation stated below (two or four spaces), no line ends with a space (rule below). No line wrapping and no length limit: a line is as long as its content (the longest line of the sample is 161 bytes).
- **Text.** Every piece of text taken from the world (names, rendered sentences, `meta` values, kind keys, role and value keys, relation kinds, biome keys, age names) is printed through `escape_history_text`: byte 0x5C (`\`) becomes `\\`; every byte below 0x20, 0x7F and every byte from 0x80 becomes `\xHH` with upper-case hex; every other byte is copied. With valid data nothing but a backslash is ever escaped (names are ASCII letters, space, apostrophe and hyphen: Paul's decision 13; templates are ASCII: P3-04 8.4), so a sentence in the report is byte for byte the sentence `render_event` returns, which the viewer and the game print.
- **No trailing space.** If a line would end with one or more spaces (a template ending in a space, or a rendered sentence that is empty: see "Sentences"), each of those trailing spaces is written as `\x20`.
- **Numbers.** Decimal, `-` before a negative value (only event values can be negative), no `+`, no leading zeros, no grouping, no padding, `0` for zero; formatted without a locale (`std::to_chars`). There are no fractions: a ratio is an integer per mille, `floor(1000 × n / d)` on non-negative `int64` (9).
- **Plurals.** `pl(n, w)` is `n`, a space and `w` when n = 1, else `w` followed by `s`. Words: `year`, `event`, `civilization`, `region`, `settlement`, `more event` (so `1 more event`, `2 more events`).
- **Spans.** An event's span is `<year>` when `end_year` = `year`, else `<year>-<end_year>` (years are never negative, so the hyphen is unambiguous). Ages, phases, peak runs and closed relations always print both ends, `<start>-<end>`, inclusive, also when they are equal.
- **Ids.** Every entity is printed with its id and a prefix fixed here for every type: event `e`, age `a`, phase `p`, and by entity type code (P3-04 9.2): 1 race `ra`, 2 god `g`, 3 civilization `c`, 4 settlement `s`, 5 region `rg`, and reserved for P5 and P6: 6 figure `f`, 7 faction `fa`, 8 monster `m`, 9 site `si`. A reference to a named entity is `<name> (<prefix><id>)`, for example `Voeledell (s1)`; the name of a race is its `race.name` (`Elf`), of a god, civilization, settlement, region its `name` column. An entity of types 6–9 (none exists in P3) is printed `<prefix><id>` without a name.
- **Words.** Phase kinds (P3-04 9.2): 1 `opening`, 2 `quiet`, 3 `flare_up`, 4 `final`. Camps: 0 `good`, 1 `neutral`, 2 `evil`. Settlement kinds: 1 `hamlet`, 2 `village`, 3 `town`, 4 `city`, 5 `fortress`. Statuses: 1 `thriving`, 2 `declining`, 3 `ruined`, 4 `faded`. "Standing" means status 1 or 2.
- **Sentences.** `<sentence of e>` is `render_event(view, e)` (P3-04 8.5) escaped. A head followed by a sentence is written `<head>: <sentence>`; when the sentence is empty, the line ends with `:\x20`. A sentence is `render_event`'s text unchanged; its rule for `The …` names inside a sentence (P3-04 8.5, r10: a leading `The ` of a name not at a sentence start is written `the `) is not applied again and not undone. Every name the report prints outside a sentence (headings, `<name> (<prefix><id>)` references, `{<refs>}` lists, field values, `[present]` and end-state lines) is the stored name, with its capital `The` (r7).
- **Order.** Ages, phases, civilizations, settlements, regions and events in ascending id; everything else as stated per line. Nothing depends on the order SQLite returns rows in beyond `ORDER BY` the primary key.

### 6.2 Header and blocks

```
# worldgen history v1
level = <summary|full>
[settings]
...
[ages]
...
[phases]
...
[civilizations]
...
[chronicle]
...
[present]
...
[stats]
...
```

The seven blocks of P3-00 11, always all seven, always in this order, each opened by its heading line. A block with nothing to list still has its heading and the lines that are always printed (6.3–6.9).

### 6.3 `[settings]`

Twelve lines `<key> = <value>` in this order, the value from `meta`, escaped (6.1); an empty value is printed `<key> =` (as P0-04 7):

| Key | `meta` key |
| --- | --- |
| `seed` | `seed` |
| `seed_phrase` | `seed_phrase` |
| `map_size` | `param.map_size` |
| `history_span` | `param.history_span` |
| `history_detail` | `param.history_detail` |
| `races` | `param.races` |
| `civs_per_race` | `param.civs_per_race` |
| `event_intensity` | `param.event_intensity` |
| `monster_pressure` | `param.monster_pressure` |
| `divine_activity` | `param.divine_activity` |
| `divine_script` | `param.divine_script` |
| `generator_version` | `generator_version` |

`content_hash` is left to the world report (it identifies the data, not the history).

### 6.4 `[ages]`

One line per `age` row:

```
a<id> <name>: <start_year>-<end_year>, <pl(end_year − start_year + 1, year)>; <race.plural> (ra<race>) made by <god.name> (g<god>, <camp>); <pl(k, civilization)> founded
```

race = `age.race`; god = that race's `creator_god`; camp = the god's `camp` word; k = the civilizations with `start_year` ≤ `founded_year` ≤ `end_year`.

### 6.5 `[phases]`

One line per `phase` row:

```
p<id> a<age> <kind> <start_year>-<end_year>, <pl(end_year − start_year + 1, year)>[; trigger e<trigger_event>]; <pl(m, event)>
```

The trigger part only when `trigger_event` ≠ 0; m = the events with `event.phase` = id.

### 6.6 `[civilizations]`

One block per `civilization` row; the single line `(none)` when there is none. For civilization c (fields of 5.1's `CivilizationDigest`, computed in section 7):

```
c<id> <name>
  race: <race.name> (ra<race>)
  home: <region ref>, <biome key>[, upland]
  founded: <founded_year> in <age name> (a<age>), e<founding_event>
  capital: <settlement ref>                    or   capital: none
  peak regions: <count>, <from>-<to>           or   peak regions: 0
  peak population: <population> in <year>      or   peak population: 0
  settlements: <f> founded, <h> held at the end, <r> ruined
  relations: none                              or   relations: <n> (<kind> <count>, ...)
    <relation line>                                 (one per relation, see below)
  events: <n>
  fate: alive in <E>: <pl(regions, region)>, <pl(h, settlement)>, population <p>
  fate: ended in <ended_year>, e<ending_event>: <sentence of ending_event>
  milestones:
    <span> e<id> <kind key>: <sentence>             (one per milestone, 7.3)
```

- `home`: the region `home_region`, its `biome` column's `biome.key`, and `, upland` when 4 × `walk_cells` < `cells` (the upland rule of P3-00 7, so the critic can check "dwarves settle uplands, faeries wild places" from the report alone).
- `founded`: the age whose years contain `founded_year`.
- `capital`: `civilization.capital` when ≠ 0 (its end state), else `none`.
- `peak regions`, `peak population`: 7.1, 7.2.
- `settlements`: f = settlements with `founder_civ` = c; h = standing settlements with `owner_civ` = c; r = settlements with `founder_civ` = c and status 3 or 4.
- `relations`: R = the `relation` rows with (`a_type` = 3 and `a_id` = c) or (`b_type` = 3 and `b_id` = c). The count line lists each kind present in R once, in ascending byte order of the kind, with its count, separated by `, `. Then one line per row of R, ordered by (`since`, `a_type`, `a_id`, `b_type`, `b_id`, `kind`) ascending: a closed row `    <kind>: <a ref>, <b ref>, <since>-<until - 1>, e<since_event> to e<until_event>` (when `until` = `since`, a relation that began and ended within one year, P3-04 6.7 r9, the span prints `<since>-<since>`, not `<since>-<since - 1>`; r6), an open row (`until` NULL) `    <kind>: <a ref>, <b ref>, since <since>, e<since_event>`. Both entities are printed in stored order (a, then b), whichever is c, so a symmetric relation reads the same in both civilizations' blocks; `until - 1` is the last year of the relation (P3-04 10: in force for `since` ≤ Y < `until`).
- `events`: the number of events with at least one participant of type 3 and id c.
- `fate`: `alive in` when `ended_year` is NULL: regions = rows of `region_control` with `civ` = c and `to_year` NULL; h as above; p = Σ `population` of the standing settlements with `owner_civ` = c. Otherwise `ended in`, with the sentence of the ending event.
- `milestones:` is always printed; its lines (7.3) are the short form of a chronicle line: span, id, kind key and sentence, without salience, references or causes.

### 6.7 `[chronicle]`

```
threshold = <T>
events = <number of event rows>
shown = <number of events printed>
== a<id> <age name>: <start_year>-<end_year>                  one per age, ascending id
-- p<id> <kind>: <start_year>-<end_year>                      one per phase of that age, ascending id
<event line>                                                  one per shown event of that phase, ascending id
(<pl(h, more event)> not shown)                               summary only, when h > 0 events of the phase are not shown
```

T is 0 at `full` and the summary threshold of 7.4 at `summary`; an event is shown when its `salience` ≥ T. Events are grouped under the phase `event.phase` (5.3 makes it the phase containing `year`; since years do not decrease with id and phases tile the span in order, each phase's events are consecutive ids). Every age and phase heading is printed, also when no event of it is shown. The event line:

```
<span> e<id> <kind key>/<salience>: <sentence> {<refs>}[ [causes e<c1>, e<c2>, ...]]
```

`<refs>` is a space-separated list, possibly empty (`{}`), in this order: `@rg<region>` when `event.region` ≠ 0; `@s<settlement>` when `event.settlement` ≠ 0; each participant in `seq` order as `<role key>=<prefix><entity_id>`; each value in `seq` order as `<value key>=<value>`. The causes part follows when the event has `event_cause` rows, the causes in ascending id separated by `, `. Example (sample, e3): `0 e3 settlement_founded/40: Finen founded Voeledell in Northeast Downs IV. {@rg12 @s1 settlement=s1 founder=c1 population=200} [causes e2]`.

**Parsing a line** (for scripts): the causes part, if present, starts at the last occurrence of ` [causes ` and the line ends with `]`; the refs start at the last occurrence of ` {` before it. Neither part can contain `{`, `[` or a second `}`, so this is unambiguous whatever the sentence contains. Role keys and value keys of one kind are distinct (P3-04 8.2), and a reference starts with a letter prefix while a value starts with a digit or `-`, so `key=...` items are unambiguous too.

**Grepping**: a civilization's events, `grep -E '=c3[ }]'`; events at a region, `grep -E '@rg12[ }]'`; what an event caused, `grep -E 'causes (.*, )?e45[],]'`.

### 6.8 `[present]`

The end state, as of year E (P3-00 2.1):

```
year: <E>
civilizations alive: <alive> of <civilization rows>
c<id> <name>: <pl(regions, region)>, <pl(h, settlement)>, population <p>         one per alive civilization, as its fate line (6.6)
regions: <region rows>; held <H>, free <region rows − H>
  rg<id> <name>: <civilization ref> since <from_year>       full only, one per region; or "  rg<id> <name>: free"
settlements: <settlement rows>; thriving <n1>, declining <n2>, ruined <n3>, faded <n4>
  <settlement line>                                         full only, one per settlement
settlement kinds: hamlet <k1>, village <k2>, town <k3>, city <k4>, fortress <k5>
largest settlement: <settlement ref>, population <p>        or   largest settlement: none
```

H = rows of `region_control` with `to_year` NULL (the open row of a region gives its holder and `from_year`). The status and kind counts are over all settlements by their end state. The largest settlement is the standing settlement of the largest `population`, the smaller id on a tie; `none` when no settlement stands. The settlement line:

```
  s<id> <name>: <kind>, <status>, population <population>, <owner ref or "no owner">, at <x> <y> in <region ref>, founded <founded_year> by <founder ref>[, ruined <ruined_year>, e<ruin_event>]
```

### 6.9 `[stats]`

One line `<key> = <integer>` per key of section 9, in the order of section 9, at both levels (the block does not depend on `--level`).

## 7. Computations

All integer, all over the checked tables; E = `history_end_year`; years Y range over [0, E − 1].

### 7.1 Regions held and the peak

held(c, Y) = the number of `region_control` rows with `civ` = c and `from_year` ≤ Y < t, where t = `to_year`, or E when it is NULL. M = max over Y of held(c, Y). If M = 0, `peak regions: 0`. Otherwise from = the smallest Y with held(c, Y) = M, and to = the largest Y′ ≥ from such that held(c, y) = M for every y in [from, Y′]: the first run at the peak, inclusive. Implementation: per civilization, a difference array over [0, E] from its rows and a prefix sum (O(E + rows) time, O(E) memory, reused across civilizations); no per-row loop over years.

### 7.2 Population and the peak

The census records populations at foundings, at changes of owner, kind or status, and at phase ends (P3-04 6.7), so the population of a civilization is known at the census years and the peak is taken over them. Let the census years be the distinct `year` values of `settlement_census`, ascending. At a census year Y, the state of settlement s is its row with the largest `year` ≤ Y (none when `founded_year` > Y; 5.3 puts the first row at `founded_year`). pop(c, Y) = Σ `population` over the settlements whose state at Y has `owner_civ` = c and status 1 or 2. P = max over the census years of pop(c, Y); if P = 0 (or c never owns a standing settlement), `peak population: 0`; otherwise the year is the smallest census year with pop(c, Y) = P. Implementation: one pass over all census rows sorted by (year, settlement), with the current state per settlement and a running sum per civilization (only the civilizations a row changes need to be compared with their peak), O(rows log rows).

### 7.3 Milestones

E_c = the events with at least one participant of type 3 and id c. The milestones are the first min(6, |E_c|) of E_c ordered by (`salience` descending, `id` ascending), printed in ascending id. Six (`MILESTONES`, section 8) is enough for founding, peak events, wars and fate at a glance; the chronicle has the rest.

### 7.4 The summary threshold

n(t) = the number of events with `salience` ≥ t. **T** = the smallest t in [`SUMMARY_MIN_SALIENCE`, 100] = [20, 100] with n(t) ≤ `SUMMARY_BUDGET` = 250; if there is none, T = 100 (every salience-100 event is shown, even beyond the budget). So the chronicle of a `summary` report has at most 250 event lines unless more than 250 events have salience 100, and never shows an event below salience 20: with the shipped saliences (P3-04 4.2) a `quiet_era` (20) is shown while the log is small and drops out first when it grows. Computed with a histogram of the 101 salience values; ties need no rule (the threshold is a value, not a choice among events).

### 7.5 Shares of the land (the runaway bound)

A region is an **islet** when `cells` − `lake_cells` < `ISLET_MAX_DRY_CELLS` = 1172 (P3-00 7, decided; the shipped value of `history.engine.islet_max_dry_cells`, P3-04 4.1). N = the number of non-islet regions. h(c, Y) = held(c, Y) of 7.1 counting non-islet regions only. When N > 0, share(c, Y) = floor(1000 × h(c, Y) / N), per mille.

- `share_max_permille` = the largest share(c, Y) over all c and Y; `share_max_civ`, `share_max_year` = the (Y, c) of the smallest Y, then the smallest c, attaining it; all three 0 when N = 0, there is no civilization, or the largest share is 0.
- `share_over_<k>_years`, for k = 500, 600, 700, 800, 900: the largest, over civilizations c, of the length of the longest run of consecutive years Y in [0, E − 1] with share(c, Y) > k; 0 when there is none. A run belongs to one civilization: when the dominant civilization changes, the run starts again.

P3-00 12.3's runaway bound ("no civilization holds more than 0.8 of non-islet regions for more than 1,000 consecutive years") is `share_over_800_years` ≤ 1000, read from `[stats]` alone. The other levels let the sweeps see how close worlds come.

### 7.6 Bounds

Counts and sums are `int64`: at most 4,096 civilizations, 1,000,000 settlements, 10,000,000 events (P3-04 6.1), populations below 2^31, so Σ population < 2^51 and 1000 × n < 2^44. No computation needs more than O((events + participants + census rows + region_control rows) log + civilizations × E) time.

## 8. Constants of the format (no tuning)

| Constant | Value | Used in |
| --- | --- | --- |
| `SUMMARY_MIN_SALIENCE` | 20 | 7.4 |
| `SUMMARY_BUDGET` | 250 | 7.4 |
| `MILESTONES` | 6 | 7.3 |
| `ISLET_MAX_DRY_CELLS` | 1172 | 7.5 |
| share levels | 500, 600, 700, 800, 900 | 7.5, 9 |

They are part of format v1: changing one changes the text of the same world, so it is a `v2` change (10.3). **`history.report` gains no field** (3.2, question 1): what a reader tunes is which kinds of events matter, and that is the per-kind base `salience` of `history.report.events` (data, stored in `event_kind.salience` and `event.salience` of every world). A rule that sets an event's salience above or below its kind's base is P3-05's and P3-06's. `ISLET_MAX_DRY_CELLS` duplicates a tuning value because the world does not store it; if `history.engine.islet_max_dry_cells` is ever changed, the islet flag should be stored in the world (question 5), since the report must not read data.

With these numbers a `summary` report of a 6,000-year world with a dozen civilizations is about 600 lines: about 40 of frame (`[settings]`, `[ages]`, `[phases]`), about 17 per civilization, at most 250 event lines plus about 20 headings, about 20 of `[present]` and about 90 of `[stats]`.

## 9. `[stats]` keys

Integers only. "Standing" = status 1 or 2. Every key of the world report's `[history]` section (P3-04 11.1) appears here with **the same name and the same value**, so a sweep reads one block; `[history]` stays in the world report as a quick summary (invariant 6). In this order:

| Key | Definition |
| --- | --- |
| `end_year` | E |
| `ages` | rows of `age` |
| `phases` | rows of `phase` |
| `phases_opening`, `phases_quiet`, `phases_flare`, `phases_final` | rows of `phase` of kind 1, 2, 3, 4 |
| `simulated_years` | Σ (`end_year` − `start_year` + 1) over phases of kinds 1, 3, 4 |
| `simulated_permille` | floor(1000 × `simulated_years` / E) |
| `quiet_years` | the same sum over kind 2 |
| `flare_years` | the same sum over kind 3 |
| `events` | rows of `event` |
| `events_<key>` | one line per `event_kind` row in id order: rows of `event` of that kind (0 included) |
| `events_caused` | events with at least one `event_cause` row |
| `cause_links` | rows of `event_cause` |
| `events_per_1000_simulated_years` | floor(1000 × `events` / `simulated_years`); 0 when `simulated_years` is 0 (P3-00 12.3: at most 200 events per simulated year, so ≤ 200000) |
| `summary_threshold` | T of 7.4 (also at `full`) |
| `summary_events` | n(T): the events a `summary` report shows |
| `civilizations` | rows of `civilization` |
| `civilizations_alive` | rows with `ended_year` NULL |
| `civilizations_ended` | rows with `ended_year` not NULL |
| `civilizations_<race key>` | one line per `race` row in id order, present or not: civilizations of that race (P3-00 12.3: ≥ 1 for every present race) |
| `civ_lifetime_max` | max over civilizations of (`ended_year`, or E when NULL) − `founded_year`; 0 when there is none |
| `civ_regions_peak_max` | max over civilizations of the peak count of 7.1; 0 when none |
| `civ_population_peak_max` | max over civilizations of the peak of 7.2; 0 when none |
| `settlements` | rows of `settlement` |
| `settlements_standing` | rows with status 1 or 2 |
| `settlements_thriving`, `settlements_declining`, `settlements_ruined`, `settlements_faded` | rows by end status 1, 2, 3, 4 |
| `settlements_hamlet`, `settlements_village`, `settlements_town`, `settlements_city`, `settlements_fortress` | rows by end kind 1–5, any status |
| `settlements_unowned` | standing rows with `owner_civ` 0 (abandoned but not ruined) |
| `population` | Σ `population` of the standing settlements |
| `population_settlement_max` | max `population` of a standing settlement; 0 when none |
| `census_rows` | rows of `settlement_census` |
| `regions` | rows of `region` |
| `regions_nonislet` | N of 7.5 |
| `regions_held` | rows of `region_control` with `to_year` NULL |
| `region_control_rows` | rows of `region_control` |
| `regions_ever_held` | distinct `region` values of `region_control` |
| `share_max_permille`, `share_max_civ`, `share_max_year` | 7.5 |
| `share_over_500_years`, `share_over_600_years`, `share_over_700_years`, `share_over_800_years`, `share_over_900_years` | 7.5 |
| `relations` | rows of `relation` |
| `relations_open` | rows with `until` NULL |
| `wars` | rows of kind `war` (the war relation's kind key, which P3-06 is asked to use: 18.7) |
| `wars_open` | rows of kind `war` with `until` NULL |
| `population_<race key>` | one line per `race` row in id order: Σ `population` of the standing settlements whose `owner_civ` is a civilization of that race (P3-05 15.4) |
| `settlements_standing_<race key>` | one line per `race` row: those settlements |
| `regions_held_<race key>` | one line per `race` row: rows of `region_control` with `to_year` NULL whose `civ` is of that race |
| `civ_settlements_max` | max over alive civilizations of their standing settlements; 0 when none |
| `settlements_colonies` | settlements whose `founding_event` is of kind `colony_founded` |
| `civilizations_seceded` | civilizations whose `founding_event` is of kind `civ_seceded` |
| `relations_enmity`, `relations_peace` | rows of `relation` of kind `enmity`, of kind `peace` (P3-06 15.4; r3) |
| `war_years_total` | Σ over `war` rows of (`until`, or E when NULL) − `since` |
| `war_years_max` | the largest of those; 0 when there is none |
| `civilizations_conquered` | civilizations whose `ending_event` is of kind `civ_conquered` |
| `settlements_sacked` | settlements whose `ruin_event` is of kind `sacked` |
| `settlements_captured` | distinct `event.settlement` of events of kind `settlement_captured` |
| `regions_conquered` | `region_control` rows whose `event` is of kind `settlement_captured` or `region_ceded` |
| `races_alive` | `race` rows with at least one civilization with `ended_year` NULL |
| `wars_attacker_<race key>` | one line per `race` row in id order: `war` rows whose `a_id` is a civilization of that race |

For the P3-04 to P3-06 sweeps (P3-00 12.2, 12.3): order and schedule from `[ages]` and the world report's `[chronology]`; phases and the 10–50% bound from `simulated_permille`; "not empty" from `civilizations_alive`, `settlements_standing` and `civilizations_<race>`; "not runaway" from `share_over_800_years` and `events_per_1000_simulated_years`; log integrity is the report's exit status 0 (5.2) together with P3-04's own invariant tests. Population against capacity is P3-05's statistic: capacity is not stored in the world.

## 10. What the stubs fill now, what fills later, and how the format grows

### 10.1 With P3-04's stubs (now)

The report can be implemented and tested now against stub worlds and the hand-made fixtures of 14.3. On a stub world: one civilization and one hamlet of 200 per race, all alive; `peak regions` 1 from the founding to E − 1, `peak population` 200 at the founding; `relations: none`, `wars = 0`; no flare-up phase (`phases_flare = 0`); `[present]` all thriving hamlets; `share_max_permille` = floor(1000 / N); the log has at most a few dozen events, all of salience ≥ 20, so T = 20 and the `summary` chronicle equals the `full` one (only the `threshold` line differs, as the `[present]` lists do). The fixtures exercise everything else (ended civilizations, ruins, relations, flare-ups, budgets, islets).

### 10.2 With P3-05 and P3-06

P3-05 (peoples): more civilizations per race (`civs_per_race`), many settlements with real census populations (peaks become meaningful), settlement kinds and statuses beyond thriving hamlets, abandonment (`settlements_unowned`), more event kinds (`events_<key>` lines appear by themselves). P3-06 (conflict): relations and wars (`relations:` lines, `wars`), conquest (`region_control` changes, `regions_ever_held`, the shares), sacking and ruin (`ruined`, `ruin_event`), ended civilizations (`fate: ended`), flare-ups (`flare_up` phases), and logs of thousands of events, where the summary threshold rises above 20. None of this needs a format change: new event kinds come with their templates in the world, relation kinds are printed as stored, and every count is defined over code lists that schema 7 already fixes.

### 10.3 Format evolution: what keeps `v1`, what needs `v2`

**Keeps `v1`** (the header stays `# worldgen history v1`):

1. A new `[stats]` key appended after the last key of the table in section 9 as it stands then (P3-05, P3-06 and P5 specs append keys by an amendment to this section, with a definition in the same style). Keys are never renamed, removed, reordered or redefined.
2. Lines that repeat by world content: `events_<key>` and `civilizations_<race key>` lines (one per catalogue kind and per race row), chronicle lines of new kinds, relation lines and counts of new relation kinds, entity references of types 6–9 once they exist.

**Needs `v2`**: a new block or a new line type in any block other than `[stats]`; any change to the grammar of an existing line, to the order of blocks or lines, or to a heading; removing, renaming or redefining a `[stats]` key; changing a constant of section 8; changing the escaping or the number format. A change to `render_event` or to a stored template changes sentences but not the format (it is a P3-04 change or a data change with its own version bump).

Consumers (sweep scripts, the critic's tools) must read `[stats]` by key, not by position, and ignore keys they do not know.

## 11. Performance

Release build, 6-core machine (P3-00 12.1): `report --history` within **0.5 s at 512, 2 s at 1024, 8 s at 2048** with P3-06's logs (P3-00 6.1 expects 5,000–30,000 events at 1024), either level; on stub worlds under 50 ms. The cost is reading the tables once (`HistoryView::load` and `read_history_tables`), one `render_event` per printed sentence (each event at most twice: chronicle and milestones or fate), and the sweeps of section 7. A test with a synthetic large world (14.5) reports the time in a tagged suite, not in `ctest`'s default run. Memory: the tables in memory, under 100 MB at 2048.

## 12. Determinism notes (for the auditor)

The history report is part of the cross-platform contract, as `report` and `hash` are (P0-04 10, 11): the same world file gives byte-identical text on every platform and compiler.

| Concern | Rule |
| --- | --- |
| Arithmetic | integers only (`int64`), divisions `floor` of non-negative values; no floating point anywhere in `digest.cpp` or `history_report.cpp` (H1) |
| Formatting | `std::to_chars` or project code; no locale, no `printf` family, no streams with locale state; LF only, written as bytes (P0-04 4.4: binary stdout on Windows) |
| Order | every list in ascending id or a stated key ending in an id; maps keyed by integers (or by a relation's full key); no unordered container; no pointer order |
| Ties | 7.1 earliest run; 7.2 earliest census year; 7.3 (salience desc, id asc); 7.5 (Y, then c); largest settlement (population desc, id asc); 7.4 needs none |
| Rows | read with `ORDER BY` the primary key (P0-02 raw rows), so SQLite's internal order never matters |
| Text | escaping by bytes (6.1), never by `isprint` or locale classification; no case conversion |
| Time | none; `--verbose` timings go to stderr only (P0-04 4.5) |

Lint impact: none expected. `worldgen/src/history/` and `worldgen/src/inspect/` are in the lint's scope; the new code uses no floating point, no `<random>`, no unordered containers and no clock.

## 13. Invariants

For every world with history and both levels:

1. The text satisfies 6.1: ASCII and LF only, ends with LF, no empty line, no trailing space, the header and the seven blocks in order.
2. At `full`, the chronicle has exactly one event line per `event` row, in id order, and `shown` = `events`; at `summary`, the event lines are exactly the events with `salience` ≥ T, `shown` = `summary_events`, and `shown` ≤ 250 unless T = 100.
3. The sum over phases of shown lines and `(n more events not shown)` counts equals `events`; each phase's count in `[phases]` equals its shown and hidden events.
4. Every sentence in the report equals `render_event` of its event byte for byte (after escaping, which is the identity on valid data), including a sentence with a `The …` name mid-sentence (lower-cased by `render_event`) printed next to the same entity's reference in its refs (capitalised) (r7).
5. `[stats]` is identical at both levels; `[settings]`, `[ages]`, `[phases]` and `[civilizations]` are identical at both levels; the two levels differ only in the `level` line, the chronicle (`threshold`, `shown`, event lines, hidden lines) and the `full`-only lines of `[present]`.
6. Every key of the world report's `[history]` section appears in `[stats]` with the same value.
7. Consistency between blocks: each `fate: alive` line has the same counts and population as that civilization's line in `[present]`; `civilizations_alive` = the `alive` count of `[present]`; `[present]`'s `held` = `regions_held`.
8. The report of a world does not depend on the installed data, the current directory, the environment, the thread count the world was generated with, or the platform.

## 14. Tests required

For the independent test writer. The reference `worldgen/tests/reference/p3_report_reference.py` (Python, exact integers, reading the world with `sqlite3` only) reproduces the whole report byte for byte from a world file at both levels, from this spec alone. It may reuse the P3-04 reference's template renderer (`p3_engine_reference.py`, itself independent of the C++); it does not call or port the C++.

### 14.1 Unit tests (C++, `worldgen-tests-history-report`)

1. `escape_history_text`: backslash, each control byte, 0x7F, bytes 0x80–0xFF, plain ASCII unchanged; the trailing-space rule on a template ending in a space and on an empty sentence.
2. `pl` words and spans of 6.1 through report lines (`1 year`, `2 years`, `1 more event`, a one-year phase `2500-2500`).
3. Each block of section 6 on the fixtures of 14.3, against expected lines written in the test (not produced by the code under test), including `(none)` civilizations, `capital: none`, `peak regions: 0`, `peak population: 0`, `largest settlement: none`, `relations: none`, a relation of a reserved entity type printed without a name.
4. 7.1: two runs at the same peak (the first is printed); a peak of one year; a region held until E − 1 (open row). 7.2: two census years with the same peak (the earlier); a settlement changing owner between two civilizations at a census year; ruined and faded settlements not counted. 7.3: ties of salience broken by id; fewer than six events. 7.4: the 1,000 notes of H4 (salience (37 × i) mod 101, i = 1 … 1000) alone have n(76) = 248 ≤ 250 < n(75), so T = 76 if the fixture's other events do not change the count (the test counts all events of its fixture by hand); H5's 300 events of salience 100 give T = 100 and at least 300 lines. 7.5: islets excluded from N and from h; a civilization above 800 per mille for exactly 1,000 and then 1,001 consecutive years; a handover from one dominant civilization to another in the same year (two runs, not one); `share_max_*` ties in year and in civilization.
5. Each check of 5.2 item 1 and 5.3 once, with its exact message, on a fixture world corrupted with `db::testing::execute` (the first failure only; corruptions chosen so that the earlier checks of 5.2 pass).
6. `HistoryView::load` failures arrive as `corrupt_layer` errors and never as `FatalError` (one case per kind of corruption P3-04 lists).
7. A world whose catalogue has kinds the build does not compile (the fixtures' test kinds) is reported normally (A1).
8. `[stats]` ⊇ `[history]` (invariant 6) on each fixture and stub world.

### 14.2 CLI tests (`worldgen-cli-tests`, file `cli_history_report_test.cpp`)

9. `report --history` on a default world: exit 0, stdout is the report; `--level full`; `--out` writes the same bytes and prints nothing; existing `--out` without `--force` → 4; with `--force` replaced; `--force` without `--out` accepted.
10. Usage: `--level full` without `--history` → 2 with `--level needs --history`; `--level brief` → 2 with its message; `--history x` (a positional) → 2; `report --help` lists `--history` and `--level`.
11. No history: worlds from `--until none`, `--until regions`, `--until chronology` and `--test-pattern` → exit 3 with the exact message of section 4 (stages printed, `none` for the empty list), nothing on stdout, no `--out` file.
12. A schema 6 file → exit 3 with the reader's message.
13. On Windows, no CR byte in the output; on every platform, environment independence (`TZ`, `LANG`) as P0-04 12.2.

### 14.3 Hand-made fixture worlds

Small worlds described in fixture files under `worldgen/tests/data/history-report/` that both sides read (TOML is suggested: the references already parse it). **Two independent builds of each fixture**: the C++ test writes it through the writer API (a synthetic stage through `generate_world_with_stages`, as P3-04 16.2 item 17, writing every schema 7 table and the `meta` keys the fixture lists), and the Python reference writes its own copy through SQL (`sqlite3`, the schema 7 DDL of `docs/specs/schema.md`, rows inserted from the same file). **The two copies must have the same table hashes, not the same file bytes**: Python's SQLite is not the vendored version and does not use the writer's page settings and `VACUUM INTO`, so the bytes may differ legitimately. The golden file records, per fixture, the logical hash of every table (P0-04 8, the `table <name> <hex>` lines of `hash --detail`, computed by the reference over its copy) as `<fixture> table <name> <hex>`; the C++ test first compares `hash --detail`'s table lines of its world with them (a difference is a fixture-building defect, reported before any report text is compared), and only then compares the report hashes. The `schema` and `file` lines are not compared. Coverage required (the test writer chooses the rows):

- **H1 war and ruin**: at least two races and three civilizations, one of them ended (with `ended_year`, `ending_event` and its sentence); one settlement ruined by a sacking (`ruined_year`, `ruin_event`) and one faded; one standing settlement with `owner_civ` 0; a flare-up phase with its `flare_up` event and trigger; a closed `war` relation and an open relation of another kind, with two relations of one pair starting in different years (ordering of 6.6); a region taken from one civilization by another (a closed and an open `region_control` row of one region); census rows with population changes at phase ends, and a tie of peak population between two census years; events with two or more causes; a role of `:all` form with two participants; a negative value; an event with no region and no participants (`{}` after the sentence or only values); hand-made event kinds absent from the compiled catalogue, with templates; all five settlement kinds; a region name with a backslash and a non-ASCII byte, and a template literal with a backslash (escaping); a relation with a reserved entity type 6.
- **H2 minimal**: one race, one civilization with no settlement (`capital: none`, `peak population: 0`, `largest settlement: none`), a span of 2000, an opening and a final phase only (no quiet phase), no relation.
- **H3 runaway**: regions of which some are islets; a civilization above 800 per mille for 1,001 consecutive years and another above 900 for a shorter run; a dominance handover in one year (7.5).
- **H4 budget**: H2 with 1,000 extra events of a test kind (no roles, no region, one value `n` = i), all in one year of its opening phase, salience (37 × i) mod 101 for i = 1 … 1000, so that T rises above 20; **H5 saturated**: H2 with 300 extra events of salience 100 (T = 100, 300 lines beyond the budget).

### 14.4 Golden values

`worldgen/tests/golden/p3-07-report.txt`, written by the reference, LF, sorted by case then level: one line `<case> <level> <sha256 of the report text>` for

- the world cases of `world-hashes.sh` that have history: `geo-seed-0-512`, `geo-seed-42-512`, `geo-phrase-512`, `geo-seed-1-1024`, `hist-span-2000-512`, `hist-races-elf-orc-1024`, `hist-dense-1024` (generated with the checkout's `game/data`, as the script does), at `summary` and `full`;
- the fixtures H1–H5 at `summary` and `full`;
- `sample-42-1024`: `generate --seed 42 --map-size 1024` with default settings, at `summary`, whose text must equal section 16 byte for byte (the test writer checks the reference's text against the section when recording; the C++ test compares the hash). The 1024 cases run in a `*-slow` suite.

Plus, for each world case, the `[stats]` lines as `<case> stats <key> <value>`, so that a difference is readable without regenerating. The world cases' lines are re-recorded by running the reference whenever a merge changes those worlds (P3-05, P3-06, a data change), like the other world goldens; never to make a C++ test pass.

### 14.5 Property and cross-platform tests

14. Over the P3-04 sweep worlds (1024 seeds 0–49, 512 seeds 0–19, and the span and subset variants of P3-00 12.2) and the fixtures: invariants 1–8; the reference's text equals the C++ text for every world (the validation specialist runs the reference over the sweep).
15. A synthetic large world (30,000 events of a test kind with salience spread over 0–100, 2,000 settlements, 20,000 census rows, 500 `region_control` rows, 200 relations) for the timing of section 11 (tagged suite) and invariant 2.
16. Cross-compiler: `world-hashes.sh` saves the `full` history report of every case with history and `compare` includes it (amendment 18.6).

## 15. Viewer impact

**None.** The viewer neither runs nor parses the report; `gdext-headless-check` reads the world report only. P4's browser may call `digest_history` (5.1) for a civilization's peaks and fate, and must call `render_event` for sentences (P3-00 6.3), so that the viewer and the report never disagree.

## 16. Sample: seed 42, 1024, default settings, level `summary`

`worldgen-cli generate --seed 42 --map-size 1024` with generator 0.14.0 (P3-04's stub world, its section 17), then `worldgen-cli report --db <file> --history`. From the spec writer's prototype; the reference must reproduce it byte for byte (14.4). 269 lines; at `full` the text differs only in `level = full`, `threshold = 0` and the fifty `  rg<id> ...` and six `  s<id> ...` lines of `[present]`.

```
# worldgen history v1
level = summary
[settings]
seed = 42
seed_phrase =
map_size = 1024
history_span = 6000
history_detail = normal
races = elf,goblin,dwarf,faerie,human,orc
civs_per_race = auto
event_intensity = normal
monster_pressure = normal
divine_activity = normal
divine_script =
generator_version = 0.14.0
[ages]
a1 The Age of Elves: 0-2073, 2074 years; Elves (ra1) made by Fielie (g1, good); 1 civilization founded
a2 The Age of Goblins: 2074-3241, 1168 years; Goblins (ra2) made by Krok (g2, evil); 1 civilization founded
a3 The Age of Dwarves: 3242-4242, 1001 years; Dwarves (ra3) made by Bronhak (g3, neutral); 1 civilization founded
a4 The Age of Faeries: 4243-4968, 726 years; Faeries (ra4) made by Syfiash (g4, good); 1 civilization founded
a5 The Age of Humans: 4969-5595, 627 years; Humans (ra5) made by Wealstuld (g5, neutral); 1 civilization founded
a6 The Age of Orcs: 5596-5999, 404 years; Orcs (ra6) made by Kruu (g6, evil); 1 civilization founded
[phases]
p1 a1 opening 0-149, 150 years; trigger e1; 3 events
p2 a1 quiet 150-2073, 1924 years; 1 event
p3 a2 opening 2074-2223, 150 years; trigger e5; 3 events
p4 a2 quiet 2224-3241, 1018 years; 2 events
p5 a3 opening 3242-3391, 150 years; trigger e10; 3 events
p6 a3 quiet 3392-4242, 851 years; 3 events
p7 a4 opening 4243-4392, 150 years; trigger e16; 3 events
p8 a4 quiet 4393-4968, 576 years; 4 events
p9 a5 opening 4969-5118, 150 years; trigger e23; 3 events
p10 a5 quiet 5119-5595, 477 years; 5 events
p11 a6 opening 5596-5745, 150 years; trigger e31; 3 events
p12 a6 final 5746-5999, 254 years; 0 events
[civilizations]
c1 Finen
  race: Elf (ra1)
  home: Northeast Downs IV (rg12), grassland
  founded: 0 in The Age of Elves (a1), e2
  capital: Voeledell (s1)
  peak regions: 1, 0-5999
  peak population: 200 in 0
  settlements: 1 founded, 1 held at the end, 0 ruined
  relations: none
  events: 7
  fate: alive in 6000: 1 region, 1 settlement, population 200
  milestones:
    0 e2 civ_founded: The Elves founded Finen in Northeast Downs IV.
    0 e3 settlement_founded: Finen founded Voeledell in Northeast Downs IV.
    150-2073 e4 quiet_era: Finen passed 1924 quiet years (12 generations), from 150 to 2073.
    2224-3241 e8 quiet_era: Finen passed 1018 quiet years (7 generations), from 2224 to 3241.
    3392-4242 e13 quiet_era: Finen passed 851 quiet years (6 generations), from 3392 to 4242.
    4393-4968 e19 quiet_era: Finen passed 576 quiet years (4 generations), from 4393 to 4968.
c2 The Skib Horde
  race: Goblin (ra2)
  home: Northeast Downs II (rg10), grassland
  founded: 2074 in The Age of Goblins (a2), e6
  capital: Skrisligg Pit (s2)
  peak regions: 1, 2074-5999
  peak population: 200 in 2074
  settlements: 1 founded, 1 held at the end, 0 ruined
  relations: none
  events: 6
  fate: alive in 6000: 1 region, 1 settlement, population 200
  milestones:
    2074 e6 civ_founded: The Goblins founded The Skib Horde in Northeast Downs II.
    2074 e7 settlement_founded: The Skib Horde founded Skrisligg Pit in Northeast Downs II.
    2224-3241 e9 quiet_era: The Skib Horde passed 1018 quiet years (67 generations), from 2224 to 3241.
    3392-4242 e14 quiet_era: The Skib Horde passed 851 quiet years (57 generations), from 3392 to 4242.
    4393-4968 e20 quiet_era: The Skib Horde passed 576 quiet years (39 generations), from 4393 to 4968.
    5119-5595 e27 quiet_era: The Skib Horde passed 477 quiet years (31 generations), from 5119 to 5595.
c3 The Kaindnimm Throne
  race: Dwarf (ra3)
  home: Northeast Wood (rg14), temperate_forest
  founded: 3242 in The Age of Dwarves (a3), e11
  capital: Daidmin (s3)
  peak regions: 1, 3242-5999
  peak population: 200 in 3242
  settlements: 1 founded, 1 held at the end, 0 ruined
  relations: none
  events: 5
  fate: alive in 6000: 1 region, 1 settlement, population 200
  milestones:
    3242 e11 civ_founded: The Dwarves founded The Kaindnimm Throne in Northeast Wood.
    3242 e12 settlement_founded: The Kaindnimm Throne founded Daidmin in Northeast Wood.
    3392-4242 e15 quiet_era: The Kaindnimm Throne passed 851 quiet years (12 generations), from 3392 to 4242.
    4393-4968 e21 quiet_era: The Kaindnimm Throne passed 576 quiet years (8 generations), from 4393 to 4968.
    5119-5595 e28 quiet_era: The Kaindnimm Throne passed 477 quiet years (7 generations), from 5119 to 5595.
c4 The Circle of Fysoli
  race: Faerie (ra4)
  home: West Downs (rg30), grassland
  founded: 4243 in The Age of Faeries (a4), e17
  capital: Lowhou (s4)
  peak regions: 1, 4243-5999
  peak population: 200 in 4243
  settlements: 1 founded, 1 held at the end, 0 ruined
  relations: none
  events: 4
  fate: alive in 6000: 1 region, 1 settlement, population 200
  milestones:
    4243 e17 civ_founded: The Faeries founded The Circle of Fysoli in West Downs.
    4243 e18 settlement_founded: The Circle of Fysoli founded Lowhou in West Downs.
    4393-4968 e22 quiet_era: The Circle of Fysoli passed 576 quiet years (6 generations), from 4393 to 4968.
    5119-5595 e29 quiet_era: The Circle of Fysoli passed 477 quiet years (5 generations), from 5119 to 5595.
c5 The Kingdom of Alird
  race: Human (ra5)
  home: Northeast Downs V (rg18), grassland
  founded: 4969 in The Age of Humans (a5), e24
  capital: Aledsealdbury (s5)
  peak regions: 1, 4969-5999
  peak population: 200 in 4969
  settlements: 1 founded, 1 held at the end, 0 ruined
  relations: none
  events: 3
  fate: alive in 6000: 1 region, 1 settlement, population 200
  milestones:
    4969 e24 civ_founded: The Humans founded The Kingdom of Alird in Northeast Downs V.
    4969 e25 settlement_founded: The Kingdom of Alird founded Aledsealdbury in Northeast Downs V.
    5119-5595 e30 quiet_era: The Kingdom of Alird passed 477 quiet years (19 generations), from 5119 to 5595.
c6 The Warband of Baaldkrok
  race: Orc (ra6)
  home: Northwest Downs II (rg4), grassland
  founded: 5596 in The Age of Orcs (a6), e32
  capital: Uld'kag (s6)
  peak regions: 1, 5596-5999
  peak population: 200 in 5596
  settlements: 1 founded, 1 held at the end, 0 ruined
  relations: none
  events: 2
  fate: alive in 6000: 1 region, 1 settlement, population 200
  milestones:
    5596 e32 civ_founded: The Orcs founded The Warband of Baaldkrok in Northwest Downs II.
    5596 e33 settlement_founded: The Warband of Baaldkrok founded Uld'kag in Northwest Downs II.
[chronicle]
threshold = 20
events = 33
shown = 33
== a1 The Age of Elves: 0-2073
-- p1 opening: 0-149
0 e1 awakening/100: Fielie awakened the Elves in Northeast Downs IV. {@rg12 race=ra1 creator=g1}
0 e2 civ_founded/80: The Elves founded Finen in Northeast Downs IV. {@rg12 civilization=c1 race=ra1} [causes e1]
0 e3 settlement_founded/40: Finen founded Voeledell in Northeast Downs IV. {@rg12 @s1 settlement=s1 founder=c1 population=200} [causes e2]
-- p2 quiet: 150-2073
150-2073 e4 quiet_era/20: Finen passed 1924 quiet years (12 generations), from 150 to 2073. {@rg12 civilization=c1 years=1924 generations=12}
== a2 The Age of Goblins: 2074-3241
-- p3 opening: 2074-2223
2074 e5 awakening/100: Krok awakened the Goblins in Northeast Downs II. {@rg10 race=ra2 creator=g2}
2074 e6 civ_founded/80: The Goblins founded The Skib Horde in Northeast Downs II. {@rg10 civilization=c2 race=ra2} [causes e5]
2074 e7 settlement_founded/40: The Skib Horde founded Skrisligg Pit in Northeast Downs II. {@rg10 @s2 settlement=s2 founder=c2 population=200} [causes e6]
-- p4 quiet: 2224-3241
2224-3241 e8 quiet_era/20: Finen passed 1018 quiet years (7 generations), from 2224 to 3241. {@rg12 civilization=c1 years=1018 generations=7}
2224-3241 e9 quiet_era/20: The Skib Horde passed 1018 quiet years (67 generations), from 2224 to 3241. {@rg10 civilization=c2 years=1018 generations=67}
== a3 The Age of Dwarves: 3242-4242
-- p5 opening: 3242-3391
3242 e10 awakening/100: Bronhak awakened the Dwarves in Northeast Wood. {@rg14 race=ra3 creator=g3}
3242 e11 civ_founded/80: The Dwarves founded The Kaindnimm Throne in Northeast Wood. {@rg14 civilization=c3 race=ra3} [causes e10]
3242 e12 settlement_founded/40: The Kaindnimm Throne founded Daidmin in Northeast Wood. {@rg14 @s3 settlement=s3 founder=c3 population=200} [causes e11]
-- p6 quiet: 3392-4242
3392-4242 e13 quiet_era/20: Finen passed 851 quiet years (6 generations), from 3392 to 4242. {@rg12 civilization=c1 years=851 generations=6}
3392-4242 e14 quiet_era/20: The Skib Horde passed 851 quiet years (57 generations), from 3392 to 4242. {@rg10 civilization=c2 years=851 generations=57}
3392-4242 e15 quiet_era/20: The Kaindnimm Throne passed 851 quiet years (12 generations), from 3392 to 4242. {@rg14 civilization=c3 years=851 generations=12}
== a4 The Age of Faeries: 4243-4968
-- p7 opening: 4243-4392
4243 e16 awakening/100: Syfiash awakened the Faeries in West Downs. {@rg30 race=ra4 creator=g4}
4243 e17 civ_founded/80: The Faeries founded The Circle of Fysoli in West Downs. {@rg30 civilization=c4 race=ra4} [causes e16]
4243 e18 settlement_founded/40: The Circle of Fysoli founded Lowhou in West Downs. {@rg30 @s4 settlement=s4 founder=c4 population=200} [causes e17]
-- p8 quiet: 4393-4968
4393-4968 e19 quiet_era/20: Finen passed 576 quiet years (4 generations), from 4393 to 4968. {@rg12 civilization=c1 years=576 generations=4}
4393-4968 e20 quiet_era/20: The Skib Horde passed 576 quiet years (39 generations), from 4393 to 4968. {@rg10 civilization=c2 years=576 generations=39}
4393-4968 e21 quiet_era/20: The Kaindnimm Throne passed 576 quiet years (8 generations), from 4393 to 4968. {@rg14 civilization=c3 years=576 generations=8}
4393-4968 e22 quiet_era/20: The Circle of Fysoli passed 576 quiet years (6 generations), from 4393 to 4968. {@rg30 civilization=c4 years=576 generations=6}
== a5 The Age of Humans: 4969-5595
-- p9 opening: 4969-5118
4969 e23 awakening/100: Wealstuld awakened the Humans in Northeast Downs V. {@rg18 race=ra5 creator=g5}
4969 e24 civ_founded/80: The Humans founded The Kingdom of Alird in Northeast Downs V. {@rg18 civilization=c5 race=ra5} [causes e23]
4969 e25 settlement_founded/40: The Kingdom of Alird founded Aledsealdbury in Northeast Downs V. {@rg18 @s5 settlement=s5 founder=c5 population=200} [causes e24]
-- p10 quiet: 5119-5595
5119-5595 e26 quiet_era/20: Finen passed 477 quiet years (3 generations), from 5119 to 5595. {@rg12 civilization=c1 years=477 generations=3}
5119-5595 e27 quiet_era/20: The Skib Horde passed 477 quiet years (31 generations), from 5119 to 5595. {@rg10 civilization=c2 years=477 generations=31}
5119-5595 e28 quiet_era/20: The Kaindnimm Throne passed 477 quiet years (7 generations), from 5119 to 5595. {@rg14 civilization=c3 years=477 generations=7}
5119-5595 e29 quiet_era/20: The Circle of Fysoli passed 477 quiet years (5 generations), from 5119 to 5595. {@rg30 civilization=c4 years=477 generations=5}
5119-5595 e30 quiet_era/20: The Kingdom of Alird passed 477 quiet years (19 generations), from 5119 to 5595. {@rg18 civilization=c5 years=477 generations=19}
== a6 The Age of Orcs: 5596-5999
-- p11 opening: 5596-5745
5596 e31 awakening/100: Kruu awakened the Orcs in Northwest Downs II. {@rg4 race=ra6 creator=g6}
5596 e32 civ_founded/80: The Orcs founded The Warband of Baaldkrok in Northwest Downs II. {@rg4 civilization=c6 race=ra6} [causes e31]
5596 e33 settlement_founded/40: The Warband of Baaldkrok founded Uld'kag in Northwest Downs II. {@rg4 @s6 settlement=s6 founder=c6 population=200} [causes e32]
-- p12 final: 5746-5999
[present]
year: 6000
civilizations alive: 6 of 6
c1 Finen: 1 region, 1 settlement, population 200
c2 The Skib Horde: 1 region, 1 settlement, population 200
c3 The Kaindnimm Throne: 1 region, 1 settlement, population 200
c4 The Circle of Fysoli: 1 region, 1 settlement, population 200
c5 The Kingdom of Alird: 1 region, 1 settlement, population 200
c6 The Warband of Baaldkrok: 1 region, 1 settlement, population 200
regions: 50; held 6, free 44
settlements: 6; thriving 6, declining 0, ruined 0, faded 0
settlement kinds: hamlet 6, village 0, town 0, city 0, fortress 0
largest settlement: Voeledell (s1), population 200
[stats]
end_year = 6000
ages = 6
phases = 12
phases_opening = 6
phases_quiet = 5
phases_flare = 0
phases_final = 1
simulated_years = 1154
simulated_permille = 192
quiet_years = 4846
flare_years = 0
events = 33
events_awakening = 6
events_civ_founded = 6
events_settlement_founded = 6
events_quiet_era = 15
events_flare_up = 0
events_caused = 12
cause_links = 12
events_per_1000_simulated_years = 28
summary_threshold = 20
summary_events = 33
civilizations = 6
civilizations_alive = 6
civilizations_ended = 0
civilizations_elf = 1
civilizations_goblin = 1
civilizations_dwarf = 1
civilizations_faerie = 1
civilizations_human = 1
civilizations_orc = 1
civ_lifetime_max = 6000
civ_regions_peak_max = 1
civ_population_peak_max = 200
settlements = 6
settlements_standing = 6
settlements_thriving = 6
settlements_declining = 0
settlements_ruined = 0
settlements_faded = 0
settlements_hamlet = 6
settlements_village = 0
settlements_town = 0
settlements_city = 0
settlements_fortress = 0
settlements_unowned = 0
population = 1200
population_settlement_max = 200
census_rows = 6
regions = 50
regions_nonislet = 50
regions_held = 6
region_control_rows = 6
regions_ever_held = 6
share_max_permille = 20
share_max_civ = 1
share_max_year = 0
share_over_500_years = 0
share_over_600_years = 0
share_over_700_years = 0
share_over_800_years = 0
share_over_900_years = 0
relations = 0
relations_open = 0
wars = 0
wars_open = 0
```

## 17. Illustrative excerpt: a war, a sacking and a ruin (P3-06)

**Illustrative only**: the event kinds `region_claimed`, `war_declared`, `battle`, `sacked`, `civ_ended`, `peace`, their roles, values, saliences and templates, and the relation kinds `war` and `peace` are invented for this example and bind nothing; P3-05 and P3-06 define the real ones. The format is exact: the excerpt is cut from the prototype's report of a hand-made world (races `elf,goblin`, `civs_per_race` 2, 26 events, a flare-up 2500–2549), and it shows how the lines of section 6 read once P3-06 exists. From the `summary` report: `[phases]`, the block of the ended civilization, the chronicle of the goblins' opening phase and the flare-up; then, from the `full` report, the `[present]` lines that list settlements. Lines `...` stand for omitted lines, `(level full)` marks the switch to the `full` report, and the `full` report's 46 `  rg<id> <name>: free` lines are left out; none of these three is part of the format.

```
[phases]
p1 a1 opening 0-149, 150 years; trigger e1; 5 events
p2 a1 quiet 150-2073, 1924 years; 1 event
p3 a2 opening 2074-2223, 150 years; trigger e7; 12 events
p4 a2 quiet 2224-2499, 276 years; 2 events
p5 a2 flare_up 2500-2549, 50 years; trigger e20; 4 events
p6 a2 quiet 2550-5699, 3150 years; 2 events
p7 a2 final 5700-5999, 300 years; 0 events
[civilizations]
...
c2 The Skib Horde
  race: Goblin (ra2)
  home: Northeast Downs II (rg10), grassland
  founded: 2074 in The Age of Goblins (a2), e8
  capital: none
  peak regions: 1, 2074-2140
  peak population: 200 in 2074
  settlements: 1 founded, 0 held at the end, 1 ruined
  relations: 1 (war 1)
    war: Finen (c1), The Skib Horde (c2), 2098-2140, e12 to e17
  events: 8
  fate: ended in 2141, e17: The Skib Horde was no more.
  milestones:
    2074 e8 civ_founded: The Goblins founded The Skib Horde in Northeast Downs II.
    2098 e12 war_declared: The Skib Horde declared war on Finen.
    2104 e13 battle: The Skib Horde defeated Finen in North Heath; 310 fell.
    2105 e14 sacked: The Skib Horde sacked Aelith of Finen; 95 died and the town lay in ruins.
    2141 e16 sacked: Finen sacked Skrisligg Pit of The Skib Horde; 180 died and the town lay in ruins.
    2141 e17 civ_ended: The Skib Horde was no more.
...
[chronicle]
threshold = 20
events = 26
shown = 25
...
== a2 The Age of Goblins: 2074-5999
-- p3 opening: 2074-2223
2074 e7 awakening/100: Krok awakened the Goblins in Northeast Downs II. {@rg10 race=ra2 creator=g2}
2074 e8 civ_founded/80: The Goblins founded The Skib Horde in Northeast Downs II. {@rg10 civilization=c2 race=ra2} [causes e7]
2074 e9 settlement_founded/40: The Skib Horde founded Skrisligg Pit in Northeast Downs II. {@rg10 @s3 settlement=s3 founder=c2 population=200} [causes e8]
2074 e10 civ_founded/80: The Goblins founded The Gutrot Clans in Northeast Downs. {@rg7 civilization=c3 race=ra2} [causes e7]
2074 e11 settlement_founded/40: The Gutrot Clans founded Ghashpit in Northeast Downs. {@rg7 @s4 settlement=s4 founder=c3 population=200} [causes e10]
2098 e12 war_declared/70: The Skib Horde declared war on Finen. {attacker=c2 defender=c1} [causes e8]
2104 e13 battle/60: The Skib Horde defeated Finen in North Heath; 310 fell. {@rg13 winner=c2 loser=c1 dead=310} [causes e12]
2105 e14 sacked/85: The Skib Horde sacked Aelith of Finen; 95 died and the town lay in ruins. {@rg13 @s2 attacker=c2 owner=c1 dead=95} [causes e13]
2140 e15 battle/60: Finen defeated The Skib Horde in Northeast Downs II; 520 fell. {@rg10 winner=c1 loser=c2 dead=520} [causes e12, e14]
2141 e16 sacked/85: Finen sacked Skrisligg Pit of The Skib Horde; 180 died and the town lay in ruins. {@rg10 @s3 attacker=c1 owner=c2 dead=180} [causes e15]
2141 e17 civ_ended/90: The Skib Horde was no more. {@rg10 civilization=c2} [causes e16]
2141 e18 region_claimed/30: Finen claimed Northeast Downs II. {@rg10 civilization=c1} [causes e17]
-- p4 quiet: 2224-2499
2224-2499 e19 quiet_era/20: Finen passed 276 quiet years (2 generations), from 2224 to 2499. {@rg12 civilization=c1 years=276 generations=2}
2224 e20 region_claimed/30: The Gutrot Clans claimed Northeast Downs III. {@rg11 civilization=c3}
-- p5 flare_up: 2500-2549
2500 e21 flare_up/50: In 2500 tensions flared into 50 years of upheaval. {years=50} [causes e20]
2501 e22 war_declared/70: The Gutrot Clans declared war on Finen. {attacker=c3 defender=c1} [causes e20, e21]
2512 e23 battle/60: Finen defeated The Gutrot Clans in Northeast Downs III; 240 fell. {@rg11 winner=c1 loser=c3 dead=240} [causes e22]
2530 e24 peace/60: Finen and The Gutrot Clans made peace after 29 years of war. {parties=c1 parties=c3 years=29} [causes e23]
...
[present] (level full)
year: 6000
civilizations alive: 2 of 3
c1 Finen: 2 regions, 1 settlement, population 1450
c3 The Gutrot Clans: 2 regions, 1 settlement, population 610
regions: 50; held 4, free 46
  rg7 Northeast Downs: The Gutrot Clans (c3) since 2074
  rg10 Northeast Downs II: Finen (c1) since 2141
  rg11 Northeast Downs III: The Gutrot Clans (c3) since 2224
  rg12 Northeast Downs IV: Finen (c1) since 0
settlements: 4; thriving 2, declining 0, ruined 1, faded 1
  s1 Voeledell: town, thriving, population 1450, Finen (c1), at 755 293 in Northeast Downs IV (rg12), founded 0 by Finen (c1)
  s2 Aelith: village, faded, population 0, no owner, at 742 361 in North Heath (rg13), founded 61 by Finen (c1)
  s3 Skrisligg Pit: hamlet, ruined, population 0, no owner, at 808 251 in Northeast Downs II (rg10), founded 2074 by The Skib Horde (c2), ruined 2141, e16
  s4 Ghashpit: village, thriving, population 610, The Gutrot Clans (c3), at 836 140 in Northeast Downs (rg7), founded 2074 by The Gutrot Clans (c3)
settlement kinds: hamlet 1, village 2, town 1, city 0, fortress 0
largest settlement: Voeledell (s1), population 1450
```

## 18. Amendments requested

Status: 18.1–18.5 **applied 2026-11-19** on `p3/07-spec` (P3-04 revision r1, P0-02 amendment 20, P0-04 amendment 21, P3-00 correction note for P3-07, P3-01 revision r5). 18.6 is a note for the build engineer, 18.8 for the lead; 18.7 became section 20.

### 18.1 P3-04 (8.5, `HistoryView`; urgent: the engine is being implemented now)

1. `HistoryView::load` returns `db::Result<HistoryView>`: a corrupt world is an error value, code `corrupt_layer`, message `corrupt table '<table>': <detail>` (the engineer's wording, recorded in P3-04 16), never a `FatalError` (P0-02 9: the read API returns errors as values; a bad file is an input error, exit 3). `render_event` keeps its `FatalError` for an id outside the log (a caller's bug).
2. Read accessors used by P3-07 and P4 (names are the engineer's): the number of events; per event its row (`year`, `end_year`, `kind`, `phase`, `region`, `settlement`, `salience`), participants (`seq`, role seq, entity type, id) in `seq` order, values in `seq` order, causes ascending; the stored catalogue (kind key, role keys, value keys); the display name of an entity by type and id (8.3), absent for types 6–9.
3. `HistoryView::load` validates every stored template and every reference of the log, so that `render_event` on a loaded view fails only for an id outside the log. `HistoryView` and `render_event` accept every kind present in the world's catalogue tables, including kinds the running build does not compile (a world of a later generator of the same schema, or a test fixture): the world is self-describing (A1); the compiled catalogue is used only by the writer.
4. 11.1: one sentence: "Every key of `[history]` is also a key of the history report's `[stats]`, with the same definition (P3-07 9)." No behaviour changes.

### 18.2 P0-02 (errors)

`db::ErrorCode` gains `history_missing` (after `real_value`; `error_code_name` returns `"history_missing"`): the world has no history (P3-07 5.2). The CLI maps it to exit 3.

### 18.3 P0-04 (`report`)

Amendment 21: (a) synopsis `worldgen-cli report --db PATH [--history [--level summary|full]] [--out PATH] [--force] [--verbose]`; `--history` prints the history report, format `# worldgen history v1` (P3-07 6), instead of the world report; `--level` (default `summary`) needs `--history`; the usage errors, the no-history error (exit 3) and the help lines of P3-07 4. (b) Section 7's "Later" note is fulfilled by P3-07. (c) Section 10's invariant "`report` and `hash` output is byte-identical across platforms" covers `report --history`. (d) The exit code table's "input error" gains "a world without history given to `report --history`".

### 18.4 P3-00

1. 11: the exact format, ids and grammar are P3-07's; the output is ASCII (a subset of UTF-8); `[stats]` holds integers only, ratios as per mille (no decimals); the chronicle line is `<span> e<id> <kind>/<salience>: <sentence> {<refs>}[ [causes e<c1>, e<c2>, ...]]`, grouped under age and phase headings; the summary level is a budget on salience (P3-07 7.4), not a fixed threshold; the report reads no data and adds no `history.report` field (P3-07 3.2, 8).
2. 3.2: P3-07 changes no world byte and no data, so no version bump.
3. 12.4: P3-07's golden file is `p3-07-report.txt` (report text hashes of the history cases and of hand-made fixtures).

### 18.5 P3-01

Section 5, table row `report`: "P3-04 (the event salience and templates); P3-07 adds no field (P3-07 8)"; 5.3: delete "P3-07 adds its own fields (a data change)".

### 18.6 `tools/ci/world-hashes.sh` (build engineer)

For every case whose world has history (every case except `empty-*` and `pattern-*`), also save `<preset>-<case>.history.txt` = `report --db <file> --history --level full`, and make `compare` compare these files too (same failure rule and `diff -u` output). CLAUDE.md's description of the script follows (lead).

### 18.7 P3-05 and P3-06

Moved to section 20 (requirements on those specs, which are being written now).

### 18.8 CLAUDE.md and README (lead)

The golden file `p3-07-report.txt`, the CTest group `worldgen-tests-history-report`, `report --history` among the commands. No `docs/specs/schema.md` change: the schema does not change.

## 19. Open questions for Paul

Each with the recommendation and the interim choice this spec follows.

1. **Report constants, not tuning.** The summary budget (250 events), the minimum salience (20) and the milestone count (6) are constants of the report format, and `history.report` gains no field, because the report must read only the world and the world does not store tuning. The knob for "what matters" stays the per-kind base salience, which is data and is stored in every world. Recommendation and interim: yes. Alternative: store report settings in the world (a schema change) or add a `--min-salience N` option.
2. **Summary size.** At most 250 event lines, none below salience 20, the threshold rising with the size of the log (7.4), for a report of about 600 lines on a world with a dozen civilizations. Recommendation and interim: as stated; the critic may ask for a different budget after the first P3-06 sweep (a `v2` change, cheap while nothing archives reports).
3. **No filters in v1.** `--civ`, `--region` and year ranges are left to `grep` and `awk` over the `full` level, whose lines carry every id and year; following a civilization is P4's browser. Recommendation and interim: no filters now.
4. **A world without history is refused** (exit 3) rather than given a partial report. Recommendation and interim: refuse.
5. **Islet threshold in the report.** The runaway statistics need the islet rule, which the world does not store; the report uses the decided value 1172 as a constant. Recommendation and interim: keep it; if the tuning value is ever changed, store an islet flag in the world with that change (a schema change).
6. **Stdout by default**, as `report` (P0-04 question 2, approved). Recommendation and interim: yes.

## 20. Requirements on P3-05 and P3-06

Binding on those specs, which are being written in parallel with this one:

1. P3-06 names its war relation kind `war` (the `wars` and `wars_open` statistics of section 9 count that kind), and states for each relation kind whether it is symmetric (P3-04 6.1); the report prints relations in stored order either way.
2. Each spec appends its sweep statistics to section 9 by amendment (10.3 rule 1), each with a definition computable from the world's tables; a statistic that needs values not stored in the world (capacities, tension) is checked by that spec's own tests, not by the report.
3. Every event that ends a civilization or ruins a settlement is that row's `ending_event` or `ruin_event`, and its salience is at least 20 and high enough to rank among the civilization's milestones (7.3); the illustrative excerpt of 17 uses 85–90.
4. No new event kind may be needed by the report: a kind is printed from its stored template, roles and values (A1). A spec that wants a new block or line in the report asks for a `v2` (10.3).

## 21. Summary selection, war notes and the P3-09 statistics (amendment r8, P3-09, generator 0.18.0; before implementation)

Written 2026-11-20 by the spec writer for change P3-09 "conflict balance" (`docs/specs/p3-09-conflict-balance.md`), in answer to the world critic's review of P3-06 (`docs/reviews/p3-06-conflict-world-critic.md`, finding F4 and suggestions 2 and 8). P3-07 is not implemented yet, so this section is part of format v1 as it will first be implemented: no `v2` is needed and no report text exists that changes. **Where this section and sections 5–13 differ, this section rules**; the sections it replaces are named in each item. It changes no world byte (the report reads the world only); the salience change of claims (21.6) is a data change of P3-06's and belongs to generator 0.18.0.

### 21.1 Why the threshold alone is not enough (measured at 0.16.0)

The summary of 7.4 shows every event at or above one salience threshold. On the 0.16.0 worlds (1024, default settings, seeds 0–39, the spec writer's prototype `~/.cache/the-unmapped/out/p3-09-spec/proto.py`, which reproduces the 0.16.0 log) this has three defects the critic found:

1. **Ages vanish.** The thresholds of 26–46 drop the quiet early ages almost entirely: seed 42 shows 3, 8, 12, 28, 30 and 74 events in its six ages (the Age of Elves, 2,074 years, gets 3 lines).
2. **Wars lose their endings and their consequences.** A `truce` (50) or `region_ceded` (55) is below a threshold of 56, a `capital_moved` (50) below 51: with four civilizations per race the critic counted 12 truces, 4 cessions and 32 moved capitals hidden under shown declarations and sacks (seed 42, threshold 56).
3. **Repetition reads as a timer.** The same declaration sentence recurs with nothing to say that it is the fifth war between the same pair, or how old the claim is; the `flare_up` sentence names no one.

### 21.2 Units (new; used by 21.3)

The **unit** U(e) of an event e is e together with its **companions**, found from the stored log only:

| Kind of e | Companions |
| --- | --- |
| `war_declared` | the war's **ending**: the `until_event` of the `war` relation row whose `since_event` is e, when that row is closed (a `peace`, `truce`, `civ_conquered` or `civ_ended`); and every event of kind `region_ceded` or `tribute` that has that ending among its causes |
| `peace`, `truce` | every event of kind `region_ceded` or `tribute` that has e among its causes |
| `sacked`, `settlement_captured` | every event of kind `capital_moved`, `refugees_settled` or `civ_conquered` that has e among its causes |
| any other kind | none |

Companions are not followed further (a companion's own companions are not added, except as listed: the war's ending brings its cession or tribute). Kinds absent from a world's catalogue simply never occur. The kind keys are those of P3-06 11 and P3-06 20 (amendment r9 there: kinds 32 `refugees_settled` and 33 `tribute`, after P3-08's kind 31); the report finds them by key in the stored catalogue, never by id.

### 21.3 The summary set and its threshold (replaces 7.4; 6.7's "an event is shown when its `salience` ≥ T"; 13 item 2)

1. **Candidates**: the events with `salience` ≥ `SUMMARY_MIN_SALIENCE` (20), ranked by (`salience` descending, `id` ascending).
2. **Age floor.** For each age k in ascending id (its years `start_year`–`end_year` of `age`): walk the candidates whose `year` lies in age k in rank order; for each one not yet in the floor set, add its whole unit; stop when the floor set holds at least `SUMMARY_AGE_FLOOR` = 20 events whose `year` lies in age k, or when the age's candidates are exhausted. A unit may carry companions of later years into the floor (a war declared at the end of an age ends in the next); they count toward the age of their own year. The floor set is the union over the ages.
3. **Threshold.** C(t) = the union of U(e) over the candidates with `salience` ≥ t. **T** = the smallest t in [20, 100] with |floor ∪ C(t)| ≤ `SUMMARY_BUDGET` (250). The summary shows floor ∪ C(T). If no t qualifies, T = 100 and the summary shows floor ∪ C(100), which may exceed 250 lines (only when more than about 130 events have salience 100). The set grows as t falls, so T is well defined; no tie needs a rule (the floor walk is in rank order, the threshold a value).
4. The floor set holds at most (ages) × (20 + 3) events (a unit has at most four events), so with the six races of the design it never exceeds 138 and always leaves room under the budget.
5. **`[chronicle]`**: `threshold = <T>` as before; an event line is printed for each event of the summary set (in id order, under its phase); `shown` = the size of the set; the `(n more events not shown)` lines count the rest. At `full`, T stays 0 and every event is shown (unchanged).
6. **`[stats]`**: `summary_threshold` = T of item 3 (also at `full`), `summary_events` = the size of the summary set (both definitions of 9 replaced).
7. **Invariant 13 item 2** becomes: at `summary`, the event lines are exactly the summary set of item 3; every shown `war_declared` with a closed `war` row has its ending shown; every shown `sacked` or `settlement_captured` has its `capital_moved` and `refugees_settled` shown; each age with at least 20 candidates shows at least 20 events of its years; `shown` ≤ 250 unless T = 100.
8. Constants (section 8 gains one row): `SUMMARY_AGE_FLOOR` = 20, used in 21.3. It is part of format v1 like the others.

**Measured**: see 21.7.

### 21.4 Notes after a sentence (new; 6.7's event line, 6.1 "Sentences", 13 item 4)

Two kinds get a **note**, written by the report from the log after the rendered sentence and before the refs, at both levels:

```
<span> e<id> <kind key>/<salience>: <sentence> (<note>) {<refs>}[ [causes e<c1>, e<c2>, ...]]
```

The sentence is still `render_event` byte for byte (13 item 4 holds for the sentence part); the note is the report's own text, built from stored names, years and links, escaped like any other text (6.1), with names rendered as `render_event` renders a name in that position (a leading "The" lower-cased in the middle of a sentence and kept at its start, Paul's decision 2 on the region names, P3-04 8.5 as amended by P3-08). The parsing rule of 6.7 is unchanged (refs start at the last ` {`, which a note cannot contain: it holds names, digits, letters, spaces, `;` and `.` only).

1. **`war_declared`.** c = the event's cause of kind `claim` with the smallest id (every declaration has one: P3-06 14 item 2); y_c its `year`. n = 1 + the number of earlier `war_declared` events whose two civilization participants (`attacker`, `defender`) are the same two civilizations, in either role. Note: `Claimed since <y_c>.` when y_c < the event's year, `Claimed the same year.` when equal; when n ≥ 2, followed by ` The <ordinal(n)> war between them.` No cause of kind `claim` (a world of another generator): no note.
2. **`flare_up`.** c = the event's first cause. If c is of kind `claim`, with claimant A (role `claimant`), holder B (role `holder`) and region R (`event.region` of c): note `<A> pressed its claim to <R> against <B>.` with A at the start of a sentence and R, B in its middle. Otherwise no note.
3. `ordinal(n)`: `second`, `third`, `fourth`, `fifth`, `sixth`, `seventh`, `eighth`, `ninth`, `tenth` for n = 2–10; for n ≥ 11 the decimal number followed by `th` when n mod 100 is 11, 12 or 13, else `st`, `nd`, `rd` for n mod 10 = 1, 2, 3, else `th` (`11th`, `21st`, `22nd`, `23rd`, `101st`, `111th`).

Examples (seed 42, 1024, the 0.16.0 log and its numbered region names; from 0.17.0 the regions carry P1-06 r3's names):

```
2949 e93 flare_up/50: In 2949 tensions flared into 50 years of upheaval. (The Grork Horde pressed its claim to Southwest Wood against Finen.) {years=50} [causes e75]
2949 e94 war_declared/70: The Grork Horde declared war on Finen over Southwest Wood. (Claimed since 2574. The second war between them.) {@rg35 attacker=c3 defender=c1} [causes e44, e75]
```

**API** (5.1): `std::string history_note(const HistoryView&, std::int64_t event_id)` in `worldgen/history/digest.hpp` returns the note without its parentheses, or an empty string; the report and P4's browser call it, nobody re-implements it (as `render_event`, P3-00 6.3). It does not fail on a loaded view.

### 21.5 New `[stats]` keys (appended after `wars_attacker_<race key>`, 10.3 rule 1)

Integers only; "the war of d" for a `war_declared` event d is its `war` relation row (`since_event` = d) and the events reached from d: the `battle` and `battle_at_pass` events caused by d, the `siege` events caused by those, the `sacked` and `settlement_captured` events caused by those, and the row's `until_event` u with the `region_ceded` and `tribute` events caused by u.

| Key | Definition |
| --- | --- |
| `wars_hollow` | `war_declared` events whose war has no `sacked`, no `settlement_captured`, no `region_ceded`, no `tribute`, and whose u is not a `civ_conquered`: wars that changed nothing (an open war counts with what it has so far) |
| `wars_hollow_permille` | floor(1000 × `wars_hollow` / the number of `war_declared` events); 0 when there is none |
| `wars_same_goal_max` | the largest number of `war_declared` events with one `defender` and one `event.region`; 0 when there is none |
| `wars_same_goal_unchanged_max` | the largest number of consecutive `war_declared` events (in id order) with one `defender` and one region G such that no `region_control` row of G has `from_year` or `to_year` within the war's span [since, until] (until = E for an open row) for any of them: the longest run of wars over one goal that never moved it |
| `race_population_share_max_permille` | max over races of floor(1000 × `population_<race>` / `population`); 0 when `population` is 0 |
| `race_regions_share_max_permille` | max over races of floor(1000 × `regions_held_<race>` / `regions_held`); 0 when `regions_held` is 0 |

The validation sweep reads them (P3-06 15.6 as amended by P3-06 20.9).

### 21.6 Claims at salience 35 (a data change of P3-06's, generator 0.18.0)

`history.report`'s `claim` entry goes from 45 to **35** (P3-06 20.8), so that a claim, whose year is now in the declaration's note (21.4 item 1), is shown at summary level only in small logs; a claim that leads to no war stays visible there. Sections 16 and 17 are illustrative and predate P3-06; the sample of 16 is re-produced from the implementation and recorded by its golden case (14.4), as it already had to be.

### 21.7 Measured effect

`docs/specs/p3-09-conflict-balance.md` 5.7 gives the figures (the spec writer's prototype, which computes this selection from the log; 1024, default settings, seeds 0–39; the 0.16.0 rules and the expected 0.18.0 rules): the median world shows 223 events (142 at least, 248 at most) at a median threshold of 31; its six ages show a median of 19.5, 20, 26, 27.5, 30.5 and 83 events, against 5, 16.5, 27.5, 28.5, 31 and 89 under 7.4's threshold alone; seed 42's first three ages go from 3, 8 and 12 events to 20 each. The companions add nothing at default settings (no shown war lost its ending there) and matter when the threshold rises above 50, as with four civilizations per race (P3-09 5.4).

### 21.8 War endings and searches (amendment r9, P3-09 r3, generator 0.18.0)

P3-06 20.15–20.22 (r10 there) add kinds 34 `war_won` (a war won where the loser did not sue for peace), 35 `search_abandoned` (a war against a hidden people that found no one) and 36 `refuge_held` (a last refuge that held for want of a place to flee). This section amends 21 for them; nothing else changes.

1. **Units** (21.2): the row "`peace`, `truce`" reads "`peace`, `war_won`, `truce`". A declaration's ending (the `war` row's `until_event`) may be any of `peace`, `war_won`, `truce`, `search_abandoned`, `civ_conquered`, `civ_ended`, and is its companion in every case. Wherever 21 says "the peace" of a war, `war_won` is meant too: 21.5's "u", and the cession or tribute caused by it.
2. **`wars_hollow`** (21.5) excludes the wars whose u is a `search_abandoned`. New key **`wars_unfound`**, appended after `race_regions_share_max_permille`, counts them. `wars_hollow_permille` = floor(1000 × `wars_hollow` / (the number of `war_declared` events − `wars_unfound`)): a war that never found its enemy is neither hollow nor a war that changed something, and it is counted apart.
3. New key **`refuges_held_no_destination`**, appended after `wars_unfound`: events of kind `refuge_held` with `reason` 1. Validation checks each with the reference (P3-06 20.22).
4. Invariant 13 item 7 of 21.3: "every shown `war_declared` with a closed `war` row has its ending shown" covers the new endings.
5. (r10) **`wars_same_goal_unchanged_max`** (21.5) is redefined to follow the weariness rule (P3-06 20.4, 20.23): any change of the region's owner breaks the run, also one between two wars. For one defender D and region G, take the `war_declared` events with defender D and region G in id order. A run is a sequence of consecutive such wars w_i … w_j such that no `region_control` row of G has `from_year` or `to_year` in [since(w_i), until(w_j)] (until = E for an open row). The key is the length of the longest run over all (D, G); 0 when there is none. This replaces 21.5's definition, which checked each war's own span only and missed a change of owner between two wars.
6. (r10) **Events by kind**: `[stats]` prints `events_war_won`, `events_search_abandoned` and `events_refuge_held` after `events_tribute`, by kind id (9: one line per catalogue kind). They need no rule here. The war line words of the conflict golden (`won-c<W>`, `-ceded`, `-tribute`, `truce`, `unfound`, P3-06 20.26) are the reference's, not a report line: the report prints wars as `war` relation rows (6.6) and their endings as event lines.
7. (r10) **The summary's companions** (21.2 with item 1 above) name every ending kind: a shown `war_declared` brings its `peace`, `war_won`, `truce`, `search_abandoned`, `civ_conquered` or `civ_ended`; a `peace` or `war_won` brings its `region_ceded` or `tribute`. A `refuge_held` (salience 25) has no companions and is no companion: it is the siege of a refuge, and a siege is shown only at its own salience.

## 22. Quiet stretches, outbreaks and wording (amendment r11, P3-10, generator 0.19.0; before implementation)

Normative for the change P3-10 (`docs/specs/p3-10-followups.md`). P3-07 is not implemented; its implementer follows this section with 21 from the start. Format v1 is unchanged: every line keeps its grammar.

1. **Kinds 37 and 38.** `[stats]` prints `events_plague_outbreak` and `events_quiet_stretch` after `events_refuge_held`, by kind id (9: one line per catalogue kind); `events_plague` and `events_quiet_era` are 0 in a 0.19.0 world and are still printed. Wherever a P3-07 measure or a validation statistic counts plagues (the plague share of the summary, P3-05 15.4, P3-08 6.5), it counts kinds 12 and 37 together, so worlds of 0.18.0 and 0.19.0 compare. 21.2's units and companions are unchanged (neither kind is in a unit).
2. **Generations in a civilization's milestones** (6.6). A milestone line of civilization c whose event is a `quiet_stretch` gets a note in the form of 21.4: `<C>: no new generation of rulers.`, `<C>: one new generation of rulers.` or `<C>: <n> new generations of rulers.`, with C the civilization's name (at a sentence start) and n = g(`end_year` + 1) − g(`year`), g(t) = (t − c.`founded_year`) div the race's `generation_years` (P3-04 6.6's count, which `quiet_era` stored as `generations` before 0.19.0). The chronicle's lines of `quiet_stretch` have no note (the stretch is the world's, not one realm's). `history_note` gains an optional civilization argument for this note (0 at the chronicle: no note), so P4's browser can show it in a realm's page; otherwise its contract is 21.4's.
3. **The flare-up note** (21.4 item 2) reads `The claim of <A> to <R>, held by <B>, came to a head.` in place of `<A> pressed its claim to <R> against <B>.`: "its" does not agree with a realm named in the plural (`The Warrens of Zeez`), and the new wording starts with a fixed word, so A, R and B are all mid-sentence (a leading `The` of each lower-cased). It is the one place where a flare-up's parties are named (P3-04 20.5): the `flare_up` sentence stays generic. Example: `In 2949 tensions flared into 50 years of upheaval. (The claim of the Grork Horde to Southwest Wood, held by Finen, came to a head.)`
4. **Numbers** inside a sentence carry P3-04 20.3's separators; the report's own numbers (years in notes, `[stats]` values, refs such as `{years=1924}`, census and peak figures) do not: they are machine-readable integers (6.1).
5. **Samples.** Every sentence quoted in sections 6, 15, 17 and 21 that shows a quiet era, a plague, a battle with "0 of the victors", a moved capital or a secession was rendered with the templates of 0.14.0 to 0.18.0 and is illustrative for 0.19.0.
6. **Tests** (amends 14): on fixture H1, a milestone note for a `quiet_stretch` with n = 0, 1 and 12; no note on its chronicle line; the flare-up note with a `The …` claimant at the third position (lower-cased); `events_plague_outbreak` and `events_quiet_stretch` lines in id order.

## Appendix A. Proposals

No proposals entry is needed: the report realizes design 13's "text history report" within design 6's rule (text from records and templates) and does not change the design. Its relation to design 14's chronicles (a deterministic template report for checking the history, not prose for players) is stated in section 1.

## Revision note r1 (2026-11-19, amendments for P3-05)

Made with P3-05 (its 18.5), in place: section 9 gains seven keys after `wars_open` (10.3 rule 1; format stays v1). The world report's `[history]` gains `population` and the three per-race families in the same order (P3-05 15.4), so invariant 6 (every `[history]` key in `[stats]` with the same value) still holds.

## Revision note r2 (2026-11-19, ASCII message texts, after the P3-04 rulings)

Text only, no behaviour change. Messages and output formats are part of the cross-platform contract and are ASCII: the corrupt-data reasons of 5.2 print `<E - 1>` (was written with a minus sign), the relation line of 7 `<until - 1>`, and the chronicle line quoted in 18.4 item 1 matches 6's grammar (`[ [causes e<c1>, e<c2>, ...]]`). The placeholders always printed a number; the change is to the spec text. Checked against the P3-04 rulings of 2026-11-19 (P3-04 r3): nothing else in this spec contradicts them (the `flare_up` example of 14 is a point event with a cause, as ruled).

## Revision note r3 (2026-11-19, amendments for P3-06)

Made with P3-06 (its 18.4), in place: section 9 gains nine keys and one family of six per-race lines after `civilizations_seceded` (10.3 rule 1; format stays v1): `relations_enmity`, `relations_peace`, `war_years_total`, `war_years_max`, `civilizations_conquered`, `settlements_sacked`, `settlements_captured`, `regions_conquered`, `races_alive` and the six `wars_attacker_<race key>` lines (one per `race` row). They are defined over kinds and relation kinds that exist from generator 0.16.0; on an earlier world they print 0 (and `races_alive` the races with a living civilization). Section 20 item 1 is met by P3-06 4.3: `war` is asymmetric (a the attacker), `enmity` and `peace` symmetric. The illustrative excerpt of 17 stays illustrative: P3-06's kinds are `claim`, `war_declared`, `battle`, `siege`, `sacked`, `peace`, `truce`, `civ_conquered` and others (P3-06 11), not `region_claimed` or `civ_ended` by conquest. No world byte changes.

## Revision note r4 (2026-11-19, P3-04 r6)

5.2 item 2 notes P3-04 r6's bound on phase `end_year` (audit A3), which the shared phase definitions of `[stats]` rely on. The corrupt-data list of this spec is otherwise unchanged and agrees with P3-04 11.1 (it delegates the `phase`, `event` and `event_cause` checks to it). No world byte or report text of a valid world changes.

## Revision note r5 (2026-11-19, faded rows)

The illustrative excerpt of 17 is corrected: a faded settlement's `ruined_year` and `ruin_event` are cleared on fading (P3-04 9.2: they hold only while the row is ruined; P3-05 r3), so its `[present]` line carries no `, ruined <year>, e<event>` part. The format is unchanged.

## Revision note r6 (2026-11-19, same-year relation rows)

6.6: a closed relation row with `until` = `since` (opened and closed before one bookkeeping, P3-04 6.7 r9; reachable from P3-06 on) prints its span as `<since>-<since>`. Format stays v1: no world before 0.16.0 has such a row.

## Revision note r7 (2026-11-20, names inside sentences; for P3-08)

Text only (P1-06 r3 14.7, Appendix B.2; P3-04 r10 8.5). 6.1 "Sentences": the report prints `render_event`'s text unchanged, so a `The …` name mid-sentence is lower-cased there and nowhere else; every name printed as a field keeps its stored capital. 13 invariant 4 names the case. Format stays v1; no world byte changes. The golden report hashes (14.4) are recorded with P3-08's worlds and renderer (generator 0.17.0). The sample of 16 and the excerpt of 17 predate P3-04 r10 and P1-06 r3 (their region names and some sentences differ from what 0.17.0 prints); section 14.4 already makes section 16 equal the output of the generator current when P3-07 is implemented, so the sample is re-made then; this revision changes neither section.

## Revision note r8 (2026-11-20, summary selection and war notes; for P3-09)

New section 21 (P3-09, `docs/specs/p3-09-conflict-balance.md`, decided by Paul 2026-11-20; the P3-06 world critic's F4 and suggestions 2 and 8), before implementation, so format v1 as first implemented: the summary set is the per-age floor of 20 events plus the threshold set, each event with its companions (a declaration with its ending and the ending's cession or tribute, a sack or capture with the moved capital, the refugees and the conquest it caused), replacing 7.4's threshold-only rule; a note in parentheses after the sentence of `war_declared` (claim year, ordinal of the war between the pair) and of `flare_up` (claimant, region, holder), from the library function `history_note`; six `[stats]` keys appended (hollow wars, wars over one goal, race shares); constant `SUMMARY_AGE_FLOOR` = 20. The claim salience 35 is P3-06's data (r8 there). No world byte changes by this section.

## Revision note r9 (2026-11-20, war endings and searches; for P3-09 r3)

New 21.8: kinds 34 `war_won`, 35 `search_abandoned` and 36 `refuge_held` (P3-06 r10) in the summary units and the statistics; `wars_hollow_permille` counted over the wars that found their enemy; two keys, `wars_unfound` and `refuges_held_no_destination`. No world byte changes by this section.

## Revision note r10 (2026-11-20, text after the P3-09 integration)

21.8 items 5–7: `wars_same_goal_unchanged_max` breaks a run at any change of the region's owner, also between two wars (the lead's ruling); the `events_<key>` lines of kinds 34–36; the companions name every ending kind, and `refuge_held` is neither a head nor a companion. No world byte changes.

## Revision note r11 (2026-11-20, quiet stretches, outbreaks and wording; for P3-10)

New section 22 (`docs/specs/p3-10-followups.md`, generator 0.19.0), before implementation: the report keys of kinds 37 and 38 and plague counts over kinds 12 and 37 (22.1); the generations note on a civilization's `quiet_stretch` milestone (22.2); the flare-up note reworded so that it agrees with plural realm names and stays the one place for the flare-up's parties (22.3); separators in sentences only (22.4); samples illustrative (22.5); tests (22.6). No earlier section is renumbered.
