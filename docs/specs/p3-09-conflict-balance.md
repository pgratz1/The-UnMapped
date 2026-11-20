# P3-09 Conflict balance

Status: **decided, ready for implementation (2026-11-20)**. Paul accepted every recommendation of section 8 ("I accept all your recommendations on P3-09 conflict balance"; `docs/design/proposals.md`, "Paul's decision of 2026-11-20: the P3-09 conflict balance questions go with the lead's recommendations"). Written by the spec writer on `p3/09-spec`, cut from `origin/p3/06-final`, with `origin/p3/08-spec`, `origin/p3/08-int` (P3-08 as implemented, dae7b96) and `origin/main` merged. Phase P3 History core, a change spec after P3-06 (expansion and conflict, generator 0.16.0) and P3-08 (names and peoples follow-ups, generator 0.17.0), which is implemented and lands first. Generator **0.18.0**. **No schema change**: schema stays 7; the change appends two event kinds (32, 33), adds tuning fields and race fields (data), changes one base salience and the name filter's word list (data), and keeps every other new fact in the conflict subsystem's members, derivable from the log.

This spec is short on purpose. It lists every item, names the stage-spec section that holds its normative text (each stage spec stays the single source for its stage, amended with a new revision and a revision note), gives the order of implementation, the effect on output and goldens, the tests to add and the statistics to re-measure.

## 1. Purpose

The world critic's review of P3-06 at 0.16.0 (`docs/reviews/p3-06-conflict-world-critic.md`; the validation is `docs/reviews/p3-06-expansion-conflict-validation.md`) found the conflict stage acceptable with follow-ups: 42% of wars change nothing and the same defender is attacked over the same region up to 17 times (F1); a race's last settlement can never fall, so villages hold out against 30 to 45 sieges (F2); goblins start 81% of the wars and elves and faeries none, and the orcs lose three wars in four (F3); the summary loses war endings and whole ages (F4); faeries end with one settlement in a quarter of worlds (F5); race-level runaway goes unmeasured (F6); the goblin–elf feud is missing in a fifth of worlds (F7); and map size sets the tone more than any setting (F9). P3-09 changes the rules that cause these, adds the statistics that measure them, and amends the history report's summary selection before the report is implemented. The wording fixes of F8 ("0 fled", battle losses from the smaller force) and the faerie forest weights are P3-08's and are assumed here.

## 2. Items and where their normative text is

