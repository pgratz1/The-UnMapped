# P5-07 mountain depths: implementation notes (parts A, B, D)

Generator engineer, 2026-11-26, branch `p5/07-impl`. For the engineer of part C, the test writer, the auditor and
validation. Spec `docs/specs/p5-07-mountain-depths.md` r1; lead rulings L-1 to L-7 as the spec recommends.

## What is built

**Part A, the deep rules** (`worldgen/src/history/monsters_deep.cpp`, new; `monsters.hpp`, `monsters.cpp`):

- `deepen` (6.2) runs in T4 after the monsters' year and in Q4 after `outposts_left`; `deep_waking` (6.4, 6.5)
  runs in T4 after `deepen` and before the one creation (rise, else a wild appearance); a waking is not a creation.
- The gate (6.5) is drawn once per waking, before any event; a fall of a race's last refuge with no flight plan
  becomes a seal. Sealed: `deep_sealed`, the horror asleep and `sealed`, the mine `sealed`.
- `deep_take` (6.6): toll, `hold_lost`, then `ConflictSubsystem::deep_flight` (last refuge) or
  `PeoplesSubsystem::ruin`. `takes` and `holds_settlement` are set.
- `deep_after_raid` (6.7) replaces the overrun after a raid of a `wake` 3 kind; the fear flight of P5-03 9.4 still
  runs after when the target stands.
- A sealed monster is skipped whole in `monsters_year` (no rewake draw, no raid, no hunt).
- `living()` leaves out `wake` 3 kinds (the world cap); `living_of(kind)` counts every kind. `kind_wake(kind)`.
- `mine_depth(site)` (7.1), `set_conflict` (7.3). Stats (not stored): `deepened`, `deep_woken`, `deep_sealed`,
  `holds_lost`, `deep_flights` in `MonstersStats`.
- The deep kinds load with `wake = "deep"` as code 3; `monster_kind.wake` is written 3.
- `settlement.ending` 2 for a `hold_lost` ruin event in both derivations (`engine.cpp` `ending_of`,
  `event_catalogue.cpp` `settlement_ending_code`).

**Part B, the seams**:

- `AllianceView::forge_depth` (pure virtual) and `AlliancesSubsystem::forge_depth` (the deepest of the realm's
  open mines in `mines_`, sealed ones included, through `MonstersSubsystem::mine_depth`); conflict's force factor
  `1000 + forge_strength_permille + forge_depth_permille x D(c)` when forged (`conflict.cpp` `strengthened`).
- The flight seam: `DeepFlightPlan` (namespace scope, `conflict.hpp`), `deep_flight_plan` (pure) and
  `deep_flight`. Conflict's own flight now calls the same two private helpers, `flight_destination` (20.1's order
  with a list of threat regions: the attacker's presence for conflict, `{lair region}` for the deep) and
  `settle_refuge` (items 2 and 3); its output is meant to be unchanged.
- `RelationsSubsystem::credit_units`; the quiet renewal of P5-04a 6.2 calls it (audit A1).
- Figures: `deep_woken` and `deep_sealed` name the ruler of their `civilization` role (6.9, P5-06 6.4 rule 3), in
  the same branch as the spoils kinds.
- Stage wiring: `monsters.set_conflict(&conflict)` beside `set_figures`.
- Five streams `hist.deep.delve`, `.wake`, `.kind`, `.gate`, `.take` (103 streams); generator 0.29.0; schema 13.

**Part D, data**: `ymrach`, `ysgarth`, `zhakul` in `game/data/monsters/` and both fixture roots
(`worldgen/tests/data/geo`, `geo-small`); the eight `history.monsters` fields with ranges and the check
`monsters_deep_years_ordered`; the monster check `monster_deep_biomes` and the enum value `deep`;
`history.conflict.forge_depth_permille` (start 15, code default 0); renaming rule 8 `deep`; the four new templates and
the four changed ones (`civ_ended`, `region_renamed`, `refugees_arrived`, `ally_refused` motive 5) in
`history.report`, in all three roots. The catalogue rows of kinds 93 to 96 (`event_catalogue.cpp`). `schema.md`:
version 13, the changelog row, the `wake`, `settlement.ending` and site kind 3 notes, catalogue rows 93 to 96.

