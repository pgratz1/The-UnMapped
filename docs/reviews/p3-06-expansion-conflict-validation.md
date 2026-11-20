# Validation: P3-06 expansion and conflict (generator 0.16.0, schema 7)

Validation specialist, 2026-11-20. Branch `p3/06-validation`, cut from `origin/p3/06-int` at `e06c957`: the conflict subsystem, the engine's relation bookkeeping (P3-04 r9 6.7), and the test writer's tests and references. I merged `origin/p3/05-final` (`613799d`, main plus validated P3-05 at 0.15.0) into it, then `origin/p3/06-audit` (`3111b27`). Both merges were clean, so nothing under `docs/specs/` conflicted. After the merges every file under `docs/specs/` equals `p3/05-final`, except `schema.md`, where the engineer's change on `p3/06-int` (kinds 20–30 moved into the catalogue table) is newer. No per-stage golden vector conflicted. `p3-01-data.txt`, `p3-04-engine.txt`, `p3-05-peoples.txt` and `p3-06-conflict.txt` are the `p3/06-int` files, and every one of them passes. The world-hash files and the inline hashes came in at their 0.15.0 values from `05-final` and are re-recorded here (section 6).

The baseline for "what must not change" has three parts. The first is the saved 0.15.0 `linux-gcc-release` binary (`out/p3-05-validation/bin-015/`). The second is its 15 case files (`out/p3-05-val/world-hashes/`). The third is 20 sweep worlds that binary generated here from the `p3/05-final` data root. Spec: `docs/specs/p3-06-expansion-conflict.md` r6 (sections 4–11, 14, 15.2, 15.3, 15.5, 16.0, 16.4), with `p3-04-simulation-engine.md` r9 6.7.

Machine: 6-core Linux desktop. **It was not quiet.** Another agent's work ran during the builds, the release round 1 and part of the sweep: six `sample.py` Python processes, then C++ compiles. Load was 7 to 9 for the release and debug rounds and 10 to 13 for the asan round, which ran alongside the sweep. Each time below carries its load. Windows MSVC is checked by CI.

**Verdict: pass.** No defect against the stage. One defect in the test writer's reference (D1) is a too-narrow invariant reading, not a disagreement on any row. The findings in section 9 are for the spec writer, the critic and the lead.

