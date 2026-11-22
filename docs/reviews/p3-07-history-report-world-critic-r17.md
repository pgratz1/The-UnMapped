# World critic: P3-07 history report, second round (revision r17)

World critic, 2026-11-22. Reports read: `~/.cache/the-unmapped/out/p3-07-impl/samples-r17/`, preset `linux-gcc-release`, generator 0.20.0. I read in full the summaries of the phrase world, seeds 0, 5 and 7 and `civs4` seeds 3 and 7 (blocks of `civs4` 7 by sample), and the first third of the phrase world's full report. I counted over all 15 summaries. Quotes give the world and the event id.

## Verdict

**Pass with notes.** Of the ten first-round findings, six are fixed, three partly fixed and one not. The summary now reads as a history: ages, then the chronicle, then each realm with a war tally, then the present. Paul can be given the phrase world, seed 7 or seed 5 as it is. Two things should be done first, both in the report only: the lowest summary threshold floods seed 0 with plagues (B1), and ruins caused by an abandonment print a raw kind name (B2).

## A1 to A10

| # | State | Evidence |
| --- | --- | --- |
| A1 declarations dropped (`civs4`) | **Fixed** | `civs4` seed 3 now shows 28 declarations for 27 war endings, 29 against 29 in seed 7. The orphan of round one now has its war: "5628 e1592 war_declared: The Warrens of Sleex-Skrib declared war on the Kingdom of Stid over Usurper's Refuge. (Claimed since 5082.)" then "5638 e1627 settlement_captured: ... took Waldford from the Kingdom of Stid". Length is unchanged though (B4). |
| A2 claims fill the summary | **Fixed** | The chronicle holds no `claim` line. A pursued claim lives in its war's note ("Claimed since 2344."). `[present]` lists the standing ones: "e273 4327 The Realm of Lecier (c1) on The Gnokkirk Horde (c3): Yewholt (rg39)" (phrase). Seed 7 has 9, `civs4` 3 has 38. |
| A3 quiet stretches dropped | **Fixed** | All shown: "2644-3326 e78 quiet_stretch: For 683 years, from 2644 to 3326, the land was quiet, and three realms kept the peace." (phrase). Short ones (25 years) still hide, see B5. |
| A4 relation rows doubled | **Fixed** | "The Baskeeb Mob (c2): 6 wars, won 4, lost 2, drawn 0, open 0, 2893-5863; now at peace" (seed 0, c1); also `seceded from: The Gnokkirk Horde (c3), e481`, `wars: 27 declared, 1 defended; won 14, lost 2, drawn 12, open 0` and `last capital` for an ended realm (`civs4` 3, c14). I checked the tallies of phrase c1 against the chronicle (e65, e161, e164, e220): they agree. Residual noise in B3. |
| A5 milestones | **Fixed** | Eight, own deeds first. Seed 0 Nife now lists "4087 e138 war_won" and "5040 e221 war_won"; the generations note prints: "(The House of Nife: 12 new generations of rulers.)". Realms with no wars fall back on famines ("Clan Durgarin: 5064 e229 famine: Famine struck Nidinnird; 214 died.", seed 0), which is thin but honest. |
| A6 `[ages]` count | **Fixed** | "a6 The Age of Orcs: 5638-5999, 362 years; ...; 4 civilizations founded: 1 at the awakening, 3 by secession" (phrase). |
| A7 age floor fills with small calamities | **Partly fixed** | Phrase, Age of Faeries, p11: 9 of 17 lines are plague, famine or succession (e232 to e264). Seed 0 is the worst case, see B1. In the other 14 worlds calamities are 8 to 20% of the chronicle, down from the first round's 20-line runs. |
| A8 battle texture | **Fixed** (T of 26 or more) | One battle per war, the deciding one: phrase e64, e72, e214, e219. At T=20 (seed 0) sieges still print in runs: "e55 / e57 The Baskeeb Mob besieged Mielae ...; it held." |
| A9 order, form, `[present]` | **Partly fixed** | Order works. `[phases]` is gone from summary. `[present]` now lists ruins with the year, event and who sacked, refuges, open wars and standing claims (seed 7: 2 open wars, 1 refuge, 7 ruins, 9 claims). Refs and causes stay (accepted). `[stats]` is still 146 lines at the end of every summary (seed 0, lines 500-645). |
| A10 `became_city` size | **Not fixed** | "Guldton grew into a city of 8,414." (phrase e349), "Lafin grew into a city of 8,017." (seed 0 e176); seed 0 has four lines of 8,005 to 8,035, seed 7 "8,010" and "8,006". |

## What works

- **A realm's story can be followed from the summary alone.** Phrase, Realm of Lecier: founded (e2), twelve generations of quiet, a colony in 1675, a war in 2594 over Yewholt, a sack in 3339 (e155) with the seat moved to Simi (e156), peace and a cession in 3341, two tributes in 4072 and 4077. Its block says "wars: 0 declared, 6 defended; won 0, lost 3, drawn 3" and the `[present]` list still carries its claim on Yewholt (e273). That is a grudge told in four places, each agreeing with the others.
- **The new order is right.** A reader meets the whole chronicle first (about 160 to 240 lines at default), then blocks that restate each realm with counts, then the ending.
- **The adversary line is the best addition.** It shows a runaway as one sentence: seed 5, the Slatch Horde: "wars: 27 declared, 1 defended; won 14, lost 2, drawn 12", eight wars with Saethoth "2859-5886", beside "17 regions, 62 settlements, population 147,969".
- **`[present]` gives play.** Seed 7 ends with "open wars: 2 ... e651 The Sneerk-Grik Mob (c4) against The Realm of Ienngleth (c1) over The Ancient Grove (rg11), since 5980", a hidden refuge ("s92 Orz'thugh Camp ... founded 5891 by flight from Muush Fort"), and a ruin made in the last year ("s12 Rofis ... ruined 5999, e674 sacked"). Every ruin names the sacker and the last holder; seed 5 shows nine, seed 0 eleven.
- **Dark high fantasy.** Seed 0 reads as an elven house ground down by six wars with a goblin mob; phrase has a faerie court that "found no one" when hunted (e466) and a chain of eight war_declared lines against one goblin horde. Loss, ruin and old grudges are what the text is made of.

