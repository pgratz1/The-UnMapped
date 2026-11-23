# P5-02 Beasts and monsters: determinism audit

Auditor: determinism-auditor (Opus), 2026-11-23.
Scope: `git diff origin/main...HEAD -- worldgen/include worldgen/src tools/worldgen-cli tools/ci tools/lint game/data`, at `origin/p5/02-int` cb04d8e (the implementation merged with the tests and with main). Tests and references were read as context only. Spec: `docs/specs/p5-02-beasts-monsters.md` (r1). Generator 0.23.0, schema 9, history report format v4.

**Verdict: pass with advisories.** I found no blocking finding. There are two advisories (A1, A2). I added one lint rule, `history-member-held-reference`.

The lead has already ruled on one point, so it is not reported here: `holds_settlement` is set only by an overrun.

## Lint

- `python3 -B tools/lint/determinism_lint.py --self-test`: passed, 20 snippet files (19 before this audit), 0 failures.
- `python3 -B tools/lint/determinism_lint.py`: 180 C++ files and 13 CMake files scanned, 0 findings, both before and after the new rule.
- The diff adds no `determinism-lint: allow` suppression.
- **New rule `history-member-held-reference`** (`tools/lint/determinism_lint.py`, `tools/lint/README.md`, self-test `tools/lint/selftest/bad_history_member_held.cpp`).
  - `MonstersSubsystem` keeps its monsters and sites in its own vectors, `monsters_` and `sites_`, and hands out `Monster&` and `Site&` by id. An append to one of these vectors can reallocate it, which is the same hazard as a founding for `State`'s vectors. The existing rule `history-held-reference` only knows `Civilization` and `Settlement`.
  - The new rule reports a local `Monster&`, `Site&`, `Monster*` or `Site*` (or an `auto&` / `auto*` from `monster(` or `site(`) that is used after a call in the same block that grows the vector it points into. That call is either a direct append to the member (`push_back`, `emplace_back`, `insert`, `resize`, ...) or a call of a function of the same file that appends, directly or through another such function.
  - A `Monster&` is not affected by `sites_` growing, and a `Site&` is not affected by `monsters_` growing.
  - The current code passes. `overrun` keeps `const Monster& m` across `sites_.push_back`, which is safe, and it re-fetches with `monster(id)` after `ruin`.

## What was checked

1. **Random streams** (`core/streams.hpp`, `monsters.cpp:44-65`, `peoples.cpp`, `names.cpp`).
   - Sixteen new streams are appended at the end of `all_stream_names` (56 to 72), in the order of spec 3 with r1's `hist.monsters.raid_target` last. All names are unique.
   - Every draw is keyed: `ctx.draw(key, a, b, k)`, with the step's year added by `StepContext::draw`. The keys match the table in spec 25 item 13:
     - bold (r): `:652`;
     - wake (r, kind): `:781`;
     - rise (r, kind): `:750`;
     - sleep (m): `:934`;
     - rewake (m): `:921`;
     - raid (m): `:946`;
     - raid_target (m): `:1005`;
     - raid_toll (m): `:1021`;
     - overrun (m, settlement): `:1073`;
     - hunt and hunt_outcome (m, c): `:1147`, `:1152`;
     - lair_name (site): `:1420`;
     - outpost (c, R): `:1354`;
     - ghost (settlement), in `peoples.cpp:744`.
   - The names use `hist.names.monster` and `hist.names.site` with (culture id, entity id). `in.cultures` holds every culture definition (`chronology.cpp:333`), so the culture id is never 0.
   - Every key is made of ids, never a list position. No key is drawn twice in one step for different decisions. A quiet step and a tick never share a year.
   - Each draw comes after its preconditions. The raid-target draw comes after the raid's chance and before the toll, as 6.4.1 requires.
   - Lint `history-sequential-rng` is clean. No two draws share one expression.
