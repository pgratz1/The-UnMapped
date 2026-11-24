# P5-03a Relations follow-ups

Status: **r1 (draft, 2026-11-24), for the lead's approval.** Written by the spec writer on `p5/03a-spec`, cut from `main` at 1dd8f89 (generator 0.24.0, schema 10, history report format v5; P5-03 merged). Phase P5 History depth, stage 03a: the follow-ups of P5-03 (`docs/specs/p5-03-race-relations.md` r2), from the world critic's review (`docs/reviews/p5-03-race-relations-world-critic.md`, F1 to F10), the validation (`docs/reviews/p5-03-race-relations-validation.md`, section 6), the audit (`docs/reviews/p5-03-race-relations-audit.md`, A1, A2) and Paul's direction of 2026-11-24 (friendlier relations more often, "more alliances forged"; alliances themselves are P5-04's). Generator **0.25.0**; **schema stays 10**; history report format **v6**.

**No schema change.** No table, column or event kind is added. The change:
- adds rules to `RelationsSubsystem` (three new memory sources, meetings by word, graded settlers, an expulsion gap, the ruling share kept) and one keyed stream (`hist.relations.settlers`);
- appends eleven fields to `history.relations` and one to `history.monsters`, and changes four shipped values (data);
- appends values to three existing event kinds (catalogue rows of `event_kind_value`, written by the generator; P5-01 appended whole kinds at schema 7 the same way) and rewrites eight templates (data);
- adds three renderer forms for monster roles (P3-04 8.4);
- changes the undead threshold (P5-02 6.3.2 as amended by P5-03 14.2);
- rewrites parts of the history report as format v6.

The form is that of `docs/specs/p5-01-followups.md`, except that the normative text is here (P5-03 r3 gets a pointer). Figures marked *measured* were taken by the spec writer from validation's stored 0.24.0 sweep (`~/.cache/the-unmapped/out/p5-03-validation/sweep/`, 100 + 100 default reports, 12 default world files); every other figure is the review's or is for validation.

## 1. Items

| # | Item (source) | In short | Section | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Relations that move (F1; Paul) | Three sources of good memory that occur (long peace between neighbours, a common enemy, shelter worth more); a turn's causes by its direction; a public `credit` seam for P5-04 | 3 | yes |
| 2 | Isolated races (F2, F10; RR1, RR3) | Peoples sealed apart by closed borders meet **by word** after 300 years within three borders; the summary names unmet pairs and a race that met no one | 4, 9.4 | yes |
| 3 | Mixed settlements and expulsions (F3, F4) | Settlers drawn in a range (shares no longer at 900); the mixed sentence by standing and majority; the expulsion sentence with its standing and years; one expulsion per realm in 60 years; at most three mixed lines in the summary | 5, 9.5 | yes |
| 4 | Summary length (F6, F7; T18) | Summary milestones become a count (quiet stretches kept); no "without war" line; at most 15 ruins listed | 9.2, 9.3 | report |
| 5 | Late undead (F5) | Threshold relative to the world's population | 6 | yes |
| 6 | Wording (F8.1, F8.2, F8.3, F8.5, F8.7, F8.8, F10) | `{monster:its}`, `{monster:aside}`, `{monster:where}`; refugees' causes; `[peoples]` order, plurals, "from" | 7, 9.4 | yes (templates) |
| 7 | Audit A1, A2 | The ruling race keeps a share; dispositions valid before the first step | 8 | A1 yes (rarely), A2 no |
| 8 | Test-side items | Five reference and comment fixes; a `p5-monsters-high` case | 12 | no |

Not here, with reasons: first meetings repeated in one year (critic's answer 1; a summary grouping, a later report round); "again" on chained raids (F8.4; needs a value on kind 48, and the chain is already in the causes); refugees mostly among kin (F8.6; the kin preference is the shelter rule, and section 3 makes other peoples' shelter count more); monster names and epithets (F9; rare, the critic says ignore); tribute "paid faithfully" as goodwill (the brief's suggestion): a tribute is war's last-resort cost in people carried off (P3-06 20.3), a grievance, not a kindness; P5-04's vassalage is where paying brings favour.

## 2. Inputs, streams, draws

As P5-03 3, plus: `HistoryInputs::region_borders` (every row, whatever its walk pairs or pass) read by `RelationsSubsystem` for 4.1's graph; `history.relations` fields of 10.1. **Stream** appended to `streams.hpp` after `hist.relations.expel`: `hist.relations.settlers`, key (settlement, new owner civilization, 0), 5.1. Every other rule of this stage is deterministic (ids, orders).

# Part 1. Dispositions

## 3. Relations that move (F1)

### 3.1 Diagnosis

Measured on the 100 default reports at 0.24.0: memory moves only by war's events (P5-03 6.2.2) and by shelter; wars are late (1,238 of 2,568 declarations after 5500, *measured*, because the orcs arrive about 5550 and the humans about 5000); 488 of 1,431 meetings are after 5500 (*measured*); shelter almost never moves a pair (82 of 105 arrivals the critic read are among kin, P5-03 6.4.3's kin order, and a kin arrival is no pair). Of 127 turns read by pair (*measured*): goblin–dwarf to hatred 60, goblin–human 32, human–orc 20, dwarf–orc 4, goblin–orc to wariness 4, goblin–dwarf to wariness 4, dwarf–human to friendship 3. So every pair's band is its base band (P5-03 21's paragraph) until a war happens to it, and nothing warms a pair. Common enemies as the brief defines them are rare: two wars of one realm against two other peoples open in the same year, 12 world files: 11 credits; with a window of 100 years, 60 credits (5 a world, 11 before 5500); a monster raiding two peoples within 100 years, 18 (7 before 5500) (*measured*). The only source that acts early and often is **peace between neighbours**, which every pair in contact has from its meeting on.

