# P5-08 integration, part 1: reference against engine, C++ tests on the real API

Test writer, 2026-11-27, `p5/08-int` (calibrated engine with both champion rulings, the tests, main); spec
`docs/specs/p5-08-pantheon-cults.md` r1. Part 2 (a separate agent) follows; its list is at the end.

## Result

- **No engine change.** No file under `worldgen/src`, `tools/worldgen-cli/src` or `game/data` changed.
- `UNMAPPED_P508_API` is on (`worldgen/tests/history/p5_08_api.hpp`). Every P5-08 test file compiles against the
  engine API unchanged: the 11.1 signatures are the spec's.
- Sweep: 36 worlds at 1024, `game/data`. Default seeds 0 to 23 (`d*`); `--divine-activity meddling` and `distant`
  seeds 0 to 3 (`m*`, `x*`); `--monster-pressure high` seeds 0 and 1 (`h*`); `--civs-per-race 4` seeds 0 and 1
  (`c*`). Twenty more default worlds (seeds 24 to 43, `e*`) were generated to find witnesses.
- `p3_conflict_reference.py check-world` replays the history and compares it table by table, then runs every earlier
  invariant (14). `p5_08_pantheon_reference.py check-world` checks 13 and the reader checks of 9 on the stored rows.
  The driver also ran `run_problems`, the replay-only checks (4's pestilence and lair, P5-07's and P5-02's
  invariants).
  - With the spec's readings: 12 of 36 worlds agree table for table.
  - With the three diagnostic switches that replay the engine's defects E1, E2 and E4: 35 of 36 agree table for
    table and pass every invariant. The 36th, `d8`, agrees table for table and fails one invariant through E3, which
    the reference shares.
  - The P5-08 row checks and the reader checks pass on all 36 worlds.
- Report v11 (`p3_report_reference.py --version 0.30.0`, new): equal to the C++ report at both levels on all 36
  sweep worlds and on H20.
- Rounds used: 4 reference-against-engine rounds of the sweep and 3 rounds of the report comparison.

## Disagreements by class

