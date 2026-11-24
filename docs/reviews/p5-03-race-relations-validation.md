# P5-03 race relations, mixed settlements and migrations: validation (generator 0.24.0)

Validated by the validation specialist on 2026-11-24 on branch `p5/03-validation` (from `origin/p5/03-int` at 6cabd78). Generator 0.24.0, schema 10, history report format v5. Spec: `docs/specs/p5-03-race-relations.md` r2, targets in its section 21. Form: `docs/reviews/p5-02-beasts-monsters-validation.md`. MSVC and Windows were not available (CI).

Verdict: **no generator defect found; the five Linux presets pass `ctest` except the known D1 reader test; sixteen cases identical on all five; goldens recorded. Targets: five misses on the default sweep** (RR1 pairs met in 84 worlds against 90; MX3 expulsions maximum 22 against 10; F1a most raids on one place maximum 33 against 20; F1c raids citing an earlier raid 38.7% against 50%; M4 lair on a former settlement open 62 worlds against 65, with 66 on seeds 100 to 199, 128 of 200 together), and two observations beyond the bounds (T18 maximum 906 on seeds 100 to 199, RR3 worlds with an isolated race). Two reference-script false positives and one stale reference check are listed in section 6.

Machine: 6 cores shared with two other agents (load 3 to 6), so `ctest` times are inflated.

## 1. Presets

| Preset | Build | `ctest` | Result |
| --- | --- | --- | --- |
| linux-gcc-release | 43 min (cold, 3 jobs) | 319 s first run | 41 of 43 before recording: `worldgen-tests-other` (hash constants) and `worldgen-tests-relations` (D1). After recording and a rebuild: 42 of 43, only D1 fails |
| linux-clang-release | 766 s | 459 s | 42 of 43 (D1 only) |
| linux-gcc-debug | 575 s | 1,676 s | 42 of 43 (D1 only) |
| linux-clang-debug | 364 s | 1,522 s | 42 of 43 (D1 only) |
| linux-clang-asan | 913 s | 651 s | 42 of 43 (D1 only), no sanitizer report |
| linux-gcc-release-slow | (built) | 2,131 s | **19 of 19 pass** (including `-slow-relations`, 880 s) |

D1 is `history_relations_reader_test.cpp:98`, case `race-participant` (the only failed assertion: 56,670 of 56,671 pass in `worldgen-tests-relations`), as stated. `worldgen-cli-tests-other` (which reads `p0-worlds.txt`) and `cli_p1_generate_test.cpp` pass with the recorded files; the inline hashes the brief names at `cli_p1_generate_test.cpp` lines 92 to 175 hold no 64-digit value (the test reads the golden files), so only `db_determinism_test.cpp` and `stages_pipeline_test.cpp` needed constants. The determinism lint was not re-run (no C++ under `worldgen/src` changed). Logs: `~/.cache/the-unmapped/out/p5-03-validation/logs/`.

## 2. Cross-preset hashes and goldens

`tools/ci/world-hashes.sh compare` over `linux-gcc-release linux-clang-release linux-gcc-debug linux-clang-debug linux-clang-asan`: **all 16 cases identical** (detail lines, world report, both history reports, territory and markers hashes; 6 files per case with history). `expect` against the recorded files: 16 of 16 ok on `linux-gcc-release` and on `linux-clang-asan`. No new `world-hashes.sh` case: spec section 23 keeps the sixteen cases ("no new setting"), so no case count in a test or in `CLAUDE.md` changes (it already says sixteen).

Recorded (LF, C-locale order): `worldgen/tests/golden/p0-worlds.txt` (8), `p1-worlds.txt` (4), `p3-worlds.txt` (4); `db_determinism_test.cpp` `GOLDEN_CASES` (5) and `stages_pipeline_test.cpp` lines 496 and 509, with their comments. Every other golden file is untouched. Values from the gcc-release build; all five presets agree.

