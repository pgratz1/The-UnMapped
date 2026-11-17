# World critic review: P1-03 climate (generator 0.7.0 candidate)

Date: 2026-11-17. Reviewer: world critic. Build: `agent-a444690661282c5a3/linux-gcc-release` (`worldgen-cli 0.7.0 schema 2`).

## 1. Verdict

**Climate can merge as it is.** I found no blockers. The rain field is clean at full resolution, and rain shadows sit on the correct side and read clearly at 1024. The one consequential weakness is that rainfall depends on map size. P1-04 has to absorb that with **per-size rainfall thresholds**, not with per-world quantiles, and that decision belongs in P1-04's recalibration, not in this merge. Three changes should follow soon after merge: store the wind direction, revisit the coast bonus, and ask Paul about a per-world temperature offset.

## 2. Material examined

- All of `final/`: contact sheets at 512 and 1024 (seeds 0 to 9), the 2048 sheets for seeds 1 and 5, the eight-wind sheet for seed 8, per-world PNGs, reports and `stats.md`.
- I generated extra worlds with the candidate CLI: 512 and 1024 seeds 10 to 17, and 2048 seeds 2, 3, 4 and 7 (`critic/worlds/`). That makes **18 worlds at 512, 18 at 1024 and 6 at 2048.**
- I decoded every layer with numpy (codec 1 as in P0-02 7). I inferred the wind of each world as the direction whose upslope term best correlates with rainfall. It matched the spec's C2 list for all 20 worlds where that list gives the wind.
- **Biome trial:** P1-04 B4 with the shipped rules, applied to the real layers. Height above drainage (G7) is exact, computed by following `flow_dir` to the first river, lake or ocean cell. Slope is the G8 central difference. The B3 jitter is a substitute: value-noise fbm at 600 m wavelength, 3 octaves, ±0.8 °C and ±15 %. Shares move by at most a few points with or without the jitter.
- **Diagnostics:** high-pass images of log rainfall (rainfall minus its 13-cell box mean, contrast ×1500, coast in red), to make streaks and grid texture visible that the palette hides. I also took profiles across ranges.

## 3. What works

- **Shadows on the correct side, believable in size.**
  - Profile s5-2048, column x = 1050, wind toward N: plain at y 872 to 824 gets 590–740 mm. The windward face (y 812 to 764) rises 1,320 → 2,585 → 2,019 mm. Peak 998 m at y 752. The lee is 530–570 mm from the crest northward.
  - At 1024, windward lowland averages 931 mm against 671 mm in the lee.
  - In the biome trial at 1024, grassland covers **35–72 % of lee lowland and 0–26 % of windward lowland** in all 18 worlds. Seed 8 at 1024 (`critic/biome-s8-1024.png`) reads as a forested south-east and a dry grass lee behind the main ridge, with a second grass pocket in the valley between the two ranges. A player would understand that map at a glance.
- **Clean at full resolution.**
  - The comb bands across the wind are gone.
  - Crops at native resolution (`critic/crop-s0-1024-rain-x250-750-y250-750.png`, diagonal wind; `critic/crop-s5-2048-rain-x600-1600-y150-1150.png`) show smooth, blobby fields with no visible grid texture.
  - Cell-scale texture on wet slopes (land above 1,500 mm, deviation from the 3×3 mean): p99 is 0.3–0.5 % in 9 of 10 worlds at 1024. Orthogonal and diagonal winds are indistinguishable (SW 0.42 %, NW 0.34 %, E 0.52 %, S 0.43 %).
- **The eight-wind sheet** (`final/contact-wind8-s8.png`) shows eight different islands from one terrain, with the same character under diagonal and orthogonal winds.
- **No capping.** Land p100 is 2,884–8,671 mm. The 250 mm floor is never reached; the lowest land value is about 490 mm.

## 4. Findings

### Blocker

None.

### Should-fix (none needs to hold the merge)

**S1. Rainfall size dependence reaches the biomes and splits the world sizes into different ecologies.** Results of the biome trial with the shipped fixed thresholds (forest ≥ 700 mm, marsh ≥ 500 mm). "Free land" is land that is neither alpine, moor nor marsh, so the forest threshold decides it.

