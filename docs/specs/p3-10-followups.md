# P3-10 Wording and site follow-ups

Status: **decided, ready for implementation (2026-11-20)**; corrected after the implementation, r1 (2026-11-21, section 10c); items 12 to 17 added the same day after Paul's decisions on the P3-09 review and on question 7 (section 10b): Paul accepted every recommendation of section 10 ("Yes I agree with the recommendations on all five"; `docs/design/proposals.md`, "Paul's decision of 2026-11-20: the P3-10 questions go with their recommendations"), and the lead accepted the grammar extension (question 3). Draft (written by the spec writer on `p3/10-spec`, cut from `origin/p3/09-int` with `origin/p3/09-audit` and `origin/main` merged). Phase P3 History core, a change spec after P3-09 (conflict balance, generator 0.18.0, implemented and in validation) and before P3-07 (the history report, specified, not implemented). Generator **0.19.0**. **No schema change**: schema stays 7; the change appends two event kinds (37, 38), stops writing two (4, 12), changes templates, saliences and lists (data), adds three race fields and two tuning fields (data), and extends the template grammar with one placeholder that was invalid before, as P3-08 did (section 8).

This spec is short on purpose. It lists every item, names the stage-spec section that holds its normative text (each stage spec stays the single source for its stage, amended with a new revision, a bounded new section and a revision note; nothing is renumbered), gives the order of implementation, the effect on output and goldens, the tests to add, the statistics to re-measure and the questions for the owner. Every figure is stated with the worlds it was measured on (section 6); figures measured at 0.17.0 are marked *(0.17.0)*, since P3-09 may move them.

## 1. Purpose

