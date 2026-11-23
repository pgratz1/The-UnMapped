# P5-12 renamed regions: world critic review

Generator 0.22.0, engineer's sweep `~/.cache/the-unmapped/out/p5-12-impl/sweep/` (100 default worlds and 20 civs4 worlds at 1024; every `became known as` sentence of the 120 full reports, 540 in all, plus 100 summaries read by script and 12 by eye, seeds 0, 1, 5, 15, 19, 22, 26, 35, 47, 57, 58, 84). Counts are over those 540 sentences unless said.

## Verdict

**Pass with notes.** Renamings read as earned, trace to their events, stay consistent in the text, and F1, F2 and F3 are fixed. One wording family (settlement names with a space inside, `Waste` above all) and one cause clause should change soon; neither needs to block the merge.

## What works

- Each renaming is a short, dark, legible sentence with its cause in the next field: "After the plague, Dimbrake became known as Ster's Grave." (seed 0, 5752, e358, after e356). Seed 0's three renamings are the three most dramatic things in its last 250 years.
- Names in force: a scripted check over all 100 default full reports found 0 sentences that use the new name before its event id or the old name after it (same-year earlier events correctly keep the old name, e.g. seed 1 e974 to e980 "Bellgrove", then e981 "Alid's Bane"). Seed 0 e387 reads "defeated the Duchy of Cear in the Ruin of Huldbury", seven years after e380 renamed it.
- Rate: median 4 per world (1 to 7 in summaries), 1.9% of shown summary lines (max 3.4%), at most 3 in any 100 years (3 worlds), 2 in 52 worlds. It does not crowd the summary.
- Second renamings (17) read as history: "Measland" (a heartland, 5210) becomes "The Ashes of Bardham" (5976) in seed 22; "The Hant Reach" (5258) becomes "The Ashes of Wummomstead" (5789) in seed 47. A hopeful name overtaken by ruin is the right tone.
- `renamed regions:` gives name, year, event, cause kind and former name on one line (seed 0: `rg54: The Ruin of Huldbury since 5780, e380 after e378 sacked; formerly The Cruel Barrows`).

## Findings, most serious first

