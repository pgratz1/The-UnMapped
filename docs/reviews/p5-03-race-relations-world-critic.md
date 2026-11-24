# World critic review: P5-03 race relations, mixed settlements, migrations (generator 0.24.0)

Material: 12 default and 4 `--monster-pressure high` seeds at 1024 (`critic/`), and for counts the 100 default worlds in `sweep/default/`. Seeds are named as in the files (default-s0, high-s1, sweep s12 and so on).

## Verdict

Not a blocker. The new events are caused, readable and in tone, and the monster side (flights, bold beasts, undead) is clearly better than in P5-02. The weak point is shape, not correctness: relations are frozen for about 3,000 years and then turn all at once in the last 500, and the mixed-settlement lines are one sentence repeated. Fix those in a follow-up, not before merge.

## Findings, most serious first

### F1. Race relations are static, then ratchet late and in one direction

Evidence, 100 default worlds:
- 149 `disposition_turned` events. By year: 3000s 1, 3500s 10, 4000s 18, 4500s 17, 5000s 22, 5500s 81. 54% fall in the last 500 years of a 6,000-year history.
- Direction: 116 "came to hate", 30 "grew wary", 3 "became friends". Relations only sour. Friendship happens three times in 100 worlds and is never lost. Median turns per world is 1.
- Final tally in 77 of 100 worlds is one of three patterns (`hatred 5, wariness 8, friendship 2` in 35; `4/9/2` in 25; `6/7/2` in 17). The first-meeting disposition from the race data effectively decides the whole history. Example default-s0: Elves/Goblins "hated them from the first" (-700) at 2068 and never move; at the end they are still -700.
- In default-s0 the only turn is Humans and Orcs at 5849 (e506). Between the 2068 meeting and 5849 no pair changes.

The turns that do happen read well ("After the conquests, the wars and the sacks, the Goblins and the Humans came to hate one another", default-s3 e786, with eight causes). The problem is that the first 4,000 years of the chronicle show no mood change anywhere, and a fixed table of starting bands is visible to anyone who reads three worlds.

