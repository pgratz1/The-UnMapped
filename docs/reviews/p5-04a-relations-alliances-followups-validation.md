# P5-04a relations and alliances follow-ups: validation (generator 0.28.0)

Validated on 2026-11-26 on branch `p5/04a-validation` at 09eb744 (engine, calibrated data, tests, audit). Generator 0.28.0, schema 12 (unchanged), history report v9, 98 streams. Spec: `docs/specs/p5-04a-relations-alliances-followups.md` r2; calibration: `docs/reviews/p5-04a-calibration.md`.

Verdict: **pass.** All five Linux presets are green (45 of 45 each), the slow suite on `linux-gcc-release` is green (21 of 21), the lint has no finding, the sixteen world-hash cases are identical across the five presets, and all 370 worlds of the sweeps generated and reported with exit 0 and empty stderr. No stage-logic defect was found. Two targets are missed on one sample each (SP4 on seeds 100 to 199, RR6 on seeds 100 to 199, both already known from calibration); Paul's two monster bounds hold. The sweep numbers on seeds 0 to 99, 100 to 199 and the 30-seed sets reproduce the calibration's final numbers exactly. Engine and data were not touched. The only code changes are the world-file hash goldens (section 2).

Machine: 6 cores shared with other agents (load 8 to 9 during the builds), builds two at a time with `-j 3`, `ctest` three at a time, sweeps three worlds at a time. The task text said 470 worlds; the sweeps listed add to 370 (200 default, 5 x 30, 2 x 10).

## 1. Presets and suites

| Preset | Build | `ctest` |
| --- | --- | --- |
| linux-gcc-release | 1,448 s | **45 of 45** (555 s) |
| linux-clang-release | 1,151 s | **45 of 45** (735 s) |
| linux-gcc-debug | 1,034 s | **45 of 45** (2,197 s) |
| linux-clang-debug | 939 s | **45 of 45** (2,009 s) |
| linux-clang-asan | 1,335 s | **45 of 45** (889 s), no sanitizer report |
| linux-gcc-release `-L slow -j 3` | built | **21 of 21** (2,560 s) |

Determinism lint: self-test 22 snippet files, 0 failures; scan 189 C++ files, 13 CMake files, 0 findings. `gdext-headless-check` not built (extension off). The ctest runs were made after the goldens of section 2 were recorded and the two test files rebuilt. Logs: `~/.cache/the-unmapped/out/p5-04a-validation/logs/`.

## 2. Hashes and goldens

`tools/ci/world-hashes.sh local` over the five presets: **all 16 cases identical** (detail lines, world report, both history reports, territory hashes and markers hashes where the case has history). Afterwards `world-hashes.sh expect linux-gcc-release` matches the re-recorded files. Hashes recorded (all five presets agree):

| Case | SHA-256 |
| --- | --- |
| empty-phrase | `2da139ead4928fdc8ff8400fa2a22e5a25f6114feaf5da64bbb174dd4201fd17` |
| empty-seed-0 | `45ebeacf599bc985ffaeea86084a6217aad2c2b84b2da6c530303c2a33f71001` |
| empty-seed-0-512 | `384aca784f1de28343d148a8b57a91e19d98336f0b1c1535f24dc785cb328154` |
| empty-seed-1 | `1d27071b3526279b47d235f1ee80603a7cf50af3980630830b792c552960f4cb` |
| empty-seed-42 | `cb151bae9ccd38ca7d9658e0ad650513cee5f433244f726ea5f787b939c9b709` |
| empty-seed-max | `a59eed40ec18b5f466038e24f802883a49d4404a9fa07e1c91f804ee201aa374` |
| pattern-seed-0 | `113512e0fbf312468d68a86404e80afa630f11948282453bddd9a8b6b17d6f62` |
| pattern-seed-42 | `ab11dc68dbff8881ee8366e11c526df90e1269c9a68a3330ea7a047f7a3c6ed4` |
| geo-phrase-512 | `5306fc91a17f97c2d9b4fe5e1dd2c79c9fcc7977bd42f28ad48ffa3bd2f8f8a2` |
| geo-seed-0-512 | `3109ad16118876261f98930d799ad654637e8c96d7af5bc52e0958b24b396765` |
| geo-seed-1-1024 | `61768915a4b5bf306bffd75fdad13855a40d0bfcb9c9173827f00ea83f4dcdff` |
| geo-seed-42-512 | `9e2f22ee0698af2492d8fa8070a18587ef6698dd405f2328e0354350664624ad` |
| hist-dense-1024 | `2400fb3ffbfa8225e5cc295bae49bcc80cadbfd9ed6232d08e6725da560e8c7e` |
| hist-races-elf-orc-1024 | `8d7687078727ce7d9871ba3f63634a4c095fb6e81d4cb09477e9174feaf21ef5` |
| hist-span-2000-512 | `bc8f6214bab53f01a90a4c3c6cc554749dda18ecd33c615766ffbf6dfc353afa` |
| p5-monsters-high-1024 | `8374c85d07805bb8ea7cce15b92577d68336efaedca348eef14e1d12e5be1a4f` |