| Case | SHA-256 |
| --- | --- |
| empty-phrase | 8d48701d2cb36bf39a2c38baff5606874099b12c365ed0a2c5a8c129e32e92cc |
| empty-seed-0 | d007ccb02ff22ebecc9376b382a8bbeb7fa1c290ce3111e0c21c4b3813d7fcee |
| empty-seed-0-512 | 5500554f5f01375d7017c909acbddfc20cb70422f9774483c2e39abf53f64740 |
| empty-seed-1 | 5803602ddd594eec879b8295ce999fa6962c3c4d722c0cec188198c20e8f0a29 |
| empty-seed-42 | 17203e3371e5fe34f392d752dd74d23531eb8ca48d4e96e5336e865b888783ea |
| empty-seed-max | 58d67b8ef6103914e641f0c84667d45627a2a5fd3cc6ecc0aa73df6f2b61cf7d |
| pattern-seed-0 | a283651b26106130d0e044cff0ef1fa9d2df2065efb1f1f0b0aae7be9808d1ac |
| pattern-seed-42 | a0ad6f56bae87c79d4f0e8579e30b67abe84417aefbf5505394c124eb91c18cf |
| geo-phrase-512 | 2a6a39322d963e3090dc869f08c3f8930145ffe6e5805a30532bd82a780f12c3 |
| geo-seed-0-512 | 8b4c30ba82ec41d32cb4466f06be0fdb19e9a0f766dd2e17e8055df2899888c5 |
| geo-seed-42-512 | d165d96b2c2e37df42d1c8982b8aa723b1f2411ff223ecc4b48f60f0121c9413 |
| geo-seed-1-1024 | 68f70c1adc07bd329686d31c28271852f3649fd48da44023b7abc190b79a3a17 |
| hist-dense-1024 | 53e90e03a632cb60384b9b68a946e66069f5d2fafe772cd891a20957132adb02 |
| hist-races-elf-orc-1024 | df0251c68610f205aec3a6a85962ac37c97f943c5d59d339bb955a1c38ee332b |
| hist-span-2000-512 | 2c4167be778d0dcaff5d76eb69e2c550f23d6c08288edd78d157573447c24ed0 |
| p5-monsters-high-1024 | dabd4a462e55bbf21fffbf9c039bd5df3304c1da26b939489667ac9457f85c28 |

Unit-test constants (no data, generator 0.24.0): `empty-seed-0` 4c607319bcccc2d4bf23534af23fdad07f6b6990566bcf94a66576ed9c90a89d (also `stages_pipeline_test.cpp:496`), `empty-phrase` e4a13fe943ded990714bde0d30abffacedabc6cbdc014194a6ab890c3a640917, `empty-seed-1` 615aa8cc863fbad13b2b6e58e2e642184397e38397c090d20f9f4a46c6550e1d, `empty-seed-42` d21a08b1794df403367de6645a2187255176469449b681edfb490f880c3c80c7, `empty-seed-max` b01f8ad0fe1225e3961f97a8ed702f0098ba7f4ccb82bc0a2af3aefd476a9d17, test pattern 947027056feb7247ca715ebc0be0ab3a1cf083ad7e5fda5c9277f4694fa91529. Why they move: schema 10 (two new tables), generator 0.24.0, the new `content_hash` (new ids, fields, phrases, filter word, report entries), and every history through part B and part A.

**README phrase world** (`generate --seed-phrase "The Unmapped"`, 1024, default data): file SHA-256 (`hash --db` and `sha256sum` agree) **`38f412c511be443691ca2db745f8e59a3106d409b179f7706a882459b76790bc`**. Files: `~/.cache/the-unmapped/out/p5-03-validation/phrase/{world.db,world.report.txt,summary.txt,hash.txt}`. (The README still shows the 0.23.0 hash `531e39e1...`.)

## 3. Sweeps

All at the shipped data, gcc-release binary, 1024 unless stated: `default` seeds 0 to 99 and `default2` seeds 100 to 199 (a second sample); 30 each (seeds 0 to 29) for `low` and `high` (`--monster-pressure`), `calm` and `turbulent` (`--event-intensity`) and `civs4` (`--civs-per-race 4`); `d512` and `d2048` seeds 0 to 9. **370 worlds, every `generate` exit 0 with empty stderr, every `report --history` at both levels exit 0 with empty stderr.** Independent references on 170 worlds (default 100, `high` 30, `civs4` 30, `d512` 10): `p3_report_reference.py compare` equal at both levels in **170 of 170**; `p3_conflict_reference.py check-world` **0 failures in 170** (843,270 checks: T3, T6, T7 and the conflict invariants); `p5_03_relations_reference.py check-world` 156 of 170 pass and 14 fail on one check, a reference false positive (section 6, finding 1); `p5_12_renaming_reference.py check-world` 163 of 170 pass and 7 fail on a reference gap (finding 2). Measurements are the engineer's scripts (`m502.py`, `m503.py`, `t501.py`, `t16.py`, `t10_12.py`, copied to `scripts/`) plus my `extra.py` for the bands, ruins and pairs; run on the engineer's worlds' recipe, they reproduce his figures exactly (62 lair-open, wars 25.5, 0.93 repeated battles per thousand, T18 620 and 785).

