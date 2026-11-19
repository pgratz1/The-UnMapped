# World database schema reference

The contract between the generator and the game (design doc section 6; P0-02). This file holds the full current DDL, the required `meta` keys, the layer catalogue and the changelog. It is updated by every branch that changes the schema (P0-02 5.2) and by every spec that adds a layer.

**Current schema version: 6** (from generator 0.13.0, the chronology merge; version 5 is generator 0.11.0 to 0.12.0, version 4 generator 0.9.0 to 0.10.0, version 3 was generator 0.8.0). `SCHEMA_VERSION` in `worldgen/include/worldgen/db/schema.hpp`; equal to `PRAGMA user_version` and `meta.schema_version`.

## 1. DDL at version 6

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
```

`pass` (version 2): one row per natural pass site, written by the elevation stage in `id` order; empty when no site was placed or the stage did not run. Columns and their meaning in `p1-01-elevation.md` section 9.1.

`biome` (version 3): one row per loaded biome definition, written by the biome stage in ascending `id`: `id` = the biome `code`, `key` = the definition id, `name`, `colour` (0xRRGGBB), `water` (0 or 1); empty when the stage did not run (P1-04 3, B6). Twelve rows with the shipped data of generator 0.9.0 (codes 1 to 12; P1-00 6.6).

`climate` (version 3; column `temp_offset_dc` from version 4): one row, `id` 1, `wind_toward` = the prevailing wind's D8 code (0 E, 1 SE, 2 S, 3 SW, 4 W, 5 NW, 6 N, 7 NE), the direction it blows toward, and `temp_offset_dc` = the world's temperature offset in tenths of a degree Celsius (−50..50; −20..20 with the shipped tuning), written by the climate stage; empty when the stage did not run (P1-03 3, C2b, C7).

`region`, `region_border`, `region_biome` (version 5): written by the regions stage in `id`, (`a`, `b`) and (`region`, `biome`) order; empty when the stage did not run. One row per region, per pair of regions sharing a 4-adjacent cell pair, and per region and biome code present. Columns and their meaning in `p1-06-regions.md` section 3.

`race`, `race_biome`, `culture`, `god`, `age` (version 6): written by the chronology stage in `id`, (`race`, `biome`), `id`, `id`, `id` order; empty when the stage did not run. One row per race definition (present or not), per listed biome preference, per culture definition, per race (its creator god, `id` = race id), and per present race (its age). Columns and their meaning in `p3-03-chronology.md` section 5.2.

SQLite adds the automatic indexes `sqlite_autoindex_layer_1` for `layer.name`, `sqlite_autoindex_biome_1` for `biome.key`, `sqlite_autoindex_race_1` for `race.key` and `sqlite_autoindex_culture_1` for `culture.key`; the version 5 tables add none. The empty world is 18 pages, 73728 bytes (11 at version 5; 8 at versions 3 and 4, P0-02 amendments 15 and 16; the three version 5 tables add one root page each; the version 6 tables add the roots of `race`, `sqlite_autoindex_race_1`, `race_biome`, `culture`, `sqlite_autoindex_culture_1`, `god` and `age`, measured with the vendored SQLite 3.54.1).

File settings (P0-02 4): page size 4096, UTF-8, no auto-vacuum, `application_id` 0x554E4D50 (`UNMP`), `user_version` = schema version, written with `VACUUM INTO`. Layer blobs: format version 1, codec 0 (raw) or 1 (delta-varint) (P0-02 7).

## 2. Required `meta` keys at version 6

| Key | Value |
| --- | --- |
| `content_hash` | SHA-256 of the loaded data's canonical encoding, 64 lowercase hex digits (P0-03 8) |
| `format` | `the-unmapped-world` |
| `generator_version` | `MAJOR.MINOR.PATCH` |
| `param.civs_per_race` | `auto`, or `1`, `2`, `3`, `4` |
| `param.divine_activity` | `distant`, `normal` or `meddling` |
| `param.divine_script` | empty (readers also accept 64 lower-case hex digits, the P5 form) |
| `param.event_intensity` | `calm`, `normal` or `turbulent` |
| `param.history_detail` | `sparse`, `normal` or `dense` |
| `param.history_span` | decimal integer 2000 to 12000, canonical (no sign, no leading zero) |
| `param.map_size` | 512, 1024 or 2048 |
| `param.monster_pressure` | `low`, `normal` or `high` |
| `param.races` | one or more race ids (P0-03 definition id grammar) in race order, joined by `,` with no spaces, none twice, e.g. `elf,goblin,dwarf,faerie,human,orc` |
| `schema_version` | `6` |
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

From P1-00 section 9.2 and P3-00 6.1; the DDL becomes final in the spec of the branch that makes the change. Version 6 is in sections 1 and 2.

| Version | Change | Branch |
| --- | --- | --- |
| 7 | the history tables of P3-00 6.1 (`event_kind`, `event_kind_role`, `event_kind_value`, `phase`, `event`, `event_participant`, `event_cause`, `event_value`, `civilization`, `settlement`, `settlement_census`, `region_control`, `relation`) and `meta.history_end_year` | P3-04 |

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
