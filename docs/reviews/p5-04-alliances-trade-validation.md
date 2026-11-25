# P5-04 alliances, trade, mines and arms: validation (generator 0.26.0)

Validated by the validation specialist on 2026-11-25 on branch `p5/04-validation` (from `origin/p5/04-int` at e33040f). Generator 0.26.0, schema 11, history report format v7. Spec: `docs/specs/p5-04-alliances-trade.md` r3, targets in its section 15, with the lead's bounds as given in the task (the same as 15.1 except where listed in section 4). Form: `docs/reviews/p5-03a-relations-followups-validation.md`. MSVC and Windows were not available (CI).

Verdict: **pass with three defect reports and a list of target misses.** All five Linux presets are fully green after recording (44 of 44 `ctest` entries each), the slow suite on `linux-gcc-release` is green (20 of 20), the sixteen `world-hashes.sh` cases are identical on all five presets, and `expect` matches the recorded goldens on all five. The sweeps (370 worlds) had no failed `generate` or `report --history` run and no stderr output. The independent references found **two engine or report discrepancies and one stale reference check** in 160 worlds (findings 1 to 3). Of the lead's targets, **AL6 (refusals at most joins) is missed on seeds 100 to 199** (746 against 698), **dwarf participation is below 90%** on both samples (89% and 87%), **Paul's lair figure is below 65** on both (58 and 55), and the P5-02 target M4 is below 85 (77 and 81); everything else is met on both samples. The engineer's seed 0 to 99 figures of spec 15.1 are reproduced exactly (section 4).

Machine: 6 cores shared with other agents (load 3 to 7), so `ctest` times are inflated; sweeps ran three worlds at a time.

## 1. Presets

Cold builds (`cmake --build --preset <p> -j 4`), then `ctest --preset <p>`; the first run of each preset (before recording) is not listed: integration reported `linux-gcc-release` 42 of 44 with the two `-other` entries failing on the world-file hashes, which I recorded (section 2).

| Preset | Build | `ctest` after recording |
| --- | --- | --- |
| linux-gcc-release | 565 s | **44 of 44**, 1,210 s (`-j 1`) |
| linux-clang-release | 455 s | **44 of 44**, 1,083 s (`-j 1`) |
| linux-gcc-debug | 331 s | **44 of 44**, 4,771 s (`-j 1`) |
| linux-clang-debug | 286 s | **44 of 44**, 1,581 s (`-j 3`) |
| linux-clang-asan | 730 s | **44 of 44**, 737 s (`-j 3`), no sanitizer report |
| linux-gcc-release-slow | (built) | **20 of 20**, 2,241 s (`ctest -L slow -j 3`), including both witness cases the integration fixed |

`gdext-headless-check` is not built in these trees (extension off). The determinism lint (`determinism_lint.py`): 186 C++ files, 0 findings. Logs: `~/.cache/the-unmapped/out/p5-04-validation/logs/`.

## 2. Cross-preset hashes and goldens

`tools/ci/world-hashes.sh local` over the five presets: **all 16 cases identical** (detail lines, world report, both history reports, territory and markers hashes; 6 files per case with history). The case list is still sixteen; spec 17 and 18 name no new case (only the `markers.txt` values change with the mine markers), so none was added. After recording, `expect` matches 16 of 16 on gcc-release, clang-release, gcc-debug, clang-debug and linux-clang-asan.

Recorded (values from gcc-release; all five agree): `worldgen/tests/golden/p0-worlds.txt` (8), `p1-worlds.txt` (4), `p3-worlds.txt` (4); `db_determinism_test.cpp` `GOLDEN_CASES` (5) and `stages_pipeline_test.cpp` lines 496 and 509 with their comments (0.26.0, schema 11). Why they move: every world file carries the generator version and the content hash of the shipped `game/data` (the P5-04 data: alliances tuning, mine forms, report templates), so even the empty worlds change; the history worlds also change through the P5-04 rules. No other golden file was touched.

