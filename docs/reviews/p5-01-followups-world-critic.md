# World critic review: P5-01 carried-over follow-ups (generator 0.21.0 against 0.20.0)

Date: 2026-11-22. Reviewer: world-critic (Sonnet 5.5).

## Verdict

**Pass with notes.** Every war reads as motivated, the summary is shorter and no longer drowns in plagues, and the new events read well. Nothing found blocks the merge. Two small things are worth fixing before it (a name stutter and a `[present]` label); the rest can wait.

## Material

- 0.21.0: `~/.cache/the-unmapped/out/p5-01-impl/sweep/` (`default` seeds 0-99, `civs4` 20 worlds), summary and full text.
- 0.20.0: `~/.cache/the-unmapped/out/p3-07-validation/`.
- Read closely at summary level: default seeds 0, 5, 42, 72, 84, 58, 31, and `civs4` seeds 1, 3, 7 (partly), plus the per-world blocks and `[present]` of about ten more. Full level: seeds 72, 84 and 58 (event by event around claims, searches and refuges). Side by side with 0.20.0: seeds 0, 5, 42, 72 (line counts, calamity share) and the whole sweeps by script. The counts below come from scripts over all 100 default and 20 `civs4` worlds.

## What works

- **Seed 0 is fixed.** 0.20.0 printed 104 of 221 chronicle lines as plague, famine or succession (47%). Now 164 shown lines, about 9% calamities. Across the 100 default worlds the calamity share of the summary chronicle is median 7.8%, max 22.9% (seed 72), against 15.2% and 53.8% in 0.20.0. No default world is above 25%.
- **Summary length.** Default median 573 lines (0.20.0: 724), max 755 (913). `civs4` median 1,047 (1,366), max 1,353 (1,579).
- **Every war carries a claim.** In the 100 default worlds, 2,601 declarations. 2,524 read "(Claimed since N.)"; the other 77 read "(The claim, since 5586, was to Thomar in Sourholt.)" (seed 17 e316, seed 14 e337). None is without a stated grievance. Seed 0 reads as a chain: "e51 Baskeeb Mob declared war on the House of Nife over the Holt of Yews. (Claimed since 2180.)", then "over Cairnwater Grove. (Claimed since 3265. The second war between them.)", then "over the Broken Forest (Claimed since 3665...)", up to "The sixth war between them" at 5781 with "(Claimed since 5776.)". Fresh claims dated a few years before the war ("Claimed since 5776", war 5781) are plausible: a grievance turns into a war.
- **Orcs still start wars, and the first wars read well.** First orc declaration after awakening, 100 default worlds: median 166 years (0.20.0 by the same script: 59), range 7 to 448. 81 of 100 worlds have one (0.20.0: 83 of 100). Seven fall within 40 years, all on a claim made at the awakening, for example seed 34: "5549 Clan Uk'ghaam declared war on the Kingdom of Sordis over the Primeval Grove. (Claimed since 5542.)" Seed 0: "5766 The Krurz Horde declared war on Clan Drourd over South Heath. (Claimed since 5534.)", 232 years after the awakening, with Drourd winning two battles first (e361, e367); this reads as a horde that grew before it struck. `civs4`: median 101 years, 18 of 20 worlds.
- **Battle tolls vary.** Of 8,765 battles in the sweep, winners lost fewer than losers in all but one, and the victor-to-loser ratio spreads (seed 0: 129/560, 125/454, 206/821, 85/527, 138/510). The most repeated pair is "13 of the defeated and 3 of the victors" (7 times in 8,765), and the small ones in words ("five of the defeated fell, and none of the victors", seed 31 e393; 6 times). No clause mixes digits and words.
- **Flare-ups.** Shortest in the sweep is 10 years. 381 of 383 flare-up phases show their `flare_up` line.
- **The ruins list** now says "sacked by" or "abandoned" and no kind name leaks (B2). In the whole sweep: 1,082 "sacked by", 46 "abandoned".
- **New events.** "The Gub Horde let the claim against the Sawy Ring over South Wood lapse." (claim_dropped, 161 in the default sweep) and "Nurngouk lay hidden from 3811 to 4110." (refuge_hidden, 51) read cleanly and are causally attached (refuge e279 follows "e278 The survivors of Tondho Deep fled into hiding and founded Nurngouk in the Tangled Hurst", seed 58, and the old city is later "The ruins of Tondho Deep faded from memory", 5323). Seed 58's refuge story, from sack (3811) to the capital moving there, reads as a real piece of history.
- **Region names.** The mix is sound: about half plain (Rustwood, Central Moor, Hartwood), a third gentle-poetic (Leas of the Harvest, Sward of Larks), a tail of grim (Plain of Ashes, Wilds of Bones, Rises of Wyrms, Wood of Ghosts, Weald of Wyrms, Plain of Chains). Ghosts and Ashes and Wyrms recur (the Wood of Ghosts 12 times over the 100 worlds), which is fine at that spread. The grim share feels like "a few, not a theme".
- **`became_city`.** "Lafin in the Broken Forest grew into a city." (seed 0 e182) reads better than the old wording.

