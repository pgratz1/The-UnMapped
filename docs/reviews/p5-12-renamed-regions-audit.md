# P5-12 Regions renamed by history: determinism audit

Auditor: determinism-auditor (Opus), 2026-11-23.
Scope: `git diff origin/p4/03-int...origin/p5/12-int -- worldgen/include worldgen/src tools/worldgen-cli tools/ci tools/lint game/data`, at `origin/p5/12-int` 535dd47. Tests and references were read as context only. Spec: `docs/specs/p5-12-renamed-regions.md` (r0) with the amended sections it names. Generator 0.22.0, schema 8, history report format v3.

**Verdict: pass with advisories.** I found no blocking finding. There are two advisories (A1, A2).

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed, 19 snippet files, 0 failures.
- `python3 -B tools/lint/determinism_lint.py`: 176 C++ files and 13 CMake files scanned, 0 findings.
- The diff adds no `determinism-lint: allow` suppression.
- I added no lint rule. The diff has no new construct a rule could catch reliably. A1 is a missing data check, not a code pattern.

## What was checked

1. **Random streams** (`worldgen/src/history/renaming.cpp`).
   - Two new streams, `hist.renaming.chance` and `hist.renaming.name`. They are appended at the end of `streams::all_stream_names` (size 54 to 56), after `hist.conflict.toll`, as spec 3 requires.
   - Draws are keyed only:
     - the chance draw is `ctx.draw(chance_key_, e, r, k)` with k = rule index + 1 (`renaming.cpp:336-337`);
     - the name draw is `ctx.draw(name_key_, e, r, 0)` (`renaming.cpp:365`).
   - `StepContext::draw` adds the year. The (e, r, k) key is unique per event, region and rule, and each stream has its own key, so the chance and name draws cannot collide.
   - There is no sequential generator. Whether a draw happens (only after the magnitude, salience and capital tests pass) does not affect any other draw.
2. **Iteration order.**
   - The log is read in ascending id, from `cursor_` to `last_event_id()` (`renaming.cpp:454-457`).
   - Rules are read in data order, and regions are chosen in ascending id (`:460`).
   - Windows are vectors in event order. Culture forms are kept in culture-id order.
   - There are no unordered containers, and nothing is keyed by pointer.
   - The name registry is `std::set<std::string, std::less<>>`, and the subsystem only reads it (`contains`) or adds to it.
3. **Total orders.**
   - Candidate choice is by largest weight, then smallest event (`:466-472`). This is total:
     - the schema check `renaming_rule_weights` (`kinds_history.cpp`) makes weights unique;
     - one (event, rule) pair gives at most one candidate.
   - `sorted_rows` uses `stable_sort` on (region, from_year). The key is unique because `region_gap_years >= 1`, and the writer's `require_next_pair` enforces strictly ascending keys.
   - `digest_tables.cpp` sorts integer culture ids with `stable_sort`.
   - `region_land_nouns(rows)` uses `stable_sort` and `unique` on integer labels.
4. **Land nouns, stage against stored rows** (`regions.cpp`, `region_lands`).
   - The stage and the history stage share one function, `region_lands`, so they apply the same rules.
   - The stored-row overload sums `region.cells` per landmass and renumbers the labels in ascending order. P1-06 guarantees three things:
     - every land cell is in exactly one region (RG7, invariant 3);
     - every landmass has a region (RG6);
     - the labels are 1 to M.
   - So the sums equal the stage's `landmass_cells`, and the relabelling is the identity. The "largest landmass, ties to the smaller label" rule therefore gives the same answer on both paths.
   - The engine reads `HistoryInputs.regions` both in memory (`copy_rows`) and from the file. Both are the same row values.
5. **P3-04 6.9 rules.**
   - `choose()` appends inside the loop over its local `candidates` vector, not over a live list of `State`.
   - Everything the event needs (settlement, namer, culture, name) is read into locals before `ctx.append` (`:418-438`).
   - `mine` (a reference into `renamings_[region - 1]`) is used only before the append. After the append, `renamings_` is indexed afresh (`:442`).
   - `append_event` (`engine.cpp:650`) calls back into no subsystem, so an append cannot resize `renamings_`, `windows_` or `candidates`. `best` points into the local `candidates[r - 1]`, which nothing changes during `choose()`.
6. **Quiet budget.**
   - In Q4 the namer's budget is tested before the append (`:413`).
   - The engine charges the first civilization participant, which is the namer, or 0 when there is none.
   - W9 therefore cannot fail, and the outcome does not depend on scheduling.
7. **Integer arithmetic.**
   - Magnitudes and window sums use `checked_add` in int64.
   - `chance_pm` uses `checked_mul(p, 2^32)` with p <= 1000.
   - `renaming_cap` computes `regions * permille / 1000` in int64 with permille <= 1000 (schema), which cannot overflow.
   - Year differences are bounded by the span.
   - There is no floating point in the diff. Its one `Fx` use is `Fx::from_raw` in `chance_pm`.
