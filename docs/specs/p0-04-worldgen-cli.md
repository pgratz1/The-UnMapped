# P0-04 worldgen-cli

Status: approved by the lead, 2026-11-15. Amended 2026-11-19 for P3-03 (amendment 19: the chronology stage, schema version 6). Phase P0 Foundation. Design doc sections 3, 10 (conventions for the handoff, world viewer guardrails), 13 (P0 exit check). Depends on P0-01, P0-02, P0-03.

## 1. Purpose

`worldgen-cli` is the command-line front end of the generator: it writes worlds, exports layer images, prints reports and hashes files. It runs without Godot. In P0 it proves the exit check: an empty world file is byte-identical across GCC, Clang and MSVC, and `worldgen-cli hash` shows it in continuous integration.

Code location: `tools/worldgen-cli/`. It links `worldgen` and the vendored image writer (stb_image_write expected). No other dependency: argument parsing and SHA-256 are project code.

## 2. Inputs

Command-line arguments, the optional settings file and data directory (P0-03), world files (P0-02), and the environment variables `UNMAPPED_BUILD_ROOT`, `HOME`, `LOCALAPPDATA` (for the default output location only).

## 3. Outputs

World files, PNG images, report text and hash lines, at the locations below. Diagnostics on stderr.

## 4. Common conventions

### 4.1 Invocation

```
worldgen-cli <command> [--option value | --flag]...
worldgen-cli --help | --version
```

- The command is the first argument: `generate`, `render`, `report`, `hash`. `worldgen-cli <command> --help` prints that command's usage.
- Options are `--name value` (value is the next argument, even if it starts with `-`) or `--flag`. No `--name=value`, no short options, no positional arguments.
- An unknown command or option, a missing value, a repeated option, or a positional argument is a usage error (exit 2) with a one-line hint to use `--help`.
- `--help` prints usage to stdout and exits 0. `--version` prints `worldgen-cli <GENERATOR_VERSION> schema <SCHEMA_VERSION> sqlite <sqlite3_libversion()>` and a line feed, exits 0.

**Arguments on Windows.** The CLI obtains its arguments as UTF-16 (`wmain` or `GetCommandLineW` with `CommandLineToArgvW`) and converts them to UTF-8, because `main`'s `argv` on Windows is in the ANSI code page and would corrupt a non-ASCII seed phrase or path. All paths are handled as UTF-8 internally and converted with `std::filesystem::path` from UTF-8 (`char8_t`) strings.

### 4.2 Numbers on the command line

- Seeds: decimal `0|[1-9][0-9]*` up to 18446744073709551615, or `0x` followed by 1 to 16 hex digits (either case). Nothing else (no sign, no spaces, no underscores).
- Other integers: decimal, no sign unless the option allows negative values (none in P0), no leading zeros.
- No option in P0 takes a fractional value; when one does, it is parsed with `parse_decimal` (P0-01 section 5.6).

### 4.3 Output locations

CLAUDE.md: tools take explicit output paths and default to `out/` under the build root, never the current directory.

- **Build root** = `UNMAPPED_BUILD_ROOT` if set and non-empty; otherwise, on Windows, `%LOCALAPPDATA%\the-unmapped`; otherwise `$HOME/.cache/the-unmapped`. If none can be resolved and the command needs a default output, it is a usage error (exit 2) asking for `--out`.
- **Default output directory** = `<build root>/out/`, created (with parents) if missing.
- An explicit `--out` path is used as given (relative paths resolve against the current directory, because the user typed them). Its parent directory must already exist; otherwise exit 4. The CLI never creates directories other than the default output directory.
- No command writes into the current directory unless the user's `--out` says so.

### 4.4 Writing files

- Every output file is written to `<out>.partial` in the same directory and renamed to `<out>` only when complete. On failure `<out>.partial` is deleted.
- If `<out>` exists, the command fails with exit 4 unless `--force` is given, in which case it is replaced by the rename.
- Text output (stdout, stderr, report files) is UTF-8 with LF line endings on every platform. On Windows, stdout and stderr are switched to binary mode at start-up (`_setmode(_fileno(stdout), _O_BINARY)` and the same for stderr) so that LF is not converted to CR LF.
- The CLI never calls `setlocale` and never formats numbers through a locale.

### 4.5 Diagnostics

- Errors go to stderr as `worldgen-cli: error: <message>` followed by a line feed; loader errors use the P0-03 format, one per line, after that line.
- `--verbose` (accepted by every command) prints progress and timings to stderr. Timings never reach a world file, report text or hash output.

### 4.6 Exit codes

| Code | Meaning |
| --- | --- |
| 0 | Success |
| 1 | Internal error: the project fatal error (P0-01 section 4.1) or any other unexpected failure. A bug, by definition. |
| 2 | Usage error: bad command, option or value; missing seed; no default output location |
| 3 | Input error: a file missing or unreadable; invalid settings or data (P0-03); not a world file; unsupported schema version; corrupt layer; layer not found; a REAL value found by `hash --detail` |
| 4 | Output error: output exists without `--force`; parent directory missing; write or rename failed |
| 5 | Verification mismatch (`hash --expect`) |

