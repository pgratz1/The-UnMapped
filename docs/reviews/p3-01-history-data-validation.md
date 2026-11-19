# Validation: P3-01 history data and settings (generator 0.12.0, schema 5)

Validation specialist, 2026-11-19. Commit validated: `8558935` (`origin/p3/01-int`, "Merge p3/01-int-fix (tests expect generator 0.12.0)"), with the implementation, the independent tests, the tightened tests, the revised specs (revision notes 1 and r2) and the 0.12.0 version strings. Baseline: `e9ca9c5` (generator 0.11.0, the last `main` commit before the P3 work), built in its own scratch worktree. Spec: `docs/specs/p3-01-history-data-settings.md`, section 7: the merge changes only `meta.content_hash` and `meta.generator_version`; no layer, table or other `meta` key changes. Machine: 6-core Linux desktop, **under heavy load from other agents' builds throughout** (load average 13 to 31). Windows MSVC is checked by CI.

**Verdict: pass.** No defect.

- **Builds.** All five Linux presets build with warnings as errors.
- **Tests before recording.** On all five presets the only failures are the comparisons with recorded world-file hashes (section 2). Every per-stage golden file (`p1-01` to `p1-06`, `p3-01-data.txt`) passes unchanged.
- **Section 7 holds.**
  - Twelve CI cases, 0.11.0 against 0.12.0: every layer and table line of `hash --detail` is identical. `meta` differs in exactly `content_hash` and `generator_version`.
  - Sweep of 70 worlds (1024 seeds 0–49, 512 seeds 0–19): the same in all 70.
  - No history flag or settings key reaches the world, and `--threads` 1, 2 and 6 agree.
- **Cross-preset.** `world-hashes.sh local` over the five presets: all 12 cases are identical. The extension build (`linux-gcc-release-gdext`) gives the same 12 files too.
- **Recorded:** `p0-worlds.txt`, `p1-worlds.txt`, the five `GOLDEN_CASES` values and the two `stages_pipeline_test.cpp` hashes (section 4). After recording, `ctest` passes on all five presets and `expect` gives 12 of 12 on each.
- **Content hash.** `p3_data_reference.py check` gives 97 checks and 0 failures. Every generated world carries `content_hash` `be84128b…2f74f65e`, the `game/data` value of `p3-01-data.txt`.
- **Cost.** Loading three more kinds costs nothing measurable. A full `--until none` run, data loading included, takes 19.5 ms with 0.11.0 and 20.0 ms with 0.12.0 (mean of 20 each). The full-generation timings are paired 0.11.0/0.12.0 runs under load (section 6).
- **Extension.** `gdext-headless-check` passes.

## 1. What was run

| Check | Result |
| --- | --- |
| Build of `e9ca9c5` (`linux-gcc-release`, scratch worktree `p301val-base`) | pass; `worldgen-cli --version` prints 0.11.0. Its twelve case files equal the committed 0.11.0 `p1-worlds.txt`, so they are the 0.11.0 output |
| Configure and build of the five Linux presets at `8558935` | pass, all five (2.5 to 4 min each with ccache, under load) |
| `ctest` before recording, five presets | 18 of 20 entries pass on each. The two failures (`worldgen-tests-other`, `worldgen-cli-tests-other`) are only world-hash comparisons (section 2) |
| Spec section 7, the twelve `world-hashes.sh` cases (`cmpworld.py`) | pass, 12 of 12 (section 3.1) |
| Spec section 7, sweep: 1024 seeds 0–49, 512 seeds 0–19 | pass, 70 of 70 (section 3.2) |
| History flags and settings keys (section 3.3) | pass: 108 worlds, three byte-identical groups |
| `--threads` 1, 2, 6 (1024 seed 1, 512 seed 3) | pass: identical bytes. The 1024 file equals CI case `geo-seed-1-1024` |
| `world-hashes.sh local` over the five presets | pass: "all 12 cases identical across 5 presets" (25 min under load) |
| Extension build's `worldgen-cli` (`world-hashes.sh generate linux-gcc-release-gdext --build-dir …`) | the 12 `hash --detail` files are byte-identical to `linux-gcc-release`'s |
| Recording (section 4) | done from the compared output |
| `ctest` after recording | pass. The full set (20 of 20) on `linux-gcc-release` (202 s), `linux-clang-release` (230 s), `linux-gcc-debug` (508 s) and `linux-clang-debug` (517 s). On `linux-clang-asan`, after the rebuild, the entries that hold the changed tests and goldens passed: `worldgen-tests-other`, `worldgen-cli-tests-other` and both partition checks, 4 of 4 with no sanitizer report. Its other 16 entries passed in the first round, which had the same sources apart from the two edited test files of `worldgen-tests-other` |
| `world-hashes.sh expect`, five presets | 12 of 12 `ok` on each, exit 0 |
| `determinism_lint.py --self-test` and scan | pass: 13 snippet files with 0 failures; 125 C++ and 13 CMake files with 0 findings |
| `p3_data_reference.py check` | 97 checks, 0 failures |
| `content_hash` of generated worlds against `p3-01-data.txt` | equal in all 12 CI cases on every preset, and in every sweep world |
| `linux-gcc-release` with `-DUNMAPPED_BUILD_GDEXT=ON` (own build tree), `gdext-headless-check` | pass (31 s; 18 min clean extension build under load) |
| Timing | section 6 |

