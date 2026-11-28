# V3D-02 Biome ground and vegetation

Status: **r2, as built** (2026-11-27). r1 was approved by Paul on 2026-11-27 with the recommendations of section 17 accepted; r2 records the build and Paul's changes after flying it (revision notes). Second stage of the 3D terrain viewer track (`docs/handoff-3d-terrain-viewer.md`), on top of V3D-01 as built (`docs/specs/v3d-01-terrain-preview.md` r2). Paul flew V3D-01 on 2026-11-27: "looks good, but just basic terrain, no trees, and the only difference between biomes is the ground colour". He chose **generated placeholders** for this stage's art: nothing licensed or vendored; textures and meshes are built in code and can be replaced by real assets later. Design doc section 7 ("Wilderness": vegetation scattered from biome rules and drawn with instancing; "What makes each biome distinct": silhouette and ground shape) and section 1 (greybox art carries the project until each generator is good enough).

**No world change.** The stage reads `world.db` and writes nothing to it or anywhere else in the source tree. `GENERATOR_VERSION`, the schema version, every golden file, `world-hashes.sh` and its cases are unchanged; nothing under `worldgen/`, `tools/worldgen-cli/` or `game/data/` changes. It uses worldgen only through its public headers. Floating point is allowed (nothing reaches a world file), but every buffer the checks hash is integer arithmetic (section 15). Owner: viewer engineer.

**Not in this stage** (later stages, not specified here): real art assets; detail noise and upsampling below 16 m; carved river channels; streaming of regions or of vegetation around the camera; collision; markers; VR; the fly-across test of design section 7; biome scatter sets as data in the biome TOML files (design 7, "the signatures are data").

## 1. Purpose

After this stage each biome has its own ground on screen: a generated texture with its own grain, relief and roughness, coloured so that seen from above it still matches the 2D map. Borders between biomes blend over a few cells instead of a 16 m staircase. Steep slopes show rock and cold peaks show snow, both taken from the world's layers. Rivers are drawn as painted lines that match the 2D map. Placeholder trees, shrubs and reeds stand on the land, built in code and placed by Terrain3D's instancer, deterministically from the world seed and cell coordinates. The sea plane reaches the horizon with no visible edge. Every new part can be switched off, so V3D-01's look and checks stay reachable.

## 2. Parts and inputs

| Area | Files | Contents |
| --- | --- | --- |
| ground (plain C++, no Godot types) | new `gdext/src/terrain/ground.hpp`, `.cpp` | ground types, texture generation, per-cell ground, control map, colour map, river paint (sections 3 to 5) |
| vegetation (plain C++) | new `gdext/src/terrain/vegetation.hpp`, `.cpp` | kinds, densities, hashes, placement, cap (section 6) |
| meshes (plain C++) | new `gdext/src/terrain/placeholder_meshes.hpp`, `.cpp` | vertex data of each kind and LOD (6.2) |
| terrain data | `terrain_data.hpp`, `.cpp` | adds `ground_low` (6.5) |
| scene logic | `terrain_preview.hpp`, `.cpp` | applies the above through ClassDB (section 9), sea (section 7), options and keys (section 10) |
| check | `terrain_check.cpp`, `gdext/cmake/terrain_check.cmake` | T9 to T16 (11.2) |
| unit tests | new `gdext/tests/ground_test.cpp`, `vegetation_test.cpp`, `meshes_test.cpp` | U7 to U16 (11.1) |
| docs | `gdext/README.md`, section "Terrain preview" | options, keys, checks, bench, checklist |

The three new `.cpp` files join the static library `unmapped_terrain_core` (V3D-01 9.1), which the extension and `gdext-terrain-tests` link. They include only standard headers and worldgen's public headers.

Read from the world, all through `WorldFile` (no SQL), in addition to V3D-01's layers:

| Item | Type | Required | Use |
| --- | --- | --- | --- |
| meta `seed` | unsigned decimal (`get_meta_value("seed")`) | no; absent or not a u64: seed 0 and the note `no seed in meta: vegetation placed with seed 0` | placement key (6.4) |
| layer `water` | `u8`, 0 land, 1 river, 2 lake, 3 ocean (P1-02) | no | rivers, exclusions |
| layer `river` | `u8`, class 0 to 15 (P1-02) | no | river paint |
| layer `flow_dir` | `u8`, D8 code, 8 on ocean (P1-02) | no | river paint (diagonals) |
| layer `timber` | `u8`, 0 to 255 (P1-05 R4: biome timber × slope factor) | no; absent: fallback of 6.3 | tree density |
| layer `temperature` | `i16`, 0.1 °C (P1-03) | no; absent: no snow | snow |
| table `biome` | `biome_records()`: id, key, colour | with `biome` | ground type and colour |

Each optional layer follows V3D-01 5.1's rules (size, type, read error: "not used" note, treated as absent).

## 3. Ground textures (item 1)

### 3.1 Ground types

One `Terrain3DTextureAsset` per ground type, ids 0 to 14, always all fifteen whatever the world holds. A biome maps to a type by its record's **key**, not its code (codes come from data). Id 0 is `unknown` on purpose: a control value is a denormal float only when base = 0 and overlay ≤ 1 (bits 23 to 30 zero), so only unknown cells can produce one (W10); no id reaches 15, so no value is a NaN.

| Id | Type | Used for | Colour (sRGB) | P0 | G | S | Rough. byte | uv_scale (1/m) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | unknown | a code with no record, or a key not in this table | 0xFF00FF | 1 | 0 | 0 | 230 | 1/16 |
| 1 | grassland | `grassland` | record (0x96AA5A) | 8 | 50 | 2 | 217 | 1/8 |
| 2 | forest floor | `temperate_forest` | record (0x2E6034) | 6 | 80 | 3 | 230 | 1/8 |
| 3 | carr | `wet_woodland` | record (0x23505E) | 6 | 70 | 2 | 179 | 1/8 |
| 4 | marsh | `marsh` | record (0x6AA396) | 4 | 60 | 1 | 140 | 1/12 |
| 5 | fen | `fen` | record (0x8A8070) | 4 | 60 | 1 | 153 | 1/12 |
| 6 | steppe | `steppe` | record (0xE0D296) | 4 | 40 | 1 | 230 | 1/12 |
| 7 | heath | `heath` | record (0x94664A) | 8 | 80 | 3 | 230 | 1/8 |
| 8 | moor | `highland_moor` | record (0x7E6680) | 6 | 70 | 2 | 217 | 1/10 |
| 9 | scree | `alpine` | record (0xBEBEB4) | 16 | 90 | 4 | 204 | 1/6 |
| 10 | waste | `blighted_waste` | record (0x605446) | 4 | 100 | 3 | 242 | 1/10 |
| 11 | rock | overlay on steep land (4.2) | 0x6E6A64 | 4 | 110 | 6 | 191 | 1/24 |
| 12 | snow | overlay on cold scree (4.2) | 0xF0F2F5 | 2 | 25 | 1 | 115 | 1/16 |
| 13 | water | `lake`; painted river cells (5.2) | 0xFFFFFF (the colour map tints it) | 8 | 20 | 1 | 20 | 1/8 |
| 14 | shore | `ocean` (sea bed; seen only where a coastal triangle rises above the plane, so it reads as a beach) | 0xB8A878 | 16 | 40 | 2 | 217 | 1/6 |

