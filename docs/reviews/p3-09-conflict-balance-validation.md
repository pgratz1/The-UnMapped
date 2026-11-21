# Validation: P3-09 conflict balance (generator 0.18.0, schema 7)

Validation specialist, 2026-11-20.

**Input.** Branch `p3/09-validation` was cut from `origin/p3/09-int` at `8d9447d`. That commit holds the engineer's code, the independent test writer's tests and the slow-suite repair. Two merges followed, with no conflicts:
- `origin/p3/09-audit` at `ed9b7e3`: the determinism audit (pass with notes), the lint extension and the spec text of `p3/09-spec` `83e19a0`;
- `origin/main` at `7763484` (generator 0.17.0): already contained, so nothing came in.

After the merges, `worldgen/src`, `worldgen/include`, `tools/worldgen-cli` and `game/` are byte-identical to `8d9447d`.

**Specs.** Change spec `docs/specs/p3-09-conflict-balance.md` r5. Normative text: P3-06 20 (r12), P3-05 21, P3-07 21.5 and 21.8, and P3-02 11.11 (the 393-word list).

**Baseline.** The P3-08 validation's 0.17.0 binary, sweep and world-hash files: `~/.cache/the-unmapped/out/p3-08-validation/`, `~/.cache/the-unmapped/out/p3-08-val/world-hashes/`.

## Verdict

**After the cession fix: pass** (section 12). D1, D2 and D3 are closed; the rest of this report describes the first run, before the fix.

**First run: fail on one defect (D1).** Everything else passes. D1 is a rule gap in P3-06 20.3 that leaves a region held without a settlement of its holder for 760 years. It occurs in 1 of 375 sweep worlds, and the test writer's reference flags it as a failure. The C++ matches the reference row for row, so the code follows the spec as written. D2 is a definition mismatch: four worlds miss T2 as the statistic is defined, although weariness invariant 12 holds in every world. D3 is a small defect in the reference's statistic. Section 9 has the reproductions.

| Gate | Result |
| --- | --- |
| Build, five Linux presets | warning-free |
| `ctest`, five presets, after recording | 33 of 33 entries pass |
| Lint | both commands clean |
| Extension (`linux-gcc-release`, `UNMAPPED_BUILD_GDEXT=ON`) | 34 of 34 pass, `gdext-headless-check` included; its world-hash `expect` gives 15 of 15 |
| Slow suites, `linux-gcc-release` and `linux-clang-release` | `worldgen-tests -ts=*-slow --no-skip` 57 of 57 and `worldgen-cli-tests` 8 of 8 on both |
| World hashes across the five presets | 15 cases identical; `expect` 15 of 15 on each |
| Unchanged against 0.17.0 | everything section 6 says must not move did not move |
| Reference `p3_conflict_reference.py --version 0.18.0 check-world` | 374 of 375 pass; d2048 seed 3 fails (D1) |
| Peoples reference `check-world` | 20 of 20 elf-only worlds pass (the only set without conflict) |
| My independent check (`check09.py`) | 371 of 375 pass; the 4 failures are D2 |
| Purity | threads 1, 2 and 6 byte-identical; `--until history` equals the default |

## 1. What was run

Evidence is under `~/.cache/the-unmapped/out/p3-09-validation/`. The scripts and their logs:

- `build-all.sh` builds the five presets; `ctest_run.sh` runs ctest rounds 1 and 2; logs are in `logs/`.
- `phaseA.sh` runs world-hashes `local` over the five presets.
- `slow.sh` runs the slow suites.
- `run-sweep.sh` and `sweep-job.sh` run 375 worlds with `jobs.txt`, the same sets as the P3-08 validation:
  - 1024 seeds 0–99 at default settings;
  - 512 seeds 0–29;
  - 2048 seeds 0–4;
  - 1024 seeds 0–19 each for span 2000 and 12000, sparse, dense, calm, turbulent, civs 1 and 4, `elf,orc`, `elf,goblin`, `elf`, and all races but elves.
- Each sweep world gets: generate, `hash --detail`, `report`, the C++ sentence dump (`p3-08-dump`), the region-name reference, `check09.py`, and the conflict reference's `check-world` (also the peoples reference's for `elf`).
- `old09.sh` runs `check09.py` over the 375 0.17.0 worlds, for the comparison columns.
- `stats09.py` writes `sweep/stats09.txt`.
- `cession_scan.py` looks for D1.
- `purity.sh` and `timing.sh` produce `purity/` and `timing/`.
- `critic/make.sh` builds the critic folder.

**`check09.py`.** This is my independent check from the rows, written from P3-06 20, P3-05 21 and P3-07 21.8. It extends `check08.py`, keeping its P3-05 and P3-06 checks with the 0.18.0 readings. It adds:
- kinds 32–36 and their causes;
- the flight (20.1, 20.13 item 3), tributes and 20.3 cessions (20.13 item 4);
- weariness invariant 12 (20.23), recomputed conservatively from the log and `region_control`;
- T1, T2, T5, T6 and T7 from the rows;
- invariants 10 and 11 by replaying the `hist.conflict.search` draw;
- the race-level floor (P3-05 21 item 6).

T7 is checked without recomputing forces. Since dead_W = min(F_W, F_L) × `loss_winner` div 1000, the bound holds when dead_L × `loss_winner` < (dead_W + 1) × max(`kill_max`, `loss_loser`). Its hollow, unfound and same-goal statistics agree with the reference's `[stats]` in every world except the four of D3.

## 2. Tests

### 2.1 Failing set before recording

On every preset only `worldgen-tests-other` and `worldgen-cli-tests-other` failed in round 1. The failing checks were all world-hash constants:
- `db_determinism_test.cpp`: the 5 empty worlds without content;
- `stages_pipeline_test.cpp`: 2 checks;
- `cli_golden_test.cpp`: `p0-worlds.txt`.

The set of failing check lines (`values:`) has the same MD5 on all five presets (`4246479c…`), so the failing values are identical. After recording, all five presets pass 33 of 33 (round 2).

### 2.2 Times

These are full `ctest` times, four jobs. Round 2 ran beside a two-process recheck of the sweep, at load average 5–9.

| Preset | Round 1 | Round 2 | Longest entries (round 2) |
| --- | --- | --- | --- |
| `linux-gcc-release` | 146 s | 133 s | `-chronology-property` 32.0 s, `-hydrology` 29.1, `cli-chronology` 28.0, `-elevation-golden` 27.4, `-balance` 26.2 |
| `linux-clang-release` | 148 s | 133 s | `-chronology-property` 32.7 s, `-hydrology` 29.3, `-balance` 26.4 |
| `linux-gcc-debug` | 591 s | 527 s | `-hydrology` 150 s, `-chronology-property` 140, `-balance` 113 |
| `linux-clang-debug` | 542 s | 529 s | `-hydrology` 150 s, `-balance` 114 |
| `linux-clang-asan` | 2342 s | 2309 s | `-hydrology` 680 s, `-chronology-property` 586, `-elevation` 522, `-balance` 513 |

