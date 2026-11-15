# P1-01 Elevation

Status: draft for approval, 2026-11-15. Phase P1 Geography, stage 1 (`elevation`). Design doc section 4 (stage 1: "layered noise shaped by a continent mask, then hydraulic erosion"). Depends on P0-01 to P0-04 and P1-00, whose sections are cited as "P1-00 n" and whose shared algorithms G1 to G6 and determinism rules D1 to D10 are used by name.

## 1. Purpose

Produce the `elevation` layer: one continent per world that reads as a continent. A coastline with bays and peninsulas; mountain ranges that run as ranges; broad lowlands; and enough relief variety for the six biomes and for history to find defensible and fertile places. The stage composes integer gradient noise into a raw height field, sets the coast so that a target share of the map is land, maps ranks of the raw field onto a target distribution of heights in metres, carves the result with stream-power erosion and hillslope diffusion, quantizes to decimetres, and removes depressions too small to be lakes. Its output is the final terrain: no later stage changes it (P1-02 section 9).

## 2. Inputs

| Input | Source |
| --- | --- |
| World seed (`uint64`) | `meta.seed` |
| Map side N | `param.map_size` (512, 1024, 2048); library and tests: any 16 ≤ N ≤ 2048 |
| Tuning | definition `worldgen.elevation` (section 4) |
| Streams | `geo.elevation.continent_warp_x`, `geo.elevation.continent_warp_y`, `geo.elevation.continent`, `geo.elevation.belt`, `geo.elevation.relief_warp_x`, `geo.elevation.relief_warp_y`, `geo.elevation.ridge`, `geo.elevation.hills`, `geo.elevation.roughness`. Each gives a noise key K_name = `stream_key(seed, name)`. No `Rng` draws anywhere in this stage. |
| Earlier layers | none |

## 3. Outputs

One layer, `elevation`: `i16`, N × N, codec 1, decimetres above sea level (P1-00 4). Values in [-30000, 30000], never 0. Negative cells are ocean, positive cells land. Postconditions in section 6.

## 4. Tuning

### 4.1 Noise records and noise functions

Two record types, shared with later stages (P1-00 6.2):

| Record | Fields |
| --- | --- |
| `fbm_map` | `cycles` fx(0.125, 64), the base frequency in cycles per map side; `octaves` int(1, 10); `gain` fx(0.0625, 1) |
| `fbm_m` | `wavelength_m` int(64, 65536), the base wavelength in metres; `octaves` int(1, 10); `gain` fx(0.0625, 1) |

`LAC` = `from_int(2)`. Lacunarity is a fixed constant of the code, not tuning: a power of two keeps every octave's frequency an exact multiple of the base, and nothing in the design calls for another value.

