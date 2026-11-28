# P5-08 pantheon, divine acts and cults: validation (generator 0.30.0)

Validated on 2026-11-27 and 28 on branch `p5/08-val` from `p5/08-int` at 5ca9daa (engine with the champion rules and integration fixes E1, E3, E4). Generator 0.30.0, schema 14, history report v11. Spec: `docs/specs/p5-08-pantheon-cults.md` section 15; calibration: `docs/reviews/p5-08-calibration.md` (last section); integration: `docs/reviews/p5-08-integration-notes.md`.

Verdict: **pass, with the misses of section 5.** All five Linux presets are green (62 of 62 each), the slow suite on `linux-gcc-release` is green (23 of 23), both lint commands are clean, the sixteen world-hash cases are identical across the five presets and are recorded, and all 326 sweep worlds (200 default, 100 meddling or distant, 60 in the three other option sets, 20 at 512, 6 at 2048) generated and reported with exit 0 and empty stderr. No stage-logic defect found. Windows and MSVC were not available: cross-platform equality is proven across five Linux presets only. `gdext-headless-check` is not built (extension off).

Machine: 6 cores shared with other agents; builds `-j 5`; `ctest -j 4` for `linux-gcc-debug` (alone on the machine) and `-j 3` for the rest, which ran while the sweeps ran, so only the `linux-gcc-debug` time is a clean figure.

## 1. Presets and suites

| Preset | Build | `ctest` (62 entries) |
| --- | --- | --- |
| linux-gcc-release | 731 s | **62 of 62** (766 s; first run 4 failures, 605 s, see below) |
| linux-clang-release | 786 s | **62 of 62** (759 s) |
| linux-gcc-debug | 596 s | **62 of 62** (2,621 s, alone on the machine) |
| linux-clang-debug | 473 s | **62 of 62** (5,064 s, under load) |
| linux-clang-asan | 994 s | **62 of 62** (1,505 s), no sanitizer report |
| linux-gcc-release `-L slow -j 3` | built | **23 of 23** (3,095 s; longest `-slow-relations` 988 s, `-slow-pantheon` 804 s, `-slow-alliances` 763 s, `-slow-figures` 704 s) |

Determinism lint: self-test 22 snippet files, 0 failures; scan 194 C++ files, 13 CMake files, 0 findings.

