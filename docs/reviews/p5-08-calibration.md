# P5-08 calibration (pantheon, interventions, cults; generator 0.30.0)

Engine `a76f579` (branch `p5/08-calib`); values in `game/data/` only, mirrored unchanged into the fixture roots `worldgen/tests/data/geo` and `geo-small` (byte copies of the shipped files, as in P5-07). No C++ change, no golden re-recorded. Scripts and tables: `~/.cache/the-unmapped/out/p508-calib/` (`scripts/sweep.sh`, `m10.py`, `agg10.py`; folders `base0`, `r1` to `r5`, `A2`, `r8*`, `FA`, `FB`, `Fm`, `Fd`, `Fhigh`, `Fciv`, `Fturb`). Eight tuning sweeps (base0, r1, r2, r3, A, r5, A2, r8 on seeds 0-49 or 0-99) and then the confirmation runs FA (seeds 0-99), FB (100-199) and the option sets, all at the final values. No generate or report error in any run. Measured from the world files (python `sqlite3`) and `report --history`; T16 = repeated battles (same participants and both tolls, kinds `battle`, `battle_at_pass`) per thousand battles.

## Values changed

`history/pantheon.toml`: `act_permille` 150 to **14**; `act_threshold` 100 to 70; `regard_fade_per_century` 20 to 8; `god_rest_years` 30 to 20; `pair_rest_years` 100 to 80 (the rests and the fade were what held meddling back: at `act_permille` 50 the ratio was 1.09; at 14 it is 1.9); `plenty_growth_permille` 300 to 550; `pestilence_toll_permille` 120 to 80 (G2 population); `send_hops` 2 to 3; `flare_regard` 200 to 130 (PN8: the flare never fired at 200); `cult_found_permille` 250 to 400; `cult_fear_permille` 500 to 700; `suppress_permille` 10 to 8; `hide_permille` 400 to 800 (CU5 was 4 worlds); creator `favour` {bless 500, champion 300, mark 200} to {400, 450, 150} (CU6).
`gods/`: `wrath.monster` of ruin 400, death 500, wild 500 all to 600 (PN4: monster acts were 2.3%); deeds: wild `colony_founded` -8 to -15, `mine_opened` -15 to -30, `monster_slain` -25 to -40; ruin `truce` -20 to -40; lore `tower_founded` 30 to 60, `book_written` 20 to 40; harvest `became_town` 10 to 20, `became_city` 15 to 30, `trade_opened` 10 to 20; oaths `alliance_formed` 15 to 30 (PN3: a good god acted in 74 worlds of 100); `favour` (bless, champion, mark): craft 600/200/200 to 500/350/150, lore 500/200/300 to 400/350/250, oaths 400/400/200 to 300/500/200, ruin 500/300/200 to 400/450/150.
`history/alliances.toml`: `renew_written_max` 3 to 1 (FU2). `history/monsters.toml`: `deep_warren_wake_permille` 200 to 300 (GW2 was 8%), `deep_stir_permille` 1000 to 700 (DP2; the stir stays on).
Tried and reverted: `figures.toml caster_ppm` 1500 to 2200 (CU6 58.5% in a 50-seed run) gave casters median 9 and books 12, outside FG7 (2-6, 3-10).
Untouched: `flare_permille`, `valour_permille`, `defeat_permille`, `cult_spread_permille`, `adopt_*`, `tower_lapse_years`, `deep_wake_ppm`, `figures.toml`, all P5-07 values.

## Targets and guards (cells: A = seeds 0-99 / B = 100-199; "0.30.0" = engine's shipped values on A)