```
fbm_map(K, a, b, rec) = fbm2(K, a, b, rec.octaves, rec.cycles, LAC, rec.gain)
fbm_m(K, a, b, rec)   = fbm2(K, a, b, rec.octaves, from_ratio(1, rec.wavelength_m), LAC, rec.gain)

ridged_m(K, a, b, rec, weight):              // Musgrave-style ridged multifractal, result in [0, ONE]
    f = from_ratio(1, rec.wavelength_m); amp = ONE; wgt = ONE; sum = 0; norm = 0
    for o in 0 .. rec.octaves - 1:
        n    = noise2(substream(K, o), mul(a, f), mul(b, f))
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

Every statement is evaluated as written, in this order, with P0-01 rounding. The loop of `ridged_m` updates `amp` and `f` after the last octave as `fbm2` does.

**How the stage normalizes.** `fbm2` and `noise2` do not reach their nominal ±1 (P0 measurement: `fbm2` stays within about ±0.67). The stage never relies on that range. The coast is set by a quantile of the raw field (E5), mountain belts by a quantile of the belt noise (E3), and heights in metres by mapping ranks onto a target curve (E7), so the land fraction, belt coverage and height distribution come out as specified whatever the noise's actual spread. What still depends on the spread is the balance between terms (the weights, the warp amplitudes, the roughness mapping); those are tuning values chosen against measured output.

### 4.2 Fields of `worldgen.elevation`

All fields required (P1-00 6.1). "Designed" values follow from the design or the algorithm; "guess" values are starting points **to be tuned against the seed sweep**.

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `land_fraction` | fx(0.1, 0.8) | "0.42" | designed (target) | share of map cells above sea level before clean-up |
| `edge_margin_frac` | fx(0, 0.25) | "0.03" | designed | band along every edge forced to ocean, as a fraction of N (at least 1 cell) |
| `continent_warp` | `fbm_map` | cycles "2", octaves 4, gain "0.5" | guess | domain warp of the continent shape |
| `continent_warp_amp_frac` | fx(0, 0.5) | "0.1" | guess | warp displacement, fraction of the map side, per unit of noise |
| `continent_noise` | `fbm_map` | cycles "3", octaves 5, gain "0.5" | guess | bays and peninsulas at continental scale |
| `continent_noise_amp` | fx(0, 4) | "0.35" | guess | |
| `continent_ramp` | fx(0.01, 4) | "0.3" | guess | width of the coastal transition in units of the continent field |
| `island_min_area_m2` | int(0, 1000000000) | 50000 | guess | land components smaller than this are sunk |
| `belt_noise` | `fbm_map` | cycles "1.5", octaves 3, gain "0.5" | guess | mountain belts follow the zero lines of this noise |
| `belt_fraction` | fx(0, 1) | "0.18" | guess | share of the map inside a belt |
| `relief_warp` | `fbm_m` | wavelength 3000, octaves 3, gain "0.5" | guess | domain warp of the relief noise |
| `relief_warp_amp_m` | fx(0, 5000) | 400 | guess | metres of displacement per unit of noise |
| `ridge_noise` | `fbm_m` | wavelength 4000, octaves 6, gain "0.5" | guess | ridge spacing and detail |
| `ridge_weight` | fx(0, 4) | 2 | designed (Musgrave's usual value) | how strongly a ridge suppresses detail off it |
| `hill_noise` | `fbm_m` | wavelength 2500, octaves 7, gain "0.5" | guess | hills and coastline roughness |
| `roughness_noise` | `fbm_m` | wavelength 8000, octaves 2, gain "0.5" | guess | where land is hilly and where it is plain |
| `roughness_min` | fx(0, 1) | "0.25" | guess | hill amplitude on the plainest land |
| `weight_continent` | fx(0, 8) | 1 | designed (the reference scale) | |
| `weight_mountain` | fx(0, 8) | "0.6" | guess | |
| `weight_hills` | fx(0, 8) | "0.12" | guess | |
| `land_curve` | array(`curve_point`, 2, 16) | see below | guess | heights of land by rank |
| `ocean_curve` | array(`curve_point`, 2, 16) | see below | guess | depths of the sea by rank |
| `erosion_iterations` | int(0, 200) | 16 | guess | |
| `erosion_route_interval` | int(1, 64) | 1 | designed | recompute drainage every this many iterations (fallback knob, section 8) |
| `erosion_k` | fx(0, 1) | "0.002" | guess | stream-power coefficient per iteration |
| `erosion_diffusion` | fx(0, 0.25) | "0.05" | guess | hillslope diffusion per iteration; 0.25 is the stability limit of the 4-neighbour filter |
| `land_floor_m` | fx(0.05, 10) | "0.1" | designed | no land cell is lowered below this |
| `lake_min_area_m2` | int(0, 1000000000) | 40000 | guess | smaller depressions are filled |
| `lake_min_depth_m` | fx(0, 100) | 1 | guess | shallower depressions are filled |

`curve_point` is a record { `rank` fx(0, 1), `height_m` fx(-3000, 3000) }.

Defaults of the curves (rank → metres):

| `land_curve` | 0 → 0.5 | 0.3 → 40 | 0.6 → 150 | 0.8 → 400 | 0.92 → 900 | 0.98 → 1600 | 1 → 2400 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ocean_curve` | 0 → -400 | 0.5 → -120 | 0.9 → -20 | 1 → -0.5 | | | |

The land curve is chosen for the six biomes before erosion lowers it: about 30% of land below 40 m (coast and river lowlands, marsh), the middle of the land at 100 to 150 m (grassland and forest), about 20% above 400 m (forested uplands, highland moor) and about 3% above 1500 m (alpine). Ocean depth matters only for the map's look and the runtime shoreline.

Named checks (P0-03 7.5), each an error at the definition's header:

1. In each curve, `rank` is strictly increasing, the first is 0 and the last is 1 (ONE exactly).
2. In each curve, `height_m` is non-decreasing.
3. Every `land_curve` height is ≥ `land_floor_m`; every `ocean_curve` height is ≤ -0.1.

