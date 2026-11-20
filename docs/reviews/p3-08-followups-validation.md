# Validation: P3-08 names and peoples follow-ups (generator 0.17.0, schema 7)

Validation specialist, 2026-11-20. Input: `origin/p3/08-int` at `dae7b96` (the engineer's code, the test writer's tests, the spec fixes and the audit branch), with `origin/main` (`dd56fce`, generator 0.16.0) merged into `p3/08-validation`. Change spec: `docs/specs/p3-08-followups.md` r1 and the stage-spec revisions it names. Determinism audit: pass with notes (`docs/reviews/p3-08-followups-determinism-audit.md`). Baseline 0.16.0: the P3-06 validation's binary and worlds (`~/.cache/the-unmapped/out/p3-06-validation/`, `~/.cache/the-unmapped/out/p3-06-val/world-hashes/`).

**Verdict: pass.** I found no defect in the stage, the tests or the references.

- **Presets.** All five Linux presets build without warnings and pass 32 of 32 CTest entries after recording. The extension build (`linux-gcc-release` with `UNMAPPED_BUILD_GDEXT=ON`) passes 33 of 33, `gdext-headless-check` included. Both lint commands are clean.
- **World hashes.** The fifteen world-hash cases are byte-identical across the five presets and the extension build. `expect` gives 15 of 15 on every preset.
- **Unchanged against 0.16.0.** Everything section 5 says must not move did not move.
- **Sweep.** I swept 375 worlds in 15 sets. My independent check and the test writer's `p3_conflict_reference.py check-world` both pass on 375 of 375. The P3-06 defect D1 is gone. The peoples reference's `check-world` passes on the 20 elf-only worlds (the only set without conflict).
- **Spec figures.** At default settings, seeds 0–49 reproduce the spec writer's predicted figures of P3-08 6.4 ("all + both (shipped)") exactly, median for median.

The findings in section 9 are for the spec writer, the critic and the lead.

## 1. What was run

