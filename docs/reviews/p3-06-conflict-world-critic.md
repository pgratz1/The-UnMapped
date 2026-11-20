# World critic review: P3-06 expansion and conflict (generator 0.16.0)

Date 2026-11-20. Material: the 13 worlds in `~/.cache/the-unmapped/out/p3-06-validation/critic/`. They are seeds 42, 0, 1, 2, 3, 7, 11 and 23 at 1024; seed 42 `turbulent`; seed 42 `--civs-per-race 4`; seeds 42 and 5 at 512; and seed 42 at 2048. For each I read the summary and the chronicle, and the map and region images where they mattered. I also ran read-only queries on the 375 sweep worlds in `sweep/` (100 default 1024 worlds, 30 at 512, 20 each for `turbulent`, `civs4`, `span12000`, `dense` and `calm`). The query scripts are in my session scratchpad. They only read the `.db` files.

Note on names: a few generated names look like slurs. In this review I mask the offending syllable with an asterisk (for example `N*gsnib Pit`).

## 1. Verdict

**Acceptable with follow-ups.** Conflict changes the history from a gazetteer into a chronicle with real causes and real ruins: the claim → war → sack → moved capital → cession chains read well, and they reach back thousands of years across peoples. Nothing here should block the merge. But the same mechanism repeats too often, and in a fifth to a half of the wars it changes nothing: the same goblin horde fights the same elven region up to 17 times, attackers "win" and gain nothing, and last-refuge villages hold out against 30 to 40 sieges. Two follow-ups should land before Paul's P3 review (section 7, items 1 and 2).

## 2. What works

