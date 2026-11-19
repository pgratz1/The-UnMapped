# P3-03 Chronology

Status: **approved by the lead 2026-11-19** (the six questions of section 16 go to Paul; the interim choices stand); **revised 2026-11-19 after implementation** (text, and check D0 by the lead's decision): see the revision note at the end. Written by the spec writer 2026-11-19. Phase P3 History core, spec 3 (P3-00 3.1), pipeline stage 7, `chronology`. Generator **0.13.0**; **schema version 6** (five new tables and eight new required `meta` keys, section 5). Design doc sections 5 (the six ages, gods and the order of the races, the awakening table, world-creation settings), 6 (tables `race`, `god`, `age`; `meta` holds the settings) and 14 (every name a model writes must resolve: names unique). Depends on P0-01 to P0-04 with their amendments, P1-06 (table `region`), P3-00 (binding: 2.1 calendar, 4.5 H1–H8, 4.6 keyed draws, 5, 6.1, 6.5, 12.3, 12.4), P3-01 (kinds `race`, `culture`, `history.chronology`; `Settings`) and P3-02 (keyed draws, names, registry). Implementation state read: `origin/p3/01-int` (`data::Settings` carries the history settings, `WorldParams` does not) and `origin/p3/02-names` (`worldgen/history/names.hpp`). Proposals read: up to "Paul's decisions of 2026-11-19: the P3 interim choices are accepted; names are ASCII and unique". Numbers in this spec come from the spec writer's exact-integer prototype `~/.cache/the-unmapped/out/p3-03-spec/chrono_ref.py` (on `core_reference` and the P3-02 reference) and from a 0.12.0 world, `~/.cache/the-unmapped/out/p3-03-spec/s42-1024.db`.

## 1. Purpose

Fix the frame of every history before anything is simulated: which races are present, the year each one awakens (the design's shares of the span, shifted a little by the seed, the order never changed), the ages those awakenings open, one named creator god per race, and an echo of the race and culture data the world was made with. It also records the eight world-creation settings in `meta`. The `history` stage (P3-04 on) reads these tables and settings from the stored world (H8); the viewer, the history report and the enrichment pass of design 14 read them to name the ages, gods and peoples.

## 2. What this spec touches (for the engineer)

| Area | Files |
| --- | --- |
| Settings to the pipeline | new `worldgen/include/worldgen/history/params.hpp`, `worldgen/src/history/params.cpp` (`HistoryParams`, 4.1); `worldgen/include/worldgen/generate.hpp` (`WorldParams::history`); `worldgen/src/generate.cpp` (writes the eight keys) |
| Database | `worldgen/include/worldgen/db/schema.hpp` (version 6, five DDL statements, 16 required keys, canonical forms), `worldgen/src/db/schema.cpp`, `writer.hpp`/`writer.cpp` (rows, `set_param_text`), `reader.cpp` (value checks) |
| Stage | new `worldgen/include/worldgen/stages/chronology.hpp`, `worldgen/src/stages/chronology.cpp`; `worldgen/include/worldgen/stages/stage.hpp` and `src/stages/stage.cpp` (stage list, `StageContext::regions()`, `history_params()`, row writers) |
| Data checks | new `worldgen/include/worldgen/history/data_checks.hpp`, `worldgen/src/history/data_checks.cpp` (section 6) |
| Streams | `worldgen/include/worldgen/core/streams.hpp`: `hist.chronology.awakening` |
| Report | `worldgen/src/inspect/report.cpp` (and a reader of the new tables in `inspect/`): the `[chronology]` section (9.1) |
| CLI | `tools/worldgen-cli/cli.cpp`: `HistoryParams` from `Settings`; the data checks (exit 3); the eight stage names in the `--until` message |
| CI | `tools/ci/world-hashes.sh`: two `hist-*` cases (13); golden `worldgen/tests/golden/p3-worlds.txt` |
| Version | `worldgen/include/worldgen/version.hpp`: **0.13.0** |
| Docs | `docs/specs/schema.md` (amendment list, 15.1) |
| Tests | `worldgen/tests/history/` (the `worldgen-tests-history` group of P3-02), `tools/worldgen-cli/tests/`; reference `worldgen/tests/reference/p3_chronology_reference.py`; golden `worldgen/tests/golden/p3-03-chronology.txt` |

No data file changes: the six races, six cultures and `history/chronology.toml` of P3-01 are used as shipped.

## 3. Inputs

| Input | Source |
| --- | --- |
| World seed | `meta.seed` (`StageContext::seed()`) |
| History settings | `WorldParams::history` (4.1), the same values the world records in `meta` (`StageContext::history_params()`) |
| Race, culture, biome definitions | kinds `race`, `culture`, `biome` (P3-01 3, 4; P1-04 4.1) |
| Tuning | definition `history.chronology` (P3-01 5.1): `awakening_jitter_frac`, `awakening_min_gap_years`, `age_name_form` |
| Region names | table `region` as written by the regions stage, in id order (`StageContext::regions()`, a new accessor like `passes()`) |

The stage reads no layer. Of the geography it reads only the region names, and only to keep names unique (8.1); with the shipped data no region name can equal a god or age name (their forms differ: two or three capitalized words against one word or `The Age of …`), so on shipped data the chronology is a function of the seed, the settings and the data alone. This is a property to test (14.3 item 21), not a rule to rely on.

Streams: **K_aw = `stream_key(seed, "hist.chronology.awakening")`** (new; registered in `streams.hpp`) and `hist.names.god` (P3-02 4.5). No `Rng`, no noise, no `hash_at`.

## 4. Settings and `meta`

### 4.1 How the settings reach the stage (the question P3-01 left open)

**Decision: a new value type `history::HistoryParams` in `WorldParams`, not `data::Settings`.** `WorldParams` is "everything a world depends on" (P0-04 10) and never holds file positions or unresolved input; `Settings` holds `races_origins`, an optional seed and `races` before resolution, which belong to loading. So:

```cpp
// worldgen/include/worldgen/history/params.hpp
namespace worldgen::history {
struct HistoryParams {
    std::int64_t span = 6000;                                   // 2000 … 12000
    data::HistoryDetail detail = data::HistoryDetail::normal;
    std::optional<std::int64_t> civs_per_race;                  // nullopt = auto; else 1 … 4
    std::vector<std::string> races = {"elf", "goblin", "dwarf", "faerie", "human", "orc"};  // present, race order
    data::EventIntensity event_intensity = data::EventIntensity::normal;
    data::MonsterPressure monster_pressure = data::MonsterPressure::normal;
    data::DivineActivity divine_activity = data::DivineActivity::normal;
    std::string divine_script;                                  // empty in P3
    friend bool operator==(const HistoryParams&, const HistoryParams&) = default;
};
HistoryParams history_params_from(const data::Settings&);       // FatalError `history_params_from: the races are not resolved` unless resolve_races ran
std::vector<std::pair<std::string_view, std::string>> history_meta_entries(const HistoryParams&);  // the eight keys of 4.2, ascending key
HistoryParams history_params_from_meta(const db::WorldReader&); // inverse of history_meta_entries (for resume tests and P3-04);
                                                                // FatalError `history_params_from_meta: meta key '<key>' is missing or not canonical`
}
// worldgen/include/worldgen/generate.hpp: struct WorldParams { …; history::HistoryParams history; };
```

The default `races` is the design's roster in its fixed order (design 5); the CLI always replaces it with `resolve_races`' result. Flow:

1. `generate` (CLI): settings and flags are merged and `resolve_races` runs (P3-01, as on `p3/01-int`); then `params.history = history_params_from(settings)`.
2. `generate_world` checks `params.history` before any work (with the existing checks of `params`): span in [2000, 12000], `civs_per_race` unset or in [1, 4], `races` non-empty, each in the definition id grammar of P0-03, no id twice, `divine_script` empty or 64 lower-case hex digits; FatalError `generate_world: params.history.<field> is invalid` otherwise, `<field>` the first invalid one of `span`, `civs_per_race`, `races`, `divine_script` (`history_params_problem`, 14). Whether the ids name loaded races, in race order, is the stage's check (7, C1), since a world that does not run the stage needs no race data.
3. `generate_world` writes the eight keys into `meta` for **every** world, with the other world-creation settings (after `param.map_size`), through `WorldWriter::set_param_text(name, value)` with the text of `history_meta_entries`. So the empty world, the test-pattern world and a geography-only world record them too (P3-00 5: "every world records them").
4. The pipeline hands the same `HistoryParams` to each stage's context: `StageContext::history_params()`. Since the stage sees exactly what `meta` records, a run resumed from a stored world (`history_params_from_meta`) sees the same values (H8).

`WorldWriter::set_param_text(name, value)`: sets `meta.param.<name>` = value; FatalError if `param.<name>` is one of the eight keys and the value is not in its canonical form (4.2), `WorldWriter::set_param_text: '<value>' is not the canonical form of a value of meta key 'param.<name>'`, or the name is invalid (as `set_param_integer`); `map_size` stays integer-only (`WorldWriter::set_param_text: map_size is an integer parameter`, as `set_param_decimal` already refuses it). `set_param_integer` and `set_param_decimal` apply the same canonical-form check to the eight keys, with the same text under their own name (`WorldWriter::set_param_integer: '<value>' …`, `WorldWriter::set_param_decimal: '<value>' …`, the value as the writer would store it). Every required `param.*` key must be set before `finalize`: FatalError `WorldWriter::finalize: meta key '<key>' was not set`, naming the first missing one in ascending key order; this also replaces the old message for `param.map_size` (`WorldWriter::finalize: param.map_size not set`). The test helper `write_fixture_world` sets the defaults of `HistoryParams`.

### 4.2 The eight keys and their canonical text

| Key | Canonical value | Written from |
| --- | --- | --- |
| `param.civs_per_race` | `auto`, or `1`, `2`, `3`, `4` | `civs_per_race` (nullopt → `auto`) |
| `param.divine_activity` | `distant`, `normal` or `meddling` | `divine_activity_name` |
| `param.divine_script` | empty; (reader also accepts 64 lower-case hex digits, the P5 form) | `divine_script` (always empty in P3) |
| `param.event_intensity` | `calm`, `normal` or `turbulent` | `event_intensity_name` |
| `param.history_detail` | `sparse`, `normal` or `dense` | `history_detail_name` |
| `param.history_span` | decimal integer 2000 … 12000, canonical (P0-02 5.5: no sign, no leading zero) | `span` |
| `param.monster_pressure` | `low`, `normal` or `high` | `monster_pressure_name` |
| `param.races` | one or more ids in the P0-03 definition id grammar, joined by `,` with no spaces, none twice, e.g. `elf,goblin,dwarf,faerie,human,orc` | `races` (race order, P3-01 6.3; also when `all` was given) |

Defaults (no flag, no settings key): `auto`, `normal`, empty, `normal`, `normal`, `6000`, `normal`, and every loaded race in race order (with the shipped data the six ids above).

The reader validates the eight values at open, as it validates `param.map_size` (P0-02 amendment 2): a value outside its canonical form is "meta value invalid", exit 3, with the reader's existing message `world file '<path>' has an invalid value '<value>' for meta key '<key>'`; a missing key is the error value `meta_key_missing` with the reader's existing text `world file '<path>' has no meta key '<key>'` (exit 3). The reader cannot know the data, so it checks only the syntax of `param.races`; that its ids are the present rows of table `race` in id order is invariant 1 (10).

`REQUIRED_META_KEYS` becomes, in ascending byte order (the order the writer inserts them): `content_hash`, `format`, `generator_version`, `param.civs_per_race`, `param.divine_activity`, `param.divine_script`, `param.event_intensity`, `param.history_detail`, `param.history_span`, `param.map_size`, `param.monster_pressure`, `param.races`, `schema_version`, `seed`, `seed_phrase`, `stages` (16 keys).

### 4.3 What each kind of world contains at schema 6

| World | Eight `param.*` keys | Five new tables | `meta.stages` |
| --- | --- | --- | --- |
| default `generate` (every implemented stage) | present | filled | `elevation,hydrology,climate,biomes,resources,regions,chronology` |
| `--until chronology` | present | filled | the same; the file is byte-identical to the default world while `history` is not implemented |
| `--until regions` (geography only) and every earlier limit | present | present, empty | the stages run |
| `--until none` (the P0 empty world) | present | present, empty | empty |
| `--test-pattern` | present (the flags given, or the defaults) | present, empty | `test_pattern` |

`--test-pattern` with history flags: the flags are accepted, have no effect on any table or layer, and **are recorded in `meta`** (this refines P3-01 6.2, "accepted and have no effect": the world records the settings it was created with; amendment 15.7).

`--until history` is a usage error, exit 2, `stage 'history' given to --until is not implemented yet` (P1-00 10.1's existing case). An unknown name lists the eight stage names: `unknown stage '<x>' given to --until; expected none or a stage name (elevation, hydrology, climate, biomes, resources, regions, chronology, history)`. The stage list `GEOGRAPHY_STAGE_NAMES` becomes the full list of eight names (rename to `STAGE_NAMES`; the implemented list gains `chronology`).

### 4.4 Readers of schema 5

`SCHEMA_VERSION`, `user_version` and `meta.schema_version` become **6**. A reader of version 6 refuses version 5 files: `world schema version 5, this build reads 6` (exit 3), the same text whether `user_version` or `meta.schema_version` is the one found to differ (P0-02 amendment 16's rule). Worlds are regenerated (design 6: no migrations).

## 5. Outputs

### 5.1 Schema change: version 5 → 6

Five statements are added to the DDL, executed after `region_biome`'s (ninth to thirteenth), byte for byte:

```
CREATE TABLE race (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT NULL, adjective TEXT NOT NULL, camp INTEGER NOT NULL, lifespan_years INTEGER NOT NULL, generation_years INTEGER NOT NULL, culture INTEGER NOT NULL, present INTEGER NOT NULL, awakening_year INTEGER, creator_god INTEGER NOT NULL) STRICT
CREATE TABLE race_biome (race INTEGER NOT NULL, biome INTEGER NOT NULL, weight INTEGER NOT NULL, PRIMARY KEY (race, biome)) STRICT, WITHOUT ROWID
CREATE TABLE culture (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, race INTEGER NOT NULL) STRICT
CREATE TABLE god (id INTEGER PRIMARY KEY, name TEXT NOT NULL, camp INTEGER NOT NULL) STRICT
CREATE TABLE age (id INTEGER PRIMARY KEY, race INTEGER NOT NULL, start_year INTEGER NOT NULL, end_year INTEGER NOT NULL, name TEXT NOT NULL) STRICT
```

Every world of schema 6 has the five tables, empty when the stage did not run (4.3). `race.key` and `culture.key` are `UNIQUE`, as `biome.key` is (the echo tables are looked up by key), so SQLite adds `sqlite_autoindex_race_1` and `sqlite_autoindex_culture_1`; names are not `UNIQUE` (as `region.name`; uniqueness is invariant 8, checked by tests). **The empty world grows from 11 to 18 pages, 73728 bytes**: the root pages of `race`, `sqlite_autoindex_race_1`, `race_biome`, `culture`, `sqlite_autoindex_culture_1`, `god` and `age` (measured with `VACUUM INTO` on the 0.12.0 empty world plus the five statements and the eight `meta` rows, and confirmed on the `--until none` world written by the vendored SQLite 3.54.1, 14.5). The `sqlite_schema` text is then 2928 bytes, still on page 1 (P3-04 adds thirteen tables and must measure again). No foreign-key clauses; references are invariants (10). `awakening_year` is the only nullable column of P3 so far (P3-00 6.1 conventions: NULL only for a year that has not happened). No REAL, no BLOB.

### 5.2 Columns

**`race`**: one row per loaded `race` definition (six with the shipped data), present or not, in `id` order.

| Column | Value |
| --- | --- |
| `id` | the definition's `order` (1 … n) |
| `key` | the definition id (`elf`) |
| `name`, `plural`, `adjective` | the definition's words |
| `camp` | 0 good, 1 neutral, 2 evil |
| `lifespan_years`, `generation_years` | from the definition |
| `culture` | `culture.id` of the definition's `culture` (5.2 `culture`) |
| `present` | 1 if the id is in `param.races`, else 0 |
| `awakening_year` | the year of C3 when present, NULL when not |
| `creator_god` | the id of the race's creator god: **= `race.id` for every row**, present or not (C5) |

The other race fields (`awakening_share`, growth, site and biome weights, `upland`, `coast`, `expansion`, `aggression`, `settlement_kinds`) are tuning that the `history` stage reads from the loaded data (H8 lists "the loaded data"); they are in `content_hash` and not echoed, except the biome preferences, which readers need to explain homelands.

**`race_biome`**: one row per entry of each race's `biomes` array, every race, in (`race`, `biome`) order: `race` = `race.id`, `biome` = the referenced definition's `code` (= `biome.id`), `weight` as listed (zero weights included; a biome not listed has weight 0 and no row). The loader's `race_biomes_unique` makes the key unique. 45 rows with the shipped data (elf 7, goblin 6, dwarf 7, faerie 8, human 8, orc 9).

**`culture`**: one row per loaded `culture` definition, in `id` order: `id` = 1, 2, … in canonical id order (ascending id bytes; shipped: `dwarven` 1, `elven` 2, `faerie` 3, `goblin` 4, `human` 5, `orcish` 6, the numbering P3-02 4.2 calls c), `key` the definition id, `name`, `race` = `race.id` of its `race`. The naming data stays in the content hash, not in the table (P3-00 6.1).

**`god`**: one row per race definition, `id` = the race's id, `name` by C5, `camp` = the race's camp. **Absent races keep their creator god** (decision, question 2): the pantheon of a world does not depend on which races were chosen, so a race's god keeps its name and id across `--races` subsets (a name is keyed by the god's id, P3-02 4.5), and `creator_god` is never 0. A god whose race is absent created nothing in this world; P5's pantheon adds gods that create nothing anyway.

**`age`**: one row per **present** race, in race order: `id` = 1, 2, … , P (dense, H2), `race` = that race's id, `start_year` = its awakening year, `end_year` = the next present race's awakening year − 1, or E − 1 for the last, `name` by C4. With all six races present, `age.id` = `race.id`; with a subset they differ (`--races elf,orc`: ages 1 and 2 with `race` 1 and 6). This replaces P3-00 2.1's "age ids 1–6" for subsets (amendment 15.6).

### 5.3 Writing

One stage transaction (P0-02): rows of `race` (id order), `race_biome` ((race, biome) order), `culture` (id order), `god` (id order), `age` (id order), through new `StageContext::write_race`, `write_race_biome`, `write_culture`, `write_god`, `write_age` (each in strictly ascending key, FatalError otherwise, as the region writers), with row types `db::RaceRow` (`awakening_year` a `std::optional<std::int64_t>`), `db::RaceBiomeRow`, `db::CultureRow`, `db::GodRow`, `db::AgeRow`, columns in declared order.

## 6. Data checks across kinds

P3-01 3 deferred to this stage the conditions a per-kind check cannot see. **They run in two places:** in `generate`, right after the data is loaded and before anything else uses it (also for `--until none` and `--test-pattern`, since invalid data is invalid whatever the plan), as an **input error, exit 3**; and at the stage entry as a seam guard (FatalError, C1; unreachable through the CLI). A library function returns them:

```cpp
// worldgen/include/worldgen/history/data_checks.hpp
std::vector<data::LoadError> check_history_data(const data::ContentSet&);   // sorted as P0-03 9
```

Each error has path `races` (the kind directory), no position, and one of these messages; D0 is evaluated first, once for the data; D1 to D5 for every race definition (present or not), in race order, each condition in this order; the list is then sorted and deduplicated by the P0-03 9 rule (so, sharing path and position, by message bytes):

| # | Condition (fails when) | Message |
| --- | --- | --- |
| D0 | the data has no `race` definition | `no race is defined; kind 'race' needs at least one definition` (new; the lead's decision 2026-11-19) |
| D1 | a `biomes[i].biome` has `water` true | `race '<id>' prefers water biome '<biome id>'` (P3-01 3 item 1) |
| D2 | a `biomes[i].biome` has `water` false but is not **natural**: natural = `fallback` true or `rules` non-empty, i.e. a biome the biome stage can assign (`blighted_waste` is not: `rules = []`, `fallback = false`; history assigns it, P1-04 1) | `race '<id>' prefers biome '<biome id>', which no geography stage assigns` (new) |
| D3 | the race's `culture` has `race` ≠ the race | `race '<id>' has culture '<culture id>' of race '<other id>'` (P3-01 3 item 2) |
| D4 | the race's age name (C4 step 1, before any numeral) is longer than 48 bytes | `race '<id>' has the age name '<name>', which is longer than 48 bytes` (new) |
| D5 | the race's age name has a byte other than `A`–`Z`, `a`–`z`, space, `'`, `-`, or starts or ends with a space, or has two spaces in a row | `race '<id>' has the age name '<name>', which is not ASCII letters with single spaces, ' and -` (new; Paul's decision 13, names ASCII) |

D0: a data root with zero race definitions is an input error (exit 3 in the CLI). Through the loader it is caught earlier: kind `race` is required, so a `races/` directory without definitions is already the loader's error `required kind 'race' has no definitions in 'races/'` (P0-03, exit 3, printed under `invalid data in '<data root>'`), and a missing directory `the directory of required kind 'race' is missing`; D0 guards a `ContentSet` built otherwise. At the stage entry such data is refused by C1 item 3, which comes before item 4 (`chronology: settings: no race present` for an empty list, else `chronology: settings: unknown race '<id>'` for its first id), so D0's own text is not what the stage prints. `generate_world`'s FatalError `generate_world: params.history.races is invalid` (an empty `races`, 4.1 step 2) remains the last line of defence.

P3-01 3 item 3 ("every race has at least one culture") is implied by D3: `culture` is a required ref, and if it names a culture of this race the race has one. It has no message of its own.

D4 and D5 exist because `age_name_form` is `string(64)` without a grammar and the plural and adjective are up to 32 bytes each: the shipped form cannot fail (`The Age of Faeries`, 18 bytes), but an edited form could produce a name the P3-02 contract does not allow (48 bytes leaves room for a numeral, P3-02 4.4). A name produced in D4 or D5's message is printed as it is; the loader's string rules already exclude control characters.

CLI form (P0-04 4.5): `worldgen-cli: error: invalid data in '<data root>'`, `<data root>` printed exactly as the loader's own `invalid data in` error prints it: the root as resolved and not normalized (the `--data` value as given, else `UNMAPPED_DATA_DIR`, else the `game/data` path recorded at configure time; P0-03 7.6, P1-00 6.3); then one line per error, `races: error: <message>`, exit 3, no world written. With the shipped data and the two fixture roots none fails; the tests make each fail once (14.1 item 6).

## 7. Algorithm

E = `span`; the present races r_1 … r_P are the definitions whose ids are in `races`, in race order (ascending `order`); n = the number of race definitions. `Fx` is Q32.32, every rounding RHA (P0-01 5). Steps C1 to C6.

### C1. Entry checks (fatal, in this order)

The stage entry `run_chronology` applies all four items. The seam `chronology_stage` (14) applies items 2 to 4 only and accepts an empty region list (14.3 item 21); `run_chronology` checks item 1 and then calls it.

1. `chronology: input precondition: no region rows` if no `region` row was written by an earlier stage (the stage needs the regions stage; a world always has at least one region).
2. Tuning (seam guards, unreachable with loaded data): each field inside its schema range of P3-01 5.1 (`chronology: tuning: <field> outside its schema range`, declaration order), then the named check (`chronology: tuning: age_name_form must contain {plural} or {adjective} and no other placeholder`).
3. Settings (seam guards), checked in this order: span, empty list, unknown ids (the first in list order), race order: `chronology: settings: history_span <E> outside 2000 to 12000`; `chronology: settings: no race present` (empty list); `chronology: settings: unknown race '<id>'` (first id, in list order, with no definition); `chronology: settings: races not in race order at '<id>'` (the first id whose order is not above the previous one's; a repeated id fails here).
4. Data: `chronology: data: <message>` with the first error of `check_history_data` (6).

### C2. Bounds of the schedule

```
G    = min(awakening_min_gap_years, E div P)          // effective least gap; = the tuning value whenever P × gap ≤ E
jmax = round_int(mul_int(awakening_jitter_frac, E))   // largest shift either way
```

With the shipped tuning (`"0.01"`, raw 42949673; gap 100): jmax = 20 / 60 / 120 at E = 2000 / 6000 / 12000, and G = 100 for every span and subset (E div P ≥ 333).

### C3. Awakening years (sequential, race order)

```
base(r) = round_int(mul_int(r.awakening_share, E))
y_1 = 0                                                // the first present race opens history, whatever its share
for i = 2 … P:
    u   = draw(K_aw, 0, r_i.id, 0, 0)                  // P3-02 4.1; w0 = year 0 (P3-00 4.6), a = race id
    d   = pick(u, 2 × jmax + 1) − jmax                 // uniform in [−jmax, jmax]
    lo  = y_{i−1} + G
    hi  = E − G × (P − i + 1)
    y_i = min(max(base(r_i) + d, lo), hi)
```

`lo` ≤ `hi` always (by induction, y_{i−1} ≤ E − G × (P − i + 2), and y_1 = 0 ≤ E − G × P since G ≤ E div P), so the clamp is well defined; it keeps the order strict, consecutive awakenings at least G years apart, and the last age at least G years long. **With the shipped data the clamp never acts** for any seed and span (the closest shares, faeries 0.70 to humans 0.83 and humans to orcs 0.93, leave at least 0.10 E − 2 jmax = 0.08 E; measured on seeds 0–1999 at spans 2000, 6000, 12000: no clamp, least gap between consecutive awakenings 161 / 483 / 966 years). It exists for edited data and short spans with large tuning values.

**Subsets.** The first present race awakens at year 0, whatever its share (year 0 is "the start of history", P3-00 2.1; ages must tile [0, E)); each later present race keeps its own share and its own draw, keyed by its race id, so a race's year does not depend on which other races are present except through the clamp. `--races goblin,orc`: goblins at 0, orcs at 0.93 E ± jmax. Decision of this spec; question 1.

The draw keys by `race.id` (= `order`), not by the position in the present list. The race of order 1 has share 0 by P3-01's `race_awakening_order` check, so with elves present y_1 = 0 = base: "elves at year 0 always" (design 5).

### C4. Ages (sequential, id order)

For i = 1 … P: `start_year` = y_i, `end_year` = y_{i+1} − 1 (i < P) or E − 1 (i = P), `race` = r_i.id, and the name:

1. a = `age_name_form` with every `{plural}` replaced by r_i's `plural` and every `{adjective}` by its `adjective`, scanning left to right, replacement text not rescanned (shipped: `The Age of Elves`);
2. if a is not in the registry, the name is a; otherwise the first of a + " " + roman(k), k = 2, 3, …, that is not (P3-02's fallback and `roman`); the name is added to the registry.

### C5. Creator gods (sequential, race id order)

For every race definition r (present or not), in id order: god `id` = r.id, `camp` = r.camp, `name` = `make_name(naming of r's culture, r.culture's id, NameKind::god, seed, c = culture.id of r's culture, e = r.id, registry)` (P3-02 4.3; entity id e = `god.id`, P3-02 4.5). `race.creator_god` = r.id.

### C6. Write

Section 5.3.

### 7.1 Worked examples

Computed by the prototype; the test writer's reference must reproduce them independently. Shipped data and tuning, no region name colliding.

**Seed 42, span 6000, all races.** K_aw = 0xA7284FA6D2415A60; jmax 60 (pick over 121 values); share raws: goblin 1503238554, dwarf 2362232013, faerie 3006477107, human 3564822856, orc 3994319585.

| Race | base | u | pick | d | year | Age (id, years, name) | God (id, name, attempt) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| elf | 0 | — | — | — | **0** | 1, 0–2073, The Age of Elves | 1, **Fielie**, 0 |
| goblin | 2100 | 0x48F251C73BC89E2C | 34 | −26 | **2074** | 2, 2074–3241, The Age of Goblins | 2, **Krok**, 0 |
| dwarf | 3300 | 0x04C4CCA630137EBA | 2 | −58 | **3242** | 3, 3242–4242, The Age of Dwarves | 3, **Bronhak**, 1 |
| faerie | 4200 | 0xDA392A12D57DEAEB | 103 | 43 | **4243** | 4, 4243–4968, The Age of Faeries | 4, **Syfiash**, 0 |
| human | 4980 | 0x67CC0B87B4CE8D5A | 49 | −11 | **4969** | 5, 4969–5595, The Age of Humans | 5, **Wealstuld**, 0 |
| orc | 5580 | 0xA1211F03C3A8FF14 | 76 | 16 | **5596** | 6, 5596–5999, The Age of Orcs | 6, **Kruu**, 0 |

The 1024 world of seed 42 (0.12.0, 50 regions, `North Downs`, `North Downs II`, …) changes nothing here: no region name equals a god or age name.

**Seed 0, span 6000, all races**: years 0, 2068, 3265, 4178, 5003, 5534 (d = −32, −35, −22, 23, −46); gods Saene, Kruskux, Nurnthaimm, Whaia (attempt 1), Steldhil, Bald. (Saene, Kruskux and Nurnthaimm are the god samples of P3-02 5.1 for entities 1, 2 and 3 of their cultures.)

**Seed 1, span 6000**: years 0, 2071, 3341, 4178, 4934, 5580 (d = −29, 41, −22, −46, 0); gods Laesiel, Gragg-Gnurk, Khummhor, Synmia, Steck, Kaag.

**Seed 7, span 2000** (the `hist-span-2000-512` case): jmax 20, bases 0, 700, 1100, 1400, 1660, 1860; years 0, 688, 1114, 1409, 1644, 1879; ages end 687, 1113, 1408, 1643, 1878, 1999; gods Vanir, Grux, Mammga, Ouwheefy, Gord, Maark'zaagh.

**Seed 3, span 6000, `--races elf,orc`** (the `hist-races-elf-orc-1024` case): orc u = 0xAEA8CB7C6C7550B5, d = 22, year 5602; ages (1, race 1, 0–5601, The Age of Elves), (2, race 6, 5602–5999, The Age of Orcs); six gods Vaennela, Buzz, Nonthom (attempt 1), Pyia, Alis (attempt 1), Uguuggrush; `race` rows 2–5 `present` 0, `awakening_year` NULL, `creator_god` 2–5.

**Seed 42, `--races goblin,dwarf,human`**: goblins at 0 (base 2100 ignored), dwarves 3242 and humans 4969 (the same draws as in the full world); ages (1, race 2, 0–3241), (2, race 3, 3242–4968), (3, race 5, 4969–5999); gods as in the full world (Fielie … Kruu).

**Seed 2^64 − 1, span 12000, `--races orc`**: one age (1, race 6, 0–11999, The Age of Orcs); gods Ciefa, Slogob, Narinnadin, Oupoubia, Hard, Kruurz.

**Collisions** (seed 42, span 6000, all races, registry preloaded with the region names `Fielie`, `The Age of Elves`, `Krok`, `Krok II`): age 1 is **The Age of Elves II**; god 1 is **Mical** (attempt 2: attempt 0 gives `Fielie`, taken), god 2 is **Zatch** (attempt 1).

**Clamp** (hand-made data, span 6000, gap 100, jitter 0): shares 0, 0.5, 0.501, 0.999 → bases 0, 3000, 3006, 5994; years 0, 3000, 3100 (lo), 5900 (hi = 6000 − 100). With gap 2000 and six races at span 2000: G = 333.

## 8. Names, the registry and what the history stage rebuilds

### 8.1 Order of naming

One `NameRegistry` per run of the stage, filled in this order:

1. every `region.name`, in id order (P3-02 4.4);
2. the age names, in age id order (C4);
3. the god names, in god id order (C5).

Ages are named before gods because their names are fixed text: a drawn name can move to another attempt, a fixed one only to a numeral. Race and culture words (`Elf`, `Elves`, `Elven`) are data vocabulary, not names of entities, and are not in the registry (a generated name equal to one is harmless: every reference in the log is by type and id).

### 8.2 Rule for later stages (H8)

**The registry after the chronology stage is exactly the set of names in tables `region`, `age` and `god`.** The registry is a set used for membership only (P3-02 7: never iterated), so its content, not its insertion order, determines every later name. The `history` stage (P3-04) therefore rebuilds it from the stored world by adding every `region.name`, `age.name` and `god.name` (any order), then adds each name it generates (civilizations, settlements) in creation order. P3-04 states this as its first step; `--until chronology` followed by `history` on the read-back world then names everything as one run does (H8, Y2). Any later stage that adds a named table extends the rule with that table.

## 9. Report, hash and viewer

### 9.1 `report`

`[tables]` lists the five tables with the others, ascending name: `age`, `biome`, `climate`, `culture`, `god`, `layer`, `meta`, `pass`, `race`, `race_biome`, `region`, `region_biome`, `region_border` (empty world: 13 lines, `meta = 16`, the rest 0). `[meta]` prints the eight keys with the others in key order (`param.divine_script =` with no trailing space).

A new section **`[chronology]`**, after `[geography]` (or after `[layers]` when there is no `[geography]`), printed when table `age` has at least one row. Keys, in this order, integers only:

| Key | Value |
| --- | --- |
| `races_present` | rows of `race` with `present` = 1 |
| `awakening_years` | `awakening_year` of the present `race` rows in id order, separated by single spaces |
| `age_years` | `end_year` − `start_year` + 1 of each `age` row in id order, separated by single spaces |
| `age_years_min` | the smallest of them |
| `gods` | rows of `god` |

Example, 1024 seed 42 (prototype):

```
[chronology]
races_present = 6
awakening_years = 0 2074 3242 4243 4969 5596
age_years = 2074 1168 1001 726 627 404
age_years_min = 404
gods = 6
```

The report format stays **v2** (P1-00 appendix C item 20: the number changes once a format has shipped in recorded goldens; no golden records report text). Names are left to the history report (P3-07 `[ages]`).

**Corrupt data**, each exit 3 with nothing on stdout, checked after every `[geography]` check, whenever table `race` or `age` has rows, first failure reported, in this order: (1) `race` rows in id order: `corrupt table 'race': row <id>: present is <v>` (not 0 or 1), `corrupt table 'race': row <id>: awakening_year does not match present` (NULL exactly when `present` is 0); (2) `age` rows in id order, with E = `param.history_span`: `corrupt table 'age': row <id>: <reason>`, reasons `id not consecutive` (the first row's id is not 1 or a row's id is not the previous + 1), `race <r> is not a present race`, `start_year <s>, expected <x>` (x = 0 for the first row, else the previous `end_year` + 1), `end_year below start_year`, `race <r> awakens in <y>` (its `awakening_year` ≠ `start_year`); then `corrupt table 'age': the last age ends in <e>, expected <E − 1>` (when `age` has rows) and `corrupt table 'age': <rows> rows for <p> present races`. The row-count check also applies when table `age` is empty and `race` has rows: present races without age rows are corrupt (`corrupt table 'age': 0 rows for <p> present races`, p ≥ 1), because a chronology run writes both tables and a world stopped before the chronology has both empty. With both tables empty there is no `[chronology]` section and no check (and with `race` rows none of which is present and `age` empty, `0 rows for 0 present races` holds and no section is printed). Before these value checks each row's shape is checked: `corrupt table '<t>': row <id>: has <k> columns, expected <n>`, `… column <k> is not text`, `… column <k> is not an integer` (only `race.awakening_year` may be NULL; `<id>` is `?` when the id itself is not an integer). `report` does not recompute the schedule from the data.

### 9.2 `hash --detail`

Unchanged rules (P0-04 8): one `table` line per table in ascending name, so the five new lines `table age`, `table culture`, `table god`, `table race`, `table race_biome` fall among the others; `awakening_year` NULL is the column value tag `0x00`. The `schema` line changes with the DDL.

### 9.3 Viewer

**None required.** The viewer reads worlds through worldgen's reader, so it is rebuilt with schema 6 and refuses schema 5 files; nothing it shows changes. `gdext-headless-check` generates its worlds itself (the real world now also runs `chronology`) and reads the layer list from `report`'s `[layers]`, stopping at the next `[`-line, so the extra `[tables]` rows, the eight `meta` rows and the `[chronology]` section must not disturb it (they do not with `headless_check.cmake` as on `main`). The viewer engineer confirms the check passes on both CI jobs; ages and gods on the timeline are P4.

## 10. Invariants

For every seed, span, subset and data set accepted by section 6:

1. `race`: one row per race definition, ids 1 … n = `order`; `present` = 1 exactly for the ids of `param.races`, whose text is the present keys in id order; `awakening_year` is non-NULL exactly when `present` = 1; `creator_god` = `id`; `culture` is the id of the definition's culture; the words, camp and lifespans equal the data.
2. Awakening years strictly increase with race id over present races; the first is 0; consecutive differences ≥ G; the last ≤ E − G; each equals C3's formula.
3. With the shipped data and tuning, every present race other than the first has |`awakening_year` − base| ≤ jmax (no clamp).
4. `age`: P rows, ids 1 … P; row i has the i-th present race, `start_year` = its awakening year, `end_year` = the next row's `start_year` − 1, the last `end_year` = E − 1; so the ages tile [0, E) (P3-00 12.3).
5. `god`: n rows, `id` = race id, `camp` = the race's camp.
6. `culture`: one row per culture definition, ids in canonical id order, `race` = the id of its race; D3 holds for every race.
7. `race_biome`: exactly the listed entries, `biome` the codes, no D1 or D2 biome.
8. Every `god.name` and `age.name` matches P3-02 6 invariant 1's character set and length (age names: the form's text plus an optional numeral); the names of `region`, `age` and `god` are pairwise distinct.
9. The eight `meta` keys are present and canonical in every world (4.3).
10. Pure function of the seed, `HistoryParams`, the loaded data and the region names; independent of `--threads` and of every layer.

## 11. Determinism notes and bounds

| Step | Rule | Notes |
| --- | --- | --- |
| C2, C3 | H1, H4 | `Fx` from the loader, `mul_int` and `round_int`; one keyed draw per race after the first, keyed by race id; sequential clamp |
| C4, C5 | H2, H3, H5 | ids in race order; names by P3-02 (keyed, no shared generator); the registry is never iterated |
| C6 | P0-02 | rows in key order, one transaction |

No floating point, no unordered iteration, no `Rng`. Overflow: |raw| of a share ≤ 2^32 and E ≤ 12000, so `mul_int` < 2^46; years, G, jmax and differences ≤ 12000. Name lengths are bounded by D4 and P3-02 4.4.

## 12. Validation: hard bounds, statistics, performance

**Hard bounds** (P3-00 12.3, "Order and schedule", the part this stage owns; defects on any seed): invariants 2, 3 and 4 with the schedule recomputed from the data by the sweep script, not read back from the C++; the first present race at year 0; ages tile [0, E); invariant 8. The awakening events are P3-04's.

**Statistics** (from `[chronology]`; measured by the prototype on seeds 0–1999, all six races):

| Statistic | Hard | Typical | Measured min – max (span 2000 / 6000 / 12000) |
| --- | --- | --- | --- |
| awakening year of each later race | base ± jmax | uniform over that range | goblin 680–720 / 2040–2160 / 4080–4320; orc 1840–1880 / 5520–5640 / 11040–11280 (every value of the range occurs) |
| `age_years_min` | ≥ G (100) | 0.07 E ± jmax: 120–160 / 360–480 / 720–960 (always the last age, the orcs') | as typical |
| least gap between consecutive awakenings | ≥ G | — | 161 / 483 / 966 |
| god names drawn at attempt > 0 | descriptive | — | seeds 0, 1, 3, 7, 42: 4 of 30 |

**Performance**: budget 0.1 s at every size (P3-00 12.1). The work is P ≤ 6 draws, n ≤ 6 names of at most 64 attempts each, and one pass over the region names (about 50 at 1024, 200 at 2048): microseconds. The stage adds nothing measurable to `generate`.

## 13. Golden cases

- **`world-hashes.sh`**: the geography cases of P1-00 14 X5 now run `chronology` too; their hashes in `p1-worlds.txt` and the P0 empty and test-pattern worlds in `p0-worlds.txt` are re-recorded (schema 6, 16 `meta` keys, 18 pages). Added now, of P3-00 12.4's three: **`hist-span-2000-512`** (`--seed 7 --map-size 512 --history-span 2000`) and **`hist-races-elf-orc-1024`** (`--seed 3 --races elf,orc`), since the span and the subset already change chronology bytes (7.1). **`hist-dense-1024`** waits for P3-04: at P3-03 `--history-detail dense` changes only one `meta` value, which the flag tests cover; the case becomes meaningful when the detail sets phase lengths. The two cases' hashes go in a new golden file **`worldgen/tests/golden/p3-worlds.txt`** (same format as `p1-worlds.txt`), read by `world-hashes.sh expect` (a note, not a failure, while absent), recorded by the validation specialist. `world-hashes.sh cases` lists fourteen cases; generation time grows by one 512 and one 1024 geography (about 2 s release).
- **Stage golden vectors**: `worldgen/tests/golden/p3-03-chronology.txt` (14.4).

## 14. Tests required

For the independent test writer; seams (interim; the engineer records the final signatures here after implementation). **Final signatures** (engineer, 2026-11-19): as below, with these differences. `ChronologyTuning`, `AwakeningBounds` and the five row types have defaulted `operator==`; `ChronologyResult` has a sixth member, `std::vector<history::NameChoice> god_names`: one entry per god in god id order, holding the accepted name, the attempt that gave it and `fallback` true when the numeral fallback was used (shown in the block below); `stages::age_name_text` is `using history::age_name_text` (declared in `worldgen/history/data_checks.hpp`, so that the data checks need no stage header); `check_chronology_tuning(const ChronologyTuning&)` is public (C1 item 2); `history::history_params_problem(const HistoryParams&)` returns the first invalid field name (`span`, `civs_per_race`, `races`, `divine_script`) or an empty view; `StageContext::regions()` returns `std::span<const db::RegionRow>`; the stage list is `stages::STAGE_NAMES`, `implemented_stages()` and `stages_until()` (the former names `implemented_geography_stages()` and `geography_stages_until()` remain as inline aliases); `roman` is `worldgen::roman_numeral` in `worldgen/core/roman.hpp`, with `stages::roman` and `history::roman` forwarding to it.

```cpp
namespace worldgen::stages {
struct ChronologyTuning { Fx awakening_jitter_frac; std::int64_t awakening_min_gap_years; std::string age_name_form; };
ChronologyTuning chronology_tuning_from(const data::ContentSet&);
struct AwakeningBounds { std::int64_t gap; std::int64_t jmax; };
AwakeningBounds awakening_bounds(std::int64_t span, std::int64_t present, const ChronologyTuning&);          // C2
std::vector<std::int64_t> awakening_years(std::uint64_t seed, std::int64_t span,
    std::span<const std::int64_t> present_ids, std::span<const Fx> present_shares, const ChronologyTuning&);   // C3
std::string age_name_text(std::string_view form, std::string_view plural, std::string_view adjective);        // C4 step 1
struct ChronologyResult { std::vector<db::RaceRow> races; std::vector<db::RaceBiomeRow> race_biomes;
    std::vector<db::CultureRow> cultures; std::vector<db::GodRow> gods; std::vector<db::AgeRow> ages;
    std::vector<history::NameChoice> god_names; };                    // god id order: accepted attempt, fallback flag
ChronologyResult chronology_stage(const data::ContentSet&, const history::HistoryParams&, std::uint64_t seed,
    std::span<const std::string> region_names);                                                               // C1 items 2 to 4, C2 to C5
void run_chronology(StageContext&);
}
```

### 14.1 Unit tests

1. Every worked example of 7.1, through the seams: u, pick, d and year per race; ages; gods with their attempt numbers; the collision and clamp cases.
2. C2: jmax 0 (`awakening_jitter_frac` 0: every later race at its base, `pick(u, 1)`); G reduced to E div P (gap 2000, span 2000, six races: 333).
3. Subsets: every one of the 63 non-empty subsets of the six shipped races at seed 42, span 6000: the first present race at 0; every other present race at the year it has in the full world (no clamp acts); ages dense; six gods with the full world's names.
4. Age names: the shipped form; a form with `{adjective}` only (`The {adjective} Age` → `The Elven Age`); a form using both placeholders twice; the numeral fallback when the name is preloaded, and II taken too (→ III).
5. Entry checks of C1, each with its message: no region rows; a settings race list with an unknown id, out of order, repeated, empty; span 1999.
6. Data checks of section 6, each failing once on a fixture data root (a race preferring `lake`; one preferring `blighted_waste`; a race whose culture belongs to another race; a form giving 49 bytes; a form `"Age {plural} 2"`): through `check_history_data` (path `races`, no position, message), through `generate` (exit 3, the two-line form of 6, no world file) also with `--until none` and `--test-pattern`, and through the stage seam (fatal `chronology: data: …`). Two failures at once are reported sorted by message.
7. Schema: `SCHEMA_VERSION` 6; the DDL text of 5.1 byte for byte; the empty world (`--until none`) has 18 pages, 73728 bytes, the five empty tables and 16 `meta` keys; a version 5 file is refused with `world schema version 5, this build reads 6`; round trip of every row type, including NULL `awakening_year`; no REAL (`typeof`).
8. `meta`: each of the eight keys is written with its canonical text for every setting value (each flag value of P3-01 6.1 reaches its key; `--races orc,elf` gives `elf,orc`; `all` gives the six ids); the writer refuses a non-canonical value for each key and `finalize` without one of them; the reader refuses a world with each key missing (`meta_key_missing`) or invalid (`2000 ` with a space, `02000`, `1999`, `Normal`, `auto,1`, `elf,,orc`, `elf,elf`, a 63-digit hex script) with its message; `history_params_from_meta` inverts `history_meta_entries` for every combination of the setting values above.
9. `--test-pattern --history-span 2000 --races orc` records `param.history_span = 2000` and `param.races = orc`, and its tables and layers equal those of a plain test-pattern world.
10. Report: on a hand-made world, the `[chronology]` keys recomputed by the test; no `[chronology]` section with an empty `age` table; each corrupt case of 9.1 exits 3 with its message; `[tables]` lists the 13 tables.
11. `hash --detail`: the five new `table` lines in ascending-name position; a world with a NULL `awakening_year` hashes with tag 0x00 (hand-computed).
12. CLI: `--until chronology` gives the same file bytes as the default run; `--until history` exits 2 with `stage 'history' given to --until is not implemented yet`; an unknown stage lists the eight names; `meta.stages` of the default run ends in `,chronology`.

### 14.2 Property tests

13. Seeds 0–999 × spans 2000, 2001, 6000, 11999, 12000 × the 63 subsets of the shipped races, through `awakening_years`: invariants 2 and 3 and C3 recomputed by independent code (315,000 schedules; microseconds each).
14. Hand-made race data with shares packed close (random shares in [0, 1) sorted, seeds 0–999, gap 1–2000, jitter up to 0.05): invariant 2 always; the clamp keeps strict order.
15. Invariants 1 and 4 to 9 on fixture worlds built with the test seam `stages::testing::generate_world_with_stages` from a synthetic regions stage that writes hand-made region rows (names chosen per seed, some colliding with the god and age names) followed by the chronology stage: both fixture data roots, seeds 0–15, spans and race subsets varied by seed; the stored rows also equal `chronology_stage` on the stored region names. The four P1 golden worlds are covered by the world cases of 14.4, whose table hashes come from the independent reference.

### 14.3 Determinism tests

16. Threads 1, 2, 5: identical chronology tables.
17. Resume (P3-00 Y2 for this stage): a world written with `--until regions`, read back (region names from table `region`, `HistoryParams` from `history_params_from_meta`), and `chronology_stage` run on it gives exactly the rows of the uninterrupted run.
18. Draw independence (Y3 in small): adding a test-only extra keyed draw on another stream before C3 changes nothing.
19. `chronology_stage` called twice with the same inputs gives the same rows (no hidden state); the registry passed in is not shared across calls.
20. Y5: every world of 4.3's table has the eight keys.
21. On shipped data, the chronology rows do not depend on the region names: the full run's rows equal `chronology_stage` with an empty region list, for the four P1 golden worlds.

### 14.4 Golden values

22. `worldgen/tests/golden/p3-03-chronology.txt`, written by the test writer's **independent reference** `worldgen/tests/reference/p3_chronology_reference.py` (from this spec, using `core_reference` for hashing and the P3-02 reference `p3_names_reference.py` for names; not derived from the C++):
    - **Seam cases** (no geography; region names as listed): seeds 0, 1, 42, 2^64 − 1 and `phrase_seed("the unmapped")` × spans 2000, 6000, 12000 × races `all`, `elf,orc`, `orc`, `goblin,dwarf,human`, region list empty; plus the collision case of 7.1. Per case one line `<case> years <y…>`, one `<case> ages <id:race:start:end:name|…>`, one `<case> gods <id:name:attempt|…>` (`attempt` the accepted attempt, `F` when the numeral fallback gave the name), and the logical hashes (P0-04 8 encoding) of the five tables.
    - **World cases**: the four P1 golden cases (512 seed 0, 512 seed 42, 512 phrase "The Unmapped", 1024 seed 1, shipped data) and the two `hist-*` cases: the logical hashes of the five tables and the eight `meta` values, region names taken from the stored world (the P1-06 golden already pins them). The 1024 cases are read by a `*-slow` suite.
    The C++ must reproduce every line. The 7.1 examples must appear unchanged in it (the test writer states where).

**Version and golden re-recording.** `GENERATOR_VERSION` **0.13.0**. Every world changes (schema, `meta`, page count; full worlds also gain rows): `p0-worlds.txt` and `p1-worlds.txt` re-recorded and `p3-worlds.txt` recorded by the validation specialist after the cross-preset comparison. The P1-01 to P1-06 golden vectors (layer and table hashes) and P3-01's `p3-01-data.txt` and P3-02's `p3-02-names.txt` must not change; a difference is a defect.

### 14.5 Measured values to confirm

The empty-world size of 5.1 (**18 pages, 73728 bytes**) was first measured by the spec writer with Python's SQLite 3.46.1. **Confirmed** (engineer, 2026-11-19): `--until none` written by the vendored SQLite 3.54.1 is 18 pages, 73728 bytes, root pages 2 to 18 in the order of 15.2, and the `sqlite_schema` SQL text is 2928 bytes.

## 15. Amendments requested (the engineer applies 15.1; the lead the others)

Applied: 15.1 by the engineer with the implementation; 15.2 to 15.7 on 2026-11-19 (P0-02 amendment 18, P0-04 amendment 19, P0-01 section 16, P1-00 appendix C item 23, P3-00 correction note, P3-01 revision note r2 and r3).

### 15.1 `docs/specs/schema.md`

1. Header: "Current schema version: 6 (from generator 0.13.0, the chronology merge; version 5 is generator 0.11.0 to 0.12.0)".
2. Section 1 becomes "DDL at version 6": the five statements of 5.1 after `region_biome`'s.
3. A paragraph after the `region` tables: "`race`, `race_biome`, `culture`, `god`, `age` (version 6): written by the chronology stage in `id`, (`race`, `biome`), `id`, `id`, `id` order; empty when the stage did not run. One row per race definition (present or not), per listed biome preference, per culture definition, per race (its creator god, `id` = race id), and per present race (its age). Columns and their meaning in `p3-03-chronology.md` section 5.2."
4. The autoindex sentence: adds `sqlite_autoindex_race_1` and `sqlite_autoindex_culture_1`; "The empty world is 18 pages, 73728 bytes (11 at version 5)".
5. Section 2 becomes "Required `meta` keys at version 6" and gains the eight rows of 4.2 (key, canonical value), with `schema_version` `6`.
6. Section 4 (planned changes): one row, version 7, P3-04, the history tables of P3-00 6.1.
7. Changelog: "6 | 2026-11-19 | 5 → 6 with generator 0.13.0 (planned, the chronology merge): tables `race`, `race_biome`, `culture`, `god`, `age` added, executed ninth to thirteenth; required `meta` keys `param.civs_per_race`, `param.divine_activity`, `param.divine_script`, `param.event_intensity`, `param.history_detail`, `param.history_span`, `param.monster_pressure`, `param.races`; `meta.schema_version` and `user_version` 6; readers refuse version 5 worlds; the empty world grows from 11 to 18 pages. Proposals "History: schema versions 6 and 7" | P3-03 5, 4.2, P3-00 6.1".

### 15.2 P0-02

Amendment 18, "Schema version 6": the five statements of 5.1 after `region_biome`'s; version 6; every world has the five tables; the empty world 18 pages, 73728 bytes (page 1 `sqlite_schema`, then the eleven roots of version 5, then `race`, `sqlite_autoindex_race_1`, `race_biome`, `culture`, `sqlite_autoindex_culture_1`, `god`, `age`); the eight required keys with the canonical forms of 4.2, validated by writer and reader; refusal `world schema version 5, this build reads 6`; `WorldWriter::set_param_text`.

### 15.3 P0-04

Amendment 19: `generate` runs `chronology` after `regions` by default; `--until` accepts `chronology`, refuses `history` as not implemented, and lists eight names; the data checks of section 6 are input errors (exit 3); `report` lists the five tables and prints `[chronology]` (9.1); the expected `--until none` report text gains the eight `meta` rows, `schema_version = 6` and the five `[tables]` rows; `world-hashes.sh` gains the two `hist-*` cases and `p3-worlds.txt`.

### 15.4 P0-01

The stream list gains `hist.chronology.awakening`.

### 15.5 P1-00

Section 3's stage table and 10.1: stage 7 `chronology` (P3-03); `meta.stages` of a default run ends in `chronology`. (P3-00 5 already lists it.)

### 15.6 P3-00

- 2.1, row "Ages": "age ids 1 … P over the present races in race order (1–6 when all are present); with a subset, the first present race awakens at year 0" (C3, 5.2).
- 6.1 `god` row: "one per race, present or not (`id` = race id)" (5.2); `race` row: `creator_god` = `id` on every row; the table outline gains `plural`, `adjective`, `culture`.
- 12.4: `hist-dense-1024` moves to P3-04 (13); the two others are added by P3-03 with `p3-worlds.txt`.
- 4.6 / P3-04: the registry rule of 8.2 is P3-04's first step.

### 15.7 P3-01 (text for the writer of `p3/spec-fixes`, not edited here)

- 3, "Cross-kind conditions": checked by `check_history_data` in `generate` (input error, exit 3) and again at the stage (P3-03 6); the new conditions D2 (not natural), D4 and D5 (age names); item 3 is implied by item 2.
- 6.2, last paragraph: with `--test-pattern` the history flags have no effect on tables or layers but are recorded in `meta` (P3-03 4.3).
- 6.3: the keys are written by `generate_world` for every world from P3-03 on (4.1).

## 16. Open questions for Paul

Each with the recommendation and the interim choice this spec follows.

1. **Subsets without the elves.** The design says the order of appearance never changes for any subset, not when the first present race appears. Recommendation and interim: the first present race awakens at year 0 (history starts with it); each later race keeps its own share of the span. Alternative: rescale the shares over the present races, which would move every race's date with the choice of subset.
2. **Creator gods of absent races.** Recommendation and interim: every race keeps its creator god in the `god` table, so a race's god has the same name and id whichever races are chosen; a god whose race is absent created nothing in this world. Alternative: gods only for present races (ids, and so names, would then shift with the subset).
3. **Data errors across kinds are input errors** (exit 3, before generating), not stage failures (exit 1, "a bug"). Recommendation and interim: yes, through `check_history_data` in the CLI; the stage repeats the check as a guard.
4. **Test-pattern worlds record the history settings given.** Recommendation and interim: yes; the world records the settings it was created with, and nothing else changes.
5. **`hist-dense-1024` waits for P3-04.** At P3-03 the detail level changes one `meta` value. Recommendation and interim: add `hist-span-2000-512` and `hist-races-elf-orc-1024` now, `hist-dense-1024` with the engine.
6. **The world report gains a `[chronology]` section without a format bump** (report v2). Recommendation and interim: yes, by P1-00's rule that the number changes only once a format is in recorded goldens.

## Appendix A. Proposals entry (added to `docs/design/proposals.md` with this spec)

"History: with a subset of races the first one present opens history; every race keeps its creator god" (questions 1 and 2). Schema 6 itself is recorded in "History: schema versions 6 and 7".

## Revision note (2026-11-19, after implementation)

Text changes so that the spec says what the implementation (`p3/03-int`) and its independent tests do, and one check added by the lead. No golden value or world byte changes.

1. 6: check **D0** (the lead's decision): no `race` definition is an input error, `no race is defined; kind 'race' needs at least one definition`, path `races`, first in the D order (to be implemented: `check_history_data` on `p3/03-int` at 14f6f0a does not have it yet); how the loader's required-kind error and C1 item 3 come first in practice, and `generate_world`'s FatalError as the last line of defence.
2. 9.1: the row-count check applies with an empty `age` table and present races (`0 rows for <p> present races`); both tables empty gives no section; the row-shape messages.
3. 4.1, 4.2: the message texts of `set_param_text`, `set_param_integer`, `set_param_decimal`, `finalize` (the first missing `param.*` key, replacing the old `param.map_size` message), `generate_world`, `history_params_from`, `history_params_from_meta`, and `meta_key_missing`.
4. C1: `run_chronology` applies all of C1, the seam `chronology_stage` items 2 to 4 and accepts an empty region list; the order of the settings checks.
5. 14.2 item 15: fixture worlds through `generate_world_with_stages` with a synthetic regions stage (not N = 128 through `generate_world`).
6. 6: `invalid data in '<data root>'` prints the root as the loader's error does.
7. 14, 14.4: `ChronologyResult::god_names` in the seam block; a numeral-fallback god's attempt is `F` in the golden file.
8. 5.1, 14.5: the empty-world size (18 pages, 73728 bytes; schema text 2928 bytes) is confirmed with the vendored SQLite 3.54.1.
9. 15: 15.2 to 15.7 applied.

## Revision note r2 (2026-11-19, amendments for P3-04)

Requested by P3-04 18.2; text only, nothing of this stage changes. 4.3: from P3-04 (generator 0.14.0) `--until history` is accepted and is the default, and `--until chronology` no longer gives the default world's bytes (the default also runs `history`; 14.1 item 12's equality holds only at 0.13.0). 5.1: from schema 7 the empty world is 34 pages (P3-04 9.6; P0-02 amendment 19). 8.2: the registry rule is applied by the `history` stage as its first step (P3-04 7.2, "Registry"). The P3-03 audit's guards (`origin/p3/03-audit-guard`) are recorded by the audit, not here.