| N | worlds | forest | grassland | open share of free land | worlds with grassland < 5 % |
| --- | --- | --- | --- | --- | --- |
| 512 | 18 | 51–69 % (median 64) | 2–13 % (median 6) | 0–15 % | 7 |
| 1024 | 18 | 43–56 % (median 51) | 14–26 % (median 19) | 19–36 % | 0 |
| 2048 | 6 | 27–35 % (median 31) | 33–42 % (median 37) | 49–60 % | 0 |

- At 512 the shadow shows in the rain field but not in the biomes. Lee lowland is 707–902 mm, above 700, so it stays forest.
- At 2048 even windward lowland is 7–55 % grassland, because the interior dries with path length. Example: s4-2048, wind toward SE, range on the downwind coast. The whole windward plain is about 600 mm and beige (`critic/contact-2048-extra.png`).
- Grassland carries fertility 1 against forest 0.7, so history would see systematically different farmland at each size.
- This is P1-04's to fix (recommendation in section 6). P1-05 should also note that its absolute fresh-water and timber scores (proposals item 4, "a poor world is poor") will make every 2048 world poorer than every 512 world.

**S2. The wind direction is not stored (P1-03 open question 4).**
- The runtime needs it for weather direction, cloud drift and vegetation lean.
- The viewer and `report` cannot show it.
- Every reviewer has to infer it: I did it by correlation, and the contact sheets carry hand-written labels.
- Add it with the next schema change (P1-04's `biome` table, schema 3), as the spec already suggests. Do not leave it for later than that.

**S3. A coastal forest ring on every coast, lee coasts included.** The share of free land that is forest falls with distance from the coast:

| Distance from coast | 1024 (seeds 0–9) | 2048 (seeds 1, 5) |
| --- | --- | --- |
| 0–300 m | 89 % | 77 % |
| 750–1500 m | 70 % | 58 % |
| 1500–3000 m | 57 % | 41 % |
| beyond 3000 m | – | 27 % |

- The 150 mm coast bonus applies equally in every direction. Together with the rain noise, it puts a forest rim around nearly every world, including the lee coasts that should be the driest ground: s8-1024 north-west coast, `critic/biome-s8-1024.png`. That blurs the rain shadow where it should be strongest.
- The cause is tuning, so it can be fixed during P1-04's calibration. Either halve `coast_rain_mm` or weight the bonus toward windward coasts.

**S4. Temperature is the same in every world and is mostly height (proposal for Paul, not a change to this stage).**
- Elevation explains 88–93 % of land temperature variance in every 1024 world.
- The latitude term would matter, because the moor line at 4 °C sits about 600 m higher at the south edge than at the north. But uplands are concentrated at mid-map: the north third holds 0–13 % upland in the 1024 worlds. So the north–south span changes almost nothing in practice.
- The noise term adds almost nothing: residual standard deviation 0.13–0.23 °C.
- Every world therefore has the same treeline at about 1,000 m. A per-world sea-level offset (for example −2 to +2 °C) would make some worlds moor-heavy and some mild, which is more variety than any change to the wind distribution would give. It is a data and design question, for Paul in the "Second-pass geography" proposals entry.

### Acceptable

**A1. Lee streaks and diagonal lines are visible only in the high-pass images.**
- The ×1500 high-pass shows faint lines along the wind:
  - seed 0 at 1024: diagonal lines inland, around (330,620) to (420,750);
  - seed 5 at 2048: vertical lines from coast bends, around x 740–760, y 180–300, and x ≈ 1400, y 300–600;
  - seed 0 at 1024: streaks over the sea downwind of coasts, at (690,880) and (930,600) (`critic/hp-s0-1024.png`, `critic/hp-s5-2048.png`).
- Their real amplitude is a few mm. The profile across s5-2048 row 350, x 1440–1530, is monotone with no ripple. None of these lines shows in the palette renders or in the biome trial. Accepting them is right.

**A2. Grid crosshatch on wet slopes, also visible only under high-pass.** It appears only at ×1500 (`critic/hp-s8-1024.png`, slopes around (250–500, 380–700)), and its amplitude is below 1 %. Accepting it is right.

**A3. Passes do not channel moisture.**
- Lee rain 20–60 cells behind each col, compared with 30 cells to either side, over the 14 passes of the 1024 worlds, shows no consistent signal in either direction. For example, s8 pass (517,313): 648 mm behind the col against 614 and 618 mm to either side.
- At annual-mean level a 300–500 m notch would not make a lowland green in reality either. A "weather pours through the pass" effect belongs in runtime weather, not in this layer. Accepting it is right.

**A4. The shoreline jumps on windward cliff coasts.**
- At s2-1024 around (895,261), wind toward W, ocean gets about 920 mm and the first land cell gets 3,900–4,300 mm. The ocean rule ignores the rise of the smoothed surface.
- Physically the rain should start a little offshore. But nothing reads ocean rain, and on land the front is correct, so this is cosmetic.

**A5. The sea is wetter than the land in every render.**
- `d = max(0, coast_dist)` is 0 on every ocean cell, so all ocean gets the full 150 mm bonus (about 950 mm). The renders therefore show a beige land on a blue-green sea, and lowland gradients look washed out against it.
- This is as the spec defines it and only cosmetic. If it bothers Paul, mask the ocean in the `rainfall` render, or let the bonus fade with |`coast_dist`| over the sea.

**A6. Peak rain of 5–8.7 m on windward faces.**
- 61–74 % of land at 3,000 mm or more is moor or alpine, so no biome rule reads these values.
- The values are extreme for 1.5 km peaks but invisible to later stages, except as timber or fresh-water scores on the forest strip at the foot of the wet band.

**A7. Rain noise is what makes the 2048 ecotone.** Where lowland rain sits near the threshold over large areas, the ±40 % rain noise and the biome jitter decide forest against grass. The result is a broad mottled parkland (`critic/biome-s5-2048.png`, west centre), which I find interesting rather than a defect.

## 5. Patterns across seeds

- **Shadow strength depends on where the range lies.** The lee lowland share (lowland reached within 400 cells downwind of ground at 800 m or more) is 1–24 % of land.
  - Worlds with almost no lee: 512 seeds 1, 4 and 12 (≤ 5 %); 1024 seed 4 (1 %); 2048 seed 4 (2 %). In each, the range lies on the downwind coast, and the climate is essentially "coast wet, interior slightly dry".
  - That is 5 of 42 worlds: realistic, but those are the dullest climates.
- **Variety between worlds mostly comes from the wind.** Every world shows the same picture: one wet band on the windward faces, a dry lee, wetter coasts. The wind direction and the layout of the ranges are what make worlds differ.
  - I saw no "wet-west island with a dry interior basin" at 512.
  - At 1024 that kind of contrast appears in about half the worlds (seeds 0, 2, 5, 7, 8, 9, 10, 17).
  - At 2048 the dry interior from path length dominates every world.
  - Uniform winds are right; the variety that is missing is temperature (S4).
- **Upland biomes:** alpine 3–12 % at every size; moor 5–28 % at 512, 5–21 % at 1024, 3–9 % at 2048. Alpine and moor are almost absent from the north third of each map, because the uplands sit at mid-map.
- **Marsh in the trial:** 3–11 % at 512, 5–17 % at 1024, 14–28 % at 2048. This comes from height above drainage and slope; the 500 mm rule hardly binds. It is a P1-04 calibration issue, not a climate one.
- **Of land at 1,500 mm or more,** forest is 29–52 %, moor 33–37 % and alpine 14–34 %. The strongest climate signal, the very wet windward faces, has no biome of its own. The runtime can still use `rainfall` directly for vegetation density and moss.

## 6. Recommendation for P1-04 thresholds

Use **per-size thresholds**: one scale factor on every rule's `rain_min_mm` and `rain_max_mm`.

| Option | Result in the trial | Variety between worlds |
| --- | --- | --- |
| Fixed mm (700 forest, 500 marsh) | 512 mostly forest (grassland median 6 %, 7 of 18 worlds under 5 %); 2048 more open than wooded | Keeps the absolute meaning of mm, but map size, not the world, decides the ecology |
| **Per size: forest about 820 / 700 / 600 mm, marsh about 580 / 500 / 430 at 512 / 1024 / 2048** (about 700 × (1024/N)^0.22) | Grassland 14–35 % / 14–26 % / 15–24 %, forest 39–52 / 43–56 / 43–50 %, no world under 5 % grassland; the s9-512 lee becomes visible grass and the s1-2048 windward north-east turns back to forest (`critic/biome-trial-sheet.png`) | **Keeps the spread between seeds** caused by range placement and wind (grassland range of about 10–20 points within a size) |
| Rainfall quantiles per world | Grassland fixed by construction in every world | **Removes all variety in biome mix**: a world in a deep rain shadow gets the same grassland as one without a range; `biome_share` stops meaning anything as a statistic; every world gets an "interior grassland" whether its climate supports one or not |

- **Why per size:** it is the only option that removes the size artefact and keeps the climate's own variety.
- **How to implement it:**
  - Add a data field: a three-entry `rain_scale` keyed by map size in `worldgen.biomes`, or one exponent.
  - Keep the thresholds in mm, so resources and history can still read rainfall in mm.
  - Recalibrate on the sweep. 820 at 512 may be slightly high (grassland up to 35 %); 800 is a reasonable start.
- **The alternative on the climate side** (the moisture path as a fraction of the map, open question 6) would fix biomes and resources together. But it weakens the per-km shadow on 2048 maps, and the spec's reasoning against it stands. If P1-05's absolute scores make 2048 worlds look poor, revisit it then.

## 7. The two accepted limitations and the tuning

- **Passes not channelling moisture:** accept (A3).
- **Lee streaks:** accept (A1). Neither is visible in a palette render or in the biome trial.
- **Tuning D against C32** (`rain_oro_mm` 2500 against 3500):
  - In the experiment, C32 raises windward lowland by about 100–230 mm at 512 and 1024 (1,456 against 1,221 at 512, 1,037 against 931 at 1024) and leaves the lee almost unchanged (799 / 680 against 789 / 671).
  - For biomes this matters little: windward lowland is forest under both, and the lee decides grassland.
  - C32 still reached the 10,000 mm cap on one 512 world, and contrast at 2048 barely differs (2.07 against 1.96).
  - **D is the right choice.** If more contrast is wanted later, make the orographic term saturate (for example `oro × g / (1 + g/g0)`) rather than raising `rain_oro_mm`. Lowland contrast would then rise while peaks stay bounded.

## 8. Suggestions as rule or parameter changes

1. P1-04: per-size rainfall thresholds (section 6). Also re-measure marsh at 2048 (14–28 % in the trial).
2. Schema 3: store the wind code, for example `meta` or a `climate` row with `wind_toward`.
3. Climate tuning, at P1-04 calibration time: `coast_rain_mm` 150 → about 75, or a windward-weighted coast bonus, to weaken the coastal forest ring on lee coasts.
4. Proposal for Paul: a per-world sea-level temperature offset (for example a uniform draw in [−2, +2] °C from a new stream) for variety in treeline and moor.
5. Optional: drop `temp_noise_amp_c`, or raise it to about 2. At 1 °C it is invisible: residual standard deviation about 0.17 °C.
6. Optional, cosmetic: mask ocean in the `rainfall` render, or fade the coast bonus over the sea.

## 9. Images

Under `~/.cache/the-unmapped/out/p1-03-climate/`:

| Image | What it shows |
| --- | --- |
| `critic/biome-trial-sheet.png` | P1-04 rules on real layers: s9-512 at 700 / 820 mm, s3-1024 and s8-1024 at 700, s1-2048 at 700 / 600 |
| `final/contact-rainfall-1024.png` | ten worlds, winds labelled |
| `final/contact-wind8-s8.png` | one terrain under eight winds |
| `critic/crop-s5-2048-rain-x600-1600-y150-1150.png` with `critic/crop-s5-2048-map-x600-1600-y150-1150.png` | full-resolution shadow, wind toward N |
| `critic/contact-2048-extra.png` | 2048 seeds 2, 3, 4, 7: map and rainfall |
| `critic/biome-s8-1024.png`, `critic/biome-s5-2048.png` | full-size biome trials |
| `critic/hp-s8-1024.png`, `critic/hp-s0-1024.png`, `critic/hp-s5-2048.png` | high-pass diagnostics (contrast ×1500) |

The extra worlds are in `critic/worlds/`. The analysis scripts are in the session scratchpad and are not kept.
