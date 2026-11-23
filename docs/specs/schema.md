# World database schema reference

The contract between the generator and the game (design doc section 6; P0-02). This file holds the full current DDL, the required `meta` keys, the layer catalogue and the changelog. It is updated by every branch that changes the schema (P0-02 5.2) and by every spec that adds a layer.

**Current schema version: 8** (from generator 0.22.0, the P5-12 merge, table `region_name`; version 7 is generator 0.14.0 to 0.21.0, the engine merge onward; version 6 is generator 0.13.0; version 5 is generator 0.11.0 to 0.12.0, version 4 generator 0.9.0 to 0.10.0, version 3 was generator 0.8.0). `SCHEMA_VERSION` in `worldgen/include/worldgen/db/schema.hpp`; equal to `PRAGMA user_version` and `meta.schema_version`.

## 1. DDL at version 8

Executed in this order, each as one statement, byte for byte as written (P0-02 5.3, 5.4; `DDL_STATEMENTS` in `worldgen/include/worldgen/db/schema.hpp`):

```
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) STRICT, WITHOUT ROWID
CREATE TABLE layer (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, width INTEGER NOT NULL, height INTEGER NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL) STRICT
CREATE TABLE pass (id INTEGER PRIMARY KEY, range_id INTEGER NOT NULL, kind INTEGER NOT NULL, capped INTEGER NOT NULL, col_x INTEGER NOT NULL, col_y INTEGER NOT NULL, saddle_dm INTEGER NOT NULL, col_dm INTEGER NOT NULL, path BLOB NOT NULL) STRICT
CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT
CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL, temp_offset_dc INTEGER NOT NULL) STRICT
CREATE TABLE region (id INTEGER PRIMARY KEY, name TEXT NOT NULL, biome INTEGER NOT NULL, x_min INTEGER NOT NULL, y_min INTEGER NOT NULL, x_max INTEGER NOT NULL, y_max INTEGER NOT NULL, seed_x INTEGER NOT NULL, seed_y INTEGER NOT NULL, landmass INTEGER NOT NULL, cells INTEGER NOT NULL, lake_cells INTEGER NOT NULL, river_cells INTEGER NOT NULL, river_class_max INTEGER NOT NULL, coast_cells INTEGER NOT NULL, walk_cells INTEGER NOT NULL, walk_area INTEGER NOT NULL, elev_mean_dm INTEGER NOT NULL, elev_max_dm INTEGER NOT NULL, fertility_mean INTEGER NOT NULL, forage_mean INTEGER NOT NULL, timber_mean INTEGER NOT NULL, ore_mean INTEGER NOT NULL, fresh_water_mean INTEGER NOT NULL, habitability_mean INTEGER NOT NULL, habitability_p90 INTEGER NOT NULL, ore_rich_cells INTEGER NOT NULL, ore_reach_cells INTEGER NOT NULL) STRICT
CREATE TABLE region_border (a INTEGER NOT NULL, b INTEGER NOT NULL, length_m INTEGER NOT NULL, low_dm INTEGER NOT NULL, low_x INTEGER NOT NULL, low_y INTEGER NOT NULL, walk_pairs INTEGER NOT NULL, river_pairs INTEGER NOT NULL, river_class_max INTEGER NOT NULL, pass_id INTEGER NOT NULL, passes INTEGER NOT NULL, PRIMARY KEY (a, b)) STRICT, WITHOUT ROWID
CREATE TABLE region_biome (region INTEGER NOT NULL, biome INTEGER NOT NULL, cells INTEGER NOT NULL, PRIMARY KEY (region, biome)) STRICT, WITHOUT ROWID
CREATE TABLE race (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, plural TEXT NOT NULL, adjective TEXT NOT NULL, camp INTEGER NOT NULL, lifespan_years INTEGER NOT NULL, generation_years INTEGER NOT NULL, culture INTEGER NOT NULL, present INTEGER NOT NULL, awakening_year INTEGER, creator_god INTEGER NOT NULL) STRICT
CREATE TABLE race_biome (race INTEGER NOT NULL, biome INTEGER NOT NULL, weight INTEGER NOT NULL, PRIMARY KEY (race, biome)) STRICT, WITHOUT ROWID
CREATE TABLE culture (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, race INTEGER NOT NULL) STRICT
CREATE TABLE god (id INTEGER PRIMARY KEY, name TEXT NOT NULL, camp INTEGER NOT NULL) STRICT
CREATE TABLE age (id INTEGER PRIMARY KEY, race INTEGER NOT NULL, start_year INTEGER NOT NULL, end_year INTEGER NOT NULL, name TEXT NOT NULL) STRICT
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
CREATE TABLE region_name (region INTEGER NOT NULL, from_year INTEGER NOT NULL, name TEXT NOT NULL, culture INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID
```