- **Wars have causes and consequences, and the chains are long.** In P3-05 every causal chain ended inside one people and was at most 4 deep. Now the longest chain ending in a sack is 16 to 44 events deep in every 1024 and 512 world, and spans up to 5,945 years. Uncaused events fell from 63–66% of the log to 27–53%. Seed 42 is a model case:
  - `2148 e38` The Grork Horde laid claim to Southwest Wood (cause: Finen's colony Glathenddell, e25, 1475);
  - nine wars follow between 2220 and 5876;
  - `4274 e233` war over West Wood III → `4284 e240` "The Grork Horde sacked Voeledell of Finen; 407 died and 950 fled." Voeledell is Finen's first settlement, founded in year 0 (e3) → `e241` capital moved to Rannve → `4287 e246` "Finen ceded West Wood III to The Grork Horde."

  A player can stand in the ruin of Voeledell and the log explains it back to year 0.
- **The fall of an elder realm is the strongest story in the set** (seed 42, Finen). Its seat moves six times: Voeledell (sacked 4284), Rannve (5299), Ithithe (5621), then Nienann, Lafeco and Ithimaesdell, three capitals sacked in three years (`5873 e931`, `5875 e936`, `5876 e941`). It ends as one town, Noviemedell, with 1,544 elves and seven ruins around it. This is the dark high fantasy the design asks for.
- **The P3-05 critic's wishes (iv) 1–3 are met.** There are endings and ruins: 91% of ruins now come from sacks, and 70 of 100 default worlds end at least one civilization. There are cross-civilization causes. The goblin–elf enmity row exists from the goblins' founding year in every world.
- **Goblin infighting reads as goblin history.** Seed 42: the Warrens of Dribnark secede (e283, 4392), lose Sneezskrub Hole to the dwarves (e342), are cut down by their parent horde (e374, e378) and fall (`4858 e379`). Then the Zak-Neez Mob does the same (e436 → e706).
- **Shelter years and the per-civilization runaway guard work.** No civilization in the 13 worlds holds more than 15 of about 50 regions at the end (seed 11, Kingdom of Wurdrom).
- **Turbulent is now darker, not only busier.** This was P3-05 F8. The sweep medians, turbulent against default: sacked 16.5 against 12, civilizations ended 1.5 against 1.
- **Same-year relation rows read naturally.** Seed 42 `civs4`, year 5907: "The Warrens of Gnunk declared war on Finen" (e1901), and in the same year "Finen fell to The Warrens of Gutch-Sluz" (e1905). Year 5625: the Warrens of Skabug sue one horde for peace (e1477) and fall to another (e1481) in the same year. Both read as a realm torn apart, not as a bug.

## 3. Findings, most serious first

### F1. Futile, repeated wars: the same pair, the same region, no outcome (rule)

The war's goal is usually the claimed border region. When that region holds the defender's capital it can never be ceded (7.7), and when the defender's last settlement is a last refuge it can never fall (7.3). The attacker then wins on battle score and goes home with nothing, and after the 200-year truce the same claim comes back.

- Default sweep, 100 worlds: **1,129 of 2,667 wars (42%) have no sack, capture, cession or conquest**. Of the 1,393 peaces won by the attacker, **416 (30%) gained nothing**. At 512 the figures are 472 of 844 wars (56%) and 236 of 467 victories (51%).
- The busiest (defender, goal region) pair per world has a median of 6 wars at default, 9 at 512 and 10.5 with `civs4`. The highest are 17 at default (s63, The Dodgorin Throne, Central Peaks), 35 with `turbulent` (s7, The Realm of Ienngleth, Central Wood) and 25 with `span12000` (s3, Vefal, Northeast Wood).
- In the critic worlds:
  - seed 42: 8 wars on Finen over West Wood II;
  - seed 5 at 512: 15 wars on Vamiel over West Downs;
  - seed 42 `turbulent`: 18 wars on Finen over Southwest Wood;
  - seed 2: 9 wars on the Court of Ienriend over Northwest Wood III.
- How it reads at summary level, seed 42 at 512, The Beepo Ring (faeries): ten wars over South Wood between 4418 and 5995. Each is "declared war on The Beepo Ring over South Wood" followed by "The Beepo Ring sued X for peace after N years of war" (e315, e376, e590, e596, e616, e779, e790, e836, e841), with nothing ceded, because South Wood is the Ring's capital region. "The Beepo Ring sued The Grork Horde for peace after 40 years of war" (`5669 e596`): a 40-year war with 12 battles, and the loser loses nothing.
- Seed 42 at 1024: `4844 e356` → `4849 e364` "Finen sued The Grork Horde for peace after 5 years of war": 3 battles and no other effect. The same happens at e443 → e452 against the Teeshti Court, and e795 → e824.

This is the main reason the war log reads as noise in places. A defeat has to cost the loser something.

### F2. The last-refuge guard makes villages invulnerable, and it is used constantly (rule; your question 3)

- 3,068 sieges of a last refuge across the sweep, and none fell. The most-besieged settlement that never fell:
  - Saen (seed 42 at 512), a faerie village of 843: **32 sieges by four peoples in 352 years** (e557 at 5643 to e840 at 5995), every one "it held";
  - Thellraedell (turbulent s7), the elves' last town: **40 sieges from 4655 to 5965**;
  - Shaetwoli Glade (`civs4` s12): 45 sieges;
  - Baewou Hollow (default s59): 32 sieges.
- Sieges are hidden at summary level (salience 25), so the summary shows only F1's empty wars. At the `full` level, and in any lore drawn from the log, a hamlet that has held for three centuries against every horde is absurd unless something explains it.

What would read better, in order of preference:

1. **Flight instead of invulnerability.** Let the last refuge fall and be sacked as usual. Its survivors then found a hidden refugee hamlet in the free region within reach that is furthest from the attacker (a `refugees_settled` event caused by the sack), so the race lives on as fugitives. The refuge becomes a real ruin with a cause, and the new hamlet is a quest hook. Keep today's guard only as the fallback when no free region is in reach.
2. **No claim on a last refuge.** A civilization whose only settlement is its race's last is not a valid claim or declaration target. Its strain still counts toward tension, so the flare-up picks another pair. This removes the futile wars rather than explaining them.
3. If the guard stays, make it legible and rare. Write one event per war, "X could not take Saen", not one per siege. Count failed sieges toward the attacker's exhaustion. Give a pair that has fought a war over a last refuge a much longer truce.

Option 1 fits design 5 best: faeries are "hidden communities in wild places".

### F3. One people starts the wars; the others never act (rule and data; your question 2)

- Attackers in the 100 default worlds: goblins 2,163 of 2,667 (81%), orcs 286, humans 145, dwarves 73, and elves and faeries **0**. In seed 42 the goblins start 34 of 38 wars, and with `civs4` 78 of 85.
- **Elves never strike back.** Finen loses its capital six times in seed 42 and never declares a war. Old wars leave grudges (design 5), but the grudge enters only through drive, and drive is multiplied by aggression squared (elves 22, faeries 2). So grudges shape nothing for the peoples that carry the oldest ones. The best story the log could tell is an elven war to retake a sacked city, and it never happens.
- **Orcs lose.** Of their wars that ended in a peace, orcs won 55 and lost 169 (default sweep). Goblins won 1,204 and lost 348, humans 98 and 12, dwarves 36 and 15. With `declare_ratio` × (1000 − 900), orcs declare against peoples ten times their strength. A young horde of about 9,000 (seed 42, The Daald Horde) attacks the Duchy of Dant and Finen and loses twice (e721, e746) before it wins twice.
- **The orc age is still the goblins' age.** In the orc age goblins declare 958 of 1,429 wars (67%) and orcs 286 (20%). Design 5 says the orcs' "coming opens the most recent age of conflict". In the log they are a minor party in their own age.
- **Orcs are almost never attacked:** 4 of 2,667 wars have an orc defender.

Is it monotonous? Yes. Seed 3's summary is a 2,500-year loop of "The Sneegg Horde declared war on Roelaele / Vefal; sacked one village; Roelaele / Vefal sued for peace after 1–7 years" (e139 to e594, 14 wars). Each line is fine; together they read as a timer. The balance is right in direction (goblins the elves' lasting enemy) but wrong in degree. Suggestions are in section 6, items 3 and 4.

