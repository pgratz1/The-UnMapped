# P5-03a relations follow-ups: validation (generator 0.25.0)

Validated by the validation specialist on 2026-11-24 and 2026-11-25 on branch `p5/03a-validation` (from `origin/p5/03a-int` at 9db4d1c). Generator 0.25.0, schema 10, history report format v6. Spec: `docs/specs/p5-03a-relations-followups.md` r1, targets in its section 11, with the bounds ruled by the lead where they differ (listed in section 4). Form: `docs/reviews/p5-03-race-relations-validation.md`. MSVC and Windows were not available (CI).

Verdict: **pass. No generator defect found.** All five Linux presets are fully green after recording (43 of 43 `ctest` entries on each) and the slow suite on `linux-gcc-release` is green (section 1); the sixteen `world-hashes.sh` cases are identical on all five presets and `expect` matches the recorded goldens (16 of 16) on all five. The sweeps (370 worlds) had no failed `generate` or `report --history` run and no stderr output. Of the lead's bounds, **two are missed and one is borderline** on the second default sample: RR5 (26 worlds share one end tally against at most 25, seeds 100 to 199; 20 on seeds 0 to 99), RR2d's 55% (54% on seeds 100 to 199; 57% on seeds 0 to 99) and the P5-02 target M4 "a living monster at the end in at least 85 worlds" (82 and 83 against 85; 0.24.0 had 87 and 89), which traces to the intended U1 change (finding 2). Paul's lair figure is 66 on seeds 0 to 99 (bound 65, met) and **53 on seeds 100 to 199** (0.24.0: 66). Reference scripts: one false positive of `p5_03_relations_reference.py` invariant 4 (finding 1).

Machine: 6 cores shared with other agents (load 10 to 26 for most of the run), so `ctest` and sweep times are inflated.

## 1. Presets

Cold builds with the default job count, then `ctest --preset <p> -j 3`.

| Preset | Build | `ctest` before recording | `ctest` after recording and rebuild |
| --- | --- | --- | --- |
| linux-gcc-release | 511 s | 41 of 43 (`worldgen-tests-other`, `worldgen-cli-tests-other`: hash constants and goldens only) | **43 of 43**, 918 s |
| linux-clang-release | 456 s | not run (the run was restarted after recording) | **43 of 43** |
| linux-gcc-debug | 335 s | not run | **43 of 43**, 3,339 s |
| linux-clang-debug | 332 s | not run | **43 of 43**, 4,583 s |
| linux-clang-asan | 1,071 s | not run | **43 of 43**, 970 s, no sanitizer report |
| linux-gcc-release-slow | (built) | | see below |

The two failures before recording were exactly the expected ones: `worldgen-cli-tests-other` (the P0, P1 and P3 golden files; `cli_golden_test.cpp`, `cli_p1_generate_test.cpp`) and `worldgen-tests-other` (`db_determinism_test.cpp` `GOLDEN_CASES`, `stages_pipeline_test.cpp` lines 496 and 509). Nothing else failed. Logs: `~/.cache/the-unmapped/out/p5-03a-validation/logs/` (`ctest1-linux-gcc-release.log` before, `ctest2-*.log` after).

Slow suite (`ctest --preset linux-gcc-release-slow -j 3`, 3,544 s): first run **18 of 19**, `worldgen-tests-slow-engine` failed on its two seed 34 witnesses (finding 8, a test witness that moved; no world defect). After the witness was moved to seed 4 (test infrastructure, two files), `worldgen-tests-slow-engine` passed (317 s, `-R 'worldgen-tests-slow-engine$'`); the other 18 entries passed in the first run (including `-slow-relations` 1,058 s, `-slow-history-report`, `-slow-monsters`, `-slow-renaming`, `-slow-timeline`, `-slow-markers`, `-slow-browser`). `worldgen-tests-partition` passes. The test sources changed only in these two slow cases, so the release, debug and asan runs of section 1 (which exclude the slow label) are unaffected.

The determinism lint was not re-run (no C++ under `worldgen/src` changed on this branch).

## 2. Cross-preset hashes and goldens

`tools/ci/world-hashes.sh local linux-gcc-release linux-clang-release linux-gcc-debug linux-clang-debug linux-clang-asan` before recording, and `generate` plus `compare` again after the rebuilds: **all 16 cases identical** on the five presets (detail lines, world report, both history reports, territory and markers hashes; 6 files per case with history). `expect` against the recorded files: **16 of 16 ok on each of the five presets.** No new `world-hashes.sh` case (the sixteen stay).

