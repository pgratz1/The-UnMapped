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
