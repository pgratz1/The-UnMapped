# P4-04 settlement and place markers, Part A: determinism audit

Scope: `git diff origin/main...origin/p4/04-impl -- worldgen/include worldgen/src tools/worldgen-cli tools/ci tools/lint` (commit 301a0a2), read against spec `docs/specs/p4-04-markers.md` r1 sections 3 to 7, 10 to 13. Tests and the gdext viewer were read as context only. Audited on branch `p4/04-audit` (from `origin/p4/04-int`, 82eb66a); not built (integration ran gcc-release, clang-debug and asan green).

**Verdict: pass.** No blocking findings. Three advisories, none of which can change output.

## What was checked

| Concern | Result |
| --- | --- |
| Floating point, `<cmath>`, `<random>`, time, environment, threads | None in `markers.cpp`, `markers.hpp`, the moved functions or the CLI change. Glyph predicates are `int32` squares and absolute values with radius at most 11 (`markers.cpp` 531-541); no value near overflow. |
| Narrowing (MSVC `/W4`, `-Wconversion`) | Every narrowing is an explicit `static_cast`: kind to `uint8_t` only after the 1..5 test (691), sizes to `ptrdiff_t` (362-363), glyph offsets to `size_t` after adding the radius (586-587, 761-762). |
| Unordered iteration, pointer order | No unordered container. The only pointer is `found` in the place grouping (453-467), used for identity, never for order. `civilization_colours` uses an ordered `std::map` whose order is not used (P4-01, audited). |
| Capital role match | `capital_role_seqs` (99-115) finds kind and role by key and role name in the stored catalogue (byte comparison) and stores the role's seq; the participant test is `role == seq && entity_type == 3` (304). Stored role seqs are checked consecutive per kind and kind ids consecutive (`event_catalogue.cpp`, stored-catalogue load), so `index_of(kind->id)` and the position-indexed `kind_symbols` agree. Equivalent to the spec's name match. |
| Binary search in `run_settlement` (361-370) | Runs are filled in event id order (266-321). `load_history` refuses an event log whose ids are not 1, 2, ... or whose year decreases with id (`timeline_reads_disagree`, `timeline.cpp` 75-77, run on every load before `Timeline::build`), so a damaged file cannot reach the search unsorted. `upper_bound` with `y < e.year` gives the last entry with year at most Y, so ties in year take the largest event id, as 3.2 "last entry" asks. |
| Place grouping (440-469) | `YearState::events` is ascending (`timeline.cpp` 412-420); linear search by cell keeps first-seen order, so places are in ascending smallest event id and each place's ids ascending; symbol is the maximum by enumerator value (sack highest). Deterministic. |
| Canvas clipping (743-789) | The early skip rejects only glyphs wholly outside the canvas and is evaluated in `int64` before `x + dx` is formed, so any stored `x`, `y` (including extreme seed or settlement cells) is safe. Per-cell tests keep 0 <= cx < width, 0 <= cy < height; `put` then indexes at most `((height k - 1) width k + width k - 1) 3 + 2`, below `rgb.size()`, which `draw_markers` checks equals 3 (width k)(height k) with a 64-bit overflow guard (807-813). |
| Draw order | 6.1 order: passes in id order, places in list order, then four passes over settlements in ascending id (822-848). Overlaps and shared cells are settled by that fixed order; no tie is left to the container. |
| Glyph bounds | Every set cell lies inside its grid: settlement grid radius o + 1 with outline at o (606-616), city centre S(1), crosses at |d| <= 2 in radius 2 and 3, chevron dy in -1..1 in radius 3, ring C(10) in radius 11 (C(10) reaches |dx| 10). The halo loop tests neighbour bounds (568). Grids are value-initialised to `GLYPH_NONE` (551); no uninitialised byte in a glyph or the image (the image is `render_rgb`'s). |
| Damaged world.db | `region` and `pass` reader errors are returned; pass rows go through the report's `read_pass_row` with map size from the reader; region ids compared to the history's (180-187). Every other datum `load` uses is checked by `load_history` before `load` runs: settlement and civilization ids 1..n, census keyed (settlement, year) by the primary-key `ORDER BY` of `table_rows`, census kind 1..5, status 1..4 and owner a civilization (`digest_tables.cpp` 398-443), event kind, region, settlement, participant civilization ids and cause ids (`timeline.cpp` 63-101 and the view load). So the `markers_fatal` calls in `load` (201, 212, 226, 241, 270, 274, 280, 293, 309) and `draw_fatal` "no colour" / glyph base 0 are unreachable on accepted data, matching spec 13 "Errors"; none is undefined behaviour if reached. `std::bad_alloc` becomes `out_of_memory`. |
| "First census row with status 3" | Census rows arrive in (settlement, year) primary-key order, so the first such row is the smallest year. |
| Moved code, report bytes | `read_pass_row` moved verbatim from `report.cpp` to `geography.cpp` (byte-identical body). `kind_word` / `status_word` give the same word as the old `word(TABLE, code, 1)` for every code (in range: same table; out of range: "?"); the new form also removes a latent `code - 1` overflow at `INT64_MIN`. `report` and `report --history` bytes unchanged. |
| CLI (`cli.cpp`) | Check order as 7: usage (`--faded`, `--year`), open, layer checks, `load_history`, `MarkerIndex::load`, year range, draw. `MarkerOptions{faded, true, true}`. Progress notes go to the verbose stream only. Default names `<stem>.<name>-<Y>.markers[-faded].png`. |
| `world-hashes.sh` | POSIX shell (`cut`, `mv`, `rm`, `echo`; nothing Bash-only), LF output under Git Bash. Scratch PNGs in `$out_dir/png-<preset>` (outside the tree; the output directory check refuses the source tree), deleted on success, directory removed at the end. `markers.txt` is in `HISTORY_KINDS`, so `compare` compares and names it. A failing territory step skips markers, leaving `markers.txt` missing, which `compare` reports. |
| ASCII-only literals | No non-ASCII byte in the changed source files. |
| Lint | `--self-test`: 19 snippet files, 0 failures. Scan: 178 C++ files, 13 CMake files, 0 findings (the new files are in scope). No new pattern worth a rule. |

## Blocking findings

None.

## Advisories

- **A1** `tools/ci/world-hashes.sh` 49-57 (`markers_hashes`): on a render failure the function returns before removing `m<year>.png` and `<case>.markers.txt.partial`. Both are outside the source tree, the next case clears the PNGs, and the next run clears the partial file, so nothing reaches `compare`; `territory_hashes` behaves the same. If tidiness matters, remove both on the failure path.
- **A2** `worldgen/src/inspect/markers.cpp` 746-747: the early skip tests `x > width + r` (and `y > height + r`) where a glyph is already wholly off the canvas at `x >= width + r`. One cell loose, so a glyph at exactly `width + r` runs the loops and every cell is clipped. Correct and safe; `>=` would match the comment.
- **A3** `worldgen/src/inspect/markers.cpp` 244, 497: `civ_ended_` stores -1 for "not ended" and `capital()` tests `civ_ended_ < 0`. Safe because `read_history_tables` checks `ended_year >= founded_year >= 0`, but it differs from the timeline's `std::optional` form (`timeline.cpp` 490). `capital()` is a test and diagnostic entry; `markers_at` uses `YearState::civilizations`. Keeping the optional would remove the coupling.

## Verdict

Pass.