## 5. `generate`

```
worldgen-cli generate (--seed N | --seed-phrase TEXT) [--out PATH] [--map-size 512|1024|2048]
                      [--settings FILE] [--data DIR] [--test-pattern] [--force] [--verbose]
```

| Option | Default | Meaning |
| --- | --- | --- |
| `--seed N` | from settings | The 64-bit world seed (4.2). |
| `--seed-phrase TEXT` | from settings | A text phrase, scrubbed and hashed by `seed_from_phrase` (P0-01 section 7.3; amendment 15). Exclusive with `--seed`. |
| `--out PATH` | `<build root>/out/world-<seed>.db`, seed in canonical decimal | Output world file. |
| `--map-size` | from settings, else 1024 | Map side in cells, stored as `param.map_size`. No P0 stage uses it. |
| `--settings FILE` | none | Settings file (P0-03 section 6). Flags override it key by key. |
| `--data DIR` | none in P0 | Data root (P0-03 section 7). |
| `--test-pattern` | off | Run the P0 test-pattern stage (5.2). |
| `--force` | off | Replace an existing output file. |

If no seed is given by flag or settings: exit 2 (`no seed given; pass --seed or --seed-phrase`). There is no random default seed; choosing one belongs to the create-world screen, which then passes it explicitly. (Interim choice.)

On success, stdout receives the absolute path of the written file and a line feed; nothing else.

### 5.1 What `generate` does

1. Parse arguments; load settings and data (P0-03); resolve the seed and output path.
2. Create the working database and schema (P0-02 section 6, steps 1–2).
3. Run the stages in order, recording each name for `meta.stages`. **P0 stages:** none by default; `test_pattern` with `--test-pattern`. **Later:** P1 adds the geography stages in design doc order (elevation, hydrology, climate, biomes, resources, regions); P3 the history stages; P6 derivation. Later phases may add `--until STAGE` to stop early; P6 wires the remaining world-creation settings to flags.
4. Write `meta` and finalize the file (P0-02 section 6, steps 4–9).

### 5.2 The P0 test-pattern stage

Purpose: exercise layer writing, the codec, location hashing, streams and noise end to end, so that cross-platform identity of those primitives is proved in P0 rather than discovered in P1. Stage name `test_pattern`. Writes three layers, each 256 × 256 regardless of `--map-size`, codec 1, in this order (ids 1, 2, 3), in one transaction:

| id | name | format | value at cell (x, y), 0 ≤ x, y < 256 |
| --- | --- | --- | --- |
| 1 | `test.ramp` | `u8` | (x + 2y) mod 256 |
| 2 | `test.hash` | `u8` | `hash_at(K_hash, x, y) >> 56`, with K_hash = `stream_key(seed, "test.hash")` |
| 3 | `test.noise` | `i16` | `mul_div(n.raw, 32767, 2^32)`, with n = `fbm2(K_noise, from_int(x) + HALF, from_int(y) + HALF, octaves = 5, base_freq = from_raw(134217728) (= 1/32), lacunarity = from_int(2), gain = HALF)` and K_noise = `stream_key(seed, "test.noise")`. Range [-32767, 32767]. |

Cells are computed in index order (y outer, x inner); since every value is location-keyed, the order does not affect results.

## 6. `render`

```
worldgen-cli render --db PATH --layer NAME [--out PATH] [--palette auto|gray] [--scale N] [--force] [--verbose]
```

| Option | Default | Meaning |
| --- | --- | --- |
| `--db PATH` | required | World file, opened read-only through the P0-02 reader. |
| `--layer NAME` | required | Layer name. Not found: exit 3, and the message lists the available layer names. |
| `--out PATH` | `<build root>/out/<db file stem>.<layer>.png` | Output PNG. |
| `--palette` | `auto` | `auto` picks the layer's registered palette; in P0 every layer's palette is `gray`. |
| `--scale N` | 1 | Integer 1 to 16; each cell becomes an N × N block (nearest neighbour). |

**`gray` palette.** Over all cells, min and max of the layer's values (compared as values of the element type). For each cell with value v: if max = min, g = 0; otherwise g = floor((510 × (v - min) + (max - min)) / (2 × (max - min))), computed exactly on integers (a 128-bit intermediate where the type needs it), which is (v - min) × 255 / (max - min) rounded half up. Pixel = (g, g, g).

**Image.** 8-bit RGB PNG, no alpha, width × scale by height × scale pixels, top row = y 0 (north), left column = x 0 (west), via the vendored image writer. No time, text or gamma chunks. The pixel buffer is the contract; the PNG's compressed bytes are not (they depend on the writer's version), so tests hash the RGB buffer produced by a library function (`render_rgb`, interim name) rather than the file.

**Later.** P1 registers per-layer palettes (hypsometric elevation, categorical biomes with colours from the biome definitions, river overlay) under `auto`; later phases add marker overlays. The palette list grows; `gray` stays.

On success, stdout receives the absolute path of the PNG and a line feed.

