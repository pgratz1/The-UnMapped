# Validation: P3-11 orc arrival, wording and name follow-ups (generator 0.20.0, schema 7)

Validation specialist, 2026-11-22.

**Input.** Branch `p3/11-validation`, cut from `origin/p3/11-int` at `6db462b` (spec r1, implementation and tests integrated; the determinism audit passed on the implementation, `origin/p3/11-audit`). Nothing else was merged.

**Specs.**
- `docs/specs/p3-11-followups.md` (r1), sections 4, 5 and 8.
- P3-05 r17 section 23; P3-06 r20 section 23 (23.1 targets and reported figures); P3-02 r15 section 14; P3-04 r17 21.1.

**Baseline.** 0.19.0 as merged: `~/.cache/the-unmapped/out/p3-10-validation2/` (`sweep3/` worlds, `merged/` checks). The sweep uses the same 375 jobs (settings and seeds), so every comparison is pair for pair.

Evidence is under `~/.cache/the-unmapped/out/p3-11-validation/` (V below).

## Verdict

**Fail on two targets, with no defect.** The change does what the specs say: the test writer's reference passes all 375 sweep worlds, and so does my own check. T11, the change's main target, is met by a wide margin (median 9 years against 232). Two of the kept orc targets are missed by one world each:

- **T9 is 11 worlds (bound at most 10; 0.19.0: 7).** The orcs now act at once and fight far more (388 declarations against 217). In each of the 11 worlds they founded two to six settlements and lost all but one in wars (section 4.2).
- **T10 is 3 worlds (the lead's bound at most 2; spec 0; 0.19.0: 0).** All three are realms of four to six settlements living at peace with no open war, which is the kind the lead's ruling calls acceptable. None is a broken or frozen single-settlement realm. The count is over the bound by one (section 4.3).

By P3-06 22.4 and 23.1 a missed target goes to the lead for a ruling before the merge. I changed no tuning. Every other gate passes:

| Gate | Result |
| --- | --- |
| Build, five Linux presets | warning-free (0 warnings in every build log) |
| `ctest` before recording | only `worldgen-tests-other` and `worldgen-cli-tests-other` fail, on the goldens listed below (checked on `linux-gcc-release`) |
| `world-hashes.sh` (generate on each preset, then `compare` of all five) | **15 of 15 identical** on `linux-gcc-release`, `linux-clang-release`, `linux-gcc-debug`, `linux-clang-debug`, `linux-clang-asan` |
| `ctest` after recording | **34 of 34** on `linux-gcc-release` (385 s), `linux-clang-release` (349 s), `linux-gcc-debug` (1084 s), `linux-clang-debug` (1131 s), `linux-clang-asan` (4421 s; longest entry `-chronology-property` 591 s); two jobs, beside the sweep |
| Slow entries | `ctest --preset linux-gcc-release-slow`: **12 of 12** (1611 s, two jobs) |
| `expect` | 15 of 15 on each of the five presets |
| Lint | `--self-test` passes (18 snippets); the scan finds 0 findings (164 C++ files, 13 CMake files) |
| Purity | `--threads` 1, 2 and 6 byte-identical at default seeds 0, 40 and 88, and equal to the sweep worlds. 512 seed 42 at 5 threads equals golden `geo-seed-42-512`. Calm seed 7 at 6 threads equals the sweep world |
| Reference `p3_conflict_reference.py --version 0.20.0 check-world` | **375 of 375**, 0 failures |
| Peoples reference `check-world` (the `elf` set) | 20 of 20 |
| My independent check (`check11.py`) | 375 of 375 (after one fix to my own check, section 1) |
| Reference golden files (`p3-0x-*.txt`) | untouched; they pass in `ctest` |

## 1. What was run

The scripts are the P3-10 second round's, with their paths changed and these additions.

**`check11.py`** (built from `check10.py` by `V/mkcheck11.py`):
- Kinds 39 and 40 are read as `claim` and `region_ceded` by every earlier check, so (13), (15), (16), (17), T10 and the claims and cessions rows count them (P3-06 23.2 "kinds 20 and 39 are read alike").
- **Claims (23.2).** A kind 20 claim's holder holds R at the claim; a kind 39 claim's holder does not. Its `settlement` participants are a non-empty ascending list of the holder's standing settlements in R at the claim. Each kind 39 claim is counted by R's holder: the claimant, a third civilization or no one.
- **Cessions (23.3).** A kind 29 cession opens a `region_control` row and a kind 40 cession opens none. Kind 40's settlements are exactly the settlements that pass in the replay, in ascending id. The foothold cessions are exactly the kind 40 events.
- **War goals (23.4).** For each declaration whose region differs from its claim's, the reason, read from the rows. *Lost*: the defender neither holds R_c nor has a standing settlement in it. *Capital*: R_c is the defender's capital region. *Weary*: the rest. Weary is found by elimination, not recomputed.
- **Number words (P3-04 21.1).** No sentence may hold a value from 1 to 9 as a lone digit. Sentences with a number word are collected.
- **Names (P3-02 14).** My own syllable split, written from 14.2's text, is applied to every replayed attempt. It must agree with the reference's flag on every attempt, and no stored name may come from a repeated attempt (invariant 8). Faerie names may hold no `p` or `b`. `ee` and `ou` are counted.
- **One fix to my own check.** It expected the plague total as digits. In civs4 seed 16 (e1520) the C++ correctly writes "Plague broke out in Liafla; nine died." by 21.1. I corrected the check and re-ran that world, which then passes. No other world reached that case.

**New scripts:**
- `orc11.py` computes T11 and the other orc figures of 23.1 from the rows. It reproduces 0.19.0's 232, T5 98 / 50, T9 7 and T8 71 exactly.
- `rounds11.py` is `rounds.py` with kinds 39 and 40.
- `agg11.py` sums the P3-11 counters per set.
- `ref_r19.py` runs the test writer's `r19_facts` on the stored rows. It agrees with my figures in every one of the 100 default worlds: T11, kind 39 by holder (45 / 74 / 10), kind 40 (2) and goal differences (280).
- `orc_timeline.py`, `critic_lists.py` and `chronicle11.py` are for the critic.

**Sweep.** `run-sweep.sh` ran 3 workers and took 7250 s. Each world got:
- `generate`, `hash --detail` and `report`;
- the C++ sentence dump;
- the region-name reference;
- `check11.py`;
- the reference's `check-world` (and the peoples reference for `elf`).

Jobs were ordered with the default, calm and turbulent sets first. The set of jobs is the same as 0.19.0's (`jobs-ordered.txt` sorts to `jobs.txt`).

## 2. Goldens

**Why they changed.** Every world changes through `content_hash`:
- the race field `awakening_population`;
- the faerie lists;
- the filter lists;
- the two `history.report` entries.

`meta.generator_version` changes too.

- **P0 worlds** (`p0-worlds.txt`). The new empty and pattern worlds differ from 0.19.0's only in `meta.generator_version` and `meta.content_hash`. If the 0.19.0 values of those two strings are written into the new file's bytes, the SHA-256 equals the 0.19.0 golden exactly, for all 8 cases (`V/subst.py`).
- **Geography and history cases** (`p1-worlds.txt`, `p3-worlds.txt`). Compared with 0.19.0's detail hashes, no layer and no geography table changes, the region names included. `meta`, the history tables and (in `geo-phrase-512`, `geo-seed-1-1024`, `hist-dense-1024` and `hist-races-elf-orc-1024`) `god` change.
- **Inline constants.** `GOLDEN_CASES` in `db_determinism_test.cpp` and the two hashes in `stages_pipeline_test.cpp` hash worlds made with no data. Spec section 4 expected them not to move, but they do, because `meta.generator_version` is in those files.
  - I kept the test's files with an `LD_PRELOAD` shim (`V/keepdb/`) and wrote "0.19.0" back in place of "0.20.0" in their bytes. That reproduces all seven old constants exactly, so the version is their only change.
  - The constants are re-recorded, with the version in their comments.
  - *For the spec writer*: section 4's row should say that these constants move with every `GENERATOR_VERSION` change.

`worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase a61bb80dc02958092134a42f03a5d2233d6fbfe33996cfb699879b736ee5f92a
empty-seed-0 76b437fb29f68d1f4d5974a53f5b86a9665044208770a8f1113243576381fc08
empty-seed-0-512 8f076e477a6e5fad7b62d2aa0d524b951ff3f6c7b414b1fd265fb96ba0c76909
empty-seed-1 60a192cda2ec466203a1962628b8756bbabda6c64445d94a55194939a330e48c
empty-seed-42 36b4372d11ef8301640656ebf5ed1378376e2082d8125858ee4faba0838195ad
empty-seed-max 6da6a8ee169a46fc14e1e8df0615b9f761b5c4727df17c9355af5c32ff3fe874
pattern-seed-0 343e5094896c467aa88cb311d800e3f7b32e0352ba1939fc4c0cdd9e74b97932
pattern-seed-42 c77c9b5c33f08fa2a49c4dc2c8ea98cfb671863786066ee03d206fd7b3c378ff
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 2e1362c82c0a6f542a2d5a54825d9f5ce18f3e06f135d16564706f7aa856ca0f
geo-seed-0-512 a5aee363024837487dc79d60e9619259d5abe1dbce6d6831b41f7c888fab60cf
geo-seed-1-1024 095ab58cf4c3b4195b13e1e8f9e679426c9fb2b61200358fb35f332156277969
geo-seed-42-512 0cef1d7c779fce71a81cf041368842a932874b9386fac3b4c0e54c386494cc33
```

`worldgen/tests/golden/p3-worlds.txt`:

```
hist-dense-1024 9a0779c8c3e1badcbc0cdb7e35bd4fa0ecb784e9f67fe31af4e1c2ba63a03b32
hist-races-elf-orc-1024 97cba4b3791410300e3666b308b7412f0ecb6059cef51194423c0dcba8f09933
hist-span-2000-512 8223d489a84964c5375ab267233980634f0b8bd2917e7e87e53eca69802f1fff
```

Inline (no data, generator 0.20.0):

| Case | Hash |
| --- | --- |
| empty-phrase | 8eccdc0efeb9188bd15a5d1ca77bb08d197a6ddbbbfdcdbd8e4b88c329424d9c |
| empty-seed-0 (also `stages_pipeline_test.cpp`) | 68670576a71c6ebd743aad4218f2a5a149cea4443c73a1d366473281495f5101 |
| empty-seed-1 | fe512c0f980e2e5fea10237df02a086a7cbe963f50de1c36ddfa216748423e88 |
| empty-seed-42 | ab74a45ad09e390862f5125dd419dc3fa3bd47491e1427ae45385c1fb55dc0b2 |
| empty-seed-max | d7385c19d2b0c7893c330f164a5c9e43d6795e1a16160a7442183a83ca31ec68 |
| test pattern seed 0, `stages_pipeline_test.cpp` | d668c8fcf263c05f7e49bf1fad11775d7489197258edb857d2354da6d2158a35 |

**README phrase world.** `generate --seed-phrase "The Unmapped"` hashes to `1ba4dd20a119f6408956fe390220f34a832a72da4c53148ad46d6f0ae1e686d8` (0.19.0: `f6f6fd25…`).
- Its report is `V/readme-report.txt`: 11 civilizations, 600 events, 80 outbreaks, 7 quiet stretches, 18 wars, 5 standing orc settlements, no kind 39 or 40.
- I did not edit the README.

## 3. Defects

None. The reference replays every world of the sweep with 0 failures. My own check finds no broken invariant, no malformed sentence and no rule-breaking name.

## 4. Targets at 1024 defaults, seeds 0–99

| Target | Bound | 0.19.0 | **0.20.0** | |
| --- | --- | --- | --- | --- |
| **T11** median years, awakening to first orc colony, declaration, sack or capture | below 100 | 232 | **9** | met |
| **T9** worlds with exactly one standing orc settlement | at most 10 | 7 | **11** (seeds 5, 7, 11, 37, 55, 65, 76, 80, 81, 88, 95) | **missed** |
| **T10** worlds where the orcs do not act after 5750 | lead: at most 2 (spec 0) | 0 | **3** (seeds 0, 29, 90; all peaceful realms of 4 to 6 settlements) | **missed by one** (section 4.3) |
| **T5** orc wars won / lost | won ≥ lost | 98 / 50 | **168 / 95** | met |
| quick aim wins (22.5) | at most 3 | 0 | **0** | met |
| **T1** hollow wars ‰ | at most 250 | 227 (509 / 2237) | **224 (518 / 2307)** | met |
| T2 worlds with `wars_same_goal_unchanged_max` > 3 / inv (12) | 0 / 0 | 0 / 0 | **0 / 0** | met |
| T3 `refuge_held` reason 1 with free candidates | 0 | 0 | **0** (1 without) | met |
| T4 worlds with an extinct race, every set | 0 | 0 | **0** | met |
| T6 last-battle winner as loser / T7 | 0 / 0 | 0 / 0 | **0 / 0** (aim 0) | met |
| quiet stretches under 50 years | 0 | 0 of 729 | **0 of 729** (shortest 50) | met |
| names with a triple letter, `pee`/`poo`/`tit`/`fass`/`cum`, `tard`; fallbacks | 0 | 0 | **0; 0** | met |
| names from an attempt with a repeated syllable (P3-02 14.2 inv 8) | 0 | — | **0** (replay, my split agreeing with the reference on every attempt) | met |
| faerie names with `p` or `b` | 0 | 202 and 158 of 1059 | **0 of 1061** | met |
| sentences with a value 1–9 as a digit | 0 | — | **0** (375 worlds) | met |
| T8 worlds with an orc declaration (reported) | — | 71 | **83** | — |

### 4.1 T11

- **Distribution (100 worlds).** Median 9, 10th percentile 1, 25th 3, 75th 19, 90th 32. The minimum is 0 and the maximum 477.
  - By decade: 52 worlds under 10 years, 27 at 10–19, 9 at 20–29, 5 at 30–39, 3 at 40–49, 2 at 50–59 and 1 at 60–69.
  - One world is above 100 (seed 88, 477).
  - 0.19.0: median 232, 10th percentile 198, 90th 285, and 89 worlds at 200 or more.
- **The first act** is a colony in 99 worlds. In one world (seed 88) there is no act at all.
- **Worlds with 200 or more silent years:** 1 (0.19.0: 89; the spec quotes the critic's 88).
- **First orc declaration:** 83 worlds, median 59 years after the awakening (10th percentile 17, 90th 290; 0.19.0: 244).
- **First colony:** median 9 years after the awakening. From the first colony to the first declaration: median 47 years (10th percentile 4, 90th 282).
- **First wars of the awakened orc realms** (85 realms; 19 declare none):
  - Not one is declared with at most one standing settlement (0.19.0: 8).
  - Outcomes: won 28, lost 33, truce 14, unfound 9, open at E 1.
- **Residual cases of P3-05 23.2,** over the 104 awakened orc civilizations of the 100 worlds:

  | Case | Civilizations | Their own T11 (median) |
  | --- | --- | --- |
  | fortress capital at the awakening | 1 (seed 4) | 12 |
  | home held by another civilization at the awakening | 0 | — |
  | no colony ever (no target) | 1 (seed 88) | 477 |
  | ordinary | 102 | 9 |

  - **Seed 88, the one residual world.** The Warband of Azok wakes in Thane's Leas. Two goblin settlements of the Warrens of Drak-Slark (c4) still stand there, and all five walkable neighbours (regions 25, 27, 30, 34, 36) are held by other realms.
  - So the pending first colony never finds a target, which is the "home with no free site and no fit neighbour" case. The capital grows to a town of 1,517 and lives under the mark for 396 years.
  - The realm declares two late wars and loses both. In 5920 its capital is sacked and it flees to Koshkom (refugees_settled e1020), so it is also one of the T9 worlds.
  - P3-11 10 question 2 predicted exactly this residual. It is rare (1 in 100), and it is not what misses T9 or T10.

### 4.2 T9: 11 worlds

Every T9 world has one civilization, still alive, holding one town or village. The orcs founded more and lost them in war:

| Seed | Founded | Wars lost (won) | Sacked / taken / ceded from orcs | Flights | At the end |
| --- | --- | --- | --- | --- | --- |
| 5 | 6 | 3 (1) | 2 / 1 / 1 | 1 | Ukh'kum, 2,575 |
| 7 | 3 | 2 | 1 / 1 / 0 | 1 | Orz'thugh Camp, 6,503 |
| 11 | 3 | 3 | 1 / 1 / 0 | 1 | Borz, 4,197 |
| 37 | 5 | 5 (2) | 3 / 1 / 3 | 0 | Aagh'zosh Camp, 3,027 |
| 55 | 3 | 1 | 1 / 1 / 0 | 1 | Kraagthokh, 1,928 |
| 65 | 2 | 1 | 0 / 1 / 0 | 0 | Kruuzkaal Fort, 3,153 |
| 76 | 6 | 2 | 2 / 3 / 0 | 1 | Thulshaal Camp, 1,294 |
| 80 | 2 | 3 (1) | 2 / 0 / 0 | 0 | Oz'ugo Fort, 6,755 |
| 81 | 4 | 1 | 3 / 0 / 0 | 0 | Zogh Fort, 1,563 |
| 88 | 2 | 2 | 1 / 0 / 0 | 1 | Koshkom, 5,658 |
| 95 | 5 | 1 (1) | 3 / 1 / 0 | 1 | Mugh Camp, 2,038 |

- **Overlap with 0.19.0.** Four of the eleven seeds (55, 65, 81, 95) were T9 worlds at 0.19.0 too.
- **Why.** The orcs now reach their quarrel and their wars a century and a half sooner. Against 0.19.0:
  - declarations 388 against 217;
  - wars lost 95 against 50, 87 of them as attacker (0.19.0: 36);
  - settlements sacked from them 67 against 39, taken 30 against 16, regions ceded 26 against 14;
  - flights 16 against 7.

  They also found more (771 against 617) and stand at 859 settlements against 706. So more realms end reduced to one town, while the race as a whole is stronger.
- **Several of the single towns are large** (6,503, 6,755, 5,658). A capital needs 750‰ of min(share, 7,999) to be ready again by pressure, so a lone orc town can sit for a century before it colonises again.
- **The levers,** for the lead, as P3-06 23.1 lists them for T5: `quarrel_min_settlements` 3, then orc `odds_min_permille` 600. Fewer early losing wars is what T9 needs. I did not try them.

### 4.3 T10: 3 worlds, each described

| Seed | The orc realm | Last orc act | At the end |
| --- | --- | --- | --- |
| 0 | The Krurz Horde. First colony at +3; its first war, at +82 on Clan Drourd (dwarves), lost in 5622; then it settles Elmholt | colony 5742 | 4 settlements (Aark'ugok, town of 1,766, and three villages of 588), all in Elmholt; no open war |
| 29 | Clan Uush'gha. First colony at +1; it never claims and never declares | colony 5724 | 6 settlements (10,885 people) in three regions; no war ever |
| 90 | The orcs' first colony at +16; one claim at the awakening, never pursued | colony 5747 | 4 settlements (5,150) in one region; no war ever |

- **They are peaceful, not broken.** Each is a realm of several settlements living at peace, which is the case the lead's ruling accepts ("Paul accepted orcs at peace"). None is a broken or frozen single-settlement realm. Each is silent after 5750 because its colonies now wait for pressure (P3-05 23.2) and it has no open quarrel.
- **The count still breaks the bound** of at most 2 by one, and the spec's own bound is 0. A ruling is needed.
- **Outside the defaults:** T10 is 2 of 20 in `civs4` (as at 0.19.0) and 2 of 30 at 512 (0.19.0: 1). It is 0 in every other set, `span2000` included (0.19.0: 6).

### 4.4 Calm and turbulent (20 worlds each)

| Set | T11 median (0.19.0) | T5 (0.19.0) | T9 (0.19.0) | T10 (0.19.0) | T8 |
| --- | --- | --- | --- | --- | --- |
| calm | **6** (221.5) | **40 / 19** (28 / 6) | **0** (2) | **0** (0) | 19 of 20 |
| turbulent | **17** (228.5) | **50 / 27** (23 / 22) | **1**, seed 3 (2) | **0** (0) | 18 of 20 |

- In `turbulent` one world exceeds 100 years (391) and two exceed 100 in all.
- Turbulent T5 is no longer close: 50 won against 27 lost.

## 5. Reported statistics (1024 defaults unless stated)

**Orcs.**
- **People and settlements.** 1.96 M people (0.19.0: 1.51 M) and 859 standing settlements (706). Per world, orc standing settlements are 1 – 20, median 6.5 (0.19.0: 5).
- **Race shares, people ‰ / standing settlements ‰,** 0.20.0 against 0.19.0:

  | Race | 0.20.0 | 0.19.0 |
  | --- | --- | --- |
  | goblin | 439 / 494 | 436 / 496 |
  | human | 299 / 132 | 318 / 140 |
  | dwarf | 93 / 94 | 96 / 99 |
  | orc | **88 / 95** | 68 / 79 |
  | elf | 55 / 113 | 56 / 114 |
  | faerie | 22 / 69 | 22 / 69 |

  The orcs' gain comes mostly from the humans.
- **Declarations by race:** goblins 1910, orcs 388, humans 146, dwarves 132, elves 41, faeries 4. The goblins' share is 728‰ (0.19.0: 774‰).
- **No race goes extinct in any of the 375 worlds.** No orc civilization is extinct either.

**Claims and cessions** (P3-11 8 item 2):

| Set | Kind 20 claims | Kind 39 claims (claimant / third / no one) | Kind 39 per world (min / median / max) | Kind 40 (worlds) | Kind 29 |
| --- | --- | --- | --- | --- | --- |
| default | 4148 | 129 (45 in 29 worlds / 74 in 32 / 10 in 9) | 0 / 0 / 13 | 2 (seeds 40, 67) | 815 |
| civs4 | 3007 | 127 (22 / 96 / 9) | 0 / 5 / 18 | 5 (3 worlds) | 446 |
| d512 | 836 | 39 (14 / 24 / 1) | 0 / 1 / 7 | 1 | 96 |
| d2048 | 456 | 14 (3 / 11 / 0) | 0 / 1 / 12 | 0 | 121 |
| dense | 1015 | 48 (11 / 35 / 2) | 0 / 1 / 12 | 1 | 212 |
| span12000 | 1611 | 108 (32 / 66 / 10) | 0 / 3 / 28 | 1 | 314 |
| turbulent | 1602 | 79 (16 / 51 / 12) | 0 / 3 / 15 | 0 | 280 |
| calm | 430 | 14 (6 / 6 / 2) | 0 / 0 / 2 | 0 | 114 |

- **Default worlds.**
  - The critic's 46 "claims on a region the claimant held" are now kind 39: 45 such claims in 29 worlds.
  - Of the 129 kind 39 claims, 74 are on a region held by a third civilization and 10 on one held by no one.
  - The kind 39 claims list 157 settlements. None of the holder's standing settlements in R was left out, so in the sweep no hidden settlement stood in a claimed region.
- **Every world, every set:**
  - no kind 20 claim whose holder does not hold its region at the claim;
  - no kind 39 claim whose holder does;
  - no kind 29 cession that opens no `region_control` row;
  - no kind 40 cession that opens one;
  - kind 40's settlements are exactly those that pass.

  The 0.19.0 footholds per set were 2 at defaults, 5 in civs4, 4 at 512, 1 at 2048 and 1 in dense. At 0.20.0 they are 2, 5, 1, 0, 1, and 1 in span12000.

**War goals (23.4).** Declarations whose region differs from their claim's: 280 at defaults (per world 0 / 2 / 9).
- **By reason:** lost 135, capital 104, weary 41. "Weary" is found by elimination: neither lost nor the capital.
- The reference counts the same 280.
- civs4 has 223 (lost 146, capital 47, weary 30), turbulent 99 and span12000 98.

**Names** (P3-02 14.2, 14.3).
- **Rejection rates, from the test writer's scan** (`p3_names_reference.py scan`, the 59,400-name sample of 13.1, `V/names/scan-r15.txt`), counted as attempts rejected by 14.2 against all attempts:

  | Culture | Repeated-syllable rejections / attempts | Rate | `ass` | `tard` | the four words |
  | --- | --- | --- | --- | --- | --- |
  | dwarven | 7 / 13,314 | 1 in 1,902 | 0 | 20 | 0 |
  | elven | 83 / 11,406 | 1 in 137 | 0 | 0 | 0 |
  | faerie | 63 / 11,019 | 1 in 175 | 205 (1 in 54) | 0 | 13 |
  | goblin | 58 / 12,768 | 1 in 220 | 0 | 0 | 0 |
  | human | 6 / 17,810 | 1 in 2,968 | 0 | 38 (1 in 469) | 0 |
  | orcish | 12 / 11,530 | 1 in 961 | 0 | 0 | 0 |

  Fallbacks: 0 in every culture.
  - **The spec's estimate should be corrected.** P3-02 14.2 says "about one attempt in 60 to 150 for a culture with two-syllable shapes". Only the elves fall in that range, at 1 in 137. Faerie (1 in 175) and goblin (1 in 220) are near it, while orcish (1 in 961), dwarven (1 in 1,902) and human (1 in 2,968) are far rarer. The spec writer may replace the sentence with "from 1 in 137 (elven) to 1 in 3,000 (human)".
  - **The other two estimates hold.** 14.1's "about one root in 50" for faerie `ass` is confirmed (1 in 54), and so is 14.3's "about 1/450" for human `tard` (1 in 469).
- **In the default sweep's stored names** (replayed entities): 50 attempts were rejected as repeated (elven 19, goblin 19, faerie 7, dwarven 2, human 2, orcish 1). Triple-letter rejections: 5, all faerie.
- **Faerie names, 100 default worlds:** 1061, of which 124 hold `ee` and 182 `ou`. At 0.19.0 there were 1059, with 200 `ee`, 273 `ou`, 202 `p` and 158 `b`.
- **Regions** are untouched: every region table hash is unchanged in the golden cases, and the region-name reference agrees in every world. `Reekpool Thicket`, `Jaypool Shaw` and `The Pool of Whispers` (2048) remain; region names are not covered by the filter.

**Sentences.**
- No value 1–9 is written as a digit in any of the 375 worlds.
- There is no "0 of the victors", no "its seat" and no malformed separator.
- 7,800 outbreaks; none of salience 45 shows a total below 1,000.
- Dwindles: 1,349 with a cause clause and 48 without. Abandonments: 54 of 54 with a clause.

**`search_abandoned`** per world: 0 / 3 / 9, median 3 (0.19.0: 0 / 3 / 11; P3-09's search median target, at most 3, is still met).

### 5.1 Typical ranges for P3-06 15.5 at 0.20.0 (5th–95th percentile, 1024 defaults, 100 worlds)

| Statistic | 5th–95th | Median | 0.19.0 (5.1 with the addendum) |
| --- | --- | --- | --- |
| events | 404 – 1205 | 681.5 | 382 – 1219, 675.5 |
| wars | 9 – 48 | 25 | 9 – 51, 23 |
| flare-ups | 1 – 8 | 4 | 1 – 8, 4 |
| claims (kinds 20 and 39) | 17 – 84 | 38 | 17 – 85, 38 |
| battles | 29 – 197 | 81.5 | 30 – 191, 76 |
| battles involving faeries | 0 – 12 | 4 | 0 – 11, 4 |
| sacked | 5 – 25 | 12 | 5 – 26, 12 |
| taken | 1 – 25 | 6 | 1 – 21, 6 |
| cessions (all, kinds 29 and 40); of 20.3 | 2 – 15; 0 – 10 | 7; 5 | 2 – 15; 1 – 11, 8; 4.5 |
| tributes | 1 – 13 | 5 | 1 – 13, 5 |
| flights | 0 – 2 | 0 | 0 – 2, 0 |
| endings: peace / war_won / truce | 3 – 15 / 1 – 15 / 1 – 14 | 8 / 5.5 / 6 | 2 – 16 / 1 – 17 / 1 – 15 |
| endings: search_abandoned | 1 – 6 | 3 | 1 – 6, 3 |
| endings: conquest | 0 – 2 | 1 | 0 – 2, 1 |
| `wars_hollow_permille` | 0 – 333 | 224.5 | 43 – 375, 220.5 |
| `wars_same_goal_max` | 2 – 5 | 3 | 2 – 5, 3 |
| `wars_same_goal_unchanged_max` | 2 – 3 | 3 | 2 – 3, 3 |
| largest race share of people ‰ | 326 – 798 | 529.5 | 303 – 802, 564.5 |
| `race_regions_share_max_permille` | 260 – 775 | 457.5 | 268 – 751, 454 |
| largest civilization land share | 113 – 347 | 208 | 117 – 367, 208 |
| civilizations; ended | 8 – 14; 0 – 2 | 10; 1 | 8 – 14; 0 – 2 |
| simulated years; ‰ | 1200 – 1540; 200 – 256 | 1337.5; 222.5 | unchanged |
| summary threshold | 26 – 36 | 31 | 26 – 41, 31 |
| first war after the goblins' awakening (years) | 299 – 2704 | 812.5 | unchanged |
| standing: faerie / elf / orc / human / dwarf / goblin | 2–13 / 2–26 / 1–20 / 3–25 / 4–19 / 5–89 | 5.5 / 8 / 6.5 / 10 / 7.5 / 39 | 3–13 / 1–27 / 1–17 / 3–28 / 4–18 / 6–96 |

## 6. Generation time

`V/timing/gen-times.txt`: seed 0, three runs at each size, the merged 0.19.0 binary and data (`p3-10-validation2/bin-019b`) interleaved with 0.20.0's. The machine was not quiet: the asan `ctest` ran beside it at two jobs, with a load average of 5 to 9. Both versions saw the same load because their runs alternate.

| Map | 0.19.0 | 0.20.0 |
| --- | --- | --- |
| 512 | 3.30–5.40 s | 3.31–4.39 s |
| 1024 (default) | 11.19–12.89 s | 10.97–11.38 s |
| 2048 | 43.70–44.34 s | 43.66–44.62 s |

There is no slowdown: the default world is within the noise, and if anything faster. The sweep's per-world times (`V/sweep/gen-times.txt`) show a median of 36.3 s for a default world against 26.2 s in the 0.19.0 sweep. That gap is load, not the generator: this sweep's default worlds ran beside the debug and asan builds, `ctest` and four `world-hashes.sh` runs.

## 7. Critic folder

`V/critic/`, with `INDEX.txt` saying what each world is for. Every world there is a byte-identical copy of its sweep world.
- **Full event logs** (`.chronicle.txt`) of 14 default worlds: seeds 0, 1, 2, 3, 5, 7, 11, 23, 40, 42, 58, 65, 73 and 88. Also calm seeds 0 and 7 and turbulent seeds 11 and 14. Each has its map, report, names and summary.
- **`new-kinds-sentences.txt`:** all 129 kind 39 and 2 kind 40 sentences of the 100 default worlds.
- **`faerie-names.txt`:** 300 faerie names at 0.20.0 beside 300 at 0.19.0, the same seeds.
- **`number-word-sentences.txt`:** every sentence with a number word in seeds 0, 7 and 40 (182 lines).
- **`orc-timeline-default.txt`:** the orc timeline for each of the 100 default worlds, and the same for calm and turbulent.

## 8. Findings for the lead

1. **T9 missed: 11 worlds against at most 10** (section 4.2). It is a target miss, not a defect. The orcs act at once, as T11 wants, and lose more early wars as attackers. P3-06 23.1's levers for T5 (`quarrel_min_settlements` 3, then orc `odds_min_permille` 600) are the ones that would also reduce T9. A ruling is needed before the merge.
2. **T10 missed by one: 3 worlds against the ruling's at most 2** (section 4.3). All three are peaceful realms of four to six settlements, the kind the ruling accepts. If the bound is read as "at most 2 unless the extra worlds are of the accepted kind", the target is met. A ruling is needed.
3. **The one residual T11 case** is seed 88 (section 4.1): a home shared with goblins and ringed by held regions. 10 question 2's suggested fix (outward targets from the home's neighbours for a pending civilization) would not help it, since every neighbour is held. It is reported, not blocking.
4. **Text corrections for the spec writer:**
   - P3-02 14.2's "1 in 60 to 150" (section 5);
   - P3-11 section 4's row for `GOLDEN_CASES` and the pipeline hashes, which move with every version (section 2);
   - the 0.19.0 count of 200-year silences is 89 by 23.1's definition, not 88.
5. **Typical ranges** (5.1) to replace P3-06 15.5's table.
6. **T8 is 83 of 100** (0.19.0: 71), above Paul's "about 80%".

## 9. Files

Under `~/.cache/the-unmapped/out/p3-11-validation/`:

| Path | Contents |
| --- | --- |
| `logs/summary.txt` | every step's exit status and time |
| `logs/build-*.log`, `logs/ctest2-*.log`, `logs/slow-linux-gcc-release.log`, `logs/wh-gen-*.log`, `logs/world-hashes-compare5.log`, `logs/expect-*.log`, `logs/lint*.log`, `logs/ctest0-other.log` | builds, tests, hashes, lint |
| `sweep/<set>/` | per world: `.db`, `.txt`, `.report.txt`, `.sent.txt`, `.regnames.txt`, `.check.json`, `.ref.txt` (`.pref.txt` for `elf`) |
| `sweep/ref-results.txt`, `sweep/pref-results.txt` | reference results |
| `sweep/stats11.txt`, `sweep/agg2.txt`, `sweep/agg11.txt`, `rounds11.txt`, `orcs-default.txt`, `orcs-calm.txt`, `orcs-turbulent.txt`, `orcs-compare-default.txt`, `ref-r19-default.txt`, `stats-default.txt` | statistics |
| `names/scan-r15.txt` | the test writer's 59,400-name scan |
| `critic/` | the critic folder and `INDEX.txt` |
| `purity/`, `timing/`, `readme/`, `readme-report.txt`, `keepdb/` | purity, timing, the README phrase world, the inline-golden check |

The scripts are in the same folder: `check11.py` (from `mkcheck11.py`), `orc11.py`, `rounds11.py`, `agg11.py`, `ref_r19.py`, `orc_timeline.py`, `critic_lists.py`, `chronicle11.py`, `subst.py`, `sweep-job.sh`, `run-sweep.sh`, `gen-one.sh`, `chain-tests.sh`, `critic/make.sh`.
