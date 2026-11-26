# P5-06 figures: validation (generator 0.27.0)

Validated on 2026-11-25 on branch `p5/06-validation` (from `origin/p5/06-int` at cfcc2c0, with `origin/p5/06-slow` 640f7e3 merged by fast-forward). Generator 0.27.0, schema 12, history report v8. Spec: `docs/specs/p5-06-figures.md` r4 (sections 15 to 17, 25).

Verdict: **pass, with a list of target misses and two checker defects (test infrastructure, one fixed here).** All five Linux presets are green (45 of 45 each), the slow suite on `linux-gcc-release` is green (21 of 21), the sixteen world-hash cases are identical across the five presets, and every world of every sweep generated and reported with exit 0 and empty stderr. No stage logic defect was found. Eleven targets are missed (section 4); the engine was not touched and nothing was tuned.

Machine: 6 cores shared with another agent (load 3 to 8); sweeps ran three worlds at a time, `ctest` four at a time.

## 1. Presets

| Preset | Build | `ctest` |
| --- | --- | --- |
| linux-gcc-release | 808 s | **45 of 45** (330 s); first run 44 of 45, see 2 |
| linux-clang-release | 847 s | **45 of 45** (316 s) |
| linux-gcc-debug | 449 s | **45 of 45** (1,388 s) |
| linux-clang-debug | 341 s | **45 of 45** (1,466 s) |
| linux-clang-asan | 829 s | **45 of 45** (631 s), no sanitizer report |
| linux-gcc-release `-L slow -j 3` | (built) | **21 of 21** (2,609 s), including `-slow-monsters`, `-slow-relations`, `-slow-engine`, `-slow-history-report` and `-slow-figures` |

Determinism lint: 189 C++ files, 13 CMake files, 0 findings. `gdext-headless-check` is not built (extension off). Logs: `~/.cache/the-unmapped/out/p5-06-validation/logs/`.

## 2. Hashes and goldens

`tools/ci/world-hashes.sh local` over the five presets: **all 16 cases identical** (detail lines, world report, both history reports, territory hashes; 6 files per case with history). The spec names no new case; the script keeps sixteen. Recorded (gcc-release values, all five agree):

| Case | SHA-256 |
| --- | --- |
| empty-phrase | `bee545f257dd525eb2466ab476e9c6a8e33b6f6ba3de43692c31aaf1f4a1a0c3` |
| empty-seed-0 | `b045d5125ca333f2c12789f72b93da63a2e7d11f51943d2e908dfa7d54c83ac5` |
| empty-seed-0-512 | `8dd682014b575866f17e307a89a8afe5e956e2e9618be89be36c02307f480332` |
| empty-seed-1 | `f864a879ec9958d0b624c8e5dc60ad23826fee299314cf92e52a63c3fbf5b4ae` |
| empty-seed-42 | `4ec5ae6df03ec5524c8bebacc3bfad868a99768a9f95e507cdcfb0c9d2f15d41` |
| empty-seed-max | `317f5fe55e53e89c5baa50ea8c1728793152c4f60fff1af32766ba5eaa70ee88` |
| pattern-seed-0 | `a3cf48e244b0866bf1bbb7d90098c1bd69b752c737e24b25226e8f49563ed251` |
| pattern-seed-42 | `88701539463cecb65c564f1cf1ca527160293b21a8f63efd6f23723bc5f1b5ae` |
| geo-phrase-512 | `7652c59f548bde363bb4012c1ee2f7d15d1ed37f061efc0ee84c48407c6fa63d` |
| geo-seed-0-512 | `0d93658dcbc057ae8c99850f1632f5e27e8584d7c9d9f6e4320d996f6569f319` |
| geo-seed-1-1024 | `9c6fb82fe5d86ed5ce45e93611374117ce217e4f39155f711bb5e10a3de19991` |
| geo-seed-42-512 | `9dd4e5d6a37bc5d3c684048d0bf6a76846cfcec61cafd7584635185a70093bd6` |
| hist-dense-1024 | `5111eb44d7839925acbd4223624359ba15dc88cd7accb8718c3d3a5d1d933759` |
| hist-races-elf-orc-1024 | `417d3baf704e1e00b02546e8ef9800b8718142834d5cd4e616397e3742f04a60` |
| hist-span-2000-512 | `5feab7f675fa865bc959d9a355320257f3a0c5d94e39a5c45e09a1553afdc1f4` |
| p5-monsters-high-1024 | `5bab17934fb0b0b306dc99fb95ce40d88e968ec0f7325fd1cfa1015382e0e383` |