### Section 21 targets, default sweep (seeds 0 to 99)

| Target | Bound | Measured | |
| --- | --- | --- | --- |
| RR1 pairs met | every pair of present races met in at least 90% of worlds; median at least 13 of 15 | median 15 of 15; **all 15 met in 84 worlds** (87 on seeds 100 to 199) | **missed** (the 16 other worlds each have a race that never meets any other, see RR3) |
| RR2 `disposition_turned` | median 1 to 4, max at most 10, at least one in 50 worlds, every band entered | median 1, max 6, 73 worlds with one; turns into hatred 116, wariness 30, friendship 3 | met |
| RR3 participation | every present race named by a P5 event in every world with two races | 10 worlds have a race with no P5 event (seeds 4, 6, 26, 36, 62, 74: dwarves; 49, 88: humans; 77, 87: elves), 5 more on seeds 100 to 199. **In every case that race has no `race_relation` row: it never meets another race** (an isolated race on its own land, which the spec says to report) | reported to the lead |
| MX1 mixed standing settlements at E | median 3 to 12; at least one in 80 worlds | median 8 (report `[stats]`; 10 by final `settlement_race` rows with more than one race), 99 worlds | met |
| MX2 `refugees_arrived` | median 3 to 15 | median 5, max 30; sheltered median 4,652 | met |
| MX3 `people_expelled` | median at most 3, max at most 10 | median 3, **max 22** (seeds 100 to 199: median 2.5, max 12) | **missed (max)** |
| C1 wars | median 21 to 26 (0.23.0: 23.5) | median **25.5** (+8.5%); seeds 100 to 199: 25.0; enmity rows median 15 | met |
| T16 repeated battles | at most 2 per thousand | 8 of 8,605 = **0.93**; seeds 100 to 199: 0.99 | met |
| G2 standing settlements, population at E | medians at least 81 and 214,500 | **83** and **217,827** (seeds 100 to 199: 86, 217,408) | met |
| F1a most raids by one monster on one settlement | median at most 8, max at most 20 | median 5, **max 33** (seeds 100 to 199: 6, 30; 0.23.0 max 50) | **missed (max)** |
| F1b years from first raid to slaying or driving off | median at most 150 | median 27 (n 477): ogre 28, troll 28.5, undead 22, wyvern 24, giant 48, dragon 42 | met |
| F1c raid lines | no phrase above 40%; (dead, plunder) pair at most 1%; at least 50% cite an earlier raid | most common phrase 11.8%; top pair 0.61%; **38.7% cite an earlier raid** | **missed (citations)** |
| F1d `settlement_fled` | median 1 to 3 | median 1, max 9, total 177 | met |
| F2 told monsters | every notable monster told in the summary; T18 holds | every default summary has its `[monsters]` block (100 of 100) | met |
| F3a abandonments with `lost` > 0; kind 16 share | at least 80%; 5% to 25% | 421 of 421 (100%); 10.8% | met |
| F3b present ruins older than 1,500 years at E | in at least 50 worlds | **96 worlds**; 81 worlds have a present ruin founded and ruined before year 4000, 237 such ruins in all (0.23.0: none) | met |
| F4 most used epithet | at most 40% | wyvern `The Screamers of` 40 of 106 (37.7%); giant bare 27%; ogre bare 21%; troll bare 19.5%; dragon `the Black` 15% | met (wyvern close) |
| F5 `beasts_bold` by species | wolves at most 60%, boar and bears at least 5% | wolf 33%, boar 25%, bear 18%, giant spider 24% | met |
| F8 overruns of towns | at most 10%; overrun median 2 to 4 | 15 of 283 (5.3%); median 2.5 | met |
| M4 living monster at E | at least 85 worlds | **87** (89 on seeds 100 to 199) | met |
| M4 lair on a former settlement open at E | at least 65 worlds | **62** on seeds 0 to 99, **66** on seeds 100 to 199; together 128 of 200 (64%) | **missed on the bound's sample, met on the second** |
| M5 slain among ended | 20% to 70% | 364 of 664 (55%) | met |
| M1 to M6, G1, G3, S1, B1 | as P5-02 r1 | monsters median 8, max 13; all six kinds; raids in 99 worlds; invariants 3 to 7 clean; ghost places median 3, max 7; once-held regions free median 1 (reported); outposts median 11.5, ended in 98 worlds; `beasts_bold` median 3, max 13; flare-ups max 2 | met |
| T18 summary lines | default median at most 700, max at most 800; `civs4` median about 1,100, max at most 1,400 | default median **620**, max **785** (seed 87); `civs4` median **1,112**, max **1,347**. Seeds 100 to 199: median 618, **max 906 (seed 174; 854 seed 100, 805 seed 161)** | met on seeds 0 to 99; the maximum bound fails on 3 of the next 100 |
| timing | `generate` seed 0 1024 release at most 5 s over 0.23.0; `report --history` under 2 s | 9.12, 9.12, 9.19 s wall, user 25.5 s, load 3 (0.23.0: 9.1 s, 25.5 s); reports 0.01 s | met |