| Case | SHA-256 |
| --- | --- |
| empty-phrase | 88c4491ac1a5c737022cd3cb16e8e61d035bfb08f93aa28b96253670d8484364 |
| empty-seed-0 | bbdb975ec3baaae73a87d6e6585812d5c45734b7bc3bdc5c1904202240f98258 |
| empty-seed-0-512 | 93114c1e6ea950e3cd14a37bfb308e99463522408804e520321df203c1dffea6 |
| empty-seed-1 | 041b8768408b82a975692a9545520494a60187ef730ccf98b25d8c134fe82c78 |
| empty-seed-42 | 1cd43312432eaf9f7e96eb4eaa61676b98e227a2d9b04f696da03c172bc3cc03 |
| empty-seed-max | c2c52d3a315832f20f6c3db61af2a9c135b02993ca4d2e96e73e29a3a3378932 |
| pattern-seed-0 | b43924f667ac5fdc2faaa9123a51f9bad19f57fa2ffb6cadabcbaf551f17cffa |
| pattern-seed-42 | 96e16b74bc0c7979514a2932fe3b3908c2fe5dcf9de60b90f8cd81f1f4eaa37b |
| geo-phrase-512 | 96396decf58c143c798369f5f4cf3d19df79e56d5de76e35eff03048c980c401 |
| geo-seed-0-512 | 7396387e00c3511b92087a804f168fa178d48cb8fae2ba98352660621e52de0f |
| geo-seed-1-1024 | e7583e45bacaec1e124b78486a785939b882ad48baf1415710a4afd31ab84177 |
| geo-seed-42-512 | 889786d6a9efde6b7d9b6c8e8b6e0be1362d51e45b12cb03c9de25c77a39dc4c |
| hist-dense-1024 | 1eea7fbcfefd9270fa567c871a33f21bf196fe5a98ae49d2feb1b7cdae6d6fcb |
| hist-races-elf-orc-1024 | 654916ac73eadef4b9750198aac104e6a3f45dfee0c3387313d7e1d81d7feb79 |
| hist-span-2000-512 | 512d8b052bc61cca321415ad96a1d8eedd904433e4e2ec794cce6ddda02e5740 |
| p5-monsters-high-1024 | 58ba7d1e99bc0148c43b9b1f0054da9886b1f5184fefad32cb55c99bf8c79a0b |

Unit-test constants (no data, generator 0.26.0): `empty-phrase` 7b94c1c3ee2ee40ca5245d88089035fcaa5d4d545d2e5aeb04aedddade3d7807, `empty-seed-0` (also `stages_pipeline_test.cpp:496`) 7b1a975b6a039fc83da71e26c1e84cda4dfdd51a3a2f96f8113203830420c46b, `empty-seed-1` 1ba2d598df7099ec233c0458df10ad8bd15cc6cd786fb657f2d98dcfb0632410, `empty-seed-42` d5a265e336fe87b8cd90b404f75d8dae34c06b9b1b674ec016b1c127073b420c, `empty-seed-max` 0527608a4cabf529f2640e1b393bb42660a1297803f45418ba0d3549d91f34c8, test pattern c8488be9a5f1c250b0bebd1a64789127db5f0994395670546b5e6719c17b7e8e.

**README phrase world** (`generate --seed-phrase "The Unmapped"`, 1024, shipped data): file SHA-256 (`hash --db` and `sha256sum` agree) **`962e0e4a03645f4c211adb005829d6cfb0b7c5cdcb0391707e19e02f7abcfb5e`**. Files: `~/.cache/the-unmapped/out/p5-04-validation/phrase/{world.db,world.report.txt,summary.txt,full.txt,hash.txt}`. (The README shows the 0.25.0 hash `a5e8a8f3...`.)

## 3. Sweeps

Shipped data, gcc-release binary, 1024 unless stated: `default` seeds 0 to 99 and `default2` seeds 100 to 199; 30 each (seeds 0 to 29) for `low` and `high` (`--monster-pressure`), `calm` and `turbulent` (`--event-intensity`) and `civs4` (`--civs-per-race 4`); `d512` and `d2048` seeds 0 to 9. **370 worlds. Every `generate` and every `report --history` at both levels exited 0 with empty stderr** (no `fail.txt` in any of the nine sweeps).

