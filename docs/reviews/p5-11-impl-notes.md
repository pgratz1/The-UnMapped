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
