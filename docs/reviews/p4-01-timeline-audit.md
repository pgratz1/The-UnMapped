# P4-01 timeline, civilization colours, territory image: determinism audit

Auditor: determinism-auditor, 2026-11-22.
Change: `git diff origin/main...origin/p4/01-impl` (library and CLI), reviewed on `p4/01-int` at 8f8c548.
Spec: `docs/specs/p4-01-timeline.md`.

No world bytes change (generator 0.20.0, schema 7). This is what has to match across GCC, Clang and MSVC: the timeline's answers, the territory RGB buffer and PNG, and the text of `report --history`.

This was a reading audit. As instructed, no suites were rebuilt or rerun. Tests were reported green on gcc-release, clang-debug and asan, with the independent reference matching every golden line.

## Verdict

**Pass with advisories.** No blocking findings.

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed (18 snippet files, 0 failures).
- `python3 -B tools/lint/determinism_lint.py`: 172 C++ files and 13 CMake files scanned, 0 findings.
- No new rule was added. Nothing in this change is a new pattern that a rule could catch. The one suppression is reviewed below (item 6).

## What was checked

1. **Free-region `since` and the binary search in `Timeline::holder_at`** (`worldgen/src/history/timeline.cpp:286-307`).
   - The search uses `upper_bound` with `y < r.from_year`, then steps one row back. That gives the last row with `from_year <= Y`.
   - The interval is held when `to_year` is NULL or `Y < to_year`. That is half-open [from, to), as in spec section 4.
   - If no row has started, the region is free with `since` 0. If the last started row has ended, `since` is its `to_year`.
   - That last `to_year` is the largest `to_year <= Y` because the rows do not overlap. `read_history_tables` enforces this ("overlaps the previous interval", `worldgen/src/history/digest_tables.cpp:459`). The rows arrive in primary-key order (`region`, `from_year`), so `from_year` is unique and ascending within each run, and `upper_bound`'s partition precondition holds.
   - Y = 0 and Y = E are correct. `to_year <= E - 1` is checked, so at Y = E an ended row is always free and an open row is always held: the end state.
   - Settlements (`:318`) and phases and ages (`covering`) use the same search. `phase_at(E)` = 0 and `age_at(E)` = the last age, as in the spec.

2. **Merge in `Timeline::events_at`** (`timeline.cpp:365-395`).
   - `starting` is a contiguous id run, found by `lower_bound`/`upper_bound` on `event_year_`. The build checks that `event_year_` is non-decreasing (`timeline_fatal` "dated before").
   - The multi-year ids are kept only when `event_year_ < Y <= event_end_`, so they are disjoint from `starting`.
   - Both lists are ascending, and the merge is a plain two-pointer merge. The output is ascending id and does not depend on the implementation.
   - At Y = E nothing is dated E, so both lists are empty.

3. **The `drawn` flag** (`worldgen/src/inspect/render.cpp`, `shaded_colours` and `paint_rivers`).
   - The flag is write-only alongside the existing colour writes, and it is null by default. With a null flag the colour code paths are unchanged: `render_composite_map` and `render_composite_biomes` pass none.
   - `render_composite_regions` now goes through `detail::regions_composite_cells`. It runs the same steps, the same border rule and the same `fill_buffer`, so its pixels are unchanged. Spec invariant 9 also tests this equality.
   - The flag marks step 3 (water 2), step 4 without `river` (water 1) and every painted entry of the river list. "Fillable" (`worldgen/src/inspect/territory.cpp:153`) reads that flag and the border flag. It does not re-derive them from colours, as spec 5.6 item 1 requires.

