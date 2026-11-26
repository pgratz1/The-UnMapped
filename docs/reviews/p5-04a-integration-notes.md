# P5-04a integration, part 1: tests and references joined to the engine

Spec `docs/specs/p5-04a-relations-alliances-followups.md` r2, generator 0.28.0, report v9. Branch `p5/04a-int`.
Part 1 (this note): the adapter switched on, the references brought to the calibrated data and compared with the
engine, the readings settled. Part 2 (the next agent): stale expected values in the older tests and the older
goldens (section 4).

## 1. What was done

- `worldgen/tests/history/p5_04a_api.hpp`: `UNMAPPED_P504A_API` is defined. The spec fixed every name the tests
  use, so nothing else in the adapter changed.
- `history_alliances_p504a_data_test.cpp`: the shipped values are the calibrated ones
  (`docs/reviews/p5-04a-calibration.md`). The spec's start values are kept in comments.
- Fixture H18 (`reference/p5_04a_h18_fixture.py`, `data/history-report/h18-v9.toml`) was rewritten for the engine's
  readers. The rows at E that repeated a pair's previous `race_relation` row are gone (the reader refuses them:
  "equals the previous row of this pair"). `civ_conquered` e91 now has its region (region_use 1; the reader refuses
  region 0). H18 was added to `hrtest::fixture_names()` and to `FIXTURE_NAMES_V9` of the report reference.
- Report v9 is now in `p3_report_reference.py` (`use_version("0.28.0")`): 12.1 to 12.6, 21.5 items 1, 2 and 4, and
  the 12.3 and 12.5 lines through `p5_04_alliances_reference.alliances_block(..., v9=True)`.
- I0 and the fixture runs at 0.28.0: `p5_04 fixtures --version 0.28.0`, `p5_04 i0-world --version 0.28.0` and
  `p5_06 i0-world --version 0.28.0`. At 0.28.0 the zero tunings hold the new fields at their shipped values.
- The tower unit cases of 16 item 16 are `check_tower_units_028` in `p5_06_figures_reference.py units` (147 checks,
  no failure). They cover a free cell at D and at D + 1, no free cell, D = 0, ties, open and ended towers of the
  realm (both repel), a rival's tower (does not repel), each ending in the table's order, and a realm's fall before
  a lost settlement.
- Goldens: P5-04a adds no new golden file. Every new test reads the H18 fixture or computes its values. Nothing was
  recorded. The older reference goldens and the world hashes are part 2's and validation's.

## 2. Engine and references on 24 worlds

Worlds: `worldgen-cli generate --map-size 1024`, with default seeds 0 to 11, plus seeds 0 to 3 of each of
`--monster-pressure high`, `--event-intensity turbulent` and `--civs-per-race 4`. They are in
`~/.cache/the-unmapped/out/p504a-int/worlds/`. Scripts are in `.../p504a-int/scripts/`: `run_all.sh`,
`cmp_world028.py`, `rep_cmp.py` and `recheck.sh`.

- The replay (`p3_conflict_reference.history_stage` at 0.28.0) equals the engine table for table on all 24 worlds.
  This covers every history table, `figure` and `book` included. Conflict's invariants and hard bounds pass.
- `check-world` passes on all 24 worlds for `p5_04` (with the 0.28.0 invariants and reader checks), `p5_03`
  (`--version 0.28.0`), `p5_06` (`--version 0.28.0`), `p5_02` and `p5_12`.
- `report --history` equals `p3_report_reference.py` at 0.28.0 at both levels on all 24 worlds. H18's report
  tests pass.

## 3. Readings settled (which side moved)

All of the engineers' readings that the worlds can show were confirmed by the comparison. None was moved.

