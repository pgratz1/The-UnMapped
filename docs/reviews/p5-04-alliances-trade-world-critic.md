# World critic review: P5-04 alliances, trade, mines and arms (generator 0.26.0, history report v7)

Verdict: **accept with notes.** Alliances now give the realms a politics a reader can follow (a stated reason, a partner, an end that cites an event), and the allied-war lines read clearly; what is left is thin motive in refusals and betrayals, an abrupt and delayed spurn, and mechanical renewals and trade that add lines more than story.

Material: default seeds 0-11 and `--monster-pressure high` seeds 0-3 at 1024 (16 worlds); images read: default-s1. Counts are over the 16 full reports unless stated. Totals: 181 formed (3 to 26 a world in my count, 202 with the high seeds), 538 renewed, 24 lapsed, 36 broken, 16 betrayed, 94 joined, 77 refused, 22 withdrew, 219 allied battles, 176 trade opened, 57 broken, 18 spoils shared, 28 withheld, 16 spurned, 46 mines, 57 arms sold (the formed/renewed figures count the default seeds; the race pairs use all 16).

## Point 1: are alliances believable?

Yes in reason and ending, with one caution on mix. Every formation states a basis ("After the outbreak of war, ... swore to stand together against the Skreegg Horde" s1 e172; "After long years of trade, ... swore friendship and alliance"; "swore the alliance their peoples had kept since the gods made them" s1 e301, elves and faeries at 4178) and the end cites an event (broken after a refused call, lapsed "its cause long past", spurned after spoils kept, betrayed by a war e609). Partners (202 alliances, all 16 worlds): Elf-Faerie 32, Goblin-Goblin 30, Goblin-Orc 25, Elf-Elf 21, Elf-Human 19, Faerie-Human 13, Dwarf-Human 10, Dwarf-Faerie 10, Goblin-Human 10, Faerie-Faerie 9, Dwarf-Elf 9, Human-Human 4, Dwarf-Goblin 4, Dwarf-Dwarf 2, Human-Orc 2, Dwarf-Orc 2. Elf-Goblin and Elf-Orc never ally, so the old hatreds hold; the dark pairings are goblin-orc (25, natural kin of the evil camp) and goblin-human (10, a pact of convenience). The mix is not too much; a median of 14 a world is high only in worlds with a dozen realms, and most pacts are "against a common enemy" (94 of 202) so they read as wartime convenience. The caution is that nothing says an alliance is across hatreds: Dwarf-Goblin (4) and Dwarf-Orc (2) read the same as Elf-Faerie. A clause when the peoples' standing is hatred or wariness ("though their peoples did not love each other") would make the odd pairs tell a story.

## Point 3: spoils withheld and the spurn

As built, ending at once reads abrupt, and I would not keep it as is; but the fix is a grace step, not Paul's looser rule alone. Withheld shares are the common outcome (28 withheld against 18 shared, 61 percent), so "one withheld share ends the alliance" makes the ally's reward a coin flip that mostly ends in a split, and the spurn comes late: from the withheld event to the spurn is 1, 5, 14, 16, 23, 89 and 106 years (s10 e546 at 5645 then e617 at 5751; s5 e1146 at 5842 then e1194 at 5931), yet the text says "After the spoils of ... were kept from it, ... broke its alliance", which a reader takes as an act of the next season. Suggest: the first withheld share sours (a line or a standing drop, "the Warband of Muuz'grurk remembered"), the alliance ends at the second, or at once only when the kept share is large or the ally fought in the battles (the "share" values range from 40 to 2105 with no unit stated); and the spurn should fire within a few years of the withholding or say how long the grudge was kept ("after 106 years of grudge"). Dark-fantasy wise the betrayal-by-greed is right; the delay is what reads wrong.

## Findings

