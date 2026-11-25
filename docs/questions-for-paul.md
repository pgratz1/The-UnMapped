# Questions for Paul

A tally of every question the specs, reviews, `proposals.md` and `CLAUDE.md` put to Paul, as of 2026-11-25 (generator 0.26.0 on main; the P5-06 spec on a branch).

**How to use it.** Write your answer in the `Answer` cell of a row, or change its status word. The lead then moves answered rows to section 2, records the decision in `docs/design/proposals.md`, and adds new questions at each merge. A question that was asked in a spec and again in `proposals.md` is one row with both sources. Where no decision could be found, the row says so.

**Count: open 0, decided 196, settled by phase approval 95.** (Rows that cover a range of question numbers are counted once per question.)

Contents: [1 Open](#1-open) | [2 Decided](#2-decided) | [3 Settled by phase approval](#3-settled-by-phase-approval)

---

## 1. Open

### Blocking

None.

No open questions. (2026-11-25: Paul answered the two rows on monsters and lairs and on spoils; they are in section 2.)

Work the lead took from the answers (not questions): record a famine or plague in a large world's summary when its impact is significant (Q-P5-01-1), in the next stage that changes the report; recording a movie on Windows stays untested until a Windows machine is at hand (Q-P4-06-7).

---


## 2. Decided

### Answered by Paul on 2026-11-25 (in this file)

Recorded in `proposals.md` as "Paul's answers of 2026-11-25". Both go into work in flight: the recalibration into P5-06, the spoils rule into the follow-up stage P5-04a.

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P5-03a-M4 | P5-02's target M4 asks for a living monster at the end of history in at least 85 of 100 default worlds. Since 0.25.0 the undead rise through the whole history instead of mostly after year 5500 (as intended), so fewer worlds end with a late undead host as their one living monster: 82 and 83 worlds in two samples (0.24.0: 87 and 89). Restate the bound, or tune so that more monsters live at the end? | Recommended: recalibrate in P5-06 rather than restate. At 0.26.0 the figure fell again (77 and 81 worlds), and worlds with a lair on a former settlement fell to 58 and 55 against your bound of 65; P5-06's rule that every ruin stays will move both, so the lead will tune monsters and lairs back to 85 and 65 there. Built: nothing changed. | P5-03a validation finding 2 (`docs/reviews/p5-03a-relations-followups-validation.md`) | Agree, we should recalibrate since the living monsters will be a major highlight of the game when players explore the land |
| Q-P5-04-8 | Spoils, after calibration. You decided that an ally kept from its share "might sour the relationship", and agreed (Q-P5-04-3) that one withheld share would not by itself end an alliance. To reach the target of a spurned alliance in 20% to 60% of worlds, the engineer's calibration made one withheld share end the alliance at once (`withheld_to_break` 1; the spec began at 2). Measured on 100 worlds: a share is granted 68% of the time, 28 worlds have a spurning, 5% of ended alliances end that way. Keep one, or go back to two and accept fewer spurnings? | Recommended now (changed 2026-11-25 after the world critic's review, which finds the ending abrupt: the break can come up to 106 years after the withholding): go back to what you first agreed, a first withheld share sours the alliance and a second, or one large kept share, ends it; the lead will do this in the follow-up stage P5-04a unless you say otherwise. Built and merged at 0.26.0: one. | P5-04 spec r3 section 24; engineer's report | Agreed|

### Answered by Paul on 2026-11-24 (in this file, and for group H in the session)

Paul's words are in the `Answer` column. Recorded in `proposals.md` as "Paul's answers of 2026-11-24 to the open questions".

#### A. P5 as a whole (P5-00 overview)

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P5-00-1 | Which of the remaining P5 stages to build, and which to defer to P6? Stages left: P5-05 roads, P5-06 figures, P5-07 mountain depths, P5-08 pantheon, P5-09 casters and spells, P5-10 cults and factions, P5-11 artifacts. With P5-04's mines now in, is P5-07 wanted after all? Also, the optional hill-dwarf stage after P5-07 (decided as "none inside P5's stages, optional if wanted"). | Lead's proposal (session of 2026-11-24): keep P5-06 figures, P5-08 pantheon and P5-11 artifacts; defer P5-05 roads to P6 and P5-07 mountain depths; fold minimal casters into figures (P5-09) and cults into the pantheon (P5-10). Decision pending, so no later stage is started. | Lead, session of 2026-11-24; [P5-00 3.1, 9.3](specs/p5-00-history-depth-overview.md); P5-04 spec 6.14 (branch `origin/p5/04-spec`) | I say we take the recommendation except for mountain depths since we have the mines in this stage already. |

#### B. P5-04 alliances and trade (merged 2026-11-25, generator 0.26.0)

