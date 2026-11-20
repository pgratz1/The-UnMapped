<!-- Copied by the lead from the world critic's review file; one slur-like generated name is partly masked in this copy. -->
# World critic review: P3-05 "peoples" (generator 0.15.0)

Date 2026-11-19. Material: seeds 42, 0, 1, 2, 3 and 7 at 1024 with default settings, plus seed 42 `--event-intensity turbulent` and seed 42 `--civs-per-race 4`, in `~/.cache/the-unmapped/out/p3-05-validation/critic/` (chronicles, reports, map and region images, world files). I added settlement overlays for each world, `<name>.settlements.png` in the same folder (white elves, red goblins, brown dwarves, magenta faeries, yellow humans, black orcs; squares are fortresses, dot size is kind, cyan squares are ruins). The script that draws them is `critic-plot-settlements.py`. All other figures come from read-only queries of the `.db` files.

This is eight worlds, not the ten or more the role asks for. Where a pattern rests on six default worlds I give counts, and where it needs a sweep I say so.

## Verdict

**Acceptable with follow-ups** as the peaceful base for conflict. The races wake in the right order at the right times, each people has a recognisable homeland and arc, and the end state leaves land hunger, a frontier and empty wilderness for P3-06 to use. Three problems should be fixed in P3-05's rules or data, not left for conflict to hide:

- Dwarven settlements are placed on the lowest farmland of their home region, and that produces dwarven forest cities.
- The quiet-era and pressure lines make the log mechanical and back-loaded.
- Some salience values send the wrong events to the summary.

The name generator also produces some crude and offensive syllable combinations. That needs a blocklist before any world is shown outside the project.

## Checks against the design

- **Order and timing.** Correct in every world. Seed 42 awakenings fall at years 0, 2074, 3242, 4243, 4969 and 5596, against the design's 0, ~2100, ~3300, ~4200, ~5000 and ~5600. The other five default seeds are within 70 years of these.
- **The oldest peoples are visibly old.** Every world's oldest settlements are elven and founded in year 0. They are still standing: seed 42 Voeledell (s1) and Mofaesi (s2), seed 1 Elomaell (e3).
- **Every race settles.** Every race founds at least one civilization with three or more settlements in all eight worlds. The one exception is a secession: seed 1's Clan Aam'urush (e520, 1 settlement).
- **No runaway.** No race holds more than 12 of a world's ~50 regions. That high comes from the goblins in seed 42 turbulent. With default settings the top is 10, held by the goblins and humans in seed 42 and the humans in seed 3.
- **No civilization ends.** None ends in any of the eight worlds. In the six default worlds there are 0 to 3 ruins each and 7 in all. Every one of the 7 is an abandoned elven village.

## Findings, most important first

### F1. Dwarves are put on the lowland of their region, which yields dwarven forest cities (rule change, P3-05)

Evidence:

- The site rule (6.2) takes the free cell with the highest `habitability`. Every race therefore settles the lowest, most farmable quarter of its region. The median elevation rank of a settlement cell within its own region is 0.25 for dwarves, the same as elves (0.25) and goblins (0.23). For dwarves it should be the other way round.
- Seed 7: Clan Nirgurn's home, West Wood IV, rises to 2,076 m. Its 6 settlements sit at a median of about 85 m, between the two elven realms. They include 3 dwarven *cities*, with 52,635 dwarves: more people than both elven realms together (36,909) and than the humans (37,310). Dwarves are the largest people in that world, at 80,588. The other dwarven realm, Clan Dradintuk (East Peaks), has its fortresses strung along the sea shore below the peaks rather than in them (see `seed7-1024.settlements.png`).
- Seed 42: the Hold of Dulimin (home South Wood II, which reaches 2,052 m) is 4 towns in forest. Its capital Grirnbra became a *city* of 8,531 at year 4017 (e146). Meanwhile the southern massif next to it stays empty for the whole history.
- Seed 3: the Dommtom Throne's 4 holds stand at about 126 m on a lake shore, in grass and forest.
- Overall: 4 of 10 dwarven realms in the six default worlds have a home whose dominant biome is forest, and their settlement cells are in forest or grassland in 25 of 59 cases. Dwarven fortresses happen only where the region carries the upland flag, so a dwarf in a mixed region builds a lowland town.

