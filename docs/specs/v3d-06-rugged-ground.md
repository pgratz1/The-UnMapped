# V3D-06 Rugged ground

Status: **r1, 2026-11-28, approved by Paul on 2026-11-28**: Q-V3D-06-1, 2, 3 and 5 as recommended; Q-V3D-06-4 decided against the recommendation: pads lie under every place that ever stood in the world's history, so the ground is the same at every year (the text of the flattening section still describes the shown year; the as-built revision rewrites it). (2026-11-28). Sixth stage of the 3D terrain viewer track (`docs/handoff-3d-terrain-viewer.md`), on top of V3D-01 r3, V3D-02 r2, V3D-03 r2, V3D-04 r2 and V3D-05 r2 as built ("V3D-0n m" refers to their sections). It answers Paul's review of 2026-11-28 (`docs/design/proposals.md`, last entry, item 1): "from a distance the world looks appropriately rough, but as you get down to human level everything seems too smooth". It is the first piece of design section 7's "chunk heightmaps upsample the 16 m elevation layer and add seeded detail noise", done for the whole map at load; chunks, streaming and stitching stay later stages. It also lightens the mine beacon (Q-V3D-05-6, decided by Paul). Textures stay generated; nothing new but the boulder meshes.

**No world change.** The stage reads `world.db` through `worldgen`'s public API and writes nothing to it or to the source tree. `GENERATOR_VERSION`, the schema, every golden file, `world-hashes.sh` and its cases are unchanged; nothing under `worldgen/`, `tools/worldgen-cli/`, `game/data/` or `extern/` changes (Terrain3D is not edited). No new dependency or asset file. Owner: viewer engineer.

**Not in this stage:** streaming of terrain regions or chunks; 1 m ground; carved river channels (rivers stay painted, V3D-02 5.2); real textures (next stage); roads and paths (not in the world file until P6); town layouts, tree clearings in towns and nothing-in-water (Paul: wait for P6 site plans); collision; VR; the fly-across test.

## 1. Purpose

The preview draws one vertex per 16 m cell, so at human height every slope is a flat plane. This stage builds, at load, a finer height field: the 16 m heights interpolated to a spacing s (default 4 m) plus seeded detail noise whose amplitude depends on the biome and on nearness to mountains, zero on water and at shores, flattened under each settlement, ruin and site. The control and colour maps are rebuilt at s so that borders stay soft. Boulders stand on rugged ground near the camera. Everything that asks for ground height asks the new field. `--ground-detail off` gives V3D-05's 16 m ground.

## 2. Parts

| Area | Files | Contents |
| --- | --- | --- |
| detail (plain C++, no Godot types) | new `gdext/src/terrain/detail.hpp`, `.cpp` | `GroundField`, layouts (3), coarse factor fields (5), upsampling (4), noise (6), pads (7), the field (8), fine control and colour (9), stats (14) |
| boulders (plain C++) | new `gdext/src/terrain/boulders.hpp`, `.cpp` | density, placement, meshes, buffer (11) |
| vegetation, near cells, markers, terrain data | `vegetation.*`, `near_cells.*`, `markers3d.*`, `terrain_data.*` | read the field (10); `MINE_BEACON` (12) |
| scene | `terrain_preview.*`, `terrain_preview_near.cpp`, `terrain_preview_markers.cpp` | load order, worker bands, Terrain3D settings, options, key J, status, bench (13) |
| check | new `terrain_check_detail.cpp`; `terrain_check.cmake` | T41 to T53 (15.2) |
| unit tests | new `gdext/tests/detail_test.cpp`, `boulders_test.cpp` | U53 to U66 (15.1) |
| docs | `gdext/README.md`, "Terrain preview" | options, key, status, checks, screenshots, bench |

The new `.cpp` files of plain C++ join `unmapped_terrain_core`; the test files join `gdext-terrain-tests`. One commit, as earlier stages.

## 3. The central choice: whole-map upsampling at s = 4 m

Options: (a) the whole map upsampled at load to spacing s; (b) fine terrain only around the camera (streamed regions: the design's end state, a much larger stage that needs Terrain3D region add and remove and a worker pipeline per region); (c) 16 m vertices with detail faked in the shader (no geometry at human level: silhouettes, markers, trees and the camera stay on 16 m planes, which is exactly Paul's complaint). **Choice: (a) at s = 4 m at every map size** (Q-V3D-06-1). It is a step towards (b): the field is a pure function of (world, s, pads, coordinates), computed in row bands, so the streaming stage can compute it per region on a worker without change.

### 3.1 Terrain3D limits (from source)

| Limit | Source | Value |
| --- | --- | --- |
| region grid | `Terrain3DData::REGION_MAP_SIZE` (`terrain_3d_data.h:19`) | 32 × 32, locations −16 to 15 |
| region size | `Terrain3D::set_region_size` (`terrain_3d.cpp:593`), enum `RegionSize` | power of 2, 64 to 2048 vertices |
| fit | `Terrain3DData::import_images` (`terrain_3d_data.cpp:1009-1039`) | vertex grid of the image within [−16R, 16R − 1]: a map of V vertices centred at the origin fits iff V ≤ 32R |
| regions drawn | `Terrain3DMaterial::_max_regions` (`terrain_3d_material.h:73`, default 1024) | ≤ 1024 |
| vertex spacing | `Terrain3D::set_vertex_spacing` (`terrain_3d.cpp:799`) | clamped to [0.25, 100] |
| clipmap | `set_mesh_lods` (1 to 10, default 7), `set_mesh_size` (default 48), `terrain_3d_mesher.cpp` | LOD k has spacing s·2^k; outer half-extent about 96·2^(L−1)·s |
| maps | `import_images` (`:995`) | height, control and colour images must have equal sizes, so all three are at s |
| instancer | `Terrain3DInstancer::CELL_SIZE` 32 vertices (`terrain_3d_instancer.h:23`) | an instancer cell is 32·s m: 512 m today, 128 m at 4 m |
| heights | `get_height` = `get_pixel(TYPE_HEIGHT)` (`terrain_3d_data.h:307`); `get_surface_height` bilinear (`terrain_3d_data.cpp:722`) | |

### 3.2 Arithmetic

Map side L = 16N m; f = 16/s; V = N·f vertices per side. Region size chosen R = max(256, V/16) (16 regions per axis at most, 256 regions, map centred: locations −V/(2R) to V/(2R) − 1). Memory per vertex, steady (after load): our float field 4 B; Terrain3D's region images 12 B plus colour mipmaps (about 13.3 B); GPU texture arrays the same 13.3 B: **about 31 B**. Peak during load adds our control and colour buffers (8 B) and the three Godot images handed to `import_images` (12 B): **about 51 B**. Generation estimate 0.16 µs per vertex single-threaded (field 0.10, control 0.05, colour 0.01), divided by the worker count; `import_images` and upload about 0.03 µs per vertex. Measured values replace these (13.3).

