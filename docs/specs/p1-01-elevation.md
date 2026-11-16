# P1-01 Elevation

Status: **third version, complete; awaiting approval.** Spec writer, 2026-11-16. Phase P1 Geography, stage 1 (`elevation`). Design doc section 4 (stage 1: "layered noise shaped by a continent mask, then hydraulic erosion"). Depends on P0-01 to P0-04 and P1-00, whose sections are cited as "P1-00 n" and whose shared algorithms G1 to G6 and determinism rules D1 to D10 are used by name.

This version specifies design candidate **v16** of the engineer's exploration exactly: branch `p1/elevation-explore`, commit `ef1f734`, file `worldgen/src/stages/elevation.cpp` with the v16 values of `game/data/worldgen/elevation.toml`, taking only the path that v16's values select. It replaces the second version, in which E5 to E7 and E10 were placeholders. The design decisions behind it are Paul's and the lead's (`docs/design/proposals.md`: "High peaks may be inaccessible; history opens the passes", "Every world has real uplands", "Natural passes through the ranges") and the world critic's reviews of v13 and v15 (`docs/reviews/p1-01-elevation-v13.md`, `docs/reviews/p1-01-elevation-v15.md`). The statements of section 5 were checked against the code by an independent exact-integer reference written from this text; it reproduces the exploration build's grids bit for bit (section 10.4).

## 1. Purpose

Produce the `elevation` layer: one continent per world that reads as a continent. A coastline with bays and peninsulas; long mountain ranges laid out along curving belts, with peaks up to about 2,500 m whose high ground is mostly steeper than a walker can climb (ranges are barriers; history opens the passes); broad lowlands; lakes in basins, including tarns among the peaks; and drainage to the sea everywhere else. Every world has real uplands: where a world's belts carry less than 8% of its land in the mountain core, the ranges are lengthened along their belt, or the belt is moved to an inland level set, until they do, as far as the tried offsets allow (E5.1).

The stage builds a continent field from warped map-scale noise, a radial and square edge falloff and metre-scale coast noise; sets the coast so that a target share of the map is land; lays out mountain belts along a level set of a second map-scale noise, interrupted by a segmentation noise, and moves or lengthens them if a world has too little mountain core (E5.1); builds heights in metres (E5 to E7); deepens the sea with distance from land; carves the land with stream-power erosion along randomized steepest-descent receivers, hillslope diffusion and a talus step; quantizes to decimetres; and conditions depressions so that every remaining one is a lake and everything else drains to the sea (E10). Its output is the final terrain: no later stage changes it (P1-02 section 9).

## 2. Inputs

| Input | Source |
| --- | --- |
| World seed (`uint64`) | `meta.seed` |
| Map side N | `param.map_size` (512, 1024, 2048); library and tests: any 16 ≤ N ≤ 2048 |
| Tuning | definition `worldgen.elevation` (section 4) |
| Earlier layers | none |

Streams. Each gives a key K_name = `stream_key(seed, "geo.elevation.<name>")`, used only as a noise key (`noise2`, `fbm2`) or a location-hash key (`hash_at`). No `Rng` is used anywhere in this stage.

| Key | Stream name | Used in |
| --- | --- | --- |
| K_cwx | `geo.elevation.continent_warp_x` | E2 |
| K_cwy | `geo.elevation.continent_warp_y` | E2 |
| K_continent | `geo.elevation.continent` | E1 (centre hash), E2 |
| K_belt | `geo.elevation.belt` | E2 |
| K_coast | `geo.elevation.coast` | E2 |
| K_segment | `geo.elevation.segment` | E2 |
| K_rwx | `geo.elevation.relief_warp_x` | E5.2 |
| K_rwy | `geo.elevation.relief_warp_y` | E5.2 |
| K_ridge | `geo.elevation.ridge` | E5.2 |
| K_hills | `geo.elevation.hills` | E5.2 |
| K_roughness | `geo.elevation.roughness` | E5.2 |
| K_basin | `geo.elevation.basin` | E5.2 |
| K_detail | `geo.elevation.detail` | E5.2, E10.0 (bump) |
| K_erosion | `geo.elevation.erosion` | E8.2 (random receivers) |
| K_fill | `geo.elevation.fill` | E10.0 (cost) |

All fifteen are registered in `streams.hpp` (P0-01 7.3). Changing any of them changes every world. The exploration branch also registers `geo.elevation.lake`; v16 does not use it and it is not registered by the implementation.

## 3. Outputs

One layer, `elevation`: `i16`, N × N, codec 1, decimetres above sea level (P1-00 4). Land values in [1, 29990], ocean values in [−29005, −1] for any tuning inside the bounds of section 4 ([−3005, −1] with the shipped `ocean_depth_m`), never 0. Negative cells are ocean, positive cells land (including lake beds). Postconditions in section 6. No schema change.

## 4. Tuning

All fields of `worldgen.elevation` are required (P1-00 6.1). "Designed" values follow from the design or the algorithm; "guess" values are tuned against the seed sweeps (v13 to v16 rounds).

### 4.1 Noise records and noise functions

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

Every statement is evaluated as written, in this order, with P0-01 rounding; `amp` and `f` are updated after the last octave too, as in `fbm2`. `fbm_map` samples at map-fraction coordinates, `fbm_m` and `ridged_m` at metre coordinates.

**Normalization.** `fbm2` does not reach its nominal ±1 (about ±0.67 measured). The stage never relies on its range: the coast is a quantile of the continent field (E3), the belt width is scaled by the median gradient of the belt noise (E3), and the mountain heights are scaled from quantiles (E6). What still depends on the spread is the balance between terms, which is tuning.

### 4.2 Fields

