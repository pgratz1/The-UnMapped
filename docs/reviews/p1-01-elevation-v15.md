# P1-01 elevation, design candidate v15: critic's review

2026-11-15 · world critic · candidate `p1/elevation-explore` commit 15ffaa4

Material reviewed: `~/.cache/the-unmapped/out/p1-elevation-review/explore/` (called `explore/` below), round four in `INDEX.txt`: `v15-definition.md`, `v15-erosion-levels-1024.png` with its caption and statistics, `v13-vs-v15-{512,1024,2048}-contact.png`, the per-world crops, slope and rivers-and-lakes images for 1024 s0, s3, s5, s9 and 2048 s0, s1, `v15-stripes-D3-1024.png`, `v15-2048-s1-crater-fill.png`, `v15-access.txt` and the stats files. I also rendered my own views from the `r4/v13` and `r4/v15` grids (listed at the end). Coordinates are map cells of the named world, (0, 0) at the north-west corner, unless a crop is named.

I judge v15 against Paul's direction recorded in `docs/design/proposals.md` ("High peaks may be inaccessible; history opens the passes"): v13's maps, a little more erosion, unwalkable high ground allowed. Walkability of the ranges is not a criterion here.

## 1. Verdict

**v15 can go to spec and implementation after one small tuning change: scale the lake area limit with map size so that 2048 s1's 13 km² filled plateau becomes a lake.** Everything else is should-fix or acceptable for a first merge. v15 keeps v13's character: at contact-sheet scale the 1024 and 2048 worlds are hard to tell from v13 except where the mountain floor acted, and at 1:1 the ranges gain side gullies and lose v13's worst artefacts (the polygonal fill plates with parallel channels, and most of the axis-aligned stripes). The new defects it introduces (smooth filled pads, coast-hugging rims from the floor, octagonal sea contours) are smaller than the ones it removes.

The spec must also amend P1-01's purpose sentence ("ranges … that can be walked"), as the engineer notes; that is a text change, not a generator change.

## 2. What works

- **Character kept.** In `v13-vs-v15-1024-contact.png` seeds 0, 2, 3, 5, 6, 7, 8, 11, 14 to 19 have the same ranges, outlines and reading as v13. Paul's preference is respected.
- **Plates and their channels are gone.** `v15-2048-s0-lowland-1to1.png`: v13's staircase channels across the west plain (left half, x ≈ 100 to 350 of the crop) are gone; the plain now drains through ordinary branching streams. `flat_patch_up100_km2` at 1024 drops from a v13 maximum of 0.070 to 0.025 km².
- **Rivers and lakes read well.** `v15-1024-s9-rivers-lakes.png` has a chain of lakes along the foot of the range (x ≈ 370 to 690, y ≈ 300 to 560) linked by a river that leaves through the north-east coast: a believable piedmont lake district. `v15-1024-s0-rivers-lakes.png` has tarns along the crest and trunk rivers reaching bays. `v15-1024-s3-rivers-lakes.png` has a large lake with an island (x ≈ 290 to 400, y ≈ 715 to 850) and an outlet to a bay. Every 512 world now has at least one lake (v13: five had none).
- **Mountain-light worlds have mountains.** `above500` minimum at 1024 rises from 0.009 to 0.077. 1024 s9 and s10 now have real ranges (s9: an arc across the south from the west coast to the east lobe).
- **Determinism and cost.** Thread- and compiler-independent hashes; 6.7 s at 1024 within the 8 s budget.

## 3. Erosion level

**6× is the right level; keep it.** In `v15-erosion-levels-1024.png` (and my 2× zooms, below), 3× is indistinguishable from 1× at 1:1, as the engineer says; 6× is the first level where flanks carry branching side gullies (1024 s7 north-east flank, s8 north-west flank, s0 west flank); 9× starts to comb smooth flanks into sub-parallel rows (s8 north-west, s2 south flank) and raises `max_step` to 111 m. Peaks and crests are unchanged at 6× (p100 median 2256 m against 2265 m at 1×), so v13's silhouettes survive.

