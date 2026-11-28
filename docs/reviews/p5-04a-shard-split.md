# P5-04a: splitting the relations, alliances and figures test entries

## The CI failure

CI run 45826699356 on `windows-msvc-debug`: `worldgen-tests-relations` hit the 3600 s timeout, `-alliances` took 3374 s and `-figures` 2679 s. The P5-04a test files made each entry far heavier than the 30 s cost its shard line assumed.

## Per-file times

Measured under `linux-gcc-release`, three files at a time, in seconds. They serve as relative weights only.

| File (`history_<x>_`) | s |
| --- | --- |
| alliances_p504a_property | 211.1 |
| figures_property | 184.6 |
| figures_p504a | 173.5 |
| alliances_property | 122.4 |
| relations_golden | 101.4 |
| relations_p504a | 96.7 |
| relations_p503a | 32.8 |
| alliances_golden | 27.6 |
| figures_golden | 23.1 |
| all other files (19) | under 15 each |

Totals: relations 265, alliances 367, figures 410.

## New entries

| Entry | Holds | Share of old entry |
| --- | --- | --- |
| `relations-golden` | golden file | 38% |
| `relations-p504a` | P5-04a file | 36% |
| `relations` | the other nine files | 25% |
| `alliances-p504a` | P5-04a property file | 57% |
| `alliances-property` | property file | 33% |
| `alliances` | the six other files | 9% |
| `figures-property` | property file | 45% |
| `figures-p504a` | P5-04a file | 42% |
| `figures` | the five other files | 13% |

## Limits

Patterns select whole files, so a single file sets the floor. The heaviest entries are `alliances-p504a` (57%), `figures-property` (45%), `figures-p504a` (42%), `relations-golden` (38%) and `relations-p504a` (36%). That puts them above the 40% target (a third for relations) except `relations-p504a`, which is just over a third. Scaling the old Windows times by these shares gives about 2000 s for `alliances-p504a` and 1200 s for `figures-property`. Splitting `alliances_p504a_property_test.cpp` itself into two files would be needed to bring `alliances-p504a` under 1500 s. That is a test change and outside this task. The slow entries are unchanged.

## P5-07: depths

CI run 45930034000 on `windows-msvc-debug`: `worldgen-tests-depths` hit the 3600 s timeout (the shard line `depths` assumed 30 s) and `worldgen-tests-alliances-p504a` took 2679 s. The ratio MSVC Debug to `linux-gcc-release` is about 12.7. Target: no entry above about 115 s of `linux-gcc-release` time (about 1500 s on MSVC Debug).

Per-file times under `linux-gcc-release`, three files at a time (relative weights only): `history_depths_property_test.cpp` 236, `history_depths_golden_test.cpp` 68, reader 7, data 7, report, forced and unit under 1 each. The P5-04a alliances property file took 122 s in the same conditions (65.7, 40.3 and 15.7 s for its three cases).

Both large property files were split by whole `TEST_CASE`s, with no change to any case, case name or suite name. The file-local helpers moved to the headers `p5_07_depths_property_common.hpp` and `p5_04a_alliances_property_common.hpp` (all `inline`).

| New entry | Holds | Linux s (alone) | MSVC Debug s at 12.7 |
| --- | --- | --- | --- |
| `depths-property-a` | cases 1 and 8 of the old file, the slow cases | 75 | 950 |
| `depths-property-b` | the shipped data at 512, determinism | 82 | 1040 |
| `depths-property-c` | I0, I1, the aid settings, warrens, resume | 73 | 930 |
| `depths-golden` | golden file | 68 | 860 |
| `depths` | data, forced, reader, report, unit files | 15 | 190 |
| `alliances-p504a-a` | invariants on the shipped worlds at 512, slow cases | 66 | 840 |
| `alliances-p504a-b` | alliances-small coverage, determinism | 56 | 710 |

`depths-property-c` took 148 s in a `ctest` run with four entries at once, against about 73 s alone, so the MSVC figure may be higher on a loaded runner. The case counts are unchanged: 1848 with `--count` and 1951 with `--count --no-skip`, before and after. The slow entries are unchanged (`slow-depths`, `slow-alliances` select by file pattern and take the new files).

CI run 45951641439 on `windows-msvc-debug` (MSVC Debug seconds): `depths-property-a` 1744, `-b` 1809, `-c` 3158 (estimate 930; timeout 3600).
`depths-property-c` is split into `-c` (I0, I1, the aid settings: 73 s on `linux-gcc-release`, loaded and alone alike) and `-d` (the warren rules and resume: 74 s); both about 1600 s expected on MSVC Debug at the observed ratio of 43.
The warren-rules case alone takes 67 s of the 146 s of old part c (46%), so no single case dominates; the case counts stay 1848 and 1951.
The `build` job timeout in `ci.yml` is raised from 180 to 300 minutes (`windows-msvc-debug` needs about 200 minutes for `ctest`).

## MSVC Debug job split

Before: in run 46127847316 the single `windows-msvc-debug` job took 249 minutes, 13,163 s (219 minutes) of it `ctest`; the longest entries were `figures-property` 2,249 s, `relations-p504a` 2,054, `other` 2,023, `alliances-p504a-b` 1,955, `figures-p504a` 1,881, `pantheon-property-c` 1,846 and `depths-property-b` 1,816. Every stage adds tests, so the next one would pass the 300-minute limit.

Design: the `build` matrix has two fields, `shard` and `shards`; `windows-msvc-debug` appears three times (shards 1 to 3 of 3), every other preset once (1 of 1) and behaves as before. Each shard configures and builds the preset, then runs `ctest --preset windows-msvc-debug -I k,,3`, that is the entries numbered k, k+3, k+6, ... of the preset's list. CTest numbers entries in definition order, which is fixed by the CMake files, so the parts are disjoint and together complete by construction. As a guard, each shard first lists all three parts with `ctest -N -I k,,3` and compares the sorted union with the sorted full `ctest -N` list (`diff`; an empty list also fails). Only shard 1 runs `world-hashes.sh generate` and `expect` and uploads `world-hashes-windows-msvc-debug`; the other steps are skipped, so `compare-hashes` and its `needs` are unchanged. Job names read `windows-msvc-debug (1/3)`. The timeout of a shard is 180 minutes. The stride puts neighbouring (usually same-stage) entries into different shards, which balances better than blocks.

Expected: ctest about 75 minutes per shard (219 / 3, with imbalance up to about 100), plus build and, in shard 1, the world hashes (some minutes): roughly 90 to 120 minutes a job instead of 249. The slow entries and the partition checks are unaffected (the latter run once, in whichever shard holds them). A new stage that adds entries keeps the split balanced on average; if a shard exceeds about 150 minutes, raise `shards` to 4 (matrix entries and nothing else).
