# P5-01 carried-over follow-ups: validation (generator 0.21.0)

Validated by the validation specialist on 2026-11-22 on branch `p5/01-validation` (from `origin/p5/01-merged`, the P5-01 integration branch merged with the P4-01/P4-02 candidates). Generator 0.21.0, schema 7, history report format v2. Spec: `docs/specs/p5-01-followups.md`, targets in its section 8.

Verdict: **FAIL on three counts, none of them a generator defect found by the sweep**: (1) `worldgen-tests-timeline` (and its slow twin) fails on every preset because `p4-01-timeline.txt` was recorded for 0.20.0 worlds and fixtures; a test writer must re-record it from the reference (section 3); (2) target T15 is missed by one name, the region `Tarden` (section 5); (3) the absolute timing bound cannot be judged on this machine, though 0.21.0 is as fast as 0.20.0 (section 6). Everything else is met: all fifteen world cases are byte-identical across four Linux presets, the goldens are recorded, and every other test entry passes.

Machine: 6 cores, load average 6 to 12 throughout (other agents running), so every wall time below is inflated against the "quiet machine" figures of the spec.

## 1. Cross-compiler world hashes and goldens

`tools/ci/world-hashes.sh local linux-gcc-release linux-clang-debug`: all 15 cases identical (detail lines, world reports, both history reports of each case with history). `compare` over four presets (linux-gcc-release, linux-clang-release, linux-gcc-debug, linux-clang-debug): all 15 cases identical, 5 files each. `expect linux-gcc-release` against the recorded goldens: 15 of 15 ok. Windows was not available.

Recorded in commit "P5-01 validation: record the world goldens at generator 0.21.0":

| File | Change |
| --- | --- |
| `worldgen/tests/golden/p0-worlds.txt` | all 8 cases (`empty-*`, `pattern-*`) |
| `worldgen/tests/golden/p1-worlds.txt` | the 4 `geo-*` cases |
| `worldgen/tests/golden/p3-worlds.txt` | the 3 `hist-*` cases |
| `worldgen/tests/db_determinism_test.cpp` | `GOLDEN_CASES` (5 values) and the comment |
| `worldgen/tests/stages_pipeline_test.cpp` | the two inline hashes (about lines 496 and 509) |

Why they changed: every file carries `meta generator_version` 0.21.0 (and the content hash of the changed `game/data`); the history cases also change through the P5-01 rules. The inline constants hash worlds written with no data and no stage, so they move only through the version string. I did not repeat the 0.19.0-to-0.20.0 substitution proof (putting the old version string back into the new bytes) for them; the new values are the values the 0.21.0 build writes, read from the failing checks, and the two tests that hold the same world (`empty-seed-0` in both files) agree with each other (1bfedb0a...).

Hashes (SHA-256 of the world file, `hash --db`):

| Case | SHA-256 |
| --- | --- |
| empty-phrase | cf3a57c0acc1805c9764f3171db1ebea535db0de2c9bb7ef822da44de46452c4 |
| empty-seed-0 | 319fd325a8e0100bf5a8797766d968f08b73dd66d7df14db072adb0b5e7b9be7 |
| empty-seed-0-512 | d29d634bc7f4a9ac7bd1af0e2a2c9cb448a1ea0b598094a3d74b5270d9be8dc5 |
| empty-seed-1 | e6b1fc6ed1d1a54b9f1ea5b49b72d304d307885ad0544a6a0a6128c1ca7e3067 |
| empty-seed-42 | b80e3e11d55274a5f9567f89d87e8787277f3be019d8960d615f7f85b8922302 |
| empty-seed-max | d859827783e462076ece22066530483fb3cf9f74e6ef2b75cd088c0ea8d2af8c |
| pattern-seed-0 | aed29bca130de52ea022ae96fb3b26fe2c167282cd61aff2acb1727a3d8c76a8 |
| pattern-seed-42 | 9b1d5c1ed6017dea16177eb3d93e2a194d05be2506c9be23c812d58faac01fcc |
| geo-phrase-512 | 6a65ae9d33c998a7405ad9d9aa7747f81e5a15a0e16d8a1c19887563b33415f2 |
| geo-seed-0-512 | 7fa4fd6a3dd851b3491751e4fa0a13766ff40c0933121843d5be5bfdfbf2cd34 |
| geo-seed-42-512 | de694fd0ddf76aa1363c13991a96073731db50ca163d6fe6175d7e1129ad7bf6 |
| geo-seed-1-1024 | 00ac54e88e93bbdcb3df9241b4f0c025a313907f8a66534137fd380588f4211a |
| hist-span-2000-512 | 5e491584c393bd6df9a1875168ab9117ab85126014f01f65bcfdba495b92dc2f |
| hist-races-elf-orc-1024 | a15779b2b4f96205345c467a57a717bc15253198ec625e47761f50cbfffa5e73 |
| hist-dense-1024 | 0af519f2ee5db41a9354d2be5a7a7554e9e60feeb098528bb8b8693676be9258 |

