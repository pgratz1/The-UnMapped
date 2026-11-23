# P5-01 Carried-over follow-ups

Status: **draft r0, 2026-11-22, for the lead's approval; r1 (2026-11-22): text after the integration, the measured results, T18 restated and the quarrel threshold kept by Paul's decisions of 2026-11-22 (see the revision note at the end).** Written by the spec writer on `p5/01-spec`, cut from `origin/main` at f29deec (generator 0.20.0, P3 approved, P4-00 and P5-00 merged). Phase P5 History depth, stage 1 (`docs/specs/p5-00-history-depth-overview.md` 3.1, "P5-01 Carried-over follow-ups"): the changes Paul's approval of P3 carried into P5, as one change. Generator **0.21.0**.

**No schema change**: schema stays 7. The change:
- appends two event kinds, 41 `claim_dropped` and 42 `refuge_hidden` (P5-00 4.4: P5-01's kinds take 41 onward);
- adds a fresh claim at a re-aimed declaration and a strength test to the orcs' awakening quarrel (rules, no new kind);
- adds one stream (`hist.conflict.toll`), two `history.conflict` fields and one `history.engine` field (data);
- edits the region-name lists, the name filter, the dwarven culture and three report entries (data);
- refines one rendering rule (number words) and rewrites the history report as format v2 (report only).

This spec is short on purpose, in the form of `docs/specs/p3-11-followups.md`. It lists every item and names the stage-spec section that holds its normative text. Each stage spec stays the single source for its stage, amended with a new revision, a bounded new section and a revision note; nothing is renumbered. It then gives the order of implementation, the effect on output and goldens, the tests, the targets, the decisions and the questions. Unlike P3-11, the spec writer measured several items with prototypes on the stored 0.20.0 worlds of the P3-11 validation sweep (section 5); every other figure is quoted from the review named with it, or marked *to be measured by validation*.

## 1. Purpose

Paul approved P3 on 2026-11-22 and carried into P5's first stage the items of `docs/specs/p3-11-followups.md` section 12 and the world critic's notes on the history report (`docs/design/proposals.md`, "Paul's approval of phase P3 History core, 2026-11-22"; P5-00 3.1). P5-01 collects them under one version bump, before any P5 subsystem, because each changes world bytes and every later stage re-records on top of it:
- every war cites a claim on its own region (Paul's decision 6 of 2026-11-22);
- the orcs' first war waits until they are strong enough to win it (the P3-11 critic's F1);
- fewer grim region names (Paul's decision of 2026-11-22, item 3);
- a city's founding without its census figure (the P3-07 critic's A10);
- the name filter's three carried entries and two the critic listed with them;
- one number form in a battle toll; the reference's participant bound;
- a record of a refuge's hidden years and of a dropped claim, so the report's lists are exact;
- from the "also consider" list, the two that a small rule fixes: identical battle tolls in a row, and a flare-up of one year cut short by the final phase;
- the history report's format v2: the world critic's B1 to B6 and the audit's A3.

Not here, each with its reason (section 12 lists them):
- **Repeated succession crises at one town** (nine at Natch Pit, turbulent seed 14): succession becomes a named ruler's line in P5-06; a rule here would be replaced there.
- **Five held sieges of one town; four seat moves in seven years**: true to the rows; a report grouping, which is a v2 amendment of its own (P3-07 25.1's growth rule does not cover a regrouping of chronicle lines, so it needs a reading of its own).
- **Large tributes**, **repeated searches for the same hidden realm**: balance passes with their own targets.

## 2. Items and where their normative text is

| # | Item (source) | Resolution, in short | Normative text | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | A fresh claim at a declaration (Paul's decision 6 of 2026-11-22; P3-11 10 item 1 (b)) | When "Declaring" re-chooses the goal G ≠ the claim's region R_c, a fresh claim on G (kind 20, or 39 when b does not hold G) is appended right before the `war_declared`, caused by the old claim and by why G is b's; the war cites the fresh claim. The report's note "(The claim, since …, was to …)" no longer occurs; the note dates the grievance from the old claim | `docs/specs/p3-06-expansion-conflict.md` r22 24.1; `docs/specs/p3-07-history-report.md` r19 25.8 | yes |
| 2 | The awakening quarrel gated on strength (P3-11 critic F1; the lead's ruling on the P3-11 orc targets) | The quarrel claim (22.1) is also held back while a's **strength** against b is below `quarrel_strength_permille` (shipped 1000): the odds of 20.6 item 3 without the aggression discount, force(a) × 1000 div (force(b) × def(G) div 1000 × `declare_ratio` div 1000), capped at 1000. With the shipped data: the orcs' whole force at least the defender's force weighted by the goal's defence. Held back in 22.1's three places; the data lever is the field | P3-06 r22 24.2; `docs/specs/p3-01-history-data-settings.md` r15 3.8 | yes |
| 3 | Fewer grim region names (Paul's decision of 2026-11-22, item 3) | Of the 40 grim words r6 kept in `common`, the six of spirits and curses stay (`Ghost`, `Wraith`, `Accursed`, `Cursed`, `Ghosts`, `Shades`); 34 move to `trait_barren`. Grim drawn names at 1024: **90‰ → 43‰** (measured) | `docs/specs/p1-06-regions.md` r10 16 | yes (region names) |
| 4 | `became_city` without the figure (P3-07 critic A10) | Template `{settlement} in {@region} grew into a city.` (data) | `docs/specs/p3-05-peoples.md` r18 24 | yes (template row and `content_hash`) |
| 5 | Names (P3-11 section 12; critic F3, F4) | `fag` from `slurs.at_start` to `slurs.anywhere` (`fagg` removed); `shiva`, `hindi` in `famous.whole`; `fannie` in `crude.whole`; the dwarven `forbidden` list gains `didin` and `ririn` | `docs/specs/p3-02-names.md` r17 15 | yes |
| 6 | One number form per toll (critic F6); the reference's participant bound (P3-11 7) | The numbers of one clause of a template (cut at the literal `; ` and `, `) are all words when all are 1 to 9, else all digits: "24 of the defeated and 7 of the victors fell". The test writer's reference lists at most 62 settlements in kinds 39 and 40 | `docs/specs/p3-04-simulation-engine.md` r19 22.1, 22.2 | no (sentences and a test only) |
| 7 | Exact refuges and standing claims (P3-07 24.7's two approximations) | Kind 41 `claim_dropped` (cause the claim) where 20.2 item 2 drops a claim at a declaration with no allowed goal; kind 42 `refuge_hidden` at every flight, with `end_year` the refuge's last hidden year within the history. The report marks refuges `; hidden` or `; found <year>` and closes claims by kind 41 | P3-04 r19 22.2; P3-06 r22 24.3, 24.4; P3-07 r19 25.9 | yes (events only) |
| 8 | Identical battle tolls in a row (P3-07 critic (b); P3-11 1, "left for P5") | Each side's toll is multiplied by a keyed factor in [800, 1200]‰ (`toll_spread_permille` 200, stream `hist.conflict.toll`); a check keeps the victors' toll below the defeated's | P3-06 r22 24.5; P3-01 r15 3.8 | yes |
| 9 | A one-year flare-up cut by the final phase (P3-07 critic (b)) | No flare-up shorter than `flare_min_years` (10, scaled): the quiet row goes on; 11 of 393 flare-ups at defaults | P3-04 r19 22.3; P3-01 r15 3.8 | yes |
| 10 | The history report v2 (critic B1 to B6; audit A3) | `# worldgen history v2` with P5's growth rule; at most two calamity candidates per phase at summary (B1); ruins in words, faded apart (B2, B3); adversary lines for wars only and one count line (B3); ended realms in short (B4); every flare-up with a war shown (B5); one end year per war, colonies before calamities, `[stats]` at summary cut to 15 keys (B6, A9); `find` for `std::map::at` (A3) | P3-07 r19 25 | no (report only) |

The wording points, one by one (the sentence now, the sentence wanted, the change):

| Point (source) | Now (0.20.0) | Wanted (0.21.0) | Change |
| --- | --- | --- | --- |
| A war over another region than its claim (P3-11 N4, B6) | "Clan A'thuukh declared war on the Kingdom of Sunt over East Wood. (The claim, since 5930, was to Woodward's Shaw.)" | full: "Clan A'thuukh laid claim to East Wood, held by the Kingdom of Sunt." then the declaration; the note "(Claimed since 5930.)" | item 1 |
| A city (A10) | "Lafin grew into a city of 8,017." | "Lafin in the Holt of Yews grew into a city." | item 4 |
| A toll (F6) | "…; 24 of the defeated and seven of the victors fell." | "…; 24 of the defeated and 7 of the victors fell." | item 6 |
| A sack (F6's rule, same clause) | "…; eight died and 19 fled." | "…; 8 died and 19 fled." | item 6 |
| A dropped claim (24.7) | nothing | "The Grotch Mob let the claim against the Drolidrai Throne over Bellringer's Forest lapse." (full only) | item 7 |
| A refuge (24.7) | nothing | "Orz'thugh Camp lay hidden from 5891 to 5990." (full only) | item 7 |
| A one-year flare-up | "In 5699 tensions flared into one year of upheaval." | no flare-up; the quarrel goes into the final phase | item 9 |
| A ruin by abandonment (B2) | "…: ruined 4277, e295 settlement_abandoned_to; last held by …" | "…: ruined 4277, e295 abandoned, survivors to <ref>; last held by …" | item 10 |
| An adversary with no war (B3) | five lines "…: no war; now none" | "without war: 5 (enmity 0, none 5)" | item 10 |
| A war's end (B6) | chronicle "the war ended after six years" (5753), block "5747-5752" | block "5747-5753" | item 10 |

## 3. Order of implementation

One branch, one engineer and one test writer, working from the stage specs separately. `GENERATOR_VERSION` becomes **0.21.0** in step 1, the first step that changes a world byte. Every golden is re-recorded once, in step 8. Intermediate commits may fail the world-golden tests; the merged branch must not.

| Step | What | Spec | Output |
| --- | --- | --- | --- |
| 1 | Data: `placename` lists (`common.toml`, `traits.toml`); name filters; dwarven `forbidden`; `history.report` (the `became_city` template; entries `claim_dropped`, `refuge_hidden`); `history.conflict` (`quarrel_strength_permille`, `toll_spread_permille`, one check); `history.engine` (`flare_min_years`, one check). In `game/data` and both fixture roots | P1-06 16; P3-02 15; P3-05 24; P3-06 24.6; P3-04 22.3; P3-01 3.8 | `content_hash`, names, region names |
| 2 | Renderer: number clauses | P3-04 22.1 | sentences only |
| 3 | Catalogue: kinds 41 and 42 | P3-04 22.2 | catalogue tables |
| 4 | Engine: `flare_min_years` in SC6 | P3-04 22.3 | `phase` rows, events after them |
| 5 | Conflict: the fresh claim, the quarrel's strength test, `claim_dropped`, `refuge_hidden`, the toll factors and their stream | P3-06 24.1 to 24.5 | history tables |
| 6 | Report: format v2 and `history_note`'s year (`digest.cpp`, `history_report.cpp`), A3 | P3-07 25 | report text |
| 7 | Docs: `docs/specs/schema.md` 6 gains kinds 41 and 42; `report --help` | P3-04 22.2; P3-07 25.14 | — |
| 8 | Goldens and figures: every golden of section 4; the README hash | 4 | — |

### 3.1 Per role

- **Engineer.**
  - `render_event` (P3-04 22.1): clauses and the all-or-none number words, beside 20.3's separators and 21.1's words.
  - `event_catalogue.cpp`: kinds 41 and 42.
  - The scheduler (`schedule.cpp`, SC6): `flare_min_years`, scaled as the detail lengths are.
  - `conflict.cpp`: 24.1 in "Declaring" (the fresh claim, its kind by 23.2's rule, its two causes, the war's cause); 24.2 in `held_back` beside 22.1's count; 24.3 where 20.2 item 2 drops the claim.
  - `conflict_war.cpp`: 24.4 in the flight right after `refugees_settled`; 24.5 in the battle's losses.
  - `streams.hpp`: `hist.conflict.toll` after `hist.conflict.search`.
  - `kinds_history.cpp` (or wherever `history.conflict` and `history.engine` are read): the three fields and checks.
  - `digest.cpp`: 25.8's year (root of a fresh claim), 25.9's two lists, 25.10's `find`; `history_report.cpp`: 25.1 to 25.7.
  - Data: `game/data/placenames/{common,traits}.toml`, `game/data/name_filters/{slurs,famous,crude}.toml`, `game/data/cultures/dwarven.toml`, `game/data/history/{report,conflict,engine}.toml`, and the copies under `worldgen/tests/data/geo/` and `geo-small/`.
  - Defaults of hand-made tuning built in code: `toll_spread_permille` 0, `quarrel_strength_permille` 0, `flare_min_years` 1 (every rule off).
- **Test writer** (from the specs, not the C++).
  - `p1_regions_reference.py`: P1-06 16 (the lists; the golden).
  - `p3_names_reference.py`: P3-02 15, with the scan of 13.1's sample per culture for the new entries.
  - `p3_engine_reference.py`: P3-04 22.1 in its sentence lines; 22.3 in SC6 (`ForcedTension` cases).
  - `p3_conflict_reference.py`: P3-06 24.1 to 24.5 with invariants (18) to (21), the self-check of 24.8 item 1; the participant bound 62 (P3-04 22.2); the open claims and hidden refuges at E for P3-07 25.9 (T19).
  - `p3_report_reference.py`: P3-07 25.
  - `p3_data_reference.py`: the fields, lists and checks.
  - Tests: P1-06 16.3, P3-02 15.4, P3-04 22.4, P3-05 24, P3-06 24.8, P3-07 25.13, P3-01 3.8.
- **Validation.** The sweep of P3-06 15.6 at 0.21.0 (the 375 jobs of the P3-10 and P3-11 rounds, so every figure compares pair for pair with `~/.cache/the-unmapped/out/p3-11-validation/sweep/`), section 8's targets, and P5-00 8.2's regression list. The sweeps read `[stats]` from `report --history --level full` from now on (P3-07 25.7 item 4). A missed target goes to the lead before the merge, and validation changes no tuning on its own judgement.
- **World critic.** The orc age at defaults, `calm` and `turbulent` (first wars, T9 worlds, the later orc age); the region names (the share and the feel of the six uncanny words); the v2 summaries of the phrase world, seeds 0, 5 and 7 and `civs4` seeds 3 and 7 (the same worlds as the r17 review); the new sentences.

## 4. Effect on output: golden values

**Every world file changes.** The data edits change every `content_hash`, the P0 empty worlds included, and `GENERATOR_VERSION` changes in `meta`. The empty world's size is unchanged (no table).

| Golden file | Re-recorded | What moves | What must not move (a difference is a defect) |
| --- | --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt` | yes, by the validation specialist after the cross-preset comparison | every case | — |
| `GOLDEN_CASES` (`db_determinism_test.cpp`), `stages_pipeline_test.cpp`'s hashes | yes | `meta.generator_version` | every other byte of those worlds |
| README's `--seed-phrase "The Unmapped"` hash | yes | — | — |
| `core_vectors.txt`, `p1-01-elevation.txt` … `p1-05-resources.txt` | no | — | every line |
| `p1-06-regions.txt` | yes | the `region` table hash of every case with a drawn name (P1-06 16.3) | the layer, `region_border`, `region_biome`; the cases whose regions are all keepers |
| `p3-01-data.txt` | yes | the data roots' content hashes | — |
| `p3-02-names.txt` | yes | dwarven names with `didin` or `ririn`; names `fag`, `shiva`, `hindi`, `fannie` reach | names none of these reach |
| `p3-03-chronology.txt` | yes, where a god name changes | god names the new entries reach | `race`, `race_biome`, `culture`, `age`, the awakening years |
| `p3-04-engine.txt` | yes | stub sentence lines with two numbers in a clause; `ForcedTension` cases with a flare-up shorter than 10 scaled years; the catalogue rows (kinds 41, 42; template of kind 9) | the schedule cases without short flare-ups |
| `p3-05-peoples.txt` | yes | every history line (the region names of every world change) | σ and C lines of every race |
| `p3-06-conflict.txt` | yes | every history line | — |
| `hist-conflict-73-1024` (P3-10 addendum's slow case) | yes, from the reference | every line | — |
| `p3-07-report.txt` | yes | every report (header v2 at least) | — |

`world-hashes.sh` keeps its fifteen cases. The history cases' `.report.txt` gain `events_claim_dropped` and `events_refuge_hidden`; their `.history.summary.txt` and `.history.full.txt` are v2.

## 5. Measurements

The spec writer ran prototypes for items 2, 3 and 10 on the stored 0.20.0 worlds of the P3-11 validation sweep (`~/.cache/the-unmapped/out/p3-11-validation/sweep/`), each built on the test writer's reference, which reproduces those worlds exactly. The harnesses are in `~/.cache/the-unmapped/out/p5-01-spec/`:
- `grim5.py`: P1-06's RG11 (`p1_regions_reference.py`) with edited `placename` lists, on the stored region rows (P1-06 16.2);
- `proto51.py`: the P3-06 reference (`p3_conflict_reference.py --version 0.20.0`) with a variant of 22.1's `held_back`, writing the recomputed history into a copy of the world; `orcsum.py` and `firstwar.py` read the orc statistics from it (they reuse validation's `orc11.py`). The unchanged run of seed 0 gives the stored tables byte for byte (P3-06 24.2);
- `b1proto.py` and `b5.py`: the summary set of P3-07 24.3 from `p3_report_reference.py`, with 25.2's cap and 25.6's flare-ups (P3-07 25.2, 25.6).

Counted directly from the stored 0.20.0 rows: re-aimed declarations (280, from the P3-11 validation), flare-ups shorter than 10 years (11 of 393 at defaults), repeated battle tolls (361 of 9,066 battles in 83 worlds) and mixed tolls (230), the names the new filter entries reach (P3-02 15.3), and the lengths of the r17 sample summaries (P3-07 25.5).

*To be measured by validation*: everything of section 8 at 0.21.0; the fresh claims and dropped claims per world; refuges hidden at E; the rejection rates of P3-02 15 per culture; every typical range of P3-06 15.5; timing (P5-00 8.1: at most 5 s more at 1024).

## 6. Tests to add

Each stage spec lists its own. In summary:
- **P1-06 16.3**: the edited lists in all three data roots; `p1-06-regions.txt`.
- **P3-02 15.4**: `Shyfaglae`, `Shiva`, `The Hindi Court`, `Fannie Glade` filtered; `didin`, `gririnhard` invalid, `dadin`, `kadin` valid; the redundancy check on the edited lists; `fagg` absent.
- **P3-04 22.4**: number clauses (seven table rows, cut and no-cut cases); kinds 41 and 42 in the catalogue and `template_problem`; `flare_min_years` in SC6 and its check; the reference's bound 62.
- **P3-05 24**: the `became_city` template.
- **P3-06 24.8**: the fresh claim in each re-aim case and its self-check; the quarrel's strength test; `claim_dropped`; `refuge_hidden` (orc, goblin, hidden years 0); the toll factors and their check; invariants (18) to (21).
- **P3-07 25.13**: header; calamity cap; ruins; adversaries; ended blocks; flare-ups; wording; the claim note's year; refuges and standing claims exact; invariants 25.12.
- **P3-01 3.8**: the three fields' ranges and absence; the checks `conflict_toll_spread` and `engine_flare_min`.

## 7. Schema

**No schema change** (schema version 7; `docs/specs/schema.md` 6 gains kinds 41 and 42 in its catalogue section, as for every appended kind).
- **Kinds 41 `claim_dropped` and 42 `refuge_hidden`** are appended (P3-04 8.2: appending a kind is not a schema change). They touch the contents of `event_kind` and `event_kind_role` only (two rows in `event_kind`, two in `event_kind_role` for each; no `event_kind_value` row). Their ids are provisional until the lead fixes the merge order (P5-00 4.4); P5-01 merges first, so they are expected to stay 41 and 42.
- **Why events, not columns** (P5-00 6.1, the overview's interim reading). A hidden-until year on `settlement` or a `dropped` flag on a claim would be columns, hence a schema bump in P5's first stage, and a claim is not a row of any table. Each fact is an event with a year, a place, participants and a cause, which P4's timeline and P6's quest hooks read like every other fact (P5-00 10.1: "a standing claim" is a quest seed).
- **Why kind 42 is a span.** The refuge's hidden years end at a year fixed at the flight (P3-06 20.1, 22.2); writing that year as the event's `end_year` needs no event when the hiding ends (which would fall inside a quiet step, against P3-04 W2 and W9's budgets). An event whose `end_year` lies after its `year` is P3-04's (W2; `flare_up`'s phase, `quiet_stretch`).
- **Why not a role or value on kinds 20, 21 or 32.** The fresh claim is a claim (kind 20 or 39 with their structures); the cause list says what it answers. A `hidden_until` value on kind 32 would be a new value key of a shipped kind, a schema change by P3-04 8.2.
- **The template of kind 9** is data (P3-04 8.2: "changing a template … is a data change").
- **The number clauses** (P3-04 22.1) refine a rendering rule: no stored value changes.
- **New data fields**: `history.conflict.quarrel_strength_permille`, `history.conflict.toll_spread_permille`, `history.engine.flare_min_years`, with two named checks (`conflict_toll_spread`, `engine_flare_min`). With the list edits they change `content_hash` only.
- **The report's v2** changes no world byte (P3-07 3.3).
- **No proposals entry for a schema change** is needed; section 11's entry records the scope.

## 8. Targets (P5-01's validation)

On the sweeps of P3-06 15.6 at 0.21.0, at 1024 defaults (100 worlds, seeds 0–99) unless stated, against the 0.20.0 figures (`docs/reviews/p3-11-followups-validation.md`, `docs/reviews/p3-11-followups-world-critic.md`, P3-06 r21 23.1, and this spec's counts on the same stored worlds). "Orcs" means every civilization of race orc together.

| Target | Definition | 0.20.0 | Bound at 0.21.0 |
| --- | --- | --- | --- |
| **T1** | hollow wars ‰ over the sweep (P3-09, P3-07 21.8 item 2) | 224 | at most 250 |
| **T2** | `wars_same_goal_unchanged_max` ≤ `weary_wars` in every world (P3-06 20.23) | met | every world |
| **T5** | orc wins against orc losses (`peace`, `war_won`) | 168 / 95 | wins ≥ losses |
| **T9** (restated by the lead's ruling) | worlds in which the orcs have exactly one standing settlement at E, won no war (no `peace` or `war_won` as `winner`, no `civ_conquered` as `conqueror`) and sacked nothing (no `sacked` as `attacker`) | **7** by this definition (seeds 7, 11, 55, 65, 76, 81, 88; the critic's 6 left out seed 55) | at most 7 (not worse) |
| **T10** (restated) | worlds in which no orc civilization acts after E − 250 (P3-06 22.4) and the orcs have at most two standing settlements at E | 0 | 0 |
| **T11** | median years from the orcs' awakening to their first colony, declaration, sack or capture (P3-06 23.1) | 9 | below 100 |
| **T12** (new) | the first war declared by an orc civilization in each world: won (`peace` or `war_won` with the orc as winner) against lost | **27 / 32** (and 14 truces, 9 searches, 1 open; 83 worlds) | won ≥ lost (prototype of 24.2 alone: 28 / 18) |
| **T13** (new) | invariant (18) of P3-06 24.7 (every war cites a claim on its own region, its attacker the claimant) in every world of the sweep; fresh claims per world reported | 280 of 2,621 declarations cite a claim on another region | 0 such declarations |
| **T14** (new) | grim drawn region names (P1-06 15.2's class) per mille of drawn names, at 1024 and at 2048 (512 reported) | 90‰ (390 of 4,328); 73‰ at 2048 | at most 50‰ each (prototype: 43‰, 34‰) |
| **T15** | names: none holding `fag`, `didin` or `ririn`; none with a word `shiva`, `hindi` or `fannie`; three equal letters, crude fragments, `tard`: none; fallbacks 0 (names and regions) | `Shyfaglae` 1, `didin` 8, `ririn` 19, `Shiva` 1, `Hindi` 1, `Fannie` 2 (375 worlds) | 0 each |
| **T16** (new) | battles that repeat an earlier battle's sides and both tolls exactly; battle sentences with a digit toll and a word toll in one clause | 361 (83 worlds); 230 | at most 10; 0 |
| **T17** (new) | the summary's calamity share (`famine`, `plague`, `plague_outbreak`, `succession_crisis` lines of the summary chronicle) | median 15%, max 53% (seed 0: 104 of 221) | median at most 10%, every default world at most 25% (prototype: 8%, 22%) |
| **T18** (new; restated by Paul's decision of 2026-11-22, "restate the report length target") | summary report lines (`report --history`) | default 644–806 (the r17 samples); `civs4` 1,270–1,358 | default worlds: median ≤ 700 and maximum ≤ 800; four civilizations per race (`civs4`): median ≤ 1,100 and maximum ≤ 1,400 (r0: at most 700 in every default world, 1,100 in every `civs4` world) |
| **T19** (new) | `[present]`'s standing claims and refuges marked `; hidden` equal the reference's open claims and hidden refuges at E | approximate (P3-07 24.7) | equal in every world of the sweep |
| **T20** (new) | flare-ups shorter than `flare_min_years` (scaled) | 11 of 393 | 0 |
| quick aim wins (P3-06 22.5) | as 22.5 defines them | 0 | at most 3 |
| T3, T4, T6, T7 (P3-09, P3-10) | as stated there | met | as before |
| P3-00 12.3 hard bounds | every world | met | every world |
| timing (P5-00 8.1) | `generate` at 1024, release, seed 0, three runs | about 9.3 s | at most 5 s more |

**Measured at 0.21.0** (r1; the engineer's sweep on `p5/01-int`, 100 default 1024 worlds unless stated):

| Target | Result |
| --- | --- |
| T1 | 215‰ |
| T5 | 169 / 62 |
| T9 | 3 |
| T11 | median 12.5 years |
| T12 | 38 / 17 |
| T13 | 0 of 2,601 declarations |
| T14 | 43.9‰ at 1024, 34.7‰ at 2048 |
| T15 | one region named `Tarden` (default seed 84), otherwise 0 each; the name filter does not apply to region names and the placename `forbidden` list has no `tard`; deferred (section 12) |
| T16 | 9; 0 |
| T17 | median 7.8%, maximum 22.8% |
| T18 | default 321–754 lines, median 571; 20 `civs4` worlds 890–1,353, median 1,046 |
| T19 | equal to the reference on all 12 golden world cases |
| T20 | 0 of 383 flare-ups |

**T12 and T8.** The first orc declaration comes a median **166** years after the awakening (0.20.0: 59), in **81** of 100 worlds (0.20.0: 83). Paul's decision of 2026-11-22: `quarrel_strength_permille` stays 1000; no lower value is measured (question 1 closed).

**Reported**, with no target:
1. **Orcs**: the first war's outcome by the number of standing settlements at the declaration and by the defender's race; the median years from the awakening to the first orc declaration (0.20.0: 59) and the worlds with one (T8, 0.20.0: 83); the worlds in which a quarrel claim was ever held back by 24.2, and for how long; T5, T9, T10, T11, T12 on `calm` and `turbulent`; orc people and standing settlements (0.20.0: 88‰ and 95‰).
2. **Claims**: fresh claims per world by the reason of the re-aim (lost, capital, weary; 0.20.0: 135, 104, 41); `claim_dropped` per world; standing claims at E per world.
3. **Refuges**: `refuge_hidden` per world; refuges hidden at E, by race.
4. **Battles**: the war's dead and the endings by reason against 0.20.0, to see 24.5's effect on exhaustion.
5. **Report**: per world, T, shown lines, calamity lines, declarations shown, empty phases, the length of each block; the same on the `civs4` set.
6. **Typical ranges**: every row of P3-06 15.5 at 0.21.0.
7. **Region names**: the plain share (unchanged: 33% / 12% / 4%), attempts and fallbacks, and a list of grim names left outside barren land for the critic.

## 9. Decisions made

1. **Item 1: the fresh claim is a claim of the existing kinds, caused by the old claim.**
   - It is written only when the goal moves (G ≠ R_c), as Paul's decision reads ("a war fought over another region than the claim it cites"). A claim whose region is unchanged but whose holder changed since is left as it is: its sentence was true when written.
   - Its causes are the old claim (the grievance moves with the war) and 5.3's cause on G (why G is the defender's). This answers 23.4's objection, "the new claim would have no cause of its own".
   - The war cites the fresh claim only (and the last peace), so invariant (18) holds without exception and the report needs no special case in a 0.21.0 world.
   - The report's note dates the grievance from the old claim (`Claimed since 5930.`), so the re-aim does not turn a long grudge into "Claimed the same year." (P3-07 25.8).
2. **Item 2: the gate is the odds without the aggression discount, held back as 22.1 holds back.**
   - The rules already compute force, the goal's defence and the odds at every declaration (20.6 item 3). The orcs' odds are discounted by their aggression (m = 500), which lets them declare on a foe twice their weighted force; the gate asks for the undiscounted odds of 1000, a fair fight by the declaration's own measure.
   - It applies to the quarrel claim only, like 22.1: after the first war with the quarrel neighbour every claim is ordinary, and the orcs' later wars keep 21.9's short odds. Paul's "gamble between collapse and conquest" (decision 5 of 2026-11-22) stays for every later war.
   - The critic's two other forms were not taken: a minimum population ("awakening population plus one colony's growth") is a new number unrelated to the foe's strength, and higher odds for a realm younger than 50 years is an age rule that the measure already covers when the realm is weak. Validation's lever `quarrel_min_settlements` 3 brings back part of the early silence (P3-06 23.1) and does not look at the foe.
   - The data lever is `quarrel_strength_permille`: lower values let the orcs start their quarrel against a stronger foe (700: a foe up to 1/0.7 of their force).
3. **Item 3: spirits and curses stay common; death and mourning go to barren land.** The edit follows r6's method (move, not remove) and the measured share halves (P1-06 16).
4. **Item 4: the region in place of the figure.** The city's region is the information a summary reader lacks; the figure stays in the refs.
5. **Item 5: `fag` anywhere.** As `tard` (P3-02 14.3): the fragment is a stressed syllable inside a word. `hindi` and `fannie` were listed by the critic "for the next filter edit"; this is it.
6. **Item 6: clauses, not sentences.** A sentence-wide rule would bring back "spread to 2 other settlements; 1,084 died" (P3-11's fix): the literal `; ` and `, ` of the templates already separate the counts that belong together from those that do not.
7. **Item 7: events, a span for the refuge** (section 7).
8. **Items 8 and 9 are in**: each is one rule and one field, with the old behaviour at the field's off value, and each fixes a line the critic quoted three times since P3-08 (tolls) or once with a cause in the schedule (the short flare-up). The other "also consider" items are left (section 1).
9. **Item 10: the cap on calamities over a floor on T** (measured, P3-07 25.2); v2 rather than a redefinition of v1, since v1 was released with P3-07's merge.

## 10. Open questions

1. **The quarrel gate's cost in time** (r1: **closed** by Paul's decision of 2026-11-22: `quarrel_strength_permille` stays 1000, no lower value is measured; at 0.21.0 the first orc declaration comes a median 166 years after the awakening, in 81 of 100 worlds). (For the lead; non-blocking.) The strength test (P3-06 24.2) makes the first orc war winnable (measured: 28 won, 18 lost, against 27 and 32 at 0.20.0; T5 153 / 65; T9 6; T10 0; T11 9), but it delays it: the median first orc declaration comes 181 years after the awakening instead of 59, and orcs declare in 72 worlds instead of 83 (T8, a reported statistic). The colony still comes first (T11), so the orcs are not silent; they are building. Recommendation: accept; the orc age at defaults is about 450 years, so a war around +180 still leaves the last age to the orcs. If the lead or the critic finds the first two centuries too quiet, the data lever is `quarrel_strength_permille` 700 (not prototyped; expected between the two columns of P3-06 24.2's table). Interim: 1000.
2. **The `civs4` summary length** (r1: **closed** by Paul's decision of 2026-11-22, which restates T18 as medians and maxima, section 8). (For the lead.) With B3, B4 and the `[stats]` cut the `civs4` summaries are expected at about 1,040 to 1,095 lines, above P5-00 7.2's "about 1,000" (which is stated for default worlds, which fall to about 510 to 670). Recommendation: accept T18's 1,100 for `civs4`; a further cut (at most 6 milestones for every realm at summary) would bring them under 1,000 at the cost of every default world's blocks. Interim: as specified.
3. **Items 8 and 9 were optional** (for the lead). Both change balance a little: the toll factor moves every war's dead and so some endings (T1, T5, T12 are re-measured), and `flare_min_years` removes 11 of 393 flare-ups at defaults. Recommendation: keep both. Interim: in, with their off values (`toll_spread_permille` 0, `flare_min_years` 1) as the fallback if validation finds a target missed through them.

## 11. Proposals

One entry in `docs/design/proposals.md`: "History and report: P5-01 carried-over follow-ups (scope)", recording this change's scope, the readings of section 9 that the design does not settle (the gate's measure, events for the two records, the report's v2), and questions 1 to 3. No item changes the design document:
- the fresh claim follows Paul's decision 6 of 2026-11-22;
- the orc age keeps "their coming opens the most recent age of conflict" (design 5): the first war is delayed, not removed;
- the grim share follows Paul's decision of 2026-11-22, item 3;
- the new kinds are appended within schema 7, as P5-00 6.1 plans.

## 12. Left for later

Listed here, not specified. Sources: the P3-07 world critic's (b) and (c), the P3-11 world critic's F3 to F6.
- **Repeated succession crises at one town** (nine at Natch Pit, turbulent seed 14; eight at Gnug, default seed 76): P5-06 makes succession a ruler's line.
- **Repeated held sieges of one town and runs of seat moves** (five sieges of Beckford, default seed 42; four seat moves in seven years, default seed 76): a grouping of chronicle lines in the report, a v2 amendment that needs a reading of its own (P3-07 25.1's growth rule covers blocks and appended lines only).
- **The faerie words `Glee` and `Faith`** (critic F3): Paul left the faerie names as they are on 2026-11-22; to the next names change if he wants them gone.
- **A realm that failed to find the same hidden realm twice** (critic F6, 43 fruitless searches in 27 worlds): a balance pass.
- **Large tributes** (8 above 5,000 at defaults, the largest 9,492): a balance pass.
- **Equal populations of sister villages** (critic F6): harmless now; the viewer will show it (P4).
- **A plain reading form of the report** (A9): Paul's choice of 2026-11-22 keeps ids and refs on every line.
- **`tard` in region names** (r1, T15): one region is named `Tarden` at 0.21.0 (default seed 84). The name filter (P3-02 11) does not apply to region names and P1-06's placename `forbidden` list has no `tard`. Deferred to the next change that alters world bytes: add `tard` to the placename `forbidden` list, or let P5-12's renamed regions go through the filter (P5-00 4.5), whichever comes first; for P5-12.

## Revision note r1 (2026-11-22, text after the integration)

Text only; the implementation and tests on `p5/01-int` (generator 0.21.0) already follow every point. Section 8: T18 restated by Paul's decision of 2026-11-22 (default median ≤ 700, maximum ≤ 800; `civs4` median ≤ 1,100, maximum ≤ 1,400), the measured results of the engineer's sweep, and T12 / T8 with Paul's decision to keep `quarrel_strength_permille` at 1000; section 10 questions 1 and 2 closed by those decisions; section 12 gains `Tarden` (T15) for P5-12. The amended stage specs' corrections of this round: P3-02 r18, P3-04 r20, P3-06 r23, P3-07 r20.
