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
