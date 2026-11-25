# World critic review: P5-03a relations follow-ups (generator 0.25.0, history report v6)

Verdict: **accept with notes.** The relations now move in both directions and the causes mostly read as causes. The defects that remain are wording (a thaw is called "grew wary") and a summary that lost its milestones, not generation faults.

Material: default seeds 0-11 and `--monster-pressure high` seeds 0-3 at 1024. Images read: default-s1 and high-s0 (the two others not opened). Counts below are over the 12 default worlds unless stated.

## Point 1: do relations move enough, and believably?

Yes, enough: 12 default worlds have 49 `disposition_turned` events (0 to 7 per world; s3 and s5 have none), and the final tally is hatred 2-5, wariness 6-10, friendship 2-5, so every world ends with at least two friendships. Believably, mostly: the three friendship causes (long peace, a common enemy, shelter given) are things a reader accepts and the chains in `[causes]` are real (s6 e369: wars and sacks e247, e362, e368 give the hatred). The weak spots are that "grew wary" is used both for a thaw (hatred to wariness) and for a decline (friendship to wariness), which reads as a worsening when it is an improvement (F1), and that some pairs oscillate every 300 to 500 years (F2).

## Findings

**F1. "Grew wary" hides the direction of a turn, and some causes read backwards.**
The text for band 1 is always "grew wary of one another", whether the pair was warming from hatred or cooling from friendship. Examples that read as absurd:
- s6 e223 (4078) "After long years of peace, the Elves and the Goblins grew wary of one another." The `[peoples]` line says it was a thaw from hatred. As written, peace makes enemies wary.
- s10 e470 (5004) "After the raids of a common foe, the Goblins and the Faeries grew wary of one another." Also from hatred; a shared foe should read as a reason to put hatred aside.
- s7 e424 (5232) "After wars against a common enemy, the Elves and the Goblins grew wary" (thaw). Against that, s7 e389 (5019) "After the wars, the Elves and the Dwarves grew wary" is a real decline (from friendship).
Of 49 turns, 11 are "long years of peace ... grew wary" and 9 are "raids of a common foe ... grew wary"; nearly all are thaws.
Fix: two wordings per band 1, "softened toward one another" / "put aside their hatred of one another" for a thaw and "grew wary of one another" for a decline. Choose by the sign of the change.

**F2. Pairs flip back and forth on a fixed rhythm.**
- s1 Elves/Goblins: e121 3171 wary, e258 4066 hate, e346 4428 wary, e385 4679 hate.
- s6 Elves/Goblins: e223 4078 wary, e369 4493 hate, e387 4897 wary (same cause e17 both times).
- s7 Elves/Goblins: e318 4826, e374 4987, e424 5232, e597 5824. Elves/Dwarves: friends e208 4015, wary e389 5019, friends e436 5357.
- s8 Goblins/Faeries: e482 5285, e522 5438, e651 5732, e741 5839 (four turns in 554 years).
Each turn alone is fine; a pair that turns four times reads as a thermostat. The peace thaw even cites the same cause event (e17) on both thaws in s6, so the "cause" is not a new fact.
Fix: a cool-down after a turn (no thaw within about 300 years of a hatred turn unless something new has happened since), and for the thaw causes require a cause event newer than the last turn.

**F3. Friendship comes late and in a clump.**
Of 63 turns in 16 worlds, 30 fall in 5500-5999 and 14 in 5000-5499. Friendships in particular cluster at the end: s11 has two on the same year (e629 and e630, 5811); s2 5717, s4 5698 and 5870, s9 5953, s1 5897 and 5990. The turn to friendship of the Elves and Humans in s1 at 5990 is ten years before the present and nobody gets to see it matter. The cause is partly structural (Humans appear at 5003 and Orcs at 5534), but the Elf-Goblin-Dwarf-Faerie pairs have 2,000 years and still turn late.
Fix: the "long peace" test could be applied from year 3000 for pairs that have long met; also log a `peoples_met` follow-up for the pair after the first thaw so the player can see it matter (an alliance, a trade, a shared war).

**F4. The summary's civilization "milestones" are a constant and a quiet stretch.**
In 111 of 115 civilization blocks the line reads `milestones: 8` (the others 7, 5, 2, 1), so it is a cap, not a count. Below it the only line shown is a `quiet_stretch` (93 of the lines that appear; for the rest, nothing). Example, default-s0 c6 (the Duchy of Cear, a human realm razed by orcs at 5757): `milestones: 8` then "For 381 years ... the land was quiet" and nothing about its fall, which is only in `fate:`. A reader of the summary meets no foundation, war, sack or loss in a civilization's block.
Fix: show the top three milestones by weight with the quiet stretch dropped, and print the true count and "and N more" (as the ruins list does).

**F5. Undead "die of great age".**
s5 e910 (5783) "The Dead of Gik-Kutch died of great age in the Forsaken Downs."; s? e269 "The Host of Illniend died of great age". The dead cannot die of age. Undead also have "hoard 0" and always "died" in the monster tallies.
Fix: separate wording for undead ("lay still for good", "crumbled", "were laid to rest" or "were forgotten"), and let a long sleep end them rather than age.

**F6. Expulsions are almost all of Elves.**
Of 48 expulsions in the 16 worlds: Elves 35, Goblins 7, Faeries 4, Humans 1, Orcs 1. Typical: s2 e344 "Hating the Elves, the Warrens of Skozz-Noz drove them out of Ielelienn, where it had held them as subjects for 1,280 years; 302 left." (also s1 e215 and e229 at 860 and 1,280 years; s? e709 at 1,696). It follows from hatred (good, as asked), and the next event is often `refugees_arrived` with the kin (good). But the pattern is "the Elves are everyone's subjects and everyone's scapegoat", and an expulsion after 1,696 years of subjecthood feels arbitrary. Also the Elves' first meeting with the Goblins is almost always "hated from the first" (disposition -700).
Fix: fewer expulsions of a people held for over 800 years (the longer the hold, the lower the chance), and let a subject people whose ruler has meanwhile turned friendly be kept. Move the Elf bias to a proposal if it is meant (elves as the lost people is a theme, but it should have a reason in the text).

