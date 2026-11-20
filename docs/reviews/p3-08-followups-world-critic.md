# World critic review: P3-08 names and peoples follow-ups (generator 0.17.0)

Date 2026-11-20. Material: the 13 worlds in `~/.cache/the-unmapped/out/p3-08-validation/critic/`, which are seeds 42, 0, 1, 2, 3, 7, 11 and 23 at 1024, seed 42 `turbulent`, seed 42 with four civilizations per race, seeds 42 and 5 at 512, and seed 42 at 2048. The same worlds at 0.16.0 are in `p3-06-validation/critic/`. I also ran read-only queries on the 100 default 1024 sweep worlds of both versions. For dwarves and faeries I drew settlement overlays (white elves, red goblins, brown dwarves, magenta faeries, yellow humans, black orcs; squares are fortresses, cyan marks ruins). The scripts and images are in my session scratchpad (`crit08/`), not in the repository. Event ids and years below are those of the 0.17.0 worlds. No generated name in this set is slur-like, so none is masked.

## 1. Verdict

**Acceptable with follow-ups.** Nothing blocks the merge. The change does what both reviews asked, and the worlds now read like places rather than spreadsheets:

- Regions have names you can remember.
- The dwarves live in the mountains.
- The worst names are gone.
- A quiet stretch takes one line.
- Wars no longer kill more victors than losers.

What remains is mostly about how the new pieces read together:

- Major plagues are announced with small death tolls.
- Abandonments state no reason at summary level.
- Quiet eras come in blocks of identical lines.
- Faeries line the coasts.
- Upland dwarves have almost no history.
- A short list of names still makes a reader laugh.

## 2. What works

- **Region names give identity, and they read well in sentences.** Examples:
  - "The Grork Horde laid claim to the Wood of Stags, held by the House of Finen" (seed 42, `2148 e27`);
  - "A'ghaz awakened the Orcs in the Sweep of Bones" (seed 3);
  - "Maarkzaagh awakened the Orcs in South Downs", and the orcs then fight over the Bloodless Wilds (seed 7).

  Other names a player would want to visit include:
  - the Brake of the Forgotten, Drover's Sorrow and Stranger's Oath (seed 42);
  - Wraithstrand and Emberhurst (seed 23);
  - the Weald of Smoke (sweep);
  - Rookstrand Forest and Wardtree Wood (seed 2, seed 7).

  The land words fit the land:
  - shore words (strand, kelp, wreck, spray, gull, landing) appear only on regions with coast cells, in all 13 worlds;
  - "The Leaden Stack" and "The Stack of Martyrs" (seed 0) are islets of 235 and 462 cells.

  The lower-case "the" works everywhere. No sentence in the 10,860 of the set has a capital "The" after its first word.
- **The worst names are gone.** None of the names the two earlier reviews cited survives except `Sneezskrub Hole` (seed 42, as the spec says). That includes the slur-like goblin forms, `Turdstead`, `Cockford`, `Cockton`, `Bald`, `Buzz` and `Gord`. The goblin, orcish and elven styles are intact.
- **The House of …** fixes the bare elven realm. "The House of Finen moved its seat to Rannve" and "The Grork Horde sacked Voeledell of the House of Finen" (seed 42, `4794 e274`) now read as a realm and its town. Elven realm forms vary between House, Realm and Court across the set.
- **Gods are longer and stranger**, for example Uruld'kruum, Nurnthaimm, Maarkzaagh, Kruskux, Khummhor, Kaag'azak and Syfiash.
- **Dwarves read as dwarven on the map in most worlds.** Seed 2's Clan Brark and Clan Drommgird are 16 fortresses through the central massif on ore-rich alpine ground. Seed 0's Clan Durgarin has 8 fortresses at 111–148 m on ore up to 255. Seed 3's Dommtom Throne holds the hills above the lake, and seed 5 at 512 has its holds at 145–211 m. Sweep: 9 of 144 dwarven realms are mostly lowland. In P3-05 that was the rule.
- **Faeries are forest people.** In the default sweep 807 of 861 faerie settlements stand in temperate forest.
- **Quiet eras read like the design's "four centuries of peace under the same dynasty".** Examples: "The House of Finen passed 1924 quiet years, from 150 to 2073, in which 12 new generations of rulers came to power" (seed 42, e6), and "…passed 25 quiet years … under one generation of rulers". "(0 generations)" is gone.
- **Crises in the summary now matter.** Each of seed 42's three summary crises is followed by its secession (e209 → e210, e412 → e413, e581 → e582), and the sentence names the court: "A succession crisis at Zugnob Hole shook the Grork Horde".
- **Wording fixes hold.**
  - No "and 0 fled" in the set.
  - Victors never lose more than the defeated.
  - Secessions now take 3, 4, 6 or 8 settlements instead of always 4 (P3-05 F10).
  - "0 of the victors fell" does not occur in the 13 worlds.
