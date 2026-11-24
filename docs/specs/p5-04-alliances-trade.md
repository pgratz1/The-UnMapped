# P5-04 Alliances and trade

Status: **r2, approved by Paul on 2026-11-24** with the decisions of section 24 (r1 was the draft of the same day). Written by the spec writer on `p5/04-spec`, cut from `origin/p5/03a-impl` at c5b68ff (generator 0.25.0 in flight, schema 10, history report format v6; P5-03a not yet merged, its code and spec r1 with the calibrated values of its proposals entry are in this tree; this stage is implemented on top of it). Phase P5 History depth, stage 04 (`docs/specs/p5-00-history-depth-overview.md` 3.1 "P5-04", 3.2, 4, 6, 7, 8, 8.3; 3.1 and 6.1 superseded on vassalage, `civilization_census` and `history.economy`, 20). Generator **0.26.0**. **Schema 11**: `site.kind` gains code 3 `mine` (part C, 9.2); relation kinds are text keys (P3-06 4.3) and kinds are appended (P3-04 8.2), so parts A and B alone would not have needed it. History report format **v7** (10.1, the lead's ruling).

Sources: design doc 5 ("Deep time": "the first wars and alliances form"; the race table: faeries "allies of the elves by origin"; "Entities": civilizations with "relations to other civilizations"; "What phase P3 has settled": "Alliances, vassalage, trade and wealth come with phase P5"; "Waiting for phase P5": "aligned peoples in contact form partnerships that strengthen both, and an ally may join a partner's war"; "The plan for phase P5": "Dwarves are great forgers", the exit check "a bloc of allies and vassals is held to the same runaway bound as a single realm"), 6 (`relation`, `site`), 13; P5-00 (3.1, 3.2, 4, 6.1, 7, 8.1 to 8.3); P3-04 r23 (6.9, 8.2, 8.4); P3-06 r23 (4, 5, 6, 7, 20, 21, 22); P3-07 r24; P5-02 r2 (sites); P5-03 r2 and P5-03a r1 (dispositions, `DispositionView`, `credit`); the P5-03a audit's advisory A4 (`origin/p5/03a-audit`, `docs/reviews/p5-03a-relations-followups-audit.md`); `docs/design/proposals.md`: "Paul's notes of 2026-11-20" item 3 (alliances and trade: "a close partner might come to the aid of an ally" in battle), Paul's decision of 2026-11-22 on the mountain peoples, and P5-03a's calibration entry (wars median 25 at 0.25.0, `border_friction` 300). Paul's direction of 2026-11-24 (relayed by the lead): alliances forged, more often rather than less; the lead's scope ruling: alliances are the centre of this stage, trade and wealth are kept thin. Paul's decisions of 2026-11-24 on r1's seven questions (section 24): part C now; allies fight in the partner's battles; an ally receives a share of the spoils, and withholding it sours the alliance; elves and faeries allied by origin; betrayal, wealth, vassalage and the bloc bound as recommended. The lead's rulings of the same day: report format v7, browser group `alliances` = 7, tuning id `history.alliances`, the P5-00 amendment note.

## Plan (approved by Paul, 2026-11-24)

**What a world gains.** Realms now swear alliances, keep them, let them lapse, break them and sometimes betray them. An ally is called to arms when its partner is attacked, and sometimes when its partner attacks; when it comes, its armies fight in its partner's battles, and the battle names both. When the war is won, the ally that fought is owed a share of the spoils; its partner grants it or keeps it, and spoils kept sour the alliance, sometimes until the ally breaks it. Elves and faeries are allied from the faeries' first year, by origin. Realms in reach of each other open trade, which makes both stronger in war and warms their peoples toward each other, and long trade is the usual road to an alliance. Dwarven realms open mines on reachable ore, and their trade partners buy the arms forged there. Every alliance and trade is a `relation` row by year, so the viewer can later draw blocs; every mine is a `site` row and a marker on the map. Sentences: *"After the outbreak of war, the Hold of Durn and the Kingdom of Cear swore to stand together against the Gub Horde."* · *"The Hold of Durn, with the Kingdom of Cear, defeated the Gub Horde in the Grey Fells; 812 of the defeated and 140 of the victors fell."* · *"The Hold of Durn kept the spoils of the Grey Fells from the Kingdom of Cear, its ally."* · *"After the spoils of the Grey Fells were kept from it, the Kingdom of Cear broke its alliance with the Hold of Durn."* · *"The Hold of Durn sold arms forged in the Mines of Kazad Rune to the Kingdom of Cear."* A summary might read:

```
5402 e640 war_declared/70: The Gub Horde declared war on the Hold of Durn over the Grey Fells.
5402 e642 ally_joined_war/40: The Kingdom of Cear came to the defence of the Hold of Durn against the Gub Horde.
5404 e652 allied_battle/35: The Hold of Durn, with the Kingdom of Cear, defeated the Gub Horde in the Grey Fells; 812 of the defeated and 140 of the victors fell.
5409 e661 war_won/60: The Hold of Durn prevailed over the Gub Horde after seven years of war.
5409 e663 spoils_withheld/35: The Hold of Durn kept the spoils of the Grey Fells from the Kingdom of Cear, its ally.
5550 e774 alliance_spurned/50: After the spoils of the Grey Fells were kept from it, the Kingdom of Cear broke its alliance with the Hold of Durn.
5820 e901 alliance_betrayed/65: The Urarz Horde broke faith and made war on the Skreegg Horde, an ally.
[alliances]
alliances: 11 formed, 4 standing at E; ended: 3 lapsed, 2 broken, 1 spurned, 1 betrayed, 1 with a realm
spoils: 6 owed, 4 granted, 2 withheld
```

**The rules in short.**
- *Who may ally*: two realms within three open borders of each other, not at war and not lately at war, whose disposition (their peoples' disposition less the realms' grudge) is at least −300, with a reason: a common enemy, fear of a great realm or bloc, long trade, or friendship. Faeries and elves ally by origin with no reason needed, wherever they live. Good and evil (−700) ally only after centuries of goodwill between their peoples.
- *How one forms*: each tick year a qualifying pair forms one by a keyed draw whose chance grows with the reason and the warmth. Each realm has at most three allies.
- *Kept, renewed, lapsed*: every 150 years an alliance is renewed if a reason still holds, else it lapses. It breaks when the two cool below −350, or when an ally refuses its partner's defence twice.
- *Called to arms*: when a war is declared, the defender's allies are called (likely), then the attacker's (rarely, and only allies that dislike the enemy). An ally that joins fights in its partner's battles near its own lands; the enemy's battles against the side count the ally's levies, and the ally takes its share of the dead. Allies take no land and pay no tribute; the principal realms' peace decides the war. A bled-out ally leaves the war.
- *Spoils*: when a war ends with the ally's side winning, the spoils are the tribute the loser paid plus a quarter of the people of the towns the winner sacked; land is never shared. Each ally still in the war that lost people in it is owed a share in proportion to its dead (at least a tenth, at most four tenths). The partner grants it by a keyed draw that is likelier the warmer the two are and the more the ally did, and less likely the more the partner itself bled. A granted share warms the two peoples. A share kept chills them and leaves a grudge between the two realms; one kept share does not break an alliance by itself, but two in one term end it at its renewal, and an alliance that cools past breaking after spoils were kept is broken by the wronged ally, "after the spoils of the Grey Fells were kept from it".
- *Betrayal*: allies quarrel at a quarter of the usual rate; when one still declares war on the other, that is a betrayal, the alliance ends and the betrayed never trusts the betrayer again.
- *Peoples remember*: forming, joining, sharing spoils and trading warm the two races through P5-03a's `credit`; refusals, kept spoils and betrayals chill them; an ally that fought a people grows colder toward it.
- *Blocs*: no alliance forms when either side's bloc (a realm with its allies) would hold more than half of the land; a bloc above 700‰ sheds its weakest ally at once; the coalition by fear of P3-06 now fears blocs, not only realms.
- *Trade*: realms in reach and not in hatred open trade by a keyed draw (also in quiet centuries); each partner is 5% stronger in war per partner, up to three; trade ends with war, cooling or loss of reach.
- *Mines and arms* (part C): a dwarven realm opens a mine (a `site` of kind `mine`) at its largest town in a region with reachable ore, by a keyed draw; a realm with an open mine is 5% stronger in war (its forges), and each of its trade partners that has no mine of its own buys its arms and is 10% stronger; the first sale to a partner is an event. A mine closes when the realm loses its town or the region. P5-07 Mountain depths deepens these same mines.

**Scope: option C, now** (Paul, 2026-11-24): alliances, thin trade, and the dwarves' mines and arms, in one stage; **schema 11** for the site kind; a mine glyph in the viewer. Wealth is left out: no rule needs a wealth number (a share of spoils is counted in people, as tribute is). Vassalage is deferred until after figures.

**Targets in everyday terms** (1024 default worlds; validation measures, the engineer calibrates): 6 to 15 alliances formed in a typical world, at least 3 standing at the end; between a third and two thirds of them across races; elves and faeries allied in every world where both live; an ally joins in 20% to 45% of wars, mostly in defence; of the shares owed, half to five sixths granted; an alliance spurned over spoils in a fifth to three fifths of worlds, but few: at most one ended alliance in ten; a betrayal in 20% to 60% of worlds, 3% to 15% of alliances ending in one; 8 to 25 trades opened; a mine in nine of ten worlds whose dwarves hold ore, arms sold in most of them; wars per world still 22 to 28 (25 now); no bloc above 800‰ of the land for more than 1,000 years (by rule, never for more than one step, about 25 years); the summary still within its limits.

**Decided** (section 24): r1's seven questions, by Paul on 2026-11-24, and the lead's rulings of the same day.

## 1. Purpose

At 0.25.0 realms meet only as rivals: `enmity`, `war` and `peace` rows, strain and claims (P3-06 4.3, 5). Peoples have dispositions that history moves and that can reach friendship (P5-03, P5-03a), but friendship does nothing between realms except keep them from quarrelling. P5-04 gives realms a positive relation that acts:

- **Alliances** (part A): realms with a reason swear an alliance (relation `alliance`), renew it, let it lapse, break it, or betray it by making war on an ally. An ally is called to arms at a declaration and, when it comes, fights in its partner's battles. At a won war the ally is owed a share of the spoils, which its partner grants or keeps (6.11); kept spoils sour the alliance and may end it (6.7). Joining, refusing, sharing, keeping and betraying move the two peoples' disposition.
- **Trade** (part B, thin): realms in reach whose disposition allows it keep a trade (relation `trade`), which strengthens both in war and warms their peoples; long trade is a reason to ally.
- **Dwarven mines and arms** (part C): dwarven realms open mines on reachable ore (`site` kind 3 `mine`, schema 11); a realm with a mine is stronger (its forges), and its trade partners buy its arms and are stronger (6.12, 6.13, 7.5). Section 19 lists where each of part C's rules is and the seam to P5-07.

The runaway bound of P5-00 8.3 is extended to blocs and kept by rule (6.7, 7.3).

## 2. Items and where their normative text is

| # | Item | Normative text | Amends | Changes worlds |
| --- | --- | --- | --- | --- |
| A1 | Race fields `origin_allies`, `ally_join_permille`; tuning `history.alliances`; five `history.conflict` fields | 4 | P3-01 (fields, id), P3-06 12.1 | yes |
| A2 | `AlliancesSubsystem`: formation, origin, calls, betrayal, renewal, lapse, breaks, blocs, credits | 5, 6 | P3-04 6.4 (configuration) | yes |
| A3 | Seams into conflict: allied strain, fear of blocs, deterrence, joined allies, allied battles, withdrawal, ends | 7 | P3-06 5.1, 5.2, 6.1, 7.2, 7.3, 7.7, 7.8, 8.1, 20.15 | yes |
| A4 | Event kinds 63 to 71; renderer forms `with`, `against` | 8 | P3-04 8.2, 8.4 | yes |
| A5 | Spoils: the share owed, granted or withheld, and the alliance spurned; events 74 to 76; seams `war_spoils`, `add_grudge` | 6.7, 6.11, 7.15, 8 | P3-06 4.2 (a grudge source), 7.10 | yes |
| B1 | Trade: relation `trade`, its strength, credit, events 72, 73 | 6.8, 7.5, 8 | P3-06 6.1 | yes |
| C1 | Mines: race field `mining_permille`, site kind 3 `mine`, event 77 | 4.1, 4.2, 6.12, 8, 9.2 | P3-01, P5-02 11 | yes, schema 11 |
| C2 | Forges and arms: strength of a realm with a mine and of its buyers, event 78 | 4.3, 6.13, 7.5, 8 | P3-06 6.1 | yes |
| C3 | The mine marker | 11.3 | P4-04 3.6 | viewer, overlay images |
| D1 | Relation kinds, site kind, schema 11, reader checks | 9 | P3-04 9, P5-02 11.4, `schema.md` 1, 5, 6 | schema 11 |
| D2 | Report v7 | 10 | P3-07 (new section 30), P5-02 12.5 | report |
| D3 | Battle kinds set; browser groups; markers | 11 | P3-07, P4-04, P4-05 4.4, P5-02 6.3.2, P5-12 data | yes |
| D4 | Timeline reads | 12 | P4-01 (r6) | no |

## 3. Inputs, streams, draws

- **Live state** (`State`): civilizations (race, founded year and event, ended), settlements (owner, region, status, population, founding event), regions (owner, owner event, `ore_reach_cells` of P1-06), relations (`war`, `peace`, `alliance`, `trade`), sites (P5-02; this stage adds and ends `mine` sites, 6.12).
- **Conflict's const reads** (7.13, 7.15): `disposition_of`, `at_war`, `open_wars_of`, `force`, `reaches`, `truce_over`, `lying_low`, `share`, `last_peace`, `war_open`, `war_end_event`, `war_spoils`; and two mutators, `join_war` (7.6) and `add_grudge` (7.15).
- **Relations**: `DispositionView` (`disposition`, `band`, `met`, `hatred_event`, and the new `turn_event`, 7.13) and `RelationsSubsystem::credit` (P5-03a 3.5).
- **Peoples**: `PeoplesView::hops` (open borders, P3-05 4.4), `population`.
- **The log**: `war_declared` events from a cursor (6.5), through `StepContext`'s reads.
- **Data**: race fields `origin_allies`, `ally_join_permille`, `mining_permille` (4.1) and `muster_permille` (P3-06 20.6); `history.alliances` (4.2); `history.conflict`'s new fields (4.3) and `shelter_years`, `fear_share`, `exhaustion_permille`, `truce_years`, `war_grudge`.
- **Settings**: none. `param.event_intensity` does not scale this stage (its wars already scale by it).
- **Streams** (P5-00 4.2), appended to `streams.hpp` after `hist.relations.settlers`, in this order: `hist.alliances.form`, `hist.alliances.join`, `hist.alliances.trade`, `hist.alliances.spoils`, `hist.alliances.mine`.

| Stream | a, b, k | Section |
| --- | --- | --- |
| `hist.alliances.form` | civilization x, civilization y (x < y), 0 | 6.3 |
| `hist.alliances.join` | the war's `war_declared` event w, the called ally z, 0 | 6.5 |
| `hist.alliances.trade` | civilization x, civilization y (x < y), 0 | 6.8 |
| `hist.alliances.spoils` | the war's `war_declared` event w, the ally z, 0 | 6.11 |
| `hist.alliances.mine` | civilization c, region R, 0 | 6.12 (part C) |

`D(stream; a, b, k)` = `StepContext::draw(stream, a, b, k)` with w0 the step's year (a quiet step's start); `chance_pm`, `pick` as P5-02 3. All arithmetic int64 with checked multiplication; `div` is floor division of non-negative values; `clamp(v, lo, hi)` = min(hi, max(lo, v)).

