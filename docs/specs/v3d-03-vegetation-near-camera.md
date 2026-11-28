# V3D-03 Vegetation near the camera

Status: **r2, as built** (2026-11-28). r1 was approved by Paul on 2026-11-27 with the recommendations of section 16 accepted (Q-V3D-03-1 to 5), Q-V3D-03-2 then changed by him (far budget 2,000,000); r2 records the build, the lead's ruling on the forest start (12.1) and the measurements (revision notes). Third stage of the 3D terrain viewer track (`docs/handoff-3d-terrain-viewer.md`), on top of V3D-01 r3 and V3D-02 r2 as built (`docs/specs/v3d-02-biome-ground-and-vegetation.md`; section numbers "V3D-02 n" refer to it). Paul chose this stage on 2026-11-27: vegetation placed around the camera, so that every map size shows full density near the viewer, as the first piece of the streaming work of design section 7 ("A chunk manager keeps a ring of chunks around the player loaded and frees the rest"; "Vegetation ... scattered per chunk ... drawn with instancing"; "Threading and budgets": generation on workers, main-thread attach work capped per frame). Art stays generated placeholders.

**No world change.** The stage reads `world.db` and writes nothing to it or to the source tree. `GENERATOR_VERSION`, the schema, every golden file, `world-hashes.sh` and its cases are unchanged; nothing under `worldgen/`, `tools/worldgen-cli/`, `game/data/`, `extern/` or the compatibility header changes. No new dependency. Floating point is allowed (nothing reaches a world file); every instance position stays exact and identical on every machine (section 13). Owner: viewer engineer.

**Not in this stage** (later stages, not specified here): streaming of terrain regions; detail noise; carved rivers; real art; better placeholder shrubs (they read as boulders); the mountain look; the sea's horizon band; markers; collision; VR; the fly-across test of design section 7.

## 1. Purpose

V3D-02 places every instance of the map at load, capped at 2,000,000: 1024 keeps all 1,088,122 (624 MB static memory drawn, transfer 916 ms); 2048 keeps 36% of 5,523,211 (about 54 trees per hectare in forest instead of 150; 1,175 MB). This stage splits vegetation into two tiers. The **far tier** is V3D-02's whole-map placement restricted to the kinds seen beyond 1 km (trees and snags) and thinned by V3D-02's keyed `thin` draw to a fixed budget; it is placed once at load in Terrain3D's instancer and never touched again. The **near tier** holds, in 256 m cells within a radius of the camera, exactly the instances the far tier left out, so that near the camera every map size shows V3D-02's full density. Near cells are computed on worker threads and attached on the main thread under a per-frame budget, filled nearest and in-view first, dropped with hysteresis.

## 2. Parts

| Area | Files | Contents |
| --- | --- | --- |
| vegetation (plain C++) | `gdext/src/terrain/vegetation.hpp`, `.cpp` | `place_candidates`, the far tier, `near_instances`, `instance_buffer` (sections 4, 5) |
| near cells (plain C++, no Godot types) | new `gdext/src/terrain/near_cells.hpp`, `.cpp` | grid, boxes, distances, the cell state machine and budgets (section 6); the forest path's `forest_column`, `forest_start_cell`, `forest_start_altitude` (12.1) |
| scene | `terrain_preview.hpp`, `.cpp`, new `terrain_preview_near.cpp` | worker tasks, near nodes, options, keys, HUD, bench (sections 7, 9, 12) |
| check | new `terrain_check_near.cpp`, `terrain_check.cmake` | T17 to T24 (11.2) |
| unit tests | new `gdext/tests/near_cells_test.cpp`; `vegetation_test.cpp` | U17 to U30 (11.1) |
| docs | `gdext/README.md`, "Terrain preview" | options, keys, HUD, checks, bench, checklist |

The new `.cpp` files of plain C++ join `unmapped_terrain_core` (V3D-01 9.1). Inputs are V3D-02's (section 2 there); nothing new is read from the world. As built, the stage landed in one commit: the CMake files name every source and scene-side file, so a part alone does not configure.

## 3. Terrain3D's instancer in this version (findings, item 5)

From `extern/terrain3d/src/terrain_3d_instancer.h/.cpp` and `terrain_3d_mesh_asset.cpp`:

| Topic | What the source does |
| --- | --- |
| Storage | Per region (`region_size` 256 vertices × 16 m = 4,096 m here): `Terrain3DRegion::get_instances()` = Dictionary mesh id → Dictionary cell `Vector2i` → [`TypedArray<Transform3D>` region-local, `PackedColorArray`, modified]. Cell = pixel / `CELL_SIZE` (32 vertices = 512 m; `_get_cell`), 8 × 8 cells per region. |
| Render objects | `_mmi_rids[region][(mesh id, lod)][cell]` = one RS instance and multimesh per region, mesh, LOD and cell, placed at the region origin. |
| `add_transforms` | Splits by `get_region_location`, then `append_location` (localise) and `append_region`, which for **each** instance reads the cell's triple, pushes one transform and colour and writes both arrays back (the copy-on-write copy V3D-02 found: quadratic in a cell's count). With `update` it queues `update_mmis(mesh, region)`. |
| Removal | `clear_by_mesh`, `clear_by_location`, `clear_by_region`: a whole mesh id in a whole region (`mesh_inst_dict.erase`, `_destroy_mmi_by_location`). `remove_instances` is a brush (size ≤ 4,096 m) that removes each instance inside the radius only if `UtilityFunctions::randf() < clamp(0.175·strength, 0.005, 10)` and `_get_usable_height` succeeds: random unless strength ≥ 5.72, and slope-dependent. No call removes one cell. |
| Rebuild | `update_mmis` queues (region, mesh); `_process_updates` on `frame_pre_draw` runs `_update_mmi_by_region`, which visits **every** cell of that mesh in the region, recreates the multimesh of each modified cell with `_create_multimesh` (one `multimesh_instance_set_transform` and one `_set_color` RS call per instance and LOD, main thread), and re-applies material, shadow, layer mask, LOD range and transform to the MMIs of every other cell. |
| LOD and fade | `_set_mmi_lod_ranges`: Godot visibility range from the asset's `get_lod_range_begin/end`; `fade_margin` > 0 widens both ends by the margin with `VISIBILITY_RANGE_FADE_SELF`, else fade disabled. |