## 7. `report`

```
worldgen-cli report --db PATH [--out PATH] [--force] [--verbose]
```

Prints a deterministic text summary of the world to stdout, or to `--out` if given (interim: stdout is the default, since it is not a file write; say if a file under `out/` should be the default instead).

**Format v1 (P0).** UTF-8, LF line endings, no trailing spaces, ends with a line feed:

```
# worldgen report v1
[meta]
<key> = <value>                one line per meta row, in key order
[tables]
<table> = <row count>          one line per table, ascending name, excluding sqlite_* tables
[layers]
<name> <format> <width>x<height> codec=<c> bytes=<blob length> min=<min> max=<max>
                               one line per layer in id order, or the single line "(none)"
```

A meta row with an empty value is printed as `<key> =` with no trailing space. In values, a backslash is printed as `\\` and any control character (bytes 0x00–0x1F, 0x7F) as `\xHH` with upper-case hex; all other bytes, including UTF-8 sequences, are printed as they are. min and max are decimal integers.

Expected output for `generate --seed 0` (empty world):

```
# worldgen report v1
[meta]
content_hash = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
format = the-unmapped-world
generator_version = 0.2.0
param.map_size = 1024
schema_version = 1
seed = 0
seed_phrase =
stages =
[tables]
layer = 0
meta = 8
[layers]
(none)
```

For a test-pattern world the `[layers]` section begins `test.ramp u8 256x256 codec=1 bytes=<n> min=0 max=255`; the `bytes` values and the `test.hash` and `test.noise` min/max are recorded by the validation specialist.

**Later.** P1 adds per-layer statistics where useful; P3 adds the text history report (`--history`, with its own format version) that the world critic reads.

## 8. `hash`

```
worldgen-cli hash --db PATH [--detail] [--expect HEX] [--verbose]
```

**Algorithm: SHA-256** (FIPS 180-4), the project implementation in the core (P0-01 section 10). Chosen because it needs no new dependency, has standard test vectors, and lets anyone check a result with `sha256sum` or `Get-FileHash`.

**Default output:** the SHA-256 of the file's bytes, exactly as stored, as 64 lowercase hex digits and a line feed. The file is read as bytes and need not be a valid world; this is what CI compares.

**`--detail`** opens the file as a world (P0-02 reader; errors exit 3) and prints, after the file line, logical hashes that help locate a difference:

```
file <hex>
schema <hex>
table <name> <hex>             one per table, ascending name, excluding sqlite_* tables
layer <name> <hex>             one per layer, in id order
```

Definitions, all SHA-256 over byte streams built with these encodings: `u8`, `u32`, `u64`, `i64` little-endian; `str(s)` = u64 byte length then the bytes; a column value is a tag byte then its content — NULL `0x00`; INTEGER `0x01`, i64; TEXT `0x03`, str; BLOB `0x04`, str. A REAL value anywhere is an error (exit 3).

- **schema**: the rows of `sqlite_schema` ordered by `type`, then `name` (bytes); for each row, the columns `type`, `name`, `tbl_name`, `sql` as column values. (`rootpage` is excluded: it describes layout, not meaning.)
- **table `<name>`**: u64 number of columns, then every row in primary-key order (`ORDER BY` the primary-key columns, in key order, `BINARY` collation), each column in declared order as a column value. A table without a primary key is an error (it would violate P0-02 section 5.3).
- **layer `<name>`**: u32 width, u32 height, u8 element type code (P0-02 section 7.1), then the decoded elements in index order, each little-endian at its natural size. This hash is independent of the codec, so it is the per-stage golden value for layers.

**`--expect HEX`** (64 hex digits, either case): compare with the file hash. Match: print as usual, exit 0. Mismatch: print as usual to stdout, then `worldgen-cli: error: hash mismatch: expected <hex>, got <hex>` to stderr, exit 5.

## 9. CI exit check and golden hashes

Requested from the build engineer and the validation specialist; the CLI behaviour above is what makes it possible.

1. Every preset (Linux GCC, Linux Clang, Windows MSVC) builds `worldgen-cli` and runs the cases below, writing worlds under the build root.
2. For each case it runs `hash --db <file> --detail` and saves the output as a CI artifact named `<preset>-<case>.txt`.
3. Each job runs `hash --db <file> --expect <golden>` against `worldgen/tests/golden/p0-worlds.txt` once that file exists (lines `<case> <hex>`, sorted by case name, LF endings).
4. A final job compares the artifacts of all presets and fails on any difference. This checks cross-compiler identity even before golden values are committed, and the detail lines point to the table or layer that differs.

| Case | Command arguments |
| --- | --- |
| `empty-seed-0` | `--seed 0` |
| `empty-seed-1` | `--seed 1` |
| `empty-seed-42` | `--seed 42` |
| `empty-seed-max` | `--seed 18446744073709551615` |
| `empty-seed-0-512` | `--seed 0 --map-size 512` |
| `empty-phrase` | `--seed-phrase "The Unmapped"` |
| `pattern-seed-0` | `--seed 0 --test-pattern` |
| `pattern-seed-42` | `--seed 42 --test-pattern` |

