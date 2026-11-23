# P4-03 history view, library part: determinism audit

Auditor: determinism-auditor, 2026-11-23.
Change: `git diff origin/p5/01-final...origin/p4/03-int -- worldgen/include worldgen/src tools/worldgen-cli tools/ci tools/lint`, reviewed on `p4/03-int` at 2502f6b.
Spec: `docs/specs/p4-03-history-view.md` sections 7 to 9 and 11.4. The viewer sections are not in scope.

No world bytes change (generator 0.21.0 from P5-01, schema 7). These outputs have to match across GCC, Clang and MSVC:
- the territory RGB buffer and PNG, which `tools/ci/world-hashes.sh` hashes into `territory.txt`;
- the text of `report --history`, which now gets its phase words through `phase_kind_word`.

This audit was done by reading the code. No suites were rebuilt or rerun, as instructed. The integration branch was reported green on gcc-release, clang-debug and asan.

## Verdict

**Pass with advisories.** There are no blocking findings.

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed (19 snippet files, 0 failures).
- `python3 -B tools/lint/determinism_lint.py`: 174 C++ files and 13 CMake files scanned, 0 findings.
- No rule was added. The change has no new construct that a rule could catch. It adds no suppression, and `tools/lint/` and `tools/ci/` are untouched.

## What was checked

1. **Colour table of 7.2** (`worldgen/src/inspect/territory.cpp:25-48`, `race_civilization_colour` at `:146-154`).
   - All 24 constants match the spec table entry by entry.
   - Entry 0 is returned by `race_base_colour` (`:127-132`).
   - Keys are compared as `std::string_view ==`, which is a byte comparison with no locale and no hashing.
   - `ordinal % 4` runs only after the `ordinal < 0` check, so it is in 0..3, and the cast to `size_t` is explicit.
   - Keys without a row keep the P4-01 path (`hashed_base`, `civilization_shade`) unchanged.
   - `civilization_colours` still counts ordinals in a `std::map`, which is ordered, and walks civilizations in table order.
   - A civilization whose race id is missing raises FatalError.
2. **`dim`** (`territory.cpp:70-73`).
   - `unsigned(v) * 12U / 16U` is unsigned integer arithmetic with floor division. The maximum is 191, which fits in a byte.
   - The narrowing back to `uint8_t` is an explicit `static_cast`, so MSVC `/W4` (C4244) has nothing to report.
   - `shade_colour` (`render.cpp:588`) is unchanged: integer, with rounding by `+128 >> 8` and a clamp.
   - The spec example gives dim((40, 190, 70)) = (30, 142, 52), and the code agrees.
3. **Rule 8.2: inner border candidates** (`TerritoryBase::build`, `territory.cpp:206-244`; the border pass, `render.cpp:847-886`).
   - The E and S neighbour values come from the composite's own border pass (`out.east`, `out.south`). They are set by the same `east` and `south` booleans that make the cell black, so they are not re-derived.
   - The `ids[i + 1U]` and `ids[i + w]` reads are guarded by `x + 1 < w` and `y + 1 < h`, as before.
   - `east` and `south` are assigned to the full grid size, so every entry is written.
   - A candidate needs v > 0, e >= 0 and not drawn, which is the same test as for a fillable cell. The only difference is the border flag, as the spec says.
   - The hillshade factor is now also computed on candidate cells. It is the same integer function as before.
   - `Cell` packs to 12 bytes with no padding (3 + 1 + 4 * 2). It is never hashed or serialized; only the RGB buffer is.
4. **`render_territory` and `territory_pixel`** (`territory.cpp:104-122`, `:250-266`).
   - Every lookup is bounded:
     - `fill[cell.region]` only after `cell.region < fill.size()`;
     - `holder_of` returns 0 for `v >= holders.size()` (`:99-101`);
     - `cells[i]` with i < w * h, where `cells_` has `e.size()` = w * h entries (the dimensions are checked by `composite_regions_problem`).
   - Civilization ids from `holders` are only compared for equality and never used as an index.
   - An empty `holders` gives c = 0, so B is drawn. That matches 9.2: with no holders, no inner border is drawn and the image equals the `regions` composite.
   - The output buffer is checked against `territory_rgb_size`, and `fill_buffer` writes every byte. No byte is left uninitialised.
   - The output order is cell index order, with no unordered container and nothing that depends on pointers.
5. **`territory_holders`** (`territory.cpp:195-204`).
   - Size and year check come from the shared `region_vector_size` (`:84-96`), which is the code moved out of `territory_fill` unchanged.
   - Region ids outside 1..65535 are skipped before indexing, and `holders[r]` with r <= largest is in range.
   - It walks `timeline.region_ids()`, a sorted vector.
