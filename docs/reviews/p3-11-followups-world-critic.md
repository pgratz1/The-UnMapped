# World critic: P3-11 orc arrival, wording and name follow-ups (generator 0.20.0)

World critic, 2026-11-22.

**Material.**
- The critic folder `~/.cache/the-unmapped/out/p3-11-validation/critic/`: 14 default chronicles, calm seeds 0 and 7, turbulent seeds 11 and 14, and the lists.
- The 375-world sweep in `sweep/`. I read the sentence dumps of default seeds 4, 14, 29, 37, 52, 53, 55, 67, 76, 80, 81, 90 and 95, and turbulent seeds 5 and 12.
- The 0.19.0 sweep (`p3-10-validation2/sweep3/`) for pair-for-pair comparison.

**Reading.** Design doc section 5, P3-11 sections 1, 2 and 9, the validation report, my round-2 review of P3-10 (N1 to N3) and Paul's decisions of 2026-11-21.

**Method.** I recounted with my own read-only scripts on the `.db` files. Where my counts overlap validation's (T9 seeds, T10 seeds, kind 39 by holder: 45 / 74 / 10, faerie `ee` 124 and `ou` 182), they agree.

## Verdict

**Pass with notes.** N1 is answered, and N2 and N3 are fixed:
- **N1.** The orc age is now the violent last age the design asks for, at defaults, calm and turbulent, and it does not drown the other peoples.
- **N2.** The town-naming claims and cessions read well and say what happened.
- **N3.** `tard` and the faerie `-ass` are gone.

The baby-talk in the faerie names is reduced, not removed. The remaining cases are near-repeats that the new rule does not see (F3).

**The two missed targets.** I recommend that the lead accept both for this merge.
- The eleven one-settlement worlds are real falls, of two different kinds, and their endings leave places worth finding.
- The three silent worlds are settled realms of four to six towns.

What the misses do show is one real balance weakness for the next pass (F1): the orcs now fight their first war with two settlements, a capital and a colony a few years old, and lose more of those wars than they win.

## What works

**The orcs arrive and fight.** Default worlds, recounted from the rows, 0.19.0 against 0.20.0:

| Measure (100 default worlds) | 0.19.0 | **0.20.0** |
| --- | --- | --- |
| longest orc silence (no colony, claim, declaration, sack or capture), median | 221 years | **128** |
| worlds with a 200-year orc silence anywhere in the age | 62 | **7** |
| first orc declaration within 100 years of the awakening | (median 244) | **58 worlds** |
| sacks by orcs after 5600 | 100 | **181** (goblins 168, humans 130) |
| wars after 5600 with an orc side | 238 of 988 | **373 of 1,070** |
| declarations after 5750, median per world | 7 | **7** |
| worlds where orcs win no war | 51 (round-2 count) | **38** |
| worlds where orcs are the largest race by people | 0 | **4** (seeds 38, 40, 52, 53) |

The orcs now sack more than any other people in the last age. The other peoples' wars barely fall (goblin declarations after 5600: 525 to 486; human 151 to 132), and the median number of declarations in the last 250 years is unchanged. So the orc wars are added to the last age, not substituted for it. The orc share is 88‰ of people and 95‰ of standing settlements. The median world share is 53‰, and above 340‰ only in seeds 38, 40, 52 and 53. That is strong, not dominant.

**The orc age has rises as well as falls.**

- **Default seed 53.** The Warband of Shurzzaarz is beaten twice by the Realm of Ranaes (5622, 5730). On its third attempt it wins: "The Warband of Shurzzaarz sacked Elithiethdell of the Realm of Ranaes; 2,547 died and 5,946 fled." (5833, e290). It goes on to conquer a human realm: "The Berd League fell to the Warband of Shurzzaarz." (5869, e314). Then "The Warband of Shurzzaarz sacked Breardbasham of the Kingdom of Ashodward; 4,600 died and 10,735 fled." (5945, e352). In its last year Bocksul cedes the Downs of Echoes (5999, e387). This is the design's "most recent age of conflict" in one realm.
- **Default seed 52.** At 0.19.0 Clan Mok was stripped to one fort (1,051 orcs at the present). Now it wins nine of its thirteen wars and spawns a daughter horde: "Clan Zork broke away from Clan Mok with eight settlements." (5907, e349). It ends at 73,243 of the world's 127,693 people. The same seed tells the opposite story, and both read well.
- **Calm seed 0.** The Krurz Horde conquers the Duchy of Cear in 19 years: "The Duchy of Cear fell to the Krurz Horde." (5683, e233). It then wrings tribute from Clan Drourd three times (e267, e296, e336).
- **Turbulent seed 5.** The Om'karz Horde never founds a colony. In its last 50 years it turns on its own camp and swallows a goblin realm: "The Slatch Horde fell to the Om'karz Horde." (5987, e2560).
- **Turbulent seed 14.** The Baldbaal Horde declares a new war within one to five years of every peace from 5766 to 5904: on Clan Kurin, Rathenn, the Whiellmae Ring, the Derd League and the Stildrurd League. It reads like a horde that cannot stop.

