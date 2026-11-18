# P0-02 World database layer

Status: approved by the lead, 2026-11-15. Phase P0 Foundation. Design doc sections 3, 4 (reproducible database file), 6, 10, 13. Depends on P0-01.

## 1. Purpose

`world.db` is the contract between the generator and the game. This spec defines how the generator writes it so that the same seed, parameters and generator version give a byte-identical file on every platform and compiler; what the schema is at version 1; the layer blob format; what an empty P0 world contains; and the read API that `worldgen-cli` and, later, the Godot viewer use.

Code location (interim): `worldgen/include/worldgen/db/`, `worldgen/src/db/`.

## 2. Inputs

- The vendored SQLite amalgamation in `extern/` (version chosen and pinned by the build engineer; must be 3.37.0 or newer for `STRICT` tables).
- From the caller: the world seed (uint64), the seed phrase if any, the settings (P0-03), the content hash (P0-03), the list of stages run, and whatever rows the stages produce.

## 3. Outputs

One file at a path given by the caller. No other file survives a successful or failed run (no `-journal`, `-wal`, `.partial`).

## 4. SQLite build configuration

### 4.1 Compile-time options (required)

The build engineer compiles the amalgamation once, as a static library used by `worldgen`, `worldgen-cli`, the tests and later the GDExtension, with exactly these options affecting storage or semantics:

| Option | Value | Why |
| --- | --- | --- |
| `SQLITE_DEFAULT_PAGE_SIZE` | 4096 | Fixed page size. |
| `SQLITE_MAX_DEFAULT_PAGE_SIZE` | 4096 | Stops SQLite choosing a larger default from the device's sector size. |
| `SQLITE_DEFAULT_FILE_FORMAT` | 4 | Fixed schema format number. |
| `SQLITE_DEFAULT_AUTOVACUUM` | 0 | No auto-vacuum pointer-map pages. |
| `SQLITE_SECURE_DELETE` | 1 | Deleted content is zeroed; no stale bytes can survive in a page. |
| `SQLITE_TEMP_STORE` | 3 | Temporary data always in memory; no temp files. |
| `SQLITE_DQS` | 0 | Double-quoted strings are identifiers only (catches SQL typos). |
| `SQLITE_OMIT_LOAD_EXTENSION` | 1 | No extension loading. |
| `SQLITE_THREADSAFE` | 1 | The viewer may read from several threads. No effect on the file. |
| `SQLITE_DEFAULT_MEMSTATUS` | 0 | Performance only. |

Not allowed: `SQLITE_ENABLE_STAT3/4` and anything that makes SQLite write statistics tables; any option that changes the default journal mode to WAL. Options that only affect performance or API surface (for example `SQLITE_OMIT_DEPRECATED`) may be added by the build engineer; the golden hashes prove they change nothing. SQLite is compiled with the project's flags, so no fast-math.

**Version pinning.** The SQLite version number is written into every file header (offset 96). Upgrading the vendored SQLite is therefore a change to the output: it needs a generator version bump, new golden hashes and a proposals entry.

### 4.2 Runtime settings

The generator builds the world in an **in-memory working database** and writes the output file in one step at the end (section 6). Pragmas on the working database, issued in this order immediately after `sqlite3_open_v2(":memory:", …)` and before any table is created:

```
PRAGMA page_size = 4096
PRAGMA encoding = 'UTF-8'
PRAGMA auto_vacuum = NONE
PRAGMA journal_mode = MEMORY
PRAGMA temp_store = MEMORY
PRAGMA foreign_keys = OFF
PRAGMA application_id = 1431194960
PRAGMA user_version = <SCHEMA_VERSION>
```

`application_id` 1431194960 = 0x554E4D50 = ASCII `UNMP`. `user_version` is the schema version (1 in P0). Never run `ANALYZE`, `PRAGMA optimize`, or `VACUUM` on the working database, and never enable WAL.

Why in memory: no temporary file can be left behind, journal and sync settings cannot affect the result, and generation is faster. Memory cost is the size of the encoded output (tens of MB for a 2048-cell map); stages keep their own working arrays anyway.

## 5. Schema version 1

### 5.1 Which tables exist in P0

**Interim choice: only `meta` and `layer` exist at schema version 1.** Each later phase adds its tables (section 6 of the design doc) in the spec of the stage that first writes them, with a schema version bump.

