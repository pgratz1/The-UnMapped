# Design proposals

`design.md` in this folder is read-only: it is exported from a working document and overwritten on each update. Record proposed changes, open questions and discovered problems here. Paul carries them back to the design conversation, and accepted items appear in the next export of `design.md`.

## How to add an entry

Add entries at the bottom, newest last, using this shape:

```
### <short title>

- Date:
- Raised by: <agent role or person>
- Design doc section:
- Problem: what is wrong, unclear or missing
- Proposal: the suggested change
- Interim choice: what was done in the meantime, if anything
- Status: open
```

Status values: `open`, `accepted`, `declined`, `superseded`.

## Entries

### License decided: MIT

- Date: 2026-11-15
- Raised by: Paul, with the lead session
- Design doc section: 10 (tech stack table, "License: To be decided") and 12 (open question "License and distribution")
- Problem: the license was undecided, and the repository is now public.
- Proposal: record the project as open source under the MIT license, with no plan to sell it. Mark the open question as decided. Note for milestone 9: the newer Piper and espeak-ng are GPL-3. Linking them is allowed, and it makes any distributed build that includes them GPL-3 as a whole while the project's own source stays MIT. Running the voice engine as a separate process avoids that.
- Interim choice: `LICENSE` (MIT) added at the repository root.
- Status: accepted (merged into design.md, 2026-11-15)

### Build output outside the source tree

- Date: 2026-11-15
- Raised by: Paul, with the lead session
- Design doc section: 10 (build and CI)
- Problem: the working checkout lives in a Dropbox folder, so build trees and generated worlds written into the repository would be synced.
- Proposal: state in section 10 that all generated files go to a build root outside the tree (`$HOME/.cache/the-unmapped/` by default, overridable with `UNMAPPED_BUILD_ROOT`), and that in-source builds are refused.
- Interim choice: the rule is recorded in the "Build and test" section of `CLAUDE.md`, with a `.gitignore` as a backstop.
- Status: accepted (merged into design.md, 2026-11-15)

### Build root override and per-worktree build trees

- Date: 2026-11-15
- Raised by: build-engineer
- Design doc section: 10 (build and CI); CLAUDE.md "Build output lives outside the source tree"
- Problem: CMake presets cannot read an optional environment variable with a fallback, so `UNMAPPED_BUILD_ROOT` cannot change a preset's `binaryDir` directly. Separately, worktrees sharing one `build/<preset>/` directory would share a CMake cache, which CMake rejects.
- Proposal: presets default to `<root>/build/<source dir name>/<preset>/`, where root is `$HOME/.cache/the-unmapped` (Windows: `%LOCALAPPDATA%\the-unmapped`). `UNMAPPED_BUILD_ROOT` is applied with `cmake --preset P -B "$UNMAPPED_BUILD_ROOT/build/P"`, and configure warns when it is set but unused.
- Interim choice: implemented as above and documented in CLAUDE.md "Build and test".
- Status: accepted (merged into design.md, 2026-11-15)

### Pinned dependency versions for the prototype

- Date: 2026-11-15
- Raised by: build-engineer
- Design doc section: 10 (Dependencies row), 4 (reproducible database file)
- Problem: the design doc names the dependencies but not their versions or the libraries chosen for TOML parsing and image writing.
- Proposal: record SQLite 3.54.1, doctest 2.5.3, toml++ 3.4.0 and stb_image_write 1.18 (stb commit 9600522) as the pinned set. Changing the SQLite version or its compile options is treated like a schema change: spec update, proposals entry, new golden hashes.
- Interim choice: vendored in extern/ with checksums in extern/README.md.
- Status: accepted (merged into design.md, 2026-11-15)

### P0 world schema starts with meta and layer only

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6, 13
- Problem: Section 6 lists 24 tables. Most of their column types, units and blob formats are unspecified, so creating them all in P0 would freeze guesses that later phases must change. Rule 7 makes every schema change a version bump plus a proposal, which would mean heavy churn during the prototype.
- Proposal: Schema v1 has `meta` and `layer` only. Each phase adds its tables in its stage spec. During the prototype: one schema version bump per merged branch that changes the schema, one proposals entry per phase listing that phase's changes, a schema reference at docs/specs/schema.md from P1 on, and no migrations (readers accept exactly the current version; old worlds are regenerated).
- Interim choice: As proposed, in docs/specs/p0-02-world-db.md section 5.
- Status: accepted (merged into design.md, 2026-11-15)

