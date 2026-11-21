# Validation: P3-10 wording, site and conflict follow-ups (generator 0.19.0, schema 7)

Validation specialist, 2026-11-21.

**Input.** Branch `p3/10-validation`, cut from `origin/p3/10-int` at `14fc50d`: the engineer's code and data, the independent test writer's tests, references and goldens, the build engineer's slow-suite CI change, and the spec text r16. Nothing else was merged. The determinism audit ran in parallel on `p3/10-audit`; I did not wait for it.

**Specs.** Change spec `docs/specs/p3-10-followups.md` (r2, sections 9 and 10b–10d). Stage specs P3-04 r14 section 20, P3-05 r14 section 22, P3-06 r16 section 21, P3-07 r13 section 22, P3-02 r12 section 12, P1-06 r9 section 15.

**Baseline.** The P3-09 validation's 0.18.0 binary, sweep and world-hash files: `~/.cache/the-unmapped/out/p3-09-validation/` and `~/.cache/the-unmapped/out/p3-09-val/world-hashes/`.

## Verdict

**Pass.** I found no defect. Every gate passes, and every invariant and target T1–T7 holds where it is defined (1024 defaults). The reference passes 375 of 375 sweep worlds, and my own independent check also passes 375 of 375.

There is one finding for the lead to rule on: **the orcs are weaker at 0.19.0** (section 6). At 1024 defaults they end with a single standing settlement in 21 of 100 worlds; on the same seeds at 0.18.0 that was 7. No race went extinct anywhere. T5 holds at 1024 defaults (95 won, 77 lost). It fails in the `calm`, `turbulent` and `span2000` sets, but T5 is defined only at defaults. Under Paul's ruling, T8 (78 of 100) is a reported statistic.

| Gate | Result |
| --- | --- |
| Build, five Linux presets | warning-free |
| `ctest` round 1 (before recording) | only `worldgen-tests-other` and `worldgen-cli-tests-other` fail on each preset; the failing cases and the `values:` lines are identical on all five, and every failure is a world-hash constant |
| `ctest` round 2 (after recording) | 33 of 33 on all five presets |
| Lint | `--self-test` and the scan are both clean |
| Slow entries | `ctest --preset linux-gcc-release-slow`: 12 of 12 (1324 s, two jobs); `linux-clang-release -L slow`: 12 of 12 (1427 s); `ctest --preset linux-gcc-release -N` lists no slow entry |
| Extension (`linux-gcc-release`, GDEXT on) | 34 of 34, `gdext-headless-check` included (7.4 s); `expect` 15 of 15; byte-identical to the plain build on all 15 cases |
| `world-hashes.sh local`, five presets | 15 cases identical |
| `expect` | 15 of 15 on each of the five presets |
| Reference `p3_conflict_reference.py --version 0.19.0 check-world` | **375 of 375** pass |
| Peoples reference `check-world` | 20 of 20 (the `elf` set) |
| My independent check (`check10.py`) | **375 of 375** pass |
| Purity | threads 1, 2 and 6 byte-identical; `--until history` equals the default; the 512 pair equals the golden |
| 0.19.0 reads a 0.18.0 world | `report` and `hash --detail` byte-identical; all renders exit 0 |

## 1. What was run

Evidence is under `~/.cache/the-unmapped/out/p3-10-validation/`. The scripts are adapted copies of the P3-09 ones; the P3-09 folder was not modified.

- **Build and tests.** `build-all.sh` builds; `ctest_run.sh` runs the ctest rounds (round 2 with two jobs beside the sweep); `phaseA.sh` runs world-hashes `local`. Logs are in `logs/`.
- **Sweep.** `run-sweep.sh` and `sweep-job.sh` run the same 375 jobs as P3-09 (`jobs.txt`, two halves at two workers). Each world gets: generate, `hash --detail`, `report`, the C++ sentence dump (`p3-08-dump`), the region-name reference, `check10.py`, and the reference's `check-world --version 0.19.0` (plus the peoples reference for `elf`).
- **Recheck.** `recheck.sh` re-ran `check10.py` over all 375 worlds after one fix to my own check, described under F7.
- **Comparison run.** `old10.sh` runs `check10.py` on the 375 0.18.0 worlds to fill the comparison columns.
- **Comparisons and statistics.** `compare18.py` diffs each 0.18.0/0.19.0 world pair. `stats10.py` writes `sweep/stats10.txt`, `regstats.py` gives the region-name measures, and `orcs.py` writes `sweep/orcs.txt`.
- **T8 harness.** `t8.py` is a statistics harness on the reference: it records every odds evaluation, the quarrel log and the contacts, and checks that the run reproduces the stored log.
- **Other.** `purity.sh` and `timing.sh` produce `purity/` and `timing/`, and `critic/make.sh` builds the critic folder.

**`check10.py`** is my independent check from the rows. It extends `check09.py` with the 0.19.0 readings:

- **Catalogue.** It requires 38 kinds, and that kinds 4 and 12 are never written.
- **Kind 37.**
  - `total` = `dead` + the dead of the caused spreads (invariant 13);
  - `reached` = the number of spreads;
  - salience follows `total`;
  - no salience-45 outbreak has a `total` below 1,000;
  - the sentence carries the separated total, and "in all" appears exactly when `reached` > 0.
- **Kind 38.**
  - exactly one `quiet_stretch` per quiet phase row that has a living civilization;
  - its span equals the row and `years` = span;
  - no region, settlement or cause;
  - `realms` = the civilizations alive;
  - participants = all of them in ascending id, or 64 when there are more.
- **Causes.**
  - A dwindling cites a reduction or a colony it sent (`origin`).
  - No cited reduction or decline start is older than `cause_memory_years`; a founding event is exempt.
  - The "Weakened by …" clause is rebuilt from the cause kinds with 22.3's map and compared with the C++ sentence.