The homeland choice is mostly right; the site inside the region is wrong. Recommendation: add a race-specific site term to 6.2, as data. Candidate score = `habitability` + `port` + w_ore × `ore` + w_rough × (rough or upland cell), with dwarves weighting ore and rough ground strongly. Faeries could take a forest or wet-woodland term the same way, which fits scheduled change (c). Also make a dwarven settlement on a rough or upland *cell* a fortress, not only one in an upland *region*. Secondary: dwarves count walkable lowland in full for capacity (5.1), which is why the forest realms are the large ones. Consider `rough_r` as a bonus and lowland as a discount for dwarves.

### F2. The log is mechanical: about 60% is famine, plague, spread and quiet-era lines, and it is back-loaded into the last 400 years (tuning, wording)

Share of the log that is `famine` + `plague` + `plague_spread` + `quiet_era`:

| World | 42 | 0 | 1 | 2 | 3 | 7 | 42 turbulent | 42 civs4 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Share | 58% | 62% | 65% | 63% | 66% | 61% | 65% | 61% |

`became_village`, `became_town` and colonies inside a region add another 18 to 25%. A reader would skim at least three lines in four.

Events per age, seed 42: Elves (2,074 years) 33, Goblins 72, Dwarves 47, Faeries 64, Humans 130, Orcs (404 years) 184. The Age of Orcs holds 30 to 38% of every default log (seed 3: 253 of 666) and 47% of the turbulent one (382 of 811). In that age 125 to 220 lines are famine and plague. The last 100 years of seed 42 turbulent are 96 lines, 70 of them famine or plague. This happens because the final phase and the orc opening phase are about 400 consecutive tick years with every people at capacity. The famine rule fires on crowding, so the fuller the world, the more famine lines.

Quiet eras have fragmented into noise:

- 25 to 36 of the 70 to 79 quiet-era lines per world cover 50 years or less. For example `2224-2248 e44 The Grork Horde passed 25 quiet years (1 generation)`, followed one year later by a colony (e45).
- Some cover 1 or 2 years: `4968 e216 The Teeshti Court passed 1 quiet year (0 generations), from 4968 to 4968` and `5594-5595 e346 The Kingdom of Stard passed 2 quiet years (0 generations)`.
- 7 to 24 per world say "(0 generations)". The generation count wobbles: goblins have 25-year eras with 0, 1 and 2 generations, in roughly equal numbers.

Recommendations:

1. Merge a civilization's consecutive quiet steps into one `quiet_era` until something happens to it, and write none shorter than about 100 years (or one generation of the race). Report what the stretch held ("…passed 400 years under one line of kings and founded three settlements"), which is the design's "four centuries of peace under the same dynasty".
2. Replace "(0 generations)" with "under one ruling line"; it was never 0 rulers.
3. Turn famine into a *regional bad year*: one event per region per year listing the settlements struck and the total dead. Plague spread could also be written as one line with a total, as an option at the report level (P3-07). At the summary level this matters less, since famines and spreads are below the threshold (F3).
4. Consider lowering `famine_ppm` in the final phase, or letting crowding fall back after a famine, so a people at capacity for three thousand years does not starve once a generation. The elves do: they have 22 to 46 famines per default world, the most of any race in seeds 3 (46 of 127) and 7 (41 of 72). Humans have 5 to 15. Starving elves in an ancient forest realm read wrong. A forage-based people should be among the least famine-prone.

### F3. Salience: plagues are 40 to 46% of the summary, and crises without outcomes crowd it (data)

With P3-07's rule (at most 250, from salience 20 up) the threshold is 26 in all eight worlds, and the summary would hold 101 to 120 events per default world. Of these, plagues are 39 to 46% (seed 42: 45 of 116; seed 1: 55 of 120). Only 7 to 16 of the 39 to 55 salience-45 plagues per world killed 1,000 or more. `3362 e136 Plague broke out in Voeledell; 298 died` ranks with `4017 e146 Grirnbra grew into a city of 8531` and above a succession crisis.

