# P1-01 Elevation

Status: **second version, partly approved.** Draft by the spec writer, 2026-11-15. Phase P1 Geography, stage 1 (`elevation`). Design doc section 4 (stage 1: "layered noise shaped by a continent mask, then hydraulic erosion"). Depends on P0-01 to P0-04 and P1-00, whose sections are cited as "P1-00 n" and whose shared algorithms G1 to G6 and determinism rules D1 to D10 are used by name.

**This version replaces the first version of P1-01 entirely.** The first version was implemented as written and its terrain was rejected: every world was a round, centred island with a raised tableland and thin worm-shaped mountain walls, and three hard statistics of P1-00 13.1 failed on every seed. This version is built from design candidate v13 of the engineer's exploration (branch `p1/elevation-explore`, commit `3396de5`); the world critic's review of v13 (`docs/reviews/p1-01-elevation-v13.md`, branch `p1/review-elevation`, commit `ca2e131`) froze its continent outline and range layout and sent its relief and its depression conditioning back for another design round.

So each section below is marked:

| Mark | Meaning |
| --- | --- |
| **stable** | Approved for implementation as written. Changes need a spec amendment. |
| **placeholder** | Not approved. Records what v13 does in outline, the critic's blockers and the open design questions, and states the interface the next design must meet. Not to be implemented from this text. |

| Section | Mark |
| --- | --- |
| 1 Purpose, 2 Inputs, 3 Outputs | stable (the relief streams of 2 are provisional) |
| 4.1 Noise records | stable |
| 4.2 Tuning fields of E1 to E4, E8, E9 | stable |
| 4.3 Tuning fields of E5 to E7 and E10 | placeholder |
| 4.4 Named checks, 4.5 tuning file, 4.6 derived integers | stable for the stable fields |
| 5.0 Conventions; E1, E2, E3, E4; E8; E9; E11 | stable |
| E5 to E7 (heights, mountain scale, compose) | placeholder, with a stable output contract (5.5) |
| E10 (depression conditioning) | placeholder, with a stable output contract (5.10) |
| 6 Invariants | stable except items marked |
| 7 Determinism notes and bounds | stable for the stable steps |
| 8 Performance, 9 Database rows | stable |
| 10 Tests | stable for the stable steps; placeholders listed |
| 11 Open questions | — |
| Appendix V13 | record of the superseded v13 relief and conditioning; **not approved** |

## 1. Purpose (stable)

Produce the `elevation` layer: one continent per world that reads as a continent. A coastline with bays and peninsulas at several scales; mountain ranges that run as ranges and can be walked; broad lowlands, some of them flat; and enough relief variety for the six biomes and for history to find borders, passes, defensible and fertile places. The stage builds a continent field from warped map-scale noise, a radial and square edge falloff and metre-scale coast noise; sets the coast so that a target share of the map is land; lays out mountain belts along the zero lines of a second map-scale noise, interrupted by a segmentation noise; builds heights in metres from these (E5 to E7, placeholder); carves them with stream-power erosion and hillslope diffusion; quantizes to decimetres; and conditions depressions so that every remaining one is a lake (E10, placeholder). Its output is the final terrain: no later stage changes it (P1-02 section 9).

## 2. Inputs (stable)

| Input | Source |
| --- | --- |
| World seed (`uint64`) | `meta.seed` |
| Map side N | `param.map_size` (512, 1024, 2048); library and tests: any 16 ≤ N ≤ 2048 |
| Tuning | definition `worldgen.elevation` (section 4) |
| Earlier layers | none |

Streams. Each gives a noise key K_name = `stream_key(seed, "geo.elevation.<name>")`. No `Rng` draw anywhere in this stage.

| Key | Stream name | Used in | Status |
| --- | --- | --- | --- |
| K_cwx | `geo.elevation.continent_warp_x` | E2 | stable |
| K_cwy | `geo.elevation.continent_warp_y` | E2 | stable |
| K_continent | `geo.elevation.continent` | E1 (centre hash), E2 | stable |
| K_belt | `geo.elevation.belt` | E2 | stable |
| K_coast | `geo.elevation.coast` | E2 | stable |
| K_segment | `geo.elevation.segment` | E2 | stable |
| K_rwx | `geo.elevation.relief_warp_x` | E5 | provisional |
| K_rwy | `geo.elevation.relief_warp_y` | E5 | provisional |
| K_ridge | `geo.elevation.ridge` | E5 | provisional |
| K_hills | `geo.elevation.hills` | E5 | provisional |
| K_roughness | `geo.elevation.roughness` | E5 | provisional |
| K_basin | `geo.elevation.basin` | E5 | provisional |
| K_detail | `geo.elevation.detail` | E5 | provisional |

All thirteen are registered in `streams.hpp` (P0-01 7.3). A provisional stream may be dropped or renamed by the next relief design; a stable one may not change without a spec amendment, since it changes every world.

## 3. Outputs (stable)

One layer, `elevation`: `i16`, N × N, codec 1, decimetres above sea level (P1-00 4). Values in [-30000, 30000], never 0. Negative cells are ocean, positive cells land (including lake beds). Postconditions in section 6.

## 4. Tuning

All fields of `worldgen.elevation` are required (P1-00 6.1). "Designed" values follow from the design or the algorithm; "guess" values are starting points to be tuned against the seed sweep.

### 4.1 Noise records and noise functions (stable)

Two record types, shared with later stages (P1-00 6.2):

| Record | Fields |
| --- | --- |
| `fbm_map` | `cycles` fx(0.125, 64), base frequency in cycles per map side; `octaves` int(1, 10); `gain` fx(0.0625, 1) |
| `fbm_m` | `wavelength_m` int(64, 65536), base wavelength in metres; `octaves` int(1, 10); `gain` fx(0.0625, 1) |

`LAC` = `from_int(2)`, a constant of the code: a power of two keeps every octave's frequency an exact multiple of the base.

```
fbm_map(K, a, b, rec) = fbm2(K, a, b, rec.octaves, rec.cycles, LAC, rec.gain)
fbm_m(K, a, b, rec)   = fbm2(K, a, b, rec.octaves, from_ratio(1, rec.wavelength_m), LAC, rec.gain)

ridged_m(K, a, b, rec, weight):              // Musgrave-style ridged multifractal, result in [0, ONE]
    f = from_ratio(1, rec.wavelength_m); amp = ONE; wgt = ONE; sum = 0; norm = 0
    for o in 0 .. rec.octaves - 1:
        xs   = mul(a, f)
        ys   = mul(b, f)
        n    = noise2(substream(K, o), xs, ys)
        s    = ONE - abs(n)
        s    = mul(s, s)
        s    = mul(s, wgt)
        wgt  = clamp(mul(s, weight), 0, ONE)
        sum  = sum + mul(s, amp)
        norm = norm + amp
        amp  = mul(amp, rec.gain)
        f    = mul(f, LAC)
    return clamp(div(sum, norm), 0, ONE)
```

Every statement is evaluated as written, in this order, with P0-01 rounding; `amp` and `f` are updated after the last octave too, as in `fbm2`. `fbm_map` samples at map-fraction coordinates, `fbm_m` and `ridged_m` at metre coordinates. `ridged_m` is used only by the placeholder E5 at present; it stays here because P0-01 amendment (P1-00 A1 (e)) points to it.

**Normalization.** `fbm2` does not reach its nominal ±1 (P0 measurement: within about ±0.67). The stage never relies on its range: the coast is a quantile of the continent field (E3), belt width is scaled by the median gradient of the belt noise (E3), and the heights in metres are scaled from quantiles (E6, placeholder). What still depends on the spread is the balance between terms, which is tuning.

