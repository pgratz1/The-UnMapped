# P1-03 Climate

Status: approved by the lead, 2026-11-16; revised 2026-11-17 after the first implementation (branch `p1/03-climate`) and a tuning experiment (branch `p1/03-climate-experiment-c4`, commit `2689076`): C4 becomes a triangle kernel, diagonal winds take their taps across the wind (C5), shipped `moisture_path_mm_km` 32000 and `rain_oro_mm` 2500, statistics ranges from the experiment, seams as implemented, corrections. Revised again 2026-11-17 after the world critic's review and the determinism audit: shipped `coast_rain_mm` 75 (coast experiment, 11.1), entry guards of the C4 seams and of `climate_temperature_cell` (10), section 7's C4 bound corrected, limitations 8 to 10 and open questions 4 and 7. Paul has provisionally accepted the choices of taste (open questions 1 and 2). Phase P1 Geography, stage 3 (`climate`). Design doc section 4 (stage 3: "temperature from latitude and elevation; rainfall from prevailing wind and rain shadow"; "scale is compressed"). Depends on P0-01 to P0-04, P1-00 (shared algorithms G1 to G8, rules D1 to D10, section 2.2 map-size convention), P1-01 (the `elevation` layer) and P1-02 (`water`, `water_depth`, `coast_dist`). The climate merge is `GENERATOR_VERSION` 0.7.0; this revision lands before that merge and needs no further bump. Tuning values marked "guess" and every statistics range are still provisional until the validation sweep (section 11).

## 1. Purpose

Give every cell an annual mean temperature and an annual precipitation. Biomes (P1-04) classify on them; resources (P1-05) score timber and fresh water with rainfall; history reads them through biomes and habitability; the runtime will use them for vegetation and weather colour.

The map is small (8 to 33 km) and its terrain is one continent with steep ranges (P1-01: peaks 1,200 to 2,600 m, ranges as walls, most land low). So climate is a local gradient with a strong orographic effect, not a stack of planetary belts:

- **Temperature** falls from south to north by a fixed amount per map (the latitude span is a map fraction, P1-00 2.2, proposals "Climate's latitude span is a fraction of the map"), and falls with height at a lapse rate. The ranges are what make cold places.
- **Rainfall** comes from one prevailing wind per world. Saturated air comes off the sea, rains more where it is forced up a slope, and loses moisture as it rains, so the far side of a range is dry (rain shadow) and inland air is drier than coastal air. A small bonus near every coast stands for sea breezes and fog.

Every distance in this stage is in metres (P1-00 2.2): a 2048 map has a wider continent and so a drier interior, with the same rain shadow behind each range (limitation 7).

## 2. Inputs

| Input | Source |
| --- | --- |
| `elevation` (`i16`, dm) | P1-01 |
| `water` (`u8`), `water_depth` (`u16`, dm), `coast_dist` (`i16`, m) | P1-02 |
| Tuning | definition `worldgen.climate` (section 4) |
| World seed, N | `meta.seed`, `param.map_size` |

Streams (P0-01 7.3; registered in `streams.hpp`):

| Key | Stream name | Use |
| --- | --- | --- |
| K_wind | `geo.climate.wind` | `Rng(K_wind)`, one `weighted_index` call, sequential (C2; D9) |
| K_temp | `geo.climate.temperature` | noise key of `fbm_m` (C3) |
| K_rain | `geo.climate.rainfall` | noise key of `fbm_m` (C6) |

The test-only stream `test.climate` (section 10.2) is not registered in `streams.hpp` and is never used by the stage.

**Surface.** S[i] = `elevation`[i] + `water_depth`[i], an `int64` in decimetres: 0 on ocean (P1-02 invariant 7: depth = −e), the lake level on lake cells, e on dry land and river cells. The air sees the water surface, not the lake bed.

**Preconditions**, checked first, in this order; each violation is fatal with a message that begins `climate: input precondition: ` and, because the stage fails inside `generate`, leaves no world file and no `.partial` (P0-04 4):

