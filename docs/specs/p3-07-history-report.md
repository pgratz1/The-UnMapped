# P3-07 History report

Status: **approved by the lead 2026-11-19** (r12, 2026-11-20: section 22 for P3-10, generator 0.19.0; r14, 2026-11-21: section 23 for P3-11, generator 0.20.0; r16, 2026-11-22: text after the implementation and audit; r17, 2026-11-22: section 24, readability after the world critic's review; r18, 2026-11-22: section 24's text after the implementation and the reference; r19, 2026-11-22: section 25, format v2, for P5-01, generator 0.21.0; r20, 2026-11-22: section 25's text after the P5-01 integration; r21, 2026-11-22: section 26, format v3, renamed regions and hidden refuges, for P5-12, generator 0.22.0) (the six questions of section 19 go to Paul; the interim choices stand). Written by the spec writer 2026-11-19; the amendments of 18.1–18.5 applied the same day on `p3/07-spec`. Phase P3 History core, spec 7 (P3-00 3.1): the text history report, `worldgen-cli report --db PATH --history`. It reads a world file only and changes no world byte, so it needs **no generator version bump and no schema change**. Design doc sections 5 (history: the log is the primary product), 6 (the database stores facts and names; text is rendered from records and templates), 13 (P3 delivers "a text history report"; milestone 1: "the event log reads as coherent history") and 14 (the enrichment pass narrates from the log: this report is its deterministic, template-only counterpart and is not a chronicle in the sense of 14; it is a tool for the world critic, the validation sweeps and Paul). Depends on P0-02 (reader, errors as values), P0-04 (`report`: options, `--out`, `--force`, exit codes, text conventions, the "Later" note), P3-00 (binding: 11, 6.2–6.4, 12.2, 12.3, 13), P3-03 (tables `race`, `god`, `age`; `[chronology]` and its checks, 9.1), P3-04 (binding: schema 7 tables and codes of 9; the catalogue, `render_event` and `HistoryView` of 8, used and not redefined; the world report's `[history]` and its checks, 11.1; the queries of 10). Read on `origin/p3/04-spec` at 4a00615 (the newest text of every spec; the code there is P3-03's). Proposals read: up to "History: active phases scale with the history span; the detail level sets the simulated share". Revision r1 (P3-05's appended `[stats]` keys) 2026-11-19.

The sample report that section 16 held until r15 and the illustrative excerpt (17) were produced by the spec writer's exact-integer prototype `~/.cache/the-unmapped/out/p3-07-spec/history_report.py`, run on worlds built by `build_world.py` (the P3-04 prototype's stub world for seed 42, 1024, written as SQLite tables) and `illus.py` (a hand-made world). The prototype is scratch; the test writer's reference is written from this spec. (r16: section 16 now holds an excerpt of a 0.20.0 report of the implementation.)

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
| Tests | `worldgen/tests/history_report_*.cpp` (new CTest group `worldgen-tests-history-report`, pattern `*history_report_*`, and its slow entry `worldgen-tests-slow-history-report`), `tools/worldgen-cli/tests/cli_history_report_test.cpp`, fixtures `worldgen/tests/data/history-report/`; reference `worldgen/tests/reference/p3_report_reference.py`; golden `worldgen/tests/golden/p3-07-report.txt` |

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

