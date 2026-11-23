# Reference implementation of the deterministic core

`core_reference.py` is an independent Python 3 implementation of
`docs/specs/p0-01-deterministic-core.md`, written from the spec text alone
(spec section 13.8). It uses only the standard library and arbitrary-precision
integers; there is no floating point in it. It produces the golden vector file
`worldgen/tests/golden/core_vectors.txt`, which the C++ tests compare against
on every preset.

It covers every primitive with a defined numeric result: RHA rounding, `Fx`
conversions and arithmetic with overflow detection, `fade`, `lerp`, `mul_div`,
the plain-integer helpers, `U128` multiply and divide, decimal parse, canonical
text and format, `isqrt`, `sqrt`, `hypot`, SplitMix64, xoshiro256** and its
seeding, every sampling function with its draw count, `mix64`, `absorb`,
`hash_words`, byte-string packing, the domain tags, `stream_key`, `substream`,
`hash_at`, `hash_at3`, `phrase_seed`, the phrase scrub and `seed_from_phrase`, `noise2`,
`fbm2`, and SHA-256 (written out from FIPS 180-4; `hashlib` is used only to
cross-check it).

## Commands

Run from the repository root. `-B` stops Python writing `__pycache__` into the
tree.

```
python3 -B worldgen/tests/reference/core_reference.py check
python3 -B worldgen/tests/reference/core_reference.py generate
```

`check` runs every vector in spec sections 13.1 to 13.7, the checkable claims
in the prose, and self-tests, and exits 1 on any disagreement. `generate`
rewrites `worldgen/tests/golden/core_vectors.txt`; `generate -` writes the same
bytes to standard output, so `generate - | cmp - worldgen/tests/golden/core_vectors.txt`
confirms the committed file is current. Output is byte-for-byte reproducible.

Never regenerate the file to make a C++ test pass. A difference is either a
C++ defect or a reference defect, and is resolved against the spec first.

## Vector file format

ASCII, LF line endings, one vector per line, no blank lines. Lines starting
with `#` are comments. A vector line is

```
<kind> <arg> ... = <result> ...
```

with tokens separated by single spaces:

