# P5-07 mountain depths: validation (generator 0.29.0)

Validated on 2026-11-26 on branch `p5/07-val` at f18efdd (engine r2 with goblin warrens and allies' aid, both calibrations, tests, integration parts 1 to 3). Generator 0.29.0, schema 13, history report v10, 107 streams. Spec: `docs/specs/p5-07-mountain-depths.md` r2; calibration: `docs/reviews/p5-07-calibration.md`.

Verdict: **pass.** All five Linux presets are green (52 of 52 each), the slow suite on `linux-gcc-release` is green (22 of 22), both lint commands are clean, the sixteen world-hash cases are identical across the five presets, and all 360 sweep worlds generated and reported with exit 0 and empty stderr. No stage-logic defect found. One checker false positive (D1, reference only). Targets: every DP, GW and AD bound holds on A and B except the known tail cases listed in section 4 (DP4 "most in one world" is 4 on both samples; GW4's mean misses as the lead accepted). Windows and MSVC were not available: cross-platform equality is proven across five Linux presets only.

Machine: 6 cores shared with other agents (load 8 to 9 during builds), builds two chains at -j 3 and -j 2, `ctest -j 4`, sweeps three worlds at a time.

## 1. Presets and suites

| Preset | Build | `ctest` |
| --- | --- | --- |
| linux-gcc-release | 1,317 s | **52 of 52** (836 s, includes the rebuild of two edited test files) |
| linux-clang-release | 1,660 s | **52 of 52** (452 s) |
| linux-gcc-debug | 921 s | **52 of 52** (1,990 s) |
| linux-clang-debug | 1,116 s | **52 of 52** (2,085 s) |
| linux-clang-asan | 1,472 s | **52 of 52** (911 s), no sanitizer report |
| linux-gcc-release `-L slow -j 4` | built | **22 of 22** (2,144 s) |

Determinism lint: self-test 22 snippet files, 0 failures; scan 190 C++ files, 13 CMake files, 0 findings. `gdext-headless-check` not built (extension off). Logs: `~/.cache/the-unmapped/out/p5-07-validation/logs/`.

Before the goldens were recorded, `worldgen-tests-other` failed on the world-file constants of two C++ tests (as the integration notes expected): the five `GOLDEN_CASES` of `worldgen/tests/db_determinism_test.cpp` and the two hashes of `worldgen/tests/stages_pipeline_test.cpp` (worlds with no data; they carry the content hash of the changed shipped data, so the change is explained by 0.29.0's `monsters.toml`, race fields and kinds). Both updated from the values the tests printed; the same values were then confirmed green on all presets.

## 2. Hashes and goldens

`tools/ci/world-hashes.sh local` over the five presets: **all 16 cases identical** (detail lines, world report, both history reports, territory and markers hashes). Then `world-hashes.sh expect linux-gcc-release` matches the re-recorded files (all 16 ok). Recorded: `worldgen/tests/golden/p0-worlds.txt` (8), `p1-worlds.txt` (4), `p3-worlds.txt` (4), byte-sorted.

| Case | SHA-256 |
| --- | --- |
| empty-phrase | `adbcf31736137f497eac1242bac5b258559bdeae3a9cd14663ef4704e3f8024e` |
| empty-seed-0 | `33d65418655176b61953936519b4261bc4be499e1ef66b6fe8655af0067eedc8` |
| empty-seed-0-512 | `6efc6f1f5b0d3057701f99f74482fe03712d11903147feb736a2f11ae5794f7b` |
| empty-seed-1 | `ff557d77e1dc42701b654d16d006787c3695bd1cadcfeff1f4d40bf6a3b5fbc6` |
| empty-seed-42 | `1895008aec9294b26603ccd9d6586994b73091d6067919a2b5ffd275bc66a7d1` |
| empty-seed-max | `6aa417218dbfcdc49570b02ebd9b796f5346f32e5e9d814754787acd0407ffed` |
| geo-phrase-512 | `4dc8b027d0e4b7df76046b22b4272068270c699eecee784ba1fc734e0c8389d3` |
| geo-seed-0-512 | `736f6b9e5aff33256bee2a6e37b350f4eca3059d77c700066cb0e851aef68cba` |
| geo-seed-1-1024 | `cadacecf8aeabaae5d77df88d1a3b37647824fe1392c00d54c955da83940c5ad` |
| geo-seed-42-512 | `db1c7d439260cb15aad2067bb67a1c7f0870d24675ed9dc396ca20ef5c2d52e3` |
| hist-dense-1024 | `df096fcde67d09037bfe46341f70528286246d923202766953696454030a4de4` |
| hist-races-elf-orc-1024 | `bb22f17542c04285ccb4054d41f3f69e19f547d8d55a2810e07ac4ce1f8e1bd2` |
| hist-span-2000-512 | `482b59a88e065a304c1b502e1b6f79ca215a8a9f1bbc833af2abf1516534e341` |
| p5-monsters-high-1024 | `1d698cb682b67adf15373fb86c30f48031bd8c1cc230a3e9069c2a073638ba4d` |
| pattern-seed-0 | `43d99f19970fbe330a7c138105d5ab3c392b6b8689d34d45d3a36947d1ae374b` |
| pattern-seed-42 | `32c3e7d8558cfe4e961b38566e44fde8e8fd0561d7fb6dfb2a1e67d16321d0a2` |

Phrase world "The Unmapped" (default size 1024): file hash `ee31c8ab8da7ab7312913682c4215a7cc14bca6fe684cca79f5478345d76309e` (0.28.0: `4766b025...`; the README hash needs updating by the lead), with `hash --db --detail` in `~/.cache/the-unmapped/out/p5-07-validation/phrase/hash.txt`.

## 3. Sweeps

Release build (gcc), `generate --map-size N` and `report --history` at both levels, then the metric scripts: default seeds 0 to 99 (A) and 100 to 199 (B), 30 seeds each of `--monster-pressure high`, `--civs-per-race 4` and `--event-intensity turbulent`, 10 at 512 and 20 at 2048: **360 worlds, 0 generate or report failures, no stderr.** `--monster-pressure low` and `calm` were not in the brief and were not run.

Reference checks on the sweep: `p5_07_depths_reference.py check-world` on all 200 default worlds (199 pass, 1 false positive, D1); `p3_conflict_reference.py check-world` (full replay) on seeds 0 to 7 of A: pass; `p3_report_reference.py compare` on seeds 0 to 19 of A: pass (20 of 20).

Timing: `generate --seed 0 --map-size 1024` **25.7 s and 25.6 s user** (wall 13.2 s; 0.28.0: 24.9 s and 25.6 s; the machine was loaded). Mean wall per world with three jobs and metrics: 17.7 to 19.8 s at 1024, 6.6 s at 512, about 90 s at 2048.

## 4. Targets

A = default seeds 0 to 99, B = 100 to 199. Calibration figures (r2 final) match exactly on A and, where the calibration gave them, on B.

| Target | Bound | A | B |
| --- | --- | --- | --- |
| DP1 worlds with a depth-3 mine | 40-90 | 53 | 50 |
| DP2 worlds with a waking; most in one world | 25-55; <= 4 | 37; 3 | 31; 3 |
| DP3 sealed share of wakings | 30-65% | 42.2% (19 of 45) | 41.7% (15 of 36) |
| DP4 worlds with a hold lost; most in one world | 15-40; <= 4 | 21; 4 | 18; 4 |
| DP5 realms ended by the deep (dwarven realms) | <= 8%, >= 1 in 200 | 1 of 145 (0.7%) | 0 of 158 (1 in 303 worlds in all) |
| DP6 alive at E | >= 70% | 82.2% (37 of 45) | 88.9% (32 of 36) |
| DP7 slain | <= 25% | 17.8% (8 of 45) | 11.1% (4 of 36) |
| DP9 dwarf regions held, median / mean | >= 2 / >= 0.28.0 - 0.2 | 2 / 2.11 | 2 / 2.23 |
| GW1 worlds with a depth-3 goblin warren | 10-50 | 30 | 22 |
| GW2 goblin share of wakings | 10-30% | 13.3% (6 of 45) | 13.9% (5 of 36) |
| GW3 goblin holds lost (reported) | - | 4 of 39 | 4 of 27 |
| GW4 goblin regions held, median / mean | median <= 12; mean <= 12.1 / 12.6 + 0.2 | 12 / **12.82** | 12 / **12.86** |
| GW5 dwarf regions median | >= 2 | 2 | 2 |
| AD1 aided wakings | 10-40% | 28.9% (13 of 45) | 25.0% (9 of 36) |
| AD2 aided gates held / fell | reported | 8 / 5 | 6 / 3 |
| unaided gates held / fell | reported | 15 / 17 | 12 / 15 |
| Most holds lost in one world | <= 4 | 4 | 4 |
| M4a living monster at E | >= 85 | 92 (90 without deep horrors) | 95 (95) |
| M4b lair on a former settlement | average >= 65, each >= 60 | 69 | 75 (average 72) |
| C1 wars median | 22-28 | 24 | 23 |
| AL5 wars with an ally | 20-45% | 30.1% | 32.3% |
| T16 repeated battles per thousand | <= 2 | 1.49 | 1.06 |
| G2 standing settlements / population, median | >= 81 / >= 205,000 | 83.5 / 211,121 | 82 / 217,205 |
| AL1 alliances formed / standing, median | report | 14 / 10 | 14 / 10 |
| AL6 refusals against joins | refused <= joined | 781 <= 959 | 787 <= 1,002 |
| Dwarves in alliance or trade | >= 85% | 90% | 89% |
| FG1 figures median / most in a world | 60-120 / <= 250 | 78 / 225 | 78.5 / 212 |
| T18 summary median / max (lines) | <= 670 | 625 / 778 | 622.5 / 778 |
| TW1 towers ended | >= 5% | 7.6% | 7.4% |

GW4 mean: the lead accepted the mean's miss (median 12 holds). Both samples' goblin mean (12.82, 12.86) is the engine baseline of 0.28.0 plus noise, not the warrens (calibration, Misses 1). Nothing else misses. The "most figures in a world" cells (225, 212) are over the older bound of 200 of P5-06 (known) and inside the 250 of the 0.28.0 table.

Per-race regions held at the end (median / mean; 0.28.0 in brackets, from the 04a validation):

| Race | A | B |
| --- | --- | --- |
| goblins | 12 / 12.82 (12 / 12.1) | 12 / 12.86 (12 / 12.6) |
| humans | 3 / 4.71 (3 / 4.9) | 3 / 5.49 (4 / 5.6) |
| elves | 3 / 3.64 (3 / 3.7) | 3 / 3.64 (2 / 3.5) |
| orcs | 3 / 3.64 (2 / 3.3) | 3 / 3.45 (2 / 3.4) |
| faeries | 2 / 2.48 (2 / 2.9) | 2 / 2.46 (2 / 2.7) |
| dwarves | 2 / 2.11 (2 / 2.2) | 2 / 2.23 (2 / 2.4) |

Goblins hold about 0.7 and 0.3 regions more on average than 0.28.0 (A, B); dwarves 0.1 and 0.2 fewer; faeries 0.4 and 0.2 fewer; orcs about 0.3 more. Medians equal 0.28.0 except orcs (3 against 2) and, in B, elves (3 against 2) and humans (3 against 4).

Other sets (bounds are the defaults' per 100 and apply only as guards; counts are of 30 worlds):

| Set | Results |
| --- | --- |
| `high` | 19 wakings in 13 worlds; 5 worlds with 12 holds lost, **most 5 in one world**; 0 realms ended; GW1 9 worlds; AD1 2 of 19; living monster 29 of 30; wars median 21; standing 77, population 207,760; dwarves in alliance or trade 86.7%; summary median 609.5 (max 785); T16 0.81 |
| `civs4` | 26 wakings in 17 worlds; 7 worlds with 11 holds lost (most 3); 0 ended; aided 42.3% (11 of 26); DP3 65.4%; wars median 92; standing 131; summary median 911 (max 1,079); figures median 261.5 (max 359); AL5 48.1% |
| `turbulent` | 18 wakings in 13 worlds; 6 worlds with 11 holds lost (most 4); 0 ended; **AL6 refused 615 > joined 548**; wars median 46.5; T16 1.85; dwarf regions median 1 (mean 2.40); dwarves in alliance or trade 83.3%; population 199,203; summary median 722 (max 940) |
| 512 (10) | 4 wakings in 4 worlds; standing 31.5; summary median 537.5; T16 15.08 per thousand (small map, as before) |
| 2048 (20) | 18 wakings in 12 worlds; 9 worlds with 16 holds lost (most 4); **1 realm ended by the deep (1 of 70 dwarven realms)**; dwarf regions median 4; standing 297, population 857,855; summary median 926 (max 1,132) |

## 5. Findings

1. **GW4 mean** misses on both samples, as the lead accepted; the median holds.
2. **DP5 is fragile**: one realm in 303 default worlds (seed 72), and one in 20 worlds at 2048; none in the 90 worlds of the three option sets. It holds the bound ("at least 1 in 200") by one event in A.
3. **DP4 most-in-one-world is 5 under `--monster-pressure high`** (seed 0); 4 on A and B.
4. **Non-default sets outside the default bounds**: turbulent has refusals above joins (615 against 548, as at 0.28.0) and population 199k; `civs4` has 48.1% of wars with an ally and 42.3% aided wakings; `high` has standing 77 (below 81); `high` dwarves in alliance or trade 86.7% and turbulent 83.3%. No bounds are set for these sets.
5. **Small sets**: warrens reach depth 3 in 9 of 30 `high` worlds and 1 of 10 at 512 (default bound 10 to 50 per 100); reported only.
6. **Forced seal and refused later take** (integration notes): exercised by the hand fixture `depths-forced`; not observed in any sweep world (not looked for here).

## 6. Defect reports

No stage-logic defect.

**D1 (checker false positive, open, not fixed): `p5_07_depths_reference.py check-world` invariant 12 on seed 55.**
- Seed 55, `--map-size 1024`, default settings, generator 0.29.0. Reproduce: `worldgen-cli generate --seed 55 --map-size 1024 --data game/data --out s55.db --force`, then `python3 -B worldgen/tests/reference/p5_07_depths_reference.py check-world s55.db --data game/data` prints `FAIL ... invariant 12: c2 started warren 11 (e428) while it held the live warren 23: got False, want True` and exits 1.
- Expected (spec 13 item 12 as restated by the lead): a realm starts no new warren while it holds a live one; a warren lost and won back, or taken from another realm, may leave a realm with two.
- Actual: c2 (Goggkrutch Horde) delved warren s23 (Gragg Pit, e109), lost Gragg Pit when c8 (Sluz Horde) seceded from it (e372, "with eight settlements"; plague e376 names Gragg Pit under c8), later c9 held it (siege e706), and c2 retook it in year 5691 (e707), after it started warren s11 (e428, year 5353). So at e428 c2 did not hold s23, and the engine is right.
- Root cause: `_life_end` in the reference (line 952) ends a warren's life on a `civ_seceded` only when the settlement is the event's own settlement or a participant. A secession lists one settlement (`@s20`) and a count (`settlements=8`), so the transfer of s23 is invisible to the checker. The engine's live-warren test (owner equals delver) is correct. The fix belongs to the test writer: derive the owner from the settlement's ownership history (the later events naming s23 with another civilization) rather than from participants. Left unfixed since the checker's data (no owner table across time) needs a design choice.

## 7. Not measured

- `gdext-headless-check` (extension off); Windows and MSVC (no machine; hashes compared across five Linux presets only, so cross-platform equality is still open for CI).
- `--monster-pressure low` and `calm`; `p3_report_reference.py compare` on seeds 20 to 199; the full-replay check on more than 8 worlds; no viewer or map images (no geography change).

## 8. Artifacts

`~/.cache/the-unmapped/out/p5-07-validation/`: `logs/` (builds, `ctest-*.log`, `ctest-slow.log`, `hashes.log`, `lint.log`, `timing.txt`), `sweep/<tag>/` (`table7.txt`, `table8.txt`, `m6.jsonl`, `m7.jsonl`, `m9.jsonl`, `refs.txt`, `times.txt`; tags `d0`, `d1`, `high`, `civs4`, `turb`, `s512`, `s2048`), `scripts/`, `phrase/` (world file, report, both history reports, hash, file hash), `critic/` (12 worlds with `summary.txt`, `full.txt`, `world.report.txt` and `world.db`, and `index.txt` saying what each shows), `s55/` (the D1 world).
