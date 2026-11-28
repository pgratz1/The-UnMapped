# P5-11 artifacts: implementation notes (parts A, B, C)

Generator engineer, 2026-11-28, branch `p5/11-impl`. For the engineer of part D, the test writer, the auditor,
calibration and validation. Spec `docs/specs/p5-11-artifacts.md` r1; lead rulings L-1 to L-11 as recommended.

## What is built

**A (data, schema, catalogue).** Data kind `artifact` (`game/data/artifacts/`, the fifteen kinds; schema and checks
`artifact_battle_class`, `artifact_hunt_class`, grammars `artifact_word`, `artifact_noun` in
`src/data/kinds_history.cpp`). Culture field `artifact_forms` (grammar `artifact_form`) with 4.2's forms. Tuning
`history.artifacts` (`game/data/history/artifacts.toml`; checks `artifacts_occasion_kind|role|bearer|classes`).
`history.figures.smith_age_permille`, `smith_age_span_permille`; `history.pantheon.cult_return_years` 400; the deeds
of 4.5 (`craft`, `war`), the cause words in the templates of kinds 100 to 104, the faerie `The Silver Ring of {god}`.
All of it also in `worldgen/tests/data/geo` and `geo-small` (both still equal to `game/data` outside `worldgen/`).
Schema 15 (`db/schema.hpp`: three DDL statements, 40 tables; rows, writer, readers `read_artifact_kind_rows`,
`read_artifact_rows`, `read_artifact_holding_rows`; `HistoryResult::artifact_kinds|artifacts|artifact_holdings`),
generator 0.31.0, 127 streams, kinds 111 to 116 (`event_catalogue.cpp`), `EntityType::artifact` = 10, name kind
`artifact` (`hist.names.artifact`), figure role 8 `smith` (role word, reader range 1 to 8), templates of 8.2 in the
three `report.toml`.

**B (`src/history/artifacts.cpp`, `artifacts_moves.cpp`, `artifacts_internal.hpp`, header `history/artifacts.hpp`).**
`ArtifactsSubsystem`: T3, T4, T6, Q4, last in `subsystems`; per event of the log in id order the moves of 6.7 (M1 to
M14) then the making of 6.3 (eligibility, forge, class, kind, maker, name, `artifact_made`, smith, first holding);
holdings with incremental holder indexes (`std::map` holder id to ascending artifact ids); quiet moves and the Q4
budget (6.8); the seat snapshot refreshed at the end of each step; `ArtifactView`; the three row sets.
`ArtifactsStats` (not stored).

