# worldgen

The world generator library (design doc section 10). It has no engine dependencies and must never include or link Godot.

| Path | Holds |
| --- | --- |
| `include/worldgen/` | Public API, one subdirectory per `src/` area (`core/`, `db/`, `data/`, `stages/`, `inspect/`), plus `generate.hpp` and `version.hpp` |
| `src/core/` | Deterministic core (spec P0-01): fixed-point and wide integer math, decimal parsing, hashing, the random generator and streams, noise, SHA-256 |
| `src/db/` | World file schema, writer, reader and finalization (spec P0-02); the single place for SQLite runtime settings (page size, pragmas) |
| `src/data/` | TOML loader for settings and data files (spec P0-03) |
| `src/stages/` | Generator stages; in P0 only the `test_pattern` stage (spec P0-04 5.2) |
| `src/inspect/` | Map rendering, the text report and the file and logical hashes behind `worldgen-cli render`, `report` and `hash` (spec P0-04) |
| `src/generate.cpp`, `src/version.cpp` | The generation pipeline and `GENERATOR_VERSION` |
| `src/geo/` | Elevation, hydrology, climate, biomes (phase P1) |
| `src/history/` | Entities, tick loop, events (phase P3) |
| `src/derive/` | Sites, dungeons, quest hooks (phase P6) |
| `tests/` | doctest unit, property and golden tests (`worldgen-tests`); `tests/golden/` holds golden values, `tests/data/` committed TOML fixtures |

`src/geo/`, `src/history/` and `src/derive/` are created with their first source file; git does not keep empty directories. Each stage is implemented from its approved spec in `docs/specs/`. Add sources to `worldgen/CMakeLists.txt` and tests to `worldgen/tests/CMakeLists.txt`.
