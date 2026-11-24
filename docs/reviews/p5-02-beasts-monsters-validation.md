# P5-02 beasts and monsters: validation (generator 0.23.0)

Validated by the validation specialist on 2026-11-23 on branch `p5/02-validation` (from `origin/p5/02-cand` at 322049e). Generator 0.23.0, schema 9, history report format v4. Spec: `docs/specs/p5-02-beasts-monsters.md` r2, targets in its section 16. Form: `docs/reviews/p5-12-renamed-regions-validation.md`.

Verdict: **every target of section 16 that is bounded for the default sweep is met, with one miss that is a matter of sample: T18 for `civs4` is 1,101 on the 20-world sample of the spec (bound 1,100) and 1,089 on 30 worlds.** All five Linux presets pass `ctest` (the only failures were the world-hash tests, which pass after recording), the sixteen `world-hashes.sh` cases are identical on all five, the goldens are recorded, and the slow suite fails only in the four known witness assertions. No generator defect found. Two findings concern test tooling (section 7). MSVC and Windows were not available.

Machine: 6 cores shared with three other agents (load 3 to 14 during builds and sweeps), so the `ctest` times are inflated; timing was re-measured on a quiet machine (section 6).

## 1. Presets

| Preset | Build | `ctest` | Result |
| --- | --- | --- | --- |
| linux-gcc-release | 1,290 s | 819 s | 39 of 41 pass; the two failures are the known hash tests (`worldgen-tests-other`, `worldgen-cli-tests-other`) before recording. After recording and a rebuild, those entries and the three others that read goldens pass (5 of 5) |
| linux-clang-release | 1,199 s | 697 s | same two, same values; 5 of 5 after recording |
| linux-gcc-debug | 762 s | 2,060 s | built before my edit of the test constants reached it: one failure (`worldgen-tests-other`, the five `GOLDEN_CASES` and the two inline hashes). The values it produced are the values of gcc-release (cb87e9d2..., ba0734be...). After the rebuild: 5 of 5 pass |
| linux-clang-debug | 568 s | 2,385 s | **41 of 41 pass** (built with the recorded constants) |
| linux-clang-asan | 889 s | 1,185 s | **41 of 41 pass**, no sanitizer report |
| linux-gcc-release-slow | (built) | 2,340 s | 15 of 17 pass; the two failures are exactly the four known witness assertions: `conflict_engineer_test.cpp:517`, `history_balance_golden_test.cpp:166`, `history_report_worlds_test.cpp:207` and `:210` |

Determinism lint was not re-run (no C++ under `worldgen/src` changed). Logs: `~/.cache/the-unmapped/out/p5-02-validation/logs/`.

## 2. Cross-preset hashes and goldens

`tools/ci/world-hashes.sh compare` over `linux-gcc-release linux-clang-release linux-gcc-debug linux-clang-debug linux-clang-asan`: **all 16 cases identical** (detail lines, world report, both history reports, territory hashes and markers hashes of each case with history; 6 files per case with history). `expect linux-gcc-release`: 16 of 16 ok after recording.

New case (spec 18, P5-00 5): `p5-monsters-high-1024` = `--seed 7 --map-size 1024 --monster-pressure high`, in `HIST_CASES` of `tools/ci/world-hashes.sh` (so it is compared against `p3-worlds.txt`); `cli_history_test.cpp` now states sixteen cases. **CLAUDE.md still says "fifteen cases" in two places (lines 156 and 171): for the lead.**

Recorded (LF, C-locale order): `worldgen/tests/golden/p0-worlds.txt` (8), `p1-worlds.txt` (4), `p3-worlds.txt` (4); `db_determinism_test.cpp` `GOLDEN_CASES` (5) and `stages_pipeline_test.cpp` lines 496 and 509 with their comments. Not touched: every other golden file. Values come from the 0.23.0 build; all five presets agree; the unit-test values (no data) agree between gcc and clang.