### layer table gets a surrogate id column

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6 (layer: name, width, height, format, data)
- Problem: Keyed by name alone, `layer` would be a WITHOUT ROWID table. SQLite stores whole rows in the interior pages of those, which is unsuitable for multi-megabyte blobs.
- Proposal: Add `id INTEGER PRIMARY KEY`, keep `name` UNIQUE, and record in section 6 that every table is STRICT with no REAL columns.
- Interim choice: Done in schema v1.
- Status: accepted (merged into design.md, 2026-11-15)

### Layer blob compression is a project codec

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6 ("compressed blobs"), 4 (reproducible file)
- Problem: Section 6 says layer blobs are compressed but names no format. A general-purpose compressor would be a new dependency whose output must be byte-identical everywhere.
- Proposal: Codec 1 is a project delta-zigzag-varint encoding with zero runs, defined exactly in p0-02. A general-purpose codec (zstd or deflate) may be added later only if it is single-threaded, pinned and proven byte-identical, with a schema bump.
- Interim choice: Codec 1 is the default; raw (codec 0) is supported.
- Status: accepted (merged into design.md, 2026-11-15)

### Fractional numbers in TOML are decimal strings

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 10 (Data files: TOML), 4 (no platform math)
- Problem: Parsing TOML floats goes through platform or parser floating-point code before it reaches fixed point.
- Proposal: The loader rejects TOML float literals. Fractions are written as quoted decimal strings ("0.35") and parsed by project code into fixed point; integers stay plain TOML integers.
- Interim choice: As proposed, in p0-03.
- Status: accepted (merged into design.md, 2026-11-15)

### Seed phrase normalization

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 5 (World-creation settings: "Any 64-bit value or a text phrase")
- Problem: The design does not say how a phrase becomes 64 bits, or whether case and Unicode form matter.
- Proposal: Decide whether phrases are case-insensitive or Unicode-normalized before the create-world screen exists.
- Interim choice: UTF-8 bytes after trimming ASCII whitespace, hashed with a domain tag. Case and normalization are significant. The phrase is stored in meta.seed_phrase, so the same seed entered as a number and as its phrase gives different files.
- Status: superseded (design.md section 4 now specifies scrubbed phrases; implemented in generator version 0.2.0)

### worldgen-cli command list in section 10

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 10 (repository tree: "generate, inspect, export map images") vs 13 and CLAUDE.md (generate, render, report, hash)
- Problem: The two sections disagree on the command set.
- Proposal: Align section 10 with section 13.
- Interim choice: p0-04 follows section 13.
- Status: accepted (merged into design.md, 2026-11-15)

### Generator version is a manual constant

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 ("The database records the generator version")
- Problem: The design doesn't say what the version is. A git-derived version would change every world file on every commit and break golden hashes.
- Proposal: A hand-bumped MAJOR.MINOR.PATCH constant, bumped by any change to output bytes, with golden hashes updated in the same branch. Upgrading the vendored SQLite counts as such a change.
- Interim choice: 0.1.0 for P0; 0.2.0 from the scrubbed seed phrases (first use of the bump rule).
- Status: accepted (merged into design.md, 2026-11-15)

### P0 core numeric choices

- Date: 2026-11-15
- Raised by: lead, from the spec-writer's decisions list
- Design doc section: 4 (rules that make bit-identical output achievable)
- Problem: Section 4 requires fixed-point math, an own random generator and seed-stream hashing but names none of them.
- Proposal: record the choices in docs/specs/p0-01-deterministic-core.md: one Q32.32 fixed-point type with round-half-away-from-zero and overflow as a fatal error; xoshiro256** seeded by SplitMix64; SplitMix64-based hashing with domain tags for streams and location keys; 2D integer gradient noise; project SHA-256 for file and content hashes. Sine, cosine, exp and similar are deferred to P1.
- Interim choice: approved by the lead for P0 so work can proceed; open to change at Paul's P0 review.
- Status: accepted (merged into design.md, 2026-11-15)