### F4. The summary is a war log, and at four civilizations per race it loses war endings (data and P3-07; your question 6)

- Conflict and capital moves are 555‰ of the summary at the median default world (275–910), 790‰ with `turbulent` and 842‰ with `civs4`. Seed 42's summary (threshold 46) has 130 conflict lines of 155. No settlement founding, city or plague survives. The Age of Elves, 2,074 years, gets 3 summary lines (median 3 across the default sweep).
- With `civs4` (seed 42, threshold 56), the summary shows 85 declarations, 54 peaces, 36 sacks, 27 captures and 13 conquests. It hides:
  - 12 truces;
  - 4 cessions;
  - 32 moved capitals;
  - 5 flare-ups;
  - all 107 claims.

  So 12 wars visibly never end, and capitals are sacked with no word of where the seat went.
- The flare-up sentence names no one: `2949 e93` "In 2949 tensions flared into 50 years of upheaval." With conflict, its trigger is always a claim (5.4), so it could say whose.
- At threshold 31 (seed 3) the claims are shown, and 34 claim lines plus 62 plague lines (P3-05 F3) crowd it. A claim followed within a few years by its war is redundant at summary level.

Not blocking, since P3-07 is not implemented yet. But the P3-07 selection rule should change before it is: see section 6, item 2.

### F5. Faeries are reduced too far, elves in the tail too far (balance; your question 4)

- **Faeries** end with a single settlement in **26 of 100 default worlds** (37 with two or fewer), 19 of 30 at 512 and 11 of 20 with `civs4`. In the critic set they have 1 settlement in seeds 3, 11, 23, 42 `turbulent`, `civs4` and both 512 worlds. With the guard, that one settlement is usually the besieged village of F2. A people "hidden in wild places" (design 5) that is besieged dozens of times is not hidden.
- **Elves**: a median of 10 standing at default is about right for the tone. A fading elder people, its oldest cities in ruins, is the right present day. Seed 42 (one realm at its end, one island realm untouched since year 1250) is an excellent spread. Elves are too reduced in the tail: 1 settlement in 7 of 100 default worlds and in 7 of 30 at 512 (seed 42 at 512: 1 standing, 20 elven ruins, capital moved 11 times). With `civs4`, 32 of 85 elven realms are conquered, including Finen in seed 42 at 5907 (e1905). The count matters less than the shape: every loss comes from the same horde and the same goal (F1), and the elves never answer (F3).
- **Humans** end with one settlement in 6 of 100 default worlds. That is odd for the fast-growing late people "spreading over land left open". Elsewhere they are the largest people in 51 of 100 worlds.

### F6. Race-level runaway goes unmeasured (your question 5)

No world in the set reads as one civilization having won: the per-civilization guard works. A race can win, though, because goblin hordes split by secession and each piece stays under the fear share.