| # | Item (review) | Rule, in short | Normative text | Changes worlds |
| --- | --- | --- | --- | --- |
| 1 | Last refuge falls into flight (F2, suggestion 5) | A fallen last refuge is sacked; its survivors found a hidden settlement in the free region within three borders farthest from the attacker (`refugees_settled`, kind 32), hidden for 300 years (out of every army's sight: no claim, battle or siege); the guard holds only when there is nowhere to flee. No race goes extinct by conflict. | `docs/specs/p3-06-expansion-conflict.md` 20.1; P3-05 21 (the seam) | yes |
| 2 | Wars that change something (F1, suggestion 1) | Goals avoid the defender's capital region unless it is the only one in reach; a won war with no cession of the goal costs the loser (attacker or defender) its best non-capital region in the winner's reach, else a tribute of 100‰ of its people (`tribute`, kind 33); a defender that has withstood three wars over one region without losing it is no longer attacked over it until it changes hands | P3-06 20.2, 20.3, 20.4 | yes |
| 3 | Agency for the other peoples (F3, suggestions 3 and 4) | Revenge: a people wronged by another (a settlement sacked or taken, a region ceded) fights it with aggression at least 400 while the grudge lasts, and claims the lost land first. Orcs: race field `muster_permille` 4000 with exhaustion scaled by it; battles fought with local levies (settlements within three borders); declaration odds weighed by the goal's defence, with a floor on the aggression term and a minimum below which no war is declared and no flare-up happens; a shorter truce for peoples more warlike than goblins | P3-06 20.5, 20.6; P3-01 3.2 (r9) | yes |
| 4 | Faeries hidden, elves' tail (F5, suggestion 6) | Race fields `conceal_min` (faeries: concealed where their biome term B ≥ 400, i.e. in forest) and `home_defense_permille` (elves 1500, faeries 2000: defence and garrisons on their own land) | P3-06 20.7; P3-01 3.2 (r9) | yes |
| 5 | Early feud, long peace, map size (F7, F9, suggestions 9 and 10) | Armies cross three borders, not two, against a civilization they have an `enmity` row with (`enmity_hops`); no conflict rule for map size (5.3: the other rules remove the 512 massacre); `auto` founds more realms per race on a larger map (+1 per 90 non-islet regions, at most +2: 2048 only) | P3-06 20.8; P3-05 21 item 5 | yes (2048 also from the founding) |
| 6 | Sweep statistics (F6, suggestion 8) | `[stats]` keys `wars_hollow`, `wars_hollow_permille`, `wars_same_goal_max`, `wars_same_goal_unchanged_max`, `race_population_share_max_permille`, `race_regions_share_max_permille`; typical ranges and validation measures | P3-07 21.5; P3-06 20.11 | no (report only) |
| 7 | Summary selection (F4, suggestion 2) | Per-age floor of 20 events; a shown declaration brings its ending and the ending's cession or tribute, a shown sack or capture its moved capital, refugees and conquest; claim salience 45 → 35; a note after the declaration ("Claimed since 2574. The second war between them.") and after the flare-up ("The Grork Horde pressed its claim to Southwest Wood against Finen.") | P3-07 21 (r8); the salience in P3-06 20.9 | the salience only |
| 8 | Short English-word list of the name filter (Paul's decision 1 on the name filter questions, `docs/design/proposals.md` "Paul's decisions of 2026-11-20 on the name filter questions") | `game/data/name_filters/words.toml` becomes the short list (about 150 crude, comic or modern-sounding words) in place of P3-08's 647; ordinary English words that fit the setting stay possible as name roots. Data only, produced by another spec writer on branch `p3/10-word-list` (the data file and a P3-02 revision); **the engineer merges that branch into the P3-09 implementation branch** and does not edit the list. It changes every `content_hash`, the names drawn wherever a rejected word now passes, every world hash and the names goldens (`p3-02-names.txt`, `p3-01-data.txt`), and through the names every history golden | P3-02 (revision on `p3/10-word-list`); this table | yes |

## 3. What it touches (for the engineer)

| Area | Files |
| --- | --- |
| Conflict | `worldgen/src/history/conflict.cpp`, `conflict_war.cpp`, `worldgen/include/worldgen/history/conflict.hpp` (members `hidden_until`, `futile`, `wronged`; flight, costs, odds, local force, truce, concealment) |
| Peoples seam | `worldgen/src/history/peoples.cpp`, `peoples.hpp`: `PeoplesView::site`, `founding_kind`, `hops`, `settlement_name` (P3-05 21) |
| Catalogue | `event_catalogue.cpp`: kinds 32 `refugees_settled`, 33 `tribute` |
| Data | `game/data/history/conflict.toml` (+11 fields, 20.9), `game/data/history/peoples.toml` (+2 fields, P3-05 21 item 5), `game/data/history/report.toml` (two entries, `claim` 35), `game/data/races/*.toml` (three fields, 20.12); the same in `worldgen/tests/data/geo*/` |
| Data kinds | `src/data/kinds_history.cpp`: the race fields and the `history.conflict` fields with their ranges |
| Report | P3-07 is not implemented: its implementer follows 21 (r8) from the start |
| Version | `worldgen/include/worldgen/version.hpp`: **0.18.0** |
| Docs | `docs/specs/schema.md` (kinds 32, 33), README and CLAUDE.md (the lead) |
| Tests | `worldgen/tests/history/` (conflict group), the reference `p3_conflict_reference.py`, golden `p3-06-conflict.txt` |

## 4. Order of implementation

One branch, one engineer and one test writer working from the stage specs separately; P3-08 (0.17.0) merged first. `GENERATOR_VERSION` becomes 0.18.0 in step 1; every golden is re-recorded once, in step 8.

| Step | What | Spec |
| --- | --- | --- |
| 0 | Merge `p3/10-word-list` (the short word list, item 8): data only | P3-02 (that branch) |
| 1 | Data: race fields, `history.conflict` fields, kinds 32 and 33 with their report entries, `claim` 35 | P3-01 3.2; P3-06 20.9, 20.12 |
| 2 | Hidden settlements and presence; concealment; home defence | P3-06 20.1 (hidden), 20.7 |
| 3 | Flight of the last refuge (with the P3-05 seam) | P3-06 20.1; P3-05 21 |
| 4 | Goals, the cost of a lost war, weariness | P3-06 20.2–20.4 |
| 5 | Revenge, muster, local levies, odds, truce, enmity hops; `auto` by map size | P3-06 20.5, 20.6, 20.8; P3-05 21 item 5 |
| 6 | Invariants and tests | P3-06 20.13, 20.14 |
| 7 | Statistics (report keys, with the P3-07 implementation or as reference-only checks before it) | P3-07 21.5; P3-06 20.11 |
| 8 | Goldens and figures; validation sweep; world critic | 6, 7 below |

The test writer extends `p3_conflict_reference.py` from P3-06 20 alone (not from the C++ and not from the spec writer's prototype).

## 5. Measured effect (prototype, at 0.16.0 with P3-08 emulated)

**Every figure in this section and in P3-06 20 is a prototype figure at generator 0.16.0** (with two parts of P3-08 emulated, 5.1). None was measured on the C++ or on 0.17.0's worlds. The engineer re-measures them on the implementation (the reference's world command over 1024 seeds 0–39 is enough before handing on), and validation re-measures them at 0.18.0 on its sweeps (5.6). A figure that moves is not a defect; an **acceptance target** that is missed is a finding that goes back to the spec writer before merge.

**Acceptance targets** (1024, default settings, the 0.18.0 validation sweep; checked from `[stats]`, the tables and the reference):

| # | Target | Prototype (`FINE`, seeds 0–39) |
| --- | --- | --- |
| T1 | hollow wars below 25% (`wars_hollow_permille` < 250 over the sweep) | 24% |
| T2 | at most 3 consecutive wars per (defender, goal) without a change of owner (`wars_same_goal_unchanged_max` ≤ 3) in at least 95% of worlds | 38 of 40 |
| T3 | no last refuge holds a siege while a flight destination with enough survivors exists (the reference counts sieges held by the guard with a destination: 0) | 0 of 41 sieges |
| T4 | no race extinct (`races_alive` = the present races; P3-06 14 item 7) in every world of every sweep | 0 worlds |
| T5 | orcs win at least as often as they lose: over the sweep, peaces with an orc civilization as `winner` ≥ peaces with one as `loser` | 39 against 30 |

The other figures (faeries and elves with one settlement, goblin–elf wars, orcs' share of their age, race shares, the summary) are typical ranges (P3-06 20.11), not acceptance targets.


### 5.1 Method and configurations

The prototype is `~/.cache/the-unmapped/out/p3-09-spec/proto.py` and its snapshots `proto_b2.py` … `proto_fin.py` (each batch ran on a frozen copy), a subclass of the test writer's reference `worldgen/tests/reference/p3_conflict_reference.py`, which reproduces the 0.16.0 world row for row (checked: seed 42 at 1024 gives the 1,030 events of P3-06 15.2). It recomputes each world's history from the stored geography of the 0.16.0 validation sweep (`~/.cache/the-unmapped/out/p3-06-validation/sweep/`). Two parts of P3-08 are emulated in every configuration but `base016`: the victors' losses from the smaller force (P3-06 r7 7.3) and the faerie forest weights (P3-05 12.3, in a copy of `game/data`, `data-p308/`). P3-08's dwarven upland sites, quiet-era merging, halved famine and spread rates, the chance-abandonment rule for last settlements, kind 31 and the names are **not** emulated, so every figure here is "measured at 0.16.0" and must be re-measured at 0.18.0 by the statistic named in 5.6. Scripts: `run.sh` (four processes), `agg.py` (the statistics), `agg_sum.py` (the summary), `batch1.sh` … `batch9.sh`; results in `runs/<name>.jsonl`. Seeds: 1024 default settings seeds 0–39; 512 seeds 0–29; 2048 seeds 0–4; `--civs-per-race 4` and `turbulent` seeds 0–9.

| Name | Rules (P3-06 20 subsections) |
| --- | --- |
| `base016` | 0.16.0 as shipped |
| `base017` | P3-08 emulated (the baseline of every comparison) |
| `A` | + flight (20.1) |
| `ALT` | `base017` + no claim or declaration against a last refuge (the alternative of 20.1) |
| `D` | `A` + goals (20.2), cost of the attacker's victories only (20.3), weariness per pair, revenge 400 |
| `F` | `D` + orc muster 2000 without exhaustion scaling, home defence in battles only (faerie 1500, elf 1000) |
| `G1` | `F` with weariness per defender (20.4) |
| `G3` | `G1` + home defence 2000/1500 on battles and garrisons, orc muster 2500, odds floor 500, target chosen by chance (not adopted) |
| `G5` | `G3` + the defender's victory costs the attacker too (20.3) |
| `H1` | `G5` without G3's target choice and odds floor, orc muster 2000 |
| `H2` | `H1` + concealment (20.7) |
| `FIN` | every rule of 20 except `enmity_hops` |
| **`FINE`** | **every rule of 20 as specified** (the expected 0.18.0) |

### 5.2 1024, default settings, seeds 0–39

| Statistic | `base016` | `base017` | `FIN` | **`FINE`** | Target (brief, review) |
| --- | --- | --- | --- | --- | --- |
| wars per world (min / median / max) | 7 / 23.5 / 48 | 7 / 21.5 / 45 | 1 / 16.5 / 43 | 2 / 16 / 39 | — |
| hollow wars (no sack, capture, cession, tribute, conquest) | 403 of 972 (41%) | 367 of 911 (40%) | 150 of 668 (22%) | **177 of 736 (24%)** | below 25% |
| attacker victories that gained nothing | 132 of 502 | 124 of 460 | 2 of 344 | 0 of 387 | — |
| longest run of wars over one (defender, goal) leaving its owner unchanged, per world | 2 / 4 / 16 | 1 / 5 / 12 | 1 / 3 / 4 | **1 / 3 / 4** | at most 3 |
| worlds where that run exceeds 3 | 29 | 26 | 2 | **2** | 0 (5.5) |
| most wars over one (defender, goal), per world | 2 / 5.5 / 16 | 1 / 6 / 12 | 1 / 3 / 9 | 2 / 3 / 9 | — |
| attackers: goblin / orc / human / dwarf / elf / faerie | 744 / 131 / 68 / 29 / 0 / 0 | 697 / 124 / 65 / 25 / 0 / 0 | 472 / 88 / 67 / 29 / 12 / 0 | **550 / 83 / 61 / 32 / 10 / 0** | goblins most warlike; elves and faeries some |
| goblin share of all wars | 76% | 76% | 70% | 74% | — |
| orc wars ended by peace: won / lost | 22 / 85 | 19 / 84 | 40 / 27 | **39 / 30** | won ≥ lost |
| orc age: wars, orc share, goblin share | 532, 24%, 59% | 500, 24%, 59% | 331, 26%, 45% | **320, 25%, 48%** | orcs ≥ 33% (not met, 5.5) |
| wars with an orc defender | 1 | 0 | 5 | 6 | — |
| faerie standing settlements; worlds with 1; with ≤ 2 | 1 / 4 / 16; 8; 14 | 1 / 5 / 18; 6; 9 | 4 / 8 / 18; 0; 0 | **4 / 8 / 18; 0; 0** | 5.5 |
| elven standing settlements; worlds with 1; with ≤ 2 | 1 / 12 / 44; 1; 1 | 1 / 12 / 44; 3; 4 | 1 / 11.5 / 48; 3; 3 | **3 / 9 / 45; 0; 0** | 5.5 |
| human standing settlements; worlds with 1 | 1 / 14 / 44; 1 | 1 / 16 / 47; 1 | 1 / 12 / 52; 1 | 1 / 10.5 / 39; 1 | — |
| dwarven, orcish, goblin standing settlements (median) | 6, 5, 32.5 | 5, 5, 32 | 4, 5.5, 36.5 | 4.5, 5, 41.5 | — |
| largest race share of people ≥ 600‰ (worlds); median share | 8; 477 | 10; 487 | 12; 526 | 12; 546 | 5.5 |
| largest race share of held regions (min / median / max ‰) | 217 / 406 / 676 | 214 / 396 / 685 | 208 / 403 / 657 | 208 / 440 / 736 | — |
| worlds without a goblin–elf war | 7 | 6 | 8 | **4** | nearly none |
| first war after the goblins' awakening: median years; worlds > 1,000 | 900; 18 | 900; 18 | 1,162; 21 | **788; 15** | — |
| longest stretch without a declaration after the goblins (median; worlds ≥ 1,500) | 1,138; 10 | 1,138; 10 | 1,162; 11 | **944; 9** | — |
| sieges of a last refuge / held by the guard / flights / no destination | 238 / 142 / 0 / 0 | 216 / 122 / 0 / 0 | 49 / 0 / 12 / 0 | **41 / 0 / 12 / 0** | none held where a flight is possible |
| tributes / cessions of 20.3 | 0 / 0 | 0 / 0 | 200 / 119 | 224 / 139 | — |
| races extinct (worlds) | 0 | 0 | 0 | **0** | 0 |
| events (min / median / max) | 439 / 805 / 1,422 | 432 / 779 / 1,415 | 455 / 754 / 1,346 | 469 / 787 / 1,537 | — |
| simulated years (min / median / max) | 1,162 / 1,295 / 1,450 | 1,112 / 1,280 / 1,450 | 1,162 / 1,294 / 1,450 | 1,169 / 1,309 / 1,572 | P3-00 4.1 |
| civilizations ended, sacked settlements (median) | 1, 12 | 1, 13.5 | 1, 8 | 0, 10 | — |
| flare-ups (min / median / max) | 0 / 3 / 5 | 0 / 2.5 / 5 | 0 / 3 / 5 | 0 / 3 / 8 | — |

Item by item (each against the configuration before it in 5.1, same seeds): flight (`A` against `base017`) cuts sieges of a last refuge from 216 to 88 and hollow wars from 40% to 32%; the alternative (`ALT`) leaves 161 sieges and 38%. The goals, the costs of the attacker's victories and revenge (`D`) bring hollow wars to 17%, but the elves' median falls to 8 and 5 worlds end with one elven settlement; the defender's costs (`G5`) bring hollow wars to 15%. Weariness per defender (`G1` against `F`) removes every run above 3 (13 worlds before). Concealment (`H2` against `H1`) takes faeries with one settlement from 3 to 0 and their median from 4 to 8. Muster with scaled exhaustion, local levies, the odds rules and the truce turn the orcs from 11 won against 56 lost (`H1`) to 40 against 27 (`FIN`). `enmity_hops` (`FINE` against `FIN`) halves the worlds without a goblin–elf war (8 → 4) and, by giving the elves their wars earlier and shorter, leaves no world with one elven settlement (3 → 0).

### 5.3 Map size

512, seeds 0–29 (`b512` = `base017` rules; `FINE` = every rule):

| Statistic | `b512` | `FINE` |
| --- | --- | --- |
| wars per world | 6 / 29.5 / 47 | 6 / 16 / 29 |
| hollow wars | 493 of 840 (58%) | 77 of 460 (16%) |
| longest futile run over one (defender, goal), per world; worlds above 3 | 2 / 10 / 17; — | 2 / 3 / 4; 2 |
| sieges of a last refuge / held / flights / held for want of land | 578 / 302 / 0 / — | 157 / 76 / 16 / 76 |
| orc wars won / lost; orc share of the orc age | 28 / 68; 27% | 40 / 17; 40% |
| worlds with one standing settlement: faerie, elf, human, orc | 16, 6, 7, 3 | 1, 3, 4, 8 |
| largest race share of people ≥ 600‰ (worlds) | 3 | 8 |
| events (median) | 632 | 491 |

2048, seeds 0–4 (`b2048`, `FIN2048` = `FIN`, `FIN2048E` = `FINE`, `FIN2048H` = `FIN` with `march_hops` 3 for every pair, `FIN2048C` = `FIN` with `--civs-per-race 4`):

| Statistic | `b2048` | `FIN2048` | `FIN2048E` | `FIN2048H` | `FIN2048C` |
| --- | --- | --- | --- | --- | --- |
| wars per world | 2 / 9 / 18 | 2 / 9 / 19 | 2 / 11 / 18 | 3 / 11 / 19 | 17 / 41 / 64 |
| hollow wars | 16 of 47 (34%) | 7 of 47 (14%) | 8 of 50 (16%) | 10 of 53 (18%) | 30 of 192 (15%) |
| worlds without a goblin–elf war | 2 | 2 | 2 | 2 | 0 |
| civilizations ended (median, max) | 0, 1 | 0, 0 | 0, 1 | 0, 1 | 3, 5 |
| events (median) | 791 | 755 | 867 | 867 | 2,583 |

So no size rule is needed for 512 (the massacre came from the futile and endless sieges, which the rules remove; the orcs' tail at 512 is a finding for validation), and 2048's peace comes from having the same dozen realms on four times the land: P3-05 21 item 5 founds more realms per race there (three or four at about 200 non-islet regions); the rule's own figures are measured at 0.18.0 (only the explicit setting 4 was measured here).

### 5.4 Heavier settings (seeds 0–9)

`--civs-per-race 4`, 1024, seeds 0–9 (the validation's `civs4` worlds; `base017` rules against `FINE`):

| Statistic | `base017` | `FINE` |
| --- | --- | --- |
| wars per world | 48 / 82 / 136 | 51 / 67.5 / 107 |
| hollow wars | 357 of 947 (37%) | 135 of 709 (19%) |
| longest futile run over one (defender, goal), per world | 4 / 9.5 / 16 | 3 / 3 / 5 |
| goblin share of wars; elven attackers | 83%; 0 | 81%; 39 |
| orc wars won / lost; orc share of the orc age | 2 / 20; 14% | 18 / 8; 23% |
| faerie, elven standing settlements (median); worlds with one | 1, 8; 6, 1 | 18, 18.5; 0, 0 |
| human standing settlements (median) | 22.5 | 9 |
| civilizations ended (median) | 12.5 | 5 |
| sieges of a last refuge / held | 105 / 72 | 2 / 0 |
| flare-ups; simulated years (min / median / max) | 3 / 7 / 9; 1,298 / 1,471 / 1,563 | 7 / 11 / 17; 1,550 / 1,672 / 1,905 |
| events (median) | 2,204 | 2,368 |
| largest race share of people ≥ 600‰ (worlds) | 4 | 4 |

With four civilizations per race the rules end fewer realms (5 against 12.5) and keep the elves and faeries, but the humans lose ground (median 9 against 22.5: they are the goblins' and orcs' new neighbours, and wars now cost land), and the odds minimum lets strain build to more flare-ups (11 against 7; the simulated share stays at most 1,905 of 6,000 years, 318‰, under P3-04's cap of 450‰). Both are findings for the 0.18.0 validation, not defects.

`turbulent`: not measured by this spec (the run did not finish in time); validation measures it.

### 5.5 Targets

| Target | `FINE` | Met |
| --- | --- | --- |
| hollow wars below 25% | 24% (1024), 18% (512 with `FIN`) | yes, narrowly at 1024 |
| at most 3 wars per (defender, goal) without a change of owner | a run of 4 in 2 of 40 worlds (1024), 2 of 30 (512) | nearly; weariness is checked at the claim and again at the declaration (P3-06 20.2), so a fourth war needs wars of two attackers open over one region at once; the 2 cases were not traced (validation checks `wars_same_goal_unchanged_max`) |
| orcs win at least as often as they lose | 39 won, 30 lost | yes |
| orcs start at least a third of the orc age's wars | 25% | **no**, accepted (decision 8.5) |
| elves and faeries start some wars after losses | elves 10, faeries 0 in 40 worlds | elves yes; faeries no (concealed, they are rarely wronged; accepted, decision 8.6) |
| goblins remain the most warlike | 74% of all wars, 48% of the orc age's | yes |
| faeries with one settlement in at most 5% of worlds | 0 of 40 | yes |
| elves with one settlement in at most 5% of worlds | 0 of 40 | yes |
| goblin–elf feud in nearly every world | missing in 4 of 40 (where the two never come within three borders) | partly |

### 5.6 What validation re-measures at 0.18.0

On the sweeps of P3-06 15.6 (1024 seeds 0–199, 512 seeds 0–49, 2048 seeds 0–19 and the settings rows), from `[stats]` and the tables: `wars_hollow_permille` (target < 250), `wars_same_goal_unchanged_max` (≤ 3 in ≥ 90% of worlds), `race_population_share_max_permille` and `race_regions_share_max_permille` (distribution; people ≥ 700‰ in at most a quarter of worlds: 9 of 40 with `FINE`, 4 with `base017`), `wars_attacker_<race>` (goblins first, elves > 0), orc wars won and lost and the orc age's attackers (computed from the tables, P3-06 20.11), `settlements_standing_faerie` and `_elf` (worlds with 1: at most 5%), `events_refugees_settled`, `events_tribute`, worlds without a goblin–elf war, the first war and the longest quiet stretch after the goblins' awakening, `simulated_permille` and `events` against P3-06 15.5's bounds, `races_alive`, and the summary measures of P3-07 21 (5.7). A median more than 20% from 5.2's `FINE` column is a finding for the spec writer, not a defect: P3-08's unemulated changes move the dwarves and the logs.

### 5.7 The summary (P3-07 21)

The selection of P3-07 21 against 7.4's threshold-only rule, computed by the prototype from the log (1024, default settings, seeds 0–39; `FINE` with `claim` at 35; `base017` with 45; median over worlds unless stated):

| Measure | `base017`, threshold only | `base017`, 21 | `FINE`, threshold only | **`FINE`, 21** |
| --- | --- | --- | --- | --- |
| threshold T (min / median / max) | 26 / 31 / 46 | 26 / 31 / 46 | 26 / 31 / 46 | 26 / 31 / 46 |
| events shown (min / median / max) | 123 / 208 / 249 | 156 / 221 / 245 | 115 / 210 / 248 | 142 / 223 / 248 |
| events shown per age, ages 1–6 (median) | 5, 9, 24, 24, 34, 98 | 19.5, 20, 21, 22, 31, 92 | 5, 16.5, 27.5, 28.5, 31, 89 | **19.5, 20, 26, 27.5, 30.5, 83** |
| conflict and capital moves, ‰ of shown | 533 | 503 | 496 | 450 |
| claims shown | 22 | 17 | 23.5 | 22 |

Seed 42 (0.16.0 log): 3, 8, 12, 28, 30, 74 events per age become 20, 20, 20, 28, 30, 74 (192 shown, T 46). With four civilizations per race (seeds 0–9, `FINE`), where the thresholds are high, the companions matter: under the threshold alone 275 shown declarations lack their ending and 164 sacks or captures their moved capital (339 and 329 with `base017`); under 21 none. The cost: the companions make the set grow in steps as t falls, so T rises (46 / 71 / 71 against 46 / 51 / 61) and fewer events are shown (127 / 144.5 / 240 against 196 / 223.5 / 245). A fill step (after T, add whole units of the next candidates in rank order while the set stays within 250) would recover most of that budget; it is not in 21 because it was not measured (decision 8.11). At default settings no shown war lost its ending under either rule (the thresholds stay at or below 46, under `truce` 50 and `peace` 60); the companions matter when T rises above 50, as with four civilizations per race (5.4). With the claim at 35 and a median threshold of 31, claims stay visible in small logs (median 22 shown): the note in the declaration makes them redundant only where the threshold rises.

## 6. Golden values

Every world with history changes from 0.17.0: the `history.conflict` and race data change every `content_hash`, the catalogue gains kinds 32 and 33 and the `claim` salience changes in every world with conflict, and the history tables change from the first point where a new rule applies (in practice the first claim). Re-recorded once, after the cross-preset comparison: `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, `p3-01-data.txt`, `p3-04-engine.txt` (the catalogue tables), `p3-05-peoples.txt` (the peoples-only configuration cases carry the catalogue), `p3-06-conflict.txt` (every line), and P3-07's report goldens if P3-07 is implemented by then. Golden vectors of P1 stages and `p3-03-chronology.txt` do not change by P3-06 20; `p3-02-names.txt` and the god, civilization and settlement names in every golden do change by item 8 (the word list). Never regenerated to make a C++ test pass.

## 7. Tests (additions)

P3-06 20.14 lists them (flight, hidden, goals, costs, weariness, revenge, orcs and odds, concealment and home defence, invariants 20.13, a golden case with flights). P3-07's tests (14) gain: the units of 21.2 on fixture H1 (a war with its ending, a cession and a tribute caused by the peace, a sack with its moved capital); the floor with an age of fewer than 20 candidates and one of more; the threshold when the floor alone is near the budget (a fixture of eight ages); the notes (claim the same year, ordinal 2, 11, 21, 22, 23, 111; a flare-up whose trigger is not a claim: no note); the six new `[stats]` keys on a fixture with hand-counted values; invariant 13 item 2 as amended.

## 8. Decisions (questions to Paul, closed 2026-11-20)

Paul accepted every recommendation ("I accept all your recommendations on P3-09 conflict balance"; `docs/design/proposals.md`, "Paul's decision of 2026-11-20: the P3-09 conflict balance questions go with the lead's recommendations"). Each question is **decided** as the rule now written:

1. **Decided: the last refuge falls into flight** (P3-06 20.1): sacked, its survivors found a settlement in hiding within three borders, out of every army's sight for 300 years and found again without an event; the guard holds only when no free land is in reach (or, from the P3-08 alignment of 10, the survivors would be too few to settle).
2. **Decided: a lost war always costs the loser**, also when the defender wins (20.3): the goal, else the loser's best non-capital region in the winner's reach, else a tribute of 100‰ of its people, carried off; tribute as people now, not waiting for wealth in P5.
3. **Decided: revenge** at aggression at least 400 while the grudge lasts (20.5).
4. **Decided: runaway measured per race** as statistics with typical ranges, not a hard bound (20.11, P3-07 21.5); watched in the validation sweep.
5. **Decided: the orcs' share of their own age** of about 25% is accepted; they win more than they lose (20.6; target T5).
6. **Decided: faeries never strike back** (20.5, 20.7).
7. **Decided: the race fields** `muster_permille`, `conceal_min`, `home_defense_permille` with the shipped values (20.12; P3-01 3.2).
8. **Decided: local levies, odds weighed by terrain, an odds minimum that also stops flare-ups, a shorter truce for the most warlike** (20.6).
9. **Decided: armies cross three borders against an enemy of the other camp** (20.8 item 1).
10. **Decided: no conflict rule for map size**; `auto` founds more civilizations per race on a larger map (20.8 item 2; P3-05 21 item 5).
11. **Decided: the summary** of P3-07 21: per-age floor 20, a shown declaration always with its ending, claims at 35, the notes, within 250 lines and minimum salience 20. The fill step is added only if the P3-07 implementer's measurements confirm it (not specified now).

## 9. Proposals

The lead's entry "four rule changes proposed by the world critic's review of conflict" covers questions 1–4. This spec added one entry for the rest, "History: conflict balance beyond the four rule changes (P3-09)" (questions 5–11), now accepted with Paul's decision entry. The word list (item 8) is recorded in "Paul's decisions of 2026-11-20 on the name filter questions".

## 10. Checked against P3-08 as implemented (`p3/08-int`, dae7b96)

| P3-08 as built | Effect on P3-09 |
| --- | --- |
| The victors' losses from the smaller force (P3-06 r7 7.3) | Already in the prototype; with local levies (20.6 item 2) the forces compared are the local ones (text added) |
| Kind 31 `settlement_abandoned_to` with a destination, chosen by `nearest_standing` and `refugee_room` (P3-05 8.5 item 1) | A flight is not an abandonment and writes no kind 31: the survivors found a new settlement (kind 32, with `origin`); the remainder goes through `ruin`, whose destination is the same 8.5 item 1 rule and is the new refuge (20.1 item 3, text added). Kind ids 32 and 33 follow 31 |
| The last settlement is never abandoned by chance, but the deterministic rule below `abandon_min` applies to it (P3-05 r6 8.5 (a), (b)) | **Rule change**: a flight needs at least `abandon_min` survivors to settle, else the guard holds (20.1); a last refuge pays no tribute (20.3). Without them a refuge or a tribute could leave a race's only settlement below `abandon_min`, to be abandoned in the next T1, ending the race. The prototype ran without these conditions and saw no extinction (T4), so no figure changes by them as far as was measured |
| The plague plan is a replay of the outbreak's strikes (P3-05 r7 10.2); shared helpers `strike_result`, `other_standing` | No interaction: P3-09's people losses (tribute) go through 7.4's casualties, not strikes; a hidden settlement takes plague and famine like any other |
| `founding_kind` depends on the site cell (8.4 r6); names through `next_name` and the name filter | The P3-05 seam (21 item 1) exposes them as built (text corrected) |
| One quiet era per quiet stretch (P3-04 r10/r11 6.6) | No interaction: the costs of a war ended in Q4 are charged to the loser (W9) and wait for budget like a cession (20.3) |
| The three-word template form (P3-04 r10 8.4) | Available; kind 33's template uses the two-word form (`people` ≥ 1 always) |

## Revision note r1 (2026-11-20)

First version, for the lead's approval.

## Revision note r2 (2026-11-20, decided; P3-08 as built)

Paul accepted all eleven recommendations: section 8 closed, the status set to decided. Merged `origin/p3/08-int` and `origin/main`; the P3-09 amendments renumbered after P3-08's fixes (P3-05 r8, P3-06 r9; P3-01 r9 and P3-07 r8 unchanged). New item 8 (the short word list, from `p3/10-word-list`). Section 5 states that every figure is a prototype figure at 0.16.0 and lists the acceptance targets T1–T5. New section 10: checked against P3-08 as built, with two rule changes for the no-extinction invariant (a flight needs `abandon_min` survivors; no tribute from a last refuge) and text corrections of the seam.
