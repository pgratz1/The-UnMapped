# Determinism audit: P3-04 simulation engine (generator 0.14.0, schema 7)

Auditor: determinism auditor, 2026-11-19. Change audited: `git diff origin/p3/03-int..origin/p3/04-int` at the tip `40e1064` ("P3-04 integration: the floor of simulated years is clamped to the cap"), non-test code. Spec: `docs/specs/p3-04-simulation-engine.md` at revision r3 on `origin/p3/04-spec-fixes` (13b8062); 16.6 is the normative fatal-message list, W1 to W9 the writer rules, section 9 the tables and row orders. `p3/04-spec-fixes` carries an older `schedule.cpp` without the floor clamp; the audited code is the `p3/04-int` one, which matches r3 5.1. Line numbers are those of `40e1064`.

Scope:
- `worldgen/src/history/engine.cpp`, `history/engine.hpp`: entry checks, `GeoView`, `Engine` (steps, event writer W1-W9, quiet eras, bookkeeping 6.7, interventions), `State`, `StepContext`, `run_engine`.
- `worldgen/src/history/schedule.cpp`, `history/schedule.hpp`: tuning, SC1-SC6, `Runner`, `plan_phases`.
- `worldgen/src/history/event_catalogue.cpp`, `history/event_catalogue.hpp`: compiled catalogue, `report_events_problems`, template parser, `read_event_catalogue`, `HistoryView::load`, `render_event`.
- `worldgen/src/history/inputs.cpp`, `history/inputs.hpp`; `worldgen/src/history/stubs.cpp`, `history/stubs.hpp`.
- `worldgen/src/stages/history.cpp`, `stages/history.hpp`; `stages/stage.{hpp,cpp}`, `stages/pipeline.hpp` (committed rows, the new writers, the stage list).
- `worldgen/src/db/rows.cpp` (typed readers), `db/schema.hpp` (13 DDL statements, `history_end_year`), `db/writer.{hpp,cpp}`, `db/writer_state.hpp` (13 writers, key state and rollback, `build_meta`), `db/reader.{hpp,cpp}` (the `history_end_year` checks), `db/layer_blob.hpp` (`operator==`).
- `worldgen/src/data/kinds.cpp`, `kinds_history.cpp`, `data/schema.hpp` (`history.engine`, `history.report`, kind check `report_events`).
- `worldgen/src/inspect/history.cpp`, `inspect/history.hpp`, `inspect/report.cpp` (`[history]`).
- `tools/worldgen-cli/cli.cpp` (usage text only), `tools/ci/world-hashes.sh` (`hist-dense-1024`), `game/data/history/engine.toml`, `report.toml`; `stages/chronology.cpp` (the new `gap < 1` guard).

Read for context, unchanged: `history/draw.cpp`, `history/names.cpp` (registry, `make_name`), `db/reader.cpp` `visit_table_rows` (`ORDER BY` every primary-key column `COLLATE BINARY`), `inspect/hashes.cpp`.

**Verdict: pass with notes.** No blocking finding and no should-fix finding. Every value the stage writes is an integer or ASCII text derived from stored inputs with `int64` arithmetic far from overflow, the engine draws nothing, the stub draws only names through keyed draws, every container is a vector indexed by id or a `std::map` over integers and byte-ordered strings, every table is written in strictly ascending primary key (checked by the writers), and `render_event` and `[history]` format with the project's own integer formatting. The resume constructor reads exactly what the pipeline holds in memory (R1 and Y2 pass, section 4). Four advisories, all hazards for the subsystems P3-05 and P3-06 will plug in or for corrupt input; nine notes. One lint rule added: `history-live-iteration` (section 3).

Classification: **blocking** can change output between platforms, compilers, standard libraries or thread counts. **Should-fix** breaks or leaves open a stated contract and needs action before merge. **Advisory** is fragile but currently safe. A **note** is for the record.

## 1. Findings

### Blocking

None.

### Should-fix

None.

### Advisory

