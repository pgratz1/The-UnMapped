# P5-12 regions renamed by history: validation (generator 0.22.0)

Validated by the validation specialist on 2026-11-23 on branch `p5/12-validation` (from `origin/p5/12-int` at 535dd47). Generator 0.22.0, schema 8, history report format v3. Spec: `docs/specs/p5-12-renamed-regions.md`, targets in its section 11. Model: `docs/reviews/p5-01-followups-validation.md`.

Verdict: **the stage's own targets R1 to R8 and N1 to N3 are all met, the world hashes agree on four presets and are recorded, and every ctest entry passes except one slow entry; but three findings need the lead** (sections 4, 5 and 6): (1) `worldgen-tests-slow-biomes-resources-regions` fails on the case `shipped-512-phrase` because the test writer's own RG11 in `p1_06_regions_names.hpp` has no V8 (test defect; the generator and the reference agree); (2) the independent report reference differs from the C++ report in 17 of 185 sweep worlds, on one point: the region name inside a war declaration's claim note (a spec reading); (3) T16 (repeated battles) is 13 against a bound of 10, a count the lead is to judge (section 5). `p3_conflict_reference.py check-world` still refuses schema 8 (test writer).

Machine: 6 cores shared, load 3 to 7 throughout; wall times are inflated.

## 1. Cross-compiler world hashes and goldens

`tools/ci/world-hashes.sh local linux-gcc-release linux-clang-debug linux-clang-release linux-gcc-debug`: **all 15 cases identical across the four presets** (detail lines, world report, both history reports and the territory hashes of each case with history). `expect linux-gcc-release`: 15 of 15 ok after recording. Windows and MSVC were not available.

Recorded (LF, C-locale sort order, which `cli_golden_test` checks):

| File | Change |
| --- | --- |
| `worldgen/tests/golden/p0-worlds.txt` | 8 cases |
| `worldgen/tests/golden/p1-worlds.txt` | 4 `geo-*` cases |
| `worldgen/tests/golden/p3-worlds.txt` | 3 `hist-*` cases |
| `worldgen/tests/db_determinism_test.cpp` | `GOLDEN_CASES` (5 values) and its comment |
| `worldgen/tests/stages_pipeline_test.cpp` | the two inline hashes (lines 496, 509) and comments |

Why they move: every world carries `generator_version` 0.22.0 and schema 8 (`user_version`, and the new table `region_name`, one more page in the empty world); the cases with data also carry the new `content_hash` (new tuning `renaming.toml`, human culture, report entry); the history cases change through the renamings, names and ids. The inline constants (no data, no stage) move only through the version string and the schema. Values are those the 0.22.0 build writes, read from the failing checks; the two tests holding `empty-seed-0` agree (2ae6024a...). Not touched: every other golden file.

| Case | SHA-256 |
| --- | --- |
| empty-phrase | 4f5961a241e4d235b80e5a133fcc6af408f9194593447956dc311f3279591ff2 |
| empty-seed-0 | 8f57f1a4cbf1c1b5aa418a91f0a125a200e015324a3b1c86994f31d972a27fd9 |
| empty-seed-0-512 | f3865331423f378efd2e9769db257ef8f173e8ee537a955e97b661b4be7a2cbf |
| empty-seed-1 | 2970e9c13c3d6dc90d5ea1f255528e0b693f1cb7203f7413381d314f22dfc879 |
| empty-seed-42 | a183da07b3acd2b44297d89ce99816037a4a034d2b27a5759004f1461363f0b8 |
| empty-seed-max | e0933ec2ce157f1bfaa30038cfe3554f24b97e9a9a1e08bcec8c51bc30516248 |
| pattern-seed-0 | 267f4890a711d622a86039df70801f2f1b63f95508e63a6e9d6927d5ea34797a |
| pattern-seed-42 | ae3a3e95dae400767021cf7c04ed7bb086b9500d4fdb9ff6584da6285d69857c |
| geo-phrase-512 | 07c40dc42005112d88c3bb78439cedee7daa784939fcbe503587edf281905310 |
| geo-seed-0-512 | a5ecaab6a7cd9a86802754d06d810d15a488cb6b22372a606620d8c49bdf51be |
| geo-seed-42-512 | 4fc140e9dc7c2d98aea0e14610c46683c81c98f65cd1fa6bf3b135c5dbb1016a |
| geo-seed-1-1024 | 36f6a547751b53fe8a8f82558f526583752410e067d3d0844758ea36d9e05173 |
| hist-span-2000-512 | 159b95730543a2f796c0610e1bc55c53ccbbdd289f3b2ce7e303423b859d91f9 |
| hist-races-elf-orc-1024 | 49f4a1c9ddbb1f7c0e4850669950be017a7069e7e6a2424c06ef62a79f8fbc60 |
| hist-dense-1024 | 24d9653135a0d519bfd3f1a81b94ee95dddd9ba9015017015ccfa3748f2c98f3 |