"record (0x…)" is the colour of the `biome_records()` row with that key, or the shipped value in brackets when the world has no such row. P0 is the base lattice period, G the grain depth, S the normal strength.

### 3.2 Texture generation (normative; integer only)

Each type gives two 256 x 256 RGBA8 images, identical in every world (they do not depend on the world; colour comes in through `albedo_color`, 3.3). All arithmetic is on `uint32_t` (wrapping) or `int64_t`; `/` truncates toward zero; `>>` is logical on unsigned values.

```
hash32(a, b, c):  h = a*0x8DA6B343 + b*0xD8163841 + c*0xCB1AB31F      (uint32, wrapping)
                  h ^= h >> 16; h *= 0x7FEB352D; h ^= h >> 15; h *= 0x846CA68B; h ^= h >> 16
smooth(f):        (f*f*(768 - 2f)) >> 16                               (f in 0..255 -> 0..255)

for octave o in 0..3:  P = P0 << o                                       (P <= 128)
  for pixel (i, j), i the column, j the row, both in 0..255:
    ux = i*P; xl = ux >> 8; fx = ux & 255;  uy = j*P; yl = uy >> 8; fy = uy & 255
    x1 = (xl + 1) % P; y1 = (yl + 1) % P
    v(a, b) = hash32(a, b, 16*id + o) >> 24
    sx = smooth(fx); sy = smooth(fy)
    top = v(xl, yl)*(256 - sx) + v(x1, yl)*sx;  bot = v(xl, y1)*(256 - sx) + v(x1, y1)*sx
    q_o = (top*(256 - sy) + bot*sy) >> 8                                 (0..65280)
n(i, j) = (8*q_0 + 4*q_1 + 2*q_2 + q_3 + 1920) / 3840                    (0..255; the height)
```

The noise tiles: pixel 256 maps to lattice P ≡ 0. Then, with indices taken mod 256:

```
albedo(i, j) = (g, g, g, n)          g = 255 - (G*(255 - n) + 127) / 255
gx = n(i+1, j) - n(i-1, j);  gy = n(i, j+1) - n(i, j-1)
X = -S*gx;  Y = S*gy;  Z = 512;  L = isqrt(X*X + Y*Y + Z*Z)              (floor square root, int64)
normal(i, j) = (128 + 127*X/L, 128 + 127*Y/L, 128 + 127*Z/L, roughness byte)
```

Terrain3D's packing (verified in source): albedo alpha is height, used for height blending (`main.glsl` `accumulate_material`, `alb.a`); normal alpha is roughness (`nrm.a`, plus the asset's `roughness` modifier, left 0); every texture must have the same size, format and mipmap setting or Terrain3D refuses the set (`Terrain3DAssets::_update_texture_files`). Both images become `Image::create_from_data(256, 256, false, FORMAT_RGBA8, ...)`, then `generate_mipmaps()`, then `ImageTexture::create_from_image`. The green channel is +Y (OpenGL, W5).

### 3.3 Colour

The shader computes `albedo = texel.rgb (sRGB-decoded) × albedo_color.rgb × colour map.rgb (sRGB-decoded)` (`main.glsl:338`, `pbr_views.glsl:58`); `_texture_color_array` is a plain `vec4` uniform, so `albedo_color` is used as given, in linear terms. So that a type seen from afar (its mipmaps averaged) shows its sRGB colour c:

```
gbar = (sum of g over the 65,536 texels) / 65536                        (double)
m    = lin(gbar / 255)
albedo_color = (lin(c.r/255)/m, lin(c.g/255)/m, lin(c.b/255)/m, 1)     (channels may exceed 1; Terrain3D does not clamp)
lin(u) = u <= 0.04045 ? u/12.92 : ((u + 0.055)/1.055)^2.4
```

As built: Godot converts a `Color` given to `albedo_color` from sRGB to linear on its way to the shader, so the scene sets `srgb_encode` of each channel above (`ground.hpp`, the inverse of `lin`), and the shader receives the linear value. T9 compares the encoded value.

The colour map stays: on land it is neutral white, and it carries the 2D colours of lakes and rivers (5.1). With `--flat-colours` everything is V3D-01's (one white texture, control 0, biome colours in the colour map).

