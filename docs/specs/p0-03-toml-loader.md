# P0-03 TOML loader

Status: approved by the lead, 2026-11-15. Phase P0 Foundation. Design doc sections 5 (definitions in data files), 10 (TOML data files, conventions). Depends on P0-01.

## 1. Purpose

Races, beasts, monsters, cultures, biomes, event rules and generator tuning are hand-edited TOML (CLAUDE.md rule 4). This spec defines how the generator finds, parses, validates and orders those files, and how their numbers reach the generator so that no platform floating-point parsing can affect a world. It also defines the world settings file and the content hash stored in `meta`.

P0 delivers the loader machinery, the settings file, and the content hash. No content kinds (biomes, races, …) are defined in P0; each later stage spec that needs one registers it with a schema in the form given here.

Code location (interim): `worldgen/include/worldgen/data/`, `worldgen/src/data/`.

## 2. Inputs

| Input | Source | P0 |
| --- | --- | --- |
| Settings file | Optional path, `worldgen-cli generate --settings FILE` | Yes |
| Data root | Optional directory, `worldgen-cli generate --data DIR` (normally `game/data/`) | Accepted; no kinds registered, so nothing is read from it |
| Kind registry | Compiled into `worldgen` (one source file listing every kind schema) | Empty in production; test-only kinds in tests |

The TOML parser is the vendored one in `extern/` (toml++ expected), used only for syntax: it turns text into a tree of tables, arrays, strings, integers and booleans with source positions. The loader never reads a floating-point value from it.

## 3. Outputs

- `Settings`: the world-creation settings (section 6).
- `ContentSet`: for each registered kind, its definitions in ascending id order, each a validated record with defaults applied.
- `content_hash`: SHA-256 over the canonical encoding of the `ContentSet` (section 8), written to `meta.content_hash` by P0-02.
- Or a list of errors (section 9). Loading is all-or-nothing.

No database rows are written by the loader itself. The settings reach `meta` as `param.*` keys (P0-02 section 5.5).

## 4. Value types and the numeric rule

**Rule: TOML floating-point literals are rejected.** Fractional numbers are written as decimal strings and parsed by project code (`parse_decimal`, P0-01 section 5.6) into `Fx`. Integers are TOML integers, which the parser reads exactly as int64. This is the whole of the numeric policy; it keeps `strtod`, `from_chars` for doubles and any parser float code out of the path to the database.

| Schema field type | Accepted TOML | Value | Rejected with an error |
| --- | --- | --- | --- |
| `int(min, max)` | integer (decimal, hex, octal, binary, underscores: any TOML integer syntax) | int64 | string, float, out of [min, max] |
| `fx(min, max)` | integer n (gives `from_int(n)`), or a string matching the decimal grammar of P0-01 5.6 | `Fx` | float literal, `inf`, `nan`, string not matching the grammar, out of [min, max] or of the `Fx` range |
| `bool` | `true` / `false` | bool | anything else |
| `string(max_bytes[, grammar])` | string (any TOML string form) | UTF-8 bytes after the newline rule below | over max_bytes (default 4096); fails grammar; forbidden control character |
| `enum(v1, v2, …)` | string equal to one of the listed values | the string | other strings |
| `ref(kind)` | string, id grammar | the id; checked to exist in `kind` after all files are loaded | unknown id |
| `array(T, min_len, max_len)` | TOML array whose every element is valid for T | ordered list | wrong element type, length out of range |
| `record(schema)` | TOML table (standard or inline) | nested record | as for a definition (section 7.5) |

Examples in a data file:

```toml
[biome.temperate_forest]
moisture_min = "0.35"        # fx: decimal string
moisture_max = 1             # fx: integer is fine
tree_density = 0.6           # error: floating-point value; write "0.6"
colour = 0x2E6B30            # int
```

Error text for a float (tests check the words "floating-point"): `floating-point value for 'biome.temperate_forest.tree_density'; write it as a decimal string such as "0.6", or as an integer`.

Other TOML types — offset date-time, local date-time, local date, local time — are rejected: `date and time values are not allowed`.

