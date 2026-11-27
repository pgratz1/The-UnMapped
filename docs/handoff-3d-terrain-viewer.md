# Hand-off brief: the 3D terrain viewer (second session)

Written by the lead session on 2026-11-27 for a separate Claude Code session on Paul's laptop. The lead session on the home machine keeps building P5 (history) at the same time. Read `CLAUDE.md` first; it applies in full.

## Goal

The "3D heightmap preview" of design section 13 (row P6) and section 11 ("Later: a 3D heightmap preview, which leads into terrain streaming"), with the terrain approach of design section 7 ("Terrain approach: Terrain3D first, with a test"). Read those three places in `docs/design/design.md` before anything else. A first stage is: open a generated `world.db` in the Godot project and fly over its terrain in 3D.

**Decided by Paul (2026-11-27): "Plain preview first".** The first stage loads the elevation and biome layers into Terrain3D and lets the user fly over them. The fly-across performance test of design section 7 (VR at 90 Hz, region loading), detail noise, river carving and streaming are later stages of their own.

## The boundary between the two sessions

- This session changes only `gdext/`, `game/`, `extern/` (to vendor Terrain3D), the build files those need, and its own files under `docs/specs/` and `docs/reviews/`.
- It never changes `worldgen/`, `tools/worldgen-cli/`, the world file's schema, golden files or `game/data/`. It reads `world.db` and nothing else. If the 3D side needs something the world file lacks, write an entry in `docs/design/proposals.md` and tell Paul; the lead session schedules it.
- Branch names start with `v3d/` (for example `v3d/01-spec`, `v3d/01-terrain`). Do not push to `main`. When a stage is green, tell Paul; the lead session merges it, because `main` moves under the history stages and the README and `CLAUDE.md` are edited there at every merge.
- Do not edit `README.md`, `CLAUDE.md`, `docs/questions-for-paul.md` or existing entries of `proposals.md` on a branch (appending a new proposals entry is fine; the lead keeps both sides at the merge). Put questions for Paul in the stage's spec and give them to him in chat.
- The world file changes with each history stage (generator 0.29.0 and schema 13 on `main` today; 0.30.0 and schema 14 soon). The geography layers and tables this work reads have been stable since P1, so rebase on `main` when it moves and expect no conflict outside build files.

## Rules that matter most here

- Spec before code: a spec in `docs/specs/` (role `spec-writer`), approved by Paul, then `viewer-engineer` builds it. Paul approves this one himself since no lead is in the session.
- C++ only, no GDScript. Scenes and resources hold layout and assets.
- Dependencies are vendored: Terrain3D goes into `extern/` at a pinned version with an entry in `extern/README.md`, never fetched by the build. Godot is pinned to 4.7.2 and godot-cpp to 10.0.0 because of Terrain3D v1.1.0 (`gdext/README.md`, "Pinned versions"). How a second GDExtension is built and placed in the run copy `<build>/game/` is the first thing the spec must settle (`gdext/README.md` describes the run copy).
- Nothing generated inside the source tree. Godot always runs on the run copy in the build tree.
- The extension must keep building on Linux and Windows (CI jobs `gdext-linux`, `gdext-windows`), and `gdext-headless-check` must stay green. A 3D scene needs a GPU, so its own checks are run by hand or kept to what a headless run can test (data loading, mesh or height data built from the layers).

## What to read in the code

- `gdext/src/world_file.*`, `world_layer.*`: the existing binding to `world.db`.
- `gdext/src/viewer/`: the 2D viewer, for how a world is opened and how options after `--` are parsed.
- `worldgen/include/worldgen/inspect/layer_catalogue.hpp`: the layers, their units and value texts.
- `docs/specs/schema.md`: the tables.
- Layers a terrain needs: `elevation` (16 m cells at every map size), `biome`, the hydrology layers (rivers, lakes, flow) from spec `docs/specs/p1-02-hydrology.md`, `region`; later the settlement and site tables for markers.

## Usage

Both sessions draw on one subscription limit. Follow `~/.claude/CLAUDE.md` (copy it from the home machine): few agents, tight briefs, Sonnet for builds and mechanical work.