**A1. `State`'s entity vectors are handed out as spans and references that a founding invalidates; with real subsystems this becomes platform-dependent undefined behaviour.** (P3-05, P3-06 must respect; lint rule added.)
- `engine.hpp:138-141`: `civ(id)`, `settlement(id)` return references and `civs()`, `settlements()` return spans into `std::vector<Civilization>` / `std::vector<Settlement>` (`engine.hpp:176-177`). `found_civilization` and `found_settlement` (`engine.cpp:899`, `engine.cpp:949`) `push_back` into the same vectors.
- Failure scenario: a P3-05 subsystem in T2 or Q4 writes `for (const Civilization& c : ctx.state().civs()) { if (ctx.visible(c) && …) state.found_civilization(…); }` (secession), or holds `const Settlement& s = state.settlement(id)` and reads `s.region` after a `found_settlement`. When the push reallocates, the loop reads freed memory. Whether it reallocates at a given count depends on the growth policy: libstdc++ and libc++ double the capacity (1, 2, 4, 8, …), MSVC's STL grows by half (1, 2, 3, 4, 6, 9, …). So the 4th founding reallocates under MSVC (capacity 3 → 4) and not under GCC or Clang (capacity already 4): the same seed reads valid memory on Linux and freed memory on Windows, and the output (or a crash) differs. This is exactly the "iteration over civilizations/settlements while they are being created" hazard the lead asked about.
- The engine itself is safe today: `quiet_eras` (`engine.cpp:531-556`) and `bookkeeping` (`engine.cpp:748-772`) index by position and found nothing inside the loop, and the stub's `settlement_at` (`stubs.cpp:52-59`) only reads.
- Fix asked (P3-05/P3-06 engineers; the engine may help): iterate by id up to a count taken before the loop (`for (id = 1; id <= n; ++id)`, re-fetching `state.civ(id)` after any founding), or collect the decisions and apply them after the loop; never keep a reference, pointer or span across a `found_*` call, including in a subsystem's members between steps (keep ids). Optionally the engine could `reserve(MAX_CIVILIZATIONS)` for civilizations (4096 small structs; it removes the hazard for that vector entirely) and document the rule at `State`. The new lint rule `history-live-iteration` (section 3) catches the range-for form; references held across a founding remain an audit item.
- Same for `relations()` (`engine.hpp:143`): a range-for over the map that calls `close_relation` on the current key erases the node under the iterator (undefined behaviour on every library); `open_relation` during the loop inserts a node that the loop visits or not according to its key, which is deterministic but easy to get wrong. The lint rule covers both.

**A2. The tension source is not called at the step that ends a quiet interval, so a source that accumulates in `tension()` sees a call sequence shaped by the schedule.**
- `schedule.cpp:256-257` passes `asks_tension = false` for the last step of an interval (SC6, E3) and `engine.cpp:483-486` then skips `tension()`. `TensionSource::tension` is non-const (`engine.hpp:263`) and the spec only says it "must not change state or append" (world state); it may change its own members.
- Failure scenario: P3-06 keeps a running tension sum in its source and adds the step's grievances inside `tension()`. The interval-ending steps are silently not counted, so the sum depends on where awakenings fall. Deterministic on every platform, but a P3-06 test that assumes one call per quiet step would fail, and a refactor that starts calling it at the last step (to log it, say) would change world bytes without any rule saying so.
- Fix asked (P3-06): accumulate tension in a `Subsystem` step (Q4, or T6 for tick years) and make `tension()` a pure read of that member. Optionally the engine could declare `tension()` `const` in P3-06's revision of the seam.

**A3. `[history]` corrupt-data checks do signed `int64` arithmetic on unchecked stored values before the range check that would reject them.**
- `inspect/history.cpp:135` (`simulated_years += end - start + 1`), `:138` (`expected_start = end + 1`), `:193` (`simulated_years * 1000`). `start` is checked against `expected_start` and `end ≥ start`, but `end` itself is bounded only after the loop (`:140`, last phase must end at E − 1). A hand-edited world with a phase row `end_year` = 9223372036854775807 overflows `end + 1`, which is undefined behaviour; in practice every compiler wraps and the later check reports `start_year -9223372036854775808, expected …` or the last-phase message, but UBSan (`linux-clang-asan`) aborts and an optimiser is entitled to do anything. `report` text, including its corrupt-data messages, is a cross-platform contract.
- Not reachable from any world the generator writes (E ≤ 12000).
- Fix asked (engineer, low priority): in the phase loop, report `end_year <e>, expected at most <E - 1>` (or reuse `end_year below start_year` wording) when `end > end_year - 1`, before any arithmetic; then `simulated_years` ≤ 12000 and the products are bounded. The same pattern should be kept in P3-07's `report --history` `[stats]`, which shares these definitions.

