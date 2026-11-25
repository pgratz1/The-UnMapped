# P5-04a Relations and alliances follow-ups

Status: **r1 (draft, 2026-11-25)**, written by the spec writer on `p5/04a-spec`, cut from `main` at 1ded5db (generator 0.26.0, schema 11, history report format v7; P5-04 merged). Phase P5 History depth, stage 04a: the follow-ups of P5-04 (`docs/specs/p5-04-alliances-trade.md` r3) and the open notes of P5-03a (`docs/specs/p5-03a-relations-followups.md` r2), from the world critics' reviews (`docs/reviews/p5-04-alliances-trade-world-critic.md`, Point 1, Point 3, F1 to F11; `docs/reviews/p5-03a-relations-followups-world-critic.md`, F1 to F7), the P5-04 validation (`docs/reviews/p5-04-alliances-trade-validation.md`, findings 1 to 5), the audits (`docs/reviews/p5-04-alliances-trade-audit.md` A1 to A3; `docs/reviews/p5-03a-relations-followups-audit.md` A1 to A4; `origin/p5/06-audit:docs/reviews/p5-06-figures-audit.md` A1, A3) and Paul's decision of 2026-11-25 on spoils (`docs/questions-for-paul.md` Q-P5-04-8; `docs/design/proposals.md` "Paul's answers of 2026-11-25"). **Built after P5-06 Figures** (0.27.0, schema 12, report v8; `docs/specs/p5-06-figures.md` r3 on `origin/p5/06-spec`): it reads P5-06's rulers and tempers. Generator **0.28.0**; **schema stays 12**; history report format **v9**.