| Target | Bound | 0.30.0 A | Final A / B |
| --- | --- | --- | --- |
| PN1 acts median; max | 8-25; <=60 | 8; 42 | 14; 28 / 13.5; 33 |
| PN2 meddling/normal; distant/normal; distant worlds with an act (seeds 0-49) | >=1.8; <=0.6; >=30 | - | 1.88 (22.5/12); 0.33 (4/12); 50 of 50 |
| PN3 worlds with an act of a good / neutral / evil god | >=80 each | 74 / 80 / 86 | 99 / 100 / 84 ; 96 / 100 / 85 |
| PN4 bless / curse / champion / monster / mark | each 5-45% | 17.9 / 43.2 / 15.3 / **2.3** / 21.4 | 16.8 / 29.6 / 18.7 / 15.1 / 19.9 ; 17.8 / 30.3 / 17.2 / 14.5 / 20.2 |
| PN5 worlds with a champion; share that fights | >=30; >=50% | 65; 32.1% | 92; **29.2%** / 90; **31.1%** |
| PN6 sent monsters alive at E (share of living) | <=20% | 2.2% | 13.8% / 15.1% |
| PN7 `divine_dead` median share of population | <=3% | 1.12% | 0.53% / 0.67% |
| PN8 flare-ups median (worlds with one) | <=3 | 0 (2) | 2 (87) / 2 (89) |
| CU1 cults median; worlds with a cult | 2-6; >=85 | 1; 62 | 3; 99 / 3; 99 |
| CU2 cults in 2+ realms | >=40% | 75% | 88.3% / 88.5% |
| CU3 worlds with a faith | >=50 | 56 | 97 / 99 |
| CU4 worlds with a suppression | 20-70 | 26 | 56 / 57 |
| CU5 worlds with a hidden cult at E | 10-50 | 4 | 18 / 20 |
| CU6 prophets among founders | <=60% | 54.8% | 54.7% / **60.8%** |
| FU1 wakings before E-500 | >=35% | 55.7% | 52.9% / 59.7% |
| FU2 `alliance_renewed` median | <=8 | 12.5 | 7 / 7 |
| FU3 kind 4 sites ended at E | >=25% | 69.9% | 70.1% / 71.1% |
| C1 wars median | 22-28 | 24 | 25.5 / 24.5 |
| G2 standing median (mean); population median | >=81; >=205,000 | 81.5 (82.9); 203,214 | **80.0** (83.3); 206,074 / 86.5 (87.8); 211,583 |
| M4a living monster at E | >=85 | 99 | 99 / 98 |
| M4b lair on a former settlement | avg >=65, each >=60 | 83 | 75 / 80 |
| T16 repeated battles per thousand | <=2 | - | 1.66 / 1.20 |
| T18 summary median; max | <=700 | 638; 806 | 656; 788 / 655.5; 819 |
| GW4 goblin regions median; mean | median <=12 | 12; 12.44 | 12; 12.61 / 13.5; 13.32 |
| DP9/GW5 dwarf regions median; mean | >=2 | 1.5; 2.09 | 2; 2.02 / 2; 2.52 |
| per-race regions medians (human, elf, orc, faerie) | - | 2, 3, 3, 2 | 3, 3, 2.5, 2 / 3, 3, 2, 2 |
| AL1 formed median; AL5 wars with an ally | AL5 20-45% | 14; 29% | 14; 31.7% / 15; 31.4% |
| DP1 depth-3 mine worlds | 40-90 | 50 | 43 / 40 |
| DP2 worlds with a waking; most | 25-55; <=4 | 49; 4 | 54; 3 / **62; 5** |
| DP3 sealed; DP6 alive; DP7 slain | 30-65%; >=70%; <=25% | 55.7; 95.1; 4.9 | 52.9; 94.3; 5.7 / 54.5; 87.0; 13.0 |
| DP4 hold-lost worlds; most | 15-40; <=4 | 23; 4 | 24; 4 / 28; **7** |
| DP5 realms ended by the deep | <=8%, >=1 in 200 | 0 | 0 of 144 / 1 of 155 |
| GW1 warren depth-3 worlds; GW2 goblin share of wakings; AD1 aided | 10-50; 10-30%; 10-40% | 27; **8.2%**; 29.5% | 29; 12.9%; 30.0% / 24; **6.5%**; 27.3% |
| P dwarves in alliance or trade | >=85% | 88 | 89 / 92 |

## Option sets (final values)

