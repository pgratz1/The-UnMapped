# Determinism audit: P3-03 chronology (generator 0.13.0, schema 6)

Auditor: determinism auditor, 2026-11-19. Change audited: `git diff origin/p3/02-int origin/p3/03-int` at the tip `b8d57ad` ("Merge branch 'p3/03-chronology' into p3/03-int"). That tip includes the engineer's last commit `e420e64` (input check D0: a content set with no race definition is a data error of `check_history_data`). The first pass was made at `14f6f0a`; the D0 commit changes only `worldgen/src/history/data_checks.cpp` and its header, and every run in section 4 was repeated or confirmed at `b8d57ad`. Specs: `docs/specs/p3-03-chronology.md` on that branch, and `docs/specs/p3-00-history-overview.md` 4.5 (rules H1 to H8) and 4.6 (keyed draws). Line numbers are those of `b8d57ad`.

Scope:
- `worldgen/src/stages/chronology.cpp`, `worldgen/include/worldgen/stages/chronology.hpp`: tuning, settings and data checks (C1), the schedule (C2, C3), ages (C4), gods (C5), the write (C6).
- `worldgen/src/history/params.cpp`, `history/params.hpp`: `HistoryParams`, `history_params_from`, `history_meta_entries`, `history_params_from_meta`, `history_params_problem`.
- `worldgen/src/history/data_checks.cpp`, `history/data_checks.hpp`: D0 to D5, `age_name_text`.
- `worldgen/src/db/`: schema 6 DDL, the eight `meta` keys (`schema.hpp`, `schema.cpp`), the five row writers and `set_param_text` (`writer.cpp`, `writer_state.hpp`), the reader's validation of the eight keys (`reader.cpp`), `bind_null` (`sqlite_util.cpp`).
- `worldgen/src/stages/stage.cpp`, `pipeline.hpp`, `stage.hpp`: `StageContext::history_params()`, `regions()`, the new writers, the stage list.
- `worldgen/src/generate.cpp`: the history settings check and `write_history_params` for every world.
- `worldgen/src/inspect/chronology.cpp`, `inspect/report.cpp`: the `[chronology]` section and its corrupt-data checks.
- `worldgen/include/worldgen/core/roman.hpp`, `worldgen/src/core/roman.cpp`, and the callers in `regions.cpp` and `history/names.cpp`.
- `worldgen/include/worldgen/core/streams.hpp`: `hist.chronology.awakening`.
- `tools/worldgen-cli/cli.cpp`: the data checks, `--until`, `history_params_from`.
- `tools/ci/world-hashes.sh`: the two `hist-*` cases and `p3-worlds.txt`.

Code the change depends on was also read: `core/fx.cpp` (`mul_int`, `round_int`), `history/draw.cpp` (`draw`, `pick`), `history/names.cpp` (`choose_name`, `NameRegistry`), `data/settings.cpp` (`race_ids_in_order`), `data/load_error.cpp` (`sort_errors`), `data/kinds_history.cpp` (the race and chronology schemas and `race_awakening_order`), `inspect/hashes.cpp` (the NULL value tag).

**Verdict: pass with notes.** No blocking finding and no should-fix finding. Every value the stage writes is an integer or ASCII text computed from the seed, the eight settings, the loaded data and the region names, with exact `Fx` arithmetic and keyed draws; nothing depends on compiler, standard library, thread count, container iteration or evaluation order. The 14 cases of `world-hashes.sh` are byte-identical between `linux-gcc-release` and `linux-clang-release`, and the independent reference agrees with the C++ on every world checked, including edited data at the edges of the tuning ranges where the clamp acts. Two advisories, eight notes. No lint change was needed (section 3).

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts. **Should-fix** breaks or leaves open a stated contract and needs action before merge. **Advisory** is fragile but currently safe. A **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

**A1. The schedule's well-formedness rests on the race `order` range, which nothing near the schedule checks.**
- `chronology.cpp:201` computes G = min(`awakening_min_gap_years`, E div P). The proof in spec C3 that `lo` ≤ `hi` holds for any G ≥ 0, but the strict order of awakening years and `end_year` ≥ `start_year` of every age need G ≥ 1, that is P ≤ E.
- Today P ≤ 6, because `order` is `integer(1, 6)` (`kinds_history.cpp:332`) and orders are unique, and E ≥ 2000. So G ≥ 333 and the case cannot occur.
- If a later data spec widens `order`, or E could fall below P, G would be 0, two races could share a year, and an age would be written with `end_year` = `start_year` − 1. `StageWriter::write_age` (`writer.cpp:742-753`) checks only the ascending id, so the row would go into the world without complaint. The output would still be the same on every platform; it would just be wrong.
- Fix asked (engineer, low priority): a seam guard in `awakening_years` (`chronology.cpp:213`, after `awakening_bounds`), fatal if `b.gap < 1`; optionally `write_age` fatal when `end_year < start_year`, as `report` already treats it as corrupt (`inspect/chronology.cpp:116`).

