# World database schema reference

The contract between the generator and the game (design doc section 6; P0-02). This file holds the full current DDL, the required `meta` keys, the layer catalogue and the changelog. It is updated by every branch that changes the schema (P0-02 5.2) and by every spec that adds a layer.

**Current schema version: 1.** `SCHEMA_VERSION` in `worldgen/include/worldgen/db/schema.hpp`; equal to `PRAGMA user_version` and `meta.schema_version`.

## 1. DDL at version 1

Executed in this order, each as one statement, byte for byte as written (P0-02 5.3, 5.4):

```
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) STRICT, WITHOUT ROWID
CREATE TABLE layer (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, width INTEGER NOT NULL, height INTEGER NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL) STRICT
```

SQLite adds the automatic index `sqlite_autoindex_layer_1` for `layer.name`.

File settings (P0-02 4): page size 4096, UTF-8, no auto-vacuum, `application_id` 0x554E4D50 (`UNMP`), `user_version` = schema version, written with `VACUUM INTO`. Layer blobs: format version 1, codec 0 (raw) or 1 (delta-varint) (P0-02 7).

## 2. Required `meta` keys at version 1

| Key | Value |
| --- | --- |
| `content_hash` | SHA-256 of the loaded data's canonical encoding, 64 lowercase hex digits (P0-03 8) |
| `format` | `the-unmapped-world` |
| `generator_version` | `MAJOR.MINOR.PATCH` |
| `param.map_size` | 512, 1024 or 2048 |
| `schema_version` | `1` |
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
| `temperature` | `i16` | annual mean, 0.1 °C | `climate` | planned (P1 second pass) |
| `rainfall` | `u16` | mm per year | `climate` | planned |
| `biome` | `u8` | biome code (table `biome`) | `biomes` | planned |
| `fertility`, `ore`, `timber`, `fresh_water`, `habitability` | `u8` | score 0 to 255 on an absolute scale (P1-05) | `resources` | planned |
| `walk_area` | `u16` | walkable-area id, 0 none (P1-05) | `resources` | planned (added 2026-11-16) |
| `region` | `u16` | region id (table `region`), 0 on ocean | `regions` | planned |
| `test.ramp`, `test.hash`, `test.noise` | `u8`, `u8`, `i16` | P0 test pattern, 256 × 256 (P0-04 5.2) | `test_pattern` | P0 (schema 1) |

## 4. Planned changes

From P1-00 section 9.2; the DDL becomes final in the spec of the branch that makes the change.

| Version | Change | Branch |
| --- | --- | --- |
| 2 | `CREATE TABLE pass (id INTEGER PRIMARY KEY, range_id INTEGER NOT NULL, kind INTEGER NOT NULL, capped INTEGER NOT NULL, col_x INTEGER NOT NULL, col_y INTEGER NOT NULL, saddle_dm INTEGER NOT NULL, col_dm INTEGER NOT NULL, path BLOB NOT NULL) STRICT`, executed after `layer`'s; natural pass sites, one row per site in `id` order (`p1-01-elevation.md` section 9.1; proposals "Pass sites are stored in the world file"); with generator 0.6.0 | elevation revision 0.6.0 (passes; adopted) |
| 3 | `CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT` | biomes (P1 second pass) |
| 4 | tables `region` and `region_border` (DDL proposed in `p1-06-regions.md` section 3) | regions (P1 second pass) |

## 5. Changelog

| Version | Date | Change | Spec |
| --- | --- | --- | --- |
| 1 | 2026-11-15 | Initial schema: `meta`, `layer` | P0-02 |
| 1 | 2026-11-15 | No schema change: the layer catalogue gains the elevation and hydrology layers; `meta.stages` lists geography stages | P1-00, P1-01, P1-02 |
