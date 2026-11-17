# The Unmapped

A single-player, first-person, open-world RPG whose world and history are generated from a seed. Dark high fantasy, exploration first, flat screen and PC VR. Built on Godot 4 with all code in C++20.

## Read this first

- **Design doc:** `docs/design/design.md`. It is the specification for the whole project. Read the sections relevant to your task before starting.
- **Current work:** the world generator prototype, section 13 of the design doc (phases P0 to P6).
- **Current phase:** P1 Geography. P0 Foundation and P2 (Godot bring-up and viewer v0) are done. The elevation and hydrology stages (specs P1-01, P1-02) are merged; climate, biomes, resources and habitability, and regions (specs P1-03 to P1-06, approved) are next.
- **Next phase:** P3 History core, after Paul's review of P1.

## The design doc is read-only

`docs/design/design.md` is exported from a working document maintained outside this repository, and it is overwritten on every update. Do not edit it.

When the design is wrong, unclear or missing something, add an entry to `docs/design/proposals.md` and carry on with the most reasonable reading. Paul brings proposals back to the design conversation.

## Repository state

The repository follows the layout in section 10 of the design doc. Phase P0 is in place: the `worldgen` library holds the deterministic core, the world database layer, the TOML loader, the P0 test-pattern stage and the inspection code behind `report` and `hash`; `worldgen-cli` has `generate`, `render`, `report` and `hash`. Of phase P1 the shared geography algorithms (`src/geo/`), the stage pipeline and the first two geography stages, `elevation` (spec P1-01, `src/stages/elevation.cpp`, with the pass layout in `src/stages/elevation_passes.cpp`; it also writes table `pass`, schema version 2) and `hydrology` (spec P1-02, `src/stages/hydrology.cpp`), are in place, with the required tuning kind `worldgen` (ids `elevation` and `hydrology`) read from `game/data/worldgen/`. `generate` runs the implemented geography stages by default; `--until none` writes the P0 empty world. No later geography stage (climate onward) or history stage exists yet.

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

Each preset has a configure, a build and a test preset of the same name. Linux presets are hidden on Windows and the reverse.

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

