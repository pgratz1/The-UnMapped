# P5-08 pantheon and cults: world critic review (generator 0.30.0)

Material: 16 worlds (seeds 0 to 15), summaries read for about 8, full reports searched by script, two viewer screenshots.

## Verdict

Good enough to merge. The cults read as arcs and the curses and monsters carry real weight. Blessings, marks and most champions leave little behind them, and cult suppression repeats too often. These are follow-ups, except F1, which should get a decision before merge.

## Counts (16 worlds, event lines of the full reports, by year-regex)

| Kind | Events |
| --- | --- |
| divine_bless | 59 |
| divine_curse | 79 |
| divine_champion | 13 |
| divine_monster | 32 |
| divine_mark | 42 |
| cult_founded | 48 |
| cult_spread | 237 |
| cult_adopted | 51 |
| cult_suppressed | 58 |
| tower_abandoned | 33 (full-text lines, may include block lines) |
| tower_taken_up | 9 (same) |
| cult_ended | 0 (the `[cults]` block shows "ended" through suppression events) |

Acts per world run from 9 (seeds 3, 9) to 23 (seed 2). In summaries, divine and cult lines are about 1.5% of all lines.

## Findings

**F1. Blessings and marks have no consequence in the chronicle (follow-up, decide before merge).**
- I scanned every event's `[causes]` across the 16 worlds for any non-divine, non-cult event that cites a bless, curse, mark or cult event. The only hits were:
  - `divine_monster` cited by `monster_raid`: 149
  - `divine_monster` cited by `monster_slept`: 15
  - `divine_monster` cited by `monster_died`: 2
  - `divine_champion` cited by `general_appointed`: 9
  - `divine_champion` cited by `figure_died`: 8
- No event cites a `divine_bless` (59), a `divine_mark` (42), a `cult_*` event (the one exception below), or a curse. The curse itself carries a death toll in its own text.
- Example: s1 e12 "Aerfin ... smiled upon the fields of the House of Glaefaenn for 50 years" (effect=1, years=50). Nothing later says the fields did better. A blessing is a line with a number and no echo.
- Marks are the same. s2 e24 (570) "cursed the ground of the Withered Grove" follows the blight e22, and then nothing follows it.
- A cult that is founded or adopted is cited by nothing either, apart from the suppression events that cite it.
- Suggestion: let an active blessing or curse appear as a cause on a later famine, plague or growth event, or on a war's weariness, inside its `years` window. Failing that, say plainly in the text what the blessing did.

**F2. A suppressed cult can be put down again and again (follow-up).**
- s14 fa2, the Grovellers of Skoskrozz: founded 3371 e202, then suppressed 10 times (e341, e381, e443, e470, e471, e636, e648, e732, e827, e1321) by four different realms. The cult is "present" at the end.
- s5 fa3 was suppressed 9 times, s4 fa2 8 times, s10 fa1 6 times.
- Of 19 cults that are suppressed at all, 4 have 6 or more suppressions. A suppression is also "put down" with no stated motive: its only cause is the cult's founding event, e.g. e381 [causes e329]. Each one is plausible alone, but the sequence reads as a counter.
- `cult_spread` has the same shape: 19 spreads of one cult (s14 fa2), 14 (s5 fa3), 13 (s4 fa2), and 171 of 237 spreads fall after year 5000.
- Suggestion: give each cult a per-realm suppression cool-down, or let a suppression end the cult in that realm for good. Cap spreads per cult per century.