P0 passes its exit check when the empty cases (and, as a strong recommendation, the pattern cases) are identical across all three presets.

## 10. Invariants

- The output of `generate` depends only on seed, phrase, settings, data and generator version: not on the output path, current directory, environment, locale, time or machine.
- No command writes to the current directory by default.
- No command leaves a `.partial` file behind.
- `hash` without `--detail` equals the SHA-256 of the file bytes as computed by any standard tool.
- `report` and `hash` output is byte-identical across platforms for the same file.
- `render` produces the same RGB buffer for the same file and options on every platform.

## 11. Determinism notes

| Risk | Mitigation |
| --- | --- |
| Windows `argv` code page | Arguments read as UTF-16 and converted to UTF-8. |
| Windows text-mode stdout turns LF into CR LF | Binary mode on stdout and stderr. |
| Locale-dependent number formatting | No `setlocale`; integer formatting with `std::to_chars` or project code. |
| PNG encoder output | Contract is the pixel buffer, not the compressed bytes. |
| Output path leaking into the file | Paths never stored (P0-02 section 5.3); test compares worlds written to different paths. |
| Wall-clock time | Used only for `--verbose` timings on stderr. |

## 12. Tests required

Integration tests run the built binary (CTest), each with `UNMAPPED_BUILD_ROOT` pointing at a fresh temporary directory under the real build root and the current directory set to another fresh, empty temporary directory.

### 12.1 Conventions

- Exit codes: no command → 2; unknown command → 2; unknown option → 2; repeated `--seed` → 2; `--seed` and `--seed-phrase` together → 2; `--seed -1`, `--seed 042`, `--seed 18446744073709551616`, `--seed 0x` → 2; no seed → 2; `--map-size 1000` → 2; invalid settings file → 3 with the P0-03 error lines; missing `--db` file → 3; output exists without `--force` → 4; `--out` into a missing directory → 4.
- `--version` prints one line matching `worldgen-cli 0.2.0 schema 1 sqlite <version>`.
- After every test, the current directory is still empty and no `.partial` file exists anywhere under the temporary build root.
- On Windows, stdout of `hash` and `report` contains no CR byte.

### 12.2 `generate`

- Default path: `generate --seed 42` writes `<root>/out/world-42.db`, prints that absolute path, exits 0. `--seed-phrase "The Unmapped"` writes `world-<phrase_seed>.db`.
- `--force` replaces an existing file; without it the old file is untouched.
- Equivalence: `--seed 42`, `--seed 0x2A`, and `--settings` with `seed = 42` give byte-identical files. Same for map size given by flag and by settings.
- Path independence: the same arguments with `--out a/w.db` and `--out b/other-name.db` give byte-identical files.
- Environment independence: identical bytes with `TZ=UTC` and `TZ=Asia/Kolkata`, `LANG=C` and `LANG=de_DE.UTF-8` (where available).
- Phrase on Windows and Linux: `--seed-phrase "Ünmapped Wörld"` stores `seed_phrase` as the UTF-8 bytes of that text and gives the same `seed` on all presets (golden value recorded by the validation specialist).
- Empty world contents as P0-02 section 8; `report` output for `--seed 0` exactly as in section 7.
- Test pattern: the reader decodes `test.ramp` and every cell equals (x + 2y) mod 256; every `test.hash` cell equals `hash_at(stream_key(seed, "test.hash"), x, y) >> 56` computed by the test with the core functions; every `test.noise` cell equals the formula in 5.2 computed by the test; `meta.stages` = `test_pattern`; layer ids 1, 2, 3.
- Fatal path: a test build hook that forces the fatal error inside a stage gives exit 1 and leaves no output and no `.partial`.

### 12.3 `render`

- `render` of `test.ramp` with `gray`: min 0, max 255, so every pixel (x, y) equals (v, v, v) with v = (x + 2y) mod 256; image 256 × 256. With `--scale 3`, 768 × 768 and pixel (3x + i, 3y + j) equals pixel (x, y) for i, j in 0..2.
- A constant layer (test-only fixture) renders all black.
- Gray formula on a synthetic i16 layer (test-only fixture) with values -32767, 0, 32767: g = 0, 128, 255. (For 0: (510 × 32767 + 65534) / 131068 = 16776704 / 131068 = 128 exactly. For 32767: 33487874 / 131068 = 255.5, floor 255.)
- Unknown layer → exit 3 and the message lists the layer names. Default output path `<root>/out/<stem>.<layer>.png`.
- The SHA-256 of `render_rgb` for each test-pattern layer, seed 0, is a golden value compared on all presets.
- The PNG file decodes (with the vendored reader, if present, or by checking the PNG signature and IHDR width, height, bit depth 8, colour type 2).

### 12.4 `report`

- Empty world, seed 0: exact text of section 7, byte for byte.
- A world whose `seed_phrase` contains a backslash and a tab prints them as `\\` and `\x09`.
- `--out` writes the same bytes as stdout; existing file without `--force` → 4.

### 12.5 `hash`

