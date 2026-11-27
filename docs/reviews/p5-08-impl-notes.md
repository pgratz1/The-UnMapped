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

## Parts D and E (second engineer, 2026-11-27)

**D.** `monsters_deep.cpp`: `mine_candidate`/`warren_candidate` (6.4's test, shared), `deep_stir` (Q4 after the
delvings, streams 119/120, `history.monsters.deep_stir_permille` 1000), the certain waking in T4 of the flare's first
year (`phase_start_` as pantheon's), `deep_wake_at(..., certain)`; tellings `(s + 4d) mod 9` (d the old depth, as
P5-07 160); waking order woken, outcome (citing the waking only), then aids citing (waking, outcome), each with its
credit; `deep_take(..., const DeepFlightPlan*)` returns its `hold_lost` id and uses the gate's plan (A1); later takes
need the waking realm visible (A2). `alliances.cpp`: `renew_written_max` 3, per-row `written_renewals`, a capped
renewal is quiet (units, R unchanged). `figures.cpp`: `take_up_tower` (kind 109), the lapse in `tower_endings`
(kind 110, ending 7, Q4 waits for budget), `tower_lapse_years` 200, the fresh tower's extra cause, `TowerSite` keeps
master and death. Templates 93, 96, 97, 98, `tower_founded` in all three `report.toml`; readers: 93/98 telling 0-8,
97 must cite its outcome, a book may be written at a tower taken up; choices of up to 9 words.
**Switching the stir off:** `deep_stir_permille = 0` in `game/data/history/monsters.toml` (no stir draw, no flare).

**E.** Header v11 (CLI help too); `[pantheon]`, `[cults]` after `[figures]`; realm lines `gods:`, `faith:`, `deep:`
after `allies:` (not at summary above 16 realms); 12.1 F pinning (kinds 94, 95, 96, 99 with units; 97 in the unit of
its outcome); `[monsters]` summary awake, sealed, asleep; `[sites]` counts and lines for kinds 5-7, endings 7, 8;
`[figures]` champions, prophets; 25 stats; `god_block`/`cult_block` (report and `BrowserIndex`), group `gods` = 9,
`ALL_GROUPS` 0x3FF; timeline `faith_at`, `cults_in_at`, `cult_status_at`, `divine_effect_at`; markers glyphs 21-23
(cross, house, diamond), `MarkerOptions::shrines`/`temples`, `shrine_word`; `docs/specs/schema.md` per spec 22.

**Readings.** (1) Ruling 1: `{profane? cursed| hallowed}` and `{hidden?, and its faithful fled into hiding|}`.
(2) The stirring candidate wakes before the other candidates; "still a candidate" is 6.4's test only (a later
deepening does not cancel it). (3) Ruling 3: `renewals_capped` is not printed; `deep_stirred` is printed as the flare
phases whose trigger is a `delving_deepened` or `warren_delved` (only a stir makes one the trigger; a stir whose flare
lost to another source is not counted). (4) Take-up candidates need a dead master (`master_death` != 0). (5) A shrine
line reads `si<id> <name>, sacred: shrine in ...`; a temple's `, of <cult> (fa<id>):`. (6) `in` and `faith of` list
rows open at E or closed by the cult's ending event, by (since, id); `in none` when empty; `faith of` left out when
empty; realm `faith:` lists every row ever, by since. (7) `and <n> more` in `[pantheon]` counts the qualifying gods
past 8 (not all gods); `[cults]` the cults past 6. (8) `divine_flares` is computed in the digest and printed in the
`[pantheon]` head at both levels. (9) Browser: no god/faction filter fields were added to `EventFilter` (viewer work).
(10) Glyph offsets: a shrine at a settlement (+7, +7), a temple (-7, +7), wild shrine and sanctum (0, 0).

**Verification.** GCC and Clang release clean; both lint commands clean. 16 worlds (1024, `game/data`; seeds 0-11,
0-1 meddling, 0-1 distant): world hashes and both reports byte-identical GCC vs Clang. Summary lines: w0 589, w1 692,
w2 654, w3 570, w4 607, w5 701, w6 614, w7 585, w8 684, w9 557, w10 583, w11 656, m0 585, m1 711, d0 563, d1 698
(T18 median <= 700: 609.5).

Excerpts (seed 2, 9, 8 summary):
```
  g12 Civono the Keeper of Oaths (good, oaths): acts 4 (curse 3, mark 1); cult fa3 The Grovellers of Civono
  fa1 The Children of Gagg-Sneegg, of g2 Gagg-Sneegg: founded 4210 e392 by f27 Ithorsa at si21 The White Fane of Gagg-Sneegg in s1 Ciegliel; in c1 The House of Ithasfa; ended 4252 e432 suppressed
  gods: blessed by g13 Kibgank e984; cursed by g2 Gagg-Sneegg e549, g12 Civono e620, g1 Saeriemi e965; champion f19 Geeb Chosen of Stamedock e252, ...
  faith: fa3 The Grovellers of Civono since 5025 e632
  deep: woke m8 Throunmaik the Deep-Coiled e620; sealed m8 Throunmaik the Deep-Coiled in si22 The Coils of Throunmaik e621; aided by c7 The Shywi Court e622
5501 e622 ally_aided_hold/40: Answering its ally's call, the Shywi Court sent warriors from Maeth Hollow to stand with the Hold of Thraidba at the gate of Brormu Deep against Throunmaik the Deep-Coiled, and the gate held.
5903 e1547 tower_taken_up/20: After the death of its last master, Tentgeald of the Bild League became master of Deald's Tower.
```

**For calibration.** `alliance_renewed` still 30 in three of 16 worlds (cap 3 per row; FU2 median 9.5 here); towers
abandoned 1 to 5 a world, taken up 0 or 1 (FU3); `divine_flares` 0 in all 16 worlds; no `divine_monster` and no
hidden cult in these 16; a tower lapses before a new caster can take it up (T3 order: endings, then rises). Stir:
`deep_stirred` 0 or 1 a world. **For the auditor:** the T4 phase tracking in monsters (`phase_seen_`), the gate plan
copied into the take, the order of aid events after the outcome's flight events, `towers_` updates in `die()`.