## Findings

### Fix before merge (small)

**F1. Name stutters are still in the human settlement names.** The filter catches `didin` and `ririn` but not a doubled place-word suffix. In the default sweep: `Sudsteadstead` 20 mentions (seed 20, "The Duchy of Senbel founded Sudsteadstead in the Shuddering Rises", e289, 4937), `Galwickwick` 14 (seed 82, e774), `Stonton` 11 (seed 41), `Canashamford` 7, `Beasstontstead` 3, `Gontashamwick` 3. These are the human builder gluing `-stead`, `-wick`, `-ton` on a stem that already ends in one. A second flaw in the same family: `-asstead` appears in seven names, 31 mentions (`Deasstead` 9, `Edasstead` 7, `Brurtasstead` 6, `Teasstead`, `Weasstead`, `Alasstead`). It reads "ass-stead" more than any `fag` ever did, and is the sort of thing that T15 was meant to remove.
Rule: reject a name whose last two syllables are the same suffix, and reject `ss` before a suffix starting with `s`.

**F2. `tard` still passes the filter, in a region name.** Seed 84: "The Weeli Court sent settlers from Lilaewhae to found Sollfli Glade in Tarden." (e332, 5156) and `rg38 Tarden` in the full report; 3 mentions. The spec's T15 says "`tard`: none". The region-name path (spec P1-06 compound names, `placename`) does not appear to go through the name filter, or the check is on the whole word with a boundary. A single check pass applied to every generated string, region names included, would answer it.

**F3. `[present]` "refuges" lists refuges found long ago.** Seed 58: "refuges: 2 / s63 Nurngouk ...: The Drourktou Throne (c5), population 5410, founded 3811 by flight from Tondho Deep (s61), e278; found 4111". It is a thriving 5,410-person capital with colonies, found 1,900 years before the end; it is not a hidden refuge in any sense a player would care about. Of 40 refuges listed in the 100 default worlds, 10 are marked "; hidden" and 30 are found. T19 asks that the "hidden" marks equal the reference, which they do; the problem is the heading. Either list only the hidden ones, or call the section "refuges founded by flight" and sort the hidden ones first. The same line in seed 31, "Vaean lay hidden from 5997 to 5999" (e397), is a refuge hidden for the last two years only because history ended; fine as data, but it belongs in `[present]` and nowhere else.

### For a later stage

**G1. Old claims never close.** Standing claims at E in the default worlds: 612, median age 102 years, 113 older than 500, the oldest 3,773 years ("e27 2263 The Warrens of Kugg (c3) on The House of Iernell (c1): Wayfarer's Tree (rg10)", seed 47; seed 18 e27 2227). A claim held 3,700 years that never led to a war and never lapsed is a grudge the world forgot. `claim_dropped` fires 161 times (1.6 per world) and does not reach these. In `civs4` seed 1 the `[present]` list has 47 standing claims (about 50 lines), the length of five realm blocks and not a read. Suggestions: cap the list (the 10 oldest, plus "and 37 more"), and let a claim lapse after `history.conflict.claim_max_years` (try 1,500) unless the claimant is at war with the holder.

**G2. Searches that "found no one" in a realm that is still there.** Seed 84: "5717 The Aak'ugarz Horde declared war on the Weeli Court over Wyrmgrove. ... 5721 e441 search_abandoned: The Aak'ugarz Horde searched Wyrmgrove for the Weeli Court and found no one; the war ended after four years." The Weeli Court holds 5 regions in 6000 and has no refuge. Seed 72, e74 and e101: the Folaelo Ring is "found no one" twice in the Brake of the Hunt, with no `refuge_hidden` after either, and holds two regions at the end. 332 `search_abandoned` in the default sweep against 51 `refuge_hidden`: most hidings leave no refuge to see. The sentence is fair for a hiding people; it needs one line saying where they went, or else a faerie reader sees a ghost war.

**G3. The search-abandon war length differs by a year or two between the clause and the block** (B6 first bullet): not re-checked; the three samples agree (seed 72 e100 5166, e101 5174, "eight years").

**G4. Lines per world still above the T18 aims.** Default worlds above 700 lines: 5 (seeds 12, 33, 59, 66, 98; max 754). `civs4` worlds above 1,100: 7 of 20, max 1,353 (seed 1: 845 lines of blocks and 149 of `[present]`, 62 realm lines). The blocks got shorter (ended realms are 6 to 12 lines, "without war: 8 (enmity 8, none 0)" is one line) but `civs4` is still a long read. Not a merge block; a cap on `civs4` worlds' standing claims (G1) is the cheapest cut.

**G5. A realm with 0 regions and 4 settlements.** `civs4` seed 1, "c16 The Koz-Krurk Horde: 0 regions, 4 settlements, population 6194". Held settlements with no held region. If this is the intended result of conquest of its land, the line wants a word ("settlements in lands held by others").