- **Sentences.**
  - every number of four or more digits carries a well-formed separator, except rendered years;
  - no "0 of the victors" and no "0 of the defeated";
  - `winner_dead` 0 renders "and none of the victors" and 1 renders "and one of the victors fell";
  - no "its seat" and no "moved its".
- **Names (P3-02 12.2).**
  - It replays the attempts in founding order: every free attempt before the stored one must be in the registry or near K.
  - No two civilization or settlement cores are equal or one letter apart.
- **Regions (P1-06 15.7).**
  - one keeper per land noun (most cells, ties to the smaller id), named its base name;
  - no other region is plain;
  - every stored name equals the reference's.
- **Conflict.**
  - Weariness and the run statistic by holder: a change is a `region_control` row of another civilization.
  - (8) as amended, using the region's owner at the ending event.
  - (14).
  - 21.7 items 1–3: no claim or declaration on (y, R) within `search_weary_years` unless R's holder changed; no `search_abandoned` after a battle of its war in R; salience 40 for the first and 15 after.
  - (15) for lying low, read the r15 way and also checked strictly.
- **Sites.** Faerie and dwarven site statistics: Chebyshev distance to the sea, elevation below 30 m, cell and region biome.

Invariant (16) and the odds of 21.9 need the run's contact and forces, so I leave them to the reference. It checks them in every world, and they pass.

## 2. Tests

### 2.1 Failing set before recording

- **Release presets.** On `linux-gcc-release` and `linux-clang-release` the whole suite ran. Two entries failed, `worldgen-tests-other` and `worldgen-cli-tests-other`.
- **Failing cases.** The failing test cases are 2 in the library and 5 in the CLI:
  - `db_determinism_test.cpp`: the 5 empty-world constants;
  - `stages_pipeline_test.cpp`: 2 constants;
  - `cli_golden_test.cpp` and `cli_p1_generate_test.cpp`: the `p0-worlds.txt` values.
- **Debug and asan presets.** Their binaries held the old constants, so I ran their two `-other` entries with the old `p0-worlds.txt` restored temporarily (the rest of their suites is covered by round 2). They fail the same 7 cases.
- **Comparison.** The sorted `values:` lines are identical on all five presets, and so is the list of failing test cases (MD5 `74f82bf2…`).

### 2.2 Round 2 and times

All five presets pass 33 of 33. The release and debug rounds ran with two ctest jobs beside the two sweep workers (load average 3 to 6), so the times are high.

| Preset | Round 2 | Longest entries |
| --- | --- | --- |
| `linux-gcc-release` | 227 s | `-engine` 34 s, `-chronology-property` 29, `-chronology` 24 |
| `linux-clang-release` | 232 s | `-engine` 35 s, `-chronology-property` 32, `-chronology` 27 |
| `linux-gcc-debug` | 984 s | `-hydrology` 132 s, `-chronology-property` 127 |
| `linux-clang-debug` | 1031 s | `-chronology-property` 135 s, `-hydrology` 129 |
| `linux-clang-asan` | 4269 s | `-chronology-property` 566 s, `-hydrology` 544, `-chronology` 531, `-balance` 488 |

**Over the marks.** On release, `worldgen-tests-engine` takes 34–41 s, and alone on a quiet machine still 31 s. It is the one entry over 30 s that P3-10 made slower. Four new test cases add about 13 s:
- r12 20.1 quiet stretch on `geo-seed-42-512`, 5.8 s;
- separators, 3.5 s;
- `{@causes:…}`, 2.2 s;
- the outbreak sentence, 1.9 s.

`-chronology-property` is at 29–32 s, as at 0.18.0. No asan entry reaches 1200 s.

**Slow entries.**
- `linux-gcc-release-slow`: 12 of 12 in 1324 s with two jobs. The largest are `slow-engine-resume` (407 s), `slow-elevation` (397 s) and `slow-climate` (312 s).
- `linux-clang-release -L slow`: 12 of 12 in 1427 s.
- The suites hold 60 library and 8 CLI slow cases.
- The two `slow-other` catch-alls hold no case, because every slow file is named by a SLOW line. The partition check accepts that.

## 3. What changed against 0.18.0

**World-hash cases** (detail hashes, `linux-gcc-release`):
- The empty and pattern worlds differ only in `meta`. The empty world is still 139,264 bytes.
- The geography and history cases keep every layer and every geography table except `region`.
- `region` differs only in `name`: every column but `name` is identical.
- `race`, `race_biome`, `culture` and `age` are unchanged.
- `god` changes in `hist-dense-1024` (Flopfia → Patie) and `hist-span-2000-512` (Gordman → Balbron).

**Sweep, 375 world pairs** (`compare18.py`, `sweep/compare18.jsonl`):

- **Unchanged.** In all 375:
  - every layer;
  - the geography tables `biome`, `climate`, `pass`, `region_border` and `region_biome`;
  - `region` without its `name`;
  - `race`, `race_biome`, `culture` and `age`.
- **Region names.** 11,661 of 18,507 change.
- **Gods.** These change in 42 worlds. That is seeds 5, 7 and 11 of every set that has them, and they are exactly the critic's three:
  - Grammam → Buukbaakh (god 6);
  - Gordman → Balbron (god 5);
  - Flopfia → Patie (god 4).
- **Civilization and settlement names.** 772 of 3,810 civilizations and 13,334 of 34,116 settlements change. These counts include settlements that exist in only one version, since the histories diverge.
- **Catalogue.**
  - 38 kinds against 36.
  - Changed rows: templates of 10, 15, 16, 17, 22, 23 and 31; the new 37 and 38.
  - Kinds 4 and 12 are written in none of the 375 worlds.
- **First differing event.** In 333 worlds it is the first quiet line (`quiet_stretch` for `quiet_era`). In the other 42 it is the first plague (`plague_outbreak` for `plague`). In `hist-dense-1024` it is e4.