## Findings (a): report text, no world change

### B1. The lowest threshold floods the first world Paul will open

Seed 0 prints at `threshold = 20`: 104 of its 221 chronicle lines (47%) are plague, famine or succession crisis. The final 300 years (p17) are about 55 lines of them against two wars. The lines include "Famine struck Sluggatch Hole; seven died." (e418) and "...13 died." (e419) a year later. Round one had T=26 for seed 0 with 166 lines shown, and the first review's A7 asked for fewer of exactly these. Other seeds: phrase 14%, seeds 1 to 9 between 8 and 20%.

**Fix.** Give the threshold a floor at the salience of the warfare kinds' lowest (30), or let a plague or famine in the chronicle require a death toll above a number (1,000), or print one line per year-window of calamities ("In 5888 to 5893 famine and plague took 7,000 in Gick, Steckstead and Sluggatch Hole").

### B2. A raw kind name in the ruins list

"s43 Guk-Gnik Hole in Reaver's Plain (rg44): ruined 4277, e295 settlement_abandoned_to; last held by The Sneerk-Grik Mob (c4)" (seed 7), and "s11 Elocis ... e1163 settlement_abandoned_to" (`civs4` 3). Every other ruin reads "e<id> sacked, by <civ>". An abandonment to a rival should read "abandoned to <civ>" (or "abandoned, ceded to <civ>"), and a faded ruin needs its own phrase; the sentence of the event itself, which the chronicle has, is the model.

### B3. The adversary list pads with "no war"

Every other realm gets a line: "The Warband of Uuld'thu (c8): no war; now enmity" (phrase, c1 to c11; Miaflie Court has six such lines and nothing else). In `civs4` 3, 52 of the 148 adversary lines say "no war". They add no history. Print only realms with a war, and the count "also in enmity with 4 others".

Related: phrase c1 "settlements: 6 founded, 4 held at the end, 1 ruined" has a ruin that `[present]` does not list (its "ruined 5, faded 1" and the per-realm "ruined" counts add to six): the block counts faded ruins as ruined. Name the faded ones in the block ("1 faded").

### B4. With four civilizations per race the summary is as long as before

`civs4`: 1,270 to 1,358 lines (round one: 1,269 to 1,478), of which about 730 are blocks (26 realms in seed 3). The wars are in again (A1) but a reader still meets 700 lines of blocks. Cut the blocks of `civs4` by dropping the milestones of a realm that ended before the first war ("The Lith Court", 9 events) to one line and by B3.

### B5. Whole phases show nothing in `civs4`

Phases with no event shown: seed 0 16 of 44, seed 7 13 of 46, seed 3 9 of 32, seed 12 2 of 26; none in the default worlds (0 or 1 of 17 to 23). In seed 3 the Goblin age's first 200 years (p4 to p6, 2247-2446) are three headings and "(31 more events not shown)". At T of 76 a 50-year flare-up with 31 events prints no `flare_up` line (salience 50). Show the flare-up line of every phase that has a war; it is the phase's own record.

### B6. Small wording and consistency points

- A war's end year differs between lines: phrase "5753 e466 search_abandoned: ... the war ended after six years" (Hold of Girinnam) against the block's "5747-5752".
- "(The claim, since 5110, was to Whinfurrow Leas.)" sits on a war "over the Silent Weald" with no word on why the goal moved; fine for the data, but the line reads as a typo. "the claim moved" in the note would help (phrase e364).
- Own milestones of a no-war realm are famines and a plague. A founded colony is a better milestone than a death toll of 214.
- `[stats]` at summary: see A9.

## Patterns across seeds

- 9 of 15 summaries show a threshold of 26; seed 0 shows 20, seed 5 31, `civs4` 71 to 76.
- Every one of the 11 default worlds ends with ruins, 5 to 20 (median 11); `civs4` 3 has 32.
- In `civs4` the largest realm holds 21 to 33% of the land at its peak (`share_max_permille`, seeds 0, 3, 7, 12: 215, 333, 304, 291) and only 1 to 6 regions are left free at 6000; seed 5's Slatch Horde holds 17 of 33 held regions. The report shows these well; the cause is in the history (known).

## (b) The history (not re-listed at length)

The repeated succession crises and the identical battle tolls are still visible ("27 of the defeated and eight of the victors fell" twice, seed 7 e562 and e564). Seed 5 reads as one goblin horde's 27 wars; that is the data, and the tally now makes it legible.

## Suggestions

1. Floor the summary threshold at 30, or cap calamity lines (B1).
2. Print the kind of a ruin in words (B2).
3. Drop the "no war" adversary lines (B3, B4).
4. Show every flare-up line (B5).
5. Drop the exact size in `became_city`; cut `[stats]` at summary to the dozen keys a reader wants (A9, A10).

## Worlds to hand Paul first

1. **The phrase world.** Eleven realms, six ages, one realm (Lecier) with a complete arc, a refuge-less but fully legible present with five ruins and two standing claims.
2. **Seed 7.** Open wars and a hidden refuge at 6000, a fallen goblin horde, a ruin made in 5999.
3. **Seed 5.** Shows a runaway as a legible record (27 wars, eight "war between them" notes), nine ruins and a refuge founded in 5977.

Not first: seed 0 until B1 is fixed, and the `civs4` worlds (too long).