Example file `game/data/worldgen/elevation.toml`, also the content of the fixture `worldgen/tests/data/geo/worldgen/elevation.toml` (P1-00 6.4):

```toml
[worldgen.elevation]
land_fraction = "0.42"
edge_margin_frac = "0.03"
continent_warp = { cycles = "2", octaves = 4, gain = "0.5" }
continent_warp_amp_frac = "0.1"
continent_noise = { cycles = "3", octaves = 5, gain = "0.5" }
continent_noise_amp = "0.35"
continent_ramp = "0.3"
island_min_area_m2 = 50000
belt_noise = { cycles = "1.5", octaves = 3, gain = "0.5" }
belt_fraction = "0.18"
relief_warp = { wavelength_m = 3000, octaves = 3, gain = "0.5" }
relief_warp_amp_m = 400
ridge_noise = { wavelength_m = 4000, octaves = 6, gain = "0.5" }
ridge_weight = 2
hill_noise = { wavelength_m = 2500, octaves = 7, gain = "0.5" }
roughness_noise = { wavelength_m = 8000, octaves = 2, gain = "0.5" }
roughness_min = "0.25"
weight_continent = 1
weight_mountain = "0.6"
weight_hills = "0.12"
land_curve = [
  { rank = "0", height_m = "0.5" }, { rank = "0.3", height_m = 40 },
  { rank = "0.6", height_m = 150 }, { rank = "0.8", height_m = 400 },
  { rank = "0.92", height_m = 900 }, { rank = "0.98", height_m = 1600 },
  { rank = 1, height_m = 2400 },
]
ocean_curve = [
  { rank = "0", height_m = -400 }, { rank = "0.5", height_m = -120 },
  { rank = "0.9", height_m = -20 }, { rank = 1, height_m = "-0.5" },
]
erosion_iterations = 16
erosion_route_interval = 1
erosion_k = "0.002"
erosion_diffusion = "0.05"
land_floor_m = "0.1"
lake_min_area_m2 = 40000
lake_min_depth_m = 1
```

### 4.3 Derived integers

| Name | Definition | 512 / 1024 / 2048 with defaults |
| --- | --- | --- |
| margin M | max(1, (edge_margin_frac.raw × N) >> 32) | 15 / 30 / 61 |
| I_min (cells) | ceil(island_min_area_m2 / 256) = (a + 255) div 256 | 196 |
| lake_min_cells | ceil(lake_min_area_m2 / 256) | 157 |
| lake_min_depth_dm | `mul_div(lake_min_depth_m.raw, 10, 2^32)` (RHA) | 10 |
| floor | `land_floor_m` as `Fx` | raw 429496730 |

## 5. Algorithm

Notation: n = N²; cell (x, y), index i = yN + x; all values `Fx` unless stated; P0-01 operations and rounding. Steps E1 to E11 run in this order. Each step names its determinism rule.

### E1. Coordinates (D1)

```
X = from_int(16x + 8)            Y = from_int(16y + 8)            // metres, ≤ 32760
U = from_ratio(2x + 1, 2N)       V = from_ratio(2y + 1, 2N)       // map fraction, in (0, 1)
```

### E2. Continent and belt fields, pass A (D1)

For every cell:

```
wx = fbm_map(K_continent_warp_x, U, V, continent_warp)
wy = fbm_map(K_continent_warp_y, U, V, continent_warp)
Uw = U + mul(continent_warp_amp_frac, wx)
Vw = V + mul(continent_warp_amp_frac, wy)
d  = mul_int(hypot(Uw - HALF, Vw - HALF), 2)        // 0 at the centre, 1 at an edge midpoint
nc = fbm_map(K_continent, Uw, Vw, continent_noise)
c  = ONE - d + mul(continent_noise_amp, nc)          // continent field
ab = abs(fbm_map(K_belt, Uw, Vw, belt_noise))       // distance-like value to a belt line
```

Keep c and ab for every cell (two `int64` grids).

### E3. Field thresholds (D7, G1)

```
c_sea = quantile(c over all n cells, ONE - land_fraction)
w_b   = max(from_raw(1), quantile(ab over all n cells, belt_fraction))
```

c_sea is where the coast would be if the continent field alone decided; it anchors the coastal ramp. w_b makes `belt_fraction` of the map lie inside a belt.

