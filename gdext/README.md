# gdext

The C++ GDExtension of The Unmapped (design doc section 10): `world.db` readers bound into Godot, and the world viewer's logic. Later it will hold runtime generators and gameplay. Owned by the viewer engineer; started in phase P2.

It links `worldgen` through its public headers and nothing else of it; `worldgen` and `worldgen-cli` never depend on it (hard rule 2). All logic is C++; the Godot project in `game/` holds layout only (hard rule 3).

## Pinned versions

| What | Version | Where |
| --- | --- | --- |
| Godot editor | **4.7.2-stable** (`4.7.2.stable.official.ed1daf0bf`) | Downloaded into the tools folder under the build root, see below; not in the repository |
| godot-cpp | **10.0.0-stable**, built for API **4.7** | Vendored in `extern/godot-cpp/` (`extern/README.md`) |
| `.gdextension` `compatibility_minimum` | `4.7` | `game/unmapped.gdextension` |

**Why 4.7.2.** The design doc (section 10) pins Godot to the newest stable version that the Terrain3D plugin supports. Checked on 2026-11-15:

- Terrain3D's newest release is **v1.1.0-stable** (2026-10-30, https://github.com/TokisanGames/Terrain3D/releases/tag/v1.1.0-stable). Its release notes, under "Requirements", say: "Supports Godot 4.5-4.8+." Terrain3D's installation guide (https://terrain3d.readthedocs.io/en/1.1/docs/installation.html) defers to the release notes: "See the release notes for each version to learn which versions of Godot are supported."
- Godot's newest stable release is **4.7.2-stable** (2026-08-18, https://github.com/godotengine/godot/releases/tag/4.7.2-stable). 4.8 is at beta 3 (2026-11-09, https://github.com/godotengine/godot-builds/releases), so it is not a candidate.
- So the newest stable Godot that Terrain3D supports is 4.7.2.

**Why godot-cpp 10.0.0.** Since version 10, godot-cpp is versioned separately from Godot. Its README (https://github.com/godotengine/godot-cpp/blob/10.0.0-stable/README.md) says: "Using the `api_version` parameter (see below), godot-cpp v10 can target Godot 4.3 or later", and "GDExtensions targeting an earlier version of Godot should work in later minor versions, but not vice-versa." 10.0.0-stable (2026-09-15) is its newest release and ships `extension_api-4-7.json`. That file equals the API dumped from the pinned 4.7.2 editor in every section except the version header, and the interface JSON is byte-identical (`extern/README.md`, Notes). Terrain3D v1.1.0 itself builds against a godot-cpp master commit from 2026-03 (submodule `60b5a419`), so it does not pin a godot-cpp tag that we could share.

Terrain3D is not integrated now; it only decides the version (milestone 2 confirms it).

## Getting the pinned editor

The build never downloads anything. Install the editor once into the tools folder under the build root (Paul approved this location):

Linux:

```
mkdir -p ~/.cache/the-unmapped/tools/godot-4.7.2-stable && cd ~/.cache/the-unmapped/tools/godot-4.7.2-stable
curl -fLO https://github.com/godotengine/godot/releases/download/4.7.2-stable/Godot_v4.7.2-stable_linux.x86_64.zip
curl -fLO https://github.com/godotengine/godot/releases/download/4.7.2-stable/SHA512-SUMS.txt
grep ' Godot_v4.7.2-stable_linux.x86_64.zip$' SHA512-SUMS.txt | sha512sum -c
unzip Godot_v4.7.2-stable_linux.x86_64.zip
```

Expected checksums (from `SHA512-SUMS.txt` of the release; the SHA-256 is GitHub's asset digest):

| File | SHA-512 | SHA-256 |
| --- | --- | --- |
| `Godot_v4.7.2-stable_linux.x86_64.zip` | `9aa00f7a605200940bce3027a567b782f49bd8e940dd06ae9e987bd65aee1b1467edd56ed84fcdcbdd44354bf613bdbb4e5d2913e925850368e150c59ed54c65` | `cadd3204e728a35d3f13adb7fd0d7902636b79f6b95c40c265eb73b6c35329e4` |
| `Godot_v4.7.2-stable_win64.exe.zip` | `83decd58fdf67b9d657958a1ae6bf1929c20785315a81effe245874cdc57acb709bf868e00778a96984338c1b29dafdb453c6847747694621c6ecf5da2259993` | `731980f9608d61333e5baf54a2ef17210acc7a538446c0cb9969f002aca1e953` |

Windows: download `Godot_v4.7.2-stable_win64.exe.zip` from the same release, check it with `certutil -hashfile Godot_v4.7.2-stable_win64.exe.zip SHA512`, and unzip it into `%LOCALAPPDATA%\the-unmapped\tools\godot-4.7.2-stable\`. The zip holds `Godot_v4.7.2-stable_win64.exe` and `Godot_v4.7.2-stable_win64_console.exe`; the console one prints to the terminal and is the one the headless check uses.

CMake looks for the editor there, then on `PATH`; `-DUNMAPPED_GODOT=<path>` or the environment variable `UNMAPPED_GODOT` names another copy. It must report version `4.7.2.stable`, or the headless check is not registered.

## Building

The extension is the CMake option `UNMAPPED_BUILD_GDEXT`, **OFF by default**. With it off, the build is exactly as without `gdext/`. Turn it on for any non-sanitizer preset:

```
cmake --preset linux-gcc-debug -DUNMAPPED_BUILD_GDEXT=ON
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

The option is cached, so later builds of that tree keep it. To keep a tree without the extension next to one with it, give the second its own directory, e.g. `-B ~/.cache/the-unmapped/build/<source dir name>/linux-gcc-debug-gdext`, and use `cmake --build <dir>` and `ctest --preset linux-gcc-debug --test-dir <dir>`.

With the option on:

- godot-cpp is built as a vendored target (warnings suppressed, the project's floating-point flags applied). Its bindings are generated by Python 3 into the build tree.
- `worldgen` and SQLite are built position independent (`CMAKE_POSITION_INDEPENDENT_CODE`), since they are linked into a shared library. This does not change any integer result; `tools/ci/world-hashes.sh` gives the same hashes with the option on and off.
- `unmapped_gdext` is a project target (`unmapped_project_target()`, warnings as errors).

Requirements in addition to the usual ones: Python 3.8 or newer (to generate the bindings).

Times on a 6-core Linux machine, no ccache: godot-cpp and the extension add about 1,180 compile steps; a clean `linux-gcc-debug` build with the extension takes about 3 minutes against about 25 seconds without. A rebuild after changing only `gdext/` takes seconds. With ccache (used automatically when installed), a clean rebuild of an unchanged godot-cpp is fast.

The sanitizer preset `linux-clang-asan` can build the extension, but the headless check is not registered there: an instrumented library cannot be loaded into the uninstrumented editor.

## How the project finds the extension

Godot loads an extension from a `.gdextension` file inside the project, and writes its cache (`.godot/`) inside the project. To keep both the library and the cache out of the repository (it may sit in a synced folder), Godot never runs on `game/` itself but on a **run copy** that the build assembles under the build tree:

```
<build>/game/                     e.g. ~/.cache/the-unmapped/build/<source dir>/<preset>/game/
  project.godot        -> <repo>/game/project.godot
  unmapped.gdextension -> <repo>/game/unmapped.gdextension
  viewer/              -> <repo>/game/viewer/
  ...                     one link per entry of game/
  bin/                    real folder: the extension's output directory
    libunmapped_gdext.so  (Windows: unmapped_gdext.dll)
  .godot/                 real folder: Godot's cache
```

- The library is built straight into `<build>/game/bin/`. `game/unmapped.gdextension` names it as `res://bin/libunmapped_gdext.so` (Linux) and `res://bin/unmapped_gdext.dll` (Windows), relative to the project, so the committed file is the same on every machine.
- After each build of the library, `gdext/cmake/assemble_project.cmake` makes one link per entry of `game/` (new files appear without reconfiguring; links to deleted entries are removed) and writes `.godot/extension_list.cfg`, so the viewer runs straight after a build.
- Links: symbolic links on Linux. On Windows, folders are directory junctions (no administrator rights or Developer Mode needed); files are symbolic links where Windows allows them, else hard links, else copies (with a warning). **Not yet tried on Windows.**
- Each build tree has its own run copy, so several presets and worktrees never share a library or a cache.
- The editor edits the repository's files through the links: scenes saved in `viewer/` land in `game/viewer/`.
- One caveat: when the editor saves a file at the project root (in practice `project.godot`, from Project Settings) with "safe save" on, it writes a temporary file and renames it over the link, so the run copy gets a real file and the repository copy is not updated. The next build notices that the file differs from the repository's and warns, without overwriting it; copy the change across by hand. Project settings are rarely changed and are best edited in `game/project.godot` directly.

Do not open `game/project.godot` itself in the editor: the extension is not there (no `bin/`), and the editor would create `game/.godot/` in the repository.

## Classes

All in `gdext/src/`, registered at the scene level:

| Class | Base | Role |
| --- | --- | --- |
| `WorldFile` | `RefCounted` | Data access: a `world.db` opened read-only through `worldgen::db::WorldReader`. `open(path)` returns false with `get_error_code()` (worldgen's code name: `file_unreadable`, `not_sqlite`, `not_world`, `unsupported_schema_version`, `meta_key_missing`, `meta_value_invalid`, `corrupt_layer`, ...) and `get_error_message()` (worldgen's plain-English message). `get_meta_entries()`, `get_meta_value(key)`, `get_layer_count()`, `get_layer_names()`, `get_layer_info(i)` (id, name, width, height, element_type, codec, encoded_size; no decoding), `read_layer(name)`. |
| `WorldLayer` | `RefCounted` | A decoded layer: integer cells, `get_cell_text(x, y)` (exact decimal, also for u64), `get_min_text()`, `get_max_text()`, and `to_image()`, an RGB8 `Image` made by worldgen's `render_rgb` with the layer's registered palette, scale 1, the same pixels as `worldgen-cli render`. |
| `ViewerModel` | `RefCounted` | The model of the viewer screen: the open world, the selected layer, decoded images; signals `world_opened`, `world_open_failed`, `world_closed`, `layer_selected`, `layer_failed`. No Controls; a VR view can use it as is. |
| `LayerMapView` | `Control` | Presentation: draws one layer image with nearest filtering, pan (drag with left or middle button), zoom about the cursor (wheel), fit (double click); signal `cell_hovered(x, y)`. |
| `ViewerScreen` | `Control` | The flat view: root of `game/viewer/viewer.tscn`; finds its parts by unique name, wires them to a `ViewerModel`, reads the command line, runs the headless check. |

Integers stay integers up to `WorldLayer`; floats appear only on the Godot side (zoom, pan) and never reach worldgen. The viewer lists whatever layers the file holds, so the layers of phase P1 appear without viewer changes. Every layer is shown with worldgen's registered palette (gray in P0); per-layer palettes come with P1 through `render_rgb`.

## Opening the viewer

After building with the option on (Linux, `linux-gcc-debug` tree; substitute your preset):

```
B=~/.cache/the-unmapped/build/$(basename "$PWD")/linux-gcc-debug
GODOT=~/.cache/the-unmapped/tools/godot-4.7.2-stable/Godot_v4.7.2-stable_linux.x86_64
"$B/tools/worldgen-cli/worldgen-cli" generate --seed 42 --test-pattern --out ~/.cache/the-unmapped/out/seed42.db
"$GODOT" --path "$B/game" -- ~/.cache/the-unmapped/out/seed42.db
```

Without a path after `--`, use "Open world..." to pick a file. The path must be absolute: Godot changes the working directory to the project. To edit the scenes in the editor: `"$GODOT" --editor --path "$B/game"`.

## Headless check

CTest test `gdext-headless-check`, registered when the option is on, tests are built, the build is not a sanitizer build and the pinned editor is found. It runs `gdext/cmake/headless_check.cmake`:

1. `worldgen-cli generate --seed 42 --test-pattern` into `<build>/gdext/headless-check/`, and `worldgen-cli render` of each of its three layers to PNG.
2. `godot --headless --path <build>/game --import`: the editor loads the project and the extension.
3. `godot --headless --path <build>/game -- <world> --check <dir> --expect-layers 3`: the real viewer scene opens the world, the check (`gdext/src/viewer/viewer_check.hpp`) chooses each layer through the layer list and compares the image handed to the map view with the PNG, pixel for pixel; it also checks the meta panel and two reader errors.

It fails unless every step exits 0 and the viewer prints `UNMAPPED-CHECK: PASS`. Run it alone with `ctest --preset <preset> -R gdext-headless-check -V`.
