# P5-04a tests: notes for integration

Spec: `docs/specs/p5-04a-relations-alliances-followups.md` r2 (generator 0.28.0). Written by the test writer from the spec alone, on `p5/04a-tests` (base ef338f8, generator 0.27.0), without reading the implementation branch.

## Switch

`worldgen/tests/history/p5_04a_api.hpp`: define `UNMAPPED_P504A_API` (the commented line, or `-DUNMAPPED_P504A_API` in `worldgen/tests/CMakeLists.txt`). Until then every case that needs the 0.28.0 engine starts with `P504A_REQUIRE_INTEGRATED()` and returns with a message. The spec fixes every name the tests use, so the header guesses no library call. Only behaviour waits for the engine.

## Files

| File | Covers | Waits for the engine |
| --- | --- | --- |
| `worldgen/tests/history/p5_04a_api.hpp` | The switch, the spec's constants (98 streams, header v9, `SUMMARY_MILESTONE_SALIENCE` 40), `tuning_or` | — |
| `worldgen/tests/history/p5_04a_checks.hpp` | The formulas of 3.3, 4.2, 5, 6.1, 6.3, 7.1, 7.2, 8, 8.5, 21.4 and 21.5 (`p504a_f`), and the row invariants of 16 items 13 and 19 (`p504a_check`: `alliance_violations`, `relation_violations`, `figure_violations`, `value_coverage`), ported from the reference | — |
| `history/history_alliances_p504a_unit_test.cpp` (shard alliances) | 16 items 2 to 8 and 10 on hand-checked values: large shares at the bound ± 1, each factor alone with its motive, ties, motive 0, the floored chance, the excused codes, the betrayal motives at their bounds, the renewal chain and count, each turn's change, the cool-down at 299 and 300, expulsions at 799 and 800 with the turn inside and outside 150 years, tower spacing, hero quiet at 99 and 100, the 2-to-8-word choice | Runs now. Two cases wait: the catalogue's ten value rows and `template_problem` for 8 and 9 words and `{m:died}` |
| `history/history_alliances_p504a_data_test.cpp` (alliances) | 16 item 1 on `game/data`: every new field in order after the field it follows, at its shipped value; refused at low − 1 and high + 1 and when absent; the changed values (14.1, 14.2, 14.2a, L-7, L-8) | All |
| `history/history_alliances_p504a_property_test.cpp` (alliances, slow `alliances-slow`) | P5-04 13 (item 12 filtered out because it is replaced) plus `alliance_violations` on seeds 0–9 at 512. The slow suite runs seeds 0–19 at 1024 and civs4 0–4, with civs4 seed 6 as the witness for 11 item 1. Also alliances-small at 0.28.0 (P5-04 18.3 plus 14.1's edits), checking coverage of motives, grievance 2 and spurning reason 2, and determinism at 1 and 4 threads | All |
| `history/history_relations_p504a_test.cpp` (relations, slow `relations-slow`) | `relation_violations` (7.1 change, the 7.2 cool-down as rows show it, rule 8 on every expulsion) on seeds 0–9 at 512 and 0–19 at 1024. relations-small with `turn_cooldown_years` 0 must show changes 0 to 3 | All |
| `history/history_figures_p504a_test.cpp` (figures, slow `figures-slow`) | `figure_violations` (towers apart by D, ended towers by 21.4's table, books, `hero_rose` quiet) on seeds 0–9 and 0–19. figures-small must have at least one ended tower | All |
| `history/history_report_v9_test.cpp` (history-report) | 16 items 10 to 12 and 14.4 on fixture H18: header v9; 13 changed sentences; the `[alliances]` and trade lines; `allies:` at summary; the two same-day group lines; milestones at summary (count ≥ 5, three lines, no quiet stretch); the 19 new `[stats]` keys and their order. Also the reader checks of 14.4 on SQL-corrupted copies, in both readers with the same message | All (a 0.27.0 reader refuses `reason` 2) |
| `worldgen/tests/data/history-report/h18-v9.toml` | Fixture H18, written by `reference/p5_04a_h18_fixture.py`. The README lists it | — |
| `worldgen/tests/browser_support.hpp`, `golden/p4-05-browser.txt` | 21.7 item 3: the C++ dump no longer drops figures (type 6). Three figure queries per case (reading R9). `search` lines re-recorded from `p4_browser_reference.py golden` on the 0.27.0 inputs | Runs now |
| `reference/p3_engine_reference.py` | Version 0.28.0 (`RENDER_R28`): `COMPILED_028` with the ten value rows, `choice_word` (2 to 8 words, `choice of <n> words at byte <b>`), `{m:died}` | — |
| `reference/p3_data_reference.py` | 0.28.0: `KINDS_028` with the new fields. Lists the new fields, changed values, code defaults and small-fixture edits (`ALLIANCES_NEW_RANGES_028` and the others). A 0.27.0 root is read at 0.28.0 with the shipped values filled in (`FALLBACK_028_USED`) | — |
| `reference/p3_conflict_reference.py`, `p3_peoples_reference.py` | Version 0.28.0 (flag `p504a`, off at the 0.27.0 default); `has_claim`, `claim_event`, `last_war_end`, `arms_cause` (9.2) in `fight`; the battle-cause invariant allows one `arms_sold` | — |
| `reference/p5_04_alliances_reference.py` | Replay of 3 to 6, 9.2, 10 items 2 and 4 in `Alliances` (behind `r28`). Formulas plus `check_units_028`. Also `invariant_violations_028`, `reader_problems_028`, `stats_028`, the v9 lines, `fixtures-028`, and `READINGS_028` (R1 to R10). `check-world` runs the 0.28.0 checks on a world whose catalogue has `grievance` | — |
| `reference/p5_03_relations_reference.py` | Replay of 7.1 (change), 7.2 (cool-down), 8 (long-held expulsions) and 11 item 6 (A2); `violations_028`; invariant 4 fixed (below) | — |
| `reference/p5_06_figures_reference.py` | Replay of `temper`, tower spacing (`height_cell`), one library per settlement ever, tower endings, books and blights stopped after a tower's end, `hero_rose` quiet. Also `violations_028`, `reader_problems_028` and `check_units_028` (`the Old` at 1479 and 1480 with two earlier bearers, D, quiet) | — |

Self-tests: `p5_04_alliances_reference.py units` gives 154 checks with 3 failures. The 3 are the grant-chance units of P5-04 ("g sample of 6.11", "g at max", "g base alone"), which also fail at HEAD: they are stale against the shipped `spoils_grant_permille`. All 43 new checks pass. `p5_06_figures_reference.py units` gives 131 checks, 0 failures. `p5_03_relations_reference.py units` gives 186, 0 failures. `p4_browser_reference.py check --quick` gives 1 failure ("T3 c3 milestone e13"), also present at HEAD. `p5_04_alliances_reference.py fixtures-028` runs the 0.28.0 replay on alliances-small seeds 3, 5, 6, 7, 10 and 0 with no invariant or reader failure. It shows refusal motives 0 and 2 to 6, betrayal motives 0 to 3, renewal events cut from 145 to 6 on seed 3, and no withheld share: owed shares are rare on the small stress maps (1 in 12 runs at 0.27.0 too). The spoils rules are therefore checked on H18 and, at integration, on generated worlds. Kinds not seen are reported, not failed.

## Waiting for the engine (to switch on at integration)

Every case of the data, property, relations, figures and report v9 files, plus two cases of the unit file. Golden values recorded at integration from the reference chain, not from C++:

- `p5-04-alliances.txt`, `p5-03-relations.txt`, `p5-06-figures.txt` and the world goldens.
- `p3-07-report.txt`, which also needs H18 added to `fixture_names()` and `FIXTURE_NAMES_V9`.
- `p4-05-browser.txt`, re-recorded once more at 0.28.0.

Not done by the reference (for the integration agent and the lead):

- **Report v9 in `p3_report_reference.py`** (12.2 milestones, 12.4 groups, 21.5 summaries, `reign <n>`). It still serves v8. H18's expected lines come from the alliance functions of `p5_04_alliances_reference.py` and the engine reference's renderer. The milestone lines are only checked for shape.
- **I0 and resume at 0.28.0.** The commands `i0-world` and `fixtures` still pin 0.26.0. `fixtures-028` checks invariants and coverage only.
- **The tower rule's unit cases** of 16 item 16 (a free cell at D and D + 1, no free cell, each ending order). These are in the replay (`height_cell`, `tower_endings`) but have no hand fixture. `tower_spacing_permille` 0 reproducing 0.27.0 is not compared.

## Invariant 4 of `p5_03_relations_reference.py` (21.7 item 2)

Reproduced on civs4 seeds 10, 13 and 22 at 1024 (0.27.0). The interim reading of 21.7 item 2 does not match what happened. In seed 10, colony 145 (Sleez-Snik, founded 4733 by c6 from origin 137) is in region 18, which c6 holds from 4254 to 5033. Its owner never changed. In the same year, after the founding, c2 sacked the origin 137. The survivors, with the origin's mix (race 1 at 79 per mille), moved into the new colony through `PeopleLedger::moved` (P5-03 6.4.2: the mover's copied entries). No event names the colony, so its first rows are 66/934. In seed 22, colony 165 shows the same: its origin s70 was sacked in the founding year.

Only 1 of the 34 colonies from mixed origins in seed 10 has mixed first rows. So this is a same-year move, not a rule that colonies copy their origin. The engine follows 6.4.2, and the check was stale. It now accepts first rows whose other races all come from a settlement of the colony's owner ruined in the founding year. All three worlds pass. There is no generator defect.

## Readings taken where the spec is ambiguous

The R numbers are those of `READINGS_028` in the reference.

1. **R1 (3.4):** the grievance is decided before `spoils_withheld` is written, since it is one of its values. A grievance-2 spurning that waits for a quiet budget is made at the start of the next T2 or Q4 keeping.
2. **R2 (4.2 f1):** `last_war_end` = (0, 0) is not weariness, even though 0 ≥ Y − `weary_years` early in history.
3. **R3 (7.2):** a pair that never turned has no cool-down.
4. **R4 (8):** a pair that met in hatred and never turned into it has no `hatred_event`, so a people held 800 years is never expelled.
5. **R5:** `last_war_end` reads conflict's `last_peace`, which `search_abandoned` also sets (P5-04 reading A3), although the spec lists four kinds.
6. **R6 (6.2):** a notable renewal without budget renews quietly. Its notable events stay notable for the next due year.
7. **R7 (3.5):** the soured renewal draw waits for the wronged ally's budget in Q4, and is keyed by its due year, so it is drawn once.
8. **R8 (4.4):** `{count?|, and its ally did not forget it|}` prints the clause for count 1 only.
9. **R9 (21.7 item 3):** "the first ruler" is the smallest-id figure of role 1; "a regnal display name" is the smallest-id figure with `regnal` ≥ 2; figures match by display name in ascending id after the regions.
10. **R10:** the fixture is H18, because H17 is P5-06's.
11. **6.3:** "a closing in the very step of a due year counts it" conflicts with k = #{s + n·term ≤ end − 1} when the row closes in the due year itself (c1-c5 in H18, closed in 1250, its due year, counts 0). The formula is used.
12. **14.4:** the messages are taken to have the reader's prefix: `corrupt table 'event': row <e>: refusal over spoils without its share` (and the same for the other two). `term` has no upper bound, so the test checks only the prefix `outside 1 to `. The order of the three messages against the range checks for one event is not given; each corrupt case is built to fail only one.
13. **16 item 13:** "every turn within `turn_cooldown_years` has a contribution of its direction" cannot be read from rows, because a credit may cite an older event. The row check is that such a turn does not rest on its direction's fallback cause alone. The full rule is in the replay.
14. **16 item 13:** "every `ally_refused` motive matches the replayed factors" is checked from rows for f1, f2 and f7, plus the cited claim for motive 6. f3, f4 (forces, warmth) and f5 (an unrecorded reign's temper) are in the replay only.
15. **14.1:** "both fixture roots" is read as the in-memory alliances-small, relations-small and zero tunings of the tests and the reference. The on-disk roots `worldgen/tests/data/geo` and `geo-small` (their `history/*.toml`) also need the new fields and the 0.28.0 templates. That is the engineer's part D.
16. **21.4:** "one library per settlement ever" excludes an ended library at the city, which 0.27.0 did not. In the 13 item 16 invariant, a tower may take another realm's ended tower's cell.

17. **12.4 at full:** a chronicle line at full ends with its event's participants, values and causes, and the spec does not say what a group line carries there. The test checks the group line's prefix up to the sentence. At summary, P5-03a 9.5 makes only the first `settlement_mixed` of each race pair a candidate. So a group of one pair, like both of H18's, shows at full only.

## Existing tests whose expected values change with 0.28.0 (do not change before integration)

- **Report header v8 → v9:** `history_report_v2/v3/v4_test.cpp`, `history_report_blocks_test.cpp`, `history_report_checks.hpp`, `history/history_alliances_h16_test.cpp`, `history/history_figures_h17_test.cpp`, `history/history_figures_views_test.cpp`, `history/history_renaming_readers_test.cpp`, `history/p3_07_engineer_test.cpp`, `history/history_relations_report_test.cpp`.
- **Milestones at summary (12.2):** `history/history_relations_report_test.cpp` (`milestones: 8` with a quiet-stretch line), `history_report_blocks_test.cpp`, `history_report_v2_test.cpp`, `history_report_r17_test.cpp`, the H15 and H9 expectations.
- **P5-04:**
  - `history_alliances_h16_test.cpp`: the `against a common enemy against` line; `allies:` summary lines without names; renewed counts by event rather than the chain; spoils by year; the stats lists; the `[alliances]` lines.
  - `history_alliances_catalogue_test.cpp`: the value lists of kinds 63, 67, 69, 75 and 76.
  - `history_alliances_data_test.cpp` and `p5_04_alliances_data.hpp`: 55 fields with `mine_forms` last; `withheld_to_break` 1; `withheld_grudge` 400.
  - `p5_04_alliances_checks.hpp` item 12: replaced at 0.28.0; the new property test filters it.
  - `history_alliances_property_test.cpp`, and the golden.
- **P5-03 and P5-03a:** catalogue tests of kind 58's values; `history_relations_report_test.cpp`; the golden `p5-03-relations.txt`.
- **P5-02:** `history_monsters_render_test.cpp` and `history_monsters_p503a_test.cpp` (`monster_died` for a dead kind now `crumbled to dust at last`); `p5_03a_relations_data.hpp` and `p5_06_figures_data.hpp` listings of the template.
- **P5-06:**
  - `history_figures_reader_test.cpp`: `ending N is not 0 for a tower` becomes `is not 0, 4 or 5`.
  - `history_figures_unit_test.cpp` and `p5_06_figures_data.hpp`: `skill_min` 500 and `skill_max` 800 become 600 and 900; `the Old` 1300/1450 becomes 1400/1480; grasping allied strain 2000 becomes 3000; 38 fields become 40.
  - `history_figures_h17_test.cpp`: `[figures]` and `[monsters]` summaries; `reign <n>` at summary (L-3); `hero_rose` sentences.
  - `p5_06_figures_api.hpp`: `STREAMS_SPEC` 97 becomes 98; `GENERATOR_SPEC` "0.27.0".
- **P3-04 renderer:** tests that a choice of 5 to 8 words is refused as an unknown placeholder now get no problem; above 8 they get the new message.
- **P3-06:** a C++ check that a battle cites exactly its declaration (the reference's is amended to allow one `arms_sold` at 0.28.0).
- **Version 0.27.0:** `data_history_golden_test.cpp`, `db_writer_reader_test.cpp`, `p0_04_reference.hpp`, `history_alliances_data_test.cpp`, `p5_06_figures_api.hpp`.
- **References:** the 0.27.0 default of `use_version` in `p3_conflict_reference`, `p3_engine_reference`, `p3_data_reference`, `p3_report_reference`, `p5_02`, `p5_03`, `p5_12`, `p5_04` and `p5_06` moves to 0.28.0 when the goldens are re-recorded.
- **Goldens:** every world and history golden (16), and `p4-05-browser.txt` once more.