### P0 implementation findings for the design doc

- Date: 2026-11-15
- Raised by: lead, from the P0 engineers, test writers and determinism auditor
- Design doc section: 4, 6, 10
- Problem: implementing P0 settled details the design doc does not record.
- Proposal: note in the design doc, or leave to the specs with a pointer to `docs/specs/`: (1) the world file is built in memory and written with SQLite `VACUUM INTO`, which makes its bytes independent of write order; confirmed with SQLite 3.54.1. (2) STRICT tables coerce a lossless REAL instead of rejecting it, so "no floating point in the database" is enforced by never binding one and by a `typeof` check, not by STRICT alone. (3) The TOML parser's number conversion is pinned to `std::from_chars` on every compiler so that malformed or extreme float literals are reported identically everywhere. (4) A seed phrase in a settings file may not contain control characters other than TAB and LF, although one given on the command line may. (5) `worldgen-cli` reads its arguments as UTF-16 on Windows and writes text output in binary mode.
- Interim choice: recorded in the amendments sections of `docs/specs/p0-01` to `p0-04`.
- Status: accepted (merged into design.md, 2026-11-15)

## Phase P1 Geography, first pass (2026-11-15)

### Map size means more land at the same scale

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 (16 m cells, map size a setting), 5 (map size "8, 16 or 32 km")
- Problem: the design does not say whether a larger map shows more land at the same scale or the same land in finer detail, nor which features keep their physical size.
- Proposal: cells are 16 m at every size, so 512, 1024 and 2048 cells are 8.2, 16.4 and 32.8 km. A larger map is more land at the same scale. The continent's outline, its large bays and the layout of mountain belts are specified in fractions of the map, so one continent fills every map; hills, ridges, erosion, rivers, lakes and region sizes are specified in metres and keep their size. State the nominal sizes as 8, 16 and 32 km with 16 m cells.
- Interim choice: as proposed, in `docs/specs/p1-00-geography-overview.md` section 2.2.
- Status: open

### Climate's latitude span is a fraction of the map

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 (Climate: "temperature from latitude"; "scale is compressed")
- Problem: a 16 km map spans no real latitude. A gradient per kilometre would give an 8 km world almost no climate variety and a 32 km world four times as much.
- Proposal: the north-to-south temperature difference is a tuning value per map (a map-fraction quantity), the same at every map size, standing for the larger land the map represents.
- Interim choice: recorded in p1-00 section 2.2 for the climate spec (second pass).
- Status: open

### Generator tuning is data, and shipped tuning changes bump the version

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 10 ("Content rules live in TOML data files under game/data/ and worldgen config"), 4 (generator version)
- Problem: the design says tuning lives in TOML but not where, how it is identified in a world, or whether changing it is a version change.
- Proposal: tuning is a data kind `worldgen` in `game/data/worldgen/`, one definition per stage, every field required, covered by `content_hash`. A change to the shipped data bumps the PATCH version and re-records the shipped-data golden hashes; algorithm golden tests use a frozen fixture copy so tuning work does not churn them.
- Interim choice: as proposed, in p1-00 section 6 (needs P0-03 amendment 16 for kinds with a schema per id).
- Status: open

### Rivers are rasters in P1; a river table later if history needs one

- See P1-02 section 9 (the entry is written there in full).

### Elevation is final after the elevation stage; hydrology does not carve

- See P1-02 section 9.

### Biome categories and the biome table

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 (six starting biomes; special cases for coast, river and high peaks), 6 (`region.biome`)
- Problem: the biome layer needs codes for water cells; the design does not say whether coast, river and peak special cases are separate biomes; a world file does not say what its biome codes mean.
- Proposal: biome definitions carry a fixed integer `code` and a colour. Categories: the six land biomes plus `ocean` and `lake`. River cells take the biome of their land; coast and peak special cases choose among the six. `blighted_waste` is assigned only by history. The biome stage writes a `biome` table (code, key, name, colour, water) into the world (schema version 2).
- Interim choice: as proposed, in p1-00 section 6.6 and 9.2.
- Status: open

