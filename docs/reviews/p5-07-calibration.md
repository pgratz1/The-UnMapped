# P5-07 calibration (mountain depths, generator 0.29.0)

Engine 5e60904; values in `game/data/` only, mirrored unchanged into the fixture roots `worldgen/tests/data/geo` and `geo-small` (they were byte copies of the shipped files before; "fixtures as shipped"). Scripts `~/.cache/the-unmapped/out/p507-calib/scripts/` (`sweep.sh`, `m8.py`, `agg8.py`), tables in the sibling folders (`base0`, `r1` to `r3`, `r4B`, `r5`, `r6B`, `high`, `civs4`, `turb`). Six sweeps after the baseline. No generate or report error in any sweep. Test expectations and goldens not touched.

## Values changed

| File | Field | From | To | Why |
| --- | --- | --- | --- | --- |
| `history/monsters.toml` | `deep_years` | [150, 500, 1000] | [250, 1000, 2000] | DP1: 97 of 100 worlds reached depth 3; the years, not the delve chance, are the lever |
| `history/monsters.toml` | `deep_delve_permille` | 10 | 6 | with the above (alone it moved DP1 from 97 to 96) |
| `history/monsters.toml` | `deep_wake_ppm` | [0, 150, 600] | [0, 350, 1400] | DP2: later, fewer deep mines gave 16 worlds with a waking; restored to 52 |
| `history/monsters.toml` | `deep_takes_max` | 2 | 3 | DP4, DP5 |
| `history/monsters.toml` | `deep_reach_hops` | 1 | 2 | DP5 |
| `monsters/ymrach.toml`, `ysgarth.toml`, `zhakul.toml` | `overrun_permille` | 300, 200, 400 | 600, 400, 800 | DP5: the first realms ended by the deep |

Untouched: `deep_gate_levy_permille`, `deep_seal_max_permille`, `deep_toll_permille`, the kinds' `might`, `forge_depth_permille` (DP3, DP6, DP7, C1, DP9, P were inside bounds).

## Targets (A = seeds 0-99, B = 100-199; baseline = shipped values on A)

| Target | Bound | 0.28.0 | Baseline A | Final A | Final B |
| --- | --- | --- | --- | --- | --- |
| DP1 depth-3 worlds | 40-90 | - | 97 | 80 | 85 |
| DP2 worlds with a waking; most | 25-55; <=4 | - | 45; 4 | 52; 3 | 52; 3 |
| DP3 sealed share | 30-65% | - | 50.8% | 58.2% | 55.4% |
| DP4 worlds with a hold lost; most | 15-40; <=4 | - | 26; 5 | 24; 4 | 27; **5** |
| DP5 realms ended by the deep | <=8%, >=1 in 200 | - | 0 | 1 of 146 | 1 of 158 |
| DP6 alive at E | >=70% | - | 80% | 89.6% | 87.7% |
| DP7 slain | <=25% | - | 20% | 10.4% | 12.3% |
| DP9 dwarf regions median; mean | >=2; >=0.28.0 - 0.2 | 2/2.2; 2/2.4 | 2; 2.09 | 2; 2.12 | 2; 2.20 |
| P dwarves in alliance/trade | >=85% | 90, 91 | 90 | 90 | 89 |
| M4a living monster (without deep horrors) | >=85 | 94, 93 | 98 (95) | 96 (93) | 98 (95) |
| M4b lair on former settlement | avg >=65, each >=60 | 62, 68 | 73 | 71 | 72 |
| C1 wars median | 22-28 | 25, 23 | 26 | 24.5 | 22.0 |
| G2 standing; population medians | >=81; >=205,000 | 82 / 214k; 86 / 211k | 83 / 212.7k | 82.5 / 211.3k | 85.5 / 218.6k |
| T18 summary median; max | <=670 | 625 / 784, 627 / 801 | 627.5; 757 | 622; 775 | 627.5; 801 |
| AL5 wars with an ally | about 30% | 29.7, 31.2 | 30.0 | 29.8 | 30.9 |

