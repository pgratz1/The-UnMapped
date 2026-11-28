# V3D-04 Look pass: mountains, shrubs, sea horizon

Status: **r1, 2026-11-28, interim: built on the recommendations, pending Paul's review.** Paul is away overnight and authorised building this stage on the recommendations of section 14 (Q-V3D-04-1 to 4); each is one constant or one option item, so it reverses cheaply. Fourth stage of the 3D terrain viewer track (`docs/handoff-3d-terrain-viewer.md`), on top of V3D-01 r3, V3D-02 r2 and V3D-03 r2 as built ("V3D-02 n" and "V3D-03 n" refer to their sections). It answers V3D-02's "Open after r2" items 2, 3, 4 and 6 and Paul's ruling of 2026-11-27: **mountains are mostly rock up to the snow.** Art stays generated placeholders; no asset file is added.

**No world change.** The stage reads `world.db` and writes nothing to it or to the source tree. `GENERATOR_VERSION`, the schema, every golden file, `world-hashes.sh` and its cases are unchanged; nothing under `worldgen/`, `tools/worldgen-cli/`, `game/data/` or `extern/` changes (Terrain3D is not edited). No new dependency. **Placement is unchanged bit for bit**: `GroundCells::rock` and `::snow` keep V3D-02 4.1's values, and placement, densities, tiers and the far budget read only those (V3D-02 6.4, V3D-03 4.2). Owner: viewer engineer.

**Not in this stage:** carved rivers; the pale fringe along painted rivers (11); detail noise; real art; markers (V3D-05); streaming; collision; VR; any change to placement, densities, tiers or the far budget; Terrain3D's macro variation, projection and dual scaling (section 3).

## 1. Items

| Item | Fix | Restore V3D-03 (section 9) |
| --- | --- | --- |
| R rock | two more rock ground types, chosen per cell by altitude band (strata) | `rock` |
| L light | sun from the south-south-west, ambient partly a fixed colour | `light` |
| S snow | a snow line over all land from temperature and slope; cliffs stay rock | `snow` |
| B shrubs | a three-lobed, smooth-shaded, greener shrub | `shrubs` |
| H sea | unshaded sea in exactly the fog and sky-ground colour | `sea` |

V3D-02 4's biome bands under the rock (the thin orange and purple stripes) are **kept** (4.5, Q-V3D-04-3).

## 2. Parts

| Area | Files | Contents |
| --- | --- | --- |
| look (plain C++, no Godot types) | new `gdext/src/terrain/look.hpp`, `.cpp` | `Look`, `parse_look_v3d03`, light constants, `sun_direction`, `peak_view` (sections 5, 9) |
| ground (plain C++) | `ground.hpp`, `.cpp` | ids 15 and 16, `rock_variant`, `snow_line`, `build_control(layout, cells, look)`, `look_counts` (section 4) |
| meshes (plain C++) | `placeholder_meshes.hpp`, `.cpp` | `build_mesh(kind, lod, look)`, smooth normals for the shrub (section 7) |
| scene | `terrain_preview.hpp`, `.cpp` | 17 texture assets, sun, environment, sea material, options, status, bench (sections 5, 6, 9) |
| check | `terrain_check_ground.cpp`, `terrain_check.cmake` | T25 to T30 (10.2) |
| unit tests | new `gdext/tests/look_test.cpp`; `ground_test.cpp`, `meshes_test.cpp` | U31 to U39 (10.1) |
| docs | `gdext/README.md`, "Terrain preview" | options, status, checks, screenshots |

`look.cpp` joins `unmapped_terrain_core`; `look_test.cpp` joins `gdext-terrain-tests`. The CMake files name every file, so the build lands in one commit. Nothing new is read from the world.

## 3. Terrain3D features considered (from source)

