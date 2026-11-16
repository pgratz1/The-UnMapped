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
python3 -B worldgen/tests/reference/p1_hydrology_reference.py golden [PATH|-] [--large]
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