Recorded (LF, C-locale order; values from the gcc-release build, all five presets agree): `worldgen/tests/golden/p0-worlds.txt` (8), `p1-worlds.txt` (4), `p3-worlds.txt` (4); `db_determinism_test.cpp` `GOLDEN_CASES` (5); `stages_pipeline_test.cpp` lines 496 and 509 with their comments. `cli_p1_generate_test.cpp` holds no inline hash (it reads the golden files). Every other golden file is untouched.

Why they move: every world file carries the generator version (0.25.0) in `meta` and the content hash of the shipped `game/data` (the P5-03a data: report, monsters, relations, conflict, renaming tuning), so even the empty worlds change; the history worlds also change through the P5-03a rules themselves.

| Case | SHA-256 |
| --- | --- |
| empty-phrase | e3a7e9054c236d79b81ee591aba3b7712163695fa4c7869aefbc61412847601c |
| empty-seed-0 | 73130450cece7dd415a44406b882d5dfeba1e9e39f733bf5baa20bbcc9ece682 |
| empty-seed-0-512 | 389cd6500207efb23a16480ff3dfebd3449db653f7c157f78c6c696923b84a20 |
| empty-seed-1 | 4af0ddd465eeedf2bd6d41e21322402febf0538276eea6efebaeee989c734574 |
| empty-seed-42 | cf76a6d6051580336e10116610aca6af265b7c6755cc2986350f0d55d9d4112f |
| empty-seed-max | 8f30dbd97cb433645f753c5df19ca30c5842e7095f14e1dfff3e764dc3ed9a49 |
| pattern-seed-0 | 6452af693de5cdab96704c322f7e82bc8272a2629656d6a8c557dc33a914ed52 |
| pattern-seed-42 | 3a5f93e15a181f25c7176920dc32e9d608330b16740210cf8344492ac0132018 |
| geo-phrase-512 | 94d93175f918adbbb1601a7774cb7421425dfb09fe2338524d4b9b2e9a5b4ee7 |
| geo-seed-0-512 | ea8c5d0f5c5aca0d5bea12fe0956bfa714c91fa937f08ba868d742c9f9a0ac4e |
| geo-seed-42-512 | 36a3eb8f8f38f698666d4dd6d1ca80155ef0aa6648f6654a161f400d4561d833 |
| geo-seed-1-1024 | d46591d49eeb2f279ebc5a5fec2def7e7095f20657eb30b138f8eac95f7bfe58 |
| hist-dense-1024 | 8704ec0fed2d22c280f7c9fc56994a7bc7ccc716770c63fefb692123bd186144 |
| hist-races-elf-orc-1024 | c32bb33fca14daeefa40f14039dcbc3653a3be520a0c89191197fcc5a4341471 |
| hist-span-2000-512 | c0df9bcbdbbf6903afeea36a1b210a5976aad421cacfeea0a175470c006e645d |
| p5-monsters-high-1024 | 93c3210aed9b6ae9e15dc918e9ec5fbb0150a6547891d9e7870f95e7e78e9628 |

Unit-test constants (no data, generator 0.25.0): `empty-seed-0` f269a85e7ead85e378438cb696487d7aa0bef31d8460e0d7d1d77edb39639aea (also `stages_pipeline_test.cpp:496`), `empty-phrase` 7f5f61a24ca9648026f33715022b739478c2cec2084a1bb35bc07cf44d068c94, `empty-seed-1` cbd90d4ae116c025633487ee214db3cb7b56e6f4220c5b262c8448d834d64020, `empty-seed-42` ad722ada9176dcc2cca72b63e5bc9f661f6cdfc344617f331dd22d5b50075f74, `empty-seed-max` 61f3352b3c10337e9c6c5bc83eb3860e991098b9f7937e1f19d1457d2b9160c0, test pattern 2b7177fdf69e3d526f5e5ff4c6d1161dbcdf0dbf7ba559938a58253165bc668f.

**README phrase world** (`generate --seed-phrase "The Unmapped"`, 1024, default data): file SHA-256 (`hash --db` and `sha256sum` agree) **`a5e8a8f3341d421ecdfb1bda6e87011d2c40a6afad994a604b4ec2383cbd662a`**. Files: `~/.cache/the-unmapped/out/p5-03a-validation/phrase/{world.db,world.report.txt,summary.txt,full.txt,hash.txt}`. (The README shows the 0.24.0 hash `38f412c5...`.)

## 3. Sweeps