The world critic's review of P3-08 (`docs/reviews/p3-08-followups-world-critic.md`, generator 0.17.0) found the change acceptable with follow-ups: major plagues announced with small tolls (F1), abandonments with no stated reason (F2), quiet eras in blocks of identical lines (F3), faeries on the shore (F4), dwarves at sea level (F5), comic and near-identical names (F6), region names with a skewed plain-name mix, dense gloom and parish words (F7), and wording faults (F9). P3-10 collects the wording, data and site follow-ups, the two advisories of the P3-09 determinism audit (`docs/reviews/p3-09-conflict-balance-determinism-audit.md`), slow tests in CI, and three small items left by P3-08 and P3-09, under one version bump. Items 12 to 17 add six conflict rules from the world critic's review of P3-09 (`~/.cache/the-unmapped/out/p3-09-validation/review-p3-09-conflict-balance.md`, with the validation `docs/reviews/p3-09-conflict-balance-validation.md`), decided by Paul on 2026-11-20. Not here: other conflict rules (goblins starting most wars is accepted; human aggression stays 400), alliances, trade and wealth (P5, Paul's decision), history renaming regions and mountain peoples' own history (open proposals for P5), dwarven agency, and the P3-07 report command itself.

## 2. Items and where their normative text is

| # | Item (review) | Resolution, in short | Normative text | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Plague sentence carries the total (F1) | A new kind 37 `plague_outbreak` with values `dead`, `population`, `total`, `reached` replaces kind 12 for every outbreak (kind 12 has no value key for them and a shipped kind is never edited); `Plague broke out in X and spread to 3 other settlements; 1,084 died in all.` | `docs/specs/p3-05-peoples.md` r12 22.1; `docs/specs/p3-04-simulation-engine.md` r12 20.4 | yes |
| 2 | Abandonment names its cause (F2) | A renderer placeholder `{@causes:key=word,…\|before\|after}` names the kinds of an event's causes; kinds 10, 16 and 31 lead with `Weakened by famine and plague, …`; causes older than `cause_memory_years` (50) are not cited; a dwindling with no recent blow cites the colony it sent | P3-04 r12 20.2; P3-05 r12 22.2, 22.3 | yes |
| 3 | Quiet eras (F3) | One world-level `quiet_stretch` event (kind 38) per quiet phase row, with every living realm as participant and their count; generations of rulers derivable, printed by P3-07 in a realm's milestones; kind 4 no longer written | P3-04 r12 20.1; `docs/specs/p3-07-history-report.md` r12 22.2 | yes |
| 4 | Faeries off the shore (F4) | Race field `cell_weights` gains `port`, `habitability` and `inland` (a cell's distance from the sea, Chebyshev, capped); faeries `port` 0, `inland` 400 (`inland_cap` 20 cells, 320 m) and race `coast` −500, so they also lose the region seed cell as first site: 54% → 10% of faerie settlements within 300 m of the sea | P3-05 r12 22.5; `docs/specs/p3-01-history-data-settings.md` r11 | yes |
| 5 | Dwarves (F5) | Dwarves `port` 0 and `habitability` 500 in the site value (lowland counted at half when scoring a home site): dwarven settlements below 30 m 7 → 0 of about 160; region capacity unchanged (a lowland discount there cost a quarter of the dwarves' people and moved no more holds) | P3-05 r12 22.5; P3-01 r11 | yes |
| 6 | Region names (F7) | Data: church words out, two thirds of the grim words moved from `common` to `trait_barren`, `-hanger`, `High`, lowland `Gap`/`Cleft` out; rule V7: one use of a word stem per world; **one plain compass name per land noun**, kept by that noun's largest region (Paul's decision; 83% / 42% / 15% plain at 512 / 1024 / 2048 become 33% / 12% / 4%) | `docs/specs/p1-06-regions.md` r6 section 15, r7 15.7 | yes |
| 7 | Names (F6) | Eight filter entries (`suck`, `shiet`, `dead`, `hack`, `whop`, `flop`, `gord`, `gramma`); a core uniqueness rule: no two civilization or settlement names of a world whose cores (roots without `'` and `-`) are equal or one letter apart | `docs/specs/p3-02-names.md` r10 section 12 | yes |
| 8 | Wording (F9) | Thousands separators in rendered values (not in years); battle sentences without "0 of the victors"; moved seats and secessions without "its"; the flare-up's parties stay in P3-07's note, reworded (the one place); `note_reduction` only where people were lost and causes bounded by item 2's memory, so a dwindling no longer cites an unrelated old battle | P3-04 r12 20.3; `docs/specs/p3-06-expansion-conflict.md` r14 21.1, 21.2; P3-05 r12 22.2, 22.4; P3-07 r12 22.3, 22.4 | yes |
| 9 | P3-09 audit A1, A2 | A1: `flight_plan` copies its hop row; the cache's fixed outer size documented. A2: one last-refuge predicate in peoples | P3-06 r14 21.3, 21.4; P3-05 r12 22.7 | no |
| 10 | Slow tests in CI | Section 4 item 1 (the build engineer) | this spec, 4 | no |
| 12 | Aim-achieved endings (P3-09 critic F1; Paul's decision 1 of 2026-11-20 on the P3-09 review) | An attacker wins by aim only when it holds the goal; when the defender lost the goal to no one or to a third party, the war ends at once by the score with the last-battle rule (a truce on 0 or when the loser won the last battle); costs only for a winner by that rule | `docs/specs/p3-06-expansion-conflict.md` r14 21.6 | yes |
| 13 | Failed faerie searches (F2; decision 2) | A `search_abandoned` makes the region weary for every claimant for `search_weary_years` (500); no search in a region where the war has fought a battle (such a war ends by the ordinary rules, no second template); only the first `search_abandoned` of a world keeps salience 40, later ones `search_repeat_salience` 15, and their declarations leave the summary | P3-06 r14 21.7; P3-07 r12 22.5a | yes |
| 14 | Resettlement does not reset weariness (F3; decision 3) | A region's holder is its owner or, while it has none, its last owner; weariness (20.4, 20.23) and `wars_same_goal_unchanged_max` (20.28, P3-07 21.8 item 5) both count a change only when another civilization holds it | P3-06 r14 21.8; P3-07 r12 22.5b | yes |
| 15 | Orcs declare at shorter odds (F4; decision 4) | Race field `odds_min_permille` (orc 500, others 700) replaces `history.conflict.odds_min` | P3-06 r14 21.9; P3-01 r11 3.5 | yes |
| 17 | Orcs awaken with a quarrel (Paul's decision of 2026-11-20 on question 7) | Race field `awakening_quarrel` (orc 1): at its awakening a civilization of such a race gets strain at `quarrel_strain` (600, r1) toward the nearest standing settlement's owner of another race it can come to war with (disposition below `border_friction`) (within `enmity_hops` first, then by distance; ties by id; no draw); the existing claim rules do the rest; pending until another people stands; silent until the claim | P3-06 r14 21.12; P3-01 r11 3.5 | yes |
| 16 | A hidden refuge lies low (F6; decision 6) | While its refuge is hidden a civilization founds no colony (peoples hook `lie_low`) and raises no claim, strain or declaration; it still defends; its refuge grows to its share and waits | P3-06 r14 21.10; P3-05 r12 22.7a | yes |
| 11 | Left from earlier | Inline figures to golden files (4 items 2 and 3); kind 16 and kind 31's "were lost" can still occur and are kept (P3-05 r12 22.6) | 4 below; P3-05 r12 22.6 | no |

## 3. Order of implementation

One branch, one engineer and one test writer working from the stage specs separately. `GENERATOR_VERSION` becomes **0.19.0** in step 2 (the first step that changes a world byte); every golden is re-recorded once, in step 9. Intermediate commits may fail the world-golden tests; the merged branch must not.

| Step | What | Spec | Output |
| --- | --- | --- | --- |
| 0 | No output change, each its own commit checked byte-identical against 0.18.0's goldens: A1, A2 (P3-06 21.3, 21.4; P3-05 22.7); slow shards and CI (4 item 1) | P3-06 r14, P3-05 r12, 4 | none |
| 1 | Test housekeeping: the inline figures to golden files (4 items 2, 3) | 4 | none |
| 2 | Renderer: separators (P3-04 20.3); `{@causes:…}` (20.2) | P3-04 r12 | rendered text only |
| 3 | Catalogue and engine: kinds 37 and 38 with their report entries; `quiet_stretch` in Q6 (20.1) | P3-04 r12 20.1, 20.4 | `event` rows, catalogue tables |
| 4 | Peoples: kind 37 (22.1); `cause_memory_years` and the dwindle's colony cause (22.2); P3-06's `note_reduction` rule (21.1) | P3-05 r12, P3-06 r14 | history tables |
| 5 | Templates (data): kinds 10, 15, 16, 17, 22, 23, 31, 37, 38 | P3-05 22.3, 22.4; P3-06 21.2; P3-04 20.1 | templates; content hash |
| 6 | Sites: the `cell_weights` fields and the inland distance (22.5); race data | P3-05 r12 22.5; P3-01 r11 | sites, everything after |
| 7 | Names: filter entries (data) and the core rule | P3-02 r10 | names; content hash |
| 8 | Region names: the list edits (data), V7 and the keeper per land noun | P1-06 r6, r7 15.7 | `region.name`; content hash |
| 8a | Conflict rules: aim (21.6), searches (21.7), holder (21.8), odds per race with the race field (21.9, P3-01 3.5), lying low with the peoples hook (21.10, P3-05 22.7a), the awakening quarrel (21.12); P3-07 22.5a, 22.5b for the report's implementer | P3-06 r14 21.6–21.11 | history tables |
| 9 | Goldens and figures: re-record every golden of section 5; README hash; `docs/specs/schema.md` (kinds 37, 38) | 5 | — |

**Per role, for items 12 to 17:**
- *Engineer*: `conflict.cpp`, `conflict_war.cpp` (7.7's checks, the search's battle test and salience, `searched_until`, the holder in `futile_`, the odds minimum from the race, the awakening quarrel (`quarrel_pending`, strain seeding in T2), lying low in claims, strain and declarations, the `lie_low` call in the flight); `peoples.cpp` (`lie_low`, `low_until`, the skip in 9.3); `kinds_history.cpp` (race fields `odds_min_permille` and `awakening_quarrel`, `history.conflict` without `odds_min` and with `search_weary_years`, `search_repeat_salience`); `game/data/races/*.toml`, `game/data/history/conflict.toml` and the fixture copies; `digest.cpp`'s statistic if P3-07 is implemented by then.
- *Test writer*: `p3_conflict_reference.py` from P3-06 21.6–21.11 (and its `check_invariants`: (8) amended, (14), (15), (16), (12) with the holder), `p3_peoples_reference.py` for 22.7a, `p3_data_reference.py` for the fields; the golden `p3-06-conflict.txt`.
- *Validation*: the targets below, measured at 0.19.0 (section 9 items 9 to 13).

The test writer updates, from the specs and not from the C++: `p3_engine_reference.py` (P3-04 20.1–20.4), `p3_peoples_reference.py` (P3-05 22), `p3_conflict_reference.py` (P3-06 21), `p3_names_reference.py` and the history references' naming (P3-02 12), `p1_regions_reference.py` (P1-06 15), `p3_data_reference.py` (the new fields and lists).

## 4. Tasks without output change

1. **Slow tests in CI** (the build engineer; item 10). The `*-slow` doctest suites (`TEST_SUITE("<stage>-slow" * doctest::skip())`, eight suite names, `biomes-slow`, `chronology-slow`, `climate-slow`, `elevation-slow`, `history-slow`, `hydrology-slow`, `regions-slow` and `resources-slow`, in 33 `TEST_SUITE` blocks of 31 files of `worldgen/tests/` and `tools/worldgen-cli/tests/`) are skipped by the default ctest entries and went stale unnoticed (four stale cases fixed on `p3/09-slow`, 8d9447d). From this change:
   - `unmapped_add_doctest_shards()` (`cmake/UnmappedTestShards.cmake`) gains a form `SLOW <name> <cost> <suite> <patterns>`: CTest entry `<target>-<name>` running `<target> -ts=<suite> --no-skip -sf=<patterns>`, with the call's TIMEOUT, COST `<cost>` and CTest label `slow`. `<suite>` may be a doctest wildcard (`*-slow`). The existing SHARD and REST entries are unchanged (they skip the suites as before).
   - The partition check (`UnmappedCheckTestShards.cmake`) also counts the slow entries: every test case of a skipped suite (`--count --no-skip` minus `--count`) is in exactly one SLOW entry, and no SLOW entry is empty.
   - Default test presets exclude label `slow` (`"filter": {"exclude": {"label": "slow"}}` in `test-base`), so `ctest --preset <p>` runs what it runs today. Two new test presets, `linux-gcc-release-slow` and `windows-msvc-release-slow`, include only label `slow` (four jobs). SLOW entries are not registered when `UNMAPPED_SANITIZE` is set (never under `linux-clang-asan`).
   - CI: the `build` jobs of `linux-gcc-release` and `windows-msvc-release` run `ctest --preset <p>-slow` after the normal tests (inside the 180-minute job timeout).
   - The P3-09 merge added the shard `worldgen-tests-balance` (P3-09's balance tests; counted above); its slow cases join a SLOW entry like the others. No stream is added by P3-10 (53 names in `streams.hpp`, unchanged); kind ids continue after 36 (`refuge_held`), so 37 and 38 are free. Group the suites into SLOW entries of about equal cost (the build engineer measures each file on a release preset). The lead's measurement on a release build: 57 + 8 slow test cases (library and CLI), about 37 minutes serial; expected 12 to 15 minutes on the 4-vCPU runners. Update CLAUDE.md's shard table and README's test notes.
2. **The peoples slow test's inline figures** (`worldgen/tests/history/history_peoples_golden_test.cpp`, `forts == 5`, `ladder == 3`, P3-05 r6 6.2, 8.4): `p3_peoples_reference.py` writes a line `hist-peoples-42-1024 dwarf_kinds forts <n> ladder <m>` (the dwarven settlements of the case by their first census kind) into `worldgen/tests/golden/p3-05-peoples.txt`, and the test reads it. At 0.19.0 the values follow the new site rules (22.5); they are never edited by hand.
3. **The 1024 elevation hashes** (`worldgen/tests/stages_elevation_golden_test.cpp`, the slow cases "layer hashes at N = 1024, seeds 0 to 9" and "with the neutral settings, the 0.5.0 layer hashes", inline lists commented "Fourth version (0.6.0) values; the test writer's reference reproduces seeds 0, 1 and 3"): the test writer runs `p1_elevation_reference.py` for the seeds it can (a long job, in the background) and writes `elevation-1024 s<seed> <hash>` and `elevation-1024-neutral s<seed> <hash>` lines into `worldgen/tests/golden/p1-01-elevation.txt`; the test reads them and checks every seed that has a line. Seeds the reference has not computed are dropped from the test, not kept as recorded C++ values (CLAUDE.md "Golden values": never values written from the C++). No output changes (the elevation stage is unchanged since 0.6.0).
4. **Kind 16 and kind 31's "were lost"** (P3-08 validation F6): both still occur by the rules and are kept (P3-05 r12 22.6).

## 5. Effect on output: golden values

**Every world file changes**: new tuning and race fields and changed templates and lists change every `content_hash` (the P0 empty worlds too); `GENERATOR_VERSION` changes in `meta`. The empty world's size is unchanged (no table).

| Golden file | Re-recorded | What moves | What must not move (a difference is a defect) |
| --- | --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt` | yes, by the validation specialist after the cross-preset comparison | every case | — |
| `GOLDEN_CASES` (`db_determinism_test.cpp`), `stages_pipeline_test.cpp`'s hashes, README's `--seed-phrase "The Unmapped"` hash | yes | `content_hash`, names | — |
| `core_vectors.txt`, `p1-01-elevation.txt` … `p1-05-resources.txt` | no (except item 3 of 4, new lines) | — | every existing line |
| `p1-06-regions.txt` | yes, by the reference | the `region` table hash of 33 of its 43 cases (every case in which a land noun occurs twice or a name is drawn; P1-06 15.6 lists them) | the `region` layer, `region_border`, `region_biome` |
| `p3-01-data.txt` | yes | the data roots' content hashes | — |
| `p3-02-names.txt` | yes | names the new filter entries reject | names they do not reach |
| `p3-03-chronology.txt` | yes | `god` names the filter entries reach; `race_biome` unchanged | `race`, `culture`, `age`, the awakening years |
| `p3-04-engine.txt` | yes | the stub worlds' `quiet_stretch` events in place of `quiet_era` ones, every sentence line (separators) | the schedule cases, `phase` rows, the years and participants of the other events |
| `p3-05-peoples.txt` | yes | every history line | σ and C lines of every race but the faeries (22.5 changes the faeries' `coast`, which enters σ; capacity rules are unchanged) |
| `p3-06-conflict.txt` | yes | every history line | — |
| P3-07 report goldens | none yet | — | — |

`world-hashes.sh` keeps its fifteen cases; the history cases' `.report.txt` gain `events_plague_outbreak` and `events_quiet_stretch`.

## 6. Measurements

Method: the stored worlds of two sweeps and the test writers' references.

- **0.18.0 worlds**: 1024, default settings, seeds 0 to 39, generated by the spec writer from this branch (`linux-gcc-release`, `~/.cache/the-unmapped/out/p3-10-spec/w18/default/`), and `--civs-per-race 4` seeds 0 to 9 (`w18/civs4/`). Read-only analyses of their tables: `~/.cache/the-unmapped/out/p3-10-spec/an/`.
- **0.17.0 worlds**: the P3-08 validation sweep and the critic's 13 worlds (`~/.cache/the-unmapped/out/p3-08-validation/`).
- **Rule variants**: `~/.cache/the-unmapped/out/p3-10-spec/h10.py`, a harness on the P3-09 test writer's `p3_conflict_reference.py` at `--version 0.18.0`, which reproduces the 0.18.0 worlds row for row (checked: seed 42, 821 events, 103 settlements, every name); variants of the site value, capacity and name rule switched on by monkeypatching; 1024 default seeds 0 to 19 (seeds 0 to 11 for some name variants); about 17 s per world. Region names: `rn2.py` on `p1_regions_reference.py` over the stored region rows (P1-06 15.5). Names: `an/frag.py` on `p3_names_reference.py`. The harnesses are evidence, not references: the stage specs rule.

### 6.1 Log and wording (stored 0.18.0 logs, 1024 seeds 0 to 39, unless marked)

| Measure | Now | With P3-10 |
| --- | --- | --- |
| quiet lines | 1,640 `quiet_era` (median 40 per world) | 325 `quiet_stretch` (median 8) |
| quiet lines, `civs4` *(0.17.0, 20 worlds)* | 2,296 | 203 |
| quiet lines of 50 years or less | 340 | 48 |
| salience-45 outbreaks stating fewer than 1,000 dead | 370 of 760 | 0 (the sentence states the total, ≥ 1,000 for salience 45) |
| outbreaks that spread (to 1 / 2 / 3 / 4+ settlements) | 1,543 of 2,939 (955 / 388 / 157 / 43) | — |
| abandonments with their cause in the sentence | 0 of 23 | 23 of 23 |
| dwindles with a cause (a blow of the last 50 years / a colony / none) | 475 (any age) / — / 38 | 381 / 108 / 24 |
| events with a value of 1,000 or more (separators) | 10,030 of 27,971 | — |
| battles with `winner_dead` 0 | 1 of 3,078 | 0 sentences with "0 of" |
| moved capitals / secessions of a plural realm (`The Warrens of …`) | 30 of 181 / 24 of 82 | no "its" |

### 6.2 Faeries and dwarves (variants, 1024 default seeds 0 to 19)

P3-05 r12 22.5 has the full table (variants one by one). As shipped (faeries `port` 0, `inland` 400, `coast` −500; dwarves `port` 0, `habitability` 500; the name rule on):

| Measure (20 worlds) | 0.18.0 | P3-10 |
| --- | --- | --- |
| faerie settlements within 300 m of the sea | 99 of 181 (54%) | 22 of 196 (11%) |
| within 48 m (the port radius) | 46% | 4% |
| faerie settlements in temperate forest | 92% | 84% |
| faerie standing settlements; population (medians) | 6; 4,351 | 8; 5,824 |
| dwarven settlements below 30 m (of them fortresses) | 7 (5) of 158 | 0 (0) of 166 |
| dwarven median elevation rank; in the lowest quarter of the region | 0.73; 41 | 0.88; 10 |
| dwarven standing settlements; population (medians) | 8; 18,784 | 8; 18,595 |
| standing settlements, all worlds: elves, goblins, dwarves, faeries, humans, orcs | 185, 861, 163, 161, 293, 116 | 205, 809, 207, 169, 248, 110 |
| events (median); declarations (sum) | 724; 479 | 690; 476 |

The faerie and dwarven figures meet the critic's targets (fewer than 20% of faerie homes within 300 m; no "Deep" at sea level). In these 20 harness worlds humans lost 15% of their standing settlements (9% of their people); **measured at the implementation** (`origin/p3/10-impl` f5caea9, 1024 default seeds 0 to 39, with every P3-10 rule) humans have **3% more** standing settlements than at 0.18.0, so the harness figure was the reshuffle of keyed war draws, and faerie settlements within 300 m of the sea are **11%** (0.18.0: 55%). None of the 0.18.0 dwarven realms is mostly lowland (0 of 28; the critic's 9 of 144 was *(0.17.0)*).

### 6.3 Names

P3-02 12.1 and 12.2 have the tables. Filter entries: about 0.24% of drawn names change (141 of 59,400 drawn with the shipped lists). Core rule (1024 default seeds 0 to 19, 2,222 civilization and settlement names): pairs with equal cores 14 → 0, pairs one letter apart 123 → 0, names redrawn 383 → 471, 0.07 more attempts per name, no fallback.

### 6.4 Region names

P1-06 15.5 and 15.7 have the tables. With the keeper per land noun (15.7): plain names 33% / 12% / 4% at 512 / 1024 / 2048, drawn names 63 → 261, 2,863 → 4,328, 837 → 951, attempts per drawn name 0.07 (max 1), 0.14 (max 6), 0.75 (max 11), no fallback, grim drawn names 11% / 9% / 7%. Without it (r6 alone): grim drawn names 13% → 8% at 1024 and 14% → 6% at 2048, church words 72 → 1 (a false match), repeated stems 75 → 26 (1024, 100 worlds) and 145 → 41 (2048, 5 worlds), attempts per drawn name 0.11 (max 4) and 0.64 (max 9), no fallback.

## 7. Tests to add

Each stage spec lists its own; in summary:

- P3-04 20.6: quiet stretches (participants, `realms`, `years`, no `quiet_era`); `{@causes:…}` (lists, joins, empty, malformed bodies); separators.
- P3-05 22.8: kind 37's values and sentence; the cause memory at 49, 50, 51 years; the dwindle's colony cause; templates of 10, 15, 16, 17, 31; the site terms of 22.5.
- P3-06 21.12: the awakening quarrel (neighbour choice, strain seeding, pending case, secessions excluded).
- P3-06 21.11: aim by holding the goal, the search's weariness, battle test and salience, the holder, the odds per race, lying low; invariants (8), (14), (15).
- P3-06 21.5: `note_reduction` only with a loss; the battle sentence with (4, 0), (37, 1), (491, 14), (0, 0); A1 and A2 byte-identical.
- P3-07 22.6: the generations note; the reworded flare-up note; the new `events_` lines.
- P3-02 12.3: the filter entries; the core rule (equal, distance 1, fallback, gods unaffected); invariant 6.
- P1-06 15.6: the list edits; V7; the keeper per land noun.
- Build: the partition check counts SLOW entries; `ctest --preset linux-gcc-release` lists no slow entry, `ctest --preset linux-gcc-release-slow -N` lists only them.

## 8. Schema

**No schema change** (schema version 7; `docs/specs/schema.md` gains kinds 37 and 38 in its catalogue section, as for every appended kind):

- **Kinds 37 `plague_outbreak` and 38 `quiet_stretch`** are appended (P3-04 8.2: appending a kind is not a schema change). Kinds 4 `quiet_era` and 12 `plague` keep their structure and rows in every catalogue and are no longer written; nothing about them changes meaning.
- Item 1 asked whether the totals can be stored as event values "without a schema change". Not on kind 12: adding a value key to an existing kind is a schema change by P3-04 8.2, hence the new kind.
- **`{@causes:…}`** (P3-04 20.2) is a renderer extension like P3-08's three-word form: no table, column, code list or kind structure changes; every earlier template renders as before (but for separators, a rendering rule). A world of 0.19.0 is refused by `HistoryView::load` of 0.18.0 or earlier; no shipped program of those versions renders sentences. Should the lead read a template-grammar extension as a contract change, the alternative is to drop the cause clause and leave the cause to P3-07's companions (a shown abandonment brings its causes), which needs no grammar change (question 3).
- New data fields (`cell_weights.port`, `.habitability`, `.inland`, `history.peoples.cause_memory_years`, `inland_cap`, race `odds_min_permille`, `awakening_quarrel`, `history.conflict.search_weary_years`, `search_repeat_salience`; `history.conflict.odds_min` removed) change `content_hash` only. Items 12 to 17 add no event kind: a lost goal ends with the existing `peace`, `war_won` or `truce`, and the repeated search keeps kind 35 with a lower salience.

## 9. Statistics to re-measure (P3-10's validation)

On the sweeps of P3-06 15.6 at 0.19.0, against section 6 and P3-09's acceptance targets T1–T7 (which must still hold):

1. Quiet lines per world and per quiet row (one), `realms` against the living civilizations.
2. Outbreaks: `total` equals the sum of the outbreak's and its spreads' `dead` (invariant 13); salience-45 outbreaks with `total` < 1,000 (must be 0).
3. Abandonments and dwindles with a cause clause; causes older than 50 years (must be 0 for reductions and decline starts).
4. Faeries (and the watch item of decision 5): share of settlements within 300 m (18 cells, Chebyshev) of the sea (target below 20%), forest share, standing settlements and population, worlds with one settlement; humans' standing settlements and population against 0.18.0 (the harness's −15% was not borne out: +3% at the implementation's sweep).
5. Dwarves: settlements below 30 m, fortresses below 30 m, median elevation rank, realms mostly in the lowest quarter of their region, standing settlements, population, cities.
6. Names: equal-core pairs (must be 0), pairs one letter apart, redraws per name, fallbacks; filtered names (P3-02 invariant 5).
7. Region names: P1-06 15.5's and 15.7's measures at 512, 1024 and 2048: plain share, attempts, fallbacks (must be 0), grim share, repeats; one keeper per land noun in every world (invariant 6 as restated).
8. The summary: per-age floor and threshold (P3-07 21) with kinds 37 and 38.
9. **Aim** (item 12): invariants (8) as amended and (14) in every world; endings by the score of P3-06 21.6 item 2 by outcome (estimate at 0.18.0, 40 worlds: of 141 endings with the goal lost to no one or a third party, 132 attacker wins by the score, 7 truces, 2 defender wins, 9 costs no longer paid).
10. **Searches** (item 13), targets: median `search_abandoned` per 1024 default world **≤ 3** (r1 below; 0.18.0: 4.5 at validation; measured at the implementation: 3, max 8); none in a region where its war fought a battle; at most one per world in the default summary.
11. **Weariness by holder** (item 14): `wars_same_goal_max` (any owner) per world against 0.18.0 (critic: up to 11); seed 42's Wood of Stags: at most 3 wars after 21.8 (0.18.0: 11); invariant (12) and T2 with the holder definition.
12. **Orcs** (item 15), target **T8**: orcs declare at least one war in ≥ 90% of 1024 default worlds where they are present (0.18.0: 76 of 100; harness at orc 500: 16 of 20, against 15 at 700); **T5** kept (harness: 24 won, 18 lost). T8 is a real target with item 17 (the awakening quarrel); if T5 breaks, validation tries orc `odds_min_permille` 600, then 700, first (P3-06 21.12).
13. **Lying low** (item 16): invariant (15), no colony, claim or declaration by a civilization with a hidden refuge (0.18.0: 7 of 46 flights followed by a declaration).
14. **The awakening quarrel** (item 17): invariant (16); orc claims in their first active phase; T8 and T5 above.

## 10. Decisions (questions to Paul, closed 2026-11-20)

Paul accepted every recommendation ("Yes I agree with the recommendations on all five"; `docs/design/proposals.md`, "Paul's decision of 2026-11-20: the P3-10 questions go with their recommendations"); the lead accepted the grammar extension. Each question is **decided** as the rule now written:

1. **Decided: one plain compass name per land noun**, kept by that noun's largest region, ties to the smaller id; every other region draws a name (P1-06 r7 15.7, replacing the keeper per base name of Paul's decision of 2026-11-19). Plain names 33% / 12% / 4% at 512 / 1024 / 2048; no fallback with V7.
2. **Decided: one world-level `quiet_stretch` per quiet phase row** (P3-04 20.1); generations of rulers in P3-07's milestones.
3. **Decided (the lead): the `{@causes:…}` placeholder** for the cause clause (P3-04 20.2).
4. **Decided: thousands separators from 1,000, in sentences only**, never in years or the report's own numbers (P3-04 20.3).
5. **Decided: the faerie and dwarven site data as specified** (P3-05 22.5). **Watch item for validation**: humans' standing settlements and population against 0.18.0 (the harness estimated −15%; the implementation measured +3%, section 6.2); the lead follows it in validation.
6. **Decided: `Mammga` and "The Twiwhiwou Ring" stay unfiltered** (P3-02 12.1).

## 10b. Decisions of 2026-11-20 on the P3-09 review and on question 7

Paul's decision ("let's take your recommendation for 1-4 and 6. On 5 it's ok for goblins to start most wars, do not increase human aggression"; `docs/design/proposals.md`, "Paul's decision of 2026-11-20 on the six questions from the P3-09 review"): items 12 to 16 above; goblins starting most wars is accepted and human `aggression` stays 400. The rule details (the score ending of a lost goal, `search_weary_years` 500, no search after a battle in the region, salience 15 for repeats, the holder reading, orc 500, the lying-low hook) are the spec writer's and were measured where section 9 says.

7. **Orcs that never quarrel: answered.** Paul, 2026-11-20: "having the orcs start with a quarrel with a neighbor is reasonable" (`docs/design/proposals.md`, "Paul's decision of 2026-11-20: orcs awaken with a quarrel"). Specified as item 17 (P3-06 21.12), shipped with 21.9's orc 500, not held back until T8 is measured. T8 and T5 are targets for validation at 0.19.0; the tuning to try first if T5 breaks is in P3-06 21.12.

## 10c. Corrections after the implementation (r1, 2026-11-21)

The engineer's report on `origin/p3/10-impl` (f5caea9) and the lead's rulings; each fixed in the stage spec named.

1. **T8 missed** (29 of 40 worlds with an orc war, 72%; T5 met, 44 won, 31 lost): P3-06 r15 21.12. The quarrel neighbour is the nearest people the orcs can come to war with (disposition below `border_friction`, so never a people of their own camp; not a hidden refuge), and the quarrel strain is the new `history.conflict.quarrel_strain` **600** (check: ≥ `claim_strain`), which gives a declaration chance of 75‰ a year before odds at once (it was 0 at `claim_strain` 300 = `declare_min`). 7.2's choice of the declaration candidate is not changed (21.12 item 3a). If T5 breaks, validation tries `quarrel_strain` 450, then orc `odds_min_permille` 600, then 700.
2. **Searches**: the target is median ≤ 3 (P3-06 r15 21.7); no rule change.
3. **Template length**: `report_event.template` is string(512) (P3-04 r13 4.2), as implemented.
4. **`quiet_stretch` participants**: at most 64, the most populous realms; `realms` counts them all (P3-04 r13 20.1, test 20.6 item 0).
5. **The engineer's readings**, confirmed in the text: the quiet-step ending writes `peace` when the loser is exhausted, else `war_won`; aim needs only that the goal's owner is the attacker (P3-06 21.6); the search weariness is applied in the allowed goals (21.7); "lies low" means a standing settlement y owns now is still hidden, and only y's own strain is held (21.10); hidden refuges are not quarrel candidates, unreachable hops are +∞ (21.12); the core rule belongs to the history stage's registry (P3-02 r11); the defaults of hand-made tuning (`cause_memory_years` 100000, `inland_cap` 20, `search_weary_years` 0, `search_repeat_salience` 40, `quarrel_strain` 600) leave each rule off or as at 0.18.0 (P3-05 r13, P3-06 r15); P3-07 22.2's generations note is the P3-07 implementer's (P3-07 r13).
6. **Measured facts** in place of estimates: humans +3% standing settlements (6.2), faeries 11% within 300 m of the sea (6.2), plain region names 12% at 1024 and 33% at 512, as predicted (P1-06 15.7).

**Per role:**
- *Engineer*: P3-06 21.12 items 1 (the disposition test and the hidden-refuge exclusion) and 2 (`quarrel_strain`, its field, range and check, in `game/data/history/conflict.toml`, the fixtures and `kinds_history.cpp`); nothing else changes in code (items 3 to 5 confirm what is built).
- *Test writer*: `p3_conflict_reference.py` for 21.12 items 1 and 2 and the new tests of 21.12 item 6; the conflict golden and the world goldens again; `p3_data_reference.py` for the field.
- *Validation*: T8 and T5 with r15, and the count of 21.12 item 3a; the search median against ≤ 3.

## 11. Proposals

Two entries in `docs/design/proposals.md`, both accepted with Paul's decision entry of 2026-11-20: "Region names: the plain compass name, a second look" (question 1) and "History: one quiet line for the world" (question 2). The other items are wording and data within earlier decisions.
