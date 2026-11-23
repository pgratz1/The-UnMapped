# The Unmapped

A single-player, first-person, open-world RPG whose world and history are generated from a seed. Dark high fantasy, exploration first, flat screen and PC VR. Built on Godot 4 with all code in C++20.

## Read this first

- **Design doc:** `docs/design/design.md`. It is the specification for the whole project. Read the sections relevant to your task before starting.
- **Current work:** the world generator prototype, section 13 of the design doc (phases P0 to P6).
- **Current phases:** P4 Viewer v1 and P5 History depth, started 2026-11-22, in parallel (overview specs P4-00 and P5-00 first). Before them: P3 History core, started 2026-11-19. The overview spec (P3-00, `docs/specs/p3-00-history-overview.md`) splits it into specs P3-01 to P3-07. P3-01 (history data and settings; generator 0.12.0), P3-02 (names: `src/history/draw.cpp`, `src/history/names.cpp`), P3-03 (chronology; generator 0.13.0, schema 6) P3-04 (simulation engine; generator 0.14.0, schema 7) P3-05 (peoples; generator 0.15.0) and P3-06 (expansion and conflict; generator 0.16.0) are merged. P3-08 names and peoples follow-ups (change spec `docs/specs/p3-08-followups.md`; generator 0.17.0) is merged: region names without numerals (spec P1-06, data kind `placename` in `game/data/placenames/`), a lower-case "the" mid-sentence, the name filter (spec P3-02 section 11, data kind `name_filter` in `game/data/name_filters/`, source `src/data/kinds_names.cpp`), the faerie forest weights, dwarven upland sites and event kind 31. P3-09 conflict balance (generator 0.18.0; P3-06 section 20, P3-05 section 21, rules decided by Paul in `proposals.md`) is merged: event kinds 32 to 36 (`refugees_settled`, `tribute`, `war_won`, `search_abandoned`, `refuge_held`), the flight of a last refuge, the loser's cost, weariness, concealment (stream `hist.conflict.search`), the history notes of `worldgen/history/digest.hpp` (`src/history/digest.cpp`) and the short English-word list of the name filter. P3-10 wording, site and conflict follow-ups (change spec `docs/specs/p3-10-followups.md`; generator 0.19.0; P3-06 sections 21 and 22) is merged: event kinds 37 and 38 (`plague_outbreak`, `quiet_stretch`), cause clauses, one compass name per land type, faerie and dwarf sites, the conflict rules of a quarrel on awakening (held back below `history.conflict.quarrel_min_settlements`), an aim won only by holding the goal, foothold cessions and the end decision in the order of 21.6 (`end_decision` in `src/history/conflict_war.cpp`), the race field `hidden_years`, one lying-low definition (`PeoplesSubsystem::lie_low`), `history.engine.quiet_min_years`, and the `*-slow` suites in CI. P3-11 orc arrival, wording and name follow-ups (change spec `docs/specs/p3-11-followups.md`; generator 0.20.0; P3-05 section 23, P3-06 section 23, P3-02 section 14, P3-04 section 21) is merged: the race field `awakening_population` and the first colony of an awakened realm free of the pressure test (`first_colony_pending_` in `src/history/peoples.cpp`), event kinds 39 and 40 (`claim_settlements`, `settlements_ceded`), the faerie syllable lists, the repeated-syllable rule of the name builder, `tard` in the name filter and values one to nine rendered as words. Its section 12 lists what is left for the next change that alters world bytes. P3-07 the history report (spec `docs/specs/p3-07-history-report.md` r18; no world change, generator stays 0.20.0) is merged: `worldgen-cli report --db <world.db> --history [--level summary|full] [--out PATH] [--force]`, with the digest in `worldgen/history/digest.hpp` (`src/history/digest.cpp`, `digest_tables.cpp`: `read_history_tables`, `digest_history`, `history_note`) and the text in `src/inspect/history_report.cpp` (`build_history_report`); a world without a history gives `ErrorCode::history_missing` (exit 3). Its text is part of the cross-platform contract and is compared by `world-hashes.sh` at both levels. The world critic's remaining notes (`docs/reviews/p3-07-history-report-world-critic-r17.md` B1 to B6) and the audit's advisory A3 wait for the round after Paul's review. P3 was reviewed and approved by Paul on 2026-11-22. P0 Foundation, P1 Geography (approved by Paul on 2026-11-19) and P2 (Godot bring-up and viewer v0) are done. The reference map size for history is 1024.
- **Next phase:** P6 Derivation and polish, after P4 and P5.

## The design doc is read-only

