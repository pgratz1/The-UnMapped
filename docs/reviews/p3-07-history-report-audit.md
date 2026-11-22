# Determinism audit: P3-07 history report (generator 0.20.0, schema 7)

Auditor: determinism auditor, 2026-11-22. Change audited: `origin/p3/07-impl` at `81bbf17`, against `origin/main` (generator 0.20.0), as `git diff origin/main...origin/p3/07-impl -- worldgen tools`. Spec: `docs/specs/p3-07-history-report.md`, sections 4 to 9 and 12, with amendments 21 to 23 (r8 to r15). Audit branch `p3/07-audit`, cut from `81bbf17`. It adds only this file. Line numbers are those of `81bbf17`.

Scope (non-test code):
- `worldgen/src/history/digest_tables.cpp` (new): `read_history_tables`, the checks of 5.2 items 1 to 3 and of 5.3, and the engineer's extra checks (god and biome shapes, the population cap, god camp, creator god).
- `worldgen/src/history/digest.cpp`, `digest.hpp`: `digest_history` (peaks 7.1 and 7.2, milestones 7.3, land shares 7.5, the summary set of 21.3, the run statistic of 21.8 item 5 with 22.5b, the `[stats]` keys), `history_note` with the optional civilization (22.2), and the 23 item 4a note.
- `worldgen/src/inspect/history_report.cpp`, `history_report.hpp` (new): `build_history_report`, `escape_history_text`, `parse_history_level`, the trailing-space rule.
- `worldgen/src/history/event_catalogue.cpp`, `.hpp`: `race_generation_years`, `civilization_info`.
- `worldgen/src/db/result.cpp`, `result.hpp` (`ErrorCode::history_missing`), `reader.cpp`, `reader.hpp` (`WorldReader::path()`).
- `tools/worldgen-cli/cli.cpp` (`--history`, `--level`), `output.cpp` (exit code of `history_missing`).
- No change to `cmake/`, `CMakePresets.json`, `extern/`, `.github/`, `tools/ci/`, the schema, the writer, any stage or `game/data`. No world byte changes. `GENERATOR_VERSION` stays 0.20.0, as it should (3.3).

**Verdict: pass.** No blocking finding. There is one should-fix finding: a signed overflow that only a hostile world file can reach (S1). There are two advisory findings (A1, A2). Every valid world I tried gives byte-identical text, at both levels, under GCC release and Clang with ASan and UBSan. These are the 11 engineer samples and 3 worlds of my own, 28 reports in all. The text also matches the engineer's `linux-clang-debug` texts. None of the reports contains a CR, a byte outside 0x0A and 0x20 to 0x7E, or a trailing space. On 36 damaged copies, every corrupt file gives an error as a value (exit 3) with no sanitizer report, except S1. The lint is clean with its self-test, and I added no rule (section 3).

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts for a world the generator can write. **Should-fix** breaks or leaves open a stated contract (here 5.2: a corrupt file is an input error, never undefined behaviour). **Advisory** is fragile but currently safe. A **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

**S1. Signed overflow in the upland flag on a hostile `region` row** (`worldgen/src/inspect/history_report.cpp:236`; checks in `worldgen/src/history/digest_tables.cpp:247-263`).
- **Rule.** Hard rule 1 and P3-07 5.2 and 12: integer arithmetic must be defined on every input that passes the checks. A corrupt file is an input error, not undefined behaviour.
- **Code.** `4 * home->walk_cells < home->cells` on `std::int64_t`. The region checks of 5.3 bound `cells` below (at least 1) and bound `walk_cells` and `lake_cells` to 0 to `cells`. They put no upper bound on `cells`.
- **Failing scenario.** Take the copy `damage/overflow_walk.db` of `seed-0-1024.db`. Set `cells = walk_cells = 4611686018427387904` (2^62) on the home region of `c1`. Every check passes. Then UBSan reports `history_report.cpp:236:17: runtime error: signed integer overflow: 4 * 4611686018427387904 cannot be represented in type 'std::int64_t'` at both levels.
- **Why it matters across compilers.** Without the sanitizer the result is undefined. GCC and Clang may fold `4 * w < c` on the assumption that it does not overflow, while MSVC wraps. So the same file can print `, upland` on one compiler and not on another.
- **Who can reach it.** No world of the generator: a region has at most map_size² cells.
- **Suggested fix**, one of:
  - bound `cells` in `read_history_tables`, for example `row <id>: cells above <map_size²>` (the reader knows `map_size`), with a proposals entry adding the reason to 5.3, as was done for the population cap;
  - or compute the flag without a product: `home->walk_cells < home->cells / 4 + (home->cells % 4 != 0 ? 1 : 0)`. For non-negative integers this equals 4w < c.

  The islet test `cells - lake_cells` (`digest.cpp:398`) cannot overflow, because 0 ≤ `lake_cells` ≤ `cells`. I tried `cells` = INT64_MAX on a region that is no civilization's home (`damage/huge_cells_nonhome.db`): exit 0, no sanitizer report.

