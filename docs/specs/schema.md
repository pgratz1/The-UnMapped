# World database schema reference

The contract between the generator and the game (design doc section 6; P0-02). This file holds the full current DDL, the required `meta` keys, the layer catalogue and the changelog. It is updated by every branch that changes the schema (P0-02 5.2) and by every spec that adds a layer.

**Current schema version: 4** (from generator 0.9.0, the biome variety merge; this file describes version 4 ahead of that merge; version 3 was generator 0.8.0). `SCHEMA_VERSION` in `worldgen/include/worldgen/db/schema.hpp`; equal to `PRAGMA user_version` and `meta.schema_version`.

## 1. DDL at version 4

Executed in this order, each as one statement, byte for byte as written (P0-02 5.3, 5.4; `DDL_STATEMENTS` in `worldgen/include/worldgen/db/schema.hpp`):

```
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) STRICT, WITHOUT ROWID
CREATE TABLE layer (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, width INTEGER NOT NULL, height INTEGER NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL) STRICT
CREATE TABLE pass (id INTEGER PRIMARY KEY, range_id INTEGER NOT NULL, kind INTEGER NOT NULL, capped INTEGER NOT NULL, col_x INTEGER NOT NULL, col_y INTEGER NOT NULL, saddle_dm INTEGER NOT NULL, col_dm INTEGER NOT NULL, path BLOB NOT NULL) STRICT
CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT
CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL, temp_offset_dc INTEGER NOT NULL) STRICT
```

`pass` (version 2): one row per natural pass site, written by the elevation stage in `id` order; empty when no site was placed or the stage did not run. Columns and their meaning in `p1-01-elevation.md` section 9.1.

`biome` (version 3): one row per loaded biome definition, written by the biome stage in ascending `id`: `id` = the biome `code`, `key` = the definition id, `name`, `colour` (0xRRGGBB), `water` (0 or 1); empty when the stage did not run (P1-04 3, B6). Twelve rows with the shipped data of generator 0.9.0 (codes 1 to 12; P1-00 6.6).

`climate` (version 3; column `temp_offset_dc` from version 4): one row, `id` 1, `wind_toward` = the prevailing wind's D8 code (0 E, 1 SE, 2 S, 3 SW, 4 W, 5 NW, 6 N, 7 NE), the direction it blows toward, and `temp_offset_dc` = the world's temperature offset in tenths of a degree Celsius (−50..50; −20..20 with the shipped tuning), written by the climate stage; empty when the stage did not run (P1-03 3, C2b, C7).

SQLite adds the automatic indexes `sqlite_autoindex_layer_1` for `layer.name` and `sqlite_autoindex_biome_1` for `biome.key`. The empty world is 8 pages, 32768 bytes (P0-02 amendments 15 and 16: the version 4 column adds no page).

File settings (P0-02 4): page size 4096, UTF-8, no auto-vacuum, `application_id` 0x554E4D50 (`UNMP`), `user_version` = schema version, written with `VACUUM INTO`. Layer blobs: format version 1, codec 0 (raw) or 1 (delta-varint) (P0-02 7).

## 2. Required `meta` keys at version 4

| Key | Value |
| --- | --- |
| `content_hash` | SHA-256 of the loaded data's canonical encoding, 64 lowercase hex digits (P0-03 8) |
| `format` | `the-unmapped-world` |
| `generator_version` | `MAJOR.MINOR.PATCH` |
| `param.map_size` | 512, 1024 or 2048 |
| `schema_version` | `4` |
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
| `fertility`, `ore`, `timber`, `fresh_water`, `habitability` | `u8` | score 0 to 255 on an absolute scale (P1-05) | `resources` | planned |
| `walk_area` | `u16` | walkable-area id, 0 none (P1-05) | `resources` | planned (added 2026-11-16) |
| `region` | `u16` | region id (table `region`), 0 on ocean | `regions` | planned |
| `test.ramp`, `test.hash`, `test.noise` | `u8`, `u8`, `i16` | P0 test pattern, 256 × 256 (P0-04 5.2) | `test_pattern` | P0 (schema 1) |

## 4. Planned changes

From P1-00 section 9.2; the DDL becomes final in the spec of the branch that makes the change. Version 4 is in sections 1 and 2.

| Version | Change | Branch |
| --- | --- | --- |
| 5 | tables `region` and `region_border` (DDL proposed in `p1-06-regions.md` section 3; planned as version 4 until the variety revision took 4) | regions (P1 second pass) |

## 5. Changelog

| Version | Date | Change | Spec |
| --- | --- | --- | --- |
| 1 | 2026-11-15 | Initial schema: `meta`, `layer` | P0-02 |
| 1 | 2026-11-15 | No schema change: the layer catalogue gains the elevation and hydrology layers; `meta.stages` lists geography stages | P1-00, P1-01, P1-02 |
| 2 | 2026-11-17 | 1 → 2 with generator 0.6.0: table `pass` added (natural pass sites, executed third, after `layer`); `meta.schema_version` and `user_version` 2; the empty world grows from 4 to 5 pages (P0-02 amendment 14). Proposals "Pass sites are stored in the world file" | P1-01 (fourth version) 9.1, P1-00 9.3 |
| 3 | 2026-11-17 | 2 → 3 with generator 0.8.0 (the biomes merge): tables `biome` (one row per biome definition, written by the biome stage) and `climate` (one row with the prevailing wind `wind_toward`, written by the climate stage) added, executed fourth and fifth, after `pass`; `meta.schema_version` and `user_version` 3; the empty world grows from 5 to 8 pages (`biome`, `sqlite_autoindex_biome_1`, `climate`; P0-02 amendment 15). Proposals "P1 schema changes" | P1-04 (second version) 3, P1-03 3, P1-00 9.2 |
| 4 | 2026-11-18 | 3 → 4 with generator 0.9.0 (the biome variety merge): table `climate` gains `temp_offset_dc INTEGER NOT NULL` (the per-world temperature offset in tenths of a degree); its DDL statement is replaced in place (still fifth); `meta.schema_version` and `user_version` 4; readers refuse version 3 worlds; the empty world stays 8 pages (P0-02 amendment 16). Planned `region` and `region_border` move to version 5. Proposals "More natural biomes, per-world temperature, a thinner coastal forest" | P1-03 3, P1-04 (third version) 3 and appendix B, P1-00 9.2 |
