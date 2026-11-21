# World critic: P3-10 wording, site and conflict follow-ups (generator 0.19.0)

World critic, 2026-11-21. Material: `~/.cache/the-unmapped/out/p3-10-validation/critic/` (17 worlds), the 375-world sweep in `sweep/`, and the 0.18.0 sweep and critic folder in `~/.cache/the-unmapped/out/p3-09-validation/`. Read against design doc section 5, change spec `docs/specs/p3-10-followups.md` r2 and the validation report on `origin/p3/10-validation`. Every count below was taken from the sweep databases, the `*.sent.txt` sentence dumps and the reference `*.ref.txt` war lines. My scratch scripts are in the session scratchpad. They are not part of the evidence chain: every figure can be rebuilt from the sweep files.

## Verdict

**Fail, on two conflict rules only.** The wording, names, regions and sites in this change are clear improvements, and I would pass them as they stand. Two of the new conflict rules do something other than what they were for, and both can be fixed with a small rule change:

- **F1.** Items 15 and 17 were meant to make the orcs more warlike. They make the orcs weaker at defaults, and much weaker in `calm` and `turbulent`.
- **F2.** Item 12 was meant to make wars end for reasons a reader can follow. It opens a new case: a war won in one year with no battle, over a region the attacker already owns.

## What works

- **Quiet stretches.** 4,488 identical `quiet_era` lines at 1024 defaults become 873 single lines that read well. Two examples: "For 1,918 years, from 150 to 2067, the land was quiet; one realm held it." (calm seed 0, e4) and "For 763 years, from 3415 to 4177, the land was quiet, and 4 realms kept the peace." (e77). The long elven solitude now reads as deep time, as section 5 asks.
- **Plague sentences** carry their weight. "Plague broke out in Elilien and spread to 4 other settlements; 1,015 died in all." (calm seed 0, e188). The summary shows only the outbreak, and the spreads stay in the full chronicle. I found no salience-45 outbreak with a small toll.
- **Cause clauses** are short and varied, and they never cite an old blow. Across the sweep: "Weakened by plague," (1,777), "Weakened by war," (1,682), "Weakened by emigration," (1,133), "Weakened by famine," (330), "Weakened by tribute," (199), "Weakened by famine and plague," (65). One example: "Weakened by famine and plague, Meithisae was abandoned; 608 survivors went to Eliesigledell." I found no ungrammatical clause.
- **Thousands separators** are consistent, and no "0 of the victors" or "its seat" is left.
- **Region names** are much better. Seed 42 at 1024 has two plain compass names among 50 regions (Northwest Steppe, West Peaks); the rest are names like Tanner's Oath, Queen's Wake, Drover's Stair and the Wood of Stags. The grim words are rarer, and no church words remain.
- **Sites.** Faeries now sit inland in the woods: 11% of faerie settlements lie within 300 m of the sea, against 54% before. Dwarves hold the uplands: 5 of 760 dwarven settlements lie below 30 m, none of them a fortress. Validation's F2 (lowland dwarven colonies in regions with no high ground) is an honest consequence of the region and not worth a rule.
- **Searches of a hidden wood** now read as a single story. "The Duchy of Deas searched the Riven Holt for the Twiwhiwou Ring and found no one; the war ended after 5 years." (calm seed 0, e257). Seed 42's Wood of Stags is fought over 3 times, not 11.
- **2048 and many civilizations.** Seed 42 at 2048 has a strong, grim last age. Clan Ol'thul sacks Ithoomo and takes Hagwood from the House of Finen (e1174–e1176). The Burd League hunts the Kingdom of Burdid's seat through four towns in six years (e1269–e1294). The Duchy of Teck chases the Warrens of Dribnark's capital from Snaksnuz Pit to Bagg Hole to Krik to Zatch (e1357–e1379). This is the dark high fantasy section 5 asks for. Here the orcs are what the design says they are: four orc realms awaken, and two of them win wars against elves and dwarves.

## Findings, by weight

### F1 (blocking). The awakening quarrel breaks the orcs instead of unleashing them

Design section 5 calls the orcs "Strong, warlike" and says "their coming opens the most recent age of conflict". Items 15 (orc odds 500) and 17 (the awakening quarrel) were made to push the orcs toward that. At 0.19.0 they push the other way on nearly every measure, at defaults and more so in the settings where orcs should matter most.

