# World critic review: P5-04a relations and alliances follow-ups (generator 0.28.0, history report v9)

Material: default seeds 0-11 and high-monster-pressure seeds 0-3 at 1024, compared with the same seeds at 0.27.0. Counts below are over all 16 worlds unless stated.

## Verdict

Acceptable to merge. Every note this stage set out to answer is fixed or clearly better. What is left is wording and thin-effect polish, nothing that breaks a history.

## Earlier notes and their status

| # | Earlier note | Status | Evidence |
|---|---|---|---|
| 1a | Refusals without a motive | Fixed | 101 refusals, 8 motives (0:17, 1:24, 2:2, 3:33, 4:3, 5:16, 6:5, 7:1); each line opens with a reason |
| 1b | Betrayals without a motive | Fixed | 14 betrayals (19 before), 3 motives: "Seeing its ally weak" 7, "Grasping for more" 4, "Coveting its ally's land" 3 |
| 2 | Identical renewal lines | Mostly fixed | 272 renewal events against 873 at 0.27.0; the text now names a basis (trade 138, war together 114, two other bases 20) |
| 3a | Arms without effect | Fixed | 34 battles read "With new-bought arms" and cite the sale in their causes, from 54 sales |
| 3b | Spoils without effect | Fixed but rare | `spoils_withheld` reads and has consequences, but there are only about 11 in 16 worlds |
| 4a | `against a common enemy against cN` | Fixed | 0 occurrences; block lines read `against a common enemy` |
| 4b | Thaw wording, turn cool-down | Fixed | "put aside their hatred of one another", with the cause named: "After the raids of a common foe" or "After long years of peace" |
| 4c | `allies:` lines without names | Fixed in summary | `allies: 5; at the end: c7 The Sawy Ring, c9 ..., c10 ...` (default-s1). The names are plain text, not links, and only the first three are listed |
| 5a | Towers piled up, never ended | Partly fixed | 88 towers against 90, 4 ended (`si39` e1443 "fell", `si18` e615, `si20` e458, `si32` e1238 "lost") |
| 5b | `[figures]` listed only generals | Fixed | default-s1 lists general, general, general, caster, hero, ruler, then "and 104 more" |
| 5c | `[monsters]` living first | Fixed | default-s1: the asleep Host of Fernhurst comes before the slain ones |
| 5d | `hero_rose` after a long quiet | Fixed | "When, after 746 quiet years, the Host of Aelglie raided again, Noithienn ... took up the hunt." (default-s5 e1088) |
| 5e | Epithet "the Old" too common | Fixed | 178 lines against 285 |
| 6 | Realm milestones and summary length | Mixed | see F2 and F6 |

## Findings

**F1 (should fix in follow-up): the renewal lines left are still a dull stream.**
A third of the old volume is still 272 lines, and they come from a small set of sentences. 33 read "After long years of trade, the X and the Y renewed their alliance." with only the names changed, plus 18 of "After the outbreak of a new war and the war they fought together". Pairs renew 26 to 38 times (largest per-pair counts in the realm blocks). The "something happened" test passes, but a pair that renews every 150 years for 5,000 years is not a history.
Remedy: cap renewals per pair, for example the first one and then only after a change of basis or a war, or fold them into a single "renewed N times" count in the block.

**F2 (should fix in follow-up): milestones are three of the same kind and miss the realm's story.**
The top three non-quiet events are usually the three biggest sackings. default-s1 c1: `5604 e1065 sacked`, `5606 e1075 sacked`, `5878 e1350 sacked`, all against the same horde, all late, out of 25. default-s3 shows the same for the House of Roelaele (4107, 4979, 5623, all sackings of the Sneegg Horde). Founding, first war, first loss and the ruin of a seat never appear. A realm with two or three milestones shows `settlement_founded` as a milestone (default-s3: "The House of Vefal founded Gloafedell", year 0), which is not one.
Remedy: choose at most one milestone per kind, and prefer a spread across time (earliest, largest loss, latest).

