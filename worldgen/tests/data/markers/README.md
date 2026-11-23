# Marker fixture (spec P4-04 10.2 item 3)

`t2-markers.toml` is fixture T2, built twice as P4-01 13.3 builds T1: by the C++ tests through the writer API
(`worldgen/tests/markers_fixture.hpp`, a pipeline of the stages `biomes`, `regions`, `chronology` and
`history`; the pass rows are written by `biomes`) and by the reference through SQL
(`worldgen/tests/reference/p4_markers_reference.py fixture t2-markers OUT.db`). The two copies must have the
same table hashes (`hash --detail`'s `table` lines); the golden file `worldgen/tests/golden/p4-04-markers.txt`
records the reference's (`t2-markers table <name> <hex>`).

## Format

The format of `../history-report/README.md` (`[meta]`, `[tables]`, `[[kind]]`, `[[event]]`), plus a table
`[markers]`:

| Key | Meaning |
| --- | --- |
| `region_seed` | `[id, seed_x, seed_y]` per region: the seed cell, which the P3-07 format leaves 0 |
| `pass` | rows of table `pass`: `[id, range_id, kind, capped, col_x, col_y, saddle_dm, col_dm, [cells]]`, the path written as u32 little-endian cell indices, 4 bytes per cell (P1-01 9.1) |

No layers: the marker tests read none on T2 (overlays are tested on generated worlds and on hand-built marker
sets in `markers_draw_test.cpp`).

## What T2 covers

E = 2000, one race (elf), civilizations c1 Alder, c2 Birch (ends 1500), c3 Cedar (secedes 1200).

| Case of 10.2 item 3 | Where |
| --- | --- |
| a settlement before its founding | s7, founded 1900; s3, founded 600 |
| standing, declining | s1 declining 1000 to 1599, thriving again from 1600 |
| ruined in R (standing at R - 1) | s5, abandoned 1300 (row `ruin_event` 17); s6, sacked 800 (e11) |
| faded in D (ruin at D - 1, absent or faded at D) | s6, faded 1100 (e15 `ruin_faded`) |
| a faded ruin whose ruin event is found only through `ruin_faded` | s6: its row has `ruined_year` NULL and `ruin_event` 0; e15 is caused by e11 |
| an owner change | s2, captured by c1 in 1400 |
| a capital founded, moved, by secession | c1: s1 (e3, 0), then s4 (e12 `capital_moved`, 900); c2: s2 (e5, 100); c3: s3 (e16 `civ_seceded`, 1200) |
| a civilization without a capital, one ended | c2 from 1400 (its seat owned by c1) to 1499; c2 ended in 1500 |
| an event with a settlement, with only a region, with neither | e14 at s1; e19 `claim` and e27 `battle_at_pass` at r4's seed (450, 450); e13 and e20 `omen` |
| two events at one cell with symbols battle and sack | e21 `battle` and e22 `settlement_captured` at s2 in 1400: one place, `sack` |
| multi-year events | e18 `siege` 1380-1399; e24 `plague_outbreak` 1500-1502 |
| a ring at a faded ruin's cell | e25 `pilgrimage` (a test kind, symbol plain) at s6 in 1500 |
| Y = 0, E - 1, E | e1, e2 at r1's seed and e3 at s1 in 0; e28 `plague` at s3 in 1999; no events at E |
| passes | p1 lowered at (250, 250), p2 natural gap at (10, 500) |
