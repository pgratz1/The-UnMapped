# P5-04a calibration (generator 0.28.0, data only)

Engine d2412b1; values in `game/data/` only. Sweeps: `generate --map-size 1024` plus both report levels, scripts in `~/.cache/the-unmapped/out/p504a-calib/scripts/` (`sweep.sh`, `m6.py`, `m7.py`, `agg7.py`), tables in the sibling folders (`base0`, `r1` to `r8`, `fin0`, `fin1`, `high`, `civs4`, `turb`). Eight tuning sweeps after the baseline, then the final values on seeds 0-99 and 100-199 and 30 seeds each of `high`, `civs4`, `turbulent`. No generate or report error in any sweep.

## Targets

A = seeds 0-99, B = seeds 100-199. Baseline = shipped values at d2412b1 (A only).

| Target | Bound | 0.27.0 | Baseline A | Final A | Final B |
| --- | --- | --- | --- | --- | --- |
| AL6 refused vs joined | refused <= joined | -- | 862 > 783 FAIL | 782 <= 917 | 698 <= 945 |
| AL6b motive max / motive 0 | <= 50% / <= 35% | -- | 38.5% / 11.1% | 33.4% / 11.6% | 34.8% / 11.5% |
| AL5 wars with ally; defence; attack | 20-45%; >=70%; >=10% | 25.9%, 27.5% | 24.0%; 83.5%; 16.5% | 29.7%; 84.4%; 15.6% | 31.2%; 84.9%; 15.1% |
| AL1 formed / standing median | 6-15 / >=3 | 14.5, 15 / 9.5, 9 | 14 / 9 (min 3) | 14 / 9.5 (min 4) | 13.5 / 9 (min 4) |
| AL7 betrayal worlds; share of ended | 20-60%; 3-15% | -- | 60; **23.1%** | 53; **16.8%** | 51; **16.3%** |
| SP1 granted per mille | 500-850 | -- | 598 | 631 | 584 |
| SP2 spurned worlds; share of ended | 10-35 worlds; <= 6% | -- | 25; 5.0% | 29; **6.6%** | 30; **8.4%** |
| SP4 large share of grievous withholdings; ended by it | 30-50%; >= 40% | -- | 58.5% (fail); 73% | 30.4%; 74% | 27.3% (low); 75% |
| RN1 written renewals | <= 35% | -- | 25.7% | 25.2% | 26.4% |
| RR2 into friendship; before 5500; max turns | >=25%; >=55%; <=20 | 54%, 55% | 55%?; 57%; 11 | 57.4%; 58.6%; 12 | 58.3%; 63.9%; 10 |
| RR6 worlds with a pair of 4+ turns | <= 5 | -- | 11 | 5 | 9 (**miss**) |
| RR4 friendship at E median | >= 3 | -- | 4 | 4 | 4 |
| MX3 expulsions median (max) | <= 3 | -- | 1 (12) | 2 (12) | 1 (11) |
| TR2 basis 4 | >= 15% | -- | 21.8% | 21.7% | 21.9% |
| P dwarves in alliance or trade | >= 85% (90% others) | -- | -- | 90%; human 93% | 91%; human 92%, orc 94% |
| C1 wars median | 22-28 | 25, 25 | 25 | 25 | 23 |
| T16 per thousand | <= 2 | 1.19, 1.68 | 1.36 | 1.26 | 1.39 |
| G2 standing / population median | >= 81 / >= 205,000 | 85.5/218k | 83 / 210.7k | 82 / 214.5k | 86 / 210.9k |
| T18 summary median / max | <= 670 | 613.5 | 625 / 766 | 625 / 784 | 627 / 801 |
| M4a living monster at E | >= 85 | 92, 93 | 86 | 94 | 93 |
| M4b lair on former settlement | A+B average >= 65, neither < 60 | 63, 68 | 57 | 62 | 68 (average 65) |
| M slain median; overruns median; towns | <= 4; 2-4; <= 10% | 4; 3; 4.4% | 4; 3; 3.3% | 4; 3; 5.0% | 4; 3; 7.6% |
| UD2 worlds with rising; median; UD3 | 70-95; <= 3; <= 50% | 77/2, 85/2 | -- | 84; 2; 48.6% | 84; 2; 42.0% |
| FG1 figures median; max | 60-120; <= 250 (L-6) | 77/226 | 80; 190 | 79; 175 | 76; 208 |
| FG5 slain by heroes | >= 25% (L-4) | 27.9% | 22.0% | 30.8% | 33.8% |
| FG8 alive through a quiet step | >= 15% (L-5); elf/faerie reported | 20.3% | 20.3% | 20.8%; 23.2% | 21.1%; 22.6% |
| FG11 over bound; epithet share | 0 worlds; 15-40% | 1 world; 17% | 0; 16.0% | 0; 15.6% | 0; 16.1% |
| FX1 larger skill wins | 55-70% | 55.7%, 54.3% | 56.6% | 57.5% | 56.7% |
| FX3 grasping betrayal ratio | >= 1.5 (fallback >= 1.25) | 1.53, 1.27 | 1.49 | 1.47 | 1.57 |
| TW1 kind 4 sites ended | >= 15% | 0% | 7.2% | 8.7% | 7.0% (**miss**) |
| TW2 towers within spacing | none | -- | 0 | 0 | 0 |
| FG7 casters median; tower worlds; books | 2-6; >=80%; 3-10 | 6; 99%; 8 | 6; 99; 8 | 6; 99; 8 | 6; 100; 8 |
| HR1 `hero_rose` with a long quiet | reported | -- | 16 (median 667) | 20 (median 758) | 15 (median 418) |

