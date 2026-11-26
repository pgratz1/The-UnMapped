# P5-07 tests: notes for integration

Spec: `docs/specs/p5-07-mountain-depths.md` r1 (generator 0.29.0, schema 13, history report format v10), with the lead's rulings on L-1 to L-7 (`docs/design/proposals.md`, last entry). Written by the test writer from the spec alone, on `p5/07-tests` (base 4eef014, generator 0.28.0 plus the spec), without reading the implementation branch.

## Switch

`worldgen/tests/history/p5_07_api.hpp`: define `UNMAPPED_P507_API` (the commented line, or `-DUNMAPPED_P507_API` in `worldgen/tests/CMakeLists.txt`). Until then every case that needs the 0.29.0 engine starts with `P507_REQUIRE_INTEGRATED()` and returns with a message.

The new library names are reached only through `p507api::` functions, and only under the switch: `Timeline::mine_depth_at(site, year)`, `Timeline::sealed_year(monster)` and `inspect::mine_depth_word(depth)` (11.1). The signatures are the spec's, so a compile error at integration means the library differs from the spec. That is a finding to report, not a test to change. The seams of 7.1, 7.3 and 7.5 (`forge_depth`, `deep_flight_plan`, `credit_units`) are not called from C++. Their contracts are checked by the reference's replay, which must equal the engine table for table.

## Files