Shipped data, gcc-release binary, 1024 unless stated: `default` seeds 0 to 99 and `default2` seeds 100 to 199; 30 each (seeds 0 to 29) for `low` and `high` (`--monster-pressure`), `calm` and `turbulent` (`--event-intensity`) and `civs4` (`--civs-per-race 4`); `d512` and `d2048` seeds 0 to 9. **370 worlds. Every `generate` and every `report --history` at both levels exited 0 with empty stderr** (`fail.txt` absent in all nine sweeps).

Independent references on 160 worlds (default 100, `high` 30, `civs4` 30): `p3_report_reference.py compare` equal at both levels in **160 of 160**; `p3_conflict_reference.py check-world` **0 failures**; `p5_12_renaming_reference.py check-world` **0 failures**; `p5_02_monsters_reference.py check-world` **0 failures**; `p5_03_relations_reference.py check-world` 159 of 160 pass, one failure (`default` seed 66, finding 1). Results: `refs-{cmp,conf,rel,ren,mon}.txt`, `refout/`.

Names: no empty-name artefact in 740 report files (`known as '`, a name beginning `'s`, `known as 's`: 0 hits; the possessives that match a looser grep are real names such as `Grulimarn's Shadow`).

## 4. Targets (section 11 of the spec, with the lead's bounds)

Measured with the engineer's `measure2.py` and `agg2.py` (copied to `scripts/`). I reproduce the engineer's seed 0 to 99 figures exactly: 33% into friendship, wars median 25, T16 1.65, population 211,977, RR1 99 worlds (linked pairs), summary median 550 and maximum 689.

| Target | Bound (lead) | Seeds 0 to 99 | Seeds 100 to 199 | |
| --- | --- | --- | --- | --- |
| RR1 pairs met | every pair of present races joined by a chain of borders met (openly or by word) in at least 95 of 100 worlds | **99** (raw, counting pairs with no land path: 95) | **97** (raw 95) | met |
| RR3 race that meets no other | excludes a race alone on a land mass | 0 worlds with a land path; 4 worlds with a race on its own land (seeds 44, 75, 86, 87; the 24 unlinked pairs) | 0; 2 worlds (seeds 4, 73; 10 unlinked pairs) | met (reported) |
| RR2a `disposition_turned` per world | max at most 20 (spec: median 2 to 6, at most 15 worlds with none) | median 3, max 16, 12 worlds with none | median 3, max 13, 7 worlds with none | met |
| RR2b turns into friendship | at least 25% | 125 of 375 (33%) | 141 of 396 (36%) | met |
| RR2c turns to a higher band | at least 30%; hatred to wariness at least 5 | 246 of 375 (66%); hatred to wariness 121 | 257 of 396 (65%); 116 | met |
| RR2d turns before 5500; before 4500 | at least 55%; at least 20% | 57%; 23% | **54%**; 24% | met on 0 to 99; 55% missed by one point on 100 to 199 |
| RR4 friendship and hatred pairs at E | medians at least 3 (friendship), at least 3 (hatred) | 3; 4 | 3; 4 | met |
| RR5 worlds sharing one end tally | at most 25 | 20 (tally 4, 8, 3) | **26** (same tally) | met on 0 to 99; **missed by one** on 100 to 199 |
| C1 wars | median 23 to 28 | 25 (mean 27.0) | 25 (26.7) | met |
| T16 repeated battles | at most 2 per thousand | 15 of 9,071 = 1.65 | 9 of 9,170 = 0.98 | met |
| G2 standing settlements; population at E | medians at least 81 and 205,000 | 85; 211,977 | 90; 221,590 | met |
| MX1 mixed standing at E | median 3 to 12; at least 80 worlds | 10; 100 worlds | 9; 97 worlds | met |
| MX3 `people_expelled` per world | median at most 3, max at most 15 | median 2, max 13 | median 2, max 12 | met |
| MX4 share values | no value in more than 15% | top 588 in 0.6% of 1,914 | top 609 in 0.5% of 2,033 | met |
| MX5 summary `settlement_mixed` lines | at most 3 a world | max 3 | max 3 | met |
| U1 `undead_rose` after 5500; risings | at most 55%; 150 to 290 | 48% of 190 | 40% of 186 | met |
| W1 arrival lines with a cause clause | 100% | 661 of 661 | 662 of 662 | met |
| W2 `its lair` for a grouped kind | none | 0 | 0 | met |
| T18 summary lines | default median at most 580, no world over L(C) and none over 800 with C at most 16; `civs4` median at most 1,000, max at most 1,200 | median 550, max 689; none over | median 547.5, max 779; none over | met; `civs4` median 867.5, max 1,086, met |
| Paul's lair figure: worlds with a lair on a former settlement open at E | at least 65 | **66** | **53** (0.24.0: 62 and 66) | met on 0 to 99; **53 on 100 to 199** |
| M4 a living monster at E | at least 85 worlds (P5-02, unchanged) | **82** (0.24.0: 87) | **83** (0.24.0: 89) | **missed** (finding 2) |
| timing | `generate` seed 0, 1024, release: at most 1 s over 0.24.0 | user 25.7 s (0.24.0: 25.5 s); wall 15.9 to 17.3 s on a machine at load 10 | | met on user time; wall not comparable (0.24.0: 9.1 s at load 3) |