One caveat for the spec, not a change request: at 6× the new channels are one-cell slots with crisp walls, read as cracks rather than V-shaped valleys (`critic-v15/zoom-ero-s5-1x-vs-6x.png`, the radial cracks on the inner flank of the s5 crescent; `zoom-ero-s8-1x-vs-6x.png`, the north-west flank). That is acceptable for this merge. If valleys are wanted wider later, a little more diffusion in the last few iterations only would widen slots without rounding the crests; I would not tune that now.

## 4. Findings, by class

Seeds are 1024 unless noted.

### Blocker (fix before the spec is written)

**B1. 2048 s1: the floor's ring massif encloses a 13 km² filled plateau.** `v15-2048-s1-crater-fill.png`, middle panel: a smooth tan surface inside the ring, about x ≈ 750 to 900, y ≈ 1100 to 1300 of the world, visible even on `v13-vs-v15-2048-contact.png` as a flat-topped mesa. It is the single most artificial feature in the set, on the size most likely to be shown. The engineer's own third panel (lake limit 20 km², share 5%) turns it into a crater lake that reads well. Minimal fix: scale `lake_max_area_m2` with map area (3 km² at 1024, 12 km² at 2048, 0.75 km² at 512) and scale nothing else. If that alone does not keep this basin (the 3% share cap may still drop it), let the cap skip a basin only when it is the largest candidate in the world, or raise `lake_max_share` to 0.04 at 2048 only. Re-render 2048 s0 to s4 after the change.

### Should fix (during implementation or the next tuning pass; none blocks the spec)

**S1. Filled basins as smooth pads with crisp edges (engineer's item 1, K2).** Confirmed, and ranked first among the should-fix items. Places: 1024 s3 north plain (x ≈ 390 to 510, y ≈ 540 to 625, about 2 km², the basin that held v13's wedge lake), s3 foothills (x ≈ 530 to 590, y ≈ 715 to 850; x ≈ 245 to 290, y ≈ 795 to 830), small tan pads among the s0 peaks (x ≈ 600 to 700, y ≈ 450 to 700, erosion-levels row 1), 2048 s0 west plain (x ≈ 480 to 740, y ≈ 720 to 930). My overlay (`critic-v15/pads-1024-s3.png`, red = near-planar, nearly level cells above 20 m) shows the s3 north pad as one solid untextured block where v13's surface carried the ordinary bump field. A pad on a lowland is a plausible sediment flat, and later stages can make it marsh or meadow, so in lowlands it is close to acceptable. Among peaks it reads as a paved patch. Two cheap mitigations: B1's area scaling turns some pads into lakes; and the filled surface can take a fraction of the pre-fill detail noise as long as each cell stays above the cell that reached it in the flood (add `max(0, detail)` scaled to a metre or two onto `eps` for depressed cells). Do not reintroduce breaching; that is where v13's straight channels came from.

**S2. Straight river across a pad.** `v15-1024-s9-rivers-lakes.png`, x ≈ 550 to 630, y ≈ 750 to 830: a river runs a straight 45° line about 1.8 km long across a filled basin. This is the old D2 or D10 failure in a new place: the flood order across a nearly uniform fill draws a straight path. S1's texture on filled cells would also break this line. Cosmetic on its own.

