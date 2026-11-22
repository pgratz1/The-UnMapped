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
| `h4-budget.toml` | H4: H2 with 1,000 notes of salience (37 i) mod 101 in one year: T = 76 |
| `h5-saturated.toml` | H5: H2 with 300 events of salience 100: T = 100, 301 lines |
| `h6-notes.toml` | 21.4, 22.3, 22.5a, 23 items 2 to 4a: declaration notes (claims of kinds 20 and 39, the claim's region different from the war's, same year and earlier, ordinals), flare-up notes (kind 20 with a `The ...` claimant, kind 39 with one and three settlements), two searches abandoned (salience 40 and 15), a peace whose cession of settlements is shown as a companion, a sack with its moved capital and refugees as companions, an open war |
| `h7-runs.toml` | `wars_same_goal_unchanged_max` (21.8 item 5 with 22.5b): a region emptied and resettled by its holder (one run of 2) and one taken by another civilization between wars (runs of 1): key 2 |

## Format

TOML. `[meta]` holds the settings recorded in `meta` (`seed`, `seed_phrase`, `map_size`, `history_span`,
`history_detail`, `races`, `civs_per_race`, `event_intensity`, `monster_pressure`, `divine_activity`,
`divine_script`); the world's stages are `biomes,regions,chronology,history`, its content hash that of no
data, its generator version the current one. `[tables]` holds rows as arrays in column order (schema 7,
`docs/specs/schema.md`), with these exceptions:

- `region` rows are `[id, name, biome, cells, lake_cells, walk_cells]`; every other column is 0.
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
