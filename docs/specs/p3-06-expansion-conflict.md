# P3-06 Expansion and conflict

Status: **approved by the lead 2026-11-19 as the basis for implementation; r14 (2026-11-20, P3-10, generator 0.19.0): section 21, P3-10 follow-ups; revision r7 (2026-11-20, for P3-08, generator 0.17.0; `docs/specs/p3-08-followups.md`): battle losses from the smaller force (7.3), the capture sentence without "and 0 fled" (11), casualties under extreme tuning (7.4, 8.4), a same-step colony in a ceded region (8.3), the peoples-only prefix of the log (16.2), the 100-seed figures (15.3, 15.5), same-year rows (8.3)** (the ten questions of section 19 go to Paul; the interim choices stand; the measured shortfalls of 15.2 and 19 items 7, 9, 10 go to the world critic after implementation). Written by the spec writer 2026-11-19; the amendments of section 18 applied the same day on `p3/06-spec`, after merging `origin/p3/04-spec-fixes` at 13b8062 (P3-04 r3, P3-07 r2); checked against P3-04 r3 (16.5, note at the end). Phase P3 History core, spec 6 (P3-00 3.1 row P3-06), a subsystem of pipeline stage 8, `history`, and its tension source. Generator **0.16.0**; **no schema change** (schema stays 7: P3-06 appends event kinds 20–30 to the catalogue and writes rows of `event`, `relation`, `region_control`, `settlement`, `civilization` and `settlement_census` through P3-04's `State`; no column). Design doc sections 5 (the races and their camps and characters; "goblins hostile to elves from the first year", dwarves "arrive into a world already at war", humans "spread quickly over land left open or ruined by older wars", orcs "their coming opens the most recent age of conflict"; deep time and flare-ups "two expanding borders meet"; alignment sets starting relations; race relations "old wars leave grudges"; mountains, passes and roads in history; the tick loop items 2 and 5), 6 (`relation`, `settlement`, `civilization`), 13 (P3 row: wars, ruin; P5 exit: "no world is empty or runaway") and 14 (records a model can narrate). Depends on P3-00 (binding), P3-01 (race fields `camp`, `expansion`, `aggression`; amended in 18.3: one new race field), P3-02 (keyed draws), P3-04 (engine, step seam, `TensionSource`, flare-ups SC5/SC6, event writer W1–W9, bookkeeping), **P3-05 (peoples: its `PeoplesView`, `note_reduction`, and one new hook, amendment 18.1, to be applied before P3-05 is implemented)** and P3-07 (report: section 9 statistics, section 20 requirements; amended in 18.4). Branch read: `origin/p3/05-spec` at 73b01a4 (newest text of P3-00 to P3-07). Proposals read: up to "History: random pressures act only in active phases; migration waits for conflict".

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
| Tuning | `history.conflict` (12.1); `history.peoples` only through `PeoplesView` (`wanted_held`, `hunger`) and its `intensity` record (5.2) | everything numeric |

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
strain(a, b) += rate(a, b) × years × p × I div 1000,   p = tick_strain_permille in T2 (years 1), quiet_strain_permille in Q4 (years L)
strain(a, b)  = min(strain(a, b), strain_max × 1000)
```

I is the event intensity multiplier of P3-05 10.1 (`history.peoples.intensity`: calm 500, normal 1000, turbulent 2000 for `param.event_intensity`): a turbulent world quarrels twice as fast. Strain is in milli-units: `strain div 1000` is on the engine's tension scale (1000 = "a flare-up is due", P3-04 4.1). Aggression enters squared: readiness for war decides both how fast a people's grievances grow and, through drive, against whom (orcs 900: 810; goblins 600: 360; humans 400: 160; dwarves 300: 90; elves 150: 22; faeries 50: 2). Contact over a pass or a large river (cost 3) builds strain at a third of the rate over a walkable border: ranges and rivers keep peoples apart (design 5 "ranges are real barriers"). Pairs not in contact, at war or in truce keep their strain unchanged. A strain is reset to 0 for both directions of a pair when a war between them starts or ends (7.2, 7.7).

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

**Calibration against the engine's thresholds** (sparse 1500, normal 1000, dense 700; the 45% cap, P3-04 4.1): a goblin horde (aggression 600) against an elven realm across a walkable border (drive 200 + 700 = 900) builds 324 × 6 = 1,944 milli-units a quiet year: a claim after 155 quiet years and a flare-up after 515 at `normal` (360 at `dense`, 770 at `sparse`); an orc horde against a human kingdom (drive 450, rate 364) builds 9.1 a tick year: a claim and the first declarations within about 35 years of contact. With the shipped numbers a 1024 world has a median of 3 flare-ups (15.3); the simulated years stay within P3-00 4.1's median of 1000–1400 at `normal`.

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

Woods, wetlands and rough ground favour the defender: an army fighting into an elven forest or a dwarven upland meets twice its foe's strength. A pass held against an army doubles the defence (design 5 "passes are places to fortify and fight over"); lowland borders are political and give nothing (P3-00 7, decided).

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

For each open war in declaration order (ascending w), **except a war declared in this year** (its first battle is in the year after its declaration, so that no war opens and closes before one bookkeeping, P3-04 6.9 item 7; r2):

1. x, y, the target entry (R, cost, R_from) by 6.2; no front: go to 7.7.
2. A battle happens if `chance_pm(draw(hist.conflict.battle, Y, w, 0, 0), battle_permille)`; otherwise go to 7.7.
3. A = force(x) div wars(x), D = force(y) div wars(y), D' = D × def(R) div 1000; p = A × 1000 div (A + D') (500 when A + D' = 0); x wins if `chance_pm(draw(hist.conflict.battle_win, Y, w, 0, 0), p)`.
4. The winner W with effective force F_W, the loser L with F_L (A or D, unmultiplied): dead_L = F_L × `loss_loser` div 1000, dead_W = **min(F_W, F_L)** × `loss_winner` div 1000 (r7, P3-08; before r7, generator 0.16.0, F_W × `loss_winner` div 1000). The victors' losses scale with the smaller force, the size of the fight they actually had: a host three times its foe's size does not lose more men than the side it broke. With `loss_winner` ≤ `loss_loser` (shipped 80 and 250) the victors never lose more than the defeated, since min(F_W, F_L) ≤ F_L; at 0.16.0 they did in 3,934 of 9,450 battles of the default sweep (`docs/reviews/p3-06-conflict-world-critic.md` F8), and in the P3-08 prototype in none (P3-08 6).
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

`casualties(c, dead, e)`: ss = c's standing visible settlements ascending, T = Σ P over ss (if T = 0 or dead = 0: nothing); dead = min(dead, T) (never binding with the shipped tuning: force ≤ T × 250‰ × 1600‰ = 0.4 T, so dead ≤ 0.1 T; with other legal tuning, `levy_permille` up to 1000, `prowess_permille` up to 4000 and `loss_loser_permille` up to 1000, dead can reach 4 T and the cap binds: every settlement of the side can fall to 0 and stay standing until the next T1, r7, the P3-06 audit's A1); each s loses dead × P_s div T; the remainder goes from the most populous settlement after those losses (then smaller id), at most its population; `set_population` for each; `note_reduction(s, e)` for every s of ss (P3-05 13: the cause of a later abandonment). Status is not changed (a war is not a P3-05 pressure; P3-05's recovery rule needs no `before`). The war's dead of c += dead.

### 7.5 Falls: sacking and capture

A fallen settlement t of y, taken by x in region R:

1. **Sack or capture**: sack if `chance_pm(draw(hist.conflict.sack, Y, w, t, 0), min(1000, sack_base + h(x, y) × sack_hostility div 1000))`. Hatred sacks (goblins against elves, h = 700: 900‰), neighbours of one camp take (h = 0: 200‰).
2. **Sack**: dead = P_t × `sack_toll` div 1000; P_t −= dead; append `sacked` (region R, settlement t; participants x as `attacker`, y as `defender`; values `dead`, `survivors` = P_t; cause the siege; salience `capital_sacked_salience` if t is y's capital, else the base); then `PeoplesSubsystem::ruin(ctx, t, that event)` (amendment 18.1; P3-05 r3 adds the step context): the survivors join y's nearest other standing settlement up to its room and the rest are lost, t is ruined with `ruin_event` = the sack and no owner, y's hold on R is released if y has no other standing settlement there, and y's capital moves if t was it.
3. **Capture**: dead = P_t × `capture_toll` div 1000; P_t −= dead; excess = max(0, P_t − 2 × C(R, r_x)) (P3-00 12.3's bound for the new owner's race); append `settlement_captured` (region R, settlement t; participants x as `attacker`, y as `defender`; values `dead`, `fled` = excess; cause the siege; salience `capital_captured_salience` if t is y's capital, else the base); P_t −= excess and the excess joins y's nearest other standing settlement as in P3-05 8.5 item 2; `set_population`; `set_settlement_owner(t, x, e)`; if y holds R and has no other standing settlement in R, `set_region_owner(R, x, e)` (**conquest of the region**); if t was y's capital, P3-05 8.5 item 5's rule (most populous standing settlement, then smaller id; `capital_moved`, cause e).
4. The war's settlements taken by x += 1; grudge{x, y} += `capture_grudge`; the war's last event = e.
5. If y has no standing settlement left: 7.6 with cause e.

### 7.6 Conquest of a civilization

`conquer(y, x, e, R)`: append `civ_conquered` (region R; participants y as `civilization`, x as `conqueror`; cause e); `end_civilization(y, that event)` (it is y's `ending_event`, P3-07 20 item 3); every region y still holds is freed with it; every open relation with y as a side (in `RelationKey` order) is closed with it; every open war with y as a side is over (no peace event). y's people: those of captured settlements are now x's; the survivors of its last sack are lost (no settlement to flee to).

### 7.7 The end of a war and peace

After 7.3 for a war that is still open, in this order:

1. **Exhaustion**: the sides with dead × 1000 ≥ `exhaustion_permille` × max(1, pop0). If any: s = the war score (below); if both are exhausted, or s = 0, or the exhausted side is not the one s favours, the war ends with s's outcome; if the one exhausted side leads on score, the war ends in a truce (a winning side that has bled itself white stops, but does not lose).
2. **Aim achieved**: if the defender b neither holds the goal G nor has a standing settlement in it (it was sacked or taken, 7.5), the attacker wins. Wars are limited: the attacker fights for its goal and stops when it has it (6.2 makes the goal its target whenever it can reach it).
3. **No front** (6.2): ends with s's outcome.
4. **Length**: Y − start ≥ `war_max_years`: ends with s's outcome.

**War score**: (battles won by a − by b) + 2 × (settlements taken by a − by b): positive → a wins, negative → b wins, 0 → truce.

**Ending with winner W, loser L** (P3-07's excerpt shape): append `peace` (no region; participants W as `winner`, L as `loser`; value `years` = Y − start; cause the war's last event). **Ending in a truce**: append `truce` (participants both as `parties`, ascending id; value `years`; same cause). Then: close the `war` relation with that event; open the pair's `peace` relation with it; `last_peace{a, b}` = (Y, it); grudge{a, b} += `war_grudge`; strains of both directions 0; claims of both directions dropped.

**Cession**: if W is the attacker, L still holds the goal G, and G is not the region of L's capital: append `region_ceded` (region G; participants L as `from`, W as `to`; cause the peace); each standing visible settlement of L in G in ascending id (8.3 item 4; **from r13 every standing settlement of L in G, visible or not: 20.27**) gives its excess over 2 × C(G, r_W) to L's nearest other standing settlement (P3-05 8.5 item 2) and passes to W (`set_settlement_owner`, the cession event); `set_region_owner(G, W, cession)`. The peace terms are thus: every settlement taken stays taken, the attacker's goal is ceded when the attacker wins, and a truce of `truce_years` follows.

### 7.8 Quiet steps (Q4) and the end of an active phase

Quiet steps have no battles and no declarations (P3-00 4.4). P3-06's Q4, after P3-05's foundings: (1) housekeeping (8.1); (2) **every open war ends**, in declaration order, by the war score as in 7.7 (peace with cession, or truce), dated at the step's start (the year after the active phase): the fighting of a flare-up or an opening phase does not outlast it. An end that finds no quiet-event budget for its charged civilization (the winner, or the smaller id of a truce; for a cession also the loser) waits for the next step, the war still open and inactive (P3-04 W9 holds by construction, P3-05 10.5's rule); (3) grudge decay over L years; (4) strain over L years and claims (5.2, 5.3). Then the engine's Q5 asks the tension (5.4).

A war is therefore never carried across a quiet interval. A flare-up cut at an awakening (P3-04 E3) runs straight into the next opening phase, so its war goes on there.

## 8. Order of operations

### 8.1 Housekeeping (first in each of T2, T6, Q4)

1. Every open relation (in `RelationKey` order) with a side that has ended (by P3-05's decline, or earlier in this step) is closed with that side's `ending_event` (the smaller id's first if both); every open war with an ended side is over; claims involving an ended civilization are dropped.
2. Each alive visible civilization not yet in `seen`, ascending: added; if its founding event is a `civ_seceded` (`StepContext::event_kind`), the secession grudge of 4.2 with the parent read as the event's `from` participant (`StepContext::event_participant`, P3-04 r8; r5).
3. Each pair a < b of alive visible civilizations not in `enmity_opened` with camp(a, b) ≤ `enmity_max`: open `enmity` (a, b) with b's `founding_event` (b is the younger: ids follow founding order); add to `enmity_opened`.

### 8.2 The steps

| Step | P3-06 does (after P3-05's part of the same step) |
| --- | --- |
| T2 | housekeeping; grudge decay (1 year); strain (1 year, `tick_strain_permille`) and claims; declarations (7.2) |
| T6 | housekeeping; each open war: battle, siege, fall, conquest (7.3–7.6), end of war (7.7) |
| Q4 | housekeeping; end of every open war (7.8); grudge decay (L years); strain (L years, `quiet_strain_permille`) and claims |
| Q5 | tension (5.4), read-only |

P3-06 installs as the second subsystem (`EngineConfig::subsystems` = peoples, conflict) with steps T2, T6, Q4, and as the only tension source. It runs in no other step; it changes the world only through `State` and P3-05's hooks (`note_reduction`, `ruin`). Entities created in a step are not processed in it (P3-04 6.2): P3-05's colonies of this T2 do not count in `presence` (their region's ownership does), a civilization founded by secession in T4 is first seen in T6.

### 8.3 Implementation rules from the P3-04 audit (normative; r2)

P3-04 6.9's rules, as P3-06 meets them:

1. **A1, loops.** P3-06 founds no civilization or settlement. It closes relations in housekeeping (8.1 item 1) and in a conquest (7.6), and opens them in housekeeping (item 3), declarations and peace: every such pass first copies the relation keys (or the pairs) it will touch, then applies the changes; it never closes a relation inside a loop over `relations()`. Loops over settlements (casualties 7.4, cession 7.7, refugees) take the id list before changing populations or owners and re-fetch by id. Members hold ids only: wars are keyed by their declaration event id and name their sides, goal region and last event by id; strain, claims, grudges and `last_peace` are maps keyed by civilization ids.
2. **A2, tension.** **Strain is accumulated only in P3-06's subsystem steps** (T2 for a tick year, Q4 for a quiet step, 5.2), together with the claims and the contact of each pair; `tension()` (5.4) only reads the stored strain, claims and contact and returns the maximum. It changes no member. So the engine skipping `tension()` at the step that ends a quiet interval changes nothing; the prototype behaves the same way (its tension is a read of values set in Q4), so **no sweep number depends on it**. The first-year flare pair of 7.2 is recomputed in T2, a subsystem step, by the same read; `last_t2_phase` is set in T2.
3. **N1, order.** Shipped `EngineConfig::subsystems` = peoples, then conflict, fixed in `stages/history.cpp`; conflict is the only tension source. **T2 and Q4: peoples first, then conflict; T6: conflict only**; T1, T4, Q1, Q3: peoples only. `steps()` lists T2, T6 and Q4, each once.
4. **N2, what conflict sees of peoples' part of a shared step.** In T2 and Q4 the colonies (and in an awakening year nothing else; secessions are T4) that peoples founded earlier in the same step are invisible: they count in no presence, force, casualty, siege target or cession, and are never the settlement whose founding is a claim's cause (5.3's second branch); but when such a colony made b the holder of the claimed region, the region's `owner_event` is its founding event, and 5.3's first branch cites it (an event, not an entity read; r5: the two texts agree, and the reference, the C++ and the prototype all do this); their region's ownership, set by the founding, is live state and counts in `held` and so in `presence` and `wanted_held` (a region is state, not an entity). Populations (a source reduced by its colonists), `hunger` and `wanted_held` are read as peoples left them. Peoples sees nothing of conflict's part of the same step. **A same-step colony in a ceded region** (r7, the P3-06 audit's A2; **replaced from r13 by 20.27**: a cession passes every standing settlement of the loser in the ceded region, also one founded earlier in the same step, so the colony below becomes the winner's; the rest of this paragraph describes 0.16.0 to 0.18.0 before r13): in Q4 peoples may found a colony of a loser L inside the goal region G (P3-05 9.3, a founding inside L's own region) before conflict ends L's war there; the colony is invisible to conflict in that step, so a cession of G in the same Q4 (7.8) transfers only L's visible settlements of G and sets G's owner to the winner W, and the colony stays standing, owned by L, in a region held by W. This is the one way a region changes owner while its former owner keeps a standing settlement in it; invariant 4 of P3-05 and 5 of 14 still hold (W has a settlement in G), and peoples treats the colony like any settlement in a region held by another civilization (its share by its own race; foundings from it only outward, since 9.3's inside founding needs L to hold the source's region). It needs a Q4 founding by L inside G in the step L's war over G ends with a cession; it is as specified, and no rule is added to prevent it. The one exception to visibility is P3-05 8.5 item 2's: refugees (7.5, 7.7) may join a settlement created earlier in the step. (r13: a second exception, 20.27: every test of whether a civilization has a standing settlement in a region, made to decide who owns the region or which settlements pass with it, counts every standing settlement, created earlier in the step or not.)
5. **N3, draw keys.** `declare` (attacker, defender), `battle` and `battle_win` (the war's declaration event id, 0), `siege` and `sack` (declaration event id, besieged settlement). One battle per war per year, so (w, 0) is unique within a year; a settlement is besieged at most once per war per year. All ids (the declaration event id is stored in the world); no key is a position.
6. **N4.** `enmity` and `peace` are opened and closed through one helper that orders the pair (lower civilization id first); `war` is stored as (attacker, defender).
7. **N5, what one bookkeeping records** (P3-04 6.7 item 2, r9: every opening and closing in call order). P3-06 never opens one relation key twice before the same bookkeeping (which would be fatal): a war is not fought in its declaration year (7.3), so it cannot end then by peace, and its pair's `peace` row, closed by the declaration, is reopened at the earliest in a later year; a truce of `truce_years` ≥ 1 keeps a new war from closing a `peace` opened in the same year; `enmity` is opened once per pair, ever. A relation of an earlier year closed and reopened in one bookkeeping would give two rows; P3-06 does not do it. **Opened and closed in one step** (r5): three paths open a relation and close it within one bookkeeping: an `enmity` opened in a year's housekeeping whose older side is conquered in the same T6; a `peace` opened when one war ends in T6, a side then conquered by a later war of the same T6; a `war` declared in T2 whose side is conquered by another war in that year's T6. Each still gets its row, `since` = `until` = the year, with both events (P3-04 6.7 item 2, r8, r9); such a row is in force in no year under the half-open convention, and a `peace` closed by a declaration in year y does not overlap the `war` opened in y. The prototype wrote such rows too (it records opens and closes, not net state). They are rare: none at 1024 seeds 0–49 with default settings, none at `turbulent` or without refuge and shelter (seeds 0–19) or at 512 with four civilizations per race, and 5 rows in 5 of 20 worlds at 1024 with four civilizations per race (4 `war`, 1 `peace`); no `enmity` row of this kind was seen. The validation of 0.16.0 (375 worlds, `docs/reviews/p3-06-expansion-conflict-validation.md` 4.2, F7) found them also outside four civilizations per race: one `war` row in a `calm` world and one in a `span12000` world (seeds 0–19 each), besides the 4 `war` and 1 `peace` rows of `civs4`; each is a war declared in T2 whose defender another war conquered in the same T6 (r7). A region can change owner twice in one T6 (two wars' captures in declaration order, or a sack's release followed by nothing): the last change and its event are recorded, the events stay in the log. Peoples and conflict never set the same region's owner in one step (in T2 conflict sets none; in Q4 a cession concerns a held region, which peoples does not colonize). (r13: wrong for 20.3. In Q4 peoples may found a colony of L into a free region R, making L its holder, and a 20.3 cession of the same Q4 may then cede R to W: 20.27, the seed-3 case. The bookkeeping records the last owner and its event, P3-04 6.7 item 1 and 6.9 item 7, so `region_control` shows R passing from no owner to W with the `region_ceded`, and L holds R in no recorded row.)

### 8.4 What P3-06 respects in the peoples code (normative; from the P3-05 determinism audit, r4)

The audit of P3-05 (`docs/reviews/p3-05-peoples-determinism-audit.md` on `p3/05-audit`) lists what conflict must respect; each item with how P3-06's rules meet it:

1. **Order and visibility.** Conflict is installed after peoples; in T2 and Q4 peoples' same-step foundings are live state (their regions' owners) but invisible entities to conflict. Met: 8.3 items 3 and 4.
2. **`ruin`** needs a standing, owned settlement, may append `capital_moved` and does not end the civilization; in a quiet step its caller must check the budget for every event it causes. Met: P3-06 calls `ruin` only for a sack (7.5 item 2), only in T6, on a settlement that is standing and owned at the call; it never ruins in a quiet step. A later rule that ruins in Q4 must first check `quiet_events_left` for the `sacked`, the possible `capital_moved` and the possible `civ_conquered` of the charged civilizations.
3. **End what conflict empties.** A civilization left with no standing settlement and no ruin on record stops the next T1 (`history: peoples: civilization <c> has no standing settlement and none was ruined`). Met: 7.5 item 5 appends `civ_conquered` in the same T6 whenever a sack **or a capture** leaves the defender with no standing settlement (the capture path transfers the last settlement and ends the civilization at once; the prototype does the same). A cession never takes the last settlement (the loser's capital region is never ceded, 7.7). **With the shipped tuning** casualties never empty a settlement (7.4 takes at most 0.1 of a side's people). With other legal tuning (7.4, r7) they can leave settlements at 0 people while standing: such a settlement is a siege target with garrison 0 (a siege of it falls whenever the attacker has force left), and the next T1 grows it to 1 (P3-05 9.1's max(1, d)) and abandons it below `abandon_min`, with `civ_ended` if it was its civilization's last; deterministic, and every invariant holds (P3-05 14 item 3's floor is checked at the end of T1). A settlement casualties leave below `abandon_min` is abandoned by P3-05 in the next T1 with `civ_ended` as usual.
4. **Ruin only in T6, or end at once.** Met: sacks happen only in T6 (7.3–7.5); T2 and Q4 ruin nothing; a civilization emptied in T6 is ended in the same step (item 3), so the next year's T4 crisis never meets a ruined capital.
5. **Regions with settlements; `note_reduction`; `set_population`.** Met: a region changes owner only by a capture of the defender's last standing settlement there, a cession with its settlements, or `ruin`'s release (14 item 5); 7.4 calls `note_reduction` for every settlement of a side after casualties; every population change (casualties, sack and capture tolls, refugees, capture and cession excess) goes through `set_population`.
6. **Own streams, keys from ids and the year.** Met: 3.3 and 8.3 item 5.
7. **`share` and `wanted_held` once per civilization per step** (each scans every settlement or region). Met: P3-06 calls `wanted_held` once per alive civilization at the start of each strain update (5.3's snapshot) and never calls `share`; `population`, `regions` and `settlements` are called once per side per war event that needs them, and an implementation may cache them per step.
8. **Ids, not references, across steps** (lint rules `history-held-reference` and `history-live-iteration`). Met: 8.3 item 1.
9. **No `homes()`.** Met: P3-06 never calls it (homes are P3-05's awakening).

Checking P3-06 against items 3 and 4 found no gap: the capture path already ends the civilization in the same step, and no rule ruins outside T6. No behaviour or number changes.

## 9. The race characters (design 5; P3-00 13)

| Character | Rule and data that produce it | Measured (15) |
| --- | --- | --- |
| Goblins hostile to elves from their first year | camp table good–evil −700: an `enmity` row from the goblins' founding year (4.3); the fastest strain of any older pair (5.4); sacks over captures at h = 700 (7.5) | `enmity` from the goblins' founding in every world; goblin–elf wars 0 / 8 / 18 a world (15.3) |
| Dwarves arrive into a world already at war | dwarves are neutral (wary, −150/−250); their awakening opens an active phase in which the strain the goblins and elves built in the quiet centuries turns into declarations (declare_min below the flare threshold) | a war within 50 years of the awakening in 30 of 50 worlds (15.3; question 10) |
| Humans spread over land left open or ruined | sacks free regions (7.5 item 2) and P3-05's outward foundings resettle free land; humans reach 3 hops | 14 human settlements standing at the median against 11 without war (127 taken from others in 50 worlds); 5% of human settlements on ruined ground (question 9) |
| Orcs open the last age of conflict | aggression 900 (810 squared, the largest); evil against everyone but goblins; strong (prowess 1300); their age is active to the present (P3-04 E4) | 55% of all wars fall in the orc age, 3.3 a century against 0.19 before; orcs the attackers at 0.80 wars a century, the highest rate |
| Elves old and few, oldest in the present | prowess 1600 in forest cover; capitals walled; `last_refuge` | the first elven realm alive in 49 of 50 worlds; 12 elven settlements at the median against 20 without war |
| No race wiped out | `last_refuge` (7.3 item 7) and `shelter_years` (7.2) | no race extinct in any sweep; without them 17 of 20 worlds lose a race, the orcs in 15 (question 4) |

## 10. Anti-runaway and anti-stagnation

### 10.1 What stops one civilization from holding the map

1. **Reach and secession** (P3-05 9.3, 10.3): a civilization founds only within reach of its capital and large realms split in succession crises; conquests do not extend reach.
2. **Overextension**: a civilization's force is split over its open wars (6.1); a large realm is fought on several fronts.
3. **Coalition by fear** (5.1): above `fear_share` (400‰) of the land, every neighbour's drive against the holder rises, up to `fear_weight` at the whole map.
4. **Exhaustion and limited war** (7.7): wars end after `war_max_years` at most, by exhaustion long before; peace cedes at most the goal region.
5. **Defence**: walls, capitals, terrain and passes make the last cities of a people the hardest to take.

Measured: no civilization ever held more than 288‰ of the non-islet land at 1024 (median of the worlds' largest 176‰), 446‰ with four civilizations per race, 636‰ at 512, 113‰ at 2048; `share_over_500_years` is 0 on every swept 1024 and 2048 world, and at 512 (11–16 regions) at most 3,213 years above 500‰ and 469 above 600‰, so the 80% / 1,000-year bound (P3-00 12.3) holds with a wide margin (15.5).

### 10.2 What stops the world from freezing

Strain never decays: any two neighbours with drive > 0 (every pair below +200 disposition, every pair with want or fear) quarrel eventually; grudges decay, so old enemies of one camp make peace in the long run while good and evil stay enemies; P3-05's growth and foundings keep presence and hunger changing; the flare-up rule turns accumulated strain into an active phase. Measured: every 1024 world has wars (7 at least); 44 of 50 have at least one flare-up, and half of all wars fall in the last age.

## 11. Event kinds (appended to the catalogue after P3-05's 6–19)

Ids continue P3-05's; roles and values in `seq` order; entity types as P3-04 6.1 (`civilization` 3, `settlement` 4); every count 1–1 except `parties` 2–2. Every kind is **caused** (`caused` = 1). Base salience is data (`history.report`), shipped as listed; templates as listed (P3-04 8.4 grammar; `\|` is a table escape for `|`).

| id | key | region | settlement | roles | values | salience | template |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 20 | `claim` | 1 | 0 | 1 `claimant`, 2 `holder` | — | 45 | `{claimant} laid claim to {@region}, held by {holder}.` |
| 21 | `war_declared` | 1 | 0 | 1 `attacker`, 2 `defender` | — | 70 | `{attacker} declared war on {defender} over {@region}.` |
| 22 | `battle` | 1 | 0 | 1 `winner`, 2 `loser` | `winner_dead`, `loser_dead` | 30 | `{winner} defeated {loser} in {@region}; {loser_dead} of the defeated and {winner_dead} of the victors fell.` |
| 23 | `battle_at_pass` | 1 | 0 | 1 `winner`, 2 `loser` | `winner_dead`, `loser_dead`, `pass` | 35 | `{winner} defeated {loser} at the pass into {@region}; {loser_dead} of the defeated and {winner_dead} of the victors fell.` |
| 24 | `siege` | 1 | 1 | 1 `attacker`, 2 `defender` | `fell` | 25 | `{attacker} besieged {@settlement} of {defender}; it {fell?fell\|held}.` |
| 25 | `sacked` | 1 | 1 | 1 `attacker`, 2 `defender` | `dead`, `survivors` | 75 (90 a capital) | `{attacker} sacked {@settlement} of {defender}; {dead} died and {survivors} fled.` |
| 26 | `settlement_captured` | 1 | 1 | 1 `attacker`, 2 `defender` | `dead`, `fled` | 60 (80 a capital) | `{attacker} took {@settlement} from {defender}; {dead} died{fled?\| and one fled\| and # fled}.` (r7; before r7 `…; {dead} died and {fled} fled.`) |
| 27 | `peace` | 0 | 0 | 1 `winner`, 2 `loser` | `years` | 60 | `{loser} sued {winner} for peace after {years} {years?year\|years} of war.` |
| 28 | `truce` | 0 | 0 | 1 `parties` (2–2) | `years` | 50 | `{parties:all} made peace after {years} {years?year\|years} of war, neither the victor.` |
| 29 | `region_ceded` | 1 | 0 | 1 `from`, 2 `to` | — | 55 | `{from} ceded {@region} to {to}.` |
| 30 | `civ_conquered` | 1 | 0 | 1 `civilization`, 2 `conqueror` | — | 90 | `{civilization} fell to {conqueror}.` |

`settlement_use` 1 means the event's settlement column is the settlement concerned (not a role: P3-04 W7 checks it lies in the region). Kinds of P3-05 that P3-06 also emits, unchanged: `capital_moved` (17) after a capture (7.5 item 3) and, through `ruin`, after a sack. P3-06 never emits `civ_ended` (18): a civilization it ends gets `civ_conquered`, which names the conqueror (P3-05 13 allowed kind 18; a new kind is the only way to add the `conqueror` role, P3-04 8.2).

**The capture sentence** (r7, P3-08; `docs/reviews/p3-06-conflict-world-critic.md` F8): with P3-04 r10's three-word value form, a capture that drove no one out ends at the dead (illustrative, after 0.16.0's e318: `The Grork Horde took Tyfie Glade from the Teeshti Court; 135 died.`), one with refugees keeps the clause (`…; 135 died and 12 fled.`). 797 of 821 captures of the 0.16.0 default sweep had `fled` 0. A template is data (`game/data/history/report.toml` and the fixtures): no kind or schema change. The sack sentence is unchanged (a sack always leaves survivors with the shipped `sack_toll`).

**Cause rules** (every cause is an earlier event, W6):

| Kind | Causes |
| --- | --- |
| `claim` | the region's `owner_event` (the event by which the holder got it: its founding, colony, capture or cession), or the founding of the holder's settlement there |
| `war_declared` | the claim (the casus belli), and the pair's last `peace` or `truce` if any |
| `battle`, `battle_at_pass` | the war's `war_declared` |
| `siege` | the battle it followed |
| `sacked`, `settlement_captured` | the siege |
| `peace`, `truce` | the war's last event (its declaration, last battle, siege, sack or capture) |
| `region_ceded` | the peace |
| `civ_conquered` | the sack or capture of the last settlement |
| `capital_moved` (17) | the sack or capture of the old capital |
| `flare_up` (engine) | the claim reported by the tension (5.4) |

So a reader can walk every war back: conquest → sack → siege → battle → declaration → claim → the founding of the coveted settlement.

**Report requirements** (P3-07 20): `sacked` is the ruined row's `ruin_event` (75, a capital 90) and `civ_conquered` the ended row's `ending_event` (90), both ≥ 20 and above every other event of a civilization except a capital's sack, so they rank among its milestones (P3-07 7.3). The war relation kind is `war` (asymmetric); `enmity` and `peace` are symmetric (4.3). No kind needs a new report block or line (format v1).

**Saliences and the summary.** P3-07's summary shows at most 250 events, from the smallest salience that fits (7.4). With conflict a 1024 log grows to about 800 events and the threshold to 26–46, median 31 (15.3): shown are awakenings, foundings, capitals, claims (45), declarations (70), sacks and captures, peace and truces, cessions, conquests, secessions, endings, cities and town plagues (45), moved capitals (50) and crises (40); hidden are battles (30, 35 at a pass) and sieges (25), which the `full` level lists, colonies, and the peoples' smaller events. A war at `summary` reads: claim, flare-up, declaration, sacks or captures, peace, cession.

Rendered samples (seed 42, 1024, generator 0.16.0; illustrative from 0.17.0, r8: P1-06 r3 renames regions, P3-02 r5 renames some settlements, `Assill` among them, and P3-04 r10 writes `the Grork Horde` mid-sentence): `The Grork Horde laid claim to Southwest Wood, held by Finen.` · `The Grork Horde declared war on Finen over Southwest Wood.` · `Finen defeated The Grork Horde in Southwest Wood; 905 of the defeated and 111 of the victors fell.` · `The Grork Horde besieged Assill of Finen; it held.` · `Finen sacked Groz of The Grork Horde; 540 died and 1261 fled.` · `Finen sued The Grork Horde for peace after 11 years of war.` · `Finen ceded Southwest Wood to The Grork Horde.` · `The Grork Horde and The Hold of Dulimin made peace after 17 years of war, neither the victor.` · `The Grork Horde took Gob-Skreet from The Warrens of Dribnark; 132 died and 0 fled.` · `The Warrens of Dribnark fell to The Grork Horde.`

## 12. Tuning and race data

### 12.1 `history.conflict` (new id of kind `history`, P3-01 5)

All fields required. Status: **taste** (tuned on the sweep), **calibration** (scale), **decided** (by P3-00, Paul or the design), **bound** (a guard).

| Field | Type | Shipped | Status | Meaning |
| --- | --- | --- | --- | --- |
| `camp` | record {`good_good`, `good_neutral`, `good_evil`, `neutral_neutral`, `neutral_evil`, `evil_evil`} int(−1000, 1000) | 400, −150, −700, 100, −250, 300 | decided (P3-00 10.6) in sign, taste in size | starting disposition by camp pair (4.1) |
| `enmity_max` | int(−1000, 1000) | −500 | taste | camp disposition at or below which a pair gets an `enmity` row (4.3) |
| `border_friction` | int(0, 2000) | 200 | taste | drive of any contact (5.1) |
| `want_weight` | int(0, 2000) | 500 | taste | drive of land hunger against a holder of wanted land (5.1) |
| `hunger_full` | int(1, 100000) | 300 | taste | years of hunger at which `want` is full (5.1) |
| `fear_share`, `fear_weight` | int(0, 999), int(0, 2000) | 400, 1000 | taste | the coalition against a giant (5.1) |
| `tick_strain_permille`, `quiet_strain_permille` | int(0, 1000) | 25, 6 | calibration | strain per year in ticks and in quiet steps (5.2) |
| `strain_max` | int(1, 1000000) | 5000 | bound | cap of a strain, tension units (5.2) |
| `claim_strain` | int(1, 1000000) | 300 | taste | strain of a claim (5.3) |
| `declare_min`, `declare_span` | int(0, 1000000), int(1, 1000000) | 300, 600 | taste | strain at which declarations start and become certain-rate (7.2) |
| `declare_permille` | int(0, 1000) | 150 | taste | yearly chance at full strain and odds (7.2) |
| `declare_ratio` | int(1, 10000) | 1000 | taste | force ratio of full odds before aggression (7.2) |
| `shelter_years` | int(0, 1000) | 150 | taste | years after its founding during which a civilization is not attacked (7.2) |
| `march_hops` | int(1, 4) | 2 | taste | borders an army crosses to reach a foe (5.1, 6.2) |
| `cost_penalty` | int(0, 10000) | 100 | taste | crossing cost against land value in choosing a target (5.3, 6.2) |
| `levy_permille` | record {hamlet, village, town, city, fortress} int(0, 1000) | 100, 100, 100, 80, 250 | calibration | people under arms (6.1) |
| `terrain_defense`, `pass_defense`, `river_defense`, `fort_defense` | int(0, 10000) | 1000, 1000, 500, 500 | taste | defence (6.3) |
| `battle_permille` | int(0, 1000) | 300 | taste | yearly chance of a battle in a war (7.3) |
| `loss_loser_permille`, `loss_winner_permille` | int(0, 1000) | 250, 80 | taste | casualties of a battle (7.3) |
| `garrison_permille` | int(0, 1000) | 300 | taste | defenders of a settlement per mille of its people (7.3) |
| `walls_permille` | record {hamlet, village, town, city, fortress} int(1, 100000) | 1000, 1500, 2500, 4000, 6000 | taste | walls by kind (7.3) |
| `capital_walls` | int(0, 100000) | 2000 | taste | added walls of a capital (7.3) |
| `sack_base`, `sack_hostility` | int(0, 1000), int(0, 2000) | 200, 1000 | taste | chance of a sack over a capture (7.5) |
| `sack_toll_permille`, `capture_toll_permille` | int(0, 1000) | 300, 100 | taste | killed in a sack, in a capture (7.5) |
| `exhaustion_permille` | int(1, 1000) | 80 | taste | dead of a side per mille of its people that ends a war (7.7) |
| `war_max_years` | int(1, 1000) | 40 | taste | longest war (7.7) |
| `truce_years` | int(0, 10000) | 200 | taste | years after a peace without strain or war (5.2, 7.2) |
| `war_grudge`, `capture_grudge`, `secession_grudge` | int(0, 2000) | 150, 50, 300 | taste | grudges (4.2) |
| `grudge_decay_per_century` | int(0, 10000) | 50 | taste | grudge forgotten per century (4.2) |
| `last_refuge` | int(0, 1) | 1 | decided (9; question 4) | the last settlement of a race's last civilization never falls (7.3) |
| `capital_sacked_salience`, `capital_captured_salience` | int(0, 100) | 90, 80 | taste | salience of the fall of a capital (7.5) |

Named checks (per definition, in this order; messages as given):

| Check | Condition | Message |
| --- | --- | --- |
| `conflict_declare` | `claim_strain` ≤ `declare_min` | `claim_strain must not be above declare_min` (a declaration needs a claim, 7.2) |
| `conflict_strain_max` | `declare_min` + `declare_span` ≤ `strain_max` | `declare_min + declare_span must not be above strain_max` |

```toml
# game/data/history/conflict.toml and the fixtures under worldgen/tests/data/geo*/history/
[history.conflict]
camp = { good_good = 400, good_neutral = -150, good_evil = -700, neutral_neutral = 100, neutral_evil = -250, evil_evil = 300 }
enmity_max = -500
border_friction = 200
want_weight = 500
hunger_full = 300
fear_share = 400
fear_weight = 1000
tick_strain_permille = 25
quiet_strain_permille = 6
strain_max = 5000
claim_strain = 300
declare_min = 300
declare_span = 600
declare_permille = 150
declare_ratio = 1000
shelter_years = 150
march_hops = 2
cost_penalty = 100
levy_permille = { hamlet = 100, village = 100, town = 100, city = 80, fortress = 250 }
terrain_defense = 1000
pass_defense = 1000
river_defense = 500
fort_defense = 500
battle_permille = 300
loss_loser_permille = 250
loss_winner_permille = 80
garrison_permille = 300
walls_permille = { hamlet = 1000, village = 1500, town = 2500, city = 4000, fortress = 6000 }
capital_walls = 2000
sack_base = 200
sack_hostility = 1000
sack_toll_permille = 300
capture_toll_permille = 100
exhaustion_permille = 80
war_max_years = 40
truce_years = 200
war_grudge = 150
capture_grudge = 50
secession_grudge = 300
grudge_decay_per_century = 50
last_refuge = 1
capital_sacked_salience = 90
capital_captured_salience = 80
```

With the new id, every data root carries `chronology.toml`, `conflict.toml`, `engine.toml`, `peoples.toml` and `report.toml` (P3-01 5's rule); P3-01's unknown-id test expects `'chronology', 'conflict', 'engine', 'peoples', 'report'` (amendment 18.3). The field names in sections 5–7 use the short forms `loss_loser` = `loss_loser_permille`, `sack_toll` = `sack_toll_permille`, `capture_toll` = `capture_toll_permille`, `walls_permille`, `levy_permille`.

### 12.2 Race data (a data change in this merge)

1. **New field** `prowess_permille` int(1, 4000), required, after `aggression` (P3-01 amendment 18.3): elf 1600, goblin 800, dwarf 1400, faerie 1200, human 1000, orc 1300 (6.1). `aggression` is readiness for war; nothing in the race data says how well a people fights, and without it the few elves (density 300) fall to the numerous goblins (900): with every race at 1000 (1024, seeds 0–19), 4 of 36 elven civilizations were conquered against 1 with the shipped values, and the elves kept 10.5 standing settlements at the median against 11.5. Elves and dwarves are old, long-lived and skilled; faeries hidden in wild places; orcs "strong, warlike" (design 5); goblins numerous rather than strong.
2. **Goblins**: `aggression` 700 → **600**. The goblins are numerous, the second people, and split into several hordes by P3-05's secessions, so they meet everyone; at 700 (1024, seeds 0–19, everything else shipped) they started 80% of all wars, the median world had four flare-ups (1,350 simulated years) and the most warlike world 59 wars; at 600, 77%, three flare-ups (1,302 years) and 43 wars, with the orcs' share up from 12% to 14%. With squared aggression (5.2) the orcs' strain then grows 2.25 times as fast as the goblins' (1.65 times at 700), which keeps the design's order: goblins the elves' first and lasting enemy, orcs the most warlike.
3. Unchanged: every other field and race.

These change every world's `content_hash` (P1-00 6.4); P3-01's content-hash lines are re-recorded with this merge by their reference.

## 13. The seam with peoples (P3-05 13) and the subsystem's interface

```cpp
namespace worldgen::history {
class ConflictSubsystem final : public Subsystem, public TensionSource {
public:
    ConflictSubsystem(PeoplesSubsystem& peoples, const data::ContentSet&);   // reads history.conflict, history.peoples, races; holds no State between steps (P3-05 r3)
    std::span<const Step> steps() const override;                          // T2, T6, Q4
    void run(StepContext&) override;
    TensionReport tension(const StepContext&) override;                     // 5.4, const in effect
    // test seams (not used by run_history): the pure functions of 5.1, 6.1, 6.3, 7.3, 7.4 on a hand-made State
};
}
```

What P3-06 uses of P3-05: `PeoplesView` (`score`, `capacity`, `neighbours`, `hunger`, and the state readers `population`, `regions`, `settlements`, `wanted_held`, which take the step's `StepContext`, P3-05 r3), `note_reduction` (7.4), and the new `ruin` (amendment 18.1, 7.5). What it changes through `State`: populations (casualties, sacks, captures, refugees), settlement owners (captures, cessions), statuses (through `ruin`), region owners (conquest, cession, release through `ruin`), capitals (`capital_moved` after a capture), civilization ends (`civ_conquered`), relations. Everything P3-05's 20 forbids stays untouched: its kinds, its members, foundings into free land, the step order.

**P3-05's rules see P3-06's changes from the next step** (P3-05 13): a captured town grows toward its new owner's share and may dwindle to the new owner's ladder (a goblin-held elven city is capped at a goblin town's size); a sacked region is free land for P3-05's outward foundings of any people within reach, which is how "land left open or ruined" is resettled; a settlement left below `abandon_min` by casualties is abandoned in the next T1 with the battle as its cause.

## 14. Invariants

For every seed, setting, data set within the schemas and map size, with P3-05's invariants (as amended in 18.1):

1. **Relations**: every `relation` row has `a_type` = `b_type` = 3 and kind `enmity`, `war` or `peace`; `enmity` and `peace` rows have a_id < b_id, `war` rows a = the attacker; at most one `enmity` row per pair; a pair's `war` rows do not overlap, nor its `peace` rows, and a `war` and a `peace` row of one pair overlap in no year (the peace closes at the declaration year, the war at the peace year: P3-04 10's half-open intervals); every row's `since_event` is of the kinds of 4.3, every closed row's `until_event` too; an `enmity` row's `since` is the younger side's `founded_year`.
2. **Wars**: every `war_declared` has among its causes a `claim` of its attacker on its defender, dated earlier or the same year; every war ends by exactly one of: a `peace` or `truce` caused by its last event, the `civ_conquered` or `civ_ended` of a side, or the end of history (open row); no `battle`, `siege`, `sacked` or `settlement_captured` event lies in a quiet phase; every `peace`, `truce`, `region_ceded` in a quiet phase is dated at the start of a quiet step (the first one unless the quiet-event budget deferred it).
3. **Falls**: every `sacked` and `settlement_captured` is caused by a `siege` with `fell` 1 of the same settlement and year, which is caused by a `battle` or `battle_at_pass` of the same year won by the siege's attacker; every ruined settlement's `ruin_event` is a `settlement_abandoned`, a `settlement_abandoned_to` (P3-05 r6, kind 31, from generator 0.17.0) or a `sacked`.
4. **Ends**: every `civ_conquered` ends its civilization (`ending_event`), is caused by a fall of that civilization's last standing settlement, and names a conqueror alive at that year; an ended civilization has no open relation and holds no region.
5. **Territory**: a region's owner has a standing settlement in it (P3-05 invariant 4, now without its exception); every change of a region's owner by P3-06 is a `settlement_captured` or a `region_ceded`.
6. **Capacity**: every standing settlement has P ≤ 2 × C(R, owner race) after every step (P3-00 12.3).
7. **Races**: with `last_refuge` 1, every present race has an alive civilization at the present.
8. **Tension**: every flare-up's trigger is a `claim`; the claim's pair had strain ≥ the threshold at that quiet step (the reference recomputes it).
9. **Draw independence** (P3-00 Y3): a test-only extra draw on any `hist.conflict.*` stream before a decision changes nothing else.

## 15. Measurements (prototype)

### 15.1 Method

The prototype runs P3-05's prototype unchanged with P3-06 installed in the steps of 8.2 and a dynamic scheduler (P3-04 SC1–SC4 for the base schedule, SC5–SC6 with this spec's tension for quiet rows and flare-ups). It emulates the engine's bookkeeping only as far as the statistics need (region holders by step for the shares of P3-07 7.5; relation rows). Sweeps: 1024 seeds 0–49 (the reference), 512 seeds 0–19, 2048 seeds 0–4, and 1024 seeds 0–49 with each setting of 15.3's table; three counterfactual sweeps for the decisions of 12.2 and 9.

### 15.2 Sample: seed 42, 1024, default settings

**Illustrative from 0.17.0 (r8).** The sample is of generator 0.16.0; P3-08 changes its region names, some settlement and civilization names, its sentences and its history. The golden case `hist-conflict-42-1024` (16.3 item 20) is re-recorded from the reference at 0.17.0 and is not compared with this text.

Ages as P3-04 17 (2074 goblins, 3242 dwarves, 4243 faeries, 4969 humans, 5596 orcs). 1,030 events (P3-05's world had 530) over 1,404 simulated years (0.73 per simulated year): five flare-ups, 2949–2998 (trigger e75, the Grork Horde's claim on Southwest Wood, strain 1025), 3817–3866 (e150, the Horde's claim on South Wood II of the Hold of Dulimin), 4818–4867 (e251, the Horde's claim on East Wood III of the Teeshti Court), 5294–5343 (e430, the Horde's claim on West Wood II of Finen) and 5519–5568 (e473, the Zak-Neez Mob's claim on South Wood II). Summary threshold 46, 155 events shown (the claims, at 45, are below it in this world; every declaration cites its claim by id).

**Civilizations and their fates** (at year 6000; 13 civilizations, two conquered):

| Civilization | Race, founded | Fate |
| --- | --- | --- |
| c1 Finen | elves, 0 | alive: 1 region, 1 settlement, 1,544 people (11 settlements and 11,624 without conflict); the Grork Horde's first and lasting enemy, attacked 13 times; its capital Voeledell sacked in 4284; ceded Southwest Wood (3846) and its home West Wood III (4287); sacked the goblin town Groz in 3297 and threw back the orcs in 5668 |
| c2 The Realm of Ithieglie | elves, 0 | alive: 1 region (Southeast Isle), 4 settlements, 1,411; never at war (an island) |
| c3 The Grork Horde | goblins, 2074 | alive: 7 regions, 23 settlements, 53,716; the attacker in 23 wars; conquered two of the four hordes that broke away from it |
| c4 The Hold of Dulimin | dwarves, 3242 | alive: South Wood II, 4 settlements, 14,500; attacked ten times by four goblin hordes, never lost its hold, and took and sacked goblin towns |
| c5 The Narinthri Throne | dwarves, 3242 | alive: Northwest Peaks, 4 settlements, 6,267; never at war (behind the peaks) |
| c6 The Teeshti Court | faeries, 4243 | alive: 1 region, 2 settlements, 1,615; attacked six times, ceded East Wood III in 5761 |
| c7 The Warrens of Dribnark | goblins, 4392 (from c3) | **ended 4858, conquered by the Grork Horde** after losing its towns to the Hold of Dulimin and then the Horde |
| c8 The Kingdom of Stard | humans, 4969 | alive: 10 regions, 21 settlements, 132,342; repelled one goblin war and lost Northeast Downs V to the Grork Horde in another |
| c9 The Duchy of Dant | humans, 4969 | alive: 2 regions, 7 settlements, 18,955; held off the orcs once, lost to them once |
| c10 The Zak-Neez Mob | goblins, 5097 (from c3) | **ended 5656, conquered by the Grork Horde** |
| c11 The Kritch-Gux Horde | goblins, 5536 (from c3) | alive: 4 regions, 10 settlements, 11,350 |
| c12 The Daald Horde | orcs, 5596 | alive: 1 region, 4 settlements, 8,920; four wars in its first 350 years, on the elves and the human duchy |
| c13 The Warrens of Kug-Nuk | goblins, 5637 (from c3) | alive: no region, 1 settlement, 4,535 (its town stands in a region another holds) |

**Every war** (declaration event, years, attacker on defender, goal, casus belli, outcome):

- e43, 2220–2224: The Grork Horde (c3) on Finen (c1) over Southwest Wood, claim e38 (2148); 0 battles, 0 sacked, 0 taken; end of the active phase, no victor.
- e94, 2949–2956: The Grork Horde (c3) on Finen (c1) over Southwest Wood, claim e75 (2574); 1 battles, 1 sacked, 0 taken; aim taken, won by The Grork Horde.
- e134, 3286–3311: The Grork Horde (c3) on Finen (c1) over Southwest Wood, claim e127 (3259); 5 battles, 1 sacked, 0 taken; exhaustion, no victor.
- e179, 3817–3834: The Grork Horde (c3) on The Hold of Dulimin (c4) over South Wood II, claim e150 (3316); 4 battles, 0 sacked, 0 taken; exhaustion, no victor.
- e196, 3835–3846: The Grork Horde (c3) on Finen (c1) over Southwest Wood, claim e174 (3642); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Grork Horde, ceded.
- e233, 4274–4287: The Grork Horde (c3) on Finen (c1) over West Wood III, claim e219 (4192); 3 battles, 1 sacked, 0 taken; exhaustion, won by The Grork Horde, ceded.
- e252, 4318–4328: The Grork Horde (c3) on The Hold of Dulimin (c4) over South Wood II, claim e227 (4266); 4 battles, 0 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e312, 4818–4842: The Grork Horde (c3) on The Teeshti Court (c6) over East Wood III, claim e251 (4317); 9 battles, 0 sacked, 1 taken; exhaustion, won by The Grork Horde.
- e323, 4826–4852: The Warrens of Dribnark (c7) on The Hold of Dulimin (c4) over South Wood II, claim e299 (4593); 6 battles, 1 sacked, 1 taken; exhaustion, won by The Hold of Dulimin.
- e356, 4844–4849: The Grork Horde (c3) on Finen (c1) over West Wood II, claim e300 (4618); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e371, 4853–4858: The Grork Horde (c3) on The Warrens of Dribnark (c7) over South Wood, claim e302 (4643); 2 battles, 0 sacked, 2 taken; conquest of the defender, won by The Grork Horde.
- e405, 4982–4988: The Grork Horde (c3) on The Hold of Dulimin (c4) over South Wood II, claim e320 (4823); 4 battles, 0 sacked, 0 taken; exhaustion, no victor.
- e443, 5101–5109: The Grork Horde (c3) on The Teeshti Court (c6) over East Wood III, claim e424 (5073); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e475, 5294–5302: The Grork Horde (c3) on Finen (c1) over West Wood II, claim e430 (5080); 3 battles, 1 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e512, 5341–5344: The Grork Horde (c3) on The Zak-Neez Mob (c10) over South Wood, claim e467 (5219); 1 battles, 0 sacked, 1 taken; end of the active phase, won by The Grork Horde.
- e548, 5519–5543: The Zak-Neez Mob (c10) on The Hold of Dulimin (c4) over South Wood II, claim e473 (5269); 5 battles, 0 sacked, 1 taken; exhaustion, won by The Hold of Dulimin.
- e551, 5523–5531: The Grork Horde (c3) on The Teeshti Court (c6) over East Wood III, claim e511 (5338); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e585, 5548–5558: The Zak-Neez Mob (c10) on The Teeshti Court (c6) over East Wood III, claim e491 (5305); 5 battles, 0 sacked, 0 taken; exhaustion, won by The Teeshti Court.
- e613, 5567–5569: The Grork Horde (c3) on The Hold of Dulimin (c4) over South Wood, claim e506 (5333); 1 battles, 1 sacked, 0 taken; end of the active phase, won by The Grork Horde.
- e643, 5600–5605: The Kritch-Gux Horde (c11) on Finen (c1) over West Wood II, claim e634 (5594); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Kritch-Gux Horde.
- e656, 5618–5621: The Grork Horde (c3) on Finen (c1) over West Wood II, claim e606 (5562); 3 battles, 1 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e673, 5628–5656: The Grork Horde (c3) on The Zak-Neez Mob (c10) over South Wood, claim e652 (5610); 4 battles, 0 sacked, 1 taken; conquest of the defender, won by The Grork Horde.
- e702, 5655–5668: The Daald Horde (c12) on Finen (c1) over West Wood II, claim e671 (5628); 3 battles, 0 sacked, 0 taken; exhaustion, won by Finen.
- e723, 5670–5678: The Kritch-Gux Horde (c11) on The Hold of Dulimin (c4) over South Wood II, claim e686 (5645); 5 battles, 1 sacked, 1 taken; exhaustion, won by The Hold of Dulimin.
- e724, 5672–5684: The Daald Horde (c12) on The Duchy of Dant (c9) over Northwest Downs II, claim e672 (5628); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Duchy of Dant.
- e747, 5686–5695: The Grork Horde (c3) on The Kingdom of Stard (c8) over Northeast Downs V, claim e685 (5645); 3 battles, 1 sacked, 0 taken; aim taken, won by The Grork Horde.
- e773, 5744–5761: The Warrens of Kug-Nuk (c13) on The Teeshti Court (c6) over East Wood III, claim e759 (5709); 4 battles, 2 sacked, 0 taken; exhaustion, won by The Warrens of Kug-Nuk, ceded.
- e795, 5764–5773: The Grork Horde (c3) on The Teeshti Court (c6) over East Wood V, claim e785 (5759); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e796, 5764–5791: The Kritch-Gux Horde (c11) on The Grork Horde (c3) over West Downs II, claim e763 (5717); 5 battles, 0 sacked, 1 taken; exhaustion, won by The Grork Horde.
- e800, 5766–5772: The Warrens of Kug-Nuk (c13) on The Kingdom of Stard (c8) over North Downs III, claim e761 (5711); 5 battles, 3 sacked, 1 taken; exhaustion, won by The Kingdom of Stard.
- e827, 5776–5778: The Warrens of Kug-Nuk (c13) on The Hold of Dulimin (c4) over South Wood, claim e762 (5712); 1 battles, 0 sacked, 1 taken; aim taken, won by The Warrens of Kug-Nuk.
- e856, 5798–5820: The Grork Horde (c3) on The Warrens of Kug-Nuk (c13) over South Downs, claim e772 (5737); 6 battles, 2 sacked, 2 taken; aim taken, won by The Grork Horde.
- e885, 5826–5838: The Grork Horde (c3) on The Hold of Dulimin (c4) over South Wood II, claim e883 (5821); 4 battles, 0 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e911, 5844–5849: The Kritch-Gux Horde (c11) on Finen (c1) over West Wood II, claim e901 (5836); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Kritch-Gux Horde.
- e928, 5872–5876: The Grork Horde (c3) on Finen (c1) over West Wood II, claim e914 (5847); 3 battles, 3 sacked, 0 taken; exhaustion, won by The Grork Horde.
- e975, 5928–5939: The Daald Horde (c12) on Finen (c1) over West Wood II, claim e953 (5896); 3 battles, 0 sacked, 0 taken; exhaustion, won by The Daald Horde.
- e978, 5935–5936: The Kritch-Gux Horde (c11) on The Hold of Dulimin (c4) over Southwest Wood II, claim e966 (5912); 1 battles, 1 sacked, 0 taken; aim taken, won by The Kritch-Gux Horde.
- e987, 5940–5955: The Daald Horde (c12) on The Duchy of Dant (c9) over Northwest Downs II, claim e964 (5906); 4 battles, 1 sacked, 0 taken; exhaustion, won by The Daald Horde.

**Sixty lines of the summary chronicle** (P3-07 6.7's line format; from the goblins' first civilization; the war of the Horde against the Warrens of Dribnark runs whole in it, from the declaration e371 to the conquest e379, and the war e196 from its declaration to the peace e202 and the cession e203):

```
2074 e35 civ_founded/80: The Goblins founded The Grork Horde in Southwest Steppe. {@rg36 civilization=c3 race=ra2} [causes e34]
2220 e43 war_declared/70: The Grork Horde declared war on Finen over Southwest Wood. {@rg35 attacker=c3 defender=c1} [causes e38]
2224 e44 truce/50: Finen and The Grork Horde made peace after 4 years of war, neither the victor. {parties=c1 parties=c3 years=4} [causes e43]
2949 e93 flare_up/50: In 2949 tensions flared into 50 years of upheaval. {years=50} [causes e75]
2949 e94 war_declared/70: The Grork Horde declared war on Finen over Southwest Wood. {@rg35 attacker=c3 defender=c1} [causes e44, e75]
2956 e97 sacked/75: The Grork Horde sacked Glathenddell of Finen; 1160 died and 2707 fled. {@rg35 @s9 attacker=c3 defender=c1 dead=1160 survivors=2707} [causes e96]
2956 e98 peace/60: Finen sued The Grork Horde for peace after 7 years of war. {winner=c3 loser=c1 years=7} [causes e97]
3242 e120 awakening/100: Bronhak awakened the Dwarves in South Wood II. {@rg44 race=ra3 creator=g3}
3242 e121 civ_founded/80: The Dwarves founded The Hold of Dulimin in South Wood II. {@rg44 civilization=c4 race=ra3} [causes e120]
3242 e123 civ_founded/80: The Dwarves founded The Narinthri Throne in Northwest Peaks. {@rg15 civilization=c5 race=ra3} [causes e120]
3286 e134 war_declared/70: The Grork Horde declared war on Finen over Southwest Wood. {@rg35 attacker=c3 defender=c1} [causes e98, e127]
3297 e143 sacked/75: Finen sacked Groz of The Grork Horde; 540 died and 1261 fled. {@rg31 @s18 attacker=c1 defender=c3 dead=540 survivors=1261} [causes e142]
3311 e147 truce/50: Finen and The Grork Horde made peace after 25 years of war, neither the victor. {parties=c1 parties=c3 years=25} [causes e146]
3817 e178 flare_up/50: In 3817 tensions flared into 50 years of upheaval. {years=50} [causes e150]
3817 e179 war_declared/70: The Grork Horde declared war on The Hold of Dulimin over South Wood II. {@rg44 attacker=c3 defender=c4} [causes e150]
3834 e195 truce/50: The Grork Horde and The Hold of Dulimin made peace after 17 years of war, neither the victor. {parties=c3 parties=c4 years=17} [causes e194]
3835 e196 war_declared/70: The Grork Horde declared war on Finen over Southwest Wood. {@rg35 attacker=c3 defender=c1} [causes e147, e174]
3846 e202 peace/60: Finen sued The Grork Horde for peace after 11 years of war. {winner=c3 loser=c1 years=11} [causes e201]
3846 e203 region_ceded/55: Finen ceded Southwest Wood to The Grork Horde. {@rg35 from=c1 to=c3} [causes e202]
4243 e222 awakening/100: Syfiash awakened the Faeries in East Wood III. {@rg26 race=ra4 creator=g4}
4243 e223 civ_founded/80: The Faeries founded The Teeshti Court in East Wood III. {@rg26 civilization=c6 race=ra4} [causes e222]
4274 e233 war_declared/70: The Grork Horde declared war on Finen over West Wood III. {@rg27 attacker=c3 defender=c1} [causes e202, e219]
4284 e240 sacked/90: The Grork Horde sacked Voeledell of Finen; 407 died and 950 fled. {@rg27 @s1 attacker=c3 defender=c1 dead=407 survivors=950} [causes e239]
4284 e241 capital_moved/50: Finen moved its seat to Rannve. {@rg22 @s10 civilization=c1 settlement=s10} [causes e240]
4287 e245 peace/60: Finen sued The Grork Horde for peace after 13 years of war. {winner=c3 loser=c1 years=13} [causes e244]
4287 e246 region_ceded/55: Finen ceded West Wood III to The Grork Horde. {@rg27 from=c1 to=c3} [causes e245]
4318 e252 war_declared/70: The Grork Horde declared war on The Hold of Dulimin over South Wood II. {@rg44 attacker=c3 defender=c4} [causes e195, e227]
4328 e264 peace/60: The Hold of Dulimin sued The Grork Horde for peace after 10 years of war. {winner=c3 loser=c4 years=10} [causes e263]
4392 e283 civ_seceded/70: The Warrens of Dribnark broke away from The Grork Horde, taking 4 settlements with it. {@rg42 @s23 civilization=c7 from=c3 settlements=4} [causes e282]
4818 e311 flare_up/50: In 4818 tensions flared into 50 years of upheaval. {years=50} [causes e251]
4818 e312 war_declared/70: The Grork Horde declared war on The Teeshti Court over East Wood III. {@rg26 attacker=c3 defender=c6} [causes e251]
4822 e318 settlement_captured/80: The Grork Horde took Tyfie Glade from The Teeshti Court; 135 died and 0 fled. {@rg26 @s43 attacker=c3 defender=c6 dead=135 fled=0} [causes e317]
4822 e319 capital_moved/50: The Teeshti Court moved its seat to Fleefieli. {@rg26 @s53 civilization=c6 settlement=s53} [causes e318]
4826 e323 war_declared/70: The Warrens of Dribnark declared war on The Hold of Dulimin over South Wood II. {@rg44 attacker=c7 defender=c4} [causes e299]
4837 e342 settlement_captured/80: The Hold of Dulimin took Sneezskrub Hole from The Warrens of Dribnark; 407 died and 0 fled. {@rg42 @s23 attacker=c4 defender=c7 dead=407 fled=0} [causes e341]
4837 e343 capital_moved/50: The Warrens of Dribnark moved its seat to Gruz-Keegg. {@rg42 @s17 civilization=c7 settlement=s17} [causes e342]
4842 e353 peace/60: The Teeshti Court sued The Grork Horde for peace after 24 years of war. {winner=c3 loser=c6 years=24} [causes e352]
4844 e356 war_declared/70: The Grork Horde declared war on Finen over West Wood II. {@rg22 attacker=c3 defender=c1} [causes e245, e300]
4849 e364 peace/60: Finen sued The Grork Horde for peace after 5 years of war. {winner=c3 loser=c1 years=5} [causes e363]
4852 e367 sacked/90: The Hold of Dulimin sacked Gruz-Keegg of The Warrens of Dribnark; 507 died and 1185 fled. {@rg42 @s17 attacker=c4 defender=c7 dead=507 survivors=1185} [causes e366]
4852 e368 capital_moved/50: The Warrens of Dribnark moved its seat to Grak-Kutch. {@rg42 @s24 civilization=c7 settlement=s24} [causes e367]
4852 e369 peace/60: The Warrens of Dribnark sued The Hold of Dulimin for peace after 26 years of war. {winner=c4 loser=c7 years=26} [causes e367]
4853 e371 war_declared/70: The Grork Horde declared war on The Warrens of Dribnark over South Wood. {@rg42 attacker=c3 defender=c7} [causes e302]
4854 e374 settlement_captured/80: The Grork Horde took Grak-Kutch from The Warrens of Dribnark; 231 died and 0 fled. {@rg42 @s24 attacker=c3 defender=c7 dead=231 fled=0} [causes e373]
4854 e375 capital_moved/50: The Warrens of Dribnark moved its seat to Gob-Skreet. {@rg42 @s25 civilization=c7 settlement=s25} [causes e374]
4858 e378 settlement_captured/80: The Grork Horde took Gob-Skreet from The Warrens of Dribnark; 132 died and 0 fled. {@rg42 @s25 attacker=c3 defender=c7 dead=132 fled=0} [causes e377]
4858 e379 civ_conquered/90: The Warrens of Dribnark fell to The Grork Horde. {@rg42 civilization=c7 conqueror=c3} [causes e378]
4969 e392 awakening/100: Wealstuld awakened the Humans in Northeast Downs II. {@rg10 race=ra5 creator=g5}
4969 e393 civ_founded/80: The Humans founded The Kingdom of Stard in Northeast Downs II. {@rg10 civilization=c8 race=ra5} [causes e392]
4969 e395 civ_founded/80: The Humans founded The Duchy of Dant in Northwest Downs II. {@rg4 civilization=c9 race=ra5} [causes e392]
4982 e405 war_declared/70: The Grork Horde declared war on The Hold of Dulimin over South Wood II. {@rg44 attacker=c3 defender=c4} [causes e264, e320]
4988 e411 truce/50: The Grork Horde and The Hold of Dulimin made peace after 6 years of war, neither the victor. {parties=c3 parties=c4 years=6} [causes e410]
5097 e436 civ_seceded/70: The Zak-Neez Mob broke away from The Grork Horde, taking 2 settlements with it. {@rg42 @s24 civilization=c10 from=c3 settlements=2} [causes e435]
5101 e443 war_declared/70: The Grork Horde declared war on The Teeshti Court over East Wood III. {@rg26 attacker=c3 defender=c6} [causes e353, e424]
5109 e452 peace/60: The Teeshti Court sued The Grork Horde for peace after 8 years of war. {winner=c3 loser=c6 years=8} [causes e451]
5294 e474 flare_up/50: In 5294 tensions flared into 50 years of upheaval. {years=50} [causes e430]
5294 e475 war_declared/70: The Grork Horde declared war on Finen over West Wood II. {@rg22 attacker=c3 defender=c1} [causes e364, e430]
5299 e483 sacked/90: The Grork Horde sacked Rannve of Finen; 664 died and 1552 fled. {@rg22 @s10 attacker=c3 defender=c1 dead=664 survivors=1552} [causes e482]
5299 e484 capital_moved/50: Finen moved its seat to Ithithe. {@rg22 @s52 civilization=c1 settlement=s52} [causes e483]
5302 e489 peace/60: Finen sued The Grork Horde for peace after 8 years of war. {winner=c3 loser=c1 years=8} [causes e488]
```

**One war at the `full` level, from casus belli to peace** (e174 to e203):

```
3642 e174 claim/45: The Grork Horde laid claim to Southwest Wood, held by Finen. {@rg35 claimant=c3 holder=c1} [causes e100]
3835 e196 war_declared/70: The Grork Horde declared war on Finen over Southwest Wood. {@rg35 attacker=c3 defender=c1} [causes e147, e174]
3836 e197 battle/30: The Grork Horde defeated Finen in Southwest Wood; 363 of the defeated and 290 of the victors fell. {@rg35 winner=c3 loser=c1 winner_dead=290 loser_dead=363} [causes e196]
3836 e198 siege/25: The Grork Horde besieged Assill of Finen; it held. {@rg35 @s29 attacker=c3 defender=c1 fell=0} [causes e197]
3839 e199 battle/30: Finen defeated The Grork Horde in Southwest Wood; 905 of the defeated and 111 of the victors fell. {@rg35 winner=c1 loser=c3 winner_dead=111 loser_dead=905} [causes e196]
3846 e201 battle/30: The Grork Horde defeated Finen in West Downs II; 347 of the defeated and 286 of the victors fell. {@rg31 winner=c3 loser=c1 winner_dead=286 loser_dead=347} [causes e196]
3846 e202 peace/60: Finen sued The Grork Horde for peace after 11 years of war. {winner=c3 loser=c1 years=11} [causes e201]
3846 e203 region_ceded/55: Finen ceded Southwest Wood to The Grork Horde. {@rg35 from=c1 to=c3} [causes e202]
```

The sample reads as P3-00 13 asks: the goblins quarrel with the elves over the wood between them within 74 years of their awakening and fight for it over 3,700 years, take it by cession and then the elves' home wood, sack their capital and leave the oldest realm a single town; the elves sack a goblin town in between and throw back the orcs; the dwarves arrive in 3242 to a feud whose last war ended in 2956 and whose next war comes in 3286, and hold their hills against four hordes; the goblins' secessions fight their parent and two are conquered by it; the faeries' court is attacked six times; the humans fill the downs and fight the goblins to a draw; the orcs attack the elves and the human duchy within sixty years of their awakening. What reads less well: Finen is reduced to one settlement (the median world keeps 12 elven settlements; 15.3), and the first war (e43) is declared four years before the goblins' opening phase ends and closes without a battle.

### 15.3 Sweep statistics

**Whole worlds** (min / median / max):

| Sweep | wars | battles | sacked | taken | civs ended | ruined (by sack) | flare-ups | simulated years | events | events / sim. year (max) | share max ‰ | over 800‰ years | bounds |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1024, seeds 0-49 | 7 / 23 / 48 | 19 / 72 / 207 | 2 / 12 / 32 | 0 / 6 / 20 | 0 / 1 / 3 | 2 / 14 / 36 (2 / 12 / 32) | 0 / 3 / 5 | 1122 / 1293 / 1450 | 439 / 815 / 1422 | 1.11 | 288 | 0 | none |
| 1024, seeds 0-99 (r7: the 0.16.0 validation, generator 0.16.0) | 5 / 24 / 75 | 17 / 79 / 302 | 2 / 12 / 45 | 0 / 6 / 36 | 0 / 1 / 3 | 1 / 13 / 37 (2 / 12 / 45) | 0 / 2.5 / 7 | 1122 / 1278 / 1511 | 387 / 819 / 1795 | 1.26 | 340 | 0 | none |
| 512, seeds 0-19 | 6 / 27 / 48 | 26 / 102 / 196 | 1 / 11 / 29 | 0 / 4 / 14 | 0 / 0 / 2 | 2 / 11 / 31 (1 / 11 / 29) | 0 / 3 / 6 | 1132 / 1275 / 1455 | 240 / 606 / 836 | 0.58 | 636 | 0 | none |
| 2048, seeds 0-4 | 2 / 12 / 20 | 10 / 49 / 72 | 0 / 5 / 9 | 0 / 4 / 9 | 0 / 1 / 1 | 1 / 9 / 10 (0 / 5 / 9) | 0 / 1 / 2 | 1148 / 1250 / 1270 | 666 / 784 / 1008 | 0.79 | 113 | 0 | none |
| 1024 turbulent | 8 / 38 / 174 | 28 / 130 / 838 | 2 / 20 / 51 | 0 / 13 / 61 | 0 / 2 / 9 | 4 / 23 / 60 (2 / 20 / 51) | 0 / 6 / 12 | 1162 / 1426 / 1750 | 580 / 1339 / 3495 | 2.07 | 422 | 0 | none |
| 1024 calm | 4 / 14 / 33 | 11 / 42 / 114 | 0 / 8 / 23 | 0 / 3 / 13 | 0 / 0 / 1 | 0 / 9 / 23 (0 / 8 / 23) | 0 / 0 / 3 | 1118 / 1200 / 1313 | 335 / 521 / 946 | 0.79 | 326 | 0 | none |
| 1024 civs-per-race 4 | 47 / 86 / 162 | 104 / 303 / 681 | 22 / 46 / 75 | 7 / 26 / 47 | 6 / 11 / 15 | 26 / 48 / 80 (22 / 46 / 75) | 1 / 6 / 9 | 1250 / 1450 / 1650 | 1218 / 2122 / 3029 | 1.98 | 446 | 0 | none |
| 1024 races elf,goblin | 0 / 5 / 17 | 0 / 16 / 75 | 0 / 3 / 11 | 0 / 0 / 12 | 0 / 0 / 1 | 0 / 4 / 11 (0 / 3 / 11) | 0 / 3 / 6 | 600 / 750 / 900 | 108 / 311 / 732 | 0.86 | 306 | 0 | none |
| 1024 races elf,orc | 0 / 1 / 3 | 0 / 2 / 9 | 0 / 0 / 2 | 0 / 0 / 1 | 0 / 0 / 1 | 0 / 1 / 4 (0 / 0 / 2) | 0 / 0 / 0 | 600 / 600 / 600 | 34 / 148 / 263 | 0.44 | 224 | 0 | none |
| 1024 span 2000 | 0 / 5 / 14 | 0 / 14 / 52 | 0 / 2 / 7 | 0 / 0 / 4 | 0 / 1 / 2 | 0 / 2 / 7 (0 / 2 / 7) | 0 / 0 / 1 | 370 / 390 / 408 | 75 / 224 / 402 | 1.00 | 215 | 0 | none |
| 1024 span 12000 | 12 / 53 / 120 | 43 / 180 / 475 | 4 / 28 / 69 | 2 / 18 / 68 | 0 / 3 / 10 | 6 / 34 / 75 (4 / 28 / 69) | 0 / 5 / 10 | 2224 / 2800 / 3300 | 815 / 1892 / 3517 | 1.12 | 352 | 0 | none |
| 1024 dense | 7 / 28 / 102 | 26 / 99 / 429 | 3 / 17 / 52 | 1 / 9 / 47 | 0 / 2 / 7 | 4 / 19 / 54 (3 / 17 / 52) | 0 / 3 / 8 | 1612 / 1886 / 2303 | 462 / 940 / 2415 | 1.17 | 295 | 0 | none |
| no prowess (all 1000), seeds 0-19 | 10 / 21 / 39 | 35 / 87 / 159 | 2 / 17 / 24 | 0 / 7 / 21 | 0 / 1 / 3 | 4 / 18 / 25 (2 / 17 / 24) | 1 / 3 / 3 | 1182 / 1296 / 1350 | 485 / 831 / 1400 | 1.08 | 291 | 0 | none |
| no refuge, no shelter, seeds 0-19 | 7 / 20 / 40 | 15 / 53 / 160 | 3 / 9 / 22 | 0 / 5 / 25 | 1 / 3 / 5 | 5 / 10 / 22 (3 / 9 / 22) | 0 / 2 / 6 | 1112 / 1281 / 1468 | 385 / 707 / 1402 | 1.08 | 283 | 0 | none |
| goblin aggression 700, seeds 0-19 | 11 / 25 / 59 | 32 / 71 / 207 | 5 / 12 / 26 | 1 / 6 / 17 | 0 / 1 / 3 | 5 / 14 / 27 (5 / 12 / 26) | 1 / 4 / 7 | 1232 / 1350 / 1475 | 525 / 905 / 1444 | 1.03 | 274 | 0 | none |

The last three rows are the counterfactuals of 12.2 and question 4 (everything else shipped). **The 100-world row** (r7) is the implementation's, measured by the validation of 0.16.0 (`docs/reviews/p3-06-expansion-conflict-validation.md` 4.1, 4.2, F3): seeds 50–99, which the prototype did not run, push every maximum of the 50-world row up (wars 75 against 48, sacked 45 against 32, events 1,795 against 1,422, flare-ups 7, the largest share 340‰, the threshold 51, goblin–elf wars per world 34); the medians agree within 10%. These are 0.16.0 figures: the P3-08 change (generator 0.17.0) moves them again, and its validation re-measures this table (`docs/specs/p3-08-followups.md` 8). P3-05's figures without conflict (its 15.3) are the baseline: ruins 0 / 1 / 4, no civilization ended, 310 / 564 / 966 events at 1024.

All hard bounds hold on every world of every sweep (15.5). At 1024 (seeds 0–49):

| Statistic | Value |
| --- | --- |
| wars per world | 7 / 23 / 48; length 1 / 10 / 42 years (1,208 wars; none ends in its declaration year, 7.3); seeds 0–99 (r7): 5 / 24 / 75, length 1 / 11 / 42 (2,667 wars) |
| how wars end | exhaustion 75%, aim taken 16%, conquest of a side 3%, end of the active phase 3%, open at the present 2%, no front, length or a side's decline 1% |
| attackers | goblins 77% of wars, orcs 13%, humans 7%, dwarves 3%, elves and faeries never; per century of their existence orcs start 0.80 wars a world, goblins 0.48 |
| claims, flare-ups | claims 9 / 28 / 68; flare-ups 0 / 3 / 5 (6 worlds none, 6 worlds five); simulated years 1122 / 1293 / 1450 (P3-00 4.1's median band 1000–1400 holds) |
| falls | sacked 2 / 12 / 32, taken 0 / 6 / 20, cessions 0 / 2 / 8; regions changing hands 2 / 8 / 18 |
| ruins | 2 / 14 / 36 per world, **92% by sacking** (P3-05 alone: 0 / 1 / 4, all by abandonment) |
| civilizations ended | 0 / 1 / 3; at least one in 30 of 50 worlds (70 of 100 at seeds 0–99, r7); by race: goblins 23 of 148, faeries 8 of 62, elves 4 of 86, dwarves 2 of 74, humans 2 of 82, orcs 1 of 53 |
| races extinct | none (`last_refuge`) |
| standing settlements at the present, by race (P3-05 alone in brackets) | elves 12 (20), goblins 32 (33), dwarves 7 (7), faeries 4 (7), humans 14 (11), orcs 5 (5) |
| relations | rows 17 / 55 / 116: `enmity` 11, `war` 24, `peace` 23 a world on average |
| shares | largest share of the non-islet land ever held by one civilization 106 / 176 / 288 ‰; `share_over_500_years` 0 on every world |
| events | 439 / 815 / 1,422; 0.62 per simulated year (max 1.11); seeds 0–99 (r7): 387 / 819 / 1,795, 0.63 (max 1.26) |
| summary threshold | 26 / 31 / 46 (P3-07 7.4; 122 / 207 / 250 events shown); seeds 0–99 (r7): 26 / 31 / 51 |

**Race characters** (P3-00 13; 1024, 50 worlds):

| Character | Measured |
| --- | --- |
| goblins hostile to elves from their first year | an `enmity` row from the goblins' founding in every world with both; goblin–elf wars 0 / 8 / 18 per world (none in 9 worlds, where the two never come within two borders); first such war within 200 years of the goblin awakening in 19 worlds, median 500 years |
| dwarves arrive into a world already at war | a war open within 50 years either side of the dwarven awakening in 30 of 50 worlds (open at the awakening in 5, declared in the dwarves' first 25 years in 24) |
| humans spread over land left open or ruined | human settlements 3 / 15 / 36; 42 of 815 stand in a region where a settlement had been ruined before their founding, 42 of 361 human regions had another holder before; humans took 127 settlements by capture or cession |
| orcs open the last age of conflict | 55% of all wars are declared in the orc age (the last 7% of the span): 3.3 wars a century there against 0.19 before; orcs declare a war within 100 years of their awakening in 30 worlds (none in 6); they are the attackers in 13% of all wars and 0.80 a century |
| elves old and few, oldest in the present | the first elven realm alive at the present in 49 of 50 worlds; elves hold 12 settlements at the median against 20 without war (1 at the least, seed 42 among them) |
| no world empty or dominated | 53 standing settlements at least; no civilization above 29% of the land |

### 15.4 Report keys

**History report `[stats]`** (P3-07 9 and 10.3 rule 1: appended at the end of the table, after `civilizations_seceded`), integers only, in this order:

| Key | Definition |
| --- | --- |
| `relations_enmity`, `relations_peace` | rows of `relation` of kind `enmity`, of kind `peace` |
| `war_years_total` | Σ over `war` rows of (`until`, or E when NULL) − `since` |
| `war_years_max` | the largest of those; 0 when there is none |
| `civilizations_conquered` | civilizations whose `ending_event` is of kind `civ_conquered` |
| `settlements_sacked` | settlements whose `ruin_event` is of kind `sacked` |
| `settlements_captured` | distinct `event.settlement` of events of kind `settlement_captured` |
| `regions_conquered` | `region_control` rows whose `event` is of kind `settlement_captured` or `region_ceded` |
| `races_alive` | `race` rows with at least one civilization with `ended_year` NULL |
| `wars_attacker_<race key>` | one line per `race` row in id order: `war` rows whose `a_id` is a civilization of that race |

Claims, battles and sieges are counted by P3-07's `events_<key>` lines. Strain, tension, force and capacity are not stored and are checked by this spec's tests (P3-07 20 item 2). The world report's `[history]` is unchanged.

### 15.5 Hard bounds and typical ranges (the validation sweep, P3-00 12.2)

**Hard bounds** (defects on any seed, P3-00 12.3; from `[stats]` and the tables):

| Bound | Rule | Measured (every swept world) |
| --- | --- | --- |
| not empty | P3-05 15.5's | at least 53 standing settlements at 1024, 26 at 512, 81 at 2048 |
| races | `races_alive` = the present races (`last_refuge` 1) | every world |
| runaway | `share_over_800_years` ≤ 1000 | 0 on every world of every sweep; the largest share ever held by one civilization was 288‰ at 1024 seeds 0–49 and 340‰ at seeds 0–99 (r7; 446‰ with four civilizations per race) and 636‰ at 512 (a 13-region world) |
| events | `events_per_1000_simulated_years` ≤ 200000 | at most 2.07 per simulated year (`turbulent`; 1.11 at default settings on seeds 0–49, 1.26 on seeds 0–99, r7) |
| phases | `simulated_permille` ≤ 450 (P3-04's cap) | at most 383‰ (`dense`, 2,303 of 6,000 years); 291‰ at `turbulent`, 275‰ at span 12000, 241‰ at default settings |
| capacity | P ≤ 2 × C(R, owner race) for every standing settlement after every step (this spec's tests) | holds in every world |
| sizes | history tables under 5 MB at 1024 | at most 3,495 events (`turbulent`; 3,029 with four civilizations per race, 3,517 at span 12000): well under 1 MB |

**Typical ranges from generator 0.18.0** (r13; 1024, default settings; outside them is a finding, not a defect). The 5th to 95th percentile of the 100 worlds of the P3-09 validation (`docs/reviews/p3-09-conflict-balance-validation.md` 5.4, 1024 seeds 0–99, release build), with the median:

| Statistic | Typical range | Median |
| --- | --- | --- |
| events | 382 – 1,216 | 726 |
| wars | 7 – 43 | 23 |
| flare-ups | 1 – 7 | 3 |
| claims | 14 – 73 | 37 |
| battles | 20 – 168 | 76 |
| battles involving faeries | 0 – 7 | 2 |
| sacked | 2 – 22 | 10.5 |
| taken (`settlement_captured`) | 1 – 21 | 7 |
| cessions (`region_ceded`, 7.7 and 20.3) | 1 – 15 | 6 |
| tributes | 0 – 12 | 5 |
| flights (`refugees_settled`) | 0 – 2 | 0 |
| endings: `peace` | 1 – 16 | 7 |
| endings: `war_won` | 1 – 12 | 5 |
| endings: `truce` | 1 – 12 | 5 |
| endings: `search_abandoned` (`wars_unfound`) | 0 – 9 | 4 |
| endings: conquest (`civ_conquered`) | 0 – 3 | 1 |
| `wars_hollow_permille` | 52 – 466 | 226 |
| `wars_same_goal_max` | 2 – 7 | 3 |
| `wars_same_goal_unchanged_max` | 2 – 3 | 3 |
| `race_population_share_max_permille` | 368 – 835 | 596.5 |
| `race_regions_share_max_permille` | 269 – 737 | 467 |
| largest civilization share of the non-islet land, max over the history (‰) | 127 – 334 | 211.5 |
| civilizations | 8 – 14 | 10 |
| civilizations ended | 0 – 2 | 1 |
| simulated years | 1,196 – 1,481 | 1,324.5 |
| `simulated_permille` | 199 – 247 | 220.5 |
| summary threshold | 26 – 36 | 31 |
| years from the goblins' awakening to the first war | 299 – 2,959 | 812.5 |
| standing settlements: faerie | 4 – 15 | 7 |
| standing settlements: elf | 2 – 26 | 8 |
| standing settlements: orc | 1 – 13 | 6 |
| standing settlements: human | 1 – 28 | 9 |
| standing settlements: dwarf | 4 – 18 | 5.5 |
| standing settlements: goblin | 4 – 94 | 37 |

Every race alive (a hard bound, T4). `wars_hollow_permille` is a per-world range here; T1 (P3-09 5) is the sweep's ratio, 229‰ at 0.18.0. `wars_same_goal_unchanged_max` ≤ 3 is an invariant from r13 (20.28), not a range. The known observations of 20.29 (F1: 20.11's race-share range narrowly missed; F3: T1 above 250 outside the 1024 default set) are recorded there and are not defects. The paragraph below is r7's, for 0.16.0, kept as the record.

**Typical ranges before 0.18.0** (1024, default settings; outside them is a finding, not a defect; r7: set to about the 5th to 95th percentile of the 100 worlds of the 0.16.0 validation, `docs/reviews/p3-06-expansion-conflict-validation.md` F3, measured by the P3-08 spec writer on its world files): wars 10–50 (p5 10, p95 50; before r7 10–40, which 20 of 100 worlds left); flare-ups 0–5 (p95 5); simulated years 1,120–1,450 (p5 1,162, p95 1,405); sacked 4–30 (p5 4, p95 30); civilizations ended 0–4; ruins mostly (≥ 70%) by sacking (p5 700‰; before r7 ≥ 80%); events 450–1,500 (p5 452, p95 1,451); summary threshold 26–46 (p95 46); every race alive; elves 3–40 standing settlements (p10 3; 7 of 100 worlds had 1, a finding for the critic, `docs/reviews/p3-06-conflict-world-critic.md` F5). The maxima of the 100 worlds: wars 75, sacked 45, events 1,795, flare-ups 7, simulated years 1,511, threshold 51.

### 15.6 For the validation specialist

Sweeps with shipped data and default settings, release build, from `report --history`'s `[stats]`: **1024 seeds 0–199**, **512 seeds 0–49**, **2048 seeds 0–19**; and 1024 seeds 0–49 with each setting of 15.3's table (`--event-intensity calm` and `turbulent`, `--civs-per-race 4`, `--races elf,goblin` and `elf,orc`, `--history-span 2000` and `12000`, `--history-detail dense`), plus P3-05's `--civs-per-race 1` and `--races elf`. On every world: the hard bounds of 15.5 and P3-05's; purity (threads 1 and 5, resume). A median more than 20% away from 15.3's is a finding for the spec writer. Performance: 16.6 at 1024 seeds 0–19.

## 16. Tests required

For the independent test writer; the engineer records final signatures here after implementation.

**What the reference must reproduce.** The reference `worldgen/tests/reference/p3_conflict_reference.py` (Python, exact integers, on `core_reference`, `p3_names_reference`, `p3_chronology_reference`, `p3_engine_reference` and `p3_peoples_reference`) is written **from this spec alone**, without reading or porting the C++. From a world's stored geography, chronology rows and the data root it computes **every row of all thirteen history tables** with peoples and conflict installed (P3-04's scheduler with this spec's tension, SC5/SC6), and compares them with the world byte for byte through the logical table hashes (P0-04 8), row by row on a mismatch. It also prints per war one line (declaration, sides, goal, battles, falls, outcome), the tension of every quiet step that has one, and the `[stats]` lines of 15.4. The spec writer's prototype (`conflict.py`) is not the reference; where it and this text differ, the text rules and the difference is reported to the lead.

### 16.0 Readings agreed in implementation (normative; r5)

Agreed by the engineer and the test writer (the reference's `gaps` list); each is the rule.

1. **7.2 items 2 and 3.** The flare pair is computed once per T2, after the strain update and the claims, before the first declaration; no claimed pair means no flare pair. When a = a* but b* is not a candidate (sheltered, out of contact, …), item 3 applies to a as to every other civilization.
2. **7.1 `pop0`** = `PeoplesView::population` at the declaration: every standing settlement of the side, including a colony peoples founded earlier in the same T2 (P3-05 13 defines it without a visibility filter; it is a total of people, not a use of the colony as an entity). The prototype summed the visible settlements only, so it counted one fewer settlement whenever a side had founded a colony in that T2; since colonists are taken from a visible source, the difference is the colonists (at most 12% of one settlement, under 1% of a realm). It can change the exhaustion test of 7.7 only when a side is within that margin of `exhaustion_permille` × `pop0`; no compared world differs (seed 42 and 17 others), and the sweep figures of 15 stand.
3. **7.3 item 5.** `winner_dead` and `loser_dead` are the dead of step 4 as computed, before 7.4's min(dead, T); the war's dead (7.7) add 7.4's dead = min(dead, T). 7.4's remainder step takes at most the largest settlement's population, so the people removed can fall short of the recorded dead by at most one person per settlement of the side; never with shipped tuning.
4. **7.3 item 7, 7.7 item 2, 7.2's goal re-choice** read standing **visible** settlements (as `presence` does); in T6 every settlement is visible.
5. **7.7 cession refugees**: the excess of a ceded settlement goes to L's nearest other standing settlement at that moment, which may be another settlement of L in G not yet ceded (P3-05 8.5 item 2 applied literally, the settlements of G in ascending id).
6. **7.5 item 3**: if the captured settlement was y's capital and y has another standing settlement, the capital moves with `capital_moved` caused by the capture; with none left, 7.6 ends y and no `capital_moved` is appended.
7. **7.6**: a conquest drops every claim involving y and closes every war of y (the effective forces of its other sides rise accordingly); strain, grudges and `last_peace` are unchanged.
8. **8.1 item 1**: relations closed in housekeeping and in a conquest are taken in `RelationKey` order; a war with an ended side is over without an event of its own, its row closed by that relation pass; when both sides have ended, the row is closed with the smaller id's ending event. (The prototype took the row's a side first, which for a `war` is the attacker and may be the larger id; it differs only when both sides of a war end before one housekeeping, which no swept world showed.)
9. **5.4 "past their truce"** is tested at the quiet step's start year (the year of Q4).
10. **4.2 decay in ticks**: years × `grudge_decay_per_century` div 100 with years = 1 is 0 at the shipped 50, so grudges decay only in quiet steps (12 per 25-year step); a tuning of 100 or more decays them in ticks too.
11. **16.3 (c)**: one tension line per quiet step whose tension the engine asks (every quiet step but the one that ends its interval, SC6), value 0 and trigger e0 included.
12. **16.3 (b)** war lines: `<case> war e<w> <start>-<end|open> c<a> c<b> rg<goal> claim e<claim> battles <n> sacked <n> taken <n> <how> <outcome>`, `<how>` one of `exhaustion`, `aim`, `no-front`, `length`, `quiet`, `conquest`, `ended`, `open`, `<outcome>` `won-c<W>` (with `-ceded`), `truce` or `-`.
13. **15.4 `settlements_sacked`** counts current rows: a sacked ruin that has faded no longer has a `ruin_event` (P3-05 r3), so it is not counted; the statistic is "sacked ruins still standing as ruins".
14. **15.4 `war_years_total`**: E = `history_end_year`.
15. **5.3 and 8.3 item 4**: the claim's cause, as stated in 8.3 item 4 (r5).
16. **14 item 4 "a conqueror alive"**: alive at the conquest: `ended_year` NULL, or later than the year, or the same year with an ending event after the conquest.

### 16.1 Unit tests (seams on hand-made state)

**Worked example W1 (a war on three regions, by hand).** Regions R1, R2, R3 in a row, borders (1, 2) and (2, 3) walkable (cost 1), no river, cover 0 (grassland, no rough ground); N = 3 non-islet regions. Civilization 1 (elves: good, aggression 150, prowess 1600) holds R2 and R3: s1 its capital, a town of 2,000 in R3, and s2 a village of 500 in R2. Civilization 2 (goblins: evil, aggression 600, prowess 800) holds R1: s3, its capital, a town of 6,000. No grudge, hunger 0, `wanted_held`(2) = ∅; the view's σ for goblins: R2 500, R3 600; shipped tuning; civilization 1 founded at year 0, civilization 2 at year 0 with `shelter_years` passed.

| Quantity | Value |
| --- | --- |
| d(1, 2), h | −700, 700 |
| share(1), fear(1) | 666, 1000 × 266 div 600 = 443 |
| drive(2 → 1), drive(1 → 2) | 200 + 700 + 0 + 443 = 1343; 200 + 700 = 900 |
| front(2, 1) | (R2, 1, R1), (R3, 2, R2); k = 1 |
| rate(2 → 1) | 600 × 600 div 1000 = 360; × 1343 div 1000 = 483; × 1000 div 1000 = 483 |
| strain per tick year | 483 × 25 = 12,075 milli |
| claim | in the 25th tick year (301,875 ≥ 300,000); region: keys (1, −400, 2) and (1, −400, 3), so R2; cause R2's `owner_event` |
| force(2), force(1) | 6000 × 100 div 1000 × 800 div 1000 = 480; (200 + 50) × 1600 div 1000 = 400 |
| declaration at year 50 (strain 603,750, s = 603) | p = 150 × (303 × 1000 div 600 = 505) div 1000 = 75; odds: 400 × 1000 div 1000 × 400 div 1000 = 160, 480,000 div 160 = 3000, so 1000; p = 75‰ |
| battle in R2 (in year 51 at the earliest: no battle in the declaration year, 7.3) | A = 480, D = 400, def = 1000, D' = 400, p = 480,000 div 880 = 545‰ |
| if civilization 2 wins | dead_L = 100, dead_W = min(480, 400) × 80 div 1000 = 32 (r7; 38 at 0.16.0, 480 × 80 div 1000); civilization 1 loses 80 at s1 and 20 at s2; civilization 2 loses 32 at s3 |
| siege of s2 | walls 1500; G = 480 × 300 div 1000 = 144, × 1600 div 1000 = 230, × 1500 div 1000 = 345; A2 = 448 (r7; 442 at 0.16.0); ps = 448,000 div 793 = 564‰ (r7; 442,000 div 787 = 561‰ at 0.16.0) |
| if it falls | sack chance 200 + 700 = 900‰; a sack kills 480 × 300 div 1000 = 144, 336 flee to s1 (room permitting); R2 is released; the aim is achieved: `peace` (winner 2, loser 1); no cession (R2 no longer held by 1); grudge 50 + 150 = 200, so d = −900 |
| if it is taken instead | 48 die; s2 passes to civilization 2 with 432 (if 2 × C(R2, goblin) ≥ 432), R2 to civilization 2; aim achieved |

The test builds this state through the seams, runs the pure functions and the T2 of the declaration year and the T6 of the next year with the reference's draws, and checks every line of the table (both branches of each draw through hand-made `ConflictTuning` values of 0 and 1000 where needed: `battle_permille` 1000, `sack_base` 0 or 1000 with `sack_hostility` 0).

1. Disposition and grudges: the camp table for every camp pair; secession grudge; capture and war grudges; decay at L = 25 (12 per step at 50 per century) and the floor at 0.
2. Front: shared region (cost 1); one border; two borders through a third civilization's region and through an islet; a pass (cost 3) against a two-border walkable path (cost 2: the path wins); `march_hops` 1 and 2; ties of (cost, last region).
3. Drive and rate: each term alone; friendly pair (d ≥ friction) with drive 0; fear at share 400 (0) and 1000 (`fear_weight`); left-to-right integer evaluation of the rate; cap at `strain_max`; intensity calm and turbulent.
4. Claims: threshold exactly at `claim_strain`; region choice with and without `wanted_held`; the cause when the holder has the region and when it has only a settlement there; no claim during a truce, a war, without budget in a quiet step.
5. Tension: the largest strain among claimed pairs; ties by (a, b); pairs without a claim, at war, in truce, out of contact ignored; value 0 and trigger 0 when there is none; read-only (state hash before and after).
6. Declarations: shelter; at most one war as attacker; odds for aggression 150, 600, 900; the flare-up war without a draw in the first T2 of a flare-up only; goal re-choice when the claim region is gone; the closing of the pair's `peace` row.
7. Battles: initiative swap when x has no front; goal targeting; def with each bonus; `battle_at_pass` with `pass`; casualties' distribution with a remainder; `note_reduction` on every settlement of the side; (r7) the victors' dead from the smaller force: F_W = 900, F_L = 300 gives dead_W = 24 and dead_L = 75, F_W = 300, F_L = 900 gives dead_W = 24 and dead_L = 225; the capture sentence with `fled` 0 (`…; 135 died.`), 1 (`…; 135 died and one fled.`) and 12 (`…; 135 died and 12 fled.`).
8. Sieges and falls: capital walls; `last_refuge` on the last settlement of a race and not on the last of a civilization whose race lives on; sack with refugees and region release; capture with the 2 × C cap and its refugees; capital moved after a capture; conquest with its relation closings.
9. Ends of war: no battle and no end check in the declaration year (7.3); each of 7.7's items once (aim, both exhausted, exhausted leader → truce, exhausted loser → peace, no front, length); cession and its conditions (attacker wins, goal still held, not the capital's region); 7.8's quiet-step end, with and without budget.
10. Housekeeping: relations of a civilization ended by P3-05's decline closed with its `civ_ended`; `enmity` opened once per pair, for good–evil only with the shipped table.
11. Catalogue: kinds 20–30 equal section 11 transcribed; templates pass `template_problem`; `history.report` lists every kind.
12. Tuning: each field's range, each named check's message once, the unknown-id expectation of P3-01 (12.1).
13. Race data: `prowess_permille` required and ranged (P3-01's loader tests); the goblin aggression 600.
14. P3-05's `ruin` (18.1): items 2–5 with the given event; `abandon` unchanged on P3-05's fixture worlds. From 0.16.0 the P3-05 golden cases are explicit peoples-only configuration runs through the library (`run_engine` with peoples and no conflict); their catalogue tables hold the 30 kinds of 0.16.0, so their hashes are re-recorded (r5).

### 16.2 Property tests

15. Over the fixture worlds (P3-04 16.2 item 17's synthetic regions stage: 2 to 60 regions, islets, barriers, passes, river borders) × seeds 0–15 × spans 2000, 6000 × races all, `elf,goblin`, `elf,orc`, `dwarf,faerie` × `civs_per_race` auto, 4 × intensities: invariants 1–9 of 14 and P3-05's; rows equal the reference.
16. On the 1024 golden cases: the hard bounds of 15.5.
17. Y3 for each `hist.conflict.*` stream; Y4 with conflict installed (relations opened in two different orders in one step give the same rows).
18. Threads 1 and 5 identical; resume (P3-04 Y2) with conflict installed.
18a. **The peoples-only prefix** (r7; the validation's F1). On the golden cases and the fixture worlds of item 15: the log with conflict installed equals the log of the same inputs with peoples alone (P3-05's configuration, conflict not installed) for every event with id below the first `claim`, row for row in `event`, `event_participant`, `event_cause` and `event_value`, **except** the `quiet_era` events open in the quiet step of the first claim, whose `end_year`, `years` and `generations` may be smaller: that step can end its quiet phase row (a flare-up started by the claim's tension, 5.4), which closes every era open in the row. Generator 0.16.0 had a second exception, the claimant's own era cut by the claim (an event charged to it closed its era, P3-04 6.6 before r10; in 5 of 27 worlds of the 0.16.0 validation); from 0.17.0 an era is not closed by its civilization's events (P3-04 r10). A world with no claim equals the peoples-only world in every history table except the three catalogue tables (as `hist-races-elf-orc-1024` did at 0.16.0).

### 16.3 Golden vectors

19. `worldgen/tests/golden/p3-06-conflict.txt`, written by the reference: for the four P1 golden cases and the three `hist-*` cases: (a) the logical hashes of the thirteen history tables; (b) the war lines of 16; (c) the tension of every quiet step (`<case> q<start> <value> e<trigger>`); (d) the `[stats]` lines of 15.4; (e) the rendered sentence of every event of the 512 cases and of every event of kinds 20–30 of the 1024 cases (`*-slow` suite). The C++ must match every line.
20. The sample of 15.2 (seed 42 at 1024, default settings) is a golden case of its own, `hist-conflict-42-1024`, in `p3-06-conflict.txt` only: its war list and the sixty summary lines of 15.2 must equal the spec's (prototype and reference are independent; a difference is resolved before merge). The shipped re-recording of seed 42 is this case; P3-05's `hist-peoples-42-1024` stays a peoples-only configuration case (item 14), and the CLI peoples test file is retired in favour of the conflict one (r5). Extra golden cases of the test writer: `hist-conflict-turbulent-512`, `hist-conflict-civs4-512`, and three `norefuge` stress cases (`last_refuge` 0, `shelter_years` 0) that exercise conquest to extinction.

### 16.4 Fatal messages (normative; P3-04 16.6's form, all ASCII)

Seam guards, unreachable with loaded data and tested through the seams with hand-made tuning; tests compare the texts exactly. `<...>` are decimal integers unless stated.

| Group | Messages |
| --- | --- |
| Tuning (at configuration, in the order of 12.1, record fields as `<record>.<field>`) | `history: tuning: the content has no definition history.conflict`; `history: tuning: conflict.<field> outside its schema range`; `history: tuning: claim_strain must not be above declare_min`; `history: tuning: declare_min + declare_span must not be above strain_max` |
| Race data | `history: conflict: race '<key>' is not in the content` (a present race row with no race definition) |
| Bounds (H6) | `history: bound: conflict force of civilization <c> exceeds 2^40`; `history: bound: conflict strain of civilization <a> against <b> exceeds 2^40` (unreachable: `strain_max` caps it) |
| Seam | `history: conflict: ruin of settlement <s>, which is not standing` (a fall of a settlement that is not standing: an internal error) |
| Internal guards (r5) | `history: conflict: camp <k> is not 0, 1 or 2`; `history: conflict: race <r> does not exist`; `history: conflict: region <R> does not exist`; `history: conflict: contact cost <k> below 1` |

Every other failure surfaces through P3-04's `State` and event-writer messages (16.6 there), with the call or event that failed.

### 16.5 Agreement with P3-04 r3

Checked after merging P3-04 r3: P3-06's steps T2, T6, Q4 are subsystem steps (r3's refusal of engine-owned steps does not apply); P3-06 calls `quiet_events_left` only in quiet steps (r3 makes it `quiet_events_max` elsewhere, which P3-06 does not rely on); the first year of a flare-up is recognized by the phase id and kind in T2 (7.2), after the engine's `flare` step has appended the point event `flare_up` (r3 6.3), whose id P3-06 neither needs nor predicts; P3-06 creates no civilization or settlement, so W4's next-id rule does not touch it; every relation it opens is closed with an event (r3's guard "a relation vanished without a closing event" cannot fire); the floor clamp of r3 5.1 changes no flare-up arithmetic (P3-06's flare-ups are bounded by the cap as before). All message texts of this spec (12.1's named checks, 16.4) are ASCII.

### 16.6 Performance

`history` at 1024 with peoples and conflict (prototype: about 2.2 s per world on top of P3-05's 3.4 s of CPU time in Python, single-threaded): budget **4 s** in a release build (P3-00 12.1 gives 15 s for P3-04 to P3-06 together; P3-05's own budget is 2 s), 512: 1 s, 2048: 16 s. Costs per tick year: the fronts of every ordered pair of alive civilizations (about 100 pairs × a two-border search over about 30 presence regions), `wanted_held` per civilization, and per open war a battle; per quiet step the same strain update once. A microbenchmark tagged like P3-04 14.3's reports it. The engineer may cache presence and fronts per step (they change only with ownership), which this text permits because they are pure functions of the step's state.

## 17. Determinism notes

| Concern | Rule |
| --- | --- |
| Arithmetic | `int64` only; floor division (`floor_div`) wherever a numerator can be negative (d, drive before its max); products bounded: rate ≤ 1000 × 5000 × 1000 / 10^6, strain ≤ 5 × 10^6, force × 1000 < 2^41 |
| Order | civilizations ascending; ordered pairs (a, b) ascending; wars by declaration event id; relations by `RelationKey`; settlements ascending; front entries by region id, paths by (cost, last region) |
| Snapshots | `wanted_held`, holdings and shares for the strain update from the step's start; owners and populations live elsewhere, in the stated order |
| Draws | all keyed (H4) with the stored declaration event id as the war key; no draw depends on another's outcome except through state |
| Containers | ordered maps keyed by integers or integer pairs; no unordered container |
| Visibility | entities created in a step are not processed in it (P3-04 6.2); refugees may join a settlement created earlier in the step (P3-05 8.5's exception) |

Lint impact: none expected.

## 18. Amendments requested

**Applied 2026-11-19** on `p3/06-spec` after the lead's approval, each in its spec's convention: P3-05 "Revision note r1 (… for P3-06)", P0-01 section 19, P3-01 "Revision note r7", P3-07 "Revision note r3", P3-00 "Correction note (2026-11-19, for P3-06)", P3-04 "Revision note r4", `schema.md` section 6 ("Planned additions"). Each amendment that describes data or behaviour of generator 0.16.0 is marked so in the amended spec; **P3-05's implementer at 0.15.0 applies only 18.1 item 1** (the `ruin` restructuring, no world byte changes).

### 18.1 P3-05 (peoples) — applied as P3-05 r1 (8.5, 13, 14, note); item 1 is the only part of 0.15.0

1. **New hook** `PeoplesSubsystem::ruin(std::int64_t settlement, std::int64_t event)` (as implemented, P3-05 r3: `ruin(StepContext&, settlement, event)`): performs 8.5 items 2–5 for the settlement with `event` in place of the abandonment event (survivors to the owner's nearest other standing settlement up to its room; `set_population(s, 0)`, `set_settlement_status(s, ruined, event)` so that `ruin_event` = `event`, `set_settlement_owner(s, 0, event)`; the release of the region; the capital move with `capital_moved` caused by `event`), and **not** item 6 (the caller ends the civilization). `abandon(s, cause)` becomes: append `settlement_abandoned`, `ruin(s, that event)`, then item 6. No behaviour of a P3-05 world changes (a refactor; P3-05's golden vectors at 0.15.0 are unaffected).
2. 13 "Ruin by sacking": P3-06 calls `ruin(settlement, sack event)`, not `abandon`; the ruined row's `ruin_event` is the `sacked` event (P3-07 20 item 3).
3. 13 "Ending by conquest": P3-06 appends its own kind `civ_conquered` (30, roles `civilization` and `conqueror`), not `civ_ended`.
4. 14 invariant 4: the exception "(P3-05 alone; P3-06 may hold regions without settlements)" becomes "with P3-06 too: P3-06 transfers regions only with settlements (P3-06 14 item 5)"; invariant 7: a ruined row's `ruin_event` is a `settlement_abandoned` or (from P3-06) a `sacked`.
5. 15: the measurements are those of a world without conflict (0.15.0); P3-06 15 gives the 0.16.0 figures.

### 18.2 P0-01 (stream list) — applied as P0-01 section 19

The registry gains, after `hist.peoples.split`, `hist.conflict.declare`, `hist.conflict.battle`, `hist.conflict.battle_win`, `hist.conflict.siege`, `hist.conflict.sack` (51 names). No world byte changes by registration.

### 18.3 P3-01 (data) — applied as P3-01 r7 (3, 3.1, 5, 8.1 item 4, status), every item from 0.16.0

1. Race field `prowess_permille` int(1, 4000), required, after `aggression`: "fighting strength of the race's people, per mille of the standard (P3-06 6.1)"; shipped values 12.2; the fixture race files gain it.
2. Goblin `aggression` 600 (section 3.1's shipped data).
3. Field meaning of `aggression`: readiness for war, which P3-06 squares in the strain rate (5.2) and uses against the odds of a declaration (7.2); of `expansion`: unchanged (P3-05).
4. Section 5's id table: `conflict` "added by P3-06 (12.1 there)"; 8.1 item 4's expected list `'chronology', 'conflict', 'engine', 'peoples', 'report'`, and a future-id example that is not registered (for example `monsters`).

### 18.4 P3-07 (appended keys; format stays v1) — applied as P3-07 r3 (9, note)

Section 9 gains, after `civilizations_seceded`, the keys of 15.4 in that order with those definitions. 20 item 1 is met by 4.3 (`war` asymmetric, `enmity` and `peace` symmetric). The illustrative excerpt of 17 stays illustrative (its kinds `region_claimed` and `civ_ended`-by-conquest are not P3-06's).

### 18.5 P3-00, P3-04, `schema.md` — applied as the P3-00 correction note for P3-06 (4.4, 6.1, 10.5, 10.6), P3-04 r4 (6.4, 8.2), `schema.md` 6

1. **P3-00 6.1** (`relation` row note): kinds `enmity`, `war`, `peace` (P3-06 4.3); vassalage and alliances deferred; the expected event count marked as a proposed correction (question 8), awaiting Paul. **10.5**: a sacked settlement is ruined (its survivors flee) or a fallen one changes owner; a civilization with no standing settlement left by conquest ends with `civ_conquered`. **10.6**: the 3 × 3 table's shipped values (4.1). **4.4** Q4: P3-06 ends open wars and builds strain and claims (7.8).
2. **P3-04 6.4**: the shipped `EngineConfig` installs `ConflictSubsystem` after `PeoplesSubsystem` (steps T2, T6, Q4) and as the only tension source, from generator 0.16.0; 8.2: the catalogue is extended by P3-06 with kinds 20–30. No engine rule changes.
3. **`schema.md`**: a "Planned additions" list after the current (0.14.0) catalogue gives kinds 20–30 from 0.16.0 (structure only), which the engineer moves into the table at the merge; the relation kinds `enmity` (symmetric), `war` (asymmetric, a the attacker), `peace` (symmetric). No schema change; `schema_version` stays 7.

## 19. Open questions for Paul

Each with the recommendation and the interim choice this spec follows.

1. **Limited wars that end with the active phase** (7.7, 7.8). A war is fought for one claimed region and ends when the attacker has it, when a side is exhausted, after 40 years at most, or at the first quiet step after its active phase, by the war score. Recommendation and interim: yes. A war carried through a quiet interval would be a war of centuries with no battle in it; ending it at the quiet step reads as the design's "four centuries of peace". Cost: a war declared in the last years of an active phase may end after a few years without a battle (36 of 1,208 wars at 1024, 3%, end that way).
2. **Wars only in yearly ticks; strain builds in quiet steps; a flare-up always brings its war** (5.2, 7.2). Recommendation and interim: yes (P3-00 4.4 already has no wars in quiet steps). The flare-up's pair declares without a draw in the flare-up's first year, so a flare-up never passes without the conflict that caused it.
3. **A fighting strength per race and goblins less aggressive** (12.2). New race field `prowess_permille` (elves 1600, dwarves 1400, orcs 1300, faeries 1200, humans 1000, goblins 800) and goblin `aggression` 700 → 600. Recommendation and interim: yes. Without prowess, four times as many elven realms fall (12.2). At goblin aggression 700 the goblins start 80% of all wars, and the median world has a fourth flare-up (12.2).
4. **No race is wiped out** (7.3 item 7, 7.2 `shelter_years`). The last settlement of a race's last civilization never falls, and a civilization is not attacked in its first 150 years. Recommendation and interim: yes: design 5 gives every race a role in play (townsfolk, traders, enemies) and P5's exit check needs every race to take part; a race that survives as a single walled town is still a history. Without the guard and the shelter (1024, seeds 0–19), 17 of 20 worlds lost at least one race: the orcs in 15, the faeries in 7, the humans in 3, the goblins, the elves and the dwarves in 1 each (the late peoples are conquered as hamlets before they grow). Alternative: allow extinction and let P5/P6 treat an extinct race as history only.
5. **Coalition by fear instead of alliances; no vassals** (4.3, 5.1). Recommendation and interim: yes in P3; alliances and vassalage come with figures and factions (P5), as new relation kinds without a schema change.
6. **Aggression squared** (5.2). The race field's 0–1000 range is read as readiness, and its square drives strain, so orcs (900) quarrel nine times as fast as dwarves (300) and thirty times as fast as elves (150). Recommendation and interim: yes; the linear reading made goblins and orcs nearly equal and left elves and faeries starting wars.
7. **Armies march two borders** (5.1). Recommendation and interim: `march_hops` 2; with 1 a quarter of the 1024 worlds had no goblin–elf war and the median world two (P3-05's homes keep the two peoples apart); with 2 the median world has eight and 9 of 50 none.
8. **The event log is far smaller than P3-00 6.1's estimate.** P3-00 expected 5,000–30,000 events at 1024; P3-05 and P3-06 together give 439 / 815 / 1,422 at 1024 and up to 3,517 at span 12000. Recommendation: correct P3-00's expectation to "400–1,500 events at default settings, up to about 4,000 with the heavier settings (`turbulent`, four civilizations per race, span 12000), well under 1 MB"; the 200-per-simulated-year bound is three orders of magnitude away. Interim: no rule changes; P5's subsystems add their own.
9. **Humans on land left open or ruined** (9). Humans spread over open land (P3-05) and resettle sacked regions within their reach, but the old wars are mostly fought far from the farmland where humans awaken, so few human settlements stand on ruined ground (40 of 762 human settlements; 40 of 357 human regions had another holder before). Recommendation: accept in P3; P5's refugee migration (P3-05 question 6) is the stronger reading. Interim: as specified.
10. **"A world already at war" when the dwarves arrive** (9). Measured as a war open within 50 years either side of the dwarven awakening: 30 of 50 worlds at 1024. Recommendation: accept; the rest of the worlds have their goblin–elf wars earlier or later in the goblin age, and every world has the `enmity` rows. Alternative: an awakening raises every hostile pair's strain (a rule the design does not state).

## 20. Conflict balance (amendment r9, P3-09, generator 0.18.0)

Written 2026-11-20 by the spec writer for change P3-09 "conflict balance" (`docs/specs/p3-09-conflict-balance.md`), in answer to the world critic's review of this stage at 0.16.0 (`docs/reviews/p3-06-conflict-world-critic.md`: findings F1, F2, F3, F5, F6, F7, F9; suggestions 1, 3–6, 8–10). **Normative from generator 0.18.0, not before.** It builds on r7 and r8 (P3-08, generator 0.17.0, as implemented on `p3/08-int`: the victors' losses from the smaller force, the capture sentence; and P3-05 r6 and r7: the faerie forest weights, dwarven upland sites, kind 31 `settlement_abandoned_to` with its destination, no chance abandonment of a civilization's last settlement, the plague plan replayed, one quiet era per quiet stretch). Where this section and sections 4–17 differ, this section rules; each item names what it replaces. Four of its rules are design-level (`docs/design/proposals.md`, "four rule changes proposed by the world critic's review of conflict"): flight of the last refuge (20.1), the cost of a lost war (20.3), revenge (20.5) and runaway measured per race (20.11). **Paul accepted them and every other recommendation of P3-09 8 on 2026-11-20**; the rules are decided as written here.

**Measurements.** Figures marked *(m16)* come from the spec writer's prototype `~/.cache/the-unmapped/out/p3-09-spec/proto.py` and its variants (a subclass of the test writer's reference `p3_conflict_reference.py`, which reproduces 0.16.0 row for row), run on the geography of the 0.16.0 validation sweep (`~/.cache/the-unmapped/out/p3-06-validation/sweep/`), with two parts of P3-08 emulated: the victors' losses from the smaller force (r7 7.3) and the faerie weights of P3-05 12.3 (a copy of `game/data`). P3-08's other changes (dwarven upland sites, quiet eras, halved famine and plague spread, names, kind 31) were not emulated. Every figure is "measured at 0.16.0 with that emulation" and is **re-measured at 0.18.0** by the statistic named with it (P3-09 5). Unless stated otherwise: 1024, default settings, seeds 0–39.

### 20.1 The last refuge falls into flight (replaces the last line of 7.3 item 7; F2, suggestion 5)

**Last refuge.** A settlement t of civilization y is a *last refuge* when t is y's only standing settlement and y is the only alive civilization of its race (as 7.3 item 7).

**Siege of a last refuge.** When the siege draw of 7.3 item 7 says the last refuge t fell and `last_refuge` = 1:

1. if `refuge_flight` = 1, a flight destination exists (below) and the survivors are enough to settle (n of the procedure's item 2, computed with the sack toll of item 1, is at least P3-05's `abandon_min`), t **falls into flight**: the `siege` event has `fell` = 1 and the flight procedure follows instead of 7.5;
2. otherwise the guard holds as before: `fell` = 0 ("there is nowhere to flee"). The `abandon_min` condition keeps the invariant below: a refuge founded with fewer people would be abandoned by P3-05 8.5 (b) in the next T1 (a deterministic rule that applies to a last settlement, P3-05 r6), ending the race.

**Flight destination** (pure; no draw). Candidates D: every region with no owner, not an islet, D ≠ the region R_t of t, `hops`(R_t)(D) ≤ `flight_hops` (P3-05 4.4's border hops over the open borders), and C(D, r_y) ≥ `expand_min_capacity` (P3-05 12.1). For each, h_x(D) = min over the regions P of `presence(x)` of `hops`(P)(D), or `march_hops` + 1 when unreachable, capped at `march_hops` + 1 (every region beyond the attacker's reach is equally safe). Order the candidates by (−h_x(D), −σ(D, r_y), D) ascending (farthest from the attacker, then the best land, then the smaller id); the destination is the first with a site by P3-05 6.2 (not relaxed). None: no destination.

**Flight procedure** (T6 only, right after the `siege` event e_s; x the besieger, w the war):

1. dead = P_t × `sack_toll` div 1000; P_t −= dead (`set_population`); append `sacked` (region R_t, settlement t; participants x `attacker`, y `defender`; values `dead`, `survivors` = P_t; cause e_s; salience `capital_sacked_salience`, t being y's capital). No sack draw is taken: a refuge that falls is always sacked.
2. n = min(P_t, 2 × C(D, r_y)) (P3-05 8.5's room, `refugee_room`, of a settlement of population 0 in D); h = the next settlement id; its name as P3-05 names a colony (`next_name`: the race's culture, kind settlement, entity h, through the name filter of P3-02 r5); append `refugees_settled` (32; region D, settlement h; participants h `settlement`, y `founder`, t `origin`; value `population` = n; cause the sack); `found_settlement` at the site (status thriving, population n, owner y, kind by P3-05 8.4 r6 for that site cell: a fortress by the fortress and cell rules, else `ladder(r_y, n)`); `set_region_owner(D, y, refugees_settled)`; `hidden_until[h]` = Y + `hidden_years`.
3. P_t −= n (`set_population`); `PeoplesSubsystem::ruin(ctx, t, sack)`: ruin's destination is P3-05 8.5 item 1's (`nearest_standing`, `refugee_room`), which is h, y's only other standing settlement; the rest (P_t > 0 only when n was capped) finds h full and is lost; t is ruined with `ruin_event` = the sack, R_t released, and y's capital moves to h with `capital_moved` caused by the sack (P3-05 8.5 item 5; h is a destination created earlier in the step, the stated exception). **Not an abandonment**: no `settlement_abandoned_to` (kind 31) is written. Kind 31 records survivors who walk to an *existing* settlement of their own civilization; kind 32 records survivors who *found* a new one, with the old settlement as `origin` and the new one as the event's settlement. `ruin` appends neither (as for every sack).
4. The war: settlements taken by x += 1; grudge{x, y} += `capture_grudge`; `wronged`(y, x) gains R_t (20.5); the war's last event = the `refugees_settled`. y is not conquered (h stands). 7.7 follows as usual (aim achieved when the goal was R_t).

**Hidden settlements** (new; 5.1, 6.2, 7.3). A settlement s is *hidden* in year Y when Y < `hidden_until[s]` (a fled refuge) or when it is *concealed* (20.7). A hidden settlement:

- counts in no presence: `presence(c)` (5.1) = the regions of c's standing visible settlements that are not hidden, together with the regions c holds in which c has no hidden settlement (a region whose only settlements of c are hidden is out of everyone's sight); so a hidden settlement's region is never in a front, never a claim region, battlefield, war goal or siege target, and a civilization all of whose settlements are hidden is in contact with no one (its strains stop growing, its open wars end by 7.7 item 3, no front);
- still counts in its owner's force (6.1), population (`PeoplesView`), and region holdings (shares, fear, `region_control`); P3-05 treats it as any settlement (growth, foundings: its colonies are not hidden);
- is never besieged, so it is never a last refuge under siege while hidden.

When Y reaches `hidden_until[h]` the refuge is *found*: it is an ordinary settlement from then on. No event marks it (the year follows from the `refugees_settled` event and the tuning). `hidden_until` is a member (ordered map by settlement id), set only here.

**Invariant** (14 item 7 restated): with `last_refuge` 1, no race present goes extinct by conflict: a last refuge either holds or flees, the flight never ends its civilization, and a hidden refuge cannot be attacked. (P3-05's abandonment no longer drops a civilization's only settlement by chance, P3-05 r6 8.5 (a).)

**Why flight and not "no claim on a last refuge"** (the review's second option). Measured *(m16)*: with flight, sieges of a last refuge fall from 216 to 88 in 40 worlds, none is held by the guard, 52 refuges flee (never for want of a destination), hollow wars fall from 40% to 32% and faeries end with one settlement in 3 worlds instead of 6. With the alternative (no claim or declaration against a civilization that is its race's last refuge, strains kept) 161 sieges of a last refuge remain and 102 are held by the guard (wars declared before the defender became a last refuge, and battles that reach the refuge on the way to another goal), hollow wars 38%, faeries with one settlement in 8 worlds. The alternative hides the symptom; flight turns it into a story (a ruin, a sack with a cause, a hidden hamlet to find) and fits design 5's "hidden communities in wild places".

### 20.2 War goals (amends 5.3 and 7.2's goal re-choice; F1, suggestion 1a)

**Allowed goals** of a against b: the entries of `front(a, b)`, without the regions b is *weary of* (20.4), and without the region of b's capital unless that leaves none (then the capital region is the only allowed goal: a war of plunder, whose cost to the loser is 20.3's).

1. **Claims** (5.3): the claim region is the allowed goal with the smallest key (0 if R ∈ `wronged`(a, b) and b holds R, else 1; 0 if R ∈ `wanted_held(a)` and b holds R, else 1; −(σ(R, r_a) − `cost_penalty` × cost); R). No allowed goal: no claim.
2. **Declarations** (7.2, "Declaring"): if the claim region is no longer an allowed goal, or b neither holds it nor has a standing visible settlement in it, the goal is the allowed goal with the smallest 6.2 target key; if there is none, the claim is dropped and no war is declared (the draw was taken; nothing else changes).

### 20.3 The cost of a lost war (amends 7.7 "Cession"; F1, suggestion 1b)

After the `peace` of a war won by W over L (either side: the attacker or the defender may win), when 7.7's cession of the goal did not happen:

1. **Cession of another region.** Candidates: the regions R held by L, in `front(W, L)`, other than the region of L's capital. If any: G' = the one with the smallest 6.2 target key for W; append `region_ceded` (region G'; participants L `from`, W `to`; cause the peace) and transfer as 7.7's cession (each standing visible settlement of L in G', ascending id (**from r13 every standing settlement of L in G', visible or not: 20.27**), gives its excess over 2 × C(G', r_W) to L's nearest other standing settlement and passes to W; `set_region_owner(G', W, cession)`); `wronged`(L, W) gains G'.
2. **Otherwise tribute.** n = Σ P over L's standing visible settlements that are not hidden × `tribute_permille` div 1000; no tribute is taken from a civilization that is its race's last refuge (one standing settlement, the only alive civilization of its race: casualties could leave it below `abandon_min`, and P3-05 8.5 (b) would end the race); if n > 0: append `tribute` (33; no region; participants L `payer`, W `payee`; value `people` = n; cause the peace), then `casualties(L, n, tribute)` (7.4: the people are taken from L's settlements in proportion, `note_reduction` on each; W gains no people: P3 has no wealth or captives to count, and the event is the record lore needs).

A truce costs nothing. In a quiet step (7.8) an end that would cede or take tribute waits, like a cession, until both the winner and the loser have quiet-event budget (the loser is charged: it is the first civilization participant of `region_ceded` and `tribute`). *(m16: the prototype skipped the cost instead of waiting when the loser had no budget; budgets of 2 per step make this rare. The reference implements the wait.)*

Measured *(m16; configurations of P3-09 5.1)*: with 20.1–20.5 and 20.7's home defence (configuration G5), hollow wars (no sack, capture, cession, tribute or conquest) are 15% of 821 wars against 40% of 911 at the baseline, and attacker victories that gained nothing 1 of 392 against 124 of 460, with 160 cessions of 20.3 and 336 tributes in 40 worlds; costing only the attacker's victories (G3) leaves 32% hollow. With every rule of this section (configuration FINE) 24% of 736 wars are hollow, 0 of 387 attacker victories gain nothing, 139 cessions of 20.3 and 224 tributes.

### 20.4 War weariness (new; amends 5.3; F1, suggestion 1c)

`futile[(b, G)]` (member; ordered map keyed by (defender id, region id)) = (count, owner). When a war with defender b and goal G ends by peace or truce (7.7, 7.8), and G's owner at that moment is the owner G had at the declaration (recorded in the war), the count rises by 1 and the owner is recorded; a count whose recorded owner differs from G's current owner reads as 0 (any change of owner resets it). b is **weary of** G for every claimant while the count read is ≥ `weary_wars` (shipped 3): G is not an allowed goal against b (20.2). The count is per defender and region, over all attackers: the measured repetition is several hordes of one people against one wood, not one pair.

Measured *(m16)*: the longest run of consecutive wars over one (defender, goal) that left the goal's owner unchanged is, per world, 1 / 3 / 4 (min / median / max) and above 3 in 2 of 40 worlds with every rule (FINE; against 1 / 5 / 12 and 26 worlds at the baseline). Counted per pair instead of per defender (configuration F) the maximum was 7 and 13 worlds exceeded 3, because several hordes of one people take turns at one wood.

### 20.5 Revenge (amends 5.2, 5.3, 7.2; F3, suggestion 3)

`wronged(a, b)` (member; ordered map from (a, b) to an ordered set of region ids): a region is added when b sacks or captures a settlement of a there (7.5, 20.1), or a cedes it to b (7.7, 20.3). While `wronged(a, b)` is not empty and grudge{a, b} > 0, a's **effective aggression** against b is g'(a, b) = max(g_a, `revenge_aggression`) (shipped 400); g' replaces g in the rate of 5.2 and in the odds of 7.2. The set is never emptied; the grudge's decay (4.2) ends the revenge. Claims prefer wronged land still held by b (20.2 item 1). The flare-up war, the declaration chance and everything else are unchanged.

So a people of any temper strikes back at whoever took its towns, at the pace of a moderately warlike one (400² / 1000 = 160 against the elves' own 22), and only while the wound is fresh. Measured *(m16)*: elves declare 17 wars in 40 worlds with revenge and the costs alone (configuration D; 0 at the baseline) and 10 with every rule (FINE: 20.6's odds minimum holds back the weaker side); faeries none (concealed, they are rarely wronged); goblins still declare 74% of all wars.

### 20.6 Orcs that win their age (amends 6.1 and 7.2; F3, suggestion 4)

Four rules, which together make the orcs' wars ones they can win (design 5: "strong, warlike"; "their coming opens the most recent age of conflict"):

1. **Muster** (6.1). force(c) = levy(c) × `prowess_permille`(r_c) div 1000 × `muster_permille`(r_c) div 1000 (race field, 20.12; shipped orc **4000**, every other race 1000: an orc horde puts 40% of its people in the field). The exhaustion test of 7.7 item 1 reads pop0 × `muster_permille` div 1000 for each side: a people that arms more of itself bears proportionally more losses before it breaks (without this, a large muster only makes the orcs bleed out faster: 26 won against 60 lost with muster 4000 and the unscaled test, *m16*).
2. **Local levies** (7.3 items 3 and 4). In a battle at R each side fights with its *local force* (the A and D of 7.3 item 3, and so the F_W and F_L of item 4: the victors' dead are `loss_winner` of the smaller local force, r7): the levy over its standing visible settlements s with `hops`(R)(R_s) ≤ `levy_hops` (shipped 3), times prowess and muster, divided by its open wars. A realm defends a border with the levies of the lands near it, not with its whole people; a sprawling kingdom is weaker at its far edge, a compact horde is not. Declarations, odds and exhaustion keep the whole force and population.
3. **Odds** (7.2 item 3). The target's force in the odds is weighted by the defence of the claimed region G: fb = force(b) × def(G) div 1000, with def by 6.3 for the entry of G in `front(a, b)` (1000 if G is not in it); the aggression term is m = max(1000 − g', `odds_floor`) (shipped 500); odds = min(1000, force(a) × 1000 div max(1, fb × `declare_ratio` div 1000 × m div 1000)). If odds < `odds_min` (shipped 700) there is no declaration and no draw. **The tension (5.4) and the flare pair (7.2 item 2) skip a claim whose odds are below `odds_min`**: a quarrel a people cannot pursue does not flare up (without this rule one world of 40 ran fifteen flare-ups on a claim no one could act on, 2,478 events, *m16* seed 29). The odds are a pure read of the step's state, so `tension()` stays a pure read.
4. **Truce** (5.2, 7.2). The truce that bars a's strain and declarations against b after their last peace is directional: a is past it when Y ≥ the peace year + `truce_years` × min(1000, (1000 − g_a) × 1000 div (1000 − `truce_break_aggression`)) div 1000 (shipped 600). Only a people more warlike than the goblins keeps a shorter truce: the orcs (900) 50 years, everyone else 200. 5.4's "past their truce" is a's.

Measured *(m16)*: orcs win 39 and lose 30 of their wars that end in a peace (19 and 84 before); they are attacked in 6 wars (0 before); they start 25% of the wars of the orc age (24% before: 83 of 320) and goblins 48% (59% before), goblins 74% of all wars (76%). **The target "orcs start a third of the orc age's wars" is not met**: it was reached only by letting orcs declare against stronger foes (36% with `odds_min` 500 and no tension rule, at 28 won against 55 lost); winning and starting a third pull against each other with a horde of 5,000 against kingdoms of 30,000 (P3-09 decision 8.5).

### 20.7 Faeries hidden, elves' home ground (amends 6.3, 7.3; F5, suggestion 6)

Two race fields (P3-01 amendment, 20.12), both on P3-05 4.2's biome term B(R, r) (Σ over the race's biome weights of weight × share div 1000, from −1000 to 1000):

1. **Concealment** `conceal_min` int(0, 1001): a standing settlement of race r in region R is **concealed** (hidden, 20.1) while B(R, r) ≥ `conceal_min(r)`; 1001 = never. Shipped: faerie **400**, every other race 1001. With P3-05 12.3's weights a faerie settlement in a region about half or more forest or wet woodland is out of sight; one in open land is not.
2. **Home defence** `home_defense_permille` int(0, 10000): the defender's terrain in its own preferred land. 6.3's def(R) gains + `home_defense(r_y)` × max(0, B(R, r_y)) div 1000, and the garrison of 7.3 item 7 is multiplied by (1000 + `home_defense(r_t)` × max(0, B(R_t, r_t)) div 1000) div 1000 (r_t the besieged settlement's owner race; applied after the walls, left to right). Shipped: elf **1500**, faerie **2000**, every other race 0 (dwarves already have the rough ground of `cover`, and P3-05 r6 puts them in uplands).

Measured *(m16)*: home defence alone left the faeries with one settlement in 3–6 of 40 worlds (configurations F to H1; they lost 101 of 115 wars against them in G5: a faerie court of 5,000 fields 600); concealment at 400 brings it to **0 of 40** (H2, and FINE: faerie standing settlements 4 / 8 / 18, P3-05's median of 8 without conflict, against 1 / 5 / 18 and 6 worlds with one at the baseline), while their settlements in open land are still fought over. Elves with every rule: 3 / 9 / 45 standing settlements and **none** of 40 worlds with one (baseline 1 / 12 / 44 and 3); the costs of 20.3 take land from elves, home defence, local levies and the early feud's shorter wars give some back.

### 20.8 The early feud and map size (F7, F9; suggestions 9 and 10)

1. **Enmity hops** (5.1, 6.2). For an ordered pair (a, b) with an `enmity` row (4.3, opened or closed: the pair was ever of hostile camps), `front(a, b)` searches paths of at most `enmity_hops` (shipped **3**) borders instead of `march_hops` (2); every other pair keeps `march_hops`. The goblins and the elves, the good and the evil, find each other one valley further; neutral peoples do not. Measured *(m16)*: worlds without a goblin–elf war 4 of 40 (8 without this rule, 6 at the baseline); worlds whose first war comes more than 1,000 years after the goblins' awakening 15 (21; 18); worlds with 1,500 years or more without a declaration after it 9 (11; 10). The review's alternative (an awakening raises every hostile pair's strain) does nothing for a pair that is not in contact, which is the reason the feud is missing; it is not adopted. The remaining 4 worlds have the two peoples more than three borders apart (their homes are chosen by P3-05's homeland rule, design 5's "goblins hostile to elves from the first year" then stays a relation row).
2. **Map size: no conflict rule; more realms on a larger map** (P3-05 21 item 5). The rules of 20.1–20.7 remove most of what made 512 "a massacre" without a size rule *(m16, 512 seeds 0–29, `base017` against every rule)*: hollow wars 58% → 16%, the most futile wars over one goal 17 → 3, sieges of a last refuge 578 (302 held) → 157 (76 held, every one for want of free land: a 512 map has about 12 regions), faeries with one settlement 16 → 1 of 30 worlds, elves 6 → 3, humans 7 → 4; orcs, however, end with one settlement in 8 worlds (3 before). At 2048 (seeds 0–4) the rules leave a world nearly at peace (9 wars median at the baseline, 11 with every rule; armies crossing three borders change nothing there, 11 with `march_hops` 3 for every pair): the peoples are as many as at 1024 on four times the land (about 200 non-islet regions against 50; design 2: "a larger map shows more land at the same scale"). With four civilizations per race at 2048 (the explicit setting, as an approximation of P3-05 21 item 5) the five worlds have 17 / 41 / 64 wars, a goblin–elf war in every world, 15% hollow wars, 1–5 civilizations ended and 1,431 / 2,583 / 3,135 events: the tone of 1024 repeated across a larger land. So the scaling belongs to how many realms a race founds, not to how far armies march.

### 20.9 Data: tuning, kinds, saliences

`history.conflict` (12.1) gains, after `last_refuge`, in this order (all required):

| Field | Type | Shipped | Status | Meaning |
| --- | --- | --- | --- | --- |
| `refuge_flight` | int(0, 1) | 1 | decided (Paul, 2026-11-20: P3-09 8 item 1) | a fallen last refuge flees (20.1); 0: the guard of 0.16.0 |
| `flight_hops` | int(1, 8) | 3 | taste | farthest flight in borders (20.1) |
| `hidden_years` | int(0, 10000) | 300 | taste | years a fled refuge stays hidden (20.1) |
| `weary_wars` | int(0, 100) | 3 | taste | futile wars over one region after which its defender is weary of it; 0 = never (20.4) |
| `tribute_permille` | int(0, 1000) | 100 | taste | people taken from a loser with no region to cede (20.3) |
| `revenge_aggression` | int(0, 1000) | 400 | taste | effective aggression against whoever wronged a people (20.5) |
| `levy_hops` | int(0, 16) | 3 | taste | borders from the battlefield within which a side's settlements fight (20.6) |
| `odds_floor` | int(0, 1000) | 500 | taste | floor of (1000 − aggression) in the odds (20.6) |
| `odds_min` | int(0, 1000) | 700 | taste | no declaration, and no flare-up, below these odds (20.6) |
| `truce_break_aggression` | int(0, 999) | 600 | taste | aggression above which a people's truce is shorter (20.6) |
| `enmity_hops` | int(1, 4) | 3 | taste | borders an army crosses against a civilization it has an `enmity` row with (20.8); a named check `conflict_enmity_hops`: `enmity_hops` ≥ `march_hops`, message `enmity_hops must not be below march_hops` |

Named check added: `conflict_enmity_hops` (above), after the two of 12.1. The race kind gains `muster_permille`, `conceal_min` and `home_defense_permille` (20.12).

**Event kinds** appended after P3-08's kind 31 (`settlement_abandoned_to`, P3-05 r6); caused, every count 1–1:

| id | key | region | settlement | roles | values | salience | template |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 32 | `refugees_settled` | 1 | 1 | 1 `settlement`, 2 `founder`, 3 `origin` | `population` | 65 | `The survivors of {origin} fled into hiding and founded {@settlement} in {@region}.` |
| 33 | `tribute` | 0 | 0 | 1 `payer`, 2 `payee` | `people` | 50 | `{payer} paid tribute to {payee}; {people} {people?was\|were} carried off.` |

Cause rules (11): `refugees_settled` ← the sack of the refuge; `tribute` ← the peace; `region_ceded` of 20.3 ← the peace (as 7.7's); `capital_moved` ← the sack (20.1 item 3). Saliences and templates are data in `history.report` (and the fixtures). **`claim` salience 45 → 35** (P3-07 21.6): with the claim year in the declaration's note, a claim followed by its war is redundant at summary level.

`docs/specs/schema.md`: the catalogue gains kinds 32 and 33 (the engineer, at the merge). No schema change: appending kinds is not one (P3-04 8.2), and every other new fact is a member of the subsystem or a value of a new kind (`schema_version` stays 7).

### 20.10 Order, implementation rules and determinism

- **Steps**: 20.1's flight, 20.3's costs and 20.4's counts happen where the events they follow happen (T6 for the flight; T6 and Q4 for the end of a war); 20.2 in the claims (T2, Q4) and declarations (T2); 20.5 in the strain updates and declarations; 20.7 in battles and sieges (T6) and in presence (everywhere). Nothing runs in another step; `tension()` stays a pure read (5.4: hidden settlements change presence, hence contact, before the read).
- **P3-04 6.9 / 8.3 item 1 (A1).** P3-06 now founds a settlement (20.1 item 2). The founding happens inside T6's loop over the subsystem's own war list (ids, not `State`'s lists), never inside a loop over `civs()`, `settlements()` or `relations()`; after `found_settlement` every settlement is re-fetched by id (t for the `ruin` call); no reference into `State` is held across it (lint rules `history-live-iteration`, `history-held-reference`). 8.4 item 2: `ruin` is still called only in T6, on a standing owned settlement.
- **8.2**: the founded refuge is created in T6 and not processed by any rule of that step except as `ruin`'s destination (P3-05 8.5's exception); P3-05 first sees it in the next T1.
- **Draws**: none new. The flight takes no draw (a fallen refuge is always sacked); 20.2–20.7 are pure. Every draw keeps its key (8.3 item 5), so a world whose history never reaches a new rule differs only from the first point where one applies.
- **Orders**: candidates and maps ascending by integer keys (regions, civilizations, settlements, (defender, region)); `hops` from P3-05 4.4.
- **Arithmetic**: int64; the new products are bounded (force × 10000 / 1000, garrison × 11000 / 1000, population × 1000).

### 20.11 Statistics (amends 15.4–15.6; F6, suggestion 8)

P3-07 21.5 appends six `[stats]` keys: `wars_hollow`, `wars_hollow_permille`, `wars_same_goal_max`, `wars_same_goal_unchanged_max`, `race_population_share_max_permille`, `race_regions_share_max_permille` (definitions there). `events_refugees_settled` and `events_tribute` come with the kinds (P3-07 9's `events_<key>`).

**Typical ranges** (15.5) gain, at 1024 with default settings (outside them is a finding, not a defect; r13: measured at 0.18.0 in 20.29): `wars_hollow_permille` below 250 (over the 1024 default sweep only; T1 is not defined for any other map size or setting, 20.29); `wars_same_goal_unchanged_max` at most 3 in at least 90% of worlds; `race_population_share_max_permille` below 700 in at least 75% of worlds (*m16*: 31 of 40 with every rule, 36 of 40 at the baseline; the costs of war move people toward the strong: P3-09 decision 8.4); faeries and elves each with one standing settlement in at most 5% of worlds; no sieges of a last refuge held by the guard except when no destination exists or the survivors would be fewer than `abandon_min` (counted by the reference, not stored; acceptance target T3 of P3-09 5).

**Validation** (15.6) measures these on the 1024, 512 and 2048 sweeps, and in addition, from the tables: attackers by race (`wars_attacker_<race>`), orc wars won and lost (peace events with an orc civilization as winner or loser), the attackers of the orc age (wars declared at or after the orcs' awakening year), the years from the goblins' awakening to the first war and the longest stretch without a declaration after it, worlds without a goblin–elf war, and the standing settlements per race (`settlements_standing_<race>`); the spec writer's prototype figures are in P3-09 5.

### 20.12 Race data and P3-01 (amendment)

Three race fields, required, after `prowess_permille` (P3-01 r9): `muster_permille` int(1, 10000) (6.1, 20.6), `conceal_min` int(0, 1001) (20.7), `home_defense_permille` int(0, 10000) (20.7). Shipped:

| Race | `muster_permille` | `conceal_min` | `home_defense_permille` |
| --- | --- | --- | --- |
| elf | 1000 | 1001 | 1500 |
| goblin | 1000 | 1001 | 0 |
| dwarf | 1000 | 1001 | 0 |
| faerie | 1000 | 400 | 2000 |
| human | 1000 | 1001 | 0 |
| orc | 4000 | 1001 | 0 |

The fixture race files gain the same values. These change every world's `content_hash`.

### 20.13 Invariants (amends 14)

1. Item 5 (territory): every change of a region's owner by P3-06 is a `settlement_captured`, a `region_ceded` (7.7 or 20.3) or a `refugees_settled` (into a free region).
2. Item 7: every present race has an alive civilization at the present (unchanged, now with flight).
3. New: every `refugees_settled` has `population` ≥ `abandon_min` and is caused by a `sacked` of the same year whose settlement is the event's `origin`, whose defender is the event's `founder` and was then its race's last refuge; its settlement is never the settlement of a `siege` dated before its year + `hidden_years`, nor in the region of a `battle`, `claim` or `war_declared` against its founder in those years while it is the founder's only settlement there.
4. New: every `tribute` is caused by a `peace` whose loser is its `payer` and whose winner is its `payee`; no `region_ceded` of 20.3 and `tribute` follow the same peace together; the ceded region of 20.3 is not the loser's capital region.
5. New: no claim and no declaration names a region whose defender was weary of it at that step (the reference recomputes the counts from the log).
6. New: no settlement that is concealed at a siege's year is that siege's settlement.

Invariants (7)–(11) are in 20.19 and (12) in 20.23. (r13) **(13)**, 20.27 item 6: right after each `region_ceded` of G from L to W, L has no standing settlement in G, and every settlement of L that stood in G is W's; item 1 above (territory) then holds with no exception.

### 20.14 Tests required (adds to 16)

For the independent test writer; the reference `p3_conflict_reference.py` gains every rule of this section (from this text).

1. Units: flight destination order (−h_x, −σ, id) with the cap at `march_hops` + 1, the hop limit, capacity and islet exclusions, the first candidate without a site skipped; no destination, or survivors below `abandon_min`, → the guard holds; no kind 31 written by a flight; the flight procedure's events, populations (n capped at 2C, the rest lost), capital move and region owners; hidden: no presence, no front, no siege, found at `hidden_until`; a civilization wholly hidden loses contact and its war ends by no front.
2. Goals: the capital region excluded unless alone; the declaration's re-choice; a claim dropped when no allowed goal remains.
3. Costs: cession of the best non-capital region in front for a winning attacker and for a winning defender; tribute when none; no tribute from a last refuge; truce costs nothing; the quiet-step wait for the loser's budget.
4. Weariness: the count, its reset on a change of owner, per defender over two attackers, the exclusion at the third futile war.
5. Revenge: g' in the rate and the odds, only while wronged and grudge > 0; the claim key's first component.
6. Orcs and odds: muster in force and in the exhaustion test; local force with settlements at `levy_hops` and one beyond; the odds weighted by the goal's defence, the floor, `odds_min` (699, 700) with no draw below it; tension and flare pair skipping such claims (a read-only check: state hash before and after); the directional truce (orc 50 years, goblin 200) in strain, claims and declarations; `enmity_hops` in `front` for an enmity pair only.
7. Concealment and home defence: B at the threshold (399, 400), def and garrison with home defence, a concealed settlement never besieged and its region out of presence.
8. Invariants 20.13 on the property worlds of 16.2 item 15 and on the golden cases; the extra golden case `hist-conflict-flight` of the test writer (a world with flights and found refuges).
9. Golden: every P3-06 golden vector re-recorded at 0.18.0 (P3-09 6).

### 20.15 Who won the war (r10; replaces 7.7's "War score"; Paul's note 1 of 2026-11-20)

Paul, reading seed 42 at 0.17.0: in 3826 the Hold of Dulimin killed 991 goblins for 44 of its own (e146) and then "sued for peace" (e147). Its own 44 dead took it over `exhaustion_permille`, and the score of 7.7 (battles won, not their size) stood 2:1 for the goblins from two small battles. With 20.3 that defender would also cede land. Three rules (`docs/design/proposals.md`, "Paul's notes of 2026-11-20 from reading a generated history", points 1 and 2, accepted for P3-09):

1. **The war score weighs the scale of the fighting.** For a war with attacker a and defender b:

   ```
   s = (dead(b) − dead(a)) + (taken(a) − taken(b))
   ```

   Here dead(c) is the war's dead of c (7.4: c's casualties in the war's battles; a tribute's people are not war dead), and taken(c) is the sum of P, at the moment of the fall and before its toll, of every settlement c took in this war (sacked, captured or fallen into flight, 7.5 and 20.1). s > 0: a won; s < 0: b won; 0: a truce. The score counts people killed and people taken: what the war did, not how many skirmishes went each way. (A draw band, a truce whenever |s| is within 10% or 3% of all the dead, was measured and not adopted: it adds truces, and so hollow wars: 20.18.)
2. **An exhausted side that leads wins** (replaces the truce of 7.7 item 1 for "the one exhausted side leads on score"). The war ends with that side as winner: it has bled, but it has done more harm than it took. The other side, not exhausted, has not yielded, so the event is a `war_won` (item 4), not a `peace`. Both sides exhausted, or the exhausted side behind: 7.7 item 1 as before.
3. **The winner of the last battle does not lose.** In every ending decided by the score (7.7 items 1, 3 and 4, and 7.8's end in a quiet step), if the side the score names as loser won the war's most recent battle, the war ends in a `truce` instead. An ending by aim achieved (7.7 item 2) is unchanged: a defender that has lost its goal has lost, whatever its last battle. (Measured against the variant "only when the last battle was in the ending's own step": 20.18.)
4. **"Sued for peace" only when the loser yields.** Kind 27 `peace` (`{loser} sued {winner} for peace after …`) is written only when the loser is exhausted: an ending by exhaustion in which both sides are exhausted, or the one exhausted side is the one the score disfavours. That side is the one that asks. Every other ending with a winner (item 2's exhausted leader, aim achieved, no front, length, the quiet step's end) writes the new kind **34 `war_won`** (no region; roles 1 `winner`, 2 `loser`; value `years`; caused; salience 60; template `{winner} prevailed over {loser} after {years} {years?year|years} of war.`), with the same causes, relation changes, `last_peace` and grudge as a `peace`.

   Wherever P3-06 20, P3-07 21 or P3-09 name "the peace" of a war, a `war_won` is meant too. This covers 20.3's costs (cause: the `peace` or `war_won`), 20.4's count, P3-07 21.2's units (the row "`peace`, `truce`" reads "`peace`, `war_won`, `truce`"), P3-07 21.5's definitions, and the invariants of 14 item 2 and 20.13 item 4. 11's cause rule for `peace` and `truce` holds for `war_won`.

### 20.16 Battle losses bounded by the victors (r10; amends 7.3 item 4)

dead_L = min(F_L × `loss_loser_permille` div 1000, F_W × `kill_max_permille` div 1000), with F_W and F_L the sides' local forces (20.6 item 2); dead_W as r7 (from the smaller force). Shipped `kill_max_permille` **500**: a host kills at most half its own number in one battle. Without the bound, forty defenders in a strong position that win the draw kill a quarter of a horde of thousands (seed 42 at 0.17.0, 4424, e225: 881 goblins and 4 faeries fell).

### 20.17 Faeries are not found (r10; replaces 20.7 item 1 and the "concealed" case of 20.1's hidden settlements; Paul's note 2)

Paul: "maybe instead of getting involved in lots of battles the Faeries could be so hard to find that they often can't be conquered?" Concealment becomes a search, and faeries are protected by not being found, not by winning.

**One notion, two strengths.** A settlement is out of an army's reach in one of two ways:

- **Hidden** (20.1, unchanged): a fled refuge, for `hidden_years`. It is out of every presence, so no one quarrels with it, claims it or marches on it.
- **Concealed**: a standing settlement of race r in region R with B(R, r) ≥ `conceal_min(r)` (20.7; faerie 400). It is now **in presence like any settlement**: its people are known, quarrelled with, claimed and warred on. But an army must find it before it can fight there:

1. **Search** (7.3, before item 2's battle draw). When the battlefield R of 6.2 holds standing visible settlements of the side being attacked, y, and every one of them is concealed, x searches: found if chance_pm(draw(`hist.conflict.search`, Y, w, R, 0), `find_permille`) (shipped **250**).
   - Not found: no battle and no siege this year, and the war's `unfound` count rises by 1.
   - Found: `unfound` = 0 and the year goes on as 7.3, in which the siege may take a concealed settlement.
   - A region where y also has a settlement that is not concealed needs no search.
2. **Giving up.** When `unfound` reaches `search_years` (shipped **3**), the war ends in this T6, before 7.7: append **35 `search_abandoned`** (region R; roles 1 `searcher` (x), 2 `hidden` (y); value `years` = Y − start; caused by the war's last event; salience **40**; template `{searcher} searched {@region} for {hidden} and found no one; the war ended after {years} {years?year|years}.`).
   - It is then treated as a truce (7.7 "Ending in a truce": `war` row closed, `peace` row opened, `last_peace`, `war_grudge`, strains and claims cleared), with no cost (20.3 needs a winner).
   - It counts in 20.4's weariness like any war that left the goal unchanged.
   - One event per war, at its end, and none for each failed search. A reader meets "the horde searched the Tangle of Yews and found no one" once per such war, below the summary's usual threshold (P3-07 21: shown when T ≤ 40, as companion of a shown declaration).
3. **Faerie home defence 0.** `home_defense_permille` of the faerie becomes **0** (20.12's table: was 2000); the elves keep 1500 (decided). A faerie host that is found fights as what it is, a small people.
4. **Statistics.** A war ended by `search_abandoned` is **not hollow**: it is the faeries' way of not being conquered. P3-07 21.5's `wars_hollow` excludes wars whose `war` row's `until_event` is a `search_abandoned`; a new key `wars_unfound` (appended after `race_regions_share_max_permille`) counts them. Acceptance target T1 (P3-09 5) is `wars_hollow_permille` < 250 with that definition. P3-09 5 also reports wars_unfound separately.
5. **Stream** `hist.conflict.search`, registered after `hist.conflict.sack` (P0-01's stream list; registration changes no world byte). Key (w, R): one search per war per year.

### 20.18 Measured (r10)

**Prototype at 0.17.0.** The test writer's reference on the merged branch is now P3-08's, generator 0.17.0. The spec writer's prototype (`~/.cache/the-unmapped/out/p3-09-spec/proto_v7.py`, a subclass of it, run with the shipped `game/data`) reproduces the 0.17.0 world exactly (seed 42 at 1024: 847 events and 41 wars, as the 0.17.0 C++ world of the P3-08 validation). So these figures are **measured at 0.17.0**; the r2 figures of 20.1–20.8 were at 0.16.0 with P3-08 emulated.

The prototype now follows the text where the test writer found it did not (P3-09 11):
- the refuge's kind by P3-05 8.4's cell rule;
- the `abandon_min` condition of 20.1 and the last refuge's exemption from tribute;
- a quiet-step end that waits for the loser's budget;
- a dropped claim in one direction only;
- presence counting every standing settlement;
- 20.20 item 1 (20.20 item 2 is P3-05's and is not prototyped).

1024, default settings, seeds 0–39. `B17` is 0.17.0 as shipped; `F17` is every rule of r2 (P3-09 as decided); `L0` is every rule of r2 and r10 as written.

| Statistic | `B17` | `F17` (r2) | **`L0` (r10)** |
| --- | --- | --- | --- |
| wars per world (min / median / max) | 7 / 21.5 / 59 | 3 / 17 / 40 | 8 / 23.5 / 45 |
| wars ended unfound (`search_abandoned`) | — | — | 169 |
| hollow wars, of wars that found their enemy (T1) | 399 of 974 (40%) | 172 of 740 (23%) | **185 of 807 (22%)** |
| worlds with a run of more than 3 futile wars over one (defender, goal) (T2) | 28 | 0 | **0** |
| sieges of a last refuge held while a destination existed (T3) | 163 of 278 held (no flight rule) | 0 of 53 | **0 of 46** |
| worlds with a race extinct (T4) | 0 | 0 | **0** |
| orc wars won / lost (T5) | 17 / 91 | 55 / 24 | **52 / 35** |
| endings with a winner whose loser won the war's last battle | 58 of 720 | 29 of 482 | **5 of 524** (endings by aim, which item 3 allows) |
| endings: `peace` / `war_won` / truce / unfound | 536 / 184 / 195 / 0 (`war_won` not yet a kind: endings that are not by exhaustion) | 298 / 184 / 219 / 0 | 303 / 221 / 244 / 169 |
| battles in which a victor of less than a tenth of the loser's force killed more than half its own number | 46 | 15 | **0** (21 bounded by 20.16) |
| battles with faeries (per world median; total) | 7.5; 451 | 0; 0 (fully concealed) | **3; 124** |
| searches; failed | — | — | 1,019; 749 |
| faerie standing settlements (median); worlds with 1 | 5; 6 | 8; 0 | **7; 0** |
| elven standing settlements (median); worlds with 1 | 11.5; 4 | 8; 4 | 8; 2 |
| goblin share of wars; orc share of the orc age | 76%; 24% | 73%; 28% | 73%; 27% |
| worlds without a goblin–elf war | 7 | 3 | 2 |
| events (median); simulated years (median) | 652; 1,284 | 620; 1,297 | 690; 1,311 |

Variants measured and not adopted (same seeds; each as `L0` but **without item 2** of 20.15, the exhausted leader's win, and with a draw band of 10% of all the dead, unless stated):
- **`N1b`** (that base): hollow 29% of wars that found their enemy.
- **`S30`** (band 3%): 28%.
- **`S0`** (no band): 26%. The new score and item 3 make more truces, and truces are hollow; item 2 turns most of them into wins, which brings `L0` to 22%.
- **`SV1`** (no band, with 7.7's battle-count score): hollow 25%, but orcs win 29 and lose 34, so T5 fails: the count favours the many small victories of the larger people.
- **`N2`** (item 3 only within the ending's own step): 10 endings still have a loser that won the last battle.
- **`N4`** (no claims on a region where the holder is wholly concealed): only 3 wars end unfound and faeries fight 1 battle in 40 worlds, but hollow wars are 32% and wars fall to 786. Not adopted: Paul's reading is that faeries are warred on and not found, not that no one wars on them.
- **`N3`** (`find_permille` 150): 198 wars end unfound and faeries fight 61 battles; 250 is kept.

At 512, seeds 0–29 (`B512` 0.17.0, `L0_512` r10): hollow wars of wars that found their enemy 58% → 31% (T1 is a 1024 target; 512 stays harsher); runs above 3 in 27 → 4 worlds; refuges held while a destination existed 291 of 549 → 0 (75 held for want of land); no race extinct; orcs 23 / 80 → 30 / 21; faeries with one settlement 15 → 2 worlds, elves 7 → 0; faerie battles 685 → 178.

### 20.19 Data and tests (r10)

- `history.conflict` gains, after `enmity_hops`: `kill_max_permille` int(1, 10000) 500 (taste, 20.16), `find_permille` int(0, 1000) 250 (taste, 20.17), `search_years` int(1, 100) 3 (taste, 20.17). Race data: faerie `home_defense_permille` 0 (P3-01 3.2's table as amended in r10 there).
- Kinds 34 `war_won`, 35 `search_abandoned` (above) and 36 `refuge_held` (20.22; region 1, settlement 1, caused, roles 1 `attacker` and 2 `defender` civilization 1–1, value `reason`), appended after 33; `history.report` entries; `docs/specs/schema.md`.
- Invariants (adds to 20.13):
  - (7) every `peace` follows an ending by exhaustion, and its loser was exhausted;
  - (8) no `peace` or `war_won` has as loser the winner of its war's last `battle` or `battle_at_pass`, unless the ending was by aim (the goal no longer the defender's);
  - (9) every battle's `loser_dead` ≤ the victors' local force × `kill_max_permille` div 1000 (the reference recomputes the force);
  - (10) every `search_abandoned` follows `search_years` searches without a find (the reference counts them);
  - (11) no `battle` or `siege` lies in a region where, at that year, the defender's only standing settlements were concealed, unless the search of that year found them (the reference replays the draw).
- Tests (adds to 20.14):
  - the score with dead and people taken (s = 0 a truce); the exhausted leader as winner (`war_won`);
  - the last-battle truce, and the aim exception;
  - `peace` against `war_won` for each ending;
  - the kill bound with F_W = F_L / 20;
  - search found and not found (`find_permille` 0 and 1000), `unfound` reset by a find, the end at `search_years`, a region with a concealed and a visible settlement (no search), and the truce bookkeeping of `search_abandoned`;
  - golden: `p3-06-conflict.txt` re-recorded.

### 20.20 A race's last refuge is never emptied (r10; closes a defect of 20.13 item 2 found by the test writer)

The test writer's stress cases 1, 12 and 14 (`origin/p3/09-tests`, fixture tuning, not shipped) end a race. The orcs are their race's last refuge, and the guard holds every siege, but battle casualties bring the refuge below `abandon_min`. P3-05 8.5 (b) then abandons it in the next T1. 20.1 and P3-09 10 protected flights and tributes, not casualties; local levies draw every battle's dead from that one settlement. Two rules close it:

1. **Casualties** (7.4): when c is its race's last refuge (one standing settlement, the only alive civilization of its race), dead = min(dead, P − `abandon_min`), at least 0. The battle's recorded values (`winner_dead`, `loser_dead`) stay as computed (16.0 reading 3); the war's dead add the people actually removed.
2. **Abandonment** (P3-05 8.5 (a) and (b), amended in P3-05 21 item 6): a settlement that is its race's last refuge is not abandoned, by chance or by the rule below `abandon_min`. It stays standing with its few people and grows by 9.1, or recovers.

   This is wider than P3-08's rule: "a civilization's last settlement is never abandoned by chance" (P3-05 r6 8.5 (a)) is per civilization and only about the chance. This rule is per race, deterministic, and covers every cause (P3-05's famine and plague, P3-06's casualties).

   P3-05 8.5 item 6 then never ends the race's last civilization by abandonment, so with 20.1, 20.3 and item 1 no race ends by any rule (20.13 item 2 holds for all legal tuning).

The bound on the defeated's dead (20.16) lowers losses but does not close the gap: a refuge that loses battle after battle still reaches `abandon_min` without item 1.

### 20.21 Readings of the test writer, settled (r10)

The test writer's reference (`p3_conflict_reference.py gaps`, `origin/p3/09-tests` d45dd89) took these readings where the text was open. Each is **confirmed** as the rule unless corrected:

1. 20.1 presence (reworded in r12, the lead's ruling on the engineer's and the test writer's readings). `presence(c)` has two parts, and only the second looks at invisible settlements:
   - (a) the regions of c's standing **visible** settlements that are not hidden. A colony peoples founded earlier in the same step is not visible (8.3 item 4), so it puts no region into presence;
   - (b) the regions c **holds**, except a held region in which c has a hidden settlement. For "has a hidden settlement" every standing settlement of c counts, visible or not.

   Nothing else in presence counts an invisible settlement. **Q5 is a step of its own** (P3-04 6.2), so colonies peoples founded in Q4 are visible in Q5: the forces that 20.6's odds read for the tension (5.4) and the flare pair count them.
2. 20.1: the cap of h_x(D) stays `march_hops` + 1 for every pair, also when the pair's fronts use `enmity_hops` (20.8). Confirmed: the flight flees the attacker's ordinary reach.
3. Hidden settlements (fled refuges; from r10 not concealed ones, which are in presence) still count for 7.7 item 2's aim test, for the transfer of "each standing visible settlement" in a cession (7.7, 20.3; from r13 the transfer takes every standing settlement, 20.27) and for casualties (7.4). They are excluded only from presence, siege targets, claim causes (5.3) and the tribute's n (20.3). Confirmed.
4. 20.2 item 2: a dropped claim removes a's claim on b only, not b's on a. Confirmed.
5. 20.3: the cession candidates come from `front(W, L)`, and regions the loser is weary of (20.4) are not excluded (weariness bars claims, not peace terms). In a quiet step the war's end waits for the loser's budget whenever any cost would follow. Confirmed.
6. 20.4: the count updates at the war's end event (`peace`, `war_won`, `truce` or `search_abandoned`). Confirmed.
7. 20.6 item 3: the odds use the claim's region before the goal is re-chosen (20.2 item 2); the defence is 1000 when that region is not in `front(a, b)`. Confirmed.
8. 20.7: a settlement's race is its owner's race (P3-05 `race_of`). Confirmed.
9. Report order: kinds 32, 33 after P3-08's 31, then 34, 35, 36 (r10); P3-07 9's `events_<key>` lines follow ids. Confirmed.

### 20.22 A refuge that holds says why (r10; makes T3 checkable)

When the guard holds a fallen last refuge (20.1 item 2), the step writes **36 `refuge_held`** in place of the `siege`:

- region and settlement t; roles 1 `attacker`, 2 `defender`;
- value `reason`: 1 = no flight destination, 2 = too few survivors to settle;
- caused by the battle; salience 25;
- template `{attacker} besieged {@settlement} of {defender}; with {reason?nowhere to flee|too few to flee}, its people held on.`

Every other siege, including a last refuge's siege whose draw said it held, stays a `siege` with `fell` 0. 14 item 3 and 20.13 item 3 read "a `siege` or `refuge_held`" where they name the siege a fall is caused by (a `refuge_held` is never followed by a fall).

**T3 from the rows.** Every `refuge_held` of reason 1 must have no free, non-islet region within `flight_hops` of R_t whose capacity for the defender's race is at least `expand_min_capacity` and that has a site. The reference recomputes this from the tables and the data (region owners at that year from `region_control`). Validation counts `refuge_held` by reason and runs the reference's check. A refuge that falls into flight is visible as a `refugees_settled`.

### 20.23 Weariness counts wars in progress (r11; amends 20.4 and 20.2; the lead's ruling on the engineer's T2 finding)

The implementation of r10 (`origin/p3/09-impl` 845793e, generator 0.18.0, 1024 seeds 0–39) met every target but T2: 4 of 40 worlds had a run above 3 wars over one (defender, goal) without a change of owner (at most 6).

The cause: 20.4's count rises only when a war ends, and 20.2 checks it at the claim and the declaration. So several attackers can each claim and declare over the same (defender, region) while the count still reads 2. Most such runs were against faerie regions, which several hordes search in turn and give up (seed 2: four `search_abandoned` wars over one region).

**Rule** (the lead's ruling, 2026-11-20). Let `futile(D, G)` be 20.4's count as read today (reset by any change of G's owner), and `open(D, G)` the number of wars open at that moment with defender D and goal G, by any attacker. D is **weary of** G, for every claimant, while `futile(D, G) + open(D, G)` ≥ `weary_wars`. This applies at both points where weariness is read:

1. **Claims** (5.3 with 20.2 item 1): a weary G is not an allowed goal, so no claim on G against D is raised.
2. **Declarations** (7.2 with 20.2 item 2): when the claim's region is weary at the declaration, the goal is re-chosen among the allowed goals, or the claim is dropped and no war is declared.

`open(D, G)` counts a war from its `war_declared` until its ending event (any of 4.3's war-closing events and 20.15's, 20.17's endings), and by its stored goal (7.1; a goal re-chosen at the declaration is the war's goal). It is read live: a war declared earlier in the same T2 counts for a later declaration in that T2, in ascending attacker order (7.2). Nothing else about weariness changes: the count, its reset, the cession candidates of 20.3 (not restricted by weariness) and `weary_wars` = 3.

**Invariant** (adds to 20.13): (12) for every `war_declared` with defender D and region G, the number of wars over (D, G) declared earlier that ended leaving G's owner unchanged since the last change of G's owner, plus the number of wars over (D, G) still open at that declaration, is below `weary_wars`. The reference recomputes it from the log and `region_control`. In consequence `wars_same_goal_unchanged_max` ≤ `weary_wars` in every world, and T2 becomes a property of every world rather than a sweep target. (r13: this holds only with the statistic's definition of 20.28, which counts the same wars as this count; with P3-07 21.8 item 5 as written before r13 it failed in 4 worlds of the 0.18.0 validation, because a war closed by a side's end is in that run but never in the count.)

**Tests** (adds to 20.14): two attackers, one defender, one region; the second attacker cannot claim or declare while the first's war is open and the count is 2; it can once that war ends with a change of owner, or once the count is reset; a declaration in the same T2 by a lower-id attacker blocks a higher-id attacker's declaration over the same (D, G); with `weary_wars` 0 nothing is blocked.

**Not measured by the spec writer.** No prototype run was made for r11 (the lead's instruction); the engineer's implementation measures it.

### 20.24 Readings of the engineer, settled (r11)

Each is **confirmed** as the rule:

1. 20.22: with `refuge_flight` = 0, a fallen last refuge held by the guard writes `refuge_held` with `reason` 1 (no flight destination: flight is off, so there is none).
2. 20.17 item 1: the search considers the attacked side's standing, visible, not hidden settlements in the battlefield region; when there are none, no search is made (the year goes on as 7.3, and the battle, if any, has no siege target).
3. 20.17: after a failed search that does not end the war (`unfound` < `search_years`), 7.7's end checks still run in that T6, so a war can end the same year by exhaustion, aim, no front or length.

### 20.25 Statistics notes (r11; text only, from the P3-08 validation)

1. **Typical ranges of 15.5 predate P3-08.** P3-08 halved famine and plague spread and merged quiet eras, so 15.5's ranges no longer describe a default world. The P3-08 validation suggests, from the 5th–95th percentiles of 100 default 1024 worlds at 0.17.0: events 400–1,250, wars 9–53, flare-ups 0–6. These move again at 0.18.0 (20.18: more wars, the searches), so **every typical range of 15.5 and 20.11 is to be reset from the 5th–95th percentiles of the P3-09 validation sweep at 0.18.0**. Until then, an outlier from 15.5 is not a finding. (r13: done, 15.5 and 20.29.)
2. **Plague in the summary.** P3-08 predicted that plague would be 121‰ of the summary's events after its salience change (P3-05 r6 10.2). The P3-08 validation measured 83‰. The summary figures of P3-07 21 and P3-09 5.7 do not depend on that prediction; validation reports the measured share at 0.18.0.

### 20.26 War lines of the golden (r12; amends 16.0 reading 12 for r10)

The reference's war lines (16.0 item 12; `p3-06-conflict.txt` (b)) gain one `<how>` word and two outcome forms, as the test writer's reference writes them on `origin/p3/09-int`:
- `<how>` **`unfound`**: the war ended by `search_abandoned` (20.17); its `<outcome>` is **`unfound`**.
- `<outcome>` **`won-c<W>`** for a war won by W, whether its ending event is a `peace` or a `war_won` (20.15); the ending's kind is in the event lines.
  - **`-ceded`** is appended for a cession of 7.7 or 20.3;
  - **`-tribute`** is appended for a tribute (20.3).
- An exhausted leader's win (20.15 item 2) is `exhaustion` with `won-c<W>`.

Other words unchanged: `exhaustion`, `aim`, `no-front`, `length`, `quiet`, `conquest`, `ended`, `open`; outcomes `truce` and `-`. A `refuge_held` (20.22) is not a war ending and does not appear in a war line.

### 20.27 A cession passes every settlement of the loser in the ceded region (r13; amends 7.7 "Cession", 20.3 item 1 and 8.3 item 4; closes defect D1 of the P3-09 validation)

**The defect** (`docs/reviews/p3-09-conflict-balance-validation.md` D1; 2048 seed 3, default settings, 0.18.0). In 5225, in war e1109 (goblins c15 against dwarves c10, goal region 138), c10 sacks c15's town s168 in region 158 (e1122), and 158 is released. In the quiet step of 5234 peoples founds colony s240 of c15 in the free region 158 (e1138), so c15 holds 158 again. Later in the same Q4 the war ends in `war_won` for the defender c10 (e1141), and 20.3 item 1 cedes c15's best non-capital region in front, 158 (`region_ceded` e1142). s240 was created in that step, so it was invisible (8.3 item 4) and did not pass: c10 held 158 from 5234 to 5994 with no settlement in it, while s240 stayed c15's and grew to a town. This breaks 14 item 5 and P3-05 14 item 4. r7's text (8.3 item 4) assumed that in 7.7's goal cession the winner gets a visible settlement of L in G. That is false when L's only settlement in G is the same-step colony: L's earlier settlement there was sacked and G released, then L recolonized G in the Q4 that ends the war. 20.3 widens the gap, because it cedes any held region in front.

**Rule** (the lead's ruling, 2026-11-20: ownership is a fact, not a matter of visibility).

1. **What passes in a cession.** In every cession of a region G from L to W (7.7's cession of the goal and 20.3 item 1's), the settlements that pass are **every settlement of L standing in G** at the cession: visible ones, hidden ones (20.1) and those created earlier in the same step (a colony of P3-05 9.3 founded in this Q4). Procedure, after appending `region_ceded`: take the id list of the settlements in `State` (all of them, not only `visible()`) with owner L, region G and status standing, ascending (8.3 item 1: the list before any change); for each, re-fetched by id, its excess over 2 × C(G, r_W) goes to L's nearest other standing settlement (P3-05 8.5 item 2, whose destinations already include settlements created earlier in the step) and `set_settlement_owner(s, W, region_ceded)`; then `set_region_owner(G, W, region_ceded)`. This is a stated exception to P3-04 6.2's visibility, in the sense of P3-04 6.9 item 4: it chooses nothing, it records what the peace gave away. **Reading (a)** (the test writer's, confirmed): the excess's destination is read live as each settlement passes, in ascending id. So it may be a settlement of L in G with a later id that has not passed yet; that settlement then carries the excess with it when it passes, subject to its own excess test. 7.7 already reads it this way. Nothing else in 7.7 or 20.3 changes: the candidates, the choice of G', the tribute alternative, `wronged`, the quiet-step wait for both budgets.
2. **The other ownership tests.** The same reading holds for every test that decides a region's owner from a civilization's standing settlements in it: the capture's conquest of the region (7.5 item 3, "has no other standing settlement in R"), `ruin`'s release (P3-05 8.5 item 4, for a sack, 7.5 item 2, and a flight, 20.1 item 3) and the conquest of a civilization (7.5 item 5, 7.6, "no standing settlement left"). Each counts every standing settlement. These run only in T6, where peoples founds nothing. The one settlement created in T6 is a fled refuge h (20.1). h is its civilization's only standing settlement, stands in a region that was free, and is hidden, so no later siege, sack or capture of the same T6 concerns its owner in h's region, and its owner is not conquered in that step. The visible reading and the full reading therefore agree in every reachable state: **item 2 changes no world byte and needs no code change**. P3-05's own abandonments (T1, T4, Q1) run in steps where peoples founds no settlement (P3-05 13.1 item 1), and no peoples rule changes.
3. **Not affected.**
   - **Tribute** (20.3 item 2): it moves no settlement and no region. Its n and its casualties stay over L's visible, not hidden settlements, and its last-refuge test is unchanged.
   - **Flight destinations** (20.1): "a region with no owner" already reads the live owner.
   - **Presence and every choice of conflict** (claims, fronts, goals, battlefields, siege targets, the cession candidates, the odds): unchanged. The lead's earlier ruling that a not-yet-visible colony puts no region into presence (20.21 item 1) stands. In the seed-3 case region 158 stays a 20.3 candidate, by presence part (b) (c15 holds it), and the cession now carries s240 with it.
   - **7.7's goal cession** with a same-step colony of L in G (r7's case of 8.3 item 4): the colony now passes with any visible settlements of L in G, so L keeps no settlement in a region it ceded. When the colony was L's only settlement in G, 0.18.0 left W holding G with no settlement there, the same defect as 20.3's (item 9).
4. **Events and their order.** No new event and no new kind; schema 7. In the Q4 of the case, in order:
   1. peoples: `colony_founded` (region G, settlement s, founder L), which makes L the holder of G if it was free;
   2. conflict, after housekeeping (7.8): the war's ending (`peace` or `war_won`), then `region_ceded` (region G; L `from`, W `to`; cause the ending), then the transfers in ascending settlement id and the region's owner, all with the `region_ceded` as their event (no event of their own; the excess moves silently, as in 7.7).

   The `region_ceded` is the record of every settlement that passes, visible or not. The colony's `colony_founded` keeps L as founder and stays its `founding_event`. The step's bookkeeping writes W as its `owner_civ` (`settlement`, `settlement_census`) and opens W's `region_control` row for G with the `region_ceded`. When L's hold began in the same step, as in the case, L has no `region_control` row for G (P3-04 6.7 item 1's net rule), so a check that L held G reads the log (the step's `colony_founded`), not `region_control`.
5. **When the colony becomes visible.** From the next step (Q5, a step of its own, P3-04 6.2) it is visible, and it is an ordinary settlement of W, like any settlement passed by cession (P3-05 13, "Ownership changes by P3-06"). It counts in W's presence (part (a)), force, population and casualties, and in the forces Q5's tension reads. Its race is W's (20.21 item 8), so P3-05 grows or shrinks it toward W's share. Its kind stays the kind it was founded with and changes only by P3-05 8.2 (P3-05 14 item 6: a ladder kind built by a race that owned it). No event marks the moment. L never acts on it as its own: invisible to L's rules in the step it was founded, W's from the next.
6. **Invariants.** 14 item 5 and P3-05 14 item 4 hold without exception: a region's owner has a standing settlement in it after every step. New invariant (13), added to 20.13: right after each `region_ceded` of a region G with `from` L and `to` W, L has no standing settlement in G, and every settlement of L that stood in G just before it is W's. With 7.5 item 3 and `ruin`'s release, a civilization that loses a region to P3-06 keeps no standing settlement in it (r7's exception in 8.3 item 4 no longer exists). The reference checks (13) by replaying settlement owners from the log: foundings, captures, cessions (every settlement of `from` standing in the region), ruins and **`civ_seceded`** (reading (b), the test writer's, confirmed: a secession passes settlements to the new civilization, so a replay without it has the wrong owner at a later cession).
7. **The seed-3 case under r13.** The log is unchanged up to and including e1142: the transfer writes no event. s240 passes to c10 with e1142; any people above 2 × C(158, dwarf) join c15's nearest other standing settlement. c10 holds 158 from 5234 with the `region_ceded`, as before, but now with s240 in it, and invariant 14 item 5 holds. From Q5 of that step s240 is a dwarven settlement of c10. The world differs from 0.18.0 as validated from that step's bookkeeping on (s240's census owner, then everything that depends on it). The reference's `check-world` must report no failure.
8. **Tests** (adds to 20.14).
   - A Q4 in which L founds a colony into a free region in `front(W, L)` and its war then ends in a `war_won` for W: 20.3 cedes that region, the colony passes to W with the `region_ceded`, and no event is added.
   - 7.7's goal cession with one visible settlement and one same-step colony of L in G: both pass, in ascending id, with the excess to L's nearest other settlement.
   - A same-step colony of L in a region that is not ceded stays L's.
   - Invariant (13) on the property worlds of 16.2 and on 2048 seed 3 (slow suite).
9. **Effect on output.** Bytes change only in worlds where a cession, by 7.7 or by 20.3, meets a settlement of the loser in the ceded region that was created earlier in the same step. Both kinds of cession left a region with no settlement of its holder at 0.18.0. Found so far:
   - 20.3: 2048 seed 3 (above);
   - 7.7: the golden world `hist-conflict-flight` (512 seed 1: colony s42 by e351, cession e354, year 4878), 1024 default seed 5 (e262, s61) and `civs4` seed 13 (e1332, s167).

   That is 4 of 53 sampled worlds, so the validation's "once in 375" undercounts. **Validation's re-run reports the final count.** The golden `hist-conflict-flight` changes. **Generator stays 0.18.0** (not yet released, P3-09 6). After the engineer's change, validation re-records the world hashes and golden vectors that change and re-runs the cross-preset comparison; a golden case without the situation keeps its value.

### 20.28 Which wars weariness and T2 count (r13; amends 20.23 and P3-07 21.8 item 5; closes defects D2 and D3 of the P3-09 validation)

**The defect** (validation D2). 20.23 claims that `wars_same_goal_unchanged_max` ≤ `weary_wars` in every world. It was 4 in 1024 seeds 6 and 77 and in `--civs-per-race 4` seeds 0 and 13, while invariant (12) held. 20.4's count never counts a war closed by a side's end, but P3-07 21.8 item 5's run counted every war. Weariness (20.4, 20.23, invariant (12)) is right as specified; the statistic counted different wars.

**One definition** (the lead's ruling: the run counts exactly the wars weariness counts). A war is **closed by a side's end** when the `until_event` of its `war` row is a `civ_conquered` or a `civ_ended` (7.6, 8.1 item 1), of either side. Weariness treats such a war as follows, unchanged by r13:
- at its close 20.4's count does not rise (20.4 counts the endings `peace`, `war_won`, `truce` and `search_abandoned`, 20.21 item 6);
- its close does not reset the count (only a change of G's owner does);
- while it is open it counts in `open(D, G)` (20.23).

So it neither adds to the count nor clears it. The statistic follows: **a war closed by a side's end neither extends a run nor breaks one**. It is left out of the sequence, and the wars on either side of it are consecutive. A change of G's owner breaks a run only through the window below, whichever war's span it falls in. The weariness rule is not changed, so no world byte changes; the statistic is a report key (P3-07 21.8 item 5, r11 there), and so is T2.

**The statistic** (normative text in P3-07 21.8 item 5). For each (D, G), the *counted wars* are the `war_declared` events with defender D and region G, in id order, without those closed by a side's end; a war open at the present is counted, with until = E. A run is a sequence of consecutive counted wars w_i … w_j such that no `region_control` row of G has `from_year` or `to_year` in the **window** [since(w_i), max(until(w_i), …, until(w_j))]. The key is the longest run over every (D, G), 0 when there is none. The window's end is the latest end in the run, not the last war's end: wars over one (D, G) can overlap (20.23 allows two open at once), and with the latest end a part of a run is always a run, so the incremental count of P3-07 21.8 item 5 finds the longest.

**Why T2 then follows** (with `weary_wars` ≥ 1). Take a run w_1 … w_k over (D, G), declared in that order. At w_k's declaration each earlier w_i is either open or ended. If it is open, it counts in `open(D, G)`. If it ended, it ended by `peace`, `war_won`, `truce` or `search_abandoned`, since it is not closed by a side's end. No owner change lies in the run's window, which contains [since(w_1), since(w_k)]. So w_i left G's owner as it was at its declaration, 20.4's count rose at its end under the current owner, and no reset followed. Hence `futile(D, G)` + `open(D, G)` ≥ k − 1 at w_k's declaration. Invariant (12) gives k − 1 < `weary_wars`, so k ≤ `weary_wars` (3). A war open at the present is in the run (until = E). It is also in `open` at any later declaration, so the bound covers it.

**Seed 6 worked through** (1024, default settings; region 40, defender c1; c1 holds 40 throughout, so no `region_control` row of 40 changes in 4892–5597):

| War | Years | Ending | In the run? | Count at its declaration (futile + open) | Count after its end |
| --- | --- | --- | --- | --- | --- |
| e341 | 4892–4922 | `war_won` | yes, 1 | 0 + 0 | 1 |
| e445 | 5297–5309 | `peace` | yes, 2 | 1 + 0 | 2 |
| e453 | 5310–5338 | `civ_conquered` e488 (the attacker's end) | **no: skipped** | 2 + 0 | 2 (not counted, not reset) |
| e538 | 5591–5597 | `truce` | yes, 3 | 2 + 0 | 3: c1 is weary of 40 |

`wars_same_goal_unchanged_max` = 3 (it was 4 by 21.8 item 5 as written, which counted e453). Every declaration had a count below 3, so invariant (12) holds. Seed 77 (three `search_abandoned` and one `civ_conquered`) and `civs4` seeds 0 and 13 (each with one war closed by `civ_conquered`) have the same shape and give 3. Validation confirms this at the re-record.

**Where a run starts** (validation D3; the reference's undercount). The first war over (D, G) after a change of G's owner, by a war not closed by a side's end, starts a **run of 1** if no change lies in its own span [since, until]. A war whose own span contains a change belongs to no run (0), and the next counted war starts again at 1 under the same test. Example (1024 `calm` seed 17): e285 (5577–5587) and e297 (5588–5598) over region 11 against c1, with region 11's rows changing at 5039 and 5857 only: one run of 2, not 1. The change at 5039 precedes both wars; it ends the run before them and does not stop e285 from starting one.

**Tests** (adds to 20.14): the statistic on a hand-made log with a war closed by a `civ_conquered` between two `peace` wars (run 2, not broken), with that war's span containing a change of owner (run broken), and with a change of owner between two wars (the second war starts a run of 1); the run of three overlapping wars where the middle war ends last (20.28's window, P3-07 21.8 item 5).

### 20.29 Typical ranges at 0.18.0 and known observations (r13; from the P3-09 validation 5.4 and F1, F3)

1. **15.5's typical ranges are reset** to the 5th–95th percentiles of the 100 default 1024 worlds of the 0.18.0 validation (15.5, r13 table). This closes 20.25 item 1.
2. **20.11's ranges at 0.18.0** (1024 defaults, 100 worlds): `wars_hollow_permille` 229‰ over the sweep (met); `wars_same_goal_unchanged_max` ≤ 3 in 98 of 100 worlds by the old definition, and in every world by 20.28's, where it is an invariant; faeries with one standing settlement in 0 worlds and elves in 2 (met); `race_population_share_max_permille` below 700 in **73** of 100 worlds against "at least 75" (F1, below).
3. **T1 is defined at 1024 with default settings only.** T1 (P3-09 5) and 20.11's "`wars_hollow_permille` below 250" are the ratio over the 1024 default sweep, hollow wars over the wars that found their enemy. No target is set for any other map size or setting. Measured at 0.18.0 (validation F3), above 250 elsewhere: 512 293‰, span 2000 327‰, `elf,orc` 314‰, `elf,goblin` 256‰, `calm` 253‰. At 1024 defaults, 41 of 100 worlds are individually at or above 250. These are known observations, not findings against T1.
4. **Known observation F1, no rule change.** A single race holds at least 700‰ of the people in **27 of 100** default worlds (0.17.0: 18); the goblins in 21 of them, the humans in 6. The costs of war move people toward the strong, as P3-09 decision 8.4 expected. 20.11's range ("below 700 in at least 75% of worlds") is narrowly missed and kept as the aim. The observation goes to the world critic and to Paul's review of P3; no rule is changed for it here.

## 21. P3-10 follow-ups: wording and five conflict rules (amendment r14, P3-10, generator 0.19.0)

Normative for the change P3-10 (`docs/specs/p3-10-followups.md`), from generator **0.19.0**, not before. 21.1 to 21.5 are wording and code tasks; 21.6 to 21.11 are five rule changes after the world critic's review of P3-09 (`~/.cache/the-unmapped/out/p3-09-validation/review-p3-09-conflict-balance.md`, F1 to F4 and F6), decided by Paul on 2026-11-20 (`docs/design/proposals.md`, "Paul's decision of 2026-11-20 on the six questions from the P3-09 review"). No new event kind; no schema change.

### 21.1 A reduction is noted only where people were lost (amends 7.4)

7.4 calls `note_reduction(s, e)` for every settlement of the side, also one that lost no one (its share of the dead rounded to 0), so a battle far away became the "last reduction event" of a town that lost nobody, and a later dwindling or abandonment cited it (`docs/reviews/p3-08-followups-world-critic.md` F9: seed 42 at 0.17.0, e246 "Glathenddell dwindled to 1142 souls" cites e226, a battle 218 years earlier). From 0.19.0: `note_reduction(s, e)` is called for a settlement s of ss **only when its loss is at least 1** (its population after the casualties is below its population before). The casualties, their distribution and the war's dead are unchanged; tribute (20.3, through `casualties`) follows the same rule. With P3-05 22.2's horizon this bounds every cited reduction to one that took people from the settlement within the last `cause_memory_years`.

### 21.2 Battle sentences without "0 of the victors" (amends 11's templates of kinds 22 and 23; data)

With P3-04 r10's three-word form:

```
battle:         {winner} defeated {loser} in {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}.
battle_at_pass: {winner} defeated {loser} at the pass into {@region}; {loser_dead?none|one|#} of the defeated{winner_dead? fell, and none of the victors| and one of the victors fell| and # of the victors fell}.
```

Renderings: `…; 491 of the defeated and 14 of the victors fell.` (unchanged but for P3-04 20.3's separators), `…; 4 of the defeated fell, and none of the victors.`, `…; 37 of the defeated and one of the victors fell.` Measured at 0.18.0 (1024 default, seeds 0 to 39): 1 of 3,078 battles has `winner_dead` 0 and none has `loser_dead` 0 (at 0.17.0, 32 sentences of the 375 sweep worlds read "0 of the victors fell"); the rule is for the wording, the counts are P3-09's.

### 21.3 The flight plan copies its hop row (P3-09 audit advisory A1; no output change)

`flight_plan` (`worldgen/src/history/conflict_war.cpp`) holds `const std::vector<std::int64_t>& from_t = view.hops(from)` across further calls of `view.hops(r)`, which may fill other entries of the lazily filled cache `PeoplesGeo::hops_` (`docs/reviews/p3-09-conflict-balance-determinism-audit.md` A1). It is safe while the cache's outer vector is sized once at construction; from 0.19.0 the code does not depend on that: `flight_plan` takes a copy of the row (`const std::vector<std::int64_t> from_t = view.hops(from);`), and `peoples.hpp` states next to `hops_` that its outer size is fixed at construction and that a reference returned by `hops()` stays valid for the subsystem's life (which `local_force`, holding one row without a second fill, relies on). No world byte changes; the determinism auditor confirms on the audit's eight cases.

### 21.4 One last-refuge predicate (P3-09 audit advisory A2; no output change)

`ConflictSubsystem::last_refuge_of` and `PeoplesSubsystem::race_last_refuge` state the same predicate twice. From 0.19.0 there is one: P3-05 22.7 defines it in the peoples subsystem, and `casualties` (20.20 item 1) and the tribute exemption (20.3) call it with the live state. No world byte changes.

### 21.5 Tests (amends 16.1)

1. 21.1: a side of three settlements of 1000, 1000 and 10 with 2 dead: the losses are 1, 1, 0 (7.4's distribution); `note_reduction` is called for the first two only, so the third's last reduction event is unchanged (a later dwindling of it does not cite the battle).
2. 21.2: the battle sentence with (`loser_dead`, `winner_dead`) = (4, 0), (37, 1), (491, 14), (0, 0) renders `4 of the defeated fell, and none of the victors.`, `37 of the defeated and one of the victors fell.`, `491 of the defeated and 14 of the victors fell.`, `none of the defeated fell, and none of the victors.` after the `; `.
3. 21.3, 21.4: the P3-09 golden lines and the audit's eight worlds are byte-identical before and after the change (with the 0.18.0 data: the refactoring is checked on its own commit, before the data and rule changes of P3-10).

### 21.6 A war is won by aim only by holding the goal (amends 7.7 item 2 and 20.15 item 3; the P3-09 critic's F1; Paul's decision 1)

**The fault.** 7.7 item 2 ends a war as won by the attacker when the defender b neither holds the goal G nor has a standing settlement in it, and 20.15 item 3 exempts such an ending from the last-battle rule. So when a *third* civilization took G, or G was emptied, the attacker "prevailed" and collected 20.3's tribute or cession even after losing the fighting (`~/.cache/the-unmapped/out/p3-09-validation/review-p3-09-conflict-balance.md` F1: 42 such endings in the 100 default worlds of the 0.18.0 validation, 19 with nothing taken, 7 with more battles lost than won; seed 23 at 1024, e893: "The Warrens of Kaggdrark prevailed over the Hold of Muli after 23 years of war" with 1,175 carried off, after losing three of four battles while the Urarz Horde took the goal).

**Rule** (from 0.19.0; replaces 7.7 item 2). At each check of 7.7 (after 7.3 in T6) and at 7.8's end of an active phase, for an open war of attacker a and defender b over G, with o = G's owner (`RegionState.owner`) at the check:

1. **Aim achieved**: o = a. The attacker holds its goal: it wins, as 7.7 item 2 and 20.15 item 4 said (`war_won`, or `peace` when b is exhausted), and 20.3's cost applies (G cannot be ceded, since b does not hold it: the loser's best non-capital region in reach, else tribute). The last-battle rule does not apply: a defender that lost its goal to the attacker has lost.
2. **Goal lost to no one or to someone else**: o ≠ a, and b neither holds G nor has a standing settlement in it (o = 0 after a sack or an abandonment, or o a third civilization after its own capture, cession or flight into G, or after a secession took G). The war has lost its cause and **ends now by the score**: 20.15 items 1 and 3 decide it like any ending by score (s > 0 a wins, s < 0 b wins, s = 0 a truce; the winner of the war's last battle never loses: a truce instead). The ending kinds follow 20.15 item 4 (`peace` only when the loser is exhausted, else `war_won`; a truce is `truce`). 20.3's cost applies only to a loser of a `peace` or `war_won` written by this rule: no tribute or cession when it ends in a truce. Cause: the war's last event, as for every ending.
3. Otherwise (b still holds G or stands in it): 7.7 items 1, 3 and 4 as before.

**Open wars when G changes hands to a third party.** Nothing happens at the change itself: no event, no war closed. Every open war whose goal is G and whose defender has now lost G ends at its own next check, item 2 (its T6 if the change was in a tick, before its next battle is fought only if the change came earlier in the step's war loop: the order of 8.2's T6 loop over the wars by index stands; in a quiet step at 7.8's end of the active phase). A war whose attacker is the new owner ends by item 1. A war whose defender is the new owner (c took G from b, and c is the defender of another war over G) is unaffected: c holds G. Two wars of different attackers against b over G both end by item 2, each by its own score.

**Invariant (8) of 20.19** reads from 0.19.0: no `peace` or `war_won` has as loser the winner of its war's last `battle` or `battle_at_pass`, unless its winner is the attacker and held the war's goal at the ending. **New (14)**: no `region_ceded` or `tribute` is caused by an ending in which the attacker won while not holding the goal, unless that ending is by the score of item 2 with the attacker as the score's winner and the winner of the last battle.

**Measured** (estimate from the stored 0.18.0 logs, 1024 default, seeds 0 to 39; the score recomputed from the battles' dead and the people of the settlements taken; the rule's knock-on effects on later events are not simulated): of the 315 endings in which the attacker won while the defender no longer held the goal, 174 had the attacker holding it (aim, unchanged), 123 had it held by no one (the attacker's own sack had emptied it) and 18 by a third civilization. Under the rule, 120 of the 123 and 12 of the 18 stay attacker wins (by the score, the attacker having won the last battle), 7 become truces and 2 defender wins; 9 tributes or cessions are no longer paid. The critic's blatant cases (seed 23 e893 and its kind) are among the truces. **Target** (validation at 0.19.0): invariant (8) as amended and (14) in every world.

### 21.7 A failed search makes the wood weary, and only the first is news (amends 20.17 and 20.23; F2; Paul's decision 2)

Three rules, from 0.19.0:

1. **A failed search makes its region weary for a time.** When a war ends by `search_abandoned` in region R against hidden civilization y (20.17 item 2), y is **weary of R for every claimant** until year Y + `search_weary_years`, whatever the counts of 20.4 and 20.23 read: R is not an allowed goal against y (20.2) in a claim or at a declaration (20.23 items 1 and 2). New member `searched_until[(y, R)]` (ordered map; the latest search sets it). It is also cleared by a change of R's owner (21.8's sense: another civilization holds R). The 20.4 count still rises for the search as before. New `history.conflict` field **`search_weary_years`** int(0, 10000), shipped **500** (taste; 0: rule off), appended after `search_years`. A faerie wood gets a reputation: one hunt finds no one, and for 500 years no one tries again there. Other regions of the same court can still be searched.
2. **No search where the war has fought.** In 20.17 item 1, a battlefield R in which this war has already fought a `battle` or `battle_at_pass` needs no search for the rest of the war: the army knows the way, and the year goes on as 7.3 (battle draw, then the siege, which may take a concealed settlement). So such a war never ends by `search_abandoned` in R; it ends by 7.7's rules as amended in 21.6 (exhaustion, aim, no front, length, the score with the last-battle rule). "Found no one" is never written after a battle in that wood (at 0.18.0, 67 of the 437 searches of the 100 validation worlds were), and no second template is needed. The `unfound` count of a war is kept per war as before; a found region resets it.
3. **Only the first failed search reaches the summary.** The salience of a `search_abandoned` is fixed at its append: the kind's base salience (40, `history.report`) when no earlier event of kind 35 is in the log, else **`search_repeat_salience`** (new `history.conflict` field int(0, 100), shipped **15**, taste; appended after `search_weary_years`), below P3-07's `SUMMARY_MIN_SALIENCE` 20. The rule is W8's (P3-04 6.6: salience at append) and reads only the log; like the outbreak's toll rule (P3-05 10.2) it changes a value, not a kind. P3-07 r12 22.7 keeps the declaration of such a war out of the summary with it.

**Measured** (estimate from the stored 0.18.0 logs, 1024 default seeds 0 to 39, dropping a search when the same region had a counted search within the weary years; the wars that would be declared elsewhere instead are not simulated): `search_abandoned` per world 5 / 12 (median / max; 184 in 40 worlds) at 0.18.0; with item 2 alone 156; with items 1 and 2 and `search_weary_years` 300, 500 or 1000: median 3, 2, 2 (max 8, 8, 6; 110, 103, 89 in all), worlds with 4 or more 11, 9, 6 of 40. 500 is shipped. With item 3 at most one `search_abandoned` per world has salience 40. **Targets** (validation at 0.19.0, 1024 default): median `search_abandoned` per world at most 2; no `search_abandoned` in a region where its war fought a battle; at most one `search_abandoned` per world in the default summary (P3-07 21).

### 21.8 Resettlement by the same owner is not a change of owner (amends 20.4, 20.23, 20.28 and P3-07 21.8 item 5; F3; Paul's decision 3)

**The fault.** A sack empties a region (`ruin` releases it, P3-05 8.5 item 4), its owner resettles it by a colony, and 20.4 reads the release and the resettlement as changes of owner, which reset the count. Seed 42 at 1024 (0.18.0), the Wood of Stags (region 35), held by the House of Finen (c1) throughout: eleven wars over it, each ending with the region empty; `region_control` has ten rows, all of c1 (1475–2527, 2530–3287, 3291–3846, 3849–4280, 4309–4973, 4992–5578, 5580–5603, 5609–5688, 5744–5781, 5849–5873).

**Rule.** The **holding civilization** of region G in year Y, hold(G, Y), is G's owner if G has one, else the civilization that last owned G (0 if G was never owned). Every "change of G's owner" in the weariness rules means a change of hold(G, ·): G passes to a different civilization. A gap without owner followed by the same civilization is not a change.

- **20.4**: `futile[(b, G)]` records hold(G) at a war's declaration; at its end (peace, `war_won`, truce, `search_abandoned`) the count rises when hold(G) at the end equals the recorded one; a count reads 0 when its recorded holder differs from hold(G) now.
- **20.23**: `futile(D, G)` is 20.4's count read with hold; `open(D, G)` is unchanged.
- **The statistic** `wars_same_goal_unchanged_max` (20.28; P3-07 21.8 item 5, amended in P3-07 r12 22.8): a run over (D, G) is broken by a *change of holder*: a `region_control` row of G whose `civ` differs from the `civ` of G's previous `region_control` row (G's first row is a change). A row of the same civilization after a gap is not a change. So the run counts exactly the wars 20.4 counts, and T2 (`wars_same_goal_unchanged_max` ≤ `weary_wars`) remains a consequence of invariant (12).

**Seed 42, the Wood of Stags, under 21.8.** hold(35) is c1 from 1475 to 6000. The Grork Horde's wars e56 (2524–2527, `war_won`), e114 (3266–3287, `peace`) and e166 (3835–3846, `war_won`) each end with hold(35) = c1 as at their declaration, so the count reads 1, 2, 3; from 3846 c1 is weary of the wood for every claimant, and the eight later wars over it (e206, e288, e423, e438, e455, e540, e590, e681, 4274–5873) are not declared, unless another civilization takes the wood first. (The claims and wars that take their place elsewhere follow from the changed log and are for validation to measure.) The run statistic over (c1, 35) at 0.18.0 by 20.28 breaks at every row; by 21.8 it is one run, which the rule caps at 3.

### 21.9 Orcs declare at shorter odds (amends 20.6 item 3; F4; Paul's decision 4)

New race field **`odds_min_permille`** int(0, 1000) (P3-01 r11 3.5), required, after `home_defense_permille`; shipped **orc 500, every other race 700**. It replaces `history.conflict`'s `odds_min` (removed from the tuning and its fixtures; 12.1 and 20.9 lose the row): in 20.6 item 3, a's declaration on b and the tension's flare pair (5.4, 7.2 item 2) are skipped when odds < `odds_min_permille`(r_a), the claimant's race. Everything else in the odds is unchanged (`declare_odds` is not needed: the odds formula stays). With 700 for every race but the orcs, every non-orc decision is exactly 0.18.0's.

**Target** (new, validation at 0.19.0, 1024 default): **T8**: orcs declare at least one war in at least 90% of the worlds in which they are present in their age (0.18.0: 76 of 100, the critic's F4); **T5** still holds (orc wins ≥ orc losses over the sweep).

**Measured** (the harness of `docs/specs/p3-10-followups.md` 6 at 0.18.0 with the field, 1024 default seeds 0 to 19): worlds (of 20, all with orcs) in which the orcs declare at least one war: 15 at 0.18.0, **16 with orc 500**, 18 with orc 300; orc wins/losses (`peace` and `war_won`) 24/15, **24/18**, 18/22. 300 breaks T5. Of the 9 of 40 stored 0.18.0 worlds without an orc declaration, 5 have no orc claim at all (the orcs never build strain toward a neighbour in their roughly 400 years), which no odds rule reaches; so **T8 is probably not met by this rule alone** (expected about 80%, against 90%; question 1 of `docs/specs/p3-10-followups.md` 10b).

### 21.10 A hidden refuge lies low (amends 20.1 and P3-05 9.3; F6; Paul's decision 6)

A civilization y **lies low** in year Y while one of its settlements s has Y < `hidden_until[s]` (20.1 item 2: its refuge is hidden). While y lies low:

1. **No colony** (P3-05 9.3): y founds no settlement in T2 or Q4, inside or outward. P3-05 learns it through a new hook, `PeoplesSubsystem::lie_low(civ, until_year)`, which P3-06 calls in the flight right after `hidden_until[h]` is set (20.1 item 2), as it calls `note_reduction`; peoples keeps `low_until[civ]` (an ordered map, the latest call wins) and skips 9.3 for c while Y < `low_until[c]` (in a quiet step: while the step's start is below it). No draw is taken for a skipped founding (`hist.peoples.found` and `outward` are not drawn), and `hunger(c)` is left as it is (neither raised nor reset: land hunger is about having no target, and y is not looking).
2. **No claim and no declaration** (5.3, 7.2): y raises no claim and declares no war, and the tension (5.4) skips its pairs as claimant. Its strain toward others is not raised either (5.2), so no quarrel waits for its return. It still **defends**: a war declared on it before its flight goes on; its hidden settlements are out of presence (20.1), any visible ones can be attacked, and it fights the battles brought to it.
3. **Population pressure.** The refuge grows by P3-05 9.1 toward its share (min(C_s, kind cap)) and stops there; above its share it declines by `decline_permille` as any settlement does; famine and plague strike it as any settlement (10.2), and a crowded refuge (k ≥ `famine_crowding_permille`) is at risk of famine. Its pressure (9.3 item 1) builds, and once it stops lying low its first founding follows the ordinary chance. Nothing is stored for the waiting years: no event marks the end of hiding, as before (20.1).

Effect at 0.18.0 (the critic's sweep, 100 default worlds): 7 of 46 flights were followed by a declaration while hidden, and seed 23's Welbrarwick founded four colonies in its hidden years; under 21.10 none of either. **Invariant (15)** (adds to 20.13): no `colony_founded` with `founder` y and no `claim` or `war_declared` with y as `claimant` or `attacker` is dated in [Y_f, Y_f + `hidden_years`) of a `refugees_settled` of founder y in Y_f.

### 21.11 Tests (adds to 21.5)

1. 21.6: a war whose goal a third civilization captures, with the attacker leading the score but the defender winning the last battle: `truce`, no tribute; the same with the attacker winning the last battle: `war_won` and 20.3's cost; the goal emptied by the attacker's own sack (o = 0): by the score; the goal captured by the attacker: aim, `war_won`, last-battle rule not applied; two open wars over one goal both end by the score at their checks.
2. 21.7: a `search_abandoned` in R blocks every claimant's claim and declaration on (y, R) for `search_weary_years`, and not after; a change of R's holder clears it; a war that fought a battle in R does not search there again; the first kind-35 event of a log has salience 40 and every later one 15.
3. 21.8: a region sacked and resettled by its holder keeps the count (seed 42's Wood of Stags in miniature: three wars, each emptying the region, the fourth claim refused); a region taken by another civilization and retaken resets it; the statistic on the same logs.
4. 21.9: an orc claimant at odds 600 declares (chance drawn), a goblin at 600 does not (no draw); the tension skips a goblin pair at odds 650 and not an orc pair.
5. 21.10: a fled refuge founds no colony and declares no war for `hidden_years`, no `found` draw is taken, its growth stops at its share; the first founding after the hidden years.
6. Property tests (16.2): invariants (8) as amended, (14), (15); golden `p3-06-conflict.txt` re-recorded.

## Appendix A. Proposals entries (added to `docs/design/proposals.md` with this spec)

- "History: wars are limited, fought for a claimed region, and end with the active phase" (questions 1 and 2).
- "History: no race is wiped out; newly awakened peoples are sheltered" (question 4).
- "History: a fighting strength per race; goblins less aggressive than orcs" (question 3).

## Revision note r1 (2026-11-19, approval and amendments applied)

Approved by the lead as the basis for implementation. Section 18 applied to P3-05 (r1), P0-01 (19), P3-01 (r7), P3-07 (r3), P3-00 (correction note), P3-04 (r4) and `schema.md` (6), and turned into a record of what was applied. `origin/p3/04-spec-fixes` (13b8062, P3-04 r3, P3-07 r2) merged; the spec checked against r3: new 16.4 (fatal messages, ASCII) and 16.5 (agreement with r3), performance renumbered 16.6. No rule or number changes.

## Revision note r2 (2026-11-19, rules from the P3-04 audit)

New 8.3: how P3-06 meets P3-04 6.9 (the determinism audit's A1, A2, N1–N5). Two rule changes follow from it: (1) **a war has no battle in its declaration year** (7.3), so that no `war` row opens and closes before one bookkeeping (N5; before, 54 of 1,136 wars at 1024 ended in the year they began and would have left no `war` row and an unbroken `peace` row); (2) a cession transfers the loser's **visible** settlements in the goal region (N2; the prototype had included a settlement peoples founded earlier in the same quiet step). A2: strain was already accumulated only in T2 and Q4 and `tension()` already a pure read, in the text and in the prototype; no number depended on it. N3: every draw key is built from ids. The prototype was rerun with both changes; the sample and the sweep statistics of 15 are the new figures.

## Revision note r3 (2026-11-19, P3-05's implementation rulings)

7.5 and 13: `PeoplesSubsystem::ruin` and the `PeoplesView` state readers take the step's `StepContext` (P3-05 r3); P3-06's member holds the subsystem, never `State`. A sacked settlement passed to `ruin` must be standing and owned, which 7.5's order guarantees (the sack event is appended and the population set before the call). No rule or number changes.

## Revision note r4 (2026-11-19, the P3-05 audit)

New 8.4: what P3-06 respects in the peoples code, from the P3-05 determinism audit (items 1–9), each with the rule that meets it. Checking items 3 (end a civilization whose last settlement is sacked or taken, in the same step) and 4 (ruin only in T6) against 7.3–7.7 found no gap; no rule, prototype behaviour or number changes.

## Revision note r5 (2026-11-19, rulings from the implementation)

Recorded after the implementation (`p3/06-conflict`, matching the prototype on seed 42 and 17 other worlds). 8.1 item 2: the parent of a seceded civilization is read with `StepContext::event_kind` and `event_participant` (P3-04 r8). 8.3 item 4 and 5.3 made to agree: an invisible colony is never the settlement whose founding a claim cites, but may be the claimed region's `owner_event`. 8.3 item 7: the three paths that open and close a relation in one step, each written as a row (P3-04 6.7, r8). New 16.0: readings agreed by the engineer and the test writer. 16.1 item 14 and 16.3 item 20: peoples-only golden cases at 0.16.0, the new case `hist-conflict-42-1024`, the test writer's extra cases, the CLI peoples test retired. 16.4: four internal guard messages. No rule or number changes.

## Revision note r6 (2026-11-19, relation bookkeeping)

8.3 item 7 restated against P3-04 6.7 item 2 as implemented (r9): openings and closings recorded in call order; P3-06 never opens a key twice before one bookkeeping; the net-change wording removed. No rule or number changes.

## Revision note r7 (2026-11-20, for P3-08; generator 0.17.0)

Normative for the P3-08 change (`docs/specs/p3-08-followups.md`), not before it. Rules: 7.3 item 4, the victors' dead are `loss_winner` of the smaller of the two forces (the world critic's F8 and suggestion 7); 11, the `settlement_captured` template drops "and 0 fled" with P3-04 r10's three-word form (data). Text: 7.4 and 8.4 item 3, casualties can empty settlements under legal non-shipped tuning, and what follows (the audit's A1); 8.3 item 4, a loser's same-step Q4 colony in a ceded region stays the loser's (the audit's A2); 8.3 item 7, same-year relation rows also in `calm` and `span12000` (the validation's F7); 14 item 3, the new ruin event kind 31 of P3-05 r6; 16.2 item 18a, the log equals the peoples-only log below the first claim except the eras of that claim's quiet step (the validation's F1); 15.3 and 15.5, the 100-world figures of the 0.16.0 validation and typical ranges at its 5th to 95th percentiles (F3). The 0.16.0 figures of 15 stay as measured; P3-08's validation re-measures them for 0.17.0.

## Revision note r8 (2026-11-20, corrections after the P3-08 implementation)

Text only. 11's rendered samples, the capture example of r7 and the sample of 15.2 are marked as generator 0.16.0's: from 0.17.0 the region names (P1-06 r3), the name filter and elven realm form (P3-02 r5) and the "the" rule (P3-04 r10) change them; 16.3 item 20's golden case is re-recorded from the reference and not compared with 15.2's text. The report line of P3-05's kind 31, `events_settlement_abandoned_to`, follows `events_civ_conquered` (P3-07 9: one line per kind in id order).

## Revision note r9 (2026-11-20, conflict balance; for P3-09, generator 0.18.0)

New section 20 (P3-09, `docs/specs/p3-09-conflict-balance.md`; the world critic's review of 0.16.0; decided by Paul 2026-11-20; checked against P3-08 as built, P3-09 10: a flight needs `abandon_min` survivors, a last refuge pays no tribute), normative from generator 0.18.0: a fallen last refuge flees into hiding (`refugees_settled`, kind 32; hidden settlements, 20.1); war goals avoid the defender's capital (20.2); a lost war costs the loser a region or a tribute (`tribute`, kind 33; 20.3); war weariness per defender and region (20.4); revenge (20.5); orc muster, local levies, odds weighed by terrain with a floor and a minimum that also stops flare-ups, a shorter truce for peoples more warlike than goblins (20.6); faerie concealment and elven and faerie home defence (20.7); enmity hops and the map-size reading (20.8); eleven tuning fields, three race fields, the two kinds, `claim` salience 35 (20.9, 20.12); order and determinism (20.10); statistics (20.11); invariants (20.13); tests (20.14). Sections 1–19 keep describing 0.16.0 and 0.17.0 where 20 does not replace them. Figures are the spec writer's prototype at 0.16.0 with P3-08 partly emulated, to be re-measured at 0.18.0 (P3-09 5.6).

## Revision note r10 (2026-11-20, Paul's notes on a generated history and the test writer's findings; for P3-09 r3, generator 0.18.0)

New subsections 20.15–20.22, after 20.14; nothing before them is reorganised or renumbered, and where they differ from 20.1–20.14 they rule.
- 20.15: the war score counts the dead inflicted and the people taken; an exhausted side that leads wins; the winner of the war's last battle never loses an ending decided by the score; `peace` only when the loser is exhausted, else the new kind 34 `war_won`.
- 20.16: the defeated's dead are bounded by half the victors' force.
- 20.17: concealed settlements are in presence but must be found (`find_permille` 250 a year); after 3 years unfound the war ends with kind 35 `search_abandoned`; faerie home defence 0.
- 20.18: measured at 0.17.0.
- 20.19: data, invariants, tests.
- 20.20: a race's last refuge is never taken below `abandon_min` by casualties nor abandoned (the test writer's extinction case).
- 20.21: the test writer's readings confirmed.
- 20.22: kind 36 `refuge_held` with a `reason`, so that T3 can be checked from the rows.

New stream `hist.conflict.search`; new tuning `kill_max_permille`, `find_permille`, `search_years`.

## Revision note r11 (2026-11-20, weariness counts wars in progress; the engineer's readings)

New 20.23 (the lead's ruling on T2): a defender is weary of a region while the futile count plus the wars over it still open reaches `weary_wars`, at the claim and at the declaration; invariant (12) and its tests. New 20.24: three readings of the engineer confirmed. New 20.25: the typical ranges of 15.5 to be reset from the P3-09 validation sweep; plague's measured summary share (83‰ against 121‰ predicted). Nothing before 20.23 is changed or renumbered.

## Revision note r12 (2026-11-20, text after the P3-09 integration)

Text only, no rule change.
- 20.21 item 1 is reworded so that it cannot be over-read (the lead's ruling): a colony founded earlier in the same step does not put its region into presence; it counts only for whether a held region has a hidden settlement. Q5 is its own step, so Q4's colonies count in the odds read there.
- New 20.26: the war lines' `unfound`, `won-c<W>` for `peace` and `war_won`, and `-tribute`.

## Revision note r13 (2026-11-20, the P3-09 validation's D1, D2, D3 and typical ranges; generator stays 0.18.0)

Rules and text after the P3-09 validation (`docs/reviews/p3-09-conflict-balance-validation.md` on `p3/09-validation`, 537f981), with the lead's rulings of 2026-11-20.
- **New 20.27 (D1, a rule change).** A cession (7.7 and 20.3) passes every settlement of the loser standing in the ceded region, including one created earlier in the same step, such as a Q4 colony. Ownership is a fact, not a matter of visibility, and presence keeps ignoring such a colony. The same reading of "standing settlement" holds for the capture's conquest of a region, `ruin`'s release and the conquest of a civilization; there it cannot differ from the visible reading, so nothing changes. No new event: the `region_ceded` records the transfer. New invariant (13). The seed-3 case is worked through. 7.7, 8.3 items 4 and 7, 20.3 item 1, 20.13 and 20.21 item 3 are marked.
- **New 20.28 (D2, D3; text only for the generator).** `wars_same_goal_unchanged_max` counts the wars weariness counts: a war closed by a side's end (`civ_conquered`, `civ_ended`) neither extends nor breaks a run. The run's window ends at the latest end in the run. The first war after a change of owner starts a run of 1. With this definition T2 follows from invariant (12) in every world; the argument and seed 6 are worked through. 20.23's claim is marked.
- **New 20.29 and 15.5's new table.** The typical ranges are reset from the 0.18.0 validation's 5th–95th percentiles (closing 20.25 item 1). T1 is stated as defined at 1024 defaults only. F1 (a race at ≥ 700‰ of the people in 27% of default worlds) and F3 (T1 above 250 outside the 1024 default set) are recorded as known observations, with no rule change.

Output: only 20.27 changes bytes, and only in worlds where a cession meets a same-step settlement of the loser in the ceded region (at least 4 of 53 sampled worlds, by 7.7 and 20.3 alike, among them the golden `hist-conflict-flight`; validation's re-run reports the final count). **Generator stays 0.18.0**, which is not yet released. Validation re-records the goldens and world hashes after the engineer's change. Schema stays 7.

## Revision note r14 (2026-11-20, wording follow-ups; for P3-10, generator 0.19.0)

New section 21, normative from generator 0.19.0 (`docs/specs/p3-10-followups.md`). Rules (21.6 to 21.10, Paul's decision of 2026-11-20 on the P3-09 review): aim achieved only when the attacker holds the goal, else the score with the last-battle rule (21.6); a failed search makes the region weary for `search_weary_years` (500), no search where the war has fought, only the first search at salience 40 (21.7); resettlement by the same holder is no change of owner, in weariness and in the run statistic alike (21.8); per-race `odds_min_permille`, orc 500 (21.9); a hidden refuge lies low (21.10); tests 21.11. Wording: `note_reduction` only for a settlement that lost people (21.1, amends 7.4); the battle templates without "0 of the victors" (21.2, data); the P3-09 audit's advisories A1 (the flight plan copies its hop row) and A2 (one last-refuge predicate), no output change (21.3, 21.4); tests (21.5). No earlier section is renumbered.