`ctest` runs `worldgen-tests`, `worldgen-cli-tests` and two quick `worldgen-cli` checks (`--version`, unknown command). Run a doctest executable directly for options such as `-tc="<name>"` or `-s`. Tests write scratch files under the build tree or the system temp directory, never into the source tree. `worldgen-tests` and `worldgen-cli-tests` have a CTest timeout of 3600 s on every preset (CTest's default of 1500 s is too close to the sanitizer run), and `gdext-headless-check` one of 300 s.

Approximate full `ctest` time on a 6-core Linux machine:

| Preset | `ctest` |
| --- | --- |
| `linux-gcc-release`, `linux-clang-release` | about 90 seconds |
| `linux-gcc-debug`, `linux-clang-debug` | about four and a half minutes |
| `linux-clang-asan` | about 18 minutes (`worldgen-tests` alone about 12) |

Measured 2026-11-16 at generator 0.4.0. Windows times are not measured yet. `worldgen-tests` takes most of it. For quick iteration use `linux-gcc-release` or `linux-clang-release` (run a single doctest executable with `-tc=` or `-ts=` for narrower checks); run both debug presets and `linux-clang-asan` before handing work on.

### Determinism lint

`tools/lint/determinism_lint.py` (owned by the determinism auditor; see `tools/lint/README.md`) reports constructs banned in the output path. Python 3 only, no build needed. From the repository root:

```
python3 -B tools/lint/determinism_lint.py --self-test
python3 -B tools/lint/determinism_lint.py
```

The second command scans `worldgen/` without `worldgen/tests/` and `tools/worldgen-cli/` without its `tests/` directory (`report` and `hash` text is part of the cross-platform contract), the project's CMake files for floating-point flags, and the pinned toml++ configuration. Exit status 1 means findings. CI runs both. On Windows use `py -3 -B`. The rule list and the suppression syntax are in `tools/lint/README.md`.

### Cross-compiler world hashes

`tools/ci/world-hashes.sh` (POSIX shell; Git Bash on Windows) holds the case list of spec P0-04 section 9 (the `empty-*` cases with `--until none`, amendment 16 (d)) and the geography cases of spec P1-00 section 14 X5 (`geo-seed-0-512`, `geo-seed-42-512`, `geo-phrase-512`, `geo-seed-1-1024`), and is what CI runs. Every case reads the checkout's `game/data` through `--data`. For one built preset it generates the twelve cases and saves, per case, `<preset>-<case>.db`, `<preset>-<case>.txt` (`hash --db <file> --detail`) and `<preset>-<case>.report.txt` (`report --db <file>`) into `out/<source dir name>/world-hashes/` under the build root (`out/world-hashes/` when `UNMAPPED_BUILD_ROOT` is set). It refuses an output directory inside the source tree.

```
tools/ci/world-hashes.sh local linux-gcc-release linux-clang-debug   # generate for each built preset, then compare
tools/ci/world-hashes.sh generate linux-gcc-release                  # one preset
tools/ci/world-hashes.sh expect linux-gcc-release                    # hash --expect against the golden files
tools/ci/world-hashes.sh compare linux-gcc-release linux-clang-debug # compare saved results
tools/ci/world-hashes.sh cases
```

Each preset must be built first (default build tree, or `--build-dir DIR` for `generate` and `expect`). `compare` fails when any preset's text for any case differs from the others or is missing, names the case and the presets of each variant, and prints a `diff -u` of the detail lines, so the differing table or layer is visible. Generating all twelve cases takes about 15 seconds with a release preset, about two minutes with a debug preset and about six minutes with `linux-clang-asan` (the geography cases run the elevation and hydrology stages). It is not part of `ctest`, since one build tree cannot compare compilers.

### Golden values

- `worldgen/tests/golden/core_vectors.txt`: reference outputs of the deterministic core (spec P0-01 13.8), read by `worldgen-tests`.
- `worldgen/tests/golden/p0-worlds.txt`: SHA-256 of the world file for each P0 case, lines `<case> <hex>` sorted by case name, LF. Read by `worldgen-cli-tests` (skipped while the file is absent) and by `world-hashes.sh expect` in CI. Recorded by the validation specialist. Since the `worldgen` kind is registered, these worlds carry the content hash of the shipped `game/data`, so a change to the shipped tuning changes them too (P1-00 6.4).
- `worldgen/tests/golden/p1-worlds.txt`: the same for the geography cases of P1-00 14 X5, read by `world-hashes.sh expect` (a note, not a failure, while the file is absent). Recorded by the validation specialist.

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

- Library sources go in `worldgen/CMakeLists.txt`, library tests in `worldgen/tests/CMakeLists.txt` (`worldgen-tests`), CLI sources in `tools/worldgen-cli/CMakeLists.txt` and CLI integration tests in `tools/worldgen-cli/tests/CMakeLists.txt` (`worldgen-cli-tests`). Every new project target calls `unmapped_project_target()`.
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

`gdext-headless-check` is registered only when the pinned editor is found (in the tools folder above, on `PATH`, or named by `-DUNMAPPED_GODOT=<path>`). It generates `--seed 42 --test-pattern`, imports the project headless, runs the real viewer scene on that world and checks that it lists three layers and shows, for each, the same pixels as `worldgen-cli render`. It takes about 4 seconds. A clean build with the extension takes about 3.5 minutes on 6 cores with an empty ccache (godot-cpp is about 1,100 files; `linux-gcc-release`, measured 2026-11-15), against under a minute without it; with a warm ccache a clean build of the same tree takes about 3 seconds.

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
- `build` (one job per preset, all seven, on `ubuntu-latest` and `windows-latest`, 180-minute job timeout): configure, build and `ctest` under `$RUNNER_TEMP/unmapped`; then `world-hashes.sh generate` and `expect` (a warning, not a failure, while `p0-worlds.txt` is absent); upload the case files as artifact `world-hashes-<preset>`; finally fail if the checkout holds any generated file.
- `compare-hashes` (Linux, after every `build` job, also when some failed, not when the run is cancelled): downloads every `world-hashes-*` artifact and runs `world-hashes.sh compare` over every build preset in `CMakePresets.json`. A difference, or a preset with no artifact, fails it. This job is the P0 exit check: the empty worlds are byte-identical across GCC, Clang and MSVC.
- `gdext` (two jobs, `gdext-linux` with `linux-gcc-release` and `gdext-windows` with `windows-msvc-release`): the extension. Fetches the pinned editor with `tools/ci/fetch-godot.sh --tools-dir "$RUNNER_TEMP/unmapped/tools"` (cached per Godot version and digests file; a restored copy is re-verified), configures with `-DUNMAPPED_BUILD_GDEXT=ON -DUNMAPPED_GODOT=<editor>` into `build/<preset>-gdext`, fails straight after configure if `ctest -N` does not list `gdext-headless-check`, builds, runs `ctest`, prints the headless check's full log, then `world-hashes.sh generate` and `expect` under the label `<preset>-gdext` (artifact `gdext-world-hashes-<preset>`, a name `compare-hashes` does not download), and ends with the clean-checkout check. On failure it also lists the run copy of the project and uploads the headless check's files.
- `compare-gdext-hashes` (Linux, after `build` and `gdext`): `world-hashes.sh compare <preset> <preset>-gdext` for both presets, which proves that building `worldgen` position independent for the extension changes no output byte. A separate job, so that `compare-hashes` does not wait for the extension builds.

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