| Class | Count |
| --- | --- |
| Reference or test error (fixed on the test side) | 21 |
| Spec reading settled (spec silent; the engine's behaviour taken and recorded as P-R22 to P-R28) | 7 |
| Engine defect (not fixed; reported) | 5 (E1 to E5) |

### Reference and test errors (fixed)

1. `p3_conflict_reference.py check-world` wanted schema 13. Now 14 under the `pantheon` flag.
2. The chronology hashes used three `god` columns. Now `pchr.columns(t)`: six from 0.30.0, in the conflict, peoples
   and engine references.
3. `KEY_OF` and `keys` lacked kinds 100 to 110 (`KeyError: 100`).
4. P3-06 invariant 1 rejected the `cult_in` and `faith_of` rows (type 7). They are skipped there, because 13 items 6
   and 7 check them.
5. P3-06 invariant 8 accepted only conflict and monster flare-ups. It now also accepts a god's flare-up (the
   pantheon source's own log) and a stir of the deep (12.2, a delving as the trigger).
6. P3-05 invariant 8 did not count a `divine_curse` as a reduction, but a pestilence calls `note_reduction` (6.4).
7. Lead rulings PN5 were not mirrored. Now mirrored:
   - Champion eligibility needs work for a champion: `Pantheon.champion_work`, `Monsters.huntable_by`.
   - `hero_of` returns the realm's champion first.
   - `champion_to_war` runs at the raising, and a waiting side is filled in `vacancies`.
   - 13 item 4 gains a necessary row check for champion work.
8. The reference named a ruler for a cult event whose reign began after the event (P-R22).
9. Kinds 93 and 98 `telling` used the new depth instead of the old one (P-R23).
10. A book and a blight cited the tower's `tower_founded` at a tower taken up (P-R24).
11. The divine force factor was applied to `force` only, not to `local_force` (P-R25).
12. Report: the cult founder and the champion in `gods:` lines need the display name, with epithet (P5-06 10.1).
13. Report: `in` ordering, `faith of` filtering and `in none` (P-R27).
14. Report: the reference had no name for an entity of type 7 in the full relations lines.
15. Report: a temple's or a sanctum's cult as `, of <name> (fa<id>)` (P-R28).
16. Report: 12.1 item 3 (`[monsters]` at summary: awake, then sealed, then the other asleep) was missing from the
    report reference.
17. H20: the stress geographies' `Synthetic <n>` names made names the readers refuse (`monster ... name is not a
    name`), and its map size was 192. The dump now turns the digits into letters and writes map size 512.
18. `history_pantheon_data_test.cpp` held 4.1's and 4.2's starts, which calibration moved. It now holds the shipped
    values, with the starts in comments.
19. `history_pantheon_data_test.cpp` expected the loader to refuse a god whose `name_culture` is not a culture. That
    check runs in `check_history_data`, so `refused()` now runs it too.
20. `history_pantheon_reader_test.cpp`: an `event_value` key reads `row (<event>, <seq>)`. The G7 and F10 cases now
    take any first error, since the participant check may come first.
21. `pantheon-cults.toml` lost the paths `hidden` and `ended suppressed` under the champion rulings. A case was
    added: seed 0, span 2000, four realms a race.

In the reference's `units`, the "12 follow-up starts" check now tests the fields' ranges.

### Readings settled from the engine (recorded in `p5_08_pantheon_reference.READINGS`)

- **P-R22:** a cult event read after a later reign began names no ruler.
- **P-R23:** 12.3's d is P5-07 160's old depth.
- **P-R24:** a book or a blight cites the caster's `start_event` (`tower_founded` or `tower_taken_up`).
- **P-R25:** the divine factor applies wherever 7.5's factors do (force and local force; 13 item 10, "in every
  battle").
- **P-R26:** the realm lines run allies, then gods, faith, deep, then rulers.
- **P-R27:** `in` and `faith of` list the rows open at E or closed by the cult's ending event, by (since, id).
  `in none` is printed when the list is empty; the faith part is left out when empty.
- **P-R28:** a site's cult reads `, of <ref>`; a shrine's `, sacred` or `, profane`.

The report also follows the engine's E-reading 3: `renewals_capped` is not printed, and `deep_stirred` is derived
from the rows.

### Engine defects (not fixed; for the lead)

- **E1 (6.5, 7.1; P3-05 9.1).** `PeoplesSubsystem::grow_years` (`peoples.cpp:486`) multiplies the increment after the
  cap, `min(P + d, cap) - P`. The spec multiplies "the positive growth term" d and then caps (`P' = min(P + d', cap)`).
  - Effect: the two differ when `P + d > cap` and the factor is below 1000 (withering).
  - Evidence: seed `d4`, settlement 14, year 3270. P = 7984, cap 7999, g = -500. The spec gives 7999, the engine
    7991.
  - About 12 of the 36 worlds diverge through it. Switch: `p3_peoples_reference.GROWTH_OF_CAPPED_INCREMENT`.
- **E2 (6.8).** For a creator god, "same when camp(g) = camp(r_a) (a creator: r_a = its race)" compares races. The
  engine (`pantheon_cults.cpp:334`) compares camps.
  - Evidence: seed `d1`, event 1228. Cult fa2 of creator g2 spreads to c11 (another race of its camp) at 5871 in the
    engine; the spec's "near" chance does not draw it.
  - Switch: `p5_08_pantheon_reference.AFFINITY_BY_CAMP`.
  - Lower confidence: the parenthesis is terse. The lead may rule for the engine's reading instead.
- **E3 (6.10; P3-06 invariant 4; 13 item 14).** A realm conquered in T6 of the last year keeps its open `cult_in`: the
  pantheon reads the log only in T4 and Q4, and no step follows.
  - Evidence: seed `d8`. `civ_conquered` e1014 at 5999 leaves `cult_in`(fa1, c11) open from e865.
  - The reference agrees with the engine, since this is a gap in the spec. It needs a rule, such as closing at E or
    an end hook.
- **E4 (6.12 item 4, "entities created in the step are invisible").** The engine spreads a cult in the step of its
  founding (`spreads`, `faiths` take no step guard). The reference's P-R9 does not.
  - Evidence: seed `d13`, e1762 `cult_founded` fa1 and e1763 `cult_spread` fa1 to c13, both at 5886. The same holds
    at `d12` e726/e727 and `m3` e970/e972.
  - Switch: `p5_08_pantheon_reference.NEW_CULT_ACTIVE`.
- **E5 (9, "both readers, one function").** The two readers give different first errors:
  - With a `god` row deleted, `read_history_tables` reports `corrupt table 'god': row 8: id 8 is not 7` and
    `HistoryView::load` reports `corrupt table 'event_participant': row (96, 1): god 7 does not exist`.
  - With a faction id moved, the same happens.
  - With a `cult_spread`'s faction set to 99999, one reports `event: ... before its founding`, the other
    `event_participant: ... does not exist`.
  - `history_pantheon_reader_test.cpp` cases G7, F10 and V3 fail on this (line 82).

## C++ tests on the engine (`worldgen-tests -sf="*history_pantheon_*"`, fast suite)

- 30 cases: 22 passed, 8 failed. After the fixes above, the data cases and the V1 and V2 reader cases pass.
- Left failing:
  - The three reader cases of E5.
  - The golden test's fixture-path case: the golden file `p5-08-pantheon.txt` is stale (part 2).
- The property tests (a, b, c), the unit tests and the H20 report test pass. That includes every line of
  `h20-v11.expected.txt` and the timeline reads. The slow suite was not run (part 2).

## H20 (16 item 11)

- `worldgen/tests/data/pantheon/pantheon-report.toml` is new. It is pantheon-small with:
  - a fast deep and certain aid (`deep_aid_permille` 1000, `deep_aid_hops` 6);
  - more alliances;
  - `caster_ppm` 20000.
- Its seed 16, span 2000, four realms a race, shows all 19 features of `p5_08_h20_fixture.FEATURES`. It is H20's
  source (1,829 events). The features include an aid, a sealing, a counted flare (a god's flare-up whose first-year
  act cites it), a take-up and a tower abandoned.
- The C++ fixture loader (`history_report_fixture.hpp`) reads six-column `god` rows (three columns stay creators) and
  `faction` rows.
- `h20-v11` is added to `hrtest::fixture_names()` and to the report reference's `FIXTURE_NAMES_V11`.
- The reference's fixture world, reported by `worldgen-cli`, equals the reference's v11 text at both levels.

## Witness worlds (1024, `game/data`; `d` default, `m` meddling, `x` distant, `h` monster pressure high, `c` civs 4)

| Path | Seeds |
| --- | --- |
| Every act kind | Bless, curse and mark in 35 or 36 worlds. Champion in 21, e.g. d10, d14, d15, c0, c1. Monster in 27 (d10, d14 and d15 have all five) |
| A god's flare-up with an act (counted flare) | 30 of 36, e.g. d0, d1, d10 |
| A champion appointed general at raising | d10 e744 (cause e743), d12 e1328; 16 worlds |
| A champion's hunt | d22 e794, d3 e710, h0 e1002, c1 e1910 (`monster_slain_by_hero`) |
| A cult founded by a prophet | 33 worlds |
| A cult founded by a caster | 22 worlds |
| A cult founded by a ruler | d1, d2, d10, d11, d14, d18, d5, d8, m0, c1 |
| A cult founded by a champion | c0, c1, d2, d21, d3, d5, m0, m1, x2 |
| A spread | 35 worlds |
| A faith | 36 worlds |
| A suppression into hiding | c0 (fa3, fa4), d1 fa3, d2 fa2 and fa4, d11, d15, h0, m0, x1 |
| A suppression that ends a cult | c1 fa2 e395, d2 fa3 e537, d5 fa4 e1084, m2 fa2 e359 |
| A cult ended by its realm's end | none in 56 worlds at 1024. Shown in the fixtures: pantheon-small stress 5 and pantheon-report seed 16 (H20) |
| A tower taken up | c0 e2737, d1 e944, d13 e186, d15, d6, h1, m2, x1 |
| A tower abandoned | 36 worlds |
| A capped renewal | 31 worlds (replay) |
| A stirred waking | d0 (phase 18 at 5403, trigger e339, waking e450), d14 (e423 at 4578), d2 (e555 at 4746), m0, m2, x0 |

The 20 extra default worlds (seeds 24 to 43) have no cult ended by its realms either (faction ending 2 in none of 56 worlds at 1024: realms that hold a cult rarely all fall). With the three switches they all agree with the engine table for table and pass every invariant, so 55 of 56 replayed worlds agree in all (d8 is E3).

## For part 2 (not done here)

1. Stale embedded data and expected values in the older tests, from the 0.30.0 data:
   - culture forms, the `god` kind, `history.pantheon`, `creator_title`, `deep_stir_permille`, `renew_written_max`,
     `tower_lapse_years`;
   - `p3_01_history_data.hpp` first;
   - three-column `god` rows in the hand-made fixtures, which now meet the creator check.
2. Versions and counts:
   - generator 0.30.0, schema 14 and "this build reads 14";
   - 120 streams, kinds to 110 (test kinds above 110), report header v11;
   - the default `use_version` of every reference moves to 0.30.0 (for example, `p5_07_depths_reference.py units` at
     its default 0.29.0 refuses the 0.30.0 culture forms);
   - the `--divine-script` message;
   - `p5_07_checks.hpp`: the `telling` mod 9 with the old depth (P-R23), the aid after the outcome, the first take's
     causes;
   - the browser and marker goldens: group 9, glyphs 21 to 23.
3. Goldens, from the references only, with the spec's readings (switches off). Inputs `--until regions`:
   - `p5-08-pantheon.txt` via `golden --inputs DIR` (DIR: `pantheon-<s>-512.db`, seeds 0 to 9, and
     `geo-seed-42-1024.db`); the fixture lines changed with this part and `pantheon-report.toml` is new;
   - `p3-07-report.txt` gains `h20-v11`;
   - every other reference-made golden.
   - Until E1, E2 and E4 are fixed or ruled the other way, the world golden lines of the C++ tests will disagree
     where the defects bite.
4. The full fast and slow suites on `linux-gcc-release`, including `-slow-pantheon`.