Other asset properties: `uv_scale` from 3.1 (the shader's UV is world metres × uv_scale, W4); `detiling_rotation` 0.2 and `detiling_shift` 0.2 for ids 1 to 12 and 14, 0 for 0 and 13; `normal_depth`, `ao_strength`, `roughness` left at their defaults. Material features stay at their defaults, all off: auto shader (it would override the control map), macro variation (it would shift the colours), dual scaling (a later stage).

## 4. Control map (item 2)

### 4.1 Per-cell ground

For each cell, from the layers (integer only; window cells outside the map are skipped; slope neighbours are clamped to [0, N − 1]):

```
type(c)   = 3.1's id by the key of biome(c)'s record (0 if none);   absent biome layer: --flat-colours forced (note 10.3)
painted(c)= the river paint class of 5.2 (0 = not painted)
land(c)   = type(c) in 0..10                                          (water 13 and shore 14 are not land)
E         = elevation, int32 decimetres
g2        = (E(x+1,y) - E(x-1,y))^2 + (E(x,y+1) - E(x,y-1))^2          (int64)
g         = isqrt(g2)                                                 (tan of the slope = g / 320)
rock(c)   = g <= 224 ? 0 : g >= 320 ? 255 : ((g - 224)*255 + 48) / 96 (35 degrees to 45 degrees)
snow(c)   = type(c) != 9 or no temperature layer ? 0 :
            T >= -20 ? 0 : T <= -60 ? 255 : ((-20 - T)*255 + 20) / 40 (T in 0.1 C: -2 C to -6 C)
```

Rock and snow are 0 on cells that are not land. The rock band starts at 35°, where P1-05 says ground stops being walkable; the snow band lies inside the alpine biome, which the world already marks as above the cold limit.

### 4.2 Base, overlay and blend

For a land cell, count the land types of the 5 x 5 window around it (itself included; water and shore cells are not counted). Let b be the count of type(c), and o the largest count of any other land type, ov that type (ties to the smaller id); o = 0 if there is none.

```
border = o == 0 ? 0 : min(127, (255*o + (b + o)/2) / (b + o))     (capped: a vertex always shows mostly its own biome)
candidates, in this order: (11, rock(c)), (12, snow(c)), (ov, border)
pick the candidate with the largest blend; ties go to the earlier one
base    = painted(c) ? 13 : type(c)
overlay, blend = painted(c) or not land(c) or the pick's blend is 0 ? (base, 0) : the pick
control(c) = (base << 27) | (overlay << 22) | (blend << 14)       (uint32; every other bit 0)
```

The encoding is Terrain3D's (`terrain_3d_util.h`: `enc_base`, `enc_overlay`, `enc_blend`; UV rotation, UV scale, hole, navigation and autoshader bits 0). The `FORMAT_RF` control image holds these bit patterns, copied with `memcpy` (never through float arithmetic). Terrain3D's shader already interpolates texture ids across the four vertices of a cell; the overlay widens the transition to about two cells on each side of a border, breaking the 16 m staircase. Lake cells (type 13) and sea cells (type 14) never get an overlay.

## 5. Colour map and rivers (item 4)

### 5.1 Colour map (default mode)

```
colour(c) = painted(c) > 0 :  river_rgb(painted(c)), alpha 128
            type(c) = 13   :  the `lake` record's colour (else 0x4080CC), alpha 128
            otherwise      :  (255, 255, 255), alpha 128
river_rgb(k) = k <= 15 ? worldgen::inspect::composite_river_colour(k) : (52, 100, 200)
```

### 5.2 River paint

Exactly the composite map's step 4 (P1-00 10.2, documented at `composite_river_colour` and `render_composite_map` in `worldgen/inspect/render.hpp`), so 3D and 2D rivers lie on the same cells:

- Needs `water`. Without `river`, every cell with water 1 is painted with class 16 (the colour (52, 100, 200)).
- With `river`: scan cells with water 1 in index order; for (x, y) of class k ≥ 1 add (x, y, k); if `flow_dir` is present and its code d is odd and ≤ 7, add (x + dx(d), y, k) (D8: 0 E, 1 SE, 2 S, 3 SW, 4 W, 5 NW, 6 N, 7 NE); if k ≥ 4, add (x + 1, y, k) then (x, y + 1, k).
- Paint the entries in ascending k, equal k in list order (later overwrites), each only if its cell is inside the map and has water 0 or 1.

A painted cell is a 16 m band of water texture tinted by its class colour; the river is not carved and not wider than the 2D map shows it. Lakes stay as V3D-01 (flat at their surface), now with the water texture.

## 6. Vegetation (item 3)

### 6.1 Kinds and mesh assets

| Mesh id | Kind | LODs | lod0_range (m) | lod1_range (m) | Scale range |
| --- | --- | --- | --- | --- | --- |
| 0 | broadleaf | 2 | 600 | 6000 | 0.70 to 1.30 |
| 1 | conifer | 2 | 600 | 6000 | 0.70 to 1.40 |
| 2 | shrub | 1 | 1000 | | 0.60 to 1.40 |
| 3 | reed clump | 1 | 800 | | 0.70 to 1.30 |
| 4 | snag (dead tree) | 1 | 3000 | | 0.80 to 1.20 |

Every mesh asset: `cast_shadows` 0 (Off; sun shadows are off, Q-V3D-01-4), `height_offset` 0, `fade_margin` 0, `visibility_layers` 1. Terrain3D's instancer groups instances in cells of 32 vertices (512 m, `Terrain3DInstancer::CELL_SIZE`), one multimesh instance per cell and LOD, culled by Godot's visibility range from these LOD ranges (`_set_mmi_lod_ranges`). Trees are visible to 6 km so that the default start view (about 300 m above the highest peak) shows forests.

### 6.2 Meshes (normative)

Built by `build_mesh(kind, lod)` as flat-shaded triangles (three vertices each, normal = the outward face normal, clockwise seen from outside: Godot's front face, W7), with one sRGB vertex colour per primitive. Origin at the ground point, +Y up, metres. `cone(a, b, ra, rb, sides, cap)` is a frustum from a to b (one triangle per side when rb = 0, two otherwise; cap: a fan closing the a end); its ring starts on u and turns toward v, with w = normalize(b − a), u = normalize(w × Z) if |w·Z| < 0.9 else normalize(w × X), v = w × u, and point i at angle 2πi/sides. `ellipsoid(c, rx, ry, rz, sides, rings)` has latitudes −90° + 180°k/rings and gives sides·(2·rings − 2) triangles.

| Kind, LOD | Primitives | Colours | Triangles |
| --- | --- | --- | --- |
| broadleaf 0 | cone((0,−2,0), (0,6,0), 0.35, 0.25, 6, no); ellipsoid((0,8.5,0), 3.5, 3.0, 3.5, 8, 6) | trunk 0x5A4632, crown 0x3F6A2E | 92 |
| broadleaf 1 | cone(same, 3 sides); ellipsoid(same, 4, 2) | same | 14 |
| conifer 0 | cone((0,−2,0), (0,4,0), 0.30, 0.20, 6, no); cones (y 2 → 9, r 3.0), (6 → 12, 2.3), (10 → 15.5, 1.5), rb 0, 8 sides, cap | trunk 0x5A4632, foliage 0x2B4A2C | 60 |
| conifer 1 | cone((0,1.5,0), (0,15.5,0), 2.8, 0, 4, cap) | foliage | 8 |
| shrub 0 | ellipsoid((0,0.5,0), 1.2, 0.9, 1.2, 6, 4) | 0x55663A | 36 |
| reed 0 | cones (0,−0.5,0) → (0,2.2,0); (0.4,−0.5,0.2) → (0.5,1.9,0.25); (−0.3,−0.5,0.35) → (−0.4,2.0,0.45); 0.12, 0, 4, no | 0x8F8A52 | 12 |
| snag 0 | cone((0,−2,0), (0,7,0), 0.30, 0.08, 5, no); cone((0,4,0), (1.8,6,0), 0.10, 0.02, 4, no); cone((0,5.2,0), (−1.4,6.8,0.3), 0.09, 0.02, 4, no) | 0x4E453C | 26 |

Trunks reach 2 m below the origin, so a tree on a slope never shows a gap under it (6.5). The scene turns each into an `ArrayMesh` (one surface: vertices, normals, colours) with one shared `StandardMaterial3D`: `vertex_color_use_as_albedo` true, `vertex_color_is_srgb` true, roughness 0.9, cull back.

### 6.3 Densities

Per land type. Trees come from the `timber` layer t (0 to 255) at the cell, or the fallback t when the layer is absent (round(255 × the shipped biome timber of P1-04 4.1)). Expected counts are Q8 per cell (256 = one instance per 16 m cell = 39.06 per hectare).

```
eT       = (tree_q8*t + 127) / 255                       (tree_q8 393 = 60 trees per hectare at t = 255; 983 = 150)
conifer% = min(100, c0 + max(0, E - 3000) / 100)         (E in dm: +1 point per 10 m above 300 m)
eConifer = (eT*conifer% + 50) / 100;   eBroadleaf = eT - eConifer
```

| Type | Fallback t | c0 (%) | tree_q8 (per ha at t = 255) | Shrub Q8 (per ha) | Reed Q8 (per ha) | Snag Q8 (per ha) |
| --- | --- | --- | --- | --- | --- | --- |
| 1 grassland | 38 | 20 | 393 (60) | 52 (8) | 0 | 0 |
| 2 forest floor | 255 | 30 | 983 (150) | 721 (110) | 0 | 0 |
| 3 carr | 179 | 10 | 983 (150) | 753 (115) | 66 (10) | 0 |
| 4 marsh | 77 | 0 | 393 (60) | 66 (10) | 393 (60) | 0 |
| 5 fen | 51 | 10 | 393 (60) | 66 (10) | 262 (40) | 0 |
| 6 steppe | 13 | 40 | 393 (60) | 98 (15) | 0 | 0 |
| 7 heath | 13 | 80 | 393 (60) | 262 (40) | 0 | 0 |
| 8 moor | 26 | 70 | 393 (60) | 164 (25) | 0 | 0 |
| 9 scree | 0 | 100 | 393 (60) | 26 (4) | 0 | 0 |
| 10 waste | 0 | 0 | 393 (60) | 0 | 0 | 52 (8) |
| 0 unknown | 0 | 0 | 393 (60) | 0 | 0 | 0 |

r2 (Paul, 2026-11-27, after flying r1): the two forest types carry 150 trees per hectare (`tree_q8` 983, was the fixed 393) and 100 shrubs per hectare of undergrowth (655) on top of their r1 shrubs (66 and 98, now 721 and 753); the shrub range stays 1000 m (6.1). Every other value is r1's. The table is `VEGETATION_DENSITY` in `vegetation.cpp`.

The tree line is the alpine biome (timber 0 there). The design's later rule, scatter sets named in each biome's TOML file, replaces this table (Q-V3D-02-6).

### 6.4 Placement (normative)

```
mix64(z): z += 0x9E3779B97F4A7C15; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9;
          z = (z ^ (z >> 27)) * 0x94D049BB133111EB; return z ^ (z >> 31)          (uint64, wrapping; SplitMix64)
key0     = mix64(seed ^ 0x7633642D73636174)
hb(x, y) = mix64(key0 ^ ((uint64(x) << 32) | uint64(y)))
```

Cells in row-major order (y, then x); in each cell the kinds 0 to 4 in order. A cell gets nothing if it is not a land type 1 to 10, or painted(c) > 0, or rock(c) ≥ 128, or snow(c) ≥ 128, or water(c) ≠ 0, or E ≤ 0. Otherwise, for kind j with expected count e (6.3):

```
hc = mix64(hb + 64*j);  count = (e >> 8) + ((hc & 0xFF) < (e & 0xFF) ? 1 : 0)      (count <= 62)
for k in 0..count-1:
  h   = mix64(hb + 64*j + 1 + k)
  jx  = cell_jitter(mix64(h ^ 0x6A09E667F3BCC909));  jz = cell_jitter(mix64(h ^ 0xBB67AE8584CAA73B))
  cell_jitter(u) = float(2*(u >> 54) + 1) / 128 - 8      (metres: 1024 steps of 1/64 m over [-7.992, 7.992])
  X   = 16*(x - N/2) + jx;  Z = 16*(y - N/2) + jz;  drop the instance if X or Z is outside [-8N, 8N - 16]
  yaw = (h >> 20) & 0x3FF                          (yaw in degrees = yaw * 360 / 1024)
  sc  = smin + ((smax - smin)*((h >> 30) & 0x3FF) + 511) / 1023      (per mille, from 6.1)
  tint= (h >> 40) & 0xFF
  thin= uint32(mix64(h ^ 0xD1B54A32D192ED03) >> 32)
  Y   = ground_low(X, Z)                           (6.5)
```

Jitter covers the whole cell, strictly less than 8 m from the cell's vertex on each axis, so an instance's nearest vertex is its own cell and it keeps more than 8 m from a painted river vertex. r2 (Paul, 2026-11-27): r1's jitter (`3*(h & 0x3FF)/256 - 6` and the next 10 bits, ±6 m) left trees standing in rows 16 m apart; each axis now has its own keyed draw. Yaw, scale, tint, `thin` and the edge-drop rule are unchanged; bits 0 to 19 of h are no longer used.

**Cap.** Let S be the number of candidates and B the cap (default 2,000,000, `DEFAULT_INSTANCE_CAP`, r2; r1 1,000,000; `--instance-cap`). If S > B, keep exactly the B candidates with the smallest key (thin << 32) | p, where p is the candidate's index in placement order (S < 2^32); otherwise keep all. Survivors keep placement order. Whole-map placement at load is the choice: streaming is a later stage, and a uniform thinning keeps every biome's proportions.

### 6.5 Ground under an instance

`ground_low(layout, heights, X, Z)` (new in `terrain_data`): with i, j, fu, fv as `sample_bilinear` (V3D-01 6), the smaller of the two triangulations of the cell at (fu, fv):

```
diag 00-11: fu >= fv ? h00 + fu(h10 - h00) + fv(h11 - h10) : h00 + fv(h01 - h00) + fu(h11 - h01)
diag 10-01: fu + fv <= 1 ? h00 + fu(h10 - h00) + fv(h01 - h00) : h11 + (1 - fu)(h01 - h11) + (1 - fv)(h10 - h11)
```

Terrain3D draws one of the two (W9), so the origin is never above the drawn surface, and the 2 m of buried trunk covers the gap.

### 6.6 Expected counts

Measured (r2; seed 42, Linux, RTX 4090, with the r2 densities and cap, before the r2 jitter change, which moves instances but changes no count except at the map's edge):

| N | candidates S | kept (cap 2,000,000) | transfer | first frame | static memory |
| --- | --- | --- | --- | --- | --- |
| 512 | 64,385 (r1 densities; not re-measured) | all | | | |
| 1024 | 1,088,122 | all | 916 ms | 319 ms | 624 MB |
| 2048 | 5,523,211 | exactly 2,000,000 (36%; about 54 trees per hectare in forests) | 1,318 ms | 537 ms | 1,175 MB |

Flight frames: mean 0.7 ms, worst under 5 ms. Memory is about 0.58 KB per instance (Terrain3D's region dictionaries and the multimeshes), about twice r1's estimate of 0.3 KB. Geography-only worlds have no `blighted_waste`, so no snags.

## 7. Sea (item 5)

The V3D-01 plane's edge shows as a change of tone and the plane does not fill the view from high altitude. Choice: **a wider plane faded by depth fog to the sky's ground colour.**

| Setting | Value |
| --- | --- |
| `PlaneMesh` side, centre | 800,000 m (D = 400 km each way), origin, Y = 0 |
| Camera far | 450,000 m (near stays 1.0 m) |
| `Environment` fog | `fog_enabled` true, `fog_mode` 1 (depth), `fog_light_color` the sea colour, `fog_light_energy` 1.0, `fog_density` 1.0, `fog_sun_scatter` 0, `fog_aerial_perspective` 0, `fog_sky_affect` 0, `fog_depth_begin` 200,000, `fog_depth_end` 400,000, `fog_depth_curve` 1.0 |
| `ProceduralSkyMaterial` | `ground_bottom_color` and `ground_horizon_color` the sea colour (as V3D-01 r2) |

At the plane's edge the fog is complete, so the plane and the sky below the horizon have the same colour (W8). No terrain is ever 200 km from the camera (the camera stays within 16N of the origin and below 30 km: at most 75 km), so fog never touches the land. Applies in every mode, `--flat-colours` included.

## 8. Plain C++ contracts (item 7)

Namespace `unmapped::terrain`. No Godot types.

| Name | Contract |
| --- | --- |
| `enum class GroundType : std::uint8_t` | the ids of 3.1 |
| `struct GroundTypeInfo { const char* key; std::uint32_t fallback_rgb; int p0, grain, normal_strength; std::uint8_t roughness; float uv_scale; }`; `GROUND_TYPES[15]` | 3.1 |
| `GroundType ground_type_of_key(std::string_view key)` | 3.1; unknown key gives `unknown` |
| `struct GroundTexture { std::vector<std::uint8_t> albedo, normal; double mean_grey; }`; `GroundTexture build_ground_texture(GroundType)` | 3.2: 4 · 256² bytes each, row-major, RGBA |
| `std::array<float, 3> albedo_colour(GroundType, const GroundTexture&, std::span<const BiomeRecord>)` | 3.3 |
| `struct GroundInputs { const Layout* layout; spans elevation, biome, water, river, flow_dir, temperature; std::span<const BiomeRecord> records; }` | spans empty when absent; throws `std::invalid_argument` on a size mismatch |
| `struct GroundCells { std::vector<std::uint8_t> type, painted, rock, snow; }`; `GroundCells build_ground_cells(const GroundInputs&)` | 4.1, 5.2 |
| `std::vector<std::uint8_t> river_paint(const Layout&, water, river, flow_dir)` | 5.2: class per cell, 0 none, 16 no class |
| `std::vector<std::uint32_t> build_control(const Layout&, const GroundCells&)` | 4.2 |
| `std::vector<std::uint8_t> build_textured_colours(const Layout&, const GroundCells&, records)` | 5.1, 4n² bytes |
| `std::uint64_t mix64(std::uint64_t)`; `std::uint64_t scatter_key(std::uint64_t seed)` | 6.4 |
| `struct Instance { float x, y, z; std::uint16_t yaw1024, scale_pm; std::uint8_t tint; }` | 6.4 |
| `struct Placement { std::array<std::vector<Instance>, 5> kinds; std::uint64_t candidates; std::uint64_t cap; }` | kept instances per mesh id, placement order |
| `Placement place_vegetation(const Layout&, std::span<const float> heights, std::span<const std::int16_t> elevation, std::span<const std::uint8_t> timber, std::span<const std::uint8_t> water, const GroundCells&, std::uint64_t seed, std::uint64_t cap)` | 6.3, 6.4 |
| `float ground_low(const Layout&, std::span<const float> heights, float X, float Z)` | 6.5 |
| `struct MeshData { std::vector<float> positions, normals; std::vector<std::uint32_t> colours; }` (colours 0xRRGGBBAA, three vertices per triangle); `MeshData build_mesh(int kind, int lod)`; `int lod_count(int kind)` | 6.2 |

## 9. Scene side (item 7)

### 9.1 Order in `open_world` (additions to V3D-01 7)

1. Read the extra layers and the seed (section 2). Build `GroundCells`; in default mode the control and colour buffers of 4.2 and 5.1 replace V3D-01's zeros and biome colours (`--flat-colours`: V3D-01's).
2. Unless `--no-trees` or no `biome` layer: `place_vegetation` with the cap.
3. Textures and meshes (once per process; they do not depend on the world): the fifteen `GroundTexture`s and their `ImageTexture`s, and for each kind a `PackedScene` built in code: a `Node3D` root with one `MeshInstance3D` child per LOD named `MeshLOD0`, `MeshLOD1`, owner the root, the `ArrayMesh` of `build_mesh` with the shared material on its surface, then `PackedScene::pack(root)` (W1).
4. Before `add_child` (V3D-01 r2's order): texture assets 0 to 14 (`set_texture_asset`), or the one white asset with `--flat-colours`; mesh assets 0 to 4 (`set_mesh_asset`) unless vegetation is off. On each mesh asset set `scene_file` **first**, since `set_scene_file` resets name, height offset, density and LOD ranges (`Terrain3DMeshAsset::set_scene_file`), then the rest of 6.1.
5. After `import_images`: for each kind with instances and each instancer cell (32 x 32 vertices, 512 m) holding instances of it, in cell order and placement order within the cell, one `get_instancer().add_transforms(id, transforms, colours, false)` (as built: Terrain3D copies a cell's colour array for every instance appended, so one call per kind is quadratic; the stored instances are the same), with `Transform3D(Basis(Vector3(0, 1, 0), yaw).scaled(Vector3(s, s, s)), Vector3(X, Y, Z))`, s = sc / 1000, yaw = yaw1024 · 2π / 1024, colour (v, v, v, 1) with v = (200 + tint·55/255) / 255 (integer division in the numerator); then once `update_mmis(-1, Vector2i(0, 0), true)` (rebuild everything; Terrain3D builds the multimeshes on the next `frame_pre_draw`, W3).
6. Sea and fog (section 7).

### 9.2 Every Terrain3D name called (a wrong name fails only at run time; T9 to T12 cover each)

| Class | Name | Kind | Source |
| --- | --- | --- | --- |
| `Terrain3D` | `assets`, `get_assets`, `get_instancer`, `get_data` (and V3D-01's) | property, methods | `terrain_3d.cpp:1396-1399` |
| `Terrain3DAssets` | `set_texture_asset(id, texture)`, `get_texture_asset(id)`, `get_texture_count`, `set_mesh_asset(id, mesh)`, `get_mesh_asset(id)`, `get_mesh_count` | methods | `terrain_3d_assets.cpp:734-755` |
| `Terrain3DTextureAsset` | `name`, `albedo_color`, `albedo_texture`, `normal_texture`, `uv_scale`, `detiling_rotation`, `detiling_shift` | properties | `terrain_3d_texture_asset.cpp:262-276` |
| `Terrain3DMeshAsset` | `scene_file`, `name`, `enabled`, `height_offset`, `cast_shadows`, `visibility_layers`, `lod0_range`, `lod1_range`, `fade_margin` | properties | `terrain_3d_mesh_asset.cpp:669-700` |
| `Terrain3DMeshAsset` | `get_lod_count`, `get_lod_range(lod)` | methods | `terrain_3d_mesh_asset.cpp:634, 643` |
| `Terrain3DInstancer` | `add_transforms(mesh_id, transforms, colors, update)`, `update_mmis(mesh_id, region_location, rebuild_all)` | methods | `terrain_3d_instancer.cpp:1382, 1387` |
| `Terrain3DData` | `get_control(global_position)`, `get_region(region_location)`, `get_region_locations` | methods | `terrain_3d_data.cpp:1460, 1419` |
| `Terrain3DRegion` | `get_instances` (Dictionary mesh id → Dictionary cell → [transforms, colours, modified]) | method | `terrain_3d_region.cpp:526`; `terrain_3d_instancer.h` |

## 10. Options, keys, HUD (item 6)

### 10.1 Options after `--` (added to V3D-01 4)

| Option | Meaning |
| --- | --- |
| `--flat-colours` | V3D-01's ground: one white texture, control 0, biome colours, no river paint |
| `--no-trees` | no mesh assets and no placement (as built: Terrain3D then inserts its own default mesh into the empty mesh list; T11) |
| `--instance-cap <n>` | the cap B of 6.4, 0 to 4,000,000 (default 2,000,000, r2) |
| `--above <m>` | start altitude `ground_clamp` + m at the start cell, instead of the default or `--alt` (for screenshots) |

`--flat-colours --no-trees` is V3D-01's look, with the sea of section 7. Refusals: `--instance-cap <v>: not a whole number from 0 to 4000000`, `--above <v>: not a number`, `--above cannot be combined with --alt`. `--screenshot` now saves 2.0 s after the **first drawn frame** after the terrain is added (the instancer builds its multimeshes in that frame).

### 10.2 Keys

| Key | Action |
| --- | --- |
| T | vegetation on or off: `enabled` on every mesh asset (instances are kept); nothing with `--no-trees` |
| G | ground textured or flat: reopens the world in the other mode, keeping the camera's `FlyState`; nothing when there is no `biome` layer |

The key line becomes `WASD move, E/Q up/down, right mouse look, wheel speed, Shift fast, R start, T trees, G ground, H hide`. The HUD's first line is unchanged. As built, headless only: key T logs Terrain3D thumbnail errors (`_viewport_texture is null`); the check still passes.

### 10.3 Status lines (added)

`ground: textured, 15 ground types` or `ground: flat colours`; `no biome layer: ground drawn grey, no vegetation` (replaces V3D-01's "terrain drawn grey" line); `vegetation: <n> instances` with `, thinned from <S> to the cap` when S > B; `vegetation: off`; the seed note of section 2; `no water layer: rivers not drawn`; `no timber layer: tree density from the biome alone`; `no temperature layer: no snow`.

## 11. Testing (item 8)

### 11.1 Unit tests (`gdext-terrain-tests`, no Godot)

| Id | Test |
| --- | --- |
| U7 | `ground_type_of_key` for the twelve shipped keys and `"other"`; the table of 3.1 (fifteen rows, ids 0 to 14, P0 · 8 ≤ 128) |
| U8 | `build_ground_texture`: sizes; the test's own copy of 3.2 gives the same bytes at (0, 0), (255, 255), (17, 93) and (128, 1) for every type; the noise tiles (n(256 + i, j) = n(i, j) through the formula); id 0 is flat (normal (128, 128, 255)); FNV-1a 64 of each albedo and normal buffer equals the value recorded in the test (the same constants must pass on GCC, Clang and MSVC) |
| U9 | `hash32` and `mix64` vectors: `mix64(0)` = 0xE220A8397B1DCDAF (the first SplitMix64 output of seed 0) and three recorded values |
| U10 | `build_control` on hand-made 16 x 16 grids: a straight border (blends 0, 51, 102 by distance), a lone cell (blend capped at 127), rock at g = 224, 272, 320 (0, 128, 255), snow at T = −20, −40, −60 (0, 128, 255), the candidate order on ties, painted and lake cells with blend 0, bits exactly `(base << 27) | (overlay << 22) | (blend << 14)`, no value with base 15 or more |
| U11 | `river_paint` against the 2D rule: a diagonal step closes its corner, class 4 widens east and south, a lake cell is never painted, without `river` the class is 16 |
| U12 | `build_textured_colours`: land white, lake colour, river class colours, alpha 128 |
| U13 | `place_vegetation`: the same inputs twice give identical output; another seed differs; no instance on water, painted, rock ≥ 128, snow ≥ 128 or non-land cells; each instance less than 8 m from its cell's vertex on each axis and inside the map; on a 512 world of one land type and t = 255 the count per kind, over the interior cells (edge cells lose instances jittered off the map), is within 4 standard deviations of the expected count for every row of 6.3. r2 adds: the values of 6.3's table (`tree_q8` 983 on forest types, 393 otherwise; shrubs 721 and 753), `DEFAULT_INSTANCE_CAP` 2,000,000 and the shrub `lod0_range` 1000; the in-cell offsets (dx, dz) fill a 4 x 4 histogram evenly (each bin within 2% of 1/16), with no correlation (\|r\| < 0.01) between dx and dz, between neighbouring cells or between slots of one cell |
| U14 | the cap: S > B keeps exactly B, and the kept set equals a brute-force sort of the keys; S ≤ B keeps all; B = 0 keeps none |
| U15 | `ground_low` equals the height at a vertex and is ≤ `sample_bilinear` and ≤ both triangulations at 1,000 fixed points |
| U16 | `build_mesh`: triangle counts of 6.2; unit normals; each triangle's normal points away from its primitive's axis or centre; bounds (broadleaf top 11.5 m, conifer 15.5 m) |

### 11.2 Terrain check (`gdext-terrain-check`; additions)

`terrain_check.cmake` also renders `render --composite map` as `map.png` for `w512` and `w1024`, and runs: `w512` default (`present`); `w512 --flat-colours --no-trees` (V3D-01's T1 to T8 unchanged, one texture asset, no mesh asset); `w512 --no-terrain3d` (T7); `w1024 --instance-cap 50000`; `elev/` (no biome: flat forced, no vegetation, the note); `pattern/` (T5). The check keeps its own copy of the formulas of 4 and 5 and of the cap rule.

| Id | Check |
| --- | --- |
| T3 (default mode) | colour image equals the check's own 5.1 at every cell; every painted cell equals `map.png` at that cell |
| T9 | `present`: 15 texture assets; for each, albedo and normal images 256 x 256 RGBA8 with mipmaps, level 0 equal to `build_ground_texture`; `albedo_color` equal to `srgb_encode` of the check's 3.3 within 1e-6; `uv_scale` as 3.1 |
| T10 | control image equal to the check's own 4.2 at every cell; `get_control` at the 32 points of V3D-01 T6 equals it bit for bit |
| T11 | `present`: 5 mesh assets, `get_lod_count` 2, 2, 1, 1, 1, the LOD ranges of 6.1, `cast_shadows` 0; without vegetation, no mesh asset of ours (Terrain3D's own default mesh may be the one entry) |
| T12 | `present`: over `get_region_locations`, the transforms of `get_region(l).get_instances()[id]` total the placement's count per kind; the first and last instance of each kind are found at their position within 0.01 m (region-local, W3) |
| T13 | placement: no instance on a cell excluded by 6.4 (the check's own exclusion rule), each less than 8 m from its vertex on each axis; on `w1024 --instance-cap 50000` the kept count is min(S, 50000) and equals the check's own cap rule |
| T14 | sea: plane side 800,000, camera far 450,000, the fog settings of section 7 |
| T15 | status lines of 10.3 for each run |
| T16 | key G reopens in the other mode with the camera unchanged and one Terrain3D node; key T flips `enabled` on all mesh assets |

### 11.3 By hand on a GPU (Paul)

After V3D-01 9.3's build and `generate` commands (`B`, `GODOT`, `OUT` and the `seed42-<N>.db` worlds as there):

```
P="$GODOT --path $B/game res://viewer3d/terrain_preview.tscn --"
$P $OUT/seed42-1024.db
$P $OUT/seed42-1024.db --at 512 512 --alt 22000 --pitch -89 --screenshot $OUT/v3d02-top.png
$P $OUT/seed42-1024.db --above 250 --pitch -15 --screenshot $OUT/v3d02-low.png
$P $OUT/seed42-1024.db --alt 30000 --pitch -10 --screenshot $OUT/v3d02-horizon.png
$P $OUT/seed42-1024.db --flat-colours --no-trees --screenshot $OUT/v3d02-as-v3d01.png
$P $OUT/seed42-2048.db --bench
"$B/tools/worldgen-cli/worldgen-cli" render --db $OUT/seed42-1024.db --composite map --out $OUT/seed42-1024.map.png
```

| Id | Look for |
| --- | --- |
| G8 | each biome has its own grain, relief and sheen at 250 m above ground; from 22 km the top-down view matches `seed42-1024.biomes.png` in colour and layout |
| G9 | biome borders blend over two or three cells; no 16 m staircase at 250 m above ground |
| G10 | rock on mountain sides and not on gentle ground; snow only on the highest alpine ground |
| G11 | forests dense, grassland scattered, heath and moor with shrubs, reeds in marsh and fen; nothing in lakes, rivers or the sea or on rock; no floating tree, none sunk to its crown |
| G12 | rivers as blue lines in the same places as in `seed42-1024.map.png` |
| G13 | at 30 km the sea reaches the horizon with no edge and no change of tone; no flicker where low coast meets the sea (V3D-01 G4 again, with the far plane at 450 km) |
| G14 | T and G work; `v3d02-as-v3d01.png` looks like V3D-01 |
| G15 | at 2048, flying at 200 m/s 150 m above a forest: no stall after the first frame; the bench lines recorded in `gdext/README.md` |

## 12. Performance (measurements only, no bounds)

`--bench` adds, after V3D-01's first two lines (headless prints these three lines only):

```
UNMAPPED-TERRAIN-BENCH: ground textures <ms> ms, cells <ms> ms, control <ms> ms; vegetation placement <ms> ms, transfer <ms> ms, instances <n> of <S> (cap <B>)
UNMAPPED-TERRAIN-BENCH: view at cell (<x>, <y>), 150 m above ground: frames 300, mean <ms>, max <ms>, over 16.7 ms <a>
```

`transfer` is the `add_transforms` calls and `update_mmis`; the multimesh build falls in `first frame`. The view line is taken after 60 warm-up frames at cell (N/2, N/2), `ground_clamp` + 150 m, heading 0, pitch −10, vsync off; then V3D-01's flight line follows. The engineer records 512, 1024 and 2048 in `gdext/README.md` with static memory before and after vegetation (`--no-trees` run).

## 13. CI (item 9)

No workflow change. `gdext-linux` and `gdext-windows` run the new unit tests in `gdext-terrain-tests` and the longer `gdext-terrain-check` (about 25 s expected, timeout 300 s). The `build` jobs, `world-hashes.sh` and `compare-gdext-hashes` are unaffected, since worldgen is untouched. U8's and U9's recorded constants passing on both jobs prove the texture and hash code is the same under GCC and MSVC.

## 14. Invariants

1. The preview opens `world.db` read-only and writes nothing but `--screenshot`'s file; nothing in `worldgen/`, `tools/`, `game/data/`, the schema or a golden file changes.
2. The fifteen textures are the same bytes in every world, on every platform.
3. Every control value has base and overlay in 0..14, blend in 0..255, every other bit 0; a lake, sea or painted cell has blend 0; a land cell's border blend is at most 127.
4. Every painted cell is exactly a cell the 2D composite map paints as river, in the same colour.
5. For a given world file, cap and option set, the placement is identical on every run and platform; no instance stands on a cell excluded by 6.4, outside the map, or 8 m or more from its cell's vertex on either axis.
6. The kept count is min(S, B).
7. With `--flat-colours --no-trees` the height, control and colour buffers are V3D-01's.

## 15. Determinism notes

Textures, ground cells, control, colours, river paint, counts, jitter, yaw, scale, tint and the cap use unsigned wrapping or `int64` integer arithmetic with truncating division, so they are exact everywhere. X and Z are exact in float (r2: odd multiples of 1/128 below 2^15 need 22 bits). Y comes from `ground_low` arithmetic on floats with `-ffp-contract=off` and `/fp:precise` (the project's flags apply to `unmapped_terrain_core`), so it is IEEE-exact and reproducible. `lin` (a power function) feeds only `albedo_color`, which is compared with a tolerance and never hashed. Nothing iterates an unordered container on our side; Terrain3D's own unordered maps affect only rendering order. Control values are copied as bits; id 0 is the only base that can form a denormal (3.1).

## 16. To verify against Terrain3D and Godot (engineer, before building on it)

| Id | Assumed | If false |
| --- | --- | --- |
| W1 | a `PackedScene` packed in code is accepted by `set_scene_file`: `find_children("*LOD?", "MeshInstance3D")` finds the owned children, giving `get_lod_count` 2 | one LOD per tree kind: the LOD 1 mesh only, `lod0_range` 6000 |
| W2 | (read in source, not a guess) `set_scene_file` resets name, height offset, density and LOD ranges | — |
| W3 | `add_transforms(..., false)` stores instances in the regions at once; `update_mmis(..., true)` builds multimeshes on the next `frame_pre_draw`, which a headless run may never reach, so `get_instance_count` may stay 0 headless | T12 reads `get_instances()`; if that structure differs, T12 counts per kind with `get_instance_count` after one drawn frame, GPU only (G-list) |
| W4 | the shader's texture UV is world metres × `uv_scale` (`i_vertex = v_vertex`, world space) | multiply every `uv_scale` by 16 |
| W5 | normal maps are OpenGL (+Y green) | negate Y in 3.2 |
| W6 | a multimesh's instance colour multiplies the vertex colour under `vertex_color_use_as_albedo` | white instance colours (no tint) |
| W7 | Godot's front faces are clockwise | reverse the winding in `build_mesh` (U16 follows) |
| W8 | depth fog (`fog_mode` 1) in Godot 4.7 makes the plane at 400 km exactly the fog colour, equal to the sky's ground colour, with `fog_sky_affect` 0: in `v3d02-horizon.png` the pixels just above and below the plane's edge differ by at most 3/255 | sea material `shading_mode` unshaded with albedo the sea colour, so plane and sky ground match without fog |
| W9 | Terrain3D draws each cell as two triangles on one of its diagonals | bury trunks 4 m instead of 2 |
| W10 | control bit patterns survive `import_images` and the GPU (T10 reads them back) | none needed beyond id 0's rarity |
| W11 | mesh assets set before `add_child` initialise headless without error (`create_mesh_thumbnails` needs no display) | set them after `add_child` through `get_assets()` |
| W12 | at the cap, transfer plus first frame stay under about 5 s at 2048 | lower the default cap (Q-V3D-02-2) |

**Found (r2).** Every item held: W1, W2, W3 (T12 reads `get_instances()`), W4, W5, W7, W9, W10 and W11 by the checks and the source; W6 and W8 by eye on Paul's GPU; W12 at 1.1 s (transfer plus first frame at 2048 with r1's cap, before the density change; 1.9 s at r2's cap of 2,000,000, 6.6).

## 17. Questions for Paul (decided 2026-11-27: Paul accepts the recommendations)

| Id | Question | Recommendation (interim reading) |
| --- | --- | --- |
| Q-V3D-02-1 | Where the biome colour lives: in each ground texture (`albedo_color`, colour map white on land, as specified), or kept in the colour map over neutral textures | textures: rock, snow and water then keep their own colours instead of being tinted by the biome; the far view still matches the 2D map through 3.3 |
| Q-V3D-02-2 | Vegetation for the whole map at load with a cap of 1,000,000 (2048 thinned to about 45%), or a lower cap, or placing only near the camera now | whole map, cap 1,000,000; near-camera placement is the streaming stage's work. Decided as recommended, then changed by Paul after flying it (2026-11-27): cap 2,000,000 |
| Q-V3D-02-3 | Snow on alpine ground below −2 °C mean annual temperature, or no snow until weather exists | snow as specified; it reads the world's temperature layer and only inside the alpine biome |
| Q-V3D-02-4 | Tree density at most 60 per hectare (a real forest has several hundred), or denser with a shorter view range | 60: placeholder trees are large, and the cap binds at 2048 already. Decided as recommended, then changed by Paul after flying it (2026-11-27): 150 per hectare on forest floor and carr, with 100 shrubs per hectare of undergrowth (6.3) |
| Q-V3D-02-5 | Rivers painted by the 2D map's rule (one cell, widened from class 4), or width by class | the 2D rule: 3D and 2D agree cell for cell; true widths come with carving |
| Q-V3D-02-6 | The ground and vegetation tables live in C++ for now; design 7 wants each biome's TOML to name its scatter sets, which is `game/data/` and the other session's ground | C++ placeholders now; a later stage proposes the data fields through `proposals.md` and the lead |

**Open after r2.**

1. At 2048 the cap thins forests below the 1024 density (about 54 trees per hectare); true density needs placement near the camera (a later stage).
2. A soft lighter band below the horizon: the lit sea is lighter than the fog colour.
3. Rock covers most mountain slopes, so far views are greyer than the 2D map's alpine colours.
4. Shaded slopes are very dark.
5. Rivers read as painted ribbons, with a pale one-cell fringe on steep slopes.
6. Shrubs read as small boulders.
7. MSVC and CI have not run on this stage.
8. The interactive keys T and G were checked by Paul only informally.

## Revision notes

- r1 (2026-11-27): first draft. Approved by Paul the same day; Q-V3D-02-1 to 6 decided as recommended (status line and section 17 heading only).
- r2 (2026-11-27): as built. 3.3: `albedo_color` is set `srgb_encode`d (Godot linearises it); T9 compares the encoded value. 6.3: column `tree_q8` (983 on forest floor and carr, 393 elsewhere) and forest undergrowth (shrubs 721 and 753), Paul's change after flying r1. 6.4: jitter over the whole cell from one keyed draw per axis (`cell_jitter`), so instances keep more than 8 m from a painted river vertex; default cap 2,000,000 (`DEFAULT_INSTANCE_CAP`). 6.6: the measured counts, times and memory (0.58 KB per instance) replace the estimates. 9.1 step 5: one `add_transforms` per kind and instancer cell. 10.1: Terrain3D's default mesh under `--no-trees`. 10.2: headless thumbnail errors on key T. 10.3: the no-biome line replaces V3D-01's. 11.1 U13: interior cells only; the table, cap and even-jitter tests. 11.2 T9, T11, T13 as built. 14 invariant 5, 15: 8 m and 22 bits. 16: every item found to hold. 17: Q-V3D-02-2 and -4 changed by Paul. Open after r2 added.
