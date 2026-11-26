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

## Runs (linux-gcc-release, shipped data, 1024, seeds 0 to 19)

`generate` and `report --history` at both levels run without error on all twenty seeds and on the sixteen cases of
`world-hashes.sh generate linux-gcc-release`; seed 0 generated twice gives the same file hash. Over the twenty
worlds: 134 deepenings (38 mines reach depth 3, in 19 worlds), 17 wakings (in 11 worlds), 8 sealed, 13 holds lost
(in 9 worlds), 0 realms ended by the deep, 0 deep flights, 14 horrors alive at E, 3 slain. With hurried data
(`deep_years` 5, 10, 20, delve 1000, wake 0, 200000, 600000, takes 3) at 512 the flight fired (4 in seeds 0 to 5);
with `--civs-per-race 3` as well, a realm ended by the deep (seed 0..8: e.g. "After the loss of its last hold to the
deep, Clan Drailimai under Hamgri the Last came to an end."). The shipped values were not changed.

## Existing tests (ctest linux-gcc-release)

31 of 45 entries fail. Causes, none a crash, invariant or reader error found:

- `worldgen/tests/p3_01_history_data.hpp` embeds a copy of the shipped `history` and `monster` files at 0.28.0
  (`history_data::shipped_files()`): it lacks the new required fields (`forge_depth_permille`, the eight `deep_*`)
  and the three deep kinds, so every test that loads it fails to load (about 2,300 `missing required field` lines;
  the engine tests' "unknown exception" and `data_history_kinds_test`'s path checks are this). Updating it first
  will unmask the rest.
- Golden values and expected counts at 0.28.0 (world hashes, table hashes, report hashes, stats lines, history
  notes, `cat.kinds.size() == 92U`, the stream count).

# Part C: readers, report v10, timeline, browser, markers

Generator engineer (part C), 2026-11-26, same branch. No world byte changes: part C is readers and text only
(world file hashes of seeds 0 to 5 at 1024 equal those of cd9dce1).

## What is built

- **Reader checks of 9** (`event_catalogue.cpp`, `monster_tables_problem`, which both readers call, so both give the
  same first error): in the `monster` rows, a kind with `wake` 3 is born only of a `deep_woken` naming it in its year
  (`born_event <e> is not the waking of this monster`), and a kind with `wake` 1 or 2 no longer accepts a
  `deep_woken` (the old appearance message). After P5-04 9.1's checks, the new `deep_tables_problem`: the values
  `depth` (1 to 3, kinds 93 and 94), `telling` (0 to 2, 93 to 96), `dead` and `survivors` (0 to its maximum, 96) in
  P5-03 16.4's form; then per event in id order `site <s> is not a mine of <c>` (93, 94), `depth <d> again`,
  `depth <d> without depth <d-1>`, `a second waking of site <s>`, `<kind> without its waking` (every 95 and the
  first 96 of a monster must cite the monster's `deep_woken`), `hold_lost <e> did not ruin its settlement`. All on
  `corrupt table 'event': row <e>: `. Part A's relaxations (wake 1 to 3, a lair's origin 94, 94 must be a site's
  origin) stay where they were.
- **Report v10** (`history_report.cpp`, `digest.cpp`, `cli.cpp` help): header `# worldgen history v10`
  (`HISTORY_REPORT_HEADER_V10`); `[monsters]` `sealed in si<id> <name>` for a status 2 monster named by a
  `deep_sealed`; `[sites]` (full) a mine's `, depth <d>` and `, sealed <year> e<id>` after its settlement part;
  `[present]` ruins `lost to <monster ref>` for a `hold_lost` ruin; `[alliances]` `; soured e<id>` after the spoils
  part; summary milestones at most one per kind (`summary_milestones`); eight stats appended after P5-04a's, in 10
  item 7's order (`deep_stats`).
- **`tagged` segment** (`SegmentKind::tagged = 6`): report text `<tag> <escaped name>` (also with an empty name,
  as before); display `<tag> <name>` with one link over the whole text, `tagged = false`. Both `allies:` forms (summary
  `; at the end:` list and full ally lines) use it; the report text is byte for byte that of v9's form.
- **Timeline**: `mine_depth_at(site, year)`, `sealed_year(monster)`; monster state: `deep_woken` awake,
  `deep_sealed` asleep.
- **Browser**: 93 `settlements`, 94 to 96 `monsters` (table of 96 rows).
- **Markers**: `event_symbol("hold_lost")` = `sack`; `mine_depth_word(d)` in `markers.hpp`.

## Readings

1. "Sealed in" is decided by any `deep_sealed` naming the monster (a sealed horror never wakes, so it is its last
   sleeping event); the count line still counts it asleep.
2. A mine's final depth for `[sites]` is the largest `depth` of the kind 93 events naming it as `site`; the sealed
   part names the first `deep_sealed` naming the mine as `site`.
3. `deep_flights` and `realms_lost_to_deep` count `refugees_settled` and `civ_ended` with any `hold_lost` among their
   causes; `mines_deepest` counts distinct sites with a depth-3 event; `deep_alive` monsters of a `wake` 3 kind with
   status 1 or 2.
4. The `hold_lost` ruin check reads a faded ruin's ruin through its `ruin_faded` cause, as the ending check does.
5. `soured`: the first `spoils_withheld` of the pair by id with `grievance` 1, inside the same span as the spoils
   counts (after the formation, before the closing event).

## For the viewer engineer

- New mine hover data: `Timeline::mine_depth_at(site, Y)` and `mine_depth_word(d)`; for ", sealed in <year>", the
  mine's horror is the monster of the `deep_woken` naming the site (`MarkerIndex` has no such index; read the
  events, or `Timeline::sealed_year(monster)` once the monster is known).
