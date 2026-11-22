# P3-07 history report: validation

Candidate: branch `p3/07-val` = `origin/p3/07-final` at `debdd8c`; generator 0.20.0, schema 7. Validator: Claude Sonnet 5.5 (validation specialist). Linux only (no Windows here; CI covers it).

## Verdict: PASS

No defect found. No world byte changed (all 15 golden world hashes match, `expect` ok on both compilers). No golden file recorded or touched.

## 1. Builds and world hashes

| Check | Result |
| --- | --- |
| Build `linux-gcc-release`, `linux-clang-release` | both exit 0 |
| `tools/ci/world-hashes.sh local linux-gcc-release linux-clang-release` | rc 0; "all 15 cases identical across 2 presets" (the compare includes the saved `history.summary.txt` and `history.full.txt` of each case, 4 files per world case); 1m19s |
| `tools/ci/world-hashes.sh expect` against `p0/p1/p3-worlds.txt` | gcc: 15 ok, 0 other; clang: 15 ok, 0 other |

Ctest was not rerun (the test writer ran all five Linux presets and the slow suite; CI runs all seven).

## 2. Sweep

Worlds: the P3-11 sweep, `~/.cache/the-unmapped/out/p3-11-validation/sweep/<variant>/s<seed>-1024.db`: `default` seeds 0-99, `calm`, `turbulent` and `civs4` seeds 0-19, all 1024, 160 worlds, 320 reports. Clang texts compared for 22 worlds (default 0-9; calm, turbulent, civs4 0-3), both levels. Each report was run twice with GCC (second run compared byte for byte). Reports and per-run log: `~/.cache/the-unmapped/out/p3-07-validation/<variant>/s<seed>.{summary,full}.txt`, `runs.log`, `rows.json`.

Extra check beyond the brief: every one of the 160 worlds was also compared, both levels, byte for byte, with the independent reference (`p3_report_reference.py compare`); all equal.

| Variant | Worlds | Exit 0 | Rerun identical | GCC = Clang | Reference = C++ | Orphans (sum) | Quiet stretches shown/total | Full: lines = events, id order |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| default | 100 | 100 | 100 | 10 of 10 | 100 | 0 | 729/729 | 100 |
| calm | 20 | 20 | 20 | 4 of 4 | 20 | 0 | 113/113 | 20 |
| turbulent | 20 | 20 | 20 | 4 of 4 | 20 | 0 | 180/180 | 20 |
| civs4 | 20 | 20 | 20 | 4 of 4 | 20 | 0 | 185/185 | 20 |

(Both levels of each world are in every count; "GCC = Clang" holds at both levels.) Orphans come from the reference (`metrics`, an independent computation from the db, 24.10 item 1); the shown-event count of the reference equals the C++ `shown` on every world. Quiet stretches: lines of kind `quiet_stretch` in the summary chronicle against `events_quiet_stretch` in `[stats]`. Full: event lines of the chronicle equal `events` = `shown`, ids strictly increasing. Summary: event lines equal the header `shown`.

### Length and time

| Variant | Summary lines min / median / max | Full lines min / median / max | Events min / median / max | Shown min / median / max | T max | Report ms median / max |
| --- | --- | --- | --- | --- | --- | --- |
| default | 497 / 722.5 / 912 | 731 / 1421.5 / 2565 | 229 / 681.5 / 1459 | 115 / 208.5 / 249 | 71 | 12 / 19 |
| calm | 517 / 651.5 / 788 | 833 / 1091 / 1579 | 285 / 441.5 / 719 | 132 / 194.5 / 249 | 26 | 10 / 13 |
| turbulent | 647 / 885 / 1232 | 1212 / 2051 / 4193 | 597 / 1137 / 2664 | 158 / 216.5 / 250 | 76 | 16 / 30 |
| civs4 | 1149 / 1364.5 / 1578 | 2813 / 3675.5 / 4222 | 1439 / 1978.5 / 2313 | 159 / 208 / 247 | 76 | 25 / 29 |

Time is the slower of the two levels per world, wall clock of one `report --history` process, GCC release, with four reports running at once (so an upper estimate). Maximum 30 ms against the 2 s bound. `shown` never exceeds 250 (T never 100).

Statistical notes (not defects): every world has at least one quiet stretch; the longest summaries are the civs4 worlds (up to 1578 lines), as expected from the extra realms' blocks, with the chronicle held to 159-250 events.

## 3. CLI behaviour

| Case | Command (abbreviated) | Exit | Message |
| --- | --- | --- | --- |
| `--out` existing file, no `--force` | `report --db seed42-1024.db --history --out exist.txt` | 4 | `output 'exist.txt' already exists (pass --force to replace it)`; file left unchanged |
| same with `--force` | `... --out exist.txt --force` | 0 | file replaced (792 lines) |
| unknown level | `report --db seed42-1024.db --history --level bogus` | 2 | `invalid --level 'bogus'; expected summary or full` |
| world made with `--until none` | `report --db none.db --history` | 3 | `world file 'none.db' has no history (stages: none); generate it with the history stage` |
| missing db | `report --db nonexist.db --history` | 3 | `cannot read ...: the file is missing or unreadable` |

## 4. Commands