### 4.2 Fields of the stable steps (stable)

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `land_fraction` | fx(0.1, 0.8) | "0.40" | designed (target) | share of the map's cells above the coast threshold, before E4's margin, pockets and islands |
| `edge_margin_frac` | fx(0, 0.25) | "0.03" | designed | band along every edge forced to ocean, fraction of N (at least 1 cell) |
| `continent_warp` | `fbm_map` | cycles "1.5", octaves 3, gain "0.5" | guess | domain warp of the continent field |
| `continent_warp_amp_frac` | fx(0, 0.5) | "0.2" | guess | warp displacement per unit of noise, fraction of the map side |
| `continent_noise` | `fbm_map` | cycles "3", octaves 4, gain "0.55" | guess | low-frequency shape noise: bays, peninsulas, off-centre lobes |
| `continent_noise_amp` | fx(0, 4) | "1.0" | guess | weight of the shape noise against the radial cone |
| `continent_center_jitter` | fx(0, 0.25) | "0.06" | guess | largest offset of the cone centre from the map centre per axis, map fraction |
| `edge_falloff_frac` | fx(0.01, 0.5) | "0.12" | guess | width of the square guard band, map fraction |
| `edge_falloff_depth` | fx(0, 8) | 2 | guess | strength of the edge falloff (square guard and radial), in units of the continent field |
| `edge_noise_amp` | fx(0, 0.25) | "0.15" | guess | perturbation of both falloff distances by the shape noise |
| `edge_radial_start` | fx(0, 2) | "0.8" | guess | radial distance (1 = an edge midpoint) where the radial falloff starts |
| `edge_radial_end` | fx(0.01, 2) | "1.0" | guess | radial distance where it is complete |
| `coast_noise` | `fbm_m` | wavelength 2000, octaves 5, gain "0.5" | guess | metre-scale coastline detail, unwarped |
| `coast_noise_amp` | fx(0, 2) | "0.1" | guess | its weight in the continent field |
| `island_min_area_m2` | int(0, 1000000000) | 50000 | guess | land components smaller than this are sunk |
| `belt_noise` | `fbm_map` | cycles "1.25", octaves 2, gain "0.5" | guess | mountain belts follow the zero lines of this noise (signed) |
| `belt_halfwidth_m` | fx(100, 20000) | 2000 | guess | belt half-width in metres |
| `belt_halfwidth_frac` | fx(0.01, 1) | "0.15" | guess | cap of the half-width as a fraction of the map side (small maps) |
| `segment_noise` | `fbm_map` | cycles "2.5", octaves 2, gain "0.5" | guess | where ranges start and stop along a belt |
| `segment_contrast` | fx(0, 16) | "0.75" | guess | sharpness of range ends |
| `segment_bias` | fx(-1, 1) | "0.25" | guess | share of belt length that carries a range |
| `erosion_iterations` | int(0, 200) | 16 | guess | E8 iterations |
| `erosion_route_interval` | int(1, 64) | 1 | designed | recompute drainage every this many iterations (fallback knob, section 8) |
| `erosion_k` | fx(0, 1) | "0.002" | guess | stream-power coefficient per iteration |
| `erosion_diffusion` | fx(0, 0.25) | "0.05" | guess | hillslope diffusion per iteration; 0.25 is the stability limit of the 4-neighbour filter |
| `land_floor_m` | fx(0.05, 10) | "0.1" | designed | no land cell below this before E9 |

The belt fields decide where mountains may stand (`w_b` and `sg`, E3 and E2); how high they are is E5 to E7. The critic's "should fix" items D7 (circular falloff and edge-aligned coasts), D8 (coast detail at 100 m to 1 km) and D9 (belts on the shoreline) touch E2; if the next round changes E2, it is an amendment to a stable step (open question 3).

### 4.3 Fields of E5 to E7 and E10 (placeholder)

v13's fields, with v13's values, for reference only; the next design round replaces this table. Removed against the first version of P1-01: `continent_ramp`, `belt_fraction`, `weight_continent`, `weight_mountain`, `weight_hills`, `land_curve`, `ocean_curve` and the record `curve_point` with its named checks.

| Field | Type and bounds (v13) | v13 value | Used in |
| --- | --- | --- | --- |
| `mountain_coast_t` | fx(0.01, 16) | 2 | E5 |
| `relief_warp` | `fbm_m` | 3000, 3, "0.5" | E5 |
| `relief_warp_amp_m` | fx(0, 5000) | 150 | E5 |
| `ridge_noise` | `fbm_m` | 2000, 6, "0.5" | E5 |
| `ridge_weight` | fx(0, 4) | 2 | E5 |
| `foothill_fraction` | fx(0, 1) | "0.15" | E5 |
| `mountain_height_m` | fx(0, 2900) | 1900 | E6 |
| `mountain_peak_rank` | fx(0.5, 1) | "0.998" | E6 |
| `mountain_cover_height_m` | fx(0, 2900) | 700 | E6 |
| `mountain_cover_rank` | fx(0.5, 1) | "0.85" | E6 |
| `lowland_height_m` | fx(0, 2000) | 160 | E5 |
| `basin_noise` | `fbm_m` | 12000, 3, "0.5" | E5 |
| `basin_amp_m` | fx(0, 1000) | 12 | E5 |
| `hill_noise` | `fbm_m` | 1500, 6, "0.5" | E5 |
| `roughness_noise` | `fbm_m` | 8000, 2, "0.5" | E5 |
| `roughness_min` | fx(0, 1) | "0.25" | E5 |
| `hill_height_m` | fx(0, 1000) | 90 | E5 |
| `hill_coast_min` | fx(0, 1) | "0.6" | E5 |
| `detail_noise` | `fbm_m` | 250, 3, "0.5" | E5 |
| `detail_amp_m` | fx(0, 100) | 6 | E5 |
| `ocean_depth_m` | fx(0, 2900) | 300 | E5 |
| `lake_min_area_m2` | int(0, 1000000000) | 100000 | E10 |
| `lake_max_area_m2` | int(0, 1000000000) | 3000000 | E10 |
| `lake_min_depth_m` | fx(0, 100) | 2 | E10 |
| `breach_max_depth_m` | fx(0, 3000) | 30 | E10 |
| `fill_slope_dm` | fx(0, 10) | "0.2" | E10 |
| `fill_relief` | fx(0, 1) | "0.25" | E10 |
| `condition_passes` | int(1, 32) | 8 | E10 |

The experimental branch also carries switches for rejected variants (`height_remap`, `land_curve`, `ocean_curve`, `belt_height_scaling`, `precondition`, `fill_mode`); none is part of any version of this spec.

### 4.4 Named checks (P0-03 7.5)

Each an error at the definition's header.

1. (stable) `edge_radial_end − edge_radial_start ≥ 0.01`. Keeps the E2 divisor positive and bounded away from 0 (section 7). Not checked by the experimental code, where end = start is a fatal division by zero and end < start inverts the falloff.
2. (placeholder, v13) `mountain_cover_rank ≤ mountain_peak_rank`.
3. (placeholder, v13) `lake_min_area_m2 ≤ lake_max_area_m2`.

### 4.5 Tuning file (stable fields; placeholder fields marked)

`game/data/worldgen/elevation.toml` and, when the stage is approved in full, the fixture `worldgen/tests/data/geo/worldgen/elevation.toml` (P1-00 6.4). Fields in the order of the schema. The fields below the marker line are v13's and will be replaced by the next round; an implementation of the experiment uses them as written.