# Part A. Alliances

## 4. Data

### 4.1 Race fields (amends P3-01)

Appended after `stances` in every race definition (shipped and both fixture roots), all three required:

| Field | Type | Meaning |
| --- | --- | --- |
| `origin_allies` | array(string(32), 0, 8) | races this race is allied with by origin (6.4) |
| `ally_join_permille` | int(0, 1000) | the race's readiness to send its armies to an ally's war (6.5) |
| `mining_permille` | int(0, 1000) | the race's readiness to open mines (6.12; part C) |

Check `race_origin_allies` (with the history data checks, the error path of P5-03 4.1): `origin_allies[<i>] '<r>' is not a race`; `… is the race itself`; `… is listed twice`; and, after every race is loaded, `origin_allies '<r>' is not returned by race '<r>'` (the relation must be listed by both races).

Shipped (taste):

| race | `origin_allies` | `ally_join_permille` | `mining_permille` | why |
| --- | --- | --- | --- | --- |
| `elf` | `faerie` | 700 | 0 | the elves keep faith with their kin of the good gods |
| `faerie` | `elf` | 150 | 0 | "faeries avoid battle" (design 5): they seldom send an army |
| `dwarf` | — | 800 | 1000 | slow to ally, stubborn once sworn; "great forgers" (design 5) |
| `goblin` | — | 400 | 0 | numerous, unreliable |
| `human` | — | 600 | 0 | |
| `orc` | — | 600 | 0 | |

Keyed by data, never by race key in code (P5-00 9.3).

### 4.2 Tuning `history.alliances` (new fixed id of kind `history`; `game/data/history/alliances.toml` and both fixture roots)

All required, in this order. Starting values; the engineer calibrates them (15.2) and r2 records the shipped ones.

| Field | Type | Start | Meaning |
| --- | --- | --- | --- |
| `ally_hops` | int(−1, 8) | 3 | reach: presences within this many open borders (6.2); −1: never (a test seam) |
| `ally_min_disposition` | int(−2000, 2000) | −300 | the least disposition at which two realms ally |
| `ally_full_disposition` | int(−2000, 2000) | 300 | at or above this the warmth is full (6.2) |
| `ally_friend_disposition` | int(−2000, 2000) | 150 | friendship is a reason at or above this (6.2) |
| `ally_break_disposition` | int(−2000, 2000) | −350 | an alliance breaks below this (6.7) |
| `allies_max` | int(0, 16) | 3 | allies a realm may have at once |
| `ally_after_war_years` | int(0, 100000) | 100 | years after a war between two realms before they may ally |
| `foe_years` | int(1, 100000) | 100 | a common enemy counts while its war is open or declared within this (6.2) |
| `fear_share` | int(0, 1000) | 300 | a realm or bloc at or above this share is feared (6.2) |
| `form_foe_permille` | int(0, 1000) | 40 | yearly weight of a common enemy |
| `form_fear_permille` | int(0, 1000) | 30 | yearly weight of fear |
| `form_trade_permille` | int(0, 1000) | 20 | yearly weight of long trade |
| `form_friend_permille` | int(0, 1000) | 10 | yearly weight of friendship |
| `form_max_permille` | int(0, 1000) | 60 | cap of the yearly chance |
| `bloc_share_max` | int(0, 1000) | 500 | no alliance forms that makes a bloc hold more (6.3) |
| `bloc_lapse_share` | int(0, 1001) | 700 | a bloc above this sheds allies (6.7); 1001: never |
| `term_years` | int(1, 100000) | 150 | years between renewals (6.7) |
| `refusals_to_break` | int(1, 100) | 2 | refused defence calls that break an alliance |
| `defend_permille` | int(0, 1000) | 700 | base chance that an ally joins its partner's defence |
| `attack_permille` | int(0, 1000) | 200 | base chance that an ally joins its partner's attack |
| `hostility_full` | int(1, 2000) | 500 | an ally's hostility toward the enemy at which an attack call is at full chance |
| `ally_wars_max` | int(1, 16) | 2 | an ally joins only while its open wars are fewer than this |
| `ally_memory` | int(0, 1000) | 30 | `credit` at a formation |
| `renew_memory` | int(0, 1000) | 15 | at a renewal |
| `join_memory` | int(0, 1000) | 30 | at a join, between ally and partner |
| `refuse_memory` | int(0, 1000) | 40 | lost at a refusal |
| `betray_memory` | int(0, 1000) | 150 | lost at a betrayal |
| `ally_war_memory` | int(0, 1000) | 75 | lost between an ally and its enemy when their war ends (as `war_memory`) |
| `trade_hops` | int(−1, 8) | 3 | trade reach (6.8); −1 never |
| `trade_min_disposition` | int(−2000, 2000) | −250 | least disposition for trade |
| `trade_break_margin` | int(0, 2000) | 100 | trade ends below `trade_min_disposition` − this |
| `trade_after_war_years` | int(0, 100000) | 50 | years after a war before trade |
| `trade_permille` | int(0, 1000) | 15 | yearly chance at full warmth |
| `trade_max` | int(0, 16) | 4 | trades a realm may keep |
| `trade_credit_years` | int(1, 100000) | 50 | trade warms its peoples once per this |
| `trade_memory` | int(0, 1000) | 20 | `credit` per `trade_credit_years` held |
| `trade_ally_years` | int(0, 100000) | 100 | trade held this long is a reason to ally |
| `spoils_plunder_permille` | int(0, 1000) | 250 | the part of a sacked town's people counted as spoils (6.11) |
| `spoils_floor_permille` | int(0, 1000) | 100 | the least part of the spoils an ally that fought is owed |
| `spoils_cap_permille` | int(0, 1000) | 400 | the most part of the spoils one ally is owed |
| `spoils_grant_permille` | int(0, 1000) | 450 | base chance that the principal grants a share |
| `spoils_warmth_permille` | int(0, 1000) | 300 | added at full warmth between principal and ally |
| `spoils_part_permille` | int(0, 1000) | 500 | added per mille of the ally's part (at the cap 400: + 200) |
| `spoils_need_permille` | int(0, 1000) | 400 | taken away when the principal is bled to exhaustion |
| `spoils_grant_min` | int(0, 1000) | 100 | least chance of a grant |
| `spoils_grant_max` | int(0, 1000) | 950 | most chance of a grant |
| `share_memory` | int(0, 1000) | 25 | `credit` at a granted share |
| `withheld_memory` | int(0, 1000) | 50 | lost at a withheld share |
| `withheld_grudge` | int(0, 2000) | 120 | grudge between the two realms at a withheld share |
| `withheld_to_break` | int(1, 100) | 2 | withheld shares in one term that end the alliance at its renewal (6.7) |
| `mine_permille` | int(0, 1000) | 20 | yearly chance per region with ore (6.12; part C) |
| `mine_ore_min` | int(1, 100000) | 1 | least `ore_reach_cells` of a mine's region |
| `mine_settlement_min` | int(0, 1000000) | 200 | least population of the town a mine is opened at |
| `arms_trade_permille` | int(0, 4000) | 2000 | trade's opening chance for a pair with exactly one seller of arms, per mille of the usual (6.13) |
| `mine_forms` | array(string(64), 1, 4) | `["The Mines of {settlement}", "The Delvings of {settlement}"]` | mine names (6.12) |

Checks (with the history data checks): `alliances_spoils`: `spoils_floor_permille` ≤ `spoils_cap_permille` (`spoils_floor_permille <a> is above spoils_cap_permille <b>`), `spoils_grant_min` ≤ `spoils_grant_max` (`spoils_grant_min <a> is above spoils_grant_max <b>`); `alliances_mine_forms`: every form contains `{settlement}` exactly once and no other placeholder (`mine_forms[<i>] '<form>' must contain {settlement} once`); `alliances_dispositions`: `ally_min_disposition` < `ally_full_disposition` (`ally_min_disposition <a> is not below ally_full_disposition <b>`), `ally_break_disposition` < `ally_min_disposition` (`ally_break_disposition <a> is not below ally_min_disposition <b>`); `alliances_blocs`: `bloc_share_max` < `bloc_lapse_share` (`bloc_share_max <a> is not below bloc_lapse_share <b>`).

**The `zero` fixture tuning** (proof obligation I0, 7.14): `ally_hops` −1, `trade_hops` −1, `mine_permille` 0, every race's `origin_allies` empty, the others as shipped; with the `history.conflict` fields of 4.3 at any value (allied strain and deterrence are inert without alliances; `trade_strength_permille` without trade; `forge_strength_permille` and `arms_strength_permille` without mines; spoils without joined allies).

### 4.3 `history.conflict` fields (appended after `capital_captured_salience`; amends P3-06 12.1)

| Field | Type | Start | Meaning |
| --- | --- | --- | --- |
| `allied_strain_permille` | int(0, 1000) | 250 | strain between allies, per mille of the usual rate (7.2) |
| `ally_deterrence_permille` | int(0, 1000) | 300 | the defender's allies' force counted in a declaration's odds (7.4) |
| `ally_effort_permille` | int(0, 1000) | 1000 | an ally's local force counted in its partner's battle (7.7) |
| `trade_strength_permille` | int(0, 1000) | 50 | force gained per trade partner (7.5) |
| `trade_strength_max` | int(0, 16) | 3 | trade partners counted for strength |
| `forge_strength_permille` | int(0, 1000) | 50 | force gained by a realm with an open mine (7.5; part C) |
| `arms_strength_permille` | int(0, 1000) | 100 | force gained by a buyer of arms (7.5; part C) |

## 5. State

`AlliancesSubsystem` (`worldgen/src/history/alliances.cpp`, header `worldgen/history/alliances.hpp`) keeps in its own members, ids only:

- **Alliances**, keyed by the pair (x, y), x < y, open ones only: formed year and event, basis, foe (0 none), origin flag, the year and event of the last renewal (the formation at first), refusals, and the **withheld** record of the current term (6.11): count (0 at formation and at each renewal), the last `spoils_withheld` event, its region and its wronged ally.
- **Betrayed pairs**: an ordered set of (x, y), x < y, that never ally again.
- **Trades**, keyed by (x, y), open ones only: opened year and event, next credit year.
- **Joined records**, in record order: (war key w, ally z, partner p, enemy e, join event, credited flag, settled flag (6.11)).
- **Mines** (part C): open mines keyed by (c, R): site id, settlement t; and the set of settlements that ever had a mine (6.12).
- **Arms sold**: the ordered set of (seller, buyer) pairs whose trade has had its `arms_sold` (6.13), cleared for a pair when its trade closes.
- The **log cursor** (first event id not yet read).
- A per-step **cache** of presences, shares and bloc shares (6.2), rebuilt at the start of each step.

Bounds: alliances ≤ civilizations × `allies_max` / 2; trades ≤ civilizations × `trade_max` / 2; joined records ≤ wars × civilizations (each a war of `wars_` in conflict).

## 6. Rules

### 6.1 Steps and order

`steps()` = T2, T6, Q4. Installed in `EngineConfig::subsystems` **after** `RelationsSubsystem`: peoples, conflict, renaming, monsters, relations, alliances (P5-00 4.1). Not a tension source. Y is the step's year (a quiet step's start), L its length.

| Step | In this order |
| --- | --- |
| T2 | the cache (6.2); the log: for each new `war_declared` ascending, betrayal (6.6), trade broken by war (6.8), calls (6.5); cooling (6.7); blocs too great (6.7); renewals and lapses (6.7); origin alliances (6.4); formations (6.3); trade breaks by cooling and reach, trade openings (6.8); trade credits (6.9); mine closings, mine openings (6.12); arms (6.13) |
| T6 | ends of joined wars: spoils (6.11), then the ally–enemy credits (6.9); mine closings (6.12) |
| Q4 | the cache; ends of joined wars (spoils, credits); cooling; blocs too great; renewals and lapses; trade breaks, trade openings; trade credits; mine closings, mine openings; arms. No formation, origin alliance, betrayal or call (declarations are T2 only, P3-06 7.8) |

### 6.2 Terms