| Feature | Source | Use here |
| --- | --- | --- |
| macro variation | `shaders/macro_variation.glsl:7-24`: multiplies the albedo of **every** texture by two colours mixed by a noise texture, weighted by `clamp(normal.y + macro_variation_slope, 0, 1)`; the noise is a `FastNoiseLite` cellular texture Terrain3D generates itself (`terrain_3d_material.cpp:666-698`) | no: it shifts every biome's colour (breaks V3D-02 G8's match with the 2D map) and its noise is not ours to test |
| projection | `shaders/projection.glsl:6-18`: on faces steeper than 45° (`normal.y <= sqrt(0.5)`) the UV becomes (along the face, −height), with the face direction snapped to 45° steps; all textures | no (deferred): helps only with texture strata, which this stage does not add, and the 45° snapping may show seams |
| dual scaling | `shaders/dual_scaling.glsl:7-11`: one texture id drawn at two scales by distance | no: a far-tiling fix, not a tone fix |
| blend sharpness | `shaders/main.glsl:88`, 0.5 | unchanged |
| texture ids | control base and overlay are 5 bits (`terrain_3d_util.h` `enc_base`, `enc_overlay`) | ids 15 and 16 added (4.1) |

## 4. Ground (items R and S; plain C++, integer only)

### 4.1 Two rock types

Added to V3D-02 3.1 (`GROUND_TYPES`, `GROUND_TYPE_COUNT` 17); textures by V3D-02 3.2 unchanged (the hash's third argument `16*id + o` makes them differ from id 11); `albedo_color` by 3.3; detiling 0.2 and 0.2. Ids 0 to 14 and their bytes are unchanged (U8's hashes stay).

| Id | Type | Used for | Colour (sRGB) | P0 | G | S | Rough. byte | uv_scale (1/m) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 15 | pale rock | rock band overlay (4.2) | 0x8C8880 | 4 | 100 | 6 | 191 | 1/24 |
| 16 | brown rock | rock band overlay (4.2) | 0x7A6A58 | 8 | 120 | 5 | 204 | 1/20 |

Neither is a land type (`is_land_type` stays ≤ 10) and neither is ever a base. Control values: base ≤ 14 always, so bits 27 to 30 are never all set (no NaN or infinity) and a denormal still needs base 0 with overlay ≤ 1 (V3D-02 3.1, W10 unchanged).

### 4.2 Rock variant (strata)

For every land cell, with E the elevation in dm (int32) and `fdiv` floor division:

```
jr(c)       = int(hash32(x, y, 0x524B4A54) >> 24) - 128                (-128..127 dm)
band(c)     = fdiv(E + jr(c), ROCK_BAND_DM)                             ROCK_BAND_DM = 450 (45 m)
rock_variant(c) = ROCK_SEQ[((band mod 5) + 5) mod 5]                    ROCK_SEQ = {11, 15, 11, 16, 15}
```

On a 45° face a band is about 3 cells wide, its edge ragged by up to ±13 m; the sequence repeats every 225 m of height. Not land: 11 (unused).

### 4.3 Snow line

For every land cell (types 0 to 10, not only scree), with the temperature layer present; g as V3D-02 4.1:

```
jt(c)  = int((hash32(x, y, 0x534E4F57) >> 24) % 13) - 6                (-6..6, 0.1 C)
T'     = T + jt(c)
sT     = T' >= SNOW_T0 ? 0 : T' <= SNOW_T1 ? 255 : ((SNOW_T0 - T')*255 + 10) / 20     SNOW_T0 = -10, SNOW_T1 = -30
hold   = g <= 320 ? 255 : g >= 457 ? 0 : ((457 - g)*255 + 68) / 137                    (45 to 55 degrees)
snow_line(c) = (sT*hold + 127) / 255
```

`SNOW_T1 = SNOW_T0 − 20` always (the ramp of 2 °C, about 300 m at 6.5 °C per km). Not land, or no temperature layer: 0.