8. **Text.**
   - Every new literal is ASCII.
   - Lower-casing uses the project's ASCII `to_lower` and `is_lower` (`names.cpp`), not `<cctype>`.
   - The `rename_form` grammar allows only ASCII letters, space and `'`.
   - The report adds no locale or float formatting.
9. **Database.**
   - Table `region_name` is `STRICT, WITHOUT ROWID` with PRIMARY KEY (region, from_year).
   - It is executed after `relation` (`DDL_STATEMENTS` 27).
   - It is written in ascending key order, with `require_next_pair` enforcing that order (`writer.cpp`).
   - It has no timestamps. `hash` covers it through the ordered table list.
   - The reader appends ORDER BY each primary-key column COLLATE BINARY.
10. **Damaged files.**
    - `read_region_name_rows` type-checks each column.
    - `inspect::history_stats` shape-checks `region_name` (text column 2).
    - `region_name_rows_problem` (`event_catalogue.cpp:1055`) checks that each row has a known region, `from_year` in [0, E-1], name text, a known culture, and an event that is a `region_renamed` of the same region and year. It also checks that every `region_renamed` event has a row.
    - It runs from both `HistoryView::load` and `read_history_tables`. `history_reads_disagree` compares the two reads.
    - `region_name_range_` dereferences its `lower_bound` only after `sorted_has` has confirmed the region.
    - `region_name_at_event` and `region_name_at_year` return null for an unknown region.
    - Index arithmetic on event ids uses checked ranges. See A1 for the one gap.
11. **P5-01 audit advisory A1** (`docs/reviews/p5-01-followups-audit.md`): **closed.**
    - `inspect::settlement_row` (`history_report.hpp`) returns null when the id is out of range.
    - Both sites now use it: `last_held_by` and `hidden_at_end` (`history_report.cpp`).
    - The new `refuge_hidden_at_end` in `digest.cpp:1288-1294` carries its own bounds check.
12. **Hard rule 7.**
    - `SCHEMA_VERSION` 7 to 8.
    - Spec: P5-12 7.2 and P3-04 r21 23.4.
    - `docs/specs/schema.md` is updated (current version 8, DDL, row-order note).
    - Proposals entry "Schema 8: table `region_name` (P5-12)" is in `docs/design/proposals.md`.
    - `GENERATOR_VERSION` 0.21.0 to 0.22.0.
13. **Data** (`game/data/history/renaming.toml`).
    - Six rules with unique keys and weights. The schema bounds every integer field.
    - `renaming_rules_problem` checks each rule's kinds, values and roles against the compiled catalogue before the engine runs. `event_value` therefore cannot hit its fatal error on shipped data.
14. **Build flags.** No CMake file is in the diff, and the lint's flag scan is clean.

## Blocking findings

None.

## Advisories

**A1. Nothing checks that `region_name` rows are strictly ascending in (region, from_year)**

Where:
- `worldgen/src/history/event_catalogue.cpp:1055-1095`, `region_name_rows_problem`;
- consumer `worldgen/src/history/timeline.cpp:450`.

This is currently safe:
- the table's primary key makes (region, from_year) unique;
- the reader asks for primary-key order.

The risk: a damaged b-tree, or a hand-edited file that re-declares the table with another key, could give rows out of order or with repeated keys. Then:
- `Timeline::build` checks only that rows are grouped by region (`timeline.cpp:157-175`);
- `Timeline::region_name` runs `std::upper_bound` over a range that may not be partitioned, which violates the precondition. libstdc++, libc++ and the MSVC STL bisect differently, so the viewer and timeline text could differ across platforms;
- `HistoryView::region_name_at_year` and `region_name_at_event`, which take the last matching row by linear scan, could disagree with the timeline.

`region_control` has the corresponding guard (`check_control_runs`); `region_name` does not.

Suggested fix: in `region_name_rows_problem`, reject a row whose (region, from_year) is not above the previous row's, with "corrupt table 'region_name': row (r, y): not above the previous row".

**A2. Rendering the history report is quadratic in the number of renamings**

Where: `worldgen/src/history/event_catalogue.cpp:1083-1093`.

The check that every `region_renamed` event has a row runs `std::any_of` over all rows for each such event, so it costs events × rows. This is not a determinism risk: the result does not depend on order. It is advisory only because the check runs on every report and viewer load.

The world cap keeps rows at or below R × 150 / 1000 (at most about 10,000 at 65,535 regions). Suggested fix: walk the rows' `event` column sorted alongside the events, or mark found events in a `std::vector<std::uint8_t>`.

## Verdict

**Pass with advisories** (A1, A2, advisory only). No blocking findings.