2. **Integer arithmetic.**
   - There is no `float` or `double` and no `<cmath>` call in the diff.
   - Products go through `checked_mul` and `checked_add`, which fail the same way on every platform. The data bounds (`kinds_history.cpp`) keep every product far below 2^63:
     - `prey_weight` (`:958-974`): base ≤ 1000, `might_f` ≤ 10,000 (because `prey_might_max_permille` ≤ 10,000 and `might` ≤ 10^7), so `might × 1000` ≤ 10^10. The hop factor is ≤ 1000, applied at most `range_hops` ≤ 4 times. The capital factor is ≤ 1000. A weight is therefore ≤ 10^4.
     - `weighted` (`draw.cpp:38`) compares with `mul_u64_wide` (128-bit, the project's own), so W × 2^32 cannot overflow. A negative weight is impossible: every factor is ≥ 0 by schema. When the candidates exist but W = 0, nothing is drawn (`:1002`).
     - Habitat sums: cells × weight ≤ map cells × 1000. K0 ≤ H × 10^5 × 10^4. The wake probability is ≤ 10^7 × H / 10 × 10^4 / 1000, and each step divides before the next multiply.
     - `beast_year` ≤ 10^6 × 1000 × 10^6 = 10^15.
     - The P5-02 7.1 growth numerator P × g × G × (C − P) is checked.
     - `chance_scaled` multiplies by 2^32 a value ≤ 10^9 (ppb, clamped) or ≤ 10^8 (ppm, `wake_rate × M / 1000`), which is below 2^63.
   - **Division by zero is impossible:**
     - `active_years` ≥ 1 (`:934`);
     - `prey_might_ref` ≥ 1;
     - `people_floor` ≥ 1, so Pd ≥ 1;
     - `strength + might` ≥ 1;
     - `checked_mul(1000, k)` only runs when b < k, so k ≥ 1;
     - `pick` always gets n ≥ 1, because `lair_forms` has at least 1 entry and the toll order is checked by `monster_toll_ordered`.
   - When there are no beasts, `beasts_bold` never reaches `beasts_[0]`: menace is 0 and `bold_on_permille` ≥ 1.
3. **Iteration order.**
   - Regions, species, kinds, monsters, sites and civilizations are visited in ascending id. The log is read from the cursor in ascending event id. The undead windows are kept in event order.
   - Hop distances come from a breadth-first search over `GeoView` neighbours. The distances do not depend on visiting order.
   - `relations()` is a `std::map`. Raid memory is a vector in ascending civilization, kept by `lower_bound`.
   - There is no unordered container and no pointer key.
   - Every `stable_sort` has a total order:
     - window entries by (dead desc, event);
     - causes by event id;
     - `taken` cells by index;
     - `beast_population` rows by (region, beast, year);
     - the summary monsters by (hoard desc, id).
4. **Row order of the new tables.**
   - `beast` and `monster_kind` are written in canonical definition order. `monster` and `site` are written in creation id order. All four are enforced by `require_next_id` (`writer.cpp`).
   - `beast_population` is sorted and enforced strictly ascending by the writer (`write_beast_population`).
   - No two rows can share a key. A row is written only when the count changed, and counts change only in an update, whose `beast_last_year_` strictly increases.
   - The DDL is byte for byte spec 11.1, appended after `region_name` (`DDL_STATEMENTS` 32). `SCHEMA_VERSION` is 9.
   - The readers append ORDER BY the primary key. `settlement.ending` is the last column and is bound at ?15.
5. **P3-04 6.9.**
   - The subsystem calls no founding and opens or closes no relation. It writes `State` only through `set_population`, `note_reduction` and `ruin`.
   - Every loop over settlements, civilizations, monsters or sites runs over ids up to a count taken before the loop: `monsters_year` uses n before the loop, `outposts_left` uses n, and the settlement scans use `settlement_count`.
   - References into `State` are not used after `ctx.append` or `ruin`. In `raid` (`:1016`) and `overrun` (`:1053`), `t`'s fields are copied before the append. After `ruin` the monster is re-fetched (`:1110`). `beasts_bold` reads `s` only before the append.
   - The subsystem's own `Monster&` and `Site&` are re-fetched after every append that can grow their vector. Only `create` grows `monsters_`, and it runs after `monsters_year`'s loop. `hunts` walks a copy of the raid memory.
   - `tension()` is pure. `pending_flare_` is set and cleared in Q4 only.
   - Lints `history-live-iteration`, `history-held-reference` and the new `history-member-held-reference` are clean.
6. **Quiet-step budget (W9).**
   - A wild appearance in Q4 checks `quiet_events_left(0)` once, before its scan (`:761`).
   - Each death of age checks the budget before its event (`:1196`).
   - Each outpost end checks `quiet_events_left(founder)` (`:1291`).
   - Nothing that waits makes a draw.
7. **Engine, peoples and conflict seams.**
   - `phase_trigger_` is set in `active_phase`, after `awakening()` fills the opening row, and in `quiet_row` (`engine.cpp:497-504, 595`).
   - The forced flare declaration depends only on the stored kind key of the trigger (`conflict.cpp:1336-1338`).
   - `grow_population` keeps 9.1's exact expression for G = 1000 (`peoples_geo.cpp:466`), so I0 holds by construction.
   - The engine's `settlement_endings` (`engine.cpp:272-325`) and the reader's `settlement_ending_code` (`event_catalogue.cpp:1311`) apply the same rule. A missing ruin event gives 5 on both paths.
8. **Reads of damaged files** (`monster_tables_problem`, `event_catalogue.cpp:1333`, shared by `HistoryView::load` and `read_history_tables`).
   - Every index is checked before use. `kind_key` returns empty when out of range, and `year_of` only runs after `kind_key` has matched (short-circuit).
   - `origin_of`, `faded_ruin` and `causes` are indexed only after a range check.
   - `lair_site` is bounds-checked before `t.sites[...]`.
   - `is_plural`'s `s[i + 1]` is safe because a trailing space is refused first.
   - The per-event participant and cause ranges cannot run outside the vectors on an ungrouped file.
   - Timeline (`timeline.cpp:241-316, 456-517`):
     - site origins are checked non-decreasing before `upper_bound`;
     - the beast rows are grouped by slot with a fatal error on leftovers, and their year order is guaranteed by 11.4's "not above the previous row";
     - the per-monster transitions follow event order, and event years are checked non-decreasing in the same `build`.
   - `history_reads_disagree` also compares monsters, sites and beasts.
9. **Report text** (`history_report.cpp`, `digest.cpp`).
   - Integers are formatted through `num`/`format_i64`, never a locale or float. Names pass through `escape_history_text`. All literals are ASCII (lint `non-ascii-literal` is clean).
   - The `killed` sums saturate (spec 25 item 17).
   - 12.9's budget uses `checked_*` on small counts.
   - The summary is rendered with `SUMMARY_BUDGET` to count O, and the set is chosen again with B. Both renderings are pure functions of the file.
   - `report --help` names v4.
10. **MSVC portability.**
    - No `long`, no `size_t` stored in a row (`Monster::kind` is an index; the stored value is `kinds_[k].id`).
    - Signed/unsigned conversions are explicit casts.
    - Cell indices are `y × N + x` as `uint32_t`, with N ≤ 65,536 by the meta check.
    - The `std::pair` CTAD in an initializer-list range-for and `std::min({…})` (with `<initializer_list>`) are standard C++20.
    - The project's `-Wconversion`/`/W4` gate covers the rest.
11. **P5-12 advisory A1** (`docs/reviews/p5-12-renamed-regions-audit.md`): **closed.**
    - `region_name_rows_problem` (`event_catalogue.cpp:1219-1227`) refuses, as its first reason, a row whose (region, from_year) is not strictly above the previous row's: "not above the previous row".
    - This function serves both `HistoryView::load` and `read_history_tables`. Every file that is accepted therefore gives `Timeline::region_name`'s binary search a partitioned range.
12. **P5-12 advisory A2**: **closed.**
    - The check that every `region_renamed` event has a row is now linear (`:1248-1265`). One pass marks `has[event]` in a `std::vector<std::uint8_t>`, where every row's event is already range-checked by the row loop. A second pass tests each kind-43 event with a bounds-checked read.
    - The 11.4 site check uses the same form (`origin_of`).
13. **Hard rule 7.**
    - `SCHEMA_VERSION` goes from 8 to 9, and `GENERATOR_VERSION` from 0.22.0 to 0.23.0.
    - The spec section is 11.
    - I did not re-verify the `docs/specs/schema.md` and proposals entries, which spec 24 lists. They are the validation gate's.
14. **Build flags.** No CMake file is in the diff, and the lint's flag scan is clean.

I did not build. The cross-compiler comparison is the validation specialist's.

## Blocking findings

None.

## Advisories

**A1. Two report rules are in the code but not in spec r1**

Where:
- `worldgen/include/worldgen/history/digest.hpp:49`: `SUMMARY_LINES_CAP` = 1390 caps L in `summary_budget` (`digest.cpp:1796`);
- `worldgen/src/inspect/history_report.cpp:600-601`: with more than `SUMMARY_LINES_CIVS` (16) civilizations, the summary's `[monsters]` block prints only its count line.

Neither rule is a determinism risk: both are pure integer functions of the file. They came in with commit ba90687 and are mirrored in the reference (`p3_report_reference.py`). However, the report text is part of the cross-platform contract (`world-hashes.sh` compares it), and spec r1 says otherwise:
- 12.9 gives L with no cap;
- 12.4 gives up to `SUMMARY_MONSTER_LINES` living monsters at every summary;
- 23 item 4 drops monster lines only if the `civs4` median passes 1,100.

Suggested fix: record both rules in the next spec revision (12.4, 12.9, and the constants of P3-07 24.9).

**A2. `read_history_tables` skips the schema-9 checks when the stored `event_kind` ids are not consecutive**

Where: `worldgen/src/history/digest_tables.cpp:594-597`.

`monster_tables()` returns without checking when the stored catalogue's ids are not 1, 2, …. It relies on `HistoryView::load` to refuse the file ("id not consecutive").

This is currently safe. Every consumer (`build_history_report` `history_report.cpp:914-918`, `load_history` `timeline.cpp:695-699`) reads both and fails when either fails.

The risk: a future consumer that calls `read_history_tables` alone would accept unchecked `monster` and `site` rows. The report then indexes those rows without a further check, at `t_.sites[m->lair_site - 1]` (`history_report.cpp`, `monsters()`), which is out of bounds on a damaged file.

Suggested fix: throw `Corrupt{"corrupt table 'event_kind': ... id not consecutive"}` there, as the other `Checks` methods do (`digest_tables.cpp:317`, `:355`), instead of returning.

## Verdict

**Pass with advisories** (A1, A2). No blocking findings. One lint rule added (`history-member-held-reference`).