**F3 (should fix in follow-up): the motive "Grasping for its own gain" tells nothing.**
For a refusal, motive 5 (16 cases) reads as flavour: "Grasping for its own gain, the Gub Horde under Zig would not take up arms for the Snibutch Mob against the Hold of Nadkirn" (default-s?, e820, year 5039). It names no gain. "Fearing the strength of the foe" (33) and "Weary of war" (24) dominate, which is believable, but the three together are 70% of refusals. The better motives (a claim on the ally's land, the spoils kept from it, grown cold) are rare (5, 1, 3). "Remembering the spoils kept from it" (e1290, motive 7) is the best line of the lot and should happen more.
Remedy: make motive 5 name the thing wanted (the ally's rival, a region), or fold it into 6; raise the weight of motives tied to earlier events.

**F4 (should fix in follow-up): a first withholding is too rare to be seen, and the escalation only shows in one shape.**
About 11 `spoils_withheld` events in 16 worlds, 8 of them in four worlds. Example of the intended escalation working: default-s1 e1384 "...kept the spoils of the Fallen Wood, 778 captives, from Clan Throrinhu, its ally, which would not forgive it", followed at once by e1385 `alliance_spurned` "Outraged that the spoils of the Fallen Wood had been kept from it, Clan Throrinhu broke its alliance". Proportionate, and it is a good chain. But the first-sour step ("did not forget it", grievance 1, e.g. default-s5 e653, e1255) leaves nothing visible in the alliance line, and where the grievance is 0 the line just stops (e858 "kept the spoils ... from the House of Vamiel, its ally."). Readers cannot tell the souring from a non-event.
Remedy: show the grievance in the block line (`sour since e653`), and let a later refusal cite the withheld share as its cause more often.

**F5 (note): towers are fewer but still pile up inside one region and one realm.**
default-s1: three towers of the House of Glaefaenn in the same region, the Torn Hurst: "Star-Tower of Femaeth" (112), "of Glaelath" (3464), "of Asithie" (4455), all listing e3 (the first settlement) as cause. All three are open. Only 4 of 88 towers end in 16 worlds, though most worlds lose realms. The 30-cell spacing works; the region repetition and the shared cause read mechanical.
Remedy: one tower per region per realm unless the older one has ended; give each tower the founding figure's event as cause, not the settlement's.

**F6 (note): summaries are slightly longer and read slightly better.**
Default medians: about 609 lines against about 590 at 0.27.0 (range 544 to 657; high-s1 766, which is the busiest world, 740 before). Not an increase worth worrying about. The realm blocks are better with the count (`milestones: 25`), the unmet and the `allies:` lines, and the `[figures]` block now shows the shape of a world (rulers 73, claimants 7, generals 18, heroes 6, casters 6). The "same-day group" lines I looked for did not turn up in default-s1 (I found no grouped line there); I cannot say whether they are rare or absent.

**F7 (note): thaw and hate flip-flop between the same two peoples.**
default-s1 Elves and Goblins: thaw 3096 (e131), hate 3233 (e152), thaw 3390 (e225), hate 4357 (e532). The cool-down holds (about 140 years apart) and each flip has a cause, so it reads as an old grudge that never settles, which suits the tone. The "after long years of peace" cause for a thaw between Goblins and Faeries (5905 e1039, causes only e352) rests on one cause event, a single trade opening, which is thin.
Remedy: none required; consider citing the war that ended the peace window.

**F8 (note): betrayer epithet "the Faithless" and the stock phrase.**
All 14 betrayals read "[Motive], the X under N the Faithless broke an alliance of 936 years with the Y". The epithet is a good device and 90 lines of it in 16 worlds (116 before) is acceptable. A betrayal of 1,036 years ("Skreegg Horde ... 1,036 years", 4986 e834) is a strong line; they all share one frame, but that suits a rare act.

## What reads well

- The refusal and betrayal lines now give an actual reason, and the reasons sit near events that explain them (e456 and e457 in default-s0: "Fearing the strength of the foe" against the Krurz Horde, right after e453 arms sold to that horde and followed by e458 "With new-bought arms, the Krurz Horde defeated Clan Drourd"). That is a chain a reader can follow from sale to refusal to defeat, and a first-rate piece of dark storytelling.
- The refusal that breaks the alliance (e563 to e564: "After a call to arms refused, the alliance ... was broken") finally has the refusal before it.
- Thaw and hate lines name their cause.
- `[monsters]` (living first), `[figures]` (one per role) and `allies:` lines with names give the summary a better opening.
- Ruined and fallen towers now exist (e1443 "fell").