Not touched: `p1-06`, `p3-01` to `p3-07` and `p4-01-timeline.txt` goldens.

## 2. Test results per preset

| Preset | ctest | Result |
| --- | --- | --- |
| linux-gcc-release | 38 entries, 37 pass | FAIL: `worldgen-tests-timeline` only |
| linux-clang-release | 38 entries, 37 pass | FAIL: `worldgen-tests-timeline` only |
| linux-gcc-debug | 38 entries, 37 pass | FAIL: `worldgen-tests-timeline` only |
| linux-clang-debug | 38 entries, 37 pass | FAIL: `worldgen-tests-timeline` only |
| linux-clang-asan | 38 entries, 37 pass (73 min under load) | FAIL: `worldgen-tests-timeline` only; no AddressSanitizer or UBSan report in the log |
| linux-gcc-release-slow | 14 entries, 13 pass | FAIL: `worldgen-tests-slow-timeline` only |

Determinism lint and its self-test: clean (174 C++ files, 0 findings). The five presets above configured and built without warnings.

Every other entry passes on every preset, including the P3-07 history report entries and their slow 1024 cases, the partition checks, and the CLI tests that read `p0-worlds.txt`.

## 3. Failure: `p4-01-timeline.txt` is stale (needs the test writer)

Minimal case: `ctest --preset linux-gcc-release -R '^worldgen-tests-timeline$'`; same four test cases on every preset. First error line of each, from `worldgen/tests/timeline_golden_test.cpp` and `timeline_support.hpp`:

| Test case | Failing items | First error |
| --- | --- | --- |
| `P4-01 13.3: T1 through the writer API has the reference's table hashes` | `t1-edges`, table `meta` only | `timeline_golden_test.cpp:74: CHECK( got.at(table) == g.tables.at(table) )` (meta `2ce30a53...` against recorded `5ef9b32c...`) |
| `P4-01 13.4: the fixtures (P3-07 and T1) against the golden file` | 12 fixtures `h1-war-and-ruin`, `h2-minimal`, `h3-runaway`, `h3b-runaway-1000`, `h3c-share-ties`, `h4-budget`, `h5-saturated`, `h6-notes`, `h7-runs`, `h8-frames`, `h9-realms`, `h10-floor`: table `meta` only | `timeline_golden_test.cpp:35: CHECK( it->second == hex )` (`h1-war-and-ruin` meta `71fb3529...` against `89b5b524...`) |
| the same | `h11-v2`: no golden lines in the file | `timeline_support.hpp:513: REQUIRE( it != golden().end() )` "no golden lines for h11-v2" (FATAL) |
| `P4-01 13.4: the 512 world cases against the golden file` | `geo-phrase-512`, `geo-seed-0-512`, `geo-seed-42-512`, `hist-span-2000-512`: static hash `S` and many `D(y)` | `timeline_support.hpp:523: CHECK( sha(s) == g.static_hash )` (`geo-phrase-512`: `1981c6ab...` against `171e561d...`), then `D(0)`, `D(2094)`, ... differ |
| `...(slow): the 1024 world cases and sample-42-1024` | `geo-seed-1-1024`, `hist-dense-1024`, `hist-races-elf-orc-1024`, `sample-42-1024`: same | same lines |
| `P4-01 13.4: every case of the golden file is checked by a test` | (the FATAL above stopped it, doctest reports it with the fixtures case) | |