```toml
[worldgen.elevation]
land_fraction = "0.40"
edge_margin_frac = "0.03"
continent_warp = { cycles = "1.5", octaves = 3, gain = "0.5" }
continent_warp_amp_frac = "0.2"
continent_noise = { cycles = "3", octaves = 4, gain = "0.55" }
continent_noise_amp = "1.0"
continent_center_jitter = "0.06"
edge_falloff_frac = "0.12"
edge_falloff_depth = 2
edge_noise_amp = "0.15"
edge_radial_start = "0.8"
edge_radial_end = "1.0"
coast_noise = { wavelength_m = 2000, octaves = 5, gain = "0.5" }
coast_noise_amp = "0.1"
island_min_area_m2 = 50000
belt_noise = { cycles = "1.25", octaves = 2, gain = "0.5" }
belt_halfwidth_m = 2000
belt_halfwidth_frac = "0.15"
segment_noise = { cycles = "2.5", octaves = 2, gain = "0.5" }
segment_contrast = "0.75"
segment_bias = "0.25"
# ---- placeholder (v13 relief, E5 to E7; not approved) ----
mountain_coast_t = 2
relief_warp = { wavelength_m = 3000, octaves = 3, gain = "0.5" }
relief_warp_amp_m = 150
ridge_noise = { wavelength_m = 2000, octaves = 6, gain = "0.5" }
ridge_weight = 2
foothill_fraction = "0.15"
mountain_height_m = 1900
mountain_peak_rank = "0.998"
mountain_cover_height_m = 700
mountain_cover_rank = "0.85"
lowland_height_m = 160
basin_noise = { wavelength_m = 12000, octaves = 3, gain = "0.5" }
basin_amp_m = 12
hill_noise = { wavelength_m = 1500, octaves = 6, gain = "0.5" }
roughness_noise = { wavelength_m = 8000, octaves = 2, gain = "0.5" }
roughness_min = "0.25"
hill_height_m = 90
hill_coast_min = "0.6"
detail_noise = { wavelength_m = 250, octaves = 3, gain = "0.5" }
detail_amp_m = 6
ocean_depth_m = 300
# ---- stable (E8, E9) ----
erosion_iterations = 16
erosion_route_interval = 1
erosion_k = "0.002"
erosion_diffusion = "0.05"
land_floor_m = "0.1"
# ---- placeholder (v13 conditioning, E10; not approved) ----
lake_min_area_m2 = 100000
lake_max_area_m2 = 3000000
lake_min_depth_m = 2
breach_max_depth_m = 30
fill_slope_dm = "0.2"
fill_relief = "0.25"
condition_passes = 8
```

The canonical encoding (P0-03 8), and so `content_hash` and the world file, depends on the field set, order and values; the `elevation` layer depends only on the values. This file therefore gives a different world file hash from the experimental branch's file for the same layer hash.

### 4.6 Derived integers (stable)

| Name | Definition | 512 / 1024 / 2048 with defaults |
| --- | --- | --- |
| margin M | max(1, (edge_margin_frac.raw × N) >> 32), in `int64` (< 2^43) | 15 / 30 / 61 |
| I_min (cells) | (island_min_area_m2 + 255) div 256 | 196 |
| floor | `land_floor_m` as `Fx` | raw 429496730 |
| H_MAX | `from_int(2990)`, a constant of the code: the largest land height before E9 | raw 12841952215040 |
| side | `from_int(16 × N)`, the map side in metres | 8192 / 16384 / 32768 m |

## 5. Algorithm

### 5.0 Conventions (stable)