Other P5-02 M, G, S, B and F figures on seeds 0 to 99 (`results-m502-default*.txt`): monsters median 9, max 16, all six kinds present; raids in 100 worlds; M5 slain 379 of 693 (55%); no invariant problem (M6 empty); ghost places median 3; outposts median 12; `beasts_bold` median 3, max 12; flare-ups max 3; `regions_renamed` median 5.

Turns by band, seeds 0 to 99: into hatred 106, wariness 144, friendship 125 (0.24.0: 116, 30, 3). Seeds 100 to 199: 113, 142, 141.

### Other sweeps

| Set | wars med | turns med / max | into friendship | RR1 linked all met | RR5 top tally | mixed at E med | expulsions med / max | standing med | population med | T16 per thousand | U1 risings (after 5500) | T18 med / max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| high | 24 | 3 / 9 | 35% | 29 of 30 | 11 | 9 | 2.5 / 10 | 79.5 | 213,990 | 3.25 | 132 (49%) | 544 / 666 |
| low | 24 | 4 / 9 | 38% | 30 of 30 | 11 | | 2 / 8 | 85 | 218,737 | 0.73 | **1** | 526.5 / 674 |
| calm | 13.5 | 5 / 12 | 28% | 30 of 30 | 8 | | 1 / 6 | 78.5 | 232,628 | 1.36 | 49 (53%) | 486 / 611 |
| turbulent | 42 | 3.5 / 10 | 35% | 29 of 30 | 7 | | 4 / 20 | 83 | 198,038 | 4.92 | 69 (45%) | 595 / 827 |
| civs4 | 93.5 | 9.5 / 22 | 39% | 28 of 30 | 3 | 14 | 13.5 / 28 | 149 | 268,825 | 3.14 | 43 (44%) | 867.5 / 1,086 |
| d512 | 16 | 3 / 12 | 52% | 10 of 10 | 2 | | 0 / 4 | 37 | 63,545 | 9.24 (6 of 649) | 16 | 465.5 / 519 |
| d2048 | 86 | 5.5 / 17 | 35% | 8 of 10 | 1 | | 9.5 / 37 | 321 | 920,813 | 1.81 | **0** | 945.5 / 1,126 |

Oddities (no bound applies): `turbulent` has a T18 world at 827 lines; T16 is above 2 per thousand in `high` (3.25), `turbulent` (4.92), `civs4` (3.14) and the 512 maps (9.24 on 649 battles), where the bound does not apply (0.24.0: 2.02, 2.24, 3.11, 9.80); `low` has one undead rising in 30 worlds and `d2048` none in 10 (finding 3); `calm` friendship share is 28%, the lowest.

## 5. Timing

`generate --seed 0 --map-size 1024`, gcc-release, three runs at load 10: wall 17.17, 17.25 and 15.86 s, user 25.70, 25.78, 25.71 s. The 0.24.0 figures (P5-03 report, load 3): 9.12 s wall, 25.5 s user. User time is +0.2 s; wall time here is inflated by the shared machine and cannot be compared. The 0.24.0 binary was not rebuilt for a side-by-side run. Reports: under 1 s each.

## 6. Findings

No generator defect. Numbered for the lead:

1. **`p5_03_relations_reference.py check-world` invariant 4 reports a false positive on `default` seed 66** (1 of 160 worlds): "colony 56's first rows {1: (822, 3), 2: (178, 0)} are not its founder's race". Reproduce: `worldgen-cli generate --seed 66 --map-size 1024 --out s66.db`, then `python3 -B worldgen/tests/reference/p5_03_relations_reference.py check-world s66.db`. Cause: colony 56 (Othela in Yarrowfell, region 20) is founded by civilization 2 (elves) in 3343 from Krizz (s27), which is sacked the same year (e192 siege, e193 sacked), and event e194 `settlement_mixed` at s56 records a second people (goblins, 178 of 1000) in the same year. The census holds one row per entry per year, the year's last state, so the first rows already show the mix; the owner did not change (civ 2 throughout). The spec's amended invariant (12 item 1) accepts a first row altered by a same-year owner change only; it should also accept a same-year `settlement_mixed` or other 5.1 movers' mix. No world defect: the C++ and the other references agree.
2. **M4 "a living monster at the end of history in at least 85 worlds" is missed (82 and 83; 0.24.0: 87 and 89), and Paul's lair figure drops on the second sample (53; 0.24.0: 66; the first sample moved the other way, 62 to 66).** Cause, measured on the kept worlds: in **11 of 100 worlds (13 of 100 on seeds 100 to 199) the only living monster at the end is an undead host risen after year 5500**; U1 (`undead_min_dead` 1000, `undead_dead_permille` 25, the threshold by world population) took the late risings from 74% of all to 48% and 40%, which is what the spec asked for (U1 at most 55%), and removes those end-of-history undead hosts. M4's bound was written for 0.24.0's late undead. Either restate M4 (at least 80 worlds), or accept: a decision for the lead and Paul. The 0 to 99 lair figure meets Paul's 65 only by one world, and the two samples together hold 119 of 200 (59.5%) against 128 (64%) at 0.24.0.
3. **Undead rise almost never under `--monster-pressure low` (1 rising in 30 worlds) or at 2048 (0 in 10 worlds).** Probably the same threshold by world population (a 2048 world's population of about 920,000 puts the undead threshold far above any one place's dead; `low` has few raids). The spec's U1 bound (150 to 290 risings) is for the default sweep, so no bound is missed; flagged because a 2048 world with no undead at all is a statistical oddity. Not debugged to the threshold's line.
4. **RR5 26 worlds (bound 25) and RR2d 54% (bound 55%) on seeds 100 to 199** are one world and one point from the bound; seeds 0 to 99 meet both (20; 57%). Reported as samples, not defects. The shared end tally is (4 hatred, 8 wariness, 3 friendship) in every sample (default 20 of 100, 26 of 100, `high` 11 of 30, `low` 11 of 30), so the generator still tends to a common end shape.
5. **T16** is within its bound on both default samples (1.65, 0.98) but `high` (3.25), `turbulent` (4.92) and `civs4` (3.14) are above 2 per thousand, as at 0.24.0 (2.02, 2.24, 3.11).
6. `RR3`: the four worlds on seeds 0 to 99 and two on 100 to 199 where a race meets no other are all races alone on a land mass (no land path), as the lead's rule excludes.
7. The README phrase hash changes to the value in section 2.
8. **The slow witness of P3-06 20.27 items 7 and 8 moved again** (`conflict_engineer_test.cpp` and `history_balance_golden_test.cpp`, the cases "1024 turbulent seed 34 ..."): at 0.25.0 seed 34 has no colony founded in the step of a cession, so `REQUIRE(found != nullptr)` and `CHECK(found >= 1)` failed (the witness has been replaced at each of 0.21.0, 0.23.0 and 0.24.0 for the same reason). I scanned 1024 turbulent seeds 0 to 39 (`scripts/witness.py`, `wit.sh`): seeds 4, 5, 15 and 29 have one each; the cases now use **seed 4** (region_ceded e173 of region 24 in 3030, colony e171, s50) and both pass. The invariants the cases check (invariant 13, the colony's census row to the winner, the winner's region control row) hold on it. The cases keep their names except the seed number. Fix is test infrastructure only; committed with this report.

## 7. Artifacts

`~/.cache/the-unmapped/out/p5-03a-validation/`: `sweep/<set>/` (per world `.full.txt`, `.summary.txt`, `.report.txt` and `.json`, `m.jsonl`, `times.txt`; the `default` and `default2` world files are kept), `results-m502-default.txt`, `results-m502-default2.txt`, `refs-{cmp,conf,rel,ren,mon}.txt` and `refout/`, `logs/` (builds, `ctest1-*` before, `ctest2-*` after recording, `ctest-slow.log`, `wh-*`), `scripts/` (`sweep3.sh`, `all.sh`, the engineer's `measure2.py`, `agg2.py`), `phrase/` (section 2), `critic/` (`default-s0` to `default-s11` and `high-s0` to `high-s3`: `.full.txt`, `.summary.txt` and `.db`; territory-with-markers PNGs at the end year for `default-s0`, `default-s1`, `high-s0`, `high-s1`). World hash files: `~/.cache/the-unmapped/out/p503a-val/world-hashes/`.
