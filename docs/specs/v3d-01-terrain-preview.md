# V3D-01 Terrain preview

Status: **r3, 2026-11-27, as built** (r3 records the first passing CI run). r1 was approved by Paul on 2026-11-27 with all six recommendations of section 14 accepted; r2 records where the build differs from r1 and the findings of section 13 (revision notes at the end). First stage of the 3D terrain viewer track (`docs/handoff-3d-terrain-viewer.md`). Design doc section 7 ("Terrain approach: Terrain3D first, with a test"; "Fixed world" and threading, 648), section 10 (engine and terrain rows, 890 to 897), section 11 ("Later: a 3D heightmap preview", 950) and section 13 (row P6, 1117). Paul's decision of 2026-11-27: **plain preview first**, the `elevation` and `biome` layers loaded into Terrain3D and flown over. Builds on the viewer as built (`gdext/README.md`: `WorldFile`, the run copy `<build>/game/`, `gdext-headless-check`).

**No world change.** The stage reads `world.db` and writes nothing to it or anywhere else in the source tree. `GENERATOR_VERSION`, the schema version, every golden file, `world-hashes.sh` and its cases are unchanged; nothing under `worldgen/`, `tools/worldgen-cli/` or `game/data/` changes. Floating point is allowed throughout this stage: nothing it computes reaches a world file. Owner: viewer engineer; build engineer for sections 3 and 11.

**Not in this stage** (later stages of the track, not specified here): the fly-across performance test of design section 7 (VR at 90 Hz, player-following collision, region loading and unloading), streaming of regions around the camera, upsampling with seeded detail noise and 128 m chunks, river channels carved from the flow rasters, roads, markers for settlements and sites, the terrain interface of design 7 (height at a point, request or release an area, cut a hole), VR.

## 1. Purpose

After this stage a generated world can be opened in a 3D scene of the Godot project and flown over on a flat screen. Its terrain is the world's `elevation` layer at one vertex per 16 m cell, at true scale, coloured by the world's `biome` layer with the same colours as the 2D map. Sea is a flat plane at sea level, and lakes are drawn flat at their surface. The preview is the first use of Terrain3D in the project. It shows whether the plug-in builds from vendored source next to our extension and takes a whole world through its API. It invents no detail, so what is on screen is what the world file holds.

## 2. Parts

| Area | Files | Owner |
| --- | --- | --- |
| Terrain3D vendored source | new `extern/terrain3d/`, `extern/README.md` entry, block `TERRAIN3D CONFIGURATION` in `extern/CMakeLists.txt` | build engineer |
| build option, run copy | top-level option `UNMAPPED_BUILD_TERRAIN3D`, `gdext/CMakeLists.txt`, `gdext/cmake/assemble_project.cmake`, new `gdext/terrain3d/terrain.gdextension.in` | build engineer |
| terrain data (C++ only, no Godot types) | new `gdext/src/terrain/terrain_data.hpp`, `.cpp` | viewer engineer |
| fly state (C++ only, no Godot types) | new `gdext/src/terrain/fly_state.hpp`, `.cpp` | viewer engineer |
| scene logic | new `gdext/src/terrain/terrain_preview.hpp`, `.cpp` (class `TerrainPreview`, registered in `register_types.cpp`) | viewer engineer |
| check | new `gdext/src/terrain/terrain_check.hpp`, `.cpp`; new `gdext/cmake/terrain_check.cmake` | viewer engineer |
| unit tests | new `gdext/tests/terrain_data_test.cpp`, `fly_state_test.cpp` (executable `gdext-terrain-tests`) | viewer engineer |
| scene | new `game/viewer3d/terrain_preview.tscn` (layout only) | viewer engineer |
| CI | `.github/workflows/ci.yml`, jobs `gdext-linux` and `gdext-windows` only | build engineer |
| docs | `gdext/README.md` (section "Terrain preview"), `extern/README.md` | both |

Read from the world, all through `WorldFile` (no SQL in the extension):

| Item | Type | Required | Use |
| --- | --- | --- | --- |
| layer `elevation` | `i16`, N x N, decimetres above sea level, < 0 ocean, > 0 land, never 0 | yes | heights |
| layer `water_depth` | `u16`, decimetres of standing water: ocean −e, lake (lake level − e), else 0 (P1-02) | no | lake surfaces |
| layer `biome` | `u8`, biome code | no | colours |
| table `biome` | `WorldFile::biome_records()` (id, key, name, colour 0xRRGGBB, water) | with `biome` | colours, HUD |
| layer `region`, table `region` | `u16`; `WorldFile` region records, `worldgen::inspect::region_name` | no | HUD |

## 3. Vendoring and build (item 1)

### 3.1 Options weighed