- Seed 42 `civs4`: goblins hold 32 of 39 held regions and 88% of all people. Humans are reduced to 1 settlement after the Duchy of Breld and the Ashas League are conquered (5244, 5245). Elves have 4, faeries 1, dwarves 5.
- Default sweep: the most populous race has ≥ 60% of all people in 38 of 100 worlds (humans 51 times, goblins 44).

Two peoples owning the present (men and goblins, the elder races fading) is a good setting. A goblin world in which every other people is a refuge is dull, because the losers are too small to be places worth finding. The sweep statistics of 15.5 should add the largest race share alongside the largest civilization share.

### F7. Long peace in a third of worlds; the goblin–elf feud missing in a fifth

- Seed 11 has no claim between the goblin awakening (2070) and 4466, and no war until 5041. The Ages of Goblins, Dwarves and Faeries have 0 wars, and goblins and elves never fight.
- Default sweep: the first war of any kind comes a median of 838 years after the goblins awaken, and more than 1,000 years later in 46 of 100 worlds. The longest stretch without a declaration after the goblins awaken is a median of 1,145 years, ≥ 1,500 years in 31 of 100, and 3,528 at most. There is no goblin–elf war at all in 19 of 100 (validation 4.2).

Quiet intervals are by design. But "hostile to the elves from the first year" (design 5) is only a relation row in those worlds.

### F8. Wording and numbers

- **"0 fled"**: 797 of 821 captures in the default sweep (97%), and 107 of 108 in the critic worlds, end "…; 135 died and 0 fled." (seed 42 e318).
- **Victors lose more than the defeated** in 3,934 of 9,450 battles (42%). An example is `civs4` e1902: "The Warrens of Gutch-Sluz defeated Finen in West Wood III; 38 of the defeated and 157 of the victors fell." The cause is that losses are a share of each side's own force. When one side is three times the other, the winner's 8% exceeds the loser's 25%. Battles are hidden at summary level, but the numbers will feed lore.
- **Repeated wars have no ordinal**: "the third war over West Wood II" would turn F1's repetition into a feud the reader can follow (P3-07 or P5 wording).
- **Passes** are almost never fought over: 55 of 9,450 battles (0.6%), and none in 12 of the 13 critic worlds (8 in seed 5 at 512). This is mostly geography, since seed 42 has one pass, but design 5's "places to fortify and fight over" is not yet visible.

### F9. Small oddities (your question 6)

- **Captured, then ceded back in the same step**: one case in 375 worlds (span 12000, seed 3, 11253: c10 retakes Aleckstead, e2058, sues for peace, e2059, and cedes the region, e2060). None in the default sweep or the critic worlds. It reads as "won the battle, lost the war", which is plausible. Leave it.
- **Ping-pong captures** (a settlement retaken by its previous owner within 60 years): 4 in 100 default worlds. Fine.
- **Rows that open and close in the same year**: only in `civs4` here (seed 42: war c5→c1 5907, e1901/e1905; peace c6–c16 5625, e1477/e1481). They read well (section 2). The P3-07 war line should say "…before a battle was fought" for a war that ends at the defender's conquest in its declaration year.
- **Map size changes the tone more than any setting.** Seed 42 at 2048 has 7 wars, no goblin–elf war and no elven ruin, so it is nearly peaceful. At 512, the elves are down to one settlement and the faeries live under constant siege. The design's 32 km world would be the gentlest. Scaling `civs_per_race` (auto) or `march_hops` with map size would keep the tone constant.

### F10. Names in war sentences (your question 7)

Conflict puts names into sentences of violence, which makes the P3-05 F5 blocklist more urgent. Found in the 13 worlds, with slur-like syllables masked:

- **At summary level**:
  - seed 42 `5769 e808` "The Kingdom of Stard sacked Krob-Sn*gg of The Warrens of Kug-Nuk" (humans sacking a goblin town whose name ends in a slur stem);
  - seed 42 `turbulent` `5846 e1419` "The Zak-Neez Mob took Kreets*ut from The Skrax-Snix Mob";
  - seed 11 `5910 e723` "…took Gn*zitch Hole…" (embeds "nazi");
  - `civs4` `5648 e1554` "Finen sacked Dork of The Warband of Azam" (comic).
