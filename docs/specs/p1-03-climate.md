# P1-03 Climate

Status: draft, spec writer, 2026-11-16, awaiting approval. Phase P1 Geography, stage 3 (`climate`). Design doc section 4 (stage 3: "temperature from latitude and elevation; rainfall from prevailing wind and rain shadow"; "scale is compressed"). Depends on P0-01 to P0-04, P1-00 (shared algorithms G1 to G8, rules D1 to D10, section 2.2 map-size convention), P1-01 (the `elevation` layer) and P1-02 (`water`, `water_depth`, `coast_dist`). Tuning values marked "guess" and every statistics range are starting points, calibrated only against a floating-point prototype (section 11), to be revised against the first implementation sweep.

## 1. Purpose

Give every cell an annual mean temperature and an annual precipitation. Biomes (P1-04) classify on them; resources (P1-05) score timber and fresh water with rainfall; history reads them through biomes and habitability; the runtime will use them for vegetation and weather colour.

The map is small (8 to 33 km) and its terrain is one continent with steep ranges (P1-01: peaks 1,200 to 2,600 m, ranges as walls, most land low). So climate is a local gradient with a strong orographic effect, not a stack of planetary belts:

- **Temperature** falls from south to north by a fixed amount per map (the latitude span is a map fraction, P1-00 2.2, proposals "Climate's latitude span is a fraction of the map"), and falls with height at a lapse rate. The ranges are what make cold places.
- **Rainfall** comes from one prevailing wind per world. Saturated air comes off the sea, rains more where it is forced up a slope, and loses moisture as it rains, so the far side of a range is dry (rain shadow) and inland air is drier than coastal air. A small bonus near every coast stands for sea breezes and fog.

Every distance in this stage is in metres (P1-00 2.2): a 2048 map has a wider continent and so a drier interior, with the same rain shadow behind each range.

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

**Surface.** S[i] = `elevation`[i] + `water_depth`[i], an `int64` in decimetres: 0 on ocean (P1-02 invariant 7: depth = −e), the lake level on lake cells, e on dry land and river cells. The air sees the water surface, not the lake bed.

