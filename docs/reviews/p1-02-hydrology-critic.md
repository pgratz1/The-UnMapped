# P1-02 hydrology: critic's review

2026-11-16 · world critic · candidate commit `3d96001` (main, `GENERATOR_VERSION` 0.4.0), shipped tuning (`river_min_area_m2` = 125000, T = 489 cells)

Material: worlds I generated with the `linux-gcc-release` build: 1024 seeds 0 to 19, 512 seeds 0 to 4, 2048 seeds 0 and 1. I also looked at the engineer's threshold comparison (`~/.cache/the-unmapped/out/p1-02-hydrology/alt/compare-s4.png`). Everything below is under `~/.cache/the-unmapped/out/p1-02-critic/` (`O/` below). Coordinates are map cells of the named world, with (0, 0) at the north-west corner. In the two-panel crops, the right panel shows rivers coloured by class (darker means a higher class) over a grey elevation stretch. Red marks river steps whose downstream cell has the same decimetre elevation, which are the steps routed by flood order. Pale yellow marks dry cells with a 4-neighbour at the same decimetre.

## 1. Verdict

**Hydrology can merge as it is.** Nothing I found is a blocker. Every lake in all 27 worlds has a visible river outflow, and the networks branch, have a clear hierarchy and reach sensible mouths. Two should-fix items are cheap and stay inside hydrology or the renderer:

- the composite map hides rivers and their hierarchy (S1);
- on true decimetre flats, flood order draws bundles of parallel 45° rivers (S2).

The angular trunks on gentle lowland, the "staircase" rivers, are mostly inherited from the elevation surface, not from hydrology's flat routing. My prototype shows that a hydrology-only tie-break barely moves them (A1). They are acceptable for a first merge and belong to a later elevation revision or to the runtime's smoothing.

## 2. What works

- **Networks read as drainage.** Rivers branch, confluences point downstream, and trunks run in the valley floors erosion cut. Examples:
  - `O/crops/s0-1024-west-lowland.png` (window x 280 to 480, y 540 to 740): an apron of branching streams that join and reach the coast in a few mouths.
  - `O/crops/s9-1024-central-plain.png` (x 400 to 600, y 300 to 500): a dendritic plain network feeding the big lake.
  - `O/maps/map-1024-s9.png`: the whole range drains north into the lake, which has one outlet to the north-east coast.