**No schema change.** No table, column, event kind or code list of `schema.md` is added or changed. The change:
- changes rules of `AlliancesSubsystem` (spoils, refusals, betrayals, renewals), `RelationsSubsystem` (a turn's wording and a cool-down, expulsions of long-settled peoples, two audit fixes) and `ConflictSubsystem` (two const reads, the arms cause of a battle, three audit fixes); adds one keyed stream (`hist.alliances.renew`, 98 streams);
- appends fields to `history.alliances` and `history.relations` (data) and changes three shipped values;
- appends values to seven existing event kinds and one code to an existing value (`reason` of kind 76, a value range, not a `schema.md` code list); catalogue rows of `event_kind_value` written by the generator, as P5-03a did;
- widens the renderer's choice to eight words and adds the form `{m:died}` (P3-04 8.4);
- rewrites templates (data) and parts of the history report as format v9.

The form is that of P5-03a: the normative text is here, and P5-04, P5-03 and P5-03a get a pointer. Figures marked *measured* are validation's at 0.26.0 (seeds 0–99 / 100–199, 1024 defaults) or the critics' (16 worlds); every other figure is for the engineer's calibration and validation.

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

Not here, with reasons (section 17): a mine's ruin or lair (F6), naming a sacked town in a share (F9, needs a settlement on kinds 74 and 75), the set-aside claims line (F11), the dwarves' refusal to swear to goblins (F7), late friendships (P5-03a F3, beyond the cool-down), people met by word through a carrier (P5-03a F8), the ruins list's breakdown (P5-03a F9), late secessions (P5-03a F12).

## 2. Inputs, streams, draws

As P5-04 3 and P5-06 3, plus:

- **`FigureView`** (P5-06 7.1) gains `std::int64_t temper(std::int64_t civ) const`: the temper code of c's current reign (0 steady, 1 warlike, 2 peaceable, 3 grasping, 4 just, the order of P5-06 4.1's record), −1 with no reign or no view. Read by `AlliancesSubsystem` (4.2, 5).
- **Conflict's const reads** (amend P5-04 7.13): `bool has_claim(std::int64_t a, std::int64_t b) const` (an open claim of a against b) and `claim_event(a, b)` (its event, 0 none); `last_war_end(std::int64_t c) const`: the largest (year, event) of `last_peace(c, b)` over every civilization b ≠ c ascending (a `peace`, `war_won`, `truce` or `ally_withdrew` that ended a war of c, principal or joined), (0, 0) none.
- **`AllianceView`** (P5-04 7.1) gains `std::int64_t arms_event(std::int64_t c) const` (9.2).
- **Stream** appended to `streams.hpp` after P5-06's last (`hist.monsters.hero_fall`, the 97th): **`hist.alliances.renew`**, key (x, y, d) with x < y the alliance's pair and d the renewal's due year (6.1), 3.4. The list holds **98** streams. Every other rule of this stage is deterministic (ids, orders, values).

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
8. **P5-06 audit A1**: the figures subsystem remembers, in an ordered set of event ids, each `succession_crisis` read without a `civ_seceded` citing it, and links a later secession to it in any range. **A3**: `FigureView::split_bonus_permille(civ, year)` takes the crisis year (`ctx.year()` of peoples' step). Neither changes output under the shipped order.
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

At summary, per realm: +3 milestone lines, −1 quiet stretch where there was one; about +20 a default world outside `[chronicle]` (P5-02 12.9's budget gives back as many chronicle lines where L binds). Groups (12.4) shorten the chronicle. **Bound**: default median ≤ **670** (P5-06's 650 + 20); every world ≤ L(C); `civs4` median ≤ 1,050 (unchanged: the milestone lines are dropped above `SUMMARY_LINES_CIVS`).

## 13. Dwarves' participation (item 10; amends P5-04 15.1 P)

*Measured*: dwarves 89% and 87% (other sweeps 80% to 87%); every other race 94% to 100%. Reason (one line, not a tuning gap): reach for alliance and trade is open-border hops (P5-04 6.2), and a dwarven realm sealed in its uplands by borders with no walk pair and no pass (P5-03a 4.1; seeds 4 and 6) reaches no one; P5-03a lets such peoples **meet** by word, not trade. The world critic reads the reserve as character. Restated: participation ≥ 90% per race, **dwarves ≥ 85%** at 1024 defaults; sealed realms are reported. Contact across closed borders and by sea comes with roads and ships in P6 (Paul, 2026-11-24).

## 14. Data and catalogue

### 14.1 `history.alliances` (`game/data/history/alliances.toml` and both fixture roots)

Changed: `withheld_to_break` 1 → **2** (3.7, meaning changed), `withheld_grudge` 400 → **250**. Appended after `mine_forms`, in this order, all required: `spoils_large_share`, `spoils_large_part_permille`, `soured_renew_permille`, `soured_join_permille` (3.7); `weary_years`, `join_weary_permille`, `join_busy_permille`, `join_fear_floor`, `join_warmth_floor`, `grasping_join_permille`, `join_claim_permille`, `excused_memory`, `refuse_grudge` (4.4); `betray_cooled_disposition`, `betray_weak_permille` (5). The `alliances-small` root: `spoils_large_share` 1, `soured_renew_permille` 0, `weary_years` 0 (so every kind and motive occurs); the `zero` root: shipped values (inert without alliances, I0).

**Defaults in code** (a tuning built in code keeps 0.26.0's meaning): `spoils_large_share` 1000000, `spoils_large_part_permille` 1001, `soured_renew_permille` 1000, `soured_join_permille` 1000, `weary_years` 0 with `join_weary_permille` 1000, `join_busy_permille` 0, `join_fear_floor` 0, `join_warmth_floor` 0, `grasping_join_permille` 1000, `join_claim_permille` 1000, `excused_memory` 0, `refuse_grudge` 0, `betray_cooled_disposition` 0, `betray_weak_permille` 0. (The refusal motive and the excusing are rules, not tunings: they act at these defaults too.)

### 14.2 `history.relations`

Appended after `expel_gap_years`: `turn_cooldown_years` 300 (7.2), `expel_settled_years` 800, `expel_fresh_years` 150 (8). Code defaults 0, 100000, 0 (inert). The `zero` root: shipped; `relations-small`: `turn_cooldown_years` 0.

### 14.3 `history.report`

The templates of 3.6, 4.4, 5, 6.2, 7.1, 9.1, 9.2 (four battle kinds, P5-06's `battle_led` included), 10 items 3 to 5. Cause words added to an existing list are appended after its last entry in the order this spec gives; 10 item 3 replaces four words in place. Saliences unchanged.

### 14.4 Catalogue (no DDL change)

| Kind | Values appended, in seq order |
| --- | --- |
| 58 `disposition_turned` | `change` (0 to 3) |
| 63 `alliance_formed` | `term` (≥ 1), `band` (0 to 2) |
| 67 `alliance_betrayed` | `motive` (0 to 3), `years` (0 to E − 1) |
| 69 `ally_refused` | `motive` (0 to 7), `count` (0 to 100) |
| 75 `spoils_withheld` | `grievance` (0 to 2) |
| 76 `alliance_spurned` | `years` (0 to E − 1); `reason` 0 to **2** |

`event_kind_value` gains nine rows; every event of these kinds carries the new values. `schema.md`'s catalogue listing is updated by the engineer (the code lists of `schema.md` are unchanged). **Reader checks** (both readers, P5-03 16.4's form `corrupt table 'event': row <e>: value <k> <v> outside <a> to <b>` for every range above), and: an `ally_refused` with `motive` 7 cites a `spoils_withheld` (`refusal over spoils without its share`); an `alliance_spurned` with `reason` 2 cites a `spoils_withheld` of its year whose `grievance` is 2, unless it waited for a quiet budget (`years` > 0 then) (`spurned at once without its share`); an `ally_refused` with `count` 0 has `motive` 1 or 2 (`excused refusal with motive <m>`).

## 15. Targets and calibration

### 15.1 Targets (1024 defaults, seeds 0–99 and 100–199)

| Target | Definition | Bound | 0.26.0 *measured* |
| --- | --- | --- | --- |
| **AL6** | refusals | `ally_refused` ≤ `ally_joined_war` on **both** samples; excused refusals reported | 687 ≤ 736; **746 > 698** |
| **AL6b** | refusal motives | no motive above 50% of refusals; motive 0 ≤ 35% | — (no motive) |
| **AL5** | wars with an ally; joins | 20% to 45%; defence ≥ 70% of joins; attack ≥ 10% of joins | 25.8%, 25.0%; defence 82%, 79%; attack 18%, 21% |
| **AL7** | betrayals | as P5-04 (20% to 60% of worlds; 3% to 15% of ended) | 50, 46 worlds; 14.5%, 13.1% |
| **AL8** | breaks after refusals | `alliance_broken` reason 0 reported; ≤ half of counted refusals | — |
| **SP1** | shares granted | 500 to 850 per mille (P5-06's temper term included) | 680, 649 |
| **SP2** (restated) | alliances spurned | at least one in **10% to 35%** of worlds; at most **6%** of ended alliances | 28, 25 worlds; 5.3%, 5.4% |
| **SP3** | cause and effect | by rule: a spurning `reason` 2 is in its share's year (or states `years`); every other states `years` | 1 to 106 years, unstated |
| **SP4** | large shares; ends | 30% to 50% of withheld shares on open alliances are large; ≥ 40% of those withholdings end their alliance (at once, at renewal or by cooling) | — |
| **RN1** | written renewals | ≤ 35% of `alliances_renewals` | 100% (538 in 16 worlds) |
| **RR2** | P5-03a's turns | into friendship ≥ 25%; ≤ 20 turns a world; ≥ 55% before 5500 | 53%, 54%; —; 61%, 59% |
| **RR6** | flips | pairs with four or more turns in a world: in ≤ 5 worlds of 100 | — (critic: 4 pairs in 16 worlds) |
| **RR4** | friendship at E | median ≥ 3 | 4, 4 |
| **MX3** | expulsions | median ≤ 3; none of an entry held ≥ `expel_settled_years` without a turn into hatred within `expel_fresh_years` (by rule); the share of elves reported | median 2 to 3; elves 35 of 48 (critic) |
| **TR2** (restated) | trade-based alliances | basis 4 ≥ 15% of formed | 23.1%, 20.7% |
| **P** (restated) | participation | ≥ 90% per race; dwarves ≥ 85% | dwarves 89%, 87% |
| **C1**, **T16**, **G2** | wars; repeated battles; settlements and population | 22 to 28; ≤ 2 per thousand; ≥ 81 and ≥ 205,000 | 23, 24; 1.44, 1.18; 83/208,364, 88/222,506 |
| **T18** | summary lines | default median ≤ 670; every world ≤ L(C); `civs4` median ≤ 1,050 | 577, 578 at 0.26.0 (P5-06 bound 650) |
| **M** | P5-02 M4 and lairs | as P5-06 25.3 (85 and 65) | P5-06's |
| **timing** | `generate` seed 0 1024 release | at most 1 s over 0.27.0 | — |

**Why SP2 falls.** At 0.26.0 nearly every withholding (28 of about 38) led to a spurning, because one kept share ended the alliance at its renewal and a chill of 500 broke a cool one. Under Paul's rule a first, small share only sours: it ends the alliance at the next renewal half the time (`soured_renew_permille` 500) if the alliance lives that long, or by cooling. With about 0.4 withheld shares a world on open alliances, 30% to 50% of them large, and half the soured ones surviving to their renewal, spurnings come to about 0.4 × (0.4 + 0.6 × 0.5 × 0.5) ≈ 0.22 a world, in about 15 to 25 worlds of 100, and below 5% of ended alliances. Raising `spoils_large_*` or lowering the grant chance to reach 20 worlds would make keeping the usual outcome again, against "it might sour".

### 15.2 Calibration (data only, the engineer, in this order)

1. AL6, AL6b, AL5 by `join_warmth_floor` and `join_fear_floor` (raise both to bring joins up), then `join_busy_permille`, `join_weary_permille`, `weary_years`; AL8 by `refuse_grudge` and `refusals_to_break`.
2. SP4 by `spoils_large_share`, `spoils_large_part_permille`; SP2 by `soured_renew_permille`, `withheld_grudge`; SP1 checked (P5-06's `temper_spoils_permille` is not moved here).
3. RN1 is by rule (no lever).
4. RR2, RR6 by `turn_cooldown_years`; RR4 re-checked.
5. MX3 by `expel_settled_years`, `expel_fresh_years`.
6. AL7 checked (`betray_*` change wording only); C1, T16, G2, M after each step (a refusal's grudge and more joins move strain and wars).

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

Reference (`p5_04_alliances_reference.py`, `p5_03_relations_reference.py`): 2 to 8 in the replay; 13's invariants in `check-world`. Test-side items carried (from validation, for the test writer): `p3_conflict_reference.py` r16 knows `forgive` at `alliance_formed` (validation finding 3); `p5_03_relations_reference.py` invariant 4 accepts a same-year `settlement_mixed` (seed 66); `p5_04` invariant 11 as amended. Goldens: every world file and every history golden is re-recorded once after the cross-preset comparison (as P5-03a 14); `core_vectors.txt`, `p1-0*`, `p3-02-names.txt`, `p3-03-chronology.txt` must not move; `world-hashes.sh` keeps its cases.

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

## 18. Determinism notes

One new keyed stream (`hist.alliances.renew`, key of ids and a due year), drawn at most once per (alliance, due year). Every new choice is a minimum over a fixed code order (4.2, 5) or a test of values, read live in fixed passes; the arms set and the closed set are ordered sets of ids (and years). Arithmetic int64 with checked multiplication: the defence chance is nine factors ≤ 1000 each divided at every step; 3.3's tests compare values < 2^31; the casualty split by the wide multiply-divide. `FigureView::temper` is a const read of the reign at the step's start. No floating point, no unordered container.

## 19. Amendments

| Spec | Section | What | Note |
| --- | --- | --- | --- |
| P5-04 | 5, 6.3, 6.5, 6.6, 6.7, 6.11, 7.1, 7.13, 7.15, 8.1, 8.3, 10.2, 10.3, 13 items 11, 12, 14, 15 | 3 to 6, 9, 10, 11 items 1 to 5, 12, 13 | r4 pointer |
| P5-03 | 6.2.3, 6.5, 8.1, 8.2 | 7, 8 | r4 pointer |
| P5-03a | 3.3, 3.5, 4.2, 5.3, 9.2 | 7, 8, 11 items 6, 7, 12.2 | r3 pointer |
| P5-06 | 6.4 rule 3, 7.1, 7.4, 8.2 (battle templates) | the deed `ally_refused` motive 5; `temper`, `split_bonus_permille(civ, year)`; A1; 9.2's prefix | this spec (built first) |
| P5-02 | `monster_died` template | `{m:died}` | this spec |
| P3-04 | 8.4, 8.5 | choice of 2 to 8 words; form `died` | this spec |
| P3-06 | 7.3, 7.13 | `has_claim`, `claim_event`, `last_war_end`; the arms cause; A1, A2 | this spec |
| P3-07 | new 32 | v9 (12) | this spec |
| `schema.md` | catalogue | nine value rows; kind 76's `reason` range | the engineer |

8.5 (here): **the choice form** (amends P3-04 8.4 and P5-03 8.2): 2 to 8 words; with n ≥ 5 words, word i for value i (0 ≤ i < n − 1) and the last word for every other value (negative included); `#` renders the value as in the three-word choice. `template_problem`: `choice of <n> words at byte <b>` above 8. **The form `{m:died}`**: 10 item 5; `template_problem` as P5-03a 7.1's monster forms.

## 20. Proposals and questions

Entry "History: P5-04a relations and alliances follow-ups (readings)" in `docs/design/proposals.md`. One question for Paul, `docs/questions-for-paul.md` Q-P5-04a-1 (SP2 restated; interim: the restated bound, which the build carries).

## Revision notes

- r1 (2026-11-25): first draft.