`succession_crisis` (40) appears 6 to 17 times per default summary, and 33 times in the turbulent one, as "A succession crisis shook X." with no stated outcome unless it split the realm.

The summary is also back-loaded: summary events per age, seed 42, are 7, 8, 11, 11, 30 and 49. The first third of history gets 5 to 11 lines in the six default worlds.

What the summary gets right: awakenings, foundings, capitals, colonies into new land (30), cities, secessions (70) and abandonments (55). Those are the right milestones.

Recommendations:

- Make `plague` salience depend on the toll: 45 when dead ≥ 1,000 or the outbreak spread to two or more settlements, otherwise 25.
- Lower a crisis that does not split to 25, and leave the secession at 70.
- Give the crisis template its consequence, for example "…shook X; the court at Y lost N", using the capital's loss as a value.
- Consider a small bonus to the salience of the first `became_town` and `became_city` of each civilization, so an old people's growth reaches the summary.

### F4. Abandonment reads as unmotivated, and so far it only ever happens to elves (rule and wording)

All 7 abandonments in the six default worlds are elven villages 1,700 to 4,800 years old, and 5 of them fall in the last 300 years. The lines read as a whole people walking away from a living village after a small blow:

- seed 42: `e521 The plague spread to Rannve; 68 died.` → `e522 Rannve was abandoned; 869 survivors left.`
- seed 2: `e417` 66 died → `e418 Meithisae was abandoned; 608 survivors left.`
- seed 2: `e469` 96 died → `e470 Ethnan was abandoned; 236 survivors left.`

The real cause is the earlier blow that left the village declining (the frail rule, 8.5 (a)), but the chronicle cites only the last one. "survivors left" is also ambiguous: it can mean they stayed or that they departed.

The pattern itself is good dark fantasy: the elves' oldest villages emptying in the present age. The text has to carry it.

Recommendations:

- Cite both the last blow and the blow that began the decline.
- Name the destination: "Rannve was abandoned; its 869 survivors went to Voeledell", or "…were scattered" when none had room. Settlement t is already known in 8.5.
- Consider requiring a cumulative loss since `before` (for example ≥ 30%) for the random abandonment, so a village does not go after losing 8%.

### F5. Names: offensive and comic combinations, and bare elven realm names (data)

This is a list from about 700 names; a full sweep with a blocklist check is needed.

- **Offensive risk:** goblin `N*gsnib Pit` (seed 2, also e124), `Snig` and `Drinigg`. The goblin syllable set can form a racial slur at a word start. This needs a substring blocklist in the name generator (P3-02) before anything ships.
- **Crude or comic:** human `Cockford`, `Cockton`, `Cackford`, `Cickalolwick`, `Wardsickbury`; dwarf `Bumdou`; goblin `But-Skrotch`, `Snotgab`, `Sneezskrub Hole`, `Bag Hole`; orc `Dog Fort`, `Dork`, `Baa Fort`; faerie `Pypy`, `Bap Glade`; gods `Buzz` (seed 3), `Bald` (seed 0) and `Gord` (seed 7). A god called Bald undercuts the tone more than any settlement could.
- **Gods** take short peasant-like forms from their race's culture: `Sten`, `Steck`, `Alis`, `Krok`, `Kruu`. They should have their own style, longer and stranger, perhaps with a domain epithet in P5. `Vanir` (seed 7) is a real pantheon name.
- **Elven realms are often bare names:** `Finen`, `Nife`, `Glaefaenn`, `Ithasfa`, `Roelaele`, `Vefal`, `Ithathiecae`, which is 7 of 14 elven civilizations. The result is "The Elves founded Finen in West Wood III. Finen founded Voeledell", where a realm reads like a town. Every other race's realms carry a form ("The Hold of", "Clan", "The … Horde"). Give elven realms a form always (Realm, Court, House, Kingdom-under-the-Leaves).
- **Repetition within a culture:** 43 of 176 elven settlements end in `-dell`. Goblin names end in ` Pit` (65) or ` Hole` (52) in half of all cases. Same-core pairs occur within one world: seed 2 `Skreek Pit` and `Skreek Hole`; seed 1 `Bonk` and `Gonk`, `Slok` and `Slork`. Goblin and orc stems overlap (`Grork` is a goblin horde and an orc town).
- **What is good:** each culture is recognisable at a glance (elven vowel runs, dwarven consonant clusters with Deep and Hold, faerie whispers with Glade and Hollow, English-like human -bury, -ford and -wick, orcish apostrophes), and human realms (`The Kingdom of Stard`, `The Duchy of Dant`) read well.

