# P5-04a Relations and alliances follow-ups

Status: **r3 (2026-11-26)**, text only: two readings recorded in 21.4 (revision notes). **r2 (2026-11-25)**: r1 (approved by Paul on 2026-11-25 with Q-P5-04a-1) refreshed against P5-06 as built, with the items P5-06's validation left to this stage (section 21). No rule of r1 is changed in substance. Written by the spec writer on `p5/04a-spec` (r1 cut from `main` at 1ded5db; r2 on the same branch after the merge of the P5-06 candidate, 924ef70). Phase P5 History depth, stage 04a: the follow-ups of P5-04 (`docs/specs/p5-04-alliances-trade.md` r3) and the open notes of P5-03a (`docs/specs/p5-03a-relations-followups.md` r2), from the world critics' reviews (`docs/reviews/p5-04-alliances-trade-world-critic.md`, Point 1, Point 3, F1 to F11; `docs/reviews/p5-03a-relations-followups-world-critic.md`, F1 to F7; `docs/reviews/p5-06-figures-world-critic.md` F1, F3 to F5, F8), the validations (`docs/reviews/p5-04-alliances-trade-validation.md`, findings 1 to 5; `docs/reviews/p5-06-figures-validation.md` 4 to 6), the audits (`docs/reviews/p5-04-alliances-trade-audit.md` A1 to A3; `docs/reviews/p5-03a-relations-followups-audit.md` A1 to A4; `docs/reviews/p5-06-figures-audit.md` A1 to A3), Paul's decision of 2026-11-25 on spoils (`docs/questions-for-paul.md` Q-P5-04-8; `docs/design/proposals.md` "Paul's answers of 2026-11-25") and the lead's ruling "P5-06 figures as validated" (proposals, 2026-11-25). **Base: P5-06 Figures as built**: generator 0.27.0, schema 12 (36 tables), history report format v8, event kinds 1 to 92, 97 streams, browser groups 0 to 8 (`ALL_GROUPS` 0x1FF); `docs/specs/p5-06-figures.md` r4. It reads P5-06's rulers and tempers. Generator **0.28.0**; **schema stays 12**; history report format **v9**.

**No schema version change.** No table or column is added or changed, and no event kind is added; no new code is added to a `schema.md` code list. The change:
- changes rules of `AlliancesSubsystem` (spoils, refusals, betrayals, renewals), `RelationsSubsystem` (a turn's wording and a cool-down, expulsions of long-settled peoples, two audit fixes), `ConflictSubsystem` (two const reads, the arms cause of a battle, three audit fixes) and `FiguresSubsystem` (tower spacing and endings, 21.4; P5-06 audit A1 to A3, 11 item 8); adds one keyed stream (`hist.alliances.renew`, the 98th);
- appends fields to `history.alliances`, `history.relations` and `history.figures` (data) and changes shipped values (14);
- appends values to seven existing event kinds (58, 63, 67, 69, 75, 76 and 87, ten `event_kind_value` rows) and one code to an existing value (`reason` of kind 76, a value range, not a `schema.md` code list); catalogue rows written by the generator, as P5-03a did;
- lets a site of kind 4 `tower` end with the existing `site.ending` codes 4 and 5 (21.4): no DDL and no new code, but `schema.md`'s note "towers do not end" and P5-06's reader check change (lead question L-2, 23);
- widens the renderer's choice to eight words and adds the form `{m:died}` (P3-04 8.4);
- rewrites templates (data) and parts of the history report as format v9.

The form is that of P5-03a: the normative text is here, and P5-04, P5-03, P5-03a and P5-06 get a pointer. Figures marked *measured* are validation's at 0.26.0 or 0.27.0 as stated (seeds 0–99 / 100–199, 1024 defaults) or the critics' (16 worlds); every other figure is for the engineer's calibration and validation.

## 1. Items

| # | Item (source) | In short | Section | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Spoils withheld (Paul, Q-P5-04-8; P5-04 critic Point 3) | A first kept share sours the alliance (memory, grudge, the next renewal and the next call at risk); a second, or one large, ends it at once, in the same year; SP2 restated | 3 | yes |
| 2 | Refusals and betrayals with a motive (critic F1, F2; AL6) | The join chance is a product of named factors; the smallest names the refusal's motive; excused refusals do not count toward a break; a betrayal states its motive and the alliance's age | 4, 5 | yes |
| 3 | Renewals (critic F3) | No event for a renewal with nothing new; the renewal dates are a fixed chain so the report still counts them | 6 | yes |
| 4 | Disposition wording and rhythm (P5-03a critic F1 to F3) | A thaw and a decline into wariness worded apart; a cool-down of 300 years after a turn unless a fresh cause | 7 | yes |
| 5 | Summary milestones (P5-03a critic F4) | A true count and the three weightiest milestones that are not quiet stretches | 12.2 | report |
| 6 | Wording (both critics) | `against … against`, the trade clause on war pacts, "a neighbour's land", names in `allies:`, undead "great age", one line for same-day mixed and standing lines, `; ended` for a trade closed by a realm's end; a clause for an alliance across hatred | 8, 12 | yes (templates, one value) |
| 7 | Visible effects (critic F6, F9) | A share names its size; the first battle a buyer of arms wins after a sale cites the sale | 9 | yes |
| 8 | Expulsions of long-settled peoples (P5-03a critic F6) | A people held 800 years or more is expelled only after a fresh turn into hatred | 10 | yes |
| 9 | Housekeeping (validation, audits) | Spoils only for the principal's own conquest; reader checks in both readers; audit fixes | 11 | finding 1 yes (rarely); the rest no |
| 10 | Dwarves' participation (validation finding 5) | Restated for dwarves (85%), with the reason: sealed uplands | 13 | no |
| 11 | Carried from P5-06 (lead's ruling; P5-06 validation and critic) | `the Old`'s thresholds; heroes, quiet-step lives, figure counts, skill and grasping betrayals retuned or restated; towers spaced and ended; `[figures]` and `[monsters]` summaries; a `hero_rose` that states a long quiet; guard targets for living monsters and lairs; housekeeping | 21 | yes |

Not here, with reasons (section 17): a mine's ruin or lair (F6), naming a sacked town in a share (F9, needs a settlement on kinds 74 and 75), the set-aside claims line (F11), the dwarves' refusal to swear to goblins (F7), late friendships (P5-03a F3, beyond the cool-down), people met by word through a carrier (P5-03a F8), the ruins list's breakdown (P5-03a F9), late secessions (P5-03a F12).

## 2. Inputs, streams, draws

As P5-04 3 and P5-06 3, plus:

- **`FigureView`** (P5-06 7.1) gains `std::int64_t temper(std::int64_t civ) const`: the temper code of c's current reign (0 steady, 1 warlike, 2 peaceable, 3 grasping, 4 just, the order of P5-06 4.1's record), −1 with no reign or no view. Read by `AlliancesSubsystem` (4.2, 5).
- **Conflict's const reads** (amend P5-04 7.13): `bool has_claim(std::int64_t a, std::int64_t b) const` (an open claim of a against b) and `claim_event(a, b)` (its event, 0 none); `last_war_end(std::int64_t c) const`: the largest (year, event) of `last_peace(c, b)` over every civilization b ≠ c ascending (a `peace`, `war_won`, `truce` or `ally_withdrew` that ended a war of c, principal or joined), (0, 0) none.
- **`AllianceView`** (P5-04 7.1) gains `std::int64_t arms_event(std::int64_t c) const` (9.2).
- **Stream** appended to `streams.hpp` after P5-06's last (`hist.monsters.hero_fall`, the 97th): **`hist.alliances.renew`**, key (x, y, d) with x < y the alliance's pair and d the renewal's due year (6.1), 3.4. The list holds **98** streams. Every other rule of this stage is deterministic (ids, orders, values).
- (r2) **`FigureView::split_bonus_permille(civ, year)`** (P5-06 audit A3, 11 item 8) and **`MonstersSubsystem::height_cell(ctx, region, avoid)`**, the height of P5-02 6.6.1 with a set of cells to keep clear of (21.4).

### 2.1 Figures in this stage's rules (r2)

P5-06 6.15 left alliance wording, motives and renewals to this stage. Each point below says whether a rule reads a figure; none adds a figure mechanic beyond the two r1 already had (the `temper` read and the deed `ally_refused` with `motive` 5).

| Rule | Reads a figure? | How |
| --- | --- | --- |
| Spoils kept or shared (3, 9.1) | yes, as P5-06 | `{principal:under}` names the ruler P5-06 6.4 rule 3 records at the `spoils_*` event; the grant chance keeps P5-06 7.3's `temper_spoils_permille` term, not moved here. The souring, the soured renewal and the soured call read no temper. |
| Refusal (4) | yes | f5 = `grasping_join_permille` when `FigureView::temper(z)` = 3, the reign current at alliances' T2 (figures' T3 runs after it, so a reign that ends this year is still the one read); `{ally:under}` renders the ruler recorded at the event; P5-06 6.4 rule 3 gains the deed `ally_refused` with `motive` 5 (the ally), so a grasping refusal always names its ruler (r1, unchanged). An excused or other refusal names a ruler only when one is already recorded and reigning. |
| Betrayal (5) | yes | motive 0 when `FigureView::temper(a)` = 3 at T2, the same reign P5-06 names at the `alliance_betrayed` in its T3 log; `{betrayer:under}` as P5-06. |
| Renewal, soured renewal, spurning (3.5, 6) | no | no ruler is named or read; `alliance_renewed` and `alliance_spurned` stay without `{r:under}` (P5-06 6.15). A ruler's death does not renew, test or end an alliance. |
| Thaw, decline, cool-down (7) | no | race-level; no figure. |
| Expulsions (8) | no | |
| Arms in a battle won (9.2) | no new read | the prefix is added to `battle_led` as to the other three battle kinds; the general clauses are kept after the prefix. |
| Towers (21.4), heroes (21.2), `[figures]` (21.5) | P5-06's own state | rules of `FiguresSubsystem` and the report. |