All read once per step into the cache (live state at the step's start for this subsystem: after the earlier subsystems of the step), except where a rule says "live".

- **Alive, visible, free**: c alive and visible (P3-06's `alive_visible`) and not lying low (`ConflictSubsystem::lying_low`, P3-06 22.3).
- **Presence** P(c): P3-06 5.1's simple definition (as P5-03 6.2.4).
- **Reach**: hops(x, y) = the smallest `PeoplesView::hops(R_x)[R_y]` over R_x ∈ P(x), R_y ∈ P(y) (unreachable: none). x and y are **in reach** when hops(x, y) ≤ `ally_hops`; **in trade reach** when ≤ `trade_hops`. Symmetric.
- **Disposition** d(x, y) = `ConflictSubsystem::disposition_of(x, y)` (P3-06 4.1 as amended by P5-03 7.1: D(r_x, r_y) − grudge{x, y}).
- **Warmth** w(x, y) = clamp((d(x, y) − `ally_min_disposition`) × 1000 div (`ally_full_disposition` − `ally_min_disposition`), 0, 1000), for d ≥ `ally_min_disposition`.
- **Share** s(c) = `ConflictSubsystem::share(c)` (P3-06 5.1: non-islet regions held × 1000 div N). **Allies** A(c): the realms in an open alliance with c, ascending. **Bloc share** bs(c) = min(1000, s(c) + Σ_{z ∈ A(c)} s(z)). A **bloc** is a realm with its direct allies (P5-00 8.3); allies of allies are not in it.
- **At war** (x, y): an open `war` relation in either direction (a principal war or a joined one, 7.6).
- **Lately at war** (x, y): at war, or the pair's `last_peace` year (P3-06 3.2) is above Y − `ally_after_war_years` (for trade, − `trade_after_war_years`).
- **Reasons** of a pair (x, y), each with its cause event and, for the first two, its foe z:
  1. **Common enemy**: the z ∉ {x, y}, alive and visible, with a `war_declared` between z and one of x, y (either side, as principal) that is open or whose year is above Y − `foe_years`, such that the other of x, y has d(·, z) < 0 and is in reach of z. Cause: the latest such `war_declared` (largest id); z its other side. (A joined war does not make a common enemy; only declarations do.)
  2. **Fear**: the z ∉ {x, y} with bs(z) ≥ `fear_share`, alive and visible, not allied with x or y, in reach of both, with d(x, z) < 0 and d(y, z) < 0; the z of the largest bs(z), then the smaller id. Cause: the largest `owner_event` among the regions z holds (the latest gain of land).
  3. **Trade**: an open trade between x and y opened at least `trade_ally_years` before Y. Cause: its `trade_opened`.
  4. **Friendship**: d(x, y) ≥ `ally_friend_disposition`. Cause: the later of the younger realm's `founding_event` and `turn_event(r_x, r_y)` when that turn was into friendship (7.13); for r_x = r_y the younger's founding event.
- **Basis** of an alliance: 0 **origin** (6.4), 1 **common enemy**, 2 **fear**, 3 **friendship**, 4 **trade**: the first reason present in the order common enemy, fear, trade, friendship (codes 1, 2, 4, 3).

### 6.3 Formation (T2)

For each pair (x, y), x < y ascending, of civilizations that are alive, visible and free, not allied, not in the betrayed pairs, not lately at war, in reach, with d(x, y) ≥ `ally_min_disposition`, |A(x)| < `allies_max` and |A(y)| < `allies_max` (live: counts change as alliances form in this loop):

```
m = (common enemy ? form_foe_permille : 0) + (fear ? form_fear_permille : 0)
  + (trade ? form_trade_permille : 0) + (friendship ? form_friend_permille : 0)
p = min(form_max_permille, m × w(x, y) div 1000)
```

If p > 0 and both bs(x) + s(y) + Σ_{z ∈ A(y)} s(z) ≤ `bloc_share_max` and bs(y) + s(x) + Σ_{z ∈ A(x)} s(z) ≤ `bloc_share_max` (the blocs after forming; bloc shares live, 6.2's cache updated for this loop), and `chance_pm(D(hist.alliances.form; x, y, 0), p)`: append **`alliance_formed`** (region 0, settlement 0; participants x `ally_a`, y `ally_b`, and the foe z as `foe` for bases 1 and 2; value `basis`; causes the reasons' cause events present, ascending); open relation `alliance` (x, y) with it; record the alliance (renewal = the formation); `credit(r_x, r_y, ally_memory, the event)`.

The bloc test comes before the draw, so no draw is made for a pair it refuses.

### 6.4 Origin alliances (T2)

For each pair (x, y), x < y ascending, alive, visible and free, not allied, not in the betrayed pairs, not at war, whose races list each other in `origin_allies`: if |A(x)| < `allies_max`, |A(y)| < `allies_max` and the bloc test of 6.3 passes: append `alliance_formed` with `basis` 0, no foe, cause the younger realm's `founding_event`; open the relation; record it with the origin flag; `credit(r_x, r_y, ally_memory, event)`. No draw and **no reach is needed**: kin of one camp know each other wherever they live (reading, 25). An origin alliance is never renewed by an event (6.7), never broken by a refusal (6.5), and its ally is called as any other.

### 6.5 Calls to arms (T2)

**Reading the log.** Read every event from the cursor to `last_event_id()` in ascending id, then advance the cursor; act on each `war_declared` w (attacker a, defender b), in id order: betrayal first (6.6), then trade broken by war (6.8), then the calls below. Events of earlier steps are already read (the cursor only ever reads this step's declarations in practice: declarations happen only in conflict's T2).

**Who is called.** First the defender's allies z ∈ A(b) ascending (a **defence call**, side 0), then the attacker's allies z ∈ A(a) ascending (an **attack call**, side 1); the enemy e is a for a defence call and b for an attack call, the partner p the other. A call is made to z only when all hold (live), and is otherwise not made (no event):

- z is alive, visible and free; z ∉ {a, b};
- z is not allied with e (allied with both sides, it stays out);
- z is not at war with e already (it fights already) and not at war with p;
- `ConflictSubsystem::reaches(z, e)` (z's armies can reach the enemy, 7.13);
- Y ≥ `founded_year`(e) + `shelter_years` (P3-06 7.2's shelter: a new realm is not attacked in its first years, by allies either);
- `open_wars_of(z)` < `ally_wars_max`;
- for an attack call only: `truce_over(z, e)` and d(z, e) < 0.

**The chance.**

```
defence:  p = defend_permille × ally_join_permille(r_z) div 1000 × w(z, p) div 1000 × σ div 1000
          σ = min(1000, (force(z) + force(p)) × 1000 div max(1, force(e)))
attack:   p = attack_permille × ally_join_permille(r_z) div 1000 × h div 1000
          h = min(1000, −d(z, e) × 1000 div hostility_full)
```

(w(z, p) is 0 when d(z, p) < `ally_min_disposition`; force is P3-06 6.1's whole force as amended in 7.5.) z **joins** if `chance_pm(D(hist.alliances.join; w, z, 0), p)`:

- **Join**: append **`ally_joined_war`** (region 0, settlement 0; participants z `ally`, p `partner`, e `enemy`; value `side`; causes w and the alliance's formation or last renewal event, ascending); `ConflictSubsystem::join_war(ctx, w, z, that event)` (7.6); a joined record (w, z, p, e, event); `credit(r_z, r_p, join_memory, event)`.
- **Refusal of a defence call** (not joined, side 0, the alliance not of origin): append **`ally_refused`** (participants z `ally`, p `partner`, e `enemy`; causes w and the alliance's formation or last renewal event); refusals += 1; `credit(r_z, r_p, −refuse_memory, event)`; if refusals ≥ `refusals_to_break`: append **`alliance_broken`** (participants the pair as `ally_a`, `ally_b`; value `reason` 0; cause the refusal); close the relation with it; drop the alliance.
- An origin ally that does not join, and an attack call refused, write nothing.

**No chain.** Calls are made only at a `war_declared`: a realm attacked by its enemy's ally does not call its own allies (the war's sides called theirs at its declaration). So one declaration brings at most |A(a)| + |A(b)| calls.

### 6.6 Betrayal (T2, in the log reading)

For a `war_declared` w whose attacker a and defender b are allied (an open alliance, read live): append **`alliance_betrayed`** (region 0, settlement 0; participants a `betrayer`, b `betrayed`; causes w and the alliance's formation or last renewal event, ascending); close the `alliance` relation with it; drop the alliance; add (min, max) to the betrayed pairs; `credit(r_a, r_b, −betray_memory, event)`. The calls of 6.5 follow without a (it is no longer b's ally).

A betrayal happens only through conflict's own declaration: allies still build strain, at `allied_strain_permille` of the rate (7.2), so the warlike turn on their allies (orcs on goblins, design 5 "evil attacks evil at times") and the mild almost never do.

### 6.7 Keeping an alliance (T2 and Q4)

In this order, each over the open alliances ascending (x, y), on a copy of the keys taken before the pass (P3-04 6.9 rule 1):

1. **Cooling**: d(x, y) < `ally_break_disposition` (and the alliance not of origin):
   - if the term's withheld count is 0: append `alliance_broken` (value `reason` 1; cause the alliance's formation or last renewal event); close; drop;
   - else the alliance is **spurned**: append **`alliance_spurned`** (region: the withheld record's region; participants the wronged ally `ally`, the other `principal`; value `reason` 1; causes the last `spoils_withheld` and the formation or last renewal event, ascending); close; drop.
2. **Blocs too great**: for each civilization c ascending with bs(c) > `bloc_lapse_share` (live): while bs(c) > `bloc_lapse_share` and A(c) is not empty, the alliance of c with the z ∈ A(c) of the smallest s(z) (then the larger id) lapses: append **`alliance_lapsed`** (participants the pair; value `reason` 1; cause the alliance's formation or last renewal event); close; drop. Origin alliances lapse by this rule too (the bound is the exit check's).
3. **Renewal**: an alliance with Y ≥ renewal year + `term_years` (in Q4: Y + L − 1 ≥ it, tested at Y):
   - of origin: its renewal year becomes Y, no event; the withheld count becomes 0;
   - else if the term's withheld count ≥ `withheld_to_break`: append `alliance_spurned` (as in item 1, value `reason` 0); close; drop;
   - else if some reason of 6.2 holds: append **`alliance_renewed`** (participants the pair and the foe for bases 1 and 2; value `basis` by 6.2's order; causes the reasons' events and the last renewal or formation, ascending); the renewal year and event become Y and it; the withheld count becomes 0; `credit(r_x, r_y, renew_memory, event)`;
   - else: append `alliance_lapsed` (value `reason` 0; cause the last renewal or formation); close; drop.

**Spoils and breaking** (Paul, 2026-11-24: "if the other ally doesn't want to give it, it might sour the relationship"). A withheld share (6.11) is a grievance in two places: the peoples' memory (`credit`, − `withheld_memory`) and the realms' grudge (+ `withheld_grudge`, which decays as every grudge, P3-06 4.2), so d(x, y) falls by up to 170 at the shipped values. No rule breaks an alliance at one withheld share: it is renewed at its term if a reason holds. Two in one term (`withheld_to_break`) end it at its renewal; and an alliance already cool that the grievance takes below `ally_break_disposition` is broken by cooling, written as spurned because spoils were kept in its term (the last straw, not the only cause). An alliance formed at d ≥ `ally_min_disposition` + 170 = −130 cannot be broken by one withheld share in the year of it. Origin alliances are never spurned (their bond is not of interest, 6.4); their peoples still remember. In the sentence the spoils are named by the war's goal region at the year of the break: *"After the spoils of the Grey Fells were kept from it, the Kingdom of Cear broke its alliance with the Hold of Durn."*

Every event of this section in Q4 needs `quiet_events_left` > 0 of its first civilization participant (x, the smaller id; for `alliance_spurned` the wronged ally), else it waits for the next T2 or Q4.

An alliance with an ended side is closed by conflict's housekeeping (P3-06 8.1 item 1, which closes every open relation of an ended side with its ending event) or by a conquest (P3-06 7.6); this subsystem drops it at its next step with no event (counted as "ended with a realm", 10.2).

### 6.8 Trade (part B; T2 and Q4)

**Broken by war** (T2, in the log reading after 6.6): a `war_declared` w between two trading realms: append **`trade_broken`** (participants the pair as `partner_a`, `partner_b`; value `reason` 0; cause w); close `trade`; drop.

**Breaks** (T2 and Q4, open trades ascending, on a copy of the keys): d(x, y) < `trade_min_disposition` − `trade_break_margin`: `trade_broken` reason 1, cause the `trade_opened`; else not in trade reach (hops > `trade_hops` + 1, a margin of one border): reason 2, same cause. Close; drop.

**Openings** (T2 and Q4): for each pair (x, y), x < y ascending, alive, visible and free, not trading, not lately at war (`trade_after_war_years`), in trade reach, with d(x, y) ≥ `trade_min_disposition`, fewer than `trade_max` trades each (live):

```
wt = clamp((d(x, y) − trade_min_disposition) × 1000 div (ally_full_disposition − trade_min_disposition), 0, 1000)
p  = trade_permille × wt div 1000                    T2
p  = min(1000, trade_permille × wt div 1000 × L)     Q4
```

For a pair with exactly one seller of arms (part C, 6.13), p = min(1000, p × `arms_trade_permille` div 1000).

if `chance_pm(D(hist.alliances.trade; x, y, 0), p)`: append **`trade_opened`** (participants x `partner_a`, y `partner_b`; cause: the largest of the younger's `founding_event`, the pair's `last_peace` event and `peoples_met` of (r_x, r_y) when r_x ≠ r_y); open relation `trade` (x, y); next credit year = Y + `trade_credit_years`. In Q4 the events of this section need `quiet_events_left(x)` > 0.

Trade is open in quiet centuries as well as in active phases: peace is when trade grows. `ally_full_disposition` > `trade_min_disposition` is required (`alliances_trade`: `trade_min_disposition <a> is not below ally_full_disposition <b>`).

### 6.9 Credits (P5-03a 3.5)

Every disposition change of this stage goes through `RelationsSubsystem::credit(race_a, race_b, units, event)`, which acts only when the races differ and have met (P5-03a 3.5). Each call is made at a fixed point of this subsystem's `run`, in the order of 6.1, with the id of an event already appended (the P5-03a audit's A4).

| When | Races | Units | Event |
| --- | --- | --- | --- |
| formation (6.3, 6.4) | the pair | + `ally_memory` | `alliance_formed` |
| renewal (6.7) | the pair | + `renew_memory` | `alliance_renewed` |
| join (6.5) | ally, partner | + `join_memory` | `ally_joined_war` |
| refusal (6.5) | ally, partner | − `refuse_memory` | `ally_refused` |
| betrayal (6.6) | the pair | − `betray_memory` | `alliance_betrayed` |
| a share granted (6.11) | principal, ally | + `share_memory` | `spoils_shared` |
| a share withheld (6.11) | principal, ally | − `withheld_memory` | `spoils_withheld` |
| a joined war's end (below) | ally, enemy | − `ally_war_memory` | the war's ending event |
| arms sold (6.13) | seller, buyer | + `trade_memory` | `arms_sold` |
| trade held (below) | the pair | + `trade_memory` | `trade_opened` |

**Ends of joined wars** (T6 and Q4): for each joined record, in record order, that is not yet settled or not yet credited: first, if not settled, its spoils (6.11); then, if not credited, when `war_open(w)` is false or the ally's joined row is closed (`ConflictSubsystem::ally_left(w, z)`, 7.13): `credit(r_z, r_e, −ally_war_memory, E_w)` with E_w the war's ending event, or the `ally_withdrew` when the ally left first, or the ending event of a side when a side ended (7.12); credited = true.

**Trade credits** (T2 and Q4): for each open trade ascending with Y ≥ next credit year: `credit(r_x, r_y, trade_memory × k, its trade_opened)` with k = 1 in T2 and k = 1 + (Y + L − 1 − next) div `trade_credit_years` in Q4 (every period the step covers); next credit year += k × `trade_credit_years`.

The memory moves reach `RelationsSubsystem`'s bands at its next T6 or Q4 (it runs before this subsystem in every shared step), and conflict's disposition at its next call.

### 6.10 P3-04 6.9, visibility, quiet steps

1. No `found_civilization` or `found_settlement`. Sites of kind 3 only, through 7.16. `open_relation` and `close_relation` for `alliance` and `trade` only (symmetric, a < b, `a_type` = `b_type` = 3; through P3-06 8.3 N4's ordering helper); every pass that opens or closes copies its keys first, never inside a loop over `relations()`. The `war` and `peace` rows of joined wars are conflict's (7.6, 7.11). Members hold ids.
2. Not a tension source.
3. Fixed place: after relations in `subsystems`, built in `stages/history.cpp`, which also sets `conflict.set_alliances(&alliances)` (7.1) and `alliances.set_sites(&monsters)` (7.16).
4. Visibility, below.
5. Keys: civilization ids, the war's declaration event id, region ids (mines).
6. Relation rows: `alliance`, `trade` (9); site rows of kind 3 (9.2).
7. No region owner changes.

**What it sees in shared steps.** T2: peoples' colonies and conflict's strain, claims and declarations of the step, live (the declarations through the log). T6: conflict's battles and war ends, relations' log reading, live. Q4: conflict's quiet ends of wars, live. Civilizations founded in the step are invisible to it (P3-04 6.2). Its own changes reach conflict at conflict's next call: a join takes effect in conflict's next T6 (the year after the declaration: a war has no battle in its declaration year, P3-06 7.3), alliances and trades formed in T2 are read by conflict's strain and odds from its next T2.

**Quiet steps.** Renewals, lapses, breaks, spurnings, spoils, trade, mine and arms events are charged to their first civilization participant and need its budget (W9 by construction); none is world-level. A mine closing writes no event and needs none.

### 6.11 Spoils (T6 and Q4, in the ends of joined wars, 6.9)

Paul, 2026-11-24: "the ally should receive a share and if the other ally doesn't want to give it, it might sour the relationship."

**What the spoils are.** P3 has no wealth; what a won war takes from the loser is counted in people: the tribute's people carried off (P3-06 20.3) and the towns sacked. The spoils of war w for its winning principal W are

```
V(w) = tribute_people(w) + plunder(w) × spoils_plunder_permille div 1000
```

with `tribute_people` the value `people` of the `tribute` the war's ending caused (0 when none) and `plunder` the sum, over the settlements W sacked in w, of their population at the fall before its toll (the value 20.15 adds to taken(W) for a sack; captures are not counted). Both come from `ConflictSubsystem::war_spoils(w)` (7.15). **Land is never shared**: a ceded region and a captured town stay the principal's, so allies never redraw the map, and the bloc bound (6.7) and the territory view keep their meaning (reading, 24 L1). Nothing moves either: a granted share is a record and a warming, as a tribute moves no people to its payee (P3-06 20.3).

**Who is owed.** For a joined record (w, z, p, e) whose war is no longer open: the war's **winner** W is `war_spoils(w).winner`: the winner of its `peace` or `war_won`, or, when it ended by a principal's end (7.11), the surviving principal; a truce has none. So an end by the partner's own end leaves the partner's allies owed nothing. z is **owed** a share when W = p, z's ally record was closed by the war's end (not by `ally_withdrew`, not by z's own end), z and W are alive, dead(z) = `ally_dead(w, z)` > 0 (z lost people in w's battles: it fought), and V(w) > 0. Otherwise the record is settled with no event.

**The share.** Let dead_S = `side_dead` of `war_spoils(w)`, the dead of W's side in w (W's own and every ally's of that side, withdrawn ones included; 7.8). For each owed ally z, in join order of w:

```
part(z)  = clamp(dead(z) × 1000 div max(1, dead_S), spoils_floor_permille, spoils_cap_permille)
share(z) = min(V × part(z) div 1000, V − Σ share(z') over the owed allies z' of w before z)
```

A share of 0 settles the record with no event. So one ally is owed between a tenth and four tenths of V, in proportion to its dead, and the shares never exceed V.

**Granted or withheld.** With w(W, z) the warmth of 6.2 (live: 0 below `ally_min_disposition`) and the principal's need

```
ex(W) = min(1000, own_dead(W) × 1000 div max(1, exhaustion_permille × max(1, pop0(W) × muster_permille(r_W) div 1000) div 1000))
g     = clamp(spoils_grant_permille + spoils_warmth_permille × w(W, z) div 1000
              + spoils_part_permille × part(z) div 1000 − spoils_need_permille × ex(W) div 1000,
              spoils_grant_min, spoils_grant_max)
```

(own_dead and pop0 = `winner_dead` and `winner_pop0`, the principal's own in w, as 7.7 item 1's exhaustion reads them), the share is **granted** if `chance_pm(D(hist.alliances.spoils; w, z, 0), g)`, else **withheld**. A warm pair, an ally that bled much and a principal that bled little make a grant likely; a cold pair and a bled-out principal make keeping likely. Shipped, a pair at warmth 600 with an ally at part 200 and a principal half-way to exhaustion: g = 450 + 180 + 100 − 200 = 530.

- **Granted**: append **`spoils_shared`** (region: w's goal region; settlement 0; participants W `principal`, z `ally`, the loser `enemy`; value `share`; causes the war's ending event and z's `ally_joined_war`, ascending); `credit(r_W, r_z, share_memory, event)`.
- **Withheld**: append **`spoils_withheld`** (the same shape); `credit(r_W, r_z, −withheld_memory, event)`; `ConflictSubsystem::add_grudge(W, z, withheld_grudge)` (7.15: the grievance between the two realms, which `credit` cannot carry when they are of one race); if W and z have an open alliance: its term's withheld count += 1 and its withheld record = (this event, w's goal region, z). 6.7 decides what the alliance does with it.

The record is settled. In Q4 either event needs `quiet_events_left(W)` > 0 (the principal is its first civilization participant), else the record waits unsettled for the next T6 or Q4. Each record draws at most once (the key is the war and the ally).

### 6.12 Mines (part C; closings in T2, T6 and Q4; openings in T2 and Q4)

Sites are kept by `MonstersSubsystem` (P5-02 5), which writes table `site`; mines are sites of the same table and the same id sequence. This stage adds them through a seam (7.16) and every P5-02 rule that reads sites reads kinds 1 and 2 only (`holds_site` of P5-03 11.2 included: a mine does not hold back a ruin's fading; it closes before, below).

**Closings** (first, every step; open mines ascending by site id): a mine of c at settlement t in region R ends when

| ending (P5-02 11.3) | when | ending event |
| --- | --- | --- |
| 5 the founder's realm fell | c has ended | c's `ending_event` |
| 4 the founder lost the land | t is not standing, or t's owner is not c, or R's owner is not c | t's `ruin_event` when t is not standing; else the event that last changed t's owner (a `settlement_captured`, `region_ceded` or `settlements_ceded`) when it is not c; else R's `owner_event` |

`end_site(site, Y, that event, ending)` (7.16); the mine is dropped from the open mines; no event of its own (as an outpost lost, P5-02 6.6.3). The ending event is of year Y: every step that takes a town or a region runs before this subsystem in the same year (6.10), and closings run in T2, T6 and Q4.

**Openings** (T2 and Q4, after the closings): for each civilization c ascending, alive, visible and free, with `mining_permille`(r_c) > 0; for each region R ascending with owner c, `ore_reach_cells`(R) ≥ `mine_ore_min`, and no open mine of c in R: t = c's standing settlement in R with the largest population (then the smaller id) that never had a mine; none, or its population below `mine_settlement_min`: skip.

```
p = mine_permille × mining_permille(r_c) div 1000          T2
p = min(1000, p × L)                                       Q4
```

If `chance_pm(D(hist.alliances.mine; c, R, 0), p)`: id = `next_site_id()`; append **`mine_opened`** (region R, settlement t; participants c `civilization`, id `site`; cause the larger of R's `owner_event` and t's `founding_event`); `open_site` (kind 3; name `mine_forms[(id − 1) mod n]` with `{settlement}` t's name, a leading `The ` lowered to `the ` (P1-06 r3 14.7); region R; cell and settlement of t; `founder_civ` c; `monster` 0; origin Y and the event); record the mine and t. One mine per settlement ever, so mine names are unique. In Q4 the event needs `quiet_events_left(c)` > 0.

### 6.13 Forges and arms (part C; T2 and Q4, after the mines)

A **seller** is a civilization with at least one open mine; its **forge** is its smallest-id open mine. For each open trade (x, y) ascending with exactly one seller s of the two (the other, b, the **buyer**; two sellers sell each other nothing): if (s, b) is not in the arms-sold set, append **`arms_sold`** (region 0, settlement 0; participants s `seller`, b `buyer`, the forge `site`; causes the trade's `trade_opened` and the forge's `mine_opened`, ascending); add (s, b); `credit(r_s, r_b, trade_memory, event)`. In Q4 it needs `quiet_events_left(s)` > 0, else it waits. The pair leaves the set when its trade closes (a new trade sells anew).

The strength (7.5): forged(c) = 1 when c is a seller; arms(c) = 1 when c is not a seller and some open trade of c has a seller as its other side (one buyer gains once, however many sellers). Read live by conflict through `AllianceView` at its next call.

Trade openings (6.8) of a pair with exactly one seller (at the step's start) use p × `arms_trade_permille` div 1000 (then min 1000): realms seek the dwarves' arms.

### 6.14 What P5-07 deepens (the seam)

P5-07 Mountain depths (P5-00 3.1) works the same mines: a mine is a `site` row of kind 3 with its settlement, founder and `mine_opened`, and `mining_permille` is the race's will to dig. P5-07 adds delving events that name a mine as `site` (causes chained from its `mine_opened`), may wake a deep horror whose lair is the mine (P5-02's lair rules with the mine's cell), and may end a mine with a new ending code appended to P5-02 11.3 (for example 7 delved too deep), with the reader check of 9.2 widened then. Nothing here is renamed or moved for it: no second mine kind, no change to 6.12's closings, and arms keep reading "an open mine".

## 7. Seams into conflict (amend P3-06)

### 7.1 `AllianceView`

Declared in `conflict.hpp`; `ConflictSubsystem` gains `void set_alliances(const AllianceView*)`. Const reads, valid from construction:

```cpp
class AllianceView {
public:
    virtual bool allied(std::int64_t a, std::int64_t b) const = 0;                    // an open alliance
    virtual std::span<const std::int64_t> allies(std::int64_t c) const = 0;           // ascending
    virtual std::int64_t trade_partners(std::int64_t c) const = 0;                    // open trades of c
    virtual bool forged(std::int64_t c) const = 0;                                    // c has an open mine (6.13)
    virtual bool arms(std::int64_t c) const = 0;                                      // c buys arms (6.13)
};
```

With no view set (or a view with no alliance, no trade and no mine), conflict is as at 0.25.0.

### 7.2 Strain between allies (amends 5.2)

For an allied pair (a, b): rate(a, b) = rate as 5.2 × `allied_strain_permille` div 1000 (applied after 5.2's left-to-right evaluation, before the step's multiplication). Claims, declarations and the tension read the strain as before; a declaration between allies is a betrayal (6.6).

### 7.3 Fear of blocs (amends 5.1)

fear(b) = `fear_weight` × max(0, bs(b) − `fear_share`) div (1000 − `fear_share`), with bs(b) = min(1000, share(b) + Σ_{z ∈ allies(b)} share(z)) in place of share(b). The coalition by fear (10.1 item 3) now rises against every member of a great bloc.

### 7.4 Deterrence (amends 7.2 item 3 and 20.6 item 3)

In the odds, fb = (force(b) + `ally_deterrence_permille` × Σ force(z) div 1000) × def(G) div 1000, the sum over z ∈ allies(b) with z ≠ a, z not allied with a and `reaches(z, a)`. The tension's odds skip (20.6 item 3) reads the same fb. A realm with allies is attacked less; with `ally_deterrence_permille` 0, 0.25.0's odds.

### 7.5 Trade, forge and arms strength (amends 6.1, 20.6 item 1 and 2)

force_of(c) (whole and local) gains three last factors, in this order, each with floor division, before the 2^40 bound check:

```
f = f × (1000 + trade_strength_permille × min(trade_partners(c), trade_strength_max)) div 1000
f = f × (1000 + forge_strength_permille × forged(c)) div 1000          (part C)
f = f × (1000 + arms_strength_permille × arms(c)) div 1000             (part C)
```

They apply wherever force is read: declarations, odds, battles, the joint strength of 6.5. Exhaustion still reads population. forged and arms are exclusive (6.13), so a realm gains at most 15% from part C at the shipped values.

### 7.6 `join_war` and the joined ally (new)

`ConflictWar` gains `allies`: a list in join order of {civilization z, side (0 the defender's, 1 the attacker's), join event, dead, pop0, open}; and `end_event` (the war's ending event, 0 while open). `void ConflictSubsystem::join_war(StepContext&, std::int64_t war, std::int64_t ally, std::int64_t event)` with war the declaration event id:

1. Fatal (`history: conflict: join_war: <reason>`) unless the war is open, `ally` is not a side or an ally of it, and the pair (ally, enemy) has no open `war` relation (the caller's conditions, 6.5).
2. Append the ally record (pop0 = `PeoplesView::population(ally)`, dead 0, open).
3. Close the open `peace` relation of (ally, enemy) with `event`, if any; open relation `war` (ally, enemy) with `event` (asymmetric: the ally as a); strain of both directions 0; drop the claims of both directions.

A joined row is a `war` relation like any other for every rule that tests "at war" (strain, claims, declarations, P5-03's hosts, P5-03a's strife and foes). `open_wars_of(c)` counts the wars with c as a side **and** the open joined records of c (so an ally's force is split over its wars). `attacking(c)` (one war as attacker, 7.2) counts principal wars only.

### 7.7 Allied battles (amends 7.3 items 3 to 6, 20.6 item 2, 20.16, 24.5)

In a battle of war w at R between x and y, each side S is its principal and its open allies of that side. For each member m of S: F_m = `local_force(m, R)` for the principal; F_m = `local_force(m, R)` × `ally_effort_permille` div 1000 for an ally (local force as amended in 7.5 and 7.6, divided by m's open wars). The side's force F_S = Σ F_m. In 7.3 item 3, A = F_{side of x}, D = F_{side of y}; everything after (the battle chance, def, the win draw, 20.16's bound, 24.5's toll spread) is unchanged on these totals.

**Casualties**: the side's dead d_S (dead_W or dead_L) are split: each ally m with F_m > 0 takes d_S × F_m div F_S, in ascending id; the principal takes the rest. `casualties(m, share, ev)` for each in that order (winners first, as now); each ally's `dead` in the war += its share.

**The event**: when some ally of either side has F_m > 0, the battle is **`allied_battle`** (region R; participants W `winner`, L `loser`, each ally of W's side with F_m > 0 ascending as `winner_ally`, each of L's as `loser_ally`; values `winner_dead`, `loser_dead` = the side totals, `pass` = the border's `pass_id` when def(R) included `pass_defense`, else 0; cause w); otherwise `battle` or `battle_at_pass` as before. The war's tallies (won, initiative, last battle winner, fought regions) are the principals', as before.

**Sieges and falls** (7.3 item 7, 7.5): after a victory of x's side, A2 = max(0, A − dead of x's side). The besieged and the taker are the principals; an ally never takes, sacks or is besieged in w. The siege's cause is the `allied_battle`.

### 7.8 What the score counts (amends 20.15 item 1)

dead(c) in s is the dead of c's side: the principal's and every ally's dead in this war (open or withdrawn). taken(c) is the principal's (allies take nothing). Exhaustion (7.7 item 1, 20.6 item 1) stays the principal's own dead against its own pop0.

### 7.9 Withdrawal (new, before 7.7 item 1)

For each open ally m of the war in join order: if dead(m) × 1000 ≥ `exhaustion_permille` × max(1, pop0(m) × `muster_permille`(r_m) div 1000): append **`ally_withdrew`** (region 0, settlement 0; participants m `ally`, the partner `partner`, the enemy `enemy`; cause the war's last event); the ally record is closed; the `war` relation (m, enemy) is closed with it; `peace` (m, enemy) opened with it; `last_peace`{m, enemy} = (Y, it); grudge{m, enemy} += `war_grudge`; strains and claims of the pair reset. In Q4 (7.8's quiet ends) no withdrawal is tested.

### 7.10 Ends (amends 7.7, 7.8, 20.3, 22.5)

When war w ends with event E (peace, war_won or truce), after its costs (and after recording the tribute's people for 7.15): for each open ally m in join order: close `war` (m, enemy) with E; open `peace` (m, enemy) with E; `last_peace`{m, enemy} = (Y, E); grudge{m, enemy} += `war_grudge`; strains and claims of the pair reset; the record closed. `end_event` = E. **Costs** (cession, foothold, tribute) are the principals' only: allies pay nothing and receive no land; their share of the spoils is decided afterwards by this stage (6.11), outside conflict. In a quiet step an ending that waits (7.8, 20.3) waits with its allies.

### 7.11 Ends by a side's end (amends 7.6, 8.1 item 1)

When a principal ends (conquered or ended), the war is over as now; each open ally's `war` relation is closed by the same pass that closes the side's relations, with the side's ending event when the ended side is the ally's enemy, and otherwise (the partner ended) with the partner's ending event; no `peace` row opens. `end_event` = that event. When an ally ends, its record closes with its ending event (its relations are closed by housekeeping).

### 7.12 Housekeeping

Unchanged in rule: 8.1 item 1 already closes every open relation of an ended side, `alliance` and `trade` included. Conquest (7.6) closes them too.

### 7.13 Other reads

`ConflictSubsystem` makes public, as const reads with the live state of the step: `disposition_of(ctx, a, b)`, `at_war(a, b)` (now: any open `war` relation either way), `open_wars_of(c)`, `force(ctx, c)`, `truce_over(ctx, a, b)`, `lying_low(ctx, c)`, `share(ctx, c)`, `last_peace(a, b)` (year and event, or none), `reaches(ctx, x, y)` (true when 5.1's `front(x, y)` with x's march hops, 8.1's `enmity_hops` where an enmity row exists, is not empty), `war_open(w)`, `war_end_event(w)`, `ally_left(w, z)`, `war_spoils(w)` (7.15). `DispositionView` gains `std::int64_t turn_event(std::int64_t race_a, std::int64_t race_b) const` (the pair's last `disposition_turned`, 0 none) and its band.

### 7.14 Proof obligation I0

With the `zero` tuning (4.2) and the `history.conflict` fields at any value, the history tables of a world are byte-identical to those of the same world without `AlliancesSubsystem` and with no `AllianceView` set: no pair is ever in reach, no race has an origin ally and `mine_permille` is 0, so no alliance, trade or mine forms and no spoils are owed; allied strain, deterrence, fear of blocs, trade, forge and arms strength and allied battles then reduce to 0.25.0's rules (empty ally lists, zero partners, no seller). What differs is the catalogue rows of kinds 63 to 78 and the schema version (11). The `site` rows are P5-02's alone.

### 7.15 Spoils reads and the grudge seam (new; amends P3-06 4.2, 7.5, 20.3)

`ConflictWar` gains `plunder_a`, `plunder_b` (0 at the declaration): when principal x of the war sacks a settlement in it (7.5), plunder_x += the settlement's population at the fall before its toll (the value 20.15 adds to taken(x)); and `tribute_people`, `tribute_event` (0 until 20.3's cost appends a `tribute` for this war's ending: its `people` and id). Allies never sack in w (7.7), so plunder is the principals'.

```cpp
struct WarSpoils {
    std::int64_t winner, loser;        // peace, war_won: as the event; a principal's end (7.11): the survivor, the ended; truce, both ended: 0, 0
    std::int64_t end_event, goal_region;
    std::int64_t tribute_people, tribute_event, plunder;   // the winner's plunder
    std::int64_t winner_dead, winner_pop0;                 // the principal's own, as exhaustion reads them
    std::int64_t side_dead;                                // the winner's side (7.8)
};
WarSpoils war_spoils(std::int64_t war) const;                       // fatal while the war is open
std::int64_t ally_dead(std::int64_t war, std::int64_t ally) const;  // the ally's dead in the war
void add_grudge(std::int64_t a, std::int64_t b, std::int64_t units);
```

`add_grudge` (a ≠ b, units ≥ 0, fatal otherwise): grudge{min(a, b), max(a, b)} += units. It is a new grudge source of P3-06 4.2's table ("an ally's share of the spoils withheld", 6.11) and decays as every grudge. It is called only from 6.11, at one fixed point of this subsystem's T6 and Q4; conflict reads it at its next call. Conflict keeps each ended war's record (allies, dead, plunder, tribute, end) at least until every joined record of it is settled and credited by this subsystem (keeping every ended war is simplest; the reads are the contract, the storage the engineer's).

### 7.16 The site seam (new; amends P5-02 5, 6.6, 11)

`MonstersSubsystem` makes public, for the alliances subsystem (set by `stages/history.cpp` as `alliances.set_sites(&monsters)`):

```cpp
std::int64_t next_site_id() const;                                  // the id the next site will take
void open_site(const SiteSpec&);                                    // kind 3 only from this stage; fatal unless spec.id == next_site_id()
void end_site(std::int64_t site, std::int64_t year, std::int64_t event, std::int64_t ending);  // as P5-02's
```

with `SiteSpec` {id, kind, name, region, x, y, settlement, founder_civ, monster, origin_year, origin_event}. Monsters runs before alliances in every step (6.1), so ids are allocated in one order. Every loop of P5-02 over sites (outposts' ends, lairs, `holds_site`, the report's lairs and outposts counts) tests the kind and skips kind 3. `site_rows()` writes every site, mines included, in id order.

## 8. Events

### 8.1 Catalogue (appended after 62; ids provisional, the next free ones at merge, P5-00 4.4)

| id | key | caused | region | settlement | roles (seq: role, entity type, min–max) | values |
| --- | --- | --- | --- | --- | --- | --- |
| 63 | `alliance_formed` | 1 | 0 | 0 | 1 `ally_a` civ 1–1; 2 `ally_b` civ 1–1; 3 `foe` civ 0–1 | `basis` |
| 64 | `alliance_renewed` | 1 | 0 | 0 | 1 `ally_a`; 2 `ally_b`; 3 `foe` 0–1 | `basis` |
| 65 | `alliance_lapsed` | 1 | 0 | 0 | 1 `ally_a`; 2 `ally_b` | `reason` |
| 66 | `alliance_broken` | 1 | 0 | 0 | 1 `ally_a`; 2 `ally_b` | `reason` |
| 67 | `alliance_betrayed` | 1 | 0 | 0 | 1 `betrayer` civ 1–1; 2 `betrayed` civ 1–1 | — |
| 68 | `ally_joined_war` | 1 | 0 | 0 | 1 `ally` civ 1–1; 2 `partner` civ 1–1; 3 `enemy` civ 1–1 | `side` |
| 69 | `ally_refused` | 1 | 0 | 0 | 1 `ally`; 2 `partner`; 3 `enemy` | — |
| 70 | `ally_withdrew` | 1 | 0 | 0 | 1 `ally`; 2 `partner`; 3 `enemy` | — |
| 71 | `allied_battle` | 1 | 1 | 0 | 1 `winner` civ 1–1; 2 `loser` civ 1–1; 3 `winner_ally` civ 0–16; 4 `loser_ally` civ 0–16 | `winner_dead`, `loser_dead`, `pass` |
| 72 | `trade_opened` | 1 | 0 | 0 | 1 `partner_a` civ 1–1; 2 `partner_b` civ 1–1 | — |
| 73 | `trade_broken` | 1 | 0 | 0 | 1 `partner_a`; 2 `partner_b` | `reason` |
| 74 | `spoils_shared` | 1 | 1 | 0 | 1 `principal` civ 1–1; 2 `ally` civ 1–1; 3 `enemy` civ 1–1 | `share` |
| 75 | `spoils_withheld` | 1 | 1 | 0 | 1 `principal`; 2 `ally`; 3 `enemy` | `share` |
| 76 | `alliance_spurned` | 1 | 1 | 0 | 1 `ally` civ 1–1; 2 `principal` civ 1–1 | `reason` |
| 77 | `mine_opened` | 1 | 1 | 1 | 1 `civilization` civ 1–1; 2 `site` site 1–1 | — |
| 78 | `arms_sold` | 1 | 0 | 0 | 1 `seller` civ 1–1; 2 `buyer` civ 1–1; 3 `site` site 1–1 | — |

(civ = entity type 3, site = entity type 9; unlisted counts are 1–1.) In 63 to 66, 72 and 73 `ally_a` < `ally_b` (`partner_a` < `partner_b`) by id. Codes: `basis` 0 origin, 1 common enemy, 2 fear, 3 friendship, 4 trade; `reason` of 65: 0 no cause left, 1 the bloc too great; of 66: 0 deserted (refusals), 1 cooled; of 73: 0 war, 1 cooled, 2 out of reach; `side` 0 defence, 1 attack; `reason` of 76: 0 at its renewal (`withheld_to_break` shares kept in its term), 1 cooled after spoils were kept; `share` ≥ 1, in people as the tribute's `people` (6.11). In 74 and 75 the region is the war's goal region; in 76 the region of the term's last `spoils_withheld`; in 77 the mine's. The charged civilization of a quiet-step event is its first civilization participant. Kinds 63 to 66, 68 to 70, 72, 73 and 78 have no region (they are relations between realms, as `peace`); kind 71 has the battlefield; 74 to 76 name the region the spoils were won over, so a sentence can say whose spoils.

### 8.2 Renderer forms (amends P3-04 8.4, 8.5)

For any role r: **`{r:with}`** renders `, with ` + `{r:all}` + `,` when r has participants, else nothing; **`{r:against}`** renders ` against ` + `{r:all}` when r has participants, else nothing. `template_problem`: the role must exist (as `{r:all}`). No other change; not a schema change (as P5-03a's forms).

### 8.3 Templates and salience (data, `history.report`)

| key | sal. | template | summary? |
| --- | --- | --- | --- |
| `alliance_formed` | 50 | `{@causes:trade_opened=long years of trade,war_declared=the outbreak of war,disposition_turned=the friendship of their peoples,civ_conquered=the fall of a realm,settlement_captured=the fall of a neighbour,sacked=the sack of a neighbour,region_ceded=the loss of a neighbour's land,settlements_ceded=the loss of a neighbour's land|After |, }{ally_a} and {ally_b} {basis?swore the alliance their peoples had kept since the gods made them|swore to stand together|made common cause|swore friendship and alliance}{foe:against}.` | yes |
| `alliance_renewed` | 10 | `{ally_a} and {ally_b} renewed their alliance{foe:against}.` | no |
| `alliance_lapsed` | 25 | `The alliance of {ally_a} and {ally_b} lapsed{reason?, for together they had grown too great|, its cause long past}.` | at low thresholds |
| `alliance_broken` | 40 | `{@causes:ally_refused=a call to arms refused|After |, }the alliance of {ally_a} and {ally_b} was broken{reason?, their friendship having cooled|}.` | yes |
| `alliance_betrayed` | 65 | `{betrayer} broke faith and made war on {betrayed}, an ally.` | yes |
| `ally_joined_war` | 40 | `{ally} {side?joined the war of|came to the defence of} {partner} against {enemy}.` | companion of its war (10.3) |
| `ally_refused` | 30 | `{ally} would not take up arms for {partner} against {enemy}.` | companion of its war |
| `ally_withdrew` | 25 | `Bled white, {ally} left the war of {partner} against {enemy}.` | at low thresholds |
| `allied_battle` | 35 | `{winner}{winner_ally:with} defeated {loser}{loser_ally:with} {pass?in|at the pass into|at the pass into} {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}.` | as a battle (11.1) |
| `trade_opened` | 20 | `{@causes:peace=the peace,war_won=the war,truce=the truce,peoples_met=the first meeting of their peoples|After |, }{partner_a} and {partner_b} opened trade.` | at low thresholds |
| `trade_broken` | 15 | `{@causes:war_declared=war came,alliance_betrayed=war came|When |, }the trade between {partner_a} and {partner_b} {reason?ended|dwindled as their friendship cooled|was lost with the roads between them}.` | no |
| `spoils_shared` | 20 | `{principal} gave {ally} its share of the spoils of {@region}.` | companion of its war (10.3); at low thresholds |
| `spoils_withheld` | 35 | `{principal} kept the spoils of {@region} from {ally}, its ally.` | companion of its war; yes |
| `alliance_spurned` | 50 | `After the spoils of {@region} were kept from it, {ally} broke its alliance with {principal}.` | yes |
| `mine_opened` | 20 | `{civilization} opened {site} in {@region}.` | at low thresholds |
| `arms_sold` | 30 | `{seller} sold arms forged in {site} to {buyer}.` | yes |

`basis` uses the four-word choice (P5-03 8.2): w3 renders for 3 and 4. Samples (illustrative): `The House of Aelin and the Court of Onsae swore the alliance their peoples had kept since the gods made them.` · `After the outbreak of war, the Hold of Durn and the Kingdom of Cear swore to stand together against the Gub Horde.` · `After long years of trade, the Kingdom of Cear and the Hold of Durn swore friendship and alliance.` · `The Kingdom of Cear came to the defence of the Hold of Durn against the Gub Horde.` · `The Hold of Durn, with the Kingdom of Cear, defeated the Gub Horde in the Grey Fells; 812 of the defeated and 140 of the victors fell.` · `After a call to arms refused, the alliance of the Gub Horde and the Skreegg Horde was broken.` · `When war came, the trade between the Kingdom of Cear and the Hold of Durn ended.` · `The Hold of Durn gave the Kingdom of Cear its share of the spoils of the Grey Fells.` · `The Hold of Durn kept the spoils of the Grey Fells from the Kingdom of Cear, its ally.` · `After the spoils of the Grey Fells were kept from it, the Kingdom of Cear broke its alliance with the Hold of Durn.` · `The Hold of Durn opened the Mines of Kazad Rune in the Iron Fells.` · `The Hold of Durn sold arms forged in the Mines of Kazad Rune to the Kingdom of Cear.`

Causes (W6): a formation, its reasons' events (or the younger's founding for origin); a renewal, its reasons and the last renewal; a lapse or a break by cooling, the last renewal or formation; a break by desertion, the refusal; a betrayal, the declaration and the alliance's last renewal; a join or a refusal, the declaration and the alliance's last renewal; a withdrawal, the war's last event; an allied battle, the declaration; a trade, the latest of the founding, peace and meeting; a trade broken, the declaration or the opening; a share granted or withheld, the war's ending event and the ally's join; a spurning, the last withheld share and the alliance's last renewal or formation; a mine, the later of its region's gain and its town's founding; an arms sale, the trade's opening and the forge's opening.

## 9. Relation kinds, the site kind `mine`, schema 11 and reader checks

### 9.1 Relation kinds (no schema change by themselves)

`relation` gains rows of two kinds (text keys, P3-06 4.3; `docs/specs/schema.md` 6 lists them):

| Kind | Symmetric | Opened by | Closed by |
| --- | --- | --- | --- |
| `alliance` | yes (a < b) | `alliance_formed` | `alliance_lapsed`, `alliance_broken`, `alliance_spurned`, `alliance_betrayed`, or a side's ending event |
| `trade` | yes (a < b) | `trade_opened` | `trade_broken`, or a side's ending event |
| `war` (joined) | no: a the ally, b the enemy | `ally_joined_war` | `ally_withdrew`, the war's `peace`/`war_won`/`truce`, or a side's ending event |
| `peace` (after a joined war) | yes | `ally_withdrew` or the war's ending event | as P3-06 4.3 |

At most one open row per key; a renewal writes no row. **Reader checks** (`HistoryView::load`, after P3-04's relation checks; `corrupt table 'relation': row <key>: <reason>`, first failing row in key order): `alliance since_event <e> is not an alliance_formed of the pair`; `trade since_event <e> is not a trade_opened of the pair`; `war since_event <e> is not a war_declared or an ally_joined_war of the pair` (an `ally_joined_war` names a as `ally` and b as `enemy`); for a closed `alliance` row, `until_event <e> does not end the alliance` (one of the four kinds naming the pair, or a side's `ending_event`). Event checks (P5-03 16.4's form): `basis` 0 to 4, `reason` 0 to 1 (65, 66, 76) and 0 to 2 (73), `side` 0 to 1, `pass` 0 or a `pass` row, `share` ≥ 1; every `allied_battle` has at least one ally participant (`allied_battle without an ally`); every `spoils_shared` and `spoils_withheld` cites a `peace`, `war_won` or `civ_conquered` and an `ally_joined_war` naming its `ally` (`spoils without its war`).

### 9.2 Schema 11: the site kind `mine` (part C; hard rule 7)

`site.kind` gains code **3 `mine`** (a code list change, P3-04 8.2; P5-02 11.2's list: 1 lair, 2 outpost, 3 mine). No DDL statement changes; no table or column is added.

- `SCHEMA_VERSION` (`worldgen/include/worldgen/db/schema.hpp`), `PRAGMA user_version` and `meta.schema_version` **11**; readers refuse version 10 worlds (as every bump, P0-02).
- The empty world keeps its 45 pages and 184320 bytes; its bytes change (the version), so `p0-worlds.txt` is re-recorded (17).
- A mine row: `kind` 3, `settlement` the town it was opened at (≠ 0), `founder_civ` the realm (≠ 0), `monster` 0, `origin_event` its `mine_opened`, `ending` 0 open, 4 the founder lost the land, 5 the founder's realm fell (6.12).
- **Reader check** (amends P5-02 11.4's `site` line): `kind <k> is not 1, 2 or 3`; for kind 3: `unknown civilization <c>`, monster ≠ 0 (`mine with a monster`), settlement 0 (`mine without a settlement`), `origin_event` of kind `mine_opened` (the list of kinds gains it), `ending <k> is not 0, 4 or 5 for a mine`.
- `docs/specs/schema.md` (the engineer): section 1 (the version line: "Current schema version: 11 (from generator 0.26.0, the P5-04 merge: site kind 3 `mine`)"), the `site` paragraph (kind 3 and its endings), section 5 (changelog row: "10 → 11 with generator 0.26.0: `site.kind` gains code 3 `mine`; no DDL change; the empty world stays 45 pages; readers refuse version 10 worlds; the event catalogue gains kinds 63 to 78; relation kinds `alliance` and `trade`, joined `war` rows. Proposals 'Schema 11: mine sites (P5-04 part C)'"), section 6 (kinds 63 to 78; the relation kinds list gains `alliance`, `trade` and the joined `war` rows).
- Proposals entry "Schema 11: mine sites (P5-04 part C)" (decided, 25).

## 10. Report (amends P3-07; new section 30)

### 10.1 Version

The header becomes **`# worldgen history v7`** at both levels, and `report --help` prints it (the lead's ruling, 2026-11-24: a new block and new lines are a change of shape readers should see, although P3-07 25.1's growth rule would allow v6; the growth rule holds for v7). This stage appends a block after `[peoples]` and before `[stats]`, appends line types at the end of each `[civilizations]` block, widens `[sites]`'s count line (10.6), appends `[stats]` keys, and adds companions; no existing line changes meaning otherwise (10.4). Reading: the block is named for what it lists (alliances), as `[peoples]` was (proposals entry).

### 10.2 `[alliances]` (new block after `[peoples]`)

```
[alliances]
alliances: <n> formed, <m> standing at E; ended: <l> lapsed, <b> broken, <s> spurned, <t> betrayed, <x> with a realm
  c<a> <name> and c<b> <name>: <from>-<to|E>, <basis word>[ against c<z>], e<id>[; renewed <k>][; joined <j>, refused <r>][; spoils <g> granted, <k> withheld][; <lapsed|broken|spurned|betrayed|ended> e<id>]
calls to arms: <j> joined (<d> in defence, <a> in attack), <r> refused, <w> withdrew; wars with an ally: <k> of <n>
spoils: <o> owed, <g> granted, <k> withheld; <p> people owed, <q> granted
largest bloc: <bs>‰, c<id> with c<id>[, c<id>…] in <year>
trade: <n> opened, <m> standing at E; broken <k> (war <a>, cooled <b>, out of reach <c>); arms sold <s>
  c<a> <name> and c<b> <name>: <from>-<to|E>, e<id>[; arms from c<s>][; broken e<id>]
```

Basis words: `by origin`, `against a common enemy`, `in fear`, `in friendship`, `after trade`. `joined`/`refused` count kinds 68 and 69 of the pair (either as ally). `wars with an ally` counts `war_declared` events with a kind 68 citing them, of all `war_declared`. `spoils`: `owed` counts kinds 74 and 75, `granted` 74, `withheld` 75, the people their `share` values; `spoils: none` when none was owed. An alliance line's `spoils` part counts the pair's kinds 74 and 75 (either as principal) and is left out when both are 0. `arms from c<s>` names the seller of the pair's `arms_sold`. **Largest bloc**: the largest bs(c) (6.2 by the report's islet rule, as `share_max_permille`) over every year and civilization, the earliest year and smallest id at a tie, with c's allies at that year; `largest bloc: none` when no alliance ever stood. Alliance lines by (from, a, b); trade lines likewise. Plurals as P5-03a 9.4 (`1 alliance`…). **Full**: every line. **Summary**: the five count lines, the alliance lines standing at E and those ended by betrayal or spurned, at most `SUMMARY_ALLIANCES_MAX` (**10**, `digest.hpp`) by (from, a, b) then `  and <x> more`; no trade lines. When the world has more than `SUMMARY_LINES_CIVS` civilizations, the count lines only.

### 10.3 `[civilizations]`, companions, chronicle

- Appended at the end of each block: `  allies: <n>` (alliances the realm was ever in); full: followed by `    c<id> <name>: <from>-<to|E>, <basis word>` per alliance; summary: the count only, and `; at the end: c<id>, c<id>` when the realm has allies at E.
- **Companions** (amends P3-07 24.2): `ally_joined_war` and `ally_refused` are companions of the `war_declared` they cite; `spoils_shared` and `spoils_withheld` are companions of the war ending they cite; `alliance_betrayed` is a companion of its declaration and the declaration of it.
- **Battles**: `allied_battle` is a battle kind everywhere P3-07 names `battle` and `battle_at_pass` (11.1): not a summary candidate by itself, a war's last battle in its frame, a unit of 21.2.

### 10.4 War lines and counts

A `war` row whose `since_event` is an `ally_joined_war` is an ally's row, not a war: it is in no war line, no adversaries count, `wars`, `wars_open`, `wars_hollow_permille` or T16's battles per war; the digest's war of an event (24.1) is unchanged, since a joined row's ending event is its war's (its `since_event` is larger than the war's declaration). So every existing line keeps its meaning.

### 10.5 `[stats]` (appended, full only; `SUMMARY_STATS` unchanged)

In this order: `alliances_formed`, `alliances_standing`, `alliances_cross_race`, `alliances_origin`, `alliances_lapsed`, `alliances_broken`, `alliances_betrayed`, `alliances_ended_with_realm`, `alliances_years_median` (median length of ended alliances, `-` when none), `allies_joined`, `allies_joined_defence`, `allies_refused`, `allies_withdrew`, `wars_with_ally`, `allied_battles`, `bloc_share_max_permille`, `bloc_over_800_years` (the longest run of consecutive years in which some bloc's share is above 800‰), `trade_opened`, `trade_standing`, `trade_broken`, `alliances_spurned`, `spoils_owed`, `spoils_granted`, `spoils_withheld`, `spoils_granted_permille` (granted × 1000 div owed, `-` when none owed), `spoils_people_owed`, `mines_opened`, `mines_open`, `arms_sold`, `arms_buyers_at_end` (realms with arms(c) = 1 at E − 1).

### 10.6 `[sites]`

**`[sites]`** (amends P5-02 12.5): the count line becomes `sites: <n>; open <o>; lairs <l>, outposts <p>, mines <m>`; a mine's line is `  si<id> <name>: mine in <region ref> at <x> <y>, since <origin_year> e<origin_event>, of <founder ref>, at <settlement ref>; <open|ended <ended_year> e<ending_event> <ending word>>` (ending words `lost`, `fell` as P5-02's). Full: every site; summary: the count line, as before.

### 10.7 Length

At summary in a default world: `[alliances]` 5 lines plus 3 to 10 alliance lines; `[civilizations]` one line per realm; the chronicle gains formations (about 10 a world at 50), betrayals, breaks and spurnings, arms sales (about 2 a world at 30), and joins and shares as companions of shown wars, within the budget (P5-02 12.9). Measured by the engineer against T18 (15).

## 11. Other readers

### 11.1 The battle kinds set

Everywhere a rule or a reader names `battle` and `battle_at_pass`, `allied_battle` is added with the same meaning (its dead are `winner_dead` + `loser_dead`, its pass the value `pass` when non-zero):

- P5-02 6.3.2 (undead: violent dead in a region) and P5-03a 6;
- P5-12's renaming rule `slaughter`'s `kinds` (data: `game/data/history/renaming.toml` gains `allied_battle`);
- the cause words `battle=…` of the `history.report` templates of kinds 13, 16, 31, 43, 45 (data: `allied_battle=` with the same word);
- P3-07's digest (`battle_kind`), T16 and the war frame;
- P4-04 markers (`EventSymbol::battle`);
- P5-03a 3.2 strife and 3.3 foes read `war` relations, which already include joined rows (intended: an ally at war with a people is at strife with it).

### 11.2 Browser (amends P4-05 4.4)

Kinds 63 to 70, 72 to 76 and 78 are in the new group **`alliances` = 7** (appended; the lead's ruling); kind 71 `allied_battle` in `war` (2); kind 77 `mine_opened` in `settlements` (1), where a town's works belong (a mine is opened at a settlement and closes with it; no existing group fits better and `realms` is for a realm's founding and fate); kind 78 `arms_sold` in `alliances`, with the trade it rides on. `ALL_GROUPS` becomes **0xFF**; `event_group_word` gives `alliances`; the viewer's group menu lists it last (the viewer engineer). All 78 kinds are covered (P4-05 10.2 item 2's test).

### 11.3 The mine marker (part C; amends P4-04 3.6, 4, 6.1)

P4-04 reserved entry type 6 `site` and glyph codes 16 to 31 for site kinds. This stage builds the first: `markers_at(Y)` gives one entry of type 6 for every site of **kind 3** existing at Y (origin_year ≤ Y and (ended_year NULL or Y < ended_year), P3-04 10's half-open rule; from `Timeline::sites_existing`), at the site's cell, glyph code **19** (16 + kind) `mine`: two crossed strokes (a pick and a hammer) in the 7 × 7 glyph cell, dark grey (60, 60, 60) with a one-pixel light outline as the settlement glyphs; drawn in P4-04 6.1's reserved position (between ruins and standing settlements), so a standing town hides its own mine's glyph only where they overlap (the mine is at the town's cell: the viewer offsets the site glyph by one glyph cell to the upper right, the library's `draw_markers` likewise). Lairs and outposts stay unbuilt (P4-04 3.6). Hover text: `<site name> (mine of <civilization name>), since <year>`. `MarkerOptions::sites` (new, default on) and the viewer's Markers group gain a `mines` row (the viewer engineer). The `--markers` overlay images and `markers.txt` hashes change; `p4-04-markers.txt` is re-recorded (17).

## 12. Timeline reads (amends P4-01; r6)

`Timeline` gains, from the `relation` rows it already holds:

```cpp
struct AllianceState { std::int64_t a, b, since, since_event; };
std::vector<AllianceState> alliances_at(std::int64_t year) const;              // kind "alliance" in force at Y, ascending (a, b)
std::vector<std::int64_t> allies_of(std::int64_t civ, std::int64_t year) const; // ascending
std::vector<std::vector<std::int64_t>> alliance_groups_at(std::int64_t year) const;
std::vector<std::pair<std::int64_t, std::int64_t>> trades_at(std::int64_t year) const;
```

In force at Y: since ≤ Y < until (P3-04 10's half-open rule). `alliance_groups_at` gives the connected components of the alliance graph among the civilizations alive at Y with at least one ally, each ascending, ordered by smallest id: what the territory view can draw as a bloc outline or a shared hue later (the runaway bound uses direct allies, 6.2; the groups are for display). O(rows in force). Dumps (P4-01 13.1): D(Y) gains `alliance <a> <b> <since>` and `trade <a> <b> <since>` per row in force, ascending. Mines need no new read: `sites_existing(Y)` and `site_at` (P5-02 13) return them with kind 3. No viewer work in this stage beyond the browser group (11.2) and the mine marker (11.3); the viewer engineer is told the reads.

## 13. Invariants

1. At every year no realm has more than `allies_max` allies or `trade_max` trades; alliances and trades are between alive civilizations only, at most one open row per pair.
2. No alliance row of a pair in the betrayed pairs opens after its betrayal; every `alliance_formed` other than by origin has d(x, y) ≥ `ally_min_disposition` at the event (replayed; good–evil realms need memory above +400 for it).
3. Every `alliance_formed` with basis 0 names realms of races in each other's `origin_allies`; others name realms in reach at the event (replayed).
4. Every `ally_joined_war` cites a `war_declared` of the same year whose sides are its `partner` and `enemy`; its ally was allied with the partner and not with the enemy at the event; the ally's joined `war` row opens with it.
5. Every `allied_battle` has at least one ally, every ally participant had an open joined row against the other side at the event; `winner_dead` ≤ the defeated's total; 20.16's bound holds on side totals.
6. No ally participant of a war is the besieger, taker or payee of a cost of that war; costs name principals only.
7. Every `alliance_betrayed` cites a `war_declared` between its two realms of the same year.
8. **Bloc bound**: no bloc share above `bloc_lapse_share` at the end of any T2 or Q4 of this subsystem; so with the shipped 700 no bloc holds more than 800‰ beyond one step (P5-00 8.3's hard bound: 1,000 years, met by construction).
9. With the `zero` tuning, I0 (7.14) holds.
10. P3's hard bounds (P3-00 12.3), "no race dies out", the shelter rule (no call against a sheltered realm), P5-03's and P5-03a's invariants.
11. **Spoils**: every `spoils_shared` or `spoils_withheld` follows, in the same year or a later one, a `peace`, `war_won` or `civ_conquered` of the war its `ally_joined_war` cites, whose winner is its `principal` and whose loser is its `enemy`; its ally had dead in that war and did not withdraw; at most one of the two per (war, ally); Σ `share` over a war ≤ V(w) (replayed from the `tribute` and `sacked` events); no region, settlement or relation row changes owner or opens because of a share.
12. **Spurned**: every `alliance_spurned` closes an open alliance of its two realms that is not of origin, and cites a `spoils_withheld` of the same pair with `ally` as its ally, after the alliance's last renewal or formation; reason 0 only at a renewal year with at least `withheld_to_break` such shares in the term.
13. **Mines**: every kind 3 `site` row has a settlement of its founder in its region at its origin year, of population ≥ `mine_settlement_min`, a region with `ore_reach_cells` ≥ `mine_ore_min`, and a founder whose race has `mining_permille` > 0; no settlement has two mine rows; at every year a civilization has at most one open mine per region; an open mine's settlement is standing and its founder's, and its region its founder's (checked at each year's end).
14. **Arms**: every `arms_sold` names a seller with an open mine (its `site`, founder the seller, open at the event) and a buyer with an open `trade` with it and no open mine; at most one per opened trade.

## 14. Determinism notes (for the auditor)

- Five streams keyed by ids (3): the formation by the pair, the join by the war's declaration event and the ally, trade by the pair, the spoils by the war and the ally, the mine by the civilization and the region; each decision is made at most once per key per step (the spoils once ever per key). Everything else is ordered by ids.
- Containers: `std::map` keyed by id pairs for alliances and trades, `std::set` for betrayed pairs, a vector of joined records in record order; conflict's `ConflictWar::allies` in join order. No unordered container, no pointer keys.
- The log cursor reads events in ascending id. Opens and closes of relations are made on key copies (6.10). The join seam changes conflict's members only through `join_war`, called at one fixed point of this subsystem's T2.
- Arithmetic int64, checked multiplication: largest products force × 1000 (force < 2^40, 7.5) < 2^50; shares ≤ 1000; disposition terms within ±10^4 × 1000.
- `credit` calls (6.9) are at fixed points with existing events (A4); `add_grudge` (7.15) at one fixed point of 6.11.
- Site ids: allocated by `MonstersSubsystem::next_site_id()` and checked by `open_site` (7.16); the monsters subsystem runs before this one in every step, so the id order is the order of 6.1 within a step.
- Mine names are filled from `mine_forms` by the site id (6.12), no draw.
- Spoils arithmetic: V ≤ population sums < 2^31; V × part ≤ 2^31 × 1000; the exhaustion denominator max(1, …) avoids a division by zero.

# Part B. Trade

Specified with part A: 6.8 (rules), 6.9 (credit), 7.5 (strength), events 72 and 73, the `trade` relation (9), the report's trade lines (10.2), `trades_at` (12). Trade is the only path of this stage by which peace makes realms stronger; it is not a path into mixed settlements (P5-03 7.5's `add_people` is not used; 20).

**Wealth is left out** (Paul, 2026-11-24). No rule of A, B or C needs a wealth number: an alliance's reasons are events and dispositions, trade's and arms' effect is strength, the spoils are counted in people as tribute is (6.11), and no cost is paid in goods. `civilization_census` (P5-00 6.1) is therefore not written (20).

# Common

## 15. Targets (validation) and calibration

### 15.1 Targets

At 1024 defaults, seeds 0–99, unless stated; P5-00 8.2's other sweeps report. Bounds are r2 estimates; the measured column is filled in at the integration (r3).

| Target | Definition | Bound |
| --- | --- | --- |
| **AL1** | `alliance_formed` per world | median 6 to 15; at least one in ≥ 95 worlds |
| **AL2** | alliances standing at E | median ≥ 3 |
| **AL3** | alliances across races | 30% to 70% of formed; within a race reported by race |
| **AL4** | elves and faeries by origin | every world with an elven and a faerie realm alive together at some year has an origin alliance (by rule) |
| **AL5** | wars with an ally (`wars_with_ally` / `wars`) | 20% to 45%; joins in defence ≥ 70% of joins |
| **AL6** | refusals | reported; `ally_refused` ≤ `ally_joined_war` |
| **AL7** | betrayals | at least one in 20% to 60% of worlds; 3% to 15% of ended alliances |
| **AL8** | how alliances end; median length | reported (lapsed, broken, spurned, betrayed, with a realm) |
| **AL9** | blocs | hard (every world of every sweep, 512 included): `bloc_over_800_years` ≤ 1000 (by rule: ≤ one step); typical: `bloc_share_max_permille` median ≤ 450, max ≤ 700 |
| **AL10** | allied battles | reported; share of battles |
| **SP1** | shares granted (`spoils_granted_permille` over the sweep's pooled owed shares) | 500 to 850 per mille; `spoils_owed` median ≥ 1 per world |
| **SP2** | alliances spurned (`alliances_spurned`) | few but present: at least one in 20% to 60% of worlds; at most 10% of ended alliances |
| **SP3** | spoils kept and remembered | reported: withheld shares followed by a spurning of the same pair, by an `alliance_broken` by cooling, or by neither; the median d(x, y) fall at a withheld share |
| **TR1** | `trade_opened` per world; standing at E | median 8 to 25; median ≥ 3 |
| **TR2** | alliances of basis trade, or citing a `trade_opened` | ≥ 15% of formed |
| **TR3** | strength of trade | reported: wars won by the side with more trade partners |
| **MC1** | mines | a mine in ≥ 90% of worlds with a dwarven realm holding a region with `ore_reach_cells` ≥ `mine_ore_min` and a settlement of `mine_settlement_min` there (P5-00 8.3's dwarves in mines); `mines_opened` median 2 to 8 in worlds with dwarves |
| **MC2** | arms | `arms_sold` in ≥ 60% of worlds with a mine; arms buyers at E reported |
| **MC3** | strength of the dwarves | reported: dwarven wars won and dwarven regions at E against 0.25.0 (a check that +5% forge strength does not make the dwarves a runaway: P3-00 12.3 and the race's region share bound hold) |
| **RR** | P5-03a's RR2a to RR5 | still within their bounds (regression); friendship at E median ≥ 3 |
| **C1** | wars per world (`war_declared`) | median 22 to 28 (25 at 0.25.0, ±15%; the lead's bound) |
| **T16** | repeated battles (allied battles included) | ≤ 2 per thousand |
| **G2** | standing settlements; population at E | medians ≥ 81 and the bound in force after P5-03a (205,000 proposed) |
| **T18** | summary lines | default median ≤ 620; every world ≤ L(C); `civs4` median ≤ 1,050 |
| **P** | participation (P5-00 8.3) | every race is a participant of an alliance or trade event in ≥ 90% of worlds where it has a realm in reach of another (reported per race) |
| **timing** | `generate` seed 0 1024 release | at most 5 s over 0.25.0 (P5-00 8.1) |

### 15.2 Calibration (data only, the engineer, in this order; values into r3)

1. AL1, AL3 by the four `form_*_permille` and `form_max_permille`.
2. C1 by `allied_strain_permille` and `ally_deterrence_permille` (both lower wars) against the fear of blocs (which raises them), then `border_friction` only if needed (lead's ruling).
3. AL5 by `defend_permille`, `attack_permille`.
4. AL7 by `allied_strain_permille` (re-check C1).
5. AL9 by `bloc_share_max`.
6. TR1 by `trade_permille`, TR2 by `trade_ally_years`.
7. MC1 by `mine_permille` and `mine_settlement_min`; MC2 by `arms_trade_permille`; MC3 and C1 by `forge_strength_permille` and `arms_strength_permille` (lower them before touching anything else if the dwarves gain land or wars move).
8. SP1 by `spoils_grant_permille` (then `spoils_need_permille`); `spoils_plunder_permille` only if `spoils_owed` is below its bound.
9. SP2 by `withheld_grudge` and `withheld_memory` (then `withheld_to_break`; re-check AL2 and AL8).
10. T18 by saliences.

G2, T16 and RR checked after each step.

## 16. Order of implementation and roles

One branch, one engineer, one test writer working from the specs separately. `GENERATOR_VERSION` 0.26.0 and `SCHEMA_VERSION` 11 in step 1; goldens once, in step 11.

| Step | What | Spec |
| --- | --- | --- |
| 1 | Version and schema 11; data: race fields, `history.alliances`, conflict fields, `history.report` entries, renaming data; both fixture roots | 4, 8.3, 9.2, 11.1 |
| 2 | Catalogue 63–78; forms `with`, `against` | 8 |
| 3 | Conflict seams: `AllianceView`, allied strain, fear of blocs, deterrence, trade, forge and arms strength, `join_war`, allied battles, score, withdrawal, ends, spoils reads, `add_grudge`; public reads | 7 |
| 4 | The site seam in monsters | 7.16 |
| 5 | `AlliancesSubsystem` (A, B, spoils, mines, arms), its streams | 5, 6 |
| 6 | Battle kinds set in monsters, digest, markers, browser groups; the mine marker in the library (`markers_at`, `draw_markers`, glyph 19) | 11 |
| 7 | Reader checks (relations, events, site kind 3); `Timeline` reads | 9, 12 |
| 8 | Report v7 | 10 |
| 9 | I0 on the fixtures | 7.14 |
| 10 | Calibration; timing | 15 |
| 11 | Goldens, `schema.md`, README and CLAUDE.md (lead) | 9.2, 17 |

- **Engineer**: steps 1 to 10; tell the viewer engineer 11.2, 11.3 and 12.
- **Viewer engineer**: the browser group menu, the Markers group's `mines` row, the mine glyph and hover in the viewer, the headless check's marker cases (P4-04's M checks gain a mine on a world with dwarves).
- **Test writer**: 18, from the specs only.
- **Determinism auditor**: 14, the seams of 7.
- **Validation**: 15 on its sweeps, the cross-preset comparison, timing.
- **World critic**: 21's reading list.

## 17. Golden values and CI

Every world file changes (`GENERATOR_VERSION`, schema 11 in `user_version` and `meta.schema_version`, `content_hash`, catalogue rows, every history from the first alliance, trade or mine). The empty world keeps its pages but its bytes change (the version).

| Golden | Re-recorded | By |
| --- | --- | --- |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, `GOLDEN_CASES`, `stages_pipeline_test.cpp`, README phrase hash | yes | validation, after the cross-preset comparison |
| `core_vectors.txt`, `p1-0*`, `p3-02-names.txt`, `p3-03-chronology.txt` | no, must not move | — |
| `p3-01-data.txt`, `p3-04-engine.txt`, `p3-06-conflict.txt`, `p3-07-report.txt`, `p4-01-timeline.txt`, `p4-04-markers.txt` (mine markers), `p4-05-browser.txt`, `p5-12-renaming.txt`, `p5-02-monsters.txt`, `p5-03-relations.txt` | yes | test writer |
| `p3-05-peoples.txt` | only if it hashes catalogue or template rows | test writer |
| **`p5-04-alliances.txt`** (new) | new | test writer, `worldgen/tests/reference/p5_04_alliances_reference.py`: per world case and fixture, hashes of the `alliance`, `trade` and joined `war` rows, of events 63–78, of the kind 3 `site` rows, and the counts of 15's AL, SP, TR and MC rows |

`world-hashes.sh` keeps sixteen cases; its `markers.txt` hashes change with the mine markers. New CTest shards `worldgen-tests-alliances` (`*alliances_*`) and `-slow-alliances`.

## 18. Tests required

### 18.1 Unit (C++)

1. Data: the race fields and `race_origin_allies`'s messages (one-sided origin included); every `history.alliances` field's bounds and absence; `alliances_dispositions`, `alliances_blocs`, `alliances_trade`, `alliances_spoils`, `alliances_mine_forms`; the conflict fields.
2. Terms: reach at `ally_hops` and + 1, −1; warmth at its two ends; bloc share with and without allies; each reason with its cause and foe (common enemy within and at `foe_years`, a joined war not counting; fear at `fear_share`; trade at `trade_ally_years`; friendship and its turn cause); the basis order.
3. Formation: each condition one by one (betrayed pair, lately at war, `allies_max` live in the loop, the bloc test before the draw); the event, relation and credit.
4. Origin: no reach needed; `allies_max`; no renewal event; no break by refusal; lapse by the bloc rule; never spurned.
5. Calls: every condition of 6.5 (allied with both, already at war, `reaches`, shelter, `ally_wars_max`, truce and hostility for attack); the two chances at their caps; join (event, `join_war`, credit), refusal and the break at `refusals_to_break`; origin refusals silent; no chain.
6. Betrayal: a declaration between allies, its events, the betrayed set, calls without the betrayer, trade broken.
7. Keeping: cooling at `ally_break_disposition` ± 1; bloc lapse sheds the smallest share first until within the bound; renewal with each reason, lapse without; quiet budget and Q4's renewal test.
8. Trade: openings at their chance in T2 and Q4, the cause; breaks by war, cooling and reach (+ 1 margin); credits in T2 and over a quiet step of several periods.
9. Conflict seams: allied strain; fear by bloc share; deterrence in odds and tension; trade, forge and arms strength in force and local force (the three factors in order, floor division each); `join_war` (relations opened and closed, strains, claims, fatal reasons); allied battle (side forces, casualties split with remainder to the principal, the kind chosen only with an ally present, `pass`); the score with allies' dead; withdrawal; ends with allies (peace rows, `last_peace`, grudge); ends by a side's end; plunder and tribute recorded for `war_spoils`; `add_grudge` and its decay; no view: 0.25.0's behaviour.
10. **Spoils**: V with tribute only, sacks only (`spoils_plunder_permille`), both, none (no event); captures and cessions not counted; owed only on the winning side, not after a truce, not to a withdrawn or ended ally, not with dead 0, after an end by the enemy's end (V from sacks), not after the partner's end; part at the floor, in between and at the cap; two and three allies with the shares never above V (join order, remainder); g at `spoils_grant_min` and `spoils_grant_max`, and each of its terms alone; granted: event, causes, credit; withheld: event, credit, `add_grudge`, the alliance's withheld record (none when the pair has no open alliance); the quiet-step wait for the principal's budget; one draw per (war, ally).
11. **Spurned**: one withheld share and a reason at the renewal: renewed, count reset; `withheld_to_break` shares in a term: spurned at the renewal (reason 0, region and causes); cooling with a withheld share in the term: spurned (reason 1), without: broken; an origin alliance with withheld shares: renewed silently, never spurned; the region named at the year of the break (a renamed region, P5-12).
12. **Mines**: each condition of 6.12 (race field 0, region not held, ore below `mine_ore_min`, town below `mine_settlement_min`, a settlement that had a mine, an open mine in the region); the chance in T2 and Q4; the site row (id from `next_site_id`, name form by id with `The` lowered, cell, settlement, founder); the event and its cause; each closing (realm fell, town ruined, town captured, region ceded, town ceded, region lost with the town still the founder's) with its ending code and event of year Y; P5-02's loops skip kind 3 (`holds_site`, outposts).
13. **Arms**: a trade with one seller: `arms_sold` once, its causes and credit; two sellers: none; a seller's first mine opening during a trade: the sale then; trade broken and reopened: sold anew; forged and arms flags; the opening boost `arms_trade_permille`.
14. Rendering: `{r:with}`, `{r:against}` with none, one and three participants, and `template_problem`; every new template on a fixture event; the cause words.
15. Reader: each message of 9.1 and 9.2 on a corrupt fixture; a version 10 world refused.
16. Report v7 on fixture **H16** (`h16-alliances.toml`, from H15: an origin alliance, a foe alliance with two joins and an allied battle, a refusal that breaks, a betrayal, a lapse, a bloc lapse, three trades, a broken trade, a share granted, two withheld and a spurning, a dwarven mine with an arms sale and a mine lost), both levels; the header; companions; `[stats]`; `[sites]` with a mine; the joined `war` rows kept out of war lines.
17. Timeline: `alliances_at`, `allies_of`, `alliance_groups_at`, `trades_at` around a formation, a betrayal, a spurning and a lapse; `sites_existing` with a mine.
18. Battle kinds set: undead dead, renaming, markers, browser groups (every kind 63–78 in its group), T16.
19. Markers: a mine entry at its cell from origin to end, glyph 19, the offset, `MarkerOptions::sites` off.

### 18.2 Property and identity

- **I0** (7.14) on the fixture worlds and seeds 0–4 at 512 (default suite) and 0–4 at 1024 (`*-slow`).
- Invariants 1 to 14 on seeds 0–9 at 512 (default) and 0–19 at 1024 and the `civs4` seeds 0–4 (`*-slow`); AL9's hard bound on every swept world, 512 included.
- Resume (P3-04 16.3).

### 18.3 Reference

`p5_04_alliances_reference.py` inside the conflict reference's replay (with P5-03's and P5-03a's, and P5-02's sites): sections 6 and 7 and a `check-world` mode for invariants 1 to 8 and 11 to 14 and 10's checks. A hand-made root **`alliances-small`** (the conflict stress data with `form_*` 1000, `defend_permille` 1000, `allied_strain_permille` 1000, `refusals_to_break` 1, `term_years` 10, `trade_permille` 1000, `spoils_grant_permille` 500 with `spoils_grant_min` 300 and `spoils_grant_max` 700, `withheld_to_break` 1, `mine_permille` 1000, `mine_settlement_min` 0, and dwarves present) makes every kind of 63 to 78 happen on small maps.

## 19. Part C: dwarven mines and arms (where its rules are)

Part C is in this stage (Paul, 2026-11-24). Its rules are folded into the numbered sections, each marked "(part C)":

| What | Section |
| --- | --- |
| race field `mining_permille`; tuning `mine_*`, `arms_trade_permille`, `mine_forms`; conflict fields `forge_strength_permille`, `arms_strength_permille` | 4.1, 4.2, 4.3 |
| stream `hist.alliances.mine` | 3 |
| state: open mines, settlements that had one, arms-sold pairs | 5 |
| step order | 6.1 |
| mines opened and closed | 6.12 |
| forges and arms; the trade boost | 6.13 |
| strength factors; `AllianceView::forged`, `arms` | 7.1, 7.5 |
| the site seam in monsters | 7.16 |
| events 77 `mine_opened`, 78 `arms_sold` | 8 |
| schema 11, site kind 3, reader checks, `schema.md` | 9.2 |
| report: `[sites]`, trade lines, `[stats]` | 10.2, 10.5, 10.6 |
| browser groups; the mine marker | 11.2, 11.3 |
| invariants 13, 14; targets MC1 to MC3; calibration step 7; tests 18.1 items 12, 13, 19 | 13, 15, 18 |

**The P5-07 seam** (6.14): P5-07 Mountain depths deepens these same mines (delvings, deep horrors woken in them, new ending codes); it adds events and codes on kind 3 sites and needs no second mine kind. The schema bump that r1 left to P5-07 is taken here.

## 20. Deferred, with the stage that takes them

P5-00 3.1 (P5-04's row) and 6.1 (P5-04's tables) are **superseded** by this spec on three points: vassalage (the `vassal` relation and "became vassal") is not in P5-04; table `civilization_census` (wealth and arms by year) is not written; the tuning id is `history.alliances`, not `history.economy`. P5-00 carries a note saying so.

| Item | Why not now | Stage |
| --- | --- | --- |
| Vassalage (`vassal` relation, tribute in goods) | needs a payment (wealth) and, to read as more than a label, a ruler who swears (figures); Paul, 2026-11-24: after figures | a small stage after P5-06 Figures (proposed P5-04b), or P5-10 |
| Wealth and `civilization_census` | no rule of this stage needs it (Paul, 2026-11-24) | with vassalage |
| Tribute in goods to a monster (P5-03 27) | needs wealth | with wealth |
| Allies sharing land (a region ceded to an ally) | allies would redraw the map, and the bloc bound and the territory view would have to follow (24 L1) | not planned |
| Roads: trade and reach over roads | P5-05 lowers crossing costs; `hops` will read roads | P5-05 |
| Mines deepened, deep horrors | 6.14 | P5-07 |
| Rulers who swear and break alliances; marriages; envoys | figures | P5-06 |
| Gods blessing or cursing an alliance | interventions | P5-08 |
| Artifacts as gifts between allies | | P5-11 |
| Trade as a path into mixed settlements (`add_people`, P5-03 7.5) | thin trade | a later trade round |

## 21. For P6, and for the world critic

**For P6** (P5-00 10.2): alliances and trades at E (`relation` rows in force at E − 1) are who a realm's NPCs call friends and whose merchants are in town; a betrayal is a grievance an NPC can name ("the Urarz broke faith with us in 5820"), and so are spoils kept ("they kept the spoils of the Grey Fells from us"); a quest hook (an oath broken, an ally deserted, a share owed); allied battles are battlefields with two banners; an open mine is a place (a dungeon-to-be for P5-07 and P6), and its arms are what a buyer's soldiers carry. No replay is needed.

**For the world critic** (0.26.0; default seeds 0, 1, 17, 42, 84; `civs4` 1; `--races dwarf,human` 0; 512 seed 3): whether alliances read as politics (who allies with whom and why; how long they last; whether the reasons in the sentences ring true); joined wars and allied battles; spoils shared and kept, and whether a spurning reads as earned; betrayals and desertions; the elf–faerie bond; trade lines; mines, their names and the arms trade (do the dwarves read as great forgers?); the `[alliances]` and `[sites]` blocks; whether blocs feel like history or like a rule.

## 22. Seams for later stages

- `AllianceView` (7.1) is the one read conflict has of alliances, trade and arms; vassalage adds a kind and a read, not a new path.
- `alliance_formed`'s `basis` is extended by appending codes (a figure's marriage, a god's blessing) with a new kind if roles change (P3-04 8.2).
- Trade's, forges' and arms' strength are factors in `force_of` (7.5); wealth can later replace the partner count and the flags without moving any call site.
- A share of the spoils is a value in people (6.11); when wealth comes, `share` can be read as its measure, or a new kind can carry goods, without changing 74 and 75.
- `credit` (6.9) remains the one path by which these events move peoples' dispositions; `add_grudge` (7.15) the one path by which they move a grudge between realms.
- Mines are `site` rows of kind 3 made through the site seam (7.16); P5-07 deepens them (6.14).
- Roads (P5-05) change `PeoplesView::hops`, and reach and trade reach follow.

## 23. Amendments to earlier specs

| Spec | Section | What | Note |
| --- | --- | --- | --- |
| P3-01 | race fields; kind `history` ids | `origin_allies`, `ally_join_permille`, `mining_permille`; id `alliances` | pointer |
| P3-04 | 6.4, 8.2, 8.4 | configuration; kinds 63–78; forms `with`, `against` | r25 pointer |
| P3-06 | 4.2, 5.1, 5.2, 6.1, 7.2, 7.3, 7.5, 7.6, 7.7, 7.8, 8.1, 12.1, 20.3, 20.6, 20.15, 22.5 | section 7 here (a grudge source, plunder and tribute recorded, the strength factors) | r24 pointer |
| P3-07 | new 30; 24.2 | format v7, `[alliances]`, civilization lines, companions, battle kinds, `[stats]` | r25 pointer |
| P4-01 | 5, 13.1 | alliances and trades by year | r6 |
| P4-04 | 3.6, 4, 6.1, markers | `allied_battle` as a battle; the first site marker, mines (glyph 19) | r3 pointer |
| P4-05 | 4.4 | group `alliances` = 7, `ALL_GROUPS` 0xFF; kinds 71, 77 in existing groups | r4 |
| P5-00 | 3.1 P5-04, 4.6, 6.1 | superseded on vassalage, `civilization_census` and `history.economy` (20); the note is in P5-00 | note |
| P5-02 | 5, 6.3.2, 6.6, 11.2, 11.4, 12.5 | allied battles count as violent dead; site kind 3 and the site seam; P5-02's loops skip mines; `[sites]` counts mines | pointer |
| P5-03a | 3.5, 13 | the credits of 6.9 | pointer |
| P5-12 | data | rule `slaughter` reads `allied_battle` | — |
| `schema.md` | 1, `site`, 5, 6, relation kinds | 9.2 | the engineer |

## 24. Decided

**Paul, 2026-11-24**, on r1's seven questions (relayed by the lead):

1. **Allies in the partner's battles**: as recommended. Allies fight inside the partner's battles (`allied_battle`, 7.7).
2. **Spoils**, changed from r1's recommendation: *"the ally should receive a share and if the other ally doesn't want to give it, it might sour the relationship."* An ally that fought in a won war is owed a share of the tribute and plunder in proportion to its dead; the principal grants or withholds it by a keyed draw; a withheld share chills the peoples, adds a grudge between the realms, and two in one term (or one that cools a cool alliance past breaking) end the alliance as spurned (6.7, 6.11).
3. **Elves and faeries**: as recommended. Allied by origin from the faeries' first year wherever they live, never broken by a refusal; faeries seldom send armies (6.4, 4.1).
4. **Betrayal and desertion**: as recommended (6.5, 6.6).
5. **Wealth and vassalage**: as recommended. No wealth number; vassalage deferred until after figures (20).
6. **Part C**: **now**, not with P5-07: schema 11, generator 0.26.0, the mine glyph (6.12, 6.13, 9.2, 11.3, 19).
7. **The bloc bound by rule**: as recommended. 500‰ to form, 700‰ to keep (6.3, 6.7).

**The lead, 2026-11-24**: report format v7 (10.1); browser group `alliances` = 7 and `ALL_GROUPS` 0xFF, `allied_battle` in `war`, the mine and arms kinds in a group the spec proposes (11.2: `mine_opened` in `settlements`, `arms_sold` in `alliances`); tuning id `history.alliances`; P5-00 3.1 and 6.1 superseded, with a note in P5-00 (20).

**Readings taken in r2** (the spec writer's; for the lead, not Paul):

- **L1. No land in the spoils.** Only tribute and plunder (sacks) are shared; a ceded region or a captured town stays the principal's. Reason: a share of land would let allies redraw the map, which the bloc bound counts by direct allies and the territory view draws by holder; and a region given to an ally the principal did not conquer it for would need a cession event and a holder change outside conflict. *Recommendation*: keep it so.
- **L2. One withheld share does not break.** No rule ends an alliance at one withheld share, but its chill (up to 170 at the shipped values) can take an alliance already near −350 below the break line, and then the break by cooling is written as spurned. Alliances formed at −130 or warmer cannot break in the year of one withheld share. *Recommendation*: keep it (the last straw reads true); if the lead wants a strict "never at one share", the cooling test can ignore the most recent withheld share's grudge for `term_years`, at the cost of one more rule.
- **L3. Spoils measured in people, nothing moves.** A share is a number (the event's `share`), as a tribute's people are; no population moves to the ally, as none moves to a tribute's payee (P3-06 20.3). *Recommendation*: keep it until wealth.
- **L4. Origin alliances are never spurned.** Kept spoils chill the elves and faeries but never end their bond (only betrayal and the bloc rule do). *Recommendation*: keep it.
- **L5. The forge's own strength.** A realm with an open mine gains `forge_strength_permille` (5%) itself, besides the arms its buyers gain; r1 had only the buyers. Reason: "Dwarves are great forgers" (design 5) should show in the dwarves' own wars. *Recommendation*: keep it, calibrated against MC3.

## 25. Proposals

Two entries in `docs/design/proposals.md`: "History: alliances and trade, P5-04 (readings and scope)" and "Schema 11: mine sites (P5-04 part C)", both decided by Paul on 2026-11-24.

## Revision notes

- r1 (2026-11-24): first draft, for Paul's approval.
- r2 (2026-11-24): approved by Paul with his decisions (24). Part C taken now and folded into the numbered sections (4.1–4.3, 5, 6.12–6.14, 7.1, 7.5, 7.16, 8, 9.2, 10.6, 11.3, 19); schema 11; report format v7 (lead); spoils of war for allies (6.11, 7.15, events 74 and 75) and the alliance spurned over spoils kept (6.7, event 76); mine and arms kinds renumbered 77 and 78; the forge's own strength (L5); targets SP1–SP3 and MC1–MC3, calibration order, tests, goldens and amendments updated; open questions replaced by the decided list; P5-00 superseded on three points (20).