Recommendations: a blocklist of substrings at word starts (P3-02 rule), a separate god name style, a mandatory form for elven civilizations, and caps on how often a suffix may repeat within a world. Settlement uniqueness should be extended to "not the same core with a different suffix".

### F6. Race character: mostly right, with human and elven exceptions (tuning, mostly leave)

- **Elves (few, old).** Elven population is 13,035 to 36,909, against 29,416 to 81,199 for goblins. On the map, though, elves are the densest dots in the north and west: 35 settlements in seed 3 and 29 in seed 7, the most of any race in both. On a map they read as many small villages, not few. Their capitals are small for the oldest people: seed 42's second realm, the Realm of Ithieglie, is 4 villages and 1,411 elves on Southeast Isle, with a capital (Mofaesi) of 659 that has not grown since year 1250. That makes a good isolated-island hook; leave it. Elves outnumber goblins in seed 7 (36,909 against 34,760). Recommendation: a lower `region_slots_max` for elves (2 or 3) or a higher elven `capital_weight`, so an elven realm is a few halls, not a scatter of hamlets.
- **Goblins (numerous).** They are right. They fill the open country within about 800 years and split by secession: in seed 42 the Grork Horde splits three times (e198, e256, e367). Nearly all goblin settlements are towns (seed 42 Zak-Neez Mob: 12 towns, 0 villages), which is fine at the compressed scale.
- **Dwarves.** See F1.
- **Faeries (few, hidden).** Their numbers are right (1,817 to 12,617). Their places are not hidden: not one of 52 faerie settlement cells in the six default worlds is in fen, marsh or wet woodland, 15 are on grassland, and in seed 1 and civs4 they line open coasts. Scheduled change (c) plus a forest site term (F1) should fix this. In civs4 the Sheesi Court's home is North Downs IV.
- **Humans (late, expansive).** They are the largest people in 3 of 6 default worlds (42, 0, 3), with 88,254 to 128,609 and 8 to 10 cities in seed 42. In seeds 1, 2 and 7 they hold 2 to 3 regions and 33,109 to 37,310 people, smaller than the goblins and, in seed 7, the dwarves. Seed 1's Hodmal League is squeezed into a strip of the north-west coast between elves, orcs and goblins. That is exactly the frontier P3-06 should break open. Leave it for conflict.
- **Orcs (last, few).** Mostly right: 4 to 8 settlements and 8,480 to 18,328 people. Seed 0 is the exception, with 34,936 orcs in 466 years, more than the 6,000-year-old elves (23,888). Seed 1's Clan Thaald has three succession crises in 343 years and spins off a one-town clan (e450, e477, e519, e520). That is odd for a new people but plausible. Leave it.

### F7. Settlements and cities that never change, and cities that come and go (rule, minor)

Populations sit exactly at their shares for millennia: seed 42 Voeledell is 1,800 from 2073 to 5595, Zugnob Hole 6,260 from 3241 to 5999, and Rienigla 600 from 2073 to 5595. That is what the quiet-step rule promises, but on P4's timeline scrubber the map will not move for two thousand years at a time. Conflict will change this; P3-05 alone does not need to.

Cities flicker:

- seed 42 Bildheldham became a city of 8,479 at 5269 (e290) and "dwindled to 6316 souls" at 5344 (e302, no cause);
- seed 42 Stuckham became a city at 5469 (e323) and dwindled at 5569 (e343);
- seed 0 Lafin became a city at 4065 (e131) and dwindled at 4628 (e177).