**1. Settlement names holding a space make clumsy region names (should change soon; does not block).** 91 of 540 names (17%; 32% of the 283 sack names) wrap a two-word settlement name (goblin and orc `Pit`, `Hole`, `Hollow`, `Glade`, `Hold`, `Deep`, `Fort`, `Camp`). Three shapes:
- `{name} Waste` after a generic noun, 28: "Tondho Deep Waste", "Nub Pit Waste" (seed 0, 2362), "Wosk Hollow Waste" (seed 31, 5884), "Uroz Fort Waste" (seed 85 in the sweep list, 5994). Two nouns stacked; "Pit Waste" reads as a rubbish tip. Worst of the three.
- `{name}'s Fall` or `'s Pyre` after the generic, 27: "Bugg-Drux Pit's Fall" (seed 20, 5916), "Krax Hole's Fall" (seed 14). Readable but the possessive lands on the noun, not the place.
- `The Ruin/Ashes of {name}`, 36: "the Ruin of Maflaetwia Hollow" (seed 6, 4977). Acceptable; this shape reads as a place.
Counts by noun: Pit 27, Hole 19, Hollow 17, Glade 12, Hold 7, Fort 4, Deep 4, Camp 1.
Suggested change: when `name_role = settlement`, strip a trailing generic (the culture's settlement-noun list) before filling `{name}` in the `Waste`, `'s Fall` and `'s Pyre` forms ("Nub Waste", "Bugg-Drux's Fall"), and keep the whole name in the `Ashes of` and `Ruin of` forms. Alternatively drop `{name} Waste` from `capital_sack` and `great_sack` and keep `The Ashes of {name}`.

**2. The heartland cause clause does not match the name (wording; can wait).** All 38 "After the rise of a city" sentences carry a name built from the realm, not the city: "After the rise of a city, Gleanlow became known as the Hant Reach." (seed 47, 5258, e258; the city is Wummomstead, the realm the Duchy of Hant). A reader asks which city and why Hant. Suggested wording for rule 6: "After {realm}'s seat grew into a city" or "After the rise of its capital". The same 38 are 12 `Reach`, 10 `-land`, 8 `Heart of`, 7 `Hearth`, 1 other; all are human or elf realms (no goblin, orc, dwarf or faerie heartland in the sweep, because their capitals rarely `became_city`), so the "hopeful" rule never speaks for the oldest-night races.

**3. Possessive after a final s (small; can wait).** 6 names: "Bantheas's Barrow", "Omos's Fall", "Iersaes's Fall", "Vaenaes's Fall", "Wodalis's Bane", "Alildis's Bane". Suggest a rule that a root ending in `s` takes `'` alone, or use the `The X of {name}` forms for such roots.

**4. `{name}'s Bane` is ambiguous (19, rule `slaughter`).** "Alid's Bane" (seed 1, 5960) is the field where Alid was beaten, but reads as the thing that killed Alid, or as Alid's own scourge. Suggest `The Rout of {name}` or `{name}'s Last Field`. The other slaughter forms are fine.

**5. Little variety of nouns and few repeats across worlds (taste).** Of 84 noun forms, `Downs` 44, `Wood` 33, `Steppe` 6, `Moor` 1, `Peaks` 1 (in "of the Slain"). Mountain, marsh, isle and fen regions are never renamed this way (they are rarely where the blood or plague falls). The names repeat across seeds: "the Red Downs" 15 times, "the Silent Wood" 10, "the Bloody Downs" 10, "the Downs of the Slain" 9. Inside one world the registry prevents repeats. More forms per rule (data) would help most: 4 forms for `slaughter`, 4 for `plague`.

**6. Timing is late (can wait, may be intended).** Renamings by thousand years: 2000s 2, 3000s 25, 4000s 77, 5000s 293 (41% in the last 300 years, the 5700s onward). The ancient age is almost never renamed; the oldest named place keeps its drawn name for 3,000 years. Rules need sacks, battles or a conquest, which come after the awakenings of humans and orcs. If Paul wants ancient scars in names, add a lower threshold for `conquest` and `great_sack` before year 3000.

**7. Ruins list uses the present name for older ruins (follow-up).** 75 of 494 ruin lines (45 worlds) sit in a region renamed after the ruin: seed 0, `s51 Edadbury in The Ruin of Huldbury (rg54): ruined 5778`, where the ruin predates the name and the sentence for e374 says "Cruel Barrows" (the sack of Huldbury, which gave the name, is two years later). This follows the spec (present names outside sentences), and the `renamed regions:` list explains it, but a reader reaches that list last. Suggest appending ", then <old name>" in the ruins and civilization blocks when the region is renamed, or listing `renamed regions:` before `ruins:`.

**8. Rules over-represented.** By rule over 402 default renamings: capital_sack 185 (46%), slaughter 66, plague 46, conquest 44, heartland 34, great_sack 27. Counting the shared clause "After the sack", sacks are 52% of the 540 in all (283). Since most sacks are of capitals, "sack" dominates the summary of a war-heavy world. Not wrong for a dark world, but lowering `capital_sack` `chance_permille` from 700 to 500 (or raising `conquest` and `heartland` chance) would give a more even spread. Rule 2 and rule 3 also overlap: a capital sack with over 1500 dead can take either `The Ashes of` form.

## F1 to F3 and the previous notes

- **F1 (doubled suffix, `ass`)**: fixed. 0 doubled or `ass` settlement names in the sweep's check (N1), and a regex pass of the full text found no `steadstead`, `wickwick`, `Deasstead`, `Stonton`. Words with `ass` in them are real words only (`Wardgrass`, `Glassbourne`, `pass`).
- **F2 (filter on region names)**: fixed. Seed 84 region 38 "Tarden" is now "The Tainted Weald"; no `Tarden` in any report. Replacements are good: seed 15 "Kestrelhorn Reaches" (was `Wanegrass`), seed 59 "Larchwood" and seed 91 "Archer's Grove" (were `Pinegrove`). No new awkward drawn name seen.
- **F3 (refuges list)**: fixed as a reader sees it: `hidden refuges: 0 of 0 founded by flight` in seed 0; the sweep's N3 shows 9 hidden of 50 founded.

## Patterns across seeds

| Pattern | Count |
| --- | --- |
| Renamed regions per world | median 4, p10 2, max 8; 100 of 100 worlds have at least one |
| Names with a space inside a settlement name | 91 of 540 (28 `Waste`, 27 `'s Fall/Pyre`, 36 `Ruin/Ashes of`) |
| Heartland cause clause vs realm-built name | 38 of 38 |
| `s's` possessive | 6 |
| `'s Bane` | 19 |
| Names from the same form and noun repeated across worlds | "the Red Downs" 15, "the Silent Wood" 10 |
| Renamings after year 5000 | 293 of 397 (74%) in 100 default worlds |
| Name used out of order in a sentence | 0 in 100 worlds |
| Second renamings | 17 in 100 worlds (a heartland later burned, 6; a sack later conquered, 6; others) |

## Suggestions, as rule or data changes

1. (Soon) Settlement-role names: strip the trailing generic noun in `Waste`, `'s Fall` and `'s Pyre`, or drop `{name} Waste` (finding 1).
2. (Soon) Heartland cause clause names the seat, not "a city" (finding 2).
3. (Later) A final `s` takes `'` alone (finding 3); replace `{name}'s Bane` (finding 4).
4. (Later) More forms in `slaughter` and `plague`; one more noun for the lands those hit (finding 5).
5. (Later, Paul's taste) Ancient renamings, and the sack share (findings 6 and 8).
6. (Later) Ruins and civilization blocks note the former name (finding 7).

**Blocks the merge: nothing.** F1 to F3 are fixed, the log is consistent and the rate is right. Findings 1 and 2 are the first to take after it.