### E4. Raw height, pass B (D1)

For every cell, with c and ab from E2:

```
cs  = clamp(div(c - c_sea, continent_ramp), -ONE, ONE)   // -1 open sea .. 0 coast .. +1 interior
g   = fade(clamp(cs, 0, ONE))                             // keeps mountains off the coast
B   = (ab >= w_b) ? 0 : fade(ONE - div(ab, w_b))          // 1 on a belt line, 0 outside the belt
qx  = X + mul(relief_warp_amp_m, fbm_m(K_relief_warp_x, X, Y, relief_warp))
qy  = Y + mul(relief_warp_amp_m, fbm_m(K_relief_warp_y, X, Y, relief_warp))
R   = ridged_m(K_ridge, qx, qy, ridge_noise, ridge_weight)
Hh  = fbm_m(K_hills, qx, qy, hill_noise)
rn  = fbm_m(K_roughness, X, Y, roughness_noise)
rho = roughness_min + mul(ONE - roughness_min, fade(clamp(HALF + rn, 0, ONE)))
r   = mul(weight_continent, cs) + mul(weight_mountain, mul(mul(B, g), R)) + mul(weight_hills, mul(Hh, rho))
```

The test `ab >= w_b` comes before the division so that `div(ab, w_b)` is always below ONE and cannot overflow when w_b is tiny.

Why this shape: the continent field saturates inland (cs = 1), so the interior is not a dome; its height comes from belts and hills. Mountain belts follow the zero lines of a low-frequency noise, which are long and curving, so high ground forms ranges; the ridged noise inside them, in metres, gives ridges about 4 km apart with valleys between. The roughness field makes some country hilly and some plain. All of it is warped in metres so ridges and hills do not line up with the noise lattice.

### E5. Coast and ocean (D7, D5)

```
t_sea = quantile(r over all n cells, ONE - land_fraction)
P(i)  = r[i] <= t_sea  or  min(x, y, N-1-x, N-1-y) < M      // provisional ocean
O     = the cells of P that are 4-connected, through cells of P, to a cell on the map edge
```

Every edge cell is in P (M ≥ 1), so O is the 4-connected component of P containing the edge. Any method that computes this set conforms (it is a set, not an order); the reference is a G2-style FIFO flood from the edge cells in index order with the 4-neighbour order. Cells of P not in O (pockets enclosed by land) become land.

### E6. Small islands (D5, G2)

Label the 8-connected components of the land set (not O) with G2. Every component with fewer than I_min cells is added to O. If no land remains, raise the fatal error ("elevation: no land"; impossible within the tuning bounds, but checked).

O stays a single 4-connected region touching the edge: a sunk component's 4-neighbours outside it are all in O (a land 4-neighbour would belong to the same 8-component), so its cells join O. Land and ocean from here on are fixed: erosion and conditioning do not move the coast.

### E7. Heights from ranks (D1 after D7)

Let Land be the cells not in O, n_L = |Land|, and n_O = |O|. For the land curve with points (p_j, m_j), j = 0 .. J-1:

```
k_j   = min(n_L - 1, (p_j.raw × n_L) >> 32)
rho_j = kth(r over Land, k_j)                       // G1; rho_0 is the smallest land r, rho_{J-1} the largest
```

For a land cell with raw height r: j = the largest index with rho_j ≤ r (it exists, since rho_0 ≤ r).

```
if j = J-1:   h = m_{J-1}
else:         h = Fx::from_raw(m_j.raw + mul_div((m_{j+1} - m_j).raw, r.raw - rho_j.raw, rho_{j+1}.raw - rho_j.raw))
```

The denominator is positive because rho_{j+1} > r ≥ rho_j. Ocean cells use the ocean curve the same way, over O. Pocket cells added to the land in E5 have the smallest r on land and get heights near m_0; sunk island cells have the largest r in the ocean and get depths near the last ocean point. The map is continuous and non-decreasing in r, so the ordering of the raw field is kept and the height distribution follows the curve.

### E8. Erosion (D3)

A detachment-limited stream-power model with exponent m = 1/2 and n = 1, solved implicitly along the drainage tree (Braun and Willett 2013, "FastScape"), followed by explicit hillslope diffusion. Land cells only; the ocean is base level at height 0 and is never changed. h is the `Fx` height from E7.