Files re-recorded for 0.28.0: `worldgen/tests/golden/p0-worlds.txt` (8), `p1-worlds.txt` (4), `p3-worlds.txt` (4), all in byte order (`LC_ALL=C sort`); `GOLDEN_CASES` of `db_determinism_test.cpp` (worlds with no data: phrase `453c912d...`, seed 0 `c647477b...`, seed 1 `4c86e054...`, seed 42 `e3dfc2ce...`, seed max `ee042e40...`); `stages_pipeline_test.cpp` (seed 0 `c647477b...`, test pattern `0b369bc3...`). Every world changes for the stated reasons: the generator version in `meta`, the content hash of the shipped `game/data` (calibrated values and new fields), and, for worlds with a history, the new histories. Other tests that name a world hash passed without change. Nothing else was edited.

Phrase world "The Unmapped" (default size 1024): file hash `4766b0254dfa896b1efae22c8eedef4732976822ae525e34ef127297c5afa811` (the README hash), in `phrase/`.

## 3. Sweeps

Release build (gcc), `generate --map-size N` (not a cached-geography tool) and `report --history` at both levels: default seeds 0 to 99 (A) and 100 to 199 (B), 30 seeds each of `--monster-pressure low` and `high`, `--event-intensity calm` and `turbulent`, `--civs-per-race 4`, and 10 each at 512 and 2048: **370 worlds, 0 generate or report failures, no stderr** (every `fail.txt` is absent). Reference checks on every default world (A and B, 200 runs of each): `p5_04` alliances, `p5_03` relations, `p5_12` renaming, `p5_02` monsters and `p5_06` figures `check-world` at 0.28.0: 1,000 runs, **0 failures**; the report comparison against `p3_report_reference.py` on seeds 0 to 99: **equal at both levels on 100 of 100**. `p3_conflict_reference.py check-world` fails on all 200 worlds with `generator_version: got '0.28.0', want '0.27.0'`: a stale checker (section 6, D1); its job (replay and conflict invariants) was done by `cmp_world028.py` of the integration on the 16 critic worlds, **table-for-table equal on 16 of 16** (default 0 to 11, high 0 to 3, `critic/*/world.db.cmp.txt`). The 512 worlds (seeds 0 to 9, regenerated) pass the figures reference.

Timing: `generate --seed 0 --map-size 1024` **24.9 s and 25.6 s user** (wall 12.0 s; 0.27.0: 25.6 s; the machine was loaded by a `ctest` run). Median wall per world with three jobs and load: 16.6 s at 1024 (with the reference checks), 5.0 s at 512, 75 s at 2048.

## 4. Targets

A = default seeds 0 to 99, B = 100 to 199. Miss marked **MISS**.