- `allies:` lines: segments of kind `tagged` (type 3, the ally's id, its name); `block_line_display` gives
  `c<id> <name>` with one link (type 3, id, `tagged = false`) over the whole text, so B4's transform needs no change.
  B14 can count the links with type 3 on an `allies:` line.
- No new glyph: a lost hold is a ruin, its ring symbol at the place marker is `sack`.

## Runs (part C, linux-gcc-release, shipped data, 1024)

Seeds 0 to 19: `generate` and `report --history` at both levels run without error, every report starts
`# worldgen history v10`; world files of seeds 0 to 5 hash equal to cd9dce1's. The sixteen cases of
`world-hashes.sh generate linux-gcc-release` run. Lost holds show in 8 of the 20 summaries (seed 0: waking e391,
fall e392, the ruin `s29 Thraid ... lost to The Eyeless of Handkaird (m13)`, the horror slain by a hero in 5653);
`sealed in` and `; soured` lines appear in several. `linux-clang-debug` builds without warnings; lint clean.

## Left

Nothing of part C. Tests (fixture H18, the reader corruption cases, timeline, browser and markers cases) are the
test writer's.

# r2: allies' aid and goblin deep warrens

Generator engineer, 2026-11-26, same branch. Spec r2 (6.10, 6.11, 6.12); lead rulings L-8 to L-12 as recommended.
Generator 0.29.0, schema 13 and report v10 unchanged.

## What is built

- **Engine** (`monsters_deep.cpp`): `warren_delving` (6.10; T4 and Q4 after `deepen`), the shared waking
  `deep_wake_at(DeepHold)` for a mine (site > 0) or a warren (site 0), called by `deep_waking` for mines first, then
  warrens, at most one waking a step; `deep_aid` (6.11: allies copied from `AllianceView::allies(c)`, q by population
  then smaller id within `deep_aid_hops` by the monster's BFS hops, draw `hist.deep.aid` (m, a, 0)), its strength
  added to the gate's S (`gate_odds(..., aid)`); one `ally_aided_hold` per aid after the `deep_woken`, each followed
  by `RelationsSubsystem::credit(r_a, r_c, deep_aid_memory, aid_event)` when the races differ; the outcome
  (`deep_sealed`, `warren_sealed` or `hold_lost`) cites the waking and then the aids. `deep_take` takes a list of
  causes. `hold_lost` and `deep_woken` `telling` + 3 for a warren horror (`Monster::from_warren`).
- State: per settlement `warren_depth_`, `warren_delver_`, `warren_event_`, `warren_woken_`, `warren_sealed_`
  (grown to the settlement count at each use); per region `warren_region_` (upland or `ore_reach_cells` >=
  `deep_warren_ore_min`, from `inputs.regions`); per race `delving_`, `ally_join_`. `set_alliances`,
  `set_relations`, `warren_depth(settlement)` (7.6); the stage wires both. Stats `warrens_delved`, `warrens_woken`,
  `warrens_sealed`, `aids`, `wakings_aided`.
- Streams: four appended (107). Catalogue: 94's `site` 0-1; kinds 97 to 99.
- Data: race field `delving_permille` (`Field::optional`, default 0; check `race_delves_one_way`); goblin 300, the
  other five 0 in `game/data/races/`; fixture roots leave it out. The eight r2 `history.monsters` fields with 4.2's
  ranges in all three roots; code defaults inert (`deep_warren_wake_permille` 0, `deep_aid_permille` 0). Templates 94
  and 96 widened, 97 to 99 added in the three `report.toml`. `depths_small_content()` gains the r2 edits
  (`with_race_field` sets goblin `delving_permille` 1000).
- Readers (`deep_tables_problem`): telling 0 to 5 on 94 and 96; depth of 98; the 98 chain per settlement (`depth <d>
  again`, `depth <d> without depth <d-1>`), `warren <t> delved by two realms`; **a 94 without a site is legal only
  as a warren waking**: it needs an earlier `warren_delved` of its settlement by its `civilization`
  (`waking of settlement <t> without a warren`) and is the only one there (`a second waking of settlement <t>`); a 99
  must cite its monster's waking and that waking has no site (else `warren_sealed without its waking`); a 97 must
  cite its monster's waking, of its year, whose `civilization` is its `partner`, with `ally` != `partner`
  (`ally_aided_hold <e> does not answer its waking`).
- Report: `sealed in` also for a `warren_sealed`; six stats after `deep_alive` (10 item 7). Timeline:
  `warren_depth_at(settlement, year)`, `sealed_year` and the monster state read 99. Browser: 97 alliances, 98
  settlements, 99 monsters (99 rows). Markers: nothing to change (97 to 99 plain by default). `schema.md`: changelog
  row 13 and catalogue rows 94, 96 to 99.

## Readings

1. `w` for a warren is Y - `event_year(owner_event(t))`; a settlement with `owner_event` 0 is skipped.
2. "Live warren" is computed once before the delving loop (standing, owner = delver, depth >= 1, not sealed) and
   set when a warren gets its first level in the loop, so the first candidate by id wins.
3. The warren wake chance is floor(floor(ppm x M / 1000) x `deep_warren_wake_permille` / 1000), then min 10^6.
4. An ally equal to c, ended or invisible is skipped with no draw; `deep_aid_max` 0 or no seams: no aid, no draw.
5. A warren's waking region is its settlement's region; the hops for the aid are the new monster's hops.
6. `warren_sealed` is not added to P5-06's deed list (spec names only 94 and 95); `{civilization:under}` renders a
   ruler only when one reigns, as for refusals.

## Runs (linux-gcc-release, 1024, shipped data)

Seeds 0 to 19: `generate` and `report --history` at both levels ran without error, every report `v10`; seed 0
twice: same hash. The sixteen cases of `world-hashes.sh generate` ran. Counts: 69 warrens dug, 43 reach depth 3 (in
all 20 worlds: GW1's 10 to 50 per hundred is exceeded, for calibration), 17 wakings (13 mine, 4 goblin, in 4
worlds), 2 warrens sealed, 2 lost at the waking (3 goblin holds lost in all), 6 aids at 6 wakings, all 6 aided gates
held (12 gates held in all). No realm ended by the deep. Forced scratch data at 512 (6.10's hurried values, aid
1000) seeds 0 to 5: 21 goblin wakings, 27 goblin holds lost, a goblin realm ended by the deep, aid at a warren
gate that held and one that fell.

**I1**: with goblin `delving_permille` 0 and `deep_aid_permille` 0 in a scratch copy, seeds 0 to 5 at 1024 differ
from 5e60904 (its own data) only in `event_kind`, `event_kind_role`, `event_kind_value` (the catalogue: rows 94, 97
to 99 and the templates) and `meta` (the data's content hash); every other table hash is equal, so the file hash
differs for these reasons only.

## Left

- Tests (16 items 15 to 19, H19, reader corruption cases) are the test writer's; `p3_01_history_data.hpp`'s copy
  of the shipped files also lacks the r2 fields and `delving_permille`.
- Calibration (15.2 steps 8 to 12): GW1 is high at the starts.
- `ctest linux-gcc-release`: the same 31 of 45 entries fail as at r1 (the stale embedded `p3_01_history_data.hpp` copy, then 0.28.0 goldens and counts); none is a crash or reader error of r2.

## For the viewer engineer

`Timeline::warren_depth_at(t, Y)` with `mine_depth_word(d)` for the settlement hover (11.2 item 4); M12 can use
`depths_small_content()` or the hurried data above (every seed 0 to 5 at 512 has a warren at depth >= 1).