6. **Damaged `world.db`.**
   - A region value from the layer is a `uint16`, bounded against `fill` and `holders`.
   - Region ids from the tables are checked and ordered by `read_history_tables` and again by `Timeline::build`.
   - Race ids are found by linear search, with FatalError when missing.
   - Civilization ids are bounded in `territory_fill` (FatalError "no colour for civilization") and compared only in `render_territory`.
   - I found no index taken from database data without a bound.
7. **A2 closed** (spec 9.3).
   - `detail::fill_buffer` (`worldgen/src/inspect/render_internal.hpp:22-51`) is the former `render.cpp` template, character for character. The only change is the comment's reference to `composite_buffer_size`.
   - `render_rgb`, `render_shaded`, `render_composite_regions` and `render_territory` all call it, so the duplicate loop in `territory.cpp` is gone.
8. **A3 closed** (spec 9.4; `worldgen/src/history/timeline.cpp:103-122`, called at `:157`).
   - The check runs after the run-grouping check, so runs are contiguous and every row's region is a known region.
   - For each row r in order, its own `to_year > from_year` check comes first. The pair check of the previous row p of the same region against r comes second: p open, or `p.to_year > r.from_year`, is an overlap. This is the order the brief and the reference state.
   - The check does no arithmetic, so it cannot overflow, and the only index, `rows[i - 1]`, is guarded by `i == 0`.
   - The messages match 9.4 (the "timeline: " prefix comes from `timeline_fatal`).
   - The check cannot fire on a file that `load_history` accepts. `read_history_tables` already rejects both cases (`digest_tables.cpp:454-460`): `to_year < from_year + 1` is the same as `to_year <= from_year`, and its overlap test is the same expression. Equal boundaries are accepted in both places.
   - Non-ascending `from_year` within a run is caught by the overlap test, as 9.4 argues. `holder_at`'s `upper_bound` precondition is now enforced locally.
9. **`phase_kind_word`** (`worldgen/src/history/digest_tables.cpp:659-667`; `history_report.cpp:61-67`).
   - The strings are those of the removed `PHASE_KIND` table.
   - The old `word(PHASE_KIND, kind, 1)` gave "?" outside 1..4, and `phase_word` maps the empty result to "?". So the report bytes are unchanged for every kind value, including corrupt ones.
10. **CLI** (`tools/worldgen-cli/cli.cpp:1015-1016`). `holders` comes from the same timeline and year as `fill`. The year was already validated, and both functions use the same check. Options, messages and exit codes are unchanged.
11. **ASCII.** Every changed source and header is pure ASCII, comments included (checked with a byte scan). The `non-ascii-literal` rule is clean.
12. **Golden diff of 11.4.**
    - In `p4-01-timeline.txt`, the fixture cases (h1 to h10, t1-edges) change only in their `static` lines and their `table meta` line. The meta line changes because the generator version is now 0.21.0. No `year` or `years` line changes in those cases, and h11-v2 is new.
    - The world cases also change in their `year` and `years` lines. The base file on `p5/01-final` is still the 0.20.0 recording, so these lines move because of P5-01's histories, not because of P4-03. Commit 57187aa records the test writer's three-step check: `--rules p4-01` reproduces the old file; the P4-03 rules on 0.20.0 inputs change only `static`, `territory` and the header; then the re-record at 0.21.0. I did not rerun the reference.
13. **Floating point, `<cmath>`, time, threads, `<random>`.** The diff adds none of them.

## Advisories

- **A1** (`worldgen/include/worldgen/inspect/territory.hpp:105-119`, `territory.cpp:104-122`): `render_territory` takes `fill` and `holders` as two independent spans. Nothing ties them to the same timeline and year. If a caller passes a mismatched pair, for example a `fill` cached from one year and `holders` from another, the output is still defined, but some inner borders follow one year and some fills the other, so the image no longer follows rule 8.2 for any single year. Both current callers are consistent: the CLI builds both from one `(timeline, year)`, and the viewer is to rebuild them together under 3.4. Suggested fix for the engineer, at a later change: a convenience overload that takes `(timeline, colours, year)`, or a debug assertion that `fill[v].has_value()` matches `holders[v] != 0`. This does not affect output today.
- **A2** (process, outside this diff): `origin/p5/01-final` has generator 0.21.0 but still carries the 0.20.0 `worldgen/tests/golden/p4-01-timeline.txt`. On that branch, the non-slow test "P4-01 13.4: the 512 world cases against the golden file" (`worldgen/tests/timeline_golden_test.cpp:87`) would compare 0.21.0 worlds with 0.20.0 lines. `p4/03-int` re-records the file, so the integration is consistent. If P5-01 is merged to `main` on its own before P4-03, the validation specialist should confirm whether it needs its own re-record.

## Blocking findings

None.