### 3.2 Long peace between neighbours (new; T6 and Q4, after decay, before turns)

For each met pair (a, b) ascending:

- **Contact**: the pair is in contact when some region of P_a and some region of P_b lie within `contact_hops` of each other on the **border graph** of 4.1 (every border, closed or open), where P_r is the union, ascending, of `presence(c)` (P3-06 5.1's simple definition, as P5-03 6.2.4) over the alive, visible civilizations c of race r. Computed once per step: a per-region bit set of the races present, then for each region of P_a its region and its neighbours within `contact_hops`.
- **Strife**: the pair's `strife_year` = its met year at the meeting; at every T6 and Q4, when an open `war` relation joins a civilization of race a and one of race b (either order), `strife_year` = Y.
- **Gain**: when in contact, no such war is open, and Y − `strife_year` ≥ `peace_years`: g = years × `peace_memory_per_century` × 1000 div 100 (T6: years 1; Q4: L); cap = `peace_memory_max` × 1000; if M < cap: M = min(cap, M + g). No contribution is recorded (3.5).

So peace lifts a grudge at `peace_memory_per_century` plus the decay (both act toward warmth while M < 0) and builds goodwill at `peace_memory_per_century` − `memory_decay_per_century` while 0 ≤ M < cap, never past the cap by itself. With the shipped values (60, cap 300; decay 20) a good–neutral pair (base −150) at peace in contact reaches D = 150 = `friend_min` after about 100 + 750 years; a neutral–neutral pair (100) in about 230; a neutral–evil pair (−250) rises to +50 and stays wary; a good–evil pair (−700) can leave hatred (D > −450 needs M > 250) only after about 725 years of unbroken peace, the "way back" the critic asks for. Every war between them resets the clock and costs `war_memory`.

### 3.3 A common enemy (new; T6 and Q4, after the log)

**In war.** The subsystem keeps, per civilization z, per race r ≠ race(z), the last year Y_zr at which an open `war` relation joined z and a civilization of race r, and that relation's opening `event` (the `war_declared`; `RelationState::event`). At every T6 and Q4, first update every entry from the open `war` relations (ascending key; a relation with two such civilizations updates both sides' entries), then for each z ascending, each pair of its entries r1 < r2 with Y − Y_zr1 < `foe_years` and Y − Y_zr2 < `foe_years`, at least one of them = Y, the pair (r1, r2) met, and (r1, r2, z) not yet credited: `credit(r1, r2, foe_war_memory, e)` with e the larger of the two opening events; (r1, r2, z) is credited (once per pair and enemy realm, ever).

**A monster.** While reading the log (P5-03 6.2.2), for each `monster_raid` or `settlement_overrun` e (participants `monster` m, `civilization` c, r = race(c)): for each race r' ≠ r, ascending, with an entry (m, r') whose year y' satisfies Y − y' < `foe_years`, the pair met and (pair, m) not yet credited: `credit(r, r', foe_monster_memory, e)`, (pair, m) credited; then the entry (m, r) = (year(e), e).

State added: the per-(z, r) and per-(m, r) entries (ids and years only) and the credited sets (ordered sets of id triples). Bounds: entries ≤ (civilizations + monsters) × races.

### 3.4 Shelter

Unchanged in rule; `shelter_memory` 45 → **90** (10.1). With 3.2 and 3.3 shelter is a third, smaller source; doubling it makes one arrival among another people worth more than a capture.

### 3.5 `credit`, contributions and a turn's causes (amends P5-03 6.2.2, 6.2.3)

`void RelationsSubsystem::credit(std::int64_t race_a, std::int64_t race_b, std::int64_t units, std::int64_t event)`: when the pair has met: M += units × 1000 (units may be negative), clamped to [`memory_min`, `memory_max`] × 1000; the contribution (event, sign of units) is appended to the pair's list. Every memory change of P5-03 6.2.2's table and of 3.3 goes through it (war events with −Δ, shelter with +`shelter_memory`); 3.2's gain and the decay do not. **It is the seam for P5-04**: trade kept, an ally's help, a broken alliance call it with their own events and data values; nothing else is specified for P5-04 here (13).

Contributions keep the last 8 of **each sign**. At a turn the causes are: a turn to a higher band (warmer), the positive contributions; to a lower band, the negative ones; both lists are cleared at the turn. When the turn's own list is empty: a warmer turn cites the pair's `peoples_met` event (peace did it); a colder turn cites the pair's last turn event, else its `peoples_met`. P5-03 r2's rule (the last turn, else the meeting, for every turn without contributions) is replaced.