| | V3D-02 4.1 snow (kept for placement) | V3D-04 snow line (control only) |
| --- | --- | --- |
| where | scree (alpine) only | every land type (in practice alpine, since T < −1 °C needs about 1 km) |
| temperature | −2 °C to −6 °C | −1 °C to −3 °C, ±0.6 °C per-cell jitter |
| slope | none; rock wins on steep cells by the largest-blend rule | full to 45°, none past 55° |
| pick | largest blend of rock, snow, border | snow first when `snow_line ≥ 128` (4.4) |

Alpine ground starts at T ≤ 0.5 °C (P1-04): the line lies about 230 m above the tree line and is full about 300 m higher, so the mountain is bare scree and rock up to it and white above it but for faces past about 50°. `SNOW_T0` is the one tuning constant (10.3).

### 4.4 Control (replaces V3D-02 4.2's pick when the look item is on)

```
rock_id = look.rock ? rock_variant(c) : 11
if look.snow and snow_line(c) >= 128:   pick = (12, snow_line(c))
else: candidates (rock_id, rock(c)), (12, look.snow ? snow_line(c) : snow(c)), (ov, border); largest blend, ties to the earlier
```

Base, painted cells, non-land cells, the border rule and the encoding are V3D-02 4.2's. With `rock` and `snow` both restored, `build_control` is V3D-02's bit for bit (U34). `GroundCells` gains `rock_variant` and `snow_line` (n² bytes each); `rock` and `snow` are untouched.

`LookCounts look_counts(const Layout&, const GroundCells&, std::span<const std::uint32_t> control)`: over cells of type 9 not painted, `alpine`, `snow` (overlay 12, blend ≥ 128), `rock` (overlay 11, 15 or 16, blend ≥ 128).

### 4.5 Biome bands under the rock: kept

The orange and purple stripes are heath (0x94664A) and highland moor (0x7E6680): P1-04's uplands lie between forest and alpine, so on a flank they run as contour belts, and where rock's blend is below 255 they show between rock texels. They are the 2D map's own belts; dropping or softening them breaks V3D-02 G8's match with the 2D map, and they read loudest today because the slopes they lie on are near black and lit from behind (item L). Judged again on the L and R screenshots (Q-V3D-04-3).

## 5. Light (item L; scene side, constants in `look.hpp`)