- n = N²; cell (x, y), index i = yN + x; D8 codes and neighbour orders of P1-00 2.3.
- Every value is `Fx` unless stated; every operation is the P0-01 operation of that name: `+` and `−` checked; `mul`, `div`, `div_int`, `mul_div`, `from_ratio` with RHA; `mul_int` exact; `abs`, `min`, `max`, `clamp`, `fade`, `hypot`; `kth` and `quantile` of G1. `0` is `Fx()` (raw 0); `ONE`, `HALF` the P0-01 constants; `from_raw(1)` the smallest positive `Fx`.
- `quantile_rank(q, n)` = min(n − 1, (q.raw × n) >> 32) in `uint64`, fatal if n = 0 (the rank G1's `quantile` uses; P1-00 7.1).
- Statements are evaluated exactly in the order written, one operation at a time with that operation's rounding. Parenthesised sums are evaluated as parenthesised; an unparenthesised chain `a + b + c` is evaluated left to right.
- Steps run in the order E1, E2, E3, E4, E5, E6, E7, E8, E9, E10, E11. Each step names its determinism rule (P1-00 8).

### E1. Coordinates and continent centre (stable; D1)

Per cell:

```
X = from_int(16x + 8)            Y = from_int(16y + 8)            // metres, ≤ 32760
U = from_ratio(2x + 1, 2N)       V = from_ratio(2y + 1, 2N)       // map fraction, in (0, 1), exact for N a power of two
```

Once per world, for k = 0 and 1:

```
hv_k = hash_at(K_continent, k, 0) >> 32                    // uint64 in [0, 2^32)
J_k  = HALF + mul(continent_center_jitter, from_raw((hv_k − 2^31) × 2))   // (hv_k − 2^31) × 2 in int64
cx = J_0;  cy = J_1
```

`from_raw((hv − 2^31) × 2)` lies in [−ONE, ONE − raw 2], so cx and cy lie in [0.25, 0.75). The hash is location-keyed (P0-01 7.3) at the fixed coordinates (0, 0) and (1, 0); it cannot collide with the noise lattice of `fbm_map(K_continent, …)`, which hashes `substream(K_continent, o)`, a different key.

### E2. Pass A: continent, belt and segment fields (stable; D1, parallel by G6 row blocks)

For every cell, in this order:

```
wx  = fbm_map(K_cwx, U, V, continent_warp)
wy  = fbm_map(K_cwy, U, V, continent_warp)
Uw  = U + mul(continent_warp_amp_frac, wx)
Vw  = V + mul(continent_warp_amp_frac, wy)
d   = mul_int(hypot(Uw − cx, Vw − cy), 2)                 // radial cone: 0 at the centre
nc  = fbm_map(K_continent, Uw, Vw, continent_noise)
Uf  = U + div_int(Uw − U, 2)                               // half-warped frame for the falloff
Vf  = V + div_int(Vw − V, 2)
ed  = min(min(Uf, Vf), min(ONE − Uf, ONE − Vf))            // distance to the nearest edge
edn = ed + mul(edge_noise_amp, nc)
fe  = fade(clamp(div(edn, edge_falloff_frac), 0, ONE))     // 0 at the edge, 1 inside the guard band
dm  = mul_int(hypot(Uf − HALF, Vf − HALF), 2) − mul(edge_noise_amp, nc)
fr  = fade(clamp(div(dm − edge_radial_start, edge_radial_end − edge_radial_start), 0, ONE))
edge = mul(edge_falloff_depth, max(ONE − fe, fr))
cd  = fbm_m(K_coast, X, Y, coast_noise)
c   = (((ONE − d) + mul(continent_noise_amp, nc)) − edge) + mul(coast_noise_amp, cd)
nb  = fbm_map(K_belt, Uw, Vw, belt_noise)                  // signed; belts follow nb = 0
ns  = fbm_map(K_segment, Uw, Vw, segment_noise)
sg  = fade(clamp((HALF + segment_bias) + mul(segment_contrast, ns), 0, ONE))
```

Keep three `int64` grids: c, nb and sg (raw). Nothing else from this pass is kept.

Why this shape: the cone `ONE − d` around a jittered centre gives one continent; the warped shape noise of comparable weight moves lobes, bays and peninsulas off the circle; the square guard band (`fe`) and the radial falloff (`fr`) keep land off the edge, both perturbed by the same shape noise so the guard does not draw a square or a circle; the unwarped metre-scale coast noise adds detail at a fixed physical scale (P1-00 2.2). Belts follow the zero lines of `nb`, which are long and curving; `sg` switches ranges on and off along them.

### E3. Thresholds and scales (stable; D7/G1, gradient D2)

1. **Coast threshold.** `c_sea = from_raw(quantile(c over all n cells, ONE − land_fraction))`.
2. **Belt-noise gradient** per cell (D2, G6 row blocks, reading the stored nb grid; one-sided at the map edge):
   ```
   x0 = max(x − 1, 0); x1 = min(x + 1, N − 1); y0 = max(y − 1, 0); y1 = min(y + 1, N − 1)
   gx = div_int(nb[x1, y] − nb[x0, y], x1 − x0)
   gy = div_int(nb[x, y1] − nb[x, y0], y1 − y0)
   g[x, y] = hypot(gx, gy).raw                                  // units of nb per cell
   ```
3. `g_med = from_raw(max(1, kth(g, quantile_rank(HALF, n))))`.
4. **Belt half-width** in metres: `hw = min(belt_halfwidth_m, mul(belt_halfwidth_frac, side))`.
5. **Belt width in units of nb**: `w_b = max(from_raw(1), div_int(mul(g_med, hw), 16))` (hw / 16 m is the half-width in cells; times the median gradient per cell it is the |nb| at that distance from a zero line).
6. **Land scale** `s_land`: the multiset of `c − c_sea.raw` over every cell with `c > c_sea.raw`; `s_land = from_raw(max(1, kth(values, quantile_rank(HALF, count))))`. Fatal "elevation: no land" if count = 0.
7. **Ocean scale** `s_ocean`: the multiset of `c_sea.raw − c` over every cell with `c ≤ c_sea.raw`; same formula. Fatal "elevation: no ocean" if count = 0.

s_land and s_ocean are taken on this provisional split, before E4's margin, pockets and islands. They are used only by E5 (placeholder); if the next relief design does not use them, items 6 and 7 are deleted by that amendment.

The lists in 6 and 7 may be built in any order (G1 is a value). The two fatal errors are checked, not argued unreachable: with `land_fraction` in [0.1, 0.8] the quantile in step 1 leaves cells on both sides unless the field is constant over most of the map, which no seed has produced.

### E4. Coast and ocean (stable; D5, G2)

1. Provisional ocean: P(i) = `c[i] ≤ c_sea.raw` **or** min(x, y, N−1−x, N−1−y) < M.
2. Label the 4-connected components of P with G2. The ocean O is the union of the components that contain at least one cell on the grid edge (x = 0, y = 0, x = W−1 or y = H−1). On the N × N map every edge cell is in P (M ≥ 1), so O is the one component that contains the edge. P-cells not in O (pockets enclosed by land) become land.
3. Label the 8-connected components of the land (cells not in O) with G2. Every component with fewer than I_min cells is added to O. If no component has at least I_min cells: fatal "elevation: no land".

O stays one 4-connected region containing the margin: a sunk component's 4-neighbours outside it are all in O (a land 4-neighbour would belong to the same 8-component), so its cells join O's region. **From here on O is fixed:** no later step changes which cells are ocean.

The function behind this step accepts any W × H grid and any P (test seam, section 10); on a test grid O may be several components, one per edge-touching component of P.

### E5 to E7. Heights, mountain scale, compose (placeholder)

**v13 in outline** (full text in appendix V13): land height = lowland profile `lowland_height_m × co` (co = tl / (1 + tl), tl the continent field above the coast in units of s_land, so half the asymptote at the land median) plus basin undulation, hills scaled by a roughness field and by distance from the coast, fine detail noise, and a mountain term `scale × unit`. `unit` = belt mask (fade of |nb| / w_b) × segment `sg` × coast fade × ridged-noise profile. `scale` (E6) is set from two quantiles of `unit` over land: the 99.8th percentile maps to `mountain_height_m` (1900 m), capped so the 85th percentile maps to at most `mountain_cover_height_m` (700 m). Ocean depth = −(0.5 m + `ocean_depth_m` × to / (1 + to)). E7 clamps land to [`land_floor_m`, 2990 m].

**Critic's blockers against these steps** (review section 7):

- **D1 Unwalkably steep mountains.** Median slope of land above 500 m is 45° to 60° in every measured world; 69% to 92% of it is steeper than 35°. Cause: peaks of 2000 to 2500 m on belts a few kilometres wide. At 512 the belt is capped at 0.15 of the map side (1228.8 m half-width) with the same peak height.
- **D3 Axis-aligned striping on smooth planar flanks**, where single-direction erosion cuts straight down a grid axis (flanks need relief before erosion).
- **D4 Mountain-light worlds** (about a quarter of seeds: one round massif on a plain). The lead has withdrawn acceptance of relaxed bounds for them; the generator must guarantee uplands.
- Should fix: **D5** flanks without spurs or side valleys; **D6** uniform lowland bump texture, almost nothing flatter than 0.5°, no floodplains; **D9** belts on the shoreline and crescents around bowls.

**Open design questions for the next round:** peak height as a function of belt half-width (the rejected v13 variant scaled by √(hw / belt_halfwidth_m)); a floor on mountain coverage per world (at least two belt segments, or a minimum share of land above 500 m) and how it is enforced deterministically (a quantile-driven segment threshold is the natural form); relief on flanks before erosion; a lowland model with flats and distinct hill groups; whether s_land and s_ocean remain.

**Output contract (stable; whatever E5 to E7 become):** a grid h of `Fx` raw values with, for every cell, land (not in O): `land_floor_m ≤ h ≤ H_MAX`; ocean (in O): `−3000 m ≤ h < 0` (raw −12884901888000 ≤ h.raw ≤ −1). Computed by D1, D7/G1 steps only (no new determinism rule without amendment). E8 and E9 below rely only on this contract.

### E8. Erosion (stable; D3, made of D6, D1, D4, D2)

A detachment-limited stream-power model with exponent m = 1/2 and n = 1, solved implicitly along the drainage tree (Braun and Willett 2013, "FastScape"), followed by explicit hillslope diffusion. Land cells only; the ocean is base level at height 0 and never changes. h is the `Fx` grid of the output contract above. The arrays F, rcv and order persist between iterations; hn is a second `Fx` grid.

```
for it in 0 .. erosion_iterations − 1:
    if it mod erosion_route_interval == 0:
        (L, parent, order) = G3 routing flood, keys h.raw, ocean mask O        // D6
        rcv = G4 receivers (L, parent, O)                                       // D1, G6 blocks
        A   = G5 accumulation with G3's order                                   // D4
        for each land cell c:                                                   // D1, G6 blocks
            S    = from_raw(isqrt_u64(A[c] << 32) << 16)                        // √A, 16 fractional bits exact
            F[c] = mul(erosion_k, S)
            if rcv[c] is odd (diagonal): F[c] = mul(F[c], INV_SQRT2)
        for each ocean cell: F[c] = 0
    // implicit solve, D4 in G3's order (every receiver precedes its donors)
    for c in order:
        if c in O: hn[c] = h[c]; continue
        m    = the neighbour of c in direction rcv[c]
        base = (m in O) ? 0 : hn[m]
        v    = (h[c] > base) ? base + div(h[c] − base, ONE + F[c]) : h[c]   // no incision in a depression
        hn[c] = max(v, floor)
    // diffusion, D2, G6 blocks: reads hn, writes h
    for each land cell c:
        S4 = 0
        for each code in the 4-neighbour order (E, S, W, N):
            n = the neighbour of c in direction code
            if n is outside the grid: S4 = S4 + hn[c]      // no flux across the edge (test grids only)
            else if n is land:        S4 = S4 + hn[n]
            else (n is ocean):        S4 = S4 + 0
        h[c] = max(floor, hn[c] + mul(erosion_diffusion, S4 − mul_int(hn[c], 4)))
    for each ocean cell: h[c] = hn[c]
```

Notes:

- On the N × N map land never touches the edge (M ≥ 1), so the edge rule applies only to W × H test grids.
- G3 requires every land key ≥ 1: land h ≥ floor ≥ 0.05 m (raw ≥ 214748365) by the contract, and every update keeps it (`max(…, floor)`).
- The update base + (h − base)/(1 + F) is the exact implicit solution of one step of dh/dt = −K √A (h − h_receiver)/Δx; K, dt and Δx are folded into `erosion_k` (√(256 A)/16 = √A). Large rivers (large F) are cut nearly to their receiver's height in one step; ridges barely change.
- A cell whose receiver is not lower (a depression, where G4 fell back to the flood parent) is not incised, so depressions survive to E10.
- When routing is not recomputed (`erosion_route_interval` > 1), rcv, order and F are reused; `order` still lists every receiver before its donors, so the solve is still valid.
- With `erosion_iterations` = 0 the step does nothing.
- **Maxima do not grow.** The solve gives v between base and h[c] (or h[c]); the diffusion value is hn[c] + RHA(D(S4 − 4hn[c])), whose exact value (1 − 4D)hn[c] + D·S4 with 0 ≤ D ≤ 0.25 is a weighted mean of land values and zeros; an integer RHA of a value ≤ M (M the largest land hn, an integer raw) is ≤ M. So the largest land height never increases and land stays in [floor, H_MAX].

### E9. Quantization (stable; D1, G6 blocks)

```
q = mul_div(h.raw, 10, 2^32)                       // decimetres, RHA
e = (c in O) ? min(−1, q) : max(1, q)              // int16
```

Bounds from the contract and E8: land e ∈ [1, 29900], ocean e ∈ [−30000, −1]; so no cell is 0, every sign matches O, and e fits `i16`. (A value outside [−32768, 32767] would be a fatal error; it cannot occur.)

### E10. Depression conditioning (placeholder)

**v13 in outline** (full text and analysis in appendix V13): on the decimetre grid, up to `condition_passes` passes. Each pass runs G3, labels depression components (G2, 8-connected, joined at equal flood level), and tests each in label order: kept as a lake if lake_min_cells ≤ size ≤ lake_max_cells and depth ≥ lake_min_depth_dm; otherwise, except in the last pass, breached along the flood-parent path from its lowest cell if the deepest cut is at most `breach_max_depth_m`, else partially filled to a surface rising from the outlet by `fill_slope_dm` per BFS step that keeps `fill_relief` of the original depth; in the last pass every failing component is filled flat to its flood level. Stops after a pass with no failing component.

**Critic's blockers:** **D2** polygonal, dead-flat fill plates of up to several km² with straight or right-angled outlines and parallel 45° channels. The cause is in the procedure (appendix V13, findings V2 and V3): the fill surface is a function of BFS hop distance from the outlet, whose level sets in 8-connectivity are squares, and basins that exhaust the passes are filled flat. Also **D10/D11** (straight breach trenches along flood-parent paths; wedge-shaped lakes; no lakes on several 512 maps).

**Open design questions:** a fill surface that follows the basin's own floor with noise, or breaching as the default with a much larger limit; whether depressions larger than `lake_max_area_m2` should be lakes rather than plains; whether a component touched earlier in the same pass is deferred to the next pass (finding V1); whether conditioning should run before erosion as well (the rejected v13 "precondition" variant), so that erosion cuts the outlets.

**Output contract (stable; whatever E10 becomes):** E10 maps the E9 grid e to the final grid e' with:

1. e'[c] = e[c] for every ocean cell; land cells stay in [1, 29990] (so no cell is 0 and every sign is O's).
2. Every depression component of e' (G3 on e', components as in P1-02 H3) passes the stage's lake test, whatever that test is; the test's thresholds live in `worldgen.elevation`.
3. Determinism: D6, D5 and D4 steps only, single-threaded, every order stated.

