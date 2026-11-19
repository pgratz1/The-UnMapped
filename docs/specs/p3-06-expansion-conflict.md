# P3-06 Expansion and conflict

Status: **draft for the lead's approval** (written by the spec writer 2026-11-19). Phase P3 History core, spec 6 (P3-00 3.1 row P3-06), a subsystem of pipeline stage 8, `history`, and its tension source. Generator **0.16.0**; **no schema change** (schema stays 7: P3-06 appends event kinds 20–30 to the catalogue and writes rows of `event`, `relation`, `region_control`, `settlement`, `civilization` and `settlement_census` through P3-04's `State`; no column). Design doc sections 5 (the races and their camps and characters; "goblins hostile to elves from the first year", dwarves "arrive into a world already at war", humans "spread quickly over land left open or ruined by older wars", orcs "their coming opens the most recent age of conflict"; deep time and flare-ups "two expanding borders meet"; alignment sets starting relations; race relations "old wars leave grudges"; mountains, passes and roads in history; the tick loop items 2 and 5), 6 (`relation`, `settlement`, `civilization`), 13 (P3 row: wars, ruin; P5 exit: "no world is empty or runaway") and 14 (records a model can narrate). Depends on P3-00 (binding), P3-01 (race fields `camp`, `expansion`, `aggression`; amended in 18.3: one new race field), P3-02 (keyed draws), P3-04 (engine, step seam, `TensionSource`, flare-ups SC5/SC6, event writer W1–W9, bookkeeping), **P3-05 (peoples: its `PeoplesView`, `note_reduction`, and one new hook, amendment 18.1, to be applied before P3-05 is implemented)** and P3-07 (report: section 9 statistics, section 20 requirements; amended in 18.4). Branch read: `origin/p3/05-spec` at 73b01a4 (newest text of P3-00 to P3-07). Proposals read: up to "History: random pressures act only in active phases; migration waits for conflict".

Numbers in this spec come from the spec writer's exact-integer prototype `~/.cache/the-unmapped/out/p3-06-spec/conflict.py`, built on the P3-05 prototype `peoples.py` (which it imports unchanged) with a dynamic scheduler that follows P3-04 SC1–SC6, run on the P1-06 validation worlds `~/.cache/the-unmapped/out/p1-06-validation/` (geography unchanged since 0.11.0). The prototype is evidence, not the reference (16).

## 1. Purpose

P3-06 gives the history its conflicts. Civilizations hold dispositions toward each other from their creators' camps and from their own past; neighbours that dislike each other, or that want each other's land, build up **strain**; strain becomes a **claim** on a border region, the claim becomes the casus belli of a **war**, and the war is fought over the region graph in **battles**, **sieges**, **sackings** and **captures** until one side is exhausted, the war has run its course, or one side is gone. Peace leaves a truce, a grudge, sometimes a **ceded** region. A sacked town is a ruin and its survivors flee; a civilization that loses its last settlement is **conquered** and ends. In quiet intervals strain keeps building and no one fights; the highest strain is the **tension** that the engine compares with its flare-up threshold (P3-04 6.5), so that a quiet interval is interrupted when "two expanding borders meet" (design 5). The result of the 0.16.0 merge is a history in which the peoples of P3-05 meet, quarrel and fight: goblin hordes raid the elven woods from their first generations, old feuds flare up in the quiet centuries, realms fall and their towns lie in ruins, and the orcs' arrival opens a last age of war.

## 2. What this spec touches (for the engineer)

