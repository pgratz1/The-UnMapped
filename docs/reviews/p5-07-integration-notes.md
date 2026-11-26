# P5-07 integration, part 1: reference against engine, C++ tests on the real API

Test writer, 2026-11-26, `p5/07-int` (engine r2, both calibrations); spec `docs/specs/p5-07-mountain-depths.md` r2.

## Result

- **No engine change. No world byte changed** (the calibration figures stand).
- `p3_conflict_reference.py check-world` (the full replay, table for table) agrees with the engine on all 36 sweep
  worlds at 1024: default seeds 0 to 23, and seeds 0 to 3 of each of `--monster-pressure high`, `--civs-per-race 4`
  and `--event-intensity turbulent`. One round was enough: every difference found was on the test side.
- `p5_07_depths_reference.py check-world` (the row invariants of 13 and the reader checks of 9) passes on all 336
  worlds of the sweep and the witness search.
- `golden/p5-07-depths.txt` recorded from the reference (`golden --inputs`, inputs `--until regions`); the C++ test
  matches it on the ten depths-small worlds at 512.
- `worldgen-tests -sf="*history_depths_*"`: 41 of 41 cases pass; the slow suite `depths-slow` 4 of 4 (`linux-gcc-release`).

## Disagreements, and which side was wrong

| # | Where | What | Wrong side, fix |
| --- | --- | --- | --- |
| 1 | `p3_conflict_reference.py` `cmd_check_world` | the data root was loaded before the world's version was selected, so a 0.29.0 root (with `delving_permille`) was refused as `unknown key` | reference: select the version from `meta` first |
| 2 | `p3_conflict_reference.py`, `p3_peoples_reference.py` | `KEY_OF`/`keys` lacked kinds 93 to 99 (`KeyError`) | reference: name `pe.KINDS_029` |
| 3 | `p3_conflict_reference.py` invariants | a ruin by `hold_lost` (P3-05 inv 7), and `capital_moved` and `civ_ended` caused by a `hold_lost` (inv 8), were refused; spec 6.6 items 4 and "The realm's end" make them so | reference: `hold_lost` among the ruin kinds (`overrun_kinds()`) and those causes |
| 4 | invariant 12 (both checkers) | a warren's life in the row check did not end when its region was ceded (`region_ceded`, which names no settlement) or its delver conquered, so a realm seemed to hold two live warrens (d16, high3) | reference and `p5_07_checks.hpp`: those events end a warren's life for its delver; and the lead's restatement (below) |
| 5 | `history_report_fixture.hpp` | the fixture builder derived `ending` 5 for a `hold_lost` ruin, so H19 failed both readers (`ending 5 is not its ruin's ending 2`) | test: `hold_lost` gives 2 (6.6 "The ending") |
| 6 | `history_depths_data_test.cpp` | held 4.1's and 4.2's start values; the calibration moved them | test: the shipped (calibrated) values, with the starts in a comment |
| 7 | `history_depths_data_test.cpp` `refused` | an out-of-range race field drops the race, so the first load error is `race_order_dense`; the field's error comes later | test: some error must hold the words |
| 8 | `history_depths_reader_test.cpp` C2, R1 | each corruption tripped an earlier check first (`cause not below event`; the settlement row's ending check) | test: C2 cites the waking's own cause; R1 also sets `ending` 5 and a ruin event of the same settlement |
| 9 | `p5_07_depths_reference.py units` | compared the shipped r2 fields with 4.2's starts | reference: checks 4.2's ranges on the file; every replayed value is read from the files |

## Lead rulings applied
- **Invariant 12** now reads: a realm starts no new deep warren while it holds a live one. Checked by event ids: no
  warren's first `warren_delved` lies inside the life (to its waking, ruin, capture, cession, secession, a cession of
  its region, or its delver's conquest) of an earlier warren of the same realm. A warren lost and won back, or taken
  from another realm, may leave a realm with two; neither checker reports that.
- `delving_permille`: the last field of the race record (`Field::optional`, absent = 0), as built.

## Readings of the build, for the spec's as-built revision

1. **Aid hops** (6.11): breadth-first distances over the GeoView neighbours (every `region_border` row, ascending),
   from the hold's region (a mine's waking: the mine site's region, which is the lair's; a warren's: the
   settlement's region), computed once as the new horror's `hops`. A region not reached has no distance and
   holds no candidate `q`. The same measure serves the later takes' `deep_reach_hops` (from the lair's region).
