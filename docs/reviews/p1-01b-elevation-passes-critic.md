# P1-01 elevation revision "natural passes and final talus" (generator 0.6.0): critic review

World critic, 2026-11-16. Merge-gate review of the implementation validated at `820ddb5`. Material: the validation sweep (`~/.cache/the-unmapped/out/p1-01b-validation/`, 512 seeds 0–49, 1024 seeds 0–49, 2048 seeds 0–7), compared cell for cell with the 0.5.0 sweep (`~/.cache/the-unmapped/out/p1-02b-validation/`). I read every lowered pass and natural gap in the `pass` table (113 lowered, 6 gaps). Each one was cropped as a before/after strip, and 15 places were looked at closely with profiles.

All my files are under `~/.cache/the-unmapped/out/p1-01b-validation/critic/`:

- `tools/`: `ana6.py` (per-pass measures), `ana6x.py` (the same measures on the exploration worlds), `side.py` (changes away from passes), `crop6.py`, `strips.py`.
- `data/`: per-pass tables `a6-{512,1024,2048}.tsv`, `a6x-R4-1024.tsv`, `r4measure-R5C500-1024.tsv`, `side-*.tsv`, `breach.txt`.
- `strips/`: every pass, 5 per sheet. The columns are 0.5.0 map | 0.6.0 map | 0.5.0 hillshade | 0.6.0 hillshade. Red marks slopes over 35°, and cyan outlines ground lowered by more than 50 m.
- `img/`: detailed crops with a lowering map and two profiles, one along the stored path and one along the crest.

Cells are 16 m at every size.

**Identity check.** The 0.6.0 1024 `elevation` layers equal the round-five exploration worlds `R5C500` byte for byte, and the 0.5.0 layers equal `v16/d-R4` (seeds 0, 5 and 13 checked). So round four's own script (`critic-r4/tools/ana4.py`) can be run unchanged on the implemented result.

## Verdict

**Merge 0.6.0 as is.** Capping at 500 m fixed round four's blocker. Passes now read as natural saddles and gates, both on the map and in the hillshade. The faults that remain are fixes for the next elevation revision, and none of them is a reason to hold the merge:

- some uncapped low gates breach a narrow range down to lowland level;
- tarns are still erased;
- long smooth ribbons appear at 2048;
- lakes are swapped away from passes.

## What works

- **The road look is gone.** In round four the col was cut by a median of 846 m (41 of 72 cuts were 800 m or more). Now the cut is 498 m (p90 501), and the col sits at 0.42 of the lower shoulder (round four: 0.25).
  - Round four's script, run on the same 1024 worlds: zone-wall slope p75 is 63° against 59° on the same cells in 0.5.0 and 65° on the mountains around. Round four had 76°, with 31% of wall cells at 75° or more; that share is now 0%.
  - Capped high cols keep the shape of the natural crossing, a scaled copy about 500 m lower, and read as U saddles between two summits. Examples: `img/c6-1024-s13-p1.png` (1662 → 1166 m) and `img/c6-1024-s11-p2.png`.
- **At map scale the passes are quiet notches and through-valleys.** I looked at all 59 lowered passes at 1024. None reads as a pale band, and almost all read like valleys a cartographer would draw. Good examples, all in `strips/`:
  - 1024 s8 pass 1 (517,313), `s1024-02.png`;
  - s0 pass 2, s17 pass 1, s24 pass 1, s28 pass 1, s31 passes 1–2, s46 pass 2;
  - 2048 s6 pass 1 and s3 passes 1–2.
- **Summits are kept.** No 0.5.0 summit within 3 km of a col drops by more than 200 m (0 of 113 passes). Within 1 km, one drops by more than 300 m (512 s23). No world's highest point drops by more than 7.6 m.
- **Crossings go across the range.** At 1024, oblique crossings fell from 5 to 4 and crossings with an arc over 4 km from 11 to 6.
- **Side benefits.** At 512 s9 (169,199) a pass drains a large straight-edged filled pad from 0.5.0 (limitation 2), and real terrain and rivers come back. At 1024 s49 a filled pad became a lake with an island.
- **The final talus is invisible at map scale away from passes** (see F6).