`docs/design/design.md` is exported from a working document maintained outside this repository, and it is overwritten on every update. Do not edit it.

When the design is wrong, unclear or missing something, add an entry to `docs/design/proposals.md` and carry on with the most reasonable reading. Paul brings proposals back to the design conversation.

## Repository state

The repository follows the layout in section 10 of the design doc. Phase P0 is in place: the `worldgen` library holds the deterministic core, the world database layer, the TOML loader, the P0 test-pattern stage and the inspection code behind `report` and `hash`; `worldgen-cli` has `generate`, `render`, `report` and `hash`. Of phase P1 the shared geography algorithms (`src/geo/`), the stage pipeline and the six geography stages, `elevation` (spec P1-01, `src/stages/elevation.cpp`, with the pass layout in `src/stages/elevation_passes.cpp`; it also writes table `pass`, schema version 2), `hydrology` (spec P1-02, `src/stages/hydrology.cpp`), `climate` (spec P1-03, `src/stages/climate.cpp`: layers `temperature` and `rainfall`, and the one-row table `climate` with the wind and the per-world temperature offset) and `biomes` (spec P1-04, `src/stages/biomes.cpp`: layer `biome` and table `biome`, nine natural biomes; schema version 4) and `resources` (spec P1-05, `src/stages/resources.cpp`: the score layers `fertility`, `ore`, `timber`, `fresh_water`, `forage` and `habitability`, and layer `walk_area`) and `regions` (spec P1-06, `src/stages/regions.cpp`: layer `region` and tables `region`, `region_border` and `region_biome`; schema version 5), are in place, with the required tuning kind `worldgen` (ids `elevation`, `hydrology`, `climate`, `biomes`, `resources` and `regions`) read from `game/data/worldgen/` and the required data kind `biome` from `game/data/biomes/`. Of phase P3 the data kinds `race` (`game/data/races/`), `culture` (`game/data/cultures/`) and the fixed-id tuning kind `history` (`game/data/history/`, id `chronology`) are registered and required (spec P3-01, `src/data/kinds_history.cpp`), and `generate` parses the world-creation settings (`--history-span`, `--history-detail`, `--civs-per-race`, `--races`, `--event-intensity`, `--monster-pressure`, `--divine-activity`; `--divine-script` is refused until P5), which are recorded in `meta` as `param.*` keys. The first history stage, `chronology` (spec P3-03, `src/stages/chronology.cpp`: tables `race`, `race_biome`, `culture`, `god` and `age`; schema version 6), writes the races present, their awakening years, the ages and a creator god per race; `report` prints them in `[chronology]`. `generate` runs the implemented stages by default; `--until none` writes the P0 empty world. `render` also draws the composites `map`, `biomes` and `regions`. The stage `history` (spec P3-04, `src/stages/history.cpp`, with the engine in `src/history/engine.cpp`, the schedule in `schedule.cpp`, the event catalogue in `event_catalogue.cpp` and a placeholder founding rule in `stubs.cpp` kept for tests; schema version 7) writes tables `event_kind`, `event_kind_role`, `event_kind_value`, `phase`, `event`, `event_participant`, `event_cause`, `event_value`, `civilization`, `settlement`, `settlement_census`, `region_control` and `relation`, and `report` prints `[history]`; its tuning is the `history` ids `engine`, `report`, `peoples` and `conflict`. The shipped history is the peoples subsystem (spec P3-05, `src/history/peoples.cpp`, `peoples_geo.cpp`): event kinds 6 to 19, 31 and 37, the engine's kind 38 (`quiet_stretch`), and the conflict subsystem (spec P3-06, `src/history/conflict.cpp`, `conflict_war.cpp`): event kinds 20 to 30, 32 to 36, 39 and 40, claims, wars, battles, sieges, sackings, captures, truces and cessions, with the `war`, `peace` and `enmity` rows of table `relation`. History subsystems follow the rules of spec P3-04 section 6.9 (no founding inside a loop over the live lists, no reference held across a founding; lint rules `history-live-iteration` and `history-held-reference`). History code may use keyed draws only, never the sequential generator (lint rule `history-sequential-rng`).