| Set | Result |
| --- | --- |
| `meddling` 0-49 | acts median 22.5 (max 57), 1.88 x normal's 12 on the same seeds; sent monsters alive 22.6% (above the default bound, not a target); T16 2.33 per thousand; standing 84.4 mean, population 207,150 |
| `distant` 0-49 | acts median 4 (0.33 x), at least one act in 50 of 50; cults median 1 (38 worlds with one); T16 2.15; population 200,763 |
| `--monster-pressure high` 0-19 | acts 12.5, cults 3, DP2 10 worlds, DP4 most 8 (one world; P5-07's high set had 5), wars 20, standing 76.5, T16 0; no break |
| `--civs-per-race 4` 0-19 | acts 27.5, cults 5, wars 101, summary median 983.5 (max 1,175; bound 1,050 holds on the median), T16 2.69 (2,602 battles), renewals 36 |
| `--event-intensity turbulent` 0-19 | acts 18, cults 4, wars 42, summary median 718 (max 1,027), population 192,854, T16 1.58 |

## Misses

1. **PN5** (champions that fight): 29.2% and 31.1% against 50%. Champions rise young and live 25 to 40 years (figures `life_*`); in seeds 2 and 5 most died in peace before a declaration, and only a war, a hunt or a death in battle counts. No lever in this stage's data moves the share (`hero_*` and the life values belong to P5-06 guards). A rule change would be needed (for example choosing a realm at war or with a monster raid for a champion).
2. **CU6** B 60.8% (pooled A and B 57.6%); A 54.7%. The only lever found (`caster_ppm`) breaks FG7.
3. **G2** standing median 80.0 on A against 81 (mean 83.3; the 0.30.0 values gave 81.5 and 79.5 on seeds 0-49, so the median swings by about 3 between runs); population is inside the bound (206,074 A, barely).
4. **DP2/DP4** on B: 62 worlds with a waking (bound 55) and one world with 5, DP4 most 7 in a world (bound 4). DP2 A is 54. With the first values of this calibration B had 46 and A 49, so the move comes with `deep_warren_wake_permille` 300 and `act_permille` (history shifts); I had no sweep left to separate them. The lead may set `deep_warren_wake_permille` back to 200 (GW2 would again be about 8%) or lower `deep_wake_ppm[2]`.
5. **GW2** B 6.5% (A 12.9%): small counts (5 of 77 wakings).
6. **PN3** evil 84 and 85 (bound 80) is near the edge. **PN6** 22.6% under `meddling` (default sets inside).
7. Not tunable here: dwarf regions A median 2 but 1.5 with the shipped values (an existing 0.30.0 effect; B 2).

## Suspected defects for the engineer

- `tower_taken_up` does fire (28 and 32 worlds, 32 and 42 events over 200 worlds), so notes about a lapse always preceding a take-up do not hold.
- The gods' flare-up fires only once `flare_regard` is below 200 (2 events in 100 worlds at 200; 199 and 220 events, in 87 and 89 worlds, at 130). Regard seldom reaches 200 with `regard_max` 400 and the fade: probably intended, but the shipped value was inert.
- The meddling response is bound by regard supply and rests: at `act_permille` 50 the ratio was 1.09 whatever the chance; the ratio needs a low chance per tick year (14). The spec's 2.5 x on `act_permille` then becomes the lever. A per mille scale of the chance per tick year with a floor of 14 leaves little headroom (`activity_permille` meddling 2500 gives 35).
- No hidden-cult or divine-monster rule is blocked (hidden at E in 18 and 20 worlds, `divine_monster` events 208 and 222).
- Prophets (role 7) are 55 to 61% of founders because most realms have neither a living caster nor a recorded ruler at the time of the act.

## Chronicle lines (seeds 100 to 199, summary level)

- `5910 e1084 divine_bless/55: Pleased by a battle won, Selston the Shield of Battles smiled upon the arms of the Duchy of Meas for 50 years.`
- `4360 e446 divine_curse/60: Angered by the sack of a city, Nukzogg the Maker of Goblins sent a pestilence upon the Realm of Vinna; 468 died.`
- `3216 e241 divine_curse/60: Angered by the felling of the wild, Stantmird the Hunter in the Green withered the fields of the Grat Mob.`
- `3374 e140 divine_champion/55: Pleased by a tower raised and a book written, Ciendrae the Lamp of the Wise chose Aernie Chosen of Ciendrae as the champion of the House of Faemovie.`
- `2714 e283 divine_monster/60: Angered by the felling of the wild, Caldhes the Hunter in the Green woke Og'ugark the Hungering, an ogre, against the Greerk Mob.`
- `2875 e105 divine_mark/45: After the felling of the wild, Rudstam the Hunter in the Green cursed the ground of the Stinking Ground of Rudstam.`
- `176 e15 cult_founded/50: Calling upon Thonngle the Lamp of the Wise, Voithaell the Learned founded the Children of Thonngle at the White Fane of Thonngle in Elienaenn.`
- `3433 e157 cult_spread/25: The Children of Voseitho spread from the Realm of Aennitha into Clan Braidinkom along the roads of trade.`
- `5499 e594 cult_suppressed/50: The Snib Mob under Sneebgreeg put down the Grovellers of Shiasifia, and its faithful fled into hiding.`
- `875 e18 tower_abandoned/25: After the death of its last master, the Star-Tower of Onnfaell of the Realm of Aennitha stood empty.`

Reading notes: several acts cite the same few deeds ("a tower raised and a book written" for the lore god, "the felling of the wild" for the wild god), so lines of one god repeat their cause; the ruler is not named in an act. The lines are from the sweep at `act_permille` 50 (seeds 100 to 119; wording does not depend on the values).
