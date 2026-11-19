# Determinism audit: P1-06 regions (generator 0.11.0, schema 5)

Auditor: determinism auditor, 2026-11-18. Change audited: `git diff main...p1/06-regions` without `docs/` (`main` at `4caea81`, branch `p1/06-regions` at `167c9ac`). Spec: `docs/specs/p1-06-regions.md` as on the branch (second version), with P0-02 amendment 17, P0-03 amendment 17 and P0-04 amendment 18. Line numbers are those of `167c9ac`.

Scope: `worldgen/src/stages/regions.cpp` and `worldgen/include/worldgen/stages/regions.hpp` (RG1 to RG14 and every seam); `stages/stage.cpp`, `stage.hpp` and `pipeline.hpp` (`StageContext::passes()`, the pipeline's pass list, the three new write calls); `db/writer.cpp`, `writer.hpp`, `writer_state.hpp` and `db/schema.hpp` (the three table writers, rollback state, DDL); `core/streams.hpp` (two new names); `version.hpp`; `data/schema.cpp`, `schema.hpp` and `data/kinds.cpp` (the `word` grammar, `worldgen.regions`, `region_names`); `inspect/geography.cpp`, `report.cpp`, `render.cpp` and their headers (the eight report keys, the corrupt-data checks, palette `ids` for `region`, the composite `regions`); `tools/worldgen-cli/cli.cpp` (render of the composite, help text); `gdext/src/world_file.cpp` (the composite is not offered yet); `game/data/worldgen/regions.toml` and both fixture copies. The functions the stage reuses (`water_surface`, `height_above_drainage`, `label_components`, `chamfer_metres`, `fbm_m`, `hash_at`, `mul_div`, `for_each_row_block`, `percentile_rank`, `table_rows`) were read where the stage's bounds or orders depend on them; they are unchanged by the diff. Test files were read only where they define data.

**Verdict: pass.** No blocker, no should-fix. On every input `generate` can produce, the layer `region` and the tables `region`, `region_border` and `region_biome` are functions of the seed, N, the stored inputs, table `pass`, the biome definitions and the tuning, independent of compiler, standard library, thread count and row-block partition. Layers 1 to 18 and the tables `pass`, `biome` and `climate` are byte-identical to 0.10.0; only `meta` (version, schema version, stages, content hash), the `layer` table (one new row, id 19) and the schema text differ. Every order the stage depends on is a strict total order on distinct elements or an ordered container iterated in key order, and every arithmetic step is checked `Fx`, `int64` or `uint64` within the bounds of spec section 7. The four new lint suppressions state true reasons. Seven notes (section 1). One lint rule added, `pointer-order` (section 5).

Classification: **blocker** can change output between platforms, compilers, standard libraries or thread counts; **should-fix** is fragile or breaks a stated contract and needs action before merge or soon after; **note** is for the record.

## 1. Findings

### Blockers

None.

### Should-fix

None.

### Notes

**N1. The four suppressions are true; the sort and the heap are fully determined.**
- `regions.cpp:352-353`, `std::sort(sorted, region_site_better)`. `region_site_better` (`:244-249`) is lexicographic on (habitability descending, S ascending, hash descending, cell ascending). It is irreflexive (`a.cell < a.cell` is false), transitive and total on distinct cells, and `region_seeds` refuses a cell listed twice before sorting (`:341-350`). So no two candidates compare equal, and every conforming `std::sort` (introsort in libstdc++, pdqsort-like in libc++, MSVC's introsort) yields the one sorted order. Candidates come from distinct tiles (`:262-285`), so on the stage path the duplicate check cannot fire.
- `regions.cpp:523-524`, `std::priority_queue<FloodEntry, …, FloodLater>`. `FloodLater` (`:476-480`) orders by (k, q); q is `seq`, incremented on every push (`:536-537, 568-569`), so no two entries are equal and the pop sequence is the unique ascending order of (k, q) whatever the heap layout. The relaxation is strict (`nk < out.cost[m]`, `:565`), over `NEIGHBOURS_8` in fixed order, and `done` is set on pop, so owner and cost are the spec's RG7 exactly. The comment "pop order is the same with any heap implementation" is correct.
- `geography.cpp:1533-1534` and `:1554-1555`, two `std::sort` of `std::vector<std::int64_t>` with the default `<`. Equal integers are indistinguishable, so the sorted vector is unique. The first feeds `percentile_rank`; the second only `std::binary_search`.

**N2. No other order in the change depends on hashing, pointers or completion.**
- Ordered containers only: `std::map<std::uint32_t, PairCounts>` per region (RG8, `:606`), iterated in ascending neighbour number, which is the stated tie-break; `std::map<std::uint32_t, std::uint16_t>` (RG9, `:690`) and `std::map<std::string, std::int64_t>` (RG11, `:801`) are used only for lookup; `std::map<std::pair<std::uint16_t, std::uint16_t>, RegionBorderRow>` (RG12, `:834`) is iterated in (a, b) order to make the rows; `std::map<std::pair<int64, int64>, size_t>` (`:927`) is lookup only. No unordered container, `std::hash`, or pointer-keyed container anywhere in the diff (lint, and the new `pointer-order` rule, section 5).
- `by_code` (`:775`) and `words` (`:178`) are arrays of pointers indexed by code or position, never compared.
- RG8 (`:635-673`): the small region is the first k in ascending number with the smallest cells (`<`, so ties keep the smaller number); the target is the first neighbour in map order with the largest (nr, n) (strict `>`, ties keep the smaller number). The merge loop erases `r` from other regions' maps and adds into `nb[t]`, `nb[u]`; neither is `nb[r]`, which is being iterated, so no iterator is invalidated. Map entries are only created with n ≥ 1, so `nb[k].empty()` is exactly "no neighbour".
- RG10 `walk_counts` (`:1077`) is in first-seen order, but `most_common` (`:1082-1092`) is a maximum with ties to the smaller value, so the order does not reach the output. Dominant biome (`:1258-1278`) and the p90 (`:1308-1319`) are scans of fixed 256-bin histograms in ascending code.
- RG12 passes (`:922-987`): passes in table order, crossings recorded in first-crossing order; `passes` is a count and `pass_id` a minimum, both order-free.

**N3. The pass list is the committed rows of this run, in id order.** `stage.cpp:72-79, 125-128`; `pipeline.hpp:38, 50`. `StageContext::write_pass` appends to `written_passes`, and `Pipeline::run` moves them into `passes_` only after `commit()`, in writing order. `StageWriter::write_pass` (`writer.cpp:447-450`) refuses any id but `next_pass_id`, so the list is in ascending id with ids 1, 2, …. There is no resume path (the library constructs its one `Pipeline` in `generate.cpp:109`, on a new world), so the regions stage always sees every pass the elevation stage wrote in this run, never a partial list. A stage that throws leaves `written_passes` unmoved, and the whole generation fails.

**N4. Threading: RG3 is the only parallel step and is partition-independent.** `regions.cpp:1114-1132`. Each block writes `out.weight[i]` (a `std::vector<Fx>`, distinct elements, no `vector<bool>`) for its own rows only, and reads `in.water`, `out.hand` and the tuning, all complete before the call. The noise is location-keyed (`fbm_m(K_cost, X, Y)`), evaluated on land cells only; nothing is drawn sequentially. The fatal check in the block (`:1125-1127`) has one message, so which block throws first cannot change the error text, and on G7 output it cannot fire (hand ≤ 30000 on hydrology output, ≤ 131070 on hand-built input, P1-05 audit N5). G7, the chamfer, the flood, the merge and every reduction run sequentially in index order. Observed: identical files for threads 1, 2, 3, 5 and 8 on both compilers (section 4).

**N5. Overflow bounds of spec section 7 hold against the types used.**
- RG3: nf ∈ [0.1, 1.9] ONE (amp ≤ 0.9); hf = 1 + from_ratio(hand, 10 hand_ref_m) ≤ 13108 ONE at hand 131070 and hand_ref_m 1; wt ≤ 24905 ONE < 2^15 ONE. Probe: `region_step_weight(ONE, 131070, …)` with amp 0.9 and hand_ref_m 1 gives raw 106967019495096 (24905 ONE), below the flood's 2^15 guard (`:500`), so the guard cannot fire on stage output.
- RG7: the largest step, a diagonal onto a lake of that weight with `lake_cost_factor` 10, river corners on both sides and `river_cost_m` 100000, is 57354030 (< 2^26), measured through `region_step_cost` under UBSan; 2^22 such steps < 2^48, far below INF = 2^62. `mul(wt, D160)` and `mul_int(lake_factor, hz)` are checked `Fx` (≤ 10 ONE × 2^23 < 2^59 raw). `onto4`, `onto8` (`:508-517`) precompute exactly `step_hz + rv`, and the corner term uses (m.x, c.y), (c.x, m.y) as the spec.
- RG5: (16 Δx)² + (16 Δy)² ≤ 2 × 2^30 and `seed_spacing_m`² ≤ 2^30 in `int64` (`:354, 361-363`). The segment test `rha(dx · j, m)` (`:321-322`) goes through `mul_div` (exact 128-bit, RHA, P0-01 5.4) with |dx · j| < 2^22; m ≥ 1 whenever the loop runs.
- RG10: elevation sums ≤ 2^22 × 32767 < 2^37, score sums < 2^30, Σx, Σy < 2^33 (`uint64`), the p90 rank 90 n < 2^29 in `uint64`, histogram counts ≤ 2^22 in `uint32`. Means are `mul_div(sum, 1, n)`, RHA for negative elevation sums as the spec says.
- RG9: ids are assigned by first occurrence in index order, and the 65536th distinct owner is fatal before the `uint16` cast (`:700-704`). RG11: `region_sector` is guarded at ±2^24 (`:714-717`), so dx² + dy² and 29 × a stay far inside `int64`; `roman` is guarded to [1, 100000] and k ≤ 65535.
- No signed shift; no signed division except through `mul_div`, `Fx` and `km2_text`, whose operands are non-negative (`v = mul_div(cells, 256, 10000)` with cells ≥ 1, `geography.cpp:1514-1518`). Every narrowing cast is bounded (`uint16` after the 65535 check, `uint8` dominant code ≤ 255, `int32` cell coordinates from a valid grid).

**N6. Database rows and text output are order-defined and integer-only.**
- `writer.cpp:559-625`: each writer refuses a key not strictly above the last one written to that table (id; (a, b) with 1 ≤ a < b; (region, biome) with region ≥ 1), and `StageWriter::~StageWriter` restores all three last keys on rollback (`:420-422`), recorded at `begin_stage` (`:712-714`). The stage writes in id, (a, b) and (region, biome) order (`regions.cpp:1434-1442`), which the writers enforce. Values are bound with `bind_int64` and `bind_text` with an explicit length (UTF-8; the names are ASCII by the word grammar). The three DDL strings equal spec 3.1 byte for byte (checked by script). Observed on 512 seed 3: page size 4096, `user_version` 5, `application_id` "UNMP", `auto_vacuum` 0; a plain scan of each new table returns its rows in key order; the table has no REAL and no BLOB.
- `report`: `region_lines` (`geography.cpp:1522-1573`) prints counts through `format_u64` and km² through `km2_text` (integer RHA, two digits, no locale or stream). The table checks read rows through `table_rows`, which orders by every primary-key column `COLLATE BINARY`, so the `lower_bound` in `read_region_border_rows` (`:1469`) sees ascending ids, and "first failing row in key order" is well defined. `hash --detail` lists `table region`, `table region_biome`, `table region_border` in byte order of the name, from the unchanged generic code.
- `render`: palette `ids` for `region`; the composite (`render.cpp:871-908`) is the shaded biome colours plus integer neighbour tests on the `u16` ids.
- Names (RG11, `regions.cpp:764-818`): built only from the tuning words, the biome `region_noun`s (checked by the word grammar, `is_valid_word`, `schema.cpp:51-61`), integer sectors and `roman`. `is_valid_word` compares `char` against ASCII ranges, and a byte ≥ 0x80 is rejected whether `char` is signed (negative, below `'A'`/`'a'`) or unsigned (above `'Z'`/`'z'`), so the grammar does not depend on `char` signedness. The `seen` map counts per base name in id order.

**N7. Small observations, no action needed.**
- With `name_central_frac` 0 (allowed by the schema) r_c = 0, and a seed exactly at the centroid has dx = dy = 0, which falls to `west` by the literal rule of RG11 2 (`29 · 0 ≤ 12 · 0`, dx > 0 false). Deterministic and as specified; noted only because it reads oddly.
- `biome_hist` and `hab_hist` (`regions.cpp:1162-1163`) are (R + 1) × 256 `uint32`, 67 MB each at R = 65535. Memory, not output; the shipped tuning gives about 200 regions at 2048.
- `gdext/src/world_file.cpp:222-224` withholds the composite `regions` from the viewer until the viewer engineer's change; no `worldgen` output depends on it.

## 2. What was checked against the hard rules

| Rule | Result |
| --- | --- |
| Integer and fixed-point only in the output path | yes: `Fx` (checked, RHA), `int64`, `uint64`, `mul_div`; no `float`, `double`, `<cmath>` or `<random>` in any changed library or CLI file (lint and reading) |
| No standard transcendentals or distributions | none; `SQRT2` is the P0-01 constant; the only randomness is `fbm_m` on K_cost and `hash_at` on K_seed |
| One generator, streams by name | two new names, `geo.regions.seed` and `geo.regions.cost`, appended to `all_stream_names`; no `Rng`; no other key changes (layers 1 to 18 unchanged, section 4) |
| No unordered iteration, pointer order, thread timing, clock | none; every order is a strict total order or an ordered map in key order (N1, N2); RG3 is partition-independent (N4) |
| No fast-math | no CMake or flag change in the diff except one source line in `worldgen/CMakeLists.txt`; the lint scans the CMake files |
| SQLite pinned, rows in a defined order, no timestamps | three tables written in strictly ascending key, enforced by the writer, with rollback (N6); no timestamp column; settings unchanged |
| UB and implementation-defined behaviour | none found (N5): no signed shift, no division of a negative value outside `mul_div`/`Fx`, bounded casts, `char` signedness irrelevant to the word grammar |

## 3. Lint

On `167c9ac`: `python3 -B tools/lint/determinism_lint.py --self-test` passed (13 snippet files, 0 failures); `python3 -B tools/lint/determinism_lint.py` scanned 124 C++ files and 13 CMake files with 0 findings. With the `pointer-order` rule of section 5 (this branch), the same scan of `167c9ac` gives 0 findings, and the self-test passes.

## 4. What was run

All runs on Linux (6 cores, shared with other agents' jobs), in a detached worktree of `167c9ac` (`.claude/worktrees/p1-06-audit-build`). Builds out of tree under `~/.cache/the-unmapped/build/p1-06-audit-build/`, worlds under `~/.cache/the-unmapped/out/p1-06-audit/`. The 0.10.0 reference is the `p1-05-val` `linux-gcc-release` build (`worldgen-cli 0.10.0 schema 4`) with `main`'s `game/data`; its 512/42 world hashes to `69598991…d92c`, the value in `main`'s `p1-worlds.txt`, so it is the merged 0.10.0.

| Run | Result |
| --- | --- |
| Thread sweep, `linux-gcc-release` and `linux-clang-release`, threads 1, 2, 3, 5 and 8: 512 seeds 3, 11 and 77; 1024 seeds 5 and 23 (50 files) | one SHA-256 per case across both compilers and all thread counts: 512/3 `479d55ad…`, 512/11 `d265fd17…`, 512/77 `4b8b01f7…`, 1024/5 `209ae75e…`, 1024/23 `c43ada49…` |
| 2048 seed 3, GCC release threads 1 against Clang release threads 6 | both `97185587…` (169 s and 95 s on the loaded machine); 202 regions, 545 borders, 130 river borders, 5 crossed by a pass |
| `tools/ci/world-hashes.sh local linux-gcc-release linux-clang-release` | all 12 cases identical across the two presets |
| `world-hashes.sh expect linux-gcc-release` | the eight P0 cases match the branch's re-recorded `p0-worlds.txt`; the four geo cases differ from `p1-worlds.txt`, which still holds the 0.10.0 values (expected: the validation specialist re-records it after the comparison across presets) |
| 0.10.0 against 0.11.0, 512 seed 3 (`hash --detail`, and `meta` and `layer` row by row) | layers 1 to 18 byte-identical; tables `biome`, `climate`, `pass` identical; `meta` differs only in `generator_version`, `schema_version`, `stages` and `content_hash`; `layer` rows 1 to 18 identical, row 19 `region` added |
| Seam probe against the `linux-gcc-release` library, compiled with UBSan | the RG3 and RG7 extremes of N5; `roman(100000)`, `roman(3999)` = `MMMCMXCIX`; `region_sector` at ±2047 and at its guard: no report, values as stated |

Not run: MSVC and Windows (not available here; CI's `compare-hashes` covers them); another standard library (libc++ is not installed here; N1's argument does not depend on the implementation); `linux-clang-asan` (the engineer's and test writer's runs cover it).

## 5. Lint rules

**Added: `pointer-order`** (`tools/lint/determinism_lint.py`, `README.md`, self-test lines in `selftest/bad_constructs.cpp`). It reports `std::map`, `std::set`, `std::multimap`, `std::multiset` (also `std::pmr::`) whose key type is a pointer, `std::less`, `std::greater`, `std::less_equal`, `std::greater_equal` and `std::compare_three_way` of a pointer type, and `std::uintptr_t` / `std::intptr_t`. P0-01 4 bans dependence on pointer values, but until now only by review: nothing in the lint caught a container ordered by address. This change does not have one (it uses arrays of pointers indexed by code and ordered maps keyed by integers, both near misses in the self-test), but the RG8 and RG12 maps are exactly the place where a later edit could key by `const BiomeDef*` or a row pointer and still compile, and such an order would differ between runs and allocators. The rule finds nothing in the current tree. A pointer-ordered comparator written as a lambda (`a < b` on two pointers) is still beyond a syntactic rule; `unstable-sort` and `heap-order` make every sort and heap a reviewed site, which covers the comparator.