## 2. Test results per preset

| Preset | ctest | Result |
| --- | --- | --- |
| linux-gcc-release | 39 entries | first run 38 pass; `worldgen-cli-tests-other` failed only because my first `p0-worlds.txt` was sorted by locale (`cli_golden_test.cpp:108`); fixed with a C-locale sort and the three affected CLI entries re-run: pass. All 39 pass |
| linux-clang-release | 39, 39 pass | PASS (463 s) |
| linux-gcc-debug | 39, 39 pass | PASS (1,215 s) |
| linux-clang-debug | 39, 39 pass | PASS (1,552 s) |
| linux-clang-asan (subset: `worldgen-tests-renaming`, `-timeline`, `-history-report`, `-other`, both partition checks) | 6, 6 pass | PASS (1,181 s), no sanitizer report |
| linux-gcc-release-slow | 15, 14 pass | **FAIL: `worldgen-tests-slow-biomes-resources-regions`** (section 4) |

The eleven failing checks listed in the brief are all resolved by the recorded hashes. Determinism lint and self-test: clean (176 C++ files, 0 findings).

## 3. Sweep and targets

Worlds: `default` seeds 0 to 99 and `civs4` seeds 0 to 19 (`--civs-per-race 4`) are the engineer's (`~/.cache/the-unmapped/out/p5-12-impl/sweep/`); I regenerated seed 0 of both with my gcc-release build and they are byte-identical (`cmp`). `calm` and `turbulent` (seeds 0 to 19), `d512` (0 to 19) and `d2048` (0 to 4) are mine, 65 worlds, generated with the engineer's trace build `worldgen-cli-trace` (it prints the renaming statistics for R4) after `cmp` showed byte-identity with my build on `d512` seed 3 and `turbulent` seed 2. 185 worlds in all. Every `report --history` exits 0 at both levels (370 calls; at most 0.07 s each, warm). The independent checks of the test writer's references on all 185 worlds: `p5_12_renaming_reference.py check-world`: 0 problems in 185 of 185 (invariants 1 to 6 and the caps, R2 and R6 hard); `p3_conflict_reference.py check-world` (a scratch copy with its schema gate "7" widened to "8", otherwise unchanged): 0 failures in 185 of 185, 841,108 checks (T3, T6, T7, invariant 18, P3-00 12.3); `p3_report_reference.py compare`: equal at both levels in **168 of 185**, see section 5.

### P5-12 targets (default = 100 worlds at 1024)

| Target | Bound | Measured (default) | |
| --- | --- | --- | --- |
| R1 renamed regions per world | median 3 to 6; at least one in 90 worlds | median 4, p10 2, max 8 (145 per mille); 100 of 100 worlds with one | met |
| R2 caps | every world | hold in all 185 worlds | met |
| R3 by rule | no rule above 60%; each at least 10 | 402: capital_sack 185 (46.0%), slaughter 66, plague 46, conquest 44, heartland 34, great_sack 27 | met |
| R4 dropped for want of a name | at most 1% of candidates reaching item 2.6 | 0 of 402 reached (647 candidates), all 185 worlds 0 | met |
| R5 second renamings | reported | 17 of the 402 (prototype said 0); civs4 5, calm 0, turbulent 2, d512 2, d2048 4 | reported |
| R6 invariants 1 to 6 | every world | 0 problems in 185 (own checker and the reference) | met |
| R8 summary lines | default median at most 700, max at most 800; `civs4` median at most 1,100, max at most 1,400; T17 holds | default median 587, max 777 (seed 12); civs4 median 1,085, max 1,357 (seed 1); T17 median 7.6%, max 21.0% | met (civs4 median is within 1.4% of its bound) |
| N1 doubled or `ass` human settlement names | 0 in every sweep; fallbacks 0 | 0 in all 185 worlds; fallbacks 0 | met |
| N2 region names across a join; drawn names vs 0.21.0 | 0; only `Wanegrass` (15), `Pinegrove` (59, 91), `Tarden` (84) change | 0 renamed across a join; default changes are exactly those four: 4 `Wanegrass` to `Kestrelhorn Reaches`, 59 `Pinegrove` to `Larchwood`, 84 `Tarden` to `The Tainted Weald`, 91 `Pinegrove` to `Archer's Grove`; civs4 (seeds 0 to 19) only seed 15 | met |
| N3 `[present]` hidden refuges equal the reference's | every world; none listed found before E | equal in 185 of 185 (hidden 9 of 50 default, 2 of 4 civs4, 3 of 9 d512) | met |

