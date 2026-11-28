# P5-11 calibration (artifacts; generator 0.31.0)

Engine `65843de` (branch `p5/11-calib`). Values in `game/data/` only, mirrored byte for byte into `worldgen/tests/data/geo` and `geo-small` (`history/artifacts.toml`, `history/pantheon.toml`). No C++ change, no golden re-recorded, no ctest run. Scripts and tables: `~/.cache/the-unmapped/out/p511-calib/` (`scripts/sweep.sh`, `m11.py`, `agg11.py`; tables `FA/`, `FB/`, `O*/table.txt`). Seven tuning sweeps on seeds 0-49 (t1 to t7) plus a no-artifact baseline (`occasions = []`); then A, B and the option sets with the final values. Bounds that count worlds are per 100 worlds (halve them for a 50-world tuning sweep).

## Values changed (old to new)

`history/artifacts.toml`
- occasion chances: war_declared.defender 60 to **180**, .attacker 30 to **100**, sacked 100 to 160, civ_conquered 200 to 320, succession_disputed 120 to **60**, hero_rose 150 to 240, settlement_overrun 150 to 240, deep_woken 400 to 640, divine_champion 500 to 800, cult_founded 300 to **130**.
- staff weight: 0 to 1000 on civ_conquered, succession_disputed, hero_rose, divine_champion, cult_founded; war_declared 200/100 to 150/150; sacked 200 to 300; overrun 300 (unchanged); deep_woken 400 to 500. A staff is only drawn when a caster with a tower is present, so a high weight costs nothing elsewhere.
- two new occasions (staff 1000 only, bearer realm): `tower_founded.civilization` 120 and `book_written.civilization` 40.
- `realm_rest_years` 50 to 30; `burial_permille` 300 to 150; `battle_take_permille` 500 to 850; `sack_take_permille` 700 to 1000; `capture_take_permille` 500 to 1000; `raid_take_permille` 30 to 800.
`history/pantheon.toml`: `cult_return_years` 400 to 1500; `cult_spread_permille` 20 to 6 (FU5: spreads per cult saturate at the number of reachable realms, so only the chance moves the median; cult_return_years alone did not).
Untouched: `forge_chance_permille`, `world_max` 40, all caps, claim values, the fifteen kind files, `smith_age_*`.

## Targets and guards (A = seeds 0-99, B = 100-199)

| Target | Bound | A | B |
| --- | --- | --- | --- |
| AR1 artifacts a world: median; max; worlds with one | 8-20; <=40; >=95 | 15; 38; 100 | 15; 40; 100 |
| AR2 smith-made; dwarven maker of those; commissions of those; casters; gods | >=45; >=30; >=10; >=5; >=5 % | 81.9; 44.0; 30.9; 10.6; 7.5 | 82.7; 43.8; 29.8; 10.8; 6.4 |
| AR3 changed hands | >=30% | 32.0 | 30.5 |
| AR4 lost at E: median; worlds with one | 2-8; >=80 | 3; 94 | 3; 94 |
| AR5 held by others, worlds | >=40 | 77 | 76 |
| AR6 recovered pooled | >=15 | 124 | 84 |
| AR7 wars citing an artifact | <=10% | 1.6 | 0.6 |
| **AR8 figures a world, max** | <=250 | **263** | **301** |
| FU4 battles under acts (reported) | - | 174 total, median 0 | 171, median 0 |
| FU5 cults with 6+ suppressions; median spreads per cult; max | <=2; <=4; <=12 | 0; 3; 12 | 0; 3; **18** |
| C1 wars median | 22-28 | 25.5 | 26 |
| G2 standing median; population median | >=81; >=205,000 | 86; 216,401 | 86; 206,338 |
| M4a / M4b | >=85; avg>=65, each>=60 | 97 / 77 | 95 / 81 |
| T16 per thousand | <=2 | 1.61 | 1.48 |
| T18 summary median (max) | <=750 | 675 (817) | 685.5 (884) |
| PN1 median; max | 8-25; <=60 | 13; 31 | 14; 29 |
| PN3 camps (good/neutral/evil) | >=80 | 100/98/89 | 100/98/89 |
| PN4 champion share (others 11-30%) | 5-45% | 6.6 | 5.9 |
| PN5 / PN6 / PN7 / PN8 | >=50% / <=20% / <=3% / <=3 | 68.9% / 9.9% / 0.41% / 2 | 73.3% / 8.6% / 0.48% / 2 |
| CU1 median; CU2; CU3; CU4; CU5 | 2-6; >=40%; >=50; 20-70; 10-50 | 3; 76.6%; 96; 38; 26 | 3; 79.6%; 97; 40; 23 |
| **CU6 prophets** | <=60% | **68.2** | **67.4** |
| FU1 / FU2 / FU3 | >=35% / <=8 / >=25% | 43.8% / 7 / 72% | 52.5% / 7.5 / 71.4% |
| AL1 formed; AL5 wars with ally; dwarves in alliance or trade | AL5 20-45; >=85% | 14; 29.8%; 90% | 15; 32.3%; 91% |
| DP1 / DP2 / DP3 | 40-90 / 25-55 / 30-65% | 41 / 51 / 64.1% | 52 / **61** / 47.5% |
| DP4 worlds; most | 15-40; <=4 | 18; **5** | 32; 4 |
| DP5 | <=8% | 0 of 144 | 0 of 158 |

No-artifact baseline on seeds 0-49 for comparison: CU6 67.9%, T16 1.13, T18 661 (max 757), figures max 156, population median 203,953 (under 205,000; with artifacts 216,401 on A).

## Option sets (seeds 0-19)

