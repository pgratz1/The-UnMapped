# game

The Godot 4 project of The Unmapped: scenes, assets and TOML data, no scripts (design doc section 10; hard rule 3). Owned by the viewer engineer; started in phase P2. Pinned engine: **Godot 4.7.2-stable** (see `gdext/README.md` for why and how to install it).

## Contents

| Path | What |
| --- | --- |
| `project.godot` | Project settings. Main scene: the world viewer. |
| `unmapped.gdextension` | Loads the C++ extension from `res://bin/` (see below). |
| `unmapped.gdextension.uid` | The resource UID Godot gives the `.gdextension` file; committed as Godot recommends for `.uid` files. |
| `viewer/viewer.tscn` | World viewer v0: layout only. Its root is `ViewerScreen`, a C++ class; parts the code uses have unique names (`%OpenButton`, `%OpenDialog`, `%FileLabel`, `%LayerList`, `%MetaTree`, `%MapView`, `%StatusLabel`). Rearranging the layout in the editor is safe as long as those names stay. |

Folders from design doc section 10 (`core/`, `world/`, `rigs/`, `ui/`, `data/`) are added with their first file.

## Do not open this folder in Godot

Godot keeps a cache (`.godot/`) inside the project and loads the extension from `res://bin/`. Neither belongs in the repository, which may sit in a synced folder. Building with `-DUNMAPPED_BUILD_GDEXT=ON` assembles a **run copy** of this project under the build tree, `<build>/game/`, made of links to the files here plus a real `bin/` (the compiled extension) and a real `.godot/`. Open and run that copy:

```
GODOT=~/.cache/the-unmapped/tools/godot-4.7.2-stable/Godot_v4.7.2-stable_linux.x86_64
B=~/.cache/the-unmapped/build/<source dir name>/<preset>
"$GODOT" --path "$B/game" -- /absolute/path/to/world.db   # the viewer
"$GODOT" --editor --path "$B/game"                        # the editor
```

Edits made in the editor go through the links into this folder, except a file saved at the project root with "safe save" (in practice `project.godot`): see `gdext/README.md`, "How the project finds the extension".

## What Godot writes

Measured with Godot 4.7.2 (`--headless --import`, then a viewer run):

| Written | Where it goes with the run copy | If `game/` were opened directly |
| --- | --- | --- |
| `.godot/` cache: `extension_list.cfg`, `uid_cache.bin`, `global_script_class_cache.cfg`, `editor/` (filesystem cache, project metadata, scene folding state), `imported/` (empty: the project has no imported assets) | `<build>/game/.godot/`, about 40 KB | `game/.godot/`, about 30 KB, ignored by git but synced by Dropbox |
| `unmapped.gdextension.uid` | Already committed; the run copy links to it | Same content (`uid://dcb378ighbkva`), so no change |
| `*.import` files beside imported assets (images, models, audio) | Inside the linked folders, so in `game/`; none exist yet | Same |
| Scene files | Not rewritten by import or by running | Not rewritten |

Nothing else is written. `.import` files are small text files that record import settings and are meant to be committed with their assets; the imported data itself goes to `.godot/imported/`. The first asset added to the project brings the first `.import` file, which should be committed with it.

Godot cannot move `.godot/` out of the project folder: the only related setting, `application/config/use_hidden_project_data_directory`, switches between `.godot/` and `godot/`. Hence the run copy.