```
CMakeLists.txt        top-level build; refuses in-tree builds
CMakePresets.json     configure, build and test presets
cmake/                build modules; UnmappedCompilerFlags.cmake holds every compiler flag
worldgen/             C++ library, no engine dependencies
  include/worldgen/   public API
  src/                core/, db/, data/, stages/, inspect/; geo/, history/, derive/ come with their first file
  tests/              doctest tests
tools/worldgen-cli/   command-line front end: generate, render, report, hash
tools/lint/           banned-construct lint (owned by the determinism auditor)
tools/ci/             world-hashes.sh, the cross-compiler world hash check
gdext/                C++ GDExtension: world.db binding and viewer logic (phase P2, viewer engineer)
game/                 Godot project, layout only; the viewer scene is game/viewer/viewer.tscn (phase P2); data/worldgen/ holds the shipped generator tuning
extern/               vendored third-party libraries at pinned versions, see extern/README.md
docs/                 design/, specs/, reviews/
.github/workflows/    continuous integration
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

**Models.** Run the main session on Fable. The subagents in `.claude/agents/` name their model in their frontmatter: Opus for `spec-writer`, `generator-engineer`, `test-writer`, `determinism-auditor` and `viewer-engineer`; Sonnet for `validation-specialist`, `build-engineer` and `world-critic` (Paul, 2026-11-22, to save usage). A validation failure that needs debugging to a root cause may be sent to an Opus agent. Start a fresh agent per task with a tight brief rather than resuming one whose context has grown large.

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

`README.md` carries the public status and the phase timeline. The lead updates its Status and Timeline sections whenever a stage merges to `main`, a phase starts or ends, or work in flight changes, in the same push.

## Build and test

Requirements: CMake 3.25 or newer, Ninja, and GCC, Clang or MSVC (Visual Studio 2022 or newer) with C++20. Everything else is vendored in `extern/`; the build never uses the network. The optional Godot extension also needs Python 3.8 or newer, and the pinned Godot editor to run it (see "Godot extension and viewer" below).

### Presets

| Preset | Compiler | Build type |
| --- | --- | --- |
| `linux-gcc-debug` | GCC | Debug |
| `linux-gcc-release` | GCC | Release |
| `linux-clang-debug` | Clang | Debug |
| `linux-clang-release` | Clang | Release |
| `linux-clang-asan` | Clang | Debug with AddressSanitizer and UndefinedBehaviorSanitizer |
| `windows-msvc-debug` | MSVC | Debug |
| `windows-msvc-release` | MSVC | Release |

Each preset has a configure, a build and a test preset of the same name; the test presets `linux-gcc-release-slow` and `windows-msvc-release-slow` run only the slow entries (label `slow`, see below), which the other test presets exclude. Linux presets are hidden on Windows and the reverse.

### Commands

Linux, from the repository root:

```
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

Replace `linux-gcc-debug` with any preset above. The build tree is `$HOME/.cache/the-unmapped/build/<source dir name>/<preset>/`; the executables below are inside it.

Windows, from an "x64 Native Tools Command Prompt for VS" (or after running `vcvars64.bat`), so that `cl` and Ninja are on the path:

```
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

The build tree is `%LOCALAPPDATA%\the-unmapped\build\<source dir name>\<preset>\`.

### Executables and tests

| Path in the build tree | What it is |
| --- | --- |
| `tools/worldgen-cli/worldgen-cli` | The command-line tool: `generate`, `render`, `report`, `hash` (spec P0-04) |
| `worldgen/tests/worldgen-tests` | doctest tests of the library: core, database layer, TOML loader, test-pattern stage, render, report and hashes |
| `tools/worldgen-cli/tests/worldgen-cli-tests` | doctest integration tests that run `worldgen-cli` as a child process |
| `tools/worldgen-cli/worldgen-cli-test-hooks` | `worldgen-cli` with a test-only fault hook, used by `worldgen-cli-tests`; never shipped |

`ctest` runs each doctest executable as several entries, each a disjoint group of its source files (doctest `-sf=`), so that they run in parallel; the test presets run four at a time (`"jobs": 4`; `ctest --preset <preset> -j 6` overrides it). The groups are declared with `unmapped_add_doctest_shards()` (`cmake/UnmappedTestShards.cmake`) at the end of `worldgen/tests/CMakeLists.txt` and `tools/worldgen-cli/tests/CMakeLists.txt`:

| CTest entry | What it runs |
| --- | --- |
| `worldgen-tests-elevation-golden`, `-elevation`, `-hydrology`, `-climate`, `-biomes`, `-biomes-inspect`, `-resources`, `-regions`, `-history`, `-chronology`, `-chronology-property`, `-engine`, `-engine-r12`, `-engine-worlds`, `-engine-worlds-resume`, `-peoples`, `-conflict`, `-conflict-worlds`, `-balance`, `-history-report` | `worldgen-tests` on that stage's test files (`-history`: the keyed draws and names tests under `worldgen/tests/history/`; `-chronology`: the golden, unit and guard files, `-chronology-property`: the property, schema and report files; `-engine-worlds`: the 512 world case and threads, `-engine-worlds-resume`: R1, resume and, in the slow suite, the 1024 cases; `-engine` also holds the skipped suite `engine-bench`; `-engine-r12`: the engine tests of spec revisions 12, 15 and 17; `-balance`: the P3-09 conflict balance tests; `-history-report`: the files `*history_report_*`, the P3-07 history report's fixture, block, corruption and 512 world tests) |
| `worldgen-tests-other` | `worldgen-tests` on every file no group names (core, data, database, geo, pipeline, inspection; a new test file lands here until it is given a group) |
| `worldgen-cli-tests-hydrology-elevation`, `-passes-climate`, `-biomes`, `-resources-generate`, `-regions`, `-history-settings`, `-chronology`, `-engine`, `-conflict` | `worldgen-cli-tests` on those files |
| `worldgen-cli-tests-other` | `worldgen-cli-tests` on every other file (among them `cli_history_report_test.cpp`, the P3-07 `report --history` tests) |
| `worldgen-tests-slow-elevation`, `-slow-engine-resume`, `-slow-peoples`, `-slow-conflict`, `-slow-climate`, `-slow-hydrology`, `-slow-engine`, `-slow-biomes-resources-regions`, `-slow-history-report` (the history report's 1024 cases), `-slow-other`; `worldgen-cli-tests-slow-history-regions`, `-slow-geography`, `-slow-other` | label `slow`, not run by the default test presets: the skipped `*-slow` suites (`-ts=*-slow --no-skip`) of those files (`SLOW` lines of `unmapped_add_doctest_shards()`), `-slow-other` every slow case of a file no `SLOW` line names. `ctest --preset linux-gcc-release-slow` (or `windows-msvc-release-slow`) runs them, four at a time; for another build tree, `ctest --test-dir <build tree> -L slow -j 4` (not `--preset <preset> -L slow`: the default presets exclude the label). Not registered under `linux-clang-asan` |
| `worldgen-tests-partition`, `worldgen-cli-tests-partition` | checks with `--count` (and `--count --no-skip`) that the groups and `-other` hold every test case exactly once and that no group is empty, and that the slow entries hold every case of the `*-slow` suites exactly once with no `SLOW` entry empty (the skipped suites `engine-bench`, `peoples-bench` and `p3-08-dump` are run by hand only) |
| `worldgen-cli-version`, `worldgen-cli-unknown-command` | two quick `worldgen-cli` checks |
| `gdext-headless-check` | only with the extension (see below) |

`ctest -R '^worldgen-tests'` runs the library entries only. The executables are unchanged: run one directly for the whole set or for options such as `-tc="<name>"`, `-ts=`, `-sf=` or `-s`. A group keeps a stage's files together, since its support headers cache inputs in memory per process; a group that grows past the others is split by naming fewer files. Every entry is safe to run alongside the others: scratch directories are per process (process id, or created atomically) and caches are in memory. Tests write scratch files under the build tree or the system temp directory, never into the source tree. Each `worldgen-tests-*` and `worldgen-cli-tests-*` entry has a CTest timeout of 3600 s on every preset, and `gdext-headless-check` one of 300 s.

Approximate full `ctest` time on a 6-core Linux machine, with the presets' four jobs:

| Preset | `ctest` |
| --- | --- |
| `linux-gcc-release`, `linux-clang-release` | about 55 seconds |
| `linux-gcc-debug`, `linux-clang-debug` | about four minutes |
| `linux-clang-asan` | about 17 minutes |

Measured 2026-11-19 at generator 0.11.0 on an otherwise quiet machine (`linux-gcc-release` 53 s, `linux-gcc-debug` 223 s, `linux-clang-debug` 235 s, `linux-clang-asan` 996 s). Windows times are not measured locally; CI job times vary by up to a factor of two between runs with the hosted runner's speed (`docs/reviews/ci-job-time-variance.md`). With `-j 1` it takes two to three times as long (measured at generator 0.9.0: release about two and a half minutes, `linux-clang-asan` about 44). Under `linux-clang-asan` the longest entries (`worldgen-tests-elevation-golden`, `worldgen-cli-tests-hydrology-elevation`, `worldgen-tests-hydrology`) take 6 to 7 minutes each, which is the floor however many jobs run. For quick iteration use `linux-gcc-release` or `linux-clang-release` (run a single doctest executable with `-tc=` or `-ts=` for narrower checks); run both debug presets and `linux-clang-asan` before handing work on.

### Determinism lint

`tools/lint/determinism_lint.py` (owned by the determinism auditor; see `tools/lint/README.md`) reports constructs banned in the output path. Python 3 only, no build needed. From the repository root:

```
python3 -B tools/lint/determinism_lint.py --self-test
python3 -B tools/lint/determinism_lint.py
```

The second command scans `worldgen/` without `worldgen/tests/` and `tools/worldgen-cli/` without its `tests/` directory (`report` and `hash` text is part of the cross-platform contract), the project's CMake files for floating-point flags, and the pinned toml++ configuration. Exit status 1 means findings. CI runs both. On Windows use `py -3 -B`. The rule list and the suppression syntax are in `tools/lint/README.md`.

### Cross-compiler world hashes

`tools/ci/world-hashes.sh` (POSIX shell; Git Bash on Windows) holds the case list of spec P0-04 section 9 (the `empty-*` cases with `--until none`, amendment 16 (d)) and the geography cases of spec P1-00 section 14 X5 (`geo-seed-0-512`, `geo-seed-42-512`, `geo-phrase-512`, `geo-seed-1-1024`) and the history cases of spec P3-03 (`hist-span-2000-512`, `hist-races-elf-orc-1024`) and P3-04 (`hist-dense-1024`), and is what CI runs. Every case reads the checkout's `game/data` through `--data`. For one built preset it generates the fifteen cases and saves, per case, `<preset>-<case>.db`, `<preset>-<case>.txt` (`hash --db <file> --detail`) and `<preset>-<case>.report.txt` (`report --db <file>`), and for every case whose world has a history (every case except `empty-*` and `pattern-*`; spec P3-07 amendment 18.6) also `<preset>-<case>.history.summary.txt` and `<preset>-<case>.history.full.txt` (`report --db <file> --history --level summary`, `--level full`) and `<preset>-<case>.territory.txt` (spec P4-01 13.6: lines `<year> <sha256>`, the SHA-256 of the PNG of `render --composite territory` at year 0, floor(E/2) and the history's end year E, ascending, each year once; the PNGs are rendered into a scratch directory beside the results and deleted), into `out/<source dir name>/world-hashes/` under the build root (`out/world-hashes/` when `UNMAPPED_BUILD_ROOT` is set). It refuses an output directory inside the source tree.

```
tools/ci/world-hashes.sh local linux-gcc-release linux-clang-debug   # generate for each built preset, then compare
tools/ci/world-hashes.sh generate linux-gcc-release                  # one preset
tools/ci/world-hashes.sh expect linux-gcc-release                    # hash --expect against the golden files
tools/ci/world-hashes.sh compare linux-gcc-release linux-clang-debug # compare saved results
tools/ci/world-hashes.sh cases
```

Each preset must be built first (default build tree, or `--build-dir DIR` for `generate` and `expect`). `compare` fails when any preset's text for any case (detail lines, world report and, for a case with history, both history reports and the territory hashes) differs from the others byte for byte or is missing, names the case, the file kind (`txt`, `report.txt`, `history.summary.txt`, `history.full.txt`, `territory.txt`) and the presets of each variant, and prints a `diff -u` of at most 200 lines per differing file (then the number of lines left out), so the differing table, layer or chronicle line is visible. Line ends are not normalised: a CR written on Windows is a difference. Generating all fifteen cases takes about a minute with a release preset (62 s with `linux-gcc-release`, measured 2026-11-22 at generator 0.20.0 on a shared machine; the cases run every implemented stage, history included); `local` with `linux-gcc-release` and `linux-clang-debug` took about ten minutes alongside a `ctest` run. The history reports add well under a second per case. The earlier figures of about two minutes with a debug preset and six with `linux-clang-asan` predate the history stages and are now low. It is not part of `ctest`, since one build tree cannot compare compilers.

### Golden values

- `worldgen/tests/golden/core_vectors.txt`: reference outputs of the deterministic core (spec P0-01 13.8), read by `worldgen-tests`.
- `worldgen/tests/golden/p0-worlds.txt`: SHA-256 of the world file for each P0 case, lines `<case> <hex>` sorted by case name, LF. Read by `worldgen-cli-tests` (skipped while the file is absent) and by `world-hashes.sh expect` in CI. Recorded by the validation specialist. Since the `worldgen` kind is registered, these worlds carry the content hash of the shipped `game/data`, so a change to the shipped tuning changes them too (P1-00 6.4).
- `worldgen/tests/golden/p1-worlds.txt`: the same for the geography cases of P1-00 14 X5, read by `world-hashes.sh expect` (a note, not a failure, while the file is absent). Recorded by the validation specialist.
- `worldgen/tests/golden/p3-worlds.txt`: the same for the history cases, read by `world-hashes.sh expect`. Recorded by the validation specialist.
- `worldgen/tests/golden/p1-01-elevation.txt`, `p1-02-hydrology.txt`, `p1-03-climate.txt`, `p1-04-biomes.txt`, `p1-05-resources.txt`, `p1-06-regions.txt`, `p3-01-data.txt` (content hashes of the three data roots, from `p3_data_reference.py`), `p3-02-names.txt` (keyed draws and names, from `p3_names_reference.py`), `p3-03-chronology.txt` (the chronology tables), `p3-04-engine.txt` (schedules and history tables, from `p3_engine_reference.py`), `p3-05-peoples.txt` (peoples histories, from `p3_peoples_reference.py`), `p3-06-conflict.txt` (conflict histories, from `p3_conflict_reference.py`), `p3-07-report.txt` (the history report: `<case> <level> <sha256>` for the world cases with history, the fixtures and `sample-42-1024`, plus each world case's `[stats]` lines; from `p3_report_reference.py`): each stage's golden vectors (layer and table hashes of fixture and shipped worlds), written by the test writer's independent reference in `worldgen/tests/reference/` and read by `worldgen-tests` and `worldgen-cli-tests` (the large cases by the `*-slow` suites). Never regenerated to make a C++ test pass.

Golden values describe the generator's output contract. A change that alters the bytes of any world file or golden vector must bump `GENERATOR_VERSION` (`worldgen/include/worldgen/version.hpp`) and re-record the golden values in the same change; the comparison across presets must pass before they are recorded. A schema change also needs a schema version bump, a spec update and a proposals entry (hard rule 7). Golden files are LF on every platform (`.gitattributes`).

### Choosing another build root

Presets cannot read an optional environment variable with a fallback, so `UNMAPPED_BUILD_ROOT` is applied with `-B` and the build and test steps name the directory:

```
export UNMAPPED_BUILD_ROOT=/some/fast/disk/the-unmapped
cmake --preset linux-gcc-debug -B "$UNMAPPED_BUILD_ROOT/build/linux-gcc-debug"
cmake --build "$UNMAPPED_BUILD_ROOT/build/linux-gcc-debug"
ctest --preset linux-gcc-debug --test-dir "$UNMAPPED_BUILD_ROOT/build/linux-gcc-debug"
```

Configure warns when `UNMAPPED_BUILD_ROOT` is set but the build directory is not under it. Continuous integration uses this form.

### Several worktrees

The default build tree includes the source directory name, so the main checkout and each git worktree get their own build trees and CMake caches. Two checkouts with the same directory name would collide; give one of them its own `UNMAPPED_BUILD_ROOT` with the `-B` form above.

### Compiler flags

All flags are set in `cmake/UnmappedCompilerFlags.cmake`:

- No fast-math on any compiler, and floating-point contraction off (`-fno-fast-math -ffp-contract=off` on GCC and Clang, `/fp:precise` on MSVC), for every target including vendored code. Never add `-ffast-math`, `-Ofast`, `/fp:fast` or `/fp:contract`.
- Project targets call `unmapped_project_target()`: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion` and more, or `/W4`, as errors. `-DUNMAPPED_WARNINGS_AS_ERRORS=OFF` turns off the error part for local experiments only.
- Vendored targets call `unmapped_vendored_target()`: warnings suppressed. Their headers are `SYSTEM` includes.
- SQLite compile options are set in one place only: the block marked `SQLITE CONFIGURATION` in `extern/CMakeLists.txt`. Runtime settings (page size, pragmas) belong in one place in `worldgen/src/db/`.
- ccache is used automatically when installed (`-DUNMAPPED_USE_CCACHE=OFF` to disable).