`pass` (version 2): one row per natural pass site, written by the elevation stage in `id` order; empty when no site was placed or the stage did not run. Columns and their meaning in `p1-01-elevation.md` section 9.1.

`biome` (version 3): one row per loaded biome definition, written by the biome stage in ascending `id`: `id` = the biome `code`, `key` = the definition id, `name`, `colour` (0xRRGGBB), `water` (0 or 1); empty when the stage did not run (P1-04 3, B6). Twelve rows with the shipped data of generator 0.9.0 (codes 1 to 12; P1-00 6.6).

`climate` (version 3; column `temp_offset_dc` from version 4): one row, `id` 1, `wind_toward` = the prevailing wind's D8 code (0 E, 1 SE, 2 S, 3 SW, 4 W, 5 NW, 6 N, 7 NE), the direction it blows toward, and `temp_offset_dc` = the world's temperature offset in tenths of a degree Celsius (−50..50; −20..20 with the shipped tuning), written by the climate stage; empty when the stage did not run (P1-03 3, C2b, C7).

`region`, `region_border`, `region_biome` (version 5): written by the regions stage in `id`, (`a`, `b`) and (`region`, `biome`) order; empty when the stage did not run. One row per region, per pair of regions sharing a 4-adjacent cell pair, and per region and biome code present. Columns and their meaning in `p1-06-regions.md` section 3.

`race`, `race_biome`, `culture`, `god`, `age` (version 6): written by the chronology stage in `id`, (`race`, `biome`), `id`, `id`, `id` order; empty when the stage did not run. One row per race definition (present or not), per listed biome preference, per culture definition, per race (its creator god, `id` = race id), and per present race (its age). Columns and their meaning in `p3-03-chronology.md` section 5.2.

The history tables (version 7), written by the history stage, empty when it did not run (columns and their meaning in `p3-04-simulation-engine.md` 9.2):

- **Catalogue**: `event_kind`, `event_kind_role`, `event_kind_value`, the compiled structure of every event kind with the data's base salience and template (section 6 below), written whether a kind is used or not.
- **Phases and log**: `phase` (the schedule, `end_year` inclusive; kinds 1 opening, 2 quiet, 3 flare-up, 4 final), `event` (ids in append order, `year` non-decreasing with `id`), `event_participant` (`role` = the role's `seq`, `entity_type` a code), `event_cause` (cause < event), `event_value` (`seq` = the value key's `seq`; one row per key).
- **Entities**: `civilization`, `settlement`, the end state of the run (settlement `kind` 1 hamlet, 2 village, 3 town, 4 city, 5 fortress; `status` 1 thriving, 2 declining, 3 ruined, 4 faded).
- **By year**: `settlement_census`, `region_control`, `relation` (P3-04 6.7 and 10).

`region_name` (version 8), written by the history stage in (`region`, `from_year`) order, empty when it did not run: one row per renaming of a region by history (`p5-12-renamed-regions.md` 7.2): the new `name`, in force from `from_year`, the namer's `culture` (0 none) and the `region_renamed` `event`. `region.name` is never rewritten; the name of a region at year Y is the row with the largest `from_year` ≤ Y, else `region.name`.

