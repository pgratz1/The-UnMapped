# P5-02 beasts and monsters: world critic review

Generator 0.23.0. Material: `~/.cache/the-unmapped/out/p5-02-validation/` (`critic/`: 12 default and 4 high-pressure worlds at 1024, summary and full; `sweep/default`: 100 worlds, full level, counted by grep). Summary reports read in full or in their monster parts: default s0 to s11 and high s0 to s3; full level read for monster parts in default s0, s8, high s3, and sweep s17.

## Verdict

Ready to merge. Nothing blocks. The monster stories read as causes and effects, the prose is clean, and the abandoned places are a real gain. Three things are weak enough to want a change soon (F1 to F3): raids are mechanical and can repeat for centuries without a response, the summary chronicle drops the whole monster story in busy worlds, and abandonment is always a tidy relocation.

## What works

- Chains read well. Default s0: a troll lairs in the Holt of Yews (e8, 525), the elves found Fievos nearby, the troll raids it and drives the people out the same year (e28, e29, 2071), the region is renamed "the Haunt of Beegsleerk" (e30), the elves hunt the troll down 49 years later in that region (e48, 2120). Reading from cause to present-day ruin is possible.
- Undead chain, high s3: Whinrigg Downs becomes "the Weeping Downs" after a slaughter (e690, 5807); the dead of the Weeping Downs rise as the Restless of Heald (e710, 5848); a horde slays them (e728). Renaming and the rising feed each other. This is the dark tone the design wants.
- The five-way ending of settlements (sack 1011, overrun 196, plague 166, famine 72 across the 100 default worlds' present-day ruins) gives a mixed ruin list; `[present]` ruins lines carry a cause and an event id (default s8 `s76 Zuuld Fort ... e548 overrun by The Restless of Ashont (m8)`).
- A dragon that lairs in a taken town and outlives it: default s8, Andithond the Black took Snokdrazz (e198, 3342); the ruin faded from memory (e344, 4845); the lair `si9` is still open at the end. A good place to find.
- Dragons stay asleep (57 of 67 end asleep, 9 slain, 1 awake): they feel like old-world threats, not wandering pests.
- No grammar slips found: grep over 100 worlds for "the the", "a ogre", "an troll", doubled spaces, and `s's` gave nothing; possessives after s are right ("Niemes' Shadow"); numbers one to nine are words.
- Region names fit the lair: "the Haunt of Beegsleerk", "Zug's Shadow", "the Forsaken Wood" read well.
- High pressure differs by count (raids per world median 86 against 39.5; monsters 11 against 8; ravage renamings 2.1 per world against 1.45).

## Findings

### F1. Raids are mechanical and can go on for centuries with no response

Evidence:
- Sweep s17 (default): Thound, a giant, raided the fortress Tourn 50 times (3383 to 5043; `e238` ... `e518`). The monster record says `raids 51, killed 3707`. The first reply is `e519` "Clan Thrombar drove Thound back" in 5047, 1,870 years after the lair `e193` (3178). Tourn is also named in famine `e509` and plague `e694`, but never abandoned.
- Default s0, Skrig-Snaz (`e186`, `e194`, `e214` to `e237`): ten raids on seven settlements in 4205 to 4257, nine of them "60 died" with `plunder=8` exactly. The Restless of Cear (`e540` to `e588`): `plunder=8` on five of eight raids, "200 died" twice.
- Across the 100 default worlds, 4,557 raids; the most common toll/plunder pairs are `dead=50 plunder=2` (63), `dead=60 plunder=4` (51), `dead=60 plunder=6` (48), `dead=40 plunder=6` (47). Tolls clamp at per-kind caps (40, 50, 60, 150, 200), so the numbers read as flat.
- Raids per world: min 2, median 41, p90 73, max 151. Same pair hit repeatedly: s17 Tourn 50, s87 Kruz-Nork 30, s45 Wantham 30, s16 Wildbury 29.
- Every raid cites the monster's first event as its cause (`[causes e193]`, `[causes e8]`), never the previous raid or any wake/response. 4,330 of 4,557 raid lines are the same sentence form: "X fell upon Y; N died."
- A raid is 5.4 percent of all events (sample: 4,357 of 80,207), and none appear in the summary chronicle in 14 of 16 reports.

Why it matters: a place raided fifty times with no flight, fortification or hunt is not a causal story; the repeats look like a timer. The reader meets "fell upon" lines in runs of three to ten.

Direction: let a raid cite the last raid on that settlement (or the last response) as cause; after the second or third raid on one settlement within a short time, force a consequence (hunt, tribute, a decline, an outpost raised, or abandonment) with a minimum of one per N raids; scale tolls to the target's population instead of per-kind constants (round to tens only above 100); cap per-pair raids per century; vary the verb by kind (wyverns "stooped on", dead "came out of the barrows to") and let plunder be absent for undead and wyverns.

### F2. The summary drops the monster story in busy worlds

Evidence:
- Default s1: threshold 56, 1,124 events. The summary shows 2 `monster_appeared` and no death, no slaying, no rising. The Restless of Stead rose from the dead of Geackham's ashes (`e765`, 5736), raided 19 times, killed 1,465, overran Niethraedell (`e791`): none of it is in the chronicle; only `[monsters]` (`m6 ... raids 19, killed 1465`) and the ruin line show it. Its three siblings are missing too.
- Sweep: threshold above 45 (hides `monster_appeared`, priority 45) in 16 of 100 worlds (46 x7, 51 x2, 56 x1, 61 x3, 71 x3), above 50 (hides risings and overruns) in 9.
- Default s0 (threshold 20): the summary carries zero raid lines yet monster lines are 36 of 192 shown. So the summary tells that monsters came and went but not what they did; only overruns show. In 14 of 16 reports no raid appears.
- `[monsters]` in the summary lists only awake or asleep monsters (default s0: one line for 12 monsters, `m12 The Host of Aler ... raids 0`). Ended monsters, including the ones that did the damage (Old Beegsleerk, `raids 10, killed 227`), appear only at full level.

Direction: give a monster a floor priority at its first overrun, its rising, its slaying and its death-after-killing-N, so they survive a threshold up to 71; and list in the summary `[monsters]` the ended monsters with raids above some number (e.g. killed above 200), one line each. Without that, a busy world reads as if monsters were a footnote.

### F3. Abandonment is always an orderly move

Evidence: 309 `settlement_abandoned_to` in 100 default worlds, 308 with `lost=0`. Every line is "Weakened by plague/famine, X was abandoned; N survivors went to Y." (default s0 `e102`, `e311`, `e322`, `e385`, `e431`, `e515`, `e566`; six of eight present ruins in s0 are this kind). Survivors are 243 to 597, equal to or above what the plague killed.
- Abandoned places are only 16 percent of present ruins (238 of 1,445); 70 percent are sacks.
- The most distant ruin is from the 4000s: ruin years by thousand in the sweep, 4000s 319, 5000s 1,126, earlier 0. Ruins fade, so ancient ruins disappear from the present-day list (Snokdrazz, 3342 overrun, 4845 faded). The oldest places that can be discovered are about 2,000 years old; no ruin from the first ages of elves or goblins. Not a defect of this stage alone, but monsters worsen it: overrun lairs keep the place's name while the ruin is dropped.

Direction: allow some abandonment without a destination ("the last of them left in the night", `lost` above 0, "no one knew where they went"), and an abandonment cause of a monster (a settlement left after N raids, "abandoned for fear of X") so F1's repeat raids get an end; lengthen the fade of ruins from monster lairs, or keep a ruin whose lair is open; or add a "remembered ruin" site kind so ancient ruins stay in the sites list.

### F4. Monster naming is flat, and one name does double duty

- The epithet "the Glutton" is 68 of the 90 epithet uses on `monster_appeared` ogres, repeating inside a world (high s3: `e7` "Uukh'ghak the Glutton, an ogre" and `e8` "Uraz'gruurk the Glutton, an ogre" in the same age). "Old" is 130 of 373 trolls. A player will remember "a Glutton" and "an Old" as types, not as names. Dragons use only "the Black" (25) and "the Pale" (20) as epithets, or none.
- "Restless of X"/"Host of X": X is a bare name that is not tied to the place (default s0 `e536`: the dead of Wolf's Brows rose as "the Restless of Cear"; Cear is the duchy that held the area, never said). The dead take the name of a race-civilization, which is a nice thought but unreadable. Better a place name: "the Restless of Wolf's Brows".
- "the Hollow of Bird": sweep, one ravage rename (`e775`, `@rg10`, 5915) uses a plain English word as the monster's name. The name filter missed it. Check for common words in the stem list.
- Ravage names have three forms only: "the Haunt of X" 40, "the Hollow of X" 38, "the Forsaken <land>" 24, and 40+ "X's Shadow" (about 1 per world). They fit the tone, but "Haunt of" and "Hollow of" are interchangeable; a region named "the Hollow of Y" is also the name of two lair types ("Beegsleerk's Hollow" lair in "the Haunt of Beegsleerk"). Reader sees the same word three times in one line.

Direction: add 8 to 12 more epithets weighted per kind (the Gaunt, the Hungering, the Rust-Maned, of the Marsh) and forbid reuse within a world; name risings after the ruin's settlement name; extend the name filter with the word list.

### F5. Beasts are almost all wolves

`beasts_bold` events: 328 in 100 worlds, "the wolves" 272 (83 percent), giant spiders 53, bears 3. Boar and deer never turn bold. The full report lists the populations (`beasts: bears 56, boar 591, deer 2169, giant spiders 44, wolves 314`, default s0) but only wolves act. The line "travellers from X went in fear; N died on the roads" is the single template (default s0 `e31`, `e42`, `e50`, one to four dead). Low impact, cheap to vary: boar rampage on farms, bears in the uplands, a famine tie.

### F6. Hunters and monsters have a thin back and forth

- 317 slain, 126 driven off, 468 woke again (default sweep). A driven-off monster "woke again" and raids again; default s0 Skub-Slux woke twice (`e256`, `e306`) in 750 years with no raid line in the chronicle between. "Woke again" is the same sentence each time.
- Hunters: median 1,242 (min 14, max 6,041), so a slaying of an ogre by 14 people reads oddly beside "hunters=2201" for a troll. Hunting parties are unnamed; the slayer is a civilization, never a person or a god's champion. The gods do not appear in any monster event (check: no `monster_*` line names a god).
- Hunt delay is long (Thound 1,870 years, F1).

Direction: a named slayer or a company ("a company of 40 from Nife"), and a god's blessing or curse for one dragon or undead host per world to tie to the gods acting, which the design wants.

### F7. Pressure high is the same shape, only more of it

High (30 worlds) against default (100): overruns median 3.0 in both, living monster at the end 28 of 30 against 87 of 100, risings median 2.0 against 1.0. High-s0 has 12 monsters, 1 overrun, none alive at the end: a high-pressure world with a quiet present. The difference shows in raid counts (86 against 39.5), which F2 hides in the summary. A player picking "high" would not see the world differ. Direction: at high, raise overruns and open lairs, not only raid frequency (for example 2 more overruns, undead risings from a smaller slaughter).

### F8. Minor

- Timing: undead risings sit late: 127 of 172 after year 5500 across the sweep, 0 before 3000 (the numbers show a floor; bucket counts: 3000 x2, 3500 x1, 4000 x18, 4500 x5, 5000 x19, 5500 x127). That is the age of orcs, so undead are the last act in nearly every world. Fine for tone, but makes the world's present dense in risen dead and the first four ages undead-free. Consider a lower slaughter threshold early or a single ancient host from an elven or faerie ruin.
- Lairs at year 0 to 200 appear before any settlement exists (default s0 `e4` 76, `e6` 96): a monster arrives, its lair named, and nothing is done for 1,500 years. Fine, but a monster that sleeps 2,000 years with `raids 0` (m1 in default s0: lair 76, died 2568) is invisible; 5 of 12 monsters in s0 had zero raids and zero kills (`m1, m2, m4, m6, m8`).
- Default s7: only 10 sites and none open at the end (`sites: 10; open 0; lairs 8, outposts 2`), and default s9 has one outpost: a world with nothing for a player to find from this stage. Sites per world: 10 to 28 in the sample.
- Settlement kind of overrun targets: "a preference for small ones" is not visible; overruns hit towns of 1,837, 3,025 survivors (default s8 `e198`; sweep `e89`). That is fine as story ("a dragon takes a town") but contradicts the stage description. Verify the weighting.

## Patterns across seeds

- Monsters per world: default median 8 (max 14), high 11 (max 18); every world has at least one; seeds with 3 to 4 monsters exist (default s3, s4, s5).
- Overruns: median 3 per world (max 8); 15 of 100 worlds have none.
- Raids per world median 41; 98 of 100 worlds see a raid.
- In 9 of 12 default reports, the monster blocks in `[present]` and `[monsters]` agree with the chronicle; in the 3 busy ones (s1, s4, s5) the summary threshold removes the story (F2).
- Dragons: 67 across 65 worlds; 57 asleep at the end.

## What a player could do with the present-day state

Abandoned and lair sites are in good supply (typically 5 to 11 open sites): a haunted region renamed after its troll, an open lair on a faded town, an outpost lost to the dark. The strongest hooks are the asleep dragons with hoards (default s8 Andithond `hoard 314`, high s2 Thoni `hoard 603`, high s3 Modinkhond `hoard 814`, killed 3,798). Weak: the hoard is a number only, and the lair of a slain monster (si24 in s8, "slain" in ten years) leaves nothing.

## Suggestions, as rule or parameter changes

1. Raids cite the previous event on the same settlement and force a response after N repeats (F1); scale tolls by target population, vary verbs by kind.
2. Raise the priority of the first overrun, rising, and slaying so they survive a threshold up to 71; list ended monsters of note in the summary `[monsters]` (F2).
3. Add abandonment with no destination and abandonment for fear of a monster (F3); keep monster-lair ruins from fading while the lair is open.
4. Widen the epithet list and tie the undead name to the ruin (F4); add common-word names to the filter ("Bird").
5. More beast kinds acting; vary the beasts_bold sentence (F5).
6. A named slayer, and a god in at least one monster event per world (F6).
7. High pressure raises overruns and open lairs, not only raids (F7).
