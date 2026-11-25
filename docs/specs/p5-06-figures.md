# P5-06 Figures

Status: **r3, 2026-11-25: r2 brought in line with P5-04 as built (0.26.0), with Paul's two riders of 2026-11-24 and their recalibration.** r2 was approved by Paul on 2026-11-24 with the decisions of section 22 (scope C, the minimal casters in this stage; regnal numbers for humans and dwarves some of the time, 6.14; epithets of substantial deeds without the cap of two, 6.10; the other questions as recommended); r3 reopens none of them. r1 was the draft of the same day. The lead's rulings on r1's readings are in 23. Written by the spec writer on `p5/06-spec` (r1, r2 cut from `origin/p5/04-spec` at 32f7c09; r3 on the same branch after the merge of the P5-04 candidate, 52fd4f3: P5-04 r3 as built, generator 0.26.0, schema 11, report format v7). Phase P5 History depth, stage 06 (`docs/specs/p5-00-history-depth-overview.md` 3.1 "P5-06", with the minimal casters of "P5-09" folded in by Paul's scope decision of 2026-11-24, 24). Implemented after P5-04. Generator **0.27.0**, **schema 12**, history report format **v8**, event kinds **79 to 92** (final: 0.26.0's catalogue ends at 78, P5-04's 63 to 78 as r2 assumed).

**r3 changes** (details in the revision notes): (A) P5-04 as built: event ids final; 97 streams; browser group and site seam as built (`open_site` takes kinds 3 and 4, P5-02's loops test kinds 1 and 2); a library's glyph offset to the upper left, away from a mine's (11.4); a city seat with an open library raises its next caster's tower at the height (6.8); the realm reach for exiles defined directly (6.5); figures and alliances stated rule by rule (6.15); the cause-word kinds corrected to 10, 16, 31, 43, 45 (8.2); shard pattern `*history_figures_*`. (B) Paul's riders of 2026-11-24 (section 25): every ruin stays to E, sacked ones included (B1, 25.1); a famine or plague of significant impact is in the summary (B2, 25.2); the monster recalibration that goes with them, with two small undead rules for low pressure and large maps (B3, 25.3); T18's default bound 650. (C) Every keyed stream with its key (14); every reader check with its message (9.4); slow cases marked (18).

Sources: design doc 5 ("Entities": figures "with birth and death years and ties to each other"; "Quiet intervals": "ruling lines turn over as a count of generations … long-lived figures and monsters simply persist"; the race table: elves "very long-lived", goblins "short-lived", dwarves and faeries "long-lived"; "Peoples, beasts and monsters": each race has "lifespan, temperament and stances"; monsters "sometimes slain by heroes"; "Magic in history": "Some figures are casters. They found towers and libraries … write the spellbooks the player later finds, and sometimes cause disasters such as a blight"; the tick loop item 3 "Figures are born, rise, act on ambitions and die"; "From history to play": "a dead ruler gets a tomb", "an heir in exile"), 6 (`figure`, `relation`, `spell`, `spellbook`), 9 ("Magic": only to keep the seam), 14 ("Lives and legends"); P5-00 (3.1, 3.2, 4, 6, 7, 8, 10); P3-04 r23 (6.2 T3, 6.4, 6.6 W2 and W9, 6.7, 6.9, 8.2, 8.4); P3-05 (10.3 succession crisis and secession); P3-06 (5.2, 7.2, 7.3, 7.6, 7.7, 8.1); P5-02 r2 (6.4.2 hunts, sites, 9 names, 12.9 the summary limit); P5-03 r2 (9.3 hunts, 10.1 monster stories, 12.1 epithets); P5-03a r1 (3.5 `credit`, 9 the summary); P5-04 r3, as built at 0.26.0 (6.2 reach, 6.11 spoils, 7.2 allied strain, 7.6 joined allies, 7.7 allied battles, 7.16 the site seam, 9.1 reader checks, 11.1 the battle kinds set, 11.2 browser, 11.3 the mine marker); r3's riders: `docs/questions-for-paul.md` rows Q-RUINS-1 and Q-P5-01-1 and `docs/design/proposals.md` "Paul's answers of 2026-11-24 to the open questions" (every ruin stays; significant calamities recorded); P3-02 (4.3, 4.5 the reserved name kind `person`, 12.2); P3-07 r25 (24.2, 24.3) and P4-05 (4.4, 9). Paul's decision of 2026-11-24 on Q-P5-00-1: "take the recommendation except for mountain depths" (24).

## Plan (approved by Paul, 2026-11-24)

**What a world gains.** Named people who change what happens. Every realm has a ruling line; the record names the rulers a chronicle would: founders, the ruler under whom a realm went to war, broke faith, kept or shared spoils or fell, the rivals of a disputed succession, and the ruler at the end of the age. The rest of each line is counted. Realms at war may name a general who leads their battles and can fall in one; a realm raided by a monster may raise a hero who slays it or dies trying; a claimant who loses a disputed succession may flee into exile; casters raise towers and libraries, write named books and rarely call down a blight. Elves, faeries and dwarves outlive quiet ages; goblins and orcs live briefly. *"The Gub Horde under Grishnak declared war on the Hold of Durn over the Grey Fells."* · *"The Hold of Durn under Thrain the Unbeaten, with the Kingdom of Cear, defeated the Gub Horde in the Grey Fells; 812 of the defeated and 140 of the victors fell."* · *"Aldric took the throne of the Kingdom of Cear against the claim of Edric, who broke away with the far lands."* · *"Maela Dragonslayer of the Kingdom of Cear hunted down and slew Saelith the Black, a dragon of the old world, in the Cold Peaks."*

```
c4 The Hold of Durn
  rulers: 38 reigns, 6 recorded; first Dorin the Founder (f7); at the end Bolli (f71)
5402 e640 war_declared/70: The Gub Horde under Grishnak declared war on the Hold of Durn over the Grey Fells.
5581 e702 hero_slain/45: Maela Dragonslayer of the Kingdom of Cear was slain hunting the Brood of Skarr in the Red Crags.
[figures]
figures: 84; rulers 52, claimants 3, generals 17, heroes 6, casters 6; alive at the end 19
```

**The rules in short.**
- *Ruling line*: reigns of about one ruling generation of the race, each with a temper (steady, warlike, peaceable, grasping, just) drawn from the race's leanings; a ruler rules until death; a succession crisis ends a reign.
- *Generals*: each side of a new war may name one (often for elves and dwarves, seldom for faeries); the general leads that war's battles, may command again within a career, and may fall, likelier when beaten.
- *Heroes*: after a monster's raid; while one lives, the realm hunts sooner and stronger; a failed hunt may kill the hero.
- *Exiles*: a claimant who loses without splitting the realm may flee to the friendliest realm in reach, or to the wilds.
- *Casters* (part C, in this stage): rare; a tower (a library in a city) as a new kind of site, up to three named books, at most one blight.
- *Regnal numbers*: a new human or dwarven ruler sometimes takes the name of an earlier recorded ruler of the same realm and bears its number (*"Balin II"*, *"Aldric III the Victorious"*); elves, faeries, goblins and orcs never do. The first bearer stays unnumbered; a number never changes once given.
- *Epithets*: earned by deeds. An ordinary deed gives its epithet to at most two figures of a world; a substantial deed (a threshold per epithet in data: two realms conquered, a giant or a dragon slain, six battles won unbeaten) gives it whoever else bears it.
- *Quiet ages*: reigns and lives are drawn at their start, so a quiet step advances a line without ticking each year; a named figure due to die in it dies at its start.
- *Effects, bounded*: warlike rulers build strain 30% faster and peaceable ones 30% slower, scaled so each race's average is unchanged; grasping and just rulers move the chance of sharing spoils by 20 points and their strain toward allies ×2 and ×½; a general adds 10% to 30% to the side's odds; a hero adds to the hunt's chance and half again to its strength; a crisis after a short reign splits a realm 10 points more often.

**Scope: C** (Paul, 2026-11-24): rulers, disputes and exiles (part A), generals and heroes (part B) and minimal casters (part C: one table `book`, site kind 4, three event kinds, a marker glyph), in one stage. Schema 12, generator 0.27.0, report v8.

**Targets** (1024 defaults): 60 to 120 named figures a world, at least half rulers; 3 to 8 recorded rulers per realm; 10% to 30% of recorded human and dwarven rulers bear a regnal number, no other ruler does; no epithet on more than a tenth of a world's figures; a general in a quarter to a half of the sides at war; 2 to 8 heroes, who make a third to two thirds of the slayings; 1 to 4 exiles; 2 to 6 casters, 3 to 10 books, at most one blight; wars per world still 22 to 28, P5-03a's relations and P5-04's alliance targets still met; the summary about 20 lines longer (default median bound 645; r3: 650 with Paul's riders, 25).

**Decided by Paul** (section 22): scope C; rulers named by deed; regnal numbers for humans and dwarves some of the time, never for the other races; tempers lean by race with each race's average kept; a conquered realm's last ruler falls with it; epithets earned by deeds, at most two bearers for an ordinary deed and no cap for a substantial one; births are never events; a default summary median bound of 645.

## 1. Purpose

At 0.26.0 history has realms, settlements, monsters and peoples, but no person: ruling lines are a count of generations (P3-04 6.6, Q2), wars are fought by realms, monsters are slain by realms, and the design's figures, heirs in exile, tombs of dead rulers and casters' books have nothing to stand on. P5-06 adds:

- **Rulers** (part A): a ruling line for every realm (6.3), the rulers the record names (6.4), disputed successions with a named claimant and exiles (6.5), the ruler at the end of the age (6.12), regnal numbers for human and dwarven lines (6.14).
- **Generals and heroes** (part B): generals named at a declaration who lead their side's battles (6.6); heroes raised by monsters' raids who lead hunts (6.7).
- **Minimal casters** (part C): towers and libraries (`site` kind 4), named books (table `book`), blights (6.8). No spell, no school, no `spell` table (the full P5-09 is deferred, 20).
- **Effects** of figures on what happens, each bounded (7): tempers on strain, spoils and allied strain; a general's skill in battle; a hero in hunts; a short reign in secession.

Table `figure` holds every named figure; ties are `relation` rows with entity type 6 (9.2).

## 2. Items and where their normative text is

| # | Item | Normative text | Amends | Changes worlds |
| --- | --- | --- | --- | --- |
| A1 | Race fields `temper_weights`, `general_permille`, `hero_permille`, `magic_permille`; culture fields (with `regnal_permille`); monster field `slayer_epithet`; tuning `history.figures` | 4 | P3-01, P3-02 3, P5-02 4.2 | yes |
| A2 | `FiguresSubsystem`: the ruling line, naming, disputes, exiles, present rulers, deaths, epithets, names, regnal numbers | 5, 6.1 to 6.5, 6.9 to 6.14 | P3-04 6.4 (configuration) | yes |
| A3 | Seams: `FigureView`; tempers in strain, spoils and allied strain; the split bonus; housekeeping limited to civilization pairs; `draw_at` | 7.1 to 7.5, 7.9, 7.10 | P3-04 6.4, P3-05 10.3, P3-06 5.2, 7.6, 8.1, P5-04 6.11, 7.2 | yes |
| A4 | Events 79 to 83; renderer form `under`; template changes of six kinds | 8 | P3-04 8.2, 8.4, 8.5; data | yes |
| B1 | Generals; kind `battle_led`; the general's skill in battle | 6.6, 7.6, 8 | P3-06 7.3, P5-04 7.7 | yes |
| B2 | Heroes; kinds `monster_slain_by_hero`, `hero_slain`; the hero in hunts | 6.7, 7.7, 8 | P5-02 6.4.2, P5-03 9.3 | yes |
| C1 | Casters: site kind 4 `tower`, table `book`, kinds 90 to 92, placeholder `{@book}`, tower glyph | 6.8, 7.8, 8, 9.3, 11.3 | P5-02 11, P5-04 7.16, P4-04 3.6 | yes, schema 12 |
| D1 | Table `figure`, relation kinds, schema 12, reader checks | 9 | P3-04 9, `schema.md` | schema 12 |
| D2 | Report v8 | 10 | P3-07 (new section 31) | report |
| D3 | Battle kinds and monster death kinds sets; browser group and entity; markers | 11 | P3-07, P4-04, P4-05, P5-02, P5-04 11.1, P5-12 data | yes |
| D4 | Timeline reads | 12 | P4-01 (r7) | no |
| R1 | (r3, Paul's rider) every ruin stays to E; old ruins on the map | 25.1 | P3-05 8.6, 12.1; P5-03 11.1, 11.2; P3-07 6.6, 24.7; P4-04 3.3, 7, 8, 9 | yes |
| R2 | (r3, Paul's rider) major calamities in the summary | 25.2 | P3-07 24.3, 25.2; P5-02 12.9 | report |
| R3 | (r3) undead at low pressure and on large maps; the monster recalibration | 25.3, 15 | P5-03 14.2, P5-03a 6; data | yes |

## 3. Inputs, streams, draws

- **Live state** (`State`): civilizations (race, culture, capital, founded year and event, ended), settlements (owner, region, kind, cell, population), regions (owner), relations (`war`, `peace`, `alliance`).
- **Conflict's const reads** (P5-04 7.13, as built in `conflict.hpp`): `war_open(w)`, `war_end_event(w)`, `disposition_of(ctx, a, b)`, `at_war(a, b)` (any open `war` relation either way, joined rows included); **peoples**: `PeoplesView::hops(region)` (the open-border distances from a region), `population`; **monsters**: `next_site_id()`, `open_site(const SiteSpec&)` (P5-04 7.16; kind 4 from this stage, 7.8), the wild cell and the height of a region (P5-02 6.6.1), `monster_kind_of(m)`. `end_site_seam` is not used (towers never end in this stage).
- **The log**: every event from the subsystem's cursor, through `StepContext`'s reads (P3-04 23.3).
- **Data**: race fields of 4.1 and `lifespan_years`, `generation_years`, `awakening_year` (P3-01, P3-03); culture fields of 4.2; monster field of 4.3; `history.figures` (4.4).
- **Settings**: none. `param.event_intensity` does not scale this stage (the crises and wars it reads already scale by it).
- **Streams** (P5-00 4.2), appended to `streams.hpp` (the name constants and `all_stream_names`, whose size becomes 97) after `hist.alliances.mine`, the 82nd and last stream at 0.26.0, in this order; the list then holds **97** streams (14 has each key):

| Stream | a, b, k | Year (w0) | Section |
| --- | --- | --- | --- |
| `hist.figures.reign` | civilization c, reign number n, k: 0 length, 1 accession age, 2 temper | the reign's start | 6.3 |
| `hist.figures.regnal` | civilization c, reign number n, k: 0 the reuse test, 1 the name picked | the step's | 6.14 |
| `hist.figures.life` | figure f, 0, k: 0 age at entry, 1 age at death | the figure's entry year | 6.9 |
| `hist.figures.appoint` | the war's `war_declared` w, civilization c, vacancy number v | the step's | 6.6 |
| `hist.figures.skill` | figure f, 0, 0 | the step's | 6.6 |
| `hist.figures.fall` | the battle event b, figure f, 0 | the step's | 6.6 |
| `hist.figures.exile` | the `succession_crisis` e, civilization c, 0 | the step's | 6.5 |
| `hist.figures.hero` | the `monster_raid` r, civilization c, 0 | the step's | 6.7 |
| `hist.figures.caster` | civilization c, 0, 0 | the step's | 6.8 (part C) |
| `hist.figures.book` | figure f, 0, 0 | the step's | 6.8 (part C) |
| `hist.figures.blight` | figure f, 0, 0 | the step's | 6.8 (part C) |
| `hist.figures.tower_name` | site s, 0, 0 | the step's | 6.8 (part C) |
| `hist.names.person` | P3-02 4.2's attempt keys (t, culture, figure id, j) | attempt t | 6.11 |
| `hist.names.book` | book id, 0, 0 | the step's | 6.8 (part C) |
| `hist.monsters.hero_fall` | monster m, civilization c, 0 | the step's | 7.7 |

`D(stream; a, b, k)` = `StepContext::draw(stream, a, b, k)` (w0 the step's year, a quiet step's start). `D_at(stream; y; a, b, k)` = `StepContext::draw_at(stream, y, a, b, k)` (7.10: w0 = y, a year inside the step). `chance_pm`, `pick`, `weighted` as P5-02 3 and P3-02 4.1. All arithmetic int64 with checked multiplication; `div` is floor division of non-negative values; `clamp(v, lo, hi)` = min(hi, max(lo, v)). L is the step's length (1 in a tick), Y its year (a quiet step's start), Z = Y + L − 1 its last year.

**Why `draw_at`.** A reign or a life is drawn once, at its start year, whatever step reaches that year: so an elven reign that begins inside a quiet step has the same length, temper and ruler's age as if that year had been ticked, and a quiet step advances a line through several reigns without ticking each year (deterministic, keyed by the realm and the reign number, never by a loop position).

# Rules (parts A, B and C)

The three parts are one stage (Paul, 2026-11-24, 22 item 1): part A the rulers (4, 5, 6.1 to 6.5, 6.9 to 6.14), part B generals and heroes (6.6, 6.7, 7.6, 7.7, kinds 84 to 89), part C minimal casters (6.8, 7.8, kinds 90 to 92, `{@book}`, 9.3, the tower glyph 11.4; section 19). Rules of parts B and C are marked where they share a section.

## 4. Data

### 4.1 Race fields (amends P3-01)

Appended after `mining_permille` (P5-04 4.1) in every race definition (shipped and both fixture roots), all required:

| Field | Type | Meaning |
| --- | --- | --- |
| `temper_weights` | record {`steady`, `warlike`, `peaceable`, `grasping`, `just`} int(0, 1000) | the race's leanings of temper (design 5 and 6 "temperament"); check `race_temper_weights`: the sum is above 0 (`temper_weights must not all be 0`) |
| `general_permille` | int(0, 1000) | chance that a side of this race names a general at a declaration (6.6) |
| `hero_permille` | int(0, 1000) | chance that a raided realm of this race raises a hero (6.7) |
| `magic_permille` | int(0, 1000) | the race's bent for magic, scaling the rise of casters (6.8; part C; P5-09's `magic_schools` will refine it) |

Shipped (taste):

| race | steady | warlike | peaceable | grasping | just | general | hero | magic | why |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `elf` | 450 | 100 | 250 | 50 | 150 | 500 | 100 | 1000 | old, patient, learned |
| `goblin` | 300 | 300 | 50 | 300 | 50 | 250 | 40 | 150 | quarrelsome and greedy; shamans |
| `dwarf` | 450 | 150 | 100 | 200 | 100 | 500 | 120 | 300 | stubborn; rune-smiths |
| `faerie` | 400 | 50 | 350 | 50 | 150 | 150 | 60 | 900 | "faeries avoid battle"; old magic |
| `human` | 400 | 175 | 175 | 125 | 125 | 450 | 150 | 500 | adaptable; heroes |
| `orc` | 250 | 500 | 50 | 150 | 50 | 400 | 80 | 150 | "strong, warlike" |

Keyed by data, never by race key in code (P5-00 9.3). Lifespans are the existing `lifespan_years` (elf 700, faerie 400, dwarf 300, human 70, orc 50, goblin 40) and reigns follow `generation_years` (elf 150, faerie 100, dwarf 70, human 25, orc 18, goblin 15): no new lifespan field is needed.

### 4.2 Culture fields (amends P3-02 3)

Appended after `site_forms` (P5-02 4.4), all required (shipped and both fixture roots):

| Field | Type | Meaning |
| --- | --- | --- |
| `person_forms` | array(string(48, `name_form`), 1, 8) | forms of a person's name (P3-02 4.5's reserved kind `person`) |
| `tower_forms` | array(string(48, `name_form`), 1, 8) | a caster's tower; `{root}` is the caster's root (6.8; part C) |
| `library_forms` | array(string(48, `name_form`), 1, 8) | a caster's library in a city; likewise |
| `book_titles` | array(string(48, `book_title`), 4, 32) | titles of books (6.8); grammar `book_title`: ASCII letters, single spaces, `'` and `-`, starting with an upper-case letter |
| `regnal_permille` | int(0, 1000) | chance that a new recorded ruler of a line of this culture takes the name of an earlier recorded ruler of the same line and bears a regnal number (6.14); 0: never |

Shipped `regnal_permille` (taste; calibrated by FG10): `human` 250, `dwarven` 200, `elven` 0, `faerie` 0, `goblin` 0, `orcish` 0 (Paul, 2026-11-24: "humans and dwarves might have numbers some of the time the rest probably not"). A ruler's culture is its realm's, so the field is read from the realm's culture; it is a culture field, not a race field, because naming customs are culture (P3-02 3), and a later culture of the same race may differ.

Shipped: `person_forms` = `["{root}"]` in every culture (persons are named by their root; the field is the seam for patronymics or house names later). Tower, library forms and titles (taste; every word checked against the name filter by the engineer, a failing word a data defect):