**F3. The champion's life is a story only when it is appointed general (note, with one follow-up).**
- Champions are rare: 0 in 8 of 16 worlds, and 11 of the 13 raisings that are not at the start of the list fall after 5000.
- The good case: s12 e1327 raises Nound, e1328 appoints him general, and he leads four battles (e1330, e1344, e1355, e1373) to the pass into Northwest Downs, then dies at 258 (e1592). That is a short saga, and the god, the war and the hero all connect.
- The hunt case: s3 e698 raises Snatkog, e700 has him found the Teeth of Briltick, e710 has him slay the giant Nirngidin, and e718 has him die at 41. It reads well. The cult and the hunt are the same story.
- The weak cases are the other 9 of the 13 champions that are neither generals nor hunters: they are raised and then die.
- Cause phrases for champions are all war or lore: "a battle won" and "a war won" make up most of them. There are no champions of harvest or craft gods, and no champion of any god of the dead. So no champion is raised by the gods who would be the interesting ones.
- Suggestion: require a champion to do something within a window or be withdrawn, and widen the domains that can raise one.

**F4. Cause phrases repeat per god and per deed (follow-up).**
Counted over bless, curse, champion, monster and mark lines in 16 worlds:

| Phrase | Count |
| --- | --- |
| Angered by the felling of the wild | 23 (plus 5 with "a monster slain" and 4 with "a monster slain and the felling of the wild") |
| Angered by the sack of a city | 22 |
| Angered by its faithful put down | 19 |
| Pleased by a battle won | 18 |
| Pleased by a tower raised and a book written | 11 |
| Pleased by a book written | 10 |

- Together these six phrases are about half of all divine acts that carry a cause phrase, and they are consecutive in the list. Every `divine_monster` line in my samples (s1 e184, s1 e647, s2 e177, s3 e567, s3 e238) is "Angered by the felling of the wild"; the Hunter in the Green sends a beast every time.
- Many acts have several causes but only one is phrased, so one clause such as "Angered by the felling of the wild and a mine opened" carries it.
- The "split of act kinds" by domain is believable (the wild god sends monsters, the lamp god blesses towers and books) but narrow. See F3 for the missing domains.
- Suggestion: add two or three other deeds per domain, such as a god of oaths angered by a truce broken, or a death god by a sack.

**F5. Goblin cults of the elven maker, and "hidden" in the name (note).**
- s2 fa2 "The Teeth of Saeriemi", god Saeriemi the Maker of Elves, is a goblin-founded cult in goblin realms. s2 e377 and e395 then show the same god sending a pestilence on goblins "Angered by its faithful put down". It is coherent as a story but reads strange: the elf-god's pestilence falls on the goblins who worshipped it.
- s2 fa7 is "The Hidden Ring of Iseland", faith of two realms, present, with no hidden state. The word is part of the name template, not a state.
- Suggestion: drop "Hidden" from the open-cult name template, or reserve it for cults in hiding.

**F6. Name stutter and repetition in cult sites (note).**
- "The Pit of Briltick in Buzgrork Pit" (s3 e700), "The Pit of Refae in Snink-Grozz Hole" (s1 e1187), "The Pit of Saeriemi in Drirkgnox Pit" (s2).
- Founding-site words across all 48 founding lines: Pit 11, Bone-House 9, White Fane 8, House 7, Deep Shrine 4, Moon-Glade 3, Blood-Hall 3, War-Fane 2, Sanctuary 1.
- Goblin cults are almost all Teeth, Grovellers or Children. Cult names follow the founder's race, so the same pair of words recurs across a world: s2 has the Grovellers of Mambus, of Civono and of Ghamdark.
- Founders are close to indistinguishable: "Sworn to X, Y founded the ..." or "Calling upon X ...". Two of three phrasings do not say what moved the prophet. The founder's own cause (a ruler or a prophet) only shows in the block.
- Suggestion: a prefix and suffix check on the cult name against its site name, and a wider name list for goblin cults.

**F7. Gods are acts without presence in most worlds (note).**
- In s1 only 4 of 15 gods have any act; in s2 most gods do nothing; the six creator gods almost never act except to curse. A god of death, craft or ruin acts rarely (s1 g9, g13, g8: all "acts 0").
- The ones that act are lore, oaths and the wild. So "the gods as actors" is true of three or four gods per world.
- Suggestion: a weight for each domain so that war, death and ruin gods get a turn.