The cause is the city's own colonists: new settlements in the region reduce its share (5.2). The dwindle line gives no reason and does not say the city lost its rank.

Recommendations:

- Template: "{settlement} shrank to a town of {population}".
- When the reduction comes from a new settlement taking the region's share, cite that founding as the cause.
- Or exempt a capital's share from colonies inside its region for one generation.

### F8. Turbulent and four civilizations: they mostly do what a player expects

**Seed 42 turbulent** has:

- 811 events against 530;
- 126 plagues against 60 and 162 famines against 101;
- 33 crises and 6 secessions against 10 and 3;
- 5 abandonments against 1;
- 15 civilizations, 7 of them goblin.

Because draws are keyed it is the normal history with more on top (the same secessions in years 4392, 5097 and 5637), which is a fine property. But the world ends *more* populous (257,938 against 249,649) and holds *more* land (34 regions against 31), because secessions move capitals and so extend reach. No civilization ends. A player who picks "turbulent" expects a scarred present: more ruins, fallen realms and smaller peoples. At P3-05 it gives a busier log and a more fragmented goblin map, not a darker world. Recommendation: let intensity scale the abandonment chances (`abandon_permille`) and the plague tolls too, not only the rates (10.1 excludes the tolls). Re-check after P3-06, where intensity will also drive war.

**Seed 42 civs4** gives exactly what was asked: 26 civilizations, 4 of every race, 178 settlements and 49 of 50 regions held (see `seed42-1024-civs4.settlements.png`). The map is full, the late races are squeezed, and humans fall to 82,276 people in 6 regions. The southern massif still holds no one. Weak realms appear and are interesting: Clan Uul'graal is 4 orc fortresses with 1,914 people in the West Peaks, and the Duchy of Teck has 2 settlements. This works. The log reaches 1,080 events with 170 quiet-era lines, so F2's merging matters most here.

### F9. Causes: correct, but short and local

Every cause I checked made sense:

- plague, then spread, then abandonment;
- crisis, then secession;
- founding, then growth in kind;
- abandonment, then moved capital (civs4 e986, then e987: the Taflie Court moves its seat after Foss is abandoned).

But chains are at most 4 deep in every world, and 63 to 66% of events have no cause. Colonies are uncaused by design, and no cause ever crosses from one civilization to another. You cannot yet follow a chain from an early event to a present-day ruin across peoples; that is P3-06's job (F11). One cheap peaceful link would be plague crossing a border at a lower chance (the spread now stops at the owner's border, 10.2). The first cross-civilization cause would then exist before war, as in "the plague came from the goblin towns".

### F10. Wording, small items

- "taking 4 settlements with it" appears in 15 of 17 secessions in the eight worlds; the other two take 1. The seceding part is always one full region (the slot limit is 4). This is not wrong, but it is uniform. Let secession sometimes take two regions when the far side has them.
- "The Elves founded The Realm of Ithieglie": scheduled change (b) covers the capital "The".
- "Famine struck Mofaesi; 66 died." is fine, but with famine regional (F2) it would read better as "Famine in Southeast Isle: 66 died in Mofaesi and …".
- Region names: scheduled change (a) will help a great deal. "Northeast Downs IV" is the single most repeated noun in the chronicles.

## The lead's specific questions

**(i) An elven realm of one village that died of plague within forty years (seed 17 turbulent).** I could not inspect it, since it is not in my folder. My view: an early failed realm is good history, a "lost colony" that P6 can turn into an ancient ruin. It should be rare and legible, not a coin toss on year 30. Recommendations:

- A civilization's *last* standing settlement should never be abandoned by the random frail chance of 8.5 (a). A people with nowhere to go does not walk away from its only home; it holds on until it falls below `abandon_min`.
- Keep the deterministic death below `abandon_min`, so a realm can still die of a plague that leaves too few to go on.
- Never let decline end a race's *only* civilization in P3-05, so no race goes empty by plague. P3-06 may still destroy one by conquest.

With that, seed 17's realm would survive the plague as a fragile village, unless the plague left fewer than `abandon_min`, in which case its end is a real and readable catastrophe.