The first `ctest` run on `linux-gcc-release` failed four entries, all stale world-hash constants of the kind the integration notes (part 3b) listed, plus three stale test values that the notes had not foreseen. Fixed in tests only (no file under `worldgen/src`, `worldgen/include`, `tools/worldgen-cli/src` or `game/data` changed):
- `worldgen/tests/db_determinism_test.cpp` (`GOLDEN_CASES`, five hashes) and `worldgen/tests/stages_pipeline_test.cpp` (the two hashes of worlds with no data, and the comment "generator 0.30.0"): the world carries the content hash of no data and the generator version in `meta`; both changed with 0.30.0. New values are what the tests printed; the CLI's `empty-seed-0` hash (3e7470...) equals the pipeline test's, an independent confirmation, and they are the same on all five presets.
- `tools/worldgen-cli/tests/cli_report_test.cpp` "cli 12.4: test-pattern world": the layer lines moved from 56 to 57 and on (37 tables since schema 14; the test's own table list already held 37 names and `REQUIRE(lines.size() == 61)`, the indices were stale).
- `tools/worldgen-cli/tests/cli_regions_test.cpp`: `table faction` joins the list of tables the `--until regions` run does not write.
- `tools/worldgen-cli/tests/cli_history_settings_test.cpp`: the expected `generator_version` is 0.30.0 (it said 0.29.0).

## 2. Hashes and goldens

`tools/ci/world-hashes.sh generate` for each preset, then `compare` over the five: **all 16 cases identical** (detail lines, world report, both history reports, territory and markers hashes; 6 files compared per case). Then `world-hashes.sh expect linux-gcc-release` matches the re-recorded files (all 16 ok). Recorded: `worldgen/tests/golden/p0-worlds.txt` (8), `p1-worlds.txt` (4), `p3-worlds.txt` (4). Why they changed: every world file carries the content hash of the shipped `game/data` and the generator version, both of which changed with 0.30.0 (pantheon data, gods, culture forms, 0.30.0 tuning), so even the empty worlds differ. No golden under `worldgen/tests/golden/` other than these three was touched.

| Case | SHA-256 |
| --- | --- |
| empty-phrase | `c29102a9566942749f80ee743a5ba0dbc631958023427eb59d5db4b3af953bde` |
| empty-seed-0 | `8555436f1982269d971f8f816b49da22f36f167f4b090c7d3634585725c91177` |
| empty-seed-0-512 | `074c75b1c1817da80f09cec773b13108ecfea79481f852840e204d5456dd3330` |
| empty-seed-1 | `f294b4c7c02f4aa1c166b6515a901ff6cb9566f1fcc74662bcd71601db1efcef` |
| empty-seed-42 | `f6652e4c6959017b3c3b5856f329a3712f47c94863c325fbe2e894744648025b` |
| empty-seed-max | `da6f576693267f9ee39efe4d3d4d23fabf7f715c17c101c540fdbdbdfb735aa4` |
| geo-phrase-512 | `8ab3aeba103045f3c1c331d7bd7e98995be52d2229ee4ef65bfcc119830cfeea` |
| geo-seed-0-512 | `eca5ac9e14ed3fb27e08505c48310f9ac50f5551efc5a36c112ff1a6300ae4b1` |
| geo-seed-1-1024 | `efdf62b228d2ee59528b18a7a7337f2ed4cf7514d50377cfe89e07bea56a0663` |
| geo-seed-42-512 | `08cf06927cb5f832ce5482e6edbf0c1d56943466c63aad1a7f83602952aa02f6` |
| hist-dense-1024 | `4e131ac6541e9e582e403ce680a47b9d34bd8bfcf3d921a59c5c0c21223088e5` |
| hist-races-elf-orc-1024 | `929028e1b179493eff4ad0ce7969d25837c255bd397656dfc4302093623e904a` |
| hist-span-2000-512 | `b6c57551a955d2622e14d9ff935ae757ea2265e50ab53027ecc9ceaf50abf123` |
| p5-monsters-high-1024 | `e7413a430d684fe8597f630280ac6cfc4fe774c2a58c081c5d491cf502063fb6` |
| pattern-seed-0 | `89664d515f8fc308347ca24b289be960317cbf0c972d2dad555dd1eff0891593` |
| pattern-seed-42 | `5af92a796a5efa3367a9d02ba1b032cda880d0e342cd6b135737a218c1860a6f` |

Phrase world "The Unmapped" (default size 1024): file hash `28027c74406d716703122a68a2328b854397af28050a8f41d344dfec83d4c57b` (0.29.0: `ee31c8ab...`; the README hash needs updating by the lead); `hash.txt` in the phrase folder.

Determinism spot checks: seeds 3, 17, 29, 41, 58, 76, 94 and 123 at 1024 generated with `linux-gcc-release`, `linux-clang-debug` and `linux-clang-asan`: **all eight byte-identical across the three**. Seed 7 generated with the default, `--threads 1` and `--threads 4`: identical (`856799b2...`); seed 0 generated twice: identical files.

## 3. Sweeps

Release build (gcc), `generate --map-size N --data game/data`, `report --history` at both levels and the world report, then `m10.py` metrics (the calibration scripts, copied unchanged), five worlds at a time: default seeds 0 to 99 (A) and 100 to 199 (B); `--divine-activity meddling` and `distant` on seeds 0 to 49; `--monster-pressure high`, `--civs-per-race 4` and `--event-intensity turbulent` on seeds 0 to 19; 20 worlds at 512 (seeds 0 to 19) and 6 at 2048 (seeds 0 to 5): **326 worlds, 0 generate or report failures, no stderr.** (200 + 100 + 60 + 20 + 6.)

Timing, `generate --seed 0 --map-size 1024` on the quiet machine: **25.8 s user, 9.4 s wall** (two runs, 25.81 s and 25.77 s; 0.29.0: 25.7 s and 25.6 s; peak 116 MB). Mean wall per world with five in parallel and the other jobs running: 28.0 s at 1024, 7.2 s at 512, 75.7 s at 2048 (three in parallel).

## 4. Targets

A = default seeds 0 to 99, B = 100 to 199. The engine changed after the calibration table was measured (E1, E3, E4); the numbers below are as measured now. Counts of 100 worlds.

| Target | Bound | A | B | Result |
| --- | --- | --- | --- | --- |
| PN1 acts median; max | 8-25; <=60 | 13; 30 | 14; 28 | pass |
| PN3 worlds with an act of a good / neutral / evil god | >=80 each | 98 / 100 / 91 | 97 / 100 / 87 | pass |
| PN4 bless / curse / champion / monster / mark (share of acts) | each 5-45%; champion >=5% | 27.5 / 33.3 / **5.2** / 11.1 / 22.9 | 26.8 / 30.8 / **6.3** / 11.6 / 24.5 | pass (champion just inside on A) |
| PN5 worlds with a champion; share that fights | >=30; >=50% | 47; 71.8% (51 of 71) | 61; 71.3% (67 of 94) | pass |
| PN6 sent monsters alive at E (share of living) | <=20% | 11.4% | 9.3% | pass |
| PN7 `divine_dead` median share of population | <=3% | 0.45% | 0.44% | pass |
| PN8 flare-ups median (worlds with one) | <=3 | 2 (86) | 2 (89) | pass |
| CU1 cults median; worlds with a cult | 2-6; >=85 | 3; 98 | 3; 99 | pass |
| CU2 cults in 2+ realms | >=40% | 87.9% | 90.1% | pass |
| CU3 worlds with a faith | >=50 | 97 | 99 | pass |
| CU4 worlds with a suppression | 20-70 | 61 | 60 | pass |
| CU5 worlds with a hidden cult at E | 10-50 | 14 | 18 | pass |
| CU6 prophets among founders | <=70% pooled (lead ruling) | 61.7% (193 of 313) | 70.1% (235 of 335) | pass pooled **66.0%** (428 of 648); B alone over 60% and 70% |
| FU1 wakings before E-500 | >=35% | 59.7% | 58.0% | pass |
| FU2 `alliance_renewed` median | <=8 | 7.5 | 6 | pass |
| FU3 towers ended at E | >=25% | 69.3% | 69.7% | pass |
| C1 wars median | 22-28 | 26 | 24.5 | pass |
| G2 standing median (mean); population median | >=81; >=205,000 | 82 (84.0); 205,953 | 86.5 (88.3); 216,108 | pass (A population by 953) |
| M4a living monster at E | >=85 | 96 | 98 | pass |
| M4b lair on a former settlement | average >=65, each >=60 | 77 | 82 | pass |
| T16 repeated battles per thousand | <=2 | 0.80 | 0.73 | pass |
| T18 summary median; max (lines) | <=700 | 662.5; 779 | 657; 882 | pass (B max 882, reported) |
| GW4 goblin regions median; mean | median <=12 (guard) | 12; 12.56 | **14**; 13.69 | A pass, **B misses the guard** |
| DP9/GW5 dwarf regions median; mean | >=2 | 2; 2.15 | 2; 2.17 | pass |
| Regions held, median (mean): human, elf, orc, faerie | reported | 4 (4.63), 3 (3.45), 3 (3.74), 3 (2.82) | 3 (4.86), 3 (3.70), 2 (3.32), 2 (2.68) | - |
| AL1 formed / standing median; AL5 wars with an ally | AL5 20-45% | 14.5 / 9.5; 29.5% | 14 / 10; 30.7% | pass |
| DP1 worlds with a depth-3 mine | 40-90 | 46 | 44 | pass |
| DP2 worlds with a waking; most in one | 25-55 (average of A and B); <=4 | 51; 2 | **57**; 3 | pass: average 54 |
| DP3 sealed share; DP6 alive; DP7 slain | 30-65%; >=70%; <=25% | 53.2%; 85.5%; 14.5% | 55.1%; 92.8%; 7.2% | pass |
| DP4 worlds with a hold lost; most in one | 15-40; <=4 | 23; 4 | 26; 4 | pass |
| DP5 realms ended by the deep | <=8%, >=1 in 200 worlds | 0 of 144 | 0 of 157 | **0 in 200 worlds** (under the floor of 1 in 200; reported, see finding 9) |
| GW1 depth-3 warren worlds; GW2 goblin share of wakings; AD1 aided | 10-50; 10-30%; 10-40% | 30; 11.3%; 24.2% | 24; **7.2%**; 18.8% | pass; GW2 on B under 10% (the lead accepted 6%) |
| P dwarves in an alliance or trade | >=85% | 89% | 88% | pass |

The PN champion share is the one to watch: 5.2% on A against the bound 5%, with the weight at its cap of 1,000 (calibration, remaining misses 3).

Other P5-08 counts (reported): `tower_taken_up` in 28 and 31 worlds (33 and 39 events), cults founded in 98 and 99 worlds (313 and 335 in all), `divine_monster` events in 75 and 83 worlds (152 and 172 sent monsters), flare-ups 203 and 225 in 86 and 89 worlds, stirred wakings 25 and 29.

### Option sets

Counts are of the set's worlds; bounds are the defaults' and apply as guards only (the calibration set none for these sets).

| Set | Results |
| --- | --- |
| `meddling` 0-49 | acts median **24** against 12 at the default on the same seeds = **2.00 x** (PN2 >= 1.8 met; max **61**, over the default's 60); every world has an act; sent monsters alive **20.7%** (default bound 20%); T16 **2.85** per thousand (over 2); cults median 4 in 50 worlds; CU5 14 worlds; standing median 78, population 204,717; living monster 50 of 50 |
| `distant` 0-49 | acts median **4 = 0.33 x** (PN2 <= 0.6 met), an act in **50 of 50** worlds (PN2 >= 30 met); cults median 1 (39 worlds with one), hidden at E in 3 worlds; T16 1.55; standing 81.5; dwarves in an alliance or trade 84% |
| `--monster-pressure high` 0-19 | acts median 14.5; cults 4; 13 wakings worlds; DP4 most 4; standing median 74.5, population 194,260; T16 1.56; CU5 8 worlds; dwarves in an alliance or trade 80%; no break |
| `--civs-per-race 4` 0-19 | acts median 30; cults median 6 (all 20 worlds); wars median 108.5; summary median 945 (max 1,144); renewals median 35; T16 2.72; standing 131.5 |
| `--event-intensity turbulent` 0-19 | acts median 17; cults 4; wars 34; summary median 716.5 (max 1,013); population 202,511; T16 3.53; DP4 most 5; renewals median 9.5; dwarves in an alliance or trade 80% |

### Sizes

| Size | Results |
| --- | --- |
| 512 (20 worlds) | acts median 8 (max 18); cults median 2 (19 worlds with one); **no hidden cult at E**; every default-bound count scales with the map (wars median 15.5, standing 33.5, T16 23.5 per thousand as at 0.29.0); summary median 552; no failure |
| 2048 (6 worlds) | acts median 25.5 (max 32); cults median 4.5; **flare-ups median 5.5** (default bound 3); champion worlds 2 of 6; wars 89; standing median 312.5, population 844,789; summary median 981.5 (max 1,270); no failure |

## 5. Findings

Misses against the default bounds, as measured:
1. **GW4 on B**: goblin regions median 14 (bound 12; A 12, mean 12.56 and 13.69). The guard was 14 on B in the calibration too.
2. **CU6**: B alone 70.1% and A 61.7%; the lead's pooled bound (66.0% of 648 founders) holds. Both samples were over the spec's original 60%.
3. **DP2 on B**: 57 worlds with a waking; the average of A and B (54) holds.
4. **Narrow margins**: PN4 champion share 5.2% (A); G2 population median 205,953 against 205,000 (A); PN8 and the meddling sent-monster share 20.7%.
5. **`meddling`**: sent monsters alive 20.7% (bound 20% for defaults), T16 2.85 per thousand (above 2), one world with 61 acts (default bound 60). Not default bounds; reported.
6. **`high` and `turbulent`**: dwarves in an alliance or trade 80% (bound 85%); turbulent T16 3.53 and DP4 most 5. These sets were never bounded.
7. **2048**: flare-ups median 5.5 (bound 3), six worlds.
8. **No hidden cult at 512** (0 of 20 worlds) and no champion in 11 of 20 worlds at 512; the bounds are per 100 worlds at 1024.
9. **DP5**: no realm ended by the deep in 200 default worlds (P5-07 had 1 and 0). The ">= 1 in 200" floor of the P5-07 table is not met on these two samples; the P5-07 acceptance (Q-P5-07-5, 8% is a ceiling) stands.
10. **T18 summary max** 882 on B (the bound is on the median).

## 6. Defect reports

No stage-logic defect. No new check-world false positives were looked for (reference `check-world` scripts were not run in this round; the integration notes say 55 of 56 worlds agree, d8 being a known E3).

**D1 (test infrastructure, fixed): stale constants after 0.30.0.** Seven values in five tests, section 1. Cause: the notes (3b) foresaw the world-hash constants and the `cli 12.4` / `cli P1-06` / `cli P3-01 8.3 test 10` entries but not the `lines[56]` table index, the `later` table list and the `0.29.0` string in the CLI tests; the pipeline test's comment is updated. Not engine code.

## 7. Not measured

- Windows and MSVC (no machine); `gdext-headless-check` (extension off); `worldgen-cli` reference `check-world` scripts and `p3_report_reference.py compare` on the sweep worlds; no map images (no geography change); `--monster-pressure low`, `--event-intensity calm`.
- `linux-clang-debug` ctest time (5,064 s) and the other presets' times were taken while the sweeps ran; only `linux-gcc-debug` (2,621 s) was quiet.

## 8. Longest fast entries on `linux-gcc-debug` (quiet machine; CI's MSVC Debug is slower still, limit 3600 s per entry)

| Seconds | Entry |
| --- | --- |
| 469 | `worldgen-tests-figures-property` |
| 434 | `worldgen-tests-relations-p504a` |
| 408 | `worldgen-tests-alliances-p504a-b` |
| 391 | `worldgen-tests-figures-p504a` |
| 386 | `worldgen-tests-depths-property-b` |
| 386 | `worldgen-tests-pantheon-property-c` |
| 377 | `worldgen-tests-depths-property-a` |
| 368 | `worldgen-tests-pantheon-property-b` |
| 362 | `worldgen-tests-other` |
| 354 | `worldgen-tests-alliances-property` |

On `linux-clang-debug` under load the longest was `worldgen-tests-depths-property-a` at 933 s in the first minutes of its run. On MSVC Debug the P5-07 review measured 1,700 to 2,300 s for the longest entries; the entries above run at 354 to 469 s here, so a 4 to 5 times factor would put the first at about 2,300 s, inside the limit.

## 9. Artifacts

`~/.cache/the-unmapped/out/p5-08-validation/`: `logs/` (builds, `ctest-*.log`, `ctest-slow.log`, `hashcompare.log`, `hashes.txt`, `lint.log`, `ctest-times.txt`, `build-times.txt`), `sweep/<tag>/` (`table.txt`, `all.jsonl`, `times.txt`, per-seed `summary.txt`; tags `d0`, `d1`, `med`, `dist`, `high`, `civs4`, `turb`, `s512`, `s2048`), `scripts/`, `timing/`, `det/` (`result.txt`, `threads.txt`), `phrase/` (world file, report, both history reports, hash), `critic/` (16 worlds, each `sN/summary.txt` and `full.txt`, and `README.txt` saying which world shows what).