Independent references on 160 worlds (default seeds 0 to 99, `high` 30, `civs4` 30): `p3_report_reference.py compare` equal at both levels in **159 of 160** (finding 2); `p3_conflict_reference.py check-world` 159 of 160 (finding 3); `p5_12_renaming_reference.py`, `p5_02_monsters_reference.py` and `p5_03_relations_reference.py check-world` **0 failures** (the stale invariant 4 of P5-03a's report did not fire); `p5_04_alliances_reference.py check-world` 159 of 160 (finding 1). Results: `refs-{cmp,conf,rel,ren,mon,all}.txt` and `refout/`.

Names: `known as '`, `{placeholder}` and doubled-space artefacts: 0 hits in the 370 full reports.

## 4. Targets (spec 15.1, with the lead's bounds)

Measured with the engineer's `measure2.py`/`agg2.py` extended (`scripts/measure3.py`, `agg4.py`: the [stats] block of the full report, the event rows, `tr2.py`). **The engineer's figures for seeds 0 to 99 are reproduced exactly** (AL1 median 14 mean 14.4, AL2 9, AL3 66%, AL5 25.8% with defence 82%, AL6 687 refused against 736 joined, AL7 50 worlds and 14.5%, 525 ended with lapsed 82, broken 245, spurned 28, betrayed 76, with a realm 94, median length 294, AL9 median 368 max 567, 1,585 allied battles of 7,622, SP2 28 worlds and 5.3%, TR1 14.5 and 10, TR2 43%, MC1 99 worlds, MC2 85 of 99, C1 23, T16 1.44, G2 83 and 208,364, T18 577 and 742, `civs4` summary 932 and 1,070, friendship 53% and median 4). Two differ only in rounding or definition: SP1 is 680 permille (81 of 119; the spec says 681) and the participation figures use the spec's denominator (the engineer's were over all worlds).

| Target | Bound | Seeds 0 to 99 | Seeds 100 to 199 | |
| --- | --- | --- | --- | --- |
| AL1 formed per world | median 6 to 15; at least one in 95 worlds | median 14 (4 to 32); 100 worlds | median 13.5 (4 to 33); 100 worlds | met |
| AL2 standing at E | median at least 3 | median 9 (min 3) | median 9 (**min 2**; 2 worlds under 3) | met |
| AL3 across races | 30% to 70% | 66.2% (950 of 1,436) | 64.8% (946 of 1,459) | met |
| AL4 elves and faeries by origin | every world with both alive together | 100 of 100 | 100 of 100 | met |
| AL5 wars with an ally; defence | 20% to 45%; at least 70% of joins | 25.8%; 82.3% | 25.0%; 79.4% | met |
| AL6 refused at most joined | `ally_refused` <= `ally_joined_war` | 687 <= 736 | **746 > 698** | **missed on 100 to 199** (finding 4) |
| AL7 betrayal | 20% to 60% of worlds; 3% to 15% of ended | 50 worlds; 14.5% (76 of 525) | 46 worlds; 13.1% (70 of 535) | met (0 to 99: within 0.5 point of the bound) |
| AL8 ended; median length | reported | 525: lapsed 82, broken 245, spurned 28, betrayed 76, with a realm 94; 294 years | 535: 80, 257, 29, 70, 99; 264.5 years | |
| AL9 bloc share; hard bound | median at most 450, max at most 700; over-800 years at most 1000 | median 368, max 567; 0 years | median 399, max 570; 0 years | met |
| AL10 allied battles | reported | 1,585 of 7,622 | 1,359 of 7,630 | |
| SP1 shares granted; owed | 500 to 850 permille; median owed at least 1 | 680 permille of 119; median 1 | 649 permille of 114; median 1 | met |
| SP2 spurned | 20% to 60% of worlds; at most 10% of ended | 28 worlds; 5.3% | 25 worlds; 5.4% | met |
| TR1 trades opened; standing | median 8 to 25; standing median at least 3 | 14.5; 10 | 15; 9 | met |
| TR2 trade-based alliances (basis trade or citing `trade_opened`) | at least 15% | 43.2% (basis 4 alone 23.1%) | 41.4% (20.7%) | met |
| MC1 worlds with a mine; mines opened | at least 90% of worlds with dwarves; median 2 to 8 | 99 of 100; median 2 | 98 of 100; median 2 | met (median at the lower bound) |
| MC2 arms sold in worlds with a mine | at least 60% | 85 of 99 (85.9%) | 84 of 98 (85.7%) | met |
| Participation (realm in reach of another) | at least 90% per race | dwarf **89%**, elf 100, faerie 100, goblin 98, human 95, orc 96 | dwarf **87%**, elf 100, faerie 100, goblin 99, human 94, orc 94 | **dwarves missed on both** (finding 5) |
| C1 wars | median 22 to 28 | 23 (mean 24.4) | 24 (23.8) | met |
| T16 repeated battles | at most 2 per thousand | 1.44 (11 of 7,622) | 1.18 (9 of 7,630) | met |
| G2 standing; population | medians at least 81 and 205,000 | 83; 208,364 | 88; 222,506 | met |
| RR turns into friendship; friendship pairs at E | at least 25%; median at least 3 | 53%; 4 | 54%; 4 | met |
| T18 summary lines | default median at most 620, none over L(C) | median 577, max 742 | median 578, max 718 | met |
| P5-02 M4: a living monster at E | at least 85 worlds (reported) | **77** (0.25.0: 82) | **81** (83) | below (finding 6) |
| Lairs on a former settlement open at E | bound 65 | **58** (0.25.0: 66) | **55** (53) | below on both (finding 6) |
| Undead risings; after 5500 | reported | 173; 77 (45%) | 187; 104 (56%) | |
| P5-03a regression | RR2a to RR5, MX, U1, W1, W2 | as 0.25.0 (RR2d 61% and 23%; RR5 top tally 18 worlds; RR1 on linked pairs 96 of 100; MX3 max 9; W2 none) | RR2d 59% and 24%; RR5 19 worlds; RR1 linked 97 of 100; MX3 max 15 | met |
| timing | `generate --seed 0 --map-size 1024`, at most 5 s over 0.25.0 | user 25.6 s, wall 12.4 s (load 5 to 7) | | met (0.25.0: user 25.7 s) |

