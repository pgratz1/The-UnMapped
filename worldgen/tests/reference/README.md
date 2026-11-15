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
`hash_at`, `hash_at3`, `phrase_seed` and the phrase text rules, `noise2`,
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
| `ERROR` | the call returns its error value (`parse_decimal`, phrase checks) |

Sampling vectors start from `Rng::from_state(S0, S1, S2, S3)` given as four
`0x` tokens, and give the result followed by the number of `next()` calls
consumed. The header of the vector file lists every kind with its argument and
result types.

## Readings chosen where the spec is open

- 7.3 `stream_key`: one length word, the one that the byte-string encoding
  of 7.2 already holds: `[seed, len, packed words...]`.
- 7.3 / 13.5 `phrase_seed`: `hash.phrase_seed` vectors hash the bytes exactly
  as given; `hash.phrase_text` vectors apply the text rules first (strict UTF-8,
  trim space/tab/CR/LF only, then 1 to 256 bytes).
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
`stream_key`, `hash_at`, `fbm2`, `mul_div` and `phrase_seed_text` (the text
rules of `seed_from_phrase`). It was used
to confirm the golden values in `worldgen/tests/golden/p0-worlds.txt` and the
`GOLDEN_RGB_*` constants of `worldgen/tests/inspect_render_test.cpp`.

```
python3 -B worldgen/tests/reference/p0_world_check.py check WORLD (--seed N | --phrase TEXT)
        [--map-size N] [--test-pattern] [--detail FILE] [--report FILE]
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
