# History report fixtures (spec P3-07 14.3)

Hand-made worlds that both the C++ tests (`worldgen/tests/history_report_fixture.hpp`, through the writer
API) and the reference (`worldgen/tests/reference/p3_report_reference.py fixture`, through SQL) build.
The two copies must have the same table hashes (`hash --detail`'s `table` lines); the golden file
`worldgen/tests/golden/p3-07-report.txt` records the reference's.

| File | Covers |
| --- | --- |
| `h1-war-and-ruin.toml` | H1: two races present and one absent, three civilizations (one conquered), a sacked ruin, a faded ruin, a standing unowned settlement, all five settlement kinds, two flare-ups (one of one year), a closed war, open relations of other kinds, a relation with a reserved entity type 6, a region taken by another civilization, census changes at phase ends with a tie of peak population, two runs at the peak of regions and a one-year peak, causes, a `:all` role, a negative value, an event with `{}`, test kinds outside the compiled catalogue, a template ending in a space and one rendering empty, a backslash in a template and a region name with a backslash and a non-ASCII byte, quiet-stretch milestones with 0, 1 and 12 generations |
| `h2-minimal.toml` | H2: one race, one civilization with no settlement and no region, span 2000, an opening and a final phase |
| `h3-runaway.toml` | H3: islets (one at 1171 dry cells, one exactly at 1172 is not), a civilization above 800 per mille for 1,001 years, another above 900 for 500, a handover in one year |
| `h3b-runaway-1000.toml` | H3 with the first run 1,000 years long |
| `h3c-share-ties.toml` | `share_max_*` ties: the earlier year wins over the smaller id, then the smaller id |
| `h4-budget.toml` | H4: H2 with 1,000 notes of salience (37 i) mod 101 in one year and a quiet stretch (r17 24.11 item 4): T = 77 |
| `h5-saturated.toml` | H5: H2 with 300 events of salience 100 and a quiet stretch: T = 100, 303 lines |
| `h6-notes.toml` | 21.4, 22.3, 22.5a, 23 items 2 to 4a: declaration notes (claims of kinds 20 and 39, the claim's region different from the war's, same year and earlier, ordinals), flare-up notes (kind 20 with a `The ...` claimant, kind 39 with one and three settlements), two searches abandoned (salience 40 and 15), a peace whose cession of settlements is shown as a companion, a sack with its moved capital and refugees as companions, an open war |
| `h8-frames.toml` | r17 24.1, 24.2, 24.11 item 1: a war of three battles, a siege, a sack with its moved capital and refugees, a peace and a tribute; a war ended by a conquest; refugees alone a candidate in a third war; a war ended by a repeated search; a capital moved after an abandonment; ruins by sack and by abandonment; two refuges |
| `h9-realms.toml` | r17 24.5 to 24.7, 24.11 items 3, 7 to 10: three secessions in one age, the wars line with every outcome, adversaries with every state, milestones (the kind limit, ties, the longest quiet stretch with 0 and 4 generations, a realm with fewer than eight events), every standing-claim case, a settlement captured twice before its sack, a refuge, two open wars |
| `h10-floor.toml` | r17 24.3 item 3, 24.11 item 5: an age whose candidates of salience 25 are split between its race and another; 300 chants of salience 90 in the next age (T = 91) |
| `h11-v2.toml` | P3-07 r19 25.13 items 2 to 9 (format v2; the spec's "H9 v2", named H11 since H9 is r17's): five calamity candidates 45, 45, 40, 25, 25 in one phase and one in another; ruins by a sack with and without an attacker, an abandonment, an abandonment with survivors to a destination and an unknown kind (`swallowed`); a faded and a ruined settlement of an ended realm; two war adversaries and three without war (two in enmity, one none); a realm with only a no-war adversary; an ended realm in short with a quiet-stretch milestone; flare-ups with and without a war in their phase at T = 100 (300 chants of 100 in the second age); a war ended in 1656; a no-war realm whose kind changes and claims outrank its plagues; a fresh kind 20 claim caused by a claim of 1300 on another region and a fresh kind 39 claim; refuges with kind 42 to E - 1, to 1990, and none; a claim dropped (kind 41) and one standing. Its `sacked` kind has an optional attacker (role min 0) and a template that names only the defender, so the sack without an attacker loads and renders (the view refuses an event that lacks a participant of a role of min 1, and one with fewer values than its kind) |
| `h12-v3.toml` | P3-07 r21 26.8 items 2 to 5 (format v3, P5-12): region 2 renamed twice (after a sack, then after a battle; the second renaming of salience 95) and region 4 once with no namer; refuges with kind 42 to E - 1, to 1200 and none; a renamed home region; a war declared over region 2 before its renamings and a claim after them; 1000 notes of salience (37 i) mod 101 (T = 78, above the renaming's 60) |
| `h13-v4.toml` | P5-02 19.1 item 15 (format v4, schema 9): two monsters (a troll appears, raids, is slain; a dragon appears in a quiet step, starts the flare-up whose note is 12.7's, raids and overruns a village and sleeps), five sites (three lairs, one moved and one slain; two outposts, one left when the frontier fell quiet), a ghost place of each ending (plague with survivors to a destination, famine, left after a raid), a sacked ruin in a region renamed after it (`then called`), a boldness of the wolves, beast rows. Its settlement rows have fourteen columns: `ending` is derived from the log (P5-02 8.3) by both builders; the five new tables are given as rows |
| `h14-v5.toml` | P5-03 24.1 item 11 (format v5, schema 10): three races and three realms; a pair met in hatred, a pair met wary and turned to hatred after a sack, a pair that never met; a capture that leaves the elves subject in Fievos (mixed), their expulsion and their shelter in Durnhold; goblin survivors of a sack sheltered in Durnhold and held as subjects from a phase start (a standing change), so that Durnhold is mixed of three races; an ogre that raids a hamlet three times (a chain of causes), drives its people away for fear (ending 6, survivors to Durnhold) and is slain: told by the summary's floor and listed among the ended monsters. Its `race_relation` and `settlement_race` rows are given in column order; `beast` and `monster_kind` rows carry their phrase columns (schema 10); generated from the shipped 0.24.0 templates by the test writer's script |
| `h15-v6.toml` | P5-03a 14 item 11 (format v6): from H14, four races and four realms; a pair met wary, turned to hatred after a capture and back to wariness after long years of peace (a warmer turn citing the meeting), a pair met by word (`sealed` 1), a pair that never met while both met others, a race that met no one; four `settlement_mixed` events of two race pairs (only the first of each a summary candidate), one arrival caused by a plague and its abandonment, one expulsion; seventeen ruins of distinct ruined years and sizes; a realm with more than eight milestone events, two of them quiet stretches; an enmity without war. Written by `worldgen/tests/reference/p5_03a_h15_fixture.py` from the 0.25.0 listings |
| `h7-runs.toml` | `wars_same_goal_unchanged_max` (21.8 item 5 with 22.5b): a region emptied and resettled by its holder (one run of 2) and one taken by another civilization between wars (runs of 1): key 2 |

## Format

TOML. `[meta]` holds the settings recorded in `meta` (`seed`, `seed_phrase`, `map_size`, `history_span`,
`history_detail`, `races`, `civs_per_race`, `event_intensity`, `monster_pressure`, `divine_activity`,
`divine_script`); the world's stages are `biomes,regions,chronology,history`, its content hash that of no
data, its generator version the current one. `[tables]` holds rows as arrays in column order (schema 8; schema 9 from generator 0.23.0, P5-02,
`docs/specs/schema.md`), with these exceptions:

- `region` rows are `[id, name, biome, cells, lake_cells, walk_cells]`; every other column is 0.
- `region_name` rows (schema 8, P5-12 7.2) are `[region, from_year, name, culture, event]`.
- Schema 9 (P5-02 11.2): `settlement` rows may have fourteen columns, `ending` then derived from the log (8.3;
  a faded row through its `ruin_faded` event's cause); `beast`, `monster_kind`, `beast_population`, `monster` and
  `site` rows are given in column order (`-1` is NULL in `monster.died` and `site.ended_year`), empty when absent.
- Schema 10 (P5-03 16.1): `race_relation` rows `[race_a, race_b, year, disposition, band]` and `settlement_race`
  rows `[settlement, race, year, share, standing]`; `beast` and `monster_kind` rows end with their phrase column
  (`bold_phrases`, `raid_phrases`); a fixture of schema 9 without it gets one phrase (`grew bold near`, `fell upon`).
- `-1` stands for NULL in the nullable columns (`race.awakening_year`, `civilization.ended_year`,
  `settlement.ruined_year`, `region_control.to_year`, `relation.until`).
- Rows are written in primary-key order whatever their order in the file.

`[[kind]]` entries are the event kinds in id order (id = position, from 1): `key`, `caused`,
`region_use`, `settlement_use`, `salience`, `template`, `roles` (`[role, entity_type, min_count,
max_count]`, seq = position) and `values` (keys, seq = position).

`[[event]]` entries are the events in id order (`id` must equal the position): `year`, `end` (default
`year`), `kind` (a key of the fixture's catalogue), `phase`, `region` and `settlement` (default 0),
`salience` (default the kind's), `parts` (`[role, entity id]`; seq = position, role and entity type from the
kind's role), `values` (in the kind's value order) and `causes`.

`[[bulk]]` entries append `count` events after the listed ones (i = 1 ... count): `kind`, `year`, `phase`,
no region, settlement or participant; salience `salience`, or `(salience_mul * i) mod salience_mod`; with
`value = "index"` one value, i.