| Setting | V3D-03 (scene file) | V3D-04 | Reason |
| --- | --- | --- | --- |
| sun azimuth (sun's position, clockwise from north) | 315° (north-west) | 210° (south-south-west) | default views, the start and the forest flights look north, so they saw the faces turned away from a north-west sun |
| sun elevation | 40° | 45° | less of the land in grazing light |
| sun `light_energy` | 1.0 | 0.75 | keeps flat lit ground at V3D-03's brightness with the added ambient |
| `ambient_light_source` | 3 sky | 3 sky | |
| `ambient_light_sky_contribution` | 1.0 | 0.4 | the sky's lower half is the dark sea colour (V3D-02 7), which starved faces turned sideways |
| `ambient_light_color` (sRGB) | (0, 0, 0) (unused) | (0.70, 0.74, 0.82) | a pale sky grey-blue, the same from every direction |
| `ambient_light_energy` | 1.0 | 0.8 | |

```
sun_direction(A, e) = (-cos e * sin A, -sin e, cos e * cos A)      (float; the light's -Z axis, the direction light travels; X east, Z south)
sun_direction(315, 40) = (0.541675, -0.642788, 0.541675)           (the scene file's value, U38)
sun_direction(210, 45) = (0.353553, -0.707107, -0.612372)
```

The scene sets the `Sun` node's basis to `Basis::looking_at(sun_direction, (0, 1, 0))` and the environment values above in `open_world`, every mode (`--flat-colours` too). Estimated effect (linear, tonemap linear): a flat lit cell gets 0.75 · sin 45° + about 0.31 ambient ≈ 0.84 against V3D-03's 1.0 · sin 40° + about 0.2 ≈ 0.84, so the map seen from above keeps its brightness (L2); a face turned from the sun gets about 0.31 against about 0.1 to 0.15, so shaded rock rises from near black to about 60/255 (L1). Trees and shrubs lighten on their shaded sides the same way. Top-down views are now lit from the bottom of the image, which some viewers read as inverted relief (Q-V3D-04-1).

## 6. Sea (item H; scene side)

The sea plane's `StandardMaterial3D` becomes `shading_mode` unshaded, albedo the sea colour (`sea_colour(records)`), no other change; the fog and the sky's ground colours stay V3D-02 7's. Cause of the band: the shaded sea (roughness 0.2) reflects the bright sky horizon at grazing angles (Fresnel) inside the fog's start, so it is lighter than the fog colour. Unshaded, the sea, the fog and the sky below the horizon are one colour, so no band can form. Loss: no sun glint on the sea (Q-V3D-04-4). Lakes and painted rivers are terrain and stay lit.

## 7. Shrub mesh (item B; plain C++)

| | V3D-02 6.2 | V3D-04 |
| --- | --- | --- |
| primitives | one ellipsoid((0,0.5,0), 1.2, 0.9, 1.2, 6, 4) | three ellipsoids, 5 sides, 3 rings: A ((0, 0.55, 0), 0.95, 0.75, 0.95); B ((0.55, 0.45, 0.30), 0.70, 0.60, 0.70); C ((−0.45, 0.50, −0.40), 0.75, 0.65, 0.75) |
| triangles (LOD 0, the only LOD) | 36 | 60 (3 × 5 · (2·3 − 2)) |
| normals | flat (face) | smooth: at vertex p of a lobe with centre c and radii r, normalize((p − c).x/rx², (p − c).y/ry², (p − c).z/rz²) |
| colour | 0x55663A per primitive | per vertex: t = clamp(p.y / 1.30, 0, 1), channel = round(dark + (light − dark)·t), dark 0x3C6420, light 0x6E9E38 |
| size at scale 1 | 2.4 m wide, 1.4 m high | 2.45 m (x) by 2.15 m (z), 1.30 m high (top of A), bottoms 0.15 to 0.2 m below ground |

Winding is V3D-02's (clockwise from outside, W7). Scale range, tint, material (roughness 0.9, vertex colour as sRGB albedo) and placement are unchanged. Colour against the grounds shrubs stand on: luma (54r + 183g + 19b) / 256 of dark 86 and light 140 lies between forest floor (82) and grassland (160) and steppe (208); hue about 90°, saturation 0.65 at the light end against the old 0.43 at hue 80°: a clear green that reads as foliage on pale ground and as lighter undergrowth on forest floor. Reeds (thin straw stalks, 12 triangles) and snags (grey-brown dead wood, only on waste) do not read as stones and stay as built. Kind 2 at 300 m is about 4 pixels wide: colour carries it there, shape at 10 m.

`build_mesh(kind, lod, const Look&)`: `look.shrubs` false gives V3D-02's shrub; every other kind ignores the look.

## 8. Plain C++ contracts (`look.hpp`, namespace `unmapped::terrain`)

| Name | Contract |
| --- | --- |
| `struct Look { bool rock = true, light = true, snow = true, shrubs = true, sea = true; }` | true = the V3D-04 look of that item |
| `std::optional<Look> parse_look_v3d03(std::string_view list)` | `all`, or a comma list of `rock`, `light`, `snow`, `shrubs`, `sea` (repeats allowed): those items false; anything else: `std::nullopt` |
| `SUN_AZIMUTH_DEG` 210, `SUN_ELEVATION_DEG` 45, `SUN_ENERGY` 0.75, `AMBIENT_SKY_CONTRIBUTION` 0.4, `AMBIENT_RGB` {0.70, 0.74, 0.82}, `AMBIENT_ENERGY` 0.8, and the V3D-03 set `V3D03_SUN_AZIMUTH_DEG` 315, `_ELEVATION_DEG` 40, `_ENERGY` 1.0, contribution 1.0, colour 0, energy 1.0 | section 5 |
| `std::array<float, 3> sun_direction(float azimuth_deg, float elevation_deg)` | section 5 |
| `ROCK_BAND_DM` 450, `ROCK_SEQ`, `SNOW_T0` −10 (in `ground.hpp`) | section 4 |
| `struct PeakView { int x, y; int cam_x, cam_y; bool found; }`; `PeakView peak_view(const Layout&, std::span<const std::int16_t> elevation, const GroundCells&)` | the land cell (types 1 to 10, not painted) of largest E, ties to the smaller row then column; camera cell (x, min(N − 1, y + 125)), 2,000 m south; none: the centre, `found` false |

## 9. Options, status, bench

### 9.1 Options after `--`

| Option | Meaning |
| --- | --- |
| `--look-v3d03 <list>` | the named items (8, `parse_look_v3d03`) drawn as in V3D-03; `all` for every item |
| `--at-peak` | start at `peak_view`'s camera cell, at the peak vertex's height + 200 m, heading 0, pitch −8 (each overridden by `--alt`, `--above`, `--heading`, `--pitch`); for the snow and rock screenshots |

Refusals: `--look-v3d03 <v>: all, or a comma list of rock, light, snow, shrubs, sea`; `--at-peak cannot be combined with --at or --at-forest`. Key G (reopen) keeps the look. `--flat-colours`: control 0 as V3D-01, so R and S do not show; L, B and H still apply. `--flat-colours --no-trees --look-v3d03 all` is V3D-01's look.

### 9.2 Status and bench

Status lines: `ground: textured, 17 ground types` (was 15); when any item is restored, `look: V3D-03 for <items>` in the order rock, light, snow, shrubs, sea; with `--at-peak` and no land, `no land on this map: the peak view starts at the map centre`. `--bench` (headless too) adds after V3D-02's third line:

```
UNMAPPED-TERRAIN-BENCH: look: alpine cells <a>, snow <s> (<p>%), rock <r> (<q>%); with V3D-03's control snow <s0> (<p0>%), rock <r0> (<q0>%)
```

from `look_counts` on the current control and on `build_control` with `rock` and `snow` restored; percentages of `alpine` to one decimal (round half up on integers: `(1000·s + a/2) / a`, printed as tenths; 0.0 when a = 0).

## 10. Tests

### 10.1 Unit tests (`gdext-terrain-tests`, no Godot)

| Id | Test |
| --- | --- |
| U31 | the table of 4.1 (17 rows); `build_ground_texture` of 15 and 16 against the test's own copy of V3D-02 3.2 at four pixels and recorded FNV-1a 64 hashes (GCC, Clang, MSVC alike); U8's hashes of ids 0 to 14 unchanged |
| U32 | `rock_variant` on hand grids: E = 0, 449, 450, 2,250, −1, −450, −451 with jr forced through chosen (x, y): the band and sequence entry by the test's own floor division; jr in [−128, 127] over 10,000 cells |
| U33 | `snow_line` at T' ∈ {−9, −10, −11, −20, −30, −31} × g ∈ {0, 320, 388, 456, 457}: the formula's values (for example T' −20, g 0 → 128; T' −30, g 388 → 128); jt in [−6, 6]; non-land and no temperature give 0 |
| U34 | `build_control`: a cell with `snow_line` 128 and rock 255 picks 12; 127 and rock 255 picks the rock variant; the rock overlay id equals `rock_variant`; with `rock` and `snow` restored, the output equals V3D-02's rule (the test's existing copy) on the U10 grids and on a 512 test world; no base above 14, no overlay above 16 |
| U35 | `build_ground_cells`: `type`, `painted`, `rock`, `snow` equal to V3D-02 4.1's on the 512 test world; so `place_vegetation` and `place_far_tier` output is unchanged (U13, U18 pass unchanged) |
| U36 | shrub: 60 triangles, unit normals, each normal within 1e-5 of its lobe's gradient direction and on the outward side; top 1.30 m ± 0.01; colours between dark and light per channel, non-decreasing in y; restored: V3D-02's 36-triangle shrub byte for byte; kinds 0, 1, 3, 4 equal with either look |
| U37 | `parse_look_v3d03`: `all`, `rock`, `sea,rock`, `rock,rock`; refused: empty, `Rock`, `trees`, `rock,`; `Look{}` all true |
| U38 | `sun_direction` at (315, 40) and (210, 45) within 1e-5 of section 5; unit length; the constants of 8 |
| U39 | `peak_view`: the highest land cell, ties to the smaller row then column; painted and water cells ignored; camera row clamped at N − 1; no land: centre, `found` false |