Other sweeps (no bound applies except T18's `civs4`; the AL bounds are the 1024 default's):

| Set | AL1 med | AL2 med | AL3 | AL5 (defence) | AL6 refused / joined | AL7 worlds; of ended | AL9 max | SP1 | SP2 worlds | TR1 med | TR2 | MC1 mine worlds | MC2 | wars med | T16 per 1000 | T18 med / max | M4 | risings (after 5500) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| high | 13 | 9 | 63.8% | 27.7% (80.9%) | 160 / 230 | 14 of 30; 16.2% | 508 | 571 | 11 | 14 | 43.4% | 29 of 30 | 23 of 29 | 20.5 | **2.99** | 564.5 / 731 | 30 | 159 (75) |
| low | 12 | 9 | 64.1% | 26.5% (79.7%) | 222 / 246 | 14; 12.9% | 519 | 638 | 7 | 15.5 | 45.3% | 29 | 24 of 29 | 23 | 0.75 | 561.5 / 776 | 15 | **1** (0) |
| calm | 10.5 | 9 | 78.9% | 29.4% (82.3%) | 105 / 147 | 7; 9.2% | 498 | 666 | 4 | 12 | 51.5% | 29 | 24 of 29 | 14 | **2.19** | 527 / 611 | 23 | 31 (14) |
| turbulent | 23.5 | 11 | 40.8% | 21.4% (77.9%) | **541 / 321** | 23; 17.2% | 598 | 802 | 10 | 27 | 38.3% | 29 | 23 of 29 | 39 | **4.49** | 644.5 / **850** | 23 | 65 (25) |
| civs4 | 61 | 24 | 46.0% | 42.8% (77.3%) | 1,536 / 1,636 | 29 of 30; 6.9% | 633 | 656 | 26 | 55.5 | 40.6% | 30 | 29 of 30 | 88 | 0.83 | **932.5 / 1,070** | 19 | 81 (21) |
| d512 | 8.5 | 6 | 73.4% | 36.4% (78.9%) | 59 / 76 | 5; 17.1% | 766 | 666 | 4 | 11.5 | 40.4% | 9 of 10 | 8 of 9 | 16.5 | **5.75** (3 of 522) | 495.5 / 541 | 9 | 14 (4) |
| d2048 | 55 | 28.5 | 36.4% | 24.5% (80.5%) | **291 / 241** | 9; 12.3% | 246 | 647 | 5 | 63 | 33.0% | 10 of 10 | 9 of 10 | 85.5 | 1.65 | 1,009 / 1,175 | 9 | 4 (1) |

`civs4` meets its T18 bounds (median 932.5 at most about 1,050; max 1,070). The AL bounds are for the 1024 default: `civs4` has 61 alliances a world and betrayal in 29 of 30 worlds, as expected with 24 realms; the `d512` bloc maximum is 766 permille (the hard bound, at most 1,000 years above 800, holds in every world of every sweep: 0 years).