**N2, claims and footholds.** The best story of my last review can now be read:
- The town is founded: "The Zatzeegg Mob founded Greerk-Zark Pit in the Graven Thicket." (2134, e15).
- It is lost: "The Bizz-Skuz Horde took Greerk-Zark Pit from the Zatzeegg Mob; 246 died." (3731, e102).
- The Mob claims it back: "The Zatzeegg Mob laid claim to Greerk-Zark Pit of the Bizz-Skuz Horde in the Graven Thicket." (4193, e168).
- After the war, the town returns: "The Bizz-Skuz Horde gave up Greerk-Zark Pit in the Graven Thicket to the Zatzeegg Mob." (4254, e198).

Three more examples:
- **Default seed 7.** The orcs' claim on their own lost camp now says what the orcs want: "Clan Kuushbam laid claim to Ghaldgruz Camp of the Kingdom of Geadalack in Snowscar." (5779, e577).
- **Default seed 67.** One year has two claims on one region against its two holders, which reads cleanly. The first is "The Duchy of Educk laid claim to Darnelgill Woods, held by the Nagnog Mob." (5524, e690); the second is "The Duchy of Educk laid claim to Gnuggsneex Pit of the Krunk Horde in Darnelgill Woods." (e691).
- **The list form** reads well with three names: "The Grank-Skeeg Mob laid claim to Gnuggsneex Pit, Gragruk Pit and Skreex of the Krunk Horde in Darnelgill Woods." (5797, e898).

**Mislead check.** I read all 129 kind 39 sentences against `region_control`: 45 are on a region the claimant holds, 74 on one a third realm holds, and 10 on one nobody holds. None misleads. Each says only that the named realm owns the named towns in that region, which is true, and none implies who holds the land. The third-realm cases are the old "held by" problem turned the right way round. One example is "The Grotch Mob laid claim to Alglaldell of the Drolidrai Throne in Bellringer's Forest." (default seed 14, 5479, e407). Bellringer's Forest is held by the House of Notholl, from whom the Drolidrai Throne had taken the town; the chain is visible from e295 to e299. The remaining weakness is in the war line that follows (F5).

**N3 and the filter.**
- No name in any culture of the 375 worlds contains `tard`.
- No faerie name ends in `ass`.
- No faerie name holds `p` or `b`.

**Small numbers as words.** No sentence I read reads wrongly. "For 1,918 years, from 150 to 2067, the land was quiet; one realm held it." (default seed 0, e4), "spread to one other settlement", "after one year of war", "nine died" and "with eight settlements" are all better than the digits were.

## Findings, by weight

### F1. The orcs now fight their first war at two settlements, and lose it more often than they win (balance; not blocking)

The pending colony makes the orcs' second settlement come at once (median 9 years). P3-06 22.1's hold-back, "two standing settlements before the quarrel war", is therefore met a few years after the awakening. In 63 of the 83 worlds with an orc declaration, the first one is made with exactly two settlements: a camp of about 1,200 and a colony of about 144.

| First orc war, 100 default worlds | won | lost | truce | unfound |
| --- | --- | --- | --- | --- |
| declared with 2 standing settlements (63) | 19 | **29** | 10 | 5 |
| declared with 4 or more (17; one still open) | 7 | 3 | 3 | 3 |
| declared within 100 years of the awakening (58) | 18 | **26** | 8 | 6 |
| declared 200 or more years after (17; one still open) | 8 | 3 | 2 | 3 |