**A4. `found_settlement` silently skips the region check when the `region` layer does not cover the map or is not `u16`.**
- `engine.cpp:917-926`: the r3 check "the cell lies in the given region" runs only if the layer is N × N and `std::uint16_t`; otherwise any cell is accepted. `cell_value` in the stub (`stubs.cpp:63-75`) likewise returns −1 for an unexpected type.
- Both constructors of `HistoryInputs` decode the same stored blob, so this cannot differ between platforms or between a full run and a resume. It matters only if a later spec changes the layer's element type (more than 65,535 regions) or a test builds inputs by hand: the guard would vanish rather than fire.
- Fix asked (engineer, low priority): make a missing or mis-typed `region` layer a precondition failure of `run_engine` (`history: input precondition: …`, added to 16.6), so the check is unconditional.

### Notes

**N1. Subsystem call order.** Subsystems run in `EngineConfig::subsystems` order per step (`engine.cpp:181-191`, `361-366`); entities by ascending id; regions, settlements, relation keys ascending in bookkeeping. P3-05 and P3-06 must install subsystems in a fixed order written in code (the spec says peoples, then conflict), never from a container ordered by pointer (the lint's `pointer-order` rule catches `std::set<Subsystem*>`). A subsystem that lists the same step twice in `steps()` runs twice in that step (`by_step_` is appended per listed step, no duplicate check): deterministic, but probably not intended; the engine could refuse duplicates with the existing "is not a subsystem step" style message.

**N2. `visible()` is a convention, not enforced.** `civs()` and `settlements()` return every entity, including those created in the current step; only `StepContext::visible` filters (`engine.cpp:1096-1102`). In Q4, shared by peoples and conflict, a conflict subsystem that forgets `visible()` would see the settlements peoples founded a moment earlier in the same step. Deterministic (fixed config order), but it breaks P3-00 4.2 and makes a world depend on the order of the two subsystems. P3-05/P3-06 should iterate through a helper that applies `visible()`; a later engine revision could offer `visible_civs()`.

**N3. Draw keys.** `StepContext::draw(stream, a, b, k)` fixes w0 = `year()` (`engine.cpp:1112-1115`); in a quiet step that is the step's start, and T1 to T6 of one tick year share it. Two decisions with the same stream key and the same (a, b, k) in different steps of one year get the same value. Not a determinism defect, but a correlation trap: each subsystem should use its own declared stream (P0-01 stream list) per decision kind, and put the entity id(s) in a, b and a decision ordinal (never a position in a filtered or sorted list that depends on insertion history) in k. The P3-04 engine itself draws nothing (confirmed: no `draw(` call in `engine.cpp`, `schedule.cpp`), and the stub's names are keyed by culture and entity id (`stubs.cpp:146-147`, `188-189`).

**N4. Relation keys are not normalized.** `open_relation` (`engine.cpp:1027-1042`) stores the key as given; the spec's rule that a symmetric relation is stored with (a_type, a_id) < (b_type, b_id) is left to P3-06. Opening `war(A, B)` in one place and `war(B, A)` in another would give two rows. Deterministic, but P3-06 should normalize in one helper (or the engine should, once P3-06 lists the symmetric kinds).

**N5. Net-change bookkeeping hides some events.** A relation closed and re-opened within one step leaves no row change and keeps the original `since_event` (`engine.cpp:741-744`); a region set to its current owner with a new event records nothing, and two owners set in one step record the last `owner_event`. This follows 6.7 ("the last change of the year wins; the events stay in the log") and is order-independent for the owner but not for `owner_event`: two subsystems that both set the same new owner in one step with different events record the event of the later one. Fixed config order makes this deterministic; P3-06 should know.

**N6. Row order of the thirteen tables.** Catalogue tables in kind id then seq (`catalogue_rows`, `event_catalogue.cpp:409-423`); `phase` in id order; `event`, `event_participant` (event, seq), `event_cause` (event, ascending cause, W6), `event_value` (event, seq) in append order, which is id order (`engine.cpp:208-219`); `civilization`, `settlement` in id order; `settlement_census`, `region_control` and `relation` collected in bookkeeping order and then `stable_sort`ed by their full primary key (`engine.cpp:231-245`). Each primary key is unique (each bookkeeping has its own year Y; a census row per (settlement, year) is guarded at `engine.cpp:764-767`; a relation key re-opens only at a later Y), so the sorts are total and `stable_sort` is not even needed. `RelationKey`'s defaulted `<=>` and the relation sort compare `std::string` with `char_traits<char>`, which compares as `unsigned char`: the same order as SQLite's `BINARY` collation the reader uses. Every writer refuses a key not above the previous one (`writer.cpp`, `require_next_id` / `require_next_pair` / the relation tuple), and its key state is restored on a rolled-back stage (`writer.cpp:442`, `1099`).

**N7. `meta.history_end_year`.** Written by `build_meta` from the stored `param.history_span` text (`writer.cpp:152-156`), so it is canonical whenever the span is, and inserted in the `std::map` key order like every key (between `generator_version` and `param.civs_per_race`, as `REQUIRED_META_KEYS` lists). The reader checks its form in place and its textual equality with `param.history_span` last (`reader.cpp:91-96`, `135-136`); both are canonical decimals, so textual and numeric equality coincide. No timestamp anywhere in the new code.

**N8. Integer ranges.** `scale_years`: v ≤ 3000, E ≤ 12000, product < 2^26 (the spec's 2^43 is a loose bound); the function itself only guards v ≥ 0 and E ≥ 1 (`schedule.cpp:109-115`), so it is safe for the values `check_engine_tuning` admits and should not be exported as a general helper. Floor and cap: permille ≤ 500 × E ≤ 12000. `era_values`: (t − founded_year) ≥ 0, divided by `generation_years` ≥ 1 (checked, `engine.cpp:564`), so `div` of non-negatives. Population is bounded to [0, 2^31 − 1] at every setter and never summed in P3-04. `simulated_permille` ≤ 12000 × 1000. Every `size_t` that reaches text goes through `format_u64`; no `long`, no `%zu`, no `printf`, no `std::to_string` in the new code; `int_text`/`size_text` wrap `db::format_i64`/`format_u64`.

**N9. Templates and rendering.** The parser tests bytes with `static_cast<unsigned char>(…) >= 0x80` (`event_catalogue.cpp:221`, `254`) and compares ASCII letters with `'a'`…`'z'` (`is_key`, `:32-42`), which gives the same result for signed and unsigned `char` (a byte ≥ 0x80 is outside the range either way). `render_event` uses no locale, no case mapping and no number formatting other than `format_i64`; `HistoryView` looks names up by binary search on vectors filled from the reader's primary-key order. The C++ sentences match the independent Python reference byte for byte (the golden test, section 4).

## 2. What was checked, item by item

| Item asked | Result |
| --- | --- |
| Floating point, transcendental, `std::` distribution, `std::hash`, unordered containers | None in the new code (lint clean; read by hand). |
| Pointer-value or address ordering | None. Pointers appear only as `const RaceRow*` / `const KindDef*` / `const RegionRow*` lookups (never compared, never keys) and the `EngineConfig` vectors, iterated in index order. |
| `std::sort` comparators | No `std::sort`. `stable_sort` on census, region control and relations by full primary key (total); `GeoView` neighbours by region id (unique per region); `check_compiled` on `string_view` values. |
| Locale/platform formatting, `char` signedness | See N8, N9. |
| `scale_years`, floor/cap, population, census, permille | See N8; A3 for the report on corrupt input. |
| LP64/LLP64 widths | No `long`; `std::size_t` only for indices and lengths, converted with explicit casts; ids and years `int64`; `Step` is `uint8_t`, range-checked (`engine.cpp:186`). |
| Keyed draws only | Yes: the engine draws nothing; the stub calls `make_name` (P3-02 keyed draws, no new stream; 2 of the spec). Lint `history-sequential-rng` clean over `HISTORY_DIRS`, which covers `src/history/`, `include/worldgen/history/`, `src/stages/history*` and `include/worldgen/stages/history*`, so every new history file. `inspect/history.cpp` is not history-stage code and needs no RNG rule. |
| Stream names declared in the spec | No stream added (spec 2, P0-01 note). |
| Row order of the 13 tables; total keys; no timestamps | N6, N7. |
| State containers | `State`: vectors by id, `std::map<RelationKey, …>` with defaulted `<=>` over four `int64` and `std::string` (total), `closed_` the same; engine: `std::map<int64, int64>` for `charged_` and `open_eras_`, vectors for bookkeeping, `std::map<RelationKey, size_t>` for open relation rows. No unordered or pointer-keyed container. |
| Resume path | `history_inputs_from_world` reads the same rows (reader `ORDER BY` primary key = the writers' enforced ascending order = the order the pipeline appended them) and decodes the same layer blobs as the pipeline (layers are held decoded from their stored blobs, P1-00 5.1); integers and text round-trip exactly. R1 compares the two constructors member by member and Y2 compares the resumed tables with a full run; both pass (section 4). |
| `render_event`, `[history]` | N9; `[history]` keys printed in fixed order, `events_<key>` in `event_kind` id order from the reader, all integers through `format_i64`. A3 for corrupt input. |
| Lint | Both commands run before and after the change (section 3). |
| Subsystem seams | A1, A2, N1 to N5. |

## 3. Lint

Both commands were run on `40e1064` before any change: `--self-test` passed (15 snippet files), the default scan reported 0 findings over 154 C++ and 13 CMake files. `HISTORY_DIRS` (prefixes) already covers every new history file and `stages/history.cpp`.

Added rule **`history-live-iteration`** (A1): in library history code (`HISTORY_DIRS`), a range-for over `State::civs()`, `settlements()` or `relations()` (through `.` or `->`, or the members `civs_`, `settlements_`, `relations_`) whose body calls `found_civilization(`, `found_settlement(`, `open_relation(` or `close_relation(` is reported. It reads the loop header for a top-level `:` (not `::`, not a classic `for`), then the braced or single-statement body. It does not follow helper calls and does not find references held across a founding outside a loop; the README states this. New self-test snippet `selftest/bad_history_live.cpp` (four forms reported; near misses: a read-only loop, ids up to a count taken first, changes collected then applied, a founding after the loop, the pattern in a comment and a literal), and `selftest/good_history_scope.cpp` gains a geography-stage case where the rule does not apply. README updated (rule table, area note, self-test table). After the change: self-test passed (16 snippet files, 0 failures); default scan 0 findings (the engine and stub have no such loop).

## 4. What was run

All in a worktree of `origin/p3/04-int` at `40e1064` (branch `p3/04-audit`), build tree outside the source tree, `linux-gcc-release` only (the machine is shared with a validation run).

| Run | Result |
| --- | --- |
| `python3 -B tools/lint/determinism_lint.py --self-test`, then the default scan, before the lint change | passed (15 files); 0 findings |
| The same after the lint change | passed (16 files, 0 failures); 0 findings |
| `generate --seed 42` (1024, default settings) with `--threads 1`, `--threads 5`, and `--until history --threads 2` | the three files are byte-identical (SHA-256 `6e7491f6ba304cc9…`); `[history]` equals spec 11.1 (1154 simulated years, 33 events) |
| `hash --detail` of that world and of `--seed 11 --history-detail dense --threads 3` (case `hist-dense-1024`) against `worldgen/tests/golden/p3-04-engine.txt` (written by the independent Python reference) | all 13 table lines equal, both worlds |
| `worldgen-tests -ts=history-slow --no-skip -sf='*history_engine*'` (the 1024 world cases: tables, `[history]`, phases, every rendered sentence byte for byte; worked example with both `ForcedTension` variants; R1 and Y2 on the 1024 cases) | 4 of 4 cases pass, 10,903,015 assertions |
| `worldgen-cli-tests -sf='*cli_history_test*'` | 6 of 6 pass |
| `worldgen-tests -sf='*history_engine*'` (default suites) | 38 of 45 cases pass, among them R1, R2, Y2 and the refusals (512), threads 1/2/5, Y3 draw independence, Y4 call order, item 24 (no hidden state), the bookkeeping and quiet-era cases, the 512 world cases. **7 fail**, see below |

**The 7 failing cases are expectations from before r3, not determinism defects** (for the lead and the test writer; none shows a difference between runs, thread counts or builds):
- `5.4 cap guard`, `3.4 entry checks`: expect the Unicode messages `6 × active_years … − 30`; r3 4.1 made them ASCII (`x`, `-`), as implemented.
- `5.6 edge cases E1-E10`, `16.4 every schedule case through run_engine`, `6.5 tension guards`: expect the pre-r3 trigger ids (a `flare_up` event not taking an id) and `ForcedTension(25, 5000, 1)` giving 900 simulated years; r3 6.3 and 7.4 give phase 4's trigger e3 and 216 years, as implemented.
- `16.2 item 16 hand-made tuning`: expects the unclamped floor (439 against a cap of 438) and packed starts that now hit the P3-03 A1 guard `age 6: end_year below start_year`; r3 5.1 clamps the floor to the cap.
- `4.2 report_events through the loader`: the loader's messages are not the texts the test looks for (it compares whole messages with the golden list); r3 4.2 makes `report_events` a kind-level check of kind `history`, reported in the P0-03 7.5 form `check 'report_events' failed for 'history.report': <message>`, as implemented.

These belong to the test writer (bring the tests to r3) and the validation specialist (the merge gate needs every preset green); they do not change the verdict, since the implementation follows r3 and the golden vectors of the independent reference agree with it.

Not run: `linux-clang-asan` and any second compiler (instructions: shared machine, `linux-gcc-release` only). The cross-compiler comparison of the 15 `world-hashes.sh` cases, including `hist-dense-1024`, is the validation specialist's; nothing found in this audit predicts a difference.