`disposition_turned`'s template (10.2) gains the words `war_declared=wars against a common enemy`, `monster_raid=the raids of a common foe`, `settlement_overrun=the raids of a common foe`, `peoples_met=long years of peace`, `disposition_turned=long years apart`. Samples: `After long years of peace, the Elves and the Dwarves became friends.` · `After wars against a common enemy, the Humans and the Dwarves became friends.` · `After long years of peace, the Elves and the Goblins grew wary of one another.` · `After long years apart, the Goblins and the Orcs grew wary of one another.`

### 3.6 Order in T6 and Q4 (amends P5-03 6.1)

shares of new settlements; pending moves; the log (with 3.3's monster credits); owner changes; expulsions (T6); pending displacements; **war foes (3.3)**; decay; **peace (3.2)**; turns; meetings (4.2); mixed.

# Part 2. Meetings

## 4. Peoples sealed apart (F2)

### 4.1 Diagnosis and the border graph

Measured on the 12 stored default worlds (*measured*): in seeds 4 and 6 the dwarves' realm lies **one border** from the goblins and two or three from every other people, but every one of those borders has no walkable pair and no pass, so P3-05's open-border graph (`PeoplesView::hops`, `border_cost`) never reaches it and `meet_hops` (8) cannot help. The dwarves are sealed in an upland by closed borders, not far away. The **border graph** of this stage: regions as nodes, one edge per `region_border` row (a, b), whatever its walk pairs or pass; hops by breadth-first search from a region, neighbours in ascending id, cached per source region (as `PeoplesGeo::hops`).

### 4.2 Meeting by word (amends P5-03 6.2.4)

For each pair (a, b) ascending not met, both awakened, for which 6.2.4's open meeting does not happen this step: take the smallest key (h, R_x, R_y, x, y) of 6.2.4 with h the border-graph hops. If h ≤ `meet_sealed_hops`: when the pair's `word_since` is 0, it is set to Y; when Y − `word_since` ≥ `meet_sealed_years`, the pair meets as in 6.2.4 (the younger's region in the key, its founding event as cause), with the new value `sealed` = 1. If h > `meet_sealed_hops` (or no key), `word_since` = 0. Every meeting of 6.2.4 carries `sealed` = 0. `meet_sealed_hops` −1: never. In Q4 it needs `quiet_events_left(0)` > 0, else it waits.

A pair met by word has a disposition, bands, turns and a common enemy like any other; it meets no army (conflict's reach is the open graph, unchanged) and no refugees (`migrate_hops` is open hops, unchanged). With the shipped 3 and 300, both sealed dwarf realms of seeds 4 and 6 meet every other people (the farthest is three borders).

**Kind 57** gains the value `sealed` (seq after `band`). Template: `The {race_a:plural} {sealed?first had word of|first met} the {race_b:plural} in {@region}{band?, and hated them from the first|, and were wary of them|, and were glad of them}.` Sample: `The Dwarves first had word of the Goblins in the Grey Fells, and were wary of them.`

# Part 3. Mixed settlements and expulsions

## 5. Shares and expulsions (F3, F4)

### 5.1 Graded settlers (amends P5-03 6.3.3 item 2)

Measured: 1,395 of 1,855 `settlement_mixed` events (75%) have `share` 900 (*measured*), because the event is told in the step of the conquest, when the conquered hold exactly 1000 − `conquest_settlers_permille`. Item 2's settlers become, with σ = `conquest_settlers_permille` + pick(D(hist.relations.settlers; s, c, 0), `conquest_settlers_span_permille` + 1), c the new owner: the ruling share = x + (1000 − x) × σ div 1000. Shipped 50 and 400: the conquerors are 5% to 45% of a taken town. The draw is made only when q ≠ q_s (a change of people).

### 5.2 The mixed sentence (amends P5-03 8.2)

**Kind 60** gains the values `standing` (the entry's standing at the event, 0 to 2) and `majority` (1 when the entry's share is above the ruling race's, else 0), seq after `share`. Template:

`{@causes:settlement_captured=the conquest,settlements_ceded=the cession,region_ceded=the cession,sacked=the sack,settlement_overrun=the ravage,refugees_arrived=the coming of refugees,refugees_settled=the flight,settlement_abandoned_to=the abandonment of a neighbour|After |, }in {settlement} the {ruling:plural} {standing?ruled over|tolerated|lived as equals with} the {race:plural}{majority?, who outnumbered them|}.`

Samples: `After the cession, in Fievos the Goblins ruled over the Elves, who outnumbered them.` · `After the coming of refugees, in Kazad Rune the Dwarves tolerated the Humans.` · `In Lece the Elves lived as equals with the Faeries.`

### 5.3 Expulsions (amends P5-03 6.5, 8.2)

- **Gap**: a realm o expels only when its last expulsion is at least `expel_gap_years` (shipped 60, two generations) before Y. The test is made before the draw, so no draw is made in the gap.
- **Kind 62** gains the values `standing` (the entry's standing, 0 or 1) and `years` (Y − the entry's `since_year`), seq after `people`. Template: `Hating the {race:plural}, {civilization} drove them out of {settlement}, where it had {standing?held them as subjects|tolerated them|tolerated them} for {years} {years?year|years}; {people} left.` Sample: `Hating the Elves, the Drunogg Horde drove them out of Thiefann, where it had held them as subjects for 2,002 years; 268 left.` The causes are unchanged (the subjection and the turn into hatred).

`years` ≥ 0; a value of 0 reads "for 0 years" and needs an entry whose standing changed this year, which the standing rule (phase starts) and the expulsion (T6) never give together except at a phase start in T1 followed by T6 of the same year: then `years` is 0. Accepted (rare; reads as true).

# Part 4. Monsters

## 6. Undead late in history (F5)

**Diagnosis.** 142 of 191 risings (74%) after year 5500. A rising needs 4,000 violent dead in one region within 100 years (P5-02 6.3.2, scaled by pressure, P5-03 14.2). Two causes: wars are late (48% of declarations after 5500, 3.1), and the dead of a battle or a sack scale with population, which grows three to five times from year 3000 to E (world population at 3000 / 5999: seed 0 28,153 / 174,263; seed 1 77,943 / 185,903; seed 3 54,910 / 259,349, *measured*). The first cause is the history's shape and right; the second makes an early slaughter of the same weight count for less.

**Rule** (amends P5-02 6.3.2, P5-03 14.2): with P_W the summed population of the standing settlements of `State` when the rising is tested, a region qualifies when

```
T   = max(undead_min_dead, P_W × undead_dead_permille div 1000)
S(r) ≥ T × 1000 div max(1, M)
```

`undead_min_dead` becomes the floor, shipped **1000** (was 4000); `undead_dead_permille` new, shipped **25** (at a world of 160,000 the threshold is 4,000, as now; at 40,000, 1,000). So the threshold follows the size of the world: a slaughter that kills one fortieth of the world's people in one region wakes the dead in any age. Everything else of the rising is unchanged.

## 7. Wording (F8)

### 7.1 Three forms of a monster role (amends P3-04 8.4, 8.5)

For a role of entity type monster (as `raid`, P5-03 9.5; `template_problem`: `form '<f>' needs a monster role at byte <n>`):

| Form | Renders |
| --- | --- |
| `{m:its}` | `its` when the monster's kind has `grouped` 0, `their` when 1 |
| `{m:aside}` | for a kind with `wake` = `wild`: `, ` + the kind's `noun` + `,`; for `wake` = `dead`: empty |
| `{m:where}` | ` in ` + the event region's text and link exactly as `{@region}`; empty when the kind has `wake` = `dead` and the monster's stored name ends with ` of ` + R, R the region's name in force at the event's year with a leading `The ` written `the ` (P5-03 12.2's `{place}` rule) |

### 7.2 Templates (data, `history.report`)

| Kind | Template |
| --- | --- |
| `monster_appeared` | `{monster}, {monster:kind}, made {monster:its} lair in {@region}.` |
| `settlement_overrun` | `{monster} drove the people from {@settlement} and made it {monster:its} lair; {survivors?no one escaped|one escaped|# escaped}.` |
| `monster_driven_off` | `{civilization} drove {monster} back to {monster:its} lair{monster:where}.` |
| `monster_slain` | `{slayer} hunted down and slew {monster}{monster:aside}{monster:where}.` |
| `monster_died` | `{monster}{monster:aside} died of great age{monster:where}.` |
| `undead_rose` | `{@causes:battle=the slaughter,battle_at_pass=the slaughter,sacked=the sack|After |, }{monster} rose from {monster:its} graves{monster:where}.` |

Samples: `The Screamers of Nux-Nink, a brood of wyverns, made their lair in Krozigg.` · `The Kingdom of Cear hunted down and slew the Dead of the Red Heath.` · `After the sack, the Dead of Aladdald rose from their graves in the Sallow Woods.` (F8.3: one name for the place in each sentence) · `Saelith the Black, a dragon of the old world, died of great age in the Cold Peaks.`

### 7.3 Refugees' causes (F8.5; amends P5-03 6.4.3 item 4, 8.2)

Measured causes of the 698 arrivals (*measured*): `people_expelled` 328, `sacked` 303, `settlement_overrun` 39, `settlement_abandoned` 27 (no word: the critic's plague case), `region_ceded` 1 (no word). Rule: an arrival's causes are the record's cause and, when that cause is an abandonment (kind 16 or 31), also that abandonment's own causes (ascending, without repeats). Template words added to `refugees_arrived`: `settlement_abandoned=the abandonment of their home`, `settlement_abandoned_to=the abandonment of their home`, `plague=the plague`, `plague_outbreak=the plague`, `plague_spread=the plague`, `famine=the famine`, `settlement_fled=their flight`, `region_ceded=the cession`, `settlements_ceded=the cession`. Sample: `After the plague and the abandonment of their home, 123 Elves from Thiordell found shelter in Leithiedell, welcomed by the House of Roelaele.` The engineer lists any cause kind of an arrival in the calibration sweep without a word (target W1, 11).

# Part 5. Audit advisories

## 8. A1 and A2

- **A1** (amends P5-03 6.3.2): after `mix`'s largest-remainder split, if the ruling race q_s was in S and its share is 0, it takes 1 from the largest share (then the smaller race id). So the ruling entry is never dropped and every settlement's shares sum to 1000 with one ruling race (invariant 3). Changes a world only where a host of a few people takes a group of about a thousand times its size (none known).
- **A2** (amends P5-03 7.1): B(a, b) and the pairs are computed when the subsystem is constructed in `stages/history.cpp` (from `HistoryInputs` and the content set, which the stage holds), not in its first step; `disposition` and `hatred_event` are valid from then (D = B, no hatred event). No output change.

# Part 6. Report v6

## 9. History report format v6 (amends P3-07; new section 29)

### 9.1 Version

Header **`# worldgen history v6`** at both levels (9.2 and 9.3 remove lines at summary, which the growth rule of 25.1 does not cover). `report --help` prints it. The growth rule holds for v6.

### 9.2 `[civilizations]` at summary

- **Milestones**: the line `  milestones:` and its list become `  milestones: <n>` (n = the milestones the full block lists for that realm; an ended realm's n counts its full list), followed by the milestone lines of kind `quiet_stretch` only, as now. Reading: the other milestones are chronicle events, and the summary's chronicle or the full block carries each; *measured*: 76 milestone lines a default summary, 8 of them quiet stretches.
- **Adversaries**: no `    without war:` line (the count line `  adversaries: <n>` and the war lines stay).
- Full is unchanged. No line outside `[chronicle]` depends on the summary set (P5-02 12.9 stays exact).

### 9.3 `[present]`'s ruins at summary (F6)

When there are more than 15 ruins: the listed ruins are the union of the 5 oldest (`ruined_year` ascending, then id), the 5 largest (their largest `settlement_census` population, descending, then id) and the 5 most recent (`ruined_year` descending, then id descending), in ascending id as now, followed by `  and <x> more` (x = ruins not listed). The count line `ruins: <n>` is unchanged. Full is unchanged.

### 9.4 `[peoples]` (F2, F8.7, F8.8, F10)

```
  <A> and <B>: <band> <D>, met <year> e<id>[ by word][, turned <year> e<id> from <old band>]
  never met: <A> and <B>[, <C> and <D>…]
  the <plural> met no other people
mixed: …; refugees: <a> arrival(s), <p> people; expelled: <e> time(s), <q> people
  s<id> <name> (<owner ref>): <ruling race> <share> ruling; <race> <share> <standing>[; …]
```

- `by word`: the pair's `peoples_met` has `sealed` 1. `from <old band>`: the band before the pair's last turn (`hatred`, `wariness`, `friendship`).
- **Unmet lines, both levels**, after the pair lines: `  the <plural> met no other people` for each present race that met no race (ascending race id; only when two or more races are present); then one line `  never met: …` of the other unmet pairs (both races met some race), in ascending (race_a, race_b), when there is one. The full level keeps its per-pair `never met` lines as well. When the world has more than `SUMMARY_LINES_CIVS` civilizations the summary keeps these lines (at most races + 1).
- Plurals: `1 arrival`, `1 time` (else `arrivals`, `times`).
- A mixed settlement's entries: the ruling race first, then share descending, then race id.

### 9.5 Mixed events in the summary (F3; amends P3-07 24.3)

A `settlement_mixed` event is a candidate of the summary set only when it is the first `settlement_mixed` of its unordered pair {`race`, `ruling`} in the world and among the first `SUMMARY_MIXED_MAX` (**3**, new constant in `digest.hpp`) such events by id. *Measured*: up to 26 such lines in one default summary at 0.24.0.

### 9.6 `[stats]` (full; appended)

`peoples_met_by_word` (kind 57 with `sealed` 1), `dispositions_warmed` (kind 58 into a higher band than the pair's previous band).

### 9.7 Length

*Measured* by removing 9.2's and 9.3's lines from the 200 default summaries of 0.24.0 and re-applying P5-02 12.9's budget: median 620 → about 542, maximum 906 → about 753 (seed 174, 19 civilizations); `civs4` median 1,112 → about 890 (the budget binds there, so the chronicle takes back some of what the blocks give up). `SUMMARY_LINES_MAX` (790) and the rest of 12.9 are unchanged: the cut lowers O, and the line limit L(C) = min(1390, 790 + 40 × max(0, C − 16)) still caps the rest. T18's maximum of 800 is restated as L(C) (11): a default world with 17 to 19 realms is allowed 830 to 910 lines by 12.9 itself, which is why seeds 100, 161 and 174 passed 800.

# Part 7. Data, catalogue and everything else

## 10. Data

### 10.1 `history.relations` (`game/data/history/relations.toml` and both fixture roots)

Changed values: `shelter_memory` 45 → **90**; `conquest_settlers_permille` 100 → **50** (now the least settlers' share, 5.1). New fields, appended after `expel_share_max` in this order, all required:

| Field | Type | Shipped | Meaning |
| --- | --- | --- | --- |
| `peace_years` | int(0, 100000) | 100 | years without war between two peoples before peace counts (3.2) |
| `peace_memory_per_century` | int(0, 10000) | 60 | goodwill gained a century at peace in contact |
| `peace_memory_max` | int(0, 2000) | 300 | peace alone builds memory up to this |
| `contact_hops` | int(0, 8) | 1 | contact: presences within this many borders of the border graph |
| `foe_years` | int(1, 100000) | 100 | window of a common enemy (3.3) |
| `foe_war_memory` | int(0, 1000) | 60 | gained per common enemy realm |
| `foe_monster_memory` | int(0, 1000) | 60 | gained per common monster |
| `meet_sealed_hops` | int(−1, 8) | 3 | meeting by word within this many borders of the border graph; −1 never (4.2) |
| `meet_sealed_years` | int(0, 100000) | 300 | years in reach before a meeting by word |
| `conquest_settlers_span_permille` | int(0, 1000) | 400 | span of the settlers' draw (5.1) |
| `expel_gap_years` | int(0, 100000) | 60 | least years between two expulsions by one realm (5.3) |

Checks (with the history data checks): `relations_peace_cap`: `peace_memory_max` ≤ `memory_max` (`peace_memory_max <a> is above memory_max <b>`); `relations_settlers`: `conquest_settlers_permille` + `conquest_settlers_span_permille` ≤ 1000 (`conquest settlers <a> + <b> exceed 1000`).

The **`zero`** fixture (P5-03 7.4): the new `*_memory` fields and `peace_memory_per_century` 0, `meet_sealed_hops` −1, `conquest_settlers_span_permille` 0; I0 holds as stated there (no meeting, so no credit, no peace, no event). The **`relations-small`** root: `meet_sealed_hops` 3, `meet_sealed_years` 0, the `*_memory` fields large as its others, `expel_gap_years` 0.

### 10.2 `history.monsters`

`undead_min_dead` 4000 → **1000** (its meaning: the least threshold, 6); new `undead_dead_permille` int(0, 1000) **25**, appended after `overrun_people_ref`. P5-02's `zero` fixture keeps the shipped values.

### 10.3 `history.report`

The templates of 3.5, 4.2, 5.2, 5.3, 7.2 and 7.3; saliences unchanged.

### 10.4 Catalogue (amends P5-03 8.1; no DDL change)

| Kind | Values, in seq order |
| --- | --- |
| 57 `peoples_met` | `disposition`, `band`, **`sealed`** |
| 60 `settlement_mixed` | `share`, **`standing`**, **`majority`** |
| 62 `people_expelled` | `people`, **`standing`**, **`years`** |

`event_kind_value` gains five rows; every event of these kinds carries the new values. `docs/specs/schema.md`'s catalogue listing is updated by the engineer. Reader checks (16.4) unchanged in form; the event checks of P5-03 16.4 add: `sealed` 0 or 1, `majority` 0 or 1, `standing` 0 to 2 (kind 60) and 0 to 1 (kind 62), `years` ≥ 0 (`value <k> <v> outside <a> to <b>`, the existing form).

## 11. Targets (validation)

At 1024 defaults, seeds 0–99 (and 100–199 as a second sample where named), unless stated.

| Target | Definition | Bound | 0.24.0 |
| --- | --- | --- | --- |
| **RR2a** | `disposition_turned` per world | median 2 to 6; max ≤ 15; ≤ 15 worlds with none | median 1; 27 worlds with no pair changing band |
| **RR2b** | turns into friendship | ≥ 20% of all turns | 3 of 149 |
| **RR2c** | turns to a higher band (to wariness from hatred, to friendship) | ≥ 30% of all turns; hatred to wariness entered at least 5 times in the sweep | 3 into friendship; 30 into wariness, from either side (not separated) |
| **RR2d** | turns before year 5500; before 4500 | ≥ 55%; ≥ 25% (the brief's 40% before 5500 is already met at 0.24.0) | 68 of 149 (46%); 29 (19%), by the critic's table |
| **RR4** | pairs in friendship at E | median ≥ 3 of 15; pairs in hatred at E median ≥ 3 (the world stays dark) | friendship 2; hatred 4 to 6 |
| **RR5** | end tallies | no (hatred, wariness, friendship) tally in more than 20 worlds | 35 worlds share one |
| **RR1** | all present pairs met | in ≥ 95 worlds (seeds 0–99 and 100–199 each) | 84, 87 |
| **RR3** | a present race that meets no other | ≤ 3 worlds, each named in its summary (9.4, by rule) | 10 |
| **C1** | wars per world | median within ±10% of 0.24.0's 25.5 (23 to 28); T1 to T20 of P5-01 as bounded | 25.5 |
| **T16** | repeated battles | ≤ 2 per thousand | 0.93 |
| **G2** | standing settlements; population at E | medians ≥ 81 and 214,500 | 83; 217,827 |
| **MX1** | mixed standing settlements at E | median 3 to 12; ≥ 80 worlds | 8 |
| **MX3** | `people_expelled` per world | median ≤ 3; max ≤ 12 | 3; 22 |
| **MX4** | `share` values of `settlement_mixed` | no value in more than 15% of the sweep's events | 900 in 75% |
| **MX5** | summary `settlement_mixed` lines | ≤ 3 a world (by rule) | up to 26 |
| **U1** | `undead_rose` after year 5500 | ≤ 55% of risings; risings in the sweep 150 to 290 | 142 of 191 (74%) |
| **W1** | arrival lines with a cause clause | 100% (every cause kind has a word) | 670 of 698 |
| **W2** | monster lines | no `its lair` for a grouped kind; no undead slaying, death or rising line that names its region twice (by rule; checked by grep) | — |
| **T18** | summary lines | default median ≤ 580; every world ≤ L(C) (P5-02 12.9) and ≤ 800 when C ≤ 16; `civs4` median ≤ 1,000, max ≤ 1,200 | 620; 906 (C = 19); `civs4` 1,112 |
| **M** | P5-02 and P5-03 part-B targets as bounded in P5-03 r2 21 | unchanged | — |
| **timing** | `generate` seed 0 1024 release | at most 1 s over 0.24.0 (9.1 s) | — |

**Calibration** (data only, the engineer, in this order; values into r2): (1) RR2b, RR2c, RR4 by `peace_memory_per_century`, `peace_memory_max` and `peace_years`; (2) C1 by `war_memory` (back toward 0.24.0's wars if peace lowers strain) and, if needed, `peace_memory_max`; (3) RR2d by `foe_war_memory`, `foe_monster_memory` and `shelter_memory`; (4) RR1, RR3 by `meet_sealed_hops` and `meet_sealed_years`; (5) MX4 by the two settlers' fields; MX3 by `expel_gap_years`; (6) U1 by `undead_dead_permille` and `undead_min_dead`; G2 and M checked after each. `friend_min`, `hatred_max` and `band_margin` stay (P5-04 will read the bands; moving them moves every pair's base band).

## 12. Test-side items (for the test writer)

1. `p5_03_relations_reference.py check-world` invariant 4: a colony founded in the year its region changes hands has first rows already under the new rulers (one row per year, P5-03 6.7; validation finding 1, 14 of 170 worlds). The invariant becomes: a colony's first rows are its founder's race at 1000, **or** the result of 6.3.3 (as amended by 5.1) applied to that in the same year when the settlement's owner changed that year.
2. Same file, lines 1317 and 1726: the reader's message for a race participant without a row is `corrupt table 'event_participant': row (<event>, <seq>): race <r> does not exist` (`event_catalogue.cpp`'s `race_participant_problem`, P5-02 11.4's order), not `corrupt table 'event': row <e>: race <r> has no row`.
3. `p5_12_renaming_reference.py check-world` `max_weight`: the earlier row's rule is looked up by the cause kinds of every rule (`heartland`'s `became_city` included), so it is never `None` for a rule in force (validation finding 2; P5-03 15's rule in force).
4. `p5_02_monsters_reference.py check-world` invariants 5 and 11: replaced by P5-03 9.2's toll (above `kill_max` at `toll_over_permille`) and 9.1's causes (the waking event and at most one earlier raid on the place within `raid_chain_years`).
5. The test comments that say "43 pages" (`stages_biomes_golden_test.cpp:718`, `stages_regions_golden_test.cpp:944` and `:950`, `db_writer_reader_test.cpp:219`, `cli_generate_test.cpp:372`, `cli_regions_test.cpp:310`): 45 pages at schema 10 (184,320 bytes, the asserted value, is right).
6. `worldgen/tests/golden/p5-03-relations.txt` gains the case `p5-monsters-high-1024` (seed 7, `--monster-pressure high`, the `world-hashes.sh` case) in the slow suite.

## 13. The seam for P5-04

Not designed here: alliances, trade and vassalage. What this stage leaves for them: (a) `credit` (3.5), the one path by which any later event moves a pair's memory with its own cause; (b) `DispositionView` gains `std::int64_t band(std::int64_t race_a, std::int64_t race_b) const` (the pair's band, the plain band of B before a meeting) and `bool met(std::int64_t race_a, std::int64_t race_b) const`, both const reads that conflict does not use in this stage; (c) the friendship band, which with 3.2 is reached by neighbours in long peace and by peoples who fought a common enemy, the ground the design names for alliances ("aligned peoples in contact form partnerships", design 5 "Waiting for phase P5"; "the first wars and alliances form" in an age's active phase, design 5 "Deep time"). P5-04 decides what band or disposition an alliance needs.

## 14. Tests required

C++ (shards: `-relations` for relations files, `-monsters` for 6 and 7, `-history-report` for 9):

1. Data: the eleven fields' bounds and absence, `relations_peace_cap`, `relations_settlers`, `undead_dead_permille`.
2. Peace: contact at `contact_hops` and + 1 over a closed border; the war reset of `strife_year`; the gain at `peace_years` − 1 and `peace_years`; the cap (M below, at, above it); ticks against a quiet step of L years; with decay.
3. Common enemy: two wars of one realm (overlapping, within `foe_years`, at `foe_years`); same race as z excluded; once per (pair, z); the event cited; a monster raiding two peoples within and outside the window, once per (pair, m); unmet pairs not credited.
4. Causes: a warmer turn with positive contributions, without (the meeting); a colder turn without (the last turn); eight of each sign kept; both cleared at a turn.
5. Meeting by word: h at `meet_sealed_hops` and + 1; `word_since` set, kept, reset; the meeting at `meet_sealed_years`; an open meeting first; `sealed` value; −1.
6. Settlers: the draw's range at 0 and the full span; no draw for the same people.
7. Expulsion gap: at `expel_gap_years` − 1 and `expel_gap_years`; `standing` and `years` values.
8. A1: a host of population 1 with room for 2,000 keeps its ruling race at share ≥ 1; A2: `disposition` before the first step equals B.
9. Undead threshold: T at P_W below and above `undead_min_dead` × 1000 div `undead_dead_permille`; with M.
10. Rendering: `{m:its}`, `{m:aside}`, `{m:where}` (wild; dead named for the region, renamed region, named for a settlement) and their `template_problem`; every changed template on a fixture event; arrival cause words.
11. Report v6 on fixture **H15** (`h15-v6.toml`, from H14: a pair met by word, a warmer turn, a race that met no one, 17 ruins of distinct ages and sizes, four mixed events of two race pairs, a realm with ten milestones of which two quiet stretches), both levels.
12. Invariants of P5-03 19 on the existing seeds, plus: (a) a pair's warmer turn cites only positive contributions or its meeting; (b) two expulsions by one realm are at least `expel_gap_years` apart; (c) a `peoples_met` with `sealed` 1 has open hops > `meet_hops` or none between the key's regions and border hops ≤ `meet_sealed_hops`.
13. I0 (P5-03 7.4) with the new `zero` values; resume (P3-04 16.3).

Reference (`p5_03_relations_reference.py`): 3, 4, 5 and 8 in the replay; 12's items 1 to 6. Goldens (all re-recorded once, after the cross-preset comparison): every world file (`GENERATOR_VERSION` 0.25.0, `content_hash`, catalogue rows, every history): `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, `GOLDEN_CASES`, `stages_pipeline_test.cpp`, the README phrase hash (validation); `p3-01-data.txt`, `p3-04-engine.txt`, `p3-06-conflict.txt`, `p3-07-report.txt` (v6), `p4-01-timeline.txt`, `p4-04-markers.txt`, `p4-05-browser.txt`, `p5-12-renaming.txt`, `p5-02-monsters.txt`, `p5-03-relations.txt` (test writer); `p3-05-peoples.txt` only if it hashes the catalogue or template rows (the test writer checks); `core_vectors.txt`, `p1-0*`, `p3-02-names.txt`, `p3-03-chronology.txt` must not move. `world-hashes.sh` keeps sixteen cases.

## 15. Determinism notes

One new keyed stream (5.1), drawn only on a change of ruling people, key of ids. Every new choice is a minimum of a tuple ending in ids (4.2's key) or a loop in ascending id over ordered containers (3.2's bit sets per region, 3.3's entries and credited sets as `std::set` of id tuples, the border graph's neighbours ascending). Arithmetic int64 with checked multiplication: 3.2's g ≤ 10^5 × 10^4 × 1000 / 100 = 10^10; 6's P_W × 1000 < 2^63 for any population `State` allows; T × 1000 as P5-03 14.2. The renderer's `{m:where}` compares stored bytes only. No floating point, no unordered container.

## 16. Amendments

| Spec | Section | What | Note |
| --- | --- | --- | --- |
| P5-03 | 3, 4.2, 5, 6.1, 6.2.2 to 6.2.4, 6.3.2, 6.3.3, 6.4.3, 6.5, 7.1, 8.1, 8.2, 17, 19, 21 | this spec, parts 1 to 3 and 5 | r3 pointer |
| P5-02 | 6.3.2, 7.2's templates | undead threshold (6); templates (7.2) | r4 pointer |
| P3-04 | 8.4, 8.5 | forms `its`, `aside`, `where` | r24 pointer |
| P3-07 | new 29; 24.3 | v6 (9) | r24 pointer |
| `schema.md` | catalogue | five value rows | the engineer |

## 17. Open questions (for the lead and Paul)

1. **How warm should peace make neighbours?** With the shipped cap (300), every good–neutral and neutral–neutral pair at peace in contact for about 850 years becomes friends, and only war keeps them from it. *Recommendation*: as shipped, tuned to RR4 (friendship median ≥ 3, hatred median ≥ 3): it is what Paul asked for, and wars still undo it. *Alternative*: cap 250, so that peace alone stops just short of friendship and a common enemy or shelter must finish it (fewer, better-caused friendships).
2. **Meeting by word across closed borders.** *Recommendation*: yes (three borders, 300 years); it turns a hole in the record into a fact and gives the sealed dwarves a disposition P5-04 can use, without letting armies or refugees through. *Alternative*: report only (9.4's lines), the dwarves stay strangers; RR1 and RR3 then stay reported, not bounded.
3. **The undead threshold by world population.** *Recommendation*: yes (6). *Alternative*: keep the absolute 4,000 and record that late risings follow late wars (design-true, and the present-day picture the critic calls "a gift").
4. **Summary milestones as a count.** *Recommendation*: count plus quiet stretches (9.2). *Alternative*: list the milestones that are not in the summary set, which needs a second pass of the budget (the blocks would depend on the set).
5. **One expulsion per realm in 60 years.** *Recommendation*: yes (5.3). *Alternative*: two in 100 years (a policy can still read as a series).
6. **T18's maximum.** *Recommendation*: restate as L(C) of P5-02 12.9 (≤ 800 only for C ≤ 16), since 12.9 itself allows more for more realms. *Alternative*: lower `SUMMARY_LINES_PER_CIV` so that 800 holds for every default world.

## 18. Proposals

One entry in `docs/design/proposals.md`: "History: P5-03a relations follow-ups (readings)".

## Revision notes

- r1 (2026-11-24): first draft.
