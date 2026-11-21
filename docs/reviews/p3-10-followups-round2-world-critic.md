# World critic: P3-10 second round (generator 0.19.0 r17)

World critic, 2026-11-21. Material: `~/.cache/the-unmapped/out/p3-10-validation2/critic/` (28 worlds), the 375-world sweep in `sweep/`, `rounds-r17.txt`, the first round's folder and sweep (`p3-10-validation/`) and the 0.18.0 sweep (`p3-09-validation/`). Read against design doc section 5, the first round's review (`docs/reviews/p3-10-followups-world-critic.md`), P3-06 r17 section 22 and the round-2 validation report on `origin/p3/10-validation2`. I recounted my own round-1 measures with the same script on all three sweeps. On round 1 it gives exactly the figures I reported then (9 / 4 / 6 inert, 29 / 5 / 4 took, 56 / 9 / 6 declared late), so the columns can be compared. Defect D1 (default seed 73, e914) is not judged here, and I did not use anything after e914 in that world.

## Verdict

**Pass with notes.** F1 and F2 are fixed. The orcs are a real threat again wherever they fight. The one-year tribute wars over land the attacker already owns are gone, and wars over held land are now fought. The quiet lines and the name filter do what they were meant to do. Nothing in this round needs another rule change before the merge. Four notes remain:

- (N1) the orcs now take the first half of their age to arrive;
- (N2) the foothold cession and the claim sentence before it say the wrong realm holds the region;
- (N3) two new name fragments, "tard" and a faerie word-final "-ass";
- (N4) small matters.

## What works

**F1, the orcs.** Recounted for default (100), calm (20) and turbulent (20) worlds:

| Measure | 0.18.0 | r16 | **r17** |
| --- | --- | --- | --- |
| Orcs do nothing after 5750 (no claim, declaration, colony, sack or capture) | 0 / 0 / 0 | 9 / 4 / 6 | **0 / 0 / 0** |
| Orcs sacked or took any settlement | 41 / 10 / 10 | 29 / 5 / 4 | **37 / 10 / 10** |
| Orcs declared a war in 5800–6000 | 68 / 12 / 13 | 56 / 9 / 6 | **68 / 15 / 15** |
| Worlds with at most 2 orc settlements standing at the present (default) | 11 | 24 | **11** |
| Worlds where the orcs win no war (default / turbulent / calm) | 45 / 9 / 10 | 60 / 17 / 14 | **51 / 9 / 8** |

Every measure is back at 0.18.0 or better, except the share of default worlds with no orc win (51 against 45).

**The first round's four orc worlds** now read as the design asks:

- **Default seed 53.** The Warband of Shurzzaarz loses Ugu to a sack, "The Realm of Ranaes sacked Ugu of the Warband of Shurzzaarz; 204 died and 478 fled." (5764, e229). It then fights two drawn wars against Ranaes. From 5927 it sacks Elaereth and Miethies (e324, e328), takes Rearstead and Ashuldton (e343, e346), wins "The Warband of Shurzzaarz prevailed over the Kingdom of Ashodward after 3 years of war." (5946, e347), and takes Steckwick from the Kingdom of Bocksul (5974, e361). It ends with 13 settlements and 53,493 people, and in 5994 it lays a fresh claim on Houndside Weald (e368).
- **Default seed 0.** The Krurz Horde conquers a human realm: "The Krurz Horde sacked Alardton of the Duchy of Cear; 1,346 died and 3,142 fled." (5764, e371), then it takes Ster and Waldtaldford, and the Duchy of Cear ends, conquered (e394, 5787). The Horde loses one war to the Duchy of Deas (e441, e442, tribute 2,531) and keeps fighting.
- **Calm seed 0.** The Krurz Horde wins all five of its wars (e221, e241, e276, e303, e325), with three sacks. Its last act is "The Krurz Horde sacked Koudinbai Hold of Clan Drourd; 646 died and 1,510 fled." (5998, e323).
- **Turbulent seed 14.** The Baldbaal Horde is sacked and flees in 5828: "The survivors of Gruush fled into hiding and founded Uraghgrurk in the Stricken Downs." (e805). It comes back out after the 100 hidden years. It searches Keenscar Wood in vain (5931–5936), then wins two wars in a row with sacks of Aladham and Asheas (e907, e913) and the capture of Rantbury (e947). At the present it has two wars under way. This is the recovery that 22.2 was written for.

