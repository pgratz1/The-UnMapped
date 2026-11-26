# P5-04a implementation notes (parts A and D)

Hand-over from the generator engineer who built parts A and D of spec
`docs/specs/p5-04a-relations-alliances-followups.md` r2 (section 22) to the
engineer of parts B and C on branch `p5/04a-impl`. Generator 0.28.0, schema 12.

## Built

Part A (engine):

- 3 Spoils withheld: `AlliancesSubsystem::spoils` decides `grievance` (0, 1, 2)
  before appending kind 75; `on_withheld` sours (record, `renewal_due`,
  `call_due`) or spurns at once (`spurn(..., reason 2, ...)`, same pass and
  year); a Q4 spurning without budget waits in `Alliance::pending_*` and is
  made by `pending_spurns` at the start of the next T2 or Q4 (`years` > 0).
  Soured renewal draw on `hist.alliances.renew` (x, y, d); soured call factor
  f7; cooling of a soured alliance spurns with reason 1 and `years`.
- 4 Refusals: the eight factors and the motive in `calls`; excused (1, 2)
  versus counted refusals; causes by motive; `refuse_grudge` through
  `add_grudge_seam`. Attack chance times f1, f2, f5.
- 5 Betrayals: motive and `years` on kind 67.
- 6 Renewals: the fixed chain (`Alliance::next_due`), notable renewals only
  (joint war, granted share, changed basis since R), quiet renewals credit
  with R; kind 63 gains `term` and `band`.
- 7 Turns: `change` on kind 58; the cool-down.
- 8 Expulsions of long-settled entries.
- 9.2 Arms: `AllianceView::arms_event`, conflict's `arms_cited_` set; the
  battle (all four kinds, one code path in `conflict_war.cpp`) cites it.
- 10 item 2 (formation causes: the basis reason's cause only), item 4 (`band`).
- 11 item 1 (conquest owes only when the principal conquered the enemy),
  item 6 (A2, larger opening event), item 7 (`word_since` -1). Items 3, 4 and
  5 (audit A1 to A3) were already in place at 0.27.0; nothing changed.
- `FigureView::temper` (virtual with a default of -1, so test doubles still
  compile), `FiguresSubsystem::temper`; the deed `ally_refused` with
  `motive` 5 names the ally's ruler (in `FiguresSubsystem`'s log reader, next
  to the `spoils_*` deed).
- `ConflictSubsystem::has_claim`, `claim_event(a, b)`, `last_war_end`.

Part D (data):

- `history.alliances`, `history.relations`, `history.figures`: the appended
  fields with ranges in `src/data/kinds_history.cpp`, the loaders and the code
  defaults of 14.1, 14.2, 14.2a; shipped values in `game/data/history/` and
  the two fixture roots `worldgen/tests/data/geo{,-small}/history/` (copies).
  The figures fields `tower_spacing_permille` and `hero_return_years` are
  loaded into `FiguresTuning` but read by nothing yet (part B, C).
- Changed values: `withheld_to_break` 2, `withheld_grudge` 250, `the Old`
  1400/1480, `skill_min` 600, `skill_max` 900, grasping allied strain 3000.
- Templates (`report.toml`): 3.6, 4.4, 5, 6.2, 7.1, 9.1, 9.2 (four battle
  kinds), 10 items 3 and 4.
- Catalogue: values on kinds 58, 63, 67, 69, 75, 76 (nine rows); kind 76's
  `reason` range widened to 0..2 in the reader check (needed for the report
  to read a 0.28.0 world). `schema.md` catalogue rows for those six kinds.
- Renderer: the choice of up to eight words (P5-04a 8.5) in
  `event_catalogue.cpp` (`Token::words_n`, `choice_word`), needed because the
  `ally_refused` template has eight.
- Stream `hist.alliances.renew` (98 streams), `GENERATOR_VERSION` 0.28.0.

## Readings (where the spec allowed more than one)

1. A pending spurning at once (Q4 without budget) is made at the start of the
   next T2 or Q4, before the log is read (so before a betrayal of that step).
   While pending, cooling and renewals skip the alliance.
2. 3.4 item 2 says `withheld(A) = 1` at a souring; I use `withheld += 1` (the
   same at the shipped `withheld_to_break` 2; with a larger value the count
   could otherwise never reach it). The soured record is the first
   withholding's; the flags are set again at every souring.
3. f1 needs a war end at all: `last_war_end(z)` = (0, 0) is never "weary"
   (else every realm is weary while Y <= `weary_years`).
4. `arms_event(c)`: the seller must still be a seller (an open mine) as well
   as have an open trade with c and a sale in the set.
5. A soured renewal in Q4 waits (no draw) while the wronged ally has no
   budget; a notable renewal without budget renews quietly (6.2). A lapse
   without budget waits (as at 0.27.0).
6. The `call_due` flag is cleared at the first defence call of the wronged
   ally to its partner that passes the eligibility filters (a call skipped
   by them is not a call).
7. Betrayal motive 2 compares `force(b) x 1000` with
   `betray_weak_permille x force(a)` using the live force of the step.
8. A formation's `band` of one race is 2; of two races `DispositionView::band`.
9. The defence factors are applied in code order 0..7, so with the code
   defaults the rounding differs slightly from 0.26.0's
   (defend x join x warmth x sigma); the spec's order is followed.

## Left for B and C (not done here)

- Kind 87 `hero_rose` value `quiet` (catalogue row, template, the figures
  rule of 21.5 item 3): not added, since the catalogue would demand the value
  of every event and `FiguresSubsystem::on_raid` does not write it yet.
- `{m:died}` and the `monster_died` template (10 item 5): template unchanged.
- The `template_problem` text `choice of <n> words at byte <b>` above eight
  words: a choice of nine or more still gives the generic unknown-token
  problem.