**Names: the core rule** (1024 defaults, 100 worlds):
- Equal-core pairs: 53 → **0**.
- Pairs one letter apart: 646 → **0**.
- Of 11,170 civilization and settlement names, 2,346 are redrawn, with 3,248 extra attempts (660 skipped as near). There are no fallbacks.
- Every set has 0 such pairs.

**New names worth a look** (a scan of the 4,201 names of the 1024 defaults that are new at 0.19.0):
- `Beamcumstead` (human; "cum");
- `Twypoo` (faerie);
- `Teefass`;
- `Wasssie` (a triple s).

The rest of the scan's matches are the faerie syllables `wee` and `who` and the goblin `gnob` and `snot`, as in earlier sweeps.

**Region names** (P1-06 15.5 and 15.7 measures):

| Size | Regions | Plain names, 0.18.0 → 0.19.0 | Drawn names | Grim drawn names | Church words | Repeats | Attempts (mean, max) | Fallbacks |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 512, 30 worlds | 393 | 330 (839‰) → **132 (335‰)** | 63 → 261 | 14% → 11% | 0 → 0 | 0 → 0 | 0.07, 1 | 0 |
| 1024, 100 worlds | 4,951 | 2,088 (421‰) → **623 (125‰)** | 2,863 → 4,328 | 14% → 9% | 72 → 3 | 57 → 60 | 0.14, 6 | 0 |
| 2048, 5 worlds | 995 | 158 → **44 (44‰)** | 837 → 951 | 14% → 7% | 23 → 0 | 96 → 41 | 0.75, 11 | 0 |

- The three church matches left at 1024 are false matches of "rood" in "Brooding".
- One keeper per land noun holds in every world.
- Every stored region name equals the reference's.
- The 512, 1024 and 2048 shares are exactly 15.7's predictions: 33%, 12% and 4%.

**Reading a 0.18.0 world.** The 0.19.0 binary reads the 0.18.0 golden `hist-dense-1024` (`c709e94f…`):
- `report` and `hash --detail` are byte-identical to 0.18.0's own output;
- `render` of `map`, `regions`, `biomes` and the elevation layer each exits 0.

## 4. Targets T1–T7 (all sets; T1, T5 defined at 1024 defaults)

| Set | Worlds | T1 hollow ‰ (hollow / found) | Unfound | T2 worlds > 3 / inv (12) | T3 `refuge_held` r1 (with free regions) | T4 | T5 orc won / lost (0.18.0) | T6 (by aim) | T7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **default 1024** | 100 | **234** (517 / 2203) **met** | 302 | 0 / 0 | 8 (0) | 0 | **95 / 77 met** (114 / 68) | 0 (1) | 0 |
| 512 | 30 | 274 | 43 | 0 / 0 | 36 (0) | 0 | 35 / 15 (32 / 17) | 0 (2) | 0 |
| 2048 | 5 | 229 | 21 | 0 / 0 | 0 | 0 | 12 / 4 (13 / 5) | 0 | 0 |
| span 2000 | 20 | 336 | 12 | 0 / 0 | 0 | 0 | 1 / 9 (3 / 5) | 0 | 0 |
| span 12000 | 20 | 175 | 128 | 0 / 0 | 0 | 0 | 85 / 44 (85 / 35) | 0 (2) | 0 |
| sparse | 20 | 186 | 58 | 0 / 0 | 0 | 0 | 27 / 13 (27 / 17) | 0 | 0 |
| dense | 20 | 207 | 64 | 0 / 0 | 0 | 0 | 20 / 18 (16 / 15) | 0 (2) | 0 |
| calm | 20 | 207 | 39 | 0 / 0 | 0 | 0 | **8 / 18** (24 / 9) | 0 | 0 |
| turbulent | 20 | 225 | 68 | 0 / 0 | 0 | 0 | **8 / 20** (22 / 15) | 0 | 0 |
| civs 1 | 20 | 226 | 32 | 0 / 0 | 0 | 0 | 28 / 17 (21 / 17) | 0 (1) | 0 |
| civs 4 | 20 | 212 | 117 | 0 / 0 | 5 (0) | 0 | 31 / 24 (16 / 22) | 0 (2) | 0 |
| `elf,orc` | 20 | 290 | 0 | 0 / 0 | 0 | 0 | 19 / 16 (24 / 9) | 0 | 0 |
| `elf,goblin` | 20 | 244 | 0 | 0 / 0 | 0 | 0 | — | 0 | 0 |
| `elf` | 20 | no wars | 0 | — | — | 0 | — | — | — |
| no elves | 20 | 180 | 76 | 0 / 0 | 0 | 0 | 21 / 16 (24 / 14) | 0 (1) | 0 |

- **Agreement with the reference.** My statistics agree with the reference's `[stats]` in every world of every set: hollow, unfound, same-goal, unchanged runs, held refuges and the race share of people.
- **T1** is above 250 only outside the defaults: at 512, in span 2000 and in `elf,orc`. P3-06 20.29 records this as known.
- **T2** holds everywhere: `wars_same_goal_unchanged_max` is at most 3 in every world, with weariness counted by holder.
- **T5** is lost in calm, turbulent and span 2000 (section 6).

## 5. P3-10 targets and statistics (section 9 of the change spec)

### 5.1 Invariants and rule checks (every world of every set)

All of the following hold in all 375 worlds, by the reference and by `check10.py`:
- (8) as amended and (14), checked against the region's owner at the ending;
- (15), lying low: no colony, claim or declaration by a civilization during its refuge's hidden years. Under the r15 reading no refuge was gone before such an event, so the strict and the r15 readings agree.
- (16), checked by the reference;
- (12) with the holder definition;
- 21.7 items 1 to 3:
  - no claim or declaration on a search-weary region;
  - no search after a battle of its war in that region;
  - first search salience 40, later ones 15;