Root cause: the golden was written from 0.20.0 worlds. The fixtures differ only in `meta` (the version), and the world cases in the history they read, which P5-01 changed. `h11-v2` is a P5-01 fixture (report v2) the golden never had. Not regenerated, per the brief; the test writer must re-record `p4-01-timeline.txt` from `p4_timeline_reference.py` against 0.21.0 worlds (the world cases: the 512 cases are from the checkout's `game/data`, the four 1024 cases as `world-hashes.sh` makes them, `sample-42-1024` = `generate --seed 42 --map-size 1024`). The C++ side agrees with the reference only once that is done; until then the timeline change is not validated by its own golden. The CLI timeline tests and the layer catalogue entry pass.

## 4. Sweep and targets (1024 unless stated, generator 0.21.0, gcc-release)

Worlds: `default` seeds 0 to 99 and `civs4` seeds 0 to 19 are the engineer's (`~/.cache/the-unmapped/out/p5-01-impl/sweep/`); seeds 0, 17, 42, 84, 99 were regenerated with my build and are byte-identical (`cmp`). I generated `calm` and `turbulent` (seeds 0 to 19, `--event-intensity`), `d512` (seeds 0 to 19) and `d2048` (seeds 0 to 4). 185 worlds in all. On every world: `report --history` at both levels exits 0 (370 calls); the C++ report is byte-equal to the independent reference at both levels (`p3_report_reference.py compare`: 185 of 185); and `p3_conflict_reference.py check-world` has 0 failures (185 of 185; 2,000 to 8,000 checks a world, among them T3's `guard_with_dest`, T6's last-battle rule, T7's kill bound as amended by 24.5, invariant 18). Report time: at most 1.05 s (summary, one cold call), 0.09 s (full).