The lead's asked-for figures, default sweep: wars median 25.5 against 23.5; T16 0.93; raids on one place median 5, max 33; expulsions median 3, max 22; turns into hatred 116, wariness 30, friendship 3; race pairs that ever change band per world: none in 27 worlds, one in 45, two in 24, three in 4 (median 1, max 3); mixed settlements at E median 8 (stats) or 10 (rows); `settlement_fled` median 1 (max 9); present ruins median 19 per world, of which abandoned (endings 2 to 6) 755 of 1,985 (38.0%, median 7 per world); present ruins predating year 4000: 237 in 81 worlds (all abandoned ones; sacked ruins fade); summary lines default median 620, max 785, `civs4` median 1,112.

### P5-01 and earlier targets re-measured (default)

T1 hollow wars 207 per mille (bound 250); T2 0 worlds; T3 0 failures (conflict reference, 170 worlds); T5 orc wins 158 of losses 79; T9 7 worlds (bound 7: 13, 21, 29, 40, 81, 95, 99); T10 0; T11 median 11.5 years; T12 first orc war won 27 and lost 20; T13 0 of 2,568; T14 38.4 per thousand names grim (bound 50); T15 none; T17 calamity share median 5.2%, max 14.6%; T18 as above; T19 `[present]` equals the report reference in 170 of 170; T20 0 of 454. All within their bounds; T9 is at its bound.

### Other sweeps (30 worlds; 10 for the sizes)

| Set | wars med | turns med / max | pairs met all | mixed at E med | arrivals med | expulsions med / max | raids on one place med / max | fled med / max | standing med | T16 per thousand | T18 med / max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| low | 24 | 1 / 4 | 26 of 30 | 9 | 5 | 3 / 8 | 2 / 10 | 0 / 2 | 84.5 | 1.92 | 612 / 829 |
| high | 24.5 | 1 / 4 | 24 of 30 | 9 | 6 | 2.5 / 12 | 10 / 28 | 4 / 12 | 78 | 2.02 | 628 / 736 |
| calm | 15 | 0 / 3 | 26 of 30 | 7 | 4 | 2 / 10 | 5.5 / 45 | 1 / 9 | 81.5 | 0.70 | 538 / 707 |
| turbulent | 35.5 | 1 / 6 | 23 of 30 | 9.5 | 9.5 | 2.5 / 12 | 5.5 / 22 | 2 / 8 | 86.5 | 2.24 | 737 / 1,016 |
| civs4 | 91 | 3 / 6 | 29 of 30 | 12 | 31 | 12.5 / 36 | 7.5 / 28 | 3 / 7 | 142 | 3.11 | 1,112 / 1,347 |
| d512 | 14.5 | 0 / 8 | 8 of 10 | 3.5 | 1.5 | 0.5 / 5 | 2 / 9 | 0 / 2 | 37.5 | 9.80 (5 of 510) | 470 / 604 |
| d2048 | 69 | 2.5 / 8 | 8 of 10 | 21.5 | 16.5 | 7.5 / 30 | 7 / 27 | 2 / 8 | 287.5 | 1.13 | 1,180 / 1,283 |