4. **Integer widths** (`territory.cpp:42-56`).
   - `hashed_channel`: the byte is at most 255, so `byte * 160 / 255` is at most 160 and the channel at most 208, all in `uint64_t`. The shift `8U * j` is at most 16.
   - `light`, `dark` and `deep` widen to `unsigned` before multiplying. The results are at most 255 (light: c + (255 - c) / 2), so the casts back to `uint8_t` never truncate.
   - The territory pixel uses the existing `shade_colour`, which is `uint64_t` with a clamp at 255, and `hillshade_factor`, which is `int64_t` clamped to 160..352 and stored in `uint16_t`.
   - There are no floats anywhere in the new files.
   - `civilization_shade` uses `ordinal % 4` on an int64 that has already been checked to be non-negative.

5. **`std::map` in `civilization_colours`** (`territory.cpp:86`).
   - It is keyed by the integer race id and used only as a per-race counter, with lookup by key. It is never iterated, so the colours depend only on the civilization order (primary key, ascending).
   - The race lookup is a linear scan over `tables.races` in primary-key order.
   - `race_base_colour` compares keys as bytes. `encode_bytes(string_view)` casts each `char` to `uint8_t`, so `char` signedness does not matter.

6. **Suppressed `std::sort`** (`timeline.cpp:140`): justified. It sorts plain `int64_t` values, where equal elements cannot be told apart, so every correct sort gives the same sequence. `unique` then removes the duplicates.

7. **`history_reads_disagree`** (`digest_tables.cpp:635`).
   - The body and the message are byte-identical to the removed `reads_disagree`. `history_report.cpp:668` calls it at the same point, so the `report --history` text and the error text are unchanged.
   - `load_history` (`timeline.cpp:494`) adds `timeline_reads_disagree` (`:63`). It is a pure check of ids, order and references, and gives the same message and error code. It only ever turns a changed-between-reads file into an error.

8. **CLI `--year`** (`tools/worldgen-cli/cli.cpp:917`, `:973`).
   - It is parsed by `parse_canonical_u64`: canonical digits, then `std::from_chars`. There is no locale, sign or leading zero.
   - It is compared unsigned against E, which is non-negative on checked data.
   - The default file name uses `db::format_i64` (`std::to_chars`), so it is locale-free.
   - The PNG path is unchanged: the same buffer goes through the same vendored writer.

9. **General**
   - There are no unordered containers, no ordering by pointer, no time, environment or thread use, and no `<cmath>` or `<random>` in the new code.
   - Every list in the timeline is built in id or primary-key order.
   - Every `Cell` and `YearState` field is assigned before it is read: vectors are value-initialized or fully overwritten.
   - MSVC `/W4`: every narrowing is an explicit `static_cast`, and the signed/unsigned comparisons are cast on both sides (`index_of`, `territory_fill`). There are no C4996 functions, and `[[noreturn]]` on `timeline_fatal` covers the lambdas' control paths.

## Advisories

- **A1** (`tools/ci/world-hashes.sh`): the territory image is not in the cross-compiler `compare`. Spec 13.6 item 14 recommends it, for the build engineer.
  - Today the RGB buffers are pinned by `worldgen/tests/golden/p4-01-timeline.txt`, which runs under `ctest` on every preset, MSVC included. The 1024 cases only run in the slow suites, which run on the two release presets.
  - That is enough for now. Adding `<preset>-<case>.territory.txt` would also cover the PNG writer's bytes across compilers.
- **A2** (`territory.cpp:169-193`): `render_territory` repeats the layout loop of `fill_buffer` (`render.cpp:377`) instead of sharing it. The two are identical today, and spec invariant 9 tests that. Sharing one helper through `render_internal.hpp` would keep them from drifting apart.
- **A3** (`timeline.cpp:286`): `Timeline::build` checks that control rows are grouped by region, but not that `from_year` ascends within a run or that the rows do not overlap. `holder_at` relies on both, and they are guaranteed only because `load_history` runs `read_history_tables` first. A caller that builds a `Timeline` from hand-made tables gets defined but possibly wrong answers. A cheap check in `build` (`from_year` above the previous `to_year`) would make the precondition local. Not output-affecting on any checked world.