| Target | Bound | Measured | |
| --- | --- | --- | --- |
| T1 hollow wars per mille, 100 default | at most 250 | 215 (488 of 2,269 fought wars; 2,601 declared, 332 unfound) | met |
| T2 `wars_same_goal_unchanged_max` | at most `weary_wars` (3) in every world | max 3, 0 worlds above, in all 185 worlds | met |
| T3 no last refuge holds a siege while a destination exists | 0 | 0 in 185 worlds (reference `guard_with_dest`); `refuges_held_no_destination` is a count of permitted holds (default 0, civs4 4, turbulent 9, d512 56) | met |
| T4 no race extinct | 0 worlds | 0 of 185 (`races_alive` = present races) | met |
| T5 orc wins against losses, default | wins >= losses | 169 / 62 (civs4 45/18, calm 24/10, turbulent 29/20, 512: 20/5, 2048: 19/9) | met |
| T6 no ending whose loser won the last battle (except by aim) | 0 | 0 (reference, 185 worlds) | met |
| T7 no battle above `kill_max_permille` | 0 | 0 (reference, 185 worlds) | met |
| T9 worlds with one standing orc settlement, no war won, nothing sacked | at most 7 | 2 (seeds 42, 94); the looser "exactly one standing" reading of `orc11.py`: 3 (adds 97) | met |
| T10 worlds with no orc act after E - 250 and at most two standing | 0 | 0 (default; also civs4, turbulent, 2048) | met |
| T11 median years awakening to first orc colony, declaration, sack or capture | below 100 | 12.5 (p90 39, max 368; 1 world at or above 100) | met |
| T12 first orc war: won against lost | won >= lost | 38 / 17 (16 truces, 2 civ_conquered, 7 searches abandoned, 1 open; 81 worlds) | met |
| T13 war declarations not citing a claim on their region | 0 | 0 of 2,601 (calm 0 of 293, turbulent 0 of 875, civs4 0 of 1,737); 304 fresh claims, median 2 a world, max 13 | met |
| T14 grim drawn region names, per mille of drawn | at most 50 each at 1024 and 2048 | 1024: 44.1 (191 of 4,328, 100 worlds); 2048: 34.7 (33 of 951, 5 worlds); 512 reported: 52.9 (47 of 889) | met |
| T15 names: no `fag`, `didin`, `ririn`; no word `shiva`, `hindi`, `fannie`; no triple letter, crude fragment or `tard`; fallbacks 0 | 0 each | `fag`, `didin`, `ririn`, `shiva`, `hindi`, `fannie`, triple letters: 0 in 33,896 names of 185 worlds. **`tard`: 1**, region 38 `Tarden` (South Wood), default seed 84 | **missed by one** |
| T16 battles repeating an earlier battle's sides and tolls; mixed digit/word toll clauses | at most 10; 0 | 9 of 8,820 (civs4 9 of 6,080, turbulent 9 of 3,307, 512: 19 of 1,056, reported); 0 mixed | met |
| T17 calamity share of the summary chronicle, default | median at most 10%, every world at most 25% | median 7.8%, max 22.8% (seed 72) | met |
| T18 summary lines (Paul's restatement), default | median at most 700, max at most 800 | median 571.5, max 754 (seed 66), min 321 | met |
| T18 `civs4` | median at most 1,100, max at most 1,400 | median 1,046, max 1,353 (seed 1) | met |
| T19 `[present]` claims and refuges marked `; hidden` equal the reference's | every world | equal in 185 of 185 (reference compare is byte exact at both levels) | met |
| T20 flare-ups shorter than `flare_min_years` | 0 | 0 of 383 default (0 of 272 civs4, 0 of 164 turbulent, 0 of 18 calm) | met |
| Quick aim wins (P3-06 22.5) | at most 3 | 0 | met |
| P3-00 12.3 hard bounds | every world | reference `check-world`: 0 failures in all 185; no world with fewer than 6 races alive | met |
| Timing, `generate` seed 0 at 1024 | brief: under 60 s; spec 8: at most 5 s above 0.20.0 | see section 6 | not judgeable on this machine; brief's 60 s met |

Reported, with no target:

| Set | Worlds | T1 | T5 | T9 / T10 (orc11) | T17 median / max | T18 median / max | fresh claims | claim_dropped |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| default | 100 | 215 | 169 / 62 | 3 / 0 | 7.8% / 22.8% | 571.5 / 754 | 304 | 161 |
| civs4 | 20 | 202 | 45 / 18 | 1 / 0 | 0.0% / 0.5% | 1,046 / 1,353 | 246 | 248 |
| calm | 20 | 232 | 24 / 10 | 0 / 1 (seed 15) | 9.0% / 12.3% | 457.5 / 684 | 34 | 9 |
| turbulent | 20 | 207 | 29 / 20 | 2 / 0 | 3.8% / 15.7% | 675.5 / 948 | 106 | 68 |
| 512 | 20 | 295 | 20 / 5 | 5 / 6 | 12.3% / 23.9% | 429 / 563 | 20 | |
| 2048 | 5 | 197 | 19 / 9 | 0 / 0 | 0.0% / 1.8% | 1,063 / 1,116 | 19 | |

Summary statistics, default (100 worlds): simulated years 1,169 to 1,816 (median 1,350); events 229 to 1,537 (median 682); wars declared 4 to 64 (median 25); sacks 1 to 36 (median 12); flare-ups 0 to 14; civilizations 7 to 15; hollow-war share 0 to 500 per mille by world; races alive 6 in every world; 994 standing orc settlements at E over 100 worlds, every world has orcs; T8 (worlds in which orcs declare a war) 81.

Orc figures at 0.21.0, default: first colony median 12.5 years after awakening; first declaration median 166 years after (81 worlds); 38 of the first wars won by the orcs, 17 lost.

## 5. Failure and statistical oddities

**T15, `Tarden`.** Seed 84, map size 1024, default settings, generator 0.21.0: `worldgen-cli generate --seed 84 --map-size 1024 --data game/data --out S.db`, then `python3 -B worldgen/tests/reference/p1_regions_reference.py names S.db | grep -i tard` prints `38|Tarden|Tarden|wood||t0|South Wood`. Spec 8 T15 asks for no name with `tard`, "names and regions". The filter of P3-02 section 11 (data kind `name_filter`) applies to the name builder's roots; region names come from `placename` data (P1-06) and are not filtered, so a region draw such as `Tar` + `den` passes. Which of the two readings the spec intends is for the lead and the spec writer: either T15 means names only (met) or the region names need the filter (missed, 1 of 4,951). Region names changed in P5-01 (the `trait_barren` list edits), so the name is new at 0.21.0; the 375 worlds of 0.20.0 had none. I changed nothing.

Oddities, all passing:

- **`civs4` and 2048 summaries hold almost no calamity lines** (median 0.0%, max 0.5% and 1.8%), against 7.8% at default. T17 is defined for default only, but a summary of a larger world that shows no famine or plague at all may not be intended; the summary threshold scales with the world. For the critic.
- **Default T9 has two definitions**: the spec's (restated) gives 2, the earlier `orc11.py` (exactly one standing settlement) 3. Both are under 7.
- **512 maps**: T10 (no orc act in the last 250 years) holds in 6 of 20 worlds (by `orc11.py`'s looser definition, which does not check the standing count) and T9 in 5 of 20; T1 is 295 per mille. No target applies at 512; small worlds leave the orcs little room. Calm has one T10 world (seed 15).
- `civs4` T18 maximum 1,353 is within 4% of its bound of 1,400; turbulent summaries reach 948 lines (no bound).
- `refuges_held_no_destination` is 56 over the 512 set (6 of 20 worlds), 4 in one civs4 world and 9 in one turbulent world: permitted holds, T3 itself is clean.

## 6. Timing

`generate --seed 0 --map-size 1024`, gcc-release, three runs interleaved with the 0.20.0 binary (0.20.0 data for it), machine load 6 to 8: 0.21.0 13.3, 17.0, 17.7 s wall; 0.20.0 13.9, 16.9, 18.2 s wall; both 26.1 to 26.3 s user. So 0.21.0 is within noise of 0.20.0 on this machine. A last set of three runs at load about 5: 12.0, 11.8, 11.3 s wall (25.8 s user, the generator uses two threads). The machine is slower than the spec's "about 9.3 s, quiet" (26 s of user time), so the spec's absolute bound of 14.3 s cannot be checked here; the brief's 60 s is met by a wide margin. Other generate times under load, median and maximum: 512 7.3 s and 8.8 s, 1024 23.2 s and 27.0 s (two jobs at once), 2048 88.7 s and 90 s (spec: 37.5 s quiet; again load). Peak-load report time: summary 1.05 s, full 0.09 s.

## 7. Artifacts

Under `~/.cache/the-unmapped/out/p5-01-validation/`: `sweep/<set>/` (worlds, both reports per world, `ref.txt`, `cmp.txt`, `times.txt`, `rtimes.txt`), `phrase/` and `phrase.summary.txt`, `default/s{0,5,42}.{summary,full}.txt`, `civs4/s0.*`, `calm/s3.*`, `turbulent/s3.*`, `logs/` (builds, ctest, world-hashes), scripts `job.sh`, `bat.sh`, `agg.py`, `t14.py`; the world hashes of the fifteen cases in `new-hashes.txt`.

README sample (`worldgen-cli generate --seed-phrase "The Unmapped"`, 1024, default data, generator 0.21.0): file SHA-256 `c29df29a98cff62aa66c75ceb17215b8e16b1c653609e183805da0b0a12adb92` (`hash` and `sha256sum` agree); `content_hash` in `[meta]` `8492abec5c22c529be0a764c5dbd3a556fdfca799e9f07dfe69e90c3c98d25ab`; the README's `[tables]` counts and layer lines come from `phrase/world.report.txt`. The README's text still shows generator 0.20.0 and the 0.20.0 hash; the lead updates it.