**(ii) Faerie villages that take in refugees stay above their size cap indefinitely.** I found no settlement above its kind cap in the census of these eight worlds (faeries ≤ 1,499, goblin and orc towns ≤ 7,999), so I cannot judge how it reads. In principle it is a small absurdity, a "village" of 2,500. Recommendation: in T1's decline branch use the target min(C_s, kind_cap(s)), so the excess drains at `decline_permille` like any other excess. Alternatively, let surplus refugees found a hamlet when the receiving village is at cap, which is better history (a refugee hamlet with a cause).

**(iii) At map size 512 a late race sometimes holds no region at all.** I did not review 512 worlds. As a premise it is good for dark fantasy and for P3-06: a people that wakes as a minority in land another people holds. That is the strongest possible tension seed, and it fits the design's humans "spreading over land left open or ruined". It needs two things:

- The chronicle should say so: the awakening or `civ_founded` sentence should read "…in West Downs, land held by the Grork Horde".
- P3-06 should treat a landless civilization as at maximum land hunger, with the holder's region as its wanted target, so it is resolved by war or by expansion into freed land rather than lasting forever.

If P3-06 cannot promise that, give the landless people the first free region adjacent instead.

**(iv) What the chronicle most needs from conflict.** In order:

1. **Endings and ruins.** No civilization ends in any of the eight worlds. Ruins are 0 to 3 per default world (7 in all) and all are elven villages. The present day needs sacked towns, fallen capitals and a few dead realms for P6 to make sites from.
2. **Cross-civilization causes.** Every chain now stays inside one people. A war caused by land hunger (seed 42's Grork Horde has been boxed in since its third secession), a sack caused by a war, a capital moved because of a sack, and refugees founding a colony elsewhere would give the first chains a reader can follow from an old event to a present ruin.
3. **The goblin-elf enmity from day one.** Elves and goblins live side by side for about 3,900 years in every world; in seed 42 the goblins found Snugg in Central Wood and Gruz-Keegg in South Wood (e52, e54) beside the elves without a word.
4. **Events in the long middle.** The Ages of Dwarves and Faeries are thin, with 44 to 94 events of which 20 to 55 are pressures. Flare-ups from tension in quiet intervals would put events where the log has its gaps: seed 42 has nothing between years 3592 and 4017.
5. **Human expansion at others' expense** in worlds like seeds 1, 2 and 7, where humans are boxed in.

The Age of Elves (19 to 38 events and no loss in 2,000 years) is not conflict's to fix. That waits for P5's gods and monsters, and is noted here so it is not forgotten.

## What is good and should be protected

- The awakening schedule and the pacing of the ages, and the first two thousand years as an elven idyll: slow foundings a century apart (seed 1 e7 to e19) give a real sense of deep time.
- Homelands. Elves are in coastal deep forest (15 of 16 seed 42 elven cells are forest), goblins on grassland and steppe, humans on coastal farmland (17 of 22 seed 42 human cells are grassland), and orcs on the last free downs. Read on the maps, each world has peoples in separate, recognisable territories, with large empty wilderness left over (the southern forest of seed 42, the western forest of seed 3, the eastern forest of seed 1).
- Reach plus secession. The goblin hordes splitting into Mobs and Warrens (seed 42: four goblin realms from one by year 5637) gives P3-06 rivals of one race, and boxed-in peoples with long hunger.
- Kind growth caused by the founding event, colonies with an origin settlement, and the moved capital caused by the abandonment: the bookkeeping is sound and every cause checked made sense.
- The elves' oldest villages emptying in the last centuries (F4) is the right note for the tone. Keep the mechanism and fix the text.
- Keyed draws making turbulent a superset of normal on the same seed.
- The scale: worlds of 170,000 to 270,000 people, human cities of 8,000 to 17,000, and elven realms of 13,000 to 37,000 fit the compressed scale of design section 4.

## What I could not judge

- Seed 17 (question i) and 512 worlds (question iii), which are not in the folder.
- Refugee villages above their cap (question ii), which do not occur in these eight worlds.
- Patterns that need a sweep (for example how often dwarves end up in lowland forest); my counts are over six default worlds.