In schema declaration order (this order fixes the canonical encoding, P0-03 8).

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `land_fraction` | fx(0.1, 0.8) | "0.40" | designed | share of cells above the coast threshold, before E4's margin, pockets and islands |
| `edge_margin_frac` | fx(0, 0.25) | "0.03" | designed | band along every edge forced to ocean, fraction of N (at least 1 cell) |
| `continent_warp` | `fbm_map` | cycles "1.5", octaves 3, gain "0.5" | guess | domain warp of the continent field |
| `continent_warp_amp_frac` | fx(0, 0.5) | "0.2" | guess | warp displacement per unit of noise, fraction of the map side |
| `continent_noise` | `fbm_map` | cycles "3", octaves 4, gain "0.55" | guess | shape noise: bays, peninsulas, off-centre lobes |
| `continent_noise_amp` | fx(0, 4) | "1.0" | guess | weight of the shape noise against the radial cone |
| `continent_center_jitter` | fx(0, 0.25) | "0.06" | guess | largest offset of the cone centre from the map centre per axis, map fraction |
| `edge_falloff_frac` | fx(0.01, 0.5) | "0.12" | guess | width of the square guard band, map fraction |
| `edge_falloff_depth` | fx(0, 8) | 2 | guess | strength of the edge falloff, units of the continent field |
| `edge_noise_amp` | fx(0, 0.25) | "0.15" | guess | perturbation of both falloff distances by the shape noise |
| `edge_radial_start` | fx(0, 2) | "0.8" | guess | radial distance (1 = an edge midpoint) where the radial falloff starts |
| `edge_radial_end` | fx(0.01, 2) | "1.0" | guess | radial distance where it is complete |
| `coast_noise` | `fbm_m` | wavelength 2000, octaves 5, gain "0.5" | guess | metre-scale coastline detail, unwarped |
| `coast_noise_amp` | fx(0, 2) | "0.1" | guess | its weight in the continent field |
| `island_min_area_m2` | int(0, 1000000000) | 50000 | guess | land components smaller than this are sunk |
| `belt_noise` | `fbm_map` | cycles "1.25", octaves 2, gain "0.5" | guess | mountain belts follow a level set of this noise |
| `belt_halfwidth_m` | fx(100, 20000) | 2000 | guess | belt half-width in metres |
| `belt_halfwidth_frac` | fx(0.01, 1) | "0.15" | guess | cap of the half-width, fraction of the map side (binds at 512) |
| `segment_noise` | `fbm_map` | cycles "2.5", octaves 2, gain "0.5" | guess | where ranges start and stop along a belt |
| `segment_contrast` | fx(0, 16) | "0.75" | guess | sharpness of range ends |
| `segment_bias` | fx(−1, 1) | "0.25" | guess | share of belt length that carries a range; the floor's lower bias limit |
| `segment_on_share` | fx(0, 1) | "0.08" | designed | mountain floor: least share of land in the mountain core (E5.1) |
| `segment_floor_mask` | fx(0.01, 1) | "0.25" | guess | mask level that defines the mountain core |
| `belt_offset_steps` | int(0, 8) | 4 | guess | belt offsets tried on each side of the zero line |
| `floor_inland_gm` | fx(0, 1) | "0.9" | guess | offsets are scored only by core cells with coast factor gm ≥ this |
| `mountain_coast_t` | fx(0.01, 16) | 2 | guess | continent-field distance (in land medians) at which mountains reach full strength |
| `relief_warp` | `fbm_m` | wavelength 3000, octaves 3, gain "0.5" | guess | warp of the ridge and hill noise |
| `relief_warp_amp_m` | fx(0, 5000) | 150 | guess | warp amplitude, metres |
| `ridge_noise` | `fbm_m` | wavelength 2000, octaves 6, gain "0.5" | guess | ridge spacing and detail |
| `ridge_weight` | fx(0, 4) | 2 | guess | multifractal weight |
| `foothill_fraction` | fx(0, 1) | "0.15" | guess | mountain base under the ridges |
| `mountain_height_m` | fx(0, 2900) | 1900 | guess | height at `mountain_peak_rank` |
| `mountain_peak_rank` | fx(0.5, 1) | "0.998" | guess | |
| `mountain_cover_height_m` | fx(0, 2900) | 700 | guess | largest height at `mountain_cover_rank` |
| `mountain_cover_rank` | fx(0.5, 1) | "0.85" | guess | |
| `lowland_height_m` | fx(0, 2000) | 160 | guess | lowland asymptote |
| `basin_noise` | `fbm_m` | wavelength 12000, octaves 3, gain "0.5" | guess | basins and swells |
| `basin_amp_m` | fx(0, 1000) | 12 | guess | |
| `hill_noise` | `fbm_m` | wavelength 1500, octaves 6, gain "0.5" | guess | |
| `roughness_noise` | `fbm_m` | wavelength 8000, octaves 2, gain "0.5" | guess | where land is hilly |
| `roughness_min` | fx(0, 1) | "0.25" | guess | hill factor on the plainest land |
| `hill_height_m` | fx(0, 1000) | 90 | guess | |
| `hill_coast_min` | fx(0, 1) | "0.6" | guess | hill factor at the coast |
| `detail_noise` | `fbm_m` | wavelength 250, octaves 3, gain "0.5" | guess | fine relief; also the fill bump (E10.0) |
| `detail_amp_m` | fx(0, 100) | 6 | guess | |
| `ocean_depth_m` | fx(0, 2900) | 300 | guess | asymptotic ocean depth below the 0.5 m minimum |
| `ocean_shelf_m` | fx(0, 50000) | 2000 | guess | distance from land at which the sea is half its asymptotic depth |
| `erosion_iterations` | int(0, 200) | 24 | designed (lead: 6× v13's stream-power work) | E8 iterations |
| `erosion_route_interval` | int(1, 64) | 1 | designed | recompute drainage every this many iterations (fallback knob, section 8) |
| `erosion_k` | fx(0, 1) | "0.008" | designed (lead) | stream-power coefficient per iteration |
| `erosion_diffusion` | fx(0, 0.25) | "0.0333" | designed (lead) | hillslope diffusion per iteration (v13's total over the run); 0.25 is the stability limit |
| `talus_slope` | fx(0.05, 4) | 3 | designed (lead) | talus tangent (3 is 71.6°) |
| `talus_rate` | fx(0, 0.0625) | "0.0625" | guess | share of the excess over the talus drop moved per iteration and neighbour |
| `land_floor_m` | fx(0.05, 10) | "0.1" | designed | no land cell below this before E9 |
| `lake_min_area_m2` | int(0, 1000000000) | 100000 | guess | smallest lake |
| `lake_max_area_m2` | int(0, 1000000000) | 3000000 | guess | largest lake on a map of side `lake_area_ref_n` |
| `lake_area_ref_n` | int(16, 2048) | 1024 | designed | map side for which `lake_max_area_m2` is stated; the limit scales with map area |
| `lake_min_depth_m` | fx(0, 100) | 2 | guess | shallowest lake (deepest cell below the spill level) |
| `lake_max_share` | fx(0.001, 1) | "0.03" | guess | cap on kept lake cells, share of land; always applied |
| `fill_slope_dm` | fx(0, 10) | 1 | guess | rise of a filled surface per unit cost, decimetres per cell |
| `fill_noise` | `fbm_m` | wavelength 400, octaves 3, gain "0.5" | guess | noise of the fill cost |
| `fill_cost_amp` | fx(0, 16) | 3 | guess | cost = 1 + amp × clamp(½ + noise, 0, 1) |
| `fill_detail_m` | fx(0, 10) | 3 | guess | scale of the positive detail noise added to the fill increment |

No field of the second version's stable table is removed. Added: `segment_on_share`, `segment_floor_mask`, `belt_offset_steps`, `floor_inland_gm`, `ocean_shelf_m`, `talus_slope`, `talus_rate`, `lake_area_ref_n`, `lake_max_share`, `fill_noise`, `fill_cost_amp`, `fill_detail_m`. Dropped from the second version's placeholder table: `breach_max_depth_m`, `fill_relief`, `condition_passes`. None of the exploration branch's switches (`segment_floor`, `fill_mode`, `fill_depressed_only`, `lake_order_volume`, `fill_flats`, `erosion_random`, `height_remap`, `belt_height_scaling`, and the others listed in `v16-definition.md` section 11) exists: their v16 values are the only algorithm.

### 4.3 Named checks (P0-03 7.5)

Required loader validation: each check is evaluated when the definition is loaded, and a failure is a configuration error (a P0-03 load error at the definition's header), so a file that fails one is rejected and no world is generated.

1. `edge_radial_end − edge_radial_start ≥ 0.01`. Keeps the E2 divisor positive and bounded away from 0 (section 7).
2. `mountain_cover_rank ≤ mountain_peak_rank`.
3. `lake_min_area_m2 ≤ lake_max_area_m2`.

(The exploration code checks none of them; its values satisfy all three, so adding the checks changes no world.)

### 4.4 Tuning files

**Shipped** `game/data/worldgen/elevation.toml`, and the fixture `worldgen/tests/data/geo/worldgen/elevation.toml` (P1-00 6.4), identical:

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
segment_on_share = "0.08"
segment_floor_mask = "0.25"
belt_offset_steps = 4
floor_inland_gm = "0.9"
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
ocean_shelf_m = 2000
erosion_iterations = 24
erosion_route_interval = 1
erosion_k = "0.008"
erosion_diffusion = "0.0333"
talus_slope = 3
talus_rate = "0.0625"
land_floor_m = "0.1"
lake_min_area_m2 = 100000
lake_max_area_m2 = 3000000
lake_area_ref_n = 1024
lake_min_depth_m = 2
lake_max_share = "0.03"
fill_slope_dm = 1
fill_noise = { wavelength_m = 400, octaves = 3, gain = "0.5" }
fill_cost_amp = 3
fill_detail_m = 3
```

**Small-map fixture** `worldgen/tests/data/geo-small/worldgen/elevation.toml`: the file above with exactly these five lines changed, so that library tests at N = 64 to 256 reach the lake test, the share cap and the mountain floor (with the shipped values, lake_max_cells < lake_min_cells for N < 187, so no small world has a lake):

```toml
segment_on_share = "0.12"
lake_min_area_m2 = 10240
lake_max_area_m2 = 160000
lake_area_ref_n = 128
lake_min_depth_m = 1
```

The directory also holds `worldgen/tests/data/geo-small/worldgen/hydrology.toml`, a copy of the hydrology fixture, because every id of the fixed-id kind must be defined where the kind's directory exists (P1-00 6.1). With these values at N = 128: lake_min_cells 40, lake_max_cells 625, lake_min_depth_dm 10.

The `elevation` layer depends only on the values; `content_hash` and the world file also depend on the field set and order (P0-03 8), so these files give the same layer as the exploration branch's file but a different file hash.

### 4.5 Derived integers

| Name | Definition | 512 / 1024 / 2048 with the shipped values |
| --- | --- | --- |
| margin M | max(1, (edge_margin_frac.raw × N) >> 32), `int64` (< 2^43) | 15 / 30 / 61 |
| I_min (cells) | (island_min_area_m2 + 255) div 256 | 196 |
| side | `from_int(16 × N)`, the map side in metres | 8192 / 16384 / 32768 m |
| hw | min(belt_halfwidth_m, mul(belt_halfwidth_frac, side)) | 1228.8 / 2000 / 2000 m |
| floor | `land_floor_m` as `Fx` | raw 429496730 |
| H_MAX | `from_int(2990)`, a constant of the code: the largest land height before E9 | raw 12841952215040 |
| RATIO_CAP | 1024, a constant of the code (`rc`, 5.0) | |
| D_o, D_d | D_o = mul_int(talus_slope, 16), D_d = mul(D_o, SQRT2): allowed talus drop to an orthogonal and a diagonal neighbour | 48 m (raw 206158430208), 67.88 m (raw 291552048000) |
| lake_min_cells | (lake_min_area_m2 + 255) div 256 | 391 |
| lake_max_cells | a = (lake_max_area_m2 × N²) div (lake_area_ref_n²) in `int64` (floor; both positive); lake_max_cells = (a + 255) div 256, `int64` | 2930 / 11719 / 46875 (128: 184; 256: 733) |
| lake_min_depth_dm | mul_div(lake_min_depth_m.raw, 10, 2^32) | 20 |
| fine_slope | mul_div(fill_slope_dm.raw, 2^16, 2^32): decimetres per cell in fine units (2^16 per decimetre) | 65536 |
| KEY_MAX | 29990, a constant of the code: the largest land key E10 writes | |
| L_O | number of land cells after E4 | per world |
| target | round_int(mul(from_int(L_O), segment_on_share)) | per world |
| max_share_cells | max(1, round_int(mul(from_int(L_O), lake_max_share))), `int64` | per world (≥ 6 with the shipped values, since L_O ≥ I_min = 196) |

## 5. Algorithm

### 5.0 Conventions

- n = N²; cell (x, y), index i = yN + x (on a W × H test grid i = yW + x); D8 codes, 8- and 4-neighbour orders of P1-00 2.3.
- Every value is `Fx` unless stated; every operation is the P0-01 operation of that name: `+` and `−` checked; `mul`, `div`, `div_int`, `mul_div`, `from_ratio` with RHA; `mul_int` exact; `abs`, `min`, `max`, `clamp`, `fade`, `hypot`, `round_int`; `isqrt_u64`; `hash_at`, `substream`; `SQRT2`, `INV_SQRT2` of amendment A1; `kth`, `quantile`, `quantile_rank` of G1. `0` is `Fx()` (raw 0); `ONE`, `HALF` the P0-01 constants; `from_raw(1)` the smallest positive `Fx`. A plain integer expression (no `Fx` operation named) is exact `int64` arithmetic, `div` meaning floor division of non-negative operands.
- `rc(a, b)` ("ratio, capped", for a ≥ 0, b > 0) = `from_int(RATIO_CAP)` if a ≥ mul_int(b, RATIO_CAP), else div(a, b).
- `quantile_rank(q, n)` = min(n − 1, (q.raw × n) >> 32) in `uint64`, fatal if n = 0 (P1-00 7.1).
- Statements are evaluated exactly in the order written, one operation at a time with that operation's rounding. Parenthesised sums are evaluated as parenthesised; an unparenthesised chain `a + b + c` left to right.
- Step order: E1, E2, E3, E4, E5 (E5.1, E5.2, E5.3), E6, E7, E8, E9, E10, E11. E10.0 depends only on the seed, the cell and O, so it may run at any point after E4 (the exploration code runs it before E8). Each step names its determinism rule (P1-00 8).

### E1. Coordinates and continent centre (D1)

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

### E2. Pass A: continent, belt and segment fields (D1, parallel by G6 row blocks)

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
nb  = fbm_map(K_belt, Uw, Vw, belt_noise)                  // signed; belts follow a level set of nb
ns  = fbm_map(K_segment, Uw, Vw, segment_noise)
```

Keep three `int64` grids of raw values: c, nb and ns. Nothing else from this pass is kept.

Why this shape: the cone `ONE − d` around a jittered centre gives one continent; the warped shape noise of comparable weight moves lobes, bays and peninsulas off the circle; the square guard band (`fe`) and the radial falloff (`fr`) keep land off the edge, both perturbed by the same shape noise; the unwarped metre-scale coast noise adds detail at a fixed physical scale (P1-00 2.2). Belts follow the level sets of `nb`, which are long and curving; `ns` switches ranges on and off along them (E5).

### E3. Thresholds and scales (D7/G1; gradient D2)

1. **Coast threshold.** `c_sea = from_raw(quantile(c over all n cells, ONE − land_fraction))`.
2. **Belt-noise gradient** per cell (D2, G6 row blocks, reading the stored nb grid; one-sided at the map edge):
   ```
   x0 = max(x − 1, 0); x1 = min(x + 1, N − 1); y0 = max(y − 1, 0); y1 = min(y + 1, N − 1)
   gx = div_int(nb[x1, y] − nb[x0, y], x1 − x0)
   gy = div_int(nb[x, y1] − nb[x, y0], y1 − y0)
   g[x, y] = hypot(gx, gy).raw                                  // units of nb per cell
   ```
3. `g_med = from_raw(max(1, kth(g, quantile_rank(HALF, n))))`.
4. **Belt half-width** in metres: `hw` (4.5).
5. **Belt width in units of nb**: `w_b = max(from_raw(1), div_int(mul(g_med, hw), 16))` (hw / 16 m is the half-width in cells; times the median gradient per cell it is the change of nb across that distance).
6. **Land scale** `s_land`: over the multiset of `c − c_sea.raw` for every cell with `c > c_sea.raw`, `s_land = from_raw(max(1, kth(values, quantile_rank(HALF, count))))`. Fatal "elevation: no land" if count = 0.

The list in 6 may be built in any order (G1 is a value). The fatal error is checked, not argued unreachable: with `land_fraction` in [0.1, 0.8] the quantile in step 1 leaves cells above it unless the field is constant over most of the map. (The second version's ocean scale `s_ocean` is gone: E5.3 no longer uses the continent field below the coast. The exploration code still computes it, unused.)

### E4. Coast and ocean (D5, G2)

1. Provisional ocean: P(i) = `c[i] ≤ c_sea.raw` **or** min(x, y, N−1−x, N−1−y) < M.
2. Label the 4-connected components of P with G2. The ocean O is the union of the components that contain at least one cell on the grid edge (x = 0, y = 0, x = W−1 or y = H−1). On the N × N map every edge cell is in P (M ≥ 1), so O is the one component that contains the edge. P-cells not in O (pockets enclosed by land) become land.
3. Label the 8-connected components of the land (cells not in O) with G2. Every component with fewer than I_min cells is added to O. If no component has at least I_min cells: fatal "elevation: no land".

O stays one 4-connected region containing the margin: a sunk component's 4-neighbours outside it are all in O (a land 4-neighbour would belong to the same 8-component), so its cells join O's region. **From here on O is fixed:** no later step changes which cells are ocean. L_O = n − |O|.

The function behind this step accepts any W × H grid and any P (test seam, section 10); on a test grid O may be several components, one per edge-touching component of P.

### E5. Pass B: heights before erosion

**Coast factor and mask.** For a land cell (not in O):

```
s  = from_raw(c) − c_sea
tl = rc(abs(s), s_land)                      // abs: pocket cells (s ≤ 0) get the profile of |s|
gm = fade(clamp(div(tl, mountain_coast_t), 0, ONE))
```

For a belt offset o and a segment bias β (both `Fx`), per land cell:

```
a      = abs(nb − o)
b_o    = (a ≥ w_b) ? 0 : fade(ONE − div(a, w_b))                  // belt profile across the level set nb = o
sg_β   = fade(clamp((HALF + β) + mul(segment_contrast, ns), 0, ONE))  // segment on/off along the belt
mask(o, β) = mul(mul(b_o, sg_β), gm)
```

`div(a, w_b)` is evaluated only when a < w_b, so its value is at most ONE.

#### E5.1 Mountain floor (D8 integer counts in a fixed search order; D3 bisection)

Once per world. Define, over the land cells after E4 (pockets included):

```
lev        = segment_floor_mask
core(o, β) = #{land cells i : mask(o, β)(i) ≥ lev}
inl(o, β)  = #{land cells i : gm(i) ≥ floor_inland_gm and mask(o, β)(i) ≥ lev}
```

Counts are exact integers (≤ 2^22); they may be summed in parallel blocks in any order (D8). Then:

```
o* = 0;  β* = segment_bias
if core(0, segment_bias) < target:                 // the world lacks mountain core
    d = div_int(w_b, 2)
    best_o = 0;  best_r = inl(0, from_int(2))
    if best_r < target:
        for s in 1 .. belt_offset_steps:
            for sign in (+1, −1):
                o = mul_int(d, sign × s)
                r = inl(o, from_int(2))
                if r > best_r: best_r = r; best_o = o
                if r ≥ target: leave both loops
    o* = best_o
    lo = segment_bias;  hi = from_int(2)
    if core(o*, lo) ≥ target:
        hi = lo
    else:
        repeat 32 times:
            mid = from_raw((lo.raw + hi.raw) >> 1)      // arithmetic shift (floor)
            if core(o*, mid) ≥ target: hi = mid else lo = mid
    β* = hi
```

So the offsets are tried in the order 0, +d, −d, +2d, −2d, …, +Kd, −Kd (K = `belt_offset_steps`); a later offset replaces the best only if its inland reach is strictly larger (ties keep the earlier one), and the search stops at the first offset whose inland reach meets the target. If none does, the offset with the largest inland reach is used. The bias is then the result of the 32-step bisection on the full core (any gm), or `segment_bias` if that already reaches the target, or 2 if nothing does (hi never moves).

In words: a world whose mountain core (mask ≥ 0.25) already covers 8% of its land is unchanged. Otherwise the ranges are lengthened along their own belt (raising the bias extends segments along the level set) if the belt can put 8% of land into the core well inland (gm ≥ 0.9, about 1.6 land medians above the coast threshold); if it cannot, the belt moves to the first level set nb = o, in the fixed order, whose inland reach meets the target. Counting only inland cells when choosing the offset makes coastal rims lose to inland positions (critic's S3). Mountains stay on level sets of a smooth noise, so they stay elongated bands.

Equivalent ways of counting (the exploration code first lists the land cells with mul(b_o, gm) ≥ lev, which is necessary for mask ≥ lev because sg ≤ ONE and `mul` is monotone in each argument) give the same counts. With `segment_on_share` = 0, target is 0 and the floor never triggers.

#### E5.2 Land heights (D1, G6 row blocks)

For every land cell, with the single (o*, β*) of E5.1:

```
s     = from_raw(c) − c_sea
tl    = rc(abs(s), s_land)
co    = div(tl, ONE + tl)                            // lowland profile: half its asymptote at the land median
un    = fbm_m(K_basin, X, Y, basin_noise)
base  = mul(lowland_height_m, co) + mul(mul(basin_amp_m, un), co)
mask  = mask(o*, β*)
qx    = X + mul(relief_warp_amp_m, fbm_m(K_rwx, X, Y, relief_warp))
qy    = Y + mul(relief_warp_amp_m, fbm_m(K_rwy, X, Y, relief_warp))
if mask > 0:
    ridge   = ridged_m(K_ridge, qx, qy, ridge_noise, ridge_weight)
    profile = foothill_fraction + mul(ONE − foothill_fraction, ridge)
    unit    = mul(mask, profile)
else:
    unit    = 0
hh    = fbm_m(K_hills, qx, qy, hill_noise)
rn    = fbm_m(K_roughness, X, Y, roughness_noise)
rho   = roughness_min + mul(ONE − roughness_min, fade(clamp(HALF + rn, 0, ONE)))
hf    = hill_coast_min + mul(ONE − hill_coast_min, co)
hills = mul(hill_height_m, mul(mul(hh, rho), hf))
det   = mul(detail_amp_m, fbm_m(K_detail, X, Y, detail_noise))
h0    = (base + hills) + det
```

Keep h0 and unit per land cell. (The `if` only saves work: unit is 0 whenever mask is 0.)

#### E5.3 Ocean depth (D4 chamfer, then D1)

```
dl = chamfer(sources = the land cells, i.e. not O)          // P1-02 H7, fifths of a cell; D4
for each ocean cell:
    dist_m = div_int(from_int(16 × min(dl, 2^20)), 5)       // metres to the nearest land cell
    q      = div(dist_m, dist_m + ocean_shelf_m)
    h      = −(HALF + mul(ocean_depth_m, q))
```

`chamfer` is P1-02 H7's two-pass 5/7 transform (INF = 2^40 before the passes; forward pass in index order over the neighbours W, NW, N, NE with weights 5, 7, 5, 7; backward pass in reverse index order over E, SE, S, SW). Every ocean cell has dl ≥ 5, so dist_m ≥ 16 m and the divisor is positive even with `ocean_shelf_m` = 0. Ocean depths never enter a land computation (E8 treats the ocean as base level 0; E9 and E10 read only its sign), so E5.3 changes no land cell.

### E6. Mountain scale (D7, G1)

Ul = the multiset of unit over all land cells (zeros included).

```
q     = from_raw(quantile(Ul, mountain_peak_rank))
qc    = from_raw(quantile(Ul, mountain_cover_rank))
cap   = mul_int(mountain_height_m, 3)
ps    = (mul_int(q, 3) > ONE) ? div(mountain_height_m, q) : cap
scale = (qc > 0 and mul(ps, qc) > mountain_cover_height_m) ? div(mountain_cover_height_m, qc) : ps
```

The unit at the 99.8th percentile of land maps to `mountain_height_m`, capped so that the unit at the 85th percentile maps to at most `mountain_cover_height_m`; if the 99.8th percentile is at most 1/3, the factor is 3 × `mountain_height_m`.

### E7. Compose (D1, G6 row blocks)

Land: `h = clamp(h0 + mul(scale, unit), floor, H_MAX)`. Ocean: h from E5.3.

**Contract before E8** (relied on by E8 and E9): every land h ∈ [floor, H_MAX]; every ocean h ∈ [−(0.5 + ocean_depth_m), −0.5] m, so ≥ −2900.5 m and < 0.

### E8. Erosion (D3; its parts D6, D1, D4, D2)

A detachment-limited stream-power model with exponent m = 1/2 and n = 1, solved implicitly along the drainage tree (Braun and Willett 2013, "FastScape"), with receivers drawn at random among the lower neighbours in proportion to the square of the slope, followed by explicit hillslope diffusion and a talus step. Land cells only; the ocean is base level 0 and never changes. h is the `Fx` grid of the contract above; hn is a second `Fx` grid; L, parent, order, rcv and F persist between iterations.

```
for it in 0 .. erosion_iterations − 1:
    if it mod erosion_route_interval == 0:
        E8.1  (L, parent, order) = G3 routing flood, keys h.raw, ocean mask O        // D6
              rcv = G4 receivers (L, parent, O)                                       // D1, G6 blocks
        E8.2  random receivers (below), changing rcv                                  // D1, G6 blocks
        E8.3  A = G5 accumulation with rcv and G3's order                             // D4
              for each land cell c:                                                   // D1, G6 blocks
                  S    = from_raw(isqrt_u64(A[c] << 32) << 16)                        // √A with 16 exact fractional bits
                  F[c] = mul(erosion_k, S)
                  if rcv[c] is odd (diagonal): F[c] = mul(F[c], INV_SQRT2)
              for each ocean cell: F[c] = 0
    E8.4  implicit solve, D4, in G3's order (every receiver precedes its donors):
          for c in order:
              if c in O: hn[c] = h[c]; continue
              m    = the neighbour of c in direction rcv[c]
              base = (m in O) ? 0 : hn[m]
              v    = (h[c] > base) ? base + div(h[c] − base, ONE + F[c]) : h[c]   // no incision in a depression
              hn[c] = max(v, floor)
    E8.5  diffusion, D2, G6 blocks: reads hn, writes h:
          for each land cell c:
              S4 = 0
              for each code in the 4-neighbour order (E, S, W, N):
                  n = the neighbour of c in direction code
                  if n is outside the grid: S4 = S4 + hn[c]      // no flux across the edge (test grids only)
                  else if n is land:        S4 = S4 + hn[n]
                  else (n is ocean):        S4 = S4 + 0
              h[c] = max(floor, hn[c] + mul(erosion_diffusion, S4 − mul_int(hn[c], 4)))
          for each ocean cell: h[c] = hn[c]
    E8.6  talus step, D2, G6 blocks: reads h, writes t; then h = t:
          for each land cell c:
              δ = 0
              for code in 0 .. 7 (8-neighbour order):
                  m = the neighbour of c in direction code; skip if outside the grid
                  hm = (m in O) ? 0 : h[m]
                  Dk = (code odd) ? D_d : D_o
                  x  = h[c] − hm
                  if x > Dk:                      δ = δ − mul(talus_rate, x − Dk)
                  else if m is land and −x > Dk:  δ = δ + mul(talus_rate, (−x) − Dk)
              t[c] = max(floor, h[c] + δ)
          for each ocean cell: t[c] = h[c]
```

**E8.2 Random receivers** (D1, G6 row blocks; reads L, writes rcv). For each land cell c:

```
for code in 0 .. 7: w[code] = 0
W = 0                                                    // uint64
for code in 0 .. 7 (8-neighbour order):
    m = the neighbour of c in direction code; skip if outside the grid or L[m] ≥ L[c]
    δ = L[c] − L[m]                                      // int64, > 0
    q = δ >> 16                                          // uint64
    w[code] = (code odd) ? q × q : 2 × q × q              // ∝ slope²: δ² / step², step² = 2 or 1
    W = W + w[code]
if W > 0:
    r = hash_at(K_erosion, c, it) mod W                  // c the cell index, it the 0-based iteration, both int64
    acc = 0
    for code in 0 .. 7:
        acc = acc + w[code]
        if w[code] ≠ 0 and acc > r: rcv[c] = code; stop
// W = 0: rcv[c] keeps G4's value
```

W = 0 happens on a flat or in a depression of the flood surface (no lower neighbour, where G4 gave the flood parent) and, in principle, where every drop is below 2^16 raw (15 µm). Every chosen receiver has a strictly smaller L, so G3's `order` still lists every receiver before its donors, G5 and the solve remain valid, and there are no cycles (P1-00 7.4's argument). This spreads incision over the lower neighbours and removes the comb of parallel straight gullies that pure steepest descent cuts on planar flanks (critic's D3).

Notes:

- On the N × N map land never touches the edge (M ≥ 1), so the edge rules of E8.5 and E8.6 apply only to W × H test grids.
- G3 requires every land key ≥ 1: land h ≥ floor ≥ 0.05 m (raw ≥ 214748365) by the contract, and every update keeps it (`max(…, floor)`).
- The update base + (h − base)/(1 + F) is the exact implicit solution of one step of dh/dt = −K √A (h − h_receiver)/Δx; K, dt and Δx are folded into `erosion_k` (√(256 A)/16 = √A). Large rivers are cut nearly to their receiver's height in one step; ridges barely change. A cell whose receiver is not lower (a depression, where G4 fell back to the flood parent) is not incised, so depressions survive to E10.
- With D_o = 48 m per 16 m cell (71.6°) the talus step touches only cliff-like steps, mostly the walls of one-cell gorges cut by the stronger incision. With `talus_rate` = 0 it changes nothing (δ = 0 and h ≥ floor), so it needs no switch.
- When routing is not recomputed (`erosion_route_interval` > 1), rcv, order and F are reused; `order` still lists every receiver before its donors.
- With `erosion_iterations` = 0 the step does nothing.
- **The contract is kept, and the largest land height does not grow** (section 6.2).

### E9. Quantization (D1, G6 row blocks)

```
q = mul_div(h.raw, 10, 2^32)                       // decimetres, RHA
e = (c in O) ? min(−1, q) : max(1, q)              // int16
```

Land e ∈ [1, 29900], ocean e ∈ [−29005, −1] (section 7); a value outside [−32768, 32767] would be fatal and cannot occur.

### E10. Depression conditioning

E10 maps the E9 grid e to the final grid e'. Kept lakes are left exactly as they are; every other land cell is raised, where needed, to just above the cell from which a priority flood from the sea reaches it. The rise is one fine unit (1/65536 dm) on cells that were not in a depression, so natural slopes are not reshaped; on cells of a filled depression it is a slope of `fill_slope_dm` × a noisy cost per step plus a bump from the positive detail noise, so filled floors keep an irregular, non-polygonal grain; dead flats at the land floor (key 1 dm, typically pocket lowland at sea level) get the bump without the slope, which breaks straight flood-order paths across them. No breaching, no passes, no flat fill.

#### E10.0 Cost and bump grids (D1, G6 row blocks)

For every land cell:

```
nz      = fbm_m(K_fill, X, Y, fill_noise)
cost[i] = ONE + mul(fill_cost_amp, clamp(HALF + nz, 0, ONE))          // in [1, 1 + fill_cost_amp]
dn      = fbm_m(K_detail, X, Y, detail_noise)                          // the value E5.2's det scales
bv      = mul(fill_detail_m, max(0, dn))
bump[i] = mul_div(bv.raw, 10 × 2^16, 2^32)                            // int64, fine units (dm × 2^16), ≥ 0
```

For every ocean cell: cost = ONE, bump = 0 (never read). With `fill_detail_m` = 0 every bump is 0.

#### E10.1 Flood levels (D6)

`(L, parent, order) = G3(keys k = e as int64, ocean mask O)`. `depressed[c] = (c not in O) and L[c] > k[c]`.

#### E10.2 Depression components (D5, G2)

Label the depressed cells with G2: 8-connected, two adjacent depressed cells joined only when their L are equal (P1-02 H3); labels 1, 2, … in G2 order. For each component C:

```
size   = |C|
depth  = max over C of (L[c] − k[c])          // decimetres
volume = Σ over C of (L[c] − k[c])            // int64, decimetre-cells
```

(Adjacent depressed cells always have equal L, section 6.3 (c), so the equality condition never splits an 8-connected set of depressed cells; it is kept for agreement with P1-02 H3.)

#### E10.3 Lake selection (D7)

C is a **candidate** if lake_min_cells ≤ size ≤ lake_max_cells and depth ≥ lake_min_depth_dm (all compared in `int64`). Sort the candidates by (volume descending, label ascending), a strict total order. Then:

```
total = 0                                   // int64
for each candidate C in that order:
    if total + size(C) > max_share_cells: skip C
    else: mark every cell of C kept; total = total + size(C)
```

A skipped candidate does not stop the loop: a later, smaller one may still fit. The cap always applies; max_share_cells ≥ 1. This deliberately differs from the exploration code, where max_share_cells = 0 meant no cap; with the shipped tuning the two cannot differ (section 12 item 2). Ordering by volume keeps deep basins (crater lakes, mountain tarns) and fills shallow lowland basins first when the cap binds (critic's B1).

#### E10.4 Noisy priority flood (D6)

A min-priority queue of entries (P, seq, cell), popped in ascending (P, seq); seq is a global push counter from 0, so pairs are unique and any queue structure gives the same pops. kf (fine key) and P are `int64` per cell; visited is one flag per cell.

```
seq = 0
for i in 0 .. n − 1 (index order): if i in O: visited[i] = 1; push (0, seq, i); seq += 1
while the queue is not empty:
    pop (lv, ·, c)                                   // lv = P of c (0 for ocean)
    for code in 0 .. 7 (8-neighbour order):
        m = the neighbour of c in direction code; skip if outside the grid or visited[m]
        visited[m] = 1
        base = checked k[m] × 2^16
        if kept[m]:
            kf[m] = base
            P[m]  = max(base, lv)
        else:
            ce  = (code odd) ? mul(cost[m], SQRT2) : cost[m]           // direction from c to m
            eps = depressed[m] ? max(1, mul_div(fine_slope, ce.raw, 2^32)) : 1
            if depressed[m] or (k[m] = 1 and base ≤ lv):
                eps = checked eps + bump[m]
            kf[m] = max(base, checked lv + eps)
            P[m]  = kf[m]
        push (P[m], seq, m); seq += 1
```

`depressed` is the flag of E10.1 on e, not recomputed. Because kept cells take the first branch, the bump reaches only filled cells (depressed, not kept) and land-floor flats (key 1 dm, not above the level they are reached from). Fatal if O is empty (cannot happen after E4).

#### E10.5 Output

For ocean and kept cells e'[i] = k[i]; for every other land cell e'[i] = min(KEY_MAX, kf[i] >> 16) (kf > 0, so the shift is a floor).

### E11. Write

Write `elevation` (`i16`, codec 1) as the stage's only layer, in one stage transaction.

## 6. Invariants and arguments

### 6.1 Invariants

For every seed, N and tuning inside the bounds:

1. Every value of `elevation` is in [−30000, 30000] and none is 0.
2. The ocean (e < 0) is 4-connected, contains every cell with min(x, y, N−1−x, N−1−y) < M, and so touches all four edges.
3. Every 8-connected land component has at least I_min cells.
4. The land/ocean classification after E4 equals the sign of the final layer.
5. Land values are in [1, 29990]; ocean values in [−29005, −1].
6. The layer is a pure function of (seed, N, `worldgen.elevation`): independent of thread count, platform and compiler.
7. **Every depression of the layer is a kept lake.** The depression components of e' (G3 on e', components as in P1-02 H3) are exactly the components kept by E10.3, each with the same cells, size in [lake_min_cells, lake_max_cells], depth ≥ lake_min_depth_dm and its E9 values unchanged; together they hold at most max_share_cells cells. Every other land cell drains to the sea.
8. With `erosion_iterations` = 0, the layer equals E9 and E10 applied to the E7 output.
9. Unless E5.1 triggers, (o*, β*) = (0, `segment_bias`). If it triggers, core(o*, β*) ≥ target or β* = 2, and β* ≥ `segment_bias`.

### 6.2 E8 keeps the contract

- **Solve.** v lies between base and h[c] (or equals h[c]), so the solve never raises a cell; `max(·, floor)` keeps land ≥ floor.
- **Diffusion.** The exact value (1 − 4D)·hn[c] + D·S4 with 0 ≤ D ≤ 0.25 is a weighted mean of land values, zeros and (test grids) hn[c] itself; an integer RHA of a value ≤ M (M the largest land hn, an integer raw) is ≤ M. So the largest land height does not grow.
- **Talus.** A cell gains only from a land neighbour m with h[m] − h[c] > Dk ≥ D_o ≥ 0.8 m; each gain is mul(rate, h[m] − h[c] − Dk) with rate ≤ 1/16, so the exact total gain is at most 8 × (1/16) × (M − h[c]) = (M − h[c])/2, plus at most 8 × ½ raw of rounding; since M − h[c] > 0.8 m, the result stays below M. A cell that gains nothing does not rise. So no cell rises above the largest land height, and every write ends with `max(·, floor)`.
- Hence every land cell stays in [floor, H_MAX] through E8, and E9 gives land e ∈ [1, 29900].
- **No cycles.** G4 receivers and E8.2's random receivers both point to a strictly lower L or (G4 only) to the flood parent; P1-00 7.4's argument applies unchanged, and G3's `order` is topological for both.

### 6.3 E10: termination and drainage

**Termination.** One G3, one G2, one sort of at most n candidates, and one priority flood in which every cell is pushed exactly once (it is marked visited when pushed) and popped exactly once. The grid is 8-connected and O is non-empty, so every cell is pushed.

**Signs and bounds.** E10 writes no ocean cell. A kept cell keeps k ≥ 1. A non-kept land cell gets min(29990, kf >> 16) with kf ≥ base = k × 2^16, so e' ≥ k ≥ 1 (k ≤ 29900 < 29990). So land e' ∈ [1, 29990], ocean unchanged, no zero, and |e'| fits `i16`.

**Claim (a): no non-kept land cell is depressed in e'.** Call π(m) the cell that was being popped when m was pushed (its flood parent in E10.4). For a non-kept m, P[m] = kf[m] ≥ lv + eps ≥ P[π(m)] + 1, because lv is π(m)'s P and eps ≥ 1 in every branch (bump ≥ 0). For a kept m, P[m] = max(base, lv) ≥ P[π(m)]. Every chain m, π(m), π(π(m)), … ends at an ocean cell (each pushed cell's π was pushed earlier, and only ocean cells are pushed without one), and along it P never increases. For every land cell x of the chain, e'(x) ≤ min(29990, P[x] >> 16): for non-kept x, e'(x) = min(29990, kf[x] >> 16) and P[x] = kf[x]; for kept x, e'(x) = k[x] = base >> 16 ≤ P[x] >> 16 and k[x] ≤ 29900. Ocean cells have e' < 0. So every cell of the chain has e' ≤ min(29990, P[m] >> 16) = e'(m). The chain is an 8-path from m to the ocean whose largest key is e'(m); hence the flood level L'(m) ≤ e'(m), and since L' ≥ e' on land, L'(m) = e'(m): m is not depressed.

**Claim (b): kept cells stay depressed and get no shallower.** Kept cells keep k; every other land cell ends at e' ≥ e. Flood levels are monotone in the keys, so L' ≥ L everywhere. A kept cell had L > k = e', so L' − e' ≥ L − e > 0.

**Claim (c): the depression components of e' are exactly the kept components.** For adjacent land cells a, b of any surface, L(a) ≤ max(k(a), L(b)) (a can drain through b). If both are depressed, L(a) > k(a) gives L(a) ≤ L(b), and symmetrically L(b) ≤ L(a): adjacent depressed cells have equal L (P1-02 H3). So the G2 components of E10.2 are exactly the 8-connected components of the depressed cells, and two different components of e are never 8-adjacent. By (a) and (b) the depressed cells of e' are exactly the kept cells; each kept component is 8-connected, and no two are adjacent, so each is one depression component of e', with its size and a depth ≥ its depth in e.

Together: invariant 7, and P1-02 H3 finds exactly the kept lakes. The argument needs only eps ≥ 1 for every non-kept cell; the cost, the bump and the selection order do not enter it.

### 6.4 Why the fill does not reshape the land

A cell that was not depressed has eps = 1 (one fine unit) unless it is a land-floor flat; kf = max(base, lv + 1) exceeds base only where the cell is not above the cell that reached it, which on a non-depressed cell happens only on a flat of equal keys or where a filled area spills onto it. The output is kf >> 16, so a rise of less than 65536 fine units leaves the decimetre value unchanged. On a filled depression the rise per step is fine_slope × cost × (1 or √2) + bump: 1 to 4 dm per 16 m step with the shipped values (0.36° to 1.4°) plus 0 to about 2 m where the detail noise is positive.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| E1 centre | D1 | two location hashes, computed once |
| E1, E2 | D1, parallel by G6 row blocks | location-keyed noise only |
| E3 quantile, medians | D7, G1 | values; any selection method; an array selected in place is used only as a multiset afterwards |
| E3 gradient | D2, parallel | reads the complete nb grid, writes g |
| E4 | D5, G2 | |
| E5.1 | D8 counts, D3 | exact integer counts (any order or block split); fixed offset order; 32 bisection steps, no tolerance |
| E5.2, E7 | D1, parallel | |
| E5.3 chamfer | D4 | two sweeps in stated orders, single-threaded |
| E6 | D7, G1 | |
| E8.1 routing | D6, single-threaded | pop order (key, seq) |
| E8.1 G4, E8.2, E8.3 factors | D1, parallel | E8.2's draw is `hash_at(K_erosion, c, it)`: location-keyed, independent of evaluation order (D9) |
| E8.3 accumulation, E8.4 solve | D4 in G3's order | |
| E8.5 diffusion, E8.6 talus | D2, parallel | read one grid, write another; S4 summed in the 4-neighbour order; δ in the 8-neighbour order |
| E9 | D1, parallel | |
| E10.0 | D1, parallel | location-keyed noise only; may run any time after E4 |
| E10.1, E10.4 | D6, single-threaded | |
| E10.2 | D5, G2 | |
| E10.3 | D7 | sort by (volume desc, label asc); the cap loop runs in that order |

No floating point, no `<cmath>`, no `Rng`, no unordered containers, and no new math function: only P0-01 operations, `hypot`, `fade`, `isqrt_u64`, `hash_at`, `SQRT2` and `INV_SQRT2`. The determinism of the whole stage was checked on the exploration build: one file hash and one layer hash per case for 512 seeds 0, 42, 7 and 1024 seed 1 at 1, 2 and 5 threads with GCC and Clang release, and a clean ThreadSanitizer run (`v16-determinism.txt`). Windows/MSVC has not been run.

**Overflow bounds** for N = 2048 and any tuning inside the bounds. Every bound is far below the range of `Fx` (±2^31 in value) unless stated.

- **Noise coordinates.** Map fraction: U, V ∈ (0, 1); |wx| ≤ 1 and amp ≤ 0.5, so Uw, Vw ∈ (−0.5, 1.5). With cycles ≤ 64 and octaves ≤ 10 the tenth octave's frequency is 64 × 2^9 = 32768, so |sample| < 1.5 × 32768 = 49152; the final frequency update after the last octave is 65536. Metres: X, Y ≤ 32760, and the warped qx, qy ≤ 32760 + 5000; with wavelength ≥ 64 the tenth octave samples at ≤ 37760 × 2^9 / 64 = 302080.
- **E1.** (hv − 2^31) × 2 ∈ [−2^32, 2^32 − 2] fits `int64`; J ∈ [0.25, 0.75).
- **E2.** |Uw − cx| < 1.25, so hypot < 1.768 and d < 3.536. |nc| ≤ 1. Uf, Vf ∈ (−0.25, 1.25) and ed ∈ (−0.25, 0.5]. |edn| ≤ 0.75; with `edge_falloff_frac` ≥ 0.01 the ratio before the clamp is ≤ 75 in magnitude. The radial hypot < 1.061, dm ∈ (−0.25, 2.372); dm − start ∈ (−2.25, 2.372); with named check 1 the divisor is ≥ 0.01, so the ratio is ≤ 237.2 in magnitude. edge ∈ [0, 8]. c ∈ (−16.54, 7].
- **E3.** |nb| ≤ 1, so each difference ≤ 2 and g ≤ 2.829. hw ≤ 20000 m; mul(g_med, hw) ≤ 56569; w_b ≤ 3536. c − c_sea < 23.54 (raw < 2^37). Quantile products q.raw × n ≤ 2^54.
- **E4.** Integer labels; ≤ 2^22 cells.
- **E5.** tl ≤ 1024 (rc), co < 1, gm ∈ [0, 1]. |o| ≤ 8 × div_int(w_b, 2) ≤ 14144, |nb − o| < 14146; div(a, w_b) only for a < w_b. β ∈ [−1, 2]; lo.raw + hi.raw ≤ 4 × 2^32 fits `int64`; |(HALF + β) + contrast × ns| ≤ 18.5. Counts ≤ 2^22; target, max_share_cells ≤ L_O. base ≤ 2000 + 1000 m; |hills| ≤ 1000 m; |det| ≤ 100 m; unit ∈ [0, 1].
- **E5.3.** dl ≤ 7 × 2 × 2048 < 2^15 (INF cannot remain: there is land after E4); dist_m ≤ 16 × 2^20 / 5; q < 1; h ∈ [−2900.5, −0.5] m.
- **E6, E7.** ps ≤ 3 × 2900 = 8700 when q > 1/3, cap ≤ 8700 otherwise; scale ≤ 8700; h0 + scale × unit < 12800 m before the clamp.
- **E8.** A ≤ 2^22, A << 32 ≤ 2^54, isqrt ≤ 2^27, << 16 ≤ 2^43; S ≤ 2048, F ≤ 2048. |h − base| ≤ 2990 m. S4 ∈ [0, 4 × 2990 m], |S4 − 4hn| ≤ 11960 m (by 6.2). E8.2: δ ≤ 2990 m raw < 2^44, q < 2^28, w < 2^57, W < 2^60 (`uint64`). E8.6: |x| ≤ 2990 m, |δ| ≤ 8 × 0.0625 × 2990 m. Flood keys < 2^44.
- **E9.** |h.raw × 10| < 2^46 inside `mul_div`'s 128-bit product; ocean h ≥ −2900.5 m gives e ≥ −29005 (−3005 with the shipped `ocean_depth_m`).
- **E10.** lake_max_area_m2 × N² ≤ 10^9 × 2^22 < 2^52; lake_max_cells can exceed N² (and 2^32) for extreme tuning, so it is an `int64` and compared as one (the exploration code narrows it to `uint32`, section 12 item 1). volume ≤ 2^22 × 29990 < 2^37. cost ∈ [1, 17]; bump ≤ mul_div(10 × 2^32, 10 × 2^16, 2^32) < 2^23. base = k × 2^16 ≤ 29990 × 2^16 < 2^31. eps ≤ mul_div(10 × 2^16, 17 × 1.4143 × 2^32, 2^32) + 2^23 < 2^25; along any flood chain (≤ 2^22 cells) P < 2^31 + 2^22 × 2^25 < 2^48 (`checked_add`); output ≤ 29990.

## 8. Performance

Budget (P1-00 11): 2 / 8 / 35 s at 512 / 1024 / 2048 for the stage on the 6-core development machine, release build; peak memory within P1-00's 200 MB / 600 MB / 2 GB for all of geography.

Measured on the exploration build (`linux-gcc-release`, 6 threads; `v16-timings.txt`, `worldgen-cli generate` including loading and writing the file, elevation the only stage):

| | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| `generate`, 6 threads | 1.87–1.92 s (3.7 s on a cold first run) | 6.84–6.87 s | 27.7 s |
| `generate`, 1 thread | | 18.7 s | |
| peak RSS | 32 MB | 110 MB | 385 MB |

Per step (spec writer's measurement of the library calls, 1024 seed 3, 6 threads): E1 to E3 1.37 s; E4 0.02 s; E5 to E7 1.00 s; E8 3.45 s (24 iterations; the G3 flood, the accumulation and the solve are single-threaded); E9 0.02 s; E10.0 and E10 about 0.3 s. At 2048 seed 0: 5.4, 0.1, 3.7, 14.6, 0.1 s and about 1.5 s. 512 is within its budget by about 5%, 1024 by 15%, 2048 by 20%.

The cost per cell: E2 evaluates 19 `noise2` octaves (warp 3 + 3, continent 4, coast 5, belt 2, segment 2); E5.2 about 20 per land cell plus 6 ridged octaves where the mask is positive; E10.0 6 per land cell. Memory: about 8 bytes per cell for each `int64` grid (c, nb, ns, g, h, hn, F, L, kf, P, cost, bump and the flood queue are the large ones; not all are live at once).

Erosion is a grid method with an implicit tree solve rather than droplets: droplet erosion is sequential and order-sensitive and has no deterministic parallelism; the stream-power solve is O(N²) per iteration with a defined order and is stable for any step size.

If the stage is too slow, in this order:

1. **Without changing output:** a radix or bucket heap for the floods (pop order is defined by (key, seq), not by the structure; the shared G3 already uses a radix heap); a table for `isqrt_u64` of A < 2^16; the basin-parallel solve of P1-00 section 8 (each drainage basin's cells solved by one thread in `order`; basins write disjoint cells); computing E10.0 in the same pass as E5.2 (it reads the same detail noise); dropping the unused computations the exploration code still makes (E3's `s_ocean`, a chamfer from the ocean, the continent-field ocean depth).
2. **Tuning only:** `erosion_route_interval` 2 or 4, then fewer iterations with a larger `erosion_k`. Changes output; a shipped tuning change (PATCH bump).
3. **Algorithm change (spec amendment):** erode a 2:1 downsampled grid at 2048 and add the difference back bilinearly.

## 9. Database rows written

One `layer` row: `name` = `elevation`, `width` = `height` = N, `format` = `i16`, codec 1, in one stage transaction; its id is the next layer id (1 in a normal world). No schema change: `SCHEMA_VERSION` stays 1, and `docs/specs/schema.md` is unchanged.

## 10. Tests required

Library seams (interim names, as on the exploration branch): `elevation_fields(seed, N, tuning, threads)` returning c, nb, ns as raw grids and c_sea, w_b, s_land (E1 to E3); `elevation_ocean_from_provisional(grid, P, I_min)` (E4 steps 2 and 3 on any W × H grid); `elevation_classify(fields, tuning)` (E4 on the map); a floor seam returning (trigger, target, o*, β*) for given land cells' (nb, ns, gm) lists and w_b (E5.1); `elevation_heights(seed, fields, ocean, tuning, threads)` (E5 to E7); `erode(grid, h, ocean, params, threads)` with its parts (E8 on any W × H grid with at least one ocean cell); `quantize_elevation` (E9); a conditioning seam taking (grid, e, ocean, cost, bump, lake_min_cells, lake_max_cells, lake_min_depth_dm, max_share_cells, fine_slope) (E10.1 to E10.5 on any W × H grid); `elevation_grid(seed, N, tuning, threads)` for the whole stage.

### 10.1 Unit tests with worked examples

Every figure below was computed by the spec writer's exact-integer reference of this spec; the examples marked "checked" were also run through the exploration library and agree (the floor has no library seam there; F1 and F2 come from the reference alone, whose floor agrees with the library's output on every triggering world of 10.4).

**Derived integers.** margin M for N = 512, 1024, 2048 with "0.03" (raw 128849019) is 15, 30, 61; with "0" it is 1. I_min for 50000 m² is 196; for 0, 0; for 256, 1; for 257, 2. `land_floor_m` "0.1" is raw 429496730. lake_max_cells with the shipped values at N = 128, 256, 512, 1024, 2048: 184, 733, 2930, 11719, 46875 (at 2048: a = 12000000, (12000000 + 255) div 256 = 46875). lake_min_cells 391; lake_min_depth_dm 20; fine_slope 65536; D_o raw 206158430208, D_d raw 291552048000 (67.882 m). target and max_share_cells for L_O = 6517: 521 and 196; for L_O = 26261: 2101 and 788.

**Coordinates.** N = 1024: U at x = 0 is raw 4194304 (1/2048), at x = 1023 raw 4290772992; N = 16: U at x = 3 is `from_ratio(7, 32)` = raw 939524096. X at x = 2047 is `from_int(32760)`.

**Continent centre.** With `continent_center_jitter` "0.06" (raw 257698038): hv = 2^31 gives J = HALF exactly; hv = 0 gives J = raw 1889785610; hv = 2^32 − 1 gives J = raw 2405181686. For seed 0 the test computes hv_0 and hv_1 from `hash_at` and checks cx, cy returned by the seam.

**E2 structural.** For seed 0, N = 64 and the shipped tuning, at 32 cells chosen by the test (including the four corners and cells on the row and column N/2), c, nb and ns returned by the seam equal the values the test computes from `fbm2`, `hypot`, `fade` and the statements of E2, in order.

**E3 coast threshold and land scale.** Ten cells with c raw values 5, −3, 2, 0, 8, −1, 4, −7, 1, 3 and `land_fraction` "0.4" (raw 1717986918): q = raw 2576980378, rank (2576980378 × 10) >> 32 = 6; sorted −7, −3, −1, 0, 1, 2, 3, 4, 5, 8, so c_sea = raw 3. Land values (c > 3): 1, 2, 5; rank 1, s_land = raw 2.

**E3 belt gradient.** A 4 × 4 nb grid, values in units of 1/64 (raw multiples of 67108864), rows y = 0 to 3: `0 4 8 12`, `2 6 10 14`, `4 8 12 16`, `6 10 14 30`. At (0, 0): gx = 4/64 (one-sided), gy = 2/64, g = raw 300119964. The last row's g values are 300119964, 300119964, 684378814, 1426754019 and (3, 2) is 600239927; the other eleven cells are 300119964. g_med = raw 300119964. With hw = 2000 m: w_b = raw 37514995500.

**Belt half-width cap, 512 against 1024.** `belt_halfwidth_frac` "0.15" is raw 644245094. At N = 512, mul(644245094, from_int(8192)) = raw 5277655810048 (1228.8 m) < 2000 m, so hw = raw 5277655810048; at 1024 and 2048 hw = 2000 m. With g_med = "0.01" (raw 42949673): w_b at 512 = raw 3298534884; at 1024 and 2048 raw 5368709125.

**E4 on a given mask.** N = 8, M = 1, I_min = 2, provisional ocean P ('.') and land ('#'):

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

Expected: (2, 2) becomes land (not 4-connected to the edge through P); (6, 1) is sunk; (5, 6) stays. Final land: 19 cells, (1..3, 1), (1..4, 2), (1..5, 3), (2..5, 4), (3..4, 5), (5, 6). Also: a 5 × 5 grid whose only P-cells are the border and (2, 2) gives O = the border only; on a test grid with two edge-touching P components both are ocean.

**E5 coast factor.** s = HALF, s_land = "0.4" (raw 1717986918): tl = div = raw 5368709121 (1.25 rounded up), div(tl, 2) = raw 2684354561, gm = fade(·) = raw 3112960001; co = raw 2386092942. With s ≥ 1024 × s_land, tl = from_int(1024) and gm = ONE.

**E5 mask and E5.1 floor, case F1 (bisection, offset 0).** w_b = "0.15" (raw 644245094), so d = div_int(w_b, 2) = raw 322122547; `segment_contrast` "0.75", `segment_bias` "0.25", `segment_floor_mask` "0.25" (lev raw 1073741824), `floor_inland_gm` "0.9" (raw 3865470566), `belt_offset_steps` 4, `segment_on_share` "0.2" with L_O = 10, so target = round_int(mul(from_int(10), raw 858993459)) = 2. Ten land cells, (nb, ns, gm) as decimal strings, and the raw values of b_o, mask(o, 0.25) and mask(o, 2) at o = 0, +d, −d (sg at bias 2 is ONE for every cell):

| i | nb | ns | gm | sg(0.25) | b₀ | m₀(.25) | m₀(2) | b₊ | m₊(.25) | m₊(2) | b₋ | m₋(.25) | m₋(2) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0.01 | −0.6 | 0.5 | 700423267 | 4283480114 | 349274969 | 2141740057 | 2678025583 | 218366206 | 1339012792 | 1616941713 | 131845428 | 808470857 |
| 1 | −0.02 | −0.7 | 0.6 | 338970816 | 4212436052 | 199474332 | 2527461632 | 1123559201 | 53204659 | 674135521 | 3171408094 | 150177831 | 1902844857 |
| 2 | 0.03 | −0.5 | 0.4 | 1182007296 | 4046202788 | 445418173 | 1618481115 | 3594544031 | 395698219 | 1437817612 | 700423267 | 77104700 | 280169307 |
| 3 | −0.07 | −0.4 | 1 | 1747506766 | 2415124793 | 982649372 | 2415124793 | 1512254 | 615296 | 1512254 | 4293455043 | 1746891471 | 4293455043 |
| 4 | −0.08 | −0.5 | 0.95 | 1182007296 | 1879842488 | 491479449 | 1785850364 | 0 | 0 | 0 | 4293455043 | 1122511557 | 4078782291 |
| 5 | 0.08 | −0.6 | 1 | 700423267 | 1879842488 | 306564713 | 1879842488 | 4293455043 | 700176649 | 4293455043 | 0 | 0 | 0 |
| 6 | 0.2 | 0.4 | 1 | ONE | 0 | 0 | 0 | 152444828 | 152444828 | 152444828 | 0 | 0 | 0 |
| 7 | −0.3 | 0.3 | 1 | 4294321122 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 8 | 0.5 | 0 | 1 | 3850371072 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 9 | −0.6 | 0.2 | 0.2 | 4258202376 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

core(0, 0.25) = 0 < 2: the floor triggers. inl(0, 2) counts cells with gm ≥ 0.9 (3 to 8) and mask ≥ lev: cells 3, 4, 5, so 3 ≥ 2 and the offset search is skipped: o* = 0. core(0, 0.25) = 0 < 2, so the bisection runs on [0.25, 2]: mid = 4831838208 (core 6, hi), 2952790016 (6, hi), 2013265920 (4, hi), 1543503872 (1, lo), 1778384896 (1, lo), 1895825408 (2, hi), …; step 31 (the 32nd) has lo = 1844380217, hi = 1844380220, mid = 1844380218 with core 1. Result **β* = raw 1844380220** (0.42943). (With `floor_inland_gm` 0, the v15 rule, the result would be the same here, since inl = core at o = 0 bias 2 is 6.)

**E5.1 floor, case F2 (offset search).** As F1 with cells 3, 4 and 5 changed to (−0.09, −0.4, 1), (−0.1, −0.5, 0.95) and (0.08, −0.6, 0.5). Then at o = 0: b₃ = 1363394412, m₃(2) = 1363394412 ≥ lev; b₄ = 901412886, m₄(2) = 856342242 < lev; cell 5 is no longer inland (gm 0.5). core(0, 0.25) = 0 < 2: trigger. inl(0, 2) = 1 < 2: search. +d: b₃ = b₄ = 0 and cell 5 is not inland, inl = 0 (not > 1). −d: b₃ = 4258202375, b₄ = 4142522467, m₃(2) = 4258202375, m₄(2) = 3935396343, inl = 2 ≥ 2: best_o = −d = **raw −322122547**, search stops. core(−d, 0.25): m₃ = 1732548108 and m₄ = 1083050666 are ≥ lev, so core = 2 ≥ target, hi = lo and **β* = `segment_bias`** (raw 1073741824). Note that the full core at o = 0, bias 2 is 4 (cells 0, 1, 2, 3, mostly coastal): with `floor_inland_gm` 0 (v15) the result would have been o* = 0 with a raised bias, a coastal rim.

**E5.3 ocean depth.** `ocean_shelf_m` 2000, `ocean_depth_m` 300. dl = 5 (one orthogonal step): dist_m = raw 68719476736 (16 m), q = raw 34087042, h = raw −12373596248 (−2.881 m), E9 e = −29. dl = 7: q = raw 47570840, h = raw −16418735648, e = −38. dl = 12: h = raw −26420454248, e = −62. dl = 100 (320 m): q = raw 592409282, h = raw −179870268248, e = −419.

**E10.0 cost and bump.** `fill_cost_amp` 3: nz = "0.1" gives cost raw 12025908430 (2.8), eps on a depressed cell (fine_slope 65536) 183501 orthogonal, 259509 diagonal; nz = "−0.7" gives cost ONE, eps 65536 / 92682; nz = "0.6" gives cost from_int(4), eps 262144 / 370728. `fill_detail_m` 3: dn = "0.3" (raw 1288490189) gives bv raw 3865470567 (0.9 m), bump 589824 (9 dm × 2^16); dn = "−0.2" gives 0; dn = "0.6667" gives bump 1310786.

**E8 solve, by hand.** A 5 × 1 grid, cell 0 ocean, cells 1 to 4 land with heights 10, 20, 40, 80 m; `erosion_k` = HALF, `erosion_diffusion` = 0, `talus_rate` = 0, floor 0.1 m, one iteration. Each land cell has one lower neighbour (west), so E8.2 keeps it whatever the hash. A = 4, 3, 2, 1. S raw = 8589934592, 7439056896, 6073942016, 4294967296; F raw = 4294967296, 3719528448, 3036971008, 2147483648. Results (raw): cell 1: 21474836480 (5 m exactly); cell 2: 55999923266; cell 3: 123833544243; cell 4: 270342770534.

**Diagonal factor.** A = 4, `erosion_k` = HALF, diagonal receiver: F raw = mul(4294967296, 3037000500) = 3037000500.

**Diffusion.** One land cell with hn = 10 m whose 4-neighbours are ocean, 20, 30 and 40 m; `erosion_diffusion` "0.1" (raw 429496730): S4 = 90 m, S4 − 4hn = 50 m, result raw 64424509460.

**Edge rule.** A 3 × 1 grid: ocean, land 10 m, land 20 m at the east edge; `erosion_k` 0, `erosion_diffusion` "0.1", `talus_rate` 0, one iteration. East cell: S4 = 20 + 20 + 10 + 20 = 70 m, result raw 85899345920 − 4294967300 = 81604378620.

**E8 one full iteration with random receivers and talus** (checked). A 4 × 3 test grid, column x = 0 ocean (h = −5 m), land in metres:

```
y0:  O   20   60  140
y1:  O   10   50  130
y2:  O   30   70  150
```

Shipped values (`erosion_k` raw 34359738, `erosion_diffusion` raw 143022411, `talus_slope` 3, `talus_rate` raw 268435456, floor raw 429496730), it = 0, K_erosion = `stream_key(0, "geo.elevation.erosion")` = 0x7d5f47a9629481b8. There are no depressions, so L = h on land. G3 order: 0, 4, 8, 5, 1, 9, 6, 2, 10, 7, 3, 11. G4 receivers: every land cell W (4) except (2, 2), NW (5). E8.2, per land cell: weights by code (0 to 7), W, the hash, r = hash mod W and the chosen code:

| cell | w (codes 2, 3, 4, 5, 6) | W | hash_at(K, c, 0) | r | rcv |
| --- | --- | --- | --- | --- | --- |
| 1 (1,0) | 858993459200, 1717986918400, 3435973836800, 0, 0 | 6012954214400 | 16623902295446874679 | 2025025268279 | 3 |
| 2 (2,0) | 858993459200, 10737418240000, 13743895347200, 0, 0 | 25340307046400 | 8202078681277673717 | 4117420060917 | 3 |
| 3 (3,0) | 858993459200, 34789235097600, 54975581388800, 0, 0 | 90623809945600 | 10539194117446402976 | 7516012905376 | 3 |
| 5 (1,1) | 0, 429496729600, 858993459200, 429496729600, 0 | 1717986918400 | 10738554057075179731 | 227722117331 | 3 |
| 6 (2,1) | 0, 1717986918400, 13743895347200, 3865470566400, 0 | 19327352832000 | 8503336810010755729 | 16675985539729 | 5 |
| 7 (3,1) | 0, 15461882265600, 54975581388800, 21045339750400, 0 | 91482803404800 | 10446593672691320968 | 80869093804168 | 5 |
| 9 (1,2) | 0, 0, 7730941132800, 3865470566400, 3435973836800 | 15032385536000 | 6609825904454596978 | 10822347716978 | 5 |
| 10 (2,2) | 0, 0, 13743895347200, 15461882265600, 3435973836800 | 32641751449600 | 6895401036938547231 | 26893719244831 | 5 |
| 11 (3,2) | 0, 0, 54975581388800, 42949672960000, 3435973836800 | 101361228185600 | 13145864497773222452 | 22730698201652 | 4 |

(Codes 0, 1 and 7 have weight 0 everywhere. For cell 1: S to (1, 1) δ = 10 m, q = 655360, w = 2q²; SW to the ocean δ = 20 m, w = q²; W to the ocean, w = 2q².) Receivers rows: `8 3 3 3`, `8 3 5 5`, `8 5 5 4`. A rows: `0 3 2 1`, `0 5 2 1`, `0 1 2 1`. F rows (raw): `0 42081660 34359405 24296004`, `0 54327163 34359405 24296004`, `0 24296004 34359405 34359738`. After the solve, hn rows (raw): `−21474836480 85065878946 255989444282 599115291631`, `−21474836480 42413187352 213719148628 556644983913`, `−21474836480 128124238736 298598250103 641501865506`. After the diffusion, h rows: `−21474836480 86504605273 260316179429 586274939663`, `−21474836480 50979829367 223668165360 549465549000`, `−21474836480 126680308153 301513681839 627257440952`. After the talus step (the steps between columns 2 and 3 exceed D_o = 48 m and the diagonal ones D_d), h rows: `−21474836480 86504605273 267803700056 574346498642`, `−21474836480 50979829367 242588847069 541988114410`, `−21474836480 126680308153 308987764896 612781031170` (about 20.14, 62.35, 133.73 / 11.87, 56.48, 126.19 / 29.50, 71.94, 142.67 m).

**E9 quantization.** Land 50.5 m (raw 216895848448) → 505; land 0.04 m (raw 171798692) → q 0 → 1; land 0.05 m → 1; ocean −0.05 m → −1; ocean raw −1 → −1; land 12.345 m (raw 53021371269) → 123; land 2990 m → 29900; ocean −2900.5 m → −29005.

**E10.3 lake selection** (arithmetic). lake_min_cells 3, lake_max_cells 12, lake_min_depth_dm 20, max_share_cells 30. Components (label: size, depth, volume): 1: (5, 25, 60); 2: (14, 40, 300); 3: (8, 30, 120); 4: (2, 50, 90); 5: (6, 15, 50); 6: (10, 22, 120); 7: (9, 21, 61); 8: (3, 20, 25). Candidates: 1, 3, 6, 7, 8 (2 too large, 4 too small, 5 too shallow). Order: 3 (120), 6 (120, larger label), 7 (61), 1 (60), 8 (25). Kept: 3 (total 8), 6 (18), 7 (27); 1 is skipped (27 + 5 > 30); 8 is kept (30). Kept labels 3, 6, 7, 8.

**E10 fill flood** (checked). A 7 × 6 test grid, keys in decimetres, ocean −5 on the border:

```
      x:  0    1    2    3    4    5    6
y0       -5   -5   -5   -5   -5   -5   -5
y1       -5   30   40   35   50   45   -5
y2       -5   45   10   14   60    1   -5
y3       -5   38   12   25    1    1   -5
y4       -5   20   28   70    1    1   -5
y5       -5   -5   -5   -5   -5   -5   -5
```

cost = ONE everywhere except (2, 2) from_int(2) and (2, 3) ONE + HALF; bump = 0 everywhere except (2, 2) 30000, (4, 3) 200000, (5, 2) 70000; fine_slope 65536; lake_min_cells 2, lake_max_cells 10, max_share_cells 100 (does not bind). E10.1: L = k on every land cell except (2, 2) and (2, 3), whose L is 14 (they spill through (3, 2), which drains through the flat at (4, 3)); one component, label 1, cells (2, 2) and (2, 3), size 2, depth 4, volume 6.

*Case 1: lake_min_depth_dm 5* (not a candidate; filled). Land pushes in order (cell, from, code, lv, base, eps, kf): the 14 land cells next to the border are pushed first from ocean cells at lv 0 with eps 1, each kf = base (for example (5, 2) from (6, 1), code 3: base 65536, kf 65536 — its bump is not used, since base > lv); then (4, 3) from (5, 2), code 3, lv 65536, base 65536: a land-floor flat with base ≤ lv, eps = 1 + 200000, kf = 265537; (4, 2) from (5, 2), code 4, kf = base 3932160; (3, 3) from (4, 4), code 5, kf = base 1638400; (3, 2) from (4, 3), code 5, lv 265537, base 917504, eps 1, kf 917504; (2, 3) from (3, 2), code 3 (diagonal), lv 917504, base 786432, eps = mul_div(65536, mul(ONE + HALF, SQRT2).raw, 2^32) = 139023 (depressed, bump 0), kf = 1056527; (2, 2) from (3, 2), code 4, eps = 131072 + 30000 = 161072, kf = 1078576. Land pop order: (5, 2), (5, 3), (5, 4), (4, 4) at P 65536; (4, 3) 265537; (3, 2) 917504; (2, 3) 1056527; (2, 2) 1078576; then (1, 4), (3, 3), (2, 4), (1, 1), (3, 1), (1, 3), (2, 1), (5, 1), (1, 2), (4, 1), (4, 2), (3, 4). Output: (2, 2) → 1078576 >> 16 = 16, (2, 3) → 16, (4, 3) → 4; every other cell unchanged.

*Case 2: lake_min_depth_dm 4* (kept). The same pushes up to (3, 2); then (2, 3) and (2, 2) take the kept branch: kf = base, P = max(base, 917504) = 917504. Output: (2, 2) 10 and (2, 3) 12 unchanged (the lake, level 14), (4, 3) → 4.

**Tuning validation.** Named check 1 fails on `edge_radial_start` "1.0", `edge_radial_end` "1.005" and on end below start; check 2 on `mountain_cover_rank` "0.999" with `mountain_peak_rank` "0.998"; check 3 on `lake_min_area_m2` 200000 with `lake_max_area_m2` 100000; a float literal in any field is a P0-03 error; a missing field is an error; each bound of 4.2 rejects a value one step outside it (including `lake_area_ref_n` 15 and `lake_max_share` "0.0009"). Each named-check failure is a load error and no world is written.

### 10.2 Property tests

Over seeds 0 to 63 at N = 128 with both fixtures (4.4), and seeds 0 to 3 at N = 512 with the shipped fixture, by code independent of the stage (its own components, floods and flood levels; a Dijkstra-style minimax search for flood levels is acceptable):

- Invariants 1 to 9 of section 6.
- E4 alone: O is 4-connected, contains the margin, and every land 8-component has at least I_min cells; every P-cell not in O is land; no non-P cell is ocean unless it belongs to a sunk component.
- E3 values equal those found by sorting a copy.
- E5.1: for seeds where the floor triggers (with the shipped values at N = 128, among seeds 0 to 15: seeds 9, 10, 12 and 13; at seed 13 the chosen offset is the last one tried, o* = −4d = −2 w_b), the seam's (o*, β*) equals the test's own implementation of the search; for the others it is (0, `segment_bias`).
- E8 alone, on any input meeting the contract of E7: no land cell is raised by the solve; every land cell after the loop is ≥ floor; ocean cells are unchanged; the largest land height does not increase; every random receiver points to a neighbour with strictly smaller L.
- E10 alone, on the E9 grids above and on 200 random fixtures (random keys in [1, 200] on a 32 × 32 interior with an ocean border, test stream `test.elevation.fill`, test-chosen thresholds, cost in [ONE, 4 ONE] and bump in [0, 2^20]): e' = e on ocean and kept cells; e' ≥ e on land; the depression components of e' are exactly the kept components; kept cells ≤ max_share_cells; every non-kept land cell reaches the ocean along a path on which e' never increases.

### 10.3 Determinism tests

- Threads (D10): `elevation_fields`, `erode` and `elevation_grid` for seeds 0 and 42 at N = 128 and 512 with 1, 2 and 5 threads give identical grids.
- Repeat: two runs in one process give identical grids (no hidden state).
- Map sizes are independent worlds: the layer for seed 0 at 512 does not depend on whether a 1024 world was generated first in the same process.

### 10.4 Reference values

Obtained by the spec writer by linking a small driver against the exploration build (`~/.cache/the-unmapped/build/agent-ab9284166ef3d88c1/linux-gcc-release/worldgen/libworldgen.a`, commit `ef1f734`) that calls `elevation_fields`, `elevation_classify`, `elevation_heights`, `erode` (with the key of `geo.elevation.erosion`), `quantize_elevation` and `elevation_grid` with the branch's data file (whose values are those of 4.4), and independently by the exact-integer Python reference written from this spec, which agreed on every grid of every case below. Grid hashes are SHA-256 over the values in index order, little-endian, with no header: `int64` for c, nb, ns, h7 (after E7) and h8 (after E8), one byte (0 or 1) for O, `int16` for e (after E9) and e'. "layer" is the P0-04 8 layer hash (`u32` N, `u32` N, `u8` 4, then the `int16` values), as `hash --detail` prints it.

Shipped values:

| Case | Grid hashes |
| --- | --- |
| N = 64, seed 0 | c 8cb0bbd013e3bf7259047e494d2e89d2b28dc4cdc5c93d998b70a9453d1e8039; nb 52ada98aa8e8557cd24d436331807d46435aaff597c1a540f89b4a0e673e0b66; ns 9fc8ac504252e04392b61f39048f6b3e4203d0d59e810b61ea9317a0374cc58a; O 936760537c3a1cf2561f800975ea0c893765badcb4d5f11383519c5a7fad6056; h7 a6ae624b4587b0342e3fe5347db71ab01d74846bc9fe92d85f1bee7ceab52dc6; h8 ec6a46ad0e448a23fbbff052dae9e5a34a8b877da23e38dddda4cd470ad77d49; e e5489854f2bd4171d5c6f13292ed8dc2796bed55bae2a374a0e19e39fd2db8bd; e' 73eb0e7a9434dc294d1c239565209ecf595c530721f32ef2c7a3efc243087d9f; layer 4adc347172241077037b573efe645967a7b64fbb86e45af24cda1de4278b5fdc. c_sea raw 1527296118, w_b raw 633782745, s_land raw 664447675, L_O 1627, floor not triggered, scale raw 5468777764307 |
| N = 128, seed 0 | c 506d10ce615f08f3ae707f37e48d5fc69fba61be3967d46ad092331316ca5e01; O 22e099d3abfb1fa7d0c6b6590bafffcd876b0f44f1c6d684381cab8e6ce488b7; h7 13dfebec68615f598877827ee22f8de0b1bfc7dcc9cf080582ed2b7cfbe26110; h8 45f75a532f1197cedf1005eee6db8ac526e4d4b9a624f94647a34794cc1b5306; e c22424f0cc8b6742f3737fd7269c9a9eb81dad04ac7811fe3d549ed7190f4d5a; e' 27511abd265a988a0da35cd4cccd635c7f53b8a29206255948f191fd69da1d7e; layer ccbf793522d6eae22f935cb9296e31ffb6568222065d507f66385831c0e51acd |
| N = 128, seed 13 (floor triggers: o* raw −1533020888, β* = `segment_bias`) | h7 1c16c847fea2fad99071e240bf63eb4465d1b8a6f33c9ade38e7e87d3095ac33; h8 0a59c6f90fd205178a7545e4519ab4c48d3c350dc31a4030ef31593a8b0a7ab1; e' 9fd82613e7df1dcf43e59f133eb6b2e3addcbad3d10fbd576557246352f39594; layer 0cf48bf013cdb90bf551f0320337bbce49df60de8045423c4bafcf2e5955e756. w_b raw 766510444, target 533 |
| N = 256, seed 0 | h7 3a87ba85f4e24dbbb297cbf4b0170c028b57fb8bffc488d0f953f52d09761378; h8 48810f9b99469dc5d0846d2d1044c502b3907e5f06462a43eaa356eb7ca980c0; e' c3b8b51e6c9b4bb92e8f937c111e74d441d94ab073fc3ff696aa455c8050bfdb; layer da71dd297ce6fdb90e564918e7be5cf840cb31c323945eccaadc1a6081933271. 8 depression components, no candidate (lake_max_cells 733, all larger or shallower) |

Small-map fixture, N = 128 (for seed 0 the floor triggers under neither share, so every grid before E10 equals the shipped case):

| Seed | e' | layer | E10.3 |
| --- | --- | --- | --- |
| 0 | 207e485c79b5333a6c1251a69594d6e34e3ccbf178dab98f6fdcfd33918188f7 | 35829c03890351f3df0726bbb88b7910a1721685f9763f39a38256e1951f7965 | 3 components, 1 candidate, kept: label 3 (48 cells, volume 3383); cap 196 |
| 1 | 37e519364e5bea8f1d448d2a787fdb918814f92ab2afee76f4579c7943b864a7 | 1b8e01fa5e289fa021ee585e93860819e7d49fff63a22c2b08c73c99cfb3a010 | floor triggers; 2 candidates; kept: label 1 (141 cells, volume 24065); the other skipped by the cap 197 |
| 2 | b63af0ed000726beb6466ee87ec02dcc008d60ab4b39aa2f6d4afe5ed4ceb4d6 | 686c589a630b7c5691bf0278bc2027f274e8f73cce05bf56c603dab06fe471cf | 2 candidates, both kept in volume order: label 8 (46 cells, 25446), then label 1 (49 cells, 1183) |

Mountain floor at N = 512, shipped values (spec writer's reference; seeds 0 to 19 of the sweep trigger only at these four): seed 9 o* raw 688414388 (+2d), seed 10 raw 653599488 (+2d), seed 12 raw −397564838 (−d), seed 13 raw −1530846568 (−4d), each with β* = `segment_bias`. In every triggering world checked (N = 128 seeds 9, 10, 12, 13 with both fixtures and seed 1 with the small one; N = 512 these four) an offset reaches the target at `segment_bias`, so no natural world reaches the bisection. **Required test:** the floor's bisection must be exercised by a constructed case, either a direct call of the floor seam with given land-cell lists (worked example F1, expecting β* = raw 1844380220 and the mid sequence given there) or a tuning fixture in which `core(o*, segment_bias)` < target, and the test checks β* against the test's own implementation of E5.1. A test suite in which every floor case ends with β* = `segment_bias` does not meet this requirement.

Full-size layer hashes of the exploration build at N = 1024, seeds 0 to 9, shipped values (`v16-definition.md` section 10; seed 0 re-checked by the spec writer): 070dbf1206416efcbed64aa009b89e5e3f23fec990a4397d99675cfba9450786, 0cc4f2c15156aa89424cee960d9e434e8fe3a018b855d6a4a2e703cf314a31d2, c67f2aa09e7e73e96d151846bdc7b771ef55ebd1fac44bb726ba57863d7e79ee, e9fb8259e2813298306a12b7d3611405485dd144de7cc6ec6cb6d0daca3d1273, bafe937a342236d2e7565939f79a6ee40586a38b6f614217e5868ea24f04d37c, aae827ebf0a93b753d73d05a95bd83393ec803b9a7a46deed18c0a948d311eef, 91114f3ad92fa40ce007f9ffcced15c51efeca14af23252eb756eaee35bab2ac, 14e232deb39aa1e8f52b73ab2a627b61d0b27ab8c5c3a9d18cc7dbf12cc3dbad, c68fcc1ebbb517ae949d34fdcce6c73043c13b6bffe7e7ff522fa626dec5c8ed, df59c33313cf315232f028eb71c1d0da21e6ab64981bdee5d9b4563d7657745f.

These are reference values that the implementation must reproduce (they are what the approved algorithm computes), not golden values. The validation specialist records the goldens once the implementation passes: the layer hash for seeds 0, 42 and the phrase "The Unmapped" at N = 512, seed 1 at N = 1024, seed 0 at N = 128 (library, both fixtures); and, to isolate the outline from the relief, the grid hashes of c, nb, ns and O for seed 0 at N = 128 and 512.

### 10.5 Statistics

The validation specialist's sweep (P1-00 13.1) checks the hard ranges there, including `step_p999_m` ≤ 150 m at every map size (lead's decision); the other steepness, flatness and edge-run statistics are descriptive: reported, not tested. The ranges were set from the v16 sweeps (512 seeds 0–19, 1024 seeds 0–19, 2048 seeds 0–4, `v16-{512,1024,2048}-stats.txt`).

## 11. Known limitations

Accepted for the first merge (lead's decision); each is a candidate for a later P1 revision. Seeds are 1024 unless stated; coordinates are cells, (0, 0) at the north-west.

1. **Closed ranges, no natural passes.** Ranges are walls with walk-around ends; at 1024 a median of 1.6% of land above 500 m is reachable from the main lowland under 35°, and walkers detour up to 10× (`v16-access.txt`). By decision (proposals "High peaks may be inaccessible; history opens the passes", "Natural passes through the ranges").
2. **Small filled pads among the peaks.** Basins that fail the lake test become smooth, gently sloping surfaces; lowland fills carry the bump texture, but small fills among mountains rise only a few cells and still read as smooth patches (s0, x 600–700, y 450–700; s3 foothills, x 530–590, y 715–850).
3. **The floor guarantees area, not shape.** s13 still has a ring of ranges around a central basin; s12's west range runs along the coast. Where the floor triggers, land above 500 m is 0.17–0.23 at 1024.
4. **A few straight gullies** on the most planar flanks (s4 north flank, s5 west flank near y 540) and the s5 diagonal crease, a feature of the surface before erosion.
5. **Octagonal ocean depth contours** far from land (the 5/7 chamfer), and no offshore banks. Depth matters only for sea rendering.
6. **Coastal-cliff pockets.** Where a range runs into the sea at both ends it can seal off a small lowland with steep cells only a few metres above the sea (s18: 12 km² behind six steep cells at 12 m; s15; 512 s0). They are range feet on the shore, not river slots.
7. **Lake share at the cap.** The 3% cap binds in most 1024 and 2048 worlds; it keeps the deepest basins. `lake_share` is not evidence of variety.
8. **512 is steep.** v13's treatment is kept (no height scaling with the narrower belts): `step_p999_m` 65–130 m, a median 39% of land steeper than 35°.
9. **Coasts and lowlands as in v13.** Coasts press on the falloff arcs and have little 100 m to 1 km detail (critic's D7, D8); flat lowland is rare (`lowland_flat_share` 0.2% to 4%, critic's D6).
10. **Rivers across land-floor flats** may keep short straight 45° or 90° segments (s9).

## 12. Open questions and findings for the lead

1. **Code defect, not specified (decided by the lead: keep `int64`):** the exploration code narrows lake_max_cells to `uint32` (`static_cast<std::uint32_t>((a + 255) / 256)`), which truncates for extreme tuning (for example `lake_max_area_m2` 10^9 with `lake_area_ref_n` ≤ 64 at N = 2048). This spec keeps it `int64` (4.5); with every value the exploration has used the two agree. The code also accepts `lake_area_ref_n` = 0 (meaning "do not scale"); this spec bounds it to [16, 2048] since only the scaled rule is v16.
2. **Decided (lead): the lake share cap always applies.** `lake_max_share` ≥ 0.001 and max_share_cells = max(1, round_int(…)); the exploration code's "0 = no cap" is not specified. This and the `int64` lake_max_cells of item 1 deliberately differ from the exploration code. Neither can change a world generated with the shipped tuning, at any N from 16 to 2048: the code's cap differs only when round_int(L_O × lake_max_share) = 0, i.e. L_O × lake_max_share < 0.5, while E4 guarantees L_O ≥ I_min = 196, so with 0.03 the value is at least round_int(5.88) = 6; and the `uint32` narrowing differs only when lake_max_cells ≥ 2^32, while the shipped values give at most 46875 (N = 2048).
3. **Decided (lead): `step_p999_m` ≤ 150 m is a hard check at every map size** (512, 1024, 2048; measured maxima 130, 87 and 61 m). It is the only hard steepness bound.
4. **River density.** With v16's terrain and hydrology's 0.5 km² threshold, `river_share_of_land` is 0.4% to 0.9% at 512 and 1024, below P1-00's former hard bound of 1%. P1-00 13.1 is revised to the measured range; whether to lower `river_min_area_m2` is a hydrology tuning question (P1-02 open question 3).
5. **Pockets get mountains.** E5 uses |s| for pocket cells (land enclosed below the coast threshold), so a deep pocket gets the coast factor and lowland profile of high inland ground. This is v13's rule, kept; noted because it is not obvious.
6. **No lakes on small maps** with the shipped values (N < 187). Accepted; library tests use the small-map fixture for lakes.

Proposals entries needed (for the lead): none new for the schema (no change). The entry "Elevation is final after the elevation stage; hydrology does not carve" points to P1-02 section 9, whose text said the stage "ends by filling depressions too small to be lakes"; with v16 it also fills depressions that are too shallow, too large or beyond the lake share, and P1-02 9.1 and its quoted entry are updated accordingly. If the lead keeps a copy of that wording elsewhere, it needs the same change.