### Adding code

- Library sources go in `worldgen/CMakeLists.txt`, library tests in `worldgen/tests/CMakeLists.txt` (`worldgen-tests`), CLI sources in `tools/worldgen-cli/CMakeLists.txt` and CLI integration tests in `tools/worldgen-cli/tests/CMakeLists.txt` (`worldgen-cli-tests`). A new test file runs in the `-other` CTest entry; a large one gets its own `SHARD` line in the `unmapped_add_doctest_shards()` call at the end of the same file (pattern `*<file name>`, no directory). Every new project target calls `unmapped_project_target()`.
- Vendored libraries are linked as `unmapped::sqlite3`, `unmapped::doctest`, `unmapped::tomlplusplus` and `unmapped::stb_image_write`; with `UNMAPPED_BUILD_GDEXT=ON` also `unmapped::godot-cpp`.
- Extension sources go in `gdext/CMakeLists.txt` (`unmapped_gdext`); scenes and resources in `game/`. See `gdext/README.md`.

### Godot extension and viewer

Pinned versions, in summary. The record, with the evidence for the choice, is `gdext/README.md` ("Pinned versions"); the editor's SHA-512 digests are kept only in `tools/ci/godot-4.7.2-stable.sha512`:

| What | Version |
| --- | --- |
| Godot | **4.7.2-stable**, the newest stable Godot that Terrain3D supports: Terrain3D v1.1.0-stable (newest release, 2026-10-30) states "Supports Godot 4.5-4.8+", and Godot 4.8 is still in beta (checked 2026-11-15) |
| godot-cpp | **10.0.0-stable**, vendored in `extern/godot-cpp/`, built for GDExtension API **4.7** |