- kind 37's totals and saliences, and kind 38's one stretch per quiet row;
- no cited cause older than 50 years;
- names and keepers as in section 3.

Counts:
- **Claims at the weariness boundary.** There are 25 claims or declarations on (y, R) exactly at year Y + 500 at 1024 defaults. I read "until Y + 500" as exclusive.
- **Clearing.** 2 claims followed a change of holder that cleared the weariness.
- **Participants.** No quiet stretch has more than 64 participants. The largest `realms` in the sweep is 27 (civs 4), so the 64 cap is never reached.

### 5.2 Searches (21.7)

| Set | `search_abandoned` per world (min / median / max) | 0.18.0 | Worlds with ≥ 4 | Salience 40 per world (max) | Shown at the threshold (max per world) |
| --- | --- | --- | --- | --- | --- |
| **default 1024** | 0 / **3** / 8 | 0 / 4 / 12 | 32 of 100 | 1 | 1 |
| 512 | 0 / 2 / 3 | 0 / 3 / 6 | 0 | 1 | 1 |
| 2048 | 2 / 5 / 7 | 2 / 4 / 13 | 3 of 5 | 1 | 1 |
| civs 4 | 1 / 5.5 / 13 | 3 / 9 / 18 | 13 | 1 | 0 |
| span 12000 | 1 / 5.5 / 17 | 0 / 6 / 18 | 16 | 1 | 1 |

- **Target met:** the median at 1024 defaults is **3**, against the target of at most 3.
- At most one `search_abandoned` per world has salience 40, so at most one reaches the default summary.
- Total `search_abandoned` at 1024 defaults: 302 (0.18.0: 437 in the P3-09 run, 443 after its fix).

### 5.3 Aim endings (21.6), 1024 defaults

The ending reasons are from the reference's war lines. A cost is a cession or a tribute.

| Reason | Attacker wins | Defender wins | Truce |
| --- | --- | --- | --- |
| aim (attacker holds the goal) | 133 (75 ceded, 58 tribute) | — | — |
| goal lost to no one or a third party | 376 (163 ceded, 162 tribute, 51 nothing) | 10 | 38 |
| exhaustion | 582 | 256 | 520 |
| quiet step | 22 | 41 | 60 |
| length | 11 | 13 | 14 |
| no front | — | 23 | 8 |
| unfound / conquest / open | 302 / 63 / 33 | | |

- **Goal holder at the ending** (my classification):
  - attacker: 133, every one a cost-paying attacker win;
  - no one: 385;
  - a third civilization: 40.
- **Goal-lost endings.** Truces after a goal lost to no one or to a third party: 29 and 10. Every such attacker win that paid a cost had the attacker winning the last battle, as (14) requires.
- **Seed 42's Wood of Stags.**
  - It now has **3** wars, all by the Grork Horde (2524–2538, 3266–3291, 3823–3843), against 11 at 0.18.0.
  - Its `region_control` has one row, c1 from 1475.
  - `wars_same_goal_max` at 1024 defaults is 1 / 3 / 7 (0.18.0: 1 / 3 / 11).

### 5.4 T8 (reported statistic) and its split

Worlds with orcs that have at least one orc declaration:

| Set | Worlds with an orc declaration | 0.18.0 |
| --- | --- | --- |
| **default 1024** | **78 of 100 (78%)** | 75 of 100 |
| 512 | 23 of 30 | 22 |
| 2048 | 4 of 5 | 5 |
| span 2000 | 12 of 20 | 11 |
| span 12000 | 18 of 20 | 19 |
| sparse | 17 of 20 | 18 |
| dense | 16 of 20 | 13 |
| calm | 17 of 20 | 15 |
| turbulent | 14 of 20 | 15 |
| civs 1 | 19 of 20 | 16 |
| civs 4 | 16 of 20 | 19 |
| `elf,orc` | 17 of 20 | 16 |
| no elves | 16 of 20 | 16 |

**The 22 default worlds without one.** `t8.py` reproduces each stored log and records every odds evaluation for the orc claimants. Its classes:

- **9 worlds: the quarrel neighbour never comes into contact.** The realm has no claim and no odds evaluation at all, and its neighbours in reach are goblins (its own camp). These are the "only goblins within reach" worlds: seeds 25, 26, 27, 60, 66, 79, 82, 83 and 87.
- **4 worlds: in contact with the quarrel neighbour, but the claim pass refuses every claim** ("no allowed goal": every front region weary or excluded by 20.2). This is the exception that (16) allows. Seeds 35, 54, 57 and 92; seed 35 has a second, far orc realm too.
- **6 worlds: claims held, odds never reach 500.** The best odds are 143 to 430 (seeds 12, 22, 51, 65, 76, 81). All of them are against dwarves or humans.
- **3 worlds: a claim reaches its minimum, but no declaration followed.** Seeds 3, 90 (against dwarves) and 48 (against goblins). This is item 3a's case: the yearly chance, or 7.2's largest-strain choice, sent no declaration.

Of 105 awakened orc civilizations at 1024 defaults, 89 claim; the median first claim falls in the awakening year. Their first claims are against humans 44, dwarves 21, elves 12, faeries 11 and goblins 1.

### 5.5 Faeries, dwarves, humans (22.5 and the watch item)

At 1024 defaults (100 worlds, all settlements ever founded):

