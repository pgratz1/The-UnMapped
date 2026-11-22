# P3-11 Orc arrival, wording and name follow-ups

Status: **merged as ruled (r2, 2026-11-22)**: validated, audited and reviewed by the world critic on `p3/11-final` (95858f2); T9 (11 worlds) and T10 (3 worlds) missed by the targets as written and **accepted by the lead's ruling of 2026-11-22**, with both restated for later measurements (`docs/design/proposals.md`, "History: the P3-11 orc targets, the lead's ruling (awaiting Paul)"; P3-06 r21 23.1). r1 (2026-11-21): item 1 corrected after the implementation missed T11 (section 9 item 1). r2 (2026-11-22): text after validation (sections 4, 7, 9, 10, 12). Draft written by the spec writer on `p3/11-spec`, cut from `origin/main` at dfa4e66 (generator 0.19.0, P3-10 merged). Phase P3 History core, a change spec after P3-10 (generator 0.19.0) and before P3-07 (the history report, specified, not implemented). Generator **0.20.0**.

**No schema change**: schema stays 7. The change:
- appends two event kinds (39, 40);
- adds one race field (data);
- changes the faerie syllable lists, the name-filter lists and two report entries (data);
- adds one name-validity rule and one rendering rule (code).

There is no new stream and no new draw.

This spec is short on purpose, in the form of `docs/specs/p3-10-followups.md`. It lists every item and names the stage-spec section that holds its normative text. Each stage spec stays the single source for its stage, amended with a new revision, a bounded new section and a revision note; nothing is renumbered. It then gives the order of implementation, the effect on output and goldens, the tests, the statistics to re-measure, the decisions made and the questions for the owner. The spec writer ran nothing for this change. Every figure is quoted from the reviews named with it, and every figure not in them is marked *to be measured by validation*.

## 1. Purpose