**Strings and line endings.** After parsing, every string value has each CR LF pair replaced by LF. Then any remaining control character (U+0000–U+001F except TAB and LF, and U+007F) is an error. A multi-line string therefore reads the same from a checkout with CRLF line endings as from one with LF. (A `.gitattributes` rule `*.toml text eol=lf` is requested from the build engineer as a second line of defence.) No Unicode normalization is applied; text is used as written.

## 5. Parsing

- Files are read as bytes (binary mode) in full, maximum 1 MiB each (error above), and handed to the parser. Invalid UTF-8 is a parse error. A UTF-8 byte-order mark at the start is ignored. CRLF and LF line endings are both accepted.
- Syntax errors (including duplicate keys, which TOML forbids) are reported with the parser's line and column and the parser's message, prefixed as in section 9.
- Nesting deeper than 8 tables/arrays is an error.
- The loader never uses the parser's own conversion to floating point, its formatting of values, or its iteration order over table keys for anything that matters: where the loader walks a table's keys, it first sorts them by UTF-8 bytes.

## 6. Settings file (P0)

World-creation settings. Optional; every value has a default or comes from the command line. Command-line flags override the file key by key.

```toml
[world]
seed = "18446744073709551615"   # or an integer 0 .. 9223372036854775807, or "0x..." hex
# seed_phrase = "The Unmapped"  # alternative to seed
map_size = 1024                 # 512, 1024 or 2048 cells
```

| Key | Type | Default | Rule |
| --- | --- | --- | --- |
| `world.seed` | TOML integer ≥ 0, or string: decimal `0\|[1-9][0-9]*` up to 18446744073709551615, or `0x` followed by 1–16 hex digits (either case) | none | at most one of `seed`, `seed_phrase` |
| `world.seed_phrase` | string, trimmed and validated as in P0-01 section 7.3 | none | |
| `world.map_size` | int, one of 512, 1024, 2048 | 1024 | |

Anything else in the file (another key in `[world]`, another top-level table) is an error. Later phases add keys (history span, detail, civilizations per race, races present, monster pressure, divine activity, event intensity) with their defaults from design doc section 5; each is stored in `meta` as `param.<key without "world.">`.

Precedence: a seed given on the command line (`--seed` or `--seed-phrase`) replaces both seed keys of the file. If no seed is given anywhere, `generate` fails with a usage error (P0-04).

The settings file is not part of `content_hash`; its effect on the world is recorded through the `meta` keys it sets.

## 7. Data directory (framework in P0, kinds from P1 on)

### 7.1 Kinds

A kind is a type of definition, registered in code with:

| Property | Example | Rule |
| --- | --- | --- |
| `name` | `biome` | grammar `[a-z][a-z0-9_]*`, unique |
| `directory` | `biomes` | relative to the data root, one path segment, grammar as name |
| `required` | true | if true, a missing directory or zero definitions is an error |
| `schema` | list of fields | field name (grammar `[a-z][a-z0-9_]*`), type (section 4), required or default value |

Kinds are processed in ascending `name` order. The registry is a fixed list in one source file; a test checks names and directories are unique.

### 7.2 File discovery and order

For each kind, in the directory `<root>/<directory>`:

1. List the directory entries (the filesystem's order is never used beyond this step).
2. Ignore every entry whose name starts with `.`, and every entry whose name does not end in `.toml` (so `README.md`, `notes.txt`, editor backups like `races.toml~` are ignored).
3. Each remaining entry must be a regular file, not a directory or symbolic link, and its name must match `[a-z0-9][a-z0-9_-]*\.toml`. Anything else is an error (for example `Races.toml`, `my races.toml`, a directory `old.toml`). Lower-case-only names mean two valid names can never collide on a case-insensitive filesystem, so Linux and Windows always see the same set.
4. Sort the names by their bytes, ascending.
5. Parse the files in that order.

The data directory is not searched recursively, and directories of kinds that are not registered are not read at all (the game keeps other data under `game/data/`).

### 7.3 File structure

A file contains exactly one top-level key, the kind's `name`, whose value is a table mapping definition ids to definition tables:

```toml
[biome.temperate_forest]
moisture_min = "0.35"

[biome.highland_moor]
moisture_min = "0.5"
```

Any other top-level key is an error. A file may hold any number of definitions, including zero (an empty file is allowed). Ids: grammar `[a-z][a-z0-9_]*`, 1 to 64 bytes.

### 7.4 Definition order

**The canonical order of a kind's definitions is ascending id (by bytes)**, not file order or order within a file. Moving a definition from one file to another, or reordering a file, therefore never changes a world. An id defined twice, in the same kind, in any files, is an error naming both locations.

Stages that iterate a kind (for example a weighted choice over biomes) iterate in this order.

### 7.5 Validation

For each definition, in id order:

1. Every key must be a field of the schema; an unknown key is an error (catches typos).
2. Every required field must be present; a missing one is an error at the definition's header line.
3. Each value is checked against its field type (section 4).
4. Defaults are applied to absent optional fields.

After all kinds are loaded: every `ref` value must name an existing id of its target kind.

All errors from all files are collected before failing (a syntax error stops only its own file). Kind-specific rules beyond types (for example "moisture_min ≤ moisture_max") are part of each kind's schema as named checks, defined in the stage spec that registers the kind, and run after type validation.

### 7.6 Data root location

P0: `--data DIR` only; if absent, the content set is empty. From the first phase that registers a kind (P1), the default when `--data` is absent is the `UNMAPPED_DATA_DIR` environment variable if set, otherwise `game/data/` in the source tree as recorded at configure time by CMake. The path is never written into the world file.

## 8. Canonical encoding and content hash

`content_hash` = SHA-256 (P0-01 section 10) of this byte stream:

```
for each registered kind, ascending name:
    str(kind name)
    u64(number of definitions)
    for each definition, ascending id:
        str(id)
        record(definition)

record(r)   = for each field of the schema, in declaration order:
                  str(field name) value(field value after defaults)
value(int)  = 0x01, i64
value(fx)   = 0x02, i64 of the raw value
value(bool) = 0x03, 0x00 or 0x01
value(string, enum or ref) = 0x04, str(bytes)
value(array)  = 0x05, u64(count), value(element) for each element in order
value(record) = 0x06, record(r)

str(s) = u64(length in bytes), the bytes
u64, i64 = 8 bytes little-endian (two's complement for i64)
```

Properties: the hash depends on the validated values only, so comments, whitespace, key order, file names and file boundaries do not change it, and a value written as `1` or `"1.0"` in an `fx` field hashes the same. A kind with no files still contributes its name and a zero count, so registering a kind changes the hash. With no kinds registered (P0) the stream is empty and the hash is `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.

## 9. Errors

Format, one per line on stderr:

```
<path>:<line>:<column>: error: <message>
<path>: error: <message>                   (when there is no position)
```

- `<path>` is relative to the data root with `/` separators on every platform; for the settings file it is the path as given on the command line.
- Line and column are 1-based, as reported by the parser for the offending value (for a missing field: the definition's table header).
- Messages name the full dotted key, e.g. `'biome.temperate_forest.moisture_min'`, and say what was expected.
- Errors are sorted by (path bytes, line, column, message bytes) and duplicates removed, so the output is the same on every platform.
- The CLI exits with code 3 (P0-04).

## 10. P0 versus later phases

| Item | P0 | Later |
| --- | --- | --- |
| Parser integration, type rules, numeric rule | Yes | — |
| Settings file with `world.seed`, `world.seed_phrase`, `world.map_size` | Yes | P3/P6 add history and population settings |
| Kind registry, discovery, ordering, validation, references, content hash | Yes, exercised by test-only kinds | — |
| Real kinds | None | P1: `biome` and generator tuning (interim recommendation: a kind `worldgen` with one definition per stage, e.g. `[worldgen.elevation]`, so tuning is part of `content_hash`); P3: `race`, `culture`, event rules; P5: `beast`, `monster`, `god` name tables, spell components |

## 11. Invariants

- The result of loading depends only on the bytes of the files' parsed values and their names, never on filesystem order, line endings, locale or platform.
- No floating-point value is produced or consumed anywhere in the loader.
- Definitions of each kind are in strictly ascending id order with no duplicates.
- Every `ref` resolves.
- `content_hash` is identical for any two data sets with the same validated values.
- Either a complete `ContentSet` or a non-empty sorted error list, never both and never a partial set.

## 12. Determinism notes

| Risk | Mitigation |
| --- | --- |
| Platform float parsing | Floats rejected; decimal strings parsed by project code. |
| Directory iteration order | Names sorted by bytes; canonical order by id. |
| Case-insensitive filesystems | Lower-case names only. |
| CRLF checkouts | Line endings irrelevant to TOML structure; CR LF in strings normalized; `.gitattributes`. |
| Parser's table iteration order | Keys sorted before walking. |
| Comments and formatting | Not part of the content hash. |

## 13. Tests required

Fixtures live under `worldgen/tests/data/loader/` (committed with LF endings) and use test-only kinds registered by the tests:

- `alpha` (directory `alphas`, not required) with fields: `count` int(0, 100) required; `ratio` fx(0, 1) default "0.5"; `enabled` bool default true; `label` string(32) default ""; `mode` enum(fast, slow) default fast; `tags` array(string(16), 0, 4) default []; `partner` ref(beta) required. (P0 has no "optional with no value" field type; a later spec adds one if it needs it, with its encoding in section 8.)
- `beta` (directory `betas`, required) with field `weight` int(1, 1000) required.

Unit tests:

1. **Numeric rule.** `ratio = 0.5` → error containing "floating-point"; `ratio = "0.5"` → raw 2147483648; `ratio = 1` → ONE; `ratio = "1.5"` → error (out of range); `ratio = "0.1234567890123456789"` → error (19 digits); `count = "5"` → error; `count = 1_0` → 10; `count = 0x10` → 16; `ratio = inf`, `ratio = nan` → floating-point error; a date value → "date and time values are not allowed".
2. **Strings.** A multi-line string in a file with CRLF line endings equals the same string from the LF version; an escaped `\u0001` in a string is an error; a 33-byte `label` is an error.
3. **Discovery.** Directory containing `b.toml`, `a.toml`, `.hidden.toml`, `notes.md`, `c.toml~`: `a.toml` and `b.toml` are loaded, in that order, the rest ignored. `Upper.toml` → error. A subdirectory `x.toml/` → error. Missing `alphas/` → zero definitions, no error; missing `betas/` → error (required).
4. **Order independence.** Definitions split as {x in a.toml, y in b.toml} and as {y in a.toml, x in b.toml}, and with keys in a different order within a definition: identical `ContentSet` and identical `content_hash`. Definitions come out in id order regardless of file order.
5. **Duplicates and unknowns.** Same id in two files → one error naming both paths; unknown key → error at its line; missing required `count` → error at the header line; top-level key other than the kind name → error.
6. **References.** `partner = "nope"` → error; valid reference resolves.
7. **Error collection and order.** A data set with errors in `b.toml` line 3, `a.toml` line 9 and `a.toml` line 2 prints them in the order a:2, a:9, b:3, all three, with relative `/` paths.
8. **Canonical encoding.** For one hand-built `alpha` definition (`count = 3`, `partner = "b1"`, all other fields defaulted) and one `beta` definition `b1` (`weight = 7`), the test builds the expected byte stream from section 8 by hand and checks the SHA-256 of it equals `content_hash`. Empty registry → `e3b0c442…b855`. A comment or whitespace change in a fixture leaves the hash unchanged; changing `count` changes it.
9. **Settings.** Seed as integer 42, as `"42"`, as `"0x2A"` → 42; `"18446744073709551615"` → 2^64 - 1; `"18446744073709551616"`, `"-1"`, `"042"`, `"0x"`, `"0x1_0"` → error; both `seed` and `seed_phrase` → error; `map_size = 1000` → error; unknown key `[world] size = 1` → error; absent file → defaults (map_size 1024, no seed).
10. **Limits.** A 1 MiB + 1 byte file → error; nesting depth 9 → error.

Determinism tests: run test 4 and test 8 on every preset; the fixture `content_hash` values are recorded once as golden values by the validation specialist and compared on all platforms.

## 14. Open questions

1. **Numeric rule:** floats rejected, fractions as decimal strings. The cost is that hand-edited files must quote fractions. The alternative (re-reading the float's source text from the parser's source position and parsing that) depends on parser internals and was rejected. Approve.
2. **Lower-case file names only.** Approve.
3. **One kind per directory, definitions keyed by id under a top-level table named after the kind.** Approve, or prefer one definition per file.
4. **Where generator tuning lives** (section 10): proposed as a data kind, so tuning changes show in `content_hash`. P1 decides.
5. **Unicode normalization of strings** is not applied; names copied from TOML into the database are byte-exact. Fine while files are hand-edited in one editor; flag if contributors on macOS (which may write NFD) are expected.

## 15. Amendments after the first implementation (2026-11-15)

Decisions by the lead on points raised by the generator engineer. Where these differ from the text above, these win.

1. **Float and date messages (section 4).** The float message always uses the fixed example `"0.6"`; the loader never reads the literal's value. The date message may add the key: `date and time values are not allowed (found one for '<key>')`. Tests check for the words "floating-point" and "date and time values are not allowed", not the whole line.
2. **Where the checks apply (section 4).** Floats, date/time values and forbidden control characters are reported wherever they appear in a parsed file, including under unknown keys.
3. **CR LF (section 4).** Every CR LF pair in a string value becomes LF, including one written with escapes (`"\r\n"`). A lone CR, however written, is a forbidden control character.
4. **Seed phrases in a settings file (section 6).** The string rule of section 4 applies to `world.seed_phrase` like any other string, so a control character other than TAB or LF is an error in a settings file, even though P0-01 section 7.3 accepts control characters in a phrase given on the command line. The settings-file rule is the stricter one and wins inside files.
5. **Nesting depth (section 5).** Depth counts tables and arrays below the document root: the kind table is depth 1 and a definition is depth 2. Depth 9 or more is an error and, like a syntax error, stops that file only.
6. **Seed forms (section 6).** The hex prefix is lower-case `0x` only (the digits may be either case). A negative TOML integer is an error. When both `seed` and `seed_phrase` are present the conflict is reported at `seed_phrase`, and each value is still validated. `Settings` carries the numeric seed derived from a phrase as well as the phrase.
7. **Discovery (7.2).** A kind directory that is a symbolic link or not a directory is an error. Kind directories are matched by exact bytes from a listing of the data root, so `Biomes` is not `biomes` on any platform. `NAME.TOML` does not end in `.toml` and is ignored. An entry that is both not a regular file and badly named gives one error ("not a regular file").
8. **Validation (7.5).** Named checks run only on a definition that passed type validation, and their errors are placed at the definition's header. References in default values are checked, at the header. A reference resolves if its target id exists, even when the target has its own errors.
9. **Data root (7.6).** With no kinds registered, the data root is not opened, so a mistyped `--data` path is not reported in P0. A required kind with no data root is an error.
10. **Errors (section 9).** An unknown key is reported at its value's position. Errors without a position sort before positioned errors of the same path. Array elements are named `key[i]`, 0-based. A duplicate id is reported at the second occurrence in file-name order, naming both locations. Columns count Unicode code points, and a byte-order mark is not counted. A definition written with a standard `[kind.id]` header is positioned at the `[`; one written with dotted keys or an inline table is positioned at the key or the `{`.
11. **Parser messages (section 5).** The text of a syntax-error message comes from the parser and may differ between platforms. Tests assert the path, line and column of a syntax error and that it is an error, not the message text. No world can differ because of this.
12. **Registration errors.** Mistakes in registering a kind (bad names, duplicates, minimum above maximum, a default that fails its own type) are fatal errors, as is a `ref` to an unregistered kind at load time.
13. **In-memory loading.** The loader can load from an in-memory description of a data root with the same discovery rules, so every discovery case in section 13 can be tested without the filesystem; at least the symbolic-link and case-sensitivity cases are also tested on disk.