| Token | Meaning |
| --- | --- |
| `0x...` | unsigned 64-bit integer, lowercase hex (hashes, keys, words, RNG state, `uniform_u64` bounds, `U128` halves) |
| `-?[0-9]+` | decimal signed integer (`Fx` raw values, int64 values, small counts) |
| `true` / `false` | boolean |
| `[a,b,...]` | list of integer tokens, no spaces; `[]` is empty |
| `"..."` | byte string; bytes 0x21 to 0x7e other than `"` and `\` are literal, every other byte (space included) is `\xHH`, so a string token never holds a space |
| `FATAL` | the call raises `worldgen::FatalError` |
| `ERROR` | the call returns its error value (`parse_decimal`, `seed_from_phrase`) |

Sampling vectors start from `Rng::from_state(S0, S1, S2, S3)` given as four
`0x` tokens, and give the result followed by the number of `next()` calls
consumed. The header of the vector file lists every kind with its argument and
result types.

## Readings chosen where the spec is open

- 7.3 `stream_key`: one length word, the one that the byte-string encoding
  of 7.2 already holds: `[seed, len, packed words...]`.
- 7.3 / 13.5 phrases (scrubbed rule, generator 0.2.0). Three kinds:
  `hash.phrase_seed "bytes" = seed` hashes the bytes exactly as given (no
  scrub); `hash.phrase_scrub "bytes" = "scrubbed"` is the scrub alone, with no
  emptiness or length check, so its result may be `""` or longer than 256
  bytes; `hash.phrase_text "bytes" = seed|ERROR` is `seed_from_phrase`, that is
  `phrase_seed` of the scrubbed bytes, or `ERROR` when the scrubbed text is
  empty or longer than 256 bytes. Both phrase kinds are given for the same
  inputs (the 7.3 and 13.5 examples, every byte value 0x00 to 0xff between two
  letters, valid and invalid UTF-8, runs and edge separators, error cases,
  scrubbed lengths 255 to 257). `ERROR` does not say which of the two errors;
  the matching `hash.phrase_scrub` line does (`""` for empty, more than 256
  bytes for too long). Readings:
  - The steps run in the order written: `A`-`Z` are lower-cased first, then
    every byte that is not `a`-`z` or `0`-`9` becomes a space. So `@` (0x40),
    `[` (0x5b), `` ` `` (0x60), `{` (0x7b), 0x7f and all of 0x80 to 0xff are
    separators; nothing outside 0x41 to 0x5a is case-mapped (no Latin-1 or
    Unicode case folding, and no `& 0x7f` folding of high bytes).
  - Purely byte-wise: no UTF-8 decoding or validation, no Unicode
    normalization, so a precomposed `é` (two separator bytes) and `e` plus a
    combining accent (`e`, then separators) scrub differently: `caf noir` and
    `cafe noir`. A UTF-8 byte order mark and NUL are separators like any other.
  - Step 4 removes at most one space at each end, which after step 3 is all of
    them; a text that is a single space after step 3 becomes empty and is the
    empty error.
  - The 256-byte limit is on the final scrubbed text (after collapsing and
    trimming); the input length is not limited. Interior spaces count.
- 5.4 `lerp`: overflow of `mul(b - a, t)` is fatal, as `mul` defines, even
  though the table lists only `b - a` and the sum. `t` is not range-checked.
- 5.4 `mul_div`: arguments equal to INT64_MIN are accepted; only the result is
  range-checked.
- 5.4 `floor_div` / `floor_mod`: divisor 0 is fatal; `floor_div(INT64_MIN, -1)`
  is fatal (overflow); `floor_mod(INT64_MIN, -1)` is 0.
- 8.2 `fbm2`: the loop is followed literally, so the `f` and `amp` updates after
  the last octave are evaluated and can be fatal.
- 8.1 `noise2`: no argument check beyond what `Fx` guarantees.

## Independent check of P0 world files

`p0_world_check.py` checks a world file written by `worldgen-cli generate`
against `docs/specs/p0-02-world-db.md` and `docs/specs/p0-04-worldgen-cli.md`
without any of the project's C++. It reads the file with the standard-library
`sqlite3` module (read-only, immutable) and imports `core_reference.py` for
`stream_key`, `hash_at`, `fbm2`, `mul_div` and `seed_from_phrase`. `--phrase`
takes the argument bytes exactly as passed to `generate --seed-phrase` (any
bytes, not only UTF-8); the expected `seed_phrase` row is the scrubbed phrase
and the expected seed its `phrase_seed` (P0-01 7.3, P0-02 amendment 12). The
expected `generator_version` is the constant `GENERATOR_VERSION` at the top of
the script (`0.5.0`), overridable with `--generator-version`; the expected
`content_hash` is that of no data unless `--content-hash` gives another (worlds
written by `worldgen-cli` carry the hash of the loaded data, P0-03 amendment
17). It was used
to confirm the golden values in `worldgen/tests/golden/p0-worlds.txt` and the
`GOLDEN_RGB_*` constants of `worldgen/tests/inspect_render_test.cpp`.

```
python3 -B worldgen/tests/reference/p0_world_check.py check WORLD (--seed N | --phrase TEXT)
        [--map-size N] [--test-pattern] [--detail FILE] [--report FILE]
        [--generator-version V] [--content-hash HEX]
python3 -B worldgen/tests/reference/p0_world_check.py stats WORLD...
```

`check` verifies the SQLite header fields (P0-02 6.1), the `sqlite_schema`
rows and DDL text, the eight `meta` rows (P0-02 8), each layer blob (header,
strict codec decode, canonical re-encoding to the same bytes) and, with
`--test-pattern`, every cell of the three layers against P0-04 5.2. It
computes the `hash --detail` lines (P0-04 8) and the `report` text (P0-04 7),
compares them byte for byte with the files given by `--detail` and `--report`
(the CLI's output saved to a file), and prints the detail lines and the
SHA-256 of the gray `render_rgb` buffer of each layer (P0-04 6). Exit status 1
on any disagreement. A test-pattern world takes about half a minute, almost
all of it `fbm2`.

`stats` decodes the layers of each world and prints
`<path> <file sha256> <layer> <min> <max> <sum> <cells>` per layer, for seed
sweeps.

Example, from a build of any preset (worlds go under the build root, never the
repository):

```
CLI=$HOME/.cache/the-unmapped/build/The_UnMapped/linux-gcc-release/tools/worldgen-cli/worldgen-cli
O=$HOME/.cache/the-unmapped/out
$CLI generate --seed 42 --test-pattern --out $O/p42.db
$CLI hash --db $O/p42.db --detail > $O/p42.detail.txt
$CLI report --db $O/p42.db > $O/p42.report.txt
python3 -B worldgen/tests/reference/p0_world_check.py check $O/p42.db --seed 42 --test-pattern \
    --detail $O/p42.detail.txt --report $O/p42.report.txt
```

## Reference of the P1-01 elevation stage

`p1_elevation_reference.py` is an independent exact-integer implementation of
`docs/specs/p1-01-elevation.md` (design v16), written by the test writer from
the spec text and P1-00 alone, without the stage's implementation. It imports
`core_reference.py` for the P0-01 primitives and re-implements the hot `Fx`
operations and `noise2` (with a lattice cache) for speed; `check`
cross-checks those against `core_reference.py`.

```
python3 -B worldgen/tests/reference/p1_elevation_reference.py check [--large]
python3 -B worldgen/tests/reference/p1_elevation_reference.py case shipped|small N SEED
python3 -B worldgen/tests/reference/p1_elevation_reference.py golden [PATH|-]
```

`check` runs every worked example of P1-01 10.1 and the reference values of
10.4 at N = 64, 128 and 256 (a few minutes); `--large` adds the N = 512 floor
results and the N = 1024 seed 0 layer hash (about twenty minutes in all).
`case` prints the per-step values of one world. `golden` writes
`worldgen/tests/golden/p1-01-elevation.txt` (format in its header), which
`stages_elevation_golden_test.cpp` reads; it takes a few minutes.

Readings chosen where the spec is open are listed in the test writer's report
of the stage; the one that changes a checked number is 10.1 "Coordinates":
the values printed for N = 1024 are those of N = 512, and the script checks
the correct ones.

## Reference of the P1-02 hydrology stage

`p1_hydrology_reference.py` is an independent exact-integer implementation of
`docs/specs/p1-02-hydrology.md` (steps H1 to H9), written by the test writer
from the spec text and P1-00 alone, without the stage's implementation. The
elevation input of a world comes from `p1_elevation_reference.py`, so no
hydrology golden value depends on the C++ elevation stage. The flood (G3) is
its own bucket-queue implementation; `check` compares it with a binary-heap
flood and with the elevation reference's `g3_flood`, `g4_receivers` and
`chamfer`, and checks the results against independent second methods
(Dijkstra minimax levels, accumulation by walking upstream, union-find lakes,
Dijkstra chamfer) through the invariants of P1-02 section 6.

```
python3 -B worldgen/tests/reference/p1_hydrology_reference.py check [--quick]
python3 -B worldgen/tests/reference/p1_hydrology_reference.py case shipped|small N SEED [AREA]
python3 -B worldgen/tests/reference/p1_hydrology_reference.py golden [PATH|-] [--large] [--legacy]
python3 -B worldgen/tests/reference/p1_hydrology_reference.py fixtures
```

`check` runs worked example 5.1 (every grid, `order` and the pre-rerouting
receivers), every expected value of 8.1, the designed fixtures, the class cap
on a one-row corridor, 200 random fixtures of 8.2 and the golden worlds up to
N = 256 (about a minute and a half with a warm elevation cache; `--quick`
skips the worlds). `case` prints the golden items of one world. `golden`
writes `worldgen/tests/golden/p1-02-hydrology.txt` (format in its header),
which `stages_hydrology_golden_test.cpp` and `cli_hydrology_test.cpp` read;
`--large` adds the N = 512 and 1024 cases of P1-02 8.4, whose elevation takes
the elevation reference about thirteen minutes. The committed file was written
with `--large`. `fixtures` prints every grid of the hand-made fixtures, as the
C++ tests hard-code them.

`UNMAPPED_ELEVATION_CACHE=DIR` keeps the elevation grids of the reference
worlds in DIR (outside the repository, e.g.
`~/.cache/the-unmapped/out/hydro-elev-cache`), so later runs skip the
elevation reference.

The random fixtures of 8.2: fixture f is a 34 x 34 grid drawn in index order
from `Rng(substream(stream_key(0, "test.hydro"), f))`, `uniform_i64(-50, -1)`
on the border and `uniform_i64(1, 200)` inside; the golden file holds
fixtures 0 to 3 with `river_min_area_m2` 2048 (T = 8).

The H2 tie-break (P1-02 H2, amended 2026-11-16 and re-specified the same day
as variant B, generator 0.5.0): hydrology's own flood with L[m] = max(e[m],
L[c]), cells visited when pushed, pushed with key L[m] × 2^16 + t[m] and
popped by the smallest (key, seq) (a binary heap; `check` compares it with a
sorted-list queue, and at t = 0 with the bucket-queue G3); G4 runs on the
decimetre levels with the flood parent as fallback. `check` reproduces worked
example 5.2 for both tie settings. t is `flat_tie`
(`fbm_m` of stream `geo.hydrology.flat` with `flat_noise`, shipped wavelength
400, octaves 1, gain 0.5) for the world cases, the random ties of 8.2
(`uniform_i64(0, 65535)` per land cell from `Rng(substream(stream_key(0,
"test.hydro"), 1000 + f))`) for the random fixtures, and 0 for the case marked
`zero`. `golden --legacy` writes every case with t = 0 in the item list of the
0.4.0 file; it reproduces that file byte for byte, which shows that t = 0 is
the stage without the tie-break (P1-02 H2, "t = 0"). `check` also tests
invariant 11 (equal `water_depth`, `lake`, `coast_dist`, lake and ocean cells
and decimetre levels with and without t) on the random fixtures with random t
and on the golden worlds with `flat_tie`.

## Reference of the P1-03 climate stage

`p1_climate_reference.py` is an independent exact-integer implementation of
`docs/specs/p1-03-climate.md` (steps C1 to C6, the report keys and palettes
of section 9), written by the test writer from the spec text and P1-00
alone, without the stage's implementation. The input layers of a world
(`elevation`, `water`, `water_depth`, `coast_dist`) come from
`p1_hydrology_reference.py` with the shipped threshold and `flat_tie` of the
seed, and its elevation from `p1_elevation_reference.py`; each cached
elevation grid is checked against the hydrology golden file's `elevation`
item before use. It uses the elevation reference's fast `Fx` operations and
`fbm_m`; `check` cross-checks them against `core_reference.py`.

```
python3 -B worldgen/tests/reference/p1_climate_reference.py check [--quick]
python3 -B worldgen/tests/reference/p1_climate_reference.py case shipped|small N SEED [WEIGHTS]
python3 -B worldgen/tests/reference/p1_climate_reference.py golden [PATH|-] [--large]
python3 -B worldgen/tests/reference/p1_climate_reference.py winds FIRST LAST
```

The spec was revised on 2026-11-17 (triangle kernel in C4, diagonal taps
across the wind in a raster sweep in C5, preconditions in order with S ≤
32767, shipped `rain_oro_mm` 2500 and `moisture_path_mm_km` 32000); the
reference and the golden file follow the revision.

`check` reproduces every worked example of P1-03 5.1 (the wind draws, the
temperature table, the one-box pass, the triangle with r = 2 and its edge
clipping example, both sweeps cell by cell with every q raw and p to three
decimals, the rainfall table and both sweeps' rainfall rows, the palette
example of section 9). It checks the sweep against a second method
(memoized per-cell recursion, no line order; the line or raster order is
also asserted to read only computed taps) and both C4 passes against direct
window sums; invariant 7 (all 24 pairs of the three generators and eight
codes, and the triangle, on the 7 x 5 and other bordered grids, grids with
random water, and 40 random fixtures; the hs bound on all 200); the C4
overflow margin (S = 32767 with r = 250 on 260 x 255 and 300 x 1 grids,
every partial sum checked against int64); the preconditions in order; and
invariants 1 to 5 on the 200 random fixtures of 10.2 and (without `--quick`)
on the golden worlds up to N = 128. About 40 seconds with `--quick`. The
golden file also holds one case of fixture `overflow` (format in its
header). `golden` writes
`worldgen/tests/golden/p1-03-climate.txt` (format in its header), read by
`stages_climate_golden_test.cpp` and `cli_climate_test.cpp`; `--large` adds
the N = 512 and 1024 worlds of P1-03 10.4 (two minutes with a warm cache;
the elevation reference takes about three minutes per 512 world and eight
for the 1024 one). The committed file was written with `--large`.
`UNMAPPED_ELEVATION_CACHE=DIR` works as for the hydrology reference.

The random fixtures of 10.2 (the test writer's construction): fixture f is
34 x 34; `Rng(substream(stream_key(0, "test.climate"), f))` draws the wind
code `uniform_u64(8)` first, then `uniform_i64(1, 3000)` for each interior
cell in index order (elevation = S, `water` 0, depth 0); the border is
ocean with elevation -10 and depth 10; `coast_dist` is P1-02 H7 on the
ocean mask; the climate tuning is shipped with `wind_weights` one-hot at the
drawn code, and the world seed is f.

### Generator 0.9.0 (P1-03 revised 2026-11-18)

The climate reference adds C2b (`temp_offset_max_c`, one `uniform_i64(-M, M)`
draw from `geo.climate.temp_offset`, M = round_int(10 x `temp_offset_max_c`))
and C3 with the offset, ships `coast_rain_mm` 40, and writes the golden item
`temp_offset_dc` and a seventh case-line field (`temp_offset_max_c`). `check`
reproduces the C2b offsets of 5.1 (seeds 0 to 11, 42, the phrase; 0.04 and
0.05; both extremes over seeds 0 to 9999 at M = 50), the C3 offset table, the
C6 table and both sweep rainfall grids with 40 mm (and the 75 mm values), and
invariant 8. `golden --legacy` uses temp_offset_max_c 0 and coast_rain_mm 75
with the 0.8.0 cases, header and items and reproduces the 0.8.0 file byte for
byte (checked). Large cases need verified elevation grids: the cached 512 and
1024 grids under `p1-04-tests/elev-cache` were stale; grids decoded from the
0.8.0 validation worlds, each checked against the hydrology golden
`elevation` item, were used.

## Reference of the P1-04 biome stage

`p1_biomes_reference.py` is an independent exact-integer implementation of
`docs/specs/p1-04-biomes.md` (second version: B2 to B6, G7 with the minimum
river class, G8, the `biome` and `climate` table rows, the report keys of
section 9), written by the test writer from the spec text and P1-00 alone,
without the stage's implementation. The input layers of a world come from
`p1_hydrology_reference.py` (with `flow_dir` and `river`) and
`p1_climate_reference.py`.

```
python3 -B worldgen/tests/reference/p1_biomes_reference.py check [--quick]
python3 -B worldgen/tests/reference/p1_biomes_reference.py realcells DB
python3 -B worldgen/tests/reference/p1_biomes_reference.py golden [PATH|-] [--large] [--db-dir DIR]
python3 -B worldgen/tests/reference/p1_biomes_reference.py case NAME
```

`check` reproduces the worked examples of P1-04 5.1: G7 on P1-02's example
with k = 1 and k = 3 (and k = 2, 15) by the spec's FIFO and by a second
method (recursion along `flow_dir`), the G7 precondition failures, the 21
lookups B1 to B21 with every raw tq and rq the table gives, the slope and
scale claims, the size rule, the B5 examples, the real-cell table from its
tabulated inputs, the eight `biome` rows, and the 6 x 6 report example; then
invariants 1 to 4 of section 6 (its own exhaustive rule evaluation and B5)
on the designed fixtures and the golden worlds up to N = 128, and that the
designed fixtures fire every natural biome and make every live condition the
sole failure of its rule somewhere. About ten seconds. `realcells DB`
checks the real-cell table on the 0.7.0 world file of 1024 seed 3 (every
input, hand, S2, nt, nr, tq, rq, the B4 codes and the river cell's B5;
about seven seconds).

`golden` writes `worldgen/tests/golden/p1-04-biomes.txt` (format and the
designed fixtures' construction in its header), read by
`stages_biomes_golden_test.cpp` and `cli_biomes_test.cpp`. `--large` adds
N = 256, 512 (seeds 0, 42 and the phrase) and 1024 (seed 1). With
`--db-dir DIR`, the inputs of a large case are decoded from `DIR/<case>.db`
(a world of the merged 0.7.0 pipeline with the shipped data) instead of
being recomputed, and each decoded layer is first compared with the layer
hashes of `p1-03-climate.txt` and `p1-02-hydrology.txt` for that world, so
they are proven equal to the references' layers. The committed file was
written with `--large --db-dir` (the validation worlds of
`~/.cache/the-unmapped/out/p1-03-validation/` for 512 seeds 0 and 42 and
1024 seed 1, and a 0.7.0 `generate --seed-phrase "The Unmapped"` for the
phrase); the N = 256 case and every default case come from the references.
`UNMAPPED_ELEVATION_CACHE=DIR` works as for the hydrology reference.

### Third version (generator 0.9.0)

Twelve definitions, the rule fields `river_dist_max_m` and `wet_frac_min`
(sentinels 32767 and 0, seam range checks), B2b (the two raster passes;
`river_distance_closed` is the brute-force closed form), B2c (two clipped box
passes by running sums; `box_pass_direct` sums every window), the per-size
wet scale and B4's two comparisons. New golden items `wet_scale`, `d34`,
`wet_count`, `wet_total`, `temp_offset_dc`; table `climate` hashed with three
columns; conditions `rdist` and `wet` in `sole_fail`. Terrace fixtures (header
of the golden file) cover fen failing only on hand and wet woodland failing
only on elevation. `check` reproduces every worked example of 5.1 (chamfer
and wet-share grids, exactness at 0.45 and 0.675, V1 to P6 with their sole
failures, B1 to B21 and S1 to S6 with the four changes and both variants,
the nine real cells from their tabulated inputs); `realcells3 DB` reproduces
the nine real cells on the 0.8.0 world of 1024 seed 3 (inputs, d34, wet
share, noise, B4). `golden --large --db-dir DIR` takes only the elevation and
hydrology layers from `DIR/<case>.db` (0.8.0 validation worlds; the phrase
world of 0.7.0) and recomputes the climate; the four large cases equal P1-04
10.4 (layer hashes, table hashes, offsets, biome_share of 512 seed 0).
`golden --legacy --large --db-dir <0.7.0/0.8.0 worlds> --climate-golden
<0.8.0 p1-03-climate.txt>` reproduces the 0.8.0 file byte for byte (checked).

## Reference of the P1-05 resources stage

`p1_resources_reference.py` is an independent exact-integer implementation of
`docs/specs/p1-05-resources-habitability.md` (second version, generator
0.10.0: R2 to R6, the seven layers, the report keys and the `score` palette
of section 9), written by the test writer from the spec text and P1-00
alone, without the stage's implementation and without the spec writer's
prototype. It uses G7 (`hand_fifo`, k = 1) and G8 (`slope_s2`) of
`p1_biomes_reference.py`, the fast `Fx` operations and `fbm_m` of
`p1_elevation_reference.py`, and its own chamfer, ramps, scores, walk areas
and report values.

```
python3 -B worldgen/tests/reference/p1_resources_reference.py check [--quick]
python3 -B worldgen/tests/reference/p1_resources_reference.py golden [PATH|-] [--large] [--db-dir DIR]
python3 -B worldgen/tests/reference/p1_resources_reference.py case NAME [--db-dir DIR]
```

`check` reproduces every worked example of P1-05 5.1 (ramp, q255 and the
seven tangents; the 9 x 3 chamfer grid and its metres; the eleven R3 rows
with fs and fr; every R4 row with sf, al, st, fish, eu, su and vn; the eight
R6 rows with sc, b, wat, the slope factor, flood and pocket; R5 (a) to (c)
and the step limits 112/113 and 158/159), the palette example of 9, the
section-2 preconditions with their messages, and invariants 1 to 5 of
section 6 with a second method for each intermediate (G7 by recursion,
the chamfer by Dijkstra, walk areas by union-find) on the designed and
terrace fixtures and (without `--quick`) the shipped and small-map worlds
up to N = 128. About 20 seconds.

`golden` writes `worldgen/tests/golden/p1-05-resources.txt` (format in its
header), read by `stages_resources_golden_test.cpp`,
`stages_resources_property_test.cpp` and `cli_resources_test.cpp`. Each
case names a case of `p1-04-biomes.txt` whose seven input layers (elevation,
hydrology, rainfall and the biome layer, generator 0.9.0) it uses; every
input is checked against that file's layer hashes before use, so a stale
elevation cache or world file stops the run. The default cases are
recomputed by the biome reference (`UNMAPPED_ELEVATION_CACHE=DIR` as for
the earlier references); `--large` adds 512 seeds 0, 42, the phrase and 1024
seed 1, whose inputs are decoded from `DIR/<case>.db` with `--db-dir`. The
committed file was written with

```
UNMAPPED_ELEVATION_CACHE=~/.cache/the-unmapped/out/p1-04b-tests/elev-cache \
python3 -B worldgen/tests/reference/p1_resources_reference.py golden --large \
    --db-dir ~/.cache/the-unmapped/out/p1-05-tests/db
```

where the four world files were generated by a 0.9.0 `worldgen-cli`
(`main` at e368844, linux-gcc-release) with `--data game/data`
(`--seed 0|42 --map-size 512`, `--seed-phrase "The Unmapped" --map-size
512`, `--seed 1 --map-size 1024`), and every one of their seven input layers
matched `p1-04-biomes.txt` (about two and a half minutes). The case
`shipped-512-s0` reproduces the report example of P1-05 9 in all eight keys.
Tuning variants of the designed fixtures cover every branch of R3 to R6
(golden item `cover`).

### Revision r2 (2026-11-18)

The spec's r2 tuning (habitability weights 70 / 15 / 10 / 5, `alluvial_bonus`
0.15, `hab_flood_hand_m` 0.5, `hab_flood_factor` 0.75, `ore_slope_base` 0.65,
`ore_elev_hi_m` 600; no algorithm change) is the reference's shipped tuning.
`check` reproduces the recomputed worked examples of 5.1 (fertility, ore and
the eleven R6 rows), and the golden file was regenerated by the same
command as above from the same checked 0.9.0 inputs; `shipped-512-s0`
reproduces the r2 report example of 9 in all eight keys. The report of a
world without land prints `walk_main_share = n/a` (r2: `n/a` takes
precedence over the `0.0000` rule).

## Reference of the P1-06 regions stage

`p1_regions_reference.py` is an independent exact-integer implementation of
`docs/specs/p1-06-regions.md` (second version, generator 0.11.0, schema 5:
RG2 to RG13, the layer `region`, the tables `region`, `region_border` and
`region_biome`, the report keys of section 9, the `ids` palette and the
composite `regions` of P0-04 amendment 18 (e)), written by the test writer
from the spec text and P1-00 alone, without the stage's implementation and
without the spec writer's prototype. It uses G7 (`hand_fifo`, k = 1) of
`p1_biomes_reference.py`, G2 of `p1_elevation_reference.py`, the fast `Fx`
operations, `hash_at` and `fbm_m` of `p1_elevation_reference.py`, and the
chamfer of `p1_resources_reference.py`; the flood, the seeds, the merge,
the rows, the names and the borders are its own.

```
python3 -B worldgen/tests/reference/p1_regions_reference.py check [--quick]
python3 -B worldgen/tests/reference/p1_regions_reference.py golden [PATH|-] [--large] [--db-dir DIR]
python3 -B worldgen/tests/reference/p1_regions_reference.py case NAME [--db-dir DIR]
python3 -B worldgen/tests/reference/p1_regions_reference.py world DB...
```

`check` reproduces every worked example of P1-06 5.1 (the RG3 weights and
step costs; the five flood grids with every owner and every printed cost,
each also by a bucket queue; the RG5 seeds with and without the river and
the segment points; RG8 on the graph and on a 32 x 40 grid realising the
example; the RG11 names, sectors and numerals; the RG12 row and both pass
crossings), the composite example of P0-04 amendment 18 (e), the word
grammar of P0-03 amendment 17, the tuning checks, the section-2
preconditions with their messages, an RG6/RG9 fixture (an islet seeded by
RG5, a mainland seeded by RG6, a river-and-lake landmass seeded at its
smallest index), and invariants 1 to 6 of section 6 with a second method for
the flood (bucket queue), the merge (recount from the grid after each merge)
and the border rows (second enumeration) on the designed and terrace
fixtures and (without `--quick`) the golden worlds up to N = 128. About a
minute (5 s with `--quick`).

`golden` writes `worldgen/tests/golden/p1-06-regions.txt` (format in its
header), read by `stages_regions_golden_test.cpp`,
`stages_regions_property_test.cpp` and `cli_regions_test.cpp`. Each case
names a case of `p1-05-resources.txt` whose thirteen input layers
(generator 0.10.0) it uses; every input is checked against that file's
layer hashes, and table `pass` against `p1-01-elevation.txt`'s `pass_rows`
where that file has the case. The default cases are recomputed by the
resources reference; table `pass` of small-128-s2 comes from the elevation
reference (cached as `passes-small-128-2.txt` in `UNMAPPED_ELEVATION_CACHE`).
`--large` adds 512 seeds 0, 42, the phrase and 1024 seed 1, decoded from
`DIR/<case>.db`. The committed file was written with

```
UNMAPPED_ELEVATION_CACHE=~/.cache/the-unmapped/out/p1-06-tests/elev-cache \
python3 -B worldgen/tests/reference/p1_regions_reference.py golden --large \
    --db-dir ~/.cache/the-unmapped/out/p1-06-tests/db
```

where the four world files were generated by a 0.10.0 `worldgen-cli`
(`main` at f3f901a, linux-gcc-release) with `--data game/data` (512 seed 0
is byte-identical to the P1-05 validation world), every one of their
thirteen inputs matched `p1-05-resources.txt`, and the pass tables of 512
seed 42 and the phrase matched `p1-01-elevation.txt` (1024 seed 1 has no
case there; its table is checked as a chain only). About two minutes. The
case `shipped-512-s0` reproduces the report example of P1-06 9 in all eight
keys; `world` on a 0.10.0 world of 1024 seed 0 reproduces the second example
(`regions = 56`, `region_area_km2 = 0.06 1.75 4.53`, 113 borders, 85
walkable, 10 river, 4 pass, 6 isolated, 12 upland).

Tuning variants of the designed fixtures (40 x 40; test tuning of 256 m
tiles, 200 m spacing and 64-cell regions) and a synthetic pass table "X"
cover every branch of RG5 to RG12 (golden item `cover`): the bank rule,
the corner rule, merges, central and numbered names, pass crossings and
lake steps; the isle names and RG6 seeds are reached by the 512 cases and
the RG6 fixture.

### Revision r2 (2026-11-18)

The reference follows P1-06 section 13: RG12 attaches each pass to at most
one border (the crossing of its path nearest its col by (d, j), then the
first in sequence order; `pass_attachment`), the col cell must lie on the
path (`pass <id>: col cell is not on its path`), and RG11's noun takes the
rules isle, `Mere` (2 `lake_cells` >= `cells`, the lake definition's noun),
`names.upland` for upland regions whose dominant biome is not in
`upland_own_noun`, then the dominant biome. Pass rows are (id, path, col);
the golden item `passes` hashes the col cell too, the synthetic table "X"
has cols, `cover` gains `mere`, `fells` and `crossed`, and `names` takes
eleven words. `check` adds the attachment and noun examples of 5.1, a Mere
fixture, the col precondition and invariant 8; `world` on 1024 seed 0
gives `region_borders_pass = 2` (the r2 report example). The golden file
was regenerated by the same command as above from the same checked inputs.

### Revisions r3 and r4 (region names; shipped with P3-08, generator 0.17.0)

The reference implements RG11 r3 from the spec text (P1-06 4, 4.1, RG11 1
to 8, check 4a, invariant 6, r4 14.8): base names without numerals, the
keeper (most cells, then the smaller id), the traits of record
`name_traits`, the three pools per slot with `name_pool_weights` (an empty
pool weighs 0), the draws `hash_at3(K_name, id, t, k)` with K_name =
`substream(stream_key(seed, "geo.regions.name"), N)`, `pick` and
`weighted` of P3-02 4.1, V1 to V6, U (keepers first, then the draws in id
order), the fallback and the fatal message. RG11 now runs after RG12 (the
`pass` trait). It reads `game/data/placenames/` with `tomllib` (the fixture
roots hold identical copies) and uses the shipped tuning records of P1-06 4
in every case. The loader checks of 4.1 (`placename_problem`) and check 4a
(`check_4a`) pass on the shipped files.

```
python3 -B worldgen/tests/reference/p1_regions_reference.py names DB...
python3 -B worldgen/tests/reference/p1_regions_reference.py rename-db SRC DST
UNMAPPED_ELEVATION_CACHE=~/.cache/the-unmapped/out/p1-06-tests/elev-cache \
python3 -B worldgen/tests/reference/p1_regions_reference.py golden --large \
    --db-dir ~/.cache/the-unmapped/out/p1-06-tests/db [--against OLD]
```

`names` prints, per region of a stored world (any generator from 0.11.0:
r3 leaves the region and border rows unchanged), `id|old name|new
name|land|traits|how|base`, with the world seed from `meta`, N the width of
layer `water` and the land centroid from that layer. `rename-db` copies a
world and replaces only the `name` column of table `region` (it first
checks that the stored r2 names, numerals stripped, equal the recomputed
base names). The history references read their inputs from such copies:
`~/.cache/the-unmapped/out/p3-08-tests/inputs/<case>.db` are the twelve
worlds of `p3-06-tests/inputs/` renamed so (every other table checked
identical).

`check` adds `check_rg11_r3`: K_name and every u(k) of 5.1 (seed 42, N
1024), the four drawn names with form, pool, pool size and index of every
slot (`The Scarred Downs`, `Knelldown`, `Harrowholt`, `Prince's Vow`
through the invalid `Birchhurst`, and `Fletcher's Wolds` for region 1's
attempt 1), the validity examples of 5.1 and r4's `Bonereach` / `Dungrove`
compounds, the fallback `Adderbog` of land `fen`, the shipped sizes of 4.1,
the whole table 14.4 (1024 seed 42, every region: name, land, traits,
attempt) from the 0.16.0 world `p3-06-tests/inputs/example-42-1024.db`, and
(without `--quick`) the space sizes of 14.3. Invariant 6 is re-checked by
`check_invariant6`, independently of `rg11_r3` (a backtracking parse of each
drawn name against its land's forms and pools, V1 to V6, keepers, no base
name), on every designed and terrace fixture and the golden worlds up to
N = 128.

Readings and spec errata found: 4.1's table gives 79 common objects and
141 distinct objects; the normative files hold 78 and 140. 14.3's space
sizes were measured with r3's 19 forbidden entries: the reference
reproduces every number exactly with `boner` and `dung` removed, and with
the shipped 21 gives slightly smaller spaces (`SPACE_R4`). 10.5 asks for the
golden line `<case> names <id>:<name>|…`; names hold spaces, so the value
of a `names` line is the rest of the line after `<case> names ` (both
`read_golden`s were changed accordingly). The cover key `numbered` keeps
its name and now counts drawn names (the same regions as r2's numbered
ones, every region of a base name but its first).

The golden file was regenerated by the command above; `golden` compares
the new text with the previous file (`--against`, default the file being
replaced) and refuses unless every line is unchanged except `table_region`
(which must move exactly in the cases whose cover has `numbered` > 0) and
the two new lines. Against the 0.16.0 file: the 4 large cases and 3 of the
N <= 128 cases (`designed-0-lakemerge`, `shipped-128-s0-t`,
`shipped-128-s42-t`, the ones with a repeated base name) moved; layers,
hand, wt, owners, seeds and the two other tables did not. Drawn names at
512 seed 0, 42 and the phrase and 1024 seed 1: 1, 3, 1 and 35, as 10.5
says.

## History data: content hash of the data roots (P3-01)

`p3_data_reference.py` is an independent implementation of the canonical
encoding and content hash of P0-03 section 8 over the TOML files of a data
root, written from `docs/specs/p3-01-history-data-settings.md` and the
schemas of the kinds it encodes (P1-00 6.6 with P1-04 4.1 and P1-05 4.1 for
`biome`; P1-01 4.1 and 4.2, P1-02 4, P1-03 4, P1-04 4.2, P1-05 4.2 and
P1-06 4 for `worldgen`; P3-01 3, 4, 5.1 and P3-02 3 for `race`, `culture`
and `history`), transcribed field by field in declaration order. It parses
the files with the standard library's `tomllib` (syntax only), checks every
key and value type against those schemas, and hashes with
`core_reference.py`'s SHA-256 (cross-checked with `hashlib`). It never runs
the C++.

```
python3 -B worldgen/tests/reference/p3_data_reference.py check
python3 -B worldgen/tests/reference/p3_data_reference.py golden [-] [--from-spec]
python3 -B worldgen/tests/reference/p3_data_reference.py hash ROOT [--kinds biome,worldgen] [--from-spec]
```

`golden` writes `worldgen/tests/golden/p3-01-data.txt`: one line
`<root> <hex>` for `game/data`, `worldgen/tests/data/geo` and
`worldgen/tests/data/geo-small`. The spec listings of P3-01 3.1, 4 and 5.1
and the naming blocks of P3-02 5 are transcribed in the script; with
`--from-spec` they stand in for a root's `races/`, `cultures/` or `history/`
directory while it does not exist (the committed golden file was written so,
before the data files were committed; `check` then confirms that the files
on disk hold exactly the listings' values and that the committed file is
what a fresh computation from the disk gives). `check` also verifies the
spec's claims about the shipped data (orders 1 to 6 in the design's order,
shares strictly increasing from 0, every named check and cross-kind
condition of P3-01 3 satisfied).

Cross-check of the older schemas: `hash ROOT --kinds biome,worldgen` gives
the content hash of a generator 0.11.0 data root. It was compared with the
`content_hash` that the 0.11.0 `worldgen-cli report` prints for a world
generated from each of the three roots, and agreed for all three
(`1543a4ae…` for game/data and geo, `c39875a8…` for geo-small).

## P3-02 names

`p3_names_reference.py` is an independent exact-integer implementation of
`docs/specs/p3-02-names.md` (keyed draws `draw`, `pick`, `chance`,
`weighted` and `TAG_HIST` of 4.1; one attempt of 4.2 with Cap and the
validity rules; the registry, the 64-attempt bound and the numeral fallback
of 4.3 and 4.4; the six shipped naming blocks of section 5, transcribed; the
grammars and named checks of section 3), written from the spec alone, with
hashing from `core_reference.py` and Roman numerals from P1-06 RG11 4. It
writes `worldgen/tests/golden/p3-02-names.txt`, read by the doctest files in
`worldgen/tests/history/`.

```
python3 -B worldgen/tests/reference/p3_names_reference.py check
python3 -B worldgen/tests/reference/p3_names_reference.py golden [PATH|-]
python3 -B worldgen/tests/reference/p3_names_reference.py samples
python3 -B worldgen/tests/reference/p3_names_reference.py sweep [COUNT]
```

`check` reproduces every value of the worked example of 4.6 (stream key,
u(0) to u(6), picks, root, `Focoth` on collision, 61 distinct valid names
and `Iensath II` on the full registry, and the four seed-42 vectors), the
helper values of 9.1 item 2, a second method for `pick` (bracketing) and
`weighted` (P3-00 4.6's wording), and invariant 1 and uniqueness on the
golden lists (about 3 s). `samples` runs the protocol of 5.1 (seed 0, one
registry per culture) and compares with the table of 5.1: all 168 names
agree. `sweep 2000` is 9.2 item 8: 0 fallbacks for every shipped culture
and kind, seeds 0 and 1 (about 2 minutes).

The test cultures `tiny` (two names, Ba and Da) and `never` (no valid root)
are defined both here and in `worldgen/tests/history/p3_02_names_support.hpp`.

Revision of 2026-11-19 (spec text revised after implementation): `weighted`
is fatal when the sum exceeds 2^63 − 1 (4.1, P3-00 H6), `roman` outside
[1, 100000] (4.3); the golden file gains the matching `weighted` and `roman`
lines, the `attempt` lines print `- -` for the root and name of an invalid
attempt (unspecified, 4.2), and the header says 250 earlier names. No name
list or SHA-256 line changed.

## P3-03 chronology

`p3_chronology_reference.py` is an independent exact-integer implementation
of `docs/specs/p3-03-chronology.md`: the data checks D1 to D5 of section 6,
the entry checks of C1, the bounds of C2, the awakening schedule of C3 (one
keyed draw per later race on `hist.chronology.awakening`, the sequential
clamp), the ages of C4 with the numeral fallback, the creator gods of C5
with the registry order of 8.1 (region names, then age names, then god
names), the rows of tables `race`, `race_biome`, `culture`, `god` and `age`
in row order, the eight `meta` keys of 4.2 with the reader's syntax check,
the `[chronology]` keys of 9.1, and the P0-04 8 logical table hashes. It is
written from the spec alone and uses `core_reference.py` (hashing, decimal
parse), `p3_names_reference.py` (`draw`, `pick`, `make_name`, `roman`) and
`p3_data_reference.py` (reading a data root with `tomllib`) as libraries.
It writes `worldgen/tests/golden/p3-03-chronology.txt`, read by the doctest
files `worldgen/tests/history/history_chronology_*` and by
`tools/worldgen-cli/tests/cli_chronology_test.cpp`.

```
python3 -B worldgen/tests/reference/p3_chronology_reference.py check
python3 -B worldgen/tests/reference/p3_chronology_reference.py golden [PATH|-] [--inputs DIR]
python3 -B worldgen/tests/reference/p3_chronology_reference.py schedule SEED SPAN [RACES]
python3 -B worldgen/tests/reference/p3_chronology_reference.py check-world DB [--data ROOT]
```

`check` reproduces every worked example of 7.1 (K_aw, u, pick, d, base and
year per race, ages, gods with attempts, the subsets, the collisions, the
clamp, G = 333), the data checks on the shipped data and each failing once
on edited data, the C1 messages, the 63 subsets at seed 42, a property
sweep (seeds 0-99, five spans, 63 subsets), the statistics claims of 7 C3
and 12 on seeds 0-1999 (no clamp; least gaps 161 / 483 / 966; every value
of base +- jmax occurs for goblins and orcs; 4 of 30 god names at attempt
> 0), and the committed golden file against a fresh computation (about
3 s). `check-world` recomputes the five tables of a schema-6 world from its
`meta` and its region names, for the validation sweep of P3-03 12.

The world cases of the golden file need the region names of six geography
worlds (`--inputs`, default `~/.cache/the-unmapped/out/p3-03-tests/inputs/`,
`<case>.db`); any generator from 0.11.0 on gives the same table `region`,
and the reference refuses a world whose table `region` differs from
`p1-06-regions.txt` for the four P1 cases. With the shipped data no region
name can collide with an age or god name, so the rows of the world cases
equal those of the seam cases with the same seed and settings.

## P3-04 simulation engine

`p3_engine_reference.py` is an independent reference of
`docs/specs/p3-04-simulation-engine.md` (with revision r1 of 8.5), written
from the spec and P3-00 alone, on `core_reference.py`,
`p3_data_reference.py`, `p3_names_reference.py`, `p3_chronology_reference.py`
and `p0_world_check.py` as libraries. It computes the scheduler (SC1-SC6,
E1-E10, floor and cap) for any age list, span, detail level and
`ForcedTension`, the whole stub world (homes S1, sites S2, names with the
registry rebuilt from `region`, `age`, `god`), the event log, quiet eras,
`region_control`, `settlement_census`, the catalogue tables, table hashes in
`hash --detail` form, the `[history]` lines, and every sentence, with its own
implementation of the template grammar and renderer (8.4, 8.5).

```
python3 -B worldgen/tests/reference/p3_engine_reference.py check [--inputs DIR] [--no-golden]
python3 -B worldgen/tests/reference/p3_engine_reference.py golden [PATH|-] [--inputs DIR]
python3 -B worldgen/tests/reference/p3_engine_reference.py schedule SEED SPAN [DETAIL] [RACES] [--tension P,V,T]
python3 -B worldgen/tests/reference/p3_engine_reference.py world DB [--data ROOT] [--tension P,V,T]
python3 -B worldgen/tests/reference/p3_engine_reference.py check-world DB [--data ROOT]
```

`golden` writes `worldgen/tests/golden/p3-04-engine.txt`. Its world cases
read geography from `--until regions` worlds in `--inputs` (default
`~/.cache/the-unmapped/out/p3-04-tests/inputs/<case>.db`; the four P1 cases
are checked against `p1-06-regions.txt`); the chronology rows come from the
P3-03 reference. `check-world` is for the validation sweep on schema-7
worlds: it recomputes the five chronology and thirteen history tables and
compares them table by table, re-checks invariants 1, 2, 4, 5 on the stored
rows, and renders every event from the stored catalogue.

Readings: `history.engine` and `history.report` fall back to the listings
of spec 4.1 and 4.2 where a root has no such file (the golden depends on
the shipped files matching them; `check` compares them once they exist); a
`flare_up` is a point event (`end_year` = `year`); a non-ASCII byte inside
a placeholder is reported at its own offset; `{@settlement}`'s problem text
is `{@settlement} needs a kind with a settlement at byte <n>`; in a quiet
step the phase-end census (6.7 item 4) uses the step's Q5 decision, so a
quiet row closed by a flare-up ends at that step.

Never regenerate the golden file to make a C++ test pass.

## Generator 0.17.0 (P3-08): data, names, chronology, engine

Written from `docs/specs/p3-08-followups.md` and the stage-spec revisions it
names (P3-01 r8, P3-02 r5/r6 section 11, P3-03 r3, P3-04 r10), without the
engineer's branch. Each of the four references serves 0.17.0 by default and
keeps 0.16.0 behind a switch, which reproduces the committed 0.16.0 golden
files byte for byte (checked before the 0.17.0 files were written):

```
python3 -B worldgen/tests/reference/p3_data_reference.py golden - --version 0.16.0
python3 -B worldgen/tests/reference/p3_names_reference.py golden - --rules r4
python3 -B worldgen/tests/reference/p3_engine_reference.py golden - --version 0.16.0 \
    --inputs ~/.cache/the-unmapped/out/p3-04-tests/inputs
```

- `p3_data_reference.py`: kinds `name_filter` and `placename`, race field
  `cell_weights`, culture field `god_patterns`, `history.peoples`
  `plague_major_dead` and `crisis_small_salience`, `worldgen.regions`
  `name_pool_weights` and `name_traits`. The 0.17.0 listings are derived
  from the 0.16.0 ones by the edits the specs state (`SPEC_FILES_017`); they
  stand in while the disk holds 0.16.0 files, as do game/data's
  `name_filters/` and `placenames/` for a fixture root without them and P1-06
  4's two records for a `regions.toml` without them (each noted by `check`).
  `check` adds the 0.17.0 listing facts and the `name_filter` checks of
  P3-02 11.2 (sizes, effective sizes 101 / 35 / 768, sorted lists, each
  check failing once). `--version 0.16.0` also gives the content hash
  `676a3ed5…` that a 0.16.0 world reports.
- `p3_names_reference.py`: the rule of 11.3 (`root_word`, `name_filtered`),
  `names_form_text` with r6's fixed words, `god_patterns` and the elven
  `The House of {root}` (`CULTURES_R5`), a filtered attempt skipped like any
  invalid one. `check` reproduces 11.3's examples (offensive ones written as
  concatenations), 11.7 items 4 to 8 and 10, the r5 sample table, the god
  and realm tables and the seed-42 `Khorkhound` of 11.6, invariant 5 and
  item 9 on the golden lists, and item 7 (r4's lists, as 0.16.0's sha256
  lines, from the empty filter with `god_patterns` = `patterns`). `samples`
  compares with 11.6's table (0 differences); `sweep 2000`: no fallback, no
  filtered root word, every god root of its culture's `god_patterns`, no bare
  realm (about 2 minutes). The golden file keeps every keyed-draw vector of
  4.1 and adds `fattempt`, `rootword`, `filter`, `formtext`, the scenarios
  `filtered`, `redraw_alone`, `redraw_after` (test culture `filt`, test
  filter `anywhere = [ba]`) and `r4sha256`; no offensive stem is stored.
- `p3_chronology_reference.py`: gods drawn with the world's filter and
  `god_patterns`; `names_form_text` after the data checks D1 to D5 (message
  as 11.3 writes it, prefix `history:`). The 7.1 examples, data checks and
  statistics run on the 0.16.0 rules (they are r4's), the properties on both,
  `check_r3` the god table of P3-02 11.6 through the stage. Only `god` and
  `race_biome` table hashes, the `gods` lines and `input_region` move.
- `p3_engine_reference.py`: kind 31 in the catalogue, the three-word value
  form, names inside a sentence (`render`), the r10 `quiet_era` template;
  `QUIET_ERA_R10` is read by the peoples and conflict references' quiet
  steps (the stub charges no event). `check` keeps 17 as written on the
  0.16.0 rules and inputs, and adds `check_example_r10` (seed 42 at 1024 on
  the renamed inputs) and `check_render_r10` (16.1 item 10). In the golden
  file the schedule cases, `phase`, `event`, `event_participant` and the
  other row tables of the stub worlds are unchanged; `event_kind*`,
  `civilization`, `settlement`, the names, sentences and `report` lines move.

The history world cases read `~/.cache/the-unmapped/out/p3-08-tests/inputs/`:
the P3-06 test inputs with table `region`'s names replaced by P1-06 r3's
(`p1_regions_reference.py rename-db`; nothing else changed).

## P3-05 peoples and P3-06 expansion and conflict (generator 0.16.0)

`p3_peoples_reference.py` (P3-05) and `p3_conflict_reference.py` (P3-06) are
independent references written from their specs and the P3-04 seam alone.
The conflict reference extends the peoples one: it runs the whole 0.16.0
history (peoples, then conflict in T2, T6 and Q4; conflict the one tension
source, asked by the engine in Q5 between Q4 and Q6) and writes every row of
the thirteen tables, `relation` included (P3-04 6.7 item 2, net change per
bookkeeping). It reproduces the sample of P3-06 15.2 (seed 42 at 1024: 1030
events, the five flare-ups and their triggers, the 38 wars, the end state,
the sixty summary lines and the full-level excerpt, compared with the spec
text in `check`).

```
python3 -B worldgen/tests/reference/p3_conflict_reference.py check [--inputs DIR] [--quick] [--write]
python3 -B worldgen/tests/reference/p3_conflict_reference.py golden [PATH|-] [--inputs DIR]
python3 -B worldgen/tests/reference/p3_conflict_reference.py world DB [--data ROOT] [--span E] [--races R] [--detail D] [--civs C] [--intensity I]
python3 -B worldgen/tests/reference/p3_conflict_reference.py check-world DB [--data ROOT]
python3 -B worldgen/tests/reference/p3_conflict_reference.py synthetic SEED [--stress]
python3 -B worldgen/tests/reference/p3_conflict_reference.py gaps
```

`golden` writes `worldgen/tests/golden/p3-06-conflict.txt`: the eight
golden worlds of P3-06 16.3 plus two 512 cases with `--event-intensity
turbulent` and `--civs-per-race 4`, sixteen synthetic geographies (P3-05's
`make_synthetic`), twelve stress cases (a conflict tuning that reaches every
kind 20-30, flare-ups, battles at passes, `last_refuge` and the shelter) and
three `norefuge` cases (the stress tuning without the guards: a race dies
out). World cases read geography from `--inputs` (default
`~/.cache/the-unmapped/out/p3-06-tests/inputs/<case>.db`, any world up to
regions or chronology; `example-42-1024.db` is the input of
`hist-conflict-42-1024`, `geo-seed-42-512.db` of the two settings cases).
`check` runs the hand-made units (the worked example W1 of 16.1, fronts,
defence, decay, tension, odds, casualties, the 16.4 tuning messages), the
invariants of 14 and P3-05's (as amended by P3-06 18.1) and the hard bounds
on every case, draw independence, the peoples-only configuration against
the P3-05 reference, and the sample of 15.2; full `check` takes about
5 minutes on a quiet machine, `--quick` under 2. `check-world` is for the
validation sweep on 0.16.0 worlds.

Since 0.16.0 `p3-05-peoples.txt` holds the peoples-only configuration
(run_engine with peoples alone) and `p3-04-engine.txt` the stub
configuration, both on the catalogue of 30 kinds; `p3_data_reference.py`
knows `history.conflict`, `prowess_permille` and goblin aggression 600, and
stands the spec listings in for data roots older than 0.16.0. The readings
taken where the spec is open are printed by `gaps`.

Never regenerate the golden files to make a C++ test pass.

### Generator 0.17.0 (P3-08: P3-05 r6, P3-06 r7)

Both references implement the P3-08 rules from P3-05 r6 and P3-06 r7, each
behind a flag (`p3_peoples_reference.FLAGS`: `cell` and `fortress_cell`,
6.2 and 8.4; `last`, `causes`, `dest`, 8.5 with kind 31
`settlement_abandoned_to`; `cap`, 9.1; `plague_sal`, `crisis_sal`, 10.2 and
10.3; `quiet`, P3-04 r10 6.6; `p3_conflict_reference.FLAGS`: `loss`, 7.3
item 4). `--legacy` (or `legacy()`) turns every flag off and switches the
engine, data and names references to 0.16.0 (`pe.use_version`): then
`golden` reproduces the committed 0.16.0 files `p3-05-peoples.txt` and
`p3-06-conflict.txt` byte for byte from the 0.16.0 inputs
(`~/.cache/the-unmapped/out/p3-06-tests/inputs`; checked at P3-08).

The synthetic geography gains the layers `elevation`, `ore` and `walk_area`
and the region column `elev_max_dm` from a second sequence (seed
`0x5EED1000 + seed`, one draw per cell), so every earlier value of
`make_synthetic` is unchanged (the C++ `make_synthetic` builds the same).
The goldens were regenerated with the inputs of P1-06 r3's region names:

```
python3 -B worldgen/tests/reference/p3_peoples_reference.py golden --inputs ~/.cache/the-unmapped/out/p3-08-tests/inputs
python3 -B worldgen/tests/reference/p3_conflict_reference.py golden --inputs ~/.cache/the-unmapped/out/p3-08-tests/inputs
python3 -B worldgen/tests/reference/p3_peoples_reference.py compare-016 OLD_P3-05_FILE --inputs DIR
```

`compare-016` asserts P3-08 5 against the 0.16.0 file: score lines move
only for faeries; a home line of another race moves only where each 0.16.0
home is a home again or not eligible (held) at that awakening in the
0.17.0 run (it prints the holder and the counts of 7.2). `check` adds the
units of P3-05 16.1a (17a-17i) and P3-06 16.1 (r7: the victors' dead from
the smaller force, the capture sentence with `fled` 0, 1, 12), the r6
invariants (6 as the P3-06 validation's D1 reads it, 11, 12 with an in-run
recomputation of the destination, 13), and the peoples-only prefix of
P3-06 16.2 item 18a on every world and synthetic case (`check_prefix`).
The 15.2 samples of both specs are 0.16.0 measurements and are checked only
with `--legacy`.

## Generator 0.18.0 (P3-09: P3-06 r9 20, P3-05 r8 21, P3-01 r9 3.2, P3-07 r8 21)

Written from `docs/specs/p3-09-conflict-balance.md` r2 and the stage-spec
sections it names, without the engineer's branch (the spec writer's
prototype was read only afterwards, to list differences). Every reference
serves 0.18.0 by default; `--version 0.17.0` (and `--version 0.16.0`, or
`--legacy`) serves the older rules, data, names and catalogue, and
reproduces the committed 0.17.0 goldens byte for byte (checked before and
after the 0.18.0 files were written; the 0.17.0 files are those of d750d15):

```
I=~/.cache/the-unmapped/out/p3-08-tests/inputs
python3 -B worldgen/tests/reference/p3_data_reference.py golden - --version 0.17.0
python3 -B worldgen/tests/reference/p3_engine_reference.py golden - --version 0.17.0 --inputs $I
python3 -B worldgen/tests/reference/p3_peoples_reference.py golden - --version 0.17.0 --inputs $I
python3 -B worldgen/tests/reference/p3_conflict_reference.py golden - --version 0.17.0 --inputs $I
python3 -B worldgen/tests/reference/p3_names_reference.py golden - --version 0.17.0
```

- `p3_data_reference.py`: `SPEC_FILES_018` (three race fields after
  `prowess_permille`, eleven `history.conflict` fields after `last_refuge`,
  two `history.peoples` fields after `regions_per_civ`, `claim` 35, the
  entries of kinds 32 and 33 appended after kind 31's: an assumption, as
  for every earlier kind), the 54-field conflict schema and the named check
  `conflict_enmity_hops`. While a root on disk holds 0.17.0 files, the
  0.18.0 listings stand in (`disk_version`, noted by `check`).
- `p3_engine_reference.py`: the catalogue of 33 kinds (`COMPILED_018`),
  `at_least(v)`; the stub worlds change only in the catalogue tables and
  the two new `events_<key>` lines.
- `p3_peoples_reference.py`: `FLAGS["auto_size"]` (21 item 5) and invariant
  2 with `refugees_settled`; no 512 or 1024 world changes by it.
- `p3_conflict_reference.py`: one flag per rule of 20 (`flight`, `goals`,
  `cost`, `weary`, `revenge`, `muster`, `levies`, `odds`, `truce`,
  `conceal`, `home`, `enmity`); `use_version`, `rules(v)` (flags only);
  the hand-made units of 20.14 (`check_units_r9`); the invariants of 20.13
  from the rows (`check_invariants_r9`, `weary_violations`); P3-07 r8 21's
  six `[stats]` keys, `history_note`, units and the summary set
  (reference-only until P3-07 is implemented), written to the golden file
  as `stats`, `note` and `summary` lines; the extra world case
  `hist-conflict-flight` (seed 1 at 512, input `sweep-1-512.db`).

Re-recording after a data change (the short word list of P3-09 item 8) is
one command per reference, in this order:

```
python3 -B worldgen/tests/reference/p3_data_reference.py golden
python3 -B worldgen/tests/reference/p3_names_reference.py golden
python3 -B worldgen/tests/reference/p3_chronology_reference.py golden
python3 -B worldgen/tests/reference/p3_engine_reference.py golden --inputs $I
python3 -B worldgen/tests/reference/p3_peoples_reference.py golden --inputs $I
python3 -B worldgen/tests/reference/p3_conflict_reference.py check --write --inputs $I
```

### P3-09 r3 (P3-06 r10 20.15-20.22, P3-05 r9 21 item 6, P3-07 r8 21.8, P3-01 r10 3.3)

Flags `score`, `kill`, `search`, `floor`, `held` (conflict) and
`refuge_floor` (peoples); kinds 34 `war_won`, 35 `search_abandoned`, 36
`refuge_held`; the stream `hist.conflict.search`; the three conflict fields
after `enmity_hops`; faerie home defence 0; `[stats]` `wars_unfound` and
`refuges_held_no_destination`; the hand-made units of 20.19
(`check_units_r10`) and the invariants 7-11 and T3 from the rows
(`check_invariants_r10`). The r2 finding (stress cases 1, 12, 14: a race
bled out behind its guard) is closed by 20.20 and checked as an ordinary
invariant.

The short word list (P3-02 r8 11.11) is read from disk at 0.18.0; at 0.17.0
the name filter is read from git at d750d15 (`NAME_FILTERS_017_COMMIT` in
`p3_data_reference.py`, `FILTER_COMMIT` in `p3_names_reference.py`), so the
0.17.0 goldens stay reproducible after the data change. `p3_names_reference.py`
takes `--version 0.17.0` too.

### P3-09 fixes (P3-06 r13 20.27, 20.28; P3-07 r11 21.8 item 5; generator stays 0.18.0)

Flag `cede_all` (conflict): a cession (7.7's and 20.3's) passes every
settlement of the loser standing in the ceded region, a colony founded
earlier in the same step included. Invariant (13) is checked from the rows by
`cession_violations` (owners replayed from the log; the reading is in
`gaps`), with `check_invariants_r9`. `wars_same_goal_unchanged_max` follows
21.8 item 5's incremental form (`same_goal_unchanged_max`): wars closed by
`civ_conquered` or `civ_ended` are skipped, the window ends at the latest end,
and the first war after a change starts a run of 1. Hand-made units:
`check_units_r13`. `check-world` on a world whose stored history differs from
the reference still runs the row invariants and reports, rather than stops,
when an in-run check does not apply. The golden file was rewritten by
`golden --inputs $I`; the entries that changed are those of
`hist-conflict-flight` (seed 1 at 512 holds a cession of a same-step
colony, e354) and the `wars_same_goal_unchanged_max` line of `stress-5`,
`-6`, `-7`, `-12`, `-13` and `norefuge-13`.

## Generator 0.19.0 (P3-10): data, names, region names, chronology

Written from `docs/specs/p3-10-followups.md` and the stage-spec revisions it
names (P3-01 r11 3.4 and 3.5, P3-02 r10 section 12, P1-06 r6 section 15 and
r7 15.7), without the engineer's branch. Each reference serves 0.19.0 by
default and keeps 0.18.0 behind `--version 0.18.0`, which reproduces the
committed 0.18.0 golden files byte for byte (checked before the 0.19.0 files
were written; the 0.18.0 files are those of 81908fa):

```
python3 -B worldgen/tests/reference/p3_data_reference.py golden - --version 0.18.0
python3 -B worldgen/tests/reference/p3_names_reference.py golden - --version 0.18.0
python3 -B worldgen/tests/reference/p1_regions_reference.py golden - --large \
    --db-dir ~/.cache/the-unmapped/out/p1-06-tests/db --version 0.18.0
python3 -B worldgen/tests/reference/p3_chronology_reference.py --version 0.18.0 golden -
```

- `p3_data_reference.py`: `SPEC_FILES_019` (race `cell_weights` gains
  `inland`, `port`, `habitability` after `height`; faerie `coast` -500;
  `odds_min_permille` and `awakening_quarrel` after `home_defense_permille`;
  `history.conflict` without `odds_min`, with `search_weary_years` and
  `search_repeat_salience` after `search_years`; `history.peoples`
  `cause_memory_years` and `inland_cap` after `civs_extra_max`; the templates
  of kinds 10, 15, 16, 17, 22, 23, 31 and the entries of kinds 37 and 38
  appended after kind 36's: an assumption, as for every earlier kind). The
  copied kinds (`placename`, `name_filter`) are served by the spec's edits
  applied to the 0.18.0 files on disk (`placenames_019`, `name_filters_019`:
  P1-06 r6 15.1-15.3, P3-02 r10 12.1), or read from git at 81908fa when the
  disk holds 0.19.0 files and an earlier version is served
  (`copied_files_019`). `write-root DIR` writes a scratch 0.19.0 data root
  (outside the repository) with every listing; its content hash is the
  `game/data` line of `p3-01-data.txt`. `check` adds `check_spec_019` and
  `check_placenames_019` (the grim words kept and moved, every list but
  `excluded` byte-sorted without repeats, the three small edits).
- `p3_names_reference.py`: the filter follows `p3_data_reference`'s served
  version (`use_version`). `make_name(..., cores=K)` is 12.2's rule for the
  history stage (core = root without `'` and `-`; near = equal or one letter
  apart; the fallback adds its first valid attempt's core); `cores=None` is
  4.3 unchanged (gods). `check` adds `check_r10` (12.1's table, Gramm and the
  culture syllables, 12.3's units, the fallback, invariant 6 on the K lists)
  and reads 11.6's god table at 0.19.0 as "only the gods the new entries
  reach are redrawn" (Grammam, Gordman). The golden file adds `corename`,
  `corenear`, `corefallback`, `core` (seeds 0 and 42, one registry and one K
  for civilizations and settlements, gods without K) and `coresha256` lines.
- `p1_regions_reference.py`: `use_version` (`VERSION`, `r7()`); the
  placename lists come through `p3_data_reference.read_dir_files`; V7
  (`v7_stem`, `v7_stems`; `name_attempt(..., slots=)` records the slot
  values) and the keeper per land noun (`keepers`, `land_noun`) in
  `rg11_r3`; invariant 6 as restated by 15.7 item 4; `check_v7_world` on
  every checked world; the hand-made units of 15.6 (`check_rg11_r7`). At
  0.19.0 `check` runs r3's draw examples, sizes and 14.4 table under 0.18.0
  (they describe the 0.18.0 lists). `golden` at 0.19.0 guards against the
  old file: only `table_region`, `names` and `name_attempts` may move (33
  cases moved, exactly those of 15.6). `rename-db` accepts an input whose
  stored names are r3's (the P3-08 inputs), checked against the r3 rule.
- `p3_chronology_reference.py`: `--version`; inputs
  `~/.cache/the-unmapped/out/p3-10-tests/inputs` (with a `READY` marker) at
  0.19.0, the P3-08 inputs before; `p1_06_region_hash` reads
  `p1-06-regions.txt` at 81908fa for versions before 0.19.0.

The history inputs of 0.19.0 are the P3-08 inputs with table `region`'s
names replaced by P1-06 r7's (`p1_regions_reference.py rename-db SRC DST`,
nothing else changed), in `~/.cache/the-unmapped/out/p3-10-tests/inputs/`.

### Generator 0.19.0 (P3-10): engine and peoples (P3-04 r12 20, P3-05 r12 22)

Written from `docs/specs/p3-10-followups.md` and the stage-spec sections it names, without the engineer's
branch. Both references serve 0.19.0 by default; `--version 0.18.0` serves the 0.18.0 rules, data, names and
catalogue and reproduces the committed 0.18.0 `p3-04-engine.txt` and `p3-05-peoples.txt` byte for byte on the
0.18.0 inputs (checked before the 0.19.0 files were written):

```
I8=~/.cache/the-unmapped/out/p3-08-tests/inputs        # P1-06 r3 region names (0.17.0, 0.18.0)
I=~/.cache/the-unmapped/out/p3-10-tests/inputs         # P1-06 r7 region names (0.19.0)
python3 -B worldgen/tests/reference/p3_engine_reference.py golden - --version 0.18.0 --inputs $I8
python3 -B worldgen/tests/reference/p3_peoples_reference.py golden - --version 0.18.0 --inputs $I8
python3 -B worldgen/tests/reference/p3_engine_reference.py check --inputs $I
python3 -B worldgen/tests/reference/p3_peoples_reference.py check --inputs $I
```

- `p3_engine_reference.py`: `COMPILED_019` (kinds 37 `plague_outbreak` and 38 `quiet_stretch`, 20.4);
  `RENDER_R12` (20.2's `{@causes:...}`: `causes_entries`, the grammar and its `unknown placeholder` cases in
  `placeholder_problem`, the rendering in `render`, which reads the causes from the view (`view_from_rows`
  now carries `event_cause`); 20.3's separators, `fmt_value`, in `{value}` and `#` only); `QUIET_R12` (20.1:
  `World.quiet_stretch(start, last, phase)`, one world-level event per quiet phase row, kept in `open_era`
  under key 0 so that every reset of the eras resets it; the stub's `quiet_step` and the peoples and
  conflict references' `quiet_eras` call it). The stub draws its names with K (P3-02 r10 12.2: the history
  stage's rule); no stub name changes by it. `check` adds `check_render_r12` (20.6 items 2 and 3),
  `check_quiet_r12` (20.6 item 1 on every world case and both ForcedTension cases, and a hand-made world in
  which a civilization ends within the row) and `check_example_r12` (20.1's example, seed 42's first row);
  r10's example runs on 0.18.0 and the 0.18.0 inputs. The golden file gains the template cases
  `TEMPLATE_CASES_R12` (every malformed body of 20.2, the shipped templates of kinds 37 and 38).
- `p3_peoples_reference.py`: `FLAGS_019` (`outbreak37`, `memory`, `sites19`, `lie_low`, `cores`) and
  `flags_for(v)`, the flags of generator v (used by the conflict reference). 22.1: kind 37 with `total` and
  `reached` from the plan (`plague_plan` counts the planned spreads; the run checks it against the real
  spreads); 22.2: `recent(e)` (Y - year(e) <= `cause_memory_years`), the dwindle's colony cause
  (`last_colony`), the abandonment's reduction and decline start by the memory; 22.5: `Geo.cell_record`,
  `has_cell_term` against the neutral record, the new site value and `sea_d` (one-cell dilations of the sea
  mask, capped at `inland_cap`); 22.7: `race_last_refuge(civ)`, the one predicate (`race_refuge(s)` calls
  it); 22.7a: `lie_low(civ, until_year)` and the skip in 9.3 (`lying_low`); names through
  `World.hist_name` with `cores=self.cores` (P3-02 r10 12.2, `p3_names_reference.make_name`). `check` adds
  `check_units_r12` (22.8 items 1, 2, 3, 4, 4a; 22.7) and `check_r12_invariants` on every world and
  synthetic case (no kind 4 or 12 event; invariant 13 with `total` and `reached`; no cited reduction,
  decline start or colony older than the memory). The r6 units (17a-17i) run with r6's site value and kind
  12, as written for 0.17.0; 17c gains a second civilization of the race (it contradicted r9 21 item 6).
- `p3-05-peoples.txt` gains the line `hist-peoples-42-1024 dwarf_kinds forts <n> ladder <m>` (P3-10 4 item 2),
  read by `history_peoples_golden_test.cpp` in place of its inline counts (0.19.0: forts 6, ladder 2).

The readings taken are printed by `p3_peoples_reference.py gaps` (the P3-10 entries at the end).

### Generator 0.19.0 (P3-10: P3-06 r14 section 21, P3-07 r12 22.3, 22.5a, 22.5b)

`p3_conflict_reference.py` serves 0.19.0 by default; `--version 0.18.0` (or
0.17.0, 0.16.0) serves the older rules and reproduces the committed 0.18.0
`p3-06-conflict.txt` byte for byte from the P3-08 inputs (checked before the
0.19.0 file was written). One flag per rule of 21 (`FLAGS_019`):

- `reduce_loss` (21.1): `note_reduction` only for a settlement whose loss is
  at least 1 (battles and tributes, through `casualties`);
- `aim_hold` (21.6): at every check of 7.7 and at 7.8's end of an active
  phase the attacker wins by aim only when it holds G (`peace` when the
  defender is exhausted, else `war_won`; no last-battle rule); when the
  defender neither holds nor stands in G the war ends now by the score with
  the last-battle rule (war line how `goal-lost`); the run's ending log
  (`aim_log`) feeds invariants (8) as amended and (14);
- `search_weary`, `search_battle`, `search_sal` (21.7): `searched_until`
  per (hidden civilization, region), cleared by a change of holder; no search
  in a region of a battle of the same war; the first kind-35 event keeps the
  kind's salience 40, later ones `search_repeat_salience`;
- `holder` (21.8): `hold(G)` (owner, else last owner) in the futile counts,
  a change of holder resetting them; `change_years` (the from_year of each
  region_control row whose civ differs from the previous row's) in
  `wars_same_goal_unchanged_max` and the row check of invariant (12);
- `race_odds` (21.9): the claimant race's `odds_min_permille` in the
  declaration and the tension;
- `lie_low` (21.10): the flight calls the peoples hook `lie_low(civ,
  hidden_until)`; while one of its standing settlements is hidden a
  civilization raises no strain, claim or declaration and the tension skips
  its pairs as claimant (`hides`);
- `quarrel` (21.12): `quarrels()` before 5.2 in each T2: the first visible
  T2 of a `civ_founded` civilization of a race with `awakening_quarrel` 1
  seeds strain at `quarrel_strain` x 1000 (r15, 600) toward the owner of
  the nearest standing visible, not hidden settlement of an alive people of
  another race whose disposition is below `border_friction` (r15: never a
  people of the orcs' camp; key: within `enmity_hops`, hops, squared
  distance, id), or keeps it pending (`quarrel_pending`);
- `note_019` (P3-07 r12 22.3, 22.5a): the reworded flare-up note; a
  declaration whose war ended by a `search_abandoned` below 20 is no summary
  candidate.

Hand-made units: `check_units_r14` (21.5 items 1 and 2, 21.11 items 1 to 5,
21.12 item 6); the older units run on the rules they were written for
(`rules("0.17.0")`, `rules("0.18.0")`). Row invariants at 0.19.0:
`check_invariants_r14` ((8) amended, (14), (15), (16) with contact recomputed
in the run, 21.7's three rules) and the P3-05 r12 cause rules (kind 37 as a
reduction, the colony cause, `cause_memory_years`, no kind 4 or 12 event) in
`check_peoples_invariants`. Readings in `gaps` (`r14 ...`).

The C++ side: `p3_06_conflict_support.hpp` counts changes by holder
(`holder_change_years`) in `stats_lines_21` and `weary_violations`, reads (8)
as amended in `check_invariants_10`, accepts kind 38 in the 18a prefix, and
adds `invariants_19` ((14), (15), 21.7) for the property and golden tests;
`history/history_conflict_r14_test.cpp` holds the hand-made row units and the
seam tests on the engineer's names (the P3-10 switch was removed at integration).

### The N = 1024 elevation hashes (P3-10 4 item 3)

`p1_elevation_reference.py layers-1024 shipped|shipped-neutral SEED...` prints
`elevation-1024 s<seed> <hash>` (or `elevation-1024-neutral`) per world, about
an hour each on a loaded machine; the lines sit in a commented block at the end
of `p1-01-elevation.txt`, which `golden` keeps from the committed file. The
slow cases of `stages_elevation_golden_test.cpp` check every seed with a line;
seeds without one are not checked.

### Generator 0.19.0, second round (P3-10 section 12; P3-06 r17 22, P3-05 r15 22.7a, P3-04 r15 20.1 item 1a, P3-02 r13 13, P3-01 r13 3.6)

Written from the spec text alone, without the engineer's branch. The round replaces the unmerged 0.19.0
behaviour in place: `--version 0.19.0` (the default) serves the second round's rules and data, `--version
0.18.0` is unchanged and still reproduces the 0.18.0 files of 81908fa byte for byte (checked again for
`p3-01-data.txt`, `p3-02-names.txt`, `p3-04-engine.txt`, `p3-05-peoples.txt` and `p3-06-conflict.txt`). The
first round's 0.19.0 data on disk is recognised (`pre_019_r17`, `name_filters_r16`) and the second round's
listings stand in for it, as for every earlier change.

- `p3_data_reference.py`: race `hidden_years` after `awakening_quarrel` (orc 100, others 300,
  `HIDDEN_YEARS_019`); `history.conflict` without `hidden_years` and with `quarrel_min_settlements` 2 after
  `quarrel_strain` (int(0, 1000)); the check `conflict_quarrel_strain_max` (`conflict_check_messages`);
  `history.engine.quiet_min_years` 50 after `quiet_events_max` (int(0, 400), `ENGINE_019`) and
  `engine_check_messages` with `engine_quiet_min`; the filter edits of P3-02 13.1 (`NAME_FILTER_EDITS_019_R13`,
  applied after r10's). Effective filter sizes 110 / 31 / 508.
- `p3_names_reference.py`: `triple(name)` and the rule in `attempt` (`TRIPLE_RULE`, from 0.19.0); the test
  cultures `trip` and `trip_hollow`; golden lines `triple`, `tattempt`, `tresult`; `check_r13` (13.3's units,
  `name_filter_not_redundant` on the edited lists and on copies that keep `crude.at_start` `pee` or
  `words.whole` `teepee`, invariant 7 on the lists, the numeral exempt).
- `p3_engine_reference.py`: `QUIET_R15`; `Engine.assured(s, len, closes)` (the assured length A of 20.1 item
  1a, with `q_end` the interval's end); `World.quiet_stretch(..., assured)` marks a row decided without a
  stretch (`open_era[-1]`) when A < `quiet_min_years`; the entry check `engine_quiet_min` and the field's
  range; `check_quiet_r15` (20.6 item 00 on probe runs: rows of 25, 35, 49, 50 years at Q 25 and 50,
  quiet_min 0 and 100, and an exactness sweep over some 700 schedules).
- `p3_peoples_reference.py`: `lie_low(civ, settlement, until)` and `lying_low(civ, year)` (22.7a r15: the
  refuge stands and is the civilization's); 22.8 item 4a's r15 units (an orc's 100 years, the refuge ruined or
  passed early, the new owner).
- `p3_conflict_reference.py`: flags `quarrel_hold`, `race_hidden`, `aim_foot` (0.19.0); `quarrel_with` and
  `held_back` (22.1: in 7.2 item 3 after `declare_min` and before the odds, and in 5.4); `hidden_years_of`
  (22.2); `hides` is peoples' `lying_low` (22.3); `aim_clear` (22.5 item 1, every standing settlement of b in G)
  and 21.6 item 2 only for o != a; the foothold cession in `cost_plan` / `end_war` (22.5 item 3);
  `after_cost` (invariant (17) in the run); `replay_r17` (from the rows: (8) restated, (15) restated, (17),
  the replay against the tables, and the facts: quick aim wins, foothold cessions, wars over a held goal by
  outcome), one golden line per case `<case> r17 quick_aim_wins <n> foothold_cessions <m> held_goal_wars
  <a>/<d>/<t>/<o>`; `check_units_r17` (22.10 items 1 to 6). The check of 8.3 item 4 (a claim's cause is no
  colony of the same step) exempts a colony that was the claimed region's owner_event although a later step of
  the year (a secession) passed the region on and the net rule left it no region_control row (stress-7 e1153,
  first seen with this round's histories; the C++ support has the same exemption).

The goldens re-recorded from these references: `p3-01-data.txt`, `p3-02-names.txt`, `p3-04-engine.txt`,
`p3-05-peoples.txt`, `p3-06-conflict.txt`. `p3-03-chronology.txt` does not move: no god of its cases is reached
by the new filter entries or the triple rule.

Integration with `p3/10-fix2` (branch `p3/10-int2`): the switch `p3_10_r3_switch.hpp` is removed, since every
guessed name was the engineer's. `check-world` on 20 worlds written by the C++ (1024 defaults seeds 0, 7, 23, 40,
42, 53, 58; 512 seeds 0 to 7; 1024 `--civs-per-race 4` seeds 0 and 1; 1024 `calm` seed 0 and `turbulent` seed 14;
2048 seed 0) found every chronology and history table equal to the reference's and no invariant failure. No
golden moved at integration. Foothold cessions in that set: 1024 seed 40 (e198), seed 58 (e922), 512 seeds 3, 4
and 7; no quick aim win. The 8.3 item 4 exemption above is reached by stress-7 e1153 only; there the region had no
owner at all the year before (the holder's row ended with the sack of 4455). The slow cases of 20.27 item 7 (a
colony founded in the Q4 of its cession) move from 1024 seed 36, whose case the round removes, to 1024 seed 5 with
`--event-intensity turbulent` (e933, e1102; `check-world` again without a difference): no 1024 default world of
seeds 0 to 160 holds one.

### P3-10 second round, validation's D1 (21.6's order of items)

`p3_conflict_reference.py` gains the world case `hist-conflict-73-1024` (`WORLD_CASES_019`, served from 0.19.0;
input `hist-conflict-73-1024.db`, a `--until regions` world of 1024 seed 73 with default settings, in the inputs
directory). It is the one world of validation's 375 whose history reaches a T6 check where 21.6 item 1 (aim) and
7.7 item 1 (exhaustion) both hold with different endings: war e875, checked in 5063, ends by aim with the
attacker's `peace` (e914, "The House of Fiegla sued the Snonugg Mob for peace after 29 years of war.") and a
tribute. `golden` with that inputs directory rewrites `p3-06-conflict.txt` with the case's lines appended after
`hist-conflict-flight` and every other line unchanged (checked by diff). A scan of every other golden case, with
the reference's checks instrumented, finds no check where the order of 21.6's items changes an ending, nor do 96
synthetic and stress runs at `exhaustion_permille` 20 to 600: an attacker usually gains its goal by its own capture
in the same T6, so the score is for it and both orders agree. The hand-made order cases are
`history/history_conflict_order_test.cpp`.

## Generator 0.20.0 (P3-11: P3-01 r14 3.7, P3-02 r15 14, P3-04 r17 21, P3-05 r16 23, P3-06 r19 23, P3-07 r14 23)

Written from `docs/specs/p3-11-followups.md` and the stage-spec sections it names, without the engineer's
branch. Every reference serves 0.20.0 by default; `--version 0.19.0` serves the 0.19.0 rules and data and
reproduces the 0.19.0 goldens of dfa4e66 byte for byte (checked for `p3-01-data.txt`, `p3-02-names.txt`,
`p3-03-chronology.txt`, `p3-04-engine.txt`, `p3-05-peoples.txt` and `p3-06-conflict.txt`, the last with
`hist-conflict-73-1024`). The inputs are those of P3-10 (region names do not change), gathered with the slow case's
input in `~/.cache/the-unmapped/out/p3-11-tests/inputs/`.

```
I=~/.cache/the-unmapped/out/p3-11-tests/inputs
python3 -B worldgen/tests/reference/p3_data_reference.py check
python3 -B worldgen/tests/reference/p3_names_reference.py check
python3 -B worldgen/tests/reference/p3_names_reference.py scan          # the 59,400-name rates of 14.2 and 14.3
python3 -B worldgen/tests/reference/p3_chronology_reference.py check
python3 -B worldgen/tests/reference/p3_engine_reference.py check --inputs $I
python3 -B worldgen/tests/reference/p3_peoples_reference.py check --inputs $I
python3 -B worldgen/tests/reference/p3_conflict_reference.py check --inputs $I
```

- `p3_data_reference.py`: `SPEC_FILES_020` (race `awakening_population` after `hidden_years`, orc 1200, others 0;
  the faerie lists of P3-02 14.1; the entries of kinds 39 and 40 after `quiet_stretch`), `NAME_FILTER_EDITS_020`
  (14.3), `pre_020` and `name_filters_pre_020` (the disk's version), `P3_11_BASE_COMMIT` dfa4e66 (the 0.19.0
  `name_filters/` read from git when 0.19.0 is served over 0.20.0 files), `check_spec_020`, the filter sizes
  33/18/5, 71/12/14, 5/1/391 (effective 109/31/511).
- `p3_names_reference.py`: `CULTURES_R15` (the faerie lists), `REPEAT_RULE`, `syllables` and `repeated` (14.2:
  boundaries at the first symbol, before O, before N not after O, at separators; s_i and b_i; repeated when s_i is
  not empty and s_i = s_{i+1} or b_{i+1}); the attempt's `flags` gain `repeated`, `drawn`, `r4valid`, `word`. Golden
  lines `rrow` (14.2's nine rows and 14.4's OONC, each in a test culture made of the row's strings, with the first
  (e, t) that draws exactly them), `rattempt`, `rresult` (culture `rep`: Ba valid, Baba repeated; the fallback),
  `sattempt` (shipped cultures, seed 42) and `sample15` (5.1's protocol at 0.20.0 for the faeries,
  `SAMPLES_R15`). `check_r15`; the 11.6 tables at 0.20.0 allow only the faerie row and names whose 0.19.0 attempt
  0.20.0 rejects to change (`old_name_rejected`); 11.7 item 7's 0.16.0 hashes are checked with the rule off
  (`REPEAT_OFF`), since the library applies 14.2 to every culture and the `r4sha256` golden lines move.
- `p3_engine_reference.py`: `COMPILED_020` (kinds 39, 40), `RENDER_R17` (`fmt_value`: 1 to 9 as words in `{value}`
  and the three-word form's `#`), `TEMPLATE_CASES_R17`, `check_render_r17`; r10's grammar checks run without r17.
- `p3_peoples_reference.py`: flag `awaken_pop` (`World.awakening_population`; P3-05 r17 23.2 item 2: the ordered set
  `first_colony_pending`, filled at an awakening of a race with the field above 0, read in 9.3 item 1 for the
  capital, cleared at the civilization's first `colony_founded` and in `end_civ`; `colony_log` records each colony's
  source pressure and whether the founder was pending), `check_units_r16` (r17 23.4 items 1 to 3, 6, 8, 10: the
  formula with C set by hand; a village of 1,200 at share 4,700 has pressure 255, is ready only while pending, its
  colony of 144 clears the mark, and the next T2 takes no found draw; lying low skips first; every race at 0 equals
  the 0.19.0 rule on the synthetic worlds; invariant 3 at every awakening with 100,000; every colony ready by
  pressure or the first of a pending civilization).
- `p3_conflict_reference.py`: flags `claim39` (23.2) and `foot40` (23.3); `CLAIMS` and `CEDES` read kinds 20/39 and
  29/40 alike in every invariant, statistic, war line (`-foothold`) and note; `history_note` gains P3-07 r14 23
  items 3 and 4; `replay_r17` checks 23.7 item 3 and 23.2's holder; `check_self_r19` (23.7 item 5) on every
  synthetic and stress world; `check_units_r19` (23.7 item 1 on W1); one golden line per case
  `<case> r19 t11 ... first_orc_war ... claims39 a/t/n cessions40 n goal_differs n`.

Readings taken (also in the hand-back report):
- 23.2's settlement list is the holder's standing visible settlements **not hidden** (`svis_seen`, the set that puts
  R in presence(b)), as 23.7 item 1's test and decision 4 say; the rule text says only "visible".
- 5.2's `kind_cap` of a non-fortress settlement is the top of the race's ladder (orcs: town, 7,999), so an orc
  village of 1,200 has pressure max(1,200,000 div C_s, 150), not 800 as P3-05 r16 23.1 and 23.2 computed with a cap
  of 1,499. P3-05 r17 (spec commit e361880) corrects it with the pending mark above; the goldens follow r17.

First integration with `p3/11-impl` at e7b9988 (r16, before the correction; branch `p3/11-int`): the C++ matched
every r16 reference golden and `check-world` on the 1024 default worlds of seeds 0 to 13 and 73; T11 on those 15
worlds was 120, 149, 149, 140, 52, 121, 164, 153, 119, 188, 175, 112, 92, 151, 164 (median 149), which is what led
to r17.

## Generator 0.21.0 (P5-01: P1-06 r10 16, P3-01 r15 3.8, P3-02 r17 15, P3-04 r19 22, P3-05 r18 24, P3-06 r22 24, P3-07 r19 25)

Written from `docs/specs/p5-01-followups.md` and the stage-spec sections it names, without the engineer's
branch. Every reference serves 0.21.0 by default; `--version 0.20.0` serves the 0.20.0 rules and data (the
copied kinds `placenames/` and `name_filters/` read from git at f29deec, `P5_01_BASE_COMMIT`; the race, culture
and history listings of 0.20.0 stand in for the disk's 0.21.0 files) and reproduces the committed 0.20.0 goldens
byte for byte (checked for `p1-06-regions.txt`, `p3-01-data.txt`, `p3-02-names.txt` (but for one header comment
line the committed file predates), `p3-03-chronology.txt`, `p3-04-engine.txt`, `p3-05-peoples.txt`,
`p3-06-conflict.txt`). The data edits of the spec are applied in this branch's `game/data` and both fixture roots
exactly as the spec states them (P1-06 16.1, P3-02 15.1 and 15.2, P3-05 24, P3-06 24.6, P3-01 3.8); the
engineer's files must hold the same values (the content hash reads values, not layout).

The history inputs of 0.21.0 carry P1-06 r10's region names: the P3-11 inputs renamed with
`p1_regions_reference.py rename-db SRC DST` (it now accepts an input that stores r7's names on the 0.19.0
lists), in `~/.cache/the-unmapped/out/p5-01-tests/inputs/` (with `READY`).

```
I=~/.cache/the-unmapped/out/p5-01-tests/inputs
export UNMAPPED_ELEVATION_CACHE=~/.cache/the-unmapped/out/p1-06-tests/elev-cache
python3 -B worldgen/tests/reference/p3_data_reference.py golden
python3 -B worldgen/tests/reference/p1_regions_reference.py golden --large --db-dir ~/.cache/the-unmapped/out/p1-06-tests/db
python3 -B worldgen/tests/reference/p3_names_reference.py golden
python3 -B worldgen/tests/reference/p3_chronology_reference.py golden
python3 -B worldgen/tests/reference/p3_engine_reference.py golden --inputs $I
python3 -B worldgen/tests/reference/p3_peoples_reference.py golden --inputs $I
python3 -B worldgen/tests/reference/p3_conflict_reference.py golden --inputs $I
```

- `p3_data_reference.py`: `SPEC_FILES_021`, `KINDS_021` (history.conflict 61 fields, history.engine with
  `flare_min_years`), `CONFLICT_CHECKS_021` and `conflict_check_messages`'s `conflict_toll_spread`,
  `engine_check_messages` with `engine_flare_min` (per level, sparse, normal, dense), `GRIM_MOVED_021`,
  `NAME_FILTER_EDITS_021`, `DWARVEN_FORBIDDEN_021`, `placenames_021`, `check_spec_021`, `check_placenames_021`.
  Filter sizes slurs 33/17/5, crude 71/12/15, famous 0/0/103 (P3-02 15.1 writes 105; 101 + shiva + hindi is
  103), effective 109/30/514.
- `p1_regions_reference.py`: `VERSION` 0.21.0 (`r10()`); the lists only; `check_rg11_r10`; the r7 golden guard
  also refuses a `table_region` that moves in a case without drawn names (16.3). 28 cases move.
- `p3_names_reference.py`: `CULTURES_R17` (dwarven forbidden + didin, ririn); golden lines `dattempt`,
  `dresult`, `dresult15`, `dvalid`; `check_r17`; `scan` at 0.21.0 prints the per-culture rejections by each new
  entry (`scan --r15` the P3-11 scan).
- `p3_engine_reference.py`: `COMPILED_021` (kinds 41, 42), `RENDER_R19` (`template_pieces`,
  `clause_of_placeholders`, `clause_digits`: 22.1's clauses), `FLARE_R19` (lengths' `M`, SC6 skips a flare-up
  shorter than M; `Engine.flares_skipped`), the entry check `engine_flare_min`, `TEMPLATE_CASES_R19`,
  `check_render_r19`, `check_flare_r19` (22.4 items 1 to 3). The `ForcedTension` cases whose flare-ups were
  shorter than the scaled minimum lose them: `sched-0-2000-normal-goblin,dwarf,human-ft1`,
  `sched-1-6000-normal-all-ft1`, `sched-42-12000-normal-all-ft1`, `sched-18446744073709551615-2000-normal-all-ft1`,
  `sched-18446744073709551615-12000-normal-all-ft1` (22.4 item 5).
- `p3_peoples_reference.py`: catalogue of 42 kinds, the became_city template; no rule changes.
- `p3_conflict_reference.py`: flags `fresh` (24.1, `World.fresh_claim`), `strength` (24.2, `World.strength` in
  `held_back`), `dropped` (24.3), `hidden42` (24.4), `toll` (24.5, `World.battle_tolls`, stream
  `hist.conflict.toll`); `MAX_PARTICIPANTS` 62 (P3-04 r19 22.2; no world reaches it, the 0.20.0 goldens are
  unchanged); invariants (18) to (21) (`r22_violations`, `check_invariants_r22`), the open claims at E from the
  state and from the rows (`open_claims_at_end`, `open_claims_from_rows`), `hidden_refuges_at_end`, the self-check
  of 24.8 item 1 (`check_self_r22`, on every synthetic and stress case), the hand-made units of 24.8
  (`check_units_r22`), the golden lines `<case> r22 fresh <20>/<39> dropped <n> refuge_hidden <n> repeated_tolls
  <n> mixed_tolls <n>`, `<case> r22 open_claims <e..|->`, `<case> r22 hidden_refuges <s..|->`. The units of r14,
  r17 and r19 run on the 0.20.0 rules (`rules("0.20.0")`). Readings in `gaps` (`r22 ...`).
- `p3_report_reference.py`: format v2 (P3-07 r19 25.1 to 25.10) by default, v1 with `--version 0.20.0` (reproduces
  the committed 0.20.0 `p3-07-report.txt`); fixture H11 (`h11-v2.toml`, the cases of 25.13; "H9 v2" in the spec,
  H9 being r17's); `check_fixture_facts_v2`. The world cases of the golden are 0.21.0 worlds built by the
  references, since no 0.21.0 C++ exists yet:
  `p3_report_reference.py build-inputs ~/.cache/the-unmapped/out/p3-07-tests/inputs
  ~/.cache/the-unmapped/out/p5-01-tests/report-inputs` (P1-06 r10 names, the chronology and history tables of
  the references, meta generator_version 0.21.0 and game/data's content hash), then `golden`. The history of
  `geo-seed-42-512` so built equals `p3-06-conflict.txt`'s table hashes. At integration the C++ worlds of
  `world-hashes.sh` must give the same reports.

Integration (P5-01 phase 2, against the engineer's branch):
- Reference defect fixed: `p3_conflict_reference.py`'s `Engine.run_quiet` (the override that asks the tension in
  Q5) did not apply P3-04 r19 22.3 (it copied SC6 from before r19), so flare-ups shorter than m survived in the
  conflict and report worlds (`geo-seed-42-512` e335 "one year of upheaval", `hist-conflict-turbulent-512`,
  `stress-7`, report world `civs4-seed-12-1024`). After the fix `p3-06-conflict.txt` and, with the report inputs
  rebuilt by `build-inputs`, `p3-07-report.txt` were re-recorded; no other case moved.
- Fixture H11: famine and plague events with one value of two, and a sack without an attacker under a kind whose
  attacker role had min 1; both are corrupt worlds for the C++ view (P3-04 9.3). The fixture now gives both values
  and the sack kind an optional attacker; `expand_fixture` refuses a wrong value count.

## Generator 0.22.0 (P5-12: regions renamed by history; P1-06 r11 17, P3-01 r16 5.4, P3-02 r19 16, P3-04 r21 23, P3-07 r21 26, P4-01 r3 16)

Written from `docs/specs/p5-12-renamed-regions.md` r0 and the sections it amends, without the engineer's branch.
Every reference serves 0.22.0 by default; `--version 0.21.0` serves the 0.21.0 rules and data and reproduces the
committed 0.21.0 goldens byte for byte (checked for `p1-06-regions.txt`, `p3-01-data.txt`, `p3-02-names.txt`,
`p3-03-chronology.txt`, `p3-04-engine.txt`, `p3-05-peoples.txt`, `p3-06-conflict.txt`, `p3-07-report.txt`; the
0.21.0 inputs and goldens of an older version are read from git at 0b78b31, `P5_12_BASE_COMMIT`). The data edits
(`game/data/history/renaming.toml` and both fixture copies, the kind 43 entry of `history/report.toml`, human
`forbidden` + `ass`) are applied exactly as the spec states; the engineer's files must hold the same values.

Inputs: the P5-01 inputs renamed with `p1_regions_reference.py rename-db SRC DST` (V8: only `geo-phrase-512`
moves, region 11 `Tarden` -> `Lichenknoll Forest`) in `~/.cache/the-unmapped/out/p5-12-tests/inputs/` (with
`READY`); the report inputs rebuilt with `p3_report_reference.py build-inputs ~/.cache/the-unmapped/out/p3-07-tests/inputs
~/.cache/the-unmapped/out/p5-12-tests/report-inputs` (adds table `region_name`, schema 8, when the source lacks it).

```
I=~/.cache/the-unmapped/out/p5-12-tests/inputs
export UNMAPPED_ELEVATION_CACHE=~/.cache/the-unmapped/out/p1-06-tests/elev-cache
python3 -B worldgen/tests/reference/p3_data_reference.py golden
python3 -B worldgen/tests/reference/p1_regions_reference.py golden --large --db-dir ~/.cache/the-unmapped/out/p1-06-tests/db
python3 -B worldgen/tests/reference/p3_names_reference.py golden
python3 -B worldgen/tests/reference/p3_chronology_reference.py golden -   # (redirect to the golden file)
python3 -B worldgen/tests/reference/p3_engine_reference.py golden --inputs $I
python3 -B worldgen/tests/reference/p3_peoples_reference.py golden --inputs $I
python3 -B worldgen/tests/reference/p3_conflict_reference.py golden --inputs $I    # about 9 minutes
python3 -B worldgen/tests/reference/p5_12_renaming_reference.py golden             # about 10 minutes
python3 -B worldgen/tests/reference/p3_report_reference.py golden
```

- `p3_data_reference.py`: `KINDS_022` (history gains `renaming`: `RENAMING_022`, `RENAME_RULE`), the listing
  `RENAMING_RULES_022` transcribed from 4.1, `HISTORY_FILES_022`, `CULTURE_FILES_022`, `KIND43_ENTRY`,
  `rename_form_ok` (grammar `rename_form`), `renaming_check_messages` (the four named checks), `check_spec_022`;
  at 0.22.0 every check of the 0.21.0 listings runs on them first (`check_spec_invariants_upto_021`).
- `p1_regions_reference.py`: `VERSION` 0.22.0 (`r11()`); `v8_spans(parts, filt)` (17.1) after V7 in the drawn
  attempts and in the fallback; `check_rg11_r11` (17.3's units); `V8_REJECTED`. On the 100 default 1024 worlds
  of the 0.21.0 sweep exactly four names change (seeds 15, 59, 84, 91; P1-06 17.2's four). In the golden file
  `shipped-512-phrase` moves (`Tarden`), against 17.3's "expected none".
- `p3_names_reference.py`: `CULTURES_R19`, `DOUBLE_RULE`, `fused_tails`, `first_syllable`, `doubled_item`
  (16.1 items 1 to 3), `check_r19`, golden lines `tails`, `fs`, `doubled`, `hreject`, `hresult` (seeds 0 and 42,
  human settlements 1 to 400). The 11.6 sample's human settlement 2 `Alantton` is redrawn (`Serd`).
- `p3_engine_reference.py`: `COMPILED_022` (kind 43), `RENDER_R21` (N(r, e) through the view's `region_at`,
  `{@region_former}`, `region_at_event_fn`, `region_at_year`), schema 8 (`COLUMNS_022`, `TABLES_022`,
  `REGION_NAME_DDL`), `[history]` `region_names` after `relations`, `TEMPLATE_CASES_R21`, `check_render_r21`.
- `p3_peoples_reference.py`: `COMPILED_022`; no rule; the peoples-only configuration installs no renaming.
- `p3_conflict_reference.py`: flag `rename` (0.22.0): `World.renaming` (the subsystem of
  `p5_12_renaming_reference.py`) runs after conflict in T6 and Q4; `res["region_name"]`; inputs read `landmass`
  and `biome` of table `region`; `unit` gains the renaming companions (P3-07 r21 26.5); a note names a region as
  in its event's sentence (`_rname`, reading); golden line `<case> r21 renamings ...`.
- `p5_12_renaming_reference.py` (new): `Renaming` (section 6), `land_nouns` (RG11 3 from rows), `realm_root`
  (5.3), `build_name`, `name_problem` (5.2), `stage_check_message` (P3-01 r16 5.4), `rows_violations`
  (invariants 1 to 5), fixtures `rename-small` (stress data, every `min_magnitude` 1, `chance_permille` 1000,
  caps opened) and `off` (`region_max` 0), `check` (units of 13 on a stub world) and `check-world DB...`.
  Readings: `readings`.
- `p3_report_reference.py`: format v3 (`V3`): header, `hidden refuges: <h> of <n> founded by flight` (26.3, the
  `refuge_hidden` of smallest id), `renamed regions:` (26.2), present names outside sentences (26.4), the four
  `[stats]` keys and `SUMMARY_STATS_022` (26.5); fixture DDL from `schema.md` at the current version.

### 0.22.0, phase 2 (integration with the engineer's branch)

- `p4_timeline_reference.py`: rule set `p5-12` (the default; `--rules p4-03` keeps the 0.21.0 golden, `--rules p4-01`
  the 0.20.0 one): the static dump S gains `name <region> <from_year> <name>` per `region_name` row after the
  `region` lines (P4-01 r3 16 item 4), `History.region_name(r, Y)` (16 item 1, a scan of the rows); world cases
  from `~/.cache/the-unmapped/out/p5-12-int/inputs` (0.22.0 worlds by `worldgen-cli generate`, the arguments of
  `tools/ci/world-hashes.sh`, and `sample-42-1024`). `p4-01-timeline.txt` re-recorded from it: every world case
  moves (0.22.0 histories), the fixtures' `table meta` (schema 8, 0.22.0) and a `table region_name` line each, and
  H12.
- `p3_report_reference.py`: fixture H12 (`h12-v3.toml`, P3-07 r21 26.8 item 9) in `FIXTURE_NAMES_V3`; fixture
  rows of `region_name` (`[region, from_year, name, culture, event]`). `p3-07-report.txt` gains H12's lines only.
- `p3_conflict_reference.py` `check`: invariant 8/14.2 admits `region_renamed` in a quiet step (P5-12 6: Q4);
  `hist-conflict-flight`'s "flights and found refuges" is printed, not checked, at 0.22.0 (seed 1 at 512 has no
  flight once the renamings shift the event ids; P5-12 6 "Effect on the rest of history").
- After validation (P5-12 validation report, findings A and B; the test-side fixes of branch `p5/12-fix`):
  `p3_report_reference.py` names a region in a history note as at the noted event (`note_region`, P3-07 r22
  26.4; reading R27): the claim note of a declaration printed the present name, which 17 of the 185 validation
  worlds showed; it now agrees with the C++ on all 17. A renaming with no cause prints its renamed-regions line
  without the `after` part (r22 26.2). `p3-07-report.txt` is reproduced byte for byte (no golden case has either).
  `p3_conflict_reference.py check-world` reads schema 8 at 0.22.0 and schema 7 with `--version 0.21.0` or older.
  The test's own RG11 (`p1_06_regions_names.hpp`) gains V8 (P1-06 r11 17.1) from the data root's `name_filters`,
  in the drawn attempts and the fallback.
- The C++ synthetic geographies (`p3_05_peoples_support.hpp` `inputs_of`) carry `landmass` 1 and the dominant
  dry biome of `region_biome` in their region rows, as `land_nouns` reads them without `region_land` (the lead's
  ruling 3); no peoples or conflict rule reads those columns.

## Generator 0.23.0 (P5-02: beasts, monsters and sites; abandoned places; P5-12 carry-overs)

Written from `docs/specs/p5-02-beasts-monsters.md` r0 (and the sections it amends) without the engineer's branch.
Every reference serves 0.23.0 by default and keeps `--version 0.22.0` (for the timeline `--rules p4-03a`), which
reproduces the committed 0.22.0 goldens byte for byte (checked before the change for `p3-01-data.txt`,
`p3-02-names.txt`, `p3-04-engine.txt`, `p3-05-peoples.txt`, `p3-06-conflict.txt`, `p3-07-report.txt`,
`p5-12-renaming.txt`; the files of the base are those of `73fc624`, `P5_02_BASE_COMMIT`). The data of 4.1 to 4.5,
9.3, 10.2 and 21.2 are written into `game/data` and both fixture roots exactly as the spec states them, with one
reading (D2 below): kinds 48 and 49 read `{@settlement}`.

Inputs: `~/.cache/the-unmapped/out/p5-02-tests/inputs/` (the P5-12 inputs, geography only, plus
`p5-monsters-high-1024.db`, `generate --seed 7 --map-size 1024 --monster-pressure high --until regions` with the
0.22.0 CLI and a 0.22.0 data root: geography does not change at 0.23.0); the report and timeline inputs
`~/.cache/the-unmapped/out/p5-02-tests/report-inputs/`, rebuilt with `p3_report_reference.py build-inputs
~/.cache/the-unmapped/out/p3-07-tests/inputs ~/.cache/the-unmapped/out/p5-02-tests/report-inputs` (schema 9: the
settlement statement replaced, the five tables added, history recomputed by the conflict reference).

```
I=~/.cache/the-unmapped/out/p5-02-tests/inputs
python3 -B worldgen/tests/reference/p3_data_reference.py golden
python3 -B worldgen/tests/reference/p3_names_reference.py golden
python3 -B worldgen/tests/reference/p3_engine_reference.py golden --inputs $I
python3 -B worldgen/tests/reference/p3_peoples_reference.py golden --inputs $I                 # about 1 minute
python3 -B worldgen/tests/reference/p3_conflict_reference.py golden --inputs $I                # about 5 minutes
python3 -B worldgen/tests/reference/p5_12_renaming_reference.py golden --inputs $I             # about 10 minutes
python3 -B worldgen/tests/reference/p5_02_monsters_reference.py golden --inputs $I             # about 12 minutes
python3 -B worldgen/tests/reference/p3_report_reference.py golden
python3 -B worldgen/tests/reference/p4_timeline_reference.py golden
```

After the engineer's calibration (17 step 9) the rates are read from the data roots: edit the listings
`BEASTS_023`, `MONSTERS_KINDS_023` and `MONSTERS_TUNING_023` of `p3_data_reference.py` (the one place; `check`
compares them with the files), the C++ listing `p3_01_history_data.hpp` (generated from the files) and the tables of
`history/history_monsters_data_test.cpp`, then run the commands above; each golden is one command.

- `p3_data_reference.py`: `KINDS_023` (new kinds `beast` (`BEAST`), `monster` (`MONSTER`), history `monsters`
  (`MONSTERS_023`), culture `site_forms`, peoples `ghost_*`), the listings of 4.1 to 4.5, `RENAMING_RULES_023`,
  `REPORT_ENTRIES_023`, `KIND43_CAUSES_023`, the grammars `display_name`, `beast_plural`, `monster_noun`,
  `name_form` and `rename_form` with `{stem}`, the named checks with their messages (`beast_check_messages`,
  `monster_check_messages`, `monsters_check_messages`; the messages 4.2 and 4.3 do not give are this reference's,
  reading A3), `pre_023`, `check_spec_023`. The beast and monster files have no comment line (headers at line 1).
- `p3_names_reference.py`: `CULTURES_R23` (site_forms), `STREAMS_023` (`hist.names.monster`, `hist.names.site`),
  `with_forms` (a monster's D is its naming culture's, F its kind's name_forms), `monster_name`, `site_name`,
  golden lines `mattempt`, `mformtext`, `formtext <culture> site`, `mname`, `sname`, `msha256` (`r23_golden_lines`).
- `p3_engine_reference.py`: `KINDS_023`, `COMPILED_023`, `RENDER_R23` (names of types 8 and 9, form `kind`,
  `{@beast}` and their `template_problem` messages), schema 9 (`COLUMNS_023`, `SETTLEMENT_DDL_023`, `DDL_023_NEW`,
  `ddl_023`), `settlement_ending` / `endings_from_rows` (8.3, a faded row through its `ruin_faded` cause),
  `beast_rows`, `monster_kind_rows`, the `[history]` keys of 11.3, `TEMPLATE_CASES_R23`. Stub worlds: beast and
  monster_kind rows written, the other three tables empty (reading S1).
- `p3_peoples_reference.py`: flag `ghost` (8.2 in the strike and in the outbreak's plan), `set_wild`,
  `wild_growth` (7.1's formula), `shunned` (7.2: no inside or outward target), stream `hist.peoples.ghost`,
  `rows_of_023`. The peoples-only configuration installs no monsters (no view: G = 1000).
- `p3_conflict_reference.py`: flags `monsters` (the subsystem after renaming in T1, T4, T6, Q1, Q4; `tension_all`,
  the larger of the two sources, conflict's on a tie; `finish` at E - 1) and `flare_claim` (7.4: the forced
  declaration only for a `claim` or `claim_settlements` trigger), the monster flare-up note (12.7) in
  `history_note`, the schema 9 gate of `check-world`, invariant 8 admitting the monsters' flare-ups, `HEADER_023`.
- `p5_02_monsters_reference.py` (new): `Monsters` (sections 5 and 6), `WildView`, `GeoNb` (GeoView neighbours =
  every region_border row, reading G1), `fill_form` (21.2's possessive), lair names (9.2), `invariant_violations`
  (14's invariants 1, 3 to 11 on rows, also on a world file: `check-world DB`), `res_violations` (with invariant
  2), `i0_differences` (7.5's I0: the zero tuning with the subsystem against the configuration without it, the
  history tables equal; on the stress fixtures and the 512 inputs), `check_units` (19.1's units on stub worlds of
  `p3_conflict_reference.unit_world`: habitat and capacity, the yearly step at B < K, B > K, B = K, B = 0, d capped,
  g at the caps, Q1 = L years of T1, the rows at year 0, a phase change and finish, boldness's hysteresis and toll,
  the caps, chance_ppb at 0, 1, 10^9, islets, the quiet budget, pending_flare, tension pure, the undead window, raid
  targeting and toll, the hunt odds, each overrun condition and its effects, the shun lifted, outposts at war on the
  height, one a year, the quiet ending), fixtures `monsters-small` (`monsters_small_data`: the conflict stress data,
  every wake_rate x 2000, living_permille 1000, overrun_permille 1000; in-memory, reading X1) and `zero`
  (`zero_data`), `golden` (`p5-02-monsters.txt`). Readings: `readings`.
- `p5_12_renaming_reference.py`: `--version` (default 0.23.0), `{stem}` (`stem_value`), the possessive rule
  (`form_pieces`, `fill`), a monster `name_role` (the monster's root through `Monsters.monster_root_of`), the stage
  check admitting a monster role, `rename-small` = monsters-small + the renaming edits at 0.23.0, `HEADER_023`.
- `p3_report_reference.py`: format v4 (`V4`): header, references of types 8 and 9, the ruins list (`overrun by`,
  `after plague`, `after famine`, `then called`), the `beasts:` line (full), `[monsters]` (`SUMMARY_MONSTER_LINES`
  2), `[sites]`, the `[stats]` keys of 12.6 (full only), fixture H13 (`h13-v4.toml`), fixture DDL at schema 9 (the
  spec's statements while `schema.md` holds 8), `build-inputs` to schema 9.
- `p4_timeline_reference.py`: rule set `p5-02` (default; `--rules p4-03a` keeps the 0.22.0 file): `site_at`,
  `monster_at`, `beast_count`, `sites_existing`, the settlement ending at Y; S gains `site`, `monster`, `beast`
  lines after the settlement lines and D(Y) the open sites and living monsters (reading P5: 13 gives no line
  format), inputs the report inputs above.

Goldens re-recorded and why each moved: `p3-01-data.txt` (the new kinds, fixed id and fields: content hashes),
`p3-02-names.txt` (the two streams' lines and the r23 section; no line of the existing kinds moves),
`p3-04-engine.txt` (the catalogue of 55 kinds, the five tables and settlement's ending, the `[history]` keys, the
template cases; no sentence or schedule moves), `p3-05-peoples.txt` (the ghost rule: the cases whose histories hold
a famine or plague on a small place move from that event on; settlement.ending), `p3-06-conflict.txt` (every
history: the monsters subsystem, outposts, the renaming forms), `p5-12-renaming.txt` (every history; `ravage`
renames in the rename-small fixtures), `p3-07-report.txt` (v4, every report), `p4-01-timeline.txt` (every case:
dumps of schema 9), and the new `p5-02-monsters.txt`. Not touched: `core_vectors.txt`, `p1-*`,
`p3-03-chronology.txt`, and the world-hash files `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt` (validation's).

## P3-07 history report

`p3_report_reference.py` reproduces `worldgen-cli report --history` at both levels from a world file
(sqlite3 only), written from `docs/specs/p3-07-history-report.md`; it reuses the sentence renderer of
`p3_engine_reference.py` and the notes, units and summary set of `p3_conflict_reference.py`. It builds its
own SQL copy of each fixture in `worldgen/tests/data/history-report/` and writes
`worldgen/tests/golden/p3-07-report.txt` (`golden`, world cases from `--inputs DIR`, generated by
`worldgen-cli generate` as `tools/ci/world-hashes.sh` does). `compare DB SUMMARY FULL` compares the C++
texts with its own byte for byte; `check` runs its self-checks. The readings it chose are listed in its
`READINGS` string.

The table hash of `detail_tables` encodes a BLOB with the tag `0x04` of P0-04 8 (fixed with P4-01: it used
`0x03`, which no P3-07 fixture exposed since none has a layer; `p3-07-report.txt` is unchanged).

## P4-01 timeline

`p4_timeline_reference.py` computes, from a world file (sqlite3 only), the timeline of
`docs/specs/p4-01-timeline.md` by the definitions of its section 4 applied to the stored rows (linear scans,
no index of section 6): the canonical dumps S and D(Y) of 13.1, the civilization colours of 5.5 and the
territory image of 5.6. It reuses `p3_report_reference.py` (`connect`, `pk_rows`, `detail_tables`,
`expand_fixture`, `ddl_statements`, `write_fixture`), `p0_world_check.py` (`decode_blob`,
`encode_delta_varint`) and `core_reference.py` (`hash_words`, `pack_bytes`, `tag_value`). It holds the first
Python rendering of the composites: `map` steps 1 to 4 (P1-00 10.2), `biomes` (P1-04 9) and `regions`
(P0-04 amendment 18 (e)); with an empty fill it equals `worldgen-cli render --composite regions` of 0.20.0
on `geo-seed-42-512` and on T1's layers in every water, river and flow_dir variant.

```
python3 -B worldgen/tests/reference/p4_timeline_reference.py check [--quick]
python3 -B worldgen/tests/reference/p4_timeline_reference.py golden [PATH|-] [--inputs DIR]
python3 -B worldgen/tests/reference/p4_timeline_reference.py dump DB [--static | --year Y | --years]
python3 -B worldgen/tests/reference/p4_timeline_reference.py territory DB YEAR OUT.ppm
python3 -B worldgen/tests/reference/p4_timeline_reference.py fixture t1-edges OUT.db
```

`golden` writes `worldgen/tests/golden/p4-01-timeline.txt` from the P3-07 fixtures, fixture T1
(`worldgen/tests/data/timeline/t1-edges.toml`, the P3-07 format with `[[layer]]` entries) and the world
cases in `--inputs DIR` (default `~/.cache/the-unmapped/out/p4-01-tests/inputs`: the cases of
`tools/ci/world-hashes.sh` with history and `sample-42-1024`, generated by `worldgen-cli generate` at
0.20.0); it takes about four minutes. `check` runs the worked colour table, 5.6's example, T1's
hand-checked facts and pixels, the invariants of section 8 on the fixtures and, unless `--quick`, on the
input worlds with the golden file's currency. The readings it chose are listed in its `READINGS` string.

### P4-03 colours, holders and inner borders (generator 0.21.0)

Written from `docs/specs/p4-03-history-view.md` (sections 7, 8, 9.2, 9.4, 9.6, 11.1) without the engineer's
branch. The default rules are P4-03's: the table of 7.2 (`TABLE_P403`) and `race_civilization_colour` (keys
outside the table keep 5.5's hash base and `civilization_shade`), `territory_holders`, `TerritoryBase`'s
kind, east and south values and rule 8.2 in `Base.pixel` (`dim` = floor(c 12 / 16)), the distance rule of
7.3 (`distance`, `background_set`, `distance_table`; checked only, never used for output) and the texts of
9.4 (`control_run_error`). `--rules p4-01` gives P4-01's colours and image (holders ignored) and serves the
0.20.0 P3-07 fixtures (`p3_report_reference.use_version("0.20.0")`).

```
python3 -B worldgen/tests/reference/p4_timeline_reference.py check            # P4-03 rules, 0.21.0 inputs
python3 -B worldgen/tests/reference/p4_timeline_reference.py golden
python3 -B worldgen/tests/reference/p4_timeline_reference.py distances        # the "min" columns of 7.2
python3 -B worldgen/tests/reference/p4_timeline_reference.py --rules p4-01 golden -   # = the P4-01 file
```

`check` adds: the table of 7.2 for k = 0 ... 7, the hash rule for `troll`, the empty key, `Elf` and `elf `;
rules C1 to C3 over the 24 entries against B (the ocean ramp for t = 0 ... 30000, the lake colour, the six
river colours, the twelve biome colours of `game/data/biomes/`, confirmed equal to every input world's table
`biome`), the "min" columns of 7.2, the worst pair (goblin 1 / steppe, 88), the closest pair across races
(dwarf 1 / orc 1, 90) and the four distances 7.1 quotes for P4-01's colours (20, 26, 7, 44); 8.2's 4 x 2
example; the 8 x 6 layout of `timeline_borders_test.cpp` (`check_inner_borders`); the A3 cases of 11.1 item 7
on T1's rows; and that rule 8.2 dims at least one cell of the 512 world cases at E (it does: the four
512 world cases dim 1 299 cells in all at 0.21.0). The readings are `READINGS` A3a, A3b and P1 to P5.

Inputs at 0.21.0: `~/.cache/the-unmapped/out/p4-03-tests/inputs` (the new default), the eight world cases
generated as P4-01 did, by `worldgen-cli generate --data game/data` with the arguments of
`tools/ci/world-hashes.sh` (sample-42-1024: `--seed 42`), here with the 0.21.0 CLI of the P5-01 integration
branch (`~/.cache/the-unmapped/build/p5-01-int/linux-gcc-release/`), used for nothing but these inputs. All
26 tables of each (`hash --detail`'s table lines, layers, meta and every history table included) equal those
of the reference-built worlds of `~/.cache/the-unmapped/out/p5-01-tests/report-inputs/` (P5-01 above).

Re-recording of `worldgen/tests/golden/p4-01-timeline.txt` (P4-03 9.6), in three steps:

1. P4-01's rules on the 0.20.0 inputs (`--rules p4-01 golden`, inputs `p4-01-tests/inputs`): byte for byte
   the committed P4-01 file.
2. P4-03's rules on the same 0.20.0 inputs, the P3-07 fixtures still at 0.20.0: against the committed file
   exactly the 21 `static` lines (every case: all 21 have civilizations of shipped races, so their `colour`
   lines change) and the 24 `territory` lines move, and the header (two lines for one). No `year`, `years` or
   `table` line moves.
3. P4-03's rules on the 0.21.0 inputs (the committed file): against step 2, the world cases' `year`, `years`
   and `static` lines move (their histories are 0.21.0's: geo-phrase-512 20 year lines, geo-seed-0-512 14,
   geo-seed-1-1024 50, geo-seed-42-512 32, hist-dense-1024 64, hist-races-elf-orc-1024 10,
   hist-span-2000-512 16, sample-42-1024 49), and the territory line at E of every world case but
   hist-races-elf-orc-1024 (the holders at the first change year and at E / 2 are those of 0.20.0); and the
   `table meta` line of each of the 13 fixtures (meta `generator_version` 0.21.0). No fixture's `static`,
   `year` or `years` line moves between steps 2 and 3.

### P4-03a territory colour and border follow-ups (generator 0.22.0, no world change)

Written from `docs/specs/p4-03a-territory-colour-followups.md` r1 and P4-03 r4 (7.2, 7.3, 7.5, 8.2, 9.2) without
the engineer's branch. Rule set `p4-03a` is the new default; `--rules p5-12` keeps P4-03 r3's colours and image
on the same 0.22.0 inputs (`~/.cache/the-unmapped/out/p5-12-int/inputs`) and reproduces the golden file as P5-12
recorded it, byte for byte (checked before and after the change). `p4-03a` adds: the r4 table `TABLE_P403A` and
its "min" columns, `luma`, `dim` with n = 10 when L > 175 (`LUMA_BOUND`), `Base.half` (half(U) on kind-2 cells)
and rule 3 of 8.2 in `Base.pixel`.

```
python3 -B worldgen/tests/reference/p4_timeline_reference.py check --quick           # P4-03a rules
python3 -B worldgen/tests/reference/p4_timeline_reference.py golden                  # P4-03a rules, 0.22.0 inputs
python3 -B worldgen/tests/reference/p4_timeline_reference.py --rules p5-12 golden -  # = the P5-12 file
```

`check` adds: the four changed entries and that exactly those four differ from r3; the table of P4-03a 2.2
(each changed entry's nearest colour in B, in its race and in another race, with the nearest in B named), the
effects it names and the minima C1 89, C2 78, C3 90; L of 7.5 for all 24 entries, the n = 10 set {goblin 1,
faerie 1, goblin 0, dwarf 1, human 1, elf 1}, the 18 others between 51 and 163, the three pale r3 entries of
2.1; 8.2's r4 examples (one side free, all free on steppe (112, 105, 75), goblin 1 dim (146, 134, 56)); the
free-border fixture of `timeline_colours_p403a_test.cpp` (`check_free_borders`); T1's half-strength cells at
299, 1500 and 2000; the 8 x 6 layout of `timeline_borders_test.cpp` with regions 5 | 6 at half strength. The
readings are `READINGS` Q1 to Q4.

The re-record moved, against the P5-12 file, exactly: the two header lines; the `static` lines of the 18 cases
that have a civilization of key elf, goblin or human with ordinal k = 1 (mod 4) or of goblin with k = 3 (mod 4)
(every case but geo-seed-0-512, h10-floor, h2-minimal, h4-budget and h5-saturated), the set the rule of
P4-03a 5 (b) predicts; and 23 of the 24 `territory` lines (geo-seed-0-512 at 6000 has no free-free border, no
inner border of a realm with L > 175 and no cell of a changed colour, so its image is unchanged). No `year`,
`years` or `table` line moved.