| Measure | 0.18.0 | 0.19.0 |
| --- | --- | --- |
| faerie settlements within 300 m of the sea | 479 of 889 (54%) | **93 of 831 (11%)**: target below 20%, met |
| within 48 m | 411 | 21 |
| faerie settlements on a temperate-forest cell | 820 (92%) | 681 (82%) |
| faerie settlements in a temperate-forest region | 889 (100%) | 828 (99.6%) |
| faerie standing at the present (sum; median per world) | 763; 7 | 638; 5.5 |
| faerie people (sum) | 578,764 | 509,385 |
| worlds with one faerie settlement | 0 | 1 (seed 98) |
| dwarven settlements below 30 m (fortresses) | 40 (12) of 714 | **5 (0) of 760** |
| dwarven in the lowest quarter of their region | 177 | 48 |
| dwarven median elevation rank (median over worlds) | 0.80 | 0.89 |
| dwarven standing (sum); people | 766; 1,856,595 | 864; 2,005,223 |
| dwarven cities standing | 17 | **3** |
| **humans**: standing (sum); people | 1,254; 7,126,659 | **1,237 (−1%); 7,172,782 (+1%)** |

**Faeries.** The target is met at 1024, 2048 (0 of 133) and in every 1024 set (5–14%). At 512 it is 58 of 159 (36%; 0.18.0: 60%): small maps have narrow coastal woods with no inland site.

**Dwarves.** The five dwarven settlements below 30 m at 1024 defaults are all colonies on lowland ground, in regions with little or no high ground: seed 23 s101, seed 50 s49, seed 61 s50, seed 76 s106 and seed 98 s127, with elevations 0.9–22.7 m. Every 1024 set has 0 to 2 below 30 m, except civs 4, which has 12 of 341. Two of those are fortresses: civs 4 seed 11 s146 (1.7 m) and seed 4 s170 (2.0 m), both colonies in mostly unwalkable coastal regions. The rule halves lowland habitability but cannot lift a colony out of a region that has no high ground, so this follows 22.5 (F2).

**Humans.** The watch item holds: −1% standing settlements and +1% people at 1024 defaults. In the other sets the change ranges from −14% (span 2000) to +34% (span 12000).

### 5.6 Wording, quiet lines, outbreaks, causes (1024 defaults)

- **Quiet lines.** 4,488 `quiet_era` lines (18 / 42 / 88 per world) become **873 `quiet_stretch` lines (5 / 9 / 14)**.
  - At 2048: 601 → 51.
  - In civs 4: one per quiet row, at most 36 realms.
- **Outbreaks.** 7,711 outbreaks, 4,145 of which spread. 1,966 have salience 45, and **0** of those show a total below 1,000.
- **Sentences.**
  - 60,212 numbers are rendered and 11,849 carry separators. 2,035 plain four-digit numbers are rendered years.
  - No malformed separator occurs, and none is missing.
  - No "0 of the victors", "its seat" or "moved its".
- **Cause clauses.**
  - Dwindles: 1,307 with a clause, 45 without (no mapped cause).
  - Abandonments: 51 of 51 have a clause.
- **Cited causes.** 1,307 for dwindles (oldest 50 years) and 83 for abandonments (oldest 47 years). None is older than 50.

### 5.7 Other statistics against 0.18.0 (1024 defaults, min / median / max)

| Statistic | 0.19.0 | 0.18.0 |
| --- | --- | --- |
| events | 307 / 676.5 / 1479 | 251 / 726 / 1348 |
| wars | 4 / 22.5 / 69 | 4 / 23 / 53 |
| endings: peace / war_won / truce | 8 / 6 / 6 (medians) | 7 / 5 / 5 |
| battles | 9 / 79 / 260 | 7 / 77 / 202 |
| battles involving faeries | 0 / 4 / 20 | 0 / 2 / 8 |
| flights (worlds with one) | 69 (48 of 100) | 45 (36) |
| `refuge_held` reason 1 | 8 (2 worlds) | 1 |
| largest race share of people | 272 / 604.5 / 891; ≥ 600: 51, ≥ 700: 24 | 290 / 596.5 / 875; ≥ 600: 50, ≥ 700: 27 |
| wars by attacker: elf / goblin / dwarf / faerie / human / orc | 29 / 1961 / 122 / 3 / 149 / 241 | 38 / 1929 / 105 / 4 / 144 / 306 |
| worlds without a goblin–elf war | 8 | 11 |
| civilizations; ended | 7 / 10 / 15; 0 / 0 / 2 | 7 / 10 / 16; 0 / 1 / 4 |
| summary threshold; shown | 20 / 31 / 51; 139 / 207 / 247 | 20 / 31 / 36; 119 / 206 / 250 |

Standing settlements per race at the present. The last column counts the worlds that end with exactly one; 0.18.0 is in brackets.

| Race | 0.19.0 | 0.18.0 | Worlds with 1 |
| --- | --- | --- | --- |
| faerie | 1 / 5.5 / 19 | 2 / 7 / 20 | 1 (0) |
| elf | 1 / 8 / 38 | 1 / 8 / 42 | 5: seeds 23, 32, 47, 59, 89 (2) |
| orc | 1 / 5 / 24 | 1 / 6 / 18 | **21 (7)** |
| human | 1 / 10 / 50 | 1 / 9 / 49 | 5: seeds 19, 33, 34, 57, 92 (6) |
| dwarf | 2 / 8 / 31 | 3 / 5.5 / 29 | 0 (0) |
| goblin | 1 / 42 / 114 | 1 / 37 / 114 | 1: seed 81 (1) |

- **2048 tone** (5 worlds, 0.18.0 in brackets):
  - civilizations 23 / 28 / 30 (24 / 27 / 31);
  - summary threshold 31 / 46 / 51 (36 / 46 / 56);
  - events 975 / 2,022 / 2,343 (1,172 / 1,861 / 2,349);
  - wars 24 / 52 / 82 (29 / 42 / 73);
  - no race holds 600‰ of the people.
- **Per-set details** are in `sweep/stats10.txt`: every statistic for every set, with 0.18.0 alongside.

### 5.8 Typical ranges for P3-06 15.5 (5th–95th percentile, 1024 defaults, 100 worlds)