## Round-four measures, re-measured

The headline comparison uses my lowering-based zone proxy (`ana6`), because the exploration's `notch` grid changed meaning in round five. It now stores depth × bump, so round four's "bump ≥ ½" core selects only part of a capped zone. I applied `ana6` identically to R4 and to 0.6.0 at 1024. Round four's own script, run on 0.6.0, is given in brackets.

| Measure (1024) | R4 (round four) | 0.6.0 | Judgement |
| --- | --- | --- | --- |
| Cut at col p50 | 840 m | 498 m | fixed |
| Wall slope p75, zone / same cells in 0.5.0 / ring mountains | 74.4 / 61.9 / 63.6° | 68.0 / 61.8 / 63.9° [63 / 59 / 65°] | fixed: walls about 4° steeper than the mountains around, not 11° |
| Wall cells ≥ 75° | 25% | 6% [0%] | fixed |
| Floor roughness, zone / ring | 0.34 | 0.48 [0.43] | better; uncapped floors still smooth (0.33) |
| Floor roughness, zone / same cells in 0.5.0 | 0.43 | 0.64 [0.58] | better |
| Flat core cells (< 5°) p50 | 11% | 2% | better |
| Oblique crossings (> 30°) | 5 / 72 | 4 / 59 (s12 pass 2, s15 pass 2, s37 pass 1, s48 pass 2) | same |
| Arc > 4 km | 11 / 72 | 6 / 59 [6] | better; at 2048, 6 of 21 |
| Col position along the range, p10 / p50 | [0.15 / 0.32] | [0.11 / 0.27]; under 0.15: 16 / 59 by my proxy (R4: 16 / 72) | slightly nearer the ends (lowest saddle first) |
| Range end, lower shoulder < max(600, 0.4 × other) | [1] | [3] (s2 pass 1, s23 pass 2, s33 pass 2) | acceptable |
| 3 km summit drop > 200 m / 1 km summit drop > 300 m | 0 / 1 | 0 / 0 (512: 0 / 1; 2048: 0 / 0, max 261 m) | fixed |
| Tarns > 100 cells halved, in the zone | 7 | 6–7 (7 by round four's script) | not fixed |
| Lakes created in the zone | 2 | 0 | fixed |
| Path cells steeper than 35°, p50 (capped / uncapped) | 6 | 45 / 0 | the cost of capping, accepted by Paul |

512 and 2048 by `ana6`:

| Size | Wall p75 / 0.5.0 / ring | Roughness zone/ring; zone/0.5.0 | Arc > 4 km | Oblique |
| --- | --- | --- | --- | --- |
| 512 | 69.6 / 65.0 / 67.9° | 0.35; 0.48 | 0 | 4 |
| 2048 | 68.0 / 61.8 / 61.0° | 0.53; 0.65 | 6 | 1 |

## Findings

### F1. Uncapped low gates breach the range to lowland level. Should-fix; the most frequent artificial look.

Where the cap does not bind, the target saddle comes from the walk grade, clamped to at least `pass_min_col_m` (150 m). After the floor drainage and erosion, the highest point of the crossing ends only 50–150 m above the higher foot. The range is cut clean through: a 400–600 m wide slot with a flat, lowland-green floor. Its walls run straight on narrow uniform ranges.

**Frequency** (`data/breach.txt`): crossing relief above the higher foot is under 150 m in:

- 512: 11 of 12 uncapped gates (plus 4 capped);
- 1024: 11 of 15 uncapped;
- 2048: 3 of 3 uncapped.

In 0.5.0 the same crossings stood 300–560 m above the feet. Rule (m) asks for the target to be 150 m above the feet, and it is not kept after erosion.

**How it looks depends on the range:**

- **Natural** where the range is broad and textured. 1024 s8 pass 1 (relief 61 m), s15 pass 1, s28 pass 1 and s31 pass 1 read as wind gaps.
- **Artificial** on narrow or smooth ranges:
  - **2048 s2 pass 1 (539,867)**, `img/c6-2048-s2-p1.png`. A sharp slot through a smooth 800 m ridge: the crossing profile is flat at 150–190 m for the whole 0.9 km and the west wall is a straight line. This is the worst at 2048.
  - **512 s23 pass 2 (311,107)**, `img/c6-512-s23-p2.png`. A 1 km gap at 190 m severs the range, and the 980 m peak beside it falls to 548 m (the 432 m drop, 1 of 33 passes at 512). The north half becomes a separate massif. On the map it reads as a believable gap, but it is a breach rather than a pass.
  - **512 s15 pass 1 (170,144)**, `img/c6-512-s15-p1.png`. The crossing is only 0.6 km long, so the saddle target is the 150 m minimum, and the floor is a flat green pad with a straight east edge (relief 79 m, the core is 25% flat). This is the most artificial pass in the sweep. The same mechanism (short crossing, minimum target) appears at 512 s23 pass 1 (0.5 km), s31 pass 1 (0.6 km) and s44 pass 1 (relief 48 m, flat 24%).

About 8 of the 29 uncapped gates read as artificial on close inspection: 512 s15, s23 pass 2, s31 and s44; 2048 s2 pass 1, s6 pass 1 and s0 pass 2; 1024 s33 pass 2. The rest read as natural gaps.

**Smallest fix:** keep a residual hump. One option is to cap the uncapped cut too, as a fraction of the natural relief, for example the saddle stays at or above feet + 0.4 × (M − feet). The other is to raise `pass_min_col_m` to about 250 m and measure rule (m) on the drained surface. Either keeps the gate a low pass instead of a breach.

### F2. Tarns at and near cols are still erased. Should-fix.

**Frequency:**

- 1024: 6 tarns in 5 worlds (7 by round four's script):
  - s11 pass 2: 900 → 0;
  - s21 pass 1: 1658 → 500;
  - s22 pass 2: 320 → 0;
  - s44: 487 → 0;
  - s48 passes 1 and 2: 442 → 0 and 441 → 67.
- 2048: 1 (s1 pass 3 (1297,1697), 706 → 0).
- 512: none.

This is the same count as round four: the tarn-at-col rule changed nothing measurable.

**Cause:** rule (j) looks within `pass_col_tarn_m` = 160 m of the col, and rule (g) only rejects basins of 300 cells or more on the axis. The lost tarns lie 0.3–2 km along the crossing or beside it, and their pre-erosion basins are under 300 cells.

**Effect:**

- **1024 s11 pass 2 (545,359)**, `img/c6-1024-s11-p2.png`. A pair of tarns on the shoulder of a natural high col becomes a smooth tan slope. In 0.5.0 that was the most pass-like spot in the range.
- **1024 s21 pass 1 (750,389)**, `img/c6-1024-s21-p1.png`. A glen with a lake at each end loses most of both: a 5.5 km uncapped crossing runs along the lake valley.

For dark high fantasy a tarn at a pass is a prime site (a shrine, a drowned watchpost), so losing about one world in eight's best one matters for content more than for realism.

**Smallest fix:** apply rule (j) to every axis cell within the core half-width (about hr), not only to the col, with basin components of any size. The alternative is `pass_tarn_min_cells` 300 → 100. Re-measure the pass count, since R4S lost 9 of 72 passes with its stricter rule.

### F3. Long crossings at 2048 read as smooth ribbons. Should-fix, low.

6 of 21 crossings at 2048 are over 4 km:

| Pass | Length |
| --- | --- |
| s0 pass 1 | 5.8 km |
| s1 pass 3 | 5.9 km |
| s2 pass 3 | 4.8 km |
| s6 pass 3 | 4.5 km |
| s6 pass 4 | 4.0 km |
| s7 pass 2 | 5.0 km |

At 1024, 6 of 59 are. The lowered band is a uniform pale stripe with near-parallel edges, lowered about 500 m along its whole length. 2048 s7 pass 2 (1731,1038) in `img/c6-2048-s7-p2.png` is a broad trough across the whole massif to the sea, and s7 pass 1 is next to it. On the map these are believable glacial troughs. In the hillshade, and probably on the ground, the floor is smoother than anything nearby.

**Smallest fix:** round four's S3 rule, a total arc limit of about 4 km on the axis. Alternatively, fade the depth with distance from the col (for example the bump × fade(1 − |s_m| / 2 km) term) so the lowering is concentrated at the col.

### F4. Lakes are swapped far from any col. Should-fix, low; limitation 16.

E10's lake share cap binds at 1024 and 2048. Erasing or deepening a lake at a pass, plus small terrain changes, reshuffles which basins are kept, and a basin that is dropped becomes a filled smooth pad (limitation 2).

**Frequency:** counting lakes over 100 cells more than 4 km from every lowered crossing:

| Size | Lost | Gained |
| --- | --- | --- |
| 1024 | 6 in 5 worlds (s21, s43, s44 ×2, s49) | 4 (s11, s21, s43) |
| 2048 | 4 in 3 of 8 worlds (s0, s1, s6) | 2 (s1, s7) |

The 2048 losses include a 4195-cell lake at s6 (1013,593) and a 3277-cell lake at s1 (952,1670). Examples:

- 2048 s0 (1308,1013): a tarn becomes a pad 159 m higher, about 8 km from any col (`img/c6-2048-s0-filledlake.png`).
- 2048 s1 (638,1017): the lake beside the natural gap becomes a pad (`strips/s2048-00.png`).

The number of lakes barely moves. The cost is that a world's lakes are not stable under a local change, and every lost lake leaves a visible pad.

**Fix:** belongs with limitation 2, for example give filled pads the bump texture in the mountains too, or let E10 fill a dropped basin to its spill surface with noise. No change is needed in the pass rules.

### F5. Cols nearer range ends; long ranges without a pass. Acceptable.

**Cols near the ends.** Picking the lowest saddle first moves cols slightly toward range ends (p10 0.11 against 0.15). Three cols sit on a low shoulder: 1024 s2 pass 1, s23 pass 2 and s33 pass 2. 1024 s14 pass 1 (725,430) sits where the snow range drops to a short spur 3 km from the coast; a traveller could go round, but the col is a true saddle.

**Ranges with no pass.** 1024 s1, s20, s29 and s39 have long ranges and no site. I tested whether ranges actually cut the land: is the land below 400 m (lakes excluded) split into parts of more than 5% each? Only these worlds are:

- 1024 s20 (no site): 91% / 7%;
- 1024 s30 (no site): 94% / 6%;
- 512 s20 (two sites): 91% / 9%.

In every other world, s1, s29 and s39 included, the lowland is connected round the range ends. So a world without a pass costs detours, not reachability. That is good for history (passes stay rare and valuable) and does not hurt play.

At 512, 21 of 50 worlds have no site. This is limitation 13, unchanged and accepted.

### F6. Final talus and other changes away from passes. Acceptable.

Away from passes (more than 3 km from any lowered crossing), the share of land changed by more than 5 m averages 0.66% / 0.14% / 0.17% of land at 512 / 1024 / 2048. By more than 20 m it is 0.32% / 0.03% / 0.03%.

Cells changed by more than 50 m:

- 512: 16 of 50 worlds. Most cells are at s12 (531), s34 (408) and s41 (412, the intended cliff fix of limitation 11, −451 m).
- 1024: 6 worlds, 1 to 88 cells each.
- 2048: 2 worlds, both lake swaps (F4).

Near-shore cells changed by more than 20 m: 532 at 512 (10 worlds), 525 at 1024 (6 worlds: s10, s19, s20, s27, s40, s41), none at 2048.

The talus moves material down steep faces: the upper faces drop and their feet rise by up to about 100 m. At 512 s43 (432,192), `img/c6-512-s43-cliff.png`, two spur faces are reshaped by about ±100 m, and the two maps are almost indistinguishable. Erosion re-running also shifts gullies sideways by a few cells along ranges, visible as ±50–100 m stripes in the difference maps (512 s15 south of the col, `img/c6-512-s15-p1.png`), but not on the map. Nothing here reads as an artefact.

### F7. Capped high cols are smooth and non-walkable. Acceptable, by Paul's decision.

Capped cols need a median of 45 steep path cells. They read as natural U saddles (1024 s13 pass 1, s5 pass 2, s27 pass 1, s33 pass 1). Their floors are smoother than the mountainside: roughness about 0.56 of the ring. At 1024 s13 this shows as a pale smooth notch in the snow, but it is no longer a band.

The 2048 slots (s2 pass 2, s3 passes 1 and 2) are narrow, straight-sided notches across uniform ranges. They are noticeable but plausible, and the smooth 2048 ranges make them stand out more than the passes do.

Paul accepts that many capped passes are not walkable, and history can improve them. That is fine for play: a capped col is a named, visible saddle where a road, a stair or a tunnel can be built later.

## Patterns across seeds

| | 512 | 1024 | 2048 |
| --- | --- | --- | --- |
| Lowered passes / worlds with a site | 33 / 29 | 59 / 36 | 21 / 7 |
| Cut at col p50 | 494 m | 498 m | 496 m |
| Col height p50 | 322 m | 507 m | 533 m |
| Uncapped gates breaching to < 150 m above the feet | 11 / 12 | 11 / 15 | 3 / 3 |
| Of those, artificial-looking | 4 | 1 | 3 |
| Tarns erased | 0 | 6 | 1 |
| Far lakes lost / gained | 0 / 0 | 6 / 4 | 4 / 2 |
| 1 km peak lowered > 300 m | 1 (s23) | 0 | 0 |
| Crossings > 4 km | 0 | 6 | 6 |
| Oblique > 30° | 4 | 4 | 1 |
| Worlds whose lowland is split by a range with no pass | 0 | 2 (s20, s30) | 0 |

## Follow-ups for the next elevation revision, in priority order

1. **Keep a hump in uncapped gates (F1).** Bound the uncapped cut, saddle ≥ feet + max(150 m, 0.4 × (M − feet)), or raise `pass_min_col_m` to about 250 m and check rule (m) on the drained surface. Removes the breaches and flat pads (about 8 artificial gates in the sweep).
2. **Protect tarns along the crossing core (F2).** Apply rule (j) at every axis cell within the core half-width, with basins of any size; or `pass_tarn_min_cells` 300 → 100. Saves about 7 tarns per 58 worlds; re-measure pass counts.
3. **Limit the total crossing arc to about 4 km, or concentrate the depth near the col (F3).** Mostly a 2048 fix.
4. **Give filled pads texture, or stabilise E10's choice under the cap (F4, limitation 2).** Removes visible pads where a lake was dropped.
5. **512 coverage (limitation 13), unchanged.** Size-scale the rise and half-width. Lowest priority, since lowland stays connected.

## Images for Paul

- Good:
  - `img/c6-1024-s13-p1.png`: capped high col, a natural U saddle in a snow range.
  - `strips/s1024-02.png`: 1024 s8 low gate and s9/s10 capped cols.
- Bad:
  - `img/c6-2048-s2-p1.png`: an uncapped gate breached to the lowland, a straight-walled slot.
  - `img/c6-512-s23-p2.png`: range severed and peak lowered by 432 m.
  - `img/c6-1024-s11-p2.png`: tarn pair erased at a natural col.
  - `img/c6-2048-s7-p2.png`: 5 km smooth ribbon.
- Also: `img/c6-512-s15-p1.png` (flat-edged pad), `img/c6-2048-s0-filledlake.png` (lake swapped to a pad), and all passes in `strips/s{512,1024,2048}-*.png`.