Preconditions, checked first (fatal "climate: input precondition: …"): the five layers exist, are N × N with the catalogue types; S = 0 on every ocean cell (`water` = 3) and S ≥ 1 elsewhere.

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
| `temp_north_c` | fx(−30, 40) | 5 | guess | sea-level annual mean at the north edge (v = 0) |
| `temp_south_c` | fx(−30, 40) | 9 | guess | sea-level annual mean at the south edge (v = 1) |
| `lapse_c_per_km` | fx(0, 15) | "6.5" | designed (standard atmosphere) | cooling per km of height |
| `temp_noise` | `fbm_m` | wavelength 6000, octaves 3, gain "0.5" | guess | local temperature variation |
| `temp_noise_amp_c` | fx(0, 5) | 1 | guess | its weight, °C per unit of noise |
| `wind_weights` | array(int(0, 1000), 8, 8) | [1, 1, 1, 1, 1, 1, 1, 1] | guess (open question 1) | relative chance that the prevailing wind blows **toward** D8 code 0 (E) to 7 (NE) |
| `smooth_radius_m` | int(0, 4000) | 320 | guess | half-width of the square window over which the air sees the terrain (C4) |
| `rain_base_mm` | int(0, 5000) | 800 | guess | precipitation from saturated air over level ground |
| `rain_oro_mm` | int(0, 100000) | 5000 | guess | extra precipitation per unit of upslope gradient along the wind (rise per metre of path) |
| `moisture_path_mm_km` | int(100, 2000000) | 16000 | guess | rain integrated along the path (mm × km) that would use up saturated air; sets how fast air dries |
| `loss_max` | fx(0, "0.9") | "0.25" | designed | cap on the share of the air's moisture lost in one step |
| `ocean_recharge_m` | int(16, 100000) | 500 | guess | open water over which air takes up moisture again (per step it regains step/recharge of its deficit) |
| `coast_rain_mm` | int(0, 5000) | 150 | guess | rain added at the coast, every direction (sea breeze, fog) |
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
rain_oro_mm = 5000
moisture_path_mm_km = 16000
loss_max = "0.25"
ocean_recharge_m = 500
coast_rain_mm = 150
coast_rain_reach_m = 1500
rain_noise = { wavelength_m = 5000, octaves = 3, gain = "0.5" }
rain_noise_amp = "0.4"
rain_min_mm = 250
```

Every fixed-id directory gets this file (P1-00 6.1: every id must be defined where the kind's directory exists), so adding `climate` to the `worldgen` id set changes `content_hash` of every world: a PATCH bump and re-recorded golden hashes in the implementing branch (P1-00 6.4; proposals "Every world's hash depends on the shipped tuning files").

Fixed constants (not tuning): the 3-tap upwind weights (1, 2, 1)/4; the step lengths 16 m and 16√2 m (`SQRT2`, P1-00 amendment A1); off-map air is saturated (q = ONE) at sea level.

## 5. Algorithm

Notation as P1-01 5.0: every value is `Fx` unless stated, every operation the P0-01 operation of that name with its rounding, statements evaluated in the order written. X = `from_int(16x + 8)`, Y = `from_int(16y + 8)` (metres); V = `from_ratio(2y + 1, 2N)` (P1-00 2.1). On a W × H test grid, N in V is replaced by H and lines run over W or H as stated. Steps C1 to C7 in order.

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

### C4. Smoothed surface (D2, two separable passes, G6 row blocks)

r = `smooth_radius_m` div 16 (cells; 20 with the default). For each cell, the window is the square [x − r, x + r] × [y − r, y + r] clipped to the map, with c_w cells (a rectangle):

```
sum  = Σ S over the window           // int64, exact: ≤ 4097² × 30000 < 2^39
hs   = from_ratio(sum, c_w)          // Fx, decimetres
```

Computed as a row pass (sums along x) followed by a column pass over the row sums; both are exact integer sums, so the window sum, and hence hs, does not depend on how the passes are split. r = 0 gives hs = `from_int(S)`.

### C5. Moisture sweep (D4 over lines)

Moisture q ∈ [0, ONE] is the air's share of saturation; p is precipitation in mm/yr (`Fx`).

**Lines.** If dx ≠ 0 the lines are columns, processed in the order x = 0, 1, …, N − 1 when dx = 1 and x = N − 1, …, 0 when dx = −1. If dx = 0 the lines are rows, y ascending when dy = 1, descending when dy = −1. Each cell reads only values of the previous line, so the cells of one line may be computed in any order or in parallel; the lines themselves are sequential.

**Upwind taps** of cell (x, y): three cells of the previous line,

```
dx ≠ 0:  (x − dx, y − dy − 1), (x − dx, y − dy), (x − dx, y − dy + 1)      // a, b, c
dx = 0:  (x − 1, y − dy),      (x,      y − dy), (x + 1, y − dy)
```

A tap outside the map has q = ONE and hs = 0 (saturated air at sea level: the margin is ocean). With taps a, b, c in that order:

```
q_up  = div_int((q_a + mul_int(q_b, 2)) + q_c, 4)
hs_up = div_int((hs_a + mul_int(hs_b, 2)) + hs_c, 4)
```

**Step.** Diagonal winds (dx ≠ 0 and dy ≠ 0) step 16√2 m, the others 16 m:

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

Computed by the spec writer with an exact-integer Python reference of this section (an implementation of the P0-01 operations checked against P0-01's own vectors: `stream_key(0, "geo.elevation.erosion")` = 0x7d5f47a9629481b8, `Rng(0)` first output 0x99ec5f36cb75f2b4, `parse_decimal("0.4")` raw 1717986918); the sweep cells marked "by hand" were also checked by hand. Noise amplitudes are 0 unless stated, shipped values otherwise.

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

**C4 box mean**, r = 1, a 4 × 3 grid S (rows y = 0 to 2): `0 10 20 30`, `0 40 50 60`, `0 0 70 80`. hs in dm: row 0 `12.5 20 35 40`; row 1 `8.3333 21.1111 40 51.6667` (raw 35791394133, 90671531804, 171798691840, 221906643627); row 2 `10 26.6667 50 65` (26.6667 is raw 114532461227). For example (0, 1): window x 0..1, y 0..2, sum 50 over 6 cells.

**C5 sweep, wind E (k_w = 0)**, r = 0 (hs = S), a 6 × 3 grid, columns 0 and 5 ocean (S = 0), S rows: `0 20 60 120 80 0`, `0 30 90 160 100 0`, `0 10 40 80 60 0`. Shipped `rain_base_mm` 800, `rain_oro_mm` 5000, `moisture_path_mm_km` 16000, `loss_max` 0.25, `ocean_recharge_m` 500 (rec = 0.032, raw 137438953).

- (1, 0), by hand: the upwind column is outside the map, so q_up = ONE, hs_up = 0; rise 20 dm, g = 20/160 = 0.125 (raw 536870912); p = 800 + 5000 × 0.125 = 1425 mm; lam = 1425 × 16 / 16000000 = 0.001425 (raw 6120328); q = 4294967296 − 6120328 = 4288846968.
- (2, 1), by hand: taps (1, 0), (1, 1), (1, 2) with q 4288846968, 4287504790, 4290189145: q_up = RHA(17154045693 / 4) = 4288511423; hs_up = (20 + 60 + 10)/4 = 22.5 dm; rise 67.5, g = 0.421875 (raw 1811939328); p raw 12476887921291 (2905.002 mm).

q (raw), rows y = 0 to 2:

```
4294967296 4288846968 4280922232 4270788875 4271670939 4276491274
4294967296 4287504790 4276053289 4263755588 4265212810 4269935559
4294967296 4290189145 4283599746 4276126894 4274335696 4277781016
```

p (mm, three decimals of the raw value):

```
800.000 1425.000 2125.684 2901.415 796.295 796.445
800.000 1737.500 2905.002 3599.202 795.090 795.183
800.000 1112.500 1657.731 1967.095 796.792 796.693
```

Column 4 descends (rise 0), so it gets only base rain from the depleted air; column 5 is ocean, whose own rain uses q_up and which starts recharging.

**Diagonal step, wind SE (k_w = 1)**, same grid: (1, 0) has taps (0, −2), (0, −1), (0, 0), the first two outside the map: q_up = ONE, hs_up = 0, g = div(20 dm, mul_int(SQRT2, 160)) = raw 379625062, p raw 5334099146800 (1241.942 mm), lam raw 7543555, q 4287423741. (3, 0) has taps (2, −2), (2, −1), (2, 0): q_up 4291438217, hs_up 15 dm, p 3117.630 mm.

**C6 rainfall** (shipped values; rn given):

| p (mm) | `coast_dist` | rn | cb (mm) | `rainfall` |
| --- | --- | --- | --- | --- |
| 800 | 16 | 0 | 148.4 | 948 |
| 800 | 1500 | 0 | 0 | 800 |
| 100 | 3000 | 0 | 0 | 250 (floor) |
| 12000 | 50 | 0 | 145 | 10000 (cap) |
| 800 | 700 | 0.5 | 80 | 1040 |
| 800 | −40 (ocean) | −0.3 | 150 | 854 |

For the wind-E sweep above, with `coast_dist` 16 on the land columns and −16 on the ocean columns, `rainfall` rows: `950 1573 2274 3050 945 946`, `950 1886 3053 3748 943 945`, `950 1261 1806 2115 945 947`.

## 6. Invariants

For every world and every tuning inside the bounds:

1. `temperature` ∈ [−600, 500] and `rainfall` ∈ [`rain_min_mm`, 10000] on every cell.
2. **Lapse.** With `temp_noise_amp_c` = 0: on cells of one row, S(a) ≥ S(b) implies `temperature`(a) ≤ `temperature`(b); on cells with the same S, `temperature` is non-decreasing from north to south when `temp_south_c` ≥ `temp_north_c`.
3. **Moisture stays a share.** Every q of C5 is in [0, ONE]: a 1-2-1 mean of values in [0, ONE] is in [0, ONE]; the ocean update adds at most ONE − q_up; on other cells lam ≤ `loss_max` < 1.
4. **Rain shadow.** On every non-ocean cell of C5, q ≤ q_up (air only dries over land), and where rise = 0, p = mul(q_up, `rain_base_mm`) (a lee slope gets only base rain from the air that reaches it).
5. **No wind, no shadow.** With `rain_oro_mm` = 0 and both noise amplitudes 0, rainfall on land depends only on the moisture path, not on the terrain.
6. Pure function of the seed, N, the five input layers and `worldgen.climate`; independent of thread count (D10).

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| C2 | D9, sequential | one `Rng`, one `weighted_index` call; no other `Rng` in the stage |
| C3, C6 | D1, parallel by G6 row blocks | location-keyed noise only |
| C4 | D2, parallel by G6 row blocks | exact integer sums; any split gives the same sums |
| C5 | D4 over lines | lines in the stated order; within a line D1 (reads only the previous line) |

No floating point, no new math function: `fbm_m` (P1-01 4.1), `mul`, `div`, `div_int`, `mul_int`, `from_ratio`, `round_int`, `clamp`, `SQRT2`.

Overflow (N = 2048, any tuning inside the bounds):

- C3: |t_sea| ≤ 40; lap ≤ 15 × 30000 / 10000 = 45 (`mul_int` raw ≤ 15 × 30000 × 2^32 < 2^51); |tn| ≤ 5; T ∈ [−90, 85] before the clamp; × 10 fits.
- C4: sums < 2^39; hs ≤ 30000.
- C5: rise ≤ 30000 dm, step_dm ≥ 160, so g ≤ 187.5; `mul_int(g, rain_oro_mm)` ≤ 1.875 × 10^7; p ≤ 1.876 × 10^7 mm (q ≤ 1); mul(p, step_m) ≤ 4.25 × 10^8 (< 2^31); 1000 × `moisture_path_mm_km` ≤ 2 × 10^9 fits `int64` as a `div_int` divisor; rec ≤ ONE.
- C6: f ≤ 2; pr ≤ 3.76 × 10^7 < 2^31; `coast_rain_mm` × reach ≤ 5000 × 32767 fits; round_int then clamp.

## 8. Performance

Budget (P1-00 11): 0.3 / 1.5 / 6 s at 512 / 1024 / 2048. Cost per cell: 6 `noise2` octaves (C3, C6; parallel), a constant-time box filter (C4; parallel), and about 20 integer operations in the single-threaded sweep (C5: about 0.05 s at 1024 by estimate). Memory: S, hs, q (one line suffices), p as `int64` grids, about 32 bytes per cell (130 MB at 2048), within P1-00's 2 GB.

## 9. Report and render

**`report`** (P1-00 10.3), appended to `[geography]` when `temperature` and `rainfall` are present, in this order ("land" is `elevation` > 0, as in 10.3):

| Key | Value |
| --- | --- |
| `temperature_c` | land percentiles 0, 10, 50, 90, 100 of `temperature`, each printed as tenths with one decimal (the `metres1` formatter applied to tenths of a degree, e.g. `-14.5`), separated by spaces |
| `rainfall_mm` | land percentiles 0, 10, 50, 90, 100 of `rainfall`, integers |
| `land_below_0c` | ratio4(land cells with `temperature` < 0, land cells) |
| `rain_contrast` | ratio4(p90, p10) of land `rainfall` (the two values above); `n/a` if p10 = 0 |

**Statistics** (for P1-00 13.1; all **provisional**: set from the floating-point prototype of section 11 on 20 worlds, to be replaced by the validation specialist's first implementation sweep with the 13.1 seed set, taking the measured min–max widened by about 10% as typical and the hard bound only where a value outside would mean a defect):

| Statistic | All seeds (hard) | Typical (≥ 90%) | Prototype |
| --- | --- | --- | --- |
| `temperature_c` p50 | within the catalogue range (by construction) | 3.0–9.0 °C | 4.6–6.2 °C |
| `rainfall_mm` p50 | ≥ `rain_min_mm` (by construction) | 600–1200 mm | 670–950 mm |
| `land_below_0c` | descriptive | — | not measured |
| `rain_contrast` | descriptive | 1.5–10 | p90/p10 ≈ 2.5–7 |

The real acceptance test of climate is the biome mix (P1-04 9).

**Palettes** (P1-00 10.2 interpolation rule; registered under `auto`):

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

Library seams (interim names; the engineer may add helpers but should not change these without a spec note): `climate_wind(seed, weights) → code`; `climate_temperature_cell(...)`; `climate_box_mean(S grid, r)`; `climate_sweep(S grid, water grid, hs grid, wind, tuning) → (q grid, p grid)`; `climate_rainfall_cell(p, coast_dist, rn, tuning)`; `climate_layers(elevation, water, water_depth, coast_dist, tuning, seed, threads)`; all on any W × H grid.

### 10.1 Unit tests

1. Every worked example of 5.1 exactly (raw values where given).
2. The C3 noise term: for seed 0, N = 64 and the shipped tuning, at 16 cells chosen by the test, `temperature` equals the test's own evaluation of C3 with `fbm2`.
3. Box mean at r larger than the grid (every window is the whole grid): every hs is the grid mean.
4. Sweep with each of the 8 wind codes on a 5 × 5 grid of S = 10 on land with an ocean border: the result is the 90°-rotated or mirrored image of the code 0 and code 1 results (the test rotates the grid, sweeps, rotates back and compares exactly).
5. Wind: weights with a single non-zero entry always give that code; a weights array summing to 0 is a load error (named check); 7 or 9 entries are a type error.
6. Tuning validation: each bound of section 4 rejects a value one step outside it; a float literal is a P0-03 error.

### 10.2 Property tests

Over seeds 0 to 63 at N = 128 (fixture tuning; elevation and hydrology layers from the P1-01 and P1-02 stages) and 200 random fixtures (S random in [1, 3000] on a 32 × 32 interior with an ocean border, `water` 0 on land, random wind code; stream `test.climate`, test-only): invariants 1 to 5 by code independent of the stage.

### 10.3 Determinism tests

Threads 1, 2 and 5 give identical layers (seeds 0 and 42 at N = 128 and 512); resume (P1-00 X2): climate run on layers read back from a world written after hydrology equals the uninterrupted pipeline.

### 10.4 Golden values

Layer hashes of `temperature` and `rainfall` with fixture tuning for seeds 0, 42 and "The Unmapped" at N = 512, seed 1 at 1024, seed 0 at 128, recorded by the validation specialist after the implementation passes. Their inputs come from the hydrology golden cases, so an upstream change re-records them together.

## 11. Known limitations and calibration

1. **Calibration is from a floating-point prototype**, not this arithmetic: the spec writer's Python model of C3 to C6 with substitute noise (smoothed Gaussian fields of similar wavelength) and an approximate hydrology (G3/G4 without lake rerouting), run on generator 0.3.0 worlds (512 seeds 0–11, 1024 seeds 0–5, 12, 18) with wind code = seed mod 8. Values marked "guess" were chosen there so that the five natural biomes of P1-04 all appear (section P1-04 11); expect to retune after the first implementation sweep.
2. **One wind, no seasons.** The model has no seasonal cycle, no secondary wind and no temperature effect of the sea (continentality). On a 16 km map these would be small next to the orographic effect.
3. **Streaks along the wind.** Advection along one D8 direction leaves faint stripes parallel to the wind in the rain field downwind of sharp terrain; the 320 m smoothing, the 1-2-1 taps and the rain noise hide most of them. Diagonal winds step along exact diagonals.
4. **Rain on steep windward faces is high** (prototype land p90 1300–3500 mm, maximum at the 10000 mm cap on a few cells): the ranges are walls (P1-01), and even smoothed over 320 m their windward gradient is large. It matters only where it moves a biome boundary; lower `rain_oro_mm` if the critic finds the windward flanks too wet.
5. **Rivers do not follow the rain.** `flow_accum` counts cells (P1-02 open question 2); this spec adds no rainfall-weighted discharge (open question 3).

## 12. Open questions for the lead

1. **Wind direction per world** (a matter of taste for Paul). Chosen: drawn uniformly from the 8 D8 directions, so the wet side differs between worlds. Alternatives: always a westerly (weights `[1, 0, 0, 0, 0, 0, 0, 0]`), or westerlies favoured (e.g. `[4, 2, 1, 1, 1, 1, 1, 2]`). Both are a data change only. Recommendation: keep uniform for variety; the north–south temperature gradient is fixed, so the wind is the main thing that makes two continents' climates differ.
2. **Temperature span and level** (taste). Chosen: 5 °C at sea level in the north to 9 °C in the south, 6.5 °C per km lapse: a cool temperate land in keeping with dark high fantasy, alpine conditions above about 1,000 m. The span is a map fraction (proposals "Climate's latitude span is a fraction of the map"), the same at every map size. Recommendation: approve; revisit with the biome images.
3. **Rainfall-weighted discharge** (P1-02 open question 2). Recommendation: none in P1. Rivers are sized by area; biomes and resources use `rainfall` and the river raster directly; a rain-shadow river being as large as a wet-side river is acceptable (and real: exotic rivers). If history or the runtime needs discharge, a later stage can compute it from `flow_dir` and `rainfall` without changing any existing layer.
4. **Store the wind direction?** It is not in the world file, so `report` cannot print it and the viewer cannot draw it. Recommendation: no change now; if the critic or the viewer needs it, add a one-row table (`climate`: `wind_toward`) with the next schema change rather than an extra `meta` key.
5. **Lakes do not recharge the air.** Large lakes would moisten their lee shore. Recommendation: leave it; the lake share is capped at 3% of land (P1-01).

**Schema and catalogue impact:** none. The `worldgen` kind gains the id `climate` (P1-00 6.1), which changes `content_hash` of every world (PATCH bump, P1-00 6.4). Three stream names are registered.
