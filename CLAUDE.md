# The Unmapped

A single-player, first-person, open-world RPG whose world and history are generated from a seed. Dark high fantasy, exploration first, flat screen and PC VR. Built on Godot 4 with all code in C++20.

## Read this first

- **Design doc:** `docs/design/design.md`. It is the specification for the whole project. Read the sections relevant to your task before starting.
- **Current work:** the world generator prototype, section 13 of the design doc (phases P0 to P6).
- **Next phase:** P0 Foundation. Nothing in `worldgen/` exists yet.

## The design doc is read-only

`docs/design/design.md` is exported from a working document maintained outside this repository, and it is overwritten on every update. Do not edit it.

When the design is wrong, unclear or missing something, add an entry to `docs/design/proposals.md` and carry on with the most reasonable reading. Paul brings proposals back to the design conversation.

## Repository state

The repository currently holds a custom-engine style skeleton (`src/engine`, `src/render`, `src/platform` and similar). Phase P0 replaces it with the layout in section 10 of the design doc:

```
worldgen/     C++ library, no engine dependencies (geo, history, derive, db)
tools/        worldgen-cli: generate, render, report, hash
gdext/        C++ GDExtension: world readers, runtime generators, gameplay
game/         Godot project: scenes, assets, data; the viewer lives in game/viewer/
extern/       vendored third-party libraries at pinned versions
docs/         design/, specs/, reviews/
```

## Hard rules

1. **Bit-identical worlds.** The same seed, parameters and generator version must give a byte-identical `world.db` on Linux and Windows with GCC, Clang and MSVC. In `worldgen/`:
   - Integer and fixed-point math only for anything that reaches the database.
   - No standard-library transcendental functions or random distributions in the output path. Use the project's own.
   - One project random generator; streams derived by hashing the seed with a stage name or coordinates.
   - No iteration over unordered containers, no dependence on pointer values, thread timing or wall-clock time.
   - No fast-math flags.
   - SQLite at the vendored version, fixed page size and settings, rows written in a defined order, no timestamps.
2. **`worldgen` never depends on Godot.** The library and `worldgen-cli` build and run without the engine.
3. **C++ only.** No GDScript. Godot scenes and resources hold layout, assets and tuning data.
4. **Data in TOML.** Races, beasts, monsters, cultures, biomes and event rules are hand-edited TOML. SQLite is for generated output.
5. **Dependencies are vendored** in `extern/` at pinned versions. Do not add network fetches to the build.
6. **Spec before code.** Each generator stage has an approved spec in `docs/specs/` before it is implemented.
7. **The database schema is a contract.** A schema change needs a schema version bump, a spec update and a proposals entry.

## How work is organized

**Models.** Run the main session on Fable. The subagents in `.claude/agents/` are set to Opus in their frontmatter.

The main session acts as **lead**: it reads the design doc, breaks the current phase into stage tasks, delegates to the subagents in `.claude/agents/`, and merges finished work. The lead does not write stage code itself.

A stage moves through the roles in this order:

1. `spec-writer` writes the stage spec. Paul or the lead approves it.
2. `generator-engineer` implements the stage and `test-writer` writes its tests, each working from the spec separately.
3. `determinism-auditor` reviews the change.
4. `validation-specialist` builds every preset, runs the tests and a seed sweep, and records golden hashes.
5. `world-critic` reviews the output when a stage changes how a world looks or how its history reads.
6. The lead merges when every check passes. A failed check goes back to the engineer with the defect report.

`viewer-engineer` owns the Godot side, and `build-engineer` owns CMake, `extern/` and continuous integration.

## Merge gate

- Tests pass on every build preset.
- Golden-seed hashes are recorded and match across platforms.
- The determinism audit passes.
- Map images or a history report are attached for any stage that changes output.

One stage per branch, merged when green. Each phase ends with a review by Paul before the next phase starts.

## Build and test

Build presets, commands and the continuous integration setup are created in phase P0. The build engineer records them in this section when they exist.

### Build output lives outside the source tree

Paul's checkout is inside a Dropbox folder, so nothing generated may be written into the repository directory. This is a requirement on the presets, not a habit to remember:

- Every CMake preset sets `binaryDir` outside the tree. The default root is `$HOME/.cache/the-unmapped/` (`%LOCALAPPDATA%\the-unmapped\` on Windows), overridable with the `UNMAPPED_BUILD_ROOT` environment variable. Layout under the root: `build/<preset>/` for build trees, `out/` for generated worlds, map images, reports and seed-sweep results, `tools/` for downloaded tools such as the pinned Godot editor.
- The top-level `CMakeLists.txt` refuses an in-source build with an error.
- `worldgen-cli`, tests and sweep scripts take explicit output paths and default to `out/` under the build root, never the current directory.
- Compiled GDExtension libraries are produced under the build root and placed where the Godot project loads them by a symlink or by a path in the `.gdextension` file, not by copying binaries into `game/`.
- `.gitignore` covers the usual in-tree build directories and Godot's `.godot/` cache as a backstop only.

Anything meant to be kept in the repository (golden hashes, review images attached to `docs/reviews/`) is copied in deliberately and kept small.