**C (seams).** Conflict: win factor with `battle_permille`, strain factor with `claim_permille`, the declaration's
claim cause, battle causes (`battle_artifact`, 12.1's `force_act` of winner and loser), causes sorted and distinct.
Monsters: hunts with `hunt_permille` (strength) and `hoard_motive_permille` (both terms of the min);
`site_brief`, `monster_alive`, `lair_of`, `set_artifacts`. Figures: `raise_maker` (role 8, smith ages, name, life),
`brief`, `caster_site`; a smith dies at its `artifact_made`'s region; no epithet. Alliances: `forge_site`. Pantheon:
`force_act`; 12.2's bar (`suppressed_` (cult, civ) to its last suppression year; `PantheonStats::cult_returns_barred`).
Readers, minimum only: `HistoryView` loads `artifact_kind` and `artifact` (names, the `kind` form text), entity type
10 in `entity_type_from_code`, `name()`, the view's participant check; template form `kind` on artifact roles and
`titled` on optional roles (empty renders nothing). `report --history` runs at both levels on 0.31.0 worlds.

## Readings (spec silent or contradictory)

1. **Forge** (6.4): `forge(c)` = c when it has an open mine (`AllianceView::forged`), else the partner with an open
   mine and the largest `forge_depth`, then the smaller id. The spec's `forge_depth(c) >= 1` would exclude a fresh
   depth-0 mine, which contradicts its own "(an open mine)" and invariant 3 ("had no open mine").
2. **`occasion_event`** = the occasion e. For a deep artifact the cause list is (delving_deepened, e) ascending, so
   "its first cause" would be the delving; the reader check of 9 should read "is a cause" (part D).
3. **Region with a settlement**: an artifact event that names a settlement takes that settlement's region (engine W7):
   `artifact_made` at a mine or library whose cell lies across a border, `artifact_hoarded` `how` 2 and 3 (region of
   the raided or lost seat, not the lair's).
4. **Staff first holding**: kept by c when the caster already bears a staff (one per class, 6.2).
5. **M12**: a seat that stands but has no living owner other than c is treated as a ruin (lost in t, holder type 4).
6. **Holding years**: a written move opens in Y (its event's year); a quiet move in the year of its triggering event.
   A move to an earlier year than the open holding is fatal (`history: artifacts: ... moved in year ...`).
7. **M10** also needs the captor alive (as M9; no draw is moved).
8. **M5**: `figure` is the hero whenever the event is `monster_slain_by_hero`, also when the item goes to the realm.
   **M13** (`how` 2) names no figure.
9. **M14**: the temple is any enshrining site of kind 6 whose `ending` is 8 and `ending_event` is this suppression.
10. **12.1**: the battle's year is the step's year; 12.2's bar is tested after the cult's partner is found and before
    the Q4 budget; the count is kept in `PantheonStats` only (the report stat must be rebuilt from the log).
11. New view reads are virtual with neutral defaults, not pure (`FigureView::brief`, `caster_site`,
    `AllianceView::forge_site`, `DivineView::force_act`), so test fakes compile (as P5-08 reading 9).
12. A making by a smith with no figures seam set is skipped after its draws (test seam only).

## Left for part D

- Reader checks of 9 in both readers (one function, as `pantheon_tables_problem`): `artifact_kind`, `artifact`,
  `artifact_holding`, role 8's `start_event`, values of 111 to 116, the participant rules; the digest
  (`digest_tables.cpp`) does not read the three tables yet; `inspect/history.cpp` SHAPES lacks them.
- `history_entity_tag` `ar` (the report prints `artifact=?1`), report v12 (header, `[artifacts]`, realm, figure and
  monster lines, `[stats]`, companions of 8.3), `[figures]` counts role 8 (the `figures:` line counts 1 to 7 only).
- Timeline, browser group 10, markers glyph 24, `artifact_state_word`, `artifact_place_at`; `schema.md`.
- The empty world is **52 pages** (measured, `--until none`; spec 9: 52 or 53).

## For the determinism auditor

- Draw order in `try_make`: eligibility (pure), `make` (e, c), `class` (e, c), `kind` (a); the name (registry) before
  the append, the smith (`hist.names.person`, `hist.figures.life`) after it. Moves: the row's tests, then `take`
  (e, a), `bury` (a, f), `raid` (e).
- Every row iterates a copy of the holder's id list (`held_by`) or ids up to a count taken before the loop.
- Holder indexes are ordered maps updated on each move; no unordered container; `seat_` refreshed at the end of
  every step for alive civilizations ascending.
- Conflict's cause lists: appended, then `stable_sort` and `unique` (declaration, battles).
- Products: `chance x forge_chance` (1000 x 4000), the win factor (force x (1000 + skill + 150)), hunts
  (strength x (1000 + might + 200)), strain (rate x (1000 + 1000)), all through `checked_mul`.

## Measurements (1024, `game/data`; gcc seeds 0 to 15; clang seeds 0 to 7 byte-identical by `hash`, summaries equal)

| | value |
| --- | --- |
| artifacts a world (min / median / max) | 6 / 8.5 / 18 (AR1 median 8 to 20: inside) |
| by class (pooled 162) | arms 86, armour 32, regalia 18, relic 24, staff 2 |
| by maker | smith 89, commission 38, caster 2, god 33 (smith-made 78%; dwarven maker realm 53 of 127; commissions 30%) |
| holdings per artifact (min / median / max) | 1 / 2 / 12 |
| state at E | kept 83, borne 16, hoarded 4, lost 14, buried 23, enshrined 22 |
| events 111 to 116 pooled | made 162, taken 24, lost 14, hoarded 6, recovered 8, buried 22 |
| wars median; standing settlements median | 26; 88 |
| summary lines median (v11 text plus the new events) | 661.5 |

Occasions of the 162: war_declared 55, cult_founded 24, hero_rose 18, sacked 17, succession_disputed 15,
settlement_overrun 13, divine_champion 9, deep_woken 8, civ_conquered 3. Quiet moves: a new general taking up kept
arms 84, a bearer's death returning the item 64.

Rules not seen in the 16 worlds: M6 (a hoard keeper dying of age), M7 (raids, 3 per mille), M11 and M12 (taken or lost
with a realm), the staff enshrined at its maker's death. A stress root (every chance 1000, rest 0, cap 500, burial,
battle take and raid take 500; seeds 0 to 9 at 512, 394 artifacts) exercised M7, M11/M12 (`how` 4), M13, M14 to a
sanctum and to the realm, with no violation of invariant 1 or 4 (no kept holding on an ended realm, no borne on a dead
figure, no hoard on a dead monster, no two of a class); M6 and the staff enshrinement still did not occur (no staff was
made there: few casters with towers; hoards rarely outlive their keeper). For calibration: casters (AR2 >= 5%) are at
1%, bounded by living casters with a tower at an occasion.

## Part D (readers, report v12, timeline, browser, markers; generator engineer 2, 2026-11-28)

**Built.** `artifact_tables_problem` (`src/history/artifact_checks.cpp`, `ArtifactTables` in `event_catalogue.hpp`): the
checks of 9 in their order (artifact_kind, artifact, artifact_holding with `status`, figure role 8, values of 111 to
116, the participant rules of 111, type 10 participants), called last by both readers (`HistoryView::load` after the
pantheon checks; the digest's `Checks::monster_tables`). `HistoryView::load` no longer fails on an artifact's kind or
id order itself and defers type 10 participants, so both readers give the same first error. `inspect/history.cpp`
SHAPES gains the three tables (rows 24 to 26); `history::HistoryTables` gains `artifact_kinds`, `artifacts`,
`artifact_holdings`; `history_reads_disagree` compares the artifact and kind rows. Report v12
(`HISTORY_REPORT_HEADER_V12`, `report --help`): prefix `ar`, `[artifacts]` after `[cults]` (summary
`SUMMARY_ARTIFACTS_MAX` 8 by holdings, then id), the realm `artifacts:` line (with the gods, faith and deep lines,
same summary rule), `smiths <n>` in the `figures:` line, `; bore ar..` and a smith's `; made ar..` on full figure lines,
`, hoard ar..` on monster lines, 25 stats, companions of 8.3 and the candidate rule of 8.2. Timeline
`ArtifactPlace`, `artifact_at`, `artifacts_held`, `artifacts_placed`, `artifact_count`. Browser group `artifacts` = 10
(`ALL_GROUPS` 0x7FF), `artifact_block`, artifacts in `search` after figures. Markers `GLYPH_ARTIFACT` 24 (a four-pointed
star), `MARKER_ARTIFACT`, `MarkerOptions::artifacts`, `YearMarkers::artifacts` (`ArtifactMarker`), drawn last,
`artifact_glyph_offset`, `artifact_state_word`. `docs/specs/schema.md` at version 15.

**Readings.**

1. Rendering (7.6) was already complete from part B (display name, form `kind`, empty `titled`); nothing added.
2. Region tags print `rg<id>` (the report's prefix), where 10 item 2 writes `r<id>`.
3. `artifacts_smiths` and the header's `made by smiths` count `maker_kind` 1 and 2; `artifacts_commissioned` 2.
4. `wars_citing_artifacts`: a `war_declared` with a cause that is the opening event of a kept or borne holding of an
   artifact of the attacker's home held by the defender (or its figure); `claim_event` may be a quiet-move event of any
   kind, so the kind of the cause cannot decide. `battles_with_artifacts`: `battle_led` with an `artifact_made` cause;
   `battles_under_acts`: any battle kind with a `divine_bless` or `divine_curse` cause.
5. `cult_returns_barred` is rebuilt from the log (the count lives only in `PantheonStats`): (cult, realm, step) with a
   suppression in (Y - 400, Y], the cult present at Y, the realm alive at Y without `cult_in`, and an alliance or trade
   partner with `cult_in` in force at Y; steps are every year of a non-quiet phase and each quiet phase's first year;
   in force at Y is since <= Y < until; visibility is not stored, so not tested. The window is the shipped value
   (`history::CULT_RETURN_YEARS` 400; the report reads the world only, as `MIXED_SHARE_MIN`). It is an approximation
   of the engine's count (order within a step is not replayed); the test writer should check it against this
   definition, not against `PantheonStats`, or the lead may drop or restate the stat.
6. Realm `holds` for an ended realm: a kept or borne-by-its-figure holding in force in its `ended_year`.
7. The monster line appends `, hoard ar<id>[, ar<id>]` after the line's other parts (after `hoard <n>`).
8. Artifact markers carry the place of `artifact_at`; the glyph is drawn one glyph cell above a place with a settlement
   (`artifact_glyph_offset`), so the settlement or ruin glyph stays visible. The spec says "at the place".
9. Reader messages: `corrupt table 'artifact_holding': holding <k> of artifact <a>: <reason>` (no `row` key), others
   `corrupt table '<t>': row <k>: <reason>`; a type 10 participant not made by its event:
   `... artifact <a> does not exist`.

**For the viewer engineer (11.2).** `history.timeline.artifact_at(a, Y)`, `artifacts_held(3, civ, Y)` for the
territory hover, `artifacts_placed(Y)`; `MarkerIndex::markers_at` fills `YearMarkers::artifacts` (toggle
`MarkerOptions::artifacts`), `draw_markers` draws glyph 24; `artifact_state_word(state)`; the hover's name and kind
form from `view.name(EntityType::artifact, a)` and `view.artifact_kind_text(a)`; holder names by `view.name`.
`BrowserIndex::artifact_block(history, a)` (two lines with links), group `EventGroup::artifacts`, `search` returns type
10 matches.

**For the auditor.** `artifact_checks.cpp` is linear apart from a binary search per id; it reads spans only. The
digest's `artifact_stats` sorts a vector of `Claim` with a defaulted `<=>` (total order) and uses `binary_search`;
`cult_returns_barred` iterates sorted vectors only. Timeline holdings are indexed by offsets checked in `build`.
No floating point, no unordered container; report text through `num` and `escape_history_text`.

**Measured** (1024, `game/data`, gcc seeds 0 to 15, clang 0 to 7): world files and both report levels byte-identical
on the 8; `report --history` ran at both levels on all 16; no `=?` left; summary lines min 577, median 674, max 772
(T18 750). Stats per world (gcc, seeds in glob order): artifacts 6 to 18; changed hands 0 to 7; lost at end 0 to 7;
wars citing artifacts 0 or 1; battles with artifacts 2 to 25; battles under acts 0 to 8; cult returns barred 0 to
1717 (counted per step).

Sample `[artifacts]` at summary (seed 3):

```
[artifacts]
artifacts: 6 (kept 3, borne 0, hoarded 0, lost 0, buried 0, enshrined 3); made by smiths 3 (for other realms 0), casters 0, gods 3; of deep iron 0; changed hands 1
  ar3 The Silver Sword of Cocann, a sword: made 3433 e237 by f18 Elindvie of c1 The House of Roelaele for c1 The House of Roelaele; kept by c3 The Sneegg Horde since 4218 e321
  ar1 The Black Idol of Greekotch, an idol: made 2648 e80 by g15 Alickwear at si3 The Bone-House of Alickwear for c3 The Sneegg Horde; at si3 The Bone-House of Alickwear since 2648 e80
  ar2 The Crown of Koum, a crown: made 3363 e211 by f14 Tudinba of c4 The Dommtom Throne at si8 The Delvings of Birin Deep for c4 The Dommtom Throne; kept by c4 The Dommtom Throne since 3363 e211
  ar4 Drutch's Sword, a sword: made 5653 e738 by f45 Kreeb of c3 The Sneegg Horde for c3 The Sneegg Horde; kept by c3 The Sneegg Horde since 5653 e738
  ar5 The Black Chalice of Kizz, a chalice: made 5819 e824 by g14 Briltick at si26 The Bone-House of Briltick for c3 The Sneegg Horde; at si26 The Bone-House of Briltick since 5819 e824
  ar6 Seld, a reliquary: made 5829 e828 by g5 Bulwan at si27 The Temple of Bulwan for c7 The Kingdom of Tead; at si27 The Temple of Bulwan since 5829 e828
```

Sample event lines (chronicle, refs cut):

```
2648 e80 artifact_made/45: In answer to a cult founded, Alickwear the Hunter in the Green bestowed the Black Idol of Greekotch, an idol, upon the Sneegg Horde.
4218 e321 artifact_taken/40: After the fall of its bearer, the Sneegg Horde took the Silver Sword of Cocann, a sword, from the House of Roelaele.
5737 e1469 artifact_lost/35: After the fall of its bearer, the Helm of Brairn, a helm, was lost to the Realm of Ienngleth in Zigg's Pyre.
5983 e815 artifact_hoarded/50: After the fall of a hold, Thomkhar the Deep-Coiled carried Gourin, a sword of deep iron, off from Clan Drourd to its hoard.
5501 e1504 artifact_recovered/50: After a war won, the Warrens of Geeb-Kreek recovered the Black Hammer of Slizskrab, a hammer, from the Bikigg Mob.
```

**Lead rulings on part D.** `cult_returns_barred` is dropped from the report, the spec's stat list and 12.2, and the
engine's `PantheonStats` counter is removed (nothing read it); reading 5 above no longer applies. Reading 4's
definition of `wars_citing_artifacts` is written into spec 10 item 6; readings 2 (`rg<id>`) and 8 (the marker one glyph
cell above a settlement) stand and are written into spec 10 item 2 and 11.1.

## Part I (r2 corruption; generator engineer, 2026-11-28, branch `p5/11-spec2`)

Spec r2 6.11, 7.7, 8.4, 9 (kind 117), 10 item 8; lead rulings L-12 to L-17 as recommended. Schema stays 15,
generator stays 0.31.0 (the stage is not merged).

**Built.** Stream 128 `hist.artifacts.corrupt` (`core/streams.hpp`, 128 names). `history.artifacts` gains
`corrupt_years` 25, `corrupt_permille` 150, `corrupt_camp_permille` {good 1000, neutral 1000, evil 600},
`corrupt_warlike_permille` 500 (schema in `src/data/kinds_history.cpp`, `ArtifactsTuning`, shipped data); the deed
enum of `history.figures` gains `corrupted` (index 12) and the shipped epithets gain entry 3
`{ruler, corrupted, 1, 0, "the Dark"}`; `report.toml` gains template 117. The three files are copied unchanged into
`worldgen/tests/data/geo` and `geo-small` (both still equal to `game/data` for them). Kind 117 in
`event_catalogue.cpp` (roles artifact, civilization, god 1-1; value `temper`). `FigureView::ReignBrief` and
`reign_brief` (default `{}`); `FiguresSubsystem::reign_brief`, `corrupt_reign`, `lift_corruption`, the reign fields
`corrupt_temper` (-1), `corrupt_event`, `corrupt_artifact`, `Tally::corrupted` (was_corrupted, moved to the figure
with the reign's tallies), the private `tau(const Line&)` read by `strain_permille`, `allied_strain_permille`,
`spoils_permille` and `temper`. The pass is `src/history/artifacts_corrupt.cpp`; `Holding::periods`. Conflict
(`conflict.cpp`, the one `war_declared` append; the spec names `conflict_war.cpp`) cites
`reign_brief(a).corrupt_event`. Reader checks of 117 at the end of `artifact_tables_problem` (`artifact_checks.cpp`).
Report: the `[artifacts]` count line, `; evil[, corrupted <n>]` on an artifact line (so also in the browser's
`artifact_block`), the full `    corrupted: ...` line after `held:`, the realm part `corrupted by`, the ruler deed
`corrupted to <temper word> by ar<id> <name>`, five stats (digest). Browser: 117 in group 10. `schema.md`: row 117.

**Order of the pass as built** (the reference mirrors it). `ArtifactsSubsystem::run`: read the log (moves, makings),
refresh the seats, then `corruption`:

1. Nothing when no figures seam is set.
2. Lifting (every step T3, T4, T6, Q4): c = 1 .. civ count ascending, alive only (not ended; hidden realms count);
   a = `reign_brief(c).corrupt_artifact`; when a != 0 and a's open holding is not (state 1, holder c),
   `lift_corruption(c)`.
3. Draws (T3 and Q4 only): c ascending, alive, `reign_brief(c).number` != 0; the ids kept by c ascending (a copy
   of the index taken before the loop); evil ones only; for j = periods + 1, ... while t = from + j * T <= Y:
   corrupted now -> periods = j, next j; Q4 and `quiet_events_left(c)` <= 0 -> stop c (all its artifacts, periods
   unchanged); p = `corrupt_permille` * camp factor div 1000; u = pick(draw(key, t, a, c, j), 1000); periods = j;
   u < p -> temper 1 when u < p * warlike div 1000, else 3; append 117 (year Y, region and settlement of `seat(c)`,
   else home region and 0; causes holding event and `made_event` ascending, distinct); `corrupt_reign`, which names
   an unnamed reign by the deed (ruled row opened by the event 117) when `name_rulers`.

**Readings.**

1. `D_at(t_j)` is `history::draw(key, word(t_j), word(a), word(c), word(j))` called directly: `StepContext::draw_at`
   refuses a year before the step, and t_j is often before Y (a period that fell in a quiet step's (Y, Z] or between
   T3 passes is evaluated later). Same hash as `draw_at` would give.
2. "Alive" in both passes is `civ_alive` (not ended), as the spec writes; no visibility test (an artifact is never
   kept by a realm created in the step).
3. God and race camps come from `HistoryInputs::gods` and `races` (chronology wrote every god), so no pantheon seam.
4. The lifting count and the periods evaluated are in `ArtifactsStats` only (`lifted`, `periods`, `corrupted`,
   `corrupted_warlike`).
5. The digest's `rulers_by_deed` (6.4 rule 3) is unchanged: it never counted P5-07's or P5-08's naming kinds either;
   adding 117 would be a stat change the spec does not state.
6. Report deed placement: after `a blight`, before `; bore` (both levels, as the other deeds). The count line's
   `; evil <v>, rulers corrupted <r>` is printed always (also 0).
7. The reader check "kept by c in year": the last holding of state 1 by c with from <= year <= to (or open) is the
   one whose event must be cited.

**For the auditor.** `artifacts_corrupt.cpp` holds no reference across `ctx.append`: the holding is re-read by
index (`artifact(a).holdings[hi]`) after each append; the pass moves nothing, so the index is stable. Products
`corrupt_permille x camp` (<= 10^6), `p x warlike` (<= 10^6) and `j x T` through `checked_mul`. Order: civilizations
ascending, `held_by(KEPT, c)` copied (ascending), periods ascending. `tau()` is the only place the override enters.
I1: with `corrupt_permille` 0, p = 0 and u < 0 never holds, so no event, no naming, no lift (nothing corrupted); the
draws are still made but are pure. Resume: `periods` lives in the holdings, rebuilt by the replay.

**Measured** (1024, `game/data`, gcc seeds 0 to 39, clang 0 to 7): world files and both report levels
byte-identical on the 8; no generate or report error; no `=?`. Wars median 25.5, artifacts median 14. Evil artifacts:
5 of 41 god-given (in 4 worlds: 9, 12, 23, 30); a kept evil holding in 2 worlds only (12 from 5715, 23 from 5971;
the others are enshrined at a temple, state 6, which corrupts no one). **No `artifact_corrupted` in the 40 worlds**:
not a defect but the pool (CR0 far below L-13's 25 worlds): seed 12 has 11 periods at p = 90 (goblins, camp evil:
150 x 600 / 1000), seed 23 one period. Check with `corrupt_permille` 1000 (a scratch copy of `game/data`, seeds 12
and 23): 5 events in seed 12 (tempers 3, 1, 3, 3, 3; one war cites a corruption; `the Dark` on two rulers, the third
keeps no epithet by `epithet_repeat_max`, a fourth keeps `the Faithless`), 1 in seed 23 (in force at E), every
reader check passed (both report levels ran). Samples (forced run):

```
5765 e1722 artifact_corrupted/50: Driz, a spear of Kraalzo the Red Banner, turned the heart of the Warrens of Geeb-Kreek under Greegkink the Dark. [causes e1622, e1644]
5815 e1798 artifact_corrupted/50: Driz, a spear of Kraalzo the Red Banner, turned the heart of the Warrens of Geeb-Kreek under Neenk the Dark.
5996 e1547 artifact_corrupted/50: Dreekat, an axe of Drognob the Burner of Halls, turned the heart of the Krazz-Beenk Horde under Grutch the Dark.
5943 e1968 war_declared/70: The Warrens of Geeb-Kreek under Snirk the Faithless declared war on the Gnotkit Mob over the Thorned Grove. (Claimed since 5836.) [causes e1964, e1967]
```

For calibration: the limit is the pool, not the chance. Evil god-given artifacts are about 0.12 a world and most are
relics enshrined at the god's temple (never kept); only arms and regalia given by an evil god and later taken into a
realm's keeping (or kept from the first holding) are exposed. Raising `corrupt_permille` cannot reach CR0; L-13's
floor or a wider "evil" (a new rule) is the lead's call.

## Part M (r3 and r4; generator engineer, 2026-11-28, branch `p5/11-spec2`)

Spec r4 4.3, 4.4a, 6.1, 6.12, 6.13, 7.8, 7.9, 8.5, 9 (118, 117 by faith), 10 items 9 and 10, 11.1; lead rulings
L-18 to L-27 as recommended. Schema stays 15, generator stays 0.31.0.

**Built.** Stream 129 `hist.artifacts.corrupt_bearer` (129 names). `history.artifacts` gains `corrupt_bearer_years`
10, `corrupt_bearer_permille` 300, `corrupt_sack_permille` 500 (required, after `corrupt_warlike_permille`) and the
occasion field `god_camp` (optional enum any/good/neutral/evil, default `any`) with the check
`artifacts_occasion_god_camp` (last of the occasion checks). Shipped rows 13 (`divine_bless.civilization`, 600,
realm, 400/300/300/0/0, evil) and 14 (`cult_adopted.civilization`, 500, site, relic 1000, evil) appended after
`book_written`; the two cause words in `artifact_made`; template 118; epithets `the Butcher` (first general entry)
and `the Forsworn` (first hero entry). The three data files are copied unchanged into `worldgen/tests/data/geo` and
`geo-small`. Kind 118 in `event_catalogue.cpp` (artifact 1-1, figure 1-1, civilization 1-1, god 1-1; value `role`;
region 1, settlement 0). `ArtifactView::bearer_corruption` and `corrupt_sack_permille()` (both with defaults 0);
`DivineView::Faith`, `faith`, `faction_god`, `temple_faction` (defaults); `PantheonSubsystem` implements them
(`faith_since_`, a new member beside `faith_of_`, holds the faith's year, set and erased with it);
`stages/history.cpp` calls `artifacts.set_divine(&pantheon)`. `FiguresSubsystem::corrupt_bearer`,
`Figure::bearer_corrupted`, deed 12 for roles 3 and 4. M15 is `on_hero_rose` in `artifacts_moves.cpp`. The rest is
in `artifacts_corrupt.cpp`. Conflict's sack term and cause in `conflict_war.cpp` (`fall`). F28's allowance in
`figure_tables_problem`; the 118 checks and 117's faith allowance in `artifact_checks.cpp`; `ArtifactTables`
gains `relations` (both readers set it). Report, digest (eight stats), browser (118 in group 10), `schema.md`.

**Order as built.** `ArtifactsSubsystem::run`: the log (for each event ascending: moves, M15 among them for
`hero_rose`, then the making, with the `god_camp` filter after the Q4 budget test and before the stream-121 draw),
the seats, then `corruption`:

1. Nothing without the figures seam.
2. Lifting (T3, T4, T6, Q4): c = 1 .. n alive ascending; a = `reign_brief(c).corrupt_artifact`; lift when a != 0
   and a no longer exposes c (`exposes`: kept by c, or the faith exposure below). Then every entry of the exposure
   map ((artifact, civ) order) whose realm has ended or whose faith exposure no longer holds is erased.
3. Rulers' draws (T3, Q4): c ascending, alive, with a reign; a = 1 .. n ascending, evil only; when kept by c: r2's
   `corrupt_periods` unchanged; else when `faith_exposure(c, a)`: `faith_periods`. A Q4 without budget stops c.
   `faith_periods`: origin = max(open holding `from_year`, faith `since`); a missing entry or a changed origin
   resets the entry to (origin, 0); for j = periods + 1, ...: t = origin + j x `corrupt_years`; t > Y stops; a
   corruption in force sets periods = j with no draw; Q4 without budget stops c; p as r2;
   u = pick(draw(key128, t, a, c, 1,000,000 + j), 1000); periods = j; u < p: event 117 with the third cause (the
   faith's `cult_adopted`), causes ascending and distinct.
4. Bearers' draws (T3, Q4, after 3): a = 1 .. n ascending, evil, open holding state 2; f the holder; skip unless
   `brief(f)` alive with role 3 or 4 and f not in `bearer_corrupt_`; c = `brief(f).civ`; for j = periods + 1, ...:
   t = from + j x `corrupt_bearer_years`; t > Y stops; Q4 and `quiet_events_left(c)` <= 0 stops this artifact
   (periods unchanged); u = pick(draw(key129, t, a, f, j), 1000); periods = j; u < `corrupt_bearer_permille`:
   append 118 (region `seat(c)`'s when c is alive and has a seat, else c's home region; settlement 0; causes the
   holding's event and `made_event`), `bearer_corrupt_[f] = (e, a)`, `figures.corrupt_bearer`, next artifact.

**Readings.**

1. The faith exposure needs the temple open (`site_brief` kind 6, `open`) at the pass; `temple_faction(s)` is the
   `Temple` row's cult (also for an ended temple; openness is read from the site). `faith(c).since` is the year of
   the faith's `cult_adopted` (the `faith_of` row's `from_year`).
2. M15 on `hero_rose` runs before the making of the same event (6.1: moves, then making), so a hero who takes up an
   evil arm gets the event's own arms only in another class (else they are kept).
3. A champion (role 6), casters and smiths are never tested; an outlaw (status 2) is skipped by
   `bearer_corrupt_`, as every corrupted figure.
4. `corrupt_bearer` is fatal unless status 1: a bearer in exile is not a case that occurs (claimants bear nothing).
   The outlaw gets `host` 0 and `host_region` = the event's region, so `die` writes `how` 2 there (existing code);
   `exiled` stays false (no `exiled` event; deed 9 untouched).
5. `bearer_corruption(g)` is read live by conflict's `fall` through `figures.general_of(w, x)`; the sack cause is
   appended to `{siege_event}` and sorted.
6. Report: the count line prints `, bearers corrupted <b>` always (also 0); one list of 117 and 118 in event id
   order feeds the artifact mark, the `corrupted:` line and the realm part (the realm of a 118 is its
   `civilization` participant); the general's or hero's deed `corrupted by ar<id> <name>` follows r2's ruler deed.
7. `evil_taken_up_by_heroes` counts holdings of state 2 whose event is a `hero_rose`; `artifacts_dark_gifts` and
   `artifacts_faith_relics` count artifacts by their `occasion_event`'s kind; `rulers_corrupted_by_faith` counts
   events 117 citing a `cult_adopted`.
8. Reader check, 117 by faith: the state 6 holding counts when its site exists, `ended_year` is NULL or >= the
   year, the site's `origin_event` has a `faction` participant k and a `god` participant equal to the artifact's
   god, and a `faith_of` relation (faction k, civ c) has `since` <= year and `until` NULL or >= year. The god is
   read from the origin event (both `cult_founded` and `cult_adopted` carry it), so no faction rows are needed.

**What the test writer must mirror.** The order above; key offsets; the origin reset; the exposure map erased at
every lifting (T4 and T6 too); M15 only when `corrupt_bearer_permille` > 0 and only evil kept arms, arms first;
the bearer test skips the dead, roles other than 3 and 4, and figures already corrupted; the Q4 budget is checked
per period before the draw. The `artifacts-small` fixture (tests) needs the r3 values of 4.6 (`corrupt_bearer_years`
2, `corrupt_bearer_permille` 1000, `corrupt_sack_permille` 1000) and every hand-made `history.artifacts` record in
`worldgen/tests/data/artifacts/` needs the three new required fields: not done here (test files).

**For the auditor.** No reference is held across `ctx.append`: the bearer pass re-reads the holding by index; the
faith pass re-finds its map entry with `at(key)` after each append (the map is not changed by the append, but the
entry is re-read anyway). The exposure map is a `std::map` keyed (artifact, civ), erased in key order. Products
`j x T` and `j x Tb` through `checked_mul`; the faith key is `checked_add(1,000,000, j)` with a fatal bound at j >=
1,000,000. `temple_faction` is a linear search over `temples_` (no order assumed). The pantheon reads are const.
I2 with `corrupt_bearer_permille` 0: M15 returns at once, stream 129 is drawn but no event follows, so
`bearer_corruption` is 0 and the sack chance and causes are unchanged.

**Reading 9 (a defect of r2's pass, fixed here).** In Q4 the figures run their lines through the quiet step's
years before the artifacts' pass, so `reign_brief(c)` may name a reign that began after Y; an event 117 dated Y
then names (by the deed) a ruler whose `office_from` is after its `start_event`, and both readers refuse the world
(`office_from ... after the year of start_event`; 7 of 40 default worlds once the faith exposures and gifts raised
the pool). The rulers' draws now skip c in Q4 when `reign_brief(c).start > Y`; its periods wait for the next pass
(the keyed draw gives the same result there). The spec should say so in 6.11's Q4 paragraph (the reference must
mirror it).

**Measured** (1024, `game/data`, gcc seeds 0 to 39, clang 0 to 7): world files and both report levels
byte-identical on the 8; no generate or report error; no `=?`. Evil artifacts 19 in 14 worlds; dark gifts 10 (row
13, 9 worlds), faith relics 4 (row 14, 4 worlds); a realm exposed in 13 worlds (kept 13, by faith 2); events 117:
48 in 12 worlds (kept 46 in 11, faith 2 in 1); evil arms borne by a general or hero: 2 worlds (both M1; no M5, no
M15); events 118: 0; sacks citing a corruption 0; outlaws 0. Artifacts median 14, max 31; god-made 8.0%; wars
median 25.5; standing settlements median 77. The bearer pool is the limit: an evil god's gifts go to its kin
(goblins, orcs: few generals bear a gift, since M1 takes the smallest-id kept arms first, and few heroes rise:
`hero_permille` 40 and 80). Forced (scratch data: rows 1 to 12 at 0, `realm_rest_years` 0, row 13 all arms at
1000, bearer years 2, chance and sack 1000; seeds 8 to 27): 8 generals corrupted in 5 worlds, 4 sacks citing them;
with `general_scale_permille` 0 and every race's `hero_permille` 1000: 9 M15 take-ups and 9 heroes outlawed in 5
worlds (each died later `how` 2, `in exile`), both report levels pass the reader checks.

I2: gcc seeds 9, 12, 23, 30 with a scratch root (rows 13 and 14 removed, `corrupt_bearer_permille` 0,
`corrupt_permille` 0) against the same seeds built at 35c6581 (r2 logic, shipped data): every table equal but
`event_kind`, `event_kind_role`, `event_kind_value` (kind 118) and `meta` (content hashes). With `corrupt_permille`
150 kept, seeds 9, 12, 23 are equal likewise and seed 30 differs: the faith exposure (r4, not r3) corrupts a ruler
there, as intended.

**L-28 (lead, "the dark blade calls").** M1 now takes a kept evil arm or armour of the class before any other
(smallest id among the evil, else smallest id among the rest; `on_general`). M15 was evil-only already; M5 takes
from a hoard and has no choice among kept arms. No draw, no data field.