```
for it in 0 .. erosion_iterations - 1:
    if it mod erosion_route_interval == 0:
        (L, parent, order) = G3 routing flood with keys h.raw, ocean mask O
        rcv = G4 receivers
        A   = G5 accumulation (reverse order)
        for each land cell c:                                   // D1
            S    = from_raw(isqrt_u64(A[c] << 32) << 16)        // sqrt(A[c]), 16 fractional bits exact
            F[c] = mul(erosion_k, S)
            if rcv[c] is odd (diagonal): F[c] = mul(F[c], INV_SQRT2)
    // implicit solve, D4 in G3 order (every receiver precedes its donors)
    for c in order:
        if c in O: hn[c] = h[c]; continue
        m    = the neighbour of c in direction rcv[c]
        base = (m in O) ? 0 : hn[m]
        if h[c] > base: v = base + div(h[c] - base, ONE + F[c])
        else:           v = h[c]                               // in a depression: no incision
        hn[c] = max(v, floor)
    // diffusion, D2: reads hn, writes h
    for each land cell c:
        S4 = sum over the 4 neighbours n of c (4-neighbour order) of: hn[n] if n is land, 0 if n is ocean
        h[c] = max(floor, hn[c] + mul(erosion_diffusion, S4 - mul_int(hn[c], 4)))
    for each ocean cell: h[c] = hn[c]
```

Notes:

- Land cells never touch the map edge (margin M ≥ 1), so every land cell has four neighbours. On the W × H test grids of section 10, where land may touch the edge, a missing neighbour contributes hn[c] itself to S4 (no flux across the edge).
- The stream-power update h_new = base + (h − base)/(1 + F) is the exact implicit solution for one step of dh/dt = −K √A (h − h_receiver)/Δx with n = 1; K, dt and Δx are folded into `erosion_k` (the cell size cancels, since √(256 A) / 16 = √A). Large rivers (large A, large F) are cut nearly to their receiver's height in one step; ridges (A small) barely change. This produces dendritic valleys in few iterations.
- A cell whose receiver is not lower (a depression, where G4 fell back to the flood parent) is not incised. Depressions therefore survive erosion and become lake candidates for E10.
- On iterations where routing is not recomputed (`erosion_route_interval` > 1), rcv, order and F are reused. The solve is still valid: `order` lists every receiver before its donors whatever the heights, and the "no incision unless lower" rule keeps every update a move toward the receiver.
- With `erosion_iterations` = 0 the step is skipped.

### E9. Quantization (D1)

```
q = mul_div(h.raw, 10, 2^32)            // decimetres, RHA
e = (c in O) ? min(-1, q) : max(1, q)
```