**A2. In one run the stage sees the in-memory `HistoryParams`, not the values read back from `meta`. H8 holds only because the round trip is exact.**
- `generate.cpp:126` hands `params.history` to the pipeline, and `StageContext::history_params()` returns it (`pipeline.hpp:45`, `pipeline.hpp:56`). A resumed run would build the value with `history_params_from_meta`. The two are equal because `history_meta_entries` writes every field of `HistoryParams` and `history_params_from_meta` (`params.cpp:129-146`) parses every one back, and the schema test "history_params_from_meta inverts history_meta_entries for every value" (`history_chronology_schema_test.cpp:285`) checks this.
- The risk is a later field added to `HistoryParams` and read by a stage but not recorded in `meta` (or recorded in a lossy form). A one-run world would then differ from a resumed one, and nothing in the build would notice except that test, if it is extended.
- Fix asked (engineer or spec writer of P3-04, no change needed now): either let the pipeline hand stages `history_params_from_meta`-equivalent values (parse the canonical text that was written), or keep the round-trip test exhaustive whenever a field is added, and say so in the comment above `struct HistoryParams` (`history/params.hpp`). The same applies to `StageContext::regions()` (`stage.cpp:95-97`), which returns the in-memory rows: equal to the stored rows today, since text is bound byte for byte and every column is an integer.

### Notes

**N1. `awakening_years` is exact integer arithmetic with no overflow at any accepted input.** `chronology.cpp:196-232`.
- **Ranges.** Shares are `fixed("0", "1")`, so |raw| ≤ 2^32 (`kinds_history.cpp:337`). `awakening_jitter_frac` is `fixed("0", "0.05")`, `awakening_min_gap_years` is `integer(1, 2000)` (`kinds_history.cpp:409-410`). E is in [2000, 12000] (checked in `generate_world`, at the stage entry, and by the reader).
- **Products.** `mul_int(share, E)` ≤ 2^32 × 12000 < 2^46, and `mul_int` is itself checked (`fx.cpp:147-153`). `round_int` is RHA on the magnitude (`fx.cpp:76-79`): the same as the reference's half-away rounding, independent of sign and of the platform's right shift of negatives.
- **Bounds.** jmax ≤ round(0.05 × 12000) = 600, so `choices` = 2 jmax + 1 ≤ 1201, well inside `pick`'s [1, 2^32]; `pick(u, n) = ((u >> 32) × n) >> 32` is below n and below 2^64 (`draw.cpp:23-29`). `d`, `base`, `lo`, `hi` and the years are within [−600, 12600] in `int64`; G × (P − i + 1) ≤ 2000 × 6.
- **Casts.** `2 * b.jmax + 1` is cast to `uint64` only after the `b.jmax < 0` guard (`:214`); `pick`'s result (< 1201) is cast back to `int64` exactly. `present_ids[k]` (an `order` in 1 … 6) is cast to `uint64` for the draw.
- **Clamp.** `std::min(std::max(base + d, lo), hi)` (`:229`) is C3's formula; with `lo` ≤ `hi` (A1) the order of `min` and `max` matters only in the case the proof excludes.
- **Observed.** Edited data with jitter 0.05, gap 2000 and shares 0, 0.001, 0.002, 0.998, 0.999, 1 (every age clamped) at E = 2000, 7777 and 12000 and seeds 0, 42 and 2^64 − 1, and with jitter 0.05, gap 1 and a goblin share of 10^-9 (base 0, clamped to `lo` = 1) at E = 2000 and 12000: the reference's `check-world` agrees with the C++ on all 15 worlds.