## 2. The failing set before recording

The set is the same on `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug` and `linux-clang-debug`. On `linux-clang-asan` it is the library rows only, for the reason given below the table. Every failed assertion is a comparison with a recorded world-file hash; no other assertion fails.

| Test case | File | Compared with |
| --- | --- | --- |
| `cli 9, 12.6: golden world hashes from p0-worlds.txt` | `cli_golden_test.cpp:132`, `:135` (`hash --expect`, exit 5 = mismatch) | `p0-worlds.txt`, 8 cases |
| `cli P0-03 7.6, amendment 17: …` | `cli_p1_generate_test.cpp:116` | `p0-worlds.txt` `empty-seed-0` |
| `cli P1-00 10.1: --until none writes the P0 empty world, byte for byte` | `cli_p1_generate_test.cpp:92`–`95` | `p0-worlds.txt` |
| `cli P1-00 10.1: --threads accepts 1 to 256 …` | `cli_p1_generate_test.cpp:154` | `p0-worlds.txt` `empty-seed-0` |
| `cli P1-00 10.1, D10, X3: --threads never changes the output …` | `cli_p1_generate_test.cpp:171`, `174`, `175` | `p0-worlds.txt` pattern cases |
| `golden: empty-world hashes match the recorded values (12.4)` | `db_determinism_test.cpp:383` | `GOLDEN_CASES`, 5 values |
| `generate P1-00 10.1: --until none and every thread count give the P0 empty world` | `stages_pipeline_test.cpp:483`, `:496` | the two literal hashes |

`ctest` times before recording: `linux-gcc-release` 169 s, `linux-clang-release` 177 s, `linux-gcc-debug` 804 s, `linux-clang-debug` 950 s, `linux-clang-asan` 2643 s, with no sanitizer report. In the asan run, `worldgen-cli-tests-other` started after `p0-worlds.txt` had been re-recorded in the source tree, and it passed: the CLI tests read the golden file at run time. So on that preset only `worldgen-tests-other` (the compiled-in values) failed in round 1. These ran under load and alongside the sweep, so they say nothing about the cost of the change.

The values printed by the library tests are identical on all five presets (the `values:` lines of the seven failures have the same digest everywhere). They are the values recorded in section 4.

## 3. Spec section 7: what the merge changes in a world

The comparison script is `~/.cache/the-unmapped/out/p3-01-validation/cmpworld.py`. For a 0.11.0 and a 0.12.0 world of the same case, it checks three things:

- Every `hash --detail` line except `file` and `table meta` is equal. That covers `schema`, every `table` (including the catalogue table `layer`) and every `layer`.
- The set of detail lines is the same.
- Read through SQLite: the `meta` columns are the same, the key set is the same, and the keys whose values differ are exactly `content_hash` and `generator_version`.

### 3.1 The twelve CI cases

0.11.0 files: `out/p301val-base/world-hashes/`. 0.12.0 files: `out/p3-01-val/world-hashes/`.

| Case | Detail lines compared | `meta` keys changed | Result |
| --- | --- | --- | --- |
| `empty-phrase`, `empty-seed-0`, `empty-seed-0-512`, `empty-seed-1`, `empty-seed-42`, `empty-seed-max` | 10 each | `content_hash`, `generator_version` (0.11.0 → 0.12.0) | equal |
| `pattern-seed-0`, `pattern-seed-42` | 13 each | the same two | equal |
| `geo-phrase-512`, `geo-seed-0-512`, `geo-seed-1-1024`, `geo-seed-42-512` | 29 each (19 layers, 8 tables, schema) | the same two | equal |

`report` text differs in exactly two lines in every case: `content_hash = 1543a4ae…83f9c175` becomes `be84128b…2f74f65e`, and `generator_version = 0.11.0` becomes `0.12.0`.

### 3.2 Sweep

Each world was generated with 0.11.0 (its own `game/data`) and with 0.12.0 (the new `game/data`), and compared as above. The sweep covered 1024 seeds 0–49 and 512 seeds 0–19, using the `linux-gcc-release` binaries with default threads, two worlds at a time.