| Area | Files |
| --- | --- |
| Subsystem | new `worldgen/include/worldgen/history/conflict.hpp`, `worldgen/src/history/conflict.cpp` (the subsystem and tension source, sections 4–9), `worldgen/src/history/conflict_war.cpp` (sections 7–8: battles, sieges, falls, peace) |
| Stage | `worldgen/src/stages/history.cpp`: the shipped `EngineConfig` adds `ConflictSubsystem` after `PeoplesSubsystem` and installs it as the one tension source (P3-04 6.4) |
| Peoples hook | `worldgen/src/history/peoples.cpp`: `PeoplesSubsystem::ruin` (amendment 18.1) |
| Catalogue | `event_catalogue.cpp`: kinds 20–30 (11) |
| Streams | `worldgen/include/worldgen/core/streams.hpp`: five `hist.conflict.*` names (3.3) |
| Data | new `game/data/history/conflict.toml` and identical copies under `worldgen/tests/data/geo/history/` and `worldgen/tests/data/geo-small/history/`; registration of id `conflict` (kind `history`); `game/data/history/report.toml` and the fixtures: eleven entries; `game/data/races/*.toml` and the fixture races: field `prowess_permille` and the value change of 12.2; the race kind's schema gains `prowess_permille` (P3-01 amendment, 18.3) |
| Report | `worldgen/src/inspect/report.cpp`: P3-07's `[stats]` gains the keys of 15.4 (amendment 18.4); the world report's `[history]` is unchanged |
| Version | `worldgen/include/worldgen/version.hpp`: **0.16.0** |
| Docs | `docs/specs/schema.md`: the catalogue table gains kinds 20–30; the relation kinds `enmity`, `war`, `peace` with their symmetry (18.5) |
| Tests | `worldgen/tests/history/` (new CTest group `worldgen-tests-conflict`, pattern `*history_conflict_*`), `tools/worldgen-cli/tests/`; reference `worldgen/tests/reference/p3_conflict_reference.py`; golden `worldgen/tests/golden/p3-06-conflict.txt` |

## 3. Inputs

### 3.1 What the subsystem reads

