# World critic: P3-07 history report (`report --history`, generator 0.20.0)

World critic, 2026-11-22.

**Code.** `origin/p3/07-int` at 8aa0293, built with `linux-gcc-release`. Spec: `docs/specs/p3-07-history-report.md` r16 on `origin/p3/07-spec`. Design: sections 5, 6 and 14.

**Read.** I read 20 summary reports and 6 full reports.
- The 11 implementation samples in `~/.cache/the-unmapped/out/p3-07-impl/samples/`: the phrase world "The Unmapped" and seeds 0 to 9, at 1024.
- Reports I ran on P3-11 sweep worlds into `~/.cache/the-unmapped/out/p3-07-critic/`:
  - default seeds 5, 40, 42, 52, 76 (an orc collapse) and 88;
  - calm seed 7 and turbulent seed 14;
  - `civs4` (four civilizations per race) seeds 0, 3, 7 and 12.
- I read in full: seed 0 at summary; the phrase world and seed 42 at both levels; the civilization blocks and orc ages of seeds 76 and `civs4` 3. For the rest I read and counted the summaries, and used the full reports for the counts below.

Quotes give the world, the level and the event id.

## Verdict

**Pass with notes.** At default, calm and turbulent settings the summary reads as a history. Every war has its declaration and its ending, the notes link a war to the claim behind it, and every sentence I checked is true to the rows. Paul can be given seed 0, seed 42 or the phrase world for the phase review.

Four things should be fixed before that review, all in the report and none in the world bytes:
1. **(A1)** With four civilizations per race the summary shows the outcomes of most wars without their declarations.
2. **(A2)** Claims fill a fifth of the summary.
3. **(A3)** The quiet stretches, the design's own "four centuries of peace" records, are almost all dropped.
4. **(A4)** The civilization blocks are half relation rows, each printed twice.

## What works

- **A war reads as a unit.** In default seed 42, summary:
  - "The Ghuughzuz Horde declared war on the Duchy of Dant over Warlord's Rises. (Claimed since 5804. The third war between them.)" (e577)
  - "The Ghuughzuz Horde sacked Sack of the Duchy of Dant; 1,479 died and 3,452 fled." (e584)
  - "The seat of the Duchy of Dant moved to Alessas." (e585)
  - "The Duchy of Dant sued the Ghuughzuz Horde for peace after eight years of war." (e595)
  - "The Duchy of Dant paid tribute to the Ghuughzuz Horde; 634 were carried off." (e596)

  At default, calm and turbulent settings, no shown war outcome lacks its declaration (my count, every such world above).