- **70 of 70 equal.** Each world has 29 detail lines, and only `content_hash` and `generator_version` change in `meta`.
- No `generate` failed, and stderr is empty.
- Results: `out/p3-01-validation/sweep/result.txt`, with the detail files of both versions beside it.

The sweep changes no geography, so the P1 statistics of `p1-06-regions-validation.md` stand unchanged and are not repeated here.

### 3.3 History flags and settings keys

`out/p3-01-validation/flags.sh` ran three base worlds:

- 512 seed 7 with geography;
- seed 7 with `--until none`;
- seed 7 with `--test-pattern`.

Each base world was generated 36 ways:

- the default;
- 20 flag variants: every valid value of `--history-span` (2000, 6000, 12000), `--history-detail`, `--civs-per-race` (1, 4, `auto`), `--races` (`elf`, `orc,elf`, `all`, the full list), `--event-intensity`, `--monster-pressure` and `--divine-activity`, and one line with all of them;
- 14 settings files: each `[world]` key alone, and one file with all seven;
- one settings file with flags beside it (`--history-span 3000 --races elf`).

All 108 runs exit 0. Within each base world all 36 files are byte-identical:

| Base world | SHA-256 of all 36 files |
| --- | --- |
| geography 512 | `9cadfa6d…a9f3faf` |
| `--until none` | `98e4b7d1…53ff4eb9` |
| `--test-pattern` | `2493c2b2…a908feec` |

As a spot check of the error paths, the CLI rejects four inputs with exit 2 and the messages of spec 6.2:

- `--divine-script x`;
- `--races bogus` (the ids listed in race order);
- `--history-span 1999`;
- `--races elf,elf`.

The tests cover the full error matrix.

## 4. Recorded hashes

All values come from the compared output: the `file` line of the five presets' `hash --detail` files, which are identical. For the library values, they come from the failures printed by `worldgen-tests`, which are identical on all five presets. Every one of them changed for a reason section 3 proves:

- The CLI worlds changed only in `content_hash` and `generator_version`.
- The library worlds, which carry the content hash of no data, changed only in `generator_version`.

The `GOLDEN_CASES` value for `empty-seed-0` equals the first `stages_pipeline_test.cpp` hash, as before, since both are the same world.

`worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase 381e91ad4aeb8adf9f162bb4a7b19c12e1d83b30002e7ba0dd53f47c8b3e7122
empty-seed-0 e1f12b44455a75a26b8aae79824c28c8a08dc9b242ab3d8e60e03df35476aadf
empty-seed-0-512 01c525a7a9bee2869380116583d35edf0d3dc8e4c2742f931553e03a52bbda6e
empty-seed-1 b170b21fa29b73b00e7aa10d4fbdbf1e368a4473f36bcaa9e2039da85c952416
empty-seed-42 7ce809fbbbbab3c02d555b3b0cf9feb314eb20e2151571eb9af28c0fefb5732f
empty-seed-max fc5e34a963e90118f49f3a9a3a76b96a493c4104990d5c68960408eaa58e5084
pattern-seed-0 c1fed2f2197ecb8dbd3e35d155006be582eca4f7cb8b9c5be5b78e2a0241f52f
pattern-seed-42 164020929581d8122dd50636223935b086d21ae131cbb08fca65306f2999001b
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 02a9b70c88a04f93f01831839df0efb8d89748f3bed3b26982af7af51c461faa
geo-seed-0-512 610f7aa5fd835f7091776288da50bc47ad5ae7766cbc43f7dfc85c07e4419827
geo-seed-1-1024 2679d8f4ca676431ce3f39220ce8fdf22f665dbc50ad40a69898d3c4ebd33d11
geo-seed-42-512 dd76b4cf8a44f1aefc502c8f006977a77e756814fb4b33173395471f3d46d4b2
```

`GOLDEN_CASES` in `worldgen/tests/db_determinism_test.cpp` (the empty world with the content hash of no data):

| Case | 0.11.0 | 0.12.0 |
| --- | --- | --- |
| `empty-phrase` | `43b52e47…fdc53c69` | `b1e3e3775b590c22bff95f93f434cad66eb2aaaca28b53659d90e3c33d82825a` |
| `empty-seed-0` | `93e5c92e…ddebff63` | `d4f8a4198a329311a6332349b145e2a6f3fb63c455dd6e6d9d674ebae3ee9d92` |
| `empty-seed-1` | `37c0cd9a…91f7d856` | `e3a5041c1354ca859f0dd55968d203c2736b72d4b75fa387b3176c319598a86f` |
| `empty-seed-42` | `5c74c03e…732a6017` | `64f930c02162f9fdd17c07dd02356ff5844bdcc5ce51362e7035f449f6d212a9` |
| `empty-seed-max` | `cd114fed…835cec8d` | `b66dd726b595e758fe18cf9dba7b72f371064779904afb5e4e4ee805221b1335` |