| culture | `tower_forms` | `library_forms` | `book_titles` |
| --- | --- | --- | --- |
| dwarven | `The Rune-Tower of {root}`, `{root}'s Delving-Spire` | `The Rune-Halls of {root}` | The Deep Reckoning; On the Hearts of Stones; The Rune-Book of the Forge; The Iron Annals; On the Binding of Runes; The Book of Seams; The Hammer-Canon; The Grey Ledger |
| elven | `The Star-Tower of {root}`, `{root}'s White Tower` | `The Leaf-House of {root}` | The Lay of the First Stars; On the Speech of Trees; The Book of Falling Leaves; The Silver Canticle; On Starlight and Shadow; The Long Remembering; The Song of the Waking; The Mirror of Waters |
| faerie | `The Thorn-Tower of {root}`, `{root}'s Moonspire` | `The Dream-Hollow of {root}` | The Book of Glimmers; On the Dreams of Moths; The Dew-Book; The Little Mysteries; On Hidden Ways; The Thistle Pages; The Riddle of Hollows; The Book of Moonmilk |
| goblin | `The Smoke-Tower of {root}`, `{root}'s Bone-Tower` | `The Scratching-House of {root}` | The Black Scratchings; On Biting Things; The Book of Smokes; The Gnawed Pages; On the Fears of Elves; The Rot-Book; The Twelve Curses; The Book of Spite |
| human | `The Tower of {root}`, `{root}'s Tower` | `The Library of {root}` | The Codex of Hours; On the Nature of Fire; The Grey Grimoire; The Book of Wards; On the Turning of the Stars; The Treatise of Bindings; The Book of Keys; The Ember Codex |
| orcish | `The Skull-Tower of {root}`, `{root}'s War-Spire` | `The Bone-Hall of {root}` | The Skull-Songs; On Blood and Iron; The Book of the Storm; The Bone-Tally; The Red Chants; The War-Runes; The Ash-Book; The Chant of Ruin |

### 4.3 Monster field (amends P5-02 4.2)

`slayer_epithet`, string(24, `epithet`), required, after `raid_phrases` (P5-03 9.5): the epithet a hero earns by slaying a monster of the kind (6.10). Grammar `epithet`: ASCII letters, single spaces and `-`, not starting or ending with a space. Shipped: dragon `Dragonslayer`, giant `Giantsbane`, ogre `Ogrebane`, troll `Trollbane`, undead `the Gravewarden`, wyvern `Wyrmbane`. Not stored in the world (the epithet a hero earns is stored on the figure row, 9.1).

### 4.4 Tuning `history.figures` (new fixed id of kind `history`; `game/data/history/figures.toml` and both fixture roots)

All required, in this order. Starting values; the engineer calibrates them (15.2) and r4 (as built; r3 is the alignment with P5-04 and the riders) records the shipped ones.

| Field | Type | Start | Meaning |
| --- | --- | --- | --- |
| `name_rulers` | bool | true | rulers and claimants are named (6.4, 6.5, 6.12); false: none (the `zero` tuning) |
| `reign_min_permille` | int(1, 4000) | 500 | the shortest reign, per mille of `generation_years` |
| `reign_span_permille` | int(0, 4000) | 1000 | the span above it (6.3): reigns of 0.5 to 1.5 generations |
| `accession_age_permille` | int(0, 1000) | 250 | the youngest age at accession, per mille of `lifespan_years` |
| `accession_span_permille` | int(0, 1000) | 250 | the span above it |
| `temper_strain_permille` | record {`steady`, `warlike`, `peaceable`, `grasping`, `just`} int(0, 4000) | 1000, 1300, 700, 1000, 1000 | strain rate factor by the attacker's temper, before the race normalization (7.2) |
| `temper_allied_permille` | record, int(0, 4000) | 1000, 1000, 1000, 2000, 500 | factor on strain toward an ally (P5-04 7.2) |
| `temper_spoils_permille` | record, int(−1000, 1000) | 0, 0, 0, −200, 200 | added to the chance of granting a share of spoils (P5-04 6.11) |
| `short_reign_permille` | int(0, 4000) | 500 | a reign shorter than this per mille of `generation_years` is short (7.4) |
| `short_reign_split_permille` | int(0, 1000) | 100 | added to `split_permille` at a crisis after a short reign |
| `exile_permille` | int(0, 1000) | 250 | chance that a crisis that does not split exiles a named claimant (6.5) |
| `exile_hops` | int(0, 8) | 3 | a host lies within this many open borders |
| `general_scale_permille` | int(0, 1000) | 1000 | factor on every race's `general_permille`; 0: no generals |
| `skill_min` | int(0, 1000) | 100 | a general's skill, per mille added to its side's odds |
| `skill_max` | int(0, 1000) | 300 | |
| `career_permille` | int(0, 1000) | 300 | a general is named again only within this per mille of `lifespan_years` of the first command |
| `fall_loser_permille` | int(0, 1000) | 150 | chance that a beaten general falls |
| `fall_winner_permille` | int(0, 1000) | 30 | chance that a victorious general falls |
| `general_age_permille`, `general_age_span_permille` | int(0, 1000) | 300, 300 | age at a first command, per mille of lifespan |
| `hero_age_permille`, `hero_age_span_permille` | int(0, 1000) | 150, 200 | age at a hero's rise |
| `life_min_permille`, `life_span_permille` | int(0, 4000) | 700, 500 | age at death of generals, heroes, claimants and casters, per mille of lifespan |
| `hero_scale_permille` | int(0, 1000) | 1000 | factor on every race's `hero_permille`; 0: no heroes |
| `hero_hunt_permille` | int(0, 1000) | 30 | added to a hunt's yearly chance while the realm has a hero (7.7) |
| `hero_might_permille` | int(0, 4000) | 500 | added to the hunters' strength, per mille |
| `hero_fall_permille` | int(0, 1000) | 300 | chance that a failed hunt led by a hero kills the hero |
| `caster_ppm` | int(0, 1000000) | 1500 | yearly chance per realm, in ppm, before `magic_permille` (6.8; part C); 0: no casters |
| `caster_age_permille`, `caster_age_span_permille` | int(0, 1000) | 400, 300 | age at a caster's rise |
| `book_permille` | int(0, 1000) | 10 | yearly chance that a caster writes a book |
| `books_max` | int(0, 16) | 3 | books per caster |
| `blight_ppm` | int(0, 1000000) | 500 | yearly chance in ppm that a caster's workings blight the region |
| `blight_toll_permille` | int(0, 1000) | 80 | the share of each settlement of the region a blight kills |
| `regnal_max` | int(2, 39) | 9 | the highest regnal number; a name with this many recorded bearers in a line is no longer reused (6.14) |
| `epithets` | array(record {`role`: enum(`ruler`, `claimant`, `general`, `hero`, `caster`), `deed`: enum (6.10), `min`: int(1, 100000), `substantial`: int(0, 100000), `text`: string(24, `epithet`)}, 0, 32) | 6.10 | epithets earned by deeds, in order; `min` the ordinary threshold, `substantial` the substantial one (0: the epithet has no substantial form) |
| `epithet_repeat_max` | int(1, 100) | 2 | figures of a world that may bear one epithet text earned by an ordinary deed (6.10); a substantial deed is not capped |

Checks (with the history data checks): `figures_skill_ordered` (`skill_min <a> is above skill_max <b>`); `figures_epithet_slayer`: an `epithets` entry whose text is `{slayer}` must have deed `slew_might` (`epithets[<i>]: {slayer} needs the deed slew_might`); `figures_epithet_thresholds`: `substantial` is 0 or at least `min` (`epithets[<i>]: substantial <s> is below min <m>`).

**The `zero` fixture tuning** (proof obligation I0, 7.11): `name_rulers` false, `general_scale_permille` 0, `hero_scale_permille` 0, `caster_ppm` 0, `exile_permille` 0, `short_reign_split_permille` 0, every `temper_strain_permille` and `temper_allied_permille` value 1000, every `temper_spoils_permille` value 0; the others as shipped.

## 5. State

`FiguresSubsystem` (`worldgen/src/history/figures.cpp`, header `worldgen/history/figures.hpp`; `figures_casters.cpp` as the engineer likes) keeps, in its own members (ids only, never references into `State`):