### 10.2 Terrain check (`gdext-terrain-check`; additions)

New run `w512 --look-v3d03 all`. The check keeps its own copy of 4.2 to 4.4 and of section 5's values.

| Id | Check |
| --- | --- |
| T25 | `present`: 17 texture assets; 15 and 16 as T9; the control image equals the check's own 4.4 at every cell and at the 32 `get_control` points; under `--look-v3d03 all` it equals V3D-02's rule (T10 unchanged) |
| T26 | `Sun`: its −Z within 1e-5 of `sun_direction(210, 45)`, `light_energy` 0.75; environment ambient source, contribution, colour and energy as section 5; under `--look-v3d03 all` the V3D-03 values |
| T27 | sea material unshaded, albedo the sea colour; restored: shaded, roughness 0.2; fog and sky as T14 |
| T28 | the kind 2 mesh asset's and near nodes' `ArrayMesh`: 180 vertices, normals and colours equal to `build_mesh(2, 0, look)`; T12, T13, T17 to T19 pass unchanged on every run (placement untouched) |
| T29 | status lines of 9.2 for every run; the refusals of 9.1 |
| T30 | `--bench` headless on `w512`: the look line present, its counts equal to the check's own `look_counts` |

### 10.3 Screenshots (the engineer judges from these)

Virtual display form (V3D-03 11.3), with `OUT=~/.cache/the-unmapped/out` and the seed 42 worlds:

```
X() { env -u WAYLAND_DISPLAY xvfb-run -a -s "-screen 0 1600x900x24" "$GODOT" --display-driver x11 --path "$B/game" res://viewer3d/terrain_preview.tscn -- "$@"; }
# R: rock (same light both)
X $OUT/seed42-1024.db --at 420 930 --above 300 --pitch -10 --look-v3d03 rock --screenshot $OUT/v3d04-rock-before.png
X $OUT/seed42-1024.db --at 420 930 --above 300 --pitch -10 --screenshot $OUT/v3d04-rock-after.png
# L: light, looking south at the faces turned from the sun; and from above
X $OUT/seed42-1024.db --at 420 930 --above 300 --heading 180 --pitch -10 --look-v3d03 light --screenshot $OUT/v3d04-light-s-before.png
X $OUT/seed42-1024.db --at 420 930 --above 300 --heading 180 --pitch -10 --screenshot $OUT/v3d04-light-s-after.png
X $OUT/seed42-1024.db --at 512 512 --alt 22000 --pitch -89 --look-v3d03 light,sea --screenshot $OUT/v3d04-top-before.png
X $OUT/seed42-1024.db --at 512 512 --alt 22000 --pitch -89 --look-v3d03 sea --screenshot $OUT/v3d04-top-after.png
# S: snow, at 1024 and 2048
X $OUT/seed42-1024.db --at-peak --look-v3d03 snow --screenshot $OUT/v3d04-snow-1024-before.png
X $OUT/seed42-1024.db --at-peak --screenshot $OUT/v3d04-snow-1024-after.png
X $OUT/seed42-2048.db --at-peak --above 1500 --pitch -15 --look-v3d03 snow --screenshot $OUT/v3d04-snow-2048-before.png
X $OUT/seed42-2048.db --at-peak --above 1500 --pitch -15 --screenshot $OUT/v3d04-snow-2048-after.png
# B: shrubs at 10 m and 300 m
X $OUT/seed42-1024.db --at-forest --above 10 --pitch -20 --look-v3d03 shrubs --screenshot $OUT/v3d04-shrub-10-before.png
X $OUT/seed42-1024.db --at-forest --above 10 --pitch -20 --screenshot $OUT/v3d04-shrub-10-after.png
X $OUT/seed42-1024.db --at-forest --above 300 --pitch -35 --look-v3d03 shrubs --screenshot $OUT/v3d04-shrub-300-before.png
X $OUT/seed42-1024.db --at-forest --above 300 --pitch -35 --screenshot $OUT/v3d04-shrub-300-after.png
# H: sea horizon, high and low
X $OUT/seed42-1024.db --alt 30000 --pitch -10 --look-v3d03 sea --screenshot $OUT/v3d04-horizon-before.png
X $OUT/seed42-1024.db --alt 30000 --pitch -10 --screenshot $OUT/v3d04-horizon-after.png
X $OUT/seed42-1024.db --alt 2000 --pitch -3 --look-v3d03 sea --screenshot $OUT/v3d04-horizon-low-before.png
X $OUT/seed42-1024.db --alt 2000 --pitch -3 --screenshot $OUT/v3d04-horizon-low-after.png
# all: the whole change
X $OUT/seed42-2048.db --at-forest --above 400 --pitch -12 --look-v3d03 all --screenshot $OUT/v3d04-all-before.png
X $OUT/seed42-2048.db --at-forest --above 400 --pitch -12 --screenshot $OUT/v3d04-all-after.png
X $OUT/seed42-1024.db --bench;  X $OUT/seed42-2048.db --bench
```

