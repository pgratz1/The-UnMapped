# P5-11 tests: notes for integration

Spec: `docs/specs/p5-11-artifacts.md` r1 (generator 0.31.0, schema 15, report v12) with the lead's rulings L-1 to L-11 as recommended. Written by the test writer from the spec alone on `p5/11-tests` (base 5ef9bfa), without reading `p5/11-impl` or `worldgen/src/history/artifacts.cpp`.

## Spec findings (the reference picked a reading; `READINGS` of the reference)

- **S1 (9, 8.1; A-R1)** `artifact.occasion_event` is "that event's first cause", but a deep artifact also cites its forge site's last `delving_deepened`, which is older than the occasion whenever the mine was deepened first (the usual case): the first cause is then the delving. The reference stores the occasion e and checks "a cause, not a `delving_deepened`". 9 should say "the cause that is not a `delving_deepened`" (or list the occasion first).
- **S2 (6.5; A-R2)** The staff row has no "bears none of the class" test, so a caster who makes two staffs would bear two, against 6.2 and invariant 4. Reading: a second staff is kept by c.
- **S3 (6.3, 6.7 M2, 6.9; A-R5)** "visible" read as P3-04 6.2 visibility (as P5-08's `alive_visible`); M2's "hidden" as its negation. 6.9's "its seat when the realm is hidden" suggests a realm lying low, which the spec does not define here.
- **S4 (6.4; A-R19)** `forge(c)` tests `forge_depth(c) >= 1` "(an open mine)", but P5-07 9 gives a mine never deepened the depth 0, so an open, undeepened mine is no forge (and its realm may commission a partner's forge). The reference follows the formula. If "an open mine" is meant, 6.4 must test "has an open mine" and the deep test is unchanged.
- **S5 (13 item 8, 12.2)** Inside one step 6.8 spreads before 6.9 suppresses, so a spread and a suppression of the same cult and realm in the same year are legal when the spread comes first; the invariant must compare event order (the reference and C++ checks do).
- **S6 (13 item 3)** "maker_kind 2 only when the home realm had no open mine" cannot be decided exactly from rows: a mine lost with its town is closed by the alliances' next step with the change's event as `ending_event`, so rows may show it closed before a making that still saw it open. The row checks use "definitely open" for the home realm and "open or closed in the making's year" for the maker realm and the deep test; the replay is exact.
- **S7 (6.2, 6.8; A-R4)** A holding's `from_year` is the year of its opening event (for a quiet move, the trigger's year), so that 9's "event of year `from_year`" holds in quiet steps.
- **S8 (10 item 6; A-R14)** `cult_returns_barred` has no event, so a report of a stored world cannot print it (P5-08's F3 again); the C++ stats leave it out.
- **S9 (10 items 2, 3; A-R12, A-R13)** `held:` entries read as `<from> <state words> <tag><id> <name> e<id>`; "holds ... at its end" as the holdings opened before the ending event and closed at or after it. The summary rule for realm lines ("P5-08 10 item 4's rule") is not restated; H21's expected lines check realm lines at full only.
- **S10 (6.2; A-R3)** A holding lost in a lair has settlement 0 (6.2's "settlement (type 4) or 0"), an enshrined one the site's settlement.
- **S11 (8.1; A-R6, A-R9)** Kind 111's role `site` taken as the maker site; kind 115 names a figure only for `how` 1 by a hero.
- **S12 (6.7 M12; A-R8)** A standing seat owned by c itself or by an ended realm is treated as t = 0.
- **S13 (7.2; A-R10, A-R18)** The declaration's claim cause is added wherever a `war_declared` is written; a holding opened by a quiet move makes the declaration cite the triggering sack or conquest.
- **S14 (11.1; A-R16)** `artifact_at` for a hoard: the lair's region, site and cell; settlement 0.

## What exists

| File | Covers | Waits |
| --- | --- | --- |
| `reference/p5_11_artifacts_reference.py` (new) | `Artifacts` (5, 6.1 to 6.9, 7.1), the formulas, `violations` (13 items 1 to 8 on rows), `reader_problems` (9), `stats_of_tables` (10 item 6), the v12 lines (`artifacts_block`, `realm_line`, `figure_fragments`, `monster_fragments`), the 11.1 reads; `units` 161 checks, `fixtures` 62+ checks (every required path, I0 on stress 0 to 2), `check-world`, `golden`, `readings` | — |
| `reference/p5_11_h21_fixture.py` (new) | `data/history-report/h21-v12.toml` and `h21-v12.expected.txt` (274 lines) | — |
| chain edits (conflict flags `artifacts`, `p511`, on from 0.31.0; `VERSIONS` + 0.31.0 in data, engine, conflict, peoples) | `p3_data_reference` (`KINDS_031`, `history_031`, `culture_031`, `god_031`, `artifact_messages`, `artifact_form_ok`); `p3_engine_reference` (`COMPILED_031`, `RENDER_R31`, form `kind` of type 10, empty `titled`, schema 15); `p3_names_reference` (`hist.names.artifact`); `p3_peoples_reference` (catalogue of 116); `p3_conflict_reference` (install last at T3/T4/T6/Q4, win factor, strain factor, declaration and `battle_led` causes, 12.1, tables); `p5_02` (hunts); `p5_04` (`forge_site`); `p5_06` (role 8, `raise_maker`, `brief`, `caster_site`, smith's place, no epithet); `p5_08` (`force_act`, the return bar, role 8 in its reader check) | — |
| `data/artifacts/spec-r1/` | the spec's listing of 4.1 to 4.5 and 8.2 as TOML (15 kind files, `history/artifacts.toml`, `additions.toml`), read by the 0.31.0 loader only where the root lacks the item (`FALLBACK_031_USED`) | removed or ignored once `game/data` has them |
| `data/artifacts/artifacts-deep.toml` | artifacts-small with certain deepening: deep iron and commissions | — |
| `history/p5_11_api.hpp` | switch `UNMAPPED_P511_API` (off), constants, `artifacts_small()`, `zero()`, the 11.1 names | — |
| `history/p5_11_checks.hpp`, `p5_11_property_common.hpp` | `p511_f` formulas; `p511_check` rows, `violations`, `stats`, `paths_of`, `artifact_at`, `artifacts_held`, `artifacts_placed` | — |
| `history/history_artifacts_unit_test.cpp` (shard `artifacts`) | 16 items 2, 3, 7, 8 by hand | runs now |
| `history/history_artifacts_data_test.cpp` (`artifacts`) | 16 item 1 | all |
| `history/history_artifacts_property_a_test.cpp` (`artifacts-property-a`) | artifacts-small 0 to 9 at 512: invariants, required paths; 11.1 reads against rows (seeds 0 to 2) | all |
| `history/history_artifacts_property_b_test.cpp` (`artifacts-property-b`; slow `artifacts-slow`) | shipped 0 to 9 at 512, determinism (twice, 1 and 4 threads), I0, resume; slow: shipped 0 to 19 at 1024, turbulent 0 to 4 | all |
| `history/history_artifacts_reader_test.cpp` (`artifacts`) | 16 item 9: every table of 9 in both readers (prefixes), schema 14 refused | all |
| `history/history_artifacts_report_test.cpp` (`artifacts`) | 16 items 10, 12 on H21 | all |
| `history/history_artifacts_golden_test.cpp` (`artifacts`, slow) | world lines; the fixture and stress `path` lines (runs now) | world lines |
| `golden/p5-11-artifacts.txt` | 14 fixture cases (artifacts-small stress 0 to 14, artifacts-deep 0, 6, 13) | world lines at integration |

H21 (artifacts-small stress 6, span 2000) shows makers 1, 2 and 4, M1 to M5, M7, M9 to M11, M13, M14; it lacks a caster's staff, deep iron, a claim cited, M6, M8 and M12 (each appears in other fixture cases: the golden `path` lines). A hand fixture is needed for one report holding all.

## What integration must do

1. Define the switch; fix any signature differences of 11.1 (findings, not test edits).
2. `history_report_fixture.hpp` and `data/history-report/README.md` must load schema 15 fixtures (the three tables after `faction`) and `p3_report_reference.py` render v12; H21 and its expected lines are here.
3. Once `game/data` holds the P5-11 data, the reference reads it (no fallback); re-run `units`, `fixtures`, re-write H21 and the golden fixture lines (they depend on the kinds' weights and the culture forms), then `golden --inputs DIR` (DIR: `artifacts-<seed>-512.db` seeds 0 to 9 with artifacts-small, `geo-seed-42-1024.db`) for the world lines; `check-world` on validation worlds. Compare `p511api::artifacts_small()` with the engineer's in-memory content.
4. Existing tests that change with 0.31.0 (do not change before): schema 14 to 15 and "this build reads 14"; generator 0.30.0; streams 120 to 127, kinds 110 to 116; report header v11 to v12 (H20 and the files that pin the header); `history_pantheon_reader_test.cpp` R1 sets `figure.role = 8` expecting a refusal (8 is the smith now); browser `ALL_GROUPS` 0x3FF to 0x7FF; the empty world's page count (52 or 53); P5-08's and P5-06's role tables (8 smith); battle causes with acts (12.1) in conflict goldens.
5. Pre-existing at the base: `p5_07_depths_reference.py units` has 33 stale failures (as at 5ef9bfa).