This is the mechanism behind most of T9.
- **Default seed 0, the plainest case.** The Krurz Horde declares at +82 on Clan Drourd and loses both camps in one year: "Clan Drourd sacked Daz Camp of the Krurz Horde; 939 died and 2,194 fled." (5622, e327). It then does nothing but colonise for 120 years and has succession crises (e356, e374, e401) to the present. At 0.19.0 the same seed's Horde conquered the Duchy of Cear. Seed 0 is the first world anyone opens. Its last 250 years now hold one declaration (5839, e386, elves on goblins); 0.19.0 had none so quiet. Calm seed 0 keeps the conquest.
- **Default seed 5.** The Shuugh Horde wins its first war in one year, against the Duchy of Ashear (5600, tribute 2,853, e578). It then goes back three times to the same duchy and loses each time (e658, e750, e818). It loses Dak, Aa'shaarz and Bushkoz Fort, and finally its homeland: "The Shuugh Horde ceded the Parched Sweep to the Duchy of Ashear." (5888, e771).

The pattern is in character, a horde that keeps attacking the neighbour that beat it. But it repeats across seeds 5, 7, 11 and 37.

**Suggestion (later balance pass, not this merge).** Tie the quarrel hold-back to strength, not to a count of settlements that the pending colony now satisfies at once. Two ways:
- the quarrel war waits until the realm's population is at least its awakening population plus one colony's growth;
- or orc `odds_min_permille` rises for a realm younger than 50 years.

Validation's lever `quarrel_min_settlements` 3 would also work. It would bring back part of the early silence (the third settlement waits for pressure, about a century), so I would try it last.

### F2. T9 (11 worlds) and T10 (3 worlds): my reading, for the lead's ruling

**T9, one orc settlement at the present.** I read all eleven. They are two different stories, and neither is "the same people throwing itself away the same way every time".

- **Falls by their own wars (seeds 5, 7, 11, 37, 65, 80).** The orc realm attacks again and again and loses:
  - **Seed 11.** Five wars, none won. Two are fruitless searches for the Fiall Court. In the end "The Realm of Liesas sacked Ak'ugork of the Warband of Kraagazarz; 699 died and 1,631 fled." (5865, e492).
  - **Seed 37.** Clan Mulshugh is the most turbulent of the eleven: 11 declarations in 381 years. It wins Whindown (5754, e644), loses it (5857, e701), wins Wanderer's End (5956, e780) and loses it again in its last war (5990, e796).
  - **Seed 80.** Thirteen declarations by one realm, five of them searches for faeries who are never found (F6).
  - **Seed 7.** Clan Kuushbam loses Ghaldgruz Camp (5707, e506). It then loses a war to win it back (e607 to e617), flees into hiding, "The survivors of Muush Fort fled into hiding and founded Orz'thugh Camp in Dwalegrove." (5891, e615), and declares one more war from the refuge in 5996 (e666). A good ending: a hidden horde of 6,503 with an open war.
- **Falls by another's conquest (seeds 76, 81, 88, 95, and 55 in part).** The orcs are attacked and stripped, often in a few years at the very end:
  - **Seed 81.** "The Brorinmai Throne sacked Azag Fort of Clan Kraal; 316 died and 739 fled." (5928, e278), then Ark'bal Fort (e282) and Darz'ghoz (e289) within six years.
  - **Seed 76.** The Reanaluld League takes four orc settlements in eight years (5926 to 5933, e686 to e702), and the seat moves four times.
  - **Seed 95.** The Edud League does the same, and in the same year the Horde wins its own war in the south: "The Thaaguragh Horde prevailed over the House of Caeitha after 14 years of war." (5918, e380). That is a fine two-front ending.

**Are these endings flat?** No.
- Each leaves the ruins of two to five orc settlements, a remnant town with a grudge, and, in seeds 7 and 88, a refuge.
- Four of the eleven (55, 65, 81, 95) were one-settlement worlds at 0.19.0 too.
- Seeds 5, 7, 11, 76 and 80 had four to seventeen orc settlements then. Seed 52 went the other way, from one to thirty-four.

The orc age is now a gamble that goes both ways, which I think the design wants. Eleven in a hundred is a fair share of broken hordes against four that conquer most of their world. **I recommend accepting T9 at 11.** If a bound is kept, count one-settlement worlds where the orcs also *won no war and sacked nothing*: seeds 7, 11, 65, 76, 81 and 88, that is 6.

**Seed 88** is not a horde that throws itself away. The Warband of Azok never founds a colony and never declares; I checked the war lines.
- It is attacked twice, by the Brali Throne (5854, e978) and by the goblins of the Warrens of Drak-Slark (5917, e1016).
- The second attack sacks its fort: "The Warrens of Drak-Slark sacked Azuuz Fort of the Warband of Azok; 863 died and 2,014 fled." (5920, e1019).
- Its own acts are four claims, two of them kind 39 (e840, e973).

