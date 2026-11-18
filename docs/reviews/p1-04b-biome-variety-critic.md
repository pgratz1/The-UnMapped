# World critic: merge-gate review of P1-04 "biome variety" (generator 0.9.0 candidate)

Date: 2026-11-18. Reviewer: world critic. Candidate CLI: `~/.cache/the-unmapped/build/agent-a4d1659be5a5d605e/linux-gcc-release/tools/worldgen-cli/worldgen-cli` (worktree `agent-a4d1659be5a5d605e`, head `b19c4d9`). The shipped palette was checked against the biome table in the worlds.

**Sample: 76 worlds.**

- The engineer's 58: 512 s0–19, 1024 s0–29, 2048 s0–7.
- 18 of my own, in `critic/w/`:
  - 2048 s8, 13, 15, 27, 28, 51, 73, 88;
  - 1024 s51, 73, 87, 88, 89, 94;
  - 512 s51, 73, 87, 88.
- I chose the extra seeds from a scan of 100 seeds (30–129) for the coldest and warmest offsets. Seeds 73 and 88 are at −2.0 °C, seeds 51, 87 and 94 at +1.9 °C, and seed 0 (in the engineer's set) at +2.0 °C.
- Totals: 24 worlds at 512, 36 at 1024, 16 at 2048.

**Material.** Everything is under `~/.cache/the-unmapped/out/p1-04b-impl/critic/`:

- `img/`: sheets.
- `crops/`: each crop shows 0.8.0 composite | 0.9.0 composite | 0.9.0 flat, and the file name gives world, origin, size and zoom.
- `variant/`: one data-only experiment.
- `tools/`: `m.py`, `width.py`, `fert.py`, `crop.py`, `sheet.py`, `pal.py`.
- Metrics: `m-eng2.jsonl` and `m-cri.jsonl` (per world), `summ-all.txt` (summary), `width-*.jsonl` (ribbon shape), `fert.jsonl` (fertility), `pal-shipped.txt` (colour differences).

## 1. Verdict

**Merge as 0.9.0 as is. Nothing blocks.**

- The nine biomes read as nine kinds of country at 1024 and 2048, and as seven or eight at 512.
- Every gain claimed over 0.8.0 holds in the larger sample: lee and windward sides, the dry heart of the rain shadow, wooded river corridors, fen basins.
- The three fixes asked for before the spec:
  - **Palette:** done and it works.
  - **Fen outlines:** mostly fixed. About 1 in 7 large fens is still a smooth blob or ring.
  - **Heath band:** not fixed. Of the three, it is the one that stands out most at sheet scale. It is the first follow-up.

## 2. What works

- **Range sides differ, and the temperature offset shows.** At 0.8.0 every range had a uniform mauve collar. Now ranges have a rust lee and a mauve windward side. See `img/before-after-1024-s0-s8-s13-s27.png`:
  - s0 (+2.0 °C): the snowfield becomes a patchwork of moor and heath with a summit cap.
  - s8 (−1.9 °C): the snowfields are longer, and the dry NW plain is steppe right to the coast.
- **Steppe is the most successful biome.** It is present in 76 of 76 worlds, with median share .094 / .084 / .129 (512 / 1024 / 2048). It dominates 11%, 10% and 15% of 88-cell blocks.
  - Its patches are solid: 55–73% of steppe cells lie more than 3 cells inside a patch, and 0.2–0.4% of steppe is in patches under 16 cells.
  - Windward coasts have almost none: at most 1.2% of a world's steppe at 1024 and 0.04% at 2048.
  - Example: `crops/heath-1024-s8-x60-y100-420x420-1x.png`, a straw plain with wooded rivers running through it.
- **River corridors are restored.**
  - Tree cover within 4 cells of class ≥ 4 lowland rivers, against the lowland average:
    - 512: .59 against .49;
    - 1024: .55 against .55;
    - 2048: .55 against .59.
    - At 0.8.0 the 1024 figure was .44 against .58.
  - Wet woodland is never more than 2.3% of land.
- **Fen is real country at 2048.**
  - Present in 16 of 16 worlds, from 2.3% to 7.6% of land.
  - The median 2048 world has 23 fen patches of 1,000 cells or more.
  - The 2048 s1 basin is 45,339 cells (about 11.6 km²), a fenland laced by rivers, marsh fringes and carr: `crops/fen-2048-s1-x990-y950-440x490-1x.png`.
  - At 1024, 33 of 36 worlds have a fen of at least 1,000 cells.
- **Regions differ more.** Distinct block-dominant biomes per world (median):
  - 4 / 6 / 7, against 3.5 / 4 / 4 at 0.8.0;
  - in 8 of 16 worlds at 2048, all seven possible dominants appear.
  - Neighbouring blocks with different dominants: .60 / .43 / .31.
- **The colours work.**
  - Smallest colour difference between touching natural biomes for normal vision: 15.2, for fen against heath, which almost never touch. The pairs that touch often are all at least 16.
  - Under simulated deuteranopia and protanopia every biome can still be told apart in `img/cvd-1024-s13-2048-s1.png`. Heath turns khaki, moor blue-grey and fen grey, and lightness separates the rest.
- **No noise.**
  - Speckle (land in patches under 16 cells): 0.3% at all three sizes.
  - Patches under 4 cells: 0.1%.
  - At walking scale there is no mixed biome soup at 2048 (`crops/soup-2048-s13-x600-y600-420x420-1x.png`).
- **No biome sits somewhere it should not:**
  - **Fen on slopes:** at most 1.0% of a world's fen has more than 15 m of relief within 48 m (median 0).
  - **Fen near uplands:** fen within 160 m of upland is at most 4.5% of a world's fen at 512, 0.08% at 1024 and 0 at 2048.
  - **Steppe on windward coasts:** the 512 cases are sheltered peninsulas (see A6).
- **The extremes are good worlds.**
  - 1024 s88 (−2.0 °C) is one of the better maps in the sample: `crops/cold-full-1024-s88-x0-y0-1024x1024-1x.png`.
  - 2048 s73 (−2.0) has two snow ranges over a steppe and grass interior.
  - 2048 s51 (+1.9) is a long low range with a huge southern steppe and grass lowland.
  - See `img/2048-critic-8-composite.png`.

## 3. Findings

### Blockers

None.

### Should fix (follow-ups)

**S1. Heath is still a contour stripe, and the new rust colour makes it the most visible artefact at sheet scale.**

- **Where:**
  - 1024 s8, NW face, roughly x 100–400, y 180–420. A rust ribbon about 8–10 cells (150 m) wide runs the full length of the range between steppe and snow. A second ribbon rings the southern massif.
  - Crop: `crops/heath-1024-s8-x60-y100-420x420-1x.png`.
  - Also 512 s5 (`crops/heath-512-s5-x100-y80-330x330-1x.png`) and 1024 s13.
- **How often:** worlds where more than 25% of heath lies in ribbons (patches under 14 cells wide and more than 8 times longer than wide):

  | Size | Worlds | Every cold world (≤ −1.5 °C)? |
  | --- | --- | --- |
  | 512 | 20 of 24 | yes, 5 of 5 |
  | 1024 | 16 of 36 | yes, 6 of 6 |
  | 2048 | 8 of 16 | 4 of 5 |

  - Heath ribbon share correlates with offset at −0.24 at 1024 and −0.40 at 2048: the colder the world, the more ribbon.
  - Moor was the same collar at 0.8.0 and still is (moor ribbons in 19 of 24, 13 of 36 and 4 of 16 worlds).
- **Why:**
  - The band runs between two isotherms (≤ 4 °C, and the treeline), so on an even slope it has a constant width.
  - Elevation jitter could not break it, and that was correctly dropped.
  - A cold world compresses it, because it pushes the treeline down toward the 300 m floor.
- **What I tried:** a second heath rule (≤ 6 °C, rain < 720 mm, from 150 m), data only, on 1024 s0, s8, s13 and 512 s5 (`img/variant-heath-wider-lee.png`).
  - The ribbon metric falls from 0.6–0.9 to under 0.1.
  - But the eye sees a wider collar of the same shape. **I do not recommend it.**
- **Smallest change that can work:** give the upland rules their own large-scale temperature noise. For example, a long-wavelength term of about ±1.5 °C, with a wavelength of 1–2 km, used only by moor and heath (one field and one stream). That makes the 4 °C edge wander up and down the slope instead of following a contour.
- This is a code change and a spec item, so it belongs after the merge.

**S2. One large fen in seven is still a smooth blob or ring.**

- **How often:** of 65 fens of 3,000 cells or more (all at 1024 and 2048), 9 have solidity ≥ 0.7 and a perimeter ≤ 2.6 times that of a disc. By eye these read as geometric. Examples:
  - **1024 s9**, x 546–609, y 763–839: a delta fen with a convex, egg-shaped outline (solidity .91). Crop: `crops/fen-1024-s9-x500-y730-160x140-3x.png`.
  - **2048 s3**, x 959–1040, y 1161–1272: a rounded block across a confluence. Crop: `crops/fen-2048-s3-x930-y1140-150x160-2x.png`.
  - **2048 s4**, x 1017–1089, y 547–640: a ring around a forest island. Crop: `crops/fen-2048-s4-x990-y530-130x130-2x.png`.
  - **2048 s5**, x 954–1093, y 551–706: the disc from the earlier review. It now has dry islands, but its long SE edge is still a smooth arc parallel to the range foot. Crop: `crops/fen-2048-s5-x920-y530-220x200-2x.png`.
- **The rest read well:**
  - The large fens hold a median of 4 dry islands each.
  - The 2048 s1, 2048 s7 and 1024 s13 basins follow their rivers.
- **"Rounded with dry islands" is acceptable.** It reads as a raised bog or a carr basin. The ring in 2048 s4 is the only shape that looks drawn.
- **Smallest change:** none needed for 0.9.0. If wanted later, multiply the wet share by the same long-wavelength noise as S1, which roughens the outer edge without moving the fen. The earlier review's per-cell jitter was tried and did not break the discs, because it was too fine-grained.

**S3. Small fen pods in forest at 2048.**

- **What:** compact oval fens of 200–4,000 cells (0.2–1 km across), each in a teal marsh halo and set in forest or carr. They give 2048 forests a spotted look at sheet scale.
- **Example:** 2048 s88, x 1050–1450, y 300–800: `crops/cold-2048-s88-x1050-y300-400x500-1x.png`.
- **How often:** 3 to 22 per 2048 world (median 13), about one in five mid-sized fens. At 1024, 0 to 9 per world.
- **Judgement:** close to acceptable. Forest bogs are real, and at walking scale they are good places.
- **Smallest change, if Paul dislikes the spots:** require a minimum fen patch size at 2048 (fold patches under N cells into marsh). That is a post-pass, so a code change. Alternatively raise `wet_scale_2048` from 1.5 to 1.6 and check that the basins survive.

### Acceptable (record)

**A1. Fen at 512.**

- Under 0.1% of land in 5 of 24 worlds; a patch of 1,000 cells or more in only 4 of 24.
- The cause is the terrain: 512 worlds are steep.
- History must keep the fallback from fen to marsh to wet woodland (spec Appendix A).

**A2. Steppe strip at range feet.**

- Share of steppe within 500 m of upland (median): .59 at 512, .37 at 1024, .17 at 2048.
- Worlds where more than 25% of steppe lies in ribbons: 4 of 24, 6 of 36, 0 of 16.
- The worst case is a 2–4-cell straw sliver on a valley side, 512 s13 x ≈ 330, y 220–300 (`crops/ww-512-s13-x250-y140-180x290-2x.png`).
- It sits in the deepest shadow, so it is physically right.

**A3. River lace at 2048.**

- Wet woodland is 1.5–2.3% of land in 16 of 16 worlds. Every class ≥ 4 river has a dark sleeve with a teal fringe, which reads as thick dark rivers on the sheet (`crops/soup-2048-s13-x600-y600-420x420-1x.png`).
- Sleeves are no wider than at 1024: 2.3 wet-woodland cells per class ≥ 4 river cell, against 2.5. 2048 simply has more such rivers.
- It is right at walking scale.
- To thin it later: a per-size `river_dist_class_min`, 5 at 2048.

**A4. Marsh is thin.**

- Only .22, .25 and .38 of marsh cells have a full ring of marsh neighbours. The rest are on an edge: ribbons 1–3 cells wide.
- 4–10% of marsh lies in patches under 16 cells. It is under 0.5% of land in 3 of 24 worlds at 512 and 2 of 36 at 1024.
- On the sheet it shows only as a fringe. That suits a biome that is a crossing feature.

**A5. The lee-coast forest rim stays.**

- Forest share of the lowland coast band (within 1 km of the sea):
  - lee coasts: .21, .33 and .49;
  - windward coasts: .80, .97 and 1.00.
- At 2048 the lee rim is no greener than the interior (.48).
- The coast bonus of 40 mm thinned it. More would need the climate re-calibration that the earlier review advised against.

**A6. "Steppe on a wet coast" at 512.**

- In 512 s17 (29% of its steppe) and the seed-phrase world (26%), my test counts steppe as windward-coast steppe because sea lies within 1.5 km upwind.
- In s17 (`crops/full-512-s17-x0-y0-512x512-1x.png`) it sits on the narrow south peninsula, sheltered by the eastern lobe across a narrow bay. Plausible.
- Nowhere at 1024 or 2048.

**A7. The temperature offset: summit-cap and big-snow worlds.**

- Alpine share falls by 2.6, 2.1 and 0.8 points per °C (correlation with offset −0.82, −0.73, −0.43).
- Only seed 0 (+2.0 °C) is a summit-cap world: 0.3% alpine at 512 and 0.9% at 1024. At 2048 it has 5.4%, because its peaks are taller.
- Cold worlds with more than 15% alpine: 512 s8, s9 and s13, and 1024 s8.
- Narrow cold 2048 ridges (s13, s88) are white blades with almost no heath (0.2–0.3% of land).
- All of these are good, distinct maps.
- But terrain decides more than the offset. Cold 1024 s88 has 9.3% alpine and warm 1024 s51 has 6.6%. Only the extremes are recognisably "a cold world" or "a warm world".
- The offset changes the uplands only:
  - steppe, grassland and fen shares do not correlate with it (|r| ≤ 0.4 with slopes under 0.6 points per °C);
  - forest gains 1.2–3 points per °C, because warm worlds carry trees higher.
- That is what the spec says, and Paul has accepted it.

**A8. Heath islands inside moor.**

- Heath patches of 16 cells or more that are at least 90% ringed by moor or alpine: median 0, 3 and 4.5 per world (512 / 1024 / 2048); at most 22 (2048 s2).
- They are at most 27% of a world's heath (1024 s20).
- Looked at, they are lee faces of spurs inside a massif (`crops/heathisl-1024-s20-x300-y150-460x480-1x.png`). Plausible.

**A9. Wet woodland in mountain valleys at 512.**

- In 512 s13, 75% of the wet woodland lies within 500 m of alpine, in the inner valley at about 212 m (`crops/ww-512-s13-x250-y140-180x290-2x.png`). It is valley-floor carr, and correct.
- Wet woodland on ground with more than 15 m of relief within 48 m: median 15% at 512, 6% at 1024, 2% at 2048. 512 is steep everywhere.

**A10. Blighted waste colour, for later.** `blighted_waste` keeps 0x605446. Under deuteranopia it is 4.8 from forest, and 7.6 from fen in normal vision under shading. History will place it next to both. Use 0x4A403C (or darker) when history ships.

## 4. Patterns across seeds (76 worlds; median unless stated)

| | 512 (24) | 1024 (36) | 2048 (16) |
| --- | --- | --- | --- |
| Natural biomes ≥ 1% of land | 7 | 8 | 9 |
| Distinct block dominants | 4 | 6 | 7 |
| Neighbouring blocks with different dominants | .60 | .43 | .31 |
| Speckle (patches < 16 cells) | .3% | .3% | .3% |
| Fen < 0.1% of land | 5 worlds | 1 | 0 |
| Fen patch ≥ 1,000 cells | 4 worlds | 33 | 16 |
| Heath ribbon share > .25 | 20 worlds | 16 | 8 |
| Large fens (≥ 3,000 cells) that look geometric | – | 1 of 5 | 8 of 60 |
| Fen pods in forest, per world | – | 0–9 | 3–22 |
| Lee / windward coast forest share | .21 / .80 | .33 / .97 | .49 / 1.00 |
| Trees near class ≥ 4 rivers / lowland average | .59 / .49 | .55 / .55 | .55 / .59 |
| Blocks with fertility ≥ 0.7 | .48 | .42 | .32 |
| Largest connected cluster of fertile blocks (blocks of 2 km²) | 4 | 14 | 33 |
| Worlds whose fertile core is ≤ 2 blocks | 7 | 0 | 0 |

## 5. Resources and regions (P1-05, P1-06)

**Fertile cores.**

- Every world at 1024 and 2048 has a connected fertile core: at least 5 and 10 blocks, median 14 and 33.
- At 512, 7 of 24 worlds have no fertile core larger than 2 blocks. That is the 0.8.0 limit of steep 512 worlds, unchanged.

**Fertile land at 2048.**

- The share of fertile blocks falls to .32 (0.8.0: .48).
- Raising steppe fertility recovers only part of it:

  | Steppe fertility | 512 | 1024 | 2048 |
  | --- | --- | --- | --- |
  | 0.6 (shipped) | .50 | .42 | .32 |
  | 0.7 | .54 | .45 | .36 |
  | 0.75 | .54 | .47 | .39 |

- The rest of the loss is structural:
  - forest is exactly 0.7, so any forest block with a few percent of fen, marsh or carr drops below the threshold;
  - fen, marsh and wet woodland together are 11% of 2048 land.

**For P1-05:**

1. Do not judge fertile land with a hard 0.7 cut that equals forest's value. Use the region mean, or 0.65.
2. Consider steppe 0.7 (grazing and dry grain).
3. Give wet woodland and marsh value as resources (timber, fowl, reed, peat) rather than as fertility, so wetland-rich 2048 regions are not simply "poor".

**Marginal land is now plural:** dry plain (steppe), wet basin (fen), dry upland (heath), wet upland (moor), and the snow line. A frontier is no longer only "up the mountain".

**Regions (P1-06).**

- Distinct neighbours: 6–7 dominant kinds per world at 1024 and 2048, 4 at 512.
- Fen dominates 1.3% of 2048 blocks and 0.2% of 1024 blocks.
- Fen should therefore be a region kind at 2048, and a feature inside a region (a "fen" sub-area) at 1024.
- Heath and moor dominate 2–3% of blocks each. Upland regions will often be named for the snow (alpine dominates 6–10% of blocks), so P1-06 should merge heath, moor and alpine into one "highlands" region when no single one dominates.

## 6. Follow-ups, in priority order

1. **S1 heath stripe:** long-wavelength temperature noise of about ±1.5 °C used only by the upland rules (one field, one stream; spec and code). It is the most visible artefact, and P1-06 will draw region borders along it.
2. **P1-05 spec notes (no change to P1-04):**
   - a fertility threshold below forest's value;
   - steppe 0.7;
   - wetland resources.
3. **S3 fen pods at 2048:** a minimum fen patch size at 2048 (fold into marsh), only if Paul finds the spotting ugly. Try `wet_scale_2048` 1.6 first, with data only.
4. **S2 fen outlines:** tie them to the S1 noise later, if wanted. Otherwise leave them.
5. **A3 river lace:** a per-size `river_dist_class_min` (5 at 2048), only on request.
6. **A10:** blighted waste colour 0x4A403C, with the history stage.

## 7. Six images for Paul

1. `critic/img/before-after-1024-s0-s8-s13-s27.png`: 0.8.0 against 0.9.0. Shows the warm (+2.0) and cold (−1.9) worlds, and the lee and windward sides.
2. `critic/img/2048-critic-8-composite.png`: eight 2048 worlds, including both extremes of the offset. Shows steppe hearts, fen country and white cold ridges.
3. `critic/crops/fen-2048-s1-x990-y950-440x490-1x.png`: the 2048 fen basin. The best new country.
4. `critic/crops/heath-1024-s8-x60-y100-420x420-1x.png`: a steppe lee plain with wooded rivers (good), and the heath contour stripe (S1, bad).
5. `critic/crops/fen-2048-s4-x990-y530-130x130-2x.png`: a ring fen, the worst of the geometric fens (S2).
6. `critic/crops/soup-2048-s13-x600-y600-420x420-1x.png`: 2048 walking scale. A steppe heart, river carr and marsh fringes, with no soup but heavy river lace (A3).
