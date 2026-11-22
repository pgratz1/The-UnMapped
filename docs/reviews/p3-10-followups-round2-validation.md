# Validation: P3-10 second round (generator 0.19.0 r17, schema 7)

Validation specialist, 2026-11-21.

**Input.** Branch `p3/10-validation2`, cut from `origin/p3/10-int2` at `25187f1`. That is the first round plus the fixes after the world critic's fail. Nothing else was merged.

**Specs.**
- `docs/specs/p3-10-followups.md` section 12 (r3).
- P3-06 r17 section 22.
- P3-04 r15 20.1 item 1a.
- P3-05 r15 22.7a.
- P3-02 r13 section 13.

**Baselines.**
- First round (0.19.0 r16): `~/.cache/the-unmapped/out/p3-10-validation/` (sweep, critic folder, binary).
- 0.18.0: `~/.cache/the-unmapped/out/p3-09-validation/`.

The second round's sweep uses the same 375 jobs (settings and seeds) as both of them, so every comparison below is pair for pair.

## Verdict

**Fail, on one defect.**

- **D1 (blocking).** In T6 the C++ war check tests exhaustion **before** aim and the goal-lost ending. P3-06 21.6 and the reference test aim first.
  - **Where it shows.** In 1 of 375 sweep worlds (1024 default seed 73), the C++ writes a truce where the reference ends the war by aim. The reference's `check-world` then fails on 11 tables.
  - **A probe of the fix.** I built a throw-away copy with the order corrected; I did not commit it.
    - It makes seed 73 pass the reference (0 failures).
    - It changes no other sweep world: 374 of 375 are byte-identical.
    - It leaves all eight geography and history golden cases unchanged.
  - Section 3 has the reproduction and the root cause.
- **Gates.** Apart from D1 every gate passes.
  - Every merge target of the round is met (section 4).
  - My own independent check passes all 375 worlds. It does not model the order of the war checks, which is why it does not see D1.
- **Recorded hashes.** They are recorded on this branch (section 7). They match on all five presets and in the extension build.
  - The probe shows that D1's fix does not change any of the fifteen cases.
  - The engineer's fix must still be re-run through `world-hashes.sh local` and the reference sweep before the merge.

| Gate | Result |
| --- | --- |
| Build, five Linux presets | warning-free (0 warnings in every build log) |
| `ctest` round 1 (before recording) | `linux-gcc-release`, `linux-clang-release`: 33 of 34 pass. Only `worldgen-cli-tests-other` fails, and only on `p0-worlds.txt` constants. The sorted `values:` lines are identical on both presets. The inline constants did not move (empty worlds with no data; the version stays 0.19.0) |
| `world-hashes.sh local`, five presets | **15 of 15 identical** |
| `ctest` round 2 (after recording) | **34 of 34 on all five presets** |
| `expect` | 15 of 15 on each of the five presets |
| Slow entries | `ctest --preset linux-gcc-release-slow`: 12 of 12 (1340 s, two jobs). `linux-clang-release -L slow`: 12 of 12 (1450 s). `ctest --preset linux-gcc-release -N` lists no slow entry |
| Extension (`linux-gcc-release`, GDEXT on, separate tree) | 35 of 35, `gdext-headless-check` included (7.8 s). `expect` 15 of 15. Byte-identical to the plain build in all 15 cases |
| Lint | `--self-test` passes (18 snippets). The scan finds 0 findings |
| Purity | `--threads` 1, 2 and 6 byte-identical. `--until history` equals the default. 512 seed 42 at 1 and 5 threads equals golden `geo-seed-42-512` |
| Reference `p3_conflict_reference.py --version 0.19.0 check-world` | **374 of 375**; fails default 1024 seed 73 (D1) |
| Peoples reference `check-world` (the `elf` set) | 20 of 20 |
| My independent check (`check10.py`, extended for r17) | 375 of 375 |

## 1. What was run

Evidence is under `~/.cache/the-unmapped/out/p3-10-validation2/` (V below). The scripts are the first round's, with their paths changed. The additions are listed here.

**Changes to `check10.py`:**
- **Hidden years per race.** Read from the race files. It fails if `history.conflict` still has `hidden_years`.
- **(15) restated (22.3).**
  - y lies low when the latest `refugees_settled` with founder y has Y < Y_f + `hidden_years`(r_y).
  - Its refuge must also be standing and y's, read from the replay just before the event.
  - The check covers every colony, claim and declaration of y.
- **(8) restated.** The aim exemption needs the attacker as owner **and** no standing settlement of the loser in G.
- **(14).** Unchanged; it keeps the owner reading.
- **New (17).**
  - It reads the settlement owners after the ending and its `region_ceded` or `tribute`.
  - If W holds G, then L may keep no standing settlement in G, unless L's capital stands there.
