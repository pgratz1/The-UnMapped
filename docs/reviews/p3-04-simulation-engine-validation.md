# Validation: P3-04 simulation engine (generator 0.14.0, schema 7)

Validation specialist, 2026-11-19. Branch `p3/04-validation`, cut from `origin/p3/04-int` at `fa87955` (engine, the test writer's tests and reference, the audit report and lint rule `history-live-iteration`, three hardening commits), then merged with `origin/p3/03-final` (`8bf8417`, main plus validated P3-03) and `origin/p3/06-spec` (`87cb262`, docs only). Both merges were clean: no conflict in any file. After the merges the per-stage golden vectors are those the task names: `p3-01-data.txt` and `p3-04-engine.txt` from `p3/04-int`, every other one (`p1-01` to `p1-06`, `p3-02-names.txt`, `p3-03-chronology.txt`) identical on both sides. The world-hash files and the inline hashes came in at their 0.13.0 values from `03-final` and are re-recorded here (section 6). Baseline for "what must not change": the 0.13.0 `linux-gcc-release` binary of the P3-03 validation (`3f8d3e1`, whose `worldgen/`, `game/` and `tools/` equal `8bf8417`), frozen as `out/p3-04-validation/bin-013/worldgen-cli`, with the P3-03 validation's saved case files and sweep worlds. Spec: `docs/specs/p3-04-simulation-engine.md` r4 (sections 5, 6.6, 7, 9, 11.1, 12, 14, 16.5, 16.6, 17).

Machine: 6-core Linux desktop, **shared for the whole run**: a spec writer's six-process Python sweep at the start, another agent's P3-05 builds later, plus this validation's own builds, sweeps and test runs. Load average 11 to 26 for most of the test and sweep runs, 4 to 7 for the timing (section 7). Every time below is marked with its load. Windows MSVC is checked by CI.

**Verdict: pass.** No defect against the stage. Two observations on the tests and one on the spec text for the lead (section 9).

- **Builds.** All five Linux presets build with warnings as errors (0 warnings in any log); the extension build too.
- **Tests before recording.** On all five presets (including `linux-clang-asan` in full, 2841 s, no sanitizer report) the only failures are the seven test cases that compare with recorded world-file hashes, the same seven as at P3-03; the printed values are identical on all five presets (section 2). Every per-stage golden vector passes unchanged. The `history-slow` suites pass too.
- **What must not change.** 0.13.0 against 0.14.0, the 14 existing cases plus `hist-dense-1024` and 30 sweep worlds: every layer and every geography and chronology table identical; the differences are exactly the schema (13 tables, one autoindex), `meta` (`schema_version`, `generator_version`, `content_hash`, `stages`, the new `history_end_year`) and the rows of the 13 new tables (section 3).
- **History sweep.** 133 worlds (1024 seeds 0–49, 512 seeds 0–19, 2048 seeds 0–2, and at 1024 seeds 0–9 each of span 2000, span 12000, sparse, dense, `elf,orc`, `orc`): every hard bound and invariant holds, checked by an independent recomputation written for this validation and by the test writer's `check-world`; 133 of 133 pass both (section 4).
- **Purity.** `--threads` 1, 2, 6 identical; `--until history` is byte-identical to the default; `--until chronology` leaves the 13 tables empty; each world-creation flag changes exactly what the spec says (section 5). The library resume path (Y2) and R1 pass on every preset.
- **Worked example (17).** Seed 42, 1024: phases, triggers, homes, names, sites, every event's values, `region_control`, census and the `[history]` block equal the spec text exactly (section 5.3).
- **Empty world:** 34 pages, 139264 bytes, schema text 5839 bytes, root pages as spec 9.6 and 16.5. Schema 6 worlds are refused by `report`, `hash --detail` and `render` with `world schema version 6, this build reads 7`, exit 3 (section 8).
- **Cross-preset.** All 15 `world-hashes.sh` cases are byte-identical across the five presets; the extension build's `worldgen-cli` gives the same 15 files.
- **Recorded:** `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt` (now three cases), the five `GOLDEN_CASES` values and the two `stages_pipeline_test.cpp` hashes (section 6). After recording, `ctest` passes 26 of 26 on all five presets and `expect` gives 15 of 15 on each and on the extension build.
- **Extension.** `gdext-headless-check` passes on schema 7 worlds (its real world runs `history`, 33 events).
- **Lint:** self-test 16 snippet files, 0 failures; scan 154 C++ and 13 CMake files, 0 findings.
- **Timing.** The `history` stage costs nothing measurable at 1024. 0.14.0 runs as fast as 0.13.0. The roughly 9% geography gap between 0.12.0 and 0.13.0 that the P3-03 validation saw is still there at lower load, and 0.14.0 inherits it unchanged (section 7).

## 1. What was run

| Check | Result |
| --- | --- |
| Configure and build of the five Linux presets at the merged tip | pass, all five, 0 warnings (194 to 305 s each with ccache, under load 13 to 26) |
| `ctest` before recording, five presets | 24 of 26 entries pass on every preset; the failures are only world-hash comparisons (section 2) |
| `history-slow` suites, `linux-gcc-release` | pass: `worldgen-tests -ts=history-slow --no-skip` 9 cases, 10,940,490 assertions (777 s, alongside the sweep and the debug `ctest` runs); `worldgen-cli-tests -ts=*slow* --no-skip` 9 cases, 364 assertions (531 s) |
| 0.13.0 against 0.14.0, 15 cases and 30 sweep worlds | 45 of 45 equal outside the stage (section 3) |
| History sweep (section 4) | 133 worlds, 0 failures in the independent check, `check-world` 133 of 133 |
| `--threads` 1, 2, 6 (512 seed 7, 1024 seed 1) | identical bytes; the 1024 file equals CI case `geo-seed-1-1024` (`fe277dd1…`) |
| `--until history`, `--until chronology`, history flags (section 5) | as the spec says |
| Worked example, seed 42, 1024 (section 5.3) | equal to spec 17 and 11.1 in every value |
| `world-hashes.sh generate` on the five presets, then `compare` | "all 15 cases identical across 5 presets" |
| Recording (section 6) | done from the compared output |
| `ctest` after recording | 26 of 26 on every preset (section 2.2) |
| `world-hashes.sh expect`, five presets and `linux-gcc-release-gdext` | 15 of 15 `ok` on each, exit 0 |
| `determinism_lint.py --self-test` and scan | pass: 16 snippet files, 0 failures; 154 C++ and 13 CMake files, 0 findings |
| Extension, `linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON` (own tree `linux-gcc-release-gdext`) | build 808 s (load 15–20); `gdext-headless-check` passes (17 s); its real world is schema 7 with `stages` ending `,history` and 33 events, its test-pattern world schema 7; its `worldgen-cli` gives the same 15 case files as plain `linux-gcc-release` |
| Schema 6 refusal and empty world (section 8) | pass |
| Timing (section 7) | stage not measurable; 0.14.0 = 0.13.0 |

## 2. Tests

### 2.1 The failing set before recording

Identical on all five presets (line numbers of multi-line `CHECK`s differ by one between GCC and Clang). Every failed assertion compares with a recorded world-file hash; these are the same seven test cases that failed before recording at P3-03.

| Test case | Entry | Compared with |
| --- | --- | --- |
| `cli 9, 12.6: golden world hashes from p0-worlds.txt` | `worldgen-cli-tests-other` | `p0-worlds.txt`, 8 cases |
| `cli P0-03 7.6, amendment 17: with the required kind worldgen, a nonexistent data root is an input error` | the same | `p0-worlds.txt` `empty-seed-0` |
| `cli P1-00 10.1: --until none writes the P0 empty world, byte for byte` | the same | `p0-worlds.txt` |
| `cli P1-00 10.1: --threads accepts 1 to 256 in decimal without leading zeros` | the same | `p0-worlds.txt` `empty-seed-0` |
| `cli P1-00 10.1, D10, X3: --threads never changes the output (test-pattern world at 1, 2 and 5 threads)` | the same | `p0-worlds.txt` pattern cases |
| `golden: empty-world hashes match the recorded values (12.4)` | `worldgen-tests-other` | `GOLDEN_CASES`, 5 values |
| `generate P1-00 10.1: --until none and every thread count give the P0 empty world` | the same | the two literal hashes |

The 40 `values:` lines printed by the failures have the same digest on all five presets (`72031fc86c207796`); the seven library values are the ones recorded in section 6. The full asan run (2841 s, load 4–17) printed no AddressSanitizer, LeakSanitizer or UBSan report.

### 2.2 Times of each run, and of the engine entries

Full `ctest`, preset jobs (4). Round 1 before recording, round 2 at the recorded tip (round 2 of `linux-clang-asan` ran alongside the other four presets' round 2).

| Preset | round 1 | round 2 | `worldgen-tests-engine-worlds` (1 / 2) | `worldgen-tests-engine` (1 / 2) | `worldgen-cli-tests-engine` (1 / 2) |
| --- | --- | --- | --- | --- | --- |
| `linux-gcc-release` | 300 s, 24/26, load 23–26 | 164 s, 26/26, load 10–14 | 88.9 / 39.9 s | 31.1 / 24.8 s | 38.0 / 22.9 s |
| `linux-clang-release` | 242 s, 24/26, load 17–21 | 214 s, 26/26, load 11–17 | 86.8 / 32.8 s | 25.0 / 19.5 s | 23.6 / 19.8 s |
| `linux-gcc-debug` | 695 s, 24/26, load 15–16 | 792 s, 26/26, load 12–15 | 211.7 / 237.2 s | 69.8 / 89.8 s | 83.8 / 119.0 s |
| `linux-clang-debug` | 743 s, 24/26, load 11–16 | 732 s, 26/26, load 12–14 | 234.6 / 226.2 s | 76.6 / 78.9 s | 97.3 / 92.7 s |
| `linux-clang-asan` | 2841 s, 24/26, load 4–17 | 2599 s, 26/26, load 6–17 (alongside the other round-2 runs), 0 sanitizer reports | 1195.8 / 926.5 s | 289.8 / 291.9 s | 304.7 / 333.4 s |

`worldgen-tests-engine-worlds` under asan takes about 20 minutes, a third of the 3600 s timeout; the longest asan entry is now `worldgen-tests-chronology` (1477 s in round 1). No entry is near its timeout.

## 3. What must not change: 0.13.0 against 0.14.0

Comparison script `out/p3-04-validation/cmp013.py`. For a 0.13.0 and a 0.14.0 world of the same case it requires:

- every `layer` line of `hash --detail` identical, every `table` line of the schema 6 tables identical except `table meta`; the added lines exactly the thirteen history tables; `schema` differs;
- in `sqlite_schema`, every schema 6 object with identical SQL; the new objects exactly the thirteen tables and `sqlite_autoindex_event_kind_1`;
- in `meta`, the one added key `history_end_year`, equal to `param.history_span`; the changed keys at most `schema_version` (6 → 7), `generator_version` (0.13.0 → 0.14.0), `content_hash` and, for a world that ran chronology, `stages` (`…,chronology` → `…,chronology,history`);
- the thirteen tables non-empty exactly when `stages` contains `history` (`relation` and `event_cause` excepted, which may be empty in a history world).

`content_hash` changes because every data root gains `history/engine.toml` and `history/report.toml` (the only files that differ in `game/` and `worldgen/tests/data/` between `03-final` and this tip). The new value of every world, `21d100b0…6fa60f83`, is exactly the `game/data` line of the test writer's re-recorded `p3-01-data.txt`.

| Set | Result |
| --- | --- |
| `empty-*` (6) | equal: 12 identical detail lines each (all tables); `meta` changed `content_hash`, `generator_version`, `schema_version` |
| `pattern-*` (2) | equal, 15 identical lines each, the same three keys |
| `geo-*` (4), `hist-races-elf-orc-1024`, `hist-span-2000-512` | equal: 31 identical lines each (19 layers, 12 tables); the same three keys and `stages` |
| `hist-dense-1024` (new; 0.13.0 side generated with the 0.13.0 binary, seed 11, dense) | equal, 31 lines |
| Sweep: 1024 seeds 0–19, 512 seeds 0–9 (0.13.0 side: the P3-03 validation's sweep worlds) | 30 of 30 equal, 31 lines each |

Results: `out/p3-04-validation/base/cases-013-vs-014.txt`, `base/cmp-sweep.txt`.

## 4. History sweep (spec 14)

Binary: `linux-gcc-release` at the merged tip, frozen as `out/p3-04-validation/bin-014/worldgen-cli`. The CLI cannot resume a stored world, so every world was generated in full: 133 generations, two at a time with three threads each, 15:12 to 15:49 under load 13 to 26 (1024: 42 s mean at default settings while builds ran, 30 s for the later variant sets; 512: 8 s; 2048: 109 s). 786 MB.

Every world passed: `generate` exit 0, empty stderr, and the checks of `out/p3-04-validation/sweepcheck.py`:

1. **Independent recomputation from the stored rows**, written for this validation from spec sections 5, 6.6, 6.7, 7 and 11.1 without the C++ and without the test writer's reference: SC1–SC4 recomputed from the stored age start years and the shipped `engine.toml`, and the whole `phase` table (ids, ages, kinds, years, `trigger_event`) required equal; tiling of [0, E); simulated years in [floor, cap] and 100–450 per mille; no kind 3 row; one opening per age; the full log of a stub world rebuilt (per age: `awakening` with race and creator god, `civ_founded` caused by it, `settlement_founded` with population 200 caused by that; per quiet row one `quiet_era` per living civilization with `years` and `generations` from `race.generation_years`) and required equal row for row, participants, values and causes included; ids 1…n, years non-decreasing, `event.phase` the phase containing `year`, causes below their event, salience the base salience; stub homes by S1 (first free non-islet region by habitability, walk cells, id) and sites at the home's seed cell; `civilization` and `settlement` end state; census rows (one founding row each); `region_control` (P open rows); `relation` empty; names ASCII, at most 64 bytes, distinct; on six-race worlds at least 3 settlements standing; and `report`'s `[history]` block equal, key for key and in order, to the values computed from the rows.
2. **The test writer's reference**: `p3_engine_reference.py check-world DB --data game/data` (in process), which recomputes the five chronology and thirteen history tables from the stored geography and compares them, re-checks invariants 1–6 and renders every sentence: 35 checks per world, **133 of 133 pass**.

| Set | n | simulated years min / median / max | per mille | phases opening / quiet / flare / final | events | civs = settlements = held | floor acted (at floor) | E4 | regions |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| default 512 | 20 | 1112 / 1170 / 1200 | 185–200 | 6/5/0/1 (14), 6/6/0/1 (6) | 33 (14), 39 (6) | 6 | 0 (0) | 14 | 11–16 |
| default 1024 | 50 | 1112 / 1166 / 1200 | 185–200 | 6/5/0/1 (35), 6/6/0/1 (15) | 33 (35), 39 (15) | 6 | 0 (0) | 35 | 44–59 |
| default 2048 | 3 | 1170 / 1200 / 1200 | 195–200 | 6/5/0/1 (1), 6/6/0/1 (2) | 33, 39 | 6 | 0 (0) | 1 | 195–204 |
| span 2000 | 10 | 370 / 398 / 400 | 185–200 | 5 or 6 quiet | 33 (6), 39 (4) | 6 | 0 (0) | 6 | 47–56 |
| span 12000 | 10 | 2224 / 2390 / 2400 | 185–200 | 5 or 6 quiet | 33 (6), 39 (4) | 6 | 0 (0) | 6 | 47–56 |
| sparse | 10 | 700 / 700 / 700 | 116 | 6/6/0/1 (10) | 39 | 6 | 0 (0) | 0 | 47–56 |
| dense | 10 | 1612 / 1695 / 1722 | 268–287 | 6/5/0/1 (10) | 33 | 6 | 0 (0) | 10 | 47–56 |
| `elf,orc` | 10 | 600 / 600 / 600 | 100 | 2/1/0/1 (6), 2/2/0/1 (4) | 7 (6), 9 (4) | 2 | 6 (10) | 6 | 47–56 |
| `orc` | 10 | 600 / 600 / 600 | 100 | 1/1/0/1 (10) | 4 | 1 | 10 (10) | 0 | 47–56 |

Against spec 14.2 and 5.5:

- **`simulated_permille`**: sparse 116 (typical 116–117), normal 185–200 at all three sizes and both span extremes (typical 185–200), dense 268–287 (typical 268–288). Never near the cap of 450 (the largest is 287).
- **Simulated years**: normal at E = 6000 1112–1200 with median 1166 at 1024 (spec 1110–1200, median 1171 over 1000 seeds); span 2000: 370–400 (spec 370–400); span 12000: 2224–2400 (spec 2220–2400); sparse 700 (spec 700), dense 1612–1722 (spec 1610–1730). P3-00 4.1's 1024 median 1000–1400 holds.
- **Phases**: opening = present races in every world; final exactly 1 in every world; flare 0 in every world. Rows 12–13 (normal), 13 (sparse), 12 (dense), as 5.5.
- **Events**: 3P + one era per living civilization and quiet row in every world (recomputed row for row above). See section 9, item 3, on the 14.2 wording.
- **E4** (last opening reaching past E − L, final phase shorter than L): 50 of 73 default worlds (68%; spec 73% over 1000 seeds), every dense world, no sparse world.
- **At a bound**: every `elf,orc` and every one-race world sits exactly at the floor, 600 years = 100 per mille, the lower hard bound. For `orc` the floor lengthens the final phase backwards in all 10 worlds (E7, E10); for `elf,orc` it acts in the 6 worlds where the orcs awaken after 5550, and in the other 4 the base schedule is already A + A + L = 600 by itself (orcs before 5550: openings 150 + 150, final 300). This is the scheduler working as designed, not an oddity: the shipped normal lengths are 10% for two races and 7.5% for one. No world is at the cap.
- **Not empty**: every present race has a civilization and a settlement, every civilization is alive at E, every settlement thriving with 200 people. No race ever fails to found a settlement; no age is without events.
- **Table sizes**: the thirteen tables and their autoindex take 14 pages, 57,344 bytes, in a 512 world (far under 5 MB); the file size does not change between `--until chronology` and the full run.

Files: `out/p3-04-validation/sweep/<set>/` (world, `hash --detail`, `report` per world), `sweep/all.jsonl` (one check line per world), `sweep/gen-times.txt`.

## 5. Purity, flags and the worked example

### 5.1 Threads and stage limits

- `--threads` 1, 2, 6: identical bytes at 512 seed 7 (`55dbbfe6…`, equal to the default run) and at 1024 seed 1 (`fe277dd1…`, equal to CI case `geo-seed-1-1024`).
- `--until history` gives the same bytes as the default run (512 seed 7).
- `--until chronology` differs from the default exactly in `meta.stages` and the twelve non-empty history tables (`relation` is empty in both); the catalogue tables are empty too, as 9.5 says.
- Resume (H8): not a CLI command (spec 3.2, question 5). The library tests R1 (`history_inputs_from` = `history_inputs_from_world`), R2 and Y2 (`--until chronology` world → `history_inputs_from_world` → `history_stage` = the full run's tables), and each refusal of 3.2, pass on all five presets (`worldgen-tests-engine-worlds` for `geo-seed-42-512`; the other 512 cases and the 1024 cases in `history-slow`, which passes on `linux-gcc-release`).

### 5.2 World-creation flags

`out/p3-04-validation/flags/run.sh`: three base worlds of seed 7 (512 geography, `--until none`, `--test-pattern`) with the default settings and 20 variants, compared through `hash --detail` and `meta` (`flags/result.txt`).

| Flag | Geography world | Empty and test-pattern worlds |
| --- | --- | --- |
| `--history-span 2000`, `12000` | `meta` `param.history_span` and `history_end_year`; tables `age`, `race`, and of the history tables `phase`, `event`, `event_value`, `civilization`, `settlement`, `settlement_census`, `region_control` (years move; participants and causes keep their ids) | `param.history_span` and `history_end_year` only |
| `--history-detail sparse` | `param.history_detail`; `phase`, `event`, `event_participant`, `event_value` (a sixth quiet row adds eras) | the key only |
| `--history-detail dense` | `param.history_detail`; `phase`, `event`, `event_value` | the key only |
| `--races elf`, `orc,elf` | `param.races` (canonical `elf,orc`); `age`, `race` and eleven history tables | the key only |
| `--civs-per-race 1`, `4` | the key only (the stub ignores it, 7.2) | the key only |
| `--event-intensity`, `--monster-pressure`, `--divine-activity` | the key only | the key only |
| the defaults given explicitly, `--races all`, the full race list | byte-identical | byte-identical |

No layer and no geography table changes for any flag; `god`, `culture`, `race_biome` and the three catalogue tables never change. `--until history` cannot be combined with `--until none` or `--test-pattern`; the CLI refuses both combinations with a usage error (exit 2), as expected.

### 5.3 Worked example (spec 17)

`generate --seed 42` (1024, span 6000, normal, all races), `out/p3-04-validation/example/`:

- `report` `[history]` equals spec 11.1 line for line: `end_year = 6000`, phases 6/5/0/1, `simulated_years = 1154`, `simulated_permille = 192`, `events = 33` (6, 6, 6, 15, 0 by kind), 6 civilizations, 6 alive, 6 settlements, 6 standing, `regions_held = 6`, `census_rows = 6`, `relations = 0`.
- `phase`: the twelve rows of the table in 17, with triggers e1, e5, e10, e16, e23, e31.
- Homes 12, 10, 14, 30, 18, 4 with the names of 17; civilizations `Finen` … `The Warband of Baaldkrok`; settlements `Voeledell` (755, 293) … `Uld'kag` (241, 201), hamlets of 200.
- Every `quiet_era`'s values: 1924/12; 1018/7, 67; 851/6, 57, 12; 576/4, 39, 8, 6; 477/3, 31, 7, 5, 19, as in 17.
- `region_control` (4, 5596, NULL, 6, e32), (10, 2074, 2, e6), (12, 0, 1, e2), (14, 3242, 3, e11), (18, 4969, 5, e24), (30, 4243, 4, e17); six founding census rows.
- Sentences: the CLI has no sentence output until P3-07, so the C++ `render_event` was checked through the tests: `history-slow` renders every event of `example-42-1024` (and both `ForcedTension` variants) and compares with the golden file, whose lines e1–e4 are exactly the four sentences of spec 8.5 (`Fielie awakened the Elves in Northeast Downs IV.` · `The Elves founded Finen in Northeast Downs IV.` · `Finen founded Voeledell in Northeast Downs IV.` · `Finen passed 1924 quiet years (12 generations), from 150 to 2073.`). The reference's `world` command on the generated file prints the same 33 sentences (`example/reference-world.txt`).

## 6. Recorded hashes

From the compared output: the `file` line of the five presets' identical `hash --detail` files (checked equal to `sha256sum` of each world), and for the library values the failures printed by `worldgen-tests`, identical on all five presets. Each changed for a reason section 3 proves: every world gained the thirteen tables (34 pages instead of 18) and `meta` changed (schema 7, generator 0.14.0, `history_end_year`, and for the CLI worlds the content hash of the data with the two new files); the geography and history worlds also gained the history rows. `GOLDEN_CASES` `empty-seed-0` equals the first `stages_pipeline_test.cpp` hash, as before, since both are the same world.

`worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase cac4fdde42c7822ccc78717eb70729223fad44cf338082789f78772380f2f568
empty-seed-0 13837b9984bc6bde3ae14207e5c2631320cde16730d498db76156af2a4152744
empty-seed-0-512 baee460b478e590b0fd69cd7931e0c1719fa76b334b88886e1398fe16cfdd9b4
empty-seed-1 f2bdf744f1e99e5d37d0bb2f335802daf274e112750fedc3baa95495ed2ae2ca
empty-seed-42 42fcbb5e4c32a8b36e1b15e8e3fdcad48536c287704ccdb8e54d656c8a2acd1d
empty-seed-max 014dac98193d6606980efa9d9e260d7629b89cb2d10991ab2638a1b482e95880
pattern-seed-0 afd68da9f94a170fcdf296c52552ee6a40016662175b96905c30fd6404adf24d
pattern-seed-42 79d3fc6757c579b1361082447c18a757d41e778d56892ba8ff7273a6d2a86566
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 bb6e262e0f361b413cf218b81047cffe2ce275c18a0e70b631777e0788b9f610
geo-seed-0-512 403bac24c71b6d550144be82e5fe5abf0aa882c66f57e07d09d21d6576fd2cff
geo-seed-1-1024 fe277dd164026c30e3d7fdd3a49f1b397cbbd0ba625814836024996213400529
geo-seed-42-512 7448c6a444c1f25437bcf26bb3eb6a231db9db011f0ac031c283cfd57bc7aa42
```

`worldgen/tests/golden/p3-worlds.txt` (`hist-dense-1024` added):

```
hist-dense-1024 1e71bf850bc4148377f60f2ff37746ebd9a9e8e8eb19a53b9dbccfce6e19c709
hist-races-elf-orc-1024 574b468e8debb4669e784475720afb071f837cd198fbd063df4a23fdbb3a22e8
hist-span-2000-512 5682db8726e1698053feb1f947828b3208bd8e683909756288899faa3b749fd1
```

The three `hist-*` cases pass `check-world` and match spec 14.2: `hist-dense-1024` (seed 11, dense) 12 phases, 1632 simulated years (272 per mille), 33 events, the first case where the detail level changes table bytes; `hist-races-elf-orc-1024` (seed 3) 600 years at the floor (100 per mille), 7 events; `hist-span-2000-512` (seed 7) 371 years (185 per mille), 33 events.

`GOLDEN_CASES` in `worldgen/tests/db_determinism_test.cpp` (the empty world with the content hash of no data):

| Case | 0.13.0 | 0.14.0 |
| --- | --- | --- |
| `empty-phrase` | `d9af7260…a2a9d788` | `1577ccad1204ad1e2143749b0d23232bc3589268422ac3834bdcbf5894b3f344` |
| `empty-seed-0` | `c5326b79…0074fed2` | `ac218e049a9bf1dc77894bf0f0cb98ec152742d27a2557afa2d4cb211d1601a3` |
| `empty-seed-1` | `6bafbea5…2d9887ae` | `bce45754e193b60eabbb4dc0575fbca8af4b1a1df6081485a67e5cc86007af00` |
| `empty-seed-42` | `e54a061a…4f4c7b89` | `e6f6fbb7abae2a72b8115835cc7fe7bf1c4bcabd48437eaedc094e772128926c` |
| `empty-seed-max` | `31a5de19…62a38` | `bea95de31789a494218ec753e8f3ff3df91656b6e738452126dda27c2084a6b8` |

`worldgen/tests/stages_pipeline_test.cpp`, test case `generate P1-00 10.1: --until none and every thread count give the P0 empty world`:

- the empty world of seed 0 becomes `ac218e049a9bf1dc77894bf0f0cb98ec152742d27a2557afa2d4cb211d1601a3` (was `c5326b79…`);
- the test pattern of seed 0 becomes `1255bbe6437bcc5321ac3bf42f7bb72f65f8201119521e20a746da5b5032fc15` (was `6169c660…`).

The comments beside those values now say generator 0.14.0 and schema version 7. No other file holds a 0.13.0 world hash (searched the tree for every old value; only the P3-03 review documents quote them). No per-stage golden file was touched.

**README** (`generate --seed-phrase "The Unmapped"`, default size 1024, at this tip): `hash` `88aaeaea491579a257aab52edcbb00ce0818c9da01e4ed6fca3c0126e414804a`; `[history]` 6/5/0/1 phases, 1112 simulated years (185 per mille), 33 events. The full `report` text is `~/.cache/the-unmapped/out/p3-04-validation/readme-report.txt`.

## 7. Timing

`generate --seed 1`, the frozen 0.13.0 and 0.14.0 `linux-gcc-release` binaries, alternating, each with its own `game/data`; wall and user seconds (`out/p3-04-validation/timing/timing.txt`, `timing2.txt`). The machine was as quiet as it got in this session, but not quiet: load 3.5 to 7.6 from another agent's P3-05 builds. Wall times scatter with that load; single-thread user times are stable to about 1%.

| Run | 0.13.0 | 0.14.0 |
| --- | --- | --- |
| 512, default threads, 3 pairs: wall / user | 3.61, 4.17, 2.87 / 6.25, 6.25, 6.26 | 2.93, 5.07, 3.16 / 6.26, 6.22, 6.26 |
| 1024, default threads, 3 pairs | 14.6, 14.4, 8.9 / 25.3, 25.3, 25.2 | 14.3, 10.1, 8.9 / 25.3, 25.2, 25.2 |
| 512, `--threads 1`, 3 pairs | 6.17, 6.34, 6.34 / 6.13, 6.25, 6.25 | 6.28, 6.30, 6.58 / 6.22, 6.24, 6.24 |
| 1024, `--threads 1`, 2 pairs | 25.7, 26.0 / 25.5, 25.5 | 25.7, 25.6 / 25.5, 25.5 |
| 512, `--threads 1 --until regions`, 3 pairs (user) | 6.26, 6.22, 6.22 | 6.19, 6.19, 6.16 |
| 1024, `--threads 1 --until regions`, 2 pairs (user) | 25.16, 25.10 | 25.18, 25.09 |

- **0.14.0 is as fast as 0.13.0**: the full run and the geography alone take the same user time to within 1% at both sizes.
- **The `history` stage costs nothing measurable** at 1024: 0.14.0 `--threads 1`, `--until chronology` against the full run, alternating, user 25.05 / 25.11, 25.17 / 25.17, 25.25 / 24.96 s. The differences are noise of ±0.15 s, so the stage is well under that; spec 14.3's 50 ms could not be resolved from the CLI, and the microbenchmark of 14.3 does not exist (section 9, item 1).
- **The 0.12.0 → 0.13.0 geography gap reproduces and 0.14.0 inherits it unchanged.** `--until regions`, 512, `--threads 1`, the 0.12.0 binary of the P3-03 baseline beside the other two, three rounds (load 5–7): user 5.65, 5.69, 5.66 s (0.12.0) against 6.21, 6.19, 6.21 (0.13.0) and 6.19, 6.24, 6.21 (0.14.0): +9.6%, as the P3-03 validation measured (5.64–5.68 against 6.18–6.20) at load 4 to 7 then too. The extra cost belongs to the 0.13.0 binary and did not grow with 0.14.0. No profiler was run (`perf_event_paranoid` is 4), so the P3-03 report's reading, code placement in the linked executable with identical geography object code, is neither confirmed nor ruled out. It changes no output byte. If the README quotes generate times they should be taken from 0.14.0 on a quiet machine; 1024 at default threads took 8.9 s when the load briefly dropped.

## 8. Old files and the empty world

A schema 6 world written by the 0.13.0 build (CI cases `geo-seed-0-512` and `empty-seed-0` of the P3-03 validation, whose hashes are 0.13.0's golden values), read by the 0.14.0 `linux-gcc-release` build (`out/p3-04-validation/old/refusal.txt`):

| Command | Exit | stderr |
| --- | --- | --- |
| `report --db <file>` | 3 | `worldgen-cli: error: world schema version 6, this build reads 7` |
| `hash --db <file> --detail` | 3 | the same |
| `render --db <file> --layer elevation` | 3 | the same |
| `render --db <file> --composite map` | 3 | the same |

Nothing on stdout in any case; the same for both files. Plain `hash --db <file>` (no `--detail`) prints the SHA-256 and exits 0, as P0-04 specifies.

Empty world, `generate --seed 0 --until none`: **139264 bytes, 34 pages**, `sqlite_schema` SQL text 5839 bytes, 17 `meta` rows, `report` `[tables]` lists 26 tables. Root pages `meta` 2 … `age` 18, `event_kind` 19, `sqlite_autoindex_event_kind_1` 20, `event_kind_role` 21, (22 and 23 absent: the `sqlite_schema` leaves), `event_kind_value` 24, `phase` 25, `event` 26, `event_participant` 27, `event_cause` 28, `event_value` 29, `civilization` 30, `settlement` 31, `settlement_census` 32, `region_control` 33, `relation` 34: identical to spec 9.6 and to the engineer's 16.5.

## 9. Defects and observations

No defect against the stage, the tests or the reference.

1. **Observation, tests (for the test writer).** Spec 14.3 asks for "a microbenchmark test with a tagged doctest suite, not in `ctest`'s default run" that reports the engine's overhead per simulated year (budget below 10 µs with empty subsystems). No such test exists on this tip: no suite other than `history` and `history-slow` among the engine tests, and no test that times anything. The 50 ms budget is therefore unverified at the resolution the spec asks for; from the CLI the stage is below the ±0.15 s noise of a 25 s run (section 7). Suggested: a `history-bench` suite (skipped by default) that runs `run_engine` with the stub on the 1024 world cases and prints the time per simulated year.
2. **Observation, tests.** Spec 16.2 item 15 asks for the scheduler property sweep over seeds 0–999; the slow test is `P3-04 16.2 item 15: the scheduler sweep over seeds 0-99` (one tenth). It is the thinning the lead mentioned, done to keep `worldgen-tests-engine` under the asan timeout, and the sweep of section 4 and the 315,000-schedule P3-03 sweep cover the same ground with real worlds and age lists. The lead may want 16.2 item 15 amended to the seeds actually run, or the full range moved to a suite nobody runs by default.
3. **Observation, spec text (for the spec writer, no change to code).** Spec 14.2's "typical" events column says 33 for normal and dense, 39 for sparse. At normal, 30% of seeds have six quiet rows, not five (the worlds without E4, spec 5.3's own 73%), and then have 39 events: 15 of 50 default 1024 worlds, 6 of 20 at 512, 2 of 3 at 2048, 4 of 10 at each span extreme. The hard rule (3P + the eras) holds in every world; only the typical column is imprecise. Suggested: "normal: 33 or 39".
4. Carried over from P3-03 (lead to decide): the geography is about 9.6% slower in the 0.13.0 and 0.14.0 binaries than in 0.12.0, reproduced at lower load (section 7).
5. Carried over, low, still open: `CAPTURE(c.name)` in `db_determinism_test.cpp` prints `c.name := 1` instead of the case name (seen again in the round-1 failures). One-line fix for the test writer: `CAPTURE(std::string_view(c.name))`. Not changed here.
6. Cost note: a history sweep still regenerates the geography for every world (133 generations, 37 minutes of wall time on two jobs, for a stage that takes milliseconds). P3-05 and P3-06 sweeps will be expensive in the same way unless the resume seam gets a CLI or a test-only driver.

## 10. Files

Under `~/.cache/the-unmapped/out/`:

- `p3-04-validation/`: `logs/` (`build-*.log`, `build2-*.log`, `ctest1-*.log` before recording, `ctest2-*.log` after, `ctest-summary.txt`, `expect-*.log`, `wh-gen-*.log`, `wh-compare.log`, `slow-lib.log`, `slow-cli.log`, `gdext-build.log`, `gdext-check.log`); `sweepcheck.py`, `cmp013.py`, `cmpflag.py`, `gen-one.sh`, `build_all.sh`, `ctest_all.sh`, `round2.sh`; `bin-013/`, `bin-014/` (the frozen binaries); `base/` (`cases-013-vs-014.txt`, `cmp-sweep.txt`, the 0.13.0 `hist-dense-1024` world); `sweep/` (133 worlds with `hash --detail` and `report`, `all.jsonl`, `gen-times.txt`, `jobs.txt`); `flags/` (`run.sh`, `result.txt`, the variant worlds); `example/` (seed 42 world, its report, `reference-world.txt`); `old/` (`refusal.txt`, the empty world); `timing/` (`timing.txt`, `timing2.txt`); `readme/the-unmapped.db`, `readme/hash.txt`, `readme-report.txt`.
- `p3-04-val/world-hashes/`: the 15 case files of the five presets and of `linux-gcc-release-gdext`.