- Output equals the SHA-256 of the file bytes computed in the test with the core implementation; on Linux CI it also equals `sha256sum` output.
- `hash` of a non-world file (a PNG) works without `--detail` and fails with 3 with `--detail`.
- `--detail` for the empty world prints exactly four lines: `file`, `schema`, `table layer`, `table meta`, in that order. For a test-pattern world, three `layer` lines follow in id order.
- Layer hash is codec-independent: a test-only world storing the same layer with codec 0 and codec 1 gives the same `layer` line.
- The `table meta` hash equals a hash the test builds by hand from section 8's encoding over the eight known rows.
- `--expect` with the right hash (upper- or lower-case) → 0; with a wrong one → 5 and the stderr message.

### 12.6 Golden

The validation specialist records `p0-worlds.txt` (section 9) and the `render_rgb` hashes, and confirms the cross-preset comparison passes.

## 13. Open questions

1. **No random default seed** (section 5). Approve.
2. **`report` defaults to stdout** (section 7) rather than a file under `out/`. Approve, or ask for a file default.
3. **`--test-pattern` stage in P0.** Not required by the design doc, but it moves the cross-platform proof of hashing, noise and the codec into P0. Approve.
4. **`render` contract is the pixel buffer, not PNG bytes.** Approve.
5. **Design doc section 10 describes `worldgen-cli` as "generate, inspect, export map images"**, while section 13 and CLAUDE.md list `generate`, `render`, `report`, `hash`. This spec follows section 13. Proposals entry in the report to align section 10.

## 14. Amendments after the first implementation (2026-11-15)

Decisions by the lead on points raised by the generator engineer. Where these differ from the text above, these win.