**F8. Acts crowd the late years (note).**
- Years 5000 and after hold 24 of 59 blessings, 42 of 79 curses, 23 of 42 marks, 26 of 48 foundings, 171 of 237 spreads and 11 of 13 champion raisings.
- The deep: 7 wakings in 16 worlds (4 in 4500 to 4999, 1 in 5000 to 5499, 2 from 5500). P5-07's crowding in the last 500 years is no longer the case, though the sample is small.

**F9. The follow-ups.**
- Deep's stir: the cause is shown (s0 e450 cites e339). The wording of the line does not say "stirred", only that the clan "woke" the horror. Fine.
- Renewals: 124 `alliance_renewed` events in 16 worlds, none in any summary. A visible gain over 272 lines in 16 worlds.
- Deepening lines: 92 lines in 16 worlds.
- Towers: about 33 abandoned against 9 taken up. s1 e20 "After the death of its last master, the Star-Tower of Femaeth ... stood empty" then s1 e374 repeat the line for another tower. The abandonment and taking-up read well, but "After the death of its last master" opens every one of them; the repeat is noticeable.
- Summaries: seals and aid are still not in summaries (P5-07 F1 stays open), but realm blocks do list `deep:` lines (s2 line 451, 469).

**F10. Viewer (note).**
- Shrines (small yellow figure) and temples (small house glyph) are about 8 px at this size. On goblin yellow, as in the map screenshot, they almost vanish; a hidden sanctum is a small dark ring that reads like a plague ring.
- The legend lists them with icons, which helps, but a player cannot find them without knowing what to look for.
- The Gods tab's cult block is readable and well made: "fa1 The Starward Circle of Ingleth ... founded 676 e22 by f3 Eloraemie the Learned at si3 The White Fane of Ingleth in s2 Iersaes; in c2 ... c6 The Alaralud League; faith of c2 since 725, c5 since 4380, c1 since 4489; present." Links are underlined. The event list under it follows the cult's life.
- The kind column is truncated ("the Maker of Elv...").
- Suggestion: draw shrines and temples with a dark halo, or at 3 cells on this size.

## What works (quotes)

1. "Snatkog Chosen of Briltick of the Sneegg Horde hunted down and slew Nirngidin, a giant of the mountains, in Larkdelve Vale." (s3 e710, 5645), after being raised at e698 and founding a cult at e700. A short life with a point.
2. "Angered by the sack of a city, Saeriemi the Maker of Elves sent a pestilence upon the Snoskrag Mob; 524 died." (s2 e377, 3943). The god's anger has a visible cost, and the pestilence is followed at e395 by a second curse for the faithful put down.
3. "The Snoskrag Mob put down the Grovellers of Civono." (s2 e537, 4421) after founding at e473: a cult founded, spread and ended in 200 years, with the realm named.
4. "The Snoskrag Mob took the Grovellers of Mambus as the faith of the realm." (s2 e348, 3772) after founding at 3650 and spread at e347.
5. "The House of Ithasfa under Itheithaell put down the Children of Gagg-Sneegg, and its faithful fled into hiding." (s2 e648, 4796): the hidden sanctum then appears in the block (si40 The Hidden Fane of Gagg-Sneegg).

## Verdict again

Merge. Before it, decide F1 (the blessings with no echo) and note F2 (suppression counter).

## Lead note (2026-11-28)

F1 was checked against the world files before the merge (49 worlds, the event cause table). Acts are cited: a blessing, curse, mark, champion or sent monster is the cause of a cult's founding (156 foundings), a champion's appointment as general (17) and a sent monster's raids (475); adoptions and suppressions are in turn causes of later blessings, curses and marks (126). What is missing is narrower: a battle won under a blessing's valour, growth under plenty and a pestilence's later ruin do not cite the act. That is a follow-up, with F2 to F10, not a reason to hold the merge. The review's counts are approximate, as its author says.