**N2. One keyed draw per race, keyed by race id: the schedule is independent of iteration order and of the subset except through the clamp.** `chronology.cpp:217`, `:225`. The stream key is `stream_key(seed, "hist.chronology.awakening")`, a new registered name (`streams.hpp`, appended to `all_stream_names`, which only tests read). The draw is `history::draw(K_aw, 0, race id, 0, 0)` as spec C3 states: year 0 in w0, race id in a. It is a pure hash (rule H4), so no draw depends on how many came before. The first present race draws nothing and gets year 0. `--races elf,orc` and `--races goblin,dwarf,human` give each race the same draw as in the full world (worked examples 7.1, reproduced by the reference's `check`), and `--races orc,elf` is resolved and recorded as `elf,orc`.

**N3. The name registry is filled in the spec's order and never iterated.** `chronology.cpp:259-285` adds every region name in id order, then each age name (C4) in age id order with the `roman` fallback; `:287-322` then names the gods in race id order through `choose_name` with c = `culture.id` and e = `race.id` (P3-02 4.3, 4.5). `NameRegistry` is an ordered set used for membership only (P3-02 audit N6), so only its content matters. Every loop in the stage runs over a `std::vector` in a defined order: `races_in_order` (stable sort by `order` of definitions already in id order, `settings.cpp:396-413`), the present list in `params.races` order (checked strictly increasing in `order`, `chronology.cpp:106-110`), and culture definitions in canonical id order.

**N4. `std::stable_sort` on `race_biome` is fully determined.** `chronology.cpp:163-164` sorts one race's rows by biome code. Codes are unique per biome (`biome_code_unique`) and listed biomes are unique per race (`race_biomes_unique`), so the comparator is a strict total order on the rows and any sort, stable or not, gives the same sequence. The writer then refuses a key that is not strictly above the previous one (`writer.cpp`, `write_race_biome`).

**N5. Schema 6 is byte for byte the spec, the same in every kind of world, and holds no REAL.**
- The five DDL strings in `schema.hpp` equal the five statements of spec 5.1 and `docs/specs/schema.md` (compared mechanically), and `DDL_STATEMENTS` (`schema.hpp:136`) executes them ninth to thirteenth. All five are `STRICT`; `race_biome` is `WITHOUT ROWID` with key (race, biome). The only nullable column is `race.awakening_year`.
- `sqlite_schema` of a generated world lists the tables and the autoindexes `sqlite_autoindex_race_1` and `sqlite_autoindex_culture_1` in that order. The `hash --detail` `schema` line is the same (`90ddcf38…`) for the default world, `--until regions`, `--until none` and `--test-pattern`. The empty world is 73728 bytes (18 pages of 4096), as spec 5.1 predicts.
- `awakening_year` NULL is bound with `sqlite3_bind_null` (`writer.cpp:684`) and hashed with the value tag only (`hashes.cpp:40`). `write_race` refuses a row whose `awakening_year` is not set exactly when `present` is 1.
- Rows are written in id order (race, culture, god, age) and (race, biome) order, each checked strictly ascending, in the stage's single transaction (`chronology.cpp:347-361`). The B-tree contents therefore do not depend on insertion order anyway, but the order is defined.
- The lint's `sql-real` and `sqlite-value-type` rules pass on the new SQL and bind calls.

**N6. The eight `meta` keys: canonical text in, canonical text out, in ascending key order.**
- `history_meta_entries` (`params.cpp:111-127`) writes the eight keys in ascending byte order with integer text from `db::format_i64` (no locale) and names from the enum tables; `generate_world` writes them for every world after `param.map_size` through `set_param_text` (`generate.cpp`, `write_history_params`), which refuses non-canonical text (`writer.cpp:824-833`). `WriterState::params` is a `std::map<std::string, std::string>` (`writer_state.hpp:54`), so `meta` rows are inserted in ascending key order. `finalize` names the first missing `param.*` key in `REQUIRED_META_KEYS` order (`writer.cpp:917-921`).
- `is_canonical_history_meta_value` (`schema.cpp:385-424`) accepts exactly the forms of spec 4.2: `parse_canonical_u64` for the span (no sign, no leading zero), byte-range tests for the hex script, the P0-03 id grammar for each race id, duplicates refused. The reader validates the keys in ascending order, the ones before `param.map_size` first (`reader.cpp:91-110`), so the first invalid key reported is the same on every platform.
- Round trip observed: `--test-pattern --civs-per-race 3 --races orc,elf --history-span 2500 --event-intensity calm --monster-pressure high --divine-activity meddling --history-detail dense` records exactly those values (races in race order) in `meta`.

**N7. H8: the stage is a function of the stored world, the loaded data, the eight settings and the seed.** `run_chronology` (`chronology.cpp:334-362`) reads only `context.regions()` (names), `context.content()`, `context.history_params()` and `context.seed()`; it reads no layer and runs no parallel step. Evidence:
- the C++ test "14.3 item 17, H8" (`history_chronology_property_test.cpp:302-329`) builds the stage's rows from a `--until regions` world read back (`history_params_from_meta`, `meta.seed`, region names by id) and compares them with the five tables of the full world, for seed 42 span 6000 and seed 7 span 2000; it passes;
- the reference's `check-world` recomputes the five tables from exactly the stored world (meta and region names) and the data root, and agrees on every world in section 4;
- `--until chronology` gives the same file bytes as the default world, and every geography table and layer of the full world is identical to the `--until regions` world (test above, and `hash --detail` against 0.12.0 below).
A byte-level resumed run cannot be produced yet, since no writer resumes from a stored world; P3-04 will add it (A2).

**N8. The rest of the change cannot alter output across platforms.**
- **`roman` move.** `roman_numeral` (`core/roman.cpp`) is the former `stages::roman` body unchanged (same table, same guard); `stages::roman` and `history::roman` forward to it. This also closes advisory A2 of the P3-02 audit. Observed: the 512 and 1024 worlds of seed 42 (the 1024 one has 24 region names with a numeral) have the same `layer`, `pass`, `biome`, `climate`, `region`, `region_border` and `region_biome` table hashes under 0.12.0 (`origin/p3/02-int`) and 0.13.0; only `meta`, the schema and the five new tables differ.
- **Report.** `inspect/chronology.cpp` formats integers with `db::format_i64` and `format_u64` only. Its arithmetic is safe on corrupt input: `end_year + 1` is taken in `uint64` after `start_year` ≥ 0 and `end_year` ≥ `start_year` are established (`:111-117`); `end_year − start_year + 1` (`:176`) is computed only after every age passed those checks and the last `end_year` equals E − 1 ≤ 11999 (`:146`), so every operand is in [0, 11999]. Messages are fixed text and integers, the first failure in a fixed order. `report.cpp:349-375` reads `race` and `age` with `table_rows` (ordered by primary key) and the span from the validated `meta`.
- **CLI.** `check_history_data` runs right after the data is loaded, for every plan (`cli.cpp:654`); its errors are sorted by the P0-03 9 rule with `std::stable_sort` and byte comparison, then deduplicated (`load_error.cpp:39-48`). Observed with edited data: D1, D2, D3, D4 and D5 all reported, sorted by message bytes, the same text twice, exit 3, no file written. A data root with no race files is refused earlier by the loader (`required kind 'race' has no definitions`); D0 (`data_checks.cpp:118-122`) guards library callers. `--until history` and an unknown name give the spec 4.3 messages, exit 2.
- **`world-hashes.sh`.** The two new cases are in `HIST_CASES` in case-name order, `expect` reads `p3-worlds.txt` (or `--golden-hist`) and prints a note while it is absent, as for the other golden files, and the usage text covers the new line (`sed -n '10,15p'`).

## 2. What was checked against the hard rules and P3-00 H1 to H8

| Rule | Result |
| --- | --- |
| Integer and fixed-point only (H1) | yes: `int64`, `uint64` and `Fx` (`mul_int`, `round_int`); no `float` or `double` in any changed file (lint `float-type`) |
| No standard transcendentals or distributions | none; the jitter is `history::pick` |
| One generator, streams by name; keyed draws (H4) | one registered stream, `hist.chronology.awakening`, one `history::draw` per present race after the first, keyed by race id; god names by `hist.names.god` (P3-02). No `Rng`, no `splitmix64_next` (lint `history-sequential-rng` covers `stages/chronology*`) |
| Ids in a defined order (H2) | race id = `order`; age ids 1 … P dense in race order; culture ids in canonical id order; god id = race id |
| No unordered iteration, pointer order (H3) | vectors in defined orders, `std::map` with string keys for `meta`, an ordered registry used for membership only (N3) |
| Overflow (H6) | every quantity in [−600, 2^46] (N1); `mul_int` checked; report arithmetic bounded (N8) |
| No thread timing, clock, environment (H7) | none; the stage is sequential; world files identical at `--threads` 1 and 5 |
| Pure function of stored state (H8) | N7, with A2 for the future resume path |
| UB and implementation-defined behaviour | none found: casts after range checks, no shift of a negative value, ASCII byte tests only in D5 and the meta checks (independent of `char` signedness) |
| SQLite: fixed settings, defined row order, no timestamps | unchanged settings; rows in key order in one transaction; `meta` has no time value |
| No fast-math | `cmake-fp-flag` clean; no CMake flag changed (only `worldgen/CMakeLists.txt` and test source lists) |

## 3. Lint

- At `b8d57ad`: `python3 -B tools/lint/determinism_lint.py --self-test` passed (15 snippet files, 0 failures); the default scan found 0 findings over 139 C++ files and 13 CMake files.
- **Scan set.** The default scope includes `worldgen/src/stages/chronology.cpp`, `worldgen/include/worldgen/stages/chronology.hpp`, `worldgen/src/history/params.cpp`, `data_checks.cpp`, their headers, `worldgen/src/inspect/chronology.cpp`, `worldgen/include/worldgen/inspect/chronology.hpp` and `worldgen/src/core/roman.cpp`; an explicit scan of those 12 files also gives 0 findings. `HISTORY_DIRS` already names `worldgen/src/stages/chronology` and `worldgen/include/worldgen/stages/chronology`, so `history-sequential-rng` applies to the stage. The SQL in `writer.cpp` is checked by the SQL rules.
- **No rule added.** Nothing found in this audit is a construct a mechanical rule could catch with few false positives: A1 is a bound carried by a data schema, A2 a design property.

## 4. What was run

All runs on Linux in the worktree `.claude/worktrees/p3-03-audit` (branch `p3/03-audit`, cut from `origin/p3/03-int` and fast-forwarded to `b8d57ad`), builds under `~/.cache/the-unmapped/build/p3-03-audit/`, worlds under the session scratch directory and `~/.cache/the-unmapped/out/p3-03-audit/`.

- **Builds.** `linux-gcc-release` and `linux-clang-release`, clean, warnings as errors.
- **Cross-compiler hashes.** `tools/ci/world-hashes.sh local linux-gcc-release linux-clang-release`: all 14 cases identical (`hash --detail` and `report` text), including `hist-races-elf-orc-1024` (`41041722…`) and `hist-span-2000-512` (`c65be15b…`). The geography cases' file hashes were the same before and after `e420e64`.
- **Threads.** Seed 9, 512: `--threads 1` and `--threads 5` give byte-identical files; `--until chronology` gives the same file as the default.
- **Schema bytes.** The `schema` line of `hash --detail` is the same for the default, `--until regions`, `--until none` and `--test-pattern` worlds.
- **Regions unchanged.** 0.12.0 (`origin/p3/02-int`, `linux-gcc-release`) against 0.13.0 for seed 42 at 512 and 1024: the seven geography table and layer hashes are equal (N8).
- **Independent reference.** `p3_chronology_reference.py check`: 127 checks, 0 failures. `check-world`, 23 checks each, 0 failures, on: the four `geo-*` and two `hist-*` cases of `world-hashes.sh`; seeds 0; 2^64 − 1 span 12000 `--races orc`; 3 `--races elf,orc`; 7 span 2000; phrase `Kaer` span 12000 `--races goblin,dwarf,human`; 123456789 `--races faerie`; 5 span 2001 `--races dwarf,human,orc` (all 512); and the 15 edited-data worlds of N1.
- **Tests.** `ctest --preset linux-gcc-release` and `linux-clang-release`. The failures are only those the lead expected: the world-hash goldens awaiting re-recording (`p0-worlds.txt` through `cli_golden_test.cpp`, `cli_p1_generate_test.cpp`; `GOLDEN_CASES` in `db_determinism_test.cpp:384`; the empty-world hash in `stages_pipeline_test.cpp`), all of which observe one hash per case whatever the thread count, and `history_chronology_report_test.cpp:129`, which the test writer is correcting. Every other entry, the chronology shards included, passes.

## 5. Asked of others

- **Engineer (optional, low priority):** A1's guard `G ≥ 1` in `awakening_years`, and optionally `end_year ≥ start_year` in `write_age`.
- **P3-04 spec writer and engineer:** A2, when the history stage adds the resume path: take the settings from `meta`, and keep the `HistoryParams` round-trip test exhaustive.
- **Validation specialist:** record `p0-worlds.txt`, `p1-worlds.txt`, the new `p3-worlds.txt` and the remaining in-test hashes at 0.13.0 after the comparison across presets, MSVC included, which this audit could not run.