**S3. The mountain floor guarantees area, not shape (engineer's item 3, K5).** Confirmed. 1024 s1 is still a round knob (elongation 1.08). 1024 s12 gains an L-shaped wall along the west and south coasts, and s13 a near ring of rims on the north, east and south coasts (`v13-vs-v15-1024-contact.png`, row 3). 2048 s4 (not on the engineer's list): the floor moves the belt, so v13's east massif becomes an L-shaped range and a new straight wall appears along the west coast (`v13-vs-v15-2048-contact.png`, last column). 1024 s4, the same outline, does not trigger the floor, so the same seed now has different mountains at 1024 and 2048. 2048 triggers more often because the belt half-width is capped at 2000 m, which is half the map share at 2048 that it is at 1024. These worlds are better than v13's plain-with-a-knob for history (borders exist), so the floor stays. Cheap improvement: in the offset search, score each candidate offset by core cells that are at least a set distance inland (for example `gm` ≥ 0.9 rather than any `gm`), so that rims along the coast lose to inland level sets. It changes only the worlds where the floor triggers.

**S4. Residual straight gullies and one straight crease (engineer's item 4, K3).** `v15-stripes-D3-1024.png`. Row 1 (s4 north flank): three or four column-aligned gullies remain, against a comb of a dozen without random receivers; acceptable. Row 3 (s5 west flank): two long row-aligned channels still cross the whole flank (y ≈ 540, x ≈ 340 to 480), clearly straight. Row 4 (s5 diagonal crease, x ≈ 470 to 620, y ≈ 240 to 330): the 2.5 km straight crease from my v13 review is still there, softened but straight; erosion cannot remove a feature of the pre-erosion surface. I have not found its cause; one candidate is the kink of `abs(nb)` along the belt's centre line where `nb` is nearly linear, which the engineer can check cheaply by rendering the mask alone. Neither is worth holding the merge for.

**S5. Lowland-separating steep bands near sea level (K7).** `v15-access.txt`: 1024 s18 has a 12.2 km² walkable region cut off by six steep cells at 12 m, s15 5.5 km² behind four cells at 10 m, and 512 s0 is split into three regions at 11 m. Steep ground at 10 m is not a peak, so Paul's decision does not cover it. These are probably slot-incised river mouths or steep shores. I did not inspect them. The engineer should look at each one before the spec: if they are one-cell river slots, the talus step should catch them; if they are shoreline cliffs, they are acceptable.

### Acceptable for a first merge

**A1. Lake share at the 3% cap (engineer's item 2, K4).** Visually the lakes are not too many; 1024 s9 with six lakes is the busiest and reads well. A cap that binds in most worlds only matters through S1, since the basins the cap drops become pads. State in the spec that the share is a cap, and do not use `lake_share` as evidence of variety.

**A2. Ocean shelf by distance (engineer's item 5, K9).** My banded depth render (`critic-v15/ocean-contours-1024-s4.png`) shows it plainly: v15's 20 m contours are octagons by 3 to 4 km from the coast, where v13's followed the continent field, with offshore banks and v13's circle. The coast-hugging part of the shelf is better than v13's circle, which drew the falloff into the sea (D7). Depth matters only for sea rendering and later coast biomes, so this is cosmetic. Two cheap fixes for later: an exact integer Euclidean distance transform instead of the 5/7 chamfer, and adding back a small share of the continent-noise term to restore banks.

**A3. Closed ranges without saddle passes (engineer's item 6, K1).** Under Paul's direction this is acceptable, and it suits the design: ranges are real barriers, regions are divided (in s0, s2, s3, s6, s7 and s8 they divide the land into three to five parts), and roads through passes become something history builds. The cost is that a walker crosses no range before history has run (detour p90 of 1.5× at 1024, up to 10.6× in s17). If a few natural passes are wanted without changing v13's character, the cheapest change I see is a notch term in E5: one more low-frequency noise `np` (2 to 3 cycles, its own stream) and `unit = unit × (1 − notch_depth × fade(1 − |np| / w_n))` where `|np| < w_n`. The notches fall only where the zero line of `np` crosses a belt, so there are one to three per range, about 500 to 800 m wide, with crests untouched elsewhere. Erosion then deepens them into cols. It changes every byte (E6's quantile scale shifts slightly) but no outline and no range layout. I would not hold the merge for this. If Paul wants it, it should be a proposals entry and a spec section of its own, so that history can name the passes.

**A4. 512 steepness (engineer's item 7, K6).** `step_p999` 76 to 138 m, and 37% of land steeper than 35° (median). That is steep, and 512 s0 and s2 have walkable lowland split into separate pieces. Under Paul's decision cliffs in high ground are allowed, and 512 is the small test size. Accept it with a hard bound of `step_p999` ≤ 150 m at 512, and keep the engineer's sqrt height scaling as a documented option, not in the spec.

**A5. Coasts and lowland texture unchanged (D6, D7, D8).** They were should-fix in my v13 review, and fixing them changes every outline, which Paul did not ask for. Leave them for a later P1 revision.

## 5. Status of the v13 review's defects

| v13 defect | v15 | Note |
| --- | --- | --- |
| D1 unwalkably steep mountains | **No longer applies** as a defect | Unchanged in the numbers (median slope above 500 m 55° at 1024), but Paul accepted it. Covered by A3 for passes and S5 for low steep bands. |
| D2 polygonal fill plates with parallel channels | **Fixed** | Plates and channels are gone (`v15-2048-s0-lowland-1to1.png`). Replaced by the smaller S1 (pads) and S2 (one straight river across a pad). |
| D3 axis-aligned striping | **Mostly fixed** | Residuals in S4 (s5 west flank, s4 north flank); the s5 diagonal crease remains. |
| D4 mountain-light worlds | **Fixed in area, not in shape** | S3; the 2048 s1 ring is B1. |
| D5 flanks without valleys | **Partly fixed** | 6× cuts branching side gullies; they are slots, and the aprons between them stay smooth (section 3). |
| D6 uniform lowland, no flats | **Left** | `lowland_flat_share` median 1.7% at 1024. A5. |
| D7 edge-aligned and falloff coasts | **Left on land; fixed at sea** | E5.3 removes the circular sea shelf; the outlines are unchanged. A2 and A5. |
| D8 too little coast detail | **Left** | A5. |
| D9 arcs and rims | **Left, and the floor adds rims** | s12, s13, 2048 s4 (S3). |
| D10 rectilinear river segments | **Mostly fixed** | 2048 s0 lowland and 1024 s1 are clean; new instance S2. |
| D11 wedge lake; 512 without lakes | **No longer applies** | The s3 wedge basin is now a pad (S1); every 512 world has a lake. |

## 6. What to do before the spec

1. B1: scale `lake_max_area_m2` with map area, and check that 2048 s1's ring holds a lake; re-render 2048 s0 to s4 and re-record the statistics.
2. Look at S5's low steep bands (1024 s15, s18; 512 s0) and say what they are.
3. Write the spec from `v15-definition.md` with the amended purpose sentence. S1 to S4 can be fixed during implementation or as tuning afterwards; A1 to A5 go into the spec as known limitations.

## 7. Images and scripts made for this review

Under `~/.cache/the-unmapped/out/p1-elevation-review/critic-v15/`:

- `zoom-ero-s0-1x-vs-6x.png`, `zoom-ero-s2-1x-vs-6x.png`, `zoom-ero-s5-1x-vs-6x.png`, `zoom-ero-s7-1x-vs-6x.png`, `zoom-ero-s8-1x-vs-6x.png`: rows of `v15-erosion-levels-1024.png` at 2×, 1× left and 6× right.
- `pads-1024-s0-mtn.png` (window x 416 to 736, y 416 to 736) and `pads-1024-s3.png` (window x 200 to 600, y 500 to 900): v13, v13 with smooth cells in red, v15, v15 with smooth cells in red. Smooth = above 20 m, slope under about 2°, and the largest 4-neighbour Laplacian in a 5 × 5 window at most 0.6 m.
- `ocean-contours-1024-s4.png`: ocean depth in 20 m bands, v13 left, v15 right.
- `scripts/overlay.py`: the pad overlay renderer (numpy and PIL, reads the `r4` grids).