Files: `worldgen/tests/golden/p0-worlds.txt` (8), `p1-worlds.txt` (4), `p3-worlds.txt` (4, byte order); `GOLDEN_CASES` of `db_determinism_test.cpp` (empty worlds with no data: phrase `91ed3274...`, seed 0 `a0843b75...`, seed 1 `a54ec95c...`, seed 42 `0341b074...`, seed max `5f15a4b0...`); `stages_pipeline_test.cpp` (seed 0 `a0843b75...`, test pattern `cfb7305f...`). `cli_hash_test` holds no world hash (its only hash is the SHA-256 of "abc"). Every world changes for the stated reasons (version, schema 12, `content_hash`, catalogue rows, histories from the first reign; the empty world gains two pages). The first `ctest` run of gcc-release failed `worldgen-cli-tests-other` only because I had written `p0-worlds.txt` in locale order; the file is now in byte order (second commit).

Phrase world "The Unmapped" (default size 1024): file hash `b21d57fb9012147677cbae825ccc8a7c102b5f6971f3c567b763f27c8cab4f4e` (the README hash), in `phrase/` with `world.db`, `world.report.txt`, `summary.txt`, `full.txt`, `hash.txt`.

## 3. Sweeps

Release build; 1024 unless stated. Default 0 to 99 and 100 to 199, 30 each of `low`, `high`, `calm`, `turbulent`, `civs4` (seeds 0 to 29), 10 each at 512 and 2048 (seeds 0 to 9): 470 worlds. `generate` and `report --history` at both levels: **0 failures, no stderr**. Reference checks (`p3_report_reference compare`, `check-world` of conflict, renaming, monsters, relations, alliances, figures) on default 0 to 99, `high` and `civs4`: 910 runs, 11 nonzero, all explained as checker defects (D1, D2); the report comparison and the figures reference passed on every world. The figures of the engineer's seeds 0 to 99 (`p5-06-impl/sweep/R1`) are reproduced exactly on every target line (the two later engine fixes moved no number). Generation: seed 0 at 1024 takes **25.6 s user, 9.3 s wall** (load 5, two runs: 25.59 and 25.63 user), equal to 0.26.0's 25.6 s user (target at most +2 s). Median wall per world with three jobs: 12 to 16 s (1024), 4 s (512), 62 s (2048).

## 4. Targets

Sets A = default seeds 0 to 99, B = 100 to 199. Miss marked **MISS**.

