# P5-08 pantheon and cults: implementation notes (parts A, B, C)

Generator engineer, 2026-11-27, branch `p5/08-impl`. For the engineer of parts D and E, the test writer, the auditor
and validation. Spec `docs/specs/p5-08-pantheon-cults.md` r1; lead rulings L-1 to L-12 as recommended.

## What is built

**A (data, schema, catalogue).** Data kind `god` (`game/data/gods/`, nine files; schema and checks in
`src/data/kinds_history.cpp`: grammars `god_title`, `god_form`, checks `god_favour_weights`, `god_wrath_weights`,
`god_monster_weight`, `pantheon_deed_kind|role|against|points|foe` against the compiled catalogue; `god_culture`,
`god_monsters` in `src/history/data_checks.cpp`). Tuning `history.pantheon` (`game/data/history/pantheon.toml`, checks
`pantheon_flare_above_act`, `pantheon_act_below_max`). `history.chronology.creator_title`. The five culture forms in each
`naming` record. All of it also in both fixture roots. Chronology C5/C5a (`src/stages/chronology.cpp`). Schema 14:
`god` (domain, title, race), table `faction` (writer, reader `read_faction_rows`, `HistoryResult::factions`), generator
0.30.0, 120 streams, kinds 100 to 110 (`event_catalogue.cpp`), the form `{g:titled}`, templates of 8.2 in `report.toml`.

**B (`src/history/pantheon.cpp`, `pantheon_cults.cpp`, `pantheon_internal.hpp`, header `history/pantheon.hpp`).**
Regard from the log with the fade, the source (pure) and the applier, the five kinds and their effects, the gods'
flare-up as the third tension source, cults (founding, spread, faith, suppression, hiding, realm and temple endings).
`PantheonStats` counts for calibration (not stored).

**C (seams).** `DivineView` set on peoples (growth term, shunned regions) and conflict (`ConflictSubsystem::divine`,
after the alliance factors; `strengthened` gained a year). `InterventionApplier`, `EngineConfig::applier`, the five
supported kinds, target civilization alive and visible (`engine.cpp`). Monsters: `SentMonster`, `prepare_sent`,
`create_sent`, `lair_region_for_sent`, `living_count`; `open_site` takes kinds 3 to 7, `end_site_seam` kinds 3, 4, 6;
sent monsters outside L_world. Figures: `next_figure_id`, `raise_divine`, `champion_of`, `caster_of`, `ruler_figure`;
`hero_of` falls back to the champion; a champion is appointed general with no draw; epithets skip roles 6 and 7;
`cult_adopted`, `cult_suppressed` name the ruler. `AllianceView::partners`. Readers: `pantheon_tables_problem`
(both readers), site kinds 5 to 7 and endings 7, 8, figure roles 6, 7, `divine_monster` births. CLI message.

## Readings (spec silent or contradictory)

1. `prepare_sent` fixes ids, kind, region and cell only; `create_sent` draws the names (same streams and keys as a wild
   appearance or a rising), so the pure call does not touch the name registry.
2. 6.6 `certain`: in the first tick year of a gods' flare-up whose trigger is the flared deed, the flared pair is taken
   as the candidate (no act draw) when it is still a candidate, even if another pair has a larger |R|.
3. Faith needs a standing visible capital (kind 107 has a required settlement). tau = -1 (no reign) gives factor 1000.
4. A pantheon event read by figures after a reign that began later than the event (a quiet step: figures run before
   the pantheon) names no ruler; otherwise `office_from` would follow `start_event` (F-check).
5. A champion slain by a monster while commanding closes its `served` row with the `hero_slain` (reader accepts it).
6. Mark: `e*`'s region, else c's capital (home) region, must hold no shrine at all (shrines never end).
7. Template limit raised to 2048 bytes (kinds 100 to 104 with the full cause-word list are about 1,230).
8. The CLI message is `--divine-script: divine scripts are not supported yet` (settings file likewise).
9. `AllianceView::partners` and the three `FigureView` reads are virtual with neutral defaults (not pure), so test fakes
   still compile. Section 7.2 does not exist in the spec (7.1 then 7.3).
10. The epithet reader check accepts `'` (a god's name such as `Uuld'mam` in `Chosen of ...`).
11. Spread region and suppression region: the realm's capital region, else its home region.

## Left for parts D and E

- D (12.2 to 12.7): monsters' stir (`monsters_deep.cpp`; streams 119, 120 are registered, unused), templates and
  `telling` mod 9 of 93/98 and the hold_lost line (data, readers), aid order, renewal cap (`alliances.cpp`), tower
  take-up and lapse with kinds 109, 110 (`figures.cpp`; kinds, roles and templates exist), A1/A2.
- E: report v11 (`[pantheon]`, `[cults]`, realm `gods:`/`faith:`, `[sites]`, `[figures]` roles, stats), digest,
  12.1, timeline, browser group 9, markers. The report header is still v10; kinds 100 to 110 render through the
  catalogue and both levels run. `inspect/history.cpp` reads `faction` (index 23); `[history]` does not count it.
- Not done: `docs/specs/schema.md` (section 22, the lead's text).

## Stale tests (ctest linux-gcc-release: 43 of 58 entries fail; no crash, hang or timeout)

- Embedded data copies lack `history.pantheon`, `creator_title`, the culture forms and kind `god` (about 10,650
  `missing required field`, 341 `missing definition 'history.pantheon'`); most "unknown exception" lines are this.
- Generator version, content hash, schema 14 and goldens (every world moves).
- Reader expectations the spec changes: site kind 5 and figure role 6 are now valid (old corrupt cases expected
  `kind 5 is not 1, 2, 3 or 4`, `role 6 outside 1 to 5`); hand-made god rows fail the new creator check.
- Compile fixes only (no expectation changed): `GodRow` initializers in `tests/history/p3_04_engine_support.hpp`,
  `p3_05_peoples_support.hpp`, `tests/history_report_fixture.hpp`, `tests/history_report_worlds_test.cpp`.

## Event counts (1024, shipped data; GCC and Clang release byte-identical by `hash --db`, reports equal)

| kind | seeds 0-11 normal | 0-1 meddling | 0-1 distant |
| --- | --- | --- | --- |
| divine_bless / curse / champion | 20 / 33 / 16 | 5 / 7 / 4 | 2 / 10 / 0 |
| divine_monster / mark | 2 / 16 | 0 / 1 | 1 / 6 |
| cult_founded / spread / adopted / suppressed | 6 / 20 / 8 / 1 | 4 / 12 / 1 / 0 | 2 / 14 / 2 / 9 |
| tower_taken_up / abandoned (part D) | 0 / 0 | 0 / 0 | 0 / 0 |

Acts per world 0 to 18 (median about 7); no hidden cult in these 16 worlds (seen in none; the path ran on 512 tests
only through suppression). Meddling does not yet raise acts on seed 0: acts are bound by `god_rest_years`,
`pair_rest_years` and the threshold more than by `act_permille` (PN2, calibration). Seed 1 distant shows a
spread-and-suppress cycle (14 spreads, 9 suppressions of 2 cults).

## For the auditor

Draw order in `interventions` (act, then kind) and in `apply` (monster kind); the regard fade's cumulative formula;
`pairs_` is a `std::map` keyed (god, civ) and dropped by realm the step after its end; `cult_in_`/`faith_of_` mirror
State's rows so no loop runs over `relations()`; `place_name` touches the shared name registry (order: cult name, then
temple name; shrine and sanctum names at their events); `create_sent` names after the event is appended.