- **In chronicles only**:
  - goblin `N*gsnib Pit` (seed 2, again; already in the P3-05 review), `Drin*gg` (seed 1), `Guz-Gn*gg`, `Nik-Gn*gg`, `Gn*gsneet Pit`, `S*utkeeb` (seed 5 at 512);
  - human `Turdstead` (seed 1), `Cockford` (seed 2), `Cockton` (seed 3).
- The goblin syllable set produces the "-n*gg" ending again and again (7 names in 13 worlds). A substring blocklist over the whole name, not only at word starts, is needed before any world is shown outside the project.
- Bare elven realm names ("Finen", "Vefal", "Roelaele") are less confusing in war sentences than in P3-05's founding lines ("Finen sued The Grork Horde for peace" reads fine), but "Finen sacked Groz of The Grork Horde" still reads as a town sacking a town. The scheduled realm form helps.

## 4. Patterns across seeds (counts)

| Pattern | Count |
| --- | --- |
| Wars with no sack, capture, cession or conquest | 1,129 of 2,667 (default); 472 of 844 (512) |
| Attacker victories that gained nothing | 416 of 1,393 (default); 236 of 467 (512) |
| Wars over the same (defender, region), the most per world | median 6 (default), 9 (512), 10.5 (`civs4`); max 17, 35 (`turbulent`) |
| Sieges of a last refuge, fallen | 0 of 3,068 (all sets) |
| Most failed sieges of one settlement | median 6 per default world; 32 (s59 default), 40 (`turbulent` s7), 45 (`civs4` s12) |
| Attackers: goblin / orc / human / dwarf / elf / faerie | 81% / 11% / 5% / 3% / 0 / 0 (default) |
| Orc wars ended by peace: won / lost | 55 / 169 |
| Wars with an orc defender | 4 of 2,667 |
| Goblin share of wars in the orc age | 958 of 1,429 |
| Faeries end with one settlement | 26 of 100 (default), 19 of 30 (512), 11 of 20 (`civs4`) |
| Elves end with one settlement | 7 of 100, 7 of 30 (512) |
| Humans end with one settlement | 6 of 100 |
| Most populous race ≥ 60% of all people | 38 of 100 |
| No war in ≥ 1,500 consecutive years after the goblins awaken | 31 of 100 |
| Conflict share of the summary | median 555‰ (default), 790‰ (`turbulent`), 842‰ (`civs4`) |
| Captures that say "0 fled" | 797 of 821 |
| Battles where the victors lost more | 3,934 of 9,450 |
| Battles at a pass | 55 of 9,450 |
| Longest causal chain to a sack | 16–44 events in every 1024 and 512 critic world (P3-05: ≤ 4) |

## 5. The P3-05 follow-ups: better or worse with conflict

| P3-05 finding | With conflict |
| --- | --- |
| F1 dwarves on lowland | **More urgent.** Dwarven forest holds are attacked as lowland: the Hold of Dulimin (South Wood II) faces 10 wars in seed 42. Upland sites would give dwarves the rough-ground and pass defence that 6.3 already offers. |
| F2 mechanical, back-loaded log | **Worse in timing, better in content.** Sieges and battles add mechanical lines at `full` level (78 sieges in seed 42), and the summary per age is 3, 8, 12, 28, 30, 74 (seed 42). But war lines now carry consequences, which famines did not. |
| F3 salience | **Mixed.** A threshold of 46 pushes plagues and crises out (seed 42); at 31 they still crowd (seed 3: 62 plagues of 230). Claims (45) add a new crowd. |
| F4 unmotivated abandonment | **Better.** 91% of ruins are sacks with full causes. |
| F5 names | **Worse**: names now stand in violent sentences (F10). |
| F6 race character | Humans boxed in (seeds 1, 2, 7): **better**, since humans now lead in population in 51 of 100. Faeries not hidden: **worse**, since their open-ground villages are now siege targets (F5). |
| F7 static populations, flickering cities | **Better**: sacks, captures and refugees move the numbers. |
| F8 turbulent not darker | **Better** (section 2). |
| F9 short, local causes | **Fixed** (section 2). |
| (i) last-settlement abandonment | Unchanged; conflict's last-refuge guard is the stronger version of the same question (F2). |

## 6. Suggestions (rules and parameters)