| Target | Bound | A | B |
| --- | --- | --- | --- |
| FG1 figures median / max | 60 to 120 / <= 200 | 77 / **226 MISS** | 79 / **208 MISS** |
| FG1 rulers' share | >= 50% | 65.5% (no world below) | 66.6% |
| FG2 rulers per realm median | 3 to 8 | 3 | 3 |
| FG2 reigns per realm, median (max) | elves ~40, goblins ~260 | elf 41, goblin 146 (290), human 42, dwarf 40, orc 25, faerie 19 | elf 41, goblin 122 (286) |
| FG3 sides with a general / generals median / `battle_led` share | 25-50 / 6-20 / 20-50% | 28.1% / 16 / 44.8% | 27.2% / 15 / 44.4% |
| FG4 generals fallen | 5-25% | 18.6% | 17.4% |
| FG5 heroes median / hero where raided / slain by heroes | 2-8 / >= 80% / 30-70% | 2 / 90% / **27.9% MISS** | 2 / 88% / **27.8% MISS** |
| FG6 disputes per crisis / exiles median | 10-40% / 1-4 | 35.9% / 3 | 36.3% / 3.5 |
| FG7 casters / towers worlds / books / blights median (max) | 2-6 / >= 80% / 3-10 / <=1 (<=3) | 6 / 99% / 8 / 0 (2) | 6 / 100% / 8 / 0 (2) |
| FG8 elf-faerie age over goblin-orc / alive through a quiet step | >= 4 / >= 30% | 8.0 / **20.3% MISS** | 7.5 / **20.2% MISS** |
| FG9 longest display name | <= 81 bytes | 27 | 27 |
| FG10 regnal share, humans and dwarves | 10-30% | 14.0% (human 17%, dwarven 10%; others 0) | 15.9% |
| FG11 worlds over the epithet bound / with an epithet | 0 / 15-40% | 0 / 17.5% | **1 (`the Old`)** / 16.9% |
| FX1 larger skill wins | 55-70% | 55.7% | **54.3% MISS** |
| FX2 warlike over peaceable (pooled in race) | >= 1.3 | 1.41 | 1.44 |
| FX3 shares granted, just minus grasping | >= 20 points | 52 (91.7% vs 40.0%) | 56 (95.7% vs 39.5%) |
| FX3 betrayals by grasping over their reign share | >= 1.5 | 1.53 | **1.27 MISS** |
| FX5 split after short / long reign; civilizations median | short > long; within 1 of 0.26.0 | 15.5% / 15.1%; 11 | 16.5% / 15.2%; 11 |
| C1 wars median | 22-28 | 25 | 25 |
| T16 repeated battles per thousand | <= 2 | 1.19 | 1.68 |
| G2 standing settlements / population medians | >= 81 / >= 205,000 | 85.5 / 218,172 | 88 / 211,371 |
| AL1 alliances formed / standing / wars with an ally | 6-15 / >= 3 / 20-45% | 14.5 / 9.5 / 25.9% | 15 / 9 / 27.5% |
| Turns into friendship | >= 25% | 54% (240 of 444) | 55% |
| M4a a living monster at E | >= 85 worlds | 92 | 93 |
| M4b a lair on a former settlement open at E | >= 65 worlds | **63 MISS** | 68 |
| Monsters slain median / overruns median / towns share | <= 4 / 2-4 / <= 10% | 4 / 3 / 4.4% | 4 / 3 / 6.5% |
| UD2 worlds with a rising / median | 70-95 / <= 3 | 77 / 2 | 85 / 2 |
| UD3 risings in the last 500 years | <= 50% | 47.5% | 45.6% |
| RU1 `faded` rows and `ruin_faded` | 0 | 0 and 0 (ruins at E median 20) | 0 and 0 (22) |
| CB1 major calamities median / calamity share of summary | 3-8 / <= 10% | 7 / 4.3% | 7 / 4.3% |
| T18 summary median / max / worlds over L(C) | <= 650 / - / 0 | 613.5 / 902 / 0 | 603.5 / 785 / 0 |
| P a named figure for every race with a realm | >= 95% | 100% for all six races | 100% |

Other sweeps (30 worlds unless stated):

| Sweep | Results |
| --- | --- |
| `low` | UD1: a rising in 22 of 30 (bound >= 9). Heroes median 1, a hero where raided 55%; slain by heroes 22.7%; FX1 51.9%; FX2 1.29; T16 3.07 per thousand; living monster at E 27; overruns median 1 |
| `high` | UD2 median 5 (bound <= 8). Heroes median 6, hero where raided 100%, slain by heroes 33.5%; FG1 max 201; wars median 23; slain median 9; overruns median 8; T16 2.36; standing 77 and population 197,317 (below the default bounds) |
| `calm` | wars median 13.5; FG1 median 53; FX2 0.70 (few wars); CB1 median 3 |
| `turbulent` | wars median 39; FG1 median 126, max 272; FX2 1.33 |
| `civs4` | FG1 median 260 (156 to 374); wars 95; alliances 59.5; CB1 median 8 (bound >= 6), a famine or plague line in 30 of 30; T18 median 956 (bound 1,050), max 1,128, none over L(C); UD median 2.5; FX2 1.08; ruins at E 74.5 |
| 512 (10) | FG1 median 64; UD3 **60.7%** of 28 risings (bound <= 60%); T16 22.7 per thousand (15 of 660; default bound only); standing 33.5 |
| 2048 (10) | UD1: a rising in 10 of 10 (bound >= 6), median 7; CB1 median 8, a calamity line in 10 of 10; T18 median 1,007, max 1,230, none over L(C); FG1 median 233 |