Oddities (none a bound): RR3 holds in every `civs4` and 2048 world; in `low`, `calm`, `turbulent`, `high` and 512 the same isolated-dwarf pattern appears (seeds 4, 6, 15, 26 recur across sets, since geography is by seed). `high` has the lair open at E in 24 of 28 worlds with a living monster; `low` has the lair open in 6 of 20. Expulsions are many in `civs4` (median 12.5, max 36) and 2048 (max 30), in proportion to the settlements. `calm` has no turn in 19 of 30 worlds (median 0), against a bound written for the default. The 512 maps have a repeated-battle rate of 9.8 per thousand on 510 battles (bound applies to the default sweep only).

## 4. Phrase world and critic material

`~/.cache/the-unmapped/out/p5-03-validation/phrase/` (section 2). `critic/`: `default-s0` to `default-s11` and `high-s0` to `high-s3`, each `.full.txt` and `.summary.txt` (report v5), and `render --composite territory --markers` at the end year for `default-s0`, `default-s1`, `high-s0`, `high-s1` (`<name>.png`). The worlds for those sixteen seeds are kept in `sweep/default/` and `sweep/high/`; the other world files were deleted, their reports kept.

## 5. Timing

Section 3's last row of the target table. `generate --seed 0 --map-size 1024`, gcc-release, three runs, load 3: 9.12, 9.12, 9.19 s wall, user 25.5 s each. Sweep median `generate` plus both reports, two jobs in parallel under load: default 15.2 s per world.

## 6. Defects and advisories

No generator defect found. Targets missed are in section 3. For the test writer and the lead:

1. **`p5_03_relations_reference.py check-world` invariant 4 reports a false positive on 14 of 170 worlds** (`default` seeds 22 and 6 others, `civs4` 9 worlds, as listed in `refs-rel.txt`): "colony 82's first rows {2: (900, 0), 4: (100, 3)} are not its founder's race". Reproduce: `generate --seed 22 --map-size 1024`, then `check-world <db>`. Cause: the colony (settlement 82, founded year 4844 by civilization 8, race 2) was founded in the same year as the cession of its region (event 553, `region_ceded`, civ 8 to civ 7 of race 4). Spec 6.7 writes one row per entry per year, the last state of the year, so the first row already shows the new rulers (standing 3, share 100) and the old people as subjects (standing 0, share 900), which is 6.3.3's owner-change rule. All 14 failures have exactly this shape (900/100, ruling 100); no other check fails. The reference should accept a first row altered by a same-year owner change.
2. **`p5_12_renaming_reference.py check-world` gives 7 false positives** ("region N: the later row's rule weight 90 is not above None", six worlds, and 100 in one). The earlier row's rule is read as `None`: for `default` seed 8, region 2 is first renamed by `region_renamed` (event 436) caused by `became_city` (rule `heartland`, weight 50) and later by `sacked` (`capital_sack`, weight 90), which is in order. `max_weight` finds no rule for the cause kind (`became_city`); the generator's own checks pass. (Same family as P5-02's section 7 finding 1, now with heartland as the earlier rule.)
3. **`p5_02_monsters_reference.py check-world` is stale for P5-03's part B**: it fails 7 of 170 worlds on invariants 5 (`dead` at most `kill_max`; 2,538 hits) and 11 (raid causes; 3,336 hits), which P5-03 9.1 and 9.2 supersede (the toll above `kill_max` grows at a quarter of the rate; a raid may cite an earlier raid). No other invariant failed. Not a world defect.
4. **T18 maximum**: 906 lines (seed 174), 854 (seed 100) and 805 (seed 161) exceed the 800 bound on seeds 100 to 199 though the 0 to 99 sample (785) passes; the bound is a sample figure.
5. **`cli_p1_generate_test.cpp`** has no inline hash (the brief's lines 92 to 175): nothing to record there.
6. `CLAUDE.md`: no case count to change (sixteen stays). The README phrase hash changes to the value in section 2.

## 7. Artifacts

`~/.cache/the-unmapped/out/p5-03-validation/`: `sweep/<set>/` (reports of all 370 worlds, `gen-times.txt`; world files of the sixteen critic seeds), `results-all.txt` (every measurement per set), `t501-default.txt`, `t10_12-default.txt`, `refs-{cmp,conf,rel,ren,mon}.txt` and `refout/`, `world-hashes` text at `~/.cache/the-unmapped/out/p503-val/world-hashes/`, `logs/`, `scripts/` (`sweep.sh`, `all-sweeps.sh`, `refs.sh`, `extra.py` and the engineer's copies), `phrase/`, `critic/`.