### Advisory

**A1. `permille` is safe only under 7.6's settlement bound, which no check enforces** (`worldgen/src/history/digest.cpp:781`, used at `1118`).
- **Code.** `race_population_share_max_permille` computes `1000 * race_population_[k]` with `race_population_[k]` ≤ `population` = Σ of standing populations.
- **The bound it relies on.** `read_history_tables` caps each population at 2^31 − 1 (`digest_tables.cpp:357`, `428`). Spec 7.6 bounds the settlement count at 1,000,000 (P3-04 6.1), and under that bound the product stays below 2^61. Nothing checks the settlement count, though.
- **Failing scenario.** The product overflows only when Σ population exceeds about 9.2 × 10^15, that is with more than about 4.3 million standing settlements at the cap. That takes a hand-built file of several hundred megabytes, and the generator never writes one. I worked this out from the code; I did not build such a file.
- **Other ratios.** `simulated_permille`, `events_per_1000_simulated_years`, `wars_hollow_permille`, the land share `1000 * h / N` (`digest.cpp:563`) and `race_regions_share_max_permille` all multiply row counts or years. They cannot come near 2^63.
- **Suggested fix.** Check the 1,000,000 settlement bound of 7.6 in `read_history_tables`, next to the population cap. Or compute this one ratio with the project's wide multiply-divide in `core/wide.cpp`.