| Step | Result |
| --- | --- |
| Merge `origin/main` into `p3/08-validation` | conflicts in `CLAUDE.md` (kept the int branch's shard row, which names the new entries) and `docs/design/proposals.md` (kept both new entries); `91ce4b7` |
| Build `linux-gcc-release`, `-clang-release`, `-gcc-debug`, `-clang-debug`, `-clang-asan` | exit 0, 0 warnings each; rebuilt after recording, 0 warnings |
| CTest round 1 (before recording) | 30 of 32 on every preset; the two failing entries fail only on world-hash constants (2.1) |
| `world-hashes.sh local` over the five presets | `OK: all 15 cases identical across 5 presets` |
| Record (section 6), rebuild, CTest round 2 | 32 of 32 on every preset; `world-hashes.sh expect` 15 of 15 on every preset |
| Extension (`linux-gcc-release-gdext`) | 33 of 33 including `gdext-headless-check` (7.6 s); `compare linux-gcc-release linux-gcc-release-gdext`: 15 of 15 identical |
| Lint | `--self-test` passed (17 snippet files); scan: 162 C++ and 13 CMake files, 0 findings |
| Sweep, 375 worlds | independent check 0 failures; `check-world` 0 failures in 375; peoples `check-world` 0 failures in 20 |
| Purity, old files, timing | section 5 and 8 |

## 2. Tests

### 2.1 Failing set before recording

Round 1, all five presets: `worldgen-tests-other` and `worldgen-cli-tests-other` fail, every other entry passes. Every failing assertion is a recorded world hash:

- `cli_golden_test.cpp:132`: 8 cases against `p0-worlds.txt`.
- `cli_golden_test.cpp:135`: 8 cases. This is `hash --expect` on the same files, exiting 5 (mismatch), the same cause as 132.
- `cli_p1_generate_test.cpp:92–95, 116, 154, 171, 174, 175`: `golden(...)` reads `p0-worlds.txt`. There is no inline constant in this file.
- `db_determinism_test.cpp:384`: the five `GOLDEN_CASES`.
- `stages_pipeline_test.cpp:496, 509`: the empty-world and test-pattern hashes.

The failure text is byte-identical on all five presets once line numbers are removed. GCC and Clang report a multi-line `CHECK` at different lines; the values are the same. The audit's O1 fixture problem (`walk_area` as `u8`) is fixed on the branch (`dae7b96`): `worldgen-tests-engine` passes.

### 2.2 Times

Round 1 ran with my sweep alongside it (2 workers). Round 2 ran after the sweep, but other sessions were then running on the machine (a `p3-09-impl` debug `generate` and four `proto_v6.py` prototype runs), at load 6–10. Neither round is a quiet-machine measurement.

| Preset | Round 1 (s, load) | Round 2 (s, load) |
| --- | --- | --- |
| `linux-gcc-release` | 126 (6.8–8.6) | 107 (1.7–6.2) |
| `linux-clang-release` | 123 (8.6–8.5) | 136 (6.2–9.0) |
| `linux-gcc-debug` | 567 (8.5–6.9) | 627 (9.0–10.1) |
| `linux-clang-debug` | 593 (6.9–6.4) | 683 (10.1–10.1) |
| `linux-clang-asan` | 2,524 (6.4–4.6) | 4,315 (10.1–9.8) |

The split shards (P3-08 4.1), in seconds, rounds 1 / 2:

| Entry | gcc-release | clang-release | clang-asan (share of 3,600 s) |
| --- | --- | --- | --- |
| `worldgen-tests-chronology` | 28 / 24 | 27 / 24 | 625 (17%) / 1,044 (29%) |
| `worldgen-tests-chronology-property` | 33 / 28 | 33 / 30 | 650 (18%) / 852 (24%) |
| `worldgen-tests-engine-worlds` | 16 / 15 | 16 / 20 | 377 (10%) / 847 (24%) |
| `worldgen-tests-engine-worlds-resume` | 20 / 18 | 19 / 20 | 473 (13%) / 1,043 (29%) |
| `worldgen-tests-engine` | 21 / 18 | 21 / 23 | 220 / 351 |
| `worldgen-tests-peoples` | 17 / 13 | 16 / 14 | 411 / 571 |
| `worldgen-tests-conflict-worlds` | 16 / 15 | 16 / 15 | 378 / 744 |
| `worldgen-cli-tests-chronology` | 26 / 22 | 25 / 26 | 470 / 843 |

- **The splits.** Each split is within a factor of two (chronology 625 against 650 s; engine-worlds 377 against 473 s under asan).
- **Asan acceptance.** In round 1 (load 5–6) the longest asan entry was `worldgen-tests-hydrology` at 775 s, 21% of its timeout, so no entry was above 25%; the acceptance of 4.1 is met.
- **Above a third under asan.** Round 2: `worldgen-tests-hydrology` 1,632 s (45%) and `worldgen-tests-elevation` 1,311 s (36%), both geography suites this change does not touch, at load 10.
- **Above 30 s on release.** `worldgen-tests-chronology-property` (33 s on both release presets in round 1, 30 s on clang in round 2), and in round 2 `worldgen-tests-elevation` 33 s and `worldgen-tests-hydrology` 35 s on clang.

## 3. What must not change: 0.16.0 against 0.17.0

From `hash --detail` of the fifteen cases (0.16.0: `out/p3-06-val/world-hashes/`, 0.17.0: `out/p3-08-val/world-hashes/`, `linux-gcc-release`), and row by row (`rows017.py`):

- **Geography.** Every layer (all 19 of the geography cases, the three `test.*` layers of the pattern cases) is identical. So are the tables `pass`, `biome`, `climate`, `region_biome`, `region_border` and `layer`.
- **`region`.** Only `name` differs: no other column of any row changes in any case. Every changed name belongs to a base name that repeats; the regions that keep their name are the keepers. Names changed in `geo-seed-0-512` 2, `geo-seed-42-512` 5, `geo-phrase-512` 2, `geo-seed-1-1024` 44, `hist-races-elf-orc-1024` 37.
- **`meta`.** Only `generator_version` (0.16.0 → 0.17.0) and `content_hash` (`676a3ed5…365c` → `f785185e…9f3a`) differ.
- **Chronology.** `race`, `culture` and `age` are identical. `god` differs in names only: ids and camps are the same, and gods with an unchanged name stay (seed 42 at 512: `Krok` → `Krokzit`, `Kruu` → `Ogh'krak`; seed 0 at 512: `Bald` → `Uruld'kruum`). `race_biome` differs in the seven faerie rows only (P3-05 12.3 candidate A).
- **History.** Every history table differs, as specified.
- **The empty world.** 139,264 bytes (34 pages) at both versions.
- **Per-stage vector files.** `core_vectors.txt` and `p1-01` to `p1-05` are untouched by the branch. In the files that moved:
  - `p1-06-regions.txt`: only the `table_region` lines of the seven cases section 5 names.
  - `p3-03-chronology.txt`: only `table god`, `table race_biome` and the `input_region` lines (region names).
  - `p3-04-engine.txt`: no schedule, `phase` or stub `event` line. What moved is the stub worlds' `civilization`, `settlement` and catalogue tables, the sentence lines, and the `flare_up` template-grammar probes, where `{years?a|b|c}` became valid with the three-word form.
  - `p3-05-peoples.txt`: of the `score` lines only race 4 (faeries, 749 lines). Five non-faerie `homes` lines moved (`geo-seed-1-1024` humans; `stress-0`, `-7`, `-12` humans and orcs). The test writer's `compare-016` checks each against a 0.16.0 home that is no longer eligible at that awakening, as section 5 allows.
- **Old files.** The 0.17.0 binary reads the 0.16.0 world of seed 42 at 1024. `report` and `hash --detail` are byte-identical to 0.16.0's output for the same file, and `render` of the `map` and `regions` composites and of a layer exits 0.

## 4. Sweep

The sets are those of the P3-06 validation:

- 1024 seeds 0–99 at defaults;
- 512 seeds 0–29;
- 2048 seeds 0–4;
- at 1024 seeds 0–19 each: span 2000 and 12000, sparse, dense, calm, turbulent, civs 1 and 4, `elf,orc`, `elf,goblin`, `elf`, and `goblin,dwarf,faerie,human,orc`.

Release binary (`linux-gcc-release`), `--threads 1`, the checkout's `game/data`. For every world:

1. **My independent check `check08.py`.** It is the P3-06 validation's `check06.py`, adapted from the stage specs without the C++ or the references. It covers:
   - every check of P3-05 14 and P3-06 14 at their current revisions, plus the new ones of P3-05 r7: invariant 3 (no standing settlement below `abandon_min`); 6 (a dwarven settlement on `walk_area` 0 is a fortress); 11 (no chance abandonment of a last settlement); 12 (kind 31's destination is a standing settlement of the civilization, the nearest, `settled` ≥ 1); 13 (plague salience by total toll, crisis salience by split);
   - the abandonment causes (the blow and the decline start, ascending);
   - P3-04 r10 6.6 (one `quiet_era` per civilization and quiet phase row, `years` = the span);
   - P3-06 r8 7.3 (victors never lose more than the defeated);
   - the hard bounds of P3-06 15.5.
2. **The C++ sentences.** Every event's sentence comes from the engineer's skipped suite `p3-08-dump`, that is `render_event` itself. Every sentence is checked for: no placeholder left, no capital `The` after the first word, no "0 fled", no "0 generations", a capture with `fled` 0 never mentioning flight, and kind 31 with `lost` 0 never mentioning loss.
3. **Names.**
   - Every god, civilization and settlement name is reproduced attempt by attempt with `p3_names_reference.attempt`, counting the attempts the filter rejected before it.
   - Every word of every rendered name is tested against the shipped filter lists (`anywhere` and `at_start` on every word, `whole` on the words that are not fixed form words).
   - Every region name is checked against `p1_regions_reference.py names` (stored name = reference name, keepers, attempts, fallbacks), for uniqueness and for any numeral.
4. **The test writer's reference.** `p3_conflict_reference.py check-world` recomputes the five chronology and thirteen history tables from the stored geography, compares them table by table, and checks P3-05 14 (with D1's reading) and P3-06 14.

| Set | Worlds | Independent check | `check-world` |
| --- | --- | --- | --- |
| default 1024 | 100 | 0 | 0 |
| d512 / d2048 | 30 / 5 | 0 / 0 | 0 / 0 |
| span2000 / span12000 | 20 / 20 | 0 / 0 | 0 / 0 |
| sparse / dense | 20 / 20 | 0 / 0 | 0 / 0 |
| calm / turbulent | 20 / 20 | 0 / 0 | 0 / 0 |
| civs1 / civs4 | 20 / 20 | 0 / 0 | 0 / 0 |
| elf_orc / elf_goblin / elf / no_elf | 20 each | 0 | 0 (elf: also peoples `check-world`, 0) |

The new invariants over the 375 worlds:

- **Victors.** Victors lost more in 0 battles. The default set has 8,985 battles.
- **"fled".** No sentence says "0 fled". Of 855 default captures, 838 have `fled` 0 and their sentences end at the dead.
- **Capital "The".** No sentence has a capital "The" after its first word, and none says "0 generations".
- **Chance abandonments.** 0 of a last settlement. Every abandonment in the sweep was a chance abandonment of a settlement with another standing settlement, and every one wrote kind 31 with `lost` 0 (F6).
- **Kind 31.** Every destination is a standing settlement of the abandoning civilization and the nearest one (the replay found none further away).
- **Saliences.** Plague and crisis saliences equal the rule in every event.
- **Dwarven fortresses.** No dwarven settlement on `walk_area` 0 is anything but a fortress.
- **Quiet eras.** No civilization has two `quiet_era` events in one quiet phase row.
- **Names.** No generated name is filtered, by its root word or by any word. No name of the 38,145 checked fell back to a numeral. No region name has a numeral, and every world's region names are unique and equal the reference's.

The hard bounds of P3-06 15.5 hold on every world of every set:

- `share_over_800_years` 0 everywhere;
- events at most 1.66 per simulated year (civs4), 1.48 (turbulent), 1.10 at default settings;
- simulated at most 373‰ (dense);
- history tables at most 462,848 bytes (span12000);
- every present race alive;
- standing at least 46 at 1024 defaults, 22 at 512, 97 at 2048;
- P ≤ 2C everywhere; the largest ratio is exactly 2,000‰, the bound itself, in one default and one sparse world.

### 4.1 Whole worlds (1024 default; min / median / max)

The spec's figures are P3-08 6.1 and 6.4 (seeds 0–49, the harness on the 0.16.0 worlds). The 0.16.0 column is the P3-06 validation's 100 worlds.

| | spec `all + both` (0–49) | 0.17.0 seeds 0–49 | 0.17.0 seeds 0–99 | 0.16.0 seeds 0–99 |
| --- | --- | --- | --- | --- |
| events | 363 / 669 / 1,357 | **363 / 669 / 1,357** | 300 / 652 / 1,629 | 387 / 819 / 1,795 |
| `quiet_era` | 15 / 35.5 / 60 | **15 / 35.5 / 60** | 15 / 36 / 89 | (base 53 / 85.5 / 120, 0–49) |
| eras ≤ 50 years | (all: 0 / 7 / 23) | 0 / 7 / 17 | 0 / 7 / 17 | (base 18 / 33.5 / 53) |
| eras with 0 generations | (all: 0 / 2.5 / 15) | 0 / 3 / 10 | 0 / 3.5 / 12 | (base 4 / 16.5 / 33) |
| median era length | (all: 149 / 425 / 690) | 178 / 425 / 690 | 178 / 425 / 690 | (base 50 / 75 / 125) |
| famines | 18 / 47.5 / 95 | **18 / 47.5 / 95** | 18 / 49 / 103 | (base 44 / 96 / 179) |
| plague spreads | 13 / 53.5 / 156 | **13 / 53.5 / 156** | 11 / 55 / 174 | (base 32 / 104.5 / 291) |
| mechanical share ‰ | 200 / 304.5 / 446 | **200 / 304.5 / 446** | 200 / 309.5 / 482 | (base 272 / 441 / 558) |
| events per age (medians, 1–6) | 22, 68.5, 72.5, 86, 123, 273.5 | **22, 68.5, 72.5, 86, 123, 273.5** | 21, 70.5, 77.5, 94.5, 113, 267 | — |
| mechanical ‰ per age (medians) | — | — | 83, 110, 385, 383.5, 343.5, 333.5 | — |
| wars; sacked (medians) | 22.5; 13 | **22.5; 13** | 4 / 22.5 / 67; 2 / 13 / 35 | 5 / 24 / 75; 2 / 12 / 45 |
| taken | (all 1 / 7 / 26) | 0 / 6 / 25 | 0 / 6.5 / 29 | 0 / 6 / 36 |
| civilizations ended | (all 45 in 50) | 0 / 1 / 3 (46) | 0 / 1 / 4 (116) | 0 / 1 / 3 (99) |
| ruins | — | — | 2 / 12 / 31 (96.6% by sacking) | 1 / 13 / 37 (91.9%) |
| flare-ups | — | — | 0 / 2 / 7 | 0 / 2.5 / 7 |
| simulated years | — | — | 1,112 / 1,274.5 / 1,474 | 1,122 / 1,278 / 1,511 |
| summary threshold | 26 / 31 / 51 (6.5) | 21 / 31 / 46 | 20 / 31 / 51 | 26 / 31 / 51 |
| plague share of the summary ‰ (median) | 121 (6.5, on 0.16.0 logs) | 86 | 83 | 205 |
| crises in the summary (median) | 1 | 1 | 0 / 1 / 16 | 10 |
| largest land share ‰ | — | — | 86 / 182 / 340 | 106 / 176 / 340 |
| population elves / goblins / humans (medians) | 13,829 / 74,414 / 74,090 | **13,828.5 / 74,414 / 74,090** | 11,686 / 77,195 / 66,117 | 11,082 / 75,854 / 74,755 |

Bold: identical to the spec's prediction. The engineer's C++ and the spec writer's harness agree to the median on every whole-world figure they share.

### 4.2 Standing settlements per race at the present

| | spec shipped (0–49) | 0.17.0 seeds 0–49 | 0.17.0 seeds 0–99 | 0.16.0 seeds 0–49 | 0.16.0 seeds 0–99 |
| --- | --- | --- | --- | --- | --- |
| elves | 11 | 11 | 1 / 10 / 44 | 12 | 10 |
| goblins | 32.5 | 32.5 | 4 / 34.5 / 109 | 31.5 | 32.5 |
| **dwarves** | **4.5** | **4.5** | 2 / 5 / 28 | **6.5** | 6 |
| faeries | 5 | 5 | 1 / 4 / 19 | 4 | 4 |
| humans | 14 | 14 | 1 / 12 / 39 | 14 | 14 |
| orcs | 5 | 5 | 1 / 5 / 15 | 5 | 5.5 |
| worlds with exactly one standing (0–99) | — | elves 5, faeries 8 (0–49) | elves 8, humans 4, orcs 2, faeries 23, goblins 0, dwarves 0 | — | elves 7, humans 6, orcs 1, faeries 26, goblins 3, dwarves 3 |

The dwarves fall from 6.5 to 4.5 at seeds 0–49, as the spec predicted (its 6.4 note, 9 item 5); on seeds 0–99 they fall from 6 to 5. Dwarven population is 13,552 at 0–49, against the spec's 14,209 for `all`. No world ends with a single dwarven settlement; 0.16.0 had 3. The worlds where a race ends with one settlement, by seed (1024 default):

- elves: 6, 21, 37, 40, 49, 59, 86, 92;
- humans: 21, 23, 46, 82;
- orcs: 14, 21;
- faeries: 1, 2, 8, 15, 29, 37, 42, 45, 54, 55, 58, 59, 60, 63, 65, 66, 67, 69, 75, 76, 89, 92, 93.

Seed 21 has three races down to one settlement.

### 4.3 Dwarves (P3-08 6.2, 9 item 5)

| | spec base | spec all | 0.17.0 seeds 0–49 | 0.17.0 seeds 0–99 | 0.16.0 seeds 0–49 (measured) |
| --- | --- | --- | --- | --- | --- |
| dwarven settlements founded | 350 | 360 | 366 | 744 | 350 |
| median elevation rank in the region (pooled) | 0.21 | 0.72 | **0.72** | (per-world medians 0.18 / 0.78 / 1.00) | 0.20 |
| fortresses | 259 (74%) | 303 (84%) | 300 (82%) | 601 (81%) | — |
| on `walk_area` 0 | 159 | 251 | 250 | 508 | — |
| on rich ore (`ore` ≥ 128) | 56 (16%) | 219 (61%) | 217 (59%) | 437 (59%) | — |
| dwarven cities standing | 14 | 6 | 9 | 17 | — |
| homes upland or with reachable ore | — | — | 72 of 72 | 144 of 144 | — |

The other races' pooled ranks are unchanged against 0.16.0 except the faeries: elves 0.18 (0.16), goblins 0.21 (0.21), humans 0.12 (0.15), orcs 0.15 (0.15), faeries 0.06 (0.12; spec 0.07). The dwarven home rule (P3-05 15.5, at least 90%) holds in every set: 144 of 144 at default settings; at its floor at 512 (27 of 30, 90%) and in civs4 (75 of 79).

### 4.4 Faeries (6.3, 9 item 6)

- **Homes.** All 133 faerie homes of the default set are dominated by temperate forest. The median forest share of a home is 981‰ (min 701).
- **Standing settlements.** Seeds 0–49: 1 / 5 / 19 (spec `fae` alone: 1 / 5 / 18). Seeds 0–99: 1 / 4 / 19.
- **Population.** Seeds 0–49: 222 / 3,299 / 14,183 (spec 417 / 3,290 / 13,956).
- **One settlement or none.** 8 of seeds 0–49 (spec 5 for `fae` alone, 8 for `all`); 23 of 100 (0.16.0: 26).
- **civs4.** Faerie homes: 76 forest, 4 grassland, median forest share 858‰.

### 4.5 Battles, captures and abandonments (9 items 7, 8)

- **Battles.** Victors lost more in 0 of 8,985 default battles (spec: 0 of 4,645). The same holds in every set.
- **Captures.** 838 of 855 have `fled` 0 (spec: 419 of 423 at 0–49; the sweep has 404 of 413). The sentence drops the clause: `…; 342 died.`.
- **Abandonments.** Every abandonment of the sweep is kind 31 (47 at default, 54 at span12000, 19 in civs4); kind 16 does not occur (F6). All have `lost` 0, so the destination always had room. Chance abandonments of a last settlement: 0 (invariant 11).
- **Above the kind cap at the present.** 1 settlement at default (seed 31: s53, 1,920 against a cap of 1,499), 6 across the sweep, every one draining at the present (P3-05 inv 3, the reference). The six:
  - span12000 s0: s118, 9,486 against 7,999;
  - d2048 s1: s106, 9,159, and s136, 8,175, against 7,999;
  - turbulent s11: s38, 8,005 against 7,999;
  - turbulent s17: s97, 1,753 against 1,499;
  - no_elf s17: s27, 1,502 against 1,499.

  The spec predicted 2 in 50 worlds.

### 4.6 Names (9 item 9; P3-02 r7 11)

- **Region names.** At default settings 2,088 of 4,951 regions keep their plain name (42%; P1-06 14 figure 45%). At 2048 it is 158 of 995 (16%; spec 16%). The rest are drawn: 0 fallbacks, highest attempt 2.
  - Base names that repeat before drawing: 8 / 12 / 15 per default world. None is left: every world's names are unique.
  - Region names that differ from 0.16.0, by id: 3,620 of 4,951. That is every region that drew a name plus the old first holders of a repeated base.
  - The history filter, applied to region names for information only (the region namer does not use it): 42 hits at default settings. All are English words: Nightgrove, The Benighted Holt, Tithedown, Sexton's Steppe, Sicklefurrow Woods, Gravedigger's Woods. None is offensive (F8).
- **History names.**
  - 38,145 god, civilization and settlement names were checked: 3,139 attempts were rejected by the filter and redrawn, 65 attempts were valid but already taken, and there was no fallback. The highest winning attempt is 8.
  - Rejected attempts per list, an attempt hit by two lists counting for each: `words` 1,978, `crude` 786, `slurs` 270, `famous` 135.
  - Rejected attempts per culture: goblin settlements 2,033; human settlements 241, civilizations 114; orcish 193 in all; faerie 133; elven 120; dwarven 86.
  - The masked list is `critic/rejected-names.txt`.
- **Names that differ from 0.16.0** (same table and id, default set):
  - gods: dwarven 35%, elven 1%, faerie 24%, goblin 67%, human 78%, orcish 81% (the god patterns);
  - civilizations: elven 37% (`The House of {root}`), goblin 12%, human 15%, faerie 6%, dwarven 4%, orcish 5%;
  - settlements: goblin 13.9%, human 5.7%, faerie 5.0%, orcish 5.6%, dwarven 2.1%, elven 1.2%.

  This is consistent with P3-02 11.6's 5.4% rejection rate, highest for goblins.

### 4.7 Typical ranges of P3-06 15.5 (r7), 1024 default

| | range | 0.17.0 p5 / p50 / p95 | outside |
| --- | --- | --- | --- |
| events | 450–1,500 | 399 / 653 / 1,238 | 15 (14 below, 1 above; 0.16.0: 9) |
| wars | 10–50 | 9 / 23 / 53 | 13 |
| sacked | 4–30 | 4 / 13 / 31 | 10 |
| summary threshold | 26–46 | 26 / 31 / 46 | 5 |
| simulated years | 1,120–1,450 | 1,163 / 1,275 / 1,446 | 3 |
| flare-ups | 0–5 | 0 / 2 / 6 | 5 |
| civilizations ended | 0–4 | 0 / 1 / 3 | 0 |
| ruins by sacking | ≥ 70% | p5 81%, pooled 96.6% | — |

The outliers, by seed:

- events below 450: 25, 31, 32, 35, 43, 47, 50, 52, 53, 72 (300), 74, 81, 90 and 91; above 1,500: 66 (1,629);
- wars: 9, 31, 32, 38, 47, 74 (4) and 81 below 10; 12, 29, 66 (67), 67, 89 and 92 above 50;
- sacked: 9, 25, 74, 81 and 97 below 4; 1, 29, 54, 66 and 89 above 30;
- threshold: 32, 72, 74 and 81 below 26; 66 (51) above 46;
- flare-ups above 5: 21, 66, 70, 92 and 93 (7).

Seed 66 is the busiest world (1,629 events, 67 wars, 35 sacked). Seeds 72, 74 and 81 are the quietest.

### 4.8 The other sets (min / median / max)

| Set | events | quiet eras | eras ≤ 50 | mech ‰ | wars | sacked | threshold | dwarves standing | faeries = 1 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| d512 | 189 / 526 / 715 | 19 / 31.5 / 49 | 0 / 7 / 21 | 132 / 227.5 / 326 | 6 / 28 / 43 | 0 / 11.5 / 25 | 20 / 31 / 31 | 1 / 3 / 12 | — |
| d2048 | 513 / 650 / 821 | 21 / 39 / 44 | 0 / 9 / 14 | 301 / 344 / 411 | 2 / 11 / 17 | 0 / 4 / 11 | 26 / 26 / 31 | 4 / 5 / 12 | 0 |
| span2000 | 92 / 202.5 / 397 | 16 / 25 / 40 | 0 / 0 / 10 | 201 / 263.5 / 350 | 1 / 5.5 / 14 | 0 / 1 / 6 | 20 / 20 / 21 | — | — |
| span12000 | 715 / 1,486.5 / 3,826 | 21 / 54.5 / 108 | 0 / 0 / 26 | 219 / 354 / 474 | 12 / 58.5 / 171 | 9 / 34.5 / 94 | 26 / 46 / 71 | 4 / 9 / 20 | 7 |
| sparse | 321 / 517.5 / 759 | 24 / 33.5 / 51 | 0 / 0 / 9 | 208 / 275 / 333 | 4 / 15 / 32 | 2 / 10 / 17 | 20 / 26 / 31 | — | — |
| dense | 448 / 766.5 / 1,964 | 15 / 38.5 / 92 | 0 / 6 / 24 | 254 / 347.5 / 427 | 13 / 25 / 111 | 8 / 18 / 41 | 26 / 31 / 61 | 4 / 4 / 16 | 5 |
| calm | 257 / 425 / 842 | 17 / 26.5 / 44 | 0 / 0 / 16 | 163 / 255 / 352 | 4 / 14 / 35 | 0 / 8 / 17 | 20 / 26 / 31 | 4 / 6 / 17 | 2 |
| turbulent | 368 / 1,081.5 / 2,494 | 18 / 58.5 / 132 | 0 / 14 / 56 | 236 / 382 / 456 | 10 / 39.5 / 120 | 2 / 23 / 41 | 21 / 36 / 61 | — | — |
| civs1 | 262 / 445 / 813 | 15 / 21 / 37 | 0 / 0 / 11 | 220 / 331 / 424 | 2 / 10.5 / 28 | 0 / 7 / 16 | 20 / 26 / 31 | 3 / 5 / 12 | 4 |
| civs4 | 1,014 / 1,838.5 / 2,413 | 58 / 114.5 / 163 | 0 / 30.5 / 81 | 163 / 271 / 326 | 38 / 95.5 / 141 | 23 / 46.5 / 83 | 46 / 61 / 71 | 9 / 21.5 / 30 | 9 |
| elf_goblin | 84 / 272.5 / 598 | 3 / 14.5 / 26 | 0 / 0 / 3 | 226 / 307.5 / 425 | 0 / 6 / 16 | 0 / 3.5 / 11 | 20 / 20 / 26 | — | — |
| elf_orc | 50 / 111.5 / 154 | 1 / 2 / 5 | 0 / 0 / 3 | 197 / 264 / 352 | 0 / 0.5 / 3 | 0 / 0 / 3 | 20 | — | — |
| elf | 25 / 78.5 / 117 | 1 / 2 / 2 | 0 | 278 / 380.5 / 435 | 0 | 0 | 20 | — | — |
| no_elf | 234 / 493 / 746 | 13 / 24 / 34 | 0 / 2.5 / 11 | 244 / 334.5 / 444 | 6 / 13 / 25 | 2 / 6.5 / 15 | 20 / 26 / 31 | 4 / 7 / 17 | 7 |

The full per-set listing (per race, dwarven sites, faerie homes, abandonments, names, region names, outliers) is in `sweep/stats08.txt` and `sweep/aggregate.txt`.

## 5. Purity and settings

Seed 0 at 1024 unless stated, `hash --detail` compared:

- `--threads` 1, 2 and 6 are byte-identical, and equal the sweep's `--threads 1` world. At 512, seed 42 with `--threads` 1 and 5 gives `136ae410…f62c` both times, the recorded `geo-seed-42-512`.
- `--until history` equals the default, `meta` included.
- `--until chronology`: the history tables are empty; `stages` lacks `history`; everything else is equal.
- `--monster-pressure high` and `--divine-activity meddling` change only `param.monster_pressure` and `param.divine_activity` in `meta` (P5 settings with no effect yet).
- `--history-span 2000` and `--races elf` change `meta`, `race` and `age` and the history tables. They leave every layer and geography table, `god`, `culture` and `race_biome` unchanged.

## 6. Recorded hashes

All fifteen cases were identical across the five presets before recording. Commit `08e60f2`.

`worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase 8ca38596d673233d5b4493a153d7000d659f02827a2d61354bc4529f4d4a99b1
empty-seed-0 918dc3df4f301d6c979e8e5a7f022f88b8f81a6e7a68e5533a6d665094f8ad19
empty-seed-0-512 db1dd215891bc3821b81195b4b536e9c660f3f45f6e994c4e6b746adfa28a9ee
empty-seed-1 e4388cf197c916e87497d5299101fa79625c22e7a3251bed38a841785843fa0a
empty-seed-42 d1e39135abbd709a18173096d4fa3413d0a4af22ab9f9b02662852fb89dfc08a
empty-seed-max 2fa4878ce2d8af6f803d2b36cf6e3341d25a47302be3635a1cf56f181d581901
pattern-seed-0 1724521f1ce172d53b11d60774bc504a6489853f4e27fab0e9a30e32e9cf059b
pattern-seed-42 5c615564b367829846709f3d1b812112456921d03c32d6d9e151d33faf2b8df3
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 3915b9394c0ec0b582a62e3f3a3236a45962c321acadf4057635e565e5c09936
geo-seed-0-512 a6397189429f08a485816d5f87a460f3da6bfa950546412b1108422e0d90475f
geo-seed-1-1024 56a18f43294083bb70a39a63fc37f4d40c9c98443ffc61c0165d75bc0482ca75
geo-seed-42-512 136ae410fc02e8dc599ecb60aa6b57c3e1fd419d5afc7de6e1a1599fa625f62c
```

`worldgen/tests/golden/p3-worlds.txt`:

```
hist-dense-1024 6f9b0042038cf1e9ecad17dc8939ab5d577200102839590eb05cffc7f648890a
hist-races-elf-orc-1024 96cf4ec1cdff3ed2583fdd70bfa209e510fb9d6aca9bf578abada1a0bb3356c4
hist-span-2000-512 5c0a15791a484898e6b5cc3701c66faa1acd7ea2f79aa24250c677a9730510d0
```

`geo-seed-0-512` and `geo-seed-42-512` agree with the audit's thread runs.

`GOLDEN_CASES` (`db_determinism_test.cpp`, the empty world with the content hash of no data):

```
empty-phrase 13fc2da687b1ca306a72e5aafc88530ea1ca69cc0332e0809422445206cc7f2b
empty-seed-0 ec7d066b0f8a27c866897ad01aa8c4a7ee0376c420b4596d85f4f6e8570a9348
empty-seed-1 491972eb4285cd3bf7b1835fca89200f01ff020c9cb87c3b1c0c76c2eb24955a
empty-seed-42 cdbc24d8f65644f6b491507d57362cd4d6bf9b127eb1e9d39fc85ab4b4d05db1
empty-seed-max 621122b822c12565a4fc62a3672977557d609dab50df31ad2e16a8b1b6c1a080
```

`stages_pipeline_test.cpp`: the empty world `ec7d066b…9348` (equal to `GOLDEN_CASES` `empty-seed-0`) and the test pattern `d74f50a3…67e0`.

Why they moved (P3-08 5): `generator_version` 0.17.0 and the content hash of the new data (`f785185e…9f3a`: kinds `placename` and `name_filter`, race `cell_weights`, culture `god_patterns`, the new tuning, the templates). The geography and history cases also carry the new region, god, civilization and settlement names and the P3-08 history. The per-stage vector files were not touched.

**README phrase world** (`generate --seed-phrase "The Unmapped"`, for the lead's README update; not edited here):

- `hash`: `b392816affd9d3470663fd07ab528f44d5e6806b7b98eaf47a335e77c74ac37d`.
- `report`: `~/.cache/the-unmapped/out/p3-08-validation/readme-report.txt`. It gives `generator_version = 0.17.0`, `event = 768`, `event_kind = 31`, `civilization = 10`, `settlement = 104`, and one `settlement_abandoned_to`.

## 7. Extension

- **Build.** `linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON` into `build/p3-08-val/linux-gcc-release-gdext`: `ctest -N` lists `gdext-headless-check`. The build has no warnings and took 676 s with a cold godot-cpp cache and the sweep running alongside.
- **Tests.** 33 of 33 entries pass; `gdext-headless-check` passes in 7.6 s.
- **Hashes.** `world-hashes.sh compare linux-gcc-release linux-gcc-release-gdext`: all 15 cases are identical.

## 8. Timing

`generate --seed 0` at the default thread count, three runs each, alternating versions. The machine was quiet apart from the desktop (load 1.2–3.5):

| Size | 0.16.0 (s) | 0.17.0 (s) |
| --- | --- | --- |
| 512 | 2.60, 2.63, 2.63 | 2.62, 2.62, 2.64 |
| 1024 | 9.14, 9.12, 9.15 | 9.18, 9.17, 9.27 |
| 2048 | 36.79, 36.86, 36.82 | 36.92, 36.91, 36.93 |

The history stage alone (`peoples-bench`, worlds stored `--until chronology`, two runs) takes:

- 1024 seeds 0–4: 75–83, 148–150, 105–106, 110 and 86–87 ms;
- 512: 24–25 ms;
- 2048: 197–198 ms.

0.16.0 took 79, 110–112, 100–102, 126–128, 119–120, 27 and 170–174 ms on its own worlds. Both are far inside P3-06 16.6's budget.

The engine microbenchmark (P3-04 r11 14.3, suite `engine-bench`): median 0.40 ms, 0.33 µs per simulated year (1,200 simulated years, 39 events) on seed 0 at 1024. On its default input it is 0.43 ms and 0.37 µs per simulated year.

## 9. Defects and findings

**Defects: none.** I found no defect in the stage, the tests or the references. The P3-06 defect D1 (the reference's reading of P3-05 invariant 6) is gone: `check-world` passes on 375 of 375.

**F1 (spec writer). The P3-06 15.5 typical ranges of events and wars predate the halved famine and spread rates.**
- At 1024 default settings, 14 of 100 worlds have fewer than 450 events (p5 399, minimum 300 at seed 72), and 15 are outside 450–1,500, against 9 outside at 0.16.0. Wars: p5 9, p95 53, with 13 outside 10–50. Flare-ups: p95 6, 5 worlds above 5.
- Suggested ranges at 0.17.0 (5th to 95th percentile): events 400–1,250; wars 9–53; sacked 4–31; threshold 26–46; flare-ups 0–6.
- The outliers are listed in 4.7.

**F2 (spec writer, for the record). The summary's plague share is lower than 6.5 predicted.** It is 83‰ at the median (86‰ at seeds 0–49), against 121‰ predicted from the 0.16.0 logs, because the logs now have fewer plagues. Crises in the summary: 1 at the median, as predicted. Threshold 20 / 31 / 51, as before.

**F3 (critic, spec writer). Small battles now say "0 of the victors fell".** With the victors' losses taken from the smaller force (P3-06 r7 7.3), a battle against a force of under about 13 kills none of the victors (`loss_winner` 80‰). There are 32 such sentences in 8 of the 375 worlds:
- default seeds 27 and 91: `The Neebox Mob defeated the Warrens of Nikugg in Crossden; 1 of the defeated and 0 of the victors fell.`
- one each in civs4 s13, sparse s1, no_elf s14 and span12000 s16;
- 27 in d512 s18 alone. There the Dunbad Throne (dwarves) is beaten in Northwest Wood again and again from year 3903 to 5639 by the Gnug Mob and the Whatwapa Ring, losing 1 to 3 each time; this looks like the last-refuge guard of P3-06 holding a near-empty last settlement.

It is correct by the rule. The template could use the three-word form for `winner_dead` as the capture template does for `fled`. The repeated battles against a last refuge are P3-09's subject.

**F4 (spec writer). Quiet-era merging helps less with four civilizations per race.** In civs4, eras of 50 years or less number 0 / 30.5 / 81 per world and eras with no new generation 0 / 16 / 61, with a median era length of 259 years. The default set has 7, 3.5 and 425. With many civilizations the quiet phase rows are short (flare-ups 5.5 per world), so many eras are one step long. Turbulent is in between: 14 eras of 50 years or less, median length 197. It is the rule working as specified.

**F5 (critic). Faeries end with one settlement in 23 of 100 default worlds.** The list is in 4.2; 0.16.0 had 26. At seeds 0–49 it is 8, as 6.1 predicted for `all`; seeds 50–99 have 15. Elves end with one settlement in 8 of 100 (0.16.0: 7), with seed 21 having three races at one settlement. This is not a regression, but it is what the P3-09 last-refuge rules will meet.

**F6 (spec writer, test writer). Kind 16 and kind 31's "were lost" clause never occur in the sweep.**
- Every one of the abandonments of the 375 worlds (47 at default settings) is a chance abandonment of a settlement whose civilization had another settlement, with room for all survivors. So no `settlement_abandoned` is written, and every kind 31 has `lost` 0.
- The sentences `…; its # survivors were scattered.` and `…, and # were lost.` therefore appear only in the unit tests. P3-07 should not count on seeing them.
- Below-`abandon_min` abandonments do not occur either: P3-06's casualties are what could bring one about.

**F7 (lead). Few settlements stay above the kind cap at the present.** There are 6 in the 375 worlds, 1 at default settings (list in 4.5). Every one is draining at `decline_permille` (P3-05 r6 9.1), and the reference's invariant 3 passes. The spec predicted 2 in 50 worlds; this is consistent.

**F8 (Paul, P3-02 11.10).** The history filter would reject 42 region names of the default set if it were applied to regions, all of them English words: Nightgrove, The Benighted Holt, Tithedown, Sexton's Steppe, Sicklefurrow Woods, Gravedigger's Woods. This supports the interim choice of no shared filter with region names. No region name in the sweep is offensive.

**F9 (lead). CTest times are not quiet-machine figures.**
- Round 2 ran while other sessions used the machine at load 6–10, and under asan `worldgen-tests-hydrology` (45%) and `worldgen-tests-elevation` (36%) were above a third of their timeout.
- In round 1 (load 5–6) no asan entry was above 21%, so the 25% acceptance of P3-08 4.1 holds.
- `worldgen-tests-chronology-property` is 30–33 s on the release presets, just above 30 s.
- A quiet rerun would settle the figures for `CLAUDE.md`'s table.

**F10 (for the record).**
- **Smallest 512 worlds.** The smallest 512 world stands at 22 settlements (P3-06 measured 26), with the bound at 1.
- **512 share.** At 512 the largest share is 727‰ (P3-06: 636‰), and one world has `share_over_500_years` 3,211 (P3-06: 3,213). `share_over_800_years` stays 0.
- **Dwarven home rule.** At 512 dwarven homes upland or with reachable ore are 27 of 30, exactly the 90% floor of P3-05 15.5.

## 10. For the lead

- **Merge.** The merge gate holds: tests on every Linux preset, golden hashes recorded and identical across the five presets and the extension build, determinism audit passed, the history report and the critic material are attached (section 11). Windows MSVC is CI's `compare-hashes` job; nothing here gives a reason to expect a difference.
- **Decisions.** Whether to have the spec writer reset the P3-06 15.5 ranges (F1) and give the battle template the three-word form for the victors' dead (F3), in P3-09 or a later change. F4 to F6 are readings for P3-07 and P3-09.
- **README.** The README update with the phrase world's hash and report (section 6) is the lead's.

## 11. Files

Under `~/.cache/the-unmapped/out/p3-08-validation/`:

| Path | What |
| --- | --- |
| `logs/` | build, CTest rounds 1 and 2 (`ctest1-*.log`, `ctest2-*.log`), `summary.txt`, world hashes, expect, extension, lint |
| `sweep/<set>/` | per world: `.db`, `.txt` (hash detail), `.report.txt`, `.sent.txt` (C++ sentences), `.regnames.txt`, `.check.json`, `.ref.txt` |
| `sweep/stats08.txt`, `sweep/aggregate.txt`, `sweep/ref-results.txt`, `sweep/pref-results.txt` | sweep statistics and reference results |
| `check08.py`, `stats08.py`, `aggregate08.py`, `chronicle08.py`, `names.py`, `dwarfrank.py`, `cmp017.py`, `rows017.py` | the scripts |
| `critic/` | the thirteen worlds of the P3-06 critic set: seeds 42, 0, 1, 2, 3, 7, 11 and 23 at 1024; 42 turbulent; 42 with four civilizations per race; 42 and 5 at 512; 42 at 2048. Each has `.db`, `.report.txt`, `.regions.png`, `.map.png`, `.sentences.txt`, `.chronicle.txt` (every event with year, id, kind, salience and causes, then end states, wars and relation rows), `.summary.txt` and `.names.txt`. Also `rejected-names.txt` (masked, with counts per list) and `make.sh` |
| `purity/`, `oldfile/`, `timing/`, `readme/`, `readme-report.txt` | sections 5, 3, 8 and 6 |
