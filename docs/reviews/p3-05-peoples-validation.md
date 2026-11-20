# Validation: P3-05 peoples (generator 0.15.0, schema 7)

Validation specialist, 2026-11-19. Branch `p3/05-validation`, cut from `origin/p3/05-int` at `044127e` (peoples subsystem, the test writer's tests and reference, the audit report and lint rule `history-held-reference`, the audit hardening A1 to A3). Merged with `origin/p3/04-final` (`eaadb1e`, main plus validated P3-04, clean) and then `origin/p3/05-spec-fixes` (`8d88958`, docs only). The second merge conflicted in `docs/specs/p3-05-peoples.md` and `docs/specs/p3-06-expansion-conflict.md`; both were resolved to the `05-spec-fixes` side, and every file under `docs/specs/` now equals `8d88958`. After the merges the per-stage golden vectors are the `p3/05-int` ones (`p3-01-data.txt`, `p3-03-chronology.txt`, `p3-04-engine.txt`, `p3-05-peoples.txt` changed, the others identical on both sides). The world-hash files and inline hashes came in at their 0.14.0 values from `04-final` and are re-recorded here (section 6). Baseline for "what must not change": the frozen 0.14.0 `linux-gcc-release` binary of the P3-04 validation (`out/p3-04-validation/bin-014/`), its 15 case files (`out/p3-04-val/world-hashes/`) and its sweep worlds. Spec: `docs/specs/p3-05-peoples.md` r4 (sections 4 to 11, 14, 15.2, 15.3, 15.5, 15.6, 16.0, 16.0a, 16.4).

Machine: 6-core Linux desktop. Quiet (load 1 to 2) for the release `ctest` round 1, the timing runs and the benchmark. **Shared with other agents' P3-06 work** (a clang-debug `ctest`, a six-process Python sweep, builds) for the debug and asan rounds and most of the sweep: load 12 to 28 then. Each time below is marked with its load. Windows MSVC is checked by CI.

**Verdict: pass.** No defect against the stage, the tests or the reference. Several findings on the spec's typical ranges and one behaviour the spec says never happens are for the spec writer and the lead (section 9).

- **Builds.** All five Linux presets build with warnings as errors, 0 warnings in any log.
- **Tests before recording.** On all five presets, including `linux-clang-asan` in full (4208 s, 0 sanitizer reports), the only failures are the seven test cases that compare with recorded world-file hashes, the same seven as at P3-03 and P3-04. The 40 printed `values:` lines are identical on all five presets (digest `134dfc2c…`). Every per-stage golden vector passes unchanged, `p3-05-peoples.txt` included.
- **What must not change.** I compared 0.14.0 with 0.15.0 on the 15 cases and 73 sweep worlds. Every layer and every geography table is identical. Of the chronology tables only `race_biome` changed: 45 to 48 rows, exactly the goblin and faerie weights of spec 12.2. `age`, `race` (awakening years), `god` and `culture` are identical. `phase` is identical except for `trigger_event`, whose ids moved because the log is longer; each trigger's kind and year is unchanged. `meta` changed only in `generator_version` and `content_hash` (section 3).
- **Peoples sweep.** 355 worlds (spec 15.6 sets as sized by the lead). Every hard bound of 15.5 and every invariant of 14 holds, checked by an independent recomputation written for this validation and by the test writer's `check-world` on every world: 355 of 355 pass both (section 4).
- **Purity.** `--threads` 1, 2, 5, 6 give identical bytes. `--until history` is byte-identical to the default. `--until chronology` leaves all 13 history tables empty. `--event-intensity` and `--civs-per-race` now change the history tables and nothing in geography or chronology (section 5).
- **Seed 42 (15.2).** `[history]`, event counts, the first 60 sentences, the notable later events, the end state, the kinds, the largest settlements and the summary counts (T = 26, n(26) = 116) all equal the spec text.
- **Old files.** A 0.14.0 world (schema 7) is read by `report`, `hash --detail` and `render`. `hash --detail` is unchanged; `report` adds the 19 new `[history]` keys, computed from the stub tables. The empty world is unchanged: 34 pages, 139264 bytes, schema text 5839 bytes. The schema-6 refusal message is unchanged (section 8).
- **Cross-preset.** All 15 `world-hashes.sh` cases are identical across the five presets, and the extension build gives the same 15 files. Recorded `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, the five `GOLDEN_CASES` and the two inline hashes (commit `1fd3929`). After recording, `ctest` passes 28 of 28 on all five presets, and `expect` gives 15 of 15 `ok` on each and on the extension build.
- **`relation`** is empty in every world (393 checked, the `--civs-per-race 4` worlds included).
- **Lint:** self-test 17 snippet files, 0 failures; scan 157 C++ and 13 CMake files, 0 findings.
- **Timing.** 0.15.0 `generate` costs the same as 0.14.0 to within 0.3% at all three sizes. The history stage alone takes 52 to 82 ms at 1024, 27 ms at 512 and 163 ms at 2048 (budget 16.4: 2 s, 0.5 s, 8 s).

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build, five Linux presets | pass, 0 warnings (93 to 179 s each with ccache; load 8 to 14) |
| `ctest` round 1, five presets | 26 of 28 entries pass on every preset; the failures are only world-hash comparisons (2.1) |
| 0.14.0 against 0.15.0: 15 cases, 73 sweep worlds | 88 of 88 as expected (section 3) |
| Peoples sweep, 355 worlds | 0 failures in the independent check; `check-world` 355 of 355 (section 4) |
| Purity and flags (section 5) | as the spec says |
| Seed 42 against 15.2 | equal in every value checked |
| `world-hashes.sh generate` on five presets, then `compare` | "all 15 cases identical across 5 presets" |
| `check-world` and the independent check on the 7 history cases | 7 of 7 pass both |
| Recording (section 6) | done from the compared output |
| `ctest` round 2, five presets | 28 of 28 on every preset (2.2) |
| `world-hashes.sh expect`, five presets and `linux-gcc-release-gdext` | 15 of 15 `ok` on each, exit 0 |
| Extension, `linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON` | build 841 s; `gdext-headless-check` passes (27 s); same 15 case files (section 7) |
| Lint | pass, 0 findings |
| Timing and benchmark (section 10) | 0.15.0 = 0.14.0; stage far inside budget |

## 2. Tests

### 2.1 Failing set before recording

Identical on all five presets:

| Test case | Entry | Compared with |
| --- | --- | --- |
| `cli 9, 12.6: golden world hashes from p0-worlds.txt` | `worldgen-cli-tests-other` | `p0-worlds.txt`, 8 cases |
| `cli P0-03 7.6, amendment 17: with the required kind worldgen, a nonexistent data root is an input error` | the same | `p0-worlds.txt` `empty-seed-0` |
| `cli P1-00 10.1: --until none writes the P0 empty world, byte for byte` | the same | `p0-worlds.txt` |
| `cli P1-00 10.1: --threads accepts 1 to 256 in decimal without leading zeros` | the same | `p0-worlds.txt` `empty-seed-0` |
| `cli P1-00 10.1, D10, X3: --threads never changes the output (test-pattern world at 1, 2 and 5 threads)` | the same | `p0-worlds.txt` pattern cases |
| `golden: empty-world hashes match the recorded values (12.4)` | `worldgen-tests-other` | `GOLDEN_CASES`, 5 values |
| `generate P1-00 10.1: --until none and every thread count give the P0 empty world` | the same | the two inline hashes |

All 40 `values:` lines are the same on the five presets (sorted digest `134dfc2c132ed651…`). The asan run printed no AddressSanitizer, LeakSanitizer or UBSan report.

### 2.2 Times

Full `ctest`, preset jobs (4). Round 1 before recording; round 2 at the recorded tip.

| Preset | round 1 | round 2 |
| --- | --- | --- |
| `linux-gcc-release` | 97 s, 26/28, load 7–9 (quiet apart from one niced process) | 92 s, 28/28, load 5–6 |
| `linux-clang-release` | 105 s, 26/28, load 7 | 97 s, 28/28, load 6 |
| `linux-gcc-debug` | 830 s, 26/28, load 6–16 (alongside the sweep) | 1171 s, 28/28, load 6–16 (alongside the extension build) |
| `linux-clang-debug` | 853 s, 26/28, load 14–16 (alongside the sweep) | 741 s, 28/28, load 14–16 |
| `linux-clang-asan` | 4208 s, 26/28, load 12–28 (alongside the sweep and other agents' work), 0 sanitizer reports | 2128 s, 28/28, load 1–4 (quiet; rerun in full after a usage pause cut off the first attempt at entry 12), 0 sanitizer reports |

Peoples and engine entries, round 1 (seconds):

| Preset | `worldgen-tests-peoples` | `worldgen-cli-tests-peoples` | `worldgen-tests-engine-worlds` | `worldgen-tests-engine` | `worldgen-cli-tests-engine` | `worldgen-tests-chronology` |
| --- | --- | --- | --- | --- | --- | --- |
| `linux-gcc-release` | 14.2 | 17.8 | **29.5** | 15.7 | 8.8 | 43.8 |
| `linux-clang-release` | 15.5 | 17.7 | 32.4 | 15.2 | 9.7 | 47.6 |
| `linux-gcc-debug` | 137.9 | 149.9 | 274.6 | 78.6 | 90.7 | 396.3 |
| `linux-clang-debug` | 170.8 | 189.3 | 312.2 | 79.1 | 91.8 | 418.9 |
| `linux-clang-asan` | 646.2 | 611.8 | 1554.7 | 501.4 | 564.3 | 2199.3 |

On the quiet machine `worldgen-tests-engine-worlds` takes 29.5 s (GCC) and 32.4 s (Clang), against 47 s under load before. Every peoples and engine entry is at or under the 30 s release target except `engine-worlds` on Clang (32 s). `worldgen-tests-chronology` is the longest entry everywhere (44 s on release). Under asan it took 2199 s at load 12 to 28 in round 1, and 984 s quiet in round 2 (27% of its 3600 s timeout). Round 2 quiet, the other entries: release `worldgen-tests-peoples` 13.9 / 14.7 s (GCC / Clang), `worldgen-cli-tests-peoples` 14.7 / 15.1 s, `worldgen-tests-engine-worlds` 27.8 / 30.3 s; asan `worldgen-tests-peoples` 512 s, `worldgen-cli-tests-peoples` 472 s, `worldgen-tests-engine-worlds` 923 s.

## 3. What must not change: 0.14.0 against 0.15.0

Script `out/p3-05-validation/cmp015.py`. Both sides are schema 7. For each pair it requires:

- identical `sqlite_schema`;
- every table byte-identical row for row (layers included, through table `layer`), except `meta`, the five chronology tables, the three catalogue tables and the ten history tables;
- in `meta`, changes only to `generator_version` and `content_hash`.

It then reports, row by row, what changed in the chronology tables.

| Set | Result |
| --- | --- |
| `empty-*` (6), `pattern-*` (2) | all tables identical; `meta` changed `content_hash`, `generator_version` |
| `geo-*` (4), `hist-*` (3) | 11 tables identical (layer, pass, biome, climate, region, region_border, region_biome, culture, god, age, race); `race_biome` 45 to 48 rows; catalogue 5 to 19 kinds (roles 7 to 33, values 4 to 17); history tables rewritten |
| Sweep: 1024 seeds 0–49, 512 seeds 0–19, 2048 seeds 0–2 (0.14.0 side: the P3-04 sweep worlds) | 73 of 73 as the cases |

The `race_biome` change on every world is: 5 faerie rows removed (temperate forest 200, marsh 400, highland moor 100, wet woodland 600, fen 600); 8 rows added (goblin temperate forest −200 and wet woodland −300; faerie temperate forest 100, marsh 600, highland moor 300, heath 100, wet woodland 800, fen 800). That is exactly spec 12.2 items 2 and 3.

**Chronology does not move.** `age` (start and end years, names), `race` (`awakening_year`, `creator_god`, `present`), `god` and `culture` are identical in all 88 worlds. P3-03's awakening order does not read biome weights, so the weight change does not reach it.

`phase` is identical in `age`, `kind`, `start_year` and `end_year`. Only `trigger_event` differs: it is an event id, and the peoples log puts more events before each awakening (seed 0 at 512: triggers 1, 13, 35, 49, 72, 99 against 1, 5, 9, …). The kind and year of each trigger event, and the race and god of every awakening, are identical (`base/phase-check.txt` for the cases, the same check over the 73 sweep worlds: 0 differ).

`content_hash` changes because the data roots gain `history/peoples.toml` and change `report.toml` and the six race files. Results: `out/p3-05-validation/base/cases-014-vs-015.txt`, `base/case-*.json`, `base/sweep-014-vs-015.jsonl`.

## 4. Peoples sweep (spec 14, 15.3, 15.5)

Binary: `linux-gcc-release` at the merged tip, frozen as `out/p3-05-validation/bin-015/worldgen-cli`. Each world is generated in full, single-threaded, four at a time, 18:32 to 20:23 (1 h 51 min) at load 12 to 28. Under that load a world took about 60 s at 1024 and 225 s at 2048.

| Set | Seeds | Settings |
| --- | --- | --- |
| default-1024 | 0–99 | defaults |
| default-512 | 0–29 | defaults |
| default-2048 | 0–4 | defaults |
| span2000, span12000, sparse, dense, calm, turbulent, civs1, civs4 | 0–19 each | `--history-span 2000` / `12000`, `--history-detail sparse` / `dense`, `--event-intensity calm` / `turbulent`, `--civs-per-race 1` / `4` |
| elf_orc, elf, no_elf | 0–19 each | `--races elf,orc`, `--races elf` (the single race, as 15.3's row), `--races goblin,dwarf,faerie,human,orc` |

Every world passed. `generate` exited 0 with empty stderr, and each world passed both of the following:

1. **Independent check**, `out/p3-05-validation/check05.py`. I wrote it from spec sections 4, 5, 7 to 11, 14 and 15.5, without the C++ and without `p3_peoples_reference.py`. It reuses only the core key hash and `chance` of `core_reference.py` and `p3_names_reference.py` (for the 7.1 tie draw and the 7.2 `auto` draw) and `p0_world_check.decode_blob` (for layers). It computes σ, q and C of every (region, race) from the stored region rows, region biome counts, biome water flags, stored `race_biome` and the race TOML. From those it checks:
   - **Invariant 1.** The homes of every awakening are recomputed by 7.1 and 7.2: order, eligibility, capacity floor, the `auto` draw, `regions_per_civ`, `home_min_hops` over open borders, the fallbacks. The ownership before the awakening comes from `region_control`. The recomputed homes are required to equal the `civ_founded` regions, and each capital is required to be founded with 300 people.
   - **Invariant 2.** Founding events are of the right kind; each cell is in its region and on water 0. No shared cell and no pair closer than 32 cells while the earlier one had not faded, except relaxed awakening sites (one case, 512 seed 4).
   - **Invariant 3.** P ≤ 2C at the end and on every standing census row. Settlements above their kind cap are counted, and each one was traced to refugees (below).
   - **Invariants 4 and 5.** Open `region_control` rows against standing settlements; capitals; ended civilizations.
   - **Invariant 6.** Each settlement's kind is replayed from its founding kind (fortress rule of 8.4, `ladder`) through its `became_*` and `settlement_dwindled` events and must reach the stored kind.
   - **Invariant 7.** Ruined and faded rows.
   - **Invariant 8.** Cause rules for kinds 3 and 6 to 19, and no pressure kinds in quiet phases.
   - **W9.** At most 3 peoples events per charged civilization per quiet step.
   - **15.5 hard bounds.** Not empty; events per simulated year; the runaway share by P3-07 7.5 from `region_control` over non-islet regions; history tables under 5 MB (by `dbstat`).
   - **`[history]`.** The 19 new `[history]` keys of `report` must equal the values computed from the tables, in order.

   I proved the check can fail by running it on seed 42 with an edited data root (`people_per_kcell` 900, `home_min_capacity` 3000). It reported 60 failures: homes and capacity.
2. **Test writer's reference**, `p3_peoples_reference.py check-world` (recomputes all chronology and history tables and compares them, invariants, `[history]`): **355 of 355**, 0 failures. It ran on every world, not a subset: 10 to 25 s per world.

**Table `relation` is empty in every world** (P3-05 opens no relation, 13.1 item 6). Checked in all 355 sweep worlds, the 8 critic worlds and the 15 cases of `linux-gcc-release` and of the extension build: 393 worlds, 0 rows (`sweep/relation-check.txt`). That includes the 20 `--civs-per-race 4` worlds and critic seed 42 `civs4`, the most crowded histories: 22 to 28 civilizations and 0 to 6 secessions each.

Under `--civs-per-race 4` the invariant checks hold as everywhere. Each awakening founds at most 4 civilizations, and the homes recomputed by 7.2 equal the stored ones. Of 120 awakenings, 90 founded four, 13 three, 13 two and 4 one (spec 15.3: 226 of 300 four). The later races find no free land three hops from their first home. With `auto` at 1024, 208 of 600 awakenings founded two (35%; spec 34%).

### 4.1 Whole worlds (min / median / max)

| Set | n | civilizations | standing | ruined | faded | population | regions held | events | events per sim. year | threshold | secessions | ended |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1024 seeds 0–99 | 100 | 7 / 10 / 15 | 56 / 88 / 136 | 0 / 1 / 4 | 0 / 0 / 2 | 132,928 / 225,176 / 364,648 | 20 / 30 / 42 (of 42 / 49 / 58) | 287 / 551 / 966 | 0.25 / 0.48 / 0.81 | 20 / 26 / 26 | 0 / 2 / 7 | 0 |
| spec 15.3 (0–49) | 50 | 7 / 10 / 15 | 56 / 90 / 128 | 0 / 1 / 4 | | 132,928 / 225,212 / 340,800 | 22 / 30 / 39 | 310 / 564 / 966 | 0.48 | 20 / 26 / 26 | 0 / 2 / 6 | 0 |
| 512 seeds 0–29 | 30 | 6 / 7 / 9 | 29 / 42 / 49 | 0 / 0 / 2 | 0 / 0 / 1 | 42,003 / 66,173 / 91,387 | 9 / 11 / 13 | 154 / 268 / 354 | 0.13 / 0.22 / 0.31 | 20 / 20 / 21 | 0 / 0 / 1 | 0 |
| 2048 seeds 0–4 | 5 | 8 / 12 / 14 | 98 / 130 / 142 | 0 / 1 / 6 | 0 / 0 / 1 | 425,008 / 475,238 / 653,356 | 41 / 57 / 66 (of 194–202) | 510 / 672 / 803 | 0.43 / 0.60 / 0.67 | 26 | 0 / 1 / 5 | 0 |
| calm | 20 | 7 / 9 / 13 | 67 / 84 / 128 | 0 / 0 / 2 | 0 | 159,171 / 238,136 / 316,695 | 22 / 28 / 39 | 291 / 389 / 603 | 0.33 | 20 / 21 / 26 | 0 / 0.5 / 4 | 0 |
| turbulent | 20 | 8 / 12 / 19 | 72 / 89 / 131 | 0 / 4.5 / 10 | 0 / 0 / 1 | 129,523 / 212,700 / 293,800 | 22 / 30.5 / 38 | 531 / 776 / 1,432 | 0.68 | 26 / 26 / 41 | 0 / 4 / 10 | **0 / 0 / 1** |
| civs 1 | 20 | 6 / 7 / 11 | 52 / 67.5 / 101 | 0 / 0.5 / 4 | 0 / 0 / 1 | 107,711 / 196,380 / 308,634 | 16 / 22.5 / 34 | 238 / 416.5 / 649 | 0.35 | 20 / 26 / 26 | 0 / 1 / 5 | 0 |
| civs 4 | 20 | 22 / 24.5 / 28 | 150 / 172 / 196 | 0 / 2 / 8 | 0 / 0 / 1 | 272,352 / 318,269 / 406,387 | 42 / 47.5 / 52 | 884 / 1,193 / 1,369 | 1.01 | 26 / 26 / 31 | 0 / 3 / 6 | 0 |
| sparse | 20 | 7 / 9 / 11 | 71 / 83 / 114 | 0 / 1 / 4 | 0 / 0 / 1 | 136,753 / 226,634 / 310,398 | 22 / 28 / 39 | 351 / 439 / 620 | 0.63 | 20 / 21 / 26 | 0 / 1 / 3 | 0 |
| dense | 20 | 7 / 10 / 16 | 71 / 87.5 / 134 | 0 / 1 / 5 | 0 / 0 / 1 | 148,127 / 226,660 / 310,350 | 22 / 30 / 39 | 426 / 612.5 / 1,101 | 0.37 | 26 | 0 / 2 / 7 | 0 |
| span 2000 | 20 | 7 / 9 / 11 | 20 / 47.5 / 76 | 0 / 0 / 1 | 0 | 46,169 / 102,847 / 176,287 | 10 / 17 / 31 | 85 / 193 / 302 | 0.49 | 20 | 0 / 0 / 2 | 0 |
| span 12000 | 20 | 8 / 12 / 17 | 78 / 106 / 148 | 1 / 2 / 7 | 0 / 2 / 4 | 154,831 / 234,550 / 346,250 | 22 / 30 / 39 | 677 / 1,046 / 1,632 | 0.45 | 26 / 26 / 41 | 0 / 4 / 10 | 0 |
| `elf` | 20 | 1 / 2 / 3 | 8 / 20 / 36 | 0 / 1 / 3 | 0 | 7,827 / 24,797 / 52,454 | 2 / 5.5 / 9 | 41 / 118 / 198 | 0.20 | 20 | 0 / 0 / 1 | 0 |
| `elf,orc` | 20 | 2 / 3.5 / 5 | 18 / 35 / 48 | 0 / 1 / 3 | 0 | 30,018 / 59,484 / 92,494 | 5 / 14 / 20 | 66 / 156 / 226 | 0.26 | 20 | 0 / 0 / 1 | 0 |
| no elves | 20 | 7 / 8.5 / 12 | 53 / 75.5 / 96 | 0 / 0.5 / 2 | 0 / 0 / 1 | 142,157 / 242,672 / 382,274 | 20 / 29 / 38 | 239 / 482 / 636 | 0.46 | 20 / 26 / 26 | 0 / 1 / 5 | 0 |

The variant sets use 20 seeds against the spec's 50. Against the spec's medians (15.3), every whole-world median is within 20%: events calm 389 (spec 390), turbulent 776 (811), civs 1 416.5 (413), civs 4 1,193 (1,158), dense 612.5 (634), span 2000 193 (178), span 12000 1,046 (1,195, −12%), `elf` 118 (116), `elf,orc` 156 (145), 512 268 (269), 2048 672 (802, −16% on 5 seeds). On seeds 0–49 the implementation reproduces 15.3 itself: standing 56 / 90 / 128, events median 557, human population median 76,378, humans the largest people in 26 of 50 worlds (all as in the spec text).

### 4.2 Per race, 1024 seeds 0–99 (min / median / max)

| Race | civilizations | settlements founded | standing | regions held | population | largest settlement |
| --- | --- | --- | --- | --- | --- | --- |
| elves | 1 / 2 / 4 | 3 / 19 / 49 | 3 / 18.5 / 48 | 1 / 5 / 13 | 1,385 / 22,593 / 63,780 | 764 / 2,516 / 10,261 |
| goblins | 1 / 3 / 8 | 4 / 34.5 / 93 | 4 / 34.5 / 92 | 1 / 9 / 25 | 5,570 / 82,972 / 211,114 | 2,786 / 7,516 / 7,999 |
| dwarves | 1 / 1 / 3 | 3 / 7.5 / 16 | 3 / 7.5 / 16 | 1 / 2 / 6 | 2,891 / 15,933 / 80,588 | 1,745 / 5,443 / 23,798 |
| faeries | 1 / 1 / 2 | 3 / 8 / 19 | 3 / 8 / 19 | 1 / 4 / 10 | 1,817 / 5,975 / 16,328 | 523 / 1,499 / 1,499 |
| humans | 1 / 1 / 5 | 3 / 11 / 38 | 3 / 10.5 / 38 | 1 / 4.5 / 16 | 4,395 / 59,802 / 242,728 | 2,671 / 15,196 / 25,000 |
| orcs | 1 / 1 / 3 | 3 / 6 / 17 | 3 / 6 / 17 | 1 / 2.5 / 8 | 1,871 / 14,084 / 59,136 | 954 / 5,350 / 7,999 |

Largest people: goblins 54, humans 41, dwarves 2, elves 2, orcs 1 (of 100). No race ever holds no settlement at the present in any set. At 512 a race holds no region at the present in 6 of 30 worlds (orcs in 5, humans in 2: seeds 1, 4, 12, 18, 26; spec 15.1 "Poor worlds" saw 8 of 50); never at 1024 or 2048. Elves stay under 70,000 and 50 settlements (max 63,780 and 48).

### 4.3 Events by kind, 1024 seeds 0–99 (min / median / max)

`colony_founded` 49 / 80.5 / 130, `became_village` 9 / 21 / 41, `became_town` 19 / 49.5 / 98, `became_city` 1 / 5 / 17, `settlement_dwindled` 2 / 10 / 22, `famine` 39 / 98.5 / 176, `plague` 27 / 64 / 128, `plague_spread` 28 / 111 / 276, `succession_crisis` 3 / 14 / 33, `civ_seceded` 0 / 2 / 7, `settlement_abandoned` 0 / 1 / 4, `capital_moved` 0 / 0 / 1, `civ_ended` 0, `ruin_faded` 0 / 0 / 2; `awakening` 6, `civ_founded` and `settlement_founded` 6 / 8 / 10, `quiet_era` 42 / 71 / 107. Standing kinds: hamlets 0 / 0 / 5, villages 16 / 35 / 67, towns 18 / 42.5 / 84, cities 0 / 3 / 13, fortresses 0 / 4 / 13. All are within a few counts of 15.3. Secessions happen in 82 of 100 worlds. Over the 100 worlds: 104 ruins, 5 faded.

### 4.4 Homes (15.1), 1024 seeds 0–99

| Race | n | upland | ore reach > 0 | wet ≥ 250‰ | wet | hab | fert | forest | grass | coast | dominant biome |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| elves | 155 | 0.00 | 0.00 | 0.00 | 6 | 105 | 174 | 991 | 0 | 0.89 | forest 155 |
| goblins | 159 | 0.01 | 0.99 | 0.02 | 67 | 69 | 122 | 123 | 369 | 0.21 | grassland 85, steppe 40, forest 24, heath 5 |
| dwarves | 144 | 0.78 | 0.94 | 0.00 | 0 | 3 | 6 | 84 | 18 | 0.12 | alpine 73, forest 30, highland moor 24, grassland 8 |
| faeries | 124 | 0.00 | 0.29 | 0.08 | 81 | 84 | 136 | 848 | 1 | 0.64 | forest 121, fen 3 |
| humans | 121 | 0.00 | 0.00 | 0.00 | 23 | 104 | 216 | 192 | 708 | 0.92 | grassland 93, forest 27, steppe 1 |
| orcs | 105 | 0.01 | 0.66 | 0.00 | 32 | 66 | 121 | 78 | 270 | 0.50 | steppe 54, grassland 36, forest 15 |

This is the table of 15.1 to within a few per mille in every column. Dwarven homes upland or with reachable ore: 144 of 144 (typical range: at least 90%).

### 4.5 Hard bounds and margins (15.5), every set

- **Not empty.** Civilizations alive ≥ 6 in every six-race world; standing ≥ 29 at 512; at 1024 ≥ 20 (span 2000, whose history is a third as long), ≥ 52 in the other six-race sets, ≥ 53 without elves, ≥ 18 for `elf,orc`, ≥ 8 for `elf`; ≥ 98 at 2048. No world has fewer than 3 (the bound at 1024 and 2048; 1 at 512).
- **Capacity.** P ≤ 2C on every standing settlement at the end and on every standing census row of every world. The largest P/C ratio is 1000‰ (a settlement at its region's full capacity), so never above 1 × C.
- **Events per simulated year.** At most 1.20 (turbulent seed 12; civs 4 at most 1.17); bound 200.
- **Runaway.** Largest share of non-islet regions held by one civilization at any year: 300‰ at 1024, 500‰ at 512, 113‰ at 2048; `share_over_500_years` and `share_over_800_years` 0 everywhere.
- **Sizes.** History tables at most 245,760 bytes at 1024 (span 12000), 225,280 (turbulent and civs 4); bound 5 MB.
- **At a bound by design.** W9 reaches its maximum of 3 events per civilization per quiet step in every set, never above.
- **Kind caps.** 7 standing settlements end above their kind cap, all faerie villages above 1,499 (512 seed 3: Mashy Glade 1,895). Each crossed its cap within 100 years after an abandonment nearby, so the excess is survivors, which invariant 3 allows.
- **Census.** No standing census row below 55 people.

### 4.6 Extremes

- **Most events:** 1024 seed 29 (966); turbulent seed 12 (1,432); span 12000 seed 3 (1,632, the largest log); civs 4 seed 1 (1,369).
- **Fewest events:** 1024 seed 72 (287); `elf` seed 6 (41); span 2000 seed 18 (85).
- **Largest population:** 1024 seed 59 (364,648); 2048 seed 2 (653,356); civs 4 seed 16 (406,387).
- **Largest settlement:** human cities at the cap of 25,000 (1024 seed 55 Alunton and five more); at 512 a dwarven town of 22,140 (seed 10, Hirknarin).
- **A race with no settlement at the end:** none in any set.
- **An ended civilization:** turbulent seed 17 only (section 9, item 2).

Files: `out/p3-05-validation/sweep/<set>/s<seed>-<size>.{db,txt,report.txt,check.json,ref.txt}`, `sweep/aggregate.txt` (all tables, every set), `sweep/ref-results.txt`, `sweep/gen-times.txt`, `sweep/jobs.txt`.

## 5. Purity and flags

`out/p3-05-validation/flags/run.sh`, seed 7 at 512 against the default run (`flags/result.txt`):

| Variant | Result |
| --- | --- |
| `--threads` 1, 2, 5, 6 | byte-identical; at 1024 seed 1, `--threads` 1, 2, 6 byte-identical and equal to CI case `geo-seed-1-1024` (`547e2f1b…`) |
| `--until history` | byte-identical to the default |
| `--until chronology` | differs only in `meta.stages` and the 12 non-empty history tables; all 13 history tables (catalogue included) have 0 rows |
| `--history-span 2000`, `12000` | `age`, `race` and the history tables; `meta` `param.history_span`, `history_end_year` |
| `--history-detail sparse`, `dense` | history tables only; `param.history_detail` |
| `--races elf`, `orc,elf` | `age`, `race`, history tables; `param.races` |
| `--civs-per-race 1`, `2`, `4` | **history tables only** (civilization, event, cause, participant, value, phase, region_control, settlement, census); `param.civs_per_race`. `phase` differs only in `trigger_event` |
| `--event-intensity calm`, `turbulent` | **history tables only**, the same set; `param.event_intensity`; `phase` only `trigger_event` |
| `--monster-pressure high`, `--divine-activity meddling` | only their `meta` key |
| the defaults given explicitly; `--races` with the full list | byte-identical |

No layer and no geography or chronology table changes for any history flag; `race_biome`, `god` and `culture` never change. The library resume path and threads 1 against 5 with peoples installed (spec 16.2 item 20) are tested by `P3-05 16.3 item 21, 16.2 item 20: geo-seed-42-512 (… resume)`, `P3-05 16.2 item 20: threads 1 and 5 give identical history tables` and the CLI's `cli P3-05 16.3, 16.2 item 20: … --until history, --threads 5`, which pass on every preset.

## 6. Recorded hashes

Values are the `file` lines of the five presets' identical `hash --detail` files, checked equal to `sha256sum` of each world. The library values come from the failures printed by `worldgen-tests`, identical on all five presets, each matched to its case by its old value. Each changed for a reason section 3 proves: `meta.generator_version` (every world) and `content_hash` (CLI worlds: the new data), and, for the geography and history cases, the catalogue (19 kinds), `race_biome` and the peoples history.

`worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase e35a78ae6d93d266cfd949c42a998edaf11d39e89cc3da635c19fabf119600b7
empty-seed-0 4251163813aa0b6c6aa798545f142b380260407a401311dc30a709b5eebd4f93
empty-seed-0-512 c981368d02a3ae655fc5c28e90b73d6a77f9b212b0e8bdeb76e7ffc8452cc8f5
empty-seed-1 91032ba75bb777b28724c9146b1f502ef89f5a080d39d908b1eb3aef7d43033a
empty-seed-42 26e2b16eed2249f06a69b671223e6dea25d89fa86c9292b2fefec21b26e1fd41
empty-seed-max d3cbbeb90c53a4e1f288d07fc957b3d39dd689cb76cf3ec2b970755e08fd6115
pattern-seed-0 5c0271fbb289244c681daa1492203bb33a09a58e5b88c8888fd3db793800655e
pattern-seed-42 dc98a212ecd14ca82d239559e905d21135b5779e3b78ec07f45c790f967e029a
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 1c8c2d3287b7f1c4a8a5241105b40be270c2cb8b2518854babc3f537134b7a42
geo-seed-0-512 75c6693bf2e18bb9c465a45aba4c8705f66566953663b1798031503d4f44bd19
geo-seed-1-1024 547e2f1b51553b0f91180703f0d81c298e6c2dd659b1db895e7b3b972afbbee4
geo-seed-42-512 35a3d4384d3a3ac9308c5c9ea3e13e93b7a6123d65724622208cfee876297ff0
```

`worldgen/tests/golden/p3-worlds.txt`:

```
hist-dense-1024 0316789b1b10c546297b775836cd4df4f4ddb64139df392944e5e3a9340605c6
hist-races-elf-orc-1024 69d8b4dedd7bce88f79baa2ea242b8c281fe3aa1919ac727dc7132b2999d999a
hist-span-2000-512 d6ea52b146c14aaad18fa175312177793a290c022922edbde910290914282c6a
```

The seven geography and history cases pass `check-world` and the independent check:

| Case | events | civilizations | standing |
| --- | --- | --- | --- |
| `geo-phrase-512` | 218 | 7 | 38 |
| `geo-seed-0-512` | 154 | 6 | 33 |
| `geo-seed-1-1024` | 564 | 11 | 88 |
| `geo-seed-42-512` | 349 | 8 | 54 |
| `hist-dense-1024` | 572 | 10 | 84 |
| `hist-races-elf-orc-1024` | 226 | 3 | 44 |
| `hist-span-2000-512` | 119 | 7 | 32 |

`GOLDEN_CASES` in `worldgen/tests/db_determinism_test.cpp`:

| Case | 0.14.0 | 0.15.0 |
| --- | --- | --- |
| `empty-phrase` | `1577ccad…` | `43cadb81099d46e7975bb21bb0c5875da918a9b505cc8c0d461698480aa64658` |
| `empty-seed-0` | `ac218e04…` | `101d523a897ec382da2c137cb27a1d7c9941ce209a3881624a99ece95e6f9058` |
| `empty-seed-1` | `bce45754…` | `539c5abedea8afa2ffd10c0daf9a175c0a93c6311f3b6daba2c03efd4b9d0b73` |
| `empty-seed-42` | `e6f6fbb7…` | `50200e2c472c37ee1b71af3ec8d5cb2b2a8ae02946e4e5cea5f1d949687839a2` |
| `empty-seed-max` | `bea95de3…` | `3f0a5d716ebc0e3f134019a10eae65bcc562285617a9d1f181cc695fafa9efd0` |

`worldgen/tests/stages_pipeline_test.cpp`: the empty world of seed 0 is now `101d523a…` (was `ac218e04…`, equal to `GOLDEN_CASES` `empty-seed-0` as before); the test pattern of seed 0 is now `728b3669836d7c4e6d85f66edc34b05d7e35b83412c7392611b18f35a4ac21f2` (was `1255bbe6…`). The comments beside them and the `GOLDEN_CASES` header now say 0.15.0. No other file outside `docs/reviews/` holds a 0.14.0 world hash (searched for each old value). No per-stage golden file was touched.

**README** (`generate --seed-phrase "The Unmapped"`, default size 1024, at this tip): `hash` `e0d30998f8cf264a1d4318a78fa497324714d8ee3c287af80cfb3ab1a936830a`. `[history]`: 6/5/0/1 phases, 1112 simulated years (185 per mille), 516 events, 10 civilizations (all alive), 86 settlements (all standing), 29 regions held, population 189,186. The full `report` text is `~/.cache/the-unmapped/out/p3-05-validation/readme-report.txt`.

## 7. Extension

`linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON` in its own tree (`build/p3-05-val/linux-gcc-release-gdext`):
- The build took 841 s with 0 warnings, at load 5 to 15 alongside round 2.
- `ctest -N` lists `gdext-headless-check`, and it passes in 27 s. Its real world (`--seed 42 --map-size 512`) is schema 7 with `stages` ending `,history`, has 349 events, and is byte-identical to CI case `geo-seed-42-512` (`35a3d438…`).
- `world-hashes.sh generate` under the label `linux-gcc-release-gdext` gives 15 case files identical to plain `linux-gcc-release` ("all 15 cases identical across 2 presets"), and `expect` gives 15 of 15 `ok`, exit 0.

## 8. Old files and the empty world

A 0.14.0 world (schema 7; CI cases `geo-seed-0-512` and `empty-seed-0` of the P3-04 validation) read by the 0.15.0 binary (`old/refusal.txt`):

- `report`: exit 0. The text equals the 0.14.0 binary's report of the same file, plus the 19 keys of 15.4 at the end of `[history]`, computed from the stub tables: `population = 1200`, `population_<race> = 200` for each of the six races, `settlements_standing_<race> = 1` and `regions_held_<race> = 1` for each.
- `hash --detail`: exit 0, byte-identical to the 0.14.0 binary's output.
- `render --layer elevation`, `render --composite map`: exit 0 for the geography world. For the empty world, exit 3 with `has no layer 'elevation'; the world has no layers`, exactly as the 0.14.0 binary.

A schema 6 world (the P3-03 validation's `geo-seed-0-512` and `empty-seed-0`): `report`, `hash --detail`, `render --layer` and `render --composite` exit 3 with `worldgen-cli: error: world schema version 6, this build reads 7` and print nothing on stdout. Plain `hash` prints the SHA-256 and exits 0. All of this is unchanged from 0.14.0 (`old/refusal-schema6.txt`).

Empty world, `generate --seed 0 --until none`: **139264 bytes, 34 pages**, `sqlite_schema` text 5839 bytes, 17 `meta` rows. Root pages are identical to the 0.14.0 empty world, object for object (`meta` 2 … `relation` 34).

## 9. Defects and findings

No defect.

1. **Finding, typical ranges (spec writer).** Over seeds 0–99 at 1024 defaults, a few worlds fall outside 15.5's typical ranges, all among seeds 50–99, which the spec did not measure:
   - standing 136 (seed 65; range 55–130);
   - population 364,648 and regions held 42 (seed 59; ranges up to 350,000 and 40);
   - events 287 (seed 72; range from 300);
   - events per simulated year 0.25, 0.27, 0.27 (seeds 72, 25, 52; range from 0.3).

   On seeds 0–49 every value is inside. Humans are the largest people in 41 of 100 worlds: 26 of 50 on seeds 0–49, exactly the spec's figure, and 15 of 50 on 50–99. The human population median over 100 worlds is 59,802, 22% below 15.3's 76,786; on seeds 0–49 it is 76,378, as the spec's over-time table. By 15.6 a median more than 20% off is a finding, not a defect: the spec's per-race medians rest on 50 seeds, and the next 50 seeds give fewer humans and more goblins (goblin median 82,972 against 72,214).
2. **Finding, behaviour the spec says never happens (spec writer, world critic).** 15.3 and 20 say no civilization ends by decline at `normal` or `turbulent`. In turbulent seed 17 at 1024 one does:
   - The Realm of Thisiene, an elven realm, is founded at year 0 with one village of 300.
   - Plague strikes it at year 32 and again at year 40. At year 40 the village is declining and below `abandon_frail_max` (1,200), and the 250‰ abandonment draw falls.
   - Risfae is abandoned with 219 survivors. They are lost, since the realm has no other settlement, and `civ_ended` follows the same year (events e6 to e9).

   This is correct by 8.5 (a) and item 6, and both checks pass. But a people with one settlement of 300 is always "frail" under `abandon_frail_max` 1,200, so a young realm can die in its first decades. It reads oddly: an awakening's second realm gone before the first quiet era. To reproduce: `worldgen-cli generate --seed 17 --event-intensity turbulent --out s17.db`, then `out/p3-05-validation/chronicle.py s17.db | head -12`. Possible readings for the spec writer: exempt a civilization's last settlement from the chance abandonment of 8.5 (a), or accept it as rare (1 in 355 worlds, only under `turbulent`).
3. **Observation, kind cap.** Faerie villages that take in refugees end above their 1,499 cap: 7 worlds, by 11 to 778 people. This is allowed by invariant 3. It stays so because growth stops at the cap but decline acts only above the share. A capital's share can be well above 1,499, so the excess never drains. The only effect is that the largest faerie settlement can read 2,277 against the stated cap; harmless, noted for the critic.
4. **Observation, sweep size.** As the lead sized it (1024 seeds 0–99, 20 seeds per variant), not 15.6's 0–199 and 50 per variant: 355 worlds in 1 h 51 min at load 12 to 28. At quiet load a 1024 world costs 25 s single-threaded, so 15.6's full set (about 900 worlds) would take about 70 minutes on six cores with the reference check. The P3-04 cost note still holds: every sweep world regenerates its geography.
5. **Observation, asan timeout margin (build engineer).** Under asan, `worldgen-tests-chronology` took 984 s on the quiet machine but 2199 s at load 12 to 28, 61% of its 3600 s timeout. `worldgen-tests-engine-worlds` took 923 s quiet and 1555 s loaded. These two are the first entries to watch if P3-06 adds work to their groups or a CI runner is slow.
6. Carried over, low, still open: `CAPTURE(c.name)` in `db_determinism_test.cpp` prints `c.name := 1` (seen again in round 1). The fix is `CAPTURE(std::string_view(c.name))`.
7. Carried over from P3-03 and P3-04: the geography of the 0.13.0 and later binaries is about 9.6% slower than 0.12.0; 0.15.0 inherits it unchanged (section 10).

## 10. Timing

`generate --seed 1`, the frozen 0.14.0 and 0.15.0 `linux-gcc-release` binaries alternating, each with its own `game/data`. Load 0.8 to 2.4 (quiet). Wall / user seconds (`timing/timing.txt`):

| Run | 0.14.0 | 0.15.0 |
| --- | --- | --- |
| 512, default threads, 3 pairs | 2.41, 2.39, 2.40 / 6.23, 6.25, 6.25 | 2.42, 2.43, 2.44 / 6.27, 6.28, 6.29 |
| 1024, default threads, 3 pairs | 8.96, 8.94, 8.94 / 25.24, 25.22, 25.21 | 9.04, 8.99, 9.00 / 25.31, 25.31, 25.29 |
| 2048, default threads | 36.12 / 101.07 | 36.18 / 101.26 |
| 512, `--threads 1`, 2 pairs | 6.21, 6.17 | 6.21, 6.22 |
| 1024, `--threads 1`, 2 pairs | 24.96, 25.15 | 25.18, 25.17 |
| 2048, `--threads 1` | 101.08 | 101.14 |

0.15.0 costs at most 0.3% more than 0.14.0, the size of the history stage.

**The history stage alone**: `UNMAPPED_PEOPLES_BENCH=… worldgen-tests -ts=peoples-bench --no-skip` (`linux-gcc-release`), on worlds stored `--until chronology`, three runs, load 2 to 7 (`timing/bench.txt`):

| World | events | history |
| --- | --- | --- |
| 512 seed 1 | 354 | 26–27 ms |
| 1024 seed 0 | 462 | 52 ms |
| 1024 seed 1 | 564 | 63–64 ms |
| 1024 seed 42 | 530 | 57–58 ms |
| 1024 seed 29 (most events of the sweep) | 966 | 81–82 ms |
| 2048 seed 1 | 716 | 161–166 ms |

Budgets of 16.4: 2 s at 1024, 0.5 s at 512, 8 s at 2048. The stage uses 3–4% of them. For the README, 1024 at default threads takes 9.0 s on a quiet machine.

## 11. Material for the world critic

`~/.cache/the-unmapped/out/p3-05-validation/critic/`, for seeds 42, 0, 1, 2, 3 and 7 at 1024 (defaults) and seed 42 with `--event-intensity turbulent` and with `--civs-per-race 4`. Per world `<name>.db`, `<name>.report.txt` (`report`), `<name>.regions.png` and `<name>.map.png` (`render --composite`), and `<name>.chronicle.txt`.

The chronicle is written by `out/p3-05-validation/chronicle.py`, my own renderer over the stored tables. It applies each event's stored `event_kind.template` with the P3-04 8.4 grammar, using the names from `region`, `god`, `civilization`, `settlement` and `race`. It lists every event in id order as `<year> e<id> [<kind>, salience <n>] <sentence> (causes: e..)`, with `<year>-<end_year>` for a quiet era. It then gives the end state per civilization: name, race, home, founded, alive or ended, capital with kind and population, settlements standing and ruined, population, and regions held by name. It ends with the settlement status counts and every ruin.

Cross-check: its first 60 lines on seed 42, reformatted to 15.2's line form, are byte-identical to the spec's 60 lines. The turbulent seed 17 chronicle of section 9 item 2 is `sweep/turbulent/s17-1024.chronicle.txt`.

## 12. Files

Under `~/.cache/the-unmapped/out/`:

- `p3-05-validation/`:
  - `logs/`: `build-*.log`, `build2-*.log`, `ctest1-*.log`, `ctest2-*.log`, `ctest-summary.txt`, `wh-gen-*.log`, `wh-compare.log`, `expect-*.log`, `gdext-*.log`.
  - Scripts: `check05.py`, `cmp015.py`, `chronicle.py`, `aggregate.py`, `gen-one.sh`, `sweep-job.sh`, `ctest_run.sh`, `pipeline1.sh`, `round2.sh`.
  - `bin-015/` (the frozen binary).
  - `base/`: the 0.14.0 comparison.
  - `sweep/`: 355 worlds with `hash --detail`, `report`, `check.json` and `ref.txt` each; `aggregate.txt`; `ref-results.txt`.
  - `flags/`: `run.sh`, `result.txt`, the variant worlds.
  - `example/`: seed 42 world and report, `chronicle-42.txt`.
  - `cases/`: `check-world` and the independent check of the 7 history cases.
  - `old/`: `refusal.txt`, `refusal-schema6.txt`, `report-014-geo-seed-0-512.txt`, the empty world.
  - `timing/`: `run.sh`, `timing.txt`, `bench.txt`, the `--until chronology` inputs.
  - `critic/`.
  - `readme/the-unmapped.db`, `readme/hash.txt`, `readme-report.txt`.
- `p3-05-val/world-hashes/`: the 15 case files of the five presets and of the extension build.