| Option | For | Against |
| --- | --- | --- |
| **(a) vendored source, built by our CMake** against `extern/godot-cpp` 10.0.0, API 4.7 | no binaries in the repository (Paul's checkout is in Dropbox); the same compilers, CRT and flags as `unmapped_gdext`; Linux and Windows from one build; no SCons; the pin is a source tree with a tree hash, checked like godot-cpp | our CMake must reproduce what Terrain3D's `SConstruct` does (V13); Terrain3D v1.1.0 is built upstream against a godot-cpp master commit of 2026-03 (`60b5a419`), not 10.0.0, so it may not compile unchanged |
| (b) vendored source, built by SCons as an external step | the upstream build, unchanged | SCons and its Python packages become a requirement of every machine and CI job that builds the extension; a second build system; still compiled against its own godot-cpp |
| (c) the release's prebuilt libraries vendored in `extern/` | no compile; the exact upstream binaries | several binary files of unknown size in a Dropbox checkout, re-committed at every update; built by a third party, trusted only by digest (like `tools/ci/godot-4.7.2-stable.sha512`); debug symbols and CRT not ours; the upstream build may not load next to ours if the two use different godot-cpp versions (they do not share code, so this is not expected to matter) |

**Recommendation: (a).** It is the only option that keeps both rules "vendored in `extern/`" and "no generated or binary files in the source tree", and it adds no build system. If V13 shows that Terrain3D v1.1.0 does not compile against godot-cpp 10.0.0 without edits, the engineer stops and reports the errors. Paul then chooses (Q-V3D-01-1) between a second godot-cpp vendored at Terrain3D's submodule commit for this target only, and option (c). Vendored files are never edited.

**As built (r2).** V13 was false: 16 errors in 9 files, all from godot-cpp API changes. Paul decided on 2026-11-27 (`proposals.md`, "Terrain3D is pinned with Godot and godot-cpp as three versions"): godot-cpp stays at 10.0.0; Terrain3D is built by our CMake with the project's compatibility header `extern/terrain3d-compat/terrain3d_compat.hpp` force-included into every Terrain3D source, and **one** vendored line is edited (`extern/terrain3d/src/terrain_3d.cpp:647`, `String text = region_loc;` to `String text = String(region_loc);`), kept as the patch `extern/terrain3d-compat/terrain3d-godot-cpp-10.patch` and listed in `extern/README.md`. Godot, godot-cpp and Terrain3D are one pin of three versions, changed together. If an update needs more than 8 edited vendored lines, or any edit that changes behaviour, the build switches to the release's prebuilt libraries fetched once into the tools folder with pinned digests (like the Godot editor), not to a second godot-cpp. The rule "vendored files are never edited" above holds with that one exception.

### 3.2 What is vendored

`extern/terrain3d/` holds the files of the GitHub tag archive of `v1.1.0-stable`, kept byte-identical (r2: except the one line of 3.1), minus what the runtime does not need. The project's own `extern/terrain3d-compat/` (the header and the patch, not from the archive) sits beside it:

| Kept | Removed |
| --- | --- |
| `src/` (C++ sources, headers, shader sources), the license file, `README.md`, `project/addons/terrain_3d/terrain.gdextension` (reference only: entry symbol, compatibility), and any file under `project/addons/terrain_3d/` that the C++ loads by a `res://` path at run time (V14) | `SConstruct`, `doc/`, `.github/`, the demo, the editor plug-in (`plugin.cfg` and every `.gd` file: we never run GDScript, hard rule 3), tools, icons unless V14 needs them |

`extern/README.md` gets a row (name Terrain3D, version `v1.1.0-stable`, tag commit SHA, target `unmapped::terrain3d` only with `-DUNMAPPED_BUILD_TERRAIN3D=ON`, MIT, archive URL), the archive's SHA-256 (recorded at download, since no published checksum is known), the key file hashes and the tree hash computed as for godot-cpp, and a note listing what was kept and removed, that nothing was edited, the godot-cpp it is compiled against, and the size in the repository.

### 3.3 CMake

- New cache option **`UNMAPPED_BUILD_TERRAIN3D`, default OFF.** Configure fails with `UNMAPPED_BUILD_TERRAIN3D needs UNMAPPED_BUILD_GDEXT=ON` if it is on without the extension. It is separate from `UNMAPPED_BUILD_GDEXT` so that a 2D-viewer build does not compile Terrain3D. With it off, the build is exactly as today, apart from section 3.4's lock and the new `TerrainPreview` sources.
- Target `unmapped_terrain3d` (alias `unmapped::terrain3d`): a SHARED library from an explicit list of `extern/terrain3d/src/*.cpp`, defined in the block `TERRAIN3D CONFIGURATION` of `extern/CMakeLists.txt` (the one place for its defines and options). It is C++17 and links `unmapped::godot-cpp`; each extension statically links its own copy, which is normal for GDExtensions. It calls `unmapped_vendored_target()`, so warnings are suppressed and the project's floating-point flags apply. Its symbol visibility, defines and include directories come from what `SConstruct` sets (V13). The editor documentation data that `SConstruct` may generate is left out (V13). As built: 15 sources, include directory `src/`, no defines of its own, hidden visibility, and the compatibility header force-included (`-include` on GCC and Clang, `/FI` on MSVC).
- Output: `<build>/game/addons/terrain_3d/bin/libterrain3d.so` (Windows `terrain3d.dll`, no import library), every configuration to that one folder, PDB outside the run copy (as `unmapped_gdext`).
- `unmapped_gdext` does not link `unmapped_terrain3d`. It reaches Terrain3D only through Godot's `ClassDB` (`ClassDB::class_exists`, `ClassDB::instantiate`, `Object::set`, `get`, `call`). If V15 finds a supported C++ way to link the classes, it is not used in this stage.

### 3.4 Run copy

With the option on, `assemble_project.cmake` gets `-DTERRAIN3D=ON` and:

1. reserves `addons` (as `.godot` and `bin`): `<build>/game/addons/terrain_3d/` and its `bin/` (with a `.gdignore`) are real folders. If `game/` itself has an `addons` entry, it fails with a message to raise the conflict, since nothing in `game/` needs one now;
2. writes `<build>/game/addons/terrain_3d/terrain.gdextension` from `gdext/terrain3d/terrain.gdextension.in` (configure-time `configure_file`, so it is generated in the build tree). It holds Terrain3D's `entry_symbol` (V14), `compatibility_minimum = "4.7"` and `reloadable = false`, and names `res://addons/terrain_3d/bin/libterrain3d.so` and `res://addons/terrain_3d/bin/terrain3d.dll` for both `debug` and `release` on x86_64 (as `game/unmapped.gdextension`);
3. links any runtime resource files found by V14 into `<build>/game/addons/terrain_3d/` with the same link rules as today;
4. writes `.godot/extension_list.cfg` as the two lines `res://unmapped.gdextension` and `res://addons/terrain_3d/terrain.gdextension`. With the option off it writes the one line of today and removes `addons/` from the run copy, so a tree switched off loads no stale library.

The CTest tests `gdext-headless-check` and `gdext-terrain-check` both run `--import` on the same run copy, so both get `RESOURCE_LOCK godot-run-copy`. This property is the only change to `gdext-headless-check`.

## 4. Starting the preview (item 2)

**Recommendation: a separate scene, `res://viewer3d/terrain_preview.tscn`**, root `TerrainPreview` (a `Node3D`), started by naming the scene on Godot's command line. The 2D viewer (`viewer.tscn`, still the main scene) and its check do not change. A Terrain3D node in a scene file would fail to load in a build without Terrain3D, so `TerrainPreview` creates it in code. Q-V3D-01-6 asks whether a later stage should add "open in 3D at this cell" to the 2D viewer.

```
"$GODOT" --path "$B/game" res://viewer3d/terrain_preview.tscn -- /abs/path/world.db [options]
```

Options after `--` (the world path is the first argument that does not start with `--`; it must be absolute):

| Option | Meaning |
| --- | --- |
| `--at <x> <y>` | start above cell (x, y) instead of the default start (8.3) |
| `--alt <m>` | start altitude in metres above sea level (default 8.3) |
| `--heading <deg>` | 0 north, 90 east (default 0) |
| `--pitch <deg>` | −89 to 89, negative down (default −25) |
| `--speed <v>` | one of the speeds of 8.2 (default 200) |
| `--size <w> <h>` | window size |
| `--screenshot <file.png>` | save the window 2.0 s (process time) after the terrain is added, then quit with status 0; needs a display |
| `--bench` | section 10 |
| `--check <dir>` | section 9.2; `--expect-terrain3d present\|absent` |
| `--no-terrain3d` | behave as if the class `Terrain3D` did not exist (for the check) |

Refusals go to the status label and to stderr; with `--screenshot`, `--bench` or `--check` the scene also quits with status 1. Exact texts:

- `--at needs two cells`, `--at <x> <y>: not a cell of this map (0 to <N-1>)`, `--alt <v>: not a number`, `--heading <v>: not a number`, `--pitch <v>: not between -89 and 89`, `--speed <v>: not one of 10, 20, 50, 100, 200, 500, 1000, 2000`, `--screenshot needs a display`, `--screenshot cannot be combined with --bench or --check`, `--expect-terrain3d <w>: present or absent`.

Without a world path, the status label reads `No world given: pass an absolute path after --, or use Open world...`. An **Open world...** button opens a `FileDialog` (filesystem access, filter `*.db`), and loading a world replaces the one shown.

## 5. Data path (item 3)

### 5.1 Map size, spacing and regions

The preview accepts a square `elevation` of N ∈ {512, 1024, 2048} (the values of `param.map_size`). It uses **one vertex per cell: `vertex_spacing` s = 16.0 m and `region_size` R = 256 vertices**, so a region covers 4096 m.

- **Why one vertex per cell.** The layer holds one height per 16 m cell. Finer spacing would only interpolate, multiplying memory by (16/s)² and adding nothing true (detail noise is a later stage). Q-V3D-01-5 offers finer spacing.
- **Why R = 256.** The map is centred on the origin (design 648). Centring keeps region edges on the grid only if N/2 cells is a whole number of regions, that is R divides N/2 = 256 at N = 512. So 256 is the largest region size that keeps every map size aligned. It is also Terrain3D's default.

Vertex (cell) (x, y), with x the column and y the row of the row-major layer (row 0 is the top of the 2D map, north):

```
X = 16 · (x − N/2)        Z = 16 · (y − N/2)        Y = height h(x, y), metres
```

Godot's +X is east, −Z is north and +Y is up, so the 2D map seen from above with north at the top of the screen matches the 3D view. The map spans X, Z ∈ [−8N, 8N − 16].

| N | side (m) | regions | region locations (each axis) | vertices | heights + control + colour |
| --- | --- | --- | --- | --- | --- |
| 512 | 8,192 | 2 x 2 = 4 | −1, 0 | 262,144 | 3 MB |
| 1024 | 16,384 | 4 x 4 = 16 | −2 to 1 | 1,048,576 | 12 MB |
| 2048 | 32,768 | 8 x 8 = 64 | −4 to 3 | 4,194,304 | 48 MB |

Region location (i, j) covers X ∈ [4096 i, 4096 i + 4096), and N/2 = 256 · N/512 cells puts the map's first vertex at the corner of region (−N/512, −N/512). All locations lie within Terrain3D's ±16 grid (V5). The regions cover the map exactly: no vertex in a region lies outside the map, so there is no fill rule. Outside the regions Terrain3D draws nothing (V8) and the sea plane (5.4) shows.

Refusals (status label): `This world has no elevation layer, so there is no terrain to show.`, `elevation is <type>, expected i16.`, `elevation is <W>x<H>; the 3D preview needs a square map of 512, 1024 or 2048 cells.`, and for any other layer used, `<layer> is <W>x<H>, expected <N>x<N>; <layer> not used.` (that layer is then treated as absent). As built (r2) an optional layer is also not used, with a note, when its type is wrong (`<layer> is <type>, expected <type>; <layer> not used.`) or it cannot be read (`<layer>: <code>: <message>; <layer> not used.`). A world file that cannot be opened, or whose `elevation` cannot be read, shows `<path>: <code>: <message>` (`WorldFile`'s error code and message).

### 5.2 Heights

For every cell, with e = `elevation`(x, y) and d = `water_depth`(x, y) (0 when the layer is absent):

```
lake  = e > 0 and d > 0                       (P1-02 invariant 7)
h     = float(e + d) / 10.0f   if lake        (int32 sum; the lake's surface)
        float(e) / 10.0f       otherwise      (land and sea bed, metres)
```

Metres come from decimetres. The value is exact in `int32` and rounded once to the nearest float. Land h ∈ [0.1, 2999.0], sea bed h ∈ [−2900.5, −0.1]. Lake cells sit at their level, which the flood fill makes no higher than the lake's rim. The heights are true scale; Q-V3D-01-2 offers vertical exaggeration.

Height image: `Image` `FORMAT_RF`, N x N, no mipmaps, pixel (x, y) = h(x, y), passed to Terrain3D with offset 0.0 and scale 1.0. Because those values are the identity, the order in which `import_images` applies them does not matter (V3).

### 5.3 Colours

**Choice: the colour map**, one RGBA8 pixel per vertex, over a single white texture asset (id 0) and a control map of zeros. The other way is a control map with one flat-colour texture asset per biome. That needs one asset per code, at most 32 (codes run to 254), and its blending is Terrain3D's own. The colour map needs no asset bookkeeping, takes any number of biomes, and shows exactly the RGB of table `biome`, which is what the 2D `biome` layer and `worldgen-cli render --layer biome` show.

```
colour(x, y) = rgb of the biome_records() row with id = biome(x, y), alpha A_NEUTRAL
               (255, 0, 255, A_NEUTRAL)  when there is no such row (as the 2D viewer, P1-00 10.2)
               (128, 128, 128, A_NEUTRAL) for every cell when the `biome` layer or table is absent
control(x, y) = 0.0f    (bit pattern 0: base texture 0, no overlay, blend 0, no autoshader, no hole, navigation off)
```

`A_NEUTRAL` = 128 in this reading: the colour map's alpha value that leaves roughness unchanged (V7; as built, the shader's wetness is 1 − 2a, so 128 gives −0.004, neutral to the eye). Colour image: `FORMAT_RGBA8`, N x N, no mipmaps. Control image: `FORMAT_RF`, N x N, all 0.0f.

Texture asset 0: a `Terrain3DTextureAsset` with a 64 x 64 `FORMAT_RGBA8` albedo of (255, 255, 255, 0) with mipmaps (alpha is height in Terrain3D's packing) and a 64 x 64 normal map of (128, 128, 255, 230) with mipmaps (flat; alpha roughness about 0.9). Both are built in code, and no file is written. V7 checks the packing and that the colour map multiplies the albedo; its fallback is Terrain3D's colour-map display mode.

Between vertices the shader filters the colour map linearly, so biome edges blend over one 16 m cell. Nearest sampling is not offered.

### 5.4 Water

**Choice: an opaque sea plane at Y = 0 and lakes drawn flat at their surface (5.2). Rivers are not drawn.**

- Sea. A `MeshInstance3D` with a `PlaneMesh` of side 48N m (three map sides), centred on the origin at Y = 0. Its `StandardMaterial3D` is unshaded off, roughness 0.2, and albedo the colour of the `biome_records()` row whose key is `ocean` (else 0x183878, the shipped value). As built (r2), the sky's `ground_bottom_color` and `ground_horizon_color` (`ProceduralSkyMaterial`) are also set to that colour, so the sky below the horizon reads as sea beyond the plane. The plane's edge still shows as a change of tone, and from high altitude the plane does not fill the view below the horizon; V3D-02 fixes both. The sea bed exists in the height map but is hidden. An opaque plane is the simplest surface that is true at the cell level (sea level is 0 m by definition of the layer), and it costs no transparency sorting. A plane wider than the map makes the world read as land in a sea, which it is, since the ocean is one connected body touching the map edge (P1-02 invariant 8). Q-V3D-01-3 offers a translucent sea.
- Lakes. A lake's surface is h = (e + d)/10, flat over each lake, and coloured by the `lake` biome (the `biome` layer has the lake code exactly on lake cells, P1-04). This costs one more layer read and no mesh. The lake bed is not shown.
- Rivers are narrower than a cell (P1-02, design 7). Carving them is a later stage, and nothing marks them here.

## 6. `terrain_data.hpp` (C++ only, no Godot types)

Namespace `unmapped::terrain`. It includes only standard headers (as built: `<cstddef>`, `<cstdint>`, `<functional>`, `<optional>`, `<span>`, `<string>`, `<vector>`) and worldgen's public headers (`worldgen/inspect/geography.hpp`).

| Name | Contract |
| --- | --- |
| `constexpr float CELL_M = 16.0f; constexpr int REGION_SIZE = 256; constexpr std::uint8_t A_NEUTRAL = 128;` | 5.1, 5.3 |
| `struct Layout { int n; float spacing; int region_size; int region_min; int region_count; float origin; };` | `region_min` = −n/512, `region_count` = n/256 per axis, `origin` = −8n (X and Z of vertex (0, 0)) |
| `std::optional<Layout> layout_for(int width, int height, std::string& problem)` | `nullopt` with 5.1's refusal text unless width = height ∈ {512, 1024, 2048} |
| `std::vector<float> build_heights(const Layout&, std::span<const std::int16_t> elevation, std::span<const std::uint16_t> water_depth)` | n² values, row-major, 5.2; `water_depth` empty means absent; throws `std::invalid_argument` on a size mismatch |
| `std::vector<std::uint8_t> build_colours(const Layout&, std::span<const std::uint8_t> biome, std::span<const worldgen::inspect::BiomeRecord>)` | 4n² bytes RGBA, 5.3; `biome` empty means grey |
| `std::uint32_t sea_colour(std::span<const worldgen::inspect::BiomeRecord>)` | 5.4 |
| `Vec2 cell_to_world(const Layout&, int x, int y)` | (X, Z) of 5.1 |
| `CellHit world_to_cell(const Layout&, float X, float Z)` | `{x, y, inside}`: x = floor(X/16 + n/2 + 0.5), the same for y; `inside` iff both are in [0, n) |
| `float ground_clamp(const Layout&, std::span<const float> h, float X, float Z)` | u = X/16 + n/2 and v likewise, clamped to [0, n − 1]; i = min(floor(u), n − 2), j likewise; the max of h at (i, j), (i+1, j), (i, j+1), (i+1, j+1). This is the maximum, not an interpolation, because Terrain3D's triangles can lie above a bilinear surface on steep cells |
| `float sample_bilinear(const Layout&, std::span<const float> h, float X, float Z)` | the same i, j, with fu = u − i, fv = v − j; standard bilinear; HUD only |
| `std::size_t unknown_biome_cells(std::span<const std::uint8_t> biome, std::span<const BiomeRecord>)` | r2: the number of cells whose code has no record (the magenta note of section 7) |
| `struct HudWorld { const Layout* layout; spans heights, elevation, water_depth, biome, region; std::function<std::string(std::int64_t)> biome_name, region_name; }` | r2: what the HUD reads; a span is empty when its layer is absent |
| `std::string hud_line(const HudWorld&, float X, float Y, float Z, float speed, int fps)` | r2: the first HUD line of 8.4 |
| `constexpr const char* HUD_KEYS` | r2: the key line of 8.4 |
| `std::string one_decimal(double)` | r2: one decimal, no negative zero (`12.5`, `-0.1`, `0.0`); the HUD and the bench lines |

`build_heights` and `build_colours` throw `std::invalid_argument` when a non-empty span is not n² long.

`fly_state.hpp` (namespace `unmapped::terrain`): `struct FlyState { float x, y, z, heading_deg, pitch_deg; int speed_index; }`, `struct FlyInput { int forward, right, up; bool boost; float yaw_deg, pitch_deg; }` (each axis −1, 0 or 1; yaw and pitch are deltas), and `void step(FlyState&, const FlyInput&, float dt, const std::function<float(float, float)>& ground, float bound)`. Its rules are in 8.2. As built (r2) also: the constants `SPEEDS`, `DEFAULT_SPEED_INDEX` 4, `BOOST` 4, `MAX_ALTITUDE` 30000, `GROUND_MARGIN` 2; `float speed_of(const FlyState&, bool boost)` (m/s); `void speed_step(FlyState&, int delta)` (one step down for delta < 0, up for delta > 0, saturating at 0 and 7); `Rotation camera_rotation(const FlyState&)` (radians, (P, −H, 0), 8.2).

## 7. `TerrainPreview` (scene logic)

On `_ready`, and on each world opened:

1. `WorldFile::open`. Read `elevation`, then `water_depth`, `biome` and `region` if present (5.1 refusals). Build `Layout`, the heights and the colours (section 6).
2. Make the three images (5.2, 5.3) by copying the buffers into `PackedByteArray`s, little-endian floats for RF.
3. If `ClassDB::class_exists("Terrain3D")` and not `--no-terrain3d`: instantiate `Terrain3D` and cast it to `Node3D` (V15). Set `vertex_spacing` = 16.0 (V1), `region_size` = 256 (V2), collision disabled (V9), background outside regions none (V8) and no data directory (V11). Give it a `Terrain3DAssets` holding texture asset 0 (5.3, V7). Add it as a child, call `set_camera(camera)` (V10), then call `get_data()` → `import_images([height, control, colour], Vector3(origin, 0, origin), 0.0, 1.0)` (V3, V4).
   As built (r2), in this order: `set("vertex_spacing", 16.0)`; `set("free_editor_textures", false)` (keeps the textures built in code); a `Terrain3DAssets` with `set_texture_asset(0, asset)` and `set("assets", ...)`; `add_child`; then, since they exist only once the node is in the tree, `set("collision_mode", 0)` (Disabled) and `material.set("world_background", 0)` (None); `set_camera`; `import_images`. `region_size` is not set: 256 is the default (V2).
   Otherwise the status reads `Terrain3D is not in this build (configure with -DUNMAPPED_BUILD_TERRAIN3D=ON): showing the sea plane only.`, and steps 4 and 5 still run.
4. Size the sea plane (5.4). Place the camera (8.3), with near 1.0 m and far 48N m.
5. Notes in the status label, one per line: `no biome layer: terrain drawn grey`, `no water_depth layer: lakes drawn at their beds`, `<k> cells of unknown biome drawn magenta`, then the "not used" notes of 5.1.

The previous world's Terrain3D node is freed before a new one is made (as built: `remove_child`, then `queue_free`). Nothing is saved: the preview never calls a Terrain3D save method.

**Scene** `game/viewer3d/terrain_preview.tscn` (layout only):

| Node | Settings |
| --- | --- |
| `TerrainPreview` (root) | none |
| `WorldEnvironment` | `Environment`: background sky, `ProceduralSkyMaterial` defaults, ambient source sky, tonemap linear, fog off |
| `DirectionalLight3D` `%Sun` | rotation_degrees (−40, −135, 0): light from the north-west, as map hill shading; energy 1.0; shadows off (Q-V3D-01-4) |
| `Camera3D` `%Camera` | current; fov 70 |
| `MeshInstance3D` `%Sea` | `PlaneMesh` and material set in code |
| `CanvasLayer` → `%Hud` (Label, top left), `%Status` (Label, bottom left), `%OpenButton`, `%OpenDialog` | |

## 8. Camera, controls and HUD (item 4)

### 8.1 Keys and mouse

| Input | Action |
| --- | --- |
| W / S | forward / back along the view direction, pitch included (free flight) |
| A / D | left / right, horizontal |
| E / Q | up / down along world Y |
| Shift (held) | speed x 4 |
| mouse wheel, `-` / `=` | speed step down / up |
| right mouse button (held) | mouse look (mouse captured while held): heading += 0.15° per pixel right, pitch −= 0.15° per pixel down |
| arrow keys | heading ∓ 90° per second (Left / Right), pitch ± 60° per second (Up / Down) |
| R | back to the start position (8.3) |
| H | HUD on or off |
| Escape | release the mouse |

### 8.2 `step` rules (normative; the unit tests use them)

- `SPEEDS` = 10, 20, 50, 100, 200, 500, 1000, 2000 m/s; default index 4 (200 m/s). A step down at index 0 and a step up at index 7 do nothing.
- heading = (heading + yaw) mod 360 in [0, 360); pitch = clamp(pitch + pitch delta, −89, 89).
- forward f = (sin H · cos P, sin P, −cos H · cos P); right r = (cos H, 0, sin H); up u = (0, 1, 0), with H the heading and P the pitch in radians.
- velocity = SPEEDS[i] · (boost ? 4 : 1) · (forward·f + right·r + up·u), not normalised (diagonal flight is faster, as is usual for a fly camera); position += velocity · dt.
- Then x and z are clamped to [−bound, bound] with bound = 16N, and y is clamped to [ground(x, z) + 2, 30000], where ground is `ground_clamp`. Under the map, ground() is the edge value, and over open sea y ≥ 2 still holds since ground ≥ the sea-bed height, so the camera is never forced lower.
- Camera transform: position (x, y, z); `rotation` (radians) = (P, −H, 0) in Godot's YXZ order, so heading 0 looks along −Z (north) and 90 along +X (east).

### 8.3 Start position

Cell (N/2, 7N/8), that is X = 0, Z = 6N. Altitude h_max + 300 m, where h_max is the largest value of the height buffer. Heading 0, pitch −25. The camera is above every peak and looks north over the map. `--at`, `--alt`, `--heading`, `--pitch` and `--speed` override these values one by one.

### 8.4 HUD

The HUD is one line of text plus a key line under it, and it is updated each frame.

```
cell (<x>, <y>)  |  <X> m E, <Z> m S  |  altitude <a> m, <g> above ground  |  ground <elev>  |  <biome name>  |  <region name>  |  <v> m/s  |  <fps> fps
```

- x, y come from `world_to_cell`, and outside the map the first field reads `outside the map` and the ground, biome and region fields are left out.
- X and Z are whole metres, signed.
- a is the camera Y, and g is a − `sample_bilinear`, both with one decimal.
- `<elev>` is the display form of the cell's `elevation` value from worldgen's layer catalogue, exactly as the 2D viewer's hover shows it, followed by `, lake surface <h> m` on a lake cell.
- The biome name comes from table `biome`, and the region name from `worldgen::inspect::region_name`. A field whose layer is absent is left out.

The key line reads `WASD move, E/Q up/down, right mouse look, wheel speed, Shift fast, R start, H hide`.

## 9. Testing (item 5)

### 9.1 Unit tests (`gdext-terrain-tests`, doctest, no Godot)

The executable is built with `UNMAPPED_BUILD_GDEXT=ON` and `UNMAPPED_BUILD_TESTS`, from `terrain_data.cpp` and `fly_state.cpp`, which are compiled into a small static library that `unmapped_gdext` also links. It links worldgen and doctest only.

| Id | Test |
| --- | --- |
| U1 | `layout_for` at 512, 1024 and 2048 gives the table of 5.1; 256, 4096, 512 x 1024 and 0 give `nullopt` with the exact text |
| U2 | `build_heights` on a hand-made 512 grid: ocean −3005 → −300.5f, land 1 → 0.1f, 29990 → 2999.0f, lake (e 120, d 35) → 15.5f, a land cell with d = 0 unchanged; empty `water_depth` leaves the lake at 12.0f; the size mismatch throws |
| U3 | `build_colours`: a known code gives its RGB and 128; an unknown code gives magenta; an empty layer gives grey |
| U4 | `cell_to_world` (0, 0) = (−8N, −8N) and (N−1, N−1) = (8N−16, 8N−16); `world_to_cell` round-trips every 97th cell, and ±7.9 m about a vertex stays on it; −8N − 8.1 is outside |
| U5 | `ground_clamp` and `sample_bilinear` at vertices, cell midpoints and outside the map |
| U6 | `step`: heading 0, forward 1 s at 200 m/s goes Z −200 ±0.01; heading 90 goes X +200; boost goes 800; pitch clamps at ±89; heading wraps; flying down into a 1000 m plateau stays at 1002; speed steps saturate; the bound clamps |

### 9.2 Terrain check (`gdext-terrain-check`, CTest, timeout 300 s, `RESOURCE_LOCK godot-run-copy`)

The check is registered whenever `gdext-headless-check` is, with Terrain3D on or off. `gdext/cmake/terrain_check.cmake` runs these steps:

1. `worldgen-cli generate --seed 49 --map-size 512 --until regions --data <repo>/game/data` into `<build>/gdext/terrain-check/w512/`, and `--seed 1 --map-size 1024 --until regions` into `w1024/`. For each, `render --layer biome` gives `biome.png`. Geography only keeps the check fast; history does not change `elevation` or `water_depth`. History can write blighted land into `biome`, and the preview shows whatever the layer holds.
2. `--seed 49 --map-size 512 --until elevation` into `elev/` (no `biome` layer), and `--seed 42 --test-pattern` into `pattern/` (no `elevation` layer).
3. `godot --headless --path <build>/game --import`.
4. For each world, `godot --headless --path <build>/game res://viewer3d/terrain_preview.tscn -- <world> --check <dir> --expect-terrain3d <present|absent>`. `present` is used when the build has the option on. The 512 world runs a second time with `--no-terrain3d --expect-terrain3d absent`. Each run must print `UNMAPPED-TERRAIN-CHECK: PASS` and exit 0.

Checks (`terrain_check.cpp`; the check has its own copy of the formulas of 5.1 to 5.3, as P4-06's check does):

| Id | Check |
| --- | --- |
| T1 | layout, image sizes and formats (RF, RF, RGBA8; N x N) |
| T2 | every height equals the check's own formula exactly; min and max are reported and equal its own |
| T3 | every colour pixel's RGB equals `biome.png` byte for byte (worldgen's renderer, independent of this code); every alpha is 128; no magenta; on `elev/` every pixel is grey and the status has its note |
| T4 | the start position and the HUD text at the start, at cell (0, 0), at a lake cell (the first in row-major order, if any) and outside the map, against the format of 8.4 |
| T5 | `pattern/`: the status is exactly the no-elevation text; a missing file gives `<path>: file_unreadable: ...` |
| T6 | `present`: one `Terrain3D` child; its region locations are exactly the (N/256)² locations of 5.1; `get_height` at the 4 corners, the centre, the highest and lowest cell and 25 cells of a fixed pattern ((37k mod N, 61k mod N) for k = 1 to 25) equals the buffer within 0.001 m; the colour read back at the same points equals the buffer within 1/255 (V12). This is what proves orientation, centring and the offset and scale semantics |
| T7 | `absent` (missing class or `--no-terrain3d`): no Terrain3D node, the status text of section 7, and the sea plane and camera present, while T1 to T4 still pass |
| T8 | 300 frames of scripted `FlyInput` (forward with yaw, then a dive into the highest cell) leave the camera above `ground_clamp` + 2 every frame |
| T6 (reopen, r2) | `present`: `open_world` on the same path again leaves exactly one `Terrain3D` child, and `get_height` at the centre cell equals the buffer within 0.001 m (V16) |

If V12 finds that Terrain3D cannot run headless, T6 becomes an information line (`UNMAPPED-TERRAIN-CHECK: T6 skipped: <reason>`), and the GPU checklist's item G3 becomes required before merge. As built, V12 holds: T6 runs in CI. The check takes about 13 s.

### 9.3 By hand on a GPU (Paul or the engineer, Linux; Windows the same in Git Bash)

```
cmake --preset linux-gcc-release -DUNMAPPED_BUILD_GDEXT=ON -DUNMAPPED_BUILD_TERRAIN3D=ON
cmake --build --preset linux-gcc-release
B=~/.cache/the-unmapped/build/$(basename "$PWD")/linux-gcc-release
GODOT=~/.cache/the-unmapped/tools/godot-4.7.2-stable/Godot_v4.7.2-stable_linux.x86_64
OUT=~/.cache/the-unmapped/out
for n in 512 1024 2048; do "$B/tools/worldgen-cli/worldgen-cli" generate --seed 42 --map-size $n --out $OUT/seed42-$n.db; done
"$B/tools/worldgen-cli/worldgen-cli" render --db $OUT/seed42-1024.db --composite biomes --out $OUT/seed42-1024.biomes.png
"$GODOT" --path "$B/game" res://viewer3d/terrain_preview.tscn -- $OUT/seed42-1024.db
"$GODOT" --path "$B/game" res://viewer3d/terrain_preview.tscn -- $OUT/seed42-1024.db \
    --at 512 512 --alt 22000 --pitch -89 --screenshot $OUT/seed42-1024.top.png
"$GODOT" --path "$B/game" res://viewer3d/terrain_preview.tscn -- $OUT/seed42-2048.db --bench
```

| Id | Look for |
| --- | --- |
| G1 | no checkerboard; land in biome colours; sea a flat plane to the horizon |
| G2 | the top-down screenshot matches `seed42-1024.biomes.png` in layout: north at the top, coasts, lakes and mountain ranges in the same places, not mirrored |
| G3 | the HUD's `ground` at three cells equals the 2D viewer's hover elevation at the same cells (`--hover x y` in the 2D viewer) |
| G4 | no cracks or steps at region edges (X or Z a multiple of 4096 m); no flicker where low coast meets the sea plane |
| G5 | lakes flat and lake-blue, level with their shores; no hole or spike anywhere |
| G6 | every control of 8.1; the camera never enters the ground when diving at 2000 m/s with Shift |
| G7 | at 2048, a fly from the south edge to the north edge shows the whole map with no missing region |

## 10. Performance (item 6: measurements only, no bounds)

`--bench` prints these lines and quits. Headless it prints the first two lines only.

```
UNMAPPED-TERRAIN-BENCH: map <N>, open <ms> ms, layers <ms> ms, buffers <ms> ms, images <ms> ms, import <ms> ms, first frame <ms> ms
UNMAPPED-TERRAIN-BENCH: memory static <MB> MB, video <MB> MB, regions <k>
UNMAPPED-TERRAIN-BENCH: flight <s> s at 1000 m/s: frames <n>, mean <ms>, max <ms>, over 11.1 ms <a>, over 16.7 ms <b>, over 33.3 ms <c>
```

- `import` is the time of `import_images`, and `first frame` runs from the end of the import to the next `frame_post_draw`. As built, headless (where nothing is drawn) it runs to the next `process_frame` instead.
- Memory comes from `Performance` `MEMORY_STATIC` and `RENDER_VIDEO_MEM_USED`, read after the first frame. As built, video memory is `RenderingServer::get_rendering_info`: the larger of `RENDERING_INFO_VIDEO_MEM_USED` and the sum of `RENDERING_INFO_TEXTURE_MEM_USED` and `RENDERING_INFO_BUFFER_MEM_USED`. With the NVIDIA Vulkan driver all three read 0, so the line shows `video 0.0 MB` there (V17 unmeasured).
- The flight runs from the start position heading north at pitch −10 and 1000 m/s, for 16N / 1000 s, with vsync off.

The engineer records the figures for 512, 1024 and 2048 (release preset, the machine named) in `gdext/README.md`. They are inputs to the later fly-across test, not a gate. Recorded 2026-11-27 (RTX 4090 Laptop GPU, Vulkan): import 4.6 / 22 / 86 ms, static memory 65.5 / 84.6 / 160.7 MB, flight mean 0.3 to 0.4 ms per frame at 512 / 1024 / 2048.

## 11. CI (item 7)

- `gdext-linux` and `gdext-windows` configure with `-DUNMAPPED_BUILD_TERRAIN3D=ON` as well. Each must still list `gdext-headless-check` after configure and must now also list `gdext-terrain-check` and `gdext-terrain-tests`. They build, then run `ctest` (both checks and the unit tests included), and their `world-hashes.sh` steps and `compare-gdext-hashes` are unchanged and must still pass, since worldgen is untouched. The clean-checkout check at the end also proves that nothing was written into `extern/terrain3d/`.
- The ccache key of `gdext-linux` gains the hash of `extern/terrain3d/`.
- The seven `build` jobs, `determinism-lint` and `compare-hashes` are unchanged: they never turn the extension on.
- On failure, `gdext-*` also upload `<build>/gdext/terrain-check/`.
- As built (r2): the workflow changes are made; neither job has yet passed with them, and MSVC has not yet compiled Terrain3D or the compatibility header (open after r2, section 13).
- r3: CI run 46087708271 on `v3d/01-terrain` (2026-11-27) passed `gdext-linux` and `gdext-windows`: MSVC compiled Terrain3D through the compatibility header, and the terrain check ran on Windows.

## 12. Invariants

1. The preview opens `world.db` read-only through `WorldFile` and writes no file except `--screenshot`'s, which goes where the user names it. Terrain3D saves nothing.
2. With `UNMAPPED_BUILD_TERRAIN3D` off, the run copy has no `addons/`, `extension_list.cfg` is today's one line, and `gdext-headless-check` gives the same result as before the stage.
3. For every accepted world, the height buffer satisfies h(x, y) = 5.2's formula at every cell, and the colour buffer equals worldgen's `render --layer biome` RGB at every cell.
4. Cell (x, y) is at (16(x − N/2), 16(y − N/2)), and the regions cover the map exactly with (N/256)² regions.
5. The camera is never below `ground_clamp` + 2 m, never above 30,000 m, and never more than 16N m from the origin on X or Z.
6. Nothing in `worldgen/`, `tools/`, `game/data/`, the schema or a golden file changes, and `GENERATOR_VERSION` stays as on `main`.

## 13. To verify against Terrain3D's source (engineer, before building on it)

Each item gives the reading this spec assumes and what to do if it is false. The engineer reports the findings in the first hand-back, and the spec's next revision records them. r2 records them in the column "Found".

| Id | Assumed | If false | Found (r2) |
| --- | --- | --- | --- |
| V1 | `vertex_spacing` accepts 16.0 | use the largest power of two s ≤ the maximum (8, then 4), k = 16/s vertices per cell, bilinear heights and nearest colours, R stays 256 (centring still holds for s ≥ 4: N/(32 s) is whole); 2048 at s = 4 is 32 x 32 regions, the grid's limit, and about 800 MB; report to Paul | confirmed |
| V2 | `region_size` is settable to 256 in code before data is added (property or `change_region_size`) | 256 is the default, so leave it | not set; left at the default 256 |
| V3 | `import_images(images, global_position, offset, scale)` takes `global_position` in world metres (not vertices), splits an N x N image over aligned regions, accepts RF, RF, RGBA8, and is the identity with offset 0 and scale 1 | divide the position by s if it is in vertices; else build each `Terrain3DRegion` from 256 x 256 crops (`set_map`, height, control, colour) and `add_region`, then `update_maps` once | confirmed (T6); with offset 0 and scale 1 the height image is not even copied (`Terrain3DData::import_images`) |
| V4 | pixel p of region location l is at world (256 l + p) · s with no half-pixel shift, and the vertex shows the pixel's value | adjust `origin` by the shift found; T6 proves the result | confirmed (T6) |
| V5 | region locations run −16 to 15 on each axis | 5.1 needs only −4 to 3 | confirmed |
| V6 | a control value of 0.0f means texture 0, no autoshader, no hole | set the bits the source requires; still one constant for every cell | confirmed: base bits 27–31, overlay 22–26, blend 14–21, UV rotation 10–13, UV scale 7–9, hole 2, navigation 1, autoshader 0 (`terrain_3d_util.h`) |
| V7 | a texture asset needs an albedo (alpha = height) and a normal (alpha = roughness) of one size and format with mipmaps; the shader multiplies albedo by the colour map's RGB; the colour map is read as sRGB; alpha 128 is neutral roughness; the setter is `set_texture_asset` or its 1.1 name | match the source's packing; if the colour map is linear, convert sRGB to linear in `build_colours` (T3 then compares after the same conversion); if no plain multiply exists, use the material's colour-map display mode with lighting | confirmed: the setter is `Terrain3DAssets::set_texture_asset(id, asset)`; the colour map is sampled `source_color` (sRGB) and multiplies albedo (`pbr_views.glsl`: `ALBEDO = mat.albedo_height.rgb * color_map.rgb`); alpha a gives wetness 1 − 2a, so 128 is near neutral |
| V8 | the material can show nothing outside regions (world background none) | keep its flat background if it is at height 0, under the sea plane | confirmed (`world_background` 0, set on the material after `add_child`) |
| V9 | collision can be disabled by a property | leave the default if it costs nothing measurable; record it in the bench | confirmed, but `collision_mode` is settable only after `add_child` |
| V10 | a camera made in a scene and passed to `set_camera` drives LOD | find the node by the camera's path instead | confirmed |
| V11 | without a data directory nothing is written to disk and no error is raised | point it at a temporary folder under the build root, never the repository, and never save | confirmed; `free_editor_textures` is set false so the textures built in code are kept |
| V12 | the class instantiates under `--headless`, and `get_height(Vector3)` and a colour reader work there | section 9.2's fallback | confirmed: Terrain3D works headless, `get_height` and `get_color` answer, T6 runs in CI |
| V13 | `src/*.cpp` compile with our CMake against godot-cpp 10.0.0 as C++17, with only the defines and flags `SConstruct` sets; shaders are C++ string includes, not a generated step; documentation data is optional | reproduce any generated step as a CMake custom command in the build tree; on API errors stop and ask Paul (Q-V3D-01-1) | **false**: 16 errors from godot-cpp API changes; Paul's decision of 3.1 (compatibility header and one edited line). Shaders are string includes and documentation is optional, as assumed |
| V14 | `terrain.gdextension`'s entry symbol and `[icons]`/`[dependencies]` sections, and every `res://addons/terrain_3d/` path the C++ loads at run time | vendor and link exactly those files (3.2, 3.4) | entry symbol `terrain_3d_init`; the only run-time `res://` path is the ocean material `extras/shaders/M_ocean.tres`, loaded only when Terrain3D's own ocean is on, which the preview never does, so nothing is vendored for it |
| V15 | `Terrain3D` derives from `Node3D`; its data object is reached by `get_data()` or property `data` | adapt the calls; a non-Node3D class would end option (a)'s plan and needs Paul | confirmed (`get_data()`) |
| V16 | adding the node in code after `_ready` works, including a second world replacing the first | build the node once and clear its data between worlds | confirmed (T6 reopen) |
| V17 | GPU memory grows with the regions in use, not with all 1,024 slots | record it in the bench | **not measured**: the rendering server reports 0 MB with the NVIDIA Vulkan driver (section 10) |
| V18 | license file name and tag commit SHA of `v1.1.0-stable` | record what is found in `extern/README.md` | `LICENSE.txt` (MIT); commit `629bbd65526bbdc3d1aa371840c7028055c30b02` |

Godot itself: naming a scene after `--path` runs that scene instead of the main scene. If that is wrong, the 2D `ViewerScreen` takes a first user argument `--terrain` and calls `change_scene_to_file("res://viewer3d/terrain_preview.tscn")`. That one line is the only 2D change allowed. Found: it works; the 2D viewer is unchanged.

**Open after r2.** (1) The Windows (MSVC) build of the header and of Terrain3D, and the CI jobs `gdext-linux` and `gdext-windows` with the option on, have not passed yet. Closed (r3): CI run 46087708271 on `v3d/01-terrain`, 2026-11-27, passed both jobs; MSVC compiled Terrain3D through the compatibility header and the terrain check ran there. (2) The sea plane does not fill the view from high altitude and its edge shows as a change of tone (5.4); V3D-02 fixes it.

## 14. Questions for Paul (decided 2026-11-27: Paul accepts all six recommendations)

| Id | Question | Recommendation, accepted |
| --- | --- | --- |
| Q-V3D-01-1 | How Terrain3D is vendored: (a) source built by our CMake, (c) the release's prebuilt libraries in `extern/`, or (c′) the release zip fetched once into the tools folder like the Godot editor, with pinned digests (not in the repository, but not "vendored" either) | (a), section 3.1; if it does not compile against godot-cpp 10.0.0, the engineer stops and you choose between a second godot-cpp for Terrain3D only and (c) |
| Q-V3D-01-2 | True height scale or a vertical exaggeration option (for example `--height-scale 2`) | true scale only; an option can be added at no cost to the data path later |
| Q-V3D-01-3 | Sea as an opaque plane or translucent over the sea bed | opaque (5.4) |
| Q-V3D-01-4 | Sun shadows on or off | off: at 8 to 32 km, shadow maps cost much and show little at this cell size; relief reads from the shading of the slopes |
| Q-V3D-01-5 | One vertex per 16 m cell, or finer spacing with interpolated heights (closer to the game's 1 m terrain, 16 to 256 times the memory) | 16 m: no invented detail before the detail-noise stage |
| Q-V3D-01-6 | A separate scene, or later an "open in 3D at this cell" key in the 2D viewer | a separate scene now (section 4); the key could be a later stage |

## Revision notes

- r1 (2026-11-27): first draft. Approved by Paul the same day; Q-V3D-01-1 to 6 decided as recommended (status line and section 14 heading only).
- r2 (2026-11-27): as built. 3.1 to 3.3: Terrain3D does not compile unchanged; the force-included compatibility header, the one edited vendored line (`terrain_3d.cpp:647`) and its patch file, the three-version pin and the 8-line limit (Paul's decision). 5.1: "not used" notes for a wrong type or an unreadable layer; `<path>: <code>: <message>` for a failed open. 5.3: the wetness reading of the colour map's alpha. 5.4: the sky's ground colour set to the sea's. 6: the helpers `unknown_biome_cells`, `HudWorld`, `hud_line`, `HUD_KEYS`, `one_decimal`, `speed_of`, `speed_step`, `camera_rotation` and the constants. 7: the order of the Terrain3D calls, `free_editor_textures` false, `region_size` not set. 9.2: the T6 reopen check. 10: headless first frame, video memory from `RenderingServer::get_rendering_info`, the recorded figures. 13: each V item confirmed, false or not measured. Open after it: Windows and CI not yet passed; the sea plane (fixed by V3D-02).
- r3 (2026-11-27): 11 and 13's open item (1) closed: CI run 46087708271 on `v3d/01-terrain` passed `gdext-linux` and `gdext-windows`, so MSVC compiles Terrain3D through the compatibility header and the terrain check runs on Windows. No other change.