Reasons: the other 22 tables' column types, nullability, keys and units are not yet specified (for example `region.bounds`, `road.points`, `quest_hook.params` have no format), and defining them now would freeze guesses that the P3 and P5 specs would then each have to change, with a bump each time. An empty table also says nothing a reader can use. The viewer (P2) binds to `meta` and `layer` first, which is what it needs.

### 5.2 Schema changes during the prototype

CLAUDE.md rule 7 requires a version bump, a spec update and a proposals entry for every schema change. Proposed working rule for the prototype (P0 to P6), pending approval:

1. `SCHEMA_VERSION` is one integer constant in `worldgen/include/worldgen/db/schema.hpp`. It is incremented once per merged branch that changes the schema (adds a table, adds or changes a column, changes a blob format, adds or removes a required `meta` key), not once per table.
2. The stage spec that makes the change contains a "Schema changes" section with the new DDL text and the new version number. A schema reference, `docs/specs/schema.md`, holding the full current DDL and a changelog, is created by the first stage that changes the schema (P1), and every later change updates it.
3. One proposals entry per phase lists that phase's schema changes, rather than one per table, so Paul can carry them back to the design in one go.
4. No migrations during the prototype. A reader supports exactly the current schema version and refuses any other with a clear message. Old worlds are regenerated.
5. A change to the schema always changes output bytes, so it also bumps the generator version and updates golden hashes.

### 5.3 Table rules

- Every table is `STRICT`, so a value of the wrong type (including any REAL) is rejected on insert.
- Allowed column types: `INTEGER`, `TEXT`, `BLOB`. **No `REAL`, no `ANY`, no floating-point value anywhere.**
- `NOT NULL` on every column unless the table's spec names it as nullable and says what NULL means.
- Every table is either a rowid table with `id INTEGER PRIMARY KEY` (no `AUTOINCREMENT`, which would add `sqlite_sequence`) or a `WITHOUT ROWID` table with an explicit `PRIMARY KEY`. `WITHOUT ROWID` is used only for tables with small rows (SQLite stores whole rows in interior pages of such tables, which suits small rows only).
- Integers are bound with `sqlite3_bind_int64` only. Unsigned 64-bit values (seeds, hashes, `layout_seed`) are stored as the two's-complement int64 with the same bit pattern; readers convert back. Text is bound with `sqlite3_bind_text` as UTF-8 with an explicit byte length. Blobs with `sqlite3_bind_blob`.
- Text is valid UTF-8, compared with the default `BINARY` collation. Names are stored as generated; no Unicode normalization is applied.
- The database never stores timestamps, file paths, host or user names, compiler or platform identifiers, build identifiers or git hashes.
- DDL text is stored verbatim in `sqlite_schema`, so it is part of the file. **DDL strings are string constants in C++ source**, each a single line with single spaces, exactly as written in this spec, never loaded from a file (a checkout with CRLF line endings would change them).

### 5.4 DDL at schema version 1

Executed in this order, each as one statement:

```
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL) STRICT, WITHOUT ROWID
CREATE TABLE layer (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE, width INTEGER NOT NULL, height INTEGER NOT NULL, format TEXT NOT NULL, data BLOB NOT NULL) STRICT
```

`layer` adds a surrogate `id` to the design doc's columns (`name, width, height, format, data`) so that layer blobs, which are large, live in a rowid table; `name` stays unique and is how readers look layers up. Interim choice; proposals entry in the report.

### 5.5 `meta`

Key/value, both TEXT. Keys are ASCII, grammar `[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)*`. Values use the canonical forms below. Readers must tolerate keys they do not know (a later schema version may add optional keys), but every key listed as required for the schema version must be present.

Required keys at schema version 1:

| Key | Value | Canonical form |
| --- | --- | --- |
| `content_hash` | SHA-256 of the canonical encoding of the loaded data definitions (P0-03 section 8) | 64 lowercase hex digits |
| `format` | `the-unmapped-world` | literal |
| `generator_version` | the generator version constant, e.g. `0.2.0` | `MAJOR.MINOR.PATCH`, decimal, no leading zeros |
| `param.map_size` | map side in cells: 512, 1024 or 2048 | decimal integer |
| `schema_version` | `1` | decimal integer; equals `PRAGMA user_version` |
| `seed` | the 64-bit world seed | unsigned decimal, no leading zeros, `0` for zero |
| `seed_phrase` | the trimmed phrase if the seed came from one, else empty | UTF-8 text, may be empty |
| `stages` | names of the stages that ran, in run order, comma separated | empty in a P0 empty world; `test_pattern` with the P0 test pattern; stage names follow the stream-name grammar without dots |