These measures are mine, from the sentence dumps, over all worlds with orcs. A realm counts as acting if it raised a claim, declared a war, founded a colony, or sacked or took a settlement.

| Measure | default 0.18.0 → 0.19.0 | calm | turbulent |
| --- | --- | --- | --- |
| Orcs do nothing at all after 5750 (no claim, declaration, colony, sack or capture) | **0 → 9** of 100 | **0 → 4** of 20 | **0 → 6** of 20 |
| Orcs sacked or took any settlement in their whole history | 41 → 29 | 10 → 5 | 10 → 4 |
| Orcs declared a war in 5800–6000 | 68 → 56 | 12 → 9 | 13 → 6 |
| Orcs founded a colony after 5750 | 98 → 83 | 20 → 14 | 19 → 10 |
| One standing orc settlement at the present (validation) | 7 → 21 | 0 → 8 | 0 → 10 |
| Orc wars won / lost (validation) | 114 / 68 → 95 / 77 | 24 / 9 → 8 / 18 | 22 / 15 → 8 / 20 |

The pattern a reader sees is one short war, then silence. Seed 53 at 1024 defaults shows it:

- 5536, e174: "The Warband of Shurzzaarz laid claim to Balestrand, held by the Realm of Ranaes." This is in the awakening year, with one settlement.
- 5719, e214: the Warband declares war.
- 5722, e219 and e220: "The Realm of Ranaes sacked Mak Camp of the Warband of Shurzzaarz; 1,093 died and 2,551 fled." Then "The survivors of Mak Camp fled into hiding and founded Ugu in the Twisted Shaw."
- 5723, e223: "The Realm of Ranaes prevailed over the Warband of Shurzzaarz after 4 years of war."
- 5723 to 6000: the Warband's only line is "A succession crisis at Ugu shook the Warband of Shurzzaarz." (5998, e308).

At 0.18.0 the same Warband had 18 standing settlements. Calm seed 0 tells the same story (e170, e200, e208–e214, then 286 years with nothing but three succession crises). Turbulent seed 14 is little better: "The Derd League prevailed over the Baldbaal Horde after 1 year of war." (5720, e664), a second loss and a flight in 5795 (e724, e725), then nothing for 205 years.

**Why it happens.** The lie-low rule makes the loss permanent. `hidden_years` is 300 (`game/data/history/conflict.toml`), and the orc age lasts about 450 to 470 years. A refuge founded around 5720 is still hidden at 6000, so the realm can never found another colony. At 0.18.0 the same Krurz Horde (calm seed 0) founded a colony first, declared with two settlements and won. Validation shows the trigger: 29 of the 38 lost first wars were declared with at most one standing settlement. The quarrel fixes the target in the awakening year by distance, often humans or dwarves, and odds of 500 let a one-settlement realm act on it.

**Is it a good story in one world in five?** Sometimes it is. Default seed 0 is a fine example of a horde that loses and keeps coming back. The Krurz Horde declares six wars in 466 years, loses five, and is sacked twice (e449, e462). In 5992 it sacks Alardton: "The Duchy of Cear paid tribute to the Krurz Horde; 689 were carried off." (e486). In 5999 it lays a fresh claim on Harper's Folly (e490). It ends with one settlement but is plainly a live threat. A broken horde hiding a town of 3,500 to 8,000 in the Twisted Shaw or the Stricken Downs is also a good place to find.

But the freeze is not a story in the chronicle. The text gives no reason why a horde of 4,818 (seed 53) sits still for 280 years. In `turbulent`, where orcs should be most dangerous, 6 of 20 worlds have inert orcs for the last 250 years, and in 10 of 20 they end with one settlement. That is against the design's intent, and the change was written to serve that intent. **I would block the merge on it.**

**The smallest fix.** It cuts the chain at the start, and it is the lead's own first suggestion:

- A claim seeded by the awakening quarrel (P3-06 21.12) may not be declared on while its realm has fewer than 2 standing settlements. The claim stays pending, and the strain keeps building.
- No new field is needed. If one is wanted, use `quarrel_min_settlements`, set to 2.