**Fixtures**: `worldgen/tests/history/p5_07_depths_content.hpp` (header only, compiled by nothing yet):
`depths_small_content()` is monsters-small (P5-02 19.3: the conflict stress content, `wake_rate` x 2000,
`overrun_permille` 1000, `living_permille` 1000) with the deep edits of 4.2; `zero_depths(base)` sets
`deep_delve_permille` 0 for I0. Note that monsters-small's `overrun_permille` 1000 and `wake_rate` x 2000 apply to the
deep kinds too (their weights keep their ratios; later takes become near certain within reach).

## Readings (one way among several)

1. **State in the structs.** Spec 5 lists "members per site and per monster"; they are fields of the private
   `Site` and `Monster` structs (`depth`, `deepen_event`, `woken`, `sealed`; `waking_realm`, `takes`, `sealed`,
   `deep_event`), ids only.
2. **Q4 deepening.** p = min(1000, p x L) is computed, then the budget is tested (no draw when it is spent); the
   event's year and the draw's year are the step's first year `ctx.year()`, as every quiet event of the codebase.
3. **`mining_permille`** is read per race from the race definition; a race without the field counts 0.
4. **The forced seal** (6.5) is decided with the same plan the take would make: survivors = P - P x
   `deep_toll_permille` div 1000, threat region = the mine's region (the lair's). `deep_take` recomputes the plan
   with the same state; a disagreement is a FatalError ("found no flight"), not a silent seal.
5. **The mine participant of `hold_lost`** is the open kind-3 site whose `settlement` is t, smallest id.
6. **Later takes** test `takes(m) <= deep_takes_max` before the draw, with `takes` counting the first take (so at
   most 1 + `deep_takes_max` holds, as 6.7 says). The reach is the hop count from the lair's region (the
   monster's `hops`, the lair never moves).
7. **`deep_flight`** recomputes n from the settlement's population after the toll (= the plan's survivors), so n
   equals the plan's n.
8. **Relations' common-enemy goodwill** reads raids and overruns (P5-03a 3.3); `hold_lost` is not added there
   (spec 6.7 names raids only).
9. **`AllianceView::forge_depth`** is pure virtual as 7.1 says; no other implementation exists in the tree.
10. **The reader relaxations needed for `report --history` to run at all** were made here, minimally:
    `monster_kind.wake` 1 to 3 (message `wake <w> not 1 to 3`), a `deep_woken` accepted as a monster's
    `born_event` and as a lair's `origin_event`, and kind 94 in the list of events that must be a site's origin.
    Everything else of 9's reader checks is part C's.

## Left for part C (not done here)

- The rest of the reader checks of 9 in `monster_tables_problem` (`event_catalogue.cpp`, both readers): a `wake` 3
  kind born only of a `deep_woken` and a `deep_woken` only for a `wake` 3 kind; value ranges of `depth`, `telling`,
  `dead`, `survivors`; the depth chain (`depth <d> without depth <d-1>`, `depth <d> again`); `site <s> is not a mine
  of <c>`; `a second waking of site <s>`; `<kind> without its waking`; `hold_lost <e> did not ruin its settlement`;
  a schema 12 world refused (the version message comes from `SCHEMA_VERSION` already).
- Report v10 (10), digest stats, milestones by kind, `; soured`, `sealed in`, `, depth <d>`, `lost to`.
- The `tagged` segment (12 item 4), timeline (`mine_depth_at`, `sealed_year`, monster state for 94 and 95),
  browser groups (93 `settlements`, 94 to 96 `monsters` in `history_browser.cpp`'s kind table), markers
  (`event_symbol("hold_lost")` = sack, `mine_depth_word`).

## Seams for part C

- Event kinds 93 to 96 with roles and values as 8.1 (`event_catalogue.cpp` after kind 92); templates in
  `history.report`.
- Depth and sealed state are derivable from events only (no column): a mine's depth at Y = the largest `depth` of its
  kind 93 events with year <= Y; sealed = a kind 95 naming the site (role `site`) or the monster.
- `MonstersSubsystem::mine_depth`, `kind_wake`, `stats()` (deep counters) for tests.
- `monster_kind.wake` 3 marks a deep kind; a deep monster's `born_event` is its `deep_woken`; its lair's
  `origin_event` is the same event; a sealed horror has status 2 with `deep_sealed` as its last sleeping event.
