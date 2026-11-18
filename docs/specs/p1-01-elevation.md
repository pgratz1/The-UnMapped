# P1-01 Elevation

Status: **fourth version (revision "natural passes and final talus", `GENERATOR_VERSION` 0.6.0), approved by the lead, 2026-11-17** (the cap value of 500 m was confirmed by Paul on 2026-11-17, 4.2). Spec writer, 2026-11-16. Phase P1 Geography, stage 1 (`elevation`). Design doc section 4 (stage 1: "layered noise shaped by a continent mask, then hydraulic erosion"). Depends on P0-01 to P0-04 and P1-00, whose sections are cited as "P1-00 n" and whose shared algorithms G1 to G6 and determinism rules D1 to D10 are used by name.

The third version specified design candidate **v16** of the engineer's exploration exactly (branch `p1/elevation-explore`, commit `ef1f734`); it is what `GENERATOR_VERSION` 0.5.0 implements. This fourth version adds two things to it, and changes nothing else:

- **Natural passes** (new step E6.5 and a changed E7): in each mountain range, cols are placed at true saddles of the range's drainage divide, lowest natural saddle first, and the range's envelope is lowered along a crossing that follows the valleys draining from the col to each side, before erosion. The cut at the crossing's highest point is capped (`pass_max_cut_m`, 500 m), so a naturally low saddle becomes a low gate and a high one a high col.
- **Final talus** (new step E8.7): 100 further talus steps at 5:1 after the erosion loop, which bound very steep faces (third version's limitation 11).

It specifies **round five** of the passes exploration exactly: branch `p1/01b-elevation-passes-explore`, recorded commit `0991d74` (`3717dce` only adds an unused option), functions `elevation_pass_layout` and `divide_layout`, the E7 change and `final_talus` in `worldgen/src/stages/elevation.cpp`, with the field values of `~/.cache/the-unmapped/out/p1-01b-passes/tuning/r5c500-variant-fields.toml` (cap 500 m), taking only the path those values select. None of the exploration's switches exists (section 4.2). The design decisions are Paul's and the lead's (`docs/design/proposals.md`: "High peaks may be inaccessible; history opens the passes", "Every world has real uplands", "Natural passes through the ranges", decided 2026-11-17), the world critic's reviews of rounds two and four (`critic-review.md`, `critic-review-r4.md` in the exploration's output directory) and the engineer's round-four and round-five definitions (`passes-definition.md` sections 0 to 11). Where that definition and the code disagree, this text follows the code; the disagreements are listed in section 12.

Land/ocean outlines are unchanged from 0.5.0 by construction (E1 to E4 are untouched, section 6.5). Everything after E5 can change, and with it every hydrology layer; a world in which no pass is placed and no final-talus step changes a cell keeps its 0.5.0 layer (1024 seed 3 is one). The 0.5.0 reference hashes of the third version no longer hold; section 10.4 gives the new ones.

The statements of section 5 up to E6 and from E8.1 to E11 are the third version's, checked then by an independent exact-integer reference (`worldgen/tests/reference/p1_elevation_reference.py`). The new steps were checked by the spec writer's exact-integer extension of that reference, written from this text, against the exploration build (section 10.4).

## 1. Purpose

Produce the `elevation` layer: one continent per world that reads as a continent. A coastline with bays and peninsulas; long mountain ranges laid out along curving belts, with peaks up to about 2,500 m whose high ground is mostly steeper than a walker can climb (ranges are barriers); natural passes at the lowest saddles of most long ranges, some of them walkable, most of them high cols that history may improve; broad lowlands; lakes in basins, including tarns among the peaks; and drainage to the sea everywhere else. Every world has real uplands: where a world's belts carry less than 8% of its land in the mountain core, the ranges are lengthened along their belt, or the belt is moved to an inland level set, until they do, as far as the tried offsets allow (E5.1).

The stage builds a continent field from warped map-scale noise, a radial and square edge falloff and metre-scale coast noise; sets the coast so that a target share of the map is land; lays out mountain belts along a level set of a second map-scale noise, interrupted by a segmentation noise, and moves or lengthens them if a world has too little mountain core (E5.1); builds heights in metres (E5, E6); finds the ranges, their drainage divides and their saddles, and lowers the mountain envelope along a crossing at each chosen col (E6.5, E7); deepens the sea with distance from land; carves the land with stream-power erosion along randomized steepest-descent receivers, hillslope diffusion and a talus step, then bounds the steepest faces with a final talus (E8); quantizes to decimetres; and conditions depressions so that every remaining one is a lake and everything else drains to the sea (E10). Its output is the final terrain: no later stage changes it (P1-02 section 9).

## 2. Inputs

| Input | Source |
| --- | --- |
| World seed (`uint64`) | `meta.seed` |
| Map side N | `param.map_size` (512, 1024, 2048); library and tests: any 16 ≤ N ≤ 2048 (see below for small N) |
| Tuning | definition `worldgen.elevation` (section 4) |
| Earlier layers | none |

**Small N.** The algorithm is defined for every 16 ≤ N ≤ 2048, but whether a world exists depends on the tuning: when no 8-connected land component after E4 reaches I_min cells, the defined result is the fatal error "elevation: no land" (E4 step 3). With the shipped values (I_min = 196) this happens for every seed at N ≤ 21 (seeds 0–255 and 0–63 checked), for most seeds at N = 22 (208 of 256), for one of 256 at N = 23, and for none at N = 24 to 26 (seeds 0–255) or N = 28 to 64 (seeds 0–63), measured on the exploration build. Library tests therefore use N ≥ 64 with the shipped values; a test of the fatal error may use N = 16.

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
| K_pass | `geo.elevation.pass` | E6.5.5 (`hash_at(K_pass, cell, slot)`), E6.5.6 (`hash_at(K_pass, col, 16)` and `(…, 17)`), E6.5.7 (`noise2(substream(K_pass, 3), …)`) |

All sixteen are registered in `streams.hpp` (P0-01 7.3); `geo.elevation.pass` is new in this version and is appended to `all_stream_names` after `geo.hydrology.flat` (registration order changes no world). Changing any of them changes every world. The exploration branch's `substream(K_pass, 2)` (round three's bend noise) is not used.

## 3. Outputs

One layer, `elevation`: `i16`, N × N, codec 1, decimetres above sea level (P1-00 4). Land values in [1, 29990], ocean values in [−29005, −1] for any tuning inside the bounds of section 4 ([−3005, −1] with the shipped `ocean_depth_m`), never 0. Negative cells are ocean, positive cells land (including lake beds). Postconditions in section 6.

**Table `pass`** (adopted by the lead): one row per pass site (section 9.1). This is a schema change: `SCHEMA_VERSION` 1 → 2 with generator 0.6.0 (proposals "Pass sites are stored in the world file"). The layer does not depend on it.

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
| `edge_radial_start` | fx(0, 1.99) | "0.8" | guess | radial distance (1 = an edge midpoint) where the radial falloff starts |
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
| `pass_count_max` | int(0, 3) | 3 | guess | at most this many pass sites per range (E6.5.2); 0: no passes |
| `pass_core_mask` | fx(0.01, 1) | "0.25" | guess | crest cells: mask at least this (E6.5.2) |
| `pass_crest_frac` | fx(0, 1) | "0.3" | guess | crest cells: \|nb − o*\| at most this × w_b |
| `pass_min_length_m` | int(0, 100000) | 3000 | guess | a range shorter than this (crest geodesic length, scaled by map size) gets no pass |
| `pass_spacing_m` | int(500, 100000) | 3000 | guess | one more pass site per this much crest length beyond the minimum |
| `pass_size_ref_n` | int(16, 2048) | 1024 | designed | map side at which `pass_coast_clear_m`, `pass_min_length_m` and `pass_side_max_m` hold as stated; below it they shrink with N |
| `pass_region_mask` | fx(0.005, 1) | "0.03" | guess | support (domain of the divide flood): land with mask at least this |
| `pass_body_m` | fx(0, 2900) | 300 | guess | body: support cells whose mountain part is at least this |
| `pass_basin_depth_m` | fx(1, 1000) | 40 | guess | a pre-erosion depression at least this deep is a tarn basin cell |
| `pass_tarn_min_cells` | int(0, 1000000) | 300 | guess | a basin component this large on a crossing's body rejects it |
| `pass_divide_reach_m` | fx(0, 20000) | 1000 | guess | a divide cell farther than this from every crest cell belongs to no range |
| `pass_saddle_window_m` | fx(0, 20000) | 400 | guess | radius of the local-minimum test of a candidate col |
| `pass_saddle_rise_m` | fx(0, 2900) | 150 | guess | the divide must rise at least this on both sides of a candidate col … |
| `pass_saddle_rise_reach_m` | fx(0, 20000) | 1500 | guess | … within this radius; also the radius of the crest-axis direction |
| `pass_min_spacing_m` | fx(0, 100000) | 2500 | guess | least straight-line distance between two cols of the world |
| `pass_coast_clear_m` | fx(0, 20000) | 700 | guess | least chamfer distance from a col to the ocean (scaled by map size) |
| `pass_barrier_m` | fx(0, 2900) | 600 | guess | least envelope (mask × scale) at a col; also the least shoulder height |
| `pass_side_max_m` | fx(0, 100000) | 3000 | guess | longest crossing of the body on either side (scaled by map size) |
| `pass_col_tarn_m` | fx(0, 5000) | 160 | guess | no basin cell within this radius of a lowered col |
| `pass_grade` | fx(0, 2) | "0.9" | guess | saddle target per metre of the shorter reach, before the factor 8/15 |
| `pass_grade_jitter` | fx(0, 1) | "0.3" | guess | relative jitter of `pass_grade` per site |
| `pass_min_col_m` | fx(0, 2900) | 150 | guess | lower clamp of the saddle target; least col height above the higher foot |
| `pass_saddle_max_m` | fx(0, 2900) | 600 | guess | upper clamp of the saddle target |
| `pass_max_cut_m` | fx(1, 2900) | 500 | decided (Paul, 2026-11-17, after comparing 400, 500 and 600) | the crossing's highest point is lowered by at most this (the cut cap) |
| `pass_peak_rise_m` | fx(0, 2900) | 150 | guess | shoulders: the divide on both sides of the col rises at least this above the saddle target |
| `pass_halfwidth_m` | fx(0, 5000) | 450 | guess | half-width of the lowered band |
| `pass_width_jitter` | fx(0, 0.9) | "0.3" | guess | relative jitter of the half-width per site |
| `pass_width_var` | fx(0, 0.9) | "0.35" | guess | relative variation of the half-width along the crossing (noise) |
| `pass_walk_grade` | fx(0.01, 4) | "0.55" | guess | uncapped sites: the lowered crossing rises at most this per metre over 3 cells |
| `pass_texture_keep` | fx(0, 1) | "0.25" | designed | the relief above the foothill base keeps at least this share inside a pass |
| `pass_drain_step_m` | fx(0.01, 10) | "0.1" | designed | floor drainage: the least fall per crossing cell (E6.5.8) |
| `final_talus_iterations` | int(0, 1000) | 100 | designed (lead) | E8.7 steps; 0: none |
| `final_talus_slope` | fx(0.05, 16) | 5 | designed (lead) | E8.7 talus tangent (5 is 78.7°; 80 m per orthogonal step) |
| `final_talus_rate` | fx(0, 0.125) | "0.0625" | designed (lead) | E8.7 share of the excess moved per step and neighbour |

The 34 new fields follow the third version's fields in this order. Every one is required. Their names are the exploration's. Where the exploration's bound allowed 0 as a "switch off" value whose meaning was not the formula's value at 0, the lower bound is raised so that the formula is the only meaning: `pass_size_ref_n` (0 meant "no size scaling"), `pass_basin_depth_m` (0 meant "no basins"), `pass_max_cut_m` (0 meant "no cap"), `pass_walk_grade` (0 meant "no walk-grade clamp") and `pass_drain_step_m` (0 meant "no floor drainage"). `pass_count_max` = 0 and `final_talus_iterations` = 0 keep their meaning, which is also the formula's (no range gets a site; no step runs).

Dropped exploration fields and switches (their round-five values are the algorithm): `pass_r5`, `pass_divide`, `pass_across`, `pass_floor_mode`, `pass_keep_pits`, `pass_cut_rel`, `pass_high_col_frac`, `pass_mode`, `pass_depth`, `pass_depth_jitter`, `pass_wobble_m`, `pass_col_window_m`, `pass_cost_m`, `pass_monotone`, `pass_wander_m`, `pass_attempts`, `pass_trace_max_m`, `pass_bend_m`, `pass_cross_max_m`, `pass_body_mask`, `pass_end_margin`, `pass_wall_m`, `pass_open_end_m`, `coast_taper_m`. (The round-five data file sets `pass_attempts`, `pass_col_window_m`, `pass_trace_max_m`, `pass_bend_m` and `pass_cross_max_m`; the round-five path reads none of them.) Constants of the code, not fields: the 20th chain cell of the crossing line (E6.5.4), the 600 m (37-cell) divide-direction radius, the 45° across limit, the chord limits 1.5 and 35° (E6.5.6), the shoulder ring of 9 to 187 cells, the 300 m end taper, the width-noise frequency 1/1500 per metre (E6.5.7), and the `hash_at` second coordinates 16 and 17.

No field of the second version's stable table is removed. Added: `segment_on_share`, `segment_floor_mask`, `belt_offset_steps`, `floor_inland_gm`, `ocean_shelf_m`, `talus_slope`, `talus_rate`, `lake_area_ref_n`, `lake_max_share`, `fill_noise`, `fill_cost_amp`, `fill_detail_m`. Dropped from the second version's placeholder table: `breach_max_depth_m`, `fill_relief`, `condition_passes`. None of the exploration branch's switches (`segment_floor`, `fill_mode`, `fill_depressed_only`, `lake_order_volume`, `fill_flats`, `erosion_random`, `height_remap`, `belt_height_scaling`, and the others listed in `v16-definition.md` section 11) exists: their v16 values are the only algorithm.

### 4.3 Named checks (P0-03 7.5)

Required loader validation: each check is evaluated when the definition is loaded, and a failure is a configuration error (a P0-03 load error at the definition's header), so a file that fails one is rejected and no world is generated.

1. `edge_radial_span`: `edge_radial_end − edge_radial_start ≥ 0.01`. Keeps the E2 divisor positive and bounded away from 0 (section 7). (With `edge_radial_end` ≤ 2 this needs `edge_radial_start` ≤ 1.99, hence that field's upper bound.)
2. `mountain_ranks`: `mountain_cover_rank ≤ mountain_peak_rank`.
3. `lake_areas`: `lake_min_area_m2 ≤ lake_max_area_m2`.
4. `pass_masks` (new): `pass_region_mask ≤ pass_core_mask`. Makes every crest cell a support cell, so the range search of E6.5.3 starts inside its own domain.
5. `pass_saddle_clamp` (new): `pass_min_col_m ≤ pass_saddle_max_m`. Keeps the clamp of E6.5.6 (h) well formed.

Error shape (P0-03 9 and the loader's existing check errors): one error per failing check, at the position of the definition's header (`[worldgen.elevation]`), in the form `<path>:<line>:<column>: error: check '<name>' failed for 'worldgen.elevation': <message>`, with the messages `edge_radial_end - edge_radial_start must be at least 0.01`, `mountain_cover_rank must not exceed mountain_peak_rank`, `lake_min_area_m2 must not exceed lake_max_area_m2`, `pass_region_mask must not exceed pass_core_mask` and `pass_min_col_m must not exceed pass_saddle_max_m`. `<path>` is the file relative to the data root (for example `worldgen/elevation.toml`). Checks run only when the definition passed type validation; several failing checks give several errors, sorted as P0-03 9 sorts all errors. Tests assert path, line, column and the full text.

(The exploration code checks none of them; its values satisfy all five, so adding the checks changes no world.)

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
pass_count_max = 3
pass_core_mask = "0.25"
pass_crest_frac = "0.3"
pass_min_length_m = 3000
pass_spacing_m = 3000
pass_size_ref_n = 1024
pass_region_mask = "0.03"
pass_body_m = 300
pass_basin_depth_m = 40
pass_tarn_min_cells = 300
pass_divide_reach_m = 1000
pass_saddle_window_m = 400
pass_saddle_rise_m = 150
pass_saddle_rise_reach_m = 1500
pass_min_spacing_m = 2500
pass_coast_clear_m = 700
pass_barrier_m = 600
pass_side_max_m = 3000
pass_col_tarn_m = 160
pass_grade = "0.9"
pass_grade_jitter = "0.3"
pass_min_col_m = 150
pass_saddle_max_m = 600
pass_max_cut_m = 500
pass_peak_rise_m = 150
pass_halfwidth_m = 450
pass_width_jitter = "0.3"
pass_width_var = "0.35"
pass_walk_grade = "0.55"
pass_texture_keep = "0.25"
pass_drain_step_m = "0.1"
final_talus_iterations = 100
final_talus_slope = 5
final_talus_rate = "0.0625"
```

These are the values of `r5c500-variant-fields.toml` on top of the third version's file, with `pass_region_mask` and `pass_core_mask` at the exploration's defaults (the variant file does not set them).

**Small-map fixture** `worldgen/tests/data/geo-small/worldgen/elevation.toml`: the file above with exactly these fourteen lines changed. The first five make library tests at N = 64 to 256 reach the lake test, the share cap and the mountain floor (with the shipped values, lake_max_cells < lake_min_cells for N < 187, so no small world has a lake):

```toml
segment_on_share = "0.12"
lake_min_area_m2 = 10240
lake_max_area_m2 = 160000
lake_area_ref_n = 128
lake_min_depth_m = 1
pass_tarn_min_cells = 20
pass_divide_reach_m = 400
pass_saddle_window_m = 200
pass_saddle_rise_m = 75
pass_saddle_rise_reach_m = 600
pass_min_spacing_m = 800
pass_col_tarn_m = 64
pass_halfwidth_m = 200
pass_spacing_m = 1000
```

The other nine shrink the pass rules' radii to the 2 km map of N = 128, where ranges are a few kilometres long and the shipped radii find almost no saddle (one pass in 64 worlds at N = 128). With them, N = 128 seeds 0 to 63 (spec writer, exploration build) give 20 lowered passes in 19 worlds, 18 of them capped and 2 uncapped (seeds 31 and 47), two passes on one range (seed 8), and at least one tried candidate failing each rule of E6.5.6 except the sea rule (c) and the natural gap (i), which the worked examples of 10.1 cover: coast (seeds 0, 1, 20, 30, 51), barrier (0, 3, 24, 25, …), tarn on the crossing (9), the chord (23, 28, 50), side length (30), across (2, 7, 9, 17, …), col tarn (11, 41, 44, 56), summit near a top (20, 62), zone above p90 (0, 18, 40, 53, 55), above the feet (10, 15, 22, 31), shoulders (38); ranges with slots and no candidate occur in most worlds. (The tarn rule needs `pass_tarn_min_cells` 20: basins on 2 km maps are small.)

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
| num, den | num = min(W, `pass_size_ref_n`), den = `pass_size_ref_n` (W the grid width; N on the map) | 512/1024, 1024/1024, 1024/1024 |
| coast_clear (fifths) | mul_div(`pass_coast_clear_m`.raw, 5 × num, 16 × 2^32 × den) | 109 / 219 / 219 (N = 128: 27) |
| min_len (fifths) | (`pass_min_length_m` × 5 × num) div (16 × den) | 468 / 937 / 937 (128: 117) |
| spacing (fifths) | (`pass_spacing_m` × 5) div 16, not scaled | 937 (small fixture: 312) |
| side_max | from_raw(mul_div(`pass_side_max_m`.raw, num, den)), metres | 1500 / 3000 / 3000 m (128: 375 m) |
| reach5 (fifths) | mul_div(`pass_divide_reach_m`.raw, 5, 16 × 2^32) | 313 (small fixture: 125) |
| win, rise_b, spacing_b, tarn_b (cells) | floor_int(field) div 16 for `pass_saddle_window_m`, `pass_saddle_rise_reach_m`, `pass_min_spacing_m`, `pass_col_tarn_m` | 25, 93, 156, 10 (small fixture: 12, 37, 50, 4) |
| rise_u, body_u (units) | `pass_saddle_rise_m`.raw >> 16, `pass_body_m`.raw >> 16 (units of 2^−16 m, E6.5.0) | 9830400, 19660800 |
| crest_lim | mul(`pass_crest_frac`, w_b).raw | per world |
| D_o^f, D_d^f | D_o^f = mul_int(`final_talus_slope`, 16), D_d^f = mul(D_o^f, SQRT2) | 80 m (raw 343597383680), 113.14 m (raw 485920080000) |

## 5. Algorithm

### 5.0 Conventions

- n = N²; cell (x, y), index i = yN + x (on a W × H test grid i = yW + x); D8 codes, 8- and 4-neighbour orders of P1-00 2.3.
- Every value is `Fx` unless stated; every operation is the P0-01 operation of that name: `+` and `−` checked; `mul`, `div`, `div_int`, `mul_div`, `from_ratio` with RHA; `mul_int` exact; `abs`, `min`, `max`, `clamp`, `fade`, `hypot`, `round_int`; `isqrt_u64`; `hash_at`, `substream`; `SQRT2`, `INV_SQRT2` of amendment A1; `kth`, `quantile`, `quantile_rank` of G1. `0` is `Fx()` (raw 0); `ONE`, `HALF` the P0-01 constants; `from_raw(1)` the smallest positive `Fx`. A plain integer expression (no `Fx` operation named) is exact `int64` arithmetic, `div` meaning floor division of non-negative operands.
- `rc(a, b)` ("ratio, capped", for a ≥ 0, b > 0) = `from_int(RATIO_CAP)` if a ≥ mul_int(b, RATIO_CAP), else div(a, b).
- `quantile_rank(q, n)` = min(n − 1, (q.raw × n) >> 32) in `uint64`, fatal if n = 0 (P1-00 7.1).
- Statements are evaluated exactly in the order written, one operation at a time with that operation's rounding. Parenthesised sums are evaluated as parenthesised; an unparenthesised chain `a + b + c` left to right.
- Step order: E1, E2, E3, E4, E5 (E5.1, E5.2, E5.3), E6, E6.5 (E6.5.1 to E6.5.8), E7, E8 (the iterations E8.1 to E8.6, then E8.7), E9, E10, E11. E10.0 depends only on the seed, the cell and O, so it may run at any point after E4 (the exploration code runs it after E7 and before E8). E5.3 and E6.5 are independent of each other (E6.5 reads no ocean depth). Each step names its determinism rule (P1-00 8).

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

Keep h0 and unit per land cell, and for E6.5 also mask and profile: `mask[i]` = the value mask above (0 on ocean), `profile[i]` = profile where mask > 0, else 0. (The `if` only saves work: unit is 0 whenever mask is 0.) Since profile ≥ `foothill_fraction` whenever mask > 0 (ridge ≥ 0), `max(0, profile − foothill_fraction)` in E6.5 is profile − `foothill_fraction` there. After E5.2 the grids c and ns are no longer needed (section 8, memory).

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

### E6.5. Pass layout (single-threaded; D5, D6, D7, D8 as stated per part)

Places the pass sites and computes, for E7, a pass factor P[c] (`Fx`, ONE where no pass reaches) and a floor adjustment adj[c] (`Fx` raw, 0 where none) for every land cell. Nothing in E6.5 changes h0, unit, mask, profile or O. With `pass_count_max` = 0 no range has a slot, so P = ONE and adj = 0 everywhere and E6.5.3 to E6.5.8 need not run (E6.5.1 and E6.5.2 produce nothing that reaches the output either; see section 8 for skipping them).

#### E6.5.0 Notation

All quantities of this step are exact integers or `Fx`, with the operations of 5.0.

- **Units.** A height in *units* is an `int64` count of 2^−16 m. `mh(c) = mul(scale, unit[c]).raw >> 16` (the natural mountain part; ≥ 0) and `ter(c) = (h0[c].raw >> 16) + mh(c)` (the natural pre-erosion terrain; may be negative). `>>` on a signed value is the arithmetic shift (floor). `K(c) = max(0, ter(c))` is the flood key.
- **Lowered mountain part.** For a land cell c and `Fx` p ∈ [0, ONE], with f = `foothill_fraction` and keep = `pass_texture_keep`:
  ```
  m'(c, p) = mul(scale, mul(mul(p, mask[c]), f) + mul(mul(max(p, keep), mask[c]), max(0, profile[c] − f)))
  ```
  evaluated in this order. The foothill base falls in proportion to p; the relief above it keeps at least the share keep. m'(c, ONE) is mul(scale, unit[c]) up to rounding (section 6.5).
- **Cells and distances.** x(c), y(c) the coordinates of index c; `d2(a, b) = (x(a) − x(b))² + (y(a) − y(b))²` (cells², `int64`). A step between 8-adjacent cells has length 5 (orthogonal) or 7 (diagonal) **fifths**; fifths to metres is `fm(f) = from_ratio(16 × f, 5)` (`Fx`; f may be negative).
- **Box query** `B(c, r)` (r ≥ 0 an integer): the cells (x, y) with |x − x(c)| ≤ r, |y − y(c)| ≤ r and (x − x(c))² + (y − y(c))² ≤ r², clipped to the grid, visited by rows (y ascending), each row by x ascending: that is, in index order. Every use below is a count, a sum, a maximum, a minimum or an "any"; where an argmin or argmax is taken, the first cell found wins, which is the cell with the smallest index among the tied ones.
- **Cross product** of a vector a = (a_x, a_y) with the offset (dx, dy) of a cell q from a cell c: `cr = a_x × dy − a_y × dx`. cr > 0 is the *left* side of a, cr < 0 the *right* side; cr = 0 is neither.
- **Dijkstra** (D6): a 5/7 shortest-path search from a set of source cells, each with distance 0 and a source value; a priority queue of pairs (distance, cell), popped in ascending order of the pair (unique, since a pair is pushed only when its distance is strictly smaller than the cell's current one); a popped pair whose distance is not the cell's current distance is skipped; for a popped cell c and each neighbour m in the 8-neighbour order that exists and is in the search's domain, nd = d(c) + (5 or 7); m is updated (distance nd, source of c) and pushed only if nd ≤ the search's limit and (m has no distance yet or nd < d(m)). So a cell keeps the source of the first strictly shortest relaxation. Distances are unique values; the sources depend on this order. Sources are pushed with distance 0 in index order; a cell listed twice as a source is pushed once, with its first source value.

#### E6.5.1 World values (D6 for the flood, D1 otherwise)

1. `world_top` = the land cell with the largest mh, ties: smallest index.
2. **Tarn basins.** The flood levels of G3 on keys `kb[c] = max(1, (h0[c] + mul(scale, unit[c])).raw)` on land (ocean cells: key 0, ignored by G3) with the ocean mask O; with its flood level Lb, `basin[c]` = (c land) and Lb[c] − kb[c] ≥ `pass_basin_depth_m`.raw. These are depressions of the natural pre-erosion surface at least 40 m deep. Only the values Lb are used (the spill levels, a function of the keys; P1-00 7.3), so any method that gives these levels may be used, and, as for G3, a grid with no ocean cell is a fatal error ("elevation: pass basins: no ocean"; it cannot happen after E4): the shared G3, or (as the implementation does, to meet the memory budget of 8.1) a levels-only priority flood from the ocean that pops by the unique pair (level, cell) and stores no parent or order.
3. `bigbasin[c]` = c is in an 8-connected component of basin cells (G2) with at least `pass_tarn_min_cells` cells.
4. `docean[c]` = the P1-02 H7 chamfer distance (fifths) from c to the nearest ocean cell, with the ocean cells as sources (0 on ocean). On an unobstructed grid the two-pass 5/7 transform equals the exact shortest 5/7 path length, so for any cell c, docean[c] = min over ocean cells o of 5 × max(|dx|, |dy|) + 2 × min(|dx|, |dy|) (dx, dy the offset of o from c). It is read only at candidate cols (E6.5.6 (a)), where either form may be used.
5. Derived integers num, den, coast_clear, min_len, spacing and side_max of 4.5.

#### E6.5.2 Ranges (D5 for the components; D6 or any method for the geodesic distances, which are values)

1. `crest[c]` = c land, mask[c] ≥ `pass_core_mask`.raw and |nb[c] − o*.raw| ≤ crest_lim. The crest cells lie near the belt's centre level set in the mountain core.
2. **Ranges** are the 8-connected components of crest cells (G2), labels k = 1, 2, … in G2 order. `label[c]` = k for a crest cell, 0 elsewhere.
3. Per range k: s0 = its smallest cell index. `g_S` = the 5/7 geodesic distance (fifths) inside the range from s0 (paths through cells of range k only). **End A** = the range cell with the largest g_S, ties: smallest index. `dA` = the geodesic distance inside the range from A; **end B** = the range cell with the largest dA, ties: smallest index; `len_k = dA(B)`. dA is kept for every crest cell.
4. Slots: `np_k = 0` if len_k < min_len, else `min(pass_count_max, 1 + (len_k − min_len) div spacing)`.
5. `top_k` = the range cell with the largest mh, ties: smallest index. `p90_k = kth(mh over the range's cells, quantile_rank(from_ratio(9, 10), count))` (G1; from_ratio(9, 10) is raw 3865470566).

If no range has np_k > 0, E6.5.3 to E6.5.8 produce no site: P = ONE and adj = 0.

#### E6.5.3 Support, body and the drainage divide (D6)

1. `support[c]` = c land and mask[c] ≥ `pass_region_mask`.raw. `body[c]` = support[c] and mh(c) ≥ body_u.
2. **Divide flood** over the support, seeded just outside it. Arrays: level (units), parent (a cell), side (0, 1 or 2), all unset at first.
   ```
   seq = 0; Q = empty                                   // entries (level, seq, cell), popped in ascending (level, seq)
   for c in 0 .. n−1 (index order):
       if not support[c] and some 8-neighbour of c exists and is support:
           level[c] = 0; parent[c] = c; side[c] = (nb[c] ≥ o*.raw) ? 2 : 1
           push (0, seq, c); seq += 1
   while Q is not empty:
       pop (k, ·, c)
       for code in 0 .. 7 (8-neighbour order):
           m = the neighbour of c in direction code; skip if outside the grid, not support, or level[m] is set
           level[m] = max(K(m), k); parent[m] = c; side[m] = side[c]
           push (level[m], seq, m); seq += 1
   ```
   The seeds are lowland and ocean cells next to the support (`nb` is defined on every cell). `side` says on which flank of the belt (the sign of nb − o*) the cell's drainage, followed down the parents, leaves the mountains. Every support cell is reached on the N × N map (land never touches the map edge, so every support component has a non-support neighbour); on a test grid a support cell that is reached by no seed keeps side 0 and takes no part below.
3. **Divide cells.** For each cell c in index order with body[c] and side[c] = 1: among its 4-neighbours m in the 4-neighbour order (E, S, W, N) that exist with body[m] and side[m] = 2, the **partner** of c is the first one with the smallest K(m) (a later one replaces it only with a strictly smaller K). If there is one, c is a divide cell with partner `pt(c)` and pair height `dh(c) = max(K(c), K(pt(c)))` (units). Only side-1 cells are divide cells; a partner may be the partner of several.
4. **Range of a divide cell.** A Dijkstra (E6.5.0) over the domain "support cells", from every crest cell as a source (its source value is itself), with limit reach5. A divide cell c that is reached belongs to the range `label(source(c))`; an unreached one belongs to none. For each range k, `D_k` is the list of its divide cells in index order.

#### E6.5.4 Candidate cols (per range, in label order; D7)

The candidates of range k are computed, and its cols chosen (E6.5.5), before range k + 1 is looked at; ranges with np_k = 0 or an empty D_k are skipped. For each c in D_k, in order, c is a **candidate** when:

1. **Local minimum.** No cell q ≠ c of D_k in B(c, win) has (dh(q), q) < (dh(c), c) in lexicographic order.
2. **Crossing line.** `chainA(c)` = c, parent(c), parent(parent(c)), …, ending at the first cell whose parent is itself (a seed); `chainB(c)` = the same from pt(c). With pa = the cell at position min(20, |chainA| − 1) of chainA (position 0 is c) and pb likewise in chainB, the **across vector** is a(c) = (x(pb) − x(pa), y(pb) − y(pa)) (cells).
3. **Rise on both sides.** Over the cells q of D_k in B(c, rise_b), with cr the cross product of a(c) with the offset of q from c: lmax = the largest dh(q) with cr > 0, rmax = the largest with cr < 0 (each −1 if there is none). c is a candidate if lmax ≥ dh(c) + rise_u and rmax ≥ dh(c) + rise_u: a saddle of the divide, not a shoulder or the end of a range.

Every chain is finite and ends at a seed (a parent was popped before its child, so following parents strictly decreases the pop position), and all its cells have the same side, so chainA and chainB are disjoint. The candidates keep their order in D_k and their across vectors.

#### E6.5.5 Choosing cols (per range; D7)

`world_cols` is the list of every col placed so far in the world (lowered passes and natural gaps, all ranges), `cols_k` those of range k. For slot j = 0 .. np_k − 1:

```
repeat:
    for each candidate c of range k not yet tried, with d2(c, w) ≥ spacing_b² for every w in world_cols:
        dmin  = min(d2(c, A_k), d2(c, B_k), d2(c, w) for every w in cols_k)            // int64
        hj    = hash_at(K_pass, c, j) >> 54                                             // in [0, 1024)
        score = dmin × (1024 + (hj × 3) div 10)
    if there is no such candidate: range k gets no further site; go to the next range
    best = the one with the smallest dh, then the largest score, then the smallest index
    mark best tried; run the crossing tests of E6.5.6 on it
until it gives a pass or a natural gap
append the col to cols_k and world_cols; the site fills slot j
```

`hash_at` takes the cell index and the slot index j as its two `int64` coordinates. The lowest natural saddle is tried first; the score only orders candidates of equal pair height, away from the ends and the range's other cols. A failed candidate is not tried again for a later slot. Sites are numbered s = 0, 1, … in the order they are found (all ranges, natural gaps included); `s` is the site index used in E6.5.7.

#### E6.5.6 Crossing tests for a col c (in this order; the first failure ends the attempt)

With p = pt(c), a = a(c) of E6.5.4, and k its range:

- **(a) Coast.** Fail if docean[c] < coast_clear.
- **(b) Barrier.** Fail if mul(mask[c], scale) < `pass_barrier_m`.
- **(c) Crossing polyline and sea.** `axis` = chainA(c) reversed (from its seed up to c), followed by chainB(c) (from p down to its seed); `ci` = |chainA(c)| − 1, the position of c (p is at ci + 1). Fail if the last cell of chainA or of chainB is an ocean cell. Arc positions (fifths): arc[0] = 0, arc[q] = arc[q − 1] + (7 if axis[q − 1] and axis[q] differ in both coordinates, else 5). reach_A = fm(arc[ci]), reach_B = fm(arc[last] − arc[ci]).
- **(d) Side length.** qA = the largest q < ci with body[axis[q]] false, qB = the smallest q > ci with body[axis[q]] false (both exist: a chain ends at a seed, which is not support); the **body exits** are e_A = axis[qA], e_B = axis[qB]; body5_A = arc[ci] − arc[qA], body5_B = arc[qB] − arc[ci]. Fail if max(fm(body5_A), fm(body5_B)) > side_max.
- **(e) Across, not along.**
  1. *Local divide.* Over the cells q of D_k in B(c, 37), with (dx, dy) the offset of q from c and cr the cross product of a with it: sum dx, dy and count separately for cr > 0 (sx_L, sy_L, n_L) and cr < 0 (sx_R, sy_R, n_R). If n_L > 0 and n_R > 0: t = (fdiv(sx_R, n_R) − fdiv(sx_L, n_L), fdiv(sy_R, n_R) − fdiv(sy_L, n_L)), fdiv the floor division (rounds toward −∞); for each exit e of (e_A, e_B), with v = (x(e) − x(c), y(e) − y(c)): fail if 2 (v·t)² > |v|² |t|² (the side leaves within 45° of the divide's direction). If either count is 0 the test passes.
  2. *Crest axis.* Over the crest cells q of range k in B(c, rise_b): cmin = the first with the smallest dA, cmax = the first with the largest dA. If both exist and differ: t' = (x(cmax) − x(cmin), y(cmax) − y(cmin)); for each exit e as above: fail if 2 (v·t')² > |v|² |t'|².
- **(f) Chord.** chord = (x(e_B) − x(e_A), y(e_B) − y(e_A)), chord2 = |chord|², arc5 = body5_A + body5_B. Fail if 4 arc5² > 225 chord2 (the body arc is more than 1.5 × the chord). Else, if t' of (e) 2 exists: fail if 100 (chord·t')² > 33 chord2 |t'|² (the chord is more than about 35° off the crest axis's perpendicular: cos² > 0.33).
- **(g) Tarn on the crossing.** Fail if some axis cell q has body[q] and bigbasin[q].
- **(h) Values.**
  ```
  maxm   = max over the axis of mh                                     // units; 0 for an empty max
  M      = from_raw(maxm << 16)                                         // metres
  vj     = from_raw((hash_at(K_pass, c, 16) >> 32) × 2) − ONE           // in [−1, 1)
  wj     = from_raw((hash_at(K_pass, c, 17) >> 32) × 2) − ONE
  grade  = max(0, mul(pass_grade, ONE + mul(pass_grade_jitter, vj)))
  saddle = clamp(mul(mul(grade, min(reach_A, reach_B)), from_ratio(8, 15)), pass_min_col_m, pass_saddle_max_m)
  capped = false
  if M − pass_max_cut_m > saddle: saddle = M − pass_max_cut_m; capped = true
  halfwidth = mul(pass_halfwidth_m, ONE + mul(pass_width_jitter, wj))
  ```
  from_ratio(8, 15) is raw 2290649225. The target saddle is a walkable grade from the nearer foot (0.9 × 8/15 = 0.48 m per metre, with jitter), clamped to [150, 600] m, unless that would cut the crossing's highest point by more than the cap: then the site is a **capped** (high) col whose target is M − cap.
- **(i) Natural gap.** If M ≤ saddle the crossing is already below its target: the site is a **natural gap**. It fills the slot (E6.5.5) and is recorded, and nothing is lowered. (No further test runs for it.)
- **(j) Tarn at the col.** Fail if some cell of B(c, tarn_b) is a basin cell.
- **(k) Summits near the crossing.** hr = (floor_int(halfwidth) div 16) div 2 (cells; about where the bump falls to one half). Fail if d2(q, top_k) ≤ hr² or d2(q, world_top) ≤ hr² for some axis cell q.
- **(l) Zone below the crest p90.** zmax = the largest mh over the land cells of B(axis[a], hr) for a = 0, 4, 8, … (every fourth axis position from the first). Fail if zmax > p90_k.
- **(m) Above the feet.** Fail if (h0[c] + saddle) − max(h0[axis[0]], h0[axis[last]]) < `pass_min_col_m`. The col's target stands at least 150 m above the higher of the two lowland ends.
- **(n) Shoulders.** need = max(`pass_barrier_m`.raw, (saddle + `pass_peak_rise_m`).raw) >> 16 (units). Over the cells q of D_k in B(c, 187) with d2(q, c) ≥ 81, with cr the cross product of a with the offset of q from c and tv = max(ter(q), ter(pt(q))): hl = the largest tv with cr > 0, hr' = the largest with cr < 0 (each −1 if none). Fail if hl < need or hr' < need. (187 = 3000 div 16 and 81 = (150 div 16)²: the divide 150 m to 3 km from the col.)
- **(o) Depth.** With the axis cells c_0 … c_last:
  ```
  fits(p) = every axis cell has m'(c_q, p) ≤ saddle, and, unless capped,
            |m'(c_q, p) − m'(c_{q−3}, p)| ≤ mul_int(pass_walk_grade, 48) for q = 3 .. last
  if fits(ONE): lo = ONE
  else: lo = 0; hi = ONE; repeat 32 times: mid = from_raw((lo.raw + hi.raw) >> 1); if fits(mid): lo = mid else hi = mid
  top(p) = max over the axis of m'(c_q, p)                          // 0 for an empty max
  floor_m = M − pass_max_cut_m
  if top(lo) < floor_m:                                              // the walk grade asked for more than the cap
      plo = lo; phi = ONE
      repeat 32 times: mid = from_raw((plo.raw + phi.raw) >> 1); if top(mid) ≥ floor_m: phi = mid else plo = mid
      lo = phi; capped = true
  depth = ONE − lo
  ```
  The site is a **lowered pass**: it fills the slot, with its axis, ci, saddle, halfwidth, depth and capped flag. The walk-grade clamp asks that the lowered crossing rise at most `pass_walk_grade` × 48 m over every 3 cells; the cap bounds the depth the clamp may ask for. If nothing fits, lo stays 0 before the raise.

A failure of (a) to (g) or (j) to (n) marks the candidate tried and moves on to the next candidate (E6.5.5). The tests use only values fixed before E6.5.5 started (the placed cols enter only through E6.5.5's spacing and score).

#### E6.5.7 Pass factor (D6 per site, in site order)

P[c] = ONE for every cell. For each lowered pass s, in site order (s counts natural gaps too):

```
hwmax = mul(halfwidth, ONE + pass_width_var)
lim5  = mul_div(hwmax.raw, 5, 16 × 2^32) + 2                   // fifths
Dijkstra over the domain "land cells" from every axis cell c_a (source value a), limit lim5
for each reached cell c, in index order, with distance d and source a:
    s_m  = fm(arc[a] − arc[ci])                                 // signed metres from the col along the crossing
    dend = fm(min(arc[a], arc[last] − arc[a]))                  // metres to the nearer end
    dist = fm(d)
    nz   = noise2(substream(K_pass, 3), mul(s_m, from_ratio(1, 1500)), from_int(3s + 2))
    hwv  = mul(halfwidth, ONE + mul(pass_width_var, nz))
    if dist ≥ hwv: skip c
    bump = mul(fade(ONE − div(dist, hwv)), fade(clamp(div(dend, from_int(300)), 0, ONE)))
    P[c] = min(P[c], ONE − mul(depth, bump))
    record (c, a, bump) for site s
```

The band is about one half-width on each side of the crossing, its width varying along the crossing with a noise of 1.5 km wavelength (`noise2` of P0-01, |nz| ≤ 1, so hwv ∈ [0.1, 1.9] × halfwidth); the bump falls smoothly to 0 at the band's edge and over the last 300 m before each end of the crossing. hwv ≥ 0, and dist ≥ hwv is tested first, so `div(dist, hwv)` is evaluated only for hwv > 0. P is the minimum over sites. Each site's recorded list (index order) is kept for E6.5.8.

#### E6.5.8 Floor drainage (in site order)

For each lowered pass s, with ε = `pass_drain_step_m`, along its axis (q = 0 .. last):

```
S[q] = h0[c_q] + m'(c_q, P[c_q])          // the lowered surface, P after every site of E6.5.7
N[q] = h0[c_q] + m'(c_q, ONE)             // the natural surface
U = copy of S;  for q = ci − 1 down to 0: U[q] = min(S[q], U[q + 1] − ε)
                for q = ci + 1 up to last: U[q] = min(S[q], U[q − 1] − ε)
L = copy of N;  for q = 1 up to ci: L[q] = L[q − 1] + ε
                for q = last − 1 down to ci + 1: L[q] = L[q + 1] + ε
Δ[q] = max(U[q], min(N[q], L[q])) − S[q]
for each recorded (c, a, bump) of site s, in its order:
    if bump.raw > best[c]: best[c] = bump.raw; adj[c] = −mul(Δ[a], bump).raw
```

`best` starts at −1 and `adj` at 0 for every cell, over all sites together. U falls from the col by at least ε per cell, so the lowered floor drains outward; L rises by ε per cell from each foot's natural height toward the col (side A up to ci, side B up to ci + 1). Δ < 0 is a cut that drains a pit of the lowered floor toward the feet; Δ > 0 lowers less where the lowered floor would fall below the rising line from its foot and hold water, but never to above the natural surface. A cell takes the adjustment of the site with the largest bump there; ties keep the lower site index.

Natural gaps get no factor and no adjustment.

### E7. Compose (D1, G6 row blocks)

Land:

```
m = mul(scale, unit)
if P[c] < ONE:  m = m'(c, P[c])                                 // the lowered envelope
if adj[c] ≠ 0:  m = min(m − from_raw(adj[c]), mul(scale, unit)) // floor drainage, never above natural
h = clamp(h0 + m, floor, H_MAX)
```

Ocean: h from E5.3. Where no pass reaches (P = ONE, adj = 0) this is the third version's `clamp(h0 + mul(scale, unit), floor, H_MAX)`.

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
- With `erosion_iterations` = 0 the iterations do nothing (E8.7 still runs).
- **The contract is kept, and the largest land height does not grow** (section 6.2).

**E8.7 Final talus** (D3 of D2 steps; new in this version). After the last iteration:

```
for it in 0 .. final_talus_iterations − 1:
    t = E8.6's talus step on h with D_o^f, D_d^f (4.5) in place of D_o, D_d and final_talus_rate in place of talus_rate
    h = t
```

**Precondition** (guaranteed by the stage): every land cell has h ≥ floor on entry, since E7 clamps to floor and every write of E8 ends with max(floor, ·) (6.2). The `final_talus` seam checks it and treats a land value below floor as a fatal error; with it, t = max(floor, h) = h on a cell with δ = 0, which the active-set argument below uses.

That is, for every land cell c, δ = the sum over the existing 8-neighbours m (8-neighbour order) of −mul(rate, x − Dk) where x = h[c] − hm > Dk, and of +mul(rate, (−x) − Dk) where m is land and −x > Dk (hm = 0 for an ocean neighbour, Dk = D_d^f for odd codes, D_o^f for even); t[c] = max(floor, h[c] + δ); ocean cells unchanged. Each step reads the whole grid h of the previous step and writes a new one (Jacobi), so its result does not depend on the order in which cells are evaluated or on a split into row blocks (it may run on G6 row blocks in parallel).

With the shipped values (80 m per orthogonal step, 78.7°; rate 1/16; 100 steps) it touches only faces steeper than the mountains' own talus limit of E8.6, mostly where a range meets the shore: the ocean counts as height 0, so a cliff above the sea is lowered toward a 5:1 face. Third version's limitation 11 (512 seed 41, `step_p999_m` 183.8 m) becomes 136.4 m (section 11).

**Active set** (an allowed evaluation, used by the exploration code; it gives the same result). Step 0 evaluates every land cell. A cell's δ depends only on its own height and its neighbours'; so in step it + 1, δ can be non-zero only for a cell that changed in step it or has a neighbour that did, or whose δ was non-zero in step it. The exploration evaluates, in step it + 1, exactly the land cells that are, or are 8-adjacent to, a cell with δ ≠ 0 in step it (in index order), applies all new values of a step only after evaluating all of them, and stops early when that set is empty; this equals the full Jacobi step, because a cell outside the set has δ = 0 by the argument above and keeps its value (t = max(floor, h) = h, as h ≥ floor).

### E9. Quantization (D1, G6 row blocks)

```
q = mul_div(h.raw, 10, 2^32)                       // decimetres, RHA
e = (c in O) ? min(−1, q) : max(1, q)              // int16
```

E9 quantizes the grid after E8.7. Land e ∈ [1, 29900], ocean e ∈ [−29005, −1] (section 7); a value outside [−32768, 32767] would be fatal and cannot occur.

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

Write `elevation` (`i16`, codec 1) as the stage's only layer, in one stage transaction, followed in the same transaction by the rows of table `pass` (section 9.1).

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
8. With `erosion_iterations` = 0, the layer equals E10(E9(E8.7(E7 output))): the iterations do nothing, E8.7 still runs (with `final_talus_iterations` = 0 as well, it is E10(E9(E7 output))).
9. Unless E5.1 triggers, (o*, β*) = (0, `segment_bias`). If it triggers, core(o*, β*) ≥ target or β* = 2, and β* ≥ `segment_bias`.
10. **Outlines.** O, and so the sign of every cell of the layer, equals that of `GENERATOR_VERSION` 0.5.0 for the same seed, N and values of the third version's fields: E1 to E4 are unchanged and no later step reads anything but O's membership (6.5).
11. **Neutral settings.** With `pass_count_max` = 0 and `final_talus_iterations` = 0 the layer equals the third version's (0.5.0) for the same values of the third version's fields.
12. **Pass sites.** Every site's col c is a divide cell (6.5) of its range k, whose partner is a 4-neighbour of the other side; d2(c, c') ≥ spacing_b² for any two cols of the world; range k has at most np_k ≤ `pass_count_max` sites; every lowered pass passed every test of E6.5.6; a natural gap passed (a) to (g) and has maxm ≤ saddle.
13. **The passes never raise the natural envelope** (beyond one rounding step): after E7, for every land cell, the mountain part m satisfies m ≤ mul(scale, unit) + R with R = (scale.raw >> 32) + 1 raw (R ≤ 8701 raw, 2.1 µm), and m = mul(scale, unit) exactly where P = ONE and adj = 0; where adj ≠ 0, m ≤ mul(scale, unit) exactly. So every land cell of E7 is at most R above its 0.5.0 value. The floor drainage may leave a cell higher than the lowered envelope m'(c, P) (where Δ > 0), never higher than the natural part (6.5).
14. **Cut cap.** For every lowered pass, max over its crossing of m'(c, ONE − depth) ≥ M − `pass_max_cut_m`: before the drainage adjustment, overlapping passes and erosion, the crossing's highest point is lowered by at most the cap (6.5).
15. **E8.7 keeps the contract** of E7 and never raises the largest land height (6.6); in particular every land cell is ≥ floor on entry to E8.7 and after each of its steps.

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

### 6.5 Passes (E6.5, E7)

**Outlines (invariant 10).** E6.5 reads O and writes P and adj; E7 writes land cells only, clamped to [floor, H_MAX] with floor > 0; E8 to E10 never change O (6.2, 6.3); E8.7 writes land cells only and ends with max(floor, ·). Nothing else changes in E1 to E4. So the land/ocean mask is that of 0.5.0.

**Divide cells.** The divide flood is G3-like over the support with unique (level, seq) pops (D6), so parent and side are defined. A divide cell is a body cell draining to side 1 next to a body cell draining to side 2: the drainage divide of the range, sampled on the 4-neighbour pairs. Its pair height dh is the higher terrain of the pair, so a candidate's dh is a saddle height of the divide (a local minimum along the divide, E6.5.4 1, with higher divide on both sides of the crossing line, E6.5.4 3).

**m' is monotone and m'(c, ONE) ≈ mh.** `mul` is monotone in each argument for non-negative arguments, and mask, profile − f, f, keep and scale are ≥ 0, so m'(c, p) is non-decreasing in p. At p = ONE, mul(ONE, mask) = mask exactly and max(ONE, keep) = ONE, so the inner sum is mul(mask, f) + mul(mask, profile − f). Each `mul` is within ½ raw of its exact product and the two exact products add up to the exact mask × profile, so the sum differs from mul(mask, profile) = unit by at most 1 raw (an integer within 1.5 of it). Then mul(scale, u ± 1) differs from mul(scale, u) by at most (scale.raw >> 32) + 1 raw. So |m'(c, ONE) − mul(scale, unit[c])| ≤ R.

**Invariant 13.** Where P = ONE and adj = 0, E7 computes the third version's m. Where P < ONE and adj = 0, m = m'(c, P) ≤ m'(c, ONE) ≤ mul(scale, unit) + R. Where adj ≠ 0, m = min(·, mul(scale, unit)). `clamp` is monotone, so h7 ≤ h7(0.5.0) + R. The adjustment adj = −mul(Δ, bump): Δ < 0 lowers further, Δ > 0 raises above m'(c, P), but the `min` keeps it at or below the natural part.

**Invariant 14.** With M = maxm << 16 ≤ mul(scale, unit[c*]) for the axis cell c* that has the largest mh, top(ONE) ≥ m'(c*, ONE) ≥ M − R > M − `pass_max_cut_m`, since the cap is at least 1 m (4.2) and R < 1 m. If top(lo) ≥ floor_m after the walk-grade bisection, the cap holds at lo. Otherwise the second bisection keeps top(phi) ≥ floor_m from its start (phi = ONE) through every step, and ends with lo = phi. Because m' is monotone in p, the same lo bounds the cut at every crossing cell from below in the sense of invariant 14; where lo ≥ keep the lowered part is lo × (mask × profile) up to rounding at every cell, so every crossing cell, the col included, is cut by at most about lo's share of the cap (cut(c) ≈ (1 − lo) mh(c) ≤ (1 − lo) M ≤ cap). Where lo < keep, m'(c, lo) ≤ keep × mh(c) up to rounding, so top(lo) ≥ M − cap needs M ≤ cap / (1 − keep) (667 m with the shipped values): only such low crossings can get there, and there the col alone may be cut by more than the cap. Measured after erosion at 1024 (seeds 0–49, cap 500 m): the cut at the col has p50 498 m, p90 501 m, max 503 m (`r5c500-extra-1024.txt`).

**Summits (bound and measurement).** E6.5.6 (k) keeps every axis cell more than hr cells (straight line) from top_k and world_top. The 5/7 path distance is at least 7/(5√2) ≈ 0.99 of the straight-line distance, and hwv ≤ halfwidth × (1 + `pass_width_var`) (|nz| ≤ 1), so at those two cells a site's bump is at most fade(1 − 15.8 hr / (halfwidth × (1 + `pass_width_var`))), about 0.70 with the shipped values (hr ≈ halfwidth / 32 cells), and their factor is at least ONE − 0.70 × depth. This bounds the lowering by the factor at a top to 0.70 × depth of its mountain part; the floor adjustment of E6.5.8 is not bounded there. That is the only guarantee. Measured (`r5c500-extra-1024.txt`, 1024 seeds 0–49): no world's highest point drops by more than 50 m; no 0.5.0 cell within 1 km of a col is lowered by more than 300 m, none within 3 km by more than 200 m; at 2048 (seeds 0–7, cap 400) the same holds.

**Termination and size.** Every flood, component labelling and Dijkstra visits each cell of its domain at most once per strictly smaller distance; each bisection has 32 steps; each candidate is tried at most once; each range has at most `pass_count_max` slots. The number of sites is at most 3 × the number of ranges.

### 6.6 E8.7 keeps the contract (invariant 15)

E8.7 is E8.6's step with D_o^f ≥ 0.8 m (`final_talus_slope` ≥ 0.05) and rate ≤ 1/8. A cell gains only from land neighbours m with h[m] − h[c] > Dk ≥ D_o^f; with M the largest land height before the step, the exact total gain is at most 8 × (1/8) × (M − h[c] − D_o^f), plus at most 8 × ½ raw of rounding, and losses only lower it; so t[c] ≤ M − D_o^f + 4 raw < M. A cell that gains nothing does not rise. Every write ends with max(floor, ·). So after E8.7 every land cell is in [floor, H_MAX] and the largest land height has not grown; E9 gives land e ∈ [1, 29900] and E10's arguments (6.3) hold unchanged, since they use only E9's grid, O and the contract.

## 7. Determinism notes and overflow bounds

| Step | Rule | Notes |
| --- | --- | --- |
| E1 centre | D1 | two location hashes, computed once |
| E1, E2 | D1, parallel by G6 row blocks | location-keyed noise only |
| E3 quantile, medians | D7, G1 | values; any selection method; an array selected in place is used only as a multiset afterwards |
| E3 gradient | D2, parallel | reads the complete nb grid, writes g |
| E4 | D5, G2 | |
| E5.1 | D8 counts, D3 | exact integer counts (any order or block split); fixed offset order; 32 bisection steps, no tolerance |
| E5.2 | D1, parallel | |
| E5.3 chamfer | D4 | two sweeps in stated orders, single-threaded |
| E6 | D7, G1 | |
| E6.5.1 basins | D6 (G3), D5 (G2) | only the spill levels are used, a function of the keys; any method |
| E6.5.1 docean | D4 or closed form | the chamfer equals the 5/7 path length; read only at cols |
| E6.5.2 ranges | D5 (G2); geodesic distances are values | ends and tops: argmax with ties to the smallest index; p90 by G1 |
| E6.5.3 divide flood | D6, single-threaded | pops by (level, seq), seq the global push counter; seeds in index order |
| E6.5.3 range search, E6.5.7 factor search | D6, single-threaded | pops by (distance, cell), unique pairs; the source of a cell is fixed by that order (E6.5.0) |
| E6.5.4 to E6.5.6 | D7, single-threaded | ranges in label order; divide cells in index order; candidates chosen by (dh, −score, index), a strict total order; box queries in index order, argmin/argmax ties to the smallest index; the one hash per (cell, slot) and per (col, 16 / 17) is location-keyed (D9) |
| E6.5.7, E6.5.8 | D4 in site order | P is a minimum over sites (order-free); adj takes the largest bump, ties to the lower site index; reached cells in index order |
| E7 | D1, parallel | reads P and adj |
| E8.1 routing | D6, single-threaded | pop order (key, seq) |
| E8.1 G4, E8.2, E8.3 factors | D1, parallel | E8.2's draw is `hash_at(K_erosion, c, it)`: location-keyed, independent of evaluation order (D9) |
| E8.3 accumulation, E8.4 solve | D4 in G3's order | |
| E8.5 diffusion, E8.6 talus | D2, parallel | read one grid, write another; S4 summed in the 4-neighbour order; δ in the 8-neighbour order |
| E8.7 final talus | D3 of D2 steps, parallel | Jacobi: each step reads the previous grid and writes a new one; the active-set evaluation gives the same grid |
| E9 | D1, parallel | |
| E10.0 | D1, parallel | location-keyed noise only; may run any time after E4 |
| E10.1, E10.4 | D6, single-threaded | |
| E10.2 | D5, G2 | |
| E10.3 | D7 | sort by (volume desc, label asc); the cap loop runs in that order |

No floating point, no `<cmath>`, no `Rng`, no unordered containers, and no new math function: only P0-01 operations, `hypot`, `fade`, `isqrt_u64`, `hash_at`, `SQRT2` and `INV_SQRT2`. The determinism of the third version's steps was checked on its exploration build: one file hash and one layer hash per case for 512 seeds 0, 42, 7 and 1024 seed 1 at 1, 2 and 5 threads with GCC and Clang release, and a clean ThreadSanitizer run (`v16-determinism.txt`). Round five (cap 400, the same code path as 500) was checked the same way: 512 seeds 0 and 42 and 1024 seed 1 at 1, 2 and 5 threads, GCC and Clang release, one file hash and one elevation hash per case (`r5-determinism.txt`); not run under ThreadSanitizer, and E6.5 is single-threaded in the exploration. Windows/MSVC has not been run for either.

**Engineer's notes for the determinism auditor** (round four, kept by round five; each is part of the text above): the divide flood pops by (level, seq) with seq the global push counter (D6); every Dijkstra pops by the unique pair (distance, cell) and skips stale entries; box queries visit cells in index order and every argmin and argmax over one breaks ties to the smallest index; the divide-direction centroids use floor division of the summed offsets by their counts, so the direction vector has components of at most 2 × 37 and every product stays in `int64` (rounded centroids in `Fx` were tried first and dropped); overlapping pass zones are resolved by the minimum for P and by the largest bump, ties to the lower site index, for adj.

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
- **E6.5.** Units: 0 ≤ mh ≤ 8700 m × 2^16 < 2^30; |ter| < (4100 + 8700) m × 2^16 < 2^30 (|h0| ≤ 4100 m by E5's bounds); K, dh, level < 2^30; Lb and kb as E8's flood keys (< 2^46). Cell coordinates < 2^11, so d2 < 2^23; a box radius is at most 1250 cells (20000 m / 16) and every cross product is below 2 × 2^11 × 2^11 = 2^23. Scores: dmin < 2^23 times at most 1330, < 2^34. hash_at(…) >> 54 < 2^10; >> 32 < 2^32, × 2 < 2^33. Arc positions: an axis has at most N² cells, so arc < 7 × 2^22 < 2^25 fifths; fm(arc) < 2^25 × 16 / 5 m < 2^27 m, and mul(mul(grade, reach), 8/15) < 2^28 m, within `Fx`. Direction tests: the side test bounds each exit's arc by side_max ≤ 100000 m, 31250 fifths, so |v| < 6250 cells per component, |t| ≤ 74, |t'| ≤ 2 × 1250; 2 (v·t)² < 2^42, |v|²|t|² < 2^41, (v·t')² < 2^49; chord components < 2^14, chord2 < 2^29, arc5 ≤ 62500, 4 arc5² < 2^34, 225 chord2 < 2^37, 100 (chord·t')² < 2^53 and 33 chord2 |t'|² < 2^52, all in `int64`. m' ≤ mul(scale, ONE) + 2 raw ≤ 8700 m; |S|, |N| < 13000 m; U and L move by at most ε per axis cell, so |U|, |L| < 13000 m + 2^22 × 10 m < 2^26 m; |Δ| < 2^27 m and |adj| < 2^27 m. Bisection sums lo.raw + hi.raw ≤ 2^33. Dijkstra limits: reach5 ≤ 6250; halfwidth ≤ 5000 × (1 + 0.9) = 9500 m, hwmax ≤ 9500 × (1 + 0.9) = 18050 m and hwv ≤ 18050 m, so lim5 ≤ mul_div(18050 m, 5, 16) + 2 = 5643 fifths (the reached box: at most 5643 div 5 + 1 = 1129 cells from the axis); hr ≤ (9500 div 16) div 2 = 296 cells. Noise coordinate |mul(s_m, 1/1500)| < 2^27 / 1500 < 2^17.
- **E8.7.** As E8.6 with D_o^f ≤ 256 m and rate ≤ 1/8: |x| ≤ 2990 m, |δ| ≤ 8 × (1/8) × 2990 m.
- **E10.** lake_max_area_m2 × N² ≤ 10^9 × 2^22 < 2^52; lake_max_cells can exceed N² (and 2^32) for extreme tuning, so it is an `int64` and compared as one (the exploration code narrows it to `uint32`, section 12 item 1). volume ≤ 2^22 × 29990 < 2^37. cost ∈ [1, 17]; bump ≤ mul_div(10 × 2^32, 10 × 2^16, 2^32) < 2^23. base = k × 2^16 ≤ 29990 × 2^16 < 2^31. eps ≤ mul_div(10 × 2^16, 17 × 1.4143 × 2^32, 2^32) + 2^23 < 2^25; along any flood chain (≤ 2^22 cells) P < 2^31 + 2^22 × 2^25 < 2^48 (`checked_add`); output ≤ 29990.

## 8. Performance

Budget (P1-00 11): 2.2 / 8 / 35 s at 512 / 1024 / 2048 (512 raised from 2 s by the lead, 2026-11-17: passes and the final talus cost about 0.05 s there, measured 1.97–2.04 s, and the remaining savings were measured by the engineer at 2 to 15 ms, not worth the risk) for the stage on the 6-core development machine, release build; peak memory within P1-00's 200 MB / 600 MB / 2 GB for all of geography. **New in this version, a hard budget for the implementation: the stage's own peak memory** (peak RSS of `generate --until elevation`) **is at most 1.3 × that of 0.5.0** on the same machine: 0.5.0 measured 30 / 101 / 376 MB (`r4-timings.txt`, the v16 build), so about **40 / 132 / 490 MB**. Section 8.1 shows how the algorithm fits it.

Measured on the third version's exploration build (`linux-gcc-release`, 6 threads; `v16-timings.txt`, `worldgen-cli generate` including loading and writing the file, elevation the only stage):

| | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| `generate`, 6 threads | 1.87–1.92 s (3.7 s on a cold first run) | 6.84–6.87 s | 27.7 s |
| `generate`, 1 thread | | 18.7 s | |
| peak RSS | 32 MB | 110 MB | 385 MB |

Per step (spec writer's measurement of the library calls, 1024 seed 3, 6 threads): E1 to E3 1.37 s; E4 0.02 s; E5 to E7 1.00 s; E8 3.45 s (24 iterations; the G3 flood, the accumulation and the solve are single-threaded); E9 0.02 s; E10.0 and E10 about 0.3 s. At 2048 seed 0: 5.4, 0.1, 3.7, 14.6, 0.1 s and about 1.5 s. 512 is within its budget by about 5%, 1024 by 15%, 2048 by 20%.

The cost per cell: E2 evaluates 19 `noise2` octaves (warp 3 + 3, continent 4, coast 5, belt 2, segment 2); E5.2 about 20 per land cell plus 6 ridged octaves where the mask is positive; E10.0 6 per land cell. Memory: about 8 bytes per cell for each `int64` grid (c, nb, ns, g, h, hn, F, L, kf, P, cost, bump and the flood queue are the large ones; not all are live at once).

**Round five** (`r5-timings.txt`, `generate --until elevation`, 6 threads, GCC release, cap 400; the machine had a load of 2 to 5, so figures are approximate): 512 seed 0 1.96–1.97 s against 1.90–1.91 s for v16 on the same run (the budget was then 2 s: **at the limit**; now 2.2 s), 1024 7.10 s against 6.69 s, 2048 28.7 s against 27.8 s. The pass layout and the final talus cost about 0.06 s at 512, 0.4 s at 1024 and 0.9 s at 2048. Peak RSS of that build: 61 / 224 / 872 MB, **over the new budget**, because it keeps about fifteen whole-grid scratch arrays of E6.5 alive together (section 8.1).

Erosion is a grid method with an implicit tree solve rather than droplets: droplet erosion is sequential and order-sensitive and has no deterministic parallelism; the stream-power solve is O(N²) per iteration with a defined order and is stable for any step size.

If the stage is too slow, in this order:

1. **Without changing output:** a radix or bucket heap for the floods (pop order is defined by (key, seq), not by the structure; the shared G3 already uses a radix heap); a table for `isqrt_u64` of A < 2^16; the basin-parallel solve of P1-00 section 8 (each drainage basin's cells solved by one thread in `order`; basins write disjoint cells); computing E10.0 in the same pass as E5.2 (it reads the same detail noise); dropping the unused computations the exploration code still makes (E3's `s_ocean`, a chamfer from the ocean, the continent-field ocean depth).
2. **Tuning only:** `erosion_route_interval` 2 or 4, then fewer iterations with a larger `erosion_k`. Changes output; a shipped tuning change (PATCH bump).
3. **Algorithm change (spec amendment):** erode a 2:1 downsampled grid at 2048 and add the difference back bilinearly.

**Implementation** (branch `p1/01b-elevation-passes`, `88da7d3`; `linux-gcc-release`, 6 threads, quiet machine, `generate --until elevation`): 512 2.00–2.04 s and 28.6–29.5 MB, 1024 6.97–7.24 s and 94–97 MB, 2048 28.0–29.3 s and 354–357 MB. Memory is below 0.5.0's peaks (30 / 101 / 376 MB) and well inside the hard budget (40 / 132 / 490 MB), which stays. The 512 time is about at the old 2 s limit, inside the budget of 2.2 s. Of the savings below, the implementation applies (i), (ii), (iv) and (v); not applied: (iii), restricting the divide flood and the range search to the support's bounding box, and a bucket queue for the basin levels. The engineer measured these at 2 to 15 ms: the flood time is the priority queue over the support cells, not the whole-grid setup, and a bucket queue for the basin levels would need radix-heap scratch that the memory budget rules out.

**Where the pass cost at 512 can be saved** (all without changing output): (i) skip E6.5.1 and E6.5.3 to E6.5.8 entirely when no range has np_k > 0 (E6.5.2 first; at 512 many worlds have no range of the minimum length), and run the basin flood of E6.5.1 only when the first candidate reaches test (g); (ii) evaluate docean at a col as the closed form of E6.5.1 4 over a box of radius coast_clear div 5 cells instead of a whole-grid chamfer; (iii) restrict the divide flood and the range search to the bounding box of the support grown by one cell (exact: neither leaves the support and its seed ring); (iv) the active-set evaluation of E8.7; (v) the third version's own savings in item 1 above. The whole-grid passes (basin flood, docean chamfer, support flood) were most of the exploration's 0.06 to 0.1 s at 512.

### 8.1 Memory

The exploration's peak comes from keeping, together with c, nb, ns, h, unit, mask and profile, about fifteen whole-grid scratch arrays of E6.5 (flood level, parent, side and key, divide partner and pair height, range distance and source, factor-search distance and source, best bump, adjustment, labels, dA, a smoothed height, basin flags, docean), most of them `int64`. None of this is required:

| When | Needed per cell (bytes, with the types this spec's bounds allow) |
| --- | --- |
| after E5.2 | c and ns are dead (E5.1 and E5.2 are their last readers); keep nb (8, read by E6.5.2 and E6.5.3 only), h (8: h0 on land, ocean depth after E5.3), unit (8), mask (8), profile (8), O (1) |
| E6.5.1 | the basin flood: its level (8) and the flood's queue and visited flag, only until `basin` (1 bit or byte) is set; then bigbasin (1); docean is not stored (closed form at cols) |
| E6.5.2 | label (4: < 2^31 ranges), dA (4: < 2^25 fifths); the geodesic search scratch is per range |
| E6.5.3 | support and body (flags, 1 or 2 bits); the divide flood's parent (4, `uint32` cell) and side (1) are kept; its level (4, < 2^30 units) is needed only during the flood; partner (1: a 4-neighbour code) and dh (4) only for divide cells (a sparse list suffices); the range search's distance (4) and source (4) only until each divide cell's range is known; all over the support's bounding box |
| E6.5.4 to E6.5.6 | per candidate: two chains and an axis (lists); nothing per cell |
| E6.5.7, E6.5.8 | per site: the search's distance and source over the box of the axis grown by lim5 div 5 + 1 cells; P and adj (and best) only on reached cells, as a sorted sparse list or two grids |
| E7 | P and adj; afterwards nb, unit, mask, profile, P, adj and the layout are dead |
| E8.7 | the active set (4 per land cell at most) and its new values, or a second `int64` grid for a plain Jacobi step |

With these lifetimes the largest E6.5 working set is below about 80 bytes per cell (41 live from E5 plus at most about 40 of E6.5 at its widest, the divide search), which is less than E8's (h, hn, F, the flood level, parent and order, receivers, accumulation, the fill grids, about 85 to 90 bytes per cell), so the stage's peak stays where 0.5.0 has it, well inside 1.3 ×. An implementation that needs more must show the measurement and why.

**Implementation note: work the exploration does that this spec does not require.** None of the following is part of the algorithm, and no statement of this spec forces it: round three's smoothed mountain height and its box mean, the `near_basin` disc grid, the `notch`, `cap` and `site_of` grids, a stored whole-grid docean (E6.5.1 4 allows the closed form at cols), whole-grid scratch for the per-site factor search (only the box around the axis is reached), computing each crossing's body exits twice (E6.5.6 (d) defines e_A and e_B once and (e), (f) reuse them), and running E8.7 on one thread (it is a Jacobi step and may use G6 row blocks; the exploration's `final_talus` ignores its `threads` argument). The reference values of 10.4 do not depend on any of it.

## 9. Database rows written

One `layer` row: `name` = `elevation`, `width` = `height` = N, `format` = `i16`, codec 1, in one stage transaction; its id is the next layer id (1 in a normal world).

### 9.1 The `pass` table (schema version 2)

**Adopted by the lead** (proposals "Pass sites are stored in the world file"). The pass sites are derived data that history needs (where the natural ways over the ranges are, which of them are high cols worth improving, which are natural gaps) and that cannot be recovered from the stored layers: the col, its natural saddle height and the crossing come from pre-erosion grids (h0, unit, mask) that are not kept. The table is small (at most `pass_count_max` rows per range; 0 to 5 rows per world in the round-five sweeps), and it lets `report` print pass statistics (P1-00 10.3). It is a schema change (hard rule 7): `SCHEMA_VERSION` 1 → 2 with generator 0.6.0, `docs/specs/schema.md` (section 1 DDL, section 2 `schema_version`, changelog), and the proposals entry above. The planned `biome` table moves to version 3 and `region` with `region_border` to version 4 (P1-00 9.2). The layer does not depend on the table.

DDL (one statement, executed after `layer`'s, byte for byte):

```
CREATE TABLE pass (id INTEGER PRIMARY KEY, range_id INTEGER NOT NULL, kind INTEGER NOT NULL, capped INTEGER NOT NULL, col_x INTEGER NOT NULL, col_y INTEGER NOT NULL, saddle_dm INTEGER NOT NULL, col_dm INTEGER NOT NULL, path BLOB NOT NULL) STRICT
```

| Column | Value |
| --- | --- |
| `id` | 1 + the site index s of E6.5.5 (sites in the order they were found: ranges in label order, slots in order) |
| `range_id` | the range's label k (E6.5.2), 1 … ; ranges are not stored, so this only groups the sites of one range |
| `kind` | 1 = lowered pass, 0 = natural gap (E6.5.6 (i)) |
| `capped` | 1 if the cut cap bound (the site's final `capped` flag of E6.5.6 (h) or (o)), else 0; 0 for a natural gap |
| `col_x`, `col_y` | the col cell (the side-1 divide cell) |
| `saddle_dm` | the natural saddle height, dh of the col, in decimetres: mul_div(dh, 10, 2^16) (RHA; dh in units ≥ 0) |
| `col_dm` | the pre-erosion height of the col after E7, in decimetres: mul_div(h.raw, 10, 2^32) of E7's h at the col (RHA) |
| `path` | the crossing polyline `axis` of E6.5.6 (c), from the side-1 lowland end through the col and its partner to the side-2 lowland end: the cell indices i = yN + x as `u32` little-endian, 4 bytes per cell |

Rows are inserted in `id` order, in the elevation stage's transaction, after the `elevation` layer row; a world with no site has an empty table (the table always exists in schema 2). The final height of the col is the `elevation` layer's value there (after erosion; the cut in the layer is the difference from `saddle_dm`, approximately).

`saddle_dm` ≤ 1.28 × 10^5 and `col_dm` ≤ 29900 fit `INTEGER`; a path has at most N² cells.

## 10. Tests required

Library seams, as declared on the implementation branch `p1/01-elevation` (`worldgen/include/worldgen/stages/elevation.hpp`; all in `worldgen::stages`, `threads` defaults to 1). Tests use these; the engineer may add helpers but should not change these signatures without a spec note.

```cpp
ElevationTuning elevation_tuning_from(const data::ContentSet& content);   // and (const data::Record&)
// Derived integers (4.5)
std::int32_t  elevation_margin(Fx edge_margin_frac, std::int32_t n);
std::int64_t  island_min_cells(std::int64_t island_min_area_m2);
std::int64_t  lake_min_cells(std::int64_t lake_min_area_m2);
std::int64_t  lake_max_cells(std::int64_t lake_max_area_m2, std::int32_t lake_area_ref_n, std::int32_t n);
std::int64_t  lake_min_depth_dm(Fx lake_min_depth_m);
std::int64_t  fill_fine_slope(Fx fill_slope_dm);
Fx            belt_halfwidth(Fx belt_halfwidth_m, Fx belt_halfwidth_frac, std::int32_t n);
std::int64_t  mountain_floor_target(std::int64_t land_cells, Fx segment_on_share);
std::int64_t  lake_max_share_cells(std::int64_t land_cells, Fx lake_max_share);   // max(1, ...)
// E1 to E3
Fx elevation_map_fraction(std::int32_t k, std::int32_t n);  Fx elevation_metres(std::int32_t k);
struct ContinentCentre { Fx cx, cy; };
ContinentCentre continent_centre(std::uint64_t seed, Fx continent_center_jitter);
struct ElevationFields { std::int32_t n; std::vector<std::int64_t> c, nb, ns; ContinentCentre centre;
                         Fx c_sea, g_med, hw, w_b, s_land; };
ElevationFields elevation_fields(std::uint64_t seed, std::int32_t n, const ElevationTuning&, std::uint32_t threads);
std::vector<std::int64_t> belt_gradient(geo::Grid, std::span<const std::int64_t> nb, std::uint32_t threads);
Fx belt_width(Fx g_med, Fx hw);
// E4
std::vector<std::uint8_t> elevation_ocean_from_provisional(geo::Grid, std::span<const std::uint8_t> p,
                                                           std::int64_t island_min);
std::vector<std::uint8_t> elevation_classify(const ElevationFields&, const ElevationTuning&);
// E5, E5.1
struct LandProfile { Fx tl, gm, co; };
LandProfile land_profile(Fx s, Fx s_land, Fx mountain_coast_t);
Fx ratio_capped(Fx a, Fx b);                                  // rc
Fx belt_profile(Fx nb, Fx offset, Fx w_b);                    // b_o
Fx segment_factor(Fx ns, Fx bias, Fx segment_contrast);       // sg_beta
Fx mountain_mask(Fx nb, Fx ns, Fx gm, Fx offset, Fx bias, Fx w_b, Fx segment_contrast);
struct FloorCell { Fx nb, ns, gm; };                          // one land cell
struct MountainFloor { std::int64_t target; bool triggered; Fx offset, bias;
                       std::vector<Fx> bisection; };          // every mid, in order; empty if none ran
MountainFloor mountain_floor(std::span<const FloorCell> land_cells, Fx w_b, const ElevationTuning&);
// E5 to E7 (fourth version: mask, profile and the pass layout added)
struct ElevationHeights { std::vector<std::int64_t> h /* E7's h, every cell */, unit, mask, profile;
                          MountainFloor floor; std::int64_t land_cells; Fx q, qc, scale; PassLayout passes; };
// h0 is not exposed: tests that need it compute E5.2 themselves.
ElevationHeights elevation_heights(std::uint64_t seed, const ElevationFields&, std::span<const std::uint8_t> ocean,
                                   const ElevationTuning&, std::uint32_t threads);
std::vector<std::int64_t> chamfer_distance(geo::Grid, std::span<const std::uint8_t> source);
Fx ocean_depth(std::int64_t dl, Fx ocean_shelf_m, Fx ocean_depth_m);
struct MountainScale { Fx q, qc, scale; };
MountainScale mountain_scale(std::span<const std::int64_t> land_units, const ElevationTuning&);
// E6.5 (new; any W x H grid with an ocean cell; the stage calls it with the E5/E6 grids)
struct PassInputs { geo::Grid grid; std::span<const std::int64_t> h0, unit, mask, profile, nb;
                    std::span<const std::uint8_t> ocean; Fx scale, offset /* o* */, w_b; };
enum class PassReject : std::uint8_t { coast, barrier, sea, side, along_divide, along_crest, chord,
                                       tarn_crossing, col_tarn, summit_top, zone_p90, feet, shoulders };
struct PassRange { std::int32_t label; geo::CellIndex s0, end_a, end_b, top; std::int64_t length, p90;
                   std::int32_t slots; std::vector<geo::CellIndex> divide, candidates; };   // divide = D_k
struct PassSite { std::int32_t range; geo::CellIndex col, partner; bool natural_gap, capped;
                  std::vector<geo::CellIndex> axis; std::int32_t col_index; std::int64_t dh, maxm;
                  Fx saddle, halfwidth, depth; };
struct PassTry { geo::CellIndex col; std::int32_t range, slot; std::optional<PassReject> reject; };  // in order
struct PassLayout { std::vector<PassRange> ranges; std::vector<PassSite> sites; std::vector<PassTry> tries;
                    std::vector<std::int64_t> factor /* P raw, ONE where none */, adj /* raw, 0 where none */; };
PassLayout pass_layout(std::uint64_t seed, const PassInputs&, const ElevationTuning&);
struct DivideFlood { std::vector<geo::CellIndex> parent; std::vector<std::uint8_t> side; };  // E6.5.3 step 2
DivideFlood divide_flood(geo::Grid, std::span<const std::uint8_t> support, std::span<const std::int64_t> key,
                         std::span<const std::int64_t> nb, Fx offset);
Fx pass_lowered(Fx scale, Fx mask, Fx profile, Fx p, Fx foothill_fraction, Fx texture_keep);   // m'(c, p)
struct PassDepth { Fx lo; bool capped; };                     // E6.5.6 (o) on given axis values
PassDepth pass_depth(std::span<const std::int64_t> mask, std::span<const std::int64_t> profile, Fx scale,
                     Fx saddle, bool capped, std::int64_t maxm, const ElevationTuning&);
Fx pass_bump(Fx dist, Fx dend, Fx halfwidth, Fx width_var, Fx nz);   // 0 if dist >= hwv
std::vector<std::int64_t> pass_drainage(std::span<const std::int64_t> s, std::span<const std::int64_t> nat,
                                        std::int32_t col_index, Fx eps);        // Delta per axis cell, raw
// E8
struct ErosionParams { std::int32_t iterations, route_interval; Fx k, diffusion, floor,
                       talus_drop_o, talus_drop_d, talus_rate; std::uint64_t key; };
ErosionParams erosion_params(const ElevationTuning&, std::uint64_t seed);   // key = K_erosion
void erosion_random_receivers(geo::Grid, std::span<const std::int64_t> level, std::span<const std::uint8_t> ocean,
                              std::uint64_t key, std::int32_t it, std::span<geo::Direction> rcv, std::uint32_t threads);
void erosion_factors(geo::Grid, std::span<const std::uint32_t> accum, std::span<const geo::Direction> rcv,
                     std::span<const std::uint8_t> ocean, Fx k, std::span<std::int64_t> f, std::uint32_t threads);
void erosion_solve(geo::Grid, std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean,
                   std::span<const geo::Direction> rcv, std::span<const std::int64_t> f,
                   std::span<const geo::CellIndex> order, Fx floor, std::span<std::int64_t> hn);
void erosion_diffuse(geo::Grid, std::span<const std::int64_t> hn, std::span<const std::uint8_t> ocean,
                     Fx diffusion, Fx floor, std::span<std::int64_t> h, std::uint32_t threads);
void erosion_talus(geo::Grid, std::span<const std::int64_t> h, std::span<const std::uint8_t> ocean,
                   Fx drop_o, Fx drop_d, Fx rate, Fx floor, std::span<std::int64_t> t, std::uint32_t threads);
void erode(geo::Grid, std::span<std::int64_t> h, std::span<const std::uint8_t> ocean, const ErosionParams&,
           std::uint32_t threads);
void final_talus(geo::Grid, std::span<std::int64_t> h, std::span<const std::uint8_t> ocean, Fx drop_o, Fx drop_d,
                 Fx rate, Fx floor, std::int32_t iterations, std::uint32_t threads);   // E8.7 (new)
// E9
std::int16_t quantize_elevation_cell(std::int64_t h_raw, bool ocean);
std::vector<std::int16_t> quantize_elevation(geo::Grid, std::span<const std::int64_t> h,
                                             std::span<const std::uint8_t> ocean, std::uint32_t threads);
// E10
Fx fill_cost(Fx nz, Fx fill_cost_amp);  std::int64_t fill_bump(Fx dn, Fx fill_detail_m);
std::int64_t fill_slope_eps(std::int64_t fine_slope, Fx cost, bool diagonal);
struct FillGrids { std::vector<std::int64_t> cost, bump; };
FillGrids fill_grids(std::uint64_t seed, std::int32_t n, std::span<const std::uint8_t> ocean,
                     const ElevationTuning&, std::uint32_t threads);
struct DepressionComponent { std::uint32_t label; std::int64_t size, depth, volume; };
struct ConditionParams { std::int64_t lake_min_cells, lake_max_cells, lake_min_depth_dm,
                         max_share_cells /* >= 1 */, fine_slope; };
std::vector<std::uint32_t> select_lakes(std::span<const DepressionComponent>, const ConditionParams&);  // E10.3, kept labels in selection order
struct Conditioned { std::vector<std::int16_t> e; std::vector<DepressionComponent> components;
                     std::vector<std::uint32_t> kept; };
Conditioned condition_depressions(geo::Grid, std::span<const std::int16_t> e, std::span<const std::uint8_t> ocean,
                                  std::span<const std::int64_t> cost, std::span<const std::int64_t> bump,
                                  const ConditionParams&);                   // E10.1 to E10.5, any W x H grid
// Whole stage
std::vector<std::int16_t> elevation_grid(std::uint64_t seed, std::int32_t n, const ElevationTuning&,
                                         std::uint32_t threads);
struct ElevationResult { std::vector<std::int16_t> e; std::vector<PassSite> sites;
                         std::vector<std::int64_t> h7; };   // h7[s] = E7's h (raw) at the col of sites[s]; whole grid: elevation_heights().h
ElevationResult elevation_run(std::uint64_t seed, std::int32_t n, const ElevationTuning&, std::uint32_t threads);  // the table's source
```

Seam contracts: `pass_layout` lists every range (E6.5.2) even when no range has a slot; `PassRange.divide` is empty when E6.5.3 did not run (no range with a slot), and `candidates` is filled only for ranges with slots and a non-empty divide list (empty otherwise); a natural gap has `depth` = 0 and `capped` = false (its saddle, halfwidth and maxm are those of E6.5.6 (h)); `DivideFlood.side` is 0 exactly on cells not reached (non-support cells that are not seeds, and unreached support cells), and `parent` is unspecified there.

The exploration's equivalents are `elevation_pass_layout` (whose `reject` counters, `divide` flags and `cand_cells` list the same facts in another form) and `final_talus(grid, h, ocean, tuning, threads)`; these names and shapes are this spec's choice, since the exploration's seams are diagnostics. The E6.5 seams take the grids as `Fx` raw values; `pass_layout` computes the basins, docean, ranges and divide itself.

E6.5 is testable through `pass_layout` (whose `tries` give every tried candidate with its slot and its failing rule, or none for a site) and its parts; E8.7 through `final_talus`. E8.2 is testable through `erosion_random_receivers` (given L, ocean, key and it) and E10.2/E10.3 through `condition_depressions(...).components` and `select_lakes`; the worked examples of 10.1 map onto these seams (F1 and F2 onto `mountain_floor`, whose `bisection` list gives F1's mid sequence).

### 10.1 Unit tests with worked examples

Every figure below was computed by the spec writer's exact-integer reference of this spec; the examples marked "checked" were also run through the exploration library and agree (the floor has no library seam there; F1 and F2 come from the reference alone, whose floor agrees with the library's output on every triggering world of 10.4).

**Derived integers.** margin M for N = 512, 1024, 2048 with "0.03" (raw 128849019) is 15, 30, 61; with "0" it is 1. I_min for 50000 m² is 196; for 0, 0; for 256, 1; for 257, 2. `land_floor_m` "0.1" is raw 429496730. lake_max_cells with the shipped values at N = 128, 256, 512, 1024, 2048: 184, 733, 2930, 11719, 46875 (at 2048: a = 12000000, (12000000 + 255) div 256 = 46875). lake_min_cells 391; lake_min_depth_dm 20; fine_slope 65536; D_o raw 206158430208, D_d raw 291552048000 (67.882 m). target and max_share_cells for L_O = 6517: 521 and 196; for L_O = 26261: 2101 and 788.

**Coordinates.** N = 1024: U at x = 0 is raw 2097152 (1/2048), at x = 1023 raw 4292870144 (2047/2048); N = 512: raw 4194304 and 4290772992; N = 16: U at x = 3 is `from_ratio(7, 32)` = raw 939524096. X at x = 2047 is `from_int(32760)`.

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

**Tuning validation.** Named check 1 fails on `edge_radial_start` "1.0", `edge_radial_end` "1.005" and on end below start; check 2 on `mountain_cover_rank` "0.999" with `mountain_peak_rank` "0.998"; check 3 on `lake_min_area_m2` 200000 with `lake_max_area_m2` 100000; a float literal in any field is a P0-03 error; a missing field is an error; each bound of 4.2 rejects a value one step outside it (including `edge_radial_start` "1.991", `lake_area_ref_n` 15 and `lake_max_share` "0.0009"). Each named-check failure is a load error and no world is written. New in this version: check 4 fails on `pass_region_mask` "0.3" with `pass_core_mask` "0.25", check 5 on `pass_min_col_m` 700 with `pass_saddle_max_m` 600; the raised lower bounds reject `pass_size_ref_n` 15, `pass_basin_depth_m` "0.9", `pass_max_cut_m` "0.9", `pass_walk_grade` "0.009" and `pass_drain_step_m` "0.009"; `final_talus_rate` "0.1251" and `pass_count_max` 4 are rejected.

The following examples are new in this version. Those marked "checked" were also run through the exploration library (`elevation_pass_layout` on the given grids, by a small driver) and agree; the others come from the spec writer's exact-integer reference of this text, which reproduces the exploration build's grids on every world of 10.4.

**Pass derived integers.** With the shipped values: coast_clear 27 / 109 / 219 / 219 fifths at N = 128 / 512 / 1024 / 2048; min_len 117 / 468 / 937 / 937; side_max 375 / 1500 / 3000 / 3000 m; spacing 937 (small fixture 312); reach5 313 (mul_div rounds 312.5 up; small fixture 125); win, rise_b, spacing_b, tarn_b = 25, 93, 156, 10 (small fixture 12, 37, 50, 4); rise_u 9830400 (small fixture 4915200), body_u 19660800; from_ratio(8, 15) raw 2290649225; from_ratio(9, 10) raw 3865470566; from_ratio(1, 1500) raw 2863312; walk-grade limit mul_int(0.55, 48) raw 113387136624 (26.4 m); D_o^f raw 343597383680, D_d^f raw 485920080000.

**T1. Divide, candidate and crossing tests on a small grid** (checked). A 17 × 9 grid, ocean on the border (x = 0, x = 16, y = 0, y = 8), land inside. Support: x = 2 … 14, y = 2 … 6, with mask = ONE, profile = ONE, h0 = 0, scale = from_int(1024) and unit = K × 2^22 (so mh = K m exactly, K below); every other cell has mask = unit = profile = 0 and h0 = 0. nb = ONE on rows y ≤ 3 and −ONE on rows y ≥ 4 (every cell), o* = 0, w_b = from_int(4) (so crest_lim = mul(0.3, 4) = 1.2 ≥ |nb| and every support cell is a crest cell: one range). Shipped tuning except `pass_saddle_window_m` 32 (win 2), `pass_saddle_rise_reach_m` 96 (rise_b 6), `pass_size_ref_n` 16 (num = den = 16), `pass_min_length_m` 0, `pass_coast_clear_m` 0 and `pass_halfwidth_m` 20; seed 0 (K_pass = 0x964f8d9d472fb93b). K in metres, rows y = 2 … 6, columns x = 2 … 14:

```
y2:  100 100 100 100 100 100 100 100 100 100 100 100 100
y3:  440 400 420 440 400 420 440 400 420 440 400 420 440      (400 + 20 × (x mod 3))
y4:  800 850 900 950 900 700 550 650 900 950 900 850 800
y5:  380 400 380 400 380 400 380 400 380 400 380 400 380      (380 + 20 × (x mod 2))
y6:  100 100 100 100 100 100 100 100 100 100 100 100 100
```

Range 1: s0 = (2, 2) (index 36), A = (14, 6) (index 116), B = (2, 2), len 68 fifths, np = min(3, 1 + 68 div 937) = 1, top = (5, 4) (950 m; ties go to the smaller index, (5, 4) before (11, 4)), p90 = 850 m. The divide flood gives side 2 to rows y ≤ 3 and side 1 to rows y ≥ 5; on row 4 cells (2, 4) and (14, 4) get side 2 (reached from the side-2 ring cells (1, 3) and (15, 3) diagonally before any side-1 path) and (3, 4) … (13, 4) side 1. Divide cells (with partner and dh in m): (3, 4)–(3, 3) 850, (4, 4)–(4, 3) 900, (5, 4)–(5, 3) 950, (6, 4)–(6, 3) 900, (7, 4)–(7, 3) 700, (8, 4)–(8, 3) 550, (9, 4)–(9, 3) 650, (10, 4)–(10, 3) 900, (11, 4)–(11, 3) 950, (12, 4)–(12, 3) 900, (13, 4)–(13, 3) 850, (2, 5)–(2, 4) 800, (14, 5)–(14, 4) 800; all thirteen reach a crest cell (they are crest cells), so D_1 holds them in index order. The only candidate is (8, 4): within B(·, 2) no divide cell has a smaller (dh, index); chainA = (8, 4), (8, 5), (7, 6), (6, 7) and chainB = (8, 3), (7, 2), (6, 1), so pa = (6, 7) and pb = (6, 1) (the chains are shorter than 21) and a = (0, −6); cr = 6 dx, so the left side is east: lmax = 950 (at (11, 4)) and rmax = 950 (at (5, 4)), both ≥ 550 + 150. (7, 4) is not a candidate ((8, 4) is lower within 2 cells); (9, 4) likewise.

Its crossing tests: (a) docean 20 ≥ coast_clear 0; (b) mul(ONE, 1024) = 1024 m ≥ 600; (c) axis (6, 7), (7, 6), (8, 5), (8, 4), (8, 3), (7, 2), (6, 1), ci = 3, arc 0, 7, 14, 19, 24, 31, 38, both ends land; reach_A = reach_B = fm(19) = raw 261134011597 (60.8 m); (d) the body (mh ≥ 300 m) ends at (7, 6) and (7, 2): e_A = (7, 6), e_B = (7, 2), body5_A = body5_B = 12 (38.4 m) ≤ side_max 3000 m; (e) 1: in B(c, 37) the divide cells east of the line give (sx, sy, n) = (21, 1, 6), west (−21, 1, 6), so t = (fdiv(−21, 6) − fdiv(21, 6), fdiv(1, 6) − fdiv(1, 6)) = (−4 − 3, 0) = (−7, 0); for v = (−1, 2) and (−1, −2): (v·t)² = 49, 2 × 49 = 98 ≤ 5 × 49 = 245: across; (e) 2: in B(c, 6) cmin = (13, 6) (smallest dA; A itself is outside the box), cmax = (2, 4), t' = (−11, −2); v·t' = 7 and 15, 2 × 49 = 98 and 2 × 225 = 450, both ≤ 5 × 125 = 625; (f) chord (0, −4), chord2 16, arc5 24: 4 × 576 = 2304 ≤ 3600; chord·t' = 8: 100 × 64 = 6400 ≤ 33 × 16 × 125 = 66000; (g) no basin; (h) maxm = 550 m (36044800 units), vj raw 3576141052, wj raw 879522112, grade raw 4831028650, saddle = clamp(…, 150, 600) = 150 m, M − 500 < 150 so not capped, halfwidth raw 91176478600 (21.23 m); (i) M = 550 > 150; (j) no basin within tarn_b 10; (k) hr = (21 div 16) div 2 = 0: no axis cell is a top; (l) zmax over the cells at axis positions 0 and 4 = 440 m ≤ p90 850 m; (m) (0 + 150) − 0 ≥ 150; (n) need = max(600, 300) m = 39321600 units, but no divide cell is 9 or more cells from the col: hl = hr' = −1, **fail: shoulders**. No site.

Variants of T1, each changing one value (checked): `pass_coast_clear_m` 100 → coast_clear = mul_div(100 m, 80, 256) = 31 > 20: **fail (a)**; `pass_barrier_m` 1100: **fail (b)**; `pass_side_max_m` 30 → side_max 30 m < 38.4 m: **fail (d)**; `pass_min_col_m` = `pass_saddle_max_m` = 600 → saddle 600 m ≥ M = 550 m: **natural gap (i)**, site 0 with kind 0 and the axis above; `pass_halfwidth_m` 450 → hr = 14 and (5, 4) is within 14 cells of the axis: **fail (k)**; h0 = 10 m at (6, 7) → (0 + 150) − 10 < 150: **fail (m)**.

**T2. The direction rules on given vectors** (arithmetic). (e) 1 with t = (−7, 0) and an exit at v = (−3, 1): v·t = 21, 2 × 441 = 882 > 10 × 49 = 490: **fail** (the side runs within 45° of the divide). (e) 2 with t' = (−11, −2) and v = (−4, −1): v·t' = 46, 2 × 2116 = 4232 > 17 × 125 = 2125: **fail**. (f) arc5 = 40 with chord (0, −4): 4 × 1600 = 6400 > 225 × 16 = 3600: **fail** (arc more than 1.5 × chord); chord (−3, −4) with t' = (−11, −2): chord·t' = 41, 100 × 1681 = 168100 > 33 × 25 × 125 = 103125: **fail** (chord more than 35° off the perpendicular). fdiv(−21, 6) = −4 and fdiv(21, 6) = 3 (floor division, not truncation).

**T3. Depth** (E6.5.6 (o); `pass_depth` seam). One axis of 7 cells with mask = ONE, so unit = profile and m'(c, p) = mul(scale, mul(p, 0.15) + mul(max(p, 0.25), profile − 0.15)); shipped tuning (walk limit 26.4 m, cap 500 m).

| Case | scale; profiles | M; saddle at (h) | walk-grade bisection | cap raise | result |
| --- | --- | --- | --- | --- | --- |
| A (walk grade binds, cap not) | 1000; 0.6, 0.62, 0.64, 0.65, 0.64, 0.62, 0.6 | 650 m (maxm 42598399 units; M = 649.99998 m); 600 m, uncapped | lo = raw 2267742738 (0.528): the 3-cell rise 0.65 − 0.60 of the profile is p × 50 m ≤ 26.4 m | top(lo) = raw 1474032779000 (343.2 m) ≥ M − 500: none | not capped, depth raw 2027224558; m' 316.8, 327.36, 337.92, 343.2, 337.92, 327.36, 316.8 m |
| B (walk grade asks too much; the cap raises) | 1000; 0.15, 0.4, 0.7, 0.85, 0.75, 0.45, 0.15 | 850 m; 600 m, uncapped | nothing fits (at p = 0 the relief kept at 0.25 still rises 175 m over 3 cells): lo = 0 | top(0) < 350 m: 32 steps on [0, ONE] give lo = raw 1768515945 (0.41176), top = raw 1503238554000 (350.0 m) | capped, depth raw 2526451351; m' 61.765, 164.706, 288.235, 350.0, 308.824, 185.294, 61.765 m |
| C (capped at (h)) | 1400; 0.5, 0.9, 1, 1, 0.95, 0.7, 0.4 | 1400 m; M − 500 = 900 m > 600, so saddle 900 m, capped (no walk clamp) | lo = raw 2761050404, the largest p with every m' ≤ 900 m | top(lo) is 1 raw step below 900 m by rounding, so the raise runs and gives lo = raw 2761050405, top raw 3865470567000 (900.00000014 m) | capped, depth raw 1533916891 |

Case C shows that the raise is decided on exact values: it moves p by 1 raw here.

**T4. Bump and factor** (E6.5.7; `pass_bump` seam). Seed 0, site s = 0 (nz from `noise2(substream(K_pass, 3), x, from_int(2))`, substream key 0x262b01aeab682941), halfwidth = 450 m, depth "0.4" (raw 1717986918), arc[ci] = 100, arc[last] = 250 fifths; hwmax raw 2609192632500 (607.5 m), lim5 = 192.

| d (fifths); arc[a] | s_m; dend | x = mul(s_m, 1/1500); nz | hwv | fade(1 − dist/hwv); taper | bump | P |
| --- | --- | --- | --- | --- | --- | --- |
| 0; 100 | 0; 320 m | 0; 0 (a lattice point) | 450 m | ONE; ONE | ONE | raw 2576980378 (0.6) |
| 35 (112 m); 150 | 160 m; 320 m | raw 458129920; raw −413989064 | raw 1867532005800 (434.82 m) | raw 3815349666; ONE | raw 3815349666 | raw 2768827430 |
| 100 (320 m); 150 | 160 m; 320 m | as above | as above | raw 510662794; ONE | raw 510662794 | raw 4090702178 |
| 60 (192 m); 40 | −192 m; 128 m | raw −549755904; raw 476092256 | raw 2007719813700 (467.46 m) | raw 2851238310; raw 1565340033 | raw 1039159827 | raw 3879303365 |
| 0; 240 | 448 m; 32 m | raw 1282763776; raw −589897286 | raw 1839826460700 | ONE; raw 44140840 | raw 44140840 | raw 4277310960 |

**T5. Floor drainage** (E6.5.8; `pass_drainage` seam). Nine axis cells, ci = 4, ε = 0.1 m (raw 429496730); N = 100, 300, 600, 900, 1000, 950, 900, 800, 760 m and S = 100, 280, 520, 640, 600, 610, 560, 380, 760 m (whole metres, `from_int`). U = 100, 280, 520, 599.9, 600, 599.9, 560, 380, 379.9; L = 100, 100.1, 100.2, 100.3, 100.4 on side A and 760.3, 760.2, 760.1, 760 on side B; Δ = 0, 0, 0, −40.1, 0, +150.3, +200.2, +380.1, 0 m (raw 0, 0, 0, −172228188570, 0, 645533584590, 859852452660, 1632517069210, 0). Cell 3 is a dam above the col on side A, cut to just below it; on side B the high foot (760 m, above the lowered col) makes the line from the foot bind, so cells 5 to 7 are lowered less than S asks, never above N. At bump 0.5 cell 5 gets adj = raw −322766792295 and cell 7 raw −816258534605 (negative adj raises: E7 subtracts it).

**T6. One final-talus step** (E8.7; `final_talus` with 1 iteration). A 4 × 3 test grid, column x = 0 ocean (h = −5 m), land in metres: rows `300 310 320`, `200 400 420`, `150 380 410` (x = 1 … 3). Shipped D_o^f = 80 m, D_d^f = 113.137 m, rate 1/16, floor 0.1 m. After one step (raw): `1173905047560 1334124216320 1379758243840`, `835676373000 1646351450120 1798517555200`, `692563476480 1573873877000 1760936591360` (273.3211, 310.625, 321.25 / 194.5711, 383.3211, 418.75 / 161.25, 366.4461, 410.0 m). For example (1, 0): the ocean to the W (300 − 80) and SW (300 − 113.137) and (1, 1) to the S (100 − 80) give δ = −(13.75 + 11.6789 + 1.25) m. After the full 100 steps: `97.018 190.66 278.227`, `99.632 195.395 284.157`, `100.483 196.936 286.085` m (rounded): the face is now at about 80 m per cell.

**T7. Pass rows** (section 9.1; reference, small-map fixture). N = 128 seed 8: id 1, range_id 1, kind 1, capped 1, col (72, 94), saddle_dm 6878, col_dm 5151, path 16 cells (SHA-256 of the `u32` blob 22011adaf46efab7a64646e862e9fc7b14dee30df9af79259faa3e6f01068a4f); id 2, range_id 1, kind 1, capped 1, col (69, 35), saddle_dm 8149, col_dm 7342, path 10 cells (b464f12091144654c74871fe94fabf49e749053c2a21e58e865c39cebaf70d27). The final layer has 4775 and 6333 dm at the two cols. N = 128 seed 31: one row, range_id 2, kind 1, capped 0, col (84, 67), saddle_dm 6243, col_dm 2243 (depth ONE: lo = 0, uncapped), path 21 cells (ceb9bca4970eef67433bf47621586bf43373bf1c35fafe598c3131165454e571); final 2223 dm. Their `report` pass lines (P1-00 10.3): seed 8 `passes = 2`, `natural_gaps = 0`, `passes_capped = 2`, `pass_col_m = 477.5 633.3 633.3`, `passes_walkable = 2`; seed 31 `passes = 1`, `natural_gaps = 0`, `passes_capped = 0`, `pass_col_m = 222.3 222.3 222.3`, `passes_walkable = 1` (on a 2 km map the 37-cell feet cover much of each side, so both are trivially walkable).

### 10.2 Property tests

Over seeds 0 to 63 at N = 128 with both fixtures (4.4), and seeds 0 to 3 at N = 512 with the shipped fixture, by code independent of the stage (its own components, floods and flood levels; a Dijkstra-style minimax search for flood levels is acceptable):

- Invariants 1 to 15 of section 6 (10 and 11 against the third version's reference, see 10.4.2: with `pass_count_max` = 0 and `final_talus_iterations` = 0 the layer must equal it; with the shipped values O must equal it).
- E4 alone: O is 4-connected, contains the margin, and every land 8-component has at least I_min cells; every P-cell not in O is land; no non-P cell is ocean unless it belongs to a sunk component.
- E3 values equal those found by sorting a copy.
- E5.1: for seeds where the floor triggers (with the shipped values at N = 128, among seeds 0 to 15: seeds 9, 10, 12 and 13; at seed 13 the chosen offset is the last one tried, o* = −4d = −2 w_b), the seam's (o*, β*) equals the test's own implementation of the search; for the others it is (0, `segment_bias`).
- E6.5, against the test's own implementation from this text (its own divide flood, Dijkstras by (distance, cell), box queries, candidate choice and crossing tests): the same ranges (s0, A, B, length, slots, top, p90), the same D_k and candidates, the same tried candidates in order with the same failing rule (`tries`), the same sites (col, partner, axis, ci, kind, capped, saddle, halfwidth, depth), and the same P and adj grids. For every site: the col is a divide cell with dh as defined; its crossing passes every rule of E6.5.6 before the one that decided it; every lowered pass satisfies invariant 14; every pair of cols is at least spacing_b apart.
- **Rule coverage.** Over the small-map fixture at N = 128, seeds 0 to 63, the tests together see at least one failure of each of the rules (a), (b), (d), (e), (f), (g), (j), (k), (l), (m), (n), at least one capped and one uncapped lowered pass, and a range with two sites (the seeds of 4.4); rules (c) and (i) are covered by T1's variants or an equivalent constructed grid. A suite that sees no failure of some rule does not meet this requirement.
- E8.7 alone, on any input meeting the contract: the largest land height does not increase; every land cell stays ≥ floor; ocean cells are unchanged; with the active set and with full Jacobi steps the result is the same (if the implementation uses an active set, the test compares it with a plain Jacobi loop).
- E8 alone, on any input meeting the contract of E7: no land cell is raised by the solve; every land cell after the loop is ≥ floor; ocean cells are unchanged; the largest land height does not increase; every random receiver points to a neighbour with strictly smaller L.
- E10 alone, on the E9 grids above and on 200 random fixtures (random keys in [1, 200] on a 32 × 32 interior with an ocean border, test-chosen thresholds, cost in [ONE, 4 ONE] and bump in [0, 2^20], all drawn from `Rng(substream(stream_key(0, "test.noise"), 1))`, a fixed key derived from the registered P0 test stream, so no new stream name is needed): e' = e on ocean and kept cells; e' ≥ e on land; the depression components of e' are exactly the kept components (so no non-kept land cell is depressed); kept cells ≤ max_share_cells; and for every non-kept land cell there is an 8-path to the ocean on which the surface W' never increases, where W' = e' on non-kept land, W' = L'(c) (the cell's flood level on e', its lake level) on kept-lake cells and W' = 0 on ocean. (The path through a kept lake is monotone only at the lake level, not on the lake bed, which is lower than the slope draining into it; section 6.3 proves exactly this.)

### 10.3 Determinism tests

- Threads (D10): `elevation_fields`, `erode`, `final_talus` and `elevation_grid` for seeds 0 and 42 at N = 128 and 512 with 1, 2 and 5 threads give identical grids; at N = 128 also with the small-map fixture seeds 8 and 31 (lowered passes, so E7 reads P and adj in every row block).
- Queue independence (E6.5): the divide flood and the Dijkstras computed with two different priority-queue structures in the test (a binary heap and an ordered set) give the same parents, sides, distances and sources on the N = 128 fixtures.
- Repeat: two runs in one process give identical grids (no hidden state).
- Map sizes are independent worlds: the layer for seed 0 at 512 does not depend on whether a 1024 world was generated first in the same process.

### 10.4 Reference values

**The third version's values (0.5.0) no longer hold for the shipped tuning**; they are kept in 10.4.2 as the reference for the neutral settings, which must still reproduce them.

#### 10.4.1 Fourth version (0.6.0)

Obtained by the spec writer from the round-five exploration build (`~/.cache/the-unmapped/build/agent-a7b1ce7cc613be009/linux-gcc-release/worldgen/libworldgen.a`, branch `p1/01b-elevation-passes-explore` at `3717dce`, whose round-five path is that of `0991d74`) with a small driver that calls `elevation_fields`, `elevation_classify`, `elevation_heights` (E5 to E7 with the pass layout), `erode`, `final_talus`, `quantize_elevation` and `elevation_grid`, with data files whose values are those of 4.4 (shipped: the round-five data `bin/data-R5C500`, which also sets fields the round-five path does not read; small: the third version's small fixture plus the pass lines of 4.4). Independently, the spec writer's exact-integer reference of this text (an extension of `p1_elevation_reference.py`) reproduced the factor P, adj, h7, h8, h8t and the layer bit for bit for N = 128 small-map fixture seeds 0 to 63 and N = 256 shipped seeds 0, 1, 10, 14 and 18. Hashes as in 10.4.2; in addition P and adj are hashed as `int64` grids (P = ONE and adj = 0 where no pass reaches) and **h8t** is the grid after E8.7 (h8 is before it).

| Case | Grid hashes |
| --- | --- |
| Shipped, N = 64, seed 0 (no site) | c, nb, ns, O, h7, h8 as in 10.4.2 (unchanged); P 87d681296dff508e8132727aaa9b80254d14eda26d33ddd76721444876a5a7f0; adj c35020473aed1b4642cd726cad727b63fff2824ad68cedd7ffb73c7cbd890479; h8t 6408c155b6771eaa76b4453844f8a7b6f0f59ef30d090a0b6fb70da6d86e457e; e' 9c6c21460f25fe03f5928e707cb4404b21e66d5547657946f96ae8c2530afcff; layer 075e2af60c69e53ea2fc8e2f3e2d1fe0f1764203cfa81f9ea32f37049ffdb9bd |
| Shipped, N = 128, seed 0 (no site; one candidate fails the barrier) | c, O, h7, h8 as in 10.4.2; nb a61dff055900dccac737af967ce454cf12821b4f152948115f299792b46f82ba; ns f6f56213c65f5e25aefcc9b17c6582aa150d2a47961b383e4596b625a1276923; P 10eafb1e477739919c48e8fa320ac354548dd19a2fbc7cc7b8a2b3be246eac81; adj fa43239bcee7b97ca62f007cc68487560a39e19f74f3dde7486db3f98df8e471; h8t d925e3ab848ad8c6ffb3e6fbc486ac198f542cf6e20f4e5b53249f19fe584c4b; e' 6be551a27cd94ccc5115a2d14c245f4e197979c83f1859f87f3ad618a3c304c9; layer e61a87fb6d0151e49cb56de299669d600aca12d4c42652876cb0c5742d0ea812 |
| Shipped, N = 256, seed 0 (one capped pass: col (106, 101), axis 31 cells, ci 17) | c c3fa7a799ab0a9ec6aa3c44de39958b7312ca468b3d9697b903a0954297d39a1; nb d6b3c82b43ff0e85cf11e453992ee40b6c8832d5aa5b0d8bc8f9eb29c35788fa; ns ba84fe241f434395ab4408d7f25360b0a0d5935a74253e94793c84b93b11e464; O f4039a874b6feaf97b3966a148bcf9a985b2f88901516b5954987a2a9ae3ad8a; P 0e02226887184b32b1aed104a73e8825b72ea7d5bf55e371f899dc0a639acaa0; adj a119fc5fd29a2e1d49bbcec023502a6ff1384f16ebe884f013a090a1ac1009f2; h7 0a3f80ad3c3881a0210535c4071457edde0f5dc490a3b4386ba1027026193226; h8 d510feb3b1cde0163fe1cbaef4a8d46856c928a94bd219ef5afb869e179c0b08; h8t 4a139b55e1dd03e164a4ee22066873e5a95d77f1f504e50cde6de6ecda8f0e6e; e' e4209a31883b7deb6e9bc360f88af565c5ce810219e3be960980d6a3e69099c8; layer ead406b2867e0f54ca6c8b88aed5a009468f0fbba870e69650eac563bb3ef84b. Two tried candidates fail the coast rule and one the barrier |
| Small, N = 128, seed 0 (no site; tries: coast, barrier, barrier, zone p90) | P, adj, h7, h8 as the shipped N = 128 seed 0 case; h8t d925e3ab848ad8c6ffb3e6fbc486ac198f542cf6e20f4e5b53249f19fe584c4b; e' ca3950733df5223ce1611e6c69d6e88c182f0668769be27d9d347694e5e66a3b; layer 50eb12f852f5f38da14980b857df34fe422781a4d1b576f84f79b05a4625526a |
| Small, N = 128, seed 8 (two capped passes on range 1: cols (72, 94) and (69, 35), axes 16 and 10 cells, ci 9 and 4, depth raw 3722241624 and 2965120895) | P 153363e52dba3ac0a7f4bd4b3fdf6a8de5974b8c2a8c0be7a69925225e7ac099; adj dcb69c21eae52ffeda6a38cabb4a519d894b619a6a84c07122ca407d6b03156e; h7 a5c12ba35a719cd335bcf95c98a0b1247a4aa898dc270013cfd2c80352aeb81a; h8 0e05b9b0b512b4d54dd3bb80a88afc00fbf5da3149ed9ff81f3153f1e221789b; h8t 327e6b3916ec56edc715489d8ff51ad90a4ed948fb25a5d2de35482bfa5c1d0f; e' 3125ad6fd0b1dff9dc9603c9410a15e401c0a7d57cc1c0c4468ed267e678f94c; layer 1578240a7313d25c93b8828be004579ef84cf708e956a9d138314c8eef141850 |
| Small, N = 128, seed 9 (no site; tries: across, tarn on the crossing) | P and adj as seed 0; h7 7ecb32fd5a899bea113545a8f893477fac81543eaf298f8f343b035a077ae3c3; h8 e30981579894a7676abb613e4a0fcb93efbdaceb898716524f451574dafa3c96; h8t a532343c77c14e1b431ad153607af8b53979121c69787c4b8f041fb8bfe4154f; e' e33673ba2a648ba2811b9bcbdcf50d33dbd5ba640a57432ea1776bdd9c0c6249; layer e0b66bbbb9083fe4660eac5d6fbcd70ddfb0f17c1e9ea70051fa386e6dd97076 |
| Small, N = 128, seed 31 (one uncapped pass, depth ONE: col (84, 67), axis 21, ci 11; tries before it: feet, barrier, barrier) | P 8768912497d41aa61eccec911d16b1ede6b8ae8ec5ebcfcbe5134ae4a9db4585; adj b7b1c7d41c2b279999a924756112d3ec7bdbdde1009399cc412a4c55296dfafd; h7 35012aa76a441a944d39995017d31d2fca88af9ab33b95916662e38f8ed6d7c3; h8 fdad85f5df957983bf2696aac36616cf189f5f096482a77eb74b6630bef9c9d9; h8t 56263941cc9ca5b13836c2e61c65977ae7e17040f2392ac9b4feb9eac9f969b8; e' 30d7de5445528b35394efb713051bda2c5e4c4155e15518a7f83b1c7f7da4478; layer 910ed145f33dd585ddc999d8e18ad8e381d4845f627603851db2d1423c06db47 |

(The order of the tries in the small seed 0 and 31 rows is the order of the exploration's reject counters, not the try order; the test takes the order from its own implementation.)

Elevation layer hashes, shipped values, from `worldgen-cli-r5 hash --db … --detail` on worlds generated with `bin/worldgen-cli-r5 generate --data bin/data-R5C500` (cap 500): N = 1024 seeds 0 to 9 (`~/.cache/the-unmapped/out/p1-01b-passes/worlds/R5C500/`): 44bc2c07bc695a1e608bfe8963ea5d88b807e6f78d27d2662194ca06ffab949d, 2e3b7dee44d149f739bbf6040f0345ee310c51f2f5e27940c57f1759ef16eec4, 58fe223112f73ebc8dbe03d64386144631b2cef6735f7117a6bd905b43592861, e9fb8259e2813298306a12b7d3611405485dd144de7cc6ec6cb6d0daca3d1273, 74ff0a90733704ba6d66f0bcef621f4d0dd76ad11c7fa1647ab83437edb6cdd8, 0efc92d64a60b4acf154cd096e57d2b8609f391d0054d623cc2ef10d9f061536, 162ae0a5ec41b22c6524f1fbb3c6b242c5a62de336018b89b8bd7e3f211f4c92, 81d5e490e155cdf7e6617d1fd89c42f1610fa6132ef37882494fd5175aa7d07a, ec1de02ad1778c4e53d9e81c49e15a1c2a5de5c6b72b87aef1ac9ced67f94052, 8f8c726a9941a62e86c4c7fcfd1add9291f204b13420694d80820d0c4f6754b3 (seed 0 also from the driver; seed 3 is unchanged from 0.5.0: no site and no final-talus change). N = 512 (spec writer's sweep, `~/.cache/the-unmapped/out/p1-01-spec-r5/worlds/R5C500/`): seed 0 34f5b1dd5d31c76454be9e217a80526800aa7b32fd5a68a31748c57d821d7f2a, seed 41 08e1bfa2be85aec74e62b14c4207fe037b13bde845870f40771ae5bb272c6168, seed 42 590569e0db4d6bc84fd8e955ef77ab49bec2c82b5d383fd75d212800e31a3b91.

#### 10.4.2 Neutral settings: the third version's values

With `pass_count_max` = 0 and `final_talus_iterations` = 0 (and the other fields as shipped, or as in the third version's small fixture) every grid of this subsection holds unchanged. Checked by the spec writer on the round-five build for N = 128 and 256 seed 0, and by the engineer for N = 1024 seeds 0 to 9 and N = 512 seeds 0, 41 and 42 (`r5-neutral-check.txt`). The values were obtained as follows.

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

These are reference values that the implementation must reproduce (they are what the approved algorithm computes), not golden values. The validation specialist records the goldens (with the fourth version's values, 10.4.1) once the implementation passes: the layer hash for seeds 0, 42 and the phrase "The Unmapped" at N = 512, seed 1 at N = 1024, seed 0 at N = 128 (library, both fixtures); and, to isolate the outline from the relief, the grid hashes of c, nb, ns and O for seed 0 at N = 128 and 512.

### 10.5 Statistics

The validation specialist's sweep (P1-00 13.1) checks the hard ranges there. **`step_p999_m` is again ≤ 150 m at every map size** (the final talus bounds it; the third version's 200 m at 512 is withdrawn; round five's maxima 136.4 / 118.7 / 61.5 m at 512 / 1024 / 2048). The size-dependent `elevation_m` p90 minimum stays (300 m at 512 and 1024, 200 m at 2048). The other steepness, flatness and edge-run statistics, and the new pass statistics (P1-00 10.3: `passes`, `natural_gaps`, `passes_capped`, `pass_col_m`, `passes_walkable`), are descriptive: reported, not tested. Walkability is an outcome, not a requirement (proposals "Natural passes through the ranges"). The elevation ranges of P1-00 13.1 were set from the round-five sweeps at cap 500 m (1024 seeds 0–49 by the engineer, `r5c500-stats-1024.txt`; 512 seeds 0–49 and 2048 seeds 0–7 by the spec writer, `~/.cache/the-unmapped/out/p1-01-spec-r5/`) and are provisional until the validation sweep of the implementation; the hydrology rows are to be re-measured. No test in this section asserts a statistics bound on a fixed world: the property and determinism tests of 10.2 and 10.3 check invariants only, so the bounds above do not enter `worldgen-tests`.

**Cap 300 to 700 m** (engineer, 1024 seeds 0–49, the same 59 cols for every cap since the cap does not change selection; `passes-definition.md` 11.8; walkable by the exploration's measure, which differs from P1-00 10.3's, see P1-00):

| `pass_max_cut_m` | walkable | cut at the col p50 / p90 | zone wall slope p75 (0.5.0, same cells: 58.9°) | tarns erased / lakes created in pass zones | 0.5.0 peaks within 1 km lowered > 300 m | hard bounds |
| --- | --- | --- | --- | --- | --- | --- |
| 300 | 29% | 299 / 301 m | 59.3° | 8 / 0 | 0 | ok |
| 400 | 36% | 398 / 401 m | 60.6° | 8 / 0 | 0 | ok |
| **500 (shipped)** | **41%** | **498 / 501 m** | **62.1°** | **8 / 0** | **0** | **ok** |
| 600 | 53% | 596 / 600 m | 63.6° | 8 / 1 | 0 | ok |
| 700 | 70% | 690 / 700 m | 64.6° | 8 / 1 | 0 | ok |

At 512 and 2048 only cap 400 was swept by the engineer; the spec writer's cap-500 sweep gives at 512: 33 passes, 21 of 50 worlds without, 22 walkable (67%); at 2048: 21 passes and 1 natural gap, 1 of 8 worlds without, 8 walkable (38%).

## 11. Known limitations

Items 2 to 10 and 12 were accepted for the first merge (lead's decision); items 1 and 13 to 18 are this revision's, for approval with it; item 11 is resolved. Each is a candidate for a later P1 revision. Seeds are 1024 unless stated; coordinates are cells, (0, 0) at the north-west. Measurements are round five at cap 500 unless stated.

1. **Many passes are not walkable.** By the exploration's measure (a 35° central-difference path inside the pass zone between the two feet), 24 of 59 passes at 1024 (41%), 22 of 33 at 512 (67%) and 8 of 21 at 2048 (38%) are walkable end to end. The others are natural high cols: the col is a real dip in the crest and valleys run up to it from both sides, but the last few hundred metres below the col keep the 0.5.0 mountainside's steepness (1 to 45 steep cells to cross, median about 25, at cap 400). Accepted by Paul ("realistic, and maybe the player character with magic can find alternate ways over"; history may improve them); a larger cap raises the share (cap 700: 70%, 10.5) at the cost of steeper zone walls. Ranges remain barriers elsewhere: the reachable share of land above 500 m is small (1024 median 1.8%).
2. **Small filled pads among the peaks.** Basins that fail the lake test become smooth, gently sloping surfaces; lowland fills carry the bump texture, but small fills among mountains rise only a few cells and still read as smooth patches (s0, x 600–700, y 450–700; s3 foothills, x 530–590, y 715–850).
3. **The floor guarantees area, not shape.** s13 still has a ring of ranges around a central basin; s12's west range runs along the coast. Where the floor triggers, land above 500 m is 0.17–0.23 at 1024.
4. **A few straight gullies** on the most planar flanks (s4 north flank, s5 west flank near y 540) and the s5 diagonal crease, a feature of the surface before erosion.
5. **Octagonal ocean depth contours** far from land (the 5/7 chamfer), and no offshore banks. Depth matters only for sea rendering.
6. **Coastal-cliff pockets.** Where a range runs into the sea at both ends it can seal off a small lowland with steep cells only a few metres above the sea (s18: 12 km² behind six steep cells at 12 m; s15; 512 s0). They are range feet on the shore, not river slots.
7. **Lake share at the cap.** The 3% cap binds in most 1024 and 2048 worlds; it keeps the deepest basins. `lake_share` is not evidence of variety.
8. **512 is steep.** v13's treatment is kept (no height scaling with the narrower belts): `step_p999_m` 62–136 m (512 seeds 0–49, with the final talus; 62–184 m before it), a median 38% of land steeper than 35°.
9. **Coasts and lowlands as in v13.** Coasts press on the falloff arcs and have little 100 m to 1 km detail (critic's D7, D8); flat lowland is rare (`lowland_flat_share` 0.2% to 4%, critic's D6).
10. **Rivers across land-floor flats** may keep short straight 45° or 90° segments (s9).
11. **Resolved: a range dropping straight into the sea (512 seed 41).** In 0.5.0 `step_p999_m` was 183.8 m and `max_step_m` 298.3 m there: a narrow range meets the north-west coast (x 108–128, y 53–82) and its flank falls to the shore as one smooth, very steep face that E8 cannot lower and the talus step at 24 iterations does not converge on. The final talus (E8.7) bounds it: 136.4 m and 199.6 m (1024 seed 41: 118.7 m and 183.5 m, the worst of each sweep), so `step_p999_m` ≤ 150 m is a hard bound at every size again (P1-00 13.1). The face is still a smooth cliff into the sea, now nearer 5:1, not a shaped sea cliff.
12. **Little upland in some 2048 worlds.** The mountain floor (E5.1) guarantees core area, not heights. 2048 seed 5 has one long range on a broad continent, 7.4% of land above 500 m and `elevation_m` p90 224.2 m; seeds 4 and 5 are below the typical 0.08 for `land_above_m` 500. P1-00 13.1's hard p90 minimum is therefore 200 m at 2048 (300 m at 512 and 1024). (Round five: 2048 p90 minimum 222.8 m, seed 5.) **Amended 2026-11-18** (Paul, proposals "Large maps may have unusual worlds"): the 0.8.0 validation sweep, the first with twenty 2048 worlds (`docs/reviews/p1-04-biomes-validation.md` D2), found 2048 seed 13 with one short coastal range and an isolated massif on a broad lowland continent: `elevation_m` p90 126.8 m and `land_above_m` 500 0.0328. Elevation is not changed; at 2048 the p90 minimum and the `land_above_m` 500 minimum of P1-00 13.1 are descriptive, and the 2048 typical ranges are widened (p90 200–1120 m, `land_above_m` 500 0.066–0.23). The entry "Every world has real uplands" still holds at 512 and 1024.
13. **512 coverage.** 21 of 50 worlds at 512 have no pass, and several should have one (the critic's round-four list of long or curved ranges that split the land; at cap 400 five of its eleven seeds have a pass, s16, s22, s24, s25, s48, and six do not, s3, s29, s34, s37, s38, s41). The causes (cap 400): 19 ranges with slots but no saddle candidate (no 150 m rise within 1.5 km on a short range), 11 zone-core summits, 10 along the range, 8 coast, 8 barrier. A size-scaled half-width or rise is the natural lever; not adopted.
14. **Tarns erased.** 8 tarns (lakes over 100 cells that keep less than half their cells) are erased by passes in 50 worlds at 1024, 1 in 8 at 2048 (cap 400), none at 512; no lake is created inside a pass zone. The price of allowing basin components under `pass_tarn_min_cells` on the crossing; the variant in which any basin cell rejects (R4S) erased 1 but had 63 passes against 72 in round four.
15. **Cols near range ends and transverse ridges.** A few cols sit near a range's end (round four: s14 pass 0, s28 pass 1, s11 pass 0), legal by the rise and shoulder rules, but a traveller could walk round. Where the ridged relief runs across the belt (s22), a crossing that is across the belt follows a valley between two ridges and can read as running along "the range" on the map.
16. **Changes outside the passes.** Erosion, E10 and the final talus re-run on slightly different terrain: outside the pass zones (2.1% of land) 30% of land cells change, by 0.22 m on average and 0.5% by more than 5 m; the lakes more than 4 km from every col change by more than 10% of their area in 3 of 36 worlds (s21, s43, s49).
17. **Natural gaps are rare** (4 sites in 50 worlds at 1024, 1 in 8 at 2048, none at 512): most ranges' crossings are higher than the saddle target.
18. **Summits are protected by rules, not by a hard bound.** E6.5.6 (k) and (l) keep the crossing away from the range's and the world's highest points; the factor there is still bounded only by 1 − 0.70 × depth (6.5). Measured: no world's highest point drops by more than 50 m.
19. **The very top of the height scale is unbounded** (added 2026-11-18; 0.8.0 validation D1). The mountain heights are scaled so that the land quantile at `mountain_peak_rank` 0.998 is `mountain_height_m`; the top 0.2% is not capped, so a world with a long upper tail gets a summit well above the target: 2048 seed 15 reaches 2931.1 m (59 cells above 2800 m, a smooth massif, no step over 90 m in its 7 × 7 neighbourhood; the next highest world at any size is 2584 m). Accepted as natural variation (Paul, proposals "Large maps may have unusual worlds"): at 2048 the p100 maximum of P1-00 13.1 is descriptive and the typical range is 1990–2760 m. The `i16` layer allows 3276.7 m, so nothing overflows; climate and biomes handle any height (P1-04 4.1 sentinels).

## 12. Open questions and findings for the lead

1. **Code defect, not specified (decided by the lead: keep `int64`):** the exploration code narrows lake_max_cells to `uint32` (`static_cast<std::uint32_t>((a + 255) / 256)`), which truncates for extreme tuning (for example `lake_max_area_m2` 10^9 with `lake_area_ref_n` ≤ 64 at N = 2048). This spec keeps it `int64` (4.5); with every value the exploration has used the two agree. The code also accepts `lake_area_ref_n` = 0 (meaning "do not scale"); this spec bounds it to [16, 2048] since only the scaled rule is v16.
2. **Decided (lead): the lake share cap always applies.** `lake_max_share` ≥ 0.001 and max_share_cells = max(1, round_int(…)); the exploration code's "0 = no cap" is not specified. This and the `int64` lake_max_cells of item 1 deliberately differ from the exploration code. Neither can change a world generated with the shipped tuning, at any N from 16 to 2048: the code's cap differs only when round_int(L_O × lake_max_share) = 0, i.e. L_O × lake_max_share < 0.5, while E4 guarantees L_O ≥ I_min = 196, so with 0.03 the value is at least round_int(5.88) = 6; and the `uint32` narrowing differs only when lake_max_cells ≥ 2^32, while the shipped values give at most 46875 (N = 2048).
3. **Decided (lead): `step_p999_m` is a hard check at every map size**, now ≤ 150 m at every size (third version: ≤ 200 m at 512 because of limitation 11, which the final talus resolves; round five's maxima 136.4, 118.7 and 61.5 m). It is the only hard steepness bound.
4. **River density.** With v16's terrain and hydrology's 0.5 km² threshold, `river_share_of_land` is 0.4% to 0.9% at 512 and 1024, below P1-00's former hard bound of 1%. Resolved by the lead (2026-11-16): the shipped `river_min_area_m2` is now 125000 m² (T = 489), giving 1.8% to 2.1% at 1024 seeds 0–9; the river rows of P1-00 13.1 are provisional until the validation sweep (P1-02 open question 3).
5. **Pockets get mountains.** E5 uses |s| for pocket cells (land enclosed below the coast threshold), so a deep pocket gets the coast factor and lowland profile of high inland ground. This is v13's rule, kept; noted because it is not obvious.
6. **No lakes on small maps** with the shipped values (N < 187). Accepted; library tests use the small-map fixture for lakes.

Items 1 to 6 are the third version's. New in this version:

7. **Decided (lead): the pass sites are stored.** Table `pass` (9.1), schema version 2 with generator 0.6.0; `biome` becomes version 3 and `region`, `region_border` version 4 (proposals "Pass sites are stored in the world file"). P1-00 10.3's `report` keys read it.
8. **Resolved (Paul, 2026-11-17): the cap is 500 m.** `pass_max_cut_m` is one field, shipped 500 m. The measured trade (10.5): walkable 29 / 36 / 41 / 53 / 70% for caps 300 / 400 / 500 / 600 / 700 m at 1024, with zone wall slope p75 rising from 59.3° to 64.6° (0.5.0: 58.9°) and one lake created inside a pass zone from 600 m on. A change of the value is a shipped tuning change (P1-00 6.4) and changes no rule of this spec.
9. **Decided (lead): the report's walkability measure, which differs from the exploration's.** `pt` uses each pass's zone (cells with bump ≥ 1/4) and 600 m discs at the ends, excludes lake cells from the feet and needs the internal bump; `report` cannot see the bump, so P1-00 10.3 defines an exact measure on the stored path (a 19-cell band around it and 37-cell discs at its ends). The two give similar but not identical counts, so the report's figures are not comparable one to one with the exploration's percentages quoted in this spec; the validation sweep reports both once, to calibrate. The report's measure is the descriptive statistic.
10. **Decided (lead): summits are protected by rules, not by a hard bound** (limitation 18, 6.5): the proven bound is the weak one of 6.5 (factor at a top ≥ ONE − 0.70 × depth), and the measured statement there (no world's highest point lower by more than 50 m) is descriptive. A hard rule (for example P = ONE near top_k and world_top) is not part of this version; revisit if the critic finds a cut summit.
11. **The zero "switch" values are removed** by raising five lower bounds (4.2). `pass_count_max` = 0 and `final_talus_iterations` = 0 remain and give the third version's layer (invariant 11), which the tests use as the neutral reference (10.4.2). Decided (lead).
12. **Memory budget.** This spec sets the stage's own peak to at most 1.3 × 0.5.0's (8, 8.1). The exploration build exceeds it (224 MB at 1024, 872 MB at 2048 against 132 and 490 MB); 8.1 shows the lifetimes that meet it without changing output. Decided (lead): a hard budget for the implementation (40 / 132 / 490 MB).
13. **512 coverage and the 512 time budget.** 21 of 50 worlds at 512 have no pass (limitation 13); the 512 time is at its limit (8; savings listed there). Neither is changed here (decided, lead).
14. **Natural gaps are tested before the tarn and summit rules** (code order; 12.15). A natural gap is recorded even with a tarn at its col or a summit next to it, which is harmless since nothing is lowered.
15. **Disagreements between the engineer's definition (`passes-definition.md`) and the code**, resolved for the code: (a) the width noise's second coordinate uses the **site index** s over all sites of the world, natural gaps included (`3s + 2`), not the slot j of the range as the definition's notation suggests (2.6 against 2.4); (b) the tarn-at-the-col rule runs **after** the natural-gap test, not as "item 5b" before the values (11.3); (c) the definition's overflow bound "flood keys and levels < 2^28 units (heights < 4096 m)" is too small: the natural mountain part can reach 8700 m before E7's clamp, so keys are < 2^30 units (no effect in `int64`); (d) the crest-axis and chord-angle tests apply only when the crest box has two distinct cells with different dA (the definition does not say); (e) "the body exit, else the chain's last cell" never needs its fallback (a chain ends at a seed, which is never a body cell). Otherwise the definition's sections 0 to 7 and 11 agree with the code's round-five path.
16. **Findings in the code** (none changes this spec; for the engineer and the auditor; nothing in this spec requires any of this work — see the implementation note at the end of section 8.1): (a) with `pass_r5` the code still computes round three's smoothed height `smh`, the `near_basin` disc grid, `notch`, `cap` and `site_of` grids, a whole-grid ocean chamfer and whole-grid factor-search scratch, all unused or replaceable (most of the excess memory and of the 512 pass cost); (b) the body exits are computed twice in the direction test (identical); (c) `final_talus` ignores its `threads` argument (single-threaded; correct, but E8.7 may run in parallel); (d) E7's `min(·, natural)` applies only where adj ≠ 0, so where only the factor applies the lowered part can exceed the natural part by one rounding step R (invariant 13; harmless, and this spec keeps it so as not to change output); (e) for a site capped at (h), the cap raise usually runs and moves p by 1 raw (T3 case C), because the largest p whose top is ≤ saddle has its top just below M − cap by rounding: harmless, and specified as computed. No determinism risk was found: every queue pops by a unique key, every tie in a box query or argmax is by index, and every parallel step is D1 or D2.

Proposals entries (for the lead): (i) **"Pass sites are stored in the world file"** (recorded 2026-11-17): the `pass` table, schema version 2; (ii) the decided entry **"Natural passes through the ranges"** (2026-11-17) can record that the specification is P1-01's fourth version with the cut cap a tuning field (500 m, confirmed by Paul) and walkability an outcome, not a requirement; (iii) **`step_p999_m` ≤ 150 m at every size** again, replacing the 200 m at 512 (a P1-00 13.1 change; the lead's earlier decision recorded it). The entry "Elevation is final after the elevation stage; hydrology does not carve" still holds (passes are cut before erosion, inside the elevation stage).

The third version's note on that entry: it points to P1-02 section 9, whose text said the stage "ends by filling depressions too small to be lakes"; with v16 it also fills depressions that are too shallow, too large or beyond the lake share, and P1-02 9.1 and its quoted entry were updated accordingly. If the lead keeps a copy of that wording elsewhere, it needs the same change.