Fix: let gentle drift happen earlier (smaller thresholds in the first half of the history, or a decay toward the pair's baseline so a long peace can soften wariness and shared refuge can build friendship). Add a way back: an alliance or shared enemy can turn hatred to wariness. Make "became friends" reachable by help given (refugees sheltered, "years of shelter given" is already in the cause table but nearly never fires: 3 friendships in 100 worlds).

### F2. Dwarves never meet anyone in about 1 world in 8; the summary does not say so

16 of 100 worlds have `met` below 15 (10 pairs in 10 worlds, 12 in 2, 13 in 1, 14 in 2, 6 in 1). In default-s4 and default-s6 the dwarves (Brun Throne, c4 in s6, founded 3303) never meet elves, goblins, faeries, humans or orcs in 2,700 years; the full `[peoples]` block says "Elves and Dwarves: never met" but the summary block gives only the count ("met 10") and no names. A reader cannot tell which pair is the isolated one.

Fix: in the summary, add one line naming the unmet pairs when any exist. Consider whether `meet_hops` should be raised or a rare explorer event added. An isolated people is good dark-fantasy material, but it needs to read as a fact ("The Dwarves have never met the Humans"), not as a hole.

### F3. Mixed settlements are one sentence, repeated, and nearly always 90/10

- 1,855 `settlement_mixed` events in 100 worlds (18.5 per world in the full level; 56 reach the summary of 16 worlds read).
- 1,395 of 1,855 (75%) have `share=900`, the cap. In default-s6, six of eight mixed places are "Elves 900 subject; Humans 100 ruling". Every list line looks the same.
- 1,377 of 1,855 begin "After the cession, the ..." and read "the Elves and the Goblins lived side by side in X". default-s5 e123, e172, e183 are three of these in 500 years, the same two races, same wording.
- The wording is neutral. "Lived side by side" hides that this is a conquered or ceded town where a ten percent ruling minority holds a ninety percent subject majority.

Fix: use the standing in the sentence ("where the Goblins ruled over the Elves"), vary it by share (a ruling minority, a divided town), and cap the summary to the first of each pair of races and the largest. Check why the share saturates at 900; if the cap is a rule parameter, a lower cap or a graded share would give variety.

### F4. Expulsions have causes that are real but far away, and the text does not give them

All expulsions cite a `region_ceded` (or capture) as cause, but the gap is large: default-s5 e544 (year 4849) cites e122 (2847), 2,000 years earlier; high-s1 5594 cites 2648; default-s1 5889 cites 2711. The template has no cause clause for ceded regions, so the line reads "The Drunogg Horde drove the Elves out of Thiefann; 268 left." with no reason in the sentence. The chain exists only in the cause list.

Strengths: when one civilization expels repeatedly the series reads as a policy (default-s3: the Sneegg Horde drives out Elves at Ortho 5668, Innglinn 5678, Mithoithi 5686, Ithaellfe 5706, Lece 5736, Cerithi 5753, Aendro 5950), and 39 of 105 refugee arrivals say "After the expulsion", which closes the chain well. Of the 100 worlds, 19 have no expulsion and the maximum is 22 per world; the long series is the repetitive end.

Fix: add the cause words to the template ("Long hating the Elves of its ceded towns, the Drunogg Horde drove ...") using the pair's band; limit a civilization to a few expulsions per generation so that the series is not eight in 300 years; show the hatred band that licensed it.

### F5. Late pile-up of everything: undead, expulsions, turns

- 142 of 191 `undead_rose` events (74%) are after year 5500, and many in the last decade (high-s2/default `5997`; default-s0 `5979`, `5983` flights for fear of the Restless of Goatherd's Fall).
- 734 monster raids in the 2000s decade-block, then 5500s have 1,121 of 3,607 (31%).
- Expulsions cluster after 5000.

For the present-day picture this is a gift: the player arrives into live threats. For the chronicle it makes history feel like 5,000 quiet years, then a rush. Undead follow sacks that are late because the last races arrive late (orcs at about 5550), so this is partly structural, but a reader will notice. Suggest the undead rule allow an earlier rise (a lower slaughter threshold) so that old ruins from the 3000s can be haunted too; this also makes F6 better.

### F6. Old ruins stay in the list: acceptable, with an upper end that is too long

Ruins no longer fade. The [present] ruin lists run median 19, max 51 (sweep s12, 51 lines of 86 in `[present]`). In default-s0 the list of 15 reads well and is a plus: Ierrael (ruined 2083, "abandoned for fear of Old Beegsleerk"), Fievos (2071, "overrun by Old Beegsleerk") and Mielae give a real 4,000-year-old ruin cluster around one troll lair, and the map PNG (default-s0.png) is not cluttered by their markers. Clutter comes only in the worlds with 30 to 51 ruins.

Fix: in the summary print only the oldest 5, the five largest at their fall and the five most recent, then "and N more"; leave the full level whole.

### F7. Summary length: the `[civilizations]` block is the thing to cut

Sweep summary lines: min 396, median 620, 90th percentile 708, max 785 (so no 900 in these 100; the largest seen in critic/ is 694). Breakdown for the longest sweep worlds: `[civilizations]` 326 to 335 lines against `[chronicle]` 229 to 311. The civ blocks restate the chronicle: each carries about 8 "milestones" that are chronicle events copied (default-s0 c2: `3093 e105 war_declared`, `3101 e110 sacked`, `3101 e111 war_won`, each already in the chronicle), plus 12 fixed header lines, plus an "adversaries" sub-block.

Cut first: the milestones (they duplicate the chronicle; keep only the quiet-stretch line and the fate), then "without war: N (enmity ..., none ...)" lines, then the ruin list tail (F6). That alone brings the median under 450 without losing information.

### F8. Wording and reading errors

1. Plural monsters with a singular pronoun: "The Screamers of Nux-Nink, a brood of wyverns, made its lair" (high-s1 e14); "drove the Screamers of Krozigg back to its lair" (e688); "drove the Host of Ferrybrake back to its lair". "Brood", "Host" and "Screamers" are collectives; pick "their" for names starting with "The ... of" or for those kinds.
2. Redundant tail: "hunted down and slew the Dead of the Red Heath, a host of the restless dead, in the Red Heath" (default-s0 e553). The kind phrase repeats the name and the place repeats the name. Drop the kind clause for undead.
3. Undead rising text names the lair place differently from the host: "the dead of the Sallow Woods rose as the Dead of Aladdald" (default-s0 e511). A reader sees two names for one place; use the same one.
4. A raid that cites an earlier raid says nothing about it: 296 of 3,607 raids carry a second cause, but the wording is the same six verbs ("fell upon" 114, "came down by night on" 75, "prowled the edges of" 68, "came out of the barrows against" 49, "walked by night in" 48, "crept into" 44 of the sample counted). "Returned to" or "again" in the line when the second cause is a raid on the same place would show the repeat (e.g. default-s1 e251 and e255, Mossback Nux-Nink at Taewan Glade in 4178 and 4179, "fell upon" then "prowled the edges of").
5. `refugees_arrived` has the cause prefix missing for plague/famine abandonment ("123 Elves from Thiordell found shelter in Leithiedell", default-s0 e120, cause is a plague) while sacks, expulsions and ravage get "After the ...". Add "After the plague".
6. "refugees ... among their own kin of X" is 82 of 105 arrivals; "held as subjects by" appears 0 times in the 105. The standing words are in the template but one case does all the work.
7. Mixed-settlement list ordering: "Goblins 567 subject; Dwarves 433 ruling" (default-s1 s38) against "Dwarves 663 ruling; Goblins 337 subject" (s73) — the order follows race id, not rule; put the ruling race first.
8. `expelled: 1 times` (default-s6) and `settlements at E`: singular/plural slip.

### F9. Monster names

Plain English words as the whole name or the root: 4 of 322 monster names in the sample of the 100-world summaries (the summary shows only the top three of each world): "Slog" (troll), "Ok'uruuk" (contains ok), "The Screamers of Drab" (wyvern), "Zug"/"Zuul" type names are fine. Settlement "Nix Hole" (default-s0 s41) is another. This is rare enough to ignore for now; drab and slog are still two of the known list and appear at about one monster in 80.

Epithet variety is thin: "the Grey" 15, "of the Fells" 14, "Stonehide" 13, "the Black" 8, "the Gold-Eyed" 8, "Ashwing" 8 in 322. A world with six trolls will have two "Stonehide" or two "the Grey" (default-s1 sweep shows Zug the Grey twice among 12 seeds). Not harmful, but undead names are better: "The Restless of the Weeping Downs", "The Host of Beechshaw", "The Dead of Sneenk" read as the dark, place-bound haunts the design asks for. 112 of 322 monster names are undead.

### F10. `[peoples]` block

The summary block is five lines and is clear. The full block is good: one line per pair with disposition, meeting year and event, and the mixed list with ruling/subject shares. Two gaps: the summary omits never-met pairs (F2), and it omits the pair when it turned unless it turned; "Humans and Orcs: hatred -607, met 5534 e317, turned 5849 e506" does not say what it turned from (wary, -250). Add "from wary".

## Answers to the brief's questions, short

1. First meetings: caused by awakening or founding (cause is the younger civ's founding event) and honest, but the same event repeats up to five times in one year and region when the orcs or humans arrive (default-s0 e313 to e317 all 5534, Southeast Steppe). Merge into one line with five reactions, or list only the ones that differ. Turns read well when they happen; they are late and one way (F1).
2. Mixed, refugees, expulsions: plausible and well chained (cession, mixed, expulsion, refuge) but repetitive (F3, F4).
3. Flights: 177 in 100 worlds, 38 worlds have none; default-s0/default-s1 flights end in an abandonment with a destination and fit the ruin list well. Bold beasts: 3.5 per world, one line each, fine and varied by beast. Raids vary in verbs only (F8.4).
4. Old ruins: help more than hurt (F6).
5. `[peoples]`: good (F10).
6. Names: acceptable (F9).
7. Summary: cut the milestones first (F7).
8. See F8.

## What improved since P5-02

- Flights for fear exist and chain: monster, flight, abandonment with survivors' destination, ruin line "abandoned for fear of Old Beegsleerk (m3)" (default-s0, s3 Ierrael).
- Undead hosts are named after what they rose from and rise from slaughter and sacks with their causes listed (default-s0 e495, e511; high-s1 e944 with seven causes).
- Ruins now carry their age; a 4,000-year-old ruin is on the map and in the list.
- Refugee and expulsion chains close: expulsion, then "After the expulsion, 410 Elves from Ortho found shelter in Vienienae, among their own kin of the House of Roelaele" (default-s3 e565, e566).
- The summary keeps the monster stories to three lines with "and N more" in `[monsters]`.
- Capitalisation at sentence start is correct in all lines I scanned (no lower-case starts in 16 full reports).