1. **Phrases.** Wherever this spec says the seed of a text phrase is `phrase_seed`, the function called is `seed_from_phrase` (P0-01 section 7.3), which scrubs the phrase before hashing (amendment 15).
2. **`--help` (4.1).** Options are tokenized first, so an unknown or repeated option or a positional argument is still a usage error (exit 2); then `--help` wins before any value is validated. `--help` and `--version` as the top-level argument must be alone. The hint line is `worldgen-cli: run 'worldgen-cli [<command>] --help' for usage`.
3. **Seeds and integers (4.2).** The hex prefix is lower-case `0x` only, as in P0-03 amendment 6; `--seed` and `world.seed` share one parser.
4. **Build root (4.3).** On Windows the order is `UNMAPPED_BUILD_ROOT`, then `LOCALAPPDATA`, then a usage error; elsewhere `UNMAPPED_BUILD_ROOT`, then `HOME`, then a usage error. A relative build root is a usage error, since a default output could otherwise land under the current directory. Printed absolute paths are made absolute and lexically normalized; symbolic links are not resolved.
5. **Output paths (4.3, 4.6).** An empty `--out`, or an `--out` that names a directory, is an output error (exit 4).
6. **Exit codes for reader failures (4.6).** An SQLite I/O or corruption failure while reading is an input error (3); running out of memory is an internal error (1).
7. **Image size (section 6).** `render` refuses an image whose raw RGB data would exceed 1 GiB with a usage error asking for a smaller `--scale`. No P0 layer reaches this at any scale.
8. **Escaping (sections 7 and 8).** The escaping rule for meta values also applies to keys, table names and layer names in `report` lines and in `hash --detail` lines. Valid names are unaffected.
9. **`report --out` (section 7).** Prints nothing on stdout. `--force` without `--out` is accepted and has no effect.
10. **`hash` (section 8).** The expected value in the mismatch message is printed in lower case. Tables are excluded when their name starts with `sqlite_` in any letter case. The schema rows are ordered by `type`, then `name`, with `BINARY` collation, and a NULL `sql` is hashed as NULL. Table rows are read with SQL `ORDER BY` on the primary-key columns in key order with `BINARY` collation. A table with no primary key, or any REAL value, is an input error (3).
11. **Fatal-path test hook (12.2).** The hook lives in a separate executable, `worldgen-cli-test-hooks`, built only with tests from the same sources; it accepts `--test-fatal-in-stage` together with `--test-pattern`. The production binary rejects that option as unknown.
12. **Library placement.** The test-pattern stage, `render_rgb`, the report builder and the logical hashes live in the `worldgen` library (`worldgen/stages/`, `worldgen/inspect/`), and raw row access is part of the reader (P0-02 amendment 10); the executable only parses arguments, calls them and writes files.
13. **Recorded for the golden file.** The first implementation produced, identically on five Linux builds, the eight case hashes that the validation specialist is to confirm independently before they become golden values in `worldgen/tests/golden/p0-worlds.txt`.
14. **After the determinism audit.** (a) Any failed or short write to stdout makes the command exit 4 with `worldgen-cli: error: writing to stdout failed`, for every command and for `--help` and `--version`, whatever the output size; this also replaces exit 5 when the hash output itself was lost. A world or image already written completely stays in place. (b) Anything at an output path counts as existing, including a dangling symbolic link: without `--force` the command exits 4 and leaves the link; with `--force` the link is replaced by a regular file and its target is not created. The database writer follows the same rule. (c) On Windows an environment variable or argument that is not valid UTF-16 is a usage error (exit 2). (d) The hint line of amendment 2 is a template: it names the command when one was given (`worldgen-cli: run 'worldgen-cli generate --help' for usage`) and otherwise reads `worldgen-cli: run 'worldgen-cli --help' for usage`. (e) The image size limit of amendment 7 is on 3 × width × height bytes after scaling. (f) A table hash covers the table's stored columns in declared order; generated and hidden columns are not included. No P0 table has any.
15. **Seed phrases are scrubbed (design doc section 4).** `--seed-phrase TEXT` is scrubbed with `seed_from_phrase` (P0-01 section 7.3): the stored `seed_phrase`, the seed and the default output file name all come from the scrubbed phrase, so `--seed-phrase "Ashen Marches!"`, `"ashen-marches"` and `"ASHEN  MARCHES"` write byte-identical files. A phrase that scrubs to nothing (for example `"!!!"` or one written only in a non-Latin script) or to more than 256 bytes is a usage error (exit 2). `--seed` and `--seed-phrase` stay separate inputs: `--seed-phrase 42` is the phrase `42`, a different world from `--seed 42`. Consequences for section 12: the non-ASCII phrase test becomes "`Ünmapped Wörld` stores `seed_phrase = nmapped w rld` and gives the same seed on all presets, equal to that of `nmapped w rld`"; the `report` escaping test in 12.4 can no longer use a phrase (a scrubbed phrase has no backslash or tab) and uses a test-only world with such a meta value instead; case `empty-phrase` keeps its arguments and now stores `seed_phrase = the unmapped`. The generator version is `0.2.0`, so `--version`, the expected `report` text in section 7 and all eight golden hashes change.
16. **Geography.** P1-00 appendix A4 is adopted: `generate` runs the implemented geography stages by default, with `--until STAGE|none` and `--threads N`; `--test-pattern` runs only the test pattern; `render` takes exactly one of `--layer` and `--composite` and gains the palettes of P1-00 section 10.2; `report` emits format v2 with a `[geography]` section; the `empty-*` CI cases add `--until none` and the geography cases of P1-00 section 14 are added with their own golden file.
17. **Schema version 2** (P0-02 amendment 14). The expected report text of section 7 changes in `[meta]` to `schema_version = 2` (and the generator version) and in `[tables]` to three lines, `layer = 0`, `meta = 8`, `pass = 0`, in that order; `hash --detail` prints a `table pass` line between `table meta` and the layers. A malformed `pass` row read by `report` is an input error (exit 3) of the corrupt-data class, `worldgen-cli: error: corrupt table 'pass': <reason>` (P1-00 10.3).
18. **Schema version 5 and the regions stage** (P0-02 amendment 17, P1-06 3 and 9, generator 0.11.0, 2026-11-18).
    (a) **`report` `[tables]`** lists, in ascending byte order of the name as for every table, `biome`, `climate`, `layer`, `meta`, `pass`, `region`, `region_biome`, `region_border` (note `region_biome` before `region_border`). The expected text of section 7 for an empty world (`--until none`) is therefore, in `[tables]`: `biome = 0`, `climate = 0`, `layer = 0`, `meta = 8`, `pass = 0`, `region = 0`, `region_biome = 0`, `region_border = 0`, with `schema_version = 5` and the generator version `0.11.0` in `[meta]`.
    (b) **`hash --detail`** prints the `table` lines in the same order (`table biome`, `table climate`, `table layer`, `table meta`, `table pass`, `table region`, `table region_biome`, `table region_border`), then the `layer` lines in layer id order; in a full run `layer region` is last (id 19, after `layer walk_area`). The table hash is that of section 8 and amendment 14 (f), unchanged.
    (c) **`report` `[geography]`** gains, after the resources keys and when the `region` layer exists, eight lines in this order: `regions`, `region_area_km2`, `region_borders`, `region_borders_walkable`, `region_borders_river`, `region_borders_pass`, `regions_isolated`, `regions_upland`, with the values and the corrupt-data errors (exit 3) of P1-06 9: `corrupt layer 'region': …`, `corrupt table 'region': row <id>: cells below 1`, `corrupt table 'region_border': row (<a>, <b>): a not below b` or `… unknown region <id>`.
    (d) **`render --layer region`** uses the palette `ids` (P1-00 10.2) under `auto`; `ids` accepts `u16`.
    (e) **Composite `regions`** (`--composite regions`; default output `<stem>.regions.png`; `--composite` accepts `map`, `biomes` and `regions`, and `render --help` lists all three). Needs `elevation`, `biome` and `region`; uses `water`, `river`, `flow_dir` and table `biome` as the `biomes` composite does. Validation, in order: the checks of the `biomes` composite (P1-04 9, `composite_biomes_problem`), then `region`: missing → the reader's missing-layer error (`worldgen-cli: error: world file '<path>' has no layer 'region'; available layers: …`, exit 3, no file); not `u16` → `worldgen-cli: error: cannot render the composite regions: layer 'region' is <type>, expected u16; available layers: <names>`; not the size of `elevation` → `… layer 'region' is not the size of layer 'elevation'; available layers: <names>` (exit 3, no file). Pixels, per cell (x, y) at scale 1: first the pixel of the `biomes` composite, every step included (shading, ocean, lakes, the river drawing); then, with v = `region`(x, y), the pixel becomes **(0, 0, 0)** if v > 0 and (x + 1 < W, w = `region`(x + 1, y) > 0 and w ≠ v, or y + 1 < H, w = `region`(x, y + 1) > 0 and w ≠ v). Only the `region` values decide (not `water`); a border is drawn on the west or north cell of each pair, one cell wide. At `--scale k` each cell's final colour fills its k × k block, as for the other composites. Example: a 3 × 2 world with `region` rows `1 1 2`, `1 2 2`: (0, 0) has E 1 and S 1, unchanged; (1, 0) has E 2: black; (0, 1) has E 2: black; (1, 0)'s S neighbour (1, 1) is 2 as well; (2, 0), (1, 1) and (2, 1) have no differing E or S neighbour, unchanged. Black cells: (1, 0) and (0, 1). Seams: `composite_regions_problem(elevation, water, river, flow_dir, biome, region)` and `render_composite_regions(…, scale, biome_table)`, beside those of `biomes`.
