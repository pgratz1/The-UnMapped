# P3-08 Names and peoples follow-ups

Status: **draft for the lead's approval, 2026-11-20; r1 the same day, corrections after the implementation (section 11)** (written by the spec writer on `p3/08-spec`, cut from `origin/p3/06-final` with `origin/p1/06-region-names` and `origin/p3/02-names-filter` merged). Phase P3 History core, a change spec after P3-06 (expansion and conflict, generator 0.16.0). Generator **0.17.0**. **No schema change**: schema stays 7, no table, column, code list or kind structure changes; the change appends one event kind (31), changes stored values, templates and saliences (data), and extends the template grammar with a form that was invalid before (P3-04 8.2's list of what is a schema change does not include either; 8 below). Conflict *rule* changes (last refuge, revenge claims, war goals, orc agency, faerie cover, P3-07's summary selection) are not here: they are P3-09, written in parallel.

This spec is short on purpose. It lists every item, names the stage spec section that holds its normative text (each stage spec stays the single source for its stage, amended with a new revision and a revision note), gives the order of implementation, the effect on output, the tests to add and the statistics to re-measure. The numbers come from the spec writer's measurements of section 6, with the seeds they come from.

## 1. Purpose

Collect, for one engineer and one test writer, the changes already decided or drafted since P3-05 and P3-06 that change names, sentences and the peoples' rules, so that they ship together under one version bump: region names without numerals, a lower-case "the" inside sentences, the name filter, the faerie forest weights, the follow-ups of the P3-05 world critic, the wording asked by the P3-06 world critic, the P3-06 audit's two notes, the P3-06 validation's findings and some housekeeping.

## 2. Items and where their normative text is

| # | Item | Resolution | Normative text | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Region names without numerals | P1-06 r3 ships unchanged; r4 adds `boner` and `dung` to `common.forbidden` (data, committed: `game/data/placenames/common.toml`), registers the stream `geo.regions.name` after `geo.regions.cost` (a gap of r3), and checks the rest complete | `docs/specs/p1-06-regions.md` r3 (RG11, 4.1, 14), r4 (14.8); `docs/specs/p0-01-deterministic-core.md` 20 | yes |
| 2 | Lower-case "the" mid-sentence | P1-06 r3 14.7 / Appendix B applied: `render_event` writes a name's leading `The ` as `the ` unless at a sentence start; names are stored capitalised; P3-07 prints the sentence unchanged and every name outside sentences capitalised | `docs/specs/p3-04-simulation-engine.md` r10 8.5; `docs/specs/p3-07-history-report.md` r7 6.1 | sentences only (no world byte) |
| 3 | Name filter, god style, elven realm form | P3-02 r5 ships (data committed: `game/data/name_filters/`); r6 states that every word of the rendered name is covered (the root word by the rule at every attempt, the fixed form words by the stage check `names_form_text`, which now checks every fixed word), why the test is not across spaces, and how a rejected name is redrawn (the next keyed attempt); the sweep scan of section 6.5; the seven questions of P3-02 11.10 stay open | `docs/specs/p3-02-names.md` r5 11, r6; `docs/specs/p3-01-history-data-settings.md` r8 (3); `docs/specs/p3-03-chronology.md` r3 | yes |
| 4 | Faerie forest weights | candidate A ships (temperate forest and wet woodland 800, fen 300, marsh 200, highland moor 100, heath 0, grassland −300, alpine and steppe −400) | `docs/specs/p3-05-peoples.md` r6 12.3; P3-01 r8 (2) | yes |
| 5a | Dwarves found in uplands | a per-race cell term in the site choice (new race field `cell_weights`, dwarves ore 400, height 600) and no seed-cell rule for such a race; a dwarven settlement off the walkable ground is a fortress | P3-05 r6 6.2, 8.4; P3-01 r8 (1) | yes |
| 5b | Quiet-era merging and log volume | one `quiet_era` per civilization and quiet phase row (its own events no longer close it); the `quiet_era` sentence without "(0 generations)"; `famine_ppm` 2500 → 1250 and `plague_spread_permille` 200 → 100 (data) | P3-04 r10 6.6, 4.2; P3-05 r6 10.2, 12.1 | yes |
| 5c | Salience of plague and succession crisis | an outbreak keeps salience 45 only when it and its spreads killed 1,000 or more (25 otherwise); a crisis that does not split has 25 (40 when it splits); the crisis sentence names the capital | P3-05 r6 10.2, 10.3, 11, 12.1 | yes (saliences, templates) |
| 5d | A people's last settlement is not abandoned by chance | the frail chance of 8.5 (a) is not drawn for a civilization's only standing settlement; the deterministic rule below `abandon_min` stays | P3-05 r6 8.5 | yes |
| 5e | The faerie cap drain | T1's target is min(share, kind cap), so refugees above a kind cap drain at `decline_permille` | P3-05 r6 9.1 | yes |
| 5f | Abandonment causes and destinations | causes gain the decline start; a new kind 31 `settlement_abandoned_to` names the destination (values `settled`, `lost`); kind 16 is kept for survivors with nowhere to go, with a new sentence | P3-05 r6 8.5, 11; `docs/specs/schema.md` (catalogue, the engineer) | yes |
| 6 | Wording of the P3-06 critic (F8, suggestion 7) | the capture sentence drops "and 0 fled" with a three-word value form `{fled?w0\|w1\|w2}`, `#` for the value (P3-04 8.4 grammar extension); battle losses: the victors lose `loss_winner` of the smaller force | P3-04 r10 8.4, 8.5; `docs/specs/p3-06-expansion-conflict.md` r7 7.3, 11 | yes |
| 7 | P3-06 audit notes A1, A2 | A1: casualties can empty settlements under legal non-shipped tuning; stated with what follows (no rule change). A2: a loser's same-step Q4 colony in a ceded region stays the loser's; stated (no rule change) | P3-06 r7 7.4, 8.4 item 3, 8.3 item 4 | no |
| 8 | P3-06 validation findings | F1: the peoples-only prefix of the log, with the exception of the eras open in the first claim's step (16.2 item 18a); F3: 15.3 and 15.5 gain the 100-world figures and typical ranges at the 5th–95th percentiles; F7: same-year relation rows also in `calm` and `span12000`; D1: P3-05 invariant 6 reads "a kind built by a race that has owned the settlement"; F2: `generate --help` text | P3-06 r7 8.3, 15.3, 15.5, 16.2; P3-05 r6 14 item 6; `docs/specs/p0-04-worldgen-cli.md` amendment 22 | no (D1: the test writer's reference; F2: help text) |
| 9 | Housekeeping | P3-05 figures gain the validation's seeds 0–99; split the long CTest entries; the `CAPTURE` print fix; the P3-04 engine microbenchmark | P3-05 r6 15; section 4 below; P3-04 r10 14.3 | no |