Paul decided the seven questions of this spec on 2026-11-24 (section 2). These are readings the lead kept afterwards, listed in the spec as L2, L3 and L5.

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P5-04-1 | Is a realm with an open mine itself 5% stronger in war (`forge_strength_permille`), besides the arms its buyers gain? | Kept: "Dwarves are great forgers" should show in the dwarves' own wars; calibrated against check MC3. | Lead, session of 2026-11-24; `docs/specs/p5-04-alliances-trade.md` section 24, L5 (branch `origin/p5/04-spec`) | Agreed with recommendation |
| Q-P5-04-2 | A share of spoils is counted in people and nobody moves (as a tribute's people do). Is that enough until wealth exists? | Kept until wealth: the event's `share` is a number; no population moves to the ally. | Lead, session of 2026-11-24; same spec, section 24, L3 | Agreed with recommendation |
| Q-P5-04-3 | Should one withheld share be able to end an alliance that is already cool? | Kept ("the last straw reads true"): no rule ends an alliance at one withheld share, but its chill can take an alliance near -350 below the break line, and the break is then written as spurned. Strict alternative: ignore the latest withheld share in the cooling test, at the cost of one more rule. | Lead, session of 2026-11-24; same spec, section 24, L2 | Agreed with recommendation |

#### C. P5-03a relations follow-ups (spec r2 on branch `origin/p5/03a-spec-r2`, not merged)
 
The spec's section 17 says all six questions were decided by the lead "as r1 recommended". No decision by Paul was found for any of them. The first three rows are targets the lead relaxed.

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P5-03a-1 | Target: population at the end. Accept 205,000 in place of 214,500? | The lead restated it to 205,000; measured 211,977, so the new target is met. | Lead, session of 2026-11-24; P5-03a spec 11 (branch `origin/p5/03a-spec-r2`) | Agreed with recommendation |
| Q-P5-03a-2 | Target: most disposition turns in one world, 16 against 15. Accept? | Relaxed by the lead from 15 to 16. | Lead, session of 2026-11-24; same spec, 11 | Agreed with recommendation |
| Q-P5-03a-3 | Target: share of turns before year 4500, 23% against 25%. Accept? | Relaxed by the lead from 25% to 23%. | Lead, session of 2026-11-24; same spec, 11 | Agreed with recommendation |
| Q-P5-03a-4 | Can peace alone bring a pair of peoples to friendship (goodwill builds after 100 years without war between neighbours, capped at 300)? | Yes, cap 300 as shipped, tuned so that friendship has a median of 3 and hatred of 4 per world. Paul's direction of 2026-11-24 was "friendlier relations more often". | P5-03a spec 17 item 1, 3.2 (branch `origin/p5/03a-spec-r2`); proposals entry "P5-03a relations follow-ups (readings)" on the same branch | Keep as recommended |
| Q-P5-03a-5 | Do peoples within three borders but across closed borders (no walkable crossing, no pass) meet "by word"? | Yes, after five borders and 150 years (calibrated), 4.2. | Same spec, 17 item 2 | Agreed with recommendation |
| Q-P5-03a-6 | The undead threshold: scale it by the world's population? | Yes (section 6). | Same spec, 17 item 3 | Agreed with recommendation |
| Q-P5-03a-7 | Summary report milestones given as a count (plus quiet stretches) instead of a list? | Yes (9.2). | Same spec, 17 item 4 | Agreed with recommendation |
| Q-P5-03a-8 | At most one expulsion per realm in 60 years? | Yes (5.3). | Same spec, 17 item 5 | Agreed with recommendation |
| Q-P5-03a-9 | The 800-line summary limit (T18's maximum) applies only to worlds of 16 realms or fewer (restated as the length bound L(C) of P5-02 12.9). Accept? | Yes. | Same spec, 17 item 6; lead, session of 2026-11-24 | Agreed with recommendation |
| Q-P5-03a-10 | Races alone on a land mass never meet anyone (24 pairs in 4 of 100 worlds). Add meeting by sea later? | Not now: reported in `[peoples]`, not counted by the checks RR1 and RR3; sea contact waits for a later stage. | Same spec, 17.1; lead, session of 2026-11-24 | OK, down the road we need to deal with these situations. I think people disconnected like this could thrive and eventually build ships to come to the land but I don't know what stage we want to build that into. |

#### D. P5-03 race relations and P5-02 monsters ([P5-03 section 28](specs/p5-03-race-relations.md))

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P5-03-1 | Do realms read their peoples' disposition when they choose a quarrel? This is what lets old wars leave grudges that shape later choices, and it moves P3's approved war balance. | Yes, with check C1 (wars within +-10%; calibrated values give +8.5%). Built so, taken on recommendation. Alternative: record only. | [P5-03 28 item 1](specs/p5-03-race-relations.md); proposals "race relations ... P5-03 (readings and scope)" | Agreed |
| Q-P5-03-2 | Expulsions: may a ruling people in hatred with a minority drive it out (at most half a settlement)? | Yes, the darkest outcome of standing and the strongest reason for a refugee. Built so, taken on recommendation. Alternative: standing only. | [P5-03 28 item 2](specs/p5-03-race-relations.md) | Agreed with recommendation |
| Q-P5-03-3 | Do abandoned ruins left standing (overrun, plague, famine, left, fled) never fade? | Never (`ghost_fade_years` 0), and no ruin fades while an open lair holds it; burned ruins fade as before. Taken on recommendation. Alternative: a long fade (4,000 years). | [P5-03 28 item 3](specs/p5-03-race-relations.md); proposals "P5-02's follow-ups in P5-03 (readings)" | I'd like these ruins to stay through till the end of simulation since we want to explore those in the later game. |
| Q-P5-03-4 | Do settlers flee a place a monster keeps raiding (a small place flees after two raids in 50 years), and are hunts called sooner after repeated raids? | Yes (hunts sooner, flight for fear; tribute to a monster waits for wealth). Built so; no choice was needed beyond confirming. | [P5-03 28 item 4](specs/p5-03-race-relations.md); same proposals entry | Agreed with recommendation |
| Q-P5-03-5 | The `civs4` summary length. Paul's bound was "about 1,100"; measured median 1,112. Accept the lead's restatement to at most 1,150? | The lead's ruling in r2 (21 T18). Alternative: no `[peoples]` lines at summary in worlds of more than 16 realms. | [P5-03 28 item 5](specs/p5-03-race-relations.md); decided earlier: [P5-02](specs/p5-02-beasts-monsters.md) 23 item 4 | Agreed with recommendation |
| Q-P5-03-6 | One stage or two (P5-02a first, then P5-03)? | Built as one stage. No decision found; nothing depends on it now. | [P5-03 28 item 6](specs/p5-03-race-relations.md) | Agreed with recommendation |
| Q-P5-03-7 | Do race relations now move enough? With the calibrated memory a default world has a median of one change of band between peoples; most pairs keep the feeling their camps give them. Should dispositions move more (larger memory, smaller `band_margin`, stances nearer the bands)? | Shipped values. To be judged after the world critic's review of P5-03a. This is also the question P5-03a answers. | [P5-03 28 item 7](specs/p5-03-race-relations.md); P5-03a spec 17.1 (branch); lead, session of 2026-11-24 | Agreed, lets see what the critic says The world critic at 0.25.0 (2026-11-24, `docs/reviews/p5-03a-relations-followups-world-critic.md`): relations now move enough (every one of 12 default worlds ends with 2 to 5 friendships); its notes are on wording ("grew wary" is also used for a thaw from hatred) and on pairs that flip back and forth, taken into the next stage that changes world bytes. |
| Q-P5-03-8 | M4: a lair on a former settlement open at the end. Paul's bound (2026-11-23) is 65 of 100 worlds; two samples of 100 gave 62 and 66. Is the bound met, or keep it and calibrate further, or lower it? | No decision found. Validation figure pending. | [P5-03 28 item 8](specs/p5-03-race-relations.md); proposals "Paul's wish of 2026-11-23" (the 65 bound); lead, session of 2026-11-24 | Sounds fine |

#### E. P5-12 regions renamed by history ([spec section 17](specs/p5-12-renamed-regions.md))

The proposals entry is still marked "Status: open"; the spec was merged on its interim choices. No decision found.

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P5-12-1 | How many renamed regions? | About one region in twelve at 1024 (median 4 of 49), capped at 15%, a second name only after 300 years and for a weightier deed. Alternatives: more (median 5) or fewer (`world_max_permille` 100). | [P5-12 17 item 1](specs/p5-12-renamed-regions.md) | Agreed with recommendation |
| Q-P5-12-2 | The wider rule against doubled suffixes in names (also removes `Wentton`-type names and redraws `Pinegrove` and `Wanegrass`). | As specified. Alternative: the critic's two rules exactly, which leaves 21 `-ntton` names and keeps `Pinegrove`. | [P5-12 17 item 2](specs/p5-12-renamed-regions.md) | Agreed with recommendation |
| Q-P5-12-3 | Renamed regions in English forms only? | English only, with the namer's culture recorded so per-culture forms (a dwarven `The Halls of ...`) can be added as data later. | [P5-12 17 item 3](specs/p5-12-renamed-regions.md) | Agreed with recommendation |

#### F. Report and names leftovers

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P5-01-1 | Should a large world's summary keep a famine or a plague? At `civs4` and 2048 the summaries show almost no calamities (median 0.0%). | Not changed. Called "a report question for the critic and Paul". No decision found. | [P5-01 spec](specs/p5-01-followups.md) 12 and [P5-12 spec](specs/p5-12-renamed-regions.md) 18 | We should record these if the impact is significant |
| Q-NAMES-1 | Weak or unfortunate names: elven `Thassie` and others (`Whomoutwia`, `Kaaghdam`, `Tirkbumm`); `fag` anywhere for faerie names (`Shyfaglae`); dwarven coda `din` (`didin`). Filter them? | Not filtered; a names change if wanted, "with Paul". No decision found. | [P5-12 18 (G7)](specs/p5-12-renamed-regions.md); [P3-11 critic](reviews/p3-11-followups-world-critic.md) suggestions 1 and 7 | Leave them in |

#### G. P4 viewer

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P4-06-7 | Confirm the documented movie-recording command once on Windows (no CI job records). | Documented command at 1600x900; confirmed on Linux only. The review of 2026-11-24 approved the viewer but said recording on Windows remains untried. | [P4-06 14 item 7](specs/p4-06-history-movie.md); proposals "Paul's review of P4 Viewer v1" | I'm not super concerned about windows support, I don't have a windows box setup right now to test it, we can come back and fix it if its broken |


#### H. P5-06 figures (spec r2 on branch `p5/06-spec`, approved 2026-11-24; answered in the session)

The spec's "Plan for approval" is its first section, readable alone. Nothing is implemented yet; the interim choice is what the spec specifies.

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-P5-06-1 | Scope: A (rulers only), B (rulers, generals, heroes) or C (B with minimal casters: towers and libraries, named books, rare blights; about a quarter more work)? | C, since the scope decision of 2026-11-24 folds minimal casters into figures; B if the stage should be smaller (casters would then wait with the deferred P5-09). | [P5-06 22 item 1](specs/p5-06-figures.md) (branch `p5/06-spec`) || Paul, 2026-11-24: C ("Option C"); minimal casters in this stage. |
| Q-P5-06-2 | Which rulers are named? By deed (founders; the ruler under whom a realm declared war, broke faith, gave or kept spoils, or fell; the rivals of a disputed succession; the ruler at the end): about 55 a world, the rest of each line counted. Or every reign that begins in an active phase: about 360 a world, most of them goblin chiefs. | By deed. | Same spec, 22 item 2, 6.4 || Paul, 2026-11-24: By deed, as recommended. |
| Q-P5-06-3 | Regnal numbers ("Aldric II")? Names are unique in a world, so this needs an exception to the name registry. | No regnal numbers now. | Same spec, 22 item 3 || Paul, 2026-11-24: Changed: humans and dwarves sometimes carry regnal numbers, the other races not (spec r2 6.14). |
| Q-P5-06-4 | Rulers' tempers (steady, warlike, peaceable, grasping, just) lean by race, with each race's average quarrelling kept as now? Or the same mix for every race? | By race, average kept. | Same spec, 22 item 4, 4.1, 7.2 || Paul, 2026-11-24: By race, average kept, as recommended. |
| Q-P5-06-5 | The last ruler of a conquered realm: falls with the realm, or flees into exile (another heir in exile for P6)? | Falls with the realm (exiles come from disputed successions). | Same spec, 22 item 5, 6.4 rule 4 || Paul, 2026-11-24: Falls with the realm, as recommended. |
| Q-P5-06-6 | Epithets earned by deeds and kept with the name in every sentence ("Grishnak the Conqueror", "Maela Dragonslayer"), each used by at most two figures of a world? | Yes. | Same spec, 22 item 6, 6.10 || Paul, 2026-11-24: Changed: the cap of two applies to ordinary deeds; a substantial deed gives its epithet to any number (spec r2 6.10). |
| Q-P5-06-7 | Births are never events (a figure enters the record by an office or a deed; its birth year is on its row)? | Yes; a birth worth telling can come with P5-08's champions. | Same spec, 22 item 7 || Paul, 2026-11-24: Yes, as recommended. |
| Q-P5-06-8 | Summary length: one `rulers:` line per realm and a `[figures]` block of at most nine lines add about 20 lines to a default summary. Accept a default median bound of 645 (your bound of 2026-11-22 is 700)? | Yes. | Same spec, 22 item 8, 10.6 || Paul, 2026-11-24: Yes, 645, as recommended. |


Evidence is cited in each row: a `Decision (Paul, ...)` line or a "Paul's decision" entry in `docs/design/proposals.md` ("props"), or a spec revision note.

#### I. Follow-ups from the answers above

| # | Question | Recommendation / what the build does now | Source | Answer |
| --- | --- | --- | --- | --- |
| Q-SEA-1 | Peoples alone on a land mass (24 pairs in 4 of 100 worlds) never meet anyone. Paul: "people disconnected like this could thrive and eventually build ships to come to the land but I don't know what stage we want to build that into." Which stage takes ships and contact by sea? | Lead's proposal: with roads (P5-05, deferred to P6) as one "roads and ships" stage, since both change which lands can reach each other by year and so need the same recalibration of expansion and war. Until then such a race is reported in `[peoples]` as having met no one. Alternative: a small stage of its own after P5-07. | Q-P5-03a-10 (section 2); P5-03a spec 17.1 | Agreed (2026-11-24): "roads and ships in P6". |
| Q-RUINS-1 | Paul wants ruins to stay to the end of the simulation, to explore in the game. Ruins left by abandonment (overrun, plague, famine, left, fled) already never fade. Should ruins left by a sack (burned) also never fade? Today a sacked ruin fades after `ruin_fade_years`, and a default world ends with a median of about 19 ruins standing and a few faded. | Lead's proposal: yes, keep every ruin, with the report's summary listing at most 15 (as since P5-03a) and the viewer's "Faded ruins" switch reused for ruins older than a stated age. It changes world bytes, so it rides with the next generator stage. Alternative: sacked ruins keep fading (a burned village leaves little to explore). | Q-P5-03-3 (section 2); P5-03 spec 11, P3-05 (fading) | Agreed (2026-11-24): keep every ruin. The lead schedules it with P5-06, the next generator stage not yet in implementation, since more standing ruins move the lair and undead figures. |

### P5-04 alliances and trade (spec on branch `origin/p5/04-spec`)

| # | Question | Decision | Source |
| --- | --- | --- | --- |
| Q-P5-04-D1 | Do allies fight inside the partner's battles or in separate wars? | 2026-11-24: as recommended, allies fight in the partner's battles (`allied_battle`). | P5-04 spec 24 item 1; props "alliances and trade, P5-04" |
| Q-P5-04-D2 | Spoils of an allied war. | 2026-11-24: "the ally should receive a share and if the other ally doesn't want to give it, it might sour the relationship." | Same, item 2 |
| Q-P5-04-D3 | Elves and faeries and the alliance. | 2026-11-24: as recommended; allied by origin, never broken by a refusal. | Same, item 3 |
| Q-P5-04-D4 | Betrayal and desertion. | 2026-11-24: as recommended. | Same, item 4 |
| Q-P5-04-D5 | Wealth and vassalage. | 2026-11-24: as recommended; no wealth number, vassalage after figures. | Same, item 5 |
| Q-P5-04-D6 | Mines and arms (part C) now or with P5-07? | 2026-11-24: now; schema 11, generator 0.26.0. | Same, item 6; props "Schema 11: mine sites" |
| Q-P5-04-D7 | How is the bloc bound kept? | 2026-11-24: as recommended, by rule (500 to form, 700 to keep). | Same, item 7 |

### P5-02 monsters, P5-01 and P5-12 follow-ups, P5-00 overview

| # | Question | Decision | Source |
| --- | --- | --- | --- |
| Q-P5-02-D1 | Narrow the flare-up rule: a monster's flare-up forces no war. | 2026-11-23: "I accept the narrowing." | props "Paul's decisions of 2026-11-23 on P5-02's open questions" item 1 |
| Q-P5-02-D2 | Drop monster lines from the `civs4` summary if the median passes 1,100. | 2026-11-23: "That's fine to drop." | Same, item 2 |
| Q-P5-02-D3 | Build outposts (watchtowers) in P5-02. | 2026-11-23: "Now is good if it's easy now"; built. | Same, item 3 |
| Q-P5-02-D4 | How a settlement ended: a recorded cause or a new status? | 2026-11-23: a recorded cause (`settlement.ending`); "I take the recommendation." | Same, item 4 |
| Q-P5-02-D5 | Ancient renamings. | 2026-11-23: no change now; later, thresholds relative to the population of the time. | Same, item 5; also P5-12 critic finding 6 |
| Q-P5-02-D6 | Territory colours of P4-03a; share of renamings by capital sack. | 2026-11-23: "the colors look good to me"; "if the capital is sacked the conquerors will want to rename it." | Same entry, "Also decided" |
| Q-P5-02-D7 | Monsters take small settlements preferentially (lair on a former settlement open at the end, 44% was low). | 2026-11-23: "I'd say the monsters would attack smaller settlements preferentially as easier targets." Bound restated to 65 of 100 worlds (see open Q-P5-03-8). | props "Paul's wish of 2026-11-23" |
| Q-P5-02-D8 | May an undead host risen in a ruin overrun one living settlement? | 2026-11-23: "Keep that, makes them more scary." | props "Paul's decisions of 2026-11-23 on P5-02 (undead in ruins, summary length)" |
| Q-P5-02-D9 | The `civs4` summary median 1,101 against a bound of 1,100. | 2026-11-23: "Fine"; the miss accepted. | Same entry |
| Q-P5-01-D1 | T16, repeated battles: restate as a rate. | 2026-11-23: "2 per thousand sounds pretty low, should be fine." | props "P5-01's T16 restated as a rate" |
| Q-P5-01-D2 | The quarrel gate's cost in time (P5-01 question 1). | 2026-11-22: `quarrel_strength_permille` stays 1000. | props "report length target restated, the quarrel threshold kept"; [P5-01 10](specs/p5-01-followups.md) |
| Q-P5-01-D3 | The `civs4` summary length (P5-01 question 2) and T18. | 2026-11-22: default median at most 700, max 800; `civs4` median at most 1,100, max 1,400. | Same entry |
| Q-P5-01-D4 | The critic's three notes (doubled suffixes, `Tarden`, the report's refuges list). | 2026-11-22: "carry them into P5-12." | props "the P5-01 critic's three notes go to P5-12" |
| Q-P5-12-D1 | Should history rename regions, and give mountain peoples a history of their own? | 2026-11-22: both accepted; the share of grim region names lowered in data. | props "history renames regions and a history for the mountain peoples" |
| Q-P5-00-D1 | Sites in P5 or P6? | 2026-11-22: as recommended, P5-02 creates `site`. | props "the P5 overview's questions go with their recommendations" item 1 |
| Q-P5-00-D2 | Gods late with all five kinds of intervention? | 2026-11-22: as recommended, P5-08 after monsters and figures. | Same, item 2 |
| Q-P5-00-D3 | Renamed regions early? | 2026-11-22: as recommended, P5-12 right after P5-01. | Same, item 3 |
| Q-P5-00-D4 | Sub-peoples in P5? | 2026-11-22: none inside P5's stages; one culture per race; hill dwarves optional after P5-07, dark elves after underground geography. | Same, item 4; [P5-00 9.3, 11](specs/p5-00-history-depth-overview.md) |

### P4 viewer v1 (phase approved 2026-11-24)

On 2026-11-24 Paul, after using the viewer on seed 42 at generator 0.24.0: "the viewer works great, no concerns with it." Every choice below stands as built (props "Paul's review of P4 Viewer v1 (2026-11-24)"). The lead's lean towards a ticker strip below the map is dropped.

| # | Question | Decision | Source |
| --- | --- | --- | --- |
| Q-P4-04-1 | Marker shape and size by kind, not by population. | Stands as built. | [P4-04 16 item 1](specs/p4-04-markers.md) |
| Q-P4-04-2 | One ring per event place, with the highest symbol. | Stands as built. | P4-04 16 item 2 |
| Q-P4-04-3 | No markers on a world without history. | Stands as built. | P4-04 16 item 3 |
| Q-P4-04-4 | Hidden refuges drawn like any other. | Stands as built. | P4-04 16 item 4 |
| Q-P4-04-5 | Markers on by default, faded ruins off. | Stands as built. | P4-04 16 item 5 |
| Q-P4-05-1 | Browser in a right-hand pane with Browser and Legend tabs. | Stands as built. | [P4-05 14 item 1](specs/p4-05-browser.md) |
| Q-P4-05-2 | Follow walks the filtered list. | Stands as built. | P4-05 14 item 2 |
| Q-P4-05-3 | One level for list and block. | Stands as built. | P4-05 14 item 3 |
| Q-P4-05-4 | Highlight by dimming. | Stands as built. | P4-05 14 item 4 |
| Q-P4-05-5 | Glyph changes now (heavier ruins, haloed faded ruins, green plague). | Stands as built. | P4-05 14 item 5 |
| Q-P4-05-6 | A plain click on the map selects. | Stands as built. | P4-05 14 item 6 |
| Q-P4-06-1 | Quiet years at ten times the speed, not jumped. | Stands as built. | [P4-06 14 item 1](specs/p4-06-history-movie.md) |
| Q-P4-06-2 | Playback walks the browser's list. | Stands as built. | P4-06 14 item 2 |
| Q-P4-06-3 | Scrubbing during play pauses. | Stands as built. | P4-06 14 item 3 |
| Q-P4-06-4 | The ticker over the map's lower edge. | Stands as built; the lead's lean towards a strip below is dropped. | P4-06 14 item 4 |
| Q-P4-06-5 | Defaults: 10 years a second, skip on, ticker on. | Stands as built. | P4-06 14 item 5 |
| Q-P4-06-6 | The year bar's two rows. | Stands as built. | P4-06 14 item 6 |
| Q-P4-06-7a | Recording format (AVI at 30 fps; PNG sequence for editing). | Stands as built; confirming the command on Windows is open (Q-P4-06-7). | P4-06 14 item 7 |
| Q-P4-03-1 | Humans in teal. | 2026-11-23: "the colors look good to me." | [P4-03 12 item 1](specs/p4-03-history-view.md); props 2026-11-23 decisions |
| Q-P4-03-2 | Inner borders dimmed (not left out). | Same decision. | P4-03 12 item 2 |
| Q-P4-03-3 | Saturated realm colours. | Same decision. | P4-03 12 item 3 |
| Q-P4-03a-1 | Luma bound 175 for the border dim (not 190). | Same decision (colours stand). | [P4-03a 8 item 1](specs/p4-03a-territory-colour-followups.md) |
| Q-P4-03a-2 | Goblin 3 colour. | Same decision. | P4-03a 8 item 2 |
| Q-P4-03a-3 | Half strength for free borders. | Same decision. | P4-03a 8 item 3 |
| Q-P4-00-1 | Layer catalogue in the library. | 2026-11-22: as recommended. | props "the P4 overview's questions go with their recommendations" item 1; [P4-00 11](specs/p4-00-viewer-v1-overview.md) |
| Q-P4-00-2 | The history movie: last stage, playback, Godot's Movie Maker. | 2026-11-22: as recommended. | Same, item 2 |
| Q-P4-00-3 | The viewer opens at the present (year E). | 2026-11-22: as recommended. | Same, item 3 |
| Q-P4-00-4 | Civilization colours: a hue family per race, a shade per civilization. | 2026-11-22: as recommended. | Same, item 4 |
| Q-P4-00-5 | Walkthrough world: seed 42 at 1024. | 2026-11-22: as recommended. | Same, item 5 |

### P3 History core (approved 2026-11-22), questions explicitly decided

Paul's decisions of 2026-11-19 and later on the P3 specs; all "as recommended" unless stated.

| # | Question | Decision | Source |
| --- | --- | --- | --- |
| Q-P3-00-1 to 11 | P3-00 overview 15 items 1 to 11: quiet steps of 25 years; year 0 is the first elven awakening; one creator god per race; territory by whole regions; no stored event text; event rules in code with TOML numbers; roads wait for P5; small worlds not judged; one culture per race; report as `report --history`; settings stored from schema 6, `--divine-script` refused until P5. | 2026-11-19: every interim choice accepted (eleven rows in one). | props "the P3 interim choices are accepted; names are ASCII and unique"; [P3-00 15](specs/p3-00-history-overview.md) |
| Q-P3-02-1 to 3 | P3-02 10: first values of the six cultures' sounds; ASCII only; names unique across kinds. | 2026-11-19: accepted; "names are ASCII and unique". | Same entry; [P3-02 10](specs/p3-02-names.md) |
| Q-P3-03-1 to 6 | P3-03 16: subsets without elves; creator gods of absent races; data errors exit 3; test-pattern worlds record settings; hash cases; `[chronology]` without a format bump. | 2026-11-19: "I accept all the recommendations." | props "the chronology questions"; [P3-03 16](specs/p3-03-chronology.md) |
| Q-P3-04-1 to 6 | P3-04 19: phase lengths scale with span; last age mostly active; flare-up cap 45% floor 10%; catalogue in code; resume as a library seam; a `flare_up` event. | 2026-11-19: "all the recommendations are accepted." | props "the simulation engine questions"; [P3-04 19](specs/p3-04-simulation-engine.md) |
| Q-P3-05-1 | Population density per race. | 2026-11-19: stays a field (`density_permille`); Paul likes deriving it from age but expects it hard to tune. | props "peoples questions: density stays a field"; [P3-05 19](specs/p3-05-peoples.md) |
| Q-P3-05-7 | Faeries in forest. | 2026-11-19: faeries live in forested land, wet or not (data change, shipped in P3-08). | Same |
| Q-P3-05-2 to 6, 8 | Reach from capital; `civs-per-race` is a request; no pressures in quiet steps; secession; migration deferred; plague that empties a settlement does not spread. | 2026-11-19: "The rest ... can go with their recommended resolution." | props "the remaining peoples questions" |
| Q-P3-06-1 to 10 | P3-06 19: limited wars; wars in yearly ticks; prowess per race; no race wiped out; coalition by fear, no vassals; aggression squared; armies march two borders; event count estimate corrected; humans on open land; "already at war" when dwarves arrive. | 2026-11-19: "also accept all the ones in p3-06." | props "the expansion and conflict questions"; [P3-06 19](specs/p3-06-expansion-conflict.md) |
| Q-P3-07-1 to 6 | P3-07 19: report constants; summary size; no filters in v1; world without history refused; islet threshold; stdout default. | 2026-11-19: "And on the history-report.md also." | props "the history report questions"; [P3-07 19](specs/p3-07-history-report.md) |
| Q-P3-07-critic-1 | World critic: keep ids and refs on every line of the summary, or a plain form? | 2026-11-22: keep ids and refs (Paul's choice recorded as A9). | [P5-01 12](specs/p5-01-followups.md); [P3-07 critic](reviews/p3-07-history-report-world-critic.md) |
| Q-P3-08-1 to 5 | P3-08 10: dwarven sites; battle losses from the smaller force; one quiet era per quiet stretch; fewer famines and plague spreads; the three-word template form. | 2026-11-20: "I accept all your recommendations in P3-08 tuning." | props "the P3-08 tuning questions"; [P3-08 10](specs/p3-08-followups.md) |
| Q-P3-02-11.10-1 | English-word rule width in the name filter. | 2026-11-20: the short reject list; "the happy accidents should be preserved." | props "decisions of 2026-11-20 on the name filter questions"; [P3-02 11.10](specs/p3-02-names.md) |
| Q-P3-02-11.10-2 to 7 | Region names share the filter or not; syllable edits; god style; elven realm form; orcish apostrophe parts; public lists. | 2026-11-20: as recommended. | Same |
| Q-P3-09-1 to 11 | P3-09 8: flight of the last refuge; a lost war always costs the loser; revenge; runaway per race; orcs' share; faeries never strike back; race fields; local levies; three borders; no map-size rule; summary rules. | 2026-11-20: "I accept all your recommendations on P3-09 conflict balance." | props "the P3-09 conflict balance questions"; [P3-09 8](specs/p3-09-conflict-balance.md) |
| Q-P3-09-critic-1 to 6 | World critic's six questions on P3-09 (including goblins starting most wars, human aggression). | 2026-11-20: recommendations taken for 1 to 4 and 6; goblins may start most wars; do not raise human aggression. | props "decision of 2026-11-20 on the six questions from the P3-09 review"; [P3-10 10b](specs/p3-10-followups.md) |
| Q-P3-10-1 to 6 | P3-10 10: one compass name per land noun; one quiet era per quiet phase; cause clause placeholder; thousands separators; faerie and dwarven site data; `Mammga` stays. | 2026-11-20: "Yes I agree with the recommendations on all five." | props "the P3-10 questions"; [P3-10 10](specs/p3-10-followups.md) |
| Q-P3-10-7 | Orcs that never quarrel. | 2026-11-20: "having the orcs start with a quarrel with a neighbor is reasonable." | props "orcs awaken with a quarrel" |
| Q-P3-10-R1R2 | The lead's rulings on orc hidden years and aims won by holding the goal. | 2026-11-22: confirmed. | props "the orc rulings confirmed, and four questions answered" items 1, 2 |
| Q-P3-10-critic-1 | World critic: how often may the orcs end broken? Faerie names soft or stranger? | 2026-11-21 and 2026-11-22: faerie names keep their character; the orc age as a gamble is fine. | props "faerie names keep their character"; same 2026-11-22 entry items 4, 5 |
| Q-P3-11-1 | Orcs at war with goblins welcome? | 2026-11-22: "It makes sense that evil would attack evil sometimes." No change. | Same 2026-11-22 entry item 3; [P3-11 critic](reviews/p3-11-followups-world-critic.md) |
| Q-P3-11-2 | A war fought over another region than its claim. | 2026-11-22: a fresh claim on the new goal (option b), done in P5-01. | [P3-11 10 item 1](specs/p3-11-followups.md); same entry item 6 |
| Q-P3-11-3 | The faerie coda `p`. | 2026-11-22: stays removed. | [P3-11 10 item 3](specs/p3-11-followups.md) r3 |
| Q-P3-orcs-1 | Orcs at peace among goblins. | 2026-11-21: accepted. | props "orcs at peace among goblins is accepted" |
| Q-P3-names-1 | Faerie names: keep character, without the baby-talk sounds. | 2026-11-21: decided. | props "faerie names keep their character" |

### P1 Geography (approved 2026-11-19), questions decided by name

| # | Question | Decision | Source |
| --- | --- | --- | --- |
| Q-P1-01-8 | Pass cut cap `pass_max_cut_m`. | 2026-11-17: 500 m. | [P1-01 12 item 8](specs/p1-01-elevation.md) |
| Q-P1-03-7 | Per-world temperature offset. | 2026-11-17: uniform +-2 C in whole tenths. | [P1-03 12 item 7](specs/p1-03-climate.md); props "More natural biomes, per-world temperature" |
| Q-P1-03-1,2 | Wind direction per world; cool climate span. | 2026-11-18: accepted (the "choices of taste for Paul"). | props "Second-pass geography: choices of taste for Paul" |
| Q-P1-04-critic | Heath stripe, fen patches, habitability limits (critic options for Paul). | 2026-11-18: leave as they are. | props "Paul's decisions of 2026-11-18" |
| Q-P1-06-1 | Region names: placeholders or identity. | 2026-11-19: names with identity, no numerals. | [P1-06 12 item 1, 14.5](specs/p1-06-regions.md); props "region names get identity" |
| Q-P1-06-14.5 | The questions r3 raised (14.5). | 2026-11-19: answered; recommendations taken. | props "decisions of 2026-11-19 on the region name questions" |
| Q-P1-approval | P1 as a whole and the world critic's recommendations (1024 reference size, pass by col, river border share, islet threshold). | 2026-11-19: P1 approved, recommendations accepted. | props "P1 Geography approved" |

---

## 3. Settled by phase approval

P0 and P1 were approved by Paul on 2026-11-19 and P3 on 2026-11-22. Each question below was put with a stated interim choice, which the build follows, and no later objection was found. P4-01 and P4-02 are listed on the basis of the P4 approval of 2026-11-24; the lead's note named only P4-04 to P4-06, so check these.

<details>
<summary>P0 Foundation (approved 2026-11-19), 23 questions</summary>

- P0-01 1: Q32.32 fixed point.
- P0-01 2: round half away from zero.
- P0-01 3: overflow is a fatal error, never saturates.
- P0-01 4: xoshiro256** seeded by SplitMix64.
- P0-01 5: phrase normalization (decided in the design doc, 2026-11-15).
- P0-01 6: 2D gradient noise, 8 gradients, quintic fade.
- P0-02 1: schema version 1 holds `meta` and `layer` only.
- P0-02 2: schema-change workflow during the prototype.
- P0-02 3: `layer.id` surrogate column.
- P0-02 4: project codec 1 now, general codec later.
- P0-02 5: working database in memory.
- P0-02 6: map orientation, row 0 north.
- P0-02 7: `seed_phrase` is part of the file.
- P0-03 1: floats rejected, fractions as decimal strings.
- P0-03 2: lower-case file names only.
- P0-03 3: one kind per directory.
- P0-03 4: generator tuning as a data kind (settled by P1-00 item 3).
- P0-03 5: no Unicode normalization of strings.
- P0-04 1: no random default seed.
- P0-04 2: `report` writes to stdout.
- P0-04 3: the `--test-pattern` stage.
- P0-04 4: `render` contract is the pixel buffer.
- P0-04 5: the CLI command list follows section 13.

</details>

<details>
<summary>P1 Geography (approved 2026-11-19), 58 questions with a stated interim choice</summary>

- P1-00 1 to 10: map-size convention; stored-layer rule; tuning as a fixed-id `worldgen` kind; biome categories; layer additions are not schema changes; 15 s budget at 1024; `--threads` default; report format version; second-pass report keys; second-pass catalogue and schema.
- P1-01 1 to 16 (except item 8, decided above): lake_max_cells `int64`; lake share cap always applies; `step_p999_m` hard check; river density; pockets get mountains; no lakes on small maps; pass sites stored; report walkability measure; summits protected by rules; zero "switch" values removed; memory budget; 512 coverage; natural gaps tested first; engineer and code disagreements; findings in the code.
- P1-02 1 to 4: no endorheic lakes; uniform rain; river threshold 0.125 km2; chamfer 5-7 coast distance.
- P1-03 3 to 6, 8, 9: rainfall-weighted discharge not in P1; wind stored in table `climate`; lakes do not recharge the air; rainfall size dependence; one draw for the offset; range check of `temp_offset_dc`.
- P1-04 1 to 7: fixed two-pass window; `wet_window_m` bound 2000; new biome values; steppe fertility 0.6 (revisited in P1-05); fen at 512; rule-field checks; second-version questions resolved.
- P1-05 1 to 7: `forage` fifth score; steppe fertility 0.7; absolute scores; habitability weights 70/15/10/5; fertile threshold 0.65; small walkable pockets; timber without a rain term.
- P1-06 2 to 10: no sea regions; region size and count; borders by height above drainage; large river class 6; upland regions; table `region_biome`; region summary columns; passes linked to borders; islets are regions.

</details>

<details>
<summary>P3 History core (approved 2026-11-22), 14 questions with no explicit decision found (P3: 7, P4-01 and P4-02: 7)</summary>

- P3-01 1: shipped weights are first values (tuned in P3-05).
- P3-01 2: `--test-pattern` accepts history flags and ignores them.
- P3-01 3: cross-kind data conditions checked at the stage.
- P3-07 critic 2 and 3: unpursued claims stay in the chronicle; the summary opens with the chronicle (the report was approved as built; B-notes taken up in P5-01).
- P3-10 round-2 critic N1 and N2: orcs striking in their first century (answered by the P5-01 quarrel gate and Paul's 2026-11-22 answers); foothold cessions name the towns (P3-11 scope).
- P4-01 14 items 1 to 3: colours (later confirmed, 2026-11-23), no realm borders (done in P4-03), exact census only for population between rows.
- P4-02 10 items 1 to 4: no unit listing in `worldgen-cli`; catalogue as code; `RegionRecord` gains `biome`; descriptions not shown yet.

</details>