Worlds with a living monster only because of a deep horror: 3 (A) and 3 (B).

Other sets (30 seeds, final values):
- `high`: 22 wakings in 16 worlds; 5 worlds with 17 holds lost (most 5 in one); 0 realms ended, 1 flight; standing median 74.5, population 200,920; DP9 median 2; P 86.7%.
- `civs4`: 39 wakings in 23 worlds; 9 worlds with 18 holds lost; 0 ended; summary median 915 (max 1,126; the median is inside the 1,050 bound); DP9 median 5.
- `turbulent`: 19 wakings in 14 worlds; 4 worlds with 11 holds lost; 0 ended; summary median 687.5 (max 890); DP9 median 1 (mean 2.13); P 83.3%; standing 80, population 202,767.

## Misses

1. DP4 "most in one world <= 4": B has one world with 5 holds lost (A final: 4). A tail case; not tuned further, since the only levers (`deep_takes_max`, the overrun values) are what give DP5.
2. DP5 holds on two events (0.7% and 0.6%); none in the 30-seed sets. Fragile: it needs the overrun doubling and `deep_takes_max` 3.
3. Reported, not targets: turbulent summary median 687.5 and high-pressure standing median 74.5 are under the default-world bounds only for default worlds.

## Odd things seen

- `deep_delve_permille` has almost no effect (10 to 6 moved DP1 by one world); the gate is `deep_years`.
- A waking: `5773 e685 deep_woken/60: In the deepest halls under Gridinkord, Clan Thrairn under Khammno woke the Eyeless of Dourkha, a host of eyeless things from under the stone.`
- A hold lost: `5651 e419 hold_lost/70: After the raids, the Eyeless of Handkaird broke out of the deeps into Kairkgrou of Clan Drourd; 1,146 died, and 1,721 fled.` and the renaming that follows: `The Sunken Peaks since 5621, e514 after e513 hold_lost; formerly Pedlar's Peaks`.
- A realm ended (seed 72, A): `5698 e327 civ_ended/85: After the loss of its last hold to the deep, Clan Doulidi under Moliti the Last came to an end.`
- No `deep_sealed` line (salience 50) appeared in the sampled summaries, so no seal is quoted; sealings are in the full chronicle only.
- The quoted falls are all in years 5600 to 5800: with these years the deep is a late-history event.

## Suspected defects

None (no crash, error exit or impossible row in about 460 worlds).

## Second calibration (r2)