- On release, `worldgen-tests-chronology-property` is above 30 s, and only under load; it was not touched by P3-09.
- No asan entry is above a third of its 3600 s timeout (1200 s).
- The new entry `worldgen-tests-balance` takes 26 s on release, 113 s on debug and 513 s under asan.

**Slow suites (serial, both release presets run side by side with the sweep).**

| Preset | `worldgen-tests` | `worldgen-cli-tests` |
| --- | --- | --- |
| `linux-gcc-release` | 2312 s | 299 s |
| `linux-clang-release` | 2403 s | 273 s |

## 3. What must not change: 0.17.0 against 0.18.0

**World-hash cases** (detail hashes, `linux-gcc-release`, against the P3-08 files):
- Empty and pattern worlds differ only in `meta`: `content_hash` (`f785185e…` → `93084b3e…`) and `generator_version`. The empty world's file is 139,264 bytes in both.
- Geography worlds and history worlds keep every layer and every geography table (`biome`, `climate`, `pass`, `region`, `region_biome`, `region_border`). The same holds for `race`, `race_biome`, `culture`, `age` and `god`.
- The history tables differ, as do `meta` and the catalogue.

**The catalogue** has 36 kinds against 31:
- 32 `refugees_settled` 65, 33 `tribute` 50, 34 `war_won` 60, 35 `search_abandoned` 40, 36 `refuge_held` 25;
- `claim`'s salience goes from 45 to 35;
- kinds 1–31 are otherwise unchanged.

