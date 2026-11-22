# P4-02 layer catalogue: determinism audit

Change: `git diff origin/main...origin/p4/02-impl` (library part), reviewed on `origin/p4/02-int` (ec6a76b). Spec: `docs/specs/p4-02-layer-catalogue.md`. Generator stays 0.20.0, schema 7. This was a reading audit. No suites were rerun, because they were already green on gcc-release, clang-debug and asan.

**Verdict: pass with advisories.** There are no blocking findings.

## What was checked

- `worldgen/src/inspect/render.cpp` `registered_palette`: the removed if-chain compared entry by entry with the `palette` column of `CATALOGUE` (`layer_catalogue.cpp:37-113`). All 19 names give the same palette as before. Those are elevation hypsometric, water water, water_depth gray (before, the default branch), flow_dir direction, flow_accum log2, river river, lake ids, coast_dist signed_distance, temperature temperature, rainfall rainfall, biome biome, the six scores score, and walk_area and region ids. Any other name still gives gray. The names match byte for byte and lookup is exact-case, as before. Render pixels are unchanged.
- `add_signed_rows` (`layer_catalogue.cpp:223-248`): `SignedScale::m` is uint64 and at most 2^63. `neg_m` takes INT64_MIN when m > INT64_MAX, otherwise it negates a value of at most INT64_MAX. `neg_half_m` negates m/2, which is at most 2^62. `p` is at most INT64_MAX by the construction in `signed_scale` (`render.cpp:275`), so the cast is exact. `p / 2` is fine. None of these overflow.
- `magnitude`, `tenths` and `hundredths` (`:127-141`): unsigned negation, exact for INT64_MIN. `upstream_km2` (`:146-150`): with n = 10000q + r, 256q is at most about 4.7e17 and 256r + 5000 is below 2.6e6. The `256U` operands are promoted to uint64. There is no overflow for any uint64 n, and the identity is exact.
- `add_gray_rows` (`:250-271`): the signed range is formed as a uint64 difference, which wraps in a defined way and is exact. `gray_level(r, r)` handles r = 0. A u64 value above INT64_MAX is printed raw.
- `cell_value` (`:337-359`): the index is computed in uint64 and bounds-checked against `values.size()`. A u64 value above INT64_MAX is refused.
- `region_name` (`:525-545`): the search is on [lo, hi) with `mid = lo + (hi - lo) / 2`, so indices stay in bounds and there is no overflow. An empty span returns empty. Unordered records end within bounds. There is no undefined behaviour.
- `RegionRecord` (`geography.hpp:494`, `geography.cpp:1428-1466`): the only construction in the library is the field-by-field one in `read_region_rows`. The tests also build it field by field (`inspect_layer_catalogue_test.cpp:1011-1016`), and nothing else builds it positionally (searched `worldgen/`, `tools/`, `gdext/`). The column types are checked before the reads: column 2 is text, the others are integer. The rows come from `WorldReader::table_rows`, which is ordered by primary key `COLLATE BINARY` (`reader.cpp:738-746`). There is no new SQL. `region_lines` and the `[geography]` keys read only id, cells and walk_cells, so the `report` and `hash` text is unchanged.
- Formatting: all decimals come from `db::format_i64` / `format_u64`. There are no streams, no `printf`, no locale and no `<cctype>`. There is no `float` or `double`, no `<cmath>`, and no unordered container. The catalogue is a fixed `constexpr std::array` searched linearly. No pointer values are used as data. There is no time, environment or threading.
- MSVC `/W4`: no C4996 functions. Every narrowing is an explicit `static_cast`, and `record.colour & 0xFFFFFF` is cast to uint32 explicitly. The `switch` statements over enum classes cover every enumerator and have a return after them.
- CMake: one source file was added (`worldgen/CMakeLists.txt`). There are no flag changes.
- Lint: `python3 -B tools/lint/determinism_lint.py --self-test` passed (18 snippet files, 0 failures). The scan covered 169 C++ files and 13 CMake files and found 0 findings. There are no new suppressions.

## Findings

### A1 (advisory): the degree sign is raw UTF-8 in the source, not an escape

`worldgen/src/inspect/layer_catalogue.cpp:74, 76, 394, 422` (and a comment in `layer_catalogue.hpp:71`) contain the bytes C2 B0 directly. They are not written as an escape, which is what the spec's auditor point (c) (section 8) and the engineer's note describe. These are the only non-ASCII bytes in `worldgen/src`, `worldgen/include` and `tools/worldgen-cli`.

The output is currently correct. MSVC builds with `/utf-8` (`cmake/UnmappedCompilerFlags.cmake:47`), and GCC and Clang read and execute in UTF-8 regardless of locale. I checked GCC and Clang under `LC_ALL=C`, `POSIX` and `en_US.ISO-8859-1`, and all give `C2 B0`. The bytes only change under an explicit `-finput-charset`, as with `-finput-charset=ISO-8859-1`, which gives `C3 82 C2 B0`. They would also change on a compiler or flag set without `/utf-8`, or if an editor re-saved the file in another encoding. The result would be different legend and value text on another platform. Render pixels and world bytes would not change.

Suggested fix: write it as `"\xC2\xB0"`. Watch the hex trap: `"\xC2\xB0C"` is a single escape `\xB0C`, so split the literal, for example `"\xC2\xB0" "C"`, or use a named `constexpr std::string_view DEGREE = "\xC2\xB0";`. Also, spec line 67 reads "`°` is written `°` in the source", which looks like the escape was lost when the spec was rendered. The spec writer should restore it.

Lint: a rule `non-ascii-literal` (a raw byte of 0x80 or more inside a string or character literal, in the library and the CLI) is worth adding. It is not added in this audit because it would fail the scan on the current tree. I will add it with the engineer's fix, or on request.

### A2 (advisory, not a determinism issue): duplicate signed legend rows on a narrow range

`layer_catalogue.cpp:241`: for M = 1, the list −M, −M/2, −1, 0, P/2, P gives −1 twice and 0 two or three times, for example "0" again when P = 1. This is deterministic and follows the spec text (no de-duplication is stated). It is noted only in case the viewer should show each value once.

## Verdict

Pass with advisories (A1, A2). There are no blocking findings.