1. **Make every war cost the loser something** (F1).
   - (a) Choose the goal among the defender's regions that are not its capital region, unless the claimant can reach and take the capital. Otherwise make the war one of plunder.
   - (b) A won war with no cession pays tribute: a new event `tribute` with a value (people or wealth taken from the loser's settlements), or a cession of the loser's best non-capital region in reach.
   - (c) Lengthen the truce, or decay the strain, for a pair that has fought N wars over the same goal: "war weariness", a per-pair count stored with the grudge.
2. **P3-07 summary selection** (F4), as an amendment before P3-07 is implemented:
   - (a) a shown `war_declared` always brings its ending (peace, truce, cession, conquest) and any `capital_moved` caused by a shown event;
   - (b) a per-age floor, so each age shows at least its share (for example 20 events);
   - (c) lower `claim` to 35 and add the claim year to the declaration sentence ("over Southwest Wood, claimed since 2148");
   - (d) the `flare_up` template names the claimant, the holder and the region.
3. **Grudges that act** (F3). A civilization that lost a settlement or region by capture, sack or cession holds a revenge claim on it. Its strain grows from the grudge, at a rate floored at, for example, `grudge` × `revenge_permille`, independent of aggression. Elves would then retake ruins and dwarves avenge their holds.
4. **Orcs** (F3). Either a per-race levy factor (orcs and goblins mobilize, for example, 250‰ of a settlement rather than 100‰), or a floor on the odds term of 7.2, so that a people does not declare against more than about three times its force. Measure afterwards that orcs win at least as often as they lose and start at least a third of the orc age's wars.
5. **Last refuge** (F2): flight into hiding, as described there. Data: a `refuge_flight` switch and a minimum distance.
6. **Faeries hidden** (F5). With the scheduled forest weights, add a race-specific cover term: settlements of a race in its preferred biomes get extra `terrain_defense`, and faerie settlements in fen, marsh or wet woodland cannot be the siege target of an army entering from a non-wild region.
7. **Wording** (F8):
   - drop "and 0 fled" when `fled` = 0 (a template condition);
   - compute battle losses from the smaller force: dead_W = min(F_W, F_L) × `loss_winner`;
   - give repeated wars between a pair an ordinal in P3-07.
8. **Sweep statistics** (F6): add the largest race share of people and of regions, and the share of hollow wars, to 15.5 and to the validation sweep. Suggested typical ranges: hollow wars below 25%, and at most 3 wars per (defender, goal) without a change of owner.
9. **Map-size scaling** (F9): let `civs_per_race` auto or `march_hops` follow map size, so that 512 is not a massacre and 2048 is not a peace.
10. **Early feud** (F7): the spec's question 10 alternative, an awakening that raises the strain of every hostile pair, or `march_hops` 3 for pairs with an `enmity` row, so that the goblin–elf feud exists in nearly every world.

Proposals for `docs/design/proposals.md` (design-level; I do not change the repository, so the lead adds them):

- "History: a people's last refuge falls into flight and hiding rather than being invulnerable."
- "History: lost land leaves revenge claims that act regardless of a race's aggression."
- "History: a won war always costs the loser (land, tribute or a longer truce)."
- "History: runaway is also measured per race, not only per civilization."

## 7. Blocking and follow-up

**Blocking the merge:** none. The stage does what its spec says; the problems are balance and rules, and every fix changes output under a later version.

**Follow-ups, in priority order:**

1. Futile repeated wars and the invulnerable last refuge (F1, F2; suggestions 1 and 5). Before Paul's P3 review.
2. P3-07 summary selection keeps war endings, the per-age floor and claim and flare-up wording (F4; suggestion 2), as a P3-07 spec amendment before it is implemented.
3. Name blocklist over whole names, including "-n*gg" and the other masked forms (F10; P3-05 F5, already scheduled). Before any world is shown outside the project.
4. Agency for the other peoples: revenge claims, and orcs that win their age (F3; suggestions 3 and 4).
5. Faeries hidden and less reduced (F5; suggestion 6), together with the scheduled faerie forest weights and P3-05 F1's dwarven upland sites.
6. Wording: "0 fled", battle losses, war ordinals (F8; suggestion 7).
7. Race-level runaway and hollow-war statistics in the sweep (F6; suggestion 8).
8. Early feud and long peace (F7; suggestion 10) and map-size scaling (F9; suggestion 9).