| N | s | V | legal R (V ≤ 32R) | chosen R, regions | vertices | steady / peak memory | generation, 1 / 8 threads | import |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 512 | 16 (V3D-05) | 512 | 64 to 2048 | 256, 4 | 0.26 M | 8 / 13 MB | — | — |
| 512 | 8 | 1024 | 64 to 2048 | 256, 16 | 1.05 M | 33 / 53 MB | 0.17 / 0.03 s | 0.03 s |
| 512 | 4 | 2048 | 64 to 2048 | 256, 64 | 4.19 M | 130 / 214 MB | 0.67 / 0.10 s | 0.13 s |
| 512 | 2 | 4096 | 128 to 2048 | 256, 256 | 16.8 M | 520 / 856 MB | 2.7 / 0.4 s | 0.5 s |
| 1024 | 16 (V3D-05) | 1024 | 64 to 2048 | 256, 16 | 1.05 M | 33 / 53 MB | — | — |
| 1024 | 8 | 2048 | 64 to 2048 | 256, 64 | 4.19 M | 130 / 214 MB | 0.67 / 0.10 s | 0.13 s |
| 1024 | 4 | 4096 | 128 to 2048 | 256, 256 | 16.8 M | 520 / 856 MB | 2.7 / 0.4 s | 0.5 s |
| 1024 | 2 | 8192 | 256 to 2048 | 512, 256 | 67.1 M | 2.08 / 3.42 GB | 10.7 / 1.6 s | 2.0 s |
| 2048 | 16 (V3D-05) | 2048 | 64 to 2048 | 256, 64 | 4.19 M | 130 / 214 MB | — | — |
| 2048 | 8 | 4096 | 128 to 2048 | 256, 256 | 16.8 M | 520 / 856 MB | 2.7 / 0.4 s | 0.5 s |
| 2048 | 4 | 8192 | 256 to 2048 | 512, 256 | 67.1 M | 2.08 / 3.42 GB | 10.7 / 1.6 s | 2.0 s |
| 2048 | 2 | 16384 | 512 to 2048 | refused (1.07 G vertices, 33 GB steady; Godot's `Image` limit of 2^28 pixels is reached) | | | | |

**Clipmap.** `mesh_lods` = 7 + log2(16/s) (8, 9, 10 at s = 8, 4, 2), `mesh_size` 48 unchanged, so the outer half-extent stays 96·64·16 = 98 km as in V3D-05 and the clipmap's vertex count grows by one ring per added LOD (about 28,000 vertices each). At s = 4 the mesh is 4 m out to about 384 m, 8 m to 768 m, 16 m to 1.5 km, and beyond that exactly V3D-05's mesh: its vertices lie on multiples of 16 m, where the field is the 16 m height plus noise (8). So distant views change only by the noise sampled at 16 m vertices and by shading; the vertices are world-aligned, so the noise does not swim as the camera moves (W34).

**Instancer.** Terrain3D's far-tier cells shrink from 512 m to 128 m: up to 16 times more multimesh instances (1024: up to about 16,000 cells × 5 kind-LODs; 2048: 65,536 cells). This is the main frame-time risk (W32).

**Control and colour.** Rebuilt at s (section 9): every fine vertex takes the bilinear mix of the four surrounding 16 m controls, sampled at a position displaced by up to 4 m of seeded warp, so biome borders become soft curves, not 16 m squares or straight 16 m diagonals.

## 4. Upsampling (integer)

Coarse heights in decimetres, as V3D-01 5.2: `hd(c)` = e + d where e > 0 and the lake depth d > 0, else e. Fine vertex v = (xf, yf), 0 ≤ xf, yf < V; i = xf div f, a = xf mod f (j, b likewise); coarse indices clamped to [0, N − 1]. Catmull-Rom weights at t = a/f, scaled by 2f³ (exact integers):

```
W(-1) = -a^3 + 2a^2 f - a f^2        W(0) = 3a^3 - 5a^2 f + 2f^3
W(1)  = -3a^3 + 4a^2 f + a f^2       W(2) = a^3 - a^2 f                       (sum 2f^3)
f = 4: a = 0 (0, 128, 0, 0), 1 (-9, 111, 29, -3), 2 (-8, 72, 72, -8), 3 (-3, 29, 111, -9)
S      = sum over p, q in -1..2 of W(p; a) W(q; b) hd(i + p, j + q)              (int64)
cr_mm  = fdiv(100 S + 2 f^6, 4 f^6)                                               (fdiv: floor division)
up_mm  = clamp(cr_mm, 100 min4, 100 max4)       min4, max4 over hd(i, j), hd(i+1, j), hd(i, j+1), hd(i+1, j+1)
```

The clamp removes every overshoot: coasts (a 2 m shore next to a −50 m sea bed) and lake edges cannot ring, a quad whose four corners are one lake's surface is exactly flat at it, and up_mm = 100·hd at every coarse vertex (a = b = 0). Peaks keep their 16 m height (the clamp may flatten a few metres at a summit vertex; the noise then roughens it).

## 5. Coarse factor fields (integer, per 16 m cell, main thread)

Window cells outside the map are skipped. Chamfer distance: 3 per orthogonal step, 4 per diagonal, two passes (y ascending then x ascending with neighbours (−1, 0), (−1, −1), (0, −1), (1, −1); then the reverse), sources 0, others 6N.

| Field | Formula | Range |
| --- | --- | --- |
| `wet_noise(c)` | water ≠ 0, or painted > 0, or type not land (V3D-02 4.1) | bool |
| `wet_level(c)` | water ∈ {2, 3}, or type ∈ {13, 14} | bool |
| `ramp(D)` | clamp((D − 3)·1000 / 6, 0, 1000) (D ≤ 3: 0; 6: 500; ≥ 9: 1000) | per mille |
| `W_noise`, `W_level` | ramp(chamfer distance to the nearest `wet_noise`, `wet_level` cell) | per mille |
| `relief(c)` | max − min of E over the 9 × 9 window (144 m) | dm |
| `m_rel` | clamp((relief − 50)·1000 / 950, 0, 1000) (5 m to 100 m of relief) | per mille |
| `prox` | clamp(1000 − D_alp·1000 / 192, 0, 1000), D_alp the chamfer distance to the nearest cell of type 9 not painted (none: 6N) | per mille (1 km) |
| `M` | 1000 + m_rel + prox / 2 | 1000 to 2500 |
| `Ab`, `Rb`, `Bb` | floor of the mean of `AMP_CM`, `RIDGE_PM`, `BILLOW_PM` of the land cells (types 0 to 10) of the 5 × 5 window; 0 when none | cm, per mille |
| `A_cell` | min(`A_MAX_CM`, Ab·M / 1000)·W_noise / 1000 | cm |

| Type | `AMP_CM` | `RIDGE_PM` | `BILLOW_PM` | Reason |
| --- | --- | --- | --- | --- |
| 0 unknown | 100 | 0 | 0 | |
| 1 grassland | 120 | 0 | 600 | low, rounded swells |
| 2 forest floor | 200 | 0 | 300 | moderate: roots, hummocks, old blowdowns |
| 3 carr | 120 | 0 | 300 | wet woodland is low and level |
| 4 marsh | 40 | 0 | 0 | near flat, standing water |
| 5 fen | 60 | 0 | 0 | flat; peat hags are a later scatter |
| 6 steppe | 160 | 0 | 600 | open swells, long sight lines |
| 7 heath | 240 | 300 | 0 | broken upland |
| 8 moor | 300 | 400 | 0 | upland, tors and peat; the brief's "tundra" reads as moor: no tundra or desert biome exists (design 7 lists nine cool northern biomes) |
| 9 alpine (scree) | 600 | 1000 | 0 | high, ridged |
| 10 waste | 240 | 500 | 0 | blighted, broken; the nearest to the brief's "desert" |

`A_MAX_CM` = 1200. Expected peak amplitude A (m) before the shore and pad factors, with M at typical relief and nearness:

| Ground | relief, distance to alpine | M | A (m) | expected 64 m window std of the detail (about 0.13 A, 14) |
| --- | --- | --- | --- | --- |
| grassland, plain | 3 m, far | 1.00 | 1.2 | 0.16 m |
| forest, rolling | 15 m, far | 1.11 | 2.2 | 0.29 m |
| heath, foothills | 25 m, 300 m | 1.56 | 3.7 | 0.49 m |
| moor near peaks | 60 m, 150 m | 1.97 | 5.9 | 0.77 m |
| alpine | ≥ 100 m, 0 | 2.50 | 15 → 12 (cap) | 1.56 m |

## 6. Detail noise (integer)

Seeded, tiling-free value noise on a lattice in world metres from the map's corner (X_m = xf·s, Z_m = yf·s, both ≥ 0), so the shapes do not depend on s. Octaves o = 0 to 4 with wavelength λ_o = 128, 64, 32, 16, 8 m and weights `OCT_PM` = {361, 252, 177, 124, 86} (persistence 0.7, sum 1000); an octave is used only when λ_o ≥ 2s (s = 8 drops the 8 m octave; s = 2 adds λ_5 = 4 m with weight 60).

```
key_o   = uint32(mix64(seed ^ DETAIL_KEY) >> 32) ^ (0x9E3779B9 * (o + 1))     DETAIL_KEY = 0x64657461696C3036 (uint32 wrapping)
L = X_m / λ;  t16 = (X_m mod λ) * 65536 / λ;  K, u16 likewise from Z_m           (λ a power of two: exact)
S16(t)  = (t^2 (196608 - 2t)) >> 32                                            (smoothstep, int64, 0..65536)
val(p, q) = int32(hash32(p, q, key_o) >> 16) - 32768                            (hash32 of V3D-02 3.2)
n_o     = floor((top (65536 - Sy) + bot Sy) / 2^32)     Sx = S16(t16), Sy = S16(u16), top = val(L, K)(65536 - Sx) + val(L+1, K) Sx, bot likewise with K + 1
r_o     = max(-32768, 32767 - 2|n_o|)      (ridged)       b_o = 2|n_o| - 32768      (billow)
c_o     = n_o + fdiv(ridge (r_o - n_o) + billow (b_o - n_o), 1000)                 ridge, billow per mille (8)
N(v)    = fdiv(sum_o OCT_PM[o] c_o, 1000)                                           (about -32768 .. 32767)
```

The seed is meta `seed` as V3D-02 2 (absent: 0). Ridged and billowed octaves are not zero-mean (about +0.2 and −0.2 of the amplitude); the bias is part of the look and is removed only where the amplitude goes to zero.

**Slope bound.** The gradient of one octave is at most 3·w·A/λ (value differences up to 2, smoothstep slope up to 1.5 per lattice step), doubled for a ridged or billowed octave. With s = 4, Σ w_o/λ_o = 0.0309 per m, so |∇ detail| ≤ 0.093·A (plain) and 0.186·A (fully ridged): grassland 0.11 (6.4°), forest 0.20 (11.6°), heath 0.69 (35°), alpine at the cap 2.2 (66°, crags; typical rms slope about 0.3). Non-alpine ground stays walkable (P1-05's 35°) in at least 99% of its vertices (V10).

## 7. Pads (flattening under markers)

Pads come from the V3D-05 records of the shown year (Y = E, or `--year`), at each record's model cell (V3D-05 4 steps 1 and 2, which use cells only). No pad when the model cell has water ≠ 0 or E ≤ 0, with `--no-markers`, or when the history is absent or failed; key M does not change pads. So the ground depends on `--year` (Q-V3D-06-4). e is V3D-05 5's slot extent (ruins: of the stub slots).

| Kind | r0 (m), Chebyshev from the anchor |
| --- | --- |
| settlement kinds 1 to 3 | 16e + 16 |
| city 4, fortress 5 | 16e + 24 |
| ruin, old ruin | 16e + 16 |
| lair | 16 |
| outpost | 20 |
| mine | 24 |
| tower; library | 8; 20 |

For pad i with anchor (X_i, Z_i) and P_i = 100·hd(model cell) mm, at a fine vertex with d = max(|X − X_i|, |Z − Z_i|) (integer metres):

```
rel_i   = max - min of hd over the coarse vertices within Chebyshev r0 of the anchor           (dm)
LEVEL_i = rel_i <= PAD_FLAT_DM ? 1000 : 1000 * PAD_FLAT_DM / rel_i                             PAD_FLAT_DM = 60 (6 m)
rho_i(d)= d <= r0 ? 1000 : d >= r0 + PAD_FALL ? 0 : sm((r0 + PAD_FALL - d) * 1000 / PAD_FALL)   PAD_FALL = 48 m
sm(t)   = t^2 (3000 - 2t) / 10^6                                                                (integer, 0..1000)
N_pad   = 1000 - max_i rho_i            L_i = LEVEL_i * rho_i / 1000
```

A pad is fully level when the ground under it varies by at most 6 m; on steeper ground it is levelled in part (a cut or fill of about 6 m at most) and houses sink by V3D-05 5's ground rule. Overlapping pads blend (8), so the ground is continuous.

## 8. The field

Per fine vertex, with `bil(F)` = fdiv(Σ F(corner)·w + f²/2, f²) over the corners (i, j), (i+1, j), (i, j+1), (i+1, j+1) (clamped) and weights (f−a)(f−b), a(f−b), (f−a)b, ab:

```
A_mm  = 10 * bil(A_cell) * N_pad / 1000                    (integer division)
A_mm  = min(A_mm, max(0, up_mm - 200))                     (sea guard: land never dips below 0.2 m into the sea plane)
H1    = up_mm + fdiv(A_mm * N(v) + 16384, 32768)            ridge = bil(Rb), billow = bil(Bb) in N(v)
Ls    = sum_i L_i;  Hs = sum_i L_i (P_i - H1)
H     = H1 + fdiv(Hs * bil(W_level), 1000 * max(1000, Ls))
height(v) = float(H) / 1000.0f                             (metres; |H| < 2^24 so the conversion is exact)
```

`struct GroundField { Layout layout; std::vector<float> heights; int f; }`: `layout` is the fine layout (n = V, spacing s, region_size R, region_min −V/(2R), region_count V/R, origin −8N, so `cell_to_world` and `world_to_cell` of V3D-01 work on fine vertices); f = 1 and `heights` = V3D-01's buffer with `--ground-detail off`.

**The one height query.** `float ground_height(const GroundField&, float X, float Z)` = `ground_low(field.layout, field.heights, X, Z)` (V3D-02 6.5): the lower triangulation of the fine quad at (X, Z), clamped to the map; never above the surface Terrain3D draws, equal to the vertex height at a vertex. `ground_top(field, X, Z)` = `ground_clamp` on the field (largest of the four vertices) for the camera. Both are thread-safe readers of an immutable field.

Properties: H = 100·hd on every coarse vertex whose four quads are wet (A = 0 there); on a lake quad H = the surface exactly; W_noise is 0 on every quad that touches a river, lake or sea cell, so painted rivers and shores are the upsampled 16 m surface; at a pad anchor H = P_i unless another pad overlaps it (then the blend); every marker stands on the field at its anchor vertex (10).

## 9. Fine control and colour

The coarse control `cc` is V3D-04 4.4's (`build_control(layout, cells, look)`), the coarse colour `cl` V3D-02 5.1's (or V3D-01's with `--flat-colours`). At fine vertex v:

```
warp: wx = fdiv(64 n_w(v, key_wx) , 32768), wz likewise (1/16 m, |w| <= 4 m)   one octave, lambda 32 m, keys DETAIL_KEY ^ 1, ^ 2
U = 16 X_m + wx (1/16 m units = 1/256 cell); i = clamp(floor(U / 256), 0, N - 1), fu = U - 256 i clamped to [0, 255]; from Z_m and wz the same give j, fv
corner weights wq = (256 - fu)(256 - fv), fu (256 - fv), (256 - fu) fv, fu fv       (sum 65536; corners clamped)
per id k in 0..16:  acc[k] += wq (255 - blend(cc)) for k = base(cc);  acc[k] += wq blend(cc) for k = overlay(cc)
w1, k1 = the largest acc among ids {0..10, 13, 14} (ties: smaller id)     base = k1
w2, k2 = the largest acc among the other ids (ties: smaller id)           overlay = w2 = 0 ? k1 : k2
blend = w2 = 0 ? 0 : (255 w2 + (w1 + w2) / 2) / (w1 + w2)
```

So base is never a rock or snow id (11, 12, 15, 16): no denormal or NaN control can arise (V3D-02 3.1). Then, on land bases (0 to 10) only:

```
g_f   = isqrt(dx^2 + dz^2), dx = H(xf+1) - H(xf-1), dz likewise (mm, neighbours clamped), D = 2000 s
rock_f= g_f * 320 <= 224 D ? 0 : g_f >= D ? 255 : ((320 g_f - 224 D) * 255 + 48 D) / (96 D)   (35 to 45 degrees, V3D-02 4.1)
snow hold: if overlay = 12, blend = blend * hold_f / 255 with hold_f V3D-04 4.3's hold at g = g_f * 320 / D
if rock_f > blend and not (overlay = 12 and blend >= 128): overlay = rock id, blend = rock_f
rock id = look.rock ? ROCK_SEQ[band] with V3D-04 4.2 on (xf, yf) and E = H / 100 : 11
```

The fine colour is the same bilinear mix at the warped point of the four `cl` corners, per channel: (Σ wq·ch + 32768) >> 16. The detailed slope gives rock on small crags (Q-V3D-06-5); placement keeps reading the coarse `GroundCells` (10), so a tree may stand on a crag's rock texture. The warp moves water texture edges by up to 4 m, so V3D-02's "more than 8 m from a painted vertex" no longer means "off the water texture" for trees beside rivers.

## 10. Everything that samples height

| Reader | V3D-05 | Now |
| --- | --- | --- |
| Terrain3D | coarse buffers, spacing 16, region 256, LODs 7 | the field, s, R, 7 + log2 f; control and colour of 9 |
| vegetation (far and near, V3D-02 6.4, V3D-03 4.2) | Y = `ground_low(layout, heights)` | `VegetationInputs` gains `const GroundField* ground` (null: as before); Y = `ground_height`. Positions X, Z, yaw, scale, tint, `thin`, T, the far set, `Far`, `Extra` and every count are unchanged bit for bit: they read only cells and coarse layers |
| far-tier transfer (V3D-02 9.1 step 5) | 512 m instancer cells | grouped by the fine layout's 32-vertex cells (32·s m); same stored instances |
| near boxes (V3D-03 6.1) | heights over the cell's vertices, −2 m, +22 m | the field's vertices of the same area, −2 m (−4 m with boulders), +22 m |
| markers (V3D-05 4, 5, 8.3) | Y = `heights[cy·N + cx]`; ground rule on `ground_low` | Y = the field at fine vertex (cx·f, cy·f); ground rule on `ground_height`; `jump_view` on `ground_top` |
| camera clamp, `--above`, `--at-forest`, R, N | `ground_clamp` | `ground_top` |
| `--at-peak` | peak vertex height + 200 m | the field at the peak vertex + 200 m (the peak cell is still chosen on E) |
| HUD "above ground" | `sample_bilinear` | `sample_bilinear` on the field |
| sea plane | Y = 0 | unchanged; sea vertices carry no noise |

Checks that pin heights or buffers: V3D-02 T6, T12, T13 and V3D-03 T17 to T21, T23 compare against copies built from the same heights; under detail they take the field (15.2 T45). V3D-05 T32 ("Y equal to the height buffer exactly") becomes "equal to the field at the anchor vertex", which is the old statement with detail off. The partition invariants (V3D-03 10.2 to 10.4) hold unchanged for positions; only Y differs between `--ground-detail off` and on.

## 11. Boulders

Placed per near cell with the vegetation, near tier only (Q-V3D-06-3); not in Terrain3D's far tier, so V3D-03's partition, S, S_far, T and every existing instance are untouched (boulders are a separate list with their own keys). None with `--no-boulders`, `--no-trees`, `--veg-whole-map` or `--veg-radius 0` (status note, 13.2).

**Density** (Q8 per 16 m cell; 256 = 39 per hectare), per coarse cell:

```
q(c) = [ clamp((A_cell - 150) * 192 / 1050, 0, 192) + rock(c) * 64 / 255 ] * N_pad(c) / 1000     (N_pad at the coarse vertex)
excluded: type not 1..10, painted > 0, water != 0, E <= 0
```

Plains (A ≤ 1.5 m) none; foothill heath (3.7 m) 40 (6/ha); moor near peaks 63 (10/ha); alpine at the cap with rock 256 (39/ha); none on pads' flat parts.

**Placement** (V3D-02 6.4's keys, slot j = 5 = `BOULDER_SLOT`; slots 0 to 4 use keys up to hb + 319, so no draw is shared):

```
hc = mix64(hb + 320); count = (q >> 8) + ((hc & 0xFF) < (q & 0xFF) ? 1 : 0)
for k: h = mix64(hb + 321 + k); jx, jz = cell_jitter(mix64(h ^ JITTER_KEY_X)), cell_jitter(mix64(h ^ JITTER_KEY_Z)); X, Z and the edge drop as 6.4
  tint = h & 0xFF;  yaw1024 = (h >> 20) & 0x3FF;  u = (h >> 30) & 0x3FF
  d_mm = 500 + 5500 u^4 / 1023^4          (diameter 0.5 to 6 m: median 0.84, 18% above 3 m, 5% above 5 m)
  shape = ((h >> 40) & 0xFF) % 3;  tilt_x = (((h >> 48) & 0xFF) - 127.5) * 20 / 127.5 deg;  tilt_z from bits 56..63 likewise
  rock id = GroundCells::rock_variant of the cell (11 when the look's rock is off)
  Y = min of ground_height at (X, Z), (X +- d/2, Z), (X, Z +- d/2)  -  0.3 * height(shape) * d
```

**Meshes** (`build_boulder_mesh(shape, lod)`, flat normals, V3D-02's winding, unit size: largest horizontal extent 1, lowest y 0, highest `height(shape)`):

| Shape | LOD 0 | Displacement | Scale (x, y, z) | height | LOD 1 |
| --- | --- | --- | --- | --- | --- |
| 0 round | icosphere, one subdivision, 80 triangles | radius × (1 ± 0.15) | (0.5, 0.4, 0.45) | 0.8 | octahedron, 8 triangles, same scale |
| 1 slab | the same, 80 | ± 0.10 | (0.5, 0.225, 0.375) | 0.45 | the same |
| 2 angular | octahedron, one subdivision, 32 | ± 0.25 | (0.5, 0.35, 0.425) | 0.7 | the same |

Displacement of vertex index p: 1 + amp·(2·hash32(shape, p, `ROCK_MESH_KEY` 0x524F434B) / 2^32 − 1), shared vertices displaced once (index = order of first creation, documented in the source), then scaled and shifted to y ≥ 0. Face colour grey g = 224 + (hash32(shape, face, `ROCK_MESH_KEY` + 1) >> 24)·31 / 255. Instance colour = the rock id's colour (0x6E6A64, 0x8C8880, 0x7A6A58) per channel × v / 255, v = (216 + tint·39/255)/255. Transform: Basis = RotY(yaw)·RotX(tilt_x)·RotZ(tilt_z)·diag(d, d, d) (metres), origin (X, Y, Z); the buffer is V3D-03 7.2's layout (16 floats).

**Nodes.** `NearBoulders` (Node3D, sibling of `NearVegetation`), per filled near cell and shape with boulders and LOD: `MultiMeshInstance3D` `b<q>s<shape>l<lod>`, `custom_aabb` the cell box, LOD 0 [0, 250] m, LOD 1 [250, 1,200] end margin 100 fade self; material as the vegetation's. Boulders count toward `max_attach_instances`. Key T does not hide them (they are not vegetation); M does not either. A 2 km ring of alpine ground holds at most about 50,000 boulders, about 0.5 M triangles.

## 12. Mine beacon

`MINE_BEACON` = (214, 168, 64), 0xD6A840 (ochre; luma 170 against forest floor 82, distinct from the lair red, ruin grey and tower violet); the mine beacon's colour in V3D-05 6. `MARKER_MINE` (60, 60, 60) stays the headframe beam of a mine without founder. Pinned by U64 and T53 (and T33's table).

## 13. Scene side

### 13.1 Load order (`open_world`)

1. Layers, coarse heights, `GroundCells`, coarse control and colour (V3D-01 to V3D-04).
2. Unless `--no-markers`: `load_marker_records` and the model cells (V3D-05 3, 4 steps 1 and 2); `marker_pads`.
3. Coarse factor fields (5), main thread.
4. With detail: the field, then fine control and colour, in bands of 256 fine rows, one task per band through `WorkerThreadPool::add_group_task`, waited for with `wait_for_group_task_completion` (W35; fallback `add_task` per band as V3D-03). Rules: the inputs (coarse buffers, factor fields, pads, seed, look) are built before the first task and not touched until the last ends; a task writes only its own rows of the three output vectors and calls nothing of Godot; the main thread reads nothing until all have ended. Every vertex is a pure function of the inputs and its coordinates, so the result is the same for any band split and thread count (U66, T52). `--detail-threads 0` runs the bands in order on the main thread.
5. Images from the fine buffers; our control and colour buffers are freed after `import_images` (the field is kept); Terrain3D: `vertex_spacing` s, `region_size` R and `mesh_lods` before `add_child` (W30), then V3D-01's settings and `import_images` at origin (−8N, 0, −8N).
6. Far tier, near grid, marker placement (`place_markers(records, layout, field)`) and meshes, start view: on the field (10). Boulders with the near cells (V3D-03 7.1's tasks compute both).

### 13.2 Options, key, HUD, status

| Option | Meaning |
| --- | --- |
| `--ground-detail off\|8\|4\|2` | spacing (default 4); off: V3D-05's ground: coarse buffers, spacing 16, region 256, LODs 7, no pads, Y from the coarse buffer. 2 on a 2048 map: 4 m with the status note |
| `--no-boulders` | no boulders |
| `--detail-threads <n>` | 0 to 16 (default 8) |

Refusals (exit 1, stderr): `--ground-detail <v>: off, 8, 4 or 2`, `--detail-threads <v>: a whole number from 0 to 16`. **Key J**: detail on or off, by reopening the world as key G does (keeps `FlyState`, the look, M's state; the near tier waits for its tasks first, V3D-03 7.1); its cost is the load's detail part (13.3), under a second at 512 and 1024 by the estimate, a few seconds at 2048. `HUD_KEYS` gains `, J detail`; T35's comparison uses the constant. Status lines: `ground detail: <s> m, <V> x <V> vertices in <k> regions of <R>, <p> pads; built in <ms> ms on <t> threads`; `ground detail: off (16 m, as V3D-05)`; `--ground-detail 2 is limited to maps of 512 and 1024 cells: 4 m used`; `boulders: <n> per near cell at most, near tier only` or `boulders: off (<reason>)`. HUD line 1 is unchanged in form.

### 13.3 Bench (measurements only, no bounds)

```
UNMAPPED-TERRAIN-BENCH: ground detail: <s> m, <V>^2 vertices, <k> regions of <R>, lods <L>; factors <ms> ms, field <ms> ms, control <ms> ms, colour <ms> ms (<t> threads); images <ms> ms, import <ms> ms; memory field <MB> MB, static <MB> MB; pads <p>; far-tier instancer cells <c>
UNMAPPED-TERRAIN-BENCH: ground detail stats: <type> rms <m> m, steep <permille>; ...   (per land type present, 14)
UNMAPPED-TERRAIN-BENCH: boulders: near <n> in <cells> cells, per cell max <m>; compute mean <us> us
```

The README records 512, 1024 and 2048 at s = 8, 4 (and 2 at 512, 1024): load, memory and the V3D-02 view line's frame times, each against `--ground-detail off`.

## 14. Stats (plain C++, for the bench and V8 to V11)

`detail_stats(field, coarse heights, cells)`: per land type t, over fine vertices whose coarse quad corner (i, j) has type t, W_noise = 1000 and N_pad = 1000: `rms` = median over the 64 m windows (16 × 16 fine vertices at 4 m, aligned to multiples of 64 m, at least 128 such vertices) of the standard deviation of H − up_mm; `steep` = per mille of those vertices with rock_f > 0 (slope above 35°). Double precision, printed with two decimals; rendering-side only.

## 15. Tests

### 15.1 Unit tests (`gdext-terrain-tests`, no Godot)

Fixtures: hand-made coarse worlds on a 512 layout in the test file (flat land; a linear ramp; a lake of one surface beside land; a coast with a −50 m sea bed; an alpine ridge with heath and grassland around it; a river of painted cells), at s = 8 and 4 where the cost allows.

| Id | Test |
| --- | --- |
| U53 | Catmull-Rom weights for f = 2, 4, 8 at every a: the formula of 4, sum 2f³, f = 4's table |
| U54 | upsampling: up_mm = 100·hd at every coarse vertex; within [100 min4, 100 max4] everywhere; flat stays flat; the ramp reproduced exactly away from the edges; a lake quad exactly the surface; edge indices clamped |
| U55 | noise: the test's own copy of 6 gives identical N at 64 fixed vertices for seeds 0 and 42 and each ridge and billow setting; range; octave selection by s; shapes identical at s = 8 and 4 at shared positions for octaves λ ≥ 16 |
| U56 | factor fields: chamfer distances against brute force on the fixtures; ramp at D = 3, 4, 6, 9; relief and m_rel; prox at 0, 96, 192 chamfer; M; Ab at a border; `A_cell` cap and zero on wet cells and within one cell; the tables of 5 |
| U57 | field: A = 0 (H = up_mm) on every vertex of a quad touching a wet cell; the sea guard: H1 ≥ min(up_mm, 200) at every vertex (|N| ≤ 32768, so the noise never exceeds A_mm); FNV-1a 64 of the field's float bits for the alpine fixture at s = 8 and 4 and seed 42, recorded (the same constants pass on GCC, Clang and MSVC) |
| U58 | pads: every vertex within r0 of a pad with rel ≤ 6 m equals P exactly; the anchor equals P; LEVEL for rel 6, 12 m; two overlapping pads: adjacent vertices differ by at most the no-pad maximum plus |P_a − P_b|·4s/48; no pad on a water cell; r0 per kind of 7 |
| U59 | slope: max |∇ detail| (central differences of H − up_mm) ≤ 0.186·A_max on the fixtures; the walkability share of V10 on the ramp fixture |
| U60 | fine control: the accumulation on hand-made corners (one type; a straight border; a rock overlay corner; snow); base never in {11, 12, 15, 16}; bits as V3D-02 4.2; rock_f at g_f = 0.69, 0.85, 1.0 D (0, about 128, 255); the snow hold; warp within 4 m; fine colour of a uniform area equals the coarse colour |
| U61 | boulders: counts within 4 standard deviations of q per cell type on a 512 fixture; exclusions; jitter < 8 m; d quantiles (median 0.84 m ± 0.05, share > 5 m 5% ± 1%); every boulder's lowest point ≤ ground at its five points; `place_candidates` output byte-identical with boulders on and off; two calls identical |
| U62 | boulder meshes: triangle counts 80, 80, 32 and 8; closed (every edge in two triangles); outward unit normals; extent 1, lowest y 0, highest y = height(shape) ± 1e-5; face greys in [224, 255] |
| U63 | `ground_height` = vertex height at fine vertices and ≤ both triangulations at 1,000 points; `ground_top` ≥ it; with `--ground-detail off` both equal V3D-02's `ground_low` and `ground_clamp` |
| U64 | options: `off`, `8`, `4`, `2`; refused `16`, `3`, `x`, empty; `--detail-threads` 0, 16, refused 17; constants of 5 to 12 (`A_MAX_CM` 1200, `PAD_FLAT_DM` 60, `PAD_FALL` 48, `MINE_BEACON` 0xD6A840, `BOULDER_SLOT` 5) |
| U65 | fine layouts for N ∈ {512, 1024, 2048} × s ∈ {8, 4, 2}: V, R, region_min, region_count and mesh_lods of 3.2; 2 at 2048 refused to 4 |
| U66 | the field, control and colour built in 1, 3 and 16 bands (any order) are byte-identical |

### 15.2 Terrain check (`gdext-terrain-check`; additions)

Every existing run gains `--ground-detail off --no-boulders`, so T1 to T40 run unchanged (but for the mine beacon's colour in T33's table). New runs: `w512` (seed 49, full history; default detail 4, boulders, markers), `w512 --ground-detail 8`, `w512 --ground-detail 2`, `w512 --no-markers`, `w512 --detail-threads 0`, `w512 --ground-detail off` (boulders on), `w1024` (geography only, detail 4). The check keeps its own copy of 9's control rule and 7's pads, and calls the plain `build_ground_field` for the field (its formulas are pinned by U53 to U58).

| Id | Check |
| --- | --- |
| T41 | Terrain3D: `vertex_spacing` s, `region_size` R, `mesh_lods`, region locations exactly the centred (V/R)² set |
| T42 | the height image equals the field bit for bit; `get_height` at the 32 V3D-01 T6 points (as fine vertices) and 256 fixed fine vertices equals the field within 0.001 m |
| T43 | control and colour images equal the check's own 9 at 4,096 fixed vertices ((37k mod V, 61k mod V)) and at every vertex of four 64 × 64 windows (a shore, a painted river, a biome border, the highest peak); every control value's bits valid, base ∉ {11, 12, 15, 16} |
| T44 | field invariants on the preview's own field: lake quads flat at the surface; H = 100·hd on wet-only vertices; pads of 7 flat where full; for every marker, Y = the field at its anchor vertex |
| T45 | vegetation on the field: T12, T13, T17 to T21 with Y from the field; X, Z, yaw, scale, tint, T, the far set and every near cell's counts equal those of the `--ground-detail off` run (the check builds both) |
| T46 | markers on the field: T31 to T34; meshes equal `build_marker_mesh` with the field; on a fully levelled pad every house part's base corners at P (within 1e-3 m) |
| T47 | boulders: `NearBoulders` per filled cell, node names, counts and buffers equal the check's own `near_boulders`; none on an excluded cell or within a full pad; no `NearBoulders` under `--no-boulders` and `--veg-whole-map`, with the status line |
| T48 | camera: T8's 300 frames stay above `ground_top` + 2 on the field; the HUD's above-ground value from the field; `--above`, `--at-forest`, `--at-peak` and key N on the field |
| T49 | `--ground-detail off`: images byte-equal to the coarse buffers of V3D-05; boulders (on) lie on `ground_low` of the coarse heights |
| T50 | key J: off and back reopen with one Terrain3D node, the camera unchanged, the status line; the field after J, J equals the first (FNV-1a 64) |
| T51 | status, HUD and bench lines (headless: the detail and stats lines); the refusals of 13.2; `--ground-detail 2` on `w512` gives V = 4096 |
| T52 | `--detail-threads 0`: field, control and colour hashes equal the threaded run's |
| T53 | every mine record's beacon colour is `MINE_BEACON` (an info line when `w512` has no mine) |

Expected cost: `w512` at 4 m is 4.2 M vertices (about 0.5 s with import), at 2 m 16.8 M (about 2 s); with the check's own work about 30 s more in `linux-gcc-release` (37.5 s today) and 60 s more in debug. The timeout stays 300 s; Windows CI's slower runners (up to twice the time) stay under about 140 s in release (`gdext-windows`); measured values go in the README.

### 15.3 Screenshots (the engineer judges from these)

```
X() { env -u WAYLAND_DISPLAY xvfb-run -a -s "-screen 0 1600x900x24" "$GODOT" --display-driver x11 --path "$B/game" res://viewer3d/terrain_preview.tscn -- "$@"; }
OUT=~/.cache/the-unmapped/out; W=$OUT/seed42-1024.db; OFF="--ground-detail off --no-boulders"
for v in "grass $GX $GY" "foot $FX $FY" "shore $LX $LY" "river $RX $RY"; do set -- $v
  for h in 2 20; do X $W --at $2 $3 --above $h --pitch -6 $OFF --screenshot $OUT/v3d06-$1-$h-before.png
                    X $W --at $2 $3 --above $h --pitch -6 --screenshot $OUT/v3d06-$1-$h-after.png; done; done
for h in 2 20; do X $W --at-forest --above $h --pitch -6 $OFF --screenshot $OUT/v3d06-forest-$h-before.png
                  X $W --at-forest --above $h --pitch -6 --screenshot $OUT/v3d06-forest-$h-after.png
                  X $W --at-peak --above $h --pitch -10 $OFF --screenshot $OUT/v3d06-peak-$h-before.png
                  X $W --at-peak --above $h --pitch -10 --screenshot $OUT/v3d06-peak-$h-after.png; done
X $W --at-marker rank:1 --above 20 --pitch -8 $OFF --screenshot $OUT/v3d06-town-before.png
X $W --at-marker rank:1 --above 20 --pitch -8 --screenshot $OUT/v3d06-town-after.png
X $W --at 512 960 --alt 5000 --pitch -20 $OFF --screenshot $OUT/v3d06-far-before.png
X $W --at 512 960 --alt 5000 --pitch -20 --screenshot $OUT/v3d06-far-after.png
X $W --at-peak --above 300 --pitch -15 --screenshot $OUT/v3d06-boulders-300.png
X $OUT/seed42-512.db --bench; X $W --bench; X $W $OFF --bench; X $OUT/seed42-2048.db --bench; X $OUT/seed42-2048.db $OFF --bench
X $OUT/seed42-2048.db --ground-detail 8 --bench
```

GX GY: a grassland cell at least 1 km from alpine ground; FX FY: a heath or moor cell 200 to 500 m from alpine ground; LX LY: a lake cell beside land; RX RY: a painted river cell on low ground; chosen by the engineer from `seed42-1024.biomes.png` and `map.png` and recorded in the README. Luma and the excluded HUD rows as V3D-04 10.3.

| Id | Good means |
| --- | --- |
| V8 | `v3d06-grass-*-after`: visible swells at 2 m and 20 m, the horizon line no longer straight; bench stats: grassland rms 0.10 to 0.30 m, steep ≤ 5 per mille |
| V9 | `forest`: rms 0.18 to 0.45 m; trees stand on the ground (no trunk base visible in the air, none sunk past its trunk) |
| V10 | `foot`: rms of heath and moor 0.35 to 1.0 m; boulders scattered; at least 99% of non-alpine land vertices (steep ≤ 10 per mille summed over types 1 to 8) walkable |
| V11 | `peak`: alpine rms 1.0 to 2.2 m; crags with rock texture; many boulders, a few large; `v3d06-boulders-300.png` shows boulders as grey dots on rock, none floating |
| V12 | `town-after`: houses on a flat pad, no house sunk beyond its 0.5 m sink on a full pad; no step at the pad's edge steeper than the ground around |
| V13 | `shore` and `river`: the lake surface flat to its edge, no ring or ridge along the coast or shore, the river ribbon on smooth ground, no trench |
| V14 | `far`: mean |Δ luma| between before and after ≤ 4, and at most 5% of pixels differ by more than 24 |
| V15 | bench: the V3D-02 view line's mean frame at 1024 and 2048 within +1.0 ms of `--ground-detail off` on a real display (Paul), recorded through the virtual display (copy-bound, indicative) |

Tuning: if a type's rms misses its range, scale its `AMP_CM` by the ratio of the range's middle to the measured value, once, then report; if V14 misses, drop the 8 m octave's weight to 43 once, then report.

### 15.4 By hand on a GPU (Paul)

| Id | Look for |
| --- | --- |
| G29 | at walking height (2 m) over grassland, forest, heath and mountains: the ground is no longer planar; mountains clearly rougher; plains still gentle |
| G30 | flying low at 200 m/s: no shimmer or popping of the ground at 0.4, 0.8 or 1.5 km (the clipmap LOD rings) |
| G31 | towns on level ground; ruins, lairs and towers seated; nothing floats |
| G32 | J toggles detail within a few seconds at 2048; the far views look as before |

## 16. Invariants

1. The preview opens `world.db` read-only and writes nothing but `--screenshot`'s file; nothing in `worldgen/`, `tools/`, `game/data/`, `extern/`, the schema or a golden file changes.
2. The field is a pure function of the world file, s, the pads (so the year and `--no-markers`) and the seed; identical for any thread count, run and platform.
3. Coarse vertices of wet-only quads keep the 16 m height; lake quads are flat at their surface; quads touching river, lake or sea cells carry no noise; up_mm stays within its quad's corner range.
4. Vegetation positions, yaw, scale, tint, the far set, `Far`, `Extra` and all counts are V3D-05's bit for bit in every mode; only Y follows the field (V3D-05 invariant 5 is restated so: markers on, off or absent change vegetation Y within pads only).
5. Every instance, boulder and marker anchor is placed on `ground_height` or `ground_top` of the field Terrain3D draws.
6. Every control value has base in {0..10, 13, 14}, overlay in 0..16, blend in 0..255, other bits 0.
7. `--ground-detail off --no-boulders` gives V3D-05's preview but for the mine beacon's colour and the HUD key line; with `--flat-colours --no-trees --look-v3d03 all --no-markers` added, V3D-01's look.

## 17. Determinism notes

The field, its factor fields, pads, fine control, fine colour and boulder placement are integer arithmetic: `uint32`/`uint64` wrapping hashes, `int64` products, `fdiv` floor division written out, `isqrt64`; no floating point reaches them, so they are exact on every compiler. The only float steps are the final mm-to-metre division (IEEE-exact, |H| < 2^24), `ground_low` (V3D-02 15), tilt and yaw (`std::sin`, `std::cos`, rendering only, compared with tolerances) and the stats (double, printed only). Band order and thread count cannot change a value (each vertex is computed alone; integer sums in pads are order-free). No unordered container; pads in record order.

## 18. To verify (engineer, before building on it)

| Id | Assumed | If false |
| --- | --- | --- |
| W30 | `region_size` and `mesh_lods` set before `add_child` are honoured by `import_images` and the mesher | set them after `add_child`, before `import_images` |
| W31 | an 8192² `FORMAT_RF` image (256 MB) and its import work and take under about 3 s | import in tiles of 2048² vertices at their positions (16 calls) |
| W32 | 128 m instancer cells cost under 1 ms per frame more than V3D-05 at 2048 (far-tier MMIs about 16 times more) | the far tier moves to the preview's own multimeshes in 512 m groups (V3D-03 7.2's buffers, built once at load); T12, T17 then read those nodes |
| W33 | `mesh_lods` 7 + log2 f keeps the reach of V3D-05 (the horizon views of V3D-02 G13) | raise `mesh_size` to 64 |
| W34 | no visible shimmer of the noise at far LODs or in shading (Terrain3D's fragment normals read the full-resolution height map) | drop the 8 m octave (`OCT_PM[4]` 0) |
| W35 | `add_group_task` from the extension runs on workers headless and on Windows | `add_task` per band |
| W36 | 2048 at 4 m loads in under 15 s with static memory under 6 GB on the laptop | default 8 at 2048 (one constant; Q-V3D-06-1) |
| W37 | `get_height` returns the vertex value of the field at fine vertices (T42) | compare with `get_surface_height` within 0.01 m |

## 19. Questions for Paul (interim readings in force until he decides)

| Id | Question | Recommendation (interim) |
| --- | --- | --- |
| Q-V3D-06-1 | Spacing: 4 m at every size (2048: about 2.1 GB steady, 3.4 GB peak, a few seconds more load), or 8 m at 2048 | 4 m everywhere; 8 m at 2048 if W36 fails |
| Q-V3D-06-2 | How rugged: peak amplitudes of section 5 (grassland 1.2 m, forest 2 m, heath 2.4 m, moor 3 m, alpine 6 m, up to × 2.5 near mountains, capped at 12 m; window rms 0.16 m on plains to 1.6 m on mountains) | as specified; tuned once on V8 to V11 |
| Q-V3D-06-3 | Boulders as meshes near the camera (to 1.2 km, up to 39 per hectare on rocky alpine ground) as well as rugged ground, or rugged ground only | both |
| Q-V3D-06-4 | Flattening: pads under the shown year's settlements, ruins and sites (the ground changes with `--year`), footprint + 48 m fall-off, at most about 6 m of cut or fill; or pads for every place that ever stood | the shown year: the ground matches what is drawn |
| Q-V3D-06-5 | Rock from the detailed slope (small crags show rock; placement stays on the 16 m rule, so a few trees stand on crag rock), or rock on the 16 m slope as before | detailed slope |

No `proposals.md` entry from this spec: Paul's request is recorded in the entry of 2026-11-28; the lead may add the per-biome amplitude and the flattening by year to it.

## Revision notes

- r1 (2026-11-28): first draft for Paul's approval.