Engine 0864053 (goblin deep warrens 6.10, allies' aid 6.11). Scripts and tables: `~/.cache/the-unmapped/out/p507-calib2/` (`scripts/sweep.sh`, `m9.py`, `agg8.py`; folders `base0`, `r1` to `r5`, `rB`, `high`, `civs4`, `turb`). Five tuning rounds after the baseline, one confirmation on B and three 30-seed sets. No generate or report error in any sweep. Fixture roots `geo` and `geo-small` mirror the shipped `monsters.toml` (byte copy). Test expectations and goldens not touched. Counts of DP2 and DP4 include goblin events (spec 15.1 says every waking counts); mine-only figures are beside them.

### Values changed (`game/data/history/monsters.toml`)

| Field | From | To | Why |
| --- | --- | --- | --- |
| `deep_years` | [250, 1000, 2000] | [250, 1000, 2500] | GW1: 78 worlds with a depth-3 warren at the starting values. `deep_delve_permille` 6 x goblin `delving_permille` 300 div 1000 is 1 per mille (integer floor), so the goblin pace cannot be lowered by `delving_permille` (below 167 it is 0, above 333 it is 2); the years are shared with mines (DP1 79 to about 53, still inside 40-90) |
| `deep_warren_population_min` | 600 | 3200 | goblin towns run 850 to 3,500 (one 8,000), so 600 and 1,500 changed nothing; 2,800 and 3,200 had little effect alone |
| `deep_warren_ore_min` | 1 | 500 | the real lever: about half of all regions have ore reach 1 or more, a quarter 500 or more; 100,000 (upland only) gave 6 warrens in 100 worlds |

Unchanged: `delving_permille` 300, `deep_warren_wake_permille` 200 (GW2 was inside 10-30% at the start), `deep_aid_*` (AD1 29%, inside), `deep_delve_permille`, `deep_wake_ppm`, `deep_takes_max`, the kinds' values.

### Targets (A = seeds 0-99, B = 100-199)

| Target | Bound | First calibration | Starting values A | Final A | Final B |
| --- | --- | --- | --- | --- | --- |
| GW1 warren depth-3 worlds | 10-50 | - | 78 | 30 | 22 |
| GW2 goblin share of wakings | 10-30% | - | 25.0% | 13.3% (6 of 45) | 13.9% (5 of 36) |
| GW3 goblin holds lost (reported) | - | - | 31 of 61 | 4 of 39 | 4 of 27 |
| GW4 goblin regions median; mean | median <= 12; mean <= 12.1/12.6 + 0.2 | 12 / 12 | 12; 12.87 | 12; **12.82** | 12; **12.86** |
| GW5 dwarf regions median | >= 2 | 2 | 2 | 2 | 2 |
| AD1 aided wakings | 10-40% | - | 30.3% | 28.9% (13 of 45) | 25.0% (9 of 36) |
| aided held / fell; unaided held / fell | reported | - | 17/6; 29/24 | 8/5; 15/17 | 6/3; 12/15 |
| DP1 depth-3 mine worlds | 40-90 | 80, 85 | 79 | 53 | 50 |
| DP2 worlds with a waking; most (mine-only worlds) | 25-55; <=4 | 52, 52 | 59; 3 | 37; 3 (32) | 31; 3 (27) |
| DP3 sealed share | 30-65% | 58.2, 55.4 | 50.0% | 42.2% | 41.7% |
| DP4 hold-lost worlds; most (mine-only worlds) | 15-40; <=4 | 24, 27 | 28; 5 | 21; 4 (19) | 18; 4 (16) |
| DP5 realms ended by the deep | <=8%; >=1 in 200 | 1, 1 | 1 of 145 | 1 of 145 | 0 of 158 (1 in A and B) |
| DP6 alive at E | >=70% | 89.6, 87.7 | 89.5% | in bound (see table files) | 88.9% |
| DP7 slain | <=25% | 10.4, 12.3 | 10.5% | in bound | 11.1% |
| DP9 dwarf regions median; mean | >=2 | 2/2.12, 2/2.20 | 2/2.17 | 2/2.11 | 2/2.23 |
| P dwarves in alliance/trade | >=85% | 90, 89 | 90 | see `r5/table.txt` | 89 |
| M4a living monster | >=85 | 96, 98 | 97 | 92 | 95 |
| M4b lair on former settlement | avg>=65, each>=60 | 71, 72 | 80 | 69 | 75 |
| C1 wars median | 22-28 | 24.5, 22.0 | 24 | in bound | 23 |
| G2 standing; population | >=81; >=205,000 | 82.5/211k, 85.5/219k | 82/209k | 83.5/211k | 82/217k |
| T18 summary median; max | <=670 | 622; 775, 627.5; 801 | 619; 809 | in bound | 622.5; 778 |
| AL5 wars with ally | 20-45% | 29.8, 30.9 | 28.1% | in bound | 32.3% |

Other sets, 30 seeds: `high`: 19 wakings in 13 worlds, 5 worlds with 12 holds lost (most 5), 0 realms ended, warren depth-3 in 9 worlds, standing 77, summary median 609.5, wars 21. `civs4`: 26 wakings in 17 worlds, 11 holds lost, 0 ended, aided 42%, summary median 911 (max 1,079). `turbulent`: 18 wakings in 13 worlds, 11 holds lost, 0 ended, summary median 722, population 199k, P 83.3%.
Per-race regions held at E (median / mean): A: dwarf 2/2.11, goblin 12/12.82, human, elf, orc, faerie in `r5.log`; B: dwarf 2/2.23, goblin 12/12.86, human 3/5.49, elf 3/3.64, orc 3/3.45, faerie 2/2.46. Starting values A: dwarf 2/2.17, goblin 12/12.87, human 4/4.65, elf 3/3.62, orc 2/3.39, faerie 2/2.56.

### Misses

1. GW4 mean: goblin mean regions 12.82 (A) and 12.86 (B) against 0.28.0's 12.1 and 12.6 plus 0.2 (B misses by 0.06; A by 0.5). The median holds (12). It did not move with the warren values (12.87 at the start, 12.91 and 12.81 in rounds 1 and 2): it is the engine's baseline since 0.28.0 plus noise, not the warrens. The first calibration did not record goblin means, so no r1 baseline exists.
2. DP4 "most in one world" was 5 in rounds 1 to 4 and 4 in the final A and B. High pressure has a world with 5.
3. DP5 holds on one event in 200 worlds (A); B has none. Fragile as in the first calibration.
4. M4a 92 on A and M4b 69 on A are inside bounds but lower than the first calibration (fewer horrors).
5. `high` has GW1 9 and standing 77, turbulent summary 722: non-default sets, not targets.

### Aided gates

No aided gate falls too rarely: A+B aided 14 held, 8 fell; unaided 27 held, 32 fell. The first calibration's "all aided hold" did not recur, so `deep_aid_levy_permille` stays.

### Chronicle lines

- Goblin warren waking (seed 12): `4638 e1100 deep_woken/60: Digging after grubs and glitter beneath Zog Hole, the Gruzz Horde under Gokgreex blundered into the old dark and woke the Swarm Beneath Tamkili, a host of eyeless things from under the stone.` and its fall `4638 e1101 hold_lost/70: The Swarm Beneath Tamkili boiled up out of the burrows into Zog Hole of the Gruzz Horde; 2,484 died, and 3,726 fled.`
- Aided gate that held (seed 85): `5663 e864 ally_aided_hold/40: Unbidden, the Hold of Khard hurried its people from Drorin Hold to stand with the Hold of Khairthain at the gate of Brourkou Hold against Khark the Deep-Coiled.` then `e865 deep_sealed/50: The Hold of Khairthain sealed Khark the Deep-Coiled in the deeps of the Mines of Brourkou Hold.`
- Aided gate that fell (seed 85, 5795): `e1011 ally_aided_hold/40: Unbidden, the Duchy of Edas hurried its people from Bedterford to stand with the Hold of Khairthain at the gate of Thark Hold against Dairngound Flame-Under-Stone.` then `e1012 hold_lost/70: Dairngound Flame-Under-Stone rose from below and overran Thark Hold of the Hold of Khairthain; 1,487 died, and 2,231 fled.`
- Warren sealed with aid (seed 5): `5311 e1085 warren_sealed/50: With rockfall and fire, the Slatch Horde under Bazz buried Dundnidin the Deep-Coiled deep below Gozzbeerk, and no goblin went that way again.`
- Seals in a summary: salience 50 never reaches one. No `deep_sealed` or `warren_sealed` line appears in any of the 200 default summaries (salience 60 and above do: wakings and holds lost are in the full chronicle only for seeds shown; seals are full-chronicle only).

### Suspected defects

None (no crash, error exit or impossible row in about 590 worlds). Note for the engineer: `deep_delve_permille` x `delving_permille` div 1000 floors, so the goblin pace has only the values 0, 1, 2 per mille; a finer scale (ppm) would let data tune it.