**F2, aim over held land.** At defaults:
- Attacker wins in at most one year with no battle: 0 (r16 22, 0.18.0 2).
- Pairs and goals with two or more one-year tribute wins: 3 (r16 9, 0.18.0 3).
- The three that remain are fought raids, not free wins. For example, in default seed 98 the Kruggnugg Mob raids the Realm of Fansa's Thrallbeck Wood in 2345, 2920 and 3321, each time with a battle and a sack, and the elves resettle the wood in between. That is a cycle a reader can follow.

The first round's worlds:
- **Seed 23.** The Ancient Holt is now a causal chain worth following:
  - the Urarz Horde sacks Fiecaes (5794, e705), and the region is left empty;
  - the Kingdom of Durd settles the ashes three years later: "The Kingdom of Durd sent settlers from Ruckstead to found Alelbuldbury in the Ancient Holt." (5797, e710);
  - the Horde claims the Holt from Durd (e738) and fights a ten-year war for it, with three battles (it loses the first);
  - "The Urarz Horde took Alelbuldbury from the Kingdom of Durd; 253 died." (5918, e785) closes the war by aim.
- **Seed 7, Wardgrass.** The war is 22 years long (e437–e464), with five battles, ended by the sack of Grit Pit: "The Hold of Tokgru sued the Sneerk-Grik Mob for peace after 22 years of war." (5654, e463).
- **Seeds 73 and 88.** No one-year war without a battle; seed 73 was read only up to e914.

**F3, quiet lines.** At defaults there are 729 quiet lines, the shortest 50 years. None overlaps a declaration, battle, siege or sack. The long elven solitudes read as before.

**F4, names.** No name in any of the 375 worlds has three equal letters in a row or contains pee, poo, tit, fass or cum. I scanned civilizations, settlements and gods across every set.

**The 2048 world** (seed 42) keeps its grim last age. After 5600 it has 700 events, 12 sacks, 8 captures and 3 realms conquered:
- an orc realm falls: "The Warband of Uraa fell to the Dord Throne." (5899, e1189);
- "Clan Ol'thul sacked Gloeliel of the House of Finen; 2,222 died and 5,185 fled." (5882, e1140);
- "The Kingdom of Burdid fell to the Burd League." (5963, e1306);
- "The Warrens of Dribnark fell to the Duchy of Teck." (5992, e1389).

Orcs stand at 25 settlements here, against 14 in round 1. Clan Ol'thul wins four of its eight wars against elves and dwarves and ends with 15 settlements.

## Findings, by weight

### N1. The orcs take half their age to arrive (made later by 22.1)

The hold-back keeps an orc realm from going to war until it has a second settlement. An orc realm's first colony comes late, a median 235 years after its awakening. That figure is unchanged since 0.18.0 (248), and it follows from orc `growth_permille` 18 and `expansion` 600. Together they push the orcs' first act to the second half of an age that lasts a median 420 years:

| Default worlds, years from the orc awakening to the first orc colony, war, sack or capture | 0.18.0 | **r17** |
| --- | --- | --- |
| median | 202 | **232** |
| 10th percentile | 84 | **192** |
| worlds with 200 or more silent years | 52 of 100 | **88 of 100** |
| first orc declaration, years after the awakening (median) | 175 | **244** |