1. The four input layers `elevation`, `water`, `water_depth` and `coast_dist` exist, each N × N with its catalogue type (`i16`, `u8`, `u16`, `i16`).
2. Every `water` value is in 0..3.
3. S = 0 on every ocean cell (`water` = 3); 1 ≤ S on every other cell; S ≤ 32767 on every cell (the bound behind C4's overflow argument, section 7; every finished world meets it, since lake levels are below the highest peak).

## 3. Outputs

Two layers, N × N, codec 1, written in this order in one stage transaction (layer ids 9 and 10 in a full run):

| Layer | Type | Units | Range | Meaning |
| --- | --- | --- | --- | --- |
| `temperature` | `i16` | tenths of a degree Celsius | [−600, 500] | annual mean air temperature at the surface S; on ocean, at sea level |
| `rainfall` | `u16` | millimetres per year | [`rain_min_mm`, 10000] | annual precipitation; defined on ocean cells too (rain over the sea), which no later stage uses |

Both are in the P1-00 catalogue already ("fixed"). This spec adds no further climate layer and no table: **no schema change, no catalogue change.** The prevailing wind direction is not stored (open question 4).

## 4. Tuning

Fields of `worldgen.climate`, all required (P1-00 6.1), in schema declaration order:

| Field | Type and bounds | Default | Status | Meaning |
| --- | --- | --- | --- | --- |
| `temp_north_c` | fx(−30, 40) | 5 | guess (accepted by Paul, provisionally) | sea-level annual mean at the north edge (v = 0) |
| `temp_south_c` | fx(−30, 40) | 9 | guess (accepted by Paul, provisionally) | sea-level annual mean at the south edge (v = 1) |
| `lapse_c_per_km` | fx(0, 15) | "6.5" | designed (standard atmosphere) | cooling per km of height |
| `temp_noise` | `fbm_m` | wavelength 6000, octaves 3, gain "0.5" | guess | local temperature variation |
| `temp_noise_amp_c` | fx(0, 5) | 1 | guess | its weight, °C per unit of noise |
| `wind_weights` | array(int(0, 1000), 8, 8) | [1, 1, 1, 1, 1, 1, 1, 1] | guess (open question 1; accepted by Paul, provisionally) | relative chance that the prevailing wind blows **toward** D8 code 0 (E) to 7 (NE) |
| `smooth_radius_m` | int(0, 4000) | 320 | guess | half-width of the support of the triangle kernel through which the air sees the terrain (C4); each of its two box passes has half-width (`smooth_radius_m` div 16) div 2 cells |
| `rain_base_mm` | int(0, 5000) | 800 | guess | precipitation from saturated air over level ground |
| `rain_oro_mm` | int(0, 100000) | 2500 | calibrated (experiment, 11.1) | extra precipitation per unit of upslope gradient along the wind (rise per metre of path) |
| `moisture_path_mm_km` | int(100, 2000000) | 32000 | calibrated (experiment, 11.1) | rain integrated along the path (mm × km) that would use up saturated air; sets how fast air dries |
| `loss_max` | fx(0, "0.9") | "0.25" | designed | cap on the share of the air's moisture lost in one step |
| `ocean_recharge_m` | int(16, 100000) | 500 | guess | open water over which air takes up moisture again (per step it regains step/recharge of its deficit) |
| `coast_rain_mm` | int(0, 5000) | 75 | calibrated (coast experiment, 11.1) | rain added at the coast, every direction (sea breeze, fog) |
| `coast_rain_reach_m` | int(16, 32767) | 1500 | guess | distance inland over which that bonus falls linearly to 0 |
| `rain_noise` | `fbm_m` | wavelength 5000, octaves 3, gain "0.5" | guess | local rainfall variation |
| `rain_noise_amp` | fx(0, 1) | "0.4" | guess | relative weight: rainfall × (1 + amp × noise) |
| `rain_min_mm` | int(0, 5000) | 250 | guess | floor of the stored rainfall |

`fbm_m` is the noise record of P1-01 4.1. Named checks (P0-03 7.5; error at the definition's header, form as P1-01 4.3): `wind_weights_positive`: the sum of `wind_weights` is at least 1 (message `wind_weights must not all be 0`).

```toml
# game/data/worldgen/climate.toml (and the fixture worldgen/tests/data/geo/worldgen/climate.toml,
# and worldgen/tests/data/geo-small/worldgen/climate.toml)
[worldgen.climate]
temp_north_c = 5
temp_south_c = 9
lapse_c_per_km = "6.5"
temp_noise = { wavelength_m = 6000, octaves = 3, gain = "0.5" }
temp_noise_amp_c = 1
wind_weights = [1, 1, 1, 1, 1, 1, 1, 1]
smooth_radius_m = 320
rain_base_mm = 800
rain_oro_mm = 2500
moisture_path_mm_km = 32000
loss_max = "0.25"
ocean_recharge_m = 500
coast_rain_mm = 75
coast_rain_reach_m = 1500
rain_noise = { wavelength_m = 5000, octaves = 3, gain = "0.5" }
rain_noise_amp = "0.4"
rain_min_mm = 250
```

Every fixed-id directory gets this file (P1-00 6.1: every id must be defined where the kind's directory exists), so adding `climate` to the `worldgen` id set changes `content_hash` of every world: a bump and re-recorded golden hashes in the implementing branch (P1-00 6.4; proposals "Every world's hash depends on the shipped tuning files"). That bump is the 0.7.0 of the climate merge; the changed values above are part of the same, not yet merged, change.

Fixed constants (not tuning): the 3-tap upwind weights (1, 2, 1)/4 and the tap shapes of C5; the step lengths 16 m and 16√2 m (`SQRT2`, P1-00 amendment A1); off-map air is saturated (q = ONE) at sea level; C4 is two box passes of half-width (r div 2).

## 5. Algorithm

Notation as P1-01 5.0: every value is `Fx` unless stated, every operation the P0-01 operation of that name with its rounding, statements evaluated in the order written. X = `from_int(16x + 8)`, Y = `from_int(16y + 8)` (metres); V = `from_ratio(2y + 1, 2N)` (P1-00 2.1). On a W × H test grid, N in V is replaced by H and lines and rows run over W or H as stated. Steps C1 to C7 in order.

### C1. Preconditions (D1)

Section 2. Each violation is fatal.

### C2. Prevailing wind (sequential, D9)

```
rng = Rng(K_wind)
k_w = rng.weighted_index(wind_weights)          // one call; P0-01 6.3
(dx, dy) = the D8 vector of code k_w (P1-00 2.3)
```

The wind blows **toward** k_w: k_w = 0 is a wind from the west (a westerly). The draw happens once per world and depends only on the seed and the weights.

### C3. Temperature (D1, G6 row blocks)

```
t_sea = temp_north_c + mul(temp_south_c − temp_north_c, V)
lap   = div_int(mul_int(lapse_c_per_km, S), 10000)          // S in dm; 10000 dm = 1 km
tn    = mul(temp_noise_amp_c, fbm_m(K_temp, X, Y, temp_noise))
T     = (t_sea − lap) + tn
temperature = clamp(round_int(mul_int(T, 10)), −600, 500)
```

### C4. Smoothed surface: a triangle kernel (D2, two box passes, G6 row blocks)

r = `smooth_radius_m` div 16 (cells; 20 with the default) and h = r div 2 (10 with the default). The **box** of half-width h at cell (x, y) is the window [x − h, x + h] × [y − h, y + h] clipped to the map (on a test grid, to W × H), with c_w cells (a rectangle, 1 ≤ c_w ≤ (2h + 1)²). Clipping is the only edge rule: an h at or beyond the grid simply clips, and every h ≥ max(W, H) − 1 gives every cell the whole grid as its window.

```
// Pass 1, on the integers S:
sum1 = Σ S over the box of half-width h        // int64, exact
hs1  = from_ratio(sum1, c_w)                    // Fx, decimetres, RHA
// Pass 2, on the raw values of hs1:
sum2 = Σ hs1.raw over the box of half-width h  // int64, exact
hs   = div_int(from_raw(sum2), c_w)            // Fx, decimetres: RHA(sum2 / c_w)
```

Each pass may be computed in any split (a row pass of sums along x, then a column pass over the row sums, in G6 row blocks); every sum is an exact integer, so the result does not depend on the split or the thread count. Pass 2 reads the complete hs1 grid: all of pass 1 is finished before any of pass 2 starts. h = 0 (r ≤ 1) gives hs = `from_int(S)`.

Away from the edges, hs is S under the separable weights (1, 2, …, 2h + 1, …, 2, 1)/(2h + 1)² along each axis (up to the rounding of hs1): a triangle with support half-width 2h, which is r for even r (20 cells, 320 m, with the default, the same support as the single box of radius 20 that this replaces) and r − 1 for odd r. Near an edge each pass renormalises over its own clipped window, so the weights there are not a clipped triangle (example in 5.1).

Why two passes: along the wind, the slope of a single box-filtered surface is the difference of the two edge rows of the window (moving the window one cell adds the row that enters and drops the row that leaves), so terrain roughness at the window's edges passes straight into the rise of C5 and into rainfall as comb-like bands across the wind. The triangle's weights fall to zero at its edges, and the second pass removes those bands (experiment, 11.1: gone on every map where they showed).

### C5. Moisture sweep (D4)

Moisture q ∈ [0, ONE] is the air's share of saturation; p is precipitation in mm/yr (`Fx`). An **orthogonal** wind has dx = 0 or dy = 0 (codes 0, 2, 4, 6); a **diagonal** wind has both non-zero (codes 1, 3, 5, 7).

**Order.** Orthogonal winds sweep by lines. If dx ≠ 0 the lines are columns, processed x = 0, 1, …, W − 1 when dx = 1 and x = W − 1, …, 0 when dx = −1; if dx = 0 the lines are rows, y ascending when dy = 1, descending when dy = −1. Each cell reads only values of the previous line, so the cells of one line may be computed in any order or in parallel; the lines themselves are sequential.

Diagonal winds sweep in **raster order**, one sequential pass: rows in the order of dy (y = 0, 1, …, H − 1 when dy = 1; y = H − 1, …, 0 when dy = −1), and within each row the cells in the order of dx (x = 0, 1, …, W − 1 when dx = 1; x = W − 1, …, 0 when dx = −1). A cell reads the cell before it in its own row, so the cells of a row are not independent; the sweep is single-threaded.

**Upwind taps** a, b, c of cell (x, y), b the centre with weight 2:

```
orthogonal, dx ≠ 0 (columns):  a = (x − dx, y − 1),  b = (x − dx, y),       c = (x − dx, y + 1)
orthogonal, dx = 0 (rows):     a = (x − 1, y − dy),  b = (x, y − dy),       c = (x + 1, y − dy)
diagonal:                      a = (x − dx, y),      b = (x − dx, y − dy),  c = (x, y − dy)
```

| Code | Wind toward (dx, dy) | a | b (weight 2) | c |
| --- | --- | --- | --- | --- |
| 1 | SE (1, 1) | (x − 1, y) | (x − 1, y − 1) | (x, y − 1) |
| 3 | SW (−1, 1) | (x + 1, y) | (x + 1, y − 1) | (x, y − 1) |
| 5 | NW (−1, −1) | (x + 1, y) | (x + 1, y + 1) | (x, y + 1) |
| 7 | NE (1, −1) | (x − 1, y) | (x − 1, y + 1) | (x, y + 1) |

So a diagonal wind reads the diagonal upwind neighbour and the two orthogonal upwind neighbours, which lie on either side of the wind's axis: three taps across the wind, as for an orthogonal wind. In the raster order every tap is computed before it is read: a is the previous cell of the same row, b and c lie in the previous row.

**Off-map taps.** A tap outside the map has q = ONE and hs = 0 (saturated air at sea level: the margin is ocean). On real maps the edge band is ocean (P1-01 `edge_margin_frac`, at least one cell), so air entering the map recharges over open water and the rule never shows; on a test grid without an ocean border it does (5.1, cell (4, 0) of the SE sweep).

With taps a, b, c:

```
q_up  = div_int((q_a + mul_int(q_b, 2)) + q_c, 4)
hs_up = div_int((hs_a + mul_int(hs_b, 2)) + hs_c, 4)
```

**Step.** Diagonal winds step 16√2 m, the others 16 m:

```
step_m  = diagonal ? mul_int(SQRT2, 16)  : from_int(16)
step_dm = diagonal ? mul_int(SQRT2, 160) : from_int(160)
rec     = min(ONE, div(step_m, from_int(ocean_recharge_m)))           // once per world
```

**Ocean cell** (`water` = 3):

```
q = q_up + mul(ONE − q_up, rec)
p = mul(q_up, from_int(rain_base_mm))
```

**Other cell** (land, river, lake):

```
rise = max(0, hs − hs_up)                                  // dm, Fx
g    = div(rise, step_dm)                                  // upslope gradient along the wind
p    = mul(q_up, from_int(rain_base_mm) + mul_int(g, rain_oro_mm))
lam  = min(loss_max, div_int(mul(p, step_m), 1000 × moisture_path_mm_km))
q    = q_up − mul(q_up, lam)
```

In words: rain is proportional to the moisture left in the air and to how steeply the air is being lifted; the air loses moisture in proportion to the rain it has dropped over the step, so a long climb wrings it out and the lee gets what is left. The lateral 1-2-1 taps spread the shadow sideways a little at every step (about √(n/2) cells after n steps), so a shadow is not a one-cell-sharp streak. Lakes are treated as land (they are small); only the sea recharges the air.

**Symmetry.** C4 and C5 commute with the eight symmetries of the square grid (the mirrors x ↦ W − 1 − x and y ↦ H − 1 − y, the transpose (x, y) ↦ (y, x), and their compositions) when the wind vector is mapped by the same symmetry: for every grid G (any W × H; the transpose swaps W and H), every code k and every symmetry σ, the sweep of σ(G) with wind σ(k) equals σ applied to the sweep of G with wind k, q and p exactly. The mirrors were exact before this revision; the diagonal taps across the wind add the transpose, which maps SE and NW onto themselves and swaps SW with NE. Hence the four orthogonal winds are exact rotations and mirrors of one another, and so are the four diagonal winds; no symmetry relates a diagonal wind to an orthogonal one (they step different lengths with different tap shapes). The rest of the stage is not symmetric (the latitude gradient and the noise are fixed to the map).

### C6. Rainfall (D1, G6 row blocks)

```
rn  = fbm_m(K_rain, X, Y, rain_noise)
f   = max(0, ONE + mul(rain_noise_amp, rn))
pr  = mul(p, f)
d   = max(0, coast_dist)                                    // metres; ocean counts as 0
cb  = (d ≥ coast_rain_reach_m) ? 0 : from_ratio(coast_rain_mm × (coast_rain_reach_m − d), coast_rain_reach_m)
rainfall = clamp(round_int(pr + cb), rain_min_mm, 10000)
```

### C7. Write

`temperature`, then `rainfall`, in one transaction.

### 5.1 Worked examples

Computed by the spec writer with an exact-integer Python reference of this section (an implementation of the P0-01 operations checked against P0-01's own vectors: `stream_key(0, "geo.elevation.erosion")` = 0x7d5f47a9629481b8, `Rng(0)` first output 0x99ec5f36cb75f2b4, `parse_decimal("0.4")` raw 1717986918; the same reference reproduces every example of the first version of this section). The cells marked "by hand" were also checked by hand. The C4 and C5 examples were cross-checked against the experiment build: commit `2689076` extracted with `git archive` into a scratch directory, its `worldgen` library built out of tree (GCC, Release), and a small driver calling `climate_triangle_mean` and `climate_sweep` with `UNMAPPED_EXP_DIAG_ACROSS=1` (the experiment's switch for the diagonal taps, `experiment.md`); every value below and 300 random cases (grids up to 14 × 14, all eight codes, r 0 to 40, random `water` and tuning) agree raw for raw. Noise amplitudes are 0 unless stated, shipped values otherwise.

**C2 wind.** `wind_weights` all 1: seeds 0 to 11 give k_w = 1, 3, 4, 1, 1, 6, 0, 2, 5, 1, 2, 1; seed 42 gives 6. Weights [0, 0, 0, 0, 5, 0, 0, 0] give 4 for every seed (one draw is still consumed).

**C3 temperature** (`temp_noise_amp_c` 0; `temp_north_c` 5, `temp_south_c` 9, lapse "6.5" = raw 27917287424):

| N | y | S (dm) | t_sea (raw, value) | lap (raw, value) | `temperature` |
| --- | --- | --- | --- | --- | --- |
| 1024 | 511 | 12000 | 30056382464 (6.998047) | 33500744909 (7.8) | −8 |
| 1024 | 0 | 0 | 21483225088 (5.001953) | 0 | 50 |
| 1024 | 1023 | 0 | 38646317056 (8.998047) | 0 | 90 |
| 512 | 0 | 30000 | 21491613696 (5.003906) | 83751862272 (19.5) | −145 |
| 512 | 255 | 5 | 30047993856 (6.996094) | 13958644 (0.00325) | 70 |
| 512 | 100 | 7000 | 24847056896 (5.785156) | 19542101197 (4.55) | 12 |

(By hand, first row: V = 1023/2048, t_sea = 5 + 4 × 1023/2048 = 6.998046875 exactly; lap = 6.5 × 12000 / 10000 = 7.8; T = −0.80195…, × 10 → −8.)

**One box pass** (the seam `climate_box_mean`, which is pass 1 of C4), half-width 1, a 4 × 3 grid S (rows y = 0 to 2): `0 10 20 30`, `0 40 50 60`, `0 0 70 80`. hs1 in dm: row 0 `12.5 20 35 40`; row 1 `8.3333 21.1111 40 51.6667` (raw 35791394133, 90671531804, 171798691840, 221906643627); row 2 `10 26.6667 50 65` (26.6667 is raw 114532461227). For example (0, 1): window x 0..1, y 0..2, sum 50 over 6 cells.

**C4 triangle**, the same grid with r = 2 (`smooth_radius_m` 32 to 47), so h = 1 and pass 1 is the box pass above. hs (raw), rows y = 0 to 2:

```
66512340764  98028651710 148733126732 178956970667
70588582874 106711378805 166761384518 201624853618
70986265031 111748686127 182138427923 221906643627
```

(dm: `15.4861 22.8241 34.6296 41.6667`, `16.4352 24.8457 38.8272 46.9444`, `16.5278 26.0185 42.4074 51.6667`.) By hand, (0, 0): the window is x 0..1, y 0..1, c_w = 4; sum2 = 53687091200 + 85899345920 + 35791394133 + 90671531804 = 266049363057 (hs1 12.5, 20, 8.3333, 21.1111); RHA(266049363057 / 4) = 66512340764. The same grid with r = 3 gives the same hs (h = 1); r = 0 or 1 gives `from_int(S)`; r ≥ 6 (h ≥ 3 ≥ max(W, H) − 1) gives every cell `from_ratio(360, 12)` = raw 128849018880 (30 dm).

**C4 triangle, edge clipping**: a 9 × 9 grid with S = 900 at (4, 4) and 0 elsewhere, r = 4 (h = 2). Row y = 4 of hs, dm: `12 18 21.6 28.8 36 28.8 21.6 18 12` (raw 51539607552, 77309411328, 92771293594, 123695058125, 154618822656, …, symmetric). Unclipped, the value at x is 900 × w(x) × w(4) with the weights w = (1, 2, 3, 4, 5, 4, 3, 2, 1)/25 (36 = 900 × 5/25 × 5/25; 21.6 = 900 × 3/25 × 5/25), which holds for x = 2 to 6; at x = 0 and 1 the second pass's window is clipped to 3 and 4 columns, which gives 12 and 18 in place of the unclipped 7.2 and 14.4.

**C5 sweep, wind E (k_w = 0)**, r = 0 (hs = S), a 6 × 3 grid, columns 0 and 5 ocean (S = 0), S rows: `0 20 60 120 80 0`, `0 30 90 160 100 0`, `0 10 40 80 60 0`. Shipped `rain_base_mm` 800, `rain_oro_mm` 2500, `moisture_path_mm_km` 32000, `loss_max` 0.25, `ocean_recharge_m` 500 (rec = 0.032, raw 137438953).

- (1, 0), by hand: the upwind column is outside the map, so q_up = ONE, hs_up = 0; rise 20 dm, g = 20/160 = 0.125 (raw 536870912); p = 800 + 2500 × 0.125 = 1112.5 mm; lam = 1112.5 × 16 / 32000000 = 0.00055625 (raw 2389076); q = 4294967296 − 2389076 = 4292578220.
- (2, 1), by hand: taps (1, 0), (1, 1), (1, 2) with q 4292578220, 4292242676, 4292913765: q_up = RHA(17169977337 / 4) = 4292494334; hs_up = (20 + 60 + 10)/4 = 22.5 dm; rise 67.5, g = 0.421875 (raw 1811939328); p = q_up × (800 + 1054.6875), raw 7961235585091 (1853.620 mm).

q (raw), rows y = 0 to 2:

```
4294967296 4292578220 4289950298 4286870704 4286633270 4288375589
4294967296 4292242676 4288516008 4284675152 4284396886 4285979494
4294967296 4292913765 4290620742 4288210164 4287302465 4288699479
```

p (mm, three decimals of the raw value):

```
800.000 1112.500 1463.423 1852.908 798.767 798.732
800.000 1268.750 1853.620 2203.391 798.350 798.271
800.000  956.250 1229.199 1384.716 798.891 798.794
```

Column 4 descends (rise 0), so it gets only base rain from the depleted air; column 5 is ocean, whose own rain uses q_up and which starts recharging.

**C5 sweep, wind SE (k_w = 1)**, same grid and tuning, raster order (rows y = 0, 1, 2; x ascending in each). Step 16√2: step_dm = raw 971840160000 (226.27 dm), rec raw 194368032.

- (1, 0), by hand: taps a = (0, 0) (ocean, q = ONE, hs 0), b = (0, −1) and c = (1, −1) off the map: q_up = ONE, hs_up = 0; g = div(20 dm, step_dm) = raw 379625062 (0.0883883); p = 800 + 2500 × 0.0883883 = 1020.971 mm (raw 4385036491800); lam raw 3100689; q = 4291866607.
- (2, 1): taps a = (1, 1), b = (1, 0), c = (2, 0) with q 4290924843, 4291866607, 4289918572: q_up = 4291144157; hs_up = (30 + 2 × 20 + 60)/4 = 32.5 dm; rise 57.5; p raw 6159041656350 (1434.014 mm); q 4286792934.
- (4, 0): a = (3, 0) (q 4287755798, hs 120), b and c off the map: q_up = 4293164422, hs_up = 30 dm, rise 50, p 1351.859 mm. The cell is on the lee slope of the E sweep, but here two of its taps are the sea-level margin: the off-map rule at work on a grid without an ocean border.

q (raw), rows y = 0 to 2:

```
4294967296 4291866607 4289918572 4287755798 4289060547 4293557436
4294967296 4290924843 4286792934 4283581311 4284617726 4289340761
4294967296 4291444379 4287597741 4283771548 4281470892 4285462311
```

p (mm):

```
800.000 1020.971 1407.416 1959.521 1351.859 799.725
800.000 1076.019 1434.014 1653.805  798.523 798.902
800.000  827.427  799.079  798.365  797.936 798.146
```

**C6 rainfall** (shipped values; rn given):

| p (mm) | `coast_dist` | rn | cb (mm) | `rainfall` |
| --- | --- | --- | --- | --- |
| 800 | 16 | 0 | 74.2 | 874 |
| 800 | 1500 | 0 | 0 | 800 |
| 100 | 3000 | 0 | 0 | 250 (floor) |
| 12000 | 50 | 0 | 72.5 | 10000 (cap) |
| 800 | 700 | 0.5 | 40 | 1000 |
| 800 | −40 (ocean) | −0.3 | 75 | 779 |

For the two sweeps above, with `coast_dist` 16 on the land columns and −16 on the ocean columns, `rainfall` rows: wind E `875 1187 1538 1927 873 874`, `875 1343 1928 2278 873 873`, `875 1030 1303 1459 873 874`; wind SE `875 1095 1482 2034 1426 875`, `875 1150 1508 1728 873 874`, `875 902 873 873 872 873`.

## 6. Invariants

For every world and every tuning inside the bounds:

1. `temperature` ∈ [−600, 500] and `rainfall` ∈ [`rain_min_mm`, 10000] on every cell.
2. **Lapse.** With `temp_noise_amp_c` = 0: on cells of one row, S(a) ≥ S(b) implies `temperature`(a) ≤ `temperature`(b); on cells with the same S, `temperature` is non-decreasing from north to south when `temp_south_c` ≥ `temp_north_c`.
3. **Moisture stays a share.** Every q of C5 is in [0, ONE]: a 1-2-1 mean of values in [0, ONE] is in [0, ONE]; the ocean update adds at most ONE − q_up; on other cells lam ≤ `loss_max` < 1.
4. **Rain shadow.** On every non-ocean cell of C5, q ≤ q_up (air only dries over land), and where rise = 0, p = mul(q_up, `rain_base_mm`) (a lee slope gets only base rain from the air that reaches it).
5. **No wind, no shadow.** With `rain_oro_mm` = 0 and both noise amplitudes 0, rainfall on land depends only on the moisture path, not on the terrain.
6. Pure function of the seed, N, the four input layers and `worldgen.climate`; independent of thread count (D10).
7. **Symmetry** of C4 and C5 under the eight grid symmetries, as stated at the end of C5. Each hs lies between the smallest and largest S of the cells within distance 2h (C4 is a mean of means).

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| C2 | D9, sequential | one `Rng`, one `weighted_index` call; no other `Rng` in the stage |
| C3, C6 | D1, parallel by G6 row blocks | location-keyed noise only |
| C4 | D2, two passes, each parallel by G6 row blocks | exact integer sums; any split gives the same sums; pass 2 starts after pass 1 has finished |
| C5 | D4 | orthogonal winds: lines in the stated order, within a line D1 (reads only the previous line); diagonal winds: one sequential raster sweep in the stated order |

No floating point, no new math function: `fbm_m` (P1-01 4.1), `mul`, `div`, `div_int`, `mul_int`, `from_ratio`, `from_raw`, `round_int`, `clamp`, `SQRT2`.

Overflow (N = 2048, any tuning inside the bounds, S ∈ [0, 32767] by precondition 3):

- C3: |t_sea| ≤ 40; lap ≤ 15 × 32767 / 10000 < 50 (`mul_int` raw ≤ 15 × 32767 × 2^32 < 2^51); |tn| ≤ 5; T ∈ [−85, 45] before the clamp; × 10 fits.
- C4: h ≤ 4000 div 16 div 2 = 125, so a window holds at most 251² = 63001 cells. sum1 ≤ 63001 × 32767 < 2^31. hs1.raw ≤ 32767 × 2^32; sum2 ≤ 63001 × 32767 × 2^32 ≈ 8.866 × 10^18 < 2^63 − 1 ≈ 9.223 × 10^18. The implementation's running column sum drops the leaving row before adding the entering one, so it never holds more than one window: peak 8.866 × 10^18, a margin of 3.6 × 10^17. The row pass's prefix along a whole row is at most W × 32767 × 2^32 ≤ 2048 × 1.407 × 10^14 ≈ 2.9 × 10^17. In general a sum of more than MAX = (2^63 − 1) div (32767 × 2^32) = 65538 values of hs1.raw can overflow, which is what the seams' entry guards of section 10 exclude; raising the bound of `smooth_radius_m` above 4000, or admitting S above 32767, needs a new argument (for example 128-bit window sums). hs ≤ 32767 dm.
- C5: rise ≤ 32767 dm, step_dm ≥ 160, so g < 205; `mul_int(g, rain_oro_mm)` < 2.05 × 10^7; p < 2.06 × 10^7 mm (q ≤ 1); mul(p, step_m) < 4.7 × 10^8 (< 2^31); 1000 × `moisture_path_mm_km` ≤ 2 × 10^9 fits `int64` as a `div_int` divisor; rec ≤ ONE.
- C6: f ≤ 2; pr < 4.2 × 10^7 < 2^31; `coast_rain_mm` × reach ≤ 5000 × 32767 fits; round_int then clamp.

## 8. Performance

Budget (P1-00 11): 0.3 / 1.5 / 6 s at 512 / 1024 / 2048. Cost per cell: 6 `noise2` octaves (C3, C6; parallel), two constant-time box passes (C4; parallel), and about 20 integer operations in the single-threaded sweep (C5: about 0.05 s at 1024 by estimate, for either sweep order). Memory: S, hs1, hs, q and p as `int64` grids, about 40 bytes per cell (170 MB at 2048), within P1-00's 2 GB; q needs only two lines or rows, and hs1 can be freed after pass 2.

## 9. Report and render

**`report`** (P1-00 10.3), appended to `[geography]` whenever both `temperature` and `rainfall` are present, with or without the hydrology layers (after the hydrology lines when those are printed), in this order ("land" is `elevation` > 0, as in 10.3):

| Key | Value |
| --- | --- |
| `temperature_c` | land percentiles 0, 10, 50, 90, 100 of `temperature`, each printed as tenths with one decimal (the `metres1` formatter applied to tenths of a degree, e.g. `-14.5`), separated by spaces |
| `rainfall_mm` | land percentiles 0, 10, 50, 90, 100 of `rainfall`, integers |
| `land_below_0c` | ratio4(land cells with `temperature` < 0, land cells) |
| `rain_contrast` | ratio4(p90, p10) of land `rainfall` (the two values above); `n/a` if p10 = 0 |

With no land cell each of the four keys prints a single `n/a` (not one per percentile), as `elevation_m` does (P1-00 appendix C 6).

**Statistics** (for P1-00 13.1; all **provisional**: measured on the experiment worlds of 11.1, 512 seeds 0–9, 1024 seeds 0–9, 2048 seeds 1 and 5, with the shipped algorithm and tuning, widened by about 10%; to be replaced by the validation specialist's sweep with the 13.1 seed set, taking the measured min–max widened by about 10% as typical and the hard bound only where a value outside would mean a defect):

| Statistic | All seeds (hard) | Typical (≥ 90%) | Measured (experiment, min–max) |
| --- | --- | --- | --- |
| `temperature_c` p50 | within the catalogue range (by construction) | 4.5–7.0 °C | 5.0–6.4 °C |
| `rainfall_mm` p50, 512 | ≥ `rain_min_mm` (by construction) | 700–1030 mm | 774–932 (E1) |
| `rainfall_mm` p50, 1024 | as above | 650–880 mm | 718–798 |
| `rainfall_mm` p50, 2048 | as above | 600–745 mm | 662–677 |
| `rain_contrast` | descriptive | 1.5–4.6 | 1.64–4.21 |
| `land_below_0c` | descriptive | — | 0.019–0.085 (temperature does not depend on the rainfall variants) |

The measured values are those of variant E1 of the coast experiment (11.1), which is the shipped algorithm and tuning (variant D with `coast_rain_mm` 75), each widened by about 10% of the value (low end × 0.9, high end × 1.1, rounded). The real acceptance test of climate is the biome mix (P1-04 9), whose rainfall thresholds need re-deriving (limitation 7).

**Palettes** (P1-00 10.2 interpolation rule; registered under `auto`). The palette names are `temperature`, which suits `i16` layers only, and `rainfall`, which suits `u16` layers only (P1-00 appendix C 1: naming a palette that does not suit the layer is a usage error; under `auto` an unsuitable registered palette falls back to `gray`).

`temperature` (`i16`, t = value in tenths):

| t | −300 | −100 | 0 | 50 | 100 | 200 | 300 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RGB | 49,54,149 | 69,117,180 | 171,217,233 | 255,255,191 | 253,174,97 | 215,48,39 | 165,0,38 |

`rainfall` (`u16`, t = mm):

| t | 0 | 250 | 500 | 750 | 1000 | 1500 | 2500 | 4000 | 10000 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| RGB | 140,81,10 | 191,129,45 | 223,194,125 | 246,232,195 | 199,234,229 | 128,205,193 | 53,151,143 | 1,102,94 | 0,60,48 |

Example: `temperature` 25 (2.5 °C) between stops 0 and 50: R = 171 + floor((2·84·25 + 50)/100) = 171 + 42 = 213; G = 217 + floor((2·38·25 + 50)/100) = 236; B = 233 + floor((2·(−42)·25 + 50)/100) = 233 + floor(−20.5) = 212.

## 10. Tests required

Library seams, as implemented in `worldgen/include/worldgen/stages/climate.hpp` (namespace `worldgen::stages`; `Fx` is `worldgen::Fx`, `Grid` and `Direction` are from `worldgen/geo/grid.hpp`). All work on any W × H grid with index y × W + x; each raises `FatalError` on an array of the wrong length, an invalid grid, threads outside [1, 256] or a violated precondition; `threads` never changes a result. The engineer may add helpers but should not change these without a spec note.

```cpp
ClimateTuning climate_tuning_from(const data::Record& record);        // the 17 fields of section 4
ClimateTuning climate_tuning_from(const data::ContentSet& content);   // of definition worldgen.climate

// C1: S = elevation + water_depth; fatal "climate: input precondition: ..." unless the
// preconditions 2 and 3 of section 2 hold (sizes as precondition 1).
std::vector<std::int64_t> climate_surface(Grid grid, std::span<const std::int16_t> elevation,
                                          std::span<const std::uint8_t> water,
                                          std::span<const std::uint16_t> water_depth);
// C2: the code the wind blows toward. Fatal unless 8 weights, none negative, sum >= 1.
Direction climate_wind(std::uint64_t seed, std::span<const std::int64_t> weights);
// C3 for one cell of row y of a grid of `height` rows; noise is the fbm_m value at the cell.
// Fatal unless 0 <= y < height <= INT32_MAX.
std::int16_t climate_temperature_cell(std::int64_t y, std::int64_t height, std::int64_t s_dm, Fx noise,
                                      const ClimateTuning& tuning);
std::vector<std::int16_t> climate_temperature(std::uint64_t seed, Grid grid, std::span<const std::int64_t> surface,
                                              const ClimateTuning& tuning, std::uint32_t threads = 1);
// One box pass of half-width r_cells on integer S: hs1 of C4 with h = r_cells.
// Entry guard (below) with r = r_cells.
std::vector<Fx> climate_box_mean(Grid grid, std::span<const std::int64_t> surface, std::int64_t r_cells,
                                 std::uint32_t threads = 1);
// C4: the triangle kernel with r = r_cells (two passes of half-width r_cells div 2).
// Entry guard (below) with r = r_cells div 2, which covers both passes.
std::vector<Fx> climate_triangle_mean(Grid grid, std::span<const std::int64_t> surface, std::int64_t r_cells,
                                      std::uint32_t threads = 1);
// C5 (single-threaded). wind > 7 is fatal.
struct ClimateSweep { std::vector<Fx> q; std::vector<Fx> p; };
ClimateSweep climate_sweep(Grid grid, std::span<const std::uint8_t> water, std::span<const Fx> hs,
                           Direction wind, const ClimateTuning& tuning);
// C6 for one cell and for a grid.
std::uint16_t climate_rainfall_cell(Fx p, std::int64_t coast_dist, Fx rn, const ClimateTuning& tuning);
std::vector<std::uint16_t> climate_rainfall(std::uint64_t seed, Grid grid, std::span<const Fx> p,
                                            std::span<const std::int16_t> coast_dist,
                                            const ClimateTuning& tuning, std::uint32_t threads = 1);
// C1 to C6.
struct ClimateLayers { std::vector<std::int16_t> temperature; std::vector<std::uint16_t> rainfall; };
ClimateLayers climate_layers(Grid grid, std::span<const std::int16_t> elevation,
                             std::span<const std::uint8_t> water, std::span<const std::uint16_t> water_depth,
                             std::span<const std::int16_t> coast_dist, const ClimateTuning& tuning,
                             std::uint64_t seed, std::uint32_t threads = 1);
// The stage: reads worldgen.climate and the four layers (precondition 1), runs climate_layers, writes C7.
void run_climate(StageContext& context);
```

Both C4 seams are kept: `climate_triangle_mean` is C4 and is what `climate_layers` calls; `climate_box_mean` is its first pass alone, kept so that a single box can be tested on its own (the one-box example of 5.1). The second pass, on `Fx` raw values, is not a seam. `climate_sweep` takes the smoothed surface hs, not S.

**Entry guards of the C4 seams.** Checked before any summation; each failure is fatal with the prefix `climate: input precondition: `. With MAX = INT64_MAX div (32767 × 2^32) = 65538 (the largest number of hs1.raw values whose sum always fits `int64`, section 7) and r the guard's radius (r_cells for `climate_box_mean`; r_cells div 2 for `climate_triangle_mean`, the same for both its passes):

1. r_cells ≥ 0;
2. every S in [0, 32767];
3. W ≤ MAX (the row pass sums a prefix along a whole row);
4. min(W, 2r + 1) × min(H, 2r + 1) ≤ MAX (one window).

The code holds compile-time assertions that the stage can never trip the guard: (2 × (4000 div 16 div 2) + 1)² = 63001 ≤ MAX (the `smooth_radius_m` bound of section 4) and the largest map size, 2048, ≤ MAX. `climate_layers` and `run_climate` reach C4 only through `climate_triangle_mean`, so a hand-built tuning or grid outside these bounds is fatal there too, never a wrapped sum.

### 10.1 Unit tests

1. Every worked example of 5.1 exactly (raw values where given).
2. The C3 noise term: for seed 0, N = 64 and the shipped tuning, at 16 cells chosen by the test, `temperature` equals the test's own evaluation of C3 with `fbm2`.
3. Box and triangle at radii as large as the grid or larger: `climate_box_mean` with r_cells ≥ max(W, H) − 1, and `climate_triangle_mean` with r_cells div 2 ≥ max(W, H) − 1, give every cell `from_ratio(ΣS, W·H)`. `climate_triangle_mean` with r_cells 0 or 1 gives `from_int(S)`; with odd r_cells it equals the result for r_cells − 1.
4. **Symmetry** (invariant 7). On the 200 random fixtures of 10.2 and on a non-square 7 × 5 grid of random S with a one-cell ocean border, for every code k = 0 to 7 and each of the three generators σ (mirror in x, mirror in y, transpose), the test transforms `water` and hs by σ, sweeps with the code of σ(dx, dy), transforms the result back and compares q and p with the sweep of the original exactly. All 24 pairs must hold, including the transpose for the diagonal codes (which the former vertical taps failed). Also `climate_triangle_mean` commutes with the three σ.
5. Wind: weights with a single non-zero entry always give that code; a weights array summing to 0 is a load error (named check); 7 or 9 entries are a type error.
6. Tuning validation: each bound of section 4 rejects a value one step outside it; a float literal is a P0-03 error.
7. Preconditions: each violation of section 2 (a missing input layer, a layer of the wrong size or type, a `water` value of 4, S ≠ 0 on an ocean cell, S < 1 on another cell, S > 32767) is fatal with the prefix `climate: input precondition: `; from `worldgen-cli generate` it exits 1 and leaves no world file and no `.partial`.
8. Report: on a world with `temperature` and `rainfall` but without the hydrology layers, the four climate keys are printed; on a grid with no land each prints a single `n/a`.
9. Seam guards (fatal, prefix `climate: input precondition: `, all with S = 32767 on every cell; each case through both `climate_box_mean` and `climate_triangle_mean`): a 260 × 255 grid with r_cells 258 (triangle h = 129, window 259 × 255 = 66045; box window 260 × 255 = 66300); a 4096 × 17 grid with r_cells 8192 (the whole grid, 69632 cells); a 65539 × 1 grid with r_cells 0 (the row alone, W > MAX). These are the audit's three reproductions, which overflowed before the guard. Just inside the limits (the stage's extremes: 2048 × 2048 with r_cells 250, and a window of exactly 63001 cells) the seams return exact results. `climate_temperature_cell` with height 2^31 is fatal; with height INT32_MAX and y = height − 1 it returns a value.

### 10.2 Property tests

Over seeds 0 to 63 at N = 128 (fixture tuning; elevation and hydrology layers from the P1-01 and P1-02 stages) and 200 random fixtures: invariants 1 to 5 and 7 by code independent of the stage.

Random fixture f (f = 0 to 199) is a 34 × 34 grid: `Rng(substream(stream_key(0, "test.climate"), f))` draws the wind code with `uniform_u64(8)` first, then `uniform_i64(1, 3000)` for each interior cell (x and y in 1..32) in index order, which is that cell's `elevation` and S (`water` 0, `water_depth` 0); the one-cell border is ocean with `elevation` −10, `water` 3 and `water_depth` 10; `coast_dist` is P1-02 H7 on that ocean mask. The climate tuning is the shipped tuning with `wind_weights` one-hot at the drawn code, and the world seed is f. `test.climate` is a test-only stream.

### 10.3 Determinism tests

Threads 1, 2 and 5 give identical layers (seeds 0 and 42 at N = 128 and 512), and identical `climate_triangle_mean` results on the random fixtures; resume (P1-00 X2): climate run on layers read back from a world written after hydrology equals the uninterrupted pipeline.

### 10.4 Golden values

Layer hashes of `temperature` and `rainfall` with fixture tuning for seeds 0, 42 and "The Unmapped" at N = 512, seed 1 at 1024, seed 0 at 128, recorded by the validation specialist after the implementation passes. Their inputs come from the hydrology golden cases, so an upstream change re-records them together. Any golden file written against the first version of this spec (the test writer's `p1-03-climate.txt` on `p1/03-climate-tests`) is regenerated from the revised reference.

## 11. Known limitations and calibration

1. **First calibration was from a floating-point prototype**, not this arithmetic: the spec writer's Python model of C3 to C6 with substitute noise (smoothed Gaussian fields of similar wavelength) and an approximate hydrology (G3/G4 without lake rerouting), run on generator 0.3.0 worlds (512 seeds 0–11, 1024 seeds 0–5, 12, 18) with wind code = seed mod 8. Values marked "guess" were chosen there so that the five natural biomes of P1-04 all appear (section P1-04 11). `rain_oro_mm` and `moisture_path_mm_km` have since been retuned on the implementation (11.1); the other guesses stand until the validation sweep.
2. **One wind, no seasons.** The model has no seasonal cycle, no secondary wind and no temperature effect of the sea (continentality). On a 16 km map these would be small next to the orographic effect.
3. **Faint lee streaks along the wind.** Advection along one D8 direction leaves faint stripes parallel to the wind in the rain field downwind of sharp terrain. The triangle kernel, the 1-2-1 taps, the wetter air of the shipped `moisture_path_mm_km` and the rain noise hide most of them; faint fingers remain in places (experiment: the s5-2048 stripe in the lee of a N–S arm under a north wind is much fainter from B32 on), judged acceptable. The comb-like bands across the wind that the single box produced are gone (C4). Diagonal winds take three taps across the wind and are exact rotations and mirrors of one another (C5, symmetry), but are not rotations of the orthogonal winds, so a world's rain field is not the 45° rotation of the same world under the neighbouring orthogonal wind.
4. **Rain on steep windward faces is high**: the ranges are walls (P1-01), and even smoothed over 320 m their windward gradient is large. With the shipped `rain_oro_mm` 2500 the land maximum (`rainfall_mm` p100) has median 5798 / 5074 / 4214 mm at 512 / 1024 / 2048 and reaches 8671 mm on one 512 world (variant D); the 10000 mm cap is no longer reached on the experiment worlds (at 5000 it was reached on 512 maps). Lowering `rain_oro_mm` to about 2000 would keep every 512 world under 6000–7000 mm at the cost of more contrast.
5. **Rivers do not follow the rain.** `flow_accum` counts cells (P1-02 open question 2); this spec adds no rainfall-weighted discharge (open question 3).
6. **Passes do not let moisture through visibly.** A natural pass or gap (P1-01) is narrower than the 640 m support of C4, so the smoothed surface the air sees has no notch there and the lee of a pass is as dry as the lee of the range beside it.
7. **Rainfall depends on map size.** The moisture path is in metres (section 1), so a wider continent has a drier interior: with the shipped tuning (variant E1) land median 829 / 761 / 670 mm at 512 / 1024 / 2048, land (not lake) under 700 mm 16.7% / 33.9% / 57.7% pooled over the worlds of a size (variant D, with the former coast bonus: 880 / 786 / 676 mm). P1-04's fixed 700 mm forest threshold (and its 500 mm marsh minimum) would therefore give much more open land at 2048 than at 512 (the critic's biome trial: grassland median 6% / 19% / 37%); P1-04 re-derives its rainfall thresholds per map size once climate merges (P1-04 11, item 6; open question 6).
8. **Temperature is mostly height and the same in every world** (world critic S4). Elevation explains 88–93% of land temperature variance in every 1024 world; the uplands sit at mid-map (the north third holds 0–13% upland), so the north–south span changes little in practice, and at 1 °C the noise adds a residual of only 0.13–0.23 °C. Every world has the same treeline, about 1,000 m. A per-world offset is open question 7; raising `temp_noise_amp_c` to about 2 or dropping it are data changes.
9. **Some worlds have almost no lee.** Where the main range lies on the downwind coast the climate is "coast wet, interior slightly dry": the lee lowland share is ≤ 5% of land in 5 of the critic's 42 worlds (512 seeds 1, 4 and 12; 1024 seed 4; 2048 seed 4) against up to 24% elsewhere. Realistic, but those are the dullest climates; the wind is drawn independently of the terrain (open question 1).
10. **Cosmetic: the cliff-coast jump and ocean rainfall.** On a windward cliff coast the ocean cell gets about base rain and the first land cell the full orographic rain (critic: about 920 against 3,900–4,300 mm at s2-1024), because the ocean rule ignores the rise of the smoothed surface; physically the rain would start a little offshore. And every ocean cell gets the full coast bonus (d = 0 on ocean), so the sea renders wetter than most land (about 875 mm with the shipped tuning). Nothing reads ocean rainfall; on land the front is right. If it matters for the images, mask the ocean in the `rainfall` render or fade the bonus over the sea with |`coast_dist`|.

### 11.1 Calibration on the first implementation (2026-11-17)

Experiment on branch `p1/03-climate-experiment-c4` (commit `2689076`; the branch was first named `p1/03-climate-experiment`; generator 0.7.0 code otherwise; report `~/.cache/the-unmapped/out/p1-03-climate/exp/experiment.md`, with rainfall contact sheets and crops beside it). Worlds: 512 seeds 0–9, 1024 seeds 0–9, 2048 seeds 1 and 5; terrain and hydrology identical in every variant. The first implementation reproduced every worked example of the first version of this spec, and an independent test reference agreed, so the defects in its maps (comb-like bands across the wind; windward lowlands of 2048 maps as dry as the lee, 31% of land under 500 mm; the 10000 mm cap reached on 512 maps) were consequences of the algorithm and tuning as specified.

| Variant | C4 | `moisture_path_mm_km` | `rain_oro_mm` | Diagonal taps |
| --- | --- | --- | --- | --- |
| spec (base) | one box, r = 20 | 16000 | 5000 | vertical triple of the previous column (first version) |
| A | triangle, two boxes r = 10 | 16000 | 5000 | vertical |
| B32 | triangle | 32000 | 5000 | vertical |
| B64 | triangle | 64000 | 5000 | vertical |
| C32 | triangle | 32000 | 3500 | vertical |
| C32o25 | triangle | 32000 | 2500 | vertical |
| D (adopted) | triangle | 32000 | 2500 | across the wind (C5) |

Results: `rainfall_mm` p50 and `rain_contrast` as min / median / max over the worlds of a size, p100 as median (max); land shares as means over worlds; lowland (land, not lake, below 200 m) rainfall as cell-weighted means, where a lowland cell is "lee" if tracing against the wind for up to 400 steps meets a cell of 800 m or more, and "windward" if it is not lee and tracing with the wind meets one.

| Variant | N | p50 mm | p100 mm | `rain_contrast` | land < 500 mm | land < 700 mm | windward / lee lowland mm |
| --- | --- | --- | --- | --- | --- | --- | --- |
| spec | 512 | 811 / 835 / 1054 | 8298 (10000) | 3.18 / 5.38 / 7.20 | 3.2% | 28.3% | 1691 / 677 |
| spec | 1024 | 691 / 738 / 821 | 6670 (9571) | 2.35 / 3.93 / 6.12 | 15.2% | 44.6% | 1083 / 536 |
| spec | 2048 | 583 / 600 / 616 | 5916 (6801) | 2.83 / 2.97 / 3.11 | 31.0% | 66.5% | 668 / 379 |
| A | 512 | 802 / 826 / 1025 | 9786 (10000) | 3.13 / 5.72 / 8.00 | 4.5% | 29.2% | 1638 / 668 |
| A | 1024 | 688 / 732 / 808 | 8580 (10000) | 2.46 / 4.25 / 6.57 | 16.1% | 45.0% | 1067 / 529 |
| A | 2048 | 582 / 600 / 617 | 6955 (8177) | 2.88 / 3.03 / 3.17 | 31.2% | 66.5% | 658 / 375 |
| B32 | 512 | 846 / 886 / 1077 | 10000 (10000) | 3.09 / 5.36 / 7.21 | 0.1% | 15.8% | 1702 / 794 |
| B32 | 1024 | 760 / 805 / 874 | 9314 (10000) | 2.22 / 3.78 / 5.79 | 1.9% | 30.2% | 1136 / 664 |
| B32 | 2048 | 688 / 700 / 712 | 8617 (9369) | 2.18 / 2.32 / 2.46 | 11.9% | 49.8% | 775 / 518 |
| B64 | 512 | 876 / 930 / 1111 | 10000 (10000) | 3.03 / 5.20 / 6.74 | 0.0% | 4.8% | 1738 / 884 |
| B64 | 1024 | 818 / 853 / 908 | 9958 (10000) | 2.11 / 3.58 / 5.31 | 0.0% | 14.3% | 1177 / 771 |
| B64 | 2048 | 760 / 766 / 771 | 9833 (10000) | 1.82 / 1.96 / 2.09 | 0.5% | 30.0% | 856 / 648 |
| C32 | 512 | 839 / 890 / 1032 | 8362 (10000) | 2.50 / 4.05 / 5.26 | 0.0% | 12.0% | 1456 / 799 |
| C32 | 1024 | 761 / 804 / 867 | 6919 (9903) | 1.92 / 2.95 / 4.35 | 0.5% | 28.1% | 1037 / 680 |
| C32 | 2048 | 686 / 698 / 710 | 6400 (6967) | 1.98 / 2.07 / 2.16 | 8.4% | 50.4% | 743 / 535 |
| C32o25 | 512 | 834 / 892 / 1013 | 6314 (8740) | 2.11 / 3.18 / 4.02 | 0.0% | 9.1% | 1290 / 802 |
| C32o25 | 1024 | 763 / 803 / 859 | 5252 (7486) | 1.73 / 2.41 / 3.42 | 0.1% | 26.1% | 970 / 691 |
| C32o25 | 2048 | 685 / 696 / 707 | 4848 (5291) | 1.86 / 1.92 / 1.98 | 6.2% | 51.1% | 720 / 548 |
| D | 512 | 817 / 880 / 993 | 5798 (8671) | 1.91 / 3.06 / 4.02 | 0.0% | 11.1% | 1221 / 789 |
| D | 1024 | 736 / 786 / 829 | 5074 (6610) | 1.67 / 2.25 / 2.97 | 0.4% | 30.4% | 931 / 671 |
| D | 2048 | 668 / 676 / 685 | 4214 (5291) | 1.86 / 1.96 / 2.06 | 11.4% | 56.3% | 682 / 522 |

Temperature is the same in every variant: p50 5.0–6.4 °C, `land_below_0c` 0.019–0.085. (Variant C64, `moisture_path_mm_km` 64000 with `rain_oro_mm` 3500, was also run and is in the report.)

**Coast bonus experiment** (world critic S3: the bonus put a wet ring on every coast, lee coasts included). Branch `p1/03-climate-experiment` recreated from `p1/03-climate` at `3bea7d5` (algorithm D); report `~/.cache/the-unmapped/out/p1-03-climate/exp-coast/experiment-coast.md`; the same worlds; terrain, hydrology and temperature identical in every variant. E0 reproduces the shipped worlds of variant D.

| Variant | Coast bonus |
| --- | --- |
| E0 | as C6 with `coast_rain_mm` 150 (variant D) |
| E1 (adopted) | `coast_rain_mm` 75, data only |
| E2 | the 150 mm bonus multiplied by the cell's own q_up of C5 (experiment switch only) |
| E3 | `coast_rain_mm` 0 |

Report statistics as above (p50, p100 and `rain_contrast` min / median / max; p100 as median (max)). Land (not lake) shares by band are pooled over the cells of all worlds of a size; "forest" is rainfall ≥ 820 / 700 / 600 mm at 512 / 1024 / 2048 (the critic's per-size thresholds). Lee coast: a land cell within 3 km of the coast from which tracing against the wind crosses more than 3.2 km of land before reaching ocean or the map edge; windward otherwise. Forest shares on coasts are for 0–300 m, 300–800 m, 0.8–1.5 km and 1.5–3 km from the coast.

| Variant | N | p50 mm | p100 mm | `rain_contrast` | land < 500 mm | land < 700 mm | forest | windward coast forest | lee coast forest |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| E0 | 512 | 817 / 880 / 993 | 5798 (8671) | 1.91 / 3.06 / 4.02 | 0.0% | 11.0% | 63.2% | 95 / 83 / 54 / 50% | 42 / 29 / 30 / 42% |
| E0 | 1024 | 736 / 786 / 829 | 5074 (6610) | 1.67 / 2.25 / 2.97 | 0.4% | 29.7% | 70.3% | 100 / 100 / 96 / 83% | 72 / 58 / 47 / 47% |
| E0 | 2048 | 668 / 676 / 685 | 4214 (5291) | 1.86 / 1.96 / 2.06 | 11.5% | 55.8% | 69.9% | 100 / 100 / 100 / 100% | 78 / 72 / 58 / 62% |
| E1 | 512 | 774 / 829 / 932 | 5790 (8659) | 1.91 / 3.08 / 4.21 | 0.0% | 16.7% | 52.9% | 82 / 69 / 47 / 50% | 14 / 19 / 29 / 42% |
| E1 | 1024 | 718 / 761 / 798 | 5074 (6610) | 1.64 / 2.26 / 2.98 | 0.5% | 33.9% | 66.1% | 99 / 99 / 94 / 83% | 40 / 41 / 41 / 47% |
| E1 | 2048 | 662 / 670 / 677 | 4214 (5291) | 1.80 / 1.88 / 1.97 | 12.6% | 57.7% | 68.0% | 100 / 100 / 100 / 100% | 59 / 56 / 53 / 62% |
| E2 | 512 | 811 / 872 / 989 | 5798 (8668) | 1.93 / 3.08 / 4.11 | 0.0% | 12.8% | 61.0% | 94 / 81 / 52 / 50% | 31 / 25 / 30 / 42% |
| E2 | 1024 | 728 / 780 / 824 | 5074 (6610) | 1.69 / 2.28 / 3.00 | 0.5% | 31.5% | 68.5% | 100 / 100 / 96 / 83% | 55 / 51 / 45 / 47% |
| E2 | 2048 | 665 / 674 / 682 | 4214 (5291) | 1.87 / 1.97 / 2.06 | 12.5% | 56.6% | 68.5% | 100 / 100 / 100 / 100% | 63 / 60 / 55 / 62% |
| E3 | 512 | 735 / 780 / 870 | 5761 (8648) | 1.98 / 3.14 / 4.39 | 0.0% | 25.8% | 44.2% | 66 / 55 / 42 / 50% | 5 / 15 / 29 / 42% |
| E3 | 1024 | 706 / 738 / 777 | 5074 (6610) | 1.64 / 2.31 / 2.97 | 1.1% | 39.0% | 61.0% | 94 / 93 / 89 / 83% | 15 / 24 / 36 / 47% |
| E3 | 2048 | 656 / 664 / 672 | 4214 (5291) | 1.75 / 1.82 / 1.89 | 14.1% | 59.6% | 66.4% | 100 / 100 / 100 / 100% | 41 / 43 / 49 / 62% |

(The land shares here are pooled over cells and exclude lakes, so they differ slightly from the means over worlds of the first table: E0 is variant D.) No variant adds a step at the coast: the mean rainfall difference across land–ocean pairs is 107.4–107.5 mm in every variant, the land's own orographic and lee rain.

Decisions (the lead, 2026-11-17):

- **Triangle kernel** (A against spec): the comb-like bands across the wind are gone on every map where they showed (s5-1024, s7-1024, s1-2048); almost all of the diagonal column striping was the box kernel too. Statistics barely move.
- **`moisture_path_mm_km` 32000** (was 16000): at 16000 the windward lowlands of 2048 maps were as dry as the lee (31% of land under 500 mm, windward lowland 658 mm). 32000 raises the 2048 windward lowland to 775 mm and the lee to 518 mm with the rain shadow still reading as dry; 64000 overshoots (land under 500 mm all but disappears, the lee at 2048 reaches 648 mm).
- **`rain_oro_mm` 2500** (was 5000): at 5000 the 10000 mm cap was reached on 512 maps, and at 3500 still on one; 2500 removes it. The price is lower contrast (`rain_contrast` median 3.2 / 2.4 / 1.9 against 5.4 / 3.9 / 3.0).
- **Diagonal taps across the wind** (D against C32o25): adopted for the exact transpose symmetry (C5) and the cleaner reading "three taps across the wind", not for a visible fix once C4 is a triangle. Its effect on the statistics is small (p50 lower by 10–25 mm, p100 lower, lee lowland about the same).
- **`coast_rain_mm` 75** (was 150; E1 against E0, after the critic's S3): it flattens the lee-coast profile (forest from the shore inland 40 / 41 / 41 / 47% at 1024 against 72 / 58 / 47 / 47%), so the wet rim on lee coasts largely goes, while windward coasts stay wet (99 / 99 / 94 / 83%); a thin rim remains on some 2048 lee coasts. Rainfall p50 falls by 0 to 7%, `rain_contrast` barely moves, temperature and p100 do not change. E2 does too little at the shipped moisture path (air reaching a lee coast is still 0.6–0.9 saturated) and would be a new mechanism; E3 overshoots into lee shores drier than inland, cuts 512 forest from 63% to 44% and turns the sea buff in the renders.

## 12. Open questions for the lead

1. **Wind direction per world** (a matter of taste for Paul; provisionally accepted). Chosen: drawn uniformly from the 8 D8 directions, so the wet side differs between worlds. Alternatives: always a westerly (weights `[1, 0, 0, 0, 0, 0, 0, 0]`), or westerlies favoured (e.g. `[4, 2, 1, 1, 1, 1, 1, 2]`). Both are a data change only. Recommendation: keep uniform for variety; the north–south temperature gradient is fixed, so the wind is the main thing that makes two continents' climates differ.
2. **Temperature span and level** (taste; provisionally accepted). Chosen: 5 °C at sea level in the north to 9 °C in the south, 6.5 °C per km lapse: a cool temperate land in keeping with dark high fantasy, alpine conditions above about 1,000 m. The span is a map fraction (proposals "Climate's latitude span is a fraction of the map"), the same at every map size. Recommendation: approve; revisit with the biome images.
3. **Rainfall-weighted discharge** (P1-02 open question 2). Recommendation: none in P1. Rivers are sized by area; biomes and resources use `rainfall` and the river raster directly; a rain-shadow river being as large as a wet-side river is acceptable (and real: exotic rivers). If history or the runtime needs discharge, a later stage can compute it from `flow_dir` and `rainfall` without changing any existing layer.
4. **Store the wind direction** (world critic S2). It is not in the world file, so `report` cannot print it, the viewer cannot draw it, every reviewer has to infer it, and the runtime will need it for weather direction, cloud drift and vegetation lean. Recommendation: store it with the next schema change, schema 3 (P1-04's `biome` table), and not later: either a one-row table `climate` with the column `wind_toward` (the C2 code) or a `meta` key `climate.wind_toward`. The table is preferred, since it is where any later per-world climate value (such as the offset of question 7) would also go; `report` then prints the code and the viewer an arrow. This is a schema change (hard rule 7: version bump, spec update, proposals entry) made in the P1-04 revision or a climate follow-up, not in this merge.
5. **Lakes do not recharge the air.** Large lakes would moisten their lee shore. Recommendation: leave it; the lake share is capped at 3% of land (P1-01).
6. **Size dependence of rainfall** (limitation 7). Chosen: keep the moisture path in metres, so a larger map is a larger land with a drier interior (P1-00 2.2), and let P1-04 adapt its thresholds. The alternative, a moisture path in map fractions, would make 512 and 2048 maps equally wet but give a 2048 map's ranges a weaker shadow per kilometre. Recommendation: keep metres; P1-04 uses per-size thresholds, one size-keyed scale on its rule thresholds in mm (the critic's recommendation, recorded in P1-04 11.6), not per-world quantiles, which would remove the variety between worlds. Revisit the moisture path only if P1-05's absolute scores make every 2048 world look poor.
7. **A per-world temperature offset** (taste, for Paul; world critic S4). Temperature is the same function of height and row in every world (limitation 8), so every world has its treeline near 1,000 m. A per-world sea-level offset (for example a uniform draw in [−2, +2] °C from a new stream `geo.climate.offset`, added to t_sea) would make some worlds moor-heavy and some mild, more variety than any change to the wind distribution. It is a design question for Paul (proposals "Second-pass geography"); if adopted it is a new tuning field, a new stream and, if stored, a column of the table of question 4. No change now.

**Schema and catalogue impact:** none. The `worldgen` kind gains the id `climate` (P1-00 6.1), which changes `content_hash` of every world (the 0.7.0 bump of the climate merge, P1-00 6.4). Three stream names are registered.