| Set | Result |
| --- | --- |
| `meddling` | acts median 21.5 (max 57); artifacts median 14 (max 40); AR3 28.8%; held-by-others 18 of 20 worlds; T16 0; standing median 77.5, population 196,827 (the set's own lower values, as P5-08) |
| `distant` | acts median 4.5; artifacts 13.5; AR3 31.8%; gods 1.9% of makings (few acts and cults); T16 2.11; figures max 271 |
| `--monster-pressure high` | artifacts 17.5 (max 38); wars 22; standing 74.5; T16 2.78 (2 repeated of 719); DP4 most unchanged from P5-08's high set |
| `--civs-per-race 4` | artifacts median 40 (every world at the cap; min 30); wars 95; summary median 972.5 (max 1,171; bound 1,100 on the median holds); figures max 447; T16 2.24 |
| `--event-intensity turbulent` | artifacts 17.5 (min 13, max 40); AR3 41.9%; summary median 697 (max 1,089); wars 35.5; figures max 513; dwarven share of smith-made 23.6%; T16 3.01 |

T16 on the small sets rests on 2 to 5 repeated battles; on 100 default worlds it is inside the bound.

## Misses

1. **AR8** (figures a world, max 250): 263 on A, 301 on B. Each artifact made by a smith raises a figure; the default median is 13 smiths, the worst worlds have 31 to 39 and make 38 to 40 artifacts. With no artifacts the same seeds had max 156 (50 seeds); with the first values 216. A lower `world_max` or chance scale would meet it but costs AR1's headroom and Paul's cap; the lead decides (median figures 100 to 102, unchanged in kind).
2. **CU6** prophets 68.2 and 67.4% (bound 60): a P5-08 miss (61 to 71% then), not caused by artifacts (baseline 67.9%).
3. **DP2** B 61 worlds with a waking (bound 55) and **DP4** A most 5 in a world (bound 4): history shifts with every data change (P5-08 had B 57/62 and A 4 to 7); DP1 A 41 is at the lower edge. Not levers of this stage.
4. **FU5** max spreads of one cult 18 on B (bound 12); the median (3) and the count of heavily suppressed cults (0) are met.
5. AR3 on B 30.5% and gods 6.4% are on the edge; AR3 under `meddling` 28.8%.

## Rules that still never fire, or nearly, for the engineer

Counted over 200 default worlds (summary level chronicle plus holding openers of sweeps A and B):
- Artifacts were not seen enough to judge: `artifact_lost` is rare (33 and 37 events) because the take chances of battle, sack and capture are 850 to 1000; a 'lost on the field' is now about one battle in seven. Lost things come mostly from burial (139, 155) and hoards (88, 92).
- Rules that fired in 100 worlds each: M1 pick-up by a new general (866, 881), M2 return to the realm on a bearer's death (805, 770) and burial (139, 155), M5 hunt recoveries (116, 71), M6 (`monster_died` to a lost state, 5 and 3), M7 raids (28 in the summary lines), M13 (`war_won` and `peace`, 39 summary lines), M14 (suppressed temple, 1 to 5), the staff enshrined at its maker's death (135 and 138 with the final values; it never fired at the engine's first values because no staff was made: casters were 1.3% of makings).
- Not separable from the data kept: M11 and M12 (`how` 4, the fall of a realm): no `artifact_taken` or `artifact_lost` with a `civ_ended` cause line was seen in the summaries (companion lines are not printed at summary). A `check-world` count is needed.
- **Suspected defect (casters)**: the 5% target needed new occasions (`tower_founded`, `book_written`) and staff weight 1000 on many rows; with the spec's starting rows a staff needs a living caster with an open tower at the occasion, and that was 1% of makings. If the engineer wants staffs from the shipped rows, the staff test should not need the tower.
- The cult-spread count saturates at the number of reachable realms (median 5 of 6 at the old values), so `cult_return_years` has no effect on the median; probably intended.

## Chronicle lines (summary level, seeds 0 to 199)

- `3222 e162 artifact_made/45: In answer to the coming of war, Gaskox forged the Black Helm of Drork, a helm, for the Baskeeb Mob.`
- `4218 e296 artifact_made/45: Siatowhie the Learned wrought Shienglo, a staff, for the Twiwhiwae Ring.`
- `2519 e105 artifact_made/45: In answer to a cult founded, Ithaglir the Lamp of the Wise bestowed the Reliquary of Emisa, a reliquary, upon the House of Nife.`
- `5939 e747 artifact_taken/40: After the sack of a city, the Krurz Horde took Drurdaird, a shield of deep iron, from the Duchy of Cear.`
- `5560 e353 artifact_taken/40: After the fall of its bearer, the Mard League took the Black Hauberk of Snutch, a hauberk, from the Skoskrug Mob.`
- `5625 e468 artifact_lost/35: After the fall of its bearer, Gnogriz, a shield, was lost to the Nagrunk Horde in Larkdown.`
- `5692 e969 artifact_hoarded/50: After a raid, Mossback Zeet carried Kukh's Hammer, a hammer, off from the Zamuguk Horde to its hoard.`
- `5943 e921 artifact_hoarded/50: After the fall of a hold, Drir the Deep-Coiled carried Tummtourk, a sword of deep iron, off from the Griligair Throne to its hoard.`
- `5789 e654 artifact_recovered/50: After a war won, the Baskeeb Mob recovered Nagnab, a helm, from the House of Nife.`
- `5703 e987 artifact_recovered/50: After the slaying of its keeper, the Zamuguk Horde recovered Kukh's Hammer, a hammer, from the hoard of Mossback Zeet.`

Reading notes: kind 116 (burial) is not a summary line; the 'lost' line names the field but not the bearer; the staff line has no cause clause (no occasion is cited for a tower-born making in the template's cause words, which reads well enough); the sack and capture lines dominate (405 and 248 of 745 summary takings), so one artifact passes between realms in a run of near-identical lines.
