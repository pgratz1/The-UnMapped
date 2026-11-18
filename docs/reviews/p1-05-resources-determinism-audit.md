# Determinism audit: P1-05 resources and habitability (generator 0.10.0 candidate, schema 4)

Auditor: determinism auditor, 2026-11-18. Change audited: `git diff e368844..d399761` (`main` at `e368844`, branch `p1/05-resources` at `d399761`). Spec: `docs/specs/p1-05-resources-habitability.md` as on the branch (second version, with the engineer's "As implemented" block in section 10). Line numbers are those of `d399761`.

Scope: `worldgen/src/stages/resources.cpp` and `worldgen/include/worldgen/stages/resources.hpp` (R1 to R7 and every seam); `geo::slope_tangent` (`worldgen/src/geo/terrain.cpp:161-169`, `geo/terrain.hpp`); `core/streams.hpp` (`geo.resources.ore`); `version.hpp`; `stages/stage.cpp` and `stage.hpp` (pipeline entry); `stages/biomes.cpp` and `biomes.hpp` (`BiomeDef::forage`); `worldgen/src/data/kinds.cpp` and `data/schema.hpp` (the biome field and `worldgen.resources`); `worldgen/src/inspect/geography.cpp`, `report.cpp`, `render.cpp` and their headers (the eight report keys, the corrupt-layer checks, palette `score`); `tools/worldgen-cli/cli.cpp` (help text); the twelve biome files and `worldgen/resources.toml` in `game/data/` and both fixture roots. The functions the stage reuses (`chamfer_distance`, `height_above_drainage`, `water_surface`, `cell_slope_s2`, `slope_s2_limit`, `fbm_m`/`fbm2`, `for_each_row_block`) were read where the stage's bounds depend on them; they are unchanged by the diff. Test files were read only where they define data or golden values.

**Verdict: pass.** No blocker, no should-fix. On every input `generate` can produce, the seven new layers are functions of the seed, N, the stored input layers, the biome values and the tuning, independent of compiler, thread count and row-block partition. Layers 1 to 11 and the tables `pass`, `biome` and `climate` are byte-identical to 0.9.0; only `meta` (version, stages, content hash) and the `layer` table (seven new rows) differ. Every arithmetic step is checked `Fx`, `int64` or `U128`, and every overflow bound in the code comments and in spec section 7 was checked against the types used, by reading and by running the seams at their extremes under ASan and UBSan. Nine notes (section 1); two of them are spec-text bounds that are understated while the conclusion holds (N8), and one is a seam that skips the tuning check its header promises (N6). No lint rule added (section 5).

Classification: **blocker** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** is fragile or breaks a stated contract and needs action before merge or soon after; **note** is for the record.

## 1. Findings

### Blockers

None.

### Should-fix

None.

### Notes

**N1. The walk limits are exact, and the step comparison is ΔS² against the high word of a U128.** `resources.cpp:179-188` (`walk_limits`), `:503-506` (walkable), `:528-531` (link).
- Walkable: `s2 ≤ slope_s2_limit(walk_slope_max)`. `slope_s2_limit` (`terrain.cpp:144-159`) returns `hi` of (320 × raw)² from `mul_u64_wide`. S2 × 2^64 ≤ X ⟺ S2 ≤ X.hi holds for every integer S2 ≥ 0 (S2 = X.hi + 1 gives S2 × 2^64 > X.hi × 2^64 + X.lo). This is the spec's U128 test.
- Link: `scaled = 160 × raw`. The seam and the tuning check both bound raw to [0.01, 10] × 2^32, so scaled < 1600 × 2^32 < 2^43 and the `uint64` multiply cannot wrap. `sq = mul_u64_wide(scaled, scaled)` < 2^86, so `sq + sq` (U128 addition modulo 2^128) < 2^87 and does not wrap. `step_orth = sq.hi` and `step_diag = (2 sq).hi`, both < 2^23. The cell test is `ds² ≤ step` with `ds = surface[c] − surface[m]` in `int64`. The seam bounds surface to [−65536, 131072] (`:498-500`), so |ds| ≤ 196608 < 2^18 and ds² < 2^36 in `int64`. The stage's surface is elevation (i16) + water_depth (u16), so [−32768, 98302], inside that bound. The same high-word equivalence makes the test exactly the spec's (ΔS)² × 2^64 ≤ w (160 raw)².
- Checked against an `unsigned __int128` evaluation of the spec's inequalities: 2,006 values of `walk_slope_max` (0.01, the shipped 3007336101 and its predecessor, ONE, 10, and 2,000 random raws in [0.01, 10]). For each value, both weights, ds at the threshold −1, 0, +1, +2, both signs, anchored at both ends of the surface range, through `walk_areas` on a 2 × 1 grid (orthogonal) and on a 2 × 2 grid with ocean on the other diagonal (diagonal); also S2 at the walkable limit ±1. 38,114 checks, 0 mismatches, also under ASan/UBSan. Surface −65537 and 131073 are fatal with the stated message. −65536 next to 131072 is accepted, and with `walk_slope_max` 10 the two cells are not linked, which is correct (ΔS² = 3.9 × 10^10 > (1600 × 2^32)² / 2^64 = 2,560,000).
- An S2 of `INT64_MAX` in the seam is only compared, never used in arithmetic, and the cell is not walkable.

**N2. The walk-area labelling is a pure function of the partition, and the 65535 fatal case is exact.** `resources.cpp:507-558`.
- Seeds are visited in index order, each component by FIFO BFS (`queue` with a moving `head`), neighbours in `geo::NEIGHBOURS_8` order. An edge is followed only when both cells are walkable and linked. `linked` is symmetric (ds² is the same both ways, and `is_diagonal` is symmetric), so the BFS components are exactly the components of the link graph, whatever order the BFS visits them in. Label k is created when the smallest-index cell of its component is reached, so labels increase with each component's smallest index. Kept components are renumbered in label order (`:542-550`), and the ids are dense and ordered as R5 requires. No container's order depends on hashing or on pointers; the labels, sizes and ids are `uint32` vectors indexed by cell or label.
- Widths: n ≤ 2^24 (`check_grid_array`), so `uint32` labels and sizes cannot wrap. `id_of[k]` is ≤ 65535 by the fatal check, before it is cast to `uint16` (`:545-548, 555`).
- Reference: 400 random grids from 1 × 1 to 150 × 150 (2.2 M cells; random water including rivers and ocean, random elevation amplitude, `walk_slope_max` random in [0.01, ~4.7], walk_min_cells 1 to 20). An independent union-find with the spec's link test in `__int128`, ids assigned by smallest index: 0 mismatches under ASan/UBSan.
- 65535 / 65536: a 1 × 131069 strip of isolated walkable cells gives count 65535 and id 65535 on the last cell. A 1 × 131071 strip is fatal with `resources: more than 65535 walk areas`. With the shipped tuning (977 cells) a 2048 map holds at most 4293 kept areas, as the spec says. At the schema minimum `walk_min_area_m2` = 16384 (64 cells), 2^22 / 64 = 65536 areas would need a perfect tiling with no unwalkable or unlinked cell to spare. The fatal case is therefore reachable only in theory and only with edited data. It is deterministic either way.

**N3. R3 to R4 and R6 run in G6 row blocks; every block writes only its own cells and reads only buffers no block writes.** `resources.cpp:577-584` (S2), `:612-638` (R3 rest and R4), `:644-657` (R6).
- Writes: `s2[i]`, `tangent[i]` and `out.<score>[i]` for the cells of the block's rows only. Every output is `std::vector<std::uint8_t>`, `std::vector<std::int64_t>` or `std::vector<Fx>`, so distinct elements are distinct memory locations. There is no `vector<bool>`, which the `packed-bool` lint rule would catch.
- Reads: `in.*`, `df`, `dg`, `hand`, `values`, `compiled` and `tuning`, all written before the parallel section. `for_each_row_block` joins before it returns. R6 reads `out.fertility`, `out.fresh_water`, `out.forage`, `out.timber`, `out.ore` and `tangent` written by R4, and `out.walk_area` written by R5. All three were completed by earlier calls (sequential in this function), and R6 reads only cell i's values when it writes cell i.
- `tangent` is written only on land cells, and R6 reads it only on land cells (`:649`); lake and ocean cells keep the zero-initialized `Fx{}` and are never read. Every output vector is `assign`ed to 0 before the section, so no uninitialized byte reaches a layer. Lake cells set only `fresh_water` = 255.
- The noise is location-keyed: `fbm_m(k_ore, X, Y, ore_noise)` with K_ore computed once from the seed. It is evaluated on land cells only, so the partition cannot change which cells consume it, and nothing is drawn sequentially.
- ThreadSanitizer (own `linux-clang-release` build with `-fsanitize=thread`), 512 seed 11 at 5 threads: no report, and the file hash equals the release builds'. Hand-built multi-block grids (30 grids, H 60 to 360, random biomes, rivers of every class, rain, and an ocean column so that G7 succeeds) through `resources_layers` at threads 1, 2, 3, 7 and 256: identical layers, under ASan/UBSan.

**N4. `geo::slope_tangent` is exact.** `terrain.cpp:161-169`. `U128{S2, 0}` is S2 × 2^64 (the field order of `U128` is `{hi, lo}`, `wide.hpp:14-18`). S2 < 2^34 gives a product below 2^98 and a floor root (`isqrt_u128`, P0-01 9.1) below 2^49, so `from_raw` cannot hit INT64_MIN. `div_int(…, 320)` is P0-01's RHA division, exactly G8's tan = div_int(from_raw(isqrt_u128(S2 × 2^64)), 320). The guard is fatal for S2 < 0 and S2 ≥ 2^34; on stored i16 elevation S2 < 2^33 (|gx|, |gy| ≤ 2 × 32767). Checked against an independent floor square root in `__int128` with explicit RHA: 200,011 values, including 0, 1, the six worked-example values, 50204/50205, 2^33 − 1, 2^34 − 1 and 200,000 random values below 2^34. 0 mismatches. 2^34 and −1 are fatal.

**N5. The two chamfers and G7 are sequential and unchanged.** `resources.cpp:574-576` (`water_surface`, `height_above_drainage` with k = 1), `:586-601` (sources, then `chamfer_metres` twice).
- `chamfer_distance` is the P1-02 H7 / E5.3 function (two in-place passes, D4) and is called outside any parallel section. `chamfer_metres` (`:396-404`) maps d ≤ 2^40 + 7 to `min(32767, (32 d + 5) / 10)`. The product is < 2^46, the division is of non-negative values (no signed rounding question), and INF saturates to 32767.
- The source masks follow R3 exactly. df uses water ∈ {1, 2}; dg uses water ∈ {2, 3} or a river cell with `river ≥ fish_river_class_min`, compared in `int64`.
- G7 is the shared FIFO implementation (`geo/drainage.hpp:39-58`, D5). It is fatal on a negative hand, so every hand reaching R4 and R6 is ≥ 0. On hand-built input it is ≤ 98302 + 32768 = 131070, and every use of it (`from_ratio(hand, 10)`, `from_int(hand)`) is in range. The fatal messages carry the stage prefix (`:576`).

**N6. One new stream, used only as a noise key; the ore seam does not run the tuning check.** `streams.hpp:70, 105`; `resources.cpp:451-454, 613, 634`.
- `geo.resources.ore` is registered once, appended to `all_stream_names`, and used only through `stream_key(seed, …)` into `fbm_m`. There is no `Rng` anywhere in the stage. No other key changes, so no other stage's randomness moves (confirmed: layers 1 to 11 are unchanged, section 4).
- `cell_centre_m` computes `CELL_M × k + 8` in `int64` (`CELL_M` is `int64`, `grid.hpp:20`), so even k = INT32_MAX cannot overflow. `from_int` is then fatal outside ±(2^31 − 1). Observed: x = INT32_MAX and INT32_MIN are fatal, and |x| = 2^27 is accepted.
- `resource_ore_noise` does not call `check_resource_tuning`, although the header comment on that function (`resources.hpp:109-114`) says "every seam that takes a tuning calls it first". Observed: `ore_noise.octaves` 16 and `wavelength_m` 1, both outside the schema (1..10, 64..65536), are accepted by the seam. They stay deterministic: `fbm2` is fatal outside 1..16 octaves and for a non-positive frequency or a gain outside (0, ONE], and every `Fx` step is checked. The stage path checks the tuning first (`:565`), so no world is affected. The engineer may add the call to the seam or narrow the header sentence. This is a contract wording issue, not an output risk.

**N7. Tuning validation and data match the spec.** `kinds.cpp:465-519` (schema, 34 fields), `:80-106` (the three check helpers), `:276` (biome `forage`), `:529` (registration); `resources.cpp:323-373` (`check_resource_tuning`).
- The 34 fields appear in the declaration order of spec 4.2, with the stated types and bounds. That order fixes the canonical encoding and hence the content hash. The eight named checks are registered in the spec's order with the spec's messages. `hab_weights_positive` sums four values ≤ 1000 in `int64`.
- `check_resource_tuning` repeats every range and every named check for hand-built tuning, in the same order. The `ore_noise` bounds equal `fbm_m_schema` (`noise_schema.cpp:45-46`, octaves 1..10, gain 0.0625..1).
- The biome field `forage` is `fixed("0", "1")`, required, declared after `timber` and before `region_noun` (spec 4.1). `biome_from` reads it (`biomes.cpp:86`). The biome table writer does not store it (no `forage` or `timber` column in `db/` or the biome table), so the `biome` table bytes are unchanged (section 4).
- The twelve shipped values and steppe's `fertility` "0.7" equal spec 4.1. `game/data/biomes/` and `worldgen/resources.toml` are identical to both fixture roots (`diff -r`).
- `biome_values` (`:205-223`) maps codes 0..255 into fixed arrays. Data codes are unique (the P1-04 kind checks), so the order of `definitions.biomes` cannot change a value.

**N8. Spec section 7's bound text understates two ranges; the conclusions hold.**
- "|ΔS| ≤ 65535 + 32767, (ΔS)² × 2^64 < 2^98". S = elevation + water_depth lies in [−32768, 98302], and a walkable cell is not required to have positive elevation (the stage does not check it on hand-built input). Two linked cells can therefore differ by up to 131070 = 98302 + 32768, not 98302. The conclusion still holds: 131070² < 2^34, so (ΔS)² × 2^64 < 2^98. The code's comment (`:496-497`) and guard (`:498`) use the right range. Suggested wording: "|ΔS| ≤ 98302 + 32768".
- "`from_ratio(hand, 10)` with hand ≤ 30000". 30000 holds on hydrology output (`drainage.hpp:51`). On hand-built `resources_layers` input, hand reaches 131070 (N5) and is still in range. The cell seams `fertility_cell` and `habitability_cell` refuse hand > 65535 (`:421, :467`). So invariant 4's recheck through the seam cannot reproduce a hand-built stage cell with hand in 65536..131070, a gap that only fixtures could expose. On real worlds hand ≤ 30000, and nothing changes.
- Every other bound of section 7 matches the code: chamfer < 2^14 at 2048 (7 × 2047 = 14329); `from_int(rainfall)` < 2^49; `from_int(dg)` ≤ 32767 × 2^32; al ≤ 5 (alluvial_bonus ≤ 4); gb + fish ≤ 2; em ≤ 3276.7; sc ≤ 1,020,000 < 2^20 and b ≤ ONE; S2 × 2^64 < 2^97; 2 (160 raw)² < 2^87 (the spec's 2^96 is looser but true); flood limit ≤ 1000 × 2^32. Every ramp's lower and upper ends are ordered by a named check, and the ramp evaluates `x − a` only when a < x < b, so a seam noise of `Fx::MAX` or `Fx::MIN` returns ONE or 0 without overflow (observed).

**N9. Report and render text are integer-only and order-defined.** `geography.cpp:1311-1414`, `:945-958`; `report.cpp:148-157, 249-261, 280-282`; `render.cpp:46, 70-75, 107, 125, 201-203, 519-531`.
- The percentiles are taken over fixed 256-bin histograms with the shared `percentile_rank` rule. The shares go through `ratio4`. Walk-area counts use a 65536-entry vector indexed by id, so the "most common" value is a maximum of counts and ties cannot change it. `walk_areas` prints `largest_id` (a `size_t` ≤ 65535) through `format_u64`. Nothing is locale- or stream-formatted.
- The corrupt-layer checks run whenever a resources layer exists, before `[geography]`, in `RESOURCE_LAYER_NAMES` order.
- `score_colour` uses the shared `ramp_unchecked` with `floor_div`: t = 100 gives R = 59 + floor(−1808 / 128) = 44, as in spec 9. The per-value colour table is built once and indexed by the u8 value.
- `report` on the ASan build (512 seed 3) printed the eight keys in the spec's order, and the `world-hashes.sh` reports are identical between GCC and Clang (section 4).

## 2. What was checked against the hard rules

| Rule | Result |
| --- | --- |
| Integer and fixed-point only in the output path | yes: `Fx` (checked, RHA), `int64`, `U128`; no `float`, `double`, `<cmath>` or `<random>` in any changed library or CLI file (lint and reading) |
| No standard transcendentals or distributions | none; the only root is `isqrt_u128`; the only randomness is `fbm_m` on a location key |
| One generator, streams by name | one new name, `geo.resources.ore`, a noise key only; no `Rng` |
| No unordered iteration, pointer order, thread timing, clock | none in the diff (`unordered`, `std::sort`, `std::hash` absent); the BFS is index-ordered FIFO and the result depends only on the partition; the row blocks are fixed by H, each self-contained |
| No fast-math | no CMake or flag change in the diff; the lint scans the CMake files |
| SQLite pinned, rows in a defined order, no timestamps | seven layers written in section 3 order through the existing writer (codec 1); page size 4096 and `user_version` 4 observed; no table or column added |
| UB and implementation-defined behaviour | none found: every narrowing cast is bounded (`uint16` ids after the 65535 check; `uint8` scores after `q255`'s clamp; `int64 → uint64` of non-negative values); no signed shift of a negative value; no signed division of a negative value except in checked `Fx` RHA routines; ASan/UBSan clean at the extremes |

## 3. Lint

On `d399761`: `python3 -B tools/lint/determinism_lint.py --self-test` passed (13 snippet files, 0 failures); `python3 -B tools/lint/determinism_lint.py` scanned 122 C++ files and 11 CMake files with 0 findings.

## 4. What was run

All runs were on Linux (6 cores), in the audit worktree at `d399761`. Builds are out of tree under `~/.cache/the-unmapped/build/p1-05-audit/`, worlds under `~/.cache/the-unmapped/out/p1-05-audit/`. The 0.9.0 reference is `main` at `e368844`, built with `linux-gcc-release` from an export in the session's scratch space (`worldgen-cli 0.9.0 schema 4`). Its 512/42 file hash `433383b1…2cee` equals the one recorded in the 0.9.0 audit, so the reference is the merged 0.9.0.

| Run | Result |
| --- | --- |
| Thread sweep, `linux-gcc-release` and `linux-clang-release`, threads 1, 2, 3, 5 and 8: 512 seeds 3, 11 and 42; 1024 seeds 5 and 23 (50 files; seed 7 was the engineer's) | one SHA-256 per case across both compilers and all thread counts: 512/3 `526ec7ce…`, 512/11 `8793d7c2…`, 512/42 `e3d4ba60…`, 1024/5 `2f88fe48…`, 1024/23 `848bf8e5…` |
| 2048 seed 3, GCC release, threads 1 and 8 | both `4145bae2…` (119.4 s and 49.2 s, 390 MB peak) |
| `tools/ci/world-hashes.sh local linux-gcc-release linux-clang-release` | all 12 cases identical across the two presets |
| `world-hashes.sh expect linux-gcc-release` | the eight P0 cases match the re-recorded `p0-worlds.txt`; the four geo cases differ from `p1-worlds.txt`, which still holds the 0.9.0 values (expected: the validation specialist re-records it after the comparison across presets) |
| `worldgen-tests -tc="*golden*"` (GCC release) | 21 cases passed, among them the P1-01 to P1-04 golden vectors, which are unchanged by the diff |
| 0.9.0 against 0.10.0, 512 seeds 42 and 9 (`hash --detail`; for seed 42 also a per-row comparison of table `layer`) | layers 1 to 11 (`elevation` … `biome`) byte-identical, with the same ids, types and blobs; tables `pass`, `biome` and `climate` identical; `meta` differs only in `generator_version`, `stages` and `content_hash`; `layer` adds ids 12 to 18 in section 3 order |
| ThreadSanitizer build (`linux-clang-release` + `-fsanitize=thread`), 512 seed 11, 5 threads | no report; same file hash |
| `linux-clang-asan` (ASan + UBSan): 512 seed 3 at 5 threads, then `report` and `render` of `habitability` (score) and `walk_area` (ids) | no report; same file hash; report keys as in section 9 |
| Seam probes against the `linux-clang-asan` library: link and walkable limits (N1), walk partition against union-find (N2), 65535/65536 areas (N2), `slope_tangent` (N4), the per-cell seams at their extremes (alluvial 4, hand 65535, df `INT64_MAX`, noise `Fx::MAX`/`MIN`, all weights 1000, S2 2^34 − 1), `resource_ore_noise` at INT32 limits, `resources_layers` on an all-river grid with elevation ±32767/−32768, water_depth 65535 and rain 65535 at threads 1 and 5, and on the multi-block grids of N3 | 0 mismatches, no sanitizer report; outcomes as stated in N1 to N6 |

Not run: MSVC and Windows (not available here; CI's `compare-hashes` covers them); the resume test of 10.3 (covered by the test writer's suite). The probes are in the auditor's scratch space, not in the repository.

## 5. Lint rules

None added. Nothing in this change is a pattern a banned-construct rule could catch. The writes from parallel sections go to byte and `int64` vectors; `vector<bool>`, the one container whose distinct elements share memory, is already caught by `packed-bool`. The bounds of N1, N5 and N8 depend on value ranges, not on syntax. Every other construct in the change is already covered: `unordered`, `std::sort`, `std::hash`, floating point, `<random>`, `<cmath>`, clock, environment, directory iteration, number formatting and flags.