The P3-10 second round passed with notes (`docs/reviews/p3-10-followups-round2-world-critic.md` N1 to N4; `docs/reviews/p3-10-followups-round2-validation.md` sections 4.1, 5.1, 10 and the addendum). Paul decided on the faerie names on 2026-11-21 (`docs/design/proposals.md`, "Paul's decision of 2026-11-21: faerie names keep their character, without the baby-talk sounds"). P3-11 collects them under one version bump:
- the orcs act within their first century (N1);
- claims and foothold cessions name the settlements at stake (N2);
- the faerie syllables and a rule against a syllable said twice in a row (Paul's decision);
- `tard` and the faerie `-ass` names (N3);
- the remaining wording point of round 1's F5 (small numbers as words);
- corrected figures in P3-06;
- a reading of wars whose goal differs from the claim they cite (N4).

Not here, each with its reason:
- **Repeated "besieged …; it held."** in long wars: P3-07's grouping.
- **Identical battle tolls**: P5, as the critic says.
- **Large tributes** (3 above 5,000 at defaults, the largest 9,492): a balance cap for a later pass, not wording.
- **Plague and famine filling calm worlds**: not new, and not wording.
- **Claims inside quiet spans**: accepted by the critic.
- **The orc age's length** and **orcs at peace among goblins**: Paul decided both ("leave it as is for now"; "71% is fine").

## 2. Items and where their normative text is

| # | Item (source) | Resolution, in short | Normative text | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Orcs act within their first century (N1) | New race field `awakening_population` (orc **1,200**, others 0 = `founding_population` 300). A race with the field above 0 arrives in numbers: its awakening settlement starts at max(300, min(1,200, C(H))), and (r1) its first colony is not held back by pressure. A member set `first_colony_pending` marks the civilization until its first colony, and while marked its capital counts as ready. So the second settlement, and with it 22.1's quarrel, comes after a median of about 11 years (the existing `hist.peoples.found` chance, 60‰ a year). New target **T11**: the median years from the orcs' awakening to their first colony, declaration, sack or capture is below 100 (0.19.0: 232). T9 ≤ 10, T10 = 0, T5, quick aim wins ≤ 3 and T1 ≤ 250‰ are kept | `docs/specs/p3-05-peoples.md` r16, r17 23; `docs/specs/p3-06-expansion-conflict.md` r19, r20 23.1; `docs/specs/p3-01-history-data-settings.md` r14 3.7 | yes |
| 2 | Claims and foothold cessions name the settlements (N2) | A claim on a region the holder role does not hold is kind 39 `claim_settlements`: "The Zatzeegg Mob laid claim to Greerk-Zark Pit of the Bizz-Skuz Horde in the Graven Thicket." The foothold cession of 22.5 is kind 40 `settlements_ceded`: "The Bizz-Skuz Horde gave up Greerk-Zark Pit in the Graven Thicket to the Zatzeegg Mob." `{settlement:all}` renders one, two and three or more names alike. Kinds 20 and 29 are unchanged and still written for every other claim and cession | P3-06 r19 23.2, 23.3; `docs/specs/p3-04-simulation-engine.md` r17 21.2; `docs/specs/p3-07-history-report.md` r14 23 | yes (event kinds and participants only) |
| 3 | Faerie names (Paul's decision of 2026-11-21) | Onsets `p` → `v`, `b` → `gl`; `ee` and `ou` at half their share (16 nuclei); the coda `p` removed; `ass` in the faerie `forbidden` list. No syllable repeated twice in a row, as a validity rule for **every culture**, with syllables defined from the pattern; a rejected attempt is the next keyed attempt t + 1 | `docs/specs/p3-02-names.md` r15 14.1, 14.2; P3-01 r14 3.7 | yes |
| 4 | `tard` (N3) | `tard` in `slurs.anywhere`; `retard`, `bastard` and `slurs.whole` `tard` removed as redundant; `via`, `van`, `von`, `ova` (reached by the new faerie onsets) in `words.whole` | P3-02 r15 14.3 | yes |
| 5 | Wording still open (round 1 F5, round 2 N4) | Values from one to nine render as words in sentences ("with one settlement", "spread to two other settlements", "four realms kept the peace"); a renderer rule beside the thousands separators; no template change | P3-04 r17 21.1 | no (sentence text only) |
| 6 | Figures in P3-06 (validation 4.1, 5.1, addendum) | 22.5: 151 in 64 worlds (not 184 in 69); the quick-aim baseline at 0.18.0 is 0 by 22.5's definition (2 by the looser count). 22.4: the 0.19.0 figures as merged. 15.5: the second-round typical ranges with the addendum's three bounds (truces 1–15, largest race share 303–802, faerie standing settlements 3–13) | P3-06 r19 23.5, 15.5, 22.4, 22.5 | no |
| 7 | A war's goal and the claim it cites (N4) | The rule (20.2 item 2) re-chooses the goal when the claim's region is weary, lost or the defender's capital. This is intended, not a defect. The declaration's note names the claim's region when it differs: "(The claim, since 5930, was to Woodward's Shaw.)" | P3-06 r19 23.4; P3-07 r14 23 item 3 | no (note text only) |

Item 5's wording points, one by one (the sentence now, the sentence wanted, the change):

| Point (source) | Now (0.19.0) | Wanted | Change |
| --- | --- | --- | --- |
| Small counts as digits (round 1 F5; round 2 N4 "with 1 settlement", 40 times) | "The Berd League broke away from the Kingdom of Bocksul with 1 settlement." · "… spread to 2 other settlements; 1,084 died in all." · "…, and 4 realms kept the peace." · "… after 3 years of war." | "… with one settlement." · "… spread to two other settlements; …" · "…, and four realms kept the peace." · "… after three years of war." | P3-04 21.1: 1 to 9 render as words in `{value}` and `#`; templates unchanged |
| Claim on a region the claimant holds (N2; 46 claims in 29 worlds) | "The Zatzeegg Mob laid claim to the Graven Thicket, held by the Bizz-Skuz Horde." | "The Zatzeegg Mob laid claim to Greerk-Zark Pit of the Bizz-Skuz Horde in the Graven Thicket." | kind 39 with template `{claimant} laid claim to {settlement:all} of {holder} in {@region}.` (item 2) |
| Foothold cession (N2; 2 at defaults) | "The Bizz-Skuz Horde ceded the Graven Thicket to the Zatzeegg Mob." | "The Bizz-Skuz Horde gave up Greerk-Zark Pit in the Graven Thicket to the Zatzeegg Mob." | kind 40 with template `{from} gave up {settlement:all} in {@region} to {to}.` (item 2) |
| War goal differs from the cited claim (N4) | "Clan A'thuukh declared war on the Kingdom of Sunt over East Wood." (causes e510, e540; e540 a claim on Woodward's Shaw) | the same sentence, with the report's note "(The claim, since 5930, was to Woodward's Shaw.)" | P3-07 23 item 3, `history_note` (item 7) |
| Repeated sieges (round 1 F5) | "… besieged Rearstead …; it held." up to four times in a row | grouped in the report | **left for P3-07** (grouping is a report rule) |
| Identical battle tolls (N4) | "642 of the defeated and 205 of the victors fell" twice, 73 years apart | — | **left for P5** (the critic) |
| Large tributes (round 1 F5, N4) | "… paid tribute to …; 9,492 were carried off." | — | **left for a balance pass** (not wording) |

## 3. Order of implementation

One branch, one engineer and one test writer, working from the stage specs separately. `GENERATOR_VERSION` becomes **0.20.0** in step 1, the first step that changes a world byte. Every golden is re-recorded once, in step 7. Intermediate commits may fail the world-golden tests; the merged branch must not.

| Step | What | Spec | Output |
| --- | --- | --- | --- |
| 1 | Data: race field `awakening_population` (loader and every race file); the faerie lists; the filter edits; the two `history.report` entries. In `game/data` and both fixture roots | P3-01 r14 3.7; P3-02 14.1, 14.3; P3-06 23.6 | `content_hash`, names |
| 2 | Names: the repeated-syllable rule in the attempt's validity | P3-02 14.2 | names |
| 3 | Renderer: one to nine as words | P3-04 21.1 | sentences only |
| 4 | Catalogue: kinds 39 and 40 | P3-04 21.2 | catalogue tables |
| 5 | Conflict: kind 39 at the claim and kind 40 at the foothold cession; `history_note`'s two changes (`digest.cpp`, implemented since P3-09) | P3-06 23.2, 23.3; P3-07 23 items 3 and 4 | event kinds and participants; notes |
| 6 | Peoples: the awakening population and the pending first colony | P3-05 23.2 items 1 and 2 | history tables |
| 7 | Goldens and figures: every golden of section 4; the README hash; `docs/specs/schema.md` 6 gains kinds 39 and 40 | 4 | — |

### 3.1 Per role

- **Engineer.**
  - `names.cpp`: the syllable split and the repeated test of P3-02 14.2, in the attempt's validity beside the triple-letter test.
  - The renderer (`render_event`): P3-04 21.1, beside 20.3's separators.
  - `event_catalogue.cpp`: kinds 39 and 40.
  - `conflict.cpp`: 23.2's choice of kind at the claim (beside `point_event(ctx, "claim", …)`).
  - `conflict_war.cpp`: 23.3's kind at the foothold cession, with the participants listed before the transfer.
  - `digest.cpp`: P3-07 23 items 3 and 4; the claim lookup also finds `claim_settlements`.
  - `peoples.cpp`: P3-05 23.2 item 1 in `awaken`; item 2 (r1): `first_colony_pending` set in `awaken`, read in 9.3 item 1's readiness, cleared at the civilization's first `colony_founded` and at its end.
  - `kinds_history.cpp`: the race field.
  - Data: `game/data/races/*.toml`, `game/data/cultures/faerie.toml`, `game/data/name_filters/{slurs,crude,words}.toml`, `game/data/history/report.toml`, and the copies under `worldgen/tests/data/geo/` and `geo-small/`.
  - Default of hand-made tuning built in code: `awakening_population` 0 (the rule off).
- **Test writer** (from the specs, not the C++).
  - `p3_names_reference.py`: P3-02 14.1–14.3. The 59,400-name scan of P3-02 13.1, per culture, gives three rates: names rejected by 14.2, by `forbidden` `ass`, and by `tard` and the four words. It also adds a faerie sample at 0.20.0.
  - `p3_peoples_reference.py`: P3-05 23.
  - `p3_conflict_reference.py`: P3-06 23.2, 23.3 and the self-check of 23.7 item 5; T11's per-world value and the reported orc figures of 23.1 in its war or orc lines.
  - `p3_engine_reference.py`: P3-04 21.1 in its sentence lines.
  - `p3_data_reference.py`: the field and lists.
  - Tests: P3-02 14.4, P3-04 21.3, P3-05 23.4, P3-06 23.7, P3-07 23 item 5, P3-01 3.7.
- **Validation.** The sweep of P3-06 15.6 at 0.20.0 (the 375 jobs of the P3-10 rounds, so every figure compares pair for pair) and section 8 below. A missed target goes to the lead before the merge (P3-06 22.4).
- **World critic.** The orc age at defaults, `calm` and `turbulent`; the new claim and cession sentences; the faerie names (Paul's taste); the small-number words.

## 4. Effect on output: golden values

**Every world file changes.** The race field, the culture and filter lists and the report entries change every `content_hash`, the P0 empty worlds included, and `GENERATOR_VERSION` changes in `meta`. The empty world's size is unchanged (no table).

| Golden file | Re-recorded | What moves | What must not move (a difference is a defect) |
| --- | --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt` | yes, by the validation specialist after the cross-preset comparison | every case | — |
| `GOLDEN_CASES` (`db_determinism_test.cpp`), `stages_pipeline_test.cpp`'s hashes | yes (r2: they hash worlds made with no data, but those worlds carry `meta.generator_version`, so they move at every version change, as at P3-10; r0 said they would not) | `meta.generator_version` | every other byte of those worlds |
| README's `--seed-phrase "The Unmapped"` hash | yes | — | — |
| `core_vectors.txt`, `p1-01-elevation.txt` … `p1-06-regions.txt` | no | — | every line (region names are not drawn by `make_name`) |
| `p3-01-data.txt` | yes | the data roots' content hashes | — |
| `p3-02-names.txt` | yes | faerie names (14.1); names with a repeated syllable in every culture (14.2); human and dwarven names holding `tard` (14.3) | names none of these reach |
| `p3-03-chronology.txt` | yes | god names those rules reach (the faerie god at least) | `race`, `race_biome`, `culture`, `age`, the awakening years |
| `p3-04-engine.txt` | yes | stub names those rules reach; sentence lines with a value from one to nine | the schedule cases; `phase` rows but for `trigger_event` where event ids renumber; years and kinds of the stub events |
| `p3-05-peoples.txt` | yes | every history line with orcs; names | σ and C lines of every race |
| `p3-06-conflict.txt` | yes | every history line; war lines gain `-foothold` | — |
| `hist-conflict-73-1024` (P3-10 addendum's slow case) | yes, from the reference | every line | — |
| P3-07 report goldens | none yet | — | — |

`world-hashes.sh` keeps its fifteen cases. The history cases' `.report.txt` gain `events_claim_settlements` and `events_settlements_ceded`.

## 5. Measurements

The spec writer did not run the generator or a harness for this change (the lead's instruction). The figures quoted come from:
- the world critic's second-round review (N1's table: orc first acts at 0.18.0 and 0.19.0; N2's counts; N3's names);
- the second-round validation (sections 4, 4.1, 5, 5.1, the addendum, `sweep/orcs.txt`);
- the shipped data, read through P3-05 9.1 and 9.3 (P3-05 23.2's growth and pressure arithmetic, at an assumed share of 4,700, the median orc region of P3-05 5.1).

*To be measured by validation*:
- T11 and everything P3-06 23.1 reports;
- the share of orc capitals that are fortresses, and of orc homes held by another civilization;
- the claims on a region held by a third party or by no one;
- the war declarations whose goal differs from their claim, by reason;
- the rejection rates of P3-02 14.2 and 14.3 per culture;
- every typical range of P3-06 15.5 at 0.20.0.

## 6. Tests to add

Each stage spec lists its own. In summary:
- **P3-05 23.4** (r1, replaced): a pending capital is ready at pressure 255‰ and 152‰ (seed 1's case); the mark clears with the first colony; fortress capitals; no target keeps the mark; lying low first; a race at 0 and secessions unchanged; the population clamps; the loader; invariant 3 and the replayed-pressure property.
- **P3-06 23.7**:
  - the claim kind by the holder of R (held by b, by a, by a third civilization, by no one), with visible settlements only;
  - the foothold cession's kind and participants;
  - kinds 39 and 40 in invariants (13), (15), (16), (17) and T10;
  - the reference's self-check that item 2 changes no draw.
- **P3-04 21.3**: the number words (0, 1, 2, 9, 10, 999, 1,000, −3; years; two- and three-word forms); no shipped template starts a sentence with a value; kinds 39 and 40 in the catalogue, `template_problem` and `{settlement:all}`.
- **P3-02 14.4**:
  - the nine rows of 14.2's table, `OONC` with an empty first syllable, the redraw and the fallback;
  - the faerie lists and `ass`;
  - `tard` (`Tardham`, `Tardwuldwick`, `The Horktard Throne`, `Stard` filtered; `Standard` not);
  - `name_filter_not_redundant` on the edited lists;
  - invariant 8 by replay.
- **P3-07 23 item 5**: the `[stats]` lines; units and hollow wars with kind 40; the declaration's note with a different claim region; the flare-up note for a kind 39 claim.
- **P3-01 3.7**: the field's range and absence.

## 7. Schema

**No schema change** (schema version 7; `docs/specs/schema.md` 6 gains kinds 39 and 40 in its catalogue section, as for every appended kind).
- **Kinds 39 `claim_settlements` and 40 `settlements_ceded`** are appended (P3-04 8.2: appending a kind is not a schema change). They touch the contents of tables `event_kind` and `event_kind_role` only (two rows in `event_kind`, three in `event_kind_role` for each; no `event_kind_value` row). No column changes.
- **Why not a role on kinds 20 and 29.** Item 2 asked to prefer existing roles and values. Kinds 20 and 29 have no settlement role and no value that could carry the settlements. Adding a role or a value key to an existing kind *is* a schema change by P3-04 8.2 ("adding, removing, renaming or reordering a role or value key of an existing kind"), and a shipped kind is never edited. Hence the new kinds, as P3-10 did with kind 37. Kinds 20 and 29 keep their structure and meaning and are still written for every claim on a region its holder holds and every cession that passes a region.
- **The number words** (P3-04 21.1) are a rendering rule like 20.3's separators: no stored value changes, and every template renders as before but for the words.
- **The repeated-syllable rule** (P3-02 14.2) is a validity rule of name attempts: names change, and no table or column changes.
- **New data fields**: `awakening_population` (race). With the list edits of P3-02 14 and the two `history.report` entries, they change `content_hash` only.
- **No proposals entry for a schema change** is needed.
- **Participant bound (r2).** An event has at most `MAX_PARTICIPANTS` (64) participants, so kinds 39 and 40 list at most **62** settlements after their two civilizations (P3-04 r18 21.2, P3-06 r21 23.2, 23.3). The implementation lists 62. **Known difference:** the test writer's reference lists up to 64. No world reaches either bound (a region holds a few settlements), so no output differs. A later change should align the reference.

## 8. Statistics to re-measure (P3-11's validation)

On the sweeps of P3-06 15.6 at 0.20.0, against the 0.19.0 figures as merged (P3-06 23.1, 23.5, 15.5).

**Targets**, at 1024 defaults, 100 worlds:

| Target | 0.19.0 as merged | Bound at 0.20.0 |
| --- | --- | --- |
| **T11**: median years from the orcs' awakening to the first orc colony, declaration, sack or capture (P3-06 23.1) | 232 | **below 100** |
| **T9**: worlds with exactly one standing orc settlement at E | 7 | at most 10 |
| **T10**: worlds in which the orcs do not act after E − 250 (5750) | 0 | **0** |
| **T5**: orc wars won / lost | 98 / 50 | won ≥ lost |
| quick aim wins (P3-06 22.5) | 0 | at most 3 |
| **T1**: hollow wars ‰ over the sweep | 227 | at most 250 |
| T2, T3, T4, T6, T7 (P3-09, P3-10) | met | as before |
| names: three equal letters, the crude fragments of P3-02 13.1, `tard`; fallbacks | 0 | 0 |
| faerie names with the letter `p` or `b` | — | 0 |

**Reported**, with no target:
1. **Orcs** (P3-06 23.1's list):
   - T11's 10th and 90th percentiles;
   - worlds with 200 or more silent years (0.19.0: 88);
   - the median first orc declaration (0.19.0: 244 years);
   - the first wars declared with at most one standing settlement, and their outcomes;
   - the residual cases of P3-05 23.2 (held homes, homes with no free site), with their T11; the years to the first colony and from it to the first declaration;
   - T8 (0.19.0: 71 of 100);
   - T5, T9, T10 and T11 in `calm` and `turbulent` (0.19.0: T5 28 / 6 and 23 / 22; T9 2 and 2; T10 0 and 0);
   - orc people and standing settlements, and the race shares (0.19.0: orcs 69‰ of people, 79‰ of standing settlements).
2. **Claims and cessions**:
   - claims of kind 39 per world, by the region's holder (the claimant, a third civilization, no one);
   - foothold cessions (kind 40) per set (0.19.0: 2 at defaults, 5 `civs4`, 4 at 512, 1 at 2048, 1 `dense`);
   - in every world, no kind 20 claim whose holder does not hold its region at the claim, and no kind 29 cession whose region's owner does not change.
3. **War goals** (P3-06 23.4): declarations whose region differs from their claim's, per world, by reason.
4. **Names**:
   - per culture, names rejected by P3-02 14.2, by `forbidden` `ass`, by `tard` and by the four new words;
   - faerie names with `ee` or `ou` against 0.19.0;
   - a list of new faerie names for the critic;
   - region names untouched (not covered).
5. **Typical ranges**: every row of P3-06 15.5 at 0.20.0, claims and cessions counted with kinds 39 and 40.
6. **Sentences**: no value from 1 to 9 rendered as a digit in any sentence (years aside); no "0 of the victors", no "its seat"; separators as before.

## 9. Decisions made (merged as ruled, r2)

Every decision below shipped as written in 0.20.0 (r1 for item 1). Measured at 0.20.0 (P3-06 r21 23.1): T11 median 9, T9 11, T10 3, T5 168 / 95, T8 83, T1 224‰, quick aim wins 0. The lead's ruling of 2026-11-22 accepts the misses of T9 and T10 and restates them for later measurements.


1. **Item 1: the horde splits at once: the first colony is freed from pressure** (P3-05 r17 23.2; r1 of this spec, correcting r0).
   - **r0's mistake.** r0 relied on the awakening population alone (1,200), believing an orc village's kind cap was 1,499, so that the capital would be ready at once. P3-05 5.2 gives every orc settlement that is not a fortress the race's highest cap, 7,999. A capital is therefore ready only at about 750‰ × min(share, 7,999): about 3,525 in a median home of share 4,700, which takes some 121 years from 1,200 and 210 from 300. The implementation of r0 (`origin/p3/11-impl` e7b9988) measured a T11 median of 149.5 over seeds 0 to 19, the first act a colony in 18 of 20. r0's data lever, 1,125, could not help. P3-05 23.1 has the line-by-line reading and the arithmetic.
   - **The rule.** A civilization placed at an awakening of a race with `awakening_population` > 0 is marked pending, and while pending its capital is ready whatever its pressure. The mark clears with its first colony or its end. The first colony then waits only for 9.3's chance, 60‰ a year in ticks, a median of about 11 years, wherever a target exists, fortress capitals included. Every later colony waits for pressure as before.
   - **Draws.** No new draw and no new stream. The existing `hist.peoples.found` draw (key Y, c, 0, 0), and `hist.peoples.outward` (Y, c, 0, 0) where both targets exist, are reached from the first visible T2 instead of after about a century.
   - **The field stays**, with its name, range, value (orc 1,200) and its first effect. Its second effect is the pending mark. No field is added, renamed or removed. The population still matters: the first war (22.1) is fought by a realm of about 1,200, not 300, and the first colonists are 144.
   - **Rejected (b): an awakening population of about 6,000**, the least that passes the real threshold in every home (seed 1: 750‰ × 7,999). Every orc realm would start as a town of 4,700 to 6,000, at or near its region's capacity and a third to two fifths of an average orc realm at the present. That is a strength other peoples reach in centuries, with colonists of 720. It would inflate T5, threaten the orcs' neighbours and the race shares, and tie the rule to region capacities.
   - **Rejected (c): the two-settlement horde.** It undoes 22.1 at once, at 600 people. With the real cap its capital (3/4 share) and its second settlement (1/4 share) are ready only after about 193 and 121 years, so the first act would mostly be a declaration at small odds. It also needs a second site, name and event per awakening.
   - **Hold-back and targets.** P3-06 22.1's hold-back is kept unchanged, and so are the round-2 orc targets.
2. **T11 leaves claims out.** The awakening-quarrel claim falls in the orcs' first steps in nearly every world, so counting it would meet the target at 0.19.0 already. T11 is the critic's measure: colony, declaration, sack or capture.
3. **T10's bound becomes 0**, as the lead's constraint for this change states (P3-06 22.4 allowed 2).
4. **Item 2: two new kinds, not a role on kinds 20 and 29**, since P3-04 8.2 makes a new role on a shipped kind a schema change (section 7).
   - The claim variant applies whenever the holder role does not hold the region, which covers both of the task's cases: the claimant holds it, or a third party or no one does.
   - The cession variant applies to 22.5's foothold cession only; an ordinary cession passes the region and its sentence is true.
   - One template per kind serves one, two and three or more settlements through `{settlement:all}`.
   - "Of {holder}" follows the siege and sack sentences.
   - The critic's "its 2 towns" is not used: "its" does not agree with plural realm names, and P3-10 removed it elsewhere.
   - The claim lists only **visible** settlements: those that put the region in the holder's presence (P3-06 5.1). The cession lists every settlement that passes, a hidden one included, as 20.27 passes it.
5. **Item 3: the faerie lists** (P3-02 14.1).
   - The onsets `v` and `gl` replace `p` and `b`. They were chosen for a soft sound and few English words, over `n`, `r`, `th`, `sw` and `z`. Both are also elven onsets; the faerie character stays in the nuclei, codas and forms the elves lack (P3-02 14.1), so the names do not drift toward the elven ones.
   - `ee` and `ou` are halved by weighting the other nuclei, not removed: Paul said "fewer".
   - The coda `p` is removed too. A coda before an empty onset is an onset in all but spelling, so keeping it would keep the `p` sound the decision removes. This reads Paul's "no `p` onsets" by its intent, and the lead may restore the coda if Paul meant the letter only at a syllable's start (question 3).
   - `ass` goes in the faerie `forbidden` list: the culture data is enough, so no filter rule is needed.
6. **Item 3: the repeated-syllable rule is general, not a per-culture switch.**
   - No culture wants a syllable said twice in a row, a switch would add a required field to every culture file, and the triple-letter rule is general too.
   - "Syllable" is defined from the pattern: a boundary at the start, before every `O`, before every `N` not preceded by `O`, and at separators.
   - "Repeated" means that two consecutive syllables are equal, or that the second is the first plus a coda. This catches `Lilin` and `Wifleeflee` and lets compounds such as `Grangrak` stand.
   - Across a separator counts (`Krak-Krak`).
   - A rejected attempt is an invalid attempt: the next attempt t + 1 is drawn with its own keys (stream `hist.names.<kind>`, key (t + 1, culture, entity, j)).
7. **Item 4: `tard` in `slurs.anywhere`.**
   - `at_start` would miss `The Horktard Throne`.
   - Only human and dwarven roots reach the fragment, about 0.3% of human names by the lists' shares, and the harmless losses (`Stard`, `Stardton`) are the critic's accepted ones.
   - The four English words the new faerie onsets reach (`via`, `van`, `von`, `ova`) go in `words.whole`.
8. **Item 5: words for one to nine as a renderer rule**, not template edits. Every template (secession, outbreak, quiet stretch, war endings, battles, tolls) reads better the same way, and P3-10's separators set the precedent.
9. **Item 7: the goal rule is intended.** The report's note names the claim's region. No generator change (question 1).

## 10. Open questions (r2: merged as ruled)

Question 1 shipped with its recommendation (a), and question 3 with the coda `p` removed; both stay open for Paul's confirmation. Question 2 is closed: T11 was met (median 9), and its one residual world (seed 88) has no free target at all, so the outward rule suggested below would not have helped it. The critic's three questions for Paul are in the lead's ruling in `docs/design/proposals.md`.


1. **A war's goal and the claim it cites** (non-blocking; P3-06 23.4). A war is declared on a claim but fought over another region when, by the declaration, the claimed region is weary, lost by the defender or the defender's capital region.
   - (a) **Recommended**: keep the rule and have the report say it: "(The claim, since 5930, was to Woodward's Shaw.)" This needs no generator change, and only the note text changes.
   - (b) Write a fresh claim on the new goal at the declaration, so that the war cites a claim on its own region. That adds one event per such war, with no new fact.
   - (c) Drop the claim and declare nothing when its region is no longer a goal. That is a behaviour change, with fewer wars and every war target to re-measure.

   The interim reading is (a), as specified.
2. **If T11 is missed** (for the lead after validation; r1). No data lever is expected to help. With the first colony freed the chance is not the limit, so a miss means that P3-05 23.2's residual cases (a home held by another civilization, a home with no free site: no target) are common. Validation reports them separately, and the fix is a rule change for the spec writer, for instance outward targets from the home's neighbours for a pending civilization that holds no region. Orc `expansion` 800 (data) raises the chance to 80‰ but moves every orc colony. For T5 the levers that work are `quarrel_min_settlements` 3, orc `awakening_population` 2,000 and orc `odds_min_permille` 600, in that order (P3-06 23.1).
3. **The faerie coda `p`** (for Paul, through the lead). It is removed with the onsets, since a coda before an empty onset sounds as an onset. If Paul meant only the onset lists, the coda `p` can return as a ninth coda (a data change). It would then bring back names such as `Lipa` and `Mapou`.

## 11. Proposals

One entry in `docs/design/proposals.md`: "History and names: P3-11 orc arrival, wording and names (scope)", recording this change's scope and question 1. No item changes the design document:
- the orcs' coming "opens the most recent age of conflict" (design 5) is served, not changed;
- the faerie names follow Paul's decision of 2026-11-21;
- the new kinds are appended within schema 7.

## 12. Left for the next change that alters world bytes (r2)

Listed here, not specified. Sources: the P3-11 world critic's review and the lead's ruling of 2026-11-22.
- **The quarrel war gated on strength** (critic F1). 63 of the 83 first orc wars are declared with exactly two settlements, and 19 of them are won and 29 lost.
- **`shiva`** for the famous-name list.
- **`fag` inside a word**: today it is an `at_start` entry only.
- **The dwarven codas `din` and `rin`**, which hide stutters the repeated-syllable rule cannot see (Clan Didin, Bririn).
- **Mixed numerals and words in battle tolls**: "24 of the defeated and seven of the victors fell".
- **The reference's participant bound for kinds 39 and 40**: 64 against the implementation's 62 (section 7).
