# CI job time variance after the CTest split

Build engineer, 2026-11-19, generator 0.10.0 to 0.11.0. Question: why do CI job times vary so much since `ctest` runs four entries at a time, and is it something in our build?

**Conclusion: it is runner speed, which we cannot control. Nothing in the workflow changes.** Every step of a job gets slower or faster by the same factor, including steps that run only one process at a time. The ratio between the steps stays the same. `-j 2` or `-j 3` would make jobs longer, and would not make them steadier.

## Data

Step durations come from the Actions API (`gh api repos/<repo>/actions/runs/<id>/jobs`). Per-entry times come from the ctest summary in each job log. The runs are the seven runs since the split (`44435776742` build/split-ctest, `44439442743` main, `44453075879`, `44463634852`, `44469962310` p1/06-regions-final) and four runs before it (`44309821785`, `44349403194`, `44357786250`, `44365982237`), plus `44408792775` and `44421211889` from p1/05-resources-final.

### Every step scales together

"Hashes" is the `world-hashes.sh generate` step. It runs `worldgen-cli` one case at a time, so four parallel ctest jobs cannot affect it. Times are in minutes.

| Job | Run | Build | Test | Hashes | Test / hashes |
| --- | --- | --- | --- | --- | --- |
| `linux-clang-asan` | 44439442743 | 1.7 | 20.6 | 5.0 | 4.1 |
| | 44435776742 | 1.7 | 20.8 | 5.0 | 4.1 |
| | 44463634852 | 2.3 | 28.6 | 6.2 | 4.6 |
| | 44453075879 | 3.3 | 42.2 | 9.2 | 4.6 |
| | 44469962310 | 3.3 | 42.4 | 9.3 | 4.6 |
| `windows-msvc-debug` | 44453075879 | 2.1 | 9.7 | 2.1 | 4.7 |
| | 44439442743 | 2.1 | 11.6 | 2.7 | 4.2 |
| | 44469962310 | 3.3 | 16.9 | 3.6 | 4.6 |
| | 44435776742 | 2.8 | 18.1 | 4.2 | 4.3 |
| | 44463634852 | 3.2 | 22.8 | 4.8 | 4.8 |
| `linux-gcc-debug` | 44435776742 | 1.5 | 4.0 | 1.1 | 3.8 |
| | 44463634852 | 1.9 | 5.4 | 1.4 | 3.9 |
| | 44469962310 | 2.4 | 8.3 | 2.0 | 4.1 |

On the same job, the Test step varies by a factor of 2.0 to 2.4 between runs. The single-process hashes step and the compile step vary by about the same factor. Test / hashes stays between 3.4 and 4.8 in every run since the split, on every preset. Before the split it was 5.7 to 7.1. The split made Test about 1.5 times shorter relative to the rest of the job, and it did not add variance.

The three `p1/06-regions-final` runs (`44453075879`, `44463634852`, `44469962310`) differ only in golden files, so code changes cannot explain the spread between them. In those runs, `linux-clang-asan` takes 28.6, 42.2 and 42.4 minutes, and `windows-msvc-debug` takes 9.7, 22.8 and 16.9.

### Every ctest entry scales together

These are `linux-clang-asan` per-entry times in seconds. In the three regions runs, each entry of the slow runs is 1.43 to 1.58 times its time in `44463634852`.

| Entry | 44463634852 | 44453075879 | 44469962310 |
| --- | --- | --- | --- |
| worldgen-tests-hydrology | 760 | 1096 | 1131 |
| worldgen-tests-elevation-golden | 777 | 1112 | 1122 |
| worldgen-tests-elevation | 641 | 948 | 958 |
| worldgen-cli-tests-hydrology-elevation | 623 | 946 | 932 |
| worldgen-cli-tests-regions | 448 | 684 | 679 |
| worldgen-tests-other | 372 | 533 | 544 |
| ctest total | 1718 | 2527 | 2546 |

The `windows-msvc-debug` and `linux-gcc-debug` jobs show the same thing: one factor per run, the same for every entry. Oversubscription would hurt the entries that start threads more than the single-threaded ones, but they all scale alike: the library tests' block-parallel stages, and the CLI tests, whose `worldgen-cli` defaults to every core.

### Runners differ

Each job's "Set up job" log records where it ran. The jobs above ran in 10 Azure regions (us-east-1, us-west-2, westus2, westus3, centralus, eastus2, eastus3, westcentralus, northcentralus, southcentralus). The image versions also differed (20261103.629 and 20261111.635), even within one run. The logs do not name the CPU model, but the region and the image already vary from job to job.

### Not oversubscription or memory

These tests were run locally on 4 of the 6 cores (`taskset -c 0-3`) to stand in for a 4-vCPU runner, at generator 0.11.0.

| Preset | `-j 4` | `-j 3` | `-j 2` | User CPU |
| --- | --- | --- | --- | --- |
| `linux-gcc-debug` | 288 s | 317 s (+10%) | 415 s (+44%) | 1108 to 1110 s in all three |
| `linux-clang-asan` | 1306 s, then 1312 s | 1403 s (+7%) | not run | 5040 to 5049 s |

- The CPU time is the same at every `-j`, so four jobs on four CPUs waste nothing on contention.
- With fewer jobs the run only takes longer.
- On a machine of fixed speed, two `-j 4` asan runs differ by 0.5%.
- Peak memory under `linux-clang-asan -j 4`, sampled every 2 s as the summed resident size of all test processes, was 1.99 GB in total. The largest single process was 0.69 GB. A 16 GB runner is far from memory pressure.

No job log shows an out-of-memory kill or a sanitizer allocation failure.

## Check run

Run `44492829269` on this branch (no code change) passed on every job. It landed on a slow asan runner and a fast msvc-debug runner, and the ratio still held:

| Job | Total | Build | Test | Hashes | Test / hashes |
| --- | --- | --- | --- | --- | --- |
| `linux-clang-asan` | 55.2 | 3.3 | 42.5 | 9.2 | 4.6 |
| `windows-msvc-debug` | 14.8 | 1.9 | 10.2 | 2.2 | 4.7 |
| `linux-gcc-debug` | 13.2 | 2.3 | 8.4 | 2.0 | 4.1 |

## What this means

- **Not ours to fix.** The variance is the speed of the hosted runner each job lands on.
- **No CI change.** Fewer jobs would lengthen every run by 7 to 44% and leave the factor of two in place. No `workflow_dispatch` experiment was needed, since the logs settle the question.
- **Timeouts.** The slowest job seen since the split is `linux-clang-asan` at 55.5 minutes, against a 180-minute job timeout. The slowest single entry is 1131 s, against 3600 s per entry. Both have room for a runner twice as slow again.
- **Reading job times.** When comparing two CI runs, divide the Test step by the hashes step of the same job. That figure is steady, about 3.5 to 5 since the split, and changes only when the tests change.