| Statistic | 5th–95th | Median |
| --- | --- | --- |
| events | 389 – 1241 | 676.5 |
| wars | 9 – 47 | 22.5 |
| flare-ups | 1 – 7 | 4 |
| claims | 17 – 82 | 37.5 |
| battles | 30 – 193 | 79 |
| battles involving faeries | 0 – 14 | 4 |
| sacked | 5 – 22 | 10 |
| taken | 1 – 20 | 6 |
| cessions (all); of 20.3 | 2 – 14; 1 – 10 | 7; 4 |
| tributes | 1 – 13 | 5 |
| flights | 0 – 3 | 0 |
| endings: peace / war_won / truce | 2 – 16 / 1 – 13 / 1 – 14 | 8 / 6 / 6 |
| endings: search_abandoned | 0 – 7 | 3 |
| endings: conquest | 0 – 2 | 0 |
| `wars_hollow_permille` | 58 – 401 | 230 |
| `wars_same_goal_max` | 2 – 5 | 3 |
| `wars_same_goal_unchanged_max` | 2 – 3 | 3 |
| `race_population_share_max_permille` | 323 – 806 | 604.5 |
| `race_regions_share_max_permille` | 296 – 774 | 455.5 |
| largest civilization land share | 125 – 361 | 205.5 |
| civilizations; ended | 8 – 14; 0 – 2 | 10; 0 |
| simulated years; ‰ | 1204 – 1502; 200 – 250 | 1350; 225 |
| summary threshold | 26 – 36 | 31 |
| first war after the goblins' awakening (years) | 299 – 2704 | 812.5 |
| standing: faerie / elf / orc / human / dwarf / goblin | 3–13 / 2–29 / 1–14 / 3–29 / 4–20 / 7–92 | 5.5 / 8 / 5 / 10 / 8 / 42 |

## 6. Finding F1 for the lead: the orcs are weaker at 0.19.0

The lead asked for this after Paul's T8 ruling. Figures are from the same seeds, 0.19.0 against 0.18.0 (`sweep/orcs.txt`, from `orcs.py`).

| 1024 defaults, 100 worlds | 0.18.0 | 0.19.0 |
| --- | --- | --- |
| worlds where orcs end with one standing settlement | 7 | **21** (seeds 0, 2, 9, 11, 14, 16, 17, 24, 32, 37, 40, 46, 51, 53, 55, 62, 65, 81, 85, 94, 95; 4 of them had one at 0.18.0 too) |
| worlds where orcs are extinct | 0 | 0 |
| orc standing settlements (sum; median) | 666; 6 | 590; 5 |
| settlements founded by orcs | 611 | 541 |
| orc people (sum) | 1,431,970 | 1,306,290 |
| orc share of people (median per world) | 48‰ | 42‰ |
| orc share of standing settlements (median per world) | 63‰ | 50‰ |
| wars declared by orcs | 306 | **241** |
| orc wins / losses (`peace`, `war_won`) | 114 / 68 | **95 / 77** (T5 met) |
| losses as attacker | 57 | 68 |
| orc settlements sacked / captured | 38 / 23 | 51 / 10 |
| orc flights (`refugees_settled`) | 16 | 26 |
| an awakened orc realm's first war: realms with one | 75 | 80 |
| of these, declared with at most one standing settlement | 55 | 57 |
| of these, lost | **22** | **38** |
| of these, lost with at most one settlement | 21 | 29 |
| mean years from founding to the first declaration | 188 | 179 |

**The other sets.**

| Set | One standing orc settlement, worlds (0.18.0 → 0.19.0) | Orc standing (sum) | T5 (0.18.0 → 0.19.0) | First wars lost |
| --- | --- | --- | --- | --- |
| calm | 0 → 8 of 20 | 151 → 80 | 24/9 → 8/18 | 6 of 15 → 14 of 17 |
| turbulent | 0 → 10 | 117 → 64 | 22/15 → 8/20 | 3 of 15 → 10 of 14 |
| span 2000 | 16 → 18 | 24 → 22 | 3/5 → 1/9 | — |
| civs 4 | 0 → 4 | 187 → 166 | 16/22 → 31/24 | — |
| 512 | 4 → 9 of 30 | 132 → 105 | 32/17 → 35/15 | — |

**What causes it, as far as the rows show.** The chain is the same in nearly every one of these worlds. Calm seed 0 is the clearest case (critic folder: `orc-calm-seed0-1024`):

1. The Krurz Horde awakens in 5534 with one settlement.
2. It raises its quarrel claim on the humans' region the same year (e170; 21.12).
3. It declares in 5700, still with that one settlement, at odds of at least 500 (21.9).
4. It loses two battles. Its capital is sacked (e211), and its last refuge flees (`refugees_settled` e212; 20.1).
5. The refuge lies low for `hidden_years` (21.10) and founds no colony.
6. The orc age ends 300 years later with that single settlement.

At 0.18.0 the same Horde made its first claim in 5599, founded a colony first, declared in 5744 with two settlements and won a peace. Across the default set the realms' first wars are declared about as often as before, and as often with one settlement (57 against 55). They are lost far more often (38 against 22). What changed:

- **The quarrel (21.12) picks the neighbour by distance, not by weakness.** The first claims fall on humans (44 of 89), dwarves (21), elves (12) and faeries (11), and the claim comes in the awakening year (median 0 years).
- **The shorter odds (21.9, 500 against 700)** let a one-settlement realm declare on that neighbour.
- **Flights and lying low** then fix the loss in place. A fallen last refuge flees (26 flights against 16) and cannot colonize for `hidden_years` of an orc age of about 450 years.

**Aim endings (21.6) are not the cause.** The orc wins at 1024 defaults are all as attacker (95). The reference's ending reasons show 133 aim wins in all, and a further 376 attacker wins after the goal was lost by the defender.

**The quarrel strain is not the lever.** I stopped the tuning runs when the lead's message came; no recommendation rests on them. The one that had completed, `quarrel_strain` 450 on the default, calm, turbulent and span 2000 sets (`tune/out-q450/`), gives:
- 19 one-settlement worlds at 1024 defaults (against 21);
- T5 101 / 75, calm 7 / 18, turbulent 8 / 20.

