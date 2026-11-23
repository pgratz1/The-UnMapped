# P4-03a territory colour follow-ups: determinism audit

Auditor: determinism auditor, 2026-11-23. Spec `docs/specs/p4-03a-territory-colour-followups.md` r1 with `docs/specs/p4-03-history-view.md` r4 7.2, 7.5, 8.2 and 9.2.
Diff audited: `git diff origin/p4/03a-base...origin/p4/03a-impl -- worldgen/include worldgen/src tools/worldgen-cli tools/ci tools/lint` (the library diff of `origin/p4/03a-int` is identical; `int` adds tests, the reference and the golden only). The gdext viewer was read for its use of the new call only.

## Verdict

**Pass.** No blocking findings. Three advisories, none of which can change output.

## What was checked

- **Arithmetic.** `luma` (`worldgen/src/inspect/territory.cpp:70`) is `(77R + 150G + 29B) / 256` in `unsigned` on widened `uint8_t` channels; its maximum is 65280 / 256 = 255, no overflow on any target. `dim` (line 81) picks n in {10, 12} by `luma > 175` (line 82, the constant at line 77) and computes `v * n / 16`, at most 191, narrowed by an explicit `static_cast<std::uint8_t>`. `half` (line 88) is `v * 8 / 16`, at most 127, explicit cast. No floating point, no `<cmath>`, no implicit narrowing (clean under `-Wconversion -Wsign-conversion` and MSVC `/W4`). The four colour values of `RACE_COLOURS` (lines 31 to 38) are integer literals in range.
- **Uninitialised data.** `Rgb` has default member initialisers (`render.hpp:102`), so `Cell::half` (`territory.hpp:86`) is (0, 0, 0) on every cell `resize` creates; every other `Cell` member already had an initialiser. `RegionsCells::unbordered` is `assign`ed to the full cell count before the border pass (`render.cpp:863`). The output buffers are sized by `territory_rgb_size` and every byte is written by `fill_buffer` from a whole `Rgb`. `Cell` now has one padding byte (after `kind`); nothing hashes, `memcmp`s or serialises a `Cell` (grep over `worldgen/`, `tools/worldgen-cli/`, `gdext/src/`), so the padding never reaches output.
- **Bounds.** `cells.unbordered[i]` with i < `e.size()` = the grid size of `regions_composite_cells`; `holder_of` guards `v < holders.size()` (unchanged); `fill[cell.region]` is read only after `cell.region < fill.size()` on both paths (`territory.cpp:128`, `146`); `territory_pixel` returns before any `holders` read when it is empty (line 134).
- **The `regions` composite is unchanged.** In `regions_composite_cells` (`render.cpp:879`) the only addition is a write of `out.unbordered[i]` from `out.colour[i]`, placed before the existing blackening; `unbordered` is read nowhere but `TerritoryBase::build` (`territory.cpp:270`). `colour`, `border`, `east`, `south` and `drawn` are assigned exactly as before, and `render_composite_regions` reads `cells.colour` only, so its pixels are provably the same bytes. U is the pre-border `colour`, which for a kind-2 cell (v > 0, e >= 0, not drawn) is `shade_colour(biome colour, factor)`, the `biomes` composite pixel of 8.2 rule 3.
- **Rules of 8.2.** `territory_pixel` (`territory.cpp:122` to `150`): kind 0 is B; kind 1 is the shaded fill or B; kind 2 follows rules 1 to 4 in order (empty holders B; any differing side B; all free `half`; held with fill the dimmed fill; held without fill B). Matches the r4 text and the spec's table; an empty holder vector still reproduces the `regions` composite.
- **Timeline form and error order** (`territory.cpp:298` to `312`). Both overloads call `territory_fill` first (year check in `region_vector_size`, then the missing-colour check), then `territory_holders` (its year check cannot fire once the first passed), then the two-vector form, where `territory_rgb_size` throws for the scale before the `out` size test. This is the order of 9.2: year, missing colour, scale, size. No hidden state; the result is by construction the two-step path.
- **Iteration order.** No new loops over containers other than index loops; `territory_fill` and `territory_holders` iterate `timeline.region_ids()` (ordered, unchanged).
- **CLI** (`tools/worldgen-cli/cli.cpp:1013`): replaces the three-line two-step path by the timeline form with the same arguments; options, texts and exit codes unchanged; errors arrive in the same order as before.
- **Viewer** (`gdext/src/viewer/history_state.cpp`, `draw`): uses the timeline form at scale 1 with the stored year and colours; `holders_` is still computed for the redraw test. Not in the output path.
- **ASCII.** No non-ASCII byte on any added line of the library, CLI or gdext diff.
- **Build flags, tools/ci, tools/lint.** Untouched by the diff.
- **Lint.** `python3 -B tools/lint/determinism_lint.py --self-test`: 19 snippet files, 0 failures. Scan: 176 C++ files, 13 CMake files, 0 findings. No new pattern found that warrants a rule; the lint is unchanged.

Not built by the auditor: integration ran the gcc-release, clang-debug and asan timeline and territory entries green.

## Blocking findings

None.

## Advisories

- **A1. One padding byte in `TerritoryBase::Cell`** (`worldgen/include/worldgen/inspect/territory.hpp:85` to `91`). `Rgb base`, `Rgb half`, `uint8_t kind` give seven bytes before the first `uint16_t`, so one byte is padding and indeterminate in value. Safe today since no code hashes or compares `Cell` bytewise. Fix if it ever is: hash or compare field by field, or add an explicit `std::uint8_t reserved = 0;` after `kind`.
- **A2. The timeline form validates the year twice** (`worldgen/src/inspect/territory.cpp:301` to `302`, `309` to `310`): `territory_fill` and `territory_holders` each call `region_vector_size` and each look up every region holder. Output and error order are unaffected; it costs one extra scan per draw (the viewer's redraw). If it matters, a private helper building both vectors in one pass would keep 9.2's equivalence.
- **A3. `half` is precomputed in `TerritoryBase::build` from `RegionsCells::unbordered`** (`territory.cpp:270`, `render.cpp:879`). Correct, but it ties rule 3's U to the internal ordering of `regions_composite_cells` (copy before blackening). A future change that moves river painting or another step after the border pass would silently change U. A comment at `render.cpp:879` stating that `unbordered` must stay the `biomes` composite pixel, or a test comparing `half` against `render_composite_biomes` halved on a fixture (the property test of spec 7 item 6 partly covers this), would guard it.
