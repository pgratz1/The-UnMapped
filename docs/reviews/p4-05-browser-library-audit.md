# P4-05 browser, Part A (library): determinism audit

Branch `p4/05-a-int` at 6fd28c3, reviewed as
`git diff origin/p5/02-cand...HEAD -- worldgen/include worldgen/src`.
Spec `docs/specs/p4-05-browser.md` sections 4, 11, 12. Generator 0.23.0,
schema 9; no world byte changes.

**Verdict: pass.** No blocking findings; two advisories.

## What was checked

- `src/inspect/history_browser.cpp` (new), `history_browser.hpp`: no floats,
  no locale, no `<cmath>`, no unordered containers. Every list the browser
  returns is in a defined order: `events()` and `civilizations()` in id order
  (built by index from the id-ordered log and table); `select` walks the first
  entity list of the timeline (ascending id, built in log order with
  `append_once`) or the log, and tests the others with `binary_search` on
  ascending lists; `event_detail` keeps participant and value `seq` order,
  causes and effects ascending; `search` walks civilizations, settlements,
  regions by ascending id and region_name rows in primary-key order, one
  match per entity, cut at `limit`. No sort with a partial key anywhere.
- Search folding: `fold` maps `A`-`Z` only and compares bytes for equality,
  so `char` signedness and locale cannot change a match.
- Bounds on a damaged file: `entry()` checks `1 <= id <= size` before every
  index; `select` checks civilization and settlement ids against the timeline
  counts and region ids by `binary_search`; `build` checks event ids
  consecutive, kind in range, and digest sizes against the log and tables;
  `event_detail` checks the role index and takes the shorter of the value
  lists; `check_history` refuses a history other than the one indexed.
- Links (`TextLink`): byte offsets into UTF-8 text, appended in write order,
  so ascending and non-overlapping; `link_from` skips empty spans; sentence
  links are shifted by the segment start. `uint32_t` offsets are explicit
  casts (no `/W4` narrowing warning); lines are far below 4 GiB.
- `render_event` now returns `render_event_linked(...).text`: the appended
  bytes are unchanged (only link bookkeeping added), so every report sentence
  is unchanged.
- `history_report.cpp` refactor: `BlockBuilder` reproduces each former
  `Report::civilization`, `adversaries` and `relations` line segment by
  segment (literals, escaped text, `ref` form `name (tag)` or tag, `e<id>`,
  escaped sentence, escaped note in ` (...)`); `line()` keeps the
  trailing-space rule. `history_report_digest` computes the outside-lines
  count without the chronicle (`with_chronicle_ = false`); the chronicle
  changes no member that later sections read (`chronicle_lines_` is derived,
  phase counts come from `count_phases`), so O is the same, and the budget
  test is `!=` as before. Both levels now render once with the final digest,
  which is what the old code returned. `worldgen/tests/golden/p3-07-report.txt`
  (independent reference) is unchanged on the branch.
- `markers.cpp`: `ruin_glyph` (33 ink, 44 halo) and `faded_glyph` (9 fill,
  36 halo) recounted by hand against 12.1 F3; `halo(of)` stays integer and
  bounded by the grid radius; plague colour (0, 190, 120) per F5;
  `event_place`/`place_of` share the one bounds-checked lookup. The overlay
  hashes move as 12.1 and 11 state; `p4-04-markers.txt` is re-recorded by the
  reference on the branch.
- `digest_tables.cpp`: non-consecutive kind ids now throw `Corrupt` (caught
  in `read_history_tables` and returned as an error) instead of returning
  silently; error path only.
- `event_catalogue.hpp`/`.cpp` and `markers.hpp` header additions: no new
  platform-dependent types in stored or printed values.
- Lint: `--self-test` passed (20 snippet files); scan of 182 C++ and 13 CMake
  files, 0 findings. No new rule warranted: nothing in this change is a
  pattern the existing rules miss.

## Advisories

**A1. Empty name versus no name in a block line.**
`src/inspect/history_report.cpp` `block_line_report_text` (entity case) and
`BlockBuilder::entity` turn a missing name into an empty `Segment::text` and
print the tag alone for an empty text. The former `Report::ref` printed the
tag alone only for a missing name (`nullptr`) and ` (c3)` for an existing
empty name; likewise the race line printed `<name> (ra<id>)` unconditionally.
Generated worlds never have empty civilization, settlement or race names, so
no report byte changes, and the behaviour is the same on every compiler. On
a damaged file with an empty name the report text now differs from 0.23.0's
earlier text. Fix if wanted: carry a `has_name` flag in `Segment` (or refuse
empty names on load, as `is_region_name_text` does for regions).

**A2. `search` relies on `region_names()` being ordered by region.**
`src/inspect/history_browser.cpp:397` uses `std::lower_bound` on the
region_name rows by region. They are read in primary-key order, so this holds
for any file SQLite opens; the P5-12 audit's A1 (no check that the rows
ascend) is the related open item. Viewer-only output, not in the
cross-platform contract.