As a reading it is a horde boxed in by older peoples, which is acceptable once in a hundred worlds. *For the lead:* validation's section 4.1 says the realm "declares two late wars and loses both". It declares none: both wars were declared on it.

**T10, orcs silent after 5750.**
- **Seed 29.** Clan Uush'gha holds three regions and six settlements (10,885 people). Its neighbours are a dwarf hold, a goblin mob and three unheld regions. It never claims anything, and its only line after 5724 is a succession crisis (5880, e1145).
- **Seed 90.** Clan Uraa'dokh holds four settlements in Gloambarrow Wood and one unpursued claim on Clan Nammti's Wraithslack Heights (5572, e582). Then come four succession crises (e644, e699, e728, e763).
- **Seed 0.** The beaten Horde of F1, which is not a horde at peace by choice.

Seeds 29 and 90 read as acceptable peace: an orc realm left alone in the wilds, settled and quiet. A player would find an orc clan with old forts and no recent war, which is a tension waiting rather than a broken history. They are also a by-product of the cure: at 0.19.0 no world could be silent at the end, because the orcs only began acting near 5750. **I recommend accepting T10 at 3.** The rule could be reworded as "no orc realm with at most two standing settlements is silent after 5750", which these three meet (four, six and four settlements). Seed 0 is better answered by F1 than by T10.

### F3. Faerie names: the baby-talk is reduced, not removed (Paul's taste; next name change)

What the change achieved:
- In the 100 default worlds, `ee` falls from 200 names to 124 and `ou` from 273 to 182, and `p` and `b` are gone.
- Across all 375 worlds, names with two or more `ee`/`ou` fall from 156 to 66, and exact syllable repeats from 23 to 4 by my looser text check. The 4 that remain are Glalaesh, Lalae, Matwawa Glade and Momoufo Hollow; the rule passes them because the drawn split differs from the spelling.

**The best of the 300.** These keep the fey character and read as names, not sounds:
- realms: The Circle of Flian (s6), The Whiellmae Ring (s14), The Laessvia Court (s83), The Ievytae Ring (s37), The Fiall Court (s11), The Lyll Court (s55), The Taellsa Court (s38), The Sien Ring (s76), The Lilae Ring (s80);
- gods: Syllmia (s1), Saemieth (s46), Aewhyn (s19), Fithia (s59), Liemae (s66), Wynlie (s85);
- settlements: Laellvia (s37), Vaen Glade (s68), Vaefith (s63), Glielia Glade (s11), Flaethlo (s74), Liwae Hollow (s36).

The new `v` onset gives some of the best of these.

**The worst of the 300.** They still read as baby talk or as a stutter:
- gods: Feefi (s51), Ouwheefy (s7), Veewo (s24), Eemy (s31), Wouwyfee (s93), Lymymia (s68), Shoulouwhy (s79), Gliglia (s20), Vaefliflou (s12), Fywouta (s30);
- realms: The Shosheesha Court (s98), The Twiwhiwae Ring (s0), The Flaefla Ring (s87), The Weesky Ring (s48), The Iasoutwou Ring (s30), The Feeshee Ring (s34);
- a settlement: Twy (s87).

**What is left is mostly the near-repeat.** The same onset comes twice in a row with a different nucleus: Flaefla, Shosheesha, Gliglia, Feefi, Vaefliflou. P3-02 14.2 allows it by design (`feefi` is listed as valid). In all 375 worlds, 160 of 2,437 faerie names (6.6%) have such a pair, against 167 at 0.19.0, so the change did not touch them. Examples beyond the 300: Glyglafly Glade, Fliefliai, Meemiemae Glade, Glyshyshae.

The other remaining source is chains of `w`/`wh`/`tw` onsets: The Twiwhiwae Ring, Whiwaewhae Glade, Twawytwo Glade. Names with two or more `w` are 358 at both versions.

**New problems from `v` and `gl`.**
- **Shiva.** The seat of the Circle of Glae in default seed 77 is the name of a living religion's god: "The Circle of Glae founded Shiva in Wrenwood." (4196, e145). The famous list has `siva` but not `shiva`, and the new `v` onset reaches it. **Add `shiva` to `famous.whole`.** This is the only finding here I would fix before P3-07 shows names to players. It is a data edit that changes few worlds.
- **English words.** "The Glee Ring" (default seed 4, also calm 4 and 512 seed 4) reads as a glee club. "The Circle of Faith" (default seed 23) reads as a chapel. Both stand out among invented names. Glee is new through `gl`; Faith is not new.
- **An Italian sound.** `gl` before `i` gives an Italian `gli`: Louvoglia (s29), Sagliwhi (s5), Iaglia (s82). It is pleasant but a different flavour. Not a defect.
- **A slur fragment.** "Shyfaglae" (default seed 87) holds `fag` inside a word. The filter has `fag` only `at_start` and `fagg` anywhere. It reads as "Shy-fag-lae". Paul may want `fag` anywhere for the faerie culture only, since `fagus`-like words do not arise there.
- **Fatwash** (default seed 64) reads as "fat wash". It is harmless and comic, so I would leave it.