R7 (other sets, R1 to R5), each set's median renamed regions per world / second renamings / max rule share: civs4 7 / 5 / 50.0% (capital_sack 69 of 138); calm 1.5 / 0 / 40.0% (18 of 20 worlds have one); turbulent 5 / 2 / 47.5%; 512: 1 / 2 / 84.6% (capital_sack 22 of 26; heartland 2, conquest 2: a small world renames mostly by capital sack, and `great_sack`, `slaughter`, `plague` do not occur; R3's "every rule at least 10" is stated for the 100-world default); 2048: 12 (79 per mille max) / 4 / 27.3%. R4: 0 in every set. Renaming years: default 2,455 to 5,999, median 5,595.

### P5-01 targets re-measured (default, 0.21.0 from `p5-01-followups-validation.md`)

| Target | Bound | 0.21.0 | 0.22.0 | |
| --- | --- | --- | --- | --- |
| T1 hollow wars per mille | at most 250 | 215 | 215 (494 of 2,299 fought; civs4 212) | met |
| T2 `wars_same_goal_unchanged_max` | at most 3 | max 3 | max 3, 0 worlds above, all 185 | met |
| T3 no refuge held while a destination exists | 0 | 0 | 0 (reference, 185 worlds); permitted holds `refuges_held_no_destination`: turbulent 32 in 5 worlds, others 0 | met |
| T4 no race extinct | 0 | 0 | 0 of 185 | met |
| T5 orc wins against losses | wins >= losses | 169 / 62 | 166 / 62 (civs4 49/29, calm 25/14, turbulent 23/21, 512 14/6, 2048 28/9) | met |
| T6 ending whose loser won the last battle | 0 | 0 | 0 (reference) | met |
| T7 battle above `kill_max_permille` | 0 | 0 | 0 (reference) | met |
| T9 worlds with one standing orc settlement, no war won, no sack | at most 7 | 2 | 1 (seed 95) by the P5-01 script; 2 (21, 95) by `orc11.py`; civs4 1, d512 3 | met |
| T10 worlds with no orc act after E - 250 and at most two standing | 0 | 0 | 0 (default, calm, turbulent, 2048); civs4 1 (seed 4) and d512 1 (seed 18), no target there | met |
| T11 median years awakening to first orc act | below 100 | 12.5 | 11.0 (p90 39; 1 world at or above 100) | met |
| T12 first orc war: won against lost | won >= lost | 38 / 17 | 39 / 21 (76 worlds with a war; 10 truces, 5 searches abandoned, 1 open) | met |
| T13 declarations not citing a claim on their region | 0 | 0 of 2,601 | 0 of 2,585 default, 0 of 1,665 civs4, 0 of 295 / 840 / 288 / 289 other sets; fresh claims 309 default (median 3, max 14) | met |
| T14 grim drawn region names, per mille of drawn | at most 50 at 1024 and 2048 | 44.1; 34.7 | 44.1 (191 of 4,328); 2048: 34.7 (33 of 951); 512: 34.3 (6 of 175) | met |
| T15 names (`fag`, `didin`, `ririn`, `shiva`, `hindi`, `fannie`, triple letters, crude fragments, `tard`; fallbacks) | 0 each | `tard` 1 (`Tarden`) | 0 in all 185 worlds (the script counts regions, civilizations, settlements and gods; the renamed names are in the R6 check) | met |
| T16 repeated battles; mixed toll clauses | at most 10; 0 | 9 of 8,820 | **13 of 8,935 (default)**; civs4 13 of 5,982; calm 1 of 940; turbulent 6 of 3,156; 512 14 of 1,012; 2048 1 of 1,066; mixed 0 everywhere | **not met** (section 5) |
| T17 calamity share, default | median at most 10%, max at most 25% | 7.8% / 22.8% | 7.6% / 21.0% (seed 72) | met |
| T18 summary lines, default | median at most 700, max at most 800 | 571.5 / 754 | 587 / 777 (min 331) | met |
| T18 `civs4` | median at most 1,100, max at most 1,400 | 1,046 / 1,353 | 1,085 / 1,357 | met |
| T19 `[present]` hidden marks equal the reference | every world | equal | the differences of section 5 touch war declaration lines only, never `[present]`; N3 above equal in 185 | met |
| T20 flare-ups shorter than `flare_min_years` | 0 | 0 of 383 | 0 of 383 default, 0 of 260 civs4, 0 of 18 / 158 / 71 / 25 | met |
| Quick aim wins (P3-06 22.5) | at most 3 | 0 | not measured separately; the conflict reference passes in all 185 worlds | not measured |
| P3-00 12.3 hard bounds | every world | 0 failures | 0 failures in 185 | met |