- **Passes are fought over.** Seed 5 at 512 has 14 `battle_at_pass` lines in its summary (P3-06 F8 noted 55 of 9,450 battles before).

## 3. Findings, most serious first

### F1. A "major" plague is announced with a small toll (template; question 5)

The salience is now set by the outbreak's total toll including its spreads, but the sentence gives only the outbreak's own dead. So the summary ranks lines like these as major events, beside sacks and secessions:

- seed 11 `5913 e551` "Plague broke out in Gork-Skreeg Hole; 23 died." (salience 45);
- seed 23 `3293 e69` "Plague broke out in Raenme; 120 died.";
- seed 2 `4269 e230` "…Geeg; 299 died.";
- seed 2 `5119 e412` "…Skogig Hole; 282 died.".

Across the 13 summaries, **53 of 153 salience-45 plagues state fewer than 1,000 dead, and 6 state fewer than 300.** A reader sees a contradiction: the minor plague is in the summary while a 571-death plague elsewhere (seed 42 `3829 e148`) does not show why it matters.

Fix: give the outbreak sentence the total, for example "Plague broke out in Gork-Skreeg Hole and spread to 4 settlements; 1,0xx died in all." The planned total already exists (P3-05 r7 10.2, invariant 13). It can be a stored value, or the spreads' count can be a value of kind 13.

### F2. Abandonment has a destination but, at summary level, no visible reason (template; question 6)

- **All 7 abandonments in the set are kind 31, and their causes are right:**
  - seed 23 e899 Ithononiedell: famine 5713 (e870), then plague 5774 (e898);
  - seed 7 e145 Mieglonn: plague 3230, then famine 3487;
  - seed 42 at 512 e465 Iennmo: plagues 5553 and 5617.
- **Destinations read well:** "Ithononiedell was abandoned; 489 survivors went to Elaethaer." The P3-05 ambiguity ("survivors left") is fixed.
- **The causes are hidden at summary level.** Every cause in the set is a salience-25 famine or plague, below every summary threshold (26–51), while the abandonment (55) is shown. In the summary these are towns that empty for no stated reason.
- **The blows are small for what follows:**
  - seed 2 e406: Kizzslig, about 1,100 people, lost 60 to a plague spread and was abandoned by its 1,043 survivors;
  - sweep: survivors have a median of 910 (222–1,197), and the decline started a median of 40 years earlier, at most 1,180.
- **One old cause reaches far back.** Seed 42 `5117 e432` "Glathenddell was abandoned; 1155 survivors went to Drux-Zeek Pit" cites a plague of 4254, 863 years earlier.

Fix (wording only): put the cause into the sentence, for example "Weakened by famine and plague, Ithononiedell was abandoned; its 489 survivors went to Elaethaer." A one-word form of the first cause's kind would do. The pattern itself is good for the tone: in the sweep 24 of 47 abandonments are elven, so the elves' old villages empty in the last age.

### F3. Quiet eras come in blocks of identical lines (rule or report; question 5)