```
B=~/.cache/the-unmapped/build/p3-07-val
cmake --preset linux-gcc-release && cmake --build --preset linux-gcc-release        # also linux-clang-release
tools/ci/world-hashes.sh local linux-gcc-release linux-clang-release
tools/ci/world-hashes.sh expect linux-gcc-release; tools/ci/world-hashes.sh expect linux-clang-release
# per world (variant V, seed S), L in summary full; the script also reran and compared, and for 22 worlds ran Clang:
$B/linux-gcc-release/tools/worldgen-cli/worldgen-cli report --db ~/.cache/the-unmapped/out/p3-11-validation/sweep/V/sS-1024.db --history --level L
python3 -B worldgen/tests/reference/p3_report_reference.py metrics DB
python3 -B worldgen/tests/reference/p3_report_reference.py compare DB V/sS.summary.txt V/sS.full.txt
# phrase, seed 42, until none worlds
worldgen-cli generate --seed-phrase "The Unmapped" --map-size 1024 --data game/data --out phrase-1024.db
worldgen-cli generate --seed 42 --map-size 1024 --data game/data --out seed42-1024.db
worldgen-cli generate --seed 7 --map-size 512 --until none --data game/data --out none.db
```

The sweep driver and analysis scripts were kept in the session scratchpad, not in the repository; every number above is reproducible from the commands.

## 5. Phrase world sample

`--seed-phrase "The Unmapped"`, map size 1024, default settings: summary 674 lines, T 26, 600 events, 161 shown, 18 shown declarations, 0 orphans; reference and C++ equal at both levels. Seed 42 at 1024: 792 summary lines, T 26, 227 shown, 28 declarations, 0 orphans, equal at both levels. Full summaries: `~/.cache/the-unmapped/out/p3-07-validation/phrase.summary.txt` and `seed-42.summary.txt`.

First 40 lines of the phrase world summary (long lines cut at 260 characters here only):

```
# worldgen history v1
level = summary
[settings]
seed = 15629206466951235176
seed_phrase = the unmapped
map_size = 1024
history_span = 6000
history_detail = normal
races = elf,goblin,dwarf,faerie,human,orc
civs_per_race = auto
event_intensity = normal
monster_pressure = normal
divine_activity = normal
divine_script =
generator_version = 0.20.0
[ages]
a1 The Age of Elves: 0-2093, 2094 years; Elves (ra1) made by Esis (g1, good); 1 civilization founded: 1 at the awakening, 0 by secession
a2 The Age of Goblins: 2094-3326, 1233 years; Goblins (ra2) made by Slatbeex (g2, evil); 2 civilizations founded: 2 at the awakening, 0 by secession
a3 The Age of Dwarves: 3327-4225, 899 years; Dwarves (ra3) made by Nukgarin (g3, neutral); 1 civilization founded: 1 at the awakening, 0 by secession
a4 The Age of Faeries: 4226-5035, 810 years; Faeries (ra4) made by Sofosh (g4, good); 2 civilizations founded: 2 at the awakening, 0 by secession
a5 The Age of Humans: 5036-5637, 602 years; Humans (ra5) made by Birdsten (g5, neutral); 1 civilization founded: 1 at the awakening, 0 by secession
a6 The Age of Orcs: 5638-5999, 362 years; Orcs (ra6) made by Uul'duk (g6, evil); 4 civilizations founded: 1 at the awakening, 3 by secession
[chronicle]
threshold = 26
events = 600
shown = 161
== a1 The Age of Elves: 0-2093
-- p1 opening: 0-149, 150 years; trigger e1; 3 events
0 e1 awakening/100: Esis awakened the Elves in Stagbarrow Tangle. {@rg34 race=ra1 creator=g1}
0 e2 civ_founded/80: The Elves founded the Realm of Lecier in Stagbarrow Tangle. {@rg34 civilization=c1 race=ra1} [causes e1]
0 e3 settlement_founded/40: The Realm of Lecier founded Vaenies in Stagbarrow Tangle. {@rg34 @s1 settlement=s1 founder=c1 population=300} [causes e2]
-- p2 quiet: 150-2093, 1944 years; 7 events
150-2093 e4 quiet_stretch/20: For 1,944 years, from 150 to 2093, the land was quiet; one realm held it. {realm=c1 years=1944 realms=1}
1675 e9 colony_founded/30: The Realm of Lecier sent settlers from Simi to found Arvies in Yewholt. {@rg39 @s5 settlement=s5 founder=c1 origin=s2 population=104}
(5 more events not shown)
== a2 The Age of Goblins: 2094-3326
-- p3 opening: 2094-2243, 150 years; trigger e11; 9 events
2094 e11 awakening/100: Slatbeex awakened the Goblins in Mason's Steppes. {@rg8 race=ra2 creator=g2}
2094 e12 civ_founded/80: The Goblins founded the Warrens of Skruz-Snugg in Mason's Steppes. {@rg8 civilization=c2 race=ra2} [causes e11]
2094 e13 settlement_founded/40: The Warrens of Skruz-Snugg founded Skozz Hole in Mason's Steppes. {@rg8 @s6 settlement=s6 founder=c2 population=300} [causes e12]
```