Other figures, default: T1 by the sweep's denominator 191 per mille (494 of 2,585 declared); simulated years 1,169 to 1,640 (median 1,342.5); orc standing settlements at E 924 (0.21.0: 994); civs4 T17 median 0.2% (the summaries of larger worlds carry almost no calamity lines, as noted at 0.21.0). Other sets, T17 median / max: calm 8.6/12.5, turbulent 3.6/15.3, d512 11.8/25.0, d2048 0.0/1.6; T18 median / max: calm 455.5/650, turbulent 683.5/1,041, d512 420.5/626, d2048 1,099/1,220.

## 4. Failure: slow regions golden, test writer's own RG11 has no V8

`ctest --test-dir <build> -R '^worldgen-tests-slow-biomes-resources-regions$'` (or `worldgen-tests -ts='*regions-slow'`): case `P1-06 golden (slow): the cases at N = 512 and 1024, on the layers of --until resources`, item `shipped-512-phrase` (seed 15629206466951235176, 512). Expected: the golden `p1-06-regions.txt` (re-recorded by the reference with V8) names region 11 `Lichenknoll Forest`, attempt 1. Actual at `stages_regions_golden_test.cpp:84-86`, `:165`, `:192`: the test's own implementation (`p1_06_regions_names.hpp`, "written from the spec") draws `Tarden` at attempt 0. Root cause: that header has no V8 (no filter anywhere in it), so it accepts `Tar|den`, which the spec's V8 rejects. The generator and the independent reference agree: the CLI world `geo-phrase-512` carries region 11 `Lichenknoll Forest` (`p1_regions_reference.py names`). The fast `-regions` entry passes only because none of its cases meets a V8 rejection. For the test writer; I changed nothing.

## 5. Findings for the lead