A second lever, if the first is not enough: make `hidden_years` a race field, or cap it at a fraction of the current age's length (for example 100 years for orcs), so that a broken realm recovers within its own age. Measure both on the same 100 default, 20 calm and 20 turbulent worlds. As acceptance I would suggest:

- one-settlement orc worlds at most 10 of 100 at defaults;
- worlds with inert orcs after 5750 at most 2 of 100, and at most 1 of 20 in turbulent;
- T5 met in calm and turbulent as well as at defaults.

### F2 (blocking). A war over a region the attacker already owns is won at once by "aim"

Item 12 says a war is won by aim when the attacker holds the goal. When the attacker already owns the goal region at the declaration, the war is won in its first year with no battle. The loser pays tribute or cedes some other region. These wars repeat:

Seed 23 at 1024 defaults (critic folder):
- 5640, e620: "The Court of Onsae ceded the Ancient Holt to the Urarz Horde." The Kingdom of Durd keeps one town there, Fiecaes, which it took in 5555 (e587).
- 5621, e606: "The Urarz Horde laid claim to the Ancient Holt, held by the Kingdom of Durd."
- 5668–5669, e639–e641: "The Urarz Horde declared war on the Kingdom of Durd over the Ancient Holt." Then "The Urarz Horde prevailed over the Kingdom of Durd after 1 year of war." and "The Kingdom of Durd paid tribute to the Urarz Horde; 3,369 were carried off."
- The same thing happens twice more, at e699 (5780–5781, tribute 2,999) and e748 (5887–5888, tribute 3,105). There is still no battle, and Fiecaes never changes hands.

Seed 7 at 1024 defaults (critic folder), the Sneerk-Grik Mob against the Hold of Tokgru, over Wardgrass:
- e437–e439, 5632–5633: "prevailed … after 1 year of war", tribute 1,713.
- e655–e658, 5980–5981: the same war again, ending "The Hold of Tokgru ceded Southwest Steppe to the Sneerk-Grik Mob." That is a region that was not the war's goal.

**Counts.**
- At 1024 defaults, 22 of the 133 aim wins have 0 battles. All 22 last 1 year, and in all 22 the attacker owned the goal region (`region_control`) at the declaration while the defender held a settlement in it.
- Of the 22, 11 end with a tribute and 11 with a region ceded.
- At 0.18.0 the same sweep had 2 zero-battle wins of at most 1 year.
- Other sets: turbulent 9 of 54 aim wins, civs 4 14 of 66.
- The same pair and region repeat in seeds 7, 23, 73 and 88.

This is about 1% of wars, but it is the kind of absurdity a player would notice: a tribute won, again and again, from a neighbour who keeps one town in your land.