Merging works: seed 42 falls from 94 to 49 era lines, and `civs4` from 202 to 101. But every civilization gets its own line for the same quiet row, with the same years:

- seed 42 `4393–4417`: e211–e217, seven lines of "The X passed 25 quiet years, from 4393 to 4417, under one generation of rulers";
- seed 42 `3392–3816`: e132–e136, five identical spans.

With four civilizations per race the eras fall into 11 spans holding 101 lines. Two spans are 25 years long, with 12 lines (4393–4417) and 14 lines (5119–5143). 33 of the 101 eras are 50 years or shorter, and 17 say "under one generation of rulers". A 25-year gap between two flare-ups is not an era.

The validation's F4 and the sweep (`civs4`: 30.5 short eras per world) show the same.

Fix (either):

- (a) Write one world-level line per quiet row: "From 4393 to 4417 the land was quiet." List the realms only when the row lasts at least one generation of the longest-lived race present.
- (b) Write no `quiet_era` for a row shorter than about 100 years. The stretch is already visible as the gap between dated events.

The goblin count also reads as noise: "The Grork Horde passed 725 quiet years … in which 48 new generations of rulers came to power" (seed 42 e35), and 71 in seed 11 (`2220 e23`, the Grurk Mob, 2220–3294). A per-culture noun, such as "48 warlords rose and fell", would make the number characterful.

### F4. Faeries live in forest, but on the shore (data or rule; question 4)

The biome is right. The placement is not hidden:

- Seed 3: all 7 faerie settlements of the Circle of Feefliaie line the west coast, and the forested interior behind them is empty.
- Seed 7: the Shiap Ring lines the north coast.
- Seed 1: the faeries sit on the east coast, among human ports.
- Sweep (every fourth default world, 25 worlds): **51% of faerie settlements stand within 300 m of the sea**, more than any other race (humans 48%, elves 41%, orcs 31%, goblins 11%, dwarves 2%).

The port term of the site score (6.2) is probably what pulls them there. The forest weights choose the region, and the port term then chooses the coast within it. "Hidden communities in wild places" (design 5) needs the opposite.

Fix (data): give faeries no port bonus, or a negative coast weight, and a cell term for forage or distance from water and roads, using the `cell_weights` mechanism 5a added.

Also: no faerie settlement in the sweep stands in fen, marsh or wet woodland, which design 5 names first for faeries ("fens and wet woodland suit faeries"). Their survival (23 of 100 default worlds end with one settlement) is P3-09's.

### F5. Dwarves: high and dwarven, but nearly absent from the history; a few still in the lowland (rule and data; question 3)

**Fewer settlements is not the problem.** Four to eight fortresses on one massif read better than 6.5 lowland towns. A hold of four fortresses is a dungeon cluster, and a player will find it.

**Absence from the history is the problem.** An upland realm is left alone and has almost nothing in the log:

| Realm | World | Events in 2,700 years | Wars | Summary lines |
| --- | --- | --- | --- | --- |
| The Narinthri Throne | seed 42 | 17 (3 colonies, 5 famines, 1 plague, 7 eras) | 0 | 2 (founding) |
| Clan Durgarin | seed 0 | 23 | 0 | 3 |
| Clan Brark | seed 2 | 28 | 0 | 2 |
| Clan Drommgird | seed 2 | 30 | 0 | 4 |