| Input | Source | Used for |
| --- | --- | --- |
| Region rows | `region` (P1-06): `cells`, `lake_cells`, `walk_cells` (through P3-05's rough share and islet flag) | cover (6.3), islets, shares of the land (5.4) |
| Biome shares | `region_biome` through P3-05 4.1 `share_b` | cover (6.3) |
| Borders | `region_border`: `walk_pairs`, `pass_id`, `river_pairs`, `length_m`, through P3-05's `neighbours(R)` with cost (4.4 there) and the row itself | marches (6.1), crossing defence (6.3) |
| Race rows and data | `race.camp` (P3-03), race data `aggression` and the new field `prowess_permille` (12.2) | dispositions (4), strain (5), force (6.2) |
| Peoples | `PeoplesView` (P3-05 13): `score`, `capacity`, `neighbours`, `population`, `regions`, `settlements`, `wanted_held`, `hunger` | everything numeric about peoples |
| State | P3-04 `State`: civilizations (`race`, `capital`, `founded_year`, `founding_event`, `ended_year`, `ending_event`), settlements, `RegionState` (`owner`, `owner_event`), relations | everything else |
| Settings | `param.history_detail` (only through the engine: the flare-up threshold is the engine's) | — |
| Tuning | `history.conflict` (12.1), `history.peoples` (`expand_*`, `reach_*` through `wanted_held`; `cost_penalty`) | everything numeric |

Not read: the `pass` table. A battle at a pass records the border's `pass_id` (11, kind `battle_at_pass`); P6 locates the battlefield at the pass's col (`pass.col_x`, `col_y`, P3-00 7 "Passes", decided) from that id. `HistoryInputs` gains nothing (P3-04 R1 and R2 are unchanged).

### 3.2 State of the subsystem (its own members)

| Member | Type | Meaning |
| --- | --- | --- |
| `strain[(a, b)]` | `std::map<std::pair<int64,int64>, int64>` | strain of civilization a against b, in milli-units (5.2); absent = 0 |
| `claim[(a, b)]` | map to (event id, region) | a's open claim on a region of b (5.3) |
| `grudge[{a, b}]` | map keyed by (min, max) | the pair's grudge (4.2); absent = 0 |
| `last_peace[{a, b}]` | map to (year, event id) | the pair's last peace or truce (7.7) |
| `wars` | vector in declaration order | open and closed wars (7.1); a war is keyed by its `war_declared` event id |
| `enmity_opened` | set of (a, b) | pairs given an `enmity` row (4.3) |
| `seen` | set of civilization ids | civilizations already met by housekeeping (8.1) |
| `last_t2_phase` | int64 | the phase id of the last T2, to recognize the first year of a flare-up (7.2) |

Every map is ordered by integer keys (H3); nothing is iterated in insertion order.

### 3.3 Random streams (P3-00 4.6, P3-02 4.1)

Registered in `streams.hpp` after `hist.peoples.split`, in this order (51 names in all; amendment 18.2): `hist.conflict.declare`, `hist.conflict.battle`, `hist.conflict.battle_win`, `hist.conflict.siege`, `hist.conflict.sack`. Every draw is `draw(stream, Y, a, b, 0)` with Y the tick year and (a, b) as each rule states; w denotes a war's `war_declared` event id (stored in the world, so the keys can be recomputed from the log). P3-06 draws nothing in quiet steps. Helpers `chance_pm` as P3-05 3.3.

## 4. Relations between civilizations

### 4.1 Disposition

All arithmetic `int64`; `div` is floor division (every divisor below is positive; P3-05 4's `floor_div` where a numerator can be negative). For civilizations a ≠ b with races r_a, r_b and camps k_a, k_b (P3-03 `race.camp`: 0 good, 1 neutral, 2 evil):

```
camp(a, b)  = camp_table[min(k_a, k_b)][max(k_a, k_b)]          // history.conflict.camp, six values
d(a, b)     = camp(a, b) − grudge{a, b}                          // disposition, symmetric
h(a, b)     = max(0, −d(a, b))                                   // hostility
```

Shipped camp table (P3-00 10.6: same camp friendly, good against evil hostile, neutral wary of both; design 5 "alignment sets starting relations"):

| | good | neutral | evil |
| --- | --- | --- | --- |
| good | +400 | −150 | **−700** |
| neutral | | +100 | −250 |
| evil | | | +300 |

Same race is same camp, so same-race civilizations start friendly; the only same-race term is the secession grudge (4.2). The design's "each pair of races has a starting disposition" is this camp table in P3 (per-race-pair dispositions and the `race_relation` table are P5, P3-00 1).

### 4.2 Grudges ("old wars leave grudges", design 5)

`grudge{a, b}` ≥ 0, symmetric, starts at 0 and changes only by these rules:

| When | Change |
| --- | --- |
| a civilization c is born by secession from p (its `founding_event` is a `civ_seceded` with p as `from`; seen first by housekeeping, 8.1) | grudge{p, c} += `secession_grudge` |
| a settlement of one is sacked or taken by the other (7.5) | += `capture_grudge` |
| a war between them ends by peace or truce (7.7) | += `war_grudge` |
| every T2 (1 year) and every Q4 (L years) | every grudge −= years × `grudge_decay_per_century` div 100, floored at 0 (all pairs, ascending key) |

### 4.3 Relation kinds stored in `relation` (P3-07 20 item 1)

All rows have `a_type` = `b_type` = 3 (civilization); P3-04 6.7 writes the rows from the state's relation map by comparison, so P3-06 only calls `open_relation` and `close_relation` (P3-04 6.1).

| Kind | Symmetric | a, b | Opened by | Closed by | Interval rule |
| --- | --- | --- | --- | --- | --- |
| `enmity` | yes (a < b) | the pair | the younger civilization's `founding_event` (its `civ_founded` or `civ_seceded`), in the first P3-06 step in which both are alive and visible and camp(a, b) ≤ `enmity_max` | the `ending_event` of whichever ends first (8.1) | at most one row per pair, ever; the shipped table opens it for good–evil pairs only, so "goblins are hostile to elves from their first year" (design 5) is a row of the report that starts in the goblins' founding year |
| `war` | **no**: a = the attacker, b = the defender | the war's sides | `war_declared` (7.2) | its `peace` or `truce` (7.7), or the `civ_conquered` / `civ_ended` that ends a side (7.6, 8.1) | at most one open war per pair; a pair's wars do not overlap |
| `peace` | yes (a < b) | the pair | each `peace` or `truce` (7.7) | the pair's next `war_declared`, or the ending event of a side | the truce of `truce_years` is a rule on the open row's `since` (no event closes a truce) |

**Deferred** (with reasons; P3-00 6.1 lists "peace treaty, vassal…" as possible): alliances and coalitions as relations (a joint war needs war goals for several attackers and a rule for sharing conquests; P3-06's coalition is the fear term of 5.1 instead, which needs no stored row); vassalage and tribute (needs a payment or a ruler figure to be anything more than a label; P5 brings figures and factions). Neither needs a schema change later: relation kinds are text keys (P3-04 9.2).

## 5. Strain, claims and tension

### 5.1 Contact and drive

**Presence** of civilization c: `presence(c)` = the regions c holds ∪ the regions of c's standing visible settlements (ascending). **March** from x to y (`front(x, y)`): for each region R of `presence(y)`, in ascending id:

- if R ∈ `presence(x)`: (R, cost 1, from R) (a shared region);
- else the cheapest path from `presence(x)` to R of at most `march_hops` open borders, with cost the sum of P3-05 4.4's border costs; ties by the smaller (cost, last region before R), compared lexicographically; the entry is (R, cost, that last region);
- else R is not reachable.

Paths may run through any region, held, free or an islet (armies cross other lands; the border costs already price ranges and rivers). `front(x, y)` is the list of reachable entries; x and y are **in contact** if it is non-empty, with **contact cost** k(x, y) = the smallest cost in it. Contact and k are symmetric (border costs are). With `march_hops` = 2, a horde two valleys away is a neighbour.

**Drive** of a against b (for a ≠ b, both alive and visible, in contact):

```
want(a, b)  = want_weight × min(hunger(a), hunger_full) div hunger_full   if wanted_held(a) ∩ held(b) ≠ ∅, else 0      // P3-05 13, 20
share(c)    = (non-islet regions c holds) × 1000 div N        (N = the non-islet regions; 0 when N = 0)
fear(b)     = fear_weight × max(0, share(b) − fear_share) div (1000 − fear_share)
drive(a, b) = max(0, border_friction − d(a, b) + want(a, b) + fear(b))
```

`want` is "two expanding borders meet" (design 5): a people boxed in (P3-05's `hunger`, years crowded with no free land) next to land it would take if it were free. `fear` is the coalition against a giant: everyone's drive against a civilization holding more than `fear_share` of the land rises. `border_friction` makes even wary neighbours quarrel in the long run (anti-stagnation, 10.2); friendly ones (d ≥ `border_friction`) never build strain without want or fear.

### 5.2 Strain

For each ordered pair (a, b), a ≠ b, both alive and visible, in contact, **not at war with each other** and **past their truce** (no `last_peace`, or Y ≥ its year + `truce_years`), with g = the aggression of a's race (P3-01 `aggression`):

```
rate(a, b)    = g × g div 1000 × drive(a, b) div 1000 × (1000 div k(a, b)) div 1000     // evaluated left to right
strain(a, b) += rate(a, b) × years × p,   p = tick_strain_permille in T2 (years 1), quiet_strain_permille in Q4 (years L)
strain(a, b)  = min(strain(a, b), strain_max × 1000)
```

Strain is in milli-units: `strain div 1000` is on the engine's tension scale (1000 = "a flare-up is due", P3-04 4.1). Aggression enters squared: readiness for war decides both how fast a people's grievances grow and, through drive, against whom (orcs 900: 810; goblins 600: 360; humans 400: 160; dwarves 300: 90; elves 150: 22; faeries 50: 2). Contact over a pass or a large river (cost 3) builds strain at a third of the rate over a walkable border: ranges and rivers keep peoples apart (design 5 "ranges are real barriers"). Pairs not in contact, at war or in truce keep their strain unchanged. A strain is reset to 0 for both directions of a pair when a war between them starts or ends (7.2, 7.7).

Quiet steps build strain at `quiet_strain_permille` (shipped 6) against ticks' `tick_strain_permille` (25): the quiet interval is the design's peace, in which grievances grow slowly until a flare-up; active phases are when "the new race settles, meets its neighbours, and the first wars and alliances form" (design 5).

### 5.3 Claims (the casus belli; T2 and Q4)

After the strain update of a step, for each ordered pair (a, b) in ascending (a, b) among the pairs in contact: if a has no open claim on b, they are not at war, they are past their truce, strain(a, b) ≥ `claim_strain` × 1000, and (in a quiet step) `quiet_events_left(a)` > 0, then a **claims** a region of b:

- the region R of `front(a, b)` with the smallest key (0 if R ∈ `wanted_held(a)` and R is held by b, else 1; −(σ(R, r_a) − `cost_penalty` × cost); R): land a wants first, then the best land for a's race within reach;
- append `claim` (region R; participants a as `claimant`, b as `holder`; cause: R's `owner_event` when b holds R, else the `founding_event` of b's most populous standing settlement in R, ties to the smaller id); `claim[(a, b)]` = (that event, R).

`wanted_held` and holdings are taken once at the start of the step's strain update (a snapshot), owners live. A claim stays open until a war between the pair starts or ends, or either side ends (8.1). Claims are the land claims of design 5's tick loop ("claim a region") for held land; a claim on free land is P3-05's founding (P3-05 9.3).

### 5.4 Tension (Q5; the `TensionSource`)

```
TensionReport tension(ctx):        // read-only (P3-04 6.4)
    best = (0, 0)
    for (a, b) with an open claim, ascending:
        if a and b alive, in contact (as computed in this step's Q4), not at war, past their truce:
            v = strain(a, b) div 1000
            if v > best.value: best = (v, claim[(a, b)].event)
    return best
```

The trigger of a flare-up is therefore always the claim of the pair whose strain crossed the threshold: P3-04 6.3 makes it the cause of the `flare_up` event and `phase.trigger_event`. Pairs without a claim report nothing, so a value ≥ the threshold always has a trigger (P3-04 6.5's fatal check cannot fire).

**Calibration against the engine's thresholds** (sparse 1500, normal 1000, dense 700; the 45% cap, P3-04 4.1): a goblin horde (aggression 600) against an elven realm across a walkable border (drive 200 + 700 = 900) builds 324 × 6 = 1,944 milli-units a quiet year: a claim after 155 quiet years and a flare-up after 515 at `normal` (360 at `dense`, 770 at `sparse`); an orc horde against a human kingdom (drive 450, rate 364) builds 9.1 a tick year: a claim and the first declarations within about 35 years of contact. With the shipped numbers a 1024 world has a median of 2 flare-ups (15.3); the simulated years stay within P3-00 4.1's median of 1000–1400 at `normal`.

## 6. Military strength and the field

### 6.1 Force

```
levy(c)  = Σ over c's standing visible settlements s of P_s × levy_permille[kind(s)] div 1000      // ascending s
force(c) = levy(c) × prowess_permille(r_c) div 1000                                                 // race field, 12.2
wars(c)  = the number of open wars with c as a side;   effective force in a war = force(c) div wars(c)
```

Shipped `levy_permille`: hamlet, village, town 100, city 80, fortress 250 (a fortress is a military hold; a city's people are less often under arms). Shipped `prowess_permille`: elf 1600, goblin 800, dwarf 1400, faerie 1200, human 1000, orc 1300 (12.2). So an elven realm of 20,000 puts 3,200 in the field and a goblin horde of 70,000 5,600; fighting on two fronts halves both.

### 6.2 Choosing where to fight

In a war (7.1) the side holding the **initiative** x attacks the other, y. If `front(x, y)` is empty, y attacks x instead this year (x and y swapped; the stored initiative changes only with a battle's winner); if that is empty too, the war has **no front** (7.7). The target is:

- the war's goal region, if x is the war's attacker and the goal is in `front(x, y)`;
- else the entry of `front(x, y)` with the smallest (−(σ(R, r_x) − `cost_penalty` × cost), R).

The entry gives R (the battlefield), its cost and R_from (the region the army enters from; = R for a shared region).

### 6.3 Defence

```
cover(R) = min(1000, rough(R) + share_temperate_forest(R) + share_wet_woodland(R) + share_fen(R) + share_marsh(R))   // P3-05 4.1, per mille
def(R)   = 1000 + terrain_defense × cover(R) div 1000
         + pass_defense      if R_from ≠ R and the border (R_from, R) has walk_pairs = 0 and pass_id > 0      // a chokepoint at the col
         + river_defense     if R_from ≠ R and the border is a large-river border (P3-00 7, P3-05 4.4)
         + fort_defense      if y has a standing visible fortress in R
```

Woods, wetlands and rough ground favour the defender: an elven forest doubles nothing but adds half. A pass held against an army doubles the defence (design 5 "passes are places to fortify and fight over"); lowland borders are political and give nothing (P3-00 7, decided).

## 7. War

### 7.1 A war

Created by a declaration (7.2): attacker a, defender b, goal region G, start year, its declaration event w (the war's key for draws), last event (initially w), initiative (initially a), and per side: battles won, settlements taken, dead, and `pop0` = the side's population at the declaration (`PeoplesView::population`).

### 7.2 Declaration (T2)

After the strain update and claims of T2 (5.2, 5.3), for each alive visible civilization a in ascending id that is not the attacker of an open war:

1. Candidates: the b with an open claim of a, b alive and visible, in contact, not at war with a, past their truce, and Y ≥ `founded_year`(b) + `shelter_years` (a people just awakened or seceded is not attacked in its first years; it may attack).
2. **Flare-up war**: if this is the first T2 of a flare-up phase (the phase id differs from `last_t2_phase` and the phase kind is flare-up), the flare pair (a*, b*) is the best pair of 5.4 recomputed on this step's strain and claims; if a = a* and b* is a candidate, a declares war on b* without a draw. A flare-up is the moment "a tension measure crosses a threshold" (design 5); a war follows from it whenever the pair can still fight.
3. Otherwise b = the candidate with the largest strain(a, b), ties to the smaller b; s = strain(a, b) div 1000; if s < `declare_min`, nothing. Else

```
p    = declare_permille × min(1000, (s − declare_min) × 1000 div declare_span) div 1000
odds = min(1000, force(a) × 1000 div max(1, force(b) × declare_ratio div 1000 × (1000 − g) div 1000))      // g: a's aggression
p    = p × odds div 1000
declare if chance_pm(draw(hist.conflict.declare, Y, a, b, 0), p)
```

A people declares sooner the more strained it is and the stronger it is against its target, and a warlike people cares less about the odds: full odds need a force of `declare_ratio` × (1000 − g) / 10^6 of the target's, 85% of it for elves (150), 40% for goblins (600), 10% for orcs (900).

**Declaring**: the goal G = the claim's region, unless b no longer holds it and has no standing settlement in it, then the best entry of `front(a, b)` by 6.2's key. Append `war_declared` (region G; participants a as `attacker`, b as `defender`; causes, ascending: the claim's event and the pair's `last_peace` event if any); close the pair's open `peace` relation with it (if any); open relation `war` (a, b) with it; strain(a, b) = strain(b, a) = 0; drop the claims of both directions; create the war (7.1). `last_t2_phase` = the current phase id at the end of every T2.

Wars are declared only in ticks (T2). Several wars of one civilization at once are possible as a defender, at most one as an attacker.

### 7.3 Battles (T6)

For each open war in declaration order (ascending w):

1. x, y, the target entry (R, cost, R_from) by 6.2; no front: go to 7.7.
2. A battle happens if `chance_pm(draw(hist.conflict.battle, Y, w, 0, 0), battle_permille)`; otherwise go to 7.7.
3. A = force(x) div wars(x), D = force(y) div wars(y), D' = D × def(R) div 1000; p = A × 1000 div (A + D') (500 when A + D' = 0); x wins if `chance_pm(draw(hist.conflict.battle_win, Y, w, 0, 0), p)`.
4. The winner W with effective force F_W, the loser L with F_L (A or D, unmultiplied): dead_L = F_L × `loss_loser` div 1000, dead_W = F_W × `loss_winner` div 1000.
5. Append `battle_at_pass` when def(R) included `pass_defense` (value `pass` = that border's `pass_id`), else `battle` (region R; participants W as `winner`, L as `loser`; values `winner_dead`, `loser_dead`; cause w).
6. Casualties of W then of L (7.4) with that event; the war's dead, battles won by W, last event = the battle, initiative = W.
7. If x won and y has standing visible settlements in R: the **siege** of t = the most populous (then smaller id):

```
walls = walls_permille[kind(t)] + (capital_walls if t is y's capital)
G     = P_t × garrison_permille div 1000 × prowess(r_y) div 1000 × walls div 1000
A2    = max(0, A − dead_x)                      // x's dead in this battle (x won: dead_W)
ps    = A2 × 1000 div (A2 + G)                  // 0 when A2 + G = 0
fell  = chance_pm(draw(hist.conflict.siege, Y, w, t, 0), ps)
if last_refuge and t is the only standing settlement of the only alive civilization of its race: fell = false
```

   Append `siege` (region R, settlement t; participants x as `attacker`, y as `defender`; value `fell` 1 or 0; cause the battle); last event = the siege. If it fell: 7.5.

Then 7.7 for this war, before the next war.

### 7.4 Casualties

`casualties(c, dead, e)`: ss = c's standing visible settlements ascending, T = Σ P over ss (if T = 0 or dead = 0: nothing); dead = min(dead, T) (never binding with shipped tuning: dead ≤ force ≤ T × 0.25 × 1.6 × 0.25); each s loses dead × P_s div T; the remainder goes from the most populous settlement after those losses (then smaller id), at most its population; `set_population` for each; `note_reduction(s, e)` for every s of ss (P3-05 13: the cause of a later abandonment). Status is not changed (a war is not a P3-05 pressure; P3-05's recovery rule needs no `before`). The war's dead of c += dead.

### 7.5 Falls: sacking and capture

A fallen settlement t of y, taken by x in region R:

1. **Sack or capture**: sack if `chance_pm(draw(hist.conflict.sack, Y, w, t, 0), min(1000, sack_base + h(x, y) × sack_hostility div 1000))`. Hatred sacks (goblins against elves, h = 700: 900‰), neighbours of one camp take (h = 0: 200‰).
2. **Sack**: dead = P_t × `sack_toll` div 1000; P_t −= dead; append `sacked` (region R, settlement t; participants x as `attacker`, y as `defender`; values `dead`, `survivors` = P_t; cause the siege; salience `capital_sacked_salience` if t is y's capital, else the base); then `PeoplesSubsystem::ruin(t, that event)` (amendment 18.1): the survivors join y's nearest other standing settlement up to its room and the rest are lost, t is ruined with `ruin_event` = the sack and no owner, y's hold on R is released if y has no other standing settlement there, and y's capital moves if t was it.
3. **Capture**: dead = P_t × `capture_toll` div 1000; P_t −= dead; excess = max(0, P_t − 2 × C(R, r_x)) (P3-00 12.3's bound for the new owner's race); append `settlement_captured` (region R, settlement t; participants x as `attacker`, y as `defender`; values `dead`, `fled` = excess; cause the siege; salience `capital_captured_salience` if t is y's capital, else the base); P_t −= excess and the excess joins y's nearest other standing settlement as in P3-05 8.5 item 2; `set_population`; `set_settlement_owner(t, x, e)`; if y holds R and has no other standing settlement in R, `set_region_owner(R, x, e)` (**conquest of the region**); if t was y's capital, P3-05 8.5 item 5's rule (most populous standing settlement, then smaller id; `capital_moved`, cause e).
4. The war's settlements taken by x += 1; grudge{x, y} += `capture_grudge`; the war's last event = e.
5. If y has no standing settlement left: 7.6 with cause e.

### 7.6 Conquest of a civilization

`conquer(y, x, e, R)`: append `civ_conquered` (region R; participants y as `civilization`, x as `conqueror`; cause e); `end_civilization(y, that event)` (it is y's `ending_event`, P3-07 20 item 3); every region y still holds is freed with it; every open relation with y as a side (in `RelationKey` order) is closed with it; every open war with y as a side is over (no peace event). y's people: those of captured settlements are now x's; the survivors of its last sack are lost (no settlement to flee to).

### 7.7 The end of a war and peace

After 7.3 for a war that is still open, in this order:

1. **Exhaustion**: the sides with dead × 1000 ≥ `exhaustion_permille` × max(1, pop0). If any: s = the war score (below); if both are exhausted, or s = 0, or the exhausted side is not the one s favours, the war ends with s's outcome; if the one exhausted side leads on score, the war ends in a truce (a winning side that has bled itself white stops, but does not lose).
2. **No front** (6.2): ends with s's outcome.
3. **Length**: Y − start ≥ `war_max_years`: ends with s's outcome.

**War score**: (battles won by a − by b) + 2 × (settlements taken by a − by b): positive → a wins, negative → b wins, 0 → truce.

**Ending with winner W, loser L** (P3-07's excerpt shape): append `peace` (no region; participants W as `winner`, L as `loser`; value `years` = Y − start; cause the war's last event). **Ending in a truce**: append `truce` (participants both as `parties`, ascending id; value `years`; same cause). Then: close the `war` relation with that event; open the pair's `peace` relation with it; `last_peace{a, b}` = (Y, it); grudge{a, b} += `war_grudge`; strains of both directions 0; claims of both directions dropped.

**Cession**: if W is the attacker, L still holds the goal G, and G is not the region of L's capital: append `region_ceded` (region G; participants L as `from`, W as `to`; cause the peace); each standing settlement of L in G in ascending id gives its excess over 2 × C(G, r_W) to L's nearest other standing settlement (P3-05 8.5 item 2) and passes to W (`set_settlement_owner`, the cession event); `set_region_owner(G, W, cession)`. The peace terms are thus: every settlement taken stays taken, the attacker's goal is ceded when the attacker wins, and a truce of `truce_years` follows.

### 7.8 Quiet steps (Q4) and the end of an active phase

Quiet steps have no battles and no declarations (P3-00 4.4). P3-06's Q4, after P3-05's foundings: (1) housekeeping (8.1); (2) **every open war ends**, in declaration order, by the war score as in 7.7 (peace with cession, or truce), dated at the step's start (the year after the active phase): the fighting of a flare-up or an opening phase does not outlast it. An end that finds no quiet-event budget for its charged civilization (the winner, or the smaller id of a truce; for a cession also the loser) waits for the next step, the war still open and inactive (P3-04 W9 holds by construction, P3-05 10.5's rule); (3) grudge decay over L years; (4) strain over L years and claims (5.2, 5.3). Then the engine's Q5 asks the tension (5.4).

A war is therefore never carried across a quiet interval. A flare-up cut at an awakening (P3-04 E3) runs straight into the next opening phase, so its war goes on there.

## 8. Order of operations

### 8.1 Housekeeping (first in each of T2, T6, Q4)

1. Every open relation (in `RelationKey` order) with a side that has ended (by P3-05's decline, or earlier in this step) is closed with that side's `ending_event` (the smaller id's first if both); every open war with an ended side is over; claims involving an ended civilization are dropped.
2. Each alive visible civilization not yet in `seen`, ascending: added; if its founding event is a `civ_seceded`, the secession grudge of 4.2.
3. Each pair a < b of alive visible civilizations not in `enmity_opened` with camp(a, b) ≤ `enmity_max`: open `enmity` (a, b) with b's `founding_event` (b is the younger: ids follow founding order); add to `enmity_opened`.

### 8.2 The steps

| Step | P3-06 does (after P3-05's part of the same step) |
| --- | --- |
| T2 | housekeeping; grudge decay (1 year); strain (1 year, `tick_strain_permille`) and claims; declarations (7.2) |
| T6 | housekeeping; each open war: battle, siege, fall, conquest (7.3–7.6), end of war (7.7) |
| Q4 | housekeeping; end of every open war (7.8); grudge decay (L years); strain (L years, `quiet_strain_permille`) and claims |
| Q5 | tension (5.4), read-only |

P3-06 installs as the second subsystem (`EngineConfig::subsystems` = peoples, conflict) with steps T2, T6, Q4, and as the only tension source. It runs in no other step; it changes the world only through `State` and P3-05's hooks (`note_reduction`, `ruin`). Entities created in a step are not processed in it (P3-04 6.2): P3-05's colonies of this T2 do not count in `presence` (their region's ownership does), a civilization founded by secession in T4 is first seen in T6.

## 9. The race characters (design 5; P3-00 13)

| Character | Rule and data that produce it | Measured (15) |
| --- | --- | --- |
| Goblins hostile to elves from their first year | camp table good–evil −700: an `enmity` row from the goblins' founding year (4.3); the fastest strain of any older pair (5.4); sacks over captures at h = 700 (7.5) | MEASURE-GOBLIN |
| Dwarves arrive into a world already at war | dwarves are neutral (wary, −150/−250); their awakening opens an active phase in which the strain the goblins and elves built in the quiet centuries turns into declarations (declare_min below the flare threshold) | MEASURE-DWARF |
| Humans spread over land left open or ruined | sacks free regions (7.5 item 2) and P3-05's outward foundings resettle free land; humans reach 3 hops | MEASURE-HUMAN |
| Orcs open the last age of conflict | aggression 900 (810 squared, the largest); evil against everyone but goblins; strong (prowess 1300); their age is active to the present (P3-04 E4) | MEASURE-ORC |
| Elves old and few, oldest in the present | prowess 1600 in forest cover; capitals walled; `last_refuge` | MEASURE-ELF |
| No race wiped out | `last_refuge` (7.3 item 7) and `shelter_years` (7.2) | MEASURE-EXTINCT |

## 10. Anti-runaway and anti-stagnation

### 10.1 What stops one civilization from holding the map

1. **Reach and secession** (P3-05 9.3, 10.3): a civilization founds only within reach of its capital and large realms split in succession crises; conquests do not extend reach.
2. **Overextension**: a civilization's force is split over its open wars (6.1); a large realm is fought on several fronts.
3. **Coalition by fear** (5.1): above `fear_share` (400‰) of the land, every neighbour's drive against the holder rises, up to `fear_weight` at the whole map.
4. **Exhaustion and limited war** (7.7): wars end after `war_max_years` at most, by exhaustion long before; peace cedes at most the goal region.
5. **Defence**: walls, capitals, terrain and passes make the last cities of a people the hardest to take.

MEASURE-RUNAWAY

### 10.2 What stops the world from freezing

Strain never decays: any two neighbours with drive > 0 (every pair below +200 disposition, every pair with want or fear) quarrel eventually; grudges decay, so old enemies of one camp make peace in the long run while good and evil stay enemies; P3-05's growth and foundings keep presence and hunger changing; the flare-up rule turns accumulated strain into an active phase. MEASURE-STAGNATION

DRAFT-REST