2. **A warren's age** (6.10): w = Y − year(`owner_event(t)`), `owner_event` being the event by which t came to its
   present owner (P5-03 `Settlement::owner_event`: its founding, capture, cession or the like). The clock is the
   same for every level, so a warren whose settlement changes hands and comes back counts its years from the return
   (and its levels stay frozen while another realm holds it). A settlement with no owner event is skipped.
3. **Live warren** (6.10): taken once before the delving loop (standing, owner = delver, depth ≥ 1, not sealed;
   visibility not read) and set in the loop when a warren gets its first level, so the first candidate by id wins.
   The warren wake chance floors twice: floor(floor(ppm × M / 1000) × `deep_warren_wake_permille` / 1000), min 10^6.
4. The aid: an ally equal to c, ended or invisible is skipped with no draw; with `deep_aid_max` 0, `deep_aid_permille`
   0 or a seam unset, no aid and no draw. Aid events are written even when the seal is forced (D-R22 agrees);
   `credit` is called per aid between different races. A forced seal at a warren uses the settlement's region as
   the flight's threat region (D-R23 agrees).

## Witness worlds

Searched 336 worlds by replaying each with the reference (its deep log) and reading the event tables: default 1024
seeds 0 to 199; shipped 512 seeds 0 to 99; seeds 0 to 11 of each option set at 1024.

| Path | Seeds (1024 default unless named) |
| --- | --- |
| a seal forced (last refuge, no flight plan, 6.5) | **none in 336 worlds** |
| gate odds at the cap (`deep_seal_max_permille`) | 0, 142; 512: 45, 93 |
| a later take refused for want of a flight plan (6.6 item 2) | **none in 336 worlds** |
| two aids at one waking | 5, 70, 183, 196; `civs4` 0 |
| an aid's credit kept in the pair's disposition (a contribution in the replay's list) | 46, 70, 72, 75, 165, 179, 183; `civs4` 3; 512: 9, 10, 13, 45, 48, 61, 66, 89 |
| a `disposition_turned` citing an aid | **none in 336 worlds** |
| a warren waking | 5, 12, 28, 48, 75, 88, 107, 111, 150, 152, 179; `civs4` 0; `turb` 9; 512: 42, 78 |
| a warren sealed | 5, 48, 75, 88, 107, 150, 179; `turb` 9; 512: 78 |
| a realm ended by the deep | 72 |

Added: fast suite (`history`), shipped 512 seed 78 (a warren woken and sealed) and seed 9 (an aid); slow suite
`depths-slow`, 1024 seed 5 (two aids at one waking, a warren woken and sealed) and seed 72 (a realm ended by the
deep, an aid), with the row invariants. **The forced seal and the refused later take occur nowhere**: not in the
336 worlds, and no longer in the reference's `fixtures` run (68 checks, 0 failures) on the calibrated data (the
r2 tests notes saw both before calibration). Both paths are replayed by the reference and agree in code reading
only; a hand fixture (a race's last realm with no free land within `flight_hops`) is still owed.

## For part 2 (stale expected values of other stages; not done here)

`ctest --preset linux-gcc-release` on this branch: 31 of 46 entries fail, the same 31 as the engineer's (none of
them `worldgen-tests-depths`). Run whole, `worldgen-tests` fails 274 of 1,845 cases and `worldgen-cli-tests` 34 of
140; no failure is a crash, an invariant or a reader error of the deep rules. Failing cases by file:

- Stale embedded data, `worldgen/tests/p3_01_history_data.hpp` (`history_data::shipped_files()` lacks the sixteen
  `deep_*` fields, `forge_depth_permille`, `delving_permille` and the three deep kinds; fix it first, it unmasks the
  rest): `data_history_kinds_test.cpp` 35, `data_history_settings_test.cpp` 8, `history/history_relations_data_test.cpp`
  12, `history_alliances_data_test.cpp` 11, `history_monsters_data_test.cpp` 10, `history_peoples_data_test.cpp` 8,
  `history_renaming_data_test.cpp` 7, `history_balance_data_test.cpp` 7, `history_conflict_data_test.cpp` 6, and most
  of the `history_engine_*` cases (`state` 9, `schedule` 6, `writer` 5, `r15` 4, `r19` 4, `schema` 3, `property` 2,
  `catalogue` 2: 92 report entries against 99, `render` 1, `golden` 1), `engine_hardening_test.cpp` 2.