The orc-odds-600 run stopped at 44 worlds and gives no figures.

**Targets and invariants.** No merge target or invariant fails:
- T1–T7 hold at 1024 defaults (T5 95 / 77).
- No race is extinct in any set (T4).
- Invariants (8) and (12)–(16) hold in every world.

T5 fails outside the defaults, in calm, turbulent and span 2000. T5 is defined at 1024 defaults.

**For the critic.** Four such worlds are in `critic/`, each with `.chronicle.txt`, `.summary.txt`, `.report.txt`, maps and names. Each is byte-identical to its sweep world.

| Critic world | Orc standing settlements, 0.18.0 → 0.19.0 |
| --- | --- |
| `orc-seed0-1024` (1024 default seed 0) | 9 → 1 |
| `orc-seed53-1024` (seed 53) | 18 → 1 |
| `orc-calm-seed0-1024` (calm seed 0, the chain above) | 8 → 1 |
| `orc-turbulent-seed14-1024` (turbulent seed 14) | 17 → 1 |

## 7. Purity, settings, timing

- **Threads.** `--threads` 1, 2 and 6 at 1024 seed 0 give byte-identical files. At 512 seed 42, `--threads` 1 and 5 are identical and equal the golden `geo-seed-42-512` (`86d207b3…`).
- **`--until history`** is byte-identical to the default.
- **Settings** (against seed 0 at 1024):
  - `--monster-pressure high` and `--divine-activity meddling` change only `meta`;
  - `--until chronology` drops the history tables;
  - `--history-span 2000` and `--races elf` change `race`, `age` and the history.
- **Timing.** `generate` at seed 0, three runs each, alternating 0.18.0 and 0.19.0, at load average 2.0–3.6:

| Map | 0.18.0 | 0.19.0 |
| --- | --- | --- |
| 512 | 2.64–2.70 s | 2.64–2.68 s |
| 1024 | 9.09–9.15 s | 9.15–9.17 s |
| 2048 | 36.87–37.04 s | 37.17–37.28 s |

The new `sea_d` search, the region-name rules and the core rule together cost at most 0.7% at 2048 and less at the smaller sizes. I found no slowdown worth a defect.

## 8. Recorded hashes

`worldgen/tests/golden/p0-worlds.txt`:

```
empty-phrase 7519aac0bf423ecb9347055b687dd32ca4f4c7713d7c5eb26529f643010edc54
empty-seed-0 b9b716578c7487776281f4bb265b3f1bffdeca2b870c8fac4deab84f7a5c4845
empty-seed-0-512 8ca79cd9e1fea74fa02b8611314dd375cd159df994d57a3b530186bfd6860fb3
empty-seed-1 0efe8107054ea6737357694133ffb7d7eb2f43136a70fb5468ff5fffaef225e6
empty-seed-42 f7b6607586e6cb7b516f826b0440043c0f88406390f9c50bea7f4d4f2c8c46e1
empty-seed-max 072fcc0a03c83e8dc04eee65966136b95f0ef55c68172ddb8f33baa5dcd05678
pattern-seed-0 21c7140f5a4ccc7d5e3b25f2791c443adef3fb3934f3154b693fd319b07e54af
pattern-seed-42 4b642129743ac5174a3cda28467aae5c76537af6ac888bcdbb6225f47baa50d2
```

`worldgen/tests/golden/p1-worlds.txt`:

```
geo-phrase-512 f10818c5f27609c54fc6a098a02a9509c25728d2951dc9b807568e993f01ac6c
geo-seed-0-512 8487b93aafe34ee75416bab1afdde47575371a3309146e4a22ceae73a600ef2a
geo-seed-1-1024 008a4d705e300e768988a1b0595e8c0170e642758cac8035750ea8411e8f034c
geo-seed-42-512 86d207b351d925692e42cf51e69845dc7962a1266acec5fd3fe2e69e30b695cc
```

`worldgen/tests/golden/p3-worlds.txt`:

```
hist-dense-1024 aaedc3640a9f7b0051bf29b9098ce9276e6c661029f25c5c5fc09d6b88ff7e0f
hist-races-elf-orc-1024 e63cb2358bd66b7c3d1964d0e7f76f0e4a8dd1425b1d433f50cd3e1db68f6849
hist-span-2000-512 f79f2621d137f51c0fc518be0390b83dd2a9d4438a335ac3a9a3486d3725e7ca
```

**Inline constants** (the empty worlds with the content hash of no data, generator 0.19.0):

`worldgen/tests/db_determinism_test.cpp`:

| Case | Hash |
| --- | --- |
| empty-phrase | `3808c4155d50bf6c48fc6fa4fa1b989bc89cea97c15db2e80ae638afa001ec35` |
| empty-seed-0 | `53c6e5c98e8dff9172a02fec19d1f2cd8435aca36dea62cb5a1312d72c12449c` |
| empty-seed-1 | `1f7e3b5bee029813ab7ac9d05075f826aac3888ae55374379302344729a7a140` |
| empty-seed-42 | `762ada26542d35304d33cd0b8e57171f2a422dff228c7e481b8b441727ead570` |
| empty-seed-max | `9cc6f477d6c97fc979945201f9283c5806b8194bcfdc2fb018e0b274217715f0` |

`worldgen/tests/stages_pipeline_test.cpp`:

| Case | Hash |
| --- | --- |
| the empty world | `53c6e5c9…` |
| the test pattern | `fd643a8289ab2810d3c4ae3392d7ac22a80dc158b7c3d617d92fde43b18c9232` |

- **CLI tests.** `cli_golden_test` and `cli_p1_generate_test` read `p0-worlds.txt` and hold no inline values.
- **Per-stage vectors.** These were re-recorded by the test writer on `p3/10-int`, and I regenerated none.