The claim in the awakening year is the only line the orcs get in the meantime. Some examples:
- **Seed 53.** "The Warband of Shurzzaarz laid claim to Balestrand, held by the Realm of Ranaes." (5536, e174). Its next line is a colony in 5756 (e220).
- **Default seed 14** (`orc1-seed14`). The Baldbaal Horde makes three claims by 5621, then nothing until 5857.
- **Default seed 22.** Clan Durz'shugh has a single fort, Kram Fort, from 5638 to 5910, and five claims it never acts on.
- **Default seed 88.** The Warband of Azok has one settlement from 5524 to 5993. Its one war, declared through 22.1's side door on a non-quarrel claim (e910, 5699), is lost. It is then sacked by goblins and flees (e1028, e1029).

The age is not dull, because the goblins fight through it (seed 61 has 12 declarations in the orc age without one by the orcs). But design section 5 says "their coming opens the most recent age of conflict", and the reader meets a people that sits still for two centuries and then fights hard for the last two. The orcs read as late, not as weak, so I do not block on it. T10 measures only the last 250 years, so it cannot see this.

**Suggestion** (a race rule, not 22.1):
- Let a race with `awakening_quarrel` arrive as a horde: found two settlements at its awakening (the camp and a fort), or start the first settlement at a population that can send settlers within about 50 years.
- Measure the first-act median at defaults and aim for under 100 years.
- Raising orc `growth_permille` to the goblins' 20 alone would gain little.
- Keep the age's length, as Paul has decided.

### N2. The foothold cession, and the claim before it, name the wrong holder

The rule does the right thing to the rows. The sentence does not. In both foothold worlds the attacker has held the region throughout, and the sentences say the opposite.

**Default seed 40, the Graven Thicket** (region 28). `region_control` has one row: the Zatzeegg Mob, from 2134, its home. The Bizz-Skuz Horde, which broke away from the Mob (3356, e74), took the Mob's first town in 3731: "The Bizz-Skuz Horde took Greerk-Zark Pit from the Zatzeegg Mob; 246 died." (e102). The sentences then read:
- "The Zatzeegg Mob laid claim to the Graven Thicket, held by the Bizz-Skuz Horde." (4193, e168)
- "The Bizz-Skuz Horde ceded the Graven Thicket to the Zatzeegg Mob." (4254, e198), after a 35-year war with seven battles.
- And in one year (5859), "The Zatzeegg Mob laid claim to the Graven Thicket, held by the Skeek-Kigg Mob." (e744) beside "The Skeek-Kigg Mob laid claim to the Graven Thicket, held by the Zatzeegg Mob." (e745). Each line names the other as holder.

The story underneath is one of the best in the folder: a mob fights for 35 years to win back the town it was founded in. The reader cannot see it, because the town is never named. Instead they are told that the Mob's homeland, which it never lost, was held by another realm and then handed back.

**Default seed 58, King's Fold** (region 14). It has been the Snat Horde's since 2410. Yet the text says:
- "The Snat Horde laid claim to King's Fold, held by the Stinrild League." (5930, e889)
- "The Stinrild League ceded King's Fold to the Snat Horde." (5969, e922)

What passes is Drutnot Hole, after Nutskrank was sacked (e915).

The claim sentence has the same fault wherever the claimant holds the region: 46 claims in 29 of the 100 default worlds, and 21 in `civs4`. Seed 7's "The Sneerk-Grik Mob laid claim to Wardgrass, held by the Hold of Tokgru." (5041, e383) is one of them: Wardgrass has been the Mob's since 4978.

**Suggestion** (wording only, read from rows the renderer already has):
- When the claimant holds the region at the claim, write the claim on the holder's towns: "The Zatzeegg Mob laid claim to Greerk-Zark Pit in the Graven Thicket, held by the Bizz-Skuz Horde." With several towns, write "to the Bizz-Skuz Horde's holdings in the Graven Thicket".
- Write the foothold cession as a giving-up of towns, not of land: "The Bizz-Skuz Horde gave up Greerk-Zark Pit to the Zatzeegg Mob." With several towns, write "gave up its 2 towns in King's Fold to the Snat Horde".
- The kind stays 29. The variant is chosen by "no `region_control` row opened", which validation already uses to count footholds.
- The war line can stay "over the Graven Thicket".