Other sets (30 seeds, final values): `high`: M4a 30 of 30, UD median 5.5, slain 9 and overruns 8 (high pressure), AL6 220 <= 235, C1 22, summary max 784. `civs4`: AL6 1435 <= 1643, C1 median 92, figures median 261 (max 347), summary median 915 (max 1157; bound `civs4` <= 1,050 holds), SP2 8.3%. `turbulent`: AL6 **519 > 485**, C1 35, T16 2.38, summary median 667, figures max 265.

## Values changed (file, field, from, to, why)

| File | Field | From | To | Why |
| --- | --- | --- | --- | --- |
| `history/alliances.toml` | `join_warmth_floor` | 400 | 700 | AL6: joins above refusals |
| | `join_fear_floor` | 300 | 600 | AL6 |
| | `join_busy_permille` | 250 | 150 | AL6 |
| | `join_weary_permille` | 600 | 750 | AL6 |
| | `refuse_grudge` | 150 | 80 | AL7: fewer grudges, fewer wars on allies |
| | `withheld_grudge` | 250 | 150 | AL7, SP2 (cooling spurnings) |
| | `spoils_large_share` | 600 | 475 | SP4 (see odd findings) |
| | `spoils_large_part_permille` | 300 | 401 | SP4: 401 is never reached (cap 400), so "large" is by share only |
| | `soured_renew_permille` | 500 | 250 | SP2 (no measurable effect at the end) |
| `history/relations.toml` | `turn_cooldown_years` | 300 | 800 | RR6 |
| `history/figures.toml` | `temper_allied_permille` | 1000/1000/1000/3000/500 (steady, warlike, peaceable, grasping, just) | 300/300/300/3000/150 | AL7 down, FX3 up: only grasping rulers keep the strain toward allies |
| `history/monsters.toml` | `hunt_permille` | 20 | 12 | FG5, M4a, M4b (fewer realm slayings) |
| `monsters/dragon.toml` | `overrun_permille` | 600 | 1000 | M4b (more lairs on overrun towns; overruns median stays 3) |

Not changed: `the Old` (0.28.0 values 1400 and 1480 hold FG11), `skill_min/max` 600/900 (FX1 met), `hero_hunt_permille`, `tower_spacing_permille`, `expel_*`, `refusals_to_break`, `withheld_to_break`, `undead_dead_permille` (tried 24: M4b 73 but UD3 52.3% and slain median 4.5; back at 28).

## Misses and bounds under strain

1. **AL7 share of ended alliances 16.8% and 16.3% against 15%** (worlds 53 and 51 inside 20 to 60). Baseline 23.1%. No lever in 15.2 moved it further; betrayals by non-grasping rulers did not follow `temper_allied_permille`. Lead: restate to <= 18% or accept.
2. **SP2 at most 6% of ended: 6.6% (A), 8.4% (B)**; worlds 29 and 30 are inside. It trades against SP4: more spurnings at once come with more large shares. Lead: restate to <= 9%, or lower SP4 to 25%.
3. **SP4 large share 30.4% (A), 27.3% (B)** against 30%; ends 74%.
4. **RR6 9 worlds in B** (5 in A) against 5 (cooldown 800; 450 to 600 gave 4 to 8).
5. **TW1 7% to 9%** against the estimate 15%: no data lever (ends follow realms falling and land lost); lead may restate (21.4 says so).
6. **Turbulent: AL6 refused 519 > joined 485**, T16 2.38 > 2 (reported only; no bound given for the set).
7. M4b A is 62 (average of A and B 65, neither under 60: Paul's bound met as an average). Sample noise is plus or minus 4 worlds: the same values gave 57 to 65 in the sweeps.
8. FG8 under the spec's literal definition (a whole quiet phase inside a life) is about 6%: quiet phases last hundreds of years. Reported above is the definition validated at 0.27.0 (at least 25 years of a quiet phase inside a life, 20.3% then): the lead should restate 21.2's definition.

## Odd findings

- `spoils_large_part_permille` is capped by `spoils_cap_permille` 400: part 400 means "capped" (about half of all grievous withholdings) and 410 means never; large share jumped 54% to 9% between 400 and 410. Tune SP4 by `spoils_large_share` with part >= 401, or lower the cap.
- With `spoils_large_share` 750 and part off, large was 9%; 650, 16%; 500, 29%; 475, 30% to 27%.
- C1 wars rose to 27 when the non-grasping allied strain was lowered to 600 (r3), then fell back at 300; not understood, within noise.
- `p5-06-validation/scripts/m6.py` still runs against report v9 worlds unchanged.