| Case | SHA-256 |
| --- | --- |
| empty-phrase | 5fe725ed1b0e2d6f176bbd225092f7cbb1145691abef664bc2db21dcec832e44 |
| empty-seed-0 | 742285c70e228ab52b997a267aabf8a9d8c7237e64de5835acc948087f153583 |
| empty-seed-0-512 | f3b2527efeb8d7d4c2d5b55b2c0b34cf576f4af3749a4292cc0122e71a9765b7 |
| empty-seed-1 | 592bc0364bbe0032e071e17d87646f00a53c813d85ba69a89950ccc12d275197 |
| empty-seed-42 | 0fe04b270a62f849c4612d1cec62cacdd656599e5057f9486324ae6916e1e965 |
| empty-seed-max | 55e399fbb420cc99393f77c51cff17ba8db60f872c9121b5bccc45543685d9c3 |
| pattern-seed-0 | dfb65d71b28697903c22867d76e34f6acb4858428e4192915b23098db3451083 |
| pattern-seed-42 | 4f1c66a4c9f198c8deaa5cac82ba6b929ef9a2574bf5fc080d6a0d3e256fdfbb |
| geo-phrase-512 | 2c6c6fc5156c7a31d67abaacea47a396560861f90c09143304135e6d89129cfc |
| geo-seed-0-512 | e9ccfbf149b1742fe86367716e63f764ef192829414a7b40866bb41010652e45 |
| geo-seed-42-512 | e42985b91c908bb3dab6b9e007ec23fcb38ed4e51715451f7c1de8b0f7e0611e |
| geo-seed-1-1024 | d4908d312d6573c95131f50a256db12193be35ca617dbfc2731c6f3f62a94596 |
| hist-dense-1024 | eeda16a302d7a1c6e087cf199b633bba037a8536278313ecac55e4f932029519 |
| hist-races-elf-orc-1024 | c4bc54f68b6ed26feea12aed1df7117c063f2c6263ff589dc13801a52eca28f5 |
| hist-span-2000-512 | 7ec5796a11a12376f10788c32406081c67eda06683609b42afd3c69104f6f8c5 |
| p5-monsters-high-1024 | 4119cce93c8165d80fbbe519eb41a8e8570656e89e0737963db12560f3d879dd |