Entity type codes: 1 race, 2 god, 3 civilization, 4 settlement, 5 region; reserved 6 figure, 7 faction, 8 monster, 9 site (`history::entity_type_key()`). Nullable columns: `civilization.ended_year`, `settlement.ruined_year`, `region_control.to_year`, `relation.until`, NULL while the year has not happened.

SQLite adds the automatic indexes `sqlite_autoindex_layer_1` for `layer.name`, `sqlite_autoindex_biome_1` for `biome.key`, `sqlite_autoindex_race_1` for `race.key`, `sqlite_autoindex_culture_1` for `culture.key` and `sqlite_autoindex_event_kind_1` for `event_kind.key`; the version 5 tables add none. The empty world is 34 pages, 139264 bytes (18 at version 6, 73728 bytes; measured with the vendored SQLite 3.54.1 at generator 0.14.0): its schema text (5839 bytes) no longer fits page 1, so `sqlite_schema` is a two-level b-tree with leaf pages 22 and 23; the thirteen tables and `sqlite_autoindex_event_kind_1` have root pages 19 to 21 and 24 to 34. At version 6 it was 18 pages (11 at version 5; 8 at versions 3 and 4, P0-02 amendments 15 and 16; the three version 5 tables add one root page each; the version 6 tables add the roots of `race`, `sqlite_autoindex_race_1`, `race_biome`, `culture`, `sqlite_autoindex_culture_1`, `god` and `age`, measured with the vendored SQLite 3.54.1).

File settings (P0-02 4): page size 4096, UTF-8, no auto-vacuum, `application_id` 0x554E4D50 (`UNMP`), `user_version` = schema version, written with `VACUUM INTO`. Layer blobs: format version 1, codec 0 (raw) or 1 (delta-varint) (P0-02 7).

## 2. Required `meta` keys at version 8 (unchanged since 7)

| Key | Value |
| --- | --- |
| `content_hash` | SHA-256 of the loaded data's canonical encoding, 64 lowercase hex digits (P0-03 8) |
| `format` | `the-unmapped-world` |
| `generator_version` | `MAJOR.MINOR.PATCH` |
| `history_end_year` | = `param.history_span` (the year after the last year of history), canonical decimal; readers refuse another value (P3-04 9.4) |
| `param.civs_per_race` | `auto`, or `1`, `2`, `3`, `4` |
| `param.divine_activity` | `distant`, `normal` or `meddling` |
| `param.divine_script` | empty (readers also accept 64 lower-case hex digits, the P5 form) |
| `param.event_intensity` | `calm`, `normal` or `turbulent` |
| `param.history_detail` | `sparse`, `normal` or `dense` |
| `param.history_span` | decimal integer 2000 to 12000, canonical (no sign, no leading zero) |
| `param.map_size` | 512, 1024 or 2048 |
| `param.monster_pressure` | `low`, `normal` or `high` |
| `param.races` | one or more race ids (P0-03 definition id grammar) in race order, joined by `,` with no spaces, none twice, e.g. `elf,goblin,dwarf,faerie,human,orc` |
| `schema_version` | `7` |
| `seed` | unsigned decimal |
| `seed_phrase` | the scrubbed phrase, or empty |
| `stages` | stage names in run order, comma separated (P1-00 section 3), e.g. `elevation,hydrology` |

## 3. Layer catalogue

The authoritative catalogue, with value ranges and special values, is P1-00 section 4. Adding a layer needs no schema version bump; changing a catalogued layer's name, type, units or meaning does (P1-00 section 4, rule 3). Summary:

| Layer | Type | Units | Stage | Since |
| --- | --- | --- | --- | --- |
| `elevation` | `i16` | decimetres above sea level; < 0 ocean, > 0 land, never 0 | `elevation` | P1 (schema 1) |
| `water` | `u8` | 0 land, 1 river, 2 lake, 3 ocean | `hydrology` | P1 (schema 1) |
| `water_depth` | `u16` | decimetres of standing water | `hydrology` | P1 (schema 1) |
| `flow_dir` | `u8` | D8 code 0 to 7, 8 on ocean | `hydrology` | P1 (schema 1) |
| `flow_accum` | `u32` | upstream cells including itself; 0 on ocean | `hydrology` | P1 (schema 1) |
| `river` | `u8` | river class 1 to 15, 0 none | `hydrology` | P1 (schema 1) |
| `lake` | `u32` | lake id, 0 none | `hydrology` | P1 (schema 1) |
| `coast_dist` | `i16` | metres to the coast, positive on land, negative on ocean | `hydrology` | P1 (schema 1) |
| `temperature` | `i16` | annual mean, 0.1 °C | `climate` | P1 (schema 2, generator 0.7.0) |
| `rainfall` | `u16` | mm per year | `climate` | P1 (schema 2, generator 0.7.0) |
| `biome` | `u8` | biome code (table `biome`) | `biomes` | P1 (schema 3, generator 0.8.0) |
| `fertility`, `ore`, `timber`, `fresh_water`, `forage`, `habitability` | `u8` | score 0 to 255 on an absolute scale (P1-05) | `resources` | planned for generator 0.10.0 (`forage` added 2026-11-18); no schema change |
| `walk_area` | `u16` | walkable-area id, 0 none (P1-05) | `resources` | planned (added 2026-11-16) |
| `region` | `u16` | region id (table `region`), 0 on ocean only (lakes and rivers belong to regions) | `regions` | planned for generator 0.11.0 (layer id 19) |
| `test.ramp`, `test.hash`, `test.noise` | `u8`, `u8`, `i16` | P0 test pattern, 256 × 256 (P0-04 5.2) | `test_pattern` | P0 (schema 1) |

## 4. Planned changes