Not measured: FX4 (heroes' hunt rate with and without a hero) and CB2's comparison build with `SUMMARY_CALAMITY_MAJOR_MAX` 0; the civilization median of 0.26.0 for FX5 (11 is reported).

## 5. Findings

1. **FG1 maximum above 200** in both default sets (226, 208) and in `high` (201), `turbulent` (272), `civs4` (374) and 2048 (366). The median is in bounds. Probably a few long-lived worlds with many civilizations; a rule reading for the lead (the bound is a r2 estimate).
2. **FG8 figures alive through a quiet step 20% against >= 30%** in both sets (17.8% to 27.4% elsewhere). The age ratio (8.0) is in bounds, so the miss is in the definition of "alive through a quiet step" (my measure: at least 25 years of a quiet phase inside the figure's life, from `m6.py`) or in the number of figures that live that long. Check the definition against the spec's reading before tuning.
3. **FG5 `monsters_slain_by_heroes` 27.9% and 27.8% against 30 to 70%** in the defaults (33.5% in `high`, 22.7% in `low`). Close to the bound; `hero_hunt_permille` and `hero_might_permille` are the knobs.
4. **M4b: 63 worlds in set A against >= 65** (set B 68; the bound is Paul's). 0.26.0 had 58 and 55.
5. **FX3 betrayals by grasping rulers: 1.53 times their share in A, 1.27 in B** (>= 1.5). The granted-share part (>= 20 points) holds by a wide margin (52 and 56 points).
6. **FX1 in B 54.3%** (A 55.7%), `low` 51.9%: at the lower edge.
7. **FG11: one world in B has `the Old` over the bound** (more than max(2, figures div 10) bearers; bearers of one text reach 8 in both sets). `the Old` is the most frequent epithet (316 and 313 in 100 worlds).
8. **UD3 at 512 is 60.7%** of 28 risings (bound <= 60% for other sweeps; a sample of 10 small worlds).
9. **T16 in `low` is 3.07 and in `high` 2.36 per thousand** (bound is for defaults; 512 22.7, 15 repeats in 660 battles).
10. **FX2 outside the defaults:** `calm` 0.70, `civs4` 1.08, `low` 1.29 (counts of wars under each temper are small and `faerie` has none in any sweep).
11. **FG2 reigns:** goblins 146 and 122 per realm (median; max 290 and 286) against the spec's "about 260": the maximum matches, the median is half of it. Reported by race in section 4.
12. **Oddity, no defect: a hero may rise after the raids of a monster slain earlier in the same year** (seed 90, `hero_rose` e134 names monster 3, slain e133 in the same year). Spec 6.7 reads the rise from a raid in T4, after the hunts of the year, so this follows the rules; the text "After the raids of X, Y took up the hunt" still reads true. For the world critic.

## 6. Defect reports

**D1 (checker, fixed): `p5_02_monsters_reference.py check-world` invariant 4 flags a `hero_rose` that follows its monster's death in the same year.** Reproduce: `worldgen-cli generate --seed 90 --map-size 1024 --data game/data --out s90.db --force`, then `python3 -B worldgen/tests/reference/p5_02_monsters_reference.py check-world s90.db` gives `invariant 4: monster 3 event e134 outside its life`. Expected: spec 6.7 (the rise is read from a raid in T4, after the hunt of the year). Cause: `hero_rose` is in `MONSTER_EVENT_KINDS` and the life check does not allow it after `died`. Fix: a `hero_rose` in the monster's year of death is exempt (this commit). Seen in default seeds 33, 90, 92 and `high` seeds 6, 13, 15, 19, 26; all of those that were regenerated (33, 90, 92, 6, 13, 15, 19, 26) pass with the fix.

**D2 (checker, open, known): `p5_03_relations_reference.py check-world` invariant 4** fails on `civs4` seeds 10, 13, 22 (`colony N's first rows ... are not its founder's race`). Reproduce: `generate --seed 10 --map-size 1024 --civs-per-race 4`, then the check. It is the stale check CLAUDE.md lists (a colony founded in the year its region is ceded; seed 10's colony 145 was founded in year 4733). I did not chase it further.

No stage-logic defect was found.

## 7. Artifacts

`~/.cache/the-unmapped/out/p5-06-validation/`: `logs/` (builds, `ctest-*.log`, `ctest-slow.log`, `hashes.log`, `agg-variants.txt`), `sweep/<tag>/` (`m.jsonl`, `fx2.jsonl`, `times.txt`, `refs.txt`, per-world `.full.txt` and `.summary.txt`), `refout/`, `scripts/` (`build.sh`, `ctest.sh`, `sweep7.sh`, `m1.sh`, `m2.sh`, `m6.py`, `agg6.py`, `fx2.py`, `fxagg.py`), `phrase/`, and `critic/` (default seeds 0 to 11 and `high` seeds 0 to 3, each with `world.db`, `world.report.txt`, `summary.txt`, `full.txt`; for default-s0, default-s1, high-s0, high-s1 also `territory-markers.png` and `territory-markers-faded.png` at the end year, scale 2).