Reports without bound: arms buyers at E median 3 (default), 2 (default2); undead risings under `low` 1 in 30 worlds; at 2048 4 risings in 4 of 10 worlds (0.25.0: none); `generate` seed 0 as above; alliance end reasons and lengths in the AL8 row.

## 5. Findings

Numbered for the lead. Defect reports (1 to 3) first, then target misses and oddities (4 to 11). Nothing was tuned or fixed in stage logic; the only edits on this branch are the recorded goldens and two test constants' comments.

**1. Spoils granted or withheld on a war that a third realm ended by conquest (engine defect, 1 of 160 reference worlds).**
- Version: generator 0.26.0, schema 11. Seed 6, `--civs-per-race 4`, 1024.
- Reproduce: `worldgen-cli generate --seed 6 --map-size 1024 --civs-per-race 4 --out s6.db`, then `python3 -B worldgen/tests/reference/p5_04_alliances_reference.py check-world s6.db --data game/data` (the world is also in `~/.cache/the-unmapped/out/p5-04-validation/defects/civs4-s6.db`).
- Expected (spec 13 item 11): every `spoils_shared` or `spoils_withheld` follows "a `peace`, `war_won` or `civ_conquered` of the war its `ally_joined_war` cites, whose winner is its `principal` and whose loser is its `enemy`".
- Actual: `FAIL invariant 11: spoils e1955: end e1954 of another winner or a later year`. Year 5050: e1952 and e1953 (the Zatzork Mob, c8, takes Kuzzsneet Hole) and **e1954 `civ_conquered`: the Warrens of Skitskrot (c5) fell to the Zatzork Mob (c8)**; then e1955 `spoils_shared` with principal c17 (the Lievie Court), ally c3, enemy c5, causes e1920 and e1954. The principal c17 fought c5 (allied battles e1945, e1947) but is not the conqueror; its war was ended by another realm's conquest.
- Root cause (narrowed, not changed): `AlliancesSubsystem::spoils` in `worldgen/src/history/alliances.cpp` (the `cited` test, line about 1070) accepts any end event of kind `peace`, `war_won` or `civ_conquered` and tests `s.winner == j.partner`, but never that the end event's own winner (for `civ_conquered`, its `conqueror`) is the principal. `conflict_.war_spoils(war).winner` is c17 while the cited end event belongs to c8. One world in 160 (0 in 100 default worlds; 30 `high`, 30 `civs4` give 1), so rare.