Why they move: every world carries generator 0.23.0 and schema 9 (and the empty world grows by the new tables' pages); the cases with data also carry the new `content_hash` (kinds `beast` and `monster`, `history.monsters`, `site_forms`, report entries, renaming rule 7); the history cases change through every event id and draw.

**README phrase world** (`generate --seed-phrase "The Unmapped"`, 1024, default data): file SHA-256 (`hash --db` and `sha256sum` agree) **`531e39e1446d17df66bcf434374889fe87814c910f964e9fee9cc6b7fc48f393`**, `content_hash` `d66f378d0007201836a083d60a0fca7cb48c8096b5b4e04c8c98ccd8ba488955`. Files: `~/.cache/the-unmapped/out/p5-02-validation/phrase/{world.db,world.report.txt,summary.txt,hash.txt}`. (The README still shows the 0.22.0 hash `0c54c4be...`.)

## 3. Sweeps

Worlds, all at the shipped data, release build (the gcc-release binary, copied to `sweep/bin/`), 1024 unless stated: `default` seeds 0 to 99; 30 each (seeds 0 to 29) for `low` and `high` (`--monster-pressure`), `calm` and `turbulent` (`--event-intensity`) and `civs4` (`--civs-per-race 4`); `d512` and `d2048` seeds 0 to 9. 270 worlds, every `generate` exit 0, every `report --history` at both levels exit 0 with empty stderr (540 calls). Independent references (the test writer's, run unchanged) on 170 worlds (default 100, `high` 30, `civs4` 30, `d512` 10): `p3_report_reference.py compare` equal at both levels in 170 of 170 (T19); `p3_conflict_reference.py check-world` 0 failures in 170 (794,546 checks: T3, T6, T7, invariant 18, P3-00 12.3); `p5_02_monsters_reference.py check-world` 0 failures in 170 (1,398 monsters, 4,586 sites); `p5_12_renaming_reference.py check-world` 0 problems in 169 of 170 and one false positive (section 7). The measurements are the engineer's scripts (`m502.py`, `t501.py`, `t16.py`, `g2.py`, `t10_12.py`, copied to `scripts/`) run on my worlds; I checked that they reproduce the engineer's figures (M4 70, T18 599.5 and 774, T16 1.60).

### Section 16 targets, default sweep (100 worlds)

| Target | Bound | Measured | |
| --- | --- | --- | --- |
| M1 monsters per world | median 4 to 8, max at most 20, at least one in 95 worlds | median 8, max 14, 100 worlds | met |
| M2 participation | each kind in 10% of worlds; each kind once in `low` | worlds with the kind: dragon 65, giant 48, ogre 76, troll 93, undead 75, wyvern 65; all six kinds occur in `low` | met |
| M3 raids | at least one in 90 worlds | 98 (median 39.5 raids) | met |
| M4 living monster's lair at E | at least 85 worlds | **87** | met (margin 2) |
| M4 lair on a former settlement open at E | at least 65 worlds | **70** | met |
| M4 overruns per world | median 2 to 4 | median 3, max 8 | met |
| M5 slain among ended | 20% to 70% | 317 of 634, 50% (dragon 9/9, giant 18/53, ogre 46/142, troll 121/231, undead 87/98, wyvern 36/101) | met |
| M6 invariants 3 to 7 | every world | none broken (every sweep, all 270) | met |
| G1 ghost places | median at least 3 | median 3, max 9 (endings: sacked 1,194, overrun 295, plague 212, famine 97) | met |
| G2 standing settlements at E | median at least 81 (92% of 88) | median 83 (94.3% of 88; sums 91.8%) | met |
| G2 population at E | median at least 214,500 | 217,580 (96.4% of 225,789) | met |
| G3 once-held regions free at E | reported, expected median 2 | median 1 | reported (below the expectation; not a bound) |
| S1 outposts | median 3 to 15; ended in at least 70 worlds | median 12, max 29; 92 worlds | met |
| B1 `beasts_bold` | median at most 30 | median 3, max 10 | met |
| F1 monster flare-ups | reported; at most 2 in any world | 90 over the sweep, max 2 | met |
| R: `ravage` renamings | at least 5 | 145 | met |
| R1 renamed regions per world | median 3 to 6; at least one in 90 worlds | median 5, p10 3, max 8; 99 worlds | met |
| R2 caps | every world | hold in every world | met |
| R3 by rule | none above 60%; each at least 10 | 496 renamings: capital_sack 164 (33.1%), ravage 144, slaughter 61, conquest 41, heartland 36, plague 33, great_sack 17 | met |
| R4 dropped for want of a name | at most 1% of candidates reaching item 2.6 | **0 of 503** reaching (798 candidates), all 100 worlds 0 | met |
| T16 repeated battles | at most 2 per thousand; mixed toll clauses 0 | 13 of 8,123 = **1.60 per thousand**; mixed 0 | met |
| T18 summary lines, default | median at most 700, max at most 800 | median **599.5**, max **774** (seed 1); top values 729, 741, 774; min 360 | met |
| timing | within 10% of 0.22.0; `report --history` under 2 s | section 5: 9.1 s wall, 25.5 s user; report 0.01 s | met |

R4 was measured with a trace build (a scratch copy of this commit with one `fprintf` of `RenamingSubsystem::stats()`; `cmp` shows seeds 0 and 5 byte-identical to the release build); per-world lines in `trace/`.

### P5-01 and earlier targets re-measured (default)

| Target | Bound | Measured |
| --- | --- | --- |
| T1 hollow wars | at most 250 per mille | 215 (453 of 2,399 declared) |
| T2 same-goal-unchanged max | 0 worlds above 3 | 0 |
| T3 no refuge held while a destination exists | 0 | 0 failures (conflict reference, 170 worlds); permitted holds with no destination: 1 world, 3 holds (`civs4` 3 worlds, 25) |
| T4 no race extinct | 0 | 0 |
| T5 orc wins against losses | wins at least losses | 153 / 64 |
| T9 worlds with one standing orc settlement, no war won, no sack | at most 7 | 4 (12, 15, 79, 95) |
| T10 | 0 | 0 |
| T11 median years awakening to first orc act | below 100 | 8.5 |
| T12 first orc war won against lost | won at least lost | 32 / 17 |
| T13 declarations not citing a claim | 0 | 0 of 2,399 |
| T14 grim names | at most 50 per mille | 190 of 4,951 (38.4) |
| T15 names | 0 | none |
| T17 calamity share | median at most 10%, max at most 25% | 7.0% / 17.0% (seed 72) |
| T20 flare-ups shorter than `flare_min_years` | 0 | 0 of 431 |
| T19 `[present]` and report reference | equal | equal in 170 of 170 |

Movement against 0.22.0 (reported, not targets): wars median 23.5 (24); simulated years median 1,372.5 (1,342.5, +2.2%); ruins median 14 (11); regions renamed median 5 (4); settlements median 101 (102).

### Other sweeps (30 worlds; 10 for the sizes)

| Set | M1 median / max | M3 worlds | M4 alive / lair open | overruns med | M5 slain | G1 med | standing med | S1 med | F1 total / max | T18 med / max | T16 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| low | 5 / 10 | 28 | 23 / 12 | 1 | 43% | 3 | 87.5 | 11.5 | 22 / 2 | 609.5 / 786 | 5 of 2,465 |
| high | 11 / 18 | 30 | 28 / 20 | 3 | 55% | 3 | 79 | 12.5 | 39 / 3 | 605 / 768 | 6 of 2,339 |
| calm | 7 / 12 | 30 | 23 / 12 | 2 | 45% | 1 | 79.5 | 7.5 | 28 / 3 | 486 / 666 | 0 of 1,323 |
| turbulent | 7.5 / 12 | 30 | 23 / 17 | 3 | 50% | 8.5 | 90.5 | 17.5 | 27 / 3 | 710.5 / 964 | 10 of 4,134 |
| civs4 | 8.5 / 14 | 30 | 22 / 19 | 5 | 64% | 9 | 150.5 | 32 | 26 / 3 | 1,089 / 1,346 | 22 of 8,670 |
| d512 | 1 / 3 (one world has none) | 9 | 6 / 4 | 1 | 4 of 8 | 3.5 | 38.5 | 6.5 | 4 / 1 | 460.5 / 562 | 9 of 579 |
| d2048 | 24 / 30 | 10 | 10 / 9 | 7 | 51% | 10 | 309 | 43 | 16 / 3 | 1,136.5 / 1,360 | 3 of 2,501 |

Oddities (none is a bound): at 512 one world of ten has no monster and the median is 1 (M1's bound is stated at 1024); `high` raises monsters to 11 and `low` lowers them to 5 as intended, with `low` having no dragon among the ended; F1's "at most 2 in any world" is stated for the default sweep and is 3 in `calm`, `turbulent`, `high`, `civs4` and 2048; T16 is above 2 per thousand outside the default sweep (low 2.0, high 2.6, turbulent 2.4, civs4 2.5, 512 15.5 on 579 battles) where its bound does not apply; calm has a median of one ghost place per world; no race is extinct in any set (T4, 0 in the default sweep).

### T18 for `civs4`

The spec's sample is 20 worlds (seeds 0 to 19): summary lines sorted 847, 933, 983, 993, 1008, 1028, 1034, 1088, 1096, 1096, 1106, 1107, 1114, 1118, 1121, 1160, 1162, 1223, 1339, 1346; **median (1,096 + 1,106) / 2 = 1,101**, one above the bound of 1,100 (max 1,346, bound 1,400, met). Over the 30 worlds of this sweep (seeds 0 to 29) the median is **1,089** (met; values 1,088, 1,090 and 1,096 around it). The figure is a coin toss around the bound: two worlds either side of 1,100 decide it. This is the one open item (Paul's: the spec 16 note says accept, raise or trim).

## 4. Phrase world and critic material

`~/.cache/the-unmapped/out/p5-02-validation/phrase/` (section 2). `critic/`: `default-s0` to `default-s11` and `high-s0` to `high-s3`, each `.full.txt` and `.summary.txt` (report v4, from the sweep), and `render --composite territory --markers` at the end year for `default-s0`, `default-s1`, `high-s0`, `high-s1` (`*.territory-end.markers.png`). The worlds are kept in `sweep/default/` (seeds 0 to 11) and `sweep/high/` (0 to 3); the other worlds were deleted for disk, reports kept.

## 5. Timing

Quiet machine (load 1.5), release build, `generate --seed 0 --map-size 1024`, three runs: 9.14, 9.09, 9.11 s wall, user 25.5 s. The 0.22.0 figure of the previous validation (12.2 s wall, 26.2 s user, load 5 to 7) is on a loaded machine; user time is the comparable figure and 0.23.0 is not slower (-3%). No 0.22.0 binary remains to run side by side. `report --history` summary and full: under 0.01 s. In the loaded sweep, the median `generate` of the default set was 25.5 s wall at two jobs.

## 6. Artifacts

`~/.cache/the-unmapped/out/p5-02-validation/`: `sweep/<set>/` (reports for all 270 worlds, `gen-times.txt`, `jobs.txt`), `results-<set>.txt` (all measurements per set), `results-r512-default.txt`, `refs-{cmp,conf,ren,mon}.txt` and `refout/` (reference output per world), `trace/` (R4 trace lines), `world-hashes/` (the case texts of the five presets), `logs/` (builds, ctest, world-hashes, `summary.txt`, `summary2.txt`), `scripts/`, `phrase/`, `critic/`.

## 7. Defects and advisories

No generator defect. For the test writer and the lead:

1. **`p5_12_renaming_reference.py check-world` reports a false positive when a region is renamed first by `great_sack` and then by `capital_sack`.** Reproduce: `python3 -B worldgen/tests/reference/p5_12_renaming_reference.py check-world <db>` on `generate --seed 5 --map-size 1024 --monster-pressure high`: "region 11: the later row's rule weight 90 is not above 90". Rows: e341 (year 4324, after the sack e339, `The Cinder Downs`, a `great_sack` form, weight 80) and e472 (year 5067, after the sack e470, `Snank's Fall`, a `capital_sack` form, weight 90): the second is weightier, as P5-12 6 item 2.2 requires. Cause: `max_weight` (line 505) takes the largest weight among all rules that share the cause kind `sacked`, which spec P5-12 r2 section 8 item 2 says is not enough (the exact rule is not stored). The check can only be exact with the form text or with the subsystem's log. The generator's own stage checks pass. Not a world defect; the other 169 worlds have no case.
2. **The engineer's `r512.py` (not a repository file) flags "inv4 no form" in 7 default worlds** for names such as `Ashos' Hearth`, `Omos' Fall`: its form matching predates the possessive rule of P5-02 21.2 (a stem ending in `s` takes `'`, not `'s`). The reference check-world passes all of them, so I take it as a script limit.
3. **CLAUDE.md** states fifteen cases of `world-hashes.sh` (lines 156 and 171) and the README carries the 0.22.0 phrase hash: for the lead. CI will also compare MSVC against these recorded hashes.
4. The four known slow witness assertions remain failing in `worldgen-tests-slow-engine` and `worldgen-tests-slow-history-report`; a test writer replaces them on another branch.