| Item | Reading | Side moved |
| --- | --- | --- |
| Reference data | A 0.28.0 history directory was read as 0.27.0, so the replay used the 0.27.0 listings. Fixed with `pre_028` and `same_version` in `p3_data_reference.py` | reference (defect) |
| 2.1, 4.4 deed | `ally_refused` with `motive` 5 names the ally's ruler (P5-06 6.4 rule 3). The replay did not have it; it was the first difference on 9 worlds. Invariant 3 gains it | reference, to the spec's text |
| 4.2 f2 from rows | `open_wars_of(z)` (P5-04 7.6) counts z's own joined records. A joined row opened by another realm against z is not one of z's wars. The row check had counted it (turbulent 3, e1678) | reference and `p5_04a_checks.hpp`, to the spec |
| 21.4 cells | An ended tower's cell is free again, and a rival's tower does not repel (test reading 16). P5-06 invariant 16 ("two towers share a cell") now applies only while the earlier tower is open (civs4 2, si20, si38, si49) | reference and `p5_06_figures_checks.hpp` |
| 6.1 chain | A renewal that waited (a quiet step longer than the term, or a wait for budget) is caught up one due year per T2 or Q4, as 6.1's text gives ("the next due year is d_{n+1} whatever Y was"). The check is now "the j-th written renewal of a row has j due years at or before it", not "a due year after the last written one" (alliances-small 3, e715) | reference and `p5_04a_checks.hpp` |
| 12.2 | `own` = P3-07's own(c, e) among the events naming the realm (engineers' reading 5) | none (same) |
| 12.4 | A group needs two or more consecutive listed events of one kind and year. It has no refs, causes or note, and the settlement's text is the linked render's settlement span (engineers' reading 4) | none (same) |
| 21.5 item 1 | A reserved pick may have renown 0 (engineers' reading 6). A ruler's tie is broken by (died or E) − `office_from` | none (same) |

For the lead (a spec gap, not an engine defect): **I0 of P5-06 7.11 cannot hold at 0.28.0.** With the figures
zero tuning, reigns still draw their temper from the race's `temper_weights`. So `FigureView::temper` can return 3
(grasping), and 4.2's f5 (`grasping_join_permille`, an alliances field) changes the defence chance. Betrayal motive
0 changes `alliance_betrayed`'s value. To reproduce it, run `python3 -B worldgen/tests/reference/p5_06_figures_reference.py
i0-world --version 0.28.0 --inputs ~/.cache/the-unmapped/out/p5-02-tests/inputs geo-seed-42-512`. Zero and off
first differ at e508 (5144): `ally_refused` with motive 5 against `ally_joined_war`. geo-seed-0-512 passes. The
engine follows 4.2 and 5 as written. Either 14.2a's zero root needs `grasping_join_permille` 1000 in the I0
configuration, or 16 item 14 should drop P5-06's I0. The check stays failing in the reference. P5-04's I0
(alliances) passes at 0.28.0 on both cases.

## 4. Engine defects

None found.

## 5. What still fails in the fast suite, by cause (for part 2)

Run: `ctest --preset linux-gcc-release -j 5` (the log is `~/.cache/the-unmapped/out/p504a-int/ctest-fast.txt`;
the failing cases with their first message are in `.../fails.txt`). 21 of 45 entries pass and 24 fail. Every new
P5-04a case passes: the 32 cases of `history_*_p504a_*` and `history_report_v9_test.cpp`. No failing case is a
P5-04a case. The causes, one line each:

- **Version strings**: `0.28.0 == 0.27.0` (db, generate, reader, P3-01, P5-06 9.4, cli version and help lines).
- **Stream count**: `98 == 97` (P0-01 amendment 20, P1-03 C2b, P3-05 3.3, P3-06 3.3 and r9 20.10).
- **Data roots without the 0.28.0 fields**: `REQUIRE(21 == 1)`, `REQUIRE(false)`, `CHECK(37 == 34)` and
  `CHECK(70 == 55)` in the P3-01, P3-05, P3-06, P5-02, P5-03, P5-03a, P5-04, P5-06 and P5-12 data tests. The
  roots built from the spec listings, the fixture TOML or `shipped_edited` lack the 20 appended fields, or the tests
  count the fields of 0.27.0. The `P5-04 4.2 ... 54 int fields`, `withheld_*` and `alliances-small` loads are here
  too.
- **Report header v8 → v9** and the v9 text: the P3-07, P5-02, P5-03, P5-03a, P5-04 (H16) and P5-06 (H17) report
  tests. This covers milestones at summary (H9 and H15 `milestones: 8`), the `allies:` names, the `[stats]` key
  lists, H17's `[figures]` summary and the `reign <n>` at summary (`P5-06 10.3`).
- **Catalogue values of 0.28.0**: kind 58 `change`, kind 63 `term` and `band` (P5-03 8.1, P5-03a 10.4, P5-04 8.1).
  The changed templates: the battle prefix `{@causes:arms_sold=...}` (P3-06 11), `monster_died`
  `crumbled to dust at last` (P5-03a 7.1), and 57 to 62 (P5-03a 7.2, 10.3).
- **Renderer 8.5**: a choice of 4 to 8 words is now valid (P3-04 8.4, 16.1 item 10; P3-04 r10 8.4's four-word case).
- **P3-04 r17 21.3 item 2** (`history_engine_r17_test.cpp:209`): the heuristic reads a three-word choice at byte
  0 as a number clause. It flags 3.6's `alliance_spurned` (`{reason?Still bitter|Long resentful|Outraged}`), a
  template the spec gives. The check is stale; this is not a data defect. Lead: confirm.
- **World and reference goldens of 0.27.0**: these are the table hashes, dumps and stats of `p3-04` to `p3-07`,
  `p4-01`, `p4-04`, `p4-05`, `p5-02`, `p5-03`, `p5-04`, `p5-06`, `p5-12`, the P0, P1 and P3 world hashes and the
  data content hashes (`P3-01 8.3 test 9`, cli P3-01 7). The P4-01, P4-04 and P4-05 "every case" checks also see the
  new fixture `h18-v9` (`29 == 30`), which the re-recorded goldens will hold. The world-level counts of the 512
  cases fall here as well: `P5-02 6.2.3 beasts_bold 2 == 1`, `P5-02 14 285 == 307`, `P5-02 19.3 94 == 90`,
  `P5-04 17 4 == 5`, `P5-06 17 33 == 37`, `P5-12 13 11 == 9` and P4-01's region name at a year.
- **P5-04 13 item 12** in `history_alliances_property_test.cpp` (`12: e93 reason 0 with 1 withheld`): 16 item 13
  replaces it at 0.28.0, and the new property test filters it. The old test should filter it too.
- **P5-02 11.4** (`settlement row 4: ending 1 is not its ruin's ending 4 == ...`): the expected first message of a
  corrupt copy differs. The likely cause is the order of the readers' checks after 14.4's additions to
  `monster_tables_problem`. Part 2 should check that only the expected text is stale.
- **The world report's text v2** (`# worldgen report v2` in cli and report tests): the expected `generator_version`
  and `content_hash` meta lines.

Not run here: the slow suites (`*-slow`, including the new `alliances-slow`, `relations-slow` and `figures-slow`
cases), the other presets and `world-hashes.sh`. `linux-gcc-release` builds with no warning.

The reference self-tests: `p5_06 units` 147 with no failure. `p5_03 units --version 0.28.0` 186 with no failure.
`p5_04 units` 154 with 3 failures (the P5-04 grant-chance units, stale against the shipped `spoils_grant_permille`,
as at HEAD). `p5_04 fixtures-028` and `fixtures --version 0.28.0 --quick` pass. `p5_04 i0-world --version 0.28.0`
passes on geo-seed-0-512 and geo-seed-42-512. `p5_06 i0-world --version 0.28.0` fails on geo-seed-42-512, which is
the spec gap of section 3.

## Part 2: stale expected values of the older tests, goldens re-recorded

Branch `p5/04a-int`. Build `linux-gcc-release` without warnings; determinism lint clean (self-test 22 snippets, 189 C++
files, 0 findings). Part 2 changed no file under `worldgen/src`, `worldgen/include`, `tools/worldgen-cli` (sources) or
`game/data`.

### Changes by cause

1. **Versions, streams, headers.** `0.27.0` to `0.28.0` in the db, generate, reader, P0-04 reference header, report
   (`generator_version`), CLI version tests and `p5_06_figures_api.hpp`; stream count 97 to 98 (new last name
   `hist.alliances.renew`, so the offsets of the name checks of `stages_regions_names_test.cpp` moved by one and
   `history_conflict_data_test.cpp` expects it last); header `# worldgen history v8` to `v9` in the report tests, the CLI
   report test and its help-text expectation.
2. **Data roots.** The listings of `p3_01_history_data.hpp` (alliances, figures, relations, report, history monsters,
   dragon) are again verbatim copies of `game/data` (replaced from the 0.27.0 text by the 0.28.0 text, the 20 fields
   included); the fixture roots `worldgen/tests/data/geo` and `geo-small` carry the same six files (they were copies of
   `game/data` before). The older stage listings (`p5_06_figures_data.hpp` `files_027()` and its figures constant,
   `p5_04_alliances_data.hpp` alliances listing and its field table, `p5_03a_relations_data.hpp` field list) got the
   same line edits (fields appended in the spec's order at the shipped values; `withheld_grudge` 150,
   `withheld_to_break` 2; `temper_allied` 300/300/300/3000/150; skill 600 to 900; the Old 1400/1480; `hunt_permille`
   12; dragon `overrun_permille` 1000; the changed report templates). Field counts: alliances 55 to 70, relations 34 to
   37. `p3-01-data.txt` is the reference's hash of `game/data` (`38476882...`).
3. **Catalogue values, templates, renderer.** Kind 58 `change`, kind 63 `term` and `band`, 67 `motive, years`, 69
   `motive, count`, 75 `share, grievance`, 76 `reason, years` in the catalogue tests (kinds 64 and 70 and 74 keep
   their values); the expected templates of 14.4 (battle prefix `{@causes:arms_sold=...}` through
   `with_p506_templates`, the P5-04 template table, `disposition_turned`, `monster_died` `{monster:died}` with the
   sentences "crumbled to dust at last" for the undead); P3-04 8.4: two to eight words are valid, nine are refused with
   `choice of 9 words at byte 0`.
4. **P3-06 battle cause check.** A battle may cite one extra `arms_sold` (kind 78) besides its declaration (9.2).
5. **Old P5-04 invariant 12.** `p5_04_alliances_checks.hpp`: the clause "reason 0 only after `withheld_to_break` withheld
   shares" is removed (replaced by the spec's 16 item 13, checked in `p5_04a_checks.hpp`); the other parts of item 12
   stay.
6. **Lead rulings.**
   (a) I0 of P5-06: `p5_06_figures_reference.zero_data` and the C++ `p503api::figures_zero_of` set
   `alliances.grasping_join_permille` to 1000. `p5_06_figures_reference.py i0-world --version 0.28.0 --inputs
   p5-02-tests/inputs geo-seed-42-512 geo-seed-0-512` passes (2 checks, 0 failures); the C++ I0 cases pass.
   (b) P3-04 r17 21.3 item 2 (`history_engine_r17_test.cpp`): the heuristic took any three-word choice at byte 0 for a
   number clause. It now flags one only when the last word is exactly `#` (`{n?none|one|#}`); a word choice such as
   alliance_spurned's `{reason?Still bitter|Long resentful|Outraged}` (the spec's template) or hero_rose's
   `{quiet?After the raids of |After the raids of |When, after # quiet years, }` (a `#` inside a word) is allowed.
   (c) P5-02 corrupt copy (`history_monsters_render_test.cpp`): the expected message is `... ending 1 is not its ruin's
   ending 4`; in the 0.28.0 world s4 is a ruin whose cause gives ending 4, so the message describes the corruption
   correctly (only the expected text was stale).
7. **World-file hash goldens** (`p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, the inline hashes of
   `db_determinism_test.cpp`, `stages_pipeline_test.cpp`, `p0_04_reference.hpp` table hashes, the CLI `cli_p1_generate`
   and `cli_golden` tests): left failing for validation.

Report tests (format v9): the civilization blocks of H1, H2, H9, H11 and H15 follow 12.2 (the count, then the three
weightiest own events of salience 40 or more, no quiet stretch at summary), checked against the reference's text of
each fixture (`p3_report_reference.py fixture` and `report`); the [stats] tails grew by 19 keys (12.6); a same-day group
line `e82, e83 ...` (12.4) is read as several events by the shared checks, the engineer test and the browser test;
`allies:` names and `against c4 The Gub Horde` / `in fear of c3 The Hold of Durn` in the H16 expectations (the
reference's text); the claim-note hashes of `history_report_v3_test.cpp` are the reference's text of the same world at
v9 (summary `1fc91d5d...`, full `d4036290...`). The rulers line test accepts `1 reign`. P3-06 `region_name` renames of
the P3-04 r10 render test now also cover region 5.

Fixtures H16 and H17 (`worldgen/tests/data/history-report/h16-alliances.toml`, `h17-figures.toml`): their kinds
`ally_refused` and `alliance_betrayed` had no values (the 0.26.0 catalogue) and one event of each; a kind that lacks the
value reads as 0 in the C++ [stats] but as absent in the reference (`allies_refused_excused`, `refusal_motive_0`,
`betrayal_motive_0` differed by one). The fixtures now carry the 0.28.0 values (`motive, count` = 3, 1 and `motive,
years` = 2, 100); no sentence changes. (A value that the catalogue does not declare reading as 0 is a small difference
between engine and reference for old files; real worlds always hold the values.)

### Goldens re-recorded (the reference's `golden` command, never C++ output)

`I` = `~/.cache/the-unmapped/out/p5-02-tests/inputs` (geography inputs), `RI` = `~/.cache/the-unmapped/out/p504a-int/report-inputs`
(`p3_report_reference.py --version 0.28.0 build-inputs` of the 0.28.0 C++ worlds `gen.sh` made at 1024 and 512).

| File | Script and mode |
| --- | --- |
| `p3-01-data.txt` | `p3_data_reference.py golden --version 0.28.0` |
| `p3-04-engine.txt` | `p3_engine_reference.py golden --version 0.28.0 --inputs I` |
| `p3-05-peoples.txt` | `p3_peoples_reference.py golden --version 0.28.0 --inputs I` |
| `p3-06-conflict.txt` | `p3_conflict_reference.py golden --version 0.28.0 --inputs I` |
| `p5-12-renaming.txt` | `p5_12_renaming_reference.py golden --version 0.28.0 --inputs I` |
| `p5-02-monsters.txt` | `p5_02_monsters_reference.py golden --version 0.28.0 --inputs I` |
| `p5-03-relations.txt` | `p5_03_relations_reference.py golden --version 0.28.0 --inputs I` |
| `p3-07-report.txt` (H18 included) | `p3_report_reference.py golden --inputs RI` (default 0.28.0 now) |
| `p4-01-timeline.txt`, `p4-04-markers.txt`, `p4-05-browser.txt` | `p4_timeline_reference.py`, `p4_markers_reference.py`, `p4_browser_reference.py golden --inputs RI` |
| `p5-04-alliances.txt`, `p5-06-figures.txt` | `p5_04_alliances_reference.py`, `p5_06_figures_reference.py golden --inputs RI` |

`p3-02-names.txt` is kept as committed (spec 17). Reference changes needed for this: (1) each reference run as a script
registers itself under its module name (`sys.modules.setdefault`), because the flag `p504a` of
`p3_conflict_reference` was off in the imported copy of the module and the alliances replay then wrote the 0.27.0 value
lists (an assertion in the engine reference's `append`); (2) `p3_report_reference`, `p4_timeline_reference`,
`p4_markers_reference` and `p4_browser_reference` default to 0.28.0 (meta `generator_version`, accept
`--version 0.28.0`); (3) `p4_browser_reference` block lines follow 12.2 and `civ_alliance_lines_v9`; (4) the I0
`zero_data` ruling above. `worldgen/tests/reference/README.md` has a section "0.28.0, integration part 2".

### Listed, not fixed

**Seed 9 of the conflict stress geographies: engine and reference disagree** (not patched, not dug). The cases are
`syn-9` and `stress-9` of `p3-06-conflict.txt` and the fixtures built on stress seed 9: `monsters-small-9` (P5-02
19.3) and `rename-small-9` (P5-12 13, 12 checks). Every other seed of the same cases agrees (seeds 0 to 15 of the
sixteen synthetic geographies, the other stress seeds and every world case). First lines:

```
P3-06 14, 16.2 item 15 ... (history_conflict_property_test.cpp:49): syn-9: event, event_cause, event_participant,
    event_value, relation, settlement, site differ (event 56706657... against 6e31eac9...)
P3-06 7, 14 ... stress-9: site differs only (90334c1e... against ba6fddec...); the same site hash is monsters-small-9's
P5-12 13 ... rename-small-9: region_name differs (9f9401fc... against 09db1345...)
```

The history of syn-9 diverges (event tables), the history of stress-9 does not (only `site`). The next agent should
dump the first differing event or site row of seed 9 on both sides.

Entries that fail now: `worldgen-tests-renaming`, `-conflict`, `-monsters` (seed 9 only) and `worldgen-tests-other`,
`worldgen-cli-tests-other` (world-file hashes only: `cli_golden_test`, `cli_p1_generate_test`, `db_determinism_test`,
`stages_pipeline_test`). Every other entry passes, the partition checks included.