Versions 6 to 8 are in sections 1 and 2; no further schema change is planned here (P5-00 6.1 lists the tables P5's later stages add, each with its own bump). Appending an event kind is not a schema change (section 6).

From generator 0.18.0 (P3-09, `docs/specs/p3-06-expansion-conflict.md` 20.9), appended to the catalogue of section 6 by the engineer at the merge: 32 `refugees_settled` (caused 1, region 1, settlement 1; roles 1 `settlement` settlement 1–1, 2 `founder` civilization 1–1, 3 `origin` settlement 1–1; value 1 `population`) and 33 `tribute` (caused 1, region 0, settlement 0; roles 1 `payer` civilization 1–1, 2 `payee` civilization 1–1; value 1 `people`). From P3-09 r3 (P3-06 20.15, 20.17, 20.22): 34 `war_won` (caused 1, region 0, settlement 0; roles 1 `winner`, 2 `loser` civilization 1–1; value 1 `years`), 35 `search_abandoned` (caused 1, region 1, settlement 0; roles 1 `searcher`, 2 `hidden` civilization 1–1; value 1 `years`), 36 `refuge_held` (caused 1, region 1, settlement 1; roles 1 `attacker`, 2 `defender` civilization 1–1; value 1 `reason`). No schema change.

## 5. Changelog

| Version | Date | Change | Spec |
| --- | --- | --- | --- |
| 1 | 2026-11-15 | Initial schema: `meta`, `layer` | P0-02 |
| 1 | 2026-11-15 | No schema change: the layer catalogue gains the elevation and hydrology layers; `meta.stages` lists geography stages | P1-00, P1-01, P1-02 |
| 2 | 2026-11-17 | 1 → 2 with generator 0.6.0: table `pass` added (natural pass sites, executed third, after `layer`); `meta.schema_version` and `user_version` 2; the empty world grows from 4 to 5 pages (P0-02 amendment 14). Proposals "Pass sites are stored in the world file" | P1-01 (fourth version) 9.1, P1-00 9.3 |
| 3 | 2026-11-17 | 2 → 3 with generator 0.8.0 (the biomes merge): tables `biome` (one row per biome definition, written by the biome stage) and `climate` (one row with the prevailing wind `wind_toward`, written by the climate stage) added, executed fourth and fifth, after `pass`; `meta.schema_version` and `user_version` 3; the empty world grows from 5 to 8 pages (`biome`, `sqlite_autoindex_biome_1`, `climate`; P0-02 amendment 15). Proposals "P1 schema changes" | P1-04 (second version) 3, P1-03 3, P1-00 9.2 |
| 4 | 2026-11-18 | 3 → 4 with generator 0.9.0 (the biome variety merge): table `climate` gains `temp_offset_dc INTEGER NOT NULL` (the per-world temperature offset in tenths of a degree); its DDL statement is replaced in place (still fifth); `meta.schema_version` and `user_version` 4; readers refuse version 3 worlds; the empty world stays 8 pages (P0-02 amendment 16). Planned `region` and `region_border` move to version 5. Proposals "More natural biomes, per-world temperature, a thinner coastal forest" | P1-03 3, P1-04 (third version) 3 and appendix B, P1-00 9.2 |
| 4 | 2026-11-18 | No schema change (planned, generator 0.10.0, the resources merge): the layer catalogue gains `forage` and the resources layers become real, layer ids 12 to 18 (`region` moves to 19) | P1-05 (second version) 3, P1-00 4 |
| 5 | 2026-11-18 | 4 → 5 with generator 0.11.0 (planned, the regions merge): tables `region`, `region_border` and `region_biome` added, executed sixth to eighth, after `climate`; `meta.schema_version` and `user_version` 5; readers refuse version 4 worlds; the empty world grows from 8 to 11 pages. `region_biome` and the extended `region` and `region_border` columns are new against the first plan. Proposals "P1 schema changes" (update) | P1-06 (second version) 3, P1-00 9.2 |
| 6 | 2026-11-19 | 5 → 6 with generator 0.13.0 (planned, the chronology merge): tables `race`, `race_biome`, `culture`, `god`, `age` added, executed ninth to thirteenth; required `meta` keys `param.civs_per_race`, `param.divine_activity`, `param.divine_script`, `param.event_intensity`, `param.history_detail`, `param.history_span`, `param.monster_pressure`, `param.races`; `meta.schema_version` and `user_version` 6; readers refuse version 5 worlds; the empty world grows from 11 to 18 pages. Proposals "History: schema versions 6 and 7" | P3-03 5, 4.2, P3-00 6.1 |
| 7 | 2026-11-19 | 6 → 7 with generator 0.14.0 (the engine merge): thirteen tables added, executed fourteenth to twenty-sixth; required `meta` key `history_end_year`; readers refuse version 6; the empty world grows from 18 to 34 pages. Proposals "History: schema versions 6 and 7" | P3-04 9, P3-00 6.1 |
| 7 | 2026-11-19 | No schema change (generator 0.15.0, the peoples merge): the event catalogue gains kinds 6–19 (section 6); the history tables now hold the rows of the peoples subsystem | P3-05 11, 18.1 |
| 7 | 2026-11-19 | No schema change (generator 0.16.0, the conflict merge): the event catalogue gains kinds 20–30 (section 6); table `relation` holds rows of kinds `enmity`, `war` and `peace` | P3-06 11, 4.3, 18.5 |
| 7 | 2026-11-20 | No schema change (generator 0.17.0, the P3-08 change): the event catalogue gains kind 31 `settlement_abandoned_to` (section 6); templates may use the three-word value form `{value?w0\|w1\|w2}` (P3-04 r10 8.4) | P3-08 8, P3-05 r6 11 |
| 7 | 2026-11-20 | No schema change (generator 0.18.0, the P3-09 change): the event catalogue gains kinds 32 `refugees_settled` and 33 `tribute` (section 6) | P3-09 2, P3-06 r9 20.9 |
| 7 | 2026-11-20 | No schema change (generator 0.19.0, the P3-10 change): the event catalogue gains kinds 37 `plague_outbreak` and 38 `quiet_stretch` (section 6); kinds 4 and 12 are no longer written; templates may use the cause list placeholder `{@causes:...}` (P3-04 r12 20.2) and render values with thousands separators (20.3) | P3-10 8, P3-04 r12 20 |
| 7 | 2026-11-21 | No schema change (generator 0.20.0, the P3-11 change): the event catalogue gains kinds 39 `claim_settlements` and 40 `settlements_ceded` (section 6); values from one to nine render as words in sentences (P3-04 r17 21.1) | P3-11 7, P3-04 r17 21 |
| 7 | 2026-11-22 | No schema change (generator 0.21.0, the P5-01 change): the event catalogue gains kinds 41 `claim_dropped` and 42 `refuge_hidden` (section 6); the numbers of one clause of a template render all as words or all as digits (P3-04 r19 22.1) | P5-01 7, P3-04 r19 22 |
| 8 | 2026-11-22 | 7 → 8 with generator 0.22.0 (planned, the P5-12 merge): table `region_name` added, executed twenty-seventh, after `relation`; `meta.schema_version` and `user_version` 8; readers refuse version 7 worlds; the empty world grows from 34 to 35 pages (to be confirmed). The event catalogue gains kind 43 `region_renamed` (section 6); templates may use `{@region_former}`, and a region in a sentence is named as at the event (P3-04 r21 23.1). Proposals "Schema 8: table `region_name` (P5-12)" | P5-12 7, P3-04 r21 23 |

## 6. Event catalogue

Kinds, roles, value keys and flags are code (`history::compiled_event_catalogue()`); base salience and templates are data (`history.report`). `region_use`, `settlement_use`: 0 never, 1 always, 2 optional; `caused` 1: every event of the kind has a cause. Kept current by every spec that appends a kind.

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values (seq: key) | spec |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `awakening` | 0 | 1 | 0 | 1: `race`, race, 1–1; 2: `creator`, god, 1–1 | — | P3-04 |
| 2 | `civ_founded` | 1 | 1 | 0 | 1: `civilization`, civilization, 1–1; 2: `race`, race, 1–1 | — | P3-04 |
| 3 | `settlement_founded` | 0 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `founder`, civilization, 1–1 | 1: `population` | P3-04 |
| 4 | `quiet_era` | 0 | 1 | 0 | 1: `civilization`, civilization, 1–1 | 1: `years`; 2: `generations` | P3-04 |
| 5 | `flare_up` | 1 | 0 | 0 | — | 1: `years` | P3-04 |
| 6 | `colony_founded` | 0 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `founder`, civilization, 1–1; 3: `origin`, settlement, 1–1 | 1: `population` | P3-05 |
| 7 | `became_village` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `population` | P3-05 |
| 8 | `became_town` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `population` | P3-05 |
| 9 | `became_city` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `population` | P3-05 |
| 10 | `settlement_dwindled` | 0 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `population` | P3-05 |
| 11 | `famine` | 0 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `dead`; 2: `population` | P3-05 |
| 12 | `plague` | 0 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `dead`; 2: `population` | P3-05 |
| 13 | `plague_spread` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `dead`; 2: `population` | P3-05 |
| 14 | `succession_crisis` | 0 | 1 | 1 | 1: `civilization`, civilization, 1–1 | — | P3-05 |
| 15 | `civ_seceded` | 1 | 1 | 1 | 1: `civilization`, civilization, 1–1; 2: `from`, civilization, 1–1 | 1: `settlements` | P3-05 |
| 16 | `settlement_abandoned` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `survivors` | P3-05 |
| 17 | `capital_moved` | 1 | 1 | 1 | 1: `civilization`, civilization, 1–1; 2: `settlement`, settlement, 1–1 | — | P3-05 |
| 18 | `civ_ended` | 1 | 1 | 1 | 1: `civilization`, civilization, 1–1 | — | P3-05 |
| 19 | `ruin_faded` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1 | — | P3-05 |
| 20 | `claim` | 1 | 1 | 0 | 1: `claimant`, civilization, 1–1; 2: `holder`, civilization, 1–1 | — | P3-06 |
| 21 | `war_declared` | 1 | 1 | 0 | 1: `attacker`, civilization, 1–1; 2: `defender`, civilization, 1–1 | — | P3-06 |
| 22 | `battle` | 1 | 1 | 0 | 1: `winner`, civilization, 1–1; 2: `loser`, civilization, 1–1 | 1: `winner_dead`; 2: `loser_dead` | P3-06 |
| 23 | `battle_at_pass` | 1 | 1 | 0 | 1: `winner`, civilization, 1–1; 2: `loser`, civilization, 1–1 | 1: `winner_dead`; 2: `loser_dead`; 3: `pass` | P3-06 |
| 24 | `siege` | 1 | 1 | 1 | 1: `attacker`, civilization, 1–1; 2: `defender`, civilization, 1–1 | 1: `fell` | P3-06 |
| 25 | `sacked` | 1 | 1 | 1 | 1: `attacker`, civilization, 1–1; 2: `defender`, civilization, 1–1 | 1: `dead`; 2: `survivors` | P3-06 |
| 26 | `settlement_captured` | 1 | 1 | 1 | 1: `attacker`, civilization, 1–1; 2: `defender`, civilization, 1–1 | 1: `dead`; 2: `fled` | P3-06 |
| 27 | `peace` | 1 | 0 | 0 | 1: `winner`, civilization, 1–1; 2: `loser`, civilization, 1–1 | 1: `years` | P3-06 |
| 28 | `truce` | 1 | 0 | 0 | 1: `parties`, civilization, 2–2 | 1: `years` | P3-06 |
| 29 | `region_ceded` | 1 | 1 | 0 | 1: `from`, civilization, 1–1; 2: `to`, civilization, 1–1 | — | P3-06 |
| 30 | `civ_conquered` | 1 | 1 | 0 | 1: `civilization`, civilization, 1–1; 2: `conqueror`, civilization, 1–1 | — | P3-06 |
| 31 | `settlement_abandoned_to` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1; 3: `destination`, settlement, 1–1 | 1: `settled`; 2: `lost` | P3-05 r6 |
| 32 | `refugees_settled` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `founder`, civilization, 1–1; 3: `origin`, settlement, 1–1 | 1: `population` | P3-06 r9 |
| 33 | `tribute` | 1 | 0 | 0 | 1: `payer`, civilization, 1–1; 2: `payee`, civilization, 1–1 | 1: `people` | P3-06 r9 |
| 34 | `war_won` | 1 | 0 | 0 | 1: `winner`, civilization, 1–1; 2: `loser`, civilization, 1–1 | 1: `years` | P3-06 r10 |
| 35 | `search_abandoned` | 1 | 1 | 0 | 1: `searcher`, civilization, 1–1; 2: `hidden`, civilization, 1–1 | 1: `years` | P3-06 r10 |
| 36 | `refuge_held` | 1 | 1 | 1 | 1: `attacker`, civilization, 1–1; 2: `defender`, civilization, 1–1 | 1: `reason` | P3-06 r10 |
| 37 | `plague_outbreak` | 0 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `civilization`, civilization, 1–1 | 1: `dead`; 2: `population`; 3: `total`; 4: `reached` | P3-04 r12, P3-05 r12 |
| 38 | `quiet_stretch` | 0 | 0 | 0 | 1: `realm`, civilization, 1–0 (unbounded) | 1: `years`; 2: `realms` | P3-04 r12 |
| 39 | `claim_settlements` | 1 | 1 | 0 | 1: `claimant`, civilization, 1–1; 2: `holder`, civilization, 1–1; 3: `settlement`, settlement, 1–0 (unbounded) | — | P3-04 r17, P3-06 r19 |
| 40 | `settlements_ceded` | 1 | 1 | 0 | 1: `from`, civilization, 1–1; 2: `to`, civilization, 1–1; 3: `settlement`, settlement, 1–0 (unbounded) | — | P3-04 r17, P3-06 r19 |
| 41 | `claim_dropped` | 1 | 1 | 0 | 1: `claimant`, civilization, 1–1; 2: `holder`, civilization, 1–1 | — | P3-04 r19, P3-06 r22 |
| 42 | `refuge_hidden` | 1 | 1 | 1 | 1: `settlement`, settlement, 1–1; 2: `founder`, civilization, 1–1 | — | P3-04 r19, P3-06 r22 |
| 43 | `region_renamed` | 1 | 1 | 2 | 1: `namer`, civilization, 0–1 | — | P3-04 r21, P5-12 |

Kinds 1–5 are P3-04's, 6–19 P3-05's (generator 0.15.0), 20–30 P3-06's (generator 0.16.0, P3-06 11); in kinds 20–30 every count is 1–1 except `parties` 2–2, and every kind is caused. Kind 31 is P3-05 r6's (generator 0.17.0, the P3-08 change): an abandonment whose survivors reached another settlement; from 0.17.0 kind 16 is written only when none did. Kinds 32 `refugees_settled` and 33 `tribute` are P3-06 r9's (generator 0.18.0, the P3-09 change): the survivors of a fallen last refuge found a hidden settlement; a loser with no region to cede pays people. Kinds 37 `plague_outbreak` and 38 `quiet_stretch` are P3-04 r12's (generator 0.19.0, the P3-10 change): every outbreak with its total toll and the number of its spreads (P3-05 r12 22.1), and one quiet line for the world per quiet phase row (P3-04 r12 20.1); from 0.19.0 kinds 12 `plague` and 4 `quiet_era` keep their rows and are no longer written. Kinds 39 `claim_settlements` and 40 `settlements_ceded` are P3-04 r17's (generator 0.20.0, the P3-11 change): a claim on the visible settlements of a holder that does not hold the region (P3-06 r19 23.2), and the foothold cession that passes the loser's settlements in a region the winner already holds (23.3); kinds 20 and 29 are still written for every other claim and cession. Kinds 41 `claim_dropped` and 42 `refuge_hidden` are P3-04 r19's (generator 0.21.0, the P5-01 change): a claim dropped at a declaration that found no allowed goal (P3-06 r22 24.3), and the years a fled refuge stays hidden within the history, as a span whose `end_year` is its last hidden year (24.4). Kind 43 `region_renamed` is P3-04 r21's (generator 0.22.0, P5-12): a region renamed by history, caused by its defining event, with its row in `region_name`.

**Relation kinds** (`relation.kind`; text keys, not a schema change): from generator 0.16.0, `enmity` (symmetric, a < b), `war` (asymmetric, a the attacker, b the defender), `peace` (symmetric, a < b), all between civilizations (`a_type` = `b_type` = 3; P3-06 4.3). None exist before 0.16.0. No schema change.

**Schema changes** (P3-04 8.2): renaming, removing or renumbering a kind; adding, removing, renaming or reordering a role or value key of an existing kind; changing a role's entity type or counts, or a kind's `caused`, `region_use` or `settlement_use`; changing the meaning of any of them; changing a code list; any column. **Not a schema change**: appending a kind; changing a template or a base salience (a data change). A shipped kind is never edited: a kind that needs another role or value key is added as a new kind.
