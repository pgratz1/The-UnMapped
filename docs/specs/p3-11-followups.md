# P3-11 Orc arrival, wording and name follow-ups

Status: **draft for the lead's approval (2026-11-21)**, written by the spec writer on `p3/11-spec`, cut from `origin/main` at dfa4e66 (generator 0.19.0, P3-10 merged). Phase P3 History core, a change spec after P3-10 (generator 0.19.0) and before P3-07 (the history report, specified, not implemented). Generator **0.20.0**.

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
| 1 | Orcs act within their first century (N1) | New race field `awakening_population` (orc **1,200**, others 0 = `founding_population` 300). The awakening settlement starts at max(300, min(1,200, C(H))) and is ready to send settlers from its first tick (village cap 1,499, pressure 800‰ ≥ 750‰), so the second settlement, and with it 22.1's quarrel, comes within decades. New target **T11**: the median years from the orcs' awakening to their first colony, declaration, sack or capture is below 100 (0.19.0: 232). T9 ≤ 10, T10 = 0, T5, quick aim wins ≤ 3 and T1 ≤ 250‰ are kept | `docs/specs/p3-05-peoples.md` r16 23; `docs/specs/p3-06-expansion-conflict.md` r19 23.1; `docs/specs/p3-01-history-data-settings.md` r14 3.7 | yes |
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
| 6 | Peoples: the awakening population | P3-05 23.2 | history tables |
| 7 | Goldens and figures: every golden of section 4; the README hash; `docs/specs/schema.md` 6 gains kinds 39 and 40 | 4 | — |

### 3.1 Per role

- **Engineer.**
  - `names.cpp`: the syllable split and the repeated test of P3-02 14.2, in the attempt's validity beside the triple-letter test.
  - The renderer (`render_event`): P3-04 21.1, beside 20.3's separators.
  - `event_catalogue.cpp`: kinds 39 and 40.
  - `conflict.cpp`: 23.2's choice of kind at the claim (beside `point_event(ctx, "claim", …)`).
  - `conflict_war.cpp`: 23.3's kind at the foothold cession, with the participants listed before the transfer.
  - `digest.cpp`: P3-07 23 items 3 and 4; the claim lookup also finds `claim_settlements`.
  - `peoples.cpp`: P3-05 23.2 in `awaken`.
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
| `GOLDEN_CASES` (`db_determinism_test.cpp`), `stages_pipeline_test.cpp`'s hashes | as at P3-10: they hash worlds made with no data, so they are expected not to move; re-checked | — | — |
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
- **P3-05 23.4**: the awakening population (free home, low-capacity home, upland fortress, a race at 0, secessions and colonies unchanged, the loader, invariant 3).
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
   - the residual cases of P3-05 23.2 (fortress capitals, held homes, capitals that became towns first), with their T11;
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

## 9. Decisions made (for the lead)

1. **Item 1: a faster first colony, not a horde with two settlements.** The awakening settlement of an orc civilization starts at 1,200 (P3-05 23.2).
   - *Reasons*:
     - it is the smallest change: one race field and one line of the awakening, no new event, site or name;
     - it keeps the point of P3-06 22.1: the orcs still go to war only once a second settlement stands, but that settlement now comes within about a decade, wherever 9.3 finds a target;
     - the realm that declares the first orc war is about four times the size of a 300-person realm, which is the round-1 failure the hold-back was written for;
     - the first act it brings forward is a colony, an act in T11's sense, so T11 does not depend on war odds.
   - *Rejected*: two settlements at the awakening (P3-05 23.3). It would undo 22.1 at once, at 600 people. The first colony would still wait 75 to 120 years for growth. It needs a second site, name and event per awakening. It stays the next lever if T11 is missed through fortress capitals or held homes.
   - *Value*: 1,200 sits in the range that is ready at once (1,125 to 1,499). It keeps most of the window before the village becomes a town, and 1,125, the longest window, is the first data lever.
   - Keyed draws only; **no new draw and no new stream**: the colony's chance is the existing `hist.peoples.found` draw (key Y, c), taken sooner.
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

## 10. Open questions

1. **A war's goal and the claim it cites** (non-blocking; P3-06 23.4). A war is declared on a claim but fought over another region when, by the declaration, the claimed region is weary, lost by the defender or the defender's capital region.
   - (a) **Recommended**: keep the rule and have the report say it: "(The claim, since 5930, was to Woodward's Shaw.)" This needs no generator change, and only the note text changes.
   - (b) Write a fresh claim on the new goal at the declaration, so that the war cites a claim on its own region. That adds one event per such war, with no new fact.
   - (c) Drop the claim and declare nothing when its region is no longer a goal. That is a behaviour change, with fewer wars and every war target to re-measure.

   The interim reading is (a), as specified.
2. **If T11 is missed** (for the lead after validation). The data lever is orc `awakening_population` 1,125. The rule lever is the two-settlement horde (P3-05 23.3), which comes back to the spec writer. The residual cases are fortress capitals and homes held by another civilization (P3-05 23.2), and validation reports them separately.
3. **The faerie coda `p`** (for Paul, through the lead). It is removed with the onsets, since a coda before an empty onset sounds as an onset. If Paul meant only the onset lists, the coda `p` can return as a ninth coda (a data change). It would then bring back names such as `Lipa` and `Mapou`.

## 11. Proposals

One entry in `docs/design/proposals.md`: "History and names: P3-11 orc arrival, wording and names (scope)", recording this change's scope and question 1. No item changes the design document:
- the orcs' coming "opens the most recent age of conflict" (design 5) is served, not changed;
- the faerie names follow Paul's decision of 2026-11-21;
- the new kinds are appended within schema 7.