- **Builds.** All five Linux presets build with warnings as errors, with 0 warnings in any log.
- **Tests before recording.** On all five presets, `linux-clang-asan` in full (3180 s, 0 sanitizer reports), only the seven test cases that compare recorded world-file hashes fail (entries `worldgen-tests-other` and `worldgen-cli-tests-other`). The 40 printed `values:` lines are identical on all five presets (digest `dbd7dce2…`). Every per-stage golden vector passes unchanged, `p3-06-conflict.txt` included.
- **What must not change.** I compared 0.15.0 with 0.16.0 on the 15 cases and 20 sweep worlds. Every layer, every geography table and every chronology table is identical. `race` and `culture` do not change: `prowess_permille` and `aggression` are not stored in any table. `meta` changes only in `generator_version` and `content_hash`. The catalogue grows from 19 to 30 kinds. The log is identical up to the first `claim`, apart from one engine effect: when the claimant has a `quiet_era` open at that moment, the era is shorter (5 of 27 worlds with a claim; section 3).
- **Conflict sweep.** 375 worlds in 15 sets, sized by the lead. My independent recomputation checks every invariant of 14 (except the strain recomputation of item 8 and item 9), every hard bound of 15.5 and the lead's list, and it passes on 375 of 375. The reference's `check-world` ran on all 375. It reports 0 row differences in every world, and 365 worlds pass every check. The other 10 fail only its P3-05 invariant-6 reading (D1).
- **Distributions.** Most medians are within 20% of 15.3 (section 4); the exceptions are listed there. 1024 seeds 0–99 go past the spec's seeds 0–49 and widen every maximum: wars up to 75, events up to 1,795, 7 flare-ups, 1,511 simulated years. These are typical-range findings, not hard-bound breaches.
- **Purity.** `--threads` 1, 2 and 6 give identical bytes, as do the threads-2 critic worlds against the threads-1 sweep worlds for nine seeds. `--until history` is byte-identical to the default. `--until chronology` leaves all 13 history tables empty. Each world-creation flag changes only what it should (section 5).
- **Seed 42 (15.2).** I checked `[history]`, the five flare-ups and their triggers, the events by kind, all 38 wars, the 60 summary lines, the eight `full`-level lines and the end state of all 13 civilizations. All are equal to the spec text.
- **Old files.** The 0.16.0 binary reads a 0.15.0 world: `report` prints a text byte-identical to the 0.15.0 binary's, `hash --detail` is identical and `render` works. The empty world is unchanged: 34 pages, 139264 bytes, schema text 5839 bytes.
- **Cross-preset.** All 15 `world-hashes.sh` cases are identical across the five presets. Recorded `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, the five `GOLDEN_CASES` and the two inline hashes. After recording, `ctest` passes 30 of 30 on all five presets, and `expect` gives 15 of 15 `ok` on each and on the extension build.
- **Extension.** `linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON` builds with 0 warnings, and `gdext-headless-check` passes. Its 15 case files are identical to plain `linux-gcc-release`, and `expect` gives 15 of 15 `ok`.
- **Lint:** self-test 17 snippet files, 0 failures; scan 161 C++ and 13 CMake files, 0 findings.
- **Timing** (quiet machine, load 2 to 3). 0.16.0 `generate` costs the same as 0.15.0 to within 0.4% at all three sizes. The history stage alone takes 79 to 128 ms at 1024, 27 ms at 512 and 170 ms at 2048 (budget 16.6: 4 s, 1 s, 16 s).

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build, five Linux presets | pass, 0 warnings (98 to 207 s each with ccache; load 7 to 11) |
| `ctest` round 1, five presets | 28 of 30 entries pass on every preset; the failures are only world-hash comparisons (2.1) |
| 0.15.0 against 0.16.0: 15 cases, 20 sweep worlds | 35 of 35 as expected (section 3) |
| Conflict sweep, 375 worlds | 0 failures in the independent check; `check-world` 0 row differences in all 375, 10 worlds fail one invariant reading (D1) |
| Purity and flags | as the spec says (section 5) |
| Seed 42 against 15.2 | equal in every value checked |
| `world-hashes.sh compare`, five presets | all 15 cases identical |
| `ctest` after recording, five presets | 30 of 30 on every preset (release presets in a third run, 2.1); `expect` 15 of 15 `ok` on each |
| Extension, `gdext-headless-check` | pass; 15 case files identical to `linux-gcc-release` |
| Lint | 0 findings |

## 2. Tests

### 2.1 Failing set before recording

On every preset, round 1 has 28 of 30 entries passing. The two failing entries hold seven failing test cases, and all of them compare a recorded world-file hash:

- `worldgen-tests-other`:
  - `golden: empty-world hashes match the recorded values (12.4)`, with five `GOLDEN_CASES` values.
  - `generate P1-00 10.1: --until none and every thread count give the P0 empty world`, with two inline hashes.
- `worldgen-cli-tests-other`: `cli 9, 12.6: golden world hashes from p0-worlds.txt`, eight values.

The 40 `values:` lines are identical on all five presets (`grep values: | sort | sha256sum` = `dbd7dce26f12…` on each). Every other test passes, including:

- the new groups `worldgen-tests-conflict`, `worldgen-tests-conflict-worlds` and `worldgen-cli-tests-conflict`;
- the slow suites;
- the `hist-conflict-42-1024` golden case and the five extra golden cases of the test writer;
- every per-stage golden vector.

`linux-clang-asan` printed no AddressSanitizer, UndefinedBehaviorSanitizer or LeakSanitizer report.

### 2.2 Times (round 1, seconds)

| Entry | gcc-release | clang-release | gcc-debug | clang-debug | clang-asan |
| --- | --- | --- | --- | --- | --- |
| `worldgen-tests-conflict` | 5.6 | 2.0 | 13.0 | 13.6 | 90 |
| `worldgen-tests-conflict-worlds` | 30.1 | 16.2 | 78.8 | 84.3 | 534 |
| `worldgen-cli-tests-conflict` | 31.8 | 16.1 | 77.2 | 76.1 | 464 |
| `worldgen-tests-peoples` | 26.1 | 13.7 | 69.6 | 72.2 | 475 |
| `worldgen-tests-engine` | 17.1 | 13.2 | 41.3 | 42.9 | 334 |
| `worldgen-tests-engine-worlds` | 53.2 | 29.1 | 142.3 | 148.8 | 978 |
| `worldgen-cli-tests-engine` | 15.9 | 10.2 | 45.0 | 46.8 | 400 |
| whole `ctest` | 143 | 106 | 435 | 456 | 3180 |
| load (start–end) | 7.0–8.9 | 8.9–7.5 | 7.5–5.4 | 5.4–5.5 | 5.5–6.4, 10–13 during |

There is no `worldgen-cli-tests-peoples` entry: the CLI peoples file is retired in favour of the conflict one (P3-06 16.3 item 20, r5).

The three longest entries on each preset:

| Preset | Longest entries (s) |
| --- | --- |
| gcc-release | `engine-worlds` 53.2, `chronology` 50.5, `elevation-golden` 44.3 |
| clang-release | `chronology` 47.7, `engine-worlds` 29.1, `elevation-golden` 21.9 |
| gcc-debug | `chronology` 197.7, `engine-worlds` 142.3, `hydrology` 104.1 |
| clang-debug | `chronology` 214.1, `engine-worlds` 148.8, `elevation-golden` 107.5 |
| clang-asan | `chronology` 1696 (47% of the 3600 s timeout), `engine-worlds` 978 (27%), `elevation-golden` 856 (24%) |

**After recording.** Round 2 ran after the build of the recorded hashes:

| Preset | Result | Time | Load |
| --- | --- | --- | --- |
| `linux-gcc-debug` | 30 of 30 | 676 s | 11 to 7 |
| `linux-clang-debug` | 30 of 30 | 775 s | 7 to 6 |
| `linux-clang-asan` | 30 of 30, 0 sanitizer reports | 2043 s | 6 to 3 |

In round 2 the two release presets failed only `cli 9, 12.6`, because my first `p0-worlds.txt` was not sorted bytewise (section 6). After the fix both pass 30 of 30 in a third full run: `linux-gcc-release` 162 s and `linux-clang-release` 171 s, at load 7 to 13 alongside the asan round. `world-hashes.sh expect` gives 15 of 15 `ok` on all five presets.

Round 2 `linux-clang-asan`, at the lower load, in seconds:

| Entry | Time |
| --- | --- |
| `worldgen-tests-conflict` | 64 |
| `worldgen-tests-conflict-worlds` | 372 |
| `worldgen-cli-tests-conflict` | 425 |
| `worldgen-tests-peoples` | 358 |
| `worldgen-tests-engine` | 171 |
| `worldgen-tests-engine-worlds` | 768 |
| `worldgen-cli-tests-engine` | 195 |
| longest: `chronology` | 934 (26% of the timeout) |
| longest: `engine-worlds` | 768 |
| longest: `elevation-golden` | 479 |

The round-1 asan times were taken at load 10 to 13, with my sweep and another agent's compiles running, so they are two to three times the quiet figures. `worldgen-tests-chronology` is the longest entry on four presets, and under that load it used about half its timeout (section 9, F8).

## 3. What must not change: 0.15.0 against 0.16.0

Compared with `cmp016.py`, an extension of P3-05's `cmp015.py`:

- **Cases**: the 15 `linux-gcc-release` case files of each version.
- **Sweep worlds**: 1024 and 512 seeds 0–9, generated by the 0.15.0 binary with the `p3/05-final` data root (`out/p3-06-validation/old015/`) against the 0.16.0 sweep worlds.

| What | Result |
| --- | --- |
| `sqlite_schema` | identical in all 35 |
| Layers and geography tables (`layer`, `pass`, `biome`, `climate`, `region`, `region_border`, `region_biome`) | identical in all 35 |
| Chronology tables (`race`, `race_biome`, `culture`, `god`, `age`) | identical in all 35. `prowess_permille` and the goblin aggression (700 → 600) are race data with no column, so `race` does not move |
| `meta` | only `generator_version` (0.15.0 → 0.16.0) and `content_hash` (the data root gains `history/conflict.toml`, `prowess_permille` and the goblin value) |
| Catalogue | `event_kind` 19 → 30 rows, `event_kind_role` 33 → 54, `event_kind_value` 17 → 29 (geography and history worlds; empty and pattern worlds have none) |
| History tables | grow in every world with a claim: 1024 s0, for example, 462 → 618 events, 13 → 17 phases and 0 → 30 relation rows, with 9 civilizations in both |
| `hist-races-elf-orc-1024` | no claim at all: all 13 history tables except the catalogue equal 0.15.0's (226 events) |

**The log up to the first claim.** The rows of `event`, `event_participant`, `event_cause` and `event_value` with ids below the first `claim` are byte-identical in 22 of the 27 worlds that have a claim. In the other five, exactly one or two `quiet_era` events differ:

| World | Events that differ |
| --- | --- |
| `hist-span-2000-512` | e18 |
| 1024 s0 | e55 |
| 1024 s5 | e103 |
| 1024 s6 | e36, e56 |

In each case only the era's `end_year`, `years` and `generations` differ. In `hist-span-2000-512`, e18 is the Goblins' era from year 788: 75 years at 0.15.0 and 50 at 0.16.0, because the claim e19 in 838 ends it. This is P3-04 6.6, under which the engine lengthens an open era until its civilization's next event. With those three fields masked, all 27 prefixes are identical. So "the 0.16.0 log equals the 0.15.0 log up to the first claim" holds, except that a `quiet_era` still open at the first claim is cut short by it (F1).

`phase` differs from the first flare-up on: flare-ups are now triggered by claims, and the schedule after them changes with them. Before the first flare-up of either version, the phase rows have the same age, kind and start year in all 20 sweep pairs. Their `trigger_event` ids move with the longer log.

## 4. Conflict sweep (spec 14, 15.3, 15.5)

Every world was generated by the saved `linux-gcc-release` 0.16.0 binary with `--threads 1`, 4 to 6 processes at a time, with the shipped data. The sweep took 5,728 s wall in all, including the reference checks.

| Set | Flags | Worlds |
| --- | --- | --- |
| `default` | 1024, defaults | seeds 0–99 |
| `d512` | 512 | seeds 0–29 |
| `d2048` | 2048 | seeds 0–4 |
| `span2000`, `span12000` | `--history-span` | 1024 seeds 0–19 each |
| `sparse`, `dense` | `--history-detail` | 1024 seeds 0–19 each |
| `calm`, `turbulent` | `--event-intensity` | 1024 seeds 0–19 each |
| `civs1`, `civs4` | `--civs-per-race 1`, `4` | 1024 seeds 0–19 each |
| `elf_orc`, `elf_goblin`, `elf`, `no_elf` | `--races elf,orc`, `elf,goblin`, `elf`, `goblin,dwarf,faerie,human,orc` | 1024 seeds 0–19 each |

Each world went through two checks:

1. **`check06.py`** (`out/p3-06-validation/check06.py`), my own check, written from the spec without the C++ or the conflict reference. It covers:
   - P3-05's checks as amended (homes, spacing, capacity on every standing census row under the owner's race, ruins, causes, W9, `[history]` keys);
   - P3-06 14 items 1–7 and the first half of item 8;
   - 15.5's bounds;
   - the lead's list. Every `war_declared` has its `war` row (a = attacker, `since` = the year). No `peace` or `war` row of a pair overlaps another under half-open intervals. Every good–evil pair that coexisted has its `enmity` row from the younger civilization's `founding_event`, closed by the first ending. Symmetric rows are lower id first. Every war cites a claim of its attacker on its defender. Every `peace` or `truce` is caused by the war's last event. The shelter years hold. A replay of ownership from the log finds every siege of a last refuge (the only standing settlement of the only alive civilization of its race). Each captured settlement's census row under its new owner has P ≤ 2C. The runaway shares come from `region_control`.
2. **The reference `check-world`** (`p3_conflict_reference.py`), on all 375 worlds.

| Set | Worlds | `check06` failures | `check-world` (row differences; other failures) |
| --- | --- | --- | --- |
| default | 100 | 0 | 0; 2 (D1) |
| d512 | 30 | 0 | 0; 1 (D1) |
| d2048 | 5 | 0 | 0; 0 |
| span2000 / span12000 | 20 / 20 | 0 / 0 | 0; 0 / 3 (D1) |
| sparse / dense | 20 / 20 | 0 / 0 | 0; 0 / 2 (D1) |
| calm / turbulent | 20 / 20 | 0 / 0 | 0; 0 / 1 (D1) |
| civs1 / civs4 | 20 / 20 | 0 / 0 | 0; 0 / 1 (D1) |
| elf_orc / elf_goblin / elf / no_elf | 20 each | 0 | 0; 0 |

Totals over the 375 worlds:

- 9,579 wars, each with its `war` row; 23,069 relation rows.
- 4,665 good–evil pairs that coexisted, each with its `enmity` row.
- 2,959 captured settlements within 2C of their new owner (the largest P/C anywhere 1,599‰, `calm`).
- 3,068 sieges of a last refuge, none fallen.
- No race extinct in any world.

### 4.1 Whole worlds against 15.3 (min / median / max; spec value after the slash)

| Set | wars | battles | sacked | taken | civs ended (worlds ≥ 1) | flare-ups | simulated years | events | events per sim. year, max | share max ‰ | threshold |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1024 default (100) | 5/24/75 · spec 7/23/48 | 17/79/302 · 19/72/207 | 2/12/45 · 2/12/32 | 0/6/36 · 0/6/20 | 0/1/3 (70 of 100) · 0/1/3 (30 of 50) | 0/2.5/7 · 0/3/5 | 1122/1278/1511 · 1122/1293/1450 | 387/819/1795 · 439/815/1422 | 1.26 · 1.11 | 340 · 288 | 26/31/51 · 26/31/46 |
| 512 (30) | 6/28/48 · 6/27/48 | 26/102/196 · 26/102/196 | 1/11/29 · 1/11/29 | 0/5.5/14 · 0/4/14 | 0/0/2 · 0/0/2 | 0/4/6 · 0/3/6 | 1132/1320/1455 · 1132/1275/1455 | 240/616/836 · 240/606/836 | 0.60 · 0.58 | 636 · 636 | 20/31/41 |
| 2048 (5) | 2/12/20 · same | 10/49/72 · same | 0/5/9 · same | 0/4/9 · same | 0/1/1 · same | 0/1/2 · same | 1148/1250/1270 · same | 666/784/1008 · same | 0.79 · 0.79 | 113 · 113 | 26/31/41 |
| turbulent | 8/30.5/134 · 8/38/174 | 28/117/528 · 28/130/838 | 4/16.5/49 · 2/20/51 | 0/10/59 · 0/13/61 | 0/1.5/8 · 0/2/9 | 1/5/12 · 0/6/12 | 1207/1414/1713 · 1162/1426/1750 | 614/1262/3301 · 580/1339/3495 | 2.07 · 2.07 | 340 · 422 | 26/46/61 |
| calm | 4/12/29 · 4/14/33 | 17/36.5/114 · 11/42/114 | 0/7.5/23 · 0/8/23 | 0/3/12 · 0/3/13 | 0/0/1 · 0/0/1 | 0/0/2 · 0/0/3 | 1132/1194/1300 · 1118/1200/1313 | 335/514/946 · 335/521/946 | 0.79 · 0.79 | 288 · 326 | 20/26/31 |
| civs4 | 50/91/162 · 47/86/162 | 104/309/681 · 104/303/681 | 26/45.5/75 · 22/46/75 | 7/28/47 · 7/26/47 | 8/11/15 · 6/11/15 | 3/6/8 · 1/6/9 | 1275/1449/1578 · 1250/1450/1650 | 1441/2147/3029 · 1218/2122/3029 | 1.98 · 1.98 | 446 · 446 | 46/61/71 |
| elf,goblin | 0/5.5/15 · 0/5/17 | 0/16.5/73 · 0/16/75 | 0/3/11 · 0/3/11 | 0/0/8 · 0/0/12 | 0/0/1 · 0/0/1 | 0/3.5/6 · 0/3/6 | 600/765/900 · 600/750/900 | 128/352/550 · 108/311/732 | 0.66 · 0.86 | 255 · 306 | 20/20.5/26 |
| elf,orc | 0/0.5/3 · 0/1/3 | 0/0.5/8 · 0/2/9 | 0/0/2 · 0/0/2 | 0/0/0 · 0/0/1 | 0/0/1 · 0/0/1 | 0 · 0 | 600 · 600 | 66/158/230 · 34/148/263 | 0.38 · 0.44 | 224 · 224 | 20 |
| span 2000 | 1/7/14 · 0/5/14 | 1/16/43 · 0/14/52 | 0/2/7 · 0/2/7 | 0/0/4 · 0/0/4 | 0/1/2 · 0/1/2 | 0/0/1 · 0/0/1 | 370/393/404 · 370/390/408 | 110/244/402 · 75/224/402 | 0.99 · 1.00 | 215 · 215 | 20/20/21 |
| span 12000 | 12/49/112 · 12/53/120 | 43/166/423 · 43/180/475 | 4/29/69 · 4/28/69 | 3/17.5/40 · 2/18/68 | 0/3/7 · 0/3/10 | 0/4/10 · 0/5/10 | 2224/2793/3211 · 2224/2800/3300 | 911/1667/3171 · 815/1892/3517 | 1.06 · 1.12 | 333 · 352 | 26/46/61 |
| dense | 12/27.5/91 · 7/28/102 | 37/100/359 · 26/99/429 | 7/17.5/43 · 3/17/52 | 1/8.5/47 · 1/9/47 | 0/2/7 · 0/2/7 | 0/3/8 · 0/3/8 | 1654/1905/2303 · 1612/1886/2303 | 559/1017/2322 · 462/940/2415 | 1.09 · 1.17 | 265 · 295 | 26/36/61 |
| sparse (no spec row) | 6/17/35 | 18/56/134 | 3/9.5/20 | 0/4.5/12 | 0/0/1 | 0/1/3 | 700/730/790 | 412/661/887 | 1.22 | 297 | 21/26/31 |
| civs1 (no spec row) | 3/10/31 | 6/32.5/114 | 1/4/16 | 0/3/16 | 0/0/1 | 0/1/3 | 1112/1214/1350 | 317/548/1015 | 0.78 | 307 | 20/26/41 |
| no_elf (no spec row) | 6/12.5/30 | 21/49.5/117 | 1/7/17 | 0/5.5/16 | 0/1/3 | 0/1/3 | 962/1066/1157 | 336/584/982 | 0.88 | 333 | 21/26/41 |
| elf (no spec row) | 0 | 0 | 0 | 0 | 0 | 0 | 600 | 41/118/198 | 0.33 | 113 | 20 |

The sets of seeds 0–19 and 0–49 overlap the spec writer's prototype seeds, and most medians agree with 15.3 to within 10%. The 2048 row is identical to the spec, and 512 nearly so. The medians more than 20% away from 15.3 are these:

| Set | Statistic | Measured | Spec |
| --- | --- | --- | --- |
| 512 | taken | 5.5 | 4 |
| 512 | flare-ups | 4 | 3 |
| turbulent | taken | 10 | 13 |
| turbulent | civs ended | 1.5 | 2 |
| span 2000 | wars | 7 | 5 |
| elf,orc | wars | 0.5 | 1 |
| span 12000 | flare-ups | 4 | 5 (−20%) |

All of these are small counts, and each is a finding for the spec writer (F3).

### 4.2 The statistics of 15.3 at 1024, default settings (100 worlds; spec 50)

| Statistic | Measured | Spec |
| --- | --- | --- |
| wars per world; length | 5 / 24 / 75; 1 / 11 / 42 years (2,667 wars, none ending in its declaration year) | 7 / 23 / 48; 1 / 10 / 42 |
| how wars end (by the row's closing event) | peace 70%, truce 20%, conquest of a side 3%, peace or truce in a quiet step 3%, open at the present 1% | exhaustion 75%, aim 16%, conquest 3%, end of active phase 3%, open 2% (the cause of a peace is not stored, so exhaustion, aim, no-front and length are not distinguishable from the tables; the reference's war lines carry it) |
| peace won by attacker / defender; cessions | 1,393 / 544; 245 | — |
| attackers | goblins 81%, orcs 10%, humans 5%, dwarves 2%, elves and faeries never | 77%, 13%, 7%, 3%, never |
| claims; cessions | 6 / 29 / 91; median 2 | 9 / 28 / 68; 0 / 2 / 8 |
| civilizations ended; worlds with at least one | 0 / 1 / 3; 70 of 100 | 0 / 1 / 3; 30 of 50 |
| ended by race (of civilizations) | goblins 49 of 304 (16%), faeries 22 of 129 (17%), elves 14 of 170 (8%), dwarves 6 of 146 (4%), orcs 5 of 114 (4%), humans 3 of 155 (2%); every one conquered | 16%, 13%, 5%, 3%, 2%, 2% |
| ruins; share by sacking | 1 / 13 / 37; 91% | 2 / 14 / 36; 92% |
| flare-ups; worlds with none | 0 / 2.5 / 7; 10 of 100 | 0 / 3 / 5; 6 of 50 |
| simulated years | 1122 / 1278 / 1511 | 1122 / 1293 / 1450 |
| events; per simulated year | 387 / 819 / 1,795; 0.63 median, 1.26 max | 439 / 815 / 1,422; 0.62, 1.11 |
| summary threshold; events shown | 26 / 31 / 51; 122 / 207 / 250 | 26 / 31 / 46; 122 / 207 / 250 |
| largest land share ever; `share_over_500_years`; `share_over_800_years` | 106 / 176 / 340‰ (a goblin horde); 0 in every world; 0 | 106 / 176 / 288; 0; 0 |
| goblin–elf wars per world; worlds with none | 0 / 7 / 34; 19 of 100 | 0 / 8 / 18; 9 of 50 |
| first goblin–elf war after the goblin awakening | median 500 years; within 200 years in 37 of 100 | median 500; 19 of 50 |
| a war open within 50 years of the dwarven awakening | 62 of 100 (open at it 8, declared in the dwarves' first 25 years 43) | 30 of 50 (5, 24) |
| wars declared in the orc age | 53% (1,429 of 2,667); orcs declare within 100 years of their awakening in 53 of 100 | 55%; 30 of 50 |
| standing settlements at the present (median) | elves 10, goblins 32.5, dwarves 6, faeries 4, humans 14, orcs 5.5 | 12, 32, 7, 4, 14, 5 |
| first elven realm alive at the present | 95 of 100 | 49 of 50 |
| relation rows; per world on average | 15 / 56.5 / 167; `enmity` 11.4, `war` 26.7, `peace` 25.2 | 17 / 55 / 116; 11, 24, 23 |
| rows with `since` = `until` | none | none (0–49) |
| standing settlements, least | 47 | 53 |
| battles at a pass | 55 of 9,450 battles | — |

**Faerie and elven standing settlements with and without the last-refuge guard.** I count a world as "guard in use" when it has at least one siege, by replay, of the last standing settlement of the last alive civilization of its race. In every such world the siege held, so the guard may have decided it.

| Set | Worlds with the guard in use | Sieges of a last refuge | Elves standing, with / without | Faeries standing, with / without |
| --- | --- | --- | --- | --- |
| 1024 default | 92 of 100 | 0 / 4 / 95 per world | 1 / 10 / 44 · 4 / 10.5 / 21 (8 worlds) | 1 / 4 / 16 · 2 / 5.5 / 7 |
| 512 | 30 of 30 | 3 / 17 / 65 | 1 / 4 / 12 · — | 1 / 1 / 6 · — |
| civs4 | 18 of 20 | 0 / 6.5 / 53 | 1 / 10.5 / 25 · 16 / 18.5 / 21 | 1 / 1 / 6 · 4 / 8 / 12 |

Not one of the 3,068 sieges of a last refuge in the 375 worlds fell. The guard is rarely idle. At 1024 the besieged last refuges are mostly faerie (427 of the 768 listed, the first 30 per world) and orc (181) settlements, then human, dwarven, elven and goblin ones.

**Rows with `since` = `until`** (P3-04 r9 6.7):

| Set | Rows |
| --- | --- |
| `civs4` | 4 `war` and 1 `peace` in 5 of 20 worlds, exactly the spec writer's figure |
| `calm` | 1 `war` |
| `span12000` | 1 `war` |
| every other set | none |

No `enmity` row has `since` = `until`. Each such `war` row is a war declared in T2 whose defender was conquered by another war in the same year's T6.

### 4.3 Extremes (1024 default)

| | Seeds |
| --- | --- |
| most wars | s98 75, s82 68, s66 67 |
| fewest wars | s81 5, s74 6, s31 7 |
| most events | s98 1,795, s66 1,752, s82 1,709 |
| fewest events | s72 387, s74 434, s52 438 |

Per set (most / fewest wars):

| Set | Most wars | Fewest wars |
| --- | --- | --- |
| turbulent | s7 134 | s17 8 |
| civs4 | s6 162 | s19 50 |
| span 12000 | s7 112 | s9 12 |
| dense | s19 91 | s8 12 |
| 512 | s5 48 | s2 6 |
| elf,goblin | s1 15 | 0 in s9, s11, s13 |

**Typical ranges of 15.5 at 1024, default settings.** Outside them is a finding, not a defect:

| Range | Worlds outside | Seeds |
| --- | --- | --- |
| wars 10–40 | 20 | 31: 7, 38: 8, 59: 58, 66: 67, 98: 75, … |
| sacked 5–30 | 13 | |
| events 450–1,500 | 9 | |
| elves 4–40 standing | 14 | 1 standing in seeds 21, 54, 84, 86, 89, 90, 94; 44 in seed 38 |
| ruins ≥ 80% by sacking | 13 | |
| flare-ups 0–5 | 3 | 59, 66, 98 |
| simulated years 1,120–1,450 | 3 | the same three: up to 1,511 |
| threshold 26–46 | 2 | 82 and 98: 51 |

No world is outside "civilizations ended 0–4" or "every race alive".

### 4.4 Hard bounds of 15.5, every set

| Bound | Worst world |
| --- | --- |
| not empty (P3-05's) | 1 standing settlement at least in the single-race `elf` set; 18 at least in multi-race sets at 1024 (span 2000), 26 at 512, 81 at 2048 |
| races | `races_alive` equals the present races in every world |
| runaway | `share_over_800_years` 0 everywhere; largest share ever 636‰ (512 seed 12), `share_over_500_years` at most 3,213 (512 seed 4) |
| events | at most 2.07 per simulated year (turbulent) |
| phases | at most 383‰ simulated (dense) |
| capacity | P ≤ 2C on every standing census row of every world |
| sizes | history tables at most 409,600 bytes (turbulent), well under 5 MB |

## 5. Purity and flags

Seed 0 at 1024 (`out/p3-06-validation/purity/`):

| Run | Result |
| --- | --- |
| `--threads 1`, `2`, `6` | identical bytes (`ebb87235…`) |
| critic worlds (`--threads 2`) against sweep worlds (`--threads 1`), seeds 0, 1, 2, 3, 7, 11, 23, 42 at 1024 and 5 at 512 | identical bytes |
| `--until history` | identical to the default |
| `--until chronology` | all 13 history tables have 0 rows (catalogue included); `stages` ends at `chronology` |
| `--monster-pressure high`, `--divine-activity meddling` | only `meta` (`param.monster_pressure`, `param.divine_activity`) |
| `--history-span 2000`, `12000`; `--races` (four lists) | `meta`, `age`, `race` and the history tables; no layer or geography table |
| `--history-detail`, `--event-intensity`, `--civs-per-race` | `meta` and the history tables (`phase` included) only |

`generate --help` still says "the history stage [uses] the span and the detail; the others take effect with later history subsystems". Since 0.15.0, `--civs-per-race` and `--event-intensity` also act (F2).

Resume (P3-04 Y2 with conflict) is a library test (`worldgen-tests-conflict-worlds`, passing). The CLI has no resume flag.

## 6. Recorded hashes

The values are the `file` lines of the five presets' identical `hash --detail` files, each checked equal to `sha256sum` of its world. The library values come from the failures printed by `worldgen-tests`, identical on all five presets, each matched to its case by its old value. Each value changed for a reason section 3 shows:

- `meta.generator_version`, in every world;
- `content_hash`, in CLI worlds, from the new data;
- for the geography and history cases, also the 30-kind catalogue and the conflict history.

Commit `ebb7746`. `worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase 3fb26d526139d2e2ae28ec82a145ef0b00596ce6d919c0f1ccf4a6d9e415c37e
empty-seed-0 5204c1d407d6df769fa19ab763ae915786881b11dac9dc7a21ef16e9dedcfa8b
empty-seed-0-512 5ac0c8d0947904ae08e115e0c90c31a822379a2c17674cc393650411523bd992
empty-seed-1 cd68a5f84ec52900b4a3cc94ffeeac7958ccd62ace74db2959fc70068c9f2780
empty-seed-42 d394fecf19f71ecf1ecaa113e703ba123a5f501833493afbc19e5c0667e01696
empty-seed-max f858bd8b27cc8b20ee09f6a4ab3194fd393952ec219c106f973336b2ce18c689
pattern-seed-0 64681f0282cf59957cb5c27fadcd2aed2ff114a0bbb41d528283325b1a68f3b5
pattern-seed-42 6e088182e59326a9c87874c4a6c51c36670a6ccb3b0dc71ad274945650f3fa23
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 13d8153dfe7b6bf62735dce5260c496c36d4301df153453a92a7f875010d88f7
geo-seed-0-512 645ac6a3e715d78bd70651706c1dceb219cb4ce94fea2ef493286f8cb58d7789
geo-seed-1-1024 afffc528570065b8165326389938769fbda8054159128d8e48735806b8277d14
geo-seed-42-512 e0052ed95c87107fa04071bc9b4b38f3a26a3354d2370fb793b07199e9578202
```

`worldgen/tests/golden/p3-worlds.txt`:

```
hist-dense-1024 0863789168a0c61fe441527a73be4cf68af06639611c4b2b723dc5357ed2bbed
hist-races-elf-orc-1024 defdf99858ec4d4abfa5c3d55bc9fc60ef690d1f1ceb803a00cb7db5ed076a1c
hist-span-2000-512 643b82748aff38ee77d6583de1760f6fd60dbe29ca950135b8879185abbd292a
```

The seven geography and history cases pass `check-world` (0 failures each) and `check06`:

| Case | events | civilizations | standing | wars |
| --- | --- | --- | --- | --- |
| `geo-phrase-512` | 495 | 7 | 34 | 24 |
| `geo-seed-0-512` | 240 | 6 | 28 | 10 |
| `geo-seed-1-1024` | 1043 | 12 | 94 | 36 |
| `geo-seed-42-512` | 845 | 9 | 38 | 43 |
| `hist-dense-1024` | 828 | 10 | 85 | 20 |
| `hist-races-elf-orc-1024` | 226 | 3 | 44 | 0 |
| `hist-span-2000-512` | 242 | 7 | 32 | 14 |

`GOLDEN_CASES` in `worldgen/tests/db_determinism_test.cpp`:

| Case | 0.15.0 | 0.16.0 |
| --- | --- | --- |
| `empty-phrase` | `43cadb81…` | `56b62b46d61fcf3f37a0fa0c696e863e511c598f112ca68172cfac212e7bca3c` |
| `empty-seed-0` | `101d523a…` | `3573d3e9d4da02c516625465702cc8ebbaaef2611fa2f32fca93f8ea1e9c0755` |
| `empty-seed-1` | `539c5abe…` | `a28c3e7e91a1216dbec8be8a3ad623d9b7d585744d229874969528e364adf3fc` |
| `empty-seed-42` | `50200e2c…` | `1bae70fb5864f7bbb28d1138d7896c7aeb83ce86b3cbadc162c45418d77d9e7c` |
| `empty-seed-max` | `3f0a5d71…` | `83d3ff56d4f65cde71bca0d32d5d2fbd95466cca76226ee50debd1dd8eb83a00` |

`worldgen/tests/stages_pipeline_test.cpp`:

- The empty world of seed 0 is now `3573d3e9…` (was `101d523a…`), equal to `GOLDEN_CASES` `empty-seed-0` as before.
- The test pattern of seed 0 is now `d0bf691163bef3e79b4b4068914b01ffdd4257c2c1dbbdb2499c5fdca0e98df4` (was `728b3669…`).
- The comments beside them and the `GOLDEN_CASES` header now say 0.16.0.

No other file outside `docs/reviews/` holds a 0.15.0 world hash (searched for each old value). No per-stage golden file was touched.

My first version of the three golden files was sorted with the locale's collation, and `cli 9, 12.6` rejected `p0-worlds.txt` as unsorted. They are now sorted bytewise (`LC_ALL=C`), as before. The commit was amended before the push.

**README** (`generate --seed-phrase "The Unmapped"`, default size 1024): `hash` `9e1c254b9a02fa14e65e812823e6376df000fa817e9a22d25d984bdd6bc038a8`. `[history]`:

- 6/10/5/1 phases, 1,362 simulated years (227 per mille), 1,000 events;
- 30 wars, 20 sacked, 3 civilizations conquered;
- 12 civilizations (9 alive), 106 settlements (86 standing), 29 regions held, population 200,822.

The full `report` text is `~/.cache/the-unmapped/out/p3-06-validation/readme-report.txt`.

## 7. Extension

`linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON`, in its own tree (`build/p3-06-val/linux-gcc-release-gdext`):

- **Build.** 579 s from cold (godot-cpp included), 0 warnings, at load 8 to 12 alongside round 2.
- **Headless check.** `ctest -N` lists `gdext-headless-check`, and it passes in 10 s. Its real world (`--seed 42 --map-size 512`) has 19 layers and `stages` ending `,history`. It has 845 events and is byte-identical to the CI case `geo-seed-42-512` (`e0052ed9…`).
- **Case files.** `world-hashes.sh generate` under the label `linux-gcc-release-gdext` gives 15 case files identical to plain `linux-gcc-release` ("all 15 cases identical across 2 presets"). `expect` gives 15 of 15 `ok`.

## 8. Old files and the empty world

- The 0.16.0 binary reads a 0.15.0 world (1024 seed 1, schema 7) without complaint. `report` prints a text byte-identical to the 0.15.0 binary's report of it, `[history]` included. No key is added, because P3-06 adds no `[history]` key: 15.4's keys belong to P3-07's `[stats]`, which is not implemented yet. `hash --detail` is identical. `render --composite regions` works.
- The empty world (`empty-seed-0`) is unchanged: 34 pages, 139264 bytes, schema text 5839 bytes.

## 9. Defects and findings

**D1 (test reference, not the stage; for the test writer).** `p3_conflict_reference.py check-world` fails its "P3-05 inv 6 … kind … built" check in 10 of 375 sweep worlds. In each of them it also reports no row difference, so the C++ and the reference agree on every table.

- Reproduce (generator 0.16.0, shipped data): `worldgen-cli generate --seed 33 --map-size 1024 --out s33.db && python3 -B worldgen/tests/reference/p3_conflict_reference.py check-world s33.db --data game/data`
- Actual: `FAIL s33.db P3-05 inv 6 s61 kind 4 built: got False, want True`, then 1 failure.
- Expected: P3-05 14 item 6 ("a ladder kind is a kind the race builds"), read with P3-06 13. Under P3-06 13 a captured settlement keeps its kind and only dwindles later to its new owner's ladder.
- Root cause: in `check_invariants`, `built` is the kinds of the founder's race plus those of the census row's owner. s61 was founded as a goblin colony, became a town and was captured by humans in 5675. It became a city in 5781 and was sacked in 5950. Its ruin row has owner 0, so `built` is the goblin kinds alone, which have no city.
- The other nine are the same pattern: a ruin of a settlement grown under a second owner, or a settlement captured twice. The worlds are `default` s33 and s96, `d512` s15, `dense` s1 and s7, `span12000` s7, s11 and s15, `civs4` s7, and `turbulent` s18.
- Fix: count every race that has owned the settlement (every census row's owner, and the last owner for a ruin), or read the invariant as "a kind built by a race that held it". The spec writer may want to say this in P3-05 14 item 6. Not blocking: `check-world` is not part of `ctest`, and the C++ property tests do not hit it on their fixtures.

**F1 (spec text).** The brief's "the 0.16.0 log equals the 0.15.0 log up to the first claim" holds except for a `quiet_era` still open at the first claim. Its `end_year`, `years` and `generations` are shorter, because the claim is its civilization's next event (P3-04 6.6). This happened in 5 of 27 worlds (section 3).

**F2 (CLI help, for the engineer, low).** `generate --help` says the history stage uses only the span and the detail and that "the others take effect with later history subsystems". Since 0.15.0, `--civs-per-race` and `--event-intensity` act as well. The same text was in 0.15.0.

**F3 (spec writer, typical ranges).** The medians more than 20% away from 15.3 are in 4.1. At 1024 the 100-world sweep goes past every maximum of the spec's 50 worlds:

| Statistic | Measured | Spec |
| --- | --- | --- |
| wars | 75 | 48 |
| battles | 302 | 207 |
| sacked | 45 | 32 |
| taken | 36 | 20 |
| events | 1,795 | 1,422 |
| flare-ups | 7 | 5 |
| simulated years | 1,511 | 1,450 |
| events per simulated year | 1.26 | 1.11 |
| largest share | 340‰ | 288‰ |
| threshold | 51 | 46 |
| goblin–elf wars per world | 34 | 18 |

These come mostly from seeds 50–99, which the prototype did not run. The typical-range outliers are listed in 4.3. In seven worlds the elves are reduced to a single settlement.

**F4 (critic).** A captured settlement can go back within the same year. In span 12000 seed 3, year 11253, the defender c10 took Aleckstead (s139) in a battle of the attacker c8's war. The war then ended in the same T6 with c8 winning on score, and the goal region, Aleckstead with it, was ceded back. The census shows no change of owner. This follows the spec (7.7 cession of a goal L still holds), but it reads oddly: "took … ; sued for peace; ceded".

**F5 (critic, P3-06 question 4).** The last-refuge guard is used in 92 of 100 default worlds, at a median of 4 and up to 95 held sieges of a last refuge per world. A war against a last refuge can therefore never reach its aim. Seed 42's faeries (one civilization, then two settlements) are besieged 16 times, and every siege holds.

**F6 (critic, P3-07).** With four civilizations per race, the summary threshold is 46 / 61 / 71. Claims (45), truces (50), moved capitals (50) and cessions (55) drop out of the summary in most four-civilization worlds. At default settings it is 26 / 31 / 51.

**F7 (record).** Rows with `since` = `until` were seen outside `--civs-per-race 4`: one `war` row in a `calm` world and one in a `span12000` world (4.2). Each is legal under P3-04 r9.

**F8 (build engineer).** Under load 10 to 13, `linux-clang-asan`'s `worldgen-tests-chronology` took 1,696 s, 47% of the 3,600 s timeout. On a quiet machine it was about 6 minutes at 0.15.0. If CI hosts slow down further, this is the first entry to hit the limit.

## 10. Timing

The machine was quiet: the other agents' work had ended and only my own runs were going, at load 2 to 3. Seed 0, default threads (6), `linux-gcc-release`, wall seconds. The 0.15.0 binary and its data root alternated with the 0.16.0 binary, three runs each (`timing/gen-times.txt`):

| Size | 0.15.0 runs | 0.16.0 runs | Change (medians) |
| --- | --- | --- | --- |
| 512 | 2.607, 2.607, 2.620 | 2.617, 2.627, 2.644 | +0.3% |
| 1024 | 9.066, 9.125, 9.169 | 9.123, 9.113, 9.113 | −0.1% |
| 2048 | 36.95, 37.01, 36.91 | 36.75, 36.80, 36.94 | −0.4% |

The difference is within run-to-run noise: conflict adds well under a second to a run dominated by geography.

**The history stage alone.** `UNMAPPED_PEOPLES_BENCH=<worlds> worldgen-tests -ts=peoples-bench --no-skip`, release, two runs, on worlds stored with `--until chronology`.

What it measures: `history_inputs_from_world` is done before the clock starts. The clock then covers one call of `worldgen::stages::history_stage` with the shipped content, which is the full shipped stage, peoples and conflict, as `generate` runs it. It writes no rows. Its event counts equal those of the full worlds: 618 for 1024 seed 0, 1043 for seed 1.

| World | Time (ms) | Events |
| --- | --- | --- |
| 1024 seed 0 | 79, 79 | 618 |
| 1024 seed 1 | 110, 112 | 1043 |
| 1024 seed 2 | 100, 102 | 749 |
| 1024 seed 3 | 126, 128 | 991 |
| 1024 seed 4 | 119, 120 | 847 |
| 512 seed 0 | 27, 27 | 240 |
| 2048 seed 0 | 174, 170 | 668 |

Budget (16.6): 4 s at 1024, 1 s at 512, 16 s at 2048, so the stage uses at most 3% of it. At 0.15.0 the peoples-only stage took 52 to 82 ms at 1024, which puts conflict's cost at about 30 to 45 ms a world.

## 11. Material for the world critic

`~/.cache/the-unmapped/out/p3-06-validation/critic/`, made by `make.sh` in the same folder with the 0.16.0 binary. Each world has:

- `<name>.db`, `<name>.report.txt`, `<name>.regions.png` and `<name>.map.png`;
- `<name>.chronicle.txt`. This has every event as `<year> e<id> [<kind>, salience <n>] <sentence> (causes: …)`, then the end state per civilization (name, race, home, founded, alive or how it ended, capital, standing and ruined settlements, population, regions). After that come one line per war (years, sides, goal region, claim, battles, sacked, taken, outcome) and the relation rows;
- `<name>.summary.txt`: the events at or above the world's summary threshold (P3-07 7.4: the smallest t ≥ 20 with at most 250 events of salience ≥ t), with the same end state, wars and relations.

The chronicles are written by `chronicle06.py`, P3-05's `chronicle.py` extended for kinds 20–30 and the war and relation lists.

| World | Events | Wars | Threshold (shown) |
| --- | --- | --- | --- |
| `seed42-1024` | 1030 | 38 | 46 (155) |
| `seed0-1024` | 618 | 12 | 26 (200) |
| `seed1-1024` | 1043 | 36 | 41 (244) |
| `seed2-1024` | 749 | 17 | 31 (175) |
| `seed3-1024` | 991 | 27 | 31 (230) |
| `seed7-1024` | 795 | 26 | 31 (212) |
| `seed11-1024` | 815 | 20 | 31 (207) |
| `seed23-1024` | 617 | 16 | 26 (208) |
| `seed42-1024-turbulent` | 1666 | 59 | 46 (218) |
| `seed42-1024-civs4` | 2090 | 85 | 56 (248) |
| `seed42-512` | 845 | 43 | 31 (246) |
| `seed5-512` | 836 | 48 | 41 (239) |
| `seed42-2048` | 603 | 7 | 26 (213) |

## 12. Files

Everything is under `~/.cache/the-unmapped/out/p3-06-validation/`:

- **Scripts:** `check06.py`, the independent check; `aggregate06.py`; `cmp016.py`; `cmp152.py`, seed 42 against 15.2; `chronicle06.py`; `gen-one.sh`, `sweep-job.sh`, `run-sweep.sh`, `old-job.sh`, `purity.sh`, `timing.sh`, `ctest_run.sh`, `pipeline1.sh` and `round2.sh`.
- **Sweep:** worlds, hash, report, check and reference output under `sweep/<set>/`, with summaries in `sweep/aggregate.txt` and `sweep/ref-results.txt`.
- **Comparisons:** the 0.15.0 worlds in `old015/`; comparison results in `cmp/`; `purity/`; `oldfile/`.
- **Logs** in `logs/`; `timing/`; `critic/`; `readme-report.txt`.
- **Binaries:** the saved 0.16.0 binary in `bin-016/`; the 0.15.0 data root in `data-015/`.
- **Case files:** `~/.cache/the-unmapped/out/p3-06-val/world-hashes/`.
- **Build trees:** `~/.cache/the-unmapped/build/p3-06-val/`.