So no cell is 0, and the sign of every cell matches E6. Bound: land h ≤ max(m) ≤ 3000 m and ocean h ≥ min(m) ≥ -3000 m (E7 stays inside the curve, erosion only lowers land and never below the floor, diffusion takes weighted averages of land values and 0, conditioning fills to a level that is some land cell's height), so |e| ≤ 30000 and fits `i16`.

### E10. Depression conditioning (D6, D5)

On the integer grid e (decimetres):

```
(L, parent, order) = G3 routing flood with keys e, ocean mask O
D = the components (G2, 8-connected) of cells c not in O with L[c] > e[c],
    with equivalence: adjacent cells belong together only if their L are equal
for each component C of D:
    lake(C) = |C| >= lake_min_cells  and  max over c in C of (L[c] - e[c]) >= lake_min_depth_dm
    if not lake(C): e[c] = L[c] for every c in C
```

Filling a component to its flood level changes no other cell's flood level (L[x] is the minimax height of paths to the sea, and every path through a filled cell already reached that height), so the remaining depressions of e are exactly the components that passed the test. Hydrology (P1-02) labels every remaining depression as a lake and needs no size thresholds of its own.

### E11. Write (one transaction)

Write `elevation` (`i16`, codec 1) as the stage's only layer.

## 6. Invariants

For every seed, N and tuning inside the bounds:

1. Every value of `elevation` is in [-30000, 30000] and none is 0.
2. The ocean (e < 0) is 4-connected, contains every cell with min(x, y, N−1−x, N−1−y) < M, and so touches all four edges.
3. Every 8-connected land component has at least I_min cells.
4. Every depression component of e (G3 on e, components as in E10) has at least lake_min_cells cells and a maximum depth of at least lake_min_depth_dm.
5. Land values are ≤ RHA(10 × last land height) and ≥ 1; ocean values are ≥ RHA(10 × first ocean height) and ≤ -1.
6. The land/ocean classification after E6 equals the sign of the final layer (erosion and conditioning do not move the coast).
7. With `erosion_iterations` = 0, the layer equals E9 and E10 applied to E7's heights.
8. The layer is a pure function of (seed, N, `worldgen.elevation`): independent of thread count, platform and compiler.

## 7. Determinism notes

| Step | Rule | Notes |
| --- | --- | --- |
| E1, E2, E4 | D1, parallel by G6 row blocks | location-keyed noise only; no shared state |
| E3, E5 (threshold), E7 (rho_j) | D7, G1 | values, independent of algorithm |
| E5 (set O), E6 | D5 | sets; labels by G2 |
| E7 (mapping) | D1, parallel | |
| E8 routing | D6, single-threaded | pop order (key, seq) |
| E8 receivers, F | D1, parallel | |
| E8 accumulation, solve | D4 in G3 order | integer sums; the solve is order-dependent and follows `order` exactly |
| E8 diffusion | D2, parallel | reads `hn`, writes `h` |
| E9 | D1 | |
| E10 | D6 then D5 | |

Overflow bounds (largest map N = 2048, any tuning in bounds), so no checked operation fails on valid input:

- Noise sample coordinates: map fraction |Uw| ≤ 1.5, × cycles ≤ 64, × 2^9 for the tenth octave ≤ 49152; metres ≤ 32760 + 5000, / 64 × 2^9 ≤ 302080. Both far below 2^31; the final frequency updates are ≤ 65536.
- |c| ≤ 10, |cs| ≤ 1, |r| ≤ 24.
- E7: (m_{j+1} − m_j).raw ≤ 6000 × 2^32 < 2^45, (r − rho_j).raw ≤ 48 × 2^32 < 2^38; `mul_div` uses a 128-bit product; the result is below 2^45.
- E8: A ≤ 2^22, so A << 32 < 2^54 and S ≤ 2048; F ≤ 2048; ONE + F fits; |h − base| ≤ 3000 m; |S4 − 4 hn| ≤ 24000 m.
- E9: |h.raw × 10| < 2^48.
- Priority keys in E8 are `Fx` raw heights of at most 3000 m (< 2^44) and in E10 decimetres.

No floating point, no `<cmath>`, no `Rng`, no unordered containers. Quantiles are values; components are sets with labels by G2; the flood is ordered by (key, seq).

## 8. Performance and fallbacks

Budget (P1-00 11): 2 / 8 / 35 s at 512 / 1024 / 2048. Expected split at 1024 on 6 cores: noise passes E2 and E4 (37 `noise2` evaluations per cell, parallel) about 1 s; quantiles under 0.5 s; erosion 16 iterations at about 0.35 s each (routing flood about 0.2 s, the rest about 0.15 s) about 5.6 s; conditioning 0.3 s. Erosion dominates, and within it the routing flood (a heap of up to N² entries) and the solve (one `isqrt_u64` and one `div` per land cell).

Erosion was chosen as a grid method with an implicit tree solve rather than droplets: droplet erosion moves sediment along one path at a time and is inherently sequential and order-sensitive (each droplet sees its predecessors' changes), needs millions of droplets with interpolated gradients, and offers no deterministic parallelism. The stream-power solve is O(N²) per iteration with a defined order, stable for any step size, and produces drainage networks directly.

If measurement shows erosion too slow, in this order:

1. **Without changing output:** a radix or bucket heap for the flood (pop order is defined by (key, seq), not by the structure); the `isqrt` fast path or a table for A < 2^16; the basin-parallel solve of P1-00 section 8 / amendment A1 (each drainage basin's cells are solved by one thread in `order`; basins write disjoint cells).
2. **Tuning only:** `erosion_route_interval` = 2 or 4 (the flood then runs on every second or fourth iteration), then fewer iterations with a larger `erosion_k`. Changes output; shipped tuning change (PATCH bump).
3. **Algorithm change (spec amendment):** erode a 2:1 downsampled grid at 2048 and add the difference back bilinearly.

## 9. Database rows written

One `layer` row: `name` = `elevation`, `width` = `height` = N, `format` = `i16`, codec 1, in one stage transaction. Its id is the next layer id (1 in a normal world).

## 10. Tests required

The test writer may call the step functions through library seams (interim names): `elevation_fields(seed, N, tuning, threads)` returning the c, ab and r grids as `Fx` raw; `elevation_classify(r, N, tuning)` returning the ocean mask after E6; `elevation_remap(r, ocean, tuning)`; `erode(h, ocean, tuning, threads)`; `condition_depressions(e, ocean, tuning)`; and `elevation_grid(seed, N, tuning, threads)` for the whole stage. The shared algorithms (G1 to G5) and `condition_depressions` accept any W × H grid, so hand examples need not be square or have an ocean border all round.

### 10.1 Unit tests with worked examples

**Derived integers.** margin M for N = 512, 1024, 2048 with `edge_margin_frac` "0.03" is 15, 30, 61; with "0" it is 1. I_min for 50000 m² is 196; for 0 it is 0; for 256 it is 1; for 257 it is 2. `lake_min_depth_dm` for "1" is 10, for "0.05" is 1 (raw 214748365 × 10 / 2^32 = 0.500000001, rounds to 1).

**Ridged noise, structural.** For key `stream_key(0, "geo.elevation.ridge")` and 64 points (a, b) on a grid of metres, `ridged_m` equals the value the test computes from `noise2` with the pseudocode of 4.1; every value is in [0, ONE]; with octaves 1, weight 0 it equals `mul(ONE - |n|, ONE - |n|)` (clamped) for n = `noise2(substream(K, 0), mul(a, f), mul(b, f))`.

**E2/E4 structural.** For seed 0, N = 64 and the fixture tuning, `elevation_fields` at 32 cells chosen by the test equals c, ab and r computed by the test from core functions and the formulas of E2 to E4, with c_sea and w_b computed by the test with G1 over the full 64 × 64 field.

**E5/E6 on a given mask.** N = 8, M = 1, I_min = 2, provisional ocean P ('.') and land ('#'):

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

Expected: (2, 2) is not 4-connected to the edge through P and becomes land; (6, 1) is an 8-component of 1 cell and is sunk; (5, 6) touches (4, 5) diagonally, so it is part of the main component. Final land: 19 cells, (1..3, 1), (1..4, 2), (1..5, 3), (2..5, 4), (3..4, 5), (5, 6). (The seam takes P directly for this test.)

**E7 remap.** Land curve (0 → 1 m), (0.5 → 100 m), (1 → 1000 m); land raw values r (as `Fx` raw) 10, 20, 30, 40, 50 (n_L = 5). Then k = 0, 2, 4 and rho = 10, 30, 50. r = 20: j = 0, h = 1 + 99 × 10/20 = 50.5 m (raw 216895848448). r = 40: j = 1, h = 100 + 900 × 10/20 = 550 m. r = 50: j = 2 (last), h = 1000 m. r = 10: h = 1 m. Ocean cells with the default ocean curve (ranks 0, 0.5, 0.9, 1) and raw values -3, -2, -1 (n_O = 3): k = 0, 1 (1.5 floored), 2 (2.7 floored), 2 (min(2, 3)); rho = -3, -2, -1, -1. r = -3 → -400 m; r = -2 → j = 1 (rho_1 = -2 ≤ -2 < rho_2 = -1) → -120 m; r = -1 → j = 3 (the largest j with rho_j ≤ -1) → -0.5 m.

**E8 solve, by hand.** A 5 × 1 grid, cell 0 ocean, cells 1 to 4 land with heights 10, 20, 40, 80 m; `erosion_k` = HALF, `erosion_diffusion` = 0, floor 0.1 m, one iteration. Routing: every land cell's receiver is its west neighbour (code 4). A = 4, 3, 2, 1. S raw = 8589934592, 7439056896, 6073942016, 4294967296; F raw = 4294967296, 3719528448, 3036971008, 2147483648. Results (raw): cell 1: 21474836480 (5 m exactly); cell 2: 55999923266; cell 3: 123833544243; cell 4: 270342770534 (about 13.0385, 28.8322, 62.9441 m).

**Diagonal factor.** A = 4, `erosion_k` = HALF, diagonal receiver: F raw = `mul(4294967296, 3037000500)` = 3037000500.

**Diffusion.** One land cell with hn = 10 m whose 4-neighbours are ocean, 20, 30 and 40 m; `erosion_diffusion` = "0.1" (raw 429496730): S4 = 90 m, S4 − 4hn = 50 m, result raw 64424509460 (15.0000000047 m).

**E9 quantization.** Land 50.5 m (raw 216895848448) → 505; land 0.04 m (raw 171798692) → q = 0 → 1; land 0.05 m (raw 214748365) → 1; ocean -0.05 m (raw -214748365) → q = -1 → -1; land 12.345 m (raw 53021371269) → 123.

**E10 conditioning.** (a) 5 × 5, border -5, interior 30 except the centre 28; lake_min_cells 2, lake_min_depth_dm 5: the centre is a 1-cell depression of depth 2 and is filled to 30. (b) The 8 × 7 grid of P1-02 section 5 with lake_min_cells 2 and lake_min_depth_dm 5: the 4-cell depression (flood level 28, depths 8, 6, 7, 5) is kept unchanged; with lake_min_depth_dm 9 its four cells become 28; with lake_min_cells 5 (depth 5) they also become 28.

**Tuning validation.** Each named check of 4.2 fails on a minimal counter-example (first rank 0.1; last rank 0.9; ranks 0, 0.5, 0.5, 1; heights decreasing; a land height below `land_floor_m`; an ocean height of 0); a float literal in any field is a P0-03 error; a missing field is an error.

### 10.2 Property tests

Over seeds 0 to 63 at N = 128 and seeds 0 to 3 at N = 512, fixture tuning, checking every invariant of section 6 on the output, by code independent of the stage (the test computes components, floods and flood levels with its own straightforward implementations; a Dijkstra-style minimax search for flood levels is acceptable). In addition:

- E8 alone, on E7's output for those seeds: no land cell is raised by the solve; every land cell after the iteration loop is ≥ floor; ocean cells are unchanged; the maximum land height does not increase.
- `quantile` values used by E3 and E5 equal the values found by sorting a copy.
- With `erosion_iterations` = 0, `elevation_grid` equals E9 and E10 applied to `elevation_remap`'s output (invariant 7).

### 10.3 Determinism tests

- Threads (D10): `elevation_grid` for seeds 0 and 42 at N = 128 and 512 with 1, 2 and 5 threads gives identical grids.
- Repeat: two runs in one process give identical grids (no hidden state).
- Map sizes are independent worlds: the layer for seed 0 at 512 does not depend on whether a 1024 world was generated first in the same process.

### 10.4 Golden values

Recorded by the validation specialist, fixture tuning, the codec-independent layer hash of P0-04 8 (`layer elevation <hex>`), compared on every preset: seeds 0, 42 and the phrase "The Unmapped" at N = 512; seed 1 at N = 1024; and seed 0 at N = 128 through the library (cheap enough for every debug and sanitizer run). Also the SHA-256 of the r grid (as little-endian `int64`) for seed 0 at N = 128, which isolates the noise composition from erosion when a golden value breaks.

## 11. Open questions

1. **One height distribution for every world.** E7 forces the land's heights onto one curve, so every world has the same share of lowland and peak and the same highest pre-erosion point (2400 m by default). Variety between worlds comes from where things are, not how much of each. A seeded scale (for example a per-world multiplier on the curve's upper part, drawn from a location hash) would add variety; not specified until the sweep shows whether it is needed.
2. **No sediment deposition and no uplift.** The erosion model only cuts. Alluvial plains and deltas would need a transport-limited term; steady-state mountain building would need uplift. Both are possible extensions of E8; neither is needed for milestone 0 unless the critic finds valleys too V-shaped or lowlands too noisy.
3. **Depression thresholds live here, not in hydrology.** Lakes are decided by E10's thresholds so that the stored elevation has only lake-worthy depressions and hydrology is pure analysis (P1-02 section 9). The cost is that lake tuning sits in `worldgen.elevation`.
4. **Mountain belts at map scale.** Belts follow map-fraction noise (P1-00 2.2), so a 2048 world has the same number of belts as a 1024 world, each wider and holding more ridges. If 2048 worlds look like a few giant massifs, `belt_noise.cycles` could be made a function of N; that would break the "same outline at every size" property for belts only.
5. **Coastline detail at small maps.** At 512 the continental noise's finest octave is about 170 m across; metre-scale hills give finer detail. Whether 512 coasts look too smooth is for the critic.