Luma is (54r + 183g + 19b) / 256 on sRGB bytes. "Changed pixels" are those that differ between a pair (the pair differs in one item only, and HUD and status rows are excluded: rows 0 to 110 and the bottom 150).

| Id | Good means (record the before figure too) |
| --- | --- |
| R1 | changed pixels of `v3d04-rock-*`: after luma 10th-to-90th percentile spread ≥ 1.5 × before's; after median ≥ before's; bands of tone follow the contours by eye |
| L1 | `v3d04-light-s-after.png`, rows 300 to 600: the darkest decile of luma ≥ 45 (before: record) |
| L2 | `v3d04-top-*`: the mean luma of the image (HUD rows excluded) within ±5 of before; after, at most 1% of pixels have a channel at 255 |
| S1 | bench look line, seed 42 at 1024 and 2048: snow ≥ 30% and ≤ 60% of alpine cells and ≥ 2 × the V3D-03 figure; rock ≥ 128 still on every alpine cell past 55° (by construction, T25) |
| S2 | `v3d04-snow-*-after.png`: one continuous white cap above a ragged line, dark faces through it; changed pixels with all channels ≥ 200 at least 2 × before |
| B1 | changed pixels of `v3d04-shrub-10-*`: after mean HSV saturation ≥ 0.45 and mean hue 75° to 120°; rounded clumps, no facets, by eye |
| B2 | `v3d04-shrub-300-after.png`: shrubs visible as green dots distinct from the ground and from the darker trees, by eye |
| H1 | `v3d04-horizon-after.png` and `-low-after.png`: every pixel from the horizon row down to the first land pixel of its column within 2 per channel of the sea colour (sRGB); before: record the largest difference and the band's rows |

If S1 misses, change `SNOW_T0` in steps of 5 (0.5 °C; lower for less snow), at most three times, then report the miss. If L1 misses, raise `AMBIENT_ENERGY` to 1.0 and lower `SUN_ENERGY` to 0.65, once, then report.

## 11. River fringe: not built