- Reader checks of 14.4 (ranges of the new values and the three messages);
  only kind 76's `reason` bound was widened.
- 11 item 8 (P5-06 audit A1 to A3), 21.4 (towers), `split_bonus_permille(civ,
  year)`, `MonstersSubsystem::height_cell`.
- Report v9 (12), the `[figures]`/`[monsters]` selections (21.5), 11 item 9.
- `schema.md`: kind 87's `quiet` and kind 4's endings.

## Seams for B and C

- A battle's causes may now hold an `arms_sold` with a smaller id than the
  `war_declared`, so `causes.front()` is no longer the war. `FiguresSubsystem::
  on_battle_led` read the front and crashed (`close_relation: relation
  'served' is not open`, geo-phrase-512); it now takes the `war_declared`
  cause. Any reader (digest, report, viewer, references) that takes a battle's
  first cause as its war must do the same.

- `AlliancesStats` gained `soured`, `spurned_at_once`, `excused`,
  `renewed_quietly` (not stored).
- Kind 63's `term` is written at every formation (origin too), so the report's
  `renewed <k>` (6.3) can be computed from the world alone.
- Kind 75's `grievance` is on `spoils_withheld` only (not on kind 74).
- Kind 76's `years` is Y - the year of the share it cites.

# Parts B and C (second engineer)

## Built

Part B (engine):

- 21.4 towers apart: `MonstersSubsystem::height_cell(ctx, region, avoid, radius)` (and `wild_cell`'s
  optional `avoid`, `radius`): a cell at Chebyshev distance <= D of any avoided cell is skipped.
  `FiguresSubsystem::caster_rises` passes the cells of every kind 4 site of the realm (open or ended,
  site id order; `towers_`) with D = `tower_spacing_permille` x map size div 1000 (0: nothing kept
  clear). No cell: no rise, after the caster draw. A library is unchanged.
- 21.4 endings: `FiguresSubsystem::tower_endings` in T3 (after the log, before the rises), T6 and Q4
  (after the log): realm ended -> 5 with its `ending_event`; library settlement not standing -> 4 with
  its `ruin_event`, not the realm's -> 4 with its `owner_event`; tower in the wild whose region is not
  the realm's -> 4 with the region's `owner_event`; `end_site_seam` (now kind 3 or 4) with the event's
  year. `books` and `blights` skip a caster whose tower ended (`tower_open`).
- 21.5 item 3: kind 87 gains value `quiet` (catalogue row); `on_raid` keeps the year of each monster's
  last raid read (`last_raid_year_`), g = year(r) - year(p), `quiet` = g when g >= `hero_return_years`.
  Template `hero_rose` of 21.5 and `monster_died` with `{monster:died}` in the three `report.toml`.
- 11 item 8: A1 (`pending_crises_`: a crisis read without its secession; a later `civ_seceded` citing it
  starts the new realm's first reign instead of the `on_founded` guard), A3
  (`split_bonus_permille(civ, year)`, peoples passes `ctx.year()`), A2 was already in place.
- Reader checks of 14.4 in `monster_tables_problem` (so both readers, same first error): the ranges of
  kinds 58 (`change`, in the relation checks), 63, 67, 69, 75, 76 (in the alliance checks) and 87
  (`quiet`, in `figure_tables_problem`, which gains `FigureTables::end_year`); the three messages
  (`excused refusal with motive <m>`, `refusal over spoils without its share`, `spurned at once without
  its share`); a tower's ending `is not 0, 4 or 5 for a tower`.
- Renderer: `{m:died}` (Form::died), and a choice above eight words gives `choice of <n> words at byte <b>`.
- `schema.md`: kind 4's endings and kind 87's `quiet`.

Part C (report v9): header v9 (and `report --help`); summary milestones (12.2: `milestones_count`,
`milestones_top` in `CivilizationDigest`, `SUMMARY_MILESTONE_SALIENCE` 40); `allies:` names at summary;
same-day groups of kinds 60 and 61 (12.4); alliance lines (basis part with the foe, `renewed <k>` by
6.3 from `term`, spoils by id, 11 item 9); trade `; broken` / `; ended` (12.5); the 22 stats of 12.6
(`followup_stats`, appended last); `[figures]` reserved picks and no `reign <n>` at summary (21.5
items 1 and 4, L-3); `[monsters]` living first, up to four lines (21.5 item 2).

## Readings

1. `allies:` at summary prints `c<id> <name>` (the literal form of 12.3, as the full lines), as a
   literal and a text segment, so the viewer does not link it; a linked form would need a segment
   kind whose report text is tag-first. Left for the viewer engineer and the spec writer.
2. A tower whose ending event is older than the tower (a tower raised in a region its realm did not
   own) does not end until the next change of owner (else `ended_year` < `origin_year`).
3. A1's late secession starts reign 1 of the new realm and names no one (the crisis was already
   handled as a non-split one).
4. 12.4: a group needs two or more events of the same kind; a single event keeps its normal line. The
   group line has no `{refs}`, `[causes]` or note (12.4's form). The settlement's text is found by the
   linked render's settlement link.
5. 12.2: `own` is the digest's own(c, e) of P3-07 24.5; the events are those naming the realm.
6. 21.5 item 1: a reserved pick may have renown 0 ("each role with at least one figure").
7. `spurned at once without its share`: the cited `spoils_withheld` with `grievance` 2 must be of the
   year Y - `years`.

## Battle causes

Checked every reader of event causes in `worldgen/src/` (history, digest, report, browser, timeline,
markers): none takes a battle's first cause as its war. The digest's war of a battle is the first cause
of a chain kind (`arms_sold` is not one); the other `causes.front()` readers are of `ruin_faded`,
`flare_up`, `region_renamed` and the engine's `ruin_faded`.