**Conclusion.** A per-cell remove-and-add through Terrain3D is possible only by editing the dictionaries of `get_instances()` directly (set a cell's triple to empty arrays with modified true, then `update_mmis(mesh, region)`). Every such change costs, on the main thread in `frame_pre_draw` and in one piece, two RS calls per instance and LOD of the changed cell plus property calls on up to 64 cells; V3D-02's first frame (319 ms for 1,088,122 instances) gives about 0.3 µs per instance, about 0.4 ms for a dense 256 m cell and 2 ms for a 512 m one, which cannot be spread over frames. **Choice: the near tier uses the preview's own multimeshes** (section 7): one `multimesh_set_buffer` per multimesh, its buffer built on the worker, so main-thread work per cell is a few node calls and a copy. The far tier stays on Terrain3D's instancer exactly as V3D-02 9.1 step 5 places it (Q-V3D-03-1).

## 4. Tiers (items 2 and 3)

### 4.1 Far tier choice

Option (a), restricted to the far kinds. (b) cards or lower-detail meshes add nothing now: LOD 1 of the trees is already 8 and 14 triangles (V3D-02 6.2). (c) bare ground beyond the radius loses forests from every view above about 2 km (V3D-02 G8). Shrubs (visible to 1,000 m) and reeds (800 m) are never drawn beyond the near tier's reach (6.4), so they are near-only.

| Kind (mesh id) | far kind | Far tier: Terrain3D mesh id, ranges | Near tier: own multimesh ranges (m) |
| --- | --- | --- | --- |
| 0 broadleaf, 1 conifer | yes | same id, V3D-02 6.1 (LOD 0 to 600, LOD 1 to 6,000, no fade) | LOD 0 [0, 600]; LOD 1 [600, 1,200], end margin 100, fade self |
| 2 shrub | no | none (mesh asset stays, no instances) | [0, 1,000], no fade |
| 3 reed clump | no | none | [0, 800], no fade |
| 4 snag | yes | same id, [0, 3,000] | [0, 1,200], end margin 100, fade self |

`FAR_KIND = {true, true, false, false, true}`, `NEAR_FULL_END` = 1,200 m (the distance to which far kinds show full density), `NEAR_FADE` = 100 m, `NEAR_VISIBLE_MAX` = 1,400 m (= 1,200 + 2 × 100: a near multimesh is never drawn when its box centre is farther, whichever end of the margin Godot fades over, W13).

### 4.2 Partition (normative)

`C(x, y)` is V3D-02 6.4's candidate list of terrain cell (x, y) before the cap: kinds 0 to 4 in order, k ascending, with the edge-drop rule, each with its `thin`. `F` is the multiset of the far-kind candidates of the whole map in placement order, `S_far = |F|`, `B` the far budget (`--instance-cap`, default `DEFAULT_FAR_BUDGET` 2,000,000; r1 1,000,000, changed by Paul on 2026-11-27, Q-V3D-03-2).

```
T = S_far <= B ? 2^32 : sorted_thin(F)[B]           (uint64; 0-based index B of the thin values sorted ascending)
far(i)  = FAR_KIND[kind(i)] and thin(i) < T
Far(x, y)   = { i in C(x, y) : far(i) }            Extra(x, y) = C(x, y) \ Far(x, y)
```

Then for every terrain cell `Far ∩ Extra = ∅` and `Far ∪ Extra = C`, both in `C`'s order. The far tier is the union of `Far` over the map: `|far tier| = #{i in F : thin < T}` ≤ B, equal to B unless the value `sorted_thin(F)[B]` also occurs below index B (a 32-bit tie; expected about 10^-3 per world at 2048). B = 0 gives T = the smallest thin and an empty far tier. A near cell (section 6) holds `Extra` of its 256 terrain cells. T depends only on the world and B, so a cell's near instances are a pure function of (world, B, cell), whatever path the camera took. With `--veg-whole-map` the placement is V3D-02's, unchanged (cap with its `(thin << 32) | p` key).

### 4.3 Counts (measured, seed 42, r2)

A forest near cell (256 terrain cells, t = 255) holds 983 trees and 721 shrubs in full.

| N | S (V3D-02) | S_far | far tier at B = 2,000,000 | far trees per ha in forest | Extra per forest near cell |
| --- | --- | --- | --- | --- | --- |
| 512 | 187,951 | 104,482 | all (T = 2^32) | 150 | 721 (shrubs) |
| 1024 | 1,088,122 | 604,322 | all (T = 2^32): V3D-02's trees exactly | 150 | 721 |
| 2048 | 5,523,211 | 3,057,704 | 2,000,000 (65%) | about 98 | about 1,065 (about 344 trees + 721 shrubs) |

With `--instance-cap 1000000` the 2048 far tier keeps 33% (about 50 trees per hectare), and a soft lighter band lies beyond the near ring (row means up to 11/255 lighter); at the default 2,000,000 the step between tiers cannot be seen (row means just inside and outside the ring differ by at most 2/255; seed 42 at 2048, `v3d03-high-2048.png` and `-high-cap1m-2048.png`, virtual display).

## 5. Plain C++ additions to `vegetation` (contracts)

| Name | Contract |
| --- | --- |
| `struct Candidate { Instance instance; std::uint32_t thin; std::uint8_t kind; }` | public now (was file-local) |
| `struct VegetationInputs { const Layout* layout; std::span<const float> heights; std::span<const std::int16_t> elevation; std::span<const std::uint8_t> timber, water; const GroundCells* cells; std::uint64_t seed; }` | sizes as `place_vegetation`; throws `std::invalid_argument` on a mismatch |
| `std::vector<Candidate> place_candidates(const VegetationInputs&, int x0, int y0, int x1, int y1)` | 6.4's candidates of terrain cells x0 ≤ x < x1, y0 ≤ y < y1 (clamped to the map), in placement order (y, x, kind, k); equal to the whole map's list filtered by cell |
| `inline constexpr std::array<bool, 5> FAR_KIND`; `DEFAULT_FAR_BUDGET = 2000000` | 4.1, 4.2 |
| `struct FarTier { std::uint64_t threshold; std::uint64_t far_candidates; std::uint64_t budget; Placement placement; }`; `FarTier place_far_tier(const VegetationInputs&, std::uint64_t budget)` | 4.2: `threshold` T, `far_candidates` S_far, `placement.kinds` the far instances in placement order (ids 2, 3 empty), `placement.candidates` = S. Any method that gives this T (a histogram of `thin >> 16` then a pass over one bin is enough; the whole candidate list need not be held) |
| `std::array<std::vector<Instance>, 5> near_instances(const VegetationInputs&, int i, int j, std::uint64_t threshold)` | `Extra` of near cell (i, j) (terrain cells 16i to 16i + 15, 16j to 16j + 15), per kind, in placement order |
| `std::vector<float> instance_buffer(std::span<const Instance>, std::uint8_t tint_mode)` | 16 floats per instance (7.2) |
| `place_vegetation` | unchanged output (V3D-02 U13, U14 keep passing); may be rebuilt on `place_candidates` |

## 6. Near cells (item 7; `near_cells.hpp`, plain C++)

### 6.1 Grid, box, distance

`NEAR_CELL_CELLS` = 16 terrain cells (s = 256 m; 2 × 2 design chunks of 128 m, Q-V3D-03-4). M = N/16 cells per axis (32, 64, 128); cell q = j·M + i, i the column. Each near cell lies inside one Terrain3D instancer cell (512 m) and one region (16 divides 32 and 256).

```
X0(i) = 16*(16i - N/2) - 8;  X1(i) = X0(i) + 256          (metres; Z0, Z1 the same with j; every instance of the cell lies inside)
bot(q) = min of heights over vertices x in [16i-1, 16i+16], y in [16j-1, 16j+16], clamped to [0, N-1], minus 2 m
top(q) = max of the same, plus 22 m                      (tallest instance: conifer 15.5 m x 1.40 = 21.7 m)
d(q, P) = sqrt(dx^2 + dy^2 + dz^2)   (double)  dx = max(0, X0 - Px, Px - X1), dz likewise, dy = max(0, bot - Py, Py - top)
dh(q, P) = sqrt(dx^2 + dz^2)
in_view(q, P, H) = dh == 0 or dot(f, u) >= 0.5           f = (sin H, -cos H) (heading 0 north = -Z, 90 east = +X), u = unit (cx - Px, cz - Pz), c the box centre
```

d is the distance from the camera to the nearest point of the cell's box, so it is at most the distance to the box centre, which is what Godot's visibility range measures (the multimesh's custom AABB is the box, 7.2; W13).

### 6.2 States and rules

| State | Meaning |
| --- | --- |
| empty | nothing held |
| queued | wanted, waiting for a worker |
| computing | a worker task runs; flag `abandoned` |
| ready | result held by the scene, not attached |
| filled | nodes attached |
| dropping | filled, to be removed |

`update(viewer)` visits the cells in the square of half-side R + H + 256 around the camera and every cell not empty; with d = d(q, P):

| State | d ≤ R | R < d ≤ R + H | d > R + H |
| --- | --- | --- | --- |
| empty | → queued | stays | stays |
| queued | stays | stays | → empty |
| computing | clears `abandoned` | stays | sets `abandoned` |
| ready | stays | stays | → empty, listed in `take_released` |
| filled | stays | stays | → dropping |
| dropping | → filled | → filled | stays |

`computed(q, n)`: computing → ready with n instances, or → empty and released when `abandoned`. Priority of a cell: the key (in_view ? 0 : 1, d, q) ascending, from the last `update`.

| Call | Returns, and budget (`NearParams`) |
| --- | --- |
| `start_computing()` | queued cells in priority order, up to `max_in_flight` (4) minus the cells computing; marks them computing |
| `take_attach()` | ready cells in priority order: always the first, then more while cells < `max_attach_cells` (4) and instances ≤ `max_attach_instances` (16,384); marks them filled |
| `take_drops()` | dropping cells by d descending, then q, up to `max_drop_cells` (16); marks them empty |
| `take_released()` | cells whose ready result the scene must free |
| `counts()` | filled (filled + dropping), queued, computing, ready, instances in filled cells, `late` (cells seen by `update` with d ≤ `NEAR_VISIBLE_MAX` while queued, computing or ready, counted once per wanted episode) |
| `settled()` | no queued, computing, ready or dropping cell, and every cell with d ≤ R filled |

`NearParams { double radius = 2048, hysteresis = 512; int max_in_flight = 4, max_attach_cells = 4, max_drop_cells = 16; std::uint64_t max_attach_instances = 16384; }`. `build_near_grid(const Layout&, heights)` gives M and the boxes. The state after any sequence of `update`, `computed` and `take_*` calls is a function of that sequence alone (no clock, no threads). A teleport (key R, `--at`, a reopen with G keeping the camera) needs no special path: the next `update` queues the new cells nearest first and marks every old one dropping.

### 6.3 Arithmetic (default R = 2,048 m, H = 512 m, s = 256 m)

| Quantity | Formula | Value |
| --- | --- | --- |
| cells wanted at ground level | (πR² + 4Rs + s²) / s² | 234 |
| cells held at most | the same with R + H | 355 |
| near instances held, all-forest worst | 355 × Extra per cell | 256,000 at 1024; 378,000 at 2048 |
| lead before a cell can be drawn | R − `NEAR_VISIBLE_MAX` | 648 m |
| cells entering per second at speed v | (2R + s)/s × v/s = 17 v / 256 | 13 at 200 m/s; 53 at 800 (Shift); 133 at 2,000; 531 at 8,000 (Shift at 2,000) |
| lead in time | 648 m / v | 3.2 s; 0.81 s; 0.32 s; 0.08 s |
| attach capacity | 4 cells per frame | 240 per s at 60 fps, 360 at 90 |
| worker capacity | 4 tasks; measured 9 to 86 µs per cell on average, at most 448 µs | over 10,000 cells per s |

Measured (12.2): no late cell at 200 or 800 m/s at any size, at most 7 cells pending, attach at most 630 µs in a frame. So filling keeps ahead up to the 2,000 m/s step (133 per s against 240, 0.32 s lead = 19 frames). Faster (Shift at 1,000 or 2,000 m/s), cells arrive late: the far tier is still drawn, so the forest is thinner near the camera for a moment, never bare; `late` counts it. A teleport refills the 234 wanted cells in about 1 s, the nearest and in-view ones in the first frames; the old cells drop at 16 per frame (beyond R + H they cannot be drawn, so dropping late is invisible). From the default start (300 m above the highest peak) dy removes most cells, so few are wanted.

## 7. Scene side

### 7.1 Order

At load, after V3D-02 9.1 steps 1 to 4: `place_far_tier(inputs, B)`; the far placement goes to Terrain3D exactly as V3D-02 9.1 step 5 (one `add_transforms` per kind and instancer cell, then `update_mmis(-1, (0, 0), true)`); `build_near_grid`; a `Node3D` child `NearVegetation`; a `NearCells`. `--veg-whole-map`: V3D-02 unchanged, no near tier. `--veg-radius 0`: the far tier only.

Each frame in `_process`, after the camera step:

1. For each worker slot whose task `WorkerThreadPool::is_task_completed`: `wait_for_task_completion` (returns at once), `computed(q, n)`.
2. `update(viewer)` with the camera position and heading.
3. Free the results of `take_released()`.
4. For `take_drops()`: free the cell's nodes (`queue_free`).
5. For `take_attach()`: build the cell's nodes (7.2) and free its result.
6. For `start_computing()`: fill a free slot (cell, a pointer to the shared read-only inputs, T) and `WorkerThreadPool::add_task(callable_mp(this, &TerrainPreview::compute_near_slot).bind(slot), false, "near vegetation")`.

The task runs `near_instances` and `instance_buffer` into its slot (plain `std::vector<float>` per kind) and touches nothing else. The inputs (heights, layers, `GroundCells`) are immutable while any task runs: closing or reopening the world (G, a new world) first waits for every running task, then frees all near nodes and results and destroys the `NearCells`. `--veg-threads 0` computes on the main thread instead, at most 2 cells per frame in `start_computing` order (comparison and fallback, W17).

### 7.2 Near nodes (per filled cell, per kind with instances, per LOD)

| Object | Settings |
| --- | --- |
| `MultiMesh` | `transform_format` 3D, `use_colors` true, `instance_count` n, `mesh` V3D-02's `ArrayMesh` of that kind and LOD (shared, with its shared material), `buffer` the slot's floats copied into a `PackedFloat32Array` |
| `MultiMeshInstance3D` | name `n<q>k<kind>l<lod>`, child of `NearVegetation`, at the origin, `multimesh`, `cast_shadow` off, `custom_aabb` the cell box (X0..X1, bot..top, Z0..Z1), visibility range of 4.1's table (begin, end, end margin; `visibility_range_fade_mode` self where a margin is given, else disabled), layers 1 |

Buffer of one instance (W16), with s = scale_pm/1000, a = yaw1024 · (6.28318530717958647692f / 1024) in float, v = (200 + tint·55/255)/255 as V3D-02 9.1 step 5:

```
[ s*cos a, 0, s*sin a, X,   0, s, 0, Y,   -s*sin a, 0, s*cos a, Z,   r, g, b, 1 ]      (r, g, b) = (v, v, v); with --tier-colours (v, 0.45v, 0.45v)
```

These are the same transforms and colours Terrain3D would store for the instance (V3D-02 9.1 step 5), so a tree drawn by the near tier looks as it would in V3D-02. Key T sets `NearVegetation.visible` with the mesh assets' `enabled`; the bookkeeping keeps running, so turning T on again shows the current state.

## 8. Seam between the tiers

Far instances are placed once and never moved, removed or rebuilt, so no far tree pops when a cell changes tier and none is doubled (4.2). What changes at the seam is density: within `NEAR_FULL_END` of a near cell's box centre the extra trees and snags are drawn; over the next 100 m they fade (Godot's fade self, by dithering, W14); beyond, only the far tier. A cell is filled before it can be drawn (lead 648 m). Shrubs and reeds end at 1,000 m and 800 m with no fade, as in V3D-02. Known differences: the seam is a ring of 256 m cells fading one by one; near trees switch LOD at 600 m from their 256 m box centre and far trees at 600 m from their 512 m Terrain3D cell's, so near the LOD distance a few neighbouring trees may be at different LODs.

## 9. Options, keys, HUD (item 6)

### 9.1 Options after `--`

| Option | Meaning |
| --- | --- |
| `--veg-radius <m>` | R, 0 or a whole number from 1,536 to 8,192 (default 2,048: a 648 m lead at the 1,400 m reach, 6.3); 0: far tier only, no shrubs or reeds |
| `--instance-cap <n>` | the far budget B (default 2,000,000); with `--veg-whole-map` V3D-02's cap (default 2,000,000); 0 to 4,000,000 |
| `--veg-whole-map` | V3D-02's whole-map placement, for comparison |
| `--veg-threads <n>` | worker tasks in flight, 0 to 8 (default 4); 0: main thread, 2 cells per frame |
| `--tier-colours` | near-tier instances tinted red (7.2), to see the partition |
| `--at-forest` | start at the bench's forest path start (12.1), heading along the path (0 north or 180 south), at `forest_start_altitude(ground_clamp)` unless `--above` or `--alt` is given; for screenshots |
| `--no-trees` | as V3D-02: no tiers, no placement |

Refusals: `--veg-radius <v>: 0 or a whole number from 1536 to 8192`, `--veg-threads <v>: a whole number from 0 to 8`, `--veg-whole-map cannot be combined with --veg-radius`, `--at-forest cannot be combined with --at`. `--screenshot` now waits for the first drawn frame and then for `settled()` (at most 30 s; past that it notes `near vegetation not settled` and goes on), then 2.0 s.

### 9.2 Keys, HUD and status

Keys unchanged (T, G, R behave as above). The HUD gains a line between the position line and the key line:

| Mode | HUD line |
| --- | --- |
| tiers | `vegetation near <filled> cells, <pending> pending, <n> instances; far <F>` (pending = queued + computing + ready) |
| whole map | `vegetation whole map <n>` |
| off (`--no-trees`, no biome layer, or T) | `vegetation off` |

Status lines (V3D-02 10.3) in tier mode: `vegetation: far tier <F> trees and snags of <S_far> (budget <B>); near tier within <R> m in cells of 256 m`; with `--veg-radius 0`: `vegetation: far tier <F> trees and snags of <S_far> (budget <B>); no near tier: no shrubs or reeds`; `--veg-whole-map`: V3D-02's line. With `--at-forest` or `--bench` on a map without forest floor or carr: `no forest on this map: the forest path starts at the map centre` (not shown otherwise).

## 10. Invariants (item 10)

1. The preview opens `world.db` read-only and writes nothing but `--screenshot`'s file; nothing in `worldgen/`, `tools/`, `game/data/`, `extern/`, the schema or a golden file changes.
2. For every terrain cell, `Far` and `Extra` are disjoint and their union is V3D-02's uncapped candidate list of the cell, every instance with V3D-02's position, yaw, scale and tint.
3. The far tier holds exactly `{i in F : thin < T}`, at most B instances, none of a shrub or reed; it is placed at load and never changed.
4. A filled near cell holds exactly `Extra` of its 256 terrain cells, per kind, in placement order; the same for every camera path, run and platform.
5. A cell with d ≤ R is never left empty once `settled()`; no cell is ever filled with d > R + H after an `update`.
6. Per frame at most 4 cells attached and 16,384 instances (a near cell expects at most about 1,800, carr at t = 255, so the "always the first" rule never exceeds it), 16 cells dropped, `--veg-threads` tasks in flight.
7. `--veg-whole-map` gives V3D-02's placement bit for bit; `--flat-colours --no-trees` is still V3D-01's look.

## 11. Tests (item 8)

### 11.1 Unit tests (`gdext-terrain-tests`, no Godot)

| Id | Test |
| --- | --- |
| U17 | `place_candidates` over 20 fixed rectangles (corners, edges, one cell, the whole map) of a 512 test world equals the whole-map list filtered by cell, field for field and in order |
| U18 | partition: for B in {0, 1, 1,000, S_far − 1, S_far, 10^9}: T by a brute-force sort; `place_far_tier` gives that T, S_far and exactly the far set in order; per near cell `near_instances` ∪ far instances of its cells = `place_candidates` of its rectangle, disjoint; no far instance of kind 2 or 3; far count = B when no tie at T (true for the test world) |
| U19 | `build_near_grid`: M = N/16; every instance of `place_candidates(cell)` lies inside the cell's box, with Y in [bot, top] and the top of its mesh (21.7 m at the maximum scale) below top |
| U20 | wanted set: for 12 camera points (centre, corners, edges, 1 km outside the map, high above, inside a cell) the cells queued by one `update` equal a brute-force pass over all cells with the formula of 6.1; none outside [0, M) |
| U21 | hysteresis: a camera stepping 1 m at a time across R and back: each cell enters queued once at d ≤ R and drops only beyond R + H; a ±128 m wiggle 200 times changes no state of a filled cell |
| U22 | order and budgets: `start_computing` follows (in_view, d, q); `take_attach` returns at most 4 cells and stops at 16,384 instances but always returns one; `take_drops` at most 16, farthest first; `computing` never exceeds `max_in_flight` |
| U23 | teleport and cancel: from A to a point 10 km away: every A cell becomes dropping in one `update`; queued A cells empty; a computing A cell is abandoned and released by `computed`; back to A before `computed`: not abandoned and not queued twice; `settled()` holds after the simulated workers finish |
| U24 | `instance_buffer`: three hand-made instances give the floats of 7.2 (exact for X, Y, Z, 0, s, colours; rotation within 1e-6); `--tier-colours` mode |
| U25 | constants: `DEFAULT_FAR_BUDGET` (2,000,000), `FAR_KIND`, `NEAR_CELL_CELLS`, 1,200, 100, 1,400 and `NearParams` defaults; the lead R − 1,400 > 0 for every allowed R |
| U26 | `forest_column`: counts types 2 and 3 in rows [N/8, 7N/8) of columns [N/8, 7N/8); a tie goes to the smaller x; empty ground cells give N/2 |
| U27 | `forest_start_cell` in the column: the forest cell nearest to row 7N/8, a tie to the smaller row |
| U28 | the path's direction: the longer run of forest along the column from the start, a tie north |
| U29 | no forest in the column: the map's forest cell nearest to (x*, 7N/8) by squared distance, ties to the smaller row then column; no forest at all, or no ground cells: the centre, `found` false |
| U30 | `forest_start_altitude(g)` = max(g + 60, 60): 100 → 160, 0 → 60, −35 → 60 |

### 11.2 Terrain check (`gdext-terrain-check`; additions)

Runs: `w512` default and `w1024 --instance-cap 50000` (a split far tier, so trees are in the near tier) now also run the path; new `w1024 --instance-cap 50000 --veg-threads 0`, `w512 --veg-whole-map` (V3D-02's T12 and T13 unchanged, no near node) and `w512 --veg-radius 0`. The check keeps its own copy of the partition rule (T by sorting), of 6.1's distance and of the buffer formula, and uses `place_candidates` only for the candidates.

The path (fixed steps, not real time): P0 (N/2, N/2), P1 (N/2, N/4), P2 (3N/4, N/4), then a jump to P3 (N/16, 15N/16), then a jump back to P0; the camera 60 m above `ground_clamp`, heading toward the next point, moving 64 m per frame between P0, P1 and P2; at each point it waits for `settled()` (at most 900 frames).

| Id | Check |
| --- | --- |
| T17 | far tier: T, S_far and F equal the check's own; Terrain3D's stored instances of ids 0 to 4 total the far count per kind (ids 2, 3: none); first and last instance of each kind found at their place (as T12) |
| T18 | at each point, settled within 900 frames; filled ⊇ {d ≤ R} and ⊆ {d ≤ R + H} by the check's own distance; for each filled cell, one node per kind with instances and LOD, `instance_count` = the check's own `Extra` per kind, `buffer` equal to the check's own floats (W15), `custom_aabb` the box, visibility ranges as 4.1; no other child of `NearVegetation`. As built: headless, `get_instance_transform` returns the identity, so the read-back of the first transform (W16) runs only with a display (an info line headless) |
| T19 | for each filled cell: near positions and Terrain3D's far positions inside the box are disjoint and together equal the check's own `C` of its 256 cells, per kind |
| T20 | over the whole path, every frame: at most 4 attached, 16 dropped, `--veg-threads` in flight; no cell attached twice without a drop between; at P1 a ±128 m wiggle for 60 frames changes no filled cell among those within R + H of both ends of the wiggle (as built: a cell filled earlier that the wiggle carries beyond R + H may drop, as 6.2 requires); on the first frame at P3, with cells computing, the world is reopened and the near tier starts fresh (7.1's close path) |
| T21 | back at P0: the cells with d ≤ R hold buffers byte-identical to the first visit |
| T22 | status and HUD lines of 9.2 for each run; key T hides `NearVegetation` and disables the mesh assets, and T again restores both |
| T23 | `--veg-threads 0`: the same buffers as the threaded run at every point (equal to the check's own) |
| T24 | `--veg-radius 0`: no child of `NearVegetation` over the path; `--veg-whole-map`: no `NearVegetation` node, V3D-02's T12, T13 |

### 11.3 By hand on a GPU (Paul)

After V3D-02 11.3's setup (`B`, `GODOT`, `OUT`, `P`, the `seed42-<N>.db` worlds):

```
$P $OUT/seed42-2048.db --at-forest
$P $OUT/seed42-2048.db --at-forest --pitch -8 --screenshot $OUT/v3d03-near-2048.png
$P $OUT/seed42-2048.db --at-forest --pitch -8 --veg-whole-map --screenshot $OUT/v3d03-whole-2048.png
$P $OUT/seed42-2048.db --at-forest --above 400 --pitch -12 --screenshot $OUT/v3d03-high-2048.png
$P $OUT/seed42-2048.db --at-forest --above 400 --pitch -12 --instance-cap 1000000 --screenshot $OUT/v3d03-high-cap1m-2048.png
$P $OUT/seed42-2048.db --at-forest --above 400 --pitch -12 --tier-colours --screenshot $OUT/v3d03-tiers-2048.png
$P $OUT/seed42-1024.db --at-forest --pitch -8 --screenshot $OUT/v3d03-near-1024.png
$P $OUT/seed42-1024.db --at-forest --pitch -8 --veg-whole-map --screenshot $OUT/v3d03-whole-1024.png
$P $OUT/seed42-512.db --bench;  $P $OUT/seed42-1024.db --bench;  $P $OUT/seed42-2048.db --bench
$P $OUT/seed42-1024.db --veg-radius 100    (refused: the message of 9.1)
```

| Id | Look for |
| --- | --- |
| G16 | at 2048 low over forest: full density near (as `v3d03-near-1024.png`), thinner beyond about 1.2 km; `v3d03-whole-2048.png` thinner near |
| G17 | at 1024, `v3d03-near-1024.png` and `v3d03-whole-1024.png` show the same trees (shrubs too, within 1 km) |
| G18 | flying at 200 m/s and with Shift (800 m/s): no tree pops in front of the camera; the density ring fades, no trees doubled or jumping |
| G19 | `v3d03-tiers-2048.png`: red near trees only within about 1.2 km, ordinary ones everywhere |
| G20 | R and G keys: after a jump the near forest fills within about a second, nearest first; HUD counts settle |
| G21 | T hides and shows both tiers; the HUD lines of 9.2 |
| G22 | the bench lines recorded in `gdext/README.md` for 512, 1024 and 2048 |

As built: screenshots on a locked or remote machine are taken through a virtual X display, which renders on the GPU (`P` as above but run as `env -u WAYLAND_DISPLAY xvfb-run -a -s "-screen 0 1600x900x24" "$GODOT" --display-driver x11 --path $B/game res://viewer3d/terrain_preview.tscn -- ...`). Found by the engineer (RTX 4090 Laptop GPU, Vulkan Forward+, 1600 × 900; `gdext/README.md`): G16 (full density to the skyline at 2048, no visible ring; `--veg-whole-map` plainly thinner), G17 (identical pixel for pixel at 1024), G19 (red near trees to about 1.4 to 1.6 km, no doubled trees, no bare ring), G22 (12.2), and G18 in part (no late cell in the bench flights). G18 by eye, G20 and G21 need a person at the controls and are not done.

## 12. Bench (measurements only, no bounds)

### 12.1 Forest path

Column x* = the terrain column in [N/8, 7N/8) with the most cells of ground type 2 or 3 (forest) in rows [N/8, 7N/8) (ties: smallest x; `forest_column`). r2, the lead's ruling of 2026-11-27 (r1 started at (x*, 7N/8), which on seed 42 at 2048 lay on the sea, below the sea plane):

| Step | Rule (`forest_start_cell`) |
| --- | --- |
| start | the forest cell of column x* nearest to row 7N/8, ties to the smaller row |
| no forest in the column | the forest cell of the map nearest to (x*, 7N/8) by squared distance, ties to the smaller row, then the smaller column |
| no forest on the map | the map centre (N/2, N/2), `found` false, with the status line of 9.2 |
| direction | along the start's column, north (0°) or south (180°), whichever has the longer run of forest cells from the start (the start counted in both; ties north) |
| altitude | `forest_start_altitude(g)` = max(g + 60, 60) m, g = `ground_clamp` (never below the sea plane plus 60 m), set each frame (terrain-following) |

The flight runs min(12N, 8,192) m, pitch −8, speed × frame time per frame, vsync off, after the near tier has settled at the start (at most 30 s). Seed 42: start (592, 1759) at 2048, (275, 865) at 1024, column 129 at 512.

### 12.2 Lines

`--bench` keeps V3D-02's lines (`vegetation placement` is now `place_far_tier`, `instances` the far count of S_far, `cap` B) and adds, after the view line and before V3D-01's flight:

```
UNMAPPED-TERRAIN-BENCH: tiers: far <F> of <S_far> (budget <B>), near radius <R> m, settled <ms> ms after the first frame, near cells <c>, near instances <n>, memory static <MB> MB
UNMAPPED-TERRAIN-BENCH: forest flight at <v> m/s, column <x>, <m> m: frames <k>, mean <ms>, max <ms>, over 11.1 ms <a>, over 16.7 ms <b>, over 33.3 ms <c>; cells filled <f> (<r> per s), dropped <d>, late <L>, most pending <P>; compute mean <us> us, max <us> us; attach max <us> us; memory static <MB> MB
```

The forest flight line is printed for v = 200 and 800. `settled` runs from the first frame at the view point (cell (N/2, N/2), 150 m above ground) to `settled()`, counted only once the near tier has run a frame at the new camera position (as built: otherwise the cells of the previous position read as settled). `compute` is the worker time per cell, `attach` the main-thread time of step 5 in a frame. Static memory with `--no-trees` and `--veg-whole-map` goes in the README table beside these. Headless, the bench prints the V3D-02 lines and the `tiers` line only (the near tier settles headless; no frame is drawn).

### 12.3 Measured (r2)

2026-11-27, `linux-gcc-release`, seed 42, default options (B 2,000,000, R 2,048 m, 4 tasks), drawn through the virtual display of 11.3 on an RTX 4090 Laptop GPU at 1600 × 900. Frame times there are 20 to 30 ms for every frame, whatever is drawn, because Xvfb presents by copying: indicative only. Video memory still reads 0 MB (the NVIDIA Vulkan driver reports none).

| N | first frame | far tier: placement, transfer | far F of S_far (S) | static memory drawn, at load / after the flights | settled at the view | near cells, instances at the view |
| --- | --- | --- | --- | --- | --- | --- |
| 512 | 114 ms | 9.0 ms, 72 ms | 104,482 of 104,482 (187,951) | 132.0 / 133.2 MB | 853 ms | 181, 17,688 |
| 1024 | 281 ms | 56 ms, 498 ms | 604,322 of 604,322 (1,088,122) | 454.1 / 464.1 MB | 782 ms | 153, 14,293 |
| 2048 | 654 ms | 293 ms, 1,435 ms | 2,000,000 of 3,057,704 (5,523,211) | 1,383.7 / 1,450.1 MB | 926 ms | 181, 16,608 |

Forest flights at 200 and 800 m/s, every size: no late cell, at most 7 cells pending, compute mean 9 to 86 µs and max 448 µs per cell, attach max 630 µs per frame. Drawn static memory is far above the headless figures (no trees / tiers / `--veg-whole-map`: 47.8 / 60.3 / 66.7 MB at 512, 66.9 / 130.5 / 178.1 MB at 1024, 143.0 / 352.2 / 342.3 MB at 2048), since the renderer's multimeshes are counted. The full tables are in `gdext/README.md`.

## 13. Determinism notes

Candidates, `thin`, T, the partition, every position, yaw, scale and tint are V3D-02's integer and exact-float arithmetic (V3D-02 15); T is chosen by sorting integers. The bookkeeping uses `double` distances on exact inputs with the project's flags, and breaks every tie by q, so its output is a function of the call sequence (6.2). What is not deterministic is timing: which frame a cell is computed or attached in depends on the workers, so the set of filled cells on a given frame may differ between runs; the settled state and every cell's contents do not. Results are applied in `take_attach` priority order, never in completion order. The rotation entries of the buffer use `std::cos` and `std::sin` of a float (rendering only, may differ in the last bit between platforms, as V3D-02's `Basis` does); the checks compare them on one machine against their own copy. Nothing on our side iterates an unordered container.

## 14. CI (item 9)

No workflow change. `gdext-linux` and `gdext-windows` run the new unit tests in `gdext-terrain-tests` and the longer `gdext-terrain-check` (five more phases: about 20 s more expected, timeout 300 s unchanged); the worker tasks run under the headless editor on both. The `build` jobs, `world-hashes.sh` and `compare-gdext-hashes` are unaffected.

## 15. To verify (engineer, before building on it)

| Id | Assumed | If false | Found (r2) |
| --- | --- | --- | --- |
| W13 | Godot 4.7's visibility range measures the 3D distance from the camera to the centre of the instance's AABB (the node's `custom_aabb` when set) and never draws beyond end + end margin | R's minimum and default rise by the difference (for nearest-point distance: none needed; for anything larger, raise `NEAR_VISIBLE_MAX`) | true: screenshots through the virtual display, 2026-11-28 (`v3d03-tiers-2048.png`: near trees end inside the lead, no bare ring) |
| W14 | `visibility_range_fade_mode` self fades an opaque `StandardMaterial3D` multimesh smoothly in Forward+ | fade disabled: the extra trees of a cell appear at once at 1,200 m | true: screenshots through the virtual display, 2026-11-28 (the tier ring thins out by dithering, no hard edge) |
| W15 | headless, `MultiMesh::get_buffer` returns the buffer set | T18 compares the copy the scene keeps in `--check` mode and `instance_count` | true: T18 headless |
| W16 | the 3D buffer layout is the 3 × 4 rows (basis row with the origin component) then RGBA | change `instance_buffer`; U24 and T18 follow; the check also compares `get_instance_transform` of the first instance | true: T18's read-back with a display (Vulkan, 2026-11-27); headless `get_instance_transform` gives the identity, so that read-back is skipped there |
| W17 | `WorkerThreadPool::add_task` with a bound `callable_mp` runs on a worker from a GDExtension, headless and on Windows, and `wait_for_task_completion` after `is_task_completed` returns at once | default `--veg-threads 0` | true on Linux, headless and drawn (T20, T23, the bench); Windows by CI run 46107942031, pending |
| W18 | creating four cells' nodes (up to 28 multimeshes, about 88 KB of buffer each at most) takes under 1 ms on the main thread | `max_attach_cells` 2 (capacity 120 cells per s; the 2,000 m/s step then arrives late) | true: drawn bench through the virtual display, attach at most 630 µs a frame (12.3); screenshots of 2026-11-28 show no stall |
| W19 | V3D-02's far path is unchanged by restricting it to ids 0, 1 and 4 (ids 2, 3 assets present with no instances) | none needed | true: T17, and V3D-02's T12 and T13 under `--veg-whole-map` |

## 16. Questions for Paul (decided 2026-11-27: Paul accepts the recommendations, Q-V3D-03-2 changed)

| Id | Question | Recommendation (interim reading) |
| --- | --- | --- |
| Q-V3D-03-1 | The near tier in the preview's own multimeshes, or in Terrain3D's instancer by editing `get_instances()` per cell | own multimeshes: Terrain3D has no per-cell removal, rebuilds a changed cell with two RS calls per instance in one piece on the main thread, and revisits every cell of the region (section 3); ours attach with one buffer copy. The far tier stays in Terrain3D, so the fly-across test still exercises its instancing |
| Q-V3D-03-2 | Far tier: trees and snags thinned to 1,000,000, or 2,000,000 (closer to V3D-02's far look at 2048, about 580 MB more), or none | 1,000,000: 1024 keeps every tree as in V3D-02; 2048 about 50 trees per hectare beyond 1.2 km, about half V3D-02's memory. Decided as recommended, then changed by Paul (2026-11-27): he meant the lead's 2,000,000; measured, the step between tiers then cannot be seen (4.3) |
| Q-V3D-03-3 | Full density to 1,200 m with a fill radius of 2,048 m, or further (the cells held grow with the square) | 1,200 and 2,048: placeholder trees 1.2 km away are a few pixels tall, and the lead keeps ahead up to 2,000 m/s |
| Q-V3D-03-4 | 256 m near cells, or the design's 128 m chunks | 256 m: four times fewer multimeshes and draw calls for the preview; the game's chunk manager can keep 128 m chunks and group 2 × 2 of them for vegetation |
| Q-V3D-03-5 | Tiers at every map size (512 and 1024 fit whole), or tiers only where V3D-02's cap would thin | every size: one path, and at 512 and 1024 the far tier already holds every tree, so the look is V3D-02's |

No `proposals.md` entry is needed: the stage reads nothing new and the 256 m cell is a preview choice (Q-V3D-03-4).

**Open after r2.**

1. A frame bench on a real display: every drawn figure so far is through the virtual display, whose copy sets the frame time (12.3).
2. G18 by eye, and G20 and G21 (interactive keys R, G and T) are not done.
3. MSVC and CI: pending (run 46107942031).
4. Video memory still reads 0 MB.

## Paul's flight (2026-11-28)

Paul flew the build on the real display: a solid 60 frames per second throughout, which is the display's cap. He notes that 60 is good for a flat screen and that VR needs at least 72. An uncapped frame bench on the real display is still open.

## Revision notes

- r1 (2026-11-27): first draft. Approved by Paul the same day; Q-V3D-03-1 to 5 decided as recommended (status line only).
- r2 (2026-11-28): as built. Default far budget 2,000,000 (Paul, Q-V3D-03-2 changed) in 4.2, 5, 9.1, U25; 4.3 and 6.3 measured counts and times replace the estimates; 12.1 the lead's ruling on the forest start (`forest_column`, `forest_start_cell`, `forest_start_altitude` in `near_cells.hpp`, U26 to U30; 9.1 `--at-forest` heading and altitude; 9.2 the no-forest status line); 12.2 settling counted after a frame at the new position; 12.3 measured; T18 identity read-back headless; T20 wiggle on cells within R + H at both ends, and the reopen with cells computing; 11.3 the screenshot commands as run and through the virtual display, with what was found; 15 how each item was confirmed; 16 decided; Open after r2 added; section 2 notes the one-commit build.