- **Figures**, ids 1, 2, … in creation order (H2; at most 100,000, fatal `history: bound: figures exceed 100000`): every column of 9.1 being built (name, epithet, role, race, culture, civilization, born, office year, start event, planned death year, died and death event, death region and settlement, status, reign number, regnal number, temper, skill), and its deed tallies (6.10): wars declared, wars won, conquests, betrayals, battles won, battles lost, monsters slain (with the last slain monster's kind and the largest `might` of the kinds slain), books, blights.
- **Lines**, per civilization (index civ id − 1): the current reign {number n, start year s, planned end e, temper τ, accession age, figure f (0 unnamed), tallies of the reign: wars won, conquests, betrayals}, or none before the realm's first reign; and the line's **recorded names** (6.14): the distinct base names of its recorded rulers, each with its count of recorded bearers, in the order of their first bearers.
- **Wars known**: per `war_declared` w read: attacker, defender, and per side the serving general (0 none) and the vacancy count v.
- **Heroes**: per civilization the living hero (0 none).
- **Books** (part C): ids 1, 2, … (at most 100,000): title, author, site, year, event.
- The **log cursor** (first event id not yet read).

Every container is a vector indexed by id or a `std::map` keyed by ids; no unordered container (14).

## 6. Rules

### 6.1 Steps and order

`steps()` = T3, T4, T6, Q4. Installed in `EngineConfig::subsystems` **after** `AlliancesSubsystem`: peoples, conflict, renaming, monsters, relations, alliances, figures (P5-00 4.1). Not a tension source. T3 is the figures' step of P3-04 6.2; this subsystem is the only one in it.

| Step | In this order |
| --- | --- |
| T3 | the log (6.2) [this year's T1 and T2 events, last year's T6]; lines (6.3); deaths of age (6.9); generals for open vacancies (6.6); casters: rises, books, blights (6.8; part C) |
| T4 | the log [peoples' crises and secessions, monsters' raids, slayings and heroes slain of this T4] |
| T6 | the log [battles, war ends, conquests, spoils of this T6]; generals standing down (6.6); at Y = E − 1 the present rulers (6.12), then every figure's epithet (6.10) |
| Q4 | the log [the quiet step's war ends, spoils, realm ends]; generals standing down (6.6); lines over the step (6.3); deaths of age (6.9); books (6.8) |

### 6.2 The log

At each of its steps the subsystem reads every event from its cursor to the last event id in ascending id, then advances the cursor. It acts on these kinds only, in id order (every other kind is skipped):

| Kind read | Action |
| --- | --- |
| `civ_founded` | the realm's first reign starts at the event's year and is named: the founder (6.4 rule 1) |
| `succession_crisis` | 6.5 (with the `civ_seceded` of the same step that cites it, if any) |
| `civ_seceded` not citing a crisis | the new realm's first reign is named, as `civ_founded` (none occurs in P3; a guard) |
| `war_declared` | the attacker's reign is named (6.4 rule 3) and its tally `wars declared` + 1; the war is recorded; generals (6.6) |
| `alliance_betrayed` | the betrayer's reign is named; its `betrayals` + 1 |
| `spoils_shared`, `spoils_withheld` | the principal's reign is named |
| `peace`, `war_won` | the winner's reign: `wars won` + 1 |
| `civ_conquered` | the conqueror's reign: `conquests` + 1; then the fallen realm's end (6.4 rule 4) |
| `civ_ended` | the realm's end (6.4 rule 4) |
| `battle_led` | its generals' tallies and falls (6.6) |
| `monster_raid` | a hero may rise (6.7) |
| `monster_slain_by_hero` | the hero's `monsters slain` + 1 and the kind (6.10) |
| `hero_slain` | the hero's death (6.9, status 5) |

"The reign is named" means: if the realm's current reign has figure 0 and `name_rulers` is true, 6.4's naming is applied with the event read as the record's start. A reign's tallies are kept on the reign while it is unnamed and move to its figure when it is named.

### 6.3 The ruling line (T3, Q4)

**A reign.** Reign n of civilization c (race r, g = `generation_years`(r), λ = `lifespan_years`(r)) starts at year s:

```
len(n)  = max(1, g × (reign_min_permille + pick(D_at(hist.figures.reign; s; c, n, 0), reign_span_permille + 1)) div 1000)
age(n)  = λ × (accession_age_permille + pick(D_at(hist.figures.reign; s; c, n, 1), accession_span_permille + 1)) div 1000
τ(n)    = weighted(D_at(hist.figures.reign; s; c, n, 2), temper_weights(r) in the order steady, warlike, peaceable, grasping, just)
e(n)    = s + len(n)                                                  // the planned end: the ruler's death
```

The first reign of c starts at its founding (the `civ_founded` or the claimant's secession, 6.5). A reign ends at its planned end, at a crisis (6.5) or with its realm (6.4 rule 4). The ruler rules until death: a reign's end is its ruler's death except when the realm ends first (6.4 rule 4).

**T3** (each alive civilization c ascending with a current reign): if e ≤ Y: the reign ends: if it is named, its ruler dies of age (6.9: `figure_died`, `how` 0); the next reign n + 1 starts at s = Y, unnamed.

**Q4** (each alive civilization c ascending): while e ≤ Z:
- if the reign is named: if `quiet_events_left(c)` = 0, stop (the death waits for the next step, the reign goes on); else its ruler dies, dated Y (6.9); the next reign starts at s = Y;
- else the next reign starts at s = e (the exact year: an unnamed reign needs no event);
- then n += 1 and the new reign's values are drawn at s (`D_at`, with Y ≤ s ≤ Z).

So a quiet step advances a line through any number of unnamed reigns (a goblin horde through one or two reigns per 25 years, an elven house through one reign in six steps), each drawn at its own start, and it writes at most one event per realm for its rulers (the death of a named ruler). No reign starts in a quiet step named; its ruler is named later by a deed, or at the end of the age.

**Crisis in the year of an accession.** A `succession_crisis` of year Y read in T4 for a reign that started in year Y does not end it: it is the dispute over that accession (6.5 with the current reign as the winner's).

### 6.4 Naming rulers

Every naming creates a figure (role 1 ruler, race and culture of the realm, civilization c, reign number n, temper τ(n), office year s, born = max(`awakening_year`(r), s − age(n)), planned death e(n)), draws its name (6.11) or takes an earlier ruler's name of the line with a regnal number (6.14), and opens relation **`ruled`** (a = the figure, type 6; b = c, type 3) with the naming's record event. Rules, in the order they can apply:

1. **Founder.** At a realm's founding (`civ_founded`, read in T3 of its year; a secession's founder in 6.5): append **`came_to_rule`** (region: the realm's home region; participants f `figure`, c `civilization`; cause the founding event); the record event is it.
2. **Disputed succession.** 6.5.
3. **A deed of the realm.** At a `war_declared` (attacker), `alliance_betrayed` (betrayer), `spoils_shared` or `spoils_withheld` (principal) read while the realm's reign is unnamed: the record event is that event; no event is appended. The ruler's accession lies before it (`office_from` = s ≤ the event's year).
4. **The realm's end.** At a `civ_conquered` (the fallen `civilization`) or `civ_ended`: if the reign is unnamed, it is named with that event as record; then the reign ends with the realm: `ruled` is closed with that event; the ruler's status 6 (fell with the realm), died = the event's year, death event = that event, death region = the event's region (no event is appended). Every open `rival` row of the ruler closes with it.
5. **The present.** 6.12.

A named ruler's reign ends by its death (6.9, 6.5) or by 4; `ruled` is closed with that event. `ruled` is asymmetric (the figure as a). A realm has at most one open `ruled` row at a time (13).

**Why deeds** (reading 23 L1; Paul's decision, 22 item 2). A default world has a median of about 740 reigns over its realms (about 360 of them beginning in ticked years; goblin hordes alone about 245: one reign each 15 years), measured from 78 stored worlds of the P5-03a validation sweep at 0.25.0 with reigns of one generation. Naming every one would make rulers most of the log. A ruler enters the record for the same reasons a chronicle names them: founding, war, faith kept or broken, a dispute, a fall, the present. By the same measure about 55 to 67 rulers a world are named (median), 27 to 152 over the sweep. The count of reigns is not lost: the last recorded ruler's `reign_no` is the line's length (every alive realm has a recorded ruler at E, 6.12, and every ended realm a last ruler, rule 4).

### 6.5 Disputed successions and exiles (T4)

For a `succession_crisis` e of civilization c (P3-05 10.3, peoples in this T4), with σ the `civ_seceded` of the same step citing e (none: the crisis did not split):

1. **The reign ends** (unless it started in this year, 6.3): if named, its ruler dies (`figure_died`, `how` 1, the reign's death; 6.9); the new reign n + 1 starts at Y, unnamed so far.
2. **Split** (σ exists, c' its new realm) and `name_rulers`: name the new reign of c (the winner, f_w) and the claimant f_c as the **first reign of c'** (reign 1 of c', start Y, its values drawn as 6.3 with (c', 1)); append **`succession_disputed`** (region and settlement of e; participants c `civilization`, f_w `ruler`, f_c `claimant`, c' `new_realm`; value `outcome` 1; causes e and σ, ascending); `ruled` (f_w, c) and `ruled` (f_c, c') open with it; open **`rival`** (f_w, f_c) with it (symmetric, smaller id first); if the dead ruler f_d of c was named: open **`parent`** (f_d, f_w) and (f_d, f_c) with it (f_d as a).
3. **No split**, `name_rulers`, and `chance_pm(D(hist.figures.exile; e, c, 0), exile_permille)`: name f_w as in 2 and a claimant f_c (role 2 claimant, civilization c, race and culture of c; entry year Y; born and death by 6.9's life draws); append `succession_disputed` (participants c, f_w, f_c, no `new_realm`; value `outcome` 0; cause e); `ruled` (f_w, c), `rival`, and `parent` as in 2; then the **exile**: the host h = the civilization ≠ c, alive and visible, not at war with c (`at_war(c, h)` false), with hops(c, h) ≤ `exile_hops`, with the largest `disposition_of(c, h)`, then the smaller id; none: no host. (r3) **Reach** hops(c, h) is P5-04 6.2's definition, computed by this subsystem: the smallest `PeoplesView::hops(R_c)[R_h]` over R_c ∈ P(c), R_h ∈ P(h), P the presence of P3-06 5.1 (the regions a realm holds, live at T4); unreachable: out of reach. (As built, P5-04's breadth-first reach is private to `AlliancesSubsystem` and searches only to its own radius; the engineer may expose it as a const read when `exile_hops` ≤ that radius, since within it the distances are equal.) Alliances play no part in the choice of a host (6.15). Append **`exiled`** (region: h's capital's region, else e's region; participants f_c `figure`, c `civilization`, h `host` (0–1); value `hosted` 1 or 0; cause the `succession_disputed`); f_c's status 2 (in exile), its place the host's capital region (0 without a host).
4. Otherwise nothing is named: the new reign is unnamed; the crisis stays P3-05's.

So every secession's realm has a named founder (the claimant), and a crisis names its people only when it splits the realm or drives a claimant out (about 2 splits and 3 or 4 exiles a default world). An exile lives on among the host, or in the wilds, and dies of age (`figure_died`, `how` 2).

### 6.6 Generals (T3, T6, Q4)

**Named at a declaration** (T3, reading `war_declared` w with attacker a and defender b): record the war; for each side c ∈ {a, b} in that order, alive: v_c = 0 and the **appointment** below.

**Appointment** of side c in war w: c names a general when `chance_pm(D(hist.figures.appoint; w, c, v_c), general_permille(r_c) × general_scale_permille div 1000)`. The general is, first, c's living general not serving another war whose first command is at most λ_c × `career_permille` div 1000 years before Y (the smallest id), else a new figure (role 3 general, race and culture of c, civilization c, entry year Y, ages by 6.9, `skill` = `skill_min` + pick(D(hist.figures.skill; f, 0, 0), `skill_max` − `skill_min` + 1), drawn once per figure). Append **`general_appointed`** (region w's goal region; participants f `figure`, c `civilization`, the other side `enemy`; value `again` 1 for a general named before, else 0; cause w); open **`served`** (a = f, type 6; b = c) with it; the war's general of c = f.

**Vacancies** (T3, after the lines): for each known war w ascending that is open (`war_open`), each side c in the order (attacker, defender) whose general fell in it (6.6 below) and that has had no appointment since: v_c += 1 and the appointment draw. So a side whose general fell may name another the next year.

**Battles** (T6, reading `battle_led` b; conflict writes it, 7.6): for each general participant f, its tallies (`battles won` or `battles lost`); then f falls if `chance_pm(D(hist.figures.fall; b, f, 0), fall_winner_permille` for the winner's general, `fall_loser_permille` for the loser's`)`: append **`fell_in_battle`** (region of b; participants f `figure`, its civilization `civilization`, the other side `enemy`; cause b); f's death (status 4, death region the battle's region; 6.9); `served` closes with it; the war's general of that side = 0 (a vacancy).

**Standing down** (T6 and Q4, after the log): for each known war ascending that is no longer open, each side's serving general: `served` closes with `war_end_event(w)`; the war is dropped from the known wars once both sides are without a general.

A general leads only principals' armies (allies follow their partner's general, P5-04 7.7): generals are named only at a `war_declared`, for its attacker and defender, and never at an `ally_joined_war` (6.15). In a quiet step no general is named (no war is declared, P3-06 7.8) and every war ends, so every `served` row closes in the quiet step that ends its war.

### 6.7 Heroes (T4)

**Rise** (reading `monster_raid` r of monster m on civilization c): if c is alive and visible, has no living hero, and `chance_pm(D(hist.figures.hero; r, c, 0), hero_permille(r_c) × hero_scale_permille div 1000)`: a new figure (role 4 hero, race and culture of c, civilization c, entry year Y, ages by 6.9 with `hero_age_*`); append **`hero_rose`** (region and settlement of r; participants f `figure`, c `civilization`, m `monster`; cause r); c's hero = f.

**Hunts** are monsters' (P5-02 6.4.2, as amended in 7.7): while c has a hero, c hunts sooner and stronger, a successful hunt is the hero's (`monster_slain_by_hero`), and a failed one may kill the hero (`hero_slain`). This subsystem reads both in its T4 (6.2): a slaying adds to the hero's tallies; a `hero_slain` is the hero's death (status 5, death region the event's region), and c's hero = 0.

A hero serves the realm for life against any monster that raids it; a realm has at most one living hero. Heroes rise only in ticks (raids are T4's, P5-02 6.4).

### 6.8 Casters (part C; T3, Q4)

**Rise** (T3; each alive visible civilization c ascending with no living caster): p = `caster_ppm` × `magic_permille`(r_c) div 1000; if `chance_ppm(D(hist.figures.caster; c, 0, 0), p)`: the seat is c's capital t (none: no rise); if t is a city and no open site of kind 4 has t as its `settlement`, a **library** at t (settlement t, t's cell); else a **tower** at the height (P5-02 6.6.1) of t's region (settlement 0; no height: no rise). (r3) So one city holds at most one library: a later caster of a city whose library stands raises a tower; and since P5-02 6.6.1's height skips the cells of open sites ("taken"), two towers never share a cell. A new figure (role 5 caster, entry Y, ages by 6.9 with `caster_age_*`); the site id s = `next_site_id()`; its name: F = the culture's `library_forms` or `tower_forms`, start = pick(D(hist.figures.tower_name; s, 0, 0), |F|), the first form in cyclic order from start with `{root}` replaced by the caster's root that is valid (P5-12 5.2 items 1 to 4) and not in the registry, else the start form's name with ` ` and roman(k) (P5-02 9.2's fallback); added to the registry. Append **`tower_founded`** (region of the seat; settlement t for a library, else 0; participants f `figure`, c `civilization`, s `site`; value `library` 1 or 0; cause the larger of t's `founding_event` and c's `founding_event`); `open_site` (kind 4, the name, region, cell, settlement, `founder_civ` c, `monster` 0, origin Y and the event; P5-04 7.16).

**Books** (T3 and Q4; each living caster f ascending with fewer than `books_max` books): p = `book_permille` (T3), min(1000, `book_permille` × L) (Q4); if `chance_pm(D(hist.figures.book; f, 0, 0), p)` (and in Q4 `quiet_events_left(c)` > 0): book id k = the next; title: T = the culture's `book_titles`, start = pick(D(hist.names.book; k, 0, 0), |T|), the first title in cyclic order not in the registry, else the start title with ` ` and roman(j), the smallest j ≥ 2 free (P3-02 4.3's fallback); added to the registry. Append **`book_written`** (region of f's tower; participants f `figure`, c `civilization`, the tower `site`; cause the tower's `tower_founded` and, when f has a book, its last `book_written`); the book row (title, author f, site, year Y, event).

**Blights** (T3; each living caster f ascending with no blight yet): if `chance_ppm(D(hist.figures.blight; f, 0, 0), blight_ppm)`: R = the tower's region; for each standing visible settlement u of R ascending, dead_u = P_u × `blight_toll_permille` div 1000; `set_population(u, P_u − dead_u)` for dead_u > 0; append **`blight`** (region R; participants f `figure`, c `civilization`, the tower `site`; value `dead` = Σ dead_u; cause the tower's `tower_founded`); `note_reduction(u, blight)` for every u with dead_u > 0 (P3-05 13; a blight can be a later abandonment's cause). A blight changes no status. At most one blight per caster (`blights` tally).

A tower stays open after its caster dies (`ending` 0; a site for P6: the tower of a dead mage). A caster rises only in ticks; writes in ticks and quiet steps.

### 6.9 Lives and deaths

**Entry and death of a non-ruler** (a claimant, general, hero or caster; entry year y = Y of its creation):

```
age0   = λ × (A + pick(D_at(hist.figures.life; y; f, 0, 0), S + 1)) div 1000       // A, S: the role's *_age_permille, *_age_span_permille; claimants: accession_*
born   = max(awakening_year(r), y − age0)
death  = max(y + 1, born + λ × (life_min_permille + pick(D_at(hist.figures.life; y; f, 0, 1), life_span_permille + 1)) div 1000)
```

A ruler's born year is 6.4's and its planned death its reign's planned end e(n). With the shipped values an elven general commands at 210 to 420 and dies at 490 to 840, a goblin one at 12 to 24 and dies at 28 to 48: "long-lived figures persist".

**Deaths of age** (rulers die with their reign's end, 6.3; for every other living figure, ascending id: T3 when its planned death ≤ Y; Q4: ≤ Z, dated Y, only when `quiet_events_left` of its civilization is above 0, else it waits): append **`figure_died`** (region: the figure's place: its realm's capital's region when the realm is alive, the host's capital region for an exile, the tower's region for a caster, else the region of its start event; settlement: that capital for a ruler of an alive realm, else 0; participants f `figure`, its civilization `civilization`; values `how` (0 of age, 1 at a crisis, 2 in exile), `age` = Y − born; cause f's start event); status 3 (died), died Y, death event, death region and settlement as the event's. Every open relation with f as a side (`ruled`, `served`, `rival`; ascending key, on a copy, P3-04 6.9 rule 1) closes with it. A ruler's death in T3 is the reign's end (6.3); a ruler's death at a crisis has `how` 1 (6.5). `parent` rows never close.

**The charged civilization** of every figure event in a quiet step is its first civilization participant, the figure's realm (ended or not). A realm's quiet-step cost: at most one death per named figure of the realm whose death falls in the step (typically 0 to 2: its ruler, a general, a hero, a caster) and at most one book per caster; nothing for unnamed reigns. W9 holds by construction: every Q4 event checks the budget first and waits otherwise.

### 6.10 Epithets (Paul, 2026-11-24: "If the deeds are substantial it can be more than two per world")

**When.** Every figure's epithet is decided once, in the T6 of year E − 1 after 6.12, when every deed is known: the figures in **closing order** (the dead by their death event id ascending, then the living by figure id ascending). Rows are written at the end, so no sentence is rendered before. A figure's deed values are taken at that moment (for the living, at E).

**The rule.** For figure f, the entries of `epithets` with `role` = f's role are tried in list order; v = f's value of the entry's `deed` (table below), t = its `text` with `{slayer}` replaced by the `slayer_epithet` of the kind of the monster whose kind has the largest `might` among those f slew (the last slain at a tie), and b(t) = the number of figures earlier in closing order whose epithet is t (however earned):

1. **Substantial**: `substantial` > 0 and v ≥ `substantial`: f's epithet is t, whatever b(t).
2. **Ordinary**: v ≥ `min` and b(t) < `epithet_repeat_max` (2): f's epithet is t.
3. Otherwise the next entry.

None: the empty epithet. So an ordinary deed names at most two figures of a world by one epithet (a third ordinary conqueror stays plain, or takes a later entry), and a substantial one names every figure that did it. An ordinary earning counts the substantial bearers too: once two realms have a substantial `the Conqueror`, an ordinary conqueror does not dilute it. There is no cap on substantial epithets: they are kept meaningful by their thresholds, which are data and are calibrated to FG11 (no epithet on more than a tenth of a world's figures, 15.1). A hard bound is not set, as Paul's decision is "regardless of how many bear it"; if FG11 cannot be met by thresholds alone, the lead decides (22 lead item 2).

**Deeds** (code; the `deed` enum; every value an integer ≥ 0, measured):

| deed | value |
| --- | --- |
| `founded_years` | for a realm's first ruler (`reign_no` 1): the years its realm stood, from its founding year to the year of its `ending_event` or E; 0 for others |
| `wars_won` | `peace` and `war_won` with the realm as winner during the figure's reign (the reign's tallies, 6.2) |
| `conquests` | `civ_conquered` with the realm as conqueror, likewise |
| `betrayals` | `alliance_betrayed` with the realm as betrayer, likewise |
| `realm_fell` | 1 when the reign ended with the realm (6.4 rule 4), else 0 |
| `reign_permille` | (the reign's end year, or E for a living ruler) − `office_from`, × 1000 div `generation_years` of the race |
| `battles_won` | `battle_led` won as a general |
| `battles_unbeaten` | `battle_led` won as a general when none was lost, else 0 |
| `slew_might` | the largest `might` (P5-02 4.2) of the kinds of the monsters slain as a hero (`monster_slain_by_hero`); 0 none |
| `exiled` | 1 for a claimant named by an `exiled` event, else 0 |
| `books` | books written |
| `blights` | blights caused |

**Shipped `epithets`** (taste; starting values, calibrated by FG11), in order, as role, deed, `min`, `substantial`, text:

| # | role | deed | min | substantial | text | substantial when |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | ruler | `conquests` | 1 | 2 | `the Conqueror` | two realms conquered in one reign |
| 2 | ruler | `betrayals` | 1 | 1 | `the Faithless` | any betrayal (rare: P5-04's AL targets) |
| 3 | ruler | `realm_fell` | 1 | 0 | `the Last` | never (ordinary only: many realms fall) |
| 4 | ruler | `wars_won` | 2 | 4 | `the Victorious` | four wars won |
| 5 | ruler | `reign_permille` | 1300 | 1450 | `the Old` | a reign of 1.45 generations (reigns are 0.5 to 1.5, 6.3; about one in twenty) |
| 6 | ruler | `founded_years` | 1 | 3000 | `the Founder` | a realm that stood 3,000 years |
| 7 | general | `battles_unbeaten` | 3 | 6 | `the Unbeaten` | six battles won, none lost |
| 8 | general | `battles_won` | 5 | 10 | `the Hammer` | ten battles won |
| 9 | hero | `slew_might` | 1 | 1200 | `{slayer}` | a giant (1200), an undead host (1500) or a dragon (6000); ogres, trolls and wyverns are ordinary |
| 10 | claimant | `exiled` | 1 | 0 | `the Exile` | never |
| 11 | caster | `blights` | 1 | 1 | `the Accursed` | any blight (at most one a caster) |
| 12 | caster | `books` | 3 | 0 | `the Learned` | never (`books_max` is 3) |

The epithet is stored on the row (9.1) and is part of the figure's **display name**: `name`, then ` ` + roman(`regnal`) when `regnal` ≥ 2 (6.14), then ` ` + `epithet` when the epithet is not empty (`Maela Dragonslayer`, `Grishnak the Conqueror`, `Balin II the Victorious`), in every sentence, whatever the event's year (history remembers them by it).

### 6.11 Person names (amends P3-02 4.5)

Name kind **`person`** (P3-02's reserved kind; after `site`, P5-02 9.1), stream `hist.names.person`, entity id = the figure id; an attempt is P3-02 4.2's with D the naming data of the figure's culture, F = D.`person_forms`, c that culture's id. The name is P3-02 4.3's (64 attempts, the history stage's registry, the name filter of 11.3, the numbered fallback); person roots are not added to the core set K and not tested against it (as monster roots, P5-02 9.1: a person may share a root with a place, as places are named for people). The **root** of a figure is its name with the form's literal parts removed (P5-12 5.3's `realm_root`; with the shipped forms the name itself). Names are drawn at a figure's creation, in creation order (H2). Book names: kind **`book`** (6.8; titles from data, so no attempt, no filter; the registry only).

### 6.12 The present (T6 of year E − 1)

After the log of the last T6 (the final phase always ticks year E − 1, P3-04 SC3), for each alive civilization c ascending whose reign is unnamed, and `name_rulers`: name it (6.4) and append **`reigned_at_end`** (region: c's capital's region; participants f `figure`, c `civilization`; no cause); the record event is it. So every alive realm has a recorded ruler at E. Then 6.10's epithets, for every figure.

### 6.13 P3-04 6.9, visibility, quiet steps

1. No `found_civilization` or `found_settlement`. Sites of kind 4 only, through P5-04 7.16's seam. `open_relation` and `close_relation` for `ruled`, `served`, `rival` and `parent` only, never inside a loop over `relations()`: every pass that closes copies its keys first. Members hold ids. `set_population` and `note_reduction` only in a blight (6.8).
2. Not a tension source.
3. Fixed place: after alliances in `subsystems`, built in `stages/history.cpp`, which also sets `conflict.set_figures(&figures)`, `peoples.set_figures(&figures)`, `monsters.set_figures(&figures)`, `alliances.set_figures(&figures)` (7.1) and `figures.set_sites(&monsters)`, `figures.set_conflict(&conflict)`.
4. Visibility, below.
5. Keys: civilization ids, reign numbers, figure ids, event ids (wars, battles, crises, raids), site and book ids.
6. Relation rows: `ruled`, `served`, `rival`, `parent` (9.2).
7. No region owner changes.

**What it sees in shared steps.** T3: alone; the T1 and T2 changes of every subsystem of the year (declarations, betrayals) through the log, live. T4: peoples' crises and secessions and monsters' raids and hunts of this T4, live. T6: conflict's battles, sieges, falls, conquests and war ends, and alliances' spoils, live. Q4: conflict's quiet ends of wars and alliances' quiet spoils, live. Civilizations founded in the step are invisible to its rises (heroes, casters) but their foundings are read from the log (a secession's founder is named in the T4 of its secession). Its own changes (a reign's temper, a general, a hero) reach the others at their next call: a temper drawn in T3 is read by conflict's strain from the next T2; a general named in T3 leads from the next T6 (a war has no battle in its declaration year, P3-06 7.3); a hero risen in T4 hunts from the next T4.

**Quiet steps.** 6.9's charge: figure deaths and books, each charged to the figure's realm and checked first. No world-level event (none is charged to civilization 0).

### 6.14 Regnal numbers (Paul, 2026-11-24: "humans and dwarves might have numbers some of the time the rest probably not")

**A line's names.** The line of civilization c is its recorded rulers (role 1, `civ` c) in creation order. Each has a **base name** (column `name`) and a **regnal number** (column `regnal`: 0 none, else 2 to `regnal_max`). The line's **names** N(c) are the distinct base names of its recorded rulers, each with k(x) = the number of recorded rulers of c with base name x, in the order of their first bearers' ids; a P3-02 4.3 fallback name (one ending in ` ` and a Roman numeral) is never in N(c).

**The reuse.** At every naming of a ruler of c with reign number n (6.4 rules 1, 3, 4, 6.5 items 2 and 3 for the winner f_w, 6.12), before 6.11's draw: u = c's culture; E_c = the names x of N(c) with k(x) < `regnal_max` and x + ` ` + roman(k(x) + 1) not in the registry, in N(c)'s order. If `regnal_permille`(u) > 0, E_c is not empty and `chance_pm(D(hist.figures.regnal; c, n, 0), regnal_permille(u))`: x = E_c[pick(D(hist.figures.regnal; c, n, 1), |E_c|)]; the figure's `name` = x and `regnal` = k(x) + 1. No 6.11 draw is made for it; x + ` ` + roman(`regnal`) is added to the registry (so no later fallback name of P3-02 4.3 can equal it; x itself is there already); the filter is not applied again (x passed it when first drawn). Otherwise 6.11's draw and `regnal` 0. Neither draw is made when `regnal_permille`(u) = 0 or E_c is empty (a founder's E_c is always empty).

**Numbers never change.** A ruler is recorded only while its reign is the current one (6.4 rules 1 to 4, 6.5, 6.12 each name the current or the new reign; an ended unnamed reign is never named later), so a line's recorded rulers are created in ascending `reign_no`, and a later naming can never fall between two earlier bearers of a name. So `regnal` = 1 + the earlier recorded bearers is a bearer's rank among the recorded bearers of its name in its realm's line for ever: "Balin II" is the second recorded Balin of the Hold of Durn, and no Balin recorded later can come before him. Unrecorded reigns have no name (none is drawn or stored), and a reused name always comes from a recorded ruler. A later stage that records a past reign (none in P5) gives it a fresh name and `regnal` 0 (invariant 12).

**The first bearer stays unnumbered.** Its row keeps `regnal` 0 and every sentence names it "Balin", also after a "Balin II" exists: a row's name is fixed at its creation (the record as it was written, as every name in this history), never "Balin I" in hindsight.

**Seceded realms begin their own line.** A realm founded by secession (6.5 item 2) has reign 1 and an empty N(c'); it never takes its parent's names. `reign_no` and `ruled` are per civilization and the claimant founded the realm by losing the parent's throne, so the new realm's rulers are a new line; a count across two realms could not be checked from one realm's rows, and "Balin III" of the new realm would be ambiguous. The parent's own line goes on (its winner f_w may reuse a name).

**Uniqueness.** Names stay unique in a world (the registry, P3-02 4.3) with one exception: recorded rulers of one civilization may share a base name, the first with `regnal` 0 and each later one with its rank. Display names stay unique: the numeral sets the bearers apart, and the numbered form is held in the registry against P3-02's numbered fallback (rare for persons) in both directions.

**Rendering.** roman(k) is `worldgen::roman_numeral` (P3-02 4.3; subtractive form): II to XXXIX at most (`regnal_max` ≤ 39, so the longest numeral, XXXVIII, is 7 bytes). The display name is `name`, ` ` + roman(`regnal`) when `regnal` ≥ 2, then ` ` + the epithet when not empty (6.10). Sentences (illustrative): `The Kingdom of Cear under Aldric II declared war on the Hold of Durn over the Grey Fells.` · `Aldric III the Victorious of the Kingdom of Cear died at the age of 64.` · `Balin II took the throne of the Hold of Durn against the claim of Nori, who was driven out.` · `At the end of the age the Hold of Durn was ruled by Balin III.` Report lines (10.2, 10.3): `  f88 Balin II the Victorious: ruler of the Hold of Durn (c4), dwarven, 5321-5398, reigned 5340-5398 (reign 33, warlike); 2 won; died 5398 e9120` · `    Balin II the Victorious (f88): 5340-5398, reign 33, warlike`.

### 6.15 Figures and alliances (r3; P5-04 as built)

Where figures meet P5-04's alliances, allied wars and spoils, the rule is one of these; where none is stated, there is none in this stage (P5-04a, the alliances follow-up, owns alliance wording, refusal and betrayal motives and renewal lines).

| Point | Rule |
| --- | --- |
| A ruler named by an alliance event | Only `alliance_betrayed` (the betrayer), `spoils_shared` and `spoils_withheld` (the principal) name a ruler (6.4 rule 3) and render `{r:under}` (8.2). No other kind of 63 to 78 (`alliance_formed`, `alliance_renewed`, `alliance_lapsed`, `alliance_broken`, `ally_joined_war`, `ally_refused`, `ally_withdrew`, `allied_battle`, `trade_opened`, `trade_broken`, `alliance_spurned`, `mine_opened`, `arms_sold`) names or records a ruler. A `spoils_*` read from a quiet step names the ruler there (a naming by deed appends no event, so no budget is spent). |
| A general in an allied war | A general is named only for the two principals of a `war_declared` (6.6); a joined ally names none and its levies follow the partner's general. A battle with a general on either side is `battle_led` whatever its allies (7.6): its `winner_ally` and `loser_ally` are P5-04 7.7's, and the skill factor multiplies the whole side's force A or D (principal and allies) for the win chance only. A betrayal's war is a `war_declared` like any other: both sides may name generals. |
| Tempers | 7.2 (strain, normalized), 7.3 (strain toward an ally and the grant of a share), both read the principal's or the attacker's current reign; no temper acts on a formation, a call to arms, a refusal or a withdrawal in this stage. |
| A disputed succession and standing alliances | None. The parent realm c keeps its alliances, trades and joined wars through a crisis and a split (P5-04 has no secession rule); the new realm c' begins with none and may form them by P5-04 6.3 from its first T2. A ruler's death or a dispute does not end, renew or test an alliance. |
| Exiles and allies | None. The host is chosen by disposition and reach only (6.5): an ally of c may host its claimant, and a realm at war with c (`at_war`, joined rows included) may not. Hosting changes no alliance, strain, claim or disposition. |
| Generals and spoils | None: a share is the principal's to grant (P5-04 6.11); no general or hero is named in a `spoils_*` event. |

# Common

## 7. Seams

### 7.1 `FigureView`

Declared in `figures.hpp`; `ConflictSubsystem`, `PeoplesSubsystem`, `MonstersSubsystem` and `AlliancesSubsystem` gain `void set_figures(const FigureView*)`. Const reads, valid from construction (with nothing named: every factor neutral):

```cpp
class FigureView {
public:
    virtual std::int64_t strain_permille(std::int64_t civ) const = 0;          // 7.2; 1000 with no reign
    virtual std::int64_t allied_strain_permille(std::int64_t civ) const = 0;   // 7.3
    virtual std::int64_t spoils_permille(std::int64_t civ) const = 0;          // 7.3; may be negative
    virtual std::int64_t split_bonus_permille(std::int64_t civ) const = 0;     // 7.4
    struct General { std::int64_t figure = 0, skill = 0; };
    virtual General general_of(std::int64_t war, std::int64_t civ) const = 0;  // 7.6; figure 0: none
    struct Hero { std::int64_t figure = 0, hunt_permille = 0, might_permille = 0; };
    virtual Hero hero_of(std::int64_t civ) const = 0;                          // 7.7
    virtual bool figure_exists(std::int64_t figure) const = 0;                 // the stage's write check (8.5)
};
```

With no view set, every reader is as at 0.26.0.

### 7.2 Tempers in strain (amends P3-06 5.2, after P5-04 7.2)

```
mean_r         = Σ_t temper_weights(r)[t] × temper_strain_permille[t] div Σ_t temper_weights(r)[t]     // once per race
strain_pm(c)   = temper_strain_permille[τ(c)] × 1000 div max(1, mean_r)
rate(a, b)     = rate(a, b) × strain_pm(a) div 1000            // after P5-04 7.2's allied factor, before the step's multiplication
```

τ(c) is the temper of c's current reign (named or not). The normalization keeps each race's average rate as before: a warlike goblin chief quarrels more than an average goblin chief, and goblins as a whole no more than at 0.26.0 (shipped: goblin mean_r = 1075, warlike 1209, peaceable 651; elf mean_r = 955; orc 1135). Read live at conflict's T2 and Q4 (the reign at the step's start for this subsystem).

### 7.3 Tempers in alliances (amends P5-04 7.2 and 6.11)

- P5-04 7.2's allied strain: rate(a, b) × `allied_strain_permille` div 1000 becomes rate(a, b) × `allied_strain_permille` div 1000 × `temper_allied_permille`[τ(a)] div 1000 (grasping rulers turn on allies twice as readily, just ones half; not normalized: betrayal is a grasping ruler's act).
- P5-04 6.11's grant: g = clamp(… + `temper_spoils_permille`[τ(W)], `spoils_grant_min`, `spoils_grant_max`) (the term added inside the clamp).

### 7.4 A short reign before a crisis (amends P3-05 10.3)

At the split test: chance_pm(draw(hist.peoples.split, Y, c, 0, 0), `split_permille` + `split_bonus_permille`(c)), with the bonus `short_reign_split_permille` when c's current reign (the one the crisis ends) started less than `short_reign_permille` × `generation_years` div 1000 years before Y, else 0 (min 1000 on the sum). The figures subsystem's state at peoples' T4 is that of the reign before the crisis (figures runs after peoples).

### 7.5 Only civilization pairs in conflict's relation passes (amends P3-06 7.6 and 8.1 item 1, P5-04 7.12)

Conflict's housekeeping (8.1 item 1) and a conquest (7.6) close the open relations **whose a_type and b_type are both 3** with an ended side; relations with a figure side are this subsystem's (6.4 rule 4, 6.9). Every reader that walks `State::relations()` or the `relation` table filters by kind (`enmity`, `war`, `peace`, `alliance`, `trade`, or this stage's four), so no figure row enters a civilization rule. No output change at 0.26.0 (no such row existed).

### 7.6 Generals in battle; `battle_led` (amends P3-06 7.3 items 3 and 5, P5-04 7.7)

In a battle of war w at R between x's side and y's side (P5-04 7.7's sides, principals x and y): with G_x = `general_of(w, x)`, G_y = `general_of(w, y)` (figure 0: none, skill 0):

```
A' = A × (1000 + skill(G_x)) div 1000          // for the win chance only
p  = A' × 1000 div (A' + D' × (1000 + skill(G_y)) div 1000)      // 500 when the sum is 0
```

D' = D × def(R) div 1000 as before. Casualties, sieges (A2) and the score are computed on A and D unmultiplied, as before: a general wins battles, not men.

**The event.** When G_x or G_y names a figure, the battle is **`battle_led`** (region R; participants W `winner`, L `loser`, the allies of each side as P5-04 7.7 (`winner_ally`, `loser_ally`), W's general `winner_general` and L's `loser_general` (each 0–1); values `winner_dead`, `loser_dead`, `pass` as P5-04 7.7; cause w); otherwise `allied_battle`, `battle` or `battle_at_pass` as before. One kind covers every shape of a led battle (with or without allies, at a pass or not), so no second kind is needed for allies.

**Why a new kind** (reading, 23 L3). A role on `battle`, `battle_at_pass` or `allied_battle` would edit a shipped kind, which P3-04 8.2 forbids; a companion event per battle (`general_led`) would add a line per led battle and leave the battle sentence without the name. Every reader of battle kinds gains the one key (11.1).

### 7.7 Heroes in hunts (amends P5-02 6.4.2 as amended by P5-03 9.3)

For civilization c in the hunt loop of monster m, with H = `hero_of(c)` (figure 0: none):

```
p_hunt = min(hunt_max_permille + H.hunt_permille, hunt_permille + (R_c − 1) × hunt_raid_permille + H.hunt_permille)
S_c    = H_c × prowess_permille(r_c) div 1000 × (1000 + H.might_permille) div 1000
```

(H.hunt_permille = `hero_hunt_permille`, H.might_permille = `hero_might_permille` when c has a hero, else 0.) Odds and outcomes as before. With a hero:

- **Slain**: the event is **`monster_slain_by_hero`** (lair region; participants m `monster`, c `slayer`, H.figure `hero`; value `hunters`; cause m's last raid on c) in place of `monster_slain`; everything else as P5-02 (status 3, `slain_by` c, the lair ends with ending 1).
- **Driven off**: unchanged (`monster_driven_off`; the hero lives).
- **Nothing** (a failed hunt, which writes nothing at 0.26.0): if `chance_pm(D(hist.monsters.hero_fall; m, c, 0), hero_fall_permille)`: append **`hero_slain`** (lair region; participants H.figure `figure`, c `civilization`, m `monster`; cause m's last raid on c). Either ends the hunts of the year (as a slaying does).

The figures subsystem reads both events in its T4 (6.7). The hero's death is its record (status 5), written by monsters and applied by figures from the log: no subsystem changes another's members.

### 7.8 The site seam for towers (part C; amends P5-04 7.16, P5-02 11)

`open_site` accepts kind 4 from this subsystem (kind 3 from alliances). (r3: as built) `MonstersSubsystem::open_site` is fatal today unless `spec.kind == 3` (`history: monsters: open_site: site <id> of kind <k> is not the next site of kind 3`); it becomes fatal unless `spec.id == next_site_id()` and `spec.kind` is 3 or 4, message `history: monsters: open_site: site <id> of kind <k> is not the next site of kind 3 or 4`. `stages/history.cpp` sets `figures.set_sites(&monsters)` beside P5-04's `alliances.set_sites(&monsters)`. Every P5-02 and P5-04 loop over sites (lairs, outposts' ends, `holds_site`, the lair's "occupied" test of P5-02 6.3.2, mines, the report's counts) tests the kind it means: as built the lair and `holds_site` tests read `kind != 3`; they become `kind == 1 || kind == 2` (a lair or an outpost), so that a library at a sacked city's ruin neither blocks a lair there nor counts as one. A tower never ends in this stage (`ending` 0). Monsters and alliances run before figures in every step (6.1), so site ids are allocated in one order.

### 7.9 Rendering (amends P3-04 8.3, 8.4, 8.5)

- **Display name** of entity type 6: the figure's display name (6.10). (P3-04 8.3 had "none for 6–9"; P5-02 gave 8 and 9 names; 7 stays without.)
- **Form `under`** (8.4's `form` gains `"under"`): for a role of entity type 6, ` under ` + `{r:all}` when r has participants, else nothing; for a role of entity type 3, ` under ` + the display name of the **recorded ruler at the event** of its first participant c, else nothing. The recorded ruler of c at event e is the figure a of the `ruled` row (a, c) with `since_event` ≤ e and (`until_event` = 0 or e ≤ `until_event`); at most one row qualifies (13). `template_problem`: `form 'under' needs a figure or civilization role at byte <n>`.
- **Placeholder `{@book}`** (part C; 8.4's `body` gains `"@book"`): the title of the `book` row whose `event` is the event's id. `template_problem`: `{@book} needs the kind book_written at byte <n>`. `HistoryView::load` checks that every `book_written` has exactly one row.

Not a schema change by themselves (as P5-03a's and P5-04's forms); `HistoryView` loads `figure` and the `ruled` rows (and `book`).

### 7.10 `draw_at` (amends P3-04 6.4)

`StepContext` gains `std::uint64_t draw_at(std::uint64_t stream_key, std::int64_t year, std::uint64_t a, std::uint64_t b, std::uint64_t k) const` = P3-02 4.1's `draw(key, year, a, b, k)`, fatal `history: draw_at: year <y> outside the step <Y>-<Z>` unless Y ≤ year ≤ Z. The year is a stored or computed year of the decision (a reign's start, a figure's entry), never a loop position (P3-04 6.9 rule 5 holds with it).

### 7.11 Proof obligation I0

With the `zero` tuning (4.4), the history tables of a world are byte-identical to those of the same world without `FiguresSubsystem` and with no `FigureView` set: no ruler, claimant, general, hero or caster is created, so no figure, `ruled`, `served`, `rival` or `parent` row, no event of kinds 79 to 92, no tower and no book exists; every temper factor is neutral and the split bonus 0; `battle_led`, `monster_slain_by_hero` and `hero_slain` are never written; and `{r:under}` renders nothing, so every sentence is 0.26.0's. What differs is the catalogue rows (kinds 79 to 92, the six changed templates) and the schema version (12). The reign draws use their own stream and consume no other (H4).

## 8. Events

### 8.1 Catalogue (appended after 78; r3: ids final, 0.26.0's catalogue ending at P5-04's 78)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values | part |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 79 | `came_to_rule` | 1 | 1 | 0 | 1 `figure` fig 1–1; 2 `civilization` civ 1–1 | — | A |
| 80 | `figure_died` | 1 | 1 | 2 | 1 `figure` fig; 2 `civilization` civ | `how`, `age` | A |
| 81 | `succession_disputed` | 1 | 1 | 2 | 1 `civilization` civ; 2 `ruler` fig; 3 `claimant` fig; 4 `new_realm` civ 0–1 | `outcome` | A |
| 82 | `exiled` | 1 | 1 | 0 | 1 `figure` fig; 2 `civilization` civ; 3 `host` civ 0–1 | `hosted` | A |
| 83 | `reigned_at_end` | 0 | 1 | 0 | 1 `figure` fig; 2 `civilization` civ | — | A |
| 84 | `general_appointed` | 1 | 1 | 0 | 1 `figure` fig; 2 `civilization` civ; 3 `enemy` civ | `again` | B |
| 85 | `battle_led` | 1 | 1 | 0 | 1 `winner` civ; 2 `loser` civ; 3 `winner_ally` civ 0–16; 4 `loser_ally` civ 0–16; 5 `winner_general` fig 0–1; 6 `loser_general` fig 0–1 | `winner_dead`, `loser_dead`, `pass` | B |
| 86 | `fell_in_battle` | 1 | 1 | 0 | 1 `figure` fig; 2 `civilization` civ; 3 `enemy` civ | — | B |
| 87 | `hero_rose` | 1 | 1 | 1 | 1 `figure` fig; 2 `civilization` civ; 3 `monster` monster | — | B |
| 88 | `monster_slain_by_hero` | 1 | 1 | 0 | 1 `monster` monster; 2 `slayer` civ; 3 `hero` fig | `hunters` | B |
| 89 | `hero_slain` | 1 | 1 | 0 | 1 `figure` fig; 2 `civilization` civ; 3 `monster` monster | — | B |
| 90 | `tower_founded` | 1 | 1 | 2 | 1 `figure` fig; 2 `civilization` civ; 3 `site` site | `library` | C |
| 91 | `book_written` | 1 | 1 | 0 | 1 `figure` fig; 2 `civilization` civ; 3 `site` site | — | C |
| 92 | `blight` | 1 | 1 | 0 | 1 `figure` fig; 2 `civilization` civ; 3 `site` site | `dead` | C |

(fig = entity type 6, civ = 3, monster = 8, site = 9; unlisted counts 1–1.) Every `battle_led` has at least one general participant. Codes: `how` 0 of age, 1 at a crisis, 2 in exile; `outcome` 0 the claimant was exiled, 1 broke away (`new_realm` present exactly when 1); `hosted` 0 or 1 (`host` present exactly when 1); `again`, `library` 0 or 1; `age` ≥ 0; `dead`, `hunters` ≥ 0. The charged civilization of a quiet-step event is its first civilization participant (only 80 and 91 occur in quiet steps). All fourteen are appended (scope C), as ids 79 to 92.

### 8.2 Templates and salience (data, `history.report`)

| key | sal. | template | summary? |
| --- | --- | --- | --- |
| `came_to_rule` | 15 | `{figure} became the first ruler of {civilization}.` | no |
| `figure_died` | 15 | `{figure} of {civilization} died at the age of {age}{how?.|, and the succession was disputed.|, in exile.}` | no |
| `succession_disputed` | 30 | `{ruler} took the throne of {civilization} against the claim of {claimant}{outcome?, who was driven out|, who broke away with the far lands}.` | companion (10.3) |
| `exiled` | 25 | `{figure}, who had claimed the throne of {civilization}, {hosted?fled into the wilds|went into exile with }{host:all}.` | companion (10.3) |
| `reigned_at_end` | 10 | `At the end of the age {civilization} was ruled by {figure}.` | no |
| `general_appointed` | 15 | `{figure} {again?took|again took} command of the armies of {civilization} against {enemy}.` | no |
| `battle_led` | 35 | `{winner}{winner_general:under}{winner_ally:with} defeated {loser}{loser_general:under}{loser_ally:with} {pass?in|at the pass into|at the pass into} {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}.` | as a battle (11.1) |
| `fell_in_battle` | 30 | `{figure} of {civilization} fell in battle against {enemy} in {@region}.` | at low thresholds |
| `hero_rose` | 25 | `After the raids of {monster}, {figure} of {civilization} took up the hunt.` | at low thresholds |
| `monster_slain_by_hero` | 55 | `{hero} of {slayer} hunted down and slew {monster}{monster:aside}{monster:where}.` | yes (as `monster_slain`) |
| `hero_slain` | 45 | `{figure} of {civilization} was slain hunting {monster}{monster:where}.` | yes |
| `tower_founded` | 40 | `{figure} of {civilization} {library?raised|founded} {site} in {@region}.` | yes |
| `book_written` | 30 | `{figure} wrote {@book} at {site}.` | at low thresholds |
| `blight` | 50 | `The workings of {figure} at {site} went awry, and a blight fell on {@region}; {dead?no one|one|#} died.` | yes |

**Changed templates** (data; the kinds are unchanged): exactly these six, each gaining `{r:under}` after the role that acts:

| kind | template from 0.27.0 |
| --- | --- |
| `war_declared` | `{attacker}{attacker:under} declared war on {defender} over {@region}.` |
| `alliance_betrayed` | `{betrayer}{betrayer:under} broke faith and made war on {betrayed}, an ally.` |
| `spoils_shared` | `{principal}{principal:under} gave {ally} its share of the spoils of {@region}.` |
| `spoils_withheld` | `{principal}{principal:under} kept the spoils of {@region} from {ally}, its ally.` |
| `civ_conquered` | `{civilization}{civilization:under} fell to {conqueror}.` |
| `civ_ended` | `{civilization}{civilization:under} came to an end.` |

Each names the ruler that 6.4 rule 3 or 4 names at that very event, so the clause is never empty when `name_rulers` is true. Battles name generals through `battle_led`, hunts the hero through `monster_slain_by_hero`. No other sentence changes: an alliance's forming, a peace and a monster slain by an army stay as they are (a ruler is named for acts of will and of fate, not for every treaty). The cause words `battle=…` of the templates of kinds 10 `settlement_dwindled`, 16 `settlement_abandoned`, 31 `settlement_abandoned_to`, 43 `region_renamed` and 45 `undead_rose` gain `battle_led=` with the same word as `battle=` (data; r3: r2 and P5-04 11.1 wrote 13, which is `plague_spread`, a kind with no cause words). A blight is a reduction (`note_reduction`, 6.8), so the cause maps of kinds 10, 16 and 31 gain `blight=blight`, placed after `allied_battle=war` (r3: r2 also named `figure_died` and `fell_in_battle`, which are no reductions and never a settlement's cause; dropped).

Samples (illustrative): `Dorin the Founder became the first ruler of the Hold of Durn.` · `The Gub Horde under Grishnak the Conqueror declared war on the Hold of Durn over the Grey Fells.` · `Thrain the Unbeaten took command of the armies of the Hold of Durn against the Gub Horde.` · `The Hold of Durn under Thrain the Unbeaten, with the Kingdom of Cear, defeated the Gub Horde in the Grey Fells; 812 of the defeated and 140 of the victors fell.` · `Thrain the Unbeaten of the Hold of Durn fell in battle against the Gub Horde in the Grey Fells.` · `Aldric took the throne of the Kingdom of Cear against the claim of Edric the Exile, who was driven out.` · `Edric the Exile, who had claimed the throne of the Kingdom of Cear, went into exile with the Hold of Durn.` · `After the raids of the Brood of Skarr, Maela of the Kingdom of Cear took up the hunt.` · `Maela Dragonslayer of the Kingdom of Cear hunted down and slew Saelith the Black, a dragon of the old world, in the Cold Peaks.` · `Iensath the Learned of the House of Finen raised the Star-Tower of Iensath in the Wood of Stags.` · `Iensath the Learned wrote the Lay of the First Stars at the Star-Tower of Iensath.` · `The workings of Gnash the Accursed at the Smoke-Tower of Gnash went awry, and a blight fell on Crookhorn Steppes; 412 died.` · `At the end of the age the Kingdom of Cear was ruled by Bolli.`

### 8.3 Causes (W6)

A first ruler, its realm's founding; a death, the figure's start event; a disputed succession, the crisis (and the secession); an exile, its dispute; a general's command, the war's declaration; a fall, the battle; a hero's rise, the raid; a hero's slaying and death, the monster's last raid on the realm; a tower, the later of the capital's founding and the realm's founding; a book, the tower (and the caster's last book); a blight, the tower. `reigned_at_end` is uncaused (a record of the present, as P5-01's `refuge_hidden`).

### 8.4 Why these saliences

Rulers' accessions, deaths, commands and the present are the line's bookkeeping (10 to 15, below `SUMMARY_MIN_SALIENCE`); a disputed succession and an exile are read with the secession or crisis they explain (companions); a battle led stays a battle (35, never a candidate by itself); a hero's slaying ranks as an army's (55) and replaces it one for one; a hero slain (45), a tower (40) and a blight (50) are rare news. Estimated summary candidates per default world: 1 to 2 heroes slain, 2 to 6 towers, at most 1 blight, 2 to 4 slayings by heroes in place of slayings by armies.

### 8.5 The writer (amends P3-04 6.6 W4, P5-02 10.4)

For participants of entity type 6 W4 checks only that the id is at least 1; the history stage checks at write time that `FigureView::figure_exists` holds (fatal `history: event <id> (<kind>): figure <f> has no row`), and the reader checks it again (9.4).

## 9. Tables, relation kinds, schema 12 and reader checks

### 9.1 Table `figure`

```
CREATE TABLE figure (id INTEGER PRIMARY KEY, name TEXT NOT NULL, epithet TEXT NOT NULL, role INTEGER NOT NULL, race INTEGER NOT NULL, culture INTEGER NOT NULL, civ INTEGER NOT NULL, born INTEGER NOT NULL, office_from INTEGER NOT NULL, start_event INTEGER NOT NULL, died INTEGER, death_event INTEGER NOT NULL, death_region INTEGER NOT NULL, death_settlement INTEGER NOT NULL, status INTEGER NOT NULL, reign_no INTEGER NOT NULL, regnal INTEGER NOT NULL, temper INTEGER NOT NULL, skill INTEGER NOT NULL) STRICT
```

| Column | Meaning |
| --- | --- |
| `name`, `epithet` | the registered name (6.11), or for a ruler with `regnal` ≥ 2 the base name it shares with earlier rulers of its line (6.14); the epithet (6.10, may be empty); design 6's `name` is `name` |
| `role` | 1 ruler, 2 claimant, 3 general, 4 hero, 5 caster (design 6's `role`) |
| `race`, `culture`, `civ` | the figure's people and realm (design 6's `civ_id`) |
| `born` | design 6's `born`: a year (no event; 6.9, 6.4) |
| `office_from` | the year the role began: a ruler's accession (may precede `start_event`, 6.4 rule 3), a claimant's dispute, a general's first command, a hero's rise, a caster's tower |
| `start_event` | the event by which the figure entered the record |
| `died`, `death_event` | design 6's `died`: NULL and 0 while alive |
| `death_region`, `death_settlement` | where the figure died (P6's tombs); 0 while alive; settlement 0 when not at a settlement |
| `status` | at E: 1 alive, 2 alive in exile, 3 died, 4 fell in battle, 5 slain by a monster, 6 fell with its realm |
| `reign_no` | a ruler's place in its realm's line (1 the founder); 0 for other roles |
| `regnal` | a ruler's regnal number (6.14): 0 none (every first bearer of a name, every non-ruler), else 2 to `regnal_max`, the bearer's rank among the recorded rulers of its civilization with the same `name` |
| `temper` | a ruler's temper: 0 steady, 1 warlike, 2 peaceable, 3 grasping, 4 just; 0 for other roles |
| `skill` | a general's skill (per mille); 0 for other roles |

Rows ascending id (creation order). Written by the history stage at the end, empty without history.

### 9.2 Relation kinds (text keys; not a schema change by themselves)

| Kind | Sides | Symmetric | Opened by | Closed by |
| --- | --- | --- | --- | --- |
| `ruled` | a figure (6), b civilization (3) | no | `came_to_rule`, `succession_disputed`, `reigned_at_end`, or the deed event that named the ruler (6.4 rule 3, 4) | the ruler's `figure_died`, or the realm's `civ_conquered` / `civ_ended` |
| `served` | a figure (6), b civilization (3) | no | `general_appointed` | the war's ending event (`war_end_event`), `fell_in_battle`, or the general's `figure_died` |
| `rival` | figure, figure (smaller id as a) | yes | `succession_disputed` | the first death of either (its death event) |
| `parent` | a the parent figure, b the child | no | `succession_disputed` (6.5) | never (open at E) |

Ties that are deeds are events, not relations: a hero's slaying is `monster_slain_by_hero` (its `hero` and `monster` participants), so no `slew` relation is kept (reading, 23 L5). An heir is the `ruled` row of the successor and, where the parent was named, its `parent` row.

### 9.3 Part C: site kind 4 `tower` and table `book`

`site.kind` gains code **4 `tower`** (a code list change, P3-04 8.2; P5-02 11.2's list: 1 lair, 2 outpost, 3 mine, 4 tower). A tower row: `settlement` the library's settlement or 0 for a tower in the wild, `founder_civ` the caster's realm, `monster` 0, `origin_event` its `tower_founded`, `ending` 0 (towers do not end in this stage).

```
CREATE TABLE book (id INTEGER PRIMARY KEY, title TEXT NOT NULL, author INTEGER NOT NULL, site INTEGER NOT NULL, year INTEGER NOT NULL, event INTEGER NOT NULL) STRICT
```

One row per `book_written`, ascending id: `title` (registered), `author` (a caster figure), `site` (its tower), `year` and `event` (the `book_written`). P5-09 will give a book its spell and P6 its present places (20).

### 9.4 Schema 12 (hard rule 7)

- `figure`'s statement and (part C) `book`'s are appended after the last statement of schema 11, in this order. No existing statement changes.
- `SCHEMA_VERSION`, `PRAGMA user_version` and `meta.schema_version` **12**; readers refuse version 11 worlds (`world schema version 11, this build reads 12`).
- The empty world gains two root pages (to be measured with the vendored SQLite, P3-04 9.6's method: expected 47 pages, 192512 bytes, if `sqlite_schema` still fits).
- Entity type code 6 `figure` comes into use (no new code). `db::HISTORY_TABLES` unchanged; the new tables are read by name.
- (r3, 25.1) `settlement.status` keeps its codes 1 to 4: code 4 `faded` stays valid for every reader and for hand-made fixtures, but the 0.27.0 generator writes no row with status 4 and no event of kind 19 `ruin_faded`. No DDL or code list changes for it; `schema.md` notes it at the `settlement` columns.
- `report` (P3-04 11.1): `[tables]` gains `book`, `figure` in name order; `[history]` gains `figures` and `books` (rows) at the end of its row keys. `hash --detail`: one `table <name>` line each, in name order.
- `docs/specs/schema.md` (the engineer): the version line ("Current schema version: 12 (from generator 0.27.0, the P5-06 merge: tables `figure` and `book`, site kind 4 `tower`)"); the DDL; the columns and codes of 9.1 and 9.3; the relation kinds of 9.2; entity type 6 in use; the changelog row ("11 → 12 with generator 0.27.0: tables `figure` and `book` added after schema 11's last statement; `site.kind` gains code 4 `tower`; readers refuse version 11; the empty world grows by two pages; the event catalogue gains kinds 79 to 92; relation kinds `ruled`, `served`, `rival`, `parent`; templates may use the form `under` and `{@book}`. Proposals 'Schema 12: figures and books (P5-06)'"); section 6's catalogue rows.
- Proposals entry "Schema 12: figures and books (P5-06)" (24).

**Reader checks** (r3: every check with its message). **Where**: one function, `figure_tables_problem` (beside P5-02's `monster_tables_problem` and P5-04's `alliance_tables_problem` in `src/history/event_catalogue.cpp`), called by both `read_history_tables` and `HistoryView::load` so that both give the same first error: in `HistoryView::load` after `alliance_tables_problem`; in `read_history_tables` after P5-03's checks (P5-04's relation checks run in `HistoryView::load` only, as built; that asymmetry is P5-04a's, not this stage's). The amendments of P5-02's `site` and `monster` checks are made in place, in `monster_tables_problem`. Inside `figure_tables_problem` the order is: `figure` rows, `book` rows, `relation` rows of the four figure kinds, then events. First failing row in key order; linear in rows (P5-02 11.4). **Message forms** (the reason follows the prefix):

| Prefix | Key |
| --- | --- |
| `corrupt table 'figure': row <id>: ` | `figure.id` |
| `corrupt table 'book': row <id>: ` | `book.id` |
| `corrupt table 'relation': row (<a_type>, <a_id>, <b_type>, <b_id>, <kind>, <since>): ` | as P5-04 9.1 |
| `corrupt table 'event': row <id>: ` | `event.id` |
| `corrupt table 'site': row <id>: `, `corrupt table 'monster': row <id>: ` | as P5-02 11.4 |

`<role word>` is `ruler`, `claimant`, `general`, `hero`, `caster` (role 1 to 5). A version 11 world: `world schema version 11, this build reads 12` (P0-02's reader, `db/reader.cpp`).

**`figure`** (each row in ascending id, the checks in this order):

| # | Condition | Reason |
| --- | --- | --- |
| F1 | id = previous id + 1 (the first 1) | `id <id> is not <n>` |
| F2 | `name` passes `is_entity_name` | `name is not a name` |
| F3 | `epithet` empty or of grammar `epithet` (4.3) | `epithet is not an epithet` |
| F4 | 1 ≤ `role` ≤ 5 | `role <r> outside 1 to 5` |
| F5 | `race` is a `race` row | `unknown race <r>` |
| F6 | `culture` is a `culture` row | `unknown culture <c>` |
| F7 | `civ` is a `civilization` row | `unknown civilization <c>` |
| F8 | `start_event` is an event id | `start_event <e> is not an event` |
| F9 | `born` ≤ `office_from` | `born <b> after office_from <o>` |
| F10 | `born` ≥ the `awakening_year` of its race's `race` row | `born <b> before the race's awakening` |
| F11 | `office_from` ≤ the year of `start_event` | `office_from <o> after the year of start_event` |
| F12 | 1 ≤ `status` ≤ 6 | `status <s> outside 1 to 6` |
| F13 | (`died` NULL) = (`death_event` = 0) = (status 1 or 2); while alive `death_region` = 0 and `death_settlement` = 0 | `end columns disagree` |
| F14 | dead: `died` ≥ `office_from` | `died <d> before office_from` |
| F15 | dead: the death event by status: 3 a `figure_died` of year `died` with `figure` = id; 4 a `fell_in_battle` of year `died` with `figure` = id; 5 a `hero_slain` of year `died` with `figure` = id; 6 the `ending_event` of `civ` | `death_event <e> is not this figure's death` |
| F16 | dead: `death_region` is a `region` row | `death_region <r> is not a region` |
| F17 | `death_settlement` 0 or a `settlement` row | `death_settlement <s> is not a settlement` |
| F18 | `reign_no` ≥ 1 exactly for role 1 | `reign_no <n> of a <role word>` |
| F19 | role 1: `reign_no` above the `reign_no` of the last earlier row of role 1 and the same `civ` | `reign_no <n> not above <m> of figure <f>` |
| F20 | `regnal` 0 or 2 to 39 | `regnal <r> outside 0 or 2 to 39` |
| F21 | `regnal` 0 unless role 1 | `regnal <r> of a <role word>` |
| F22 | with k = the earlier rows (smaller id) with the same `name`: every one is of role 1 and the same `civ`, else the first other one is named | `name repeats figure <f>` |
| F23 | `regnal` r ≥ 2: k = r − 1; `regnal` 0: k = 0 | `regnal <r> but <k> earlier bearers` |
| F24 | 0 ≤ `temper` ≤ 4 | `temper <t> outside 0 to 4` |
| F25 | `temper` 0 unless role 1 | `temper <t> of a <role word>` |
| F26 | role 3: 1 ≤ `skill` ≤ 1000 (the world does not carry the tuning) | `skill <k> outside 1 to 1000` |
| F27 | `skill` 0 unless role 3 | `skill <k> of a <role word>` |
| F28 | status 2 only for role 2 | `in exile but not a claimant` |

**`book`** (part C; ascending id):

| # | Condition | Reason |
| --- | --- | --- |
| K1 | id = previous id + 1 | `id <id> is not <n>` |
| K2 | `title` passes `is_entity_name` | `title is not a name` |
| K3 | `author` is a `figure` row of role 5 | `author <f> is not a caster` |
| K4 | `site` is a `site` row of kind 4 whose `origin_event`'s `figure` is `author` | `site <s> is not the author's tower` |
| K5 | `event` is a `book_written` of year `year` with `figure` = `author` and `site` = `site` | `event <e> is not a book_written of year <y> naming the author and the site` |
| K6 | no two rows have one `event` | `event <e> has two book rows` |

**`relation`**, rows of the kinds `ruled`, `served`, `rival`, `parent` only (key order):

| # | Condition | Reason |
| --- | --- | --- |
| L1 | types: `ruled` and `served` a_type 6, b_type 3; `rival` and `parent` 6 and 6 | `<kind> with sides of types <a_type> and <b_type>` |
| L2 | `ruled`: a is a figure of role 1 with `civ` = b | `ruled a <a> is not a ruler of civilization <b>` |
| L3 | `ruled`: `since_event` = a's `start_event` | `ruled since_event <e> is not the ruler's record` |
| L4 | `ruled` closed: `until_event` is a's `death_event` of kind `figure_died`, or b's `ending_event` | `ruled until_event <e> does not end the reign` |
| L5 | at most one `ruled` row of a civilization b in force at any event id e (`since_event` ≤ e and (`until_event` 0 or e ≤ `until_event`)), tested on the rows of b ordered by `since_event` | `two rulers of civilization <b> at event <e>` |
| L6 | `served`: a is a figure of role 3 with `civ` = b; `since_event` a `general_appointed` with `figure` a and `civilization` b | `served since_event <e> is not a general_appointed of the pair` |
| L7 | `served` closed: `until_event` is a `peace`, `war_won`, `truce`, `civ_conquered` or `civ_ended`, a `fell_in_battle` or `figure_died` naming a, or b's `ending_event` | `served until_event <e> does not end the command` |
| L8 | `rival`: a < b; `since_event` a `succession_disputed` whose `ruler` and `claimant` are a and b, in either order (the claimant is of role 2, or of role 1 as the first ruler of the new realm) | `rival since_event <e> is not a succession_disputed of the pair` |
| L8a | `rival` closed: `until_event` is the `death_event` of a or of b | `rival until_event <e> is not a death of the pair` |
| L9 | `parent`: `since_event` a `succession_disputed` naming b as `ruler` or `claimant`; `until` NULL | `parent since_event <e> is not a succession_disputed of the child`; `parent with an end` |

**Events** (ascending id; after P5-04's event checks):

| # | Condition | Reason |
| --- | --- | --- |
| E1 | every participant of entity type 6 names a `figure` row | `figure <f> has no row` |
| E2 | every `battle_led` has a `winner_general` or a `loser_general` | `battle_led without a general` |
| E3 | values: `how` 0 to 2; `outcome`, `hosted`, `again`, `library` 0 to 1; `age` 0 to its maximum; `dead` (of `blight`), `hunters`, `pass` (of `battle_led`) 0 to its maximum | `value <key> <v> outside <lo> to <hi>` (`<hi>` printed `its maximum` for the unbounded ones, as P5-04's) |
| E4 | `succession_disputed`: `outcome` 1 exactly when it has a `new_realm`; `exiled`: `hosted` 1 exactly when it has a `host` | `<kind> roles and values disagree` |
| E5 | every `book_written` has a `book` row (K5 is the other direction) | `book_written without its book row` |

**`monster`** (amends P5-02 11.4 in place): status 3's death event is a `monster_slain` **or a `monster_slain_by_hero`** of year `died` with `monster` = id and `slayer` = `slain_by`; the message stays `death_event <e> is not this monster's slaying`.

**`site`** (part C; amends P5-02 11.4 and P5-04 9.2 in place): `kind <k> is not 1, 2, 3 or 4` (was `… is not 1, 2 or 3`); for kind 4: `unknown civilization <c>` (`founder_civ`), `tower with a monster` (monster ≠ 0), `ending <k> is not 0 for a tower`; a library's `settlement` is checked by the existing `settlement <s> is not at the site's cell`; the origin kinds gain `tower_founded` (message unchanged: `origin_event <e> is not an event of its year naming this site`).

## 10. Report v8 (amends P3-07; new section 31)

### 10.1 Version and references

The header becomes **`# worldgen history v8`** at both levels (`report --help` prints it): a figure reference changes form and a block is added. A figure prints `<display name> (f<id>)` (escaped), as monsters and sites (P5-02 12.2); type 7 still prints `fa<id>` alone. The growth rule holds for v8.

### 10.2 `[figures]` (new block after `[alliances]`)

```
[figures]
figures: <n>; rulers <r>, claimants <c>, generals <g>, heroes <h>, casters <k>; alive at the end <a>
  f<id> <display name>: <role word> of <civ ref>, <race adjective>, <born>-<died|E>[, reigned <office_from>-<end|E> (reign <n>, <temper word>)][; <deeds>]; <fate>
  and <x> more
books: <n>[; <title> by <figure ref> at <site ref>, <year> e<id>]…
```

Role words `ruler`, `claimant`, `general`, `hero`, `caster`; temper words `steady`, `warlike`, `peaceable`, `grasping`, `just`. Deeds, each when not 0, joined by `, `: `<n> wars declared`, `<n> won`, `<n> realms conquered`, `<n> battles led, <w> won`, `slew <monster ref>` (each), `<n> books`, `a blight`. Fate: `alive`, `in exile with <civ ref>` (or `in the wilds`), `died <year> e<id>`, `fell in battle <year> e<id> in <region ref>`, `slain by <monster ref> <year> e<id>`, `fell with the realm <year> e<id>`. Plurals as P5-03a 9.4. **Full**: every figure, ascending id; the `books` line lists every book. **Summary**: the count line; the figures **of note**, at most `SUMMARY_FIGURES_MAX` (**6**, new constant of section 8) by (renown descending, id ascending), renown = the number of events of salience ≥ `SUMMARY_MIN_SALIENCE` in the figure's events (12: participant events and those naming it through `{r:under}`); then `  and <x> more`; the books line as `books: <n>`. When the world has more than `SUMMARY_LINES_CIVS` civilizations, the heading and count line only (as `[monsters]`, P5-02 23 item 4).

### 10.3 `[civilizations]`, companions, chronicle

- Appended at the end of each block (after P5-04's `allies:` line): `  rulers: <n> reigns, <k> recorded; first <figure ref>[; at the end <figure ref>]` (n = the last recorded ruler's `reign_no`; `at the end` for an alive realm); full: followed by `    <figure ref>: <office_from>-<end|E>, reign <n>, <temper word>` per recorded ruler, ascending reign. At summary the count line only, and not at all when the world has more than `SUMMARY_LINES_CIVS` civilizations.
- **Companions** (amends P3-07 24.2): `succession_disputed` is a companion of the `succession_crisis` and the `civ_seceded` it cites; `exiled` of its `succession_disputed`; `fell_in_battle` of the battle it cites (shown only when the battle is shown, as a war's last).
- **Battles**: `battle_led` is a battle kind everywhere P3-07 names `battle`, `battle_at_pass` and `allied_battle` (11.1).
- **Monsters**: `[monsters]`'s state `slain by <civ ref>` becomes `slain by <figure ref> of <civ ref>` when the death event is a `monster_slain_by_hero`; P5-03 10.1's stories take a slaying by a hero as the monster's `death_event` (unchanged rule).

### 10.4 `[sites]` (part C)

The count line becomes `sites: <n>; open <o>; lairs <l>, outposts <p>, mines <m>, towers <t>`; a tower's line `  si<id> <name>: <tower|library> in <region ref> at <x> <y>, since <origin_year> e<origin_event>, of <founder ref>, by <figure ref>[, at <settlement ref>]; open`. Full every site; summary the count line.

### 10.5 `[stats]` (appended, full only; `SUMMARY_STATS` unchanged)

In this order: `figures`, `figures_rulers`, `figures_claimants`, `figures_generals`, `figures_heroes`, `figures_casters`, `figures_alive`, `figures_epithet` (figures with an epithet), `epithet_bearers_max` (the most figures bearing one epithet text, 0 when none), `reigns` (Σ over civilizations of n of 10.3), `rulers_recorded`, `rulers_by_deed` (rulers whose start event is a deed, 6.4 rule 3), `rulers_regnal` (rulers with `regnal` ≥ 2), `successions_disputed`, `exiles`, `generals_appointed`, `battles_led`, `generals_fallen`, `heroes_risen`, `monsters_slain_by_heroes`, `heroes_slain`, `towers`, `books`, `blights`, `figure_age_median` (median age at death of dead figures, `-` when none), and (r3, 25.2) `summary_major_calamities` (|G|, the major calamities of the summary set). `events_<key>` lines appear by themselves. `settlements_faded` and `events_ruin_faded` stay (0 in every 0.27.0 world, 25.1).

### 10.6 Length (the summary budget)

**What figures add at summary**, default world (about 10 realms): outside `[chronicle]`, one `rulers:` line per realm (about 10) and `[figures]` at most 1 + 1 + 6 + 1 = 9 lines (with `books: <n>`), so about 20 lines; nothing at more than 16 realms but the block's heading and count line. **Inside `[chronicle]` they add no line**: the budget B is fixed (P5-02 12.9) and grows smaller by exactly the lines added outside it when L binds. A `battle_led` replaces a `battle` one for one (only a war's last battle is shown); a `monster_slain_by_hero` replaces a `monster_slain` one for one; a `succession_disputed` and an `exiled` are companions of a secession or crisis that is shown (at most 2 a world); `hero_slain`, `tower_founded` and `blight` compete for the budget (about 3 to 8 candidates a default world) and displace the lowest events above the threshold. **What they replace**: nothing outside the chronicle; inside it, battle and slaying lines by their led forms, and at the threshold the weakest candidates. The quiet stretch's note `(… N new generations of rulers.)` stays (a count of generations, P3-04 6.6; the `rulers:` line counts reigns). Expected: default median +15 to +20 lines (P5-04's bound 620 → 645 here, Paul's decision 22 item 8); every world ≤ L(C); `civs4` unchanged but for the count lines (+1).

(r3) **The riders** (25): every ruin stays (25.1), which lengthens `[present]`'s ruins list only up to its summary cap of 15 (P5-03a 9.3) and removes `, faded 0` from two count lines: about +1 line a default world (0.26.0's default worlds end with a median of 17 ruins and 2 faded, so most already reach the cap); the major calamities (25.2) are inside `[chronicle]`'s budget B and add no line where B binds (the default median `summary_events` is 235.5 at 0.26.0 against B = 250, threshold 31; `civs4` 208 at 76), at most a few where a unit's granularity leaves room. **Bound**: default median **650** (645 for figures, Paul's, plus 5 for the riders); every world ≤ L(C); `civs4` median ≤ 1,050 (P5-04's bound; r2 wrote 1,100).

## 11. Other readers

### 11.1 The battle kinds set

Everywhere a rule or a reader names `battle`, `battle_at_pass` and `allied_battle` (P5-04 11.1), `battle_led` is added with the same meaning (its dead are `winner_dead` + `loser_dead`, its pass the value `pass` when non-zero, its allies its `winner_ally` and `loser_ally`):

- P5-02 6.3.2 (undead: violent dead in a region) and P5-03a 6;
- P5-12's renaming rule `slaughter`'s `kinds` (data: `game/data/history/renaming.toml` gains `battle_led`);
- the cause words `battle=…` of the `history.report` templates of kinds 10, 16, 31, 43 and 45 (data; r3: not 13, 8.2);
- P3-07's digest (`battle_kind`), T16 and the war frame (a war's last battle);
- P5-04's allied battle readers: `[stats]` `allied_battles` and AL10 count `battle_led` with an ally participant; invariant 5 of P5-04 holds for `battle_led`'s allies; P5-04 9.1's `allied_battle without an ally` stays for kind 71 only;
- P4-04 markers (`EventSymbol::battle`);
- P4-05's group `war`.

### 11.2 The monster death kinds

Every reader of a monster's slaying reads `monster_slain` and `monster_slain_by_hero` alike: P5-02 11.4 (9.4 above), `[monsters]` (10.3), `[stats]` `monsters_slain`, P5-03 10.1's stories, the timeline's `monster_at` (status by year), P4-04's event places, P4-05's group `monsters`.

### 11.3 Browser (amends P4-05 4.4 and 9)

- **Groups**: kinds 79 to 84, 86, 87, 89 to 91 in the new group **`figures` = 8** (appended to `EventGroup` after P5-04's `alliances` = 7, as built); 85 `battle_led` in `war` (2, beside P5-04's 71 `allied_battle`); 88 `monster_slain_by_hero` in `monsters` (5); 92 `blight` in `calamity` (3). `ALL_GROUPS` becomes **0x1FF** (0xFF at 0.26.0); `event_group_word` gives `figures`; the viewer's group menu lists it last. All 92 kinds are covered (P4-05 10.2 item 2's test). Kind 19 `ruin_faded` stays in `settlements` although no 0.27.0 world has one (25.1).
- **Entity**: a figure is a selectable entity of type 6, as a realm is (P4-05 9): `BrowserIndex` gains `figures()` (`FigureEntry`: id, role, civ, born, died or −1, name and display name), `figure_block` lines in the form of 4.2 (the report's `[figures]` line and, for a ruler, its realm's `rulers:` line), `EventFilter::figure` (from `Timeline::events_of_figure`), `follow_link` accepts `f`, and `search` finds figures by display name after regions. The viewer's `Figures` tab is the viewer engineer's.

### 11.4 Markers (part C; amends P4-04 3.6)

`markers_at(Y)` gives one entry of type 6 `site` for every site of **kind 4** existing at Y (`Timeline::sites_existing`), glyph code **20** (16 + kind) `tower` (8.3's glyph name `tower`): a narrow upright tower with a pointed top in the 7 × 7 glyph cell, filled violet (`MARKER_TOWER` = (120, 60, 180)) inside a one-cell ink outline and the halo of the settlement glyphs (exact grid below); (r3) **Offsets**: as built, P5-04's mine glyph is drawn with its centre `SITE_GLYPH_OFFSET` (7) cells right of and 7 above its site's cell (its town's cell), one glyph cell to the upper right. A library (settlement ≠ 0) is drawn one glyph cell to the **upper left** of its settlement (centre 7 cells left and 7 above), so a city with a mine and a library shows both glyphs side by side above it and neither covers the other; a tower in the wild (settlement 0) is drawn at its own cell (no offset: it is a cell of no settlement, P5-02 6.6.1). `SiteMarker` keeps the site's own cell; the offset is the drawing's (by kind and settlement), in the library's `draw_markers` and in the viewer alike, and the hover hit test uses the drawn position. Hover text `<site name> (tower of <figure display name>), since <year>`, for a library `<site name> (library of <figure display name>), since <year>`. `MarkerOptions::sites` covers it (with the mines); the viewer's Markers group's `Mines` box becomes `Sites` (mines and towers; the viewer engineer). A figure's place of death is P6's (no marker here). `p4-04-markers.txt` and `markers.txt` are re-recorded.

**The tower glyph** (P4-04 5.1's offsets dx, dy from the marker's cell, y southward; classes of P4-04 5: 1 halo, 2 fill, 3 ink): 2 where dx = 0 and −1 ≤ dy ≤ 1; 3 where (|dx| = 1 and −1 ≤ dy ≤ 2) or (dx = 0 and dy ∈ {−2, 2}); then P4-04 5's halo rule; radius 3. Drawing (`h` halo, `F` fill, `K` ink, `·` none), checked by the tests:

```
tower (r 3)
··hhh··
·hhKhh·
·hKFKh·
·hKFKh·
·hKFKh·
·hKKKh·
·hhhhh·
```

Counts: 20 halo, 3 fill, 10 ink, 0 interior. Drawn in P4-04 6.1's site position (between ruins and standing settlements, ascending site id), as the mine; `GLYPH_TOWER` = 20 beside `GLYPH_MINE` = 19 in `markers.hpp`, and `MarkerType::site`'s comment widens to kinds 3 and 4. P4-04 10.1's glyph dump gains `glyph tower 3` and its seven rows at its end, after P5-04's `mine` (as built the dump holds 24 glyphs; it then holds 25), so the dump's `glyphs <sha256>` changes. The key list (P4-04 8.6) gains a `tower` row right after P5-04's `mine` row (as built the last row; glyph `tower`, colours [`MARKER_TOWER`] as the mine row carries [`MARKER_MINE`]; only while `MarkerOptions::sites` is on).

## 12. Timeline reads (amends P4-01; r7)

`Timeline` gains, from the `figure` table and the `ruled` and `served` rows:

```cpp
struct FigureState { std::int64_t id, role, civ, status; bool alive; };
std::int64_t ruler_at(std::int64_t civ, std::int64_t year) const;            // the recorded ruler with office_from <= Y < the reign's end (its ruled row's until, else E); 0 none
std::vector<std::int64_t> figures_alive(std::int64_t year) const;           // born <= Y < died (or E), ascending id
std::vector<std::int64_t> generals_of(std::int64_t civ, std::int64_t year) const;  // served rows in force at Y (since <= Y < until)
FigureState figure_at(std::int64_t figure, std::int64_t year) const;
std::span<const std::int64_t> events_of_figure(std::int64_t figure) const;  // ascending: events with it as participant, and events whose text names it through {r:under} (7.9's lookup)
```

`ruler_at` gives 0 for a year of an unrecorded reign (the viewer shows "unrecorded"). Dumps (P4-01 13.1): D(Y) gains `ruler <civ> <figure>` per alive civilization with a recorded ruler at Y, ascending. The viewer's hover text on a realm may add `ruled by <name>` (the viewer engineer, P4-03's hover).

## 13. Invariants

1. At every event at most one `ruled` row of a civilization is in force (7.9's inclusive lookup); a ruler's `office_from` ≤ its `ruled` row's `since`; reigns of a realm follow in `reign_no` order and do not overlap in years.
2. Every alive realm at E has a recorded ruler whose `ruled` row is open; every civilization has a recorded first ruler (`reign_no` 1) whose start event is a `came_to_rule` or a `succession_disputed` with `outcome` 1.
3. Every `war_declared`, `alliance_betrayed`, `spoils_shared`, `spoils_withheld`, `civ_conquered` and `civ_ended` has a recorded ruler of its acting civilization at the event when `name_rulers` is true (so `{r:under}` is never empty there).
4. A figure's `born` ≤ `office_from` ≤ the year of its `start_event`; `died` − `born` ≤ 2 × `lifespan_years`, apart from a death that waited for a quiet step's budget (6.3, 6.9), which is later by at most the waits.
5. Every `battle_led` names at least one general, each with an open `served` row of the war's side at the event; every general participant is of its side's civilization.
6. Every `monster_slain_by_hero` and `hero_slain` names a hero of the slayer's (or the figure's) realm alive at the event; a realm has at most one living hero.
7. Every `succession_disputed` follows a `succession_crisis` of the same year and realm; `outcome` 1 exactly when a `civ_seceded` cites that crisis; the claimant of `outcome` 1 is the first ruler of the new realm.
8. Every figure event in a quiet step had its charged civilization's budget (W9 by construction).
9. Every tower is a kind 4 `site` of its caster's realm at its `tower_founded`; every `book` row's author is a caster with that tower; at most `books_max` books and one blight per caster.
10. With the `zero` tuning, I0 (7.11) holds.
11. P3's hard bounds (P3-00 12.3), "no race dies out", the shelter rule, and the invariants of P5-02, P5-03, P5-03a and P5-04.
12. Regnal numbers (6.14): within a civilization the rows of role 1 have ascending `reign_no` in id order; a ruler with `regnal` r ≥ 2 has exactly r − 1 earlier recorded rulers of its civilization with its `name`, the first of them with `regnal` 0; `regnal` ≥ 2 only for rulers of a culture with `regnal_permille` > 0 and never for the first ruler of a realm; no name is shared across civilizations or with a non-ruler; display names are unique in a world.
13. Epithets (6.10): an epithet text earned by an ordinary deed only (v < `substantial` or `substantial` 0) is borne, in closing order, by a figure only while fewer than `epithet_repeat_max` earlier figures bear it; every figure whose value reaches an entry's `substantial` threshold, with no earlier entry of its role giving it an epithet, bears that entry's text.
14. (r3, 25.1) No settlement row has status 4 and no event of kind `ruin_faded` exists; every settlement ruined at some year is ruined at E (status 3, `ruined_year` and `ruin_event` set, `ending` ≥ 1), and its census rows after its ruin all have status 3. (P5-03's invariant 10 is superseded by this one; invariant 11's "invariants of P5-02, P5-03" read with it.)
15. (r3, 25.2) Every member of G is in the summary set; |G| ≤ `SUMMARY_CALAMITY_MAJOR_MAX`; every member is a `famine`, `plague` or `plague_outbreak` that is major by 25.2's rule, and no major calamity outside G ranks before a member.
16. (r3, 6.8, 7.8) No two open sites of kind 4 have the same `settlement` ≠ 0, and no two sites of kind 4 share a cell; every site of kind 4 is in the region of its `tower_founded`.

## 14. Determinism notes (for the auditor)

- (r3) **Every new keyed stream** (3; the 83rd to 97th of `all_stream_names`, in this order), as draw(key, w0, a, b, k) of P3-02 4.1; every key is ids and fixed discriminators, never a loop position:

| # | Stream | w0 | a | b | k | Drawn |
| --- | --- | --- | --- | --- | --- | --- |
| 83 | `hist.figures.reign` | the reign's start year s (`draw_at`) | civilization c | reign number n | 0 length, 1 accession age, 2 temper | once per (c, n), at the reign's start (6.3; a claimant's realm (c', 1) at the split, 6.5) |
| 84 | `hist.figures.regnal` | the step's year | civilization c | reign number n | 0 the reuse test, 1 the name picked | at most once per (c, n), at the naming (6.14) |
| 85 | `hist.figures.life` | the figure's entry year y (`draw_at`) | figure f | 0 | 0 age at entry, 1 age at death | once per non-ruler f (6.9) |
| 86 | `hist.figures.appoint` | the step's year | the war's `war_declared` id w | civilization c (a principal) | vacancy number v_c | once per (w, c, v_c) (6.6) |
| 87 | `hist.figures.skill` | the step's year | figure f | 0 | 0 | once per general f (6.6) |
| 88 | `hist.figures.fall` | the step's year | the `battle_led` id | figure f | 0 | once per (battle, general) (6.6) |
| 89 | `hist.figures.exile` | the step's year | the `succession_crisis` id e | civilization c | 0 | once per crisis that does not split (6.5) |
| 90 | `hist.figures.hero` | the step's year | the `monster_raid` id r | civilization c (the raided) | 0 | once per raid read (6.7) |
| 91 | `hist.figures.caster` | the step's year | civilization c | 0 | 0 | each T3 per alive visible realm without a living caster (6.8) |
| 92 | `hist.figures.book` | the step's year | figure f (a caster) | 0 | 0 | each T3 and Q4 per living caster below `books_max` (6.8) |
| 93 | `hist.figures.blight` | the step's year | figure f (a caster) | 0 | 0 | each T3 per living caster without a blight (6.8) |
| 94 | `hist.figures.tower_name` | the step's year | site id s (`next_site_id()`) | 0 | 0 | once per tower (6.8) |
| 95 | `hist.names.person` | attempt t (P3-02 4.2) | culture id | figure id | attempt index j | P3-02 4.3's attempts, once per named figure, in creation order (6.11) |
| 96 | `hist.names.book` | the step's year | book id | 0 | 0 | once per book (6.8) |
| 97 | `hist.monsters.hero_fall` | the step's year | monster m | civilization c | 0 | once per failed hunt of m by c with a hero, in T4 (7.7) |

  Each decision is made at most once per key (appointments once per vacancy; reigns once per number); the yearly ones (91 to 93) differ by w0. No existing stream gains a draw; P5-03's `hist.monsters.rise` keeps its key under 25.3's rules (only the threshold it is compared against changes).
- `draw_at`'s year lies inside the step (7.10, checked); it is a reign's or a figure's start, both functions of earlier state, never of a loop position.
- Containers: vectors by id (figures, lines, books), `std::map` keyed by war event id (known wars), ordered sets; no unordered container, no pointer keys. The log cursor reads in ascending id.
- Relations opened and closed on key copies (6.13); figure rows written at the end in id order; `HistoryView`'s ruler lookup sorts `ruled` rows by (civ, since_event).
- Arithmetic int64 with checked multiplication: largest products λ × 1000 (≤ 10^7), force × 1000 × 2 (force < 2^40, P5-04 7.5), weights ≤ 5 × 1000.
- The temper normalization `mean_r` is computed once per race from data, integer.
- Names: P3-02 4.3 in creation order; book titles and tower names by keyed start picks and cyclic search. Regnal reuse (6.14) by (civilization, reign number) keys over N(c), a vector in first-bearer order (no map iteration by name); the registry gains the numbered form in creation order.
- Epithets (6.10) are decided once at E in closing order (death event id, then figure id), a total order of stored values.
- (r3) **The riders**: 25.1 removes a step's events and a code path (fading), adds no draw. 25.2 is report text only: integer impact from stored values (`event_value`, `settlement_census` rows in key order, `event_cause`), ranked by (ended, impact, id), no floating point; `report` text stays part of the cross-platform contract. 25.3's thresholds are int64 with floor division in a stated order; `undead_regions_ref` × the population term is bounded (P_W < 2^31, × 1000 < 2^41).

## 15. Targets and calibration

### 15.1 Targets

At 1024 defaults, seeds 0–99, unless stated; P5-00 8.2's other sweeps report. Bounds are r2 estimates.

| Target | Definition | Bound |
| --- | --- | --- |
| **FG1** | figures per world | median 60 to 120, max ≤ 200; rulers (role 1) ≥ 50% of figures |
| **FG2** | recorded rulers per realm; reigns per realm | median 3 to 8; reigns reported by race (elves about 40, goblins about 260 over a whole history) |
| **FG3** | generals | appointments in 25% to 50% of war sides; distinct generals median 6 to 20; `battles_led` 20% to 50% of all battles |
| **FG4** | generals fallen | 5% to 25% of generals |
| **FG5** | heroes | median 2 to 8; a hero in ≥ 80% of worlds with a monster raid; `monsters_slain_by_heroes` 30% to 70% of all slayings; heroes slain reported |
| **FG6** | disputes and exiles | `successions_disputed` 10% to 40% of crises; exiles median 1 to 4 |
| **FG7** | casters (part C) | casters median 2 to 6; a tower in ≥ 80% of worlds; books median 3 to 10; blights median ≤ 1, max ≤ 3 |
| **FG8** | long lives | median age at death of elven and faerie figures ≥ 4 × that of goblin and orc figures (by data); figures alive through at least one quiet step: ≥ 30% of figures |
| **FG9** | names | person names rejected by the filter, reported; no display name longer than 81 bytes (48 + 8 for the numeral + 1 + 24); the critic reads them |
| **FG10** | regnal numbers | recorded rulers with `regnal` ≥ 2 among recorded rulers of human and dwarven realms, pooled: 10% to 30% (each culture reported); 0 for every other culture (by rule); the highest `regnal` reported |
| **FG11** | epithets | per world, the bearers of any one epithet text ≤ max(2, figures div 10) (no epithet on more than a tenth of named figures); figures with an epithet 15% to 40% of figures (reported per epithet and per role, with the bearers by a substantial deed counted by the sweep tool from the deed values) |
| **FX1** | generals win | battles won by the side with the larger skill (one side led, or both with different skills): 55% to 70% |
| **FX2** | tempers in war | `war_declared` per reign-year under warlike rulers ≥ 1.3 × that under peaceable rulers of the same race (pooled; reigns from the lines' dump, 16) |
| **FX3** | tempers in alliances | shares granted by just principals minus by grasping ones ≥ 20 points; betrayals by grasping rulers ≥ 1.5 × their share of reigns |
| **FX4** | heroes hunt | slayings per hunted monster-year with a hero ≥ 1.5 × without (reported) |
| **FX5** | short reigns | secession per crisis after a short reign above that after a long one; civilizations per world median within ±1 of 0.26.0's |
| **C1** | wars per world | median 22 to 28 (0.26.0's, the lead's bound) |
| **AL** | P5-04's AL1, AL5, AL7, AL9, SP1, SP2 | within their bounds (regression) |
| **RR** | P5-03a's RR2a to RR5 | within their bounds (regression) |
| **M** | P5-02's and P5-03's monster targets (as bounded in P5-03 r2 21 and P5-03a 11) | within bounds; monsters slain per world at most +50% of 0.26.0's median (3: so ≤ 4) |
| **M4a** (r3) | a living monster at E (P5-02 M4's first part) | ≥ 85 of the default seeds 0–99 and ≥ 85 of 100–199 (0.26.0: 77 and 81) |
| **M4b** (r3) | a lair on a former settlement (overrun, or risen at a ruin) open at E | ≥ 65 of seeds 0–99 and ≥ 65 of 100–199 (Paul's bound; 0.26.0: 58 and 55); overruns median 2 to 4 and towns ≤ 10% of overruns (P5-02 M4, P5-03 F8) |
| **UD1** (r3) | undead where they were almost absent | `--monster-pressure low` (30 worlds): a rising in ≥ 9 worlds (0.26.0: 1); 2048 (10 worlds): a rising in ≥ 6 worlds, risings median ≥ 1 (0.26.0: 4 worlds, median 0) |
| **UD2** (r3) | undead elsewhere | default (both sets): worlds with a rising 70 to 95 (78, 84), risings median ≤ 3 (1.5, 2); `high`: median ≤ 8 (5); `civs4`, 512, `calm`, `turbulent` reported |
| **UD3** (r3) | undead not crowding the end | risings in the last 500 years of the history ≤ 50% of all risings, pooled over each default set (0.26.0: 45% and 56%), and ≤ 60% in every other sweep |
| **RU1** (r3) | every ruin stays (25.1) | no settlement with status 4 and no `ruin_faded` in any world of any sweep (by rule); ruins at E reported (0.26.0 default median 17 ruined + 2 faded; `civs4` 53 + 16) |
| **CB1** (r3) | major calamities (25.2) | `summary_major_calamities` median: default 3 to 8, `civs4` and 2048 ≥ 6; every member of G is in the summary chronicle (by rule); a famine or plague line in the summary of ≥ 95% of `civs4` and 2048 worlds (0.26.0: calamity share median 0%) |
| **CB2** (r3) | the calamity share stays modest | P5-01's T17 (calamity lines of the summary chronicle, default median ≤ 10%) holds; the default summary's median grows by at most 5 lines with 25.2 against the same build with `SUMMARY_CALAMITY_MAJOR_MAX` 0 |
| **G2** | standing settlements; population at E | medians ≥ 81 and ≥ 205,000 (the bounds in force after P5-04; 0.26.0: 83, 208,364), with 25.1's ruins blocking their sites to E |
| **T16** | repeated battles (led ones included) | ≤ 2 per thousand |
| **AL1** | P5-04's alliances formed | median 6 to 15 (0.26.0: 14); with AL's other rows above |
| **T18** | summary lines | default median ≤ **650** (645 Paul's for figures, + 5 for the riders, 10.6); every world ≤ L(C); `civs4` median ≤ 1,050 (P5-04's bound; r2's 1,100 was a slip) |
| **P** | participation | every present race has a named figure in ≥ 95% of worlds where it has a realm |
| **timing** | `generate` seed 0 1024 release | at most 2 s over 0.26.0 (P5-00 8.1's 5 s share) |

### 15.2 Calibration (data only but for 25.3's two rules, the engineer, in this order; values into r4, as built)

1. FG1, FG2 by nothing (rules); if FG1's median exceeds 120, `exile_permille` first, then the deeds list is reported to the lead (a rule change).
2. FG3 by `general_scale_permille`; FG4 by `fall_loser_permille`, `fall_winner_permille`.
3. FX1 by `skill_min`, `skill_max` (lower first if C1 or AL move).
4. C1 and FX2 by `temper_strain_permille` (warlike and peaceable together, keeping their mean).
5. FG5 by `hero_scale_permille`; M and FX4 by `hero_hunt_permille`, `hero_might_permille`; heroes slain by `hero_fall_permille`.
5a. (r3) **Undead** (25.3's rules in place first, then data): UD1 by the undead kind's `wake_rate` (`game/data/monsters/undead.toml`, 15000) and, at 2048 only, `undead_regions_ref`; UD3 by `undead_dead_permille` (up: the threshold grows with the late population) with `wake_rate` holding UD2's counts, then `undead_window_years`; UD2 checked after each.
5b. (r3) **M4a** by the wild kinds' `lifespan_years` and `rewake_ppm` (`game/data/monsters/*.toml`), then `living_permille` (`history.monsters`, 100), then the wild kinds' `wake_rate`; if heroes take M's slain bound or M4a out of reach, `hero_hunt_permille` and `hero_might_permille` down before any monster value. **M4b** by the undead kind's `lifespan_years` (1000) and `max_alive` (2) (a host risen at a sacked ruin is a lair on a former settlement), then the kinds' `overrun_permille` and `history.monsters.overrun_people_ref` within M4b's overrun bounds and F8. G2, C1 and T16 after each.
6. FG6 by `exile_permille`.
7. FX5 by `short_reign_split_permille` (re-check C1, civilizations per world).
8. AL7, SP1, FX3 by `temper_allied_permille` and `temper_spoils_permille`.
9. FG7 by `caster_ppm`, `book_permille`, `blight_ppm`; G2 by `blight_toll_permille`.
10. FG10 by `regnal_permille` of the human and dwarven cultures (the ratio 250 : 200 kept).
11. FG11 by each epithet's `substantial` threshold (raised first for the epithet with the most bearers), then by `min`; an epithet that still has more bearers than the bound goes to the lead (22, lead item 2).
12. T18 by saliences (8.2); (r3) CB1 and CB2 by the constants `SUMMARY_CALAMITY_MAJOR_PERMILLE` and `SUMMARY_CALAMITY_MAJOR_MAX` (25.2; code constants of P3-07 section 8, recorded in r4).

C1, AL (AL1 included), RR, M, M4a, M4b, UD2, G2, T16 and T18 are checked after each step. Every value the engineer changes goes into r4 (as built), with the measured column.

## 16. Order of implementation and roles

One branch, one engineer, one test writer from the specs separately. `GENERATOR_VERSION` 0.27.0 and `SCHEMA_VERSION` 12 in step 1; goldens once, in step 12.

| Step | What | Spec |
| --- | --- | --- |
| 1 | Version, schema 12 (`figure`, `book`, site kind 4); data: race, culture and monster fields, `history.figures`, `history.report`, renaming data; both fixture roots | 4, 8.2, 9, 11.1 |
| 2 | Catalogue 79–92; form `under`, `{@book}`; display names of type 6; `draw_at` | 7.9, 7.10, 8 |
| 3 | `FiguresSubsystem` part A: log, lines, naming, disputes, exiles, deaths, epithets, names, the present, regnal numbers | 5, 6.1–6.5, 6.9–6.14 |
| 4 | `FigureView`; seams in conflict (tempers, only civilization pairs), peoples (split bonus), alliances (spoils, allied strain) | 7.1–7.5 |
| 5 | Generals: appointments, vacancies, falls, standing down; `battle_led` in conflict | 6.6, 7.6 |
| 6 | Heroes: rises; the hunt seam in monsters | 6.7, 7.7 |
| 7 | Casters: towers through the site seam, books, blights | 6.8, 7.8 |
| 7a | (r3) The riders: no ruin fades (peoples' Q3, the two `history.peoples` fields); old ruins in the markers library and CLI help; the major calamities of the summary; the two undead rules and `undead_regions_ref` | 25 |
| 8 | Battle kinds and monster death kinds sets; browser groups and the figure entity; the tower marker | 11 |
| 9 | Reader checks; `Timeline` reads; `HistoryView`'s ruler lookup | 9.4, 12 |
| 10 | Report v8 (with 25.1's count lines and 25.2's set) | 10, 25.1, 25.2 |
| 11 | I0 on the fixtures; calibration (15.2, the riders' targets included); timing; a lines dump (`report --history --level full` holds the recorded rulers; the engineer's sweep tool also dumps every reign, for FX2) | 7.11, 15 |
| 12 | Goldens, `schema.md`, README and CLAUDE.md (lead) | 9.4, 17 |

- **Engineer**: steps 1 to 11; tell the viewer engineer 11.3, 11.4, 12 and 25.1 (e).
- **Viewer engineer**: the `Figures` tab and selection, the group menu's `figures`, the Markers group's `Sites` box (mines and towers) and the tower glyph with the library's offset, the realm hover's ruler, the `Old ruins` box, key row and hover text (25.1 (e)), the headless check's figure, tower and old-ruin cases (25.1 (e)).
- **Test writer**: 18, from the specs only.
- **Determinism auditor**: 14, the seams of 7, `draw_at`.
- **Validation**: 15 on its sweeps, the cross-preset comparison, timing.
- **World critic**: 20.3's reading list.

## 17. Golden values and CI

Every world file changes (`GENERATOR_VERSION`, schema 12, `content_hash`, catalogue rows, every history from the first reign's temper on). The empty world gains two pages.

| Golden | Re-recorded | By |
| --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, `GOLDEN_CASES`, `stages_pipeline_test.cpp`, README phrase hash | yes | validation, after the cross-preset comparison |
| `core_vectors.txt`, `p1-0*`, `p3-02-names.txt`, `p3-03-chronology.txt` | no, must not move | — |
| `p3-01-data.txt` (the `history.figures` id, the race, culture and monster fields, (r3) the two `history.peoples` fields removed and `undead_regions_ref`), `p3-04-engine.txt`, (r3) `p3-05-peoples.txt` (no ruin fades), `p3-06-conflict.txt`, `p3-07-report.txt`, `p4-01-timeline.txt`, `p4-04-markers.txt` (towers; (r3) old ruins: the years R(s) + `OLD_RUIN_YEARS` ≤ E join its year list, 25.1 (e)), `p4-05-browser.txt`, `p5-12-renaming.txt`, `p5-02-monsters.txt`, `p5-03-relations.txt`, `p5-04-alliances.txt` | yes | test writer |
| **`p5-06-figures.txt`** (new) | new | test writer, `worldgen/tests/reference/p5_06_figures_reference.py`: per world case and fixture, hashes of the `figure` and `book` rows, of the `ruled`, `served`, `rival`, `parent` rows, of events 79–92, of the kind 4 `site` rows, the counts of 15's FG rows, and (r3) per world case the `summary_major_calamities` value and the ids of G (25.2) |

`world-hashes.sh` keeps sixteen cases; its `markers.txt` hashes change with the towers and the old ruins (the command, `--markers --faded`, is unchanged). New CTest shards `worldgen-tests-figures` (pattern `*history_figures_*`, the convention of `*history_alliances_*`; r2 wrote `*figures_*`) and `worldgen-tests-slow-figures` (`SLOW slow-figures … "*-slow" "*history_figures_*"`); test files `worldgen/tests/history/history_figures_*.cpp`.

## 18. Tests required

(r3) **Default or slow.** Every case of 18.1 is in the default suite (`worldgen-tests-figures`, fixtures and maps of 512 or smaller). The slow cases are exactly those marked `*-slow` in 18.2 and 18.3 (doctest suites whose names end in `-slow`, run by `worldgen-tests-slow-figures` under the `slow` label, `ctest --preset linux-gcc-release-slow`): every case on a 1024 world, the `civs4` cases and the golden comparisons of the 1024 world cases. Viewer cases are the headless check's (25.1 (e)), not CTest shards of `worldgen-tests`.

### 18.1 Unit (C++; default suite)

1. Data: every new race, culture, monster and `history.figures` field's bounds and absence; `race_temper_weights`, `figures_skill_ordered`, `figures_epithet_slayer`; the `book_title` and `epithet` grammars.
2. `draw_at`: equal to P3-02's `draw` with that year; fatal outside the step.
3. Lines: a reign's length, age and temper at its two extremes; T3's succession at e = Y and not at e = Y + 1; Q4 over several unnamed reigns in one step, each drawn at its exact start; a named ruler's death in Q4 dated Y, with the next reign from Y; the wait when the budget is 0; the crisis in the year of an accession.
4. Naming: each rule of 6.4 (founder with `came_to_rule`; each deed kind, with no event and `office_from` before the record; the realm's end naming and closing in one bookkeeping); the tallies moving to the figure; `name_rulers` false: none.
5. Disputes: a split (both rulers, `rival`, `parent` when the dead ruler was named, the claimant as reign 1 of the new realm); an exile with a host (largest disposition, then smaller id; at war excluded; beyond `exile_hops` excluded) and without; no naming when the draw fails.
6. Generals: appointment at a declaration on both sides; re-appointment within `career_permille` and not beyond; vacancies after a fall; skill in the win chance only (casualties unchanged); `battle_led` chosen only with a general; with allies and at a pass; falls for winner and loser; standing down at a war's end, at a quiet end, at a side's end.
7. Heroes: rise after a raid, none with a living hero; the hunt chance and strength with a hero; `monster_slain_by_hero` in place of `monster_slain` (status, `slain_by`, the lair); `hero_slain` on a failed hunt; driven off leaves the hero.
8. Casters: a library at a city capital, a tower at the height otherwise, none without a height; tower names (cyclic, registry, fallback); books in T3 and Q4 (budget), `books_max`; titles (cyclic, fallback numeral); a blight's deaths, `note_reduction`, one per caster.
9. Lives: 6.9's ages at their extremes; `born` clamped at the awakening; deaths of age in T3 and Q4 with their regions and settlements; relations closed at death; `parent` kept.
10. Epithets: decided at E in closing order; rule order; `min`; the cap of `epithet_repeat_max` on ordinary earnings with the next entry taken; a substantial earning given with two or more bearers already (a third and fourth `the Conqueror` with two conquests each); an ordinary earning refused when two substantial bearers exist; `substantial` 0 never substantial; `{slayer}` by the kind of largest `might` slain; each deed value of the table at its edges (`founded_years` of a realm alive at E and of an ended one, `reign_permille` of a living ruler); the display name in sentences before the epithet was earned.
11. Seams: strain with each temper and the normalization (a race's mean factor is 1000 ± 1); allied strain and spoils with grasping and just; the split bonus at a short and a long reign; conflict's housekeeping and conquest not touching figure rows; no view: 0.26.0's behaviour.
12. Rendering: `{r:under}` for a figure role (none, one) and a civilization role (a ruler recorded at, before and after the event; the inclusive end); `{@book}`; `template_problem` messages; every new and changed template on a fixture event.
13. Reader: each message of 9.4 on a corrupt fixture; a version 11 world refused.
14. Report v8 on fixture **H17** (`h17-figures.toml`, from P5-04's H16: a founder, two rulers named by deed, a split with rivals and a parent, an exile with a host and one to the wilds, two generals (one falls, one re-appointed), a hero who slays and one slain, a caster with a library, two books and a blight, a realm conquered with its last ruler, a human line with `Aldric` and `Aldric II`, an epithet earned by a substantial deed by a third bearer), both levels; companions; `[stats]`; `[sites]` with a tower; the `rulers:` lines.
15. Timeline: `ruler_at` across an unrecorded reign, a recorded one and the present; `figures_alive`, `generals_of`, `figure_at`, `events_of_figure` (with derived events).
16. Browser and markers: every kind 79–92 in its group; the figure entity, filter, search; the tower glyph and offset.
17. Regnal numbers: with `regnal_permille` 1000 a second named ruler takes the first's name with `regnal` 2 and a third the same name with 3 (`Balin`, `Balin II`, `Balin III`; the first stays `Balin` in every sentence and row); with 0 no draw is made; a founder never reuses; a seceded realm's line never takes its parent's names, while the parent's winner may; `regnal_max` stops reuse of a name that has reached it; a fallback name is never reused; the registry and filter are untouched by a reuse; roman numerals II to XXXIX; the display name with an epithet (`Balin II the Victorious`); the reader messages of 9.4 for `regnal`.
18. (r3) Alliances (6.15): a `battle_led` with allies on both sides and a general on one (skill on the whole side's force, the ally participants of P5-04 7.7); no general at an `ally_joined_war`; a betrayal's war naming generals; an ally of c chosen as an exile's host by disposition, a realm joined against c refused; a split keeps c's alliances and gives c' none; `{r:under}` only on 69, 74, 75 of kinds 63 to 78.
19. (r3) Sites: `open_site` with kind 4 and the next id (and its fatal message for kind 2, 5 and a wrong id); a city whose library stands raises its next caster's tower at the height, two towers of one region at two cells; a library at a sacked city's ruin does not block an undead lair there (`kind == 1 || kind == 2`); the library glyph at (−7, −7) and the mine at (+7, −7) of one town, a wild tower at its cell; the key list's `tower` row after `mine`.
20. (r3) **Every ruin stays** (25.1): a sacked ruin, an overrun ruin and an abandoned one still status 3 with `ruined_year`, `ruin_event` and `ending` at E on a fixture world spanning more than 1,500 years after each ruin; no `ruin_faded`; a ruin keeps blocking its site circle to E (P3-05 6.2); the data loader refuses `ruin_fade_years` and `ghost_fade_years` as unknown keys; the report's count lines with and without faded rows (a fixture with one faded row keeps `, faded 1`; a world without prints none); the markers: a ruin of year R is `ruin` at R + 1499 and `faded` (old) at R + 1500, absent without the option; a status 4 fixture row as before; the CLI help line.
21. (r3) **Major calamities** (25.2): on a fixture (H17 or a sibling) with a famine of impact 99 and 100 per mille of its realm (out, in), a plague outbreak that is a cause of a settlement's `ruin_event` (in, ranked first), nine majors (the ninth out by rank), a realm of population 0 (impact 0), an unowned settlement; G in the summary set whatever the threshold and outside the per-phase cap (a phase with three majors keeps all three and no other calamity; a phase with one major keeps it and the best other one); census rows of the event's year not counted; `summary_major_calamities`; `full` unchanged but for the two `[stats]` values.
22. (r3) **Undead thresholds** (25.3): T at M = 500, 1000, 1600 (the low threshold equal to the normal one, the high one lowered as before); T with R = 64, 65 and 200 and `undead_regions_ref` 64 and 0 (0 the 0.26.0 rule), each at its floor `undead_min_dead`.

### 18.2 Property and identity

- **I0** (7.11) on the fixture worlds and seeds 0–4 at 512 (default suite); seeds 0–4 at 1024 (`*-slow`).
- Invariants 1 to 13 and (r3) 14 to 16 (13) on seeds 0–9 at 512 (default suite); on seeds 0–19 at 1024 and `civs4` seeds 0–4 (`*-slow`).
- Resume (P3-04 16.3): a world resumed at a quiet step and at a tick gives the same lines (draw_at); at 512 (default suite).
- (r3) No `ruin_faded` and no status 4 (invariant 14): on every world the property suites generate (default and `*-slow` as above).

### 18.3 Reference

`p5_06_figures_reference.py` inside the existing reference replay (with P5-02's to P5-04's): sections 6 and 7, (r3) 25.1's peoples without fading, 25.3's undead thresholds and 25.2's summary set, and a `check-world` mode for invariants 1 to 9 and 11 to 16. Its fixture and `figures-small` cases are in the default suite; its 1024 world cases (`p5-06-figures.txt`'s world lines) are `*-slow`. A hand-made root **`figures-small`** (the alliances stress data with `general_scale_permille` 1000 and every race's `general_permille` 1000, `hero_scale_permille` 1000 with `hero_permille` 1000, `exile_permille` 1000, `caster_ppm` 1000000, `book_permille` 1000, `blight_ppm` 100000, `fall_loser_permille` 500, every culture's `regnal_permille` 1000, `regnal_max` 3) makes every kind of 79 to 92, regnal numbers up to III and the `regnal_max` stop happen on small maps.

## 19. Part C: minimal casters (where its rules are)

Part C is in this stage (Paul, 2026-11-24, 22 item 1). Its rules are folded into the numbered sections, each marked "(part C)":

| What | Section |
| --- | --- |
| race field `magic_permille`; culture fields `tower_forms`, `library_forms`, `book_titles`; tuning `caster_*`, `book_*`, `blight_*` | 4 |
| streams `hist.figures.caster`, `.book`, `.blight`, `.tower_name`, `hist.names.book` | 3 |
| rises, towers and libraries, books, blights | 6.8 |
| the site seam for kind 4 | 7.8 |
| events 90 `tower_founded`, 91 `book_written`, 92 `blight`; `{@book}` | 8 |
| site kind 4, table `book`, reader checks | 9.3, 9.4 |
| `[figures]`'s books line, `[sites]` towers, `[stats]` keys | 10 |
| the tower glyph 20 | 11.4 |
| invariant 9; FG7; calibration step 9; tests 18.1 item 8; the `figures-small` caster values | 13, 15, 18 |

**Size**: one table, one site kind, three kinds, four data fields, five streams, one glyph and their checks and tests: about a quarter of the stage's code and tests, and a few hundred bytes a world.

**The seam to the full P5-09** (deferred, 20): a caster is a figure of role 5; a book is a `book` row with its author, tower, year and event. P5-09 adds the `spell` table (design 6: inventor figure, year) and a book's spell (a column or a `book_spell` table, its schema bump), the race field `magic_schools` beside `magic_permille`, and spell invention as events naming a caster; it may end a blight or add one by year. Nothing here is renamed for it. "Ends a calamity" is left to it: ending a monster or a plague would mean changing another subsystem's state from here (reading 23 L6, accepted by the lead).

## 20. Seams, for P6, and for the world critic

### 20.1 Seams for later stages

- **P5-07 Mountain depths**: a hold lost to the deep is a realm's or a settlement's loss; when it ends a realm, 6.4 rule 4 names its last ruler (the `civ_ended`/`civ_conquered` path, or P5-07's own ending kind added to rule 4's list), and a lord lost with the hold can be a `figure_died` with a new `how` code (3 lost in the deep; appending a value code is not a kind change but is a reader-check change). A delving's lord can be named by appending P5-07's delving kind to 6.4 rule 3's deeds.
- **P5-08 Pantheon**: a god's champion is a figure: P5-08 appends role 6 `champion` and creates it through a seam like P5-04 7.16's (`next_figure_id`, `add_figure`), its `divine_champion` event naming the figure; a cult's founder or priest can be a figure with a new role; a god's blessing or curse may read `FigureView`.
- **P5-11 Artifacts**: an artifact's maker is a figure (a caster, or a dwarven smith as a new role) or a forge (P5-04's mine); holders are figures (rulers, generals, heroes carry artifacts) through `artifact_holding` rows of holder type 6; a general's or hero's death leaves an artifact on the field (P5-11's loss events cite `fell_in_battle` or `hero_slain`).
- **Vassalage** (P5-04 20): a ruler who swears fealty is a recorded ruler at the oath (a deed).
- **Factions** (deferred): membership as `relation` rows figure → faction (type 7).

### 20.2 For P6 (P5-00 10.2)

Tombs: every dead figure has `death_region` and `death_settlement` (a ruler's capital, a general's battlefield, a hero's monster's lair, a caster's tower) and its `death_event`; rulers of realms that fell lie with their realms' ending places. Heirs in exile: claimants alive at E with status 2 and their host. Descendants as quest givers: the present rulers (`reigned_at_end` or recorded and alive), `parent` rows, and every realm's line length (`reign_no`). Books' present places: each `book` row lies at its tower at E (P6 moves and copies them with `spellbook`). Towers are sites for dungeons of origin "a dead mage's tower". No replay is needed.

### 20.3 For the world critic

0.27.0; default seeds 0, 1, 17, 42, 84; `civs4` 1; `--monster-pressure high` 7; 512 seed 3: whether rulers read as a line (founders, warlike and grasping rulers, the last ruler of a fallen realm, the present); whether the deed-named gaps read naturally; disputed successions and exiles; generals and their epithets; heroes and their slayings and deaths; casters, towers, book titles and blights; the person names by culture; `[figures]` and the `rulers:` lines; whether the effects read in the chronicle (a warlike ruler's wars, a grasping ruler's kept spoils). (r3) The riders: the summary's major famines and plagues in a default, a `civs4` and a 2048 world (whether they read as significant, and what they displaced); the ruins kept to E in `[present]` and on the map with `Old ruins` off and on; the undead under `low` and at 2048.

## 21. Amendments to earlier specs

| Spec | Section | What | Note |
| --- | --- | --- | --- |
| P3-01 | race fields; kind `history` ids | `temper_weights`, `general_permille`, `hero_permille`, `magic_permille`; id `figures` | pointer |
| P3-02 | 3, 4.3, 4.5 | culture fields (with `regnal_permille`); name kinds `person`, `book`; the registry holds a regnal name's numbered form, and a line's rulers may share a base name (6.14) | pointer |
| P3-04 | 6.1, 6.4, 6.6 W4, 8.2, 8.3, 8.4, 8.5 | entity type 6 in use; `draw_at` (amendment note in P3-04 6.4, added with r2); configuration; kinds 79–92; display name of type 6; form `under`, `{@book}` | r26 pointer |
| P3-05 | 10.3 | the split bonus | pointer |
| P3-06 | 5.2, 7.3, 7.6, 8.1 | tempers in strain; generals' skill; `battle_led`; relation passes on civilization pairs only | r25 pointer |
| P3-07 | new 31; 24.2 | format v8, `[figures]`, `rulers:` lines, references of type 6, companions, battle kinds, `[stats]` | r26 pointer |
| P4-01 | 5, 13.1 | figure reads, ruler dumps | r7 |
| P4-04 | 3.6, markers | `battle_led` as a battle; towers (glyph 20) | r4 pointer |
| P4-05 | 4.4, 9 | group `figures` = 8, `ALL_GROUPS` 0x1FF; kinds 85, 88, 92 in existing groups; the figure entity | r5 |
| P5-00 | 3.1 | the scope note of 2026-11-24 (24); P5-06's paragraph gains the minimal casters | note |
| P5-02 | 4.2, 6.4.2, 11.2, 11.4, 12.4, 12.5 | `slayer_epithet`; heroes in hunts; site kind 4; monster status 3 by a hero; `[monsters]` and `[sites]` | pointer |
| P5-03 | 9.3, 10.1 | heroes in hunts; stories read a hero's slaying | pointer |
| P5-04 | 6.11, 7.2, 7.7, 7.12, 7.16, 11.1 | tempers in spoils and allied strain; generals in allied battles; housekeeping; kind 4 through the site seam; the battle kinds set | pointer |
| P5-12 | data | rule `slaughter` reads `battle_led` | — |
| (r3) P3-05 | 6.2, 8.6, 12.1, invariants 2 and 7, test 13 | no ruin fades: 8.6 removed, `ruin_fade_years` removed; a ruin blocks its site circle to E | 25.1 (a) |
| (r3) P5-03 | 11.1, 11.2, invariant 10 | `ghost_fade_years` removed; 11.2 superseded; invariant 10 replaced by this spec's 14 | 25.1 (b) |
| (r3) P5-02 | 6.3.2, 8.3, 11.4 | no rule change ("ruined or faded" reads "ruined" in generated worlds); the `site` and `monster` checks of 9.4 | 25.1 (c), 9.4 |
| (r3) P5-03, P5-03a | P5-03 14.2, P5-03a 6 | the undead threshold: no rise below normal pressure; the population term scaled by `undead_regions_ref` | 25.3 |
| (r3) P3-07 | 6.6 (25.3 item 3), 24.3 items 2, 3, 5, 25.2, 24.7 | the faded counts only when non-zero; the major calamities G; ruins lists unchanged in rule | 25.1 (d), 25.2 |
| (r3) P5-02 | 12.9 | B unchanged; G inside the budget | 25.2 |
| (r3) P4-04 | 3.3, 5.3, 7, 8.2, 8.5, 8.6, 9, 10, 11 invariants 1 and 3 | old ruins as type `faded` with glyph 9; `OLD_RUIN_YEARS`; `--faded` help; the viewer's `Old ruins`; the headless D0 | 25.1 (e) |
| (r3) P5-04 | 7.16, 11.1, 11.3 | `open_site` kinds 3 and 4, P5-02's loops test kinds 1 and 2; the cause-word kinds 10, 16, 31, 43, 45 (11.1 wrote 13); the library's glyph offset beside the mine's | 7.8, 8.2, 11.4 |
| `schema.md` | 1, `figure`, `book`, `site`, 5, 6, relation kinds, entity types | 9.4 | the engineer |

## 22. Decided (Paul, 2026-11-24), and questions for the lead

**Paul's decision on r1's eight questions** (Q-P5-06-1 to Q-P5-06-8, `docs/questions-for-paul.md`), in his words: "Option C, agree with all the recommendations except 3. I think humans and dwarves might have numbers some of the time the rest probably not, 6. If the deeds are substantial it can be more than two per world."

1. **Scope: C.** Rulers, generals and heroes, and minimal casters (towers, named books, blights), in one stage; part C folded into the numbered sections (the Rules heading, 19).
2. **Rulers named by deed** (as recommended): founders, rulers under whom the realm declared war, broke faith, granted or kept spoils or fell, the rivals of a disputed succession, the ruler at the end (6.4); the rest of each line counted.
3. **Regnal numbers: humans and dwarves sometimes, the other races not** (changed from the recommendation of none). 6.14: a culture field `regnal_permille` (human 250, dwarven 200, the others 0), a keyed draw on `hist.figures.regnal`, a name reused only from an earlier recorded ruler of the same realm's line, the number the bearer's rank among the recorded bearers (never changing, the first bearer never renumbered), seceded realms beginning their own line, roman numerals II to XXXIX after the name and before the epithet, column `figure.regnal`; target FG10.
4. **Tempers lean by race, each race's average kept** (as recommended; 7.2).
5. **A conquered realm's last ruler falls with it** (as recommended; 6.4 rule 4).
6. **Epithets earned by deeds; the cap of two bearers applies to ordinary deeds only, a substantial deed gives its epithet however many bear it** (changed). 6.10: each epithet entry has an ordinary threshold `min` and a substantial threshold `substantial` on a measured deed value, `epithet_repeat_max` 2 for ordinary earnings, no cap for substantial ones; target FG11 (no epithet on more than a tenth of a world's figures).
7. **Births are never events** (as recommended).
8. **Default summary median bound 645** (as recommended; T18). (r3) 650 with Paul's riders of the same day (25), which add up to 5 lines; the figures' share stays 645.

**For the lead** (not for Paul):

1. Event kind ids 79 to 92 were provisional at r2: the next free ones at merge (P5-00 4.4). (r3) Final: 0.26.0's catalogue ends at 78.
2. FG11 and substantial epithets: no hard bound is set on substantial bearers, following Paul's words; their thresholds are calibrated to FG11 (15.2 step 11). If an epithet still exceeds the bound with its `substantial` threshold at the top of the deed's observed range, the lead rules between a stated per-world bound for substantial bearers (recommended form: `epithet_substantial_max`, starting 8) and taking the question back to Paul.

## 23. Readings taken, and the lead's rulings (2026-11-24)

Readings L1 to L8 were r1's; the lead accepted all of them on 2026-11-24, with report format v8 (10.1), browser group `figures` = 8 with `ALL_GROUPS` 0x1FF (11.3) and the tower glyph code 20 (11.4). L9 to L11 are r2's, within Paul's decisions.

- **L1. Rulers named by deed** (6.4; Paul's decision 22 item 2). Naming every reign would make rulers most of the log (about 740 reigns a world, 360 in active years); a ruler enters the record for a reason a chronicle would give.
- **L2. Ruler names in existing sentences by rendering** (7.9; accepted). `war_declared` and the other acts cannot gain a role (P3-04 8.2: a shipped kind is never edited), and a new declaration kind would change every reader of `war_declared` (claims, conflict's log reads in alliances, digest frames, relations, renaming, markers, the browser). The form `{r:under}` renders the ruler recorded at the event from the `ruled` rows, which 6.4 opens with that very event: so the name is a stored fact, not a guess.
- **L3. One new battle kind** (7.6; accepted): `battle_led` covers every shape of a led battle; readers gain one key.
- **L4. A crisis ends a reign** (6.5): P3-05's crisis stays a random pressure; it now means the ruler died and the succession was disputed; a crisis in the year of an accession is the dispute over it.
- **L5. Ties**: `ruled`, `served`, `rival`, `parent`; deeds (a slaying) stay events; an heir is a `ruled` row and, where known, a `parent` row.
- **L6. Casters cause, do not end, a calamity** (6.8; accepted): a caster may cause a blight and never ends a calamity; ending a plague or a monster from here would change another subsystem's state; left to P5-09.
- **L7. Outlaws** wait for factions (bandit bands); exiles are this stage's (accepted: outlaws deferred).
- **L8. Books are rows** (9.3; accepted: the `book` table), so a book has a name the world stores and P6 and P5-09 can place and extend; titles come from culture data, unique in the registry.
- **`draw_at`** (7.10; accepted as an amendment to P3-04 6.4; the note is in `docs/specs/p3-04-simulation-engine.md`).
- **L9. Regnal numbers by culture, from recorded rulers only** (6.14). The chance is a culture field (naming is culture's, P3-02 3). No name is stored for an unrecorded reign: since rulers are recorded only while their reign is current, recorded rulers of a line are created in reign order, and the number counted at creation is final without a private name per reign. The numbered form is registered so that P3-02's numbered fallback cannot collide with it.
- **L10. A seceded realm begins its own line** (6.14): its claimant founded it by losing the parent's throne, and numbers stay checkable from one realm's rows.
- **L11. Epithets are decided at the end** (6.10), in closing order, so a deed measured over a realm's life (`founded_years`) or a reign's length is known; no sentence is rendered before rows are written, so nothing changes for the reader.

r3's readings, within Paul's riders and the lead's brief (for the lead to rule at the r3 review; none needs Paul):

- **L12. Old ruins reuse the faded marker** (25.1 (e)): the lead's proposal that Paul agreed with (Q-RUINS-1: "the viewer's 'Faded ruins' switch reused for ruins older than a stated age"); the age is the old `ruin_fade_years`, 1500, as a markers constant, so the map looks as at 0.26.0 by default while every ruin stays in the world and in the reports.
- **L13. Status 4 and kind 19 stay defined** (25.1): a code or a kind is never removed (P3-04 8.2), and fixtures keep exercising faded rows; no reader refuses them.
- **L14. A major calamity by realm share or by an ended settlement** (25.2): Paul's "if the impact is significant". The realm's share is what a chronicle would weigh; a settlement ended by it is a loss the world keeps as a ruin. Measured on 0.26.0's stored worlds (12 default, 8 `civs4` generated for this revision): at 100 per mille or an ended settlement, 3 to 9 a default world and 21 to 29 a `civs4` world, hence the cap of 8.
- **L15. Two undead rules** (25.3): data alone cannot raise risings under `low` without raising them at `normal` (the threshold is doubled and the chance halved there), nor at 2048 without moving 1024 (the population term grows with the map's area while a battle's dead do not). Both rules leave every `normal` and `high` world of 1024 and 512 unchanged.
- **L16. One library per city** (6.8): r2 let a second caster of a city raise a second library at the same settlement cell.

## 24. Scope decided by Paul, and proposals

**Paul, 2026-11-24** (Q-P5-00-1, `docs/questions-for-paul.md`): "take the recommendation except for mountain depths". So the rest of P5 is: P5-06 Figures, with a minimal form of casters folded in (this spec; the full P5-09 is deferred); P5-07 Mountain depths (kept, small, on P5-04's mines); P5-08 Pantheon and interventions, with cults folded in (the full P5-10 is deferred); P5-11 Artifacts. P5-05 Roads and improved passes go to P6. P5-09's spell invention and P5-10's factions are deferred. P5-00 3.1 carries the note.

Proposals entries in `docs/design/proposals.md`: "History depth: the rest of P5 after Paul's scope decision" (decided, Paul 2026-11-24); "History: figures, P5-06 (readings and scope)" (decided, Paul 2026-11-24, 22); "Schema 12: figures and books (P5-06)" (decided, Paul 2026-11-24: schema 12 with `figure`, its column `regnal`, and `book`).

## 25. Paul's riders of 2026-11-24 (r3)

Two decisions of Paul's ride with this stage, the next one to change world bytes and the report (`docs/questions-for-paul.md` Q-RUINS-1 and Q-P5-01-1; `docs/design/proposals.md`, "Paul's answers of 2026-11-24 to the open questions"): "every ruin, sacked ones included, stays to the end of the history", with the lead's proposal of a summary listing at most 15 and the viewer's faded switch reused for old ruins; and "we should record these [famines and plagues in a large world's summary] if the impact is significant". 25.3 is the recalibration that goes with them. Each item names the spec it amends; section 21 lists them.

### 25.1 Every ruin stays (B1)

**Rule.** No settlement fades. A ruined settlement (status 3) keeps its status, `ruined_year`, `ruin_event` and `ending` (P5-02 8.3) to E, whatever ruined it (sacked, overrun, plague, famine, left, fled) and whatever stands on it. The 0.27.0 generator writes no `settlement` or `settlement_census` row with status 4 and no event of kind 19 `ruin_faded` (invariant 14). Status code 4 `faded` and kind 19 stay defined (L13): readers accept them, the catalogue keeps kind 19 with its template and salience, and hand-made fixtures with a faded row (P3-07's H1, P4-01's T1, P4-04's T2) keep their meaning and their goldens' fixture lines.

**Amendments**, by owning spec:

(a) **P3-05** (peoples):
- 8.6 *Ruins fade (Q3)* is **removed**. Peoples' Q3 does nothing (fading was its only action; `PeoplesSubsystem::fade_ruins` goes; whether `steps()` keeps Q3 is the engineer's, with no effect on output).
- 12.1: `ruin_fade_years` is **removed** from `history.peoples` (the loader's field list, `PeoplesTuning`, `game/data/history/peoples.toml` and both fixture roots, `worldgen/tests/data/geo/` and `geo-small/`); a data root that still has it is refused as for any unknown key (`unknown key 'history.peoples.ruin_fade_years'; expected …`, the loader's form).
- 6.2 (sites): text unchanged; its "faded settlements do not block" now never applies in a generated world, so a ruin keeps blocking its `site_spacing` circle to E. Expected effect: about 2 cells' circles more blocked a default world at E (0.26.0's median of faded ruins), about 16 in `civs4`; G2 is checked (15.1).
- Invariants 2 and 7: their faded clauses apply to fixtures only. Test 13 ("ruins fade at exactly `ruin_fade_years`") is replaced by 18.1 item 20.

(b) **P5-03** (race relations): 11.2 *Ruins that do not fade* is **superseded** (every ruin now stays, not only those left standing or holding a site). `ghost_fade_years` (11.1's `history.peoples` field, shipped 0) is **removed** as `ruin_fade_years` above. Invariant 10 is replaced by this spec's invariant 14. `WildView::holds_site` (P5-03 11.2 (b), implemented by `MonstersSubsystem`) loses its only reader; the engineer may remove it or keep it for P6 (no output effect; 7.8's kind test applies if kept).

(c) **P5-02** (monsters), no rule change, stated because these rules depend on ruins existing:
- 6.3.2's lair at a ruin: a host rises at a sacked settlement "ruined or faded"; in a 0.27.0 world that is "ruined", and every sacked ruin stays a possible lair for the rest of history (at 0.26.0 a faded one was too, so the rule's reach is the same).
- The overrun (P5-02 6.4.3), `holds_settlement`, a host that overruns once (Paul, 2026-11-23), the peoples' ghost rule (`ghost_permille`, `ghost_max_population`: a dwindled place left as a ghost ruin) and the endings of 8.3: unchanged. Ghost ruins never faded since P5-03; now no ruin does.
- 11.4's `settlement.ending` check for a faded row (derived from its `ruin_faded`'s cause) stays for fixtures.

(d) **P3-07** (the history report):
- `[present]`'s settlements line `settlements: <rows>; thriving <n1>, declining <n2>, ruined <n3>, faded <n4>` prints `, faded <n4>` only when n4 > 0; the civilization block's `  settlements: <f> founded, <h> held at the end, <r> ruined, <d> faded` (P3-07 25.3 item 3) prints `, <d> faded` only when d > 0. Fixture reports with a faded row are unchanged; a 0.27.0 world's lose `, faded 0` / `, 0 faded`.
- The ruins list (P3-07 24.7 and 25.3 item 1): unchanged in rule. Full lists every status 3 settlement; summary lists at most 15 by P5-03a 9.3 (the union of the 5 oldest, 5 largest and 5 most recent, then `  and <x> more`). With every ruin kept, a default world ends with a median of about 19 ruins (0.26.0: 17 ruined, 2 faded) and a `civs4` world about 69 (53, 16): the summary list stays at its cap; the full one grows by the formerly faded.
- `[stats]`: `settlements_faded`, `events_ruin_faded` and `ruins_oldest_year` stay (the first two 0). The chronicle loses every `ruin_faded` line (salience 10: none was in a summary).

(e) **P4-04** (markers; library, CLI and viewer):
- **Old ruins.** New constant `OLD_RUIN_YEARS` = **1500** in `worldgen/inspect/markers.hpp` (the old `ruin_fade_years`; L12). In `markers_at(Y)` (P4-04 3.1, 3.3), a settlement with status 3 at Y whose ruined year R(s) satisfies Y − R(s) ≥ `OLD_RUIN_YEARS` is an entry of type `faded` (an **old ruin**), with R(s) and its ruin event as a ruin's; a settlement with status 4 at Y is of type `faded` as before. Every `faded` entry is drawn with glyph 9 (P4-04 5.3 as amended by P4-05 12.1) and only with `MarkerOptions::faded`, in draw position (3) of P4-04 6.1, as now. So by default the map hides ruins older than 1,500 years, as it hid faded ones at 0.26.0, and the option shows them.
- Invariants (P4-04 12): 1's "minus status 4 without `faded`" reads "minus the entries of type `faded` without the option"; 3 becomes "a settlement's type is `ruin` from R(s) until R(s) + `OLD_RUIN_YEARS` − 1 (or the year before its `ruin_faded` event, for a fixture's faded row), then `faded`; it is never `standing` again"; 5 unchanged.
- CLI (P4-04 7): `render --help`'s line becomes `  --faded             with --markers: also old ruins (1500 years or more) and faded ruins`; the option and the file names are unchanged.
- Viewer (P4-04 8.2, 8.5, 8.6; the viewer engineer): the Markers group's `Faded ruins` box becomes **`Old ruins`** (same option `faded`, default off); the key row `faded ruin` becomes `old ruin` (glyph `faded`); hover (P4-04 8.5) of a `faded` entry: `<name>, old ruin of a <kind word>, ruined in <R>[: <sentence>]` for status 3, and as before (`faded ruin`) for status 4.
- Headless check (P4-04 9.1, 9.2): D0 becomes R1 + `OLD_RUIN_YEARS`, with R1 the smallest `ruined <year>` of the `[present]` settlement lines such that R1 + 1500 ≤ E; the world content assert requires such a ruin (in place of a `ruin_faded` chronicle line) and asserts that the chronicle has no `ruin_faded` line; LM keeps D0 − 1 and D0; `markers-faded-<Y>.png` at E and D0 as before. M1: the key list without `old ruin`. M2: with the option off, the entries are the `[present]` lines less the old ruins at E. M3's second part: for the settlement of R1, at D0 − 1 its entry is `ruin`, at D0 absent with the option off and `faded` with it on. M8: the option adds exactly the old ruins of Y.
- Goldens: `p4-04-markers.txt`'s year list (P4-04 10.3 item 2) gains the years R(s) + `OLD_RUIN_YEARS` ≤ E of every ruin; `markers.txt` of `world-hashes.sh` changes (its command, with `--faded`, stays).

(f) **P4-01** (timeline): no change (a settlement's status comes from its census rows). **P4-05**: kind 19 stays in `settlements`. **P5-12**: none (no renaming rule reads a fading). **P5-04**: 6.12's note "a mine does not hold back a ruin's fading" is moot.

### 25.2 Major calamities in the summary (B2)

Report text only (no world byte). Amends P3-07 24.3 items 2, 3 and 5 and 25.2, and P5-02 12.9 (by stating that B is unchanged).

**Definitions** (integers; every value read from the world):

```
kinds      famine, plague, plague_outbreak            // P3-07 25.2's calamity kinds without succession_crisis
dead(e)    = value total of a plague_outbreak; value dead of a famine or plague
c(e)       = the event's first participant of entity type 3 (role civilization), 0 when none
P(e)       = Σ over settlements s of population(r_s), r_s = s's settlement_census row of the largest year < year(e),
             taken only when r_s.owner_civ = c(e) ≠ 0 and r_s.status ∈ {1, 2}        // the realm's people before the blow
I(e)       = dead(e) × 1000 div P(e) when P(e) > 0, else 0                            // per mille of the realm lost
ended(e)   = 1 when e is among the causes (event_cause) of the ruin_event of some settlement with status 3, else 0
major(e)   = ended(e) = 1 or I(e) ≥ SUMMARY_CALAMITY_MAJOR_PERMILLE
```

(The census row of the event's own year is left out: it may already hold the blow.) **G** = the first `SUMMARY_CALAMITY_MAJOR_MAX` major calamities among the candidates of 24.3 item 1, ranked by (ended descending, I descending, id ascending). New constants of section 8 (`digest.hpp`): `SUMMARY_CALAMITY_MAJOR_PERMILLE` = **100**, `SUMMARY_CALAMITY_MAJOR_MAX` = **8** (starting values; CB1, CB2 calibrate them, 15.2 step 12).

**Rule.**
1. **G is in the summary set always**, outside the threshold, as F is (24.3 item 2): the floor set starts as F ∪ the units U(e) of G's members (item 3), before the age walks.
2. **The calamity cap** (25.2): in each phase p, the members of G with `event.phase` = p stay candidates; of the other calamity candidates of p (the four kinds of 25.2), ranked as now, the first max(0, `SUMMARY_CALAMITY_PER_PHASE` − |G_p|) stay candidates. So a major calamity takes a phase's slot before a lesser one, and three majors in one phase are all shown.
3. **The budget** is unchanged: B of P5-02 12.9 (O counts no line of G, which is inside `[chronicle]`), T the smallest t with |floor ∪ C(t)| ≤ B. 24.3 item 5's bound on the floor grows by at most 8 units (a unit has at most 8 events: 64): with six ages and fewer than 15 quiet stretches it stays below 250 (15 + 6 × 27 + 64 = 241). As before, a world whose floor alone exceeds B gets T = 100 and may exceed L(C); none did in the measured sweeps, and T18 checks it.
4. `full` prints every event as before; its `[stats]` `summary_threshold`, `summary_events` and the new `summary_major_calamities` (10.5) are those of the set chosen with G.

**Expected** (prototype on 0.26.0's stored worlds, `docs/design/proposals.md` P5-06 r3 entry): major calamities 3 to 9 a default world (12 worlds; median about 6), 21 to 29 a `civs4` world (8 worlds), so |G| is the cap there; of them, ended settlements 0 to 3 (default) and 3 to 19 (`civs4`). In a default world (threshold 31 at the median, famines at salience 25 below it) G brings the significant famines into the chronicle and displaces the weakest events at the threshold; in `civs4` and at 2048 (threshold about 76, no calamity shown at 0.26.0) it brings in up to eight. Lines: B binds in most worlds, so the summary does not lengthen there; CB2 bounds the rest (+5 at the default median).

### 25.3 Undead and monster recalibration (B3)

With every ruin kept, P5-02's M4 and Paul's bound on lairs on former settlements are to be met again, and the undead are to rise where they were almost absent without crowding the end. Measured at 0.26.0 (validation's sweeps, `~/.cache/the-unmapped/out/p5-04-validation/sweep/`): a living monster at E in 77 and 81 of the two default sets; a lair on a former settlement open at E in 58 and 55; undead risings in 1 of 30 `low` worlds and 4 of 10 worlds at 2048 (median 0 a world), in 78 and 84 default worlds (median 1.5 and 2), 45% and 56% of the default risings in the history's last 500 years. Heroes (6.7) slay monsters that armies did not, which works against M4a. The targets are M4a, M4b, UD1 to UD3 of 15.1, with C1, T16, G2, AL1 and T18 holding; the levers are 15.2 steps 5a and 5b.

**Two rules** (amend P5-03 14.2 and P5-03a 6; L15). With P_W, M (`pressure`), R (region rows) as there:

```
Tpop = P_W × undead_dead_permille div 1000
Tpop = Tpop × undead_regions_ref div max(undead_regions_ref, R)      // only when undead_regions_ref > 0
T    = max(undead_min_dead, Tpop)
a region qualifies when S(r) ≥ T × 1000 div max(1000, M)            // was: div max(1, M)
```

- **Low pressure** (`max(1000, M)`): pressure above normal lowers the slaughter needed (`high`, M = 1600: unchanged from 0.26.0); pressure below normal no longer raises it, so `low` keeps only its halved chance (`wake_rate` × M div 1000). `normal` is unchanged. With the `zero` tuning (M = 0) the chance is 0, as before.
- **Large maps** (`undead_regions_ref`): new field of `history.monsters`, int(0, 100000), required, appended after `undead_dead_permille`; shipped **64**, both fixture roots and P5-02's `zero` fixture as shipped; 0 leaves the factor out (0.26.0's rule). A world with at most 64 regions (every 512 and 1024 world of the sweeps: 11 to 59) is unchanged; at 2048 (191 to 204 regions) the population term is about a third, close to a 1024 world's per region, since a battle's dead do not grow with the map.

Both rules change no draw and no key. Everything else of the rising (the window, the lair at a sacked ruin, the causes) is unchanged.

## Revision notes

- r1 (2026-11-24): first draft, for Paul's approval.
- r2 (2026-11-24): approved by Paul with his decisions (22). Scope C: part C folded into the numbered sections (the Rules heading, 8.1, 19). Regnal numbers for humans and dwarves (6.14; culture field `regnal_permille`, tuning `regnal_max`, stream `hist.figures.regnal`, column `figure.regnal`, reader checks, invariant 12, FG10, tests 18.1 item 17). Epithets with ordinary and substantial thresholds (6.10 rewritten: decided at E in closing order; deeds as measured values, `reign_long` and `founder` and `slew` replaced by `reign_permille`, `founded_years` and `slew_might`, r1's `reign_long` of two generations being unreachable; `epithets` gains `substantial`; check `figures_epithet_thresholds`; invariant 13, FG11, `[stats]` keys). The tower glyph's grid (11.4). FG9's display bound 73 → 81. Section 22 decided; 23 the lead's rulings; the `draw_at` note added to P3-04. Nothing else changed.
- r3 (2026-11-25): no decision of r2 reopened. (A) P5-04 as built (0.26.0, 52fd4f3): event ids 79 to 92 final (8.1, 22); 97 streams, the list's name (3); conflict's and monsters' reads as built (3); the exile's reach defined here, P5-04's breadth-first reach being private to alliances (6.5); figures and alliances point by point (6.15, new); generals only at a `war_declared` (6.6); one library per city (6.8, L16); `open_site` kinds 3 and 4 with its message, P5-02's lair and `holds_site` tests from `kind != 3` to kinds 1 and 2 (7.8); the cause-word kinds 10, 16, 31, 43, 45 and `blight=blight` (8.2, 11.1); browser group as built (11.3); the library's glyph to the upper left of its city, the mine's to the upper right, a wild tower at its cell; the key row after `mine` with its colour; 25 glyphs in the dump (11.4); shard pattern and test file names (17); the `civs4` bound 1,050 (15.1). (B) Paul's riders (25, new; 2, 10.5, 10.6, 13 items 14 to 16, 15, 16, 17, 18, 21, 23 L12 to L15): every ruin stays (25.1); major calamities in the summary (25.2); two undead rules and the monster recalibration targets M4a, M4b, UD1 to UD3, RU1, CB1, CB2 (25.3, 15); T18's default bound 650. (C) Every new stream with its key (14); the reader checks as tables with their messages and where they run (9.4); default and slow cases (18). The schema text of 9.4 is unchanged; status 4 is kept and no longer written (9.4).