By contrast, a dwarven realm that ends up on the war road (seed 42's Hold of Dulimin, seed 23's Hold of Muli, seed 7's Clan Nirgurn) has 100 to 150 events. Design 5 says dwarves "build in places the others avoid", and so it is right that they are spared. But a people with no history gives P6 no hooks. Mines, delvings and lost holds are P5 material, but even now the dwarves never declare a war: 0 to 3 declarations per realm in the set. P3-09's agency work should include them.

**Residual lowland realms.**

- Seed 7's Clan Nirgurn is unchanged from the P3-05 review's example:
  - its capital Gaimbo Hold is a city of 8,733 at 79 m (rank 0.01 in West Wood);
  - 8 of its 10 settlements are towns or cities at ranks 0.01–0.31 around the lake;
  - its second city, Draird Hold, has 13,066 people;
  - with 52,691 dwarves it is the largest realm in the world, and dwarves are again the most populous race in seed 7 (82,792, against 26,322 humans).
- Sweep: 9 of 144 dwarven realms have most settlements in the lowest quarter of their region, and 25 of 144 first settlements are in the lowest quarter. Dwarves are the most populous race in 2 of 100 worlds.
- West Wood rises to 2,076 m but its high ground is ore-poor (ore mean 43), so the height and ore term (at most 153 + 102) loses to lakeshore habitability plus port.

**Sea-level "Deeps".** Seed 11's capital Thord Deep is a fortress at 0.1 m in coastal forest. Seed 42 at 512 has Malika and Khilikhi at under 1 m. Seed 7 has Nodinkair, Khirinnak Deep and Toudin Hold at 0.6–2 m. They are fortresses because their region is upland, not because of the cell. Sweep: 37 of 744 dwarven settlements stand below 30 m (12 fortresses, 15 towns, 5 cities).

Fix (data):

- Give dwarves `port` 0.
- Apply the P3-05 critic's secondary point: dwarves count lowland capacity at a discount.

### F6. Names still to fix (data; question 2)

These are not offensive, but they are comic or English by accident, all from the 13 worlds:

- **human**:
  - `Suckford` (seed 1 s129);
  - `Realdeadstead` (seed 42 at 512);
  - `Teamstead` (seed 11);
  - `Backham` (seed 1);
  - `Measham` (seed 3; a real English town);
  - `Gockford` and `Gockstead`, a near-`Cock-` pair in one world (seed 7);
  - `Tardrosford` (seed 42 at 2048);
  - the realm "The Timhack League" (seed 1 `5923 e963`; "Tim hack" reads modern);
  - "The Duchy of Teck" and "The Burd League" (`civs4`).
- **faerie**:
  - `Shiety Glade` (seed 3 `5774 e560`, which reads as "shitty");
  - `Whop` (seed 42, the Teeshti Court's capital in the summary, `4979 e327`);
  - `Shaepee` (seed 11);
  - the realms "The Twiwhiwou Ring" (seed 0, in the summary) and "The Lousstwo Court" (seed 1);
  - the gods `Flopfia` (seed 11), `Ouwheefy` (seed 7) and `Biewie` (seed 2).

  Faerie names are meant to be light, but "Flopfia awakened the Faeries" undercuts a creation.
- **gods**:
  - `Gordman` (seed 7, human; reads as a surname, and the old `Gord` again);
  - `Grammam` (seed 5, orcish; "grandma");
  - `Mammga` (seed 7, dwarven);
  - `Stenedock` (seed 2);
  - `Alil` (seed 23, elven, three letters for a creator).

  Human gods read as yeomen's names: 4 of 9 start `Ste-`.
- **elven**: `Vomethidell` (seed 1).

**Same-core names within one world** (P3-05 F5, not taken up):

- seed 11 `Skat` and `Skat Pit`;
- seed 2 `Skreek Pit` and `Skreek Hole` (cited in P3-05, still there);
- seed 3 goblin `Kuz Pit`, goblin `Kuzz` and orcish `Kuz`;
- seed 42 "The Slig Mob" and `Sligg Pit`;
- seed 23 "The Warrens of Gruz" and `Gruzz`, and the dwarven realms "The Hold of Nili" and "The Hold of Muli";
- `civs4`: "The Realm of Ithieglie" and "The Realm of Ithoithegle".

Within 8 worlds I count 49 pairs one letter apart, 17 of them in seed 23.

**Over-filtering.** The `words` list rejects goblin roots that are the goblins' own vocabulary, which supports the owner's decision to shorten it:

- grub, stink, slob, snob, bog, nit, gnat and gag (`rejected-names.txt`: g\*ub ×24, s\*ink ×19, s\*ob ×34);
- dwarven roots such as grim, horn, brand, grind, tarn and deep-sounding words ("Tarn" survives in seed 2 and fits a dwarven hold).

Short goblin roots are the main source of same-core pairs, so shortening the list will also give the goblin pool more room. I see no sign that the filter makes names samey; the sameness is the goblin syllable set itself.

### F7. Region names: the plain compass names fall on the busiest regions, and the drawn set leans on gloom and the parish (data; question 1)

- **The biggest regions keep the dullest names.** The keeper rule gives the plain name to the largest region of a base name, which is often the most contested. The region most often named in the summary is plain in:
  - seed 1: North Downs, 23 lines;
  - seed 3: North Wood, 27;
  - seed 11: Southeast Wood, 21;
  - seed 42: South Wood, 26, and West Wood, 21.

  Plain regions carry 39–100 summary references per world, against 72–143 for drawn ones. Seed 42 still has seven compass Woods (North-east, West, Central, East, South-east, South-west, South), three of them side by side in the south where the feud is fought.
- **The mix depends on map size.** Plain names are 84% of regions at 512 (330 of 393 in the sweep; seed 42 at 512 has 12 of 15, seed 5 has 9 of 12), 42% at 1024 and 16% at 2048. A small world gets almost no named land.

  Suggestion: keep the plain name only for a base name that occurs once. Or draw a name for every region whose base name occurs three or more times, so that the plain compass name survives only where it is unique.
- **Gloom density.** I counted drawn names carrying grief, death or ruin words:
  - 15 of 29 in seed 0;
  - 9 to 10 of 26–35 in most worlds;
  - 55 of 163 at 2048, for example Sorrowdale Vale, Dreadwold, Weepgrove, Mournshaw, Griefwood, the Lamenting Shaw, the Lamenting Woods, the Doleful Weald and the Orphaned Hurst.

  When every third region is a lament, none stands out. And names are drawn before history, so the elves' 6,000-year idyll is "Dreadlaw Hurst" (seed 0) and the humans awaken in "the Crumbling Barrows" (seed 0) and "Dirgemark Downs" (seed 1). Suggestions:
  - cap the grim adjective and noun share at about one in five drawn names;
  - later (P5 or P6), let history rename a region after its defining event ("the Brake of the Forgotten" earned by a sack), which is what design 4 already promises ("until history gives them cultural names").
- **Church vocabulary.** Abbess's Knoll (seed 0), Sexton's Steppe (seed 11), the Holt of Saints, the Plains of Saints, Roodcross Steppes, Roodford Holt, Prophet's Weald, Pilgrim's Vigil and Tithedown come from an English parish, not from this world's pantheon. 57 of 4,951 default sweep regions carry abbess, sexton, saint, rood, pilgrim, prophet, martyr, tithe or unshriven. The occupational possessives (Squire's, Bailiff's, Steward's, Miller's, Drover's, Pedlar's, Swineherd's, Poacher's) are fine in moderation, but they set a rural-English tone that sits oddly on elven land.
- **Smaller points.**
  - Tautology: "The High Heights" (seed 11, region 1).
  - Lowland "gaps" and "clefts": Hawk's Gap in grassland at most 118 m, and Hound's Cleft at 296 m (seed 0).
  - Same-theme clusters at 2048: of the Harvest ×4, of Antlers ×3 (including "the Wood of Antlers" and "the Woods of Antlers"), Sheaf- ×3 and Wyrm- ×4.
  - At 1024: Hurst ×6 in seed 1, Grove ×6 in seed 7, and seed 1's "the Weald of Floods", "the Flooded Thicket" and "the Flooded Weald".
  - A per-world rule of one use per descriptive stem would remove these.
  - "Starkhanger", "Kitehanger" and "Gallowhanger" are correct English toponyms, but "-hanger" reads as a coat hanger to most players.

### F8. The summary's balance depends on its threshold more than on this change (P3-07 or P3-09; question 5)

Per-world composition of the summary:

| World | Threshold | Composition |
| --- | --- | --- |
| seed 42 | 31 | balanced: 48 claims, 41 declarations, 16 sacks, 16 plagues, 12 cities, 9 foundings |
| seed 0 | 26 | battles (30) flood in: 46 battles of 160 |
| seed 3 | 26 | 58 battles of 250 |
| seed 11 | 26 | 69 battles of 243 |
| seed 1 | 46 | pure war: 165 lines, no city, colony, plague or founding of a settlement |
| `turbulent` | 46 | pure war |
| `civs4` | 51 | pure war |

The plague share fell as intended (83‰ at the sweep median). The summary is still back-loaded: seed 42 has 5, 10, 18, 43, 65 and 95 lines per age. In the quietest default worlds the last millennium holds 54% (s72), 68% (s74) and 73% (s81) of the whole log, and s72 has 10 events in its first 2,000 years. This is P3-07's selection (the per-age floor) and P3-09's subject. I record it so that it is not credited to this change.

### F9. Wording (question 7)

- Numbers above 999 have no separator: "1155 survivors", "Gouringi grew into a city of 8056", "passed 1924 quiet years". Design 4 writes "20,000". Thousands separators belong in the renderer.
- "0 of the victors fell" (32 sweep sentences, none in this set; d512 s18 has 27): use the three-word form, for example "…; 4 of the defeated fell, and none of the victors." The repeated tiny battles behind it ("The Zux Mob defeated the Twosspie Ring in the Weald of Smoke; 4 of the defeated and 1 of the victors fell", five times in 8 years) are the last refuge, which is P3-09's.
- Agreement: "The Warrens of Zeez moved its seat to Gnog Pit" (seed 1 e298, and every Warrens): "The Warrens … moved their seat", or a per-form pronoun.
- "In 2949 tensions flared into 50 years of upheaval" (seed 42 e73) still names no one (P3-06 F4).
- Repeated identical claims ("…laid claim to the Wood of Stags, held by the House of Finen" six times, 2148–4217, seed 42) are P3-09's futile wars.
- `settlement_dwindled` causes can be odd. Seed 42 `4643 e246` "Glathenddell dwindled to 1142 souls" cites e226, a battle 218 years earlier between other parties in another region. 15 dwindles in the set still have no cause.

### F10. For P3-09, one line each

- Faeries win battles against much larger forces: "The Opflie Court defeated the Slatch Horde at the pass into Southeast Wood; 491 of the defeated and 14 of the victors fell" (seed 5 at 512, e328).
- Goblins still start most wars. The dwarves and faeries in this set declared 0 to 3 wars per realm.
- Repeated futile wars over one region continue: seed 42, the Wood of Stags, six claims and five wars, 2148–4292.
- The last refuge holds; see the tiny battles in F9.
- Long peace at 2048: seed 42 at 2048 has 5 declarations in its summary.

## 4. Patterns across seeds (counts)

| Pattern | Count |
| --- | --- |
| Salience-45 plagues in the 13 summaries that state < 1,000 dead / < 300 | 53 / 6 of 153 |
| Abandonments in the set whose causes appear in the summary | 0 of 7 |
| Abandonments by founder race, default sweep | elves 24, goblins 16, faeries 3, dwarves 2, orcs 1, humans 1 (47) |
| Quiet eras of ≤ 50 years: seed 42 / `turbulent` / `civs4` | 7 of 49 / 14 of 69 / 33 of 101 |
| Identical quiet-era spans in `civs4` | 101 lines in 11 spans |
| Faerie settlements within 300 m of the sea (25 default worlds) | 51% (humans 48, elves 41, orcs 31, goblins 11, dwarves 2) |
| Faerie settlements in temperate forest / fen, marsh or wet woodland (100 default) | 807 / 0 of 861 |
| Dwarven realms mostly in the lowest quarter of their region (100 default) | 9 of 144 |
| Dwarven settlements below 30 m (100 default) | 37 of 744 (12 fortresses, 5 cities) |
| Upland dwarven realms with ≤ 4 summary lines and 0 wars (set) | 4 of 15 (seed 42 Narinthri, seed 0 Durgarin, seed 2 Brark and Drommgird) |
| Famines per settlement founded: elves / goblins / dwarves / humans (0.16.0 → 0.17.0) | 1.09 → 0.61 / 1.49 → 0.80 / 0.83 → 0.48 / 0.37 → 0.21 |
| Plain region names: 512 / 1024 / 2048 | 84% / 42% / 16% |
| Drawn region names with gloom words | 4–15 per 1024 world; 55 of 163 at 2048 |
| Region names with church vocabulary (100 default) | 57 of 4,951 |
| Same-core name pairs (one letter apart), 8 default 1024 worlds | 49 (1 to 17 per world) |
| Comic or English-by-accident names in the 13 worlds (F6) | about 25 |

## 5. Answers to the questions

1. **Region names.** Yes, they give identity and fit their land, and they read well after "the" (F7 for the faults). Nothing is modern except perhaps "Teamstead"-like accidents, which are settlements, not regions. Nothing is too grand for its size. Near-duplicates appear at 2048 and in forest-heavy worlds. The mix is wrong in one way: the plain names land on the largest and most contested regions, and small maps keep 84% plain.
2. **People and god names.** Clean of slurs in this set. About 25 comic or accidental names remain (F6), of which `Suckford`, `Shiety Glade`, `Realdeadstead`, "The Timhack League" and the gods `Flopfia`, `Gordman` and `Grammam` are worth adding to the lists. "The House of …" works. Gods mostly read as gods; the faerie and human patterns are the weak ones. The goblin pool is over-filtered by the English-word list, as the owner has already decided.
3. **Dwarves.** Their sites and fortresses read as dwarven on the map in 13 of 15 realms in the set. Fewer settlements is fine. Their silence in history and the residual lowland realms (seed 7) are the issues (F5).
4. **Faeries.** Right biome, wrong place within it: on the shore (F4).
5. **Log.** Better: fewer famines and spreads, crises only when they split, eras merged. The long eras read well. The short and identical ones do not (F3). Plagues in the summary are the ones that matter, but their sentences undersell them (F1). The overall balance is P3-07's (F8).
6. **Abandonments.** The chronicle has a reason in its causes; the summary does not (F2).
7. **Wording.** See F9. Every "0 fled" and "0 generations" is gone.
8. **Earlier follow-ups.** See section 6.

## 6. The earlier reviews' follow-ups

| Earlier finding | Status at 0.17.0 |
| --- | --- |
| P3-05 F1 dwarves on lowland | **Mostly resolved.** 135 of 144 realms are upland. Seed 7's Clan Nirgurn and sea-level "Deeps" remain (F5). |
| P3-05 F2 mechanical, back-loaded log | **Improved.** Mechanical share is 44% → 30%; famines per settlement nearly halved. Still back-loaded (F8); identical era blocks (F3); elves still the second most famine-struck people (0.61 per settlement, humans 0.21). |
| P3-05 F3 salience | **Resolved for crises** (10 → 1 per summary). **Partly for plagues**: share is down, but the sentence contradicts the rank (F1). |
| P3-05 F4 unmotivated abandonment | **Partly.** There is now a destination and a second cause. The reason is not in the sentence (F2). |
| P3-05 F5 names | **Mostly resolved.** Slurs gone, realm form for elves, god style. Same-core pairs remain; a short list of comic names remains (F6). |
| P3-05 F6 faeries not hidden | **Half.** Forest yes, coast still (F4). |
| P3-05 F7 cities and dwindles without cause | **Partly.** Most dwindles now cite a cause, some odd (F9); 15 without. |
| P3-05 F10 secession always 4 settlements | **Resolved** (3, 4, 6, 8 in the set). |
| P3-05 (i) last settlement abandoned by chance | **Resolved** (0 in the sweep). |
| P3-05 (ii) refugees above cap | **Resolved** (1 at default, draining). |
| P3-06 F8 "0 fled", victors losing more | **Resolved.** Small-battle "0 of the victors" is new (F9). |
| P3-06 F10 names in war sentences | **Resolved** for every cited name. |
| P3-06 F4 flare-up names no one | **Remains** (F9). |
| P3-06 F1, F2, F3, F5, F7, F9 | P3-09 or later; unchanged in kind (F10). |

**Made worse by this change:**

- F1: the plague rank and its sentence disagree, which follows from the total-toll rule.
- The upland dwarves' silence (F5) is a consequence of moving them out of harm's way.
- Small worlds lost the numerals but gained almost no names.

None is a regression in correctness.

## 7. Suggestions (rules and parameters)

1. **Plague sentence carries the total** (F1): a value for the outbreak's planned total and spread count. The template becomes "Plague broke out in X and spread to N settlements; T died in all."
2. **Abandonment sentence carries its cause** (F2): a leading clause from the first cause's kind ("Weakened by plague", "After famine and plague"). Or, at P3-07, a rule that a shown abandonment brings its causes.
3. **Quiet eras** (F3): one world-level line per quiet row, or none for rows shorter than 100 years. Use a per-culture noun for rulers.
4. **Faerie placement** (F4): faerie `port` 0 or negative, plus a forage or interior cell term through `cell_weights`. Check afterwards that fewer than 20% of faerie settlements stand within 300 m of the sea.
5. **Dwarven placement** (F5): dwarven `port` 0. Discount lowland capacity for dwarves. Make the fortress rule require a non-walkable cell, or rank ≥ 0.5, so that coastal forest sites are not "Deeps".
6. **Names** (F6): add the listed roots to `crude` or `words` (suck, shiet/shit- spellings, dead within a human compound, tim, hack, team, teck, burd, gord, gram, mamm, flop, whop). Shorten `words` as decided, keeping goblin-apt words. Add a per-world rule that no two settlement or realm cores are one letter apart (P3-05 F5). Lengthen the human and faerie god patterns, or give them their own syllable sets.
7. **Region names** (F7):
   - plain names only for unique base names, or draw for every base that repeats three or more times;
   - one use per descriptive stem per world;
   - a gloom cap of about one in five drawn names;
   - remove or reduce the church words;
   - later, renaming by history (design 4).
8. **Renderer** (F9): thousands separators; the three-word form for `winner_dead`; plural agreement for "The Warrens"; the flare-up names its parties.
9. **Dwarven and faerie agency** (F5, F10): include them in P3-09's revenge claims. In P5, give upland dwarves their own event kinds (deep delving, a lost hold, a mine opened), so that a mountain realm has history without having to be attacked.

Proposals for `docs/design/proposals.md` (design-level; the lead adds them, since I do not change the repository):

- "Regions are renamed by history: a region's defining event (a sack, a great battle, a plague) can give it its present-day name, so that the land's names record its past."
- "Mountain peoples have history of their own: delvings, lost holds and mines as events, not only wars brought to them."

## 8. Blocking and follow-ups

**Blocking the merge:** none.

**Follow-ups, in priority order:**

1. Plague sentence with the total toll (F1; suggestion 1). Small, and it affects every summary.
2. Abandonment sentence with its cause (F2; suggestion 2).
3. Name list additions and the shortened `words` list, before any world is shown outside the project (F6; suggestion 6).
4. Quiet-era blocks: one line per quiet row, or a minimum length (F3; suggestion 3). This matters most at four civilizations per race.
5. Faeries off the shore (F4; suggestion 4), together with P3-09's faerie survival work.
6. Dwarves: port and lowland capacity, sea-level fortresses (F5; suggestion 5), and dwarven agency in P3-09 or P5 (suggestion 9).
7. Region name mix, stems, gloom cap and church words (F7; suggestion 7).
8. Renderer wording (F9; suggestion 8).