**A. Report reference differs from the C++ report in 17 of 185 worlds** (default seeds 6, 12, 23, 33, 46, 66, 67, 71 and more; civs4 5, 19; all in `~/.cache/the-unmapped/out/p5-12-validation/refout/*.cmp`). Every difference is a `war_declared` line (summary and full) and only the claim note's region name. Example, default seed 12, e974 (year 5721): C++ `(The claim, since 5671, was to Snab Pit in Northwest Peaks.)`, reference `... in the Ashes of Dreez-Drurk Pit.)`; rg15 was renamed in 5802, so the C++ prints the name in force at the declaration (and the same line's `over Northwest Peaks`), the reference prints the present name. P3-07 r21 26.4 says references outside a sentence print the present name and sentences the name at the event; the claim note (P3-07 23 item 3: "every name is in mid-sentence", `R_c` the claim's region) is inside the declaration's line but is not `render_event`'s sentence. Spec reading, not a hash question (C++ output is identical on all presets). Needs the spec writer to say which name the note carries, then C++ or the reference to change. The C++ tests pass (`worldgen-tests-history-report`), so no test pins either reading.

**B. `p3_conflict_reference.py check-world` has the gate `schema_version != "7"`** (line 6878): it refuses every 0.22.0 world ("schema version 8, check-world reads 7"). I ran a scratch copy with the gate widened (0 failures, 841,108 checks); the test writer should widen it in the repository.

**C. T16, 13 against at most 10.** Counts by set in section 3. The rule counts a battle with the same winner, loser, `winner_dead` and `loser_dead` as an earlier battle of the world. Cases (default, 13 of 8,935 battles; 1.45 per thousand; 0.21.0 9 of 8,820, 1.02 per thousand), `python3 -B ~/.cache/the-unmapped/out/p5-12-validation/t16.py <dir>` lists them:

| Seed | War (cause) | Events (years) | Sides, tolls (winner dead / loser dead) | Place |
| --- | --- | --- | --- | --- |
| 1 | e738 | e765 (5772), e793 (5790) | c4 over c3, 16 / 55 | same region, Brambled Weald |
| 16 | e704 | e736 (5891), e763 (5905) | c5 over c3, 18 / 64 | same region, Ruin of Gnix Hole |
| 27 | e372 | e401 (5650), e406 (5662) | c2 over c9, 10 / 26 | different regions, Leadhurst, Chalkcombe |
| 29 | e493 | e537 (4906), e538 (4909) | c6 over c1, 17 / 45 | different regions, Harrowhorn Steppe, Grey Wolds |
| 37 | e299 | e326 (4314), e328 (4316) | c4 over c6, 2 / 7 | same region, Skrikagg Waste |
| 67 | e514 | e567 (5459), e572 (5464) | c3 over c10, 8 / 25 | different regions, Otterpool Hurst, Central Downs |
| 67 | e835 | e841 (5955), e843 (5962) | c4 over c9, 2 / 8 | same region, Mother's Fold |
| 67 | e835 | e847 (5964), e851 (5971) | c4 over c9, 2 / 9 | same region, Mother's Fold |
| 69 | e342 | e344 (4729), e349 (4734) | c2 over c6, 13 / 47 | same region, Usurper's Woods |
| 82 | e926 | e942 (5618), e945 (5620) | c2 over c4, 26 / 90 | same region, Ruin of Skaz-Slizz |
| 83 | e181 | e193 (3874), e215 (3888) | c2 over c1, 3 / 11 | same region, Worn Downs |
| 96 | e705 | e713 (5643), e735 (5655) | c10 over c8, 3 / 12 | same region, Hallowed Wolds |
| 98 | e962 | e1009 (4950), e1022 (4958) | c3 over c9, 6 / 19 | same region, Taeiall Hollow Waste |

Sentences read identically apart from the second battle's region in the three cross-region pairs, e.g. seed 37: both `The Zuskuz Horde defeated the Warrens of Nak-Snork in Skrikagg Waste; seven of the defeated and two of the victors fell.` (e326, e328), seed 82: both `The Court of Moneth defeated the Warrens of Droz in the Ruin of Skaz-Slizz; 90 of the defeated and 26 of the victors fell.` Eleven of the 13 are between two battles of one war (nine in the same place), small tolls (2 to 26 dead) of the horde and warren races dominate. It looks like draw coincidence at small tolls, not a rule fault: the rate is 1.5 per thousand, the world stream changed with the extra renaming events (every later id and draw moved), and the bound of 10 is a count over a sweep of about 8,800 battles. Not retuned.

## 6. Timing

`generate --seed 0 --map-size 1024`, gcc-release, interleaved with the 0.21.0 build (`p5-01-validation`, with the data of `origin/p5/01-merged`), load 5.5 to 6.6 (shared): 0.21.0 12.07, 11.86, 12.36 s wall; 0.22.0 12.29, 12.22, 12.25 s wall; user 26.2 to 26.3 s for both. Within noise (mean +0.1 s); the bound (at most 5 s more, whole `generate` under 60 s) is met. `report --history`: summary 0.011 s, full 0.012 s warm (at most 0.07 s over the sweep). Sweep generate times under load with the trace build, 512: 4.9 to 6.2 s, 1024: 17.5 to 19 s (three at a time), 2048: 63 to 82 s.

## 7. README sample

`generate --seed-phrase "The Unmapped"` (1024, default data, generator 0.22.0): file SHA-256 (`hash --db` and `sha256sum` agree) **`0c54c4be64e5847cfb7a099a63ecd94b6e41980d7980e7fdec16bfbfded52908`**; `content_hash` `bae8414942ee5ef1754afa7a23b8ea79c9ce64d2036588e621dbd01e151ee192`; `region_name` rows 3, `regions_renamed` 3. Files: `~/.cache/the-unmapped/out/p5-12-validation/phrase/{world.db,world.report.txt,summary.txt}`.

## 8. Artifacts

Under `~/.cache/the-unmapped/out/p5-12-validation/`: `sweep/<set>/` (my 65 worlds, reports, `times.txt`, `rtimes.txt`), `r512-<set>.txt` (R1 to R7, N1 to N3), `t501-<set>.txt` (T-targets), `orc11-<set>.txt`, `refs-cmp.txt`, `refs-conf2.txt`, `refs-ren.txt`, `refout/` (per-world reference output), `logs/` (builds, ctest, world-hashes), `timing/`, `phrase/`, scripts `job.sh`, `refs.sh`, `conf.sh`, `t16.py`, `t14.py`, `r512.py`, `tests.sh`; the engineer's worlds for `default` and `civs4` are in `~/.cache/the-unmapped/out/p5-12-impl/sweep/`; R4 for those two rests on the engineer's trace lines in their `.db.err` files.