**F1. Refusals and betrayals have no stated motive.**
Refusals: 77 lines such as s1 e537 "The Sawy Ring would not take up arms for the Lousstwo Court against the Skreegg Horde." with no why (too far, bled white, afraid, bought off). Joins (94) and refusals (77) are close, so an ally is as likely to refuse as to come, and a call to arms in s1 ends "2 joined, 8 refused" (s0: 5 joined, 7 refused). Betrayals (16): s1 e610 (5127) "The Skreegg Horde broke faith and made war on the Warrens of Zeez, an ally." is an ally of 1,732 years (e172, 11 renewals) and the cause list is only the renewal e564 and the war e609; the war itself (e609) has a claim, so the motive exists but the betrayal line does not say it. s1 e610 and e609 also share a year, so a reader meets the declaration and the "betrayal" as the same fact twice.
Fix: add a reason to refusal and betrayal events (a short stored reason code: far, weak, a war of its own, at peace with the foe, a claim on the ally's land) and print it ("Bled white, the Khirnhu Throne left the war" in e822 is the model: it already does this for withdrawals). Merge the betrayal into the war declaration line when same-year.

**F2. Refusals have a consequence, but only one, and it is lopsided.**
After a refusal the alliance can be broken (s1 e696 refused 5509, e697 broken the same year; e994 refused, e995 and e997 broken at 5799), which is a good cause chain. But 36 breaks against 77 refusals means many refusals pass without a mark, and a refusal "in defence" is not the same as one "in attack" (the report counts 0 in attack in s0 and s1: only defensive calls appear). Fix: one refusal gives a standing loss, a second breaks; allow calls in attack.

**F3. Renewals every 150 years are mechanical, and they come in pairs.**
538 renewal events; s1 has the Gub Horde and the Warrens of Zeez renew 17 times, 3541 to 5309, in identical words ("The Gub Horde and the Warrens of Zeez renewed their alliance."), with two renewals the same year (3541, 3691, 3841, ...). They are kept out of the summary (good) but in the full report they are the largest alliance kind, and their causes simply list the earlier ones (e153, e180, e197). Fix: do not emit an event for a quiet renewal; keep the count in the `[alliances]` line (it already shows "renewed 17"); emit renewals only when something happened since (a shared war, a spoils share, a crisis), with that cause in the text ("renewed their pact after the war against ...").

**F4. Duplicated "against" in the block lines.**
`[alliances]` s0 and s1: "c6 ... 5891-E, against a common enemy against c8, e592"; "by origin"/"in friendship" read fine, this one repeats. Fix: "against a common enemy, c8" or "against c8".

**F5. The wording of a double basis.**
s0 e592 "After long years of trade and the outbreak of war, the Duchy of Cear and the Duchy of Deas swore to stand together against the Krurz Horde." Fine, but the trade clause appears with a war alliance 10 times; it is not a cause of a war pact. A basis of trade only gives the plain "After long years of trade, X swore friendship and alliance" (14). Also "After the loss of a neighbour's land, the Skiz-X ..." (4) names no neighbour. Fix: name the neighbour or drop the clause.

**F6. Trade and arms add lines, and one place.**
Trade opens (176) and breaks (57) with plain causes ("When war came, the trade ... ended"; "was lost with the roads between them" s1 e626) and they are believable, and a trade breaking for war is a consequence. But trade is never named in anything the player could find: no goods, no route, no place. Arms sold (57) say "sold arms forged in the Mines of Girin to the Sawy Ring" (s1 e308, e661, e803) and nothing else follows: no battle cites an arms sale, no weaker realm wins because of it. A dwarf realm's mines (46 over 12 worlds: 5, 4, 7, 2, 3, 2, 1, 8, 3, 5, 3, 3; s6 has one) give named places ("Delvings of Harkhid Deep in the Brambled Weald"), and I like that; but in the image of default-s1 the mines are tiny and cannot be told from other glyphs. Fix: have an arms sale feed a later battle or war outcome line ("with arms from the Mines of Girin"), and let a mine be a ruin or a lair candidate when its realm falls.

**F7. The dwarves: character, not gap.**
Dwarf alliances are 31 of 202 (15 percent), and they stand in 87 to 89 percent of worlds. They open mines and sell arms (a role), so their lower share in alliances reads as dwarven reserve; s1 shows the Khirnhu Throne in four pacts (Sawy Ring, Hodmal League, Kingdom of Alun) which look like a merchant-power. I would leave the figure and not tune toward 90. The real gap: Dwarf-Dwarf 2 and Dwarf-Orc 2, Dwarf-Goblin 4, so the dwarves almost never ally with the other dark camp, which fits, but nobody says dwarves refuse; a one-line "the Khirnhu Throne would not swear to the Goblins, whom it had fought since ..." would turn the absence into a story.

**F8. Allied wars read clearly.**
"The Khirnhu Throne came to the defence of the Sawy Ring against the Gub Horde." (s1 e601), "The Sawy Ring, with the Khirnhu Throne, defeated the Gub Horde in South Wood; 540 of the defeated and 219 of the victors fell." (e813), "The Gub Horde defeated the Sawy Ring, with the Khirnhu Throne, in the Fallen Downs" (e821), "Bled white, the Khirnhu Throne left the war" (e822). Who fought on which side is clear, the order is right, the ally's withdrawal follows from a cost. Weak point: three allied battles of one war in 3 years (e813, e814, e821) are three near-identical lines. Fix: nothing needed; at most group same-war same-year battles.

**F9. Spoils shares are listed once, never felt.**
"gave the Zux Mob its share of the spoils of the Fallen Wood" (s? e600) has no consequence for the receiver (no new settlement, no standing). Spoils are the only link from an allied war to the map, and in 16 worlds they occur 46 times, 3 a world. Fix: say what was given ("two towns", "the pass of ...") so the share can be found on the map.

**F10. The summary and the block.**
Summary length is fine (431 to 731 lines; s1 631, s0 533) and the alliance events do not crowd it: s1 has 22 formed, 6 refused, 1 betrayed and 1 mine in the shown events, renewals none. The `[alliances]` block is the best addition: one line per pact with years, basis, event id, renewals, joined/refused, spoils, end and "and 4 more", plus totals ("calls to arms: 2 joined ..., 8 refused, 1 withdrew; wars with an ally: 2 of 35", "largest bloc: 413 permille"). The `allies: 3; at the end: c6, c7, c8` lines are useful; they list ids only (no names), so a reader has to look up each. Fix: print the names in the realm block (or "c6 Lousstwo Court").

**F11. Alliances do not touch the world map much.**
In s1 `wars with an ally: 2 of 35` and s0 3 of 15, so about 10 percent of wars involve an ally despite 22 pacts; the pacts stand idle most of the time. Not a fault, but it keeps alliances in the background. The set aside claims (a rule of 0.26.0) leave no line, so a reader never sees "the claim on X was set aside"; add a short line or fold it into alliance_formed.

## Patterns across seeds

- 16 of 16 worlds have alliances; 0 spurns in s1 and s0 default, 16 spurns in total (4 of 16 worlds in my sample show one with a gap over 80 years: s10, s5).
- Betrayals 16 in 16 worlds (one a world is typical); all 4 I read were in a long-standing pact of goblin realms (s1 e610, others in goblin pairs).
- Goblins are the keepers of alliances (Goblin-Goblin 30, Goblin-Orc 25, and 3 of 4 betrayals) which fits a dark tone; Elves-Faeries come by origin.

## Better than 0.25.0

- Politics outside war and hatred: pacts, a bloc, a betrayal after 1,700 years of alliance.
- Elves and faeries begin allied ("the alliance their peoples had kept since the gods made them").
- Every end cites an event; the lapsed line "its cause long past" is good.
- Allied battles say who fought with whom and what it cost; withdrawal "Bled white" gives a reason.
- Mines give named dwarven places; the `[alliances]` block is a compact summary.

## Suggestions in one list

1. Reason codes for refusals and betrayals (F1).
2. Spoils: soften to a first sour, spurn at the second or when large; spurn within a few years or state the grudge (Point 3).
3. No renewal events unless something happened (F3).
4. Wording: duplicate "against" (F4), neighbour name (F5), standing clause for odd pairings (Point 1).
5. Arms and shares to have a stated effect (F6, F9); names in `allies:` lines (F10).