**Usage errors** (exit 2, P0-04 4.1 form, with the hint line `worldgen-cli: run 'worldgen-cli report --help' for usage`): `--level needs --history` (a `--level` without `--history`; checked after tokenizing, before any file is opened); `invalid --level '<text>'; expected summary or full`. `--history` is a flag (no value). (r16, as implemented in `tools/worldgen-cli/cli.cpp`, all exit 2 with the hint line: `option '--level' needs a value` when `--level` is last; `--history foo` gives `unexpected argument 'foo' (report takes no positional arguments)`; `--history=x` and `--level=x` give `unknown option '--history=x' (write the value as the next argument: --name value)`; a repeated option gives `option '--level' given more than once`. These are P0-04's generic tokenizer messages.) `report --help` lists both options:

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
4a. (r16) **The two reads agree**: `read_history_tables` and `HistoryView::load` are separate reads of the file; the report checks that they saw the same events (5.3's table of checks as implemented, last rows) before indexing one by the other.
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
| `relation` | `a_type <t>`, `b_type <t>` (not 1–9); `a <prefix><id> does not exist`, `b <prefix><id> does not exist` (types 1–5 only; prefixes of 6.1); `since <y> outside 0 to <E - 1>`; `until and until_event disagree` (NULL exactly when 0); `until <u> outside <since> to <E - 1>` (r16: `until` = `since` is valid, a relation opened and closed within one year, P3-04 6.7 r9, printed `<since>-<since>` by 6.6 r6; r15 and earlier said `<since + 1>`, which contradicted 6.6); `since_event <e> is not an event`; `until_event <e> is not an event` (when ≠ 0) |

These are the conditions without which a computation of section 7 would be undefined or two blocks of the report would contradict each other; they are not a full validation of the log (P3-04's invariants are tested on the generator). An "is not an event" check is `1 ≤ id ≤` the number of `event` rows (P3-04 11.1 has made the ids consecutive).

**Checks as implemented, beyond the table** (r16; `worldgen/src/history/digest_tables.cpp`, `worldgen/src/inspect/history_report.cpp`). Every one is `db::ErrorCode::corrupt_layer` (exit 3) unless stated, with the message `corrupt table '<t>': …` as above:

| Where | Condition | Message |
| --- | --- | --- |
| row shapes | the tables `region` (28 columns), `biome` (5) and `god` (3) are read by this spec's reader and get P3-03 9.1's shape messages; the other tables come through the world report's reader with its messages | `corrupt table '<t>': row <key>: has <k> columns, expected <n>`, `… column <c> is not an integer`, `… column <c> is not text` |
| `settlement`, after `population <p> is negative` | population above 2^31 − 1 (7.6's bound, now enforced) | `corrupt table 'settlement': row <id>: population <p> above 2147483647` |
| `settlement_census`, after `population <p> is negative` | the same | `corrupt table 'settlement_census': row (<settlement>, <year>): population <p> above 2147483647` |
| `god`, after `relation` | `camp` not 0, 1 or 2 | `corrupt table 'god': row <id>: camp <c>` |
| `race`, after `god` | `creator_god` is not a `god` row | `corrupt table 'race': row <id>: creator_god <g> is not a god` |
| after `HistoryView::load` (5.2 item 4a, new) | the two reads disagree: the number of `event` rows seen by `read_history_tables` differs from `HistoryView::load`'s, or an event's `phase` is not a phase row of the first read (the file changed between the two reads; audit A2) | `corrupt table 'event': the file changed while it was read` |
| out of memory | in `read_history_tables` or while building | `db::ErrorCode::out_of_memory` (exit 1): `out of memory while reading the history tables`, `out of memory while building the history report` |

The order of the 5.3 checks is the table's: `region`, `phase`, `event`, `civilization`, `settlement`, `settlement_census`, `region_control`, `relation`, then `god` and `race`. (r17, as implemented at `origin/p3/07-impl` 544fbcf) Three notes on order and wording:
- **A standing settlement's population is checked first.** A bad population of a **standing** settlement (status 1 or 2) is found by P3-04 11.1's check, which runs in 5.2 item 2, before every check of 5.3. So it is reported before any problem in 5.3's table order. Its message in the history report is still 5.3's, `corrupt table 'settlement': row <id>: population <p> is negative` or `… above 2147483647`, translated from 11.1's wording by `history_stats_message` (`worldgen/src/history/digest_tables.cpp`). A settlement that is not standing is checked in 5.3's order.
- **An event whose `year` lies in no phase** is refused with `phase <p>, expected 0`, even when its stored phase is 0. It cannot arise in a world that passes P3-04 11.1.
- **A phase whose `start_year` lies in no age** is refused with `age <a>, expected 0`. It cannot arise once P3-03 9.1 has passed.

**No overflow for any admitted value** (r16; the audit's S1 and A1, no new check). The `, upland` flag of 6.6 is computed as `walk_cells` < ⌈`cells` / 4⌉, which equals 4 × `walk_cells` < `cells` with no product, so it cannot overflow for any `cells` ≥ 1 and 0 ≤ `walk_cells` ≤ `cells` that 5.3 admits. Every per-mille value of sections 7 and 9 is floor(1000 × n / d) with the product formed in 128 bits (`mul_u64_wide`, `divmod_u128_u64`), 0 when n ≤ 0 or d ≤ 0, and saturated at 2^63 − 1 if the quotient exceeded it, so no admitted value overflows it. The population sums stay below 2^63 because each population is at most 2^31 − 1 (above): only more than 2^32 settlement rows could exceed it, far beyond 7.6's 1,000,000.

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

(r17: the order and `[phases]` at `full` only are 24.4's.) The seven blocks of P3-00 11, always all seven, always in this order, each opened by its heading line. A block with nothing to list still has its heading and the lines that are always printed (6.3–6.9).

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
    <span> e<id> <kind key>: <sentence>[ (<note>)]  (one per milestone, 7.3)
```

- `home`: the region `home_region`, its `biome` column's `biome.key`, and `, upland` when 4 × `walk_cells` < `cells` (the upland rule of P3-00 7, so the critic can check "dwarves settle uplands, faeries wild places" from the report alone).
- `founded`: the age whose years contain `founded_year`.
- `capital`: `civilization.capital` when ≠ 0 (its end state), else `none`.
- `peak regions`, `peak population`: 7.1, 7.2.
- `settlements`: f = settlements with `founder_civ` = c; h = standing settlements with `owner_civ` = c; r = settlements with `founder_civ` = c and status 3 or 4.
- `relations`: R = the `relation` rows with (`a_type` = 3 and `a_id` = c) or (`b_type` = 3 and `b_id` = c). The count line lists each kind present in R once, in ascending byte order of the kind, with its count, separated by `, `. Then one line per row of R, ordered by (`since`, `a_type`, `a_id`, `b_type`, `b_id`, `kind`) ascending: a closed row `    <kind>: <a ref>, <b ref>, <since>-<until - 1>, e<since_event> to e<until_event>` (when `until` = `since`, a relation that began and ended within one year, P3-04 6.7 r9, the span prints `<since>-<since>`, not `<since>-<since - 1>`; r6), an open row (`until` NULL) `    <kind>: <a ref>, <b ref>, since <since>, e<since_event>`. Both entities are printed in stored order (a, then b), whichever is c, so a symmetric relation reads the same in both civilizations' blocks; `until - 1` is the last year of the relation (P3-04 10: in force for `since` ≤ Y < `until`).
- `events`: the number of events with at least one participant of type 3 and id c.
- `fate`: `alive in` when `ended_year` is NULL: regions = rows of `region_control` with `civ` = c and `to_year` NULL; h as above; p = Σ `population` of the standing settlements with `owner_civ` = c. Otherwise `ended in`, with the sentence of the ending event.
- `milestones:` is always printed; its lines (7.3) are the short form of a chronicle line: span, id, kind key and sentence, without salience, references or causes. (r16, as implemented) A milestone line carries the note of `history_note(view, e, c)`, c the block's civilization, in 21.4's form: `    <span> e<id> <kind key>: <sentence> (<note>)` when the note is not empty. So a declaration and a flare-up keep their notes of 21.4 and 23 (they do not depend on c), and a `quiet_stretch` gets 22.2's generations note when c is one of its participants. The chronicle passes 0 and prints no generations note.

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

(r17: replaced by 24.5's milestones.)


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
| `MILESTONES` | 6 (r17: **8**, 24.9) | 7.3, 24.5 |
| `MILESTONE_KIND_MAX` | 2 (r17) | 24.5 |
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
| `regions_conquered` | `region_control` rows whose `event` is of kind `settlement_captured` (26), `region_ceded` (29) or `settlements_ceded` (40) (r16: kinds 29 and 40 are counted together as cessions, 23 item 1; a foothold cession opens no `region_control` row, P3-06 22.5, so kind 40 adds nothing in a generated world and the rule only keeps the two cession kinds together) |
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

(r17: items 2, 3 and 5 are restated in 24.10.)


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
4. 7.1: two runs at the same peak (the first is printed); a peak of one year; a region held until E − 1 (open row). 7.2: two census years with the same peak (the earlier); a settlement changing owner between two civilizations at a census year; ruined and faded settlements not counted. 7.3: ties of salience broken by id; fewer than six events. 7.4: the 1,000 notes of H4 (salience (37 × i) mod 101, i = 1 … 1000) alone have n(76) = 248 ≤ 250 < n(75), so T would be 76 for the notes alone; (r18) with the fixture's other events and the quiet stretch that 24.3 item 2 always shows (24.11 item 4 asks for one in H4), |floor ∪ C(76)| = 251 > 250, so **T = 77** (the test counts all events of its fixture by hand); H5's 300 events of salience 100 give T = 100 and at least 300 lines. 7.5: islets excluded from N and from h; a civilization above 800 per mille for exactly 1,000 and then 1,001 consecutive years; a handover from one dominant civilization to another in the same year (two runs, not one); `share_max_*` ties in year and in civilization.
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

`worldgen/tests/golden/p3-07-report.txt`, written by `python3 -B worldgen/tests/reference/p3_report_reference.py golden`, LF, never edited by hand (r16: the order as the reference writes it). Six header lines starting `# ` (the generator version, the provenance, the three line forms, the order). Then the cases in ascending byte order of their names: the world cases of `world-hashes.sh` with history (`geo-phrase-512`, `geo-seed-0-512`, `geo-seed-1-1024`, `geo-seed-42-512`, `hist-dense-1024`, `hist-races-elf-orc-1024`, `hist-span-2000-512`, generated with the checkout's `game/data`), the hand-made fixtures (`h1-war-and-ruin`, `h2-minimal`, `h3-runaway`, `h3b-runaway-1000`, `h3c-share-ties`, `h4-budget`, `h5-saturated`, `h6-notes`, `h7-runs`) and `sample-42-1024` (`generate --seed 42 --map-size 1024`, default settings). Per case, in this order:

1. `<case> full <sha256>` and then `<case> summary <sha256>`, the SHA-256 of `report --history --level <level>` (`sample-42-1024`: the `summary` line only);
2. for a world case (not the sample): `<case> stats <key> <value>`, one per `[stats]` line in block order;
3. for a fixture: `<case> table <name> <hex>`, the table lines of `hash --detail` of the reference's copy of the fixture.

The sample's line is the reference's report of the 0.20.0 world; section 16 is an excerpt for reading and is not compared. The 1024 cases run in a `*-slow` suite (r18: CTest entry `worldgen-tests-slow-history-report`). The world cases' lines are re-recorded by running the reference whenever a merge changes those worlds, like the other world goldens; never to make a C++ test pass.

### 14.5 Property and cross-platform tests

14. Over the P3-04 sweep worlds (1024 seeds 0–49, 512 seeds 0–19, and the span and subset variants of P3-00 12.2) and the fixtures: invariants 1–8; the reference's text equals the C++ text for every world (the validation specialist runs the reference over the sweep).
15. A synthetic large world (30,000 events of a test kind with salience spread over 0–100, 2,000 settlements, 20,000 census rows, 500 `region_control` rows, 200 relations) for the timing of section 11 (tagged suite) and invariant 2.
16. Cross-compiler: `world-hashes.sh` saves the history report of every case with history at both levels and `compare` includes both (amendment 18.6, r16).

## 15. Viewer impact

**None.** The viewer neither runs nor parses the report; `gdext-headless-check` reads the world report only. P4's browser may call `digest_history` (5.1) for a civilization's peaks and fate, and must call `render_event` for sentences (P3-00 6.3), so that the viewer and the report never disagree.

## 16. Sample excerpt: seed 0, 1024, default settings, level `summary`, generator 0.20.0

(r17: illustrative; rendered by r16's rules, see 24.12.)


(r16) Until r15 this section held a full report of generator 0.14.0's stub world, which every later change had made illustrative (21.6, 22.5). It is replaced by an excerpt of a current report: `worldgen-cli generate --seed 0 --map-size 1024` at generator 0.20.0, then `worldgen-cli report --db <file> --history` (the engineer's sample, `~/.cache/the-unmapped/out/p3-07-impl/samples/`, byte-identical on `linux-gcc-release` and `linux-clang-debug`; 595 lines). Lines are copied unchanged; a line `...` marks omitted lines and is not part of the format. The excerpt is for reading and is not compared by any test: the golden value of a whole report is `sample-42-1024`'s line in `worldgen/tests/golden/p3-07-report.txt` (14.4), and the reference's text of it is the record.

```
# worldgen history v1
level = summary
[settings]
seed = 0
...
generator_version = 0.20.0
[ages]
a1 The Age of Elves: 0-2067, 2068 years; Elves (ra1) made by Saene (g1, good); 1 civilization founded
a2 The Age of Goblins: 2068-3264, 1197 years; Goblins (ra2) made by Kruskux (g2, evil); 1 civilization founded
a3 The Age of Dwarves: 3265-4177, 913 years; Dwarves (ra3) made by Nurnthaimm (g3, neutral); 2 civilizations founded
a4 The Age of Faeries: 4178-5002, 825 years; Faeries (ra4) made by Whaia (g4, good); 1 civilization founded
a5 The Age of Humans: 5003-5533, 531 years; Humans (ra5) made by Steldhil (g5, neutral); 2 civilizations founded
a6 The Age of Orcs: 5534-5999, 466 years; Orcs (ra6) made by Uruld'kruum (g6, evil); 1 civilization founded
[phases]
p1 a1 opening 0-149, 150 years; trigger e1; 3 events
...
p17 a6 final 5700-5999, 300 years; 90 events
[civilizations]
c1 The House of Nife
  race: Elf (ra1)
  home: Dourlaw Hurst (rg3), temperate_forest
  founded: 0 in The Age of Elves (a1), e2
  capital: Iensath (s1)
  peak regions: 6, 5563-5999
  peak population: 31549 in 5040
  settlements: 13 founded, 22 held at the end, 2 ruined
  relations: 14 (enmity 2, peace 6, war 6)
...
  fate: alive in 6000: 6 regions, 22 settlements, population 22908
  milestones:
    0 e2 civ_founded: The Elves founded the House of Nife in Dourlaw Hurst.
    4069 e130 sacked: The House of Nife sacked Mielae of the Baskeeb Mob; 474 died and 1,107 fled.
    5020 e205 sacked: The House of Nife sacked Asvathdell of the Baskeeb Mob; 198 died and 464 fled.
    5027 e209 sacked: The House of Nife sacked Nub Pit of the Baskeeb Mob; 316 died and 739 fled.
    5040 e220 sacked: The House of Nife sacked Zot-Sleeg Pit of the Baskeeb Mob; 441 died and 1,031 fled.
...
[chronicle]
threshold = 26
events = 438
shown = 166
== a1 The Age of Elves: 0-2067
-- p1 opening: 0-149
0 e1 awakening/100: Saene awakened the Elves in Dourlaw Hurst. {@rg3 race=ra1 creator=g1}
0 e2 civ_founded/80: The Elves founded the House of Nife in Dourlaw Hurst. {@rg3 civilization=c1 race=ra1} [causes e1]
0 e3 settlement_founded/40: The House of Nife founded Iensath in Dourlaw Hurst. {@rg3 @s1 settlement=s1 founder=c1 population=300} [causes e2]
-- p2 quiet: 150-2067
150-2067 e4 quiet_stretch/20: For 1,918 years, from 150 to 2067, the land was quiet; one realm held it. {realm=c1 years=1918 realms=1}
...
2893 e50 flare_up/50: In 2893 tensions flared into 50 years of upheaval. (The claim of the Baskeeb Mob to the Holt of Yews, held by the House of Nife, came to a head.) {years=50} [causes e25]
2893 e51 war_declared/70: The Baskeeb Mob declared war on the House of Nife over the Holt of Yews. (Claimed since 2180.) {@rg11 attacker=c2 defender=c1} [causes e25]
...
2897 e58 peace/60: The House of Nife sued the Baskeeb Mob for peace after four years of war. {winner=c2 loser=c1 years=4} [causes e57]
...
[present]
year: 6000
civilizations alive: 8 of 8
...
c8 The Krurz Horde: 1 region, 4 settlements, population 3530
regions: 56; held 23, free 33
settlements: 78; thriving 59, declining 7, ruined 11, faded 1
settlement kinds: hamlet 4, village 30, town 27, city 5, fortress 12
largest settlement: Ashealford (s57), population 21382
[stats]
end_year = 6000
...
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

For every case whose world has history (every case except `empty-*` and `pattern-*`), also save the history report at **both levels**: `<preset>-<case>.history.summary.txt` = `report --db <file> --history --level summary` and `<preset>-<case>.history.full.txt` = `report --db <file> --history --level full`. `compare` compares both files of every such case byte for byte across the presets, with the same failure rule (a difference or a missing file fails) and a `diff -u` of at most `DIFF_LINES` lines; a report that fails to generate fails `generate` (r16, as built on `p3/07-ci`; r15 and earlier named one file at `full`). CLAUDE.md's description of the script follows (lead).

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

(r17: amended by 24.2: the war frame and the cause of a consequence.)


The **unit** U(e) of an event e is e together with its **companions**, found from the stored log only:

| Kind of e | Companions |
| --- | --- |
| `war_declared` | the war's **ending**: the `until_event` of the `war` relation row whose `since_event` is e, when that row is closed (a `peace`, `truce`, `civ_conquered` or `civ_ended`); and every event of kind `region_ceded` or `tribute` that has that ending among its causes |
| `peace`, `truce` | every event of kind `region_ceded` or `tribute` that has e among its causes |
| `sacked`, `settlement_captured` | every event of kind `capital_moved`, `refugees_settled` or `civ_conquered` that has e among its causes |
| any other kind | none |

Companions are not followed further (a companion's own companions are not added, except as listed: the war's ending brings its cession or tribute). Kinds absent from a world's catalogue simply never occur. The kind keys are those of P3-06 11 and P3-06 20 (amendment r9 there: kinds 32 `refugees_settled` and 33 `tribute`, after P3-08's kind 31); the report finds them by key in the stored catalogue, never by id.

### 21.3 The summary set and its threshold (replaces 7.4; 6.7's "an event is shown when its `salience` ≥ T"; 13 item 2)

(r17: items 1 to 4 replaced by 24.3, item 7 by 24.10 item 1.)


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

**API** (5.1; r16 as implemented): `[[nodiscard]] std::string history_note(const HistoryView& view, std::int64_t event_id, std::int64_t civilization = 0);` in `worldgen/history/digest.hpp` returns the note without its parentheses, or an empty string (also for an id not in the log); `civilization` is 22.2's argument, used only for the generations note of a `quiet_stretch` (0, the chronicle's value: no such note); the report and P4's browser call it, nobody re-implements it (as `render_event`, P3-00 6.3). It does not fail on a loaded view.

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

   **(r11) Restated; this replaces the paragraph above** (P3-06 20.28, the lead's ruling: the run counts exactly the wars that weariness counts, so that T2 follows from P3-06 invariant (12)).
   - **Counted wars.** For one defender D and region G: the `war_declared` events with `defender` D and `event.region` G, in id order, **without the wars closed by a side's end**: those whose `war` row's `until_event` is a `civ_conquered` or a `civ_ended`. Such a war neither extends a run nor breaks one; the counted wars before and after it are consecutive. A war whose row is open at the present is counted, with until = E.
   - **Change.** A *change* in an interval [a, b] is a `region_control` row of G whose `from_year` or `to_year` lies in [a, b].
   - **Run.** A run is a sequence of consecutive counted wars w_i … w_j with no change in the window [since(w_i), max(until(w_i), …, until(w_j))]. The key is the length of the longest run over all (D, G), 0 when no war is counted. (The window ends at the latest end in the run, since wars over one (D, G) may overlap.)
   - **Incremental form** (equivalent; each (D, G) separately, with run = 0 and key = 0 at the start). For each counted war w = [s, u] in id order:
     - if run > 0 and there is no change in [lo, max(hi, u)]: run += 1 and hi = max(hi, u);
     - else if there is no change in [s, u]: run = 1, lo = s and hi = u. The first war after a change of owner starts a run of **1**, not 0, when its own span holds no change;
     - else run = 0: a war whose own span holds a change is in no run.

     After each war, key = max(key, run).
   - **Examples.** 1024 seed 6, region 40, defender c1: e341 `war_won`, e445 `peace`, e453 closed by `civ_conquered` e488, e538 `truce`, with no change in 4892–5597. e453 is skipped and the key is **3**: e341, e445, e538. 1024 `calm` seed 17, region 11, defender c1: e285 (5577–5587) and e297 (5588–5598), with changes at 5039 and 5857 only. The key is **2**.
6. (r10) **Events by kind**: `[stats]` prints `events_war_won`, `events_search_abandoned` and `events_refuge_held` after `events_tribute`, by kind id (9: one line per catalogue kind). They need no rule here. The war line words of the conflict golden (`won-c<W>`, `-ceded`, `-tribute`, `truce`, `unfound`, P3-06 20.26) are the reference's, not a report line: the report prints wars as `war` relation rows (6.6) and their endings as event lines.
7. (r10) **The summary's companions** (21.2 with item 1 above) name every ending kind: a shown `war_declared` brings its `peace`, `war_won`, `truce`, `search_abandoned`, `civ_conquered` or `civ_ended`; a `peace` or `war_won` brings its `region_ceded` or `tribute`. A `refuge_held` (salience 25) has no companions and is no companion: it is the siege of a refuge, and a siege is shown only at its own salience.

## 22. Quiet stretches, outbreaks and wording (amendment r12, P3-10, generator 0.19.0; before implementation)

Normative for the change P3-10 (`docs/specs/p3-10-followups.md`). P3-07 is not implemented; its implementer follows this section with 21 from the start. Format v1 is unchanged: every line keeps its grammar.

1. **Kinds 37 and 38.** `[stats]` prints `events_plague_outbreak` and `events_quiet_stretch` after `events_refuge_held`, by kind id (9: one line per catalogue kind); `events_plague` and `events_quiet_era` are 0 in a 0.19.0 world and are still printed. Wherever a P3-07 measure or a validation statistic counts plagues (the plague share of the summary, P3-05 15.4, P3-08 6.5), it counts kinds 12 and 37 together, so worlds of 0.18.0 and 0.19.0 compare. 21.2's units and companions are unchanged (neither kind is in a unit).
2. **Generations in a civilization's milestones** (6.6). A milestone line of civilization c whose event is a `quiet_stretch` gets a note in the form of 21.4: `<C>: no new generation of rulers.`, `<C>: one new generation of rulers.` or `<C>: <n> new generations of rulers.`, with C the civilization's name (at a sentence start) and n = g(`end_year` + 1) − g(`year`), g(t) = (t − c.`founded_year`) div the race's `generation_years` (P3-04 6.6's count, which `quiet_era` stored as `generations` before 0.19.0). The chronicle's lines of `quiet_stretch` have no note (the stretch is the world's, not one realm's). `history_note` gains an optional civilization argument for this note (0 at the chronicle: no note), so P4's browser can show it in a realm's page; otherwise its contract is 21.4's. (r13) Implemented with the report, not with P3-10: P3-10's engineer left `digest.cpp` unchanged, as the P3-07 implementer's task. A realm that is not a participant (more than 64 living realms, P3-04 20.1) gets no note.
3. **The flare-up note** (21.4 item 2) reads `The claim of <A> to <R>, held by <B>, came to a head.` in place of `<A> pressed its claim to <R> against <B>.`: "its" does not agree with a realm named in the plural (`The Warrens of Zeez`), and the new wording starts with a fixed word, so A, R and B are all mid-sentence (a leading `The` of each lower-cased). It is the one place where a flare-up's parties are named (P3-04 20.5): the `flare_up` sentence stays generic. Example: `In 2949 tensions flared into 50 years of upheaval. (The claim of the Grork Horde to Southwest Wood, held by Finen, came to a head.)`
4. **Numbers** inside a sentence carry P3-04 20.3's separators; the report's own numbers (years in notes, `[stats]` values, refs such as `{years=1924}`, census and peak figures) do not: they are machine-readable integers (6.1).
5a. **Repeated searches stay out of the summary** (P3-06 21.7 item 3). In 21.3 item 1, a `war_declared` whose `war` row's `until_event` is a `search_abandoned` with `salience` below `SUMMARY_MIN_SALIENCE` is **not a candidate**, and it is no companion of any event; the `search_abandoned` itself is below 20 and is not a candidate either. So neither the declaration nor the ending of a repeated fruitless search reaches the summary (only the first such war in a world, whose search has salience 40), and invariant 13 item 7 holds (the declaration is not shown). At `full` every event is shown as before.
5b. **The run statistic follows the holder** (P3-06 21.8). 21.8 item 5's `wars_same_goal_unchanged_max`, as restated by P3-06 20.28 (r13): a run over (D, G) is broken by a **change of holder**, a `region_control` row of G whose `civ` differs from that of G's previous `region_control` row (G's first row is a change); a row of the same civilization after a gap is not a change. Everything else of 20.28's definition is unchanged. P3-06 21.8 states the same reading for weariness, so the statistic and the rule count the same wars.
5. **Samples.** Every sentence quoted in sections 6, 15, 17 and 21 that shows a quiet era, a plague, a battle with "0 of the victors", a moved capital or a secession was rendered with the templates of 0.14.0 to 0.18.0 and is illustrative for 0.19.0.
6. **Tests** (amends 14): a world with two `search_abandoned` events (salience 40 and 15): the first war's declaration and search are in the summary, the second's are not, and item 7 of invariant 13 holds; the run statistic on a region emptied and resettled by its holder (one run) and on one taken by another civilization (two runs); on fixture H1, a milestone note for a `quiet_stretch` with n = 0, 1 and 12; no note on its chronicle line; the flare-up note with a `The …` claimant at the third position (lower-cased); `events_plague_outbreak` and `events_quiet_stretch` lines in id order.

## 23. Claims and cessions that name settlements; the claim behind a war (amendment r14, P3-11, generator 0.20.0; before implementation)

Normative for the change P3-11 (`docs/specs/p3-11-followups.md`). P3-07 is not implemented; its implementer follows this section with 21 and 22 from the start. Items 3 and 4 change `history_note` (`worldgen/src/history/digest.cpp`, implemented since P3-09), so P3-11's engineer applies them now; items 1 and 2 are the report implementer's. Format v1 is unchanged: every line keeps its grammar.

1. **Kinds 39 and 40** (P3-04 r17 21.2; P3-06 r19 23.2, 23.3). `[stats]` prints `events_claim_settlements` and `events_settlements_ceded` after `events_quiet_stretch`, by kind id (9). Wherever a measure of this spec or a validation statistic counts claims, it counts kinds 20 `claim` and 39 together; wherever it counts cessions, kinds 29 `region_ceded` and 40 together. The report finds both by key in the stored catalogue, as 21.2 says for every kind.
2. **Units and hollow wars** (21.2, 21.5, 21.8 item 1). In 21.2's companions, "every event of kind `region_ceded` or `tribute`" reads "of kind `region_ceded`, `settlements_ceded` or `tribute`", for a `war_declared` (through its ending) and for a `peace` or `war_won`. In 21.5, "the war of d" includes the `settlements_ceded` events caused by u, and `wars_hollow` counts a war with one as not hollow: settlements changed hands.
3. **The declaration's note names the claim's region when it differs** (21.4 item 1; P3-06 r19 23.4). c = the event's cause of kind `claim` or `claim_settlements` with the smallest id; y_c its year; R_c its `event.region`; G the declaration's `event.region`. When R_c = G the note is as before (`Claimed since <y_c>.`, `Claimed the same year.`). When R_c ≠ G it reads `The claim, since <y_c>, was to <R_c>.`, or `The claim, made the same year, was to <R_c>.` when y_c is the event's year, with R_c rendered as a name in mid-sentence (21.4). In both cases ` The <ordinal(n)> war between them.` follows when n ≥ 2. Example (illustrative, after the critic's default seed 55 e624): `5954 e624 war_declared/70: Clan A'thuukh declared war on the Kingdom of Sunt over East Wood. (The claim, since 5930, was to Woodward's Shaw. The second war between them.) {…} [causes e510, e540]`.
4. **The flare-up note for a claim of kind 39** (21.4 item 2 as worded by 22.3). If the flare-up's first cause c is of kind `claim_settlements`, with claimant A, holder B, region R and the `settlement` participants S_1 … S_k in participant order: `The claim of <A> to <S> of <B> in <R> came to a head.`, with S the names joined as `{role:all}` joins them (`S1`, `S1 and S2`, `S1, S2 and S3`) and every name in mid-sentence. A cause of kind `claim` keeps 22.3's note.
4a. **The declaration names the towns of a kind 39 claim** (r15; the P3-11 world critic's F5, `docs/reviews/p3-11-followups-world-critic.md`). When a war follows a kind 39 claim, its declaration still reads "over {@region}", the region named by the claim. A third realm may hold that region ("The Grotch Mob declared war on the Drolidrai Throne over Bellringer's Forest.", default seed 14, e674), or the claimant itself ("Clan Kuushbam declared war on the Kingdom of Geadalack over Snowscar.", default seed 7, e607). Either way, a summary that does not show the claim reads it as a war over land. At 1024 defaults 48 declarations follow a kind 39 claim, 15 of them on the claimant's own region and 33 on a third realm's. **Requirement:** when c (item 3) is of kind `claim_settlements`, the declaration's note names the towns, in place of item 3's forms:
   - `The claim, since <y_c>, was to <S> in <R_c>.`, or `The claim, made the same year, was to <S> in <R_c>.` when y_c is the event's year;
   - S is c's `settlement` participants in participant order, joined as `{role:all}` joins names (`S1`, `S1 and S2`, `S1, S2 and S3`). These are the settlements at the time of the claim, whether or not they still stand;
   - R_c is c's region, and every name is in mid-sentence (21.4);
   - this form is used whether or not R_c equals the war's region G;
   - ` The <ordinal(n)> war between them.` follows as before.

   Example: `Clan Kuushbam declared war on the Kingdom of Geadalack over Snowscar. (The claim, since 5779, was to Ghaldgruz Camp in Snowscar.)`. The sentence itself stays `render_event`'s: kind 21's structure is shipped and its region is the goal, so only the note can say it. Like items 3 and 4 this is `history_note`'s text (`digest.cpp`). No world byte changes. It was not part of P3-11's merge: P3-07's implementer, or the next change that edits `digest.cpp`, applies it.
5. **Tests** (amends 14): `[stats]` lines for kinds 39 and 40 in id order; a declaration caused by a kind 39 claim with one and with two settlements, on a region its claimant holds and on one a third realm holds (item 4a); a war ended by a `peace` that causes a `settlements_ceded` is not hollow and the cession is a companion of the peace and of the declaration; a declaration whose claim region differs from its own, in the same year and in an earlier one, with and without an earlier war between the pair; a flare-up caused by a kind 39 claim with one and with three settlements, one of them named `The …`.

## 24. Readability (amendment r17, after the world critic's review; generator stays 0.20.0)

**What this section is.** Normative for the report from r17. It answers the world critic's review of the implementation, `docs/reviews/p3-07-history-report-world-critic.md` on `origin/p3/07-critic` (pass with notes, findings A1 to A10), within the lead's rulings of 2026-11-22. Where this section and sections 4–23 differ, this section rules.

**What changes.** Only the report's selection, order and text change. No world byte changes: generator 0.20.0, schema 7, every template and salience as stored. `--level full` still prints every event.

**Format version.** P3-07 has not been merged, so no report of format `v1` has been released. r17 therefore redefines `v1` before its first release, and the header stays `# worldgen history v1`. 10.3's "needs v2" applies from the merge on.

### 24.1 Kinds and helper definitions (used below; integers only, every order total)

Kinds are found by key in the stored catalogue (21.2). Names used below:

| Name | Members | Meaning |
| --- | --- | --- |
| **W** | `war_declared`, `battle`, `battle_at_pass`, `siege`, `sacked`, `settlement_captured`, `refuge_held`, `peace`, `war_won`, `truce` | the chain kinds |
| **E_w** | `battle`, `battle_at_pass`, `siege`, `sacked`, `settlement_captured`, `refuge_held`, `peace`, `war_won`, `truce`, `search_abandoned`, `region_ceded`, `settlements_ceded`, `tribute`, `capital_moved`, `refugees_settled`, `civ_conquered` | the war-event kinds |

A kind absent from a world's catalogue never occurs.

**The war of an event, war(e)**, defined for every event in ascending id (each case uses only smaller ids, so one pass in id order computes it):
1. if e is a `war_declared`: war(e) = e;
2. else, if e is of a kind in E_w and has a cause of a kind in W: war(e) = war(c), with c the **smallest-id** such cause. This holds also when war(c) is none (r18): case 3 is then not tried;
3. else, if e is of a kind in E_w, has **no** cause of a kind in W, and is the `until_event` of one or more `relation` rows of kind `war`: the `since_event` of the row with the smallest `since_event`;
4. else war(e) = none.

Cases 2 and 3 follow the cause rules of P3-06 11 (battle ← declaration, siege ← battle, sack and capture ← siege, consequences ← sack or capture, ending ← the war's last event, cost ← the ending). An abandonment or a dwindling that cites a battle in its cause clause (P3-05 22.2) is not in E_w and has no war.

**The war frame of a declaration d**, Frame(d), is these events, in no particular order:
- d itself;
- its **last battle**: the `battle` or `battle_at_pass` x with the largest id such that war(x) = d, if there is one;
- its **ending** u: the `until_event` of the `war` row with `since_event` = d, when that row is closed;
- every event of kind `region_ceded`, `settlements_ceded` or `tribute` that has u among its causes.

Frame(d) is **empty** when d is excluded by 22.5a: its ending is a `search_abandoned` with salience below `SUMMARY_MIN_SALIENCE`.

**A claim** is an event of kind `claim` or `claim_settlements`, with claimant a (role `claimant`), holder b (role `holder`) and year y. It is **pursued** when it is a cause of at least one `war_declared`.

**Own roles**, used by the milestones in 24.5: `founder`, `attacker`, `winner`, `conqueror`, `to`, `payee`, `claimant`, `searcher`. A civilization's participation in an event is **own** when it has one of these roles in it.

### 24.2 Units (replaces 21.2's table; A1, A8)

The unit U(e) of an event e is the union of four parts:
1. e itself;
2. 21.2's companions as amended by 21.8 and 23 item 2. A `war_declared` brings its ending and the ending's cost; a `peace`, `war_won` or `truce` brings its `region_ceded`, `settlements_ceded` or `tribute`; a `sacked` or `settlement_captured` brings its `capital_moved`, `refugees_settled` and `civ_conquered`;
3. **(r17, A1) the war frame**: if war(e) = d is not none, Frame(d). This applies to every event with a war, the `war_declared` itself included (war(d) = d), so a declaration's unit holds its last battle. For a `war_declared` the rest of the frame repeats part 2; for every other war event it brings the war's declaration, its last battle, its ending and the ending's cost. When d is excluded by 22.5a, Frame(d) is empty (24.1) and part 3 adds nothing;
4. **(r17) the cause of a consequence**: for a `capital_moved`, `refugees_settled` or `civ_conquered`, its smallest-id cause of kind `sacked` or `settlement_captured`, if it has one.

Companions are not followed further; for instance, the sack added by part 4 does not bring its own `capital_moved` lines. 22.5a's rule stands: a declaration excluded there is no candidate and no companion.

**Consequence.** A shown war event is never without its declaration, and a shown declaration is never without its ending (when closed) and its last battle (when it has one). The shown consequences of a sack or capture are never without it. This holds at every threshold, with any number of civilizations per race: 24.10 item 1 makes it an invariant.

**Unit size.** A unit has at most 8 events: e, three consequences of a sack, and a frame of four. (The largest frame is a declaration, its last battle, its ending and one cost: P3-06 20.3 and 22.5 give an ending at most one cost event.)

### 24.3 The summary set (replaces 21.3 items 1 to 4; A2, A3, A7, A8)

1. **Candidates**: the events with `salience` ≥ `SUMMARY_MIN_SALIENCE` (20), **except**:
   - every claim (kinds `claim`, `claim_settlements`; A2);
   - every `battle` and `battle_at_pass` (A8); the last battle of a war enters through its war's frame;
   - every `plague_spread` (A7); the `plague_outbreak` sentence states the reach and the total;
   - every `quiet_stretch` (item 2);
   - 22.5a's excluded declarations.

   They are ranked by (`salience` descending, `id` ascending).
2. **Quiet stretches (A3).** F = every event of kind `quiet_stretch`, whatever its salience. F is in the summary set always, outside the threshold.
3. **Age floor (A7).** The floor set starts as F. For each age k in ascending id:
   - walk the candidates whose `year` lies in age k, in the order of the key (involves(k, e) descending, `salience` descending, `id` ascending). involves(k, e) = 1 when e has a participant of entity type 3 whose civilization's `race` is `age.race` of k, or a participant of entity type 1 equal to it; else 0. (r18) Equivalently: the candidates' rank order of item 1, stably partitioned so that those with involves(k, e) = 1 come first;
   - (r18) before each candidate, make the stop test below; if it does not stop the walk, then a candidate already in the floor set (as F's member or through an earlier unit) is skipped, and any other candidate's whole unit is added, events of other ages included. A unit is never cut, so the age may end with more than 20 events;
   - stop when the floor set holds at least `SUMMARY_AGE_FLOOR` (20) events whose `year` lies in age k, or when the age's candidates are exhausted. Every event of the floor set counts toward the age of its own `year`: F's quiet stretches from the start, and the events of a unit added in another age's walk.
4. **Threshold.** C(t) = the union of U(e) over the candidates with `salience` ≥ t. T = the smallest t in [20, 100] with |floor ∪ C(t)| ≤ `SUMMARY_BUDGET` (250). If there is none, T = 100. The summary set is floor ∪ C(T); it contains F.
5. **Bound.** The floor set holds at most |F| + (ages) × (19 + 8) events: each walk stops after the unit that reaches 20, and a unit has at most 8 events. A world has fewer than about 15 quiet stretches (P3-04 20.1), so with the design's six ages the floor stays below 250.
6. `[chronicle]`'s `threshold`, `shown` and hidden-count lines, and `[stats]`'s `summary_threshold` and `summary_events`, are defined on this set as in 21.3 items 5 and 6.
7. **Claims at summary.** No claim is printed in the summary chronicle.
   - A pursued claim is read through the note of the declaration that cites it (21.4 item 1, 23 item 3, 23 item 4a: its year, and its region or settlements when they differ from the war's).
   - A flare-up's claim is read through the flare-up's note (21.4 item 2, 23 item 4).
   - An unpursued claim that still stands at E is listed in `[present]` (24.7).
   - Every claim is still printed in the `full` chronicle.

### 24.4 Block order, `[phases]` and the phase headings (amends 6.2, 6.5, 6.7; A9)

At both levels the blocks are, in this order:

```
# worldgen history v1
level = <summary|full>
[settings]
[ages]
[phases]          full only
[chronicle]
[civilizations]
[present]
[stats]
```

The chronicle comes before the civilization blocks, so a reader meets the history before the realms (A9).

`[phases]` is printed at `full` only, as 6.5 gives it. At `summary` its content is in the chronicle's phase headings, which become, **at both levels**:

```
-- p<id> <kind>: <start_year>-<end_year>, <pl(end_year − start_year + 1, year)>[; trigger e<trigger_event>]; <pl(m, event)>
```

with the trigger part only when `trigger_event` ≠ 0 and m the phase's events, as in 6.5. (r18) m counts every event of the phase, shown or not, so the heading's count is the same at `summary` and at `full`. The `(n more events not shown)` line is unchanged.

### 24.5 Civilization blocks (amends 6.6 and 7.3; A4, A5)

The block's lines, in this order, at both levels unless marked:

```
c<id> <name>
  race: …                                             (6.6)
  home: …                                             (6.6)
  founded: …                                          (6.6)
  seceded from: <civilization ref>, e<founding_event>   only when founding_event is a civ_seceded
  capital: <settlement ref> | capital: none           alive (6.6)
  last capital: <settlement ref> | last capital: none ended (ended_year not NULL)
  peak regions: …                                     (6.6)
  peak population: …                                  (6.6)
  settlements: …                                      (6.6)
  wars: <a> declared, <f> defended; won <w>, lost <l>, drawn <d>, open <o>
  relations: none | relations: <n> (<kind> <count>, ...)   full only (6.6), followed by its rows
    <relation line>                                        full only (6.6)
  adversaries: none | adversaries: <n>                     summary only
    <adversary line>                                       summary only
  events: <n>                                         (6.6)
  fate: …                                             (6.6)
  milestones:
    <span> e<id> <kind key>: <sentence>[ (<note>)]    (24.5 below; r16's form)
```

- **`seceded from`**: the participant with role `from` of the `civ_seceded` that is the civilization's `founding_event`.
- **The wars line.** Count the `relation` rows of kind `war` in which the civilization is `a` (type 3, `a_id` = c: **declared**, the attacker) or `b` (type 3, `b_id` = c: **defended**). The side is read from the row, not from the declaration's roles (r18). For each such row with `until_event` u:
  - **open**: the row is open (`until` NULL);
  - **won**: u is a `peace` or `war_won` with c as `winner`, or a `civ_conquered` with c as `conqueror`;
  - **lost**: u is a `peace` or `war_won` with c as `loser`, or a `civ_conquered` with c as `civilization`;
  - **drawn**: every other closed row. This covers a `truce`, a `search_abandoned`, and an end by a third party or by a side's own end: no victor between the two. (r18) A `peace` or `war_won` in which c is neither `winner` nor `loser`, and a `civ_conquered` in which c is neither `conqueror` nor `civilization`, count as drawn.

  Then w + l + d + o = a + f.
- **Adversaries** (summary only; it replaces the relation rows, which `full` keeps). Let R be 6.6's rows of the civilization. An **adversary** is each distinct other end (entity type, id) of a row of R. The adversaries are ordered by (the smallest `since` among their rows, entity type, id), ascending. `adversaries: none` is printed when R is empty, else `adversaries: <n>`, followed by one line per adversary o:
  - with k ≥ 1 rows of kind `war`: `    <o ref>: <pl(k, war)>, won <w>, lost <l>, drawn <d>, open <o'>, <first>-<last>; now <state>`. The counts are the wars line's, over o's war rows only. first is the smallest `since` of those rows. last is the largest of, over those rows, E − 1 for an open row and max(`since`, `until` − 1) for a closed one;
  - with no war row: `    <o ref>: no war; now <state>`;
  - `<state>` reads the rows of R with o that are open at E: `at war` if one is of kind `war`, else `at peace` if one is of kind `peace`, else `enmity` if one is of kind `enmity`, else `none`. (r18) `none` covers both no open row and open rows of other kinds only (a pact, for instance).
  - (r18) `pl(k, war)` is 6.1's plural: `1 war`, `2 wars`.

  `<o ref>` is a reference as 6.1 gives it (types 6–9 without a name). A pair's rows appear in both civilizations' blocks, but each block has one line per adversary in place of every row.
- **Milestones (A5; replaces 7.3).** E_c is 7.3's events of c. The milestones are at most `MILESTONES` (**8**) events of E_c, printed in ascending id, chosen in this order:
  1. c's `founding_event`;
  2. c's `ending_event`, when c has ended;
  3. **the longest quiet stretch of c**: the `quiet_stretch` of E_c with the largest value of the key `years`, then the smallest id (none if c is in none). It carries 22.2's generations note on its milestone line, so **the note is reachable**: every civilization that lived through a quiet stretch as one of its participants prints one;
  4. **fill**: walk the remaining events of E_c, leaving out every other `quiet_stretch`, in the order of the key (own(c, e) descending, `salience` descending, `id` ascending), where own(c, e) = 1 when c has an own role in e (24.1), else 0. Take each event whose kind key has not yet supplied `MILESTONE_KIND_MAX` (**2**) fill events. Stop when 8 events are chosen or E_c is exhausted.

  Items 1 to 3 do not count toward a kind's limit. (r18) Further:
  - items 1 and 2 are taken even when the founding or ending event is not in E_c (c is not one of its participants);
  - an event is listed once: an ending equal to the founding, or a quiet stretch already taken, is not added again, and the fill walks only events not yet chosen;
  - in item 3 an event with no value of key `years` counts as 0 (every stored `quiet_stretch` has one);
  - the kind limit counts per event kind (kind id), over the fill events only.

  So a realm's list leads with what it did (its foundings, its victories, its gains, its sacks of others) before what was done to it, and no kind fills more than two slots by itself. In the critic's seed 0 the House of Nife lists its sacks of Baskeeb towns, while the Baskeeb Mob lists its won wars and the cession of the Holt of Yews (e59) first.

### 24.6 `[ages]` (amends 6.4; A6)

The end of the age line becomes

```
; <pl(n, civilization)> founded: <w> at the awakening, <s> by secession
```

where n is 6.4's k (the civilizations with `founded_year` in the age), w is the number of those whose `founding_event` is of kind `civ_founded`, and s = n − w (r18: s is computed, not counted; every other founding, in practice a `civ_seceded`, falls in s). An example: "a6 The Age of Orcs: 5638-5999, 362 years; Orcs (ra6) made by Uul'duk (g6, evil); 4 civilizations founded: 1 at the awakening, 3 by secession".

### 24.7 `[present]` (amends 6.8; A2, A9, A10)

After `largest settlement:` four lists follow, at both levels, each with its count line always printed. In every list, references are printed as 6.1 gives them.

```
ruins: <n>
  s<id> <name> in <region ref>: ruined <ruined_year>, e<ruin_event> <kind key>[, by <attacker ref>]; last held by <civilization ref | no one>
refuges: <n>
  s<id> <name> in <region ref>: <owner ref>, population <p>, founded <founded_year> by flight from <origin ref>, e<founding_event>
open wars: <n>
  e<since_event> <a ref> against <b ref> over <region ref>, since <since>
standing claims: <n>
  e<id> <year> <claimant ref> on <holder ref>: <region ref>[, for <settlement ref>, <settlement ref>, ...]
```

- **Ruins.** The settlements with status 3 (ruined; faded ones are out), in ascending id.
  - `by` is the first participant (by participant `seq`) of the `ruin_event` with role `attacker`, when there is one.
  - **Last held by** is the `owner_civ` of the settlement's `settlement_census` row of largest `year` with status 1 or 2; `no one` when there is none, and also (r18) when that row's `owner_civ` is 0.
- **Refuges.** The standing settlements whose `founding_event` is a `refugees_settled`, in ascending id. `origin` is that event's participant with role `origin`. (r18) A refuge whose `owner_civ` is 0 prints `no owner` (6.8's word) in place of `<owner ref>`. **Whether a refuge is still hidden at E cannot be read from the world.** It depends on the race's `hidden_years` (data, not stored), and the faeries' concealment (P3-06 20.17) is not stored either, so the list names every refuge with its founding year, not "hidden refuges" (see the change notes at the end).
- **Open wars.** The `relation` rows of kind `war` with `until` NULL, ordered by (`since`, `a_type`, `a_id`, `b_type`, `b_id`). The region is the `event.region` of the row's `since_event` (r18: of that event, not of a later one of the war), the war's goal.
- **Standing claims (A2).** The claims c that are **not pursued** and that the log does not show closed. (r18) "Pursued" is 24.1's: some `war_declared` has c among its causes (`event_cause`). With claimant a, holder b and year y:
  - (i) a and b both have `ended_year` NULL;
  - (ii) no `war_declared` with {`attacker`, `defender`} = {a, b} has `year` ≥ y. The pair is unordered: a declaration by b on a closes the claim too; a declaration in the claim's own year closes it;
  - (iii) no later claim (larger id, of either claim kind) has claimant a and holder b, in that order: a later claim of b on a does not replace it.

  They are listed in ascending id. The `for` part lists, for a `claim_settlements`, its `settlement` participants in participant order.
  - **Limit**: P3-06 20.2 item 2 and 20.23 drop a claim silently at a declaration when no allowed goal is left. The world records no event for that, so such a claim is listed as standing. This is the one way the list differs from the simulation's state (see the change notes at the end).
- **The settlement line at full (A10).** 6.8's settlement line of a ruined or faded settlement ends `…, ruined <ruined_year>, e<ruin_event>, last held by <civilization ref | no one>`, with the same "last held by" as the ruins list. (r18) When `ruined_year` is NULL (a faded settlement), the `, ruined <ruined_year>, e<ruin_event>` part is left out and the line ends `…, last held by <civilization ref | no one>`.

### 24.8 What is recorded and left (A7, A8, A10, A9 form)

- **A7** is answered by 24.3 item 3's preference for the age's own race and by leaving `plague_spread` out of the candidates.
- **A8** is answered by leaving battles out of the candidates and putting each war's last battle in its frame.
- **A10, "grew into a city of 8,0xx"**: not fixed. The number is part of the stored template of kind `became_city` (data, in every world), and the report prints sentences as `render_event` gives them (13 item 4). Changing the template changes world bytes, so it goes with the next change that alters world bytes (r18: listed in `docs/specs/p3-11-followups.md` section 12).
- **A9, a plain reading form** without ids, refs and causes: not in r17 (the lead's ruling). It is put to Paul (question 1 of the review). Ids and refs stay on every chronicle line.
- **A9, `[stats]`**: unchanged, at both levels.

### 24.9 Constants (amends 8)

`MILESTONES` = **8** (was 6). New: `MILESTONE_KIND_MAX` = 2 (24.5). `SUMMARY_MIN_SALIENCE`, `SUMMARY_BUDGET`, `SUMMARY_AGE_FLOOR`, `ISLET_MAX_DRY_CELLS` and the share levels are unchanged.

### 24.10 Invariants (restates 13 and 21.3 item 7)

For every world with history:

1. **(replaces 13 item 2 and 21.3 item 7)** At `full`, the chronicle has one line per event, in id order, and `shown` = `events`. At `summary`, the event lines are exactly 24.3's summary set, `shown` = `summary_events`, and `shown` ≤ 250 unless T = 100. Further, in the summary:
   - every shown event of a kind in E_w whose war(e) = d is not excluded by 22.5a has d shown, together with d's ending when closed and d's last battle when it has one: **the number of shown war events whose declaration is not shown (the orphans) is 0**;
   - every shown `war_declared` with a closed row has its ending shown;
   - every shown `sacked` or `settlement_captured` that is a candidate in its own right (salience ≥ `SUMMARY_MIN_SALIENCE`, 24.3 item 1) has its `capital_moved`, `refugees_settled` and `civ_conquered` shown. (r18) A sack or capture below that salience, shown only through part 4 of a consequence's unit, does not bring its own consequences (24.2: companions are not followed further), so this item does not apply to it;
   - every shown `capital_moved`, `refugees_settled` or `civ_conquered` caused by a sack or capture has that cause shown;
   - every `quiet_stretch` is shown;
   - no claim, no `plague_spread`, and no `battle` or `battle_at_pass` other than a war's last battle is shown;
   - each age with at least 20 candidates shows at least 20 events of its years.
2. **(replaces 13 item 3)** The sum over phases of shown lines and hidden counts equals `events`. Each phase heading's count, and at `full` each phase's count in `[phases]`, equals its shown and hidden events.
3. **(replaces 13 item 5)** `[stats]`, `[settings]` and `[ages]` are identical at both levels. `[phases]` is printed at `full` only. `[civilizations]` differs only in the relation lines (full) against the adversary lines (summary). `[present]` differs only in 6.8's full-only region and settlement lines. The chronicle differs as item 1 says. The level line and the order of blocks are 24.4's.
4. 13 items 1, 4, 6, 7 and 8 stand; item 1's "seven blocks in order" reads "the blocks of 24.4 in order".
5. Wars line: w + l + d + o = a + f for every civilization. Per adversary, the counts sum to k. The sum of k over a civilization's adversaries equals a + f.
6. Every civilization that is a participant of at least one `quiet_stretch` prints exactly one `quiet_stretch` milestone, with a generations note.
7. Every claim that is not pursued, whose two sides are alive at E and which 24.7's tests (ii) and (iii) do not close, appears in `standing claims`, and no other claim does.

### 24.11 Tests required (adds to 14)

For the test writer and the reference (`p3_report_reference.py`), from this section alone:

1. **The war frame.** On H1 and a new fixture **H8 frames**, with a war of three battles, a siege, a sack with `capital_moved` and `refugees_settled`, a peace and a tribute, and a second war ended by `civ_conquered`:
   - war(e) for every event;
   - the last battle;
   - a sack shown above the threshold brings the declaration, the last battle, the ending and the tribute, but not the other two battles;
   - a `refugees_settled` shown alone brings its sack and that sack's war frame;
   - a `capital_moved` caused by an abandonment brings nothing;
   - a declaration whose war ended in a repeated search (22.5a) is not brought in.
2. **civs4 orphans = 0.**
   - The reference and the C++ count the orphans of invariant 24.10 item 1 at `summary`. The count must be 0 on `civs4` seeds 0, 3, 7 and 12 at 1024 (the critic's worlds: 34, 56, 61 and 35 at r16) and on every world of the validation sweep.
   - On the same four worlds, report T, `shown` and the number of shown declarations, so the lead can judge the length.
   - The 1024 cases are a `*-slow` suite (r18: the skipped suite `history-report-slow`, run by the CTest entry `worldgen-tests-slow-history-report`, label `slow`; the other cases run in `worldgen-tests-history-report`).
3. **Claims.**
   - No claim in a summary chronicle.
   - A fixture with:
     - a pursued claim (in the note, not in the chronicle);
     - an unpursued claim, open at E (in `standing claims`);
     - one closed by a later war between the pair, in either direction;
     - one replaced by a later claim of the same pair;
     - one with a side ended;
     - a kind 39 claim with two settlements (the `for` part).
4. **Quiet stretches.** Every `quiet_stretch` is shown at summary in H4 (where T is high) and in a world with T = 100 (H5).
5. **Floor order.** An age whose candidates of salience 25 are split between the age's race and another race: the floor takes the age's race first.
6. **Battles and spreads.** No battle other than last battles, and no `plague_spread`, at summary. Both are present at full.
7. **Blocks.**
   - 24.4's block order at both levels; `[phases]` absent at summary; the new phase heading with and without a trigger.
   - `seceded from` on a seceded civilization; `last capital` on an ended one.
   - The wars line, with one war of each outcome (won by peace, won by `war_won`, won by conquest, lost, truce, search, third-party end, open).
   - Adversary lines: two adversaries with the same first year (ordered by type, then id); an enmity-only adversary (`no war`); each state word.
8. **Milestones.**
   - The seed 0 pair (c1, c2) has different lists.
   - Ties of the fill key.
   - The kind limit: three sacks of the same salience give two.
   - A realm with no quiet stretch.
   - The longest stretch's generations note, for n = 0, 1 and more.
   - A realm with fewer than 8 events.
9. **`[ages]`**: an age with one awakening and three secessions prints `4 civilizations founded: 1 at the awakening, 3 by secession`.
10. **`[present]`'s lists**:
    - a ruin by sack (with `by`) and a ruin by abandonment (without it);
    - a settlement captured twice before its sack (last held by its last owner);
    - a refuge;
    - two open wars in their order;
    - each standing-claim case of item 3.
11. **Invariants**: 24.10 items 1 to 7 on every fixture and every world case.
12. **Golden**: `p3-07-report.txt` is re-recorded by the reference (every report changes); H8 is added to the fixtures. 14.4's line order is unchanged.

### 24.12 Section 16 and 18.6

- **Section 16**: the excerpt is r16's output (generator 0.20.0) and is illustrative from r17. The blocks now come in 24.4's order, the phase headings carry the length, trigger and count, `[phases]` is absent at summary, the claims and battles leave the summary chronicle, the civilization blocks gain the lines of 24.5, and `[present]` gains 24.7's lists. The golden line of `sample-42-1024` is the record, as before.
- **18.6**: unchanged. The file names are `<preset>-<case>.history.summary.txt` and `<preset>-<case>.history.full.txt`.

## 25. Format v2 (amendment r19, P5-01, generator 0.21.0)

**What this section is.** Normative for the report from generator 0.21.0 (`docs/specs/p5-01-followups.md`, items 1, 7 and 10). It answers the world critic's second-round review, `docs/reviews/p3-07-history-report-world-critic-r17.md` (B1 to B6), the audit's advisory A3 (`docs/reviews/p3-07-history-report-audit.md` 7.1), and the report's part of P5-01's world changes (the fresh claim at a declaration, kinds 41 and 42). Where this section and sections 4–24 differ, this section rules. Paul's choice stands: ids and refs stay on every line.

**What changes.** The report's selection, line types and text. The world bytes this section reads change with P5-01 (P3-06 r22 24), but nothing here writes a world.

### 25.1 Format version and growth (amends 6.2, 10.3, 24.4)

The header is **`# worldgen history v2`** at both levels (24.4's first line). v1 was released with P3-07's merge, and 25.3 to 25.7 add or change line types, so 10.3 requires v2. 10.3's rules hold for v2 with one addition, the growth rule of P5 (`docs/specs/p5-00-history-depth-overview.md` 7.2): **within v2** a later stage may (a) append a block named for its entity kind (`[monsters]`, `[figures]`, `[factions]`, `[artifacts]`, `[roads]`) after `[present]` and before `[stats]`, in merge order, and (b) append new line types at the end of a civilization block of `[civilizations]` (after `milestones:` and its lines) or at the end of `[present]`. Each such amendment states its lines' grammar here, as 10.3 rule 1 does for keys. Any other change needs v3. `report --help` prints `format '# worldgen history v2'`.

The report reads every world of schema 7. A world of generator 0.20.0 or earlier gets the v2 report too; where a rule below reads kinds 41 and 42 (25.8, 25.9), such a world has none, and the stated fallback applies.

### 25.2 Calamities at summary (B1; amends 24.3 item 1)

**Why.** Seed 0 prints at T = 20: 104 of its 221 chronicle lines (47%) are plague, famine or succession crisis, and its last 300 years are about 55 such lines against two wars. Measured over the 100 default 1024 worlds at 0.20.0 (the spec writer's prototype of 24.3 on the stored worlds, `~/.cache/the-unmapped/out/p5-01-spec/b1proto.py`, which gives 24.3's set exactly): the calamity share of the summary set is 15% at the median and 53% at most.

**Calamity kinds**: `famine`, `plague`, `plague_outbreak`, `succession_crisis` (`plague_spread` is already no candidate, 24.3).

**Rule.** In 24.3 item 1, after the exclusions: for each phase p, rank the candidates of the four calamity kinds **together** (one ranking per phase, not one per kind) with `event.phase` = p by (`salience` descending, `id` ascending); only the first `SUMMARY_CALAMITY_PER_PHASE` (**2**, a new constant of section 8) of them stay candidates. The others are no candidates: neither the age floor nor the threshold takes them. They are no companions of anything (24.2). `full` prints every event, as before.

**Measured** (same prototype; 100 default 1024 worlds, and 20 `civs4` worlds):

| Rule | median T | median lines | calamity share median / max | declarations shown (default) | seed 0 |
| --- | --- | --- | --- | --- | --- |
| 24.3 (0.20.0) | 26 | 208 | 15% / 53% | 2,189 of 2,621 | 221 lines, 47% |
| floor T at 30 (B1's first option) | 30 | 198 | 15% / 39% | 2,189 | 141, 26% |
| candidates and T at 30 or more | 30 | 190 | 10% / 19% | 2,230 | 114, 13% |
| **2 per phase (chosen)** | 20 | 208 | **8% / 22%** | 2,190 | **133, 12%** |
| 1 per phase | 20 | 208 | 4% / 12% | 2,209 | 125, 6% |
| 3 per phase | 20 | 209 | 10% / 29% | 2,190 | 141, 17% |

The cap was chosen over a floor on T: a floor of 30 leaves seed 0 at 26% (the age floor still walks the calamities of 25 to 45), and raising the candidates' salience to 30 also removes every siege, refuge held and famine from the summary in small worlds, which a reader of a quiet world wants. With the cap the threshold falls back to 20 in most default worlds, so the lines freed go to colonies, foundings and sieges. In `civs4` (T of 71 to 76) nothing changes: no calamity reaches the threshold there.

### 25.3 Ruins in words; faded ruins apart (B2, B3's second part; amends 24.7 and 6.6)

1. **The ruins list** (24.7). The line becomes
   ```
     s<id> <name> in <region ref>: ruined <ruined_year>, e<ruin_event> <how>; last held by <civilization ref | no one>
   ```
   with `<how>` by the kind of the `ruin_event`:
   - `sacked` → `sacked by <attacker ref>` (the first `attacker` participant), or `sacked` when there is none (possible only where the kind's `attacker` role has min 0; under the shipped catalogue the role is 1–1, and a sack without one is a corrupt world);
   - `settlement_abandoned` → `abandoned`;
   - `settlement_abandoned_to` → `abandoned, survivors to <destination ref>` (the `destination` participant);
   - any other kind → its kind key alone, with no `, by` part, as in v1 (a later stage's ruin kinds read so until a v2 amendment names them).

   Examples: `s12 Rofis in the Silent Weald (rg8): ruined 5999, e674 sacked by The Sneerk-Grik Mob (c4); last held by The Realm of Ienngleth (c1)`; `s43 Guk-Gnik Hole in Reaver's Plain (rg44): ruined 4277, e295 abandoned, survivors to Gnokk Pit (s40); last held by The Sneerk-Grik Mob (c4)` (the destination illustrative).
2. **The settlement line at full** (24.7's last item) keeps `e<ruin_event>` and adds nothing: its kind is in the chronicle.
3. **The civilization block's settlements line** (6.6) becomes `  settlements: <f> founded, <h> held at the end, <r> ruined, <d> faded`, with r = the settlements with `founder_civ` = c and status 3, and d = those with status 4 (v1 counted both in r). So a block's ruins match `[present]`'s list, which shows status 3 only.

### 25.4 Adversaries with a war only (B3; amends 24.5)

**Why.** "no war; now enmity" lines add no history: phrase c1 to c11, the Miaflie Court with six of them and nothing else; in `civs4` seed 3, 52 of 148 adversary lines.

**Rule.** At summary the adversary count line stays `  adversaries: <n>` (every adversary, as 24.5 counts them; `adversaries: none` when there is none). It is followed by one line per adversary **with at least one war row**, in 24.5's order and form (with 25.7 item 1's span), and then, when m ≥ 1 adversaries have no war row, by one line

```
    without war: <m> (enmity <e>, none <q>)
```

with e the number of those m whose state (24.5) is `enmity` and q = m − e. The war adversaries and m add to n. `full` keeps its relation rows (6.6) unchanged.

### 25.5 Ended realms in short (B4; amends 24.5)

**Why.** With four civilizations per race the summary is 1,270 to 1,358 lines, about 730 of them blocks (26 to 28 realms). An ended realm's block repeats what its fate line and the chronicle say.

**Rule.** At summary, the block of a civilization with `ended_year` not NULL prints, in 24.5's order: the heading, `race`, `founded`, `seceded from` (when it applies), `last capital`, `settlements` (25.3), `wars`, `adversaries` (25.4), `fate` and `milestones`, and **leaves out** `home`, `peak regions`, `peak population` and `events`. Its milestones are at most `MILESTONES_ENDED` (**4**, a new constant of section 8; never more, whatever 24.5's items 1 to 3 give): 24.5's items 1 to 3 (founding, ending, longest quiet stretch), then the fill (with 25.7 item 3's key) until four are chosen. A block of an alive civilization is unchanged. `full` is unchanged.

**Reading.** The critic and P5-00 3.1 name "the block of a realm that ended before its first war". No such realm is in the four `civs4` samples: the critic's example, the Lith Court (c15 of `civs4` seed 3), ended in its first war (e840, conquered), and every ended realm there has at least one war row. So the rule takes every ended realm, which is what shortens the blocks; a one-line block was not chosen, since an ended realm's wars line and adversaries are what a reader of a `civs4` world follows from block to block.

**Effect** (counted on the r17 samples, `~/.cache/the-unmapped/out/p3-07-impl/samples-r17/`, at 0.20.0; the history changes with P5-01, so validation re-measures): `civs4` seeds 0, 3, 7 and 12 have 4 to 11 ended realms. With 25.4, 25.5 and 25.7 item 4 their summaries fall from 1,270–1,358 lines to an estimated 1,040–1,095; the default worlds' (the phrase world, seeds 0 to 9) from 644–806 to about 510–670. **Target** (P5-01 T18, restated by Paul's decision of 2026-11-22): summary length of the default 1024 worlds median ≤ 700 and maximum ≤ 800 lines; of the `civs4` worlds median ≤ 1,100 and maximum ≤ 1,400. Measured at 0.21.0: 321–754 lines, median 571, on 100 default worlds; 890–1,353, median 1,046, on 20 `civs4` worlds (r20; r19 stated at most 700 and 1,100 for every world).

### 25.6 Every flare-up with a war (B5; amends 24.3 item 2)

**Why.** In `civs4` seed 3 the Goblin age's first 200 years are three headings and "(31 more events not shown)": at T = 76 no `flare_up` line (salience 50) is shown, and 9 to 16 phases of a `civs4` world show nothing.

**Rule.** F (24.3 item 2) is every `quiet_stretch` **and every `flare_up` whose phase has at least one `war_declared`** (`event.phase` equal to the flare-up's), whatever its salience. F is in the summary set always, counts toward its age's floor and is not a companion. A flare-up's note (21.4 item 2, 22.3, 23 item 4) names the claim that came to a head.

**Measured** (0.20.0, the prototype with 25.2): in the 20 `civs4` worlds 236 of 243 flare-ups have a war in their phase and none is shown at r18; with this rule all 236 are, and the phases that show nothing fall from 202 to 134 of 715. In the 100 default worlds 390 of 393 flare-ups qualify and 304 are already shown; empty phases fall from 114 to 107 of 1,968.

### 25.7 Wording and consistency (B6; A9's `[stats]`; amends 24.5, 9)

1. **One end year per war.** In an adversary line (24.5) the span's last year is, over the pair's war rows, the largest of `E − 1` for an open row and **`until`** for a closed one (the year of the row's `until_event`, the year the chronicle gives for the war's end). v1 printed `until − 1` (the last year the relation was in force), so "the war ended after six years" in 5753 read as `5747-5752` in the block. `full`'s relation lines keep 6.6's interval `<since>-<until - 1>`: they print a relation's years in force, of every kind.
2. **The claim behind a war**: 25.8.
3. **Colonies before death tolls in milestones.** 24.5 item 4's fill key becomes (own(c, e) descending, cal(e) ascending, `salience` descending, `id` ascending), with cal(e) = 1 for an event of kind `famine`, `plague`, `plague_outbreak`, `plague_spread`, `succession_crisis` or `settlement_dwindled`, else 0. A realm with no war lists its colonies and kind changes before its famines ("Clan Durgarin: 5064 e229 famine: Famine struck Nidinnird; 214 died." gives way to its colonies, seed 0).
4. **`[stats]` at summary** (A9: 146 lines at the end of every summary). At `summary` the block prints only the keys of `SUMMARY_STATS`, in `full`'s key order (section 9's order; the summary block is `full`'s with the other keys left out): `end_year`, `events`, `summary_threshold`, `summary_events`, `civilizations`, `civilizations_alive`, `civilizations_ended`, `settlements_standing`, `settlements_ruined`, `population`, `share_max_permille`, `wars`, `wars_open`, `races_alive`, `wars_hollow_permille` (15 keys, a constant of section 8; r20: r19 listed them in another order). `settlements_ruined` counts status 3 only (section 9; status 4 is `settlements_faded`). At `full` every key is printed as before. **Consumers** that read `[stats]` (the sweeps, the critic's tools, P3-06 15.6) read the `full` report from 0.21.0, or its `[stats]` block, which is the same as v1's but for keys appended since.

### 25.8 The claim behind a war (item 1; amends 21.4 item 1, 23 items 3 and 4a)

From 0.21.0 a war whose goal differs from its claim's region cites a **fresh claim** on its goal, written at the declaration and caused by the old claim (P3-06 r22 24.1). So c of 23 item 3 (the declaration's cause of kind `claim` or `claim_settlements` with the smallest id) is always a claim on the war's region G, and 23 item 3's form `The claim, since <y_c>, was to <R_c>.` no longer occurs in a 0.21.0 world (it stays for older worlds, where R_c ≠ G is possible).

**The year of the note.** Let **root(c)** be c's cause of kind `claim` or `claim_settlements` with the smallest id, if c has one (c is then a fresh claim), else c itself. In 21.4 item 1, 23 item 3 and 23 item 4a, **y_c is the year of root(c)**: the note dates the grievance, not the declaration's re-aim. So a war re-aimed at its declaration in 5954 on a claim of 5930 reads `Claimed since 5930.`, not `Claimed the same year.` 23 item 4a's settlements S stay those of c (the fresh claim's, in G), and its region is c's (= G).

A fresh claim is a claim like any other in the chronicle (`full`: its line, the same year as the declaration, before it) and is not a candidate at summary (24.3). It is pursued (24.1: a cause of a `war_declared`); root(c) is closed in 24.7 by test (ii), since the pair's war is declared after it.

### 25.9 `[present]`'s refuges and standing claims made exact (item 7; amends 24.7)

1. **Refuges.** A refuge line (24.7) reads the `refuge_hidden` event (kind 42) with settlement s and the smallest id, if any. It ends, after `e<founding_event>`, with `; hidden` when that event's `end_year` ≥ E − 1 (the refuge was still hidden in the last year), `; found <end_year + 1>` when its `end_year` < E − 1, and nothing when there is none (a world before 0.21.0, or a race with `hidden_years` 0). 24.7's sentence "Whether a refuge is still hidden at E cannot be read from the world" is replaced by this rule. The faeries' concealment (P3-06 20.17) is not a refuge's state and is still not stored; a faerie refuge in deep forest may be concealed after it is found.
2. **Standing claims.** 24.7's list gains a fourth test: (iv) no event of kind `claim_dropped` (kind 41) has c among its causes. 24.7's "Limit" bullet is removed: with kinds 41 and the fresh claim (25.8), the list is exactly the claims the simulation holds open at E (P5-01 T19). **An open claim is closed** (r20, the simulation's rule, P3-06 7.2, 7.7, 8.1, 24.3, which the tests read from the log) by a declaration between its two sides in either direction, a `peace`, `truce`, `war_won` or `search_abandoned` of the pair, a `claim_dropped` citing it, or the end of either side. For a world before 0.21.0 the list is as in v1 (no kind 41 exists).
3. Kinds 41 and 42 are below `SUMMARY_MIN_SALIENCE` and no companions (24.2): they appear in the `full` chronicle only, and in these two lists.

### 25.10 `find` in place of `std::map::at` (audit A3; no output change)

In `digest.cpp` the two lookups the audit names (`last_claim.at({a, b})` in the standing-claims pass, and `first_since.at(...)` in the adversary sort) use `find`. A missing key reads as "not the last claim of the pair" and as "since = E" respectively, so a later edit that breaks the link between the loops gives wrong text, not an exception, and every error stays a value. No output changes.

### 25.11 Constants (amends 8 and 24.9)

New: `SUMMARY_CALAMITY_PER_PHASE` = 2 (25.2), `MILESTONES_ENDED` = 4 (25.5), `SUMMARY_STATS` (the 15 keys of 25.7 item 4). The others are unchanged.

### 25.12 Invariants (amends 24.10)

1. 24.10 item 1 holds with 25.2's candidates and 25.6's F. Further: at most `SUMMARY_CALAMITY_PER_PHASE` events of a calamity kind are shown per phase, unless one is shown as a companion or a frame member (none is, with the shipped kinds); every `flare_up` whose phase has a `war_declared` is shown.
2. 24.10 item 3 becomes: `[settings]` and `[ages]` are identical at both levels; `[stats]` at `summary` is the `SUMMARY_STATS` subset of `full`'s, with the same values, in the same order; `[civilizations]` differs in the relation lines (full) against the adversary lines (summary) and in 25.5's ended blocks; the rest as 24.10 item 3. Invariant 6 (every `[history]` key in `[stats]` with the same value) is read on `full`.
3. 24.10 item 5: the war adversaries (25.4) and m add to n.
4. 24.10 item 7 becomes: every claim not pursued, whose sides are alive at E, which 24.7's tests (ii) and (iii) and 25.9's test (iv) do not close, appears in `standing claims`, and no other claim does; in a world of 0.21.0 these are exactly the reference's open claims at E (P3-06 r22 24.3).
5. In a world of 0.21.0, no declaration's note has the form `The claim, since …, was to <R>.` with R ≠ the war's region.

### 25.13 Tests required (adds to 24.11)

For the test writer and the reference (`p3_report_reference.py`), from this section alone:

1. **Header**: `# worldgen history v2` at both levels; `report --help`'s text.
2. **Calamity cap** (25.2): a fixture phase with five calamity candidates of saliences 45, 45, 40, 25, 25: the two 45s (smaller id first) stay, the others are neither in the floor nor above any threshold; a phase with one calamity keeps it; seed 0 at 1024 (0.21.0) has at most 2 calamity lines per phase.
3. **Ruins** (25.3): one ruin of each `<how>`, a sack with no attacker participant (on a fixture whose `sacked` kind has an `attacker` role with min 0; under the shipped catalogue, 1–1, such a world is corrupt), an unknown ruin kind printed by key alone; the settlements line with a faded and a ruined settlement.
4. **Adversaries** (25.4): two war adversaries and three without war (two in enmity, one none): two lines and `without war: 3 (enmity 2, none 1)`; a realm with only no-war adversaries prints the count line and the `without war` line.
5. **Ended blocks** (25.5): an ended realm's block at summary without `home`, the peak lines and `events`, with four milestones of which a quiet stretch; the same realm at full unchanged.
6. **Flare-ups** (25.6): a flare-up whose phase has a war is shown at T = 100 (H5); one whose phase has none is not shown above its salience.
7. **Wording** (25.7): a war ended in year Y prints `…-Y` in the adversary line and `…-(Y − 1)` in the full relation line; a no-war realm with a famine and a colony lists the colony first; `[stats]` at summary has exactly the 15 keys in order, with full's values.
8. **The claim note** (25.8; the note's year is that of root(c), c's cause of a claim kind with the smallest id): a declaration citing a fresh claim (caused by a claim of an earlier year on another region) notes `Claimed since <the earlier year>.`; a fresh claim of kind 39 gives 23 item 4a's form with the fresh claim's settlements; a 0.20.0 fixture with R_c ≠ G keeps 23 item 3's form.
9. **Refuges and claims** (25.9): refuges with a kind 42 ending at E − 1 (`; hidden`), ending earlier (`; found <y>`), and none (no suffix); a claim with a `claim_dropped` is not listed; on every world case of 0.21.0 the standing-claims list equals the conflict reference's open claims at E.
10. **Invariants**: 25.12 on every fixture and world case.
11. **Golden**: `p3-07-report.txt` re-recorded by the reference (every report changes: the header at least). The fixtures gain the cases above (a new fixture **H11**, `h11-v2.toml`, holds them; r19 called it "H9 v2").

### 25.14 Section 16, 18.6 and the CLI

- Section 16's excerpt is v1 and illustrative from r19; the golden line of `sample-42-1024` is the record.
- 18.6 is unchanged: `world-hashes.sh` saves both levels per case, and `compare` checks them across presets.
- 4's help text: `print the history report (format '# worldgen history v2') instead of the world report`.

## 26. Format v3: renamed regions, hidden refuges (amendment r21, P5-12, generator 0.22.0)

Normative for the report from generator 0.22.0 (`docs/specs/p5-12-renamed-regions.md`, items 6 to 8). Where this section and sections 4–25 differ, this section rules. The report reads worlds of schema 8 only (the library accepts exactly the current version), so every world it reads has kinds 41 to 43 in its catalogue (they may have no events) and table `region_name`; 25's fallbacks for worlds before 0.21.0 no longer arise.

### 26.1 Format version (amends 25.1)

The header is **`# worldgen history v3`** at both levels: 26.3 renames a line type and 26.4 changes what references print, which 25.1's growth rule does not cover. 25.1's growth rule holds for v3 unchanged ("within v3" for "within v2"). `report --help` prints `format '# worldgen history v3'`; 25.14's help line likewise.

### 26.2 Renamed regions in `[present]` (appended after `standing claims`, both levels)

```
renamed regions: <n>
  rg<id>: <name> since <from_year>, e<event> after e<cause> <cause kind key>; formerly <previous name>
```

n = the `region_name` rows; one line per row in primary-key order (region, `from_year`). `<cause>` is the renaming event's cause (it has one, P5-12 8 item 5) and its kind key; `<previous name>` is the name in force before the row: the region's previous row's `name`, or `region.name`. Names escaped (6.1). Example: `  rg12: Edadbury's Fall since 5803, e1204 after e1201 sacked; formerly The Holt of Yews`.

### 26.3 Hidden refuges (F3; amends 24.7 and 25.9 item 1)

24.7's `refuges:` list becomes:

```
hidden refuges: <h> of <n> founded by flight
  s<id> <name> in <region ref>: <owner ref>, population <p>, founded <founded_year> by flight from <origin ref>, e<founding_event>
```

n = the standing settlements whose `founding_event` is a `refugees_settled` (24.7's set); h = those of them whose `refuge_hidden` event (kind 42, settlement s, the smallest id) has `end_year` ≥ E − 1; only the h are listed, in ascending id, and 25.9's `; hidden` / `; found <y>` suffix is dropped (every listed refuge is hidden). A refuge found before E is still told: its `refugees_settled` line in the chronicle (salience 65, a summary candidate and a companion of the sack, 24.2), its `refuge_hidden` line at full ("… lay hidden from 3811 to 4110."), its settlement line at full in `[present]`, and its realm's milestones when it ranks there (`founder` is an own role, 24.1). Source: the P5-01 world critic's F3 (30 of the 40 refuges listed in the default sweep had been found, one a capital of 5,410 found 1,900 years before E).

### 26.4 Region references by the present name (amends 6.1 "Ids" and "Sentences")

A region reference outside a sentence, `<name> (rg<id>)`, and the region name of a `[present]` region line (6.8 `rg<id> <name>: …`) print the region's **present name**: its name at year E (`HistoryView::region_name_at_year(r, E)`, P3-04 r21 23.1), which is `region.name` unless the region was renamed. Sentences print the name at the event (`render_event`), so a chronicle line of 3200 and the `home:` line of the same realm can name one region differently; the id ties them, and 26.2 lists every name a region bore.

### 26.5 Summary units and `[stats]` (amends 24.2 and section 9)

1. **Units** (24.2): part 2 gains, for every event e, each `region_renamed` that has e as its cause; part 4 gains, for a `region_renamed`, its cause. So a shown renaming is never without its deed, and a shown deed brings its renaming. A unit now has at most 10 events (24.2's 8 plus the renaming and its cause), and 24.3 item 5's bound reads 19 + 10.
2. **Keys** appended at the end of section 9's list (10.3 rule 1), in this order: `region_names` (rows of `region_name`; also a `[history]` key of the world report, P3-04 r21 23.4), `regions_renamed` (regions with at least one row), `refuges` (n of 26.3), `refuges_hidden` (h of 26.3). `SUMMARY_STATS` (25.7 item 4) gains `regions_renamed` and `refuges_hidden`, so the summary block has 17 keys, in `full`'s order.

### 26.6 Reading `region_name` (amends 5.3)

`read_history_tables` reads `region_name` in primary-key order and checks, after the `relation` checks, the first failing row in key order, reasons in this order (`corrupt table 'region_name': row (<region>, <from_year>): <reason>`): `unknown region <r>`; `from_year <y> outside 0 to <E - 1>`; `name is not a name` (empty, longer than 64 bytes, or a byte other than ASCII letters, space, `'` and `-`); `unknown culture <c>` (c ≠ 0 and no `culture` row); `event <e> is not a region_renamed of this region and year` (no such event, another kind, `event.region` ≠ r or `event.year` ≠ y). Then, over the events of kind `region_renamed` in ascending id: `corrupt table 'event': row <e>: region_renamed without a region_name row` when no row has `event` = e. Errors are values (code `corrupt_layer`, exit 3), as every check of 5.3.

### 26.7 One helper for settlement-indexed lookups (audit A1; no output change)

`history_report.cpp` reads the digest's per-settlement vectors (`last_held_by`, `refuge_hidden_end`) through one helper, `settlement_row(std::span<const T>, Id) -> const T*`, which returns null when the id is below 1 or above the span's size. A null result prints the documented fallbacks: `no one` for "last held by", and "not hidden" for a refuge (not listed in 26.3). Valid worlds never reach them (ids are dense from 1, 5.3), so no output changes; a later digest built from a filtered list gives defined text, not undefined behaviour (`docs/reviews/p5-01-followups-audit.md` A1).

### 26.8 Tests required (adds to 25.13)

1. Header v3 at both levels; `report --help`.
2. Renamed regions: a fixture with one region renamed twice and one renamed once: three lines, the twice-renamed region's first `formerly` its geographic name and its second `formerly` its first new name; n = 0 prints `renamed regions: 0`.
3. Hidden refuges: refuges with kind 42 ending at E − 1 (listed), earlier (counted in n, not listed), none (counted, not listed); the count line's both numbers.
4. References: a renamed region's `home:` and `[present]` lines print the present name while a chronicle sentence before the renaming prints the old one.
5. Units: a sack with its renaming at a threshold above the renaming's salience: both shown when the sack is; the renaming shown brings the sack.
6. `[stats]`: the four new keys at full; 17 summary keys in order.
7. Reader: each message of 26.6 on a corrupt fixture.
8. A1: `settlement_row` with ids 0, 1, size, size + 1.
9. Golden: `p3-07-report.txt` re-recorded by the reference (every report: the header at least); fixture **H12** (`h12-v3.toml`) holds 2 to 5; invariants 25.12 hold with 26.5's unit size.

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

## Revision note r11 (2026-11-20, the P3-09 validation's D2 and D3)

21.8 item 5 restated (P3-06 20.28, the lead's ruling). `wars_same_goal_unchanged_max` leaves out the wars closed by a side's end (`civ_conquered`, `civ_ended`), which neither extend nor break a run, so that it counts the same wars as P3-06 20.4's weariness and T2 follows from invariant (12). A run's window ends at the latest end in the run. An incremental form is given, in which the first war after a change of owner starts a run of 1. Worked examples: seed 6 (key 3) and `calm` seed 17 (key 2). Report key only: no world byte changes, and format stays v1.

## Revision note r12 (2026-11-20, quiet stretches, outbreaks and wording; for P3-10)

New section 22 (`docs/specs/p3-10-followups.md`, generator 0.19.0), before implementation: the report keys of kinds 37 and 38 and plague counts over kinds 12 and 37 (22.1); the generations note on a civilization's `quiet_stretch` milestone (22.2); the flare-up note reworded so that it agrees with plural realm names and stays the one place for the flare-up's parties (22.3); separators in sentences only (22.4); samples illustrative (22.5); repeated searches out of the summary and the run statistic by holder (22.5a, 22.5b, for P3-06 21.7 and 21.8); tests (22.6). No earlier section is renumbered.

## Revision note r13 (2026-11-21, after the P3-10 implementation)

22.2: the generations note is the P3-07 implementer's (P3-10 did not change `digest.cpp`); realms beyond the 64 participants of a `quiet_stretch` get none.

## Revision note r14 (2026-11-21, claims and cessions that name settlements; for P3-11)

New section 23 (`docs/specs/p3-11-followups.md`): kinds 39 `claim_settlements` and 40 `settlements_ceded` in `[stats]`, units and hollow wars, counted with kinds 20 and 29; the declaration's note names the claim's region when the war's goal differs from it (P3-06 r19 23.4); the flare-up note for a kind 39 claim; tests. Format v1 unchanged; no earlier section is renumbered.

## Revision note r15 (2026-11-22, the P3-11 world critic's F5)

23 item 4a: after a claim of kind 39 the declaration's note names the towns claimed (`The claim, since <y>, was to <S> in <R>.`), so that a war line read without its claim is not taken for a war over land; tests in item 5. A requirement on the report's notes; no world byte changes; not part of P3-11's merge.

## Revision note r16 (2026-11-22, text after the implementation and the audit)

Text only; the text follows what was ruled and built (`origin/p3/07-impl` 63972a3, tests `origin/p3/07-int`, CI `origin/p3/07-ci`, audit `docs/reviews/p3-07-history-report-audit.md`). No behaviour changes.
- 5.3: the relation check is `until` within `since` to E − 1 (the lead's ruling: 6.6 r6 stands, a row with `until` = `since` is valid).
- 5.3: the checks the implementation makes beyond the table, with their messages: row shapes of `region`, `biome`, `god`; populations above 2^31 − 1; `god.camp`; `race.creator_god`; the consistency of the two reads (5.2 item 4a, audit A2); out of memory. The order of the checks.
- 5.3: the upland flag and every per mille are free of overflow for every admitted value (audit S1, A1), with no new check.
- 4: the generic usage errors for `--history` and `--level` values (exit 2).
- 6.6: the milestone line form with its note.
- 21.4: `history_note`'s signature with the optional civilization argument.
- 9: `regions_conquered` counts kinds 26, 29 and 40.
- 14.4: the golden file's lines in the order the reference writes them, with the fixtures H1 to H7 and their table lines; section 16 is not compared.
- 14.5 item 16 and 18.6: CI saves and compares the report at both levels.
- 16: a current excerpt (seed 0, 1024, 0.20.0) in place of the 0.14.0 report.

## Revision note r17 (2026-11-22, readability after the world critic's review)

New section 24 (the world critic's A1 to A10 on `origin/p3/07-critic`; the lead's rulings of 2026-11-22). No world byte changes; generator 0.20.0; format `v1` is redefined before its first release.
- 24.1: the war of an event and a war's frame.
- 24.2: a war event brings its war's declaration, last battle, ending and cost, and a consequence brings its sack (A1); no orphans at any threshold.
- 24.3: claims, battles and plague spreads are not candidates at summary (A2, A8, A7); every quiet stretch is shown (A3); the age floor prefers the age's own race (A7).
- 24.4: the chronicle before the civilization blocks, `[phases]` at full only, phase headings with length, trigger and count (A9).
- 24.5: `seceded from`, `last capital`, a wars line, adversary lines at summary in place of relation rows (A4); milestones chosen per realm, own deeds first, at most two of a kind, the longest quiet stretch with its generations note (A5); `MILESTONES` 8.
- 24.6: `[ages]` separates awakenings and secessions (A6).
- 24.7: `[present]` lists ruins with their last owner, refuges, open wars and standing claims (A2, A9, A10).
- 24.8: what is left: the "grew into a city" number (template data), the plain reading form (to Paul).
- 24.10: invariants restated, with orphans = 0.
- 24.11: tests, among them the civs4 orphan count.
- 5.3: the three order and wording notes of the implementation (a standing settlement's population checked in 5.2 item 2; `expected 0` for an event or phase outside every phase or age).

Pointers in 6.2, 7.3, 8, 13, 16, 21.2 and 21.3. No earlier section is renumbered.

## Revision note r18 (2026-11-22, section 24's text after the implementation and the reference)

Text only; no behaviour changes. The implementation (`worldgen/src/history/digest.cpp`, `worldgen/src/inspect/history_report.cpp`) and the test writer's reference (`p3_report_reference.py`, readings R9 to R16) give byte-identical reports on 32 worlds and 12 fixtures; the text now states the readings both took.
- 24.1: war(e)'s case 2 gives war(c) also when war(c) is none; case 3 applies only to an event with no cause of a chain kind.
- 24.2: part 3 applies to every event with a war, the declaration included; the frame of a declaration excluded by 22.5a is empty.
- 24.3 item 3: the stop test before each candidate, a unit taken whole, a candidate already in the floor set skipped, the walk order as a stable partition of the rank order, every floor event counted toward its own age (R9).
- 24.4: a phase heading's count is every event of the phase, at both levels.
- 24.5: the wars line's side read from the row; a third party's `peace`, `war_won` or `civ_conquered` counts as drawn; `none` for an open row of another kind and `pl(k, war)` (R12); milestones: founding and ending taken outside E_c, each event once, `years` missing counted 0, the kind limit per kind id (R11).
- 24.6: w counts foundings of kind `civ_founded`; s = n − w (R15).
- 24.7: `no one` for an owner 0, `no owner` for a refuge (R13); an open war's region from its `since_event`; a faded line without the ruin part (R14); "pursued", and tests (ii) unordered with year ≥ y and (iii) ordered.
- 24.10 item 1: the sack and capture rule holds for candidates in their own right, as 24.2 says (R10).
- 14.1 item 4: H4's T is 77 with the quiet stretch (R16).
- 2, 14.4, 24.11 item 2: the CTest entries `worldgen-tests-history-report` and `worldgen-tests-slow-history-report`.
- 24.8: the "grew into a city" number is listed in P3-11's section 12.

## Revision note r19 (2026-11-22, format v2; P5-01, generator 0.21.0)

New section 25 (`docs/specs/p5-01-followups.md` items 1, 7 and 10; the world critic's B1 to B6 of `docs/reviews/p3-07-history-report-world-critic-r17.md`; the audit's A3). The header becomes `# worldgen history v2`, with P5's growth rule (25.1).
- 25.2: at most two calamity candidates per phase at summary (B1).
- 25.3: a ruin's kind in words; faded settlements counted apart in the settlements line (B2, B3).
- 25.4: adversary lines for realms with a war only, and one count line for the others (B3).
- 25.5: an ended realm's block in short at summary, with at most four milestones (B4).
- 25.6: every flare-up whose phase has a war is shown (B5).
- 25.7: a war's end year in the adversary line is the chronicle's; colonies before calamities in milestones; `[stats]` at summary cut to 15 keys (B6, A9).
- 25.8: the declaration's note dates the original claim behind a fresh claim (P3-06 r22 24.1).
- 25.9: refuges marked hidden or found from kind 42; standing claims closed by kind 41, so the list is exact.
- 25.10: `find` in place of `std::map::at` (A3).
- 25.11 to 25.14: constants, invariants, tests, the excerpt and the help text.

No earlier section is renumbered.

## Revision note r20 (2026-11-22, section 25's text after the P5-01 integration)

Text only; the implementation, the reference and the goldens on `p5/01-int` already follow every point. 25.2: the calamity cap ranks the four calamity kinds together per phase (salience descending, then id). 25.3 and 25.13 item 3: another ruin kind prints its key alone, with no `, by`; a sack with no attacker participant occurs only where the kind's attacker role has min 0 (under the shipped catalogue, 1–1, such a world is corrupt). 25.5: ended blocks show at most 4 milestones; T18 restated by Paul's decision of 2026-11-22 (default median ≤ 700, maximum ≤ 800; `civs4` median ≤ 1,100, maximum ≤ 1,400) with the 0.21.0 measurements. 25.7 item 4: `[stats]` at summary in full's key order (the list corrected); `settlements_ruined` counts status 3 only. 25.9: the refuge suffix from the `refuge_hidden` of the settlement with the smallest id, `; hidden` when `end_year` ≥ E − 1; an open claim is closed by a declaration in either direction, a `peace`, `truce`, `war_won` or `search_abandoned` of the pair, a `claim_dropped` citing it, or a side's end (T19). 25.13: the note's year is root(c)'s; the new fixture is H11 (`h11-v2.toml`).

## Revision note r21 (2026-11-22, format v3; P5-12, generator 0.22.0)

New section 26 (`docs/specs/p5-12-renamed-regions.md` items 6 to 8): header v3; `[present]` lists renamed regions with their former names (26.2) and only the refuges still hidden at E (26.3, the P5-01 critic's F3); region references outside sentences print the present name (26.4); a renaming and its cause are companions, four `[stats]` keys (26.5); `read_history_tables` checks `region_name` (26.6); one bounds-checked helper for the settlement-indexed lookups (26.7, audit A1).