- **New counts:**
  - foothold cessions (a `region_ceded` that opens no `region_control` row);
  - quick aim wins (22.5's definition);
  - wars over a region the attacker held at the declaration, by outcome;
  - T10.
- **Quiet lines (P3-04 r15 20.1 item 1a).**
  - No quiet row shorter than 50 years may have a `quiet_stretch`.
  - No `quiet_stretch` may have `years` below 50.
- **Names.**
  - Every god, civilization and settlement name is scanned for three equal letters in a row and for `pee`, `poo`, `tit`, `fass`, `cum`.
  - Region names are scanned but not failed, since 13.2 does not cover them.
  - Filtered and triple-letter attempts are counted per culture.

**New scripts:**
- **`rounds.py`.** It measures T5, T9, T10, quick aim wins, footholds, held-region wars and settlements in another realm's region from the rows alone, so 0.18.0, r16 and r17 are measured the same way. Output: `rounds-old.txt` and `rounds-r17.txt`.
- **`agg2.py`.** It sums `check10.py`'s r17 counters per set. Output: `sweep/agg2.txt`.
- **`risk221.py` and `q221.py`.** These look at 22.1's risk. `q221.py` is a harness on the test writer's reference. It records, at every declaration, whether the pair's quarrel was open and how many visible standing settlements the attacker had. Each run must reproduce the stored log. Output: `q221/*.jsonl`.
- **`probe/`.** D1's probe:
  - `end-checks-order.diff`, the change applied to a scratch copy;
  - `probe-job.sh` and `results.txt`, the 375 sweep jobs regenerated with that copy and compared by file hash.

**Sweep.** `run-sweep.sh` with three workers took 6641 s. Each world got:
- `generate` and `hash --detail`;
- `report`;
- the C++ sentence dump;
- the region-name reference;
- `check10.py`;
- the reference's `check-world` (and the peoples reference for `elf`).

Statistics come from `stats10.py` against the first round's sweep (`sweep/stats10.txt`, where the "0.18.0" column means round 1), `agg2.py`, `rounds.py` and `orcs.py` (`sweep/orcs.txt`, where "0.19.0" means r17 and "0.18.0" means r16).

## 2. Tests

- **Round 1.** It ran on the two release presets. The debug and asan presets hold the same constants, which are read at run time from `p0-worlds.txt` (the entry is `worldgen-cli-tests-other`).
  - Failing test cases: 5 of 50 in that entry: `cli_golden_test.cpp` (the 8 golden cases) and `cli_p1_generate_test.cpp` (`--until none`, `--threads`, the test pattern and the data-root case, each comparing against `p0-worlds.txt`).
  - Every failing check compares a hash with a `p0-worlds.txt` value, and every computed value equals the hash `world-hashes.sh` produced for that case.
  - Nothing else fails.
- **Round 2.** 34 of 34 on all five presets (two jobs, beside the sweep, at load average 4 to 8).
  - The number of entries rose from 33 to 34: `worldgen-tests-engine` is split, with the new `-engine-r12`. That closes the first round's F4: `-engine` now takes 18.7 s and `-engine-r12` 20.1 s on release.

| Preset | Round 2 | Longest entries |
| --- | --- | --- |
| `linux-gcc-release` | 242 s | `-chronology-property` 32.8 s, `cli-chronology` 28.1, `-chronology` 24.5, `-balance` 24.3 |
| `linux-clang-release` | 254 s | similar |
| `linux-gcc-debug` | 1068 s | `-hydrology` 141 s, `-chronology-property` 136 |
| `linux-clang-debug` | 1095 s | similar |
| `linux-clang-asan` | 4429 s | `-chronology-property` 608 s, `-hydrology` 552, `-chronology` 551, `-balance` 525 |

No asan entry reaches 1200 s. `-chronology-property` is 32.8 s on release, as at 0.18.0 and in round 1; it is unchanged by this round.

## 3. Defect D1: exhaustion is tested before aim in T6's war checks

- **Seed and parameters.** Default 1024 seed 73: `--seed 73 --map-size 1024`, all other settings default.
- **Generator.** 0.19.0, branch `p3/10-int2` at 25187f1, shipped `game/data`.
- **Reproduce in one command.** From the repository root, with `linux-gcc-release` built:

  ```
  B=~/.cache/the-unmapped/build/$(basename "$PWD")/linux-gcc-release; "$B/tools/worldgen-cli/worldgen-cli" generate --seed 73 --map-size 1024 --data game/data --out /tmp/s73.db --force && python3 -B worldgen/tests/reference/p3_conflict_reference.py --version 0.19.0 check-world /tmp/s73.db --data game/data | tail -n 1
  ```

  It prints `... 7282 checks, 11 failures`. The first difference is `event` row 914: stored `(914, 5063, 5063, 28, 23, 0, 0, 50)` (a `truce`); reference `(914, 5063, 5063, 27, 23, 0, 0, 60)` (a `peace`).

**The war.** War e875 (5034–5063): the Snonugg Mob (c3) against the House of Fiegla (c2), over Moonwell Grove (region 12).
- In 5063 the Mob wins a battle (e911), besieges Eloglaevie (s103, e912) and takes it (e913).
- Eloglaevie was c2's only standing settlement in region 12, so `region_control` of region 12 passes to c3 (row from 5063, event e913).
- In the same T6 check the C++ writes e914, "The House of Fiegla and the Snonugg Mob made peace after 29 years of war, neither the victor." (kind 28, `truce`).
- The reference ends the war by aim: `w war e875 5034-5063 c3 c2 rg12 ... aim won-c3-tribute`.

**Expected.** P3-06 21.6 (amended by r17 22.5 item 1), at each check of 7.7:
1. Aim: G's owner is the attacker and the defender has no standing settlement in G. The attacker wins, `war_won` or `peace` when b is exhausted, and 20.3's cost applies.
2. Goal lost: the war ends by the score.
3. "Otherwise … 7.7 items 1, 3 and 4": exhaustion, no front and length.

So exhaustion is tested only when neither item 1 nor item 2 applies. Here item 1 applies: c3 holds region 12 and c2 has no standing settlement in it. The war must be won by c3.

**Root cause.** `ConflictSubsystem::end_checks` in `worldgen/src/history/conflict_war.cpp` (lines 498–536) runs `if (ex_a || ex_b) { end_war(ctx, i, by_score, loser_exhausted); return; }` before `aim_achieved(...)` and before the goal-lost test.
- In seed 73 one side is exhausted and the score is 0, so the exhaustion branch writes a truce.
- The order has not changed since P3-06 (generator 0.16.0, 41e89b4). Before r14 aim was the last test of 7.7, so the order did not matter then.
- **Why round 1 passed.** With r14's order the two readings agree whenever the score favours the attacker. Both write `peace` when b is exhausted and `war_won` otherwise. They differ only when the attacker holds its goal while the score is 0 or favours the defender. That happened in none of round 1's 375 worlds, and r17's new histories produce it once.
- **A second, linked point.** The C++ aim branch passes `sued = false` (always `war_won`), and so does the goal-lost branch. 21.6 item 1 and item 2 write `peace` when the loser is exhausted.
  - The exhaustion-first order hid this: every aim ending with an exhausted defender went through the exhaustion branch.
  - With only the order swapped, the probe's seed 73 diverges earlier, at e788 (4822): a `war_won` where the reference writes `peace`.
  - With the order swapped **and** `sued` set to "the loser is exhausted" in both branches, seed 73 passes the reference: 7359 checks, 0 failures.
- **Quiet steps.** The quiet-step path already does this right: `conflict.cpp:1403` calls `end_war(ctx, i, w.attacker, exhausted(ctx, w, 1))`.

**Probe** (`V/probe/end-checks-order.diff`; not committed, for the engineer's information):
- the exhaustion block moves after item 2 and before the no-front and length test;
- item 1 passes `ex_b`;
- item 2 passes `by_score != 0 && (by_score == attacker ? ex_b : ex_a)`.

With it:
- seed 73 passes the reference;
- 374 of the other sweep worlds are byte-identical (`V/probe/results.txt`);
- `geo-phrase-512`, `geo-seed-0-512`, `geo-seed-1-1024`, `geo-seed-42-512` and the three `hist-*` cases keep the hashes recorded in section 7;
- the per-stage goldens and the doctest suites were not run with the probe.

**Why my own check missed it.** `check10.py` checks the invariants of an ending, (8), (13), (14) and (17), but not which of 21.6's items decided it. It also does not recompute exhaustion. Seed 73's truce breaks no invariant: a truce has no loser. Only the reference, which replays the run, sees it.

**Weight.** It is blocking, because the reference gate is 0 failures. The fix belongs to the generator engineer, with a test for 21.6's order: aim before exhaustion, with an exhausted side and a score of 0 or one favouring the defender. Golden vectors must be re-checked by the test writer. Section 7's hashes should survive, by the probe, but must be confirmed by `world-hashes.sh local` on the fix.

## 4. Merge targets at 1024 defaults, seeds 0–99

Sources:
- r17 (this round): `rounds-r17.txt`, `sweep/agg2.txt`, `sweep/stats10.txt`;
- r16 (first round) and 0.18.0: the first round's report, `rounds-old.txt`, and the same scripts run on their sweeps.

| Target | Bound | 0.18.0 | 0.19.0 r16 | **0.19.0 r17** | |
| --- | --- | --- | --- | --- | --- |
| T1 hollow wars ‰ (hollow / found) | ≤ 250 | 230 (481 / 2083) | 234 (517 / 2203) | **227 (512 / 2246)** | met |
| T2 worlds with `wars_same_goal_unchanged_max` > 3 / inv (12) | 0 / 0 | 0 / 0 | 0 / 0 | **0 / 0** | met |
| T3 `refuge_held` reason 1 with free candidates | 0 | 1 (0) | 8 (0) | **3 (0)** | met |
| T4 worlds with an extinct race (every set) | 0 | 0 | 0 | **0** | met |
| T5 orc wars won / lost | won ≥ lost | 114 / 68 | 95 / 77 | **98 / 50** | met |
| T6 last-battle winner as loser (aim exempt) | 0 | 0 (aim 17) | 0 (aim 1) | **0 (aim 0)** | met |
| T7 | 0 | 0 | 0 | **0** | met |
| T9 worlds with one standing orc settlement | ≤ 10 | 7 | 21 | **7** (seeds 14, 38, 52, 55, 65, 81, 95) | met |
| T10 worlds where orcs do not act after 5750 | ≤ 2 | 0 | 9 | **0** | met |
| quick aim wins (22.5's definition) | ≤ 3 | 0 (2; see 4.1) | 22 | **0** | met |
| quiet stretches under 50 years | 0 | — (`quiet_era`) | 141 of 873 | **0 of 729** (shortest 50) | met |
| names with a triple letter or `pee`/`poo`/`tit`/`fass`/`cum` | 0 | 27 | 21 | **0** | met |

- **T9.** The engineer's seven seeds are confirmed: exactly 14, 38, 52, 55, 65, 81, 95. Five of them had one standing orc settlement in round 1 too.
- **Search median.** The `search_abandoned` median is 3 (target ≤ 3, met). Per world: 0 / 3 / 11; r16: 0 / 3 / 8.

### 4.1 The two figures from the interim notes

**Quick aim wins at 0.18.0: 0 against the spec's 2.**
- My 0 uses 22.5's definition: a `peace` or `war_won` won by the attacker, "decided by item 1" (the attacker holds G at the ending, by `region_control`), with no battle caused by the declaration and at most one year. That is the definition the r16 count of 22 and the target use.
- The spec's 2 is the number of attacker wins with no battle and at most one year **whatever the goal's owner**. At 0.18.0 both of those ended with G held by no one or a third party (7.7 item 2's old aim). I count 2 that way at 0.18.0, and 22 at r16 (all of them with G held by the attacker).
- So the spec's "0.18.0: 2" is the looser definition. Under its own definition the 0.18.0 baseline is 0. The spec writer may correct 22.4 and 22.5's tables to "0 (2 by the looser count)".

**Settlements standing in a region another realm holds: 151 in 64 worlds against the spec's 184 in 69.**
- My 151 counts standing settlements (status thriving or declining) whose region has an open `region_control` row of **another** civilization.
- The spec's 184 counts standing settlements whose region is **not held by their owner**. That also takes in regions with no open row, held by no one: 33 more at r16.
- I reproduce 184 in 69 exactly with that definition (`V/variants.py`).
- 22.5's wording ("in a region held by another civilization") matches 151 in 64, not 184.
- The same pair of figures for each version:

  | Version | Held by another civilization | Not held by the owner |
  | --- | --- | --- |
  | 0.18.0 | 150 in 67 | 187 in 75 |
  | r16 | 151 in 64 | 184 in 69 |
  | r17 | 132 in 60 | 189 in 72 |

## 5. Reported statistics (1024 defaults unless stated)

**Orcs in `calm` and `turbulent`** (20 worlds each; reported, no target):

| Set | T5 0.18.0 → r16 → **r17** | T9 r16 → **r17** | T10 r16 → **r17** |
| --- | --- | --- | --- |
| calm | 24 / 9 → 8 / 18 → **28 / 6** | 8 → **2** (seeds 7, 14) | 4 → **0** |
| turbulent | 22 / 15 → 8 / 20 → **23 / 22** | 10 → **2** (seeds 11, 12) | 6 → **0** |

- Turbulent T5 is still close: 23 won against 22 lost (r16 8 / 20, 0.18.0 22 / 15).
- Outside the reported sets, T10 is 6 of 20 in `span2000`. The window E − 250 falls in the last 250 years of a 2000-year span, where orcs are often barely awake.
- T10 is 2 of 20 in `civs4` and 1 of 30 at 512.

**T8** (worlds with an orc declaration), a reported statistic:
- default **71 of 100** (r16 78, 0.18.0 75);
- calm 16 of 20, turbulent 16 of 20.

It is below Paul's accepted "about 80%". The fall follows from 22.1: a one-settlement realm no longer declares on its quarrel. The 29 default worlds without one are seeds 3, 4, 10, 12, 22, 26, 35, 48, 51, 54, 56, 57, 60, 61, 63, 65, 66, 72, 76, 77, 79, 81, 82, 83, 85, 87, 90, 92, 93. The first round's per-class split (`t8.py`) was not re-run.

**The orcs at r17 against r16** (`sweep/orcs.txt`, defaults):

| Measure | r16 | r17 |
| --- | --- | --- |
| standing settlements | 590 | 708 |
| founded | 541 | 618 |
| people | 1.31 M | 1.52 M |
| declared | 241 | 220 |
| flights | 26 | 7 |
| awakened realms that fought a first war | 80 | 73 |
| first wars declared with at most one settlement | 57 | 9 |
| first wars lost with at most one settlement | 29 | 3 |
| mean years from founding to the first declaration | 179 | 258 |

**22.1's risk** (`q221/`, the reference harness; it reproduces every stored log except seed 73's).

| Set | Orc declarations | On the open quarrel | Fewer than 2 settlements, on the open quarrel | Fewer than 2 settlements, on another claim | Their outcomes (won / lost / truce / unfound) |
| --- | --- | --- | --- | --- | --- |
| default | 217 | 56 | **0** | 16 | 7 / 4 / 4 / 1 |
| calm | 44 | 14 | 0 | 3 | — |
| turbulent | 63 | 6 | 0 | 8 | — |

- The hold-back fires as specified. In default worlds 79 realms are held back in at least one step (16,836 held-back evaluations).
- For comparison, r16's one-settlement orc declarations were 56 on the quarrel and 31 on other claims, by my approximation (`risk221.py`).
- 22.1's risk, a first war sent elsewhere, is real but small, and it does not miss a target.

**Footholds, held regions, settlements in another's region:**
- **Foothold cessions.** 2 at defaults (seed 40 e198, seed 58 e922; one per world). Across the sets: 5 in civs 4, 4 at 512, 1 at 2048, 1 in dense, 0 elsewhere.
- **(17)'s capital exemption.** It was never used.
- **Wars over a region the attacker held at the declaration**, defaults:

  | Version | Wars | Outcomes |
  | --- | --- | --- |
  | 0.18.0 | 28 | 18 attacker won, 2 defender won, 8 truces |
  | r16 | 32 | all attacker wins, 22 of them with no battle |
  | **r17** | **14** | attacker won 10 (foothold cession 2, region ceded 3, tribute 5), truce 1, `civ_conquered` 1, open at E 2; 46 battles; 1 with no battle (still open at E) |

- **Endings where the attacker held G with the defender still in it** (decided by 21.6 item 3): 2 at defaults, both attacker wins with a foothold cession.
- **Standing settlements in a region another civilization holds, at E:** 132 in 60 worlds (see 4.1).

**`search_abandoned`** per world: median 3, maximum 11 (r16 3 and 8; 0.18.0 4 and 12). 29 worlds have at least 4. At most one per world has salience 40 and is shown at the threshold.

**Races** (sum of the 100 worlds; people ‰ / standing settlements ‰):

| Race | r16 | r17 |
| --- | --- | --- |
| goblin | 446 / 511 | 435 / 495 |
| human | 324 / 139 | 319 / 141 |
| dwarf | 91 / 97 | 97 / 100 |
| orc | 59 / 66 | 69 / 79 |
| elf | 57 / 115 | 57 / 114 |
| faerie | 23 / 72 | 23 / 70 |

- No race goes extinct in any set.
- Civilizations ended per world: 0 / 1 / 4 (r16 0 / 0 / 2). `civ_conquered` median 1 (r16 0).
- Largest race share of people: median 564.5‰ (r16 604.5). At least 600‰ in 43 worlds (r16 51); at least 700‰ in 19 (r16 24).

**Goblins' share of declarations:** 773‰ at defaults (1958 of 2532). Calm 752, turbulent 766, civs 4 791.

**Faerie and dwarven sites:**

| Measure | r16 | r17 |
| --- | --- | --- |
| faerie settlements within 300 m of the sea | 93 of 831 | 105 of 837 (12.5%; target below 20%, met) |
| faerie settlements within 48 m | 21 | 26 |
| on a temperate-forest cell | 681 | 695 |
| in a temperate-forest region | 828 | 833 |
| dwarven settlements below 30 m | 5 (0 fortresses) of 760 | 3 (0 fortresses) of 769 |
| dwarven in the lowest quarter of their region | 48 | 51 |

**Quiet lines and wording:**
- **Quiet lines.** 729 `quiet_stretch` at defaults, 5 / 7 / 11 per world (r16 873, 5 / 9 / 14).
  - 146 quiet rows are shorter than 50 years, all with a realm alive and none with a line.
  - Every set has its shortest stretch at 50 years or more.
- **Outbreaks.** 7,735 outbreaks; none of salience 45 shows a total below 1,000.
- **Sentences.** No malformed or missing separator, no "0 of the victors", no "its seat".
- **Cause clauses.** Dwindles: 1,336 with a clause, 48 without. Abandonments: 52 of 52 with a clause. No cited cause is older than 50 years.

**Names:**
- **Near-duplicates.** 0 equal-core pairs and 0 pairs one letter apart in every set. 2,417 of 11,341 civilization and settlement names are redrawn, with no fallback.
- **Filtered attempts per culture** (attempts before the stored name, defaults): goblin 743, human 105, faerie 54, orcish 42, elven 21, dwarven 20.
- **Triple-letter attempts.** 5, all faerie.
- **Region names** keep `pool` (Reekpool Thicket, Jaypool Shaw, The Pool of Whispers at 2048). They hold `poo` but are not covered by the filter (13.2), which is correct.

**Generation time:**
- **Quiet machine** (`timing/gen-times.txt`, seed 0, three runs each, round 1's binary and data against round 2's): within 1% at 512, 1024 and 2048 (section 6).
- **In the sweep,** at three workers against round 1's two, the per-world time rose 3% (1024), 4% (512) and 6% (2048, span 12000). This is within the difference in load.

### 5.1 Typical ranges for P3-06 15.5 (re-measured, r17; 5th–95th percentile, 1024 defaults, 100 worlds)

| Statistic | 5th–95th | Median |
| --- | --- | --- |
| events | 382 – 1219 | 675.5 |
| wars | 9 – 51 | 23 |
| flare-ups | 1 – 8 | 4 |
| claims | 17 – 85 | 38 |
| battles | 30 – 191 | 76 |
| battles involving faeries | 0 – 11 | 4 |
| sacked | 5 – 26 | 12 |
| taken | 1 – 21 | 6 |
| cessions (all); of 20.3 | 2 – 15; 1 – 11 | 8; 4.5 |
| tributes | 1 – 13 | 5 |
| flights | 0 – 2 | 0 |
| endings: peace / war_won / truce | 2 – 16 / 1 – 17 / 1 – 16 | 7 / 5 / 6 |
| endings: search_abandoned | 1 – 6 | 3 |
| endings: conquest | 0 – 2 | 1 |
| `wars_hollow_permille` | 43 – 375 | 220.5 |
| `wars_same_goal_max` | 2 – 5 | 3 |
| `wars_same_goal_unchanged_max` | 2 – 3 | 3 |
| largest race share of people ‰ | 303 – 790 | 564.5 |
| `race_regions_share_max_permille` | 268 – 751 | 454 |
| largest civilization land share | 117 – 367 | 208 |
| civilizations; ended | 8 – 14; 0 – 2 | 10; 1 |
| simulated years; ‰ | 1200 – 1540; 200 – 256 | 1337.5; 222.5 |
| summary threshold | 26 – 41 | 31 |
| first war after the goblins' awakening (years) | 299 – 2704 | 812.5 |
| standing: faerie / elf / orc / human / dwarf / goblin | 2–13 / 1–27 / 1–17 / 3–28 / 4–18 / 6–96 | 5 / 8 / 5 / 11 / 8 / 40.5 |

These are the r17 figures, to replace 22.8's r16 table. D1's fix changes one world of the 100 (seed 73), so it moves them at most at the edges.

## 6. Invariants, purity, timing

**Invariants.**
- **The reference.** In 374 worlds it checks every invariant, (8) restated, (13), (15) restated, (16) and (17) included, with the earlier ones, and they hold. In seed 73 the reference's own invariant pass stops at the divergence. On the probe's fixed seed 73 they all hold.
- **`check10.py`.** It checks (8) restated, (12), (13), (14), (15) restated, (17), 21.7 items 1 to 3, the quiet minimum and the name rules in all 375 worlds, and they hold.
- **(15).** No colony, claim or declaration of a civilization falls in a year it lies low. No case of "refuge gone before its year" occurs, so the 22.3 and r16 readings agree.

**Purity.**
- `--threads` 1, 2 and 6 are byte-identical at 1024 seed 0, and `--until history` equals the default.
- `--monster-pressure high` and `--divine-activity meddling` change only `meta`.
- `--until chronology` drops the history tables.
- `--history-span 2000` and `--races elf` change `race`, `age` and the history.

**Timing** (`timing/gen-times.txt`, load average about 1):

| Map | r16 (first round) | r17 |
| --- | --- | --- |
| 512 | 2.63–2.66 s | 2.65–2.68 s |
| 1024 | 9.23–9.29 s | 9.26–9.29 s |
| 2048 | 37.27–37.67 s | 37.50–37.59 s |

The two rounds are within 1% at every size, so there is no slowdown worth a defect.

## 7. Recorded hashes

**Why they changed.** Every world changes through `content_hash`:
- race `hidden_years`;
- `history.conflict` without `hidden_years` and with `quarrel_min_settlements`;
- `history.engine.quiet_min_years`;
- the filter lists.

Compared with round 1's detail hashes:
- the empty and pattern worlds, `geo-seed-1-1024` and `hist-races-elf-orc-1024` differ only in `meta`;
- `geo-seed-42-512`, `hist-dense-1024` and `hist-span-2000-512` also change their history tables;
- no layer and no geography table changes.

The inline constants of `db_determinism_test.cpp` and `stages_pipeline_test.cpp` hash worlds made with no data, so they did not change.

`worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase 2170817838fc37d59ed34352776ab8adace40955b311bb427b7163a488291d10
empty-seed-0 836c9bd2c2057b8bbada868f052e2a3bfb4956951c541af949fa1c6c6ba1e6da
empty-seed-0-512 3e9c12f66c31d1916bb22ccb68d8635f12a48a5dcf1b1b530377bf0f160d3011
empty-seed-1 df7f5945c261dd4ca921595aae071cb36c5cfbfd5fad8fad7cf74567c419de81
empty-seed-42 82cb6141ee2b46211f729cd142c845046446a87c632bcb8f1746a9e074369cc2
empty-seed-max 1614716b3c0d348b2f718107f86df68b3082ced78c8323e38a9ccbd320904c49
pattern-seed-0 c9f84ec7bec4db02e016cb7243a535d836e99636159c78f86d668869d345e40b
pattern-seed-42 620eb685d4504a5e07ec7694d7d42f750e8db20243021bf9f926e50ae03f9bce
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 27e62a6f79fe8d255b58f6db0bdcc03ff5263cb5b28098cf4e549e645f8e24dd
geo-seed-0-512 2a5a96e28ff812c7cafb6d6a420eba1c71169f781b97f4f051acfe3f4690d9ab
geo-seed-1-1024 b30f07bc7a20d6ae5608a7d6a054fc61d0e2a7e9f525eafbb2d10a420b967d79
geo-seed-42-512 f8f000241898e8785c3fe3c625281cbe5f946be01b3a3d77b43f16a4b2608ee9
```

`worldgen/tests/golden/p3-worlds.txt`:

```
hist-dense-1024 bff8b9626d953fd54eebfe15f045653bba43437d17098b3c0005e265b7451dd1
hist-races-elf-orc-1024 9179f174401fae0002bd3f9ed6520be79610c0facbfa4942627a78354bc61c86
hist-span-2000-512 19100fc7e2b564ebc30ac93261362454a3d78f6c7719a56cb1fbbf7a653423d8
```

**README phrase world.**
- `generate --seed-phrase "The Unmapped"` hashes to `f6f6fd25d5b7514207b275c312a6b2e52b5a820fa3b67be81cfedca2531f3ebc`.
- Its report is `V/readme-report.txt`: 11 civilizations, 623 events, 79 outbreaks, 7 quiet stretches, 17 wars.
- The README still shows 0.18.0's example. I did not edit it.

## 8. Critic folder

`V/critic/`, with `INDEX.txt` saying what each world is for:
- **The first round's seventeen worlds**, under the same names, so the critic can compare them one by one with `~/.cache/the-unmapped/out/p3-10-validation/critic/`. Orc standing settlements, round 1 → round 2:
  - `orc-seed0-1024` 1 → 9;
  - `orc-seed53-1024` 1 → 13;
  - `orc-calm-seed0-1024` 1 → 9;
  - `orc-turbulent-seed14-1024` 1 → 14;
  - `seed42-2048` 14 → 25.
- **Default seeds 73 and 88.** Seed 73 holds D1 at e914; the index says so.
- **The foothold worlds**, default seeds 40 and 58.
- **The seven one-settlement orc worlds** (`orc1-seed*-1024`).

Every world that also exists in the sweep is byte-identical to it.

## 9. Names to look at (item 7)

These are among the 3,386 names new in round 2 across the sweep. None breaks a rule.
- **`Tardham`, `Tardwuldwick`** (human): "tard" reads as a slur fragment.
- **`Fiesass`, `Wobass Glade`, `Ashiass Glade`, `Liass`, `Yshiass`, `Fouflass`, `Souflass`** (faerie): a word ending in "ass".
- **`Nobbeez`, `Snobgak`, `Skrunob`, `Dreezzsnob Pit`, `Kraksnot Pit`, `Skrosnot Pit`** (goblin): the goblin `nob` and `snot` syllables, as in earlier sweeps.
- **`Bouwee`, `Feetwee`, `Mashwee`, `Weebiatwo`, `Twywhowee`** (faerie): `wee`, as before.

The first two groups are new kinds of match, and the spec writer may want `tard` (at the start of a word) and a word-final `ass` in the filter.

## 10. Findings for the lead

1. **D1** (section 3): blocking. It goes to the generator engineer, with the probe as a pointer. The test writer adds 21.6's order test. Then re-run the reference over the sweep and `world-hashes.sh local`.
2. **T8** is 71 of 100 (r16 78), below Paul's accepted "about 80%". It is a reported statistic, and the fall is the intended effect of 22.1. It may be worth one line to Paul with R1's confirmation.
3. **22.4 and 22.5 figures** (4.1):
   - the 0.18.0 quick-aim-win baseline is 0 by 22.5's definition (2 by the looser count);
   - "184 in 69" counts settlements not held by their owner, while "held by another civilization" is 151 in 64.

   These are text corrections for the spec writer.
4. **Typical ranges** (5.1): to replace 22.8's table.
5. **Turbulent T5** is 23 won against 22 lost: reported, no target, but close.
6. **Names** (section 9): for the spec writer.

## 11. Files

Under `~/.cache/the-unmapped/out/p3-10-validation2/`:

| Path | Contents |
| --- | --- |
| `logs/summary.txt` | every step's exit status and time |
| `logs/ctest1-*.log`, `logs/ctest2-*.log`, `logs/slow-*.log`, `logs/world-hashes-local.log`, `logs/expect-*.log`, `logs/gdext-*.log`, `logs/lint*.log` | tests, hashes, extension, lint |
| `sweep/<set>/` | per world: `.db`, `.txt`, `.report.txt`, `.sent.txt`, `.regnames.txt`, `.check.json`, `.ref.txt` (`.pref.txt` for `elf`) |
| `sweep/stats10.txt`, `sweep/agg2.txt`, `sweep/orcs.txt`, `rounds-r17.txt`, `rounds-old.txt` | statistics |
| `sweep/ref-results.txt`, `sweep/pref-results.txt` | reference results |
| `q221/` | the 22.1 harness |
| `probe/` | D1's probe diff and the 375-world comparison |
| `critic/` | the critic folder and `INDEX.txt` |
| `purity/`, `timing/`, `readme/`, `readme-report.txt` | purity, timing, the README phrase world |

The scripts are in the same folder: `check10.py`, `stats10.py`, `agg2.py`, `rounds.py`, `orcs.py`, `risk221.py`, `q221.py`, `variants.py`, `sweep-job.sh`, `run-sweep.sh`, `recheck.sh`, `purity.sh`, `timing.sh`, `critic/make.sh`, `probe/probe-job.sh`.

## Addendum: after the D1 fix

**Input.** `origin/p3/10-int3` at `d4d44c0` was merged into `p3/10-validation2`; the merge was a fast-forward. It adds three things to this branch:
- the engineer's fix: `end_decision` in `conflict_war.cpp`, which takes 21.6's items in their order and writes `peace` when the loser is exhausted;
- the test writer's order tests (`history_conflict_order_test.cpp`);
- the golden case `hist-conflict-73-1024` (a slow C++ comparison).

The reference only gains that case; its rules are unchanged. Evidence is in `V/logs3/`, `V/sweep3/`, `V/merged/` and `V/purity3/`.

### Final verdict: pass

D1 is fixed. Every gate passes, every merge target holds with the corrected figures, and the recorded hashes are unchanged.

### Gates

| Gate | Result |
| --- | --- |
| Build, `linux-gcc-release` and `linux-clang-release` | warning-free |
| `ctest` | 34 of 34 on both, with the recorded hash files unchanged |
| `world-hashes.sh local`, the two presets | 15 of 15 identical |
| `expect` | 15 of 15 on both |
| The 375 sweep worlds regenerated with the fixed binary (`V/sweep3/`, file hashes in `sweep3/compare.txt`) | **374 byte-identical. Exactly one changed: 1024 default seed 73**, as the probe predicted. The fixed seed 73 is byte-identical to the probe's |
| Reference `check-world` and `check10.py` on the changed world | seed 73: reference **7359 checks, 0 failures**; `check10.py` passes |
| Reference and `check10.py` on 30 unchanged worlds (two per set, fixed random sample, `sweep3/check-jobs.txt`) | all pass, with output identical to the first sweep's (apart from the file path) |
| Reference over the sweep, in effect | **375 of 375** |
| Purity, seed 73 | `--threads` 1, 2 and 6 byte-identical (`36730324…`) |

The asan and debug presets, the slow entries and the extension were not repeated here; the lead's CI run covers them.

### Seed 73 after the fix

War e875 now ends at e914 (5063) with "The House of Fiegla sued the Snonugg Mob for peace after 29 years of war.", followed by a tribute (e915, 282 carried off). That is the reference's aim ending. The rest of the world's history follows from it:

| Seed 73 | Before the fix | After |
| --- | --- | --- |
| events | 1529 | 1418 |
| wars | 72 | 63 |
| battles | 276 | 242 |
| hollow wars | 14 | 11 |
| civilizations ended | 3 | 2 |

### Corrected figures (1024 defaults; only seed 73 moves)

These are measured with the merged view `V/merged/`: the first sweep with seed 73 replaced. Its outputs are `merged/stats10.txt` and `merged/agg2.txt`, plus `rounds-r17fix.txt`.

**Unchanged.** Every merge target in section 4 keeps its value:
- T2, T3, T4, T6, T7, T5 (98 / 50), T9 (7), T10 (0);
- quick aim wins (0), quiet stretches under 50 years (0), the name rules (0);
- foothold cessions (2), wars over a held region (14, the same outcomes);
- T8 (71 of 100), `search_abandoned` (median 3, maximum 11), quiet lines (729).

**Changed:**
- **T1:** 227 (509 / 2237), formerly 227 (512 / 2246). Still met.
- **Standing settlements in a region another civilization holds:** 135 in 61 worlds, formerly 132 in 60.
- **Goblins' share of declarations:** 774‰, formerly 773‰.
- **Maxima per world:** events 1450 (formerly 1529, which was seed 73), wars 63 (72), battles 242 (276), sacked 32 (44), claims 106 (116).
- **Typical ranges (5.1):** three bounds move:
  - truces 1 – 15 (formerly 1 – 16);
  - largest race share of people 303 – 802 (303 – 790);
  - faerie standing 3 – 13 (2 – 13).

  Every other row and every median is unchanged.
- **Sites:** faerie settlements within 300 m of the sea 104 of 836 (12.4%); dwarven settlements below 30 m 3 of 766.
- **Race sums:** standing settlements elf 1019, goblin 4434, dwarf 890, faerie 624, human 1257, orc 706. The shares of people and settlements change by less than 1‰.

### Critic folder and README world

- **Critic folder.** The seed-73 files were regenerated with the fixed binary (`critic/seed73-1024.*`), and `INDEX.txt` is updated. Every other critic world is byte-identical to its sweep world, so none of them changes.
- **README phrase world.** It is regenerated and **unchanged**: `f6f6fd25d5b7514207b275c312a6b2e52b5a820fa3b67be81cfedca2531f3ebc`, and `readme-report.txt` is byte-identical to the one before the fix.

### Recorded hashes

`p0-worlds.txt`, `p1-worlds.txt` and `p3-worlds.txt` are unchanged by the fix. They are confirmed by `ctest` and `expect` on both release presets and by the two-preset `local` comparison.

### Open for the lead

Findings 2 to 6 of section 10 stand:
- T8 at 71%;
- the 22.4 and 22.5 figures;
- the typical ranges, now with the three corrected bounds above;
- turbulent T5;
- the names.