**G6. Runaway shape in seed 5.** The Slatch Horde (goblins) is still the actor of most wars after 2825 (e84, e100, e138, e170, e210, e223, e235, e242: eight declarations against five different peoples in 1,100 years, "The third war between them" twice). It reads as a campaign, not an error; no change asked, but the same horde appears in every phase list I read for that seed. Its claims are dated alike: e84 and e100 both "Claimed since 2162" on two regions (West Wood and Stoneden). A fresh claim keeps the old date (spec 9.1, so the grievance is not "claimed the same year"), which here makes two claims on different regions of the same age. Plausible, slightly odd on the page.

**G7. `Whomoutwia`, `Kaaghdam`, `Tirkbumm`, `Thassie` read badly** but not offensively. One weak faerie creator name per few worlds is the cost of a syllable builder, and I do not ask for a filter.

## B1 to B6 of the last review

| Note | Answered? | Evidence |
| --- | --- | --- |
| B1 seed 0 flooded with plague | **Yes** | Seed 0: 47% to about 9%; sweep max 22.9% (T17 met). |
| B2 raw kind in ruins | **Yes** | "sacked by" or "abandoned"; no `settlement_abandoned_to` in any ruins list. |
| B3 "no war" adversary padding | **Yes** | one line "without war: 2 (enmity 2, none 0)". |
| B4 `civs4` too long | **Partly** | median 1,366 to 1,047; still 7 of 20 above 1,100 (G4). Ended realms are short blocks. |
| B5 empty phases / missing flare-up lines | **Mostly** | 381 of 383 flare-up phases show the line (2 miss). 25-year quiet phases with "0 events" still print (e.g. seed 72 p8 and p12), harmless. |
| B6 wording | **Partly** | War lengths agree in the samples; "The claim, since 5586, was to Thomar in Sourholt" is still the odd sentence (77 times), with no word for why the goal moved. Own milestones for peaceful realms are still famines and plagues in seed 0 c3 (Clan Durgarin: "famine 214 died", "plague 215 died") though a colony is listed first now. |

## Report v2

- The summary reads as a history: ages, chronicle by phase with "(N more events not shown)", realm blocks with a tally, `[present]`, `[stats]`. Ids and refs stay, as Paul wants.
- The `[present]` section is the best addition: "standing claims", "open wars", "refuges" are the first thing a player or designer asks. The two lists need G1 and F3.
- `[stats]` (`wars_hollow_permille`, `share_max_permille`) is useful. Hollow wars over the default sweep: mean 210‰, median 214, max 500 (T1 bound 250 met). Seed 42 is at 291 (28 wars): a world where nearly a third of the wars are "found no one" or ended without a battle, worth one look by whoever tunes wars.
- Seed 42's present state is a good one to open with: 12 realms, 10 alive, the Duchy of Brel holding 11 of 31 held regions, six ruins in three regions ("Swineherd's Oath" has three), one hidden refuge ("Aak'kraaz Camp in West Peaks ... hidden") and one live claim. That is a map with places to go and a tension to resolve.

## Patterns across seeds

- Orc first war: median 166 years after the awakening; 19 of 100 default worlds never have an orc declaration (0.20.0: 17); 2 of 20 `civs4`.
- Wars without a stated claim: 0 of 2,601; 77 name a settlement-claim.
- Battle toll repeats: none above 7 of 8,765.
- Calamity share of the chronicle: median 7.8% (15.2%), max 22.9% (53.8%).
- Ruins: seed 0 has 7; seed 84 has 11 (a Gik Mob and a Realm of Ithalal that lost six towns), seed 42 six.
- Names flagged: `steadstead`-type stutters in 6 of 100 worlds (seeds 20, 41, 82 and others), `-asstead` in about as many, `tard` in 1.

## Dark tone

The tone holds. Seed 84 tells of the Gik Mob (seceded 3417 from the Buskrugg Mob, crushed 4077 by two neighbours, "The Gik Mob fell to the Buskrugg Mob" 4971 after five towns lost in 20 years, "The seat of the Gik Mob moved to Gark" three times). Seed 0's Duchy of Cear falls in 111 years: "The Krurz Horde took Edadbury ... 5803", sacks of Waldtaldford, Ashealford, Huldbury and Stuldmeldwick, "The Duchy of Cear fell to the Krurz Horde" 5914, leaving four ruins in the Cruel Barrows. Both are a reader's reasons to go to those regions.

## Suggestions (rules and parameters)

1. Name builder: reject a doubled suffix and `ss` + `s`-initial suffix (F1); run the name filter on region names (F2).
2. `[present]`: list only hidden refuges, or print "found" refuges as a count (F3); cap standing claims at 10 with "and N more" (G1).
3. `history.conflict`: a claim lapses after about 1,500 years without a war (G1); candidate field `claim_max_years`.
4. A line for where a hiding realm went after "found no one" (G2).
5. Hollow-war share of seed 42: no change asked.

## Fix before merge, in one list

1. F1 and F2 (names): small data and filter changes; they make 0.21.0 names cleaner than 0.20.0's.
2. F3 (refuge heading), which is report text only.

Everything else can wait for the next stage.