This should be in place before P3-07 builds the report on these sentences. It need not hold this merge: it touches 2 cessions and 46 claims in 100 worlds.

### N3. Names: "tard" and a faerie "-ass"

From the 100 default worlds (validation's list, recounted):

- **"tard"** in 8 names:
  - human: Tardham, Tardbury, Tardwick, Tardwuldwick;
  - dwarven: The Hold of Tardnu, The Horktard Throne;
  - Stard and Stardton, which read as harmless.

  Tardham and Tardwuldwick read as a slur fragment in English. **Suggestion:** a `tard` substring in the name filter for every culture. Stard and Stardton are an acceptable loss.
- **A word ending in "ass"** in 11 of 960 faerie names, and in no other culture's:
  - Fiesass, Liass, Miass, Soubiass, Yshiass, Waeshass, Fleeshiass;
  - Pamiass Hollow, Twoutwiass Hollow;
  - The Circle of Wass, The Circle of Twaepass.

  Fiesass and Liass read as jokes. Soubiass is the seat of a founding realm in default seed 53 (4227, e69). **Suggestion:** a word-final `ass` filter entry for faerie names, unless the syllable rework Paul has decided removes the `-ass` ending anyway. Region names (Blindgrass, Eaglepass Wolds) are outside the filter, as they should be.
- **Not worth an entry:**
  - the goblin `gnob`, `nob`, `snot` and `git` names (in character);
  - the faerie `wee` names, which are left for the rework;
  - Alaledumbury and Tirkbumm, which are single cases.

### N4. Small matters

- **T8, 71 of 100 worlds with an orc declaration.** In the worlds this does not read as a problem. The 29 worlds without one break down as follows:
  - 13 never raise a claim at all: isolated among goblins, the case Paul has accepted (0.18.0: 11). One example is seed 61, where the Warband of Zaash founds 12 settlements across the steppes and never quarrels;
  - 8 are attacked but never declare;
  - 8 hold claims and never declare (0.18.0: about the same number).

  So the fall from 0.18.0's 75 is about four worlds, mostly in the accepted case. Total orc declarations are down more than T8 suggests: 220, against 306 at 0.18.0. But the wars the orcs do declare are won far more often (as attacker, 94 won and 36 lost, against 111 and 57), which reads better.
- **Turbulent T5, 23 won against 22 lost.** Acceptable:
  - as attacker the orcs win 22 and lose 14;
  - 8 of the 22 losses are defences against other peoples, who also fight more in `turbulent`;
  - 0.18.0 had 22 won and 15 lost in 60 wars, and r17 has 73 wars;
  - the worlds with no orc win are 9 of 20, as at 0.18.0;
  - turbulent seed 14 reads as a threat.
- **The seven one-settlement worlds** (defaults 14, 38, 52, 55, 65, 81, 95). Five or six of them are good stories, and all leave a place worth finding:
  - **Seed 52, the best.** Clan Mok sacks Ithiecieme and Nieglaell and wins Chieftain's Bargain (e251, e258, e261). It overreaches into five wars and is stripped by the Duchy of Rurteas ("The Duchy of Rurteas took Grorz from Clan Mok; 330 died.", 5896, e292). It ends with a vengeance claim on its own homeland: "Clan Mok laid claim to Dustwell Reaches, held by the Duchy of Rurteas." (5983, e349). Here the "held by" is true.
  - **Seeds 81 and 95, broken and hidden.** Clan Kraal flees to Kok Fort in Swartreach (5942, e285). The Thaaguragh Horde is sacked four times in 14 years and flees to Azorz in the Grim Woods (5919, e366). Both refuges are still hidden at the present, which makes them good places to find.
  - **Seeds 14 and 55, fought and lost.** The realm fights and loses ground: the Baldbaal Horde wins one tribute and loses twice; Clan A'thuukh loses three forts in a row (e606, e648, e657).
  - **Seed 38, hapless.** The Warband of Uukh'bok fights six wars and wins none. Two are searches of hidden faerie woods ("The Warband of Uukh'bok searched Drover's Forest for the Taellsa Court and found no one; the war ended after 3 years.", 5799, e533). It ends by ceding its homeland, Usurper's Gate (5965, e653). Grim, and a little comic.
  - **Seed 65, stagnant.** The Warband of Muz has one camp for 378 years. It never founds a colony and never declares. It is besieged twice, "with nowhere to flee, its people held on" (e1078, e1086). This one is an N1 case.

  Seven in 100, with stories like these, is a fair share of fallen hordes.
- **War goals that differ from the claim they cite.** Examples:
  - "Clan A'thuukh declared war on the Kingdom of Sunt over East Wood. (causes: e510, e540)" (default seed 55, 5954, e624), where e540 is a claim on Woodward's Shaw;
  - turbulent seed 14, e873: a war over Keenscar Wood caused by the claim on Oakholt (e576).

  This is not new. A reader cannot follow it, which matters for P3-07: say why the goal moved, or cite the claim the goal came from.
- **Identical battle tolls.** "642 of the defeated and 205 of the victors fell" occurs twice, 73 years apart (seed 53, e238 and e294). This is not new, and it can wait for P5.
- **Not changed, still open from round 1:**
  - "with 1 settlement" (40 times at defaults);
  - repeated "besieged …; it held";
  - large tributes (3 above 5,000; the largest is 9,492).

## Patterns across seeds (1024 defaults unless stated)

- Orcs act after 5750: in 100 of 100 worlds (calm 20 of 20, turbulent 20 of 20).
- Orcs sack or take a settlement: in 37 of 100 worlds (calm 10, turbulent 10).
- Orcs declare in 5800–6000: in 68 of 100 (calm 15, turbulent 15).
- Orcs silent for their first 200 years: in 88 of 100 (0.18.0: 52).
- Claims of a region the claimant already holds, written "held by" another realm: 46, in 29 worlds.
- Foothold cessions: 2 at defaults, and 5 in `civs4`. Both default ones read wrongly.
- One-year wins with no battle: 0. Repeated one-year tribute wars: 3 pairs, all fought raids.
- Quiet lines: 729, none shorter than 50 years, none over a war event.
- Crude names: 8 "tard" names and 11 faerie "-ass" names. No other entry I would add.

## Suggestions, as rule and parameter changes

1. **(N2, before P3-07)** Kind 20 and kind 29 sentences get a variant:
   - when the claimant holds the claimed region at the claim, name the holder's towns in it;
   - when a `region_ceded` opens no `region_control` row (a foothold cession), write the towns given up, not the region.
2. **(N3, next change)** Name filter: `tard` (every culture); word-final `ass` (faerie, unless the syllable rework removes it).
3. **(N1, next balance pass)** A race with `awakening_quarrel` 1 arrives as a horde, with two settlements or a founding population able to colonize within about 50 years. Target: a median first orc act under 100 years after the awakening at defaults. T5, T9 and T10 to be re-measured with it.
4. **(N4, P3-07)** When a war's goal differs from the region of the claim it cites, say so in the sentence or cite the right claim.

## Questions for Paul

1. **N1.** Should the orcs strike within their first century, or is a horde that gathers for two hundred years and then breaks on its neighbours the intended shape? If the first, suggestion 3 is a data and rule change for the next balance pass. The orc age keeps its length either way.
2. **N2.** For a foothold cession, is naming the towns given up ("gave up Greerk-Zark Pit to the Zatzeegg Mob") the right reading of the peace? The alternative is to keep "ceded the Graven Thicket" as a giving-up of a claim to the land.