### E11. Write (stable)

Write `elevation` (`i16`, codec 1) as the stage's only layer, in one stage transaction.

## 6. Invariants

For every seed, N and tuning inside the bounds:

1. (stable) Every value of `elevation` is in [−30000, 30000] and none is 0.
2. (stable) The ocean (e < 0) is 4-connected, contains every cell with min(x, y, N−1−x, N−1−y) < M, and so touches all four edges.
3. (stable) Every 8-connected land component has at least I_min cells.
4. (stable) The land/ocean classification after E4 equals the sign of the final layer.
5. (stable) Land values are in [1, 29990]; ocean values in [−30000, −1].
6. (stable) The layer is a pure function of (seed, N, `worldgen.elevation`): independent of thread count, platform and compiler.
7. (placeholder contract) Every depression component of the layer passes E10's lake test.
8. (stable) With `erosion_iterations` = 0, the layer equals E9 and E10 applied to the E7 output.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| E1 centre | D1 | two location hashes, computed once |
| E1, E2 | D1, parallel by G6 row blocks | location-keyed noise only |
| E3 quantile, medians | D7, G1 | values; any selection method |
| E3 gradient | D2, parallel | reads the complete nb grid, writes g |
| E4 | D5, G2 | sets with labels by G2 |
| E5 to E7 | placeholder | contract: D1 and D7/G1 only |
| E8 routing | D6, single-threaded | pop order (key, seq) |
| E8 receivers, F | D1, parallel | |
| E8 accumulation, solve | D4 in G3 order | the solve is order-dependent and follows `order` exactly |
| E8 diffusion | D2, parallel | reads hn, writes h; S4 summed in the 4-neighbour order |
| E9 | D1, parallel | |
| E10 | placeholder | contract: D6, D5, D4 |

No floating point, no `<cmath>`, no `Rng`, no unordered containers, and no new math function: E1 to E4, E8 and E9 use only P0-01 operations, `hypot`, `fade`, `isqrt_u64` and `INV_SQRT2`.

**Overflow bounds** for N = 2048 and any tuning inside the bounds. Each bound re-derived from the formulas; where the engineer's v13 bound (v13 definition section 4) differs, the difference is noted. Every bound is far below the range of `Fx` (±2^31) unless stated.