# Part 1. Alliances

## 3. Spoils withheld (item 1; amends P5-04 5, 6.7, 6.11, 8.1, 8.3)

### 3.1 Diagnosis

*Measured*: shares owed 119 and 114 (100 + 100 worlds), granted 680 and 649 per mille, so about 38 and 40 withheld; spurnings 28 and 29, nearly one per withholding (`withheld_to_break` 1 ends the alliance at its next renewal, and the chill of 500 breaks a cool one by cooling). The critic: the spurning follows the withholding by 1 to 106 years while the sentence reads as an act of the next season. Paul (2026-11-25): a first withheld share sours the alliance; a second, or one large kept share, ends it.

### 3.2 State (amends P5-04 5)

The alliance's per-term withheld record is replaced by: **withheld** (count over the alliance's life, 0 at formation, never reset), and the **soured** record (the first withholding's `spoils_withheld` event, its year, its region, the wronged ally z; empty when none) with two flags, `renewal_due` and `call_due`, both set at the souring.

### 3.3 Large

For an owed ally z with part(z) and share(z) of P5-04 6.11:

```
large(z) = share(z) ≥ spoils_large_share  or  part(z) ≥ spoils_large_part_permille
```

Shipped starting values 600 people and 300 per mille (an ally that bore three tenths of its side's dead, or a share of 600 people or more; *measured* shares 40 to 2,105 in the critic's 16 worlds). Calibrated so that 30% to 50% of withheld shares on open alliances are large (15.1 SP4).

### 3.4 At a withheld share (replaces P5-04 6.11's "Withheld" item and 6.7's "Spoils and breaking")

After the `spoils_withheld` e is appended, its `credit` and its `add_grudge_seam` (unchanged), when W and z have an open alliance A that is **not of origin**:

1. **Ends now** when large(z) or withheld(A) + 1 ≥ `withheld_to_break` (shipped **2**: this is the second share kept in the alliance's life): e's value `grievance` = 2; withheld(A) += 1; then, in the same pass and year, append **`alliance_spurned`** (region: e's region; participants z `ally`, W `principal`; value `reason` **2** (at once), `years` 0; causes e and A's formation or last written renewal, ascending); close the `alliance` row with it; drop A. In Q4 the spurning needs `quiet_events_left(z)` > 0; when it lacks it, A is spurned at the next T2 or Q4 with `reason` 2 and `years` = that step's Y − year(e).
2. **Sours** otherwise: e's value `grievance` = 1; withheld(A) = 1; A's soured record = (e, year(e), e's region, z), `renewal_due` and `call_due` set.

When W and z have no open alliance, or it is of origin: `grievance` 0; nothing else (origin alliances are never spurned, P5-04 24 L4). The `credit` (− `withheld_memory`) and the grudge (+ `withheld_grudge`) are the sourness in memory and between the realms; shipped `withheld_grudge` **400 → 250** (no longer needed to break an alliance by cooling; calibrated, 15.2).

### 3.5 A soured alliance (amends P5-04 6.5, 6.7)

- **At the next renewal** (6.1), while `renewal_due`: the renewal is made only if `chance_pm(D(hist.alliances.renew; x, y, d), soured_renew_permille)` (shipped **500**); then `renewal_due` is cleared and the renewal proceeds by 6.1 (a reason must still hold). If the draw fails: append `alliance_spurned` (participants the soured record's z `ally`, the other `principal`; region the soured region; `reason` **0** (at its renewal), `years` = Y − the soured year; causes the soured `spoils_withheld` and A's formation or last written renewal, ascending); close; drop.
- **At the next defence call** of A's wronged ally z to its partner (4.2), while `call_due`: the factor f_spoils = `soured_join_permille` (shipped **500**); `call_due` is cleared after the call, joined or refused.
- **Cooling** (P5-04 6.7 item 1) of an alliance with a soured record: `alliance_spurned` `reason` 1 (as now), with `years` = Y − the soured year; without a soured record: `alliance_broken` as now.
- The soured record is kept for the alliance's life (a second withholding ends it, 3.4).

### 3.6 Values and templates

Kind 75 `spoils_withheld` gains `grievance` (0 to 2); kind 76 `alliance_spurned` gains `years` (0 to E − 1) and its `reason` code 2 (0 at its renewal, 1 cooled, 2 at once). Templates (9.1 names the size; `\|` in a table cell is the table's escape for `|`):

| Kind | Template |
| --- | --- |
| `spoils_withheld` | `{principal}{principal:under} kept the spoils of {@region}, {share?no captives\|one captive\|# captives}, from {ally}, its ally{grievance?\|, which did not forget it\|, which would not forgive it}.` |
| `alliance_spurned` | `{reason?Still bitter\|Long resentful\|Outraged} that the spoils of {@region} had been kept from it{years?\| the year before\| # years before}, {ally} broke its alliance with {principal}.` |

Samples: `The Hold of Durn under Thrain kept the spoils of the Grey Fells, 412 captives, from the Kingdom of Cear, its ally, which would not forgive it.` · `Outraged that the spoils of the Grey Fells had been kept from it, the Kingdom of Cear broke its alliance with the Hold of Durn.` · `Still bitter that the spoils of the Fallen Wood had been kept from it 106 years before, the Warband of Muuz'grurk broke its alliance with the Zux Mob.`

### 3.7 Fields (`history.alliances`, appended, 14.1)

`spoils_large_share` int(1, 1000000) 600; `spoils_large_part_permille` int(0, 1001) 300 (1001: never by part); `soured_renew_permille` int(0, 1000) 500; `soured_join_permille` int(0, 1000) 500. `withheld_to_break` keeps its range and changes meaning: withheld shares in the alliance's **life** that end it at once; shipped 1 → **2**.

## 4. Refusals with a motive (item 2; amends P5-04 6.5, 8.1, 8.3)

### 4.1 Diagnosis

*Measured*: 687 refused against 736 joined (seeds 0–99), **746 against 698** (100–199); joins in defence 82% and 79%, so attack joins occur (about 130 and 145, 18% and 21%; the critic's s0 and s1 had none). A defence chance is ajp × w × σ: a cool alliance (w near 0) or a strong enemy (σ small) almost always refuses, and the sentence gives no reason. Breaks after refusals: `refusals_to_break` 2 counts every refusal of the alliance's life.

### 4.2 The defence chance as factors (replaces P5-04 6.5's defence formula)

For a defence call to z (partner p, enemy e), each factor in [0, 1000], live:

| Code | Motive | Factor |
| --- | --- | --- |
| 0 | its people's reluctance | f0 = `ally_join_permille`(r_z) |
| 1 | weariness | f1 = `join_weary_permille` when `last_war_end(z)`'s year ≥ Y − `weary_years`, else 1000 |
| 2 | a war of its own | f2 = max(0, 1000 − `join_busy_permille` × `open_wars_of`(z)) |
| 3 | fear of the enemy | f3 = `join_fear_floor` + (1000 − `join_fear_floor`) × σ div 1000 (σ as P5-04 6.5) |
| 4 | a cooled friendship | f4 = `join_warmth_floor` + (1000 − `join_warmth_floor`) × w(z, p) div 1000 (w as P5-04 6.2; 0 below `ally_min_disposition`) |
| 5 | a grasping ruler | f5 = `grasping_join_permille` when `FigureView::temper(z)` = 3, else 1000 |
| 6 | an old claim | f6 = `join_claim_permille` when `has_claim(z, p)`, else 1000 |
| 7 | spoils kept | f7 = `soured_join_permille` when the alliance's `call_due` is set and z is its wronged ally (3.5), else 1000 |

```
p = defend_permille
p = p × f_k div 1000      for k = 0, 1, …, 7 in this order
```

The attack chance is P5-04 6.5's, times f1, f2 and f5 in that order (an attack call refused still writes nothing: an ally owes defence, not conquest). The draw is unchanged (`hist.alliances.join`; w, z, 0).

**The motive** of a refusal: the code k in 1 to 7 with the smallest f_k among those below 1000 (the smaller code at a tie); when none is below 1000, 0. So a refusal is put down to its people's reluctance only when nothing else held the ally back.

### 4.3 Graded consequence (replaces P5-04 6.5's refusal item)

A refusal of a defence call by a non-origin ally appends **`ally_refused`** (participants as now; values `motive` and `count`; causes w, the alliance's formation or last written renewal, and by motive: 1 the event of `last_war_end(z)`, 6 `claim_event(z, p)`, 7 the soured `spoils_withheld`; ascending, without repeats). Then:

- **Excused** (motive 1 or 2: z was spent or fighting): `count` 0; `credit(r_z, r_p, −excused_memory, event)`; the refusals count is unchanged.
- **Counted** (any other motive): refusals += 1; `count` = refusals; `credit(r_z, r_p, −refuse_memory, event)`; `add_grudge_seam(z, p, refuse_grudge)` (a second caller of the seam, amends P5-04 7.15, at the same fixed point of T2); if refusals ≥ `refusals_to_break`: `alliance_broken` (value `reason` 0, cause the refusal) as now.

An origin ally's refusal still writes nothing and counts nothing.

### 4.4 Template and fields

Kind 69 `ally_refused` gains `motive` (0 to 7) and `count` (0 to 100). The choice of eight words (P3-04 8.4 as amended in 8.5 here):

`{motive?Loath to send its people to war|Weary of war|With a war of its own to fight|Fearing the strength of the foe|Grown cold toward its ally|Grasping for its own gain|Nursing a claim on its ally's land|Remembering the spoils kept from it}, {ally}{ally:under} would not take up arms for {partner} against {enemy}{count?|, and its ally did not forget it|}.`

Samples: `Weary of war, the Sawy Ring would not take up arms for the Lousstwo Court against the Skreegg Horde.` · `Fearing the strength of the foe, the Hold of Durn would not take up arms for the Kingdom of Cear against the Gub Horde, and its ally did not forget it.` · `Grasping for its own gain, the Zux Mob under Gnash would not take up arms for the Warrens of Zeez against the Hold of Durn.` (`{ally:under}` names a ruler recorded at the event; P5-06 6.4 rule 3 gains the deed `ally_refused` with `motive` 5, the ally, so a grasping refusal always names its ruler.)

Fields (`history.alliances`, appended): `weary_years` int(0, 100000) 50; `join_weary_permille` int(0, 1000) 600; `join_busy_permille` int(0, 1000) 250; `join_fear_floor` int(0, 1000) 300; `join_warmth_floor` int(0, 1000) 400; `grasping_join_permille` int(0, 1000) 600; `join_claim_permille` int(0, 1000) 500; `excused_memory` int(0, 1000) 15; `refuse_grudge` int(0, 2000) 150. `refuse_memory` 40 and `refusals_to_break` 2 keep their values (the count is now of counted refusals).

## 5. Betrayals with a motive (item 2; amends P5-04 6.6, 8.1, 8.3)

At an `alliance_betrayed` (attacker a, defender b, alliance A), the **motive**, the first that holds: 0 a grasping ruler (`FigureView::temper(a)` = 3); 1 a cooled friendship (d(a, b) < `betray_cooled_disposition`, shipped 0); 2 a weak ally (`force(b)` × 1000 < `betray_weak_permille` × `force(a)`, shipped 500); 3 otherwise, land. Kind 67 gains `motive` (0 to 3) and `years` (Y − A's formation year). The sentence no longer repeats the declaration it accompanies (they are each other's companions and always shown together, P5-04 10.3):

`{motive?Grasping for more|Its friendship long cooled|Seeing its ally weak|Coveting its ally's land}, {betrayer}{betrayer:under} broke an alliance of {years} {years?year|years} with {betrayed}.`

Sample: `Seeing its ally weak, the Skreegg Horde under Urz broke an alliance of 1,732 years with the Warrens of Zeez.` Causes unchanged (w and A's formation or last written renewal). Field `betray_cooled_disposition` int(−2000, 2000) 0 and `betray_weak_permille` int(0, 1000) 500 appended to `history.alliances`. The betrayal itself (when conflict declares on an ally) is unchanged; P5-06 7.3's grasping factor on allied strain is what makes grasping betrayals likelier.

## 6. Renewals (item 3; amends P5-04 6.7 item 3, 8.1, 8.3, 10.2)

### 6.1 The renewal chain

An alliance formed in year s has its renewal **due years** d_n = s + n × `term_years`, n ≥ 1, fixed at formation (no drift by quiet steps). The renewal of d_n is made at the first T2 or Q4 with Y ≥ d_n (in Q4, P5-04's "Y + L − 1 ≥ it" becomes Y ≥ d_n: renewals are never early); origin alliances follow the same chain (no event, as now). After it, the next due year is d_{n+1} whatever Y was. Kind 63 `alliance_formed` gains the value **`term`** (`term_years` at the formation, ≥ 1), so a reader can count the renewals without the tuning.

### 6.2 Notable renewals only

At a renewal by P5-04 6.7 item 3 (a reason holds, 3.5 passed), let R be A's formation or last written renewal event. The renewal is **notable** when, since R, one of these happened, each with its event (the latest of its kind, by id):

1. the pair fought together: an `ally_joined_war` with one of the pair as `ally` and the other as `partner`;
2. a share was granted: a `spoils_shared` between them;
3. the basis changed: the basis by 6.2's order differs from A's basis at R; its event is that reason's cause event.

A notable renewal appends `alliance_renewed` (as now; causes the notable events present and R, ascending) and becomes A's last written renewal; it needs `quiet_events_left(x)` in Q4 and, lacking it, is made **quietly** (no event; it never waits). A renewal with nothing notable appends no event: the due year advances, the `credit(renew_memory)` is made with R as its event (an existing event, P5-03a audit A4), and R stays A's last written renewal (the event that joins, refusals and betrayals cite).

Template: `{@causes:ally_joined_war=the war they fought together,spoils_shared=the spoils they shared,war_declared=the outbreak of a new war,trade_opened=long years of trade,disposition_turned=the friendship of their peoples,civ_conquered=the fall of a realm,settlement_captured=the conquests of their foe,sacked=the sacks of their foe,region_ceded=the conquests of their foe,settlements_ceded=the conquests of their foe|After |, }{ally_a} and {ally_b} renewed their alliance{foe:against}.` Salience stays 10 (no summary line). Sample: `After the war they fought together, the Gub Horde and the Warrens of Zeez renewed their alliance.`

### 6.3 What readers see

- `[alliances]` line: `renewed <k>` with k = #{n ≥ 1 : s + n × term ≤ end − 1}, end = the row's `until` year when closed, E when standing: every due year the alliance stood at (a closing in the very step of a due year counts it; rare). Origin alliances print it too.
- `[stats]`: `alliances_renewals` (Σ k), `alliances_renewals_written` (kind 64 events).
- Browser: fewer kind 64 events in group `alliances`; nothing else. Summary: unchanged (kind 64 was never shown). *Measured*: 538 renewal events in 16 worlds; expected written about one in four (15.1 RN1).

# Part 2. Dispositions and peoples

## 7. Disposition wording and rhythm (item 4; amends P5-03 6.2.3, 8.1, 8.2; P5-03a 3.5)

### 7.1 Wording by direction

Kind 58 `disposition_turned` gains **`change`**: 0 into hatred, 1 into wariness from hatred (a thaw), 2 into wariness from friendship (a decline), 3 into friendship. Template (the cause words as P5-03a 3.5): `{@causes:…|After |, }the {race_a:plural} and the {race_b:plural} {change?came to hate one another|put aside their hatred of one another|grew wary of one another|became friends}.` Sample: `After long years of peace, the Elves and the Goblins put aside their hatred of one another.` (*measured* by the critic: 20 of 49 turns read "grew wary" for a thaw.)

### 7.2 Cool-down (P5-03a critic F2)

A pair whose last turn was less than `turn_cooldown_years` before Y does not turn, unless the contribution list of the turn's direction is non-empty (a fresh cause: lists are cleared at each turn, P5-03a 3.5, so any entry is newer than the last turn). Decay and peace still move the memory; only the band change waits. `turn_cooldown_years` int(0, 100000), shipped **300**, appended to `history.relations`. Late friendships (F3) are not ruled here (17).

## 8. Expulsions of long-settled peoples (item 8; amends P5-03 6.5, P5-03a 5.3)

*Measured* by the critic: 35 of 48 expulsions are of elves, some after 1,280 and 1,696 years as subjects. The elves are the usual subjects because they are the goblins' usual conquest (P5-03's base hatred, −700); that is history, not a defect, and stays. What reads arbitrary is the long hold. Rule: an entry held for at least `expel_settled_years` (Y − its `since_year`) is expelled only when the pair's last turn into hatred (`hatred_event`) is at least Y − `expel_fresh_years`; the test is made before the draw (no draw otherwise). Fields appended to `history.relations`: `expel_settled_years` int(0, 100000) **800**, `expel_fresh_years` int(0, 100000) **150**. The expulsion's causes already cite that turn. A subject people whose ruler's people turned friendly is already kept (expulsion needs hatred).

# Part 3. Wording and visible effects

## 9. Visible effects (item 7)

### 9.1 A share names its size

`spoils_shared`: `{principal}{principal:under} gave {ally} its share of the spoils of {@region}, {share?no captives|one captive|# captives}.` (`spoils_withheld` in 3.6.) The share is people carried off or taken in sacks (P5-04 6.11); nothing moves (P5-04 24 L3).

### 9.2 Arms in a battle won (amends P3-06 7.3, P5-04 7.7, P5-06 7.6)

`AllianceView::arms_event(c)`: when c is a buyer (P5-04 6.13, `arms(c)`), the `arms_sold` event of the smallest-id seller s with an open trade (s, c) whose pair is in the arms-sold set; else 0. Conflict keeps an ordered set of cited arms events. At a battle (`battle`, `battle_at_pass`, `allied_battle`, `battle_led`) won by principal W with a = `arms_event(W)` ≠ 0 not yet cited: the battle's causes gain a (ascending with w); a is cited. So the first battle a buyer wins after each sale names the arms. The four battle templates gain the prefix `{@causes:arms_sold=new-bought arms|With |, }`. Sample: `With new-bought arms, the Sawy Ring defeated the Gub Horde in South Wood; 540 of the defeated and 219 of the victors fell.` The digest's war of a battle stays its `war_declared` cause (the only one of that kind). Naming the forge in the battle sentence needs a site role on four battle kinds and is deferred (17).

## 10. Wording fixes (item 6)

1. **Common enemy in the block** (P5-04 critic F4): 12.3.
2. **The trade clause on war pacts** (F5): an `alliance_formed`'s causes are the cause event of its **basis** reason only (P5-04 6.3 cited every reason present). TR2 is restated on basis 4 alone (15.1).
3. **"The loss of a neighbour's land"** (F5): the cause words of `alliance_formed` for a fear basis name the foe's deeds: `settlement_captured=the conquests of their foe`, `sacked=the sacks of their foe`, `region_ceded=the conquests of their foe`, `settlements_ceded=the conquests of their foe` (replacing `the fall of a neighbour`, `the sack of a neighbour`, `the loss of a neighbour's land` twice); `civ_conquered=the fall of a realm` stays.
4. **Across hatred** (P5-04 critic Point 1): kind 63 gains **`band`**, the band of (r_x, r_y) at the formation (`DispositionView::band`; 2 for one race). The template ends `{foe:against}{band?, though their peoples hated one another||}.` Sample: `After the outbreak of war, the Hold of Durn and the Gub Horde swore to stand together against the Krurz Horde, though their peoples hated one another.`
5. **Undead and great age** (P5-03a critic F5): form **`{m:died}`** (8.5): `died of great age` for a kind with `wake` = `wild`, `crumbled to dust at last` for `wake` = `dead`. `monster_died`: `{monster}{monster:aside} {monster:died}{monster:where}.` Sample: `The Dead of Gik-Kutch crumbled to dust at last in the Forsaken Downs.`
6. **Same-day mixed and standing lines** (P5-03a critic F7): one report line per group, 12.4.
7. **`allies:` lines** and **`; broken`** (P5-04 critic F10; CLAUDE.md): 12.3, 12.5.

## 11. Housekeeping (item 9)

1. **Spoils after a third realm's conquest** (validation finding 1; P5-04 6.11's sentence): z is owed only when the war's ending event is a `peace` or `war_won` whose winner is p, or a `civ_conquered` whose **conqueror** is p and whose fallen realm is e. A war ended by another realm's conquest of e owes nothing (the record is settled with no event). P5-04 6.11's "The enemy's end closes the ally's record as the war's end (so it is owed, V from sacks)" becomes "The enemy's conquest by the principal closes the ally's record as the war's end (so it is owed, V from sacks); any other end of the enemy owes nothing." The engine's `cited` test (`AlliancesSubsystem::spoils`) checks the end event's own winner, or conqueror, against p. Changes a world only where a third realm conquers the enemy of an allied war (1 of 160 reference worlds).
2. **The relation reader checks** of P5-04 9.1 run also in `history::read_history_tables` (the digest's reader), after its relation checks, with the same messages and the same first failing row; both readers report the same first error (as P5-03's `race_participant_problem`).
3. **P5-04 audit A1** (casualty split): the share d_S × F_m div F_S is computed with the project's wide multiply-divide (`core/wide.cpp`), so it never overflows while F_S < 2^62; spec P5-04 14's bound note is restated: dead ≤ force, the product < 2^80. No output change in any reachable world.
4. **A2** (`close_allies`): guarded as `close_allies_at_side_end` is (close only an open row); no `peace` row opens with an ended enemy. No output change.
5. **A3** (`closed_now_`): keyed by (pair, kind, year) and cleared when the year changes, not at every `run`. No output change today; needed now, since 3.4 closes alliances in T6 (no reopening in T6: formations are T2 only).
6. **P5-03a audit A2** (common-enemy entry): when z is at war with several realms of race r, the entry (z, r) keeps the **larger** opening event (the later war). Changes the event a warmer turn cites, rarely. **A3** and **A4** are closed as recorded in P5-03a r2 3.5 and P5-04 6.9 (no change).
7. **The year-0 clock** (P5-03a 17.1): `word_since` uses −1 for "unset". No output change (no two peoples have realms at year 0).
8. **P5-06 audit A1**: the figures subsystem remembers, in an ordered set of event ids, each `succession_crisis` read without a `civ_seceded` citing it, and links a later secession to it in any range (the secession is then handled as 6.5 item 2's split, not by the `on_founded` guard). **A3**: `FigureView::split_bonus_permille(civ, year)` takes the crisis year (`ctx.year()` of peoples' step); the short-reign test compares that year with the current reign's start. (r2) **A2**: a book title's numbered fallback (P5-06 6.8) is refused past `NAME_MAX_BYTES` with the tower loop's fatal message (`names: a name longer than 64 bytes`). None of the three changes output under the shipped order and data.
9. **Report, validation finding 2**: an alliance line's `spoils` part counts the pair's kinds 74 and 75 with id between the row's `since_event` and its `until_event` (exclusive; no upper bound when standing), not by year.

# Part 4. Data, catalogue, report

## 12. History report format v9 (amends P3-07; new section 32)

### 12.1 Version

Header **`# worldgen history v9`** at both levels; `report --help` prints it. The growth rule holds for v9.

### 12.2 `[civilizations]` milestones at summary (item 5; replaces P5-03a 9.2's milestone item)

`  milestones: <n>` with n = the realm's own events (P3-07's `own`) of salience ≥ `SUMMARY_MILESTONE_SALIENCE` (**40**, new constant in `digest.hpp`) that are not `quiet_stretch`; then the **three** of them with the largest salience (then the smaller id), in ascending id, in the milestone line form; fewer when n < 3. An ended realm's short block (P3-07 25.5) the same. No quiet-stretch line at summary. Full is unchanged. When the world has more than `SUMMARY_LINES_CIVS` civilizations, the count line only. *Measured* (critic): 111 of 115 blocks print `milestones: 8` (the cap), and the line shown is a quiet stretch in 93.

### 12.3 `[alliances]` and `allies:` (amends P5-04 10.2, 10.3)

- Alliance line: `  c<a> <name> and c<b> <name>: <from>-<to|E>, <basis part>, e<id>[; renewed <k>]…` with basis part `by origin`, `against c<z> <name>` (basis 1), `in fear of c<z> <name>` (basis 2), `in friendship`, `after trade`; renewed by 6.3; spoils by 11 item 9.
- `allies:` at summary: `; at the end: c<id> <name>, c<id> <name>` (the entity form with the name, as the full lines; it is a block line, so the viewer's browser links it, P4-05).

### 12.4 One line for a same-day group (item 6)

In `[chronicle]` (both levels), consecutive listed events of kind `settlement_mixed` or `standing_changed` of one year whose sentences are equal when the `{settlement}` text is replaced by a fixed marker form one **group**, printed as one line: `<year> e<id>[, e<id>…] <key>/<salience>: <sentence>` with the settlements joined as `A`, `A and B`, `A, B and C` (each with its link, in event order). The summary set and the `shown` count are unchanged (events, not lines); the chronicle's budget is not refilled.

### 12.5 Trade lines

`; broken e<id>` when the row's `until_event` is a `trade_broken`; `; ended e<id>` otherwise (a side's ending event).

### 12.6 `[stats]` (full; appended)

`alliances_renewals`, `alliances_renewals_written`, `alliances_soured` (kind 75 with `grievance` 1), `alliances_spurned_at_once` (kind 76 `reason` 2), `allies_refused_excused` (kind 69 `count` 0), `refusal_motive_0` to `refusal_motive_7`, `betrayal_motive_0` to `betrayal_motive_3`, `battles_with_arms` (battle kinds citing an `arms_sold`), `dispositions_thawed` (kind 58 `change` 1).

### 12.7 Length

At summary, per realm: +3 milestone lines, −1 quiet stretch where there was one; about +20 a default world outside `[chronicle]` (P5-02 12.9's budget gives back as many chronicle lines where L binds). Groups (12.4) shorten the chronicle. (r2) 21.5 adds no line: `[figures]` keeps `SUMMARY_FIGURES_MAX` and `[monsters]` its four lines; the `quiet` clause of `hero_rose` lengthens a sentence, not the count. **Bound**: default median ≤ **670** (P5-06's 650 + 20); every world ≤ L(C); `civs4` median ≤ 1,050 (unchanged: the milestone lines are dropped above `SUMMARY_LINES_CIVS`). (r2) It still fits: 0.27.0 measured a default median of 613.5 and 603.5 lines (max 902 and 785) and `civs4` 956, so +20 gives about 634, inside 670 with 36 to spare; `civs4` gains no milestone line.

## 13. Dwarves' participation (item 10; amends P5-04 15.1 P)

*Measured*: dwarves 89% and 87% (other sweeps 80% to 87%); every other race 94% to 100%. Reason (one line, not a tuning gap): reach for alliance and trade is open-border hops (P5-04 6.2), and a dwarven realm sealed in its uplands by borders with no walk pair and no pass (P5-03a 4.1; seeds 4 and 6) reaches no one; P5-03a lets such peoples **meet** by word, not trade. The world critic reads the reserve as character. Restated: participation ≥ 90% per race, **dwarves ≥ 85%** at 1024 defaults; sealed realms are reported. Contact across closed borders and by sea comes with roads and ships in P6 (Paul, 2026-11-24).

## 14. Data and catalogue

### 14.1 `history.alliances` (`game/data/history/alliances.toml` and both fixture roots)

Changed: `withheld_to_break` 1 → **2** (3.7, meaning changed), `withheld_grudge` 400 → **250**. Appended after `mine_forms`, in this order, all required: `spoils_large_share`, `spoils_large_part_permille`, `soured_renew_permille`, `soured_join_permille` (3.7); `weary_years`, `join_weary_permille`, `join_busy_permille`, `join_fear_floor`, `join_warmth_floor`, `grasping_join_permille`, `join_claim_permille`, `excused_memory`, `refuse_grudge` (4.4); `betray_cooled_disposition`, `betray_weak_permille` (5). The `alliances-small` root: `spoils_large_share` 1, `soured_renew_permille` 0, `weary_years` 0 (so every kind and motive occurs); the `zero` root: shipped values (inert without alliances, I0).

**Defaults in code** (a tuning built in code keeps 0.26.0's meaning): `spoils_large_share` 1000000, `spoils_large_part_permille` 1001, `soured_renew_permille` 1000, `soured_join_permille` 1000, `weary_years` 0 with `join_weary_permille` 1000, `join_busy_permille` 0, `join_fear_floor` 0, `join_warmth_floor` 0, `grasping_join_permille` 1000, `join_claim_permille` 1000, `excused_memory` 0, `refuse_grudge` 0, `betray_cooled_disposition` 0, `betray_weak_permille` 0. (The refusal motive and the excusing are rules, not tunings: they act at these defaults too.)

### 14.2 `history.relations`

Appended after `expel_gap_years`: `turn_cooldown_years` 300 (7.2), `expel_settled_years` 800, `expel_fresh_years` 150 (8). Code defaults 0, 100000, 0 (inert). The `zero` root: shipped; `relations-small`: `turn_cooldown_years` 0.

### 14.2a `history.figures` (r2; `game/data/history/figures.toml` and both fixture roots)

Changed: the `epithets` entry `the Old` (`reign_permille`): `min` 1300 → **1400**, `substantial` 1450 → **1480** (21.1); with the lead's choice of 21.3, `skill_min` 500 → 600, `skill_max` 800 → 900 and `temper_allied_permille` grasping 2000 → 3000. Appended after `epithet_repeat_max`, in this order, required: `tower_spacing_permille` int(0, 1000) **30** (21.4), `hero_return_years` int(1, 100000) **100** (21.5). Code defaults (0.27.0's meaning): 0 and 100000. The `zero` root: shipped values (inert: no casters, no heroes).

### 14.3 `history.report`

The templates of 3.6, 4.4, 5, 6.2, 7.1, 9.1, 9.2 (four battle kinds, P5-06's `battle_led` included), 10 items 3 to 5, and (r2) `hero_rose` (21.5). Cause words added to an existing list are appended after its last entry in the order this spec gives; 10 item 3 replaces four words in place. Saliences unchanged.

### 14.4 Catalogue (no DDL change)

| Kind | Values appended, in seq order |
| --- | --- |
| 58 `disposition_turned` | `change` (0 to 3) |
| 63 `alliance_formed` | `term` (≥ 1), `band` (0 to 2) |
| 67 `alliance_betrayed` | `motive` (0 to 3), `years` (0 to E − 1) |
| 69 `ally_refused` | `motive` (0 to 7), `count` (0 to 100) |
| 75 `spoils_withheld` | `grievance` (0 to 2) |
| 76 `alliance_spurned` | `years` (0 to E − 1); `reason` 0 to **2** |
| 87 `hero_rose` (r2) | `quiet` (0, or `hero_return_years` to E − 1) |

`event_kind_value` gains ten rows; every event of these kinds carries the new values. `schema.md`'s catalogue listing is updated by the engineer (the code lists of `schema.md` are unchanged). **Reader checks** (both readers, P5-03 16.4's form `corrupt table 'event': row <e>: value <k> <v> outside <a> to <b>` for every range above), and: an `ally_refused` with `motive` 7 cites a `spoils_withheld` (`refusal over spoils without its share`); an `alliance_spurned` with `reason` 2 cites a `spoils_withheld` of its year whose `grievance` is 2, unless it waited for a quiet budget (`years` > 0 then) (`spurned at once without its share`); an `ally_refused` with `count` 0 has `motive` 1 or 2 (`excused refusal with motive <m>`). (r2) `hero_rose`'s `quiet` is range-checked only (0 to E − 1; the world does not carry `hero_return_years`).

## 15. Targets and calibration

### 15.1 Targets (1024 defaults, seeds 0–99 and 100–199)

Every target in force for this stage, with its bound. *Measured* columns: 0.26.0 (P5-04 validation; r1's column) and 0.27.0 (`docs/reviews/p5-06-figures-validation.md` 4; "—" where that sweep did not measure it). Validation measures every row on both samples.

| Target | Definition | Bound | 0.26.0 | 0.27.0 |
| --- | --- | --- | --- | --- |
| **AL6** | refusals | `ally_refused` ≤ `ally_joined_war` on **both** samples; excused refusals reported | 687 ≤ 736; **746 > 698** | — |
| **AL6b** | refusal motives | no motive above 50% of refusals; motive 0 ≤ 35% | — (no motive) | — |
| **AL5** | wars with an ally; joins | 20% to 45%; defence ≥ 70% of joins; attack ≥ 10% of joins | 25.8%, 25.0%; defence 82%, 79%; attack 18%, 21% | 25.9%, 27.5%; joins — |
| **AL1** | alliances formed; standing | median 6 to 15; ≥ 3 | 14, 13.5; 9, 9 | 14.5, **15** (at the edge); 9.5, 9 |
| **AL7** | betrayals | as P5-04 (20% to 60% of worlds; 3% to 15% of ended) | 50, 46 worlds; 14.5%, 13.1% | — |
| **AL8** | breaks after refusals | `alliance_broken` reason 0 reported; ≤ half of counted refusals | — | — |
| **SP1** | shares granted | 500 to 850 per mille (P5-06's temper term included) | 680, 649 | — (just 91.7%, 95.7%; grasping 40.0%, 39.5%) |
| **SP2** (restated) | alliances spurned | at least one in **10% to 35%** of worlds; at most **6%** of ended alliances | 28, 25 worlds; 5.3%, 5.4% | — |
| **SP3** | cause and effect | by rule: a spurning `reason` 2 is in its share's year (or states `years`); every other states `years` | 1 to 106 years, unstated | — |
| **SP4** | large shares; ends | 30% to 50% of withheld shares on open alliances are large; ≥ 40% of those withholdings end their alliance (at once, at renewal or by cooling) | — | — |
| **RN1** | written renewals | ≤ 35% of `alliances_renewals` | 100% (538 in 16 worlds) | — |
| **RR2** | P5-03a's turns | into friendship ≥ 25%; ≤ 20 turns a world; ≥ 55% before 5500 | 53%, 54%; —; 61%, 59% | 54%, 55%; —; — |
| **RR6** | flips | pairs with four or more turns in a world: in ≤ 5 worlds of 100 | — (critic: 4 pairs in 16 worlds) | — |
| **RR4** | friendship at E | median ≥ 3 | 4, 4 | — |
| **MX3** | expulsions | median ≤ 3; none of an entry held ≥ `expel_settled_years` without a turn into hatred within `expel_fresh_years` (by rule); the share of elves reported | median 2 to 3; elves 35 of 48 (critic) | — |
| **TR2** (restated) | trade-based alliances | basis 4 ≥ 15% of formed | 23.1%, 20.7% | — |
| **P** (restated) | participation | ≥ 90% per race; dwarves ≥ 85% | dwarves 89%, 87% | — |
| **C1** | wars per world | median 22 to 28 | 23, 24 | 25, 25 |
| **T16** | repeated battles | ≤ 2 per thousand | 1.44, 1.18 | 1.19, 1.68 |
| **G2** | standing settlements; population at E | medians ≥ 81 and ≥ 205,000 | 83/208,364, 88/222,506 | 85.5/218,172, 88/211,371 |
| **T18** | summary lines | default median ≤ 670; every world ≤ L(C); `civs4` median ≤ 1,050 | 577, 578 | 613.5, 603.5 (max 902, 785; none over L(C)); `civs4` 956 |
| **M4a** (guard) | a living monster at E | ≥ 85 on each sample (Paul) | 77, 81 | 92, 93 |
| **M4b** (guard) | a lair on a former settlement open at E | **not below 0.27.0**: ≥ 63 on seeds 0–99 and ≥ 68 on 100–199; Paul's bound 65 stands, its reading as an average is Q-P5-06-1 (open, not blocking) | 58, 55 | 63, 68 |
| **M** | monsters slain; overruns; towns | median ≤ 4; 2 to 4; ≤ 10% | — | 4, 4; 3, 3; 4.4%, 6.5% |
| **UD2, UD3** | undead (regression) | as P5-06 15.1 | — | 77/2, 85/2; 47.5%, 45.6% |
| **CB1, RU1** | calamities; ruins (regression) | as P5-06 15.1 | — | 7, 7; 0 faded |
| **FG1** (21.3) | figures per world | median 60 to 120; max: the lead's choice (21.3); rulers ≥ 50% | — | 77/226, 79/208; 65.5%, 66.6% |
| **FG5** (21.2) | slain by heroes | the lead's choice (21.2); heroes median 2 to 8; a hero in ≥ 80% of raided worlds | — | 27.9%, 27.8%; 2, 2; 90%, 88% |
| **FG7** | casters, towers, books | as P5-06 15.1 (median 2 to 6; ≥ 80% of worlds; 3 to 10) | — | 6, 6; 99%, 100%; 8, 8 |
| **FG8** (21.2) | lives through a quiet phase | the lead's choice (21.2); age ratio ≥ 4 | — | 20.3%, 20.2%; 8.0, 7.5 |
| **FG11** (21.1) | epithets | no world over max(2, figures div 10) bearers of one text on **both** samples; 15% to 40% of figures with an epithet | — | 0, **1** (`the Old`); 17.5%, 16.9% |
| **FX1** (21.3) | larger skill wins | 55% to 70% on each sample, or the lead's restated bound | — | 55.7%, **54.3%** |
| **FX3** (21.3) | grasping betrayals | ≥ 1.5 × their reign share on each sample, or the lead's restated bound; just − grasping shares ≥ 20 points | — | 1.53, **1.27**; 52, 56 |
| **TW1** (21.4) | towers end | towers and libraries ended at E: ≥ 15% of all kind 4 sites, pooled per sample (an estimate, not measured; the lead may restate it from the first sweep); by ending reported | — | 0% (critic: all open) |
| **TW2** (21.4) | towers apart | by rule: no two kind 4 sites of one realm within the spacing; checked by the invariant | — | critic: three within 3 cells |
| **HR1** (21.5) | long quiets stated | `hero_rose` with `quiet` > 0 reported (count, median years) | — | critic: gaps of 234 to 825 years unstated |
| **timing** | `generate` seed 0 1024 release | at most 1 s over 0.27.0 (25.6 s user) | — | — |

**Why SP2 falls.** At 0.26.0 nearly every withholding (28 of about 38) led to a spurning, because one kept share ended the alliance at its renewal and a chill of 500 broke a cool one. Under Paul's rule a first, small share only sours: it ends the alliance at the next renewal half the time (`soured_renew_permille` 500) if the alliance lives that long, or by cooling. With about 0.4 withheld shares a world on open alliances, 30% to 50% of them large, and half the soured ones surviving to their renewal, spurnings come to about 0.4 × (0.4 + 0.6 × 0.5 × 0.5) ≈ 0.22 a world, in about 15 to 25 worlds of 100, and below 5% of ended alliances. Raising `spoils_large_*` or lowering the grant chance to reach 20 worlds would make keeping the usual outcome again, against "it might sour".

### 15.2 Calibration (data only, the engineer, in this order)

1. AL6, AL6b, AL5 by `join_warmth_floor` and `join_fear_floor` (raise both to bring joins up), then `join_busy_permille`, `join_weary_permille`, `weary_years`; AL8 by `refuse_grudge` and `refusals_to_break`.
2. SP4 by `spoils_large_share`, `spoils_large_part_permille`; SP2 by `soured_renew_permille`, `withheld_grudge`; SP1 checked (P5-06's `temper_spoils_permille` is not moved here).
3. RN1 is by rule (no lever).
4. RR2, RR6 by `turn_cooldown_years`; RR4 re-checked.
5. MX3 by `expel_settled_years`, `expel_fresh_years`.
6. AL7 checked (`betray_*` change wording only); C1, T16, G2, M after each step (a refusal's grudge and more joins move strain and wars).
7. (r2) FG11 by `the Old`'s `substantial` (21.1), then FG11's lower share checked.
8. (r2) The lead's choices of 21.2 and 21.3: FX1 by `skill_min`, `skill_max`; FX3 by `temper_allied_permille` grasping (AL7 checked); FG5 only if the lead chose the retune, by `hero_hunt_permille` (M, M4a, M4b checked at each value).
9. (r2) TW1 by `tower_spacing_permille` only if TW1 fails (spacing moves which cells towers take, not their ends); FG7 checked.
10. M4a, M4b, AL1 and T18 after every step; a step that moves M4a below 85 or M4b below its guard is undone and reported to the lead.

## 16. Tests required

C++ (shards: `-alliances` for 3 to 6, 9.2 and 11 items 1, 3 to 5; `-relations` for 7, 8, 11 items 6, 7; `-history-report` for 12; P5-06's figures shard for 11 item 8):

1. Data: each new field's bounds and absence; the changed values; the code defaults reproduce 0.26.0 on the fixtures' alliance histories except for the motive values (I0 unchanged).
2. Spoils: large by share and by part at the bound ± 1; the second withholding at `withheld_to_break`; spurned at once in the same step with `grievance` 2, `reason` 2, `years` 0, causes; the Q4 wait (`years` > 0); souring sets the record and both flags; origin and closed alliances give `grievance` 0; the soured renewal draw both ways (spurned `reason` 0 with `years`; renewed, flag cleared); the soured call factor once; cooling with and without a soured record.
3. Refusals: each factor alone and its motive; ties to the smaller code; motive 0 only with all factors at 1000; excused (1, 2) and counted; the break at `refusals_to_break` counted refusals only; the grudge; the causes by motive; origin silent; attack chance with f1, f2, f5.
4. Betrayals: each motive in order; `years`.
5. Renewals: the chain without drift across a quiet step; Q4 at Y ≥ d; each notable kind; a quiet renewal writes nothing and credits with R; a notable one without budget renews quietly; `term`; the report's count with a closing in a due year's step.
6. Turns: `change` for each band move; the cool-down at `turn_cooldown_years` − 1 and at it; a fresh contribution inside it.
7. Expulsions: held at `expel_settled_years` − 1 and at it, with the hatred turn inside and outside `expel_fresh_years`.
8. Arms: the first battle won after a sale cites it, the second does not; a battle lost does not; a new sale cites anew; each battle kind.
9. Housekeeping: a third realm's conquest of the enemy owes nothing (the validation world `civs4` seed 6 as a slow witness); the relation messages in both readers; the wide split at F_S near 2^40; `close_allies` with an ended enemy; `closed_now_` by year; A2's larger event; `word_since` −1; P5-06 A1 (a secession read in a later range) and A3.
10. Rendering: the eight-word choice (each value, a value above 7, negative) and its `template_problem`; `{m:died}` for wild and dead; every changed template on a fixture event.
11. Reader: each range and each message of 14.4 on a corrupt fixture.
12. Report v9 on fixture **H17** (`h17-v9.toml`, from H16: a soured alliance renewed and one spurned at renewal, a spurning at once, an excused and two counted refusals with a break, a betrayal with motive, a notable and two quiet renewals, a thaw and a decline, four same-day mixed events in two groups, a realm with five milestones of salience ≥ 40, a trade ended by a realm's end), both levels.
13. Invariants (default: seeds 0–9 at 512; **slow**: 0–19 at 1024 and `civs4` 0–4): P5-04 13 with item 11 as amended (11 item 1) and item 12 replaced by: every `alliance_spurned` cites a `spoils_withheld` of its pair with `grievance` ≥ 1 after the alliance's formation; `reason` 2 in the share's year unless `years` > 0; plus: no alliance has three `spoils_withheld` with `grievance` ≥ 1; every `ally_refused` `motive` matches the replayed factors; no `alliance_renewed` without a notable event since its last written renewal; every turn within `turn_cooldown_years` of the pair's last has a contribution of its direction; 8's rule on every expulsion.
14. I0 (P5-04 7.14, P5-06 7.11) and resume (P3-04 16.3).
15. (r2) `the Old`: a ruler at `reign_permille` 1479 and 1480 (ordinary and substantial), with two earlier bearers.
16. (r2) Towers (shard `-figures`): a tower's cell keeps the spacing from an open and from an ended kind 4 site of its realm at the bound − 1 and at it, and ignores another realm's; no free cell: no rise and no figure; each ending (5 before 4; a library's settlement ruined, captured; a wild tower's region lost) with its event and year; a caster whose tower ended writes no book and causes no blight; `tower_spacing_permille` 0 reproduces 0.27.0.
17. (r2) Report (`-history-report`): `[figures]` reserved picks (a world with one figure of each role; ties within a role by reign, books, id; a role with no figure); `[monsters]` with 3 and with 5 living monsters; `hero_rose` with `quiet` 0, at `hero_return_years` − 1 and at it, and with no earlier raid; a ruler's summary line without `reign <n>` if the lead takes L-3.
18. (r2) Reader: `ending 4` and `5` accepted on a tower, `ending 6` refused with the widened message; a tower `ending_event` not of its `ended_year` refused; `quiet` outside 0 to E − 1.
19. (r2) Invariants (with item 13): every kind 4 site opened at a cell more than the spacing from every earlier kind 4 site of its realm; every ended tower's ending event is that of 21.4's table at its ended year; every `book` row's year ≤ its site's `ended_year` when ended; every `hero_rose` `quiet` equals 21.5's g when g ≥ 100, else 0.

Reference (`p5_04_alliances_reference.py`, `p5_03_relations_reference.py`, `p5_06_figures_reference.py`): 2 to 8 and (r2) 16 in the replay; 13's and 19's invariants in `check-world`. Test-side items carried (from validation, for the test writer): `p3_conflict_reference.py` r16 knows `forgive` at `alliance_formed` (validation finding 3); `p5_03_relations_reference.py` invariant 4 accepts a same-year `settlement_mixed` (seed 66) and (r2) the colony founded in the year its region is ceded (21.7 item 2); `p5_04` invariant 11 as amended; (r2) `p4_browser_reference.py` figure search lines (21.7 item 3). Goldens: every world file and every history golden is re-recorded once after the cross-preset comparison (as P5-03a 14); `core_vectors.txt`, `p1-0*`, `p3-02-names.txt`, `p3-03-chronology.txt` must not move; `world-hashes.sh` keeps its cases.

## 17. Left out, with reasons

| Item | Why | Where |
| --- | --- | --- |
| The forge named in a battle ("with arms from the Mines of Girin") | needs a site role on four battle kinds; 9.2 gives the cause and a clause | a later alliances round |
| A share naming the sacked town (F9) | kinds 74 and 75 have no settlement column flag; a share stays a count of people | with wealth (P5-04 20) |
| A mine as a ruin or a lair when its realm falls (F6) | P5-07 Mountain depths owns mines' ends | P5-07 |
| A line for claims set aside at an oath (F11) | one more event per formation for little story | not planned |
| "The dwarves would not swear to the goblins" (F7) | a refusal to ally is a non-event; the reserve reads as character (13) | not planned |
| Late friendships (P5-03a F3) | the cool-down (7.2) stops flips; RR2's "before 5500" is met (61%, 59%) | not planned |
| Word carried by a third people (P5-03a F8); the ruins list by kind and age (F9); late secessions (F12) | report and peoples rounds of their own | later report round |
| Same-war same-year battles grouped (P5-04 F8) | three battle lines of one war are a war's course; T16 is met | not planned |
| (r2) A tower that ends when its line of casters lapses (P5-06 critic F1) | needs a new `site.ending` code (7), a code-list change that P5-03 and P5-04 took with a schema bump; 21.4's endings by realm and land use existing codes | lead question L-1 (23) |
| (r2) A new caster taking over a standing tower (critic F1's "a new master") | needs an event kind for the taking (a caster's start event); spacing (21.4) removes the pile without it | P5-09 or P6 |
| (r2) More epithet texts for age and betrayal (`the Grey`, `Oathbreaker`; critic F2) | data only, but it raises FG11's share and adds texts the critic must read; 21.1 is the lead's ruling | a later data round |
| (r2) "The hunt failed" lines (critic F3) | a failed hunt writes nothing at 0.26.0 by design; one more event per hunt-year | not planned |
| (r2) Undead name forms (critic F7); library glyph size (critic F6) | data round; viewer | later rounds |

## 18. Determinism notes

One new keyed stream (`hist.alliances.renew`, key of ids and a due year), drawn at most once per (alliance, due year). Every new choice is a minimum over a fixed code order (4.2, 5) or a test of values, read live in fixed passes; the arms set and the closed set are ordered sets of ids (and years). Arithmetic int64 with checked multiplication: the defence chance is nine factors ≤ 1000 each divided at every step; 3.3's tests compare values < 2^31; the casualty split by the wide multiply-divide. `FigureView::temper` is a const read of the reign at the step's start. (r2) Tower spacing is a Chebyshev distance on integer cells against the kind 4 sites of the realm in id order; the radius is `tower_spacing_permille` × map width div 1000 (floor); the height's order (P5-02 6.6.1: largest elevation, then smaller y, then smaller x) is unchanged, so the result is a function of stored state. Tower endings read stored events (ruin, owner and ending events) in ascending site id; no draw. `quiet` of `hero_rose` is a difference of two event years. The figure and monster selections of 21.5 are stable sorts on stored values with an id tie-break. No floating point, no unordered container.

## 19. Amendments

| Spec | Section | What | Note |
| --- | --- | --- | --- |
| P5-04 | 5, 6.3, 6.5, 6.6, 6.7, 6.11, 7.1, 7.13, 7.15, 8.1, 8.3, 10.2, 10.3, 13 items 11, 12, 14, 15 | 3 to 6, 9, 10, 11 items 1 to 5, 12, 13 | r4 pointer |
| P5-03 | 6.2.3, 6.5, 8.1, 8.2 | 7, 8 | r4 pointer |
| P5-03a | 3.3, 3.5, 4.2, 5.3, 9.2 | 7, 8, 11 items 6, 7, 12.2 | r3 pointer |
| P5-06 | 6.4 rule 3, 7.1, 7.4, 8.2 (battle templates) | the deed `ally_refused` motive 5; `temper`, `split_bonus_permille(civ, year)`; A1; 9.2's prefix | r5 pointer |
| P5-06 (r2) | 4.4, 6.8, 6.10, 7.8, 8.1 (kind 87), 8.2 (`hero_rose`), 9.4 (`site` kind 4), 10.2, 13 items 9 and 16, 15.1 (FG1, FG5, FG8, FX1, FX3 as the lead rules) | 21; A2 | r5 pointer |
| P5-02 (r2) | 6.6.1 (the height with cells kept clear), 12 (`[monsters]` summary order) | 21.4, 21.5 | this spec |
| `schema.md` (r2) | `site` (kind 4 endings 4 and 5), catalogue | 21.4; ten value rows | the engineer |
| P5-02 | `monster_died` template | `{m:died}` | this spec |
| P3-04 | 8.4, 8.5 | choice of 2 to 8 words; form `died` | this spec |
| P3-06 | 7.3, 7.13 | `has_claim`, `claim_event`, `last_war_end`; the arms cause; A1, A2 | this spec |
| P3-07 | new 32 | v9 (12) | this spec |
| `schema.md` | catalogue | nine value rows; kind 76's `reason` range | the engineer |

8.5 (here): **the choice form** (amends P3-04 8.4 and P5-03 8.2): 2 to 8 words; with n ≥ 5 words, word i for value i (0 ≤ i < n − 1) and the last word for every other value (negative included); `#` renders the value as in the three-word choice. `template_problem`: `choice of <n> words at byte <b>` above 8. **The form `{m:died}`**: 10 item 5; `template_problem` as P5-03a 7.1's monster forms.

## 20. Proposals and questions

Entry "History: P5-04a relations and alliances follow-ups (readings)" in `docs/design/proposals.md`. One question for Paul, `docs/questions-for-paul.md` Q-P5-04a-1 (SP2 restated), decided by Paul on 2026-11-25 ("I'm good with the way that rule plays out"). r2's open points are in 23.

# Part 5. Carried from P5-06 (r2)

## 21. Carried from P5-06 (item 11; amends P5-06 r4 as listed in 19)

The lead's ruling "P5-06 figures as validated" (proposals, 2026-11-25) hands these to this stage. Measurements are 0.27.0's (P5-06 26.3; A = seeds 0–99, B = 100–199).

### 21.1 Epithet `the Old` (FG11; P5-06 15.2 step 11)

*Measured*: one world of B has `the Old` on more than max(2, figures div 10) figures; bearers of one text reach 8 in both samples; `the Old` is the commonest epithet (316 and 313 bearers in 100 worlds). A reign is 0.5 to 1.5 generations (P5-06 6.3), so `reign_permille` ≥ 1450 holds for about 51 of 1,001 drawn lengths (5%), before crises and the present cut reigns short.

Rule (data, `history.figures.epithets`, the entry `reign_permille`): `min` 1300 → **1400**, `substantial` 1450 → **1480** (about 21 of 1,001 drawn lengths, 2%). No per-world cap is added (Paul: "regardless of how many bear it"). Calibration: if a world of either sample still exceeds FG11's bound, raise `substantial` by 10 up to 1500 (a reign longer than 1.5 generations only by a death that waited for a quiet step's budget); FG11's lower bound (15% of figures with an epithet; 17.5% and 16.9% at 0.27.0) is checked at each value. Still over at 1500: the lead decides by P5-06 22 lead item 2 (a per-world bound `epithet_substantial_max`, or the question back to Paul).

### 21.2 Heroes and lives through quiet phases (FG5, FG8; the lead's choice)

**FG5**, monsters slain by heroes: 27.9% and 27.8% against 30% to 70%. A retune (`hero_hunt_permille` 20 → 25, then `hero_might_permille` 1000 → 1200) raises slayings by heroes but also slayings in all, while M's slain median is already at its bound (4, 4) and M4b has no room (63 against the guard 63). **Recommended: restate the share to 25% to 70%**, with the reason that the hero levers trade directly against the living monsters Paul named a highlight of play (M4a, M4b) and against M's bound, while heroes already rise in 88% to 90% of raided worlds (FG5's other parts met). If the lead prefers the retune: 15.2 step 8, stopping at the first value that breaks M, M4a or M4b.

**FG8**, figures alive through a quiet step: 20.3% and 20.2% against 30%. Validation measured "at least 25 years of a quiet phase inside the figure's life"; the spec never defined it. **Definition** (for every sweep from 0.28.0): a figure is *alive through a quiet phase* when some quiet phase of the `phase` table (P3-04 9) has its first year ≥ the figure's `office_from` and its last year < `died` (or the figure is alive at E). The share depends on the race mix (goblin, orc and human figures live 40 to 70 years), not on a lever of this stage (lifespans are race data that peoples also reads). **Recommended: restate FG8's second part to ≥ 15% of all figures and ≥ 30% of elven and faerie figures**, both by the definition above, first measured at 0.28.0; if the race part fails on its first measure, the lead rules again.

### 21.3 Figures per world, skill and grasping betrayals (FG1, FX1, FX3; the lead's choice)

- **FG1**, most figures in one world: 226 and 208 against 200 (medians 77 and 79 inside 60 to 120). Figures grow with realms and wars (`turbulent` median 126, `civs4` 260); the maximum is one world each. `exile_permille` (P5-06 15.2 step 1) moves about one figure a world and would not bring 226 under 200. This stage adds a few rulers named by a grasping refusal (4.4). **Recommended: restate the default maximum to ≤ 250**; the median bound stands.
- **FX1**, the larger skill wins: 55.7% and 54.3% against 55% to 70%. **Recommended: retune** `skill_min` 500 → **600**, `skill_max` 800 → **900** (data; the spread stays 300, so the gain is in battles with one general; C1, AL5, T16 and G2 checked). Fallback: restate to ≥ 53% on each sample.
- **FX3**, betrayals by grasping rulers over their share of reigns: 1.53 and 1.27 against 1.5 (granted shares 52 and 56 points apart, met). **Recommended: retune** `temper_allied_permille` grasping 2000 → **3000** (P5-06 7.3; betrayal is strain toward an ally), with AL7 (20% to 60% of worlds, 3% to 15% of ended alliances) and C1 checked. Fallback: restate to ≥ 1.25 on each sample.

### 21.4 Towers apart, and towers that end (P5-06 critic F1)

*Measured* by the critic: every tower is open at E in all 16 worlds, and a realm founds towers on or beside its own standing one (default seed 2: three of c1 within 5 cells, years 3216 to 5284). Cause: the height of the seat region (P5-02 6.6.1) skips only the cells of open sites, so each new tower takes the next-highest cell beside the last.

**Spacing** (amends P5-06 6.8's rise). Let W be the map's width in cells and D = `tower_spacing_permille` × W div 1000 (shipped 30: D = 30 at 1024, 15 at 512, 61 at 2048). For a tower (not a library), the cell is `MonstersSubsystem::height_cell(ctx, R, avoid)`: P5-02 6.6.1's height of R among the cells that are not taken **and** lie at Chebyshev distance > D from every cell in `avoid`, the cells of every site of kind 4 whose `founder_civ` is c, open or ended, in site id order. D = 0: `avoid` is empty (0.27.0's rule). No such cell: no rise (the `hist.figures.caster` draw is already made; no figure, name, site or event). A library is unchanged (at its city; one library per settlement ever: a settlement that has had one never gets another). Other realms' towers do not repel (a realm may build near a rival's tower).

**Endings** (amends P5-06 6.8 "A tower stays open", 7.8, 9.3). In `FiguresSubsystem`'s T3 (after the log, before the rises), T6 and Q4 (after the log), each open site of kind 4 ascending by id, of realm c, at settlement t (0 for a tower in the wild), in region R, ends when, tested in this order (as P5-04 6.12's mines, with no event of its own):

| `ending` (existing codes) | when | ending event |
| --- | --- | --- |
| 5 the founder's realm fell | c has ended | c's `ending_event` |
| 4 the founder lost the land | library: t is not standing, or t's owner is not c; tower in the wild: R's owner is not c | library: t's `ruin_event` when not standing, else the event that last changed t's owner (`Settlement::owner_event`); tower: R's `owner_event` |

`end_site_seam(site, year of that event, that event, ending)`; the ended year is the ending event's own year (P5-04 6.12 r3). (r3, as built) **An ending event older than the tower** (event 0, or of a year before the tower's `origin_year`: a tower raised in a region its realm did not own) does not end it: the tower waits for the next change of owner, so a tower never ends before its origin year (impl notes reading 2; the reference skips such an ending as `FiguresSubsystem::tower_endings` does). (r3) **The owner is tested at each check** (T3, T6, Q4), not over the interval between checks: a region or library settlement lost and won back by the realm inside one quiet step leaves the tower open (audit A2; deterministic, a function of the schedule; the rule means "not the realm's at the check", not "ever lost"). A caster whose tower has ended lives on, writes no more books and causes no blight (6.8's loops skip it; its tallies stand); its realm raises no new caster while it lives (unchanged). Readers: the `[sites]` line prints `; ended <year> e<id> lost|fell` by the existing ending words (P5-02 12.5; no report change); `Timeline::sites_existing` drops an ended tower from its ended year, so the marker goes (as a mine's); the browser and the timeline need no change. P5-06 9.4's site check for kind 4 becomes `ending <k> is not 0, 4 or 5 for a tower`, and the mine's check that `ending_event` is an event of `ended_year` applies to kind 4. `schema.md`: kind 4's `ending` "0 open, 4 the founder lost the land, 5 the founder's realm fell" (was "0 (towers do not end)"). Invariant P5-06 13 item 16 gains: two kind 4 sites of one realm are more than D apart (Chebyshev) when the later one was opened (a library at its city excepted as the later one).

**Not here**: an ending when the line of casters lapses (needs a new `site.ending` code, L-1) and a new caster taking over a standing tower (needs an event kind; 17).

### 21.5 Summary blocks and the `hero_rose` sentence (P5-06 critic F3 to F5, F8)

1. **`[figures]` at summary** (amends P5-06 10.2). The shown set S, at most `SUMMARY_FIGURES_MAX` (6): first the **reserved picks**, in the role order ruler, hero, caster, general: for each role with at least one figure, the figure of that role with the largest renown, ties broken for rulers by the longer reign ((`died`, or E when alive) − `office_from`), for casters by more books, then by the smaller id; claimants have no reserved pick. Then the other figures with renown ≥ 1 by (renown descending, id ascending) until |S| = 6. S is printed in (renown descending, id ascending) order, then `  and <x> more`. Full is unchanged. No line is added.
2. **`[monsters]` at summary** (amends P5-02 12 and P5-03 10.2's selection). The living monsters (status 1 or 2) by (hoard descending, id ascending), at most `SUMMARY_MONSTER_LINES` + `SUMMARY_MONSTER_ENDED_LINES` (4); then the ended notable monsters by harm as now, at most `SUMMARY_MONSTER_ENDED_LINES` (2) and at most 4 − (living shown). So every living monster is shown before any ended one, and the block keeps at most 4 lines and `and <x> more`. (As built at 0.27.0 the living already come first, but at most 2 of them: a third living monster fell into `and <x> more` behind two ended ones, which is the critic's reading.)
3. **`hero_rose` states a long quiet** (critic F4, F3). For the rise read from raid r of monster m: p = the `monster_raid` of m with the largest id below r (any realm); g = year(r) − year(p), and no p: g = 0. Kind 87 gains the value **`quiet`** = g when g ≥ `hero_return_years` (shipped 100), else 0. Template: `{quiet?After the raids of |After the raids of |When, after # quiet years, }{monster}{quiet?|| raided again}, {figure} of {civilization} took up the hunt.` Sample: `When, after 234 quiet years, the Brood of Skarr raided again, Gnit of the Snoskrag Mob took up the hunt.` The cause stays r.
4. **`reign <n>` on short-lived lines** (critic F8, proposed; L-3). At summary a ruler's figure line prints `reigned <from>-<to|E> (<temper word>)` without `reign <n>`; full keeps `reign <n>`; the `rulers: <n> reigns, <k> recorded` line is unchanged (a count reads as a count). The browser's figure block keeps the full form. Interim reading: built.

### 21.6 Guard targets (P5-06 25.3; Q-P5-06-1)

M4a ≥ 85 worlds on each sample; M4b not below 0.27.0's 63 (A) and 68 (B), Paul's 65 standing while Q-P5-06-1 (whether 63 and 68 meet it as an average) is open with him. Both are in 15.1 and checked after every calibration step (15.2 step 10). A rule of this stage that lowers either is reported to the lead with the step that did it; noise of one or two worlds is the lead's to read.

### 21.7 Housekeeping

1. **P5-06 audit A1 to A3**: 11 item 8.
2. **`p5_03_relations_reference.py check-world` invariant 4** (P5-06 validation D2; CLAUDE.md's stale check): fails on `civs4` seeds 10, 13 and 22 (`colony N's first rows ... are not its founder's race`; seed 10's colony 145, founded in 4733, in a region ceded that year). The test writer reproduces seed 10 and confirms from P5-03 6.4's rule which side is wrong; the interim reading is that the check is stale: a colony whose owner changed in its founding year (a `region_ceded`, `settlements_ceded` or `settlement_captured` of that year naming it or its region) may have first rows of its owner's race at that year's end. If the engine is wrong instead, it is a generator defect for the lead.
3. **Golden coverage of the figure search** (P5-06 11.3: `search` finds figures by display name after regions): `p4_browser_reference.py` and `p4-05-browser.txt` gain, per case, the result of `BrowserIndex::search` for (a) the full display name of the smallest-id figure, (b) the first three bytes of the first ruler's name, (c) a regnal display name when the case has one, in the file's existing search-line form; `worldgen-tests-browser` reads them.
4. **A headless-check world with a library** (viewer engineer): `headless_check.cmake` gains the smallest seed ≥ 42 at 512 whose full history report has a `library` line in `[sites]` (seed 42 itself when it has one), recorded in the script; the check asserts the library glyph drawn one glyph cell to the upper left of its city (P5-06 11.4) and its hover text `<site name> (library of <figure display name>), since <year>`. With 21.4, also a year after a library's ending at which the glyph is gone, when the chosen world has one.

## 22. Work split (r2, for cost)

| Part | Kind | Agent | Sections |
| --- | --- | --- | --- |
| A | engine: alliances, relations, conflict | generator engineer | 3 to 8, 9.2, 10, 11 items 1 to 7 |
| B | engine: figures, the monsters seam, the readers' checks | generator engineer (the same, or a second one on disjoint files) | 11 item 8, 21.4, 14.4's reader checks |
| C | engine: report v9 and the `[figures]`/`[monsters]` selections, the renderer's forms | one engineer for all of `history_report.cpp` and `digest` | 8.5 (choice, `{m:died}`), 12, 21.5 |
| D | data: fields, templates, the changed values | with A to C (needed before any test) | 14 |
| E | calibration sweeps, data only (history replayed on cached geography, checked equal to `generate`) | a separate agent after A to D | 15.2 steps 1 to 10; 21.1, 21.3's retunes, 21.2 if the lead retunes |
| F | expected values and references | test writer | 16; H17; the references' replays and invariants; 21.7 items 2 and 3 |
| G | goldens | validation, once, after the cross-preset comparison | every world file and history golden (16) |
| H | viewer | viewer engineer | 21.7 item 4 (no other viewer change: ended towers leave the map through `sites_existing`) |

A to C share only `history.cpp`'s wiring and the catalogue; E needs a build of A to D and no code. The lead's choices of 23 L-4 to L-8 are needed before E's steps 7 and 8.

## 23. Open questions (r2)

**For Paul**: none new. Q-P5-04a-1 is decided; Q-P5-06-1 (the lair bound as an average of two samples) stays open with him and is not blocking (21.6).

**For the lead** (each with the interim reading the spec carries):

| Id | Question | Recommendation (interim) |
| --- | --- | --- |
| L-1 | End a tower when its line of casters lapses? | Not in this stage: it needs `site.ending` code 7, a code-list change that P5-03 and P5-04 each took with a schema bump; the endings by realm and land (21.4) answer the critic's "never ends". |
| L-2 | Does 21.4's use of codes 4 and 5 on kind 4 need schema 13? | No bump: no DDL and no new code; the codes are in `site.ending`'s list since version 9 and mean the same thing. What changes is `schema.md`'s note for kind 4 and a reader message, as P5-03a changed value ranges without a bump. The cost: a 0.27.0 reader refuses a 0.28.0 world that has an ended tower (`ending 4 is not 0 for a tower`). Record it in proposals with the schema 12 entry. |
| L-3 | Drop `reign <n>` from a ruler's summary line (21.5 item 4)? | Yes (built). |
| L-4 | FG5: restate to 25% to 70%, or retune heroes? | Restate (21.2). |
| L-5 | FG8: the definition of 21.2 and ≥ 15% overall, ≥ 30% of elven and faerie figures? | Yes. |
| L-6 | FG1's maximum ≤ 250? | Yes (21.3). |
| L-7 | FX1: retune skill to 600 to 900? | Retune; fallback ≥ 53%. |
| L-8 | FX3: retune grasping allied strain to 3000? | Retune; fallback ≥ 1.25. |

Not a question, a watch: AL1's formed median is 15 on B at 0.27.0, the bound's top; this stage's grudges and spurnings should lower formations, not raise them, and validation reports it.

## Revision notes

- r1 (2026-11-25): first draft. Approved by Paul on 2026-11-25 with Q-P5-04a-1.
- r3 (2026-11-26, text only, with P5-07 r1): 21.4 records two readings as built: an ending event older than the tower waits for the next change of owner (integration notes, Part 3), and the owner is tested at each check (audit A2). No rule changes; generator 0.28.0 is as built.
- r2 (2026-11-25): refreshed against P5-06 as built (generator 0.27.0, schema 12 with 36 tables, report v8, kinds to 92, 97 streams, browser groups to 8): the status line and the change list (towers, kind 87, `history.figures`; schema version unchanged, L-2); 2 the two new seams and 2.1 (where this stage's rules read a figure; nothing new beyond r1's `temper` and the deed `ally_refused` motive 5); 11 item 8 with P5-06 audit A2; 12.7 the length checked against 0.27.0 (613.5, inside 670); 14.2a and 14.4 (`history.figures` fields and kind 87's `quiet`, ten value rows); 15.1 every target with its 0.26.0 and 0.27.0 measures and P5-06's carried and guard targets; 15.2 steps 7 to 10; 16 items 15 to 19; 17, 18, 19 rows; section 21 (carried from P5-06: `the Old`, heroes and quiet lives, figure counts, skill and grasping betrayals, towers apart and ended, the summary blocks and `hero_rose`, guard targets, housekeeping), 22 (work split) and 23 (open questions). r1's rules are unchanged in substance.