| Target | Bound | A | B |
| --- | --- | --- | --- |
| M4a living monster at E | >= 85 worlds | 94 | 93 |
| M4b lair on a former settlement | average of A and B >= 65, neither < 60 | 62 | 68 (average 65) |
| AL6 refusals against joins | refused <= joined | 782 <= 917 | 698 <= 945 |
| AL5 wars with an ally; defence share | 20-45%; >= 70% | 29.7%; 84.4% | 31.2%; 84.9% |
| AL7 betrayals, share of ended (restated) | <= 18%; 20-60 worlds | 16.8%; 53 worlds | 16.3%; 51 |
| SP1 shares granted | 500-850 per mille | 631 | 584 |
| SP2 worlds with an alliance ended by spoils | 10-35% of worlds | 29 | 30 |
| SP4 large shares of grievous withholdings | 30-50% | 30.4% | **27.3% MISS** |
| SP4 grievous withholdings that ended their alliance | >= 40% | 73.9% | 74.5% |
| AL1 alliances formed / standing, median | report (14 / 9) | 14 / 9.5 | 13.5 / 9 |
| Trades opened, median (mean) | report | 17 (18.3) | 16 (18.0) |
| Dwarves in an alliance or trade | >= 85% | 90% | 91% |
| RR2 turns into friendship | >= 25% | 57.4% | 58.3% |
| Most turns in a world | <= 20 | 12 | 10 |
| Expulsions, median (max) | <= 3 | 2 (12) | 1 (11) |
| RR6 worlds with a pair of 4 or more turns | <= 5 | 5 | **9 MISS** |
| FG1 figures median / most in a world | 60-120 / <= 250 | 79 / 175 | 76 / 208 |
| FG5 monsters slain by heroes | >= 25% | 30.8% | 33.8% |
| FG8 alive through a quiet step (0.27.0 definition; the whole-phase reading: 6.1%, 5.9%) | >= 15% | 20.8% | 21.1% |
| FX1 larger skill wins | 55-70% | 57.5% | 56.7% |
| FX3 grasping betrayal ratio | >= 1.25 | 1.47 | 1.57 |
| FG11 worlds with one epithet over max(2, figures div 10) | 0 | 0 | 0 |
| TW1 towers ended | >= 5% | 8.7% | 7.0% |
| TW2 two standing towers of a realm within 30 cells | none | 0 | 0 |
| C1 wars median | 22-28 | 25 | 23 |
| T16 repeated battles per thousand | <= 2 | 1.26 | 1.39 |
| G2 standing settlements / population, median | >= 81 / >= 205,000 | 82 / 214,482 | 86 / 210,881 |
| T18 summary median / max (lines) | <= 670 | 625 / 784 | 627 / 801 |
| UD3 undead risen in the last 500 years | <= 50% | 48.6% | 42.0% |

Per-race regions held at the end (median / mean; 0.26.0: goblins 12, humans 3, elves 2, orcs 2, faeries 2, dwarves 2):

| Race | A median / mean | B median / mean |
| --- | --- | --- |
| goblins | 12 / 12.1 | 12 / 12.6 |
| humans | 3 / 4.9 | 4 / 5.6 |
| elves | 3 / 3.7 | 2 / 3.5 |
| orcs | 2 / 3.3 | 2 / 3.4 |
| faeries | 2 / 2.9 | 2 / 2.7 |
| dwarves | 2 / 2.2 | 2 / 2.4 |

The elves hold one region more in A than at 0.26.0 (median 3, mean 3.7 against 3.3) and the humans slightly more; nothing else moved.

Other sets (30 worlds; bounds are those of the defaults and apply only as guards):

| Set | Results |
| --- | --- |
| `low` | M4a 25 of 30; AL6 190 <= 290; AL5 31.4%; AL7 16.1%; wars median 22.5; **T16 2.35**; UD3 50.0% (21 worlds with a rising); summary median 614.5 |
| `high` | M4a 30 of 30; AL6 220 <= 235; AL7 17.7%; SP4 large share 0 of 13 (no grievous large share at all); wars 22; slain median 9, overruns 8; standing 78, population 200,828 (below the default bounds); summary median 641 (max 784) |
| `calm` | AL6 81 <= 175; wars 14; figures median 52; AL7 9.8%; T16 0.00 |
| `turbulent` | **AL6 519 > 485**; wars 35; **T16 2.38**; AL7 19.3%; figures median 121, max 265; summary median 667 (max 948); TW1 15.2% |
| `civs4` | AL6 1,435 <= 1,643; **AL5 46.2%**; wars 92; figures median 261 (max 347); summary median **915**, max 1,157 (bound <= 1,050 holds on the median); AL7 5.6% |
| 512 (10) | AL6 60 <= 75; T16 20.25 per thousand (small map; 0.27.0: 22.7); standing 35; AL7 24.3% of 37 ended |
| 2048 (10) | **AL6 250 > 216**; AL7 13.3%; summary median 951 (max 1,273); figures median 235 (max 419); a rising in 10 of 10 worlds |

## 5. Rare paths (audit A3) in the 370 worlds