### The layer catalogue is part of the schema contract

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6 (schema rules; "a schema reference lives at docs/specs/schema.md")
- Problem: layers are rows of the `layer` table, so the DDL does not change when a layer is added, yet a reader depends on each layer's type and units.
- Proposal: the layer catalogue (name, type, units, meaning) is part of the contract recorded in `schema.md`. Adding a layer needs no schema version bump; changing a catalogued layer does.
- Interim choice: as proposed, in p1-00 section 4 rule 3.
- Status: open

### P1 schema changes (one entry per phase)

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6 (schema growth)
- Problem: the phase's schema changes need one proposals entry (P0-02 5.2 rule 3).
- Proposal: first pass (elevation, hydrology): none; schema stays at version 1. Second pass: version 2 adds table `biome`; version 3 (or the same bump, if in one branch) adds table `region`. This entry is updated when the second-pass specs fix the DDL.
- Interim choice: as proposed, in p1-00 section 9.
- Status: open

## Phase P2 Godot bring-up and viewer v0 (2026-11-15)

### Godot extension runs from a copy of the project in the build tree

- Date: 2026-11-15
- Raised by: viewer-engineer
- Design doc section: 10 (build output stays outside the source tree; repository layout)
- Problem: Godot writes its cache (`.godot/`) inside the project and loads a GDExtension from a path relative to the project; the cache location cannot be configured. With `game/` in a synced folder, opening it directly writes `.godot/` into the repository, and a link from `game/` to the build tree would be machine-specific and trip CI's clean-checkout check.
- Proposal: state in section 10 that Godot is always run on a copy of the project assembled under the build tree (`<build>/game/`: links to `game/`'s entries, the extension's output in `bin/`, Godot's `.godot/` cache), and that `game/` is never opened directly.
- Interim choice: implemented (`gdext/cmake/assemble_project.cmake`); `CLAUDE.md` and `gdext/README.md` describe it. Saving `project.godot` from the editor with safe save breaks its link; the build warns.
- Status: open

### Pinned engine: Godot 4.7.2 and godot-cpp 10.0.0

- Date: 2026-11-15
- Raised by: viewer-engineer
- Design doc section: 10 (Engine row: "pinned in the repo to the newest stable version that Terrain3D supports")
- Problem: the design gives the rule but not the version.
- Proposal: record Godot 4.7.2-stable (Terrain3D 1.1.0 states "Supports Godot 4.5-4.8+"; 4.8 was at beta when pinned) and godot-cpp 10.0.0-stable targeting GDExtension API 4.7, vendored in `extern/godot-cpp/`. The extension is an optional part of the build (`UNMAPPED_BUILD_GDEXT`, off by default) so `worldgen` and `worldgen-cli` never need Godot.
- Interim choice: as proposed; evidence and checksums in `gdext/README.md`, `extern/README.md` and `tools/ci/godot-4.7.2-stable.sha512`.
- Status: open

### Friction writing Godot code purely in C++ (P2 findings)

- Date: 2026-11-15
- Raised by: viewer-engineer
- Design doc section: 3, 8, 10 (C++ only, no GDScript)
- Problem: observed while building viewer v0 with godot-cpp 10 and Godot 4.7.2: (1) every method, signal and property visible to Godot needs hand-written registration, and scene-to-code wiring is by node name, checked only at run time; (2) iteration is rebuild then restart, with no hot reload, and godot-cpp adds about 1,100 files and three minutes to a clean build; (3) godot-cpp's defaults clash with a normal C++ library (exceptions off, static MSVC runtime, files written into the source tree) and each needed an override; (4) headless testing has traps: Godot idles instead of exiting if the extension fails to load, and the headless display has no window size, so layouts collapse; (5) Godot changes the working directory to the project, so relative command-line paths break; (6) Godot's integer is 64-bit signed, so unsigned 64-bit layer cells are shown as text; (7) a layout bug was invisible to a headless pixel check and found only in a windowed run; (8) sanitizer builds of the extension are impossible against the stock editor.
- Proposal: keep C++ only, and plan for it: every screen gets a headless check scene from the start, as the viewer has; section 10's "headless Godot runs for scene tests" becomes one CTest per screen, run where the pinned editor is available; budget CI time and a compiler cache for godot-cpp; revisit hot reload when gameplay iteration starts.
- Interim choice: all handled in `gdext/` and `extern/CMakeLists.txt`.
- Status: open