**F7. Mixed settlements shuttle back and forth on cessions.**
s1: Gnitch and Gatgrok go to the Elves at 4679 (e386, e387), then to the Goblins at 4961 (e464, e465); Aeglaes passes Orcs-over-Elves e794 (5683) then Humans-over-Orcs e950 (5811). 251 `settlement_mixed` events in 12 worlds (21 per world) and 34 `standing_changed`. The text is fine ("After the cession, in Sennfe the Humans ruled over the Elves, who outnumbered them."), but a run of eight identical lines on one day (s1 e1040-e1043; e642-e644) reads as a list, not as a history.
Fix: group same-year, same-parties mixings into one event ("in four towns, the Orcs ruled over the Dwarves") or count them in the cession event. `standing_changed` ("In Slok, the Khirnhu Throne tolerated the Goblins.") carries no cause and appears as four lines of the same text on one day (s1 e615-e618); same grouping.

**F8. Peoples met "by word": works, but all of it is the Dwarves.**
The wording is good ("The Dwarves first had word of the Faeries in Keenhurst, and were wary of them."); 13 such lines in the 16 worlds; 12 of 13 have the Dwarves as the hearers (s? e197/e198 Elves and Goblins hear of the Dwarves at 3395, the other 11 Dwarves hearing of Faeries, Humans, Orcs). In all 16 worlds every pair has met (`met 15`), so the unmet lines of `[peoples]` never appear and I could not judge them. The place named after "in" is the place of the hearer's frontier which is not stated.
Fix: say who carried the word ("through the Elves of ...") when the event has a cause; and let a world sometimes leave a pair unmet (a sealed-in people is meant to be a secret).

**F9. Ruins list: cap works, but the cap is inconsistent.**
High-s1: `ruins: 42` then 12 lines and "and 30 more"; default worlds with 15 ruins list all 15. The spec says cap 15; 12 are shown with 42. The listing is by settlement id, so the first lines are the ancient monster ruins and the last the recent sacks, which is a good mix. The cap drops the rest silently from the count of what you see (the 30 more are not named by kind or age).
Fix: show 15, always keep the oldest three and the five newest, and say "and 27 more (21 sacked, 6 abandoned)".

**F10. The risings of the undead read as consequences (good), with two notes.**
Cause chains are real: s0 e441 (5748) names e417, e423, e425 (battle, sack by the Krurz Horde); s0 e474 (5799) names eight events, the whole fall of the Duchy of Cear (e436 to e460). Early rises are plausible: s0 e156 (3348), 83 years after the sack of e149. Notes: (a) the text is always "After the slaughter and the sack" or "After the slaughter", never saying whose dead (the Cear humans in e474), and (b) in s0 the lair is named for the dead town ("Host of Huldbury"), which is good, but in s5 "The Dead of Gik-Kutch" rose and sank into sleep and woke again seven times in 800 years while the human duchy was alive: a mild flicker. The late-undead-only world (s3, s4, s5 have 3 to 4 monsters, mostly slain trolls) is not unlikeable but leaves the end state with no living beast; s3 and s4 end with `awake 0, asleep 0`.
Fix: "After the slaughter of the Humans of Huldbury, ...", and keep one living beast at the end in at least 7 of 8 worlds (e.g. a dragon that always sleeps rather than dies).

**F11. Wording.**
- Renamed regions: no empty-monster "'s Shadow" in any report (the one occurrence, high-s? "Grulimarn's Shadow", e466, has a name, and the monster is slain in it at e503 under that name; correct). Fixed.
- "the Forsaken Wood" is the new name in 10 of 21 renames across the 16 worlds, "Forsaken Steppe" 3, "Forsaken Downs" 3. In s0 two regions are "The Haunt of Beegsleerk" and "The Forsaken Wood"; fine, but the repetition across seeds makes the renamings feel stamped.
- "where it had held them as subjects for N years" always puts "it" for a civilization; I saw "it" and "their" used consistently, no error. "aside" and "where" forms read right.
- s0 e41 "Ierrael was abandoned; 472 survivors went to Elonoma, and 75 were lost." good.
- A lone line "p5 flare_up: 2993-3042" plus "(1 more event not shown)" in s0 is fine.
No capitalisation errors found in the sample.

**F12. Late secessions and the end of the history.**
s0 c9 (the Zuggsnak Horde) seceded in 5992, eight years before the end, with 2 events; s10 e889 (5988) a turn ten years before the end; s2 e850/e855 (5974, 5975). The final decade often holds a fresh event with no time to resolve. Not a fault, but it makes the present feel like a cut, not a state.

## Better than 0.24.0

- Friendship exists: 3 in 100 worlds before, now 2 to 5 pairs per world, with sensible causes (shelter given, long peace, a common enemy).
- Expulsions follow from hatred and always say for how long the people were held; the next event is the refugees' arrival with their kin or a tolerant host.
- "Had word of" for sealed-in peoples is a good short form.
- The `[peoples]` block is compact and tells which pairs changed and when (`turned 5818 e498 from wariness`, with event ids).
- Undead rise with cited causes and an early date; the summary lost about 100 lines against 0.24.0 (479 to 593 lines in default worlds) and reads faster, except for F4.
- The empty-name "'s Shadow" and the old renamed-region wording are gone.
- Territory images are legible; the ruins, markers and borders match the report (default-s1 shows red camps ruined along the south and west).