Rules for parameters: every world-creation setting and every generator parameter not loaded from data files is stored as `param.<name>`, with integers in decimal and fixed-point values in the canonical decimal text of P0-01 section 5.6. Later phases add `param.*` keys (history span, detail, races present, …) and `end_year`; each addition is a schema change under 5.2.

**Generator version.** `GENERATOR_VERSION` is a string constant in `worldgen/include/worldgen/version.hpp`, changed by hand. Rule: any merged change that alters output bytes for any input bumps it, and the golden hashes are updated in the same branch. It is never derived from git, the build date or the compiler, because that would change every world file on every commit. Value: `0.2.0` (0.1.0 until seed phrases were scrubbed, amendment 12).

### 5.6 `layer`

| Column | Meaning |
| --- | --- |
| `id` | 1, 2, 3, … in the order layers are written. The writing order is the stage order, and within a stage the order given by its spec. |
| `name` | Layer name, grammar as stream names (P0-01 7.3), 1 to 64 bytes, e.g. `elevation`, `biome`, `test.noise`. Names beginning `test.` are reserved for test layers. |
| `width`, `height` | Cells, 1 to 65536 each. |
| `format` | Element type: one of `u8`, `i8`, `u16`, `i16`, `u32`, `i32`, `u64`, `i64`. |
| `data` | Layer blob, section 7. Its header repeats width, height and type, which must match the columns. |

Cell (x, y): x is the column, 0 at the west edge, increasing east; y is the row, 0 at the north edge, increasing south. Element index = y × width + x (row-major). Interim choice of orientation; the P1 spec may not change it without a schema bump.

## 6. Creating and finalizing the file

```
1. Open the working database (":memory:"), apply the pragmas in 4.2.
2. BEGIN; execute the DDL in 5.4 in order; COMMIT.
3. Stages run in their fixed order. Each stage writes its rows inside its own
   BEGIN … COMMIT, inserting each table's rows in ascending primary key order.
   (Stages may UPDATE or DELETE rows if their spec says so; step 5 removes
   any effect this has on the layout.)
4. Write meta: BEGIN; insert the required keys in ascending key order (byte
   order); COMMIT. meta is written last so that `stages` and parameters are
   final.
5. Run PRAGMA integrity_check on the working database; anything but a single
   row "ok" is a fatal error.
6. Delete <out>.partial if it exists. Execute `VACUUM INTO ?` with the
   parameter bound to the UTF-8 path <out>.partial.
7. Close the working database.
8. Open <out>.partial read-only and verify the header fields in 6.1 and the
   meta keys; any mismatch is a fatal error. Close it.
9. Rename <out>.partial to <out> (replacing <out> only when the caller allows
   overwrite, P0-04).
On any failure: close everything, delete <out>.partial, report the error.
```

Why `VACUUM INTO`: it writes a fresh file in which every table is copied in key order (rowid order or primary-key order), indexes are rebuilt, there are no free pages, and no stale bytes remain. The output therefore depends only on the logical content (DDL text, the set of rows, header settings) and the SQLite version, not on how stages batched, ordered, updated or deleted their writes. Rows in rowid tables keep their `id` values, so ids must be assigned deterministically by the stage (always bind `id` explicitly).

`VACUUM INTO` preserves `application_id` and `user_version` in the SQLite versions known to the spec writer. Step 8 checks this. If the pinned version does not preserve them, the implementation sets both pragmas on `<out>.partial` after step 6 (one extra transaction, still deterministic) and this spec is updated.

### 6.1 SQLite header fields

