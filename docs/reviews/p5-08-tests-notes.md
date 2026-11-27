# P5-08 tests: notes for integration

Spec: `docs/specs/p5-08-pantheon-cults.md` r1 (generator 0.30.0, schema 14, report v11) with the lead's rulings L-1 to L-12. Written by the test writer from the spec alone on `p5/08-tests` (base 1eafe00), without reading `p5/08-impl`.

## Switch and adapter

`worldgen/tests/history/p5_08_api.hpp`: define `UNMAPPED_P508_API` (the commented line, or `-DUNMAPPED_P508_API`). Until then every case needing 0.30.0 starts with `P508_REQUIRE_INTEGRATED()` and returns with a message; `history_pantheon_unit_test.cpp` runs now. Library names reached only under the switch, with the spec's signatures (a compile error is a finding): `Timeline::faith_at(civ, year)`, `cults_in_at(civ, year)` (vector), `cult_status_at(faction, year)`, `divine_effect_at(civ, year)`, `inspect::shrine_word(bool)`. The checks read tables by name (`WorldReader::table_rows`: `god` 6 columns, `faction` 13), so they need no new row type. Content edits name `history.pantheon` fields and fail at run time on 0.29.0 content. The seams of 7 are not called from C++; the reference's replay checks them.

## Files

| File | Covers | Waits |
| --- | --- | --- |
| `reference/p5_08_pantheon_reference.py` (new) | `Pantheon` (6.2 to 6.10, 7.1, 7.3 sent monsters), formulas, `violations` (13 items 1 to 9, 12, 13), `reader_problems` (9), `stats_of_tables` (10 item 8), the v11 blocks (`pantheon_block`, `cults_block`, `realm_lines`), the 11.1 reads; `units` 165 checks, `fixtures`, `check-world`, `golden`, `readings` | — |
| `reference/p5_08_h20_fixture.py` (new) | Writes `data/history-report/h20-v11.toml` and `h20-v11.expected.txt` | — |
| chain edits (flags `pantheon`, `p508` of `p3_conflict_reference`, on from 0.30.0) | `p3_data_reference` (0.30.0, `KINDS_030`, `GODS_030`, `PANTHEON_030`, culture forms, report entries, follow-up fields, `FALLBACK_030_USED`); `p3_engine_reference` (`COMPILED_030`, `RENDER_R30`, `{g:titled}`, faction names, schema 14); `p3_peoples_reference` (growth term, shunned); `p3_conflict_reference` (install, T4/T5/Q4, third tension source, force); `p3_chronology_reference` (6.1); `p5_02` (L_world, sent monsters in its invariants); `p5_04` (12.5, `partners`); `p5_06` (champions, prophets, `raise_divine`, appointment, epithets, deeds, 12.6); `p5_07` (12.2 to 12.4, 12.7; D1) | — |
| `history/p5_08_checks.hpp` | `p508_f` formulas; `p508_check` rows, `violations`, `stats`, `paths_of`, `faith_at`, `divine_effect_at` | — |
| `history/history_pantheon_unit_test.cpp` (shard `pantheon`) | 16 items 3 to 6, 12 by hand | runs now |
| `history/history_pantheon_data_test.cpp` (`pantheon`) | 16 item 1 on game/data: the nine gods, god checks on an added file, deeds' checks, `history.pantheon` order/starts/bounds/absence/two checks, culture forms, creator_title, follow-up starts | all |
| `history/history_pantheon_property_{a,b,c}_test.cpp` (shards a, b, c; slow `pantheon-slow`) | a: pantheon-small 0 to 9 at 512, invariants and required paths; b: shipped 0 to 9, determinism (twice, 1 and 4 threads); c: I0, resume, activity order, the 15 god rows (seeds 0, 1, 42, `elf,orc`); slow: shipped 0 to 19 at 1024, meddling and distant 0 to 4 | all |
| `history/history_pantheon_reader_test.cpp` (`pantheon`) | 16 item 10: god, faction, site, figure, value and order checks in both readers; schema 13 refused | all |
| `history/history_pantheon_report_test.cpp` (`pantheon`) | 16 items 9, 11, 13 on H20: header, the expected lines, the timeline reads against rows, shrine_word, group 9, glyphs | all |
| `history/history_pantheon_golden_test.cpp` (`pantheon`, slow) | `golden/p5-08-pantheon.txt` world lines; the fixture files' promised paths | all |
| `data/pantheon/*.toml` | below | — |
| `golden/p5-08-pantheon.txt` | fixture lines (reference); world lines at integration | — |

CMake: sources and `SHARD pantheon-property-a/-b/-c` (80 s), `SHARD pantheon` (40 s), `SLOW slow-pantheon` in `worldgen/tests/CMakeLists.txt`.

## Fixtures (pantheon-small base, on the conflict stress geographies; each file lists the paths it must show)