The extension is the CMake option `UNMAPPED_BUILD_GDEXT`, OFF by default; with it off nothing in the build changes. It builds with every preset except `linux-clang-asan`, where configure refuses it (a sanitized library cannot be loaded into the stock editor).

Install the pinned editor once with the fetch script (never done by the build; CI runs the same script):

```
tools/ci/fetch-godot.sh                       # Linux; on Windows run it from Git Bash
tools/ci/fetch-godot.sh --tools-dir DIR       # somewhere else; then configure with -DUNMAPPED_GODOT=<printed path>
```

It downloads the editor archive from the Godot release on GitHub, checks it and every extracted file against the SHA-512 digests in `tools/ci/godot-4.7.2-stable.sha512` (the archive digests are those of the release's `SHA512-SUMS.txt`), and installs into `~/.cache/the-unmapped/tools/godot-4.7.2-stable/` (`%LOCALAPPDATA%\the-unmapped\tools\godot-4.7.2-stable\` on Windows, with both `Godot_v4.7.2-stable_win64.exe` and the `_console.exe` the check runs), the folder CMake searches. It prints the editor's path. When the files are already there and match, it does nothing; when an installed file does not match, it stops and touches nothing. Needs curl, and unzip or Python 3. It ignores `UNMAPPED_BUILD_ROOT`, since CMake's lookup does.

Changing the Godot version means changing, together: the table above, `VERSION` in `tools/ci/fetch-godot.sh`, the digests file (renamed for the version), `UNMAPPED_GODOT_VERSION` and the editor names in `gdext/CMakeLists.txt`, and `gdext/README.md`.

Build the extension and run every test, including the headless check:

```
cmake --preset linux-gcc-debug -DUNMAPPED_BUILD_GDEXT=ON
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R gdext-headless-check -V    # the headless check alone
```

`gdext-headless-check` is registered only when the pinned editor is found (in the tools folder above, on `PATH`, or named by `-DUNMAPPED_GODOT=<path>`). It generates `--seed 42 --test-pattern` and a real `--seed 42 --map-size 512` world, imports the project headless, runs the real viewer scene on each world and checks that it lists every layer (three, and the real world's layers from `report`, which must include `biome`, `elevation` and `rainfall`) and the composites `map`, `biomes` and `regions`, and shows, for each, the same pixels as `worldgen-cli render`. It takes about 7 seconds. A clean build with the extension takes about 3.5 minutes on 6 cores with an empty ccache (godot-cpp is about 1,100 files; `linux-gcc-release`, measured 2026-11-15), against under a minute without it; with a warm ccache a clean build of the same tree takes about 3 seconds.

Open the viewer on a test world (Linux; the path after `--` must be absolute):

```
B=~/.cache/the-unmapped/build/$(basename "$PWD")/linux-gcc-debug
GODOT=~/.cache/the-unmapped/tools/godot-4.7.2-stable/Godot_v4.7.2-stable_linux.x86_64
"$B/tools/worldgen-cli/worldgen-cli" generate --seed 42 --test-pattern --out ~/.cache/the-unmapped/out/seed42.db
"$GODOT" --path "$B/game" -- ~/.cache/the-unmapped/out/seed42.db
```

`"$GODOT" --editor --path "$B/game"` opens the project in the editor. Never open `game/` itself: Godot always runs on the run copy `<build>/game/` (see below).

### Continuous integration

`.github/workflows/ci.yml` runs on pushes to `main`, on pull requests and by hand (`workflow_dispatch`):

- `determinism-lint` (one Linux job): the two lint commands above.
- `build` (one job per preset, all seven, on `ubuntu-latest` and `windows-latest`, 180-minute job timeout): configure, build and `ctest` (four entries at a time, from the test preset; the hosted Linux and Windows runners have 4 vCPUs for a public repository) under `$RUNNER_TEMP/unmapped`; on `linux-gcc-release` and `windows-msvc-release` only, then the slow entries (`ctest --preset <preset>-slow`); then `world-hashes.sh generate` and `expect` (a warning, not a failure, while `p0-worlds.txt` is absent); upload the case files (every `<preset>-*` file: worlds, detail lines, world reports and history reports) as artifact `world-hashes-<preset>`; finally fail if the checkout holds any generated file.
- `compare-hashes` (Linux, after every `build` job, also when some failed, not when the run is cancelled): downloads every `world-hashes-*` artifact and runs `world-hashes.sh compare` over every build preset in `CMakePresets.json`, which compares the detail lines, the world report, the two history reports and the territory hashes of every case. A difference, or a preset with no artifact or a missing file, fails it. This job is the P0 exit check: the empty worlds are byte-identical across GCC, Clang and MSVC.
- `gdext` (two jobs, `gdext-linux` with `linux-gcc-release` and `gdext-windows` with `windows-msvc-release`): the extension. Fetches the pinned editor with `tools/ci/fetch-godot.sh --tools-dir "$RUNNER_TEMP/unmapped/tools"` (cached per Godot version and digests file; a restored copy is re-verified), configures with `-DUNMAPPED_BUILD_GDEXT=ON -DUNMAPPED_GODOT=<editor>` into `build/<preset>-gdext`, fails straight after configure if `ctest -N` does not list `gdext-headless-check`, builds, runs `ctest`, prints the headless check's full log, then `world-hashes.sh generate` and `expect` under the label `<preset>-gdext` (artifact `gdext-world-hashes-<preset>`, a name `compare-hashes` does not download), and ends with the clean-checkout check. On failure it also lists the run copy of the project and uploads the headless check's files.
- `compare-gdext-hashes` (Linux, after `build` and `gdext`): `world-hashes.sh compare <preset> <preset>-gdext` for both presets (the same files, history reports included), which proves that building `worldgen` position independent for the extension changes no output byte. A separate job, so that `compare-hashes` does not wait for the extension builds.

Compiler cache: `gdext-linux` uses ccache (cache key: preset, GCC version, hash of `extern/godot-cpp/`; saved once per key), so a run with an unchanged godot-cpp mostly hits. `gdext-windows` uses none, and configures with `-DUNMAPPED_USE_CCACHE=OFF`: neither ccache nor sccache has been tried with this build (godot-cpp adds a public `/MP` flag to every MSVC compile), and a cache that cannot be checked here is a risk the job does not need. The release preset has no debug information, so `/Zi` against `/Z7` is not what rules it out. A cold Windows extension build is expected to take 10 to 20 minutes. The seven `build` jobs use no compiler cache.

Third-party actions are pinned to full commit SHAs.

### Build output lives outside the source tree

Paul's checkout is inside a Dropbox folder, so nothing generated may be written into the repository directory. This is a requirement on the presets, not a habit to remember:

- Every CMake preset sets `binaryDir` outside the tree. The default root is `$HOME/.cache/the-unmapped/` (`%LOCALAPPDATA%\the-unmapped\` on Windows), overridable with the `UNMAPPED_BUILD_ROOT` environment variable through `-B` (see "Choosing another build root" above). Layout under the root: `build/<source dir name>/<preset>/` for build trees (see "Several worktrees" above), `out/` for generated worlds, map images, reports and seed-sweep results (`worldgen-cli`'s default; `world-hashes.sh` uses `out/<source dir name>/world-hashes/`), `tools/` for downloaded tools such as the pinned Godot editor.
- The top-level `CMakeLists.txt` refuses a build directory that is the source tree or inside it. CMake still writes `CMakeCache.txt` and `CMakeFiles/` into the refused directory before stopping; delete them.
- `worldgen-cli`, tests and sweep scripts take explicit output paths and default to `out/` under the build root, never the current directory.
- Compiled GDExtension libraries are produced under the build root and placed where the Godot project loads them by a symlink or by a path in the `.gdextension` file, not by copying binaries into `game/`. The build does this by assembling a run copy of the project in the build tree, `<build>/game/`: one link per entry of `game/`, a real `bin/` that is the extension's output directory, and Godot's own `.godot/` cache. Godot is always run on that copy, so neither the library nor the cache is written into the repository (`gdext/README.md`).
- `.gitignore` covers the usual in-tree build directories and Godot's `.godot/` cache as a backstop only.

Anything meant to be kept in the repository (golden hashes, review images attached to `docs/reviews/`) is copied in deliberately and kept small.