- **Noise coordinates.** Map fraction: U, V ∈ (0, 1); |wx| ≤ 1 and amp ≤ 0.5, so Uw, Vw ∈ (−0.5, 1.5). With cycles ≤ 64 and octaves ≤ 10 the tenth octave's frequency is 64 × 2^9 = 32768, so |sample| < 1.5 × 32768 = 49152; the final frequency update after the last octave is 65536. Metres: X, Y ≤ 32760; with wavelength ≥ 64 the tenth octave samples at ≤ 32760 × 2^9 / 64 = 262080, final update 2^10 / 64 = 16. (For the placeholder E5's warped coordinates, qx ≤ 32760 + 5000 gives ≤ 302080.) All far below 2^31.
- **E1.** (hv − 2^31) × 2 ∈ [−2^32, 2^32 − 2] fits `int64`; J ∈ [0.25, 0.75).
- **E2.** |Uw − cx| < 1.5 − 0.25 = 1.25, so hypot < 1.768 and d < 3.536. |nc| ≤ 1. |Uw − U| ≤ 0.5, so Uf, Vf ∈ (−0.25, 1.25) and ed ∈ (−0.25, 0.5] (engineer: |ed| ≤ 1.25, loose). |edn| ≤ 0.5 + 0.25 = 0.75 (engineer: 1.5, loose); with `edge_falloff_frac` ≥ 0.01 the ratio before the clamp is ≤ 75 in magnitude (engineer: 150). |Uf − 0.5| < 0.75, so the radial hypot < 1.061, dm ∈ (−0.25, 2.372); dm − start ∈ (−2.25, 2.372); with named check 1 the divisor is ≥ 0.01, so the ratio is ≤ 237.2 in magnitude (engineer: 440, loose; without the check the divisor may be 0 or negative, which is why the check is required). edge ∈ [0, 8]. c ∈ (1 − 3.536 − 4 − 8 − 2, 1 + 4 + 2] = (−16.54, 7] (engineer: |c| < 19, loose). |(HALF + bias) + contrast × ns| ≤ 1.5 + 16 = 17.5. All `fade` arguments are clamped to [0, ONE].
- **E3.** |nb| ≤ 1, so each difference ≤ 2, |gx|, |gy| ≤ 2 and g ≤ 2.829. side ≤ 32768 m; hw ≤ 20000 m; mul(g_med, hw) ≤ 56569 (< 2^16); w_b ≤ 3536. c − c_sea and c_sea − c are below 23.54 in magnitude (engineer: < 38, loose), raw < 2^37. quantile products q.raw × n ≤ 2^32 × 2^22 = 2^54.
- **E4.** Integer labels; ≤ 2^22 cells.
- **E8.** A ≤ N² = 2^22, so A << 32 ≤ 2^54 and isqrt ≤ 2^27, << 16 ≤ 2^43; S ≤ 2048, F ≤ 2048 (`erosion_k` ≤ 1), ONE + F fits. |h − base| ≤ 2990 m. S4 ∈ [0, 4 × 2990 m] and 4hn ∈ [0.2, 11960 m], so |S4 − 4hn| ≤ 11960 m (engineer and first version: 24000 m, loose; the tighter figure uses the E8 maximum property). Priority keys are raw heights < 2^44.
- **E9.** |h.raw × 10| ≤ 3000 × 2^32 × 10 < 2^45 inside `mul_div`'s 128-bit product; land e ≤ 29900, ocean e ≥ −30000.

## 8. Performance (stable)

Budget unchanged (P1-00 11): 2 / 8 / 35 s at 512 / 1024 / 2048 on the 6-core development machine, release build.

Measured on the first implementation at 1024 with six threads: about 5 s for the stage, of which the noise passes about 2.2 s and erosion (16 iterations) about 2.4 s; quantiles, classification, quantization and conditioning share the rest. v13 adds noise: E2 evaluates 19 `noise2` octaves per cell (warp 3 + 3, continent 4, coast 5, belt 2, segment 2) against 12 in the first version, and the placeholder E5 about 20 per land cell plus 6 ridged octaves where the mountain mask is positive. v13's own timings were not recorded in the definition file; expect the noise passes to grow by about half, which keeps the stage well inside 8 s at 1024. The validation specialist measures and reports per step.

Erosion was chosen as a grid method with an implicit tree solve rather than droplets: droplet erosion is sequential and order-sensitive, needs millions of droplets with interpolated gradients and has no deterministic parallelism. The stream-power solve is O(N²) per iteration with a defined order and is stable for any step size.

If erosion is too slow, in this order:

1. **Without changing output:** a radix or bucket heap for the flood (pop order is defined by (key, seq), not by the structure); a table for `isqrt_u64` of A < 2^16; the basin-parallel solve of P1-00 section 8 (each drainage basin's cells solved by one thread in `order`; basins write disjoint cells).
2. **Tuning only:** `erosion_route_interval` 2 or 4, then fewer iterations with a larger `erosion_k`. Changes output; a shipped tuning change (PATCH bump).
3. **Algorithm change (spec amendment):** erode a 2:1 downsampled grid at 2048 and add the difference back bilinearly.

## 9. Database rows written (stable)

One `layer` row: `name` = `elevation`, `width` = `height` = N, `format` = `i16`, codec 1, in one stage transaction; its id is the next layer id (1 in a normal world). No schema change: `SCHEMA_VERSION` stays 1.

## 10. Tests required

Library seams (interim names, as on the experimental branch): `elevation_fields(seed, N, tuning, threads)` returning c, nb, sg as raw grids and c_sea, w_b, s_land, s_ocean (E1 to E3); `elevation_ocean_from_provisional(grid, P, I_min)` (E4 steps 2 and 3 on any W × H grid); `elevation_classify(fields, tuning)` (E4 on the map); `erode(grid, h, ocean, params, threads)` with its parts `erosion_factors`, `erosion_solve`, `erosion_diffuse` (E8 on any W × H grid with at least one ocean cell); `quantize_elevation` (E9); `elevation_grid(seed, N, tuning, threads)` for the whole stage. The seams of E5 to E7 and E10 are placeholders.

### 10.1 Unit tests with worked examples (stable steps)

Each figure below was computed with a throwaway exact-integer script that implements the P0-01 operations; marked "verified".

**Derived integers** (verified). margin M for N = 512, 1024, 2048 with "0.03" (raw 128849019) is 15, 30, 61; with "0" it is 1. I_min for 50000 m² is 196; for 0, 0; for 256, 1; for 257, 2. `land_floor_m` "0.1" is raw 429496730.

**Coordinates** (verified). N = 1024: U at x = 0 is raw 4194304 (1/2048), at x = 1023 raw 4290772992 (2047/2048); N = 16: U at x = 3 is `from_ratio(7, 32)` = raw 939524096. X at x = 2047 is `from_int(32760)`.

**Continent centre** (structural, verified arithmetic). With `continent_center_jitter` "0.06" (raw 257698038): hv = 2^31 gives J = HALF exactly; hv = 0 gives unit = −ONE and J = 2147483648 − 257698038 = raw 1889785610; hv = 2^32 − 1 gives unit raw 4294967294 and J = HALF + mul(257698038, 4294967294) = 2147483648 + 257698038 = raw 2405181686 (the product 257698037.88 rounds up). For seed 0 the test computes hv_0 and hv_1 from `hash_at` and checks cx, cy returned by the seam.

**E2 structural.** For seed 0, N = 64 and the shipped tuning, at 32 cells chosen by the test (including the four corners and cells on the row and column N/2), c, nb and sg returned by the seam equal the values the test computes from `fbm2`, `hypot`, `fade` and the statements of E2, in order.

**E3 coast threshold and scales** (verified). Ten cells with c raw values 5, −3, 2, 0, 8, −1, 4, −7, 1, 3 (as raw integers) and `land_fraction` "0.4" (raw 1717986918): q = ONE − 0.4 = raw 2576980378, rank (2576980378 × 10) >> 32 = 6; sorted −7, −3, −1, 0, 1, 2, 3, 4, 5, 8, so c_sea = raw 3. Land values (c > 3): 1, 2, 5; rank (HALF × 3) >> 32 = 1, s_land = raw 2. Ocean values (c ≤ 3): 10, 6, 4, 3, 2, 1, 0; rank 3, s_ocean = raw 3. (The seam takes c directly for this test.)

**E3 belt gradient** (verified). A 4 × 4 nb grid, values in units of 1/64 (raw multiples of 67108864), rows y = 0 to 3: `0 4 8 12`, `2 6 10 14`, `4 8 12 16`, `6 10 14 30`. At (0, 0): gx = 4/64 (one-sided, divisor 1), gy = 2/64, g = hypot = raw 300119964 (√20 / 64). At (1, 1): gx = (10 − 2)/2 = 4/64, gy = (8 − 4)/2 = 2/64, the same. The last row's g values are 300119964, 300119964, 684378814, 1426754019 and (3, 2) is 600239927; the other eleven cells are 300119964. Rank quantile_rank(HALF, 16) = 8, so g_med = raw 300119964. With hw = 2000 m (raw 8589934592000): mul(g_med, hw) = raw 600239928000, w_b = div_int(·, 16) = raw 37514995500 (8.7346 units of nb: 125 cells times 0.0699 per cell).

**Belt half-width cap, 512 against 1024** (verified). `belt_halfwidth_frac` "0.15" is raw 644245094. At N = 512, side = 8192 m and mul(644245094, from_int(8192)) = raw 5277655810048 (1228.7999992 m, exact product, no rounding), below 2000 m, so hw = raw 5277655810048. At N = 1024 the product is raw 10555311620096 (2457.6 m) and hw = `belt_halfwidth_m` = raw 8589934592000; at 2048 likewise 2000 m. With g_med = "0.01" (raw 42949673): w_b at 512 = div_int(mul(42949673, 5277655810048), 16) = div_int(52776558150, 16) = raw 3298534884; at 1024 and 2048 = div_int(85899346000, 16) = raw 5368709125. So a 512 belt is 0.614 times as wide as a 1024 belt, in metres.

**E4 on a given mask** (verified). N = 8, M = 1, I_min = 2, provisional ocean P ('.') and land ('#'):

```
     x: 0 1 2 3 4 5 6 7
y0      . . . . . . . .
y1      . # # # . . # .
y2      . # . # # . . .
y3      . # # # # # . .
y4      . . # # # # . .
y5      . . . # # . . .
y6      . . . . . # . .
y7      . . . . . . . .
```

Expected: (2, 2) is not 4-connected to the edge through P and becomes land; (6, 1) is an 8-component of one cell and is sunk; (5, 6) touches (4, 5) diagonally and stays. Final land: 19 cells, (1..3, 1), (1..4, 2), (1..5, 3), (2..5, 4), (3..4, 5), (5, 6). Also: a 5 × 5 grid whose only P-cells are the border and (2, 2) gives O = the border only; and on a test grid with two edge-touching P components both are ocean.

**E8 solve, by hand** (verified). A 5 × 1 grid, cell 0 ocean, cells 1 to 4 land with heights 10, 20, 40, 80 m; `erosion_k` = HALF, `erosion_diffusion` = 0, floor 0.1 m, one iteration. Every land cell's receiver is its west neighbour (code 4). A = 4, 3, 2, 1. S raw = 8589934592, 7439056896, 6073942016, 4294967296; F raw = 4294967296, 3719528448, 3036971008, 2147483648. Results (raw): cell 1: 21474836480 (5 m exactly); cell 2: 55999923266; cell 3: 123833544243; cell 4: 270342770534 (about 13.0385, 28.8322, 62.9441 m).

**Diagonal factor** (verified). A = 4, `erosion_k` = HALF, diagonal receiver: F raw = mul(4294967296, 3037000500) = 3037000500.

**Diffusion** (verified). One land cell with hn = 10 m whose 4-neighbours are ocean, 20, 30 and 40 m; `erosion_diffusion` "0.1" (raw 429496730): S4 = 90 m, S4 − 4hn = 50 m, result raw 64424509460 (15.0000000047 m).

**Edge rule** (verified). A 3 × 1 grid: ocean, land 10 m, land 20 m at the east edge; `erosion_k` 0 (so the solve leaves h unchanged), `erosion_diffusion` "0.1", one iteration. For the east cell, in the order E, S, W, N: E outside +20, S outside +20, W land +10, N outside +20, so S4 = 70 m, S4 − 4 × 20 m = −10 m, mul(429496730, −10 × 2^32) = −4294967300, result raw 85899345920 − 4294967300 = 81604378620 (just under 19 m).

**E9 quantization** (verified). Land 50.5 m (raw 216895848448) → 505; land 0.04 m (raw 171798692) → q 0 → 1; land 0.05 m (raw 214748365) → 1; ocean −0.05 m (raw −214748365) → −1; ocean raw −1 → q 0 → −1; land 12.345 m (raw 53021371269) → 123; land 2990 m → 29900; ocean −3000 m → −30000.

**Tuning validation.** Named check 1 fails on `edge_radial_start` "1.0", `edge_radial_end` "1.005" and on end below start; a float literal in any field is a P0-03 error; a missing field is an error; each bound of 4.2 rejects a value one step outside it.

### 10.2 Property tests (stable parts)

Over seeds 0 to 63 at N = 128 and seeds 0 to 3 at N = 512, shipped tuning, by code independent of the stage (the test computes components, floods and flood levels with its own straightforward implementations; a Dijkstra-style minimax search for flood levels is acceptable):

- Invariants 1 to 6 and 8 of section 6 on the output.
- E4 alone: O is 4-connected, contains the margin, and every land 8-component has at least I_min cells; every P-cell not in O is land; no non-P cell is ocean unless it belongs to a sunk component.
- E3 values equal those found by sorting a copy.
- E8 alone, on any input meeting the E5 to E7 contract: no land cell is raised by the solve; every land cell after the loop is ≥ floor; ocean cells are unchanged; the largest land height does not increase.

### 10.3 Determinism tests (stable)

- Threads (D10): `elevation_fields`, `erode` and `elevation_grid` for seeds 0 and 42 at N = 128 and 512 with 1, 2 and 5 threads give identical grids.
- Repeat: two runs in one process give identical grids (no hidden state).
- Map sizes are independent worlds: the layer for seed 0 at 512 does not depend on whether a 1024 world was generated first in the same process.

### 10.4 Golden values

Recorded by the validation specialist only once the stage is approved in full: the codec-independent layer hash of P0-04 8 for seeds 0, 42 and the phrase "The Unmapped" at N = 512, seed 1 at N = 1024, seed 0 at N = 128 (library). Available now and recordable for the stable part: the SHA-256 of the c, nb and sg grids (little-endian `int64`) and of the E4 ocean mask (one byte per cell) for seed 0 at N = 128 and 512, which isolates the outline from the relief.

### 10.5 Placeholders

The worked examples and property tests of E5 to E7 (mountain scale from two quantiles, composed land and ocean heights) and of E10 (kept lake, breach crossing a second depression, partial fill, final flat fill), and the statistics ranges of P1-00 13.1, come with the next design.

## 11. Open questions

1. **What the next relief round must deliver** to replace the placeholders: E5 to E7 statement by statement with their tuning fields, bounds and the output contract of 5.5 met; E10 with the contract of 5.10 met, its invariants argued, and answers to findings V1 to V5 of appendix V13; measured slope statistics (P1-00 10.3) for the sweep.
2. **Depression thresholds live in `worldgen.elevation`**, not in hydrology, so the stored elevation has only lake-worthy depressions and hydrology is pure analysis (P1-02 section 9). Unchanged from the first version.
3. **E2 and the critic's D7, D8, D9.** The outline is frozen, but the critic asks for a less circular falloff and a seeded offset or elongation of the land mask (D7), metre-scale coast detail at 100 m to 1 km (D8) and belts kept off the edge band (D9). Each would change E2 and so every world. Interim: E2 as written; any such change is an amendment to a stable step.
4. **No sediment deposition and no uplift.** The erosion model only cuts. Floodplains (critic D6) may need a deposition term in E8; that would be an amendment to a stable step.
5. **Map size.** Belts are map-scale (P1-00 2.2) while belt width is capped in metres; how peak height follows belt width at 512 is for the next round.

## Appendix V13. The superseded v13 relief and conditioning (not approved)

A record of what candidate v13 (commit `3396de5`) does in E5 to E7 and E10, so the next round can see exactly what was rejected and why. Notation of 5.0. `rc(a, b)` for a ≥ 0, b > 0 = `from_int(1024)` if a ≥ mul_int(b, 1024), else div(a, b).

### V13.1 E5 to E7

```
E5, for every cell, s = from_raw(c) − c_sea:
  ocean:  to = rc(max(−s, 0), s_ocean); od = div(to, ONE + to); h = −(HALF + mul(ocean_depth_m, od)); unit = 0
  land:   tl = rc(abs(s), s_land); co = div(tl, ONE + tl)
          un = fbm_m(K_basin, X, Y, basin_noise)
          base = mul(lowland_height_m, co) + mul(mul(basin_amp_m, un), co)
          anb = abs(nb); b = (anb ≥ w_b) ? 0 : fade(ONE − div(anb, w_b))
          gm = fade(clamp(div(tl, mountain_coast_t), 0, ONE))
          mask = mul(mul(b, sg), gm)
          qx = X + mul(relief_warp_amp_m, fbm_m(K_rwx, X, Y, relief_warp))
          qy = Y + mul(relief_warp_amp_m, fbm_m(K_rwy, X, Y, relief_warp))
          unit = (mask > 0) ? mul(mask, foothill_fraction + mul(ONE − foothill_fraction,
                                   ridged_m(K_ridge, qx, qy, ridge_noise, ridge_weight))) : 0
          hh = fbm_m(K_hills, qx, qy, hill_noise); rn = fbm_m(K_roughness, X, Y, roughness_noise)
          rho = roughness_min + mul(ONE − roughness_min, fade(clamp(HALF + rn, 0, ONE)))
          hf = hill_coast_min + mul(ONE − hill_coast_min, co)
          hills = mul(hill_height_m, mul(mul(hh, rho), hf))
          det = mul(detail_amp_m, fbm_m(K_detail, X, Y, detail_noise))
          h0 = (base + hills) + det
E6: Ul = unit over land (zeros included); q = quantile(Ul, mountain_peak_rank); qc = quantile(Ul, mountain_cover_rank)
    cap = mul_int(mountain_height_m, 3); ps = (mul_int(q, 3) > ONE) ? div(mountain_height_m, q) : cap
    scale = (qc > 0 and mul(ps, qc) > mountain_cover_height_m) ? div(mountain_cover_height_m, qc) : ps
E7: land h = clamp(h0 + mul(scale, unit), land_floor_m, H_MAX); ocean keeps E5's h.
```

### V13.2 E10

Keys k = e (int64), ocean O; k changes in place. lake_min_cells and lake_max_cells = (area + 255) div 256; lake_min_depth_dm and breach_max_dm = mul_div(m.raw, 10, 2^32); fill_slope_raw = fill_slope_dm.raw; FILL_MAX = 29990.

```
for pass in 0 .. condition_passes − 1:
    last = (pass == condition_passes − 1)
    (L, parent, order) = G3 on k, O;  pos[c] = index of c in order          // computed once per pass
    components = G2 on {land c : L[c] > k[c]}, 8-connected, joined only at equal L   // once per pass
    any_failed = false
    for each component C in label order:
        depth = max over C of (L[c] − k[c])                     // current k, pass-start L
        if lake_min_cells ≤ |C| ≤ lake_max_cells and depth ≥ lake_min_depth_dm: continue
        any_failed = true
        outlet = cell of C with the smallest pos; Lv = L[outlet]
        if last: k[c] = Lv for every c in C; continue
        pit = cell of C with the smallest current k, ties by smallest pos
        walk 1: run = k[pit]; cut = 0; cur = pit
                repeat: next = neighbour of cur in direction parent[cur]; stop if next in O
                        cut = max(cut, k[next] − run); run = min(run, k[next]); cur = next
        if cut ≤ breach_max_dm:
            walk 2: run = k[pit]; cur = pit
                    repeat: next = …; stop if next in O; k[next] = min(k[next], run); run = k[next]; cur = next
            continue
        BFS (G2 order, FIFO, 8-neighbour order) over the cells of C from outlet; dist = BFS depth
        for each c in C:   inc = (fill_slope_raw × dist[c] + 2^32 − 1) >> 32
                           keep = mul_div(fill_relief.raw, Lv − k[c], 2^32)
                           k[c] = max(k[c], min(FILL_MAX, (Lv + inc) − keep))
    if not any_failed: stop
```

What holds (argued by the spec writer; usable by the next design):

- **Termination**: at most `condition_passes` passes; each walk follows flood parents, whose pop positions strictly decrease, to an ocean cell; BFS visits each cell of C once.
- **Signs, ocean, bounds**: ocean keys are never read as targets or written (walks stop before the first ocean cell; components hold land only). Walk 2 sets keys to a running minimum of land keys (≥ 1); fills only raise (`max` with the old key) and cap at 29990; a flat fill sets Lv, a land flood level, which is ≤ the largest land key. So land stays in [1, 29990] and O is untouched.
- **Final depressions pass the test.** The last executed pass either found no failing component (and changed nothing, so its L is current) or is the last pass, which only fills failing components flat to their own level. A flat fill to L[c] changes no flood level: keys only rise, so L cannot fall; and for any cell x, every cell c on x's optimal path has L[c] ≤ L[x], so its new key L[c] keeps that path's maximum at L[x]. The depressions after the pass are then exactly the passing components, measured with unchanged k and current L.
- **The breach cut is at least the depth.** A cell of C has its flood parent in C or on C's rim (a non-depressed cell with key Lv); so walk 1 reaches a key Lv while run = k[pit], and cut ≥ Lv − k[pit] = depth. A breach is possible only for depth ≤ breach_max_dm. After walk 2 the path from the pit is non-increasing to the sea.

Findings (holes, uncovered cases, discrepancies):

- **V1 In-pass interaction on stale levels.** L and parents are fixed at the start of a pass, k changes in place. A breach of component C1 may lower cells of a later component C2. If C2 then fails and cannot breach, its partial fill uses `keep = fill_relief × (Lv2 − k)` with the lowered k and raises the lowered channel cells back towards Lv2, partly re-damming C1's breach. The result is still valid (re-tested next pass; the last pass fixes it), but a basin can flip between breached and filled across passes and end in the flat fill. Equally, a component may be kept in a pass on its stale level after its rim was lowered. Neither case is covered by the definition's text; a redesign should either defer any component that has a cell changed earlier in the pass, or recompute levels after each breach.
- **V2 The fill raises cells above the spill level.** Away from the outlet `(Lv + inc) − keep` exceeds Lv wherever keep < inc, so a partial fill builds a ramp above the basin's spill level. Cells outside C that drained through C can become new depressions behind it. Again valid but it manufactures depressions, which consume passes and end flat.
- **V3 The fill contour is the BFS metric.** `dist` is the 8-connected hop count from the outlet (a Chebyshev distance), whose level sets are squares. The fill surface's contours are therefore squares and 45° lines centred on the outlet, and drainage on it runs in parallel straight lines: the cause of the critic's D2 plates and channels.
- **V4 Exhausted passes give dead-flat plates.** Any basin still failing in the last pass (typically large, deep basins that cannot breach) is filled flat, and basins over `lake_max_area_m2` always fail. The lake test's upper bound thus converts every large basin into a plain.
- **V5 `fill_slope_dm` = 0** is inside the bounds. The definition's formula then gives a flat surface at Lv minus the kept relief; the experimental code instead takes the flat-fill branch (no relief kept) when `fill_slope_dm` = 0 (and when its rejected `fill_mode` is 0). The two differ for every such tuning; v13's value 0.2 does not reach the difference. A redesign must choose.
- **V6 Breach trenches are straight.** Walk 2 follows G3 flood parents, which follow the push order of the flood, not the terrain's fall line; the trenches are D8 staircases (critic D10).
- **V7 Bound in the engineer's text.** "Lv + inc ≤ 29900 + 2^26": after an earlier pass's fill, keys and so Lv may be up to 29990, not 29900. The conclusion (fits `int64`) stands; inc ≤ ceil(10 × 2^22) = 41943040.

v13 reference hashes (layer hash of `hash --detail`, N = 1024, seeds 0 to 9) are in the engineer's definition file, section 10; they are a record of the candidate, not golden values.
