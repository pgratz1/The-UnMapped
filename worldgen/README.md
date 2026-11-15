# worldgen

The world generator library (design doc section 10). It has no engine dependencies and must never include or link Godot.

| Path | Holds |
| --- | --- |
| `include/worldgen/` | Public API |
| `src/geo/` | Elevation, hydrology, climate, biomes |
| `src/history/` | Entities, tick loop, events |
| `src/derive/` | Sites, dungeons, quest hooks |
| `src/db/` | Schema, writers, readers; the single place for SQLite runtime settings (page size, pragmas) |
| `tests/` | doctest unit, property and golden-seed tests |

The `src/` subdirectories are created with their first source file; git does not keep empty directories. Each stage is implemented from its approved spec in `docs/specs/`. Add sources to `worldgen/CMakeLists.txt` and tests to `worldgen/tests/CMakeLists.txt`.