In `hist-dense-1024` the first differing event is e125, the first claim (salience 45 → 35, and its region by 20.2's goal rule).

**The sweep, 375 worlds, table by table against the same world at 0.17.0:**
- Every layer and every geography table is identical in all 375. So are `race`, `race_biome`, `culture` and `age`.
- `god` differs in 4 worlds, all through the word list. These are god 2, the goblins' creator:
  - default seed 28 and 512 seed 28: Kazank → Gnut-Snag;
  - default seed 65: Kaz-Skog → Drab-Keeb;
  - default seed 86: Geetchkork → Notch-Zub.
- `event_kind`, `event_kind_role`, `event_kind_value` and `meta` differ in all 375.
- `event` differs in 351. The 24 that do not include the 20 elf-only worlds.
- The other history tables differ in 322–354.

**Names changed by the 393-word list.** This counts entities whose first filter-valid attempt differs between the 647-word list of 0.17.0 and the 393-word list, a measure independent of the history:
- 1024 defaults: 180 of 11,603 names (god 3, civilization 14, settlement 163), 1.6%;
- all 375 worlds: 593 of 40,100;
- the elf-only worlds: 0;
- examples: Wild, Nix Hole, Snag, Drab, Notch.

**Per-stage vectors.** Against `origin/main`, the changed golden files are `p3-01-data.txt`, `p3-02-names.txt`, `p3-04-engine.txt`, `p3-05-peoples.txt` and `p3-06-conflict.txt`, as section 6 lists. The six P1 vector files and `p3-03-chronology.txt` are unchanged. I regenerated no vector.

**Reading a 0.17.0 world.** The 0.18.0 binary reads the 0.17.0 `hist-dense-1024` world:
- `report` and `hash --detail` are byte-identical to 0.17.0's own output;
- `render` of the `map`, `regions` and `biomes` composites and of a layer exits 0.

## 4. Acceptance targets (P3-09 5, r3 and r4)

T1 is defined at 1024 defaults. I report every set and mark each miss.

- **T1** is the hollow wars over all the set's wars that found their enemy (P3-07 21.8 item 2), in permille. "Above 250" counts the worlds at or above 250.
- **Unfound** is the number of wars ended by `search_abandoned`.
- **T2** gives the worlds with `wars_same_goal_unchanged_max` above 3 (21.8 item 5), then the violations of invariant 12.
- **T3** gives `refuge_held` of reason 1, then those whose region has free, non-islet regions with capacity within `flight_hops` by my recomputation without the site rule, then reason 2. The reference recomputes with sites and finds no destination for any of them.
- **T4** counts the worlds with a race present that went extinct.
- **T5** counts orc civilizations as winner and as loser of `peace` or `war_won`.
- **T6** counts the endings decided by score whose loser won the last battle, with endings by aim in brackets (allowed).
- **T7** counts battles above the kill bound.

| Set | Worlds | T1 hollow ‰ (hollow / found) | Above 250 | Unfound | T2 | T3 | T4 | T5 won / lost | T6 | T7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **default 1024** | 100 | **229** (479 / 2086) **met** | 41 | 437 | **2** / 0 (D2) | 1 (0) / 0 | 0 | **115 / 68 met** | 0 (17) | 0 |
| 512 | 30 | 293 (128 / 436) **above** | 19 | 66 | 0 / 0 | 58 (2) / 0 | 0 | 29 / 17 | 0 (3) | 0 |
| 2048 | 5 | 140 (29 / 206) | 1 | 29 | 0 / 0 | 0 / 0 | 0 | 13 / 5 | 0 | 0 |
| span 2000 | 20 | 327 (39 / 119) **above** | 14 | 26 | 0 / 0 | 0 / 0 | 0 | 3 / 5 **lost more** | 0 | 0 |
| span 12000 | 20 | 148 | 3 | 138 | 0 / 0 | 6 (6) / 0 | 0 | 77 / 38 | 0 (4) | 0 |
| sparse | 20 | 200 | 5 | 54 | 0 / 0 | 0 | 0 | 27 / 17 | 0 | 0 |
| dense | 20 | 186 | 6 | 100 | 0 / 0 | 0 | 0 | 16 / 15 | 0 (2) | 0 |
| calm | 20 | 253 (63 / 249) **above** | 11 | 49 | 0 / 0 | 0 | 0 | 24 / 9 | 0 | 0 |
| turbulent | 20 | 200 | 5 | 110 | 0 / 0 | 0 | 0 | 22 / 15 | 0 (11) | 0 |
| civs 1 | 20 | 247 | 10 | 39 | 0 / 0 | 0 | 0 | 21 / 17 | 0 | 0 |
| civs 4 | 20 | 207 | 5 | 187 | **2** / 0 (D2) | 5 (3) / 0 | 0 | 18 / 22 **lost more** | 0 (20) | 0 |
| `elf,orc` | 20 | 314 (17 / 54) **above** | 10 | 0 | 0 / 0 | 0 | 0 | 24 / 9 | 0 | 0 |
| `elf,goblin` | 20 | 256 (37 / 144) **above** | 9 | 0 | 0 / 0 | 0 | 0 | — | 0 | 0 |
| `elf` | 20 | no wars | — | 0 | — | — | 0 | — | — | — |
| no elves | 20 | 203 | 7 | 119 | 0 / 0 | 0 | 0 | 24 / 14 | 0 (1) | 0 |

**Every invariant holds in every world**, except D1's region and the four runs of D2:
- **20.13 items 1, 3, 4:** territory changes, flights and tributes;
- **20.13 items 5, 6 and 20.19 (7)–(11):** weariness, concealment, the peace's exhausted loser, the last-battle rule, the kill bound, the searches;
- **20.23 (12):** weariness counting open wars;
- **P3-05 21 item 6:** a race's last refuge is never abandoned;
- **P3-06 14:** the invariants of conflict.

The reference checks invariants (7)–(11) by recomputation and found no failure in 374 worlds. The 375th, d2048 seed 3, fails only on D1. Two further results:
- **T6 at the war's end.** No `peace` lacks an exhausted loser, by the reference.
- **No race extinct anywhere.** This includes turbulent (20 worlds) and civs 4 (20).

## 5. Statistics

### 5.1 1024 defaults: 0.18.0 against 0.17.0

Figures are min / median / max over 100 worlds.

| Statistic | 0.18.0 | 0.17.0 |
| --- | --- | --- |
| events | 251 / 726 / 1348 | 300 / 652 / 1629 |
| wars | 4 / 23 / 53 | 4 / 22.5 / 67 |
| endings: `peace` | 0 / 7 / 20 | 4 / 16.5 / 51 |
| endings: `war_won` | 0 / 5 / 16 | — |
| endings: `truce` | 0 / 5 / 21 | 0 / 5 / 17 |
| endings: `search_abandoned` | 0 / 4 / 12 | — |
| endings: conquest | 0 / 1 / 6 | 0 / 1 / 5 |
| wars open at the present | 0 / 0 / 3 | 0 / 0 / 3 |
| battles | 7 / 76 / 202 | 10 / 79.5 / 266 |
| battles involving faeries | 0 / 2 / 8 | 0 / 8.5 / 63 |
| sacked | 0 / 10.5 / 30 | 2 / 13 / 35 |
| taken | 0 / 7 / 26 | 0 / 6.5 / 29 |
| cessions (all); of 20.3 | 0 / 6 / 18; 0 / 4 / 14 | 0 / 2 / 7; — |
| tributes | 0 / 5 / 19 | — |
| flights (`refugees_settled`) | 0 / 0 / 3 | — |
| `refuge_held` reason 1; reason 2 | 0 / 0 / 1; 0 | — |
| `wars_hollow_permille` | 0 / 226 / 1000 | 0 / 415.5 / 727 |
| `wars_same_goal_max` | 1 / 3 / 11 | 2 / 6 / 16 |
| `wars_same_goal_unchanged_max` | 1 / 3 / 4 | 1 / 5 / 16 |
| largest race share of people ‰ | 290 / 596.5 / 875 | 289 / 568.5 / 850 |
| largest race share of held regions ‰ (reference `[stats]`) | 222 / 467 / 804 | — |
| largest civilization share of non-islet land (max over the history) ‰ | 106 / 211.5 / 382 | 86 / 182 / 340 |
| goblin–elf wars | 0 / 7 / 25 | 0 / 7 / 27 |
| years from the goblins' awakening to the first war | 225 / 812.5 / 3691 | 75 / 837.5 / 3554 |
| longest stretch without a declaration after it | 546 / 1100 / 3691 | 631 / 1121.5 / 3554 |
| claims; flare-ups | 7 / 37 / 89; 0 / 3 / 10 | 6 / 26.5 / 90; 0 / 2 / 7 |
| civilizations; ended | 7 / 10 / 16; 0 / 1 / 4 | 7 / 10 / 17; 0 / 1 / 4 |
| simulated years; ‰ | 1168 / 1324.5 / 1629; 194 / 220.5 / 271 | 1112 / 1274.5 / 1474; 185 / 212 / 245 |
| summary threshold; events shown | 20 / 31 / 36; 119 / 206 / 250 | 20 / 31 / 51; 126 / 191 / 250 |

Counts over the 100 worlds:
- **Flights.** A flight occurs in **37 of 100** worlds (46 flights); the audit saw few. `refuge_held` occurs in 1 world (reason 1).
- **Other endings and costs.** Tribute occurs in 94 worlds (542 in all), `search_abandoned` in 92 (437) and `war_won` in 98 (573).
- **Attackers.** Goblins declare 1922 wars, orcs 308, humans 147, dwarves 105, elves 36 and faeries 5. At 0.17.0 the figures were goblins 2048, orcs 282, humans 131, dwarves 63, elves 0 and faeries 0. Goblins declare 76% of all wars.
- **The orc age.** It has 1170 wars, of which orcs declare 263‰ and goblins 530‰ (0.17.0: 1317 wars, 214‰ and 659‰).
- **Orc wars.** Orcs win 115 and lose 68 (0.17.0, `peace` only: 46 and 179).
- **The goblin–elf feud.** Worlds without a goblin–elf war: 11 of 100 (0.17.0: 21).
- **Largest race share of people.** It is at or above 600 in 50 worlds and at or above 700 in 27 (0.17.0: 45 and 18).
- **Largest race share of held regions.** It is at or above 600 in 26 worlds and at or above 700 in 11.
- **Summary.** Plague's share of the shown events is 108‰ (P3-08 measured 83‰).

### 5.2 Standing settlements per race at the present (1024 defaults)

Figures are min / median / max; the last column counts worlds where the race ends with exactly one settlement.

| Race | 0.18.0 | Worlds with 1 | 0.17.0 | Worlds with 1 at 0.17.0 |
| --- | --- | --- | --- | --- |
| faerie | 2 / 7 / 20 | **0** | 1 / 4 / 19 | 23 |
| elf | 1 / 8 / 42 | 2 (seeds 8, 23) | 1 / 10 / 44 | 8 |
| orc | 1 / 6 / 18 | **7** (seeds 7, 14, 20, 36, 81, 94, 95) | 1 / 5 / 15 | 2 |
| human | 1 / 9 / 49 | 6 (seeds 8, 23, 33, 46, 75, 97) | 1 / 12 / 39 | 4 |
| dwarf | 3 / 5.5 / 29 | 0 | 2 / 5 / 28 | 0 |
| goblin | 1 / 37 / 114 | 1 (seed 41) | 4 / 34.5 / 109 | 0 |

### 5.3 The other sets

Figures are medians, with the 0.17.0 median in brackets. Full min / median / max tables for every set are in `sweep/stats09.txt`.

| Set | Events | Wars | Hollow ‰ | Battles | Faerie battles | Cessions | Tributes | Worlds with a flight | Worlds with `refuge_held` | Orc age: orc / goblin share ‰ | No goblin–elf war |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 512 | 404 (526) | 16.5 (28) | 281 (588) | 52 (98.5) | 4 (20.5) | 2 (1) | 4.5 | 16 of 30 | 10 (58 held, reason 1) | 392 / 301 (321 / 484) | 4 (4) |
| 2048 | 1861 (650) | 42 (11) | 94 (400) | 163 (50) | 1 (0) | 15 (1) | 5 | 0 of 5 | 0 | 181 / 514 | 0 (2) |
| span 2000 | 210.5 (202.5) | 7 (5.5) | 285 (585) | 14.5 | 0 | 1 (0) | 1 | 6 of 20 | 0 | 246 / 753 | 1 (5) |
| span 12000 | 1458 (1486.5) | 46 (58.5) | 164 (362.5) | 162.5 (216) | 3 (15.5) | 13.5 (4.5) | 10 | 17 of 20 | 1 (6) | 369 / 341 | 1 (6) |
| sparse | 529 (517.5) | 17.5 (15) | 159.5 (367.5) | 60.5 (50) | 1.5 (5.5) | 6 (2) | 4 | 4 of 20 | 0 | 268 / 527 | 2 (3) |
| dense | 786 (766.5) | 23.5 (25) | 190.5 (366) | 85.5 | 2 (6.5) | 8.5 (2) | 4 | 14 of 20 | 0 | 217 / 595 | 0 (1) |
| calm | 425.5 (425) | 14 (14) | 261 (473) | 39.5 | 1 (4.5) | 4 (1) | 3 | 3 of 20 | 0 | 312 / 568 | 3 (3) |
| turbulent | 1166.5 (1081.5) | 32.5 (39.5) | 215 (431) | 113.5 (139) | 2 (11) | 10 (3) | 8 | 12 of 20 | 0 | 140 / 575 | 0 (1) |
| civs 1 | 424 (445) | 11 (10.5) | 245 (428) | 33.5 | 0 (4) | 3 (0) | 2 | 7 of 20 | 0 | 377 / 352 | 10 (13) |
| civs 4 | 2139.5 (1838.5) | 87 (95.5) | 198 (394) | 275 (316) | 6 (26) | 22.5 (6) | 24.5 | 2 of 20 | 1 (5) | 181 / 488 | 0 (0) |
| `elf,orc` | — | — | 314 | — | — | — | 4 in all | 0 | 0 | — | — |
| `elf,goblin` | — | — | 256 | — | — | — | 22 in all | 0 | 0 | — | 2 (4) |
| no elves | — | — | 203 | — | — | — | 45 in all | 4 of 20 | 0 | 378 / 388 | — |

**Worlds where a race ends with one settlement.**

| Set | faerie | elf | orc | human | dwarf |
| --- | --- | --- | --- | --- | --- |
| 512 | 3 (0.17.0: 15) | 2 (7) | 5 (4) | 3 (6) | 1 (2) |
| span 2000 | 4 (8) | 0 | 16 (18) | 13 (14) | 1 (1) |
| span 12000 | 0 (7) | 1 (6) | 0 (3) | 1 (1) | 0 |
| sparse | 1 (3) | 0 | 1 (0) | 0 | 0 |
| dense | 0 (5) | 0 (2) | 0 (1) | 0 | 0 |
| calm | 0 (2) | 0 (1) | 0 | 0 | 0 |
| turbulent | 0 (5) | 0 (2) | 0 (4) | 0 (1) | 0 |
| civs 1 | 0 (4) | 0 (1) | 1 (0) | 0 | 0 |
| civs 4 | 0 (9) | 0 (1) | 0 | 1 (3) | 0 |
| 2048 | 0 | 0 (1) | 1 (seed 3) | 0 | 0 |
| no elves | 0 (7) | — | 2 (0) | 0 | 0 |

**2048 under the new automatic rule** (P3-05 21 item 5):

| Measure | 0.18.0 | 0.17.0 |
| --- | --- | --- |
| civilizations per world | 24 / 27 / 31 | 8 / 12 / 14 |
| events | 1172 / 1861 / 2444 | 513 / 650 / 821 |
| wars | 29 / 42 / 73 | 2 / 11 / 17 |
| summary threshold | 36 / 46 / 56 | 26 / 26 / 31 |

At 2048, every world has a goblin–elf war, and no race holds 600‰ of the people.

**Largest race share of people at or above 600 and 700:**

| Set | ≥ 600 | ≥ 700 |
| --- | --- | --- |
| 512 | 7 | 1 |
| span 2000 | 10 | 7 |
| span 12000 | 10 | 4 |
| sparse | 5 | 1 |
| dense | 7 | 5 |
| calm | 5 | 1 |
| turbulent | 7 | 4 |
| civs 1 | 7 | 3 |
| civs 4 | 9 | 3 |
| `elf,orc` | 13 | 7 |
| `elf,goblin` | 17 | 15 |

### 5.4 Typical ranges for P3-06 15.5 and 20.11 (5th–95th percentile, 1024 defaults, 100 worlds)

These are for the spec writer to adopt (P3-06 20.25 item 1):

| Statistic | 5th – 95th percentile | Median |
| --- | --- | --- |
| events | 382 – 1216 | 726 |
| wars | 7 – 43 | 23 |
| flare-ups | 1 – 7 | 3 |
| claims | 14 – 73 | 37 |
| battles | 20 – 168 | 76 |
| battles involving faeries | 0 – 7 | 2 |
| sacked | 2 – 22 | 10.5 |
| taken | 1 – 21 | 7 |
| cessions (all) | 1 – 15 | 6 |
| tributes | 0 – 12 | 5 |
| flights | 0 – 2 | 0 |
| endings: `peace` | 1 – 16 | 7 |
| endings: `war_won` | 1 – 12 | 5 |
| endings: `truce` | 1 – 12 | 5 |
| endings: `search_abandoned` | 0 – 9 | 4 |
| endings: conquest | 0 – 3 | 1 |
| `wars_hollow_permille` | 52 – 466 | 226 |
| `wars_unfound` | 0 – 9 | 4 |
| `wars_same_goal_max` | 2 – 7 | 3 |
| `wars_same_goal_unchanged_max` | 2 – 3 | 3 |
| `race_population_share_max_permille` | 368 – 835 | 596.5 |
| `race_regions_share_max_permille` | 269 – 737 | 467 |
| largest civilization land share (max over the history) | 127 – 334 | 211.5 |
| civilizations | 8 – 14 | 10 |
| civilizations ended | 0 – 2 | 1 |
| simulated years | 1196 – 1481 | 1324.5 |
| `simulated_permille` | 199 – 247 | 220.5 |
| summary threshold | 26 – 36 | 31 |
| first war after the goblins' awakening (years) | 299 – 2959 | 812.5 |

Standing settlements at the present:

| Race | 5th – 95th percentile | Median |
| --- | --- | --- |
| faerie | 4 – 15 | 7 |
| elf | 2 – 26 | 8 |
| orc | 1 – 13 | 6 |
| human | 1 – 28 | 9 |
| dwarf | 4 – 18 | 5.5 |
| goblin | 4 – 94 | 37 |

**Against 20.11's typical ranges:**

| Range | Measured | Result |
| --- | --- | --- |
| `wars_hollow_permille` below 250 | 229 over the sweep | met |
| `wars_same_goal_unchanged_max` at most 3 in at least 90% of worlds | 98 of 100 | met |
| `race_population_share_max_permille` below 700 in at least 75% of worlds | **73 of 100** | **narrowly missed** (F1) |
| faeries with one settlement in at most 5% of worlds | 0 | met |
| elves with one settlement in at most 5% of worlds | 2 | met |

**Against the prototype (P3-06 20.18, `L0`, seeds 0–39).** Every median is within 20% of `L0` except these two:
- battles involving faeries, median 2 against 3 (small counts);
- worlds without a goblin–elf war, 11% against 5% (F2).

The medians within 20%, with `L0` in brackets:
- events 726 (690);
- wars 23 (23.5);
- hollow 229 (220);
- faerie standing settlements 7 (7) and elven 8 (8);
- goblin share of all wars 76% (73%);
- orc share of the orc age 26% (27%);
- simulated years 1324 (1311);
- unfound wars 4.4 per world (4.2).

## 6. Purity, settings and timing

- **Threads.** `--threads` 1, 2 and 6 at 1024 seed 0 give byte-identical files. At 512 seed 42, `--threads` 1 and 5 are byte-identical and equal the golden `geo-seed-42-512`. (The purity script's own 512 pair passed `--seed` twice and was refused with exit 2. I reran it by hand.)
- **`--until history`** is byte-identical to the default.
- **Settings change only what they should** (against seed 0 1024):
  - `--monster-pressure high` and `--divine-activity meddling` change only `meta`;
  - `--until chronology` drops the history tables;
  - `--history-span 2000` and `--races elf` change `race`, `age` and the history. The catalogue and `god` are unchanged.
- **Timing.** `generate` at seed 0, three runs each, with the same thread default. Machine load average was 1.9–3.7 throughout: a background load of about 2 is present on this machine without any of my jobs.

| Map | 0.17.0 | 0.18.0 |
| --- | --- | --- |
| 512 | 2.63–2.69 s | 2.64–2.65 s |
| 1024 | 9.12–9.20 s | 9.13–9.19 s |
| 2048 | 36.85–37.09 s | 37.07–37.31 s |

No measurable change. The 2048 world has 2.3 times the civilizations and 2.9 times the events, and still takes the same time: geography dominates.

## 7. Recorded hashes

`worldgen/tests/golden/p0-worlds.txt` (C-locale order, LF):

```
empty-phrase afef0cec990417d2e17c5cd273d038989e38a2bcaa26272472c561af0a33351d
empty-seed-0 37c688d40be935d0e604cab5154d2868ca61661ffabbdcd5c7cc069530a830f9
empty-seed-0-512 a411f77a6e49129eb112528b0abb085d275e1eec0f363806ee15aebcd162f468
empty-seed-1 a91e49e60c4793fc891895ddc2702b369bdf0b8917efaee58d9fda041deb631e
empty-seed-42 74a76a49c5eae545c47f09bcf78b7bc7b330bb2a52278a51b53b4f798059dd26
empty-seed-max 029c40415a8d39fc545b3a227d0b872c488c80dec9565b5d8875105cdf940bbb
pattern-seed-0 68603f64316e2241e3517958696298226dd70d2d50007636a5022d087b0c0dd8
pattern-seed-42 4a0c896557d6d01c3aab2b53d41f3d689f232a13f7dbe466934d7dce338141b5
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 75ff8ad713c747da1402d4fc1b4d39bbb7299cf8a5f1323ac8fd0b4e80d84586
geo-seed-0-512 daf03635368ca1be171e2312a229d814dc471e27e81a153aadf0f571b77849c2
geo-seed-1-1024 9c04fd8a12187256e96634072bec02038ce3b263aa32fd74ad929a45f49d0f9f
geo-seed-42-512 e903c12fcb785e8e92c9523a63796bec10950c3762197880876e49b53bfa9e7f
```

`worldgen/tests/golden/p3-worlds.txt`:

```
hist-dense-1024 c709e94f100d88a0bdd27dab24e92dee8aa4a3d6de86a1ab62e402e0af55aeb3
hist-races-elf-orc-1024 de29706e837064a649a985eaea399e2a54a6db8c68b5d442777584197355d0ab
hist-span-2000-512 90626e81dc1cd467b2b912c446c5e39a8131c975dbf38819ea8e03a613443fce
```

**Inline constants.** These are the empty worlds with the content hash of no data, generator 0.18.0.
- `worldgen/tests/db_determinism_test.cpp`:
  - empty-phrase `1fb40d5b8da2e0aca2f5d877fb4660a2b7f1a72dfd7e2be6492194e416b1c7b9`;
  - empty-seed-0 `c6f3de139282dfc48cfba8b43eb3ace14ccae5b4a4eae2d99bced1ec7c5a5890`;
  - empty-seed-1 `86cf147a4beb61831141f33b3b452ffdb7ffe604d5bf5628ab997f9d58a0122b`;
  - empty-seed-42 `2a3df85ca8d415c0c492c96144ca41ca34697005bd90e6934d98fb64e3571a52`;
  - empty-seed-max `1db2527bf4d1117693a87adb1c38d5690fcb65d19269cc7624e2fe54919b1165`.
- `worldgen/tests/stages_pipeline_test.cpp`:
  - the empty world `c6f3de13…`;
  - the test pattern `aa9545db2d7cbd9a6a9f9cdb9a6cbe86812c78001ec3b4989afbacaaf0d61bfa`.

No other test holds a world hash: the remaining 64-hex constants are generator-version independent and passed unchanged.

**Why they changed.** `GENERATOR_VERSION` is 0.18.0 in `meta` of every world, and the shipped data changed:
- race fields;
- 11 + 3 `history.conflict` fields and 2 `history.peoples` fields;
- the report entries;
- the word list.

Together these change every `content_hash`. History worlds also change from their first claim.

**README phrase world.** `generate --seed-phrase "The Unmapped"` hashes to `255e4cb0b4c5b6fa19603abfd48be2827ab5fa23cf2eeda23a6f4f0e156956cc`. Its report is `~/.cache/the-unmapped/out/p3-09-validation/readme-report.txt`: 11 civilizations, 687 events, 36 kinds. I did not edit the README; its example still shows 0.17.0.

## 8. Critic folder

`~/.cache/the-unmapped/out/p3-09-validation/critic/` holds the same thirteen worlds as the P3-08 folder:
- 1024: seeds 42, 0, 1, 2, 3, 7, 11, 23;
- 1024 seed 42 turbulent and seed 42 `--civs-per-race 4`;
- 512: seeds 42 and 5;
- 2048: seed 42.

Each has `.db`, `.report.txt`, `.regions.png`, `.map.png`, `.chronicle.txt`, `.summary.txt`, `.sentences.txt` and `.names.txt`, in the P3-08 formats (`chronicle09.py`). The war lines add the outcomes:
- `peace` or `won` e<n>, with ", ceded <region>" and ", tribute <n>";
- `truce`;
- `unfound`, with "never found in <region>";
- ", N refuge held" in the counts.

The end state lists flights (`refugees_settled`) and held refuges (`refuge_held`). Two of the thirteen worlds have a flight; none has a held refuge.

## 9. Defects and findings

### D1 (blocking; owner: spec writer, then engineer and test writer). A 20.3 cession transfers a region whose only settlement of the loser is a same-step colony

The region is then held for 760 years by a civilization with no settlement in it.

**Seed and version.** Seed 3, 2048, default settings, generator 0.18.0.

**Reproduce:**

```
worldgen-cli generate --seed 3 --map-size 2048 --data game/data --out s3-2048.db --threads 1
python3 -B worldgen/tests/reference/p3_conflict_reference.py --version 0.18.0 check-world s3-2048.db --data game/data
```

The reference reports "FAIL in-run invariants: 14 item 4: region 158 held by 10 without a settlement in 5234 … 5993".

**Expected.** A held region has a standing settlement of its holder: P3-05 14 item 4, P3-06 14 item 5, and the claim of P3-06 r7 8.3 item 4 that "invariant 4 of P3-05 and 5 of 14 still hold (W has a settlement in G)".

**Actual.** The events, in order:
1. In 5225 (war e1109, goblins c15 on dwarves c10, goal region 138), c10 wins a battle in region 158 and sacks c15's town s168 (e1122). Region 158 is released.
2. In the quiet step of 5234, peoples founds colony s240 of c15 in region 158 (e1138), so c15 holds 158 again.
3. Later in the same quiet step the war ends: `war_won` e1141, winner c10, the defender.
4. 20.3 item 1 cedes c15's "best non-capital region in front": region 158 (`region_ceded` e1142).

The colony is invisible in that step (8.3 item 4), so it is not transferred. `region_control` gives 158 to c10 from 5234 to 5994 while s240 stays c15's: thriving, a town by 5384. c10 has no settlement in 158 for those 760 years.

`cession_scan.py` over all 375 sweep worlds and the 13 critic worlds finds this case only, and none in the 0.17.0 sweep.

**Root cause.** P3-06 r7's "same-step colony in a ceded region" text assumed the goal cession of 7.7, where the winner is the attacker and the goal is the war's region. 20.3 item 1 adds cessions of any region L holds in `front(W, L)`, including one that L holds only through a same-step colony. Then neither W nor any visible settlement of L moves into it.

**Possible rules for the spec writer** (not my call):
- exclude from 20.3's candidates a region in which L has no visible settlement;
- or transfer invisible settlements too.

The C++ and the reference agree row for row, so both implement the text as written.

### D2 (owner: spec writer). T2's statistic and weariness count different wars

**Worlds.** 1024 defaults seeds 6 and 77; `--civs-per-race 4` seeds 0 and 13.

**Reproduce:** `worldgen-cli generate --seed 6 --map-size 1024 --data game/data --out s6.db`, then read `[stats]` from `p3_conflict_reference.py --version 0.18.0 check-world s6.db --data game/data`.

**Expected.** P3-06 20.23: "In consequence `wars_same_goal_unchanged_max` ≤ `weary_wars` in every world".

**Actual.** `wars_same_goal_unchanged_max` = 4, by both the reference and my check. Invariant 12 holds. In seed 6 the run is the four wars over region 40 against civilization 1, with region 40 held by civilization 1 throughout:

| War | Years | Ending |
| --- | --- | --- |
| e341 | 4892–4922 | `war_won` |
| e445 | 5297–5309 | `peace` |
| e453 | 5310–5338 | `civ_conquered` (the attacker's end, e488) |
| e538 | 5591–5597 | `truce` |

20.4 counts a war toward weariness only when it ends by peace, truce (and `war_won` and `search_abandoned`). A war closed by the end of a side never counts. P3-07 21.8 item 5's run counts every war. The other three worlds have the same shape:
- seed 77: three `search_abandoned` and one `civ_conquered`;
- civs4 seed 0: e1731 `civ_conquered`;
- civs4 seed 13: e1359 `civ_conquered`.

**Not an engine defect.** The C++ follows 20.23. The typical range of "≤ 3 in at least 90% of worlds" is met (98 of 100).

**Decide:** either count a war closed by a side's end as futile in 20.4, or exclude such wars from 21.8 item 5's run, and then restate T2.

### D3 (owner: test writer). The reference's `wars_same_goal_unchanged_max` undercounts by one when a change of owner lies between two wars

**Location.** `worldgen/tests/reference/p3_conflict_reference.py` `stats21_lines`.

**Reproduce:** `worldgen-cli generate --seed 17 --map-size 1024 --event-intensity calm --data game/data --out c17.db`, then run `check-world`. It prints `wars_same_goal_unchanged_max = 1`.

**Expected** (P3-07 21.8 item 5): **2**. The run is wars e285 (5577–5587) and e297 (5588–5598) over region 11 against civilization 1. `region_control` rows of region 11 change at 5039 and 5857 only, so no row has `from_year` or `to_year` in [5577, 5598].

**Cause.** When a change falls between the previous war and the current one (window [min(lo, prev_hi), hi]), the code sets `run = 0` and does not count the current war, which starts a new run of 1. It should set `run = 1` when the change is before `lo` and `run = 0` only when it is within [lo, hi].

**Effect.** Four of 375 worlds report one less: calm seed 17, `elf,goblin` seed 6, no-elves seed 12, 2048 seed 0. None of them crosses 3, so no T2 result changes. The reference's T2 checks use this statistic, so the gap could hide a run.

### Findings (no defect)

- **F1.** `race_population_share_max_permille` is at or above 700 in 27 of 100 default worlds. 20.11's range is at most a quarter (0.17.0: 18). The dominant race is the goblins in 21 of those worlds and the humans in 6; the costs of war move people to the strong, as decision 8.4 expected.
- **F2.** Worlds without a goblin–elf war: 11 of 100. The prototype `L0` had 2 of 40, and 0.17.0 had 21 of 100. With one civilization per race it is 10 of 20 (0.17.0: 13).
- **F3.** T1 is defined at 1024 defaults and met there (229‰). Above 250 elsewhere:

  | Set | Hollow ‰ |
  | --- | --- |
  | 512 | 293 (the engineer measured 300) |
  | span 2000 | 327 |
  | `elf,orc` | 314 |
  | `elf,goblin` | 256 |
  | calm | 253 |

  At 1024 defaults, 41 of 100 worlds are at or above 250 individually.
- **F4.** T5 is defined at 1024 defaults and met there (115 against 68). It is missed in civs 4 (18 won against 22 lost) and span 2000 (3 against 5).
- **F5.** Orcs end with one standing settlement in 7 of 100 default worlds (0.17.0: 2). Humans do in 6 (0.17.0: 4). Faeries now never do (0.17.0: 23), and elves in 2 (0.17.0: 8). No race went extinct in any set; turbulent and civs 4 have no race with one settlement except humans in civs 4 seed 12.
- **F6.** 2048 is no longer peaceful. The automatic rule founds 24–31 civilizations (8–14 at 0.17.0), with 1861 events and 42 wars (medians). The summary threshold rises to 36–56, so P3-07's companions matter there, as P3-09 5.7 predicted for four civilizations per race.
- **F7.** At 512, 10 of 30 worlds have a refuge held for want of land: 58 `refuge_held`, all reason 1. My row recomputation without the site rule finds free regions in reach for 2 of them (and 6 in span 12000, 3 in civs 4). The reference's recomputation with P3-05 6.2's sites finds no destination for any, so T3 holds.
- **F8.** On release, `worldgen-tests-chronology-property` took 32 s under load (above the 30 s mark); it is not part of this change. `worldgen-tests-balance` is 26 s on release and 513 s under asan.
- **F9.** `CLAUDE.md`'s CTest table does not list `worldgen-tests-balance`. The shard exists in `worldgen/tests/CMakeLists.txt`. The README example still shows the 0.17.0 phrase world. Both are for the lead; I edited neither.

## 10. For the lead

1. **D1 needs a rule.** Fixing it changes worlds where it applies: a re-record at 0.18.x, or a decision to merge and schedule.
2. **D2 needs a choice.** Pick which definition of a futile war T2 and weariness share.
3. **D3 is the test writer's,** independent of 1 and 2.
4. **Adopt the typical ranges** of 5.4 into P3-06 15.5 and 20.11 (spec writer), and decide whether F1 (27% of worlds with a race at ≥ 700‰ of the people) is acceptable.
5. **Update `CLAUDE.md` and the README** at the merge (F9).

## 11. Files

Under `~/.cache/the-unmapped/out/p3-09-validation/`:

| Path | Contents |
| --- | --- |
| `logs/summary.txt` | every step's exit status and time |
| `logs/ctest1-*.log`, `logs/ctest2-*.log` | ctest rounds 1 and 2 |
| `logs/slow-*.log` | slow suites |
| `logs/world-hashes-local.log` | the cross-preset comparison |
| `logs/expect-*.log` | `expect` per preset |
| `logs/gdext-*.log` | extension build and headless check |
| `logs/lint*.log` | lint |
| `sweep/<set>/` | per world: `.db`, `.txt`, `.report.txt`, `.sent.txt`, `.regnames.txt`, `.check.json`, `.ref.txt` (and `.pref.txt` for `elf`) |
| `sweep/stats09.txt` | targets and statistics |
| `sweep/ref-results.txt` | reference results |
| `sweep/cession_scan.txt` | D1 scan |
| `old09/` | `check09.py` on the 0.17.0 worlds |
| `critic/` | the thirteen critic worlds |
| `purity/`, `timing/` | purity and timing |
| `compat/` | the 0.17.0 world read by 0.18.0 |
| `readme/`, `readme-report.txt` | the README phrase world |

The scripts are in the same folder: `check09.py`, `stats09.py`, `chronicle09.py`, `cession_scan.py`, `names.py`, `sweep-job.sh`, `run-sweep.sh`, `old09.sh`, `recheck.sh`, `purity.sh`, `timing.sh` and `critic/make.sh`.

## 12. Re-validation after the cession fix

**Input.** Merged into `p3/09-validation` (merges, no conflicts):
- `origin/p3/09-fix-impl` `901ec5e` (the engineer: `cede` passes every standing settlement of the loser, P3-06 20.27);
- `origin/p3/09-fix-tests` `b08ef8a` (the test writer: reference, invariant (13), the 20.28 statistic, `p3-06-conflict.txt` re-recorded).

Both are cut from `origin/p3/09-specfix` `623e98c`, which is this branch's `537f981` plus P3-06 r13 (20.27–20.29) and P3-07 r11 (21.8 item 5). Generator 0.18.0, schema 7. Logs are in `~/.cache/the-unmapped/out/p3-09-validation/logs2/`. The first run's sweep and critic folder are kept as `sweep-first/` and `critic-first/`.

**Verdict: pass.** D1, D2 and D3 are closed, and I found no new defect.

### 12.1 Gates

| Gate | Result |
| --- | --- |
| Build, five Linux presets | warning-free |
| `ctest` | 33 of 33 on every preset |
| Lint | both commands clean |
| Slow suites, `linux-gcc-release` | `worldgen-tests` 59 of 59 (2305 s; two cases added), `worldgen-cli-tests` 8 of 8 (253 s) |
| Slow suites, `linux-clang-release` (run after, not beside, the debug and asan ctest) | 59 of 59 (2181 s) and 8 of 8 (199 s) |
| `world-hashes.sh local`, five presets | 15 cases identical |
| `expect` | 15 of 15 on each preset |

`ctest` times:

| Preset | Time | Load at the end |
| --- | --- | --- |
| `linux-gcc-release` | 117 s | 6.3 |
| `linux-clang-release` | 119 s | 6.9 |
| `linux-gcc-debug` | 520 s | 5.5 |
| `linux-clang-debug` | 541 s | 6.0 |
| `linux-clang-asan` | 2341 s | 1.8 |

**No world hash changed.** None of the fifteen cases contains a cession with a same-step colony, so `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt` and the inline constants stay as recorded in section 7, and nothing was re-recorded. The extension was not rebuilt: no source outside `worldgen/src/history/` changed.

### 12.2 The sweep again (375 worlds, same jobs)

- The reference's `check-world --version 0.18.0` passes **375 of 375**, d2048 seed 3 included.
- The peoples reference passes 20 of 20 elf-only worlds.
- My `check09.py` passes 375 of 375. It now reads 20.28's statistic: counted wars skip those closed by `civ_conquered` or `civ_ended`, with the incremental run form of P3-07 21.8 item 5 r11. It also checks invariant (13) by replaying settlement owners: after each `region_ceded`, every settlement of `from` standing in the region (by the event replay, same-step colonies included) is `to`'s in the census of that year.
- Its statistics agree with the reference's `[stats]` in every world. D3's four worlds now agree too: calm seed 17 gives 2.
- `cession_scan.py` finds no region held without a settlement of its holder.

**Worlds whose file changed against the first 0.18.0 sweep: 10 of 375.**
- civs4 seeds 3, 12, 13, 15;
- 2048 seed 3;
- 512 seed 1 (the engineer's flight world: a 7.7 goal cession);
- default seeds 5, 34;
- `elf,goblin` seed 4;
- span 12000 seed 19.

These are exactly the 10 worlds in which my invariant (13) check fails on the first sweep's file:
- in 8 of them the failing cession is a 20.3 cession;
- in 2 it is a 7.7 goal cession (512 seed 1, span 12000 seed 19).

Only 2048 seed 3 also broke 14 item 5 (region held with no settlement of the holder). In the other 9 the winner already had a settlement in the region, so only (13) caught them.

**Cessions that carry a settlement of the loser founded earlier in the same year, in the fixed worlds: 21, in 19 worlds.**
- 14 are 20.3 cessions (14 settlements) and 7 are 7.7 goal cessions (7 settlements).
- In 9 of the 19 worlds the colony was already visible to the old rule (founded in an earlier step of that year), so the bytes did not move. The 10 changed worlds are the same-step cases of 20.27.

**Targets.** T2 is the 20.28 statistic.

| Set | T1 hollow ‰ (hollow / found) | Unfound | T2 worlds above 3 | T3 `refuge_held` r1 | T4 | T5 won / lost | T6 | T7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| default 1024 | **230** (481 / 2083), met | 443 | **0** (was 2) | 1 | 0 | **114 / 68**, met | 0 (17 by aim) | 0 |
| 512 | 292 (130 / 444) | 66 | 0 | 57 | 0 | 32 / 17 | 0 (3) | 0 |
| 2048 | 158 (32 / 202) | 28 | 0 | 0 | 0 | 13 / 5 | 0 | 0 |
| span 12000 | 148 (120 / 809) | 135 | 0 | 6 | 0 | 85 / 35 | 0 (4) | 0 |
| civs 4 | 202 (324 / 1603) | 188 | **0** (was 2) | 7 | 0 | 16 / 22 (lost more, as before) | 0 (21) | 0 |
| every other set | unchanged from section 4 | | 0 | | 0 | | 0 | 0 |

T2 now holds in every world of every set: `wars_same_goal_unchanged_max` is at most 3 everywhere. The four D2 worlds give 3: default seeds 6 and 77, civs4 seeds 0 and 13. T3 holds: no reason-1 `refuge_held` has a destination by the reference, and my site-free recomputation now finds free regions for none in civs 4 (3 before).

**Statistics that moved** (1024 defaults, against the first run; every other figure of section 5 is unchanged or moves by at most one in a median):

| Statistic | After the fix | First run |
| --- | --- | --- |
| hollow ‰ over the sweep; median | 230; 230 | 229; 226 |
| worlds with a flight (flights) | 36 (45) | 37 (46) |
| `search_abandoned` | 443 | 437 |
| battles, median | 77 | 76 |
| largest civilization land share, median | 209 | 211.5 |
| largest race share of held regions, median | 460 | 467 |
| attackers: elf / goblin / dwarf / faerie / human / orc | 38 / 1929 / 105 / 4 / 144 / 306 | 36 / 1922 / 105 / 5 / 147 / 308 |
| orc age: wars, orc share, goblin share | 1173, 260‰, 535‰ | 1170, 263‰, 530‰ |
| orc wars won / lost | 114 / 68 | 115 / 68 |
| `wars_same_goal_unchanged_max` (20.28) | 1 / 3 / 3 | 1 / 3 / 4 by the old definition |

At 512:
- orcs win 32 and lose 17 (29 / 17 before);
- orcs end with one settlement in 4 worlds (5 before);
- elf standing median is 7.5 (8);
- the largest race share of people median is 485.5 (496.5).

At 2048 the hollow share is 158‰ (140‰ before) and the maximum event count is 2349 (2444). The typical ranges of section 5.4, as adopted in P3-06 20.29, still describe the 1024 defaults: no 5th or 95th percentile moved by more than one unit.

### 12.3 Critic folder and README world

- Of the thirteen critic worlds only **seed 42 1024 `--civs-per-race 4`** changed:
  - file hash `5a03e6a8…` → `9a549ca1…`;
  - its first-run file breaks invariant (13) at e267 (region 42, c5 → c4, settlement s69);
  - the new world passes `check09.py` and the reference.
- All of `critic/` was regenerated with the fixed binary. The other twelve worlds are byte-identical, and so are their chronicles.
- **The README phrase world did not change:** hash `255e4cb0b4c5b6fa19603abfd48be2827ab5fa23cf2eeda23a6f4f0e156956cc`, and `readme-report.txt` is byte-identical.

### 12.4 Defects after the fix

| Defect | Status | Evidence |
| --- | --- | --- |
| D1 | closed | (13) and 14 item 5 hold in every world; 2048 seed 3 passes |
| D2 | closed | 20.28's statistic is at most 3 everywhere |
| D3 | closed | the reference's statistic matches mine in every world |

The findings F1–F9 of section 9 are unchanged in substance. F1–F3 are now recorded as known observations in P3-06 20.29. F9 (`CLAUDE.md`'s CTest table and the README example) is still for the lead.