**A2. Two separate reads of the file, and the report indexes one by the other** (`worldgen/src/inspect/history_report.cpp:496-505`, `194-199`, `214`; `worldgen/src/db/reader.cpp:271`).
- **Code.** `build_history_report` first calls `read_history_tables`, which reads the raw history tables and checks every event's `phase` against the `phase` rows (`digest_tables.cpp:200-213`). Then `HistoryView::load` reads `event` and the rest again. The reader opens the file `SQLITE_OPEN_READONLY` without an enclosing read transaction, so each `table_rows` call is its own snapshot.
- **What depends on the two agreeing.** `Report::phases` does `++phase_events_[e.row.phase]` on the view's events, with the vector sized from the table's phases. `shown` indexes `d_.summary`.
- **Who can reach it.** Only another process changing the file in place between the two reads could make them disagree. The generator writes through `<out>.partial` and a rename, which an open read-only connection never sees, so the generator cannot. This is not a determinism defect for any file at rest. It is an unstated assumption that memory safety rests on.
- **Suggested fix.** Hold one read transaction (`BEGIN` and `COMMIT` on the reader's connection) across `read_history_tables` and `HistoryView::load`. Or bound `e.row.phase` by `phases.size()` in `Report::phases`. The viewer will bind the same function (P3-00 A1), so the first fix is the better one.

### Notes

**N1. Orders.** Every order is total, or is stable on input that is itself in primary-key order. No order depends on SQLite's row order beyond `ORDER BY` the primary key, which `table_rows` and the `read_*_rows` functions use.
- **Milestones** (`digest.cpp:482-492`): `stable_sort` by (salience descending, id ascending), cut to 6, then `stable_sort` by id. Both orders are total.
- **Relations of a civilization** (`digest.cpp:506-518`): (since, a_type, a_id, b_type, b_id, kind). This contains the whole primary key of `relation`, so it is total.
- **Population peaks** (`digest.cpp:586-588`): `stable_sort` by `year` alone. The input is `settlement_census` in key order (settlement, year), so the result is (year, settlement), a total order. The tie rule of 7.2 (strict `>`, so the earliest census year wins) needs no order inside a year anyway.
- **Summary ranking** (`digest.cpp:696-700`): (salience descending, id ascending), total. The floor walk is by age index, then by rank.
- **Captured settlements** (`digest.cpp:1004-1005`): a sort and `unique` on integers.
- **The one `std::map`** (`digest.cpp:1066`): keyed by the integer pair (defender, region), with vectors filled in event id order.
- **Kind counts** in `Report::relations` (`history_report.cpp:271-283`): an ordered insertion by `std::string <`.
- **No pointer is ordered or used as a key.** The `const KindDef*`, `const db::AgeRow*` and similar pointers are only tested against `nullptr` and dereferenced.

**N2. Byte comparison of relation kinds.** `std::string::operator<` goes through `char_traits<char>::lt` and `compare`, which the standard defines as `unsigned char` comparison. The order of kinds therefore does not depend on whether `char` is signed. `damage/relation_kinds.db` adds kinds `\xC3\xA9z`, `Zed`, `apple`, `self` (a relation of `c1` with itself) and `faction` (`a_type` 7, `fa99`). Its count line reads `Zed 1, apple 1, enmity 2, faction 1, peace 6, self 1, war 6, \xC3\xA9z 1`: byte order, with the high byte last. GCC and Clang print the same text. The self relation is listed once in its block, and the type 7 entity prints as `fa99` without a name (6.1).

**N3. Escaping and the trailing-space rule.**
- **Escaping** (`history_report.cpp:475-491`) casts each byte to `unsigned char` before testing and indexing `HEX`. It uses no `isprint` and no locale, and converts no case.
- **Trailing spaces.** `line` (`history_report.cpp:86-96`) rewrites only the spaces at the end of a line, after all the text is assembled.
- **Test file.** `damage/name_bytes.db` gives civilization 1 the bytes `Bad\Name \xC3\x80\xFF  ` and settlement 1 `Trail  `, and region 1 starts with CR, LF, TAB.
- **Result.**
  - The heading prints `c1 Bad\\Name \xC3\x80\xFF\x20\x20`.
  - The same name inside a line keeps its spaces: `Bad\\Name \xC3\x80\xFF   (c1)`.
  - The region prints `rg1 \x0D\x0A\x09 z: free`.
  - The report has as many lines as the undamaged one (985 at `full`), and no byte outside 0x0A and 0x20 to 0x7E.
- **Notes.** `history_note`'s text is escaped by the report (`history_report.cpp:116-119`), so a note cannot bring in a raw byte.

**N4. Numbers.** Every number goes through `db::format_i64` or `format_u64`, which call `std::to_chars` (`worldgen/src/db/schema.cpp:82-86`). No `printf`, stream, `std::to_string` or grouping. Every ratio is `floor` of non-negative `int64` with a zero-divisor guard (`permille`, `digest.cpp:781`). The generations count uses an explicit `floor_div` (`digest.cpp:216-219`) on bounded years, so it does not depend on the sign of the quotient; C++ truncation toward zero is the same on every compiler anyway. Sizes go to `int64` through `static_cast`. No `long` or `int` appears in the new code, so the LLP64 and LP64 widths play no part.

**N5. The summary threshold** (`digest.cpp:743-767`). `entered[u]` is the highest salience of a candidate whose unit holds u. The histogram is indexed by `min(entered, 100)`, and only for `entered` ≥ 20, so a salience above 100 counts at every threshold, as C(t) says, and the index stays in 20 to 100. The suffix sum stays within `at_least[101]`.
- **Experiments.** `damage/salience_extreme.db` sets one event to INT64_MAX and one to INT64_MIN. `salience_all_100` and `salience_all_0` set every event to 100 or 0. All three run clean under the sanitizers, and the INT64_MAX event is shown.
- **Arithmetic.** No arithmetic is done on a salience; it is only compared.

**N6. Duplicate war rows.** `war_row_` (`digest.cpp:420-427`) keeps the first `war` row of a `since_event` in key order, so a second row with the same `since_event` (a hostile file, `damage/dup_war.db`) gives a fixed answer.

**N7. Uninitialised data.** All the members of `RegionInfo`, `HistoryTables`, `RegionPeak`, `PopulationPeak`, `CivilizationDigest`, `HistoryDigest`, `Kinds`, `Digester` (`nonislet_`, `share_*`, `share_runs_{}`) and the local `State` have default initialisers. The count arrays are value-initialised (`{}`). The `db::*Row` values are built with every field given.

**N8. Line ends and output.** The history path writes through the same code as the world report (`tools/worldgen-cli/cli.cpp:1041-1048`). With `--out`, that is `write_file_atomically` with `std::ios::binary` (`output.cpp:192`). Otherwise it is `print_out` on stdout, which `prepare_stdio` sets to `_O_BINARY` on Windows at start-up (`platform.cpp:71-74`, called from `main.cpp:15`). The text contains only LF, so the output is LF on Windows too. The `--out` file is identical to stdout (checked for seed 0 at `full`), and it ends with one LF.

**N9. The path in the no-history message.** `WorldReader::path()` (`reader.cpp:395-398`) returns the string stored at open (`reader.cpp:238`), the UTF-8 path as given, not normalised. It is the same string the reader's other messages print. Examples: `report --db ./tp.db --history` prints `world file './tp.db' has no history (stages: test_pattern); ...` and `--db tp.db` prints `'tp.db'`. On Windows the argument goes through the existing UTF-16 to UTF-8 conversion of P0-04 4.1, with no further change. The text is input-dependent by design (4) and depends on nothing else.

**N10. Guards that rest on earlier checks, verified.** Each of these lookups relies on a check that runs before it:
- `age_of(c.founded_year)` is never null, because `history_end_year` must equal `param.history_span` (`reader.cpp:135`), P3-03 9.1 makes the ages tile that span, and `founded_year` is checked to 0 to E − 1.
- `race(c.race)` and `god(r->creator_god)` are never null, by "is not a present race" and the extra creator-god check.
- `home` (the home region) is never null, by "home_region is not a region".
- `event(m)`, `kind(...)` and `k.roles[p.role - 1]` are in range, because P3-04 11.1 and `HistoryView::load` make event ids and kind ids consecutive and the roles valid.
- `phase_events_[e.row.phase]` is in range, by 5.3's event-phase check (but see A2).
- The status and kind arrays are in range, by the 1–4 and 1–5 checks.
- The difference arrays are in range, because `from_year` is in 0 to E − 1 and `to_year` in `from_year` + 1 to E − 1, or E when NULL.

## 2. What was checked

- **Floating point and platform math:** none in the diff (lint `float-type`, `std-math`, `banned-include`; also read by hand).
- **Formatting and locale:** `std::to_chars` only (N4). No `printf` family, stream, `std::to_string`, `<locale>` or `<cctype>` (lint `number-text`, `stream-format`, `locale`, `ctype`).
- **Unordered containers, hashing and pointer order:** none (lint `unordered-container`, `std-hash`, `pointer-order`). The one `std::map` is keyed by integers (N1).
- **Sorting:** four `std::stable_sort` calls with total keys or stable input (N1). There is no `std::sort`, so the `unstable-sort` review rule has nothing to report.
- **`std::vector<bool>`:** none; the engineer's `std::vector<std::uint8_t>` is used (`digest.cpp:597`, `713`; lint `packed-bool`).
- **Signedness and width:** `char` in escaping and string comparison (N2, N3). No `long` (lint `long-type`). Every index cast is from a checked non-negative `int64`. Every product and sum against 7.6's bounds (S1, A1).
- **Time, environment and threads:** none in the diff. `--verbose` goes to stderr (`Progress`). The report runs on one thread.
- **Rows and SQL:** no new SQL. The rows are read through `table_rows` and the `read_*_rows` functions, with `ORDER BY` the primary key.
- **Errors as values:** `read_history_tables` and `build_history_report` catch their internal exceptions and `std::bad_alloc` and return `db::Error`. `HistoryView::load` was already value-returning.
- **Build flags:** unchanged.
- **The engineer's claims, verified:** every sort is a `stable_sort` with a total order (yes, N1). `vector<bool>` was replaced (yes). Relation kinds are compared with `std::string <` (yes, and that is byte order, N2). There is no floating point, locale or unordered container (yes).

## 3. Lint

`python3 -B tools/lint/determinism_lint.py --self-test` passed (18 snippet files). `python3 -B tools/lint/determinism_lint.py` scanned 167 C++ files and 13 CMake files with 0 findings.

I added no rule:
- **S1 and A1** are products on input whose bounds come from runtime checks. A text rule cannot tell `4 * walk_cells` from `4 * checked_small_value`.
- **A2** is a matter of how two library calls are composed.
- **Already covered:** the hazards this change could have brought in (signed `char` used as an index, `isprint`, `std::to_string`, `vector<bool>`, unstable sorts, pointer keys) already have rules (`ctype`, `number-text`, `packed-bool`, `unstable-sort`, `pointer-order`). The new code passes all of them.

## 4. Experiments

I built `worldgen-cli` from `81bbf17` in the audit worktree with `linux-gcc-release` (GCC 15.2) and `linux-clang-asan` (Clang, ASan and UBSan). Worlds and reports are under `~/.cache/the-unmapped/out/p3-07-audit/` (`cmp/` and `damage/`). Nothing was written to the source tree.

**Valid worlds, both levels, GCC release against Clang ASan and UBSan.** The texts are byte-identical, with no sanitizer output:

| World | summary lines, threshold, shown | full lines, shown |
| --- | --- | --- |
| engineer samples: seeds 0 to 9 and phrase `the unmapped`, 1024, default | 595 to 813 lines each | 985 to 1596 lines each |
| seed 5, 1024, `--civs-per-race 4 --event-intensity turbulent` | 2081, 71, 213 | 5829, 3737 |
| seed 17, 1024, `--event-intensity calm --races elf,orc` | 422, 20, 117 | 591, 193 |
| seed 3, 512, default | 667, 26, 192 | 928, 408 |

- **Against the engineer's texts.** The 22 sample reports are also identical to the engineer's `*.linux-clang-debug.txt` and `*.linux-gcc-release.txt` texts.
- **Byte checks.** No report contains a CR, a byte outside 0x0A and 0x20 to 0x7E, an empty line or a trailing space.
- **Levels.** `[stats]` is identical at the two levels for every world.
- **Companions.** The turbulent world (threshold 71) exercises the companions of 21.2 well above the age floors.

**No-history and usage paths** (GCC):
- **No history, exit 3:** `--until chronology` (512) and `--test-pattern` worlds give the message of 4, with the path as given.
- **Usage errors, exit 2:** `--level full` without `--history`, `--level Full`, a missing value, and a repeated `--history` or `--level`.
- **Output errors, exit 4:** an existing `--out` without `--force`.

**Damaged copies of `seed-0-1024.db`** (made with Python's `sqlite3` module; the originals are untouched), each run at both levels under ASan and UBSan with `halt_on_error=1`:

| Result | Cases |
| --- | --- |
| exit 3 with the 5.2 or 5.3 message, no sanitizer report | `biome_missing`, `camp7`, `capital_missing`, `cause_forward`, `census_first`, `census_none`, `creator_missing`, `ended_disagree`, `end_year_mismatch` (reader), `event_kind_gap`, `event_phase`, `founded_late`, `no_participants`, `participant_type`, `pop_big`, `rc_civ0`, `rc_overlap`, `relation_since_neg`, `relation_type0`, `stages_nohist` (phase rows without `history` in `stages`), `stages_nohist_empty` (`history_missing`), `status9`, `template_bad`, `until_below`, `value_missing`, `truncated` (half the file), `garbage` |
| exit 0, clean | `dup_war`, `huge_cells_nonhome`, `name_bytes`, `relation_kinds`, `salience_extreme`, `salience_all_100`, `salience_all_0`, `flipped` (4 KiB of XOR in the middle of the file, which lands in layer data) |
| UBSan signed overflow | `overflow_walk` (S1) |

## 5. Not checked

- **MSVC and Windows.** I built neither here. CI's `compare-hashes` covers the world files, and the report texts are saved per preset by `world-hashes.sh` (`<preset>-<case>.report.txt`), but that is the world report. For the history report on Windows, the validation specialist should compare `report --history` at both levels across presets for the golden cases, or the test writer's golden case (14.4) should run on every preset.
- **A1.** I did not build a file with millions of settlements; the finding comes from reading the code.
- **A2.** I did not test a concurrent writer.
- **Performance** (11) is not a determinism matter and was not measured.
- **The two failing CTest entries.** `worldgen-tests-conflict` and `worldgen-tests-balance` fail on this branch because of the 23 item 4a note wording. The lead says the test writer is updating them, so they are not findings. I did not run the test suites.