- **The notes help.** "(Claimed since 2180.)" and "The fifth war between them." (seed 0, summary, e51, e276) give a long grudge in a few words.

  The claim note reads well, and it explains the cases where a war's goal moved: "The Reanaluld League declared war on the Warband of Oz'ghuz over the Steppes of Thirst. (The claim, since 5817, was to Hound's Steppes.)" (default seed 76, summary, e683). It also names the towns behind a kind 39 claim (spec 23 item 4a): "Clan Kuushbam declared war on the Kingdom of Geadalack over Snowscar. (The claim, since 5779, was to Ghaldgruz Camp in Snowscar. The second war between them.)" (seed 7, full, e607). This answers my P3-11 F5.

  The claim note goes once a war on another region gets a fresh claim (Paul, 2026-11-22). Until then it is the best line in the report.
- **The flare-up note names the parties.** "In 2893 tensions flared into 50 years of upheaval. (The claim of the Baskeeb Mob to the Holt of Yews, held by the House of Nife, came to a head.)" (seed 0, summary, e50).
- **An orc collapse can be followed from the summary.** In default seed 76 the Warband of Oz'ghuz loses four settlements in eight years (e686 to e704). Its block's milestones are the same story in six lines, ending with "5933 e702 sacked: The Reanaluld League sacked Zorz of the Warband of Oz'ghuz; 235 died and 550 fled."
- **Full is usable as a reference.** It runs to 985–3,503 lines. Every line carries ids, refs and causes, so `grep -E '=c9[ }]'` gives a realm's whole history. The `[present]` settlement lines give each ruin's year and event: "s75 Edearbury: city, ruined, population 0, no owner, at 430 731 in The Silent Weald (rg48), founded 5436 by The Duchy of Rudgor (c7), ruined 5615, e371" (phrase world, full).
- **Length.** At defaults the summary is 595–962 lines, with 156–248 events shown. About 300 lines of that are chronicle, which an evening's reading covers.

## (a) Defects of the report text (no world byte changes)

### A1. With four civilizations per race, the summary drops the wars and keeps their sacks

In `civs4` the summary threshold is 71 in every world I ran (seeds 0, 3, 7, 12). It sits one point above `war_declared` (salience 70). The age floor brings in a few declarations; past it, sacks, captures, conquests and peaces (salience 75–90) are shown without the war that caused them.

| `civs4` seed | war outcomes shown | of them, declaration not shown | events shown / in log |
| --- | --- | --- | --- |
| 0 | 55 | 34 | 124 / 1,795 |
| 3 | 66 | 56 | 135 / 2,032 |
| 7 | 69 | 61 | 145 / 1,748 |
| 12 | 53 | 35 | 119 / 1,800 |

`civs4` seed 3's summary chronicle shows 43 sacks against 6 declarations. Its orc age, from the summary:
- "5638 e1627 settlement_captured/80: The Warrens of Sleex-Skrib took Waldford from the Kingdom of Stid; 291 died."
- "5644 e1653 civ_conquered/90: The Kingdom of Stid fell to the Warrens of Sleex-Skrib."
- "5956 e1976 settlement_captured/80: The Dairinhi Throne took Graag from Clan Ghum'graash; 171 died."

No line says why. The summary is also 1,269–1,478 lines long, 929 of which in seed 3 are civilization blocks, against 208 lines of chronicle for 6,000 years. So with many realms it is long where it should be short, and thin where it should be full.

**Fix.** Two report-side changes; either one would remove the orphans.
- Make the declaration a companion of its outcomes (21.2 in reverse: a shown `sacked`, `settlement_captured`, `civ_conquered`, `peace`, `truce` or `war_won` brings its war's `war_declared`).
- Or never let T rise above the declaration's salience; a world with more wars than the budget then drops battles and calamities first.

### A2. Claims take a fifth of the summary

Claims are the most frequent kind in the summaries: 588 lines of about 4,300 shown across the 20 reports. Default seeds 42 and 76 each show 53 claims among about 225 events.
- Most are claims that no war follows, which 21.6 deliberately keeps.
- They come in clusters. Default seed 42, summary, year 5685, e489–e492, has four claims in one year, one after another: "The Grork Horde laid claim to the Torn Holt, held by the House of Finen." then the Warrens of Dribnark's, the Zak-Neez Mob's and the Kit-Kotch Mob's claims on the same region.
- A claim that is pursued is already in its war's note ("Claimed since 5685.").

**Fix.** Move unpursued claims that are still open at E out of the chronicle and into `[present]`, as a list of standing claims. They are the tensions a player could find (design 5, "Quest hooks"). An unpursued claim that lapsed can drop from the summary.

### A3. Quiet stretches are almost never shown

Quiet stretches have salience 20, which is below every threshold above 20. In each of the 20 summaries one stretch is shown, two in seed 3, calm 7 and seed 40, out of 6 to 10. The one shown is the elves' first, which the age floor brings in.

The quiet phase is then a bare count. In seed 0, summary, phase p6 (2943–3264, 322 years) is a heading followed by "(14 more events not shown)". Its stretch line, "For … years …, the land was quiet, and two realms kept the peace", is the summary record design 5 asks for, and it is missing.

Phases with nothing shown at all number 0–5 per default world, 9 in seed 88 and 15 in `civs4` seed 3.

**Fix.** Always show every `quiet_stretch` at summary level. There are fewer than ten per world.

### A4. The civilization blocks are half relation rows, each printed twice

Every relation row is printed in both realms' blocks.
- In seed 0, the House of Nife and the Baskeeb Mob print the same 13 rows of their seven wars.
- The Grork Horde (default seed 42) has "relations: 39 (enmity 3, peace 17, war 19)" and 39 rows.
- Relation rows are 154 of 371 civilization lines in seed 42 and 460 of 929 in `civs4` seed 3.

The rows also do not say who won. A reader learns a war's span but not its outcome.

**Fix, at summary level.** Replace the rows with one line per adversary, for example "wars with the Baskeeb Mob: 7, won 3, lost 3, drawn 1, 2893–5863", and keep the rows at full. Add three lines that the block lacks:
- `seceded from <ref>, e<id>`, for a realm born of a secession;
- `wars: <n> declared, <n> defended; won <n>, lost <n>`;
- for an ended realm, `last capital` in place of "capital". The Kingdom of Stid (`civs4` seed 3) reads "capital: Stulhild (s185)" beside "fate: ended in 5644", and Stulhild has been a ruin since 5644.

### A5. Milestones by salience repeat the sacks and miss the outcomes

The six milestones are the highest-salience events, so a warring realm's list is its sacks.
- **Seed 0, summary.** The House of Nife and the Baskeeb Mob have the same five sacks (e130, e205, e209, e220, e292). The Mob's two won wars and two gained regions ("The House of Nife ceded the Holt of Yews to the Baskeeb Mob.", e59) appear in neither block.
- **Seed 42.** The Grork Horde's 19 wars appear as one conquest and two sacks.
- **The generations note** (22.2) is printed only on a `quiet_stretch` milestone, and a `quiet_stretch` (salience 20) never reaches a realm's top six. Not one report I read prints it.

**Fix.** Choose the milestones by kind: founding, first and last war outcome, the largest gain, the largest loss, and the end. Or raise the count to 10 and take at most two of one kind.

### A6. `[ages]` counts every civilization founded in an age, not the age's race

The phrase world, summary, reads "a6 The Age of Orcs: 5638-5999, 362 years; Orcs (ra6) made by Uul'duk (g6, evil); 4 civilizations founded". The orcs have one realm, the Warband of Uuld'thu (c8). The other three are goblin secessions (c9, c10, c11). Read in that line, the count means four orc realms.

**Fix.** "4 civilizations founded (1 of the Orcs)".

### A7. The age floor fills a short age with small calamities

The floor of 20 events per age takes candidates in salience order, so where an age has few large events it fills with salience-25 lines.
- **Seed 0, summary, the Age of Faeries, phase p11.** It shows 17 events. 14 are salience 25: a succession crisis, then plague and famine in goblin settlements, among them "Famine struck Nub Pit; 61 died." (e158) and "The plague spread to Nub Pit; 49 died." (e165). The faeries themselves have three lines.
- **Default seed 52.** The summary shows 20 such salience-25 calamity lines, and seed 0 shows 17.

**Fix.** The floor prefers events whose participants include the age's race, and it counts a plague outbreak together with its spreads as one line.

### A8. The texture of a war changes with the threshold

Battles (salience 30) are shown when T is 26 and hidden when T is 31 or more. So seed 0, the phrase world and calm 7 list every battle of a war, while seeds 1 to 9 and 42 list none. In seed 0, summary, war e198 has six battle lines between its declaration and its end.

**Fix.** Battles stay out of the summary whatever T is, except the last battle of a war, which decided it.

### A9. Order and form, for a reader rather than a script

- **Order.** For reading, the chronicle should come before the civilization blocks. Today a reader meets 200–900 lines of blocks before the first event. `[phases]` repeats what the chronicle's phase headings already say, with their years.
- **Form.** The refs `{@rg31 @s9 settlement=s9 founder=c2 population=300}` and causes `[causes e19]` on every line are what make the report a tool, and they should stay in `full`. In `summary` they double each line's length.

  **Suggestion:** a summary form, a third level or an option, without refs and causes. Format v1 needs a revision note for it, not a new version, if it is a new level.
- **`[present]` at summary.** It gives counts only. For "places worth finding" it should list:
  - the ruins (name, region, year, who sacked it);
  - the open wars;
  - the hidden refuges;
  - the standing claims of A2.

  The full report already has the first three as settlement lines.
- **`[stats]`.** It is 146 lines of integers at the end of both levels. It is harmless where it is, but at summary it could be dropped or cut to the dozen keys a reader wants.

### A10. Small points

- "became_city" lines all read "grew into a city of 8,0xx": default seed 76, summary, shows twelve of them, from 8,004 to 8,051. A city's founding as such is worth one line. Its exact size at the 8,000 threshold is not; drop the number at summary level, or drop the line.
- A ruined settlement's `[present]` line names its founder but not its last owner, so "founded 2419 by The Warrens of Skruz-Snugg (c2), ruined 5065, e314" (phrase world, full) hides that the founder sacked its own old town after the Gnokkirk Horde took it. **Suggestion:** add "last held by <ref>".

## (b) The history itself (next balance change)

These are true to the rows, and the report shows them as they are.
- **Identical battle tolls.** Default seed 88, full, has "The Realm of Viraece defeated the Warrens of Drak-Slark in Pitchslack Forest; 228 of the defeated and 73 of the victors fell." six times, from 5836 to 5873 (e954, e959, e974, e979, e986, e995). This is the P5 note of my earlier reviews; this world is its strongest case.
- **Repeated succession crises.** "A succession crisis at Natch Pit shook the Grotch Mob." appears nine times in turbulent seed 14, full, and "A succession crisis at Gnug shook the Zix Mob." eight times in default seed 76, full. It is not in the summary (salience 25 or 40), but a realm page in P4 will show it.
- **Repeated sieges that hold.** "The Ghuughzuz Horde besieged Beckford of the Duchy of Dant; it held." appears five times in default seed 42, full. Already known: P3-11 section 12.
- **Runs of seat moves.** In default seed 76, summary, e687, e692, e696 and e704 move the seat four times in seven years. They are companions of the captures, so the summary keeps all four. This is vivid once and long at the second reading.
- **A flare-up cut short by the final phase.** "In 5699 tensions flared into one year of upheaval." (default seed 88, full, e901; also "two years" in another world, e542). The flare-up runs into the final phase's start, and "one year of upheaval" reads as a mistake.
- **The claim note** goes when Paul's fresh-claim rule lands (2026-11-22). Then A2's standing claims list matters more.

## (c) Ideas for P4 and P5

- **P4 realm pages.** A realm page in the viewer is the civilization block with A4's war tally and A5's milestones. With `region_control` it is also the map movie of the 2026-11-21 proposal. The report's `history_note` is already the right shared code.
- **A plain reading view.** Sentences, years and notes only, with names linked to pages, is what Paul and later a player read. The text form with refs stays for the critic and the sweeps.
- **`[present]` as quest seeds (design 5).** The open wars, standing claims, hidden refuges, ruins with their sackers, and realms reduced to one town (the eleven orc worlds of P3-11) are the unresolved tensions the design turns into quests. A `[tensions]` block, or P5's derivation, can read them from the same digest.
- **Design 14.** The enrichment pass can narrate a realm from its block and its units. A6's and A4's lines are the facts it needs at the head of each realm.

## Questions for Paul

1. **Reading form.** Should the summary you read at the phase review keep the ids and refs on every line (as now), or have a plain form with sentences and notes only (A9)?
2. **Unpursued claims.** Should claims that no war followed stay in the chronicle, or move to a list of standing claims at the present (A2)?
3. **Where to start.** Should the summary open with the chronicle and put the realm blocks after it?

## Summary of counts

| Measure | Value |
| --- | --- |
| summaries read (default, calm, turbulent) / `civs4` | 16 / 4 |
| shown war outcomes without their declaration, default, calm and turbulent | 0 in every world |
| the same, `civs4` (seeds 0, 3, 7, 12) | 34, 56, 61, 35 |
| claims among shown events, 20 summaries | 588 of about 4,300; 53 of 228 in default seed 42 |
| quiet stretches shown | 1 of 6–10 per world (2 in three worlds) |
| relation rows among civilization lines | 154 of 371 (seed 42), 460 of 929 (`civs4` seed 3) |
| generations notes printed | 0 in every report |
| summary length at defaults | 595–962 lines, 156–248 events |
