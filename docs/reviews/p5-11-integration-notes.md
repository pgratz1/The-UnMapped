# P5-11 integration, part 1: reference against engine, C++ tests on the real API

Test writer, 2026-11-28, `p5/11-int` (calibrated engine at generator 0.31.0, the tests, main); spec
`docs/specs/p5-11-artifacts.md` with the lead's rulings of `docs/design/proposals.md` ("P5-11 as built and calibrated:
lead rulings"). Part 2 (a separate agent) follows; its list is at the end.

## Result

- **No engine change.** No file under `worldgen/src`, `worldgen/include`, `tools/worldgen-cli/src` or `game/data`
  changed.
- `UNMAPPED_P511_API` is on (`worldgen/tests/history/p5_11_api.hpp`). Every P5-11 test file compiles against the
  engine API unchanged: the 11.1 signatures are the spec's.
- Sweep: 37 worlds, `game/data`: default 1024 seeds 0 to 23 (`d*`), `--divine-activity meddling` and `distant` seeds 0
  to 2 (`m*`, `x*`), `--monster-pressure high` seeds 0 and 1 (`h*`), `--civs-per-race 4` seeds 0 and 1 (`c*`),
  `--event-intensity turbulent` seeds 0 and 1 (`t*`), and default seed 0 at 512 (`s0`).
  - Per world, one process: `p3_conflict_reference` replays the history and compares every chronology and history
    table (schema 15's three included); then `check_invariants` (P3-05, P3-06 and later), `hard_bounds`,
    `p5_11_artifacts_reference.run_problems` on the replay (13 items 1 to 8 with the P5-08, P5-07 and P5-02 replay
    checks) and `violations` on the stored rows (13 and the reader checks of 9).
  - Round 1 (with the reading changes below): 37 of 37 agree table for table; the invariants stopped on kind 111.
  - Round 2: **37 of 37 agree table for table and pass every invariant** (0 failures).
- Report v12 (`p3_report_reference.py --version 0.31.0`, new): **equal to `worldgen-cli report --history` at both
  levels on all 37 worlds and on H21** (the reference's fixture database, reported by the engine). One round of fixes
  (the role word `smith`).
- Rounds used: 2 reference-against-engine rounds of the sweep, 2 of the report comparison.
- P5-11 C++ entries (`linux-gcc-release`): `-artifacts-property-a` and `-artifacts-property-b` pass;
  `-artifacts` fails only on the golden file (part 2) and on engine defect E1.

## Disagreements by class

| Class | Count |
| --- | --- |
| Reference or test error (fixed on the test side) | 12 |
| Reading settled by a lead ruling (reference changed to it) | 5 |
| Spec silent; the engine's behaviour taken and recorded | 4 |
| Engine defect (not fixed; reported) | 1 (E1) |

### Reference and test errors (fixed)

1. `p5_11 units` checked that `cult_return_years` is the last key of `history.pantheon`; the TOML sub-table `creator`
   follows it. Now the last scalar field.
2. `p3_conflict_reference.KEY_OF` lacked kinds 111 to 116 (`KeyError: 111` in the invariants).
3. `p3_peoples_reference.check_r12_invariants` built its kind map without `KINDS_031` (same error).
4. P3-06 invariant 11 ("a battle is caused by its declaration", with at most one `arms_sold`) rejected the 0.31.0
   battle causes. Under the `p511` flag it now also admits a `battle_led`'s `artifact_made` (7.2) and any battle's
   `divine_bless` or `divine_curse` (12.1; 13 item 8 checks that they are in force for their side).
5. Reader test A12 (`UPDATE artifact SET id = id + 100`) collided with an existing id (artifacts-small makes more than
   100): `+ 1000000`.
6. `history_artifacts_data_test.cpp`: the shipped occasions are twelve (calibration added `tower_founded` and
   `book_written`, lead ruling).
7. The adapter's `stats_keys()`, the reference's `STATS_KEYS` and the golden test still carried
   `cult_returns_barred` (dropped by the lead).
8. `p3_report_reference.py` had no v12: added (`--version 0.31.0`): header, `[artifacts]` after `[cults]`, the realm
   `artifacts:` line after the deep line (same summary rule), `smiths <n>` in the `figures:` line, the full figure
   line's `; bore ...` and `; made ...` before its status, the monster line's `, hoard ar..` after `ended`, the 24
   stats, the companions of 8.3, the candidate rule of 8.2 (taken and lost as companions only, buried never), role
   word 8 `smith`, prefix `ar` and entity type 10 names, schema 15 fixture tables (`-1` NULL in `to_year`), H21 in the
   fixture list, `DEFAULT_INPUTS_031` (`out/p511-int2/report-inputs`, for part 2).
9. The P5-11 text used `r<id>` for regions: `rg<id>` (lead ruling).
10. The C++ fixture loader (`worldgen/tests/history_report_fixture.hpp`) did not read schema 15: now reads and writes
    `artifact_kind`, `artifact`, `artifact_holding` and lists `h21-v12`; `data/history-report/README.md` documents them
    and H21.
11. The H21 fixture script took span-2000 candidates only and printed the 0.30.0 header comment; H21 is now made from
    the new fixture `worldgen/tests/data/artifacts/artifacts-h21.toml` (span 6000, see H21 below).
12. The reference's realm line, `forge_of`, the rows checks of the forge (`open_mines_at`, C++ `forges_at`) and the
    unit cases of 6.4 followed the superseded readings (items of the next section).

### Readings settled (numbered; the reference's `READINGS` carry them)

1. **A-R19 forge (lead ruling on S4).** A realm with an open mine at any depth is a forge; among partners the
   largest `forge_depth`, then the smaller id; the deep test is unchanged. `forge_of` (Python and `p511_f`), the
   rows checks (invariant 3) and the unit cases changed.
2. **A-R1 `occasion_event` (lead ruling).** A cause of the making, not the first cause: the reference already did
   this; no change.
3. **A-R20 region (lead ruling, new).** An artifact event that names a settlement takes that settlement's region
   (`artifact_made` at a site across a border; `artifact_hoarded` `how` 2 and 3).
4. **A-R14 (lead ruling).** `cult_returns_barred` is gone from the stats; the return bar of 12.2 stays (invariant 8).
5. **A-R15 (lead ruling, spec 10 item 6).** `wars_citing_artifacts` no longer requires the kind's `claim_permille`
   above 0 (Python and C++ `stats`).
6. **A-R8 M12 (spec silent, the engine's).** A standing seat with no living owner other than c is treated as a ruin:
   lost in t (holder type 4). The first reading (t = 0) is withdrawn.
7. **A-R21 (spec silent, the engine's, new).** M10 takes only for a living captor (as M9; no draw otherwise); M13
   moves nothing when the winner has ended; M6 for a monster with no lair loses the hoard on the event region's field.
8. **A-R13 realm `holds` (spec silent on "at its end", the engine's part D reading 6).** For an ended realm: a holding
   in force in its ended year (`from_year <= ended_year`, open or `to_year >= ended_year`); the reading by event ids
   is withdrawn.
9. **S2 second staff.** The engine keeps a second staff with the realm (its reading 4, the reference's A-R2). No
   figure bears two of one class: invariant 4 holds on all 37 worlds, the 20 property worlds and every fixture case.
   No defect.

## Engine defect

**E1 (spec 9, reader messages).** `artifact_tables_problem` writes `corrupt table 'artifact_holding': holding <seq> of
artifact <a>: <reason>` (`worldgen/src/history/artifact_checks.cpp` 237, 275, 282, 296, 305). Spec 9 gives
`corrupt table '<t>': row <k>: <reason>`, and the readers' convention for a two-column key is `row (a, b)`
(`race_relation`, `event_catalogue.cpp` 1894). Witness: `history_artifacts_reader_test.cpp` H1 to H10 (10 checks,
prefix `corrupt table 'artifact_holding': row `). Cosmetic; the fix is one message format (`row (<artifact>, <seq>)`)
or a spec restatement by the lead. Not fixed here.

## Witness seeds (1024 unless named; `game/data`)

| Feature | Witness |
| --- | --- |
| maker 1 smith / 2 commission / 3 caster / 4 god | d0 ar2 / d0 ar9 / d0 ar5 / d0 ar1 (in 37 / 32 / 30 / 25 worlds) |
| deep iron | d0 ar10 (9 worlds) |
| end states kept, borne, hoarded, lost, buried, enshrined | c0 ar4, c0 ar5, d1 ar8, c1 ar3, c0 ar2, c0 ar1 |
| M1 | d0 ar2 e329 (36 worlds) |
| M2 enshrined / kept / buried | d0 ar5 e364 / d0 ar2 e345 / d0 ar10 e785 |
| M3 taken / lost | d0 ar6 e387 / d10 ar1 e98 |
| M4 | d15 ar1 e24 (3 worlds) |
| M5 by a hero (borne) / by a realm | d15 ar1 e47 / d10 ar6 e1088 |
| M6 | d18 ar1 e135 (1 world) |
| M7 | d1 ar8 e755 (14 worlds) |
| M8 | d7 ar7 e446 (6 worlds) |
| M9 taken | d0 ar11 e747 (26 worlds); M9 lost in a ruin: none in the worlds (sack takes 1000), shown by H21 |
| M10 | d12 ar5 e823 (18 worlds) |
| M11 | none in the worlds (a seat's capture moves the treasure first); shown by H21 |
| M12 | **none**: no `civ_ended` in the 37 worlds, one in seven span-6000 stress runs, never with a kept artifact |
| M13 kept / borne | d0 ar6 e654 / h0 ar9 e986 |
| M14 to a sanctum | d9 ar5 e445 (1 world); to the realm: the fixture stress cases |
| a war citing an artifact claim | d0 e521 cites e387 (ar6 of c2 held by c1); 13 worlds |
| regalia recovered by a won war | d22 ar2 e644 (1 world) |
| a battle citing a blessing or curse (12.1) | d1 `allied_battle` e1346 cites `divine_bless` e1327; 18 worlds |
| a barred cult return | rows cannot show it (no event); see the replay counts below |

Replay counts of 12.2's bar (`Pantheon.returns_barred`, the replay equal to the engine table for table):
nonzero in 7 of 37 worlds (counted per step, as the engine's removed counter): c0 905, c1 1108, d16 495,
d17 127, d5 8, m2 269, x2 465; 0 elsewhere. Witness: d5 (8 barred step-candidates).

## H21

`h21-v12.toml` (2,116 events) is the replay of fixture `artifacts-h21` (artifacts-small with artifacts-deep's certain
deepening, `sack_take_permille` and `capture_take_permille` 300, the monsters' normal pressure 2000) on conflict
stress geography 17 (span 6000, all races, turbulent), names lettered, written with `h21-v12.expected.txt` (532 lines)
by `p5_11_h21_fixture.py`. It shows every maker kind, deep iron, a claim cited and M1 to M11, M13, M14; **M12 is not
in it** (see the witness table: no searched run has one; 84 candidate runs over 19 stress seeds and eight tunings). The v12
report of the reference equals the engine's on it at both levels, and `history_artifacts_report_test.cpp` passes on it.

## For part 2 (not done here)

1. Goldens, from the references only (`--version 0.31.0` or after making it the default):
   - `p5-11-artifacts.txt`: `p5_11_artifacts_reference.py golden` (fixture lines changed with the shipped data, the
     forge ruling and the new fixture `artifacts-h21`), then `golden --inputs DIR` for the world lines (`artifacts-<seed>-512.db`
     seeds 0 to 9 with artifacts-small, `geo-seed-42-1024.db`).
   - `p3-07-report.txt` with H21 (`no golden lines for h21-v12`), from `p3_report_reference.py golden` after
     `build-inputs` into `out/p511-int2/report-inputs`; then the other reference-made goldens that hash reports or
     timelines (P4-01, P4-04, P4-05, P5-07, P5-08 ...).
2. `p3_report_reference.py`: make 0.31.0 the default (`use_version`), as P5-08's part 2 did; every reference's default
   version to 0.31.0.
3. Stale constants in older tests: generator 0.31.0, schema 15 ("this build reads 15"), 127 streams, kinds to 116,
   report header v12 (`history_report_checks.hpp:526`, `history_report_blocks_test.cpp:120`, `history_report_v2_test`,
   `-v3`, `-v9` ...), the `[stats]` tail and block heads (`history_report_v3_test.cpp:202`, `-v4_test.cpp:110`),
   `ALL_GROUPS` 0x7FF, table and page counts (40 tables, 52 pages), `history_pantheon_reader_test.cpp` R1 (role 8 is
   the smith now), P5-06 and P5-08 role tables, battle causes with acts in conflict goldens.
4. The full fast and slow suites; E1 stays failing until the lead decides it.
5. Known gaps: M12 has no witness in rows anywhere; `artifacts-h21.toml` promises paths that part 2's golden must show.
