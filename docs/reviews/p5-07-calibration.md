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