- `pantheon-acts`: no cults, no rests: the five kinds, plenty, valour, pestilence, defeat, both marks.
- `pantheon-cults`: every act founds, spread at 1000 by any affinity, faith 600, suppression 400: founders (prophet, ruler, caster, champion), spread, faith, suppression into hiding and to an end.
- `pantheon-flare`: act_permille 0: every act is a certain one after a gods' flare-up.
- `pantheon-activity`: act_permille 100 at distant, normal, meddling on one geography.
- `pantheon-followups`: depths-small deep years with `deep_stir_permille` 1000, `renew_written_max` 1, `tower_lapse_years` 20: a capped renewal, a tower abandoned.
- `pantheon-creators`: no god definitions (4.1): creators alone.
- pantheon-small itself (stress seeds 0 to 14) shows every path, including a stir (seed 9), a take-up (seed 1), a cult ended with its realms (seed 5).

`fixtures`: 72 checks, 0 failures (every invariant of 13 and P5-07's and P5-02's on every run; I0 on stress 0 to 2 against the build without the subsystem). P5-07 regression: `units` 231/0, `fixtures --quick` 28/0; D1's seed-55 world now passes `check-world`.

## Readings (P-R numbers of `READINGS`)

P-R1 field positions (appended last); P-R2 growth factor after the floor of 1; P-R3 order inside T4/Q4; P-R4 the flared pair takes precedence in its first year; P-R5 one event is one cause; P-R6 regions (capital, else home region); P-R7 a champion's first command is `again` 0; P-R8 take-up by region, the master is the last caster of the site; P-R9 a cult created in a step is invisible in it; P-R10 no reign: factor 1000, a faith needs a capital, rival gods each once; P-R11 creator affinity by race; P-R12 hiding decided before the event; P-R13 temple ending events compared by id; P-R14 flares count; P-R17 the flare's candidate at the quiet step's start; P-R18 e*'s region; P-R21 forgotten contributions are not cited (else invariant 2's memory bound fails: add drops old ones only when a new one comes).

## Findings for the spec writer (the reference picked a reading)

- **F1 (8.2 vs P3-04 8.4)**: `{profane? hallowed| cursed}` and `{hidden?|, and its faithful fled into hiding}` render inverted under the two-word rule (w0 for 1): the samples need the opposite. The reference swaps the words (P-R20); the C++ tests expect the samples.
- **F2 (12.3 vs P5-04a 8.5)**: the tellings of 93 and 98 are nine-word choices, above 8.5's limit of 8; the reference allows 9 from 0.30.0 (P-R19).
- **F3 (10 item 8)**: `renewals_capped` and `deep_stirred` cannot be derived from rows (no event), so a report of a stored world cannot print them unless the world stores them; the C++ stats leave them out.
- **F4 (13 item 4)**: "its lair within send_hops of c's capital region" needs the capital at the act, which rows do not keep; checked in the replay only. Item 10 and 11's stir likewise.
- **F5 (6.2)**: forgotten contributions vs "within regard_memory_years" (P-R21).
- 9's reader messages are given only for some checks; the C++ tests compare prefixes elsewhere (P-R15).

## Integration

1. Define the switch; fix any signature differences of 11.1 (findings).
2. Compare `p508api::pantheon_small()` with the engineer's in-memory content; move `use_version` defaults of the references to 0.30.0 when goldens are re-recorded.
3. `golden --inputs DIR` (DIR: `pantheon-<seed>-512.db` seeds 0 to 9, `geo-seed-42-1024.db`) adds the world lines; check-world on validation worlds.
4. `history_report_fixture.hpp` and `p3_report_reference.py` must load schema 14 fixtures (god 6 columns, `faction`; H1 to H19 keep 3-column god rows) and render v11 (`[pantheon]`, `[cults]`, realm lines, 12.1's pinned deep events and `deep:` lines, `[sites]`/`[figures]` words): not done here; H20 and its expected lines are. H20 lacks an aid, a sealing, a counted flare and a take-up (16 item 11): these are in other fixtures; a hand fixture is needed for one report holding all.
5. Existing tests that change with 0.30.0 (do not change before): report header v10 to v11 (the files listed in p5-07-tests-notes); schema 13 to 14 and "this build reads 13"; generator 0.29.0; streams 107 to 120, kinds 99 to 110; `cli_history_settings_test.cpp` (`--divine-script ...: divine scripts are not supported yet`); P5-07's depths tests and `p5_07_checks.hpp` (tellings mod 9, aid after the outcome, first take's causes); `p3-03-chronology.txt` (god table); browser and markers goldens (group 9, glyphs).
6. Pre-existing: `p5_02` and `p5_03` `units` fail at HEAD (`unknown key race.dwarf.delving_permille`); `p5_04` `units` 3 stale failures. Not caused here.