**Fix.** Use whichever is cleaner in the spec:
- (a) a civilization does not claim a region it owns (the claim's goal must be held by another);
- (b) aim needs the goal's holder to change to the attacker during the war, not merely to be the attacker.

Either keeps the battle-less win from occurring at all. If the intent is to let a realm clear a foreign town out of its own land, that should be a war over the settlement, ended by taking it.

### F3. Short quiet stretches, and "quiet" next to wars

- **Short stretches.** At defaults, 141 of 873 quiet stretches are shorter than 50 years, and 82 last exactly 25 years. In 68 of them the next year opens with a war declaration. For example: "For 16 years, from 5684 to 5699, the land was quiet, and 8 realms kept the peace." (default seed 0 e332; calm seed 0 e199), followed by "The Krurz Horde declared war on Clan Drourd" in 5700. Seed 53 has the same thing, "For 14 years, from 5686 to 5699, …" (e212), and the Warband declares in 5719.
- **Claims inside quiet spans.** 1,263 claims at defaults fall inside quiet spans, for example the goblins' claim on the Holt of Yews in 2468 inside "For 1,047 years, from 2218 to 3264, …" (calm seed 0, e25, e38). That is acceptable, since a claim is not a war, but it reads a little against "kept the peace".
- **Suggestion.** Write no `quiet_stretch` line for a quiet row shorter than 50 years (a new `history.engine` field, or a fixed bound in P3-04 20.1). The phase row stays as it is.

### F4. Faerie and other names that still read as jokes

My scan of the 100 default worlds (settlements, civilizations, gods and regions) found these:

- **"pee":** Twipee, Mypee, Lopee, Shaepee, Wypee Glade, Tapeefla, Miepeewhie Hollow. 9 names, all faerie. Twipee is also a **god** name.
- **Other faerie names:** Twypoo, The Circle of Twotitwee, Fassbou Hollow.
- **Human names:** Beamcumstead, Dincum.
- **Triple letters:** Wasssie, Passsy, Floullly Hollow, The Bosssie Ring (the faerie realm of the turbulent seed 14 critic world, in 13 of its sentences), The Circle of Lousssie, The Poullla Court. That is 6 names in 100 worlds.

Other god names are childish rather than crude (Feefi, Codcod, Beebia, Biewie). The goblin `gnob` and `snot` names are in character.

**Suggestions:**
- add `pee`, `poo`, `tit`, `cum` and `fass` to the name filter as substrings, `cum` limited to the human syllable sets;
- reject any name with three equal letters in a row (a rule in P3-02 section 11, not a list entry).

No near-duplicate pairs remain under the core rule. Similar parent and seceded names (the Burd League and the Kingdom of Burdid, 2048 e926) are rare: 2 of 515 secessions share the first four letters with their parent.

### F5. Small wording points

- **Digits and words.** "one" is a word, but 2 to 9 are digits: "spread to one other settlement" (9,012) against "spread to 2 other settlements" (4,113), and "4 realms kept the peace". Secessions say "with 1 settlement" 150 times, for example "The Berd League broke away from the Kingdom of Bocksul with 1 settlement." (seed 53, e195). Suggestion: render 1 to 9 as words in prose counts, and 10 and above as digits.
- **Repeated sieges.** Long wars still repeat "besieged X …; it held." up to four times in a row (seed 53, e267–e274, Rearstead). This is not new, and it is for P3-07's grouping.
- **Plague and famine dominate calm worlds.** In calm seed 0, 56 of the 103 lines from the orc awakening on are plague or famine. That reads grim, which fits the tone, but it is monotonous. It is not new in 0.19.0.
- **Large tributes.** 32 tributes in the sweep carry off more than 5,000 people (largest 14,582), for example "The Warrens of Gutch-Sluz paid tribute to the Duchy of Teck; 10,890 were carried off." (seed 42 at 2048, e899). This is not new, but it is worth a cap in a later balance pass.

## Patterns across seeds

- Orcs inert after 5750: 9 of 100 default worlds, 4 of 20 calm, 6 of 20 turbulent (0, 0 and 0 at 0.18.0).
- Of the 21 default worlds where orcs end with one settlement:
  - in 10 the orcs declared exactly one war;
  - in 5 they were attacked and declared none;
  - the other 6, such as seed 0, are warlike hordes that lost settlements and kept fighting.
- Zero-battle aim wins: 22 of 2,250-odd wars at defaults (0.18.0: 2), in 4 worlds repeated on the same pair and region.
- Quiet stretches under 50 years: 141 of 873 at defaults, 68 of them followed at once by a declaration.
- Crude or silly names: about 20 in 100 worlds, mostly faerie.

## Suggestions, as rule and parameter changes

1. **(F1, before merge)** P3-06 21.12: no declaration on the quarrel claim while the realm has fewer than 2 standing settlements. If needed, also P3-06 21.10: `hidden_years` per race or capped by the age's remaining length (orc about 100).
2. **(F2, before merge)** P3-06 21.6 or 20.2: no claim on a region the claimant owns, or aim only when the goal's holder changes to the attacker during the war.
3. **(F3)** P3-04 20.1: no `quiet_stretch` line for a quiet row shorter than 50 years.
4. **(F4)** P3-02 section 11: filter entries `pee`, `poo`, `tit`, `fass`, and `cum` (human); a rule against three equal letters in a row.
5. **(F5)** The renderer writes counts 1 to 9 as words.

Items 3 to 5 can follow in the next change. Items 1 and 2 are small, and re-measuring them means one sweep of the default, calm and turbulent sets.

## Questions for Paul

1. How often may the orcs end broken and in hiding? With fixes 1 and 2 I would expect about 1 world in 10 at defaults, a horde that lost early and recovered late as in default seed 0. Is that the share you want, or should the orcs never be a spent force at the present?
2. Should faerie names keep their soft, childish sound (Feefi, Biewie), with only the crude matches filtered out, or should the faerie syllables be made stranger and less cute?