The 100-byte header (SQLite's own fields are big-endian). Fields that can differ between runs or versions, and how each is pinned:

| Offset | Field | Value in a world file | How it is pinned |
| --- | --- | --- | --- |
| 0 | Magic | `SQLite format 3\0` | constant |
| 16 | Page size | 4096 | compile option plus pragma |
| 18, 19 | Write / read format version | 1, 1 | never WAL |
| 20 | Reserved bytes per page | 0 | default, never changed |
| 21–23 | Payload fractions | 64, 32, 32 | constant |
| 24 | File change counter | deterministic small value | written by `VACUUM INTO` into a fresh file: same version and same content give the same count. Not asserted as a number; covered by the golden hash. |
| 28 | Database size in pages | file size / 4096 | consequence of content |
| 32, 36 | First freelist trunk page, freelist page count | 0, 0 | `VACUUM INTO` writes no free pages |
| 40 | Schema cookie | deterministic | incremented by each DDL statement on the working database (fixed list) and copied plus one by `VACUUM INTO`. Covered by the golden hash. |
| 44 | Schema format number | 4 | compile option |
| 48 | Default page cache size | 0 | never set |
| 52 | Largest root page (auto-vacuum) | 0 | auto-vacuum off |
| 56 | Text encoding | 1 (UTF-8) | pragma |
| 60 | User version | `SCHEMA_VERSION` | pragma, checked in step 8 |
| 64 | Incremental vacuum | 0 | auto-vacuum off |
| 68 | Application ID | 0x554E4D50 | pragma, checked in step 8 |
| 72–91 | Reserved | zero | constant |
| 92 | Version-valid-for | equals offset 24 | written by SQLite with the change counter |
| 96 | `SQLITE_VERSION_NUMBER` | the vendored version | pinned by vendoring; upgrade = generator version bump |

Nothing in the file depends on time, the output path, the process, or the machine.

## 7. Layer blob format, version 1

A self-describing container, so a blob copied out of the database can still be decoded. All multi-byte fields little-endian (unlike SQLite's header).

### 7.1 Header (24 bytes)

| Offset | Size | Field | Value |
| --- | --- | --- | --- |
| 0 | 4 | magic | `U` `M` `L` `Y` (0x55 0x4D 0x4C 0x59) |
| 4 | 2 | blob format version | 1 |
| 6 | 1 | element type | 1 `u8`, 2 `i8`, 3 `u16`, 4 `i16`, 5 `u32`, 6 `i32`, 7 `u64`, 8 `i64` |
| 7 | 1 | codec | 0 raw, 1 delta-varint |
| 8 | 4 | width | 1 to 65536 |
| 12 | 4 | height | 1 to 65536 |
| 16 | 8 | payload length | bytes after the header; must equal blob length - 24 |

### 7.2 Codec 0: raw

The width × height elements in index order, each little-endian at its natural size.

### 7.3 Codec 1: delta-varint (the default)

A project codec: deterministic by definition, no dependency, a few dozen lines to encode and decode, and the viewer decodes it with the same library. It suits smooth rasters (elevation, temperature) and rasters with long runs (biomes, masks).

Encoding, over elements in index order:

```
prev = 0                                  // uint64
for each element e (widened to 64 bits: sign-extended for signed types,
                    zero-extended for unsigned):
    d = uint64(e) - prev                  // mod 2^64
    z = (d << 1) ^ (uint64(int64(d) >> 63))     // zigzag of int64(d)
    prev = uint64(e)
    emit token for z
```

Tokens:
- A maximal run of k ≥ 1 consecutive z = 0 is emitted as `LEB128(0)` followed by `LEB128(k - 1)`.
- A value z ≥ 1 is emitted as `LEB128(z)`.
- LEB128(n): 7 bits at a time, least significant group first; every byte except the last has bit 7 set. Shortest form only.

"Maximal" means the encoder never emits two zero-run tokens in a row; this makes the encoding unique. The decoder rejects (as corrupt) a non-shortest LEB128, a run that overruns the element count, a payload with trailing bytes, a decoded value that does not fit the element type, and two consecutive zero-run tokens.

Worked examples:

| Elements | Type | Deltas | zigzag | Payload bytes |
| --- | --- | --- | --- | --- |
| 5, 5, 5, 7, 0 | u8 | 5, 0, 0, 2, -7 | 10, 0, 0, 4, 13 | `0A 00 01 04 0D` |
| -1, 300, 300 | i16 | -1, 301, 0 | 1, 602, 0 | `01 DA 04 00 00` |
| 0, 0, 0 | u8 | 0, 0, 0 | 0, 0, 0 | `00 02` |

Full blob for the first example, width 5, height 1: `55 4D 4C 59 | 01 00 | 01 | 01 | 05 00 00 00 | 01 00 00 00 | 05 00 00 00 00 00 00 00 | 0A 00 01 04 0D` (29 bytes).

### 7.4 Compression beyond codec 1

The design doc calls layer blobs "compressed". Codec 1 is a light compressor (roughly 2× on smooth 16-bit data, far more on categorical data with runs). If P1 finds the files too large, a general-purpose codec can be added as codec 2. **Requirement for any such codec:** the encoder's output must be a pure function of the input bytes and fixed parameters, single-threaded, at a pinned vendored version, byte-identical on every platform and compiler, with no CPU-feature-dependent output; adding it means a new dependency in `extern/` (zstd or a deflate implementation are the candidates), a schema version bump and a proposals entry. P0 does not add one.

## 8. What an empty world contains in P0

`worldgen-cli generate --seed N --out world.db` with no test pattern writes:

- Tables `meta` and `layer`, and the automatic index `sqlite_autoindex_layer_1` for `layer.name`.
- `layer`: no rows.
- `meta`: exactly these 8 rows, in key order:

| key | value |
| --- | --- |
| `content_hash` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (SHA-256 of the empty encoding: P0 registers no data kinds) |
| `format` | `the-unmapped-world` |
| `generator_version` | `0.2.0` |
| `param.map_size` | `1024` (default) |
| `schema_version` | `1` |
| `seed` | N in canonical decimal |
| `seed_phrase` | empty, or the trimmed phrase |
| `stages` | empty |

Expected size: 4 pages (16384 bytes): page 1 (header and `sqlite_schema`), `meta`, `layer`, and the `layer.name` index. To be confirmed by the validation specialist; not a requirement in itself.

With the test pattern (P0-04), `stages` is `test_pattern` and `layer` holds three rows (P0-04 section 5.2).

## 9. Read API

The read side lives in `worldgen` (no engine dependency) and is what `worldgen-cli render`, `report`, `hash --detail` and the GDExtension binding (P2) use. P0 implements the parts marked P0; the rest is the shape later phases fill in. The interface is described by what it must offer, not by C++ signatures.

| Capability | Phase | Requirements |
| --- | --- | --- |
| Open | P0 | Opens with `SQLITE_OPEN_READONLY`; never writes, never creates a file, never leaves a journal. Validates in this order and returns a distinct error for each: file missing or unreadable; not an SQLite file; `application_id` ≠ 0x554E4D50 or `meta.format` missing/wrong ("not a world file"); `user_version` or `meta.schema_version` ≠ the supported version ("world schema version X, this build reads Y"); a required meta key missing. |
| Meta | P0 | Get a value by key (absent is a normal result); list all key/value pairs in key order; typed accessors for `seed` (uint64), `schema_version`, `generator_version`, `seed_phrase`, `param.map_size`, `stages` (list). |
| Layer list | P0 | All layers in `id` order, each with name, width, height, element type, codec, encoded size in bytes. Reading the list must not read or decode blobs. |
| Layer read | P0 | Decode a layer by name into a contiguous buffer of its element type, row-major. Validates the blob header against the columns and the codec rules (7.3); a mismatch is a "corrupt layer" error naming the layer. Not found is a distinct error. |
| Entity tables | P3+ | For each table added later: get by id; iterate in primary-key order; the filtered queries the viewer needs (for example events by year range, events by region, causes of an event), each defined in the stage spec that adds the table, each returning rows in a defined order. |
| Errors | P0 | Returned as values (a result type), never as exceptions escaping the API, so the GDExtension can bind it without exception translation. Messages are plain English and say what was expected. |
| Threads | P0 | A reader object is used by one thread at a time; open one reader per thread. All results are copies owned by the caller. |
| Floats | P0 | The reader returns integers and decoded integer buffers only. Converting to float for display is the caller's business. |

## 10. Invariants

- Same seed, phrase, settings, data and generator version → byte-identical file, independent of output path, current directory, environment, locale, time zone, time of day and machine.
- Header fields as in 6.1; freelist count 0; version-valid-for = change counter.
- No REAL values in any table (guaranteed by `STRICT`, checked by `hash --detail`).
- Every layer blob decodes, and its header matches its row.
- `meta` holds every required key with a value in canonical form; `meta.schema_version` = `user_version`.
- After a successful or failed run, no `.partial`, `-journal` or `-wal` file remains next to the output.

## 11. Determinism notes

| Risk | Mitigation |
| --- | --- |
| Insertion order, batching, updates or deletes change B-tree layout | `VACUUM INTO` rebuilds in key order; ids assigned explicitly. |
| Stale bytes in pages | Fresh file from `VACUUM INTO`; `SQLITE_SECURE_DELETE`. |
| Header counters | Deterministic for a fixed operation sequence and SQLite version; golden hash detects drift. |
| SQLite version | Vendored and pinned; version change is a deliberate generator version bump. |
| DDL text varies with line endings | DDL is C++ string constants. |
| Floats in the database | `STRICT` tables with no REAL columns; integers bound with `bind_int64`. |
| Layer encoding | Project codec, defined exactly; little-endian by arithmetic. |
| Page size picked from device | Fixed by compile option and pragma. |

## 12. Tests required

### 12.1 Unit tests: blob codec

- The three worked examples in 7.3 encode to exactly the listed bytes and decode back; the full 29-byte blob example matches.
- Round trip for every element type with: all zeros; all maximum values; alternating minimum and maximum (exercises the mod 2^64 delta); a ramp; 10^5 values from a fixed `Rng` stream; width × height of 1 × 1, 1 × 65536, 3 × 7.
- Encoding is canonical: encoding a decoded blob gives the original bytes.
- Decoder rejects: wrong magic; version ≠ 1; unknown type or codec; payload length mismatch; non-shortest LEB128 (`80 00`); two consecutive zero-run tokens (`00 00 00 00`); a run past the element count; trailing bytes; a value out of range for the type (e.g. u8 decoding to 256).
- Codec 0 round trip for every type, little-endian byte order checked on a known value (u16 0x1234 → `34 12`).

### 12.2 Unit tests: writer and reader

- Empty world for seed 0: tables, `meta` rows and values exactly as in section 8; `layer` empty.
- Header fields read from the file bytes: offsets 16, 18, 19, 20, 21–23, 32, 36, 44, 48, 52, 56, 60, 64, 68, 72–91 as in 6.1; offset 92 equals offset 24; offset 96 equals `sqlite3_libversion_number()`; offset 28 × 4096 equals the file size.
- `sqlite_schema` `sql` column for `meta` and `layer` equals the DDL strings in 5.4 byte for byte.
- Seed canonical text: seeds 0, 1, 42, 2^63, 2^64 - 1 are stored as `0`, `1`, `42`, `9223372036854775808`, `18446744073709551615`.
- Phrase seed: `seed_phrase` holds the trimmed phrase; `seed` holds `phrase_seed` of it.
- Writing a REAL into any table fails (STRICT); a layer whose header disagrees with its columns is rejected by the reader as corrupt.
- Reader errors, each distinct: missing file; a text file; an SQLite file with a different `application_id`; a world with `user_version` 2; a world missing `seed`; layer not found.
- Reader never writes: file bytes and modification time unchanged after open, list, read, close (the modification time is used only by this test, never by the generator).
- Failure cleanup: a run forced to fail after step 6 (test hook) leaves no `.partial` file.
- Layer order: writing layers `b` then `a` gives ids 1 and 2 and the list returns `b`, `a`.

### 12.3 Determinism tests

- Same inputs twice → identical bytes.
- Same inputs, different output paths and different current directories → identical bytes.
- Same inputs with environment variables `TZ`, `LANG`, `LC_ALL` changed → identical bytes.
- Layout independence: a test-only writer that inserts the same rows in reverse id order, or inserts extra rows and deletes them, produces a file identical to the normal writer's (proves `VACUUM INTO` normalizes layout).

### 12.4 Golden hashes

Recorded by the validation specialist and compared on every preset (see P0-04 section 9): SHA-256 of the empty world for seeds 0, 1, 42, 18446744073709551615 and phrase `The Unmapped`, and of the test-pattern world for seeds 0 and 42.

## 13. Open questions

1. **Schema scope at version 1:** `meta` and `layer` only (5.1). Approve, or ask for all tables now.
2. **Schema-change workflow during the prototype** (5.2). Approve.
3. **`layer.id`** added to the design's columns (5.4). Proposals entry in the report.
4. **Compression:** project codec 1 now; general-purpose codec deferred with the stated requirement (7.4). Approve.
5. **Working database in memory** (4.2). For a 2048-cell map with history this is estimated at well under 1 GB; if P1 or P3 measures otherwise, switch to a working file next to the output with `journal_mode = OFF`, which does not change the output bytes.
6. **Map orientation** (5.6): row 0 north, column 0 west. Approve.
7. **`seed_phrase`** is part of the file, so the same numeric seed generated with and without its phrase gives two different files. Acceptable as interim; say if worlds should be identical regardless of how the seed was entered.

## 14. Amendments after the first implementation (2026-11-15)

Decisions by the lead on points raised by the generator engineer. Where these differ from the text above, these win.

1. **REAL values and STRICT (replaces the first clause of the sixth bullet in 12.2).** STRICT tables coerce a REAL where the conversion is lossless: 2.0 bound to an INTEGER column is stored as the integer 2, and a REAL bound to a TEXT column is stored as text. Only a non-integral REAL into an INTEGER column, or any REAL into a BLOB column, is rejected. The invariant in section 10 is unchanged and is what must be tested: binding 1.5 to `layer.width` fails, binding a REAL to `layer.data` fails, and after any sequence of writes `typeof()` of every stored value is never `real`. The writer itself never binds a REAL.
2. **Reader errors (section 9, Open).** A sixth distinct error is added after "required meta key missing": a required key whose value is not in canonical form ("meta value invalid"). Required values are validated at open, so the typed accessors cannot fail. "Not a world file" also covers: no `meta` table; a file in WAL mode (refused so that a read-only open can never create `-wal` or `-shm`); and a file with the SQLite magic followed by garbage. `application_id` and `user_version` are read from header offsets 68 and 60.
3. **Layer list (section 9).** The codec is stored only in the blob, so the list reads the 24 header bytes of each blob and nothing more; it does not read the payload or decode. A header that disagrees with its row is a "corrupt layer" error from the list as well as from the read.
4. **Decoder errors (7.3, 12.1).** The decoder reports the first problem met in one forward pass. Consequently the example `00 00 00 00` is "two consecutive zero-run tokens" only when the layer has at least 2 elements; with 1 element it is "trailing bytes". Rejections also include: a blob shorter than 24 bytes; width or height outside 1..65536; a codec-0 payload whose length is not width × height × element size; a payload that ends inside a token; and a LEB128 value that does not fit in 64 bits.
5. **Meta (5.5).** `param.map_size` must be 512, 1024 or 2048 for both writer and reader. The writer takes a phrase that is already trimmed and valid and computes `seed` from it with `phrase_seed`; the reader checks that `seed_phrase` is valid and trimmed but does not recompute the seed.
6. **Stages (section 6, step 3).** A stage that ends without committing is rolled back and is not recorded in `stages`. Running two stages with the same name is fatal.
7. **Overwrite (section 6, step 9).** The output's existence is checked at the start of finalization and again immediately before the rename; the window between that check and the rename is accepted. A refusal because the output exists still deletes a stale `<out>.partial`.
8. **Blob size limit (5.6).** SQLite's maximum value length is 1,000,000,000 bytes. A layer whose encoded blob exceeds it is a fatal error at write time. This cannot occur for 2048 × 2048 maps.
9. **`VACUUM INTO` (section 6).** Confirmed: the vendored SQLite 3.54.1 preserves `application_id` and `user_version`; the fallback is not used. The empty world is 4 pages, 16384 bytes.
10. **Raw row access for `hash --detail`.** The read API gains one capability, implemented with P0-04: the rows of `sqlite_schema` and of any table in primary-key order, each column as a typed value (NULL, INTEGER, TEXT, BLOB, or REAL reported as an error), so that the CLI does not use SQLite directly.
11. **Test hooks.** The test-support hooks (`worldgen/db/testing.hpp`) are compiled into the library for P0. They are not part of the API that stages or the viewer may use.
12. **Seed phrases are scrubbed (design doc section 4; replaces the phrase parts of 5.5, section 8, 12.2 and amendment 5).** `meta.seed_phrase` holds the scrubbed phrase of P0-01 section 7.3 (only `a`–`z`, `0`–`9` and single interior spaces, 1 to 256 bytes), or is empty when the seed was given as a number. The writer takes a phrase that is already scrubbed (passing one that is not is a fatal error) and computes `seed` from it with `phrase_seed`. The reader accepts a non-empty `seed_phrase` only if it is at most 256 bytes and scrubbing it returns it unchanged; otherwise the value is invalid ("meta value invalid"). The reader still does not recompute the seed. Because this changes the bytes of phrase worlds, `GENERATOR_VERSION` is `0.2.0`, which changes `meta.generator_version` and therefore every golden world hash.
13. **Layers in P1.** P1-00 appendix A2 is adopted: the geography layer catalogue is in P1-00 section 4 and summarized in `docs/specs/schema.md`; each layer is written exactly once by the stage that owns it; adding a layer is not a schema change, while changing a catalogued layer's name, element type, units or meaning is; `meta.stages` lists the stage names of P1-00 section 3. The first pass of P1 adds no table, column or required meta key, so `SCHEMA_VERSION` stays 1.
14. **Schema version 2** (P1-01 fourth version, generator 0.6.0, 2026-11-17; proposals "Pass sites are stored in the world file"). The DDL gains `CREATE TABLE pass (…) STRICT`, executed after `layer`'s (exact text in `docs/specs/schema.md` and P1-01 9.1), and `SCHEMA_VERSION`, `user_version` and `meta.schema_version` become 2. Every world, including an empty one, has the (possibly empty) table. The empty world is now 5 pages, 20480 bytes (the `pass` table adds one page); this replaces the figure of section 8 and amendment 9. A reader of version 2 rejects version 1 files as an unsupported schema version, as section 5 already requires for any other version.
15. **Schema version 3** (P1-04 second version and P1-03, the biomes merge, generator 0.8.0, 2026-11-17; proposals "P1 schema changes"). The DDL gains, after `pass`'s and in this order, `CREATE TABLE biome (id INTEGER PRIMARY KEY, key TEXT NOT NULL UNIQUE, name TEXT NOT NULL, colour INTEGER NOT NULL, water INTEGER NOT NULL) STRICT` and `CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL) STRICT` (exact text and row content in `docs/specs/schema.md`, P1-04 3 and P1-03 3). `SCHEMA_VERSION`, `user_version` and `meta.schema_version` become 3. Every world, including an empty one, has both (possibly empty) tables. SQLite creates the automatic index `sqlite_autoindex_biome_1` for the UNIQUE `key` column. The empty world is now **8 pages, 32768 bytes**: page 1 `sqlite_schema`, then the root pages of `meta`, `layer`, `sqlite_autoindex_layer_1`, `pass`, `biome`, `sqlite_autoindex_biome_1` and `climate`; this replaces the figure of amendment 14 (established with the settings of section 4 and `VACUUM INTO`; the same procedure reproduces amendment 14's 5 pages for schema 2). A reader of version 3 rejects version 2 files as an unsupported schema version, as section 5 requires.
16. **Schema version 4** (P1-03 revised 2026-11-18 and P1-04 third version, the biome variety merge, generator 0.9.0; proposals "More natural biomes, per-world temperature, a thinner coastal forest"). The `climate` statement of the DDL is replaced, in the same position (fifth, after `biome`'s), by `CREATE TABLE climate (id INTEGER PRIMARY KEY, wind_toward INTEGER NOT NULL, temp_offset_dc INTEGER NOT NULL) STRICT` (exact text in `docs/specs/schema.md` and P1-03 3); the writer inserts the row with all three columns. `SCHEMA_VERSION`, `user_version` and `meta.schema_version` become 4. No table is added, so the empty world stays **8 pages, 32768 bytes** (checked with the settings of section 4 and `VACUUM INTO` on the exploration build). A reader of version 4 rejects version 3 files as an unsupported schema version with the message `world schema version 3, this build reads 4` (the same text whether `user_version` or `meta.schema_version` is the one found to differ; the number is the value found), as section 5 requires; the writer's `.partial` verification and the file-header check compare `user_version` with 4. The schema version is a compile-time constant: no environment variable or runtime switch may choose it (the exploration's `UNMAPPED_EXP_SCHEMA4` is not part of the contract).
