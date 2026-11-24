# Browser fixture (spec P4-05 10.2)

`t3-browser.toml` is fixture T3, built twice as P4-01 13.3 builds T1: by the C++ tests through the writer API
(`worldgen/tests/browser_support.hpp`, the P3-07 pipeline of `history_report_fixture.hpp` with the region seed
cells applied) and by the reference through SQL (`worldgen/tests/reference/p4_browser_reference.py fixture
t3-browser OUT.db`). The two copies must have the same table hashes (`hash --detail`'s `table` lines); the golden
file `worldgen/tests/golden/p4-05-browser.txt` records the reference's (`t3-browser table <name> <hex>`).

## Format

The format of `../markers/README.md`: the P3-07 fixture format of `../history-report/README.md` with a
`[markers]` table. T3 uses its `region_seed` rows and has no pass.

## What T3 covers

E = 2000, two races (humans, ra1, awaken at 0; dwarves, ra2, at 1000), two ages, five phases, 23 events.

| Case of 10.1 and 10.2 | Where |
| --- | --- |
| three civilizations of two races, one ended by conquest, one seceded | c1 The Kingdom of Arn and c2 Belmark (human), c3 Stonehelm (dwarf); c2 secedes from c1 in 500 (e6); c3 falls to c1 in 1800 (e22) |
| a civilization without a capital | c2 (capital 0 after c1 retakes Belford, e18) |
| an adversary of another type | c1's enmity with f1 (entity type 6, which has no name: reading B1 of the reference) |
| a renamed region, a sentence with its former name | r2 The Holt of Yews, called The Red Holt from 1300 (e15, `{@region_former}`); e4, e12 to e14 name it by its former name |
| a region found by a former name | r5 Cor Wold, renamed Ash Wold (e20) and Grey Wold (e21): "ash" finds it through a `region_name` row only |
| a settlement with a non-ASCII name | s4 Khaz-a-circumflex-d (bytes `4b 68 61 7a c3 a2 64`) |
| `{role}`, `{role:all}` with three participants, `{role:plural}`, `{role:adjective}`, a lower-cased `The `, `{settlement}` | e3, e19 (summit, three `parties`), e1, e11 (craft), e2 and others, e3 |
| a cause chain three deep, an event with two effects | e1 -> e2 -> e3 -> e5; e2 (effects e3, e4), e13 (e14, e16) |
| multi-year events | e16 siege 1350-1360, e19 summit 1500-1502 |
| events with a settlement, with only a region, with neither | e16; e12, e15, e20; e7 (omen), e11 (craft) |
| events outside the summary set | e7 (omen, salience 10) and e12 (the claim) |
| every kind group | realms, settlements, war, calamity (e23 plague), other (the test kinds omen, craft and summit) |