19. **Schema version 6 and the chronology stage** (P0-02 amendment 18, P3-03 4.3, 6, 9 and 13, generator 0.13.0, 2026-11-19).
    (a) **`generate`** runs `chronology` after `regions` by default (`meta.stages` ends in `,chronology`). `--until` accepts `chronology`; `--until history` is a usage error (exit 2), `stage 'history' given to --until is not implemented yet`; an unknown name lists the eight stage names, `unknown stage '<x>' given to --until; expected none or a stage name (elevation, hydrology, climate, biomes, resources, regions, chronology, history)`. Every world records the eight history settings in `meta`, `--until none` and `--test-pattern` worlds included.
    (b) **Data checks across kinds** (P3-03 6, D0 to D5) run right after the data is loaded, whatever the plan: an input error (exit 3), `worldgen-cli: error: invalid data in '<data root>'` (the root printed as in the loader's error of the same text), then one line per error, `races: error: <message>`; no world is written.
    (c) **`report`**: `[tables]` lists, in ascending byte order, `age`, `biome`, `climate`, `culture`, `god`, `layer`, `meta`, `pass`, `race`, `race_biome`, `region`, `region_biome`, `region_border`. The expected text of section 7 for an empty world (`--until none`) is therefore, in `[tables]`: `age = 0`, `biome = 0`, `climate = 0`, `culture = 0`, `god = 0`, `layer = 0`, `meta = 16`, `pass = 0`, `race = 0`, `race_biome = 0`, `region = 0`, `region_biome = 0`, `region_border = 0`; `[meta]` gains the eight `param.*` rows in key order with their defaults (`param.civs_per_race = auto`, `param.divine_activity = normal`, `param.divine_script =` with no trailing space, `param.event_intensity = normal`, `param.history_detail = normal`, `param.history_span = 6000`, `param.monster_pressure = normal`, `param.races = elf,goblin,dwarf,faerie,human,orc` with the shipped data), `schema_version = 6` and the generator version `0.13.0`. A `[chronology]` section follows `[geography]` (or `[layers]`) when table `age` has rows, with the corrupt-data errors (exit 3) of P3-03 9.1. The report format stays v2.
    (d) **`hash --detail`** prints the five new `table` lines (`table age`, `table culture`, `table god`, `table race`, `table race_biome`) among the others in ascending name; a NULL `race.awakening_year` is the value tag `0x00`.
    (e) **CI.** `world-hashes.sh` gains the cases `hist-span-2000-512` (`--seed 7 --map-size 512 --history-span 2000`) and `hist-races-elf-orc-1024` (`--seed 3 --races elf,orc`), fourteen cases in all, with the golden file `worldgen/tests/golden/p3-worlds.txt` (same format as `p1-worlds.txt`; a note, not a failure, while absent).
20. **Schema version 7 and the history stage** (P0-02 amendment 19, P3-04 9.5, 11 and 15, generator 0.14.0, 2026-11-19).
    (a) **`generate`** runs `history` after `chronology` by default; `meta.stages` of a default run ends in `,history`. `--until history` is accepted and gives the same file as the default; the message for an unknown stage is unchanged (it already names the eight stages). There is no resume option: resuming a stored world is a library seam (P3-04 3.2).
    (b) **`report`**: `[tables]` lists 26 tables in ascending byte order, the thirteen of P3-04 9.1 among them (`civilization`, `event`, `event_cause`, `event_kind`, `event_kind_role`, `event_kind_value`, `event_participant`, `event_value`, `phase`, `region_control`, `relation`, `settlement`, `settlement_census`); for an empty world each is `= 0` and `meta = 17`; `[meta]` gains `history_end_year = 6000` (default span), and `schema_version = 7` with generator version `0.14.0`. A `[history]` section follows `[chronology]` when table `phase` has rows, with the corrupt-data errors (exit 3) of P3-04 11.1. The report format stays v2.
    (c) **`hash --detail`** prints the thirteen new `table` lines among the others in ascending name; NULLs are the value tag `0x00`.
    (d) **CI.** `world-hashes.sh` gains the case `hist-dense-1024` (`--seed 11 --history-detail dense`), fifteen cases in all, its hash in `p3-worlds.txt`.

21. **`report --history`** (P3-07 4 and 18.3, 2026-11-19; no world byte changes). (a) Synopsis: `worldgen-cli report --db PATH [--history [--level summary|full]] [--out PATH] [--force] [--verbose]`. `--history` prints the history report, format `# worldgen history v1` (P3-07 6), instead of the world report; `--level` (`summary`, the default, or `full`) needs `--history`. Usage errors (exit 2): `--level needs --history`; `invalid --level '<text>'; expected summary or full`. `report --help` gains the lines `  --history           print the history report (format '# worldgen history v1') instead of the world report` and `  --level LEVEL       with --history: summary (default) or full`. `--out` and `--force` behave as for the world report. (b) A world without history is an input error (exit 3, nothing on stdout, no file): `worldgen-cli: error: world file '<path>' has no history (stages: <stages or none>); generate it with the history stage`; corrupt history tables are input errors with the messages of P3-07 5.2 and 5.3. (c) The "Later" note of section 7 is fulfilled by P3-07. (d) Section 10's invariant "`report` and `hash` output is byte-identical across platforms" covers `report --history`. (e) The exit code table's input errors include "a world without history given to `report --history`".
22. **`generate --help`: which history settings act** (P3-08, 2026-11-20; the P3-06 validation's F2; no world byte changes). Since generator 0.15.0 the history stage also reads `--civs-per-race` and `--event-intensity` (P3-05 7.2, 10.1), but the closing paragraph of `generate --help` still said the history stage uses the span and the detail and "the others take effect with later history subsystems". From the P3-08 change the paragraph reads, line breaks as shown:

    ```
    The history settings are recorded in every world. The chronology stage uses
    the span and the races; the history stage uses the span, the detail, the
    civilizations per race and the event intensity. The monster pressure and
    the divine activity take effect with later history subsystems (phase P5).
    --test-pattern records them and writes no history.
    ```

    Every other line of the usage text is unchanged. Test: the CLI test that compares `generate --help` with its expected text (or, if there is none, a new one in `tools/worldgen-cli/tests/`) checks these five lines.
23. **`render --composite territory` and `--year`** (P4-01 7, 2026-11-22; no world byte changes). (a) Synopsis: `worldgen-cli render --db PATH (--layer NAME [--palette P] | --composite NAME [--year Y]) [--out PATH] [--scale N] [--force] [--verbose]`. `--composite` also accepts `territory` (`invalid --composite '<x>'; expected map, biomes, regions, territory`): the `regions` composite with each region held at year Y in its holder's colour (P4-01 5.5, 5.6). `--year Y` (decimal, 4.2) applies to `territory` only, default `meta.history_end_year`; default output `<db file stem>.territory-<Y>.png`. (b) Checks in order: usage (exit 2; `--year applies to --composite territory only`, the shared integer message for a malformed value); open the world (3); the `regions` composite's layer checks with the composite named `territory` (3); the history (3: the no-history message of amendment 21 (b), or the corrupt-table messages of P3-07 5.2 and 5.3); the year range, `--year <text> is outside the history of world file '<path>' (0 to <E>)` (exit 2); then the image size, the output location and the write as for every `render`. (c) `render --help` changes as P4-01 7 gives. (d) Section 10's cross-platform invariant covers the territory pixel buffer.
24. **`render --markers`, `--faded`** (P4-04 7, 2026-11-23; no world byte changes). (a) Synopsis: `worldgen-cli render --db PATH (--layer NAME [--palette P] | --composite NAME) [--year Y] [--markers [--faded]] [--out PATH] [--scale N] [--force] [--verbose]`. `--markers` draws the markers of year Y (P4-04 3 to 6: settlements, ruins, event places, passes) over the rendered image of any layer or composite; `--faded` adds faded ruins. (b) `--year` applies to `--composite territory` and to `--markers`, default `meta.history_end_year`; the usage message of amendment 23 becomes `--year applies to --composite territory or --markers only`; new usage message `--faded applies to --markers only`. (c) Default output `<db file stem>.<name>-<Y>.markers.png` (`<name>` the layer or composite), `.markers-faded.png` with `--faded`. (d) Checks in order as P4-04 7: usage (2); open (3); the image's own checks (3); with `--markers` or `territory` the history (3); with `--markers` the marker index (3: the reader's errors, `corrupt table 'pass': <reason>` as amendment 17, `corrupt table 'region': the file changed while it was read`); the year range (2); then size, output and write. (e) `render --help` changes as P4-04 7 gives. (f) Section 10's cross-platform invariant covers the marker pixels.
