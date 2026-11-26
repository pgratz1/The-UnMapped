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