| Path | Total | Worlds |
| --- | --- | --- |
| Spurning at once (same year as the share) | 75 | 59 |
| Spurnings in all | 214 | 126 |
| `ally_refused` motive 7 | 25 | 22 |
| Tower ending with code 4 (founder lost the land) | 126 | 81 |
| Tower ending with code 5 (founder's realm fell) | 120 | 76 |
| Refusal motives 0 / 1 / 2 / 3 | 513 / 895 / 130 / 1,409 | 191 / 236 / 78 / 260 |
| Refusal motives 4 / 5 / 6 / 7 | 162 / 901 / 200 / 25 | 99 / 214 / 86 / 22 |
| Betrayal motives 0 / 1 / 2 / 3 | 139 / 4 / 101 / 127 | 108 / 4 / 81 / 98 |

Every refusal and betrayal motive occurs; the rarest are betrayal motive 1 (4 times in 4 worlds) and refusal motive 7 (25 in 22 worlds).

## 6. Findings

1. **SP4 on B: large shares 27.3% against 30% to 50%** (A 30.4%; `high` 0 of 13, `civs4` 22.9%, `calm` 33.3%). Known from calibration (the lever `spoils_large_share`; ended-by-it 75% holds). The lead's ruling of 2026-11-26 left it to validation; a restatement to at least 25% would cover it.
2. **RR6 on B: 9 worlds against 5** (A 5, `calm` 7). Known from calibration (cooldown 800).
3. **Sets outside the default bounds.** Turbulent and 2048 have refusals above joins (519 against 485; 250 against 216), `civs4` has 46.2% of wars with an ally (bound 45), `low` and `turbulent` have T16 above 2 (2.35, 2.38); 512 has T16 20.25 as at 0.27.0. No bound is set for these sets; reported for the lead.
4. **TW2 at 512 and 2048.** My TW2 measure uses a fixed 30 cells; the spec's D is `tower_spacing_permille` x width div 1000: 15 at 512 and 61 at 2048. It shows 18 close pairs at 512 (D = 15) and 0 at 2048 (under-strict), neither meaningful. The figures reference passes on the ten 512 worlds and on every default world.
5. **Betrayal motive 1** occurs 4 times in 370 worlds; the world critic may want to see one.

## 7. Defect reports

No stage-logic defect.

**D1 (checker, open, not fixed): `p3_conflict_reference.py check-world` still reads the 0.27.0 data.** Reproduce: `worldgen-cli generate --seed 0 --map-size 1024 --data game/data --out s0.db --force`, then `python3 -B worldgen/tests/reference/p3_conflict_reference.py check-world s0.db --data game/data` prints `FAIL generator_version: got '0.28.0', want '0.27.0'` and exits 1 (line 91: `GENERATOR_VERSION = "0.27.0"`; the integration's `cmp_world028.py` calls `use_version("0.28.0")` before the replay, `check-world` does not). Expected: the check accepts a 0.28.0 world as the other references do (`--version 0.28.0`). It fails on all 200 default worlds; the replay of `cmp_world028.py` stands in for it. I did not edit the reference.

## 8. Not measured

- `gdext-headless-check` (extension off); Windows and MSVC (no machine; the hashes were compared across five Linux presets only, so cross-platform equality is still open for CI).
- `p3_report_reference.py compare` on seeds 100 to 199 and the other sets (done on 0 to 99); the replay on more than the 16 critic worlds.
- Full-replay check of worlds at 512 and 2048.
- No viewer or map images (no world change in geography).

## 9. Artifacts

`~/.cache/the-unmapped/out/p5-04a-validation/`: `logs/` (builds, `ctest-*.log`, `ctest-slow.log`, `hashes.log`, `lint.log`, `timing.txt`), `sweep/<tag>/` (`table.txt`, `m6.jsonl`, `m7.jsonl`, `m8.jsonl`, `refs.txt`, `times.txt`), `scripts/` (`build.sh`, `ctest.sh`, `sweep.sh`, `sweeps.sh`, `materials.sh`, `final.py`, `m6.py` to `m8.py`, `agg7.py`), `phrase/` (`world.db`, `world.report.txt`, `summary.txt`, `full.txt`, `hash.txt`), `critic/` (default seeds 0 to 11 and high seeds 0 to 3, each with `world.db`, `world.report.txt`, `summary.txt`, `full.txt`, `world.db.cmp.txt`).