**Why they changed.** Every world records `meta.generator_version` 0.19.0. The shipped data changed too:
- race fields `cell_weights`, `coast`, `odds_min_permille` and `awakening_quarrel`;
- the tuning fields;
- the templates;
- the filter and placename lists.

Together these change every `content_hash`, so even the empty worlds differ. History worlds also change from their first quiet line or plague.

**README phrase world.**
- `generate --seed-phrase "The Unmapped"` hashes to `cdeb64eaa8f9097366f82b05de89cdfb4c3eca009a7cc4026cf17f6fbbef121a`.
- The report is `~/.cache/the-unmapped/out/p3-10-validation/readme-report.txt`: 11 civilizations, 611 events, 79 outbreaks, 7 quiet stretches, and `events_plague` and `events_quiet_era` 0.
- I did not edit the README.

## 9. Critic folder

`~/.cache/the-unmapped/out/p3-10-validation/critic/` holds the same thirteen worlds as P3-09 plus the four orc worlds of section 6:
- 1024: seeds 42, 0, 1, 2, 3, 7, 11, 23;
- 1024 seed 42 turbulent and seed 42 `--civs-per-race 4`;
- 512: seeds 42 and 5;
- 2048: seed 42.

Each world has `.db`, `.report.txt`, `.regions.png`, `.map.png`, `.chronicle.txt`, `.summary.txt`, `.sentences.txt` and `.names.txt`, in the P3-09 formats (`chronicle10.py`). The 0.19.0 sentences come from the C++ dump.

## 10. Findings (no defect)

- **F1. Orcs weaker** (section 6). This is for the lead's ruling, and the critic should read the four worlds.
- **F2. Dwarven colonies below 30 m.**
  - 1024 defaults: 5 of 760 (none a fortress).
  - civs 4: 12 of 341, 2 of them fortresses at 1.7 m and 2.0 m.
  - All are colonies in regions with little or no high ground, so the 22.5 site value cannot lift them.
  - The spec's harness measured 0 of 166 on 20 worlds.
  - Dwarven cities standing fell from 17 to 3: higher sites carry fewer people.
- **F3. Faeries at 512.** 36% of faerie settlements lie within 300 m of the sea (0.18.0: 60%). The target of 20% was stated for 1024, where it is 11%.
- **F4. `worldgen-tests-engine`** takes 31 s alone and 34–41 s in a run on release, above the 30 s mark. Four new P3-10 cases add about 13 s. The build engineer or the test writer could split the entry or move the stub-world case to a slow entry.
- **F5. New names worth a look:** `Beamcumstead`, `Twypoo`, `Teefass`, `Wasssie` (section 3).
- **F6. The 21.7 weariness boundary.** I read "weary until Y + `search_weary_years`" as exclusive, and 25 claims or declarations at 1024 defaults fall exactly on year Y + 500. The spec could state the bound.
- **F7. My own check, corrected mid-sweep.** My first `check10.py` still applied 0.18.0's invariant (11): a battle in a concealed region needs a successful search. It flagged battles in regions where the war had already fought. 21.7 item 2 lifts the search there, so I fixed the check and re-ran it over all 375 worlds. This was not a generator fault.
- **F8. Faeries fewer.** Faerie standing settlements fell from 763 to 638 (−16%) and their people by 12% at 1024 defaults. In exchange they stand inland.

## 11. For the lead

1. **Rule on F1, the orcs.** With Paul's T8 ruling, no target is missed at defaults. If the lead wants the 0.18.0 orc strength back without reopening T8, the evidence points at 21.9's odds (500) for a one-settlement realm, together with 21.12's choice of neighbour by distance, rather than at `quarrel_strain`. That is a recommendation only.
2. **F4** is for the build engineer or the test writer.
3. **F6** is a spec-text clarification for the spec writer.
4. **Adopt the typical ranges of 5.8** into P3-06 15.5 (spec writer).
5. **The README example** still shows 0.18.0. The new phrase-world hash is in section 8, and I did not edit the README or `CLAUDE.md`.

## 12. Files

Under `~/.cache/the-unmapped/out/p3-10-validation/`:

| Path | Contents |
| --- | --- |
| `logs/summary.txt` | every step's exit status and time |
| `logs/ctest1-*.log`, `logs/ctest2-*.log` | ctest rounds 1 and 2 |
| `logs/slow-*.log` | slow entries |
| `logs/world-hashes-local.log`, `logs/expect-*.log` | cross-preset comparison and `expect` |
| `logs/gdext-*.log` | extension build, ctest, headless check, hashes |
| `logs/lint*.log` | lint |
| `sweep/<set>/` | per world: `.db`, `.txt`, `.report.txt`, `.sent.txt`, `.regnames.txt`, `.check.json`, `.ref.txt` (and `.pref.txt` for `elf`) |
| `sweep/stats10.txt`, `sweep/orcs.txt`, `sweep/compare18.jsonl` | statistics, the orc finding, the 0.18.0 comparison |
| `sweep/ref-results.txt`, `sweep/pref-results.txt` | reference results |
| `old10/` | `check10.py` on the 0.18.0 worlds |
| `t8/` | the T8 harness per world |
| `critic/` | the seventeen critic worlds |
| `purity/`, `timing/`, `compat/` | purity, timing, the 0.18.0 world read by 0.19.0 |
| `readme/`, `readme-report.txt` | the README phrase world |
| `tune/out-q450/` | the stopped tuning run (appendix figures of section 6 only) |

The scripts are in the same folder: `check10.py`, `stats10.py`, `regstats.py`, `orcs.py`, `t8.py`, `compare18.py`, `chronicle10.py`, `sweep-job.sh`, `run-sweep.sh`, `recheck.sh`, `old10.sh`, `purity.sh`, `timing.sh` and `critic/make.sh`.