`worldgen/tests/stages_pipeline_test.cpp`, test case `generate P1-00 10.1: --until none and every thread count give the P0 empty world`:

- the empty world of seed 0 becomes `d4f8a4198a329311a6332349b145e2a6f3fb63c455dd6e6d9d674ebae3ee9d92` (was `93e5c92e…`);
- the test pattern of seed 0 becomes `c3f8028b7e0a7eb731fc9583dbf6c228197aefd4f9a3bbc19c8cb9e24d46f129` (was `76a547b5…`).

The comments beside those values now say generator 0.12.0. No per-stage golden file was touched. `git diff e9ca9c5 8558935 -- worldgen/tests/golden/` adds only `p3-01-data.txt`, so `p1-01` to `p1-06` are the 0.11.0 files, and they pass on every preset.

## 5. Content hash

- `python3 -B worldgen/tests/reference/p3_data_reference.py check`: 97 checks, 0 failures.
- In every generated 0.12.0 world, `meta.content_hash` is `be84128b97d680b504ebbacf3143adf58504c713b36994561b5cb6de2f74f65e`. That is the `game/data` line of `p3-01-data.txt`.
- The 0.11.0 value was `1543a4aef55105d5df66225432e37a7f39e50846e53741b86f084da283f9c175`.
- `p3-01-data.txt` gives the same hash for `worldgen/tests/data/geo` as for `game/data`. The fixture root is meant to be a copy of the shipped data, and the reference computes it from the files on its own.

## 6. Timing

Machine under heavy load (load average 13 to 22 during the runs), so the absolute numbers are not comparable with the 0.11.0 figures (2.4–2.7 / 8.8–9.3 / 34 s, quiet machine). Instead, 0.11.0 and 0.12.0 ran in interleaved pairs, so both saw the same load. `linux-gcc-release`, seed 1, default threads, wall time in seconds:

| Run | 0.11.0 | 0.12.0 |
| --- | --- | --- |
| `--until none` (data loading, the empty world), mean of 20 | 0.0195 | 0.0200 |
| `--until none`, five pairs | 0.040–0.055 | 0.041–0.082 |
| 512, three pairs | 8.74, 9.19, 9.17 | 8.40, 9.55, 10.36 |
| 1024, three pairs | 34.9, 33.5, 27.1 | 34.3, 20.9, 24.5 |
| 2048, two pairs | 80.2, 63.2 | 94.6, 61.8 |

The pairs scatter by up to 50% with the load, in both directions. The one measurement that isolates the change is the time of the data load in a `--until none` run. That run reads, checks and hashes all five kinds, and it costs 20 ms in either version. The difference, 0.5 ms, is below the noise. Loading three more kinds therefore costs nothing measurable against seconds of generation. A quiet-machine repeat of the 512 / 1024 / 2048 runs would only confirm this; the lead may want one before quoting numbers in the README.

## 7. Extension

`linux-gcc-release` was configured with `-DUNMAPPED_BUILD_GDEXT=ON` in its own build tree, `build/p3-01-val/linux-gcc-release-gdext`. `ctest -N` lists `gdext-headless-check`, and the check passes (31 s, `UNMAPPED-CHECK: PASS`):

- every layer and the composites `map`, `biomes` and `regions` are listed;
- the pixels equal `worldgen-cli render`;
- the legends are as before.

The viewer does not read `game/data` itself. Only the check's `generate` step does (`-DDATA_DIR=<repo>/game/data`), and it accepts the new required kinds. The run copy of the project links `game/data` with its new `races/`, `cultures/` and `history/` folders, and the import is unaffected.

The extension build's `worldgen-cli` gives the same 12 case files as plain `linux-gcc-release`.

## 8. Defects and observations

No defect.

Observation (low, pre-existing since P0, test output only): `CAPTURE(c.name)` in `db_determinism_test.cpp` prints `c.name := 1` instead of the case name, because `name` is a `const char*` member of a constexpr struct. So a `GOLDEN_CASES` failure does not say which case failed. Its order identified it here. A one-line fix for the test writer: `CAPTURE(std::string_view(c.name))`. Not changed here.

## 9. Files

Under `~/.cache/the-unmapped/out/`:

- `p3-01-validation/`:
  - build and `ctest` logs (`build-*.log`, `ctest1-*.log`, `ctest2-*.log`);
  - `cmpworld.py`, `cases-old-vs-new.txt`;
  - `sweep/` (detail files of both versions, `result.txt`);
  - `flags/` (`result.txt`, `legend.txt`, settings files);
  - `timing.txt`, `world-hashes-local.log`, `gdext.log`.
- `p301val-base/world-hashes/`: the 0.11.0 case files.
- `p3-01-val/world-hashes/`: the 0.12.0 case files of the five presets and of `linux-gcc-release-gdext`.
