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
