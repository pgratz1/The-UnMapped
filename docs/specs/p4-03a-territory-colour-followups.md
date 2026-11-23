# P4-03a Territory colour and border follow-ups

Status: **r1, 2026-11-23, approved by the lead** (r0 draft; r1: goblin 3 is the greener (140, 155, 30), the bound 175 and half strength confirmed, the P4-00 6.3 pointer added; later in r1: the short border labels with tooltips of P4-03 r4 6.4 (the long ones were cut off), and 5 (c) corrected to 23 of 24 lines). Written by the spec writer on `p4/03a-spec`, cut from `origin/main` at d363da3 (P4-03 merged, generator 0.21.0, schema 7). A change spec in the form of `docs/specs/p5-01-followups.md`: it lists the items and names the section of `docs/specs/p4-03-history-view.md` **r4** that holds the normative text. P4-03 stays the single source for the territory image and the history view.

Sources: the world critic's review `docs/reviews/p4-03-history-view-world-critic.md` (findings 1 to 5), the library audit `docs/reviews/p4-03-history-view-library-audit.md` (advisory A1), and the lead's decisions of 2026-11-23 after Paul saw the images. Paul may still adjust the colours: every colour value is in the one table of P4-03 7.2, and the luma bound in P4-03 8.2; a change to either is a change of those two places, the reference and the golden lines of section 5 only.

**No world byte changes.** `GENERATOR_VERSION` is unchanged; schema stays 7. The design doc is not contradicted (it says nothing on map colours or border drawing), so there is no proposals entry. P4-00 6.3 gains a pointer line: its "free land shows the `regions` composite unchanged" holds for the fill only.

## 1. Items

