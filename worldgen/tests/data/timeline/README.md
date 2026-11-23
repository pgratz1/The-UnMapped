# Timeline fixture (spec P4-01 13.3)

`t1-edges.toml` is fixture T1, built twice as the P3-07 fixtures are: by the C++ tests through the writer
API (`worldgen/tests/timeline_support.hpp`, a pipeline of the stages `biomes`, `regions`, `chronology` and
`history`) and by the reference through SQL (`worldgen/tests/reference/p4_timeline_reference.py fixture
t1-edges OUT.db`). The two copies must have the same table hashes (`hash --detail`'s `table` lines, the
`layer` table included); the golden file `worldgen/tests/golden/p4-01-timeline.txt` records the
reference's.

## Format

The format of `../history-report/README.md` (`[meta]`, `[tables]`, `[[kind]]`, `[[event]]`), plus
`[[layer]]` entries, written as layers in file order (ids 1, 2, ...) with codec 1 (delta varint) by the
stage `biomes`:

| Key | Meaning |
| --- | --- |
| `name` | the layer name (`elevation`, `water`, `river`, `flow_dir`, `biome`, `region`) |
| `type` | the element type: `u8`, `u16` or `i16` (the catalogue types of these layers) |
| `width`, `height` | the dimensions |
| `cells` | `width` x `height` integers, row by row from y = 0 (north), each row from x = 0 (west) |

The layers are 8 x 6 cells while `meta.param.map_size` is 512: the timeline and the history checks read
no layer, and the territory tests take the layers as they are.

## What T1 covers

E = 2000. Races elf (awakening 0) and dwarf (awakening 1000); civilizations c1 elf, c2 dwarf, c3 elf, c4
dwarf (ordinals counted per race). Region ids 1, 2, 3, 4 and 6 (no row 5; the `region` layer uses the
value 5 for cells of no region row).

| Case of 13.2 | Where |
| --- | --- |
| item 3: a region held, lost and retaken | r2: c3 [300, 1200), free [1200, 1300), c3 from 1300 |
| a change of holder in one year | r3: c2 [1000, 1600), c1 from 1600 |
| a region never held / one freed | r6 (since 0) / r4 (c4 [1010, 1800), then free since 1800) |
| a census gap | s1: rows at 0, 149 and 1500 |
| ruined and faded settlements | s2 ruined 1200, faded 1400; s5 ruined 1800 |
| a settlement not yet founded | s6, founded 1900; s4, founded 1300 |
| a relation with `until` = `since` | relation 2, war c2-c4 1550-1550 |
| an open relation at E; one closed at E - 1 | relation 1, enmity c1-c4 from 1100; relation 3, enmity c3-c4 [1300, 1999) |
| a civilization ending in Y | c2 ends in 1600 |
| multi-year events | e12 1150-1499, e22 1590-1600, e27 1950-1999 |
| item 4: two causes, repeated participants | e21 (`council`, a test kind): causes 17 and 19; c1 as host and guest; s1 as place and site; r6 only as a participant |
| item 7: territory cells | the held cell (0, 0), the border (1, 0), the lake (0, 1), a water-1 cell of class 0 (2, 1), rivers (4, 0) class 1 with a diagonal closing onto (5, 0) and (1, 3) class 4 widened onto (2, 3) (a border) and (1, 4), elevation 0 at (0, 5), a held region's cell with e < 0 at (6, 1), ocean at x = 7, a free region (r6), region value 5 with biome 9 (no rows) |