- Version, schema and counts: `db_writer_reader_test.cpp` 7, `history_chronology_schema_test.cpp` 3,
  `inspect_report_test.cpp` 5, `inspect_hashes_test.cpp` 4, `db_determinism_test.cpp` 1, `stages_pipeline_test.cpp` 1,
  `stages_test_pattern_test.cpp` 1, `history_alliances_catalogue_test.cpp` 1 (92 kinds), `STREAMS_SPEC`/`KINDS_SPEC`
  in `history/p5_04a_api.hpp` and `p5_06_figures_api.hpp`.
- Goldens of other stages (re-record from their references, spec 17): `stages_regions_golden_test.cpp` 3,
  `stages_biomes_golden_test.cpp` 3, `stages_resources_golden_test.cpp` 2, `data_history_golden_test.cpp` 3,
  `inspect_geography_test.cpp` 2, `inspect_geography_slope_test.cpp` 1, `stages_regions_names_test.cpp` 1,
  `stages_climate_offset_test.cpp` 1 (world hashes carry the data's content hash), `timeline_golden_test.cpp` 3,
  `browser_golden_test.cpp` 4, `browser_report_test.cpp` 2, `browser_unit_test.cpp` 1, `markers_golden_test.cpp` 1,
  `markers_fixture_test.cpp` 1, and the history goldens: `history_{engine,peoples,conflict,balance,monsters,renaming,
  relations,alliances,figures}_golden_test.cpp` (1, 1, 1, 1, 3, 2, 2, 1, 1), `history_report_worlds_test.cpp` 1.
- Report v10 text and fixtures: `history_report_v2_test.cpp` 3, `v3` 3, `v4` 2, `blocks` 2, `fixtures` 3, `r17` 1,
  `history/history_report_v9_test.cpp` 2, `history_relations_report_test.cpp` 4, `history_alliances_h16_test.cpp` 3,
  `history_figures_h17_test.cpp` 1, `history_figures_views_test.cpp` 1, `history_renaming_readers_test.cpp` 3,
  `history_figures_reader_test.cpp` 1, `history_relations_reader_test.cpp` 1, `p3_07_engineer_test.cpp` 1,
  `p4_05_engineer_test.cpp` 1; and `p3-07-report.txt` needs `h19-v10` in `hrtest::fixture_names()`.
- Other expected values (most load the stale data or count kinds; not diagnosed here): relations
  (`relation_bookkeeping_engineer` 4, `p503a` 4, `partb` 1, `unit` 1), conflict (`unit` 4, `r19` 2, `property` 2),
  peoples (`unit` 3, `property` 2), monsters (`render` 3, `p503a` 2, `names` 1), `history_figures_unit` 3, renaming
  (`unit` 1, `seams` 1), `history_balance_unit` 1, `history_alliances_p504a_unit` 1 (all `history/..._test.cpp`).
- `worldgen-cli-tests`: `cli_p1_generate_test.cpp` 4, `cli_history_report_test.cpp` 4, `cli_hash_test.cpp` 4,
  `cli_report_test.cpp` 3, `cli_history_settings_test.cpp` 3, `cli_biomes_test.cpp` 3, `cli_regions_test.cpp` 2,
  `cli_history_conflict_test.cpp` 2, and one each in `cli_slope_report`, `cli_passes`, `cli_history`, `cli_golden`,
  `cli_geography`, `cli_generate`, `cli_conventions`, `cli_chronology`, `cli_audit` (`_test.cpp`).

Also the test notes' list ("Existing tests whose expected values change"): the references' default `use_version`
moves to 0.29.0 with the goldens; the C++ copies of P5-02 invariant 3 and P3-06 invariant 3 leave the deep kinds out
of the world cap and accept a `hold_lost` as a flight's cause.

## Part 2: stale expected values of the older tests, goldens re-recorded (generator 0.29.0)

Test writer (Sonnet), 2026-11-26, `p5/07-int`. No file under `worldgen/src`, `tools/worldgen-cli/src`, `game/data` changed.
Determinism lint clean (22 snippets, 190 C++ files, 0 findings).

**Results.** Fast suite `ctest --preset linux-gcc-release`: 49 of 52 entries pass. Slow suite: 21 of 22 pass.

**Fixed (test side).** `p3_01_history_data.hpp` rebuilt from `game/data` by a script (races with `delving_permille`, `forge_depth_permille`,
the 16 `deep_*` fields, renaming rule `deep`, templates of kinds 93 to 99, monsters ymrach, ysgarth, zhakul); the fixture roots `data/geo*/races`;
the old-listing helpers (`p5_06_figures_data.hpp files_027`, `with_p506_templates`); versions 0.29.0, schema 13 ("reads 13"), 107 streams (the nine
`hist.deep.*` last), 99 kinds (test kind now 100), report header v10, the 14 stat keys appended at the end of `[stats]`, the `race_delves_one_way`
check and the optional last race field, browser groups of 93 to 99, H19 in `hrtest::fixture_names()`, the v10 milestone rule (one per kind) in the
expected blocks, `soured eN` in H18's trade lines, region names renamed by the world's history (r19, render tests), invariants of P5-02/P5-03 that
now see `deep_woken` and `hold_lost` (ending 2), deep lairs outside the one-lair-per-region test, one beast bold on geo-seed-0-512, a new witness for
the same-step colony cession (1024 turbulent seed 3: region_ceded e1018, region 26, 5234; seeds 12 and 26 also have one).

**Goldens re-recorded** (each by its reference, `--inputs` = `~/.cache/the-unmapped/out/p5-02-tests/inputs` for geography, `RI` =
`~/.cache/the-unmapped/out/p507-int/report-inputs` for the 13 report worlds, made by `p3_report_reference.py --version 0.29.0 build-inputs` from
0.29.0 C++ worlds):
`p3_data_reference.py golden --version 0.29.0`; `p3_engine`, `p3_peoples`, `p3_conflict`, `p5_12_renaming`, `p5_02_monsters`, `p5_03_relations`
`_reference.py golden --version 0.29.0 --inputs $I`; `p3_report`, `p4_markers`, `p4_browser` `_reference.py --version 0.29.0 golden --inputs $RI`;
`p4_timeline`, `p5_04_alliances`, `p5_06_figures` `_reference.py golden --inputs $RI` (they follow `p3_report_reference`, whose default is now 0.29.0).
`p3-02-names.txt` and `p5-07-depths.txt` unchanged.

**Reference edits (version constants and the new report rules, no replay logic).** `p3_report_reference.py`: default 0.29.0, fixture meta
schema 13, `DEFAULT_INPUTS_029`. `p4_timeline_reference.py`: fixture meta from `prr.GENERATOR_VERSION` and schema. `p4_markers_reference.py`: `v027()` includes
0.29.0. `p4_browser_reference.py`: the one-per-kind milestone rule of 10 item 6 and the `allies:` lines as tagged segments (12 item 4), both as
`p3_report_reference` and the engine already have them.

**Left failing.**
1. World-file hash goldens (validation records them): `worldgen-tests-other` (`db_determinism`, `stages_pipeline`), `worldgen-cli-tests-other`
   (`cli_golden`, `cli_p1_generate`).
2. `worldgen-tests-relations-golden`, `P5-03 23`: fixture `relations-small-6`, table `race_relation` or `settlement_race` (table hash
   a59dc63b... against the reference's 359b18fa...). Every other case of the file passes. Engine against reference on a deep path
   (aid credit under relations-small's large memories?); **not diagnosed, not patched: for the lead**.
3. `worldgen-tests-slow-history-report`, `P3-07 14.4`: `seed-0-1024` summary hash. The history tables agree (`p3_conflict_reference.py check-world`:
   2080 checks, 0 failures), but the report text differs (`p3_report_reference.py compare`): e422 `monster_slain_by_hero` prints the noun clause
   ("..., a host of eyeless things from under the stone, ...") in C++ and not in the reference (first-mention rule for the deep monster m13, kind ysgarth).
   **A report-text disagreement; for the lead** (which side follows P5-02 12.x's first-mention rule is not settled here).
4. Known and left: H15 v6 milestone fact, three grant-chance units of `p5_04_alliances_reference.py units`, `p4_browser_reference.py check` T3.

Rounds used: 8 build-and-test rounds, one full slow run, one rerun of the two slow entries that changed.

## Part 3: the last two disagreements, and a hand fixture for the forced paths

Test writer (Opus), 2026-11-26, `p5/07-int`. Fast suite: 50 of 52 pass (the two `-other` entries fail only on the world-file hash goldens, for validation); slow `history-report|depths|relations`: 3 of 3. **No engine change; no world byte and no report byte of the engine changed.**
Every fix is on the test side (references, goldens, one new fixture and test file).

1. **`relations-small-6` (`settlement_race`): the reference was wrong.** Engine and reference agree on every other
   history table of the fixture; the reference had two extra rows, (18, 2, 1921, 1000, 3) and (18, 6, 1921, 0, 0).
   In 1921 refugees of race 6 arrived at settlement 18 and were expelled again, and the reference wrote its rows
   twice that year: the second write replaced the first in the table (P5-03 6.7: "same-year rows of one (s, g): the
   later replaces the earlier") but was compared with the replaced row, not with the row before it. So it stored a
   row equal to the previous row of (18, 2) (16.4: `equals the previous row`) and a first row of share 0 for race 6
   (6.7: "never written"). The engine's `put_row` (`src/history/relations.cpp`) already drops both. Fix:
   `p5_03_relations_reference.py` `put_row` (per key, the written rows in order; a same-year row replaces the last
   and is dropped when it then equals the row before, or is a first row of share 0), used for `settlement_race` and
   `race_relation`. Fixture data copies were not stale. `p5-03-relations.txt` re-recorded (`golden --version 0.29.0`).
2. **e422 of `seed-0-1024`: the reference was wrong.** P5-07 8.2: "the forms `{m:aside}`, `{m:where}` and `{m:died}`
   of a `deep` kind render as for `wild`", so the noun clause is right. `p3_engine_reference.py`'s render view mapped
   `monster_kind.wake` as `1 -> wild, else dead`, so `wake` 3 read as dead. Now 1 wild, 2 dead, 3 deep. Report text
   goldens re-recorded from the references: only `p3-07-report.txt` changed (`seed-0-1024` summary and full, now equal
   to the C++ report's SHA-256); `p4-01-timeline.txt`, `p4-04-markers.txt`, `p4-05-browser.txt`, `p5-04-alliances.txt`
   and `p5-06-figures.txt` came out unchanged.
3. **Hand fixture `depths-forced`** (`worldgen/tests/data/depths/depths-forced.toml`): depths-small with
   `history.conflict.refuge_flight` 0 (7.3's flight has no plan for any last refuge) and
   `history.monsters.deep_gate_levy_permille` 0 (S = 0 without aid, so every waking falls), on conflict stress
   geographies 48 (span 2000, all races) and 39 (span 6000, dwarf and faerie). Reference replay: seed 48 has two
   forced seals and four refused later takes, seed 39 one and sixteen. `p5_07_depths_reference.py golden` reads the
   file and writes `case depths-forced-<seed> <seed> 0 depths-forced` with the four table hashes, the stats and new
   `path forced_seal <year> <monster> <event>` / `path no_take <year> <monster> <settlement>` lines (the older
   cases' lines unchanged). `history/history_depths_forced_test.cpp` (shard `depths`) runs the engine on the same
   inputs and checks the tables, stats, 13's row invariants, no flight after a `hold_lost`, each forced seal (a seal
   of that monster that year; that horror never takes a hold), each refused take (a raid of that monster on that
   settlement that year with no `hold_lost` of it), and that both paths occur; plus a twice-run determinism case.
   (The brief's "seal forced at the cap (`deep_takes_max`)" is read as the notes' "a seal forced (last refuge, no
   flight plan, 6.5)": the cap on takes is not a seal path.)