**Suggestion (data, for Paul's next look at names).** Either:
- extend P3-02 14.2's test to "the same onset twice in a row when both syllables are open (no coda)", which would catch Feefi, Flaefla, Gliglia and Shosheesha but not Lilae or Laellvia, the names Paul liked;
- or halve the weight of `w`, `wh` and `tw` together, which the data can do.

### F4. The repeated-syllable rule in other cultures, and `tard`

- **Clean.** No name in any culture holds `tard` or a crude fragment of the lists. The rule plainly removes elven and goblin doubles: names that read as a repeat go from 60 to 36 elven and from 34 to 9 goblin.
- **Dwarven, unchanged: 20 names against 19.** The codas `rin` and `din` spell onset-plus-nucleus inside a single syllable, so the rule cannot see them. Examples: Clan Didin and Didin Deep (default seed 12), Bririn (calm 0), Gririnhard and Dririn Deep. These read as a stutter, Didin most of all. The fix is data: drop the coda `din`, or forbid `didin` and `ririn` in the dwarven `forbidden` list.
- **Orcish, apostrophe echoes.** Names such as Barz'azarz Fort, Ugug'ugom Fort and Uru'uruurk Camp pass because the split falls differently from the spelling. In orcish they read as a war chant, which fits, and I would leave them.
- **Not new, for the filter's lists.** Elven "Fannie" (`civs4` seed 1; British slang) and dwarven "Hindi" (`civs4` seed 17) reached the filter at 0.19.0 too. I list them for the next filter edit, not as a finding.

### F5. The war line after a town claim still names a region (P3-07)

When a kind 39 claim leads to war, the declaration keeps "over {region}".
- Where a third realm holds the region, it reads as a war over someone else's land: "The Grotch Mob declared war on the Drolidrai Throne over Bellringer's Forest." (default seed 14, 5902, e674). Bellringer's Forest is held by the House of Notholl.
- Where the claimant holds it, it reads as a war over the attacker's own home: "Clan Kuushbam declared war on the Kingdom of Geadalack over Snowscar." (default seed 7, 5879, e607).

At defaults, 15 declarations follow a kind 39 claim on the claimant's own region, and 33 follow one on a third realm's region. The claim sentence just before makes either readable in a full log. In a summary that drops the claim, it will not.

**Suggestion (P3-07).** When the cited claim is kind 39, the report writes the war as "over Ghaldgruz Camp in Snowscar", or adds a note naming the towns, as it now does for a goal that differs from the claim.

### F6. Small matters

- **Orc searches for hidden faeries** are more frequent: 43 fruitless searches in 27 worlds, against 25 in 18. Default seed 80 has five by one realm. Three are against the Circle of Twievyglee (e310, e358, e438) and read alike: "The Warband of Krak searched the Woods of Antlers for the Circle of Twievyglee and found no one; the war ended after four years." (5986, e438). It is grim comedy, but it starts to look absurd. **Suggestion (balance pass):** a realm that has failed to find the same hidden realm twice does not claim against it again for a century.
- **Mixed numerals in one clause.** 230 of 9,065 battle sentences at defaults have a two-digit and a one-word toll side by side: "24 of the defeated and seven of the victors fell" (60 times in the sweep). It is standard style and readable, but slightly uneven. **Suggestion (P3-07 or later):** when either side of a toll is 10 or more, write both as digits.
- **Large tributes grew with the orcs.** 8 above 5,000 at defaults (0.19.0: 3); the largest is still 9,492. This remains for a balance pass.
- **Cascades of seat moves.** Default seed 76 has "The seat of the Warband of Oz'ghuz moved to …" four times in 5926 to 5933 (e687, e692, e696, e704). It is vivid in a full log, and P3-07 should group it like the repeated sieges.
- **Orcs now fight their own camp.** There are 12 orc declarations on goblins in 9 default worlds (0.19.0: none). Examples:
  - seed 4: "The Ghal Horde declared war on the Zuslit Horde over East Fells." (5926, e713), which it wins, taking Dreezork and Krogg Pit;
  - turbulent seed 5: the conquest of the Slatch Horde.

  They come late (claims 263 to 472 years after the awakening), from P3-06's `want` term, not from the camp table. They read well, as evil turning on evil when the land runs out. But Paul's decision of 2026-11-21 records that "orcs do not fight their own camp" (question 1).
- **Equal populations.** Sister villages often end at exactly the same population, for example "Burk Camp (588), Aakh'zuk Camp (588), Kuulugogh (588)" in default seed 0, and three at 234 in seed 4. This is not new; it is the shared regional share. It is harmless now, but a viewer will show it.

### Earlier notes

- **Closed:** N1 (orc arrival), N2 (claims and footholds), N3 (`tard`, faerie `-ass`), the "with 1 settlement" digits, and the war-goal note (P3-07 now says it).
- **Open:**
  - identical battle tolls (P5);
  - repeated "besieged …; it held" and seat-move runs (P3-07 grouping);
  - large tributes (balance pass);
  - the gods and monsters, which do not yet act in P3 (P5).

## Patterns across seeds (1024 defaults unless stated)

- **First orc act within 100 years:** 99 of 100 worlds (seed 88 the exception). A first declaration within 100 years: 58. No declaration ever: 17.
- **First orc war:** made with two standing settlements in 63 of 83 worlds; of those, 29 lost and 19 won.
- **One standing orc settlement at the present:** 11. Six of them are falls by the orcs' own wars, and four or five by another's conquest. Calm 0, turbulent 1.
- **Orcs largest race by people:** 4 worlds (0.19.0: 0). Orc share median 53‰, highest 573‰ (seed 52).
- **Orc share of last-age wars:** 373 of 1,070 after 5600, up from 238 of 988. The median number of declarations in 5750 to 6000 is unchanged at 7.
- **Kind 39 claims:** 129 (claimant 45, third 74, none 10), all true as written. Kind 40: 2 (seeds 40, 67), both true.
- **Faerie names:** 6.6% of 2,437 in all worlds still have a same-onset near-repeat. Exact repeats: 4. One real god's name (Shiva, seed 77).
- **Dwarven `rin`/`din` stutters:** 20 names in all worlds, unchanged.

## Questions for Paul

1. **Orcs at war with goblins.** In 9 of 100 default worlds a crowded orc realm now attacks a goblin one late in its age and sometimes conquers it (turbulent seed 5: "The Slatch Horde fell to the Om'karz Horde."). Is evil turning on evil welcome, or should the orcs never attack their own camp, as the record of your 2026-11-21 decision reads?
2. **Faerie near-repeats.** Feefi, Flaefla, Gliglia and The Shosheesha Court pass the new rule because their nuclei differ. Do you want these gone too, which needs a stricter rule or fewer `w`/`wh`/`tw`? Or is this level of fey sing-song the character you meant to keep?
3. **The orc age as a gamble.** The same seed can now end with the orcs holding half the world (seed 52) or one town (seed 80, 17 settlements at 0.19.0). Is that spread what you want, or should the early first war be made less of a coin toss (F1)?

## Suggestions

| # | Change | When |
| --- | --- | --- |
| 1 | Add `shiva` to `famous.whole`. Consider `fag` anywhere for faerie names (Shyfaglae) | next data change, before P3-07 shows names |
| 2 | Accept T9 at 11 and T10 at 3 for this merge. Restate T9 as "one standing settlement, and no war won and nothing sacked" (6 now), and T10 as "silent after 5750 with at most two standing settlements" (0 now) | lead's ruling, now |
| 3 | Gate the quarrel war on strength, not on the settlement count the pending colony meets at once: a minimum population, or higher orc odds for a realm younger than 50 years | later balance pass |
| 4 | A realm that has twice failed to find the same hidden realm does not claim against it for a century | later balance pass |
| 5 | War lines that cite a kind 39 claim name the towns ("over Ghaldgruz Camp in Snowscar"). Group runs of seat moves | P3-07 |
| 6 | When either side of a battle toll is 10 or more, write both as digits | P3-07 or later |
| 7 | Faerie: catch same-onset open syllables in a row, or halve `w`/`wh`/`tw`. Dwarven: drop the coda `din` or forbid `didin`/`ririn` | next name change, with Paul |
| 8 | Identical battle tolls; caps on large tributes | P5; balance pass |
