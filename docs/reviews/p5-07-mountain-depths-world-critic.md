# World critic review: P5-07 mountain depths (generator 0.29.0)

Material: the 12 worlds of `index.txt`. Counts are from full reports (event lines plus a stats line, so each is about 1 high).

## Verdict
ACCEPT WITH FOLLOW-UPS. The deep reads as a real story in the full report and the horrors are distinct. The summary hides half of it: seals, aid and deepening are missing there. All of it also lands in the last tenth of history.

## Findings, most serious first

F1. Seals and aid are absent from the summary. Wakings and losses are present.
- Across the 12 summaries, `deep_sealed`, `warren_sealed`, `ally_aided_hold`, `delving_deepened` and `warren_delved` appear 4 times in total (s72 1, high-s0 1, s9 2).
- default-s85: e863 (woke Khark) is in the summary, but e864 (Khard's aid) and e865 (sealed) are not. The reader meets a wyrm woken and never learns the gate held.
- The realm block for c5 Khairthain has `milestones: 1` (the founding). It says nothing of two wakings, two lost holds, a seal or its aid to Edas.
- Only `[monsters]` shows "m8 Khark ... sealed in The Coils of Khark ... raids 0" (s85 line 600). This is good, but it is hidden behind "and 7 more".
- Change: make 50 (seal) and 40 (aid) milestone-eligible for the realm concerned and for the aiding ally. Add the monster kind to the 'major calamity' rule. Rank sealed monsters first in `[monsters]`.

F2. The deep is compressed into the last tenth of history.
- Wakings by 250-year bin: 4500 (1), 4750 (2), 5000 (1), 5250 (2), 5500 (about 12), 5750 (about 13). About 25 of 31 fall at 5500 or later.
- The seals in the samples are dated 5311 to 5946.
- The delving itself runs over millennia (warrens from 2250, mines from 3575). The deep has no payoff for about 2,000 years, then everything fires in the last 500.
- Result: the early and middle ages stay quiet, and the end is crowded. s72 has three holds lost within 66 years and a realm ended (5632 to 5698).
- Change: make waking chance depend on years spent at depth 2 or 3, not on the era. Let a depth-3 mine be able to wake at once. Or lower the depth-2 threshold. The waking events should spread from about 4000.

F3. Allied aid: the reader cannot tell whether it mattered.
- `ally_aided_hold` carries "Unbidden" or "Bound by old oaths" as motive. That is good. It is dated the same year as the waking and followed by the outcome in the same year, with no link.
- s72: e300 woke, e301 aid, e302 `hold_lost`, all 5632, with e302 caused by [e300, e301]. Aid listed as a cause of loss reads as if aid failed. The text never says so.
- s85: e1010, e1011 (Edas aids), e1012 lost, 5795. The same shape.
- s85: e863, e864, e865 sealed, 5663. The "held" case reads right only because e865 lists e864 as a cause.
- The aider's own block never mentions it.
- Change: write the outcome onto the aid line ("and was too late", "and the gate held"). Or add an `aid_failed` clause to `hold_lost` ("though the Hold of Khard stood beside it").

F4. Deepening is repetitive and has no consequence-bearing text.
- 71 `delving_deepened` lines in 12 worlds (s85: 13 deepening, 8 are depth 2). Only 8 reach depth 3.
- Three openers (27 "By hammer and lamp", 23 "Year upon year", 21 "Following a rich seam") and three closers ("below the reach of daylight", "into the roots of the mountain", "into the oldest dark beneath the world").
- In s85 there are 6 near-identical lines within 4,300 to 5,650.
- The lines carry kind 15, which looks like a quiet-level kind. They are fine in the full report but clutter it. They are rightly kept out of the summary.
- Names stutter: "delved the Delvings of Tadinkhik Hold", "the Hold of Khairthain under Thum" (10 `hold_lost` lines have the form "Thark Hold of the Hold of Khairthain").
- Change: say what the delving found ("struck silver", "struck a cold draught"). Cap depth-1-to-2 lines to one per world per realm in the full report, or merge them. Fix "X Hold of the Hold of Y" to "X, a hold of Y" for dwarven realms.

F5. Sealed horrors and re-use of sealed names.
- Checked: s85 m8 sealed, raids 0, killed 0. No later events cite it. No contradiction found in the reports I grepped. The seal holds.
- A hold lost twice: none found. A mine deepened after its realm ended: none found (s72's civ_ended e326 is the last event of Doulidi).
- Minor: in s72 `civ_ended` is e326 and carries the cause e323. It reads well ("After the loss of its last hold to the deep ... Moliti the Last").

F6. A horror takes holds at an unplausible rate in one lair.
- s28: Thraikbrou takes Kreegrunk Hole (3,199 died) and two others within 270 years. Fine.
- Dairngound (s85) overran Thark Hold e1012 and Halibund Deep e1047, 43 years apart, and then is "asleep" with raids 2. After a double ruin it sleeps. Reasonable, but the realm block still lists only "3 ruined" with no text.
- high-s0 has 5 holds lost, s28 3, s72 4. Fine with Paul's decision: a single hold falls often, and a realm falls rarely (1 `civ_ended` in 12 worlds, s72).

## Specific answers
1. Coherence per hold: full report yes (delved, deepened, woken, sealed or lost, and `[sites]` reads "mines 8"). Summary only partly (F1).
2. Repetition: see F4. Kinds 93 to 99 per world are 1 to 4 woken, 1 to 3 sealed, 1 to 9 lost and 1 to 3 aided. Warren lines: 4 per world on average, and "Squabbling and scraping", "Grub by grub and stone by stone" and "Ever greedy for the dark" appear in the same world 3 times (s28 e129, e385, e605 for one warren).
3. Allies' aid: who and why (oaths, unbidden) are clear. The effect is not (F3).
4. Warrens: distinct. "Came up the goblins' own tunnels", "boiled up out of the burrows", "no goblin went that way again" are good and unlike the dwarves' "stone and rune". The warren of s28 losing 3,199 people is the largest single loss in the sample. A warren hold is also a "hold_lost", which reads a little oddly.
5. Horrors: distinct. Ymrach "a wyrm of the lightless deep" ("the Deep-Coiled", "Stonegnawer"), Ysgarth "a host of eyeless things" ("the Eyeless of X", "the Swarm Beneath X"), Zhakul "a fire that walks in the roots of the world" ("Flame-Under-Stone"). Raid lines also differ ("swarmed out of the dark", "came up burning", "fell in fire"). Ymrach raid phrases are not distinct from the ordinary wyrm: the name shape is the same ("X the Deep-Coiled"), with 2 near-copy names per world (Khark, Dimm, Bounkhai).
6. Contradictions: none found. Summary length: 447 to 851 lines (civs4 851 against the 867.5 of P5-04a), so there is no regression. Clutter in the full report is from F4.

## What reads well
- The wake-and-loss chain is caused (e863 to e1012 to civ_ended in s72) and names the people: "1,487 died, and 2,231 fled", "After the loss of its last hold to the deep, Clan Doulidi under Moliti the Last came to an end".
- Seal lines ("With stone and rune ... and the deep ways were closed", "and no goblin went that way again") carry the right dark tone.
- `[monsters]` marks "sealed in The Coils of Khark ... raids 0", which gives the player a place to find.
- Goblin and dwarven openings differ.

## Suggested changes (rules and parameters)
1. Milestone and summary eligibility for seals and aid (F1).
2. Waking tied to years at depth, spread over eras (F2).
3. Outcome text on aid lines (F3).
4. A fuller template set for deepening, with a discovery clause, and fix the "of the Hold of" doubling (F4).
