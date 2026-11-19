# Validation: P3-03 chronology (generator 0.13.0, schema 6)

Validation specialist, 2026-11-19. Branch `p3/03-validation`, cut from `origin/p3/03-int` at `3ddc942` (implementation, independent tests and reference, check D0, revised specs, merged with `main` at 0.12.0 and the P3-02 names library). Before recording, `origin/p3/03-audit-guard` (`76fc44e`, audit A1: two fatal guards and a guard test) and `origin/p3/03-audit` (`3139ac9`, the audit report) were merged in (`74a7ed0`, `af862bd`); the recorded hashes belong to that tip. Baseline: `100cb30` (`origin/main`, generator 0.12.0, schema 5), built in a scratch worktree outside the repository directory (`~/.cache/the-unmapped/wt/p303val-base`). Spec: `docs/specs/p3-03-chronology.md` (sections 4.3, 5, 7, 10, 12, 13, 14), P3-00 12.3 "order and schedule".

Machine: 6-core Linux desktop, **under heavy load for most of the run** (load average 20 to 37 from a concurrent P3-04 engine build and test runs in another worktree, plus this validation's own builds). Every timing in this report is marked with the load it was taken under. Windows MSVC is checked by CI.

**Verdict: pass.** No defect against the stage. One observation for the lead about generate time (section 7).

- **Builds.** All five Linux presets build with warnings as errors, before and after the merge of the audit branches; the extension build too.
- **Tests before recording.** On all five presets the only failures are the comparisons with recorded world-file hashes (section 2). Every per-stage golden file (`p1-01` to `p1-06`, `p3-01-data.txt`, `p3-02-names.txt`, `p3-03-chronology.txt`) passes unchanged.
- **What must not change.** 0.12.0 against 0.13.0, the twelve existing CI cases and a 70-world sweep: every layer line and every geography table line identical; the differences are exactly the schema (five tables, two autoindexes), `meta` (`schema_version`, `generator_version`, the eight `param.*` keys, `stages`) and the rows of the five new tables in worlds that run chronology (section 3).
- **Chronology sweep.** 760 worlds (spec 12 plus spans and race subsets): every hard bound holds in every world, the test writer's reference `check-world` passes on every world, no clamp ever acts, and every statistic is inside the spec's typical range (section 4).
- **Purity.** `--threads` 1, 2, 6 identical; each history flag changes only its own `meta` value, plus tables `age` and `race` for span and races, nothing in geography (section 5).
- **Cross-preset.** All 14 `world-hashes.sh` cases are byte-identical across the five presets, and the extension build's `worldgen-cli` gives the same 14 files. The merge of the audit guards changes no byte of any case file on any of the five presets.
- **Recorded:** `p0-worlds.txt`, `p1-worlds.txt`, the new `p3-worlds.txt`, the five `GOLDEN_CASES` values and the two `stages_pipeline_test.cpp` hashes (section 6). After recording, at the pushed tip, `ctest` passes 23 of 23 on all five presets (`linux-clang-asan` full run, no sanitizer report) and `expect` gives 14 of 14 on each.
- **Extension.** `gdext-headless-check` passes on schema 6 worlds, before and after the merge.
- **Old files.** Schema 5 worlds are refused by `report`, `hash --detail` and `render` with `world schema version 5, this build reads 6`, exit 3.
- **Empty world:** 18 pages, 73728 bytes, as the spec says.

## 1. What was run

| Check | Result |
| --- | --- |
| Build of `100cb30` (`linux-gcc-release`, scratch worktree) | pass; `worldgen-cli 0.12.0 schema 5`. The saved 0.12.0 case files `out/p3-01-val/world-hashes/linux-gcc-release-*.db` have exactly the SHA-256 values of `p0-worlds.txt` and `p1-worlds.txt` at `100cb30`, so they were used as the 0.12.0 side of the twelve cases |
| Configure and build of the five Linux presets at `3ddc942` | pass, all five (3 to 6 min each with ccache, under load) |
| `ctest` before recording, five presets | 21 of 23 entries pass on four presets, 22 of 23 on `linux-clang-asan`; the failures are only world-hash comparisons (section 2) |
| Spec 4.3 and 13, the twelve existing cases, 0.12.0 against 0.13.0 | pass, 12 of 12 (section 3.1) |
| The same, sweep 1024 seeds 0–49 and 512 seeds 0–19 | pass, 70 of 70 (section 3.2) |
| Chronology sweep, spec 12 (section 4) | 760 worlds, 0 hard-bound failures, `check-world` 760 of 760 |
| `--threads` 1, 2, 6 (512 seed 7, 1024 seed 1) | identical bytes; the 1024 file equals CI case `geo-seed-1-1024` |
| History flags (section 5) | 57 variants over three base worlds: each changes only its own `meta` key (and `age`, `race` for span and races) |
| H8 resume | not exposed by the CLI (no resume command; `generate --help`). Covered by the library test `P3-03 14.3 item 17, H8: --until regions, read back, then chronology_stage equals one run` (`worldgen-tests-chronology`, passes on all five presets). At CLI level: `--until regions` and the full world differ only in the five chronology tables and `meta.stages`; `--until chronology` is byte-identical to the default run; `--until history` exits 2 with `stage 'history' given to --until is not implemented yet` |
| `chronology-slow` suites (skipped in `ctest`) | pass on `linux-gcc-release`: `worldgen-tests` 4 cases, 1,324,079 assertions (the full 315,000-schedule property sweep, packed shares, threads 1/2/5 world files); `worldgen-cli-tests` 1 case |
| `p3_chronology_reference.py check` | 128 checks, 0 failures |
| `world-hashes.sh` generate on the five presets, then `compare` (what `local` does) | "all 14 cases identical across 5 presets" |
| Merge of `p3/03-audit-guard` and `p3/03-audit`, rebuild of all five presets, regenerate the 14 cases | every `.db`, `.txt` and `.report.txt` byte-identical to the pre-merge files on all five presets |
| Recording (section 6) | done from the compared output |
| `ctest` after recording, at the merged tip | 23 of 23 on every preset: `linux-gcc-release` 329 s, `linux-clang-release` 264 s, `linux-gcc-debug` 1143 s, `linux-clang-debug` 1346 s, `linux-clang-asan` 3433 s (full run, 0 sanitizer reports); all under load |
| `world-hashes.sh expect`, five presets and `linux-gcc-release-gdext` | 14 of 14 `ok` on each, exit 0 |
| `determinism_lint.py --self-test` and scan | pass: 15 snippet files, 0 failures; 139 C++ and 13 CMake files, 0 findings |
| Extension, `linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON` (own tree `linux-gcc-release-gdext`) | `gdext-headless-check` passes (26 s before the merge, 36 s after); the real world it generates is schema 6 with `stages` ending in `chronology`; its `worldgen-cli` gives the same 14 case files as plain `linux-gcc-release` |
| Schema 5 refusal (section 8) | pass |
| Empty world size | `--until none`: 73728 bytes, 18 pages of 4096, root pages 2 to 18 in the order of spec 15.2, `sqlite_schema` SQL text 2928 bytes, 16 `meta` rows, `report` `[tables]` lists 13 tables |
| Timing (section 7) | stage cost not measurable; geography observation |

## 2. The failing set before recording

Identical on `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug` and `linux-clang-debug` (line numbers of multi-line `CHECK`s differ by one between GCC and Clang). Every failed assertion compares with a recorded world-file hash.

| Test case | File | Compared with |
| --- | --- | --- |
| `cli 9, 12.6: golden world hashes from p0-worlds.txt` | `cli_golden_test.cpp:132`, `:135` | `p0-worlds.txt`, 8 cases |
| `cli P0-03 7.6, amendment 17: …` | `cli_p1_generate_test.cpp:116` | `p0-worlds.txt` `empty-seed-0` |
| `cli P1-00 10.1: --until none writes the P0 empty world, byte for byte` | `cli_p1_generate_test.cpp:92`–`95` | `p0-worlds.txt` |
| `cli P1-00 10.1: --threads accepts 1 to 256 …` | `cli_p1_generate_test.cpp:156` | `p0-worlds.txt` `empty-seed-0` |
| `cli P1-00 10.1, D10, X3: --threads never changes the output …` | `cli_p1_generate_test.cpp:173`, `176`, `177` | `p0-worlds.txt` pattern cases |
| `golden: empty-world hashes match the recorded values (12.4)` | `db_determinism_test.cpp:384` | `GOLDEN_CASES`, 5 values |
| `generate P1-00 10.1: --until none and every thread count give the P0 empty world` | `stages_pipeline_test.cpp:494`, `:507` | the two literal hashes |

These are the same comparisons that failed at 0.12.0 (P3-01 validation, section 2). On `linux-clang-asan` (4561 s, no sanitizer report) only `worldgen-tests-other` failed: `p0-worlds.txt` had been re-recorded in the tree while that run was in progress, and the CLI tests read it at run time, so `worldgen-cli-tests-other` passed against the new file. The values printed by the seven library failures are identical on all five presets (same digest of the `values:` lines); they are the values recorded in section 6.

The merge of `main` into `p3/03-int` (`3ddc942`) left the 0.12.0 values from `main` in `db_determinism_test.cpp` and `stages_pipeline_test.cpp`, next to the engineer's schema 6 change (`dbtest::set_history_defaults(writer)` in `golden_world_hash`, the renamed stage list and the seventh stage in `stages_pipeline_test.cpp`). That is the sensible result: the 0.12.0 values fail as expected and are replaced in section 6; nothing else in those files needed attention.

## 3. What must not change: 0.12.0 against 0.13.0

Comparison script `~/.cache/the-unmapped/out/p3-03-validation/cmp012.py`. For a 0.12.0 and a 0.13.0 world of the same case it requires:

- every `layer` line of `hash --detail` identical, every `table` line of the schema 5 tables identical except `table meta`; the added lines exactly `table age`, `culture`, `god`, `race`, `race_biome`; `schema` differs;
- in `sqlite_schema`, every schema 5 object with identical SQL; the new objects exactly the five tables, `sqlite_autoindex_race_1` and `sqlite_autoindex_culture_1`;
- in `meta`, the added keys exactly the eight `param.*` keys; the changed keys exactly `schema_version` (5 → 6), `generator_version` (0.12.0 → 0.13.0) and, for a geography world, `stages` (`…,regions` → `…,regions,chronology`); `content_hash` and every other key unchanged;
- the five new tables non-empty exactly when `stages` contains `chronology`.

### 3.1 The twelve existing CI cases

| Case | Identical detail lines | `meta` changed | Result |
| --- | --- | --- | --- |
| `empty-phrase`, `empty-seed-0`, `empty-seed-0-512`, `empty-seed-1`, `empty-seed-42`, `empty-seed-max` | 7 each (all tables) | `generator_version`, `schema_version` | equal |
| `pattern-seed-0`, `pattern-seed-42` | 10 each | the same two | equal |
| `geo-phrase-512`, `geo-seed-0-512`, `geo-seed-1-1024`, `geo-seed-42-512` | 26 each (19 layers, 7 tables) | the same two and `stages` | equal |

`report` of `geo-seed-1-1024` differs from 0.12.0 in exactly: `generator_version`, the eight `param.*` rows, `schema_version`, `stages`, the five new `[tables]` rows (`age = 6`, `culture = 6`, `god = 6`, `race = 6`, `race_biome = 45`), `meta = 16`, and the new `[chronology]` section. Every geography line of the report is unchanged.

### 3.2 Sweep

1024 seeds 0–49 and 512 seeds 0–19, 0.12.0 (its own `game/data`; `game/` is unchanged between the two commits) against the 0.13.0 sweep worlds of section 4: **70 of 70 equal**, 26 identical detail lines each. Results: `out/p3-03-validation/base/cmp-1024.txt`, `cmp-512.txt`.

## 4. Chronology sweep (spec 12)

`linux-gcc-release` at `3ddc942` (the binary was frozen as `out/p3-03-validation/bin-3ddc942/worldgen-cli` before the audit merge; three sweep worlds regenerated with the merged tip's binary are byte-identical to the sweep's, and the 14 cases are too). Every world was generated in full: the CLI cannot resume chronology on a stored `--until regions` world (no resume command; spec 4.1 exposes `history_params_from_meta` for the library only), so the subset and span sweeps regenerate the geography each time. Cost: 760 generations, 31,000 s of generation wall time, two at a time with 3 threads each, from 10:20 to 14:44 under load 20 to 37 (1024: 40 s mean per world; 512: 15 s; 2048: 186 s). 4.5 GB of worlds.

| Set | Worlds |
| --- | --- |
| Default settings | 1024 seeds 0–199, 512 seeds 0–49, 2048 seeds 0–9 |
| Spans | 1024 seeds 0–49 at `--history-span 2000` and at `12000` |
| Race subsets, 1024 seeds 0–49 each | `elf`, `goblin`, `dwarf`, `faerie`, `human`, `orc`, `elf,orc`, `goblin,dwarf,faerie,human,orc` (all but elves) |

**Every world passed:** `generate` exit 0, empty stderr, and the checks of `out/p3-03-validation/sweepcheck.py`:

1. **Hard bounds read from the stored rows**, written for this validation without the C++ (P3-00 12.3, spec 10 invariants 1–5 and 8): race ids 1…6 with the data's keys; `present` 1 exactly for the ids of `param.races`, in race order; `awakening_year` NULL exactly for absent races; `creator_god` = `id`; present years strictly increasing, the first at 0, consecutive differences ≥ G and the last ≤ E − G, with G = min(100, E div P); every later race within base ± jmax; ages: P rows with ids 1…P, the i-th present race, start = its awakening year, end = next start − 1, last end = E − 1, each at least G long, lengths summing to E; six gods with the race's camp; 6 cultures and 45 `race_biome` rows; every age name `The Age of <plural>` with an optional numeral; every name of `region`, `age` and `god` ASCII letters with single spaces, `'` and `-`, at most 64 bytes, and all of them pairwise distinct.
2. **The test writer's reference**: `p3_chronology_reference.py check-world DB --data game/data` (called in process), which recomputes the five tables from `meta`, the world's region names and the data, and compares the logical table hashes: **760 of 760 pass**.
3. Statistics, from the stored rows and the reference schedule.

| Set | n | least gap between awakenings | `age_years_min` | shortest age the orcs' | clamp acted | gods drawn at attempt > 0 | numerals | goblin / orc years |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| default 1024 | 200 | 486–705 | 360–480 | 200 | 0 | 145 of 1200 | 0 | 2040–2159 / 5520–5640 |
| default 512 | 50 | 486–705 | 362–480 | 50 | 0 | 41 of 300 | 0 | 2043–2159 / 5520–5638 |
| default 2048 | 10 | 525–705 | 362–472 | 10 | 0 | 9 of 60 | 0 | 2050–2097 / 5528–5638 |
| span 2000 | 50 | 162–235 | 120–160 | 50 | 0 | 41 of 300 | 0 | 681–720 / 1840–1880 |
| span 12000 | 50 | 974–1409 | 724–959 | 50 | 0 | 41 of 300 | 0 | 4086–4318 / 11041–11276 |
| `elf,orc` | 50 | 5520–5638 | 362–480 | 50 | 0 | 41 of 300 | 0 | — / 5520–5638 |
| all but elves | 50 | 486–705 | 362–480 | 50 | 0 | 41 of 300 | 0 | goblin at 0 / 5520–5638 |
| each single race | 6 × 50 | — | 6000 (one age) | — | 0 | 41 of 300 each | 0 | the race at 0 |

Against spec 12:

- **Awakening year of each later race**, typical "uniform over base ± jmax": the jitter d over the 1300 later-race awakenings at span 6000 takes all 121 values of [−60, 60], mean −0.37; at span 2000 all 41 values of [−20, 20] (250 draws), at 12000 164 distinct values of [−120, 120] (250 draws, range −119 to 119). The goblin and orc ranges above sit inside the spec's measured ranges (2040–2160, 5520–5640; 680–720, 1840–1880; 4080–4320, 11040–11280).
- **`age_years_min`**, typical 120–160 / 360–480 / 720–960 at spans 2000 / 6000 / 12000, "always the last age, the orcs'": measured 120–160 / 360–480 / 724–959, and in every world with orcs and another race the shortest age is the orcs'.
- **Least gap between consecutive awakenings**, spec minimum over seeds 0–1999 161 / 483 / 966: measured minima 162 / 486 / 974 over 50 / 260 / 50 worlds. Consistent.
- **Share of clamped schedules: 0 of 760**, as spec 7 C3 states for the shipped data.
- **Gods needing attempt > 0**, spec "4 of 30" on five seeds: 195 of 1560 gods over the 260 distinct (seed, size) worlds of the default sets, 12.5%. No god and no age ever needed a numeral. A seed gives the same six god names whatever the span or the race subset (41 of 300 in every 1024 seeds 0–49 set), as spec 5.2 decides for absent races.
- **Regions per world**: 11–16 (512), 42–59 (1024), 191–204 (2048). No collision of a region name with an age or god name occurred, so the region names never moved a chronology name in this sweep (spec 3, 14.3 item 21).

No statistical oddity: every world has six ages (or one per present race), six gods, and no age shorter than G.

Files: `out/p3-03-validation/sweep/<set>/` (world, `hash --detail`, `report` per world), `sweep/all.jsonl` (one check line per world), `sweep/gen-times.txt`.

## 5. Purity: threads and history flags

`out/p3-03-validation/flags/run.sh`: three base worlds of seed 7 (512 geography, `--until none`, `--test-pattern`), each generated with the default settings and 19 flag variants (`--history-span` 2000, 6000, 12000; `--history-detail` sparse, normal, dense; `--civs-per-race` 1, 4, auto; `--races` elf, `orc,elf`, all, the full list; `--event-intensity` calm, turbulent; `--monster-pressure` low, high; `--divine-activity` distant, meddling), and compared line by line through `hash --detail` and `meta`:

- Every variant changes exactly its own `meta` key, with the canonical text (`--races orc,elf` records `elf,orc`; `all`, the full list and the explicit defaults change nothing at all: byte-identical files).
- In the geography world, `--history-span` and `--races` also change `table age` and `table race`, and nothing else; `god`, `culture` and `race_biome` stay the same (the pantheon does not depend on the span or the subset). Every other flag changes no table. No layer and no geography table changes for any flag.
- In the empty and the test-pattern world, every flag changes only its `meta` key (spec 4.3, amendment 15.7).
- `--threads` 1, 2, 6: identical bytes at 512 seed 7 (`d4075112…`, also equal to the default run) and at 1024 seed 1 (`deb5a86e…`, equal to CI case `geo-seed-1-1024`).

## 6. Recorded hashes

From the compared output: the `file` line of the five presets' identical `hash --detail` files (checked equal to `sha256sum` of each world), and for the library values the failures printed by `worldgen-tests`, identical on all five presets. Each changed for a reason section 3 proves: the CLI worlds and the library empty worlds changed in schema (five tables, 18 pages instead of 11) and `meta` (schema 6, generator 0.13.0, the eight `param.*` keys), and the geography worlds also gained the chronology rows. `GOLDEN_CASES` `empty-seed-0` equals the first `stages_pipeline_test.cpp` hash, as before, since both are the same world.

`worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase 3957b3bdc32b4a75dbcf7defa27c23323f24a1bede1cf50aafc78889e109d664
empty-seed-0 1120fbf0cd9162526787dda50e18cc64d4e66c8bc00566c21e8d3acc9f2802e0
empty-seed-0-512 a2af5692aff8aed39aa149487d02ee8a04f623134030ca378906f9de44c166ac
empty-seed-1 1fad4b58891c0bb833571577916afec53fbdc2ed9832f3a32cd4e15638917b63
empty-seed-42 1e3f204bfbaf338d9ebc3d2ee15def2d0fa9422628e4ea277acc74144a21b185
empty-seed-max 3baf545b1f171f576948f2712020433f09570dd4a58017d823ae6d03b48fef60
pattern-seed-0 9466f02d7036eeb636f6fb2ed530c3cf57fce17eca99f5940aa9fa7604588b7f
pattern-seed-42 b91038ffa9e5da99d73610d81332356949500d7214ec933241acd6c4145ba29c
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 039696541e82a3379e22491ade9e701528fd63bb29be2cf8decf8f7fa99e1111
geo-seed-0-512 83a6e25679a9954d7162abf3386d956909c2a3c0465ffb45869c6302e2b1ece7
geo-seed-1-1024 deb5a86e35a864d0a6d7eb99216ee3c4db94f239650ce0f0f9b1c1597368d497
geo-seed-42-512 e234036c210cbcff87d01debdf7e4851ed7118404f4f2ddf69a665ca1e339d8b
```

`worldgen/tests/golden/p3-worlds.txt` (new):

```
hist-races-elf-orc-1024 41041722aee5237e487687dc0d02137c66ed0ee2b60e51bd2a443bd8164ae2b6
hist-span-2000-512 c65be15b55313c13ce7b9657b9a7a73277d5500a8505884b896d75465603bf2f
```

The two `hist-*` cases agree with spec 7.1: `hist-span-2000-512` (seed 7, span 2000) has awakenings 0, 688, 1114, 1409, 1644, 1879; `hist-races-elf-orc-1024` (seed 3) has orcs at 5602 and ages 0–5601 and 5602–5999; both pass `check-world`.

`GOLDEN_CASES` in `worldgen/tests/db_determinism_test.cpp` (the empty world with the content hash of no data):

| Case | 0.12.0 | 0.13.0 |
| --- | --- | --- |
| `empty-phrase` | `b1e3e377…3d82825a` | `d9af72602f6c3a0d843f137ad1f54408a6d7433d88b4f9faf14e7902a2a9d788` |
| `empty-seed-0` | `d4f8a419…e3ee9d92` | `c5326b79c220b1e615ff334022a783113bf92ae45b7d052dcc49e0870074fed2` |
| `empty-seed-1` | `e3a5041c…9598a86f` | `6bafbea54324e1160bc849fcced4f3a3812ac6f762977ab79d9b8c3b2d9887ae` |
| `empty-seed-42` | `64f930c0…f6d212a9` | `e54a061a15bafdb461f2b2fcc175615d746c66984ed28626298946a94f4c7b89` |
| `empty-seed-max` | `b66dd726…221b1335` | `31a5de190f791fbef0dd8b1a757ad2c096da96793e8534ea81a4361910062a38` |

`worldgen/tests/stages_pipeline_test.cpp`, test case `generate P1-00 10.1: --until none and every thread count give the P0 empty world`:

- the empty world of seed 0 becomes `c5326b79c220b1e615ff334022a783113bf92ae45b7d052dcc49e0870074fed2` (was `d4f8a419…`);
- the test pattern of seed 0 becomes `6169c66020b10a16f5f46b105c497be51cee254e976b3d243895cef6add86551` (was `c3f8028b…`).

The comments beside those values now say generator 0.13.0 and schema version 6. No per-stage golden file was touched: `p1-01` to `p1-06`, `p3-01-data.txt`, `p3-02-names.txt` and `p3-03-chronology.txt` pass as committed on every preset.

## 7. Timing

`linux-gcc-release`, seed 1, wall time in seconds, 0.12.0 and 0.13.0 in alternating pairs (`out/p3-03-validation/timing.txt`, `timing2.txt`). Taken after the sweep, at load 4 to 13 (another agent's tests were still running).

| Run | 0.12.0 | 0.13.0 | load |
| --- | --- | --- | --- |
| 512, default threads, three pairs | 2.90, 2.92, 2.87 | 3.02, 2.98, 3.05 | 7 |
| 1024, default threads, three pairs | 10.3, 11.3, 9.9 | 13.2, 12.6, 10.9 | 8–9 |
| 2048, default threads, two pairs | 42.0, 53.5 | 48.3, 66.6 | 8–13 |
| `--until none`, mean of 20 | 0.034 | 0.037 | 7 |

**The stage itself costs nothing measurable**, as spec 12 expects (budget 0.1 s): 0.13.0 `--until regions` against `--until chronology`, alternating, 512: 5.04 / 4.82, 4.60 / 4.79, 4.54 / 4.67 s; 1024: 12.1 / 10.8, 11.2 / 11.4, 11.4 / 10.9 s. The chronology run is not slower than the geography run it extends; the scatter is the load.

**Observation (for the lead, not a defect of the stage): the geography stages are about 9% slower in the 0.13.0 release binary than in 0.12.0.** Single thread, `--until regions`, user time, four alternating pairs each, load 4 to 7, very stable: 512: 5.64–5.68 s (0.12.0) against 6.18–6.20 s (0.13.0); 1024: 23.0–23.1 s against 24.8–25.2 s. Bisected by `--until`, the difference is already there for `elevation` alone (4.24–4.30 against 4.64–4.66 s at 512) and grows stage by stage. Yet the object files of every geography stage, of `geo/` and of the core are byte-identical between the two build trees (identical compile flags); only `data_root`, the `db/` files, `generate`, `report`, `regions`, `stage` and `version` objects differ, none of them in an elevation inner loop. The pre-merge and post-merge 0.13.0 binaries time the same. The likely cause is code placement in the linked executable (the addresses of the hot `geo::` functions modulo 64 differ between the two binaries); it could not be confirmed with a profiler here (`perf_event_paranoid` is 4). It does not change any output. If the README quotes generate times, they should be re-measured on a quiet machine, and the engineer may want a quick profile to rule out a real regression in the shared writer path.

## 8. Old files

A schema 5 world written by the 0.12.0 build (CI case `geo-seed-0-512` and `empty-seed-0` from the saved 0.12.0 files, whose hashes are those of `100cb30`'s golden files), read by the 0.13.0 `linux-gcc-release` build:

| Command | Exit | stderr |
| --- | --- | --- |
| `report --db <file>` | 3 | `worldgen-cli: error: world schema version 5, this build reads 6` |
| `hash --db <file> --detail` | 3 | the same |
| `render --db <file> --layer elevation` | 3 | the same |
| `render --db <file> --composite map` | 3 | the same |

Nothing on stdout in any case. Plain `hash --db <file>` (no `--detail`) prints the SHA-256 and exits 0, as P0-04 specifies (the file need not be a world).

## 9. Defects and observations

No defect.

- Observation, section 7: geography about 9% slower in the 0.13.0 release binary with identical geography object code; no output change. Lead to decide whether to ask for a profile.
- Observation, low, carried over from the P3-01 validation and still open: `CAPTURE(c.name)` in `db_determinism_test.cpp` prints `c.name := 1` instead of the case name, so a `GOLDEN_CASES` failure does not say which case failed (the order identified them here). One-line fix for the test writer: `CAPTURE(std::string_view(c.name))`. Not changed here.
- Cost note: the CLI has no way to resume chronology on a stored geography world, so a subset or span sweep regenerates the geography every time (section 4: 700 generations at 1024 for what is, on shipped data, a function of the seed and the settings). P3-04 will want a resume path for its sweeps; the library already has `history_params_from_meta`.

## 10. Files

Under `~/.cache/the-unmapped/out/`:

- `p3-03-validation/`: build logs (`build-*.log`, `build3-*.log` after the merge), `ctest1-*.log` (before recording), `ctest2-*.log` (after recording, before the merge; four presets), `ctest3-*.log` (at the pushed tip, five presets), `expect*-*.log`, `wh*-gen-*.log`; `cmp012.py`, `cmpflag.py`, `sweepcheck.py`, `stats.py`, `gen-one.sh`; `cases-012-vs-013.txt`; `base/` (0.12.0 sweep worlds and comparisons); `sweep/` (the 760 worlds, `all.jsonl`, `gen-times.txt`); `flags/result.txt`; `old/` (the schema 5 checks and the empty world); `timing.txt`, `timing2.txt`; `gdext-check.log`, `gdext-check3.log`; `world-hashes-3ddc942/` (the case files before the audit merge); `bin-3ddc942/` (the sweep's binary).
- `p3-03-val/world-hashes/`: the 14 case files of the five presets and of `linux-gcc-release-gdext`, at the pushed tip.