| File | Covers | Waits for the engine |
| --- | --- | --- |
| `history/p5_07_api.hpp` | The switch, the spec's constants (103 streams, 96 kinds, schema 13, header v10, codes), `tuning_or`, `tuning_array_or`, the fixtures `depths_small()` (4.2) and `zero(forge_depth)` (14 I0), `generated(...)` (with `until` for resume), the three adapters above | — |
| `history/p5_07_checks.hpp` | `p507_f`: the formulas of 6.2 to 6.7 and 6.3, the tellings, the depth words. `p507_check`: `violations` (13 items 1 to 9 from rows), `stats` (10 item 7), `mine_depth_at` (9's derivation). Ported from the reference | — |
| `history/history_depths_unit_test.cpp` (shard `depths`) | 16 items 2 to 7 on hand-checked values: the worked years at `deep_years[d] - 1` and at it; mining 500; Q4's p x L and its cap; the forge at depth 0 to 3 and with `forge_depth_permille` 0; the wake chance by pressure; the weights (0 never chosen, the boundaries); the gate at S = 0, S = might and the cap; u = odds - 1 seals and u = odds falls; the spec's samples (512, 375; the toll 1206/1810 and 214/321; the hunt odds 17% and 11%); the later-take conditions; the tellings | Runs now. One case waits: the library's `mine_depth_word` |
| `history/history_depths_data_test.cpp` (depths) | 16 item 1 on `game/data`: the three kinds after `wyvern` with 4.1's values; `monster_deep_biomes`; the eight `history.monsters` fields in order, their bounds (including the arrays' length and elements) and absence; `monsters_deep_years_ordered`; `forge_depth_permille` after `arms_strength_permille`; rule 8 as the table; ids 1 to 6 unchanged | All |
| `history/history_depths_property_test.cpp` (depths, slow `depths-slow`) | 16 items 13 and 14. 13's row invariants on depths-small seeds 0 to 9 at 512, with every path of 6 required (deepened, depth 3, woken, sealed, a fall at the waking, a later take; a flight and a realm lost are warnings). Also on the shipped seeds 0 to 9 at 512. Determinism twice and at 1 and 4 threads. I0: no deep event at `deep_delve_permille` 0, and `forge_depth_permille` 0 and 15 give the same tables. Resume Y2 on a depths-small world with a waking. Slow: shipped 0 to 19 at 1024, and `--monster-pressure high` 0 to 4 | All |
| `history/history_depths_reader_test.cpp` (depths) | 16 item 10: each reader check of 9 on a corrupt copy of a depths-small world, in both readers with the same first message: wake 0 and 4; a deep monster born of another event; the value ranges; `depth 2 without depth 1`; `depth 1 again`; `site <s> is not a mine of <c>`; `a second waking of site <s>`; `deep_sealed without its waking` and `hold_lost without its waking`; `hold_lost <e> did not ruin its settlement`. A schema 12 world is refused at open | All |
| `history/history_depths_report_test.cpp` (depths) | 16 items 11 and 12 on fixture H19. Covers the v10 header; the sentences of 93 to 96 and `civ_ended`; `sealed in`; the mine lines' `, depth 3` and `, sealed 1150 e30`; `lost to`; `; soured e25`; the milestones, one per kind (e6, e20, e41 where v9 prints e20, e34, e41); the `allies:` lines in v9's text; the eight stats in order. Also the timeline's `mine_depth_at` at 559, 560, 899, 900, 1449, 1450; `sealed_year`; the monster state at the waking and sealing years; the groups; the `hold_lost` symbol | One case runs now: the row invariants and stats on H19's rows. It needs the fixture builder only. Before integration it ignores the `ending 2` check, because the 0.28.0 builder derives `ending` without 6.6. All other cases wait |
| `history/history_depths_golden_test.cpp` (depths, slow) | 16 and 17: `golden/p5-07-depths.txt` (the site, monster, event and event_value hashes and the stats of each case) | All, and skipped while the file is absent |
| `data/history-report/h19-v10.toml` | Fixture H19, written by `reference/p5_07_h19_fixture.py` (`--lines` prints the reference's whole v10 report of it) | — |
| `reference/p5_07_depths_reference.py` (new) | The rules of 6.2 to 6.7 (`Deep`, installed by the conflict World at 0.29.0). Also the formulas with 149 unit checks (`units`: formulas, data listings, every template at each telling and depth, the catalogue). `violations` (13), `reader_problems` (9), `stats_of_tables` (10 item 7), `mine_depth_at` and `sealed_year` (11.1). `fixtures`: depths-small on the stress geographies, plus I0. `check-world` (rows), `golden --inputs DIR`, `readings` | — |
| `reference/p5_07_h19_fixture.py` (new) | Writes H19 | — |

Version 0.29.0 in the existing references (flag `deep` and flag `a1_units` of `p3_conflict_reference.py`, both on from 0.29.0):

| Reference | Change |
| --- | --- |
| `p3_data_reference.py` | `KINDS_029` and the listings `DEEP_KINDS_029`, `MONSTERS_NEW_029`, `CONFLICT_NEW_029`, `RENAMING_RULE_029`, `REPORT_ENTRIES_029`, `CIV_ENDED_029`, `CAUSE_WORDS_029` and `MOTIVE5_029`. Also `DEPTHS_SMALL_029`, the two check message functions and `pre_029`. A 0.28.0 root is read at 0.29.0 with these filled in (`FALLBACK_029_USED`). These listings are the one place to edit after calibration |
| `p3_engine_reference.py` | `KINDS_029`, `COMPILED_029`, `RENDER_R29`, schema 13, `WAKE_CODE` (3 deep), and the ending of a `hold_lost` ruin (2) |
| `p3_peoples_reference.py` | 0.29.0 in `VERSIONS` and the catalogue of 96 kinds |
| `p3_conflict_reference.py` | `forge_depth(c)` and the force factor of 6.3; `Deep` installed on the monsters; invariant 3 of 20.13 widened (a `hold_lost` of the same year as a flight's cause, L-7); check-world at schema 13; **12 item 3**: check-world without `--version` uses the world's `generator_version` (`VERSION_EXPLICIT`) |
| `p5_02_monsters_reference.py` | The hooks (`deep` member; T4, Q4, after a raid). `caps_allow` counts the world cap without the deep kinds. In `invariant_violations` (13 item 11): a deep monster's first event is its `deep_woken`; deep lairs are outside the one-lair rule; deep monsters are outside the world cap; `deep_woken` is allowed as a lair's origin on a settlement, as `monster_slept`'s cause and as a raid's waking |
| `p5_03_relations_reference.py`, `p5_04_alliances_reference.py` | `credit_units` (7.5); the quiet renewal calls it under `a1_units`; `; soured e<id>` (10 item 5, `V10`) |
| `p5_06_figures_reference.py` | The deeds `deep_woken` and `deep_sealed` (role civilization) in the replay and in invariant 3 |
| `p3_report_reference.py` | Format v10 (`use_version("0.29.0")`): items 1 to 7 of 10; H19 in `FIXTURE_NAMES_V10` |
| `p4_browser_reference.py`, `p4_markers_reference.py`, `p4_timeline_reference.py` | `--version 0.29.0`: the groups of 11.1; `hold_lost` gets the sack symbol; the monster state with 94 and 95 (the monster is role 2 in `deep_sealed`) |

Self-tests (all run here):

- `p5_07_depths_reference.py units`: 149 checks, 0 failures.
- `p5_07_depths_reference.py fixtures`: depths-small on 11 stress geographies. Every invariant holds, and so do the reader checks and P5-02's invariants with 13 item 11. Seen: deepenings, wakings, sealings, falls, later takes, flights and realms lost. Not seen: a forced seal (a last refuge with no flight plan) and a later take refused for want of a plan, so both are reported only. I0 passes on stress seeds 0 to 2 against the same build with the deep rules removed.
- The existing `units` commands at their defaults: `p5_02` 102 checks, 0 failures; `p5_03` 186, 0; `p5_06` 149, 0; `p5_04` 154 with 3 failures (the stale grant-chance units, also at HEAD). `p3_conflict_reference.py check-world` on a 0.28.0 world (`p504a-int/worlds/seed-6.db`, no `--version`: it selected 0.28.0 from the world) gives 3148 checks and 0 failures.
- `p3_report_reference.py check` stops in `check_fixture_facts_v6` (H15's `milestones: 8`, stale since v9's milestones). It does so at HEAD too, so it is not caused by this change; integration should restate that fact.

## Waiting for the engine (to switch on at integration)

- Every case except the unit file's formula cases and H19's row case.
- The goldens are recorded from the reference chain, not from C++:
  - `golden/p5-07-depths.txt`. Run `p5_07_depths_reference.py golden --inputs DIR`, where DIR holds `depths-<seed>-512.db` for seeds 0 to 9 (any 0.29.0 file of that geography; `--until regions` is enough) and `geo-seed-42-1024.db`.
  - Every world and history golden, re-recorded once (spec 17).
- `p3-07-report.txt`, which also needs `h19-v10` added to `hrtest::fixture_names()`.

Not done by the reference (for the integration agent):

- The **forced seal** and the **refused later take** (6.5, 6.6 item 2) are in the replay but appear in no fixture run. They are hard to provoke on the stress maps: the dwarves must be their race's last realm and have no free land within `flight_hops`. They need a hand fixture or a witness world.
- **10 item 8** (at most 3 chronicle candidates a world, T18) and the targets of 15.1 belong to validation.
- **12 item 4's `tagged` segment** is checked only through the report text (unchanged from v9). The display text and the B4 transform are the viewer engineer's (B14).
- **16 item 8:** these are not row-checkable from C++, because a contribution list is private:
  - `credit_units` keeps no contribution;
  - a quiet renewal cites nothing later;
  - `deep_flight_plan` follows conflict's own order.

  They are checked when the engine's tables equal the reference's (`p3_conflict_reference.py check-world` on 0.29.0 worlds). The reference's I0 run on stress seed 1 shows that 7.5 changes a world, in `event_cause`, as the spec expects ("rarely").

## Readings taken where the spec is ambiguous

The D-R numbers are those of `READINGS` in `p5_07_depths_reference.py`.

1. **D-R1 (16 item 11):** the fixture is H19 (`h19-v10.toml`), because H18 is P5-04a's (`h18-v9.toml`).
2. **D-R2 (6.5):** the samples give a village of 800 at 219 and 138. The formula floors and gives 218 and 137. The tests use the formula.
3. **D-R3 (8.2):** the sample sentences print 1206 and 1810. The renderer's thousands separator (P3-04 20.3) gives 1,206 and 1,810, which the tests expect.
4. **D-R4 (7.3):** "20.1 items 2 and 3" include P5-01 24.4's `refuge_hidden` right after the deep flight's `refugees_settled`. Conflict's flights counter does not count a deep flight.
5. **D-R5 (6.7):** a later take's `deep_reach_hops` uses GeoView hops (every `region_border` row), as the raid range does.
6. **D-R6:** a sealed horror counts as living for its kind's `max_alive`. Its rewake draw is never made.
7. **D-R7 (6.6 item 2):** "its race's last refuge" is P3-05 r12 22.7's predicate on State at the take.
8. **D-R8 (6.2):** in Q4 the budget is tested after the worked years. A mine without budget waits.
9. **D-R9 (4.2):** depths-small applies monsters-small's kind edits (`wake_rate` x 2000, `overrun_permille` 1000) to the deep kinds too.
10. **D-R10 (9):** the reader message `hold_lost <e> did not ruin its settlement` is put on the event's row. `dead` and `survivors` have no upper bound in 8.1, so the C++ test compares only the prefix `value dead -5 outside 0 to `.
11. **D-R11:** a take makes no `note_reduction`, because its settlement is ruined in the same step.
12. **D-R12 (13 item 6):** checked from rows as two things. Every `refugees_settled` caused by a `hold_lost` is of its year, and its founder and origin are the hold's realm and settlement. At E, every race that had a realm still has one alive.
13. **D-R13 (16 item 14):** P3-04 16.3's resume point is the history stage's start (Y2, `--until chronology`). "A world that has a waking before the resume point" is read as a resumed depths-small world whose history holds a waking.
14. **12 item 3:** "reads `GENERATOR_VERSION` 0.29.0" is read as follows. Without `--version`, check-world serves the world's own generator version. The module constant (0.27.0) stays the default for its goldens until integration moves it.
15. **6.4:** "all weights 0: next s" is tested after the wake draw. No kind draw is made then.
16. **8.1, 13 item 3:** "followed in the same step by exactly one `deep_sealed` or `hold_lost`" is checked as the first such event of the year that names the monster. It must be caused by the waking, with no event naming another monster in between. In both the reference and the engine order it is the next event.
17. **6.9 rulers:** the deed list names a ruler at `deep_woken` and `deep_sealed`. `hold_lost` is not among the realm's own roles (P3-07's `OWN_ROLES`), so it is not one of the realm's milestones. H19 shows this.

## Existing tests whose expected values change with 0.29.0 (do not change before integration)

- **Report header v9 → v10:**
  - `history_report_checks.hpp`, `history_report_blocks_test.cpp`, `history_report_v2_test.cpp`, `history_report_v3_test.cpp`, `history_report_v4_test.cpp`;
  - `history/history_alliances_h16_test.cpp`, `history/history_figures_h17_test.cpp`, `history/history_figures_views_test.cpp`, `history/history_relations_report_test.cpp`, `history/history_renaming_readers_test.cpp`, `history/p3_07_engineer_test.cpp`, `history/p5_04a_api.hpp` (`REPORT_HEADER_SPEC`);
  - `tools/worldgen-cli/tests/cli_history_report_test.cpp` (`report --help`).
- **Milestones at summary (10 item 6):** every fixture whose top three repeat a kind. H18 (`history_report_v9_test.cpp`) and the H9 and H15 expectations need re-checking.
- **`; soured e<id>` (10 item 5):** the H18 alliance lines of `history_report_v9_test.cpp` (c3-c5 and c1-c5 withheld with grievance 1), and `history_alliances_h16_test.cpp` if H16 has a grievance-1 withholding.
- **Schema 12 → 13:**
  - `db_writer_reader_test.cpp`, `history/history_engine_schema_test.cpp`, `history/history_chronology_schema_test.cpp`, `history/history_figures_reader_test.cpp` ("this build reads 12"), `history/history_relations_reader_test.cpp`, `history/history_alliances_h16_test.cpp`, `stages_regions_golden_test.cpp`;
  - `tools/worldgen-cli/tests/cli_biomes_test.cpp`, `cli_regions_test.cpp`, `cli_history_report_test.cpp`.
- **Generator version 0.28.0:**
  - `data_history_golden_test.cpp`, `db_writer_reader_test.cpp`, `p0_04_reference.hpp`, `history/history_alliances_data_test.cpp`, `history/p5_04a_api.hpp`, `history/p5_06_figures_api.hpp`;
  - `tools/worldgen-cli/tests/cli_history_conflict_test.cpp`, `cli_history_settings_test.cpp`, `cli_passes_test.cpp`.
- **Streams and kinds:** `STREAMS_SPEC` 98 → 103 and `KINDS_SPEC` 92 → 96 in `history/p5_04a_api.hpp` and `history/p5_06_figures_api.hpp`. Also any catalogue test that counts 92 kinds.
- **Monster kinds 6 → 9 (rows 7 to 9 written for every definition):** P5-02's monster data and catalogue tests (`history_monsters_data_test.cpp`, `history_monsters_render_test.cpp`) and `p5_02_monsters_content.hpp` users that iterate every kind. monsters-small now scales the deep kinds too.
- **Changed templates (8.2):**
  - `civ_ended` with its cause words, `region_renamed` and `refugees_arrived` with `hold_lost=`, and `ally_refused` motive 5 (`Hoping to profit by its ally's losses`);
  - the listings in `p3_01_history_data.hpp`, `history/p5_04_alliances_data.hpp`, `history/p5_06_figures_data.hpp`;
  - the fixture roots `data/geo/history/report.toml` and `data/geo-small/history/report.toml` (the engineer's part D);
  - the H18 fixture's `ally_refused` template (`h18-v9.toml`) if it is re-rendered at 0.29.0.
- **A1 (7.5):** relations goldens and P5-04a's relation checks where a turn cited a quiet renewal's event (`p5_04a_checks.hpp` item 13's fallback-cause check).
- **The world cap (6.4 "Caps"):** P5-02's invariant 3 in `p5_02` C++ checks (`history_monsters_golden_test.cpp`) counts living monsters against L_world. It must leave out kinds with `wake` 3, and it must leave deep lairs out of the one-lair-per-region rule, as the reference now does.
- **P3-06 invariant 3 (L-7):** the C++ copies of the flight check (a `refugees_settled` caused by a sack) must accept a `hold_lost` of the same year (`history_conflict_*` checks of 20.13).
- **References:** the default `use_version` of `p3_conflict_reference` (0.27.0), `p3_report_reference`, `p4_*` (0.28.0), `p5_02` (0.27.0) and the other `main`s moves to 0.29.0 when the goldens are re-recorded.
- **Goldens:** every world and history golden; `p4-05-browser.txt` (the new kinds' groups); `p4-04-markers.txt` (the `hold_lost` symbol); `p4-01-timeline.txt` (monster states with 94 and 95).

## r2

Spec r2 (2026-11-26): 6.10 goblin deep warrens, 6.11 the allies' aid, 6.12 the build's readings, the corrected samples. Written from the spec alone, on `p5/07-tests` (base d7e8e6f), without the engineer's branch. The switch and the file list are as r1's; what r2 adds:

### Reference

- `p5_07_depths_reference.py`: `Deep.delve_warrens` (6.10, T4 and Q4 after 6.2), the warren candidates of `Deep.waking` after every mine, `Deep.aid` (6.11, before the gate) with its events and `credit`, `warren_sealed`, `from_warren` and the tellings + 3; module flag `R2` (off: r1's rules). `units`: 231 checks, 0 failures (r2: the region test, the warren pace and wake chance, the goblin gate samples 242/166/102, the aid chance per race 400/300/300/200/350/75, S_a, the village with aid 346 and its boundary, the tellings, every r2 template at each telling and depth with 8.2's goblin samples, the catalogue rows 94 and 97 to 99, the r2 fields' starts and `delving_permille` as shipped). `violations` gains 13 items 12 to 15 (`outcome_violations`, `warren_violations`, `aid_violations`), `reader_problems` 9's r2 item, `stats_of_tables` the six r2 stats, `warren_depth_at`; `sealed_year` reads 99.
- `fixtures`: 68 checks, 0 failures. depths-small on the eleven stress geographies shows deepening, waking, sealing, falls, later takes, flights, a realm lost, forced seals, a refused later take, **warren delved / depth 3 / woken / sealed / lost** (first warren delving: stress-0, year 694; first warren waking: stress-0). It shows **no aid**: the stress worlds form few alliances, and the wakings fill the kinds' `max_alive` (5) early. So the run adds **depths-allied** (depths-small on P5-04's alliances-small, the deep kinds' `max_alive` 20, `deep_aid_hops` 8) on seeds 3, 5, 6, 7. **The first aid is allied-5, year 2062** (allied-5: 3 aids, allied-6: 1, allied-7: 3). Every invariant holds on all fifteen runs, and so do the reader checks and P5-02's invariants. Not seen: two aids at one waking, and an aid's credit kept as a contribution (the aids seen were between races that had not met, or of one race). Both are reported, not failed. I0 holds on stress 0 to 2. **I1** (delving 0 for every race, `deep_aid_permille` 0, against `R2 = False`) gives equal tables on stress 0 to 2.
- Data: `MONSTERS_NEW_029` holds the sixteen fields; depths-small has the r2 edits (`DEPTHS_SMALL_029`, `DEPTHS_SMALL_RACES_029`). `delving_permille` is in `RACE_029`, filled from `DELVING_029` for the shipped root before the engineer's data, and 0 for the fixture roots. `REPORT_ENTRIES_029` holds the six-word 94 and 96 and the entries 97 to 99.
- Other references: kinds 97 to 99 and 94's site 0–1 in `p3_engine_reference.py` and `p3_peoples_reference.py` (99 kinds). The rest:
  - `p4_browser_reference.py`: the groups;
  - `p4_timeline_reference.py`: 99 sleeps;
  - `p3_report_reference.py`: `sealed in` at a `warren_sealed`;
  - `p5_03_relations_reference.py`: `ally_aided_hold` among the positive cause kinds;
  - `p3_conflict_reference.py`: reads `delving_permille`.

  The existing `units` still pass: `p5_02` 102, `p5_03` 186, `p5_06` 149.
- H19 (`p5_07_h19_fixture.py`, regenerated): the alliances c1–c2 and c2–c4 of 1460; the waking of 1500 with **two aids** (e41, e42), so its fall e43 cites e40, e41, e42; the warrens of Gorzhak to depth 3 (e44 to e46), woken e47 (telling 4) and **sealed** e48; Gnashgut (founded e37 in 1450) to depth 2 (e49, e55), woken e56 (telling 5) and **lost** e57 (telling 3: 412 died, 618 fled). The wakings e29 and e31 have no aid. The r1 ids up to e36 are unchanged; the later ones moved (e37 → e40, e40 → e51, e41 → e52). The reference reads it with no reader problem.

### C++ (all behind `UNMAPPED_P507_API` except the unit file's arithmetic)

- `p5_07_api.hpp`: 107 streams, 99 kinds, `K_AIDED`/`K_WARREN_DELVED`/`K_WARREN_SEALED`, `with_race`. depths-small gets the r2 edits, `i1_content()` is added, and `warren_depth_at` gets an adapter for `Timeline::warren_depth_at`.
- `p5_07_checks.hpp`: the r2 formulas, plus `Tuning` with `delving`, `aid_max`, `aid_hops`. It also has `outcome_violations` and items 12 to 15 in `violations`, the six r2 stats and `warren_depth_at`.
- `history_depths_unit_test.cpp` (runs now): the warren region at the boundaries; the pace (3, 150) and the wake chance (120, 30, 192, the double floor, the min over the product); the goblin gate samples; the aid chance by race; S_a; the village with aid at 346, where u = 345 seals and u = 346 falls (and 345 falls without the aid); the tellings.
- `history_depths_data_test.cpp`: the sixteen fields in order with the r2 starts and bounds. `delving_permille`: shipped values, bounds, absence counts 0, and `race_delves_one_way` (dwarf delving 1; goblin mining 1).
- `history_depths_property_test.cpp`:
  - depths-small must show the paths "warren delved", "warren woken" and "aid" (4.2), and warns on depth 3, a seal, a loss and two aids;
  - I1 as rows show it (no event 97 to 99, every waking at a mine, tellings 0 to 2);
  - `deep_aid_max` 0 gives no aid; with `deep_aid_hops` 0 the origin is in the hold's region;
  - no mine founded by a warren delver, and every siteless waking has a warren.
- `history_depths_reader_test.cpp`: 9's r2 item, on the first depths-small world with a warren at depth 2 and a warren's waking:
  - the values of 98 and 94's telling 6;
  - the depth chain;
  - `delved by two realms`;
  - `waking of settlement <t> without a warren` (the waking's civilization changed);
  - `warren_sealed without its waking` and `ally_aided_hold <e> does not answer its waking` (ally = partner), when present.
- `history_depths_report_test.cpp`: H19's new ids and lines:
  - the r2 sentences of 97 to 99 and 94/96 at tellings 3 to 5;
  - e43's causes;
  - `sealed in` at e48 (10 item 7a), the s7 ruin, the two new alliance lines and the `allies:` lines;
  - the fourteen stats;
  - `warren_depth_at`, `sealed_year(4)` = 1540 and the monster state at 1540;
  - the groups of 97 to 99 and their plain symbols.

  The row case (runs now) uses `deep_years` 100, 200, 300 and goblin delving 300.

### Waits for the engine

Everything above except the unit file's arithmetic and H19's row case. In addition:

- the engineer's `p5_07_depths_content.hpp` (6.12 item 4) and this branch's `p507api::depths_small()` must give the same world file. Compare them at integration;
- `golden/p5-07-depths.txt`: the stat lines now carry the six r2 stats, so the kind 97 to 99 counts are `deep_aided`, `warrens_delved` and `warrens_sealed`;
- `p3-07-report.txt` with H19 at r2.

### Readings (r2; the D-R numbers continue `READINGS`)

18. **D-R18 (4.5):** the spec gives no position for `delving_permille` in the race record and does not say how an absent optional field encodes in the content hash. Read as: appended after `magic_permille`, and absent encodes as 0. A finding if the build differs. This is the first optional field in these kinds: the data reference had none.
19. **D-R19 (6.10):** "its region qualifies" is P3-05's upland (4 × `walk_cells` < `cells`), else `ore_reach_cells` ≥ `deep_warren_ore_min`.
20. **D-R20 (6.10, invariant 12):** suppose a warren is taken by another realm and won back while its delver started another warren meanwhile. Then the delver has two live warrens: the rule (`d ≥ 1: warren_delver(t) = c`) lets the old one go on. So invariant 12's "at most one live warren at every year's end" can fail under the spec's own rules. The row checks end a warren's life at the first capture or cession of its settlement, so they do not report this case. Ambiguity, for the spec writer: either "live" excludes a warren that was taken, or a won-back warren is frozen.
21. **D-R21 (6.11):**
    - the aid's hops are GeoView's (every border, as D-R5) from the hold's region;
    - q is chosen among the ally's standing visible settlements;
    - no credit when the races are equal, nor before they have met (`credit` does nothing then).
22. **D-R22 (6.5, 6.11):** a forced seal is decided with the aid in S. The aid events are written even when the seal is forced.
23. **D-R23 (6.10):** a warren's threat region for the forced seal's plan is the settlement's region.
24. **D-R24 (6.6):** `hold_lost`'s + 3 holds for every take by a warren horror, later takes included (invariant 15 says so; 6.6 item 3 says "when `from_warren(m)`").
25. **6.10 "years under its present owner"** uses `year(owner_event(t))`. Rows do not store `owner_event`, so invariant 12's years are checked against the first delving's cause (the owner event at that time). That is a necessary condition only.
26. **6.10 waking:** "owner c = `warren_delver(t)`" is read as owner(t) = `warren_delver(t)` ≠ 0.
27. **Invariant 14's "no relation row ... caused by it":** read as no relation row whose since or until event is the aid, and no `war_declared`, `ally_joined_war`, `ally_refused` or `claim_settlements` citing it. A `disposition_turned` may cite it (the credit is meant to count).

### Compared with the build's readings (6.12)

- Item 1 (the forced seal uses the take's plan, V after the toll, the threat region the lair's): the r1 reference did this already (`Deep.wake`), so it agrees.
- Item 4: the engineer's in-memory content. See "Waits for the engine".
- Item 5 (`hold_lost`'s site is the smallest open kind 3 site at t): agrees with the r1 reference (`mine[0]` of the sites ascending).
- Item 6 (`takes ≤ deep_takes_max` before the draw, counting the first): agrees with `may_take_later` (D-R5's reach from the lair's region).
- Item 7 (Q4: p computed, then the budget): agrees with D-R8.
- Item 8: agrees (the flight's n from the population after the toll).
- Item 9 (common-enemy goodwill does not read `hold_lost`): the reference adds nothing there, so it agrees.
- Item 10 (the report): agrees with the r1 reference. r2's `sealed in` for 99 is added.

D-R2 and D-R3 are now the spec's text (218/137; 1,206/1,810). D-R1 is now the spec's name.