| # | Item | Source | Normative text (P4-03 r4) |
| --- | --- | --- | --- |
| 1 | Goblin entry 3 from tan (205, 160, 60) to olive **(140, 155, 30)** | critic 1 | 7.2 |
| 2 | Pale entries 1 darkened: goblin (250, 235, 110) to **(235, 215, 90)**, elf (150, 230, 150) to **(130, 215, 130)**, human (125, 235, 230) to **(100, 215, 215)**; dwarf, faerie and orc entry 1 unchanged | critic 2 | 7.2, 7.5 |
| 3 | A border with **both sides free** drawn at half strength (each channel of the cell's unbordered colour times 8/16, floored) instead of black | critic 3 | 8.2, 9.2 |
| 4 | Inner borders dimmed by **10/16** instead of 12/16 when the realm colour's luma L is above **175** | critic 4 | 8.2 |
| 5 | Legend: the border rows `border between holders`, `border inside a realm` and `border in free land`, each with its longer explanation as a tooltip; the water rows under their own heading `water` | critic 5 (a), (b) | 6.4 |
| 6 | `render_territory(base, timeline, colours, year, scale[, out])`, used by the CLI and the viewer; the two-vector overloads stay | audit A1 | 9.2, 3.3, 3.4, 5.1 |

Critic findings not taken: 5 (c) (the long legend scrolls; the critic finds it acceptable) and 6 (colour reuse past four realms of one race: not seen, rule k mod 4 kept; to be looked at when P5 adds races).

## 2. Colours (items 1 and 2)

### 2.1 The test for "pale" (item 2)

Luma, integer: **L(c) = floor((77 R + 150 G + 29 B) / 256)** (BT.601 weights in 1/256). An entry is pale when L > 190 (the critic's figure). On the r3 table exactly three entries are pale, all entry 1: goblin 1 (225), human 1 (201), elf 1 (196); the other entries 1 are faerie 186, dwarf 182, orc 154, and no entry 0, 2 or 3 is above 185. Each pale entry is lowered by about 15 per cent in lightness, as the critic proposed, keeping its hue. The test selects which entries change; it is not a post-condition (goblin 1 is still 206 after the change: yellow cannot fall below 190 without nearing goblin 0 or steppe).

### 2.2 Verification against rule D (P4-03 7.3)

D is the red-mean distance of P4-03 7.3, d = floor(sqrt(floor(D / 256))); thresholds C1 (every entry against the background set B) d ≥ 80, C2 (same race) d ≥ 70, C3 (different races) d ≥ 80. B is the ocean ramp t = 0 … 30000, the lake colour, the six river colours and the twelve shipped biome colours (unchanged by P5-12). Computed with integer arithmetic over the whole table (spec writer's check, to be repeated by the test writer's reference):

| Entry | Colour | L | nearest in B | nearest in race | nearest other race |
| --- | --- | --- | --- | --- | --- |
| goblin 3 (new) | (140, 155, 30) | 136 | 99 grassland | 89 goblin 2 | 102 elf 3 |
| goblin 1 (new) | (235, 215, 90) | 206 | 89 steppe | 109 goblin 0 | 103 dwarf 1 |
| elf 1 (new) | (130, 215, 130) | 179 | 114 grassland | 143 elf 3 | 142 human 3 |
| human 1 (new) | (100, 215, 215) | 180 | 96 ocean (depth 0) | 100 human 3 | 143 elf 1 |

Every value passes. Effects on the rest of the table: goblin 0's nearest in race goes from 100 (old goblin 3) to 109; goblin 2's from 128 to 89 (the new goblin 3); elf 3's nearest other race from 154 to 102 (goblin 3); elf 0's from 182 to 180 (goblin 3). The table's minima become: C1 89 (goblin 1 / steppe; was 88), C2 78 (dwarf 0 / dwarf 3, unchanged), C3 90 (dwarf 1 / orc 1, unchanged). The full per-race minima are in P4-03 r4 7.2.

The r0 draft had the lead's first value (150, 150, 30): it passed (101 to grassland, 115 to elf 3) but was only 75 from goblin 2, the closest pair of one race. The lead chose the greener (140, 155, 30) in r1 for the better in-race margin (89); its L is 136 either way, so the luma split of section 3 does not move.

## 3. Borders (items 3 and 4)

Exact text in P4-03 r4 8.2; in short, for a cell of kind 2 (inner border candidate: region v > 0, e ≥ 0, not drawn by steps 3 or 4, a border cell of P0-04 amendment 18 (e)) with other regions O, and a **non-empty** holder vector:

| Holders of v and of every w in O | Pixel |
| --- | --- |
| all the same c ≠ 0, and fill[v] = C | shade_colour(dim(C), factor); dim = channels × 10/16 when L(C) > 175, × 12/16 otherwise, floored |
| all 0 (both sides free) | half(U) = (floor(U.r · 8 / 16), floor(U.g · 8 / 16), floor(U.b · 8 / 16)), U = shade_colour(biome colour of the cell, factor), the pixel the cell has in the `biomes` composite |
| anything else (a holder on at least one side, or c with fill[v] missing) | black |

Cells of kind 0 that are border cells (a lake cell or a river-painted cell on a region border) stay black whatever the holders. A half-strength cell is never next to held land: any cell of free region v whose E or S neighbour is held, or whose W or N neighbour is a held border cell, is black, so the C1 test needs no half-strength colours in B.

**Empty holder vector: stays as today.** With `holders` empty, every kind-2 cell is B (black), so `render_territory(base, {}, {}, k)` still equals the `regions` composite and `render_territory(base, fill, {}, k)` is still P4-01's image. An empty vector means "no holder information", not "every region free"; the timeline overload never passes one (territory_holders has at least the entry [0]). A non-empty vector of zeros (every region free, e.g. year 0) draws every land border between regions at half strength.

**Luma split after items 1 and 2** (bound L > 175, in the gap between 163 and 179 of the new table):

- **10/16** (L > 175): goblin 1 (206), faerie 1 (186), goblin 0 (185), dwarf 1 (182), human 1 (180), elf 1 (179).
- **12/16** (L ≤ 175): elf 3 (163), goblin 3 (136) … every other entry; the largest is elf 3 at 163, the smallest orc 2 at 51.

The critic's bound 190 was measured on the old colours, where it picked exactly the three entries the critic found faint (goblin 1, human 1, elf 1). After item 2 only goblin 1 is above 190, so 190 would give elf 1 and human 1 no help; no bound separates {179, 180, 206} from {182, 185, 186}, and 175 is the nearest that takes in all three named entries. It also takes faerie 1, goblin 0 and dwarf 1, which the critic did not name as faint; a little stronger line on them is the lesser risk. Keys without a row of 7.2 (hashed colours) follow the same L test.

## 4. Library API (item 6)

P4-03 r4 9.2 adds

```cpp
[[nodiscard]] std::vector<std::uint8_t> render_territory(const TerritoryBase& base, const history::Timeline& timeline,
                                                         std::span<const Rgb> colours, std::int64_t year,
                                                         std::uint32_t scale);
void render_territory(const TerritoryBase& base, const history::Timeline& timeline, std::span<const Rgb> colours,
                      std::int64_t year, std::uint32_t scale, std::span<std::uint8_t> out);
```

equal by definition to `render_territory(base, territory_fill(timeline, colours, year), territory_holders(timeline, year), scale[, out])`, with the same errors in the same order. `TerritoryBase::Cell` gains `Rgb half`. `worldgen-cli render --composite territory` calls the timeline form; its options, texts and exit codes are unchanged. The viewer's `HistoryState` keeps the holder vector only (for its redraw test, V6) and draws with the timeline form; it drops its fill vector.

## 5. What changes in recorded and compared output

| Output | Change | Who |
| --- | --- | --- |
| World files, `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, every golden file but `p4-01-timeline.txt` | none; a difference is a defect | validation specialist confirms |
| `render --composite territory` PNGs | change (colours of items 1 and 2, borders of items 3 and 4) | |
| `<preset>-<case>.territory.txt` of `tools/ci/world-hashes.sh` | values change. **Not stored in the repository** (checked: no golden file holds territory image hashes; `world-hashes.sh expect` reads only `p0/p1/p3-worlds.txt`); `compare` across presets must pass | validation specialist |
| `worldgen/tests/golden/p4-01-timeline.txt` | exactly: (a) the two header comment lines naming the P4-03 rules gain "and P4-03a"; (b) the `<case> static` line of every case with a civilization whose colour changes, that is one of race key `elf`, `goblin` or `human` with ordinal k ≡ 1 (mod 4), or of `goblin` with k ≡ 3 (mod 4) (S holds one `colour <civ> <r> <g> <b>` line per civilization, P4-01 13.1); (c) every `<case> territory <Y>` line (24 at d363da3) whose image has a kind-2 cell between two free regions, an inner border of a realm with L > 175, or a cell of a changed colour, which is 23 of the 24 `territory` lines (geo-seed-0-512 at 6000 has no free-free border, no inner border above the luma bound and no changed colour). No `year`, `years` or `table` line changes; any such change is a defect | test writer, from `worldgen/tests/reference/p4_timeline_reference.py golden`; never edited by hand. The commit message lists which `static` lines moved and that the rule above predicts exactly those |
| C++ tests with written-out colours or pixels (P4-01 13.2 items 1, 2, 7; P4-03 11.1 items 1, 3, 5, 6, 9) | expected values | test writer |
| `gdext-headless-check` pixel comparisons | none in the check's code: it compares the view with `worldgen-cli render` output made in the same run, so it follows; only the legend checks of 6.4 are new | viewer engineer |

## 6. Order of work and base

This change is **implemented on top of P5-12** (generator 0.22.0, branch `p5/12-int`), which re-records the same `worldgen/tests/golden/p4-01-timeline.txt` for its 0.22.0 worlds: building on it avoids a merge conflict in that file, and the re-record here then moves only the lines of section 5. The spec itself does not depend on P5-12 (P5-12 changes no biome colour, `territory.cpp`, `render.cpp` or viewer territory code). If P5-12 is not merged when this starts, work on `p5/12-int` and merge after it.

One branch: generator engineer (library and CLI), viewer engineer (history state and legend), test writer (reference, golden, tests), determinism auditor (the library diff: integer luma, the factor choice, `half` in `TerritoryBase::build`, the new overload; lint), validation specialist (every preset, `world-hashes.sh local`, the headless check), world critic (the same seeds as `docs/reviews/p4-03-history-view-world-critic.md`: 0, 15, 59, 91 at 5999 and 42 at 6000, for findings 1 to 5).

## 7. Tests required

Test writer, from P4-03 r4 (11.1) and this spec, with the independent reference:

1. **Table values.** `race_civilization_colour` for the six keys, k = 0 … 7, against r4 7.2 (the four changed entries included); `race_base_colour` unchanged.
2. **Rule D for the whole table.** C1, C2 and C3 of 7.3 over all 24 entries, B built as 7.3 from a shipped-data world's table `biome`, D in `int64`; and the minima of 7.2's last two columns reproduced exactly (a table edit that keeps the rules but not the stated margins fails, so the spec and the table move together).
3. **The pale test and the luma split.** L of every entry; the set with L > 175 is exactly {goblin 1, faerie 1, goblin 0, dwarf 1, human 1, elf 1}; on a fixture, an inner border of a realm of each side (e.g. goblin ordinal 1 and orc ordinal 0) has the 10/16 and the 12/16 colour; a hashed key's colour follows the same test.
4. **Free-land borders on a fixture** (hand-made layers, at most 8 × 6, extending 11.1 item 5): a border between two free regions (half of the shaded biome colour, values written in the test), between free and held (black), between two realms (black), inside a realm (dimmed), a free-free border cell that is a lake cell or river-painted (black), a cell whose E neighbour is free and whose S neighbour is held (black); at scale 1 and 2; with an all-zero holder vector the same free-free cells half, and with an empty holder vector every border black (equal to the `regions` composite when fill is empty).
5. **The new overload equals the two-step path** at every year of T1 and at the golden years of the 512 world cases, scale 1 and 3, both forms (vector and `out`); and the same FatalError text for a bad year, a short colour vector, a bad scale and a wrong `out` size.
6. **Property** (r4 11.1 item 9): every pixel is B, a shaded fill, a dimmed fill on a kind-2 cell whose rule holds, or half(U) on a kind-2 cell whose sides are all free.
7. **Golden**: section 5.
8. **Headless check** (viewer engineer, r4 10.2 H9): the territory legend's rows after the last civilization row are exactly `border between holders` [0x000000], `border inside a realm` [], `border in free land` [], `free land` [], the land-biome rows of the `regions` legend in its order, `water` [], and the three water rows W, with the colours of the `regions` legend; the three border rows carry the tooltips of P4-03 r4 6.4 and the other rows of the tail none; the image checks of H2 unchanged.

## 8. Open points for Paul

1. **The luma bound** is 175, not the critic's 190 (section 3), confirmed by the lead in r1; Paul may still ask for 190 (only goblin 1 at 10/16).
2. **Goblin 3**: settled by the lead in r1, (140, 155, 30).
3. **Half strength for free borders** (8/16 of the land colour) confirmed by the lead in r1; if the free web still competes with realm outlines, a weaker 12/16 is a change of one factor.