The pale one-cell fringe along painted rivers (V3D-02 "Open after r2" 5) is left for the carved-river stage. Its cause is not settled (the colour map's interpolation to white across one cell, or the white water texture blending into a lit bank), and the cheap fixes either change the painted cells' colours, which T3 compares with the 2D map, or tint every river bank blue. Neither is clearly an improvement.

## 12. Invariants

1. The preview opens `world.db` read-only and writes nothing but `--screenshot`'s file; nothing in `worldgen/`, `tools/`, `game/data/`, `extern/`, the schema or a golden file changes.
2. `GroundCells::type`, `painted`, `rock`, `snow`, every placement, the far tier and every near cell's contents are V3D-03's bit for bit, with any look.
3. The seventeen textures are the same bytes in every world and on every platform; ids 0 to 14 are V3D-02's.
4. Every control value has base in 0..14, overlay in 0..16, blend in 0..255, every other bit 0; a lake, sea or painted cell has blend 0; with `--look-v3d03 rock,snow` (or `all`) the control is V3D-02's.
5. `--look-v3d03 all` gives V3D-03's look in every item; `--flat-colours --no-trees --look-v3d03 all` gives V3D-01's.
6. Every snow overlay with blend ≥ 128 lies on a cell with g < 457 and T' < −10 (at `SNOW_T0` −10).

## 13. Determinism, CI, to verify

Determinism: rock variants, snow line, control and textures are V3D-02 15's integer arithmetic (`hash32`, truncating division, floor division written out); `sun_direction` and the shrub's normals and colours use floating point and `std::sin`, `std::cos`, `std::sqrt`, rendering only, compared with tolerances and never hashed. Nothing iterates an unordered container.

CI: no workflow change; `gdext-linux` and `gdext-windows` run U31 to U39 and the extra run of `gdext-terrain-check` (a few seconds). The `build` jobs, `world-hashes.sh` and `compare-gdext-hashes` are unaffected.

| Id | Assumed | If false |
| --- | --- | --- |
| W20 | Terrain3D accepts 17 texture assets and draws overlay ids 15 and 16 (arrays sized by the asset count) | no per-cell rock tone is possible without new ids: default `Look::rock` to false and report |
| W21 | Godot 4.7 mixes `ambient_light_color × energy` and the sky by `ambient_light_sky_contribution` when the source is sky | ambient source 2 (colour) with the same colour and energy 0.9 |
| W22 | an unshaded `StandardMaterial3D` still takes depth fog, so the far plane edge stays the fog colour | none needed: the unshaded colour equals the fog colour |
| W23 | `Basis::looking_at(d, up)` gives a light whose −Z is d | build the basis from d, up × d and d × (up × d) by hand |
| W24 | smooth normals on a multimesh with the shared material light as expected (no per-face shading forced by the material) | flat normals; colour and shape alone (B1 then by eye) |

## 14. Questions for Paul (built as recommended, pending review)

| Id | Question | Recommendation (built) | Reverse with |
| --- | --- | --- | --- |
| Q-V3D-04-1 | Sun from the south-south-west (lights what the default and forest views face; top-down relief may read inverted) or keep the map convention of a north-west sun | south-south-west, 210° at 45° | `SUN_AZIMUTH_DEG`, or `--look-v3d03 light` |
| Q-V3D-04-2 | Snow line at −1 °C to −3 °C over all land with faces past about 50° bare, or V3D-02's −2 °C to −6 °C on alpine flats | the snow line (4.3), tuned to S1 | `SNOW_T0`, or `--look-v3d03 snow` |
| Q-V3D-04-3 | Heath and moor belts under the rock: keep, soften, or drop | keep: they are the 2D map's belts, and they read worst while their slopes are dark (4.5) | none needed |
| Q-V3D-04-4 | Unshaded sea (no band, no glint) or a lit sea with a fog colour matched to it (a band remains wherever the light or view angle changes) | unshaded | `--look-v3d03 sea` |

No `proposals.md` entry: nothing here touches the design or the world file.

## Revision notes

- r1 (2026-11-28): first draft, built on its recommendations while Paul is away; his review decides Q-V3D-04-1 to 4.