- **Lakes sit well and connect.** All 184 lakes in the 27 worlds have a river at their outlet (the river or the sea is the outlet's downstream cell). 164 of them, 89%, have at least one inflowing river; the rest are tarns. `O/crops/s0-1024-mountain-lakes.png` (x 470 to 670, y 470 to 670) has a chain of three mountain lakes: the southern one (about x 545, y 600) drains north into the large lake at x 590 to 680, y 500 to 570, which spills east. Known limitation 11.1 (small lakes with no visible outflow) did not occur in any world at the shipped threshold, because a kept lake (at least 391 cells, plus its catchment) almost always reaches T at its outlet.
- **Mouths are sensible.** Class-7 trunks reach the sea through incised gorges, for example 1024 s0 at (706, 128), in `O/crops/s0-1024-mouth-north.png`. I saw no river ending on a beach short of the sea and no trunk that ends inland.
- **There are enough major rivers for history.** At 1024, worlds have 9 to 20 mouths of class 4 or more, a top class of 7 to 9, and a largest basin of 9% to 34% of land. Every world has three to seven rivers that could anchor settlements or borders. At 2048 there are 41 or 42 major mouths and the top class is 9 or 10.
- **The flood arguments hold in practice.** In my prototype (S2), routing on a finer key leaves every lake cell and level unchanged, as section 4 argues.

## 3. Findings

### Blockers

None.

### Should fix (small, and none blocks the merge)

**S1. The composite map hides rivers and their hierarchy.** Every class is drawn one cell wide in pale to mid blue: class 1 is (120, 170, 230), close to the lowland green in brightness and lighter than the mountain browns. At 1:1, class 1 and class 8 are hard to tell apart, mountain streams nearly vanish against the hillshade, and diagonal steps touch only at corners, so diagonal rivers look dotted. See `O/maps/map-1024-s0.png`: the central range's streams are almost invisible, and in the west lowland the trunk does not stand out from its tributaries.

My mock is `O/proto/s9-1024-render-mock-compare.png` (window x 300 to 700, y 250 to 650 at 2×; shipped drawing on the left). It makes three changes:

- a darker, more saturated blue ramp, (70, 125, 215) for class 1 down to (20, 50, 160) for class 6 and above;
- class 4 and above thickened by one cell to the east and south;
- the corner gap of each diagonal step closed.

With it, the hierarchy reads at a glance and the trunks look like rivers rather than borders. The change is render-only (`render_composite_map` in `worldgen/src/inspect/render.cpp`) and touches no world byte. The mock script is `O/scripts/mock_render.py`.

**S2. Parallel 45° rivers across true decimetre flats.** Where a flat covers many cells at one decimetre value, G4 falls back to the flood parent everywhere on it. The FIFO flood then sends rays from the flat's exit in the eight directions, and neighbouring catchments draw two or three rivers side by side, one cell apart, in straight diagonal lines.

The worst case is 1024 s9 at x 546 to 610, y 762 to 834 (`O/crops/s9-1024-twin-channels.png`, at 5×). This is a coastal flat at 0.2 m (keys 2 dm), the same place as S2 of my v15 review, and now carries a bundle of three straight parallel rivers. The pattern is not limited to one world: 1.7% to 2.8% of river cells at 1024 have a river neighbour that is neither upstream, downstream nor joining at the same cell (`O/scripts/twins.py`); the s9 block at x 544, y 768 is the densest.

The fix stays inside hydrology and does not touch elevation. Run H2's flood on the key k = e × 2^16 + t, where t ∈ [0, 2^16) is a low-frequency coordinate noise from its own stream (for example `hydrology.tie`, value noise with a wavelength of about 400 m). Run G4 on that flood level, and define lakes and depths as before from L_e = floor(L_k / 2^16). Because t < 2^16, floor(L_k / 2^16) is exactly the decimetre flood level for every cell: the floor is monotone and commutes with the minimax over paths. So the lake cells, lake levels, `water_depth` and invariant 3 on W are unchanged, and only the path inside a decimetre flat changes.

The prototype `O/scripts/proto_tiebreak.py` checks this: float noise, the same G3 and G4, no lake rerouting. Its lake cells equal the stored ones in all three worlds tested. On the s9 flat it merges the bundle into one river with natural confluences (`O/proto/s9-1024-pad-compare.png`: FIFO, blur and blur plus noise, left to right). A box-blur term alone (the middle panel) does nothing there; the noise does the work.

Cost of the change:

- spec: an H2 paragraph, plus amending "uses no randomness" to "uses one coordinate noise, for tie-breaking on flats only";
- a stream name;
- `GENERATOR_VERSION` and the hydrology goldens;
- the elevation goldens do not change.

The key fits `int64`, since 32767 × 2^16 + 2^16 < 2^32.

### Acceptable for a first merge

**A1. Angular trunks on gentle lowland: the "staircase" rivers.** These are lowland trunks made of straight 0°, 45° and 90° segments with sharp turns, which look like polylines at map scale. Places:

- 1024 s13 interior basin, x 440 to 640, y 400 to 600 (`O/crops/s13-1024-interior-basin.png`);
- 1024 s1 north plain, x 560 to 760, y 240 to 440 (`O/crops/s1-1024-north-plain.png`), where a 1.2 km straight diagonal runs from (660, 323) to (717, 390);
- 2048 s1 east plain, x 1200 to 1460, y 900 to 1160 (`O/crops/s1-2048-east-plain.png`);
- 2048 at half scale, where the plain networks look like a polygon mesh (`O/crops/s1-2048-half.png`). At 1:1 they read much better (`O/crops/s1-2048-plain-1to1.png`).

Between 7% and 28% of river steps lie on decimetre flats at 1024 (median about 13%), and 22% to 27% at 2048 (`O/stats-summary.txt`, `river_flat_share`). One example: the s1 trunk falls only 1.1 m (612 to 601 dm) over 225 cells (3.6 km) at 61 m above the sea (`O/scripts/trace.py ../worlds/s1-1024.db 660 323`).

I expected S2's tie-break to fix these as well. **It does not**:

- `O/proto/s1-1024-north-plain-compare.png` and `O/proto/s13-1024-interior-compare.png` (FIFO left, tie-break right) show the same trunks with only cell-scale wiggles.
- These flats are not wide pads. They are narrow channel floors and terraces of the decimetre surface, so the path is fixed by the elevation the erosion stage cut, and only the choice of cell within a one-decimetre band is free.
- A hydrology fix that crossed decimetre steps would break invariant 3 (no uphill flow).

So the cheapest real fix is in elevation, for example less D8-aligned incision on low slopes, or keeping sub-decimetre order through E10.5's rounding. Either needs a full re-validation of elevation, which I would not start for this. Walking scale is the runtime's job under spec 9.2 (sub-cell smoothing and meandering of the traced path). Map scale is helped somewhat by S1's drawing.

My straight-reach metric agrees that the effect is modest. River cells on reaches of 48 cells (768 m) within one cell of a straight line are about 1% at 1024. The longest same-direction run of river cells is 272 to 512 m at 1024 and 688 m at 2048 (`O/scripts/straight.py`, `stats.py`).

**A2. A trunk running along the coast.** On 1024 s1 the largest river (class 8 to 9, 34 km²) runs west to east about 600 m inland, parallel to the north coast, for about 4 km on a nearly level 61 m plateau. It then turns south into the eastern lake and reaches the sea at (910, 401) (`O/crops/s1-1024-full-flatsteps.png`, top right). A coastal rim from elevation (S3 of my v15 review) blocks the short way to the sea. Hydrology routes it correctly; it reads odd but legible.

**A3. Combs of parallel streams on uniform aprons.** 1024 s0 west lowland and the s9 south flank have sub-parallel consequent streams that join late. This is physically right for a planar slope with uniform rain, and lowland texture is an elevation matter (my v13 D6).

**A4. Single-cell mouths with no estuaries or deltas.** This is fine for P1. A river of 10 to 35 km² is a few metres wide, below the cell size.

**A5. The longest river exceeds the map width in basin worlds.** 1024 s13 reports 25.5 km and s1 20.6 km on a 16.4 km map, because interior basins drain by long winding routes. That is plausible and gives history a long river. Note it for P1-00's statistics ranges.

## 4. Threshold

**Keep 0.125 km² at every size.** The cell is 16 m at every size, so a fixed area in m² means the same stream everywhere. A threshold scaled with the map would make a 2048 river a different physical thing from a 1024 one.

Measured at the shipped value:

| Size | `river_share_of_land` | River sources | Mouths | Major mouths (class 4+) | Top class |
| --- | --- | --- | --- | --- | --- |
| 512 (s0–4) | 1.3% to 1.7% | 44 to 51 | 20 to 32 | 3 to 6 | 5 to 7 |
| 1024 (s0–19) | 1.7% to 2.2% | 178 to 228 | 46 to 83 | 9 to 20 | 7 to 9 |
| 2048 (s0, s1) | 2.3% to 2.5% | 891 and 946 | 181 and 208 | 41 and 42 | 9 and 10 |

The source count at the shipped value, which spec section 4 says has not been measured yet, is in the table.

- **512:** sparse but proportionate for an 8 km island (`O/maps/map-512-s3.png`).
- **1024:** about right. In `alt/compare-s4.png` the 0.5 km² panel leaves the north and west lowland without streams, and the 0.064 km² panel fills the eastern range with hairlines. 0.125 km² gives the lowland a network without cluttering the range.
- **2048:** denser on the plains (largest catchments, so a higher share), but at 1:1 it reads like a well-watered lowland, not clutter.

If overview maps at 2048 look busy, filter by class when drawing (for example, hide class 1 at scales below 1:1) rather than change the data threshold.

## 5. Recommended changes, smallest first

1. **S1, renderer only:** a darker river ramp, class 4 and above one cell thicker, and diagonal corners closed. It changes no hash.
2. **S2, hydrology only:** sub-decimetre tie-break noise in H2's flood key. Lakes and levels are provably unchanged. Bump the version and re-record the hydrology goldens; the elevation goldens stay. This can follow the merge as its own small change.
3. **A1:** nothing now. Record in spec section 11 that angular lowland trunks are inherited from the decimetre terrain, and that tie-breaking cannot remove them without crossing decimetre steps. Revisit with the next elevation revision. Walking-scale smoothing remains the runtime's job.

## 6. Images and scripts

All under `~/.cache/the-unmapped/out/p1-02-critic/`:

- `maps/map-{512,1024,2048}-s*.png`: shipped composites. `worlds/*.db` and `*.report.txt`: the worlds and their reports. `stats-summary.txt`: per-world river and lake statistics.
- `crops/`:
  - `s0-1024-west-lowland.png`
  - `s0-1024-mountain-lakes.png`
  - `s0-1024-mouth-north.png`
  - `s0-1024-mouth-east.png`
  - `s1-1024-north-plain.png`
  - `s1-1024-full-flatsteps.png`
  - `s1-1024-flatsteps-panel.png`
  - `s9-1024-central-plain.png`
  - `s9-1024-twin-channels.png`
  - `s13-1024-interior-basin.png`
  - `s1-2048-east-plain.png`
  - `s1-2048-plain-1to1.png`
  - `s0-2048-half.png`
  - `s1-2048-half.png`
- `proto/`:
  - `s9-1024-pad-compare.png`: S2, where the tie-break works;
  - `s1-1024-north-plain-compare.png` and `s13-1024-interior-compare.png`: A1, where it does not;
  - `s9-1024-render-mock-compare.png`: S1.
- `scripts/`:
  - `wdb.py`: a layer decoder for blob format v1;
  - `stats.py`, `straight.py`, `twins.py`, `trace.py`: the measurements;
  - `crop.py`: the crops;
  - `proto_tiebreak.py`: the S2 prototype (float, illustrative only);
  - `mock_render.py`: the S1 mock.