Not adopted, with the reason (the reviews proposed them as options): a cumulative-loss condition for the chance abandonment (P3-05 critic F4; the destination and the second cause make the line legible, and the last-settlement rule removes the worst case); a salience bonus for a civilization's first town and city (F3; it would compete with P3-09's per-age floor for the summary); a regional famine event (F2 item 3; famine repeats in a settlement are rare, median gap 588 years at 0.16.0, so merging per region or per settlement would remove few lines; 6.4); a famine rest per generation (same reason); a faerie cell term (5a's term is available as data; the region weights of item 4 already put every faerie home in forest).

## 3. Order of implementation

One branch, one engineer, one test writer working from the stage specs separately. The steps are in the order below so that each can be checked against the test writer's updated reference before the next; `GENERATOR_VERSION` becomes **0.17.0** in step 1 (the first step that changes a world byte) and every golden is re-recorded once, in step 9. Intermediate commits may fail the world-golden tests; the merged branch must not.

| Step | What | Spec | Output |
| --- | --- | --- | --- |
| 0 | Housekeeping: CTest split, `CAPTURE` fix, engine microbenchmark, `generate --help` text | 4 below; P3-04 r10 14.3; P0-04 22 | none |
| 1 | Region names: kind `placename` (shipped files and both fixture copies), the two `worldgen.regions` fields, stream `geo.regions.name`, RG11 r3 | P1-06 r3, r4; P0-01 20 | `region.name`, every `content_hash` |
| 2 | Renderer: names inside a sentence, the three-word value form | P3-04 r10 8.4, 8.5; P3-07 r7 | rendered text only |
| 3 | Name filter, `god_patterns`, `naming_realm_forms`, the elven `The House of {root}`, the stage check `names_form_text` | P3-02 r5 11, r6; P3-01 r8; P3-03 r3 | god, civilization and settlement names; `content_hash` |
| 4 | Quiet eras: one per civilization and quiet row; the `quiet_era` template | P3-04 r10 6.6, 4.2 | `event` rows of peoples and conflict worlds (stub worlds: sentences only) |
| 5 | Peoples data: faerie weights, race field `cell_weights` (all six race files and the fixtures) | P3-05 r6 12.3, 6.2; P3-01 r8 | `race_biome`, sites, everything after |
| 6 | Peoples rules: cell term and seed rule (6.2), fortress cell rule (8.4), last settlement (8.5 (a)), causes and destination with kind 31 (8.5, 11), cap drain (9.1), plague and crisis saliences (10.2, 10.3, new fields of 12.1), the halved famine and spread rates (12.1, data), templates of kinds 14 and 16 | P3-05 r6 | history tables |
| 7 | Conflict: battle losses from the smaller force, the capture template | P3-06 r7 7.3, 11 | history tables |
| 8 | Text only: P3-06 7.4, 8.3, 8.4, 15, 16.2 item 18a property test; P3-05 14 item 6 in the reference | P3-06 r7; P3-05 r6 | none |
| 9 | Goldens and figures: re-record every golden of section 5, the README hash, `docs/specs/schema.md` (kind 31) | 5 | — |

The test writer updates, from the specs and not from the C++: `p1_regions_reference.py` (RG11 r3, r4), `p3_data_reference.py` (kinds `placename` and `name_filter`, the race field), `p3_names_reference.py` (P3-02 11.3, 11.4, r6), `p3_chronology_reference.py` (god names), `p3_engine_reference.py` (8.4, 8.5, 6.6), `p3_peoples_reference.py` (P3-05 r6) and `p3_conflict_reference.py` (P3-06 r7, and D1's reading of P3-05 invariant 6 in `check_invariants`).

## 4. Housekeeping tasks (no output change)

1. **CTest entries** (`worldgen/tests/CMakeLists.txt`, `unmapped_add_doctest_shards`; the build engineer or the test writer). `worldgen-tests-chronology` was the longest entry on four presets at 0.16.0 (1,696 s under `linux-clang-asan` at load 10–13, 47% of its 3,600 s timeout; 934 s at lower load) and `worldgen-tests-engine-worlds` the second (978 s; it is one file, `history_engine_golden_test.cpp`, 11 test cases). Split `engine-worlds` into two files by case (the 512 and synthetic cases in one, the 1024 cases in the other, so that each holds about half the run time) with a `SHARD` line each, and split `chronology` (six files) into two groups whose run times are within a factor of two of each other: measure each file first with `worldgen-tests -sf=*<file>` on a release and on the asan preset. Acceptance: on a quiet machine no `worldgen-tests-*` or `worldgen-cli-tests-*` entry above 25% of its timeout under `linux-clang-asan`; the partition entries pass (every test case in exactly one group). Update the shard table in `CLAUDE.md` with the new entry names.
2. **`CAPTURE` of C strings** (the P3-05 validation's item 6, still open). `CAPTURE(c.name)` where `name` is a `const char*` prints `c.name := 1` in some doctest builds. Replace with `CAPTURE(std::string_view(c.name))` in `worldgen/tests/db_determinism_test.cpp` (the reported case, line 382) and in every other `CAPTURE` of a `const char*` member (a grep for `CAPTURE(` over `worldgen/tests/` and `tools/worldgen-cli/tests/`; `history_conflict_golden_test.cpp`'s `want[i].name` is one).
3. **The engine microbenchmark** of P3-04 14.3, never written: specified in P3-04 r10 14.3 (suite `engine-bench`, skipped by default).
4. **P3-05 figures**: done in P3-05 r6 15 (the validation's seeds 0–99).
5. **`generate --help`**: P0-04 amendment 22.

## 5. Effect on output: golden values

**Every world file changes**: the data roots gain the required kinds `placename` and `name_filter`, the race field `cell_weights`, the culture field `god_patterns`, new tuning fields and changed templates, so every `content_hash` changes, including the P0 empty worlds; `GENERATOR_VERSION` changes in `meta`. The empty world's size is unchanged (no table; 34 pages).

| Golden file | Re-recorded | What moves | What must not move (a difference is a defect) |
| --- | --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt` | yes, by the validation specialist after the cross-preset comparison | every case (`content_hash`; geography and history cases also names and history) | — |
| `GOLDEN_CASES` (`db_determinism_test.cpp`), `stages_pipeline_test.cpp`'s empty and pattern hashes, README's `--seed-phrase "The Unmapped"` hash | yes | `content_hash` | — |
| `core_vectors.txt` | no | — | everything |
| `p1-01-elevation.txt` … `p1-05-resources.txt` | no | — | every layer and table hash |
| `p1-06-regions.txt` | yes, by the reference (r3 adds `names` and `name_attempts` lines) | the table hash of `region` where a base name repeats: the shipped-data cases 512 seed 0, 512 seed 42, 512 phrase and 1024 seed 1, and three N ≤ 128 cases (`designed-0-lakemerge`, `shipped-128-s0-t`, `shipped-128-s42-t`; r1) | the `region` layer, `region_border`, `region_biome`, hand, wt, owners, seeds |
| `p3-01-data.txt` | yes | content hashes of the three data roots | — |
| `p3-02-names.txt` | yes, by the updated reference | names (filter, god patterns, realm form); the r5 scenario lines | the keyed-draw vectors of 4.1 that involve no filter (P3-02 11.7 item 7: with empty lists and `god_patterns` = `patterns` every r4 name is reproduced) |
| `p3-03-chronology.txt` | yes | table `god` (names), `race_biome` (faerie rows) | `race`, `culture`, `age` (no input of theirs changes), the awakening years |
| `p3-04-engine.txt` | yes | stub worlds' `civilization` and `settlement` names, every `e<n>` sentence line (region names, "the" rule, the `quiet_era` template) | the schedule cases (no tension, no names), the stub worlds' `phase` rows and event years, kinds and participants |
| `p3-05-peoples.txt` | yes | every history line | the per-region σ and C lines (`score`) except faerie rows (new weights) and the homes of every race but faeries where faerie homes do not take another race's region (the test writer compares and explains any other change) |
| `p3-06-conflict.txt` | yes | every history line, `hist-conflict-42-1024` | — |
| P3-07 report goldens | none yet (P3-07 not implemented) | — | — |

**`world-hashes.sh`** keeps its fifteen cases. Its history cases' `.report.txt` files change with the history.

## 6. Measurements

Method: the test writers' exact-integer references of 0.16.0 (`worldgen/tests/reference/p3_conflict_reference.py` on `p3_peoples_reference.py`, which reproduce the 0.16.0 worlds' table hashes; checked on 1024 seed 42), with the P3-08 rules switched on by a harness, `~/.cache/the-unmapped/out/p3-08-spec/p308.py`, run on the geography and chronology of the 0.16.0 validation sweep worlds `~/.cache/the-unmapped/out/p3-06-validation/sweep/default/` (1024, default settings, seeds 0–49; region names do not affect the simulation, P1-06 14.2). Variants: `base` (the 0.16.0 rules, reproducing the stored worlds), `fae` (item 4 alone), `dwarf` (5a alone), `loss` (the battle-loss rule alone), `all` (items 4, 5a, 5b eras, 5d, 5e and the loss rule together), and the volume variants of 6.4. Run times: about 10 s per world and variant, four at a time. The harness is evidence, not a reference: the stage specs rule. Draws keyed by event ids (a war's draws are keyed by its declaration event id, P3-06 8.3 item 5) move when the log above them changes length, so per-world differences between variants are partly reshuffling; the medians over 50 worlds are what to read.

### 6.1 Whole worlds (1024, seeds 0–49; min / median / max)

`all` here is every rule change at the 0.16.0 famine and spread rates; 6.4 adds the shipped halvings of those rates (`all + both`).

| | base (0.16.0) | all (P3-08) |
| --- | --- | --- |
| events | 439 / 805 / 1,422 | 451 / 777 / 1,440 |
| `quiet_era` events | 53 / 85.5 / 120 | 15 / 36.5 / 72 |
| eras of 50 years or less | 18 / 33.5 / 53 | 0 / 7 / 23 |
| eras with 0 new generations | 4 / 16.5 / 33 | 0 / 2.5 / 15 |
| median era length (years) | 50 / 75 / 125 | 149 / 425 / 690 |
| famine + plague + spread + quiet era, ‰ of the log | 272 / 441 / 558 | 251 / 388.5 / 523 |
| wars declared | 7 / 23 / 48 | 7 / 25 / 62 |
| sacked | 2 / 12 / 32 | 0 / 13.5 / 38 |
| captured | 0 / 5.5 / 20 | 1 / 7 / 26 |
| civilizations ended | 0 / 1 / 3 (40 in all) | 0 / 1 / 3 (45) |
| battles in which the victors lost more | 1,679 of 4,138 | 0 of 4,645 |
| captures with `fled` 0 (the sentence now ends at the dead) | 327 of 335 | 419 of 423 |
| standing settlements above their kind cap at the present | 6 | 2 |
| standing: elves / goblins / dwarves / faeries / humans / orcs (medians) | 12 / 31.5 / 6.5 / 4 / 14 / 5 | 9.5 / 32.5 / 5.5 / 4 / 16 / 5 |
| worlds with elves ≤ 1 standing; faeries ≤ 1 | 1; 9 | 2; 8 |
| per-age events (medians, ages 1–6) | 33, 81.5, 96.5, 124.5, 147.5, 314.5 | 22, 71.5, 89, 112.5, 139, 336 |

The early ages are thinner with merged eras, as intended (their lines were mostly eras), and the orc age keeps its share.

### 6.2 Dwarves (5a; 1024 seeds 0–49, all dwarven settlements founded)

| | base | `dwarf` alone | all |
| --- | --- | --- | --- |
| settlements founded | 350 | 377 | 360 |
| median elevation rank in their region (share of the region's dry cells lower) | 0.21 | 0.67 | 0.72 |
| fortresses | 259 (74%) | 307 (81%) | 303 (84%) |
| on a cell with `walk_area` 0 | 159 | 253 | 251 |
| on a rich-ore cell (`ore` ≥ 128) | 56 (16%) | 224 (59%) | 219 (61%) |
| dwarven cities standing at the present | 14 | 7 | 6 |
| dwarven standing settlements (median); population (median) | 6.5; 16,085 | 6.5; 15,044 | 5.5; 14,209 |
| wars (median) | 23 | 23 | 25 |

Seed 42 (`dwarf` against `base`): the Hold of Dulimin's capital, a city of 7,212 at 91 m (rank 0.13), is a town of 6,243 at 422 m (rank 0.63); the Narinthri Throne's capital moves from 1,017 m (rank 0.26) to 1,441 m (rank 0.93); a third hold stands at 2,052 m. The dwarven holds of seed 3 move from 38–257 m to up to 1,779 m. The other races' ranks are unchanged (elves 0.17, goblins 0.21, humans 0.15–0.16, orcs 0.15); faeries fall from 0.12 to 0.07 with item 4 (the lowest, wettest woods).

### 6.3 Faeries (item 4; 1024 seeds 0–49, `fae` alone)

Faerie standing settlements 1 / 5 / 18 (base 1 / 4 / 16), population 417 / 3,290 / 13,956 (295 / 2,639 / 14,608); worlds with one faerie settlement or none 5 (9); wars, sacks and the other races within a few per cent. This matches P3-05 12.3's prototype measurement (faerie population median 3,321 with conflict, standing 5).

### 6.4 Log volume of famine and plague (5b)

Variants on top of `all` (1024 seeds 0–49; min / median / max). `all` keeps the 0.16.0 rates; `spread` halves `plague_spread_permille` (200 → 100); `famine` halves `famine_ppm` (2500 → 1250); `both` is the shipped P3-08 data.

| | base (0.16.0) | all | all + spread | all + famine | all + both (shipped) |
| --- | --- | --- | --- | --- | --- |
| events | 439 / 805 / 1,422 | 451 / 777 / 1,440 | 410 / 746.5 / 1,361 | 353 / 720.5 / 1,458 | 363 / 669 / 1,357 |
| famines | 44 / 96 / 179 | 42 / 97.5 / 176 | 50 / 99.5 / 186 | 19 / 48 / 102 | 18 / 47.5 / 95 |
| plague spreads | 32 / 104.5 / 291 | 32 / 108 / 304 | 12 / 52 / 136 | 27 / 112.5 / 241 | 13 / 53.5 / 156 |
| quiet eras | 53 / 85.5 / 120 | 15 / 36.5 / 72 | 15 / 37.5 / 60 | 15 / 36.5 / 76 | 15 / 35.5 / 60 |
| mechanical share ‰ (famine, plague, spread, era) | 272 / 441 / 558 | 251 / 388.5 / 523 | 218 / 343 / 481 | 246 / 355.5 / 504 | 200 / 304.5 / 446 |
| wars; sacked (medians) | 23; 12 | 25; 13.5 | 23.5; 13 | 24.5; 14 | 22.5; 13 |
| standing elves / goblins / dwarves / faeries / humans / orcs | 12 / 31.5 / 6.5 / 4 / 14 / 5 | 9.5 / 32.5 / 5.5 / 4 / 16 / 5 | 10 / 36.5 / 5.5 / 4 / 14 / 5 | 12 / 33.5 / 5.5 / 5 / 13.5 / 5 | 11 / 32.5 / 4.5 / 5 / 14 / 5 |
| population elves / goblins / humans (medians) | 12,543 / 72,030 / 86,619 | 11,159 / 64,632 / 77,750 | 13,811 / 74,610 / 71,460 | 12,412 / 73,382 / 70,236 | 13,829 / 74,414 / 74,090 |
| events per age (medians, ages 1–6) | 33, 81.5, 96.5, 124.5, 147.5, 314.5 | 22, 71.5, 89, 112.5, 139, 336 | 22, 69.5, 79, 107.5, 128, 295 | 22, 68.5, 77.5, 103.5, 126, 294 | 22, 68.5, 72.5, 86, 123, 273.5 |

With everything shipped the mechanical lines fall from 44% to 30% of the log at the median (the critic measured about 60% at 0.15.0), and the orc age, the last 400 years, holds 273 instead of 315 events. The populations move within the spread between the variants (which reshuffle the keyed war draws: section 6's method); the dwarves' median standing of 4.5 against 6.5 is the largest change and is a point for the validation sweep (9 item 5). Famine repeats in one settlement are rare (median gap 588 years at 0.16.0, 4,651 famines of 1024 seeds 0–49), which is why a rest rule or a regional merge was not taken.

### 6.5 Names (items 1 and 3)

- **Region names** (P1-06 r4 14.8): on 135 sweep worlds (6,339 regions) `boner` and `dung` change one name; none of the 13 critic worlds or the seven geography and history cases.
- **History names** (P3-02 r6 11.6): of the 18,427 distinct names of the 375 sweep worlds, r5 rejects 5.4% (goblins 10.2%, humans 6.6%); every name either critic cites is rejected except the comic `Sneezskrub Hole`; the squeezed whole-name test would add only one false positive (`The Realm of Arthos`), so the rule is per word.
- **Saliences** (5c), on the stored logs of the 100 default worlds (they do not change the simulation): plagues 205‰ of the summary at the median before, 121‰ with the total-toll rule (43‰ with the outbreak's own toll only, the rejected variant), crises 10 per summary before, 1 after, the threshold 26 / 31 / 51 in both.

## 7. Tests to add

Each stage spec lists its own; in summary:

- P1-06 10.5 (r3) and 14.8 (r4): the r3 tests; `boner` and `dung` reject `Bonereach` and `Dungrove` (fixture prefix and suffix lists); the registry count 52.
- P3-02 11.7 (r5, r6): loader, rule, redraw (item 8), the critics' stems written as concatenations (item 10), the per-word guard `The Realm of Arthos` passes, `names_form_text` on fixed words.
- P3-04 16.1 items 7 and 10 (r10): eras not closed by charged events; the three-word form; names inside a sentence; the benchmark of 14.3.
- P3-05 16.1a (r6): items 17a–17i; invariants 3, 6, 7, 8 and 11–13 in the property tests of 16.2 item 17.
- P3-06 16.2 item 18a (r7): the peoples-only prefix; 16.1: a battle with F_W = 3 F_L gives dead_W = F_L × 80 div 1000; the capture sentence with `fled` 0, 1 and 12.
- P0-04 amendment 22: the help text.

## 8. Schema

**No schema change** (schema version 7; `docs/specs/schema.md` gains kind 31 in its catalogue section, as for every appended kind):

- **Kind 31 `settlement_abandoned_to`** is appended to the catalogue (P3-04 8.2: appending a kind is not a schema change; a shipped kind is never edited, so the destination needs a new kind rather than a third role on kind 16).
- **The three-word value form** (P3-04 r10 8.4) is a renderer format extension: no table, column, code list or kind structure changes, and every template that was valid before renders as before. A world of 0.17.0 whose templates use the form is refused by `HistoryView::load` of 0.16.0 or earlier; no shipped program of those versions renders sentences (P3-07 is not implemented, the viewer does not render events), so no proposals entry for a schema change is needed. Should the lead read a template-grammar extension as a contract change, the alternative is to drop "and {fled} fled" from the capture template altogether and to drop `{generations}` from the `quiet_era` template (no grammar change) and to replace kind 31's `{lost?…}` clause by a fixed `; {lost} were lost` (P3-08 question 5).
- Stored values change meaning nowhere: `quiet_era`'s `years` and `generations` keep their definitions (an era now spans the whole quiet row of its civilization); `settlement_abandoned`'s `survivors` is still the population at the abandonment.

**For P3-09** (written in parallel): kind id 31 is taken by this change; P3-09's kinds start at 32 if P3-09 ships after P3-08. P3-09 may use the three-word form. P3-08 changes P3-07 only in 6.1's "Sentences" (r7).

## 9. Statistics to re-measure (P3-08's validation)

The validation sweep of P3-06 15.6 and P3-05 15.6 at 0.17.0 (1024 seeds 0–99 at least, 512 seeds 0–29, 2048 seeds 0–4, the variant sets of 20 seeds), reporting against P3-06 15.3 and 15.5 (r7 ranges) and these, each against section 6:

1. Whole worlds: events, wars, battles, sacked, taken, civilizations ended, flare-ups, simulated years, summary threshold; the typical ranges of P3-06 15.5 (r7).
2. Quiet eras per world, eras of 50 years or less, eras with 0 new generations, median era length (6.1).
3. The mechanical share (famine, plague, plague spread, quiet era) of the log and per age (6.1, 6.4).
4. Plague share and crisis count of the summary (6.5).
5. Dwarves: median elevation rank, fortress share, rich-ore share, standing dwarven cities, standing settlements and population (6.2); dwarven homes upland or with reachable ore (P3-05 15.5, ≥ 90%).
6. Faeries: home forest share and dominant biome of every home, standing settlements and population, worlds with one settlement (6.3; P3-05 12.3).
7. Battles in which the victors lost more (must be 0 with the shipped tuning), captures and their sentences.
8. Abandonments by kind (16 and 31), with destination; chance abandonments of a last settlement (must be 0, P3-05 invariant 11); settlements above their kind cap at the present.
9. Names: the share of history names that differ from 0.16.0's per culture; no generated name filtered (P3-02 invariant 5); region names as P1-06 11 and 14.2 (unique, no numeral, keepers, attempts).
10. Timing: the history stage per world (P3-06 16.6's budget) and the engine microbenchmark.

## 10. Questions for Paul (each with the interim choice)

1. **Dwarven sites** (5a): dwarves take the high, ore-rich third of their regions (cell weights ore 400, height 600). Interim: as shipped. Alternatives: weaker weights (fewer crag holds, more valley towns), or a capacity change so that dwarves also count rough ground more than lowland (the critic's secondary point, not specified).
2. **Battle losses from the smaller force** (item 6): victors never lose more than the defeated; winners keep more of their army, so sieges fall more often (sacked median 12 → 15 with this rule alone at 1024 seeds 0–49; 12 → 13.5 with everything). Interim: adopt, as the critic proposed.
3. **One quiet era per quiet row** (5b): an era now spans the whole quiet stretch and holds the civilization's colonies, kind changes and quiet war endings of those years as separate events. Interim: yes. Alternative: keep eras closed by events and only merge eras shorter than 100 years into the next (not possible without a schema-visible change of event years; P3-04 6.6).
4. **Fewer famines and plague spreads** (5b): both rates halved, for the log's volume (6.4); famine and plague still shape the populations, but a reader meets about half as many of these lines. Interim: as shipped. Alternatives: keep the rates and leave the volume to P3-07's levels (famines and spreads are below the summary threshold), or halve only the spreads.
5. **The template grammar extension** (8): interim: the three-word form; the alternative without it is in section 8.
6. **P3-02 11.10**: the seven name questions stay open; interim choices stand (the full English-word list, no shared filter with region names, no syllable edits, god patterns, `The House of {root}`, orcish two-letter parts kept, plain lists).

## 11. Corrections after the implementation (r1, 2026-11-20)

The P3-08 engineer and test writer found these; the lead ruled; each is fixed in the stage spec named.

1. **The plague plan is a replay** (P3-05 r7 10.2): the outbreak's and its spreads' strikes are replayed in order on a copy of the populations, including an abandoned spread target's refugees joining its owner's nearest settlement, with the same keyed draws and no state change; the planned total equals the real toll (invariant 13).
2. **Example sentences** checked against 0.17.0's names: P3-04 r11 (8.5, 17: `The House of Finen`, P1-06 r3's region names, `Gnonk` for the filtered `Bonk-Gitch`), P1-06 r5 (14.7's table), P3-05 r7 (15.2's sample and kind 31's example illustrative), P3-06 r8 (11's samples, 15.2 illustrative); P3-07 r7 already marked its samples.
3. **P3-03 r4**: 7.1's gods at 0.17.0 (seed 42: Fielie, Krokzit, Bronhak, Syfiash, Wealstuld, Ogh'krak) and the collision example (god 2 `Zatchgnozz`); `names_form_text` fails in the chronology stage with P3-02 11.3's message, no `chronology:` prefix.
4. **P3-02 r7**: 11.7 item 6's lower-casing case (form `E{root}`, root `f`); where the `name_filter` kind checks report.
5. **P1-06 r5**: 4.1's object counts (78, 140); 14.3's space sizes recomputed for r4's lists; check 4a (iii)'s message (`'fen' form 2 slot '{feature}' has no words`, forms from 0, slot with braces); 14.8's `Dungrove` → `Bittertree Wood` confirmed with the engineer's binary; where `placename_one_common` and `placename_lands_distinct` report; and section 5 above names the three N ≤ 128 cases whose `region` hash moves.
6. **Kind 31 in the report**: `events_settlement_abandoned_to` follows `events_civ_conquered` (P3-07 9: one line per kind in id order; P3-05 r7, P3-06 r8).
7. **P3-05 16.0a layer message** (r7): `history: peoples: layer '<name>' is not <N> x <N> <type>`, `<type>` the layer's catalogue type (`u8`, `u16` or `i16`); **a string change for the engineer**.
8. **Smaller points**: P3-04 r11 14.3, the benchmark's simulated years are the years of opening, flare-up and final phase rows; P3-05 r7 8.4, in a synthetic world with `walk_area` 0 everywhere every dwarven founding is a fortress (the rule).