**2. The `[alliances]` line's spoils part counts a withheld share after the alliance's breaking event (report vs reference, ambiguity in spec 10.2).**
- Seed 27, `--civs-per-race 4`, 1024. Reproduce: `worldgen-cli generate --seed 27 --map-size 1024 --civs-per-race 4 --out s27.db`, `report --history --level full` and `--level summary` to files, then `python3 -B worldgen/tests/reference/p3_report_reference.py compare s27.db s27.summary.txt s27.full.txt` (full line 6159; world and report in `defects/`).
- C++: `c1 ... and c3 ...: 4782-5682, against a common enemy against c8, e1763; renewed 6; joined 7, refused 2; spoils 0 granted, 1 withheld; broken e2745`. Reference: the same line without the spoils part.
- Cause: e2752 `spoils_withheld` (principal c1, ally c3) is dated 5682, the year of the alliance's end, but follows its ending event e2745 `alliance_broken` (the same year) by id; the war it settles (e2749) ended after the break. The C++ text counts the pair's kinds 74 and 75 by year inside [from, to] inclusive; the reference counts those before the ending event. Spec 10.2 says "the pair's kinds 74 and 75" and does not say which. Not an engine defect; the lead decides which reading the spec states (the engine's shares are legal: 13 item 11 asks for no standing alliance, and the check passes).

**3. `p3_conflict_reference.py check-world` r16 fails on `high` seed 25 (probably a stale reference check, not debugged to the line).**
- Reproduce: `worldgen-cli generate --seed 25 --map-size 1024 --monster-pressure high --out s25.db`, then `python3 -B worldgen/tests/reference/p3_conflict_reference.py check-world s25.db --data game/data`: `r16 (16) c7 claims against its quarrel neighbour c2 in phase 20, their first phase of contact, or the claim pass refuses it`.
- The same seed passed at 0.25.0 (P5-03a validation, 0 failures). Narrowing: c7 (orcs, founded e283 in 5584) has no claim against c2 at all; its first claim is e452 in 5819 against c8. e451 (5817) is the alliance of c2 and c7 (basis friendship), and since r3 every `alliance_formed` drops the pair's strain and claims both ways (spec 6.3, 7.2 `forgive`); the reference's quarrel log does not know that seam, so it expects a claim or a recorded refusal. Likely the reference needs the same stale-check treatment as P5-03a's invariant 4; the lead may send it to the test writer.

**4. AL6 refusals exceed joins on seeds 100 to 199 (746 refused, 698 joined; 687 and 736 on 0 to 99).** Also in `turbulent` (541 against 321) and `d2048` (291 against 241); `high`, `low`, `calm`, `civs4` and `d512` meet it. Reported with the numbers; no tuning.

**5. Dwarf participation is below 90% on both samples (89% and 87%; `high`, `low`, `calm` 83%, `d512` 80%, `turbulent` 87%).** The other races reach 94% to 100%. Denominator: worlds where the race has a realm sharing a land component with another race's realm (all worlds for dwarves, 100 of 100); a dwarf realm is a participant when it appears in any alliance, trade or arms event (kinds 63 to 76 and 78). The other bounds on dwarves (MC1, MC2) are met.

**6. P5-02 M4 and Paul's lair figure fall again.** A living monster at E in 77 worlds (seeds 0 to 99; 0.25.0: 82) and 81 (83); a lair on a former settlement open at E in 58 and 55 worlds (66 and 53 at 0.25.0, bound 65). Both samples together: 113 of 200 (56.5%) against 119 (59.5%) at 0.25.0. Not traced; P5-04 adds no monster rule, so a changed war and ally pattern (fewer and later raids on the same realms) is the likely path, not checked.

**7. Undead.** Risings: `low` 1 in 30 worlds (as 0.25.0), `d2048` 4 in 4 of 10 worlds (0.25.0: 0), default 173 and 187.

**8. T16 above 2 per thousand outside the default sample:** `high` 2.99, `calm` 2.19, `turbulent` 4.49, `d512` 5.75 (3 of 522 battles); default 1.44 and 1.18, `civs4` 0.83, `d2048` 1.65, `low` 0.75. Allied battles are counted (spec 15.1).

**9. T18:** `turbulent` has a world at 850 summary lines (L(C) holds; the lead's bound of 800 applies with C at most 16 and none exceeds it); `civs4` median 932.5, max 1,070; `d2048` max 1,175.

**10. Small figures at the edge:** MC1 median is 2 on both samples, the lowest value of its bound; AL7 is 14.5% of ended alliances on seeds 0 to 99 (bound 15%); AL1's median bound is met but 43 and 40 worlds lie outside 6 to 15 (spec bounds the median); AL2 has two worlds with 2 standing at E (seeds 100 to 199). MC1's denominator here is worlds with a dwarven race present (100 of 100); the spec's finer denominator (a dwarven realm holding a region with ore reach and a settlement of the minimum size) was not computed.

**11. Timing.** `generate --seed 0 --map-size 1024` (gcc-release, load 5 to 7): user 25.6 s, wall 12.4 s (two runs). The spec's 0.25.0 figures are user 25.7 s and wall 15.5 to 18.9 s on a busier machine: no slowdown. Reports under 1 s.

## 6. Artifacts

`~/.cache/the-unmapped/out/p5-04-validation/`: `sweep/<set>/` (per world `.full.txt`, `.summary.txt`, `.report.txt`, `.json`, `m.jsonl`, `times.txt`), `res-<set>.txt` (the aggregates), `refs-*.txt` and `refout/`, `logs/` (builds, `ctest2-*`, `ctest-slow.log`, `wh-local.log`, `expect-*`), `scripts/`, `phrase/` (section 2), `defects/` (the three worlds of the findings with their full reports), `critic/` (`default-s0` to `default-s11` and `high-s0` to `high-s3`: `.db`, `.full.txt`, `.summary.txt`, `.report.txt`; `<name>.territory-end.markers.png` at the end year for `default-s0`, `default-s1`, `high-s0`, `high-s1`; mines are drawn by `--markers`). World hash files: `~/.cache/the-unmapped/out/p504-val/world-hashes/`.
